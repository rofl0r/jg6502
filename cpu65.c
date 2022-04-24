#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define u8 uint8_t
#define u16 uint16_t

#define CPU_TYPE_6502 0
#define CPU_TYPE_65C02 1
#define CPU_TYPE_R65C02 2
#define CPU_TYPE_HUC6280 3

#include "cpu65type.h"

#ifndef CPU_TYPE
#error need to set CPU_TYPE to one of the above list
#endif

/* define this if you don't want BCD, e.g. to emulate a Ricoh 2A03 as used in NES */
#ifndef CPU_NO_BCD
#define BCD 1
#else
#define BCD 0
#endif

#ifndef CPU_READ_N
#error you need to provide macros CPU_READ_N(dest, addr, n) and CPU_WRITE_N(source, addr, b)
#endif

#if !defined(__BYTE_ORDER__)
# error need __BYTE_ORDER__ macro
#endif

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define TUP16(F_, A_, B_) union {struct { uint8_t B_, A_; }; uint16_t F_;}
/* FIXME this is a misnomer, its purpose is to switch to and from host to little end */
#define MAKELE16(VAL) VAL
#else
#define TUP16(F_, A_, B_) union {struct { uint8_t A_, B_; }; uint16_t F_;}
#define MAKELE16(VAL) (u16)(VAL>>8|VAL<<8)
#endif

#define N_FLAG 0x80
#define V_FLAG 0x40
#define T_FLAG 0x20
#define B_FLAG 0x10
#define D_FLAG 0x08
#define I_FLAG 0x04
#define Z_FLAG 0x02
#define C_FLAG 0x01

#if CPU_TYPE == CPU_TYPE_HUC6280
#define PC_MAX_FETCH 8
#define T_INIT 0
#define B_INIT 0
#define PLP_MASK 0xff
#define INT_MASK (~(D_FLAG|T_FLAG))
#define INT_VEC 0xfff6
#define BR_PENALTY 2
#else
#define PC_MAX_FETCH 4
#define T_INIT 1
#define B_INIT 1
#define BR_PENALTY 1
// vector locations for HW interrupts: FFFE for the IRQ and FFFA for the NMI
#define INT_VEC 0xfffe
#define PLP_MASK (N_FLAG|Z_FLAG|C_FLAG|I_FLAG|D_FLAG|V_FLAG)
# if CPU_TYPE > 0
# define INT_MASK (~(D_FLAG))
# else
# define INT_MASK 0xff
# endif
#endif

/* abbreviations:
   C8MSM - "HuC6280 - CMOS 8-bit Microprocessor Software Manual.pdf"
   HUS   - HuC6280 specific
   WDS   - WDC 65c02 specific (includes HuC)
*/

enum address_mode {
	am_imp1,  /* implied, variant 1: 3.1.1 in C8MSM (IMPLID). 1byte insn */
	am_imp2,  /* implied, variant 2: 3.1.2 in C8MSM (IMPLID). 2byte insn, HUS */
	am_imp3,  /* implied, variant 3: 3.1.2 in C8MSM (IMPLID). 7byte insn, HUS */
	am_imm,   /* immediate: 3.2 in C8MSM (IMM). 2byte insn */
	am_zp,    /* zero page: 3.3 in C8MSM (ZP). 2byte insn */
	am_zpx,   /* zero page, x register indexed: 3.4 in C8MSM (ZP, X). 2byte insn */
	am_zpy,   /* zero page, y register indexed: 3.5 in C8MSM (ZP, Y). 2byte insn */
	am_zprel, /* zero page relative: 3.6 in C8MSM (ZP, REL). 3 byte insn, HUS */
	am_ind,   /* zero page indirect: 3.7 in C8MSM ((IND)). 2 byte insn, WDS */
	am_izx,   /* zero page indexed indirect: 3.8 in C8MSM ((IND, X)). 2 byte insn */
	am_izy,   /* zero page indirect indexed: 3.9 in C8MSM ((IND), Y). 2 byte insn */
	am_abs,   /* absolute: 3.10 in C8MSM (ABS). 3 byte insn */
	am_abx,   /* absolute x-register indexed: 3.11 in C8MSM (ABS, X). 3 byte insn */
	am_aby,   /* absolute y-register indexed: 3.12 in C8MSM (ABS, Y). 3 byte insn */
	am_abi,   /* absolute indirect: 3.13 in C8MSM ((ABS)). 3 byte insn */
	am_abix,  /* absolute indexed indirect: 3.14 in C8MSM ((ABS, X)). 3 byte insn */
	am_rel,   /* (PC-)relative: 3.15 in C8MSM (REL). 2 byte insn */
	am_immzp, /* immediate zero page: 3.16 in C8MSM (IMM ZP). 3 byte insn. HUS */
	am_immzx, /* immediate zero page indexed: 3.17 in C8MSM (IMM ZP, X). 3 byte insn. HUS */
	am_immab, /* immediate absolute: 3.18 in C8MSM (IMM ABS). 4 byte insn. HUS */
	am_immax, /* immediate absolute indexed: 3.19 in C8MSM (IMM ABS, X). 4 byte insn. HUS */
	am_acc    /* accumulator. 3.20 in C8MSM (ACC). 1 byte insn. */
};

struct cpu65 {
	TUP16(pc, pch, pcl);
	u8 s, a, x, y, i;
	u8 f_n; /* sign */
	u8 f_v; /* overflow */
	u8 f_t; /* memory operation - HuC6280 only */
	u8 f_b; /* breakpoint */
	u8 f_d; /* bcd mode */
	u8 f_i; /* interrupt disable */
	u8 f_z; /* zero */
	u8 f_c; /* carry */
	u8 *zp;
	u8 *stack;
	void *user; /* user data. */
	void (*trace_print) (struct cpu65*, char*);
};

/* you need to pass the address of the zeropage in the memory you allocated as
   a parameter.
   this is to have fast access to zeropage and stack. it needs to be a single
   continuous memory region of at least 512 bytes.
   additionally, also for speed, our code always reads at least 4 (huc: 8)
   bytes from pc. this means you should (c)allocate 8 bytes more for each
   memory region which can be executed (rom, ram) and if the READ_N call
   happens to read over the rom/ram area into a memory map region, ignore
   everything but the initial address for region checks.
*/
void cpu65_init(struct cpu65 *cpu, u8 *zeropage) {
	memset(cpu, 0, sizeof *cpu);
	cpu->zp = zeropage;
	cpu->stack = zeropage + 256;
	cpu->s = 0xff;
	cpu->f_t = T_INIT;
	cpu->f_b = B_INIT;
}

/* pass the address of the initial pc on reset. */
void cpu65_reset(struct cpu65 *cpu, u16 pc) {
	cpu65_init(cpu, cpu->zp);
	cpu->pc = pc;
}

static inline u8 pack_flags(struct cpu65 *cpu) {
	return
	(cpu->f_n << 7) | (cpu->f_v << 6) |
	(cpu->f_t << 5) | (cpu->f_b << 4) |
	(cpu->f_d << 3) | (cpu->f_i << 2) |
	(cpu->f_z << 1) | (cpu->f_c << 0);
}

static inline void unpack_flags(struct cpu65 *cpu, u8 f) {
	cpu->f_n = !!(f & N_FLAG);
	cpu->f_v = !!(f & V_FLAG);
#if CPU_TYPE == CPU_TYPE_HUC6280
	cpu->f_t = !!(f & T_FLAG);
	cpu->f_b = !!(f & B_FLAG); // TODO: check whether huc really restores B
#else
	/* http://www.6502.org/tutorials/register_preservation.html :
	   Chelly adds: At the end proper checking of the B flag is discussed.
	   My feedback is that it would be simpler to explain that there is no
	   B flag in the processor status register; that bit is simply unused.
	   When pushing the status register on the stack, that bit is set to a
	   fixed value based on the instruction/event (set for PHP and BRK,
	   clear for NMI). This is much simpler to explain and leads to no
	   incorrect assumption that there is a B flag in the status register
	   that can be checked.	*/
	cpu->f_t = 1;
	cpu->f_b = 1;
#endif
	cpu->f_d = !!(f & D_FLAG);
	cpu->f_i = !!(f & I_FLAG);
	cpu->f_z = !!(f & Z_FLAG);
	cpu->f_c = !!(f & C_FLAG);
}

#define M (*m)
#define PC cpu->pc
#define A cpu->a
#define X cpu->x
#define Y cpu->y

#define N cpu->f_n
#define V cpu->f_v
#define T cpu->f_t
#define B cpu->f_b
#define D cpu->f_d
#define I cpu->f_i
#define Z cpu->f_z
#define C cpu->f_c

/* get 16 bit value into addr, depending on address mode */
#define GET_W(AM) \
	switch(AM) { \
	case am_abs:	addr = MAKELE16(op.pw[0]); break; \
	case am_abi:	addr = MAKELE16(op.pw[0]); \
			CPU_READ_N(&addr, addr, 2); \
			addr = MAKELE16(addr); break; \
	case am_abix:	addr = MAKELE16(op.pw[0]) + X; \
			CPU_READ_N(&addr, addr, 2); \
			addr = MAKELE16(addr); break; \
	}

/* get 16 bit value into addr from zeropage
   the second byte of the address might wrap around if N == 0xff */
#define GET_W_ZP(N) addr = cpu->zp[(N)] | (cpu->zp[((N)+1)&0xff] << 8)
//#define GET_W_ZP(N) memcpy(&addr, cpu->zp+(N), 2); addr = MAKELE16(addr)

#define GET_M(AM) \
	switch(AM) { \
	case am_imp1:  abort() ;break; \
	case am_imp2:  abort() ;break; \
	case am_imp3:  abort() ;break; \
	case am_imm:   m = &op.pb[0]; break; \
	case am_zp:    m = &cpu->zp[op.pb[0]]; break; \
	case am_zpx:   m = &cpu->zp[(op.pb[0] + X)&0xff]; break; \
	case am_zpy:   m = &cpu->zp[(op.pb[0] + Y)&0xff]; break; \
	case am_zprel: abort() ; break; \
	case am_ind:	GET_W_ZP(op.pb[0]); \
			m = &op.pb[4]; CPU_READ_N(m, addr, 1); break; \
	case am_izx:	GET_W_ZP((op.pb[0] + X)&0xff); \
			m = &op.pb[4]; CPU_READ_N(m, addr, 1); break; \
	case am_izy:	GET_W_ZP(op.pb[0]); \
			if(pcp && ((addr + Y)^addr)>0xff) cyc += pcp; \
			addr += Y; \
			m = &op.pb[4]; CPU_READ_N(m, addr, 1); break; \
	case am_abs:	GET_W(am); \
			m = &op.pb[4]; CPU_READ_N(m, addr, 1); break; \
	case am_abx:	addr=MAKELE16(op.pw[0]); \
			if(pcp && ((addr + X)^addr)>0xff) cyc += pcp; \
			addr += X; \
			m = &op.pb[4]; CPU_READ_N(m, addr, 1); break; \
	case am_aby:	addr=MAKELE16(op.pw[0]); \
			if(pcp && ((addr + Y)^addr)>0xff) cyc += pcp; \
			addr += Y; \
			m = &op.pb[4]; CPU_READ_N(m, addr, 1); break; \
	case am_abi:	addr=MAKELE16(op.pw[0]); \
			m = &op.pb[4]; CPU_READ_N(m, addr, 2); \
			addr=MAKELE16(op.pw[1]); \
			CPU_READ_N(m, addr, 2); break; \
	case am_abix: abort() ;break; \
	case am_rel: abort() ;break; \
	case am_immzp: m = &cpu->zp[op.pb[1]]; break; \
	case am_immzx: m = &cpu->zp[(op.pb[1] + X) & 0xff]; break; \
	case am_immab: abort() ;break; \
	case am_immax: abort() ;break; \
	case am_acc: m = &A; break; \
	}

#define SET_M(AM, VAL) \
	switch(AM) { \
	case am_ind: \
	case am_izx: \
	case am_izy: \
	case am_abx: \
	case am_aby: \
	case am_abs: \
		*m = VAL; CPU_WRITE_N(m, addr, 1); break; \
	case am_acc: \
	case am_zp: \
	case am_zpx: \
	case am_zpy: \
		*m = VAL; break; \
	default: abort(); \
	}
#if 0
	case am_imp1: break; \
	case am_imp2: break; \
	case am_imp3: break; \
	case am_imm: break; \
	case am_zprel: break; \
	case am_abs: break; \
	case am_abx: break; \
	case am_aby: break; \
	case am_abi: break; \
	case am_abix: break; \
	case am_rel: break; \
	case am_immzp: break; \
	case am_immzx: break; \
	case am_immab: break; \
	case am_immax: break; \
	}
#endif

#define COND_BR8P(COND, TARGET, PENALTY) \
			do { if(!(COND)) break; \
			unsigned pcn = PC + (signed char) TARGET; \
			cyc += PENALTY; \
			cyc += (pcp && ((pcn&0xff00) != (PC&0xff00))) ?pcp:0; \
			PC = pcn; } while(0)
#define COND_BR8(COND, TARGET) \
			COND_BR8P(COND, TARGET, BR_PENALTY)

#define PUSH(VAL)	cpu->stack[cpu->s--] = VAL
#define POP()		cpu->stack[++cpu->s]
#define SET_ZN(VAL)	do {Z = (VAL == 0) ; N =!!(VAL & 0x80);} while (0)
/* http://www.6502.org/tutorials/vflag.html :
 As stated above, the second purpose of the carry flag is to indicate when the
 result of the addition or subtraction is outside the range 0 to 255,
 specifically:

    When the addition result is 0 to 255, the carry is cleared.
    When the addition result is greater than 255, the carry is set.
*   When the subtraction result is 0 to 255, the carry is set.
*   When the subtraction result is less than 0, the carry is cleared.

... and CMP works in terms of subtraction.
*/
#define CMP(VAL)	GET_M(am); tmp = VAL - M; SET_ZN(tmp); \
			C=((int)tmp >= 0)
#define LOAD(VAL)	GET_M(am); VAL = M; SET_ZN(VAL)

#define OP_ADC()	GET_M(am); tmp = A + M + C; \
			if(BCD && D) { if(CPU_TYPE == 0) Z = ((u8)tmp == 0); \
			if (((A & 0xf) + (M & 0xf) + C) > 9) tmp += 6; \
			V = !((A ^ M) & 0x80) && ((A ^ tmp) & 0x80); \
			if(CPU_TYPE == 0) N = !!(tmp & 0x80); \
			else /* N = (tmp >= 0x120) */ ; \
			if (tmp > 0x99) tmp += 96; \
			C = tmp>0x99; A = tmp; \
			if(CPU_TYPE != 0) { Z = (A==0); N = !!(A & 0x80); }\
			} else { \
			V = !((A ^ M) & 0x80) && ((A ^ tmp) & 0x80); \
			C = tmp>0xff; A = tmp; SET_ZN(A); }
#define OP_AHX()	GET_M(am); tmp = A & X & ((addr >> 8)+1); SET_M(am, tmp)
#define OP_ALR()	GET_M(am); tmp = A & M; C = tmp & 1; A = tmp >> 1; SET_ZN(A)
#define OP_ANC()	SET_ZN(A); A = A & op.pb[0] ; SET_ZN(A); C = N
#define OP_AND()	GET_M(am); A = A & M ; SET_ZN(A)
#define OP_ARR()	GET_M(am); tmp = A & M; A = (C << 7) | tmp >> 1; \
			C = !!(tmp & 0x80); SET_ZN(A); \
			V = (A >> 6 ^ A >> 5) & 1
#define OP_ASL()	GET_M(am); C = !!(M & 0x80); tmp = M << 1; \
			SET_M(am, tmp); SET_ZN(M)
#define OP_AXS()	GET_M(am); tmp = A & X ; SET_M(am, tmp)
#define OP_BBR(BIT)	COND_BR8(!(cpu->zp[op.pb[0]] & (1 << BIT)), op.pb[1])
#define OP_BBS(BIT)	COND_BR8(cpu->zp[op.pb[0]] & (1 << BIT), op.pb[1])
#define OP_BCC()	COND_BR8(!C, op.pb[0])
#define OP_BCS()	COND_BR8(C, op.pb[0])
#define OP_BEQ()	COND_BR8(Z, op.pb[0])
			/* attention: BIT imm (added to 65c02) only affects Z
			   http://www.6502.org/tutorials/65c02opcodes.html
			   this might be different on HuC6280, official doc
			   doesn't mention anything being different for imm */
#define OP_BIT()	GET_M(am); tmp = A & M; Z=(tmp == 0); \
			if(am != am_imm) {N=!!(M & 0x80); V=!!(M & 0x40);}
#define OP_BMI()	COND_BR8(N, op.pb[0])
#define OP_BNE()	COND_BR8(!Z, op.pb[0])
#define OP_BPL()	COND_BR8(!N, op.pb[0])
#define OP_BRA()	COND_BR8P(1, op.pb[0], 0)
#define OP_BRK()	++PC; PUSH(cpu->pch); PUSH(cpu->pcl); \
			PUSH(pack_flags(cpu)|B_FLAG); \
			CPU_READ_N(&op.pb[0], INT_VEC, 2); \
			PC = MAKELE16(op.pw[0]); T = T_INIT; I = 1; B = 1; \
			if(!(INT_MASK & D_FLAG)) D = 0
#define OP_BVC()	COND_BR8(!V, op.pb[0])
#define OP_BVS()	COND_BR8(V, op.pb[0])
#define OP_CLC()	C = 0
#define OP_CLD()	D = 0
#define OP_CLI()	I = 0
#define OP_CLV()	V = 0
#define OP_CMP()	CMP(A)
#define OP_CPX()	CMP(X)
#define OP_CPY()	CMP(Y)
#define OP_DCP()	GET_M(am); tmp = M-1; SET_M(am, tmp); tmp = A - M; SET_ZN(tmp); \
                        C=((int)tmp >= 0)
#define OP_DEC()	GET_M(am); tmp = M - 1; SET_M(am, tmp); SET_ZN(M)
#define OP_DEX()	--X ; SET_ZN(X)
#define OP_DEY()	--Y ; SET_ZN(Y)
#define OP_EOR()	GET_M(am); A ^= M; SET_ZN(A)
#define OP_INC()	GET_M(am); tmp = M + 1; SET_M(am, tmp); SET_ZN(M)
#define OP_INX()	++X ; SET_ZN(X)
#define OP_INY()	++Y ; SET_ZN(Y)
#define OP_ISC()	OP_INC(); OP_SBC()
#define OP_JMP()	if(CPU_TYPE == 0 && am == am_abi && op.pb[0] == 0xff) { \
			/* emulate nmos 6502 jump bug */ \
			CPU_READ_N(&op.pb[4], (op.pb[1] << 8) | 0xff, 1); \
			CPU_READ_N(&op.pb[5], (op.pb[1] << 8) | 0x00, 1); \
			PC = MAKELE16(op.pw[2]); \
			} else { GET_W(am); PC = addr; }
#define OP_JSR()	--PC; PUSH(cpu->pch); PUSH(cpu->pcl); PC = MAKELE16(op.pw[0])
#define OP_KIL()	fflush(stdout); __asm__("int3"); for(;;)
#define OP_LAS()	GET_M(am); cpu->s &= M; A = X = cpu->s; SET_ZN(A)
#define OP_LAX()	GET_M(am); A = X = M; SET_ZN(A)
#define OP_LDA()	LOAD(A)
#define OP_LDX()	LOAD(X)
#define OP_LDY()	LOAD(Y)
#define OP_LSR()	GET_M(am); C = M & 1; tmp = M >> 1; SET_ZN(tmp); SET_M(am, tmp)
/* using the magic constant of 0xff, blargg test #2 passes which was verified on real NES. */
#define OP_LXA()	GET_M(am); A |= 0xff; A &= M; X = A; SET_ZN(A)
#define OP_NOP()	if(am == am_abx && pcp) { \
			addr=MAKELE16(op.pw[0]); \
			if(((addr + X)^addr)>0xff) cyc += pcp; }
#define OP_ORA()	GET_M(am); A |= M; SET_ZN(A)
#define OP_PHA()	PUSH(A)
#define OP_PHP()	PUSH(pack_flags(cpu))
#define OP_PHX()	PUSH(X)
#define OP_PHY()	PUSH(Y)
#define OP_PLA()	A = POP() ; SET_ZN(A)
#define OP_PLP()	unpack_flags(cpu, POP() & PLP_MASK)
#define OP_PLX()	X = POP() ; SET_ZN(X)
#define OP_PLY()	Y = POP() ; SET_ZN(Y)
#define OP_RMB(BIT)	cpu->zp[op.pb[0]] &= ~(1 << BIT)
#define OP_RLA()	OP_ROL(); OP_AND()
#define OP_ROL()	GET_M(am); tmp = C; C = !!(M & 0x80); tmp |= (M << 1); \
			SET_M(am, tmp); SET_ZN(M)
#define OP_ROR()	GET_M(am); tmp2 = M & 1; tmp = (C << 7) | (M >> 1); \
			SET_M(am, tmp); C = tmp2; SET_ZN(M)
#define OP_RRA()	OP_ROR(); OP_ADC()
#define OP_RTI()	unpack_flags(cpu, POP()); cpu->pcl = POP(); cpu->pch = POP()
#define OP_RTS()	cpu->pcl = POP(); cpu->pch = POP(); ++PC;
/* these 2 are the OFFICIAL HUC6280 opcodes, not the undocumented 6502 ones,
   we call the latter AXS and SHY */
#define OP_SAX()	tmp = A; A = X; X = tmp
#define OP_SAY()	tmp = A; A = Y; Y = tmp
#define OP_SBC()	GET_M(am); tmp = A - M - (!C); \
			V = !!(((A ^ tmp) & 0x80) && ((A ^ M) & 0x80)); \
			if(!(BCD && D)) SET_ZN((u8)tmp); \
			else { \
			if(CPU_TYPE == 0) SET_ZN((u8)tmp); \
			if (((A & 0xf) - (!C)) < (M & 0xf)) tmp -= 6; \
			if (tmp > 0x99) tmp -= 0x60; \
			if(CPU_TYPE != 0) { Z = ((u8)tmp==0); N = !!((u8)tmp & 0x80); } \
			} \
			C = ((int)tmp >= 0); A = tmp
/* special case AXS variant with immediate */
#define OP_SBX()	tmp = (A & X) - op.pb[0]; SET_ZN(tmp); C=((int)tmp >= 0); X=tmp
#define OP_SEC()	C = 1
#define OP_SED()	D = 1
#define OP_SEI()	I = 1
#define OP_SET()	T = 1
/* first param: addressing mode using x or y, second: data using x or y */
#define SHY_SHX(A_XY, D_XY) \
			op.pb[2] = D_XY & (op.pb[1]+1); \
			addr = (op.pb[0] + A_XY)&0xff; \
			addr |= op.pb[2] << 8; \
			CPU_WRITE_N(&op.pb[2], addr, 1)
#define OP_SHY()	SHY_SHX(X, Y)
#define OP_SHX()	SHY_SHX(Y, X)
#define OP_SLO()	GET_M(am); C = !!(M & 0x80); tmp = M << 1; \
                        SET_M(am, tmp); A |= M; SET_ZN(A)
#define OP_SMB(BIT)	cpu->zp[op.pb[0]] |= (1 << BIT)
#define OP_SRE()	OP_LSR(); OP_EOR()
#define OP_STA()	GET_M(am); SET_M(am, A)
#define OP_STP()	break // TODO this stops the cpu, and needs a reset. should this set some internal flag?
#define OP_STX()	GET_M(am); SET_M(am, X)
#define OP_STY()	GET_M(am); SET_M(am, Y)
#define OP_STZ()	GET_M(am); SET_M(am, 0)
#define OP_SXY()	tmp = X; X = Y; Y = tmp
#define OP_TAS()	GET_M(am); cpu->s = A & X; tmp = cpu->s & ((addr >> 8)+1); \
			SET_M(am, tmp)
#define OP_TAX()	X = A; SET_ZN(A)
#define OP_TAY()	Y = A; SET_ZN(A)
#define OP_TRB()	GET_M(am); tmp = (~A) & M; Z = (!(A & M)); SET_M(am, tmp)
			/* warning: the documentation of HuC6280 states quite
			   different behaviour than 65c02 - memory is NOT written
			   back, V & M are set from "memory" - before or after
			   the operation is unclear, and Z is supposedly set from
			   tmp - which makes more sense than 65c02 behaviour. */
#define OP_TRB_HUCXXX()	GET_M(am); tmp = (~A) & M; N = !!(tmp & 0x80); \
			V = !!(tmp & 0x40); Z = (tmp == 0)
			/* in this case, the huc documentation says the value
			   IS stored in memory, but VN are set from memory,
			   additionally to what 65c02 does; Z behaviour isn't
			   explained. */
#define OP_TSB()	GET_M(am); tmp = A | M ; Z = (!(A & M)); SET_M(am, tmp)
#define OP_TSX()	X = cpu->s; SET_ZN(X)
#define OP_TXA()	A = X; SET_ZN(A)
#define OP_TXS()	cpu->s = X
#define OP_TYA()	A = Y; SET_ZN(A)
#define OP_WAI()	break // TODO: wait for interrupt - we should probably set a flag
#define OP_XAA()	GET_M(am); A = X & M; SET_ZN(A)

#ifdef CPU_DEBUG
#include <stdio.h>
char *trace_fmt(enum address_mode am, u8 p[]) {
	static char buf[64];
	switch(am) {
	case am_abs: sprintf(buf, " $%02x%02x", p[1], p[0]); break;
	case am_abx: sprintf(buf, " $%02x%02x, x", p[1], p[0]); break;
	case am_aby: sprintf(buf, " $%02x%02x, y", p[1], p[0]); break;
	case am_acc: sprintf(buf, " A"); break;
	case am_imp2:
	case am_zp:  sprintf(buf, " $%02x", p[0]); break;
	case am_zpx: sprintf(buf, " $%02x, x", p[0]); break;
	case am_zpy: sprintf(buf, " $%02x, y", p[0]); break;
	case am_imp1: buf[0] = 0; break;
	case am_imp3: sprintf(buf, " $%02x%02x, $%02x%02x, $%02x%02x", p[1], p[0], p[3], p[2], p[5], p[4]); break;
	case am_imm: sprintf(buf, " #$%02x", p[0]); break;
	case am_zprel: sprintf(buf, " $%02x, %d", p[0], (signed char)p[1]); break;
	case am_ind: sprintf(buf, " ($%02x)", p[0]); break;
	case am_izx: sprintf(buf, " ($%02x, x)", p[0]); break;
	case am_izy: sprintf(buf, " ($%02x), y", p[0]); break;
	case am_abi: sprintf(buf, " ($%02x%02x)", p[1], p[0]); break;
	case am_abix: sprintf(buf, " ($%02x%02x, x)", p[1], p[0]); break;
	case am_rel: sprintf(buf, " %d", (signed char)p[0]); break;
	case am_immzp: sprintf(buf, " #$%02x, $%02x", p[0], p[1]); break;
	case am_immzx: sprintf(buf, " #$%02x, $%02x, x", p[0], p[1]); break;
	case am_immab: sprintf(buf, " #$%02x, $%02x%02x", p[0], p[2], p[1]); break;
	case am_immax: sprintf(buf, " #$%02x, $%02x%02x, x", p[0], p[2], p[1]); break;
	}
	return buf;
}
#endif

unsigned cpu65_exec(struct cpu65 *cpu, unsigned mincycles) {
	struct {
		u8 align; // needed for alignment of 16 bit params
		u8 op;  // op, followed by up to 6 parameter bytes on HuC.
		union {
			u8  pb[6];
			u16 pw[3];
		};
		u8 pad; // needed so we can access &op.op using a word-sized memcpy
	} op;
	u8 *m; // points to the memory operand of address mode
	u16 addr; // temporary storage of address needed more than once.
	enum address_mode am;
	unsigned cyc = 0, tmp, tmp2;

#define OPDEF(N, L)  [N] = &&lab_ ## L
	static void *opdisp[256] = {
		#include "optbl.h"
	};
#undef OPDEF

#ifdef CPU_DEBUG
#define TRACE(INSN, AM) if(cpu->trace_print) { char trbuf[128]; \
	sprintf(trbuf, \
	"PC:%04x S:%02x A:%02x X:%02x Y:%02x %c%c%c%c%c%c%c%c O:%02x %s%s", \
	PC, cpu->s, A, X, Y, \
	cpu->f_n?'N':'n', cpu->f_v?'V':'v', cpu->f_t?'T':'t', cpu->f_b?'B':'b', \
	cpu->f_d?'D':'d', cpu->f_i?'I':'i', cpu->f_z?'Z':'z', cpu->f_c?'C':'c', \
	op.op, INSN, trace_fmt(AM, op.pb)); cpu->trace_print(cpu, trbuf); }
#else
#define TRACE(INSN, AM)
#endif
#define FETCH_OP() do { CPU_READ_N(&op.op, cpu->pc, PC_MAX_FETCH); } while(0)
#define DISPATCH() do { FETCH_OP(); goto *opdisp[op.op]; } while(0)
#define CHKDONE() if(cyc >= mincycles) break;
	do {
		DISPATCH();
		#include "oplabels.h"
	} while(0);
	return cyc;
}

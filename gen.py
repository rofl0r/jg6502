from csvdb import CSVDB
import sys, string, os
from cycles import cycles

if len(sys.argv) != 2:
	sys.stderr.write('error: need to specify chip type\n')
	sys.stderr.write('chip type: 0: 6502, 1: 65c02(rev1), 2:wdc 65c02, 3: huc6280\n')
	sys.stderr.write('example: %s 1 (generate table for 65c02)\n'%sys.argv[0])
	sys.exit(1)

# operations that read memory, when address modes indicates so
r_ops = {
	'lda' : 1,
	'ldx' : 1,
	'ldy' : 1,
	'eor' : 1,
	'and' : 1,
	'ora' : 1,
	'adc' : 1,
	'sbc' : 1,
	'cmp' : 1,
	'bit' : 1,
	'lax' : 1,
	'lae' : 1,
	'shs' : 1,
	'nop' : 1,
}

rw_ops = {
	'asl' : 1,
	'lsr' : 1,
	'rol' : 1,
	'ror' : 1,
	'inc' : 1,
	'dec' : 1,
	'slo' : 1,
	'sre' : 1,
	'rla' : 1,
	'rra' : 1,
	'isc' : 1,
	'dcp' : 1,
}

w_ops = {
	'sta' : 1,
	'stx': 1,
	'sty' : 1,
	'stz' : 1,
	'sax' : 1,
	'sha' : 1,
	'shx' : 1,
	'shy' : 1,
}

pcbytes = {
	'imp1' : 1,
	'imp2' : 2,
	'imp3' : 7,
	'imm'  : 2,
	'zp'   : 2,
	'zpx'  : 2,
	'zpy'  : 2,
	'zprel': 3,
	'ind'  : 2,
	'izx'  : 2,
	'izy'  : 2,
	'abs'  : 3,
	'abx'  : 3,
	'aby'  : 3,
	'abi'  : 3,
	'abix' : 3,
	'rel'  : 2,
	'immzp': 3, #hus
	'immzx': 3, #hus
	'immab': 4, #hus
	'immax': 4, #hus
	'acc'  : 1,
}

addr_code = {
	'imp1' : '',
	'imp2' : '',
	'imp3' : '',
	'imm'  : '',
	'zp'   : '',
	'zpx'  : '',
	'zpy'  : '',
	'zprel': '',
	'ind'  : '',
	'izx'  : '',
	'izy'  : '',
	'abs'  : '',
	'abx'  : '',
	'aby'  : '',
	'abi'  : '',
	'abix' : '',
	'rel'  : '',
	'immzp': '', #hus
	'immzx': '', #hus
	'immab': '', #hus
	'immax': '', #hus
	'acc'  : '',
}

addr_trace = {
	'imp1' : '',
	'imp2' : '',
	'imp3' : '',
	'imm'  : '#%02x',
	'zp'   : '',
	'zpx'  : '',
	'zpy'  : '',
	'zprel': '',
	'ind'  : '',
	'izx'  : '',
	'izy'  : '',
	'abs'  : '',
	'abx'  : '',
	'aby'  : '',
	'abi'  : '',
	'abix' : '',
	'rel'  : '',
	'immzp': '', #hus
	'immzx': '', #hus
	'immab': '', #hus
	'immax': '', #hus
	'acc'  : '',
}

def calcaddr(addrmode):
	return addr_code[addrmode]

cpu = int(sys.argv[1])
no_undocumented = os.environ['NOUNDOC'] if 'NOUNDOC' in os.environ else 0
# 65c02 have no undocumented opcodes, all prev. undoc. are nops
if no_undocumented and cpu > 0 and cpu != 3:
	print ("disabling NOUNDOC setting due to selected cpu")
	no_undocumented = 0

def return_all(row, ctx):
	return 1

def return_sel(row, ctx):
	global cpu
	return int(row['chiptype']) <= cpu

def return_undoc(row, ctx):
	undocumented = ctx
	return int(row['opcode'], 16) in undocumented

def main():
	db = CSVDB('opcodes.csv', ',')
	rows = db.query(return_sel)

	opmap = {}
	parent = {}
	addrmode = {}
	for row in rows:
		target = row['mnemonic']
		opcode = int(row['opcode'], 16)
		chip = int(row['chiptype'])
		if opcode in parent:
			if parent[opcode] == chip:
				assert(0)
			if parent[opcode] > chip:
				continue
		opmap[opcode] = target
		parent[opcode] = chip
		addrmode[opcode] = row['address_mode']

	undoc = {}

	out = open('optbl.h', 'w')
	for x in range(256):
		if not x in opmap or (no_undocumented and opmap[x][0] not in string.ascii_lowercase):
			target = 'kil_imp1'
		else:
			target = opmap[x] + '_' + addrmode[x]
			if target[0] not in string.ascii_lowercase:
				target = target[1:]

		comment = ''
		out.write('\tOPDEF(0x%02x, %s),%s\n'%(x, target, comment))
	out.close()

	out = open('oplabels.h', 'w')
	labels = {}
	for x in range(256):
		if not x in opmap or (no_undocumented and opmap[x][0] not in string.ascii_lowercase):
			target = 'kil_imp1'
		else:
			target = opmap[x] + '_' + addrmode[x]
			if target[0] not in string.ascii_lowercase:
				target = target[1:]
		if target in labels: continue
		labels[target] = 1

		op = opmap[x]
		if op[0] not in string.ascii_lowercase:
			op = op[1:]
		opname = op

		op = 'OP_' + op.upper()
		if op[-1:] in string.digits:
			op = op[:-1] + '(%s)'%op[-1:]
		else: op += '()'

		# page cross penalty
		pcp = 'u8 pcp = 0'
		if cpu < 3 and addrmode[x] in ('abx', 'aby', 'izy', 'rel'):
			if (addrmode[x] == 'rel') or (opname in r_ops) or \
			(cpu > 0 and addrmode[x] == 'abx' and opname in rw_ops):
				pcp = 'u8 pcp = 1'

		addr = calcaddr(addrmode[x]) # address mode boilerplate
		out.write('\t#undef am\n\t#define am am_%s\n'%addrmode[x])
		out.write('\tlab_%s: { OPSTART(0x%02x, %s); unsigned tmp, tmp2;/*enum address_mode am = am_%s*/; %s; TRACE("%s", am_%s); cpu->pc += %d; %s; %s; cyc += %d; CHKDONE(); DISPATCH(); }\n'% \
		(target, x, target, addrmode[x], pcp, opmap[x], addrmode[x], pcbytes[addrmode[x]], addr, op, cycles[cpu][x]))
	out.close()

	tmap = { 0: '6502', 1: '65C02', 2: 'R65C02', 3: 'HUC6280' }
	out = open('cpu65type.h', 'w')
	out.write('#define CPU_TYPE CPU_TYPE_%s\n'%tmap[cpu])
	out.close()

# cat oplabels.h | grep OP_ | sed -E 's/.*OP_/OP_/' | sed 's/;.*//' | awk '{print "#define " $0}' | sort -u

	"""
	if cpu > 0:
		if cpu == 1:
			db2 = CSVDB('opcodes_wdc.csv', ',')
		elif cpu == 2:
			db2 = CSVDB('opcodes_rockwell.csv', ',')
		rows = db2.query(return_undoc, undoc)
		for x in range(len(rows)):
			op = rows[x]['mnemonic']
			if op == 'nop': op = '%nop'
			print '%s,%s,%s,%d'%(
				rows[x]['opcode'],
				op,
				rows[x]['address_mode'],
				cpu)
	"""




if __name__ == '__main__':
	main()

from csvdb import CSVDB
import sys, string, os
from cycles import cycles

if len(sys.argv) != 2:
	sys.stderr.write('error: need to specify chip type\n')
	sys.stderr.write('chip type: 0: 6502, 1: 65c02(rev1), 2:wdc 65c02, 3: huc6280\n')
	sys.stderr.write('example: %s 1 (generate table for 65c02)\n'%sys.argv[0])
	sys.exit(1)

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
no_undocumented = os.environ['NOUNDOC'] if 'NOUNDOC' in os.environ else False
# XXX debug
no_undocumented = 1

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
	out.close

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

		op = opmap[x].upper()
		if op[0] not in string.ascii_uppercase:
			op = op[1:]
		op = 'OP_' + op
		if op[-1:] in string.digits:
			op = op[:-1] + '(%s)'%op[-1:]
		else: op += '()'
		addr = calcaddr(addrmode[x]) # address mode boilerplate
		out.write('\tlab_%s: am = am_%s; TRACE("%s", am_%s); cpu->pc += %d; %s; %s; cyc += %d; CHKDONE(); DISPATCH();\n'% \
		(target, addrmode[x], opmap[x], addrmode[x], pcbytes[addrmode[x]], addr, op, cycles[cpu][x]))
	out.close

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

cycles = {
	# mos 6502
	# those labeled as taking 9 cycles in reality KIL the chip, i.e.
	# it doesnt return from the instruction before reset.
	0: [
	7,6,9,8,3,3,5,5,3,2,2,2,4,4,6,6, # 0x
	2,5,9,8,4,4,6,6,2,4,2,7,4,4,7,7, # 1x
	6,6,9,8,3,3,5,5,4,2,2,2,4,4,6,6, # 2x
	2,5,9,8,4,4,6,6,2,4,2,7,4,4,7,7, # 3x
	6,6,9,8,3,3,5,5,3,2,2,2,3,4,6,6, # 4x
	2,5,9,8,4,4,6,6,2,4,2,7,4,4,7,7, # 5x
	6,6,9,8,3,3,5,5,4,2,2,2,5,4,6,6, # 6x
	2,5,9,8,4,4,6,6,2,4,2,7,4,4,7,7, # 7x
	2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4, # 8x
	2,6,9,9,4,4,4,4,2,5,2,9,5,5,5,9, # 9x
	2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4, # Ax
	2,5,9,5,4,4,4,4,2,4,2,9,4,4,4,4, # Bx
	2,6,2,8,3,3,5,5,2,2,2,2,4,4,6,6, # Cx
	2,5,9,8,4,4,6,6,2,4,2,7,4,4,7,7, # Dx
	2,6,2,8,3,3,5,5,2,2,2,2,4,4,6,6, # Ex
	2,5,9,8,4,4,6,6,2,4,2,7,4,4,7,7  # Fx
	],
	# wdc 65c02 rev 1
	# these are the cycles from the original wdc 65c02
	# which lacks the rockwell opcodes that were later
	# added also to wdc models (0x*7, 0x*f).
	# the first revision has them as single-byte/single-cycle nops.
	1: [
	7, 6, 2, 1, 5, 3, 5, 1, 3, 2, 2, 1, 6, 4, 6, 1,
	2, 5, 5, 1, 5, 4, 6, 1, 2, 4, 2, 1, 6, 4, 6, 1,
	6, 6, 2, 1, 3, 3, 5, 1, 4, 2, 2, 1, 4, 4, 6, 1,
	2, 5, 5, 1, 4, 4, 6, 1, 2, 4, 2, 1, 4, 4, 6, 1,
	6, 6, 2, 1, 3, 3, 5, 1, 3, 2, 2, 1, 3, 4, 6, 1,
	2, 5, 5, 1, 4, 4, 6, 1, 2, 4, 3, 1, 8, 4, 6, 1,
	6, 6, 2, 1, 3, 3, 5, 1, 4, 2, 2, 1, 6, 4, 6, 1,
	2, 5, 5, 1, 4, 4, 6, 1, 2, 4, 4, 1, 6, 4, 6, 1,
	3, 6, 2, 1, 3, 3, 3, 1, 2, 2, 2, 1, 4, 4, 4, 1,
	2, 6, 5, 1, 4, 4, 4, 1, 2, 5, 2, 1, 4, 5, 5, 1,
	2, 6, 2, 1, 3, 3, 3, 1, 2, 2, 2, 1, 4, 4, 4, 1,
	2, 5, 5, 1, 4, 4, 4, 1, 2, 4, 2, 1, 4, 4, 4, 1,
	2, 6, 2, 1, 3, 3, 5, 1, 2, 2, 2, 3, 4, 4, 6, 1,
	2, 5, 5, 1, 4, 4, 6, 1, 2, 4, 3, 3, 4, 4, 7, 1,
	2, 6, 2, 1, 3, 3, 5, 1, 2, 2, 2, 1, 4, 4, 6, 1,
	2, 5, 5, 1, 4, 4, 6, 1, 2, 4, 4, 1, 4, 4, 7, 1
	],
	# rockwell/wdc 65c02(later revisions), added opcodes
	# rmbi, smbi, bbri, bbsi.
	# the huc is based on the rockwell-enhanced model
	# which we use for templating.
	2: [
	7, 6, 2, 1, 5, 3, 5, 5, 3, 2, 2, 1, 6, 4, 6, 5,
	2, 5, 5, 1, 5, 4, 6, 5, 2, 4, 2, 1, 6, 4, 6, 5,
	6, 6, 2, 1, 3, 3, 5, 5, 4, 2, 2, 1, 4, 4, 6, 5,
	2, 5, 5, 1, 4, 4, 6, 5, 2, 4, 2, 1, 4, 4, 6, 5,
	6, 6, 2, 1, 3, 3, 5, 5, 3, 2, 2, 1, 3, 4, 6, 5,
	2, 5, 5, 1, 4, 4, 6, 5, 2, 4, 3, 1, 8, 4, 6, 5,
	6, 6, 2, 1, 3, 3, 5, 5, 4, 2, 2, 1, 6, 4, 6, 5,
	2, 5, 5, 1, 4, 4, 6, 5, 2, 4, 4, 1, 6, 4, 6, 5,
	3, 6, 2, 1, 3, 3, 3, 5, 2, 2, 2, 1, 4, 4, 4, 5,
	2, 6, 5, 1, 4, 4, 4, 5, 2, 5, 2, 1, 4, 5, 5, 5,
	2, 6, 2, 1, 3, 3, 3, 5, 2, 2, 2, 1, 4, 4, 4, 5,
	2, 5, 5, 1, 4, 4, 4, 5, 2, 4, 2, 1, 4, 4, 4, 5,
	2, 6, 2, 1, 3, 3, 5, 5, 2, 2, 2, 3, 4, 4, 6, 5,
	2, 5, 5, 1, 4, 4, 6, 5, 2, 4, 3, 3, 4, 4, 7, 5,
	2, 6, 2, 1, 3, 3, 5, 5, 2, 2, 2, 1, 4, 4, 6, 5,
	2, 5, 5, 1, 4, 4, 6, 5, 2, 4, 4, 1, 4, 4, 7, 5
	],
	# huc6280
	# this table originates from mednafen pce_fast
	# since it doesn't deal with undocumented opcodes,
	# it's likely the timings for all rockwell multi-cycle
	# nops are wrong.
	3: [
	8, 7, 3, 4, 6, 4, 6, 7, 3, 2, 2, 2, 7, 5, 7, 6,
	2, 7, 7, 4, 6, 4, 6, 7, 2, 5, 2, 2, 7, 5, 7, 6,
	7, 7, 3, 4, 4, 4, 6, 7, 4, 2, 2, 2, 5, 5, 7, 6,
	2, 7, 7, 2, 4, 4, 6, 7, 2, 5, 2, 2, 5, 5, 7, 6,
	7, 7, 3, 4, 8, 4, 6, 7, 3, 2, 2, 2, 4, 5, 7, 6,
	2, 7, 7, 5, 3, 4, 6, 7, 2, 5, 3, 2, 2, 5, 7, 6,
	7, 7, 2, 2, 4, 4, 6, 7, 4, 2, 2, 2, 7, 5, 7, 6,
	2, 7, 7, 17, 4, 4, 6, 7, 2, 5, 4, 2, 7, 5, 7, 6,
	4, 7, 2, 7, 4, 4, 4, 7, 2, 2, 2, 2, 5, 5, 5, 6,
	2, 7, 7, 8, 4, 4, 4, 7, 2, 5, 2, 2, 5, 5, 5, 6,
	2, 7, 2, 7, 4, 4, 4, 7, 2, 2, 2, 2, 5, 5, 5, 6,
	2, 7, 7, 8, 4, 4, 4, 7, 2, 5, 2, 2, 5, 5, 5, 6,
	2, 7, 2, 17, 4, 4, 6, 7, 2, 2, 2, 2, 5, 5, 7, 6,
	2, 7, 7, 17, 3, 4, 6, 7, 2, 5, 3, 2, 2, 5, 7, 6,
	2, 7, 2, 17, 4, 4, 6, 7, 2, 2, 2, 2, 5, 5, 7, 6,
	2, 7, 7, 17, 2, 4, 6, 7, 2, 5, 4, 2, 2, 5, 7, 6,
	],
}


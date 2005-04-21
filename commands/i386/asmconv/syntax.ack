asmprog:
	comment ?
	statement
	asmprog ; asmprog
	asmprog comment ? \n asmprog

letter:
	[._a-zA-Z]

digit:
	[0-9]

identifier:
	letter (letter | digit)*
	digit [bf]

string:
	'C-like string sequence'
	"C-like string sequence"

number:
	C-like number

comment:
	! .*

statement:
	label-definition statement
	empty
	assignment
	instruction
	pseudo-instruction

label-definition:
	identifier :
	digit :

assignment:
	identifier = expression

instruction:
	iX86-instruction

pseudo-instruction:
	.extern identifier (, identifier)*
	.define identifier (, identifier)*
	.data1 expression (, expression)*
	.data2 expression (, expression)*
	.data4 expression (, expression)*
	.ascii string
	.asciz string
	.align expression
	.space expression
	.comm identifier , expression
	.sect identifier
	.base expression
	.assert expression
	.symb XXX
	.line XXX
	.file XXX
	.nolist
	.list
	iX86-pseudo

expression:
	C-like expression with [ and ] for grouping

iX86-instruction:
	prefix
	prefix iX86-instruction
	identifier
	identifier iX86operand
	identifier iX86operand , iX86operand
	identifier iX86operand : iX86operand

prefix:
	o16
	o32
	a16
	a32
	rep
	repz
	repnz
	repe
	repne
	cseg | dseg | eseg | fseg | gseg | sseg

iX86operand:
	register
	( register )
	expression
	( expression )
	expression ( register )
	expression ( register * [1248] )
	expression ? ( register ) ( register )
	expression ? ( register ) ( register * [1248] )

register:
	al | bl | cl | dl | ah | bh | ch | dh
	ax | bx | cx | dx | si | di | bp | sp
	eax | ebx | ecx | edx | esi | edi | ebp | esp
	cs | ds | es | fs | gs | ss
	cr0 | cr1 | cr2 | cr3

iX86-pseudo:
	.use16
	.use32

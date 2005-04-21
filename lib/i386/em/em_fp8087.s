.sect .text; .sect .rom; .sect .data; .sect .bss
.define one, bigmin

	.sect .rom
one:
	.data2	1
two:
	.data2	2
bigmin:
	.data4 	-2147483648

#
.sect .text; .sect .rom; .sect .data; .sect .bss
.define ERANGE,ESET,EHEAP,ECASE,EILLINS,EIDIVZ,EODDZ
.define .trppc, .ignmask

ERANGE		= 1
ESET		= 2
EIDIVZ		= 6
EHEAP		= 17
EILLINS		= 18
EODDZ		= 19
ECASE		= 20

#include <em_abs.h>

.sect .data
.trppc:
	.data4	0
.ignmask:
	.data4	EIOVFL | EIUND | ECONV | EFOVFL | EFUNFL

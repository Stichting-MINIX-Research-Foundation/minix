#include "SYS.h"

ENTRY(__m68k_read_tp)
	SYSTRAP(_lwp_getprivate)
	movl	%d0,%a0
	rts
END(__m68k_read_tp)

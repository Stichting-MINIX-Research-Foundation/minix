#include "SYS.h"

ENTRY(__aeabi_read_tp)
	mrc p15, 0, r0, c13, c0, 3
#ifndef _ARM_ARCH_6
	cmp r0, #0			/* was it zero? */
	RETc(ne)			/* return it's not zero */
	push {r1}			/* syscall zeroes r1 */
	SYSTRAP(_lwp_getprivate)	/* can't fail */
	pop {r1}			/* restore r1 */
#endif
	RET
_END(__aeabi_read_tp)

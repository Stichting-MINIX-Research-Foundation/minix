/* get_bp.s */
/* */
/* return fp in r0 */
/* */

#include <machine/asm.h>

ENTRY(get_bp)
	mov	r0, fp
	bx	lr

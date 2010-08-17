/*	memmove()					Author: Kees J. Bot */
/*								2 Jan 1994 */

/* void *memmove(void *s1, const void *s2, size_t n) */
/*	Copy a chunk of memory.  Handle overlap. */
/* */
#include <machine/asm.h>

ENTRY(memmove)
	jmp	_C_LABEL(_memmove)	/* Call common code */

/*	rindex()					Author: Kees J. Bot */
/*								2 Jan 1994 */

/* char *rindex(const char *s, int c) */
/*	Look for the last occurrence a character in a string.  Has suffered */
/*	from a hostile takeover by strrchr(). */
/* */
#include <machine/asm.h>

ENTRY(rindex)
	jmp	_C_LABEL(strrchr)

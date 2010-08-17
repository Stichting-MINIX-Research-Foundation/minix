/*	index()						Author: Kees J. Bot */
/*								2 Jan 1994 */

/* char *index(const char *s, int c) */
/*	Look for a character in a string.  Has suffered from a hostile */
/*	takeover by strchr(). */
/* */
#include <machine/asm.h>

ENTRY(index)
	jmp	_C_LABEL(strchr)

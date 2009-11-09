#include <lib.h>
/* memccpy - copy bytes up to a certain char
 *
 * CHARBITS should be defined only if the compiler lacks "unsigned char".
 * It should be a mask, e.g. 0377 for an 8-bit machine.
 */

#include <ansi.h>
#include <stddef.h>

_PROTOTYPE( void *memccpy, (void *dst, const void *src,
			    int ucharstop, size_t size));
#ifndef CHARBITS
#	define	UNSCHAR(c)	((unsigned char)(c))
#else
#	define	UNSCHAR(c)	((c)&CHARBITS)
#endif

void *memccpy(dst, src, ucharstop, size)
void * dst;
_CONST void * src;
int ucharstop;
_SIZET size;
{
  register char *d;
  register _CONST char *s;
  register _SIZET n;
  register int uc;

  if (size <= 0) return( (void *) NULL);

  s = (char *) src;
  d = (char *) dst;
  uc = UNSCHAR(ucharstop);
  for (n = size; n > 0; n--)
	if (UNSCHAR(*d++ = *s++) == (char) uc) return( (void *) d);

  return( (void *) NULL);
}

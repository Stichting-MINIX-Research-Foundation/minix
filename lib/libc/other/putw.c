/* 
 * putw - write an word on a stream
 */
/* $Header$ */

#include	<stdio.h>

_PROTOTYPE(int putw, (int w, FILE *stream ));

int
putw(w, stream)
int w;
register FILE *stream;
{
	register int cnt = sizeof(int);
	register char *p = (char *) &w;

	while (cnt--) {
		(void) putc(*p++, stream);
	}
	if (ferror(stream)) return EOF;
	return w;
}

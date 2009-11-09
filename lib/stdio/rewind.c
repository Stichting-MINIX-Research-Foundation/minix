/*
 * rewind.c - set the file position indicator of a stream to the start
 */
/* $Header$ */

#include	<stdio.h>
#include	"loc_incl.h"

void
rewind(FILE *stream)
{
	(void) fseek(stream, 0L, SEEK_SET);
	clearerr(stream);
}

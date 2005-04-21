/*
 * fsetpos.c - set the position in the file
 */
/* $Header$ */

#include	<stdio.h>

int
fsetpos(FILE *stream, fpos_t *pos)
{
	return fseek(stream, *pos, SEEK_SET);
}

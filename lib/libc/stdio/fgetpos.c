/*
 * fgetpos.c - get the position in the file
 */
/* $Header$ */

#include	<stdio.h>

int
fgetpos(FILE *stream, fpos_t *pos)
{
	*pos = ftell(stream);
	if (*pos == -1) return -1;
	return 0;
}

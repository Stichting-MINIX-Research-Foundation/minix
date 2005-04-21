/*
 * fputc.c - print an unsigned character
 */
/* $Header$ */

#include	<stdio.h>

int
fputc(int c, FILE *stream)
{
	return putc(c, stream);
}

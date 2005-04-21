/*
 * assert.c - diagnostics
 */
/* $Header$ */

#include	<assert.h>
#include	<stdio.h>
#include	<stdlib.h>

void __bad_assertion(const char *mess) {

	fputs(mess, stderr);
	abort();
}

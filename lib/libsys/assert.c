/*
 * assert.c - diagnostics
 */

#include	<assert.h>
#include	<stdio.h>
#include	<minix/config.h>
#include	<minix/const.h>
#include	<minix/sysutil.h>

void __bad_assertion(const char *mess) {
	panic("%s", mess);
}

#include <sys/cdefs.h>
#include "namespace.h"
#include <assert.h>
#include <stdio.h>

#ifdef __weak_alias
__weak_alias(fputs, _fputs)
#endif

int fputs(const char *s, FILE *fp)
{
	assert(fp == stdout || fp == stderr);

	return puts(s);
}

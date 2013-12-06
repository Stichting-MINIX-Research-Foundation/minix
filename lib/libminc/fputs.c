#include <sys/cdefs.h>
#include "namespace.h"
#include <assert.h>
#include <stdio.h>

int fputs(const char *s, FILE *fp)
{
	assert(fp == stdout || fp == stderr);

	return puts(s);
}

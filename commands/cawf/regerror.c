#include <stdio.h>
#include "regexp.h"
#include "proto.h"

void
regerror(s)
char *s;
{
#ifndef DOSPORT
#ifdef ERRAVAIL
	error("regexp: %s", s);
#else
	fprintf(stderr, "regexp(3): %s", s);
	exit(1);
#endif
	/* NOTREACHED */
#endif /* ifdef'd out for less's sake when reporting error inside less */
}

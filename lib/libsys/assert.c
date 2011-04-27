/*
 * assert.c - diagnostics
 */

#include	<assert.h>
#include	<stdio.h>

#ifndef __NBSD_LIBC 
#include	<minix/config.h>
#include	<minix/const.h>
#include	<minix/sysutil.h>

void __bad_assertion(const char *mess) {
	panic("%s", mess);
}

#else /* NBSD_LIBC */

#include <sys/types.h>
#include <stdlib.h>

void
__assert13(file, line, function, failedexpr)
	const char *file, *function, *failedexpr;
	int line;
{

	(void)fprintf(stderr,
	    "assertion \"%s\" failed: file \"%s\", line %d%s%s%s\n",
	    failedexpr, file, line,
	    function ? ", function \"" : "",
	    function ? function : "",
	    function ? "\"" : "");
	abort();
	/* NOTREACHED */
}

void
__assert(file, line, failedexpr)
	const char *file, *failedexpr;
	int line;
{

	__assert13(file, line, NULL, failedexpr);
	/* NOTREACHED */
}

#endif /* NBSD_LIBC */

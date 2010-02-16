/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Header$ */

#if	defined(_POSIX_SOURCE)
#include	<sys/types.h>
#endif
#include	<signal.h>
#include	<stdlib.h>

extern void (*_clean)(void);

void
abort(void)
{
	if (_clean) _clean();		/* flush all output files */
	raise(SIGABRT);
	exit(-1);
	/* NORETURN */
}


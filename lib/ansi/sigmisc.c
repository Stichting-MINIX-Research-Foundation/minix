/*
 * sigmisc.c - used to get a signal mask
 */
/* $Header$ */

#if	defined(_POSIX_SOURCE)

/* This can't be done in setjmp.e, since SIG_SETMASK is defined in
 * <signal.h>. This is a C-file, which can't be included.
 */

#include	<sys/types.h>
#include	<signal.h>
#include	<stddef.h>

int _sigprocmask(int, sigset_t *, sigset_t *);

static void
__testsigset(void) {
	/* This switch compiles when a sigset_t has the right size. */
	switch(0) {
	case 0: 
	case sizeof(sigset_t) <= sizeof(long): break;
	}
}

void
__newsigset(sigset_t *p)
{
	/* The SIG_SETMASK is not significant */
	_sigprocmask(SIG_SETMASK, NULL, p);
}

void
__oldsigset(sigset_t *p)
{
	_sigprocmask(SIG_SETMASK, p, NULL);
}
#endif	/* _POSIX_SOURCE */

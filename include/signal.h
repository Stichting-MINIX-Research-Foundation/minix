/*	$NetBSD: signal.h,v 1.55 2015/07/31 12:51:32 kamil Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)signal.h	8.3 (Berkeley) 3/30/94
 */

#ifndef _SIGNAL_H_
#define _SIGNAL_H_

#include <sys/cdefs.h>
#include <sys/featuretest.h>

#if defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE) || \
    defined(_NETBSD_SOURCE)
#include <sys/types.h>
#endif

#include <sys/signal.h>

#if defined(_NETBSD_SOURCE)
extern const char *const *sys_signame __RENAME(__sys_signame14);
#ifndef __SYS_SIGLIST_DECLARED
#define __SYS_SIGLIST_DECLARED
/* also in unistd.h */
extern const char *const *sys_siglist __RENAME(__sys_siglist14);
#endif /* __SYS_SIGLIST_DECLARED */
extern const int sys_nsig __RENAME(__sys_nsig14);
#endif

__BEGIN_DECLS
int	raise(int);
#if defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE) || \
    defined(_NETBSD_SOURCE)
int	kill(pid_t, int);
int	__libc_sigaction14(int, const struct sigaction * __restrict,
	    struct sigaction * __restrict);

#if (_POSIX_C_SOURCE - 0L) >= 199506L || (_XOPEN_SOURCE - 0) >= 500 || \
    defined(_NETBSD_SOURCE)
int	pthread_sigmask(int, const sigset_t * __restrict,
	    sigset_t * __restrict);
#if !defined(__minix)
int	pthread_kill(pthread_t, int);
#endif /* !defined(__minix) */
int	__libc_thr_sigsetmask(int, const sigset_t * __restrict,
	    sigset_t * __restrict);
#ifndef __LIBPTHREAD_SOURCE__
#define	pthread_sigmask		__libc_thr_sigsetmask
#endif /* __LIBPTHREAD_SOURCE__ */
#endif

#ifndef __LIBC12_SOURCE__
int	sigaction(int, const struct sigaction * __restrict,
    struct sigaction * __restrict) __RENAME(__sigaction14);
#if defined(__minix) && !defined(_SYSTEM)
int	sigaddset(sigset_t *, int) __RENAME(__sigaddset14);
int	sigdelset(sigset_t *, int) __RENAME(__sigdelset14);
int	sigemptyset(sigset_t *) __RENAME(__sigemptyset14);
int	sigfillset(sigset_t *) __RENAME(__sigfillset14);
int	sigismember(const sigset_t *, int) __RENAME(__sigismember14);
#endif /* defined(__minix) && !defined(_SYSTEM) */
int	sigpending(sigset_t *) __RENAME(__sigpending14);
int	sigprocmask(int, const sigset_t * __restrict, sigset_t * __restrict)
    __RENAME(__sigprocmask14);
int	sigsuspend(const sigset_t *) __RENAME(__sigsuspend14);

#if defined(__c99inline) || defined(__SIGSETOPS_BODY)

#if defined(__SIGSETOPS_BODY)
#undef	__c99inline
#define	__c99inline
#endif

/* note: this appears in both errno.h and signal.h */
#ifndef __errno
int *__errno(void);
#define __errno __errno
#endif

/* the same as "errno" - but signal.h is not allowed to define that */
#ifndef ___errno
#define ___errno (*__errno())
#endif

#if defined(__minix) && defined(_SYSTEM)
/* In Minix system code, use alternate versions of the signal mask
 * manipulation functions that do not check signal numbers vs. _NSIG.
 * _NSIG can then represent the user-visible signal set.
 */
#define sigaddset(set, sig)    __sigaddset((set), (sig))
#define sigdelset(set, sig)    __sigdelset((set), (sig))
#define sigemptyset(set)       __sigemptyset((set))
#define sigfillset(set)                __sigfillset((set))
#define sigismember(set, sig)  __sigismember((set), (sig))
#else
__c99inline int
sigaddset(sigset_t *set, int signo)
{
	if (signo <= 0 || signo >= _NSIG) {
		___errno = 22;			/* EINVAL */
		return (-1);
	}
	__sigaddset(set, signo);
	return (0);
}

__c99inline int
sigdelset(sigset_t *set, int signo)
{
	if (signo <= 0 || signo >= _NSIG) {
		___errno = 22;			/* EINVAL */
		return (-1);
	}
	__sigdelset(set, signo);
	return (0);
}

__c99inline int
sigismember(const sigset_t *set, int signo)
{
	if (signo <= 0 || signo >= _NSIG) {
		___errno = 22;			/* EINVAL */
		return (-1);
	}
	return (__sigismember(set, signo));
}

__c99inline int
sigemptyset(sigset_t *set)
{
	__sigemptyset(set);
	return (0);
}

__c99inline int
sigfillset(sigset_t *set)
{
	__sigfillset(set);
	return (0);
}
#endif /* defined(__minix) && defined(_SYSTEM) */
#endif /* __c99inline */
#endif /* !__LIBC12_SOURCE__ */

/*
 * X/Open CAE Specification Issue 4 Version 2
 */      
#if (defined(_XOPEN_SOURCE) && defined(_XOPEN_SOURCE_EXTENDED)) || \
    (_XOPEN_SOURCE - 0) >= 500 || defined(_NETBSD_SOURCE)
int	killpg(pid_t, int);
int	siginterrupt(int, int);
int	sigstack(const struct sigstack *, struct sigstack *);
#ifndef __LIBC12_SOURCE__
int	sigaltstack(const stack_t * __restrict, stack_t * __restrict)
    __RENAME(__sigaltstack14);
#endif
int	sighold(int);
int	sigignore(int);
int	sigpause(int);
int	sigrelse(int);
void	(*sigset (int, void (*)(int)))(int);
#endif /* _XOPEN_SOURCE_EXTENDED || _XOPEN_SOURCE >= 500 || _NETBSD_SOURCE */


/*
 * X/Open CAE Specification Issue 5; IEEE Std 1003.1b-1993 (POSIX)
 */      
#if (_POSIX_C_SOURCE - 0) >= 199309L || (_XOPEN_SOURCE - 0) >= 500 || \
    defined(_NETBSD_SOURCE)
int	sigwait	(const sigset_t * __restrict, int * __restrict);
int	sigwaitinfo(const sigset_t * __restrict, siginfo_t * __restrict);
void	psiginfo(const siginfo_t *, const char *);

#ifndef __LIBC12_SOURCE__
#include <sys/timespec.h>
int	sigtimedwait(const sigset_t * __restrict,
    siginfo_t * __restrict, const struct timespec * __restrict)
    __RENAME(__sigtimedwait50);
int	__sigtimedwait(const sigset_t * __restrict,
    siginfo_t * __restrict, struct timespec * __restrict)
    __RENAME(____sigtimedwait50);
#endif
#endif /* _POSIX_C_SOURCE >= 200112 || _XOPEN_SOURCE_EXTENDED || ... */


#if defined(_NETBSD_SOURCE)
#ifndef __PSIGNAL_DECLARED
#define __PSIGNAL_DECLARED
/* also in unistd.h */
void	psignal(int, const char *);
#endif /* __PSIGNAL_DECLARED */
int	sigblock(int);
int	sigsetmask(int);
#endif /* _NETBSD_SOURCE */

#endif	/* _POSIX_C_SOURCE || _XOPEN_SOURCE || _NETBSD_SOURCE */
__END_DECLS

#endif	/* !_SIGNAL_H_ */

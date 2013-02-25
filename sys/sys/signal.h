/*	$NetBSD: signal.h,v 1.67 2011/01/10 13:56:44 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)signal.h	8.4 (Berkeley) 5/4/95
 */

#ifndef	_SYS_SIGNAL_H_
#define	_SYS_SIGNAL_H_

#include <sys/featuretest.h>
#include <sys/sigtypes.h>

#define _NSIG		26
#define NSIG _NSIG


/* Regular signals. */
#define SIGHUP             1	/* hangup */
#define SIGINT             2	/* interrupt (DEL) */
#define SIGQUIT            3	/* quit (ASCII FS) */
#define SIGILL             4	/* illegal instruction */
#define SIGTRAP            5	/* trace trap (not reset when caught) */
#define SIGABRT            6	/* IOT instruction */
#define SIGBUS             7	/* bus error */
#define SIGFPE             8	/* floating point exception */
#define SIGKILL            9	/* kill (cannot be caught or ignored) */
#define SIGUSR1           10	/* user defined signal # 1 */
#define SIGSEGV           11	/* segmentation violation */
#define SIGUSR2           12	/* user defined signal # 2 */
#define SIGPIPE           13	/* write on a pipe with no one to read it */
#define SIGALRM           14	/* alarm clock */
#define SIGTERM           15	/* software termination signal from kill */
#define SIGEMT		  16	/* EMT instruction */
#define SIGCHLD           17	/* child process terminated or stopped */
#define SIGWINCH    	  21	/* window size has changed */
#define SIGVTALRM         24	/* virtual alarm */
#define SIGPROF           25	/* profiler alarm */

/* POSIX requires the following signals to be defined, even if they are
 * not supported.  Here are the definitions, but they are not supported.
 */
#define SIGCONT           18	/* continue if stopped */
#define SIGSTOP           19	/* stop signal */
#define SIGTSTP           20	/* interactive stop signal */
#define SIGTTIN           22	/* background process wants to read */
#define SIGTTOU           23	/* background process wants to write */

#if defined(__minix) && defined(_NETBSD_SOURCE)
#define SIGIOT             SIGABRT /* for people who speak PDP-11 */

/* MINIX specific signals. These signals are not used by user proceses, 
 * but meant to inform system processes, like the PM, about system events.
 * The order here determines the order signals are processed by system
 * processes in user-space. Higher-priority signals should be first.
 */
/* Signals delivered by a signal manager. */
#define SIGSNDELAY	  26	/* end of delay for signal delivery */

#define SIGS_FIRST	  SIGHUP      /* first system signal */
#define SIGS_LAST	  SIGSNDELAY   /* last system signal */
#define IS_SIGS(signo)    (signo>=SIGS_FIRST && signo<=SIGS_LAST)

/* Signals delivered by the kernel. */
#define SIGKMEM		  27	/* kernel memory request pending */
#define SIGKMESS   	  28	/* new kernel message */
#define SIGKSIGSM    	  29	/* kernel signal pending for signal manager */
#define SIGKSIG    	  30	/* kernel signal pending */

#define SIGK_FIRST	  SIGKMEM      /* first kernel signal */
#define SIGK_LAST	  SIGKSIG     /* last kernel signal */
#define IS_SIGK(signo)    (signo>=SIGK_FIRST && signo<=SIGK_LAST)

/* Termination signals for Minix system processes. */
#define SIGS_IS_LETHAL(sig) \
    (sig == SIGILL || sig == SIGBUS || sig == SIGFPE || sig == SIGSEGV \
    || sig == SIGEMT || sig == SIGABRT)
#define SIGS_IS_TERMINATION(sig) (SIGS_IS_LETHAL(sig) \
    || (sig == SIGKILL || sig == SIGPIPE))
#define SIGS_IS_STACKTRACE(sig) (SIGS_IS_LETHAL(sig) && sig != SIGABRT)

#endif /* defined(__minix) && deinfed(_NETBSD_SOURCE) */

#ifndef _KERNEL
#include <sys/cdefs.h>
#endif

typedef void (*__sighandler_t)(int);

/* Macros used as function pointers. */
#define SIG_ERR    ((__sighandler_t) -1)	/* error return */
#define SIG_DFL	   ((__sighandler_t)  0)	/* default signal handling */
#define SIG_IGN	   ((__sighandler_t)  1)	/* ignore signal */
#define SIG_HOLD   ((__sighandler_t)  2)	/* block signal */
#define SIG_CATCH  ((__sighandler_t)  3)	/* catch signal */

#if defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE) || \
    defined(_NETBSD_SOURCE)

#if (_POSIX_C_SOURCE - 0) >= 199309L || (_XOPEN_SOURCE - 0) >= 500 || \
    defined(_NETBSD_SOURCE)
#include <sys/siginfo.h>
#endif

#if (defined(_XOPEN_SOURCE) && defined(_XOPEN_SOURCE_EXTENDED)) || \
    (_XOPEN_SOURCE - 0) >= 500 || defined(_NETBSD_SOURCE)
#include <sys/ucontext.h>
#endif /* _XOPEN_SOURCE_EXTENDED || _XOPEN_SOURCE >= 500 || _NETBSD_SOURCE */

/*
 * Signal vector "template" used in sigaction call.
 */
struct	sigaction {
	union {
		void (*_sa_handler)(int);
#if (_POSIX_C_SOURCE - 0) >= 199309L || (_XOPEN_SOURCE - 0) >= 500 || \
    defined(_NETBSD_SOURCE)
		void (*_sa_sigaction)(int, siginfo_t *, void *);
#endif
	} _sa_u;	/* signal handler */
	sigset_t sa_mask;		/* signal mask to apply */
	int	sa_flags;		/* see signal options below */
};

#define sa_handler _sa_u._sa_handler
#if (_POSIX_C_SOURCE - 0) >= 199309L || (_XOPEN_SOURCE - 0) >= 500 || \
    defined(_NETBSD_SOURCE)
#define sa_sigaction _sa_u._sa_sigaction
#endif

#include <machine/signal.h>	/* sigcontext; codes for SIGILL, SIGFPE */

/* Fields for sa_flags. */
#define SA_ONSTACK   0x0001	/* deliver signal on alternate stack */
#define SA_RESETHAND 0x0002	/* reset signal handler when signal caught */
#define SA_NODEFER   0x0004	/* don't block signal while catching it */
#define SA_RESTART   0x0008	/* automatic system call restart */
#define SA_SIGINFO   0x0010	/* extended signal handling */
#define SA_NOCLDWAIT 0x0020	/* don't create zombies */
#define SA_NOCLDSTOP 0x0040	/* don't receive SIGCHLD when child stops */

/* POSIX requires these values for use with sigprocmask(2). */
#define SIG_BLOCK          0	/* for blocking signals */
#define SIG_UNBLOCK        1	/* for unblocking signals */
#define SIG_SETMASK        2	/* for setting the signal mask */
#define SIG_INQUIRE        4	/* for internal use only */

#if defined(_NETBSD_SOURCE)
typedef	void (*sig_t)(int);	/* type of signal function */
#endif

#if (defined(_XOPEN_SOURCE) && defined(_XOPEN_SOURCE_EXTENDED)) || \
    (_XOPEN_SOURCE - 0) >= 500 || defined(_NETBSD_SOURCE)
/*
 * Flags used with stack_t/struct sigaltstack.
 */
#define SS_ONSTACK      1      /* Process is executing on an alternate stack */
#define SS_DISABLE      2      /* Alternate stack is disabled */

#define MINSIGSTKSZ	2048	/* Minimal stack size is 2k */
#define	SIGSTKSZ	(MINSIGSTKSZ + 32768)	/* recommended stack size */
#endif /* _XOPEN_SOURCE_EXTENDED || _XOPEN_SOURCE >= 500 || _NETBSD_SOURCE */

#if (defined(_XOPEN_SOURCE) && defined(_XOPEN_SOURCE_EXTENDED)) || \
    (_XOPEN_SOURCE - 0) >= 500 || defined(_NETBSD_SOURCE)
/*
 * Structure used in sigstack call.
 */
struct	sigstack {
	void	*ss_sp;			/* signal stack pointer */
	int	ss_onstack;		/* current status */
};
#endif /* _XOPEN_SOURCE_EXTENDED || _XOPEN_SOURCE >= 500 || _NETBSD_SOURCE */

#if defined(_NETBSD_SOURCE) && !defined(_KERNEL)
/*
 * Macro for converting signal number to a mask suitable for
 * sigblock().
 */
#define sigmask(n)	__sigmask(n)

#define	BADSIG		SIG_ERR
#endif /* _NETBSD_SOURCE */

#if (_POSIX_C_SOURCE - 0) >= 199309L || (_XOPEN_SOURCE - 0) >= 500 || \
    defined(_NETBSD_SOURCE)
struct	sigevent {
	int	sigev_notify;
	int	sigev_signo;
	union sigval	sigev_value;
	void	(*sigev_notify_function)(union sigval);
	void /* pthread_attr_t */	*sigev_notify_attributes;
};

#define SIGEV_NONE	0
#define SIGEV_SIGNAL	1
#define SIGEV_THREAD	2
#if defined(_NETBSD_SOURCE)
#define SIGEV_SA	3
#endif
#endif /* (_POSIX_C_SOURCE - 0) >= 199309L || ... */

#endif	/* _POSIX_C_SOURCE || _XOPEN_SOURCE || _NETBSD_SOURCE */

/*
 * For historical reasons; programs expect signal's return value to be
 * defined by <sys/signal.h>.
 */
__BEGIN_DECLS
void	(*signal(int, void (*)(int)))(int);
__END_DECLS
#endif	/* !_SYS_SIGNAL_H_ */

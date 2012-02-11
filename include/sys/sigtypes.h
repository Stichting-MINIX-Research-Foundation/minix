#ifndef	_SYS_SIGTYPES_H_
#define	_SYS_SIGTYPES_H_

/*
 * This header file defines various signal-related types.  We also keep
 * the macros to manipulate sigset_t here, to encapsulate knowledge of
 * its internals.
 */

#include <sys/featuretest.h>
#include <machine/int_types.h>
#include <machine/ansi.h>

#ifdef	_BSD_SIZE_T_
typedef	_BSD_SIZE_T_	size_t;
#undef	_BSD_SIZE_T_
#endif

#if defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE) || \
    defined(_NETBSD_SOURCE)
typedef unsigned long sigset_t;

/*
 * Macro for manipulating signal masks.
 */
#ifndef __minix
#define __sigmask(n)		(1 << (((unsigned int)(n) - 1)))
#else /* __minix */
#define __sigmask(n)		(1 << (unsigned int)(n))
#endif /* !__minix */ 
#define __sigaddset(s, n)					\
	do {							\
		*(s) = *(unsigned long *)(s) | __sigmask(n);	\
	} while(0)
#define __sigdelset(s, n)					\
	do {							\
		*(s) = *(unsigned long *)(s) & ~__sigmask(n);	\
	} while (0)

#define __sigismember(s, n)	(((*(const unsigned long *)(s)) & __sigmask(n)) != 0)
#define __sigemptyset(s)	(*(unsigned long *)(s) = 0)
#define __sigsetequal(s1, s2)	(*(unsigned long *)(s1) = *(unsigned long *)(s2))
#define __sigfillset(s)		(*(long *)(s) = -1L)
#define __sigplusset(s, t)						\
	do {								\
		*(t) = *(unsigned long *)(t) | *(unsigned long *)(s);	\
	} while (0)
#define __sigminusset(s, t)					\
	do {							\
		*(t) = *(unsigned long *)(t) & ~*(unsigned long *)(s);	\
	} while (0)
#define __sigandset(s, t)					\
	do {							\
		*(t) = *(unsigned long *)(t) & *(unsigned long *)(s);	\
	} while (0)

#if (defined(_XOPEN_SOURCE) && defined(_XOPEN_SOURCE_EXTENDED)) || \
    (_XOPEN_SOURCE - 0) >= 500 || defined(_NETBSD_SOURCE)
typedef struct
#if defined(_NETBSD_SOURCE)
               sigaltstack
#endif /* _NETBSD_SOURCE */
			   {
	void	*ss_sp;			/* signal stack base */
	size_t	ss_size;		/* signal stack length */
	int	ss_flags;		/* SS_DISABLE and/or SS_ONSTACK */
} stack_t;

#endif /* _XOPEN_SOURCE_EXTENDED || XOPEN_SOURCE >= 500 || _NETBSD_SOURCE */

#endif	/* _POSIX_C_SOURCE || _XOPEN_SOURCE || ... */
#endif	/* !_SYS_SIGTYPES_H_ */

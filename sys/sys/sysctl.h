#ifndef _SYS_SYSCTL_H
#define _SYS_SYSCTL_H

/*
 * sysctl() is not supported. Warn non-libc programs including this header.
 */
#ifndef _LIBC
#warning Including sysctl.h header. sysctl() is not supported in Minix.
#endif /* !_LIBC */

/*
 * Used by gmon.
 */
struct clockinfo {
	int	hz;		/* clock frequency */
	int	tick;		/* micro-seconds per hz tick */
	int	tickadj;	/* clock skew rate for adjtime() */
	int	stathz;		/* statistics clock frequency */
	int	profhz;		/* profiling clock frequency */
};

#endif /* _SYS_SYSCTL_H */

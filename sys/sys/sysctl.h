#ifndef _SYS_SYSCTL_H
#define _SYS_SYSCTL_H

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

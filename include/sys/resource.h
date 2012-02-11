#ifndef _SYS_RESOURCE_H_
#define	_SYS_RESOURCE_H_

#include <sys/featuretest.h>
#include <sys/time.h>

/*
 * Process priority specifications to get/setpriority.
 */
#define	PRIO_MIN	-20
#define	PRIO_MAX	20

#define	PRIO_PROCESS	0
#define	PRIO_PGRP	1
#define	PRIO_USER	2

/*
 * Resource limits
 */
#define RLIMIT_CORE	1
#define RLIMIT_CPU	2
#define RLIMIT_DATA	3
#define RLIMIT_FSIZE	4
#define RLIMIT_NOFILE	5
#define RLIMIT_STACK	6
#define RLIMIT_AS	7
#define	RLIMIT_VMEM	RLIMIT_AS	/* common alias */

#if defined(_NETBSD_SOURCE)
#define	RLIM_NLIMITS	8		/* number of resource limits */
#endif

#define RLIM_INFINITY ((rlim_t) -1)
#define RLIM_SAVED_CUR RLIM_INFINITY
#define RLIM_SAVED_MAX RLIM_INFINITY

struct rlimit
{
	rlim_t rlim_cur;
	rlim_t rlim_max;
};

#include <sys/cdefs.h>

__BEGIN_DECLS
int	getpriority(int, int);
int	getrlimit(int, struct rlimit *);
int	setpriority(int, int, int);
__END_DECLS

#endif	/* !_SYS_RESOURCE_H_ */

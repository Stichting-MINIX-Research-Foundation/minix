#ifndef _SYS_RESOURCE_H
#define _SYS_RESOURCE_H

/* Priority range for the get/setpriority() interface.
 * It isn't a mapping on the internal minix scheduling
 * priority.
 */
#define PRIO_MIN	-20
#define PRIO_MAX	 20

#define PRIO_PROCESS	0
#define PRIO_PGRP	1
#define PRIO_USER	2

int getpriority(int, int);
int setpriority(int, int, int);

#ifdef _POSIX_SOURCE

#include <sys/time.h>

typedef unsigned long rlim_t;

#define RLIM_INFINITY ((rlim_t) -1)
#define RLIM_SAVED_CUR RLIM_INFINITY
#define RLIM_SAVED_MAX RLIM_INFINITY

struct rlimit
{
	rlim_t rlim_cur;
	rlim_t rlim_max;
};

#define RLIMIT_CORE	1
#define RLIMIT_CPU	2
#define RLIMIT_DATA	3
#define RLIMIT_FSIZE	4
#define RLIMIT_NOFILE	5
#define RLIMIT_STACK	6
#define RLIMIT_AS	7

int getrlimit(int resource, struct rlimit *rlp);

#endif /* defined(_POSIX_SOURCE) */

#endif

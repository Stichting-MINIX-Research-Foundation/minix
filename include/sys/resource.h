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

#endif

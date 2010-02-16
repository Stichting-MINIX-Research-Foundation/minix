/*
nice.c
*/

#include <errno.h>
#include <unistd.h>
#include <sys/resource.h>

int nice(incr)
int incr;
{
	int r;

	errno= 0;
	r= getpriority(PRIO_PROCESS, 0);
	if (r == -1 && errno != 0)
		return r;
	return setpriority(PRIO_PROCESS, 0, r+incr);
}

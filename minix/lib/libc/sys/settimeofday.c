#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <sys/time.h>
#include <time.h>

int settimeofday(const struct timeval *tp, const void *tzp)
{
	struct timespec ts;

	ts.tv_sec = tp->tv_sec;
	ts.tv_nsec = tp->tv_usec * 1000;
	
	/* Ignore time zones */
	return clock_settime(CLOCK_REALTIME, &ts);
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(settimeofday, __settimeofday50)
#endif

#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <sys/time.h>
#include <time.h>

int settimeofday(const struct timeval *tp, const void *tzp)
{
	/* Use intermediate variable because stime param is not const */
	time_t sec = tp->tv_sec;
	
	/* Ignore time zones */
	return stime(&sec);
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(settimeofday, __settimeofday50)
#endif

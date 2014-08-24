
#include "sysutil.h"
#include <sys/time.h>

/*
 * This routine returns the time in seconds since 1.1.1970.  MINIX is an
 * astrophysically naive system that assumes the earth rotates at a constant
 * rate and that such things as leap seconds do not exist.  If a non-NULL
 * pointer to a timespec structure is given, that structure is filled with
 * the current time in subsecond precision.
 */
time_t
clock_time(struct timespec *tv)
{
	uint32_t system_hz;
	clock_t uptime, realtime;
	time_t boottime, sec;
	int r;

	if ((r = getuptime(&uptime, &realtime, &boottime)) != OK)
		panic("clock_time: getuptime failed: %d", r);

	system_hz = sys_hz();	/* sys_hz() caches its return value */

	sec = boottime + realtime / system_hz;

	if (tv != NULL) {
		tv->tv_sec = sec;

		/*
		 * We do not want to overflow, and system_hz can be as high as
		 * 50kHz.
		 */
		if (system_hz < LONG_MAX / 40000)
			tv->tv_nsec = (realtime % system_hz) * 40000 /
			    system_hz * 25000;
		else
			tv->tv_nsec = 0;	/* bad, but what's better? */
	}

	return sec;
}

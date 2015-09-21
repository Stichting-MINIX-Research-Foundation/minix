
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
	struct minix_kerninfo *minix_kerninfo;
	uint32_t system_hz;
	clock_t realtime;
	time_t boottime, sec;

	minix_kerninfo = get_minix_kerninfo();

	/* We assume atomic 32-bit field retrieval.  TODO: 64-bit support. */
	boottime = minix_kerninfo->kclockinfo->boottime;
	realtime = minix_kerninfo->kclockinfo->realtime;
	system_hz = minix_kerninfo->kclockinfo->hz;

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

/*	nanosleep() - Sleep for a number of seconds.	Author: Erik van der Kouwe
 *								25 July 2009
 */

#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/select.h>
#include <sys/time.h>

#define MSEC_PER_SEC 1000
#define USEC_PER_MSEC 1000
#define NSEC_PER_USEC 1000

#define USEC_PER_SEC (USEC_PER_MSEC * MSEC_PER_SEC)
#define NSEC_PER_SEC (NSEC_PER_USEC * USEC_PER_SEC)

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
	struct timeval timeout, timestart = { 0, 0 }, timeend;
	int errno_select, r;
	struct timespec rqt;

	/* check parameters */
	if (!rqtp)
		return EFAULT;

	if (rqtp->tv_sec < 0 || 
		rqtp->tv_nsec < 0 ||
		rqtp->tv_nsec >= NSEC_PER_SEC)
		return EINVAL;

	/* store *rqtp to make sure it is not overwritten */
	rqt = *rqtp;
	
	/* keep track of start time if needed */
	if (rmtp)
	{
		rmtp->tv_sec = 0;
		rmtp->tv_nsec = 0;
		if (gettimeofday(&timestart, NULL) < 0)
			return -1;
	}

	/* use select to wait */
	timeout.tv_sec = rqt.tv_sec;
	timeout.tv_usec = (rqt.tv_nsec + NSEC_PER_USEC - 1) / NSEC_PER_USEC;
	r = select(0, NULL, NULL, NULL, &timeout);

	/* return remaining time only if requested */
	/* if select succeeded then we slept all time */
	if (!rmtp || r >= 0)
		return r;

	/* measure end time; preserve errno */
	errno_select = errno;
	if (gettimeofday(&timeend, NULL) < 0)
		return -1;

	errno = errno_select;

	/* compute remaining time */
	rmtp->tv_sec = rqt.tv_sec - (timeend.tv_sec - timestart.tv_sec);
	rmtp->tv_nsec = rqt.tv_nsec - (timeend.tv_usec - timestart.tv_usec) * NSEC_PER_USEC;

	/* bring remaining time into canonical form */
	while (rmtp->tv_nsec < 0)
	{
		rmtp->tv_sec -= 1;
		rmtp->tv_nsec += NSEC_PER_SEC;
	}

	while (rmtp->tv_nsec > NSEC_PER_SEC)
	{
		rmtp->tv_sec += 1;
		rmtp->tv_nsec -= NSEC_PER_SEC;
	}

	/* remaining time must not be negative */
	if (rmtp->tv_sec < 0)
	{
		rmtp->tv_sec = 0;
		rmtp->tv_nsec = 0;
	}

	return r;
}


#if defined(__minix) && defined(__weak_alias)
__weak_alias(nanosleep, __nanosleep50)
#endif

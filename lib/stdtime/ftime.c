/* Ported from glibc */

#include <sys/timeb.h>
#include <sys/time.h>

int ftime(struct timeb *timebuf)
{
	struct timeval tv;
	struct timezone tz;

	if (gettimeofday (&tv, &tz) < 0)
		return -1;

	timebuf->time = tv.tv_sec;
	timebuf->millitm = (tv.tv_usec + 500) / 1000;
	if (timebuf->millitm == 1000)
	{
		++timebuf->time;
		timebuf->millitm = 0;
	}
	timebuf->timezone = tz.tz_minuteswest;
	timebuf->dstflag = tz.tz_dsttime;
	return 0;
}

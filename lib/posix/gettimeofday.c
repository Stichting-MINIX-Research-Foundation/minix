/*
gettimeofday.c
*/

#include <sys/time.h>
#include <time.h>

int gettimeofday(struct timeval *_RESTRICT tp, void *_RESTRICT tzp)
{
	if (time(&tp->tv_sec) == (time_t)-1)
		return -1;
	tp->tv_usec= 0;

	/* tzp has to be a nul pointer according to the standard. Otherwise
	 * behavior is undefined. We can just ignore tzp.
	 */
	return 0;
}

/*
settimeofday.c
*/

#define stime _stime

#include <sys/time.h>
#include <time.h>

int settimeofday(const struct timeval *tp, const void *tzp)
{
	/* Use intermediate variable because stime param is not const */
	time_t sec = tp->tv_sec;
	
	/* Ignore time zones */
	return stime(&sec);
}

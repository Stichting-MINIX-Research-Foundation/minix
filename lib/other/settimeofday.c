/*
settimeofday.c
*/

#define stime _stime

#include <sys/time.h>
#include <time.h>

int settimeofday(const struct timeval *tp, const void *tzp)
{
	/* Ignore time zones */
	return stime(&tp->tv_sec);
}

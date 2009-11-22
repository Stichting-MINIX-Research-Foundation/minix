/*
sys/time.h
*/

#ifndef _SYS__TIME_H
#define _SYS__TIME_H

#include <ansi.h>

/* Open Group Base Specifications Issue 6 (not complete) */
struct timeval
{
	long /*time_t*/ tv_sec;
	long /*useconds_t*/ tv_usec;
};

struct timezone {
	int     tz_minuteswest; /* minutes west of Greenwich */
	int     tz_dsttime;     /* type of dst correction */
};

int gettimeofday(struct timeval *_RESTRICT tp, void *_RESTRICT tzp);

/* Compatibility with other Unix systems */
int settimeofday(const struct timeval *tp, const void *tzp);

/* setitimer/getitimer interface */
struct itimerval
{
	struct timeval it_interval;
	struct timeval it_value;
};

#define ITIMER_REAL 0
#define ITIMER_VIRTUAL 1	/* Not implemented */
#define ITIMER_PROF 2		/* Not implemented */

int getitimer(int which, struct itimerval *value);
int setitimer(int which, const struct itimerval *_RESTRICT value,
		struct itimerval *_RESTRICT ovalue);

#include <sys/select.h>

#endif /* _SYS__TIME_H */

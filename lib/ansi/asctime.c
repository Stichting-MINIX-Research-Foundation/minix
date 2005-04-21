/*
 * asctime - print a date
 */
/* $Header$ */

#include	<string.h>
#include	<time.h>
#include	"loc_time.h"

#define	DATE_STR	"??? ??? ?? ??:??:?? ????\n"

static char *
two_digits(register char *pb, int i, int nospace)
{
	*pb = (i / 10) % 10 + '0';
	if (!nospace && *pb == '0') *pb = ' ';
	pb++;
	*pb++ = (i % 10) + '0';
	return ++pb;
}

static char *
four_digits(register char *pb, int i)
{
	i %= 10000;
	*pb++ = (i / 1000) + '0';
	i %= 1000;
	*pb++ = (i / 100) + '0';
	i %= 100;
	*pb++ = (i / 10) + '0';
	*pb++ = (i % 10) + '0';
	return ++pb;
}

char *asctime(const struct tm *timeptr)
{
	static char buf[26];
	register char *pb = buf;
	register const char *ps;
	register int n;

	strcpy(pb, DATE_STR);
	ps = _days[timeptr->tm_wday];
	n = ABB_LEN;
	while(--n >= 0) *pb++ = *ps++;
	pb++;
	ps = _months[timeptr->tm_mon];
	n = ABB_LEN;
	while(--n >= 0) *pb++ = *ps++;
	pb++;
	pb = two_digits(
		    two_digits(
			    two_digits(two_digits(pb, timeptr->tm_mday, 0)
					, timeptr->tm_hour, 1)
			    , timeptr->tm_min, 1)
		    , timeptr->tm_sec, 1);

	four_digits(pb, timeptr->tm_year + 1900);
	return buf;
}

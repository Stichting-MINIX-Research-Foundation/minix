/*
 * mktime - convert local time into calendar time
 */
/* $Header$ */

/* Michael A. Temari <temari@ix.netcom.com>   03/01/96    */
/*   -  fixed bug is structure fixup code                 */

#include	<time.h>
#include	<limits.h>
#include	"loc_time.h"

/* The code assumes that unsigned long can be converted to time_t.
 * A time_t should not be wider than unsigned long, since this would mean
 * that the check for overflow at the end could fail.
 */
time_t
mktime(register struct tm *timep)
{
	register long day, year;
	register int tm_year;
	int yday, month;
	register unsigned long seconds;
	int overflow;
	unsigned dst;

	timep->tm_min += timep->tm_sec / 60;
	timep->tm_sec %= 60;
	if (timep->tm_sec < 0) {
		timep->tm_sec += 60;
		timep->tm_min--;
	}
	timep->tm_hour += timep->tm_min / 60;
	timep->tm_min = timep->tm_min % 60;
	if (timep->tm_min < 0) {
		timep->tm_min += 60;
		timep->tm_hour--;
	}
	day = timep->tm_hour / 24;
	timep->tm_hour= timep->tm_hour % 24;
	if (timep->tm_hour < 0) {
		timep->tm_hour += 24;
		day--;
	}
	timep->tm_year += timep->tm_mon / 12;
	timep->tm_mon %= 12;
	if (timep->tm_mon < 0) {
		timep->tm_mon += 12;
		timep->tm_year--;
	}
	day += (timep->tm_mday - 1);
	while (day < 0) {
		if(--timep->tm_mon < 0) {
			timep->tm_year--;
			timep->tm_mon = 11;
		}
		day += _ytab[LEAPYEAR(YEAR0 + timep->tm_year)][timep->tm_mon];
	}
	while (day >= _ytab[LEAPYEAR(YEAR0 + timep->tm_year)][timep->tm_mon]) {
		day -= _ytab[LEAPYEAR(YEAR0 + timep->tm_year)][timep->tm_mon];
		if (++(timep->tm_mon) == 12) {
			timep->tm_mon = 0;
			timep->tm_year++;
		}
	}
	timep->tm_mday = day + 1;
	_tzset();			/* set timezone and dst info  */
	year = EPOCH_YR;
	if (timep->tm_year < year - YEAR0) return (time_t)-1;
	seconds = 0;
	day = 0;			/* means days since day 0 now */
	overflow = 0;

	/* Assume that when day becomes negative, there will certainly
	 * be overflow on seconds.
	 * The check for overflow needs not to be done for leapyears
	 * divisible by 400.
	 * The code only works when year (1970) is not a leapyear.
	 */
#if	EPOCH_YR != 1970
#error	EPOCH_YR != 1970
#endif
	tm_year = timep->tm_year + YEAR0;

	if (LONG_MAX / 365 < tm_year - year) overflow++;
	day = (tm_year - year) * 365;
	if (LONG_MAX - day < (tm_year - year) / 4 + 1) overflow++;
	day += (tm_year - year) / 4
		+ ((tm_year % 4) && tm_year % 4 < year % 4);
	day -= (tm_year - year) / 100
		+ ((tm_year % 100) && tm_year % 100 < year % 100);
	day += (tm_year - year) / 400
		+ ((tm_year % 400) && tm_year % 400 < year % 400);

	yday = month = 0;
	while (month < timep->tm_mon) {
		yday += _ytab[LEAPYEAR(tm_year)][month];
		month++;
	}
	yday += (timep->tm_mday - 1);
	if (day + yday < 0) overflow++;
	day += yday;

	timep->tm_yday = yday;
	timep->tm_wday = (day + 4) % 7;		/* day 0 was thursday (4) */

	seconds = ((timep->tm_hour * 60L) + timep->tm_min) * 60L + timep->tm_sec;

	if ((TIME_MAX - seconds) / SECS_DAY < day) overflow++;
	seconds += day * SECS_DAY;

	/* Now adjust according to timezone and daylight saving time */

	if (((_timezone > 0) && (TIME_MAX - _timezone < seconds))
	    || ((_timezone < 0) && (seconds < -_timezone)))
		overflow++;
	seconds += _timezone;

	if (timep->tm_isdst < 0)
		dst = _dstget(timep);
	else if (timep->tm_isdst)
		dst = _dst_off;
	else dst = 0;

	if (dst > seconds) overflow++;	/* dst is always non-negative */
	seconds -= dst;

	if (overflow) return (time_t)-1;

	if ((time_t)seconds != seconds) return (time_t)-1;
	return (time_t)seconds;
}

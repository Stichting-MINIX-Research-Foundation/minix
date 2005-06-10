/* The <time.h> header is used by the procedures that deal with time.
 * Handling time is surprisingly complicated, what with GMT, local time
 * and other factors.  Although the Bishop of Ussher (1581-1656) once
 * calculated that based on the Bible, the world began on 12 Oct. 4004 BC
 * at 9 o'clock in the morning, in the UNIX world time begins at midnight, 
 * 1 Jan. 1970 GMT.  Before that, all was NULL and (void).
 */

#ifndef _TIME_H
#define _TIME_H

#define CLOCKS_PER_SEC    60	/* MINIX always uses 60 Hz, even in Europe */

#ifdef _POSIX_SOURCE
#define CLK_TCK CLOCKS_PER_SEC	/* obsolescent mame for CLOCKS_PER_SEC */
#endif

#define NULL    ((void *)0)

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

#ifndef _TIME_T
#define _TIME_T
typedef long time_t;		/* time in sec since 1 Jan 1970 0000 GMT */
#endif

#ifndef _CLOCK_T
#define _CLOCK_T
typedef long clock_t;		/* time in ticks since process started */
#endif

struct tm {
  int tm_sec;			/* seconds after the minute [0, 59] */
  int tm_min;			/* minutes after the hour [0, 59] */
  int tm_hour;			/* hours since midnight [0, 23] */
  int tm_mday;			/* day of the month [1, 31] */
  int tm_mon;			/* months since January [0, 11] */
  int tm_year;			/* years since 1900 */
  int tm_wday;			/* days since Sunday [0, 6] */
  int tm_yday;			/* days since January 1 [0, 365] */
  int tm_isdst;			/* Daylight Saving Time flag */
};

extern char *tzname[];

/* Function Prototypes. */
#ifndef _ANSI_H
#include <ansi.h>
#endif

_PROTOTYPE( clock_t clock, (void)					);
_PROTOTYPE( double difftime, (time_t _time1, time_t _time0)		);
_PROTOTYPE( time_t mktime, (struct tm *_timeptr)			);
_PROTOTYPE( time_t time, (time_t *_timeptr)				);
_PROTOTYPE( char *asctime, (const struct tm *_timeptr)			);
_PROTOTYPE( char *ctime, (const time_t *_timer)			);
_PROTOTYPE( struct tm *gmtime, (const time_t *_timer)			);
_PROTOTYPE( struct tm *localtime, (const time_t *_timer)		);
_PROTOTYPE( size_t strftime, (char *_s, size_t _max, const char *_fmt,
				const struct tm *_timep)		);

#ifdef _POSIX_SOURCE
_PROTOTYPE( void tzset, (void)						);
#endif

#ifdef _MINIX
_PROTOTYPE( int stime, (time_t *_top)					);
#endif

extern long timezone;

#endif /* _TIME_H */

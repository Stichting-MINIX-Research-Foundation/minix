/*
 * misc - data and miscellaneous routines
 */
/* $Header$ */

#include	<ctype.h>
#include	<time.h>
#include	<stdlib.h>
#include	<string.h>

#if	defined(__BSD4_2)

struct timeval {
	long	tv_sec;		/* seconds */
	long	tv_usec;	/* and microseconds */
};

struct timezone {
	int	tz_minuteswest;	/* minutes west of Greenwich */
	int	tz_dsttime;	/* type of dst correction */
};

int _gettimeofday(struct timeval *tp, struct timezone *tzp);

#elif	!defined(_POSIX_SOURCE) && !defined(__USG)
#if	!defined(_MINIX)		/* MINIX has no ftime() */
struct timeb {
	long	time;
	unsigned short millitm;
	short	timezone;
	short	dstflag;
};
void _ftime(struct timeb *bp);
#endif
#endif

#include	"loc_time.h"

#define	RULE_LEN	120
#define	TZ_LEN		10

/* Make sure that the strings do not end up in ROM.
 * These strings probably contain the wrong value, and we cannot obtain the
 * right value from the system. TZ is the only help.
 */
static char ntstr[TZ_LEN + 1] = "GMT";	/* string for normal time */
static char dststr[TZ_LEN + 1] = "GDT";	/* string for daylight saving */

long	_timezone = 0;
long	_dst_off = 60 * 60;
int	_daylight = 0;
char	*_tzname[2] = {ntstr, dststr};

#if	defined(__USG) || defined(_POSIX_SOURCE)
char	*tzname[2] = {ntstr, dststr};

#if	defined(__USG)
long	timezone = 0;
int	daylight = 0;
#endif
#endif

static struct dsttype {
	char ds_type;		/* Unknown, Julian, Zero-based or M */
	int ds_date[3];		/* months, weeks, days */
	long ds_sec;		/* usually 02:00:00 */
}	dststart = { 'U', { 0, 0, 0 }, 2 * 60 * 60 }
	, dstend = { 'U', { 0, 0, 0 }, 2 * 60 * 60 };

const char *_days[] = {
			"Sunday", "Monday", "Tuesday", "Wednesday",
			"Thursday", "Friday", "Saturday"
		};

const char *_months[] = {
			"January", "February", "March",
			"April", "May", "June",
			"July", "August", "September",
			"October", "November", "December"
		};

const int _ytab[2][12] = {
		{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
		{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
	};

static const char *
parseZoneName(register char *buf, register const char *p)
{
	register int n = 0;

	if (*p == ':') return NULL;
	while (*p && !isdigit(*p) && *p != ',' && *p != '-' && *p != '+') {
		if (n < TZ_LEN)
			*buf++ = *p;
		p++;
		n++;
	}
	if (n < 3) return NULL;				/* error */
	*buf = '\0';
	return p;
}

static const char *
parseTime(register long *tm, const char *p, register struct dsttype *dst)
{
	register int n = 0;
	register const char *q = p;
	char ds_type = (dst ? dst->ds_type : '\0');

	if (dst) dst->ds_type = 'U';

	*tm = 0;
	while(*p >= '0' && *p <= '9') {
		n = 10 * n + (*p++ - '0');
	}
	if (q == p) return NULL;	/* "The hour shall be required" */
	if (n < 0 || n >= 24)	return NULL;
	*tm = n * 60 * 60;
	if (*p == ':') {
		p++;
		n = 0;
		while(*p >= '0' && *p <= '9') {
			n = 10 * n + (*p++ - '0');
		}
		if (q == p) return NULL;	/* format error */
		if (n < 0 || n >= 60)	return NULL;
		*tm += n * 60;
		if (*p == ':') {
			p++;
			n = 0;
			while(*p >= '0' && *p <= '9') {
				n = 10 * n + (*p++ - '0');
			}
			if (q == p) return NULL;	/* format error */
			if (n < 0 || n >= 60)	return NULL;
			*tm += n;
		}
	}
	if (dst) {
		dst->ds_type = ds_type;
		dst->ds_sec = *tm;
	}
	return p;
}

static const char *
parseDate(register char *buf, register const char *p, struct dsttype *dstinfo)
{
	register const char *q;
	register int n = 0;
	int cnt = 0;
	const int bnds[3][2] =	{	{ 1, 12 },
					{ 1, 5 },
					{ 0, 6}
				 };
	char ds_type;

	if (*p != 'M') {
		if (*p == 'J') {
			*buf++ = *p++;
			ds_type = 'J';
		}
		else	ds_type = 'Z';
		q = p;
		while(*p >= '0' && *p <= '9') {
			n = 10 * n + (*p - '0');
			*buf++ = *p++;
		}
		if (q == p) return NULL;	/* format error */
		if (n < (ds_type == 'J') || n > 365) return NULL;
		dstinfo->ds_type = ds_type;
		dstinfo->ds_date[0] = n;
		return p;
	}
	ds_type = 'M';
	do {
		*buf++ = *p++;
		q = p;
		n = 0;
		while(*p >= '0' && *p <= '9') {
			n = 10 * n + (*p - '0');
			*buf++ = *p++;
		}
		if (q == p) return NULL;	/* format error */
		if (n < bnds[cnt][0] || n > bnds[cnt][1]) return NULL;
		dstinfo->ds_date[cnt] = n;
		cnt++;
	} while (cnt < 3 && *p == '.');
	if (cnt != 3) return NULL;
	*buf = '\0';
	dstinfo->ds_type = ds_type;
	return p;
}

static const char *
parseRule(register char *buf, register const char *p)
{
	long time;
	register const char *q;

	if (!(p = parseDate(buf, p, &dststart))) return NULL;
	buf += strlen(buf);
	if (*p == '/') {
		q = ++p;
		if (!(p = parseTime(&time, p, &dststart))) return NULL;
		while( p != q) *buf++ = *q++;
	}
	if (*p != ',') return NULL;
	p++;
	if (!(p = parseDate(buf, p, &dstend))) return NULL;
	buf += strlen(buf);
	if (*p == '/') {
		q = ++p;
		if (!(p = parseTime(&time, p, &dstend))) return NULL;
		while(*buf++ = *q++);
	}
	if (*p) return NULL;
	return p;
}

/* The following routine parses timezone information in POSIX-format. For
 * the requirements, see IEEE Std 1003.1-1988 section 8.1.1.
 * The function returns as soon as it spots an error.
 */
static void
parseTZ(const char *p)
{
	long tz, dst = 60 * 60, sign = 1;
	static char lastTZ[2 * RULE_LEN];
	static char buffer[RULE_LEN];

	if (!p) return;

	if (*p == ':') {
		/*
		 * According to POSIX, this is implementation defined.
		 * Since it depends on the particular operating system, we
		 * can do nothing.
		 */
		return;
	}

	if (!strcmp(lastTZ, p)) return;		/* nothing changed */

	*_tzname[0] = '\0';
	*_tzname[1] = '\0';
	dststart.ds_type = 'U';
	dststart.ds_sec = 2 * 60 * 60;
	dstend.ds_type = 'U';
	dstend.ds_sec = 2 * 60 * 60;

	if (strlen(p) > 2 * RULE_LEN) return;
	strcpy(lastTZ, p);

	if (!(p = parseZoneName(buffer, p))) return;

	if (*p == '-') {
		sign = -1;
		p++;
	} else if (*p == '+') p++;

	if (!(p = parseTime(&tz, p, NULL))) return;
	tz *= sign;
	_timezone = tz;
	strncpy(_tzname[0], buffer, TZ_LEN);

	if (!(_daylight = (*p != '\0'))) return;

	buffer[0] = '\0';
	if (!(p = parseZoneName(buffer, p))) return;
	strncpy(_tzname[1], buffer, TZ_LEN);

	buffer[0] = '\0';
	if (*p && (*p != ','))
		if (!(p = parseTime(&dst, p, NULL))) return;
	_dst_off = dst;			/* dst was initialized to 1 hour */
	if (*p) {
		if (*p != ',') return;
		p++;
		if (strlen(p) > RULE_LEN) return;
		if (!(p = parseRule(buffer, p))) return;
	}
}

void
_tzset(void)
{
#if	defined(__BSD4_2)

	struct timeval tv;
	struct timezone tz;

	_gettimeofday(&tv, &tz);
	_daylight = tz.tz_dsttime;
	_timezone = tz.tz_minuteswest * 60L;

#elif	!defined(_POSIX_SOURCE) && !defined(__USG)

#if	!defined(_MINIX)		/* MINIX has no ftime() */
	struct timeb time;

	_ftime(&time);
	_timezone = time.timezone * 60L;
	_daylight = time.dstflag;
#endif

#endif	/* !_POSIX_SOURCE && !__USG */

	parseTZ(getenv("TZ"));		/* should go inside #if */

#if	defined(__USG) || defined(_POSIX_SOURCE)
	tzname[0] = _tzname[0];
	tzname[1] = _tzname[1];
#if	defined(__USG)
	timezone = _timezone;
	daylight = _daylight;
#endif
#endif	/* __USG || _POSIX_SOURCE */
}

static int
last_sunday(register int day, register struct tm *timep)
{
	int first = FIRSTSUNDAY(timep);

	if (day >= 58 && LEAPYEAR(YEAR0 + timep->tm_year)) day++;
	if (day < first) return first;
	return day - (day - first) % 7;
}

static int
date_of(register struct dsttype *dst, struct tm *timep)
{
	int leap = LEAPYEAR(YEAR0 + timep->tm_year);
	int firstday, tmpday;
	register int day, month;

	if (dst->ds_type != 'M') {
		return dst->ds_date[0] -
			    (dst->ds_type == 'J'
				&& leap
				&& dst->ds_date[0] < 58);
	}
	day = 0;
	month = 1;
	while (month < dst->ds_date[0]) {
		day += _ytab[leap][month - 1];
		month++;
	}
	firstday = (day + FIRSTDAYOF(timep)) % 7;
	tmpday = day;
	day += (dst->ds_date[2] - firstday + 7) % 7
		+ 7 * (dst->ds_date[1] - 1);
	if (day >= tmpday + _ytab[leap][month-1]) day -= 7;
	return day;
}

/*
 * The default dst transitions are those for Western Europe (except Great
 * Britain). 
 */
unsigned
_dstget(register struct tm *timep)
{
	int begindst, enddst;
	register struct dsttype *dsts = &dststart, *dste = &dstend;
	int do_dst = 0;

	if (_daylight == -1)
		_tzset();

	timep->tm_isdst = _daylight;
	if (!_daylight) return 0;

	if (dsts->ds_type != 'U')
		begindst = date_of(dsts, timep);
	else begindst = last_sunday(89, timep);	/* last Sun before Apr */
	if (dste->ds_type != 'U')
		enddst = date_of(dste, timep);
	else enddst = last_sunday(272, timep);	/* last Sun in Sep */

	/* assume begindst != enddst (otherwise it would be no use) */
	if (begindst < enddst) {		/* northern hemisphere */
		if (timep->tm_yday > begindst && timep->tm_yday < enddst)
			do_dst = 1;
	} else {				/* southern hemisphere */
		if (timep->tm_yday > begindst || timep->tm_yday < enddst)
			do_dst = 1;
	}

	if (!do_dst
	    && (timep->tm_yday == begindst || timep->tm_yday == enddst)) {
		long dsttranssec;	/* transition when day is this old */
		long cursec;

		if (timep->tm_yday == begindst)
			dsttranssec = dsts->ds_sec;
		else	dsttranssec = dste->ds_sec;
		cursec = ((timep->tm_hour * 60) + timep->tm_min) * 60L
			    + timep->tm_sec;

		if ((timep->tm_yday == begindst && cursec >= dsttranssec)
		    || (timep->tm_yday == enddst && cursec < dsttranssec))
			do_dst = 1;
	}
	if (do_dst) return _dst_off;
	timep->tm_isdst = 0;
	return 0;
}

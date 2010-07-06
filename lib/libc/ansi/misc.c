/*
 * misc - data and miscellaneous routines
 */
/* $Header$ */

#include	<ctype.h>
#include	<time.h>
#include	<stdlib.h>
#include	<string.h>

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

long	timezone = 0;
int	daylight = 0;


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
		while((*buf++ = *q++));
	}
	if (*p) return NULL;
	return p;
}

static int
last_sunday(register int day, const register struct tm *timep)
{
	int first = FIRSTSUNDAY(timep);

	if (day >= 58 && LEAPYEAR(YEAR0 + timep->tm_year)) day++;
	if (day < first) return first;
	return day - (day - first) % 7;
}

static int
date_of(const register struct dsttype *dst, const struct tm *timep)
{
	int leap = LEAPYEAR(YEAR0 + timep->tm_year);
	int firstday, tmpday;
	register int day, month;

	if (dst->ds_type != 'M') {
		return dst->ds_date[0] -
			    ((dst->ds_type == 'J')
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


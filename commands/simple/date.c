/* date - Display (or set) the date and time		Author: V. Archer */

#include <sys/types.h>
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#define	MIN	60L		/* # seconds in a minute */
#define	HOUR	(60 * MIN)	/* # seconds in an hour */
#define	DAY	(24 * HOUR)	/* # seconds in a day */
#define	YEAR	(365 * DAY)	/* # seconds in a (non-leap) year */

int qflag, uflag, sflag, Sflag;

/* Default output file descriptor.
 */
int outfd = 1;

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void putchar, (int c));
_PROTOTYPE(void pstring, (char *s, int len));
_PROTOTYPE(void pldecimal, (unsigned long d, int digits));
_PROTOTYPE(void pdecimal, (int d, int digits));
_PROTOTYPE(void fmtdate, (char *format, time_t t, struct tm *p));
_PROTOTYPE(time_t make_time, (char *t));
_PROTOTYPE(struct tm *september, (time_t *tp));
_PROTOTYPE(void usage, (void));

/* Main module. Handles P1003.2 date and system administrator's date. The
 * date entered should be given GMT, regardless of the system's TZ!
 */
int main(argc, argv)
int argc;
char **argv;
{
  time_t t;
  struct tm *tm;
  char *format;
  char time_buf[40];
  int n;
  int i;

  time(&t);

  i = 1;
  while (i < argc && argv[i][0] == '-') {
	char *opt = argv[i++] + 1, *end;

	if (opt[0] == '-' && opt[1] == 0) break;

	while (*opt != 0) switch (*opt++) {
	case 'q':
		qflag = 1;
		break;
	case 's':
		sflag = 1;
		break;
	case 'u':
		uflag = 1;
		break;
	case 'S':
		Sflag = 1;
		break;
	case 't':
		/* (obsolete, now -r) */
	case 'r':
		if (*opt == 0) {
			if (i == argc) usage();
			opt = argv[i++];
		}
		t = strtoul(opt, &end, 10);
		if (*end != 0) usage();
		opt = "";
		break;
	default:
		usage();
	}
  }

  if (!qflag && i < argc && ('0' <= argv[i][0] && argv[i][0] <= '9')) {
	t = make_time(argv[i++]);
	sflag = 1;
  }

  format = "%c";
  if (i < argc && argv[i][0] == '+') format = argv[i++] + 1;

  if (i != argc) usage();

  if (qflag) {
	pstring("\nPlease enter date: MMDDYYhhmmss. Then hit the RETURN key.\n", -1);
	n = read(0, time_buf, sizeof(time_buf));
	if (n > 0 && time_buf[n-1] == '\n') n--;
	if (n >= 0) time_buf[n] = 0;
	t = make_time(time_buf);
	sflag = 1;
  }

  if (sflag && stime(&t) != 0) {
	outfd = 2;
	pstring("No permission to set time\n", -1);
	return(1);
  }

  tm = Sflag ? september(&t) : uflag ? gmtime(&t) : localtime(&t);

  fmtdate(format, t, tm);
  putchar('\n');
  return(0);
}

/* Replacement for stdio putchar().
 */
void putchar(c)
int c;
{
  static char buf[1024];
  static char *bp = buf;

  if (c != 0) *bp++ = c;
  if (c == 0 || c == '\n' || bp == buf + sizeof(buf)) {
	write(outfd, buf, bp - buf);
	bp = buf;
  }
}

/* Internal function that prints a n-digits number. Replaces stdio in our
 * specific case.
 */
void pldecimal(d, digits)
unsigned long d;
int digits;
{
  digits--;
  if (d > 9 || digits > 0) pldecimal(d / 10, digits);
  putchar('0' + (d % 10));
}

void pdecimal(d, digits)
int d, digits;
{
  pldecimal((unsigned long) d, digits);
}

/* Internal function that prints a fixed-size string. Replaces stdio in our
 * specific case.
 */
void pstring(s, len)
char *s;
int len;
{
  while (*s)
	if (len--)
		putchar(*s++);
	else
		break;
}

/* Format the date, using the given locale string. A special case is the
 * TZ which might be a sign followed by four digits (New format time zone).
 */
void fmtdate(format, t, p)
char *format;
time_t t;
struct tm *p;
{
  int i;
  char *s;
  static char *wday[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
		       "Thursday", "Friday", "Saturday"};
  static char *month[] = {"January", "February", "March", "April",
			"May", "June", "July", "August",
		    "September", "October", "November", "December"};

  while (*format)
	if (*format == '%') {
		switch (*++format) {
		    case 'A':
			pstring(wday[p->tm_wday], -1);
			break;
		    case 'B':
			pstring(month[p->tm_mon], -1);
			break;
		    case 'D':
			pdecimal(p->tm_mon + 1, 2);
			putchar('/');
			pdecimal(p->tm_mday, 2);
			putchar('/');
		    case 'y':
			pdecimal(p->tm_year % 100, 2);
			break;
		    case 'H':
			pdecimal(p->tm_hour, 2);
			break;
		    case 'I':
			i = p->tm_hour % 12;
			pdecimal(i ? i : 12, 2);
			break;
		    case 'M':
			pdecimal(p->tm_min, 2);
			break;
		    case 'X':
		    case 'T':
			pdecimal(p->tm_hour, 2);
			putchar(':');
			pdecimal(p->tm_min, 2);
			putchar(':');
		    case 'S':
			pdecimal(p->tm_sec, 2);
			break;
		    case 'U':
			pdecimal((p->tm_yday - p->tm_wday + 13) / 7, 2);
			break;
		    case 'W':
			if (--(p->tm_wday) < 0) p->tm_wday = 6;
			pdecimal((p->tm_yday - p->tm_wday + 13) / 7, 2);
			if (++(p->tm_wday) > 6) p->tm_wday = 0;
			break;
		    case 'Y':
			pdecimal(p->tm_year + 1900, 4);
			break;
		    case 'Z':
			if (uflag) {
				s = "GMT";
			} else {
				s = (p->tm_isdst == 1) ? tzname[1] : tzname[0];
			}
			pstring(s, strlen(s));
			break;
		    case 'a':
			pstring(wday[p->tm_wday], 3);
			break;
		    case 'b':
		    case 'h':
			pstring(month[p->tm_mon], 3);
			break;
		    case 'c':
			if (!(s = getenv("LC_TIME")))
				s = "%a %b %e %T %Z %Y";
			fmtdate(s, t, p);
			break;
		    case 'd':
			pdecimal(p->tm_mday, 2);
			break;
		    case 'e':
			if (p->tm_mday < 10) putchar(' ');
			pdecimal(p->tm_mday, 1);
			break;
		    case 'j':
			pdecimal(p->tm_yday + 1, 3);
			break;
		    case 'm':
			pdecimal(p->tm_mon + 1, 2);
			break;
		    case 'n':	putchar('\n');	break;
		    case 'p':
			if (p->tm_hour < 12)
				putchar('A');
			else
				putchar('P');
			putchar('M');
			break;
		    case 'r':
			fmtdate("%I:%M:%S %p", t, p);
			break;
		    case 's':
			pldecimal((unsigned long) t, 0);
			break;
		    case 't':	putchar('\t');	break;
		    case 'w':
			putchar('0' + p->tm_wday);
			break;
		    case 'x':
			fmtdate("%B %e %Y", t, p);
			break;
		    case '%':	putchar('%');	break;
		    case '\0':	format--;
		}
		format++;
	} else
		putchar(*format++);
}

/* Convert a local date string into GMT time in seconds. */
time_t make_time(t)
char *t;
{
  struct tm tm;				/* user specified time */
  time_t now;				/* current time */
  int leap;				/* current year is leap year */
  int i;				/* general index */
  int fld;				/* number of fields */
  int f[6];				/* time fields */
  static int days_per_month[2][12] = {
  { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
  { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }};

/* Get current time just in case */
  now = time((time_t *) 0);
  tm  = *localtime(&now);
  tm.tm_sec   = 0;
  tm.tm_mon++;
  tm.tm_year %= 100;

/* Parse the time */
#if '0'+1 != '1' || '1'+1 != '2' || '2'+1 != '3' || '3'+1 != '4' || \
    '4'+1 != '5' || '5'+1 != '6' || '6'+1 != '7' || '7'+1 != '8' || '8'+1 != '9'
  << Code unsuitable for character collating sequence >>
#endif

  for (fld = 0; fld < sizeof(f)/sizeof(f[0]); fld++) {
	if (*t == 0) break;
	f[fld] = 0;
	for (i = 0; i < 2; i++, t++) {
		if (*t < '0' || *t > '9') usage();
		f[fld] = f[fld] * 10 + *t - '0';
	}
  }

  switch (fld) {
  case 2:
	tm.tm_hour = f[0]; tm.tm_min  = f[1]; break;

  case 3:
	tm.tm_hour = f[0]; tm.tm_min  = f[1]; tm.tm_sec  = f[2];
	break;

  case 5:
  	tm.tm_mon  = f[0]; tm.tm_mday = f[1]; tm.tm_year = f[2];
	tm.tm_hour = f[3]; tm.tm_min  = f[4];
	break;

  case 6:
	tm.tm_mon  = f[0]; tm.tm_mday = f[1]; tm.tm_year = f[2];
	tm.tm_hour = f[3]; tm.tm_min  = f[4]; tm.tm_sec  = f[5];
	break;

  default:
	usage();
  }

/* Convert the time into seconds since 1 January 1970 */
  if (tm.tm_year < 70)
    tm.tm_year += 100;
  leap = (tm.tm_year % 4 == 0 && tm.tm_year % 400 != 0);
  if (tm.tm_mon  < 1  || tm.tm_mon  > 12 ||
      tm.tm_mday < 1  || tm.tm_mday > days_per_month[leap][tm.tm_mon-1] ||
      tm.tm_hour > 23 || tm.tm_min  > 59) {
    outfd = 2;
    pstring("Illegal date format\n", -1);
    exit(1);
  }

/* Convert the time into Minix time - zone independent code */
  {
    time_t utctime;			/* guess at unix time */
    time_t nextbit;			/* next bit to try */
    int rv;				/* result of try */
    struct tm *tmp;			/* local time conversion */

#define COMPARE(a,b)	((a) != (b)) ? ((a) - (b)) :

    utctime = 1;
    do {
      nextbit = utctime;
      utctime = nextbit << 1;
    } while (utctime >= 1);

    for (utctime = 0; ; nextbit >>= 1) {

      utctime |= nextbit;
      tmp = localtime(&utctime);
      if (tmp == 0) continue;

      rv = COMPARE(tmp->tm_year,    tm.tm_year)
           COMPARE(tmp->tm_mon + 1, tm.tm_mon)
	   COMPARE(tmp->tm_mday,    tm.tm_mday)
	   COMPARE(tmp->tm_hour,    tm.tm_hour)
	   COMPARE(tmp->tm_min,     tm.tm_min)
	   COMPARE(tmp->tm_sec,     tm.tm_sec)
	   0;

      if (rv > 0)
        utctime &= ~nextbit;
      else if (rv == 0)
        break;

      if (nextbit == 0) {
	uflag = 1;
        outfd = 2;
        pstring("Inexact conversion to UTC from ", -1);
        fmtdate("%c\n", utctime, localtime(&utctime) );
	exit(1);
      }
    }
    return utctime;
  }
}

/* Correct the time to the reckoning of Eternal September. */
struct tm *september(tp)
time_t *tp;
{
  time_t t;
  int days;
  struct tm *tm;

  tm = localtime(tp);

  t = *tp - (tm->tm_hour - 12) * 3600L;  /* No zone troubles around noon. */
  days = 0;

  while (tm->tm_year > 93 || (tm->tm_year == 93 && tm->tm_mon >= 8)) {
	/* Step back a year or a month. */
	days += tm->tm_year > 93 ? tm->tm_yday+1 : tm->tm_mday;
	t = *tp - days * (24 * 3600L);

	tm = localtime(&t);
  }

  if (days > 0) {
	tm = localtime(tp);
	tm->tm_mday = days;
	tm->tm_year = 93;
	tm->tm_mon = 8;
#if SANITY
	t = mktime(tm);
	tm = localtime(&t);
#endif
  }
  return tm;
}

/* (Extended) Posix prototype of date. */
void usage()
{
  outfd = 2;
  pstring("Usage: date [-qsuS] [-r seconds] [[MMDDYY]hhmm[ss]] [+format]\n", -1);
  exit(1);
}

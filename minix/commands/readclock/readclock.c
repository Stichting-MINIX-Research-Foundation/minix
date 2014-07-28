/* frontend to the readclock.drv driver for getting/setting hw clock. */

#include <sys/cdefs.h>
#include <lib.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <minix/type.h>
#include <minix/const.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/com.h>
#include <minix/rs.h>
#include <sys/ipc.h>

void errmsg(char *s);
static void readclock(int type, struct tm *t, int flags);
void usage(void);

int quiet = 0;

int
main(int argc, char **argv)
{
	int flags = RTCDEV_NOFLAGS;
	int nflag = 0;	/* Tell what, but don't do it. */
	int wflag = 0;	/* Set the CMOS clock. */
	struct tm time1;
	struct tm time2;
	struct tm tmnow;
	char date[64];
	time_t now, rtc;
	int i, s;

	/* Process options. */
	while (argc > 1) {
		char *p = *++argv;

		if (*p++ != '-')
			usage();

		while (*p != 0) {
			switch (*p++) {
			case 'n':
				nflag = 1;
				break;
			case 'w':
				wflag = 1;
				break;
			case 'W':
				flags |= RTCDEV_CMOSREG;
				wflag = 1; /* -W implies -w */
				break;
			case '2':
				flags |= RTCDEV_Y2KBUG;
				break;
			case 'q':
				quiet = 1;
				break;
			default:
				usage();
			}
		}
		argc--;
	}

	/* Read the CMOS real time clock. */
	for (i = 0; i < 10; i++) {
		readclock(RTCDEV_GET_TIME, &time1, flags);
		now = time(NULL);

		time1.tm_isdst = -1;	/* Do timezone calculations. */
		time2 = time1;

		rtc = mktime(&time1);	/* Transform to a time_t. */
		if (rtc != -1)
			break;

		if (!quiet) printf
		    ("readclock: Invalid time read from CMOS RTC: %d-%02d-%02d %02d:%02d:%02d\n",
		    time2.tm_year + 1900, time2.tm_mon + 1, time2.tm_mday,
		    time2.tm_hour, time2.tm_min, time2.tm_sec);
		sleep(5);
	}
	if (i == 10)
		exit(1);

	if (!wflag) {
		/* Set system time. */
		if (nflag) {
			if (!quiet)
				printf("stime(%lu)\n", (unsigned long) rtc);
		} else {
			if (stime(&rtc) < 0) {
				if (!quiet)
					errmsg("Not allowed to set time.");
				exit(1);
			}
		}
		tmnow = *localtime(&rtc);
		if (strftime(date, sizeof(date),
			"%a %b %d %H:%M:%S %Z %Y", &tmnow) != 0) {
			if (date[8] == '0')
				date[8] = ' ';
			if (!quiet) printf("%s\n", date);
		}
	} else {
		/* Set the CMOS clock to the system time. */
		tmnow = *localtime(&now);
		if (nflag) {
			if (!quiet)
				printf("%04d-%02d-%02d %02d:%02d:%02d\n",
				    tmnow.tm_year + 1900,
				    tmnow.tm_mon + 1,
				    tmnow.tm_mday,
				    tmnow.tm_hour, tmnow.tm_min, tmnow.tm_sec);
		} else {
			readclock(RTCDEV_SET_TIME, &tmnow, flags);
		}
	}
	exit(0);
}

void
errmsg(char *s)
{
	static char *prompt = "readclock: ";

	if (!quiet) printf("%s%s\n", prompt, s);
	prompt = "";
}

static void
readclock(int type, struct tm *t, int flags)
{
	int r;
	message m;
	endpoint_t ep;

	r = minix_rs_lookup("readclock.drv", &ep);
	if (r != 0) {
		if (!quiet) errmsg("Couldn't locate readclock.drv\n");
		exit(1);
	}

	memset(&m, 0, sizeof(m));
	m.m_lc_readclock_rtcdev.tm = (vir_bytes)t;
	m.m_lc_readclock_rtcdev.flags = flags;

	r = _syscall(ep, type, &m);
	if (r != RTCDEV_REPLY || m.m_readclock_lc_rtcdev.status != 0) {
		if (!quiet) errmsg("Call to readclock.drv failed\n");
		exit(1);
	}
}

void
usage(void)
{
	if (!quiet) printf("Usage: readclock [-nqwW2]\n");
	exit(1);
}

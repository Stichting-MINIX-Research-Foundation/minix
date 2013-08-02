/* frontend to the readclock.drv driver for getting/setting hw clock. */

#include <sys/cdefs.h>
#include <lib.h>
#include <sys/types.h>
#include <stdlib.h>
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

int nflag = 0;			/* Tell what, but don't do it. */
int wflag = 0;			/* Set the CMOS clock. */
int Wflag = 0;			/* Also set the CMOS clock register bits. */
int y2kflag = 0;		/* Interpret 1980 as 2000 for clock with Y2K bug. */

void errmsg(char *s);
void get_time(struct tm *t);
void set_time(struct tm *t);
void usage(void);

int
main(int argc, char **argv)
{
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
				Wflag = 1;
				break;
			case '2':
				y2kflag = 1;
				break;
			default:
				usage();
			}
		}
		argc--;
	}
	if (Wflag)
		wflag = 1;	/* -W implies -w */

	/* Read the CMOS real time clock. */
	for (i = 0; i < 10; i++) {
		get_time(&time1);
		now = time(NULL);

		time1.tm_isdst = -1;	/* Do timezone calculations. */
		time2 = time1;

		rtc = mktime(&time1);	/* Transform to a time_t. */
		if (rtc != -1)
			break;

		printf
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
			printf("stime(%lu)\n", (unsigned long) rtc);
		} else {
			if (stime(&rtc) < 0) {
				errmsg("Not allowed to set time.");
				exit(1);
			}
		}
		tmnow = *localtime(&rtc);
		if (strftime(date, sizeof(date),
			"%a %b %d %H:%M:%S %Z %Y", &tmnow) != 0) {
			if (date[8] == '0')
				date[8] = ' ';
			printf("%s\n", date);
		}
	} else {
		/* Set the CMOS clock to the system time. */
		tmnow = *localtime(&now);
		if (nflag) {
			printf("%04d-%02d-%02d %02d:%02d:%02d\n",
			    tmnow.tm_year + 1900,
			    tmnow.tm_mon + 1,
			    tmnow.tm_mday,
			    tmnow.tm_hour, tmnow.tm_min, tmnow.tm_sec);
		} else {
			set_time(&tmnow);
		}
	}
	exit(0);
}

void
errmsg(char *s)
{
	static char *prompt = "readclock: ";

	printf("%s%s\n", prompt, s);
	prompt = "";
}

void
get_time(struct tm *t)
{
	int r;
	message m;
	endpoint_t ep;

	r = minix_rs_lookup("readclock.drv", &ep);
	if (r != 0) {
		errmsg("Couldn't locate readclock.drv\n");
		exit(1);
	}

	m.RTCDEV_TM = t;
	m.RTCDEV_FLAGS = (y2kflag) ? RTCDEV_Y2KBUG : RTCDEV_NOFLAGS;

	r = _syscall(ep, RTCDEV_GET_TIME, &m);
	if (r != RTCDEV_REPLY || m.RTCDEV_STATUS != 0) {
		errmsg("Call to readclock.drv failed\n");
		exit(1);
	}
}

void
set_time(struct tm *t)
{
	int r;
	message m;
	endpoint_t ep;

	r = minix_rs_lookup("readclock.drv", &ep);
	if (r != 0) {
		errmsg("Couldn't locate readclock.drv\n");
		exit(1);
	}

	m.RTCDEV_TM = t;
	m.RTCDEV_FLAGS = RTCDEV_NOFLAGS;

	if (y2kflag) {
		m.RTCDEV_FLAGS |= RTCDEV_Y2KBUG;
	}

	if (Wflag) {
		m.RTCDEV_FLAGS |= RTCDEV_CMOSREG;
	}

	r = _syscall(ep, RTCDEV_SET_TIME, &m);
	if (r != RTCDEV_REPLY || m.RTCDEV_STATUS != 0) {
		errmsg("Call to readclock.drv failed\n");
		exit(1);
	}
}

void
usage(void)
{
	printf("Usage: readclock [-nwW2]\n");
	exit(1);
}

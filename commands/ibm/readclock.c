/* setime - set the system time from the real time clock
					Authors: T. Holm & E. Froese
					Adapted by: Jorrit .N. Herder */

/************************************************************************/
/*   Readclock was updated for security reasons: openeing /dev/mem no	*/
/*   longer automatically grants I/O privileges to the calling process	*/
/*   so that the CMOS' clock could not be read from this program. The	*/
/*   new approach is to rely on the FS to do the CMOS I/O, via the new  */
/*   system call CMOSTIME (which only reads the current clock value and */
/*   cannot update the CMOS clock). 					*/
/*   The original readclock.c is still available under backup.c.	*/
/************************************************************************/
/*									*/
/*   readclock.c							*/
/*									*/
/*		Read the clock value from the 64 byte CMOS RAM		*/
/*		area, then set system time.				*/
/*									*/
/*		If the machine ID byte is 0xFC or 0xF8, the device	*/
/*		/dev/mem exists and can be opened for reading,		*/
/*		and no errors in the CMOS RAM are reported by the	*/
/*		RTC, then the time is read from the clock RAM		*/
/*		area maintained by the RTC.				*/
/*									*/
/*		The clock RAM values are decoded and fed to mktime	*/
/*		to make a time_t value, then stime(2) is called.	*/
/*									*/
/*		This fails if:						*/
/*									*/
/*		If the machine ID does not match 0xFC or 0xF8 (no	*/
/*		error message.)						*/
/*									*/
/*		If the machine ID is 0xFC or 0xF8 and /dev/mem		*/
/*		is missing, or cannot be accessed.			*/
/*									*/
/*		If the RTC reports errors in the CMOS RAM.		*/
/*									*/
/************************************************************************/
/*    origination          1987-Dec-29              efth                */
/*    robustness	   1990-Oct-06		    C. Sylvain		*/
/* incorp. B. Evans ideas  1991-Jul-06		    C. Sylvain		*/
/*    set time & calibrate 1992-Dec-17		    Kees J. Bot		*/
/*    clock timezone	   1993-Oct-10		    Kees J. Bot		*/
/*    set CMOS clock	   1994-Jun-12		    Kees J. Bot		*/
/*    removed set CMOS	   2004-Sep-06		    Jorrit N. Herder    */
/************************************************************************/

#include <minix/callnr.h>
#include <minix/config.h>
#include <minix/type.h>
#include <minix/const.h>
#include <minix/com.h>
#include <minix/syslib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioc_cmos.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <ibm/portio.h>
#include <ibm/cmos.h>
#include <sys/svrctl.h>

#define MAX_RETRIES	1

int nflag = 0;		/* Tell what, but don't do it. */
int y2kflag = 0;	/* Interpret 1980 as 2000 for clock with Y2K bug. */

char clocktz[128];	/* Timezone of the clock. */

#define MACH_ID_ADDR	0xFFFFE		/* BIOS Machine ID at FFFF:000E */

#define PC_AT		   0xFC		/* Machine ID byte for PC/AT,
					   PC/XT286, and PS/2 Models 50, 60 */
#define PS_386		   0xF8		/* Machine ID byte for PS/2 Model 80 */

/* Manufacturers usually use the ID value of the IBM model they emulate.
 * However some manufacturers, notably HP and COMPAQ, have had different
 * ideas in the past.
 *
 * Machine ID byte information source:
 *	_The Programmer's PC Sourcebook_ by Thom Hogan,
 *	published by Microsoft Press
 */

void errmsg(char *s);
int bcd_to_dec(int n);
int dec_to_bcd(int n);
void usage(void);

#define CMOS_DEV "/dev/cmos"

PUBLIC int main(int argc, char **argv)
{
  int fd;
  struct tm time1;
  struct tm time2;
  struct tm tmnow;
  char date[64];
  time_t now, rtc;
  int i, s, mem;
  unsigned char mach_id, cmos_state;
  struct sysgetenv sysgetenv;
  message m;
  int request;


  /* Process options. */
  while (argc > 1) {
	char *p = *++argv;

	if (*p++ != '-') usage();

	while (*p != 0) {
		switch (*p++) {
		case 'n':	nflag = 1;	break;
		case '2':	y2kflag = 1;	break;
		default:	usage();
		}
	}
	argc--;
  }

#if DEAD_CODE
  /* The hardware clock may run in a different time zone, likely GMT or
   * winter time.  Select that time zone.
   */
  strcpy(clocktz, "TZ=");
  sysgetenv.key = "TZ";
  sysgetenv.keylen = 2+1;
  sysgetenv.val = clocktz+3;
  sysgetenv.vallen = sizeof(clocktz)-3;
  if (svrctl(SYSGETENV, &sysgetenv) == 0) {
	putenv(clocktz);
	tzset();
  }
#endif

  /* Read the CMOS real time clock. */
  for (i = 0; i < MAX_RETRIES; i++) {

	/* sleep, unless first iteration */
	if (i > 0) sleep(5);

	/* Open the CMOS device to read the system time. */
	if ((fd = open(CMOS_DEV, O_RDONLY)) < 0) {
		perror(CMOS_DEV);
		fprintf(stderr, "Couldn't open CMOS device.\n");
		exit(1);
	}
        request = (y2kflag) ? CIOCGETTIME : CIOCGETTIMEY2K;
	if ((s=ioctl(fd, request, (void *) &time1)) < 0) {
		perror("ioctl");
		fprintf(stderr, "Couldn't do CMOS ioctl.\n");
		exit(1);
	}
	close(fd);

	now = time(NULL);

	time1.tm_isdst = -1;	/* Do timezone calculations. */
	time2 = time1;

	rtc= mktime(&time1);	/* Transform to a time_t. */
	if (rtc != -1) {
		break;
	}

	fprintf(stderr,
"readclock: Invalid time read from CMOS RTC: %d-%02d-%02d %02d:%02d:%02d\n",
		time2.tm_year+1900, time2.tm_mon+1, time2.tm_mday,
		time2.tm_hour, time2.tm_min, time2.tm_sec);
  }
  if (i >= MAX_RETRIES) exit(1);

  /* Now set system time. */
  if (nflag) {
		printf("stime(%lu)\n", (unsigned long) rtc);
  } else {
	if (stime(&rtc) < 0) {
		errmsg( "Not allowed to set time." );
		exit(1);
	}
  }
  tmnow = *localtime(&rtc);
  if (strftime(date, sizeof(date),
			"%a %b %d %H:%M:%S %Z %Y", &tmnow) != 0) {
	if (date[8] == '0') date[8]= ' ';
#if 0
	printf("%s [CMOS read via FS, see command/ibm/readclock.c]\n", date);
#endif
  }
  exit(0);
}

void errmsg(char *s)
{
  static char *prompt = "readclock: ";

  fprintf(stderr, "%s%s\n", prompt, s);
  prompt = "";
}


int bcd_to_dec(int n)
{
  return ((n >> 4) & 0x0F) * 10 + (n & 0x0F);
}

int dec_to_bcd(int n)
{
  return ((n / 10) << 4) | (n % 10);
}

void usage(void)
{
  fprintf(stderr, "Usage: settime [-n2]\n");
  exit(1);
}

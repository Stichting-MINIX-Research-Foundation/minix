/* readclock - read the real time clock		Authors: T. Holm & E. Froese */

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
/************************************************************************/


#include <sys/types.h>
#include <sys/stat.h>
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

int nflag = 0;		/* Tell what, but don't do it. */
int wflag = 0;		/* Set the CMOS clock. */
int Wflag = 0;		/* Also set the CMOS clock register bits. */
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
void get_time(struct tm *t);
int read_register(int reg_addr);
void set_time(struct tm *t);
void write_register(int reg_addr, int value);
int bcd_to_dec(int n);
int dec_to_bcd(int n);
void usage(void);

int main(int argc, char **argv)
{
  struct tm time1;
  struct tm time2;
  struct tm tmnow;
  char date[64];
  time_t now, rtc;
  int i, mem;
  unsigned char mach_id, cmos_state;
  struct sysgetenv sysgetenv;

  /* Open /dev/mem to get access to physical memory. */
  if ((mem = open("/dev/mem", O_RDONLY)) == -1) {
	errmsg( "Permission denied." );
	exit(1);
  }
  if (lseek(mem, (off_t) MACH_ID_ADDR, SEEK_SET) == -1
		|| read(mem, (void *) &mach_id, sizeof(mach_id)) < 0) {
	mach_id = -1;
  }
  if (mach_id != PS_386 && mach_id != PC_AT) {
	errmsg( "Machine ID unknown." );
	fprintf( stderr, "Machine ID byte = %02x\n", mach_id );

	exit(1);
  }
  cmos_state = read_register(CMOS_STATUS);
  if (cmos_state & (CS_LOST_POWER | CS_BAD_CHKSUM | CS_BAD_TIME)) {
	errmsg( "CMOS RAM error(s) found..." );
	fprintf( stderr, "CMOS state = 0x%02x\n", cmos_state );

	if (cmos_state & CS_LOST_POWER)
	    errmsg( "RTC lost power. Reset CMOS RAM with SETUP." );
	if (cmos_state & CS_BAD_CHKSUM)
	    errmsg( "CMOS RAM checksum is bad. Run SETUP." );
	if (cmos_state & CS_BAD_TIME)
	    errmsg( "Time invalid in CMOS RAM. Reset clock." );
	exit(1);
  }

  /* Process options. */
  while (argc > 1) {
	char *p = *++argv;

	if (*p++ != '-') usage();

	while (*p != 0) {
		switch (*p++) {
		case 'n':	nflag = 1;	break;
		case 'w':	wflag = 1;	break;
		case 'W':	Wflag = 1;	break;
		case '2':	y2kflag = 1;	break;
		default:	usage();
		}
	}
	argc--;
  }
  if (Wflag) wflag = 1;		/* -W implies -w */

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

  /* Read the CMOS real time clock. */
  for (i = 0; i < 10; i++) {
	get_time(&time1);
	now = time(NULL);

	time1.tm_isdst = -1;	/* Do timezone calculations. */
	time2 = time1;

	rtc= mktime(&time1);	/* Transform to a time_t. */
	if (rtc != -1) break;

	fprintf(stderr,
"readclock: Invalid time read from CMOS RTC: %d-%02d-%02d %02d:%02d:%02d\n",
		time2.tm_year+1900, time2.tm_mon+1, time2.tm_mday,
		time2.tm_hour, time2.tm_min, time2.tm_sec);
	sleep(5);
  }
  if (i == 10) exit(1);

  if (!wflag) {
	/* Set system time. */
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
		printf("Result: %s\n", date);
	}
  } else {
	/* Set the CMOS clock to the system time. */
	tmnow = *localtime(&now);
	if (nflag) {
		printf("%04d-%02d-%02d %02d:%02d:%02d\n",
			tmnow.tm_year + 1900,
			tmnow.tm_mon + 1,
			tmnow.tm_mday,
			tmnow.tm_hour,
			tmnow.tm_min,
			tmnow.tm_sec);
	} else {
		set_time(&tmnow);
	}
  }
  exit(0);
}

void errmsg(char *s)
{
  static char *prompt = "readclock: ";

  fprintf(stderr, "%s%s\n", prompt, s);
  prompt = "";
}


/***********************************************************************/
/*                                                                     */
/*    get_time( time )                                                 */
/*                                                                     */
/*    Update the structure pointed to by time with the current time    */
/*    as read from CMOS RAM of the RTC.				       */
/*    If necessary, the time is converted into a binary format before  */
/*    being stored in the structure.                                   */
/*                                                                     */
/***********************************************************************/

int dead;
void timeout(int sig) { dead= 1; }

void get_time(struct tm *t)
{
  int osec, n;
  unsigned long i;
  struct sigaction sa;

  /* Start a timer to keep us from getting stuck on a dead clock. */
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = timeout;
  sigaction(SIGALRM, &sa, NULL);
  dead = 0;
  alarm(5);

  do {
	osec = -1;
	n = 0;
	do {
		if (dead) {
			fprintf(stderr, "readclock: CMOS clock appears dead\n");
			exit(1);
		}

		/* Clock update in progress? */
		if (read_register(RTC_REG_A) & RTC_A_UIP) continue;

		t->tm_sec = read_register(RTC_SEC);
		if (t->tm_sec != osec) {
			/* Seconds changed.  First from -1, then because the
			 * clock ticked, which is what we're waiting for to
			 * get a precise reading.
			 */
			osec = t->tm_sec;
			n++;
		}
	} while (n < 2);

	/* Read the other registers. */
	t->tm_min = read_register(RTC_MIN);
	t->tm_hour = read_register(RTC_HOUR);
	t->tm_mday = read_register(RTC_MDAY);
	t->tm_mon = read_register(RTC_MONTH);
	t->tm_year = read_register(RTC_YEAR);

	/* Time stable? */
  } while (read_register(RTC_SEC) != t->tm_sec
	|| read_register(RTC_MIN) != t->tm_min
	|| read_register(RTC_HOUR) != t->tm_hour
	|| read_register(RTC_MDAY) != t->tm_mday
	|| read_register(RTC_MONTH) != t->tm_mon
	|| read_register(RTC_YEAR) != t->tm_year);

  if ((read_register(RTC_REG_B) & RTC_B_DM_BCD) == 0) {
	/* Convert BCD to binary (default RTC mode). */
	t->tm_year = bcd_to_dec(t->tm_year);
	t->tm_mon = bcd_to_dec(t->tm_mon);
	t->tm_mday = bcd_to_dec(t->tm_mday);
	t->tm_hour = bcd_to_dec(t->tm_hour);
	t->tm_min = bcd_to_dec(t->tm_min);
	t->tm_sec = bcd_to_dec(t->tm_sec);
  }
  t->tm_mon--;	/* Counts from 0. */

  /* Correct the year, good until 2080. */
  if (t->tm_year < 80) t->tm_year += 100;

  if (y2kflag) {
	/* Clock with Y2K bug, interpret 1980 as 2000, good until 2020. */
	if (t->tm_year < 100) t->tm_year += 20;
  }
}


int read_register(int reg_addr)
{
  int r;

  intr_disable();
  outb(RTC_INDEX, reg_addr);
  r= inb(RTC_IO);
  intr_enable();
  return r;
}



/***********************************************************************/
/*                                                                     */
/*    set_time( time )                                                 */
/*                                                                     */
/*    Set the CMOS RTC to the time found in the structure.             */
/*                                                                     */
/***********************************************************************/

void set_time(struct tm *t)
{
  int regA, regB;

  if (Wflag) {
	/* Set A and B registers to their proper values according to the AT
	 * reference manual.  (For if it gets messed up, but the BIOS doesn't
	 * repair it.)
	 */
	write_register(RTC_REG_A, RTC_A_DV_OK | RTC_A_RS_DEF);
	write_register(RTC_REG_B, RTC_B_24);
  }

  /* Inhibit updates. */
  regB= read_register(RTC_REG_B);
  write_register(RTC_REG_B, regB | RTC_B_SET);

  t->tm_mon++;	/* Counts from 1. */

  if (y2kflag) {
	/* Set the clock back 20 years to avoid Y2K bug, good until 2020. */
	if (t->tm_year >= 100) t->tm_year -= 20;
  }

  if ((regB & 0x04) == 0) {
	/* Convert binary to BCD (default RTC mode) */
	t->tm_year = dec_to_bcd(t->tm_year % 100);
	t->tm_mon = dec_to_bcd(t->tm_mon);
	t->tm_mday = dec_to_bcd(t->tm_mday);
	t->tm_hour = dec_to_bcd(t->tm_hour);
	t->tm_min = dec_to_bcd(t->tm_min);
	t->tm_sec = dec_to_bcd(t->tm_sec);
  }
  write_register(RTC_YEAR, t->tm_year);
  write_register(RTC_MONTH, t->tm_mon);
  write_register(RTC_MDAY, t->tm_mday);
  write_register(RTC_HOUR, t->tm_hour);
  write_register(RTC_MIN, t->tm_min);
  write_register(RTC_SEC, t->tm_sec);

  /* Stop the clock. */
  regA= read_register(RTC_REG_A);
  write_register(RTC_REG_A, regA | RTC_A_DV_STOP);

  /* Allow updates and restart the clock. */
  write_register(RTC_REG_B, regB);
  write_register(RTC_REG_A, regA);
}


void write_register(int reg_addr, int value)
{
  intr_disable();
  outb(RTC_INDEX, reg_addr);
  outb(RTC_IO, value);
  intr_enable();
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
  fprintf(stderr, "Usage: readclock [-nwW2]\n");
  exit(1);
}

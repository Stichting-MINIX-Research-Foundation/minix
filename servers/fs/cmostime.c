#include "fs.h"
#include <minix/com.h>
#include <minix/callnr.h>
#include <time.h>
#include <ibm/cmos.h>
#include <ibm/bios.h>


/* Manufacturers usually use the ID value of the IBM model they emulate.
 * However some manufacturers, notably HP and COMPAQ, have had different
 * ideas in the past.
 *
 * Machine ID byte information source:
 *	_The Programmer's PC Sourcebook_ by Thom Hogan,
 *	published by Microsoft Press
 */

FORWARD _PROTOTYPE( int read_register, (int register_address));
FORWARD _PROTOTYPE( int get_cmostime, (struct tm *tmp, int y2kflag));
FORWARD _PROTOTYPE( int dec_to_bcd, (int dec));
FORWARD _PROTOTYPE( int bcd_to_dec, (int bcd));



PUBLIC int do_cmostime(void)
{
  unsigned char mach_id, cmos_state;
  struct tm time1;
  int i, s;
  int y2kflag = m_in.REQUEST;
  vir_bytes dst_time = (vir_bytes) m_in.ADDRESS;

  /* First obtain the machine ID to see if we can read the CMOS clock. Only
   * for PS_386 and PC_AT this is possible. Otherwise, return an error.  
   */
  sys_vircopy(SELF, BIOS_SEG, (vir_bytes) ADR_MACHINE_ID, 
  	SELF, D, (vir_bytes) &mach_id, LEN_MACHINE_ID);
  if (mach_id != PS_386_MACHINE && mach_id != PC_AT_MACHINE) {
	printf("IS: Machine ID unknown. ID byte = %02x.\n", mach_id);
	return(EFAULT);
  }

  /* Now check the CMOS' state to see if we can read a proper time from it.
   * If the state is crappy, return an error.
   */
  cmos_state = read_register(CMOS_STATUS);
  if (cmos_state & (CS_LOST_POWER | CS_BAD_CHKSUM | CS_BAD_TIME)) {
	printf( "IS: CMOS RAM error(s) found. State = 0x%02x\n", cmos_state );
	if (cmos_state & CS_LOST_POWER)
	    printf("IS: RTC lost power. Reset CMOS RAM with SETUP." );
	if (cmos_state & CS_BAD_CHKSUM)
	    printf("IS: CMOS RAM checksum is bad. Run SETUP." );
	if (cmos_state & CS_BAD_TIME)
	    printf("IS: Time invalid in CMOS RAM. Reset clock." );
	return(EFAULT);
  }

  /* Everything seems to be OK. Read the CMOS real time clock and copy the
   * result back to the caller.
   */
  if (get_cmostime(&time1, y2kflag) != 0)
	return(EFAULT);
  sys_datacopy(SELF, (vir_bytes) &time1, 
  	who, dst_time, sizeof(struct tm));

  return(OK);
}


PRIVATE int get_cmostime(struct tm *t, int y2kflag)
{
/* Update the structure pointed to by time with the current time as read
 * from CMOS RAM of the RTC. If necessary, the time is converted into a
 * binary format before being stored in the structure.
 */
  int osec, n;
  unsigned long i;
  static int timeout_flag;

  /* Start a timer to keep us from getting stuck on a dead clock. */
  timeout_flag = 0;
  sys_flagalrm(5*HZ, &timeout_flag);
  do {
	osec = -1;
	n = 0;
	do {
		if (timeout_flag) {
			printf("readclock: CMOS clock appears dead\n");
			return(1);
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
  sys_flagalrm(0, &timeout_flag); /* not strictly necessarily; flag is static */

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
  return 0;
}

PRIVATE int read_register(int reg_addr)
{
/* Read a single CMOS register value. */
  int r = 0;
  sys_outb(RTC_INDEX, reg_addr);
  sys_inb(RTC_IO, &r);
  return r;
}

PRIVATE int bcd_to_dec(int n)
{
  return ((n >> 4) & 0x0F) * 10 + (n & 0x0F);
}

PRIVATE int dec_to_bcd(int n)
{
  return ((n / 10) << 4) | (n % 10);
}


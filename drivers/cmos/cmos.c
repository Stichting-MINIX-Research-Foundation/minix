/* This file contains a device driver that can access the CMOS chip to 
 * get or set the system time. It drives the special file:
 *
 *     /dev/cmos	- CMOS chip
 *
 * Changes:
 *     Aug 04, 2005	Created. Read CMOS time.  (Jorrit N. Herder)
 *
 * Manufacturers usually use the ID value of the IBM model they emulate.
 * However some manufacturers, notably HP and COMPAQ, have had different
 * ideas in the past.
 *
 * Machine ID byte information source:
 *	_The Programmer's PC Sourcebook_ by Thom Hogan,
 *	published by Microsoft Press
 */

#include "../drivers.h"
#include <sys/ioc_cmos.h>
#include <time.h>
#include <ibm/cmos.h>
#include <ibm/bios.h>

extern int errno;			/* error number for PM calls */

FORWARD _PROTOTYPE( int gettime, (int who, int y2kflag, vir_bytes dst_time));
FORWARD _PROTOTYPE( void reply, (int reply, int replyee, int proc, int s));

FORWARD _PROTOTYPE( int read_register, (int register_address));
FORWARD _PROTOTYPE( int get_cmostime, (struct tm *tmp, int y2kflag));
FORWARD _PROTOTYPE( int dec_to_bcd, (int dec));
FORWARD _PROTOTYPE( int bcd_to_dec, (int bcd));

/*===========================================================================*
 *				   main 				     *
 *===========================================================================*/
PUBLIC void main(void)
{
  message m;
  int y2kflag;
  int result;
  int suspended = NONE;
  int s;

  while(TRUE) {

      /* Get work. */
      if (OK != (s=receive(ANY, &m)))
          panic("CMOS", "attempt to receive work failed", s);

      /* Handle request. */
      switch(m.m_type) {

      case DEV_OPEN:
      case DEV_CLOSE:
      case CANCEL:
          reply(TASK_REPLY, m.m_source, m.PROC_NR, OK);
          break;

      case DEV_IOCTL:				

	  /* Probably best to SUSPEND the caller, CMOS I/O has nasty timeouts. 
	   * This way we don't block the rest of the system. First check if
           * another process is already suspended. We cannot handle multiple
           * requests at a time. 
           */
          if (suspended != NONE) {
              reply(TASK_REPLY, m.m_source, m.PROC_NR, EBUSY);
              break;
          }
          suspended = m.PROC_NR;
          reply(TASK_REPLY, m.m_source, m.PROC_NR, SUSPEND);

	  switch(m.REQUEST) {
	  case CIOCGETTIME:			/* get CMOS time */ 
          case CIOCGETTIMEY2K:
              y2kflag = (m.REQUEST = CIOCGETTIME) ? 0 : 1;
              result = gettime(m.PROC_NR, y2kflag, (vir_bytes) m.ADDRESS);
              break;
          case CIOCSETTIME:
          case CIOCSETTIMEY2K:
          default:				/* unsupported ioctl */
              result = ENOSYS;
          }

          /* Request completed. Tell the caller to check our status. */
	  notify(m.m_source);
          break;

      case DEV_STATUS:

          /* The FS calls back to get our status. Revive the suspended 
           * processes and return the status of reading the CMOS.
           */
	  if (suspended == NONE)
              reply(DEV_NO_STATUS, m.m_source, NONE, OK);
          else 
              reply(DEV_REVIVE, m.m_source, suspended, result);
          suspended = NONE;
          break;

      case SYN_ALARM:		/* shouldn't happen */
      case SYS_SIG:		/* ignore system events */
          continue;		

      default:
          reply(TASK_REPLY, m.m_source, m.PROC_NR, EINVAL);
      }	
  }
}

/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
PRIVATE void reply(int code, int replyee, int process, int status)
{
  message m;
  int s;

  m.m_type = code;		/* TASK_REPLY or REVIVE */
  m.REP_STATUS = status;	/* result of device operation */
  m.REP_PROC_NR = process;	/* which user made the request */
  if (OK != (s=send(replyee, &m)))
      panic("CMOS", "sending reply failed", s);
}

/*===========================================================================*
 *				gettime					     *
 *===========================================================================*/
PRIVATE int gettime(int who, int y2kflag, vir_bytes dst_time)
{
  unsigned char mach_id, cmos_state;
  struct tm time1;
  int i, s;

  /* First obtain the machine ID to see if we can read the CMOS clock. Only
   * for PS_386 and PC_AT this is possible. Otherwise, return an error.  
   */
  sys_vircopy(SELF, BIOS_SEG, (vir_bytes) MACHINE_ID_ADDR, 
  	SELF, D, (vir_bytes) &mach_id, MACHINE_ID_SIZE);
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
  clock_t t0,t1;

  /* Start a timer to keep us from getting stuck on a dead clock. */
  getuptime(&t0);
  do {
	osec = -1;
	n = 0;
	do {
	        getuptime(&t1); 
		if (t1-t0 > 5*HZ) {
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


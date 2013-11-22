/* This file takes care of those system calls that deal with time.
 *
 * The entry points into this file are
 *   do_getres:		perform the CLOCK_GETRES system call
 *   do_gettime:	perform the CLOCK_GETTIME system call
 *   do_settime:	perform the CLOCK_SETTIME system call
 *   do_time:		perform the GETTIMEOFDAY system call
 *   do_stime:		perform the STIME system call
 */

#include "pm.h"
#include <minix/callnr.h>
#include <minix/com.h>
#include <signal.h>
#include <sys/time.h>
#include "mproc.h"

/*===========================================================================*
 *				do_gettime				     *
 *===========================================================================*/
int do_gettime()
{
  clock_t ticks, realtime, clock;
  time_t boottime;
  int s;

  if ( (s=getuptime(&ticks, &realtime, &boottime)) != OK)
  	panic("do_time couldn't get uptime: %d", s);

  switch (m_in.PM_TIME_CLK_ID) {
	case CLOCK_REALTIME:
		clock = realtime;
		break;
	case CLOCK_MONOTONIC:
		clock = ticks;
		break;
	default:
		return EINVAL; /* invalid/unsupported clock_id */
  }

  mp->mp_reply.PM_TIME_SEC = (time_t) (boottime + (clock / system_hz));
  mp->mp_reply.PM_TIME_NSEC =
	(long) ((clock % system_hz) * 1000000000ULL / system_hz);

  return(OK);
}

/*===========================================================================*
 *				do_getres				     *
 *===========================================================================*/
int do_getres()
{
  switch (m_in.PM_TIME_CLK_ID) {
	case CLOCK_REALTIME:
	case CLOCK_MONOTONIC:
		/* tv_sec is always 0 since system_hz is an int */
		mp->mp_reply.PM_TIME_SEC = (time_t) 0;
		mp->mp_reply.PM_TIME_NSEC = 1000000000 / system_hz;
		return(OK);
	default:
		return EINVAL; /* invalid/unsupported clock_id */
  }
}

/*===========================================================================*
 *				do_settime				     *
 *===========================================================================*/
int do_settime()
{
  int s;

  if (mp->mp_effuid != SUPER_USER) { 
      return(EPERM);
  }

  switch (m_in.PM_TIME_CLK_ID) {
	case CLOCK_REALTIME:
		s= sys_settime(m_in.PM_TIME_NOW, m_in.PM_TIME_CLK_ID,
			m_in.PM_TIME_SEC, m_in.PM_TIME_NSEC);
		return(s);
	case CLOCK_MONOTONIC: /* monotonic cannot be changed */
	default:
		return EINVAL; /* invalid/unsupported clock_id */
  }
}

/*===========================================================================*
 *				do_time					     *
 *===========================================================================*/
int do_time()
{
/* Perform the time(tp) system call. This returns the time in seconds since 
 * 1.1.1970.  MINIX is an astrophysically naive system that assumes the earth 
 * rotates at a constant rate and that such things as leap seconds do not 
 * exist.
 */
  clock_t ticks, realtime;
  time_t boottime;
  int s;

  if ( (s=getuptime(&ticks, &realtime, &boottime)) != OK)
  	panic("do_time couldn't get uptime: %d", s);

  mp->mp_reply.PM_TIME_SEC = (time_t) (boottime + (realtime / system_hz));
  mp->mp_reply.PM_TIME_USEC =
	(long) ((realtime % system_hz) * 1000000ULL / system_hz);
  return(OK);
}

/*===========================================================================*
 *				do_stime				     *
 *===========================================================================*/
int do_stime()
{
/* Perform the stime(tp) system call. Retrieve the system's uptime (ticks 
 * since boot) and pass the new time in seconds at system boot to the kernel.
 */
  clock_t uptime, realtime;
  time_t boottime;
  int s;

  if (mp->mp_effuid != SUPER_USER) { 
      return(EPERM);
  }
  if ( (s=getuptime(&uptime, &realtime, &boottime)) != OK) 
      panic("do_stime couldn't get uptime: %d", s);
  boottime = (long) m_in.PM_TIME_SEC - (realtime/system_hz);

  s= sys_stime(boottime);		/* Tell kernel about boottime */
  if (s != OK)
	panic("pm: sys_stime failed: %d", s);

  return(OK);
}

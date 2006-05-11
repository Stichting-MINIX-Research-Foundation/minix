/* This file takes care of those system calls that deal with time.
 *
 * The entry points into this file are
 *   do_time:		perform the TIME system call
 *   do_stime:		perform the STIME system call
 *   do_times:		perform the TIMES system call
 */

#include "pm.h"
#include <minix/callnr.h>
#include <minix/com.h>
#include <signal.h>
#include "mproc.h"
#include "param.h"

/*===========================================================================*
 *				do_time					     *
 *===========================================================================*/
PUBLIC int do_time()
{
/* Perform the time(tp) system call. This returns the time in seconds since 
 * 1.1.1970.  MINIX is an astrophysically naive system that assumes the earth 
 * rotates at a constant rate and that such things as leap seconds do not 
 * exist.
 */
  clock_t uptime;
  int s;

  if ( (s=getuptime(&uptime)) != OK) 
  	panic(__FILE__,"do_time couldn't get uptime", s);

  mp->mp_reply.reply_time = (time_t) (boottime + (uptime/HZ));
  mp->mp_reply.reply_utime = (uptime%HZ)*1000000/HZ;
  return(OK);
}

/*===========================================================================*
 *				do_stime				     *
 *===========================================================================*/
PUBLIC int do_stime()
{
/* Perform the stime(tp) system call. Retrieve the system's uptime (ticks 
 * since boot) and store the time in seconds at system boot in the global
 * variable 'boottime'.
 */
  clock_t uptime;
  int s;

  if (mp->mp_effuid != SUPER_USER) { 
      return(EPERM);
  }
  if ( (s=getuptime(&uptime)) != OK) 
      panic(__FILE__,"do_stime couldn't get uptime", s);
  boottime = (long) m_in.stime - (uptime/HZ);

  if (mp->mp_fs_call != PM_IDLE)
	panic("pm", "do_stime: not idle", mp->mp_fs_call);
  mp->mp_fs_call= PM_STIME;
  s= notify(FS_PROC_NR);
  if (s != OK) panic("pm", "do_stime: unable to notify FS", s);

  /* Do not reply until FS is ready to process the stime request */
  return(SUSPEND);
}

/*===========================================================================*
 *				do_times				     *
 *===========================================================================*/
PUBLIC int do_times()
{
/* Perform the times(buffer) system call. */
  register struct mproc *rmp = mp;
  clock_t t[5];
  int s;

  if (OK != (s=sys_times(who_e, t)))
      panic(__FILE__,"do_times couldn't get times", s);
  rmp->mp_reply.reply_t1 = t[0];		/* user time */
  rmp->mp_reply.reply_t2 = t[1];		/* system time */
  rmp->mp_reply.reply_t3 = rmp->mp_child_utime;	/* child user time */
  rmp->mp_reply.reply_t4 = rmp->mp_child_stime;	/* child system time */
  rmp->mp_reply.reply_t5 = t[4];		/* uptime since boot */

  return(OK);
}


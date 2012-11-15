/* This file contains procedures to dump to PM' data structures.
 *
 * The entry points into this file are
 *   mproc_dmp:   	display PM process table	  
 *
 * Created:
 *   May 11, 2005:	by Jorrit N. Herder
 */

#include "inc.h"
#include "../pm/mproc.h"
#include <timers.h> 
#include <minix/config.h> 
#include <minix/type.h> 

struct mproc mproc[NR_PROCS];

/*===========================================================================*
 *				mproc_dmp				     *
 *===========================================================================*/
static char *flags_str(int flags)
{
	static char str[14];
	str[0] = (flags & WAITING) ? 'W' : '-';
	str[1] = (flags & ZOMBIE)  ? 'Z' : '-';
	str[2] = (flags & PAUSED)  ? 'P' : '-';
	str[3] = (flags & ALARM_ON)  ? 'A' : '-';
	str[4] = (flags & EXITING) ? 'E' : '-';
	str[5] = (flags & STOPPED)  ? 'S' : '-';
	str[6] = (flags & SIGSUSPENDED)  ? 'U' : '-';
	str[7] = (flags & REPLY)  ? 'R' : '-';
	str[8] = (flags & VFS_CALL) ? 'F' : '-';
	str[9] = (flags & PM_SIG_PENDING) ? 's' : '-';
	str[10] = (flags & PRIV_PROC)  ? 'p' : '-';
	str[11] = (flags & PARTIAL_EXEC) ? 'x' : '-';
	str[12] = (flags & DELAY_CALL) ? 'd' : '-';
	str[13] = '\0';

	return str;
}

void mproc_dmp()
{
  struct mproc *mp;
  int i, n=0;
  static int prev_i = 0;

  if (getsysinfo(PM_PROC_NR, SI_PROC_TAB, mproc, sizeof(mproc)) != OK) {
	printf("Error obtaining table from PM. Perhaps recompile IS?\n");
	return;
  }

  printf("Process manager (PM) process table dump\n");
  printf("-process- -nr-pnr-tnr- --pid--ppid--pgrp- -uid--  -gid--  -nice- -flags-------\n");
  for (i=prev_i; i<NR_PROCS; i++) {
  	mp = &mproc[i];
  	if (mp->mp_pid == 0 && i != PM_PROC_NR) continue;
  	if (++n > 22) break;
  	printf("%8.8s %4d%4d%4d  %5d %5d %5d  ", 
  		mp->mp_name, i, mp->mp_parent, mp->mp_tracer, mp->mp_pid, mproc[mp->mp_parent].mp_pid, mp->mp_procgrp);
  	printf("%2d(%2d)  %2d(%2d)   ",
  		mp->mp_realuid, mp->mp_effuid, mp->mp_realgid, mp->mp_effgid);
  	printf(" %3d  %s  ", 
  		mp->mp_nice, flags_str(mp->mp_flags)); 
  	printf("\n");
  }
  if (i >= NR_PROCS) i = 0;
  else printf("--more--\r");
  prev_i = i;
}

/*===========================================================================*
 *				sigaction_dmp				     *
 *===========================================================================*/
void sigaction_dmp()
{
  struct mproc *mp;
  int i, n=0;
  static int prev_i = 0;
  clock_t uptime;

  if (getsysinfo(PM_PROC_NR, SI_PROC_TAB, mproc, sizeof(mproc)) != OK) {
	printf("Error obtaining table from PM. Perhaps recompile IS?\n");
	return;
  }
  getuptime(&uptime);

  printf("Process manager (PM) signal action dump\n");
  printf("-process- -nr- --ignore- --catch- --block- -pending- -alarm---\n");
  for (i=prev_i; i<NR_PROCS; i++) {
  	mp = &mproc[i];
  	if (mp->mp_pid == 0 && i != PM_PROC_NR) continue;
  	if (++n > 22) break;
  	printf("%8.8s  %3d  ", mp->mp_name, i);
  	printf(" %08lx %08lx %08lx ", 
  		mp->mp_ignore, mp->mp_catch, mp->mp_sigmask); 
  	printf("%08lx  ", mp->mp_sigpending);
  	if (mp->mp_flags & ALARM_ON) printf("%8d", mp->mp_timer.tmr_exp_time-uptime);
  	else printf("       -");
  	printf("\n");
  }
  if (i >= NR_PROCS) i = 0;
  else printf("--more--\r");
  prev_i = i;
}



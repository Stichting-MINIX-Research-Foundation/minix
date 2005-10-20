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

PUBLIC struct mproc mproc[NR_PROCS];

/*===========================================================================*
 *				mproc_dmp				     *
 *===========================================================================*/
PRIVATE char *flags_str(int flags)
{
	static char str[10];
	str[0] = (flags & WAITING) ? 'W' : '-';
	str[1] = (flags & ZOMBIE)  ? 'Z' : '-';
	str[2] = (flags & PAUSED)  ? 'P' : '-';
	str[3] = (flags & ALARM_ON)  ? 'A' : '-';
	str[4] = (flags & TRACED)  ? 'T' : '-';
	str[5] = (flags & STOPPED)  ? 'S' : '-';
	str[6] = (flags & SIGSUSPENDED)  ? 'U' : '-';
	str[7] = (flags & REPLY)  ? 'R' : '-';
	str[8] = (flags & ONSWAP)  ? 'O' : '-';
	str[9] = (flags & SWAPIN)  ? 'I' : '-';
	str[10] = (flags & DONT_SWAP)  ? 'D' : '-';
	str[11] = (flags & PRIV_PROC)  ? 'P' : '-';
	str[12] = '\0';

	return str;
}

PUBLIC void mproc_dmp()
{
  struct mproc *mp;
  int i, n=0;
  static int prev_i = 0;

  printf("Process manager (PM) process table dump\n");

  getsysinfo(PM_PROC_NR, SI_PROC_TAB, mproc);

  printf("-process- -nr-prnt- -pid/ppid/grp- -uid--gid- -nice- -flags------\n");
  for (i=prev_i; i<NR_PROCS; i++) {
  	mp = &mproc[i];
  	if (mp->mp_pid == 0 && i != PM_PROC_NR) continue;
  	if (++n > 22) break;
  	printf("%8.8s %4d%4d  %4d%4d%4d    ", 
  		mp->mp_name, i, mp->mp_parent, mp->mp_pid, mproc[mp->mp_parent].mp_pid, mp->mp_procgrp);
  	printf("%d(%d)  %d(%d)  ",
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
PUBLIC void sigaction_dmp()
{
  struct mproc *mp;
  int i, n=0;
  static int prev_i = 0;
  clock_t uptime;

  printf("Process manager (PM) signal action dump\n");

  getsysinfo(PM_PROC_NR, SI_PROC_TAB, mproc);
  getuptime(&uptime);

  printf("-process- -nr- --ignore- --catch- --block- -tomess-- -pending-- -alarm---\n");
  for (i=prev_i; i<NR_PROCS; i++) {
  	mp = &mproc[i];
  	if (mp->mp_pid == 0 && i != PM_PROC_NR) continue;
  	if (++n > 22) break;
  	printf("%8.8s  %3d  ", mp->mp_name, i);
  	printf(" 0x%06x 0x%06x 0x%06x 0x%06x   ", 
  		mp->mp_ignore, mp->mp_catch, mp->mp_sigmask, mp->mp_sig2mess); 
  	printf("0x%06x  ", mp->mp_sigpending);
  	if (mp->mp_flags & ALARM_ON) printf("%8u", mp->mp_timer.tmr_exp_time-uptime);
  	else printf("       -");
  	printf("\n");
  }
  if (i >= NR_PROCS) i = 0;
  else printf("--more--\r");
  prev_i = i;
}

/*===========================================================================*
 *				holes_dmp				     *
 *===========================================================================*/
PUBLIC void holes_dmp(void)
{
	static struct pm_mem_info pmi;
	int h;
	int largest_bytes = 0, total_bytes = 0;

	if(getsysinfo(PM_PROC_NR, SI_MEM_ALLOC, &pmi) != OK) {
		printf("Obtaining memory hole list failed.\n");
		return;
	}
	printf("Available memory stats\n");

	for(h = 0; h < _NR_HOLES; h++) {
		if(pmi.pmi_holes[h].h_base && pmi.pmi_holes[h].h_len) {
			int bytes;
			bytes = (pmi.pmi_holes[h].h_len << CLICK_SHIFT);
			printf("%08lx: %6d kB\n",
				pmi.pmi_holes[h].h_base << CLICK_SHIFT, bytes / 1024);
			if(bytes > largest_bytes) largest_bytes = bytes;
			total_bytes += bytes;
		}
	}
	printf("\n"
		"Total memory free:     %7d kB\n"
		"Largest chunk:         %7d kB\n"
		"Uncontiguous rest:     %7d kB (%d%% of total free)\n"
		"Memory high watermark: %7d kB\n",
		total_bytes/1024,
		largest_bytes/1024,
		(total_bytes-largest_bytes)/1024,
		100*(total_bytes/100-largest_bytes/100)/total_bytes,
		(pmi.pmi_hi_watermark/1024 << CLICK_SHIFT));

	return;
}


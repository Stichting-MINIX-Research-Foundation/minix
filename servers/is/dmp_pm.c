/* This file contains procedures to dump to PM' data structures.
 *
 * The entry points into this file are
 *   mproc_dmp:   	display PM process table	  
 *
 * Created:
 *   May 11, 2005:	by Jorrit N. Herder
 */

#include "is.h"
#include "../pm/mproc.h"


PUBLIC struct mproc mproc[NR_PROCS];

/*===========================================================================*
 *				mproc_dmp				     *
 *===========================================================================*/
PUBLIC void mproc_dmp()
{
  struct mproc *mp;
  int i, n=0;
  static int prev_i = 0;

  printf("Process manager (PM) process table dump\n");

  getsysinfo(PM_PROC_NR, SI_PROC_TAB, mproc);

  printf("-process- -nr-prnt- -pid/grp- --uid---gid-- -flags- --ignore--catch--block--\n");
  for (i=prev_i; i<NR_PROCS; i++) {
  	mp = &mproc[i];
  	if (mp->mp_pid == 0 && i != PM_PROC_NR) continue;
  	if (++n > 22) break;
  	printf("%8.8s %4d%4d  %4d%4d    ", 
  		mp->mp_name, i, mp->mp_parent, mp->mp_pid, mp->mp_procgrp);
  	printf("%d (%d)  %d (%d)  ",
  		mp->mp_realuid, mp->mp_effuid, mp->mp_realgid, mp->mp_effgid);
  	printf("0x%04x  ", 
  		mp->mp_flags); 
  	printf("0x%05x 0x%05x 0x%05x", 
  		mp->mp_ignore, mp->mp_catch, mp->mp_sigmask); 
  	printf("\n");
  }
  if (i >= NR_PROCS) i = 0;
  else printf("--more--\r");
  prev_i = i;
}


/*===========================================================================*
 *				...._dmp				     *
 *===========================================================================*/



/* This file contains procedures to dump to PM' data structures.
 *
 * The entry points into this file are
 *   do_fkey_pressed:	a function key was pressed	
 *   mproc_dump:   	display PM process table	  
 *
 * Created:
 *   May 11, 2005:	by Jorrit N. Herder
 */

#include "pm.h"
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/keymap.h>
#include <signal.h>
#include "mproc.h"

FORWARD _PROTOTYPE( void mproc_dmp, (void));


/*===========================================================================*
 *				do_fkey_pressed				     *
 *===========================================================================*/
PUBLIC int do_fkey_pressed(void)
{
  printf("Process Manager debug dump: ");
#if DEAD_CODE
  switch (m_in.FKEY_NUM) {
#else
  switch (m_in.NOTIFY_FLAGS) {
#endif
    	case SF7:	mproc_dmp();		break;

    	default:
#if DEAD_CODE
    		printf("PM: unhandled notification for Shift+F%d key.\n",
    			m_in.FKEY_NUM);
#else
    		printf("PM: unhandled notification for Shift+F%d key.\n",
    			m_in.NOTIFY_FLAGS);
#endif
  }
}


/*===========================================================================*
 *				mproc_dmp				     *
 *===========================================================================*/
PRIVATE void mproc_dmp()
{
  struct mproc *mp;
  int i, n=0;
  static int prev_i;
  printf("Process Table\n");

  printf("-process- -nr-prnt- -pid/grp- --uid---gid-- -flags- --ignore--catch--block--\n");
  for (i=prev_i; i<NR_PROCS; i++) {
  	mp = &mproc[i];
  	if (mp->mp_pid <= 0) continue;
  	if (++n > 22) break;
  	printf("%8.8s %4d%4d  %4d%4d    ", 
  		mp->mp_name, i, mp->mp_parent, mp->mp_pid, mp->mp_procgrp);
  	printf("%d (%d)  %d (%d)  ",
  		mp->mp_realuid, mp->mp_effuid, mp->mp_realgid, mp->mp_effgid);
  	printf("0x%04x  ", 
  		mp->mp_flags); 
  	printf("0x%04x 0x%04x 0x%04x", 
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



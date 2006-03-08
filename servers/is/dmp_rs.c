/* This file contains procedures to dump RS data structures.
 *
 * The entry points into this file are
 *   rproc_dump:   	display RS system process table	  
 *
 * Created:
 *   Oct 03, 2005:	by Jorrit N. Herder
 */

#include "inc.h"
#include "../rs/manager.h"

PUBLIC struct rproc rproc[NR_SYS_PROCS];

FORWARD _PROTOTYPE( char *s_flags_str, (int flags)		);

/*===========================================================================*
 *				rproc_dmp				     *
 *===========================================================================*/
PUBLIC void rproc_dmp()
{
  struct rproc *rp;
  int i,j, n=0;
  static int prev_i=0;

  getsysinfo(RS_PROC_NR, SI_PROC_TAB, rproc);

  printf("Reincarnation Server (RS) system process table dump\n");
  printf("-----proc---pid-flag--dev- -T---checked----alive-starts-backoff-command (argc)-\n");
  for (i=prev_i; i<NR_SYS_PROCS; i++) {
  	rp = &rproc[i];
  	if (! rp->r_flags & RS_IN_USE) continue;
  	if (++n > 22) break;
  	printf("%9d %5d %s %3d/%2d %3u %8u %8u %4dx %3d %s (%d)",
  		rp->r_proc_nr_e, rp->r_pid, 
		s_flags_str(rp->r_flags),
  		rp->r_dev_nr, rp->r_dev_style,
		rp->r_period, 
		rp->r_check_tm, rp->r_alive_tm,
		rp->r_restarts, rp->r_backoff,
		rp->r_cmd,
		rp->r_argc
  	);
	printf("\n");
  }
  if (i >= NR_SYS_PROCS) i = 0;
  else printf("--more--\r");
  prev_i = i;
}


PRIVATE char *s_flags_str(int flags)
{
	static char str[5];
	str[0] = (flags & RS_IN_USE) 	 ? 'U' : '-';
	str[1] = (flags & RS_EXITING)    ? 'E' : '-';
	str[2] = '-';
	str[3] = '\0';

	return(str);
}


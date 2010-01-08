/* This file contains procedures to dump RS data structures.
 *
 * The entry points into this file are
 *   rproc_dump:   	display RS system process table	  
 *
 * Created:
 *   Oct 03, 2005:	by Jorrit N. Herder
 */

#include "inc.h"
#include <timers.h>
#include <minix/rs.h>
#include "../../kernel/priv.h"
#include "../rs/const.h"
#include "../rs/type.h"

PUBLIC struct rprocpub rprocpub[NR_SYS_PROCS];
PUBLIC struct rproc rproc[NR_SYS_PROCS];

FORWARD _PROTOTYPE( char *s_flags_str, (int flags)		);

/*===========================================================================*
 *				rproc_dmp				     *
 *===========================================================================*/
PUBLIC void rproc_dmp()
{
  struct rproc *rp;
  struct rprocpub *rpub;
  int i,j, n=0;
  static int prev_i=0;

  getsysinfo(RS_PROC_NR, SI_PROCPUB_TAB, rprocpub);
  getsysinfo(RS_PROC_NR, SI_PROC_TAB, rproc);

  printf("Reincarnation Server (RS) system process table dump\n");
  printf("----label---- endpoint- -pid- flags -dev- -T- alive_tm starts command\n");
  for (i=prev_i; i<NR_SYS_PROCS; i++) {
  	rp = &rproc[i];
  	rpub = &rprocpub[i];
  	if (! rp->r_flags & RS_IN_USE) continue;
  	if (++n > 22) break;
  	printf("%13s %9d %5d %5s %3d/%1d %3u %8u %5dx %s",
  		rpub->label, rpub->endpoint, rp->r_pid,
		s_flags_str(rp->r_flags), rpub->dev_nr, rpub->dev_style,
		rpub->period, rp->r_alive_tm, rp->r_restarts,
		rp->r_cmd
  	);
	printf("\n");
  }
  if (i >= NR_SYS_PROCS) i = 0;
  else printf("--more--\r");
  prev_i = i;
}


PRIVATE char *s_flags_str(int flags)
{
	static char str[10];
	str[0] = (flags & RS_IN_USE) 	    ? 'U' : '-';
	str[1] = (flags & RS_INITIALIZING)  ? 'I' : '-';
	str[2] = (flags & RS_UPDATING)      ? 'u' : '-';
	str[3] = (flags & RS_EXITING)       ? 'E' : '-';
	str[4] = (flags & RS_NOPINGREPLY)   ? 'N' : '-';
	str[5] = '\0';

	return(str);
}


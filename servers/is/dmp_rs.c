/* This file contains procedures to dump RS data structures.
 *
 * The entry points into this file are
 *   rproc_dump:   	display RS system process table	  
 *
 * Created:
 *   Oct 03, 2005:	by Jorrit N. Herder
 */

#include "inc.h"
#include <minix/timers.h>
#include <minix/rs.h>
#include "kernel/priv.h"
#include "../rs/const.h"
#include "../rs/type.h"

struct rprocpub rprocpub[NR_SYS_PROCS];
struct rproc rproc[NR_SYS_PROCS];

static char *s_flags_str(int flags, int sys_flags);

/*===========================================================================*
 *				rproc_dmp				     *
 *===========================================================================*/
void rproc_dmp()
{
  struct rproc *rp;
  struct rprocpub *rpub;
  int i, n=0;
  static int prev_i=0;

  if (getsysinfo(RS_PROC_NR, SI_PROCPUB_TAB, rprocpub, sizeof(rprocpub)) != OK
	|| getsysinfo(RS_PROC_NR, SI_PROC_TAB, rproc, sizeof(rproc)) != OK) {
	printf("Error obtaining table from RS. Perhaps recompile IS?\n");
	return;
  }

  printf("Reincarnation Server (RS) system process table dump\n");
  printf("----label---- endpoint- -pid- flags- -dev- -T- alive_tm starts command\n");
  for (i=prev_i; i<NR_SYS_PROCS; i++) {
  	rp = &rproc[i];
  	rpub = &rprocpub[i];
  	if (! (rp->r_flags & RS_IN_USE)) continue;
  	if (++n > 22) break;
	printf("%13s %9d %5d %6s %4d %4ld %8lu %5dx %s",
  		rpub->label, rpub->endpoint, rp->r_pid,
		s_flags_str(rp->r_flags, rpub->sys_flags), rpub->dev_nr,
		rp->r_period, rp->r_alive_tm, rp->r_restarts,
		rp->r_args
  	);
	printf("\n");
  }
  if (i >= NR_SYS_PROCS) i = 0;
  else printf("--more--\r");
  prev_i = i;
}


static char *s_flags_str(int flags, int sys_flags)
{
	static char str[10];
	str[0] = (flags & RS_ACTIVE)        ? 'A' : '-';
	str[1] = (flags & RS_UPDATING)      ? 'U' : '-';
	str[2] = (flags & RS_EXITING)       ? 'E' : '-';
	str[3] = (flags & RS_NOPINGREPLY)   ? 'N' : '-';
	str[4] = (sys_flags & SF_USE_COPY)  ? 'C' : '-';
	str[5] = (sys_flags & SF_USE_REPL)  ? 'R' : '-';
	str[6] = '\0';

	return(str);
}


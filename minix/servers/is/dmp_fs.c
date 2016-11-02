/* This file contains procedures to dump to FS' data structures.
 *
 * The entry points into this file are
 *   dtab_dump:   	display device <-> driver mappings
 *   fproc_dump:   	display FS process table
 *
 * Created:
 *   Oct 01, 2004:	by Jorrit N. Herder
 */

#include "inc.h"
#include "mfs/const.h"
#include "vfs/const.h"
#include "vfs/fproc.h"
#include "vfs/dmap.h"
#include <minix/dmap.h>

struct fproc fproc[NR_PROCS];
struct dmap dmap[NR_DEVICES];

/*===========================================================================*
 *				fproc_dmp				     *
 *===========================================================================*/
void
fproc_dmp(void)
{
  struct fproc *fp;
  int i, j, nfds, n=0;
  static int prev_i;

  if (getsysinfo(VFS_PROC_NR, SI_PROC_TAB, fproc, sizeof(fproc)) != OK) {
	printf("Error obtaining table from VFS. Perhaps recompile IS?\n");
	return;
  }

  printf("File System (FS) process table dump\n");
  printf("-nr- -pid- -tty- -umask- --uid-- --gid-- -ldr-fds-sus-rev-proc-\n");
  for (i=prev_i; i<NR_PROCS; i++) {
  	fp = &fproc[i];
  	if (fp->fp_pid <= 0) continue;
  	if (++n > 22) break;
	for (j = nfds = 0; j < OPEN_MAX; j++)
		if (fp->fp_filp[j] != NULL) nfds++;
	printf("%3d  %4d  %2d/%d  0x%05x %2d (%2d) %2d (%2d) %3d %3d %3d %3d ",
		i, fp->fp_pid,
		major(fp->fp_tty), minor(fp->fp_tty),
		fp->fp_umask,
		fp->fp_realuid, fp->fp_effuid, fp->fp_realgid, fp->fp_effgid,
		!!(fp->fp_flags & FP_SESLDR), nfds,
		fp->fp_blocked_on, !!(fp->fp_flags & FP_REVIVED)
	);
	if (fp->fp_blocked_on == FP_BLOCKED_ON_CDEV)
		printf("%4d\n", fp->fp_cdev.endpt);
	else
		printf(" nil\n");
  }
  if (i >= NR_PROCS) i = 0;
  else printf("--more--\r");
  prev_i = i;
}

/*===========================================================================*
 *				dtab_dmp				     *
 *===========================================================================*/
void
dtab_dmp(void)
{
    int i;

    if (getsysinfo(VFS_PROC_NR, SI_DMAP_TAB, dmap, sizeof(dmap)) != OK) {
        printf("Error obtaining table from VFS. Perhaps recompile IS?\n");
        return;
    }

    printf("File System (FS) device <-> driver mappings\n");
    printf("    Label     Major Driver ept\n");
    printf("------------- ----- ----------\n");
    for (i=0; i<NR_DEVICES; i++) {
        if (dmap[i].dmap_driver == NONE) continue;
        printf("%13s %5d %10d\n", dmap[i].dmap_label, i, dmap[i].dmap_driver);
    }
}

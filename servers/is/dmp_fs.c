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
#include "../mfs/const.h"
#include "../vfs/const.h"
#include "../vfs/fproc.h"
#include "../vfs/dmap.h"
#include <minix/dmap.h>

struct fproc fproc[NR_PROCS];
struct dmap dmap[NR_DEVICES];

/*===========================================================================*
 *				fproc_dmp				     *
 *===========================================================================*/
void fproc_dmp()
{
  struct fproc *fp;
  int i, n=0;
  static int prev_i;

  if (getsysinfo(VFS_PROC_NR, SI_PROC_TAB, fproc, sizeof(fproc)) != OK) {
	printf("Error obtaining table from VFS. Perhaps recompile IS?\n");
	return;
  }

  printf("File System (FS) process table dump\n");
  printf("-nr- -pid- -tty- -umask- --uid-- --gid-- -ldr- -sus-rev-proc-\n");
  for (i=prev_i; i<NR_PROCS; i++) {
  	fp = &fproc[i];
  	if (fp->fp_pid <= 0) continue;
  	if (++n > 22) break;
	printf("%3d  %4d  %2d/%d  0x%05x %2d (%2d) %2d (%2d) %3d   %3d %3d ",
		i, fp->fp_pid,
		major(fp->fp_tty), minor(fp->fp_tty),
		fp->fp_umask,
		fp->fp_realuid, fp->fp_effuid, fp->fp_realgid, fp->fp_effgid,
		!!(fp->fp_flags & FP_SESLDR),
		fp->fp_blocked_on, !!(fp->fp_flags & FP_REVIVED)
	);
	if (fp->fp_blocked_on == FP_BLOCKED_ON_OTHER)
		printf("%4d\n", fp->fp_task);
	else
		printf(" nil\n");
  }
  if (i >= NR_PROCS) i = 0;
  else printf("--more--\r");
  prev_i = i;
}

/*===========================================================================*
 *				dmap_flags				     *
 *===========================================================================*/
static char * dmap_flags(int flags)
{
	static char fl[10];
	strlcpy(fl, "-----", sizeof(fl));
	if(flags & DRV_FORCED)  fl[0] = 'F';
	return fl;
}

/*===========================================================================*
 *				dmap_style				     *
 *===========================================================================*/
static char * dmap_style(int dev_style)
{
	switch(dev_style) {
	case STYLE_DEV:	     return "STYLE_DEV";
	case STYLE_DEVA:     return "STYLE_DEVA";
	case STYLE_TTY:      return "STYLE_TTY";
	case STYLE_CTTY:     return "STYLE_CTTY";
	case STYLE_CLONE:    return "STYLE_CLONE";
	case STYLE_CLONE_A:  return "STYLE_CLONE_A";
	default:             return "UNKNOWN";
	}
}

/*===========================================================================*
 *				dtab_dmp				     *
 *===========================================================================*/
void dtab_dmp()
{
    int i;

    if (getsysinfo(VFS_PROC_NR, SI_DMAP_TAB, dmap, sizeof(dmap)) != OK) {
        printf("Error obtaining table from VFS. Perhaps recompile IS?\n");
        return;
    }
    
    printf("File System (FS) device <-> driver mappings\n");
    printf("    Label     Major Driver ept Flags     Style   \n");
    printf("------------- ----- ---------- ----- -------------\n");
    for (i=0; i<NR_DEVICES; i++) {
        if (dmap[i].dmap_driver == NONE) continue;
        printf("%13s %5d %10d %s %-13s\n",
		dmap[i].dmap_label, i, dmap[i].dmap_driver,
		dmap_flags(dmap[i].dmap_flags), dmap_style(dmap[i].dmap_style));
    }
}


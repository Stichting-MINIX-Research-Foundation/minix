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

PUBLIC struct fproc fproc[NR_PROCS];
PUBLIC struct dmap dmap[NR_DEVICES];

/*===========================================================================*
 *				fproc_dmp				     *
 *===========================================================================*/
PUBLIC void fproc_dmp()
{
  struct fproc *fp;
  int i, n=0;
  static int prev_i;

  getsysinfo(VFS_PROC_NR, SI_PROC_TAB, fproc);

  printf("File System (FS) process table dump\n");
  printf("-nr- -pid- -tty- -umask- --uid-- --gid-- -ldr- -sus-rev-proc-\n");
  for (i=prev_i; i<NR_PROCS; i++) {
  	fp = &fproc[i];
  	if (fp->fp_pid <= 0) continue;
  	if (++n > 22) break;
  	printf("%3d  %4d  %2d/%d  0x%05x %2d (%2d) %2d (%2d) %3d   %3d %3d ",
  		i, fp->fp_pid, 
  		((fp->fp_tty>>MAJOR)&BYTE), ((fp->fp_tty>>MINOR)&BYTE), 
  		fp->fp_umask,
  		fp->fp_realuid, fp->fp_effuid, fp->fp_realgid, fp->fp_effgid,
  		fp->fp_sesldr,
  		fp->fp_blocked_on, !!fp->fp_revived
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
PRIVATE char * dmap_flags(int flags)
{
	static char fl[10];
	strcpy(fl, "-----");
	if(flags & DRV_FORCED)  fl[0] = 'F';
	return fl;
}

/*===========================================================================*
 *				dmap_style				     *
 *===========================================================================*/
PRIVATE char * dmap_style(int dev_style)
{
	static char str[16];
	switch(dev_style) {
	case STYLE_DEV:	   strcpy(str, "STYLE_DEV"); break;
	case STYLE_DEVA:   strcpy(str, "STYLE_DEVA"); break;
	case STYLE_TTY:    strcpy(str, "STYLE_TTY"); break;
	case STYLE_CTTY:   strcpy(str, "STYLE_CTTY"); break;
	case STYLE_CLONE:  strcpy(str, "STYLE_CLONE"); break;
	default:           strcpy(str, "UNKNOWN"); break;
	}

	return str;
}

/*===========================================================================*
 *				dtab_dmp				     *
 *===========================================================================*/
PUBLIC void dtab_dmp()
{
    int i;

    getsysinfo(VFS_PROC_NR, SI_DMAP_TAB, dmap);
    
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


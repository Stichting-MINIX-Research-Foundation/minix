/* This file contains procedures to dump to FS' data structures.
 *
 * The entry points into this file are
 *   dtab_dump:   	display device <-> driver mappings	  
 *   fproc_dump:   	display FS process table	  
 *
 * Created:
 *   Oct 01, 2004:	by Jorrit N. Herder
 */

#include "is.h"
#include "../fs/const.h"
#include "../fs/fproc.h"
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

  getsysinfo(FS_PROC_NR, SI_PROC_TAB, fproc);

  printf("File System (FS) process table dump\n");
  printf("-nr- -pid- -tty- -umask- --uid-- --gid-- -ldr- -sus-rev-proc- -cloexec-\n");
  for (i=prev_i; i<NR_PROCS; i++) {
  	fp = &fproc[i];
  	if (fp->fp_pid <= 0) continue;
  	if (++n > 22) break;
  	printf("%3d  %4d  %2d/%d  0x%05x %2d (%d)  %2d (%d)  %3d   %3d %3d %4d    0x%05x\n",
  		i, fp->fp_pid, 
  		((fp->fp_tty>>MAJOR)&BYTE), ((fp->fp_tty>>MINOR)&BYTE), 
  		fp->fp_umask,
  		fp->fp_realuid, fp->fp_effuid, fp->fp_realgid, fp->fp_effgid,
  		fp->fp_sesldr,
  		fp->fp_suspended, fp->fp_revived, fp->fp_task,
  		fp->fp_cloexec
  	);
  }
  if (i >= NR_PROCS) i = 0;
  else printf("--more--\r");
  prev_i = i;
}


/*===========================================================================*
 *				dtab_dmp				     *
 *===========================================================================*/
PUBLIC void dtab_dmp()
{
    int i;
    char *file[] = {
        "not used  ", "/dev/mem  ", "/dev/fd0  ", "/dev/c0   ", "/dev/tty0 ", 
        "/dev/tty  ", "/dev/lp   ", "/dev/ip   ", "/dev/c1   ", "not used  ",
        "/dev/c2   ", "not used  ", "/dev/c3   ", "/dev/audio", "/dev/mixer",
    };

    getsysinfo(FS_PROC_NR, SI_DMAP_TAB, dmap);
    
    printf("File System (FS) device <-> driver mappings\n");
    printf("Major  Proc\n");
    printf("-----  ----\n");
    for (i=0; i<NR_DEVICES; i++) {
        printf("%5d  ", i);
        printf("%4d\n", dmap[i].dmap_driver);
        
#if DEAD_CODE
        if (dmap[i].dmap_opcl == no_dev)  		printf("  no_dev");	
        else if (dmap[i].dmap_opcl == gen_opcl)		printf("gen_opcl");
        else 				printf("%8x", dmap[i].dmap_opcl);
        
        if ((void *)dmap[i].dmap_io == (void *)no_dev)	printf("  no_dev");
        else if (dmap[i].dmap_io == gen_io)		printf("  gen_io");
        else 				printf("%8x", dmap[i].dmap_io);
#endif

    }
}



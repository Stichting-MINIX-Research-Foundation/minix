/* This file contains the table with device <-> driver mappings. It also
 * contains some routines to dynamically add and/ or remove device drivers
 * or change mappings.  
 */

#include "fs.h"
#include "fproc.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <minix/com.h>
#include "param.h"

/* Some devices may or may not be there in the next table. */
#define DT(enable, opcl, io, driver, flags) \
  { (enable?(opcl):no_dev), (enable?(io):0), \
  	(enable?(driver):0), (flags) },
#define NC(x) (NR_CTRLRS >= (x))

/* The order of the entries here determines the mapping between major device
 * numbers and tasks.  The first entry (major device 0) is not used.  The
 * next entry is major device 1, etc.  Character and block devices can be
 * intermixed at random.  The ordering determines the device numbers in /dev/.
 * Note that FS knows the device number of /dev/ram/ to load the RAM disk.
 * Also note that the major device numbers used in /dev/ are NOT the same as 
 * the process numbers of the device drivers. 
 */
/*
  Driver enabled     Open/Cls  I/O     Driver #     Flags Device  File
  --------------     --------  ------  -----------  ----- ------  ----       
 */
struct dmap dmap[NR_DEVICES];				/* actual map */ 
PRIVATE struct dmap init_dmap[] = {
  DT(1, no_dev,   0,       0,       	0) 	  	/* 0 = not used   */
  DT(1, gen_opcl, gen_io,  MEM_PROC_NR, 0)	        /* 1 = /dev/mem   */
  DT(0, no_dev,   0,       0,           DMAP_MUTABLE)	/* 2 = /dev/fd0   */
  DT(0, no_dev,   0,       0,           DMAP_MUTABLE)	/* 3 = /dev/c0    */
  DT(1, tty_opcl, gen_io,  TTY_PROC_NR, 0)    	  	/* 4 = /dev/tty00 */
  DT(1, ctty_opcl,ctty_io, TTY_PROC_NR, 0)     	   	/* 5 = /dev/tty   */
  DT(0, no_dev,   0,       NONE,	DMAP_MUTABLE)	/* 6 = /dev/lp    */

#if (MACHINE == IBM_PC)
  DT(1, no_dev,   0,       0,   	DMAP_MUTABLE)   /* 7 = /dev/ip    */
  DT(0, no_dev,   0,       NONE,        DMAP_MUTABLE)   /* 8 = /dev/c1    */
  DT(0, 0,        0,       0,   	DMAP_MUTABLE)   /* 9 = not used   */
  DT(0, no_dev,   0,       0,           DMAP_MUTABLE)   /*10 = /dev/c2    */
  DT(0, 0,        0,       0,   	DMAP_MUTABLE)   /*11 = not used   */
  DT(0, no_dev,   0,       NONE,     	DMAP_MUTABLE)   /*12 = /dev/c3    */
  DT(0, no_dev,   0,       NONE,	DMAP_MUTABLE)   /*13 = /dev/audio */
  DT(0, no_dev,   0,       NONE,	DMAP_MUTABLE)   /*14 = /dev/mixer */
  DT(1, gen_opcl, gen_io,  LOG_PROC_NR, 0)  	        /*15 = /dev/klog  */
  DT(0, no_dev,   0,       NONE,	DMAP_MUTABLE)   /*16 = /dev/random*/
  DT(0, no_dev,   0,       NONE,	DMAP_MUTABLE)   /*17 = /dev/cmos  */
#endif /* IBM_PC */
};

/*===========================================================================*
 *				do_devctl		 		     *
 *===========================================================================*/
PUBLIC int do_devctl()
{
  int result, proc_nr_e, proc_nr_n;

  switch(m_in.ctl_req) {
  case DEV_MAP:
      /* Check process number of new driver. */
      proc_nr_e= m_in.driver_nr;
      if (isokendpt(proc_nr_e, &proc_nr_n) != OK)
	return(EINVAL);

      /* Try to update device mapping. */
      result = map_driver(m_in.dev_nr, proc_nr_e, m_in.dev_style);
      if (result == OK)
      {
	/* If a driver has completed its exec(), it can be announced to be
	 * up.
	 */
	if(fproc[proc_nr_n].fp_execced) {
		dev_up(m_in.dev_nr);
	} else {
		dmap[m_in.dev_nr].dmap_flags |= DMAP_BABY;
	}
      }
      break;
  case DEV_UNMAP:
      result = map_driver(m_in.dev_nr, NONE, 0);
      break;
  default:
      result = EINVAL;
  }
  return(result);
}

/*===========================================================================*
 *				map_driver		 		     *
 *===========================================================================*/
PUBLIC int map_driver(major, proc_nr_e, style)
int major;			/* major number of the device */
int proc_nr_e;			/* process number of the driver */
int style;			/* style of the device */
{
/* Set a new device driver mapping in the dmap table. Given that correct 
 * arguments are given, this only works if the entry is mutable and the 
 * current driver is not busy.  If the proc_nr is set to NONE, we're supposed
 * to unmap it.
 *
 * Normal error codes are returned so that this function can be used from
 * a system call that tries to dynamically install a new driver.
 */
  struct dmap *dp;
  int proc_nr_n;

  /* Get pointer to device entry in the dmap table. */
  if (major < 0 || major >= NR_DEVICES) return(ENODEV);
  dp = &dmap[major];		

  /* Check if we're supposed to unmap it. If so, do it even
   * if busy or unmutable, as unmap is called when driver has
   * exited.
   */
 if(proc_nr_e == NONE) {
	dp->dmap_opcl = no_dev;
	dp->dmap_io = no_dev_io;
	dp->dmap_driver = NONE;
	dp->dmap_flags = DMAP_MUTABLE;	/* When gone, not busy or reserved. */
	return(OK);
  }
	
  /* See if updating the entry is allowed. */
  if (! (dp->dmap_flags & DMAP_MUTABLE))  return(EPERM);
  if (dp->dmap_flags & DMAP_BUSY)  return(EBUSY);

  /* Check process number of new driver. */
  if (isokendpt(proc_nr_e, &proc_nr_n) != OK)
	return(EINVAL);

  /* Try to update the entry. */
  switch (style) {
  case STYLE_DEV:	dp->dmap_opcl = gen_opcl;	break;
  case STYLE_TTY:	dp->dmap_opcl = tty_opcl;	break;
  case STYLE_CLONE:	dp->dmap_opcl = clone_opcl;	break;
  default:		return(EINVAL);
  }
  dp->dmap_io = gen_io;
  dp->dmap_driver = proc_nr_e;

  return(OK); 
}

/*===========================================================================*
 *				dmap_unmap_by_endpt	 		     *
 *===========================================================================*/
PUBLIC void dmap_unmap_by_endpt(int proc_nr_e)
{
	int i, r;
	for (i=0; i<NR_DEVICES; i++)
	  if(dmap[i].dmap_driver && dmap[i].dmap_driver == proc_nr_e)
	    if((r=map_driver(i, NONE, 0)) != OK)
		printf("FS: unmap of p %d / d %d failed: %d\n", proc_nr_e,i,r);

	return;

}

/*===========================================================================*
 *				build_dmap		 		     *
 *===========================================================================*/
PUBLIC void build_dmap()
{
/* Initialize the table with all device <-> driver mappings. Then, map  
 * the boot driver to a controller and update the dmap table to that
 * selection. The boot driver and the controller it handles are set at 
 * the boot monitor.  
 */
  int i;
  struct dmap *dp;

  /* Build table with device <-> driver mappings. */
  for (i=0; i<NR_DEVICES; i++) {
      dp = &dmap[i];		
      if (i < sizeof(init_dmap)/sizeof(struct dmap) && 
              init_dmap[i].dmap_opcl != no_dev) {	/* a preset driver */
          dp->dmap_opcl = init_dmap[i].dmap_opcl;
          dp->dmap_io = init_dmap[i].dmap_io;
          dp->dmap_driver = init_dmap[i].dmap_driver;
          dp->dmap_flags = init_dmap[i].dmap_flags;
      } else {						/* no default */
          dp->dmap_opcl = no_dev;
          dp->dmap_io = no_dev_io;
          dp->dmap_driver = NONE;
          dp->dmap_flags = DMAP_MUTABLE;
      }
  }

#if 0
  /* Get settings of 'controller' and 'driver' at the boot monitor. */
  if ((s = env_get_param("label", driver, sizeof(driver))) != OK) 
      panic(__FILE__,"couldn't get boot monitor parameter 'driver'", s);
  if ((s = env_get_param("controller", controller, sizeof(controller))) != OK) 
      panic(__FILE__,"couldn't get boot monitor parameter 'controller'", s);

  /* Determine major number to map driver onto. */
  if (controller[0] == 'f' && controller[1] == 'd') {
      major = FLOPPY_MAJOR;
  } 
  else if (controller[0] == 'c' && isdigit(controller[1])) {
      if ((nr = (unsigned) atoi(&controller[1])) > NR_CTRLRS)
          panic(__FILE__,"monitor 'controller' maximum 'c#' is", NR_CTRLRS);
      major = CTRLR(nr);
  } 
  else {
      panic(__FILE__,"monitor 'controller' syntax is 'c#' of 'fd'", NO_NUM); 
  }
  
  /* Now try to set the actual mapping and report to the user. */
  if ((s=map_driver(major, DRVR_PROC_NR, STYLE_DEV)) != OK)
      panic(__FILE__,"map_driver failed",s);
  printf("Boot medium driver: %s driver mapped onto controller %s.\n",
      driver, controller);
#endif
}

/*===========================================================================*
 *				dmap_driver_match	 		     *
 *===========================================================================*/ 
PUBLIC int dmap_driver_match(int proc, int major)
{
	if (major < 0 || major >= NR_DEVICES) return(0);
	if(dmap[major].dmap_driver != NONE && dmap[major].dmap_driver == proc)
		return 1;
	return 0;
}

/*===========================================================================*
 *				dmap_endpt_up		 		     *
 *===========================================================================*/ 
PUBLIC void dmap_endpt_up(int proc_e)
{
	int i;
	for (i=0; i<NR_DEVICES; i++) {
		if(dmap[i].dmap_driver != NONE
			&& dmap[i].dmap_driver == proc_e
			&& (dmap[i].dmap_flags & DMAP_BABY)) {
			dmap[i].dmap_flags &= ~DMAP_BABY;
			dev_up(i);
		}
	}
	return;
}

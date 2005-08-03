/* This file contains the table with device <-> driver mappings. It also
 * contains some routines to dynamically add and/ or remove device drivers
 * or change mappings.  
 */

#include "fs.h"
#include "fproc.h"
#include <string.h>
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
struct dmap dmap[NR_DEVICES] = {
  DT(1,     no_dev,   0,      0,       	   0) 	  	  /* 0 = not used   */
  DT(1,     gen_opcl, gen_io, MEM_PROC_NR, 0)	          /* 1 = /dev/mem   */
  DT(0,     gen_opcl, gen_io, NONE,        DMAP_MUTABLE)  /* 2 = /dev/fd0   */
  DT(NC(1), gen_opcl, gen_io, CTRLR(0),    DMAP_MUTABLE)  /* 3 = /dev/c0    */
  DT(1,     tty_opcl, gen_io, TTY_PROC_NR, 0)    	  /* 4 = /dev/tty00 */
  DT(1,     ctty_opcl,ctty_io,TTY_PROC_NR, 0)     	  /* 5 = /dev/tty   */
  DT(0,     gen_opcl, gen_io, NONE,	   DMAP_MUTABLE)  /* 6 = /dev/lp    */

#if (MACHINE == IBM_PC)
  DT(1,     no_dev,   0,      0,   	   DMAP_MUTABLE)  /* 7 = /dev/ip    */
  DT(NC(2), gen_opcl, gen_io, CTRLR(1),    DMAP_MUTABLE)  /* 8 = /dev/c1    */
  DT(0,     0,        0,      0,   	   DMAP_MUTABLE)  /* 9 = not used   */
  DT(NC(3), gen_opcl, gen_io, CTRLR(2),    DMAP_MUTABLE)  /*10 = /dev/c2    */
  DT(0,     0,        0,      0,   	   DMAP_MUTABLE)  /*11 = not used   */
  DT(NC(4), gen_opcl, gen_io, CTRLR(3),    DMAP_MUTABLE)  /*12 = /dev/c3    */
  DT(0,     gen_opcl, gen_io, NONE,	   DMAP_MUTABLE)  /*13 = /dev/audio */
  DT(0,     gen_opcl, gen_io, NONE,	   DMAP_MUTABLE)  /*14 = /dev/mixer */
  DT(1,     gen_opcl, gen_io, LOG_PROC_NR, 0)       	  /*15 = /dev/klog  */
  DT(0,     gen_opcl, gen_io, NONE,	   DMAP_MUTABLE)  /*16 = /dev/random */
#endif /* IBM_PC */
};


/*===========================================================================*
 *				do_devctl		 		     *
 *===========================================================================*/
PUBLIC int do_devctl()
{
  int result;


  switch(m_in.ctl_req) {
  case DEV_MAP:
      /* Try to update device mapping. */
      result = map_driver(m_in.dev_nr, m_in.driver_nr, m_in.dev_style);
      break;
  case DEV_UNMAP:
      result = ENOSYS;
      break;
  default:
      result = EINVAL;
  }
  return(result);
}


/*===========================================================================*
 *				map_driver		 		     *
 *===========================================================================*/
PUBLIC int map_driver(major, proc_nr, style)
int major;			/* major number of the device */
int proc_nr;			/* process number of the driver */
int style;			/* style of the device */
{
/* Set a new device driver mapping in the dmap table. Given that correct 
 * arguments are given, this only works if the entry is mutable and the 
 * current driver is not busy. 
 * Normal error codes are returned so that this function can be used from
 * a system call that tries to dynamically install a new driver.
 */
  struct dmap *dp;

  /* Get pointer to device entry in the dmap table. */
  if (major >= NR_DEVICES) return(ENODEV);
  dp = &dmap[major];		
	
  /* See if updating the entry is allowed. */
  if (! (dp->dmap_flags & DMAP_MUTABLE))  return(EPERM);
  if (dp->dmap_flags & DMAP_BUSY)  return(EBUSY);

  /* Check process number of new driver. */
  if (! isokprocnr(proc_nr))  return(EINVAL);

  /* Try to update the entry. */
  switch (style) {
  case STYLE_DEV:	dp->dmap_opcl = gen_opcl;	break;
  case STYLE_TTY:	dp->dmap_opcl = tty_opcl;	break;
  case STYLE_CLONE:	dp->dmap_opcl = clone_opcl;	break;
  default:		return(EINVAL);
  }
  dp->dmap_io = gen_io;
  dp->dmap_driver = proc_nr;
  return(OK); 
}

/*===========================================================================*
 *				map_controllers		 		     *
 *===========================================================================*/
PUBLIC void map_controllers()
{
/* Map drivers to controllers and update the dmap table to that selection. 
 * For each controller, the environment variable set by the boot monitor is
 * analyzed to see what type of Winchester disk is attached. 
 * Finally, the process number of the driver is looked up, and, if found, is
 * installed in the dmap table.  
 */
  static char ctrlr_nr[] = "c0";	/* controller currently analyzed */
  char ctrlr_type[8];			/* type of Winchester disk */
  int i, c, s; 
  int proc_nr=0;			/* process number of driver */
  struct drivertab *dp;
  struct drivertab {
      char wini_type[8];
      char proc_name[8];
  } drivertab[] = {
	{ "at",		"boot"	},	/* AT Winchester */
	{ "bios",	"bios" },	/* BIOS Winchester */
	{ "esdi",	"..." },
	{ "xt",		"..." },
	{ "aha1540",	"..." },
	{ "dosfile",	"..." },
	{ "fatfile",	"..." },
  };

  for (c=0; c < NR_CTRLRS; c++) {

    /* See if there is a mapping for this controller. */
    ctrlr_nr[1] = '0' + c;
    if ((s = get_mon_param(ctrlr_nr, ctrlr_type, 8)) != OK)  {
    	 if (s != ESRCH) panic(__FILE__,"couldn't get monitor param", s);
         continue;
    }

    /* If there is a mapping, look up the driver with the given name. */
    for (dp = drivertab;
        dp < drivertab + sizeof(drivertab)/sizeof(drivertab[0]); dp++)  {
      if (strcmp(ctrlr_type, dp->wini_type) == 0) {	/* found driver name */
	if ((s=findproc(dp->proc_name, &proc_nr)) == OK) {
	  for (i=0; i< NR_DEVICES; i++) {		/* find mapping */
	    if (dmap[i].dmap_driver == CTRLR(c)) {  
	      if ((s=map_driver(i, proc_nr, STYLE_DEV)) != OK) {
	          panic(__FILE__,"map_driver failed",s);
	      }
#if VERBOSE
	      printf("FS: controller %s (%s) mapped to %s driver (nr %d)\n",
	    	  ctrlr_nr, dp->wini_type, dp->proc_name, dmap[i].dmap_driver);
#endif
	    }
	  }
	}
      }
    }
  }
}




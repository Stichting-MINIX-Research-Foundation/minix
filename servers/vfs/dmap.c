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
#include <minix/ds.h>
#include "param.h"

#define NC(x) (NR_CTRLRS >= (x))

/* The order of the entries in the table determines the mapping between major
 * device numbers and device drivers. Character and block devices
 * can be intermixed at random.  The ordering determines the device numbers in
 * /dev. Note that the major device numbers used in /dev are NOT the same as 
 * the process numbers of the device drivers. See <minix/dmap.h> for mappings.
 */

struct dmap dmap[NR_DEVICES];

#define DT_EMPTY { no_dev, no_dev_io, NONE, "", 0, STYLE_NDEV, NULL }

/*===========================================================================*
 *				do_mapdriver		 		     *
 *===========================================================================*/
PUBLIC int do_mapdriver()
{
	int r, flags, major;
	endpoint_t endpoint;
	vir_bytes label_vir;
	size_t label_len;
	char label[LABEL_MAX];

	/* Only RS can map drivers. */
	if (who_e != RS_PROC_NR)
	{
		printf("vfs: unauthorized call of do_mapdriver by proc %d\n",
			who_e);
		return(EPERM);
	}

	/* Get the label */
	label_vir= (vir_bytes)m_in.md_label;
	label_len= m_in.md_label_len;

	if (label_len+1 > sizeof(label))
	{
		printf("vfs:do_mapdriver: label too long\n");
		return EINVAL;
	}

	r= sys_vircopy(who_e, D, label_vir, SELF, D, (vir_bytes)label,
		label_len);
	if (r != OK)
	{
		printf("vfs:do_mapdriver: sys_vircopy failed: %d\n", r);
		return EINVAL;
	}

	label[label_len]= '\0';

	r= ds_retrieve_label_endpt(label, &endpoint);
	if (r != OK)
	{
		printf("vfs:do_mapdriver: ds doesn't know '%s'\n", label);
		return EINVAL;
	}

	/* Try to update device mapping. */
	major= m_in.md_major;
	flags= m_in.md_flags;
	r= map_driver(label, major, endpoint, m_in.md_style, flags);

	return(r);
}

/*===========================================================================*
 *				map_driver		 		     *
 *===========================================================================*/
PUBLIC int map_driver(label, major, proc_nr_e, style, flags)
const char *label;		/* name of the driver */
int major;			/* major number of the device */
endpoint_t proc_nr_e;		/* process number of the driver */
int style;			/* style of the device */
int flags;			/* device flags */
{
/* Set a new device driver mapping in the dmap table.
 * If the proc_nr is set to NONE, we're supposed to unmap it.
 */
  int proc_nr_n;
  size_t len;
  struct dmap *dp;

  /* Get pointer to device entry in the dmap table. */
  if (major < 0 || major >= NR_DEVICES) return(ENODEV);
  dp = &dmap[major];		

  /* Check if we're supposed to unmap it. */
 if(proc_nr_e == NONE) {
	dp->dmap_opcl = no_dev;
	dp->dmap_io = no_dev_io;
	dp->dmap_driver = NONE;
	dp->dmap_flags = flags;
	return(OK);
  }

  /* Check process number of new driver if requested. */
  if (! (flags & DRV_FORCED))
  {
	if (isokendpt(proc_nr_e, &proc_nr_n) != OK)
		return(EINVAL);
  }

  if (label != NULL) {
	len= strlen(label);
	if (len+1 > sizeof(dp->dmap_label))
		panic("map_driver: label too long: %d", len);
	strcpy(dp->dmap_label, label);
  }

  /* Try to update the entry. */
  switch (style) {
  case STYLE_DEV:
	dp->dmap_opcl = gen_opcl;
	dp->dmap_io = gen_io;
	break;
  case STYLE_DEVA:
	dp->dmap_opcl = gen_opcl;
	dp->dmap_io = asyn_io;
	break;
  case STYLE_TTY:
	dp->dmap_opcl = tty_opcl;
	dp->dmap_io = gen_io;
	break;
  case STYLE_CTTY:
	dp->dmap_opcl = ctty_opcl;
	dp->dmap_io = ctty_io;
	break;
  case STYLE_CLONE:
	dp->dmap_opcl = clone_opcl;
	dp->dmap_io = gen_io;
	break;
  default:
	return(EINVAL);
  }
  dp->dmap_driver = proc_nr_e;
  dp->dmap_flags = flags;
  dp->dmap_style = style;

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
	    if((r=map_driver(NULL, i, NONE, 0, 0)) != OK)
		printf("FS: unmap of p %d / d %d failed: %d\n", proc_nr_e,i,r);

	return;

}

/*===========================================================================*
 *		               map_service                                   *
 *===========================================================================*/
PUBLIC int map_service(struct rprocpub *rpub)
{
/* Map a new service by storing its device driver properties. */
  int r;

  /* Not a driver, nothing more to do. */
  if(!rpub->dev_nr) {
	return OK;
  }

  /* Map driver. */
  r = map_driver(rpub->label, rpub->dev_nr, rpub->endpoint,
	rpub->dev_style, rpub->dev_flags);
  if(r != OK) {
	return r;
  }

  /* If driver has two major numbers associated, also map the other one. */
  if(rpub->dev_style2 != STYLE_NDEV) {
	r = map_driver(rpub->label, rpub->dev_nr+1, rpub->endpoint,
		rpub->dev_style2, rpub->dev_flags);
	if(r != OK) {
		return r;
	}
  }

  return OK;
}

/*===========================================================================*
 *				build_dmap		 		     *
 *===========================================================================*/
PUBLIC void build_dmap()
{
/* Initialize the table with empty device <-> driver mappings. */
  int i;
  struct dmap dmap_default = DT_EMPTY;

  for (i=0; i<NR_DEVICES; i++) {
	dmap[i] = dmap_default;
  }
}

/*===========================================================================*
 *				dmap_driver_match	 		     *
 *===========================================================================*/ 
PUBLIC int dmap_driver_match(endpoint_t proc, int major)
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
			&& dmap[i].dmap_driver == proc_e) {
			dev_up(i);
		}
	}
	return;
}

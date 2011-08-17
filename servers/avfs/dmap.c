/* This file contains the table with device <-> driver mappings. It also
 * contains some routines to dynamically add and/ or remove device drivers
 * or change mappings.
 */

#include "fs.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <minix/com.h>
#include <minix/ds.h>
#include "fproc.h"
#include "dmap.h"
#include "param.h"

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
/* Create a device->driver mapping. RS will tell us which major is driven by
 * this driver, what type of device it is (regular, TTY, asynchronous, clone,
 * etc), and its label. This label is registered with DS, and allows us to
 * retrieve the driver's endpoint.
 */
  int r, flags, major;
  endpoint_t endpoint;
  vir_bytes label_vir;
  size_t label_len;
  char label[LABEL_MAX];

  /* Only RS can map drivers. */
  if (who_e != RS_PROC_NR) return(EPERM);

  /* Get the label */
  label_vir = (vir_bytes) m_in.md_label;
  label_len = (size_t) m_in.md_label_len;

  if (label_len+1 > sizeof(label)) { /* Can we store this label? */
	printf("VFS: do_mapdriver: label too long\n");
	return(EINVAL);
  }
  r = sys_vircopy(who_e, D, label_vir, SELF, D, (vir_bytes) label, label_len);
  if (r != OK) {
	printf("VFS: do_mapdriver: sys_vircopy failed: %d\n", r);
	return(EINVAL);
  }
  label[label_len] = '\0';	/* Terminate label */

  /* Now we know how the driver is called, fetch its endpoint */
  r = ds_retrieve_label_endpt(label, &endpoint);
  if (r != OK) {
	printf("VFS: do_mapdriver: label '%s' unknown\n", label);
	return(EINVAL);
  }

  /* Try to update device mapping. */
  major = m_in.md_major;
  flags = m_in.md_flags;

  return map_driver(label, major, endpoint, m_in.md_style, flags);
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
/* Add a new device driver mapping in the dmap table. If the proc_nr is set to
 * NONE, we're supposed to unmap it.
 */

  int slot;
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

  /* Check process number of new driver if it was alive before mapping */
  if (! (flags & DRV_FORCED)) {
	if (isokendpt(proc_nr_e, &slot) != OK)
		return(EINVAL);
  }

  if (label != NULL) {
	len = strlen(label);
	if (len+1 > sizeof(dp->dmap_label))
		panic("VFS: map_driver: label too long: %d", len);
	strcpy(dp->dmap_label, label);
  }

  /* Store driver I/O routines based on type of device */
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
    case STYLE_CLONE_A:
	dp->dmap_opcl = clone_opcl;
	dp->dmap_io = asyn_io;
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
PUBLIC void dmap_unmap_by_endpt(endpoint_t proc_e)
{
/* Lookup driver in dmap table by endpoint and unmap it */
  int major, r;

  for (major = 0; major < NR_DEVICES; major++) {
	if (dmap_driver_match(proc_e, major)) {
		/* Found driver; overwrite it with a NULL entry */
		if ((r = map_driver(NULL, major, NONE, 0, 0)) != OK) {
			printf("VFS: unmapping driver %d for major %d failed:"
				" %d\n", proc_e, major, r);
		}
	}
  }
}

/*===========================================================================*
 *		               map_service                                   *
 *===========================================================================*/
PUBLIC int map_service(struct rprocpub *rpub)
{
/* Map a new service by storing its device driver properties. */
  int r;

  /* Not a driver, nothing more to do. */
  if(rpub->dev_nr == NO_DEV) return(OK);

  /* Map driver. */
  r = map_driver(rpub->label, rpub->dev_nr, rpub->endpoint, rpub->dev_style,
		 rpub->dev_flags);
  if(r != OK) return(r);

  /* If driver has two major numbers associated, also map the other one. */
  if(rpub->dev_style2 != STYLE_NDEV) {
	r = map_driver(rpub->label, rpub->dev_nr+1, rpub->endpoint,
		       rpub->dev_style2, rpub->dev_flags);
	if(r != OK) return(r);
  }

  return(OK);
}

/*===========================================================================*
 *				init_dmap		 		     *
 *===========================================================================*/
PUBLIC void init_dmap()
{
/* Initialize the table with empty device <-> driver mappings. */
  int i;
  struct dmap dmap_default = DT_EMPTY;

  for (i = 0; i < NR_DEVICES; i++)
	dmap[i] = dmap_default;
}

/*===========================================================================*
 *				dmap_driver_match	 		     *
 *===========================================================================*/
PUBLIC int dmap_driver_match(endpoint_t proc, int major)
{
  if (major < 0 || major >= NR_DEVICES) return(0);
  if (dmap[major].dmap_driver != NONE && dmap[major].dmap_driver == proc)
	return(1);

  return(0);
}

/*===========================================================================*
 *				dmap_endpt_up		 		     *
 *===========================================================================*/
PUBLIC void dmap_endpt_up(endpoint_t proc_e)
{
/* A device driver with endpoint proc_e has been restarted. Go tell everyone
 * that might be blocking on it that this device is 'up'.
 */

  int major;
  for (major = 0; major < NR_DEVICES; major++)
	if (dmap_driver_match(proc_e, major))
		dev_up(major);

}

/*===========================================================================*
 *				get_dmap		 		     *
 *===========================================================================*/
PUBLIC struct dmap *get_dmap(endpoint_t proc_e)
{
/* See if 'proc_e' endpoint belongs to a valid dmap entry. If so, return a
 * pointer */

  int major;
  for (major = 0; major < NR_DEVICES; major++)
	if (dmap_driver_match(proc_e, major))
		return(&dmap[major]);

  return(NULL);
}

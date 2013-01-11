/* This file contains the table with device <-> driver mappings. It also
 * contains some routines to dynamically add and/ or remove device drivers
 * or change mappings.
 */

#include "fs.h"
#include <assert.h>
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

#define DT_EMPTY { no_dev, no_dev_io, NONE, "", 0, STYLE_NDEV, NULL, NONE, \
		   0, NULL, 0}

/*===========================================================================*
 *				lock_dmap		 		     *
 *===========================================================================*/
void lock_dmap(struct dmap *dp)
{
/* Lock a driver */
	struct worker_thread *org_self;
	struct fproc *org_fp;
	int r;

	assert(dp != NULL);
	assert(dp->dmap_driver != NONE);

	org_fp = fp;
	org_self = self;

	if ((r = mutex_lock(dp->dmap_lock_ref)) != 0)
		panic("unable to get a lock on dmap: %d\n", r);

	fp = org_fp;
	self = org_self;
}

/*===========================================================================*
 *				unlock_dmap		 		     *
 *===========================================================================*/
void unlock_dmap(struct dmap *dp)
{
/* Unlock a driver */
	int r;

	assert(dp != NULL);

	if ((r = mutex_unlock(dp->dmap_lock_ref)) != 0)
		panic("unable to unlock dmap lock: %d\n", r);
}

/*===========================================================================*
 *				do_mapdriver		 		     *
 *===========================================================================*/
int do_mapdriver()
{
/* Create a device->driver mapping. RS will tell us which major is driven by
 * this driver, what type of device it is (regular, TTY, asynchronous, clone,
 * etc), and its label. This label is registered with DS, and allows us to
 * retrieve the driver's endpoint.
 */
  int r, flags, major, style, slot;
  endpoint_t endpoint;
  vir_bytes label_vir;
  size_t label_len;
  char label[LABEL_MAX];
  struct fproc *rfp;

  /* Only RS can map drivers. */
  if (who_e != RS_PROC_NR) return(EPERM);

  label_vir = (vir_bytes) job_m_in.md_label;
  label_len = (size_t) job_m_in.md_label_len;
  major = job_m_in.md_major;
  flags = job_m_in.md_flags;
  style = job_m_in.md_style;

  /* Get the label */
  if (label_len+1 > sizeof(label)) { /* Can we store this label? */
	printf("VFS: do_mapdriver: label too long\n");
	return(EINVAL);
  }
  r = sys_vircopy(who_e, label_vir, SELF, (vir_bytes) label, label_len);
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

  /* Process is a service */
  if (isokendpt(endpoint, &slot) != OK) {
	printf("VFS: can't map driver to unknown endpoint %d\n", endpoint);
	return(EINVAL);
  }
  rfp = &fproc[slot];
  rfp->fp_flags |= FP_SRV_PROC;

  /* Try to update device mapping. */
  return map_driver(label, major, endpoint, style, flags);
}

/*===========================================================================*
 *				map_driver		 		     *
 *===========================================================================*/
int map_driver(label, major, proc_nr_e, style, flags)
const char label[LABEL_MAX];	/* name of the driver */
int major;			/* major number of the device */
endpoint_t proc_nr_e;		/* process number of the driver */
int style;			/* style of the device */
int flags;			/* device flags */
{
/* Add a new device driver mapping in the dmap table. If the proc_nr is set to
 * NONE, we're supposed to unmap it.
 */

  int slot, s;
  size_t len;
  struct dmap *dp;

  /* Get pointer to device entry in the dmap table. */
  if (major < 0 || major >= NR_DEVICES) return(ENODEV);
  dp = &dmap[major];

  /* Check if we're supposed to unmap it. */
 if (proc_nr_e == NONE) {
	/* Even when a driver is now unmapped and is shortly to be mapped in
	 * due to recovery, invalidate associated filps if they're character
	 * special files. More sophisticated recovery mechanisms which would
	 * reduce the need to invalidate files are possible, but would require
	 * cooperation of the driver and more recovery framework between RS,
	 * VFS, and DS.
	 */
	invalidate_filp_by_char_major(major);
	dp->dmap_opcl = no_dev;
	dp->dmap_io = no_dev_io;
	dp->dmap_driver = NONE;
	dp->dmap_flags = flags;
	dp->dmap_lock_ref = &dp->dmap_lock;
	return(OK);
  }

  /* Check process number of new driver if it was alive before mapping */
  s = isokendpt(proc_nr_e, &slot);
  if (s != OK) {
	/* This is not a problem only when we force this driver mapping */
	if (! (flags & DRV_FORCED))
		return(EINVAL);
  }

  if (label != NULL) {
	len = strlen(label);
	if (len+1 > sizeof(dp->dmap_label))
		panic("VFS: map_driver: label too long: %d", len);
	strlcpy(dp->dmap_label, label, LABEL_MAX);
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
void dmap_unmap_by_endpt(endpoint_t proc_e)
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
 *		               map_service				     *
 *===========================================================================*/
int map_service(struct rprocpub *rpub)
{
/* Map a new service by storing its device driver properties. */
  int r, slot;
  struct dmap *fdp, *sdp;
  struct fproc *rfp;

  /* Process is a service */
  if (isokendpt(rpub->endpoint, &slot) != OK) {
	printf("VFS: can't map service with unknown endpoint %d\n",
		rpub->endpoint);
	return(EINVAL);
  }
  rfp = &fproc[slot];
  rfp->fp_flags |= FP_SRV_PROC;

  /* Not a driver, nothing more to do. */
  if (rpub->dev_nr == NO_DEV) return(OK);

  /* Map driver. */
  r = map_driver(rpub->label, rpub->dev_nr, rpub->endpoint, rpub->dev_style,
		 rpub->dev_flags);
  if(r != OK) return(r);

  /* If driver has two major numbers associated, also map the other one. */
  if(rpub->dev_style2 != STYLE_NDEV) {
	r = map_driver(rpub->label, rpub->dev_nr+1, rpub->endpoint,
		       rpub->dev_style2, rpub->dev_flags);
	if(r != OK) return(r);

	/* To ensure that future dmap lock attempts always lock the same driver
	 * regardless of major number, refer the second dmap lock reference
	 * to the first dmap entry.
	 */
	fdp = get_dmap_by_major(rpub->dev_nr);
	sdp = get_dmap_by_major(rpub->dev_nr+1);
	assert(fdp != NULL);
	assert(sdp != NULL);
	assert(fdp != sdp);
	sdp->dmap_lock_ref = &fdp->dmap_lock;
  }

  return(OK);
}

/*===========================================================================*
 *				init_dmap		 		     *
 *===========================================================================*/
void init_dmap()
{
/* Initialize the table with empty device <-> driver mappings. */
  int i;
  struct dmap dmap_default = DT_EMPTY;

  for (i = 0; i < NR_DEVICES; i++)
	dmap[i] = dmap_default;
}

/*===========================================================================*
 *				init_dmap_locks		 		     *
 *===========================================================================*/
void init_dmap_locks()
{
  int i;

  for (i = 0; i < NR_DEVICES; i++) {
	if (mutex_init(&dmap[i].dmap_lock, NULL) != 0)
		panic("unable to initialize dmap lock");
	dmap[i].dmap_lock_ref = &dmap[i].dmap_lock;
  }
}

/*===========================================================================*
 *				dmap_driver_match	 		     *
 *===========================================================================*/
int dmap_driver_match(endpoint_t proc, int major)
{
  if (major < 0 || major >= NR_DEVICES) return(0);
  if (dmap[major].dmap_driver != NONE && dmap[major].dmap_driver == proc)
	return(1);

  return(0);
}

/*===========================================================================*
 *				dmap_by_major		 		     *
 *===========================================================================*/
struct dmap *
get_dmap_by_major(int major)
{
	if (major < 0 || major >= NR_DEVICES) return(NULL);
	if (dmap[major].dmap_driver == NONE) return(NULL);
	return(&dmap[major]);
}

/*===========================================================================*
 *				dmap_endpt_up		 		     *
 *===========================================================================*/
void dmap_endpt_up(endpoint_t proc_e, int is_blk)
{
/* A device driver with endpoint proc_e has been restarted. Go tell everyone
 * that might be blocking on it that this device is 'up'.
 */

  int major;
  struct dmap *dp;
  struct worker_thread *worker;

  if (proc_e == NONE) return;

  for (major = 0; major < NR_DEVICES; major++) {
	if ((dp = get_dmap_by_major(major)) == NULL) continue;
	if (dp->dmap_driver == proc_e) {
		if (is_blk) {
			if (dp->dmap_recovering) {
				printf("VFS: driver recovery failure for"
					" major %d\n", major);
				if (dp->dmap_servicing != NONE) {
					worker = worker_get(dp->dmap_servicing);
					worker_stop(worker);
				}
				dp->dmap_recovering = 0;
				continue;
			}
			dp->dmap_recovering = 1;
			bdev_up(major);
			dp->dmap_recovering = 0;
		} else {
			if (dp->dmap_servicing != NONE) {
				worker = worker_get(dp->dmap_servicing);
				worker_stop(worker);
			}
			cdev_up(major);
		}
	}
  }
}

/*===========================================================================*
 *				get_dmap		 		     *
 *===========================================================================*/
struct dmap *get_dmap(endpoint_t proc_e)
{
/* See if 'proc_e' endpoint belongs to a valid dmap entry. If so, return a
 * pointer */

  int major;
  for (major = 0; major < NR_DEVICES; major++)
	if (dmap_driver_match(proc_e, major))
		return(&dmap[major]);

  return(NULL);
}

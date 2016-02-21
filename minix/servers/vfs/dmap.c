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
#include <minix/callnr.h>
#include <minix/ds.h>

/* The order of the entries in the table determines the mapping between major
 * device numbers and device drivers. Character and block devices
 * can be intermixed at random.  The ordering determines the device numbers in
 * /dev. Note that the major device numbers used in /dev are NOT the same as
 * the process numbers of the device drivers. See <minix/dmap.h> for mappings.
 */

struct dmap dmap[NR_DEVICES];

/*===========================================================================*
 *				lock_dmap		 		     *
 *===========================================================================*/
void lock_dmap(struct dmap *dp)
{
/* Lock a driver */
	struct worker_thread *org_self;
	int r;

	assert(dp != NULL);
	assert(dp->dmap_driver != NONE);

	org_self = worker_suspend();

	if ((r = mutex_lock(&dp->dmap_lock)) != 0)
		panic("unable to get a lock on dmap: %d\n", r);

	worker_resume(org_self);
}

/*===========================================================================*
 *				unlock_dmap		 		     *
 *===========================================================================*/
void unlock_dmap(struct dmap *dp)
{
/* Unlock a driver */
	int r;

	assert(dp != NULL);

	if ((r = mutex_unlock(&dp->dmap_lock)) != 0)
		panic("unable to unlock dmap lock: %d\n", r);
}

/*===========================================================================*
 *				map_driver		 		     *
 *===========================================================================*/
static int map_driver(const char label[LABEL_MAX], devmajor_t major,
	endpoint_t proc_nr_e)
{
/* Add a new device driver mapping in the dmap table. If the proc_nr is set to
 * NONE, we're supposed to unmap it.
 */
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
	dp->dmap_driver = NONE;
	return(OK);
  }

  if (label != NULL) {
	len = strlen(label);
	if (len+1 > sizeof(dp->dmap_label)) {
		printf("VFS: map_driver: label too long: %zu\n", len);
		return(EINVAL);
	}
	strlcpy(dp->dmap_label, label, sizeof(dp->dmap_label));
  }

  /* Store driver I/O routines based on type of device */
  dp->dmap_driver = proc_nr_e;

  return(OK);
}

/*===========================================================================*
 *				do_mapdriver		 		     *
 *===========================================================================*/
int do_mapdriver(void)
{
/* Create a device->driver mapping. RS will tell us which major is driven by
 * this driver, what type of device it is (regular, TTY, asynchronous, clone,
 * etc), and its label. This label is registered with DS, and allows us to
 * retrieve the driver's endpoint.
 */
  const int *domains;
  int r, slot, ndomains;
  devmajor_t major;
  endpoint_t endpoint;
  vir_bytes label_vir;
  size_t label_len;
  char label[LABEL_MAX];
  struct fproc *rfp;

  /* Only RS can map drivers. */
  if (who_e != RS_PROC_NR) return(EPERM);

  label_vir = job_m_in.m_lsys_vfs_mapdriver.label;
  label_len = job_m_in.m_lsys_vfs_mapdriver.labellen;
  major = job_m_in.m_lsys_vfs_mapdriver.major;
  ndomains = job_m_in.m_lsys_vfs_mapdriver.ndomains;
  domains = job_m_in.m_lsys_vfs_mapdriver.domains;

  /* Get the label */
  if (label_len > sizeof(label)) { /* Can we store this label? */
	printf("VFS: do_mapdriver: label too long\n");
	return(EINVAL);
  }
  r = sys_vircopy(who_e, label_vir, SELF, (vir_bytes) label, label_len,
	CP_FLAG_TRY);
  if (r != OK) {
	printf("VFS: do_mapdriver: sys_vircopy failed: %d\n", r);
	return(EINVAL);
  }
  if (label[label_len-1] != '\0') {
	printf("VFS: do_mapdriver: label not null-terminated\n");
	return(EINVAL);
  }

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
  if (major != NO_DEV) {
	if ((r = map_driver(label, major, endpoint)) != OK)
		return r;
  }
  if (ndomains != 0) {
	if ((r = smap_map(label, endpoint, domains, ndomains)) != OK) {
		if (major != NO_DEV)
			map_driver(NULL, major, NONE); /* undo */
		return r;
	}
  }
  return OK;
}

/*===========================================================================*
 *				dmap_unmap_by_endpt	 		     *
 *===========================================================================*/
void dmap_unmap_by_endpt(endpoint_t proc_e)
{
/* Lookup driver in dmap table by endpoint and unmap it */
  devmajor_t major;
  int r;

  for (major = 0; major < NR_DEVICES; major++) {
	if (dmap_driver_match(proc_e, major)) {
		/* Found driver; overwrite it with a NULL entry */
		if ((r = map_driver(NULL, major, NONE)) != OK) {
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
  struct fproc *rfp;

  if (IS_RPUB_BOOT_USR(rpub)) return(OK);

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
  r = map_driver(rpub->label, rpub->dev_nr, rpub->endpoint);
  if(r != OK) return(r);

  return(OK);
}

/*===========================================================================*
 *				init_dmap		 		     *
 *===========================================================================*/
void init_dmap(void)
{
/* Initialize the device mapping table. */
  int i;

  memset(dmap, 0, sizeof(dmap));

  for (i = 0; i < NR_DEVICES; i++) {
	dmap[i].dmap_driver = NONE;
	dmap[i].dmap_servicing = INVALID_THREAD;
	if (mutex_init(&dmap[i].dmap_lock, NULL) != 0)
		panic("unable to initialize dmap lock");
  }

  /* CTTY_MAJOR is a special case, which is handled by VFS itself. */
  if (map_driver("vfs", CTTY_MAJOR, CTTY_ENDPT) != OK)
	panic("map_driver(CTTY_MAJOR) failed");
}

/*===========================================================================*
 *				dmap_driver_match	 		     *
 *===========================================================================*/
int dmap_driver_match(endpoint_t proc, devmajor_t major)
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
get_dmap_by_major(devmajor_t major)
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
  devmajor_t major;
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
				if (dp->dmap_servicing != INVALID_THREAD) {
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
			if (dp->dmap_servicing != INVALID_THREAD) {
				worker = worker_get(dp->dmap_servicing);
				worker_stop(worker);
			}
			invalidate_filp_by_char_major(major);
		}
	}
  }
}

/*===========================================================================*
 *				get_dmap		 		     *
 *===========================================================================*/
struct dmap *get_dmap_by_endpt(endpoint_t proc_e)
{
/* See if 'proc_e' endpoint belongs to a valid dmap entry. If so, return a
 * pointer */
  devmajor_t major;

  for (major = 0; major < NR_DEVICES; major++)
	if (dmap_driver_match(proc_e, major))
		return(&dmap[major]);

  return(NULL);
}

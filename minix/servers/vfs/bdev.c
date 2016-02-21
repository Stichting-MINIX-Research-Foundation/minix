/*
 * This file contains routines to perform certain block device operations.
 * These routines are called when a user application opens or closes a block
 * device node, or performs an ioctl(2) call on such an opened node.  Reading
 * and writing on an opened block device is routed through the file system
 * service that has mounted that block device, or the root file system service
 * if the block device is not mounted.  All block device operations by file
 * system services themselves are going directly to the block device, and not
 * through VFS.
 *
 * Block device drivers may not suspend operations for later processing, and
 * thus, block device operations simply block their calling thread for the
 * duration of the operation.
 *
 * The entry points in this file are:
 *   bdev_open:   open a block device
 *   bdev_close:  close a block device
 *   bdev_ioctl:  issue an I/O control request on a block device
 *   bdev_reply:  process the result of a block driver request
 *   bdev_up:     a block driver has been mapped in
 */

#include "fs.h"
#include "vnode.h"
#include "file.h"
#include <string.h>
#include <assert.h>

/*
 * Send a request to a block device, and suspend the current thread until a
 * reply from the driver comes in.
 */
static int
bdev_sendrec(endpoint_t driver_e, message * mess_ptr)
{
	int r, status, retry_count;
	message mess_retry;

	assert(IS_BDEV_RQ(mess_ptr->m_type));
	mess_retry = *mess_ptr;
	retry_count = 0;

	do {
		r = drv_sendrec(driver_e, mess_ptr);
		if (r != OK)
			return r;

		status = mess_ptr->m_lblockdriver_lbdev_reply.status;
		if (status == ERESTART) {
			r = EDEADEPT;
			*mess_ptr = mess_retry;
			retry_count++;
		}
	} while (status == ERESTART && retry_count < 5);

	/* If we failed to restart the request, return EIO. */
	if (status == ERESTART && retry_count >= 5)
		return EIO;

	if (r != OK) {
		if (r == EDEADSRCDST || r == EDEADEPT) {
			printf("VFS: dead driver %d\n", driver_e);
			dmap_unmap_by_endpt(driver_e);
			return EIO;
		} else if (r == ELOCKED) {
			printf("VFS: deadlock talking to %d\n", driver_e);
			return EIO;
		}
		panic("VFS: uncaught bdev_sendrec failure: %d", r);
	}

	return OK;
}

/*
 * Open a block device.
 */
int
bdev_open(dev_t dev, int bits)
{
	devmajor_t major_dev;
	devminor_t minor_dev;
	message dev_mess;
	int r, access;

	major_dev = major(dev);
	minor_dev = minor(dev);
	if (major_dev < 0 || major_dev >= NR_DEVICES) return ENXIO;
	if (dmap[major_dev].dmap_driver == NONE) return ENXIO;

	access = 0;
	if (bits & R_BIT) access |= BDEV_R_BIT;
	if (bits & W_BIT) access |= BDEV_W_BIT;

	/* Set up the message passed to the driver. */
	memset(&dev_mess, 0, sizeof(dev_mess));
	dev_mess.m_type = BDEV_OPEN;
	dev_mess.m_lbdev_lblockdriver_msg.minor = minor_dev;
	dev_mess.m_lbdev_lblockdriver_msg.access = access;
	dev_mess.m_lbdev_lblockdriver_msg.id = 0;

	/* Call the driver. */
	r = bdev_sendrec(dmap[major_dev].dmap_driver, &dev_mess);
	if (r != OK)
		return r;

	return dev_mess.m_lblockdriver_lbdev_reply.status;
}

/*
 * Close a block device.
 */
int
bdev_close(dev_t dev)
{
	devmajor_t major_dev;
	devminor_t minor_dev;
	message dev_mess;
	int r;

	major_dev = major(dev);
	minor_dev = minor(dev);
	if (major_dev < 0 || major_dev >= NR_DEVICES) return ENXIO;
	if (dmap[major_dev].dmap_driver == NONE) return ENXIO;

	/* Set up the message passed to the driver. */
	memset(&dev_mess, 0, sizeof(dev_mess));
	dev_mess.m_type = BDEV_CLOSE;
	dev_mess.m_lbdev_lblockdriver_msg.minor = minor_dev;
	dev_mess.m_lbdev_lblockdriver_msg.id = 0;

	/* Call the driver. */
	r = bdev_sendrec(dmap[major_dev].dmap_driver, &dev_mess);
	if (r != OK)
		return r;

	return dev_mess.m_lblockdriver_lbdev_reply.status;
}

/*
 * Perform an I/O control operation on a block device.
 */
int
bdev_ioctl(dev_t dev, endpoint_t proc_e, unsigned long req, vir_bytes buf)
{
	struct dmap *dp;
	cp_grant_id_t grant;
	message dev_mess;
	devmajor_t major_dev;
	devminor_t minor_dev;
	int r;

	major_dev = major(dev);
	minor_dev = minor(dev);

	/* Determine driver dmap. */
	dp = &dmap[major_dev];
	if (dp->dmap_driver == NONE) {
		printf("VFS: bdev_ioctl: no driver for major %d\n", major_dev);
		return ENXIO;
	}

	/* Set up a grant if necessary. */
	grant = make_ioctl_grant(dp->dmap_driver, proc_e, buf, req);

	/* Set up the message passed to the driver. */
	memset(&dev_mess, 0, sizeof(dev_mess));
	dev_mess.m_type = BDEV_IOCTL;
	dev_mess.m_lbdev_lblockdriver_msg.minor = minor_dev;
	dev_mess.m_lbdev_lblockdriver_msg.request = req;
	dev_mess.m_lbdev_lblockdriver_msg.grant = grant;
	dev_mess.m_lbdev_lblockdriver_msg.user = proc_e;
	dev_mess.m_lbdev_lblockdriver_msg.id = 0;

	/* Call the driver. */
	r = bdev_sendrec(dp->dmap_driver, &dev_mess);

	/* Clean up. */
	if (GRANT_VALID(grant)) cpf_revoke(grant);

	/* Return the result. */
	if (r != OK)
		return r;

	return dev_mess.m_lblockdriver_lbdev_reply.status;
}

/*
 * A block driver has results for a call.  There must be a thread waiting for
 * these results; wake it up.  This function MUST NOT block its calling thread.
 */
void
bdev_reply(void)
{
	struct worker_thread *wp;
	struct dmap *dp;

	if ((dp = get_dmap_by_endpt(who_e)) == NULL) {
		printf("VFS: ignoring block dev reply from unknown driver "
		    "%d\n", who_e);
		return;
	}

	if (dp->dmap_servicing == INVALID_THREAD) {
		printf("VFS: ignoring spurious block dev reply from %d\n",
		    who_e);
		return;
	}

	wp = worker_get(dp->dmap_servicing);
	if (wp == NULL || wp->w_task != who_e || wp->w_drv_sendrec == NULL) {
		printf("VFS: no worker thread waiting for a reply from %d\n",
		    who_e);
		return;
	}

	*wp->w_drv_sendrec = m_in;
	wp->w_drv_sendrec = NULL;
	worker_signal(wp);
}

/*
 * A new block device driver has been mapped in.  This may affect both mounted
 * file systems and open block-special files.
 */
void
bdev_up(devmajor_t maj)
{
	int r, found, bits;
	struct filp *rfilp;
	struct vmnt *vmp;
	struct vnode *vp;
	char *label;

	if (maj < 0 || maj >= NR_DEVICES) panic("VFS: out-of-bound major");
	label = dmap[maj].dmap_label;
	found = 0;

	/*
	 * For each block-special file that was previously opened on the
	 * affected device, we need to reopen it on the new driver.
	 */
	for (rfilp = filp; rfilp < &filp[NR_FILPS]; rfilp++) {
		if (rfilp->filp_count < 1) continue;
		if ((vp = rfilp->filp_vno) == NULL) continue;
		if (major(vp->v_sdev) != maj) continue;
		if (!S_ISBLK(vp->v_mode)) continue;

		/* Reopen the device on the driver, once per filp. */
		bits = rfilp->filp_mode & (R_BIT | W_BIT);
		if ((r = bdev_open(vp->v_sdev, bits)) != OK) {
			printf("VFS: mounted dev %d/%d re-open failed: %d\n",
			    maj, minor(vp->v_sdev), r);
			dmap[maj].dmap_recovering = 0;
			return; /* Give up entirely */
		}

		found = 1;
	}

	/* Tell each affected mounted file system about the new endpoint. */
	for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; ++vmp) {
		if (major(vmp->m_dev) != maj) continue;

		/* Send the driver label to the mounted file system. */
		if (req_newdriver(vmp->m_fs_e, vmp->m_dev, label) != OK)
			printf("VFS: error sending new driver label to %d\n",
			    vmp->m_fs_e);
	}

	/*
	 * If any block-special file was open for this major at all, also
	 * inform the root file system about the new driver.  We do this even
	 * if the block-special file is linked to another mounted file system,
	 * merely because it is more work to check for that case.
	 */
	if (found) {
		if (req_newdriver(ROOT_FS_E, makedev(maj, 0), label) != OK)
			printf("VFS: error sending new driver label to %d\n",
			    ROOT_FS_E);
	}
}

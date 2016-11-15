/* $NetBSD: ata_raid_subr.c,v 1.2 2010/06/24 13:03:08 hannken Exp $ */

/*-
 * Copyright (c) 2008 Juan Romero Pardines.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ata_raid_subr.c,v 1.2 2010/06/24 13:03:08 hannken Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/dkio.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/kauth.h>
#include <sys/kmem.h>

#include <dev/ata/ata_raidvar.h>

struct ataraid_disk_vnode {
	struct ataraid_disk_info *adv_adi;
	struct vnode *adv_vnode;
	SLIST_ENTRY(ataraid_disk_vnode) adv_next;
};

static SLIST_HEAD(, ataraid_disk_vnode) ataraid_disk_vnode_list =
    SLIST_HEAD_INITIALIZER(ataraid_disk_vnode_list);

/* 
 * Finds the RAW_PART vnode of the block device associated with a component
 * by looking at the singly linked list; otherwise creates, opens and
 * returns the vnode to the caller.
 */
struct vnode *
ata_raid_disk_vnode_find(struct ataraid_disk_info *adi)
{
	struct ataraid_disk_vnode *adv = NULL;
	struct vnode *vp = NULL;
	device_t devlist;
	int bmajor, error = 0;
	dev_t dev;

	SLIST_FOREACH(adv, &ataraid_disk_vnode_list, adv_next) {
		devlist = adv->adv_adi->adi_dev;
		if (strcmp(device_xname(devlist),
		    device_xname(adi->adi_dev)) == 0)
			return adv->adv_vnode;
	}

	adv = NULL;
	adv = kmem_zalloc(sizeof(struct ataraid_disk_vnode), KM_SLEEP);

	bmajor = devsw_name2blk(device_xname(adi->adi_dev), NULL, 0);
	dev = MAKEDISKDEV(bmajor, device_unit(adi->adi_dev), RAW_PART);

	error = bdevvp(dev, &vp);
	if (error) {
		kmem_free(adv, sizeof(struct ataraid_disk_vnode));
		return NULL;
	}
	error = VOP_OPEN(vp, FREAD|FWRITE, NOCRED);
	if (error) {
		vput(vp);
		kmem_free(adv, sizeof(struct ataraid_disk_vnode));
		return NULL;
	}
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	VOP_UNLOCK(vp);

	adv->adv_adi = adi;
	adv->adv_vnode = vp;

	SLIST_INSERT_HEAD(&ataraid_disk_vnode_list, adv, adv_next);

	return adv->adv_vnode;
}

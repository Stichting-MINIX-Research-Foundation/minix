/*	$NetBSD: rf_netbsdkintf.c,v 1.325 2015/08/20 14:40:18 christos Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1998, 2008-2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Greg Oster; Jason R. Thorpe.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: cd.c 1.6 90/11/28$
 *
 *      @(#)cd.c        8.2 (Berkeley) 11/16/93
 */

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Mark Holland, Jim Zelenka
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/***********************************************************
 *
 * rf_kintf.c -- the kernel interface routines for RAIDframe
 *
 ***********************************************************/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rf_netbsdkintf.c,v 1.325 2015/08/20 14:40:18 christos Exp $");

#ifdef _KERNEL_OPT
#include "opt_compat_netbsd.h"
#include "opt_raid_autoconfig.h"
#endif

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/disk.h>
#include <sys/device.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/reboot.h>
#include <sys/kauth.h>

#include <prop/proplib.h>

#include <dev/raidframe/raidframevar.h>
#include <dev/raidframe/raidframeio.h>
#include <dev/raidframe/rf_paritymap.h>

#include "rf_raid.h"
#include "rf_copyback.h"
#include "rf_dag.h"
#include "rf_dagflags.h"
#include "rf_desc.h"
#include "rf_diskqueue.h"
#include "rf_etimer.h"
#include "rf_general.h"
#include "rf_kintf.h"
#include "rf_options.h"
#include "rf_driver.h"
#include "rf_parityscan.h"
#include "rf_threadstuff.h"

#ifdef COMPAT_50
#include "rf_compat50.h"
#endif

#include "ioconf.h"

#ifdef DEBUG
int     rf_kdebug_level = 0;
#define db1_printf(a) if (rf_kdebug_level > 0) printf a
#else				/* DEBUG */
#define db1_printf(a) { }
#endif				/* DEBUG */

#if (RF_INCLUDE_PARITY_DECLUSTERING_DS > 0)
static rf_declare_mutex2(rf_sparet_wait_mutex);
static rf_declare_cond2(rf_sparet_wait_cv);
static rf_declare_cond2(rf_sparet_resp_cv);

static RF_SparetWait_t *rf_sparet_wait_queue;	/* requests to install a
						 * spare table */
static RF_SparetWait_t *rf_sparet_resp_queue;	/* responses from
						 * installation process */
#endif

MALLOC_DEFINE(M_RAIDFRAME, "RAIDframe", "RAIDframe structures");

/* prototypes */
static void KernelWakeupFunc(struct buf *);
static void InitBP(struct buf *, struct vnode *, unsigned,
    dev_t, RF_SectorNum_t, RF_SectorCount_t, void *, void (*) (struct buf *),
    void *, int, struct proc *);
struct raid_softc;
static void raidinit(struct raid_softc *);

static int raid_match(device_t, cfdata_t, void *);
static void raid_attach(device_t, device_t, void *);
static int raid_detach(device_t, int);

static int raidread_component_area(dev_t, struct vnode *, void *, size_t, 
    daddr_t, daddr_t);
static int raidwrite_component_area(dev_t, struct vnode *, void *, size_t,
    daddr_t, daddr_t, int);

static int raidwrite_component_label(unsigned,
    dev_t, struct vnode *, RF_ComponentLabel_t *);
static int raidread_component_label(unsigned,
    dev_t, struct vnode *, RF_ComponentLabel_t *);


static dev_type_open(raidopen);
static dev_type_close(raidclose);
static dev_type_read(raidread);
static dev_type_write(raidwrite);
static dev_type_ioctl(raidioctl);
static dev_type_strategy(raidstrategy);
static dev_type_dump(raiddump);
static dev_type_size(raidsize);

const struct bdevsw raid_bdevsw = {
	.d_open = raidopen,
	.d_close = raidclose,
	.d_strategy = raidstrategy,
	.d_ioctl = raidioctl,
	.d_dump = raiddump,
	.d_psize = raidsize,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

const struct cdevsw raid_cdevsw = {
	.d_open = raidopen,
	.d_close = raidclose,
	.d_read = raidread,
	.d_write = raidwrite,
	.d_ioctl = raidioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

static struct dkdriver rf_dkdriver = {
	.d_strategy = raidstrategy,
	.d_minphys = minphys
};

struct raid_softc {
	device_t sc_dev;
	int	sc_unit;
	int     sc_flags;	/* flags */
	int     sc_cflags;	/* configuration flags */
	uint64_t sc_size;	/* size of the raid device */
	char    sc_xname[20];	/* XXX external name */
	struct disk sc_dkdev;	/* generic disk device info */
	struct bufq_state *buf_queue;	/* used for the device queue */
	RF_Raid_t sc_r;
	LIST_ENTRY(raid_softc) sc_link;
};
/* sc_flags */
#define RAIDF_INITED	0x01	/* unit has been initialized */
#define RAIDF_WLABEL	0x02	/* label area is writable */
#define RAIDF_LABELLING	0x04	/* unit is currently being labelled */
#define RAIDF_SHUTDOWN	0x08	/* unit is being shutdown */
#define RAIDF_WANTED	0x40	/* someone is waiting to obtain a lock */
#define RAIDF_LOCKED	0x80	/* unit is locked */

#define	raidunit(x)	DISKUNIT(x)

extern struct cfdriver raid_cd;
CFATTACH_DECL3_NEW(raid, sizeof(struct raid_softc),
    raid_match, raid_attach, raid_detach, NULL, NULL, NULL,
    DVF_DETACH_SHUTDOWN);

/*
 * Allow RAIDOUTSTANDING number of simultaneous IO's to this RAID device.
 * Be aware that large numbers can allow the driver to consume a lot of
 * kernel memory, especially on writes, and in degraded mode reads.
 *
 * For example: with a stripe width of 64 blocks (32k) and 5 disks,
 * a single 64K write will typically require 64K for the old data,
 * 64K for the old parity, and 64K for the new parity, for a total
 * of 192K (if the parity buffer is not re-used immediately).
 * Even it if is used immediately, that's still 128K, which when multiplied
 * by say 10 requests, is 1280K, *on top* of the 640K of incoming data.
 *
 * Now in degraded mode, for example, a 64K read on the above setup may
 * require data reconstruction, which will require *all* of the 4 remaining
 * disks to participate -- 4 * 32K/disk == 128K again.
 */

#ifndef RAIDOUTSTANDING
#define RAIDOUTSTANDING   6
#endif

#define RAIDLABELDEV(dev)	\
	(MAKEDISKDEV(major((dev)), raidunit((dev)), RAW_PART))

/* declared here, and made public, for the benefit of KVM stuff.. */

static void raidgetdefaultlabel(RF_Raid_t *, struct raid_softc *,
				     struct disklabel *);
static void raidgetdisklabel(dev_t);
static void raidmakedisklabel(struct raid_softc *);

static int raidlock(struct raid_softc *);
static void raidunlock(struct raid_softc *);

static int raid_detach_unlocked(struct raid_softc *);

static void rf_markalldirty(RF_Raid_t *);
static void rf_set_geometry(struct raid_softc *, RF_Raid_t *);

void rf_ReconThread(struct rf_recon_req *);
void rf_RewriteParityThread(RF_Raid_t *raidPtr);
void rf_CopybackThread(RF_Raid_t *raidPtr);
void rf_ReconstructInPlaceThread(struct rf_recon_req *);
int rf_autoconfig(device_t);
void rf_buildroothack(RF_ConfigSet_t *);

RF_AutoConfig_t *rf_find_raid_components(void);
RF_ConfigSet_t *rf_create_auto_sets(RF_AutoConfig_t *);
static int rf_does_it_fit(RF_ConfigSet_t *,RF_AutoConfig_t *);
int rf_reasonable_label(RF_ComponentLabel_t *, uint64_t);
void rf_create_configuration(RF_AutoConfig_t *,RF_Config_t *, RF_Raid_t *);
int rf_set_autoconfig(RF_Raid_t *, int);
int rf_set_rootpartition(RF_Raid_t *, int);
void rf_release_all_vps(RF_ConfigSet_t *);
void rf_cleanup_config_set(RF_ConfigSet_t *);
int rf_have_enough_components(RF_ConfigSet_t *);
struct raid_softc *rf_auto_config_set(RF_ConfigSet_t *);
static void rf_fix_old_label_size(RF_ComponentLabel_t *, uint64_t);

/*
 * Debugging, mostly.  Set to 0 to not allow autoconfig to take place.
 * Note that this is overridden by having RAID_AUTOCONFIG as an option
 * in the kernel config file.
 */
#ifdef RAID_AUTOCONFIG
int raidautoconfig = 1;
#else
int raidautoconfig = 0;
#endif
static bool raidautoconfigdone = false;

struct RF_Pools_s rf_pools;

static LIST_HEAD(, raid_softc) raids = LIST_HEAD_INITIALIZER(raids);
static kmutex_t raid_lock;

static struct raid_softc *
raidcreate(int unit) {
	struct raid_softc *sc = kmem_zalloc(sizeof(*sc), KM_SLEEP);
	if (sc == NULL) {
#ifdef DIAGNOSTIC
		printf("%s: out of memory\n", __func__);
#endif
		return NULL;
	}
	sc->sc_unit = unit;
	bufq_alloc(&sc->buf_queue, "fcfs", BUFQ_SORT_RAWBLOCK);
	return sc;
}

static void
raiddestroy(struct raid_softc *sc) {
	bufq_free(sc->buf_queue);
	kmem_free(sc, sizeof(*sc));
}

static struct raid_softc *
raidget(int unit) {
	struct raid_softc *sc;
	if (unit < 0) {
#ifdef DIAGNOSTIC
		panic("%s: unit %d!", __func__, unit);
#endif
		return NULL;
	}
	mutex_enter(&raid_lock);
	LIST_FOREACH(sc, &raids, sc_link) {
		if (sc->sc_unit == unit) {
			mutex_exit(&raid_lock);
			return sc;
		}
	}
	mutex_exit(&raid_lock);
	if ((sc = raidcreate(unit)) == NULL)
		return NULL;
	mutex_enter(&raid_lock);
	LIST_INSERT_HEAD(&raids, sc, sc_link);
	mutex_exit(&raid_lock);
	return sc;
}

static void 
raidput(struct raid_softc *sc) {
	mutex_enter(&raid_lock);
	LIST_REMOVE(sc, sc_link);
	mutex_exit(&raid_lock);
	raiddestroy(sc);
}

void
raidattach(int num)
{
	mutex_init(&raid_lock, MUTEX_DEFAULT, IPL_NONE);
	/* This is where all the initialization stuff gets done. */

#if (RF_INCLUDE_PARITY_DECLUSTERING_DS > 0)
	rf_init_mutex2(rf_sparet_wait_mutex, IPL_VM);
	rf_init_cond2(rf_sparet_wait_cv, "sparetw");
	rf_init_cond2(rf_sparet_resp_cv, "rfgst");

	rf_sparet_wait_queue = rf_sparet_resp_queue = NULL;
#endif

	if (rf_BootRaidframe() == 0)
		aprint_verbose("Kernelized RAIDframe activated\n");
	else
		panic("Serious error booting RAID!!");

	if (config_cfattach_attach(raid_cd.cd_name, &raid_ca)) {
		aprint_error("raidattach: config_cfattach_attach failed?\n");
	}

	raidautoconfigdone = false;

	/*
	 * Register a finalizer which will be used to auto-config RAID
	 * sets once all real hardware devices have been found.
	 */
	if (config_finalize_register(NULL, rf_autoconfig) != 0)
		aprint_error("WARNING: unable to register RAIDframe finalizer\n");
}

int
rf_autoconfig(device_t self)
{
	RF_AutoConfig_t *ac_list;
	RF_ConfigSet_t *config_sets;

	if (!raidautoconfig || raidautoconfigdone == true)
		return (0);

	/* XXX This code can only be run once. */
	raidautoconfigdone = true;

#ifdef __HAVE_CPU_BOOTCONF
	/*
	 * 0. find the boot device if needed first so we can use it later
	 * this needs to be done before we autoconfigure any raid sets,
	 * because if we use wedges we are not going to be able to open
	 * the boot device later
	 */
	if (booted_device == NULL)
		cpu_bootconf();
#endif
	/* 1. locate all RAID components on the system */
	aprint_debug("Searching for RAID components...\n");
	ac_list = rf_find_raid_components();

	/* 2. Sort them into their respective sets. */
	config_sets = rf_create_auto_sets(ac_list);

	/*
	 * 3. Evaluate each set and configure the valid ones.
	 * This gets done in rf_buildroothack().
	 */
	rf_buildroothack(config_sets);

	return 1;
}

static int
rf_containsboot(RF_Raid_t *r, device_t bdv) {
	const char *bootname = device_xname(bdv);
	size_t len = strlen(bootname);

	for (int col = 0; col < r->numCol; col++) {
		const char *devname = r->Disks[col].devname;
		devname += sizeof("/dev/") - 1;
		if (strncmp(devname, "dk", 2) == 0) {
			const char *parent =
			    dkwedge_get_parent_name(r->Disks[col].dev);
			if (parent != NULL)
				devname = parent;
		}
		if (strncmp(devname, bootname, len) == 0) {
			struct raid_softc *sc = r->softc;
			aprint_debug("raid%d includes boot device %s\n",
			    sc->sc_unit, devname);
			return 1;
		}
	}
	return 0;
}

void
rf_buildroothack(RF_ConfigSet_t *config_sets)
{
	RF_ConfigSet_t *cset;
	RF_ConfigSet_t *next_cset;
	int num_root;
	struct raid_softc *sc, *rsc;

	sc = rsc = NULL;
	num_root = 0;
	cset = config_sets;
	while (cset != NULL) {
		next_cset = cset->next;
		if (rf_have_enough_components(cset) &&
		    cset->ac->clabel->autoconfigure == 1) {
			sc = rf_auto_config_set(cset);
			if (sc != NULL) {
				aprint_debug("raid%d: configured ok\n",
				    sc->sc_unit);
				if (cset->rootable) {
					rsc = sc;
					num_root++;
				}
			} else {
				/* The autoconfig didn't work :( */
				aprint_debug("Autoconfig failed\n");
				rf_release_all_vps(cset);
			}
		} else {
			/* we're not autoconfiguring this set...
			   release the associated resources */
			rf_release_all_vps(cset);
		}
		/* cleanup */
		rf_cleanup_config_set(cset);
		cset = next_cset;
	}

	/* if the user has specified what the root device should be
	   then we don't touch booted_device or boothowto... */

	if (rootspec != NULL)
		return;

	/* we found something bootable... */

	/*
	 * XXX: The following code assumes that the root raid
	 * is the first ('a') partition. This is about the best
	 * we can do with a BSD disklabel, but we might be able
	 * to do better with a GPT label, by setting a specified
	 * attribute to indicate the root partition. We can then
	 * stash the partition number in the r->root_partition
	 * high bits (the bottom 2 bits are already used). For
	 * now we just set booted_partition to 0 when we override
	 * root.
	 */
	if (num_root == 1) {
		device_t candidate_root;
		if (rsc->sc_dkdev.dk_nwedges != 0) {
			char cname[sizeof(cset->ac->devname)];
			/* XXX: assume 'a' */
			snprintf(cname, sizeof(cname), "%s%c",
			    device_xname(rsc->sc_dev), 'a');
			candidate_root = dkwedge_find_by_wname(cname);
		} else
			candidate_root = rsc->sc_dev;
		if (booted_device == NULL ||
		    rsc->sc_r.root_partition == 1 ||
		    rf_containsboot(&rsc->sc_r, booted_device)) {
			booted_device = candidate_root;
			booted_partition = 0;	/* XXX assume 'a' */
		}
	} else if (num_root > 1) {

		/* 
		 * Maybe the MD code can help. If it cannot, then
		 * setroot() will discover that we have no
		 * booted_device and will ask the user if nothing was
		 * hardwired in the kernel config file 
		 */
		if (booted_device == NULL) 
			return;

		num_root = 0;
		mutex_enter(&raid_lock);
		LIST_FOREACH(sc, &raids, sc_link) {
			RF_Raid_t *r = &sc->sc_r;
			if (r->valid == 0)
				continue;

			if (r->root_partition == 0)
				continue;

			if (rf_containsboot(r, booted_device)) {
				num_root++;
				rsc = sc;
			}
		}
		mutex_exit(&raid_lock);

		if (num_root == 1) {
			booted_device = rsc->sc_dev;
			booted_partition = 0;	/* XXX assume 'a' */
		} else {
			/* we can't guess.. require the user to answer... */
			boothowto |= RB_ASKNAME;
		}
	}
}

static int
raidsize(dev_t dev)
{
	struct raid_softc *rs;
	struct disklabel *lp;
	int     part, unit, omask, size;

	unit = raidunit(dev);
	if ((rs = raidget(unit)) == NULL)
		return -1;
	if ((rs->sc_flags & RAIDF_INITED) == 0)
		return (-1);

	part = DISKPART(dev);
	omask = rs->sc_dkdev.dk_openmask & (1 << part);
	lp = rs->sc_dkdev.dk_label;

	if (omask == 0 && raidopen(dev, 0, S_IFBLK, curlwp))
		return (-1);

	if (lp->d_partitions[part].p_fstype != FS_SWAP)
		size = -1;
	else
		size = lp->d_partitions[part].p_size *
		    (lp->d_secsize / DEV_BSIZE);

	if (omask == 0 && raidclose(dev, 0, S_IFBLK, curlwp))
		return (-1);

	return (size);

}

static int
raiddump(dev_t dev, daddr_t blkno, void *va, size_t size)
{
	int     unit = raidunit(dev);
	struct raid_softc *rs;
	const struct bdevsw *bdev;
	struct disklabel *lp;
	RF_Raid_t *raidPtr;
	daddr_t offset;
	int     part, c, sparecol, j, scol, dumpto;
	int     error = 0;

	if ((rs = raidget(unit)) == NULL)
		return ENXIO;

	raidPtr = &rs->sc_r;

	if ((rs->sc_flags & RAIDF_INITED) == 0)
		return ENXIO;

	/* we only support dumping to RAID 1 sets */
	if (raidPtr->Layout.numDataCol != 1 || 
	    raidPtr->Layout.numParityCol != 1)
		return EINVAL;


	if ((error = raidlock(rs)) != 0)
		return error;

	if (size % DEV_BSIZE != 0) {
		error = EINVAL;
		goto out;
	}

	if (blkno + size / DEV_BSIZE > rs->sc_size) {
		printf("%s: blkno (%" PRIu64 ") + size / DEV_BSIZE (%zu) > "
		    "sc->sc_size (%" PRIu64 ")\n", __func__, blkno,
		    size / DEV_BSIZE, rs->sc_size);
		error = EINVAL;
		goto out;
	}

	part = DISKPART(dev);
	lp = rs->sc_dkdev.dk_label;
	offset = lp->d_partitions[part].p_offset + RF_PROTECTED_SECTORS;

	/* figure out what device is alive.. */

	/* 
	   Look for a component to dump to.  The preference for the
	   component to dump to is as follows:
	   1) the master
	   2) a used_spare of the master
	   3) the slave
	   4) a used_spare of the slave
	*/

	dumpto = -1;
	for (c = 0; c < raidPtr->numCol; c++) {
		if (raidPtr->Disks[c].status == rf_ds_optimal) {
			/* this might be the one */
			dumpto = c;
			break;
		}
	}
	
	/* 
	   At this point we have possibly selected a live master or a
	   live slave.  We now check to see if there is a spared
	   master (or a spared slave), if we didn't find a live master
	   or a live slave.  
	*/

	for (c = 0; c < raidPtr->numSpare; c++) {
		sparecol = raidPtr->numCol + c;
		if (raidPtr->Disks[sparecol].status ==  rf_ds_used_spare) {
			/* How about this one? */
			scol = -1;
			for(j=0;j<raidPtr->numCol;j++) {
				if (raidPtr->Disks[j].spareCol == sparecol) {
					scol = j;
					break;
				}
			}
			if (scol == 0) {
				/* 
				   We must have found a spared master!
				   We'll take that over anything else
				   found so far.  (We couldn't have
				   found a real master before, since
				   this is a used spare, and it's
				   saying that it's replacing the
				   master.)  On reboot (with
				   autoconfiguration turned on)
				   sparecol will become the 1st
				   component (component0) of this set.  
				*/
				dumpto = sparecol;
				break;
			} else if (scol != -1) {
				/* 
				   Must be a spared slave.  We'll dump
				   to that if we havn't found anything
				   else so far. 
				*/
				if (dumpto == -1)
					dumpto = sparecol;
			}
		}
	}
	
	if (dumpto == -1) {
		/* we couldn't find any live components to dump to!?!?
		 */
		error = EINVAL;
		goto out;
	}

	bdev = bdevsw_lookup(raidPtr->Disks[dumpto].dev);

	/* 
	   Note that blkno is relative to this particular partition.
	   By adding the offset of this partition in the RAID
	   set, and also adding RF_PROTECTED_SECTORS, we get a
	   value that is relative to the partition used for the
	   underlying component.
	*/
	   
	error = (*bdev->d_dump)(raidPtr->Disks[dumpto].dev, 
				blkno + offset, va, size);
	
out:
	raidunlock(rs);
		
	return error;
}

/* ARGSUSED */
static int
raidopen(dev_t dev, int flags, int fmt,
    struct lwp *l)
{
	int     unit = raidunit(dev);
	struct raid_softc *rs;
	struct disklabel *lp;
	int     part, pmask;
	int     error = 0;

	if ((rs = raidget(unit)) == NULL)
		return ENXIO;
	if ((error = raidlock(rs)) != 0)
		return (error);

	if ((rs->sc_flags & RAIDF_SHUTDOWN) != 0) {
		error = EBUSY;
		goto bad;
	}

	lp = rs->sc_dkdev.dk_label;

	part = DISKPART(dev);

	/*
	 * If there are wedges, and this is not RAW_PART, then we
	 * need to fail.
	 */
	if (rs->sc_dkdev.dk_nwedges != 0 && part != RAW_PART) {
		error = EBUSY;
		goto bad;
	}
	pmask = (1 << part);

	if ((rs->sc_flags & RAIDF_INITED) &&
	    (rs->sc_dkdev.dk_nwedges == 0) &&
	    (rs->sc_dkdev.dk_openmask == 0))
		raidgetdisklabel(dev);

	/* make sure that this partition exists */

	if (part != RAW_PART) {
		if (((rs->sc_flags & RAIDF_INITED) == 0) ||
		    ((part >= lp->d_npartitions) ||
			(lp->d_partitions[part].p_fstype == FS_UNUSED))) {
			error = ENXIO;
			goto bad;
		}
	}
	/* Prevent this unit from being unconfigured while open. */
	switch (fmt) {
	case S_IFCHR:
		rs->sc_dkdev.dk_copenmask |= pmask;
		break;

	case S_IFBLK:
		rs->sc_dkdev.dk_bopenmask |= pmask;
		break;
	}

	if ((rs->sc_dkdev.dk_openmask == 0) &&
	    ((rs->sc_flags & RAIDF_INITED) != 0)) {
		/* First one... mark things as dirty... Note that we *MUST*
		 have done a configure before this.  I DO NOT WANT TO BE
		 SCRIBBLING TO RANDOM COMPONENTS UNTIL IT'S BEEN DETERMINED
		 THAT THEY BELONG TOGETHER!!!!! */
		/* XXX should check to see if we're only open for reading
		   here... If so, we needn't do this, but then need some
		   other way of keeping track of what's happened.. */

		rf_markalldirty(&rs->sc_r);
	}


	rs->sc_dkdev.dk_openmask =
	    rs->sc_dkdev.dk_copenmask | rs->sc_dkdev.dk_bopenmask;

bad:
	raidunlock(rs);

	return (error);


}

/* ARGSUSED */
static int
raidclose(dev_t dev, int flags, int fmt, struct lwp *l)
{
	int     unit = raidunit(dev);
	struct raid_softc *rs;
	int     error = 0;
	int     part;

	if ((rs = raidget(unit)) == NULL)
		return ENXIO;

	if ((error = raidlock(rs)) != 0)
		return (error);

	part = DISKPART(dev);

	/* ...that much closer to allowing unconfiguration... */
	switch (fmt) {
	case S_IFCHR:
		rs->sc_dkdev.dk_copenmask &= ~(1 << part);
		break;

	case S_IFBLK:
		rs->sc_dkdev.dk_bopenmask &= ~(1 << part);
		break;
	}
	rs->sc_dkdev.dk_openmask =
	    rs->sc_dkdev.dk_copenmask | rs->sc_dkdev.dk_bopenmask;

	if ((rs->sc_dkdev.dk_openmask == 0) &&
	    ((rs->sc_flags & RAIDF_INITED) != 0)) {
		/* Last one... device is not unconfigured yet.
		   Device shutdown has taken care of setting the
		   clean bits if RAIDF_INITED is not set
		   mark things as clean... */

		rf_update_component_labels(&rs->sc_r,
						 RF_FINAL_COMPONENT_UPDATE);

		/* If the kernel is shutting down, it will detach
		 * this RAID set soon enough.
		 */
	}

	raidunlock(rs);
	return (0);

}

static void
raidstrategy(struct buf *bp)
{
	unsigned int unit = raidunit(bp->b_dev);
	RF_Raid_t *raidPtr;
	int     wlabel;
	struct raid_softc *rs;

	if ((rs = raidget(unit)) == NULL) {
		bp->b_error = ENXIO;
		goto done;
	}
	if ((rs->sc_flags & RAIDF_INITED) == 0) {
		bp->b_error = ENXIO;
		goto done;
	}
	raidPtr = &rs->sc_r;
	if (!raidPtr->valid) {
		bp->b_error = ENODEV;
		goto done;
	}
	if (bp->b_bcount == 0) {
		db1_printf(("b_bcount is zero..\n"));
		goto done;
	}

	/*
	 * Do bounds checking and adjust transfer.  If there's an
	 * error, the bounds check will flag that for us.
	 */

	wlabel = rs->sc_flags & (RAIDF_WLABEL | RAIDF_LABELLING);
	if (DISKPART(bp->b_dev) == RAW_PART) {
		uint64_t size; /* device size in DEV_BSIZE unit */

		if (raidPtr->logBytesPerSector > DEV_BSHIFT) {
			size = raidPtr->totalSectors <<
			    (raidPtr->logBytesPerSector - DEV_BSHIFT);
		} else {
			size = raidPtr->totalSectors >>
			    (DEV_BSHIFT - raidPtr->logBytesPerSector);
		}
		if (bounds_check_with_mediasize(bp, DEV_BSIZE, size) <= 0) {
			goto done;
		}
	} else {
		if (bounds_check_with_label(&rs->sc_dkdev, bp, wlabel) <= 0) {
			db1_printf(("Bounds check failed!!:%d %d\n",
				(int) bp->b_blkno, (int) wlabel));
			goto done;
		}
	}

	rf_lock_mutex2(raidPtr->iodone_lock);

	bp->b_resid = 0;

	/* stuff it onto our queue */
	bufq_put(rs->buf_queue, bp);

	/* scheduled the IO to happen at the next convenient time */
	rf_signal_cond2(raidPtr->iodone_cv);
	rf_unlock_mutex2(raidPtr->iodone_lock);

	return;

done:
	bp->b_resid = bp->b_bcount;
	biodone(bp);
}

/* ARGSUSED */
static int
raidread(dev_t dev, struct uio *uio, int flags)
{
	int     unit = raidunit(dev);
	struct raid_softc *rs;

	if ((rs = raidget(unit)) == NULL)
		return ENXIO;

	if ((rs->sc_flags & RAIDF_INITED) == 0)
		return (ENXIO);

	return (physio(raidstrategy, NULL, dev, B_READ, minphys, uio));

}

/* ARGSUSED */
static int
raidwrite(dev_t dev, struct uio *uio, int flags)
{
	int     unit = raidunit(dev);
	struct raid_softc *rs;

	if ((rs = raidget(unit)) == NULL)
		return ENXIO;

	if ((rs->sc_flags & RAIDF_INITED) == 0)
		return (ENXIO);

	return (physio(raidstrategy, NULL, dev, B_WRITE, minphys, uio));

}

static int
raid_detach_unlocked(struct raid_softc *rs)
{
	int error;
	RF_Raid_t *raidPtr;

	raidPtr = &rs->sc_r;

	/*
	 * If somebody has a partition mounted, we shouldn't
	 * shutdown.
	 */
	if (rs->sc_dkdev.dk_openmask != 0)
		return EBUSY;

	if ((rs->sc_flags & RAIDF_INITED) == 0)
		;	/* not initialized: nothing to do */
	else if ((error = rf_Shutdown(raidPtr)) != 0)
		return error;
	else
		rs->sc_flags &= ~(RAIDF_INITED|RAIDF_SHUTDOWN);

	/* Detach the disk. */
	dkwedge_delall(&rs->sc_dkdev);
	disk_detach(&rs->sc_dkdev);
	disk_destroy(&rs->sc_dkdev);

	aprint_normal_dev(rs->sc_dev, "detached\n");

	return 0;
}

static int
raidioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	int     unit = raidunit(dev);
	int     error = 0;
	int     part, pmask, s;
	cfdata_t cf;
	struct raid_softc *rs;
	RF_Config_t *k_cfg, *u_cfg;
	RF_Raid_t *raidPtr;
	RF_RaidDisk_t *diskPtr;
	RF_AccTotals_t *totals;
	RF_DeviceConfig_t *d_cfg, **ucfgp;
	u_char *specific_buf;
	int retcode = 0;
	int column;
/*	int raidid; */
	struct rf_recon_req *rrcopy, *rr;
	RF_ComponentLabel_t *clabel;
	RF_ComponentLabel_t *ci_label;
	RF_ComponentLabel_t **clabel_ptr;
	RF_SingleComponent_t *sparePtr,*componentPtr;
	RF_SingleComponent_t component;
	RF_ProgressInfo_t progressInfo, **progressInfoPtr;
	int i, j, d;
#ifdef __HAVE_OLD_DISKLABEL
	struct disklabel newlabel;
#endif

	if ((rs = raidget(unit)) == NULL)
		return ENXIO;
	raidPtr = &rs->sc_r;

	db1_printf(("raidioctl: %d %d %d %lu\n", (int) dev,
		(int) DISKPART(dev), (int) unit, cmd));

	/* Must be open for writes for these commands... */
	switch (cmd) {
#ifdef DIOCGSECTORSIZE
	case DIOCGSECTORSIZE:
		*(u_int *)data = raidPtr->bytesPerSector;
		return 0;
	case DIOCGMEDIASIZE:
		*(off_t *)data =
		    (off_t)raidPtr->totalSectors * raidPtr->bytesPerSector;
		return 0;
#endif
	case DIOCSDINFO:
	case DIOCWDINFO:
#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCWDINFO:
	case ODIOCSDINFO:
#endif
	case DIOCWLABEL:
	case DIOCAWEDGE:
	case DIOCDWEDGE:
	case DIOCMWEDGES:
	case DIOCSSTRATEGY:
		if ((flag & FWRITE) == 0)
			return (EBADF);
	}

	/* Must be initialized for these... */
	switch (cmd) {
	case DIOCGDINFO:
	case DIOCSDINFO:
	case DIOCWDINFO:
#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCGDINFO:
	case ODIOCWDINFO:
	case ODIOCSDINFO:
	case ODIOCGDEFLABEL:
#endif
	case DIOCGPART:
	case DIOCWLABEL:
	case DIOCGDEFLABEL:
	case DIOCAWEDGE:
	case DIOCDWEDGE:
	case DIOCLWEDGES:
	case DIOCMWEDGES:
	case DIOCCACHESYNC:
	case RAIDFRAME_SHUTDOWN:
	case RAIDFRAME_REWRITEPARITY:
	case RAIDFRAME_GET_INFO:
	case RAIDFRAME_RESET_ACCTOTALS:
	case RAIDFRAME_GET_ACCTOTALS:
	case RAIDFRAME_KEEP_ACCTOTALS:
	case RAIDFRAME_GET_SIZE:
	case RAIDFRAME_FAIL_DISK:
	case RAIDFRAME_COPYBACK:
	case RAIDFRAME_CHECK_RECON_STATUS:
	case RAIDFRAME_CHECK_RECON_STATUS_EXT:
	case RAIDFRAME_GET_COMPONENT_LABEL:
	case RAIDFRAME_SET_COMPONENT_LABEL:
	case RAIDFRAME_ADD_HOT_SPARE:
	case RAIDFRAME_REMOVE_HOT_SPARE:
	case RAIDFRAME_INIT_LABELS:
	case RAIDFRAME_REBUILD_IN_PLACE:
	case RAIDFRAME_CHECK_PARITY:
	case RAIDFRAME_CHECK_PARITYREWRITE_STATUS:
	case RAIDFRAME_CHECK_PARITYREWRITE_STATUS_EXT:
	case RAIDFRAME_CHECK_COPYBACK_STATUS:
	case RAIDFRAME_CHECK_COPYBACK_STATUS_EXT:
	case RAIDFRAME_SET_AUTOCONFIG:
	case RAIDFRAME_SET_ROOT:
	case RAIDFRAME_DELETE_COMPONENT:
	case RAIDFRAME_INCORPORATE_HOT_SPARE:
	case RAIDFRAME_PARITYMAP_STATUS:
	case RAIDFRAME_PARITYMAP_GET_DISABLE:
	case RAIDFRAME_PARITYMAP_SET_DISABLE:
	case RAIDFRAME_PARITYMAP_SET_PARAMS:
	case DIOCGSTRATEGY:
	case DIOCSSTRATEGY:
		if ((rs->sc_flags & RAIDF_INITED) == 0)
			return (ENXIO);
	}

	switch (cmd) {
#ifdef COMPAT_50
	case RAIDFRAME_GET_INFO50:
		return rf_get_info50(raidPtr, data);

	case RAIDFRAME_CONFIGURE50:
		if ((retcode = rf_config50(raidPtr, unit, data, &k_cfg)) != 0)
			return retcode;
		goto config;
#endif
		/* configure the system */
	case RAIDFRAME_CONFIGURE:

		if (raidPtr->valid) {
			/* There is a valid RAID set running on this unit! */
			printf("raid%d: Device already configured!\n",unit);
			return(EINVAL);
		}

		/* copy-in the configuration information */
		/* data points to a pointer to the configuration structure */

		u_cfg = *((RF_Config_t **) data);
		RF_Malloc(k_cfg, sizeof(RF_Config_t), (RF_Config_t *));
		if (k_cfg == NULL) {
			return (ENOMEM);
		}
		retcode = copyin(u_cfg, k_cfg, sizeof(RF_Config_t));
		if (retcode) {
			RF_Free(k_cfg, sizeof(RF_Config_t));
			db1_printf(("rf_ioctl: retcode=%d copyin.1\n",
				retcode));
			return (retcode);
		}
		goto config;
	config:
		/* allocate a buffer for the layout-specific data, and copy it
		 * in */
		if (k_cfg->layoutSpecificSize) {
			if (k_cfg->layoutSpecificSize > 10000) {
				/* sanity check */
				RF_Free(k_cfg, sizeof(RF_Config_t));
				return (EINVAL);
			}
			RF_Malloc(specific_buf, k_cfg->layoutSpecificSize,
			    (u_char *));
			if (specific_buf == NULL) {
				RF_Free(k_cfg, sizeof(RF_Config_t));
				return (ENOMEM);
			}
			retcode = copyin(k_cfg->layoutSpecific, specific_buf,
			    k_cfg->layoutSpecificSize);
			if (retcode) {
				RF_Free(k_cfg, sizeof(RF_Config_t));
				RF_Free(specific_buf,
					k_cfg->layoutSpecificSize);
				db1_printf(("rf_ioctl: retcode=%d copyin.2\n",
					retcode));
				return (retcode);
			}
		} else
			specific_buf = NULL;
		k_cfg->layoutSpecific = specific_buf;

		/* should do some kind of sanity check on the configuration.
		 * Store the sum of all the bytes in the last byte? */

		/* configure the system */

		/*
		 * Clear the entire RAID descriptor, just to make sure
		 *  there is no stale data left in the case of a
		 *  reconfiguration
		 */
		memset(raidPtr, 0, sizeof(*raidPtr));
		raidPtr->softc = rs;
		raidPtr->raidid = unit;

		retcode = rf_Configure(raidPtr, k_cfg, NULL);

		if (retcode == 0) {

			/* allow this many simultaneous IO's to
			   this RAID device */
			raidPtr->openings = RAIDOUTSTANDING;

			raidinit(rs);
			rf_markalldirty(raidPtr);
		}
		/* free the buffers.  No return code here. */
		if (k_cfg->layoutSpecificSize) {
			RF_Free(specific_buf, k_cfg->layoutSpecificSize);
		}
		RF_Free(k_cfg, sizeof(RF_Config_t));

		return (retcode);

		/* shutdown the system */
	case RAIDFRAME_SHUTDOWN:

		part = DISKPART(dev);
		pmask = (1 << part);

		if ((error = raidlock(rs)) != 0)
			return (error);

		if ((rs->sc_dkdev.dk_openmask & ~pmask) ||
		    ((rs->sc_dkdev.dk_bopenmask & pmask) &&
			(rs->sc_dkdev.dk_copenmask & pmask)))
			retcode = EBUSY;
		else {
			rs->sc_flags |= RAIDF_SHUTDOWN;
			rs->sc_dkdev.dk_copenmask &= ~pmask;
			rs->sc_dkdev.dk_bopenmask &= ~pmask;
			rs->sc_dkdev.dk_openmask &= ~pmask;
			retcode = 0;
		}

		raidunlock(rs);

		if (retcode != 0)
			return retcode;

		/* free the pseudo device attach bits */

		cf = device_cfdata(rs->sc_dev);
		if ((retcode = config_detach(rs->sc_dev, DETACH_QUIET)) == 0)
			free(cf, M_RAIDFRAME);

		return (retcode);
	case RAIDFRAME_GET_COMPONENT_LABEL:
		clabel_ptr = (RF_ComponentLabel_t **) data;
		/* need to read the component label for the disk indicated
		   by row,column in clabel */

		/*
		 * Perhaps there should be an option to skip the in-core
		 * copy and hit the disk, as with disklabel(8).
		 */
		RF_Malloc(clabel, sizeof(*clabel), (RF_ComponentLabel_t *));

		retcode = copyin(*clabel_ptr, clabel, sizeof(*clabel));

		if (retcode) {
			RF_Free(clabel, sizeof(*clabel));
			return retcode;
		}

		clabel->row = 0; /* Don't allow looking at anything else.*/

		column = clabel->column;

		if ((column < 0) || (column >= raidPtr->numCol +
		    raidPtr->numSpare)) {
			RF_Free(clabel, sizeof(*clabel));
			return EINVAL;
		}

		RF_Free(clabel, sizeof(*clabel));

		clabel = raidget_component_label(raidPtr, column);

		return copyout(clabel, *clabel_ptr, sizeof(**clabel_ptr));

#if 0
	case RAIDFRAME_SET_COMPONENT_LABEL:
		clabel = (RF_ComponentLabel_t *) data;

		/* XXX check the label for valid stuff... */
		/* Note that some things *should not* get modified --
		   the user should be re-initing the labels instead of
		   trying to patch things.
		   */

		raidid = raidPtr->raidid;
#ifdef DEBUG
		printf("raid%d: Got component label:\n", raidid);
		printf("raid%d: Version: %d\n", raidid, clabel->version);
		printf("raid%d: Serial Number: %d\n", raidid, clabel->serial_number);
		printf("raid%d: Mod counter: %d\n", raidid, clabel->mod_counter);
		printf("raid%d: Column: %d\n", raidid, clabel->column);
		printf("raid%d: Num Columns: %d\n", raidid, clabel->num_columns);
		printf("raid%d: Clean: %d\n", raidid, clabel->clean);
		printf("raid%d: Status: %d\n", raidid, clabel->status);
#endif
		clabel->row = 0;
		column = clabel->column;

		if ((column < 0) || (column >= raidPtr->numCol)) {
			return(EINVAL);
		}

		/* XXX this isn't allowed to do anything for now :-) */

		/* XXX and before it is, we need to fill in the rest
		   of the fields!?!?!?! */
		memcpy(raidget_component_label(raidPtr, column),
		    clabel, sizeof(*clabel));
		raidflush_component_label(raidPtr, column);
		return (0);
#endif

	case RAIDFRAME_INIT_LABELS:
		clabel = (RF_ComponentLabel_t *) data;
		/*
		   we only want the serial number from
		   the above.  We get all the rest of the information
		   from the config that was used to create this RAID
		   set.
		   */

		raidPtr->serial_number = clabel->serial_number;

		for(column=0;column<raidPtr->numCol;column++) {
			diskPtr = &raidPtr->Disks[column];
			if (!RF_DEAD_DISK(diskPtr->status)) {
				ci_label = raidget_component_label(raidPtr,
				    column);
				/* Zeroing this is important. */
				memset(ci_label, 0, sizeof(*ci_label));
				raid_init_component_label(raidPtr, ci_label);
				ci_label->serial_number = 
				    raidPtr->serial_number;
				ci_label->row = 0; /* we dont' pretend to support more */
				rf_component_label_set_partitionsize(ci_label,
				    diskPtr->partitionSize);
				ci_label->column = column;
				raidflush_component_label(raidPtr, column);
			}
			/* XXXjld what about the spares? */
		}
		
		return (retcode);
	case RAIDFRAME_SET_AUTOCONFIG:
		d = rf_set_autoconfig(raidPtr, *(int *) data);
		printf("raid%d: New autoconfig value is: %d\n",
		       raidPtr->raidid, d);
		*(int *) data = d;
		return (retcode);

	case RAIDFRAME_SET_ROOT:
		d = rf_set_rootpartition(raidPtr, *(int *) data);
		printf("raid%d: New rootpartition value is: %d\n",
		       raidPtr->raidid, d);
		*(int *) data = d;
		return (retcode);

		/* initialize all parity */
	case RAIDFRAME_REWRITEPARITY:

		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* Parity for RAID 0 is trivially correct */
			raidPtr->parity_good = RF_RAID_CLEAN;
			return(0);
		}

		if (raidPtr->parity_rewrite_in_progress == 1) {
			/* Re-write is already in progress! */
			return(EINVAL);
		}

		retcode = RF_CREATE_THREAD(raidPtr->parity_rewrite_thread,
					   rf_RewriteParityThread,
					   raidPtr,"raid_parity");
		return (retcode);


	case RAIDFRAME_ADD_HOT_SPARE:
		sparePtr = (RF_SingleComponent_t *) data;
		memcpy( &component, sparePtr, sizeof(RF_SingleComponent_t));
		retcode = rf_add_hot_spare(raidPtr, &component);
		return(retcode);

	case RAIDFRAME_REMOVE_HOT_SPARE:
		return(retcode);

	case RAIDFRAME_DELETE_COMPONENT:
		componentPtr = (RF_SingleComponent_t *)data;
		memcpy( &component, componentPtr,
			sizeof(RF_SingleComponent_t));
		retcode = rf_delete_component(raidPtr, &component);
		return(retcode);

	case RAIDFRAME_INCORPORATE_HOT_SPARE:
		componentPtr = (RF_SingleComponent_t *)data;
		memcpy( &component, componentPtr,
			sizeof(RF_SingleComponent_t));
		retcode = rf_incorporate_hot_spare(raidPtr, &component);
		return(retcode);

	case RAIDFRAME_REBUILD_IN_PLACE:

		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* Can't do this on a RAID 0!! */
			return(EINVAL);
		}

		if (raidPtr->recon_in_progress == 1) {
			/* a reconstruct is already in progress! */
			return(EINVAL);
		}

		componentPtr = (RF_SingleComponent_t *) data;
		memcpy( &component, componentPtr,
			sizeof(RF_SingleComponent_t));
		component.row = 0; /* we don't support any more */
		column = component.column;

		if ((column < 0) || (column >= raidPtr->numCol)) {
			return(EINVAL);
		}

		rf_lock_mutex2(raidPtr->mutex);
		if ((raidPtr->Disks[column].status == rf_ds_optimal) &&
		    (raidPtr->numFailures > 0)) {
			/* XXX 0 above shouldn't be constant!!! */
			/* some component other than this has failed.
			   Let's not make things worse than they already
			   are... */
			printf("raid%d: Unable to reconstruct to disk at:\n",
			       raidPtr->raidid);
			printf("raid%d:     Col: %d   Too many failures.\n",
			       raidPtr->raidid, column);
			rf_unlock_mutex2(raidPtr->mutex);
			return (EINVAL);
		}
		if (raidPtr->Disks[column].status ==
		    rf_ds_reconstructing) {
			printf("raid%d: Unable to reconstruct to disk at:\n",
			       raidPtr->raidid);
			printf("raid%d:    Col: %d   Reconstruction already occurring!\n", raidPtr->raidid, column);

			rf_unlock_mutex2(raidPtr->mutex);
			return (EINVAL);
		}
		if (raidPtr->Disks[column].status == rf_ds_spared) {
			rf_unlock_mutex2(raidPtr->mutex);
			return (EINVAL);
		}
		rf_unlock_mutex2(raidPtr->mutex);

		RF_Malloc(rrcopy, sizeof(*rrcopy), (struct rf_recon_req *));
		if (rrcopy == NULL)
			return(ENOMEM);

		rrcopy->raidPtr = (void *) raidPtr;
		rrcopy->col = column;

		retcode = RF_CREATE_THREAD(raidPtr->recon_thread,
					   rf_ReconstructInPlaceThread,
					   rrcopy,"raid_reconip");
		return(retcode);

	case RAIDFRAME_GET_INFO:
		if (!raidPtr->valid)
			return (ENODEV);
		ucfgp = (RF_DeviceConfig_t **) data;
		RF_Malloc(d_cfg, sizeof(RF_DeviceConfig_t),
			  (RF_DeviceConfig_t *));
		if (d_cfg == NULL)
			return (ENOMEM);
		d_cfg->rows = 1; /* there is only 1 row now */
		d_cfg->cols = raidPtr->numCol;
		d_cfg->ndevs = raidPtr->numCol;
		if (d_cfg->ndevs >= RF_MAX_DISKS) {
			RF_Free(d_cfg, sizeof(RF_DeviceConfig_t));
			return (ENOMEM);
		}
		d_cfg->nspares = raidPtr->numSpare;
		if (d_cfg->nspares >= RF_MAX_DISKS) {
			RF_Free(d_cfg, sizeof(RF_DeviceConfig_t));
			return (ENOMEM);
		}
		d_cfg->maxqdepth = raidPtr->maxQueueDepth;
		d = 0;
		for (j = 0; j < d_cfg->cols; j++) {
			d_cfg->devs[d] = raidPtr->Disks[j];
			d++;
		}
		for (j = d_cfg->cols, i = 0; i < d_cfg->nspares; i++, j++) {
			d_cfg->spares[i] = raidPtr->Disks[j];
			if (d_cfg->spares[i].status == rf_ds_rebuilding_spare) {
				/* XXX: raidctl(8) expects to see this as a used spare */
				d_cfg->spares[i].status = rf_ds_used_spare;
			}
		}
		retcode = copyout(d_cfg, *ucfgp, sizeof(RF_DeviceConfig_t));
		RF_Free(d_cfg, sizeof(RF_DeviceConfig_t));

		return (retcode);

	case RAIDFRAME_CHECK_PARITY:
		*(int *) data = raidPtr->parity_good;
		return (0);

	case RAIDFRAME_PARITYMAP_STATUS:
		if (rf_paritymap_ineligible(raidPtr))
			return EINVAL;
		rf_paritymap_status(raidPtr->parity_map,
		    (struct rf_pmstat *)data);
		return 0;

	case RAIDFRAME_PARITYMAP_SET_PARAMS:
		if (rf_paritymap_ineligible(raidPtr))
			return EINVAL;
		if (raidPtr->parity_map == NULL)
			return ENOENT; /* ??? */
		if (0 != rf_paritymap_set_params(raidPtr->parity_map, 
			(struct rf_pmparams *)data, 1))
			return EINVAL;
		return 0;

	case RAIDFRAME_PARITYMAP_GET_DISABLE:
		if (rf_paritymap_ineligible(raidPtr))
			return EINVAL;
		*(int *) data = rf_paritymap_get_disable(raidPtr);
		return 0;

	case RAIDFRAME_PARITYMAP_SET_DISABLE:
		if (rf_paritymap_ineligible(raidPtr))
			return EINVAL;
		rf_paritymap_set_disable(raidPtr, *(int *)data);
		/* XXX should errors be passed up? */
		return 0;

	case RAIDFRAME_RESET_ACCTOTALS:
		memset(&raidPtr->acc_totals, 0, sizeof(raidPtr->acc_totals));
		return (0);

	case RAIDFRAME_GET_ACCTOTALS:
		totals = (RF_AccTotals_t *) data;
		*totals = raidPtr->acc_totals;
		return (0);

	case RAIDFRAME_KEEP_ACCTOTALS:
		raidPtr->keep_acc_totals = *(int *)data;
		return (0);

	case RAIDFRAME_GET_SIZE:
		*(int *) data = raidPtr->totalSectors;
		return (0);

		/* fail a disk & optionally start reconstruction */
	case RAIDFRAME_FAIL_DISK:

		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* Can't do this on a RAID 0!! */
			return(EINVAL);
		}

		rr = (struct rf_recon_req *) data;
		rr->row = 0;
		if (rr->col < 0 || rr->col >= raidPtr->numCol)
			return (EINVAL);


		rf_lock_mutex2(raidPtr->mutex);
		if (raidPtr->status == rf_rs_reconstructing) {
			/* you can't fail a disk while we're reconstructing! */
			/* XXX wrong for RAID6 */
			rf_unlock_mutex2(raidPtr->mutex);
			return (EINVAL);
		}
		if ((raidPtr->Disks[rr->col].status ==
		     rf_ds_optimal) && (raidPtr->numFailures > 0)) {
			/* some other component has failed.  Let's not make
			   things worse. XXX wrong for RAID6 */
			rf_unlock_mutex2(raidPtr->mutex);
			return (EINVAL);
		}
		if (raidPtr->Disks[rr->col].status == rf_ds_spared) {
			/* Can't fail a spared disk! */
			rf_unlock_mutex2(raidPtr->mutex);
			return (EINVAL);
		}
		rf_unlock_mutex2(raidPtr->mutex);

		/* make a copy of the recon request so that we don't rely on
		 * the user's buffer */
		RF_Malloc(rrcopy, sizeof(*rrcopy), (struct rf_recon_req *));
		if (rrcopy == NULL)
			return(ENOMEM);
		memcpy(rrcopy, rr, sizeof(*rr));
		rrcopy->raidPtr = (void *) raidPtr;

		retcode = RF_CREATE_THREAD(raidPtr->recon_thread,
					   rf_ReconThread,
					   rrcopy,"raid_recon");
		return (0);

		/* invoke a copyback operation after recon on whatever disk
		 * needs it, if any */
	case RAIDFRAME_COPYBACK:

		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* This makes no sense on a RAID 0!! */
			return(EINVAL);
		}

		if (raidPtr->copyback_in_progress == 1) {
			/* Copyback is already in progress! */
			return(EINVAL);
		}

		retcode = RF_CREATE_THREAD(raidPtr->copyback_thread,
					   rf_CopybackThread,
					   raidPtr,"raid_copyback");
		return (retcode);

		/* return the percentage completion of reconstruction */
	case RAIDFRAME_CHECK_RECON_STATUS:
		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* This makes no sense on a RAID 0, so tell the
			   user it's done. */
			*(int *) data = 100;
			return(0);
		}
		if (raidPtr->status != rf_rs_reconstructing)
			*(int *) data = 100;
		else {
			if (raidPtr->reconControl->numRUsTotal > 0) {
				*(int *) data = (raidPtr->reconControl->numRUsComplete * 100 / raidPtr->reconControl->numRUsTotal);
			} else {
				*(int *) data = 0;
			}
		}
		return (0);
	case RAIDFRAME_CHECK_RECON_STATUS_EXT:
		progressInfoPtr = (RF_ProgressInfo_t **) data;
		if (raidPtr->status != rf_rs_reconstructing) {
			progressInfo.remaining = 0;
			progressInfo.completed = 100;
			progressInfo.total = 100;
		} else {
			progressInfo.total =
				raidPtr->reconControl->numRUsTotal;
			progressInfo.completed =
				raidPtr->reconControl->numRUsComplete;
			progressInfo.remaining = progressInfo.total -
				progressInfo.completed;
		}
		retcode = copyout(&progressInfo, *progressInfoPtr,
				  sizeof(RF_ProgressInfo_t));
		return (retcode);

	case RAIDFRAME_CHECK_PARITYREWRITE_STATUS:
		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* This makes no sense on a RAID 0, so tell the
			   user it's done. */
			*(int *) data = 100;
			return(0);
		}
		if (raidPtr->parity_rewrite_in_progress == 1) {
			*(int *) data = 100 *
				raidPtr->parity_rewrite_stripes_done /
				raidPtr->Layout.numStripe;
		} else {
			*(int *) data = 100;
		}
		return (0);

	case RAIDFRAME_CHECK_PARITYREWRITE_STATUS_EXT:
		progressInfoPtr = (RF_ProgressInfo_t **) data;
		if (raidPtr->parity_rewrite_in_progress == 1) {
			progressInfo.total = raidPtr->Layout.numStripe;
			progressInfo.completed =
				raidPtr->parity_rewrite_stripes_done;
			progressInfo.remaining = progressInfo.total -
				progressInfo.completed;
		} else {
			progressInfo.remaining = 0;
			progressInfo.completed = 100;
			progressInfo.total = 100;
		}
		retcode = copyout(&progressInfo, *progressInfoPtr,
				  sizeof(RF_ProgressInfo_t));
		return (retcode);

	case RAIDFRAME_CHECK_COPYBACK_STATUS:
		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* This makes no sense on a RAID 0 */
			*(int *) data = 100;
			return(0);
		}
		if (raidPtr->copyback_in_progress == 1) {
			*(int *) data = 100 * raidPtr->copyback_stripes_done /
				raidPtr->Layout.numStripe;
		} else {
			*(int *) data = 100;
		}
		return (0);

	case RAIDFRAME_CHECK_COPYBACK_STATUS_EXT:
		progressInfoPtr = (RF_ProgressInfo_t **) data;
		if (raidPtr->copyback_in_progress == 1) {
			progressInfo.total = raidPtr->Layout.numStripe;
			progressInfo.completed =
				raidPtr->copyback_stripes_done;
			progressInfo.remaining = progressInfo.total -
				progressInfo.completed;
		} else {
			progressInfo.remaining = 0;
			progressInfo.completed = 100;
			progressInfo.total = 100;
		}
		retcode = copyout(&progressInfo, *progressInfoPtr,
				  sizeof(RF_ProgressInfo_t));
		return (retcode);

		/* the sparetable daemon calls this to wait for the kernel to
		 * need a spare table. this ioctl does not return until a
		 * spare table is needed. XXX -- calling mpsleep here in the
		 * ioctl code is almost certainly wrong and evil. -- XXX XXX
		 * -- I should either compute the spare table in the kernel,
		 * or have a different -- XXX XXX -- interface (a different
		 * character device) for delivering the table     -- XXX */
#if 0
	case RAIDFRAME_SPARET_WAIT:
		rf_lock_mutex2(rf_sparet_wait_mutex);
		while (!rf_sparet_wait_queue)
			rf_wait_cond2(rf_sparet_wait_cv, rf_sparet_wait_mutex);
		waitreq = rf_sparet_wait_queue;
		rf_sparet_wait_queue = rf_sparet_wait_queue->next;
		rf_unlock_mutex2(rf_sparet_wait_mutex);

		/* structure assignment */
		*((RF_SparetWait_t *) data) = *waitreq;

		RF_Free(waitreq, sizeof(*waitreq));
		return (0);

		/* wakes up a process waiting on SPARET_WAIT and puts an error
		 * code in it that will cause the dameon to exit */
	case RAIDFRAME_ABORT_SPARET_WAIT:
		RF_Malloc(waitreq, sizeof(*waitreq), (RF_SparetWait_t *));
		waitreq->fcol = -1;
		rf_lock_mutex2(rf_sparet_wait_mutex);
		waitreq->next = rf_sparet_wait_queue;
		rf_sparet_wait_queue = waitreq;
		rf_broadcast_conf2(rf_sparet_wait_cv);
		rf_unlock_mutex2(rf_sparet_wait_mutex);
		return (0);

		/* used by the spare table daemon to deliver a spare table
		 * into the kernel */
	case RAIDFRAME_SEND_SPARET:

		/* install the spare table */
		retcode = rf_SetSpareTable(raidPtr, *(void **) data);

		/* respond to the requestor.  the return status of the spare
		 * table installation is passed in the "fcol" field */
		RF_Malloc(waitreq, sizeof(*waitreq), (RF_SparetWait_t *));
		waitreq->fcol = retcode;
		rf_lock_mutex2(rf_sparet_wait_mutex);
		waitreq->next = rf_sparet_resp_queue;
		rf_sparet_resp_queue = waitreq;
		rf_broadcast_cond2(rf_sparet_resp_cv);
		rf_unlock_mutex2(rf_sparet_wait_mutex);

		return (retcode);
#endif

	default:
		break; /* fall through to the os-specific code below */

	}

	if (!raidPtr->valid)
		return (EINVAL);

	/*
	 * Add support for "regular" device ioctls here.
	 */
	
	error = disk_ioctl(&rs->sc_dkdev, dev, cmd, data, flag, l); 
	if (error != EPASSTHROUGH)
		return (error);

	switch (cmd) {
	case DIOCWDINFO:
	case DIOCSDINFO:
#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCWDINFO:
	case ODIOCSDINFO:
#endif
	{
		struct disklabel *lp;
#ifdef __HAVE_OLD_DISKLABEL
		if (cmd == ODIOCSDINFO || cmd == ODIOCWDINFO) {
			memset(&newlabel, 0, sizeof newlabel);
			memcpy(&newlabel, data, sizeof (struct olddisklabel));
			lp = &newlabel;
		} else
#endif
		lp = (struct disklabel *)data;

		if ((error = raidlock(rs)) != 0)
			return (error);

		rs->sc_flags |= RAIDF_LABELLING;

		error = setdisklabel(rs->sc_dkdev.dk_label,
		    lp, 0, rs->sc_dkdev.dk_cpulabel);
		if (error == 0) {
			if (cmd == DIOCWDINFO
#ifdef __HAVE_OLD_DISKLABEL
			    || cmd == ODIOCWDINFO
#endif
			   )
				error = writedisklabel(RAIDLABELDEV(dev),
				    raidstrategy, rs->sc_dkdev.dk_label,
				    rs->sc_dkdev.dk_cpulabel);
		}
		rs->sc_flags &= ~RAIDF_LABELLING;

		raidunlock(rs);

		if (error)
			return (error);
		break;
	}

	case DIOCWLABEL:
		if (*(int *) data != 0)
			rs->sc_flags |= RAIDF_WLABEL;
		else
			rs->sc_flags &= ~RAIDF_WLABEL;
		break;

	case DIOCGDEFLABEL:
		raidgetdefaultlabel(raidPtr, rs, (struct disklabel *) data);
		break;

#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCGDEFLABEL:
		raidgetdefaultlabel(raidPtr, rs, &newlabel);
		if (newlabel.d_npartitions > OLDMAXPARTITIONS)
			return ENOTTY;
		memcpy(data, &newlabel, sizeof (struct olddisklabel));
		break;
#endif

	case DIOCCACHESYNC:
		return rf_sync_component_caches(raidPtr);

	case DIOCGSTRATEGY:
	    {
		struct disk_strategy *dks = (void *)data;

		s = splbio();
		strlcpy(dks->dks_name, bufq_getstrategyname(rs->buf_queue),
		    sizeof(dks->dks_name));
		splx(s);
		dks->dks_paramlen = 0;

		return 0;
	    }
	
	case DIOCSSTRATEGY:
	    {
		struct disk_strategy *dks = (void *)data;
		struct bufq_state *new;
		struct bufq_state *old;

		if (dks->dks_param != NULL) {
			return EINVAL;
		}
		dks->dks_name[sizeof(dks->dks_name) - 1] = 0; /* ensure term */
		error = bufq_alloc(&new, dks->dks_name,
		    BUFQ_EXACT|BUFQ_SORT_RAWBLOCK);
		if (error) {
			return error;
		}
		s = splbio();
		old = rs->buf_queue;
		bufq_move(new, old);
		rs->buf_queue = new;
		splx(s);
		bufq_free(old);

		return 0;
	    }

	default:
		retcode = ENOTTY;
	}
	return (retcode);

}


/* raidinit -- complete the rest of the initialization for the
   RAIDframe device.  */


static void
raidinit(struct raid_softc *rs)
{
	cfdata_t cf;
	int     unit;
	RF_Raid_t *raidPtr = &rs->sc_r;

	unit = raidPtr->raidid;


	/* XXX should check return code first... */
	rs->sc_flags |= RAIDF_INITED;

	/* XXX doesn't check bounds. */
	snprintf(rs->sc_xname, sizeof(rs->sc_xname), "raid%d", unit);

	/* attach the pseudo device */
	cf = malloc(sizeof(*cf), M_RAIDFRAME, M_WAITOK);
	cf->cf_name = raid_cd.cd_name;
	cf->cf_atname = raid_cd.cd_name;
	cf->cf_unit = unit;
	cf->cf_fstate = FSTATE_STAR;

	rs->sc_dev = config_attach_pseudo(cf);

	if (rs->sc_dev == NULL) {
		printf("raid%d: config_attach_pseudo failed\n",
		    raidPtr->raidid);
		rs->sc_flags &= ~RAIDF_INITED;
		free(cf, M_RAIDFRAME);
		return;
	}

	/* disk_attach actually creates space for the CPU disklabel, among
	 * other things, so it's critical to call this *BEFORE* we try putzing
	 * with disklabels. */

	disk_init(&rs->sc_dkdev, rs->sc_xname, &rf_dkdriver);
	disk_attach(&rs->sc_dkdev);

	/* XXX There may be a weird interaction here between this, and
	 * protectedSectors, as used in RAIDframe.  */

	rs->sc_size = raidPtr->totalSectors;

	rf_set_geometry(rs, raidPtr);

	dkwedge_discover(&rs->sc_dkdev);

}
#if (RF_INCLUDE_PARITY_DECLUSTERING_DS > 0)
/* wake up the daemon & tell it to get us a spare table
 * XXX
 * the entries in the queues should be tagged with the raidPtr
 * so that in the extremely rare case that two recons happen at once,
 * we know for which device were requesting a spare table
 * XXX
 *
 * XXX This code is not currently used. GO
 */
int
rf_GetSpareTableFromDaemon(RF_SparetWait_t *req)
{
	int     retcode;

	rf_lock_mutex2(rf_sparet_wait_mutex);
	req->next = rf_sparet_wait_queue;
	rf_sparet_wait_queue = req;
	rf_broadcast_cond2(rf_sparet_wait_cv);

	/* mpsleep unlocks the mutex */
	while (!rf_sparet_resp_queue) {
		rf_wait_cond2(rf_sparet_resp_cv, rf_sparet_wait_mutex);
	}
	req = rf_sparet_resp_queue;
	rf_sparet_resp_queue = req->next;
	rf_unlock_mutex2(rf_sparet_wait_mutex);

	retcode = req->fcol;
	RF_Free(req, sizeof(*req));	/* this is not the same req as we
					 * alloc'd */
	return (retcode);
}
#endif

/* a wrapper around rf_DoAccess that extracts appropriate info from the
 * bp & passes it down.
 * any calls originating in the kernel must use non-blocking I/O
 * do some extra sanity checking to return "appropriate" error values for
 * certain conditions (to make some standard utilities work)
 *
 * Formerly known as: rf_DoAccessKernel
 */
void
raidstart(RF_Raid_t *raidPtr)
{
	RF_SectorCount_t num_blocks, pb, sum;
	RF_RaidAddr_t raid_addr;
	struct partition *pp;
	daddr_t blocknum;
	struct raid_softc *rs;
	int     do_async;
	struct buf *bp;
	int rc;

	rs = raidPtr->softc;
	/* quick check to see if anything has died recently */
	rf_lock_mutex2(raidPtr->mutex);
	if (raidPtr->numNewFailures > 0) {
		rf_unlock_mutex2(raidPtr->mutex);
		rf_update_component_labels(raidPtr,
					   RF_NORMAL_COMPONENT_UPDATE);
		rf_lock_mutex2(raidPtr->mutex);
		raidPtr->numNewFailures--;
	}

	/* Check to see if we're at the limit... */
	while (raidPtr->openings > 0) {
		rf_unlock_mutex2(raidPtr->mutex);

		/* get the next item, if any, from the queue */
		if ((bp = bufq_get(rs->buf_queue)) == NULL) {
			/* nothing more to do */
			return;
		}

		/* Ok, for the bp we have here, bp->b_blkno is relative to the
		 * partition.. Need to make it absolute to the underlying
		 * device.. */

		blocknum = bp->b_blkno << DEV_BSHIFT >> raidPtr->logBytesPerSector;
		if (DISKPART(bp->b_dev) != RAW_PART) {
			pp = &rs->sc_dkdev.dk_label->d_partitions[DISKPART(bp->b_dev)];
			blocknum += pp->p_offset;
		}

		db1_printf(("Blocks: %d, %d\n", (int) bp->b_blkno,
			    (int) blocknum));

		db1_printf(("bp->b_bcount = %d\n", (int) bp->b_bcount));
		db1_printf(("bp->b_resid = %d\n", (int) bp->b_resid));

		/* *THIS* is where we adjust what block we're going to...
		 * but DO NOT TOUCH bp->b_blkno!!! */
		raid_addr = blocknum;

		num_blocks = bp->b_bcount >> raidPtr->logBytesPerSector;
		pb = (bp->b_bcount & raidPtr->sectorMask) ? 1 : 0;
		sum = raid_addr + num_blocks + pb;
		if (1 || rf_debugKernelAccess) {
			db1_printf(("raid_addr=%d sum=%d num_blocks=%d(+%d) (%d)\n",
				    (int) raid_addr, (int) sum, (int) num_blocks,
				    (int) pb, (int) bp->b_resid));
		}
		if ((sum > raidPtr->totalSectors) || (sum < raid_addr)
		    || (sum < num_blocks) || (sum < pb)) {
			bp->b_error = ENOSPC;
			bp->b_resid = bp->b_bcount;
			biodone(bp);
			rf_lock_mutex2(raidPtr->mutex);
			continue;
		}
		/*
		 * XXX rf_DoAccess() should do this, not just DoAccessKernel()
		 */

		if (bp->b_bcount & raidPtr->sectorMask) {
			bp->b_error = EINVAL;
			bp->b_resid = bp->b_bcount;
			biodone(bp);
			rf_lock_mutex2(raidPtr->mutex);
			continue;

		}
		db1_printf(("Calling DoAccess..\n"));


		rf_lock_mutex2(raidPtr->mutex);
		raidPtr->openings--;
		rf_unlock_mutex2(raidPtr->mutex);

		/*
		 * Everything is async.
		 */
		do_async = 1;

		disk_busy(&rs->sc_dkdev);

		/* XXX we're still at splbio() here... do we *really*
		   need to be? */

		/* don't ever condition on bp->b_flags & B_WRITE.
		 * always condition on B_READ instead */

		rc = rf_DoAccess(raidPtr, (bp->b_flags & B_READ) ?
				 RF_IO_TYPE_READ : RF_IO_TYPE_WRITE,
				 do_async, raid_addr, num_blocks,
				 bp->b_data, bp, RF_DAG_NONBLOCKING_IO);

		if (rc) {
			bp->b_error = rc;
			bp->b_resid = bp->b_bcount;
			biodone(bp);
			/* continue loop */
		}

		rf_lock_mutex2(raidPtr->mutex);
	}
	rf_unlock_mutex2(raidPtr->mutex);
}




/* invoke an I/O from kernel mode.  Disk queue should be locked upon entry */

int
rf_DispatchKernelIO(RF_DiskQueue_t *queue, RF_DiskQueueData_t *req)
{
	int     op = (req->type == RF_IO_TYPE_READ) ? B_READ : B_WRITE;
	struct buf *bp;

	req->queue = queue;
	bp = req->bp;

	switch (req->type) {
	case RF_IO_TYPE_NOP:	/* used primarily to unlock a locked queue */
		/* XXX need to do something extra here.. */
		/* I'm leaving this in, as I've never actually seen it used,
		 * and I'd like folks to report it... GO */
		printf(("WAKEUP CALLED\n"));
		queue->numOutstanding++;

		bp->b_flags = 0;
		bp->b_private = req;

		KernelWakeupFunc(bp);
		break;

	case RF_IO_TYPE_READ:
	case RF_IO_TYPE_WRITE:
#if RF_ACC_TRACE > 0
		if (req->tracerec) {
			RF_ETIMER_START(req->tracerec->timer);
		}
#endif
		InitBP(bp, queue->rf_cinfo->ci_vp,
		    op, queue->rf_cinfo->ci_dev,
		    req->sectorOffset, req->numSector,
		    req->buf, KernelWakeupFunc, (void *) req,
		    queue->raidPtr->logBytesPerSector, req->b_proc);

		if (rf_debugKernelAccess) {
			db1_printf(("dispatch: bp->b_blkno = %ld\n",
				(long) bp->b_blkno));
		}
		queue->numOutstanding++;
		queue->last_deq_sector = req->sectorOffset;
		/* acc wouldn't have been let in if there were any pending
		 * reqs at any other priority */
		queue->curPriority = req->priority;

		db1_printf(("Going for %c to unit %d col %d\n",
			    req->type, queue->raidPtr->raidid,
			    queue->col));
		db1_printf(("sector %d count %d (%d bytes) %d\n",
			(int) req->sectorOffset, (int) req->numSector,
			(int) (req->numSector <<
			    queue->raidPtr->logBytesPerSector),
			(int) queue->raidPtr->logBytesPerSector));

		/*
		 * XXX: drop lock here since this can block at 
		 * least with backing SCSI devices.  Retake it
		 * to minimize fuss with calling interfaces.
		 */

		RF_UNLOCK_QUEUE_MUTEX(queue, "unusedparam");
		bdev_strategy(bp);
		RF_LOCK_QUEUE_MUTEX(queue, "unusedparam");
		break;

	default:
		panic("bad req->type in rf_DispatchKernelIO");
	}
	db1_printf(("Exiting from DispatchKernelIO\n"));

	return (0);
}
/* this is the callback function associated with a I/O invoked from
   kernel code.
 */
static void
KernelWakeupFunc(struct buf *bp)
{
	RF_DiskQueueData_t *req = NULL;
	RF_DiskQueue_t *queue;

	db1_printf(("recovering the request queue:\n"));

	req = bp->b_private;

	queue = (RF_DiskQueue_t *) req->queue;

	rf_lock_mutex2(queue->raidPtr->iodone_lock);

#if RF_ACC_TRACE > 0
	if (req->tracerec) {
		RF_ETIMER_STOP(req->tracerec->timer);
		RF_ETIMER_EVAL(req->tracerec->timer);
		rf_lock_mutex2(rf_tracing_mutex);
		req->tracerec->diskwait_us += RF_ETIMER_VAL_US(req->tracerec->timer);
		req->tracerec->phys_io_us += RF_ETIMER_VAL_US(req->tracerec->timer);
		req->tracerec->num_phys_ios++;
		rf_unlock_mutex2(rf_tracing_mutex);
	}
#endif

	/* XXX Ok, let's get aggressive... If b_error is set, let's go
	 * ballistic, and mark the component as hosed... */

	if (bp->b_error != 0) {
		/* Mark the disk as dead */
		/* but only mark it once... */
		/* and only if it wouldn't leave this RAID set
		   completely broken */
		if (((queue->raidPtr->Disks[queue->col].status ==
		      rf_ds_optimal) ||
		     (queue->raidPtr->Disks[queue->col].status ==
		      rf_ds_used_spare)) && 
		     (queue->raidPtr->numFailures <
		      queue->raidPtr->Layout.map->faultsTolerated)) {
			printf("raid%d: IO Error (%d). Marking %s as failed.\n",
			       queue->raidPtr->raidid,
			       bp->b_error,
			       queue->raidPtr->Disks[queue->col].devname);
			queue->raidPtr->Disks[queue->col].status =
			    rf_ds_failed;
			queue->raidPtr->status = rf_rs_degraded;
			queue->raidPtr->numFailures++;
			queue->raidPtr->numNewFailures++;
		} else {	/* Disk is already dead... */
			/* printf("Disk already marked as dead!\n"); */
		}

	}

	/* Fill in the error value */
	req->error = bp->b_error;

	/* Drop this one on the "finished" queue... */
	TAILQ_INSERT_TAIL(&(queue->raidPtr->iodone), req, iodone_entries);

	/* Let the raidio thread know there is work to be done. */
	rf_signal_cond2(queue->raidPtr->iodone_cv);

	rf_unlock_mutex2(queue->raidPtr->iodone_lock);
}


/*
 * initialize a buf structure for doing an I/O in the kernel.
 */
static void
InitBP(struct buf *bp, struct vnode *b_vp, unsigned rw_flag, dev_t dev,
       RF_SectorNum_t startSect, RF_SectorCount_t numSect, void *bf,
       void (*cbFunc) (struct buf *), void *cbArg, int logBytesPerSector,
       struct proc *b_proc)
{
	/* bp->b_flags       = B_PHYS | rw_flag; */
	bp->b_flags = rw_flag;	/* XXX need B_PHYS here too??? */
	bp->b_oflags = 0;
	bp->b_cflags = 0;
	bp->b_bcount = numSect << logBytesPerSector;
	bp->b_bufsize = bp->b_bcount;
	bp->b_error = 0;
	bp->b_dev = dev;
	bp->b_data = bf;
	bp->b_blkno = startSect << logBytesPerSector >> DEV_BSHIFT;
	bp->b_resid = bp->b_bcount;	/* XXX is this right!??!?!! */
	if (bp->b_bcount == 0) {
		panic("bp->b_bcount is zero in InitBP!!");
	}
	bp->b_proc = b_proc;
	bp->b_iodone = cbFunc;
	bp->b_private = cbArg;
}

static void
raidgetdefaultlabel(RF_Raid_t *raidPtr, struct raid_softc *rs,
		    struct disklabel *lp)
{
	memset(lp, 0, sizeof(*lp));

	/* fabricate a label... */
	if (raidPtr->totalSectors > UINT32_MAX)
		lp->d_secperunit = UINT32_MAX;
	else
		lp->d_secperunit = raidPtr->totalSectors;
	lp->d_secsize = raidPtr->bytesPerSector;
	lp->d_nsectors = raidPtr->Layout.dataSectorsPerStripe;
	lp->d_ntracks = 4 * raidPtr->numCol;
	lp->d_ncylinders = raidPtr->totalSectors /
		(lp->d_nsectors * lp->d_ntracks);
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;

	strncpy(lp->d_typename, "raid", sizeof(lp->d_typename));
	lp->d_type = DKTYPE_RAID;
	strncpy(lp->d_packname, "fictitious", sizeof(lp->d_packname));
	lp->d_rpm = 3600;
	lp->d_interleave = 1;
	lp->d_flags = 0;

	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_partitions[RAW_PART].p_size = lp->d_secperunit;
	lp->d_partitions[RAW_PART].p_fstype = FS_UNUSED;
	lp->d_npartitions = RAW_PART + 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(rs->sc_dkdev.dk_label);

}
/*
 * Read the disklabel from the raid device.  If one is not present, fake one
 * up.
 */
static void
raidgetdisklabel(dev_t dev)
{
	int     unit = raidunit(dev);
	struct raid_softc *rs;
	const char   *errstring;
	struct disklabel *lp;
	struct cpu_disklabel *clp;
	RF_Raid_t *raidPtr;

	if ((rs = raidget(unit)) == NULL)
		return;

	lp = rs->sc_dkdev.dk_label;
	clp = rs->sc_dkdev.dk_cpulabel;

	db1_printf(("Getting the disklabel...\n"));

	memset(clp, 0, sizeof(*clp));

	raidPtr = &rs->sc_r;

	raidgetdefaultlabel(raidPtr, rs, lp);

	/*
	 * Call the generic disklabel extraction routine.
	 */
	errstring = readdisklabel(RAIDLABELDEV(dev), raidstrategy,
	    rs->sc_dkdev.dk_label, rs->sc_dkdev.dk_cpulabel);
	if (errstring)
		raidmakedisklabel(rs);
	else {
		int     i;
		struct partition *pp;

		/*
		 * Sanity check whether the found disklabel is valid.
		 *
		 * This is necessary since total size of the raid device
		 * may vary when an interleave is changed even though exactly
		 * same components are used, and old disklabel may used
		 * if that is found.
		 */
		if (lp->d_secperunit < UINT32_MAX ?
		    lp->d_secperunit != rs->sc_size :
		    lp->d_secperunit > rs->sc_size)
			printf("raid%d: WARNING: %s: "
			    "total sector size in disklabel (%ju) != "
			    "the size of raid (%ju)\n", unit, rs->sc_xname,
			    (uintmax_t)lp->d_secperunit,
			    (uintmax_t)rs->sc_size);
		for (i = 0; i < lp->d_npartitions; i++) {
			pp = &lp->d_partitions[i];
			if (pp->p_offset + pp->p_size > rs->sc_size)
				printf("raid%d: WARNING: %s: end of partition `%c' "
				       "exceeds the size of raid (%ju)\n",
				       unit, rs->sc_xname, 'a' + i,
				       (uintmax_t)rs->sc_size);
		}
	}

}
/*
 * Take care of things one might want to take care of in the event
 * that a disklabel isn't present.
 */
static void
raidmakedisklabel(struct raid_softc *rs)
{
	struct disklabel *lp = rs->sc_dkdev.dk_label;
	db1_printf(("Making a label..\n"));

	/*
	 * For historical reasons, if there's no disklabel present
	 * the raw partition must be marked FS_BSDFFS.
	 */

	lp->d_partitions[RAW_PART].p_fstype = FS_BSDFFS;

	strncpy(lp->d_packname, "default label", sizeof(lp->d_packname));

	lp->d_checksum = dkcksum(lp);
}
/*
 * Wait interruptibly for an exclusive lock.
 *
 * XXX
 * Several drivers do this; it should be abstracted and made MP-safe.
 * (Hmm... where have we seen this warning before :->  GO )
 */
static int
raidlock(struct raid_softc *rs)
{
	int     error;

	while ((rs->sc_flags & RAIDF_LOCKED) != 0) {
		rs->sc_flags |= RAIDF_WANTED;
		if ((error =
			tsleep(rs, PRIBIO | PCATCH, "raidlck", 0)) != 0)
			return (error);
	}
	rs->sc_flags |= RAIDF_LOCKED;
	return (0);
}
/*
 * Unlock and wake up any waiters.
 */
static void
raidunlock(struct raid_softc *rs)
{

	rs->sc_flags &= ~RAIDF_LOCKED;
	if ((rs->sc_flags & RAIDF_WANTED) != 0) {
		rs->sc_flags &= ~RAIDF_WANTED;
		wakeup(rs);
	}
}


#define RF_COMPONENT_INFO_OFFSET  16384 /* bytes */
#define RF_COMPONENT_INFO_SIZE     1024 /* bytes */
#define RF_PARITY_MAP_SIZE   RF_PARITYMAP_NBYTE

static daddr_t
rf_component_info_offset(void)
{

	return RF_COMPONENT_INFO_OFFSET;
}

static daddr_t
rf_component_info_size(unsigned secsize)
{
	daddr_t info_size;

	KASSERT(secsize);
	if (secsize > RF_COMPONENT_INFO_SIZE)
		info_size = secsize;
	else
		info_size = RF_COMPONENT_INFO_SIZE;

	return info_size;
}

static daddr_t
rf_parity_map_offset(RF_Raid_t *raidPtr)
{
	daddr_t map_offset;

	KASSERT(raidPtr->bytesPerSector);
	if (raidPtr->bytesPerSector > RF_COMPONENT_INFO_SIZE)
		map_offset = raidPtr->bytesPerSector;
	else
		map_offset = RF_COMPONENT_INFO_SIZE;
	map_offset += rf_component_info_offset();

	return map_offset;
}

static daddr_t
rf_parity_map_size(RF_Raid_t *raidPtr)
{
	daddr_t map_size;

	if (raidPtr->bytesPerSector > RF_PARITY_MAP_SIZE)
		map_size = raidPtr->bytesPerSector;
	else
		map_size = RF_PARITY_MAP_SIZE;

	return map_size;
}

int
raidmarkclean(RF_Raid_t *raidPtr, RF_RowCol_t col)
{
	RF_ComponentLabel_t *clabel;

	clabel = raidget_component_label(raidPtr, col);
	clabel->clean = RF_RAID_CLEAN;
	raidflush_component_label(raidPtr, col);
	return(0);
}


int
raidmarkdirty(RF_Raid_t *raidPtr, RF_RowCol_t col)
{
	RF_ComponentLabel_t *clabel;

	clabel = raidget_component_label(raidPtr, col);
	clabel->clean = RF_RAID_DIRTY;
	raidflush_component_label(raidPtr, col);
	return(0);
}

int
raidfetch_component_label(RF_Raid_t *raidPtr, RF_RowCol_t col)
{
	KASSERT(raidPtr->bytesPerSector);
	return raidread_component_label(raidPtr->bytesPerSector,
	    raidPtr->Disks[col].dev,
	    raidPtr->raid_cinfo[col].ci_vp, 
	    &raidPtr->raid_cinfo[col].ci_label);
}

RF_ComponentLabel_t *
raidget_component_label(RF_Raid_t *raidPtr, RF_RowCol_t col)
{
	return &raidPtr->raid_cinfo[col].ci_label;
}

int
raidflush_component_label(RF_Raid_t *raidPtr, RF_RowCol_t col)
{
	RF_ComponentLabel_t *label;

	label = &raidPtr->raid_cinfo[col].ci_label;
	label->mod_counter = raidPtr->mod_counter;
#ifndef RF_NO_PARITY_MAP
	label->parity_map_modcount = label->mod_counter;
#endif
	return raidwrite_component_label(raidPtr->bytesPerSector,
	    raidPtr->Disks[col].dev,
	    raidPtr->raid_cinfo[col].ci_vp, label);
}


static int
raidread_component_label(unsigned secsize, dev_t dev, struct vnode *b_vp,
    RF_ComponentLabel_t *clabel)
{
	return raidread_component_area(dev, b_vp, clabel, 
	    sizeof(RF_ComponentLabel_t),
	    rf_component_info_offset(),
	    rf_component_info_size(secsize));
}

/* ARGSUSED */
static int
raidread_component_area(dev_t dev, struct vnode *b_vp, void *data,
    size_t msize, daddr_t offset, daddr_t dsize)
{
	struct buf *bp;
	const struct bdevsw *bdev;
	int error;

	/* XXX should probably ensure that we don't try to do this if
	   someone has changed rf_protected_sectors. */

	if (b_vp == NULL) {
		/* For whatever reason, this component is not valid.
		   Don't try to read a component label from it. */
		return(EINVAL);
	}

	/* get a block of the appropriate size... */
	bp = geteblk((int)dsize);
	bp->b_dev = dev;

	/* get our ducks in a row for the read */
	bp->b_blkno = offset / DEV_BSIZE;
	bp->b_bcount = dsize;
	bp->b_flags |= B_READ;
 	bp->b_resid = dsize;

	bdev = bdevsw_lookup(bp->b_dev);
	if (bdev == NULL)
		return (ENXIO);
	(*bdev->d_strategy)(bp);

	error = biowait(bp);

	if (!error) {
		memcpy(data, bp->b_data, msize);
	}

	brelse(bp, 0);
	return(error);
}


static int
raidwrite_component_label(unsigned secsize, dev_t dev, struct vnode *b_vp,
    RF_ComponentLabel_t *clabel)
{
	return raidwrite_component_area(dev, b_vp, clabel,
	    sizeof(RF_ComponentLabel_t),
	    rf_component_info_offset(),
	    rf_component_info_size(secsize), 0);
}

/* ARGSUSED */
static int
raidwrite_component_area(dev_t dev, struct vnode *b_vp, void *data, 
    size_t msize, daddr_t offset, daddr_t dsize, int asyncp)
{
	struct buf *bp;
	const struct bdevsw *bdev;
	int error;

	/* get a block of the appropriate size... */
	bp = geteblk((int)dsize);
	bp->b_dev = dev;

	/* get our ducks in a row for the write */
	bp->b_blkno = offset / DEV_BSIZE;
	bp->b_bcount = dsize;
	bp->b_flags |= B_WRITE | (asyncp ? B_ASYNC : 0);
 	bp->b_resid = dsize;

	memset(bp->b_data, 0, dsize);
	memcpy(bp->b_data, data, msize);

	bdev = bdevsw_lookup(bp->b_dev);
	if (bdev == NULL)
		return (ENXIO);
	(*bdev->d_strategy)(bp);
	if (asyncp)
		return 0;
	error = biowait(bp);
	brelse(bp, 0);
	if (error) {
#if 1
		printf("Failed to write RAID component info!\n");
#endif
	}

	return(error);
}

void
rf_paritymap_kern_write(RF_Raid_t *raidPtr, struct rf_paritymap_ondisk *map)
{
	int c;

	for (c = 0; c < raidPtr->numCol; c++) {
		/* Skip dead disks. */
		if (RF_DEAD_DISK(raidPtr->Disks[c].status))
			continue;
		/* XXXjld: what if an error occurs here? */
		raidwrite_component_area(raidPtr->Disks[c].dev,
		    raidPtr->raid_cinfo[c].ci_vp, map,
		    RF_PARITYMAP_NBYTE,
		    rf_parity_map_offset(raidPtr),
		    rf_parity_map_size(raidPtr), 0);
	}
}

void
rf_paritymap_kern_read(RF_Raid_t *raidPtr, struct rf_paritymap_ondisk *map)
{
	struct rf_paritymap_ondisk tmp;
	int c,first;

	first=1;
	for (c = 0; c < raidPtr->numCol; c++) {
		/* Skip dead disks. */
		if (RF_DEAD_DISK(raidPtr->Disks[c].status))
			continue;
		raidread_component_area(raidPtr->Disks[c].dev,
		    raidPtr->raid_cinfo[c].ci_vp, &tmp,
		    RF_PARITYMAP_NBYTE,
		    rf_parity_map_offset(raidPtr),
		    rf_parity_map_size(raidPtr));
		if (first) {
			memcpy(map, &tmp, sizeof(*map));
			first = 0;
		} else {
			rf_paritymap_merge(map, &tmp);
		}
	}
}

void
rf_markalldirty(RF_Raid_t *raidPtr)
{
	RF_ComponentLabel_t *clabel;
	int sparecol;
	int c;
	int j;
	int scol = -1;

	raidPtr->mod_counter++;
	for (c = 0; c < raidPtr->numCol; c++) {
		/* we don't want to touch (at all) a disk that has
		   failed */
		if (!RF_DEAD_DISK(raidPtr->Disks[c].status)) {
			clabel = raidget_component_label(raidPtr, c);
			if (clabel->status == rf_ds_spared) {
				/* XXX do something special...
				   but whatever you do, don't
				   try to access it!! */
			} else {
				raidmarkdirty(raidPtr, c);
			}
		}
	}

	for( c = 0; c < raidPtr->numSpare ; c++) {
		sparecol = raidPtr->numCol + c;
		if (raidPtr->Disks[sparecol].status == rf_ds_used_spare) {
			/*

			   we claim this disk is "optimal" if it's
			   rf_ds_used_spare, as that means it should be
			   directly substitutable for the disk it replaced.
			   We note that too...

			 */

			for(j=0;j<raidPtr->numCol;j++) {
				if (raidPtr->Disks[j].spareCol == sparecol) {
					scol = j;
					break;
				}
			}

			clabel = raidget_component_label(raidPtr, sparecol);
			/* make sure status is noted */

			raid_init_component_label(raidPtr, clabel);

			clabel->row = 0;
			clabel->column = scol;
			/* Note: we *don't* change status from rf_ds_used_spare
			   to rf_ds_optimal */
			/* clabel.status = rf_ds_optimal; */

			raidmarkdirty(raidPtr, sparecol);
		}
	}
}


void
rf_update_component_labels(RF_Raid_t *raidPtr, int final)
{
	RF_ComponentLabel_t *clabel;
	int sparecol;
	int c;
	int j;
	int scol;

	scol = -1;

	/* XXX should do extra checks to make sure things really are clean,
	   rather than blindly setting the clean bit... */

	raidPtr->mod_counter++;

	for (c = 0; c < raidPtr->numCol; c++) {
		if (raidPtr->Disks[c].status == rf_ds_optimal) {
			clabel = raidget_component_label(raidPtr, c);
			/* make sure status is noted */
			clabel->status = rf_ds_optimal;
			
			/* note what unit we are configured as */
			clabel->last_unit = raidPtr->raidid;

			raidflush_component_label(raidPtr, c);
			if (final == RF_FINAL_COMPONENT_UPDATE) {
				if (raidPtr->parity_good == RF_RAID_CLEAN) {
					raidmarkclean(raidPtr, c);
				}
			}
		}
		/* else we don't touch it.. */
	}

	for( c = 0; c < raidPtr->numSpare ; c++) {
		sparecol = raidPtr->numCol + c;
		/* Need to ensure that the reconstruct actually completed! */
		if (raidPtr->Disks[sparecol].status == rf_ds_used_spare) {
			/*

			   we claim this disk is "optimal" if it's
			   rf_ds_used_spare, as that means it should be
			   directly substitutable for the disk it replaced.
			   We note that too...

			 */

			for(j=0;j<raidPtr->numCol;j++) {
				if (raidPtr->Disks[j].spareCol == sparecol) {
					scol = j;
					break;
				}
			}

			/* XXX shouldn't *really* need this... */
			clabel = raidget_component_label(raidPtr, sparecol);
			/* make sure status is noted */

			raid_init_component_label(raidPtr, clabel);

			clabel->column = scol;
			clabel->status = rf_ds_optimal;
			clabel->last_unit = raidPtr->raidid;

			raidflush_component_label(raidPtr, sparecol);
			if (final == RF_FINAL_COMPONENT_UPDATE) {
				if (raidPtr->parity_good == RF_RAID_CLEAN) {
					raidmarkclean(raidPtr, sparecol);
				}
			}
		}
	}
}

void
rf_close_component(RF_Raid_t *raidPtr, struct vnode *vp, int auto_configured)
{

	if (vp != NULL) {
		if (auto_configured == 1) {
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
			VOP_CLOSE(vp, FREAD | FWRITE, NOCRED);
			vput(vp);

		} else {
			(void) vn_close(vp, FREAD | FWRITE, curlwp->l_cred);
		}
	}
}


void
rf_UnconfigureVnodes(RF_Raid_t *raidPtr)
{
	int r,c;
	struct vnode *vp;
	int acd;


	/* We take this opportunity to close the vnodes like we should.. */

	for (c = 0; c < raidPtr->numCol; c++) {
		vp = raidPtr->raid_cinfo[c].ci_vp;
		acd = raidPtr->Disks[c].auto_configured;
		rf_close_component(raidPtr, vp, acd);
		raidPtr->raid_cinfo[c].ci_vp = NULL;
		raidPtr->Disks[c].auto_configured = 0;
	}

	for (r = 0; r < raidPtr->numSpare; r++) {
		vp = raidPtr->raid_cinfo[raidPtr->numCol + r].ci_vp;
		acd = raidPtr->Disks[raidPtr->numCol + r].auto_configured;
		rf_close_component(raidPtr, vp, acd);
		raidPtr->raid_cinfo[raidPtr->numCol + r].ci_vp = NULL;
		raidPtr->Disks[raidPtr->numCol + r].auto_configured = 0;
	}
}


void
rf_ReconThread(struct rf_recon_req *req)
{
	int     s;
	RF_Raid_t *raidPtr;

	s = splbio();
	raidPtr = (RF_Raid_t *) req->raidPtr;
	raidPtr->recon_in_progress = 1;

	rf_FailDisk((RF_Raid_t *) req->raidPtr, req->col,
		    ((req->flags & RF_FDFLAGS_RECON) ? 1 : 0));

	RF_Free(req, sizeof(*req));

	raidPtr->recon_in_progress = 0;
	splx(s);

	/* That's all... */
	kthread_exit(0);	/* does not return */
}

void
rf_RewriteParityThread(RF_Raid_t *raidPtr)
{
	int retcode;
	int s;

	raidPtr->parity_rewrite_stripes_done = 0;
	raidPtr->parity_rewrite_in_progress = 1;
	s = splbio();
	retcode = rf_RewriteParity(raidPtr);
	splx(s);
	if (retcode) {
		printf("raid%d: Error re-writing parity (%d)!\n",
		    raidPtr->raidid, retcode);
	} else {
		/* set the clean bit!  If we shutdown correctly,
		   the clean bit on each component label will get
		   set */
		raidPtr->parity_good = RF_RAID_CLEAN;
	}
	raidPtr->parity_rewrite_in_progress = 0;

	/* Anyone waiting for us to stop?  If so, inform them... */
	if (raidPtr->waitShutdown) {
		wakeup(&raidPtr->parity_rewrite_in_progress);
	}

	/* That's all... */
	kthread_exit(0);	/* does not return */
}


void
rf_CopybackThread(RF_Raid_t *raidPtr)
{
	int s;

	raidPtr->copyback_in_progress = 1;
	s = splbio();
	rf_CopybackReconstructedData(raidPtr);
	splx(s);
	raidPtr->copyback_in_progress = 0;

	/* That's all... */
	kthread_exit(0);	/* does not return */
}


void
rf_ReconstructInPlaceThread(struct rf_recon_req *req)
{
	int s;
	RF_Raid_t *raidPtr;

	s = splbio();
	raidPtr = req->raidPtr;
	raidPtr->recon_in_progress = 1;
	rf_ReconstructInPlace(raidPtr, req->col);
	RF_Free(req, sizeof(*req));
	raidPtr->recon_in_progress = 0;
	splx(s);

	/* That's all... */
	kthread_exit(0);	/* does not return */
}

static RF_AutoConfig_t *
rf_get_component(RF_AutoConfig_t *ac_list, dev_t dev, struct vnode *vp,
    const char *cname, RF_SectorCount_t size, uint64_t numsecs,
    unsigned secsize)
{
	int good_one = 0;
	RF_ComponentLabel_t *clabel; 
	RF_AutoConfig_t *ac;

	clabel = malloc(sizeof(RF_ComponentLabel_t), M_RAIDFRAME, M_NOWAIT);
	if (clabel == NULL) {
oomem:
		    while(ac_list) {
			    ac = ac_list;
			    if (ac->clabel)
				    free(ac->clabel, M_RAIDFRAME);
			    ac_list = ac_list->next;
			    free(ac, M_RAIDFRAME);
		    }
		    printf("RAID auto config: out of memory!\n");
		    return NULL; /* XXX probably should panic? */
	}

	if (!raidread_component_label(secsize, dev, vp, clabel)) {
		/* Got the label.  Does it look reasonable? */
		if (rf_reasonable_label(clabel, numsecs) && 
		    (rf_component_label_partitionsize(clabel) <= size)) {
#ifdef DEBUG
			printf("Component on: %s: %llu\n",
				cname, (unsigned long long)size);
			rf_print_component_label(clabel);
#endif
			/* if it's reasonable, add it, else ignore it. */
			ac = malloc(sizeof(RF_AutoConfig_t), M_RAIDFRAME,
				M_NOWAIT);
			if (ac == NULL) {
				free(clabel, M_RAIDFRAME);
				goto oomem;
			}
			strlcpy(ac->devname, cname, sizeof(ac->devname));
			ac->dev = dev;
			ac->vp = vp;
			ac->clabel = clabel;
			ac->next = ac_list;
			ac_list = ac;
			good_one = 1;
		}
	}
	if (!good_one) {
		/* cleanup */
		free(clabel, M_RAIDFRAME);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		VOP_CLOSE(vp, FREAD | FWRITE, NOCRED);
		vput(vp);
	}
	return ac_list;
}

RF_AutoConfig_t *
rf_find_raid_components(void)
{
	struct vnode *vp;
	struct disklabel label;
	device_t dv;
	deviter_t di;
	dev_t dev;
	int bmajor, bminor, wedge, rf_part_found;
	int error;
	int i;
	RF_AutoConfig_t *ac_list;
	uint64_t numsecs;
	unsigned secsize;

	/* initialize the AutoConfig list */
	ac_list = NULL;

	/* we begin by trolling through *all* the devices on the system */

	for (dv = deviter_first(&di, DEVITER_F_ROOT_FIRST); dv != NULL;
	     dv = deviter_next(&di)) {

		/* we are only interested in disks... */
		if (device_class(dv) != DV_DISK)
			continue;

		/* we don't care about floppies... */
		if (device_is_a(dv, "fd")) {
			continue;
		}

		/* we don't care about CD's... */
		if (device_is_a(dv, "cd")) {
			continue;
		}

		/* we don't care about md's... */
		if (device_is_a(dv, "md")) {
			continue;
		}

		/* hdfd is the Atari/Hades floppy driver */
		if (device_is_a(dv, "hdfd")) {
			continue;
		}

		/* fdisa is the Atari/Milan floppy driver */
		if (device_is_a(dv, "fdisa")) {
			continue;
		}

		/* need to find the device_name_to_block_device_major stuff */
		bmajor = devsw_name2blk(device_xname(dv), NULL, 0);

		rf_part_found = 0; /*No raid partition as yet*/

		/* get a vnode for the raw partition of this disk */

		wedge = device_is_a(dv, "dk");
		bminor = minor(device_unit(dv));
		dev = wedge ? makedev(bmajor, bminor) :
		    MAKEDISKDEV(bmajor, bminor, RAW_PART);
		if (bdevvp(dev, &vp))
			panic("RAID can't alloc vnode");

		error = VOP_OPEN(vp, FREAD | FSILENT, NOCRED);

		if (error) {
			/* "Who cares."  Continue looking
			   for something that exists*/
			vput(vp);
			continue;
		}

		error = getdisksize(vp, &numsecs, &secsize);
		if (error) {
			vput(vp);
			continue;
		}
		if (wedge) {
			struct dkwedge_info dkw;
			error = VOP_IOCTL(vp, DIOCGWEDGEINFO, &dkw, FREAD,
			    NOCRED);
			if (error) {
				printf("RAIDframe: can't get wedge info for "
				    "dev %s (%d)\n", device_xname(dv), error);
				vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
				VOP_CLOSE(vp, FREAD | FWRITE, NOCRED);
				vput(vp);
				continue;
			}

			if (strcmp(dkw.dkw_ptype, DKW_PTYPE_RAIDFRAME) != 0) {
				vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
				VOP_CLOSE(vp, FREAD | FWRITE, NOCRED);
				vput(vp);
				continue;
			}
				
			ac_list = rf_get_component(ac_list, dev, vp,
			    device_xname(dv), dkw.dkw_size, numsecs, secsize);
			rf_part_found = 1; /*There is a raid component on this disk*/
			continue;
		}

		/* Ok, the disk exists.  Go get the disklabel. */
		error = VOP_IOCTL(vp, DIOCGDINFO, &label, FREAD, NOCRED);
		if (error) {
			/*
			 * XXX can't happen - open() would
			 * have errored out (or faked up one)
			 */
			if (error != ENOTTY)
				printf("RAIDframe: can't get label for dev "
				    "%s (%d)\n", device_xname(dv), error);
		}

		/* don't need this any more.  We'll allocate it again
		   a little later if we really do... */
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		VOP_CLOSE(vp, FREAD | FWRITE, NOCRED);
		vput(vp);

		if (error)
			continue;

		rf_part_found = 0; /*No raid partitions yet*/
		for (i = 0; i < label.d_npartitions; i++) {
			char cname[sizeof(ac_list->devname)];

			/* We only support partitions marked as RAID */
			if (label.d_partitions[i].p_fstype != FS_RAID)
				continue;

			dev = MAKEDISKDEV(bmajor, device_unit(dv), i);
			if (bdevvp(dev, &vp))
				panic("RAID can't alloc vnode");

			error = VOP_OPEN(vp, FREAD, NOCRED);
			if (error) {
				/* Whatever... */
				vput(vp);
				continue;
			}
			snprintf(cname, sizeof(cname), "%s%c",
			    device_xname(dv), 'a' + i);
			ac_list = rf_get_component(ac_list, dev, vp, cname,
				label.d_partitions[i].p_size, numsecs, secsize);
				rf_part_found = 1; /*There is at least one raid partition on this disk*/
		}

		/*
		 *If there is no raid component on this disk, either in a
		 *disklabel or inside a wedge, check the raw partition as well,
		 *as it is possible to configure raid components on raw disk
		 *devices.
		 */

		if (!rf_part_found) {
			char cname[sizeof(ac_list->devname)];

			dev = MAKEDISKDEV(bmajor, device_unit(dv), RAW_PART);
			if (bdevvp(dev, &vp))
				panic("RAID can't alloc vnode");

			error = VOP_OPEN(vp, FREAD, NOCRED);
			if (error) {
				/* Whatever... */
				vput(vp);
				continue;
			}
			snprintf(cname, sizeof(cname), "%s%c",
			    device_xname(dv), 'a' + RAW_PART);
			ac_list = rf_get_component(ac_list, dev, vp, cname,
				label.d_partitions[RAW_PART].p_size, numsecs, secsize);
		}
	}
	deviter_release(&di);
	return ac_list;
}


int
rf_reasonable_label(RF_ComponentLabel_t *clabel, uint64_t numsecs)
{

	if (((clabel->version==RF_COMPONENT_LABEL_VERSION_1) ||
	     (clabel->version==RF_COMPONENT_LABEL_VERSION)) &&
	    ((clabel->clean == RF_RAID_CLEAN) ||
	     (clabel->clean == RF_RAID_DIRTY)) &&
	    clabel->row >=0 &&
	    clabel->column >= 0 &&
	    clabel->num_rows > 0 &&
	    clabel->num_columns > 0 &&
	    clabel->row < clabel->num_rows &&
	    clabel->column < clabel->num_columns &&
	    clabel->blockSize > 0 &&
	    /*
	     * numBlocksHi may contain garbage, but it is ok since
	     * the type is unsigned.  If it is really garbage,
	     * rf_fix_old_label_size() will fix it.
	     */
	    rf_component_label_numblocks(clabel) > 0) {
		/*
		 * label looks reasonable enough...
		 * let's make sure it has no old garbage.
		 */
		if (numsecs)
			rf_fix_old_label_size(clabel, numsecs);
		return(1);
	}
	return(0);
}


/*
 * For reasons yet unknown, some old component labels have garbage in
 * the newer numBlocksHi region, and this causes lossage.  Since those
 * disks will also have numsecs set to less than 32 bits of sectors,
 * we can determine when this corruption has occurred, and fix it.
 *
 * The exact same problem, with the same unknown reason, happens to
 * the partitionSizeHi member as well.
 */
static void
rf_fix_old_label_size(RF_ComponentLabel_t *clabel, uint64_t numsecs)
{

	if (numsecs < ((uint64_t)1 << 32)) {
		if (clabel->numBlocksHi) {
			printf("WARNING: total sectors < 32 bits, yet "
			       "numBlocksHi set\n"
			       "WARNING: resetting numBlocksHi to zero.\n");
			clabel->numBlocksHi = 0;
		}

		if (clabel->partitionSizeHi) {
			printf("WARNING: total sectors < 32 bits, yet "
			       "partitionSizeHi set\n"
			       "WARNING: resetting partitionSizeHi to zero.\n");
			clabel->partitionSizeHi = 0;
		}
	}
}


#ifdef DEBUG
void
rf_print_component_label(RF_ComponentLabel_t *clabel)
{
	uint64_t numBlocks;
	static const char *rp[] = {
	    "No", "Force", "Soft", "*invalid*"
	};


	numBlocks = rf_component_label_numblocks(clabel);

	printf("   Row: %d Column: %d Num Rows: %d Num Columns: %d\n",
	       clabel->row, clabel->column,
	       clabel->num_rows, clabel->num_columns);
	printf("   Version: %d Serial Number: %d Mod Counter: %d\n",
	       clabel->version, clabel->serial_number,
	       clabel->mod_counter);
	printf("   Clean: %s Status: %d\n",
	       clabel->clean ? "Yes" : "No", clabel->status);
	printf("   sectPerSU: %d SUsPerPU: %d SUsPerRU: %d\n",
	       clabel->sectPerSU, clabel->SUsPerPU, clabel->SUsPerRU);
	printf("   RAID Level: %c  blocksize: %d numBlocks: %"PRIu64"\n",
	       (char) clabel->parityConfig, clabel->blockSize, numBlocks);
	printf("   Autoconfig: %s\n", clabel->autoconfigure ? "Yes" : "No");
	printf("   Root partition: %s\n", rp[clabel->root_partition & 3]);
	printf("   Last configured as: raid%d\n", clabel->last_unit);
#if 0
	   printf("   Config order: %d\n", clabel->config_order);
#endif

}
#endif

RF_ConfigSet_t *
rf_create_auto_sets(RF_AutoConfig_t *ac_list)
{
	RF_AutoConfig_t *ac;
	RF_ConfigSet_t *config_sets;
	RF_ConfigSet_t *cset;
	RF_AutoConfig_t *ac_next;


	config_sets = NULL;

	/* Go through the AutoConfig list, and figure out which components
	   belong to what sets.  */
	ac = ac_list;
	while(ac!=NULL) {
		/* we're going to putz with ac->next, so save it here
		   for use at the end of the loop */
		ac_next = ac->next;

		if (config_sets == NULL) {
			/* will need at least this one... */
			config_sets = (RF_ConfigSet_t *)
				malloc(sizeof(RF_ConfigSet_t),
				       M_RAIDFRAME, M_NOWAIT);
			if (config_sets == NULL) {
				panic("rf_create_auto_sets: No memory!");
			}
			/* this one is easy :) */
			config_sets->ac = ac;
			config_sets->next = NULL;
			config_sets->rootable = 0;
			ac->next = NULL;
		} else {
			/* which set does this component fit into? */
			cset = config_sets;
			while(cset!=NULL) {
				if (rf_does_it_fit(cset, ac)) {
					/* looks like it matches... */
					ac->next = cset->ac;
					cset->ac = ac;
					break;
				}
				cset = cset->next;
			}
			if (cset==NULL) {
				/* didn't find a match above... new set..*/
				cset = (RF_ConfigSet_t *)
					malloc(sizeof(RF_ConfigSet_t),
					       M_RAIDFRAME, M_NOWAIT);
				if (cset == NULL) {
					panic("rf_create_auto_sets: No memory!");
				}
				cset->ac = ac;
				ac->next = NULL;
				cset->next = config_sets;
				cset->rootable = 0;
				config_sets = cset;
			}
		}
		ac = ac_next;
	}


	return(config_sets);
}

static int
rf_does_it_fit(RF_ConfigSet_t *cset, RF_AutoConfig_t *ac)
{
	RF_ComponentLabel_t *clabel1, *clabel2;

	/* If this one matches the *first* one in the set, that's good
	   enough, since the other members of the set would have been
	   through here too... */
	/* note that we are not checking partitionSize here..

	   Note that we are also not checking the mod_counters here.
	   If everything else matches except the mod_counter, that's
	   good enough for this test.  We will deal with the mod_counters
	   a little later in the autoconfiguration process.

	    (clabel1->mod_counter == clabel2->mod_counter) &&

	   The reason we don't check for this is that failed disks
	   will have lower modification counts.  If those disks are
	   not added to the set they used to belong to, then they will
	   form their own set, which may result in 2 different sets,
	   for example, competing to be configured at raid0, and
	   perhaps competing to be the root filesystem set.  If the
	   wrong ones get configured, or both attempt to become /,
	   weird behaviour and or serious lossage will occur.  Thus we
	   need to bring them into the fold here, and kick them out at
	   a later point.

	*/

	clabel1 = cset->ac->clabel;
	clabel2 = ac->clabel;
	if ((clabel1->version == clabel2->version) &&
	    (clabel1->serial_number == clabel2->serial_number) &&
	    (clabel1->num_rows == clabel2->num_rows) &&
	    (clabel1->num_columns == clabel2->num_columns) &&
	    (clabel1->sectPerSU == clabel2->sectPerSU) &&
	    (clabel1->SUsPerPU == clabel2->SUsPerPU) &&
	    (clabel1->SUsPerRU == clabel2->SUsPerRU) &&
	    (clabel1->parityConfig == clabel2->parityConfig) &&
	    (clabel1->maxOutstanding == clabel2->maxOutstanding) &&
	    (clabel1->blockSize == clabel2->blockSize) &&
	    rf_component_label_numblocks(clabel1) ==
	    rf_component_label_numblocks(clabel2) &&
	    (clabel1->autoconfigure == clabel2->autoconfigure) &&
	    (clabel1->root_partition == clabel2->root_partition) &&
	    (clabel1->last_unit == clabel2->last_unit) &&
	    (clabel1->config_order == clabel2->config_order)) {
		/* if it get's here, it almost *has* to be a match */
	} else {
		/* it's not consistent with somebody in the set..
		   punt */
		return(0);
	}
	/* all was fine.. it must fit... */
	return(1);
}

int
rf_have_enough_components(RF_ConfigSet_t *cset)
{
	RF_AutoConfig_t *ac;
	RF_AutoConfig_t *auto_config;
	RF_ComponentLabel_t *clabel;
	int c;
	int num_cols;
	int num_missing;
	int mod_counter;
	int mod_counter_found;
	int even_pair_failed;
	char parity_type;


	/* check to see that we have enough 'live' components
	   of this set.  If so, we can configure it if necessary */

	num_cols = cset->ac->clabel->num_columns;
	parity_type = cset->ac->clabel->parityConfig;

	/* XXX Check for duplicate components!?!?!? */

	/* Determine what the mod_counter is supposed to be for this set. */

	mod_counter_found = 0;
	mod_counter = 0;
	ac = cset->ac;
	while(ac!=NULL) {
		if (mod_counter_found==0) {
			mod_counter = ac->clabel->mod_counter;
			mod_counter_found = 1;
		} else {
			if (ac->clabel->mod_counter > mod_counter) {
				mod_counter = ac->clabel->mod_counter;
			}
		}
		ac = ac->next;
	}

	num_missing = 0;
	auto_config = cset->ac;

	even_pair_failed = 0;
	for(c=0; c<num_cols; c++) {
		ac = auto_config;
		while(ac!=NULL) {
			if ((ac->clabel->column == c) &&
			    (ac->clabel->mod_counter == mod_counter)) {
				/* it's this one... */
#ifdef DEBUG
				printf("Found: %s at %d\n",
				       ac->devname,c);
#endif
				break;
			}
			ac=ac->next;
		}
		if (ac==NULL) {
				/* Didn't find one here! */
				/* special case for RAID 1, especially
				   where there are more than 2
				   components (where RAIDframe treats
				   things a little differently :( ) */
			if (parity_type == '1') {
				if (c%2 == 0) { /* even component */
					even_pair_failed = 1;
				} else { /* odd component.  If
					    we're failed, and
					    so is the even
					    component, it's
					    "Good Night, Charlie" */
					if (even_pair_failed == 1) {
						return(0);
					}
				}
			} else {
				/* normal accounting */
				num_missing++;
			}
		}
		if ((parity_type == '1') && (c%2 == 1)) {
				/* Just did an even component, and we didn't
				   bail.. reset the even_pair_failed flag,
				   and go on to the next component.... */
			even_pair_failed = 0;
		}
	}

	clabel = cset->ac->clabel;

	if (((clabel->parityConfig == '0') && (num_missing > 0)) ||
	    ((clabel->parityConfig == '4') && (num_missing > 1)) ||
	    ((clabel->parityConfig == '5') && (num_missing > 1))) {
		/* XXX this needs to be made *much* more general */
		/* Too many failures */
		return(0);
	}
	/* otherwise, all is well, and we've got enough to take a kick
	   at autoconfiguring this set */
	return(1);
}

void
rf_create_configuration(RF_AutoConfig_t *ac, RF_Config_t *config,
			RF_Raid_t *raidPtr)
{
	RF_ComponentLabel_t *clabel;
	int i;

	clabel = ac->clabel;

	/* 1. Fill in the common stuff */
	config->numRow = clabel->num_rows = 1;
	config->numCol = clabel->num_columns;
	config->numSpare = 0; /* XXX should this be set here? */
	config->sectPerSU = clabel->sectPerSU;
	config->SUsPerPU = clabel->SUsPerPU;
	config->SUsPerRU = clabel->SUsPerRU;
	config->parityConfig = clabel->parityConfig;
	/* XXX... */
	strcpy(config->diskQueueType,"fifo");
	config->maxOutstandingDiskReqs = clabel->maxOutstanding;
	config->layoutSpecificSize = 0; /* XXX ?? */

	while(ac!=NULL) {
		/* row/col values will be in range due to the checks
		   in reasonable_label() */
		strcpy(config->devnames[0][ac->clabel->column],
		       ac->devname);
		ac = ac->next;
	}

	for(i=0;i<RF_MAXDBGV;i++) {
		config->debugVars[i][0] = 0;
	}
}

int
rf_set_autoconfig(RF_Raid_t *raidPtr, int new_value)
{
	RF_ComponentLabel_t *clabel;
	int column;
	int sparecol;

	raidPtr->autoconfigure = new_value;

	for(column=0; column<raidPtr->numCol; column++) {
		if (raidPtr->Disks[column].status == rf_ds_optimal) {
			clabel = raidget_component_label(raidPtr, column);
			clabel->autoconfigure = new_value;
			raidflush_component_label(raidPtr, column);
		}
	}
	for(column = 0; column < raidPtr->numSpare ; column++) {
		sparecol = raidPtr->numCol + column;
		if (raidPtr->Disks[sparecol].status == rf_ds_used_spare) {
			clabel = raidget_component_label(raidPtr, sparecol);
			clabel->autoconfigure = new_value;
			raidflush_component_label(raidPtr, sparecol);
		}
	}
	return(new_value);
}

int
rf_set_rootpartition(RF_Raid_t *raidPtr, int new_value)
{
	RF_ComponentLabel_t *clabel;
	int column;
	int sparecol;

	raidPtr->root_partition = new_value;
	for(column=0; column<raidPtr->numCol; column++) {
		if (raidPtr->Disks[column].status == rf_ds_optimal) {
			clabel = raidget_component_label(raidPtr, column);
			clabel->root_partition = new_value;
			raidflush_component_label(raidPtr, column);
		}
	}
	for(column = 0; column < raidPtr->numSpare ; column++) {
		sparecol = raidPtr->numCol + column;
		if (raidPtr->Disks[sparecol].status == rf_ds_used_spare) {
			clabel = raidget_component_label(raidPtr, sparecol);
			clabel->root_partition = new_value;
			raidflush_component_label(raidPtr, sparecol);
		}
	}
	return(new_value);
}

void
rf_release_all_vps(RF_ConfigSet_t *cset)
{
	RF_AutoConfig_t *ac;

	ac = cset->ac;
	while(ac!=NULL) {
		/* Close the vp, and give it back */
		if (ac->vp) {
			vn_lock(ac->vp, LK_EXCLUSIVE | LK_RETRY);
			VOP_CLOSE(ac->vp, FREAD, NOCRED);
			vput(ac->vp);
			ac->vp = NULL;
		}
		ac = ac->next;
	}
}


void
rf_cleanup_config_set(RF_ConfigSet_t *cset)
{
	RF_AutoConfig_t *ac;
	RF_AutoConfig_t *next_ac;

	ac = cset->ac;
	while(ac!=NULL) {
		next_ac = ac->next;
		/* nuke the label */
		free(ac->clabel, M_RAIDFRAME);
		/* cleanup the config structure */
		free(ac, M_RAIDFRAME);
		/* "next.." */
		ac = next_ac;
	}
	/* and, finally, nuke the config set */
	free(cset, M_RAIDFRAME);
}


void
raid_init_component_label(RF_Raid_t *raidPtr, RF_ComponentLabel_t *clabel)
{
	/* current version number */
	clabel->version = RF_COMPONENT_LABEL_VERSION;
	clabel->serial_number = raidPtr->serial_number;
	clabel->mod_counter = raidPtr->mod_counter;

	clabel->num_rows = 1;
	clabel->num_columns = raidPtr->numCol;
	clabel->clean = RF_RAID_DIRTY; /* not clean */
	clabel->status = rf_ds_optimal; /* "It's good!" */

	clabel->sectPerSU = raidPtr->Layout.sectorsPerStripeUnit;
	clabel->SUsPerPU = raidPtr->Layout.SUsPerPU;
	clabel->SUsPerRU = raidPtr->Layout.SUsPerRU;

	clabel->blockSize = raidPtr->bytesPerSector;
	rf_component_label_set_numblocks(clabel, raidPtr->sectorsPerDisk);

	/* XXX not portable */
	clabel->parityConfig = raidPtr->Layout.map->parityConfig;
	clabel->maxOutstanding = raidPtr->maxOutstanding;
	clabel->autoconfigure = raidPtr->autoconfigure;
	clabel->root_partition = raidPtr->root_partition;
	clabel->last_unit = raidPtr->raidid;
	clabel->config_order = raidPtr->config_order;

#ifndef RF_NO_PARITY_MAP
	rf_paritymap_init_label(raidPtr->parity_map, clabel);
#endif
}

struct raid_softc *
rf_auto_config_set(RF_ConfigSet_t *cset)
{
	RF_Raid_t *raidPtr;
	RF_Config_t *config;
	int raidID;
	struct raid_softc *sc;

#ifdef DEBUG
	printf("RAID autoconfigure\n");
#endif

	/* 1. Create a config structure */
	config = malloc(sizeof(*config), M_RAIDFRAME, M_NOWAIT|M_ZERO);
	if (config == NULL) {
		printf("Out of mem!?!?\n");
				/* XXX do something more intelligent here. */
		return NULL;
	}

	/*
	   2. Figure out what RAID ID this one is supposed to live at
	   See if we can get the same RAID dev that it was configured
	   on last time..
	*/

	raidID = cset->ac->clabel->last_unit;
	for (sc = raidget(raidID); sc->sc_r.valid != 0; sc = raidget(++raidID))
		continue;
#ifdef DEBUG
	printf("Configuring raid%d:\n",raidID);
#endif

	raidPtr = &sc->sc_r;

	/* XXX all this stuff should be done SOMEWHERE ELSE! */
	raidPtr->softc = sc;
	raidPtr->raidid = raidID;
	raidPtr->openings = RAIDOUTSTANDING;

	/* 3. Build the configuration structure */
	rf_create_configuration(cset->ac, config, raidPtr);

	/* 4. Do the configuration */
	if (rf_Configure(raidPtr, config, cset->ac) == 0) {
		raidinit(sc);

		rf_markalldirty(raidPtr);
		raidPtr->autoconfigure = 1; /* XXX do this here? */
		switch (cset->ac->clabel->root_partition) {
		case 1:	/* Force Root */
		case 2:	/* Soft Root: root when boot partition part of raid */
			/*
			 * everything configured just fine.  Make a note
			 * that this set is eligible to be root,
			 * or forced to be root
			 */
			cset->rootable = cset->ac->clabel->root_partition;
			/* XXX do this here? */
			raidPtr->root_partition = cset->rootable;
			break;
		default:
			break;
		}
	} else {
		raidput(sc);
		sc = NULL;
	}

	/* 5. Cleanup */
	free(config, M_RAIDFRAME);
	return sc;
}

void
rf_disk_unbusy(RF_RaidAccessDesc_t *desc)
{
	struct buf *bp;
	struct raid_softc *rs;

	bp = (struct buf *)desc->bp;
	rs = desc->raidPtr->softc;
	disk_unbusy(&rs->sc_dkdev, (bp->b_bcount - bp->b_resid),
	    (bp->b_flags & B_READ));
}

void
rf_pool_init(struct pool *p, size_t size, const char *w_chan,
	     size_t xmin, size_t xmax)
{
	pool_init(p, size, 0, 0, 0, w_chan, NULL, IPL_BIO);
	pool_sethiwat(p, xmax);
	pool_prime(p, xmin);
	pool_setlowat(p, xmin);
}

/*
 * rf_buf_queue_check(RF_Raid_t raidPtr) -- looks into the buf_queue to see
 * if there is IO pending and if that IO could possibly be done for a
 * given RAID set.  Returns 0 if IO is waiting and can be done, 1
 * otherwise.
 *
 */

int
rf_buf_queue_check(RF_Raid_t *raidPtr)
{
	struct raid_softc *rs = raidPtr->softc;
	if ((bufq_peek(rs->buf_queue) != NULL) && raidPtr->openings > 0) {
		/* there is work to do */
		return 0;
	} 
	/* default is nothing to do */
	return 1;
}

int
rf_getdisksize(struct vnode *vp, RF_RaidDisk_t *diskPtr)
{
	uint64_t numsecs;
	unsigned secsize;
	int error;

	error = getdisksize(vp, &numsecs, &secsize);
	if (error == 0) {
		diskPtr->blockSize = secsize;
		diskPtr->numBlocks = numsecs - rf_protectedSectors;
		diskPtr->partitionSize = numsecs;
		return 0;
	}
	return error;
}

static int
raid_match(device_t self, cfdata_t cfdata, void *aux)
{
	return 1;
}

static void
raid_attach(device_t parent, device_t self, void *aux)
{

}


static int
raid_detach(device_t self, int flags)
{
	int error;
	struct raid_softc *rs = raidget(device_unit(self));

	if (rs == NULL)
		return ENXIO;

	if ((error = raidlock(rs)) != 0)
		return (error);

	error = raid_detach_unlocked(rs);

	raidunlock(rs);

	/* XXXkd: raidput(rs) ??? */

	return error;
}

static void
rf_set_geometry(struct raid_softc *rs, RF_Raid_t *raidPtr)
{
	struct disk_geom *dg = &rs->sc_dkdev.dk_geom;

	memset(dg, 0, sizeof(*dg));

	dg->dg_secperunit = raidPtr->totalSectors;
	dg->dg_secsize = raidPtr->bytesPerSector;
	dg->dg_nsectors = raidPtr->Layout.dataSectorsPerStripe;
	dg->dg_ntracks = 4 * raidPtr->numCol;

	disk_set_info(rs->sc_dev, &rs->sc_dkdev, NULL);
}

/* 
 * Implement forwarding of the DIOCCACHESYNC ioctl to each of the components.
 * We end up returning whatever error was returned by the first cache flush
 * that fails.
 */

int
rf_sync_component_caches(RF_Raid_t *raidPtr)
{
	int c, sparecol;
	int e,error;
	int force = 1;
	
	error = 0;
	for (c = 0; c < raidPtr->numCol; c++) {
		if (raidPtr->Disks[c].status == rf_ds_optimal) {
			e = VOP_IOCTL(raidPtr->raid_cinfo[c].ci_vp, DIOCCACHESYNC, 
					  &force, FWRITE, NOCRED);
			if (e) {
				if (e != ENODEV)
					printf("raid%d: cache flush to component %s failed.\n",
					       raidPtr->raidid, raidPtr->Disks[c].devname);
				if (error == 0) {
					error = e;
				}
			}
		}
	}

	for( c = 0; c < raidPtr->numSpare ; c++) {
		sparecol = raidPtr->numCol + c;
		/* Need to ensure that the reconstruct actually completed! */
		if (raidPtr->Disks[sparecol].status == rf_ds_used_spare) {
			e = VOP_IOCTL(raidPtr->raid_cinfo[sparecol].ci_vp,
					  DIOCCACHESYNC, &force, FWRITE, NOCRED);
			if (e) {
				if (e != ENODEV)
					printf("raid%d: cache flush to component %s failed.\n",
					       raidPtr->raidid, raidPtr->Disks[sparecol].devname);
				if (error == 0) {
					error = e;
				}
			}
		}
	}
	return error;
}

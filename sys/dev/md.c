/*	$NetBSD: md.c,v 1.75 2015/08/20 14:40:17 christos Exp $	*/

/*
 * Copyright (c) 1995 Gordon W. Ross, Leo Weppelman.
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

/*
 * This implements a general-purpose memory-disk.
 * See md.h for notes on the config types.
 *
 * Note that this driver provides the same functionality
 * as the MFS filesystem hack, but this is better because
 * you can use this for any filesystem type you'd like!
 *
 * Credit for most of the kmem ramdisk code goes to:
 *   Leo Weppelman (atari) and Phil Nelson (pc532)
 * Credit for the ideas behind the "user space memory" code goes
 * to the authors of the MFS implementation.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: md.c,v 1.75 2015/08/20 14:40:17 christos Exp $");

#ifdef _KERNEL_OPT
#include "opt_md.h"
#else
#define MEMORY_DISK_SERVER 1
#endif

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/disklabel.h>

#include <uvm/uvm_extern.h>

#include <dev/md.h>

#include "ioconf.h"
/*
 * The user-space functionality is included by default.
 * Use  `options MEMORY_DISK_SERVER=0' to turn it off.
 */
#ifndef MEMORY_DISK_SERVER
#error MEMORY_DISK_SERVER should be defined by opt_md.h
#endif	/* MEMORY_DISK_SERVER */

/*
 * We should use the raw partition for ioctl.
 */
#define MD_UNIT(unit)	DISKUNIT(unit)

/* autoconfig stuff... */

struct md_softc {
	device_t sc_dev;	/* Self. */
	struct disk sc_dkdev;	/* hook for generic disk handling */
	struct md_conf sc_md;
	kmutex_t sc_lock;	/* Protect self. */
	kcondvar_t sc_cv;	/* Wait here for work. */
	struct bufq_state *sc_buflist;
};
/* shorthand for fields in sc_md: */
#define sc_addr sc_md.md_addr
#define sc_size sc_md.md_size
#define sc_type sc_md.md_type

static void	md_attach(device_t, device_t, void *);
static int	md_detach(device_t, int);

static dev_type_open(mdopen);
static dev_type_close(mdclose);
static dev_type_read(mdread);
static dev_type_write(mdwrite);
static dev_type_ioctl(mdioctl);
static dev_type_strategy(mdstrategy);
static dev_type_size(mdsize);

const struct bdevsw md_bdevsw = {
	.d_open = mdopen,
	.d_close = mdclose,
	.d_strategy = mdstrategy,
	.d_ioctl = mdioctl,
	.d_dump = nodump,
	.d_psize = mdsize,
	.d_discard = nodiscard,
	.d_flag = D_DISK | D_MPSAFE
};

const struct cdevsw md_cdevsw = {
	.d_open = mdopen,
	.d_close = mdclose,
	.d_read = mdread,
	.d_write = mdwrite,
	.d_ioctl = mdioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

static struct dkdriver mddkdriver = {
	.d_strategy = mdstrategy
};

extern struct cfdriver md_cd;
CFATTACH_DECL3_NEW(md, sizeof(struct md_softc),
	0, md_attach, md_detach, NULL, NULL, NULL, DVF_DETACH_SHUTDOWN);

static kmutex_t md_device_lock;		/* Protect unit creation / deletion. */
extern size_t md_root_size;

static void md_set_disklabel(struct md_softc *);

/*
 * This is called if we are configured as a pseudo-device
 */
void
mdattach(int n)
{

	mutex_init(&md_device_lock, MUTEX_DEFAULT, IPL_NONE);
	if (config_cfattach_attach(md_cd.cd_name, &md_ca)) {
		aprint_error("%s: cfattach_attach failed\n", md_cd.cd_name);
		return;
	}
}

static void
md_attach(device_t parent, device_t self, void *aux)
{
	struct md_softc *sc = device_private(self);

	sc->sc_dev = self;
	sc->sc_type = MD_UNCONFIGURED;
	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&sc->sc_cv, "mdidle");
	bufq_alloc(&sc->sc_buflist, "fcfs", 0);

	/* XXX - Could accept aux info here to set the config. */
#ifdef	MEMORY_DISK_HOOKS
	/*
	 * This external function might setup a pre-loaded disk.
	 * All it would need to do is setup the md_conf struct.
	 * See sys/dev/md_root.c for an example.
	 */
	md_attach_hook(device_unit(self), &sc->sc_md);
#endif

	/*
	 * Initialize and attach the disk structure.
	 */
	disk_init(&sc->sc_dkdev, device_xname(self), &mddkdriver);
	disk_attach(&sc->sc_dkdev);

	if (sc->sc_type != MD_UNCONFIGURED)
		md_set_disklabel(sc);

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static int
md_detach(device_t self, int flags)
{
	struct md_softc *sc = device_private(self);
	int rc;

	rc = 0;
	mutex_enter(&sc->sc_dkdev.dk_openlock);
	if (sc->sc_dkdev.dk_openmask == 0 && sc->sc_type == MD_UNCONFIGURED)
		;	/* nothing to do */
	else if ((flags & DETACH_FORCE) == 0)
		rc = EBUSY;
	mutex_exit(&sc->sc_dkdev.dk_openlock);

	if (rc != 0)
		return rc;

	pmf_device_deregister(self);
	disk_detach(&sc->sc_dkdev);
	disk_destroy(&sc->sc_dkdev);
	bufq_free(sc->sc_buflist);
	mutex_destroy(&sc->sc_lock);
	cv_destroy(&sc->sc_cv);
	return 0;
}

/*
 * operational routines:
 * open, close, read, write, strategy,
 * ioctl, dump, size
 */

#if MEMORY_DISK_SERVER
static int	md_server_loop(struct md_softc *sc);
static int	md_ioctl_server(struct md_softc *sc, struct md_conf *umd,
		    struct lwp *l);
#endif	/* MEMORY_DISK_SERVER */
static int	md_ioctl_kalloc(struct md_softc *sc, struct md_conf *umd,
		    struct lwp *l);

static int
mdsize(dev_t dev)
{
	struct md_softc *sc;
	int res;

	sc = device_lookup_private(&md_cd, MD_UNIT(dev));
	if (sc == NULL)
		return 0;

	mutex_enter(&sc->sc_lock);
	if (sc->sc_type == MD_UNCONFIGURED)
		res = 0;
	else
		res = sc->sc_size >> DEV_BSHIFT;
	mutex_exit(&sc->sc_lock);

	return res;
}

static int
mdopen(dev_t dev, int flag, int fmt, struct lwp *l)
{
	int unit;
	int part = DISKPART(dev);
	int pmask = 1 << part;
	cfdata_t cf;
	struct md_softc *sc;
	struct disk *dk;
#ifdef	MEMORY_DISK_HOOKS
	bool configured;
#endif

	mutex_enter(&md_device_lock);
	unit = MD_UNIT(dev);
	sc = device_lookup_private(&md_cd, unit);
	if (sc == NULL) {
		if (part != RAW_PART) {
			mutex_exit(&md_device_lock);
			return ENXIO;
		}
		cf = malloc(sizeof(*cf), M_DEVBUF, M_WAITOK);
		cf->cf_name = md_cd.cd_name;
		cf->cf_atname = md_cd.cd_name;
		cf->cf_unit = unit;
		cf->cf_fstate = FSTATE_STAR;
		sc = device_private(config_attach_pseudo(cf));
		if (sc == NULL) {
			mutex_exit(&md_device_lock);
			return ENOMEM;
		}
	}

	dk = &sc->sc_dkdev;

	/*
	 * The raw partition is used for ioctl to configure.
	 */
	if (part == RAW_PART)
		goto ok;

#ifdef	MEMORY_DISK_HOOKS
	/* Call the open hook to allow loading the device. */
	configured = (sc->sc_type != MD_UNCONFIGURED);
	md_open_hook(unit, &sc->sc_md);
	/* initialize disklabel if the device is configured in open hook */
	if (!configured && sc->sc_type != MD_UNCONFIGURED)
		md_set_disklabel(sc);
#endif

	/*
	 * This is a normal, "slave" device, so
	 * enforce initialized.
	 */
	if (sc->sc_type == MD_UNCONFIGURED) {
		mutex_exit(&md_device_lock);
		return ENXIO;
	}

ok:
	/* XXX duplicates code in dk_open().  Call dk_open(), instead? */
	mutex_enter(&dk->dk_openlock);
	/* Mark our unit as open. */
	switch (fmt) {
	case S_IFCHR:
		dk->dk_copenmask |= pmask;
		break;
	case S_IFBLK:
		dk->dk_bopenmask |= pmask;
		break;
	}

	dk->dk_openmask = dk->dk_copenmask | dk->dk_bopenmask;

	mutex_exit(&dk->dk_openlock);
	mutex_exit(&md_device_lock);
	return 0;
}

static int
mdclose(dev_t dev, int flag, int fmt, struct lwp *l)
{
	int part = DISKPART(dev);
	int pmask = 1 << part;
	int error;
	cfdata_t cf;
	struct md_softc *sc;
	struct disk *dk;

	sc = device_lookup_private(&md_cd, MD_UNIT(dev));
	if (sc == NULL)
		return ENXIO;

	dk = &sc->sc_dkdev;

	mutex_enter(&dk->dk_openlock);

	switch (fmt) {
	case S_IFCHR:
		dk->dk_copenmask &= ~pmask;
		break;
	case S_IFBLK:
		dk->dk_bopenmask &= ~pmask;
		break;
	}
	dk->dk_openmask = dk->dk_copenmask | dk->dk_bopenmask;
	if (dk->dk_openmask != 0) {
		mutex_exit(&dk->dk_openlock);
		return 0;
	}

	mutex_exit(&dk->dk_openlock);

	mutex_enter(&md_device_lock);
	cf = device_cfdata(sc->sc_dev);
	error = config_detach(sc->sc_dev, DETACH_QUIET);
	if (! error)
		free(cf, M_DEVBUF);
	mutex_exit(&md_device_lock);
	return error;
}

static int
mdread(dev_t dev, struct uio *uio, int flags)
{
	struct md_softc *sc;

	sc = device_lookup_private(&md_cd, MD_UNIT(dev));

	if (sc == NULL || sc->sc_type == MD_UNCONFIGURED)
		return ENXIO;

	return (physio(mdstrategy, NULL, dev, B_READ, minphys, uio));
}

static int
mdwrite(dev_t dev, struct uio *uio, int flags)
{
	struct md_softc *sc;

	sc = device_lookup_private(&md_cd, MD_UNIT(dev));

	if (sc == NULL || sc->sc_type == MD_UNCONFIGURED)
		return ENXIO;

	return (physio(mdstrategy, NULL, dev, B_WRITE, minphys, uio));
}

/*
 * Handle I/O requests, either directly, or
 * by passing them to the server process.
 */
static void
mdstrategy(struct buf *bp)
{
	struct md_softc	*sc;
	void *	addr;
	size_t off, xfer;
	bool is_read;

	sc = device_lookup_private(&md_cd, MD_UNIT(bp->b_dev));

	mutex_enter(&sc->sc_lock);

	if (sc == NULL || sc->sc_type == MD_UNCONFIGURED) {
		bp->b_error = ENXIO;
		goto done;
	}

	switch (sc->sc_type) {
#if MEMORY_DISK_SERVER
	case MD_UMEM_SERVER:
		/* Just add this job to the server's queue. */
		bufq_put(sc->sc_buflist, bp);
		cv_signal(&sc->sc_cv);
		mutex_exit(&sc->sc_lock);
		/* see md_server_loop() */
		/* no biodone in this case */
		return;
#endif	/* MEMORY_DISK_SERVER */

	case MD_KMEM_FIXED:
	case MD_KMEM_ALLOCATED:
		/* These are in kernel space.  Access directly. */
		is_read = ((bp->b_flags & B_READ) == B_READ);
		bp->b_resid = bp->b_bcount;
		off = (bp->b_blkno << DEV_BSHIFT);
		if (off >= sc->sc_size) {
			if (is_read)
				break;	/* EOF */
			goto set_eio;
		}
		xfer = bp->b_resid;
		if (xfer > (sc->sc_size - off))
			xfer = (sc->sc_size - off);
		addr = (char *)sc->sc_addr + off;
		disk_busy(&sc->sc_dkdev);
		if (is_read)
			memcpy(bp->b_data, addr, xfer);
		else
			memcpy(addr, bp->b_data, xfer);
		disk_unbusy(&sc->sc_dkdev, xfer, is_read);
		bp->b_resid -= xfer;
		break;

	default:
		bp->b_resid = bp->b_bcount;
	set_eio:
		bp->b_error = EIO;
		break;
	}

 done:
	mutex_exit(&sc->sc_lock);

	biodone(bp);
}

static int
mdioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct md_softc *sc;
	struct md_conf *umd;
	int error;

	if ((sc = device_lookup_private(&md_cd, MD_UNIT(dev))) == NULL)
		return ENXIO;

	mutex_enter(&sc->sc_lock);
	if (sc->sc_type != MD_UNCONFIGURED) {
		error = disk_ioctl(&sc->sc_dkdev, dev, cmd, data, flag, l); 
		if (error != EPASSTHROUGH) {
			mutex_exit(&sc->sc_lock);
			return 0;
		}
	}

	/* If this is not the raw partition, punt! */
	if (DISKPART(dev) != RAW_PART) {
		mutex_exit(&sc->sc_lock);
		return ENOTTY;
	}

	umd = (struct md_conf *)data;
	error = EINVAL;
	switch (cmd) {
	case MD_GETCONF:
		*umd = sc->sc_md;
		error = 0;
		break;

	case MD_SETCONF:
		/* Can only set it once. */
		if (sc->sc_type != MD_UNCONFIGURED)
			break;
		switch (umd->md_type) {
		case MD_KMEM_ALLOCATED:
			error = md_ioctl_kalloc(sc, umd, l);
			break;
#if MEMORY_DISK_SERVER
		case MD_UMEM_SERVER:
			error = md_ioctl_server(sc, umd, l);
			break;
#endif	/* MEMORY_DISK_SERVER */
		default:
			break;
		}
		break;
	}
	mutex_exit(&sc->sc_lock);
	return error;
}

static void
md_set_disklabel(struct md_softc *sc)
{
	struct disklabel *lp = sc->sc_dkdev.dk_label;
	struct partition *pp;

	memset(lp, 0, sizeof(*lp));

	lp->d_secsize = DEV_BSIZE;
	lp->d_secperunit = sc->sc_size / DEV_BSIZE;
	if (lp->d_secperunit >= (32*64)) {
		lp->d_nsectors = 32;
		lp->d_ntracks = 64;
		lp->d_ncylinders = lp->d_secperunit / (32*64);
	} else {
		lp->d_nsectors = 1;
		lp->d_ntracks = 1;
		lp->d_ncylinders = lp->d_secperunit;
	}
	lp->d_secpercyl = lp->d_ntracks*lp->d_nsectors;

	strncpy(lp->d_typename, md_cd.cd_name, sizeof(lp->d_typename));
	lp->d_type = DKTYPE_MD;
	strncpy(lp->d_packname, "fictitious", sizeof(lp->d_packname));
	lp->d_rpm = 3600;
	lp->d_interleave = 1;
	lp->d_flags = 0;

	pp = &lp->d_partitions[0];
	pp->p_offset = 0;
	pp->p_size = lp->d_secperunit;
	pp->p_fstype = FS_BSDFFS;

	pp = &lp->d_partitions[RAW_PART];
	pp->p_offset = 0;
	pp->p_size = lp->d_secperunit;
	pp->p_fstype = FS_UNUSED;

	lp->d_npartitions = RAW_PART+1;
	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);
}

/*
 * Handle ioctl MD_SETCONF for (sc_type == MD_KMEM_ALLOCATED)
 * Just allocate some kernel memory and return.
 */
static int
md_ioctl_kalloc(struct md_softc *sc, struct md_conf *umd,
    struct lwp *l)
{
	vaddr_t addr;
	vsize_t size;

	mutex_exit(&sc->sc_lock);

	/* Sanity check the size. */
	size = umd->md_size;
	addr = uvm_km_alloc(kernel_map, size, 0, UVM_KMF_WIRED|UVM_KMF_ZERO);

	mutex_enter(&sc->sc_lock);

	if (!addr)
		return ENOMEM;

	/* If another thread beat us to configure this unit:  fail. */
	if (sc->sc_type != MD_UNCONFIGURED) {
		uvm_km_free(kernel_map, addr, size, UVM_KMF_WIRED);
		return EINVAL;
	}

	/* This unit is now configured. */
	sc->sc_addr = (void *)addr; 	/* kernel space */
	sc->sc_size = (size_t)size;
	sc->sc_type = MD_KMEM_ALLOCATED;
	md_set_disklabel(sc);
	return 0;
}

#if MEMORY_DISK_SERVER

/*
 * Handle ioctl MD_SETCONF for (sc_type == MD_UMEM_SERVER)
 * Set config, then become the I/O server for this unit.
 */
static int
md_ioctl_server(struct md_softc *sc, struct md_conf *umd,
    struct lwp *l)
{
	vaddr_t end;
	int error;

	KASSERT(mutex_owned(&sc->sc_lock));

	/* Sanity check addr, size. */
	end = (vaddr_t) ((char *)umd->md_addr + umd->md_size);

	if ((end >= VM_MAXUSER_ADDRESS) ||
		(end < ((vaddr_t) umd->md_addr)) )
		return EINVAL;

	/* This unit is now configured. */
	sc->sc_addr = umd->md_addr; 	/* user space */
	sc->sc_size = umd->md_size;
	sc->sc_type = MD_UMEM_SERVER;
	md_set_disklabel(sc);

	/* Become the server daemon */
	error = md_server_loop(sc);

	/* This server is now going away! */
	sc->sc_type = MD_UNCONFIGURED;
	sc->sc_addr = 0;
	sc->sc_size = 0;

	return (error);
}

static int
md_server_loop(struct md_softc *sc)
{
	struct buf *bp;
	void *addr;	/* user space address */
	size_t off;	/* offset into "device" */
	size_t xfer;	/* amount to transfer */
	int error;
	bool is_read;

	KASSERT(mutex_owned(&sc->sc_lock));

	for (;;) {
		/* Wait for some work to arrive. */
		while ((bp = bufq_get(sc->sc_buflist)) == NULL) {
			error = cv_wait_sig(&sc->sc_cv, &sc->sc_lock);
			if (error)
				return error;
		}

		/* Do the transfer to/from user space. */
		mutex_exit(&sc->sc_lock);
		error = 0;
		is_read = ((bp->b_flags & B_READ) == B_READ);
		bp->b_resid = bp->b_bcount;
		off = (bp->b_blkno << DEV_BSHIFT);
		if (off >= sc->sc_size) {
			if (is_read)
				goto done;	/* EOF (not an error) */
			error = EIO;
			goto done;
		}
		xfer = bp->b_resid;
		if (xfer > (sc->sc_size - off))
			xfer = (sc->sc_size - off);
		addr = (char *)sc->sc_addr + off;
		disk_busy(&sc->sc_dkdev);
		if (is_read)
			error = copyin(addr, bp->b_data, xfer);
		else
			error = copyout(bp->b_data, addr, xfer);
		disk_unbusy(&sc->sc_dkdev, (error ? 0 : xfer), is_read);
		if (!error)
			bp->b_resid -= xfer;

	done:
		if (error) {
			bp->b_error = error;
		}
		biodone(bp);
		mutex_enter(&sc->sc_lock);
	}
}
#endif	/* MEMORY_DISK_SERVER */

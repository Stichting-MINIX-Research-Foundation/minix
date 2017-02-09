/*	$NetBSD: dk.c,v 1.84 2015/10/06 11:22:40 jmcneill Exp $	*/

/*-
 * Copyright (c) 2004, 2005, 2006, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dk.c,v 1.84 2015/10/06 11:22:40 jmcneill Exp $");

#ifdef _KERNEL_OPT
#include "opt_dkwedge.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/pool.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/fcntl.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/kauth.h>

#include <miscfs/specfs/specdev.h>

MALLOC_DEFINE(M_DKWEDGE, "dkwedge", "Disk wedge structures");

typedef enum {
	DKW_STATE_LARVAL	= 0,
	DKW_STATE_RUNNING	= 1,
	DKW_STATE_DYING		= 2,
	DKW_STATE_DEAD		= 666
} dkwedge_state_t;

struct dkwedge_softc {
	device_t	sc_dev;	/* pointer to our pseudo-device */
	struct cfdata	sc_cfdata;	/* our cfdata structure */
	uint8_t		sc_wname[128];	/* wedge name (Unicode, UTF-8) */

	dkwedge_state_t sc_state;	/* state this wedge is in */

	struct disk	*sc_parent;	/* parent disk */
	daddr_t		sc_offset;	/* LBA offset of wedge in parent */
	uint64_t	sc_size;	/* size of wedge in blocks */
	char		sc_ptype[32];	/* partition type */
	dev_t		sc_pdev;	/* cached parent's dev_t */
					/* link on parent's wedge list */
	LIST_ENTRY(dkwedge_softc) sc_plink;

	struct disk	sc_dk;		/* our own disk structure */
	struct bufq_state *sc_bufq;	/* buffer queue */
	struct callout	sc_restart_ch;	/* callout to restart I/O */

	u_int		sc_iopend;	/* I/Os pending */
	int		sc_flags;	/* flags (splbio) */
};

#define	DK_F_WAIT_DRAIN		0x0001	/* waiting for I/O to drain */

static void	dkstart(struct dkwedge_softc *);
static void	dkiodone(struct buf *);
static void	dkrestart(void *);
static void	dkminphys(struct buf *);

static int	dklastclose(struct dkwedge_softc *);
static int	dkwedge_cleanup_parent(struct dkwedge_softc *, int);
static int	dkwedge_detach(device_t, int);
static void	dkwedge_delall1(struct disk *, bool);
static int	dkwedge_del1(struct dkwedge_info *, int);
static struct vnode *dk_open_parent(dev_t, int);
static int	dk_close_parent(struct vnode *, int);

static dev_type_open(dkopen);
static dev_type_close(dkclose);
static dev_type_read(dkread);
static dev_type_write(dkwrite);
static dev_type_ioctl(dkioctl);
static dev_type_strategy(dkstrategy);
static dev_type_dump(dkdump);
static dev_type_size(dksize);
static dev_type_discard(dkdiscard);

const struct bdevsw dk_bdevsw = {
	.d_open = dkopen,
	.d_close = dkclose,
	.d_strategy = dkstrategy,
	.d_ioctl = dkioctl,
	.d_dump = dkdump,
	.d_psize = dksize,
	.d_discard = dkdiscard,
	.d_flag = D_DISK
};

const struct cdevsw dk_cdevsw = {
	.d_open = dkopen,
	.d_close = dkclose,
	.d_read = dkread,
	.d_write = dkwrite,
	.d_ioctl = dkioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = dkdiscard,
	.d_flag = D_DISK
};

static struct dkwedge_softc **dkwedges;
static u_int ndkwedges;
static krwlock_t dkwedges_lock;

static LIST_HEAD(, dkwedge_discovery_method) dkwedge_discovery_methods;
static krwlock_t dkwedge_discovery_methods_lock;

/*
 * dkwedge_match:
 *
 *	Autoconfiguration match function for pseudo-device glue.
 */
static int
dkwedge_match(device_t parent, cfdata_t match,
    void *aux)
{

	/* Pseudo-device; always present. */
	return (1);
}

/*
 * dkwedge_attach:
 *
 *	Autoconfiguration attach function for pseudo-device glue.
 */
static void
dkwedge_attach(device_t parent, device_t self,
    void *aux)
{

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

CFDRIVER_DECL(dk, DV_DISK, NULL);
CFATTACH_DECL3_NEW(dk, 0,
    dkwedge_match, dkwedge_attach, dkwedge_detach, NULL, NULL, NULL,
    DVF_DETACH_SHUTDOWN);

/*
 * dkwedge_wait_drain:
 *
 *	Wait for I/O on the wedge to drain.
 *	NOTE: Must be called at splbio()!
 */
static void
dkwedge_wait_drain(struct dkwedge_softc *sc)
{

	while (sc->sc_iopend != 0) {
		sc->sc_flags |= DK_F_WAIT_DRAIN;
		(void) tsleep(&sc->sc_iopend, PRIBIO, "dkdrn", 0);
	}
}

/*
 * dkwedge_compute_pdev:
 *
 *	Compute the parent disk's dev_t.
 */
static int
dkwedge_compute_pdev(const char *pname, dev_t *pdevp, enum vtype type)
{
	const char *name, *cp;
	devmajor_t pmaj;
	int punit;
	char devname[16];

	name = pname;
	switch (type) {
	case VBLK:
		pmaj = devsw_name2blk(name, devname, sizeof(devname));
		break;
	case VCHR:
		pmaj = devsw_name2chr(name, devname, sizeof(devname));
		break;
	default:
		pmaj = NODEVMAJOR;
		break;
	}
	if (pmaj == NODEVMAJOR)
		return (ENODEV);

	name += strlen(devname);
	for (cp = name, punit = 0; *cp >= '0' && *cp <= '9'; cp++)
		punit = (punit * 10) + (*cp - '0');
	if (cp == name) {
		/* Invalid parent disk name. */
		return (ENODEV);
	}

	*pdevp = MAKEDISKDEV(pmaj, punit, RAW_PART);

	return (0);
}

/*
 * dkwedge_array_expand:
 *
 *	Expand the dkwedges array.
 */
static void
dkwedge_array_expand(void)
{
	int newcnt = ndkwedges + 16;
	struct dkwedge_softc **newarray, **oldarray;

	newarray = malloc(newcnt * sizeof(*newarray), M_DKWEDGE,
	    M_WAITOK|M_ZERO);
	if ((oldarray = dkwedges) != NULL)
		memcpy(newarray, dkwedges, ndkwedges * sizeof(*newarray));
	dkwedges = newarray;
	ndkwedges = newcnt;
	if (oldarray != NULL)
		free(oldarray, M_DKWEDGE);
}

static void
dk_set_geometry(struct dkwedge_softc *sc, struct disk *pdk)
{
	struct disk *dk = &sc->sc_dk;
	struct disk_geom *dg = &dk->dk_geom;

	memset(dg, 0, sizeof(*dg));

	dg->dg_secperunit = sc->sc_size >> pdk->dk_blkshift;
	dg->dg_secsize = DEV_BSIZE << pdk->dk_blkshift;

	/* fake numbers, 1 cylinder is 1 MB with default sector size */
	dg->dg_nsectors = 32;
	dg->dg_ntracks = 64;
	dg->dg_ncylinders = dg->dg_secperunit / (dg->dg_nsectors * dg->dg_ntracks);

	disk_set_info(sc->sc_dev, dk, NULL);
}

/*
 * dkwedge_add:		[exported function]
 *
 *	Add a disk wedge based on the provided information.
 *
 *	The incoming dkw_devname[] is ignored, instead being
 *	filled in and returned to the caller.
 */
int
dkwedge_add(struct dkwedge_info *dkw)
{
	struct dkwedge_softc *sc, *lsc;
	struct disk *pdk;
	u_int unit;
	int error;
	dev_t pdev;

	dkw->dkw_parent[sizeof(dkw->dkw_parent) - 1] = '\0';
	pdk = disk_find(dkw->dkw_parent);
	if (pdk == NULL)
		return (ENODEV);

	error = dkwedge_compute_pdev(pdk->dk_name, &pdev, VBLK);
	if (error)
		return (error);

	if (dkw->dkw_offset < 0)
		return (EINVAL);

	sc = malloc(sizeof(*sc), M_DKWEDGE, M_WAITOK|M_ZERO);
	sc->sc_state = DKW_STATE_LARVAL;
	sc->sc_parent = pdk;
	sc->sc_pdev = pdev;
	sc->sc_offset = dkw->dkw_offset;
	sc->sc_size = dkw->dkw_size;

	memcpy(sc->sc_wname, dkw->dkw_wname, sizeof(sc->sc_wname));
	sc->sc_wname[sizeof(sc->sc_wname) - 1] = '\0';

	memcpy(sc->sc_ptype, dkw->dkw_ptype, sizeof(sc->sc_ptype));
	sc->sc_ptype[sizeof(sc->sc_ptype) - 1] = '\0';

	bufq_alloc(&sc->sc_bufq, "fcfs", 0);

	callout_init(&sc->sc_restart_ch, 0);
	callout_setfunc(&sc->sc_restart_ch, dkrestart, sc);

	/*
	 * Wedge will be added; increment the wedge count for the parent.
	 * Only allow this to happend if RAW_PART is the only thing open.
	 */
	mutex_enter(&pdk->dk_openlock);
	if (pdk->dk_openmask & ~(1 << RAW_PART))
		error = EBUSY;
	else {
		/* Check for wedge overlap. */
		LIST_FOREACH(lsc, &pdk->dk_wedges, sc_plink) {
			daddr_t lastblk = sc->sc_offset + sc->sc_size - 1;
			daddr_t llastblk = lsc->sc_offset + lsc->sc_size - 1;

			if (sc->sc_offset >= lsc->sc_offset &&
			    sc->sc_offset <= llastblk) {
				/* Overlaps the tail of the existing wedge. */
				break;
			}
			if (lastblk >= lsc->sc_offset &&
			    lastblk <= llastblk) {
				/* Overlaps the head of the existing wedge. */
			    	break;
			}
		}
		if (lsc != NULL) {
			if (sc->sc_offset == lsc->sc_offset &&
			    sc->sc_size == lsc->sc_size &&
			    strcmp(sc->sc_wname, lsc->sc_wname) == 0)
				error = EEXIST;
			else
				error = EINVAL;
		} else {
			pdk->dk_nwedges++;
			LIST_INSERT_HEAD(&pdk->dk_wedges, sc, sc_plink);
		}
	}
	mutex_exit(&pdk->dk_openlock);
	if (error) {
		bufq_free(sc->sc_bufq);
		free(sc, M_DKWEDGE);
		return (error);
	}

	/* Fill in our cfdata for the pseudo-device glue. */
	sc->sc_cfdata.cf_name = dk_cd.cd_name;
	sc->sc_cfdata.cf_atname = dk_ca.ca_name;
	/* sc->sc_cfdata.cf_unit set below */
	sc->sc_cfdata.cf_fstate = FSTATE_STAR;

	/* Insert the larval wedge into the array. */
	rw_enter(&dkwedges_lock, RW_WRITER);
	for (error = 0;;) {
		struct dkwedge_softc **scpp;

		/*
		 * Check for a duplicate wname while searching for
		 * a slot.
		 */
		for (scpp = NULL, unit = 0; unit < ndkwedges; unit++) {
			if (dkwedges[unit] == NULL) {
				if (scpp == NULL) {
					scpp = &dkwedges[unit];
					sc->sc_cfdata.cf_unit = unit;
				}
			} else {
				/* XXX Unicode. */
				if (strcmp(dkwedges[unit]->sc_wname,
					   sc->sc_wname) == 0) {
					error = EEXIST;
					break;
				}
			}
		}
		if (error)
			break;
		KASSERT(unit == ndkwedges);
		if (scpp == NULL)
			dkwedge_array_expand();
		else {
			KASSERT(scpp == &dkwedges[sc->sc_cfdata.cf_unit]);
			*scpp = sc;
			break;
		}
	}
	rw_exit(&dkwedges_lock);
	if (error) {
		mutex_enter(&pdk->dk_openlock);
		pdk->dk_nwedges--;
		LIST_REMOVE(sc, sc_plink);
		mutex_exit(&pdk->dk_openlock);

		bufq_free(sc->sc_bufq);
		free(sc, M_DKWEDGE);
		return (error);
	}

	/*
	 * Now that we know the unit #, attach a pseudo-device for
	 * this wedge instance.  This will provide us with the
	 * device_t necessary for glue to other parts of the system.
	 *
	 * This should never fail, unless we're almost totally out of
	 * memory.
	 */
	if ((sc->sc_dev = config_attach_pseudo(&sc->sc_cfdata)) == NULL) {
		aprint_error("%s%u: unable to attach pseudo-device\n",
		    sc->sc_cfdata.cf_name, sc->sc_cfdata.cf_unit);

		rw_enter(&dkwedges_lock, RW_WRITER);
		dkwedges[sc->sc_cfdata.cf_unit] = NULL;
		rw_exit(&dkwedges_lock);

		mutex_enter(&pdk->dk_openlock);
		pdk->dk_nwedges--;
		LIST_REMOVE(sc, sc_plink);
		mutex_exit(&pdk->dk_openlock);

		bufq_free(sc->sc_bufq);
		free(sc, M_DKWEDGE);
		return (ENOMEM);
	}

	/* Return the devname to the caller. */
	strlcpy(dkw->dkw_devname, device_xname(sc->sc_dev),
		sizeof(dkw->dkw_devname));

	/*
	 * XXX Really ought to make the disk_attach() and the changing
	 * of state to RUNNING atomic.
	 */

	disk_init(&sc->sc_dk, device_xname(sc->sc_dev), NULL);
	dk_set_geometry(sc, pdk);
	disk_attach(&sc->sc_dk);

	/* Disk wedge is ready for use! */
	sc->sc_state = DKW_STATE_RUNNING;

	/* Announce our arrival. */
	aprint_normal(
	    "%s at %s: \"%s\", %"PRIu64" blocks at %"PRId64", type: %s\n",
	    device_xname(sc->sc_dev), pdk->dk_name,
	    sc->sc_wname,	/* XXX Unicode */
	    sc->sc_size, sc->sc_offset,
	    sc->sc_ptype[0] == '\0' ? "<unknown>" : sc->sc_ptype);

	return (0);
}

/*
 * dkwedge_find:
 *
 *	Lookup a disk wedge based on the provided information.
 *	NOTE: We look up the wedge based on the wedge devname,
 *	not wname.
 *
 *	Return NULL if the wedge is not found, otherwise return
 *	the wedge's softc.  Assign the wedge's unit number to unitp
 *	if unitp is not NULL.
 */
static struct dkwedge_softc *
dkwedge_find(struct dkwedge_info *dkw, u_int *unitp)
{
	struct dkwedge_softc *sc = NULL;
	u_int unit;

	/* Find our softc. */
	dkw->dkw_devname[sizeof(dkw->dkw_devname) - 1] = '\0';
	rw_enter(&dkwedges_lock, RW_READER);
	for (unit = 0; unit < ndkwedges; unit++) {
		if ((sc = dkwedges[unit]) != NULL &&
		    strcmp(device_xname(sc->sc_dev), dkw->dkw_devname) == 0 &&
		    strcmp(sc->sc_parent->dk_name, dkw->dkw_parent) == 0) {
			break;
		}
	}
	rw_exit(&dkwedges_lock);
	if (unit == ndkwedges)
		return NULL;

	if (unitp != NULL)
		*unitp = unit;

	return sc;
}

/*
 * dkwedge_del:		[exported function]
 *
 *	Delete a disk wedge based on the provided information.
 *	NOTE: We look up the wedge based on the wedge devname,
 *	not wname.
 */
int
dkwedge_del(struct dkwedge_info *dkw)
{
	return dkwedge_del1(dkw, 0);
}

int
dkwedge_del1(struct dkwedge_info *dkw, int flags)
{
	struct dkwedge_softc *sc = NULL;

	/* Find our softc. */
	if ((sc = dkwedge_find(dkw, NULL)) == NULL)
		return (ESRCH);

	return config_detach(sc->sc_dev, flags);
}

static int
dkwedge_cleanup_parent(struct dkwedge_softc *sc, int flags)
{
	struct disk *dk = &sc->sc_dk;
	int rc;

	rc = 0;
	mutex_enter(&dk->dk_openlock);
	if (dk->dk_openmask == 0)
		;	/* nothing to do */
	else if ((flags & DETACH_FORCE) == 0)
		rc = EBUSY;
	else {
		mutex_enter(&sc->sc_parent->dk_rawlock);
		rc = dklastclose(sc); /* releases dk_rawlock */
	}
	mutex_exit(&dk->dk_openlock);

	return rc;
}

/*
 * dkwedge_detach:
 *
 *	Autoconfiguration detach function for pseudo-device glue.
 */
static int
dkwedge_detach(device_t self, int flags)
{
	struct dkwedge_softc *sc = NULL;
	u_int unit;
	int bmaj, cmaj, rc, s;

	rw_enter(&dkwedges_lock, RW_WRITER);
	for (unit = 0; unit < ndkwedges; unit++) {
		if ((sc = dkwedges[unit]) != NULL && sc->sc_dev == self)
			break;
	}
	if (unit == ndkwedges)
		rc = ENXIO;
	else if ((rc = dkwedge_cleanup_parent(sc, flags)) == 0) {
		/* Mark the wedge as dying. */
		sc->sc_state = DKW_STATE_DYING;
	}
	rw_exit(&dkwedges_lock);

	if (rc != 0)
		return rc;

	pmf_device_deregister(self);

	/* Locate the wedge major numbers. */
	bmaj = bdevsw_lookup_major(&dk_bdevsw);
	cmaj = cdevsw_lookup_major(&dk_cdevsw);

	/* Kill any pending restart. */
	callout_stop(&sc->sc_restart_ch);

	/*
	 * dkstart() will kill any queued buffers now that the
	 * state of the wedge is not RUNNING.  Once we've done
	 * that, wait for any other pending I/O to complete.
	 */
	s = splbio();
	dkstart(sc);
	dkwedge_wait_drain(sc);
	splx(s);

	/* Nuke the vnodes for any open instances. */
	vdevgone(bmaj, unit, unit, VBLK);
	vdevgone(cmaj, unit, unit, VCHR);

	/* Clean up the parent. */
	dkwedge_cleanup_parent(sc, flags | DETACH_FORCE);

	/* Announce our departure. */
	aprint_normal("%s at %s (%s) deleted\n", device_xname(sc->sc_dev),
	    sc->sc_parent->dk_name,
	    sc->sc_wname);	/* XXX Unicode */

	mutex_enter(&sc->sc_parent->dk_openlock);
	sc->sc_parent->dk_nwedges--;
	LIST_REMOVE(sc, sc_plink);
	mutex_exit(&sc->sc_parent->dk_openlock);

	/* Delete our buffer queue. */
	bufq_free(sc->sc_bufq);

	/* Detach from the disk list. */
	disk_detach(&sc->sc_dk);
	disk_destroy(&sc->sc_dk);

	/* Poof. */
	rw_enter(&dkwedges_lock, RW_WRITER);
	dkwedges[unit] = NULL;
	sc->sc_state = DKW_STATE_DEAD;
	rw_exit(&dkwedges_lock);

	free(sc, M_DKWEDGE);

	return 0;
}

/*
 * dkwedge_delall:	[exported function]
 *
 *	Delete all of the wedges on the specified disk.  Used when
 *	a disk is being detached.
 */
void
dkwedge_delall(struct disk *pdk)
{
	dkwedge_delall1(pdk, false);
}

static void
dkwedge_delall1(struct disk *pdk, bool idleonly)
{
	struct dkwedge_info dkw;
	struct dkwedge_softc *sc;
	int flags;

	flags = DETACH_QUIET;
	if (!idleonly) flags |= DETACH_FORCE;

	for (;;) {
		mutex_enter(&pdk->dk_openlock);
		LIST_FOREACH(sc, &pdk->dk_wedges, sc_plink) {
			if (!idleonly || sc->sc_dk.dk_openmask == 0)
				break;
		}
		if (sc == NULL) {
			KASSERT(idleonly || pdk->dk_nwedges == 0);
			mutex_exit(&pdk->dk_openlock);
			return;
		}
		strcpy(dkw.dkw_parent, pdk->dk_name);
		strlcpy(dkw.dkw_devname, device_xname(sc->sc_dev),
			sizeof(dkw.dkw_devname));
		mutex_exit(&pdk->dk_openlock);
		(void) dkwedge_del1(&dkw, flags);
	}
}

/*
 * dkwedge_list:	[exported function]
 *
 *	List all of the wedges on a particular disk.
 *	If p == NULL, the buffer is in kernel space.  Otherwise, it is
 *	in user space of the specified process.
 */
int
dkwedge_list(struct disk *pdk, struct dkwedge_list *dkwl, struct lwp *l)
{
	struct uio uio;
	struct iovec iov;
	struct dkwedge_softc *sc;
	struct dkwedge_info dkw;
	int error = 0;

	iov.iov_base = dkwl->dkwl_buf;
	iov.iov_len = dkwl->dkwl_bufsize;

	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = 0;
	uio.uio_resid = dkwl->dkwl_bufsize;
	uio.uio_rw = UIO_READ;
	KASSERT(l == curlwp);
	uio.uio_vmspace = l->l_proc->p_vmspace;

	dkwl->dkwl_ncopied = 0;

	mutex_enter(&pdk->dk_openlock);
	LIST_FOREACH(sc, &pdk->dk_wedges, sc_plink) {
		if (uio.uio_resid < sizeof(dkw))
			break;

		if (sc->sc_state != DKW_STATE_RUNNING)
			continue;

		strlcpy(dkw.dkw_devname, device_xname(sc->sc_dev),
			sizeof(dkw.dkw_devname));
		memcpy(dkw.dkw_wname, sc->sc_wname, sizeof(dkw.dkw_wname));
		dkw.dkw_wname[sizeof(dkw.dkw_wname) - 1] = '\0';
		strcpy(dkw.dkw_parent, sc->sc_parent->dk_name);
		dkw.dkw_offset = sc->sc_offset;
		dkw.dkw_size = sc->sc_size;
		strcpy(dkw.dkw_ptype, sc->sc_ptype);

		error = uiomove(&dkw, sizeof(dkw), &uio);
		if (error)
			break;
		dkwl->dkwl_ncopied++;
	}
	dkwl->dkwl_nwedges = pdk->dk_nwedges;
	mutex_exit(&pdk->dk_openlock);

	return (error);
}

device_t
dkwedge_find_by_wname(const char *wname)
{
	device_t dv = NULL;
	struct dkwedge_softc *sc;
	int i;

	rw_enter(&dkwedges_lock, RW_WRITER);
	for (i = 0; i < ndkwedges; i++) {
		if ((sc = dkwedges[i]) == NULL)
			continue;
		if (strcmp(sc->sc_wname, wname) == 0) {
			if (dv != NULL) {
				printf(
				    "WARNING: double match for wedge name %s "
				    "(%s, %s)\n", wname, device_xname(dv),
				    device_xname(sc->sc_dev));
				continue;
			}
			dv = sc->sc_dev;
		}
	}
	rw_exit(&dkwedges_lock);
	return dv;
}

void
dkwedge_print_wnames(void)
{
	struct dkwedge_softc *sc;
	int i;

	rw_enter(&dkwedges_lock, RW_WRITER);
	for (i = 0; i < ndkwedges; i++) {
		if ((sc = dkwedges[i]) == NULL)
			continue;
		printf(" wedge:%s", sc->sc_wname);
	}
	rw_exit(&dkwedges_lock);
}

/*
 * We need a dummy object to stuff into the dkwedge discovery method link
 * set to ensure that there is always at least one object in the set.
 */
static struct dkwedge_discovery_method dummy_discovery_method;
__link_set_add_bss(dkwedge_methods, dummy_discovery_method);

/*
 * dkwedge_init:
 *
 *	Initialize the disk wedge subsystem.
 */
void
dkwedge_init(void)
{
	__link_set_decl(dkwedge_methods, struct dkwedge_discovery_method);
	struct dkwedge_discovery_method * const *ddmp;
	struct dkwedge_discovery_method *lddm, *ddm;

	rw_init(&dkwedges_lock);
	rw_init(&dkwedge_discovery_methods_lock);

	if (config_cfdriver_attach(&dk_cd) != 0)
		panic("dkwedge: unable to attach cfdriver");
	if (config_cfattach_attach(dk_cd.cd_name, &dk_ca) != 0)
		panic("dkwedge: unable to attach cfattach");

	rw_enter(&dkwedge_discovery_methods_lock, RW_WRITER);

	LIST_INIT(&dkwedge_discovery_methods);

	__link_set_foreach(ddmp, dkwedge_methods) {
		ddm = *ddmp;
		if (ddm == &dummy_discovery_method)
			continue;
		if (LIST_EMPTY(&dkwedge_discovery_methods)) {
			LIST_INSERT_HEAD(&dkwedge_discovery_methods,
					 ddm, ddm_list);
			continue;
		}
		LIST_FOREACH(lddm, &dkwedge_discovery_methods, ddm_list) {
			if (ddm->ddm_priority == lddm->ddm_priority) {
				aprint_error("dk-method-%s: method \"%s\" "
				    "already exists at priority %d\n",
				    ddm->ddm_name, lddm->ddm_name,
				    lddm->ddm_priority);
				/* Not inserted. */
				break;
			}
			if (ddm->ddm_priority < lddm->ddm_priority) {
				/* Higher priority; insert before. */
				LIST_INSERT_BEFORE(lddm, ddm, ddm_list);
				break;
			}
			if (LIST_NEXT(lddm, ddm_list) == NULL) {
				/* Last one; insert after. */
				KASSERT(lddm->ddm_priority < ddm->ddm_priority);
				LIST_INSERT_AFTER(lddm, ddm, ddm_list);
				break;
			}
		}
	}

	rw_exit(&dkwedge_discovery_methods_lock);
}

#ifdef DKWEDGE_AUTODISCOVER
int	dkwedge_autodiscover = 1;
#else
int	dkwedge_autodiscover = 0;
#endif

/*
 * dkwedge_discover:	[exported function]
 *
 *	Discover the wedges on a newly attached disk.
 *	Remove all unused wedges on the disk first.
 */
void
dkwedge_discover(struct disk *pdk)
{
	struct dkwedge_discovery_method *ddm;
	struct vnode *vp;
	int error;
	dev_t pdev;

	/*
	 * Require people playing with wedges to enable this explicitly.
	 */
	if (dkwedge_autodiscover == 0)
		return;

	rw_enter(&dkwedge_discovery_methods_lock, RW_READER);

	/*
	 * Use the character device for scanning, the block device
	 * is busy if there are already wedges attached.
	 */
	error = dkwedge_compute_pdev(pdk->dk_name, &pdev, VCHR);
	if (error) {
		aprint_error("%s: unable to compute pdev, error = %d\n",
		    pdk->dk_name, error);
		goto out;
	}

	error = cdevvp(pdev, &vp);
	if (error) {
		aprint_error("%s: unable to find vnode for pdev, error = %d\n",
		    pdk->dk_name, error);
		goto out;
	}

	error = vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	if (error) {
		aprint_error("%s: unable to lock vnode for pdev, error = %d\n",
		    pdk->dk_name, error);
		vrele(vp);
		goto out;
	}

	error = VOP_OPEN(vp, FREAD | FSILENT, NOCRED);
	if (error) {
		if (error != ENODEV)
			aprint_error("%s: unable to open device, error = %d\n",
			    pdk->dk_name, error);
		vput(vp);
		goto out;
	}
	VOP_UNLOCK(vp);

	/*
	 * Remove unused wedges
	 */
	dkwedge_delall1(pdk, true);

	/*
	 * For each supported partition map type, look to see if
	 * this map type exists.  If so, parse it and add the
	 * corresponding wedges.
	 */
	LIST_FOREACH(ddm, &dkwedge_discovery_methods, ddm_list) {
		error = (*ddm->ddm_discover)(pdk, vp);
		if (error == 0) {
			/* Successfully created wedges; we're done. */
			break;
		}
	}

	error = vn_close(vp, FREAD, NOCRED);
	if (error) {
		aprint_error("%s: unable to close device, error = %d\n",
		    pdk->dk_name, error);
		/* We'll just assume the vnode has been cleaned up. */
	}

 out:
	rw_exit(&dkwedge_discovery_methods_lock);
}

/*
 * dkwedge_read:
 *
 *	Read some data from the specified disk, used for
 *	partition discovery.
 */
int
dkwedge_read(struct disk *pdk, struct vnode *vp, daddr_t blkno,
    void *tbuf, size_t len)
{
	buf_t *bp;
	int error;
	bool isopen;
	dev_t bdev;
	struct vnode *bdvp;

	/*
	 * The kernel cannot read from a character device vnode
	 * as physio() only handles user memory.
	 *
	 * If the block device has already been opened by a wedge
	 * use that vnode and temporarily bump the open counter.
	 *
	 * Otherwise try to open the block device.
	 */

	bdev = devsw_chr2blk(vp->v_rdev);

	mutex_enter(&pdk->dk_rawlock);
	if (pdk->dk_rawopens != 0) {
		KASSERT(pdk->dk_rawvp != NULL);
		isopen = true;
		++pdk->dk_rawopens;
		bdvp = pdk->dk_rawvp;
	} else {
		isopen = false;
		bdvp = dk_open_parent(bdev, FREAD);
	}
	mutex_exit(&pdk->dk_rawlock);

	if (bdvp == NULL)
		return EBUSY;

	bp = getiobuf(bdvp, true);
	bp->b_flags = B_READ;
	bp->b_cflags = BC_BUSY;
	bp->b_dev = bdev;
	bp->b_data = tbuf;
	bp->b_bufsize = bp->b_bcount = len;
	bp->b_blkno = blkno;
	bp->b_cylinder = 0;
	bp->b_error = 0;

	VOP_STRATEGY(bdvp, bp);
	error = biowait(bp);
	putiobuf(bp);

	mutex_enter(&pdk->dk_rawlock);
	if (isopen) {
		--pdk->dk_rawopens;
	} else {
		dk_close_parent(bdvp, FREAD);
	}
	mutex_exit(&pdk->dk_rawlock);

	return error;
}

/*
 * dkwedge_lookup:
 *
 *	Look up a dkwedge_softc based on the provided dev_t.
 */
static struct dkwedge_softc *
dkwedge_lookup(dev_t dev)
{
	int unit = minor(dev);

	if (unit >= ndkwedges)
		return (NULL);

	KASSERT(dkwedges != NULL);

	return (dkwedges[unit]);
}

static struct vnode *
dk_open_parent(dev_t dev, int mode)
{
	struct vnode *vp;
	int error;

	error = bdevvp(dev, &vp);
	if (error)
		return NULL;

	error = vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	if (error) {
		vrele(vp);
		return NULL;
	}
	error = VOP_OPEN(vp, mode, NOCRED);
	if (error) {
		vput(vp);
		return NULL;
	}

	/* VOP_OPEN() doesn't do this for us. */
	if (mode & FWRITE) {
		mutex_enter(vp->v_interlock);
		vp->v_writecount++;
		mutex_exit(vp->v_interlock);
	}

	VOP_UNLOCK(vp);

	return vp;
}

static int
dk_close_parent(struct vnode *vp, int mode)
{
	int error;

	error = vn_close(vp, mode, NOCRED);
	return error;
}

/*
 * dkopen:		[devsw entry point]
 *
 *	Open a wedge.
 */
static int
dkopen(dev_t dev, int flags, int fmt, struct lwp *l)
{
	struct dkwedge_softc *sc = dkwedge_lookup(dev);
	struct vnode *vp;
	int error = 0;

	if (sc == NULL)
		return (ENODEV);
	if (sc->sc_state != DKW_STATE_RUNNING)
		return (ENXIO);

	/*
	 * We go through a complicated little dance to only open the parent
	 * vnode once per wedge, no matter how many times the wedge is
	 * opened.  The reason?  We see one dkopen() per open call, but
	 * only dkclose() on the last close.
	 */
	mutex_enter(&sc->sc_dk.dk_openlock);
	mutex_enter(&sc->sc_parent->dk_rawlock);
	if (sc->sc_dk.dk_openmask == 0) {
		if (sc->sc_parent->dk_rawopens == 0) {
			KASSERT(sc->sc_parent->dk_rawvp == NULL);
			vp = dk_open_parent(sc->sc_pdev, FREAD | FWRITE);
			if (vp == NULL)
				goto popen_fail;
			sc->sc_parent->dk_rawvp = vp;
		}
		sc->sc_parent->dk_rawopens++;
	}
	if (fmt == S_IFCHR)
		sc->sc_dk.dk_copenmask |= 1;
	else
		sc->sc_dk.dk_bopenmask |= 1;
	sc->sc_dk.dk_openmask =
	    sc->sc_dk.dk_copenmask | sc->sc_dk.dk_bopenmask;

 popen_fail:
	mutex_exit(&sc->sc_parent->dk_rawlock);
	mutex_exit(&sc->sc_dk.dk_openlock);
	return (error);
}

/*
 * Caller must hold sc->sc_dk.dk_openlock and sc->sc_parent->dk_rawlock.
 */
static int
dklastclose(struct dkwedge_softc *sc)
{
	int error = 0, doclose;

	doclose = 0;
	if (sc->sc_parent->dk_rawopens > 0) {
		if (--sc->sc_parent->dk_rawopens == 0)
			doclose = 1;
	}

	mutex_exit(&sc->sc_parent->dk_rawlock);

	if (doclose) {
		KASSERT(sc->sc_parent->dk_rawvp != NULL);
		dk_close_parent(sc->sc_parent->dk_rawvp, FREAD | FWRITE);
		sc->sc_parent->dk_rawvp = NULL;
	}

	return error;
}

/*
 * dkclose:		[devsw entry point]
 *
 *	Close a wedge.
 */
static int
dkclose(dev_t dev, int flags, int fmt, struct lwp *l)
{
	struct dkwedge_softc *sc = dkwedge_lookup(dev);
	int error = 0;

	if (sc == NULL)
		return (ENODEV);
	if (sc->sc_state != DKW_STATE_RUNNING)
		return (ENXIO);

	KASSERT(sc->sc_dk.dk_openmask != 0);

	mutex_enter(&sc->sc_dk.dk_openlock);
	mutex_enter(&sc->sc_parent->dk_rawlock);

	if (fmt == S_IFCHR)
		sc->sc_dk.dk_copenmask &= ~1;
	else
		sc->sc_dk.dk_bopenmask &= ~1;
	sc->sc_dk.dk_openmask =
	    sc->sc_dk.dk_copenmask | sc->sc_dk.dk_bopenmask;

	if (sc->sc_dk.dk_openmask == 0)
		error = dklastclose(sc); /* releases dk_rawlock */
	else 
		mutex_exit(&sc->sc_parent->dk_rawlock);

	mutex_exit(&sc->sc_dk.dk_openlock);

	return (error);
}

/*
 * dkstragegy:		[devsw entry point]
 *
 *	Perform I/O based on the wedge I/O strategy.
 */
static void
dkstrategy(struct buf *bp)
{
	struct dkwedge_softc *sc = dkwedge_lookup(bp->b_dev);
	uint64_t p_size, p_offset;
	int s;

	if (sc == NULL) {
		bp->b_error = ENODEV;
		goto done;
	}

	if (sc->sc_state != DKW_STATE_RUNNING ||
	    sc->sc_parent->dk_rawvp == NULL) {
		bp->b_error = ENXIO;
		goto done;
	}

	/* If it's an empty transfer, wake up the top half now. */
	if (bp->b_bcount == 0)
		goto done;

	p_offset = sc->sc_offset << sc->sc_parent->dk_blkshift;
	p_size   = sc->sc_size << sc->sc_parent->dk_blkshift;

	/* Make sure it's in-range. */
	if (bounds_check_with_mediasize(bp, DEV_BSIZE, p_size) <= 0)
		goto done;

	/* Translate it to the parent's raw LBA. */
	bp->b_rawblkno = bp->b_blkno + p_offset;

	/* Place it in the queue and start I/O on the unit. */
	s = splbio();
	sc->sc_iopend++;
	bufq_put(sc->sc_bufq, bp);
	dkstart(sc);
	splx(s);
	return;

 done:
	bp->b_resid = bp->b_bcount;
	biodone(bp);
}

/*
 * dkstart:
 *
 *	Start I/O that has been enqueued on the wedge.
 *	NOTE: Must be called at splbio()!
 */
static void
dkstart(struct dkwedge_softc *sc)
{
	struct vnode *vp;
	struct buf *bp, *nbp;

	/* Do as much work as has been enqueued. */
	while ((bp = bufq_peek(sc->sc_bufq)) != NULL) {
		if (sc->sc_state != DKW_STATE_RUNNING) {
			(void) bufq_get(sc->sc_bufq);
			if (sc->sc_iopend-- == 1 &&
			    (sc->sc_flags & DK_F_WAIT_DRAIN) != 0) {
				sc->sc_flags &= ~DK_F_WAIT_DRAIN;
				wakeup(&sc->sc_iopend);
			}
			bp->b_error = ENXIO;
			bp->b_resid = bp->b_bcount;
			biodone(bp);
		}

		/* Instrumentation. */
		disk_busy(&sc->sc_dk);

		nbp = getiobuf(sc->sc_parent->dk_rawvp, false);
		if (nbp == NULL) {
			/*
			 * No resources to run this request; leave the
			 * buffer queued up, and schedule a timer to
			 * restart the queue in 1/2 a second.
			 */
			disk_unbusy(&sc->sc_dk, 0, bp->b_flags & B_READ);
			callout_schedule(&sc->sc_restart_ch, hz / 2);
			return;
		}

		(void) bufq_get(sc->sc_bufq);

		nbp->b_data = bp->b_data;
		nbp->b_flags = bp->b_flags;
		nbp->b_oflags = bp->b_oflags;
		nbp->b_cflags = bp->b_cflags;
		nbp->b_iodone = dkiodone;
		nbp->b_proc = bp->b_proc;
		nbp->b_blkno = bp->b_rawblkno;
		nbp->b_dev = sc->sc_parent->dk_rawvp->v_rdev;
		nbp->b_bcount = bp->b_bcount;
		nbp->b_private = bp;
		BIO_COPYPRIO(nbp, bp);

		vp = nbp->b_vp;
		if ((nbp->b_flags & B_READ) == 0) {
			mutex_enter(vp->v_interlock);
			vp->v_numoutput++;
			mutex_exit(vp->v_interlock);
		}
		VOP_STRATEGY(vp, nbp);
	}
}

/*
 * dkiodone:
 *
 *	I/O to a wedge has completed; alert the top half.
 */
static void
dkiodone(struct buf *bp)
{
	struct buf *obp = bp->b_private;
	struct dkwedge_softc *sc = dkwedge_lookup(obp->b_dev);

	int s = splbio();

	if (bp->b_error != 0)
		obp->b_error = bp->b_error;
	obp->b_resid = bp->b_resid;
	putiobuf(bp);

	if (sc->sc_iopend-- == 1 && (sc->sc_flags & DK_F_WAIT_DRAIN) != 0) {
		sc->sc_flags &= ~DK_F_WAIT_DRAIN;
		wakeup(&sc->sc_iopend);
	}

	disk_unbusy(&sc->sc_dk, obp->b_bcount - obp->b_resid,
	    obp->b_flags & B_READ);

	biodone(obp);

	/* Kick the queue in case there is more work we can do. */
	dkstart(sc);
	splx(s);
}

/*
 * dkrestart:
 *
 *	Restart the work queue after it was stalled due to
 *	a resource shortage.  Invoked via a callout.
 */
static void
dkrestart(void *v)
{
	struct dkwedge_softc *sc = v;
	int s;

	s = splbio();
	dkstart(sc);
	splx(s);
}

/*
 * dkminphys:
 *
 *	Call parent's minphys function.
 */
static void
dkminphys(struct buf *bp)
{
	struct dkwedge_softc *sc = dkwedge_lookup(bp->b_dev);
	dev_t dev;

	dev = bp->b_dev;
	bp->b_dev = sc->sc_pdev;
	(*sc->sc_parent->dk_driver->d_minphys)(bp);
	bp->b_dev = dev;
}

/*
 * dkread:		[devsw entry point]
 *
 *	Read from a wedge.
 */
static int
dkread(dev_t dev, struct uio *uio, int flags)
{
	struct dkwedge_softc *sc = dkwedge_lookup(dev);

	if (sc == NULL)
		return (ENODEV);
	if (sc->sc_state != DKW_STATE_RUNNING)
		return (ENXIO);

	return (physio(dkstrategy, NULL, dev, B_READ, dkminphys, uio));
}

/*
 * dkwrite:		[devsw entry point]
 *
 *	Write to a wedge.
 */
static int
dkwrite(dev_t dev, struct uio *uio, int flags)
{
	struct dkwedge_softc *sc = dkwedge_lookup(dev);

	if (sc == NULL)
		return (ENODEV);
	if (sc->sc_state != DKW_STATE_RUNNING)
		return (ENXIO);

	return (physio(dkstrategy, NULL, dev, B_WRITE, dkminphys, uio));
}

/*
 * dkioctl:		[devsw entry point]
 *
 *	Perform an ioctl request on a wedge.
 */
static int
dkioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct dkwedge_softc *sc = dkwedge_lookup(dev);
	int error = 0;

	if (sc == NULL)
		return (ENODEV);
	if (sc->sc_state != DKW_STATE_RUNNING)
		return (ENXIO);
	if (sc->sc_parent->dk_rawvp == NULL)
		return (ENXIO);

	/*
	 * We pass NODEV instead of our device to indicate we don't
	 * want to handle disklabel ioctls
	 */
	error = disk_ioctl(&sc->sc_dk, NODEV, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return (error);

	error = 0;
	
	switch (cmd) {
	case DIOCCACHESYNC:
		/*
		 * XXX Do we really need to care about having a writable
		 * file descriptor here?
		 */
		if ((flag & FWRITE) == 0)
			error = EBADF;
		else
			error = VOP_IOCTL(sc->sc_parent->dk_rawvp,
					  cmd, data, flag,
					  l != NULL ? l->l_cred : NOCRED);
		break;
	case DIOCGWEDGEINFO:
	    {
		struct dkwedge_info *dkw = (void *) data;

		strlcpy(dkw->dkw_devname, device_xname(sc->sc_dev),
			sizeof(dkw->dkw_devname));
	    	memcpy(dkw->dkw_wname, sc->sc_wname, sizeof(dkw->dkw_wname));
		dkw->dkw_wname[sizeof(dkw->dkw_wname) - 1] = '\0';
		strcpy(dkw->dkw_parent, sc->sc_parent->dk_name);
		dkw->dkw_offset = sc->sc_offset;
		dkw->dkw_size = sc->sc_size;
		strcpy(dkw->dkw_ptype, sc->sc_ptype);

		break;
	    }

	default:
		error = ENOTTY;
	}

	return (error);
}

/*
 * dkdiscard:		[devsw entry point]
 *
 *	Perform a discard-range request on a wedge.
 */
static int
dkdiscard(dev_t dev, off_t pos, off_t len)
{
	struct dkwedge_softc *sc = dkwedge_lookup(dev);
	unsigned shift;
	off_t offset, maxlen;

	if (sc == NULL)
		return (ENODEV);
	if (sc->sc_state != DKW_STATE_RUNNING)
		return (ENXIO);
	if (sc->sc_parent->dk_rawvp == NULL)
		return (ENXIO);

	shift = (sc->sc_parent->dk_blkshift + DEV_BSHIFT);
	KASSERT(__type_fit(off_t, sc->sc_size));
	KASSERT(__type_fit(off_t, sc->sc_offset));
	KASSERT(0 <= sc->sc_offset);
	KASSERT(sc->sc_size <= (__type_max(off_t) >> shift));
	KASSERT(sc->sc_offset <= ((__type_max(off_t) >> shift) - sc->sc_size));
	offset = ((off_t)sc->sc_offset << shift);
	maxlen = ((off_t)sc->sc_size << shift);

	if (len > maxlen)
		return (EINVAL);
	if (pos > (maxlen - len))
		return (EINVAL);

	pos += offset;
	return VOP_FDISCARD(sc->sc_parent->dk_rawvp, pos, len);
}

/*
 * dksize:		[devsw entry point]
 *
 *	Query the size of a wedge for the purpose of performing a dump
 *	or for swapping to.
 */
static int
dksize(dev_t dev)
{
	struct dkwedge_softc *sc = dkwedge_lookup(dev);
	int rv = -1;

	if (sc == NULL)
		return (-1);
	if (sc->sc_state != DKW_STATE_RUNNING)
		return (-1);

	mutex_enter(&sc->sc_dk.dk_openlock);
	mutex_enter(&sc->sc_parent->dk_rawlock);

	/* Our content type is static, no need to open the device. */

	if (strcmp(sc->sc_ptype, DKW_PTYPE_SWAP) == 0) {
		/* Saturate if we are larger than INT_MAX. */
		if (sc->sc_size > INT_MAX)
			rv = INT_MAX;
		else
			rv = (int) sc->sc_size;
	}

	mutex_exit(&sc->sc_parent->dk_rawlock);
	mutex_exit(&sc->sc_dk.dk_openlock);

	return (rv);
}

/*
 * dkdump:		[devsw entry point]
 *
 *	Perform a crash dump to a wedge.
 */
static int
dkdump(dev_t dev, daddr_t blkno, void *va, size_t size)
{
	struct dkwedge_softc *sc = dkwedge_lookup(dev);
	const struct bdevsw *bdev;
	int rv = 0;

	if (sc == NULL)
		return (ENODEV);
	if (sc->sc_state != DKW_STATE_RUNNING)
		return (ENXIO);

	mutex_enter(&sc->sc_dk.dk_openlock);
	mutex_enter(&sc->sc_parent->dk_rawlock);

	/* Our content type is static, no need to open the device. */

	if (strcmp(sc->sc_ptype, DKW_PTYPE_SWAP) != 0) {
		rv = ENXIO;
		goto out;
	}
	if (size % DEV_BSIZE != 0) {
		rv = EINVAL;
		goto out;
	}
	if (blkno + size / DEV_BSIZE > sc->sc_size) {
		printf("%s: blkno (%" PRIu64 ") + size / DEV_BSIZE (%zu) > "
		    "sc->sc_size (%" PRIu64 ")\n", __func__, blkno,
		    size / DEV_BSIZE, sc->sc_size);
		rv = EINVAL;
		goto out;
	}

	bdev = bdevsw_lookup(sc->sc_pdev);
	rv = (*bdev->d_dump)(sc->sc_pdev, blkno + sc->sc_offset, va, size);

out:
	mutex_exit(&sc->sc_parent->dk_rawlock);
	mutex_exit(&sc->sc_dk.dk_openlock);

	return rv;
}

/*
 * config glue
 */

/*
 * dkwedge_find_partition
 *
 *	Find wedge corresponding to the specified parent name
 *	and offset/length.
 */
device_t
dkwedge_find_partition(device_t parent, daddr_t startblk, uint64_t nblks)
{
	struct dkwedge_softc *sc;
	int i;
	device_t wedge = NULL;

	rw_enter(&dkwedges_lock, RW_READER);
	for (i = 0; i < ndkwedges; i++) {
		if ((sc = dkwedges[i]) == NULL)
			continue;
		if (strcmp(sc->sc_parent->dk_name, device_xname(parent)) == 0 &&
		    sc->sc_offset == startblk &&
		    sc->sc_size == nblks) {
			if (wedge) {
				printf("WARNING: double match for boot wedge "
				    "(%s, %s)\n",
				    device_xname(wedge),
				    device_xname(sc->sc_dev));
				continue;
			}
			wedge = sc->sc_dev;
		}
	}
	rw_exit(&dkwedges_lock);

	return wedge;
}

const char *
dkwedge_get_parent_name(dev_t dev)
{
	/* XXX: perhaps do this in lookup? */
	int bmaj = bdevsw_lookup_major(&dk_bdevsw);
	int cmaj = cdevsw_lookup_major(&dk_cdevsw);
	if (major(dev) != bmaj && major(dev) != cmaj)
		return NULL;
	struct dkwedge_softc *sc = dkwedge_lookup(dev);
	if (sc == NULL)
		return NULL;
	return sc->sc_parent->dk_name;
}


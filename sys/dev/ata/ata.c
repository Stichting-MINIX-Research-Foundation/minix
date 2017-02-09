/*	$NetBSD: ata.c,v 1.132 2014/09/10 07:04:48 matt Exp $	*/

/*
 * Copyright (c) 1998, 2001 Manuel Bouyer.  All rights reserved.
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
__KERNEL_RCSID(0, "$NetBSD: ata.c,v 1.132 2014/09/10 07:04:48 matt Exp $");

#include "opt_ata.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/proc.h>
#include <sys/pool.h>
#include <sys/kthread.h>
#include <sys/errno.h>
#include <sys/ataio.h>
#include <sys/kmem.h>
#include <sys/intr.h>
#include <sys/bus.h>
#include <sys/once.h>

#include <dev/ata/ataconf.h>
#include <dev/ata/atareg.h>
#include <dev/ata/atavar.h>
#include <dev/ic/wdcvar.h>	/* for PIOBM */

#include "locators.h"

#include "atapibus.h"
#include "ataraid.h"
#include "sata_pmp.h"

#if NATARAID > 0
#include <dev/ata/ata_raidvar.h>
#endif
#if NSATA_PMP > 0
#include <dev/ata/satapmpvar.h>
#endif
#include <dev/ata/satapmpreg.h>

#define DEBUG_FUNCS  0x08
#define DEBUG_PROBE  0x10
#define DEBUG_DETACH 0x20
#define	DEBUG_XFERS  0x40
#ifdef ATADEBUG
#ifndef ATADEBUG_MASK
#define ATADEBUG_MASK 0
#endif
int atadebug_mask = ATADEBUG_MASK;
#define ATADEBUG_PRINT(args, level) \
	if (atadebug_mask & (level)) \
		printf args
#else
#define ATADEBUG_PRINT(args, level)
#endif

static ONCE_DECL(ata_init_ctrl);
static struct pool ata_xfer_pool;

/*
 * A queue of atabus instances, used to ensure the same bus probe order
 * for a given hardware configuration at each boot.  Kthread probing
 * devices on a atabus.  Only one probing at once. 
 */
static TAILQ_HEAD(, atabus_initq)	atabus_initq_head;
static kmutex_t				atabus_qlock;
static kcondvar_t			atabus_qcv;
static lwp_t *				atabus_cfg_lwp;

/*****************************************************************************
 * ATA bus layer.
 *
 * ATA controllers attach an atabus instance, which handles probing the bus
 * for drives, etc.
 *****************************************************************************/

dev_type_open(atabusopen);
dev_type_close(atabusclose);
dev_type_ioctl(atabusioctl);

const struct cdevsw atabus_cdevsw = {
	.d_open = atabusopen,
	.d_close = atabusclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = atabusioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

extern struct cfdriver atabus_cd;

static void atabus_childdetached(device_t, device_t);
static int atabus_rescan(device_t, const char *, const int *);
static bool atabus_resume(device_t, const pmf_qual_t *);
static bool atabus_suspend(device_t, const pmf_qual_t *);
static void atabusconfig_thread(void *);

/*
 * atabus_init:
 *
 *	Initialize ATA subsystem structures.
 */
static int
atabus_init(void)
{

	pool_init(&ata_xfer_pool, sizeof(struct ata_xfer), 0, 0, 0,
	    "ataspl", NULL, IPL_BIO);
	TAILQ_INIT(&atabus_initq_head);
	mutex_init(&atabus_qlock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&atabus_qcv, "atainitq");
	return 0;
}

/*
 * atabusprint:
 *
 *	Autoconfiguration print routine used by ATA controllers when
 *	attaching an atabus instance.
 */
int
atabusprint(void *aux, const char *pnp)
{
	struct ata_channel *chan = aux;

	if (pnp)
		aprint_normal("atabus at %s", pnp);
	aprint_normal(" channel %d", chan->ch_channel);

	return (UNCONF);
}

/*
 * ataprint:
 *
 *	Autoconfiguration print routine.
 */
int
ataprint(void *aux, const char *pnp)
{
	struct ata_device *adev = aux;

	if (pnp)
		aprint_normal("wd at %s", pnp);
	aprint_normal(" drive %d", adev->adev_drv_data->drive);

	return (UNCONF);
}

/*
 * ata_channel_attach:
 *
 *	Common parts of attaching an atabus to an ATA controller channel.
 */
void
ata_channel_attach(struct ata_channel *chp)
{

	if (chp->ch_flags & ATACH_DISABLED)
		return;

	/* XXX callout_destroy */
	callout_init(&chp->ch_callout, 0);

	TAILQ_INIT(&chp->ch_queue->queue_xfer);
	chp->ch_queue->queue_freeze = 0;
	chp->ch_queue->queue_flags = 0;
	chp->ch_queue->active_xfer = NULL;

	chp->atabus = config_found_ia(chp->ch_atac->atac_dev, "ata", chp,
		atabusprint);
}

static void
atabusconfig(struct atabus_softc *atabus_sc)
{
	struct ata_channel *chp = atabus_sc->sc_chan;
	struct atac_softc *atac = chp->ch_atac;
	struct atabus_initq *atabus_initq = NULL;
	int i, s, error;

	/* we are in the atabus's thread context */
	s = splbio();
	chp->ch_flags |= ATACH_TH_RUN;
	splx(s);

	/*
	 * Probe for the drives attached to controller, unless a PMP
	 * is already known
	 */
	/* XXX for SATA devices we will power up all drives at once */
	if (chp->ch_satapmp_nports == 0)
		(*atac->atac_probe)(chp);

	if (chp->ch_ndrives >= 2) {
		ATADEBUG_PRINT(("atabusattach: ch_drive_type 0x%x 0x%x\n",
		    chp->ch_drive[0].drive_type, chp->ch_drive[1].drive_type),
		    DEBUG_PROBE);
	}

	/* next operations will occurs in a separate thread */
	s = splbio();
	chp->ch_flags &= ~ATACH_TH_RUN;
	splx(s);

	/* Make sure the devices probe in atabus order to avoid jitter. */
	mutex_enter(&atabus_qlock);
	for (;;) {
		atabus_initq = TAILQ_FIRST(&atabus_initq_head);
		if (atabus_initq->atabus_sc == atabus_sc)
			break;
		cv_wait(&atabus_qcv, &atabus_qlock);
	}
	mutex_exit(&atabus_qlock);

	/* If no drives, abort here */
	if (chp->ch_drive == NULL)
		goto out;
	KASSERT(chp->ch_ndrives == 0 || chp->ch_drive != NULL);
	for (i = 0; i < chp->ch_ndrives; i++)
		if (chp->ch_drive[i].drive_type != ATA_DRIVET_NONE)
			break;
	if (i == chp->ch_ndrives)
		goto out;

	/* Shortcut in case we've been shutdown */
	if (chp->ch_flags & ATACH_SHUTDOWN)
		goto out;

	if ((error = kthread_create(PRI_NONE, 0, NULL, atabusconfig_thread,
	    atabus_sc, &atabus_cfg_lwp,
	    "%scnf", device_xname(atac->atac_dev))) != 0)
		aprint_error_dev(atac->atac_dev,
		    "unable to create config thread: error %d\n", error);
	return;

 out:
	mutex_enter(&atabus_qlock);
	TAILQ_REMOVE(&atabus_initq_head, atabus_initq, atabus_initq);
	cv_broadcast(&atabus_qcv);
	mutex_exit(&atabus_qlock);

	free(atabus_initq, M_DEVBUF);

	ata_delref(chp);

	config_pending_decr(atac->atac_dev);
}

/*
 * atabus_configthread: finish attach of atabus's childrens, in a separate
 * kernel thread.
 */
static void
atabusconfig_thread(void *arg)
{
	struct atabus_softc *atabus_sc = arg;
	struct ata_channel *chp = atabus_sc->sc_chan;
	struct atac_softc *atac = chp->ch_atac;
	struct atabus_initq *atabus_initq = NULL;
	int i, s;

	/* XXX seems wrong */
	mutex_enter(&atabus_qlock);
	atabus_initq = TAILQ_FIRST(&atabus_initq_head);
	KASSERT(atabus_initq->atabus_sc == atabus_sc);
	mutex_exit(&atabus_qlock);

	/*
	 * First look for a port multiplier
	 */
	if (chp->ch_ndrives == PMP_MAX_DRIVES &&
	    chp->ch_drive[PMP_PORT_CTL].drive_type == ATA_DRIVET_PM) {
#if NSATA_PMP > 0
		satapmp_attach(chp);
#else
		aprint_error_dev(atabus_sc->sc_dev,
		    "SATA port multiplier not supported\n");
		/* no problems going on, all drives are ATA_DRIVET_NONE */
#endif
	}

	/*
	 * Attach an ATAPI bus, if needed.
	 */
	KASSERT(chp->ch_ndrives == 0 || chp->ch_drive != NULL);
	for (i = 0; i < chp->ch_ndrives && chp->atapibus == NULL; i++) {
		if (chp->ch_drive[i].drive_type == ATA_DRIVET_ATAPI) {
#if NATAPIBUS > 0
			(*atac->atac_atapibus_attach)(atabus_sc);
#else
			/*
			 * Fake the autoconfig "not configured" message
			 */
			aprint_normal("atapibus at %s not configured\n",
			    device_xname(atac->atac_dev));
			chp->atapibus = NULL;
			s = splbio();
			for (i = 0; i < chp->ch_ndrives; i++) {
				if (chp->ch_drive[i].drive_type == ATA_DRIVET_ATAPI)
					chp->ch_drive[i].drive_type = ATA_DRIVET_NONE;
			}
			splx(s);
#endif
			break;
		}
	}

	for (i = 0; i < chp->ch_ndrives; i++) {
		struct ata_device adev;
		if (chp->ch_drive[i].drive_type != ATA_DRIVET_ATA &&
		    chp->ch_drive[i].drive_type != ATA_DRIVET_OLD) {
			continue;
		}
		if (chp->ch_drive[i].drv_softc != NULL)
			continue;
		memset(&adev, 0, sizeof(struct ata_device));
		adev.adev_bustype = atac->atac_bustype_ata;
		adev.adev_channel = chp->ch_channel;
		adev.adev_openings = 1;
		adev.adev_drv_data = &chp->ch_drive[i];
		chp->ch_drive[i].drv_softc = config_found_ia(atabus_sc->sc_dev,
		    "ata_hl", &adev, ataprint);
		if (chp->ch_drive[i].drv_softc != NULL) {
			ata_probe_caps(&chp->ch_drive[i]);
		} else {
			s = splbio();
			chp->ch_drive[i].drive_type = ATA_DRIVET_NONE;
			splx(s);
		}
	}

	/* now that we know the drives, the controller can set its modes */
	if (atac->atac_set_modes) {
		(*atac->atac_set_modes)(chp);
		ata_print_modes(chp);
	}
#if NATARAID > 0
	if (atac->atac_cap & ATAC_CAP_RAID) {
		for (i = 0; i < chp->ch_ndrives; i++) {
			if (chp->ch_drive[i].drive_type == ATA_DRIVET_ATA) {
				ata_raid_check_component(
				    chp->ch_drive[i].drv_softc);
			}
		}
	}
#endif /* NATARAID > 0 */

	/*
	 * reset drive_flags for unattached devices, reset state for attached
	 * ones
	 */
	s = splbio();
	for (i = 0; i < chp->ch_ndrives; i++) {
		if (chp->ch_drive[i].drive_type == ATA_DRIVET_PM)
			continue;
		if (chp->ch_drive[i].drv_softc == NULL) {
			chp->ch_drive[i].drive_flags = 0;
			chp->ch_drive[i].drive_type = ATA_DRIVET_NONE;
		} else
			chp->ch_drive[i].state = 0;
	}
	splx(s);

	mutex_enter(&atabus_qlock);
	TAILQ_REMOVE(&atabus_initq_head, atabus_initq, atabus_initq);
	cv_broadcast(&atabus_qcv);
	mutex_exit(&atabus_qlock);

	free(atabus_initq, M_DEVBUF);

	ata_delref(chp);

	config_pending_decr(atac->atac_dev);
	kthread_exit(0);
}

/*
 * atabus_thread:
 *
 *	Worker thread for the ATA bus.
 */
static void
atabus_thread(void *arg)
{
	struct atabus_softc *sc = arg;
	struct ata_channel *chp = sc->sc_chan;
	struct ata_xfer *xfer;
	int i, s;

	s = splbio();
	chp->ch_flags |= ATACH_TH_RUN;

	/*
	 * Probe the drives.  Reset type to indicate to controllers
	 * that can re-probe that all drives must be probed..
	 *
	 * Note: ch_ndrives may be changed during the probe.
	 */
	KASSERT(chp->ch_ndrives == 0 || chp->ch_drive != NULL);
	for (i = 0; i < chp->ch_ndrives; i++) {
		chp->ch_drive[i].drive_flags = 0;
		chp->ch_drive[i].drive_type = ATA_DRIVET_NONE;
	}
	splx(s);

	atabusconfig(sc);

	s = splbio();
	for (;;) {
		if ((chp->ch_flags & (ATACH_TH_RESET | ATACH_SHUTDOWN)) == 0 &&
		    (chp->ch_queue->active_xfer == NULL ||
		     chp->ch_queue->queue_freeze == 0)) {
			chp->ch_flags &= ~ATACH_TH_RUN;
			(void) tsleep(&chp->ch_thread, PRIBIO, "atath", 0);
			chp->ch_flags |= ATACH_TH_RUN;
		}
		if (chp->ch_flags & ATACH_SHUTDOWN) {
			break;
		}
		if (chp->ch_flags & ATACH_TH_RESCAN) {
			atabusconfig(sc);
			chp->ch_flags &= ~ATACH_TH_RESCAN;
		}
		if (chp->ch_flags & ATACH_TH_RESET) {
			/*
			 * ata_reset_channel() will freeze 2 times, so
			 * unfreeze one time. Not a problem as we're at splbio
			 */
			chp->ch_queue->queue_freeze--;
			ata_reset_channel(chp, AT_WAIT | chp->ch_reset_flags);
		} else if (chp->ch_queue->active_xfer != NULL &&
			   chp->ch_queue->queue_freeze == 1) {
			/*
			 * Caller has bumped queue_freeze, decrease it.
			 */
			chp->ch_queue->queue_freeze--;
			xfer = chp->ch_queue->active_xfer;
			KASSERT(xfer != NULL);
			(*xfer->c_start)(xfer->c_chp, xfer);
		} else if (chp->ch_queue->queue_freeze > 1)
			panic("ata_thread: queue_freeze");
	}
	splx(s);
	chp->ch_thread = NULL;
	wakeup(&chp->ch_flags);
	kthread_exit(0);
}

/*
 * atabus_match:
 *
 *	Autoconfiguration match routine.
 */
static int
atabus_match(device_t parent, cfdata_t cf, void *aux)
{
	struct ata_channel *chp = aux;

	if (chp == NULL)
		return (0);

	if (cf->cf_loc[ATACF_CHANNEL] != chp->ch_channel &&
	    cf->cf_loc[ATACF_CHANNEL] != ATACF_CHANNEL_DEFAULT)
		return (0);

	return (1);
}

/*
 * atabus_attach:
 *
 *	Autoconfiguration attach routine.
 */
static void
atabus_attach(device_t parent, device_t self, void *aux)
{
	struct atabus_softc *sc = device_private(self);
	struct ata_channel *chp = aux;
	struct atabus_initq *initq;
	int error;

	sc->sc_chan = chp;

	aprint_normal("\n");
	aprint_naive("\n");

	sc->sc_dev = self;

	if (ata_addref(chp))
		return;

	RUN_ONCE(&ata_init_ctrl, atabus_init);

	initq = malloc(sizeof(*initq), M_DEVBUF, M_WAITOK);
	initq->atabus_sc = sc;
	TAILQ_INSERT_TAIL(&atabus_initq_head, initq, atabus_initq);
	config_pending_incr(sc->sc_dev);

	if ((error = kthread_create(PRI_NONE, 0, NULL, atabus_thread, sc,
	    &chp->ch_thread, "%s", device_xname(self))) != 0)
		aprint_error_dev(self,
		    "unable to create kernel thread: error %d\n", error);

	if (!pmf_device_register(self, atabus_suspend, atabus_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

/*
 * atabus_detach:
 *
 *	Autoconfiguration detach routine.
 */
static int
atabus_detach(device_t self, int flags)
{
	struct atabus_softc *sc = device_private(self);
	struct ata_channel *chp = sc->sc_chan;
	device_t dev = NULL;
	int s, i, error = 0;

	/* Shutdown the channel. */
	s = splbio();		/* XXX ALSO NEED AN INTERLOCK HERE. */
	chp->ch_flags |= ATACH_SHUTDOWN;
	splx(s);

	wakeup(&chp->ch_thread);

	while (chp->ch_thread != NULL)
		(void) tsleep(&chp->ch_flags, PRIBIO, "atadown", 0);


	/*
	 * Detach atapibus and its children.
	 */
	if ((dev = chp->atapibus) != NULL) {
		ATADEBUG_PRINT(("atabus_detach: %s: detaching %s\n",
		    device_xname(self), device_xname(dev)), DEBUG_DETACH);

		error = config_detach(dev, flags);
		if (error)
			goto out;
		KASSERT(chp->atapibus == NULL);
	}

	KASSERT(chp->ch_ndrives == 0 || chp->ch_drive != NULL);

	/*
	 * Detach our other children.
	 */
	for (i = 0; i < chp->ch_ndrives; i++) {
		if (chp->ch_drive[i].drive_type == ATA_DRIVET_ATAPI)
			continue;
		if (chp->ch_drive[i].drive_type == ATA_DRIVET_PM)
			chp->ch_drive[i].drive_type = ATA_DRIVET_NONE;
		if ((dev = chp->ch_drive[i].drv_softc) != NULL) {
			ATADEBUG_PRINT(("%s.%d: %s: detaching %s\n", __func__,
			    __LINE__, device_xname(self), device_xname(dev)),
			    DEBUG_DETACH);
			error = config_detach(dev, flags);
			if (error)
				goto out;
			KASSERT(chp->ch_drive[i].drv_softc == NULL);
			KASSERT(chp->ch_drive[i].drive_type == 0);
		}
	}
	atabus_free_drives(chp);

 out:
#ifdef ATADEBUG
	if (dev != NULL && error != 0)
		ATADEBUG_PRINT(("%s: %s: error %d detaching %s\n", __func__,
		    device_xname(self), error, device_xname(dev)),
		    DEBUG_DETACH);
#endif /* ATADEBUG */

	return (error);
}

void
atabus_childdetached(device_t self, device_t child)
{
	bool found = false;
	struct atabus_softc *sc = device_private(self);
	struct ata_channel *chp = sc->sc_chan;
	int i;

	KASSERT(chp->ch_ndrives == 0 || chp->ch_drive != NULL);
	/*
	 * atapibus detached.
	 */
	if (child == chp->atapibus) {
		chp->atapibus = NULL;
		found = true;
		for (i = 0; i < chp->ch_ndrives; i++) {
			if (chp->ch_drive[i].drive_type != ATA_DRIVET_ATAPI)
				continue;
			KASSERT(chp->ch_drive[i].drv_softc != NULL);
			chp->ch_drive[i].drv_softc = NULL;
			chp->ch_drive[i].drive_flags = 0;
			chp->ch_drive[i].drive_type = ATA_DRIVET_NONE;
		}
	}

	/*
	 * Detach our other children.
	 */
	for (i = 0; i < chp->ch_ndrives; i++) {
		if (chp->ch_drive[i].drive_type == ATA_DRIVET_ATAPI)
			continue;
		if (child == chp->ch_drive[i].drv_softc) {
			chp->ch_drive[i].drv_softc = NULL;
			chp->ch_drive[i].drive_flags = 0;
			if (chp->ch_drive[i].drive_type == ATA_DRIVET_PM)
				chp->ch_satapmp_nports = 0;
			chp->ch_drive[i].drive_type = ATA_DRIVET_NONE;
			found = true;
		}
	}

	if (!found)
		panic("%s: unknown child %p", device_xname(self),
		    (const void *)child);
}

CFATTACH_DECL3_NEW(atabus, sizeof(struct atabus_softc),
    atabus_match, atabus_attach, atabus_detach, NULL, atabus_rescan,
    atabus_childdetached, DVF_DETACH_SHUTDOWN);

/*****************************************************************************
 * Common ATA bus operations.
 *****************************************************************************/

/* allocate/free the channel's ch_drive[] array */
int
atabus_alloc_drives(struct ata_channel *chp, int ndrives)
{
	int i;
	if (chp->ch_ndrives != ndrives)
		atabus_free_drives(chp);
	if (chp->ch_drive == NULL) {
		chp->ch_drive = malloc(
		    sizeof(struct ata_drive_datas) * ndrives,
		    M_DEVBUF, M_NOWAIT | M_ZERO);
	}
	if (chp->ch_drive == NULL) {
	    aprint_error_dev(chp->ch_atac->atac_dev,
		"can't alloc drive array\n");
	    chp->ch_ndrives = 0;
	    return ENOMEM;
	};
	for (i = 0; i < ndrives; i++) {
		chp->ch_drive[i].chnl_softc = chp;
		chp->ch_drive[i].drive = i;
	}
	chp->ch_ndrives = ndrives;
	return 0;
}

void
atabus_free_drives(struct ata_channel *chp)
{
#ifdef DIAGNOSTIC
	int i;
	int dopanic = 0;
	KASSERT(chp->ch_ndrives == 0 || chp->ch_drive != NULL);
	for (i = 0; i < chp->ch_ndrives; i++) {
		if (chp->ch_drive[i].drive_type != ATA_DRIVET_NONE) {
			printf("%s: ch_drive[%d] type %d != ATA_DRIVET_NONE\n",
			    device_xname(chp->atabus), i,
			    chp->ch_drive[i].drive_type);
			dopanic = 1;
		}
		if (chp->ch_drive[i].drv_softc != NULL) {
			printf("%s: ch_drive[%d] attached to %s\n",
			    device_xname(chp->atabus), i,
			    device_xname(chp->ch_drive[i].drv_softc));
			dopanic = 1;
		}
	}
	if (dopanic)
		panic("atabus_free_drives");
#endif

	if (chp->ch_drive == NULL)
		return;
	chp->ch_ndrives = 0;
	free(chp->ch_drive, M_DEVBUF);
	chp->ch_drive = NULL;
}

/* Get the disk's parameters */
int
ata_get_params(struct ata_drive_datas *drvp, uint8_t flags,
    struct ataparams *prms)
{
	struct ata_command ata_c;
	struct ata_channel *chp = drvp->chnl_softc;
	struct atac_softc *atac = chp->ch_atac;
	char *tb;
	int i, rv;
	uint16_t *p;

	ATADEBUG_PRINT(("%s\n", __func__), DEBUG_FUNCS);

	tb = kmem_zalloc(DEV_BSIZE, KM_SLEEP);
	memset(prms, 0, sizeof(struct ataparams));
	memset(&ata_c, 0, sizeof(struct ata_command));

	if (drvp->drive_type == ATA_DRIVET_ATA) {
		ata_c.r_command = WDCC_IDENTIFY;
		ata_c.r_st_bmask = WDCS_DRDY;
		ata_c.r_st_pmask = WDCS_DRQ;
		ata_c.timeout = 3000; /* 3s */
	} else if (drvp->drive_type == ATA_DRIVET_ATAPI) {
		ata_c.r_command = ATAPI_IDENTIFY_DEVICE;
		ata_c.r_st_bmask = 0;
		ata_c.r_st_pmask = WDCS_DRQ;
		ata_c.timeout = 10000; /* 10s */
	} else {
		ATADEBUG_PRINT(("ata_get_parms: no disks\n"),
		    DEBUG_FUNCS|DEBUG_PROBE);
		rv = CMD_ERR;
		goto out;
	}
	ata_c.flags = AT_READ | flags;
	ata_c.data = tb;
	ata_c.bcount = DEV_BSIZE;
	if ((*atac->atac_bustype_ata->ata_exec_command)(drvp,
						&ata_c) != ATACMD_COMPLETE) {
		ATADEBUG_PRINT(("ata_get_parms: wdc_exec_command failed\n"),
		    DEBUG_FUNCS|DEBUG_PROBE);
		rv = CMD_AGAIN;
		goto out;
	}
	if (ata_c.flags & (AT_ERROR | AT_TIMEOU | AT_DF)) {
		ATADEBUG_PRINT(("ata_get_parms: ata_c.flags=0x%x\n",
		    ata_c.flags), DEBUG_FUNCS|DEBUG_PROBE);
		rv = CMD_ERR;
		goto out;
	}
	/* if we didn't read any data something is wrong */
	if ((ata_c.flags & AT_XFDONE) == 0) {
		rv = CMD_ERR;
		goto out;
	}

	/* Read in parameter block. */
	memcpy(prms, tb, sizeof(struct ataparams));

	/*
	 * Shuffle string byte order.
	 * ATAPI NEC, Mitsumi and Pioneer drives and
	 * old ATA TDK CompactFlash cards
	 * have different byte order.
	 */
#if BYTE_ORDER == BIG_ENDIAN
# define M(n)	prms->atap_model[(n) ^ 1]
#else
# define M(n)	prms->atap_model[n]
#endif
	if (
#if BYTE_ORDER == BIG_ENDIAN
	    !
#endif
	    ((drvp->drive_type == ATA_DRIVET_ATAPI) ?
	     ((M(0) == 'N' && M(1) == 'E') ||
	      (M(0) == 'F' && M(1) == 'X') ||
	      (M(0) == 'P' && M(1) == 'i')) :
	     ((M(0) == 'T' && M(1) == 'D' && M(2) == 'K')))) {
		rv = CMD_OK;
		goto out;
	     }
#undef M
	for (i = 0; i < sizeof(prms->atap_model); i += 2) {
		p = (uint16_t *)(prms->atap_model + i);
		*p = bswap16(*p);
	}
	for (i = 0; i < sizeof(prms->atap_serial); i += 2) {
		p = (uint16_t *)(prms->atap_serial + i);
		*p = bswap16(*p);
	}
	for (i = 0; i < sizeof(prms->atap_revision); i += 2) {
		p = (uint16_t *)(prms->atap_revision + i);
		*p = bswap16(*p);
	}

	rv = CMD_OK;
 out:
	kmem_free(tb, DEV_BSIZE);
	return rv;
}

int
ata_set_mode(struct ata_drive_datas *drvp, uint8_t mode, uint8_t flags)
{
	struct ata_command ata_c;
	struct ata_channel *chp = drvp->chnl_softc;
	struct atac_softc *atac = chp->ch_atac;

	ATADEBUG_PRINT(("ata_set_mode=0x%x\n", mode), DEBUG_FUNCS);
	memset(&ata_c, 0, sizeof(struct ata_command));

	ata_c.r_command = SET_FEATURES;
	ata_c.r_st_bmask = 0;
	ata_c.r_st_pmask = 0;
	ata_c.r_features = WDSF_SET_MODE;
	ata_c.r_count = mode;
	ata_c.flags = flags;
	ata_c.timeout = 1000; /* 1s */
	if ((*atac->atac_bustype_ata->ata_exec_command)(drvp,
						&ata_c) != ATACMD_COMPLETE)
		return CMD_AGAIN;
	if (ata_c.flags & (AT_ERROR | AT_TIMEOU | AT_DF)) {
		return CMD_ERR;
	}
	return CMD_OK;
}

#if NATA_DMA
void
ata_dmaerr(struct ata_drive_datas *drvp, int flags)
{
	/*
	 * Downgrade decision: if we get NERRS_MAX in NXFER.
	 * We start with n_dmaerrs set to NERRS_MAX-1 so that the
	 * first error within the first NXFER ops will immediatly trigger
	 * a downgrade.
	 * If we got an error and n_xfers is bigger than NXFER reset counters.
	 */
	drvp->n_dmaerrs++;
	if (drvp->n_dmaerrs >= NERRS_MAX && drvp->n_xfers <= NXFER) {
		ata_downgrade_mode(drvp, flags);
		drvp->n_dmaerrs = NERRS_MAX-1;
		drvp->n_xfers = 0;
		return;
	}
	if (drvp->n_xfers > NXFER) {
		drvp->n_dmaerrs = 1; /* just got an error */
		drvp->n_xfers = 1; /* restart counting from this error */
	}
}
#endif	/* NATA_DMA */

/*
 * freeze the queue and wait for the controller to be idle. Caller has to
 * unfreeze/restart the queue
 */
void
ata_queue_idle(struct ata_queue *queue)
{
	int s = splbio();
	queue->queue_freeze++;
	while (queue->active_xfer != NULL) {
		queue->queue_flags |= QF_IDLE_WAIT;
		tsleep(&queue->queue_flags, PRIBIO, "qidl", 0);
	}
	splx(s);
}

/*
 * Add a command to the queue and start controller.
 *
 * MUST BE CALLED AT splbio()!
 */
void
ata_exec_xfer(struct ata_channel *chp, struct ata_xfer *xfer)
{

	ATADEBUG_PRINT(("ata_exec_xfer %p channel %d drive %d\n", xfer,
	    chp->ch_channel, xfer->c_drive), DEBUG_XFERS);

	/* complete xfer setup */
	xfer->c_chp = chp;

	/* insert at the end of command list */
	TAILQ_INSERT_TAIL(&chp->ch_queue->queue_xfer, xfer, c_xferchain);
	ATADEBUG_PRINT(("atastart from ata_exec_xfer, flags 0x%x\n",
	    chp->ch_flags), DEBUG_XFERS);
	/*
	 * if polling and can sleep, wait for the xfer to be at head of queue
	 */
	if ((xfer->c_flags & (C_POLL | C_WAIT)) ==  (C_POLL | C_WAIT)) {
		while (chp->ch_queue->active_xfer != NULL ||
		    TAILQ_FIRST(&chp->ch_queue->queue_xfer) != xfer) {
			xfer->c_flags |= C_WAITACT;
			tsleep(xfer, PRIBIO, "ataact", 0);
			xfer->c_flags &= ~C_WAITACT;
			if (xfer->c_flags & C_FREE) {
				ata_free_xfer(chp, xfer);
				return;
			}
		}
	}
	atastart(chp);
}

/*
 * Start I/O on a controller, for the given channel.
 * The first xfer may be not for our channel if the channel queues
 * are shared.
 *
 * MUST BE CALLED AT splbio()!
 */
void
atastart(struct ata_channel *chp)
{
	struct atac_softc *atac = chp->ch_atac;
	struct ata_xfer *xfer;

#ifdef ATA_DEBUG
	int spl1, spl2;

	spl1 = splbio();
	spl2 = splbio();
	if (spl2 != spl1) {
		printf("atastart: not at splbio()\n");
		panic("atastart");
	}
	splx(spl2);
	splx(spl1);
#endif /* ATA_DEBUG */

	/* is there a xfer ? */
	if ((xfer = TAILQ_FIRST(&chp->ch_queue->queue_xfer)) == NULL)
		return;

	/* adjust chp, in case we have a shared queue */
	chp = xfer->c_chp;

	if (chp->ch_queue->active_xfer != NULL) {
		return; /* channel aleady active */
	}
	if (__predict_false(chp->ch_queue->queue_freeze > 0)) {
		if (chp->ch_queue->queue_flags & QF_IDLE_WAIT) {
			chp->ch_queue->queue_flags &= ~QF_IDLE_WAIT;
			wakeup(&chp->ch_queue->queue_flags);
		}
		return; /* queue frozen */
	}
	/*
	 * if someone is waiting for the command to be active, wake it up
	 * and let it process the command
	 */
	if (xfer->c_flags & C_WAITACT) {
		ATADEBUG_PRINT(("atastart: xfer %p channel %d drive %d "
		    "wait active\n", xfer, chp->ch_channel, xfer->c_drive),
		    DEBUG_XFERS);
		wakeup(xfer);
		return;
	}
#ifdef DIAGNOSTIC
	if ((chp->ch_flags & ATACH_IRQ_WAIT) != 0)
		panic("atastart: channel waiting for irq");
#endif
	if (atac->atac_claim_hw)
		if (!(*atac->atac_claim_hw)(chp, 0))
			return;

	ATADEBUG_PRINT(("atastart: xfer %p channel %d drive %d\n", xfer,
	    chp->ch_channel, xfer->c_drive), DEBUG_XFERS);
	if (chp->ch_drive[xfer->c_drive].drive_flags & ATA_DRIVE_RESET) {
		chp->ch_drive[xfer->c_drive].drive_flags &= ~ATA_DRIVE_RESET;
		chp->ch_drive[xfer->c_drive].state = 0;
	}
	chp->ch_queue->active_xfer = xfer;
	TAILQ_REMOVE(&chp->ch_queue->queue_xfer, xfer, c_xferchain);

	if (atac->atac_cap & ATAC_CAP_NOIRQ)
		KASSERT(xfer->c_flags & C_POLL);

	xfer->c_start(chp, xfer);
}

struct ata_xfer *
ata_get_xfer(int flags)
{
	struct ata_xfer *xfer;
	int s;

	s = splbio();
	xfer = pool_get(&ata_xfer_pool,
	    ((flags & ATAXF_NOSLEEP) != 0 ? PR_NOWAIT : PR_WAITOK));
	splx(s);
	if (xfer != NULL) {
		memset(xfer, 0, sizeof(struct ata_xfer));
	}
	return xfer;
}

void
ata_free_xfer(struct ata_channel *chp, struct ata_xfer *xfer)
{
	struct atac_softc *atac = chp->ch_atac;
	int s;

	if (xfer->c_flags & C_WAITACT) {
		/* Someone is waiting for this xfer, so we can't free now */
		xfer->c_flags |= C_FREE;
		wakeup(xfer);
		return;
	}

#if NATA_PIOBM		/* XXX wdc dependent code */
	if (xfer->c_flags & C_PIOBM) {
		struct wdc_softc *wdc = CHAN_TO_WDC(chp);

		/* finish the busmastering PIO */
		(*wdc->piobm_done)(wdc->dma_arg,
		    chp->ch_channel, xfer->c_drive);
		chp->ch_flags &= ~(ATACH_DMA_WAIT | ATACH_PIOBM_WAIT | ATACH_IRQ_WAIT);
	}
#endif

	if (atac->atac_free_hw)
		(*atac->atac_free_hw)(chp);
	s = splbio();
	pool_put(&ata_xfer_pool, xfer);
	splx(s);
}

/*
 * Kill off all pending xfers for a ata_channel.
 *
 * Must be called at splbio().
 */
void
ata_kill_pending(struct ata_drive_datas *drvp)
{
	struct ata_channel *chp = drvp->chnl_softc;
	struct ata_xfer *xfer, *next_xfer;
	int s = splbio();

	for (xfer = TAILQ_FIRST(&chp->ch_queue->queue_xfer);
	    xfer != NULL; xfer = next_xfer) {
		next_xfer = TAILQ_NEXT(xfer, c_xferchain);
		if (xfer->c_chp != chp || xfer->c_drive != drvp->drive)
			continue;
		TAILQ_REMOVE(&chp->ch_queue->queue_xfer, xfer, c_xferchain);
		(*xfer->c_kill_xfer)(chp, xfer, KILL_GONE);
	}

	while ((xfer = chp->ch_queue->active_xfer) != NULL) {
		if (xfer->c_chp == chp && xfer->c_drive == drvp->drive) {
			drvp->drive_flags |= ATA_DRIVE_WAITDRAIN;
			(void) tsleep(&chp->ch_queue->active_xfer,
			    PRIBIO, "atdrn", 0);
		} else {
			/* no more xfer for us */
			break;
		}
	}
	splx(s);
}

/*
 * ata_reset_channel:
 *
 *	Reset and ATA channel.
 *
 *	MUST BE CALLED AT splbio()!
 */
void
ata_reset_channel(struct ata_channel *chp, int flags)
{
	struct atac_softc *atac = chp->ch_atac;
	int drive;

#ifdef ATA_DEBUG
	int spl1, spl2;

	spl1 = splbio();
	spl2 = splbio();
	if (spl2 != spl1) {
		printf("ata_reset_channel: not at splbio()\n");
		panic("ata_reset_channel");
	}
	splx(spl2);
	splx(spl1);
#endif /* ATA_DEBUG */

	chp->ch_queue->queue_freeze++;

	/*
	 * If we can poll or wait it's OK, otherwise wake up the
	 * kernel thread to do it for us.
	 */
	ATADEBUG_PRINT(("ata_reset_channel flags 0x%x ch_flags 0x%x\n",
	    flags, chp->ch_flags), DEBUG_FUNCS | DEBUG_XFERS);
	if ((flags & (AT_POLL | AT_WAIT)) == 0) {
		if (chp->ch_flags & ATACH_TH_RESET) {
			/* No need to schedule a reset more than one time. */
			chp->ch_queue->queue_freeze--;
			return;
		}
		chp->ch_flags |= ATACH_TH_RESET;
		chp->ch_reset_flags = flags & (AT_RST_EMERG | AT_RST_NOCMD);
		wakeup(&chp->ch_thread);
		return;
	}

	(*atac->atac_bustype_ata->ata_reset_channel)(chp, flags);

	KASSERT(chp->ch_ndrives == 0 || chp->ch_drive != NULL);
	for (drive = 0; drive < chp->ch_ndrives; drive++)
		chp->ch_drive[drive].state = 0;

	chp->ch_flags &= ~ATACH_TH_RESET;
	if ((flags & AT_RST_EMERG) == 0)  {
		chp->ch_queue->queue_freeze--;
		atastart(chp);
	} else {
		/* make sure that we can use polled commands */
		TAILQ_INIT(&chp->ch_queue->queue_xfer);
		chp->ch_queue->queue_freeze = 0;
		chp->ch_queue->active_xfer = NULL;
	}
}

int
ata_addref(struct ata_channel *chp)
{
	struct atac_softc *atac = chp->ch_atac;
	struct scsipi_adapter *adapt = &atac->atac_atapi_adapter._generic;
	int s, error = 0;

	s = splbio();
	if (adapt->adapt_refcnt++ == 0 &&
	    adapt->adapt_enable != NULL) {
		error = (*adapt->adapt_enable)(atac->atac_dev, 1);
		if (error)
			adapt->adapt_refcnt--;
	}
	splx(s);
	return (error);
}

void
ata_delref(struct ata_channel *chp)
{
	struct atac_softc *atac = chp->ch_atac;
	struct scsipi_adapter *adapt = &atac->atac_atapi_adapter._generic;
	int s;

	s = splbio();
	if (adapt->adapt_refcnt-- == 1 &&
	    adapt->adapt_enable != NULL)
		(void) (*adapt->adapt_enable)(atac->atac_dev, 0);
	splx(s);
}

void
ata_print_modes(struct ata_channel *chp)
{
	struct atac_softc *atac = chp->ch_atac;
	int drive;
	struct ata_drive_datas *drvp;

	KASSERT(chp->ch_ndrives == 0 || chp->ch_drive != NULL);
	for (drive = 0; drive < chp->ch_ndrives; drive++) {
		drvp = &chp->ch_drive[drive];
		if (drvp->drive_type == ATA_DRIVET_NONE ||
		    drvp->drv_softc == NULL)
			continue;
		aprint_verbose("%s(%s:%d:%d): using PIO mode %d",
			device_xname(drvp->drv_softc),
			device_xname(atac->atac_dev),
			chp->ch_channel, drvp->drive, drvp->PIO_mode);
#if NATA_DMA
		if (drvp->drive_flags & ATA_DRIVE_DMA)
			aprint_verbose(", DMA mode %d", drvp->DMA_mode);
#if NATA_UDMA
		if (drvp->drive_flags & ATA_DRIVE_UDMA) {
			aprint_verbose(", Ultra-DMA mode %d", drvp->UDMA_mode);
			if (drvp->UDMA_mode == 2)
				aprint_verbose(" (Ultra/33)");
			else if (drvp->UDMA_mode == 4)
				aprint_verbose(" (Ultra/66)");
			else if (drvp->UDMA_mode == 5)
				aprint_verbose(" (Ultra/100)");
			else if (drvp->UDMA_mode == 6)
				aprint_verbose(" (Ultra/133)");
		}
#endif	/* NATA_UDMA */
#endif	/* NATA_DMA */
#if NATA_DMA || NATA_PIOBM
		if (0
#if NATA_DMA
		    || (drvp->drive_flags & (ATA_DRIVE_DMA | ATA_DRIVE_UDMA))
#endif
#if NATA_PIOBM
		    /* PIOBM capable controllers use DMA for PIO commands */
		    || (atac->atac_cap & ATAC_CAP_PIOBM)
#endif
		    )
			aprint_verbose(" (using DMA)");
#endif	/* NATA_DMA || NATA_PIOBM */
		aprint_verbose("\n");
	}
}

#if NATA_DMA
/*
 * downgrade the transfer mode of a drive after an error. return 1 if
 * downgrade was possible, 0 otherwise.
 *
 * MUST BE CALLED AT splbio()!
 */
int
ata_downgrade_mode(struct ata_drive_datas *drvp, int flags)
{
	struct ata_channel *chp = drvp->chnl_softc;
	struct atac_softc *atac = chp->ch_atac;
	device_t drv_dev = drvp->drv_softc;
	int cf_flags = device_cfdata(drv_dev)->cf_flags;

	/* if drive or controller don't know its mode, we can't do much */
	if ((drvp->drive_flags & ATA_DRIVE_MODE) == 0 ||
	    (atac->atac_set_modes == NULL))
		return 0;
	/* current drive mode was set by a config flag, let it this way */
	if ((cf_flags & ATA_CONFIG_PIO_SET) ||
	    (cf_flags & ATA_CONFIG_DMA_SET) ||
	    (cf_flags & ATA_CONFIG_UDMA_SET))
		return 0;

#if NATA_UDMA
	/*
	 * If we were using Ultra-DMA mode, downgrade to the next lower mode.
	 */
	if ((drvp->drive_flags & ATA_DRIVE_UDMA) && drvp->UDMA_mode >= 2) {
		drvp->UDMA_mode--;
		aprint_error_dev(drv_dev,
		    "transfer error, downgrading to Ultra-DMA mode %d\n",
		    drvp->UDMA_mode);
	}
#endif

	/*
	 * If we were using ultra-DMA, don't downgrade to multiword DMA.
	 */
	else if (drvp->drive_flags & (ATA_DRIVE_DMA | ATA_DRIVE_UDMA)) {
		drvp->drive_flags &= ~(ATA_DRIVE_DMA | ATA_DRIVE_UDMA);
		drvp->PIO_mode = drvp->PIO_cap;
		aprint_error_dev(drv_dev,
		    "transfer error, downgrading to PIO mode %d\n",
		    drvp->PIO_mode);
	} else /* already using PIO, can't downgrade */
		return 0;

	(*atac->atac_set_modes)(chp);
	ata_print_modes(chp);
	/* reset the channel, which will schedule all drives for setup */
	ata_reset_channel(chp, flags | AT_RST_NOCMD);
	return 1;
}
#endif	/* NATA_DMA */

/*
 * Probe drive's capabilities, for use by the controller later
 * Assumes drvp points to an existing drive.
 */
void
ata_probe_caps(struct ata_drive_datas *drvp)
{
	struct ataparams params, params2;
	struct ata_channel *chp = drvp->chnl_softc;
	struct atac_softc *atac = chp->ch_atac;
	device_t drv_dev = drvp->drv_softc;
	int i, printed, s;
	const char *sep = "";
	int cf_flags;

	if (ata_get_params(drvp, AT_WAIT, &params) != CMD_OK) {
		/* IDENTIFY failed. Can't tell more about the device */
		return;
	}
	if ((atac->atac_cap & (ATAC_CAP_DATA16 | ATAC_CAP_DATA32)) ==
	    (ATAC_CAP_DATA16 | ATAC_CAP_DATA32)) {
		/*
		 * Controller claims 16 and 32 bit transfers.
		 * Re-do an IDENTIFY with 32-bit transfers,
		 * and compare results.
		 */
		s = splbio();
		drvp->drive_flags |= ATA_DRIVE_CAP32;
		splx(s);
		ata_get_params(drvp, AT_WAIT, &params2);
		if (memcmp(&params, &params2, sizeof(struct ataparams)) != 0) {
			/* Not good. fall back to 16bits */
			s = splbio();
			drvp->drive_flags &= ~ATA_DRIVE_CAP32;
			splx(s);
		} else {
			aprint_verbose_dev(drv_dev, "32-bit data port\n");
		}
	}
#if 0 /* Some ultra-DMA drives claims to only support ATA-3. sigh */
	if (params.atap_ata_major > 0x01 &&
	    params.atap_ata_major != 0xffff) {
		for (i = 14; i > 0; i--) {
			if (params.atap_ata_major & (1 << i)) {
				aprint_verbose_dev(drv_dev,
				    "ATA version %d\n", i);
				drvp->ata_vers = i;
				break;
			}
		}
	}
#endif

	/* An ATAPI device is at last PIO mode 3 */
	if (drvp->drive_type == ATA_DRIVET_ATAPI)
		drvp->PIO_mode = 3;

	/*
	 * It's not in the specs, but it seems that some drive
	 * returns 0xffff in atap_extensions when this field is invalid
	 */
	if (params.atap_extensions != 0xffff &&
	    (params.atap_extensions & WDC_EXT_MODES)) {
		printed = 0;
		/*
		 * XXX some drives report something wrong here (they claim to
		 * support PIO mode 8 !). As mode is coded on 3 bits in
		 * SET FEATURE, limit it to 7 (so limit i to 4).
		 * If higher mode than 7 is found, abort.
		 */
		for (i = 7; i >= 0; i--) {
			if ((params.atap_piomode_supp & (1 << i)) == 0)
				continue;
			if (i > 4)
				return;
			/*
			 * See if mode is accepted.
			 * If the controller can't set its PIO mode,
			 * assume the defaults are good, so don't try
			 * to set it
			 */
			if (atac->atac_set_modes)
				/*
				 * It's OK to pool here, it's fast enough
				 * to not bother waiting for interrupt
				 */
				if (ata_set_mode(drvp, 0x08 | (i + 3),
				   AT_WAIT) != CMD_OK)
					continue;
			if (!printed) {
				aprint_verbose_dev(drv_dev,
				    "drive supports PIO mode %d", i + 3);
				sep = ",";
				printed = 1;
			}
			/*
			 * If controller's driver can't set its PIO mode,
			 * get the highter one for the drive.
			 */
			if (atac->atac_set_modes == NULL ||
			    atac->atac_pio_cap >= i + 3) {
				drvp->PIO_mode = i + 3;
				drvp->PIO_cap = i + 3;
				break;
			}
		}
		if (!printed) {
			/*
			 * We didn't find a valid PIO mode.
			 * Assume the values returned for DMA are buggy too
			 */
			return;
		}
		s = splbio();
		drvp->drive_flags |= ATA_DRIVE_MODE;
		splx(s);
		printed = 0;
		for (i = 7; i >= 0; i--) {
			if ((params.atap_dmamode_supp & (1 << i)) == 0)
				continue;
#if NATA_DMA
			if ((atac->atac_cap & ATAC_CAP_DMA) &&
			    atac->atac_set_modes != NULL)
				if (ata_set_mode(drvp, 0x20 | i, AT_WAIT)
				    != CMD_OK)
					continue;
#endif
			if (!printed) {
				aprint_verbose("%s DMA mode %d", sep, i);
				sep = ",";
				printed = 1;
			}
#if NATA_DMA
			if (atac->atac_cap & ATAC_CAP_DMA) {
				if (atac->atac_set_modes != NULL &&
				    atac->atac_dma_cap < i)
					continue;
				drvp->DMA_mode = i;
				drvp->DMA_cap = i;
				s = splbio();
				drvp->drive_flags |= ATA_DRIVE_DMA;
				splx(s);
			}
#endif
			break;
		}
		if (params.atap_extensions & WDC_EXT_UDMA_MODES) {
			printed = 0;
			for (i = 7; i >= 0; i--) {
				if ((params.atap_udmamode_supp & (1 << i))
				    == 0)
					continue;
#if NATA_UDMA
				if (atac->atac_set_modes != NULL &&
				    (atac->atac_cap & ATAC_CAP_UDMA))
					if (ata_set_mode(drvp, 0x40 | i,
					    AT_WAIT) != CMD_OK)
						continue;
#endif
				if (!printed) {
					aprint_verbose("%s Ultra-DMA mode %d",
					    sep, i);
					if (i == 2)
						aprint_verbose(" (Ultra/33)");
					else if (i == 4)
						aprint_verbose(" (Ultra/66)");
					else if (i == 5)
						aprint_verbose(" (Ultra/100)");
					else if (i == 6)
						aprint_verbose(" (Ultra/133)");
					sep = ",";
					printed = 1;
				}
#if NATA_UDMA
				if (atac->atac_cap & ATAC_CAP_UDMA) {
					if (atac->atac_set_modes != NULL &&
					    atac->atac_udma_cap < i)
						continue;
					drvp->UDMA_mode = i;
					drvp->UDMA_cap = i;
					s = splbio();
					drvp->drive_flags |= ATA_DRIVE_UDMA;
					splx(s);
				}
#endif
				break;
			}
		}
		aprint_verbose("\n");
	}

	s = splbio();
	drvp->drive_flags &= ~ATA_DRIVE_NOSTREAM;
	if (drvp->drive_type == ATA_DRIVET_ATAPI) {
		if (atac->atac_cap & ATAC_CAP_ATAPI_NOSTREAM)
			drvp->drive_flags |= ATA_DRIVE_NOSTREAM;
	} else {
		if (atac->atac_cap & ATAC_CAP_ATA_NOSTREAM)
			drvp->drive_flags |= ATA_DRIVE_NOSTREAM;
	}
	splx(s);

	/* Try to guess ATA version here, if it didn't get reported */
	if (drvp->ata_vers == 0) {
#if NATA_UDMA
		if (drvp->drive_flags & ATA_DRIVE_UDMA)
			drvp->ata_vers = 4; /* should be at last ATA-4 */
		else
#endif
		if (drvp->PIO_cap > 2)
			drvp->ata_vers = 2; /* should be at last ATA-2 */
	}
	cf_flags = device_cfdata(drv_dev)->cf_flags;
	if (cf_flags & ATA_CONFIG_PIO_SET) {
		s = splbio();
		drvp->PIO_mode =
		    (cf_flags & ATA_CONFIG_PIO_MODES) >> ATA_CONFIG_PIO_OFF;
		drvp->drive_flags |= ATA_DRIVE_MODE;
		splx(s);
	}
#if NATA_DMA
	if ((atac->atac_cap & ATAC_CAP_DMA) == 0) {
		/* don't care about DMA modes */
		return;
	}
	if (cf_flags & ATA_CONFIG_DMA_SET) {
		s = splbio();
		if ((cf_flags & ATA_CONFIG_DMA_MODES) ==
		    ATA_CONFIG_DMA_DISABLE) {
			drvp->drive_flags &= ~ATA_DRIVE_DMA;
		} else {
			drvp->DMA_mode = (cf_flags & ATA_CONFIG_DMA_MODES) >>
			    ATA_CONFIG_DMA_OFF;
			drvp->drive_flags |= ATA_DRIVE_DMA | ATA_DRIVE_MODE;
		}
		splx(s);
	}
#if NATA_UDMA
	if ((atac->atac_cap & ATAC_CAP_UDMA) == 0) {
		/* don't care about UDMA modes */
		return;
	}
	if (cf_flags & ATA_CONFIG_UDMA_SET) {
		s = splbio();
		if ((cf_flags & ATA_CONFIG_UDMA_MODES) ==
		    ATA_CONFIG_UDMA_DISABLE) {
			drvp->drive_flags &= ~ATA_DRIVE_UDMA;
		} else {
			drvp->UDMA_mode = (cf_flags & ATA_CONFIG_UDMA_MODES) >>
			    ATA_CONFIG_UDMA_OFF;
			drvp->drive_flags |= ATA_DRIVE_UDMA | ATA_DRIVE_MODE;
		}
		splx(s);
	}
#endif	/* NATA_UDMA */
#endif	/* NATA_DMA */
}

/* management of the /dev/atabus* devices */
int
atabusopen(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct atabus_softc *sc;
	int error;

	sc = device_lookup_private(&atabus_cd, minor(dev));
	if (sc == NULL)
		return (ENXIO);

	if (sc->sc_flags & ATABUSCF_OPEN)
		return (EBUSY);

	if ((error = ata_addref(sc->sc_chan)) != 0)
		return (error);

	sc->sc_flags |= ATABUSCF_OPEN;

	return (0);
}


int
atabusclose(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct atabus_softc *sc =
	    device_lookup_private(&atabus_cd, minor(dev));

	ata_delref(sc->sc_chan);

	sc->sc_flags &= ~ATABUSCF_OPEN;

	return (0);
}

int
atabusioctl(dev_t dev, u_long cmd, void *addr, int flag, struct lwp *l)
{
	struct atabus_softc *sc =
	    device_lookup_private(&atabus_cd, minor(dev));
	struct ata_channel *chp = sc->sc_chan;
	int min_drive, max_drive, drive;
	int error;
	int s;

	/*
	 * Enforce write permission for ioctls that change the
	 * state of the bus.  Host adapter specific ioctls must
	 * be checked by the adapter driver.
	 */
	switch (cmd) {
	case ATABUSIOSCAN:
	case ATABUSIODETACH:
	case ATABUSIORESET:
		if ((flag & FWRITE) == 0)
			return (EBADF);
	}

	switch (cmd) {
	case ATABUSIORESET:
		s = splbio();
		ata_reset_channel(sc->sc_chan, AT_WAIT | AT_POLL);
		splx(s);
		return 0;
	case ATABUSIOSCAN:
	{
#if 0
		struct atabusioscan_args *a=
		    (struct atabusioscan_args *)addr;
#endif
		if ((chp->ch_drive[0].drive_type == ATA_DRIVET_OLD) ||
		    (chp->ch_drive[1].drive_type == ATA_DRIVET_OLD))
			return (EOPNOTSUPP);
		return (EOPNOTSUPP);
	}
	case ATABUSIODETACH:
	{
		struct atabusiodetach_args *a=
		    (struct atabusiodetach_args *)addr;
		if ((chp->ch_drive[0].drive_type == ATA_DRIVET_OLD) ||
		    (chp->ch_drive[1].drive_type == ATA_DRIVET_OLD))
			return (EOPNOTSUPP);
		switch (a->at_dev) {
		case -1:
			min_drive = 0;
			max_drive = 1;
			break;
		case 0:
		case 1:
			min_drive = max_drive = a->at_dev;
			break;
		default:
			return (EINVAL);
		}
		for (drive = min_drive; drive <= max_drive; drive++) {
			if (chp->ch_drive[drive].drv_softc != NULL) {
				error = config_detach(
				    chp->ch_drive[drive].drv_softc, 0);
				if (error)
					return (error);
				KASSERT(chp->ch_drive[drive].drv_softc == NULL);
			}
		}
		return 0;
	}
	default:
		return ENOTTY;
	}
}

static bool
atabus_suspend(device_t dv, const pmf_qual_t *qual)
{
	struct atabus_softc *sc = device_private(dv);
	struct ata_channel *chp = sc->sc_chan;

	ata_queue_idle(chp->ch_queue);

	return true;
}

static bool
atabus_resume(device_t dv, const pmf_qual_t *qual)
{
	struct atabus_softc *sc = device_private(dv);
	struct ata_channel *chp = sc->sc_chan;
	int s;

	/*
	 * XXX joerg: with wdc, the first channel unfreezes the controler.
	 * Move this the reset and queue idling into wdc.
	 */
	s = splbio();
	if (chp->ch_queue->queue_freeze == 0) {
		splx(s);
		return true;
	}
	KASSERT(chp->ch_queue->queue_freeze > 0);
	/* unfreeze the queue and reset drives */
	chp->ch_queue->queue_freeze--;

	/* reset channel only if there are drives attached */
	if (chp->ch_ndrives > 0)
		ata_reset_channel(chp, AT_WAIT);
	splx(s);

	return true;
}

static int
atabus_rescan(device_t self, const char *ifattr, const int *locators)
{
	struct atabus_softc *sc = device_private(self);
	struct ata_channel *chp = sc->sc_chan;
	struct atabus_initq *initq;
	int i;

	/*
	 * we can rescan a port multiplier atabus, even if some devices are
	 * still attached 
	 */
	if (chp->ch_satapmp_nports == 0) {
		if (chp->atapibus != NULL) {
			return EBUSY;
		}

		KASSERT(chp->ch_ndrives == 0 || chp->ch_drive != NULL);
		for (i = 0; i < chp->ch_ndrives; i++) {
			if (chp->ch_drive[i].drv_softc != NULL) {
				return EBUSY;
			}
		}
	}

	initq = malloc(sizeof(*initq), M_DEVBUF, M_WAITOK);
	initq->atabus_sc = sc;
	TAILQ_INSERT_TAIL(&atabus_initq_head, initq, atabus_initq);
	config_pending_incr(sc->sc_dev);

	chp->ch_flags |= ATACH_TH_RESCAN;
	wakeup(&chp->ch_thread);

	return 0;
}

void
ata_delay(int ms, const char *msg, int flags)
{
	if ((flags & (AT_WAIT | AT_POLL)) == AT_POLL) {
		/*
		 * can't use tsleep(), we may be in interrupt context
		 * or taking a crash dump
		 */
		delay(ms * 1000);
	} else {
		kpause(msg, false, mstohz(ms), NULL);
	}
}

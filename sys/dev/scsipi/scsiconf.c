/*	$NetBSD: scsiconf.c,v 1.273 2014/07/25 08:10:38 dholland Exp $	*/

/*-
 * Copyright (c) 1998, 1999, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum; Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
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
 * Originally written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: scsiconf.c,v 1.273 2014/07/25 08:10:38 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/once.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/scsiio.h>
#include <sys/queue.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include "locators.h"

static const struct scsipi_periphsw scsi_probe_dev = {
	NULL,
	NULL,
	NULL,
	NULL,
};

struct scsi_initq {
	struct scsipi_channel *sc_channel;
	TAILQ_ENTRY(scsi_initq) scsi_initq;
};

static ONCE_DECL(scsi_conf_ctrl);
static TAILQ_HEAD(, scsi_initq)	scsi_initq_head;
static kmutex_t			scsibus_qlock;
static kcondvar_t		scsibus_qcv;

static int	scsi_probe_device(struct scsibus_softc *, int, int);

static int	scsibusmatch(device_t, cfdata_t, void *);
static void	scsibusattach(device_t, device_t, void *);
static int	scsibusdetach(device_t, int flags);
static int	scsibusrescan(device_t, const char *, const int *);
static void	scsidevdetached(device_t, device_t);

CFATTACH_DECL3_NEW(scsibus, sizeof(struct scsibus_softc),
    scsibusmatch, scsibusattach, scsibusdetach, NULL,
    scsibusrescan, scsidevdetached, DVF_DETACH_SHUTDOWN);

extern struct cfdriver scsibus_cd;

static dev_type_open(scsibusopen);
static dev_type_close(scsibusclose);
static dev_type_ioctl(scsibusioctl);

const struct cdevsw scsibus_cdevsw = {
	.d_open = scsibusopen,
	.d_close = scsibusclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = scsibusioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

static int	scsibusprint(void *, const char *);
static void	scsibus_config(struct scsipi_channel *, void *);

const struct scsipi_bustype scsi_bustype = {
	SCSIPI_BUSTYPE_BUSTYPE(SCSIPI_BUSTYPE_SCSI, SCSIPI_BUSTYPE_SCSI_PSCSI),
	scsi_scsipi_cmd,
	scsipi_interpret_sense,
	scsi_print_addr,
	scsi_kill_pending,
	scsi_async_event_xfer_mode,
};

const struct scsipi_bustype scsi_fc_bustype = {
	SCSIPI_BUSTYPE_BUSTYPE(SCSIPI_BUSTYPE_SCSI, SCSIPI_BUSTYPE_SCSI_FC),
	scsi_scsipi_cmd,
	scsipi_interpret_sense,
	scsi_print_addr,
	scsi_kill_pending,
	scsi_fc_sas_async_event_xfer_mode,
};

const struct scsipi_bustype scsi_sas_bustype = {
	SCSIPI_BUSTYPE_BUSTYPE(SCSIPI_BUSTYPE_SCSI, SCSIPI_BUSTYPE_SCSI_SAS),
	scsi_scsipi_cmd,
	scsipi_interpret_sense,
	scsi_print_addr,
	scsi_kill_pending,
	scsi_fc_sas_async_event_xfer_mode,
};

const struct scsipi_bustype scsi_usb_bustype = {
	SCSIPI_BUSTYPE_BUSTYPE(SCSIPI_BUSTYPE_SCSI, SCSIPI_BUSTYPE_SCSI_USB),
	scsi_scsipi_cmd,
	scsipi_interpret_sense,
	scsi_print_addr,
	scsi_kill_pending,
	NULL,
};

static int
scsibus_init(void)
{

	TAILQ_INIT(&scsi_initq_head);
	mutex_init(&scsibus_qlock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&scsibus_qcv, "scsinitq");
	return 0;
}

int
scsiprint(void *aux, const char *pnp)
{
	struct scsipi_channel *chan = aux;
	struct scsipi_adapter *adapt = chan->chan_adapter;

	/* only "scsibus"es can attach to "scsi"s; easy. */
	if (pnp)
		aprint_normal("scsibus at %s", pnp);

	/* don't print channel if the controller says there can be only one. */
	if (adapt->adapt_nchannels != 1)
		aprint_normal(" channel %d", chan->chan_channel);

	return (UNCONF);
}

static int
scsibusmatch(device_t parent, cfdata_t cf, void *aux)
{
	struct scsipi_channel *chan = aux;

	if (SCSIPI_BUSTYPE_TYPE(chan->chan_bustype->bustype_type) !=
	    SCSIPI_BUSTYPE_SCSI)
		return 0;

	if (cf->cf_loc[SCSICF_CHANNEL] != chan->chan_channel &&
	    cf->cf_loc[SCSICF_CHANNEL] != SCSICF_CHANNEL_DEFAULT)
		return (0);

	return (1);
}

static void
scsibusattach(device_t parent, device_t self, void *aux)
{
	struct scsibus_softc *sc = device_private(self);
	struct scsipi_channel *chan = aux;
	struct scsi_initq *scsi_initq;

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	sc->sc_dev = self;
	sc->sc_channel = chan;
	chan->chan_name = device_xname(sc->sc_dev);

	aprint_naive(": SCSI bus\n");
	aprint_normal(": %d target%s, %d lun%s per target\n",
	    chan->chan_ntargets,
	    chan->chan_ntargets == 1 ? "" : "s",
	    chan->chan_nluns,
	    chan->chan_nluns == 1 ? "" : "s");

	/*
	 * XXX 
	 * newer adapters support more than 256 outstanding commands
	 * per periph and don't use the tag (they eventually allocate one
	 * internally). Right now scsipi always allocate a tag and
	 * is limited to 256 tags, per scsi specs.
	 * this should be revisited
	 */
	if (chan->chan_flags & SCSIPI_CHAN_OPENINGS) {
		if (chan->chan_max_periph > 256)
			chan->chan_max_periph = 256;
	} else {
		if (chan->chan_adapter->adapt_max_periph > 256)
			chan->chan_adapter->adapt_max_periph = 256;
	}

	if (scsipi_adapter_addref(chan->chan_adapter))
		return;

	RUN_ONCE(&scsi_conf_ctrl, scsibus_init);

	/* Initialize the channel structure first */
	chan->chan_init_cb = scsibus_config;
	chan->chan_init_cb_arg = sc;

	scsi_initq = malloc(sizeof(struct scsi_initq), M_DEVBUF, M_WAITOK);
	scsi_initq->sc_channel = chan;
	TAILQ_INSERT_TAIL(&scsi_initq_head, scsi_initq, scsi_initq);
        config_pending_incr(sc->sc_dev);
	if (scsipi_channel_init(chan)) {
		aprint_error_dev(sc->sc_dev, "failed to init channel\n");
		return;
	}
}

static void
scsibus_config(struct scsipi_channel *chan, void *arg)
{
	struct scsibus_softc *sc = arg;
	struct scsi_initq *scsi_initq;

#ifndef SCSI_DELAY
#define SCSI_DELAY 2
#endif
	if ((chan->chan_flags & SCSIPI_CHAN_NOSETTLE) == 0 &&
	    SCSI_DELAY > 0) {
		aprint_normal_dev(sc->sc_dev,
		    "waiting %d seconds for devices to settle...\n",
		    SCSI_DELAY);
		/* ...an identifier we know no one will use... */
		(void) tsleep(scsibus_config, PRIBIO,
		    "scsidly", SCSI_DELAY * hz);
	}

	/* Make sure the devices probe in scsibus order to avoid jitter. */
	mutex_enter(&scsibus_qlock);
	for (;;) {
		scsi_initq = TAILQ_FIRST(&scsi_initq_head);
		if (scsi_initq->sc_channel == chan)
			break;
		cv_wait(&scsibus_qcv, &scsibus_qlock);
	}
	mutex_exit(&scsibus_qlock);

	scsi_probe_bus(sc, -1, -1);

	mutex_enter(&scsibus_qlock);
	TAILQ_REMOVE(&scsi_initq_head, scsi_initq, scsi_initq);
	cv_broadcast(&scsibus_qcv);
	mutex_exit(&scsibus_qlock);

	free(scsi_initq, M_DEVBUF);

	scsipi_adapter_delref(chan->chan_adapter);

	config_pending_decr(sc->sc_dev);
}

static int
scsibusdetach(device_t self, int flags)
{
	struct scsibus_softc *sc = device_private(self);
	struct scsipi_channel *chan = sc->sc_channel;
	struct scsipi_periph *periph;
	int ctarget, clun;
	struct scsipi_xfer *xs;
	int error;

	/* XXXSMP scsipi */
	KERNEL_LOCK(1, curlwp);

	/*
	 * Detach all of the periphs.
	 */
	if ((error = scsipi_target_detach(chan, -1, -1, flags)) != 0) {
		/* XXXSMP scsipi */
		KERNEL_UNLOCK_ONE(curlwp);

		return error;
	}

	pmf_device_deregister(self);

	/*
	 * Process outstanding commands (which will never complete as the
	 * controller is gone).
	 *
	 * XXX Surely this is redundant?  If we get this far, the
	 * XXX peripherals have all been detached.
	 */
	for (ctarget = 0; ctarget < chan->chan_ntargets; ctarget++) {
		if (ctarget == chan->chan_id)
			continue;
		for (clun = 0; clun < chan->chan_nluns; clun++) {
			periph = scsipi_lookup_periph(chan, ctarget, clun);
			if (periph == NULL)
				continue;
			TAILQ_FOREACH(xs, &periph->periph_xferq, device_q) {
				callout_stop(&xs->xs_callout);
				xs->error = XS_DRIVER_STUFFUP;
				scsipi_done(xs);
			}
		}
	}

	/*
	 * Now shut down the channel.
	 */
	scsipi_channel_shutdown(chan);

	/* XXXSMP scsipi */
	KERNEL_UNLOCK_ONE(curlwp);

	return 0;
}

/*
 * Probe the requested scsi bus. It must be already set up.
 * target and lun optionally narrow the search if not -1
 */
int
scsi_probe_bus(struct scsibus_softc *sc, int target, int lun)
{
	struct scsipi_channel *chan = sc->sc_channel;
	int maxtarget, mintarget, maxlun, minlun;
	int error;

	/* XXXSMP scsipi */
	KERNEL_LOCK(1, curlwp);

	if (target == -1) {
		maxtarget = chan->chan_ntargets - 1;
		mintarget = 0;
	} else {
		if (target < 0 || target >= chan->chan_ntargets)
			return (EINVAL);
		maxtarget = mintarget = target;
	}

	if (lun == -1) {
		maxlun = chan->chan_nluns - 1;
		minlun = 0;
	} else {
		if (lun < 0 || lun >= chan->chan_nluns)
			return (EINVAL);
		maxlun = minlun = lun;
	}

	/*
	 * Some HBAs provide an abstracted view of the bus; give them an
	 * oppertunity to re-scan it before we do.
	 */
	if (chan->chan_adapter->adapt_ioctl != NULL)
		(*chan->chan_adapter->adapt_ioctl)(chan, SCBUSIOLLSCAN, NULL,
		    0, curproc);

	if ((error = scsipi_adapter_addref(chan->chan_adapter)) != 0)
		goto ret;
	for (target = mintarget; target <= maxtarget; target++) {
		if (target == chan->chan_id)
			continue;
		for (lun = minlun; lun <= maxlun; lun++) {
			/*
			 * See if there's a device present, and configure it.
			 */
			if (scsi_probe_device(sc, target, lun) == 0)
				break;
			/* otherwise something says we should look further */
		}

		/*
		 * Now that we've discovered all of the LUNs on this
		 * I_T Nexus, update the xfer mode for all of them
		 * that we know about.
		 */
		scsipi_set_xfer_mode(chan, target, 1);
	}
	scsipi_adapter_delref(chan->chan_adapter);
ret:
	KERNEL_UNLOCK_ONE(curlwp);
	return (error);
}

static int
scsibusrescan(device_t sc, const char *ifattr,
    const int *locators)
{

	KASSERT(ifattr && !strcmp(ifattr, "scsibus"));
	KASSERT(locators);

	return (scsi_probe_bus(device_private(sc),
		locators[SCSIBUSCF_TARGET], locators[SCSIBUSCF_LUN]));
}

static void
scsidevdetached(device_t self, device_t child)
{
	struct scsibus_softc *sc = device_private(self);
	struct scsipi_channel *chan = sc->sc_channel;
	struct scsipi_periph *periph;
	int target, lun;

	target = device_locator(child, SCSIBUSCF_TARGET);
	lun = device_locator(child, SCSIBUSCF_LUN);

	/* XXXSMP scsipi */
	KERNEL_LOCK(1, curlwp);

	periph = scsipi_lookup_periph(chan, target, lun);
	KASSERT(periph->periph_dev == child);

	scsipi_remove_periph(chan, periph);
	free(periph, M_DEVBUF);

	/* XXXSMP scsipi */
	KERNEL_UNLOCK_ONE(curlwp);
}

/*
 * Print out autoconfiguration information for a subdevice.
 *
 * This is a slight abuse of 'standard' autoconfiguration semantics,
 * because 'print' functions don't normally print the colon and
 * device information.  However, in this case that's better than
 * either printing redundant information before the attach message,
 * or having the device driver call a special function to print out
 * the standard device information.
 */
static int
scsibusprint(void *aux, const char *pnp)
{
	struct scsipibus_attach_args *sa = aux;
	struct scsipi_inquiry_pattern *inqbuf;
	u_int8_t type;
	const char *dtype;
	char vendor[33], product[65], revision[17];
	int target, lun;

	if (pnp != NULL)
		aprint_normal("%s", pnp);

	inqbuf = &sa->sa_inqbuf;

	target = sa->sa_periph->periph_target;
	lun = sa->sa_periph->periph_lun;
	type = inqbuf->type & SID_TYPE;

	dtype = scsipi_dtype(type);

	scsipi_strvis(vendor, 33, inqbuf->vendor, 8);
	scsipi_strvis(product, 65, inqbuf->product, 16);
	scsipi_strvis(revision, 17, inqbuf->revision, 4);

	aprint_normal(" target %d lun %d: <%s, %s, %s> %s %s",
	    target, lun, vendor, product, revision, dtype,
	    inqbuf->removable ? "removable" : "fixed");

	return (UNCONF);
}

static const struct scsi_quirk_inquiry_pattern scsi_quirk_patterns[] = {
	{{T_DIRECT, T_REMOV,
	 "Apple   ", "iPod            ", ""},	  PQUIRK_START},
	{{T_CDROM, T_REMOV,
	 "CHINON  ", "CD-ROM CDS-431  ", ""},     PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "CHINON  ", "CD-ROM CDS-435  ", ""},     PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "Chinon  ", "CD-ROM CDS-525  ", ""},     PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "CHINON  ", "CD-ROM CDS-535  ", ""},     PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "DEC     ", "RRD42   (C) DEC ", ""},     PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "DENON   ", "DRD-25X         ", "V"},    PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "GENERIC ", "CRD-BP2         ", ""},     PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "HP      ", "C4324/C4325     ", ""},     PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "IMS     ", "CDD521/10       ", "2.06"}, PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "MATSHITA", "CD-ROM CR-5XX   ", "1.0b"}, PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "MEDAVIS ", "RENO CD-ROMX2A  ", ""},     PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "MEDIAVIS", "CDR-H93MV       ", "1.3"},  PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "NEC     ", "CD-ROM DRIVE:502", ""},     PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "NEC     ", "CD-ROM DRIVE:55 ", ""},     PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "NEC     ", "CD-ROM DRIVE:83 ", ""},     PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "NEC     ", "CD-ROM DRIVE:84 ", ""},     PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "NEC     ", "CD-ROM DRIVE:841", ""},     PQUIRK_NOLUNS},
        {{T_CDROM, T_REMOV,
	 "OLYMPUS ", "CDS620E         ", "1.1d"},
			       PQUIRK_NOLUNS|PQUIRK_NOSYNC|PQUIRK_NOCAPACITY},
	{{T_CDROM, T_REMOV,
	 "PIONEER ", "CD-ROM DR-124X  ", "1.01"}, PQUIRK_NOLUNS},
        {{T_CDROM, T_REMOV,
         "PLEXTOR ", "CD-ROM PX-4XCS  ", "1.01"},
                               PQUIRK_NOLUNS|PQUIRK_NOSYNC},
	{{T_CDROM, T_REMOV,
	 "SONY    ", "CD-ROM CDU-541  ", ""},     PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "SONY    ", "CD-ROM CDU-55S  ", ""},     PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "SONY    ", "CD-ROM CDU-561  ", ""},     PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "SONY    ", "CD-ROM CDU-76S", ""},
				PQUIRK_NOLUNS|PQUIRK_NOSYNC|PQUIRK_NOWIDE},
	{{T_CDROM, T_REMOV,
	 "SONY    ", "CD-ROM CDU-8003A", ""},     PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "SONY    ", "CD-ROM CDU-8012 ", ""},     PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "TEAC    ", "CD-ROM          ", "1.06"}, PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "TEAC    ", "CD-ROM CD-56S   ", "1.0B"}, PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "TEXEL   ", "CD-ROM          ", "1.06"}, PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "TEXEL   ", "CD-ROM DM-XX24 K", "1.09"}, PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "TEXEL   ", "CD-ROM DM-XX24 K", "1.10"}, PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "TOSHIBA ", "XM-4101TASUNSLCD", ""}, PQUIRK_NOLUNS|PQUIRK_NOSYNC},
	/* "IBM CDRM00201     !F" 0724 is an IBM OEM Toshiba XM-4101BME */
	{{T_CDROM, T_REMOV,
	 "IBM     ", "CDRM00201     !F", "0724"}, PQUIRK_NOLUNS|PQUIRK_NOSYNC},
	{{T_CDROM, T_REMOV,
	 "ShinaKen", "CD-ROM DM-3x1S",   "1.04"}, PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "JVC     ", "R2626",            ""},     PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "YAMAHA", "CRW8424S",           ""},     PQUIRK_NOLUNS},
	{{T_CDROM, T_REMOV,
	 "NEC     ", "CD-ROM DRIVE:222", ""},	  PQUIRK_NOLUNS|PQUIRK_NOSYNC},

	{{T_DIRECT, T_FIXED,
	 "MICROP  ", "1588-15MBSUN0669", ""},     PQUIRK_AUTOSAVE},
	{{T_DIRECT, T_FIXED,
	 "MICROP  ", "2217-15MQ1091501", ""},     PQUIRK_NOSYNCCACHE},
	{{T_OPTICAL, T_REMOV,
	 "EPSON   ", "OMD-5010        ", "3.08"}, PQUIRK_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "ADAPTEC ", "AEC-4412BD",       "1.2A"}, PQUIRK_NOMODESENSE},
	{{T_DIRECT, T_FIXED,
	 "ADAPTEC ", "ACB-4000",         ""},     PQUIRK_FORCELUNS|PQUIRK_AUTOSAVE|PQUIRK_NOMODESENSE},
	{{T_DIRECT, T_FIXED,
	 "DEC     ", "RZ55     (C) DEC", ""},     PQUIRK_AUTOSAVE},
	{{T_DIRECT, T_FIXED,
	 "EMULEX  ", "MD21/S2     ESDI", "A00"},
				PQUIRK_FORCELUNS|PQUIRK_AUTOSAVE},
	{{T_DIRECT, T_FIXED,
	 "MICROP",  "1548-15MZ1077801",  "HZ2P"}, PQUIRK_NOTAG},
	{{T_DIRECT, T_FIXED,
	 "HP      ", "C372",             ""},     PQUIRK_NOTAG},
	{{T_DIRECT, T_FIXED,
	 "IBMRAID ", "0662S",		 ""},     PQUIRK_AUTOSAVE},
	{{T_DIRECT, T_FIXED,
	 "IBM     ", "0663H",		 ""},     PQUIRK_AUTOSAVE},
	{{T_DIRECT, T_FIXED,
	 "IBM",	     "0664",		 ""},     PQUIRK_AUTOSAVE},
	{{T_DIRECT, T_FIXED,
	/* improperly report DT-only sync mode */
	 "IBM     ", "DXHS36D",		 ""},
				PQUIRK_CAP_SYNC|PQUIRK_CAP_WIDE16},
	{{T_DIRECT, T_FIXED,
	 "IBM     ", "DXHS18Y",		 ""},
				PQUIRK_CAP_SYNC|PQUIRK_CAP_WIDE16},
	{{T_DIRECT, T_FIXED,
	 "IBM     ", "H3171-S2",	 ""},
				PQUIRK_NOLUNS|PQUIRK_AUTOSAVE},
	{{T_DIRECT, T_FIXED,
	 "IBM     ", "KZ-C",		 ""},	  PQUIRK_AUTOSAVE},
	/* Broken IBM disk */
	{{T_DIRECT, T_FIXED,
	 ""	   , "DFRSS2F",		 ""},	  PQUIRK_AUTOSAVE},
	{{T_DIRECT, T_FIXED,
	 "Initio  ", "",		 ""},	  PQUIRK_NOBIGMODESENSE},
	{{T_DIRECT, T_REMOV,
	 "MPL     ", "MC-DISK-        ", ""},     PQUIRK_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "MAXTOR  ", "XT-3280         ", ""},     PQUIRK_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "MAXTOR  ", "XT-4380S        ", ""},     PQUIRK_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "MAXTOR  ", "MXT-1240S       ", ""},     PQUIRK_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "MAXTOR  ", "XT-4170S        ", ""},     PQUIRK_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "MAXTOR  ", "XT-8760S",         ""},     PQUIRK_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "MAXTOR  ", "LXT-213S        ", ""},     PQUIRK_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "MAXTOR  ", "LXT-213S SUN0207", ""},     PQUIRK_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "MAXTOR  ", "LXT-200S        ", ""},     PQUIRK_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "MEGADRV ", "EV1000",           ""},     PQUIRK_NOMODESENSE},
	{{T_DIRECT, T_FIXED,
	 "MICROP", "1991-27MZ",          ""},     PQUIRK_NOTAG},
	{{T_DIRECT, T_FIXED,
	 "MST     ", "SnapLink        ", ""},     PQUIRK_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "NEC     ", "D3847           ", "0307"}, PQUIRK_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "QUANTUM ", "ELS85S          ", ""},     PQUIRK_AUTOSAVE},
	{{T_DIRECT, T_FIXED,
	 "QUANTUM ", "LPS525S         ", ""},     PQUIRK_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "QUANTUM ", "P105S 910-10-94x", ""},     PQUIRK_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "QUANTUM ", "PD1225S         ", ""},     PQUIRK_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "QUANTUM ", "PD210S   SUN0207", ""},     PQUIRK_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "QUANTUM ", "ATLAS IV 9 WLS", "0A0A"},   PQUIRK_CAP_NODT},
	{{T_DIRECT, T_FIXED,
	 "RODIME  ", "RO3000S         ", ""},     PQUIRK_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "SEAGATE ", "ST125N          ", ""},     PQUIRK_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "SEAGATE ", "ST157N          ", ""},     PQUIRK_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "SEAGATE ", "ST296           ", ""},     PQUIRK_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "SEAGATE ", "ST296N          ", ""},     PQUIRK_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "SEAGATE ", "ST318404LC      ", ""},     PQUIRK_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "SEAGATE ", "ST15150N        ", ""},     PQUIRK_NOTAG},
	{{T_DIRECT, T_FIXED,
	 "SEAGATE ", "ST19171",          ""},     PQUIRK_NOMODESENSE},
	{{T_DIRECT, T_FIXED,
	 "SEAGATE ", "ST32430N",         ""},     PQUIRK_CAP_SYNC},
	{{T_DIRECT, T_FIXED,
	 "SEAGATE ", "ST34501FC       ", ""},     PQUIRK_NOMODESENSE},
	{{T_DIRECT, T_FIXED,
	 "SEAGATE ", "SX910800N",        ""},     PQUIRK_NOTAG},
	{{T_DIRECT, T_FIXED,
	 "TOSHIBA ", "MK538FB         ", "6027"}, PQUIRK_NOLUNS},
	{{T_DIRECT, T_FIXED,
	 "MICROP  ", "1924",          ""},     PQUIRK_CAP_SYNC},
	{{T_DIRECT, T_FIXED,
	 "FUJITSU ", "M2266",         ""},     PQUIRK_CAP_SYNC},
	{{T_DIRECT, T_FIXED,
	 "FUJITSU ", "M2624S-512      ", ""},     PQUIRK_CAP_SYNC},
	{{T_DIRECT, T_FIXED,
	 "SEAGATE ", "SX336704LC"   , ""}, PQUIRK_CAP_SYNC | PQUIRK_CAP_WIDE16},
	{{T_DIRECT, T_FIXED,
	 "SEAGATE ", "SX173404LC",       ""},     PQUIRK_CAP_SYNC | PQUIRK_CAP_WIDE16},

	{{T_DIRECT, T_REMOV,
	 "IOMEGA", "ZIP 100",		 "J.03"}, PQUIRK_NOLUNS|PQUIRK_NOSYNC},
	{{T_DIRECT, T_REMOV,
	 "INSITE", "I325VM",             ""},     PQUIRK_NOLUNS},

	/* XXX: QIC-36 tape behind Emulex adapter.  Very broken. */
	{{T_SEQUENTIAL, T_REMOV,
	 "        ", "                ", "    "}, PQUIRK_NOLUNS},
	{{T_SEQUENTIAL, T_REMOV,
	 "EMULEX  ", "MT-02 QIC       ", ""},     PQUIRK_NOLUNS},
	{{T_SEQUENTIAL, T_REMOV,
	 "CALIPER ", "CP150           ", ""},     PQUIRK_NOLUNS},
	{{T_SEQUENTIAL, T_REMOV,
	 "EXABYTE ", "EXB-8200        ", ""},     PQUIRK_NOLUNS},
	{{T_SEQUENTIAL, T_REMOV,
	 "SONY    ", "GY-10C          ", ""},     PQUIRK_NOLUNS},
	{{T_SEQUENTIAL, T_REMOV,
	 "SONY    ", "SDT-2000        ", "2.09"}, PQUIRK_NOLUNS},
	{{T_SEQUENTIAL, T_REMOV,
	 "SONY    ", "SDT-5000        ", "3."},   PQUIRK_NOSYNC|PQUIRK_NOWIDE},
	{{T_SEQUENTIAL, T_REMOV,
	 "SONY    ", "SDT-5200        ", "3."},   PQUIRK_NOLUNS},
	{{T_SEQUENTIAL, T_REMOV,
	 "TANDBERG", " TDC 3600       ", ""},     PQUIRK_NOLUNS},
	/* Following entry reported as a Tandberg 3600; ref. PR1933 */
	{{T_SEQUENTIAL, T_REMOV,
	 "ARCHIVE ", "VIPER 150  21247", ""},     PQUIRK_NOLUNS},
	/* Following entry for a Cipher ST150S; ref. PR4171 */
	{{T_SEQUENTIAL, T_REMOV,
	 "ARCHIVE ", "VIPER 1500 21247", "2.2G"}, PQUIRK_NOLUNS},
	{{T_SEQUENTIAL, T_REMOV,
	 "ARCHIVE ", "Python 28454-XXX", ""},     PQUIRK_NOLUNS},
	{{T_SEQUENTIAL, T_REMOV,
	 "WANGTEK ", "5099ES SCSI",      ""},     PQUIRK_NOLUNS},
	{{T_SEQUENTIAL, T_REMOV,
	 "WANGTEK ", "5150ES SCSI",      ""},     PQUIRK_NOLUNS},
	{{T_SEQUENTIAL, T_REMOV,
	 "WANGTEK ", "SCSI-36",		 ""},     PQUIRK_NOLUNS},
	{{T_SEQUENTIAL, T_REMOV,
	 "WangDAT ", "Model 1300      ", "02.4"}, PQUIRK_NOSYNC|PQUIRK_NOWIDE},
	{{T_SEQUENTIAL, T_REMOV,
	 "WangDAT ", "Model 2600      ", "01.7"}, PQUIRK_NOSYNC|PQUIRK_NOWIDE},
	{{T_SEQUENTIAL, T_REMOV,
	 "WangDAT ", "Model 3200      ", "02.2"}, PQUIRK_NOSYNC|PQUIRK_NOWIDE},
	{{T_SEQUENTIAL, T_REMOV,
	 "TEAC    ", "MT-2ST/N50      ", ""},     PQUIRK_NOLUNS},

	{{T_SCANNER, T_FIXED,
	 "RICOH   ", "IS60            ", "1R08"}, PQUIRK_NOLUNS},
	{{T_SCANNER, T_FIXED,
	 "UMAX    ", "Astra 1200S     ", "V2.9"}, PQUIRK_NOLUNS},
	{{T_SCANNER, T_FIXED,
	 "UMAX    ", "Astra 1220S     ", ""},     PQUIRK_NOLUNS},
	{{T_SCANNER, T_FIXED,
	 "UMAX    ", "UMAX S-6E       ", "V2.0"}, PQUIRK_NOLUNS},
	{{T_SCANNER, T_FIXED,
	 "UMAX    ", "UMAX S-12       ", "V2.1"}, PQUIRK_NOLUNS},
	{{T_SCANNER, T_FIXED,
	 "ULTIMA  ", "A6000C          ", ""},     PQUIRK_NOLUNS},
	{{T_PROCESSOR, T_FIXED,
	 "ESG-SHV",  "SCA HSBP M15",     ""},     PQUIRK_NOLUNS},
	{{T_PROCESSOR, T_FIXED,
	 "SYMBIOS",  "",                 ""},     PQUIRK_NOLUNS},
	{{T_PROCESSOR, T_FIXED,
	 "LITRONIC", "PCMCIA          ", ""},     PQUIRK_NOLUNS},
	{{T_CHANGER, T_REMOV,
	 "SONY    ", "CDL1100         ", ""},     PQUIRK_NOLUNS},
	{{T_ENCLOSURE, T_FIXED,
	 "SUN     ", "SENA            ", ""},     PQUIRK_NOLUNS},
};

/*
 * given a target and lun, ask the device what
 * it is, and find the correct driver table
 * entry.
 */
static int
scsi_probe_device(struct scsibus_softc *sc, int target, int lun)
{
	struct scsipi_channel *chan = sc->sc_channel;
	struct scsipi_periph *periph;
	struct scsipi_inquiry_data inqbuf;
	const struct scsi_quirk_inquiry_pattern *finger;
	int checkdtype, priority, docontinue, quirks;
	struct scsipibus_attach_args sa;
	cfdata_t cf;
	int locs[SCSIBUSCF_NLOCS];

	/*
	 * Assume no more luns to search after this one.
	 * If we successfully get Inquiry data and after
	 * merging quirks we find we can probe for more
	 * luns, we will.
	 */
	docontinue = 0;

	/* Skip this slot if it is already attached. */
	if (scsipi_lookup_periph(chan, target, lun) != NULL)
		return (docontinue);

	periph = scsipi_alloc_periph(M_NOWAIT);
	if (periph == NULL) {
#ifdef	DIAGNOSTIC
		aprint_error_dev(sc->sc_dev,
		    "cannot allocate periph for target %d lun %d\n",
		    target, lun);
#endif
		return (ENOMEM);
	}
	periph->periph_channel = chan;
	periph->periph_switch = &scsi_probe_dev;

	periph->periph_target = target;
	periph->periph_lun = lun;
	periph->periph_quirks = chan->chan_defquirks;

#ifdef SCSIPI_DEBUG
	if (SCSIPI_DEBUG_TYPE == SCSIPI_BUSTYPE_SCSI &&
	    SCSIPI_DEBUG_TARGET == target &&
	    SCSIPI_DEBUG_LUN == lun)
		periph->periph_dbflags |= SCSIPI_DEBUG_FLAGS;
#endif

	/*
	 * Ask the device what it is
	 */

#ifdef SCSI_2_DEF
	/* some devices need to be told to go to SCSI2 */
	/* However some just explode if you tell them this.. leave it out */
	scsi_change_def(periph, XS_CTL_DISCOVERY | XS_CTL_SILENT);
#endif /* SCSI_2_DEF */

	/* Now go ask the device all about itself. */
	memset(&inqbuf, 0, sizeof(inqbuf));
	{
		u_int8_t *extension = &inqbuf.flags1;
		int len = 0;
		while (len < 3)
			extension[len++] = '\0';
		while (len < 3 + 28)
			extension[len++] = ' ';
		while (len < 3 + 28 + 20)
			extension[len++] = '\0';
		while (len < 3 + 28 + 20 + 1)
			extension[len++] = '\0';
		while (len < 3 + 28 + 20 + 1 + 1)
			extension[len++] = '\0';
		while (len < 3 + 28 + 20 + 1 + 1 + (8*2))
			extension[len++] = ' ';
	}
	if (scsipi_inquire(periph, &inqbuf, XS_CTL_DISCOVERY | XS_CTL_SILENT))
		goto bad;

	periph->periph_type = inqbuf.device & SID_TYPE;
	if (inqbuf.dev_qual2 & SID_REMOVABLE)
		periph->periph_flags |= PERIPH_REMOVABLE;
	periph->periph_version = inqbuf.version & SID_ANSII;

	/*
	 * Any device qualifier that has the top bit set (qualifier&4 != 0)
	 * is vendor specific and won't match in this switch.
	 * All we do here is throw out bad/negative responses.
	 */
	checkdtype = 0;
	switch (inqbuf.device & SID_QUAL) {
	case SID_QUAL_LU_PRESENT:
		checkdtype = 1;
		break;

	case SID_QUAL_LU_NOTPRESENT:
	case SID_QUAL_reserved:
	case SID_QUAL_LU_NOT_SUPP:
		goto bad;

	default:
		break;
	}

	/* Let the adapter driver handle the device separatley if it wants. */
	if (chan->chan_adapter->adapt_accesschk != NULL &&
	    (*chan->chan_adapter->adapt_accesschk)(periph, &sa.sa_inqbuf))
		goto bad;

	if (checkdtype) {
		switch (periph->periph_type) {
		case T_DIRECT:
		case T_SEQUENTIAL:
		case T_PRINTER:
		case T_PROCESSOR:
		case T_WORM:
		case T_CDROM:
		case T_SCANNER:
		case T_OPTICAL:
		case T_CHANGER:
		case T_COMM:
		case T_IT8_1:
		case T_IT8_2:
		case T_STORARRAY:
		case T_ENCLOSURE:
		case T_SIMPLE_DIRECT:
		case T_OPTIC_CARD_RW:
		case T_OBJECT_STORED:
		default:
			break;
		case T_NODEVICE:
			goto bad;
		}
	}

	sa.sa_periph = periph;
	sa.sa_inqbuf.type = inqbuf.device;
	sa.sa_inqbuf.removable = inqbuf.dev_qual2 & SID_REMOVABLE ?
	    T_REMOV : T_FIXED;
	sa.sa_inqbuf.vendor = inqbuf.vendor;
	sa.sa_inqbuf.product = inqbuf.product;
	sa.sa_inqbuf.revision = inqbuf.revision;
	sa.scsipi_info.scsi_version = inqbuf.version;
	sa.sa_inqptr = &inqbuf;

	finger = scsipi_inqmatch(
	    &sa.sa_inqbuf, scsi_quirk_patterns,
	    sizeof(scsi_quirk_patterns)/sizeof(scsi_quirk_patterns[0]),
	    sizeof(scsi_quirk_patterns[0]), &priority);

	if (finger != NULL)
		quirks = finger->quirks;
	else
		quirks = 0;

	/*
	 * Determine the operating mode capabilities of the device.
	 */
	if (periph->periph_version >= 2) {
		if ((inqbuf.flags3 & SID_CmdQue) != 0 &&
		    (quirks & PQUIRK_NOTAG) == 0)
			periph->periph_cap |= PERIPH_CAP_TQING;
		if ((inqbuf.flags3 & SID_Linked) != 0)
			periph->periph_cap |= PERIPH_CAP_LINKCMDS;
		if ((inqbuf.flags3 & SID_Sync) != 0 &&
		    (quirks & PQUIRK_NOSYNC) == 0)
			periph->periph_cap |= PERIPH_CAP_SYNC;
		if ((inqbuf.flags3 & SID_WBus16) != 0 &&
		    (quirks & PQUIRK_NOWIDE) == 0)
			periph->periph_cap |= PERIPH_CAP_WIDE16;
		if ((inqbuf.flags3 & SID_WBus32) != 0 &&
		    (quirks & PQUIRK_NOWIDE) == 0)
			periph->periph_cap |= PERIPH_CAP_WIDE32;
		if ((inqbuf.flags3 & SID_SftRe) != 0)
			periph->periph_cap |= PERIPH_CAP_SFTRESET;
		if ((inqbuf.flags3 & SID_RelAdr) != 0)
			periph->periph_cap |= PERIPH_CAP_RELADR;
		/* SPC-2 */
		if (periph->periph_version >= 3 &&
		    !(quirks & PQUIRK_CAP_NODT)){
			/*
			 * Report ST clocking though CAP_WIDExx/CAP_SYNC.
			 * If the device only supports DT, clear these
			 * flags (DT implies SYNC and WIDE)
			 */
			switch (inqbuf.flags4 & SID_Clocking) {
			case SID_CLOCKING_DT_ONLY:
				periph->periph_cap &=
				    ~(PERIPH_CAP_SYNC |
				      PERIPH_CAP_WIDE16 |
				      PERIPH_CAP_WIDE32);
				/* FALLTHROUGH */
			case SID_CLOCKING_SD_DT:
				periph->periph_cap |= PERIPH_CAP_DT;
				break;
			default: /* ST only or invalid */
				/* nothing to do */
				break;
			}
		}
		if (periph->periph_version >= 3) {
			if (inqbuf.flags4 & SID_IUS)
				periph->periph_cap |= PERIPH_CAP_IUS;
			if (inqbuf.flags4 & SID_QAS)
				periph->periph_cap |= PERIPH_CAP_QAS;
		}
	}
	if (quirks & PQUIRK_CAP_SYNC)
		periph->periph_cap |= PERIPH_CAP_SYNC;
	if (quirks & PQUIRK_CAP_WIDE16)
		periph->periph_cap |= PERIPH_CAP_WIDE16;

	/*
	 * Now apply any quirks from the table.
	 */
	periph->periph_quirks |= quirks;
	if (periph->periph_version == 0 &&
	    (periph->periph_quirks & PQUIRK_FORCELUNS) == 0)
		periph->periph_quirks |= PQUIRK_NOLUNS;

	if ((periph->periph_quirks & PQUIRK_NOLUNS) == 0)
		docontinue = 1;

	locs[SCSIBUSCF_TARGET] = target;
	locs[SCSIBUSCF_LUN] = lun;

	if ((cf = config_search_loc(config_stdsubmatch, sc->sc_dev,
	     "scsibus", locs, &sa)) != NULL) {
		scsipi_insert_periph(chan, periph);
		/*
		 * XXX Can't assign periph_dev here, because we'll
		 * XXX need it before config_attach() returns.  Must
		 * XXX assign it in periph driver.
		 */
		config_attach_loc(sc->sc_dev, cf, locs, &sa,
					 scsibusprint);
	} else {
		scsibusprint(&sa, device_xname(sc->sc_dev));
		aprint_normal(" not configured\n");
		goto bad;
	}

	return (docontinue);

bad:
	free(periph, M_DEVBUF);
	return (docontinue);
}

/****** Entry points for user control of the SCSI bus. ******/

static int
scsibusopen(dev_t dev, int flag, int fmt,
    struct lwp *l)
{
	struct scsibus_softc *sc;
	int error, unit = minor(dev);

	sc = device_lookup_private(&scsibus_cd, unit);
	if (sc == NULL)
		return (ENXIO);

	if (sc->sc_flags & SCSIBUSF_OPEN)
		return (EBUSY);

	if ((error = scsipi_adapter_addref(sc->sc_channel->chan_adapter)) != 0)
		return (error);

	sc->sc_flags |= SCSIBUSF_OPEN;

	return (0);
}

static int
scsibusclose(dev_t dev, int flag, int fmt,
    struct lwp *l)
{
	struct scsibus_softc *sc;

	sc = device_lookup_private(&scsibus_cd, minor(dev));
	scsipi_adapter_delref(sc->sc_channel->chan_adapter);

	sc->sc_flags &= ~SCSIBUSF_OPEN;

	return (0);
}

static int
scsibusioctl(dev_t dev, u_long cmd, void *addr, int flag, struct lwp *l)
{
	struct scsibus_softc *sc;
	struct scsipi_channel *chan;
	int error;

	sc = device_lookup_private(&scsibus_cd, minor(dev));
	chan = sc->sc_channel;

	/*
	 * Enforce write permission for ioctls that change the
	 * state of the bus.  Host adapter specific ioctls must
	 * be checked by the adapter driver.
	 */
	switch (cmd) {
	case SCBUSIOSCAN:
	case SCBUSIODETACH:
	case SCBUSIORESET:
		if ((flag & FWRITE) == 0)
			return (EBADF);
	}

	switch (cmd) {
	case SCBUSIOSCAN:
	    {
		struct scbusioscan_args *a =
		    (struct scbusioscan_args *)addr;

		error = scsi_probe_bus(sc, a->sa_target, a->sa_lun);
		break;
	    }

	case SCBUSIODETACH:
	    {
		struct scbusiodetach_args *a =
		    (struct scbusiodetach_args *)addr;

		error = scsipi_target_detach(chan, a->sa_target, a->sa_lun, 0);
		break;
	    }


	case SCBUSIORESET:
		/* FALLTHROUGH */
	default:
		if (chan->chan_adapter->adapt_ioctl == NULL)
			error = ENOTTY;
		else
			error = (*chan->chan_adapter->adapt_ioctl)(chan,
			    cmd, addr, flag, l->l_proc);
		break;
	}

	return (error);
}

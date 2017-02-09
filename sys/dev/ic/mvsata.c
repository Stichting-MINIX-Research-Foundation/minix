/*	$NetBSD: mvsata.c,v 1.34 2015/01/02 19:42:07 christos Exp $	*/
/*
 * Copyright (c) 2008 KIYOHARA Takashi
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mvsata.c,v 1.34 2015/01/02 19:42:07 christos Exp $");

#include "opt_mvsata.h"

/* ATAPI implementation not finished. */
//#include "atapibus.h"

#include <sys/param.h>
#if NATAPIBUS > 0
#include <sys/buf.h>
#endif
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <machine/vmparam.h>

#include <dev/ata/atareg.h>
#include <dev/ata/atavar.h>
#include <dev/ic/wdcvar.h>
#include <dev/ata/satapmpreg.h>
#include <dev/ata/satareg.h>
#include <dev/ata/satavar.h>

#if NATAPIBUS > 0
#include <dev/scsipi/scsi_all.h>	/* for SCSI status */
#endif

#include <dev/pci/pcidevs.h>

#include <dev/ic/mvsatareg.h>
#include <dev/ic/mvsatavar.h>


#define MVSATA_DEV(sc)		((sc)->sc_wdcdev.sc_atac.atac_dev)
#define MVSATA_DEV2(mvport)	((mvport)->port_ata_channel.ch_atac->atac_dev)

#define MVSATA_HC_READ_4(hc, reg) \
	bus_space_read_4((hc)->hc_iot, (hc)->hc_ioh, (reg))
#define MVSATA_HC_WRITE_4(hc, reg, val) \
	bus_space_write_4((hc)->hc_iot, (hc)->hc_ioh, (reg), (val))
#define MVSATA_EDMA_READ_4(mvport, reg) \
	bus_space_read_4((mvport)->port_iot, (mvport)->port_ioh, (reg))
#define MVSATA_EDMA_WRITE_4(mvport, reg, val) \
	bus_space_write_4((mvport)->port_iot, (mvport)->port_ioh, (reg), (val))
#define MVSATA_WDC_READ_2(mvport, reg) \
	bus_space_read_2((mvport)->port_iot, (mvport)->port_ioh, \
	SHADOW_REG_BLOCK_OFFSET + (reg))
#define MVSATA_WDC_READ_1(mvport, reg) \
	bus_space_read_1((mvport)->port_iot, (mvport)->port_ioh, \
	SHADOW_REG_BLOCK_OFFSET + (reg))
#define MVSATA_WDC_WRITE_2(mvport, reg, val) \
	bus_space_write_2((mvport)->port_iot, (mvport)->port_ioh, \
	SHADOW_REG_BLOCK_OFFSET + (reg), (val))
#define MVSATA_WDC_WRITE_1(mvport, reg, val) \
	bus_space_write_1((mvport)->port_iot, (mvport)->port_ioh, \
	SHADOW_REG_BLOCK_OFFSET + (reg), (val))

#ifdef MVSATA_DEBUG
#define DPRINTF(x)	if (mvsata_debug) printf x
#define	DPRINTFN(n,x)	if (mvsata_debug >= (n)) printf x
int	mvsata_debug = 2;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define ATA_DELAY		10000	/* 10s for a drive I/O */
#define ATAPI_DELAY		10	/* 10 ms, this is used only before
					   sending a cmd */
#define ATAPI_MODE_DELAY	1000	/* 1s, timeout for SET_FEATURE cmds */

#define MVSATA_EPRD_MAX_SIZE	(sizeof(struct eprd) * (MAXPHYS / PAGE_SIZE))


static void mvsata_probe_drive(struct ata_channel *);
#ifndef MVSATA_WITHOUTDMA
static int mvsata_bio(struct ata_drive_datas *, struct ata_bio *);
static void mvsata_reset_drive(struct ata_drive_datas *, int, uint32_t *);
static void mvsata_reset_channel(struct ata_channel *, int);
static int mvsata_exec_command(struct ata_drive_datas *, struct ata_command *);
static int mvsata_addref(struct ata_drive_datas *);
static void mvsata_delref(struct ata_drive_datas *);
static void mvsata_killpending(struct ata_drive_datas *);

#if NATAPIBUS > 0
static void mvsata_atapibus_attach(struct atabus_softc *);
static void mvsata_atapi_scsipi_request(struct scsipi_channel *,
					scsipi_adapter_req_t, void *);
static void mvsata_atapi_minphys(struct buf *);
static void mvsata_atapi_probe_device(struct atapibus_softc *, int);
static void mvsata_atapi_kill_pending(struct scsipi_periph *);
#endif
#endif

static void mvsata_setup_channel(struct ata_channel *);

#ifndef MVSATA_WITHOUTDMA
static void mvsata_bio_start(struct ata_channel *, struct ata_xfer *);
static int mvsata_bio_intr(struct ata_channel *, struct ata_xfer *, int);
static void mvsata_bio_kill_xfer(struct ata_channel *, struct ata_xfer *, int);
static void mvsata_bio_done(struct ata_channel *, struct ata_xfer *);
static int mvsata_bio_ready(struct mvsata_port *, struct ata_bio *, int,
			    int);
static void mvsata_wdc_cmd_start(struct ata_channel *, struct ata_xfer *);
static int mvsata_wdc_cmd_intr(struct ata_channel *, struct ata_xfer *, int);
static void mvsata_wdc_cmd_kill_xfer(struct ata_channel *, struct ata_xfer *,
				     int);
static void mvsata_wdc_cmd_done(struct ata_channel *, struct ata_xfer *);
static void mvsata_wdc_cmd_done_end(struct ata_channel *, struct ata_xfer *);
#if NATAPIBUS > 0
static void mvsata_atapi_start(struct ata_channel *, struct ata_xfer *);
static int mvsata_atapi_intr(struct ata_channel *, struct ata_xfer *, int);
static void mvsata_atapi_kill_xfer(struct ata_channel *, struct ata_xfer *,
				   int);
static void mvsata_atapi_reset(struct ata_channel *, struct ata_xfer *);
static void mvsata_atapi_phase_complete(struct ata_xfer *);
static void mvsata_atapi_done(struct ata_channel *, struct ata_xfer *);
static void mvsata_atapi_polldsc(void *);
#endif

static int mvsata_edma_enqueue(struct mvsata_port *, struct ata_bio *, void *);
static int mvsata_edma_handle(struct mvsata_port *, struct ata_xfer *);
static int mvsata_edma_wait(struct mvsata_port *, struct ata_xfer *, int);
static void mvsata_edma_timeout(void *);
static void mvsata_edma_rqq_remove(struct mvsata_port *, struct ata_xfer *);
#if NATAPIBUS > 0
static int mvsata_bdma_init(struct mvsata_port *, struct scsipi_xfer *, void *);
static void mvsata_bdma_start(struct mvsata_port *);
#endif
#endif

static int mvsata_port_init(struct mvsata_hc *, int);
static int mvsata_wdc_reg_init(struct mvsata_port *, struct wdc_regs *);
#ifndef MVSATA_WITHOUTDMA
static inline void mvsata_quetag_init(struct mvsata_port *);
static inline int mvsata_quetag_get(struct mvsata_port *);
static inline void mvsata_quetag_put(struct mvsata_port *, int);
static void *mvsata_edma_resource_prepare(struct mvsata_port *, bus_dma_tag_t,
					  bus_dmamap_t *, size_t, int);
static void mvsata_edma_resource_purge(struct mvsata_port *, bus_dma_tag_t,
				       bus_dmamap_t, void *);
static int mvsata_dma_bufload(struct mvsata_port *, int, void *, size_t, int);
static inline void mvsata_dma_bufunload(struct mvsata_port *, int, int);
#endif

static void mvsata_hreset_port(struct mvsata_port *);
static void mvsata_reset_port(struct mvsata_port *);
static void mvsata_reset_hc(struct mvsata_hc *);
static uint32_t mvsata_softreset(struct mvsata_port *, int);
#ifndef MVSATA_WITHOUTDMA
static void mvsata_edma_reset_qptr(struct mvsata_port *);
static inline void mvsata_edma_enable(struct mvsata_port *);
static int mvsata_edma_disable(struct mvsata_port *, int, int);
static void mvsata_edma_config(struct mvsata_port *, int);

static void mvsata_edma_setup_crqb(struct mvsata_port *, int, int,
				   struct ata_bio  *);
#endif
static uint32_t mvsata_read_preamps_gen1(struct mvsata_port *);
static void mvsata_fix_phy_gen1(struct mvsata_port *);
static void mvsata_devconn_gen1(struct mvsata_port *);

static uint32_t mvsata_read_preamps_gen2(struct mvsata_port *);
static void mvsata_fix_phy_gen2(struct mvsata_port *);
#ifndef MVSATA_WITHOUTDMA
static void mvsata_edma_setup_crqb_gen2e(struct mvsata_port *, int, int,
					 struct ata_bio  *);

#ifdef MVSATA_DEBUG
static void mvsata_print_crqb(struct mvsata_port *, int);
static void mvsata_print_crpb(struct mvsata_port *, int);
static void mvsata_print_eprd(struct mvsata_port *, int);
#endif

struct ata_bustype mvsata_ata_bustype = {
	SCSIPI_BUSTYPE_ATA,
	mvsata_bio,
	mvsata_reset_drive,
	mvsata_reset_channel,
	mvsata_exec_command,
	ata_get_params,
	mvsata_addref,
	mvsata_delref,
	mvsata_killpending
};

#if NATAPIBUS > 0
static const struct scsipi_bustype mvsata_atapi_bustype = {
	SCSIPI_BUSTYPE_ATAPI,
	atapi_scsipi_cmd,
	atapi_interpret_sense,
	atapi_print_addr,
	mvsata_atapi_kill_pending,
	NULL,
};
#endif /* NATAPIBUS */
#endif

static void
mvsata_pmp_select(struct mvsata_port *mvport, int pmpport)
{
	uint32_t ifctl;

	KASSERT(pmpport < PMP_MAX_DRIVES);
#if defined(DIAGNOSTIC) || defined(MVSATA_DEBUG)
	if ((MVSATA_EDMA_READ_4(mvport, EDMA_CMD) & EDMA_CMD_EENEDMA) != 0) {
		panic("EDMA enabled");
	}
#endif

	ifctl = MVSATA_EDMA_READ_4(mvport, SATA_SATAICTL);
	ifctl &= ~0xf;
	ifctl |= pmpport;
	MVSATA_EDMA_WRITE_4(mvport, SATA_SATAICTL, ifctl);
}

int
mvsata_attach(struct mvsata_softc *sc, struct mvsata_product *product,
	      int (*mvsata_sreset)(struct mvsata_softc *),
	      int (*mvsata_misc_reset)(struct mvsata_softc *),
	      int read_pre_amps)
{
	struct mvsata_hc *mvhc;
	struct mvsata_port *mvport;
	uint32_t (*read_preamps)(struct mvsata_port *) = NULL;
	void (*_fix_phy)(struct mvsata_port *) = NULL;
#ifndef MVSATA_WITHOUTDMA
	void (*edma_setup_crqb)
	    (struct mvsata_port *, int, int, struct ata_bio *) = NULL;
#endif
	int hc, port, channel;

	aprint_normal_dev(MVSATA_DEV(sc), "Gen%s, %dhc, %dport/hc\n",
	    (product->generation == gen1) ? "I" :
	    ((product->generation == gen2) ? "II" : "IIe"),
	    product->hc, product->port);


	switch (product->generation) {
	case gen1:
		mvsata_sreset = NULL;
		read_pre_amps = 1;	/* MUST */
		read_preamps = mvsata_read_preamps_gen1;
		_fix_phy = mvsata_fix_phy_gen1;
#ifndef MVSATA_WITHOUTDMA
		edma_setup_crqb = mvsata_edma_setup_crqb;
#endif
		break;

	case gen2:
		read_preamps = mvsata_read_preamps_gen2;
		_fix_phy = mvsata_fix_phy_gen2;
#ifndef MVSATA_WITHOUTDMA
		edma_setup_crqb = mvsata_edma_setup_crqb;
#endif
		break;

	case gen2e:
		read_preamps = mvsata_read_preamps_gen2;
		_fix_phy = mvsata_fix_phy_gen2;
#ifndef MVSATA_WITHOUTDMA
		edma_setup_crqb = mvsata_edma_setup_crqb_gen2e;
#endif
		break;
	}

	sc->sc_gen = product->generation;
	sc->sc_hc = product->hc;
	sc->sc_port = product->port;
	sc->sc_flags = product->flags;

#ifdef MVSATA_WITHOUTDMA
	sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DATA16;
#else
	sc->sc_edma_setup_crqb = edma_setup_crqb;
	sc->sc_wdcdev.sc_atac.atac_cap |=
	    (ATAC_CAP_DATA16 | ATAC_CAP_DMA | ATAC_CAP_UDMA);
#endif
	sc->sc_wdcdev.sc_atac.atac_pio_cap = 4;
#ifdef MVSATA_WITHOUTDMA
	sc->sc_wdcdev.sc_atac.atac_dma_cap = 0;
	sc->sc_wdcdev.sc_atac.atac_udma_cap = 0;
#else
	sc->sc_wdcdev.sc_atac.atac_dma_cap = 2;
	sc->sc_wdcdev.sc_atac.atac_udma_cap = 6;
#endif
	sc->sc_wdcdev.sc_atac.atac_channels = sc->sc_ata_channels;
	sc->sc_wdcdev.sc_atac.atac_nchannels = sc->sc_hc * sc->sc_port;
#ifndef MVSATA_WITHOUTDMA
	sc->sc_wdcdev.sc_atac.atac_bustype_ata = &mvsata_ata_bustype;
#if NATAPIBUS > 0
	sc->sc_wdcdev.sc_atac.atac_atapibus_attach = mvsata_atapibus_attach;
#endif
#endif
	sc->sc_wdcdev.wdc_maxdrives = 1;	/* SATA is always 1 drive */
	sc->sc_wdcdev.sc_atac.atac_probe = mvsata_probe_drive;
	sc->sc_wdcdev.sc_atac.atac_set_modes = mvsata_setup_channel;

	sc->sc_wdc_regs =
	    malloc(sizeof(struct wdc_regs) * product->hc * product->port,
	    M_DEVBUF, M_NOWAIT);
	if (sc->sc_wdc_regs == NULL) {
		aprint_error_dev(MVSATA_DEV(sc),
		    "can't allocate wdc regs memory\n");
		return ENOMEM;
	}
	sc->sc_wdcdev.regs = sc->sc_wdc_regs;

	for (hc = 0; hc < sc->sc_hc; hc++) {
		mvhc = &sc->sc_hcs[hc];
		mvhc->hc = hc;
		mvhc->hc_sc = sc;
		mvhc->hc_iot = sc->sc_iot;
		if (bus_space_subregion(sc->sc_iot, sc->sc_ioh,
		    hc * SATAHC_REGISTER_SIZE, SATAHC_REGISTER_SIZE,
		    &mvhc->hc_ioh)) {
			aprint_error_dev(MVSATA_DEV(sc),
			    "can't subregion SATAHC %d registers\n", hc);
			continue;
		}

		for (port = 0; port < sc->sc_port; port++)
			if (mvsata_port_init(mvhc, port) == 0) {
				int pre_amps;

				mvport = mvhc->hc_ports[port];
				pre_amps = read_pre_amps ?
				    read_preamps(mvport) : 0x00000720;
				mvport->_fix_phy_param.pre_amps = pre_amps;
				mvport->_fix_phy_param._fix_phy = _fix_phy;

				if (!mvsata_sreset)
					mvsata_reset_port(mvport);
			}

		if (!mvsata_sreset)
			mvsata_reset_hc(mvhc);
	}
	if (mvsata_sreset)
		mvsata_sreset(sc);

	if (mvsata_misc_reset)
		mvsata_misc_reset(sc);

	for (hc = 0; hc < sc->sc_hc; hc++)
		for (port = 0; port < sc->sc_port; port++) {
			mvport = sc->sc_hcs[hc].hc_ports[port];
			if (mvport == NULL)
				continue;
			if (mvsata_sreset)
				mvport->_fix_phy_param._fix_phy(mvport);
		}
	for (channel = 0; channel < sc->sc_hc * sc->sc_port; channel++)
		wdcattach(sc->sc_ata_channels[channel]);

	return 0;
}

int
mvsata_intr(struct mvsata_hc *mvhc)
{
	struct mvsata_softc *sc = mvhc->hc_sc;
	struct mvsata_port *mvport;
	uint32_t cause;
	int port, handled = 0;

	cause = MVSATA_HC_READ_4(mvhc, SATAHC_IC);

	DPRINTFN(3, ("%s:%d: mvsata_intr: cause=0x%08x\n",
	    device_xname(MVSATA_DEV(sc)), mvhc->hc, cause));

	if (cause & SATAHC_IC_SAINTCOAL)
		MVSATA_HC_WRITE_4(mvhc, SATAHC_IC, ~SATAHC_IC_SAINTCOAL);
	cause &= ~SATAHC_IC_SAINTCOAL;
	for (port = 0; port < sc->sc_port; port++) {
		mvport = mvhc->hc_ports[port];

		if (cause & SATAHC_IC_DONE(port)) {
#ifndef MVSATA_WITHOUTDMA
			handled = mvsata_edma_handle(mvport, NULL);
#endif
			MVSATA_HC_WRITE_4(mvhc, SATAHC_IC,
			    ~SATAHC_IC_DONE(port));
		}

		if (cause & SATAHC_IC_SADEVINTERRUPT(port)) {
			wdcintr(&mvport->port_ata_channel);
			MVSATA_HC_WRITE_4(mvhc, SATAHC_IC,
			    ~SATAHC_IC_SADEVINTERRUPT(port));
			handled = 1;
		}
	}

	return handled;
}

int
mvsata_error(struct mvsata_port *mvport)
{
	struct mvsata_softc *sc = device_private(MVSATA_DEV2(mvport));
	uint32_t cause;

	cause = MVSATA_EDMA_READ_4(mvport, EDMA_IEC);
	/*
	 * We must ack SATA_SE and SATA_FISIC before acking coresponding bits
	 * in EDMA_IEC.
	 */
	if (cause & EDMA_IE_SERRINT) {
		MVSATA_EDMA_WRITE_4(mvport, SATA_SE,
		    MVSATA_EDMA_READ_4(mvport, SATA_SEIM));
	}
	if (cause & EDMA_IE_ETRANSINT) {
		MVSATA_EDMA_WRITE_4(mvport, SATA_FISIC,
		    ~MVSATA_EDMA_READ_4(mvport, SATA_FISIM));
	}
	MVSATA_EDMA_WRITE_4(mvport, EDMA_IEC, ~cause);

	DPRINTFN(3, ("%s:%d:%d:"
	    " mvsata_error: cause=0x%08x, mask=0x%08x, status=0x%08x\n",
	    device_xname(MVSATA_DEV2(mvport)), mvport->port_hc->hc,
	    mvport->port, cause, MVSATA_EDMA_READ_4(mvport, EDMA_IEM),
	    MVSATA_EDMA_READ_4(mvport, EDMA_S)));

	cause &= MVSATA_EDMA_READ_4(mvport, EDMA_IEM);
	if (!cause)
		return 0;

	if (cause & EDMA_IE_EDEVDIS) {
		aprint_normal("%s:%d:%d: device disconnect\n",
		    device_xname(MVSATA_DEV2(mvport)),
		    mvport->port_hc->hc, mvport->port);
	}
	if (cause & EDMA_IE_EDEVCON) {
		if (sc->sc_gen == gen1)
			mvsata_devconn_gen1(mvport);

		DPRINTFN(3, ("    device connected\n"));
	}
#ifndef MVSATA_WITHOUTDMA
	if ((sc->sc_gen == gen1 && cause & EDMA_IE_ETRANSINT) ||
	    (sc->sc_gen != gen1 && cause & EDMA_IE_ESELFDIS)) {
		switch (mvport->port_edmamode) {
		case dma:
		case queued:
		case ncq:
			mvsata_edma_reset_qptr(mvport);
			mvsata_edma_enable(mvport);
			if (cause & EDMA_IE_EDEVERR)
				break;

			/* FALLTHROUGH */

		case nodma:
		default:
			aprint_error(
			    "%s:%d:%d: EDMA self disable happen 0x%x\n",
			    device_xname(MVSATA_DEV2(mvport)),
			    mvport->port_hc->hc, mvport->port, cause);
			break;
		}
	}
#endif
	if (cause & EDMA_IE_ETRANSINT) {
		/* hot plug the Port Multiplier */
		aprint_normal("%s:%d:%d: detect Port Multiplier?\n",
		    device_xname(MVSATA_DEV2(mvport)),
		    mvport->port_hc->hc, mvport->port);
	}

	return 1;
}


/*
 * ATA callback entry points
 */

static void
mvsata_probe_drive(struct ata_channel *chp)
{
	struct mvsata_port * const mvport = (struct mvsata_port *)chp;
	uint32_t sstat, sig;

	sstat = sata_reset_interface(chp, mvport->port_iot,
	    mvport->port_sata_scontrol, mvport->port_sata_sstatus, AT_WAIT);
	switch (sstat) {
	case SStatus_DET_DEV:
		mvsata_pmp_select(mvport, PMP_PORT_CTL);
		sig = mvsata_softreset(mvport, AT_WAIT);
		sata_interpret_sig(chp, 0, sig);
		break;
	default:
		break;
	}
}

#ifndef MVSATA_WITHOUTDMA
static int
mvsata_bio(struct ata_drive_datas *drvp, struct ata_bio *ata_bio)
{
	struct ata_channel *chp = drvp->chnl_softc;
	struct atac_softc *atac = chp->ch_atac;
	struct ata_xfer *xfer;

	DPRINTFN(1, ("%s:%d: mvsata_bio: drive=%d, blkno=%" PRId64
	    ", bcount=%ld\n", device_xname(atac->atac_dev), chp->ch_channel,
	    drvp->drive, ata_bio->blkno, ata_bio->bcount));

	xfer = ata_get_xfer(ATAXF_NOSLEEP);
	if (xfer == NULL)
		return ATACMD_TRY_AGAIN;
	if (atac->atac_cap & ATAC_CAP_NOIRQ)
		ata_bio->flags |= ATA_POLL;
	if (ata_bio->flags & ATA_POLL)
		xfer->c_flags |= C_POLL;
	if ((drvp->drive_flags & (ATA_DRIVE_DMA | ATA_DRIVE_UDMA)) &&
	    (ata_bio->flags & ATA_SINGLE) == 0)
		xfer->c_flags |= C_DMA;
	xfer->c_drive = drvp->drive;
	xfer->c_cmd = ata_bio;
	xfer->c_databuf = ata_bio->databuf;
	xfer->c_bcount = ata_bio->bcount;
	xfer->c_start = mvsata_bio_start;
	xfer->c_intr = mvsata_bio_intr;
	xfer->c_kill_xfer = mvsata_bio_kill_xfer;
	ata_exec_xfer(chp, xfer);
	return (ata_bio->flags & ATA_ITSDONE) ? ATACMD_COMPLETE : ATACMD_QUEUED;
}

static void
mvsata_reset_drive(struct ata_drive_datas *drvp, int flags, uint32_t *sigp)
{
	struct ata_channel *chp = drvp->chnl_softc;
	struct mvsata_port *mvport = (struct mvsata_port *)chp;
	uint32_t edma_c;
	uint32_t sig;

	edma_c = MVSATA_EDMA_READ_4(mvport, EDMA_CMD);

	DPRINTF(("%s:%d: mvsata_reset_drive: drive=%d (EDMA %sactive)\n",
	    device_xname(MVSATA_DEV2(mvport)), chp->ch_channel, drvp->drive,
	    (edma_c & EDMA_CMD_EENEDMA) ? "" : "not "));

	if (edma_c & EDMA_CMD_EENEDMA)
		mvsata_edma_disable(mvport, 10000, flags & AT_WAIT);

	mvsata_pmp_select(mvport, drvp->drive);

	sig = mvsata_softreset(mvport, flags & AT_WAIT);

	if (sigp)
		*sigp = sig;

	if (edma_c & EDMA_CMD_EENEDMA) {
		mvsata_edma_reset_qptr(mvport);
		mvsata_edma_enable(mvport);
	}
	return;
}

static void
mvsata_reset_channel(struct ata_channel *chp, int flags)
{
	struct mvsata_port *mvport = (struct mvsata_port *)chp;
	struct mvsata_softc *sc = device_private(MVSATA_DEV2(mvport));
	struct ata_xfer *xfer;
	uint32_t sstat, ctrl;
	int i;

	DPRINTF(("%s: mvsata_reset_channel: channel=%d\n",
	    device_xname(MVSATA_DEV2(mvport)), chp->ch_channel));

	mvsata_hreset_port(mvport);
	sstat = sata_reset_interface(chp, mvport->port_iot,
	    mvport->port_sata_scontrol, mvport->port_sata_sstatus, flags);

	if (flags & AT_WAIT && sstat == SStatus_DET_DEV_NE &&
	    sc->sc_gen != gen1) {
		/* Downgrade to GenI */
		const uint32_t val = SControl_IPM_NONE | SControl_SPD_ANY |
		    SControl_DET_DISABLE;

		MVSATA_EDMA_WRITE_4(mvport, mvport->port_sata_scontrol, val);

		ctrl = MVSATA_EDMA_READ_4(mvport, SATA_SATAICFG);
		ctrl &= ~(1 << 17);	/* Disable GenII */
		MVSATA_EDMA_WRITE_4(mvport, SATA_SATAICFG, ctrl);

		mvsata_hreset_port(mvport);
		sata_reset_interface(chp, mvport->port_iot,
		    mvport->port_sata_scontrol, mvport->port_sata_sstatus,
		    flags);
	}

	for (i = 0; i < MVSATA_EDMAQ_LEN; i++) {
		xfer = mvport->port_reqtbl[i].xfer;
		if (xfer == NULL)
			continue;
		chp->ch_queue->active_xfer = xfer;
		xfer->c_kill_xfer(chp, xfer, KILL_RESET);
	}

	mvsata_edma_config(mvport, mvport->port_edmamode);
	mvsata_edma_reset_qptr(mvport);
	mvsata_edma_enable(mvport);
	return;
}


static int
mvsata_exec_command(struct ata_drive_datas *drvp, struct ata_command *ata_c)
{
	struct ata_channel *chp = drvp->chnl_softc;
#ifdef MVSATA_DEBUG
	struct mvsata_port *mvport = (struct mvsata_port *)chp;
#endif
	struct ata_xfer *xfer;
	int rv, s;

	DPRINTFN(1, ("%s:%d: mvsata_exec_command: drive=%d, bcount=%d,"
	    " r_lba=0x%012"PRIx64", r_count=0x%04x, r_features=0x%04x,"
	    " r_device=0x%02x, r_command=0x%02x\n",
	    device_xname(MVSATA_DEV2(mvport)), chp->ch_channel,
	    drvp->drive, ata_c->bcount, ata_c->r_lba, ata_c->r_count,
	    ata_c->r_features, ata_c->r_device, ata_c->r_command));

	xfer = ata_get_xfer(ata_c->flags & AT_WAIT ? ATAXF_CANSLEEP :
	    ATAXF_NOSLEEP);
	if (xfer == NULL)
		return ATACMD_TRY_AGAIN;
	if (ata_c->flags & AT_POLL)
		xfer->c_flags |= C_POLL;
	if (ata_c->flags & AT_WAIT)
		xfer->c_flags |= C_WAIT;
	xfer->c_drive = drvp->drive;
	xfer->c_databuf = ata_c->data;
	xfer->c_bcount = ata_c->bcount;
	xfer->c_cmd = ata_c;
	xfer->c_start = mvsata_wdc_cmd_start;
	xfer->c_intr = mvsata_wdc_cmd_intr;
	xfer->c_kill_xfer = mvsata_wdc_cmd_kill_xfer;
	s = splbio();
	ata_exec_xfer(chp, xfer);
#ifdef DIAGNOSTIC
	if ((ata_c->flags & AT_POLL) != 0 &&
	    (ata_c->flags & AT_DONE) == 0)
		panic("mvsata_exec_command: polled command not done");
#endif
	if (ata_c->flags & AT_DONE)
		rv = ATACMD_COMPLETE;
	else {
		if (ata_c->flags & AT_WAIT) {
			while ((ata_c->flags & AT_DONE) == 0)
				tsleep(ata_c, PRIBIO, "mvsatacmd", 0);
			rv = ATACMD_COMPLETE;
		} else
			rv = ATACMD_QUEUED;
	}
	splx(s);
	return rv;
}

static int
mvsata_addref(struct ata_drive_datas *drvp)
{

	return 0;
}

static void
mvsata_delref(struct ata_drive_datas *drvp)
{

	return;
}

static void
mvsata_killpending(struct ata_drive_datas *drvp)
{

	return;
}

#if NATAPIBUS > 0
static void
mvsata_atapibus_attach(struct atabus_softc *ata_sc)
{
	struct ata_channel *chp = ata_sc->sc_chan;
	struct atac_softc *atac = chp->ch_atac;
	struct scsipi_adapter *adapt = &atac->atac_atapi_adapter._generic;
	struct scsipi_channel *chan = &chp->ch_atapi_channel;

	/*
	 * Fill in the scsipi_adapter.
	 */
	adapt->adapt_dev = atac->atac_dev;
	adapt->adapt_nchannels = atac->atac_nchannels;
	adapt->adapt_request = mvsata_atapi_scsipi_request;
	adapt->adapt_minphys = mvsata_atapi_minphys;
	atac->atac_atapi_adapter.atapi_probe_device = mvsata_atapi_probe_device;

        /*
	 * Fill in the scsipi_channel.
	 */
	memset(chan, 0, sizeof(*chan));
	chan->chan_adapter = adapt;
	chan->chan_bustype = &mvsata_atapi_bustype;
	chan->chan_channel = chp->ch_channel;
	chan->chan_flags = SCSIPI_CHAN_OPENINGS;
	chan->chan_openings = 1;
	chan->chan_max_periph = 1;
	chan->chan_ntargets = 1;
	chan->chan_nluns = 1;

	chp->atapibus =
	    config_found_ia(ata_sc->sc_dev, "atapi", chan, atapiprint);
}

static void
mvsata_atapi_scsipi_request(struct scsipi_channel *chan,
			    scsipi_adapter_req_t req, void *arg)
{
	struct scsipi_adapter *adapt = chan->chan_adapter;
	struct scsipi_periph *periph;
	struct scsipi_xfer *sc_xfer;
	struct mvsata_softc *sc = device_private(adapt->adapt_dev);
	struct atac_softc *atac = &sc->sc_wdcdev.sc_atac;
	struct ata_xfer *xfer;
	int channel = chan->chan_channel;
	int drive, s;

        switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		sc_xfer = arg;
		periph = sc_xfer->xs_periph;
		drive = periph->periph_target;

		if (!device_is_active(atac->atac_dev)) {
			sc_xfer->error = XS_DRIVER_STUFFUP;
			scsipi_done(sc_xfer);
			return;
		}
		xfer = ata_get_xfer(ATAXF_NOSLEEP);
		if (xfer == NULL) {
			sc_xfer->error = XS_RESOURCE_SHORTAGE;
			scsipi_done(sc_xfer);
			return;
		}

		if (sc_xfer->xs_control & XS_CTL_POLL)
			xfer->c_flags |= C_POLL;
		xfer->c_drive = drive;
		xfer->c_flags |= C_ATAPI;
		xfer->c_cmd = sc_xfer;
		xfer->c_databuf = sc_xfer->data;
		xfer->c_bcount = sc_xfer->datalen;
		xfer->c_start = mvsata_atapi_start;
		xfer->c_intr = mvsata_atapi_intr;
		xfer->c_kill_xfer = mvsata_atapi_kill_xfer;
		xfer->c_dscpoll = 0;
		s = splbio();
		ata_exec_xfer(atac->atac_channels[channel], xfer);
#ifdef DIAGNOSTIC
		if ((sc_xfer->xs_control & XS_CTL_POLL) != 0 &&
		    (sc_xfer->xs_status & XS_STS_DONE) == 0)
			panic("mvsata_atapi_scsipi_request:"
			    " polled command not done");
#endif
		splx(s);
		return;

	default:
		/* Not supported, nothing to do. */
		;
	}
}

static void
mvsata_atapi_minphys(struct buf *bp)
{

	if (bp->b_bcount > MAXPHYS)
		bp->b_bcount = MAXPHYS;
	minphys(bp);
}

static void
mvsata_atapi_probe_device(struct atapibus_softc *sc, int target)
{
	struct scsipi_channel *chan = sc->sc_channel;
	struct scsipi_periph *periph;
	struct ataparams ids;
	struct ataparams *id = &ids;
	struct mvsata_softc *mvc =
	    device_private(chan->chan_adapter->adapt_dev);
	struct atac_softc *atac = &mvc->sc_wdcdev.sc_atac;
	struct ata_channel *chp = atac->atac_channels[chan->chan_channel];
	struct ata_drive_datas *drvp = &chp->ch_drive[target];
	struct scsipibus_attach_args sa;
	char serial_number[21], model[41], firmware_revision[9];
	int s;

	/* skip if already attached */
	if (scsipi_lookup_periph(chan, target, 0) != NULL)
		return;

	/* if no ATAPI device detected at attach time, skip */
	if (drvp->drive_type != ATA_DRIVET_ATAPI) {
		DPRINTF(("%s:%d: mvsata_atapi_probe_device:"
		    " drive %d not present\n",
		    device_xname(atac->atac_dev), chp->ch_channel, target));
		return;
	}

        /* Some ATAPI devices need a bit more time after software reset. */
	delay(5000);
	if (ata_get_params(drvp, AT_WAIT, id) == 0) {
#ifdef ATAPI_DEBUG_PROBE
		log(LOG_DEBUG, "%s:%d: drive %d: cmdsz 0x%x drqtype 0x%x\n",
		    device_xname(atac->atac_dev), chp->ch_channel, target,
		    id->atap_config & ATAPI_CFG_CMD_MASK,
		    id->atap_config & ATAPI_CFG_DRQ_MASK);
#endif
		periph = scsipi_alloc_periph(M_NOWAIT);
		if (periph == NULL) {
			aprint_error_dev(atac->atac_dev,
			    "unable to allocate periph"
			    " for channel %d drive %d\n",
			    chp->ch_channel, target);
			return;
		}
		periph->periph_dev = NULL;
		periph->periph_channel = chan;
		periph->periph_switch = &atapi_probe_periphsw;
		periph->periph_target = target;
		periph->periph_lun = 0;
		periph->periph_quirks = PQUIRK_ONLYBIG;

#ifdef SCSIPI_DEBUG
		if (SCSIPI_DEBUG_TYPE == SCSIPI_BUSTYPE_ATAPI &&
		    SCSIPI_DEBUG_TARGET == target)
			periph->periph_dbflags |= SCSIPI_DEBUG_FLAGS;
#endif
		periph->periph_type = ATAPI_CFG_TYPE(id->atap_config);
		if (id->atap_config & ATAPI_CFG_REMOV)
			periph->periph_flags |= PERIPH_REMOVABLE;
		if (periph->periph_type == T_SEQUENTIAL) {
			s = splbio();
			drvp->drive_flags |= ATA_DRIVE_ATAPIDSCW;
			splx(s);
		}

		sa.sa_periph = periph;
		sa.sa_inqbuf.type = ATAPI_CFG_TYPE(id->atap_config);
		sa.sa_inqbuf.removable = id->atap_config & ATAPI_CFG_REMOV ?
		    T_REMOV : T_FIXED;
		scsipi_strvis((u_char *)model, 40, id->atap_model, 40);
		scsipi_strvis((u_char *)serial_number, 20, id->atap_serial, 20);
		scsipi_strvis((u_char *)firmware_revision, 8, id->atap_revision,
		    8);
		sa.sa_inqbuf.vendor = model;
		sa.sa_inqbuf.product = serial_number;
		sa.sa_inqbuf.revision = firmware_revision;

		/*
		 * Determine the operating mode capabilities of the device.
		 */
		if ((id->atap_config & ATAPI_CFG_CMD_MASK) == ATAPI_CFG_CMD_16)
			periph->periph_cap |= PERIPH_CAP_CMD16;
		/* XXX This is gross. */
		periph->periph_cap |= (id->atap_config & ATAPI_CFG_DRQ_MASK);

		drvp->drv_softc = atapi_probe_device(sc, target, periph, &sa);

		if (drvp->drv_softc)
			ata_probe_caps(drvp);
		else {
			s = splbio();
			drvp->drive_type = ATA_DRIVET_NONE;
			splx(s);
		}
	} else {
		DPRINTF(("%s:%d: mvsata_atapi_probe_device:"
		    " ATAPI_IDENTIFY_DEVICE failed for drive %d: error 0x%x\n",
		    device_xname(atac->atac_dev), chp->ch_channel, target,
		    chp->ch_error));
		s = splbio();
		drvp->drive_type = ATA_DRIVET_NONE;
		splx(s);
	}
}

/*
 * Kill off all pending xfers for a periph.
 *
 * Must be called at splbio().
 */
static void
mvsata_atapi_kill_pending(struct scsipi_periph *periph)
{
	struct atac_softc *atac =
	    device_private(periph->periph_channel->chan_adapter->adapt_dev);
	struct ata_channel *chp =
	    atac->atac_channels[periph->periph_channel->chan_channel];

	ata_kill_pending(&chp->ch_drive[periph->periph_target]);
}
#endif	/* NATAPIBUS > 0 */
#endif	/* MVSATA_WITHOUTDMA */


/*
 * mvsata_setup_channel()
 *   Setup EDMA registers and prepare/purge DMA resources.
 *   We assuming already stopped the EDMA.
 */
static void
mvsata_setup_channel(struct ata_channel *chp)
{
#if !defined(MVSATA_WITHOUTDMA) || defined(MVSATA_DEBUG)
	struct mvsata_port *mvport = (struct mvsata_port *)chp;
#endif
	struct ata_drive_datas *drvp;
	uint32_t edma_mode;
	int drive, s;
#ifndef MVSATA_WITHOUTDMA
	int i;
	const int crqb_size = sizeof(union mvsata_crqb) * MVSATA_EDMAQ_LEN;
	const int crpb_size = sizeof(struct crpb) * MVSATA_EDMAQ_LEN;
	const int eprd_buf_size = MVSATA_EPRD_MAX_SIZE * MVSATA_EDMAQ_LEN;
#endif

	DPRINTF(("%s:%d: mvsata_setup_channel: ",
	    device_xname(MVSATA_DEV2(mvport)), chp->ch_channel));

	edma_mode = nodma;
	for (drive = 0; drive < chp->ch_ndrives; drive++) {
		drvp = &chp->ch_drive[drive];

		/* If no drive, skip */
		if (drvp->drive_type == ATA_DRIVET_NONE)
			continue;

		if (drvp->drive_flags & ATA_DRIVE_UDMA) {
			/* use Ultra/DMA */
			s = splbio();
			drvp->drive_flags &= ~ATA_DRIVE_DMA;
			splx(s);
		}

		if (drvp->drive_flags & (ATA_DRIVE_UDMA | ATA_DRIVE_DMA))
			if (drvp->drive_type == ATA_DRIVET_ATA)
				edma_mode = dma;
	}

	DPRINTF(("EDMA %sactive mode\n", (edma_mode == nodma) ? "not " : ""));

#ifndef MVSATA_WITHOUTDMA
	if (edma_mode == nodma) {
no_edma:
		if (mvport->port_crqb != NULL)
			mvsata_edma_resource_purge(mvport, mvport->port_dmat,
			    mvport->port_crqb_dmamap, mvport->port_crqb);
		if (mvport->port_crpb != NULL)
			mvsata_edma_resource_purge(mvport, mvport->port_dmat,
			    mvport->port_crpb_dmamap, mvport->port_crpb);
		if (mvport->port_eprd != NULL)
			mvsata_edma_resource_purge(mvport, mvport->port_dmat,
			    mvport->port_eprd_dmamap, mvport->port_eprd);

		return;
	}

	if (mvport->port_crqb == NULL)
		mvport->port_crqb = mvsata_edma_resource_prepare(mvport,
		    mvport->port_dmat, &mvport->port_crqb_dmamap, crqb_size, 1);
	if (mvport->port_crpb == NULL)
		mvport->port_crpb = mvsata_edma_resource_prepare(mvport,
		    mvport->port_dmat, &mvport->port_crpb_dmamap, crpb_size, 0);
	if (mvport->port_eprd == NULL) {
		mvport->port_eprd = mvsata_edma_resource_prepare(mvport,
		    mvport->port_dmat, &mvport->port_eprd_dmamap, eprd_buf_size,
		    1);
		for (i = 0; i < MVSATA_EDMAQ_LEN; i++) {
			mvport->port_reqtbl[i].eprd_offset =
			    i * MVSATA_EPRD_MAX_SIZE;
			mvport->port_reqtbl[i].eprd = mvport->port_eprd +
			    i * MVSATA_EPRD_MAX_SIZE / sizeof(struct eprd);
		}
	}

	if (mvport->port_crqb == NULL || mvport->port_crpb == NULL ||
	    mvport->port_eprd == NULL) {
		aprint_error_dev(MVSATA_DEV2(mvport),
		    "channel %d: can't use EDMA\n", chp->ch_channel);
		s = splbio();
		for (drive = 0; drive < chp->ch_ndrives; drive++) {
			drvp = &chp->ch_drive[drive];

			/* If no drive, skip */
			if (drvp->drive_type == ATA_DRIVET_NONE)
				continue;

			drvp->drive_flags &= ~(ATA_DRIVE_UDMA | ATA_DRIVE_DMA);
		}
		splx(s);
		goto no_edma;
	}

	mvsata_edma_config(mvport, edma_mode);
	mvsata_edma_reset_qptr(mvport);
	mvsata_edma_enable(mvport);
#endif
}

#ifndef MVSATA_WITHOUTDMA
static void
mvsata_bio_start(struct ata_channel *chp, struct ata_xfer *xfer)
{
	struct mvsata_port *mvport = (struct mvsata_port *)chp;
	struct mvsata_softc *sc = device_private(MVSATA_DEV2(mvport));
	struct atac_softc *atac = chp->ch_atac;
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	struct ata_bio *ata_bio = xfer->c_cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->c_drive];
	int wait_flags = (xfer->c_flags & C_POLL) ? AT_POLL : 0;
	u_int16_t cyl;
	u_int8_t head, sect, cmd = 0;
	int nblks, error;

	DPRINTFN(2, ("%s:%d: mvsata_bio_start: drive=%d\n",
	    device_xname(atac->atac_dev), chp->ch_channel, xfer->c_drive));

	if (xfer->c_flags & C_DMA)
		if (drvp->n_xfers <= NXFER)
			drvp->n_xfers++;

again:
	/*
	 *
	 * When starting a multi-sector transfer, or doing single-sector
	 * transfers...
	 */
	if (xfer->c_skip == 0 || (ata_bio->flags & ATA_SINGLE) != 0) {
		if (ata_bio->flags & ATA_SINGLE)
			nblks = 1;
		else
			nblks = xfer->c_bcount / ata_bio->lp->d_secsize;
		/* Check for bad sectors and adjust transfer, if necessary. */
		if ((ata_bio->lp->d_flags & D_BADSECT) != 0) {
			long blkdiff;
			int i;

			for (i = 0; (blkdiff = ata_bio->badsect[i]) != -1;
			    i++) {
				blkdiff -= ata_bio->blkno;
				if (blkdiff < 0)
					continue;
				if (blkdiff == 0)
					/* Replace current block of transfer. */
					ata_bio->blkno =
					    ata_bio->lp->d_secperunit -
					    ata_bio->lp->d_nsectors - i - 1;
				if (blkdiff < nblks) {
					/* Bad block inside transfer. */
					ata_bio->flags |= ATA_SINGLE;
					nblks = 1;
				}
				break;
			}
			/* Transfer is okay now. */
		}
		if (xfer->c_flags & C_DMA) {
			ata_bio->nblks = nblks;
			ata_bio->nbytes = xfer->c_bcount;

			if (xfer->c_flags & C_POLL)
				sc->sc_enable_intr(mvport, 0 /*off*/);
			error = mvsata_edma_enqueue(mvport, ata_bio,
			    (char *)xfer->c_databuf + xfer->c_skip);
			if (error) {
				if (error == EINVAL) {
					/*
					 * We can't do DMA on this transfer
					 * for some reason.  Fall back to
					 * PIO.
					 */
					xfer->c_flags &= ~C_DMA;
					error = 0;
					goto do_pio;
				}
				if (error == EBUSY) {
					aprint_error_dev(atac->atac_dev,
					    "channel %d: EDMA Queue full\n",
					    chp->ch_channel);
					/*
					 * XXXX: Perhaps, after it waits for
					 * a while, it is necessary to call
					 * bio_start again.
					 */
				}
				ata_bio->error = ERR_DMA;
				ata_bio->r_error = 0;
				mvsata_bio_done(chp, xfer);
				return;
			}
			chp->ch_flags |= ATACH_DMA_WAIT;
			/* start timeout machinery */
			if ((xfer->c_flags & C_POLL) == 0)
				callout_reset(&chp->ch_callout,
				    ATA_DELAY / 1000 * hz,
				    mvsata_edma_timeout, xfer);
			/* wait for irq */
			goto intr;
		} /* else not DMA */
do_pio:
		if (ata_bio->flags & ATA_LBA48) {
			sect = 0;
			cyl =  0;
			head = 0;
		} else if (ata_bio->flags & ATA_LBA) {
			sect = (ata_bio->blkno >> 0) & 0xff;
			cyl = (ata_bio->blkno >> 8) & 0xffff;
			head = (ata_bio->blkno >> 24) & 0x0f;
			head |= WDSD_LBA;
		} else {
			int blkno = ata_bio->blkno;
			sect = blkno % ata_bio->lp->d_nsectors;
			sect++;	/* Sectors begin with 1, not 0. */
			blkno /= ata_bio->lp->d_nsectors;
			head = blkno % ata_bio->lp->d_ntracks;
			blkno /= ata_bio->lp->d_ntracks;
			cyl = blkno;
			head |= WDSD_CHS;
		}
		ata_bio->nblks = min(nblks, ata_bio->multi);
		ata_bio->nbytes = ata_bio->nblks * ata_bio->lp->d_secsize;
		KASSERT(nblks == 1 || (ata_bio->flags & ATA_SINGLE) == 0);
		if (ata_bio->nblks > 1)
			cmd = (ata_bio->flags & ATA_READ) ?
			    WDCC_READMULTI : WDCC_WRITEMULTI;
		else
			cmd = (ata_bio->flags & ATA_READ) ?
			    WDCC_READ : WDCC_WRITE;

		/* EDMA disable, if enabled this channel. */
		if (mvport->port_edmamode != nodma)
			mvsata_edma_disable(mvport, 10 /* ms */, wait_flags);

		mvsata_pmp_select(mvport, xfer->c_drive);

		/* Do control operations specially. */
		if (__predict_false(drvp->state < READY)) {
			/*
			 * Actually, we want to be careful not to mess with
			 * the control state if the device is currently busy,
			 * but we can assume that we never get to this point
			 * if that's the case.
			 */
			/*
			 * If it's not a polled command, we need the kernel
			 * thread
			 */
			if ((xfer->c_flags & C_POLL) == 0 && cpu_intr_p()) {
				chp->ch_queue->queue_freeze++;
				wakeup(&chp->ch_thread);
				return;
			}
			if (mvsata_bio_ready(mvport, ata_bio, xfer->c_drive,
			    (xfer->c_flags & C_POLL) ? AT_POLL : 0) != 0) {
				mvsata_bio_done(chp, xfer);
				return;
			}
		}

		/* Initiate command! */
		MVSATA_WDC_WRITE_1(mvport, SRB_H, WDSD_IBM);
		switch(wdc_wait_for_ready(chp, ATA_DELAY, wait_flags)) {
		case WDCWAIT_OK:
			break;
		case WDCWAIT_TOUT:
			goto timeout;
		case WDCWAIT_THR:
			return;
		}
		if (ata_bio->flags & ATA_LBA48)
			wdccommandext(chp, 0, atacmd_to48(cmd),
			    ata_bio->blkno, nblks, 0, WDSD_LBA);
		else
			wdccommand(chp, 0, cmd, cyl,
			    head, sect, nblks,
			    (ata_bio->lp->d_type == DKTYPE_ST506) ?
			    ata_bio->lp->d_precompcyl / 4 : 0);

		/* start timeout machinery */
		if ((xfer->c_flags & C_POLL) == 0)
			callout_reset(&chp->ch_callout,
			    ATA_DELAY / 1000 * hz, wdctimeout, chp);
	} else if (ata_bio->nblks > 1) {
		/* The number of blocks in the last stretch may be smaller. */
		nblks = xfer->c_bcount / ata_bio->lp->d_secsize;
		if (ata_bio->nblks > nblks) {
			ata_bio->nblks = nblks;
			ata_bio->nbytes = xfer->c_bcount;
		}
	}
	/* If this was a write and not using DMA, push the data. */
	if ((ata_bio->flags & ATA_READ) == 0) {
		/*
		 * we have to busy-wait here, we can't rely on running in
		 * thread context.
		 */
		if (wdc_wait_for_drq(chp, ATA_DELAY, AT_POLL) != 0) {
			aprint_error_dev(atac->atac_dev,
			    "channel %d: drive %d timeout waiting for DRQ,"
			    " st=0x%02x, err=0x%02x\n",
			    chp->ch_channel, xfer->c_drive, chp->ch_status,
			    chp->ch_error);
			ata_bio->error = TIMEOUT;
			mvsata_bio_done(chp, xfer);
			return;
		}
		if (chp->ch_status & WDCS_ERR) {
			ata_bio->error = ERROR;
			ata_bio->r_error = chp->ch_error;
			mvsata_bio_done(chp, xfer);
			return;
		}

		wdc->dataout_pio(chp, drvp->drive_flags,
		    (char *)xfer->c_databuf + xfer->c_skip, ata_bio->nbytes);
	}

intr:
	/* Wait for IRQ (either real or polled) */
	if ((ata_bio->flags & ATA_POLL) == 0) {
		chp->ch_flags |= ATACH_IRQ_WAIT;
	} else {
		/* Wait for at last 400ns for status bit to be valid */
		delay(1);
		if (chp->ch_flags & ATACH_DMA_WAIT) {
			mvsata_edma_wait(mvport, xfer, ATA_DELAY);
			sc->sc_enable_intr(mvport, 1 /*on*/);
			chp->ch_flags &= ~ATACH_DMA_WAIT;
		}
		mvsata_bio_intr(chp, xfer, 0);
		if ((ata_bio->flags & ATA_ITSDONE) == 0)
			goto again;
	}
	return;

timeout:
	aprint_error_dev(atac->atac_dev,
	    "channel %d: drive %d not ready, st=0x%02x, err=0x%02x\n",
	    chp->ch_channel, xfer->c_drive, chp->ch_status, chp->ch_error);
	ata_bio->error = TIMEOUT;
	mvsata_bio_done(chp, xfer);
	return;
}

static int
mvsata_bio_intr(struct ata_channel *chp, struct ata_xfer *xfer, int irq)
{
	struct atac_softc *atac = chp->ch_atac;
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	struct ata_bio *ata_bio = xfer->c_cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->c_drive];

	DPRINTFN(2, ("%s:%d: mvsata_bio_intr: drive=%d\n",
	    device_xname(atac->atac_dev), chp->ch_channel, xfer->c_drive));

	chp->ch_flags &= ~(ATACH_IRQ_WAIT|ATACH_DMA_WAIT);

	/*
	 * If we missed an interrupt transfer, reset and restart.
	 * Don't try to continue transfer, we may have missed cycles.
	 */
	if (xfer->c_flags & C_TIMEOU) {
		ata_bio->error = TIMEOUT;
		mvsata_bio_done(chp, xfer);
		return 1;
	}

	/* Is it not a transfer, but a control operation? */
	if (!(xfer->c_flags & C_DMA) && drvp->state < READY) {
		aprint_error_dev(atac->atac_dev,
		    "channel %d: drive %d bad state %d in mvsata_bio_intr\n",
		    chp->ch_channel, xfer->c_drive, drvp->state);
		panic("mvsata_bio_intr: bad state");
	}

	/* Ack interrupt done by wdc_wait_for_unbusy */
	if (!(xfer->c_flags & C_DMA) &&
	    (wdc_wait_for_unbusy(chp, (irq == 0) ? ATA_DELAY : 0, AT_POLL)
							== WDCWAIT_TOUT)) {
		if (irq && (xfer->c_flags & C_TIMEOU) == 0)
			return 0;	/* IRQ was not for us */
		aprint_error_dev(atac->atac_dev,
		    "channel %d: drive %d timeout, c_bcount=%d, c_skip%d\n",
		    chp->ch_channel, xfer->c_drive, xfer->c_bcount,
		    xfer->c_skip);
		ata_bio->error = TIMEOUT;
		mvsata_bio_done(chp, xfer);
		return 1;
	}

	if (xfer->c_flags & C_DMA) {
		if (ata_bio->error == NOERROR)
			goto end;
		if (ata_bio->error == ERR_DMA)
			ata_dmaerr(drvp,
			    (xfer->c_flags & C_POLL) ? AT_POLL : 0);
	}

	/* if we had an error, end */
	if (ata_bio->error != NOERROR) {
		mvsata_bio_done(chp, xfer);
		return 1;
	}

	/* If this was a read and not using DMA, fetch the data. */
	if ((ata_bio->flags & ATA_READ) != 0) {
		if ((chp->ch_status & WDCS_DRQ) != WDCS_DRQ) {
			aprint_error_dev(atac->atac_dev,
			    "channel %d: drive %d read intr before drq\n",
			    chp->ch_channel, xfer->c_drive);
			ata_bio->error = TIMEOUT;
			mvsata_bio_done(chp, xfer);
			return 1;
		}
		wdc->datain_pio(chp, drvp->drive_flags,
		    (char *)xfer->c_databuf + xfer->c_skip, ata_bio->nbytes);
	}

end:
	ata_bio->blkno += ata_bio->nblks;
	ata_bio->blkdone += ata_bio->nblks;
	xfer->c_skip += ata_bio->nbytes;
	xfer->c_bcount -= ata_bio->nbytes;
	/* See if this transfer is complete. */
	if (xfer->c_bcount > 0) {
		if ((ata_bio->flags & ATA_POLL) == 0)
			/* Start the next operation */
			mvsata_bio_start(chp, xfer);
		else
			/* Let mvsata_bio_start do the loop */
			return 1;
	} else { /* Done with this transfer */
		ata_bio->error = NOERROR;
		mvsata_bio_done(chp, xfer);
	}
	return 1;
}

static void
mvsata_bio_kill_xfer(struct ata_channel *chp, struct ata_xfer *xfer, int reason)
{
	struct mvsata_port *mvport = (struct mvsata_port *)chp;
	struct atac_softc *atac = chp->ch_atac;
	struct ata_bio *ata_bio = xfer->c_cmd;
	int drive = xfer->c_drive;

	DPRINTFN(2, ("%s:%d: mvsata_bio_kill_xfer: drive=%d\n",
	    device_xname(atac->atac_dev), chp->ch_channel, xfer->c_drive));

	/* EDMA restart, if enabled */
	if (!(xfer->c_flags & C_DMA) && mvport->port_edmamode != nodma) {
		mvsata_edma_reset_qptr(mvport);
		mvsata_edma_enable(mvport);
	}

	ata_free_xfer(chp, xfer);

	ata_bio->flags |= ATA_ITSDONE;
	switch (reason) {
	case KILL_GONE:
		ata_bio->error = ERR_NODEV;
		break;
	case KILL_RESET:
		ata_bio->error = ERR_RESET;
		break;
	default:
		aprint_error_dev(atac->atac_dev,
		    "mvsata_bio_kill_xfer: unknown reason %d\n", reason);
		panic("mvsata_bio_kill_xfer");
	}
	ata_bio->r_error = WDCE_ABRT;
	(*chp->ch_drive[drive].drv_done)(chp->ch_drive[drive].drv_softc);
}

static void
mvsata_bio_done(struct ata_channel *chp, struct ata_xfer *xfer)
{
	struct mvsata_port *mvport = (struct mvsata_port *)chp;
	struct ata_bio *ata_bio = xfer->c_cmd;
	int drive = xfer->c_drive;

	DPRINTFN(2, ("%s:%d: mvsata_bio_done: drive=%d, flags=0x%x\n",
	    device_xname(MVSATA_DEV2(mvport)), chp->ch_channel, xfer->c_drive,
	    (u_int)xfer->c_flags));

	callout_stop(&chp->ch_callout);

	/* EDMA restart, if enabled */
	if (!(xfer->c_flags & C_DMA) && mvport->port_edmamode != nodma) {
		mvsata_edma_reset_qptr(mvport);
		mvsata_edma_enable(mvport);
	}

	/* feed back residual bcount to our caller */
	ata_bio->bcount = xfer->c_bcount;

	/* mark controller inactive and free xfer */
	KASSERT(chp->ch_queue->active_xfer != NULL);
	chp->ch_queue->active_xfer = NULL;
	ata_free_xfer(chp, xfer);

	if (chp->ch_drive[drive].drive_flags & ATA_DRIVE_WAITDRAIN) {
		ata_bio->error = ERR_NODEV;
		chp->ch_drive[drive].drive_flags &= ~ATA_DRIVE_WAITDRAIN;
		wakeup(&chp->ch_queue->active_xfer);
	}
	ata_bio->flags |= ATA_ITSDONE;
	(*chp->ch_drive[drive].drv_done)(chp->ch_drive[drive].drv_softc);
	atastart(chp);
}

static int
mvsata_bio_ready(struct mvsata_port *mvport, struct ata_bio *ata_bio, int drive,
		 int flags)
{
	struct ata_channel *chp = &mvport->port_ata_channel;
	struct atac_softc *atac = chp->ch_atac;
	struct ata_drive_datas *drvp = &chp->ch_drive[drive];
	const char *errstring;

	flags |= AT_POLL;	/* XXX */

	/*
	 * disable interrupts, all commands here should be quick
	 * enough to be able to poll, and we don't go here that often
	 */
	MVSATA_WDC_WRITE_1(mvport, SRB_CAS, WDCTL_4BIT | WDCTL_IDS);
	MVSATA_WDC_WRITE_1(mvport, SRB_H, WDSD_IBM);
	DELAY(10);
	errstring = "wait";
	if (wdcwait(chp, WDCS_DRDY, WDCS_DRDY, ATA_DELAY, flags))
		goto ctrltimeout;
	wdccommandshort(chp, 0, WDCC_RECAL);
	/* Wait for at least 400ns for status bit to be valid */
	DELAY(1);
	errstring = "recal";
	if (wdcwait(chp, WDCS_DRDY, WDCS_DRDY, ATA_DELAY, flags))
		goto ctrltimeout;
	if (chp->ch_status & (WDCS_ERR | WDCS_DWF))
		goto ctrlerror;
	/* Don't try to set modes if controller can't be adjusted */
	if (atac->atac_set_modes == NULL)
		goto geometry;
	/* Also don't try if the drive didn't report its mode */
	if ((drvp->drive_flags & ATA_DRIVE_MODE) == 0)
		goto geometry;
	wdccommand(chp, 0, SET_FEATURES, 0, 0, 0,
	    0x08 | drvp->PIO_mode, WDSF_SET_MODE);
	errstring = "piomode";
	if (wdcwait(chp, WDCS_DRDY, WDCS_DRDY, ATA_DELAY, flags))
		goto ctrltimeout;
	if (chp->ch_status & (WDCS_ERR | WDCS_DWF))
		goto ctrlerror;
	if (drvp->drive_flags & ATA_DRIVE_UDMA)
		wdccommand(chp, 0, SET_FEATURES, 0, 0, 0,
		    0x40 | drvp->UDMA_mode, WDSF_SET_MODE);
	else if (drvp->drive_flags & ATA_DRIVE_DMA)
		wdccommand(chp, 0, SET_FEATURES, 0, 0, 0,
		    0x20 | drvp->DMA_mode, WDSF_SET_MODE);
	else
		goto geometry;
	errstring = "dmamode";
	if (wdcwait(chp, WDCS_DRDY, WDCS_DRDY, ATA_DELAY, flags))
		goto ctrltimeout;
	if (chp->ch_status & (WDCS_ERR | WDCS_DWF))
		goto ctrlerror;
geometry:
	if (ata_bio->flags & ATA_LBA)
		goto multimode;
	wdccommand(chp, 0, WDCC_IDP, ata_bio->lp->d_ncylinders,
	    ata_bio->lp->d_ntracks - 1, 0, ata_bio->lp->d_nsectors,
	    (ata_bio->lp->d_type == DKTYPE_ST506) ?
	    ata_bio->lp->d_precompcyl / 4 : 0);
	errstring = "geometry";
	if (wdcwait(chp, WDCS_DRDY, WDCS_DRDY, ATA_DELAY, flags))
		goto ctrltimeout;
	if (chp->ch_status & (WDCS_ERR | WDCS_DWF))
		goto ctrlerror;
multimode:
	if (ata_bio->multi == 1)
		goto ready;
	wdccommand(chp, 0, WDCC_SETMULTI, 0, 0, 0, ata_bio->multi, 0);
	errstring = "setmulti";
	if (wdcwait(chp, WDCS_DRDY, WDCS_DRDY, ATA_DELAY, flags))
		goto ctrltimeout;
	if (chp->ch_status & (WDCS_ERR | WDCS_DWF))
		goto ctrlerror;
ready:
	drvp->state = READY;
	/*
	 * The drive is usable now
	 */
	MVSATA_WDC_WRITE_1(mvport, SRB_CAS, WDCTL_4BIT);
	delay(10);	/* some drives need a little delay here */
	return 0;

ctrltimeout:
	aprint_error_dev(atac->atac_dev, "channel %d: drive %d %s timed out\n",
	    chp->ch_channel, drive, errstring);
	ata_bio->error = TIMEOUT;
	goto ctrldone;
ctrlerror:
	aprint_error_dev(atac->atac_dev, "channel %d: drive %d %s ",
	    chp->ch_channel, drive, errstring);
	if (chp->ch_status & WDCS_DWF) {
		aprint_error("drive fault\n");
		ata_bio->error = ERR_DF;
	} else {
		aprint_error("error (%x)\n", chp->ch_error);
		ata_bio->r_error = chp->ch_error;
		ata_bio->error = ERROR;
	}
ctrldone:
	drvp->state = 0;
	MVSATA_WDC_WRITE_1(mvport, SRB_CAS, WDCTL_4BIT);
	return -1;
}

static void
mvsata_wdc_cmd_start(struct ata_channel *chp, struct ata_xfer *xfer)
{
	struct mvsata_port *mvport = (struct mvsata_port *)chp;
	int drive = xfer->c_drive;
	int wait_flags = (xfer->c_flags & C_POLL) ? AT_POLL : 0;
	struct ata_command *ata_c = xfer->c_cmd;

	DPRINTFN(1, ("%s:%d: mvsata_cmd_start: drive=%d\n",
	    device_xname(MVSATA_DEV2(mvport)), chp->ch_channel, drive));

	/* First, EDMA disable, if enabled this channel. */
	if (mvport->port_edmamode != nodma)
		mvsata_edma_disable(mvport, 10 /* ms */, wait_flags);

	mvsata_pmp_select(mvport, drive);

	MVSATA_WDC_WRITE_1(mvport, SRB_H, WDSD_IBM);
	switch(wdcwait(chp, ata_c->r_st_bmask | WDCS_DRQ,
	    ata_c->r_st_bmask, ata_c->timeout, wait_flags)) {
	case WDCWAIT_OK:
		break;
	case WDCWAIT_TOUT:
		ata_c->flags |= AT_TIMEOU;
		mvsata_wdc_cmd_done(chp, xfer);
		return;
	case WDCWAIT_THR:
		return;
	}
	if (ata_c->flags & AT_POLL)
		/* polled command, disable interrupts */
		MVSATA_WDC_WRITE_1(mvport, SRB_CAS, WDCTL_4BIT | WDCTL_IDS);
	if ((ata_c->flags & AT_LBA48) != 0) {
		wdccommandext(chp, 0, ata_c->r_command,
		    ata_c->r_lba, ata_c->r_count, ata_c->r_features,
		    ata_c->r_device & ~0x10);
	} else {
		wdccommand(chp, 0, ata_c->r_command,
		    (ata_c->r_lba >> 8) & 0xffff,
		    (((ata_c->flags & AT_LBA) != 0) ? WDSD_LBA : 0) |
		    ((ata_c->r_lba >> 24) & 0x0f),
		    ata_c->r_lba & 0xff,
		    ata_c->r_count & 0xff,
		    ata_c->r_features & 0xff);
	}

	if ((ata_c->flags & AT_POLL) == 0) {
		chp->ch_flags |= ATACH_IRQ_WAIT; /* wait for interrupt */
		callout_reset(&chp->ch_callout, ata_c->timeout / 1000 * hz,
		    wdctimeout, chp);
		return;
	}
	/*
	 * Polled command. Wait for drive ready or drq. Done in intr().
	 * Wait for at last 400ns for status bit to be valid.
	 */
	delay(10);	/* 400ns delay */
	mvsata_wdc_cmd_intr(chp, xfer, 0);
}

static int
mvsata_wdc_cmd_intr(struct ata_channel *chp, struct ata_xfer *xfer, int irq)
{
	struct mvsata_port *mvport = (struct mvsata_port *)chp;
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	struct ata_command *ata_c = xfer->c_cmd;
	int bcount = ata_c->bcount;
	char *data = ata_c->data;
	int wflags;
	int drive_flags;

	if (ata_c->r_command == WDCC_IDENTIFY ||
	    ata_c->r_command == ATAPI_IDENTIFY_DEVICE)
		/*
		 * The IDENTIFY data has been designed as an array of
		 * u_int16_t, so we can byteswap it on the fly.
		 * Historically it's what we have always done so keeping it
		 * here ensure binary backward compatibility.
		 */
		drive_flags = ATA_DRIVE_NOSTREAM |
		    chp->ch_drive[xfer->c_drive].drive_flags;
	else
		/*
		 * Other data structure are opaque and should be transfered
		 * as is.
		 */
		drive_flags = chp->ch_drive[xfer->c_drive].drive_flags;

	if ((ata_c->flags & (AT_WAIT | AT_POLL)) == (AT_WAIT | AT_POLL))
		/* both wait and poll, we can tsleep here */
		wflags = AT_WAIT | AT_POLL;
	else
		wflags = AT_POLL;

again:
	DPRINTFN(1, ("%s:%d: mvsata_cmd_intr: drive=%d\n",
	    device_xname(MVSATA_DEV2(mvport)), chp->ch_channel, xfer->c_drive));

	/*
	 * after a ATAPI_SOFT_RESET, the device will have released the bus.
	 * Reselect again, it doesn't hurt for others commands, and the time
	 * penalty for the extra register write is acceptable,
	 * wdc_exec_command() isn't called often (mostly for autoconfig)
	 */
	if ((xfer->c_flags & C_ATAPI) != 0) {
		MVSATA_WDC_WRITE_1(mvport, SRB_H, WDSD_IBM);
	}
	if ((ata_c->flags & AT_XFDONE) != 0) {
		/*
		 * We have completed a data xfer. The drive should now be
		 * in its initial state
		 */
		if (wdcwait(chp, ata_c->r_st_bmask | WDCS_DRQ,
		    ata_c->r_st_bmask, (irq == 0)  ? ata_c->timeout : 0,
		    wflags) ==  WDCWAIT_TOUT) {
			if (irq && (xfer->c_flags & C_TIMEOU) == 0)
				return 0;	/* IRQ was not for us */
			ata_c->flags |= AT_TIMEOU;
		}
		goto out;
	}
	if (wdcwait(chp, ata_c->r_st_pmask, ata_c->r_st_pmask,
	    (irq == 0)  ? ata_c->timeout : 0, wflags) == WDCWAIT_TOUT) {
		if (irq && (xfer->c_flags & C_TIMEOU) == 0)
		    return 0;	/* IRQ was not for us */
		ata_c->flags |= AT_TIMEOU;
		goto out;
	}
	delay(20);	/* XXXXX: Delay more times. */
	if (ata_c->flags & AT_READ) {
		if ((chp->ch_status & WDCS_DRQ) == 0) {
			ata_c->flags |= AT_TIMEOU;
			goto out;
		}
		wdc->datain_pio(chp, drive_flags, data, bcount);
		/* at this point the drive should be in its initial state */
		ata_c->flags |= AT_XFDONE;
		/*
		 * XXX checking the status register again here cause some
		 * hardware to timeout.
		 */
	} else if (ata_c->flags & AT_WRITE) {
		if ((chp->ch_status & WDCS_DRQ) == 0) {
			ata_c->flags |= AT_TIMEOU;
			goto out;
		}
		wdc->dataout_pio(chp, drive_flags, data, bcount);
		ata_c->flags |= AT_XFDONE;
		if ((ata_c->flags & AT_POLL) == 0) {
			chp->ch_flags |= ATACH_IRQ_WAIT; /* wait for intr */
			callout_reset(&chp->ch_callout,
			    mstohz(ata_c->timeout), wdctimeout, chp);
			return 1;
		} else
			goto again;
	}
out:
	mvsata_wdc_cmd_done(chp, xfer);
	return 1;
}

static void
mvsata_wdc_cmd_kill_xfer(struct ata_channel *chp, struct ata_xfer *xfer,
			 int reason)
{
	struct mvsata_port *mvport = (struct mvsata_port *)chp;
	struct ata_command *ata_c = xfer->c_cmd;

	DPRINTFN(1, ("%s:%d: mvsata_cmd_kill_xfer: drive=%d\n",
	    device_xname(MVSATA_DEV2(mvport)), chp->ch_channel, xfer->c_drive));

	switch (reason) {
	case KILL_GONE:
		ata_c->flags |= AT_GONE;
		break;
	case KILL_RESET:
		ata_c->flags |= AT_RESET;
		break;
	default:
		aprint_error_dev(MVSATA_DEV2(mvport),
		    "mvsata_cmd_kill_xfer: unknown reason %d\n", reason);
		panic("mvsata_cmd_kill_xfer");
	}
	mvsata_wdc_cmd_done_end(chp, xfer);
}

static void
mvsata_wdc_cmd_done(struct ata_channel *chp, struct ata_xfer *xfer)
{
	struct mvsata_port *mvport = (struct mvsata_port *)chp;
	struct atac_softc *atac = chp->ch_atac;
	struct ata_command *ata_c = xfer->c_cmd;

	DPRINTFN(1, ("%s:%d: mvsata_cmd_done: drive=%d, flags=0x%x\n",
	    device_xname(atac->atac_dev), chp->ch_channel, xfer->c_drive,
	    ata_c->flags));

	if (chp->ch_status & WDCS_DWF)
		ata_c->flags |= AT_DF;
	if (chp->ch_status & WDCS_ERR) {
		ata_c->flags |= AT_ERROR;
		ata_c->r_error = chp->ch_error;
	}
	if ((ata_c->flags & AT_READREG) != 0 &&
	    device_is_active(atac->atac_dev) &&
	    (ata_c->flags & (AT_ERROR | AT_DF)) == 0) {
		ata_c->r_status = MVSATA_WDC_READ_1(mvport, SRB_CS);
		ata_c->r_error = MVSATA_WDC_READ_1(mvport, SRB_FE);
		ata_c->r_count = MVSATA_WDC_READ_1(mvport, SRB_SC);
		ata_c->r_lba =
		    (uint64_t)MVSATA_WDC_READ_1(mvport, SRB_LBAL) << 0;
		ata_c->r_lba |=
		    (uint64_t)MVSATA_WDC_READ_1(mvport, SRB_LBAM) << 8;
		ata_c->r_lba |=
		    (uint64_t)MVSATA_WDC_READ_1(mvport, SRB_LBAH) << 16;
		ata_c->r_device = MVSATA_WDC_READ_1(mvport, SRB_H);
		if ((ata_c->flags & AT_LBA48) != 0) {
			if ((ata_c->flags & AT_POLL) != 0) {
				MVSATA_WDC_WRITE_1(mvport, SRB_CAS,
				    WDCTL_HOB|WDCTL_4BIT|WDCTL_IDS);
			} else {
				MVSATA_WDC_WRITE_1(mvport, SRB_CAS,
				    WDCTL_HOB|WDCTL_4BIT);
			}
			ata_c->r_count |=
			    MVSATA_WDC_READ_1(mvport, SRB_SC) << 8;
			ata_c->r_lba |=
			    (uint64_t)MVSATA_WDC_READ_1(mvport, SRB_LBAL) << 24;
			ata_c->r_lba |=
			    (uint64_t)MVSATA_WDC_READ_1(mvport, SRB_LBAM) << 32;
			ata_c->r_lba |=
			    (uint64_t)MVSATA_WDC_READ_1(mvport, SRB_LBAH) << 40;
			if ((ata_c->flags & AT_POLL) != 0) {
				MVSATA_WDC_WRITE_1(mvport, SRB_CAS,
				    WDCTL_4BIT|WDCTL_IDS);
			} else {
				MVSATA_WDC_WRITE_1(mvport, SRB_CAS,
				    WDCTL_4BIT);
			}
		} else {
			ata_c->r_lba |=
			    (uint64_t)(ata_c->r_device & 0x0f) << 24;
		}
	}
	callout_stop(&chp->ch_callout);
	chp->ch_queue->active_xfer = NULL;
	if (ata_c->flags & AT_POLL) {
		/* enable interrupts */
		MVSATA_WDC_WRITE_1(mvport, SRB_CAS, WDCTL_4BIT);
		delay(10);	/* some drives need a little delay here */
	}
	if (chp->ch_drive[xfer->c_drive].drive_flags & ATA_DRIVE_WAITDRAIN) {
		mvsata_wdc_cmd_kill_xfer(chp, xfer, KILL_GONE);
		chp->ch_drive[xfer->c_drive].drive_flags &= ~ATA_DRIVE_WAITDRAIN;
		wakeup(&chp->ch_queue->active_xfer);
	} else
		mvsata_wdc_cmd_done_end(chp, xfer);
}

static void
mvsata_wdc_cmd_done_end(struct ata_channel *chp, struct ata_xfer *xfer)
{
	struct mvsata_port *mvport = (struct mvsata_port *)chp;
	struct ata_command *ata_c = xfer->c_cmd;

	/* EDMA restart, if enabled */
	if (mvport->port_edmamode != nodma) {
		mvsata_edma_reset_qptr(mvport);
		mvsata_edma_enable(mvport);
	}

	ata_c->flags |= AT_DONE;
	ata_free_xfer(chp, xfer);
	if (ata_c->flags & AT_WAIT)
		wakeup(ata_c);
	else if (ata_c->callback)
		ata_c->callback(ata_c->callback_arg);
	atastart(chp);

	return;
}

#if NATAPIBUS > 0
static void
mvsata_atapi_start(struct ata_channel *chp, struct ata_xfer *xfer)
{
	struct mvsata_softc *sc = (struct mvsata_softc *)chp->ch_atac;
	struct mvsata_port *mvport = (struct mvsata_port *)chp;
	struct atac_softc *atac = &sc->sc_wdcdev.sc_atac;
	struct scsipi_xfer *sc_xfer = xfer->c_cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->c_drive];
	const int wait_flags = (xfer->c_flags & C_POLL) ? AT_POLL : 0;
	const char *errstring;

	DPRINTFN(2, ("%s:%d:%d: mvsata_atapi_start: scsi flags 0x%x\n",
	    device_xname(chp->ch_atac->atac_dev), chp->ch_channel,
	    xfer->c_drive, sc_xfer->xs_control));

	if (mvport->port_edmamode != nodma)
		mvsata_edma_disable(mvport, 10 /* ms */, wait_flags);

	mvsata_pmp_select(mvport, xfer->c_drive);

	if ((xfer->c_flags & C_DMA) && (drvp->n_xfers <= NXFER))
		drvp->n_xfers++;

	/* Do control operations specially. */
	if (__predict_false(drvp->state < READY)) {
		/* If it's not a polled command, we need the kernel thread */
		if ((sc_xfer->xs_control & XS_CTL_POLL) == 0 && cpu_intr_p()) {
			chp->ch_queue->queue_freeze++;
			wakeup(&chp->ch_thread);
			return;
		}
		/*
		 * disable interrupts, all commands here should be quick
		 * enough to be able to poll, and we don't go here that often
		 */
		MVSATA_WDC_WRITE_1(mvport, SRB_CAS, WDCTL_4BIT | WDCTL_IDS);

		MVSATA_WDC_WRITE_1(mvport, SRB_H, WDSD_IBM);
		/* Don't try to set mode if controller can't be adjusted */
		if (atac->atac_set_modes == NULL)
			goto ready;
		/* Also don't try if the drive didn't report its mode */
		if ((drvp->drive_flags & ATA_DRIVE_MODE) == 0)
			goto ready;
		errstring = "unbusy";
		if (wdc_wait_for_unbusy(chp, ATAPI_DELAY, wait_flags))
			goto timeout;
		wdccommand(chp, 0, SET_FEATURES, 0, 0, 0,
		    0x08 | drvp->PIO_mode, WDSF_SET_MODE);
		errstring = "piomode";
		if (wdc_wait_for_unbusy(chp, ATAPI_MODE_DELAY, wait_flags))
			goto timeout;
		if (chp->ch_status & WDCS_ERR) {
			if (chp->ch_error == WDCE_ABRT) {
				/*
				 * Some ATAPI drives reject PIO settings.
				 * Fall back to PIO mode 3 since that's the
				 * minimum for ATAPI.
				 */
				aprint_error_dev(atac->atac_dev,
				    "channel %d drive %d: PIO mode %d rejected,"
				    " falling back to PIO mode 3\n",
				    chp->ch_channel, xfer->c_drive,
				    drvp->PIO_mode);
				if (drvp->PIO_mode > 3)
					drvp->PIO_mode = 3;
			} else
				goto error;
		}
		if (drvp->drive_flags & ATA_DRIVE_UDMA)
			wdccommand(chp, 0, SET_FEATURES, 0, 0, 0,
			    0x40 | drvp->UDMA_mode, WDSF_SET_MODE);
		else
		if (drvp->drive_flags & ATA_DRIVE_DMA)
			wdccommand(chp, 0, SET_FEATURES, 0, 0, 0,
			    0x20 | drvp->DMA_mode, WDSF_SET_MODE);
		else
			goto ready;
		errstring = "dmamode";
		if (wdc_wait_for_unbusy(chp, ATAPI_MODE_DELAY, wait_flags))
			goto timeout;
		if (chp->ch_status & WDCS_ERR) {
			if (chp->ch_error == WDCE_ABRT) {
				if (drvp->drive_flags & ATA_DRIVE_UDMA)
					goto error;
				else {
					/*
					 * The drive rejected our DMA setting.
					 * Fall back to mode 1.
					 */
					aprint_error_dev(atac->atac_dev,
					    "channel %d drive %d:"
					    " DMA mode %d rejected,"
					    " falling back to DMA mode 0\n",
					    chp->ch_channel, xfer->c_drive,
					    drvp->DMA_mode);
					if (drvp->DMA_mode > 0)
						drvp->DMA_mode = 0;
				}
			} else
				goto error;
		}
ready:
		drvp->state = READY;
		MVSATA_WDC_WRITE_1(mvport, SRB_CAS, WDCTL_4BIT);
		delay(10); /* some drives need a little delay here */
	}
	/* start timeout machinery */
	if ((sc_xfer->xs_control & XS_CTL_POLL) == 0)
		callout_reset(&chp->ch_callout, mstohz(sc_xfer->timeout),
		    wdctimeout, chp);

	MVSATA_WDC_WRITE_1(mvport, SRB_H, WDSD_IBM);
	switch (wdc_wait_for_unbusy(chp, ATAPI_DELAY, wait_flags)  < 0) {
	case WDCWAIT_OK:
		break;
	case WDCWAIT_TOUT:
		aprint_error_dev(atac->atac_dev, "not ready, st = %02x\n",
		    chp->ch_status);
		sc_xfer->error = XS_TIMEOUT;
		mvsata_atapi_reset(chp, xfer);
		return;
	case WDCWAIT_THR:
		return;
	}

	/*
	 * Even with WDCS_ERR, the device should accept a command packet
	 * Limit length to what can be stuffed into the cylinder register
	 * (16 bits).  Some CD-ROMs seem to interpret '0' as 65536,
	 * but not all devices do that and it's not obvious from the
	 * ATAPI spec that that behaviour should be expected.  If more
	 * data is necessary, multiple data transfer phases will be done.
	 */

	wdccommand(chp, 0, ATAPI_PKT_CMD,
	    xfer->c_bcount <= 0xffff ? xfer->c_bcount : 0xffff, 0, 0, 0,
	    (xfer->c_flags & C_DMA) ? ATAPI_PKT_CMD_FTRE_DMA : 0);

	/*
	 * If there is no interrupt for CMD input, busy-wait for it (done in
	 * the interrupt routine. If it is a polled command, call the interrupt
	 * routine until command is done.
	 */
	if ((sc_xfer->xs_periph->periph_cap & ATAPI_CFG_DRQ_MASK) !=
	    ATAPI_CFG_IRQ_DRQ || (sc_xfer->xs_control & XS_CTL_POLL)) {
		/* Wait for at last 400ns for status bit to be valid */
		DELAY(1);
		mvsata_atapi_intr(chp, xfer, 0);
	} else
		chp->ch_flags |= ATACH_IRQ_WAIT;
	if (sc_xfer->xs_control & XS_CTL_POLL) {
		if (chp->ch_flags & ATACH_DMA_WAIT) {
			wdc_dmawait(chp, xfer, sc_xfer->timeout);
			chp->ch_flags &= ~ATACH_DMA_WAIT;
		}
		while ((sc_xfer->xs_status & XS_STS_DONE) == 0) {
			/* Wait for at last 400ns for status bit to be valid */
			DELAY(1);
			mvsata_atapi_intr(chp, xfer, 0);
		}
	}
	return;

timeout:
	aprint_error_dev(atac->atac_dev, "channel %d drive %d: %s timed out\n",
	    chp->ch_channel, xfer->c_drive, errstring);
	sc_xfer->error = XS_TIMEOUT;
	MVSATA_WDC_WRITE_1(mvport, SRB_CAS, WDCTL_4BIT);
	delay(10);		/* some drives need a little delay here */
	mvsata_atapi_reset(chp, xfer);
	return;

error:
	aprint_error_dev(atac->atac_dev,
	    "channel %d drive %d: %s error (0x%x)\n",
	    chp->ch_channel, xfer->c_drive, errstring, chp->ch_error);
	sc_xfer->error = XS_SHORTSENSE;
	sc_xfer->sense.atapi_sense = chp->ch_error;
	MVSATA_WDC_WRITE_1(mvport, SRB_CAS, WDCTL_4BIT);
	delay(10);		/* some drives need a little delay here */
	mvsata_atapi_reset(chp, xfer);
	return;
}

static int
mvsata_atapi_intr(struct ata_channel *chp, struct ata_xfer *xfer, int irq)
{
	struct mvsata_port *mvport = (struct mvsata_port *)chp;
	struct atac_softc *atac = chp->ch_atac;
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	struct scsipi_xfer *sc_xfer = xfer->c_cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->c_drive];
	int len, phase, ire, error, retries=0, i;
	void *cmd;

	DPRINTFN(1, ("%s:%d:%d: mvsata_atapi_intr\n",
	    device_xname(atac->atac_dev), chp->ch_channel, xfer->c_drive));

	/* Is it not a transfer, but a control operation? */
	if (drvp->state < READY) {
		aprint_error_dev(atac->atac_dev,
		    "channel %d drive %d: bad state %d\n",
		    chp->ch_channel, xfer->c_drive, drvp->state);
		panic("mvsata_atapi_intr: bad state");
	}
	/*
	 * If we missed an interrupt in a PIO transfer, reset and restart.
	 * Don't try to continue transfer, we may have missed cycles.
	 */
	if ((xfer->c_flags & (C_TIMEOU | C_DMA)) == C_TIMEOU) {
		sc_xfer->error = XS_TIMEOUT;
		mvsata_atapi_reset(chp, xfer);
		return 1;
	}

	/* Ack interrupt done in wdc_wait_for_unbusy */
	MVSATA_WDC_WRITE_1(mvport, SRB_H, WDSD_IBM);
	if (wdc_wait_for_unbusy(chp,
	    (irq == 0) ? sc_xfer->timeout : 0, AT_POLL) == WDCWAIT_TOUT) {
		if (irq && (xfer->c_flags & C_TIMEOU) == 0)
			return 0; /* IRQ was not for us */
		aprint_error_dev(atac->atac_dev,
		    "channel %d: device timeout, c_bcount=%d, c_skip=%d\n",
		    chp->ch_channel, xfer->c_bcount, xfer->c_skip);
		if (xfer->c_flags & C_DMA)
			ata_dmaerr(drvp,
			    (xfer->c_flags & C_POLL) ? AT_POLL : 0);
		sc_xfer->error = XS_TIMEOUT;
		mvsata_atapi_reset(chp, xfer);
		return 1;
	}

	/*
	 * If we missed an IRQ and were using DMA, flag it as a DMA error
	 * and reset device.
	 */
	if ((xfer->c_flags & C_TIMEOU) && (xfer->c_flags & C_DMA)) {
		ata_dmaerr(drvp, (xfer->c_flags & C_POLL) ? AT_POLL : 0);
		sc_xfer->error = XS_RESET;
		mvsata_atapi_reset(chp, xfer);
		return (1);
	}
	/*
	 * if the request sense command was aborted, report the short sense
	 * previously recorded, else continue normal processing
	 */

again:
	len = MVSATA_WDC_READ_1(mvport, SRB_LBAM) +
	    256 * MVSATA_WDC_READ_1(mvport, SRB_LBAH);
	ire = MVSATA_WDC_READ_1(mvport, SRB_SC);
	phase = (ire & (WDCI_CMD | WDCI_IN)) | (chp->ch_status & WDCS_DRQ);
	DPRINTF((
	    "mvsata_atapi_intr: c_bcount %d len %d st 0x%x err 0x%x ire 0x%x :",
	    xfer->c_bcount, len, chp->ch_status, chp->ch_error, ire));

	switch (phase) {
	case PHASE_CMDOUT:
		cmd = sc_xfer->cmd;
		DPRINTF(("PHASE_CMDOUT\n"));
		/* Init the DMA channel if necessary */
		if (xfer->c_flags & C_DMA) {
			error = mvsata_bdma_init(mvport, sc_xfer,
			    (char *)xfer->c_databuf + xfer->c_skip);
			if (error) {
				if (error == EINVAL) {
					/*
					 * We can't do DMA on this transfer
					 * for some reason.  Fall back to PIO.
					 */
					xfer->c_flags &= ~C_DMA;
					error = 0;
				} else {
					sc_xfer->error = XS_DRIVER_STUFFUP;
					break;
				}
			}
		}

		/* send packet command */
		/* Commands are 12 or 16 bytes long. It's 32-bit aligned */
		wdc->dataout_pio(chp, drvp->drive_flags, cmd, sc_xfer->cmdlen);

		/* Start the DMA channel if necessary */
		if (xfer->c_flags & C_DMA) {
			mvsata_bdma_start(mvport);
			chp->ch_flags |= ATACH_DMA_WAIT;
		}

		if ((sc_xfer->xs_control & XS_CTL_POLL) == 0)
			chp->ch_flags |= ATACH_IRQ_WAIT;
		return 1;

	case PHASE_DATAOUT:
		/* write data */
		DPRINTF(("PHASE_DATAOUT\n"));
		if ((sc_xfer->xs_control & XS_CTL_DATA_OUT) == 0 ||
		    (xfer->c_flags & C_DMA) != 0) {
			aprint_error_dev(atac->atac_dev,
			    "channel %d drive %d: bad data phase DATAOUT\n",
			    chp->ch_channel, xfer->c_drive);
			if (xfer->c_flags & C_DMA)
				ata_dmaerr(drvp,
				    (xfer->c_flags & C_POLL) ? AT_POLL : 0);
			sc_xfer->error = XS_TIMEOUT;
			mvsata_atapi_reset(chp, xfer);
			return 1;
		}
		xfer->c_lenoff = len - xfer->c_bcount;
		if (xfer->c_bcount < len) {
			aprint_error_dev(atac->atac_dev, "channel %d drive %d:"
			    " warning: write only %d of %d requested bytes\n",
			    chp->ch_channel, xfer->c_drive, xfer->c_bcount,
			    len);
			len = xfer->c_bcount;
		}

		wdc->dataout_pio(chp, drvp->drive_flags,
		    (char *)xfer->c_databuf + xfer->c_skip, len);

		for (i = xfer->c_lenoff; i > 0; i -= 2)
			MVSATA_WDC_WRITE_2(mvport, SRB_PIOD, 0);

		xfer->c_skip += len;
		xfer->c_bcount -= len;
		if ((sc_xfer->xs_control & XS_CTL_POLL) == 0)
			chp->ch_flags |= ATACH_IRQ_WAIT;
		return 1;

	case PHASE_DATAIN:
		/* Read data */
		DPRINTF(("PHASE_DATAIN\n"));
		if ((sc_xfer->xs_control & XS_CTL_DATA_IN) == 0 ||
		    (xfer->c_flags & C_DMA) != 0) {
			aprint_error_dev(atac->atac_dev,
			    "channel %d drive %d: bad data phase DATAIN\n",
			    chp->ch_channel, xfer->c_drive);
			if (xfer->c_flags & C_DMA)
				ata_dmaerr(drvp,
				    (xfer->c_flags & C_POLL) ? AT_POLL : 0);
			sc_xfer->error = XS_TIMEOUT;
			mvsata_atapi_reset(chp, xfer);
			return 1;
		}
		xfer->c_lenoff = len - xfer->c_bcount;
		if (xfer->c_bcount < len) {
			aprint_error_dev(atac->atac_dev, "channel %d drive %d:"
			    " warning: reading only %d of %d bytes\n",
			    chp->ch_channel, xfer->c_drive, xfer->c_bcount,
			    len);
			len = xfer->c_bcount;
		}

		wdc->datain_pio(chp, drvp->drive_flags,
		    (char *)xfer->c_databuf + xfer->c_skip, len);

		if (xfer->c_lenoff > 0)
			wdcbit_bucket(chp, len - xfer->c_bcount);

		xfer->c_skip += len;
		xfer->c_bcount -= len;
		if ((sc_xfer->xs_control & XS_CTL_POLL) == 0)
			chp->ch_flags |= ATACH_IRQ_WAIT;
		return 1;

	case PHASE_ABORTED:
	case PHASE_COMPLETED:
		DPRINTF(("PHASE_COMPLETED\n"));
		if (xfer->c_flags & C_DMA)
			xfer->c_bcount -= sc_xfer->datalen;
		sc_xfer->resid = xfer->c_bcount;
		mvsata_atapi_phase_complete(xfer);
		return 1;

	default:
		if (++retries<500) {
			DELAY(100);
			chp->ch_status = MVSATA_WDC_READ_1(mvport, SRB_CS);
			chp->ch_error = MVSATA_WDC_READ_1(mvport, SRB_FE);
			goto again;
		}
		aprint_error_dev(atac->atac_dev,
		    "channel %d drive %d: unknown phase 0x%x\n",
		    chp->ch_channel, xfer->c_drive, phase);
		if (chp->ch_status & WDCS_ERR) {
			sc_xfer->error = XS_SHORTSENSE;
			sc_xfer->sense.atapi_sense = chp->ch_error;
		} else {
			if (xfer->c_flags & C_DMA)
				ata_dmaerr(drvp,
				    (xfer->c_flags & C_POLL) ? AT_POLL : 0);
			sc_xfer->error = XS_RESET;
			mvsata_atapi_reset(chp, xfer);
			return (1);
		}
	}
	DPRINTF(("mvsata_atapi_intr: mvsata_atapi_done() (end), error 0x%x "
	    "sense 0x%x\n", sc_xfer->error, sc_xfer->sense.atapi_sense));
	mvsata_atapi_done(chp, xfer);
	return 1;
}

static void
mvsata_atapi_kill_xfer(struct ata_channel *chp, struct ata_xfer *xfer,
		       int reason)
{
	struct mvsata_port *mvport = (struct mvsata_port *)chp;
	struct scsipi_xfer *sc_xfer = xfer->c_cmd;

	/* remove this command from xfer queue */
	switch (reason) {
	case KILL_GONE:
		sc_xfer->error = XS_DRIVER_STUFFUP;
		break;

	case KILL_RESET:
		sc_xfer->error = XS_RESET;
		break;

	default:
		aprint_error_dev(MVSATA_DEV2(mvport),
		    "mvsata_atapi_kill_xfer: unknown reason %d\n", reason);
		panic("mvsata_atapi_kill_xfer");
	}
	ata_free_xfer(chp, xfer);
	scsipi_done(sc_xfer);
}

static void
mvsata_atapi_reset(struct ata_channel *chp, struct ata_xfer *xfer)
{
	struct atac_softc *atac = chp->ch_atac;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->c_drive];
	struct scsipi_xfer *sc_xfer = xfer->c_cmd;

	mvsata_pmp_select(mvport, xfer->c_drive);

	wdccommandshort(chp, 0, ATAPI_SOFT_RESET);
	drvp->state = 0;
	if (wdc_wait_for_unbusy(chp, WDC_RESET_WAIT, AT_POLL) != 0) {
		printf("%s:%d:%d: reset failed\n", device_xname(atac->atac_dev),
		    chp->ch_channel, xfer->c_drive);
		sc_xfer->error = XS_SELTIMEOUT;
	}
	mvsata_atapi_done(chp, xfer);
	return;
}

static void
mvsata_atapi_phase_complete(struct ata_xfer *xfer)
{
	struct ata_channel *chp = xfer->c_chp;
	struct atac_softc *atac = chp->ch_atac;
	struct wdc_softc *wdc = CHAN_TO_WDC(chp);
	struct scsipi_xfer *sc_xfer = xfer->c_cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->c_drive];

	/* wait for DSC if needed */
	if (drvp->drive_flags & ATA_DRIVE_ATAPIDSCW) {
		DPRINTFN(1,
		    ("%s:%d:%d: mvsata_atapi_phase_complete: polldsc %d\n",
		    device_xname(atac->atac_dev), chp->ch_channel,
		    xfer->c_drive, xfer->c_dscpoll));
		if (cold)
			panic("mvsata_atapi_phase_complete: cold");

		if (wdcwait(chp, WDCS_DSC, WDCS_DSC, 10, AT_POLL) ==
		    WDCWAIT_TOUT) {
			/* 10ms not enough, try again in 1 tick */
			if (xfer->c_dscpoll++ > mstohz(sc_xfer->timeout)) {
				aprint_error_dev(atac->atac_dev,
				    "channel %d: wait_for_dsc failed\n",
				    chp->ch_channel);
				sc_xfer->error = XS_TIMEOUT;
				mvsata_atapi_reset(chp, xfer);
				return;
			} else
				callout_reset(&chp->ch_callout, 1,
				    mvsata_atapi_polldsc, xfer);
			return;
		}
	}

	/*
	 * Some drive occasionally set WDCS_ERR with
	 * "ATA illegal length indication" in the error
	 * register. If we read some data the sense is valid
	 * anyway, so don't report the error.
	 */
	if (chp->ch_status & WDCS_ERR &&
	    ((sc_xfer->xs_control & XS_CTL_REQSENSE) == 0 ||
	    sc_xfer->resid == sc_xfer->datalen)) {
		/* save the short sense */
		sc_xfer->error = XS_SHORTSENSE;
		sc_xfer->sense.atapi_sense = chp->ch_error;
		if ((sc_xfer->xs_periph->periph_quirks & PQUIRK_NOSENSE) == 0) {
			/* ask scsipi to send a REQUEST_SENSE */
			sc_xfer->error = XS_BUSY;
			sc_xfer->status = SCSI_CHECK;
		} else
		    if (wdc->dma_status & (WDC_DMAST_NOIRQ | WDC_DMAST_ERR)) {
			ata_dmaerr(drvp,
			    (xfer->c_flags & C_POLL) ? AT_POLL : 0);
			sc_xfer->error = XS_RESET;
			mvsata_atapi_reset(chp, xfer);
			return;
		}
	}
	if (xfer->c_bcount != 0)
		DPRINTFN(1, ("%s:%d:%d: mvsata_atapi_intr:"
		    " bcount value is %d after io\n",
		    device_xname(atac->atac_dev), chp->ch_channel,
		    xfer->c_drive, xfer->c_bcount));
#ifdef DIAGNOSTIC
	if (xfer->c_bcount < 0)
		aprint_error_dev(atac->atac_dev,
		    "channel %d drive %d: mvsata_atapi_intr:"
		    " warning: bcount value is %d after io\n",
		    chp->ch_channel, xfer->c_drive, xfer->c_bcount);
#endif

	DPRINTFN(1, ("%s:%d:%d: mvsata_atapi_phase_complete:"
	    " mvsata_atapi_done(), error 0x%x sense 0x%x\n",
	    device_xname(atac->atac_dev), chp->ch_channel, xfer->c_drive,
	    sc_xfer->error, sc_xfer->sense.atapi_sense));
	mvsata_atapi_done(chp, xfer);
}

static void
mvsata_atapi_done(struct ata_channel *chp, struct ata_xfer *xfer)
{
	struct atac_softc *atac = chp->ch_atac;
	struct scsipi_xfer *sc_xfer = xfer->c_cmd;
	int drive = xfer->c_drive;

	DPRINTFN(1, ("%s:%d:%d: mvsata_atapi_done: flags 0x%x\n",
	    device_xname(atac->atac_dev), chp->ch_channel, xfer->c_drive,
	    (u_int)xfer->c_flags));
	callout_stop(&chp->ch_callout);
	/* mark controller inactive and free the command */
	chp->ch_queue->active_xfer = NULL;
	ata_free_xfer(chp, xfer);

	if (chp->ch_drive[drive].drive_flags & ATA_DRIVE_WAITDRAIN) {
		sc_xfer->error = XS_DRIVER_STUFFUP;
		chp->ch_drive[drive].drive_flags &= ~ATA_DRIVE_WAITDRAIN;
		wakeup(&chp->ch_queue->active_xfer);
	}

	DPRINTFN(1, ("%s:%d: mvsata_atapi_done: scsipi_done\n",
	    device_xname(atac->atac_dev), chp->ch_channel));
	scsipi_done(sc_xfer);
	DPRINTFN(1, ("%s:%d: atastart from wdc_atapi_done, flags 0x%x\n",
	    device_xname(atac->atac_dev), chp->ch_channel, chp->ch_flags));
	atastart(chp);
}

static void
mvsata_atapi_polldsc(void *arg)
{

	mvsata_atapi_phase_complete(arg);
}
#endif	/* NATAPIBUS > 0 */


/*
 * XXXX: Shall we need lock for race condition in mvsata_edma_enqueue{,_gen2}(),
 * if supported queuing command by atabus?  The race condition will not happen
 * if this is called only to the thread of atabus.
 */
static int
mvsata_edma_enqueue(struct mvsata_port *mvport, struct ata_bio *ata_bio,
		    void *databuf)
{
	struct mvsata_softc *sc = device_private(MVSATA_DEV2(mvport));
	struct ata_channel *chp = &mvport->port_ata_channel;
	struct eprd *eprd;
	bus_addr_t crqb_base_addr;
	bus_dmamap_t data_dmamap;
	uint32_t reg;
	int quetag, erqqip, erqqop, next, rv, i;

	DPRINTFN(2, ("%s:%d:%d: mvsata_edma_enqueue:"
	    " blkno=0x%" PRIx64 ", nbytes=%d, flags=0x%x\n",
	    device_xname(MVSATA_DEV2(mvport)), mvport->port_hc->hc,
	    mvport->port, ata_bio->blkno, ata_bio->nbytes, ata_bio->flags));

	reg = MVSATA_EDMA_READ_4(mvport, EDMA_REQQOP);
	erqqop = (reg & EDMA_REQQP_ERQQP_MASK) >> EDMA_REQQP_ERQQP_SHIFT;
	reg = MVSATA_EDMA_READ_4(mvport, EDMA_REQQIP);
	erqqip = (reg & EDMA_REQQP_ERQQP_MASK) >> EDMA_REQQP_ERQQP_SHIFT;
	next = erqqip;
	MVSATA_EDMAQ_INC(next);
	if (next == erqqop)
		/* queue full */
		return EBUSY;
	if ((quetag = mvsata_quetag_get(mvport)) == -1)
		/* tag nothing */
		return EBUSY;
	DPRINTFN(2, ("    erqqip=%d, quetag=%d\n", erqqip, quetag));

	rv = mvsata_dma_bufload(mvport, quetag, databuf, ata_bio->nbytes,
	    ata_bio->flags);
	if (rv != 0)
		return rv;

	KASSERT(mvport->port_reqtbl[quetag].xfer == NULL);
	KASSERT(chp->ch_queue->active_xfer != NULL);
	mvport->port_reqtbl[quetag].xfer = chp->ch_queue->active_xfer;

	/* setup EDMA Physical Region Descriptors (ePRD) Table Data */
	data_dmamap = mvport->port_reqtbl[quetag].data_dmamap;
	eprd = mvport->port_reqtbl[quetag].eprd;
	for (i = 0; i < data_dmamap->dm_nsegs; i++) {
		bus_addr_t ds_addr = data_dmamap->dm_segs[i].ds_addr;
		bus_size_t ds_len = data_dmamap->dm_segs[i].ds_len;

		eprd->prdbal = htole32(ds_addr & EPRD_PRDBAL_MASK);
		eprd->bytecount = htole32(EPRD_BYTECOUNT(ds_len));
		eprd->eot = htole16(0);
		eprd->prdbah = htole32((ds_addr >> 16) >> 16);
		eprd++;
	}
	(eprd - 1)->eot |= htole16(EPRD_EOT);
#ifdef MVSATA_DEBUG
	if (mvsata_debug >= 3)
		mvsata_print_eprd(mvport, quetag);
#endif
	bus_dmamap_sync(mvport->port_dmat, mvport->port_eprd_dmamap,
	    mvport->port_reqtbl[quetag].eprd_offset, MVSATA_EPRD_MAX_SIZE,
	    BUS_DMASYNC_PREWRITE);

	/* setup EDMA Command Request Block (CRQB) Data */
	sc->sc_edma_setup_crqb(mvport, erqqip, quetag, ata_bio);
#ifdef MVSATA_DEBUG
	if (mvsata_debug >= 3)
		mvsata_print_crqb(mvport, erqqip);
#endif
	bus_dmamap_sync(mvport->port_dmat, mvport->port_crqb_dmamap,
	    erqqip * sizeof(union mvsata_crqb),
	    sizeof(union mvsata_crqb), BUS_DMASYNC_PREWRITE);

	MVSATA_EDMAQ_INC(erqqip);

	crqb_base_addr = mvport->port_crqb_dmamap->dm_segs[0].ds_addr &
	    (EDMA_REQQP_ERQQBAP_MASK | EDMA_REQQP_ERQQBA_MASK);
	MVSATA_EDMA_WRITE_4(mvport, EDMA_REQQBAH, (crqb_base_addr >> 16) >> 16);
	MVSATA_EDMA_WRITE_4(mvport, EDMA_REQQIP,
	    crqb_base_addr | (erqqip << EDMA_REQQP_ERQQP_SHIFT));

	return 0;
}

static int
mvsata_edma_handle(struct mvsata_port *mvport, struct ata_xfer *xfer1)
{
	struct ata_channel *chp = &mvport->port_ata_channel;
	struct crpb *crpb;
	struct ata_bio *ata_bio;
	struct ata_xfer *xfer;
	uint32_t reg;
	int erqqip, erqqop, erpqip, erpqop, prev_erpqop, quetag, handled = 0, n;

	/* First, Sync for Request Queue buffer */
	reg = MVSATA_EDMA_READ_4(mvport, EDMA_REQQOP);
	erqqop = (reg & EDMA_REQQP_ERQQP_MASK) >> EDMA_REQQP_ERQQP_SHIFT;
	if (mvport->port_prev_erqqop != erqqop) {
		const int s = sizeof(union mvsata_crqb);

		if (mvport->port_prev_erqqop < erqqop)
			n = erqqop - mvport->port_prev_erqqop;
		else {
			if (erqqop > 0)
				bus_dmamap_sync(mvport->port_dmat,
				    mvport->port_crqb_dmamap, 0, erqqop * s,
				    BUS_DMASYNC_POSTWRITE);
			n = MVSATA_EDMAQ_LEN - mvport->port_prev_erqqop;
		}
		if (n > 0)
			bus_dmamap_sync(mvport->port_dmat,
			    mvport->port_crqb_dmamap,
			    mvport->port_prev_erqqop * s, n * s,
			    BUS_DMASYNC_POSTWRITE);
		mvport->port_prev_erqqop = erqqop;
	}

	reg = MVSATA_EDMA_READ_4(mvport, EDMA_RESQIP);
	erpqip = (reg & EDMA_RESQP_ERPQP_MASK) >> EDMA_RESQP_ERPQP_SHIFT;
	reg = MVSATA_EDMA_READ_4(mvport, EDMA_RESQOP);
	erpqop = (reg & EDMA_RESQP_ERPQP_MASK) >> EDMA_RESQP_ERPQP_SHIFT;

	DPRINTFN(3, ("%s:%d:%d: mvsata_edma_handle: erpqip=%d, erpqop=%d\n",
	    device_xname(MVSATA_DEV2(mvport)), mvport->port_hc->hc,
	    mvport->port, erpqip, erpqop));

	if (erpqop == erpqip)
		return 0;

	if (erpqop < erpqip)
		n = erpqip - erpqop;
	else {
		if (erpqip > 0)
			bus_dmamap_sync(mvport->port_dmat,
			    mvport->port_crpb_dmamap,
			    0, erpqip * sizeof(struct crpb),
			    BUS_DMASYNC_POSTREAD);
		n = MVSATA_EDMAQ_LEN - erpqop;
	}
	if (n > 0)
		bus_dmamap_sync(mvport->port_dmat, mvport->port_crpb_dmamap,
		    erpqop * sizeof(struct crpb),
		    n * sizeof(struct crpb), BUS_DMASYNC_POSTREAD);

	prev_erpqop = erpqop;
	while (erpqop != erpqip) {
#ifdef MVSATA_DEBUG
		if (mvsata_debug >= 3)
			mvsata_print_crpb(mvport, erpqop);
#endif
		crpb = mvport->port_crpb + erpqop;
		quetag = CRPB_CHOSTQUETAG(le16toh(crpb->id));
		KASSERT(chp->ch_queue->active_xfer != NULL);
		xfer = chp->ch_queue->active_xfer;
		KASSERT(xfer == mvport->port_reqtbl[quetag].xfer);
#ifdef DIAGNOSTIC
		if (xfer == NULL)
			panic("unknown response received: %s:%d:%d: tag 0x%x\n",
			    device_xname(MVSATA_DEV2(mvport)),
			    mvport->port_hc->hc, mvport->port, quetag);
#endif

		bus_dmamap_sync(mvport->port_dmat, mvport->port_eprd_dmamap,
		    mvport->port_reqtbl[quetag].eprd_offset,
		    MVSATA_EPRD_MAX_SIZE, BUS_DMASYNC_POSTWRITE);

		chp->ch_status = CRPB_CDEVSTS(le16toh(crpb->rspflg));
		chp->ch_error = CRPB_CEDMASTS(le16toh(crpb->rspflg));
		ata_bio = xfer->c_cmd;
		ata_bio->error = NOERROR;
		ata_bio->r_error = 0;
		if (chp->ch_status & WDCS_ERR)
			ata_bio->error = ERROR;
		if (chp->ch_status & WDCS_BSY)
			ata_bio->error = TIMEOUT;
		if (chp->ch_error)
			ata_bio->error = ERR_DMA;

		mvsata_dma_bufunload(mvport, quetag, ata_bio->flags);
		mvport->port_reqtbl[quetag].xfer = NULL;
		mvsata_quetag_put(mvport, quetag);
		MVSATA_EDMAQ_INC(erpqop);

#if 1	/* XXXX: flags clears here, because necessary the atabus layer. */
		erqqip = (MVSATA_EDMA_READ_4(mvport, EDMA_REQQIP) &
		    EDMA_REQQP_ERQQP_MASK) >> EDMA_REQQP_ERQQP_SHIFT;
		if (erpqop == erqqip)
			chp->ch_flags &= ~(ATACH_DMA_WAIT | ATACH_IRQ_WAIT);
#endif
		mvsata_bio_intr(chp, xfer, 1);
		if (xfer1 == NULL)
			handled++;
		else if (xfer == xfer1) {
			handled = 1;
			break;
		}
	}
	if (prev_erpqop < erpqop)
		n = erpqop - prev_erpqop;
	else {
		if (erpqop > 0)
			bus_dmamap_sync(mvport->port_dmat,
			    mvport->port_crpb_dmamap, 0,
			    erpqop * sizeof(struct crpb), BUS_DMASYNC_PREREAD);
		n = MVSATA_EDMAQ_LEN - prev_erpqop;
	}
	if (n > 0)
		bus_dmamap_sync(mvport->port_dmat, mvport->port_crpb_dmamap,
		    prev_erpqop * sizeof(struct crpb),
		    n * sizeof(struct crpb), BUS_DMASYNC_PREREAD);

	reg &= ~EDMA_RESQP_ERPQP_MASK;
	reg |= (erpqop << EDMA_RESQP_ERPQP_SHIFT);
	MVSATA_EDMA_WRITE_4(mvport, EDMA_RESQOP, reg);

#if 0	/* already cleared ago? */
	erqqip = (MVSATA_EDMA_READ_4(mvport, EDMA_REQQIP) &
	    EDMA_REQQP_ERQQP_MASK) >> EDMA_REQQP_ERQQP_SHIFT;
	if (erpqop == erqqip)
		chp->ch_flags &= ~(ATACH_DMA_WAIT | ATACH_IRQ_WAIT);
#endif

	return handled;
}

static int
mvsata_edma_wait(struct mvsata_port *mvport, struct ata_xfer *xfer, int timeout)
{
	struct ata_bio *ata_bio = xfer->c_cmd;
	int xtime;

	for (xtime = 0;  xtime < timeout / 10; xtime++) {
		if (mvsata_edma_handle(mvport, xfer))
			return 0;
		if (ata_bio->flags & ATA_POLL)
			delay(10000);
		else
			tsleep(&xfer, PRIBIO, "mvsataipl", mstohz(10));
	}

	DPRINTF(("mvsata_edma_wait: timeout: %p\n", xfer));
	mvsata_edma_rqq_remove(mvport, xfer);
	xfer->c_flags |= C_TIMEOU;
	return 1;
}

static void
mvsata_edma_timeout(void *arg)
{
	struct ata_xfer *xfer = (struct ata_xfer *)arg;
	struct ata_channel *chp = xfer->c_chp;
	struct mvsata_port *mvport = (struct mvsata_port *)chp;
	int s;

	s = splbio();
	DPRINTF(("mvsata_edma_timeout: %p\n", xfer));
	if ((chp->ch_flags & ATACH_IRQ_WAIT) != 0) {
		mvsata_edma_rqq_remove(mvport, xfer);
		xfer->c_flags |= C_TIMEOU;
		mvsata_bio_intr(chp, xfer, 1);
	}
	splx(s);
}

static void
mvsata_edma_rqq_remove(struct mvsata_port *mvport, struct ata_xfer *xfer)
{
	struct mvsata_softc *sc = device_private(MVSATA_DEV2(mvport));
	struct ata_bio *ata_bio;
	bus_addr_t crqb_base_addr;
	int erqqip, i;

	/* First, hardware reset, stop EDMA */
	mvsata_hreset_port(mvport);

	/* cleanup completed EDMA safely */
	mvsata_edma_handle(mvport, NULL);

	bus_dmamap_sync(mvport->port_dmat, mvport->port_crqb_dmamap, 0,
	    sizeof(union mvsata_crqb) * MVSATA_EDMAQ_LEN, BUS_DMASYNC_PREWRITE);
	for (i = 0, erqqip = 0; i < MVSATA_EDMAQ_LEN; i++) {
		if (mvport->port_reqtbl[i].xfer == NULL)
			continue;

		ata_bio = mvport->port_reqtbl[i].xfer->c_cmd;
		if (mvport->port_reqtbl[i].xfer == xfer) {
			/* remove xfer from EDMA request queue */
			bus_dmamap_sync(mvport->port_dmat,
			    mvport->port_eprd_dmamap,
			    mvport->port_reqtbl[i].eprd_offset,
			    MVSATA_EPRD_MAX_SIZE, BUS_DMASYNC_POSTWRITE);
			mvsata_dma_bufunload(mvport, i, ata_bio->flags);
			mvport->port_reqtbl[i].xfer = NULL;
			mvsata_quetag_put(mvport, i);
			continue;
		}

		sc->sc_edma_setup_crqb(mvport, erqqip, i, ata_bio);
		erqqip++;
	}
	bus_dmamap_sync(mvport->port_dmat, mvport->port_crqb_dmamap, 0,
	    sizeof(union mvsata_crqb) * MVSATA_EDMAQ_LEN,
	    BUS_DMASYNC_POSTWRITE);

	mvsata_edma_config(mvport, mvport->port_edmamode);
	mvsata_edma_reset_qptr(mvport);
	mvsata_edma_enable(mvport);

	crqb_base_addr = mvport->port_crqb_dmamap->dm_segs[0].ds_addr &
	    (EDMA_REQQP_ERQQBAP_MASK | EDMA_REQQP_ERQQBA_MASK);
	MVSATA_EDMA_WRITE_4(mvport, EDMA_REQQBAH, (crqb_base_addr >> 16) >> 16);
	MVSATA_EDMA_WRITE_4(mvport, EDMA_REQQIP,
	    crqb_base_addr | (erqqip << EDMA_REQQP_ERQQP_SHIFT));
}

#if NATAPIBUS > 0
static int
mvsata_bdma_init(struct mvsata_port *mvport, struct scsipi_xfer *sc_xfer,
		  void *databuf)
{
	struct mvsata_softc *sc = device_private(MVSATA_DEV2(mvport));
	struct eprd *eprd;
	bus_dmamap_t data_dmamap;
	bus_addr_t eprd_addr;
	int quetag, rv;

	DPRINTFN(2,
	    ("%s:%d:%d: mvsata_bdma_init: datalen=%d, xs_control=0x%x\n",
	    device_xname(MVSATA_DEV2(mvport)), mvport->port_hc->hc,
	    mvport->port, sc_xfer->datalen, sc_xfer->xs_control));

	if ((quetag = mvsata_quetag_get(mvport)) == -1)
		/* tag nothing */
		return EBUSY;
	DPRINTFN(2, ("    quetag=%d\n", quetag));

	rv = mvsata_dma_bufload(mvport, quetag, databuf, sc_xfer->datalen,
	    sc_xfer->xs_control & XS_CTL_DATA_IN ? ATA_READ : 0);
	if (rv != 0)
		return rv;

	KASSERT(chp->ch_queue->active_xfer != NULL);
	KASSERT(mvport->port_reqtbl[quetag].xfer == NULL);
	mvport->port_reqtbl[quetag].xfer = chp->ch_queue->active_xfer;

	/* setup EDMA Physical Region Descriptors (ePRD) Table Data */
	data_dmamap = mvport->port_reqtbl[quetag].data_dmamap;
	eprd = mvport->port_reqtbl[quetag].eprd;
	for (i = 0; i < data_dmamap->dm_nsegs; i++) {
		bus_addr_t ds_addr = data_dmamap->dm_segs[i].ds_addr;
		bus_size_t ds_len = data_dmamap->dm_segs[i].ds_len;

		eprd->prdbal = htole32(ds_addr & EPRD_PRDBAL_MASK);
		eprd->bytecount = htole32(EPRD_BYTECOUNT(ds_len));
		eprd->eot = htole16(0);
		eprd->prdbah = htole32((ds_addr >> 16) >> 16);
		eprd++;
	}
	(eprd - 1)->eot |= htole16(EPRD_EOT);
#ifdef MVSATA_DEBUG
	if (mvsata_debug >= 3)
		mvsata_print_eprd(mvport, quetag);
#endif
	bus_dmamap_sync(mvport->port_dmat, mvport->port_eprd_dmamap,
	    mvport->port_reqtbl[quetag].eprd_offset, MVSATA_EPRD_MAX_SIZE,
	    BUS_DMASYNC_PREWRITE);
	eprd_addr = mvport->port_eprd_dmamap->dm_segs[0].ds_addr +
	    mvport->port_reqtbl[quetag].eprd_offset;

	MVSATA_EDMA_WRITE_4(mvport, DMA_DTLBA, eprd_addr & DMA_DTLBA_MASK);
	MVSATA_EDMA_WRITE_4(mvport, DMA_DTHBA, (eprd_addr >> 16) >> 16);

	if (sc_xfer->xs_control & XS_CTL_DATA_IN)
		MVSATA_EDMA_WRITE_4(mvport, DMA_C, DMA_C_READ);
	else
		MVSATA_EDMA_WRITE_4(mvport, DMA_C, 0);

	return 0;
}

static void
mvsata_bdma_start(struct mvsata_port *mvport)
{

#ifdef MVSATA_DEBUG
	if (mvsata_debug >= 3)
		mvsata_print_eprd(mvport, 0);
#endif

	MVSATA_EDMA_WRITE_4(mvport, DMA_C,
	    MVSATA_EDMA_READ_4(mvport, DMA_C) | DMA_C_START);
}
#endif
#endif


static int
mvsata_port_init(struct mvsata_hc *mvhc, int port)
{
	struct mvsata_softc *sc = mvhc->hc_sc;
	struct mvsata_port *mvport;
	struct ata_channel *chp;
	int channel, rv, i;
	const int crqbq_size = sizeof(union mvsata_crqb) * MVSATA_EDMAQ_LEN;
	const int crpbq_size = sizeof(struct crpb) * MVSATA_EDMAQ_LEN;
	const int eprd_buf_size = MVSATA_EPRD_MAX_SIZE * MVSATA_EDMAQ_LEN;

	mvport = malloc(sizeof(struct mvsata_port), M_DEVBUF,
	    M_ZERO | M_NOWAIT);
	if (mvport == NULL) {
		aprint_error("%s:%d: can't allocate memory for port %d\n",
		    device_xname(MVSATA_DEV(sc)), mvhc->hc, port);
		return ENOMEM;
	}

	mvport->port = port;
	mvport->port_hc = mvhc;
	mvport->port_edmamode = nodma;

	rv = bus_space_subregion(mvhc->hc_iot, mvhc->hc_ioh,
	    EDMA_REGISTERS_OFFSET + port * EDMA_REGISTERS_SIZE,
	    EDMA_REGISTERS_SIZE, &mvport->port_ioh);
	if (rv != 0) {
		aprint_error("%s:%d: can't subregion EDMA %d registers\n",
		    device_xname(MVSATA_DEV(sc)), mvhc->hc, port);
		goto fail0;
	}
	mvport->port_iot = mvhc->hc_iot;
	rv = bus_space_subregion(mvport->port_iot, mvport->port_ioh, SATA_SS, 4,
	    &mvport->port_sata_sstatus);
	if (rv != 0) {
		aprint_error("%s:%d:%d: couldn't subregion sstatus regs\n",
		    device_xname(MVSATA_DEV(sc)), mvhc->hc, port);
		goto fail0;
	}
	rv = bus_space_subregion(mvport->port_iot, mvport->port_ioh, SATA_SE, 4,
	    &mvport->port_sata_serror);
	if (rv != 0) {
		aprint_error("%s:%d:%d: couldn't subregion serror regs\n",
		    device_xname(MVSATA_DEV(sc)), mvhc->hc, port);
		goto fail0;
	}
	if (sc->sc_rev == gen1)
		rv = bus_space_subregion(mvhc->hc_iot, mvhc->hc_ioh,
		    SATAHC_I_R02(port), 4, &mvport->port_sata_scontrol);
	else
		rv = bus_space_subregion(mvport->port_iot, mvport->port_ioh,
		    SATA_SC, 4, &mvport->port_sata_scontrol);
	if (rv != 0) {
		aprint_error("%s:%d:%d: couldn't subregion scontrol regs\n",
		    device_xname(MVSATA_DEV(sc)), mvhc->hc, port);
		goto fail0;
	}
	mvport->port_dmat = sc->sc_dmat;
#ifndef MVSATA_WITHOUTDMA
	mvsata_quetag_init(mvport);
#endif
	mvhc->hc_ports[port] = mvport;

	channel = mvhc->hc * sc->sc_port + port;
	chp = &mvport->port_ata_channel;
	chp->ch_channel = channel;
	chp->ch_atac = &sc->sc_wdcdev.sc_atac;
	chp->ch_queue = &mvport->port_ata_queue;
	sc->sc_ata_channels[channel] = chp;

	rv = mvsata_wdc_reg_init(mvport, sc->sc_wdcdev.regs + channel);
	if (rv != 0)
		goto fail0;

	rv = bus_dmamap_create(mvport->port_dmat, crqbq_size, 1, crqbq_size, 0,
	    BUS_DMA_NOWAIT, &mvport->port_crqb_dmamap);
	if (rv != 0) {
		aprint_error(
		    "%s:%d:%d: EDMA CRQB map create failed: error=%d\n",
		    device_xname(MVSATA_DEV(sc)), mvhc->hc, port, rv);
		goto fail0;
	}
	rv = bus_dmamap_create(mvport->port_dmat, crpbq_size, 1, crpbq_size, 0,
	    BUS_DMA_NOWAIT, &mvport->port_crpb_dmamap);
	if (rv != 0) {
		aprint_error(
		    "%s:%d:%d: EDMA CRPB map create failed: error=%d\n",
		    device_xname(MVSATA_DEV(sc)), mvhc->hc, port, rv);
		goto fail1;
	}
	rv = bus_dmamap_create(mvport->port_dmat, eprd_buf_size, 1,
	    eprd_buf_size, 0, BUS_DMA_NOWAIT, &mvport->port_eprd_dmamap);
	if (rv != 0) {
		aprint_error(
		    "%s:%d:%d: EDMA ePRD buffer map create failed: error=%d\n",
		    device_xname(MVSATA_DEV(sc)), mvhc->hc, port, rv);
		goto fail2;
	}
	for (i = 0; i < MVSATA_EDMAQ_LEN; i++) {
		rv = bus_dmamap_create(mvport->port_dmat, MAXPHYS,
		    MAXPHYS / PAGE_SIZE, MAXPHYS, 0, BUS_DMA_NOWAIT,
		    &mvport->port_reqtbl[i].data_dmamap);
		if (rv != 0) {
			aprint_error("%s:%d:%d:"
			    " EDMA data map(%d) create failed: error=%d\n",
			    device_xname(MVSATA_DEV(sc)), mvhc->hc, port, i,
			    rv);
			goto fail3;
		}
	}

	return 0;

fail3:
	for (i--; i >= 0; i--)
		bus_dmamap_destroy(mvport->port_dmat,
		    mvport->port_reqtbl[i].data_dmamap);
	bus_dmamap_destroy(mvport->port_dmat, mvport->port_eprd_dmamap);
fail2:
	bus_dmamap_destroy(mvport->port_dmat, mvport->port_crpb_dmamap);
fail1:
	bus_dmamap_destroy(mvport->port_dmat, mvport->port_crqb_dmamap);
fail0:
	return rv;
}

static int
mvsata_wdc_reg_init(struct mvsata_port *mvport, struct wdc_regs *wdr)
{
	int hc, port, rv, i;

	hc = mvport->port_hc->hc;
	port = mvport->port;

	/* Create subregion for Shadow Registers Map */
	rv = bus_space_subregion(mvport->port_iot, mvport->port_ioh,
	    SHADOW_REG_BLOCK_OFFSET, SHADOW_REG_BLOCK_SIZE, &wdr->cmd_baseioh);
	if (rv != 0) {
		aprint_error("%s:%d:%d: couldn't subregion shadow block regs\n",
		    device_xname(MVSATA_DEV2(mvport)), hc, port);
		return rv;
	}
	wdr->cmd_iot = mvport->port_iot;

	/* Once create subregion for each command registers */
	for (i = 0; i < WDC_NREG; i++) {
		rv = bus_space_subregion(wdr->cmd_iot, wdr->cmd_baseioh,
		    i * 4, sizeof(uint32_t), &wdr->cmd_iohs[i]);
		if (rv != 0) {
			aprint_error("%s:%d:%d: couldn't subregion cmd regs\n",
			    device_xname(MVSATA_DEV2(mvport)), hc, port);
			return rv;
		}
	}
	/* Create subregion for Alternate Status register */
	rv = bus_space_subregion(wdr->cmd_iot, wdr->cmd_baseioh,
	    i * 4, sizeof(uint32_t), &wdr->ctl_ioh);
	if (rv != 0) {
		aprint_error("%s:%d:%d: couldn't subregion cmd regs\n",
		    device_xname(MVSATA_DEV2(mvport)), hc, port);
		return rv;
	}
	wdr->ctl_iot = mvport->port_iot;

	wdc_init_shadow_regs(&mvport->port_ata_channel);

	rv = bus_space_subregion(mvport->port_iot, mvport->port_ioh,
	    SATA_SS, sizeof(uint32_t) * 3, &wdr->sata_baseioh);
	if (rv != 0) {
		aprint_error("%s:%d:%d: couldn't subregion SATA regs\n",
		    device_xname(MVSATA_DEV2(mvport)), hc, port);
		return rv;
	}
	wdr->sata_iot = mvport->port_iot;
	rv = bus_space_subregion(mvport->port_iot, mvport->port_ioh,
	    SATA_SC, sizeof(uint32_t), &wdr->sata_control);
	if (rv != 0) {
		aprint_error("%s:%d:%d: couldn't subregion SControl\n",
		    device_xname(MVSATA_DEV2(mvport)), hc, port);
		return rv;
	}
	rv = bus_space_subregion(mvport->port_iot, mvport->port_ioh,
	    SATA_SS, sizeof(uint32_t), &wdr->sata_status);
	if (rv != 0) {
		aprint_error("%s:%d:%d: couldn't subregion SStatus\n",
		    device_xname(MVSATA_DEV2(mvport)), hc, port);
		return rv;
	}
	rv = bus_space_subregion(mvport->port_iot, mvport->port_ioh,
	    SATA_SE, sizeof(uint32_t), &wdr->sata_error);
	if (rv != 0) {
		aprint_error("%s:%d:%d: couldn't subregion SError\n",
		    device_xname(MVSATA_DEV2(mvport)), hc, port);
		return rv;
	}

	return 0;
}


#ifndef MVSATA_WITHOUTDMA
/*
 * There are functions to determine Host Queue Tag.
 * XXXX: We hope to rotate Tag to facilitate debugging.
 */

static inline void
mvsata_quetag_init(struct mvsata_port *mvport)
{

	mvport->port_quetagidx = 0;
}

static inline int
mvsata_quetag_get(struct mvsata_port *mvport)
{
	int begin = mvport->port_quetagidx;

	do {
		if (mvport->port_reqtbl[mvport->port_quetagidx].xfer == NULL) {
			MVSATA_EDMAQ_INC(mvport->port_quetagidx);
			return mvport->port_quetagidx;
		}
		MVSATA_EDMAQ_INC(mvport->port_quetagidx);
	} while (mvport->port_quetagidx != begin);

	return -1;
}

static inline void
mvsata_quetag_put(struct mvsata_port *mvport, int quetag)
{

	/* nothing */
}

static void *
mvsata_edma_resource_prepare(struct mvsata_port *mvport, bus_dma_tag_t dmat,
			     bus_dmamap_t *dmamap, size_t size, int write)
{
	bus_dma_segment_t seg;
	int nseg, rv;
	void *kva;

	rv = bus_dmamem_alloc(dmat, size, PAGE_SIZE, 0, &seg, 1, &nseg,
	    BUS_DMA_NOWAIT);
	if (rv != 0) {
		aprint_error("%s:%d:%d: DMA memory alloc failed: error=%d\n",
		    device_xname(MVSATA_DEV2(mvport)),
		    mvport->port_hc->hc, mvport->port, rv);
		goto fail;
	}

	rv = bus_dmamem_map(dmat, &seg, nseg, size, &kva, BUS_DMA_NOWAIT);
	if (rv != 0) {
		aprint_error("%s:%d:%d: DMA memory map failed: error=%d\n",
		    device_xname(MVSATA_DEV2(mvport)),
		    mvport->port_hc->hc, mvport->port, rv);
		goto free;
	}

	rv = bus_dmamap_load(dmat, *dmamap, kva, size, NULL,
	    BUS_DMA_NOWAIT | (write ? BUS_DMA_WRITE : BUS_DMA_READ));
	if (rv != 0) {
		aprint_error("%s:%d:%d: DMA map load failed: error=%d\n",
		    device_xname(MVSATA_DEV2(mvport)),
		    mvport->port_hc->hc, mvport->port, rv);
		goto unmap;
	}

	if (!write)
		bus_dmamap_sync(dmat, *dmamap, 0, size, BUS_DMASYNC_PREREAD);

	return kva;

unmap:
	bus_dmamem_unmap(dmat, kva, size);
free:
	bus_dmamem_free(dmat, &seg, nseg);
fail:
	return NULL;
}

/* ARGSUSED */
static void
mvsata_edma_resource_purge(struct mvsata_port *mvport, bus_dma_tag_t dmat,
			   bus_dmamap_t dmamap, void *kva)
{

	bus_dmamap_unload(dmat, dmamap);
	bus_dmamem_unmap(dmat, kva, dmamap->dm_mapsize);
	bus_dmamem_free(dmat, dmamap->dm_segs, dmamap->dm_nsegs);
}

static int
mvsata_dma_bufload(struct mvsata_port *mvport, int index, void *databuf,
		   size_t datalen, int flags)
{
	int rv, lop, sop;
	bus_dmamap_t data_dmamap = mvport->port_reqtbl[index].data_dmamap;

	lop = (flags & ATA_READ) ? BUS_DMA_READ : BUS_DMA_WRITE;
	sop = (flags & ATA_READ) ? BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE;

	rv = bus_dmamap_load(mvport->port_dmat, data_dmamap, databuf, datalen,
	    NULL, BUS_DMA_NOWAIT | lop);
	if (rv) {
		aprint_error("%s:%d:%d: buffer load failed: error=%d",
		    device_xname(MVSATA_DEV2(mvport)), mvport->port_hc->hc,
		    mvport->port, rv);
		return rv;
	}
	bus_dmamap_sync(mvport->port_dmat, data_dmamap, 0,
	    data_dmamap->dm_mapsize, sop);

	return 0;
}

static inline void
mvsata_dma_bufunload(struct mvsata_port *mvport, int index, int flags)
{
	bus_dmamap_t data_dmamap = mvport->port_reqtbl[index].data_dmamap;

	bus_dmamap_sync(mvport->port_dmat, data_dmamap, 0,
	    data_dmamap->dm_mapsize,
	    (flags & ATA_READ) ? BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(mvport->port_dmat, data_dmamap);
}
#endif

static void
mvsata_hreset_port(struct mvsata_port *mvport)
{
	struct mvsata_softc *sc = device_private(MVSATA_DEV2(mvport));

	MVSATA_EDMA_WRITE_4(mvport, EDMA_CMD, EDMA_CMD_EATARST);

	delay(25);		/* allow reset propagation */

	MVSATA_EDMA_WRITE_4(mvport, EDMA_CMD, 0);

	mvport->_fix_phy_param._fix_phy(mvport);

	if (sc->sc_gen == gen1)
		delay(1000);
}

static void
mvsata_reset_port(struct mvsata_port *mvport)
{
	device_t parent = device_parent(MVSATA_DEV2(mvport));

	MVSATA_EDMA_WRITE_4(mvport, EDMA_CMD, EDMA_CMD_EDSEDMA);

	mvsata_hreset_port(mvport);

	if (device_is_a(parent, "pci"))
		MVSATA_EDMA_WRITE_4(mvport, EDMA_CFG,
		    EDMA_CFG_RESERVED | EDMA_CFG_ERDBSZ);
	else	/* SoC */
		MVSATA_EDMA_WRITE_4(mvport, EDMA_CFG,
		    EDMA_CFG_RESERVED | EDMA_CFG_RESERVED2);
	MVSATA_EDMA_WRITE_4(mvport, EDMA_T, 0);
	MVSATA_EDMA_WRITE_4(mvport, SATA_SEIM, 0x019c0000);
	MVSATA_EDMA_WRITE_4(mvport, SATA_SE, ~0);
	MVSATA_EDMA_WRITE_4(mvport, SATA_FISIC, 0);
	MVSATA_EDMA_WRITE_4(mvport, EDMA_IEC, 0);
	MVSATA_EDMA_WRITE_4(mvport, EDMA_IEM, 0);
	MVSATA_EDMA_WRITE_4(mvport, EDMA_REQQBAH, 0);
	MVSATA_EDMA_WRITE_4(mvport, EDMA_REQQIP, 0);
	MVSATA_EDMA_WRITE_4(mvport, EDMA_REQQOP, 0);
	MVSATA_EDMA_WRITE_4(mvport, EDMA_RESQBAH, 0);
	MVSATA_EDMA_WRITE_4(mvport, EDMA_RESQIP, 0);
	MVSATA_EDMA_WRITE_4(mvport, EDMA_RESQOP, 0);
	MVSATA_EDMA_WRITE_4(mvport, EDMA_CMD, 0);
	MVSATA_EDMA_WRITE_4(mvport, EDMA_TC, 0);
	MVSATA_EDMA_WRITE_4(mvport, EDMA_IORT, 0xbc);
}

static void
mvsata_reset_hc(struct mvsata_hc *mvhc)
{
#if 0
	uint32_t val;
#endif

	MVSATA_HC_WRITE_4(mvhc, SATAHC_ICT, 0);
	MVSATA_HC_WRITE_4(mvhc, SATAHC_ITT, 0);
	MVSATA_HC_WRITE_4(mvhc, SATAHC_IC, 0);

#if 0	/* XXXX needs? */
	MVSATA_HC_WRITE_4(mvhc, 0x01c, 0);

	/*
	 * Keep the SS during power on and the reference clock bits (reset
	 * sample)
	 */
	val = MVSATA_HC_READ_4(mvhc, 0x020);
	val &= 0x1c1c1c1c;
	val |= 0x03030303;
	MVSATA_HC_READ_4(mvhc, 0x020, 0);
#endif
}

#define WDCDELAY  100 /* 100 microseconds */
#define WDCNDELAY_RST (WDC_RESET_WAIT * 1000 / WDCDELAY)

static uint32_t
mvsata_softreset(struct mvsata_port *mvport, int waitok)
{
	uint32_t sig0 = ~0;
	int timeout, nloop;
	uint8_t st0;

	MVSATA_WDC_WRITE_1(mvport, SRB_CAS, WDCTL_RST | WDCTL_IDS | WDCTL_4BIT);
	delay(10);
	(void) MVSATA_WDC_READ_1(mvport, SRB_FE);
	MVSATA_WDC_WRITE_1(mvport, SRB_CAS, WDCTL_IDS | WDCTL_4BIT);
	delay(10);

	if (!waitok)
		nloop = WDCNDELAY_RST;
	else
		nloop = WDC_RESET_WAIT * hz / 1000;

	/* wait for BSY to deassert */
	for (timeout = 0; timeout < nloop; timeout++) {
		st0 = MVSATA_WDC_READ_1(mvport, SRB_CS);

		if ((st0 & WDCS_BSY) == 0) {
			sig0 = MVSATA_WDC_READ_1(mvport, SRB_SC) << 0;
			sig0 |= MVSATA_WDC_READ_1(mvport, SRB_LBAL) << 8;
			sig0 |= MVSATA_WDC_READ_1(mvport, SRB_LBAM) << 16;
			sig0 |= MVSATA_WDC_READ_1(mvport, SRB_LBAH) << 24;
			goto out;
		}
		if (!waitok)
			delay(WDCDELAY);
		else
			tsleep(&nloop, PRIBIO, "atarst", 1);
	}
	
out:
	MVSATA_WDC_WRITE_1(mvport, SRB_CAS, WDCTL_4BIT);
	return sig0;
}

#ifndef MVSATA_WITHOUTDMA
static void
mvsata_edma_reset_qptr(struct mvsata_port *mvport)
{
	const bus_addr_t crpb_addr =
	    mvport->port_crpb_dmamap->dm_segs[0].ds_addr;
	const uint32_t crpb_addr_mask =
	    EDMA_RESQP_ERPQBAP_MASK | EDMA_RESQP_ERPQBA_MASK;

	MVSATA_EDMA_WRITE_4(mvport, EDMA_REQQBAH, 0);
	MVSATA_EDMA_WRITE_4(mvport, EDMA_REQQIP, 0);
	MVSATA_EDMA_WRITE_4(mvport, EDMA_REQQOP, 0);
	MVSATA_EDMA_WRITE_4(mvport, EDMA_RESQBAH, (crpb_addr >> 16) >> 16);
	MVSATA_EDMA_WRITE_4(mvport, EDMA_RESQIP, 0);
	MVSATA_EDMA_WRITE_4(mvport, EDMA_RESQOP, (crpb_addr & crpb_addr_mask));
}

static inline void
mvsata_edma_enable(struct mvsata_port *mvport)
{

	MVSATA_EDMA_WRITE_4(mvport, EDMA_CMD, EDMA_CMD_EENEDMA);
}

static int
mvsata_edma_disable(struct mvsata_port *mvport, int timeout, int waitok)
{
	uint32_t status, command;
	int ms;

	if (MVSATA_EDMA_READ_4(mvport, EDMA_CMD) & EDMA_CMD_EENEDMA) {
		for (ms = 0; ms < timeout; ms++) {
			status = MVSATA_EDMA_READ_4(mvport, EDMA_S);
			if (status & EDMA_S_EDMAIDLE)
				break;
			if (waitok)
				tsleep(&waitok, PRIBIO, "mvsata_edma1",
				    mstohz(1));
			else
				delay(1000);
		}
		if (ms == timeout) {
			aprint_error("%s:%d:%d: unable to stop EDMA\n",
			    device_xname(MVSATA_DEV2(mvport)),
			    mvport->port_hc->hc, mvport->port);
			return EBUSY;
		}

		/* The diable bit (eDsEDMA) is self negated. */
		MVSATA_EDMA_WRITE_4(mvport, EDMA_CMD, EDMA_CMD_EDSEDMA);

		for ( ; ms < timeout; ms++) {
			command = MVSATA_EDMA_READ_4(mvport, EDMA_CMD);
			if (!(command & EDMA_CMD_EENEDMA))
				break;
			if (waitok)
				tsleep(&waitok, PRIBIO, "mvsata_edma2",
				    mstohz(1));
			else
				delay(1000);
		}
		if (ms == timeout) {
			aprint_error("%s:%d:%d: unable to stop EDMA\n",
			    device_xname(MVSATA_DEV2(mvport)),
			    mvport->port_hc->hc, mvport->port);
			return EBUSY;
		}
	}
	return 0;
}

/*
 * Set EDMA registers according to mode.
 *       ex. NCQ/TCQ(queued)/non queued.
 */
static void
mvsata_edma_config(struct mvsata_port *mvport, int mode)
{
	struct mvsata_softc *sc = device_private(MVSATA_DEV2(mvport));
	uint32_t reg;

	reg = MVSATA_EDMA_READ_4(mvport, EDMA_CFG);
	reg |= EDMA_CFG_RESERVED;

	if (mode == ncq) {
		if (sc->sc_gen == gen1) {
			aprint_error_dev(MVSATA_DEV2(mvport),
			    "GenI not support NCQ\n");
			return;
		} else if (sc->sc_gen == gen2)
			reg |= EDMA_CFG_EDEVERR;
		reg |= EDMA_CFG_ESATANATVCMDQUE;
	} else if (mode == queued) {
		reg &= ~EDMA_CFG_ESATANATVCMDQUE;
		reg |= EDMA_CFG_EQUE;
	} else
		reg &= ~(EDMA_CFG_ESATANATVCMDQUE | EDMA_CFG_EQUE);

	if (sc->sc_gen == gen1)
		reg |= EDMA_CFG_ERDBSZ;
	else if (sc->sc_gen == gen2)
		reg |= (EDMA_CFG_ERDBSZEXT | EDMA_CFG_EWRBUFFERLEN);
	else if (sc->sc_gen == gen2e) {
		device_t parent = device_parent(MVSATA_DEV(sc));

		reg |= (EDMA_CFG_EMASKRXPM | EDMA_CFG_EHOSTQUEUECACHEEN);
		reg &= ~(EDMA_CFG_EEDMAFBS | EDMA_CFG_EEDMAQUELEN);

		if (device_is_a(parent, "pci"))
			reg |= (
#if NATAPIBUS > 0
			    EDMA_CFG_EEARLYCOMPLETIONEN |
#endif
			    EDMA_CFG_ECUTTHROUGHEN |
			    EDMA_CFG_EWRBUFFERLEN |
			    EDMA_CFG_ERDBSZEXT);
	}
	MVSATA_EDMA_WRITE_4(mvport, EDMA_CFG, reg);

	reg = (
	    EDMA_IE_EIORDYERR |
	    EDMA_IE_ETRANSINT |
	    EDMA_IE_EDEVCON |
	    EDMA_IE_EDEVDIS);
	if (sc->sc_gen != gen1)
		reg |= (
		    EDMA_IE_TRANSPROTERR |
		    EDMA_IE_LINKDATATXERR(EDMA_IE_LINKTXERR_FISTXABORTED) |
		    EDMA_IE_LINKDATATXERR(EDMA_IE_LINKXERR_OTHERERRORS) |
		    EDMA_IE_LINKDATATXERR(EDMA_IE_LINKXERR_LINKLAYERRESET) |
		    EDMA_IE_LINKDATATXERR(EDMA_IE_LINKXERR_INTERNALFIFO) |
		    EDMA_IE_LINKDATATXERR(EDMA_IE_LINKXERR_SATACRC) |
		    EDMA_IE_LINKCTLTXERR(EDMA_IE_LINKXERR_OTHERERRORS) |
		    EDMA_IE_LINKCTLTXERR(EDMA_IE_LINKXERR_LINKLAYERRESET) |
		    EDMA_IE_LINKCTLTXERR(EDMA_IE_LINKXERR_INTERNALFIFO) |
		    EDMA_IE_LINKDATARXERR(EDMA_IE_LINKXERR_OTHERERRORS) |
		    EDMA_IE_LINKDATARXERR(EDMA_IE_LINKXERR_LINKLAYERRESET) |
		    EDMA_IE_LINKDATARXERR(EDMA_IE_LINKXERR_INTERNALFIFO) |
		    EDMA_IE_LINKDATARXERR(EDMA_IE_LINKXERR_SATACRC) |
		    EDMA_IE_LINKCTLRXERR(EDMA_IE_LINKXERR_OTHERERRORS) |
		    EDMA_IE_LINKCTLRXERR(EDMA_IE_LINKXERR_LINKLAYERRESET) |
		    EDMA_IE_LINKCTLRXERR(EDMA_IE_LINKXERR_INTERNALFIFO) |
		    EDMA_IE_LINKCTLRXERR(EDMA_IE_LINKXERR_SATACRC) |
		    EDMA_IE_ESELFDIS);

	if (mode == ncq)
	    reg |= EDMA_IE_EDEVERR;
	MVSATA_EDMA_WRITE_4(mvport, EDMA_IEM, reg);
	reg = MVSATA_EDMA_READ_4(mvport, EDMA_HC);
	reg &= ~EDMA_IE_EDEVERR;
	if (mode != ncq)
	    reg |= EDMA_IE_EDEVERR;
	MVSATA_EDMA_WRITE_4(mvport, EDMA_HC, reg);
	if (sc->sc_gen == gen2e) {
		/*
		 * Clear FISWait4HostRdyEn[0] and [2].
		 *   [0]: Device to Host FIS with <ERR> or <DF> bit set to 1.
		 *   [2]: SDB FIS is received with <ERR> bit set to 1.
		 */
		reg = MVSATA_EDMA_READ_4(mvport, SATA_FISC);
		reg &= ~(SATA_FISC_FISWAIT4HOSTRDYEN_B0 |
		    SATA_FISC_FISWAIT4HOSTRDYEN_B2);
		MVSATA_EDMA_WRITE_4(mvport, SATA_FISC, reg);
	}

	mvport->port_edmamode = mode;
}


/*
 * Generation dependent functions
 */

static void
mvsata_edma_setup_crqb(struct mvsata_port *mvport, int erqqip, int quetag,
		       struct ata_bio  *ata_bio)
{
	struct crqb *crqb;
	bus_addr_t eprd_addr;
	daddr_t blkno;
	uint32_t rw;
	uint8_t cmd, head;
	int i;
	const int drive =
	    mvport->port_ata_channel.ch_queue->active_xfer->c_drive;

	eprd_addr = mvport->port_eprd_dmamap->dm_segs[0].ds_addr +
	    mvport->port_reqtbl[quetag].eprd_offset;
	rw = (ata_bio->flags & ATA_READ) ? CRQB_CDIR_READ : CRQB_CDIR_WRITE;
	cmd = (ata_bio->flags & ATA_READ) ? WDCC_READDMA : WDCC_WRITEDMA;
	if (ata_bio->flags & (ATA_LBA|ATA_LBA48)) {
		head = WDSD_LBA;
	} else {
		head = 0;
	}
	blkno = ata_bio->blkno;
	if (ata_bio->flags & ATA_LBA48)
		cmd = atacmd_to48(cmd);
	else {
		head |= ((ata_bio->blkno >> 24) & 0xf);
		blkno &= 0xffffff;
	}
	crqb = &mvport->port_crqb->crqb + erqqip;
	crqb->cprdbl = htole32(eprd_addr & CRQB_CRQBL_EPRD_MASK);
	crqb->cprdbh = htole32((eprd_addr >> 16) >> 16);
	crqb->ctrlflg =
	    htole16(rw | CRQB_CHOSTQUETAG(quetag) | CRQB_CPMPORT(drive));
	i = 0;
	if (mvport->port_edmamode == dma) {
		if (ata_bio->flags & ATA_LBA48)
			crqb->atacommand[i++] = htole16(CRQB_ATACOMMAND(
			    CRQB_ATACOMMAND_SECTORCOUNT, ata_bio->nblks >> 8));
		crqb->atacommand[i++] = htole16(CRQB_ATACOMMAND(
		    CRQB_ATACOMMAND_SECTORCOUNT, ata_bio->nblks));
	} else { /* ncq/queued */

		/*
		 * XXXX: Oops, ata command is not correct.  And, atabus layer
		 * has not been supported yet now.
		 *   Queued DMA read/write.
		 *   read/write FPDMAQueued.
		 */

		if (ata_bio->flags & ATA_LBA48)
			crqb->atacommand[i++] = htole16(CRQB_ATACOMMAND(
			    CRQB_ATACOMMAND_FEATURES, ata_bio->nblks >> 8));
		crqb->atacommand[i++] = htole16(CRQB_ATACOMMAND(
		    CRQB_ATACOMMAND_FEATURES, ata_bio->nblks));
		crqb->atacommand[i++] = htole16(CRQB_ATACOMMAND(
		    CRQB_ATACOMMAND_SECTORCOUNT, quetag << 3));
	}
	if (ata_bio->flags & ATA_LBA48) {
		crqb->atacommand[i++] = htole16(CRQB_ATACOMMAND(
		    CRQB_ATACOMMAND_LBALOW, blkno >> 24));
		crqb->atacommand[i++] = htole16(CRQB_ATACOMMAND(
		    CRQB_ATACOMMAND_LBAMID, blkno >> 32));
		crqb->atacommand[i++] = htole16(CRQB_ATACOMMAND(
		    CRQB_ATACOMMAND_LBAHIGH, blkno >> 40));
	}
	crqb->atacommand[i++] =
	    htole16(CRQB_ATACOMMAND(CRQB_ATACOMMAND_LBALOW, blkno));
	crqb->atacommand[i++] =
	    htole16(CRQB_ATACOMMAND(CRQB_ATACOMMAND_LBAMID, blkno >> 8));
	crqb->atacommand[i++] =
	    htole16(CRQB_ATACOMMAND(CRQB_ATACOMMAND_LBAHIGH, blkno >> 16));
	crqb->atacommand[i++] =
	    htole16(CRQB_ATACOMMAND(CRQB_ATACOMMAND_DEVICE, head));
	crqb->atacommand[i++] = htole16(
	    CRQB_ATACOMMAND(CRQB_ATACOMMAND_COMMAND, cmd) |
	    CRQB_ATACOMMAND_LAST);
}
#endif

static uint32_t
mvsata_read_preamps_gen1(struct mvsata_port *mvport)
{
	struct mvsata_hc *hc = mvport->port_hc;
	uint32_t reg;

	reg = MVSATA_HC_READ_4(hc, SATAHC_I_PHYMODE(mvport->port));
	/*
	 * [12:11] : pre
	 * [7:5]   : amps
	 */
	return reg & 0x000018e0;
}

static void
mvsata_fix_phy_gen1(struct mvsata_port *mvport)
{
	struct mvsata_softc *sc = device_private(MVSATA_DEV2(mvport));
	struct mvsata_hc *mvhc = mvport->port_hc;
	uint32_t reg;
	int port = mvport->port, fix_apm_sq = 0;

	if (sc->sc_model == PCI_PRODUCT_MARVELL_88SX5080) {
		if (sc->sc_rev == 0x01)
			fix_apm_sq = 1;
	} else {
		if (sc->sc_rev == 0x00)
			fix_apm_sq = 1;
	}

	if (fix_apm_sq) {
		/*
		 * Disable auto-power management
		 *   88SX50xx FEr SATA#12
		 */
		reg = MVSATA_HC_READ_4(mvhc, SATAHC_I_LTMODE(port));
		reg |= (1 << 19);
		MVSATA_HC_WRITE_4(mvhc, SATAHC_I_LTMODE(port), reg);

		/*
		 * Fix squelch threshold
		 *   88SX50xx FEr SATA#9
		 */
		reg = MVSATA_HC_READ_4(mvhc, SATAHC_I_PHYCONTROL(port));
		reg &= ~0x3;
		reg |= 0x1;
		MVSATA_HC_WRITE_4(mvhc, SATAHC_I_PHYCONTROL(port), reg);
	}

	/* Revert values of pre-emphasis and signal amps to the saved ones */
	reg = MVSATA_HC_READ_4(mvhc, SATAHC_I_PHYMODE(port));
	reg &= ~0x000018e0;	/* pre and amps mask */
	reg |= mvport->_fix_phy_param.pre_amps;
	MVSATA_HC_WRITE_4(mvhc, SATAHC_I_PHYMODE(port), reg);
}

static void
mvsata_devconn_gen1(struct mvsata_port *mvport)
{
	struct mvsata_softc *sc = device_private(MVSATA_DEV2(mvport));

	/* Fix for 88SX50xx FEr SATA#2 */
	mvport->_fix_phy_param._fix_phy(mvport);

	/* If disk is connected, then enable the activity LED */
	if (sc->sc_rev == 0x03) {
		/* XXXXX */
	}
}

static uint32_t
mvsata_read_preamps_gen2(struct mvsata_port *mvport)
{
	uint32_t reg;

	reg = MVSATA_EDMA_READ_4(mvport, SATA_PHYM2);
	/*
	 * [10:8] : amps
	 * [7:5]  : pre
	 */
	return reg & 0x000007e0;
}

static void
mvsata_fix_phy_gen2(struct mvsata_port *mvport)
{
	struct mvsata_softc *sc = device_private(MVSATA_DEV2(mvport));
	uint32_t reg;

	if ((sc->sc_gen == gen2 && sc->sc_rev == 0x07) ||
	    sc->sc_gen == gen2e) {
		/*
		 * Fix for
		 *   88SX60X1 FEr SATA #23
		 *   88SX6042/88SX7042 FEr SATA #23
		 *   88F5182 FEr #SATA-S13
		 *   88F5082 FEr #SATA-S13
		 */
		reg = MVSATA_EDMA_READ_4(mvport, SATA_PHYM2);
		reg &= ~(1 << 16);
		reg |= (1 << 31);
		MVSATA_EDMA_WRITE_4(mvport, SATA_PHYM2, reg);

		delay(200);

		reg = MVSATA_EDMA_READ_4(mvport, SATA_PHYM2);
		reg &= ~((1 << 16) | (1 << 31));
		MVSATA_EDMA_WRITE_4(mvport, SATA_PHYM2, reg);

		delay(200);
	}

	/* Fix values in PHY Mode 3 Register.*/
	reg = MVSATA_EDMA_READ_4(mvport, SATA_PHYM3);
	reg &= ~0x7F900000;
	reg |= 0x2A800000;
	/* Implement Guidline 88F5182, 88F5082, 88F6082 (GL# SATA-S11) */
	if (sc->sc_model == PCI_PRODUCT_MARVELL_88F5082 ||
	    sc->sc_model == PCI_PRODUCT_MARVELL_88F5182 ||
	    sc->sc_model == PCI_PRODUCT_MARVELL_88F6082)
		reg &= ~0x0000001c;
	MVSATA_EDMA_WRITE_4(mvport, SATA_PHYM3, reg);

	/*
	 * Fix values in PHY Mode 4 Register.
	 *   88SX60x1 FEr SATA#10
	 *   88F5182 GL #SATA-S10
	 *   88F5082 GL #SATA-S10
	 */
	if ((sc->sc_gen == gen2 && sc->sc_rev == 0x07) ||
	    sc->sc_gen == gen2e) {
		uint32_t tmp = 0;

		/* 88SX60x1 FEr SATA #13 */
		if (sc->sc_gen == 2 && sc->sc_rev == 0x07)
			tmp = MVSATA_EDMA_READ_4(mvport, SATA_PHYM3);

		reg = MVSATA_EDMA_READ_4(mvport, SATA_PHYM4);
		reg |= (1 << 0);
		reg &= ~(1 << 1);
		/* PHY Mode 4 Register of Gen IIE has some restriction */
		if (sc->sc_gen == gen2e) {
			reg &= ~0x5de3fffc;
			reg |= (1 << 2);
		}
		MVSATA_EDMA_WRITE_4(mvport, SATA_PHYM4, reg);

		/* 88SX60x1 FEr SATA #13 */
		if (sc->sc_gen == 2 && sc->sc_rev == 0x07)
			MVSATA_EDMA_WRITE_4(mvport, SATA_PHYM3, tmp);
	}

	/* Revert values of pre-emphasis and signal amps to the saved ones */
	reg = MVSATA_EDMA_READ_4(mvport, SATA_PHYM2);
	reg &= ~0x000007e0;	/* pre and amps mask */
	reg |= mvport->_fix_phy_param.pre_amps;
	reg &= ~(1 << 16);
	if (sc->sc_gen == gen2e) {
		/*
		 * according to mvSata 3.6.1, some IIE values are fixed.
		 * some reserved fields must be written with fixed values.
		 */
		reg &= ~0xC30FF01F;
		reg |= 0x0000900F;
	}
	MVSATA_EDMA_WRITE_4(mvport, SATA_PHYM2, reg);
}

#ifndef MVSATA_WITHOUTDMA
static void
mvsata_edma_setup_crqb_gen2e(struct mvsata_port *mvport, int erqqip, int quetag,
			     struct ata_bio  *ata_bio)
{
	struct crqb_gen2e *crqb;
	bus_addr_t eprd_addr;
	daddr_t blkno;
	uint32_t ctrlflg, rw;
	uint8_t cmd, head;
	const int drive =
	    mvport->port_ata_channel.ch_queue->active_xfer->c_drive;

	eprd_addr = mvport->port_eprd_dmamap->dm_segs[0].ds_addr +
	    mvport->port_reqtbl[quetag].eprd_offset;
	rw = (ata_bio->flags & ATA_READ) ? CRQB_CDIR_READ : CRQB_CDIR_WRITE;
	ctrlflg = (rw | CRQB_CDEVICEQUETAG(0) | CRQB_CPMPORT(drive) |
	    CRQB_CPRDMODE_EPRD | CRQB_CHOSTQUETAG_GEN2(quetag));
	cmd = (ata_bio->flags & ATA_READ) ? WDCC_READDMA : WDCC_WRITEDMA;
	if (ata_bio->flags & (ATA_LBA|ATA_LBA48)) {
		head = WDSD_LBA;
	} else {
		head = 0;
	}
	blkno = ata_bio->blkno;
	if (ata_bio->flags & ATA_LBA48)
		cmd = atacmd_to48(cmd);
	else {
		head |= ((ata_bio->blkno >> 24) & 0xf);
		blkno &= 0xffffff;
	}
	crqb = &mvport->port_crqb->crqb_gen2e + erqqip;
	crqb->cprdbl = htole32(eprd_addr & CRQB_CRQBL_EPRD_MASK);
	crqb->cprdbh = htole32((eprd_addr >> 16) >> 16);
	crqb->ctrlflg = htole32(ctrlflg);
	if (mvport->port_edmamode == dma) {
		crqb->atacommand[0] = htole32(cmd << 16);
		crqb->atacommand[1] = htole32((blkno & 0xffffff) | head << 24);
		crqb->atacommand[2] = htole32(((blkno >> 24) & 0xffffff));
		crqb->atacommand[3] = htole32(ata_bio->nblks & 0xffff);
	} else { /* ncq/queued */

		/*
		 * XXXX: Oops, ata command is not correct.  And, atabus layer
		 * has not been supported yet now.
		 *   Queued DMA read/write.
		 *   read/write FPDMAQueued.
		 */

		crqb->atacommand[0] = htole32(
		    (cmd << 16) | ((ata_bio->nblks & 0xff) << 24));
		crqb->atacommand[1] = htole32((blkno & 0xffffff) | head << 24);
		crqb->atacommand[2] = htole32(((blkno >> 24) & 0xffffff) |
		    ((ata_bio->nblks >> 8) & 0xff));
		crqb->atacommand[3] = htole32(ata_bio->nblks & 0xffff);
		crqb->atacommand[3] = htole32(quetag << 3);
	}
}


#ifdef MVSATA_DEBUG
#define MVSATA_DEBUG_PRINT(type, size, n, p)		\
	do {						\
		int _i;					\
		u_char *_p = (p);			\
							\
		printf(#type "(%d)", (n));		\
		for (_i = 0; _i < (size); _i++, _p++) {	\
			if (_i % 16 == 0)		\
				printf("\n   ");	\
			printf(" %02x", *_p);		\
		}					\
		printf("\n");				\
	} while (0 /* CONSTCOND */)

static void
mvsata_print_crqb(struct mvsata_port *mvport, int n)
{

	MVSATA_DEBUG_PRINT(crqb, sizeof(union mvsata_crqb),
	    n, (u_char *)(mvport->port_crqb + n));
}

static void
mvsata_print_crpb(struct mvsata_port *mvport, int n)
{

	MVSATA_DEBUG_PRINT(crpb, sizeof(struct crpb),
	    n, (u_char *)(mvport->port_crpb + n));
}

static void
mvsata_print_eprd(struct mvsata_port *mvport, int n)
{
	struct eprd *eprd;
	int i = 0;

	eprd = mvport->port_reqtbl[n].eprd;
	while (1 /*CONSTCOND*/) {
		MVSATA_DEBUG_PRINT(eprd, sizeof(struct eprd),
		    i, (u_char *)eprd);
		if (eprd->eot & EPRD_EOT)
			break;
		eprd++;
		i++;
	}
}
#endif
#endif

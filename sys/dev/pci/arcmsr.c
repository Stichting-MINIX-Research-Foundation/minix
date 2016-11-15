/*	$NetBSD: arcmsr.c,v 1.32 2015/03/12 15:33:10 christos Exp $ */
/*	$OpenBSD: arc.c,v 1.68 2007/10/27 03:28:27 dlg Exp $ */

/*
 * Copyright (c) 2007, 2008 Juan Romero Pardines <xtraeme@netbsd.org>
 * Copyright (c) 2006 David Gwynne <dlg@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "bio.h"

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: arcmsr.c,v 1.32 2015/03/12 15:33:10 christos Exp $");

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/kmem.h>
#include <sys/kthread.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/rwlock.h>

#if NBIO > 0
#include <sys/ioctl.h>
#include <dev/biovar.h>
#endif

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/sysmon/sysmonvar.h>

#include <sys/bus.h>

#include <dev/pci/arcmsrvar.h>

/* #define ARC_DEBUG */
#ifdef ARC_DEBUG
#define ARC_D_INIT	(1<<0)
#define ARC_D_RW	(1<<1)
#define ARC_D_DB	(1<<2)

int arcdebug = 0;

#define DPRINTF(p...)		do { if (arcdebug) printf(p); } while (0)
#define DNPRINTF(n, p...)	do { if ((n) & arcdebug) printf(p); } while (0)

#else
#define DPRINTF(p, ...)		/* p */
#define DNPRINTF(n, p, ...)	/* n, p */
#endif

/* 
 * the fw header must always equal this.
 */
static struct arc_fw_hdr arc_fw_hdr = { 0x5e, 0x01, 0x61 };

/*
 * autoconf(9) glue.
 */
static int 	arc_match(device_t, cfdata_t, void *);
static void 	arc_attach(device_t, device_t, void *);
static int 	arc_detach(device_t, int);
static bool 	arc_shutdown(device_t, int);
static int 	arc_intr(void *);
static void	arc_minphys(struct buf *);

CFATTACH_DECL_NEW(arcmsr, sizeof(struct arc_softc),
	arc_match, arc_attach, arc_detach, NULL);

/*
 * bio(4) and sysmon_envsys(9) glue.
 */
#if NBIO > 0
static int 	arc_bioctl(device_t, u_long, void *);
static int 	arc_bio_inq(struct arc_softc *, struct bioc_inq *);
static int 	arc_bio_vol(struct arc_softc *, struct bioc_vol *);
static int	arc_bio_disk_volume(struct arc_softc *, struct bioc_disk *);
static int	arc_bio_disk_novol(struct arc_softc *, struct bioc_disk *);
static void	arc_bio_disk_filldata(struct arc_softc *, struct bioc_disk *,
				      struct arc_fw_diskinfo *, int);
static int 	arc_bio_alarm(struct arc_softc *, struct bioc_alarm *);
static int 	arc_bio_alarm_state(struct arc_softc *, struct bioc_alarm *);
static int 	arc_bio_getvol(struct arc_softc *, int,
			       struct arc_fw_volinfo *);
static int	arc_bio_setstate(struct arc_softc *, struct bioc_setstate *);
static int 	arc_bio_volops(struct arc_softc *, struct bioc_volops *);
static void 	arc_create_sensors(void *);
static void 	arc_refresh_sensors(struct sysmon_envsys *, envsys_data_t *);
static int	arc_fw_parse_status_code(struct arc_softc *, uint8_t *);
#endif

static int
arc_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ARECA) {
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_ARECA_ARC1110:
		case PCI_PRODUCT_ARECA_ARC1120:
		case PCI_PRODUCT_ARECA_ARC1130:
		case PCI_PRODUCT_ARECA_ARC1160:
		case PCI_PRODUCT_ARECA_ARC1170:
		case PCI_PRODUCT_ARECA_ARC1200:
		case PCI_PRODUCT_ARECA_ARC1202:
		case PCI_PRODUCT_ARECA_ARC1210:
		case PCI_PRODUCT_ARECA_ARC1220:
		case PCI_PRODUCT_ARECA_ARC1230:
		case PCI_PRODUCT_ARECA_ARC1260:
		case PCI_PRODUCT_ARECA_ARC1270:
		case PCI_PRODUCT_ARECA_ARC1280:
		case PCI_PRODUCT_ARECA_ARC1380:
		case PCI_PRODUCT_ARECA_ARC1381:
		case PCI_PRODUCT_ARECA_ARC1680:
		case PCI_PRODUCT_ARECA_ARC1681:
			return 1;
		default:
			break;
		}
	}

	return 0;
}

static void
arc_attach(device_t parent, device_t self, void *aux)
{
	struct arc_softc	*sc = device_private(self);
	struct pci_attach_args	*pa = aux;
	struct scsipi_adapter	*adapt = &sc->sc_adapter;
	struct scsipi_channel	*chan = &sc->sc_chan;

	sc->sc_dev = self;
	sc->sc_talking = 0;
	rw_init(&sc->sc_rwlock);
	mutex_init(&sc->sc_mutex, MUTEX_DEFAULT, IPL_BIO);
	cv_init(&sc->sc_condvar, "arcdb");

	if (arc_map_pci_resources(self, pa) != 0) {
		/* error message printed by arc_map_pci_resources */
		return;
	}

	if (arc_query_firmware(self) != 0) {
		/* error message printed by arc_query_firmware */
		goto unmap_pci;
	}

	if (arc_alloc_ccbs(self) != 0) {
		/* error message printed by arc_alloc_ccbs */
		goto unmap_pci;
	}

	if (!pmf_device_register1(self, NULL, NULL, arc_shutdown))
		panic("%s: couldn't establish shutdown handler\n",
		    device_xname(self));

	memset(adapt, 0, sizeof(*adapt));
	adapt->adapt_dev = self;
	adapt->adapt_nchannels = 1;
	adapt->adapt_openings = sc->sc_req_count / ARC_MAX_TARGET;
	adapt->adapt_max_periph = adapt->adapt_openings;
	adapt->adapt_minphys = arc_minphys;		
	adapt->adapt_request = arc_scsi_cmd;

	memset(chan, 0, sizeof(*chan));
	chan->chan_adapter = adapt;
	chan->chan_bustype = &scsi_bustype;
	chan->chan_nluns = ARC_MAX_LUN;
	chan->chan_ntargets = ARC_MAX_TARGET;
	chan->chan_id = ARC_MAX_TARGET;
	chan->chan_flags = SCSIPI_CHAN_NOSETTLE;

	/*
	 * Save the device_t returned, because we could to attach
	 * devices via the management interface.
	 */
	sc->sc_scsibus_dv = config_found(self, &sc->sc_chan, scsiprint);

	/* enable interrupts */
	arc_write(sc, ARC_REG_INTRMASK,
	    ~(ARC_REG_INTRMASK_POSTQUEUE|ARC_REG_INTRSTAT_DOORBELL));

#if NBIO > 0
	/*
	 * Register the driver to bio(4) and setup the sensors.
	 */
	if (bio_register(self, arc_bioctl) != 0)
		panic("%s: bioctl registration failed\n", device_xname(self));

	/* 
	 * you need to talk to the firmware to get volume info. our firmware
	 * interface relies on being able to sleep, so we need to use a thread
	 * to do the work.
	 */
	if (kthread_create(PRI_NONE, KTHREAD_MPSAFE, NULL,
	    arc_create_sensors, sc, &sc->sc_lwp, "arcmsr_sensors") != 0)
		panic("%s: unable to create a kernel thread for sensors\n",
		    device_xname(self));
#endif

        return;

unmap_pci:
	arc_unmap_pci_resources(sc);
}

static int
arc_detach(device_t self, int flags)
{
	struct arc_softc		*sc = device_private(self);

	if (arc_msg0(sc, ARC_REG_INB_MSG0_STOP_BGRB) != 0)
		aprint_error_dev(self, "timeout waiting to stop bg rebuild\n"); 

	if (arc_msg0(sc, ARC_REG_INB_MSG0_FLUSH_CACHE) != 0)
		aprint_error_dev(self, "timeout waiting to flush cache\n");

	if (sc->sc_sme != NULL)
		sysmon_envsys_unregister(sc->sc_sme);

	return 0;
}

static bool
arc_shutdown(device_t self, int how)
{
	struct arc_softc		*sc = device_private(self);

	if (arc_msg0(sc, ARC_REG_INB_MSG0_STOP_BGRB) != 0)
		aprint_error_dev(self, "timeout waiting to stop bg rebuild\n");

	if (arc_msg0(sc, ARC_REG_INB_MSG0_FLUSH_CACHE) != 0)
		aprint_error_dev(self, "timeout waiting to flush cache\n");

	return true;
}

static void
arc_minphys(struct buf *bp)
{
	if (bp->b_bcount > MAXPHYS)
		bp->b_bcount = MAXPHYS;
	minphys(bp);
}

static int
arc_intr(void *arg)
{
	struct arc_softc		*sc = arg;
	struct arc_ccb			*ccb = NULL;
	char				*kva = ARC_DMA_KVA(sc->sc_requests);
	struct arc_io_cmd		*cmd;
	uint32_t			reg, intrstat;

	mutex_spin_enter(&sc->sc_mutex);
	intrstat = arc_read(sc, ARC_REG_INTRSTAT);
	if (intrstat == 0x0) {
		mutex_spin_exit(&sc->sc_mutex);
		return 0;
	}

	intrstat &= ARC_REG_INTRSTAT_POSTQUEUE | ARC_REG_INTRSTAT_DOORBELL;
	arc_write(sc, ARC_REG_INTRSTAT, intrstat);

	if (intrstat & ARC_REG_INTRSTAT_DOORBELL) {
		if (sc->sc_talking) {
			arc_write(sc, ARC_REG_INTRMASK,
			    ~ARC_REG_INTRMASK_POSTQUEUE);
			cv_broadcast(&sc->sc_condvar);
		} else {
			/* otherwise drop it */
			reg = arc_read(sc, ARC_REG_OUTB_DOORBELL);
			arc_write(sc, ARC_REG_OUTB_DOORBELL, reg);
			if (reg & ARC_REG_OUTB_DOORBELL_WRITE_OK)
				arc_write(sc, ARC_REG_INB_DOORBELL,
				    ARC_REG_INB_DOORBELL_READ_OK);
		}
	}
	mutex_spin_exit(&sc->sc_mutex);

	while ((reg = arc_pop(sc)) != 0xffffffff) {
		cmd = (struct arc_io_cmd *)(kva +
		    ((reg << ARC_REG_REPLY_QUEUE_ADDR_SHIFT) -
		    (uint32_t)ARC_DMA_DVA(sc->sc_requests)));
		ccb = &sc->sc_ccbs[htole32(cmd->cmd.context)];

		bus_dmamap_sync(sc->sc_dmat, ARC_DMA_MAP(sc->sc_requests),
		    ccb->ccb_offset, ARC_MAX_IOCMDLEN,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		arc_scsi_cmd_done(sc, ccb, reg);
	}


	return 1;
}

void
arc_scsi_cmd(struct scsipi_channel *chan, scsipi_adapter_req_t req, void *arg)
{
	struct scsipi_periph		*periph;
	struct scsipi_xfer		*xs;
	struct scsipi_adapter		*adapt = chan->chan_adapter;
	struct arc_softc		*sc = device_private(adapt->adapt_dev);
	struct arc_ccb			*ccb;
	struct arc_msg_scsicmd		*cmd;
	uint32_t			reg;
	uint8_t				target;

	switch (req) {
	case ADAPTER_REQ_GROW_RESOURCES:
		/* Not supported. */
		return;
	case ADAPTER_REQ_SET_XFER_MODE:
		/* Not supported. */
		return;
	case ADAPTER_REQ_RUN_XFER:
		break;
	}

	mutex_spin_enter(&sc->sc_mutex);

	xs = arg;
	periph = xs->xs_periph;
	target = periph->periph_target;

	if (xs->cmdlen > ARC_MSG_CDBLEN) {
		memset(&xs->sense, 0, sizeof(xs->sense));
		xs->sense.scsi_sense.response_code = SSD_RCODE_VALID | 0x70;
		xs->sense.scsi_sense.flags = SKEY_ILLEGAL_REQUEST;
		xs->sense.scsi_sense.asc = 0x20;
		xs->error = XS_SENSE;
		xs->status = SCSI_CHECK;
		mutex_spin_exit(&sc->sc_mutex);
		scsipi_done(xs);
		return;
	}

	ccb = arc_get_ccb(sc);
	if (ccb == NULL) {
		xs->error = XS_RESOURCE_SHORTAGE;
		mutex_spin_exit(&sc->sc_mutex);
		scsipi_done(xs);
		return;
	}

	ccb->ccb_xs = xs;

	if (arc_load_xs(ccb) != 0) {
		xs->error = XS_DRIVER_STUFFUP;
		arc_put_ccb(sc, ccb);
		mutex_spin_exit(&sc->sc_mutex);
		scsipi_done(xs);
		return;
	}

	cmd = &ccb->ccb_cmd->cmd;
	reg = ccb->ccb_cmd_post;

	/* bus is always 0 */
	cmd->target = target;
	cmd->lun = periph->periph_lun;
	cmd->function = 1; /* XXX magic number */

	cmd->cdb_len = xs->cmdlen;
	cmd->sgl_len = ccb->ccb_dmamap->dm_nsegs;
	if (xs->xs_control & XS_CTL_DATA_OUT)
		cmd->flags = ARC_MSG_SCSICMD_FLAG_WRITE;
	if (ccb->ccb_dmamap->dm_nsegs > ARC_SGL_256LEN) {
		cmd->flags |= ARC_MSG_SCSICMD_FLAG_SGL_BSIZE_512;
		reg |= ARC_REG_POST_QUEUE_BIGFRAME;
	}

	cmd->context = htole32(ccb->ccb_id);
	cmd->data_len = htole32(xs->datalen);

	memcpy(cmd->cdb, xs->cmd, xs->cmdlen);

	/* we've built the command, let's put it on the hw */
	bus_dmamap_sync(sc->sc_dmat, ARC_DMA_MAP(sc->sc_requests),
	    ccb->ccb_offset, ARC_MAX_IOCMDLEN,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	arc_push(sc, reg);
	if (xs->xs_control & XS_CTL_POLL) {
		if (arc_complete(sc, ccb, xs->timeout) != 0) {
			xs->error = XS_DRIVER_STUFFUP;
			mutex_spin_exit(&sc->sc_mutex);
			scsipi_done(xs);
			return;
		}
	}

	mutex_spin_exit(&sc->sc_mutex);
}

int
arc_load_xs(struct arc_ccb *ccb)
{
	struct arc_softc		*sc = ccb->ccb_sc;
	struct scsipi_xfer		*xs = ccb->ccb_xs;
	bus_dmamap_t			dmap = ccb->ccb_dmamap;
	struct arc_sge			*sgl = ccb->ccb_cmd->sgl, *sge;
	uint64_t			addr;
	int				i, error;

	if (xs->datalen == 0)
		return 0;

	error = bus_dmamap_load(sc->sc_dmat, dmap,
	    xs->data, xs->datalen, NULL,
	    (xs->xs_control & XS_CTL_NOSLEEP) ?
	    BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
	if (error != 0) {
		aprint_error("%s: error %d loading dmamap\n",
		    device_xname(sc->sc_dev), error);
		return 1;
	}

	for (i = 0; i < dmap->dm_nsegs; i++) {
		sge = &sgl[i];

		sge->sg_hdr = htole32(ARC_SGE_64BIT | dmap->dm_segs[i].ds_len);
		addr = dmap->dm_segs[i].ds_addr;
		sge->sg_hi_addr = htole32((uint32_t)(addr >> 32));
		sge->sg_lo_addr = htole32((uint32_t)addr);
	}

	bus_dmamap_sync(sc->sc_dmat, dmap, 0, dmap->dm_mapsize,
	    (xs->xs_control & XS_CTL_DATA_IN) ? BUS_DMASYNC_PREREAD :
	    BUS_DMASYNC_PREWRITE);

	return 0;
}

void
arc_scsi_cmd_done(struct arc_softc *sc, struct arc_ccb *ccb, uint32_t reg)
{
	struct scsipi_xfer		*xs = ccb->ccb_xs;
	struct arc_msg_scsicmd		*cmd;

	if (xs->datalen != 0) {
		bus_dmamap_sync(sc->sc_dmat, ccb->ccb_dmamap, 0,
		    ccb->ccb_dmamap->dm_mapsize,
		    (xs->xs_control & XS_CTL_DATA_IN) ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ccb->ccb_dmamap);
	}

	/* timeout_del */
	xs->status |= XS_STS_DONE;

	if (reg & ARC_REG_REPLY_QUEUE_ERR) {
		cmd = &ccb->ccb_cmd->cmd;

		switch (cmd->status) {
		case ARC_MSG_STATUS_SELTIMEOUT:
		case ARC_MSG_STATUS_ABORTED:
		case ARC_MSG_STATUS_INIT_FAIL:
			xs->status = SCSI_OK;
			xs->error = XS_SELTIMEOUT;
			break;

		case SCSI_CHECK:
			memset(&xs->sense, 0, sizeof(xs->sense));
			memcpy(&xs->sense, cmd->sense_data,
			    min(ARC_MSG_SENSELEN, sizeof(xs->sense)));
			xs->sense.scsi_sense.response_code =
			    SSD_RCODE_VALID | 0x70;
			xs->status = SCSI_CHECK;
			xs->error = XS_SENSE;
			xs->resid = 0;
			break;

		default:
			/* unknown device status */
			xs->error = XS_BUSY; /* try again later? */
			xs->status = SCSI_BUSY;
			break;
		}
	} else {
		xs->status = SCSI_OK;
		xs->error = XS_NOERROR;
		xs->resid = 0;
	}

	arc_put_ccb(sc, ccb);
	scsipi_done(xs);
}

int
arc_complete(struct arc_softc *sc, struct arc_ccb *nccb, int timeout)
{
	struct arc_ccb			*ccb = NULL;
	char				*kva = ARC_DMA_KVA(sc->sc_requests);
	struct arc_io_cmd		*cmd;
	uint32_t			reg;

	do {
		reg = arc_pop(sc);
		if (reg == 0xffffffff) {
			if (timeout-- == 0)
				return 1;

			delay(1000);
			continue;
		}

		cmd = (struct arc_io_cmd *)(kva +
		    ((reg << ARC_REG_REPLY_QUEUE_ADDR_SHIFT) -
		    ARC_DMA_DVA(sc->sc_requests)));
		ccb = &sc->sc_ccbs[htole32(cmd->cmd.context)];

		bus_dmamap_sync(sc->sc_dmat, ARC_DMA_MAP(sc->sc_requests),
		    ccb->ccb_offset, ARC_MAX_IOCMDLEN,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		arc_scsi_cmd_done(sc, ccb, reg);
	} while (nccb != ccb);

	return 0;
}

int
arc_map_pci_resources(device_t self, struct pci_attach_args *pa)
{
	struct arc_softc		*sc = device_private(self);
	pcireg_t			memtype;
	pci_intr_handle_t		ih;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_dmat = pa->pa_dmat;

	memtype = pci_mapreg_type(sc->sc_pc, sc->sc_tag, ARC_PCI_BAR);
	if (pci_mapreg_map(pa, ARC_PCI_BAR, memtype, 0, &sc->sc_iot,
	    &sc->sc_ioh, NULL, &sc->sc_ios) != 0) {
		aprint_error(": unable to map system interface register\n");
		return 1;
	}

	if (pci_intr_map(pa, &ih) != 0) {
		aprint_error(": unable to map interrupt\n");
		goto unmap;
	}

	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO,
	    arc_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error(": unable to map interrupt [2]\n");
		goto unmap;
	}
	
	aprint_normal("\n");
	aprint_normal_dev(self, "interrupting at %s\n",
	    pci_intr_string(pa->pa_pc, ih, intrbuf, sizeof(intrbuf)));

	return 0;

unmap:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
	sc->sc_ios = 0;
	return 1;
}

void
arc_unmap_pci_resources(struct arc_softc *sc)
{
	pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
	sc->sc_ios = 0;
}

int
arc_query_firmware(device_t self)
{
	struct arc_softc 		*sc = device_private(self);
	struct arc_msg_firmware_info	fwinfo;
	char				string[81]; /* sizeof(vendor)*2+1 */

	if (arc_wait_eq(sc, ARC_REG_OUTB_ADDR1, ARC_REG_OUTB_ADDR1_FIRMWARE_OK,
	    ARC_REG_OUTB_ADDR1_FIRMWARE_OK) != 0) {
		aprint_debug_dev(self, "timeout waiting for firmware ok\n");
		return 1;
	}

	if (arc_msg0(sc, ARC_REG_INB_MSG0_GET_CONFIG) != 0) {
		aprint_debug_dev(self, "timeout waiting for get config\n");
		return 1;
	}

	if (arc_msg0(sc, ARC_REG_INB_MSG0_START_BGRB) != 0) {
		aprint_debug_dev(self, "timeout waiting to start bg rebuild\n");
		return 1;
	}

	arc_read_region(sc, ARC_REG_MSGBUF, &fwinfo, sizeof(fwinfo));

	DNPRINTF(ARC_D_INIT, "%s: signature: 0x%08x\n",
	    device_xname(self), htole32(fwinfo.signature));

	if (htole32(fwinfo.signature) != ARC_FWINFO_SIGNATURE_GET_CONFIG) {
		aprint_error_dev(self, "invalid firmware info from iop\n");
		return 1;
	}

	DNPRINTF(ARC_D_INIT, "%s: request_len: %d\n",
	    device_xname(self), htole32(fwinfo.request_len));
	DNPRINTF(ARC_D_INIT, "%s: queue_len: %d\n",
	    device_xname(self), htole32(fwinfo.queue_len));
	DNPRINTF(ARC_D_INIT, "%s: sdram_size: %d\n",
	    device_xname(self), htole32(fwinfo.sdram_size));
	DNPRINTF(ARC_D_INIT, "%s: sata_ports: %d\n",
	    device_xname(self), htole32(fwinfo.sata_ports));

	scsipi_strvis(string, 81, fwinfo.vendor, sizeof(fwinfo.vendor));
	DNPRINTF(ARC_D_INIT, "%s: vendor: \"%s\"\n",
	    device_xname(self), string);

	scsipi_strvis(string, 17, fwinfo.model, sizeof(fwinfo.model));
	aprint_normal_dev(self, "Areca %s Host Adapter RAID controller\n",
	    string);

	scsipi_strvis(string, 33, fwinfo.fw_version, sizeof(fwinfo.fw_version));
	DNPRINTF(ARC_D_INIT, "%s: version: \"%s\"\n",
	    device_xname(self), string);

	aprint_normal_dev(self, "%d ports, %dMB SDRAM, firmware <%s>\n",
	    htole32(fwinfo.sata_ports), htole32(fwinfo.sdram_size), string);

	if (htole32(fwinfo.request_len) != ARC_MAX_IOCMDLEN) {
		aprint_error_dev(self,
		    "unexpected request frame size (%d != %d)\n",
		    htole32(fwinfo.request_len), ARC_MAX_IOCMDLEN);
		return 1;
	}

	sc->sc_req_count = htole32(fwinfo.queue_len);

	return 0;
}

#if NBIO > 0
static int
arc_bioctl(device_t self, u_long cmd, void *addr)
{
	struct arc_softc *sc = device_private(self);
	int error = 0;

	switch (cmd) {
	case BIOCINQ:
		error = arc_bio_inq(sc, (struct bioc_inq *)addr);
		break;

	case BIOCVOL:
		error = arc_bio_vol(sc, (struct bioc_vol *)addr);
		break;

	case BIOCDISK:
		error = arc_bio_disk_volume(sc, (struct bioc_disk *)addr);
		break;

	case BIOCDISK_NOVOL:
		error = arc_bio_disk_novol(sc, (struct bioc_disk *)addr);
		break;

	case BIOCALARM:
		error = arc_bio_alarm(sc, (struct bioc_alarm *)addr);
		break;

	case BIOCSETSTATE:
		error = arc_bio_setstate(sc, (struct bioc_setstate *)addr);
		break;

	case BIOCVOLOPS:
		error = arc_bio_volops(sc, (struct bioc_volops *)addr);
		break;

	default:
		error = ENOTTY;
		break;
	}

	return error;
}

static int
arc_fw_parse_status_code(struct arc_softc *sc, uint8_t *reply)
{
	switch (*reply) {
	case ARC_FW_CMD_RAIDINVAL:
		printf("%s: firmware error (invalid raid set)\n",
		    device_xname(sc->sc_dev));
		return EINVAL;
	case ARC_FW_CMD_VOLINVAL:
		printf("%s: firmware error (invalid volume set)\n",
		    device_xname(sc->sc_dev));
		return EINVAL;
	case ARC_FW_CMD_NORAID:
		printf("%s: firmware error (unexistent raid set)\n",
		    device_xname(sc->sc_dev));
		return ENODEV;
	case ARC_FW_CMD_NOVOLUME:
		printf("%s: firmware error (unexistent volume set)\n",
		    device_xname(sc->sc_dev));
		return ENODEV;
	case ARC_FW_CMD_NOPHYSDRV:
		printf("%s: firmware error (unexistent physical drive)\n",
		    device_xname(sc->sc_dev));
		return ENODEV;
	case ARC_FW_CMD_PARAM_ERR:
		printf("%s: firmware error (parameter error)\n",
		    device_xname(sc->sc_dev));
		return EINVAL;
	case ARC_FW_CMD_UNSUPPORTED:
		printf("%s: firmware error (unsupported command)\n",
		    device_xname(sc->sc_dev));
		return EOPNOTSUPP;
	case ARC_FW_CMD_DISKCFG_CHGD:
		printf("%s: firmware error (disk configuration changed)\n",
		    device_xname(sc->sc_dev));
		return EINVAL;
	case ARC_FW_CMD_PASS_INVAL:
		printf("%s: firmware error (invalid password)\n",
		    device_xname(sc->sc_dev));
		return EINVAL;
	case ARC_FW_CMD_NODISKSPACE:
		printf("%s: firmware error (no disk space available)\n",
		    device_xname(sc->sc_dev));
		return EOPNOTSUPP;
	case ARC_FW_CMD_CHECKSUM_ERR:
		printf("%s: firmware error (checksum error)\n",
		    device_xname(sc->sc_dev));
		return EINVAL;
	case ARC_FW_CMD_PASS_REQD:
		printf("%s: firmware error (password required)\n",
		    device_xname(sc->sc_dev));
		return EPERM;
	case ARC_FW_CMD_OK:
	default:
		return 0;
	}
}

static int
arc_bio_alarm(struct arc_softc *sc, struct bioc_alarm *ba)
{
	uint8_t	request[2], reply[1];
	size_t	len;
	int	error = 0;

	switch (ba->ba_opcode) {
	case BIOC_SAENABLE:
	case BIOC_SADISABLE:
		request[0] = ARC_FW_SET_ALARM;
		request[1] = (ba->ba_opcode == BIOC_SAENABLE) ?
		    ARC_FW_SET_ALARM_ENABLE : ARC_FW_SET_ALARM_DISABLE;
		len = sizeof(request);

		break;

	case BIOC_SASILENCE:
		request[0] = ARC_FW_MUTE_ALARM;
		len = 1;

		break;

	case BIOC_GASTATUS:
		/* system info is too big/ugly to deal with here */
		return arc_bio_alarm_state(sc, ba);

	default:
		return EOPNOTSUPP;
	}

	error = arc_msgbuf(sc, request, len, reply, sizeof(reply));
	if (error != 0)
		return error;

	return arc_fw_parse_status_code(sc, &reply[0]);
}

static int
arc_bio_alarm_state(struct arc_softc *sc, struct bioc_alarm *ba)
{
	struct arc_fw_sysinfo	*sysinfo;
	uint8_t			request;
	int			error = 0;

	sysinfo = kmem_zalloc(sizeof(*sysinfo), KM_SLEEP);

	request = ARC_FW_SYSINFO;
	error = arc_msgbuf(sc, &request, sizeof(request),
	    sysinfo, sizeof(struct arc_fw_sysinfo));

	if (error != 0)
		goto out;

	ba->ba_status = sysinfo->alarm;

out:
	kmem_free(sysinfo, sizeof(*sysinfo));
	return error;
}

static int
arc_bio_volops(struct arc_softc *sc, struct bioc_volops *bc)
{
	/* to create a raid set */
	struct req_craidset {
		uint8_t		cmdcode;
		uint32_t	devmask;
		uint8_t 	raidset_name[16];
	} __packed;

	/* to create a volume set */
	struct req_cvolset {
		uint8_t 	cmdcode;
		uint8_t 	raidset;
		uint8_t 	volset_name[16];
		uint64_t	capacity;
		uint8_t 	raidlevel;
		uint8_t 	stripe;
		uint8_t 	scsi_chan;
		uint8_t 	scsi_target;
		uint8_t 	scsi_lun;
		uint8_t 	tagqueue;
		uint8_t 	cache;
		uint8_t 	speed;
		uint8_t 	quick_init;
	} __packed;

	struct scsibus_softc	*scsibus_sc = NULL;
	struct req_craidset	req_craidset;
	struct req_cvolset 	req_cvolset;
	uint8_t 		request[2];
	uint8_t 		reply[1];
	int 			error = 0;

	switch (bc->bc_opcode) {
	case BIOC_VCREATE_VOLUME:
	    {
		/*
		 * Zero out the structs so that we use some defaults
		 * in raid and volume sets.
		 */
		memset(&req_craidset, 0, sizeof(req_craidset));
		memset(&req_cvolset, 0, sizeof(req_cvolset));

		/*
		 * Firstly we have to create the raid set and
		 * use the default name for all them.
		 */
		req_craidset.cmdcode = ARC_FW_CREATE_RAIDSET;
		req_craidset.devmask = bc->bc_devmask;
		error = arc_msgbuf(sc, &req_craidset, sizeof(req_craidset),
		    reply, sizeof(reply));
		if (error != 0)
			return error;

		error = arc_fw_parse_status_code(sc, &reply[0]);
		if (error) {
			printf("%s: create raidset%d failed\n",
			    device_xname(sc->sc_dev), bc->bc_volid);
			return error;
		}

		/*
		 * At this point the raid set was created, so it's
		 * time to create the volume set.
		 */
		req_cvolset.cmdcode = ARC_FW_CREATE_VOLUME;
		req_cvolset.raidset = bc->bc_volid;
		req_cvolset.capacity = bc->bc_size * ARC_BLOCKSIZE;

		/*
		 * Set the RAID level.
		 */
		switch (bc->bc_level) {
		case 0:
		case 1:
			req_cvolset.raidlevel = bc->bc_level;
			break;
		case BIOC_SVOL_RAID10:
			req_cvolset.raidlevel = 1;
			break;
		case 3:
			req_cvolset.raidlevel = ARC_FW_VOL_RAIDLEVEL_3;
			break;
		case 5:
			req_cvolset.raidlevel = ARC_FW_VOL_RAIDLEVEL_5;
			break;
		case 6:
			req_cvolset.raidlevel = ARC_FW_VOL_RAIDLEVEL_6;
			break;
		default:
			return EOPNOTSUPP;
		}

		/*
		 * Set the stripe size.
		 */
		switch (bc->bc_stripe) {
		case 4:
			req_cvolset.stripe = 0;
			break;
		case 8:
			req_cvolset.stripe = 1;
			break;
		case 16:
			req_cvolset.stripe = 2;
			break;
		case 32:
			req_cvolset.stripe = 3;
			break;
		case 64:
			req_cvolset.stripe = 4;
			break;
		case 128:
			req_cvolset.stripe = 5;
			break;
		default:
			req_cvolset.stripe = 4; /* by default 64K */
			break;
		}

		req_cvolset.scsi_chan = bc->bc_channel;
		req_cvolset.scsi_target = bc->bc_target;
		req_cvolset.scsi_lun = bc->bc_lun;
		req_cvolset.tagqueue = 1; /* always enabled */
		req_cvolset.cache = 1; /* always enabled */
		req_cvolset.speed = 4; /* always max speed */

		/* RAID 1 and 1+0 levels need foreground initialization */
		if (bc->bc_level == 1 || bc->bc_level == BIOC_SVOL_RAID10)
			req_cvolset.quick_init = 1; /* foreground init */

		error = arc_msgbuf(sc, &req_cvolset, sizeof(req_cvolset),
		    reply, sizeof(reply));
		if (error != 0)
			return error;

		error = arc_fw_parse_status_code(sc, &reply[0]);
		if (error) {
			printf("%s: create volumeset%d failed\n",
			    device_xname(sc->sc_dev), bc->bc_volid);
			return error;
		}

		/*
		 * If we are creating a RAID 1 or RAID 1+0 volume,
		 * the volume will be created immediately but it won't
		 * be available until the initialization is done... so
		 * don't bother attaching the sd(4) device.
		 */
		if (bc->bc_level == 1 || bc->bc_level == BIOC_SVOL_RAID10)
			break;

		/*
		 * Do a rescan on the bus to attach the device associated
		 * with the new volume.
		 */
		scsibus_sc = device_private(sc->sc_scsibus_dv);
		(void)scsi_probe_bus(scsibus_sc, bc->bc_target, bc->bc_lun);

		break;
	    }
	case BIOC_VREMOVE_VOLUME:
	    {
		/*
		 * Remove the volume set specified in bc_volid.
		 */
		request[0] = ARC_FW_DELETE_VOLUME;
		request[1] = bc->bc_volid;
		error = arc_msgbuf(sc, request, sizeof(request),
		    reply, sizeof(reply));
		if (error != 0)
			return error;

		error = arc_fw_parse_status_code(sc, &reply[0]);
		if (error) {
			printf("%s: delete volumeset%d failed\n",
			    device_xname(sc->sc_dev), bc->bc_volid);
			return error;
		}

		/*
		 * Detach the sd(4) device associated with the volume,
		 * but if there's an error don't make it a priority.
		 */
		error = scsipi_target_detach(&sc->sc_chan, bc->bc_target,
					     bc->bc_lun, 0);
		if (error)
			printf("%s: couldn't detach sd device for volume %d "
			    "at %u:%u.%u (error=%d)\n",
			    device_xname(sc->sc_dev), bc->bc_volid,
			    bc->bc_channel, bc->bc_target, bc->bc_lun, error);

		/*
		 * and remove the raid set specified in bc_volid,
		 * we only care about volumes.
		 */
		request[0] = ARC_FW_DELETE_RAIDSET;
		request[1] = bc->bc_volid;
		error = arc_msgbuf(sc, request, sizeof(request),
		    reply, sizeof(reply));
		if (error != 0)
			return error;

		error = arc_fw_parse_status_code(sc, &reply[0]);
		if (error) {
			printf("%s: delete raidset%d failed\n",
			    device_xname(sc->sc_dev), bc->bc_volid);
			return error;
		}

		break;
	    }
	default:
		return EOPNOTSUPP;
	}

	return error;
}

static int
arc_bio_setstate(struct arc_softc *sc, struct bioc_setstate *bs)
{
	/* for a hotspare disk */
	struct request_hs {
		uint8_t		cmdcode;
		uint32_t	devmask;
	} __packed;

	/* for a pass-through disk */
	struct request_pt {
		uint8_t 	cmdcode;
		uint8_t		devid;
		uint8_t		scsi_chan;
		uint8_t 	scsi_id;
		uint8_t 	scsi_lun;
		uint8_t 	tagged_queue;
		uint8_t 	cache_mode;
		uint8_t 	max_speed;
	} __packed;

	struct scsibus_softc	*scsibus_sc = NULL;
	struct request_hs	req_hs; /* to add/remove hotspare */
	struct request_pt	req_pt;	/* to add a pass-through */
	uint8_t			req_gen[2];
	uint8_t			reply[1];
	int			error = 0;

	switch (bs->bs_status) {
	case BIOC_SSHOTSPARE:
	    {
		req_hs.cmdcode = ARC_FW_CREATE_HOTSPARE;
		req_hs.devmask = (1 << bs->bs_target);
		goto hotspare;
	    }
	case BIOC_SSDELHOTSPARE:
	    {
		req_hs.cmdcode = ARC_FW_DELETE_HOTSPARE;
		req_hs.devmask = (1 << bs->bs_target);
		goto hotspare;
	    }
	case BIOC_SSPASSTHRU:
	    {
		req_pt.cmdcode = ARC_FW_CREATE_PASSTHRU;
		req_pt.devid = bs->bs_other_id; /* this wants device# */
		req_pt.scsi_chan = bs->bs_channel;
		req_pt.scsi_id = bs->bs_target;
		req_pt.scsi_lun = bs->bs_lun;
		req_pt.tagged_queue = 1; /* always enabled */
		req_pt.cache_mode = 1; /* always enabled */
		req_pt.max_speed = 4; /* always max speed */

		error = arc_msgbuf(sc, &req_pt, sizeof(req_pt),
		    reply, sizeof(reply));
		if (error != 0)
			return error;

		/*
		 * Do a rescan on the bus to attach the new device
		 * associated with the pass-through disk.
		 */
		scsibus_sc = device_private(sc->sc_scsibus_dv);
		(void)scsi_probe_bus(scsibus_sc, bs->bs_target, bs->bs_lun);

		goto out;
	    }
	case BIOC_SSDELPASSTHRU:
	    {
		req_gen[0] = ARC_FW_DELETE_PASSTHRU;
		req_gen[1] = bs->bs_target;
		error = arc_msgbuf(sc, &req_gen, sizeof(req_gen),
		    reply, sizeof(reply));
		if (error != 0)
			return error;

		/*
		 * Detach the sd device associated with this pass-through disk.
		 */
		error = scsipi_target_detach(&sc->sc_chan, bs->bs_target,
					     bs->bs_lun, 0);
		if (error)
			printf("%s: couldn't detach sd device for the "
			    "pass-through disk at %u:%u.%u (error=%d)\n",
			    device_xname(sc->sc_dev),
			    bs->bs_channel, bs->bs_target, bs->bs_lun, error);

		goto out;
	    }
	case BIOC_SSCHECKSTART_VOL:
	    {
		req_gen[0] = ARC_FW_START_CHECKVOL;
		req_gen[1] = bs->bs_volid;
		error = arc_msgbuf(sc, &req_gen, sizeof(req_gen),
		    reply, sizeof(reply));
		if (error != 0)
			return error;

		goto out;
	    }
	case BIOC_SSCHECKSTOP_VOL:
	    {
		uint8_t req = ARC_FW_STOP_CHECKVOL;
		error = arc_msgbuf(sc, &req, 1, reply, sizeof(reply));
		if (error != 0)
			return error;
		
		goto out;
	    }
	default:
		return EOPNOTSUPP;
	}

hotspare:
	error = arc_msgbuf(sc, &req_hs, sizeof(req_hs),
	    reply, sizeof(reply));
	if (error != 0)
		return error;

out:
	return arc_fw_parse_status_code(sc, &reply[0]);
}

static int
arc_bio_inq(struct arc_softc *sc, struct bioc_inq *bi)
{
	uint8_t			request[2];
	struct arc_fw_sysinfo	*sysinfo = NULL;
	struct arc_fw_raidinfo	*raidinfo;
	int			nvols = 0, i;
	int			error = 0;

	raidinfo = kmem_zalloc(sizeof(*raidinfo), KM_SLEEP);

	if (!sc->sc_maxraidset || !sc->sc_maxvolset || !sc->sc_cchans) {
		sysinfo = kmem_zalloc(sizeof(*sysinfo), KM_SLEEP);

		request[0] = ARC_FW_SYSINFO;
		error = arc_msgbuf(sc, request, 1, sysinfo,
		    sizeof(struct arc_fw_sysinfo));
		if (error != 0)
			goto out;

		sc->sc_maxraidset = sysinfo->max_raid_set;
		sc->sc_maxvolset = sysinfo->max_volume_set;
		sc->sc_cchans = sysinfo->ide_channels;
	}

	request[0] = ARC_FW_RAIDINFO;
	for (i = 0; i < sc->sc_maxraidset; i++) {
		request[1] = i;
		error = arc_msgbuf(sc, request, sizeof(request), raidinfo,
		    sizeof(struct arc_fw_raidinfo));
		if (error != 0)
			goto out;

		nvols += raidinfo->volumes;
	}

	strlcpy(bi->bi_dev, device_xname(sc->sc_dev), sizeof(bi->bi_dev));
	bi->bi_novol = nvols;
	bi->bi_nodisk = sc->sc_cchans;

out:
	if (sysinfo)
		kmem_free(sysinfo, sizeof(*sysinfo));
	kmem_free(raidinfo, sizeof(*raidinfo));
	return error;
}

static int
arc_bio_getvol(struct arc_softc *sc, int vol, struct arc_fw_volinfo *volinfo)
{
	uint8_t			request[2];
	int			error = 0;
	int			nvols = 0, i;

	request[0] = ARC_FW_VOLINFO;
	for (i = 0; i < sc->sc_maxvolset; i++) {
		request[1] = i;
		error = arc_msgbuf(sc, request, sizeof(request), volinfo,
		    sizeof(struct arc_fw_volinfo));
		if (error != 0)
			goto out;

		if (volinfo->capacity == 0 && volinfo->capacity2 == 0)
			continue;

		if (nvols == vol)
			break;

		nvols++;
	}

	if (nvols != vol ||
	    (volinfo->capacity == 0 && volinfo->capacity2 == 0)) {
		error = ENODEV;
		goto out;
	}

out:
	return error;
}

static int
arc_bio_vol(struct arc_softc *sc, struct bioc_vol *bv)
{
	struct arc_fw_volinfo	*volinfo;
	uint64_t		blocks;
	uint32_t		status;
	int			error = 0;

	volinfo = kmem_zalloc(sizeof(*volinfo), KM_SLEEP);

	error = arc_bio_getvol(sc, bv->bv_volid, volinfo);
	if (error != 0)
		goto out;

	bv->bv_percent = -1;
	bv->bv_seconds = 0;

	status = htole32(volinfo->volume_status);
	if (status == 0x0) {
		if (htole32(volinfo->fail_mask) == 0x0)
			bv->bv_status = BIOC_SVONLINE;
		else
			bv->bv_status = BIOC_SVDEGRADED;
	} else if (status & ARC_FW_VOL_STATUS_NEED_REGEN) {
		bv->bv_status = BIOC_SVDEGRADED;
	} else if (status & ARC_FW_VOL_STATUS_FAILED) {
		bv->bv_status = BIOC_SVOFFLINE;
	} else if (status & ARC_FW_VOL_STATUS_INITTING) {
		bv->bv_status = BIOC_SVBUILDING;
		bv->bv_percent = htole32(volinfo->progress);
	} else if (status & ARC_FW_VOL_STATUS_REBUILDING) {
		bv->bv_status = BIOC_SVREBUILD;
		bv->bv_percent = htole32(volinfo->progress);
	} else if (status & ARC_FW_VOL_STATUS_MIGRATING) {
		bv->bv_status = BIOC_SVMIGRATING;
		bv->bv_percent = htole32(volinfo->progress);
	} else if (status & ARC_FW_VOL_STATUS_CHECKING) {
		bv->bv_status = BIOC_SVCHECKING;
		bv->bv_percent = htole32(volinfo->progress);
	} else if (status & ARC_FW_VOL_STATUS_NEED_INIT) {
		bv->bv_status = BIOC_SVOFFLINE;
	} else {
		printf("%s: volume %d status 0x%x\n",
		    device_xname(sc->sc_dev), bv->bv_volid, status);
	}

	blocks = (uint64_t)htole32(volinfo->capacity2) << 32;
	blocks += (uint64_t)htole32(volinfo->capacity);
	bv->bv_size = blocks * ARC_BLOCKSIZE; /* XXX */

	switch (volinfo->raid_level) {
	case ARC_FW_VOL_RAIDLEVEL_0:
		bv->bv_level = 0;
		break;
	case ARC_FW_VOL_RAIDLEVEL_1:
		if (volinfo->member_disks > 2)
			bv->bv_level = BIOC_SVOL_RAID10;
		else
			bv->bv_level = 1;
		break;
	case ARC_FW_VOL_RAIDLEVEL_3:
		bv->bv_level = 3;
		break;
	case ARC_FW_VOL_RAIDLEVEL_5:
		bv->bv_level = 5;
		break;
	case ARC_FW_VOL_RAIDLEVEL_6:
		bv->bv_level = 6;
		break;
	case ARC_FW_VOL_RAIDLEVEL_PASSTHRU:
		bv->bv_level = BIOC_SVOL_PASSTHRU;
		break;
	default:
		bv->bv_level = -1;
		break;
	}

	bv->bv_nodisk = volinfo->member_disks;
	bv->bv_stripe_size = volinfo->stripe_size / 2;
	snprintf(bv->bv_dev, sizeof(bv->bv_dev), "sd%d", bv->bv_volid);
	scsipi_strvis(bv->bv_vendor, sizeof(bv->bv_vendor), volinfo->set_name,
	    sizeof(volinfo->set_name));

out:
	kmem_free(volinfo, sizeof(*volinfo));
	return error;
}

static int
arc_bio_disk_novol(struct arc_softc *sc, struct bioc_disk *bd)
{
	struct arc_fw_diskinfo	*diskinfo;
	uint8_t			request[2];
	int			error = 0;

	diskinfo = kmem_zalloc(sizeof(*diskinfo), KM_SLEEP);

	if (bd->bd_diskid >= sc->sc_cchans) {
		error = ENODEV;
		goto out;
	}

	request[0] = ARC_FW_DISKINFO;
	request[1] = bd->bd_diskid;
	error = arc_msgbuf(sc, request, sizeof(request),
	    diskinfo, sizeof(struct arc_fw_diskinfo));
	if (error != 0)
		goto out;

	/* skip disks with no capacity */
	if (htole32(diskinfo->capacity) == 0 &&
	    htole32(diskinfo->capacity2) == 0)
		goto out;

	bd->bd_disknovol = true;
	arc_bio_disk_filldata(sc, bd, diskinfo, bd->bd_diskid);

out:
	kmem_free(diskinfo, sizeof(*diskinfo));
	return error;
}

static void
arc_bio_disk_filldata(struct arc_softc *sc, struct bioc_disk *bd,
		     struct arc_fw_diskinfo *diskinfo, int diskid)
{
	uint64_t		blocks;
	char			model[81];
	char			serial[41];
	char			rev[17];

	/* Ignore bit zero for now, we don't know what it means */
	diskinfo->device_state &= ~0x1;

	switch (diskinfo->device_state) {
	case ARC_FW_DISK_FAILED:
		bd->bd_status = BIOC_SDFAILED;
		break;
	case ARC_FW_DISK_PASSTHRU:
		bd->bd_status = BIOC_SDPASSTHRU;
		break;
	case ARC_FW_DISK_NORMAL:
		bd->bd_status = BIOC_SDONLINE;
		break;
	case ARC_FW_DISK_HOTSPARE:
		bd->bd_status = BIOC_SDHOTSPARE;
		break;
	case ARC_FW_DISK_UNUSED:
		bd->bd_status = BIOC_SDUNUSED;
		break;
	case 0:
		/* disk has been disconnected */
		bd->bd_status = BIOC_SDOFFLINE;
		bd->bd_channel = 1;
		bd->bd_target = 0;
		bd->bd_lun = 0;
		strlcpy(bd->bd_vendor, "disk missing", sizeof(bd->bd_vendor));
		break;
	default:
		printf("%s: unknown disk device_state: 0x%x\n", __func__,
		    diskinfo->device_state);
		bd->bd_status = BIOC_SDINVALID;
		return;
	}

	blocks = (uint64_t)htole32(diskinfo->capacity2) << 32;
	blocks += (uint64_t)htole32(diskinfo->capacity);
	bd->bd_size = blocks * ARC_BLOCKSIZE; /* XXX */

	scsipi_strvis(model, 81, diskinfo->model, sizeof(diskinfo->model));
	scsipi_strvis(serial, 41, diskinfo->serial, sizeof(diskinfo->serial));
	scsipi_strvis(rev, 17, diskinfo->firmware_rev,
	    sizeof(diskinfo->firmware_rev));

	snprintf(bd->bd_vendor, sizeof(bd->bd_vendor), "%s %s", model, rev);
	strlcpy(bd->bd_serial, serial, sizeof(bd->bd_serial));

#if 0
	bd->bd_channel = diskinfo->scsi_attr.channel;
	bd->bd_target = diskinfo->scsi_attr.target;
	bd->bd_lun = diskinfo->scsi_attr.lun;
#endif

	/*
	 * the firwmare doesnt seem to fill scsi_attr in, so fake it with
	 * the diskid.
	 */
	bd->bd_channel = 0;
	bd->bd_target = diskid;
	bd->bd_lun = 0;
}

static int
arc_bio_disk_volume(struct arc_softc *sc, struct bioc_disk *bd)
{
	struct arc_fw_raidinfo	*raidinfo;
	struct arc_fw_volinfo	*volinfo;
	struct arc_fw_diskinfo	*diskinfo;
	uint8_t			request[2];
	int			error = 0;

	volinfo = kmem_zalloc(sizeof(*volinfo), KM_SLEEP);
	raidinfo = kmem_zalloc(sizeof(*raidinfo), KM_SLEEP);
	diskinfo = kmem_zalloc(sizeof(*diskinfo), KM_SLEEP);

	error = arc_bio_getvol(sc, bd->bd_volid, volinfo);
	if (error != 0)
		goto out;

	request[0] = ARC_FW_RAIDINFO;
	request[1] = volinfo->raid_set_number;

	error = arc_msgbuf(sc, request, sizeof(request), raidinfo,
	    sizeof(struct arc_fw_raidinfo));
	if (error != 0)
		goto out;

	if (bd->bd_diskid >= sc->sc_cchans ||
	    bd->bd_diskid >= raidinfo->member_devices) {
		error = ENODEV;
		goto out;
	}

	if (raidinfo->device_array[bd->bd_diskid] == 0xff) {
		/*
		 * The disk has been disconnected, mark it offline
		 * and put it on another bus.
		 */
		bd->bd_channel = 1;
		bd->bd_target = 0;
		bd->bd_lun = 0;
		bd->bd_status = BIOC_SDOFFLINE;
		strlcpy(bd->bd_vendor, "disk missing", sizeof(bd->bd_vendor));
		goto out;
	}

	request[0] = ARC_FW_DISKINFO;
	request[1] = raidinfo->device_array[bd->bd_diskid];
	error = arc_msgbuf(sc, request, sizeof(request), diskinfo,
	    sizeof(struct arc_fw_diskinfo));
	if (error != 0)
		goto out;

	/* now fill our bio disk with data from the firmware */
	arc_bio_disk_filldata(sc, bd, diskinfo,
	    raidinfo->device_array[bd->bd_diskid]);

out:
	kmem_free(raidinfo, sizeof(*raidinfo));
	kmem_free(volinfo, sizeof(*volinfo));
	kmem_free(diskinfo, sizeof(*diskinfo));
	return error;
}
#endif /* NBIO > 0 */

uint8_t
arc_msg_cksum(void *cmd, uint16_t len)
{
	uint8_t	*buf = cmd;
	uint8_t	cksum;
	int	i;

	cksum = (uint8_t)(len >> 8) + (uint8_t)len;
	for (i = 0; i < len; i++)
		cksum += buf[i];

	return cksum;
}


int
arc_msgbuf(struct arc_softc *sc, void *wptr, size_t wbuflen, void *rptr,
	   size_t rbuflen)
{
	uint8_t			rwbuf[ARC_REG_IOC_RWBUF_MAXLEN];
	uint8_t			*wbuf, *rbuf;
	int			wlen, wdone = 0, rlen, rdone = 0;
	struct arc_fw_bufhdr	*bufhdr;
	uint32_t		reg, rwlen;
	int			error = 0;
#ifdef ARC_DEBUG
	int			i;
#endif

	wbuf = rbuf = NULL;

	DNPRINTF(ARC_D_DB, "%s: arc_msgbuf wbuflen: %d rbuflen: %d\n",
	    device_xname(sc->sc_dev), wbuflen, rbuflen);

	wlen = sizeof(struct arc_fw_bufhdr) + wbuflen + 1; /* 1 for cksum */
	wbuf = kmem_alloc(wlen, KM_SLEEP);

	rlen = sizeof(struct arc_fw_bufhdr) + rbuflen + 1; /* 1 for cksum */
	rbuf = kmem_alloc(rlen, KM_SLEEP);

	DNPRINTF(ARC_D_DB, "%s: arc_msgbuf wlen: %d rlen: %d\n",
	    device_xname(sc->sc_dev), wlen, rlen);

	bufhdr = (struct arc_fw_bufhdr *)wbuf;
	bufhdr->hdr = arc_fw_hdr;
	bufhdr->len = htole16(wbuflen);
	memcpy(wbuf + sizeof(struct arc_fw_bufhdr), wptr, wbuflen);
	wbuf[wlen - 1] = arc_msg_cksum(wptr, wbuflen);

	arc_lock(sc);
	if (arc_read(sc, ARC_REG_OUTB_DOORBELL) != 0) {
		error = EBUSY;
		goto out;
	}

	reg = ARC_REG_OUTB_DOORBELL_READ_OK;

	do {
		if ((reg & ARC_REG_OUTB_DOORBELL_READ_OK) && wdone < wlen) {
			memset(rwbuf, 0, sizeof(rwbuf));
			rwlen = (wlen - wdone) % sizeof(rwbuf);
			memcpy(rwbuf, &wbuf[wdone], rwlen);

#ifdef ARC_DEBUG
			if (arcdebug & ARC_D_DB) {
				printf("%s: write %d:",
				    device_xname(sc->sc_dev), rwlen);
				for (i = 0; i < rwlen; i++)
					printf(" 0x%02x", rwbuf[i]);
				printf("\n");
			}
#endif

			/* copy the chunk to the hw */
			arc_write(sc, ARC_REG_IOC_WBUF_LEN, rwlen);
			arc_write_region(sc, ARC_REG_IOC_WBUF, rwbuf,
			    sizeof(rwbuf));

			/* say we have a buffer for the hw */
			arc_write(sc, ARC_REG_INB_DOORBELL,
			    ARC_REG_INB_DOORBELL_WRITE_OK);

			wdone += rwlen;
		}

		while ((reg = arc_read(sc, ARC_REG_OUTB_DOORBELL)) == 0)
			arc_wait(sc);

		arc_write(sc, ARC_REG_OUTB_DOORBELL, reg);

		DNPRINTF(ARC_D_DB, "%s: reg: 0x%08x\n",
		    device_xname(sc->sc_dev), reg);

		if ((reg & ARC_REG_OUTB_DOORBELL_WRITE_OK) && rdone < rlen) {
			rwlen = arc_read(sc, ARC_REG_IOC_RBUF_LEN);
			if (rwlen > sizeof(rwbuf)) {
				DNPRINTF(ARC_D_DB, "%s:  rwlen too big\n",
				    device_xname(sc->sc_dev));
				error = EIO;
				goto out;
			}

			arc_read_region(sc, ARC_REG_IOC_RBUF, rwbuf,
			    sizeof(rwbuf));

			arc_write(sc, ARC_REG_INB_DOORBELL,
			    ARC_REG_INB_DOORBELL_READ_OK);

#ifdef ARC_DEBUG
			printf("%s:  len: %d+%d=%d/%d\n",
			    device_xname(sc->sc_dev),
			    rwlen, rdone, rwlen + rdone, rlen);
			if (arcdebug & ARC_D_DB) {
				printf("%s: read:",
				    device_xname(sc->sc_dev));
				for (i = 0; i < rwlen; i++)
					printf(" 0x%02x", rwbuf[i]);
				printf("\n");
			}
#endif

			if ((rdone + rwlen) > rlen) {
				DNPRINTF(ARC_D_DB, "%s:  rwbuf too big\n",
				    device_xname(sc->sc_dev));
				error = EIO;
				goto out;
			}

			memcpy(&rbuf[rdone], rwbuf, rwlen);
			rdone += rwlen;
		}
	} while (rdone != rlen);

	bufhdr = (struct arc_fw_bufhdr *)rbuf;
	if (memcmp(&bufhdr->hdr, &arc_fw_hdr, sizeof(bufhdr->hdr)) != 0 ||
	    bufhdr->len != htole16(rbuflen)) {
		DNPRINTF(ARC_D_DB, "%s:  rbuf hdr is wrong\n",
		    device_xname(sc->sc_dev));
		error = EIO;
		goto out;
	}

	memcpy(rptr, rbuf + sizeof(struct arc_fw_bufhdr), rbuflen);

	if (rbuf[rlen - 1] != arc_msg_cksum(rptr, rbuflen)) {
		DNPRINTF(ARC_D_DB, "%s:  invalid cksum\n",
		    device_xname(sc->sc_dev));
		error = EIO;
		goto out;
	}

out:
	arc_unlock(sc);
	kmem_free(wbuf, wlen);
	kmem_free(rbuf, rlen);

	return error;
}

void
arc_lock(struct arc_softc *sc)
{
	rw_enter(&sc->sc_rwlock, RW_WRITER);
	mutex_spin_enter(&sc->sc_mutex);
	arc_write(sc, ARC_REG_INTRMASK, ~ARC_REG_INTRMASK_POSTQUEUE);
	sc->sc_talking = 1;
}

void
arc_unlock(struct arc_softc *sc)
{
	KASSERT(mutex_owned(&sc->sc_mutex));

	arc_write(sc, ARC_REG_INTRMASK,
	    ~(ARC_REG_INTRMASK_POSTQUEUE|ARC_REG_INTRMASK_DOORBELL));
	sc->sc_talking = 0;
	mutex_spin_exit(&sc->sc_mutex);
	rw_exit(&sc->sc_rwlock);
}

void
arc_wait(struct arc_softc *sc)
{
	KASSERT(mutex_owned(&sc->sc_mutex));

	arc_write(sc, ARC_REG_INTRMASK,
	    ~(ARC_REG_INTRMASK_POSTQUEUE|ARC_REG_INTRMASK_DOORBELL));
	if (cv_timedwait(&sc->sc_condvar, &sc->sc_mutex, hz) == EWOULDBLOCK)
		arc_write(sc, ARC_REG_INTRMASK, ~ARC_REG_INTRMASK_POSTQUEUE);
}

#if NBIO > 0
static void
arc_create_sensors(void *arg)
{
	struct arc_softc	*sc = arg;
	struct bioc_inq		bi;
	struct bioc_vol		bv;
	int			i, j;
	size_t			slen, count = 0;

	memset(&bi, 0, sizeof(bi));
	if (arc_bio_inq(sc, &bi) != 0) {
		aprint_error("%s: unable to query firmware for sensor info\n",
		    device_xname(sc->sc_dev));
		kthread_exit(0);
	}

	/* There's no point to continue if there are no volumes */
	if (!bi.bi_novol)
		kthread_exit(0);

	for (i = 0; i < bi.bi_novol; i++) {
		memset(&bv, 0, sizeof(bv));
		bv.bv_volid = i;
		if (arc_bio_vol(sc, &bv) != 0)
			kthread_exit(0);

		/* Skip passthrough volumes */
		if (bv.bv_level == BIOC_SVOL_PASSTHRU)
			continue;

		/* new volume found */
		sc->sc_nsensors++;
		/* new disk in a volume found */
		sc->sc_nsensors+= bv.bv_nodisk;
	}

	/* No valid volumes */
	if (!sc->sc_nsensors)
		kthread_exit(0);

	sc->sc_sme = sysmon_envsys_create();
	slen = sizeof(arc_edata_t) * sc->sc_nsensors;
	sc->sc_arc_sensors = kmem_zalloc(slen, KM_SLEEP);

	/* Attach sensors for volumes and disks */
	for (i = 0; i < bi.bi_novol; i++) {
		memset(&bv, 0, sizeof(bv));
		bv.bv_volid = i;
		if (arc_bio_vol(sc, &bv) != 0)
			goto bad;

		sc->sc_arc_sensors[count].arc_sensor.units = ENVSYS_DRIVE;
		sc->sc_arc_sensors[count].arc_sensor.state = ENVSYS_SINVALID;
		sc->sc_arc_sensors[count].arc_sensor.value_cur =
		    ENVSYS_DRIVE_EMPTY;
		sc->sc_arc_sensors[count].arc_sensor.flags =
		    ENVSYS_FMONSTCHANGED;

		/* Skip passthrough volumes */		
		if (bv.bv_level == BIOC_SVOL_PASSTHRU)
			continue;

		if (bv.bv_level == BIOC_SVOL_RAID10)
			snprintf(sc->sc_arc_sensors[count].arc_sensor.desc,
			    sizeof(sc->sc_arc_sensors[count].arc_sensor.desc),
			    "RAID 1+0 volume%d (%s)", i, bv.bv_dev);
		else
			snprintf(sc->sc_arc_sensors[count].arc_sensor.desc,
			    sizeof(sc->sc_arc_sensors[count].arc_sensor.desc),
			    "RAID %d volume%d (%s)", bv.bv_level, i,
			    bv.bv_dev);

		sc->sc_arc_sensors[count].arc_volid = i;

		if (sysmon_envsys_sensor_attach(sc->sc_sme,
		    &sc->sc_arc_sensors[count].arc_sensor))
			goto bad;

		count++;

		/* Attach disk sensors for this volume */
		for (j = 0; j < bv.bv_nodisk; j++) {
			sc->sc_arc_sensors[count].arc_sensor.state =
			    ENVSYS_SINVALID;
			sc->sc_arc_sensors[count].arc_sensor.units =
			    ENVSYS_DRIVE;
			sc->sc_arc_sensors[count].arc_sensor.value_cur =
			    ENVSYS_DRIVE_EMPTY;
			sc->sc_arc_sensors[count].arc_sensor.flags =
			    ENVSYS_FMONSTCHANGED;

			snprintf(sc->sc_arc_sensors[count].arc_sensor.desc,
			    sizeof(sc->sc_arc_sensors[count].arc_sensor.desc),
			    "disk%d volume%d (%s)", j, i, bv.bv_dev);
			sc->sc_arc_sensors[count].arc_volid = i;
			sc->sc_arc_sensors[count].arc_diskid = j + 10;

			if (sysmon_envsys_sensor_attach(sc->sc_sme,
			    &sc->sc_arc_sensors[count].arc_sensor))
				goto bad;

			count++;
		}
	}

	/* 
	 * Register our envsys driver with the framework now that the
	 * sensors were all attached.
	 */
	sc->sc_sme->sme_name = device_xname(sc->sc_dev);
	sc->sc_sme->sme_cookie = sc;
	sc->sc_sme->sme_refresh = arc_refresh_sensors;

	if (sysmon_envsys_register(sc->sc_sme)) {
		aprint_debug("%s: unable to register with sysmon\n",
		    device_xname(sc->sc_dev));
		goto bad;
	}
	kthread_exit(0);

bad:
	sysmon_envsys_destroy(sc->sc_sme);
	kmem_free(sc->sc_arc_sensors, slen);

	sc->sc_sme = NULL;
	sc->sc_arc_sensors = NULL;

	kthread_exit(0);
}

static void
arc_refresh_sensors(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct arc_softc	*sc = sme->sme_cookie;
	struct bioc_vol		bv;
	struct bioc_disk	bd;
	arc_edata_t		*arcdata = (arc_edata_t *)edata;

	/* sanity check */
	if (edata->units != ENVSYS_DRIVE)
		return;

	memset(&bv, 0, sizeof(bv));
	bv.bv_volid = arcdata->arc_volid;

	if (arc_bio_vol(sc, &bv)) {
		bv.bv_status = BIOC_SVINVALID;
		bio_vol_to_envsys(edata, &bv);
		return;
	}

	if (arcdata->arc_diskid) {
		/* Current sensor is handling a disk volume member */
		memset(&bd, 0, sizeof(bd));
		bd.bd_volid = arcdata->arc_volid;
		bd.bd_diskid = arcdata->arc_diskid - 10;

		if (arc_bio_disk_volume(sc, &bd))
			bd.bd_status = BIOC_SDOFFLINE;
		bio_disk_to_envsys(edata, &bd);
	} else {
		/* Current sensor is handling a volume */
		bio_vol_to_envsys(edata, &bv);
	}
}
#endif /* NBIO > 0 */

uint32_t
arc_read(struct arc_softc *sc, bus_size_t r)
{
	uint32_t			v;

	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, 4,
	    BUS_SPACE_BARRIER_READ);
	v = bus_space_read_4(sc->sc_iot, sc->sc_ioh, r);

	DNPRINTF(ARC_D_RW, "%s: arc_read 0x%lx 0x%08x\n",
	    device_xname(sc->sc_dev), r, v);

	return v;
}

void
arc_read_region(struct arc_softc *sc, bus_size_t r, void *buf, size_t len)
{
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, len,
	    BUS_SPACE_BARRIER_READ);
	bus_space_read_region_4(sc->sc_iot, sc->sc_ioh, r,
	    (uint32_t *)buf, len >> 2);
}

void
arc_write(struct arc_softc *sc, bus_size_t r, uint32_t v)
{
	DNPRINTF(ARC_D_RW, "%s: arc_write 0x%lx 0x%08x\n",
	    device_xname(sc->sc_dev), r, v);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, r, v);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

void
arc_write_region(struct arc_softc *sc, bus_size_t r, void *buf, size_t len)
{
	bus_space_write_region_4(sc->sc_iot, sc->sc_ioh, r,
	    (const uint32_t *)buf, len >> 2);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, len,
	    BUS_SPACE_BARRIER_WRITE);
}

int
arc_wait_eq(struct arc_softc *sc, bus_size_t r, uint32_t mask,
	    uint32_t target)
{
	int i;

	DNPRINTF(ARC_D_RW, "%s: arc_wait_eq 0x%lx 0x%08x 0x%08x\n",
	    device_xname(sc->sc_dev), r, mask, target);

	for (i = 0; i < 10000; i++) {
		if ((arc_read(sc, r) & mask) == target)
			return 0;
		delay(1000);
	}

	return 1;
}

int
arc_wait_ne(struct arc_softc *sc, bus_size_t r, uint32_t mask,
	    uint32_t target)
{
	int i;

	DNPRINTF(ARC_D_RW, "%s: arc_wait_ne 0x%lx 0x%08x 0x%08x\n",
	    device_xname(sc->sc_dev), r, mask, target);

	for (i = 0; i < 10000; i++) {
		if ((arc_read(sc, r) & mask) != target)
			return 0;
		delay(1000);
	}

	return 1;
}

int
arc_msg0(struct arc_softc *sc, uint32_t m)
{
	/* post message */
	arc_write(sc, ARC_REG_INB_MSG0, m);
	/* wait for the fw to do it */
	if (arc_wait_eq(sc, ARC_REG_INTRSTAT, ARC_REG_INTRSTAT_MSG0,
	    ARC_REG_INTRSTAT_MSG0) != 0)
		return 1;

	/* ack it */
	arc_write(sc, ARC_REG_INTRSTAT, ARC_REG_INTRSTAT_MSG0);

	return 0;
}

struct arc_dmamem *
arc_dmamem_alloc(struct arc_softc *sc, size_t size)
{
	struct arc_dmamem		*adm;
	int				nsegs;

	adm = kmem_zalloc(sizeof(*adm), KM_NOSLEEP);
	if (adm == NULL)
		return NULL;

	adm->adm_size = size;

	if (bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW, &adm->adm_map) != 0)
		goto admfree;

	if (bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &adm->adm_seg,
	    1, &nsegs, BUS_DMA_NOWAIT) != 0)
		goto destroy;

	if (bus_dmamem_map(sc->sc_dmat, &adm->adm_seg, nsegs, size,
	    &adm->adm_kva, BUS_DMA_NOWAIT|BUS_DMA_COHERENT) != 0)
		goto free;

	if (bus_dmamap_load(sc->sc_dmat, adm->adm_map, adm->adm_kva, size,
	    NULL, BUS_DMA_NOWAIT) != 0)
		goto unmap;

	memset(adm->adm_kva, 0, size);

	return adm;

unmap:
	bus_dmamem_unmap(sc->sc_dmat, adm->adm_kva, size);
free:
	bus_dmamem_free(sc->sc_dmat, &adm->adm_seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmat, adm->adm_map);
admfree:
	kmem_free(adm, sizeof(*adm));

	return NULL;
}

void
arc_dmamem_free(struct arc_softc *sc, struct arc_dmamem *adm)
{
	bus_dmamap_unload(sc->sc_dmat, adm->adm_map);
	bus_dmamem_unmap(sc->sc_dmat, adm->adm_kva, adm->adm_size);
	bus_dmamem_free(sc->sc_dmat, &adm->adm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, adm->adm_map);
	kmem_free(adm, sizeof(*adm));
}

int
arc_alloc_ccbs(device_t self)
{
	struct arc_softc 	*sc = device_private(self);
	struct arc_ccb		*ccb;
	uint8_t			*cmd;
	int			i;
	size_t			ccbslen;

	TAILQ_INIT(&sc->sc_ccb_free);

	ccbslen = sizeof(struct arc_ccb) * sc->sc_req_count;
	sc->sc_ccbs = kmem_zalloc(ccbslen, KM_SLEEP);

	sc->sc_requests = arc_dmamem_alloc(sc,
	    ARC_MAX_IOCMDLEN * sc->sc_req_count);
	if (sc->sc_requests == NULL) {
		aprint_error_dev(self, "unable to allocate ccb dmamem\n");
		goto free_ccbs;
	}
	cmd = ARC_DMA_KVA(sc->sc_requests);

	for (i = 0; i < sc->sc_req_count; i++) {
		ccb = &sc->sc_ccbs[i];

		if (bus_dmamap_create(sc->sc_dmat, MAXPHYS, ARC_SGL_MAXLEN,
		    MAXPHYS, 0, 0, &ccb->ccb_dmamap) != 0) {
			aprint_error_dev(self,
			    "unable to create dmamap for ccb %d\n", i);
			goto free_maps;
		}

		ccb->ccb_sc = sc;
		ccb->ccb_id = i;
		ccb->ccb_offset = ARC_MAX_IOCMDLEN * i;

		ccb->ccb_cmd = (struct arc_io_cmd *)&cmd[ccb->ccb_offset];
		ccb->ccb_cmd_post = (ARC_DMA_DVA(sc->sc_requests) +
		    ccb->ccb_offset) >> ARC_REG_POST_QUEUE_ADDR_SHIFT;

		arc_put_ccb(sc, ccb);
	}

	return 0;

free_maps:
	while ((ccb = arc_get_ccb(sc)) != NULL)
	    bus_dmamap_destroy(sc->sc_dmat, ccb->ccb_dmamap);
	arc_dmamem_free(sc, sc->sc_requests);

free_ccbs:
	kmem_free(sc->sc_ccbs, ccbslen);

	return 1;
}

struct arc_ccb *
arc_get_ccb(struct arc_softc *sc)
{
	struct arc_ccb			*ccb;

	ccb = TAILQ_FIRST(&sc->sc_ccb_free);
	if (ccb != NULL)
		TAILQ_REMOVE(&sc->sc_ccb_free, ccb, ccb_link);
	
	return ccb;
}

void
arc_put_ccb(struct arc_softc *sc, struct arc_ccb *ccb)
{
	ccb->ccb_xs = NULL;
	memset(ccb->ccb_cmd, 0, ARC_MAX_IOCMDLEN);
	TAILQ_INSERT_TAIL(&sc->sc_ccb_free, ccb, ccb_link);
}

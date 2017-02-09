/*	$NetBSD: trm.c,v 1.36 2014/03/29 19:28:25 christos Exp $	*/
/*-
 * Copyright (c) 2002 Izumi Tsutsui.  All rights reserved.
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
 * Device Driver for Tekram DC395U/UW/F, DC315/U
 * PCI SCSI Bus Master Host Adapter
 * (SCSI chip set used Tekram ASIC TRM-S1040)
 *
 * Copyright (c) 2001 Rui-Xiang Guo
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * Ported from
 *   dc395x_trm.c
 *
 * Written for NetBSD 1.4.x by
 *   Erich Chen     (erich@tekram.com.tw)
 *
 * Provided by
 *   (C)Copyright 1995-1999 Tekram Technology Co., Ltd. All rights reserved.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: trm.c,v 1.36 2014/03/29 19:28:25 christos Exp $");

/* #define TRM_DEBUG */
#ifdef TRM_DEBUG
int trm_debug = 1;
#define DPRINTF(arg)	if (trm_debug > 0) printf arg;
#else
#define DPRINTF(arg)
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/queue.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/scsipi/scsi_spc.h>
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsi_message.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/trmreg.h>

/*
 * feature of chip set MAX value
 */
#define TRM_MAX_TARGETS		16
#define TRM_MAX_LUNS		8
#define TRM_MAX_SG_ENTRIES	(MAXPHYS / PAGE_SIZE + 1)
#define TRM_MAX_SRB		32 /* XXX */
#define TRM_MAX_TAG		TRM_MAX_SRB /* XXX */
#define TRM_MAX_OFFSET		15
#define TRM_MAX_PERIOD		125

/*
 * Segment Entry
 */
struct trm_sg_entry {
	uint32_t address;
	uint32_t length;
};

#define TRM_SG_SIZE	(sizeof(struct trm_sg_entry) * TRM_MAX_SG_ENTRIES)

/*
 **********************************************************************
 * The SEEPROM structure for TRM_S1040
 **********************************************************************
 */
struct nvram_target {
	uint8_t config0;		/* Target configuration byte 0 */
#define NTC_DO_WIDE_NEGO	0x20	/* Wide negotiate	     */
#define NTC_DO_TAG_QUEUING	0x10	/* Enable SCSI tagged queuing  */
#define NTC_DO_SEND_START	0x08	/* Send start command SPINUP */
#define NTC_DO_DISCONNECT	0x04	/* Enable SCSI disconnect    */
#define NTC_DO_SYNC_NEGO	0x02	/* Sync negotiation	     */
#define NTC_DO_PARITY_CHK	0x01	/* Parity check enable 	     */
	uint8_t period;			/* Target period	       */
	uint8_t config2;		/* Target configuration byte 2 */
	uint8_t config3;		/* Target configuration byte 3 */
};

struct trm_nvram {
	uint8_t subvendor_id[2];		/* 0,1 Sub Vendor ID */
	uint8_t subsys_id[2];			/* 2,3 Sub System ID */
	uint8_t subclass;			/* 4   Sub Class */
	uint8_t vendor_id[2];			/* 5,6 Vendor ID */
	uint8_t device_id[2];			/* 7,8 Device ID */
	uint8_t reserved0;			/* 9   Reserved */
	struct nvram_target target[TRM_MAX_TARGETS];
						/* 10,11,12,13
						 * 14,15,16,17
						 * ....
						 * 70,71,72,73 */
	uint8_t scsi_id;			/* 74 Host Adapter SCSI ID */
	uint8_t channel_cfg;			/* 75 Channel configuration */
#define NAC_SCANLUN		0x20	/* Include LUN as BIOS device */
#define NAC_DO_PARITY_CHK	0x08    /* Parity check enable        */
#define NAC_POWERON_SCSI_RESET	0x04	/* Power on reset enable      */
#define NAC_GREATER_1G		0x02	/* > 1G support enable	      */
#define NAC_GT2DRIVES		0x01	/* Support more than 2 drives */
	uint8_t delay_time;			/* 76 Power on delay time */
	uint8_t max_tag;			/* 77 Maximum tags */
	uint8_t reserved1;			/* 78 */
	uint8_t boot_target;			/* 79 */
	uint8_t boot_lun;			/* 80 */
	uint8_t reserved2;			/* 81 */
	uint8_t reserved3[44];			/* 82,..125 */
	uint8_t checksum0;			/* 126 */
	uint8_t checksum1;			/* 127 */
#define TRM_NVRAM_CKSUM	0x1234
};

/* Nvram Initiater bits definition */
#define MORE2_DRV		0x00000001
#define GREATER_1G		0x00000002
#define RST_SCSI_BUS		0x00000004
#define ACTIVE_NEGATION		0x00000008
#define NO_SEEK			0x00000010
#define LUN_CHECK		0x00000020

#define trm_eeprom_wait()	DELAY(30)

/*
 *-----------------------------------------------------------------------
 *			SCSI Request Block
 *-----------------------------------------------------------------------
 */
struct trm_srb {
	TAILQ_ENTRY(trm_srb) next;

	struct trm_sg_entry *sgentry;
	struct scsipi_xfer *xs;		/* scsipi_xfer for this cmd */
	bus_dmamap_t dmap;
	bus_size_t sgoffset;		/* Xfer buf offset */

	uint32_t buflen;		/* Total xfer length */
	uint32_t sgaddr;		/* SGList physical starting address */

	int sgcnt;
	int sgindex;

	int hastat;			/* Host Adapter Status */
#define H_STATUS_GOOD		0x00
#define H_SEL_TIMEOUT		0x11
#define H_OVER_UNDER_RUN	0x12
#define H_UNEXP_BUS_FREE	0x13
#define H_TARGET_PHASE_F	0x14
#define H_INVALID_CCB_OP	0x16
#define H_LINK_CCB_BAD		0x17
#define H_BAD_TARGET_DIR	0x18
#define H_DUPLICATE_CCB		0x19
#define H_BAD_CCB_OR_SG		0x1A
#define H_ABORT			0xFF
	int tastat;			/* Target SCSI Status Byte */
	int flag;			/* SRBFlag */
#define AUTO_REQSENSE		0x0001
#define PARITY_ERROR		0x0002
#define SRB_TIMEOUT		0x0004

	int cmdlen;			/* SCSI command length */
	uint8_t cmd[12];	       	/* SCSI command */

	uint8_t tag[2];
};

/*
 * some info about each target and lun on the SCSI bus
 */
struct trm_linfo {
	int used;		/* number of slots in use */
	int avail;		/* where to start scanning */
	int busy;		/* lun in use */
	struct trm_srb *untagged;
	struct trm_srb *queued[TRM_MAX_TAG];
};

struct trm_tinfo {
	u_int flag;		/* Sync mode ? (1 sync):(0 async)  */
#define SYNC_NEGO_ENABLE	0x0001
#define SYNC_NEGO_DOING		0x0002
#define SYNC_NEGO_DONE		0x0004
#define WIDE_NEGO_ENABLE	0x0008
#define WIDE_NEGO_DOING		0x0010
#define WIDE_NEGO_DONE		0x0020
#define USE_TAG_QUEUING		0x0040
#define NO_RESELECT		0x0080
	struct trm_linfo *linfo[TRM_MAX_LUNS];

	uint8_t config0;	/* Target Config */
	uint8_t period;		/* Max Period for nego. */
	uint8_t synctl;		/* Sync control for reg. */
	uint8_t offset;		/* Sync offset for reg. and nego.(low nibble) */
};

/*
 *-----------------------------------------------------------------------
 *			Adapter Control Block
 *-----------------------------------------------------------------------
 */
struct trm_softc {
	device_t sc_dev;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_dma_tag_t sc_dmat;
	bus_dmamap_t sc_dmamap;	/* Map the control structures */

	struct trm_srb *sc_actsrb;
	struct trm_tinfo sc_tinfo[TRM_MAX_TARGETS];

	TAILQ_HEAD(, trm_srb) sc_freesrb,
			      sc_readysrb;
	struct trm_srb *sc_srb;	/* SRB array */

	struct trm_sg_entry *sc_sglist;

	int sc_maxid;
	/*
	 * Link to the generic SCSI driver
	 */
	struct scsipi_channel sc_channel;
	struct scsipi_adapter sc_adapter;

	int sc_id;		/* Adapter SCSI Target ID */

	int sc_state;			/* SRB State */
#define TRM_IDLE		0
#define TRM_WAIT		1
#define TRM_READY		2
#define TRM_MSGOUT		3	/* arbitration+msg_out 1st byte */
#define TRM_MSGIN		4
#define TRM_EXTEND_MSGIN	5
#define TRM_COMMAND		6
#define TRM_START		7	/* arbitration+msg_out+command_out */
#define TRM_DISCONNECTED	8
#define TRM_DATA_XFER		9
#define TRM_XFERPAD		10
#define TRM_STATUS		11
#define TRM_COMPLETED		12
#define TRM_ABORT_SENT		13
#define TRM_UNEXPECT_RESEL	14

	int sc_phase;			/* SCSI phase */
	int sc_config;
#define HCC_WIDE_CARD		0x01
#define HCC_SCSI_RESET		0x02
#define HCC_PARITY		0x04
#define HCC_AUTOTERM		0x08
#define HCC_LOW8TERM		0x10
#define HCC_UP8TERM		0x20

	int sc_flag;
#define RESET_DEV		0x01
#define RESET_DETECT		0x02
#define RESET_DONE		0x04
#define WAIT_TAGMSG		0x08	/* XXX */

	int sc_msgcnt;

	int resel_target; /* XXX */
	int resel_lun; /* XXX */

	uint8_t *sc_msg;
	uint8_t sc_msgbuf[6];
};

/*
 * SCSI Status codes not defined in scsi_all.h
 */
#define SCSI_COND_MET		0x04	/* Condition Met              */
#define SCSI_INTERM_COND_MET	0x14	/* Intermediate condition met */
#define SCSI_UNEXP_BUS_FREE	0xFD	/* Unexpected Bus Free        */
#define SCSI_BUS_RST_DETECT	0xFE	/* SCSI Bus Reset detected    */
#define SCSI_SEL_TIMEOUT	0xFF	/* Selection Timeout          */

static int  trm_match(device_t, cfdata_t, void *);
static void trm_attach(device_t, device_t, void *);

static int  trm_init(struct trm_softc *);

static void trm_scsipi_request(struct scsipi_channel *, scsipi_adapter_req_t,
    void *);
static void trm_update_xfer_mode(struct trm_softc *, int);
static void trm_sched(struct trm_softc *);
static int  trm_select(struct trm_softc *, struct trm_srb *);
static void trm_reset(struct trm_softc *);
static void trm_timeout(void *);
static int  trm_intr(void *);

static void trm_dataout_phase0(struct trm_softc *, int);
static void trm_datain_phase0(struct trm_softc *, int);
static void trm_status_phase0(struct trm_softc *);
static void trm_msgin_phase0(struct trm_softc *);
static void trm_command_phase1(struct trm_softc *);
static void trm_status_phase1(struct trm_softc *);
static void trm_msgout_phase1(struct trm_softc *);
static void trm_msgin_phase1(struct trm_softc *);

static void trm_dataio_xfer(struct trm_softc *, int);
static void trm_disconnect(struct trm_softc *);
static void trm_reselect(struct trm_softc *);
static void trm_done(struct trm_softc *, struct trm_srb *);
static int  trm_request_sense(struct trm_softc *, struct trm_srb *);
static void trm_dequeue(struct trm_softc *, struct trm_srb *);

static void trm_scsi_reset_detect(struct trm_softc *);
static void trm_reset_scsi_bus(struct trm_softc *);

static void trm_check_eeprom(struct trm_softc *, struct trm_nvram *);
static void trm_eeprom_read_all(struct trm_softc *, struct trm_nvram *);
static void trm_eeprom_write_all(struct trm_softc *, struct trm_nvram *);
static void trm_eeprom_set_data(struct trm_softc *, uint8_t, uint8_t);
static void trm_eeprom_write_cmd(struct trm_softc *, uint8_t, uint8_t);
static uint8_t trm_eeprom_get_data(struct trm_softc *, uint8_t);

CFATTACH_DECL_NEW(trm, sizeof(struct trm_softc),
    trm_match, trm_attach, NULL, NULL);

/* real period: */
static const uint8_t trm_clock_period[] = {
	12,	/*  48 ns 20.0 MB/sec */
	18,	/*  72 ns 13.3 MB/sec */
	25,	/* 100 ns 10.0 MB/sec */
	31,	/* 124 ns  8.0 MB/sec */
	37,	/* 148 ns  6.6 MB/sec */
	43,	/* 172 ns  5.7 MB/sec */
	50,	/* 200 ns  5.0 MB/sec */
	62	/* 248 ns  4.0 MB/sec */
};
#define NPERIOD	__arraycount(trm_clock_period)

static int
trm_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_TEKRAM2)
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_TEKRAM2_DC315:
			return 1;
		}
	return 0;
}

/*
 * attach and init a host adapter
 */
static void
trm_attach(device_t parent, device_t self, void *aux)
{
	struct trm_softc *sc = device_private(self);
	struct pci_attach_args *const pa = aux;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	pci_intr_handle_t ih;
	pcireg_t command;
	const char *intrstr;
	int fl = 0;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;

	/*
	 * Some cards do not allow memory mapped accesses
	 * pa_pc:  chipset tag
	 * pa_tag: pci tag
	 */
	command = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	if ((command & (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MASTER_ENABLE)) !=
	    (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MASTER_ENABLE)) {
		command |= PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MASTER_ENABLE;
		pci_conf_write(pa->pa_pc, pa->pa_tag,
		    PCI_COMMAND_STATUS_REG, command);
	}
	/*
	 * mask for get correct base address of pci IO port
	 */
	if (command & PCI_COMMAND_MEM_ENABLE) {
		fl = pci_mapreg_map(pa, TRM_BAR_MMIO, PCI_MAPREG_TYPE_MEM, 0, &iot,
		    &ioh, NULL, NULL);
	}
	if (fl != 0) {
		aprint_verbose_dev(self, "couldn't map MMIO registers, tryion PIO\n");
		if ((fl = pci_mapreg_map(pa, TRM_BAR_PIO, PCI_MAPREG_TYPE_IO, 0,
		    &iot, &ioh, NULL, NULL)) != 0) {
			aprint_error(": unable to map registers (%d)\n", fl);
			return;
		}
	}
	/*
	 * test checksum of eeprom.. & initialize softc...
	 */
	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_dmat = pa->pa_dmat;

	if (trm_init(sc) != 0) {
		/*
		 * Error during initialization!
		 */
		return;
	}
	/*
	 * Now try to attach all the sub-devices
	 */
	if ((sc->sc_config & HCC_WIDE_CARD) != 0)
		aprint_normal(": Tekram DC395UW/F (TRM-S1040) Fast40 "
		    "Ultra Wide SCSI Adapter\n");
	else
		aprint_normal(": Tekram DC395U, DC315/U (TRM-S1040) Fast20 "
		    "Ultra SCSI Adapter\n");

	/*
	 * Now tell the generic SCSI layer about our bus.
	 * map and establish interrupt
	 */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih, intrbuf, sizeof(intrbuf));

	if (pci_intr_establish(pa->pa_pc, ih, IPL_BIO, trm_intr, sc) == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	if (intrstr != NULL)
		aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	sc->sc_adapter.adapt_dev = self;
	sc->sc_adapter.adapt_nchannels = 1;
	sc->sc_adapter.adapt_openings = TRM_MAX_SRB;
	sc->sc_adapter.adapt_max_periph = TRM_MAX_SRB;
	sc->sc_adapter.adapt_request = trm_scsipi_request;
	sc->sc_adapter.adapt_minphys = minphys;

	sc->sc_channel.chan_adapter = &sc->sc_adapter;
	sc->sc_channel.chan_bustype = &scsi_bustype;
	sc->sc_channel.chan_channel = 0;
	sc->sc_channel.chan_ntargets = sc->sc_maxid + 1;
	sc->sc_channel.chan_nluns = 8;
	sc->sc_channel.chan_id = sc->sc_id;

	config_found(self, &sc->sc_channel, scsiprint);
}

/*
 * initialize the internal structures for a given SCSI host
 */
static int
trm_init(struct trm_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	bus_dma_segment_t seg;
	struct trm_nvram eeprom;
	struct trm_srb *srb;
	struct trm_tinfo *ti;
	struct nvram_target *tconf;
	int error, rseg, all_sgsize;
	int i, target;
	uint8_t bval;

	DPRINTF(("\n"));

	/*
	 * allocate the space for all SCSI control blocks (SRB) for DMA memory
	 */
	all_sgsize = TRM_MAX_SRB * TRM_SG_SIZE;
	if ((error = bus_dmamem_alloc(sc->sc_dmat, all_sgsize, PAGE_SIZE,
	    0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		aprint_error(": unable to allocate SCSI REQUEST BLOCKS, "
		    "error = %d\n", error);
		return 1;
	}
	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, rseg,
	    all_sgsize, (void **) &sc->sc_sglist,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
		aprint_error(": unable to map SCSI REQUEST BLOCKS, "
		    "error = %d\n", error);
		return 1;
	}
	if ((error = bus_dmamap_create(sc->sc_dmat, all_sgsize, 1,
	    all_sgsize, 0, BUS_DMA_NOWAIT, &sc->sc_dmamap)) != 0) {
		aprint_error(": unable to create SRB DMA maps, "
		    "error = %d\n", error);
		return 1;
	}
	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_dmamap,
	    sc->sc_sglist, all_sgsize, NULL, BUS_DMA_NOWAIT)) != 0) {
		aprint_error(": unable to load SRB DMA maps, "
		    "error = %d\n", error);
		return 1;
	}
	DPRINTF(("all_sgsize=%x\n", all_sgsize));
	memset(sc->sc_sglist, 0, all_sgsize);

	/*
	 * EEPROM CHECKSUM
	 */
	trm_check_eeprom(sc, &eeprom);

	sc->sc_maxid = 7;
	sc->sc_config = HCC_AUTOTERM | HCC_PARITY;
	if (bus_space_read_1(iot, ioh, TRM_GEN_STATUS) & WIDESCSI) {
		sc->sc_config |= HCC_WIDE_CARD;
		sc->sc_maxid = 15;
	}
	if (eeprom.channel_cfg & NAC_POWERON_SCSI_RESET)
		sc->sc_config |= HCC_SCSI_RESET;

	sc->sc_actsrb = NULL;
	sc->sc_id = eeprom.scsi_id;
	sc->sc_flag = 0;

	/*
	 * initialize and link all device's SRB queues of this adapter
	 */
	TAILQ_INIT(&sc->sc_freesrb);
	TAILQ_INIT(&sc->sc_readysrb);

	sc->sc_srb = malloc(sizeof(struct trm_srb) * TRM_MAX_SRB,
	    M_DEVBUF, M_NOWAIT|M_ZERO);
	DPRINTF(("all SRB size=%zx\n", sizeof(struct trm_srb) * TRM_MAX_SRB));
	if (sc->sc_srb == NULL) {
		aprint_error(": can not allocate SRB\n");
		return 1;
	}

	for (i = 0, srb = sc->sc_srb; i < TRM_MAX_SRB; i++) {
		srb->sgentry = sc->sc_sglist + TRM_MAX_SG_ENTRIES * i;
		srb->sgoffset = TRM_SG_SIZE * i;
		srb->sgaddr = sc->sc_dmamap->dm_segs[0].ds_addr + srb->sgoffset;
		/*
		 * map all SRB space to SRB_array
		 */
		if (bus_dmamap_create(sc->sc_dmat,
		    MAXPHYS, TRM_MAX_SG_ENTRIES, MAXPHYS, 0,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &srb->dmap)) {
			aprint_error(": unable to create DMA transfer map.\n");
			free(sc->sc_srb, M_DEVBUF);
			return 1;
		}
		TAILQ_INSERT_TAIL(&sc->sc_freesrb, srb, next);
		srb++;
	}

	/*
	 * initialize all target info structures
	 */
	for (target = 0; target < TRM_MAX_TARGETS; target++) {
		ti = &sc->sc_tinfo[target];
		ti->synctl = 0;
		ti->offset = 0;
		tconf = &eeprom.target[target];
		ti->config0 = tconf->config0;
		ti->period = trm_clock_period[tconf->period & 0x07];
		ti->flag = 0;
		if ((ti->config0 & NTC_DO_DISCONNECT) != 0) {
#ifdef notyet
			if ((ti->config0 & NTC_DO_TAG_QUEUING) != 0)
				ti->flag |= USE_TAG_QUEUING;
#endif
		} else
			ti->flag |= NO_RESELECT;

		DPRINTF(("target %d: config0 = 0x%02x, period = 0x%02x",
		    target, ti->config0, ti->period));
		DPRINTF((", flag = 0x%02x\n", ti->flag));
	}

	/* program configuration 0 */
	bval = PHASELATCH | INITIATOR | BLOCKRST;
	if ((sc->sc_config & HCC_PARITY) != 0)
		bval |= PARITYCHECK;
	bus_space_write_1(iot, ioh, TRM_SCSI_CONFIG0, bval);

	/* program configuration 1 */
	bus_space_write_1(iot, ioh, TRM_SCSI_CONFIG1,
	    ACTIVE_NEG | ACTIVE_NEGPLUS);

	/* 250ms selection timeout */
	bus_space_write_1(iot, ioh, TRM_SCSI_TIMEOUT, SEL_TIMEOUT);

	/* Mask all interrupts */
	bus_space_write_1(iot, ioh, TRM_DMA_INTEN, 0);
	bus_space_write_1(iot, ioh, TRM_SCSI_INTEN, 0);

	/* Reset SCSI module */
	bus_space_write_2(iot, ioh, TRM_SCSI_CONTROL, DO_RSTMODULE);

	/* program Host ID */
	bus_space_write_1(iot, ioh, TRM_SCSI_HOSTID, sc->sc_id);

	/* set asynchronous transfer */
	bus_space_write_1(iot, ioh, TRM_SCSI_OFFSET, 0);

	/* Turn LED control off */
	bus_space_write_2(iot, ioh, TRM_GEN_CONTROL,
	    bus_space_read_2(iot, ioh, TRM_GEN_CONTROL) & ~EN_LED);

	/* DMA config */
	bus_space_write_2(iot, ioh, TRM_DMA_CONFIG,
	    bus_space_read_2(iot, ioh, TRM_DMA_CONFIG) | DMA_ENHANCE);

	/* Clear pending interrupt status */
	(void)bus_space_read_1(iot, ioh, TRM_SCSI_INTSTATUS);

	/* Enable SCSI interrupt */
	bus_space_write_1(iot, ioh, TRM_SCSI_INTEN,
	    EN_SELECT | EN_SELTIMEOUT | EN_DISCONNECT | EN_RESELECTED |
	    EN_SCSIRESET | EN_BUSSERVICE | EN_CMDDONE);
	bus_space_write_1(iot, ioh, TRM_DMA_INTEN, EN_SCSIINTR);

	trm_reset(sc);

	return 0;
}

/*
 * enqueues a SCSI command
 * called by the higher level SCSI driver
 */
static void
trm_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req,
    void *arg)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct trm_softc *sc;
	struct trm_srb *srb;
	struct scsipi_xfer *xs;
	int error, i, s;

	sc = device_private(chan->chan_adapter->adapt_dev);
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		xs = arg;
		DPRINTF(("trm_scsipi_request.....\n"));
		DPRINTF(("target= %d lun= %d\n", xs->xs_periph->periph_target,
		    xs->xs_periph->periph_lun));
		if (xs->xs_control & XS_CTL_RESET) {
			trm_reset(sc);
			xs->error = XS_RESET;
			return;
		}
		if (xs->xs_status & XS_STS_DONE) {
			printf("%s: Is it done?\n", device_xname(sc->sc_dev));
			xs->xs_status &= ~XS_STS_DONE;
		}

		s = splbio();

		/* Get SRB */
		srb = TAILQ_FIRST(&sc->sc_freesrb);
		if (srb != NULL) {
			TAILQ_REMOVE(&sc->sc_freesrb, srb, next);
		} else {
			xs->error = XS_RESOURCE_SHORTAGE;
			scsipi_done(xs);
			splx(s);
			return;
		}

		srb->xs = xs;
		srb->cmdlen = xs->cmdlen;
		memcpy(srb->cmd, xs->cmd, xs->cmdlen);

		if (xs->xs_control & (XS_CTL_DATA_IN | XS_CTL_DATA_OUT)) {
			if ((error = bus_dmamap_load(sc->sc_dmat, srb->dmap,
			    xs->data, xs->datalen, NULL,
			    ((xs->xs_control & XS_CTL_NOSLEEP) ?
			    BUS_DMA_NOWAIT : BUS_DMA_WAITOK) |
			    BUS_DMA_STREAMING |
			    ((xs->xs_control & XS_CTL_DATA_IN) ?
			    BUS_DMA_READ : BUS_DMA_WRITE))) != 0) {
				printf("%s: DMA transfer map unable to load, "
				    "error = %d\n", device_xname(sc->sc_dev),
				    error);
				xs->error = XS_DRIVER_STUFFUP;
				/*
				 * free SRB
				 */
				TAILQ_INSERT_TAIL(&sc->sc_freesrb, srb, next);
				splx(s);
				return;
			}
			bus_dmamap_sync(sc->sc_dmat, srb->dmap, 0,
			    srb->dmap->dm_mapsize,
			    (xs->xs_control & XS_CTL_DATA_IN) ?
			    BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);

			/* Set up the scatter gather list */
			for (i = 0; i < srb->dmap->dm_nsegs; i++) {
				srb->sgentry[i].address =
				    htole32(srb->dmap->dm_segs[i].ds_addr);
				srb->sgentry[i].length =
				    htole32(srb->dmap->dm_segs[i].ds_len);
			}
			srb->buflen = xs->datalen;
			srb->sgcnt = srb->dmap->dm_nsegs;
		} else {
			srb->sgentry[0].address = 0;
			srb->sgentry[0].length = 0;
			srb->buflen = 0;
			srb->sgcnt = 0;
		}
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
		    srb->sgoffset, TRM_SG_SIZE, BUS_DMASYNC_PREWRITE);

		sc->sc_phase = PH_BUS_FREE;	/* SCSI bus free Phase */

		srb->sgindex = 0;
		srb->hastat = 0;
		srb->tastat = 0;
		srb->flag = 0;

		TAILQ_INSERT_TAIL(&sc->sc_readysrb, srb, next);
		if (sc->sc_actsrb == NULL)
			trm_sched(sc);
		splx(s);

		if ((xs->xs_control & XS_CTL_POLL) != 0) {
			int timeout = xs->timeout;

			s = splbio();
			do {
				while (--timeout) {
					DELAY(1000);
					if (bus_space_read_2(iot, ioh,
					    TRM_SCSI_STATUS) & SCSIINTERRUPT)
						break;
				}
				if (timeout == 0) {
					trm_timeout(srb);
					break;
				} else
					trm_intr(sc);
			} while ((xs->xs_status & XS_STS_DONE) == 0);
			splx(s);
		}
		return;

	case ADAPTER_REQ_GROW_RESOURCES:
		/* XXX Not supported. */
		return;

	case ADAPTER_REQ_SET_XFER_MODE:
		{
			struct trm_tinfo *ti;
			struct scsipi_xfer_mode *xm;

			xm = arg;
			ti = &sc->sc_tinfo[xm->xm_target];
			ti->flag &= ~(SYNC_NEGO_ENABLE|WIDE_NEGO_ENABLE);

#ifdef notyet
			if ((xm->xm_mode & PERIPH_CAP_TQING) != 0)
				ti->flag |= USE_TAG_QUEUING;
			else
#endif
				ti->flag &= ~USE_TAG_QUEUING;

			if ((xm->xm_mode & PERIPH_CAP_WIDE16) != 0 && 
			    (sc->sc_config & HCC_WIDE_CARD) != 0 && 
			    (ti->config0 & NTC_DO_WIDE_NEGO) != 0) {
				ti->flag |= WIDE_NEGO_ENABLE;
				ti->flag &= ~WIDE_NEGO_DONE;
			}

			if ((xm->xm_mode & PERIPH_CAP_SYNC) != 0 &&
			    (ti->config0 & NTC_DO_SYNC_NEGO) != 0) {
				ti->flag |= SYNC_NEGO_ENABLE;
				ti->flag &= ~SYNC_NEGO_DONE;
				ti->period = trm_clock_period[0];
			}

			/*
			 * If we're not going to negotiate, send the
			 * notification now, since it won't happen later.
			 */
			if ((ti->flag & (WIDE_NEGO_DONE|SYNC_NEGO_DONE)) ==
			    (WIDE_NEGO_DONE|SYNC_NEGO_DONE))
				trm_update_xfer_mode(sc, xm->xm_target);

			return;
		}
	}
}

static void
trm_update_xfer_mode(struct trm_softc *sc, int target)
{
	struct scsipi_xfer_mode xm;
	struct trm_tinfo *ti;

	ti = &sc->sc_tinfo[target];
	xm.xm_target = target;
	xm.xm_mode = 0;
	xm.xm_period = 0;
	xm.xm_offset = 0;

	if ((ti->synctl & WIDE_SYNC) != 0)
		xm.xm_mode |= PERIPH_CAP_WIDE16;

	if (ti->period > 0) {
		xm.xm_mode |= PERIPH_CAP_SYNC;
		xm.xm_period = ti->period;
		xm.xm_offset = ti->offset;
	}

#ifdef notyet
	if ((ti->flag & USE_TAG_QUEUING) != 0)
		xm.xm_mode |= PERIPH_CAP_TQING;
#endif

	scsipi_async_event(&sc->sc_channel, ASYNC_EVENT_XFER_MODE, &xm);
}

static void
trm_sched(struct trm_softc *sc)
{
	struct trm_srb *srb;
	struct scsipi_periph *periph;
	struct trm_tinfo *ti;
	struct trm_linfo *li;
	int s, lun, tag;

	DPRINTF(("trm_sched...\n"));

	TAILQ_FOREACH(srb, &sc->sc_readysrb, next) {
		periph = srb->xs->xs_periph;
		ti = &sc->sc_tinfo[periph->periph_target];
		lun = periph->periph_lun;

		/* select type of tag for this command */
		if ((ti->flag & NO_RESELECT) != 0 ||
		    (ti->flag & USE_TAG_QUEUING) == 0 ||
		    (srb->flag & AUTO_REQSENSE) != 0 ||
		    (srb->xs->xs_control & XS_CTL_REQSENSE) != 0)
			tag = 0;
		else
			tag = srb->xs->xs_tag_type;
#if 0
		/* XXX use tags for polled commands? */
		if (srb->xs->xs_control & XS_CTL_POLL)
			tag = 0;
#endif

		s = splbio();
		li = ti->linfo[lun];
		if (li == NULL) {
			/* initialize lun info */
			if ((li = malloc(sizeof(*li), M_DEVBUF,
			    M_NOWAIT|M_ZERO)) == NULL) {
				splx(s);
				continue;
			}
			ti->linfo[lun] = li;
		}

		if (tag == 0) {
			/* try to issue this srb as an un-tagged command */
			if (li->untagged == NULL)
				li->untagged = srb;
		}
		if (li->untagged != NULL) {
			tag = 0;
			if (li->busy != 1 && li->used == 0) {
				/* we need to issue the untagged command now */
				srb = li->untagged;
				periph = srb->xs->xs_periph;
			} else {
				/* not ready yet */
				splx(s);
				continue;
			}
		}
		srb->tag[0] = tag;
		if (tag != 0) {
			li->queued[srb->xs->xs_tag_id] = srb;
			srb->tag[1] = srb->xs->xs_tag_id;
			li->used++;
		}

		if (li->untagged != NULL && li->busy != 1) {
			li->busy = 1;
			TAILQ_REMOVE(&sc->sc_readysrb, srb, next);
			sc->sc_actsrb = srb;
			trm_select(sc, srb);
			splx(s);
			break;
		}
		if (li->untagged == NULL && tag != 0) {
			TAILQ_REMOVE(&sc->sc_readysrb, srb, next);
			sc->sc_actsrb = srb;
			trm_select(sc, srb);
			splx(s);
			break;
		} else
			splx(s);
	}
}

static int
trm_select(struct trm_softc *sc, struct trm_srb *srb)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct scsipi_periph *periph = srb->xs->xs_periph;
	int target = periph->periph_target;
	int lun = periph->periph_lun;
	struct trm_tinfo *ti = &sc->sc_tinfo[target];
	uint8_t scsicmd;

	DPRINTF(("trm_select.....\n"));

	if ((srb->xs->xs_control & XS_CTL_POLL) == 0) {
		callout_reset(&srb->xs->xs_callout, mstohz(srb->xs->timeout),
		    trm_timeout, srb);
	}

	bus_space_write_1(iot, ioh, TRM_SCSI_HOSTID, sc->sc_id);
	bus_space_write_1(iot, ioh, TRM_SCSI_TARGETID, target);
	bus_space_write_1(iot, ioh, TRM_SCSI_SYNC, ti->synctl);
	bus_space_write_1(iot, ioh, TRM_SCSI_OFFSET, ti->offset);
	/* Flush FIFO */
	bus_space_write_2(iot, ioh, TRM_SCSI_CONTROL, DO_CLRFIFO);
	DELAY(10);

	sc->sc_phase = PH_BUS_FREE;	/* initial phase */

	DPRINTF(("cmd = 0x%02x\n", srb->cmd[0]));

	if (((ti->flag & WIDE_NEGO_ENABLE) &&
	     (ti->flag & WIDE_NEGO_DONE) == 0) ||
	    ((ti->flag & SYNC_NEGO_ENABLE) &&
	     (ti->flag & SYNC_NEGO_DONE) == 0)) {
		sc->sc_state = TRM_MSGOUT;
		bus_space_write_1(iot, ioh, TRM_SCSI_FIFO,
		    MSG_IDENTIFY(lun, 0));
		bus_space_write_multi_1(iot, ioh,
		    TRM_SCSI_FIFO, srb->cmd, srb->cmdlen);
		/* it's important for atn stop */
		bus_space_write_2(iot, ioh, TRM_SCSI_CONTROL,
		    DO_DATALATCH | DO_HWRESELECT);
		/* SCSI command */
		bus_space_write_1(iot, ioh, TRM_SCSI_COMMAND, SCMD_SEL_ATNSTOP);
		DPRINTF(("select with SEL_ATNSTOP\n"));
		return 0;
	}

	if (srb->tag[0] != 0) {
		/* Send identify message */
		bus_space_write_1(iot, ioh, TRM_SCSI_FIFO,
		    MSG_IDENTIFY(lun, 1));
		/* Send Tag id */
		bus_space_write_1(iot, ioh, TRM_SCSI_FIFO, srb->tag[0]);
		bus_space_write_1(iot, ioh, TRM_SCSI_FIFO, srb->tag[1]);
		scsicmd = SCMD_SEL_ATN3;
		DPRINTF(("select with SEL_ATN3\n"));
	} else {
		/* Send identify message */
		bus_space_write_1(iot, ioh, TRM_SCSI_FIFO,
		    MSG_IDENTIFY(lun,
		    (ti->flag & NO_RESELECT) == 0 &&
		    (srb->flag & AUTO_REQSENSE) == 0 &&
		    (srb->xs->xs_control & XS_CTL_REQSENSE) == 0));
		scsicmd = SCMD_SEL_ATN;
		DPRINTF(("select with SEL_ATN\n"));
	}
	sc->sc_state = TRM_START;

	/*
	 * Send CDB ..command block...
	 */
	bus_space_write_multi_1(iot, ioh, TRM_SCSI_FIFO, srb->cmd, srb->cmdlen);

	/*
	 * If trm_select returns 0: current interrupt status
	 * is interrupt enable.  It's said that SCSI processor is
	 * unoccupied.
	 */
	sc->sc_phase = PH_BUS_FREE;	/* SCSI bus free Phase */
	/* SCSI command */
	bus_space_write_1(iot, ioh, TRM_SCSI_COMMAND, scsicmd);
	return 0;
}

/*
 * perform a hard reset on the SCSI bus (and TRM_S1040 chip).
 */
static void
trm_reset(struct trm_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int s;

	DPRINTF(("trm_reset.........\n"));

	s = splbio();

	/* disable SCSI and DMA interrupt */
	bus_space_write_1(iot, ioh, TRM_DMA_INTEN, 0);
	bus_space_write_1(iot, ioh, TRM_SCSI_INTEN, 0);

	trm_reset_scsi_bus(sc);
	DELAY(100000);

	/* Enable SCSI interrupt */
	bus_space_write_1(iot, ioh, TRM_SCSI_INTEN,
	    EN_SELECT | EN_SELTIMEOUT | EN_DISCONNECT | EN_RESELECTED |
	    EN_SCSIRESET | EN_BUSSERVICE | EN_CMDDONE);

	/* Enable DMA interrupt */
	bus_space_write_1(iot, ioh, TRM_DMA_INTEN, EN_SCSIINTR);

	/* Clear DMA FIFO */
	bus_space_write_1(iot, ioh, TRM_DMA_CONTROL, CLRXFIFO);

	/* Clear SCSI FIFO */
	bus_space_write_2(iot, ioh, TRM_SCSI_CONTROL, DO_CLRFIFO);

	sc->sc_actsrb = NULL;
	sc->sc_flag = 0;	/* RESET_DETECT, RESET_DONE, RESET_DEV */

	splx(s);
}

static void
trm_timeout(void *arg)
{
	struct trm_srb *srb = (struct trm_srb *)arg;
	struct scsipi_xfer *xs;
	struct scsipi_periph *periph;
	struct trm_softc *sc;
	int s;

	if (srb == NULL) {
		printf("trm_timeout called with srb == NULL\n");
		return;
	}

	xs = srb->xs;
	if (xs == NULL) {
		printf("trm_timeout called with xs == NULL\n");
		return;
	}

	periph = xs->xs_periph;
	scsipi_printaddr(xs->xs_periph);
	printf("SCSI OpCode 0x%02x timed out\n", xs->cmd->opcode);

	sc = device_private(periph->periph_channel->chan_adapter->adapt_dev);

	trm_reset_scsi_bus(sc);
	s = splbio();
	srb->flag |= SRB_TIMEOUT;
	trm_done(sc, srb);
	/* XXX needs more.. */
	splx(s);
}

/*
 * Catch an interrupt from the adapter
 * Process pending device interrupts.
 */
static int
trm_intr(void *arg)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct trm_softc *sc;
	int intstat, stat;

	DPRINTF(("trm_intr......\n"));
	sc = arg;
	if (sc == NULL)
		return 0;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	stat = bus_space_read_2(iot, ioh, TRM_SCSI_STATUS);
	if ((stat & SCSIINTERRUPT) == 0)
		return 0;

	DPRINTF(("stat = %04x, ", stat));
	intstat = bus_space_read_1(iot, ioh, TRM_SCSI_INTSTATUS);

	DPRINTF(("intstat=%02x, ", intstat));
	if (intstat & (INT_SELTIMEOUT | INT_DISCONNECT)) {
		DPRINTF(("\n"));
		trm_disconnect(sc);
		return 1;
	}
	if (intstat & INT_RESELECTED) {
		DPRINTF(("\n"));
		trm_reselect(sc);
		return 1;
	}
	if (intstat & INT_SCSIRESET) {
		DPRINTF(("\n"));
		trm_scsi_reset_detect(sc);
		return 1;
	}
	if (intstat & (INT_BUSSERVICE | INT_CMDDONE)) {
		DPRINTF(("sc->sc_phase = %2d, sc->sc_state = %2d\n",
		    sc->sc_phase, sc->sc_state));
		/*
		 * software sequential machine
		 */

		/*
		 * call phase0 functions... "phase entry" handle
		 * every phase before start transfer
		 */
		switch (sc->sc_phase) {
		case PH_DATA_OUT:
			trm_dataout_phase0(sc, stat);
			break;
		case PH_DATA_IN:
			trm_datain_phase0(sc, stat);
			break;
		case PH_COMMAND:
			break;
		case PH_STATUS:
			trm_status_phase0(sc);
			stat = PH_BUS_FREE;
			break;
		case PH_MSG_OUT:
			if (sc->sc_state == TRM_UNEXPECT_RESEL ||
			    sc->sc_state == TRM_ABORT_SENT)
				stat = PH_BUS_FREE;
			break;
		case PH_MSG_IN:
			trm_msgin_phase0(sc);
			stat = PH_BUS_FREE;
			break;
		case PH_BUS_FREE:
			break;
		default:
			printf("%s: unexpected phase in trm_intr() phase0\n",
			    device_xname(sc->sc_dev));
			break;
		}

		sc->sc_phase = stat & PHASEMASK;

		switch (sc->sc_phase) {
		case PH_DATA_OUT:
			trm_dataio_xfer(sc, XFERDATAOUT);
			break;
		case PH_DATA_IN:
			trm_dataio_xfer(sc, XFERDATAIN);
			break;
		case PH_COMMAND:
			trm_command_phase1(sc);
			break;
		case PH_STATUS:
			trm_status_phase1(sc);
			break;
		case PH_MSG_OUT:
			trm_msgout_phase1(sc);
			break;
		case PH_MSG_IN:
			trm_msgin_phase1(sc);
			break;
		case PH_BUS_FREE:
			break;
		default:
			printf("%s: unexpected phase in trm_intr() phase1\n",
			    device_xname(sc->sc_dev));
			break;
		}

		return 1;
	}
	return 0;
}

static void
trm_msgout_phase1(struct trm_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct trm_srb *srb;
	struct scsipi_periph *periph;
	struct trm_tinfo *ti;

	bus_space_write_2(iot, ioh, TRM_SCSI_CONTROL, DO_CLRFIFO);

	srb = sc->sc_actsrb;

	/* message out phase */
	if (srb != NULL) {
		periph = srb->xs->xs_periph;
		ti = &sc->sc_tinfo[periph->periph_target];

		if ((ti->flag & WIDE_NEGO_DOING) == 0 &&
		    (ti->flag & WIDE_NEGO_ENABLE)) {
			/* send WDTR */
			ti->flag &= ~SYNC_NEGO_DONE;

			sc->sc_msgbuf[0] = MSG_IDENTIFY(periph->periph_lun, 0);
			sc->sc_msgbuf[1] = MSG_EXTENDED;
			sc->sc_msgbuf[2] = MSG_EXT_WDTR_LEN;
			sc->sc_msgbuf[3] = MSG_EXT_WDTR;
			sc->sc_msgbuf[4] = MSG_EXT_WDTR_BUS_16_BIT;
			sc->sc_msgcnt = 5;

			ti->flag |= WIDE_NEGO_DOING;
		} else if ((ti->flag & SYNC_NEGO_DOING) == 0 &&
			   (ti->flag & SYNC_NEGO_ENABLE)) {
			/* send SDTR */
			int cnt = 0;

			if ((ti->flag & WIDE_NEGO_DONE) == 0)
				sc->sc_msgbuf[cnt++] =
				    MSG_IDENTIFY(periph->periph_lun, 0);

			sc->sc_msgbuf[cnt++] = MSG_EXTENDED;
			sc->sc_msgbuf[cnt++] = MSG_EXT_SDTR_LEN;
			sc->sc_msgbuf[cnt++] = MSG_EXT_SDTR;
			sc->sc_msgbuf[cnt++] = ti->period;
			sc->sc_msgbuf[cnt++] = TRM_MAX_OFFSET;
			sc->sc_msgcnt = cnt;
			ti->flag |= SYNC_NEGO_DOING;
		}
	}
	if (sc->sc_msgcnt == 0) {
		sc->sc_msgbuf[0] = MSG_ABORT;
		sc->sc_msgcnt = 1;
		sc->sc_state = TRM_ABORT_SENT;
	}

	DPRINTF(("msgout: cnt = %d, ", sc->sc_msgcnt));
	DPRINTF(("msgbuf = %02x %02x %02x %02x %02x %02x\n",
	   sc->sc_msgbuf[0], sc->sc_msgbuf[1], sc->sc_msgbuf[2],
	   sc->sc_msgbuf[3], sc->sc_msgbuf[4], sc->sc_msgbuf[5]));

	bus_space_write_multi_1(iot, ioh, TRM_SCSI_FIFO,
	    sc->sc_msgbuf, sc->sc_msgcnt);
	sc->sc_msgcnt = 0;
	memset(sc->sc_msgbuf, 0, sizeof(sc->sc_msgbuf));

	/* it's important for atn stop */
	bus_space_write_2(iot, ioh, TRM_SCSI_CONTROL, DO_DATALATCH);

	/*
	 * SCSI command
	 */
	bus_space_write_1(iot, ioh, TRM_SCSI_COMMAND, SCMD_FIFO_OUT);
}

static void
trm_command_phase1(struct trm_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct trm_srb *srb;

	srb = sc->sc_actsrb;
	if (srb == NULL) {
		DPRINTF(("trm_command_phase1: no active srb\n"));
		return;
	}

	bus_space_write_2(iot, ioh, TRM_SCSI_CONTROL, DO_CLRATN | DO_CLRFIFO);
	bus_space_write_multi_1(iot, ioh, TRM_SCSI_FIFO, srb->cmd, srb->cmdlen);

	sc->sc_state = TRM_COMMAND;
	/* it's important for atn stop */
	bus_space_write_2(iot, ioh, TRM_SCSI_CONTROL, DO_DATALATCH);

	/*
	 * SCSI command
	 */
	bus_space_write_1(iot, ioh, TRM_SCSI_COMMAND, SCMD_FIFO_OUT);
}

static void
trm_dataout_phase0(struct trm_softc *sc, int stat)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct trm_srb *srb;
	struct scsipi_periph *periph;
	struct trm_tinfo *ti;
	struct trm_sg_entry *sg;
	int sgindex;
	uint32_t xferlen, leftcnt = 0;

	if (sc->sc_state == TRM_XFERPAD)
		return;

	srb = sc->sc_actsrb;
	if (srb == NULL) {
		DPRINTF(("trm_dataout_phase0: no active srb\n"));
		return;
	}
	periph = srb->xs->xs_periph;
	ti = &sc->sc_tinfo[periph->periph_target];

	if ((stat & PARITYERROR) != 0)
		srb->flag |= PARITY_ERROR;

	if ((stat & SCSIXFERDONE) == 0) {
		/*
		 * when data transfer from DMA FIFO to SCSI FIFO
		 * if there was some data left in SCSI FIFO
		 */
		leftcnt = bus_space_read_1(iot, ioh, TRM_SCSI_FIFOCNT) &
		    SCSI_FIFOCNT_MASK;
		if (ti->synctl & WIDE_SYNC)
			/*
			 * if WIDE scsi SCSI FIFOCNT unit is word
			 * so need to * 2
			 */
			leftcnt <<= 1;
	}
	/*
	 * calculate all the residue data that was not yet transferred
	 * SCSI transfer counter + left in SCSI FIFO data
	 *
	 * .....TRM_SCSI_XCNT (24bits)
	 * The counter always decrements by one for every SCSI
	 * byte transfer.
	 * .....TRM_SCSI_FIFOCNT ( 5bits)
	 * The counter is SCSI FIFO offset counter
	 */
	leftcnt += bus_space_read_4(iot, ioh, TRM_SCSI_XCNT);
	if (leftcnt == 1) {
		leftcnt = 0;
		bus_space_write_2(iot, ioh, TRM_SCSI_CONTROL, DO_CLRFIFO);
	}
	if ((leftcnt == 0) || (stat & SCSIXFERCNT_2_ZERO)) {
		while ((bus_space_read_1(iot, ioh, TRM_DMA_STATUS) &
		    DMAXFERCOMP) == 0)
			;	/* XXX needs timeout */

		srb->buflen = 0;
	} else {
		/* Update SG list */

		/*
		 * if transfer not yet complete
		 * there were some data residue in SCSI FIFO or
		 * SCSI transfer counter not empty
		 */
		if (srb->buflen != leftcnt) {
			/* data that had transferred length */
			xferlen = srb->buflen - leftcnt;

			/* next time to be transferred length */
			srb->buflen = leftcnt;

			/*
			 * parsing from last time disconnect sgindex
			 */
			sg = srb->sgentry + srb->sgindex;
			for (sgindex = srb->sgindex;
			     sgindex < srb->sgcnt;
			     sgindex++, sg++) {
				/*
				 * find last time which SG transfer
				 * be disconnect
				 */
				if (xferlen >= le32toh(sg->length))
					xferlen -= le32toh(sg->length);
				else {
					/*
					 * update last time
					 * disconnected SG list
					 */
				        /* residue data length  */
					sg->length =
					    htole32(le32toh(sg->length)
					    - xferlen);
					/* residue data pointer */
					sg->address =
					    htole32(le32toh(sg->address)
					    + xferlen);
					srb->sgindex = sgindex;
					break;
				}
			}
			bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
			    srb->sgoffset, TRM_SG_SIZE, BUS_DMASYNC_PREWRITE);
		}
	}
	bus_space_write_1(iot, ioh, TRM_DMA_CONTROL, STOPDMAXFER);
}

static void
trm_datain_phase0(struct trm_softc *sc, int stat)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct trm_srb *srb;
	struct trm_sg_entry *sg;
	int sgindex;
	uint32_t xferlen, leftcnt = 0;

	if (sc->sc_state == TRM_XFERPAD)
		return;

	srb = sc->sc_actsrb;
	if (srb == NULL) {
		DPRINTF(("trm_datain_phase0: no active srb\n"));
		return;
	}

	if (stat & PARITYERROR)
		srb->flag |= PARITY_ERROR;

	leftcnt += bus_space_read_4(iot, ioh, TRM_SCSI_XCNT);
	if ((leftcnt == 0) || (stat & SCSIXFERCNT_2_ZERO)) {
		while ((bus_space_read_1(iot, ioh, TRM_DMA_STATUS) &
		    DMAXFERCOMP) == 0)
			;	/* XXX needs timeout */

		srb->buflen = 0;
	} else {	/* phase changed */
		/*
		 * parsing the case:
		 * when a transfer not yet complete
		 * but be disconnected by upper layer
		 * if transfer not yet complete
		 * there were some data residue in SCSI FIFO or
		 * SCSI transfer counter not empty
		 */
		if (srb->buflen != leftcnt) {
			/*
			 * data that had transferred length
			 */
			xferlen = srb->buflen - leftcnt;

			/*
			 * next time to be transferred length
			 */
			srb->buflen = leftcnt;

			/*
			 * parsing from last time disconnect sgindex
			 */
			sg = srb->sgentry + srb->sgindex;
			for (sgindex = srb->sgindex;
			     sgindex < srb->sgcnt;
			     sgindex++, sg++) {
				/*
				 * find last time which SG transfer
				 * be disconnect
				 */
				if (xferlen >= le32toh(sg->length))
					xferlen -= le32toh(sg->length);
				else {
					/*
					 * update last time
					 * disconnected SG list
					 */
					/* residue data length  */
					sg->length =
					    htole32(le32toh(sg->length)
					    - xferlen);
					/* residue data pointer */
					sg->address =
					    htole32(le32toh(sg->address)
					    + xferlen);
					srb->sgindex = sgindex;
					break;
				}
			}
			bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
			    srb->sgoffset, TRM_SG_SIZE, BUS_DMASYNC_PREWRITE);
		}
	}
}

static void
trm_dataio_xfer(struct trm_softc *sc, int iodir)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct trm_srb *srb;
	struct scsipi_periph *periph;
	struct trm_tinfo *ti;

	srb = sc->sc_actsrb;
	if (srb == NULL) {
		DPRINTF(("trm_dataio_xfer: no active srb\n"));
		return;
	}
	periph = srb->xs->xs_periph;
	ti = &sc->sc_tinfo[periph->periph_target];

	if (srb->sgindex < srb->sgcnt) {
		if (srb->buflen > 0) {
			/*
			 * load what physical address of Scatter/Gather
			 * list table want to be transfer
			 */
			sc->sc_state = TRM_DATA_XFER;
			bus_space_write_4(iot, ioh, TRM_DMA_XHIGHADDR, 0);
			bus_space_write_4(iot, ioh, TRM_DMA_XLOWADDR,
			    srb->sgaddr +
			    srb->sgindex * sizeof(struct trm_sg_entry));
			/*
			 * load how many bytes in the Scatter/Gather list table
			 */
			bus_space_write_4(iot, ioh, TRM_DMA_XCNT,
			    (srb->sgcnt - srb->sgindex)
			    * sizeof(struct trm_sg_entry));
			/*
			 * load total xfer length (24bits) max value 16Mbyte
			 */
			bus_space_write_4(iot, ioh, TRM_SCSI_XCNT, srb->buflen);
			/* Start DMA transfer */
			bus_space_write_1(iot, ioh, TRM_DMA_COMMAND,
			    iodir | SGXFER);
			bus_space_write_1(iot, ioh, TRM_DMA_CONTROL,
			    STARTDMAXFER);

			/* Start SCSI transfer */
			/* it's important for atn stop */
			bus_space_write_2(iot, ioh, TRM_SCSI_CONTROL,
			    DO_DATALATCH);

			/*
			 * SCSI command
			 */
			bus_space_write_1(iot, ioh, TRM_SCSI_COMMAND,
			    (iodir == XFERDATAOUT) ?
			    SCMD_DMA_OUT : SCMD_DMA_IN);
		} else {	/* xfer pad */
			if (srb->sgcnt) {
				srb->hastat = H_OVER_UNDER_RUN;
			}
			bus_space_write_4(iot, ioh, TRM_SCSI_XCNT,
			    (ti->synctl & WIDE_SYNC) ? 2 : 1);

			if (iodir == XFERDATAOUT)
				bus_space_write_2(iot, ioh, TRM_SCSI_FIFO, 0);
			else
				(void)bus_space_read_2(iot, ioh, TRM_SCSI_FIFO);

			sc->sc_state = TRM_XFERPAD;
			/* it's important for atn stop */
			bus_space_write_2(iot, ioh, TRM_SCSI_CONTROL,
			    DO_DATALATCH);

			/*
			 * SCSI command
			 */
			bus_space_write_1(iot, ioh, TRM_SCSI_COMMAND,
			    (iodir == XFERDATAOUT) ?
			    SCMD_FIFO_OUT : SCMD_FIFO_IN);
		}
	}
}

static void
trm_status_phase0(struct trm_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct trm_srb *srb;

	srb = sc->sc_actsrb;
	if (srb == NULL) {
		DPRINTF(("trm_status_phase0: no active srb\n"));
		return;
	}
	srb->tastat = bus_space_read_1(iot, ioh, TRM_SCSI_FIFO);
	sc->sc_state = TRM_COMPLETED;
	/* it's important for atn stop */
	bus_space_write_2(iot, ioh, TRM_SCSI_CONTROL, DO_DATALATCH);

	/*
	 * SCSI command
	 */
	bus_space_write_1(iot, ioh, TRM_SCSI_COMMAND, SCMD_MSGACCEPT);
}

static void
trm_status_phase1(struct trm_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	if (bus_space_read_1(iot, ioh, TRM_DMA_COMMAND) & XFERDATAIN) {
		if ((bus_space_read_1(iot, ioh, TRM_SCSI_FIFOCNT)
		    & SCSI_FIFO_EMPTY) == 0)
			bus_space_write_2(iot, ioh,
			    TRM_SCSI_CONTROL, DO_CLRFIFO);
		if ((bus_space_read_1(iot, ioh, TRM_DMA_FIFOSTATUS)
		    & DMA_FIFO_EMPTY) == 0)
			bus_space_write_1(iot, ioh, TRM_DMA_CONTROL, CLRXFIFO);
	} else {
		if ((bus_space_read_1(iot, ioh, TRM_DMA_FIFOSTATUS)
		    & DMA_FIFO_EMPTY) == 0)
			bus_space_write_1(iot, ioh, TRM_DMA_CONTROL, CLRXFIFO);
		if ((bus_space_read_1(iot, ioh, TRM_SCSI_FIFOCNT)
		    & SCSI_FIFO_EMPTY) == 0)
			bus_space_write_2(iot, ioh,
			    TRM_SCSI_CONTROL, DO_CLRFIFO);
	}
	sc->sc_state = TRM_STATUS;
	/* it's important for atn stop */
	bus_space_write_2(iot, ioh, TRM_SCSI_CONTROL, DO_DATALATCH);

	/*
	 * SCSI command
	 */
	bus_space_write_1(iot, ioh, TRM_SCSI_COMMAND, SCMD_COMP);
}

static void
trm_msgin_phase0(struct trm_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct trm_srb *srb;
	struct scsipi_periph *periph;
	struct trm_tinfo *ti;
	int index;
	uint8_t msgin_code;

	msgin_code = bus_space_read_1(iot, ioh, TRM_SCSI_FIFO);
	if (sc->sc_state != TRM_EXTEND_MSGIN) {
		DPRINTF(("msgin: code = %02x\n", msgin_code));
		switch (msgin_code) {
		case MSG_DISCONNECT:
			sc->sc_state = TRM_DISCONNECTED;
			break;

		case MSG_SAVEDATAPOINTER:
			break;

		case MSG_EXTENDED:
		case MSG_SIMPLE_Q_TAG:
		case MSG_HEAD_OF_Q_TAG:
		case MSG_ORDERED_Q_TAG:
			sc->sc_state = TRM_EXTEND_MSGIN;
			/* extended message (01h) */
			sc->sc_msgbuf[0] = msgin_code;

			sc->sc_msgcnt = 1;
			/* extended message length (n) */
			sc->sc_msg = &sc->sc_msgbuf[1];

			break;
		case MSG_MESSAGE_REJECT:
			/* Reject message */
			srb = sc->sc_actsrb;
			if (srb == NULL) {
				DPRINTF(("trm_msgin_phase0: "
				    " message reject without actsrb\n"));
				break;
			}
			periph = srb->xs->xs_periph;
			ti = &sc->sc_tinfo[periph->periph_target];

			if (ti->flag & WIDE_NEGO_ENABLE) {
				/* do wide nego reject */
				ti->flag |= WIDE_NEGO_DONE;
				ti->flag &=
				    ~(SYNC_NEGO_DONE | WIDE_NEGO_ENABLE);
				if ((ti->flag & SYNC_NEGO_ENABLE) &&
				    (ti->flag & SYNC_NEGO_DONE) == 0) {
					/* Set ATN, in case ATN was clear */
					sc->sc_state = TRM_MSGOUT;
					bus_space_write_2(iot, ioh,
					    TRM_SCSI_CONTROL, DO_SETATN);
				} else
					/* Clear ATN */
					bus_space_write_2(iot, ioh,
					    TRM_SCSI_CONTROL, DO_CLRATN);
			} else if (ti->flag & SYNC_NEGO_ENABLE) {
				/* do sync nego reject */
				bus_space_write_2(iot, ioh,
				    TRM_SCSI_CONTROL, DO_CLRATN);
				if (ti->flag & SYNC_NEGO_DOING) {
					ti->flag &=~(SYNC_NEGO_ENABLE |
					    SYNC_NEGO_DONE);
					ti->synctl = 0;
					ti->offset = 0;
					bus_space_write_1(iot, ioh,
					    TRM_SCSI_SYNC, ti->synctl);
					bus_space_write_1(iot, ioh,
					    TRM_SCSI_OFFSET, ti->offset);
				}
			}
			break;

		case MSG_IGN_WIDE_RESIDUE:
			bus_space_write_4(iot, ioh, TRM_SCSI_XCNT, 1);
			(void)bus_space_read_1(iot, ioh, TRM_SCSI_FIFO);
			break;

		default:
			/*
			 * Restore data pointer message
			 * Save data pointer message
			 * Completion message
			 * NOP message
			 */
			break;
		}
	} else {
		/*
		 * when extend message in: sc->sc_state = TRM_EXTEND_MSGIN
		 * Parsing incoming extented messages
		 */
		*sc->sc_msg++ = msgin_code;
		sc->sc_msgcnt++;

		DPRINTF(("extended_msgin: cnt = %d, ", sc->sc_msgcnt));
		DPRINTF(("msgbuf = %02x %02x %02x %02x %02x %02x\n",
		    sc->sc_msgbuf[0], sc->sc_msgbuf[1], sc->sc_msgbuf[2],
		    sc->sc_msgbuf[3], sc->sc_msgbuf[4], sc->sc_msgbuf[5]));

		switch (sc->sc_msgbuf[0]) {
		case MSG_SIMPLE_Q_TAG:
		case MSG_HEAD_OF_Q_TAG:
		case MSG_ORDERED_Q_TAG:
			/*
			 * is QUEUE tag message :
			 *
			 * byte 0:
			 *        HEAD    QUEUE TAG (20h)
			 *        ORDERED QUEUE TAG (21h)
			 *        SIMPLE  QUEUE TAG (22h)
			 * byte 1:
			 *        Queue tag (00h - FFh)
			 */
			if (sc->sc_msgcnt == 2 && sc->sc_actsrb == NULL) {
				/* XXX XXX XXX */
				struct trm_linfo *li;
				int tagid;

				sc->sc_flag &= ~WAIT_TAGMSG;
				tagid = sc->sc_msgbuf[1];
				ti = &sc->sc_tinfo[sc->resel_target];
				li = ti->linfo[sc->resel_lun];
				srb = li->queued[tagid];
				if (srb != NULL) {
					sc->sc_actsrb = srb;
					sc->sc_state = TRM_DATA_XFER;
					break;
				} else {
					printf("%s: invalid tag id\n",
					    device_xname(sc->sc_dev));
				}

				sc->sc_state = TRM_UNEXPECT_RESEL;
				sc->sc_msgbuf[0] = MSG_ABORT_TAG;
				sc->sc_msgcnt = 1;
				bus_space_write_2(iot, ioh,
				    TRM_SCSI_CONTROL, DO_SETATN);
			} else
				sc->sc_state = TRM_IDLE;
			break;

		case MSG_EXTENDED:
			srb = sc->sc_actsrb;
			if (srb == NULL) {
				DPRINTF(("trm_msgin_phase0: "
				    "extended message without actsrb\n"));
				break;
			}
			periph = srb->xs->xs_periph;
			ti = &sc->sc_tinfo[periph->periph_target];

			if (sc->sc_msgbuf[2] == MSG_EXT_WDTR &&
			    sc->sc_msgcnt == 4) {
				/*
				 * is Wide data xfer Extended message :
				 * ======================================
				 * WIDE DATA TRANSFER REQUEST
				 * ======================================
				 * byte 0 :  Extended message (01h)
				 * byte 1 :  Extended message length (02h)
				 * byte 2 :  WIDE DATA TRANSFER code (03h)
				 * byte 3 :  Transfer width exponent
				 */
				if (sc->sc_msgbuf[1] != MSG_EXT_WDTR_LEN) {
					/* Length is wrong, reject it */
					ti->flag &= ~(WIDE_NEGO_ENABLE |
					    WIDE_NEGO_DONE);
					sc->sc_state = TRM_MSGOUT;
					sc->sc_msgbuf[0] = MSG_MESSAGE_REJECT;
					sc->sc_msgcnt = 1;
					bus_space_write_2(iot, ioh,
					    TRM_SCSI_CONTROL, DO_SETATN);
					break;
				}

				if ((ti->flag & WIDE_NEGO_ENABLE) == 0)
					sc->sc_msgbuf[3] =
					    MSG_EXT_WDTR_BUS_8_BIT;

				if (sc->sc_msgbuf[3] >
				    MSG_EXT_WDTR_BUS_32_BIT) {
					/* reject_msg: */
					ti->flag &= ~(WIDE_NEGO_ENABLE |
					    WIDE_NEGO_DONE);
					sc->sc_state = TRM_MSGOUT;
					sc->sc_msgbuf[0] = MSG_MESSAGE_REJECT;
					sc->sc_msgcnt = 1;
					bus_space_write_2(iot, ioh,
					    TRM_SCSI_CONTROL, DO_SETATN);
					break;
				}
				if (sc->sc_msgbuf[3] == MSG_EXT_WDTR_BUS_32_BIT)
					/* do 16 bits */
					sc->sc_msgbuf[3] =
					    MSG_EXT_WDTR_BUS_16_BIT;
				if ((ti->flag & WIDE_NEGO_DONE) == 0) {
					ti->flag |= WIDE_NEGO_DONE;
					ti->flag &= ~(SYNC_NEGO_DONE |
					    WIDE_NEGO_ENABLE);
					if (sc->sc_msgbuf[3] !=
					    MSG_EXT_WDTR_BUS_8_BIT)
						/* is Wide data xfer */
						ti->synctl |= WIDE_SYNC;
					trm_update_xfer_mode(sc,
					    periph->periph_target);
				}

				sc->sc_state = TRM_MSGOUT;
				bus_space_write_2(iot, ioh, TRM_SCSI_CONTROL,
				    DO_SETATN);
				break;

			} else if (sc->sc_msgbuf[2] == MSG_EXT_SDTR &&
			 	   sc->sc_msgcnt == 5) {
				/*
				 * is 8bit transfer Extended message :
				 * =================================
				 * SYNCHRONOUS DATA TRANSFER REQUEST
				 * =================================
				 * byte 0 :  Extended message (01h)
				 * byte 1 :  Extended message length (03)
				 * byte 2 :  SYNC DATA TRANSFER code (01h)
				 * byte 3 :  Transfer period factor
				 * byte 4 :  REQ/ACK offset
				 */
				if (sc->sc_msgbuf[1] != MSG_EXT_SDTR_LEN) {
					/* reject_msg */
					sc->sc_state = TRM_MSGOUT;
					sc->sc_msgbuf[0] = MSG_MESSAGE_REJECT;
					sc->sc_msgcnt = 1;
					bus_space_write_2(iot, ioh,
					    TRM_SCSI_CONTROL, DO_SETATN);
					break;
				}

				if ((ti->flag & SYNC_NEGO_DONE) == 0) {
					ti->flag &=
					    ~(SYNC_NEGO_ENABLE|SYNC_NEGO_DOING);
					ti->flag |= SYNC_NEGO_DONE;
					if (sc->sc_msgbuf[3] >= TRM_MAX_PERIOD)
						sc->sc_msgbuf[3] = 0;
					if (sc->sc_msgbuf[4] > TRM_MAX_OFFSET)
						sc->sc_msgbuf[4] =
						    TRM_MAX_OFFSET;

					if (sc->sc_msgbuf[3] == 0 ||
					    sc->sc_msgbuf[4] == 0) {
						/* set async */
						ti->synctl = 0;
						ti->offset = 0;
					} else {
						/* set sync */
						/* Transfer period factor */
						ti->period = sc->sc_msgbuf[3];
						/* REQ/ACK offset */
						ti->offset = sc->sc_msgbuf[4];
						for (index = 0;
						    index < NPERIOD;
						    index++)
							if (ti->period <=
							    trm_clock_period[
							    index])
								break;

						ti->synctl |= ALT_SYNC | index;
					}
					/*
					 * program SCSI control register
					 */
					bus_space_write_1(iot, ioh,
					    TRM_SCSI_SYNC, ti->synctl);
					bus_space_write_1(iot, ioh,
					    TRM_SCSI_OFFSET, ti->offset);
					trm_update_xfer_mode(sc,
					    periph->periph_target);
				}
				sc->sc_state = TRM_IDLE;
			}
			break;
		default:
			break;
		}
	}

	/* it's important for atn stop */
	bus_space_write_2(iot, ioh, TRM_SCSI_CONTROL, DO_DATALATCH);

	/*
	 * SCSI command
	 */
	bus_space_write_1(iot, ioh, TRM_SCSI_COMMAND, SCMD_MSGACCEPT);
}

static void
trm_msgin_phase1(struct trm_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	bus_space_write_2(iot, ioh, TRM_SCSI_CONTROL, DO_CLRFIFO);
	bus_space_write_4(iot, ioh, TRM_SCSI_XCNT, 1);
	if (sc->sc_state != TRM_MSGIN && sc->sc_state != TRM_EXTEND_MSGIN) {
		sc->sc_state = TRM_MSGIN;
	}

	/* it's important for atn stop */
	bus_space_write_2(iot, ioh, TRM_SCSI_CONTROL, DO_DATALATCH);

	/*
	 * SCSI command
	 */
	bus_space_write_1(iot, ioh, TRM_SCSI_COMMAND, SCMD_FIFO_IN);
}

static void
trm_disconnect(struct trm_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct trm_srb *srb;
	int s;

	s = splbio();

	srb = sc->sc_actsrb;
	DPRINTF(("trm_disconnect...............\n"));

	if (srb == NULL) {
		DPRINTF(("trm_disconnect: no active srb\n"));
		DELAY(1000);	/* 1 msec */

		bus_space_write_2(iot, ioh, TRM_SCSI_CONTROL,
		    DO_CLRFIFO | DO_HWRESELECT);
		return;
	}
	sc->sc_phase = PH_BUS_FREE;	/* SCSI bus free Phase */
	bus_space_write_2(iot, ioh, TRM_SCSI_CONTROL,
	    DO_CLRFIFO | DO_HWRESELECT);
	DELAY(100);

	switch (sc->sc_state) {
	case TRM_UNEXPECT_RESEL:
		sc->sc_state = TRM_IDLE;
		break;

	case TRM_ABORT_SENT:
		goto finish;

	case TRM_START:
	case TRM_MSGOUT:
		{
			/* Selection time out - discard all LUNs if empty */
			struct scsipi_periph *periph;
			struct trm_tinfo *ti;
			struct trm_linfo *li;
			int lun;

			DPRINTF(("selection timeout\n"));

			srb->tastat = SCSI_SEL_TIMEOUT; /* XXX Ok? */

			periph = srb->xs->xs_periph;
			ti = &sc->sc_tinfo[periph->periph_target];
			for (lun = 0; lun < TRM_MAX_LUNS; lun++) {
				li = ti->linfo[lun];
				if (li != NULL &&
				    li->untagged == NULL && li->used == 0) {
					ti->linfo[lun] = NULL;
					free(li, M_DEVBUF);
				}
			}
		}
		goto finish;

	case TRM_DISCONNECTED:
		sc->sc_actsrb = NULL;
		sc->sc_state = TRM_IDLE;
		goto sched;

	case TRM_COMPLETED:
		goto finish;
	}

 out:
	splx(s);
	return;

 finish:
	sc->sc_state = TRM_IDLE;
	trm_done(sc, srb);
	goto out;

 sched:
	trm_sched(sc);
	goto out;
}

static void
trm_reselect(struct trm_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct trm_tinfo *ti;
	struct trm_linfo *li;
	int target, lun;

	DPRINTF(("trm_reselect.................\n"));

	if (sc->sc_actsrb != NULL) {
		/* arbitration lost but reselection win */
		sc->sc_state = TRM_READY;
		target = sc->sc_actsrb->xs->xs_periph->periph_target;
		ti = &sc->sc_tinfo[target];
	} else {
		/* Read Reselected Target Id and LUN */
		target = bus_space_read_1(iot, ioh, TRM_SCSI_TARGETID);
		lun = bus_space_read_1(iot, ioh, TRM_SCSI_IDMSG) & 0x07;
		ti = &sc->sc_tinfo[target];
		li = ti->linfo[lun];
		DPRINTF(("target = %d, lun = %d\n", target, lun));

		/*
		 * Check to see if we are running an un-tagged command.
		 * Otherwise ack the IDENTIFY and wait for a tag message.
		 */
		if (li != NULL) {
			if (li->untagged != NULL && li->busy) {
				sc->sc_actsrb = li->untagged;
				sc->sc_state = TRM_DATA_XFER;
			} else {
				sc->resel_target = target;
				sc->resel_lun = lun;
				/* XXX XXX XXX */
				sc->sc_flag |= WAIT_TAGMSG;
			}
		}

		if ((ti->flag & USE_TAG_QUEUING) == 0 &&
		    sc->sc_actsrb == NULL) {
			printf("%s: reselect from target %d lun %d "
			    "without nexus; sending abort\n",
			    device_xname(sc->sc_dev), target, lun);
			sc->sc_state = TRM_UNEXPECT_RESEL;
			sc->sc_msgbuf[0] = MSG_ABORT_TAG;
			sc->sc_msgcnt = 1;
			bus_space_write_2(iot, ioh,
			    TRM_SCSI_CONTROL, DO_SETATN);
		}
	}
	sc->sc_phase = PH_BUS_FREE;	/* SCSI bus free Phase */
	/*
	 * Program HA ID, target ID, period and offset
	 */
	/* target ID */
	bus_space_write_1(iot, ioh, TRM_SCSI_TARGETID, target);

	/* host ID */
	bus_space_write_1(iot, ioh, TRM_SCSI_HOSTID, sc->sc_id);

	/* period */
	bus_space_write_1(iot, ioh, TRM_SCSI_SYNC, ti->synctl);

	/* offset */
	bus_space_write_1(iot, ioh, TRM_SCSI_OFFSET, ti->offset);

	/* it's important for atn stop */
	bus_space_write_2(iot, ioh, TRM_SCSI_CONTROL, DO_DATALATCH);
	/*
	 * SCSI command
	 */
	/* to rls the /ACK signal */
	bus_space_write_1(iot, ioh, TRM_SCSI_COMMAND, SCMD_MSGACCEPT);
}

/*
 * Complete execution of a SCSI command
 * Signal completion to the generic SCSI driver
 */
static void
trm_done(struct trm_softc *sc, struct trm_srb *srb)
{
	struct scsipi_xfer *xs = srb->xs;

	DPRINTF(("trm_done..................\n"));

	if (xs == NULL)
		return;

	if ((xs->xs_control & XS_CTL_POLL) == 0)
		callout_stop(&xs->xs_callout);

	if (xs->xs_control & (XS_CTL_DATA_IN | XS_CTL_DATA_OUT) ||
	    srb->flag & AUTO_REQSENSE) {
		bus_dmamap_sync(sc->sc_dmat, srb->dmap, 0,
		    srb->dmap->dm_mapsize,
		    ((xs->xs_control & XS_CTL_DATA_IN) ||
		    (srb->flag & AUTO_REQSENSE)) ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, srb->dmap);
	}

	/*
	 * target status
	 */
	xs->status = srb->tastat;

	DPRINTF(("xs->status = 0x%02x\n", xs->status));

	switch (xs->status) {
	case SCSI_OK:
		/*
		 * process initiator status......
		 * Adapter (initiator) status
		 */
		if ((srb->hastat & H_OVER_UNDER_RUN) != 0) {
			printf("%s: over/under run error\n",
			    device_xname(sc->sc_dev));
			srb->tastat = 0;
			/* Illegal length (over/under run) */
			xs->error = XS_DRIVER_STUFFUP;
		} else if ((srb->flag & PARITY_ERROR) != 0) {
			printf("%s: parity error\n", device_xname(sc->sc_dev));
			/* Driver failed to perform operation */
			xs->error = XS_DRIVER_STUFFUP; /* XXX */
		} else if ((srb->flag & SRB_TIMEOUT) != 0) {
			xs->resid = srb->buflen;
			xs->error = XS_TIMEOUT;
		} else {
			/* No error */
			xs->resid = srb->buflen;
			srb->hastat = 0;
			if (srb->flag & AUTO_REQSENSE) {
				/* there is no error, (sense is invalid) */
				xs->error = XS_SENSE;
			} else {
				srb->tastat = 0;
				xs->error = XS_NOERROR;
			}
		}
		break;

	case SCSI_CHECK:
		if ((srb->flag & AUTO_REQSENSE) != 0 ||
		    trm_request_sense(sc, srb) != 0) {
			printf("%s: request sense failed\n",
			    device_xname(sc->sc_dev));
			xs->error = XS_DRIVER_STUFFUP;
			break;
		}
		xs->error = XS_SENSE;
		return;

	case SCSI_SEL_TIMEOUT:
		srb->hastat = H_SEL_TIMEOUT;
		srb->tastat = 0;
		xs->error = XS_SELTIMEOUT;
		break;

	case SCSI_QUEUE_FULL:
	case SCSI_BUSY:
		xs->error = XS_BUSY;
		break;

	case SCSI_RESV_CONFLICT:
		DPRINTF(("%s: target reserved at ", device_xname(sc->sc_dev)));
		DPRINTF(("%s %d\n", __FILE__, __LINE__));
		xs->error = XS_BUSY;
		break;

	default:
		srb->hastat = 0;
		printf("%s: trm_done(): unknown status = %02x\n",
		    device_xname(sc->sc_dev), xs->status);
		xs->error = XS_DRIVER_STUFFUP;
		break;
	}

	trm_dequeue(sc, srb);
	if (srb == sc->sc_actsrb) {
		sc->sc_actsrb = NULL;
		trm_sched(sc);
	}

	TAILQ_INSERT_TAIL(&sc->sc_freesrb, srb, next);

	/* Notify cmd done */
	scsipi_done(xs);
}

static int
trm_request_sense(struct trm_softc *sc, struct trm_srb *srb)
{
	struct scsipi_xfer *xs;
	struct scsipi_periph *periph;
	struct trm_tinfo *ti;
	struct trm_linfo *li;
	struct scsi_request_sense *ss = (struct scsi_request_sense *)srb->cmd;
	int error;

	DPRINTF(("trm_request_sense...\n"));

	xs = srb->xs;
	periph = xs->xs_periph;

	srb->flag |= AUTO_REQSENSE;

	/* Status of initiator/target */
	srb->hastat = 0;
	srb->tastat = 0;

	memset(ss, 0, sizeof(*ss));
	ss->opcode = SCSI_REQUEST_SENSE;
	ss->byte2 = periph->periph_lun << SCSI_CMD_LUN_SHIFT;
	ss->length = sizeof(struct scsi_sense_data);

	srb->buflen = sizeof(struct scsi_sense_data);
	srb->sgcnt = 1;
	srb->sgindex = 0;
	srb->cmdlen = sizeof(struct scsi_request_sense);

	if ((error = bus_dmamap_load(sc->sc_dmat, srb->dmap,
	    &xs->sense.scsi_sense, srb->buflen, NULL,
	    BUS_DMA_READ|BUS_DMA_NOWAIT)) != 0) {
		return error;
	}
	bus_dmamap_sync(sc->sc_dmat, srb->dmap, 0,
	    srb->buflen, BUS_DMASYNC_PREREAD);

	srb->sgentry[0].address = htole32(srb->dmap->dm_segs[0].ds_addr);
	srb->sgentry[0].length = htole32(sizeof(struct scsi_sense_data));
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap, srb->sgoffset,
	    TRM_SG_SIZE, BUS_DMASYNC_PREWRITE);

	ti = &sc->sc_tinfo[periph->periph_target];
	li = ti->linfo[periph->periph_lun];
	if (li->busy > 0)
		li->busy = 0;
	trm_dequeue(sc, srb);
	li->untagged = srb;	/* must be executed first to fix C/A */
	li->busy = 2;

	if (srb == sc->sc_actsrb)
		trm_select(sc, srb);
	else {
		TAILQ_INSERT_HEAD(&sc->sc_readysrb, srb, next);
		if (sc->sc_actsrb == NULL)
			trm_sched(sc);
	}
	return 0;
}

static void
trm_dequeue(struct trm_softc *sc, struct trm_srb *srb)
{
	struct scsipi_periph *periph;
	struct trm_tinfo *ti;
	struct trm_linfo *li;

	periph = srb->xs->xs_periph;
	ti = &sc->sc_tinfo[periph->periph_target];
	li = ti->linfo[periph->periph_lun];

	if (li->untagged == srb) {
		li->busy = 0;
		li->untagged = NULL;
	}
	if (srb->tag[0] != 0 && li->queued[srb->tag[1]] != NULL) {
		li->queued[srb->tag[1]] = NULL;
		li->used--;
	}
}

static void
trm_reset_scsi_bus(struct trm_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int timeout, s;

	DPRINTF(("trm_reset_scsi_bus.........\n"));

	s = splbio();

	sc->sc_flag |= RESET_DEV;
	bus_space_write_2(iot, ioh, TRM_SCSI_CONTROL, DO_RSTSCSI);
	for (timeout = 20000; timeout >= 0; timeout--) {
		DELAY(1);
		if ((bus_space_read_2(iot, ioh, TRM_SCSI_INTSTATUS) &
		    INT_SCSIRESET) == 0)
			break;
	}
	if (timeout == 0)
		printf(": scsibus reset timeout\n");

	splx(s);
}

static void
trm_scsi_reset_detect(struct trm_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int s;

	DPRINTF(("trm_scsi_reset_detect...............\n"));
	DELAY(1000000);		/* delay 1 sec */

	s = splbio();

	bus_space_write_1(iot, ioh, TRM_DMA_CONTROL, STOPDMAXFER);
	bus_space_write_2(iot, ioh, TRM_SCSI_CONTROL, DO_CLRFIFO);

	if (sc->sc_flag & RESET_DEV) {
		sc->sc_flag |= RESET_DONE;
	} else {
		sc->sc_flag |= RESET_DETECT;
		sc->sc_actsrb = NULL;
		sc->sc_flag = 0;
		trm_sched(sc);
	}
	splx(s);
}

/*
 * read seeprom 128 bytes to struct eeprom and check checksum.
 * If it is wrong, update with default value.
 */
static void
trm_check_eeprom(struct trm_softc *sc, struct trm_nvram *eeprom)
{
	struct nvram_target *target;
	uint16_t *ep;
	uint16_t chksum;
	int i;

	DPRINTF(("trm_check_eeprom......\n"));
	trm_eeprom_read_all(sc, eeprom);
	ep = (uint16_t *)eeprom;
	chksum = 0;
	for (i = 0; i < 64; i++)
		chksum += le16toh(*ep++);

	if (chksum != TRM_NVRAM_CKSUM) {
		DPRINTF(("TRM_S1040 EEPROM Check Sum ERROR (load default).\n"));
		/*
		 * Checksum error, load default
		 */
		eeprom->subvendor_id[0] = PCI_VENDOR_TEKRAM2 & 0xFF;
		eeprom->subvendor_id[1] = PCI_VENDOR_TEKRAM2 >> 8;
		eeprom->subsys_id[0] = PCI_PRODUCT_TEKRAM2_DC315 & 0xFF;
		eeprom->subsys_id[1] = PCI_PRODUCT_TEKRAM2_DC315 >> 8;
		eeprom->subclass = 0x00;
		eeprom->vendor_id[0] = PCI_VENDOR_TEKRAM2 & 0xFF;
		eeprom->vendor_id[1] = PCI_VENDOR_TEKRAM2 >> 8;
		eeprom->device_id[0] = PCI_PRODUCT_TEKRAM2_DC315 & 0xFF;
		eeprom->device_id[1] = PCI_PRODUCT_TEKRAM2_DC315 >> 8;
		eeprom->reserved0 = 0x00;

		for (i = 0, target = eeprom->target;
		     i < TRM_MAX_TARGETS;
		     i++, target++) {
			target->config0 = 0x77;
			target->period = 0x00;
			target->config2 = 0x00;
			target->config3 = 0x00;
		}

		eeprom->scsi_id = 7;
		eeprom->channel_cfg = 0x0F;
		eeprom->delay_time = 0;
		eeprom->max_tag = 4;
		eeprom->reserved1 = 0x15;
		eeprom->boot_target = 0;
		eeprom->boot_lun = 0;
		eeprom->reserved2 = 0;
		memset(eeprom->reserved3, 0, sizeof(eeprom->reserved3));

		chksum = 0;
		ep = (uint16_t *)eeprom;
		for (i = 0; i < 63; i++)
			chksum += le16toh(*ep++);

		chksum = TRM_NVRAM_CKSUM - chksum;
		eeprom->checksum0 = chksum & 0xFF;
		eeprom->checksum1 = chksum >> 8;

		trm_eeprom_write_all(sc, eeprom);
	}
}

/*
 * write struct eeprom 128 bytes to seeprom
 */
static void
trm_eeprom_write_all(struct trm_softc *sc, struct trm_nvram *eeprom)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	uint8_t *sbuf = (uint8_t *)eeprom;
	uint8_t addr;

	/* Enable SEEPROM */
	bus_space_write_1(iot, ioh, TRM_GEN_CONTROL,
	    bus_space_read_1(iot, ioh, TRM_GEN_CONTROL) | EN_EEPROM);

	/*
	 * Write enable
	 */
	trm_eeprom_write_cmd(sc, 0x04, 0xFF);
	bus_space_write_1(iot, ioh, TRM_GEN_NVRAM, 0);
	trm_eeprom_wait();

	for (addr = 0; addr < 128; addr++, sbuf++)
		trm_eeprom_set_data(sc, addr, *sbuf);

	/*
	 * Write disable
	 */
	trm_eeprom_write_cmd(sc, 0x04, 0x00);
	bus_space_write_1(iot, ioh, TRM_GEN_NVRAM, 0);
	trm_eeprom_wait();

	/* Disable SEEPROM */
	bus_space_write_1(iot, ioh, TRM_GEN_CONTROL,
	    bus_space_read_1(iot, ioh, TRM_GEN_CONTROL) & ~EN_EEPROM);
}

/*
 * write one byte to seeprom
 */
static void
trm_eeprom_set_data(struct trm_softc *sc, uint8_t addr, uint8_t data)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i;
	uint8_t send;

	/*
	 * Send write command & address
	 */
	trm_eeprom_write_cmd(sc, 0x05, addr);
	/*
	 * Write data
	 */
	for (i = 0; i < 8; i++, data <<= 1) {
		send = NVR_SELECT;
		if (data & 0x80)	/* Start from bit 7 */
			send |= NVR_BITOUT;

		bus_space_write_1(iot, ioh, TRM_GEN_NVRAM, send);
		trm_eeprom_wait();
		bus_space_write_1(iot, ioh, TRM_GEN_NVRAM, send | NVR_CLOCK);
		trm_eeprom_wait();
	}
	bus_space_write_1(iot, ioh, TRM_GEN_NVRAM, NVR_SELECT);
	trm_eeprom_wait();
	/*
	 * Disable chip select
	 */
	bus_space_write_1(iot, ioh, TRM_GEN_NVRAM, 0);
	trm_eeprom_wait();
	bus_space_write_1(iot, ioh, TRM_GEN_NVRAM, NVR_SELECT);
	trm_eeprom_wait();
	/*
	 * Wait for write ready
	 */
	for (;;) {
		bus_space_write_1(iot, ioh, TRM_GEN_NVRAM,
		    NVR_SELECT | NVR_CLOCK);
		trm_eeprom_wait();
		bus_space_write_1(iot, ioh, TRM_GEN_NVRAM, NVR_SELECT);
		trm_eeprom_wait();
		if (bus_space_read_1(iot, ioh, TRM_GEN_NVRAM) & NVR_BITIN)
			break;
	}
	/*
	 * Disable chip select
	 */
	bus_space_write_1(iot, ioh, TRM_GEN_NVRAM, 0);
}

/*
 * read seeprom 128 bytes to struct eeprom
 */
static void
trm_eeprom_read_all(struct trm_softc *sc, struct trm_nvram *eeprom)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	uint8_t *sbuf = (uint8_t *)eeprom;
	uint8_t addr;

	/*
	 * Enable SEEPROM
	 */
	bus_space_write_1(iot, ioh, TRM_GEN_CONTROL,
	    bus_space_read_1(iot, ioh, TRM_GEN_CONTROL) | EN_EEPROM);

	for (addr = 0; addr < 128; addr++)
		*sbuf++ = trm_eeprom_get_data(sc, addr);

	/*
	 * Disable SEEPROM
	 */
	bus_space_write_1(iot, ioh, TRM_GEN_CONTROL,
	    bus_space_read_1(iot, ioh, TRM_GEN_CONTROL) & ~EN_EEPROM);
}

/*
 * read one byte from seeprom
 */
static uint8_t
trm_eeprom_get_data(struct trm_softc *sc, uint8_t addr)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i;
	uint8_t read, data = 0;

	/*
	 * Send read command & address
	 */
	trm_eeprom_write_cmd(sc, 0x06, addr);

	for (i = 0; i < 8; i++) { /* Read data */
		bus_space_write_1(iot, ioh, TRM_GEN_NVRAM,
		    NVR_SELECT | NVR_CLOCK);
		trm_eeprom_wait();
		bus_space_write_1(iot, ioh, TRM_GEN_NVRAM, NVR_SELECT);
		/*
		 * Get data bit while falling edge
		 */
		read = bus_space_read_1(iot, ioh, TRM_GEN_NVRAM);
		data <<= 1;
		if (read & NVR_BITIN)
			data |= 1;

		trm_eeprom_wait();
	}
	/*
	 * Disable chip select
	 */
	bus_space_write_1(iot, ioh, TRM_GEN_NVRAM, 0);
	return data;
}

/*
 * write SB and Op Code into seeprom
 */
static void
trm_eeprom_write_cmd(struct trm_softc *sc, uint8_t cmd, uint8_t addr)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i;
	uint8_t send;

	/* Program SB+OP code */
	for (i = 0; i < 3; i++, cmd <<= 1) {
		send = NVR_SELECT;
		if (cmd & 0x04)	/* Start from bit 2 */
			send |= NVR_BITOUT;

		bus_space_write_1(iot, ioh, TRM_GEN_NVRAM, send);
		trm_eeprom_wait();
		bus_space_write_1(iot, ioh, TRM_GEN_NVRAM, send | NVR_CLOCK);
		trm_eeprom_wait();
	}

	/* Program address */
	for (i = 0; i < 7; i++, addr <<= 1) {
		send = NVR_SELECT;
		if (addr & 0x40)	/* Start from bit 6 */
			send |= NVR_BITOUT;

		bus_space_write_1(iot, ioh, TRM_GEN_NVRAM, send);
		trm_eeprom_wait();
		bus_space_write_1(iot, ioh, TRM_GEN_NVRAM, send | NVR_CLOCK);
		trm_eeprom_wait();
	}
	bus_space_write_1(iot, ioh, TRM_GEN_NVRAM, NVR_SELECT);
	trm_eeprom_wait();
}

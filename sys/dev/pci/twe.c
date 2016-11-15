/*	$NetBSD: twe.c,v 1.103 2014/07/25 08:10:38 dholland Exp $	*/

/*-
 * Copyright (c) 2000, 2001, 2002, 2003, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran; and by Jason R. Thorpe of Wasabi Systems, Inc.
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

/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from FreeBSD: twe.c,v 1.1 2000/05/24 23:35:23 msmith Exp
 */

/*
 * Driver for the 3ware Escalade family of RAID controllers.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: twe.c,v 1.103 2014/07/25 08:10:38 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/endian.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/kauth.h>

#include <sys/bswap.h>
#include <sys/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/twereg.h>
#include <dev/pci/twevar.h>
#include <dev/pci/tweio.h>

#include "locators.h"

#define	PCI_CBIO	0x10

static int	twe_aen_get(struct twe_softc *, uint16_t *);
static void	twe_aen_handler(struct twe_ccb *, int);
static void	twe_aen_enqueue(struct twe_softc *sc, uint16_t, int);
static uint16_t	twe_aen_dequeue(struct twe_softc *);

static void	twe_attach(device_t, device_t, void *);
static int	twe_init_connection(struct twe_softc *);
static int	twe_intr(void *);
static int	twe_match(device_t, cfdata_t, void *);
static int	twe_param_set(struct twe_softc *, int, int, size_t, void *);
static void	twe_poll(struct twe_softc *);
static int	twe_print(void *, const char *);
static int	twe_reset(struct twe_softc *);
static int	twe_status_check(struct twe_softc *, u_int);
static int	twe_status_wait(struct twe_softc *, u_int, int);
static void	twe_describe_controller(struct twe_softc *);
static void twe_clear_pci_abort(struct twe_softc *sc);
static void twe_clear_pci_parity_error(struct twe_softc *sc);

static int	twe_add_unit(struct twe_softc *, int);
static int	twe_del_unit(struct twe_softc *, int);
static int	twe_init_connection(struct twe_softc *);

static inline u_int32_t	twe_inl(struct twe_softc *, int);
static inline void twe_outl(struct twe_softc *, int, u_int32_t);

extern struct	cfdriver twe_cd;

CFATTACH_DECL_NEW(twe, sizeof(struct twe_softc),
    twe_match, twe_attach, NULL, NULL);

/* FreeBSD driver revision for sysctl expected by the 3ware cli */
const char twever[] = "1.50.01.002";

/*
 * Tables to convert numeric codes to strings.
 */
const struct twe_code_table twe_table_status[] = {
	{ 0x00,	"successful completion" },

	/* info */
	{ 0x42,	"command in progress" },
	{ 0x6c,	"retrying interface CRC error from UDMA command" },

	/* warning */
	{ 0x81,	"redundant/inconsequential request ignored" },
	{ 0x8e,	"failed to write zeroes to LBA 0" },
	{ 0x8f,	"failed to profile TwinStor zones" },

	/* fatal */
	{ 0xc1,	"aborted due to system command or reconfiguration" },
	{ 0xc4,	"aborted" },
	{ 0xc5,	"access error" },
	{ 0xc6,	"access violation" },
	{ 0xc7,	"device failure" },	/* high byte may be port # */
	{ 0xc8,	"controller error" },
	{ 0xc9,	"timed out" },
	{ 0xcb,	"invalid unit number" },
	{ 0xcf,	"unit not available" },
	{ 0xd2,	"undefined opcode" },
	{ 0xdb,	"request incompatible with unit" },
	{ 0xdc,	"invalid request" },
	{ 0xff,	"firmware error, reset requested" },

	{ 0,	NULL }
};

const struct twe_code_table twe_table_unitstate[] = {
	{ TWE_PARAM_UNITSTATUS_Normal,		"Normal" },
	{ TWE_PARAM_UNITSTATUS_Initialising,	"Initializing" },
	{ TWE_PARAM_UNITSTATUS_Degraded,	"Degraded" },
	{ TWE_PARAM_UNITSTATUS_Rebuilding,	"Rebuilding" },
	{ TWE_PARAM_UNITSTATUS_Verifying,	"Verifying" },
	{ TWE_PARAM_UNITSTATUS_Corrupt,		"Corrupt" },
	{ TWE_PARAM_UNITSTATUS_Missing,		"Missing" },

	{ 0,					NULL }
};

const struct twe_code_table twe_table_unittype[] = {
	/* array descriptor configuration */
	{ TWE_AD_CONFIG_RAID0,			"RAID0" },
	{ TWE_AD_CONFIG_RAID1,			"RAID1" },
	{ TWE_AD_CONFIG_TwinStor,		"TwinStor" },
	{ TWE_AD_CONFIG_RAID5,			"RAID5" },
	{ TWE_AD_CONFIG_RAID10,			"RAID10" },
	{ TWE_UD_CONFIG_JBOD,			"JBOD" },

	{ 0,					NULL }
};

const struct twe_code_table twe_table_stripedepth[] = {
	{ TWE_AD_STRIPE_4k,			"4K" },
	{ TWE_AD_STRIPE_8k,			"8K" },
	{ TWE_AD_STRIPE_16k,			"16K" },
	{ TWE_AD_STRIPE_32k,			"32K" },
	{ TWE_AD_STRIPE_64k,			"64K" },
	{ TWE_AD_STRIPE_128k,			"128K" },
	{ TWE_AD_STRIPE_256k,			"256K" },
	{ TWE_AD_STRIPE_512k,			"512K" },
	{ TWE_AD_STRIPE_1024k,			"1024K" },

	{ 0,					NULL }
};

/*
 * Asynchronous event notification messages are qualified:
 *	a - not unit/port specific
 *	u - unit specific
 *	p - port specific
 *
 * They are further qualified with a severity:
 *	E - LOG_EMERG
 *	a - LOG_ALERT
 *	c - LOG_CRIT
 *	e - LOG_ERR
 *	w - LOG_WARNING
 *	n - LOG_NOTICE
 *	i - LOG_INFO
 *	d - LOG_DEBUG
 *	blank - just use printf
 */
const struct twe_code_table twe_table_aen[] = {
	{ 0x00,	"a  queue empty" },
	{ 0x01,	"a  soft reset" },
	{ 0x02,	"uc degraded mode" },
	{ 0x03,	"aa controller error" },
	{ 0x04,	"uE rebuild fail" },
	{ 0x05,	"un rebuild done" },
	{ 0x06,	"ue incomplete unit" },
	{ 0x07,	"un initialization done" },
	{ 0x08,	"uw unclean shutdown detected" },
	{ 0x09,	"pe drive timeout" },
	{ 0x0a,	"pc drive error" },
	{ 0x0b,	"un rebuild started" },
	{ 0x0c,	"un initialization started" },
	{ 0x0d,	"ui logical unit deleted" },
	{ 0x0f,	"pc SMART threshold exceeded" },
	{ 0x15,	"a  table undefined" },	/* XXX: Not in FreeBSD's table */
	{ 0x21,	"pe ATA UDMA downgrade" },
	{ 0x22,	"pi ATA UDMA upgrade" },
	{ 0x23,	"pw sector repair occurred" },
	{ 0x24,	"aa SBUF integrity check failure" },
	{ 0x25,	"pa lost cached write" },
	{ 0x26,	"pa drive ECC error detected" },
	{ 0x27,	"pe DCB checksum error" },
	{ 0x28,	"pn DCB unsupported version" },
	{ 0x29,	"ui verify started" },
	{ 0x2a,	"ua verify failed" },
	{ 0x2b,	"ui verify complete" },
	{ 0x2c,	"pw overwrote bad sector during rebuild" },
	{ 0x2d,	"pa encountered bad sector during rebuild" },
	{ 0x2e,	"pe replacement drive too small" },
	{ 0x2f,	"ue array not previously initialized" },
	{ 0x30,	"p  drive not supported" },
	{ 0xff,	"a  aen queue full" },

	{ 0,	NULL },
};

const char *
twe_describe_code(const struct twe_code_table *table, uint32_t code)
{

	for (; table->string != NULL; table++) {
		if (table->code == code)
			return (table->string);
	}
	return (NULL);
}

static inline u_int32_t
twe_inl(struct twe_softc *sc, int off)
{

	bus_space_barrier(sc->sc_iot, sc->sc_ioh, off, 4,
	    BUS_SPACE_BARRIER_WRITE | BUS_SPACE_BARRIER_READ);
	return (bus_space_read_4(sc->sc_iot, sc->sc_ioh, off));
}

static inline void
twe_outl(struct twe_softc *sc, int off, u_int32_t val)
{

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, off, val);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, off, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

/*
 * Match a supported board.
 */
static int
twe_match(device_t parent, cfdata_t cfdata, void *aux)
{
	struct pci_attach_args *pa;

	pa = aux;

	return (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_3WARE &&
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_3WARE_ESCALADE ||
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_3WARE_ESCALADE_ASIC));
}

/*
 * Attach a supported board.
 *
 * XXX This doesn't fail gracefully.
 */
static void
twe_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa;
	struct twe_softc *sc;
	pci_chipset_tag_t pc;
	pci_intr_handle_t ih;
	pcireg_t csr;
	const char *intrstr;
	int s, size, i, rv, rseg;
	size_t max_segs, max_xfer;
	bus_dma_segment_t seg;
	const struct sysctlnode *node;
	struct twe_cmd *tc;
	struct twe_ccb *ccb;
	char intrbuf[PCI_INTRSTR_LEN];

	sc = device_private(self);
	sc->sc_dev = self;
	pa = aux;
	pc = pa->pa_pc;
	sc->sc_dmat = pa->pa_dmat;
	SIMPLEQ_INIT(&sc->sc_ccb_queue);
	SLIST_INIT(&sc->sc_ccb_freelist);

	aprint_naive(": RAID controller\n");
	aprint_normal(": 3ware Escalade\n");


	if (pci_mapreg_map(pa, PCI_CBIO, PCI_MAPREG_TYPE_IO, 0,
	    &sc->sc_iot, &sc->sc_ioh, NULL, NULL)) {
		aprint_error_dev(self, "can't map i/o space\n");
		return;
	}

	/* Enable the device. */
	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MASTER_ENABLE);

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "can't map interrupt\n");
		return;
	}

	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_BIO, twe_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "can't establish interrupt%s%s\n",
			(intrstr) ? " at " : "",
			(intrstr) ? intrstr : "");
		return;
	}

	if (intrstr != NULL)
		aprint_normal_dev(self, "interrupting at %s\n",
			intrstr);

	/*
	 * Allocate and initialise the command blocks and CCBs.
	 */
	size = sizeof(struct twe_cmd) * TWE_MAX_QUEUECNT;

	if ((rv = bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &seg, 1,
	    &rseg, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(self, "unable to allocate commands, rv = %d\n", rv);
		return;
	}

	if ((rv = bus_dmamem_map(sc->sc_dmat, &seg, rseg, size,
	    (void **)&sc->sc_cmds,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(self, "unable to map commands, rv = %d\n", rv);
		return;
	}

	if ((rv = bus_dmamap_create(sc->sc_dmat, size, size, 1, 0,
	    BUS_DMA_NOWAIT, &sc->sc_dmamap)) != 0) {
		aprint_error_dev(self, "unable to create command DMA map, rv = %d\n", rv);
		return;
	}

	if ((rv = bus_dmamap_load(sc->sc_dmat, sc->sc_dmamap, sc->sc_cmds,
	    size, NULL, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(self, "unable to load command DMA map, rv = %d\n", rv);
		return;
	}

	ccb = malloc(sizeof(*ccb) * TWE_MAX_QUEUECNT, M_DEVBUF, M_NOWAIT);
	if (ccb == NULL) {
		aprint_error_dev(self, "unable to allocate memory for ccbs\n");
		return;
	}

	sc->sc_cmds_paddr = sc->sc_dmamap->dm_segs[0].ds_addr;
	memset(sc->sc_cmds, 0, size);

	sc->sc_ccbs = ccb;
	tc = (struct twe_cmd *)sc->sc_cmds;
	max_segs = twe_get_maxsegs();
	max_xfer = twe_get_maxxfer(max_segs);

	for (i = 0; i < TWE_MAX_QUEUECNT; i++, tc++, ccb++) {
		ccb->ccb_cmd = tc;
		ccb->ccb_cmdid = i;
		ccb->ccb_flags = 0;
		rv = bus_dmamap_create(sc->sc_dmat, max_xfer,
		    max_segs, PAGE_SIZE, 0,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &ccb->ccb_dmamap_xfer);
		if (rv != 0) {
			aprint_error_dev(self, "can't create dmamap, rv = %d\n", rv);
			return;
		}

		/* Save the first CCB for AEN retrieval. */
		if (i != 0)
			SLIST_INSERT_HEAD(&sc->sc_ccb_freelist, ccb,
			    ccb_chain.slist);
	}

	/* Wait for the controller to become ready. */
	if (twe_status_wait(sc, TWE_STS_MICROCONTROLLER_READY, 6)) {
		aprint_error_dev(self, "microcontroller not ready\n");
		return;
	}

	twe_outl(sc, TWE_REG_CTL, TWE_CTL_DISABLE_INTRS);

	/* Reset the controller. */
	s = splbio();
	rv = twe_reset(sc);
	splx(s);
	if (rv) {
		aprint_error_dev(self, "reset failed\n");
		return;
	}

	/* Initialise connection with controller. */
	twe_init_connection(sc);

	twe_describe_controller(sc);

	/* Find and attach RAID array units. */
	sc->sc_nunits = 0;
	for (i = 0; i < TWE_MAX_UNITS; i++)
		(void) twe_add_unit(sc, i);

	/* ...and finally, enable interrupts. */
	twe_outl(sc, TWE_REG_CTL, TWE_CTL_CLEAR_ATTN_INTR |
	    TWE_CTL_UNMASK_RESP_INTR |
	    TWE_CTL_ENABLE_INTRS);

	/* sysctl set-up for 3ware cli */
	if (sysctl_createv(NULL, 0, NULL, &node,
				0, CTLTYPE_NODE, device_xname(self),
				SYSCTL_DESCR("twe driver information"),
				NULL, 0, NULL, 0,
				CTL_HW, CTL_CREATE, CTL_EOL) != 0) {
		aprint_error_dev(self, "could not create %s.%s sysctl node\n",
			"hw", device_xname(self));
		return;
	}
	if ((i = sysctl_createv(NULL, 0, NULL, NULL,
				0, CTLTYPE_STRING, "driver_version",
				SYSCTL_DESCR("twe0 driver version"),
				NULL, 0, __UNCONST(&twever), 0,
				CTL_HW, node->sysctl_num, CTL_CREATE, CTL_EOL))
				!= 0) {
		aprint_error_dev(self, "could not create %s.%s.driver_version sysctl\n",
			"hw", device_xname(self));
		return;
	}
}

void
twe_register_callbacks(struct twe_softc *sc, int unit,
    const struct twe_callbacks *tcb)
{

	sc->sc_units[unit].td_callbacks = tcb;
}

static void
twe_recompute_openings(struct twe_softc *sc)
{
	struct twe_drive *td;
	int unit, openings;

	if (sc->sc_nunits != 0)
		openings = (TWE_MAX_QUEUECNT - 1) / sc->sc_nunits;
	else
		openings = 0;
	if (openings == sc->sc_openings)
		return;
	sc->sc_openings = openings;

#ifdef TWE_DEBUG
	printf("%s: %d array%s, %d openings per array\n",
	    device_xname(sc->sc_dev), sc->sc_nunits,
	    sc->sc_nunits == 1 ? "" : "s", sc->sc_openings);
#endif

	for (unit = 0; unit < TWE_MAX_UNITS; unit++) {
		td = &sc->sc_units[unit];
		if (td->td_dev != NULL)
			(*td->td_callbacks->tcb_openings)(td->td_dev,
			    sc->sc_openings);
	}
}

static int
twe_add_unit(struct twe_softc *sc, int unit)
{
	struct twe_param *dtp, *atp;
	struct twe_array_descriptor *ad;
	struct twe_drive *td;
	struct twe_attach_args twea;
	uint32_t newsize;
	int rv;
	uint16_t dsize;
	uint8_t newtype, newstripe;
	int locs[TWECF_NLOCS];

	if (unit < 0 || unit >= TWE_MAX_UNITS)
		return (EINVAL);

	/* Find attached units. */
	rv = twe_param_get(sc, TWE_PARAM_UNITSUMMARY,
	    TWE_PARAM_UNITSUMMARY_Status, TWE_MAX_UNITS, NULL, &dtp);
	if (rv != 0) {
		aprint_error_dev(sc->sc_dev, "error %d fetching unit summary\n",
		    rv);
		return (rv);
	}

	/* For each detected unit, collect size and store in an array. */
	td = &sc->sc_units[unit];

	/* Unit present? */
	if ((dtp->tp_data[unit] & TWE_PARAM_UNITSTATUS_Online) == 0) {
		/*
		 * XXX Should we check to see if a device has been
		 * XXX attached at this index and detach it if it
		 * XXX has?  ("rescan" semantics)
		 */
		rv = 0;
		goto out;
   	}

	rv = twe_param_get_2(sc, TWE_PARAM_UNITINFO + unit,
	    TWE_PARAM_UNITINFO_DescriptorSize, &dsize);
	if (rv != 0) {
		aprint_error_dev(sc->sc_dev, "error %d fetching descriptor size "
		    "for unit %d\n", rv, unit);
		goto out;
	}

	rv = twe_param_get(sc, TWE_PARAM_UNITINFO + unit,
	    TWE_PARAM_UNITINFO_Descriptor, dsize - 3, NULL, &atp);
	if (rv != 0) {
		aprint_error_dev(sc->sc_dev, "error %d fetching array descriptor "
		    "for unit %d\n", rv, unit);
		goto out;
	}

	ad = (struct twe_array_descriptor *)atp->tp_data;
	newtype = ad->configuration;
	newstripe = ad->stripe_size;
	free(atp, M_DEVBUF);

	rv = twe_param_get_4(sc, TWE_PARAM_UNITINFO + unit,
	    TWE_PARAM_UNITINFO_Capacity, &newsize);
	if (rv != 0) {
		aprint_error_dev(sc->sc_dev,
		    "error %d fetching capacity for unit %d\n",
		    rv, unit);
		goto out;
	}

	/*
	 * Have a device, so we need to attach it.  If there is currently
	 * something sitting at the slot, and the parameters are different,
	 * then we detach the old device before attaching the new one.
	 */
	if (td->td_dev != NULL &&
	    td->td_size == newsize &&
	    td->td_type == newtype &&
	    td->td_stripe == newstripe) {
		/* Same as the old device; just keep using it. */
		rv = 0;
		goto out;
	} else if (td->td_dev != NULL) {
		/* Detach the old device first. */
		(void) config_detach(td->td_dev, DETACH_FORCE);
		td->td_dev = NULL;
	} else if (td->td_size == 0)
		sc->sc_nunits++;

	/*
	 * Committed to the new array unit; assign its parameters and
	 * recompute the number of available command openings.
	 */
	td->td_size = newsize;
	td->td_type = newtype;
	td->td_stripe = newstripe;
	twe_recompute_openings(sc);

	twea.twea_unit = unit;

	locs[TWECF_UNIT] = unit;

	td->td_dev = config_found_sm_loc(sc->sc_dev, "twe", locs, &twea,
					 twe_print, config_stdsubmatch);

	rv = 0;
 out:
	free(dtp, M_DEVBUF);
	return (rv);
}

static int
twe_del_unit(struct twe_softc *sc, int unit)
{
	struct twe_drive *td;

	if (unit < 0 || unit >= TWE_MAX_UNITS)
		return (EINVAL);

	td = &sc->sc_units[unit];
	if (td->td_size != 0)
		sc->sc_nunits--;
	td->td_size = 0;
	td->td_type = 0;
	td->td_stripe = 0;
	if (td->td_dev != NULL) {
		(void) config_detach(td->td_dev, DETACH_FORCE);
		td->td_dev = NULL;
	}
	twe_recompute_openings(sc);
	return (0);
}

/*
 * Reset the controller.
 * MUST BE CALLED AT splbio()!
 */
static int
twe_reset(struct twe_softc *sc)
{
	uint16_t aen;
	u_int status;
	int got, rv;

	/* Issue a soft reset. */
	twe_outl(sc, TWE_REG_CTL, TWE_CTL_ISSUE_SOFT_RESET |
	    TWE_CTL_CLEAR_HOST_INTR |
	    TWE_CTL_CLEAR_ATTN_INTR |
	    TWE_CTL_MASK_CMD_INTR |
	    TWE_CTL_MASK_RESP_INTR |
	    TWE_CTL_CLEAR_ERROR_STS |
	    TWE_CTL_DISABLE_INTRS);

	/* Wait for attention... */
	if (twe_status_wait(sc, TWE_STS_ATTN_INTR, 30)) {
		aprint_error_dev(sc->sc_dev, "timeout waiting for attention interrupt\n");
		return (-1);
	}

	/* ...and ACK it. */
	twe_outl(sc, TWE_REG_CTL, TWE_CTL_CLEAR_ATTN_INTR);

	/*
	 * Pull AENs out of the controller; look for a soft reset AEN.
	 * Open code this, since we want to detect reset even if the
	 * queue for management tools is full.
	 *
	 * Note that since:
	 *	- interrupts are blocked
	 *	- we have reset the controller
	 *	- acknowledged the pending ATTENTION
	 * that there is no way a pending asynchronous AEN fetch would
	 * finish, so clear the flag.
	 */
	sc->sc_flags &= ~TWEF_AEN;
	for (got = 0;;) {
		rv = twe_aen_get(sc, &aen);
		if (rv != 0)
			printf("%s: error %d while draining event queue\n",
			    device_xname(sc->sc_dev), rv);
		if (TWE_AEN_CODE(aen) == TWE_AEN_QUEUE_EMPTY)
			break;
		if (TWE_AEN_CODE(aen) == TWE_AEN_SOFT_RESET)
			got = 1;
		twe_aen_enqueue(sc, aen, 1);
	}

	if (!got) {
		printf("%s: reset not reported\n", device_xname(sc->sc_dev));
		return (-1);
	}

	/* Check controller status. */
	status = twe_inl(sc, TWE_REG_STS);
	if (twe_status_check(sc, status)) {
		printf("%s: controller errors detected\n",
		    device_xname(sc->sc_dev));
		return (-1);
	}

	/* Drain the response queue. */
	for (;;) {
		status = twe_inl(sc, TWE_REG_STS);
		if (twe_status_check(sc, status) != 0) {
			aprint_error_dev(sc->sc_dev, "can't drain response queue\n");
			return (-1);
		}
		if ((status & TWE_STS_RESP_QUEUE_EMPTY) != 0)
			break;
		(void)twe_inl(sc, TWE_REG_RESP_QUEUE);
	}

	return (0);
}

/*
 * Print autoconfiguration message for a sub-device.
 */
static int
twe_print(void *aux, const char *pnp)
{
	struct twe_attach_args *twea;

	twea = aux;

	if (pnp != NULL)
		aprint_normal("block device at %s", pnp);
	aprint_normal(" unit %d", twea->twea_unit);
	return (UNCONF);
}

/*
 * Interrupt service routine.
 */
static int
twe_intr(void *arg)
{
	struct twe_softc *sc;
	u_int status;
	int caught, rv;

	sc = arg;
	caught = 0;
	status = twe_inl(sc, TWE_REG_STS);
	twe_status_check(sc, status);

	/* Host interrupts - purpose unknown. */
	if ((status & TWE_STS_HOST_INTR) != 0) {
#ifdef DEBUG
		printf("%s: host interrupt\n", device_xname(sc->sc_dev));
#endif
		twe_outl(sc, TWE_REG_CTL, TWE_CTL_CLEAR_HOST_INTR);
		caught = 1;
	}

	/*
	 * Attention interrupts, signalled when a controller or child device
	 * state change has occurred.
	 */
	if ((status & TWE_STS_ATTN_INTR) != 0) {
		rv = twe_aen_get(sc, NULL);
		if (rv != 0)
			aprint_error_dev(sc->sc_dev, "unable to retrieve AEN (%d)\n", rv);
		else
			twe_outl(sc, TWE_REG_CTL, TWE_CTL_CLEAR_ATTN_INTR);
		caught = 1;
	}

	/*
	 * Command interrupts, signalled when the controller can accept more
	 * commands.  We don't use this; instead, we try to submit commands
	 * when we receive them, and when other commands have completed.
	 * Mask it so we don't get another one.
	 */
	if ((status & TWE_STS_CMD_INTR) != 0) {
#ifdef DEBUG
		printf("%s: command interrupt\n", device_xname(sc->sc_dev));
#endif
		twe_outl(sc, TWE_REG_CTL, TWE_CTL_MASK_CMD_INTR);
		caught = 1;
	}

	if ((status & TWE_STS_RESP_INTR) != 0) {
		twe_poll(sc);
		caught = 1;
	}

	return (caught);
}

/*
 * Fetch an AEN.  Even though this is really like parameter
 * retrieval, we handle this specially, because we issue this
 * AEN retrieval command from interrupt context, and thus
 * reserve a CCB for it to avoid resource shortage.
 *
 * XXX There are still potential resource shortages we could
 * XXX encounter.  Consider pre-allocating all AEN-related
 * XXX resources.
 *
 * MUST BE CALLED AT splbio()!
 */
static int
twe_aen_get(struct twe_softc *sc, uint16_t *aenp)
{
	struct twe_ccb *ccb;
	struct twe_cmd *tc;
	struct twe_param *tp;
	int rv;

	/*
	 * If we're already retrieving an AEN, just wait; another
	 * retrieval will be chained after the current one completes.
	 */
	if (sc->sc_flags & TWEF_AEN) {
		/*
		 * It is a fatal software programming error to attempt
		 * to fetch an AEN synchronously when an AEN fetch is
		 * already pending.
		 */
		KASSERT(aenp == NULL);
		return (0);
	}

	tp = malloc(TWE_SECTOR_SIZE, M_DEVBUF, M_NOWAIT);
	if (tp == NULL)
		return (ENOMEM);

	ccb = twe_ccb_alloc(sc,
	    TWE_CCB_AEN | TWE_CCB_DATA_IN | TWE_CCB_DATA_OUT);
	KASSERT(ccb != NULL);

	ccb->ccb_data = tp;
	ccb->ccb_datasize = TWE_SECTOR_SIZE;
	ccb->ccb_tx.tx_handler = (aenp == NULL) ? twe_aen_handler : NULL;
	ccb->ccb_tx.tx_context = tp;
	ccb->ccb_tx.tx_dv = sc->sc_dev;

	tc = ccb->ccb_cmd;
	tc->tc_size = 2;
	tc->tc_opcode = TWE_OP_GET_PARAM | (tc->tc_size << 5);
	tc->tc_unit = 0;
	tc->tc_count = htole16(1);

	/* Fill in the outbound parameter data. */
	tp->tp_table_id = htole16(TWE_PARAM_AEN);
	tp->tp_param_id = TWE_PARAM_AEN_UnitCode;
	tp->tp_param_size = 2;

	/* Map the transfer. */
	if ((rv = twe_ccb_map(sc, ccb)) != 0) {
		twe_ccb_free(sc, ccb);
		goto done;
	}

	/* Enqueue the command and wait. */
	if (aenp != NULL) {
		rv = twe_ccb_poll(sc, ccb, 5);
		twe_ccb_unmap(sc, ccb);
		twe_ccb_free(sc, ccb);
		if (rv == 0)
			*aenp = le16toh(*(uint16_t *)tp->tp_data);
		free(tp, M_DEVBUF);
	} else {
		sc->sc_flags |= TWEF_AEN;
		twe_ccb_enqueue(sc, ccb);
		rv = 0;
	}

 done:
	return (rv);
}

/*
 * Handle an AEN returned by the controller.
 * MUST BE CALLED AT splbio()!
 */
static void
twe_aen_handler(struct twe_ccb *ccb, int error)
{
	struct twe_softc *sc;
	struct twe_param *tp;
	uint16_t aen;
	int rv;

	sc = device_private(ccb->ccb_tx.tx_dv);
	tp = ccb->ccb_tx.tx_context;
	twe_ccb_unmap(sc, ccb);

	sc->sc_flags &= ~TWEF_AEN;

	if (error) {
		aprint_error_dev(sc->sc_dev, "error retrieving AEN\n");
		aen = TWE_AEN_QUEUE_EMPTY;
	} else
		aen = le16toh(*(u_int16_t *)tp->tp_data);
	free(tp, M_DEVBUF);
	twe_ccb_free(sc, ccb);

	if (TWE_AEN_CODE(aen) == TWE_AEN_QUEUE_EMPTY) {
		twe_outl(sc, TWE_REG_CTL, TWE_CTL_CLEAR_ATTN_INTR);
		return;
	}

	twe_aen_enqueue(sc, aen, 0);

	/*
	 * Chain another retrieval in case interrupts have been
	 * coalesced.
	 */
	rv = twe_aen_get(sc, NULL);
	if (rv != 0)
		aprint_error_dev(sc->sc_dev, "unable to retrieve AEN (%d)\n", rv);
}

static void
twe_aen_enqueue(struct twe_softc *sc, uint16_t aen, int quiet)
{
	const char *str, *msg;
	int s, next, nextnext, level;

	/*
	 * First report the AEN on the console.  Maybe.
	 */
	if (! quiet) {
		str = twe_describe_code(twe_table_aen, TWE_AEN_CODE(aen));
		if (str == NULL) {
			aprint_error_dev(sc->sc_dev, "unknown AEN 0x%04x\n", aen);
		} else {
			msg = str + 3;
			switch (str[1]) {
			case 'E':	level = LOG_EMERG; break;
			case 'a':	level = LOG_ALERT; break;
			case 'c':	level = LOG_CRIT; break;
			case 'e':	level = LOG_ERR; break;
			case 'w':	level = LOG_WARNING; break;
			case 'n':	level = LOG_NOTICE; break;
			case 'i':	level = LOG_INFO; break;
			case 'd':	level = LOG_DEBUG; break;
			default:
				/* Don't use syslog. */
				level = -1;
			}

			if (level < 0) {
				switch (str[0]) {
				case 'u':
				case 'p':
					printf("%s: %s %d: %s\n",
					    device_xname(sc->sc_dev),
					    str[0] == 'u' ? "unit" : "port",
					    TWE_AEN_UNIT(aen), msg);
					break;

				default:
					printf("%s: %s\n",
					    device_xname(sc->sc_dev), msg);
				}
			} else {
				switch (str[0]) {
				case 'u':
				case 'p':
					log(level, "%s: %s %d: %s\n",
					    device_xname(sc->sc_dev),
					    str[0] == 'u' ? "unit" : "port",
					    TWE_AEN_UNIT(aen), msg);
					break;

				default:
					log(level, "%s: %s\n",
					    device_xname(sc->sc_dev), msg);
				}
			}
		}
	}

	/* Now enqueue the AEN for mangement tools. */
	s = splbio();

	next = (sc->sc_aen_head + 1) % TWE_AEN_Q_LENGTH;
	nextnext = (sc->sc_aen_head + 2) % TWE_AEN_Q_LENGTH;

	/*
	 * If this is the last free slot, then queue up a "queue
	 * full" message.
	 */
	if (nextnext == sc->sc_aen_tail)
		aen = TWE_AEN_QUEUE_FULL;

	if (next != sc->sc_aen_tail) {
		sc->sc_aen_queue[sc->sc_aen_head] = aen;
		sc->sc_aen_head = next;
	}

	if (sc->sc_flags & TWEF_AENQ_WAIT) {
		sc->sc_flags &= ~TWEF_AENQ_WAIT;
		wakeup(&sc->sc_aen_queue);
	}

	splx(s);
}

/* NOTE: Must be called at splbio(). */
static uint16_t
twe_aen_dequeue(struct twe_softc *sc)
{
	uint16_t aen;

	if (sc->sc_aen_tail == sc->sc_aen_head)
		aen = TWE_AEN_QUEUE_EMPTY;
	else {
		aen = sc->sc_aen_queue[sc->sc_aen_tail];
		sc->sc_aen_tail = (sc->sc_aen_tail + 1) % TWE_AEN_Q_LENGTH;
	}

	return (aen);
}

/*
 * These are short-hand functions that execute TWE_OP_GET_PARAM to
 * fetch 1, 2, and 4 byte parameter values, respectively.
 */
int
twe_param_get_1(struct twe_softc *sc, int table_id, int param_id,
    uint8_t *valp)
{
	struct twe_param *tp;
	int rv;

	rv = twe_param_get(sc, table_id, param_id, 1, NULL, &tp);
	if (rv != 0)
		return (rv);
	*valp = *(uint8_t *)tp->tp_data;
	free(tp, M_DEVBUF);
	return (0);
}

int
twe_param_get_2(struct twe_softc *sc, int table_id, int param_id,
    uint16_t *valp)
{
	struct twe_param *tp;
	int rv;

	rv = twe_param_get(sc, table_id, param_id, 2, NULL, &tp);
	if (rv != 0)
		return (rv);
	*valp = le16toh(*(uint16_t *)tp->tp_data);
	free(tp, M_DEVBUF);
	return (0);
}

int
twe_param_get_4(struct twe_softc *sc, int table_id, int param_id,
    uint32_t *valp)
{
	struct twe_param *tp;
	int rv;

	rv = twe_param_get(sc, table_id, param_id, 4, NULL, &tp);
	if (rv != 0)
		return (rv);
	*valp = le32toh(*(uint32_t *)tp->tp_data);
	free(tp, M_DEVBUF);
	return (0);
}

/*
 * Execute a TWE_OP_GET_PARAM command.  If a callback function is provided,
 * it will be called with generated context when the command has completed.
 * If no callback is provided, the command will be executed synchronously
 * and a pointer to a buffer containing the data returned.
 *
 * The caller or callback is responsible for freeing the buffer.
 *
 * NOTE: We assume we can sleep here to wait for a CCB to become available.
 */
int
twe_param_get(struct twe_softc *sc, int table_id, int param_id, size_t size,
	      void (*func)(struct twe_ccb *, int), struct twe_param **pbuf)
{
	struct twe_ccb *ccb;
	struct twe_cmd *tc;
	struct twe_param *tp;
	int rv, s;

	tp = malloc(TWE_SECTOR_SIZE, M_DEVBUF, M_NOWAIT);
	if (tp == NULL)
		return ENOMEM;

	ccb = twe_ccb_alloc_wait(sc, TWE_CCB_DATA_IN | TWE_CCB_DATA_OUT);
	KASSERT(ccb != NULL);

	ccb->ccb_data = tp;
	ccb->ccb_datasize = TWE_SECTOR_SIZE;
	ccb->ccb_tx.tx_handler = func;
	ccb->ccb_tx.tx_context = tp;
	ccb->ccb_tx.tx_dv = sc->sc_dev;

	tc = ccb->ccb_cmd;
	tc->tc_size = 2;
	tc->tc_opcode = TWE_OP_GET_PARAM | (tc->tc_size << 5);
	tc->tc_unit = 0;
	tc->tc_count = htole16(1);

	/* Fill in the outbound parameter data. */
	tp->tp_table_id = htole16(table_id);
	tp->tp_param_id = param_id;
	tp->tp_param_size = size;

	/* Map the transfer. */
	if ((rv = twe_ccb_map(sc, ccb)) != 0) {
		twe_ccb_free(sc, ccb);
		goto done;
	}

	/* Submit the command and either wait or let the callback handle it. */
	if (func == NULL) {
		s = splbio();
		rv = twe_ccb_poll(sc, ccb, 5);
		twe_ccb_unmap(sc, ccb);
		twe_ccb_free(sc, ccb);
		splx(s);
	} else {
#ifdef DEBUG
		if (pbuf != NULL)
			panic("both func and pbuf defined");
#endif
		twe_ccb_enqueue(sc, ccb);
		return 0;
	}

done:
	if (pbuf == NULL || rv != 0)
		free(tp, M_DEVBUF);
	else if (pbuf != NULL && rv == 0)
		*pbuf = tp;
	return rv;
}

/*
 * Execute a TWE_OP_SET_PARAM command.
 *
 * NOTE: We assume we can sleep here to wait for a CCB to become available.
 */
static int
twe_param_set(struct twe_softc *sc, int table_id, int param_id, size_t size,
	      void *sbuf)
{
	struct twe_ccb *ccb;
	struct twe_cmd *tc;
	struct twe_param *tp;
	int rv, s;

	tp = malloc(TWE_SECTOR_SIZE, M_DEVBUF, M_NOWAIT);
	if (tp == NULL)
		return ENOMEM;

	ccb = twe_ccb_alloc_wait(sc, TWE_CCB_DATA_IN | TWE_CCB_DATA_OUT);
	KASSERT(ccb != NULL);

	ccb->ccb_data = tp;
	ccb->ccb_datasize = TWE_SECTOR_SIZE;
	ccb->ccb_tx.tx_handler = 0;
	ccb->ccb_tx.tx_context = tp;
	ccb->ccb_tx.tx_dv = sc->sc_dev;

	tc = ccb->ccb_cmd;
	tc->tc_size = 2;
	tc->tc_opcode = TWE_OP_SET_PARAM | (tc->tc_size << 5);
	tc->tc_unit = 0;
	tc->tc_count = htole16(1);

	/* Fill in the outbound parameter data. */
	tp->tp_table_id = htole16(table_id);
	tp->tp_param_id = param_id;
	tp->tp_param_size = size;
	memcpy(tp->tp_data, sbuf, size);

	/* Map the transfer. */
	if ((rv = twe_ccb_map(sc, ccb)) != 0) {
		twe_ccb_free(sc, ccb);
		goto done;
	}

	/* Submit the command and wait. */
	s = splbio();
	rv = twe_ccb_poll(sc, ccb, 5);
	twe_ccb_unmap(sc, ccb);
	twe_ccb_free(sc, ccb);
	splx(s);
done:
	free(tp, M_DEVBUF);
	return (rv);
}

/*
 * Execute a TWE_OP_INIT_CONNECTION command.  Return non-zero on error.
 * Must be called with interrupts blocked.
 */
static int
twe_init_connection(struct twe_softc *sc)
{
	struct twe_ccb *ccb;
	struct twe_cmd *tc;
	int rv;

	if ((ccb = twe_ccb_alloc(sc, 0)) == NULL)
		return (EAGAIN);

	/* Build the command. */
	tc = ccb->ccb_cmd;
	tc->tc_size = 3;
	tc->tc_opcode = TWE_OP_INIT_CONNECTION;
	tc->tc_unit = 0;
	tc->tc_count = htole16(TWE_MAX_CMDS);
	tc->tc_args.init_connection.response_queue_pointer = 0;

	/* Submit the command for immediate execution. */
	rv = twe_ccb_poll(sc, ccb, 5);
	twe_ccb_free(sc, ccb);
	return (rv);
}

/*
 * Poll the controller for completed commands.  Must be called with
 * interrupts blocked.
 */
static void
twe_poll(struct twe_softc *sc)
{
	struct twe_ccb *ccb;
	int found;
	u_int status, cmdid;

	found = 0;

	for (;;) {
		status = twe_inl(sc, TWE_REG_STS);
		twe_status_check(sc, status);

		if ((status & TWE_STS_RESP_QUEUE_EMPTY))
			break;

		found = 1;
		cmdid = twe_inl(sc, TWE_REG_RESP_QUEUE);
		cmdid = (cmdid & TWE_RESP_MASK) >> TWE_RESP_SHIFT;
		if (cmdid >= TWE_MAX_QUEUECNT) {
			aprint_error_dev(sc->sc_dev, "bad cmdid %d\n", cmdid);
			continue;
		}

		ccb = sc->sc_ccbs + cmdid;
		if ((ccb->ccb_flags & TWE_CCB_ACTIVE) == 0) {
			printf("%s: CCB for cmdid %d not active\n",
			    device_xname(sc->sc_dev), cmdid);
			continue;
		}
		ccb->ccb_flags ^= TWE_CCB_COMPLETE | TWE_CCB_ACTIVE;

		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
		    (char *)ccb->ccb_cmd - (char *)sc->sc_cmds,
		    sizeof(struct twe_cmd),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		/* Pass notification to upper layers. */
		if (ccb->ccb_tx.tx_handler != NULL)
			(*ccb->ccb_tx.tx_handler)(ccb,
			    ccb->ccb_cmd->tc_status != 0 ? EIO : 0);
	}

	/* If any commands have completed, run the software queue. */
	if (found)
		twe_ccb_enqueue(sc, NULL);
}

/*
 * Wait for `status' to be set in the controller status register.  Return
 * zero if found, non-zero if the operation timed out.
 */
static int
twe_status_wait(struct twe_softc *sc, u_int32_t status, int timo)
{

	for (timo *= 10; timo != 0; timo--) {
		if ((twe_inl(sc, TWE_REG_STS) & status) == status)
			break;
		delay(100000);
	}

	return (timo == 0);
}

/*
 * Clear a PCI parity error.
 */
static void
twe_clear_pci_parity_error(struct twe_softc *sc)
{
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, 0x0, TWE_CTL_CLEAR_PARITY_ERROR);

	//FreeBSD: pci_write_config(sc->twe_dev, PCIR_STATUS, TWE_PCI_CLEAR_PARITY_ERROR, 2);
}


/*
 * Clear a PCI abort.
 */
static void
twe_clear_pci_abort(struct twe_softc *sc)
{
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, 0x0, TWE_CTL_CLEAR_PCI_ABORT);

	//FreeBSD: pci_write_config(sc->twe_dev, PCIR_STATUS, TWE_PCI_CLEAR_PCI_ABORT, 2);
}

/*
 * Complain if the status bits aren't what we expect.
 */
static int
twe_status_check(struct twe_softc *sc, u_int status)
{
	int rv;

	rv = 0;

	if ((status & TWE_STS_EXPECTED_BITS) != TWE_STS_EXPECTED_BITS) {
		aprint_error_dev(sc->sc_dev, "missing status bits: 0x%08x\n",
		    status & ~TWE_STS_EXPECTED_BITS);
		rv = -1;
	}

	if ((status & TWE_STS_UNEXPECTED_BITS) != 0) {
		aprint_error_dev(sc->sc_dev, "unexpected status bits: 0x%08x\n",
		    status & TWE_STS_UNEXPECTED_BITS);
		rv = -1;
		if (status & TWE_STS_PCI_PARITY_ERROR) {
			aprint_error_dev(sc->sc_dev, "PCI parity error: Reseat card, move card "
			       "or buggy device present.\n");
			twe_clear_pci_parity_error(sc);
		}
		if (status & TWE_STS_PCI_ABORT) {
			aprint_error_dev(sc->sc_dev, "PCI abort, clearing.\n");
			twe_clear_pci_abort(sc);
		}
	}

	return (rv);
}

/*
 * Allocate and initialise a CCB.
 */
static inline void
twe_ccb_init(struct twe_softc *sc, struct twe_ccb *ccb, int flags)
{
	struct twe_cmd *tc;

	ccb->ccb_tx.tx_handler = NULL;
	ccb->ccb_flags = flags;
	tc = ccb->ccb_cmd;
	tc->tc_status = 0;
	tc->tc_flags = 0;
	tc->tc_cmdid = ccb->ccb_cmdid;
}

struct twe_ccb *
twe_ccb_alloc(struct twe_softc *sc, int flags)
{
	struct twe_ccb *ccb;
	int s;

	s = splbio();
	if (__predict_false((flags & TWE_CCB_AEN) != 0)) {
		/* Use the reserved CCB. */
		ccb = sc->sc_ccbs;
	} else {
		/* Allocate a CCB and command block. */
		if (__predict_false((ccb =
				SLIST_FIRST(&sc->sc_ccb_freelist)) == NULL)) {
			splx(s);
			return (NULL);
		}
		SLIST_REMOVE_HEAD(&sc->sc_ccb_freelist, ccb_chain.slist);
	}
#ifdef DIAGNOSTIC
	if ((long)(ccb - sc->sc_ccbs) == 0 && (flags & TWE_CCB_AEN) == 0)
		panic("twe_ccb_alloc: got reserved CCB for non-AEN");
	if ((ccb->ccb_flags & TWE_CCB_ALLOCED) != 0)
		panic("twe_ccb_alloc: CCB %ld already allocated",
		    (long)(ccb - sc->sc_ccbs));
	flags |= TWE_CCB_ALLOCED;
#endif
	splx(s);

	twe_ccb_init(sc, ccb, flags);
	return (ccb);
}

struct twe_ccb *
twe_ccb_alloc_wait(struct twe_softc *sc, int flags)
{
	struct twe_ccb *ccb;
	int s;

	KASSERT((flags & TWE_CCB_AEN) == 0);

	s = splbio();
	while (__predict_false((ccb =
				SLIST_FIRST(&sc->sc_ccb_freelist)) == NULL)) {
		sc->sc_flags |= TWEF_WAIT_CCB;
		(void) tsleep(&sc->sc_ccb_freelist, PRIBIO, "tweccb", 0);
	}
	SLIST_REMOVE_HEAD(&sc->sc_ccb_freelist, ccb_chain.slist);
#ifdef DIAGNOSTIC
	if ((ccb->ccb_flags & TWE_CCB_ALLOCED) != 0)
		panic("twe_ccb_alloc_wait: CCB %ld already allocated",
		    (long)(ccb - sc->sc_ccbs));
	flags |= TWE_CCB_ALLOCED;
#endif
	splx(s);

	twe_ccb_init(sc, ccb, flags);
	return (ccb);
}

/*
 * Free a CCB.
 */
void
twe_ccb_free(struct twe_softc *sc, struct twe_ccb *ccb)
{
	int s;

	s = splbio();
	if ((ccb->ccb_flags & TWE_CCB_AEN) == 0) {
		SLIST_INSERT_HEAD(&sc->sc_ccb_freelist, ccb, ccb_chain.slist);
		if (__predict_false((sc->sc_flags & TWEF_WAIT_CCB) != 0)) {
			sc->sc_flags &= ~TWEF_WAIT_CCB;
			wakeup(&sc->sc_ccb_freelist);
		}
	}
	ccb->ccb_flags = 0;
	splx(s);
}

/*
 * Map the specified CCB's command block and data buffer (if any) into
 * controller visible space.  Perform DMA synchronisation.
 */
int
twe_ccb_map(struct twe_softc *sc, struct twe_ccb *ccb)
{
	struct twe_cmd *tc;
	int flags, nsegs, i, s, rv;
	void *data;

	/*
	 * The data as a whole must be 512-byte aligned.
	 */
	if (((u_long)ccb->ccb_data & (TWE_ALIGNMENT - 1)) != 0) {
		s = splvm();
		/* XXX */
		rv = uvm_km_kmem_alloc(kmem_va_arena,
		    ccb->ccb_datasize, (VM_NOSLEEP | VM_INSTANTFIT),
		    (vmem_addr_t *)&ccb->ccb_abuf);
		splx(s);
		data = (void *)ccb->ccb_abuf;
		if ((ccb->ccb_flags & TWE_CCB_DATA_OUT) != 0)
			memcpy(data, ccb->ccb_data, ccb->ccb_datasize);
	} else {
		ccb->ccb_abuf = (vaddr_t)0;
		data = ccb->ccb_data;
	}

	/*
	 * Map the data buffer into bus space and build the S/G list.
	 */
	rv = bus_dmamap_load(sc->sc_dmat, ccb->ccb_dmamap_xfer, data,
	    ccb->ccb_datasize, NULL, BUS_DMA_NOWAIT | BUS_DMA_STREAMING |
	    ((ccb->ccb_flags & TWE_CCB_DATA_IN) ?
	    BUS_DMA_READ : BUS_DMA_WRITE));
	if (rv != 0) {
		if (ccb->ccb_abuf != (vaddr_t)0) {
			s = splvm();
			/* XXX */
			uvm_km_kmem_free(kmem_va_arena, ccb->ccb_abuf,
			    ccb->ccb_datasize);
			splx(s);
		}
		return (rv);
	}

	nsegs = ccb->ccb_dmamap_xfer->dm_nsegs;
	tc = ccb->ccb_cmd;
	tc->tc_size += 2 * nsegs;

	/* The location of the S/G list is dependent upon command type. */
	switch (tc->tc_opcode >> 5) {
	case 2:
		for (i = 0; i < nsegs; i++) {
			tc->tc_args.param.sgl[i].tsg_address =
			    htole32(ccb->ccb_dmamap_xfer->dm_segs[i].ds_addr);
			tc->tc_args.param.sgl[i].tsg_length =
			    htole32(ccb->ccb_dmamap_xfer->dm_segs[i].ds_len);
		}
		/* XXX Needed? */
		for (; i < TWE_SG_SIZE; i++) {
			tc->tc_args.param.sgl[i].tsg_address = 0;
			tc->tc_args.param.sgl[i].tsg_length = 0;
		}
		break;
	case 3:
		for (i = 0; i < nsegs; i++) {
			tc->tc_args.io.sgl[i].tsg_address =
			    htole32(ccb->ccb_dmamap_xfer->dm_segs[i].ds_addr);
			tc->tc_args.io.sgl[i].tsg_length =
			    htole32(ccb->ccb_dmamap_xfer->dm_segs[i].ds_len);
		}
		/* XXX Needed? */
		for (; i < TWE_SG_SIZE; i++) {
			tc->tc_args.io.sgl[i].tsg_address = 0;
			tc->tc_args.io.sgl[i].tsg_length = 0;
		}
		break;
	default:
		/*
		 * In all likelihood, this is a command passed from
		 * management tools in userspace where no S/G list is
		 * necessary because no data is being passed.
		 */
		break;
	}

	if ((ccb->ccb_flags & TWE_CCB_DATA_IN) != 0)
		flags = BUS_DMASYNC_PREREAD;
	else
		flags = 0;
	if ((ccb->ccb_flags & TWE_CCB_DATA_OUT) != 0)
		flags |= BUS_DMASYNC_PREWRITE;

	bus_dmamap_sync(sc->sc_dmat, ccb->ccb_dmamap_xfer, 0,
	    ccb->ccb_datasize, flags);
	return (0);
}

/*
 * Unmap the specified CCB's command block and data buffer (if any) and
 * perform DMA synchronisation.
 */
void
twe_ccb_unmap(struct twe_softc *sc, struct twe_ccb *ccb)
{
	int flags, s;

	if ((ccb->ccb_flags & TWE_CCB_DATA_IN) != 0)
		flags = BUS_DMASYNC_POSTREAD;
	else
		flags = 0;
	if ((ccb->ccb_flags & TWE_CCB_DATA_OUT) != 0)
		flags |= BUS_DMASYNC_POSTWRITE;

	bus_dmamap_sync(sc->sc_dmat, ccb->ccb_dmamap_xfer, 0,
	    ccb->ccb_datasize, flags);
	bus_dmamap_unload(sc->sc_dmat, ccb->ccb_dmamap_xfer);

	if (ccb->ccb_abuf != (vaddr_t)0) {
		if ((ccb->ccb_flags & TWE_CCB_DATA_IN) != 0)
			memcpy(ccb->ccb_data, (void *)ccb->ccb_abuf,
			    ccb->ccb_datasize);
		s = splvm();
		/* XXX */
		uvm_km_kmem_free(kmem_va_arena, ccb->ccb_abuf,
		    ccb->ccb_datasize);
		splx(s);
	}
}

/*
 * Submit a command to the controller and poll on completion.  Return
 * non-zero on timeout (but don't check status, as some command types don't
 * return status).  Must be called with interrupts blocked.
 */
int
twe_ccb_poll(struct twe_softc *sc, struct twe_ccb *ccb, int timo)
{
	int rv;

	if ((rv = twe_ccb_submit(sc, ccb)) != 0)
		return (rv);

	for (timo *= 1000; timo != 0; timo--) {
		twe_poll(sc);
		if ((ccb->ccb_flags & TWE_CCB_COMPLETE) != 0)
			break;
		DELAY(100);
	}

	return (timo == 0);
}

/*
 * If a CCB is specified, enqueue it.  Pull CCBs off the software queue in
 * the order that they were enqueued and try to submit their command blocks
 * to the controller for execution.
 */
void
twe_ccb_enqueue(struct twe_softc *sc, struct twe_ccb *ccb)
{
	int s;

	s = splbio();

	if (ccb != NULL)
		SIMPLEQ_INSERT_TAIL(&sc->sc_ccb_queue, ccb, ccb_chain.simpleq);

	while ((ccb = SIMPLEQ_FIRST(&sc->sc_ccb_queue)) != NULL) {
		if (twe_ccb_submit(sc, ccb))
			break;
		SIMPLEQ_REMOVE_HEAD(&sc->sc_ccb_queue, ccb_chain.simpleq);
	}

	splx(s);
}

/*
 * Submit the command block associated with the specified CCB to the
 * controller for execution.  Must be called with interrupts blocked.
 */
int
twe_ccb_submit(struct twe_softc *sc, struct twe_ccb *ccb)
{
	bus_addr_t pa;
	int rv;
	u_int status;

	/* Check to see if we can post a command. */
	status = twe_inl(sc, TWE_REG_STS);
	twe_status_check(sc, status);

	if ((status & TWE_STS_CMD_QUEUE_FULL) == 0) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
		    (char *)ccb->ccb_cmd - (char *)sc->sc_cmds,
		    sizeof(struct twe_cmd),
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
#ifdef DIAGNOSTIC
		if ((ccb->ccb_flags & TWE_CCB_ALLOCED) == 0)
			panic("%s: CCB %ld not ALLOCED\n",
			    device_xname(sc->sc_dev), (long)(ccb - sc->sc_ccbs));
#endif
		ccb->ccb_flags |= TWE_CCB_ACTIVE;
		pa = sc->sc_cmds_paddr +
		    ccb->ccb_cmdid * sizeof(struct twe_cmd);
		twe_outl(sc, TWE_REG_CMD_QUEUE, (u_int32_t)pa);
		rv = 0;
	} else
		rv = EBUSY;

	return (rv);
}


/*
 * Accept an open operation on the control device.
 */
static int
tweopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct twe_softc *twe;

	if ((twe = device_lookup_private(&twe_cd, minor(dev))) == NULL)
		return (ENXIO);
	if ((twe->sc_flags & TWEF_OPEN) != 0)
		return (EBUSY);

	twe->sc_flags |= TWEF_OPEN;
	return (0);
}

/*
 * Accept the last close on the control device.
 */
static int
tweclose(dev_t dev, int flag, int mode,
    struct lwp *l)
{
	struct twe_softc *twe;

	twe = device_lookup_private(&twe_cd, minor(dev));
	twe->sc_flags &= ~TWEF_OPEN;
	return (0);
}

void
twe_ccb_wait_handler(struct twe_ccb *ccb, int error)
{

	/* Just wake up the sleeper. */
	wakeup(ccb);
}

/*
 * Handle control operations.
 */
static int
tweioctl(dev_t dev, u_long cmd, void *data, int flag,
    struct lwp *l)
{
	struct twe_softc *twe;
	struct twe_ccb *ccb;
	struct twe_param *param;
	struct twe_usercommand *tu;
	struct twe_paramcommand *tp;
	struct twe_drivecommand *td;
	void *pdata = NULL;
	int s, error = 0;
	u_int8_t cmdid;

	twe = device_lookup_private(&twe_cd, minor(dev));
	tu = (struct twe_usercommand *)data;
	tp = (struct twe_paramcommand *)data;
	td = (struct twe_drivecommand *)data;

	/* This is intended to be compatible with the FreeBSD interface. */
	switch (cmd) {
	case TWEIO_COMMAND:
		error = kauth_authorize_device_passthru(l->l_cred, dev,
		    KAUTH_REQ_DEVICE_RAWIO_PASSTHRU_ALL, data);
		if (error)
			return (error);

		/* XXX mutex */
		if (tu->tu_size > 0) {
			/*
			 * XXX Handle > TWE_SECTOR_SIZE?  Let's see if
			 * it's really necessary, first.
			 */
			if (tu->tu_size > TWE_SECTOR_SIZE) {
#ifdef TWE_DEBUG
				printf("%s: TWEIO_COMMAND: tu_size = %zu\n",
				    device_xname(twe->sc_dev), tu->tu_size);
#endif
				return EINVAL;
			}
			pdata = malloc(TWE_SECTOR_SIZE, M_DEVBUF, M_WAITOK);
			error = copyin(tu->tu_data, pdata, tu->tu_size);
			if (error != 0)
				goto done;
			ccb = twe_ccb_alloc_wait(twe,
			    TWE_CCB_DATA_IN | TWE_CCB_DATA_OUT);
			KASSERT(ccb != NULL);
			ccb->ccb_data = pdata;
			ccb->ccb_datasize = TWE_SECTOR_SIZE;
		} else {
			ccb = twe_ccb_alloc_wait(twe, 0);
			KASSERT(ccb != NULL);
		}

		ccb->ccb_tx.tx_handler = twe_ccb_wait_handler;
		ccb->ccb_tx.tx_context = NULL;
		ccb->ccb_tx.tx_dv = twe->sc_dev;

		cmdid = ccb->ccb_cmdid;
		memcpy(ccb->ccb_cmd, &tu->tu_cmd, sizeof(struct twe_cmd));
		ccb->ccb_cmd->tc_cmdid = cmdid;

		/* Map the transfer. */
		if ((error = twe_ccb_map(twe, ccb)) != 0) {
			twe_ccb_free(twe, ccb);
			goto done;
		}

		/* Submit the command and wait up to 1 minute. */
		error = 0;
		twe_ccb_enqueue(twe, ccb);
		s = splbio();
		while ((ccb->ccb_flags & TWE_CCB_COMPLETE) == 0)
			if ((error = tsleep(ccb, PRIBIO, "tweioctl",
					    60 * hz)) != 0)
				break;
		splx(s);

		/* Copy the command back to the ioctl argument. */
		memcpy(&tu->tu_cmd, ccb->ccb_cmd, sizeof(struct twe_cmd));
#ifdef TWE_DEBUG
		printf("%s: TWEIO_COMMAND: tc_opcode = 0x%02x, "
		    "tc_status = 0x%02x\n", device_xname(twe->sc_dev),
		    tu->tu_cmd.tc_opcode, tu->tu_cmd.tc_status);
#endif

		s = splbio();
		twe_ccb_free(twe, ccb);
		splx(s);

		if (tu->tu_size > 0)
			error = copyout(pdata, tu->tu_data, tu->tu_size);
		goto done;

	case TWEIO_STATS:
		return (ENOENT);

	case TWEIO_AEN_POLL:
		s = splbio();
		*(u_int *)data = twe_aen_dequeue(twe);
		splx(s);
		return (0);

	case TWEIO_AEN_WAIT:
		s = splbio();
		while ((*(u_int *)data =
		    twe_aen_dequeue(twe)) == TWE_AEN_QUEUE_EMPTY) {
			twe->sc_flags |= TWEF_AENQ_WAIT;
			error = tsleep(&twe->sc_aen_queue, PRIBIO | PCATCH,
			    "tweaen", 0);
			if (error == EINTR) {
				splx(s);
				return (error);
			}
		}
		splx(s);
		return (0);

	case TWEIO_GET_PARAM:
		error = twe_param_get(twe, tp->tp_table_id, tp->tp_param_id,
		    tp->tp_size, 0, &param);
		if (error != 0)
			return (error);
		if (param->tp_param_size > tp->tp_size) {
			error = EFAULT;
			goto done;
		}
		error = copyout(param->tp_data, tp->tp_data,
		    param->tp_param_size);
		free(param, M_DEVBUF);
		goto done;

	case TWEIO_SET_PARAM:
		pdata = malloc(tp->tp_size, M_DEVBUF, M_WAITOK);
		if ((error = copyin(tp->tp_data, pdata, tp->tp_size)) != 0)
			goto done;
		error = twe_param_set(twe, tp->tp_table_id, tp->tp_param_id,
		    tp->tp_size, pdata);
		goto done;

	case TWEIO_RESET:
		s = splbio();
		twe_reset(twe);
		splx(s);
		return (0);

	case TWEIO_ADD_UNIT:
		/* XXX mutex */
		return (twe_add_unit(twe, td->td_unit));

	case TWEIO_DEL_UNIT:
		/* XXX mutex */
		return (twe_del_unit(twe, td->td_unit));

	default:
		return EINVAL;
	}
done:
	if (pdata)
		free(pdata, M_DEVBUF);
	return error;
}

const struct cdevsw twe_cdevsw = {
	.d_open = tweopen,
	.d_close = tweclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = tweioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

/*
 * Print some information about the controller
 */
static void
twe_describe_controller(struct twe_softc *sc)
{
	struct twe_param *p[6];
	int i, rv = 0;
	uint32_t dsize;
	uint8_t ports;

	ports = 0;

	/* get the port count */
	rv |= twe_param_get_1(sc, TWE_PARAM_CONTROLLER,
		TWE_PARAM_CONTROLLER_PortCount, &ports);

	/* get version strings */
	rv |= twe_param_get(sc, TWE_PARAM_VERSION, TWE_PARAM_VERSION_Mon,
		16, NULL, &p[0]);
	rv |= twe_param_get(sc, TWE_PARAM_VERSION, TWE_PARAM_VERSION_FW,
		16, NULL, &p[1]);
	rv |= twe_param_get(sc, TWE_PARAM_VERSION, TWE_PARAM_VERSION_BIOS,
		16, NULL, &p[2]);
	rv |= twe_param_get(sc, TWE_PARAM_VERSION, TWE_PARAM_VERSION_PCB,
		8, NULL, &p[3]);
	rv |= twe_param_get(sc, TWE_PARAM_VERSION, TWE_PARAM_VERSION_ATA,
		8, NULL, &p[4]);
	rv |= twe_param_get(sc, TWE_PARAM_VERSION, TWE_PARAM_VERSION_PCI,
		8, NULL, &p[5]);

	if (rv) {
		/* some error occurred */
		aprint_error_dev(sc->sc_dev, "failed to fetch version information\n");
		return;
	}

	aprint_normal_dev(sc->sc_dev, "%d ports, Firmware %.16s, BIOS %.16s\n",
		ports, p[1]->tp_data, p[2]->tp_data);

	aprint_verbose_dev(sc->sc_dev, "Monitor %.16s, PCB %.8s, Achip %.8s, Pchip %.8s\n",
		p[0]->tp_data, p[3]->tp_data,
		p[4]->tp_data, p[5]->tp_data);

	free(p[0], M_DEVBUF);
	free(p[1], M_DEVBUF);
	free(p[2], M_DEVBUF);
	free(p[3], M_DEVBUF);
	free(p[4], M_DEVBUF);
	free(p[5], M_DEVBUF);

	rv = twe_param_get(sc, TWE_PARAM_DRIVESUMMARY,
	    TWE_PARAM_DRIVESUMMARY_Status, 16, NULL, &p[0]);
	if (rv) {
		aprint_error_dev(sc->sc_dev, "failed to get drive status summary\n");
		return;
	}
	for (i = 0; i < ports; i++) {
		if (p[0]->tp_data[i] != TWE_PARAM_DRIVESTATUS_Present)
			continue;
		rv = twe_param_get_4(sc, TWE_PARAM_DRIVEINFO + i,
		    TWE_PARAM_DRIVEINFO_Size, &dsize);
		if (rv) {
			aprint_error_dev(sc->sc_dev, 
			    "unable to get drive size for port %d\n", i);
			continue;
		}
		rv = twe_param_get(sc, TWE_PARAM_DRIVEINFO + i,
		    TWE_PARAM_DRIVEINFO_Model, 40, NULL, &p[1]);
		if (rv) {
			aprint_error_dev(sc->sc_dev,
			    "unable to get drive model for port %d\n", i);
			continue;
		}
		aprint_verbose_dev(sc->sc_dev, "port %d: %.40s %d MB\n",
		    i, p[1]->tp_data, dsize / 2048);
		free(p[1], M_DEVBUF);
	}
	free(p[0], M_DEVBUF);
}

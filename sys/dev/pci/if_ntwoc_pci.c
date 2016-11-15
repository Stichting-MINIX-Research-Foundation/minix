/*	$NetBSD: if_ntwoc_pci.c,v 1.29 2014/03/29 19:28:25 christos Exp $	*/

/*
 * Copyright (c) 1998 Vixie Enterprises
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Vixie Enterprises nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY VIXIE ENTERPRISES AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL VIXIE ENTERPRISES OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for Vixie Enterprises by Michael Graff
 * <explorer@flame.org>.  To learn more about Vixie Enterprises, see
 * ``http://www.vix.com''.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_ntwoc_pci.c,v 1.29 2014/03/29 19:28:25 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/if.h>

#include <sys/cpu.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/hd64570reg.h>
#include <dev/ic/hd64570var.h>

#include <dev/pci/if_ntwoc_pcireg.h>

#if 0
#define NTWO_DEBUG
#endif

#ifdef NTWO_DEBUG
#define NTWO_DPRINTF(x) printf x
#else
#define NTWO_DPRINTF(x)
#endif

/*
 * buffers per tx and rx channels, per port, and the size of each.
 * Don't use these constants directly, as they are really only hints.
 * Use the calculated values stored in struct sca_softc instead.
 *
 * Each must be at least 2, receive would be better at around 20 or so.
 *
 * XXX Due to a damned near impossible to track down bug, transmit buffers
 * MUST be 2, no more, no less.
 */
#ifndef NTWOC_NtxBUFS
#define NTWOC_NtxBUFS     40
#endif
#ifndef NTWOC_NrxBUFS
#define NTWOC_NrxBUFS     20
#endif

#if __NetBSD_Version__ >= 104160000
static	void ntwoc_pci_config_interrupts(device_t);
#else
#define	SCA_BASECLOCK	16000000
#endif

/*
 * Card specific config register location
 */
#define PCI_CBMA_ASIC PCI_BAR(0)	/* Configuration Base Memory Address */
#define PCI_CBMA_SCA PCI_BAR(2)

struct ntwoc_pci_softc {
	/* Generic device stuff */
	device_t sc_dev;		/* Common to all devices */

	/* PCI chipset glue */
	pci_intr_handle_t *sc_ih;	/* Interrupt handler */
	pci_chipset_tag_t sc_sr;	/* PCI chipset handle */

	bus_space_tag_t sc_asic_iot;	/* space cookie (for ASIC) */
	bus_space_handle_t sc_asic_ioh;	/* bus space handle (for ASIC) */

	struct sca_softc sc_sca;	/* the SCA itself */
};

static  int ntwoc_pci_match(device_t, cfdata_t, void *);
static  void ntwoc_pci_attach(device_t, device_t, void *);

static	int ntwoc_pci_alloc_dma(struct sca_softc *);
static	void ntwoc_pci_clock_callback(void *, int, int);
static	void ntwoc_pci_dtr_callback(void *, int, int);
static	void ntwoc_pci_get_clock(struct sca_port *, u_int8_t, u_int8_t,
    u_int8_t, u_int8_t);
static	int ntwoc_pci_intr(void *);
static	void ntwoc_pci_setup_dma(struct sca_softc *);
static	void ntwoc_pci_shutdown(void *sc);

CFATTACH_DECL_NEW(ntwoc_pci, sizeof(struct ntwoc_pci_softc),
    ntwoc_pci_match, ntwoc_pci_attach, NULL, NULL);

/*
 * Names for daughter card types.  These match the NTWOC_DB_* defines.
 */
const char *ntwoc_pci_db_names[] = {
	"V.35", "Unknown 0x01", "Test", "Unknown 0x03",
	"RS232", "Unknown 0x05", "RS422", "None"
};

/*
 * At least one implementation uses a somewhat strange register address
 * mapping.  If a card doesn't, define this to be a pass-through
 * macro.  (The ntwo driver needs this...)
 */
#define SCA_REG(y)  (((y) & 0x0002) ? (((y) & 0x00fd) + 0x100) : (y))

/*
 * functions that read and write to the sca registers
 */
static void
ntwoc_pci_sca_write_1(struct sca_softc *sc, u_int reg, u_int8_t val)
{
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, SCA_REG(reg), val);
}

static void
ntwoc_pci_sca_write_2(struct sca_softc *sc, u_int reg, u_int16_t val)
{
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, SCA_REG(reg), val);
}

static u_int8_t
ntwoc_pci_sca_read_1(struct sca_softc *sc, u_int reg)
{
	return
	    bus_space_read_1(sc->sc_iot, sc->sc_ioh, SCA_REG(reg));
}

static u_int16_t
ntwoc_pci_sca_read_2(struct sca_softc *sc, u_int reg)
{
	return
	    bus_space_read_2(sc->sc_iot, sc->sc_ioh, SCA_REG(reg));
}



static int
ntwoc_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if ((PCI_VENDOR(pa->pa_id) == PCI_VENDOR_RISCOM)
	    && (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_RISCOM_N2))
		return 1;

	return 0;
}

static void
ntwoc_pci_attach(device_t parent, device_t self, void *aux)
{
	struct ntwoc_pci_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	struct sca_softc *sca = &sc->sc_sca;
	pci_intr_handle_t ih;
	const char *intrstr;
	pcireg_t csr;
	u_int8_t tmc, rdiv, tdiv;
	u_int16_t frontend_cr;
	u_int16_t db0, db1;
	u_int32_t flags;
	u_int numports;
	char intrbuf[PCI_INTRSTR_LEN];

	printf(": N2 Serial Interface\n");
	flags = device_cfdata(self)->cf_flags;

	/*
	 * Map in the ASIC configuration space
	 */
	if (pci_mapreg_map(pa, PCI_CBMA_ASIC, PCI_MAPREG_TYPE_MEM, 0,
			   &sc->sc_asic_iot, &sc->sc_asic_ioh, NULL, NULL)) {
		aprint_error_dev(self, "Can't map register space (ASIC)\n");
		return;
	}
	/*
	 * Map in the serial controller configuration space
	 */
	if (pci_mapreg_map(pa, PCI_CBMA_SCA, PCI_MAPREG_TYPE_MEM, 0,
			   &sca->sc_iot, &sca->sc_ioh, NULL, NULL)) {
		aprint_error_dev(self, "Can't map register space (SCA)\n");
		return;
	}

	/*
	 * Enable the card
	 */
	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, csr);

	/*
	 * Map and establish the interrupt
	 */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_NET, ntwoc_pci_intr,
	    sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	/*
	 * Perform total black magic.  This is not only extremely
	 * disgusting, but it should be explained a lot more in the
	 * card's documentation.
	 *
	 * From what I gather, this does nothing more than configure the
	 * PCI to ISA translator ASIC the N2pci card uses.
	 *
	 * From the FreeBSD driver:
	 * offset
	 *  0x00 - Map Range    - Mem-mapped to locate anywhere
	 *  0x04 - Re-Map       - PCI address decode enable
	 *  0x18 - Bus Region   - 32-bit bus, ready enable
	 *  0x1c - Master Range - include all 16 MB
	 *  0x20 - Master RAM   - Map SCA Base at 0
	 *  0x28 - Master Remap - direct master memory enable
	 *  0x68 - Interrupt    - Enable interrupt (0 to disable)
	 */
	bus_space_write_4(sc->sc_asic_iot, sc->sc_asic_ioh,
			  0x00, 0xfffff000);
	bus_space_write_4(sc->sc_asic_iot, sc->sc_asic_ioh,
			  0x04, 1);
	bus_space_write_4(sc->sc_asic_iot, sc->sc_asic_ioh,
			  0x18, 0x40030043);
	bus_space_write_4(sc->sc_asic_iot, sc->sc_asic_ioh,
			  0x1c, 0xff000000);
	bus_space_write_4(sc->sc_asic_iot, sc->sc_asic_ioh,
			  0x20, 0);
	bus_space_write_4(sc->sc_asic_iot, sc->sc_asic_ioh,
			  0x28, 0xe9);
	bus_space_write_4(sc->sc_asic_iot, sc->sc_asic_ioh,
			  0x68, 0x10900);

	/*
	 * pass the DMA tag to the SCA
	 */
	sca->sc_usedma = 1;
	sca->scu_dmat = pa->pa_dmat;

	/*
	 * Read the configuration information off the daughter card.
	 */
	frontend_cr = bus_space_read_2(sca->sc_iot, sca->sc_ioh, NTWOC_FECR);
	NTWO_DPRINTF(("%s: frontend_cr = 0x%04x\n",
		      device_xname(self), frontend_cr));

	db0 = (frontend_cr & NTWOC_FECR_ID0) >> NTWOC_FECR_ID0_SHIFT;
	db1 = (frontend_cr & NTWOC_FECR_ID1) >> NTWOC_FECR_ID1_SHIFT;

	/*
	 * Port 1 HAS to be present.  If it isn't, don't attach anything.
	 */
	if (db0 == NTWOC_FE_ID_NONE) {
		printf("%s: no ports available\n", device_xname(self));
		return;
	}

	/*
	 * Port 1 is present.  Now, check to see if port 2 is also
	 * present.
	 */
	numports = 1;
	if (db1 != NTWOC_FE_ID_NONE)
		numports++;

	printf("%s: %d port%s\n", device_xname(self), numports,
	       (numports > 1 ? "s" : ""));
	printf("%s: port 0 interface card: %s\n", device_xname(self),
	       ntwoc_pci_db_names[db0]);
	if (numports > 1)
		printf("%s: port 1 interface card: %s\n", device_xname(self),
		       ntwoc_pci_db_names[db1]);

	/*
	 * enable the RS422 tristate transmit
	 * diable clock output (use receiver clock for both)
	 */
	frontend_cr |= (NTWOC_FECR_TE0 | NTWOC_FECR_TE1);
	frontend_cr &= ~(NTWOC_FECR_ETC0 | NTWOC_FECR_ETC1);
	bus_space_write_2(sc->sc_sca.sc_iot, sc->sc_sca.sc_ioh,
			  NTWOC_FECR, frontend_cr);

	/*
	 * initialize the SCA.  This will allocate DMAable memory based
	 * on the number of ports we passed in, the size of each
	 * buffer, and the number of buffers per port.
	 */
	sca->sc_parent = self;
	sca->sc_read_1 = ntwoc_pci_sca_read_1;
	sca->sc_read_2 = ntwoc_pci_sca_read_2;
	sca->sc_write_1 = ntwoc_pci_sca_write_1;
	sca->sc_write_2 = ntwoc_pci_sca_write_2;
	sca->sc_dtr_callback = ntwoc_pci_dtr_callback;
	sca->sc_clock_callback = ntwoc_pci_clock_callback;
	sca->sc_aux = sc;
	sca->sc_numports = numports;

	/*
	 * get clock information from user
	 */
	rdiv = (flags & NTWOC_FLAGS_RXDIV_MASK) >> NTWOC_FLAGS_RXDIV_SHIFT;
	if (rdiv > 9)
		panic("bad rx divisor in flags");

	tdiv = (flags & NTWOC_FLAGS_TXDIV_MASK) >> NTWOC_FLAGS_TXDIV_SHIFT;
	if (tdiv > 9)
		panic("bad tx divisor in flags");
	tmc = (flags & NTWOC_FLAGS_TMC_MASK) >> NTWOC_FLAGS_TMC_SHIFT;

	ntwoc_pci_get_clock(&sca->sc_ports[0], flags & NTWOC_FLAGS_CLK0_MASK,
	    tmc, rdiv, tdiv);
	if (sca->sc_numports > 1)
		ntwoc_pci_get_clock(&sca->sc_ports[1],
		    (flags & NTWOC_FLAGS_CLK1_MASK) >> NTWOC_FLAGS_CLK1_SHIFT,
		    tmc, rdiv, tdiv);

	/* allocate DMA'able memory for card to use */
	ntwoc_pci_alloc_dma(sca);
	ntwoc_pci_setup_dma(sca);

	sca_init(sca);

	/*
	 * always initialize port 0, since we have to have found it to
	 * get this far.  If we have two ports, attach the second
	 * as well.
	 */
	sca_port_attach(sca, 0);
	if (numports == 2)
		sca_port_attach(sca, 1);

	/*
	 * Add shutdown hook so that DMA is disabled prior to reboot. Not
	 * doing do could allow DMA to corrupt kernel memory during the
	 * reboot before the driver initializes.
	 */
	shutdownhook_establish(ntwoc_pci_shutdown, sc);

#if __NetBSD_Version__ >= 104160000
	/*
	 * defer getting the base clock until interrupts are enabled
	 * (and thus we have microtime())
	 */
	config_interrupts(self, ntwoc_pci_config_interrupts);
#else
	sca->sc_baseclock = SCA_BASECLOCK;
	sca_print_clock_info(&sc->sc_sca);
#endif
}

/*
 * extract the clock information for a port from the flags field
 */
static void
ntwoc_pci_get_clock(struct sca_port *scp, u_int8_t flags, u_int8_t tmc,
    u_int8_t rdiv, u_int8_t tdiv)
{
	scp->sp_eclock =
	    (flags & NTWOC_FLAGS_ECLOCK_MASK) >> NTWOC_FLAGS_ECLOCK_SHIFT;
	scp->sp_rxs = rdiv;
	scp->sp_txs = tdiv;
	scp->sp_tmc = tmc;

	/* get rx source */
	switch ((flags & NTWOC_FLAGS_RXS_MASK) >> NTWOC_FLAGS_RXS_SHIFT) {
	case NTWOC_FLAGS_RXS_LINE:
		scp->sp_rxs = 0;
		break;
	case NTWOC_FLAGS_RXS_LINE_SN:
		scp->sp_rxs |= SCA_RXS_CLK_LINE_SN;
		break;
	case NTWOC_FLAGS_RXS_INTERNAL:
		scp->sp_rxs |= SCA_RXS_CLK_INTERNAL;
		break;
	case NTWOC_FLAGS_RXS_ADPLL_OUT:
		scp->sp_rxs |= SCA_RXS_CLK_ADPLL_OUT;
		break;
	case NTWOC_FLAGS_RXS_ADPLL_IN:
		scp->sp_rxs |= SCA_RXS_CLK_ADPLL_IN;
		break;
	default:
		panic("bad rx source in flags");
	}

	/* get tx source */
	switch ((flags & NTWOC_FLAGS_TXS_MASK) >> NTWOC_FLAGS_TXS_SHIFT) {
	case NTWOC_FLAGS_TXS_LINE:
		scp->sp_txs = 0;
		break;
	case NTWOC_FLAGS_TXS_INTERNAL:
		scp->sp_txs |= SCA_TXS_CLK_INTERNAL;
		break;
	case NTWOC_FLAGS_TXS_RXCLOCK:
		scp->sp_txs |= SCA_TXS_CLK_RXCLK;
		break;
	default:
		panic("bad rx source in flags");
	}
}


static int
ntwoc_pci_intr(void *arg)
{
	struct ntwoc_pci_softc *sc = (struct ntwoc_pci_softc *)arg;

	return sca_hardintr(&sc->sc_sca);
}

/*
 * shut down interrupts and DMA, so we don't trash the kernel on warm
 * boot.  Also, lower DTR on each port and disable card interrupts.
 */
static void
ntwoc_pci_shutdown(void *aux)
{
	struct ntwoc_pci_softc *sc = aux;
	u_int16_t fecr;

	/*
	 * shut down the SCA ports
	 */
	sca_shutdown(&sc->sc_sca);

	/*
	 * disable interrupts for the whole card.  Black magic, see comment
	 * above.
	 */
	bus_space_write_4(sc->sc_asic_iot, sc->sc_asic_ioh,
			  0x68, 0x10900);

	/*
	 * lower DTR on both ports
	 */
	fecr = bus_space_read_2(sc->sc_sca.sc_iot,
				sc->sc_sca.sc_ioh, NTWOC_FECR);
	fecr |= (NTWOC_FECR_DTR0 | NTWOC_FECR_DTR1);
	bus_space_write_2(sc->sc_sca.sc_iot, sc->sc_sca.sc_ioh,
			  NTWOC_FECR, fecr);
}

static void
ntwoc_pci_dtr_callback(void *aux, int port, int state)
{
	struct ntwoc_pci_softc *sc = aux;
	u_int16_t fecr;

	fecr = bus_space_read_2(sc->sc_sca.sc_iot,
				sc->sc_sca.sc_ioh, NTWOC_FECR);

	NTWO_DPRINTF(("dtr: port == %d, state == %d, old fecr:  0x%04x\n",
		       port, state, fecr));

	if (port == 0) {
		if (state == 0)
			fecr |= NTWOC_FECR_DTR0;
		else
			fecr &= ~NTWOC_FECR_DTR0;
	} else {
		if (state == 0)
			fecr |= NTWOC_FECR_DTR1;
		else
			fecr &= ~NTWOC_FECR_DTR1;
	}

	NTWO_DPRINTF(("new fecr:  0x%04x\n", fecr));

	bus_space_write_2(sc->sc_sca.sc_iot, sc->sc_sca.sc_ioh,
			  NTWOC_FECR, fecr);
}

static void
ntwoc_pci_clock_callback(void *aux, int port, int enable)
{
	struct ntwoc_pci_softc *sc = aux;
	u_int16_t fecr;

	fecr = bus_space_read_2(sc->sc_sca.sc_iot,
				sc->sc_sca.sc_ioh, NTWOC_FECR);

	NTWO_DPRINTF(("clk: port == %d, enable == %d, old fecr:  0x%04x\n",
		       port, enable, fecr));

	if (port == 0) {
		if (enable)
			fecr |= NTWOC_FECR_ETC0;
		else
			fecr &= ~NTWOC_FECR_ETC0;
	} else {
		if (enable)
			fecr |= NTWOC_FECR_ETC1;
		else
			fecr &= ~NTWOC_FECR_ETC1;
	}

	NTWO_DPRINTF(("new fecr:  0x%04x\n", fecr));

	bus_space_write_2(sc->sc_sca.sc_iot, sc->sc_sca.sc_ioh,
			  NTWOC_FECR, fecr);
}

static int
ntwoc_pci_alloc_dma(struct sca_softc *sc)
{
	u_int	allocsize;
	int	err;
	int	rsegs;
	u_int	bpp;

	/* first initialize the number of descriptors */
	sc->sc_ports[0].sp_nrxdesc = NTWOC_NrxBUFS;
	sc->sc_ports[0].sp_ntxdesc = NTWOC_NtxBUFS;
	if (sc->sc_numports == 2) {
		sc->sc_ports[1].sp_nrxdesc = NTWOC_NrxBUFS;
		sc->sc_ports[1].sp_ntxdesc = NTWOC_NtxBUFS;
	}

	NTWO_DPRINTF(("sizeof sca_desc_t: %d bytes\n", sizeof (sca_desc_t)));

	bpp = sc->sc_numports * (NTWOC_NtxBUFS + NTWOC_NrxBUFS);

	allocsize = bpp * (SCA_BSIZE + sizeof (sca_desc_t));

	/*
	 * sanity checks:
	 *
	 * Check the total size of the data buffers, and so on.  The total
	 * DMAable space needs to fit within a single 16M region, and the
	 * descriptors need to fit within a 64K region.
	 */
	if (allocsize > 16 * 1024 * 1024)
		return 1;
	if (bpp * sizeof (sca_desc_t) > 64 * 1024)
		return 1;

	sc->scu_allocsize = allocsize;

	/*
	 * Allocate one huge chunk of memory.
	 */
	if (bus_dmamem_alloc(sc->scu_dmat,
			     allocsize,
			     SCA_DMA_ALIGNMENT,
			     SCA_DMA_BOUNDARY,
			     &sc->scu_seg, 1, &rsegs, BUS_DMA_NOWAIT) != 0) {
		printf("Could not allocate DMA memory\n");
		return 1;
	}
	NTWO_DPRINTF(("DMA memory allocated:  %d bytes\n", allocsize));

	if (bus_dmamem_map(sc->scu_dmat, &sc->scu_seg, 1, allocsize,
			   &sc->scu_dma_addr, BUS_DMA_NOWAIT) != 0) {
		printf("Could not map DMA memory into kernel space\n");
		return 1;
	}
	NTWO_DPRINTF(("DMA memory mapped\n"));

	if (bus_dmamap_create(sc->scu_dmat, allocsize, 2,
			      allocsize, SCA_DMA_BOUNDARY,
			      BUS_DMA_NOWAIT, &sc->scu_dmam) != 0) {
		printf("Could not create DMA map\n");
		return 1;
	}
	NTWO_DPRINTF(("DMA map created\n"));

	err = bus_dmamap_load(sc->scu_dmat, sc->scu_dmam, sc->scu_dma_addr,
			      allocsize, NULL, BUS_DMA_NOWAIT);
	if (err != 0) {
		printf("Could not load DMA segment:  %d\n", err);
		return 1;
	}
	NTWO_DPRINTF(("DMA map loaded\n"));

	return 0;
}

/*
 * Take the memory allocated with sca_alloc_dma() and divide it among the
 * two ports.
 */
static void
ntwoc_pci_setup_dma(struct sca_softc *sc)
{
	sca_port_t *scp0, *scp1;
	u_int8_t  *vaddr0;
	u_int32_t paddr0;
	u_long addroff;

	/*
	 * remember the physical address to 24 bits only, since the upper
	 * 8 bits is programed into the device at a different layer.
	 */
	paddr0 = (sc->scu_dmam->dm_segs[0].ds_addr & 0x00ffffff);
	vaddr0 = sc->scu_dma_addr;

	/*
	 * if we have only one port it gets the full range.  If we have
	 * two we need to do a little magic to divide things up.
	 *
	 * The descriptors will all end up in the front of the area, while
	 * the remainder of the buffer is used for transmit and receive
	 * data.
	 *
	 * -------------------- start of memory
	 *    tx desc port 0
	 *    rx desc port 0
	 *    tx desc port 1
	 *    rx desc port 1
	 *    tx buffer port 0
	 *    rx buffer port 0
	 *    tx buffer port 1
	 *    rx buffer port 1
	 * -------------------- end of memory
	 */
	scp0 = &sc->sc_ports[0];
	scp1 = &sc->sc_ports[1];

	scp0->sp_txdesc_p = paddr0;
	scp0->sp_txdesc = (sca_desc_t *)vaddr0;
	addroff = sizeof(sca_desc_t) * scp0->sp_ntxdesc;

	/*
	 * point to the range following the tx descriptors, and
	 * set the rx descriptors there.
	 */
	scp0->sp_rxdesc_p = paddr0 + addroff;
	scp0->sp_rxdesc = (sca_desc_t *)(vaddr0 + addroff);
	addroff += sizeof(sca_desc_t) * scp0->sp_nrxdesc;

	if (sc->sc_numports == 2) {
		scp1->sp_txdesc_p = paddr0 + addroff;
		scp1->sp_txdesc = (sca_desc_t *)(vaddr0 + addroff);
		addroff += sizeof(sca_desc_t) * scp1->sp_ntxdesc;

		scp1->sp_rxdesc_p = paddr0 + addroff;
		scp1->sp_rxdesc = (sca_desc_t *)(vaddr0 + addroff);
		addroff += sizeof(sca_desc_t) * scp1->sp_nrxdesc;
	}

	/*
	 * point to the memory following the descriptors, and set the
	 * transmit buffer there.
	 */
	scp0->sp_txbuf_p = paddr0 + addroff;
	scp0->sp_txbuf = vaddr0 + addroff;
	addroff += SCA_BSIZE * scp0->sp_ntxdesc;

	/*
	 * lastly, skip over the transmit buffer and set up pointers into
	 * the receive buffer.
	 */
	scp0->sp_rxbuf_p = paddr0 + addroff;
	scp0->sp_rxbuf = vaddr0 + addroff;
	addroff += SCA_BSIZE * scp0->sp_nrxdesc;

	if (sc->sc_numports == 2) {
		scp1->sp_txbuf_p = paddr0 + addroff;
		scp1->sp_txbuf = vaddr0 + addroff;
		addroff += SCA_BSIZE * scp1->sp_ntxdesc;

		scp1->sp_rxbuf_p = paddr0 + addroff;
		scp1->sp_rxbuf = vaddr0 + addroff;
		addroff += SCA_BSIZE * scp1->sp_nrxdesc;
	}

	/*
	 * as a consistancy check, addroff should be equal to the allocation
	 * size.
	 */
	if (sc->scu_allocsize != addroff)
		printf("ERROR:  scu_allocsize != addroff: %lu != %lu\n",
		       (u_long)sc->scu_allocsize, addroff);
}

#if __NetBSD_Version__ >= 104160000
static void
ntwoc_pci_config_interrupts(device_t self)
{
	struct ntwoc_pci_softc *sc;

	sc = device_private(self);
	sca_get_base_clock(&sc->sc_sca);
	sca_print_clock_info(&sc->sc_sca);
}
#endif

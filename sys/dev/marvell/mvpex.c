/*	$NetBSD: mvpex.c,v 1.15 2015/10/02 05:22:52 msaitoh Exp $	*/
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
__KERNEL_RCSID(0, "$NetBSD: mvpex.c,v 1.15 2015/10/02 05:22:52 msaitoh Exp $");

#include "opt_pci.h"
#include "pci.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/extent.h>
#include <sys/evcnt.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <prop/proplib.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pciconf.h>

#include <dev/marvell/mvpexreg.h>
#include <dev/marvell/mvpexvar.h>
#include <dev/marvell/marvellreg.h>
#include <dev/marvell/marvellvar.h>

#include <machine/pci_machdep.h>

#include "locators.h"


static int mvpex_match(device_t, struct cfdata *, void *);
static void mvpex_attach(device_t, device_t, void *);

static int mvpex_intr(void *);

static void mvpex_init(struct mvpex_softc *, enum marvell_tags *);
#if 0	/* shall move to pchb(4)? */
static void mvpex_barinit(struct mvpex_softc *);
static int mvpex_wininit(struct mvpex_softc *, int, int, int, int, uint32_t *,
			 uint32_t *);
#else
static void mvpex_wininit(struct mvpex_softc *, enum marvell_tags *);
#endif
#if NPCI > 0
static void mvpex_pci_config(struct mvpex_softc *, bus_space_tag_t,
			     bus_space_tag_t, bus_dma_tag_t, pci_chipset_tag_t,
			     u_long, u_long, u_long, u_long, int);
#endif

enum marvell_tags *mvpex_bar2_tags;

CFATTACH_DECL_NEW(mvpex_gt, sizeof(struct mvpex_softc),
    mvpex_match, mvpex_attach, NULL, NULL);
CFATTACH_DECL_NEW(mvpex_mbus, sizeof(struct mvpex_softc),
    mvpex_match, mvpex_attach, NULL, NULL);


/* ARGSUSED */
static int
mvpex_match(device_t parent, struct cfdata *match, void *aux)
{
	struct marvell_attach_args *mva = aux;

	if (strcmp(mva->mva_name, match->cf_name) != 0)
		return 0;
	if (mva->mva_offset == MVA_OFFSET_DEFAULT ||
	    mva->mva_irq == MVA_IRQ_DEFAULT)
		return 0;

	mva->mva_size = MVPEX_SIZE;
	return 1;
}

/* ARGSUSED */
static void
mvpex_attach(device_t parent, device_t self, void *aux)
{
	struct mvpex_softc *sc = device_private(self);
	struct marvell_attach_args *mva = aux;
#if NPCI > 0
	prop_dictionary_t dict = device_properties(self);
	prop_object_t pc, iot, memt;
	pci_chipset_tag_t mvpex_chipset;
	bus_space_tag_t mvpex_io_bs_tag, mvpex_mem_bs_tag;
	uint64_t iostart = 0, ioend = 0, memstart = 0, memend = 0;
	uint32_t cl_size = 0;
	int i;
#endif

	aprint_normal(": Marvell PCI Express Interface\n");
	aprint_naive("\n");

#if NPCI > 0
	iot = prop_dictionary_get(dict, "io-bus-tag");
	if (iot == NULL) {
		aprint_error_dev(self, "no io-bus-tag property\n");
		return;
	}
	KASSERT(prop_object_type(iot) == PROP_TYPE_DATA);
	mvpex_io_bs_tag = __UNCONST(prop_data_data_nocopy(iot));
	memt = prop_dictionary_get(dict, "mem-bus-tag");
	if (memt == NULL) {
		aprint_error_dev(self, "no mem-bus-tag property\n");
		return;
	}
	KASSERT(prop_object_type(memt) == PROP_TYPE_DATA);
	mvpex_mem_bs_tag = __UNCONST(prop_data_data_nocopy(memt));
	pc = prop_dictionary_get(dict, "pci-chipset");
	if (pc == NULL) {
		aprint_error_dev(self, "no pci-chipset property\n");
		return;
	}
	KASSERT(prop_object_type(pc) == PROP_TYPE_DATA);
	mvpex_chipset = __UNCONST(prop_data_data_nocopy(pc));
#ifdef PCI_NETBSD_CONFIGURE
	if (!prop_dictionary_get_uint64(dict, "iostart", &iostart)) {
		aprint_error_dev(self, "no iostart property\n");
		return;
	}
	if (!prop_dictionary_get_uint64(dict, "ioend", &ioend)) {
		aprint_error_dev(self, "no ioend property\n");
		return;
	}
	if (!prop_dictionary_get_uint64(dict, "memstart", &memstart)) {
		aprint_error_dev(self, "no memstart property\n");
		return;
	}
	if (!prop_dictionary_get_uint64(dict, "memend", &memend)) {
		aprint_error_dev(self, "no memend property\n");
		return;
	}
	if (!prop_dictionary_get_uint32(dict, "cache-line-size", &cl_size)) {
		aprint_error_dev(self, "no cache-line-size property\n");
		return;
	}
#endif
#endif

	sc->sc_dev = self;
	sc->sc_model = mva->mva_model;
	sc->sc_rev = mva->mva_revision;
	sc->sc_offset = mva->mva_offset;
	sc->sc_iot = mva->mva_iot;

	/* Map I/O registers for mvpex */
	if (bus_space_subregion(mva->mva_iot, mva->mva_ioh, mva->mva_offset,
	    mva->mva_size, &sc->sc_ioh)) {
		aprint_error_dev(self, "can't map registers\n");
		return;
	}
	mvpex_init(sc, mva->mva_tags);

	/* XXX: looks seem good to specify level IPL_VM. */
	marvell_intr_establish(mva->mva_irq, IPL_VM, mvpex_intr, sc);

#if NPCI > 0
	for (i = 0; i < PCI_INTERRUPT_PIN_MAX; i++) {
		sc->sc_intrtab[i].intr_pin = PCI_INTERRUPT_PIN_A + i;
		sc->sc_intrtab[i].intr_refcnt = 0;
		LIST_INIT(&sc->sc_intrtab[i].intr_list);
	}

	mvpex_pci_config(sc, mvpex_io_bs_tag, mvpex_mem_bs_tag, mva->mva_dmat,
	    mvpex_chipset, iostart, ioend, memstart, memend, cl_size);
#endif
}

static int
mvpex_intr(void *arg)
{
	struct mvpex_softc *sc = (struct mvpex_softc *)arg;
	struct mvpex_intrhand *ih;
	struct mvpex_intrtab *intrtab;
	uint32_t ic, im;
	int handled = 0, pin, rv, i, s;

	for (;;) {
		ic = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVPEX_IC);
		im = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVPEX_IM);
		ic &= im;

		if (!ic)
			break;

		for (i = 0, pin = PCI_INTERRUPT_PIN_A;
		    i < PCI_INTERRUPT_PIN_MAX; pin++, i++) {
			if ((ic & MVPEX_I_PIN(pin)) == 0)
				continue;

			intrtab = &sc->sc_intrtab[i];
			LIST_FOREACH(ih, &intrtab->intr_list, ih_q) {
				s = _splraise(ih->ih_type);
				rv = (*ih->ih_func)(ih->ih_arg);
				splx(s);
				if (rv) {
					ih->ih_evcnt.ev_count++;
					handled++;
				}
			}
			bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_IC,
			    ~MVPEX_I_PIN(pin));
		}
	}

	return handled;
}


static void
mvpex_init(struct mvpex_softc *sc, enum marvell_tags *tags)
{
	uint32_t reg;
	int window;

	/*
	 * First implement Guideline (GL# PCI Express-2) Wrong Default Value
	 * to Transmitter Output Current (TXAMP) Relevant for: 88F5181-A1/B0/B1
	 * and 88F5281-B0
	 */
						/* Write the read command */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, 0x1b00, 0x80820000);
	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, 0x1b00);
	/* Prepare new data for write */
	reg &= ~0x7;		/* Clear bits [2:0] */
	reg |= 0x4;		/* Set the new value */
	reg &= ~0x80000000;	/* Set "write" command */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, 0x1b00, reg);

	for (window = 0; window < MVPEX_NWINDOW; window++)
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_WC(window), 0);

#if 0	/* shall move to pchb(4)? */
	mvpex_barinit(sc);
#else
	mvpex_wininit(sc, tags);
#endif

	/* Clear Interrupt Cause and Mask registers */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_IC, 0);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_IM, 0);

	/* now wait 60 ns to be sure the link is valid (spec compliant) */
	delay(1);
}

#if 0
static int
mvpex_wininit(struct mvpex_softc *sc, int window, int tbegin, int tend,
	      int barmap, uint32_t *barbase, uint32_t *barsize)
{
	uint32_t target, attr, base, size;
	int targetid;

	for (targetid = tbegin; targetid <= tend && window < MVPEX_NWINDOW;
	    targetid++) {
		if (orion_target(targetid, &target, &attr, &base, &size) == -1)
			continue;
		if (size == 0)
			continue;

		if (base < *barbase)
			*barbase = base;
		*barsize += size;

		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_WC(window),
		    MVPEX_WC_WINEN		|
		    barmap			|
		    MVPEX_WC_TARGET(target)	|
		    MVPEX_WC_ATTR(attr)		|
		    MVPEX_WC_SIZE(size));
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_WB(window),
		    MVPEX_WB_BASE(base));
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_WR(window), 0);
		window++;
	}

	return window;
}

/* shall move to pchb(4)? */
static void
mvpex_barinit(struct mvpex_softc *sc)
{
	const uint32_t barflag =
	    PCI_MAPREG_MEM_PREFETCHABLE_MASK | PCI_MAPREG_MEM_TYPE_64BIT;
	uint32_t base, size;
	int window = 0;

	marvell_winparams_by_tag(device_parent(sc->sc_dev),
	    ORION_TARGETID_INTERNALREG, NULL, NULL, &base, &size);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_BAR0INTERNAL,
	    barflag | (base & MVPEX_BAR0INTERNAL_MASK));
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_BAR0INTERNALH, 0);

	base = size = 0;
	window = mvpex_wininit(sc, window, ORION_TARGETID_SDRAM_CS0,
	    ORION_TARGETID_SDRAM_CS3, MVPEX_WC_BARMAP_BAR1, &base, &size);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_BAR1,
	    barflag | (base & MVPEX_BAR_MASK));
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_BAR1H, 0);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_BAR1C,
	    MVPEX_BARC_BARSIZE(size) | MVPEX_BARC_BAREN);

#if 0
	base = size = 0;
	if (sc->sc_model == MARVELL_ORION_1_88F1181)
		window = mvpex_wininit(sc, window, ORION_TARGETID_FLASH_CS,
		    ORION_TARGETID_DEVICE_BOOTCS,
		    MVPEX_WC_BARMAP_BAR2, &base, &size);
	else {
		window = mvpex_wininit(sc, window,
		    ORION_TARGETID_DEVICE_CS0, ORION_TARGETID_DEVICE_CS2,
		    MVPEX_WC_BARMAP_BAR2, &base, &size);
		window = mvpex_wininit(sc, window,
		    ORION_TARGETID_DEVICE_BOOTCS, ORION_TARGETID_DEVICE_BOOTCS,
		    MVPEX_WC_BARMAP_BAR2, &base, &size);
	}
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_BAR2,
	    barflag | (base & MVPEX_BAR_MASK));
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_BAR2H, 0);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_BAR2C,
	    MVPEX_BARC_BARSIZE(size) | MVPEX_BARC_BAREN);
#else
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_BAR2C, 0);
#endif
}
#else
static void
mvpex_wininit(struct mvpex_softc *sc, enum marvell_tags *tags)
{
	device_t pdev = device_parent(sc->sc_dev);
	uint64_t base;
	uint32_t size, bar;
	int target, attr, window, rv, i, j;

	for (window = 0, i = 0;
	    tags[i] != MARVELL_TAG_UNDEFINED && window < MVPEX_NWINDOW; i++) {
		rv = marvell_winparams_by_tag(pdev, tags[i],
		    &target, &attr, &base, &size);
		if (rv != 0 || size == 0)
			continue;

		if (base > 0xffffffffULL) {
			aprint_error_dev(sc->sc_dev,
			    "tag %d address 0x%llx not support\n",
			    tags[i], base);
			continue;
		}

		bar = MVPEX_WC_BARMAP_BAR1;
		if (mvpex_bar2_tags != NULL)
			for (j = 0; mvpex_bar2_tags[j] != MARVELL_TAG_UNDEFINED;
			    j++) {
				if (mvpex_bar2_tags[j] != tags[i])
					continue;
				bar = MVPEX_WC_BARMAP_BAR2;
				break;
			}

		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_WC(window),
		    MVPEX_WC_WINEN		|
		    bar				|
		    MVPEX_WC_TARGET(target)	|
		    MVPEX_WC_ATTR(attr)		|
		    MVPEX_WC_SIZE(size));
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_WB(window),
		    MVPEX_WB_BASE(base));
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_WR(window), 0);
		window++;
	}
	for ( ; window < MVPEX_NWINDOW; window++)
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_WC(window), 0);
}
#endif

#if NPCI > 0
static void
mvpex_pci_config(struct mvpex_softc *sc, bus_space_tag_t iot,
		 bus_space_tag_t memt, bus_dma_tag_t dmat, pci_chipset_tag_t pc,
		 u_long iostart, u_long ioend, u_long memstart, u_long memend,
		 int cacheline_size)
{
	struct pcibus_attach_args pba;
#ifdef PCI_NETBSD_CONFIGURE
	struct extent *ioext = NULL, *memext = NULL;
#endif
	uint32_t stat;

	stat = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVPEX_STAT);

#ifdef PCI_NETBSD_CONFIGURE
	ioext = extent_create("pexio", iostart, ioend, NULL, 0, EX_NOWAIT);
	memext = extent_create("pexmem", memstart, memend, NULL, 0, EX_NOWAIT);
	if (ioext != NULL && memext != NULL)
		pci_configure_bus(pc, ioext, memext, NULL,
		    MVPEX_STAT_PEXBUSNUM(stat), cacheline_size);
        else
		aprint_error_dev(sc->sc_dev, "can't create extent %s%s%s\n",
		    ioext == NULL ? "io" : "",
		    ioext == NULL && memext == NULL ? " and " : "",
		    memext == NULL ? "mem" : "");
	if (ioext != NULL)
		extent_destroy(ioext);
	if (memext != NULL)
		extent_destroy(memext);
#endif

	pba.pba_iot = iot;
	pba.pba_memt = memt;
	pba.pba_dmat = dmat;
	pba.pba_dmat64 = NULL;
	pba.pba_pc = pc;
	pba.pba_flags = PCI_FLAGS_IO_OKAY | PCI_FLAGS_MEM_OKAY;
	pba.pba_bus = MVPEX_STAT_PEXBUSNUM(stat);
	pba.pba_bridgetag = NULL;
	config_found_ia(sc->sc_dev, "pcibus", &pba, NULL);
}


/*
 * PCI-Express CPU dependent code
 */

/* ARGSUSED */
void
mvpex_attach_hook(device_t parent, device_t self,
		  struct pcibus_attach_args *pba)
{

	/* Nothing */
}

/*
 * Bit map for configuration register:
 *   [31]    ConfigEn
 *   [30:28] Reserved
 *   [27:24] ExtRegNum (PCI Express only)
 *   [23:16] BusNum
 *   [15:11] DevNum
 *   [10: 8] FunctNum
 *   [ 7: 2] RegNum
 *   [ 1: 0] reserved
 */

/* ARGSUSED */
int
mvpex_bus_maxdevs(void *v, int busno)
{

	return 32;	/* 32 device/bus */
}

/* ARGSUSED */
pcitag_t
mvpex_make_tag(void *v, int bus, int dev, int func)
{

	return (bus << 16) | (dev << 11) | (func << 8);
}

/* ARGSUSED */
void
mvpex_decompose_tag(void *v, pcitag_t tag, int *bp, int *dp, int *fp)
{

	if (bp != NULL)
		*bp = (tag >> 16) & 0xff;
	if (dp != NULL)
		*dp = (tag >> 11) & 0x1f;
	if (fp != NULL)
		*fp = (tag >> 8) & 0x07;
}

pcireg_t
mvpex_conf_read(void *v, pcitag_t tag, int reg)
{
	struct mvpex_softc *sc = v;
	pcireg_t addr, pci_cs;
	uint32_t stat;
	int bus, dev, func, pexbus, pexdev;

	if ((unsigned int)reg >= PCI_EXTCONF_SIZE)
		return -1;

	mvpex_decompose_tag(v, tag, &bus, &dev, &func);

	stat = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVPEX_STAT);
	pexbus = MVPEX_STAT_PEXBUSNUM(stat);
	pexdev = MVPEX_STAT_PEXDEVNUM(stat);
	if (bus != pexbus || dev != pexdev)
		if (stat & MVPEX_STAT_DLDOWN)
			return -1;

	if (bus == pexbus) {
		if (pexdev == 0) {
			if (dev != 1 && dev != pexdev)
				return -1;
		} else {
			if (dev != 0 && dev != pexdev)
				return -1;
		}
		if (func != 0)
			return -1;
	}

	addr = ((reg & 0xf00) << 24)  | tag | (reg & 0xfc);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_CA,
	    addr | MVPEX_CA_CONFIGEN);
	if ((addr | MVPEX_CA_CONFIGEN) !=
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVPEX_CA))
		return -1;

	pci_cs = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
	    PCI_COMMAND_STATUS_REG);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh,
	    PCI_COMMAND_STATUS_REG, pci_cs | PCI_STATUS_MASTER_ABORT);

	return bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVPEX_CD);
}

void
mvpex_conf_write(void *v, pcitag_t tag, int reg, pcireg_t data)
{
	struct mvpex_softc *sc = v;
	pcireg_t addr;
	uint32_t stat;
	int bus, dev, func, pexbus, pexdev;

	if ((unsigned int)reg >= PCI_EXTCONF_SIZE)
		return;

	mvpex_decompose_tag(v, tag, &bus, &dev, &func);

	stat = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVPEX_STAT);
	pexbus = MVPEX_STAT_PEXBUSNUM(stat);
	pexdev = MVPEX_STAT_PEXDEVNUM(stat);
	if (bus != pexbus || dev != pexdev)
		if (stat & MVPEX_STAT_DLDOWN)
			return;

	if (bus == pexbus) {
		if (pexdev == 0) {
			if (dev != 1 && dev != pexdev)
				return;
		} else {
			if (dev != 0 && dev != pexdev)
				return;
		}
		if (func != 0)
			return;
	}

	addr = ((reg & 0xf00) << 24)  | tag | (reg & 0xfc);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_CA,
	    addr | MVPEX_CA_CONFIGEN);
	if ((addr | MVPEX_CA_CONFIGEN) !=
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVPEX_CA))
		return;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_CD, data);
}

/* ARGSUSED */
int
mvpex_conf_hook(void *v, int bus, int dev, int func, pcireg_t id)
{

	if (bus == 0 && dev == 0)	/* don't configure GT */
		return 0;

	/* 
	 * Do not configure PCI Express root complex on MV78460 - avoid
	 * setting up IO and memory windows. 
	 * XXX: should also avoid that other Aramadas.
	 */
	else if ((dev == 0) && (PCI_PRODUCT(id) == MARVELL_ARMADAXP_MV78460))
		return 0;

	return PCI_CONF_DEFAULT;
}

int
mvpex_intr_map(const struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{

	switch (pa->pa_intrpin) {
	case PCI_INTERRUPT_PIN_A:
	case PCI_INTERRUPT_PIN_B:
	case PCI_INTERRUPT_PIN_C:
	case PCI_INTERRUPT_PIN_D:
		*ihp = pa->pa_intrpin;
		return 0;
	}
	return -1;
}

/* ARGSUSED */
const char *
mvpex_intr_string(void *v, pci_intr_handle_t pin, char *buf, size_t len)
{
	switch (pin) {
	case PCI_INTERRUPT_PIN_A:
	case PCI_INTERRUPT_PIN_B:
	case PCI_INTERRUPT_PIN_C:
	case PCI_INTERRUPT_PIN_D:
		break;

	default:
		return NULL;
	}
	snprintf(buf, len, "interrupt pin INT%c#", (char)('A' - 1 + pin));

	return buf;
}

/* ARGSUSED */
const struct evcnt *
mvpex_intr_evcnt(void *v, pci_intr_handle_t pin)
{

	return NULL;
}

/*
 * XXXX: Shall these functions use mutex(9) instead of spl(9)?
 *       MV78200 and MV64360 and after supports SMP.
 */

/* ARGSUSED */
void *
mvpex_intr_establish(void *v, pci_intr_handle_t pin, int ipl,
		     int (*intrhand)(void *), void *intrarg)
{
	struct mvpex_softc *sc = (struct mvpex_softc *)v;
	struct mvpex_intrtab *intrtab;
	struct mvpex_intrhand *pexih;
	uint32_t mask;
	int ih = pin - 1, s;

	intrtab = &sc->sc_intrtab[ih];

	KASSERT(pin == intrtab->intr_pin);

	pexih = malloc(sizeof(*pexih), M_DEVBUF, M_NOWAIT);
	if (pexih == NULL)
		return NULL;

	pexih->ih_func = intrhand;
	pexih->ih_arg = intrarg;
	pexih->ih_type = ipl;
	pexih->ih_intrtab = intrtab;
	mvpex_intr_string(v, pin, pexih->ih_evname, sizeof(pexih->ih_evname));
	evcnt_attach_dynamic(&pexih->ih_evcnt, EVCNT_TYPE_INTR, NULL, "mvpex",
	    pexih->ih_evname);

	s = splhigh();

	/* First, link it into the tables. */
	LIST_INSERT_HEAD(&intrtab->intr_list, pexih, ih_q);

	/* Now enable it. */
	if (intrtab->intr_refcnt++ == 0) {
		mask = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVPEX_IM);
		mask |= MVPEX_I_PIN(intrtab->intr_pin);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_IM, mask);
	}

	splx(s);

	return pexih;
}

void
mvpex_intr_disestablish(void *v, void *ih)
{
	struct mvpex_softc *sc = (struct mvpex_softc *)v;
	struct mvpex_intrtab *intrtab;
	struct mvpex_intrhand *pexih = ih;
	uint32_t mask;
	int s;

	evcnt_detach(&pexih->ih_evcnt);

	intrtab = pexih->ih_intrtab;

	s = splhigh();

	/*
	 * First, remove it from the table.
	 */
	LIST_REMOVE(pexih, ih_q);

	/* Now, disable it, if there is nothing remaining on the list. */
	if (intrtab->intr_refcnt-- == 1) {
		mask = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVPEX_IM);
		mask &= ~MVPEX_I_PIN(intrtab->intr_pin);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVPEX_IM, mask);
	}
	splx(s);

	free(pexih, M_DEVBUF);
}
#endif	/* NPCI > 0 */

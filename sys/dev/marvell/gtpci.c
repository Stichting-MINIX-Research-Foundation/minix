/*	$NetBSD: gtpci.c,v 1.32 2015/10/02 05:22:52 msaitoh Exp $	*/
/*
 * Copyright (c) 2008, 2009 KIYOHARA Takashi
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
__KERNEL_RCSID(0, "$NetBSD: gtpci.c,v 1.32 2015/10/02 05:22:52 msaitoh Exp $");

#include "opt_pci.h"
#include "pci.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/extent.h>
#include <sys/malloc.h>

#include <prop/proplib.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pciconf.h>

#include <dev/marvell/gtpcireg.h>
#include <dev/marvell/gtpcivar.h>
#include <dev/marvell/marvellreg.h>
#include <dev/marvell/marvellvar.h>

#include <machine/pci_machdep.h>

#include "locators.h"


#define GTPCI_READ(sc, r) \
	bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, r((sc)->sc_unit))
#define GTPCI_WRITE(sc, r, v) \
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, r((sc)->sc_unit), (v))
#define GTPCI_WRITE_AC(sc, r, n, v) \
    bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, r((sc)->sc_unit, (n)), (v))


static int gtpci_match(device_t, struct cfdata *, void *);
static void gtpci_attach(device_t, device_t, void *);

static void gtpci_init(struct gtpci_softc *, struct gtpci_prot *);
static void gtpci_barinit(struct gtpci_softc *);
static void gtpci_protinit(struct gtpci_softc *, struct gtpci_prot *);
#if NPCI > 0
static void gtpci_pci_config(struct gtpci_softc *, bus_space_tag_t,
			     bus_space_tag_t, bus_dma_tag_t, pci_chipset_tag_t,
			     u_long, u_long, u_long, u_long, int);
#endif


CFATTACH_DECL_NEW(gtpci_gt, sizeof(struct gtpci_softc),
    gtpci_match, gtpci_attach, NULL, NULL);
CFATTACH_DECL_NEW(gtpci_mbus, sizeof(struct gtpci_softc),
    gtpci_match, gtpci_attach, NULL, NULL);


/* ARGSUSED */
static int
gtpci_match(device_t parent, struct cfdata *match, void *aux)
{
	struct marvell_attach_args *mva = aux;

	if (strcmp(mva->mva_name, match->cf_name) != 0)
		return 0;

	if (mva->mva_unit == MVA_UNIT_DEFAULT)
		return 0;
	switch (mva->mva_model) {
	case MARVELL_DISCOVERY:
	case MARVELL_DISCOVERY_II:
	case MARVELL_DISCOVERY_III:
#if 0	/* XXXXX */
	case MARVELL_DISCOVERY_LT:
	case MARVELL_DISCOVERY_V:
	case MARVELL_DISCOVERY_VI:
#endif
		if (mva->mva_offset != MVA_OFFSET_DEFAULT)
			return 0;
	}

	mva->mva_size = GTPCI_SIZE;
	return 1;
}

/* ARGSUSED */
static void
gtpci_attach(device_t parent, device_t self, void *aux)
{
	struct gtpci_softc *sc = device_private(self);
	struct marvell_attach_args *mva = aux;
	struct gtpci_prot *gtpci_prot;
	prop_dictionary_t dict = device_properties(self);
	prop_object_t prot;
#if NPCI > 0
	prop_object_t pc, iot, memt;
	prop_array_t int2gpp;
	prop_object_t gpp;
	pci_chipset_tag_t gtpci_chipset;
	bus_space_tag_t gtpci_io_bs_tag, gtpci_mem_bs_tag;
	uint64_t iostart = 0, ioend = 0, memstart = 0, memend = 0;
	int cl_size = 0, intr;
#endif

	aprint_normal(": Marvell PCI Interface\n");
	aprint_naive("\n");

	prot = prop_dictionary_get(dict, "prot");
	if (prot != NULL) {
		KASSERT(prop_object_type(prot) == PROP_TYPE_DATA);
		gtpci_prot = __UNCONST(prop_data_data_nocopy(prot));
	} else {
		aprint_verbose_dev(self, "no protection property\n");
		gtpci_prot = NULL;
	}
#if NPCI > 0
	iot = prop_dictionary_get(dict, "io-bus-tag");
	if (iot != NULL) {
		KASSERT(prop_object_type(iot) == PROP_TYPE_DATA);
		gtpci_io_bs_tag = __UNCONST(prop_data_data_nocopy(iot));
	} else {
		aprint_error_dev(self, "no io-bus-tag property\n");
		gtpci_io_bs_tag = NULL;
	}
	memt = prop_dictionary_get(dict, "mem-bus-tag");
	if (memt != NULL) {
		KASSERT(prop_object_type(memt) == PROP_TYPE_DATA);
		gtpci_mem_bs_tag = __UNCONST(prop_data_data_nocopy(memt));
	} else {
		aprint_error_dev(self, "no mem-bus-tag property\n");
		gtpci_mem_bs_tag = NULL;
	}
	pc = prop_dictionary_get(dict, "pci-chipset");
	if (pc == NULL) {
		aprint_error_dev(self, "no pci-chipset property\n");
		return;
	}
	KASSERT(prop_object_type(pc) == PROP_TYPE_DATA);
	gtpci_chipset = __UNCONST(prop_data_data_nocopy(pc));
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
	sc->sc_unit = mva->mva_unit;
	sc->sc_iot = mva->mva_iot;
	if (bus_space_subregion(mva->mva_iot, mva->mva_ioh,
	    (mva->mva_offset != MVA_OFFSET_DEFAULT) ? mva->mva_offset : 0,
	    mva->mva_size, &sc->sc_ioh)) {
		aprint_error_dev(self, "can't map registers\n");
		return;
	}
	sc->sc_pc = gtpci_chipset;
	gtpci_init(sc, gtpci_prot);

#if NPCI > 0
	int2gpp = prop_dictionary_get(dict, "int2gpp");
	if (int2gpp != NULL) {
		if (prop_object_type(int2gpp) != PROP_TYPE_ARRAY) {
			aprint_error_dev(self, "int2gpp not an array\n");
			return;
		}
		aprint_normal_dev(self, "use intrrupt pin:");
		for (intr = PCI_INTERRUPT_PIN_A;
		    intr <= PCI_INTERRUPT_PIN_D &&
					intr < prop_array_count(int2gpp);
		    intr++) {
			gpp = prop_array_get(int2gpp, intr);
			if (prop_object_type(gpp) != PROP_TYPE_NUMBER) {
				aprint_error_dev(self,
				    "int2gpp[%d] not an number\n", intr);
				return;
			}
			aprint_normal(" %d",
			    (int)prop_number_integer_value(gpp));
		}
		aprint_normal("\n");
	}

	gtpci_pci_config(sc, gtpci_io_bs_tag, gtpci_mem_bs_tag, mva->mva_dmat,
	    gtpci_chipset, iostart, ioend, memstart, memend, cl_size);
#endif
}

static void
gtpci_init(struct gtpci_softc *sc, struct gtpci_prot *prot)
{
	uint32_t reg;

	/* First, all disable.  Also WA CQ 4382 (bit15 must set 1)*/
	GTPCI_WRITE(sc, GTPCI_BARE, GTPCI_BARE_ALLDISABLE | (1 << 15));

	/* Enable Internal Arbiter */
	reg = GTPCI_READ(sc, GTPCI_AC);
	reg |= GTPCI_AC_EN;
	GTPCI_WRITE(sc, GTPCI_AC, reg);

	gtpci_barinit(sc);
	if (prot != NULL)
		gtpci_protinit(sc, prot);

	reg = GTPCI_READ(sc, GTPCI_ADC);
	reg |= GTPCI_ADC_REMAPWRDIS;
	GTPCI_WRITE(sc, GTPCI_ADC, reg);

	/* enable CPU-2-PCI ordering */
	reg = GTPCI_READ(sc, GTPCI_C);
	reg |= GTPCI_C_CPU2PCIORDERING;
	GTPCI_WRITE(sc, GTPCI_C, reg);
}

static void
gtpci_barinit(struct gtpci_softc *sc)
{
	static const struct {
		int tag;
		int bars[2];	/* BAR Size registers */
		int bare;	/* Bits of Base Address Registers Enable */
		int func;
		int balow;
		int bahigh;
	} maps[] = {
		{ MARVELL_TAG_SDRAM_CS0,
		  { GTPCI_CS0BARS(0),	GTPCI_CS0BARS(1) },
		  GTPCI_BARE_CS0EN,	0, 0x10, 0x14 },
		{ MARVELL_TAG_SDRAM_CS1,
		  { GTPCI_CS1BARS(0),	GTPCI_CS1BARS(1) },
		  GTPCI_BARE_CS1EN,	0, 0x18, 0x1c },
		{ MARVELL_TAG_SDRAM_CS2,
		  { GTPCI_CS2BARS(0),	GTPCI_CS2BARS(1) },
		  GTPCI_BARE_CS2EN,	1, 0x10, 0x14 },
		{ MARVELL_TAG_SDRAM_CS3,
		  { GTPCI_CS3BARS(0),	GTPCI_CS3BARS(1) },
		  GTPCI_BARE_CS3EN,	1, 0x18, 0x1c },
#if 0
		{ ORION_TARGETID_INTERNALREG,
		  { -1,			-1 },
		  GTPCI_BARE_INTMEMEN,	0, 0x20, 0x24 },

		{ ORION_TARGETID_DEVICE_CS0,
		  { GTPCI_DCS0BARS(0),	GTPCI_DCS0BARS(1) },
		  GTPCI_BARE_DEVCS0EN,	2, 0x10, 0x14 },
		{ ORION_TARGETID_DEVICE_CS1,
		  { GTPCI_DCS1BARS(0),	GTPCI_DCS1BARS(1) },
		  GTPCI_BARE_DEVCS1EN,	2, 0x18, 0x1c },
		{ ORION_TARGETID_DEVICE_CS2,
		  { GTPCI_DCS2BARS(0),	GTPCI_DCS2BARS(1) },
		  GTPCI_BARE_DEVCS2EN,	2, 0x20, 0x24 },
		{ ORION_TARGETID_DEVICE_BOOTCS,
		  { GTPCI_BCSBARS(0),	GTPCI_BCSBARS(1) },
		  GTPCI_BARE_BOOTCSEN,	3, 0x18, 0x1c },
		{ P2P Mem0 BAR,
		  { GTPCI_P2PM0BARS(0),	GTPCI_P2PM0BARS(1) },
		  GTPCI_BARE_P2PMEM0EN,	4, 0x10, 0x14 },
		{ P2P I/O BAR,
		  { GTPCI_P2PIOBARS(0),	GTPCI_P2PIOBARS(1) },
		  GTPCI_BARE_P2PIO0EN,	4, 0x20, 0x24 },
		{ Expansion ROM BAR,
		  { GTPCI_EROMBARS(0),	GTPCI_EROMBARS(1) },
		  0,				},
#endif

		{ MARVELL_TAG_UNDEFINED,
		  { -1,			-1 },
		  -1,				-1, 0x00, 0x00 },
	};
	device_t pdev = device_parent(sc->sc_dev);
	uint64_t base;
	uint32_t size, bare;
	int map, rv;


	bare = GTPCI_BARE_ALLDISABLE;
	for (map = 0; maps[map].tag != MARVELL_TAG_UNDEFINED; map++) {
		rv = marvell_winparams_by_tag(pdev, maps[map].tag, NULL, NULL,
		    &base, &size);
		if (rv != 0 || size == 0)
			continue;

		if (maps[map].bars[sc->sc_unit] != -1)
			bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			    maps[map].bars[sc->sc_unit], GTPCI_BARSIZE(size));
		bare &= ~maps[map].bare;

#if 0	/* shall move to pchb(4)? */
		if (maps[map].func != -1) {
			pcitag_t tag;
			pcireg_t reg;
			int dev = GTPCI_P2PC_DEVNUM(p2pc);
			int bus = GTPCI_P2PC_BUSNUMBER(p2pc);
			uint32_t p2pc = GTPCI_READ(sc, GTPCI_P2PC);

			tag = gtpci_make_tag(NULL, bus, dev, maps[map].func);
			reg = gtpci_conf_read(sc, tag, maps[map].balow);
			reg &= ~GTPCI_BARLOW_MASK;
			reg |= GTPCI_BARLOW_BASE(base);
			gtpci_conf_write(sc, tag, maps[map].balow, reg);
			reg = gtpci_conf_read(sc, tag, maps[map].bahigh);
			reg = (base >> 16) >> 16;
			gtpci_conf_write(sc, tag, maps[map].bahigh, reg);
		}
#endif
	}
	GTPCI_WRITE(sc, GTPCI_BARE, bare);
}

static void
gtpci_protinit(struct gtpci_softc *sc, struct gtpci_prot *ac_flags)
{
	enum {
		gt642xx = 0,
		mv643xx,
		arm_soc,
	};
	const struct gtpci_ac_rshift {
		uint32_t base_rshift;
		uint32_t size_rshift;
	} ac_rshifts[] = {
		{ 20, 20, },	/* GT642xx */
		{  0,  0, },	/* MV643xx and after */
		{  0,  0, },	/* ARM SoC */
	};
	const uint32_t prot_tags[] = {
		MARVELL_TAG_SDRAM_CS0,
		MARVELL_TAG_SDRAM_CS1,
		MARVELL_TAG_SDRAM_CS2,
		MARVELL_TAG_SDRAM_CS3,
		MARVELL_TAG_UNDEFINED
	};
	device_t pdev = device_parent(sc->sc_dev);
	uint64_t acbase, base;
	uint32_t acsize, size;
	int base_rshift, size_rshift, acbl_flags, acs_flags;
	int prot, rv, p, t;

	switch (sc->sc_model) {
	case MARVELL_DISCOVERY:
		p = gt642xx;
		break;

	case MARVELL_DISCOVERY_II:
	case MARVELL_DISCOVERY_III:
#if 0
	case MARVELL_DISCOVERY_LT:
	case MARVELL_DISCOVERY_V:
	case MARVELL_DISCOVERY_VI:
#endif
		p = mv643xx;
		break;

	default:
		p = arm_soc;
		break;
	}
	base_rshift = ac_rshifts[p].base_rshift;
	size_rshift = ac_rshifts[p].size_rshift;
	acbl_flags = ac_flags->acbl_flags;
	acs_flags = ac_flags->acs_flags;

	t = 0;
	for (prot = 0; prot < GTPCI_NPCIAC; prot++) {
		acbase = acsize = 0;

		for ( ; prot_tags[t] != MARVELL_TAG_UNDEFINED; t++) {
			rv = marvell_winparams_by_tag(pdev, prot_tags[t],
			    NULL, NULL, &base, &size);
			if (rv != 0 || size == 0)
				continue;

			if (acsize == 0 || base + size == acbase)
				acbase = base;
			else if (acbase + acsize != base)
				break;
			acsize += size;
		}

		if (acsize != 0) {
			GTPCI_WRITE_AC(sc, GTPCI_ACBL, prot,
			   ((acbase & 0xffffffff) >> base_rshift) | acbl_flags);
			GTPCI_WRITE_AC(sc, GTPCI_ACBH, prot,
			    (acbase >> 32) & 0xffffffff);
			GTPCI_WRITE_AC(sc, GTPCI_ACS, prot,
			    ((acsize - 1) >> size_rshift) | acs_flags);
		} else {
			GTPCI_WRITE_AC(sc, GTPCI_ACBL, prot, 0);
			GTPCI_WRITE_AC(sc, GTPCI_ACBH, prot, 0);
			GTPCI_WRITE_AC(sc, GTPCI_ACS, prot, 0);
		}
	}
	return;
}

#if NPCI > 0
static void
gtpci_pci_config(struct gtpci_softc *sc, bus_space_tag_t iot,
		 bus_space_tag_t memt, bus_dma_tag_t dmat, pci_chipset_tag_t pc,
		 u_long iostart, u_long ioend, u_long memstart, u_long memend,
		 int cacheline_size)
{
	struct pcibus_attach_args pba;
#ifdef PCI_NETBSD_CONFIGURE
	struct extent *ioext = NULL, *memext = NULL;
#endif
	uint32_t p2pc, command;

	p2pc = GTPCI_READ(sc, GTPCI_P2PC);

#ifdef PCI_NETBSD_CONFIGURE
	ioext = extent_create("pciio", iostart, ioend, NULL, 0, EX_NOWAIT);
	memext = extent_create("pcimem", memstart, memend, NULL, 0, EX_NOWAIT);
	if (ioext != NULL && memext != NULL)
		pci_configure_bus(pc, ioext, memext, NULL,
		    GTPCI_P2PC_BUSNUMBER(p2pc), cacheline_size);
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
	if (iot == NULL || memt == NULL) {
		pba.pba_flags = 0;
		aprint_error_dev(sc->sc_dev, "");
		if (iot == NULL)
			aprint_error("io ");
		else
			pba.pba_flags |= PCI_FLAGS_IO_OKAY;
		if (iot == NULL && memt == NULL)
			aprint_error("and ");
		if (memt == NULL)
			aprint_error("mem");
		else
			pba.pba_flags |= PCI_FLAGS_MEM_OKAY;
		aprint_error(" access disabled\n");
	} else
		pba.pba_flags = PCI_FLAGS_IO_OKAY | PCI_FLAGS_MEM_OKAY;
	command = GTPCI_READ(sc, GTPCI_C);
	if (command & GTPCI_C_MRDMUL)
		pba.pba_flags |= PCI_FLAGS_MRM_OKAY;
	if (command & GTPCI_C_MRDLINE)
		pba.pba_flags |= PCI_FLAGS_MRL_OKAY;
	pba.pba_flags |= PCI_FLAGS_MWI_OKAY;
	pba.pba_bus = GTPCI_P2PC_BUSNUMBER(p2pc);
	pba.pba_bridgetag = NULL;
	config_found_ia(sc->sc_dev, "pcibus", &pba, NULL);
}


/*
 * Dependent code of PCI Interface of Marvell
 */

/* ARGSUSED */
void
gtpci_attach_hook(device_t parent, device_t self,
		  struct pcibus_attach_args *pba)
{

	/* Nothing */
}

/*
 * Bit map for configuration register:
 *   [31]    ConfigEn
 *   [30:24] Reserved
 *   [23:16] BusNum
 *   [15:11] DevNum
 *   [10: 8] FunctNum
 *   [ 7: 2] RegNum
 *   [ 1: 0] reserved
 */

/* ARGSUSED */
int
gtpci_bus_maxdevs(void *v, int busno)
{

	return 32;	/* 32 device/bus */
}

/* ARGSUSED */
pcitag_t
gtpci_make_tag(void *v, int bus, int dev, int func)
{

#if DIAGNOSTIC
	if (bus >= 256 || dev >= 32 || func >= 8)
		panic("pci_make_tag: bad request");
#endif

	return (bus << 16) | (dev << 11) | (func << 8);
}

/* ARGSUSED */
void
gtpci_decompose_tag(void *v, pcitag_t tag, int *bp, int *dp, int *fp)
{

	if (bp != NULL)
		*bp = (tag >> 16) & 0xff;
	if (dp != NULL)
		*dp = (tag >> 11) & 0x1f;
	if (fp != NULL)
		*fp = (tag >> 8) & 0x07;
}

pcireg_t
gtpci_conf_read(void *v, pcitag_t tag, int reg)
{
	struct gtpci_softc *sc = v;
	const pcireg_t addr = tag | reg;

	if ((unsigned int)reg >= PCI_CONF_SIZE)
		return -1;

	GTPCI_WRITE(sc, GTPCI_CA, addr | GTPCI_CA_CONFIGEN);
	if ((addr | GTPCI_CA_CONFIGEN) != GTPCI_READ(sc, GTPCI_CA))
		return -1;

	return GTPCI_READ(sc, GTPCI_CD);
}

void
gtpci_conf_write(void *v, pcitag_t tag, int reg, pcireg_t data)
{
	struct gtpci_softc *sc = v;
	pcireg_t addr = tag | (reg & 0xfc);

	if ((unsigned int)reg >= PCI_CONF_SIZE)
		return;

	GTPCI_WRITE(sc, GTPCI_CA, addr | GTPCI_CA_CONFIGEN);
	if ((addr | GTPCI_CA_CONFIGEN) != GTPCI_READ(sc, GTPCI_CA))
		return;

	GTPCI_WRITE(sc, GTPCI_CD, data);
}

/* ARGSUSED */
int
gtpci_conf_hook(void *v, int bus, int dev, int func, pcireg_t id)
{
	/* Oops, We have two PCI buses. */
	if (dev == 0 &&
	    PCI_VENDOR(id) == PCI_VENDOR_MARVELL) {
		switch (PCI_PRODUCT(id)) {
		case MARVELL_DISCOVERY:
		case MARVELL_DISCOVERY_II:
		case MARVELL_DISCOVERY_III:
#if 0
		case MARVELL_DISCOVERY_LT:
		case MARVELL_DISCOVERY_V:
		case MARVELL_DISCOVERY_VI:
#endif
		case MARVELL_ORION_1_88F5180N:
		case MARVELL_ORION_1_88F5181:
		case MARVELL_ORION_1_88F5182:
		case MARVELL_ORION_2_88F5281:
		case MARVELL_ORION_1_88W8660:
			/* Don't configure us. */
			return 0;
		}
	}

	return PCI_CONF_DEFAULT;
}
#endif	/* NPCI > 0 */

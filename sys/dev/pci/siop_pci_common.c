/*	$NetBSD: siop_pci_common.c,v 1.35 2014/03/29 19:28:25 christos Exp $	*/

/*
 * Copyright (c) 2000 Manuel Bouyer.
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

/* SYM53c8xx PCI-SCSI I/O Processors driver: PCI front-end */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: siop_pci_common.c,v 1.35 2014/03/29 19:28:25 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/kernel.h>

#include <machine/endian.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsipiconf.h>

#include <dev/ic/siopreg.h>
#include <dev/ic/siopvar_common.h>
#include <dev/pci/siop_pci_common.h>

/* List (array, really :) of chips we know how to handle */
static const struct siop_product_desc siop_products[] = {
	{ PCI_PRODUCT_SYMBIOS_810,
	0x00,
	"Symbios Logic 53c810 (fast scsi)",
	SF_PCI_RL | SF_CHIP_LS,
	4, 8, 3, 250, 0
	},
	{ PCI_PRODUCT_SYMBIOS_810,
	0x10,
	"Symbios Logic 53c810a (fast scsi)",
	SF_PCI_RL | SF_PCI_BOF | SF_CHIP_PF | SF_CHIP_LS,
	4, 8, 3, 250, 0
	},
	{ PCI_PRODUCT_SYMBIOS_815,
	0x00,
	"Symbios Logic 53c815 (fast scsi)",
	SF_PCI_RL | SF_PCI_BOF,
	4, 8, 3, 250, 0
	},
	{ PCI_PRODUCT_SYMBIOS_820,
	0x00,
	"Symbios Logic 53c820 (fast wide scsi)",
	SF_PCI_RL | SF_CHIP_LS | SF_BUS_WIDE,
	4, 8, 3, 250, 0
	},
	{ PCI_PRODUCT_SYMBIOS_825,
	0x00,
	"Symbios Logic 53c825 (fast wide scsi)",
	SF_PCI_RL | SF_PCI_BOF | SF_BUS_WIDE,
	4, 8, 3, 250, 0
	},
	{ PCI_PRODUCT_SYMBIOS_825,
	0x10,
	"Symbios Logic 53c825a (fast wide scsi)",
	SF_PCI_RL | SF_PCI_CLS | SF_PCI_WRI | SF_PCI_RM |
	SF_CHIP_FIFO | SF_CHIP_PF | SF_CHIP_RAM | SF_CHIP_LS | SF_CHIP_10REGS |
	SF_BUS_WIDE,
	7, 8, 3, 250, 4096
	},
	{ PCI_PRODUCT_SYMBIOS_860,
	0x00,
	"Symbios Logic 53c860 (ultra scsi)",
	SF_PCI_RL | SF_PCI_CLS | SF_PCI_WRI | SF_PCI_RM |
	SF_CHIP_PF | SF_CHIP_LS |
	SF_BUS_ULTRA,
	4, 8, 5, 125, 0
	},
	{ PCI_PRODUCT_SYMBIOS_875,
	0x00,
	"Symbios Logic 53c875 (ultra-wide scsi)",
	SF_PCI_RL | SF_PCI_CLS | SF_PCI_WRI | SF_PCI_RM |
	SF_CHIP_FIFO | SF_CHIP_PF | SF_CHIP_RAM | SF_CHIP_LS | SF_CHIP_10REGS |
	SF_BUS_ULTRA | SF_BUS_WIDE,
	7, 16, 5, 125, 4096
	},
	{ PCI_PRODUCT_SYMBIOS_875,
	0x02,
	"Symbios Logic 53c875 (ultra-wide scsi)",
	SF_PCI_RL | SF_PCI_CLS | SF_PCI_WRI | SF_PCI_RM |
	SF_CHIP_FIFO | SF_CHIP_PF | SF_CHIP_RAM | SF_CHIP_DBLR |
	SF_CHIP_LS | SF_CHIP_10REGS |
	SF_BUS_ULTRA | SF_BUS_WIDE,
	7, 16, 5, 125, 4096
	},
	{ PCI_PRODUCT_SYMBIOS_875J,
	0x00,
	"Symbios Logic 53c875j (ultra-wide scsi)",
	SF_PCI_RL | SF_PCI_CLS | SF_PCI_WRI | SF_PCI_RM |
	SF_CHIP_FIFO | SF_CHIP_PF | SF_CHIP_RAM | SF_CHIP_DBLR |
	SF_CHIP_LS | SF_CHIP_10REGS |
	SF_BUS_ULTRA | SF_BUS_WIDE,
	7, 16, 5, 125, 4096
	},
	{ PCI_PRODUCT_SYMBIOS_885,
	0x00,
	"Symbios Logic 53c885 (ultra-wide scsi)",
	SF_PCI_RL | SF_PCI_CLS | SF_PCI_WRI | SF_PCI_RM |
	SF_CHIP_FIFO | SF_CHIP_PF | SF_CHIP_RAM | SF_CHIP_DBLR |
	SF_CHIP_LS | SF_CHIP_10REGS |
	SF_BUS_ULTRA | SF_BUS_WIDE,
	7, 16, 5, 125, 4096
	},
	{ PCI_PRODUCT_SYMBIOS_895,
	0x00,
	"Symbios Logic 53c895 (ultra2-wide scsi)",
	SF_PCI_RL | SF_PCI_CLS | SF_PCI_WRI | SF_PCI_RM |
	SF_CHIP_FIFO | SF_CHIP_PF | SF_CHIP_RAM | SF_CHIP_QUAD |
	SF_CHIP_LS | SF_CHIP_10REGS |
	SF_BUS_ULTRA2 | SF_BUS_WIDE,
	7, 31, 7, 62, 4096
	},
	{ PCI_PRODUCT_SYMBIOS_896,
	0x00,
	"Symbios Logic 53c896 (ultra2-wide scsi)",
	SF_PCI_RL | SF_PCI_CLS | SF_PCI_WRI | SF_PCI_RM |
	SF_CHIP_LEDC | SF_CHIP_FIFO | SF_CHIP_PF | SF_CHIP_RAM | SF_CHIP_QUAD |
	SF_CHIP_LS | SF_CHIP_10REGS |
	SF_BUS_ULTRA2 | SF_BUS_WIDE,
	7, 31, 7, 62, 8192
	},
	{ PCI_PRODUCT_SYMBIOS_895A,
	0x00,
	"Symbios Logic 53c895a (ultra2-wide scsi)",
	SF_PCI_RL | SF_PCI_CLS | SF_PCI_WRI | SF_PCI_RM |
	SF_CHIP_LEDC | SF_CHIP_FIFO | SF_CHIP_PF | SF_CHIP_RAM | SF_CHIP_QUAD |
	SF_CHIP_LS | SF_CHIP_10REGS |
	SF_BUS_ULTRA2 | SF_BUS_WIDE,
	7, 31, 7, 62, 8192
	},
	{ PCI_PRODUCT_SYMBIOS_1010,
	0x00,
	"Symbios Logic 53c1010-33 rev 0 (ultra3-wide scsi)",
	SF_PCI_RL | SF_PCI_CLS | SF_PCI_WRI | SF_PCI_RM |
	SF_CHIP_LEDC | SF_CHIP_FIFO | SF_CHIP_PF | SF_CHIP_RAM |
	SF_CHIP_LS | SF_CHIP_10REGS | SF_CHIP_DFBC | SF_CHIP_DBLR |
	SF_CHIP_GEBUG |
	SF_BUS_ULTRA3 | SF_BUS_WIDE,
	7, 31, 0, 62, 8192
	},
	{ PCI_PRODUCT_SYMBIOS_1010,
	0x01,
	"Symbios Logic 53c1010-33 (ultra3-wide scsi)",
	SF_PCI_RL | SF_PCI_CLS | SF_PCI_WRI | SF_PCI_RM |
	SF_CHIP_LEDC | SF_CHIP_FIFO | SF_CHIP_PF | SF_CHIP_RAM |
	SF_CHIP_LS | SF_CHIP_10REGS | SF_CHIP_DFBC | SF_CHIP_DBLR | SF_CHIP_DT |
	SF_CHIP_GEBUG |
	SF_BUS_ULTRA3 | SF_BUS_WIDE,
	7, 62, 0, 62, 8192
	},
	{ PCI_PRODUCT_SYMBIOS_1010_2,
	0x00,
	"Symbios Logic 53c1010-66 (ultra3-wide scsi)",
	SF_PCI_RL | SF_PCI_CLS | SF_PCI_WRI | SF_PCI_RM |
	SF_CHIP_LEDC | SF_CHIP_FIFO | SF_CHIP_PF | SF_CHIP_RAM |
	SF_CHIP_LS | SF_CHIP_10REGS | SF_CHIP_DFBC | SF_CHIP_DBLR | SF_CHIP_DT |
	SF_CHIP_AAIP |
	SF_BUS_ULTRA3 | SF_BUS_WIDE,
	7, 62, 0, 62, 8192
	},
	{ PCI_PRODUCT_SYMBIOS_1510D,
	0x00,
	"Symbios Logic 53c1510d (ultra2-wide scsi)",
	SF_PCI_RL | SF_PCI_CLS | SF_PCI_WRI | SF_PCI_RM |
	SF_CHIP_FIFO | SF_CHIP_PF | SF_CHIP_RAM | SF_CHIP_QUAD |
	SF_CHIP_LS | SF_CHIP_10REGS |
	SF_BUS_ULTRA2 | SF_BUS_WIDE,
	7, 31, 7, 62, 4096
	},
	{ 0,
	0x00,
	NULL,
	0x00,
	0, 0, 0, 0, 0
	},
};

const struct siop_product_desc *
siop_lookup_product(uint32_t id, int rev)
{
	const struct siop_product_desc *pp;
	const struct siop_product_desc *rp = NULL;

	if (PCI_VENDOR(id) != PCI_VENDOR_SYMBIOS)
		return NULL;

	for (pp = siop_products; pp->name != NULL; pp++) {
		if (PCI_PRODUCT(id) == pp->product && pp->revision <= rev)
			if (rp == NULL || pp->revision > rp->revision)
				rp = pp;
	}
	return rp;
}

int
siop_pci_attach_common(struct siop_pci_common_softc *pci_sc,
    struct siop_common_softc *siop_sc, struct pci_attach_args *pa,
    int (*intr)(void *))
{
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_tag;
	const char *intrstr;
	pci_intr_handle_t intrhandle;
	bus_space_tag_t iot, memt;
	bus_space_handle_t ioh, memh;
	pcireg_t memtype;
	prop_dictionary_t dict;
	int memh_valid, ioh_valid;
	bus_addr_t ioaddr, memaddr;
	bool use_pciclock;
	char intrbuf[PCI_INTRSTR_LEN];

	aprint_naive(": SCSI controller\n");

	pci_sc->sc_pp =
	    siop_lookup_product(pa->pa_id, PCI_REVISION(pa->pa_class));
	if (pci_sc->sc_pp == NULL) {
		aprint_error("sym: broken match/attach!!\n");
		return 0;
	}
	/* copy interesting infos about the chip */
	siop_sc->features = pci_sc->sc_pp->features;
#ifdef SIOP_SYMLED    /* XXX Should be a devprop! */
	siop_sc->features |= SF_CHIP_LED0;
#endif
	dict = device_properties(siop_sc->sc_dev);
	if (prop_dictionary_get_bool(dict, "use_pciclock", &use_pciclock))
		if (use_pciclock)
			siop_sc->features |= SF_CHIP_USEPCIC;
	siop_sc->maxburst = pci_sc->sc_pp->maxburst;
	siop_sc->maxoff = pci_sc->sc_pp->maxoff;
	siop_sc->clock_div = pci_sc->sc_pp->clock_div;
	siop_sc->clock_period = pci_sc->sc_pp->clock_period;
	siop_sc->ram_size = pci_sc->sc_pp->ram_size;

	siop_sc->sc_reset = siop_pci_reset;
	aprint_normal(": %s\n", pci_sc->sc_pp->name);
	pci_sc->sc_pc = pc;
	pci_sc->sc_tag = tag;
	siop_sc->sc_dmat = pa->pa_dmat;

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, 0x14);
	switch (memtype) {
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
		memh_valid = (pci_mapreg_map(pa, 0x14, memtype, 0,
		    &memt, &memh, &memaddr, NULL) == 0);
		break;
	default:
		memh_valid = 0;
	}

	ioh_valid = (pci_mapreg_map(pa, 0x10, PCI_MAPREG_TYPE_IO, 0,
	    &iot, &ioh, &ioaddr, NULL) == 0);

	if (memh_valid) {
		siop_sc->sc_rt = memt;
		siop_sc->sc_rh = memh;
		siop_sc->sc_raddr = memaddr;
	} else if (ioh_valid) {
		siop_sc->sc_rt = iot;
		siop_sc->sc_rh = ioh;
		siop_sc->sc_raddr = ioaddr;
	} else {
		aprint_error_dev(siop_sc->sc_dev,
		    "unable to map device registers\n");
		return 0;
	}

	if (siop_sc->features & SF_CHIP_RAM) {
		int bar;
		switch (memtype) {
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
			bar = 0x18;
			break;
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
			bar = 0x1c;
			break;
		default:
			aprint_error_dev(siop_sc->sc_dev,
			    "invalid memory type %d\n",
			    memtype);
			return 0;
		}
		if (pci_mapreg_map(pa, bar, memtype, 0,
                    &siop_sc->sc_ramt, &siop_sc->sc_ramh,
		    &siop_sc->sc_scriptaddr, NULL) == 0) {
			aprint_normal_dev(siop_sc->sc_dev,
			    "using on-board RAM\n");
		} else {
			aprint_error_dev(siop_sc->sc_dev,
			    "can't map on-board RAM\n");
			siop_sc->features &= ~SF_CHIP_RAM;
		}
	}

	if (pci_intr_map(pa, &intrhandle) != 0) {
		aprint_error_dev(siop_sc->sc_dev, "couldn't map interrupt\n");
		return 0;
	}
	intrstr = pci_intr_string(pa->pa_pc, intrhandle, intrbuf, sizeof(intrbuf));
	pci_sc->sc_ih = pci_intr_establish(pa->pa_pc, intrhandle, IPL_BIO,
	    intr, siop_sc);
	if (pci_sc->sc_ih != NULL) {
		aprint_normal_dev(siop_sc->sc_dev, "interrupting at %s\n",
		    intrstr ? intrstr : "unknown interrupt");
	} else {
		aprint_error_dev(siop_sc->sc_dev,
		    "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return 0;
	}
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG) |
	    PCI_COMMAND_MASTER_ENABLE);
	return 1;
}

void
siop_pci_reset(struct siop_common_softc *sc)
{
	int dmode;

	dmode = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_DMODE);
	if (sc->features & SF_PCI_RL)
		dmode |= DMODE_ERL;
	if (sc->features & SF_PCI_RM)
		dmode |= DMODE_ERMP;
	if (sc->features & SF_PCI_BOF)
		dmode |= DMODE_BOF;
	if (sc->features & SF_PCI_CLS)
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_DCNTL,
		    bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_DCNTL) |
		    DCNTL_CLSE);
	if (sc->features & SF_PCI_WRI)
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST3,
		    bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST3) |
		    CTEST3_WRIE);
	if (sc->maxburst) {
		int ctest5 = bus_space_read_1(sc->sc_rt, sc->sc_rh,
		    SIOP_CTEST5);
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST4,
		    bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST4) &
		    ~CTEST4_BDIS);
		dmode &= ~DMODE_BL_MASK;
		dmode |= ((sc->maxburst - 1) << DMODE_BL_SHIFT) & DMODE_BL_MASK;
		ctest5 &= ~CTEST5_BBCK;
		ctest5 |= (sc->maxburst - 1) & CTEST5_BBCK;
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST5, ctest5);
	} else {
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST4,
		    bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST4) |
		    CTEST4_BDIS);
	}
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_DMODE, dmode);
}

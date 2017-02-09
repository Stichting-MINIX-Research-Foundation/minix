/*	$NetBSD: jmide.c,v 1.19 2014/03/29 19:28:25 christos Exp $	*/

/*
 * Copyright (c) 2007 Manuel Bouyer.
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
__KERNEL_RCSID(0, "$NetBSD: jmide.c,v 1.19 2014/03/29 19:28:25 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>

#include <dev/pci/jmide_reg.h>

#include <dev/ic/ahcisatavar.h>

#include "jmide.h"

static const struct jmide_product *jmide_lookup(pcireg_t);

static int  jmide_match(device_t, cfdata_t, void *);
static void jmide_attach(device_t, device_t, void *);
static int  jmide_intr(void *);

static void jmpata_chip_map(struct pciide_softc*, const struct pci_attach_args*);
static void jmpata_setup_channel(struct ata_channel*);

static int  jmahci_print(void *, const char *);

struct jmide_product {
	u_int32_t jm_product;
	int jm_npata;
	int jm_nsata;
};

static const struct jmide_product jm_products[] =  {
	{ PCI_PRODUCT_JMICRON_JMB360,
	  0,
	  1
	},
	{ PCI_PRODUCT_JMICRON_JMB361,
	  1,
	  1
	},
	{ PCI_PRODUCT_JMICRON_JMB362,
	  0,
	  2
	},
	{ PCI_PRODUCT_JMICRON_JMB363,
	  1,
	  2
	},
	{ PCI_PRODUCT_JMICRON_JMB365,
	  2,
	  1
	},
	{ PCI_PRODUCT_JMICRON_JMB366,
	  2,
	  2
	},
	{ PCI_PRODUCT_JMICRON_JMB368,
	  1,
	  0
	},
	{ 0,
	  0,
	  0
	}
};

typedef enum {
	TYPE_INVALID = 0,
	TYPE_PATA,
	TYPE_SATA,
	TYPE_NONE
} jmchan_t;

struct jmide_softc {
	struct pciide_softc sc_pciide;
	device_t sc_ahci;
	int sc_npata;
	int sc_nsata;
	jmchan_t sc_chan_type[PCIIDE_NUM_CHANNELS];
	int sc_chan_swap;
};

struct jmahci_attach_args {
	const struct pci_attach_args *jma_pa;
	bus_space_tag_t jma_ahcit;
	bus_space_handle_t jma_ahcih;
};

#define JM_NAME(sc) (device_xname(sc->sc_pciide.sc_wdcdev.sc_atac.atac_dev))

CFATTACH_DECL_NEW(jmide, sizeof(struct jmide_softc),
    jmide_match, jmide_attach, NULL, NULL);

static const struct jmide_product *
jmide_lookup(pcireg_t id) {
	const struct jmide_product *jp;

	for (jp = jm_products; jp->jm_product != 0; jp++) {
		if (jp->jm_product == PCI_PRODUCT(id))
			return jp;
	}
	return NULL;
}

static int
jmide_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_JMICRON) {
		if (jmide_lookup(pa->pa_id))
			return (4); /* higher than ahcisata */
	}
	return (0);
}

static void
jmide_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct jmide_softc *sc = device_private(self);
	const struct jmide_product *jp;
	const char *intrstr;
        pci_intr_handle_t intrhandle;
	u_int32_t pcictrl0 = pci_conf_read(pa->pa_pc, pa->pa_tag,
	    PCI_JM_CONTROL0);
	u_int32_t pcictrl1 = pci_conf_read(pa->pa_pc, pa->pa_tag,
	    PCI_JM_CONTROL1);
	struct pciide_product_desc *pp;
	int ahci_used = 0;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_pciide.sc_wdcdev.sc_atac.atac_dev = self;

	jp = jmide_lookup(pa->pa_id);
	if (jp == NULL) {
		printf("jmide_attach: WTF?\n");
		return;
	}
	sc->sc_npata = jp->jm_npata;
	sc->sc_nsata = jp->jm_nsata;

        pci_aprint_devinfo(pa, "JMICRON PATA/SATA disk controller");

	aprint_normal("%s: ", JM_NAME(sc));
	if (sc->sc_npata)
		aprint_normal("%d PATA port%s", sc->sc_npata,
		    (sc->sc_npata > 1) ? "s" : "");
	if (sc->sc_nsata)
		aprint_normal("%s%d SATA port%s", sc->sc_npata ? ", " : "",
		    sc->sc_nsata, (sc->sc_nsata > 1) ? "s" : "");
	aprint_normal("\n");

	if (pci_intr_map(pa, &intrhandle) != 0) {
                aprint_error("%s: couldn't map interrupt\n", JM_NAME(sc));
                return;
        }
        intrstr = pci_intr_string(pa->pa_pc, intrhandle, intrbuf, sizeof(intrbuf));
        sc->sc_pciide.sc_pci_ih = pci_intr_establish(pa->pa_pc, intrhandle,
	    IPL_BIO, jmide_intr, sc);
        if (sc->sc_pciide.sc_pci_ih == NULL) {
                aprint_error("%s: couldn't establish interrupt", JM_NAME(sc));
                return;
        }
        aprint_normal("%s: interrupting at %s\n", JM_NAME(sc),
            intrstr ? intrstr : "unknown interrupt");

	if (pcictrl0 & JM_CONTROL0_AHCI_EN) {
		bus_size_t size;
		struct jmahci_attach_args jma;
		u_int32_t saved_pcictrl0;
		/*
		 * ahci controller enabled; disable sata on pciide and
		 * enable on ahci
		 */
		saved_pcictrl0 = pcictrl0;
		pcictrl0 |= JM_CONTROL0_SATA0_AHCI | JM_CONTROL0_SATA1_AHCI;
		pcictrl0 &= ~(JM_CONTROL0_SATA0_IDE | JM_CONTROL0_SATA1_IDE);
		pci_conf_write(pa->pa_pc, pa->pa_tag,
		    PCI_JM_CONTROL0, pcictrl0);
		/* attach ahci controller if on the right function */
		if ((pa->pa_function == 0 &&
		      (pcictrl0 & JM_CONTROL0_AHCI_F1) == 0) ||
	    	    (pa->pa_function == 1 &&
		      (pcictrl0 & JM_CONTROL0_AHCI_F1) != 0)) {
			jma.jma_pa = pa;
			/* map registers */
			if (pci_mapreg_map(pa, AHCI_PCI_ABAR,
			    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0,
			    &jma.jma_ahcit, &jma.jma_ahcih, NULL, &size) != 0) {
				aprint_error("%s: can't map ahci registers\n",
				    JM_NAME(sc));
			} else {
				sc->sc_ahci = config_found_ia(
				    sc->sc_pciide.sc_wdcdev.sc_atac.atac_dev,
				    "jmide_hl", &jma, jmahci_print);
			}
			/*
			 * if we couldn't attach an ahci, try to fall back
			 * to pciide. Note that this will not work if IDE
			 * is on function 0 and AHCI on function 1.
			 */
			if (sc->sc_ahci == NULL) {
				pcictrl0 = saved_pcictrl0 &
				    ~(JM_CONTROL0_SATA0_AHCI |
				      JM_CONTROL0_SATA1_AHCI |
				      JM_CONTROL0_AHCI_EN);
				pcictrl0 |= JM_CONTROL0_SATA1_IDE |
					JM_CONTROL0_SATA0_IDE;
				pci_conf_write(pa->pa_pc, pa->pa_tag,
				    PCI_JM_CONTROL0, pcictrl0);
			} else
				ahci_used = 1;
		}
	}
	sc->sc_chan_swap = ((pcictrl0 & JM_CONTROL0_PCIIDE_CS) != 0);
	/* compute the type of internal primary channel */
	if (pcictrl1 & JM_CONTROL1_PATA1_PRI) {
		if (sc->sc_npata > 1)
			sc->sc_chan_type[sc->sc_chan_swap ? 1 : 0] = TYPE_PATA;
		else
			sc->sc_chan_type[sc->sc_chan_swap ? 1 : 0] = TYPE_NONE;
	} else if (ahci_used == 0 && sc->sc_nsata > 0)
		sc->sc_chan_type[sc->sc_chan_swap ? 1 : 0] = TYPE_SATA;
	else
		sc->sc_chan_type[sc->sc_chan_swap ? 1 : 0] = TYPE_NONE;
	/* compute the type of internal secondary channel */
	if (sc->sc_nsata > 1 && ahci_used == 0 &&
	    (pcictrl0 & JM_CONTROL0_PCIIDE0_MS) == 0) {
		sc->sc_chan_type[sc->sc_chan_swap ? 0 : 1] = TYPE_SATA;
	} else {
		/* only a drive if first PATA enabled */
		if (sc->sc_npata > 0 && (pcictrl0 & JM_CONTROL0_PATA0_EN)
		    && (pcictrl0 &
		    (sc->sc_chan_swap ? JM_CONTROL0_PATA0_PRI: JM_CONTROL0_PATA0_SEC)))
			sc->sc_chan_type[sc->sc_chan_swap ? 0 : 1] = TYPE_PATA;
		else
			sc->sc_chan_type[sc->sc_chan_swap ? 0 : 1] = TYPE_NONE;
	}

	if (sc->sc_chan_type[0] == TYPE_NONE &&
	    sc->sc_chan_type[1] == TYPE_NONE)
		return;
	if (pa->pa_function == 0 && (pcictrl0 & JM_CONTROL0_PCIIDE_F1))
		return;
	if (pa->pa_function == 1 && (pcictrl0 & JM_CONTROL0_PCIIDE_F1) == 0)
		return;
	pp = malloc(sizeof(struct pciide_product_desc), M_DEVBUF, M_NOWAIT);
	if (pp == NULL) {
		aprint_error("%s: can't malloc sc_pp\n", JM_NAME(sc));
		return;
	}
	aprint_normal("%s: PCI IDE interface used", JM_NAME(sc));
	pp->ide_product = 0;
	pp->ide_flags = 0;
	pp->ide_name = NULL;
	pp->chip_map = jmpata_chip_map;
	pciide_common_attach(&sc->sc_pciide, pa, pp);
	
}

static int
jmide_intr(void *arg)
{
	struct jmide_softc *sc = arg;
	int ret = 0;

#ifdef NJMAHCI
	if (sc->sc_ahci)
		ret |= ahci_intr(device_private(sc->sc_ahci));
#endif
	if (sc->sc_npata)
		ret |= pciide_pci_intr(&sc->sc_pciide);
	return ret;
}

static void
jmpata_chip_map(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	struct jmide_softc *jmidesc = (struct jmide_softc *)sc;
	int channel;
	pcireg_t interface;
	struct pciide_channel *cp;

	if (pciide_chipen(sc, pa) == 0)
		return;
	aprint_verbose("%s: bus-master DMA support present", JM_NAME(jmidesc));
	pciide_mapreg_dma(sc, pa);
	aprint_verbose("\n");
	sc->sc_wdcdev.sc_atac.atac_cap = ATAC_CAP_DATA16 | ATAC_CAP_DATA32;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DMA | ATAC_CAP_UDMA;
		sc->sc_wdcdev.sc_atac.atac_udma_cap = 6;
	}
	sc->sc_wdcdev.sc_atac.atac_pio_cap = 4;
	sc->sc_wdcdev.sc_atac.atac_dma_cap = 2;
	sc->sc_wdcdev.sc_atac.atac_set_modes = jmpata_setup_channel;
	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = PCIIDE_NUM_CHANNELS;
	sc->sc_wdcdev.wdc_maxdrives = 2;
	wdc_allocate_regs(&sc->sc_wdcdev);
	/*
         * can't rely on the PCI_CLASS_REG content if the chip was in raid
         * mode. We have to fake interface
         */
	interface = PCIIDE_INTERFACE_PCI(0) | PCIIDE_INTERFACE_PCI(1);
	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
	    channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		aprint_normal("%s: %s channel is ", JM_NAME(jmidesc),
		    PCIIDE_CHANNEL_NAME(channel));
		switch(jmidesc->sc_chan_type[channel]) {
		case TYPE_PATA:
			aprint_normal("PATA");
			break;
		case TYPE_SATA:
			aprint_normal("SATA");
			break;
		case TYPE_NONE:
			aprint_normal("unused");
			break;
		default:
			aprint_normal("impossible");
			panic("jmide: wrong/uninitialised channel type");
		}
		aprint_normal("\n");
		if (jmidesc->sc_chan_type[channel] == TYPE_NONE) {
			cp->ata_channel.ch_flags |= ATACH_DISABLED;
			continue;
		}
		pciide_mapchan(pa, cp, interface, pciide_pci_intr);
	}
}

static void
jmpata_setup_channel(struct ata_channel *chp)
{
	struct ata_drive_datas *drvp;
	int drive, s;
	u_int32_t idedma_ctl;
	struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);
	struct jmide_softc *jmidesc = (struct jmide_softc *)sc;
	int ide80p;

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	idedma_ctl = 0;

	/* cable type detect */
	ide80p = 1;
	if (chp->ch_channel == (jmidesc->sc_chan_swap ? 1 : 0)) {
		if (jmidesc->sc_chan_type[chp->ch_channel] == TYPE_PATA &&
		    (pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_JM_CONTROL1) &
		    JM_CONTROL1_PATA1_40P))
			ide80p = 0;
	} else {
		if (jmidesc->sc_chan_type[chp->ch_channel] == TYPE_PATA &&
		    (pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_JM_CONTROL0) &
		    JM_CONTROL0_PATA0_40P))
			ide80p = 0;
	}

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if (drvp->drive_type == ATA_DRIVET_NONE)
			continue;
		if (drvp->drive_flags & ATA_DRIVE_UDMA) {
			/* use Ultra/DMA */
			s = splbio();
			drvp->drive_flags &= ~ATA_DRIVE_DMA;
			if (drvp->UDMA_mode > 2 && ide80p == 0)
				drvp->UDMA_mode = 2;
			splx(s);
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else if (drvp->drive_flags & ATA_DRIVE_DMA) {
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		}
	}
	/* nothing to do to setup modes, the controller snoop SET_FEATURE cmd */
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL],
		    0, idedma_ctl);
	}
}

static int
jmahci_print(void *aux, const char *pnp)
{
        if (pnp)
                aprint_normal("ahcisata at %s", pnp);

        return (UNCONF);
}


#ifdef NJMAHCI
static int  jmahci_match(device_t, cfdata_t, void *);
static void jmahci_attach(device_t, device_t, void *);
static int  jmahci_detach(device_t, int);
static bool jmahci_resume(device_t, const pmf_qual_t *);

CFATTACH_DECL_NEW(jmahci, sizeof(struct ahci_softc),
	jmahci_match, jmahci_attach, jmahci_detach, NULL);

static int
jmahci_match(device_t parent, cfdata_t match, void *aux)
{
	return 1;
}

static void
jmahci_attach(device_t parent, device_t self, void *aux)
{
	struct jmahci_attach_args *jma = aux;
	const struct pci_attach_args *pa = jma->jma_pa;
	struct ahci_softc *sc = device_private(self);
	uint32_t ahci_cap;

	aprint_naive(": AHCI disk controller\n");
	aprint_normal("\n");

	sc->sc_atac.atac_dev = self;
	sc->sc_ahcit = jma->jma_ahcit;
	sc->sc_ahcih = jma->jma_ahcih;

	ahci_cap = AHCI_READ(sc, AHCI_CAP);

	if (pci_dma64_available(jma->jma_pa) && (ahci_cap & AHCI_CAP_64BIT))
		sc->sc_dmat = jma->jma_pa->pa_dmat64;
	else
		sc->sc_dmat = jma->jma_pa->pa_dmat;

	if (PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MASS_STORAGE_RAID)
		sc->sc_atac_capflags = ATAC_CAP_RAID;

	ahci_attach(sc);

	if (!pmf_device_register(self, NULL, jmahci_resume))
	    aprint_error_dev(self, "couldn't establish power handler\n");
}

static int
jmahci_detach(device_t dv, int flags)
{
	struct ahci_softc *sc;
	sc = device_private(dv);

	int rv;

	if ((rv = ahci_detach(sc, flags)))
		return rv;

	return 0;
}

static bool
jmahci_resume(device_t dv, const pmf_qual_t *qual)
{
	struct ahci_softc *sc;
	int s;

	sc = device_private(dv);

	s = splbio();
	ahci_resume(sc);
	splx(s);

	return true;
}
#endif

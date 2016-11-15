/*	$NetBSD: svwsata.c,v 1.19 2014/03/29 19:28:25 christos Exp $	*/

/*
 * Copyright (c) 2005 Mark Kettenis
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: svwsata.c,v 1.19 2014/03/29 19:28:25 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/ata/atareg.h>
#include <dev/ata/satareg.h>
#include <dev/ata/satavar.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>
#include <dev/pci/pciide_svwsata_reg.h>

static int  svwsata_match(device_t, cfdata_t, void *);
static void svwsata_attach(device_t, device_t, void *);

static void svwsata_chip_map(struct pciide_softc *,
    const struct pci_attach_args *);
static void svwsata_mapreg_dma(struct pciide_softc *,
    const struct pci_attach_args *);
static void svwsata_mapchan(struct pciide_channel *);

CFATTACH_DECL_NEW(svwsata, sizeof(struct pciide_softc),
    svwsata_match, svwsata_attach, pciide_detach, NULL);

static const struct pciide_product_desc pciide_svwsata_products[] =  {
	{ PCI_PRODUCT_SERVERWORKS_K2_SATA,
	  0,
	  "ServerWorks K2 SATA Controller",
	  svwsata_chip_map
	},
	{ PCI_PRODUCT_SERVERWORKS_FRODO4_SATA,
	  0,
	  "ServerWorks Frodo4 SATA Controller",
	  svwsata_chip_map
	},
	{ PCI_PRODUCT_SERVERWORKS_FRODO8_SATA,
	  0,
	  "ServerWorks Frodo8 SATA Controller",
	  svwsata_chip_map
	},
	{ PCI_PRODUCT_SERVERWORKS_HT1000_SATA_1,
	  0,
	  "ServerWorks HT-1000 SATA Controller",
	  svwsata_chip_map
	},
	{ PCI_PRODUCT_SERVERWORKS_HT1000_SATA_2,
	  0,
	  "ServerWorks HT-1000 SATA Controller",
	  svwsata_chip_map
	},
	{ 0,
	  0,
	  NULL,
	  NULL,
	}
};

static int
svwsata_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SERVERWORKS) {
		if (pciide_lookup_product(pa->pa_id,
		    pciide_svwsata_products))
			return (2);
	}
	return (0);
}

static void
svwsata_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct pciide_softc *sc = device_private(self);

	sc->sc_wdcdev.sc_atac.atac_dev = self;

	pciide_common_attach(sc, pa,
	    pciide_lookup_product(pa->pa_id, pciide_svwsata_products));
}

static void
svwsata_chip_map(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	pci_intr_handle_t intrhandle;
	pcireg_t interface;
	const char *intrstr;
	int channel;
	char intrbuf[PCI_INTRSTR_LEN];

	/* The 4-port version has a dummy second function. */
	if (pci_conf_read(sc->sc_pc, sc->sc_tag,
	    PCI_MAPREG_START + 0x14) == 0) {
		aprint_normal("\n");
		return;
	}

	if (pci_mapreg_map(pa, PCI_MAPREG_START + 0x14,
			   PCI_MAPREG_TYPE_MEM |
			   PCI_MAPREG_MEM_TYPE_32BIT, 0,
			   &sc->sc_ba5_st, &sc->sc_ba5_sh,
			   NULL, &sc->sc_ba5_ss) != 0) {
		aprint_error(": unable to map BA5 register space\n");
		return;
	}

	aprint_verbose_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "bus-master DMA support present");
	svwsata_mapreg_dma(sc, pa);
	aprint_verbose("\n");

	sc->sc_wdcdev.cap = WDC_CAPABILITY_WIDEREGS;

	sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DATA16 | ATAC_CAP_DATA32;
	sc->sc_wdcdev.sc_atac.atac_pio_cap = 4;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DMA | ATAC_CAP_UDMA;
		sc->sc_wdcdev.irqack = pciide_irqack;
		sc->sc_wdcdev.sc_atac.atac_dma_cap = 2;
		sc->sc_wdcdev.sc_atac.atac_udma_cap = 6;
	}

	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = 4;
	sc->sc_wdcdev.sc_atac.atac_set_modes = sata_setup_channel;

	/* We can use SControl and SStatus to probe for drives. */
	sc->sc_wdcdev.sc_atac.atac_probe = wdc_sataprobe;
	sc->sc_wdcdev.wdc_maxdrives = 1;

	wdc_allocate_regs(&sc->sc_wdcdev);

	/* Map and establish the interrupt handler. */
	if(pci_intr_map(pa, &intrhandle) != 0) {
		aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "couldn't map native-PCI interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, intrhandle, intrbuf, sizeof(intrbuf));
	sc->sc_pci_ih = pci_intr_establish(pa->pa_pc, intrhandle, IPL_BIO,
	    pciide_pci_intr, sc);
	if (sc->sc_pci_ih != NULL) {
		aprint_normal_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "using %s for native-PCI interrupt\n",
		    intrstr ? intrstr : "unknown interrupt");
	} else {
		aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "couldn't establish native-PCI interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}

	interface = PCIIDE_INTERFACE_BUS_MASTER_DMA |
	    PCIIDE_INTERFACE_PCI(0) | PCIIDE_INTERFACE_PCI(1);


	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
	     channel++) {
		cp = &sc->pciide_channels[channel];

		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		svwsata_mapchan(cp);
	}
}

static void
svwsata_mapreg_dma(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	struct pciide_channel *pc;
	int chan, reg;
	bus_size_t size;

	sc->sc_wdcdev.dma_arg = sc;
	sc->sc_wdcdev.dma_init = pciide_dma_init;
	sc->sc_wdcdev.dma_start = pciide_dma_start;
	sc->sc_wdcdev.dma_finish = pciide_dma_finish;

	if (device_cfdata(sc->sc_wdcdev.sc_atac.atac_dev)->cf_flags &
	    PCIIDE_OPTIONS_NODMA) {
		aprint_normal(
		    ", but unused (forced off by config file)");
		sc->sc_dma_ok = 0;
		return;
	}

	/*
	 * Slice off a subregion of BA5 for each of the channel's DMA
	 * registers.
	 */

	sc->sc_dma_iot = sc->sc_ba5_st;
	for (chan = 0; chan < 4; chan++) {
		pc = &sc->pciide_channels[chan];
		for (reg = 0; reg < IDEDMA_NREGS; reg++) {
			size = 4;
			if (size > (IDEDMA_SCH_OFFSET - reg))
				size = IDEDMA_SCH_OFFSET - reg;
			if (bus_space_subregion(sc->sc_ba5_st,
			    sc->sc_ba5_sh,
			    (chan << 8) + SVWSATA_DMA + reg,
			    size, &pc->dma_iohs[reg]) != 0) {
				sc->sc_dma_ok = 0;
				aprint_normal(", but can't subregion offset "
				    "%lu size %lu",
				    (u_long) (chan << 8) + SVWSATA_DMA + reg,
				    (u_long) size);
				return;
			}
		}
	}

	/* DMA registers all set up! */
	sc->sc_dmat = pa->pa_dmat;
	sc->sc_dma_ok = 1;
}

static void
svwsata_mapchan(struct pciide_channel *cp)
{
	struct ata_channel *wdc_cp = &cp->ata_channel;
	struct pciide_softc *sc = CHAN_TO_PCIIDE(wdc_cp);
	struct wdc_regs *wdr = CHAN_TO_WDC_REGS(wdc_cp);
	int i;

	cp->compat = 0;
	cp->ih = sc->sc_pci_ih;

	wdr->cmd_iot = sc->sc_ba5_st;
	if (bus_space_subregion(sc->sc_ba5_st, sc->sc_ba5_sh,
		(wdc_cp->ch_channel << 8) + SVWSATA_TF0,
		SVWSATA_TF8 - SVWSATA_TF0, &wdr->cmd_baseioh) != 0) {
		aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "couldn't map %s cmd regs\n", cp->name);
		goto bad;
	}

	wdr->ctl_iot = sc->sc_ba5_st;
	if (bus_space_subregion(sc->sc_ba5_st, sc->sc_ba5_sh,
		(wdc_cp->ch_channel << 8) + SVWSATA_TF8, 4,
		&cp->ctl_baseioh) != 0) {
		aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "couldn't map %s ctl regs\n", cp->name);
		goto bad;
	}
	wdr->ctl_ioh = cp->ctl_baseioh;

	for (i = 0; i < WDC_NREG; i++) {
		if (bus_space_subregion(wdr->cmd_iot, wdr->cmd_baseioh,
					i << 2, i == 0 ? 4 : 1,
					&wdr->cmd_iohs[i]) != 0) {
			aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
			    "couldn't subregion %s channel cmd regs\n",
			    cp->name);
			goto bad;
		}
	}
	wdc_init_shadow_regs(wdc_cp);
	wdr->data32iot = wdr->cmd_iot;
	wdr->data32ioh = wdr->cmd_iohs[0];


	wdr->sata_iot = sc->sc_ba5_st;
	wdr->sata_baseioh = sc->sc_ba5_sh;
	if (bus_space_subregion(wdr->sata_iot, wdr->sata_baseioh,
	    (wdc_cp->ch_channel << 8) + SVWSATA_SSTATUS, 1,
	    &wdr->sata_status) != 0) {
		aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "couldn't map channel %d sata_status regs\n",
		    wdc_cp->ch_channel);
		goto bad;
	}
	if (bus_space_subregion(wdr->sata_iot, wdr->sata_baseioh,
	    (wdc_cp->ch_channel << 8) + SVWSATA_SERROR, 1,
	    &wdr->sata_error) != 0) {
		aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		   "couldn't map channel %d sata_error regs\n",
		    wdc_cp->ch_channel);
		goto bad;
	}
	if (bus_space_subregion(wdr->sata_iot, wdr->sata_baseioh,
	    (wdc_cp->ch_channel << 8) + SVWSATA_SCONTROL, 1,
	    &wdr->sata_control) != 0) {
		aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "couldn't map channel %d sata_control regs\n",
		    wdc_cp->ch_channel);
		goto bad;
	}

	bus_space_write_4(sc->sc_ba5_st, sc->sc_ba5_sh,
	    (wdc_cp->ch_channel << 8) + SVWSATA_SICR1,
	    bus_space_read_4(sc->sc_ba5_st, sc->sc_ba5_sh,
	        (wdc_cp->ch_channel << 8) + SVWSATA_SICR1)
	    & ~0x00040000);
	bus_space_write_4(sc->sc_ba5_st, sc->sc_ba5_sh,
	    (wdc_cp->ch_channel << 8) + SVWSATA_SERROR, 0xffffffff);
	bus_space_write_4(sc->sc_ba5_st, sc->sc_ba5_sh,
	    (wdc_cp->ch_channel << 8) + SVWSATA_SIM, 0);

	wdcattach(wdc_cp);
	return;

 bad:
	cp->ata_channel.ch_flags |= ATACH_DISABLED;
}

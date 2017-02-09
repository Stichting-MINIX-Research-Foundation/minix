/*	$NetBSD: pciide_common.c,v 1.60 2014/03/29 19:28:25 christos Exp $	*/


/*
 * Copyright (c) 1999, 2000, 2001, 2003 Manuel Bouyer.
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
 *
 */


/*
 * Copyright (c) 1996, 1998 Christopher G. Demetriou.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * PCI IDE controller driver.
 *
 * Author: Christopher G. Demetriou, March 2, 1998 (derived from NetBSD
 * sys/dev/pci/ppb.c, revision 1.16).
 *
 * See "PCI IDE Controller Specification, Revision 1.0 3/4/94" and
 * "Programming Interface for Bus Master IDE Controller, Revision 1.0
 * 5/16/94" from the PCI SIG.
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pciide_common.c,v 1.60 2014/03/29 19:28:25 christos Exp $");

#include <sys/param.h>
#include <sys/malloc.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>

#include <dev/ic/wdcreg.h>

#ifdef ATADEBUG
int atadebug_pciide_mask = 0;
#endif

#if NATA_DMA
static const char dmaerrfmt[] =
    "%s:%d: unable to %s table DMA map for drive %d, error=%d\n";
#endif

/* Default product description for devices not known from this controller */
const struct pciide_product_desc default_product_desc = {
	0,
	0,
	"Generic PCI IDE controller",
	default_chip_map,
};

const struct pciide_product_desc *
pciide_lookup_product(pcireg_t id, const struct pciide_product_desc *pp)
{
	for (; pp->chip_map != NULL; pp++)
		if (PCI_PRODUCT(id) == pp->ide_product)
			break;

	if (pp->chip_map == NULL)
		return NULL;
	return pp;
}

void
pciide_common_attach(struct pciide_softc *sc, const struct pci_attach_args *pa, const struct pciide_product_desc *pp)
{
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_tag;
#if NATA_DMA
	pcireg_t csr;
#endif
	const char *displaydev = NULL;
	int dontprint = 0;

	sc->sc_pci_id = pa->pa_id;
	if (pp == NULL) {
		/* should only happen for generic pciide devices */
		sc->sc_pp = &default_product_desc;
	} else {
		sc->sc_pp = pp;
		/* if ide_name == NULL, printf is done in chip-specific map */
		if (pp->ide_name)
			displaydev = pp->ide_name;
		else
			dontprint = 1;
	}

	if (dontprint) {
		aprint_naive("disk controller\n");
		aprint_normal("\n"); /* ??? */
	} else
		pci_aprint_devinfo_fancy(pa, "disk controller", displaydev, 1);

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;

#if NATA_DMA
	/* Set up DMA defaults; these might be adjusted by chip_map. */
	sc->sc_dma_maxsegsz = IDEDMA_BYTE_COUNT_MAX;
	sc->sc_dma_boundary = IDEDMA_BYTE_COUNT_ALIGN;
#endif

#ifdef ATADEBUG
	if (atadebug_pciide_mask & DEBUG_PROBE)
		pci_conf_print(sc->sc_pc, sc->sc_tag, NULL);
#endif
	sc->sc_pp->chip_map(sc, pa);

#if NATA_DMA
	if (sc->sc_dma_ok) {
		csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
		csr |= PCI_COMMAND_MASTER_ENABLE;
		pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, csr);
	}
#endif
	ATADEBUG_PRINT(("pciide: command/status register=%x\n",
	    pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG)), DEBUG_PROBE);
}

int
pciide_common_detach(struct pciide_softc *sc, int flags)
{
	struct pciide_channel *cp;
	struct ata_channel *wdc_cp;
	struct wdc_regs *wdr;
	int channel, drive;
	int rv;

	rv = wdcdetach(sc->sc_wdcdev.sc_atac.atac_dev, flags);
	if (rv)
		return rv;

	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
	     channel++) {
		cp = &sc->pciide_channels[channel];
		wdc_cp = &cp->ata_channel;
		wdr = CHAN_TO_WDC_REGS(wdc_cp);

		if (wdc_cp->ch_flags & ATACH_DISABLED)
			continue;

		if (wdr->cmd_ios != 0)
			bus_space_unmap(wdr->cmd_iot,
			    wdr->cmd_baseioh, wdr->cmd_ios);
		if (cp->compat != 0) {
			if (wdr->ctl_ios != 0)
				bus_space_unmap(wdr->ctl_iot,
				    wdr->ctl_ioh, wdr->ctl_ios);
		} else {
			if (cp->ctl_ios != 0)
				bus_space_unmap(wdr->ctl_iot,
				    cp->ctl_baseioh, cp->ctl_ios);
		}

		for (drive = 0; drive < sc->sc_wdcdev.wdc_maxdrives; drive++) {
#if NATA_DMA
			pciide_dma_table_teardown(sc, channel, drive);
#endif
		}

		free(cp->ata_channel.ch_queue, M_DEVBUF);
		cp->ata_channel.atabus = NULL;
	}

#if NATA_DMA
	if (sc->sc_dma_ios != 0)
		bus_space_unmap(sc->sc_dma_iot, sc->sc_dma_ioh, sc->sc_dma_ios);
	if (sc->sc_ba5_ss != 0)
		bus_space_unmap(sc->sc_ba5_st, sc->sc_ba5_sh, sc->sc_ba5_ss);
#endif

	return 0;
}

int
pciide_detach(device_t self, int flags)
{
	struct pciide_softc *sc = device_private(self);
	struct pciide_channel *cp;
	int channel;
#ifndef __HAVE_PCIIDE_MACHDEP_COMPAT_INTR_DISESTABLISH
	bool has_compat_chan;

	has_compat_chan = false;
	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
	     channel++) {
		cp = &sc->pciide_channels[channel];
		if (cp->compat != 0) {
			has_compat_chan = true;
		}
	}

	if (has_compat_chan != false)
		return EBUSY;
#endif

	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
	     channel++) {
		cp = &sc->pciide_channels[channel];
		if (cp->compat != 0)
			if (cp->ih != NULL) {
			       pciide_unmap_compat_intr(sc->sc_pc, cp, channel);
			       cp->ih = NULL;
			}
	}

	if (sc->sc_pci_ih != NULL) {
		pci_intr_disestablish(sc->sc_pc, sc->sc_pci_ih);
		sc->sc_pci_ih = NULL;
	}

	return pciide_common_detach(sc, flags);
}

/* tell whether the chip is enabled or not */
int
pciide_chipen(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	pcireg_t csr;

	if ((pa->pa_flags & PCI_FLAGS_IO_OKAY) == 0) {
		aprint_normal_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "I/O access disabled at bridge\n");
		return 0;
	}
	csr = pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_COMMAND_STATUS_REG);
	if ((csr & PCI_COMMAND_IO_ENABLE) == 0) {
		aprint_normal_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "I/O access disabled at device\n");
		return 0;
	}
	return 1;
}

void
pciide_mapregs_compat(const struct pci_attach_args *pa, struct pciide_channel *cp, int compatchan)
{
	struct pciide_softc *sc = CHAN_TO_PCIIDE(&cp->ata_channel);
	struct ata_channel *wdc_cp = &cp->ata_channel;
	struct wdc_regs *wdr = CHAN_TO_WDC_REGS(wdc_cp);
	int i;

	cp->compat = 1;

	wdr->cmd_iot = pa->pa_iot;
	if (bus_space_map(wdr->cmd_iot, PCIIDE_COMPAT_CMD_BASE(compatchan),
	    PCIIDE_COMPAT_CMD_SIZE, 0, &wdr->cmd_baseioh) != 0) {
		aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "couldn't map %s channel cmd regs\n", cp->name);
		goto bad;
	}
	wdr->cmd_ios = PCIIDE_COMPAT_CMD_SIZE;

	wdr->ctl_iot = pa->pa_iot;
	if (bus_space_map(wdr->ctl_iot, PCIIDE_COMPAT_CTL_BASE(compatchan),
	    PCIIDE_COMPAT_CTL_SIZE, 0, &wdr->ctl_ioh) != 0) {
		aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "couldn't map %s channel ctl regs\n", cp->name);
		bus_space_unmap(wdr->cmd_iot, wdr->cmd_baseioh, wdr->cmd_ios);
		goto bad;
	}
	wdr->ctl_ios = PCIIDE_COMPAT_CTL_SIZE;

	for (i = 0; i < WDC_NREG; i++) {
		if (bus_space_subregion(wdr->cmd_iot, wdr->cmd_baseioh, i,
		    i == 0 ? 4 : 1, &wdr->cmd_iohs[i]) != 0) {
			aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
			    "couldn't subregion %s channel cmd regs\n",
			    cp->name);
			goto bad;
		}
	}
	wdc_init_shadow_regs(wdc_cp);
	wdr->data32iot = wdr->cmd_iot;
	wdr->data32ioh = wdr->cmd_iohs[0];
	return;

bad:
	cp->ata_channel.ch_flags |= ATACH_DISABLED;
	return;
}

void
pciide_mapregs_native(const struct pci_attach_args *pa,
	struct pciide_channel *cp, int (*pci_intr)(void *))
{
	struct pciide_softc *sc = CHAN_TO_PCIIDE(&cp->ata_channel);
	struct ata_channel *wdc_cp = &cp->ata_channel;
	struct wdc_regs *wdr = CHAN_TO_WDC_REGS(wdc_cp);
	const char *intrstr;
	pci_intr_handle_t intrhandle;
	int i;
	char intrbuf[PCI_INTRSTR_LEN];

	cp->compat = 0;

	if (sc->sc_pci_ih == NULL) {
		if (pci_intr_map(pa, &intrhandle) != 0) {
			aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
			    "couldn't map native-PCI interrupt\n");
			goto bad;
		}
		intrstr = pci_intr_string(pa->pa_pc, intrhandle, intrbuf, sizeof(intrbuf));
		sc->sc_pci_ih = pci_intr_establish(pa->pa_pc,
		    intrhandle, IPL_BIO, pci_intr, sc);
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
			goto bad;
		}
	}
	cp->ih = sc->sc_pci_ih;
	if (pci_mapreg_map(pa, PCIIDE_REG_CMD_BASE(wdc_cp->ch_channel),
	    PCI_MAPREG_TYPE_IO, 0,
	    &wdr->cmd_iot, &wdr->cmd_baseioh, NULL, &wdr->cmd_ios) != 0) {
		aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "couldn't map %s channel cmd regs\n", cp->name);
		goto bad;
	}

	if (pci_mapreg_map(pa, PCIIDE_REG_CTL_BASE(wdc_cp->ch_channel),
	    PCI_MAPREG_TYPE_IO, 0,
	    &wdr->ctl_iot, &cp->ctl_baseioh, NULL, &cp->ctl_ios) != 0) {
		aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "couldn't map %s channel ctl regs\n", cp->name);
		bus_space_unmap(wdr->cmd_iot, wdr->cmd_baseioh, wdr->cmd_ios);
		goto bad;
	}
	/*
	 * In native mode, 4 bytes of I/O space are mapped for the control
	 * register, the control register is at offset 2. Pass the generic
	 * code a handle for only one byte at the right offset.
	 */
	if (bus_space_subregion(wdr->ctl_iot, cp->ctl_baseioh, 2, 1,
	    &wdr->ctl_ioh) != 0) {
		aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "unable to subregion %s channel ctl regs\n", cp->name);
		bus_space_unmap(wdr->cmd_iot, wdr->cmd_baseioh, wdr->cmd_ios);
		bus_space_unmap(wdr->cmd_iot, cp->ctl_baseioh, cp->ctl_ios);
		goto bad;
	}

	for (i = 0; i < WDC_NREG; i++) {
		if (bus_space_subregion(wdr->cmd_iot, wdr->cmd_baseioh, i,
		    i == 0 ? 4 : 1, &wdr->cmd_iohs[i]) != 0) {
			aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
			    "couldn't subregion %s channel cmd regs\n",
			    cp->name);
			goto bad;
		}
	}
	wdc_init_shadow_regs(wdc_cp);
	wdr->data32iot = wdr->cmd_iot;
	wdr->data32ioh = wdr->cmd_iohs[0];
	return;

bad:
	cp->ata_channel.ch_flags |= ATACH_DISABLED;
	return;
}

#if NATA_DMA
void
pciide_mapreg_dma(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	pcireg_t maptype;
	bus_addr_t addr;
	struct pciide_channel *pc;
	int reg, chan;
	bus_size_t size;

	/*
	 * Map DMA registers
	 *
	 * Note that sc_dma_ok is the right variable to test to see if
	 * DMA can be done.  If the interface doesn't support DMA,
	 * sc_dma_ok will never be non-zero.  If the DMA regs couldn't
	 * be mapped, it'll be zero.  I.e., sc_dma_ok will only be
	 * non-zero if the interface supports DMA and the registers
	 * could be mapped.
	 *
	 * XXX Note that despite the fact that the Bus Master IDE specs
	 * XXX say that "The bus master IDE function uses 16 bytes of IO
	 * XXX space," some controllers (at least the United
	 * XXX Microelectronics UM8886BF) place it in memory space.
	 */
	maptype = pci_mapreg_type(pa->pa_pc, pa->pa_tag,
	    PCIIDE_REG_BUS_MASTER_DMA);

	switch (maptype) {
	case PCI_MAPREG_TYPE_IO:
		sc->sc_dma_ok = (pci_mapreg_info(pa->pa_pc, pa->pa_tag,
		    PCIIDE_REG_BUS_MASTER_DMA, PCI_MAPREG_TYPE_IO,
		    &addr, NULL, NULL) == 0);
		if (sc->sc_dma_ok == 0) {
			aprint_verbose(
			    ", but unused (couldn't query registers)");
			break;
		}
		if ((sc->sc_pp->ide_flags & IDE_16BIT_IOSPACE)
		    && addr >= 0x10000) {
			sc->sc_dma_ok = 0;
			aprint_verbose(
			    ", but unused (registers at unsafe address "
			    "%#lx)", (unsigned long)addr);
			break;
		}
		/* FALLTHROUGH */

	case PCI_MAPREG_MEM_TYPE_32BIT:
		sc->sc_dma_ok = (pci_mapreg_map(pa,
		    PCIIDE_REG_BUS_MASTER_DMA, maptype, 0,
		    &sc->sc_dma_iot, &sc->sc_dma_ioh, NULL, &sc->sc_dma_ios)
		    == 0);
		sc->sc_dmat = pa->pa_dmat;
		if (sc->sc_dma_ok == 0) {
			aprint_verbose(", but unused (couldn't map registers)");
		} else {
			sc->sc_wdcdev.dma_arg = sc;
			sc->sc_wdcdev.dma_init = pciide_dma_init;
			sc->sc_wdcdev.dma_start = pciide_dma_start;
			sc->sc_wdcdev.dma_finish = pciide_dma_finish;
		}

		if (device_cfdata(sc->sc_wdcdev.sc_atac.atac_dev)->cf_flags &
		    PCIIDE_OPTIONS_NODMA) {
			aprint_verbose(
			    ", but unused (forced off by config file)");
			sc->sc_dma_ok = 0;
		}
		break;

	default:
		sc->sc_dma_ok = 0;
		aprint_verbose(
		    ", but unsupported register maptype (0x%x)", maptype);
	}

	if (sc->sc_dma_ok == 0)
		return;

	/*
	 * Set up the default handles for the DMA registers.
	 * Just reserve 32 bits for each handle, unless space
	 * doesn't permit it.
	 */
	for (chan = 0; chan < PCIIDE_NUM_CHANNELS; chan++) {
		pc = &sc->pciide_channels[chan];
		for (reg = 0; reg < IDEDMA_NREGS; reg++) {
			size = 4;
			if (size > (IDEDMA_SCH_OFFSET - reg))
				size = IDEDMA_SCH_OFFSET - reg;
			if (bus_space_subregion(sc->sc_dma_iot, sc->sc_dma_ioh,
			    IDEDMA_SCH_OFFSET * chan + reg, size,
			    &pc->dma_iohs[reg]) != 0) {
				sc->sc_dma_ok = 0;
				aprint_verbose(", but can't subregion offset %d "
					      "size %lu", reg, (u_long)size);
				return;
			}
		}
	}
}
#endif	/* NATA_DMA */

int
pciide_compat_intr(void *arg)
{
	struct pciide_channel *cp = arg;

#ifdef DIAGNOSTIC
	/* should only be called for a compat channel */
	if (cp->compat == 0)
		panic("pciide compat intr called for non-compat chan %p", cp);
#endif
	return (wdcintr(&cp->ata_channel));
}

int
pciide_pci_intr(void *arg)
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct ata_channel *wdc_cp;
	int i, rv, crv;

	rv = 0;
	for (i = 0; i < sc->sc_wdcdev.sc_atac.atac_nchannels; i++) {
		cp = &sc->pciide_channels[i];
		wdc_cp = &cp->ata_channel;

		/* If a compat channel skip. */
		if (cp->compat)
			continue;
		/* if this channel not waiting for intr, skip */
		if ((wdc_cp->ch_flags & ATACH_IRQ_WAIT) == 0)
			continue;

		crv = wdcintr(wdc_cp);
		if (crv == 0)
			;		/* leave rv alone */
		else if (crv == 1)
			rv = 1;		/* claim the intr */
		else if (rv == 0)	/* crv should be -1 in this case */
			rv = crv;	/* if we've done no better, take it */
	}
	return (rv);
}

#if NATA_DMA
void
pciide_channel_dma_setup(struct pciide_channel *cp)
{
	int drive, s;
	struct pciide_softc *sc = CHAN_TO_PCIIDE(&cp->ata_channel);
	struct ata_drive_datas *drvp;

	KASSERT(cp->ata_channel.ch_ndrives != 0);

	for (drive = 0; drive < cp->ata_channel.ch_ndrives; drive++) {
		drvp = &cp->ata_channel.ch_drive[drive];
		/* If no drive, skip */
		if (drvp->drive_type == ATA_DRIVET_NONE)
			continue;
		/* setup DMA if needed */
		if (((drvp->drive_flags & ATA_DRIVE_DMA) == 0 &&
		    (drvp->drive_flags & ATA_DRIVE_UDMA) == 0) ||
		    sc->sc_dma_ok == 0) {
			s = splbio();
			drvp->drive_flags &= ~(ATA_DRIVE_DMA | ATA_DRIVE_UDMA);
			splx(s);
			continue;
		}
		if (pciide_dma_table_setup(sc, cp->ata_channel.ch_channel,
					   drive) != 0) {
			/* Abort DMA setup */
			s = splbio();
			drvp->drive_flags &= ~(ATA_DRIVE_DMA | ATA_DRIVE_UDMA);
			splx(s);
			continue;
		}
	}
}

#define NIDEDMA_TABLES(sc)	\
	(MAXPHYS/(min((sc)->sc_dma_maxsegsz, PAGE_SIZE)) + 1)

int
pciide_dma_table_setup(struct pciide_softc *sc, int channel, int drive)
{
	int error;
	const bus_size_t dma_table_size =
	    sizeof(struct idedma_table) * NIDEDMA_TABLES(sc);
	struct pciide_dma_maps *dma_maps =
	    &sc->pciide_channels[channel].dma_maps[drive];

	/* If table was already allocated, just return */
	if (dma_maps->dma_table)
		return 0;

	/* Allocate memory for the DMA tables and map it */
	if ((error = bus_dmamem_alloc(sc->sc_dmat, dma_table_size,
	    IDEDMA_TBL_ALIGN, IDEDMA_TBL_ALIGN, &dma_maps->dmamap_table_seg,
	    1, &dma_maps->dmamap_table_nseg, BUS_DMA_NOWAIT)) != 0) {
		aprint_error(dmaerrfmt,
		    device_xname(sc->sc_wdcdev.sc_atac.atac_dev), channel,
		    "allocate", drive, error);
		return error;
	}
	if ((error = bus_dmamem_map(sc->sc_dmat, &dma_maps->dmamap_table_seg,
	    dma_maps->dmamap_table_nseg, dma_table_size,
	    (void **)&dma_maps->dma_table,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		aprint_error(dmaerrfmt,
		    device_xname(sc->sc_wdcdev.sc_atac.atac_dev), channel,
		    "map", drive, error);
		return error;
	}
	ATADEBUG_PRINT(("pciide_dma_table_setup: table at %p len %lu, "
	    "phy 0x%lx\n", dma_maps->dma_table, (u_long)dma_table_size,
	    (unsigned long)dma_maps->dmamap_table_seg.ds_addr), DEBUG_PROBE);
	/* Create and load table DMA map for this disk */
	if ((error = bus_dmamap_create(sc->sc_dmat, dma_table_size,
	    1, dma_table_size, IDEDMA_TBL_ALIGN, BUS_DMA_NOWAIT,
	    &dma_maps->dmamap_table)) != 0) {
		aprint_error(dmaerrfmt,
		    device_xname(sc->sc_wdcdev.sc_atac.atac_dev), channel,
		    "create", drive, error);
		return error;
	}
	if ((error = bus_dmamap_load(sc->sc_dmat,
	    dma_maps->dmamap_table,
	    dma_maps->dma_table,
	    dma_table_size, NULL, BUS_DMA_NOWAIT)) != 0) {
		aprint_error(dmaerrfmt,
		    device_xname(sc->sc_wdcdev.sc_atac.atac_dev), channel,
		    "load", drive, error);
		return error;
	}
	ATADEBUG_PRINT(("pciide_dma_table_setup: phy addr of table 0x%lx\n",
	    (unsigned long)dma_maps->dmamap_table->dm_segs[0].ds_addr),
	    DEBUG_PROBE);
	/* Create a xfer DMA map for this drive */
	if ((error = bus_dmamap_create(sc->sc_dmat, MAXPHYS,
	    NIDEDMA_TABLES(sc), sc->sc_dma_maxsegsz, sc->sc_dma_boundary,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
	    &dma_maps->dmamap_xfer)) != 0) {
		aprint_error(dmaerrfmt,
		    device_xname(sc->sc_wdcdev.sc_atac.atac_dev), channel,
		    "create xfer", drive, error);
		return error;
	}
	return 0;
}

void
pciide_dma_table_teardown(struct pciide_softc *sc, int channel, int drive)
{
	struct pciide_channel *cp;
	struct pciide_dma_maps *dma_maps;

	cp = &sc->pciide_channels[channel];
	dma_maps = &cp->dma_maps[drive];

	if (dma_maps->dma_table == NULL)
		return;

	bus_dmamap_destroy(sc->sc_dmat, dma_maps->dmamap_xfer);
	bus_dmamap_unload(sc->sc_dmat, dma_maps->dmamap_table);
	bus_dmamap_destroy(sc->sc_dmat, dma_maps->dmamap_table);
	bus_dmamem_unmap(sc->sc_dmat, dma_maps->dma_table,
	    sizeof(struct idedma_table) * NIDEDMA_TABLES(sc));
	bus_dmamem_free(sc->sc_dmat, &dma_maps->dmamap_table_seg,
	    dma_maps->dmamap_table_nseg);

	dma_maps->dma_table = NULL;

	return;
}

int
pciide_dma_dmamap_setup(struct pciide_softc *sc, int channel, int drive, void *databuf, size_t datalen, int flags)
{
	int error, seg;
	struct pciide_channel *cp = &sc->pciide_channels[channel];
	struct pciide_dma_maps *dma_maps = &cp->dma_maps[drive];

	error = bus_dmamap_load(sc->sc_dmat,
	    dma_maps->dmamap_xfer,
	    databuf, datalen, NULL, BUS_DMA_NOWAIT | BUS_DMA_STREAMING |
	    ((flags & WDC_DMA_READ) ? BUS_DMA_READ : BUS_DMA_WRITE));
	if (error) {
		aprint_error(dmaerrfmt,
		    device_xname(sc->sc_wdcdev.sc_atac.atac_dev), channel,
		    "load xfer", drive, error);
		return error;
	}

	bus_dmamap_sync(sc->sc_dmat, dma_maps->dmamap_xfer, 0,
	    dma_maps->dmamap_xfer->dm_mapsize,
	    (flags & WDC_DMA_READ) ?
	    BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);

	for (seg = 0; seg < dma_maps->dmamap_xfer->dm_nsegs; seg++) {
#ifdef DIAGNOSTIC
		/* A segment must not cross a 64k boundary */
		{
		u_long phys = dma_maps->dmamap_xfer->dm_segs[seg].ds_addr;
		u_long len = dma_maps->dmamap_xfer->dm_segs[seg].ds_len;
		if ((phys & ~IDEDMA_BYTE_COUNT_MASK) !=
		    ((phys + len - 1) & ~IDEDMA_BYTE_COUNT_MASK)) {
			printf("pciide_dma: segment %d physical addr 0x%lx"
			    " len 0x%lx not properly aligned\n",
			    seg, phys, len);
			panic("pciide_dma: buf align");
		}
		}
#endif
		dma_maps->dma_table[seg].base_addr =
		    htole32(dma_maps->dmamap_xfer->dm_segs[seg].ds_addr);
		dma_maps->dma_table[seg].byte_count =
		    htole32(dma_maps->dmamap_xfer->dm_segs[seg].ds_len &
		    IDEDMA_BYTE_COUNT_MASK);
		ATADEBUG_PRINT(("\t seg %d len %d addr 0x%x\n",
		   seg, le32toh(dma_maps->dma_table[seg].byte_count),
		   le32toh(dma_maps->dma_table[seg].base_addr)), DEBUG_DMA);

	}
	dma_maps->dma_table[dma_maps->dmamap_xfer->dm_nsegs -1].byte_count |=
	    htole32(IDEDMA_BYTE_COUNT_EOT);

	bus_dmamap_sync(sc->sc_dmat, dma_maps->dmamap_table, 0,
	    dma_maps->dmamap_table->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

#ifdef DIAGNOSTIC
	if (dma_maps->dmamap_table->dm_segs[0].ds_addr & ~IDEDMA_TBL_MASK) {
		printf("pciide_dma_dmamap_setup: addr 0x%lx "
		    "not properly aligned\n",
		    (u_long)dma_maps->dmamap_table->dm_segs[0].ds_addr);
		panic("pciide_dma_init: table align");
	}
#endif
	/* remember flags */
	dma_maps->dma_flags = flags;

	return 0;
}

int
pciide_dma_init(void *v, int channel, int drive, void *databuf, size_t datalen, int flags)
{
	struct pciide_softc *sc = v;
	int error;
	struct pciide_channel *cp = &sc->pciide_channels[channel];
	struct pciide_dma_maps *dma_maps = &cp->dma_maps[drive];

	if ((error = pciide_dma_dmamap_setup(sc, channel, drive,
	    databuf, datalen, flags)) != 0)
		return error;
	/* Maps are ready. Start DMA function */
	/* Clear status bits */
	bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL], 0,
	    bus_space_read_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL], 0));
	/* Write table addr */
	bus_space_write_4(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_TBL], 0,
	    dma_maps->dmamap_table->dm_segs[0].ds_addr);
	/* set read/write */
	bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CMD], 0,
	    ((flags & WDC_DMA_READ) ? IDEDMA_CMD_WRITE : 0) | cp->idedma_cmd);
	return 0;
}

void
pciide_dma_start(void *v, int channel, int drive)
{
	struct pciide_softc *sc = v;
	struct pciide_channel *cp = &sc->pciide_channels[channel];

	ATADEBUG_PRINT(("pciide_dma_start\n"),DEBUG_XFERS);
	bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CMD], 0,
	    bus_space_read_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CMD], 0)
		| IDEDMA_CMD_START);
}

int
pciide_dma_finish(void *v, int channel, int drive, int force)
{
	struct pciide_softc *sc = v;
	u_int8_t status;
	int error = 0;
	struct pciide_channel *cp = &sc->pciide_channels[channel];
	struct pciide_dma_maps *dma_maps = &cp->dma_maps[drive];

	status = bus_space_read_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL], 0);
	ATADEBUG_PRINT(("pciide_dma_finish: status 0x%x\n", status),
	    DEBUG_XFERS);

	if (force == WDC_DMAEND_END && (status & IDEDMA_CTL_INTR) == 0)
		return WDC_DMAST_NOIRQ;

	/* stop DMA channel */
	bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CMD], 0,
	    bus_space_read_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CMD], 0)
		& ~IDEDMA_CMD_START);

	/* Unload the map of the data buffer */
	bus_dmamap_sync(sc->sc_dmat, dma_maps->dmamap_xfer, 0,
	    dma_maps->dmamap_xfer->dm_mapsize,
	    (dma_maps->dma_flags & WDC_DMA_READ) ?
	    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, dma_maps->dmamap_xfer);

	if ((status & IDEDMA_CTL_ERR) != 0 && force != WDC_DMAEND_ABRT_QUIET) {
		aprint_error("%s:%d:%d: bus-master DMA error: status=0x%x\n",
		    device_xname(sc->sc_wdcdev.sc_atac.atac_dev), channel,
		    drive, status);
		error |= WDC_DMAST_ERR;
	}

	if ((status & IDEDMA_CTL_INTR) == 0 && force != WDC_DMAEND_ABRT_QUIET) {
		aprint_error("%s:%d:%d: bus-master DMA error: missing "
		    "interrupt, status=0x%x\n", 
		    device_xname(sc->sc_wdcdev.sc_atac.atac_dev),
		    channel, drive, status);
		error |= WDC_DMAST_NOIRQ;
	}

	if ((status & IDEDMA_CTL_ACT) != 0 && force != WDC_DMAEND_ABRT_QUIET) {
		/* data underrun, may be a valid condition for ATAPI */
		error |= WDC_DMAST_UNDER;
	}
	return error;
}

void
pciide_irqack(struct ata_channel *chp)
{
	struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);

	/* clear status bits in IDE DMA registers */
	bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL], 0,
	    bus_space_read_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL], 0));
}
#endif	/* NATA_DMA */

/* some common code used by several chip_map */
int
pciide_chansetup(struct pciide_softc *sc, int channel, pcireg_t interface)
{
	struct pciide_channel *cp = &sc->pciide_channels[channel];
	sc->wdc_chanarray[channel] = &cp->ata_channel;
	cp->name = PCIIDE_CHANNEL_NAME(channel);
	cp->ata_channel.ch_channel = channel;
	cp->ata_channel.ch_atac = &sc->sc_wdcdev.sc_atac;
	cp->ata_channel.ch_queue =
	    malloc(sizeof(struct ata_queue), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (cp->ata_channel.ch_queue == NULL) {
		aprint_error("%s %s channel: "
		    "can't allocate memory for command queue",
		device_xname(sc->sc_wdcdev.sc_atac.atac_dev), cp->name);
		return 0;
	}
	aprint_verbose_dev(sc->sc_wdcdev.sc_atac.atac_dev,
	    "%s channel %s to %s mode\n", cp->name,
	    (interface & PCIIDE_INTERFACE_SETTABLE(channel)) ?
	    "configured" : "wired",
	    (interface & PCIIDE_INTERFACE_PCI(channel)) ?
	    "native-PCI" : "compatibility");
	return 1;
}

/* some common code used by several chip channel_map */
void
pciide_mapchan(const struct pci_attach_args *pa,
	struct pciide_channel *cp,
	pcireg_t interface, int (*pci_intr)(void *))
{
	struct ata_channel *wdc_cp = &cp->ata_channel;

	if (interface & PCIIDE_INTERFACE_PCI(wdc_cp->ch_channel))
		pciide_mapregs_native(pa, cp, pci_intr);
	else {
		pciide_mapregs_compat(pa, cp, wdc_cp->ch_channel);
		if ((cp->ata_channel.ch_flags & ATACH_DISABLED) == 0)
			pciide_map_compat_intr(pa, cp, wdc_cp->ch_channel);
	}
	wdcattach(wdc_cp);
}

/*
 * generic code to map the compat intr.
 */
void
pciide_map_compat_intr(const struct pci_attach_args *pa, struct pciide_channel *cp, int compatchan)
{
	struct pciide_softc *sc = CHAN_TO_PCIIDE(&cp->ata_channel);

#ifdef __HAVE_PCIIDE_MACHDEP_COMPAT_INTR_ESTABLISH
	cp->ih =
	   pciide_machdep_compat_intr_establish(sc->sc_wdcdev.sc_atac.atac_dev,
	   pa, compatchan, pciide_compat_intr, cp);
	if (cp->ih == NULL) {
#endif
		aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "no compatibility interrupt for use by %s "
		    "channel\n", cp->name);
		cp->ata_channel.ch_flags |= ATACH_DISABLED;
#ifdef __HAVE_PCIIDE_MACHDEP_COMPAT_INTR_ESTABLISH
	}
#endif
}

void
pciide_unmap_compat_intr(pci_chipset_tag_t pc, struct pciide_channel *cp, int compatchan)
{
#ifdef __HAVE_PCIIDE_MACHDEP_COMPAT_INTR_DISESTABLISH
	struct pciide_softc *sc = CHAN_TO_PCIIDE(&cp->ata_channel);

	pciide_machdep_compat_intr_disestablish(sc->sc_wdcdev.sc_atac.atac_dev,
	    sc->sc_pc, compatchan, cp->ih);
#endif
}

void
default_chip_map(struct pciide_softc *sc, const struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	pcireg_t interface = PCI_INTERFACE(pa->pa_class);
	pcireg_t csr;
	int channel;
#if NATA_DMA
	int drive;
	u_int8_t idedma_ctl;
#endif
	const char *failreason;
	struct wdc_regs *wdr;

	if (pciide_chipen(sc, pa) == 0)
		return;

	if (interface & PCIIDE_INTERFACE_BUS_MASTER_DMA) {
#if NATA_DMA
		aprint_verbose_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "bus-master DMA support present");
		if (sc->sc_pp == &default_product_desc &&
		    (device_cfdata(sc->sc_wdcdev.sc_atac.atac_dev)->cf_flags &
		    PCIIDE_OPTIONS_DMA) == 0) {
			aprint_verbose(", but unused (no driver support)");
			sc->sc_dma_ok = 0;
		} else {
			pciide_mapreg_dma(sc, pa);
			if (sc->sc_dma_ok != 0)
				aprint_verbose(", used without full driver "
				    "support");
		}
#else
		aprint_verbose_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "bus-master DMA support present, but unused (no driver "
		    "support)");
#endif	/* NATA_DMA */
	} else {
		aprint_verbose_dev(sc->sc_wdcdev.sc_atac.atac_dev,
		    "hardware does not support DMA");
#if NATA_DMA
		sc->sc_dma_ok = 0;
#endif
	}
	aprint_verbose("\n");
#if NATA_DMA
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DMA;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
#endif
	sc->sc_wdcdev.sc_atac.atac_pio_cap = 0;
#if NATA_DMA
	sc->sc_wdcdev.sc_atac.atac_dma_cap = 0;
#endif

	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = PCIIDE_NUM_CHANNELS;
	sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DATA16;
	sc->sc_wdcdev.wdc_maxdrives = 2;

	wdc_allocate_regs(&sc->sc_wdcdev);

	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
	     channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		wdr = CHAN_TO_WDC_REGS(&cp->ata_channel);
		if (interface & PCIIDE_INTERFACE_PCI(channel))
			pciide_mapregs_native(pa, cp, pciide_pci_intr);
		else
			pciide_mapregs_compat(pa, cp,
			    cp->ata_channel.ch_channel);
		if (cp->ata_channel.ch_flags & ATACH_DISABLED)
			continue;
		/*
		 * Check to see if something appears to be there.
		 */
		failreason = NULL;
		/*
		 * In native mode, always enable the controller. It's
		 * not possible to have an ISA board using the same address
		 * anyway.
		 */
		if (interface & PCIIDE_INTERFACE_PCI(channel)) {
			wdcattach(&cp->ata_channel);
			continue;
		}
		if (!wdcprobe(&cp->ata_channel)) {
			failreason = "not responding; disabled or no drives?";
			goto next;
		}
		/*
		 * Now, make sure it's actually attributable to this PCI IDE
		 * channel by trying to access the channel again while the
		 * PCI IDE controller's I/O space is disabled.  (If the
		 * channel no longer appears to be there, it belongs to
		 * this controller.)  YUCK!
		 */
		csr = pci_conf_read(sc->sc_pc, sc->sc_tag,
		    PCI_COMMAND_STATUS_REG);
		pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_COMMAND_STATUS_REG,
		    csr & ~PCI_COMMAND_IO_ENABLE);
		if (wdcprobe(&cp->ata_channel))
			failreason = "other hardware responding at addresses";
		pci_conf_write(sc->sc_pc, sc->sc_tag,
		    PCI_COMMAND_STATUS_REG, csr);
next:
		if (failreason) {
			aprint_error_dev(sc->sc_wdcdev.sc_atac.atac_dev,
			    "%s channel ignored (%s)\n", cp->name, failreason);
			cp->ata_channel.ch_flags |= ATACH_DISABLED;
			bus_space_unmap(wdr->cmd_iot, wdr->cmd_baseioh,
			    wdr->cmd_ios);
			bus_space_unmap(wdr->ctl_iot, wdr->ctl_ioh,
			    wdr->ctl_ios);
		} else {
			pciide_map_compat_intr(pa, cp,
			    cp->ata_channel.ch_channel);
			wdcattach(&cp->ata_channel);
		}
	}

#if NATA_DMA
	if (sc->sc_dma_ok == 0)
		return;

	/* Allocate DMA maps */
	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
	     channel++) {
		idedma_ctl = 0;
		cp = &sc->pciide_channels[channel];
		for (drive = 0; drive < sc->sc_wdcdev.wdc_maxdrives; drive++) {
			/*
			 * we have not probed the drives yet, allocate
			 * ressources for all of them.
			 */
			if (pciide_dma_table_setup(sc, channel, drive) != 0) {
				/* Abort DMA setup */
				aprint_error(
				    "%s:%d:%d: can't allocate DMA maps, "
				    "using PIO transfers\n",
				    device_xname(
				      sc->sc_wdcdev.sc_atac.atac_dev),
				    channel, drive);
				sc->sc_dma_ok = 0;
				sc->sc_wdcdev.sc_atac.atac_cap &= ~ATAC_CAP_DMA;
				sc->sc_wdcdev.irqack = NULL;
				break;
			}
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		}
		if (idedma_ctl != 0) {
			/* Add software bits in status register */
			bus_space_write_1(sc->sc_dma_iot,
			    cp->dma_iohs[IDEDMA_CTL], 0, idedma_ctl);
		}
	}
#endif	/* NATA_DMA */
}

void
sata_setup_channel(struct ata_channel *chp)
{
#if NATA_DMA
	struct ata_drive_datas *drvp;
	int drive;
#if NATA_UDMA
	int s;
#endif
	u_int32_t idedma_ctl;
	struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	idedma_ctl = 0;

	KASSERT(cp->ata_channel.ch_ndrives != 0);
	for (drive = 0; drive < cp->ata_channel.ch_ndrives; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if (drvp->drive_type == ATA_DRIVET_NONE)
			continue;
#if NATA_UDMA
		if (drvp->drive_flags & ATA_DRIVE_UDMA) {
			/* use Ultra/DMA */
			s = splbio();
			drvp->drive_flags &= ~ATA_DRIVE_DMA;
			splx(s);
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else
#endif	/* NATA_UDMA */
		if (drvp->drive_flags & ATA_DRIVE_DMA) {
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		}
	}

	/*
	 * Nothing to do to setup modes; it is meaningless in S-ATA
	 * (but many S-ATA drives still want to get the SET_FEATURE
	 * command).
	 */
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL], 0,
		    idedma_ctl);
	}
#endif	/* NATA_DMA */
}

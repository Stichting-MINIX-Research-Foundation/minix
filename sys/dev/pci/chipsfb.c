/*	$NetBSD: chipsfb.c,v 1.31 2012/01/30 19:41:18 drochner Exp $	*/

/*
 * Copyright (c) 2006 Michael Lorenz
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
 * A console driver for Chips & Technologies 65550 graphics controllers
 * tested on macppc only so far
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: chipsfb.c,v 1.31 2012/01/30 19:41:18 drochner Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/kauth.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciio.h>
#include <dev/pci/wsdisplay_pci.h>


#include <dev/ic/ct65550reg.h>
#include <dev/ic/ct65550var.h>

#include "opt_wsemul.h"
#include "opt_chipsfb.h"

struct chipsfb_pci_softc {
	struct chipsfb_softc sc_chips;
	pci_chipset_tag_t sc_pc;
	pcitag_t sc_pcitag;
};

static int	chipsfb_pci_match(device_t, cfdata_t, void *);
static void	chipsfb_pci_attach(device_t, device_t, void *);
static int	chipsfb_pci_ioctl(void *, void *, u_long, void *, int,
		    struct lwp *);

CFATTACH_DECL_NEW(chipsfb_pci, sizeof(struct chipsfb_pci_softc),
    chipsfb_pci_match, chipsfb_pci_attach, NULL, NULL);

static int
chipsfb_pci_match(device_t parent, cfdata_t match, void *aux)
{
	const struct pci_attach_args *pa = (const struct pci_attach_args *)aux;

	if (PCI_CLASS(pa->pa_class) != PCI_CLASS_DISPLAY ||
	    PCI_SUBCLASS(pa->pa_class) != PCI_SUBCLASS_DISPLAY_VGA)
		return 0;
	if ((PCI_VENDOR(pa->pa_id) == PCI_VENDOR_CHIPS) &&
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_CHIPS_65550))
		return 100;
	if ((PCI_VENDOR(pa->pa_id) == PCI_VENDOR_CHIPS) &&
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_CHIPS_65554))
		return 100;
	return 0;
}

static void
chipsfb_pci_attach(device_t parent, device_t self, void *aux)
{
	struct chipsfb_pci_softc *scp = device_private(self);
	struct chipsfb_softc *sc = &scp->sc_chips;
	const struct pci_attach_args *pa = aux;
	pcireg_t screg;

	scp->sc_pc = pa->pa_pc;
	scp->sc_pcitag = pa->pa_tag;
	sc->sc_dev = self;

	screg = pci_conf_read(scp->sc_pc, scp->sc_pcitag,
	    PCI_COMMAND_STATUS_REG);
	screg |= PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE;
	pci_conf_write(scp->sc_pc, scp->sc_pcitag, PCI_COMMAND_STATUS_REG, 
	    screg);

	pci_aprint_devinfo(pa, NULL);

	sc->sc_memt = pa->pa_memt;
	sc->sc_iot = pa->pa_iot;
	sc->sc_ioctl = chipsfb_pci_ioctl;
	sc->sc_mmap = NULL;
	
	/* the framebuffer */
	sc->sc_fb = (pci_conf_read(scp->sc_pc, scp->sc_pcitag, PCI_BAR0) & 
	    ~PCI_MAPREG_MEM_TYPE_MASK);
	sc->sc_fbsize = 0x01000000;	/* 16MB aperture */

	if (bus_space_map(sc->sc_memt, sc->sc_fb, 0x400000,
	    BUS_SPACE_MAP_LINEAR, &sc->sc_fbh)) {
		aprint_error_dev(sc->sc_dev,
		    "failed to map the frame buffer.\n");
	}

	if (bus_space_map(sc->sc_memt, sc->sc_fb + CT_OFF_BITBLT, 0x20000,
	    BUS_SPACE_MAP_LINEAR, &sc->sc_mmregh)) {
		aprint_error_dev(sc->sc_dev,
		    "failed to map MMIO registers.\n");
	}

	/* IO-mapped registers */
	if (bus_space_map(sc->sc_iot, 0x0, 0x400, 0, &sc->sc_ioregh) != 0) {
		aprint_error_dev(sc->sc_dev, "failed to map IO registers.\n");
	}

	sc->memsize = chipsfb_probe_vram(sc);

	chipsfb_do_attach(sc);
}

static int
chipsfb_pci_ioctl(void *v, void *vs, u_long cmd, void *data, int flag,
	struct lwp *l)
{
	struct vcons_data *vd = v;
	struct chipsfb_softc *sc = vd->cookie;
	struct chipsfb_pci_softc *scp = vd->cookie;

	switch (cmd) {
	/* PCI config read/write passthrough. */
	case PCI_IOC_CFGREAD:
	case PCI_IOC_CFGWRITE:
		return pci_devioctl(scp->sc_pc, scp->sc_pcitag,
		    cmd, data, flag, l);

	case WSDISPLAYIO_GET_BUSID:
		return wsdisplayio_busid_pci(sc->sc_dev, scp->sc_pc,
		    scp->sc_pcitag, data);
	}
	return EPASSTHROUGH;
}

/* $NetBSD: hdaudio_pci.c,v 1.2 2015/07/05 08:14:46 nonaka Exp $ */

/*
 * Copyright (c) 2009 Precedence Technologies Ltd <support@precedence.co.uk>
 * Copyright (c) 2009 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Precedence Technologies Ltd
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Intel High Definition Audio (Revision 1.0a) device driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hdaudio_pci.c,v 1.2 2015/07/05 08:14:46 nonaka Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/intr.h>
#include <sys/module.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <dev/hdaudio/hdaudioreg.h>
#include <dev/hdaudio/hdaudiovar.h>
#include <dev/pci/hdaudio_pci.h>

struct hdaudio_pci_softc {
	struct hdaudio_softc	sc_hdaudio;	/* must be first */
	pcitag_t		sc_tag;
	pci_chipset_tag_t	sc_pc;
	void			*sc_ih;
	pcireg_t		sc_id;
};

static int		hdaudio_pci_match(device_t, cfdata_t, void *);
static void		hdaudio_pci_attach(device_t, device_t, void *);
static int		hdaudio_pci_detach(device_t, int);
static int		hdaudio_pci_rescan(device_t, const char *, const int *);
static void		hdaudio_pci_childdet(device_t, device_t);

static int		hdaudio_pci_intr(void *);
static void		hdaudio_pci_reinit(struct hdaudio_pci_softc *);

/* power management */
static bool		hdaudio_pci_resume(device_t, const pmf_qual_t *);

CFATTACH_DECL2_NEW(
    hdaudio_pci,
    sizeof(struct hdaudio_pci_softc),
    hdaudio_pci_match,
    hdaudio_pci_attach,
    hdaudio_pci_detach,
    NULL,
    hdaudio_pci_rescan,
    hdaudio_pci_childdet
);

/*
 * NetBSD autoconfiguration
 */

static int
hdaudio_pci_match(device_t parent, cfdata_t match, void *opaque)
{
	struct pci_attach_args *pa = opaque;

	if (PCI_CLASS(pa->pa_class) != PCI_CLASS_MULTIMEDIA)
		return 0;
	if (PCI_SUBCLASS(pa->pa_class) != PCI_SUBCLASS_MULTIMEDIA_HDAUDIO)
		return 0;

	return 10;
}

static void
hdaudio_pci_attach(device_t parent, device_t self, void *opaque)
{
	struct hdaudio_pci_softc *sc = device_private(self);
	struct pci_attach_args *pa = opaque;
	pci_intr_handle_t ih;
	const char *intrstr;
	pcireg_t csr;
	int err;
	char intrbuf[PCI_INTRSTR_LEN];

	aprint_naive("\n");
	aprint_normal(": HD Audio Controller\n");

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_id = pa->pa_id;

	sc->sc_hdaudio.sc_subsystem = pci_conf_read(sc->sc_pc, sc->sc_tag,
	    PCI_SUBSYS_ID_REG);

	/* Enable busmastering and MMIO access */
	csr = pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_COMMAND_STATUS_REG);
	csr |= PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_BACKTOBACK_ENABLE;
	pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_COMMAND_STATUS_REG, csr);

	/* Map MMIO registers */
	err = pci_mapreg_map(pa, HDAUDIO_PCI_AZBARL, PCI_MAPREG_TYPE_MEM, 0,
			     &sc->sc_hdaudio.sc_memt,
			     &sc->sc_hdaudio.sc_memh,
			     &sc->sc_hdaudio.sc_membase,
			     &sc->sc_hdaudio.sc_memsize);
	if (err) {
		aprint_error_dev(self, "couldn't map mmio space\n");
		return;
	}
	sc->sc_hdaudio.sc_memvalid = true;
	sc->sc_hdaudio.sc_dmat = pa->pa_dmat;

	/* Map interrupt and establish handler */
	err = pci_intr_map(pa, &ih);
	if (err) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_AUDIO,
	    hdaudio_pci_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt");
		if (intrstr)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	if (!pmf_device_register(self, NULL, hdaudio_pci_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");

	hdaudio_pci_reinit(sc);

	/* Attach bus-independent HD audio layer */
	if (hdaudio_attach(self, &sc->sc_hdaudio)) {
		pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
		sc->sc_ih = NULL;
		bus_space_unmap(sc->sc_hdaudio.sc_memt,
				sc->sc_hdaudio.sc_memh,
				sc->sc_hdaudio.sc_memsize);
		sc->sc_hdaudio.sc_memvalid = false;
		csr = pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_COMMAND_STATUS_REG);
		csr &= ~(PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_BACKTOBACK_ENABLE);
		pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_COMMAND_STATUS_REG, csr);
		pmf_device_deregister(self);
	}
}

static int
hdaudio_pci_rescan(device_t self, const char *ifattr, const int *locs)
{
	struct hdaudio_pci_softc *sc = device_private(self);

	return hdaudio_rescan(&sc->sc_hdaudio, ifattr, locs);
}

void
hdaudio_pci_childdet(device_t self, device_t child)
{
	struct hdaudio_pci_softc *sc = device_private(self);

	hdaudio_childdet(&sc->sc_hdaudio, child);
}

static int
hdaudio_pci_detach(device_t self, int flags)
{
	struct hdaudio_pci_softc *sc = device_private(self);
	pcireg_t csr;

	hdaudio_detach(&sc->sc_hdaudio, flags);

	if (sc->sc_ih != NULL) {
		pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
		sc->sc_ih = NULL;
	}
	if (sc->sc_hdaudio.sc_memvalid == true) {
		bus_space_unmap(sc->sc_hdaudio.sc_memt,
				sc->sc_hdaudio.sc_memh,
				sc->sc_hdaudio.sc_memsize);
		sc->sc_hdaudio.sc_memvalid = false;
	}

	/* Disable busmastering and MMIO access */
	csr = pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_COMMAND_STATUS_REG);
	csr &= ~(PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_BACKTOBACK_ENABLE);
	pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_COMMAND_STATUS_REG, csr);

	pmf_device_deregister(self);

	return 0;
}

static int
hdaudio_pci_intr(void *opaque)
{
	struct hdaudio_pci_softc *sc = opaque;

	return hdaudio_intr(&sc->sc_hdaudio);
}


static void
hdaudio_pci_reinit(struct hdaudio_pci_softc *sc)
{
	pcireg_t val;

	/* stops playback static */
	val = pci_conf_read(sc->sc_pc, sc->sc_tag, HDAUDIO_PCI_TCSEL);
	val &= ~7;
	val |= 0;
	pci_conf_write(sc->sc_pc, sc->sc_tag, HDAUDIO_PCI_TCSEL, val);

	switch (PCI_VENDOR(sc->sc_id)) {
	case PCI_VENDOR_NVIDIA:
		/* enable snooping */
		val = pci_conf_read(sc->sc_pc, sc->sc_tag,
		    HDAUDIO_NV_REG_SNOOP);
		val &= ~HDAUDIO_NV_SNOOP_MASK;
		val |= HDAUDIO_NV_SNOOP_ENABLE;
		pci_conf_write(sc->sc_pc, sc->sc_tag,
		    HDAUDIO_NV_REG_SNOOP, val);
		break;
	}
}

static bool
hdaudio_pci_resume(device_t self, const pmf_qual_t *qual)
{
	struct hdaudio_pci_softc *sc = device_private(self);

	hdaudio_pci_reinit(sc);
	return hdaudio_resume(&sc->sc_hdaudio);
}

MODULE(MODULE_CLASS_DRIVER, hdaudio_pci, "hdaudio");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
hdaudio_pci_modcmd(modcmd_t cmd, void *opaque)
{
	int error = 0;

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_hdaudio_pci,
		    cfattach_ioconf_hdaudio_pci, cfdata_ioconf_hdaudio_pci);
#endif
		return error;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		error = config_fini_component(cfdriver_ioconf_hdaudio_pci,
		    cfattach_ioconf_hdaudio_pci, cfdata_ioconf_hdaudio_pci);
#endif
		return error;
	default:
		return ENOTTY;
	}
}

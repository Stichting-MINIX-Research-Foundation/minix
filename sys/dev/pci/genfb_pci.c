/*	$NetBSD: genfb_pci.c,v 1.37 2014/07/24 21:35:13 riastradh Exp $ */

/*-
 * Copyright (c) 2007 Michael Lorenz
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: genfb_pci.c,v 1.37 2014/07/24 21:35:13 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/kauth.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pciio.h>

#include <dev/wsfb/genfbvar.h>
#include <dev/pci/wsdisplay_pci.h>

#include <dev/pci/genfb_pcivar.h>

#include "opt_wsfb.h"
#include "opt_genfb.h"

#ifdef GENFB_PCI_DEBUG
# define DPRINTF printf
#else
# define DPRINTF while (0) printf
#endif

static int	pci_genfb_match(device_t, cfdata_t, void *);
static void	pci_genfb_attach(device_t, device_t, void *);
static int	pci_genfb_ioctl(void *, void *, u_long, void *, int,
		    struct lwp *);
static paddr_t	pci_genfb_mmap(void *, void *, off_t, int);
static int	pci_genfb_borrow(void *, bus_addr_t, bus_space_handle_t *);
static int	pci_genfb_drm_print(void *, const char *);
static bool	pci_genfb_shutdown(device_t, int);

CFATTACH_DECL_NEW(genfb_pci, sizeof(struct pci_genfb_softc),
    pci_genfb_match, pci_genfb_attach, NULL, NULL);

static int
pci_genfb_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;
	int matchlvl = 1;

	if (!genfb_is_enabled())
		return 0;	/* explicitly disabled by MD code */

	if (genfb_is_console())
		matchlvl = 5;	/* beat VGA */

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_APPLE &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_APPLE_CONTROL)
		return matchlvl;

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_DISPLAY)
		return matchlvl;

	return 0;
}

static void
pci_genfb_attach(device_t parent, device_t self, void *aux)
{
	struct pci_genfb_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	static const struct genfb_ops zero_ops;
	struct genfb_ops ops = zero_ops;
	pcireg_t rom;
	int idx, bar, type;

	pci_aprint_devinfo(pa, NULL);

	sc->sc_gen.sc_dev = self;
	sc->sc_memt = pa->pa_memt;
	sc->sc_iot = pa->pa_iot;	
	sc->sc_pc = pa->pa_pc;
	sc->sc_pcitag = pa->pa_tag;
	sc->sc_want_wsfb = 0;

	genfb_init(&sc->sc_gen);

	/* firmware / MD code responsible for restoring the display */
	if (sc->sc_gen.sc_pmfcb == NULL)
		pmf_device_register1(self, NULL, NULL,
		    pci_genfb_shutdown);
	else
		pmf_device_register1(self,
		    sc->sc_gen.sc_pmfcb->gpc_suspend,
		    sc->sc_gen.sc_pmfcb->gpc_resume,
		    pci_genfb_shutdown);

	if ((sc->sc_gen.sc_width == 0) || (sc->sc_gen.sc_fbsize == 0)) {
		aprint_debug_dev(self, "not configured by firmware\n");
		return;
	}

	/*
	 * if some MD code handed us a framebuffer VA we use that instead of
	 * mapping our own
	 */
	if (sc->sc_gen.sc_fbaddr == NULL) {
		if (bus_space_map(sc->sc_memt, sc->sc_gen.sc_fboffset,
		    sc->sc_gen.sc_fbsize,
		    BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_PREFETCHABLE,
		    &sc->sc_memh) != 0) {
			aprint_error_dev(self, "unable to map the framebuffer\n");
			return;
		}
		sc->sc_gen.sc_fbaddr = bus_space_vaddr(sc->sc_memt, sc->sc_memh);
	} else
		aprint_debug("%s: recycling existing fb mapping at %lx\n",
		    device_xname(sc->sc_gen.sc_dev), (unsigned long)sc->sc_gen.sc_fbaddr);

	/* mmap()able bus ranges */
	idx = 0;
	bar = PCI_MAPREG_START;
	while (bar <= PCI_MAPREG_ROM) {

		sc->sc_bars[(bar - PCI_MAPREG_START) >> 2] = rom =
		    pci_conf_read(sc->sc_pc, sc->sc_pcitag, bar);

		if ((bar >= PCI_MAPREG_END && bar < PCI_MAPREG_ROM) ||
		    pci_mapreg_probe(sc->sc_pc, sc->sc_pcitag, bar, &type)
		    == 0) {
			/* skip unimplemented and non-BAR registers */
			bar += 4;
			continue;
		}
		if (PCI_MAPREG_TYPE(type) == PCI_MAPREG_TYPE_MEM || 
		    PCI_MAPREG_TYPE(type) == PCI_MAPREG_TYPE_ROM) {
			pci_mapreg_info(sc->sc_pc, sc->sc_pcitag, bar, type,
			    &sc->sc_ranges[idx].offset,
			    &sc->sc_ranges[idx].size,
			    &sc->sc_ranges[idx].flags);
			idx++;
		}
		if ((bar == PCI_MAPREG_ROM) && (rom != 0)) {
			pci_conf_write(sc->sc_pc, sc->sc_pcitag, bar, rom |
			    PCI_MAPREG_ROM_ENABLE);
		}
		if (PCI_MAPREG_TYPE(type) == PCI_MAPREG_TYPE_MEM &&
		    PCI_MAPREG_MEM_TYPE(type) == PCI_MAPREG_MEM_TYPE_64BIT)
			bar += 8;
		else
			bar += 4;
	}

	sc->sc_ranges_used = idx;			    

	ops.genfb_ioctl = pci_genfb_ioctl;
	ops.genfb_mmap = pci_genfb_mmap;
	ops.genfb_borrow = pci_genfb_borrow;

	if (genfb_attach(&sc->sc_gen, &ops) == 0) {

		/* now try to attach a DRM */
		config_found_ia(self, "drm", aux, pci_genfb_drm_print);	
	}
}

static int
pci_genfb_drm_print(void *aux, const char *pnp)
{
	if (pnp)
		aprint_normal("drm at %s", pnp);
	return (UNCONF);
}


static int
pci_genfb_ioctl(void *v, void *vs, u_long cmd, void *data, int flag,
    struct lwp *l)
{
	struct pci_genfb_softc *sc = v;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_PCIMISC;
		return 0;

	/* PCI config read/write passthrough. */
	case PCI_IOC_CFGREAD:
	case PCI_IOC_CFGWRITE:
		return pci_devioctl(sc->sc_pc, sc->sc_pcitag,
		    cmd, data, flag, l);

	case WSDISPLAYIO_GET_BUSID:
		return wsdisplayio_busid_pci(sc->sc_gen.sc_dev, sc->sc_pc,
		    sc->sc_pcitag, data);

	case WSDISPLAYIO_SMODE: {
		int new_mode = *(int*)data, i;
		if (new_mode == WSDISPLAYIO_MODE_EMUL) {
			for (i = 0; i < 9; i++)
				pci_conf_write(sc->sc_pc,
				     sc->sc_pcitag,
				     0x10 + (i << 2),
				     sc->sc_bars[i]);
		}
		}
		return 0;
	}

	return EPASSTHROUGH;
}

static paddr_t
pci_genfb_mmap(void *v, void *vs, off_t offset, int prot)
{
	struct pci_genfb_softc *sc = v;
	struct range *r;
	int i;

	if (offset == 0)
		sc->sc_want_wsfb = 1;

	/*
	 * regular fb mapping at 0
	 * since some Sun firmware likes to put PCI resources low enough
	 * to collide with the wsfb mapping we only allow it after asking
	 * for offset 0
	 */
	DPRINTF("%s: %08x limit %08x\n", __func__, (uint32_t)offset,
	    (uint32_t)sc->sc_gen.sc_fbsize);
	if ((offset >= 0) && (offset < sc->sc_gen.sc_fbsize) &&
	    (sc->sc_want_wsfb == 1)) {

		return bus_space_mmap(sc->sc_memt, sc->sc_gen.sc_fboffset,
		   offset, prot,
		   BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_PREFETCHABLE);
	}

	/*
	 * restrict all other mappings to processes with superuser privileges
	 * or the kernel itself
	 */
	if (kauth_authorize_machdep(kauth_cred_get(), KAUTH_MACHDEP_UNMANAGEDMEM,
	    NULL, NULL, NULL, NULL) != 0) {
		aprint_normal_dev(sc->sc_gen.sc_dev, "mmap() rejected.\n");
		return -1;
	}

#ifdef WSFB_FAKE_VGA_FB
	if ((offset >= 0xa0000) && (offset < 0xbffff)) {

		return bus_space_mmap(sc->sc_memt, sc->sc_gen.sc_fboffset,
		   offset - 0xa0000, prot, BUS_SPACE_MAP_LINEAR);
	}
#endif

	/*
	 * XXX this should be generalized, let's just
	 * #define PCI_IOAREA_PADDR
	 * #define PCI_IOAREA_OFFSET
	 * #define PCI_IOAREA_SIZE
	 * somewhere in a MD header and compile this code only if all are
	 * present
	 */
	/*
	 * no.
	 * PCI_IOAREA_PADDR would be completely, utterly wrong and completely
	 * useless for the following reasons:
	 * - it's a bus address, not a physical address
	 * - there's no guarantee it's the same for each host bridge
	 * - it's already taken care of by the IO tag
	 * PCI_IOAREA_OFFSET is the same as PCI_MAGIC_IO_RANGE
	 * PCI_IOAREA_SIZE is also useless:
	 * - many cards don't decode more than 16 bit IO anyway
	 * - even machines with more than 64kB IO space try to keep everything
	 *   within 64kB for the reason above
	 * - IO ranges tend to be small so in most cases you can't cram enough
	 *   cards into a single machine to exhaust 64kB IO space
	 * - machines which need this tend to prefer memory space anyway
	 * - the only use for this right now is to allow the Xserver to map
	 *   VGA registers on macppc and a few other powerpc ports, shark uses
	 *   a similar mechanism, and what they need is always within 64kB
	 */
#ifdef PCI_MAGIC_IO_RANGE
	/* allow to map our IO space */
	if ((offset >= PCI_MAGIC_IO_RANGE) &&
	    (offset < PCI_MAGIC_IO_RANGE + 0x10000)) {
		return bus_space_mmap(sc->sc_iot, offset - PCI_MAGIC_IO_RANGE,
		    0, prot, BUS_SPACE_MAP_LINEAR);	
	}
#endif

	/* allow to mmap() our BARs */
	/* maybe the ROM BAR too? */
	for (i = 0; i < sc->sc_ranges_used; i++) {

		r = &sc->sc_ranges[i];
		if ((offset >= r->offset) && (offset < (r->offset + r->size))) {
			return bus_space_mmap(sc->sc_memt, offset, 0, prot,
			    r->flags);
		}
	}

	return -1;
}

int
pci_genfb_borrow(void *opaque, bus_addr_t addr, bus_space_handle_t *hdlp)
{
	struct pci_genfb_softc *sc = opaque;

	if (sc == NULL)
		return 0;
	if (!sc->sc_gen.sc_fboffset)
		return 0;
	if (sc->sc_gen.sc_fboffset != addr)
		return 0;
	*hdlp = sc->sc_memh;
	return 1;
}

static bool
pci_genfb_shutdown(device_t self, int flags)
{
	genfb_enable_polling(self);
	return true;
}

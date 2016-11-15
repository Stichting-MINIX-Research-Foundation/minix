/*	$NetBSD: pci_usrreq.c,v 1.29 2015/08/24 23:55:04 pooka Exp $	*/

/*
 * Copyright 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * User -> kernel interface for PCI bus access.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pci_usrreq.c,v 1.29 2015/08/24 23:55:04 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_pci.h"
#endif

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/kauth.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pciio.h>

static int
pciopen(dev_t dev, int flags, int mode, struct lwp *l)
{
	device_t dv;

	dv = device_lookup(&pci_cd, minor(dev));
	if (dv == NULL)
		return ENXIO;

	return 0;
}

static int
pciioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct pci_softc *sc = device_lookup_private(&pci_cd, minor(dev));
	struct pci_child *child;
	struct pciio_bdf_cfgreg *bdfr;
	struct pciio_businfo *binfo;
	struct pciio_drvname *dname;
	pcitag_t tag;

	switch (cmd) {
	case PCI_IOC_BDF_CFGREAD:
	case PCI_IOC_BDF_CFGWRITE:
		bdfr = data;
		if (bdfr->bus > 255 || bdfr->device >= sc->sc_maxndevs ||
		    bdfr->function > 7 || ISSET(bdfr->cfgreg.reg, 3))
			return EINVAL;
		tag = pci_make_tag(sc->sc_pc, bdfr->bus, bdfr->device,
		    bdfr->function);

		if (cmd == PCI_IOC_BDF_CFGREAD) {
			bdfr->cfgreg.val = pci_conf_read(sc->sc_pc, tag,
			    bdfr->cfgreg.reg);
		} else {
			if ((flag & FWRITE) == 0)
				return EBADF;
			pci_conf_write(sc->sc_pc, tag, bdfr->cfgreg.reg,
			    bdfr->cfgreg.val);
		}
		return 0;

	case PCI_IOC_BUSINFO:
		binfo = data;
		binfo->busno = sc->sc_bus;
		binfo->maxdevs = sc->sc_maxndevs;
		return 0;

	case PCI_IOC_DRVNAME:
		dname = data;
		if (dname->device >= sc->sc_maxndevs || dname->function > 7)
			return EINVAL;
		child = &sc->PCI_SC_DEVICESC(dname->device, dname->function);
		if (!child->c_dev)
			return ENXIO;
		strlcpy(dname->name, device_xname(child->c_dev),
			sizeof dname->name);
		return 0;

	default:
		return ENOTTY;
	}
}

static paddr_t
pcimmap(dev_t dev, off_t offset, int prot)
{
	struct pci_softc *sc = device_lookup_private(&pci_cd, minor(dev));
	struct pci_child *c;
	struct pci_range *r;
	int flags = 0;
	int device, range;

	if (kauth_authorize_machdep(kauth_cred_get(), KAUTH_MACHDEP_UNMANAGEDMEM,
	    NULL, NULL, NULL, NULL) != 0) {
		return -1;
	}
	/*
	 * Since we allow mapping of the entire bus, we
	 * take the offset to be the address on the bus,
	 * and pass 0 as the offset into that range.
	 *
	 * XXX Need a way to deal with linear/etc.
	 *
	 * XXX we rely on MD mmap() methods to enforce limits since these
	 * are hidden in *_tag_t structs if they exist at all 
	 */

#ifdef PCI_MAGIC_IO_RANGE
	/* 
	 * first, check if someone's trying to map the IO range
	 * XXX this assumes 64kB IO space even though some machines can have
	 * significantly more than that - macppc's bandit host bridge allows
	 * 8MB IO space and sparc64 may have the entire 4GB available. The
	 * firmware on both tries to use the lower 64kB first though and
	 * exausting it is pretty difficult so we should be safe
	 */
	if ((offset >= PCI_MAGIC_IO_RANGE) &&
	    (offset < (PCI_MAGIC_IO_RANGE + 0x10000))) {
		return bus_space_mmap(sc->sc_iot, offset - PCI_MAGIC_IO_RANGE,
		    0, prot, 0);
	}
#endif /* PCI_MAGIC_IO_RANGE */

	for (device = 0; device < __arraycount(sc->sc_devices); device++) {
		c = &sc->sc_devices[device];
		if (c->c_dev == NULL)
			continue;
		for (range = 0; range < __arraycount(c->c_range); range++) {
			r = &c->c_range[range];
			if (r->r_size == 0)
				break;
			if (offset >= r->r_offset &&
			    offset < r->r_offset + r->r_size) {
				flags = r->r_flags;
				break;
			}
		}
	}

	return bus_space_mmap(sc->sc_memt, offset, 0, prot, flags);
}

const struct cdevsw pci_cdevsw = {
	.d_open = pciopen,
	.d_close = nullclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = pciioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = pcimmap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

/*
 * pci_devioctl:
 *
 *	PCI ioctls that can be performed on devices directly.
 */
int
pci_devioctl(pci_chipset_tag_t pc, pcitag_t tag, u_long cmd, void *data,
    int flag, struct lwp *l)
{
	struct pciio_cfgreg *r = (void *) data;

	switch (cmd) {
	case PCI_IOC_CFGREAD:
		r->val = pci_conf_read(pc, tag, r->reg);
		break;

	case PCI_IOC_CFGWRITE:
		if ((flag & FWRITE) == 0)
			return EBADF;
		pci_conf_write(pc, tag, r->reg, r->val);
		break;

	default:
		return EPASSTHROUGH;
	}

	return 0;
}

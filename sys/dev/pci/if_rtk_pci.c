/*	$NetBSD: if_rtk_pci.c,v 1.46 2015/05/09 21:53:45 christos Exp $	*/

/*
 * Copyright (c) 1997, 1998
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	FreeBSD Id: if_rl.c,v 1.17 1999/06/19 20:17:37 wpaul Exp
 */

/*
 * Realtek 8129/8139 PCI NIC driver
 *
 * Supports several extremely cheap PCI 10/100 adapters based on
 * the Realtek chipset. Datasheets can be obtained from
 * www.realtek.com.tw.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_rtk_pci.c,v 1.46 2015/05/09 21:53:45 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/device.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_ether.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <sys/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/ic/rtl81x9reg.h>
#include <dev/ic/rtl81x9var.h>

struct rtk_pci_softc {
	struct rtk_softc sc_rtk;	/* real rtk softc */

	/* PCI-specific goo.*/
	void *sc_ih;
	pci_chipset_tag_t sc_pc;
};

static const struct rtk_type rtk_pci_devs[] = {
	{ PCI_VENDOR_REALTEK, PCI_PRODUCT_REALTEK_RT8129,
		RTK_8129, "Realtek 8129 10/100BaseTX" },
	{ PCI_VENDOR_REALTEK, PCI_PRODUCT_REALTEK_RT8139,
		RTK_8139, "Realtek 8139 10/100BaseTX" },
	{ PCI_VENDOR_REALTEK, PCI_PRODUCT_REALTEK_RT8138,
		RTK_8139, "Realtek 8138 10/100BaseTX" },
	{ PCI_VENDOR_REALTEK, PCI_PRODUCT_REALTEK_RT8139D,
		RTK_8139, "Realtek 8139D 10/100BaseTX" },
	{ PCI_VENDOR_REALTEK, PCI_PRODUCT_REALTEK_RT8100,
		RTK_8139, "Realtek 8100 10/100BaseTX" },
	{ PCI_VENDOR_ACCTON, PCI_PRODUCT_ACCTON_MPX5030,
		RTK_8139, "Accton MPX 5030/5038 10/100BaseTX" },
	{ PCI_VENDOR_DELTA, PCI_PRODUCT_DELTA_8139,
		RTK_8139, "Delta Electronics 8139 10/100BaseTX" },
	{ PCI_VENDOR_ADDTRON, PCI_PRODUCT_ADDTRON_8139,
		RTK_8139, "Addtron Technology 8139 10/100BaseTX" },
	{ PCI_VENDOR_SEGA, PCI_PRODUCT_SEGA_BROADBAND,
		RTK_8139, "SEGA Broadband Adapter" },
	{ PCI_VENDOR_DLINK, PCI_PRODUCT_DLINK_DFE520TX,
		RTK_8139, "D-Link Systems DFE 520TX" }, 
	{ PCI_VENDOR_DLINK, PCI_PRODUCT_DLINK_DFE530TXPLUS,
		RTK_8139, "D-Link Systems DFE 530TX+" },
	{ PCI_VENDOR_NORTEL, PCI_PRODUCT_NORTEL_BAYSTACK_21,
		RTK_8139, "Baystack 21 (MPX EN5038) 10/100BaseTX" },
};

static int	rtk_pci_match(device_t, cfdata_t, void *);
static void	rtk_pci_attach(device_t, device_t, void *);
static int	rtk_pci_detach(device_t, int);

CFATTACH_DECL_NEW(rtk_pci, sizeof(struct rtk_pci_softc),
    rtk_pci_match, rtk_pci_attach, rtk_pci_detach, NULL);

static const struct rtk_type *
rtk_pci_lookup(const struct pci_attach_args *pa)
{
	int i;

	for (i = 0; i < __arraycount(rtk_pci_devs); i++) {
		if (PCI_VENDOR(pa->pa_id) != rtk_pci_devs[i].rtk_vid)
			continue;
		if (PCI_PRODUCT(pa->pa_id) == rtk_pci_devs[i].rtk_did)
			return &rtk_pci_devs[i];
	}

	return NULL;
}

static int
rtk_pci_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (rtk_pci_lookup(pa) != NULL)
		return 1;

	return 0;
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static void
rtk_pci_attach(device_t parent, device_t self, void *aux)
{
	struct rtk_pci_softc *psc = device_private(self);
	struct rtk_softc *sc = &psc->sc_rtk;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	bus_space_tag_t iot, memt;
	bus_space_handle_t ioh, memh;
	bus_size_t iosize, memsize;
	pcireg_t csr;
	const char *intrstr = NULL;
	const struct rtk_type *t;
	bool ioh_valid, memh_valid;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;
	psc->sc_pc = pa->pa_pc;

	t = rtk_pci_lookup(pa);
	KASSERT(t != NULL);

	pci_aprint_devinfo_fancy(pa, NULL, t->rtk_name, 1);

	/*
	 * Map control/status registers.
	 *
	 * The original FreeBSD's driver has the following comment:
	 *
	 *   Default to using PIO access for this driver. On SMP systems,
	 *   there appear to be problems with memory mapped mode:
	 *   it looks like doing too many memory mapped access
	 *   back to back in rapid succession can hang the bus.
	 *   I'm inclined to blame this on crummy design/construction
	 *   on the part of Realtek. Memory mapped mode does appear to
	 *   work on uniprocessor systems though.
	 *
	 * On NetBSD, some ports don't support PCI I/O space properly,
	 * so we try to map both and prefer I/O space to mem space.
	 */
	ioh_valid = (pci_mapreg_map(pa, RTK_PCI_LOIO, PCI_MAPREG_TYPE_IO, 0,
	    &iot, &ioh, NULL, &iosize) == 0);
	memh_valid = (pci_mapreg_map(pa, RTK_PCI_LOMEM, PCI_MAPREG_TYPE_MEM, 0,
	    &memt, &memh, NULL, &memsize) == 0);
	if (ioh_valid) {
		sc->rtk_btag = iot;
		sc->rtk_bhandle = ioh;
		sc->rtk_bsize = iosize;
		if (memh_valid)
			bus_space_unmap(memt, memh, memsize);
	} else if (memh_valid) {
		sc->rtk_btag = memt;
		sc->rtk_bhandle = memh;
		sc->rtk_bsize = memsize;
	} else {
		aprint_error_dev(self, "can't map registers\n");
		return;
	}

	/* Allocate interrupt */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	psc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, rtk_intr, sc);
	if (psc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}

	if (t->rtk_basetype == RTK_8129)
		sc->sc_quirk |= RTKQ_8129;

	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	csr |= PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, csr);

	sc->sc_dmat = pa->pa_dmat;
	sc->sc_flags |= RTK_ENABLED;

	if (pmf_device_register(self, NULL, NULL))
		pmf_class_network_register(self, &sc->ethercom.ec_if);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	rtk_attach(sc);
}

static int
rtk_pci_detach(device_t self, int flags)
{
	struct rtk_pci_softc *psc = device_private(self);
	struct rtk_softc *sc = &psc->sc_rtk;
	int rv;

	/* rtk_stop() */
	sc->ethercom.ec_if.if_stop(&sc->ethercom.ec_if, 0);

	rv = rtk_detach(sc);
	if (rv)
		return rv;

	if (psc->sc_ih != NULL) {
		pci_intr_disestablish(psc->sc_pc, psc->sc_ih);
		psc->sc_ih = NULL;
	}

	if (sc->rtk_bsize != 0) {
		bus_space_unmap(sc->rtk_btag, sc->rtk_bhandle, sc->rtk_bsize);
		sc->rtk_bsize = 0;
	}

	return 0;
}

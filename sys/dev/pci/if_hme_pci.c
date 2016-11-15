/*	$NetBSD: if_hme_pci.c,v 1.37 2014/03/29 19:28:24 christos Exp $	*/

/*
 * Copyright (c) 2000 Matthew R. Green
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * PCI front-end device driver for the HME ethernet device.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_hme_pci.c,v 1.37 2014/03/29 19:28:24 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <sys/intr.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/hmevar.h>

#define PCI_HME_BASEADDR	PCI_BAR(0)

struct hme_pci_softc {
	struct	hme_softc	hsc_hme;	/* HME device */
	bus_space_tag_t		hsc_memt;
	bus_space_handle_t	hsc_memh;
	void			*hsc_ih;
};

int	hmematch_pci(device_t, cfdata_t, void *);
void	hmeattach_pci(device_t, device_t, void *);

CFATTACH_DECL_NEW(hme_pci, sizeof(struct hme_pci_softc),
    hmematch_pci, hmeattach_pci, NULL, NULL);

int
hmematch_pci(device_t parent, cfdata_t cf, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SUN &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SUN_HMENETWORK)
		return (1);

	return (0);
}

static inline int
hmepromvalid(uint8_t* buf)
{
	return buf[0] == 0x18 && buf[1] == 0x00 &&	/* structure length */
	    buf[2] == 0x00 &&				/* revision */
	    (buf[3] == 0x00 ||				/* hme */
	     buf[3] == 0x80) &&				/* qfe */
	    buf[4] == PCI_SUBCLASS_NETWORK_ETHERNET &&	/* subclass code */
	    buf[5] == PCI_CLASS_NETWORK;		/* class code */
}

static inline int
hmevpdoff(bus_space_tag_t romt, bus_space_handle_t romh, int vpdoff, int dev)
{
#define VPDLEN (3 + sizeof(struct pci_vpd) + ETHER_ADDR_LEN)
	if (bus_space_read_1(romt, romh, vpdoff + VPDLEN) != 0x79 &&
	    bus_space_read_1(romt, romh, vpdoff + 4 * VPDLEN) == 0x79) {
		/*
		 * Use the Nth NA for the Nth HME on
		 * this SUNW,qfe.
		 */
		vpdoff += dev * VPDLEN;
	}
	return vpdoff;
}

void
hmeattach_pci(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct hme_pci_softc *hsc = device_private(self);
	struct hme_softc *sc = &hsc->hsc_hme;
	pci_intr_handle_t ih;
	pcireg_t csr;
	const char *intrstr;
	int type;
	struct pci_attach_args	ebus_pa;
	prop_data_t		eaddrprop;
	pcireg_t		ebus_cl, ebus_id;
	uint8_t			*enaddr;
	bus_space_tag_t		romt;
	bus_space_handle_t	romh;
	bus_size_t		romsize;
	uint8_t			buf[64];
	int			dataoff, vpdoff;
	struct pci_vpd		*vpd;
	static const uint8_t promhdr[] = { 0x55, 0xaa };
#define PROMHDR_PTR_DATA	0x18
	static const uint8_t promdat[] = {
		0x50, 0x43, 0x49, 0x52,		/* "PCIR" */
		PCI_VENDOR_SUN & 0xff, PCI_VENDOR_SUN >> 8,
		PCI_PRODUCT_SUN_HMENETWORK & 0xff,
		PCI_PRODUCT_SUN_HMENETWORK >> 8
	};
#define PROMDATA_PTR_VPD	0x08
#define PROMDATA_DATA2		0x0a
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;

	aprint_normal(": Sun Happy Meal Ethernet, rev. %d\n",
	    PCI_REVISION(pa->pa_class));
	aprint_naive(": Ethernet controller\n");

	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	type = pci_mapreg_type(pa->pa_pc, pa->pa_tag, PCI_HME_BASEADDR);

	/*
	 * enable io/memory-space accesses.  this is kinda of gross; but
	 * the hme comes up with neither IO space enabled, or memory space.
	 */
	switch (type) {
	case PCI_MAPREG_TYPE_MEM:
		csr |= PCI_COMMAND_MEM_ENABLE;
		sc->sc_bustag = pa->pa_memt;
		break;
	case PCI_MAPREG_TYPE_IO:
		csr |= PCI_COMMAND_IO_ENABLE;
		sc->sc_bustag = pa->pa_iot;
		break;
	}
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MEM_ENABLE);

	sc->sc_dmatag = pa->pa_dmat;

	sc->sc_pci = 1; /* XXXXX should all be done in bus_dma. */
	/*
	 * Map five register banks:
	 *
	 *	bank 0: HME SEB registers:	+0x0000
	 *	bank 1: HME ETX registers:	+0x2000
	 *	bank 2: HME ERX registers:	+0x4000
	 *	bank 3: HME MAC registers:	+0x6000
	 *	bank 4: HME MIF registers:	+0x7000
	 *
	 */

	if (pci_mapreg_map(pa, PCI_HME_BASEADDR, type, 0,
	    &hsc->hsc_memt, &hsc->hsc_memh, NULL, NULL) != 0) {
		aprint_error_dev(self, "unable to map device registers\n");
		return;
	}
	sc->sc_seb = hsc->hsc_memh;
	if (bus_space_subregion(hsc->hsc_memt, hsc->hsc_memh, 0x2000,
	    0x1000, &sc->sc_etx)) {
		aprint_error_dev(self, "unable to subregion ETX registers\n");
		return;
	}
	if (bus_space_subregion(hsc->hsc_memt, hsc->hsc_memh, 0x4000,
	    0x1000, &sc->sc_erx)) {
		aprint_error_dev(self, "unable to subregion ERX registers\n");
		return;
	}
	if (bus_space_subregion(hsc->hsc_memt, hsc->hsc_memh, 0x6000,
	    0x1000, &sc->sc_mac)) {
		aprint_error_dev(self, "unable to subregion MAC registers\n");
		return;
	}
	if (bus_space_subregion(hsc->hsc_memt, hsc->hsc_memh, 0x7000,
	    0x1000, &sc->sc_mif)) {
		aprint_error_dev(self, "unable to subregion MIF registers\n");
		return;
	}


	/*
	 * Check if we got a mac-address property passed
	 */
	eaddrprop = prop_dictionary_get(device_properties(self), "mac-address");

	if (eaddrprop != NULL && prop_data_size(eaddrprop) == ETHER_ADDR_LEN) {
		memcpy(&sc->sc_enaddr, prop_data_data_nocopy(eaddrprop),
			    ETHER_ADDR_LEN);
		goto got_eaddr;
	}

	/*
	 * Dig out VPD (vital product data) and acquire Ethernet address.
	 * The VPD of hme resides in the Boot PROM (PCI FCode) attached
	 * to the EBus interface.
	 */
	/*
	 * ``Writing FCode 3.x Programs'' (newer ones, dated 1997 and later)
	 * chapter 2 describes the data structure.
	 */

	enaddr = NULL;

	/* get a PCI tag for the EBus bridge (function 0 of the same device) */
	ebus_pa = *pa;
	ebus_pa.pa_tag = pci_make_tag(pa->pa_pc, pa->pa_bus, pa->pa_device, 0);

	ebus_cl = pci_conf_read(ebus_pa.pa_pc, ebus_pa.pa_tag, PCI_CLASS_REG);
	ebus_id = pci_conf_read(ebus_pa.pa_pc, ebus_pa.pa_tag, PCI_ID_REG);

#define PCI_EBUS2_BOOTROM	0x10
	if (PCI_CLASS(ebus_cl) == PCI_CLASS_BRIDGE &&
	    PCI_PRODUCT(ebus_id) == PCI_PRODUCT_SUN_EBUS &&
	    pci_mapreg_map(&ebus_pa, PCI_EBUS2_BOOTROM, PCI_MAPREG_TYPE_MEM,
		BUS_SPACE_MAP_CACHEABLE | BUS_SPACE_MAP_PREFETCHABLE,
		&romt, &romh, 0, &romsize) == 0) {

		/* read PCI Expansion PROM Header */
		bus_space_read_region_1(romt, romh, 0, buf, sizeof buf);
		if (memcmp(buf, promhdr, sizeof promhdr) == 0 &&
		    (dataoff = (buf[PROMHDR_PTR_DATA] |
			(buf[PROMHDR_PTR_DATA + 1] << 8))) >= 0x1c) {

			/* read PCI Expansion PROM Data */
			bus_space_read_region_1(romt, romh, dataoff,
			    buf, sizeof buf);
			if (memcmp(buf, promdat, sizeof promdat) == 0 &&
			    hmepromvalid(buf + PROMDATA_DATA2) &&
			    (vpdoff = (buf[PROMDATA_PTR_VPD] |
				(buf[PROMDATA_PTR_VPD + 1] << 8))) >= 0x1c) {

				/*
				 * The VPD of hme is not in PCI 2.2 standard
				 * format.  The length in the resource header
				 * is in big endian, and resources are not
				 * properly terminated (only one resource
				 * and no end tag).
				 */
				vpdoff = hmevpdoff(romt, romh, vpdoff,
				    pa->pa_device);
				/* read PCI VPD */
				bus_space_read_region_1(romt, romh,
				    vpdoff, buf, sizeof buf);
				vpd = (void *)(buf + 3);
				if (PCI_VPDRES_ISLARGE(buf[0]) &&
				    PCI_VPDRES_LARGE_NAME(buf[0])
					== PCI_VPDRES_TYPE_VPD &&
				    /* buf[1] == 0 && buf[2] == 9 && */ /*len*/
				    vpd->vpd_key0 == 0x4e /* N */ &&
				    vpd->vpd_key1 == 0x41 /* A */ &&
				    vpd->vpd_len == ETHER_ADDR_LEN) {
					/*
					 * Ethernet address found
					 */
					enaddr = buf + 6;
				}
			}
		}
		bus_space_unmap(romt, romh, romsize);
	}

	if (enaddr) {
		memcpy(sc->sc_enaddr, enaddr, ETHER_ADDR_LEN);
		goto got_eaddr;
	}

	aprint_error_dev(self, "no Ethernet address found\n");
got_eaddr:

	/*
	 * Map and establish our interrupt.
	 */
	if (pci_intr_map(pa, &ih) != 0) {
		aprint_error_dev(self, "unable to map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih, intrbuf, sizeof(intrbuf));
	hsc->hsc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_NET, hme_intr, sc);
	if (hsc->hsc_ih == NULL) {
		aprint_error_dev(self, "unable to establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	sc->sc_burst = 16;	/* XXX */

	/* Finish off the attach. */
	hme_config(sc);
}

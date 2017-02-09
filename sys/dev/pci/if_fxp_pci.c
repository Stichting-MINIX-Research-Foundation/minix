/*	$NetBSD: if_fxp_pci.c,v 1.82 2015/04/13 16:33:25 riastradh Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 1999, 2000, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

/*
 * PCI bus front-end for the Intel i82557 fast Ethernet controller
 * driver.  Works with Intel Etherexpress Pro 10+, 100B, 100+ cards.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_fxp_pci.c,v 1.82 2015/04/13 16:33:25 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <machine/endian.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/mii/miivar.h>

#include <dev/ic/i82557reg.h>
#include <dev/ic/i82557var.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

struct fxp_pci_softc {
	struct fxp_softc psc_fxp;

	pci_chipset_tag_t psc_pc;	/* pci chipset tag */
	pcireg_t psc_regs[0x20>>2];	/* saved PCI config regs (sparse) */
	pcitag_t psc_tag;		/* pci register tag */

	struct pci_conf_state psc_pciconf; /* standard PCI configuration regs */
};

static int	fxp_pci_match(device_t, cfdata_t, void *);
static void	fxp_pci_attach(device_t, device_t, void *);
static int	fxp_pci_detach(device_t, int);

static int	fxp_pci_enable(struct fxp_softc *);

static void fxp_pci_confreg_restore(struct fxp_pci_softc *psc);
static bool fxp_pci_resume(device_t dv, const pmf_qual_t *);

CFATTACH_DECL3_NEW(fxp_pci, sizeof(struct fxp_pci_softc),
    fxp_pci_match, fxp_pci_attach, fxp_pci_detach, NULL, NULL,
    null_childdetached, DVF_DETACH_SHUTDOWN);

static const struct fxp_pci_product {
	uint32_t	fpp_prodid;	/* PCI product ID */
	const char	*fpp_name;	/* device name */
} fxp_pci_products[] = {
	{ PCI_PRODUCT_INTEL_82552,
	  "Intel i82552 10/100 Network Connection" },
	{ PCI_PRODUCT_INTEL_8255X,
	  "Intel i8255x Ethernet" },
	{ PCI_PRODUCT_INTEL_82559ER,
	  "Intel i82559ER Ethernet" },
	{ PCI_PRODUCT_INTEL_IN_BUSINESS,
	  "Intel InBusiness Ethernet" },
	{ PCI_PRODUCT_INTEL_PRO_100,
	  "Intel PRO/100 Ethernet" },
	{ PCI_PRODUCT_INTEL_PRO_100_VE_0,
	  "Intel PRO/100 VE Network Controller" },
	{ PCI_PRODUCT_INTEL_PRO_100_VE_1,
	  "Intel PRO/100 VE Network Controller" },
	{ PCI_PRODUCT_INTEL_PRO_100_VE_2,
	  "Intel PRO/100 VE Network Controller with 82562ET/EZ PHY" },
	{ PCI_PRODUCT_INTEL_PRO_100_VE_3,
	  "Intel PRO/100 VE Network Controller with 82562ET/EZ (CNR) PHY" },
	{ PCI_PRODUCT_INTEL_PRO_100_VE_4,
	  "Intel PRO/100 VE (MOB) Network Controller" },
	{ PCI_PRODUCT_INTEL_PRO_100_VE_5,
	  "Intel PRO/100 VE (LOM) Network Controller" },
	{ PCI_PRODUCT_INTEL_PRO_100_VE_6,
	  "Intel PRO/100 VE Network Controller" },
	{ PCI_PRODUCT_INTEL_PRO_100_VE_7,
	  "Intel PRO/100 VE Network Controller" },
	{ PCI_PRODUCT_INTEL_PRO_100_VE_8,
	  "Intel PRO/100 VE Network Controller" },
	{ PCI_PRODUCT_INTEL_PRO_100_VE_9,
	  "Intel PRO/100 VE Network Controller" },
	{ PCI_PRODUCT_INTEL_PRO_100_VE_10,
	  "Intel PRO/100 VE Network Controller" },
	{ PCI_PRODUCT_INTEL_PRO_100_VE_11,
	  "Intel PRO/100 VE Network Controller" },
	{ PCI_PRODUCT_INTEL_PRO_100_VM_0,
	  "Intel PRO/100 VM Network Controller" },
	{ PCI_PRODUCT_INTEL_PRO_100_VM_1,
	  "Intel PRO/100 VM Network Controller" },
	{ PCI_PRODUCT_INTEL_PRO_100_VM_2,
	  "Intel PRO/100 VM Network Controller" },
	{ PCI_PRODUCT_INTEL_PRO_100_VM_3,
	  "Intel PRO/100 VM Network Controller with 82562EM/EX PHY" },
	{ PCI_PRODUCT_INTEL_PRO_100_VM_4,
	  "Intel PRO/100 VM Network Controller with 82562EM/EX (CNR) PHY" },
	{ PCI_PRODUCT_INTEL_PRO_100_VM_5,
	  "Intel PRO/100 VM (MOB) Network Controller" },
	{ PCI_PRODUCT_INTEL_PRO_100_VM_6,
	  "Intel PRO/100 VM Network Controller with 82562ET/EZ PHY" },
	{ PCI_PRODUCT_INTEL_PRO_100_VM_7,
	  "Intel PRO/100 VM Network Connection" },
	{ PCI_PRODUCT_INTEL_PRO_100_VM_8,
	  "Intel PRO/100 VM Network Connection" },
	{ PCI_PRODUCT_INTEL_PRO_100_VM_9,
	  "Intel PRO/100 VM Network Connection" },
	{ PCI_PRODUCT_INTEL_PRO_100_VM_10,
	  "Intel PRO/100 VM Network Connection" },
	{ PCI_PRODUCT_INTEL_PRO_100_VM_11,
	  "Intel PRO/100 VM Network Connection" },
	{ PCI_PRODUCT_INTEL_PRO_100_VM_12,
	  "Intel PRO/100 VM Network Connection" },
	{ PCI_PRODUCT_INTEL_PRO_100_VM_13,
	  "Intel PRO/100 VM Network Connection" },
	{ PCI_PRODUCT_INTEL_PRO_100_VM_14,
	  "Intel PRO/100 VM Network Connection" },
	{ PCI_PRODUCT_INTEL_PRO_100_VM_15,
	  "Intel PRO/100 VM Network Connection" },
	{ PCI_PRODUCT_INTEL_PRO_100_VM_16,
	  "Intel PRO/100 VM Network Connection" },
	{ PCI_PRODUCT_INTEL_PRO_100_M,
	  "Intel PRO/100 M Network Controller" },
	{ PCI_PRODUCT_INTEL_82801BA_LAN,
	  "Intel i82562 Ethernet" },
	{ PCI_PRODUCT_INTEL_82801E_LAN_1,
	  "Intel i82801E Ethernet" },
	{ PCI_PRODUCT_INTEL_82801E_LAN_2,
	  "Intel i82801E Ethernet" },
	{ PCI_PRODUCT_INTEL_82801EB_LAN,
	  "Intel 82801EB/ER (ICH5) Network Controller" },
	{ PCI_PRODUCT_INTEL_82801FB_LAN,
	  "Intel i82801FB LAN Controller" },
	{ PCI_PRODUCT_INTEL_82801FB_LAN_2,
	  "Intel i82801FB LAN Controller" },
	{ PCI_PRODUCT_INTEL_82801G_LAN,
	  "Intel 82801GB/GR (ICH7) Network Controller" },
	{ PCI_PRODUCT_INTEL_82801GB_LAN,
	  "Intel 82801GB 10/100 Network Controller" },
	{ 0,
	  NULL },
};

static const struct fxp_pci_product *
fxp_pci_lookup(const struct pci_attach_args *pa)
{
	const struct fxp_pci_product *fpp;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_INTEL)
		return (NULL);

	for (fpp = fxp_pci_products; fpp->fpp_name != NULL; fpp++)
		if (PCI_PRODUCT(pa->pa_id) == fpp->fpp_prodid)
			return (fpp);

	return (NULL);
}

static int
fxp_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (fxp_pci_lookup(pa) != NULL)
		return (1);

	return (0);
}

/*
 * On resume : (XXX it is necessary with new pmf framework ?)
 * Restore PCI configuration registers that may have been clobbered.
 * This is necessary due to bugs on the Sony VAIO Z505-series on-board
 * ethernet, after an APM suspend/resume, as well as after an ACPI
 * D3->D0 transition.  We call this function from a power hook after
 * APM resume events, as well as after the ACPI D3->D0 transition.
 */
static void
fxp_pci_confreg_restore(struct fxp_pci_softc *psc)
{
	pcireg_t reg;

#if 0
	/*
	 * Check to see if the command register is blank -- if so, then
	 * we'll assume that all the clobberable-registers have been
	 * clobbered.
	 */

	/*
	 * In general, the above metric is accurate. Unfortunately,
	 * it is inaccurate across a hibernation. Ideally APM/ACPI
	 * code should take note of hibernation events and execute
	 * a hibernation wakeup hook, but at present a hibernation wake
	 * is indistinguishable from a suspend wake.
	 */

	if (((reg = pci_conf_read(psc->psc_pc, psc->psc_tag,
	    PCI_COMMAND_STATUS_REG)) & 0xffff) != 0)
		return;
#else
	reg = pci_conf_read(psc->psc_pc, psc->psc_tag, PCI_COMMAND_STATUS_REG);
#endif

	pci_conf_write(psc->psc_pc, psc->psc_tag,
	    PCI_COMMAND_STATUS_REG,
	    (reg & 0xffff0000) |
	    (psc->psc_regs[PCI_COMMAND_STATUS_REG>>2] & 0xffff));
	pci_conf_write(psc->psc_pc, psc->psc_tag, PCI_BHLC_REG,
	    psc->psc_regs[PCI_BHLC_REG>>2]);
	pci_conf_write(psc->psc_pc, psc->psc_tag, PCI_MAPREG_START+0x0,
	    psc->psc_regs[(PCI_MAPREG_START+0x0)>>2]);
	pci_conf_write(psc->psc_pc, psc->psc_tag, PCI_MAPREG_START+0x4,
	    psc->psc_regs[(PCI_MAPREG_START+0x4)>>2]);
	pci_conf_write(psc->psc_pc, psc->psc_tag, PCI_MAPREG_START+0x8,
	    psc->psc_regs[(PCI_MAPREG_START+0x8)>>2]);
}

static bool
fxp_pci_resume(device_t dv, const pmf_qual_t *qual)
{
	struct fxp_pci_softc *psc = device_private(dv);
	fxp_pci_confreg_restore(psc);

	return true;
}

static int
fxp_pci_detach(device_t self, int flags)
{
	struct fxp_pci_softc *psc = device_private(self);
	struct fxp_softc *sc = &psc->psc_fxp;
	int error;

	/* Finish off the attach. */
	if ((error = fxp_detach(sc, flags)) != 0)
		return error;

	pmf_device_deregister(self);

	pci_intr_disestablish(psc->psc_pc, sc->sc_ih);

	bus_space_unmap(sc->sc_st, sc->sc_sh, sc->sc_size);

	return 0;
}

static void
fxp_pci_attach(device_t parent, device_t self, void *aux)
{
	struct fxp_pci_softc *psc = device_private(self);
	struct fxp_softc *sc = &psc->psc_fxp;
	const struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const struct fxp_pci_product *fpp;
	const char *chipname = NULL;
	const char *intrstr = NULL;
	bus_space_tag_t iot, memt;
	bus_space_handle_t ioh, memh;
	int ioh_valid, memh_valid;
	bus_addr_t addr;
	int flags;
	int error;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;

	/*
	 * Map control/status registers.
	 */
	ioh_valid = (pci_mapreg_map(pa, FXP_PCI_IOBA,
	    PCI_MAPREG_TYPE_IO, 0,
	    &iot, &ioh, NULL, NULL) == 0);

	/*
	 * Version 2.1 of the PCI spec, page 196, "Address Maps":
	 *
	 *	Prefetchable
	 *
	 *	Set to one if there are no side effects on reads, the
	 *	device returns all bytes regardless of the byte enables,
	 *	and host bridges can merge processor writes into this
	 *	range without causing errors.  Bit must be set to zero
	 *	otherwise.
	 *
	 * The 82557 incorrectly sets the "prefetchable" bit, resulting
	 * in errors on systems which will do merged reads and writes.
	 * These errors manifest themselves as all-bits-set when reading
	 * from the EEPROM or other < 4 byte registers.
	 *
	 * We must work around this problem by always forcing the mapping
	 * for memory space to be uncacheable.  On systems which cannot
	 * create an uncacheable mapping (because the firmware mapped it
	 * into only cacheable/prefetchable space due to the "prefetchable"
	 * bit), we can fall back onto i/o mapped access.
	 */
	memh_valid = 0;
	memt = pa->pa_memt;
	if (((pa->pa_flags & PCI_FLAGS_MEM_OKAY) != 0) &&
	    pci_mapreg_info(pa->pa_pc, pa->pa_tag, FXP_PCI_MMBA,
	    PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT,
	    &addr, &sc->sc_size, &flags) == 0) {
		flags &= ~BUS_SPACE_MAP_PREFETCHABLE;
		if (bus_space_map(memt, addr, sc->sc_size, flags, &memh) == 0)
			memh_valid = 1;
	}

	if (memh_valid) {
		sc->sc_st = memt;
		sc->sc_sh = memh;
	} else if (ioh_valid) {
		sc->sc_st = iot;
		sc->sc_sh = ioh;
	} else {
		aprint_error(": unable to map device registers\n");
		return;
	}

	sc->sc_dmat = pa->pa_dmat;

	fpp = fxp_pci_lookup(pa);
	if (fpp == NULL) {
		printf("\n");
		panic("fxp_pci_attach: impossible");
	}

	sc->sc_rev = PCI_REVISION(pa->pa_class);

	switch (fpp->fpp_prodid) {
	case PCI_PRODUCT_INTEL_8255X:
	case PCI_PRODUCT_INTEL_IN_BUSINESS:

		if (sc->sc_rev >= FXP_REV_82558_A4) {
			chipname = "i82558 Ethernet";
			sc->sc_flags |= FXPF_FC|FXPF_EXT_TXCB;
			/*
			 * Enable the MWI command for memory writes.
			 */
			if (pa->pa_flags & PCI_FLAGS_MWI_OKAY)
				sc->sc_flags |= FXPF_MWI;
		}
		if (sc->sc_rev >= FXP_REV_82559_A0) {
			chipname = "i82559 Ethernet";
			sc->sc_flags |= FXPF_82559_RXCSUM;
		}
		if (sc->sc_rev >= FXP_REV_82559S_A)
			chipname = "i82559S Ethernet";
		if (sc->sc_rev >= FXP_REV_82550) {
			chipname = "i82550 Ethernet";
			sc->sc_flags &= ~FXPF_82559_RXCSUM;
			sc->sc_flags |= FXPF_EXT_RFA;
		}
		if (sc->sc_rev >= FXP_REV_82551_E)
			chipname = "i82551 Ethernet";

		/*
		 * Mark all i82559 and i82550 revisions as having
		 * the "resume bug".  See i82557.c for details.
		 */
		if (sc->sc_rev >= FXP_REV_82559_A0)
			sc->sc_flags |= FXPF_HAS_RESUME_BUG;

		break;

	case PCI_PRODUCT_INTEL_82559ER:
		sc->sc_flags |= FXPF_FC|FXPF_EXT_TXCB;

		/*
		 * i82559ER/82551ER don't support RX hardware checksumming
		 * even though it has a newer revision number than 82559_A0.
		 */

		/* All i82559 have the "resume bug". */
		sc->sc_flags |= FXPF_HAS_RESUME_BUG;

		/* Enable the MWI command for memory writes. */
		if (pa->pa_flags & PCI_FLAGS_MWI_OKAY)
			sc->sc_flags |= FXPF_MWI;

		if (sc->sc_rev >= FXP_REV_82551_E)
			chipname = "Intel i82551ER Ethernet";

		break;

	case PCI_PRODUCT_INTEL_82801BA_LAN:
	case PCI_PRODUCT_INTEL_PRO_100_VE_0:
	case PCI_PRODUCT_INTEL_PRO_100_VE_1:
	case PCI_PRODUCT_INTEL_PRO_100_VM_0:
	case PCI_PRODUCT_INTEL_PRO_100_VM_1:
	case PCI_PRODUCT_INTEL_82562EH_HPNA_0:
	case PCI_PRODUCT_INTEL_82562EH_HPNA_1:
	case PCI_PRODUCT_INTEL_82562EH_HPNA_2:
	case PCI_PRODUCT_INTEL_PRO_100_VM_2:
		/*
		 * The ICH-2 and ICH-3 have the "resume bug".
		 */
		sc->sc_flags |= FXPF_HAS_RESUME_BUG;
		/* FALLTHROUGH */

	default:
		if (sc->sc_rev >= FXP_REV_82558_A4)
			sc->sc_flags |= FXPF_FC|FXPF_EXT_TXCB;
		if (sc->sc_rev >= FXP_REV_82559_A0)
			sc->sc_flags |= FXPF_82559_RXCSUM;

		break;
	}

	pci_aprint_devinfo_fancy(pa, "Ethernet controller",
		(chipname ? chipname : fpp->fpp_name), 1);

	/* Make sure bus-mastering is enabled. */
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG) |
	    PCI_COMMAND_MASTER_ENABLE);

  	/*
	 * Under some circumstances (such as APM suspend/resume
	 * cycles, and across ACPI power state changes), the
	 * i82257-family can lose the contents of critical PCI
	 * configuration registers, causing the card to be
	 * non-responsive and useless.  This occurs on the Sony VAIO
	 * Z505-series, among others.  Preserve them here so they can
	 * be later restored (by fxp_pci_confreg_restore()).
	 */
	psc->psc_pc = pc;
	psc->psc_tag = pa->pa_tag;
	psc->psc_regs[PCI_COMMAND_STATUS_REG>>2] =
	    pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	psc->psc_regs[PCI_BHLC_REG>>2] =
	    pci_conf_read(pc, pa->pa_tag, PCI_BHLC_REG);
	psc->psc_regs[(PCI_MAPREG_START+0x0)>>2] =
	    pci_conf_read(pc, pa->pa_tag, PCI_MAPREG_START+0x0);
	psc->psc_regs[(PCI_MAPREG_START+0x4)>>2] =
	    pci_conf_read(pc, pa->pa_tag, PCI_MAPREG_START+0x4);
	psc->psc_regs[(PCI_MAPREG_START+0x8)>>2] =
	    pci_conf_read(pc, pa->pa_tag, PCI_MAPREG_START+0x8);

	/* power up chip */
	switch ((error = pci_activate(pa->pa_pc, pa->pa_tag, self,
	    pci_activate_null))) {
	case EOPNOTSUPP:
		break;
	case 0:
		sc->sc_enable = fxp_pci_enable;
		sc->sc_disable = NULL;
		break;
	default:
		aprint_error_dev(self, "cannot activate %d\n", error);
		return;
	}

	/* Restore PCI configuration registers. */
	fxp_pci_confreg_restore(psc);

	sc->sc_enabled = 1;

	/*
	 * Map and establish our interrupt.
	 */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, fxp_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	/* Finish off the attach. */
	fxp_attach(sc);
	if (sc->sc_disable != NULL)
		fxp_disable(sc);

	/* Add a suspend hook to restore PCI config state */
	if (pmf_device_register(self, NULL, fxp_pci_resume))
		pmf_class_network_register(self, &sc->sc_ethercom.ec_if);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static int
fxp_pci_enable(struct fxp_softc *sc)
{
	struct fxp_pci_softc *psc = (void *) sc;

#if 0
	printf("%s: going to power state D0\n", device_xname(self));
#endif

	/* Now restore the configuration registers. */
	fxp_pci_confreg_restore(psc);

	return (0);
}

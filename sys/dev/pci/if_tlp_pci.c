/*	$NetBSD: if_tlp_pci.c,v 1.123 2014/03/29 19:28:25 christos Exp $	*/

/*-
 * Copyright (c) 1998, 1999, 2000, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center; and Charles M. Hannum.
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
 * PCI bus front-end for the Digital Semiconductor ``Tulip'' (21x4x)
 * Ethernet controller family driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_tlp_pci.c,v 1.123 2014/03/29 19:28:25 christos Exp $");

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
#include <dev/mii/mii_bitbang.h>

#include <dev/ic/tulipreg.h>
#include <dev/ic/tulipvar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

/*
 * PCI configuration space registers used by the Tulip.
 */
#define TULIP_PCI_IOBA PCI_BAR(0)	/* i/o mapped base */
#define TULIP_PCI_MMBA PCI_BAR(1)	/* memory mapped base */
#define	TULIP_PCI_CFDA		0x40	/* configuration driver area */

#define	CFDA_SLEEP		0x80000000	/* sleep mode */
#define	CFDA_SNOOZE		0x40000000	/* snooze mode */

struct tulip_pci_softc {
	struct tulip_softc sc_tulip;	/* real Tulip softc */

	/* PCI-specific goo. */
	void	*sc_ih;			/* interrupt handle */
	bus_size_t sc_mapsize;

	pci_chipset_tag_t sc_pc;	/* our PCI chipset */
	pcitag_t sc_pcitag;		/* our PCI tag */

	int	sc_flags;		/* flags; see below */

	LIST_HEAD(, tulip_pci_softc) sc_intrslaves;
	LIST_ENTRY(tulip_pci_softc) sc_intrq;

	/* Our {ROM,interrupt} master. */
	struct tulip_pci_softc *sc_master;
};

/* sc_flags */
#define	TULIP_PCI_SHAREDINTR	0x01	/* interrupt is shared */
#define	TULIP_PCI_SLAVEINTR	0x02	/* interrupt is slave */
#define	TULIP_PCI_SHAREDROM	0x04	/* ROM is shared */
#define	TULIP_PCI_SLAVEROM	0x08	/* slave of shared ROM */

static int	tlp_pci_match(device_t, cfdata_t, void *);
static void	tlp_pci_attach(device_t, device_t, void *);
static int	tlp_pci_detach(device_t, int);

CFATTACH_DECL3_NEW(tlp_pci, sizeof(struct tulip_pci_softc),
    tlp_pci_match, tlp_pci_attach, tlp_pci_detach, NULL, NULL, NULL,
    DVF_DETACH_SHUTDOWN);

static const struct tulip_pci_product {
	uint32_t	tpp_vendor;	/* PCI vendor ID */
	uint32_t	tpp_product;	/* PCI product ID */
	tulip_chip_t	tpp_chip;	/* base Tulip chip type */
} tlp_pci_products[] = {
	{ PCI_VENDOR_DEC,		PCI_PRODUCT_DEC_21040,
	  TULIP_CHIP_21040 },
	{ PCI_VENDOR_DEC,		PCI_PRODUCT_DEC_21041,
	  TULIP_CHIP_21041 },
	{ PCI_VENDOR_DEC,		PCI_PRODUCT_DEC_21140,
	  TULIP_CHIP_21140 },
	{ PCI_VENDOR_DEC,		PCI_PRODUCT_DEC_21142,
	  TULIP_CHIP_21142 },

	{ PCI_VENDOR_LITEON,		PCI_PRODUCT_LITEON_82C168,
	  TULIP_CHIP_82C168 },

	/*
	 * Note: This is like a MX98725 with Wake-On-LAN and a
	 * 128-bit multicast hash table.
	 */
	{ PCI_VENDOR_LITEON,		PCI_PRODUCT_LITEON_82C115,
	  TULIP_CHIP_82C115 },

	{ PCI_VENDOR_MACRONIX,		PCI_PRODUCT_MACRONIX_MX98713,
	  TULIP_CHIP_MX98713 },
	{ PCI_VENDOR_MACRONIX,		PCI_PRODUCT_MACRONIX_MX987x5,
	  TULIP_CHIP_MX98715 },

	{ PCI_VENDOR_COMPEX,		PCI_PRODUCT_COMPEX_RL100TX,
	  TULIP_CHIP_MX98713 },

	{ PCI_VENDOR_WINBOND,		PCI_PRODUCT_WINBOND_W89C840F,
	  TULIP_CHIP_WB89C840F },
	{ PCI_VENDOR_COMPEX,		PCI_PRODUCT_COMPEX_RL100ATX,
	  TULIP_CHIP_WB89C840F },

	{ PCI_VENDOR_DAVICOM,		PCI_PRODUCT_DAVICOM_DM9102,
	  TULIP_CHIP_DM9102 },

	{ PCI_VENDOR_ADMTEK,		PCI_PRODUCT_ADMTEK_AL981,
	  TULIP_CHIP_AL981 },

	{ PCI_VENDOR_ADMTEK,		PCI_PRODUCT_ADMTEK_AN983,
	  TULIP_CHIP_AN985 },
	{ PCI_VENDOR_ADMTEK,		PCI_PRODUCT_ADMTEK_ADM9511,
	  TULIP_CHIP_AN985 },
	{ PCI_VENDOR_ADMTEK,		PCI_PRODUCT_ADMTEK_ADM9513,
	  TULIP_CHIP_AN985 },
	{ PCI_VENDOR_ACCTON,		PCI_PRODUCT_ACCTON_EN2242,
	  TULIP_CHIP_AN985 },

	{ PCI_VENDOR_3COM,		PCI_PRODUCT_3COM_3C910SOHOB,
	  TULIP_CHIP_AN985 },

	{ PCI_VENDOR_ASIX,		PCI_PRODUCT_ASIX_AX88140A,
	  TULIP_CHIP_AX88140 },

	{ PCI_VENDOR_CONEXANT,		PCI_PRODUCT_CONEXANT_LANFINITY,
	  TULIP_CHIP_RS7112 },

	{ 0,				0,
	  TULIP_CHIP_INVALID },
};

struct tlp_pci_quirks {
	void		(*tpq_func)(struct tulip_pci_softc *,
			    const uint8_t *);
	uint8_t		tpq_oui[3];
};

static void	tlp_pci_dec_quirks(struct tulip_pci_softc *,
		    const uint8_t *);

static void	tlp_pci_znyx_21040_quirks(struct tulip_pci_softc *,
		    const uint8_t *);
static void	tlp_pci_smc_21040_quirks(struct tulip_pci_softc *,
		    const uint8_t *);
static void	tlp_pci_cogent_21040_quirks(struct tulip_pci_softc *,
		    const uint8_t *);
static void	tlp_pci_accton_21040_quirks(struct tulip_pci_softc *,
		    const uint8_t *);

static void	tlp_pci_cobalt_21142_quirks(struct tulip_pci_softc *,
		    const uint8_t *);
static void	tlp_pci_algor_21142_quirks(struct tulip_pci_softc *,
		    const uint8_t *);
static void	tlp_pci_netwinder_21142_quirks(struct tulip_pci_softc *,
		    const uint8_t *);
static void	tlp_pci_phobos_21142_quirks(struct tulip_pci_softc *,
		    const uint8_t *);
static void	tlp_pci_znyx_21142_quirks(struct tulip_pci_softc *,
		    const uint8_t *);

static void	tlp_pci_adaptec_quirks(struct tulip_pci_softc *,
		    const uint8_t *);

static const struct tlp_pci_quirks tlp_pci_21040_quirks[] = {
	{ tlp_pci_znyx_21040_quirks,	{ 0x00, 0xc0, 0x95 } },
	{ tlp_pci_smc_21040_quirks,	{ 0x00, 0x00, 0xc0 } },
	{ tlp_pci_cogent_21040_quirks,	{ 0x00, 0x00, 0x92 } },
	{ tlp_pci_accton_21040_quirks,	{ 0x00, 0x00, 0xe8 } },
	{ NULL,				{ 0, 0, 0 } }
};

static const struct tlp_pci_quirks tlp_pci_21041_quirks[] = {
	{ tlp_pci_dec_quirks,		{ 0x08, 0x00, 0x2b } },
	{ tlp_pci_dec_quirks,		{ 0x00, 0x00, 0xf8 } },
	{ NULL,				{ 0, 0, 0 } }
};

static void	tlp_pci_asante_21140_quirks(struct tulip_pci_softc *,
		    const uint8_t *);
static void	tlp_pci_e100_quirks(struct tulip_pci_softc *,
		    const uint8_t *);
static void	tlp_pci_phobos_21140_quirks(struct tulip_pci_softc *,
		    const uint8_t *);
static void	tlp_pci_smc_21140_quirks(struct tulip_pci_softc *,
		    const uint8_t *);
static void	tlp_pci_vpc_21140_quirks(struct tulip_pci_softc *,
		    const uint8_t *);

static const struct tlp_pci_quirks tlp_pci_21140_quirks[] = {
	{ tlp_pci_dec_quirks,		{ 0x08, 0x00, 0x2b } },
	{ tlp_pci_dec_quirks,		{ 0x00, 0x00, 0xf8 } },
	{ tlp_pci_e100_quirks,		{ 0x00, 0xa0, 0x59 } },
	{ tlp_pci_asante_21140_quirks,	{ 0x00, 0x00, 0x94 } },
	{ tlp_pci_adaptec_quirks,	{ 0x00, 0x00, 0x92 } },
	{ tlp_pci_adaptec_quirks,	{ 0x00, 0x00, 0xd1 } },
	{ tlp_pci_phobos_21140_quirks,	{ 0x00, 0x60, 0xf5 } },
	{ tlp_pci_smc_21140_quirks,	{ 0x00, 0x00, 0xc0 } },
	{ tlp_pci_vpc_21140_quirks,	{ 0x00, 0x03, 0xff } },
	{ NULL,				{ 0, 0, 0 } }
};

static const struct tlp_pci_quirks tlp_pci_21142_quirks[] = {
	{ tlp_pci_dec_quirks,		{ 0x08, 0x00, 0x2b } },
	{ tlp_pci_dec_quirks,		{ 0x00, 0x00, 0xf8 } },
	{ tlp_pci_cobalt_21142_quirks,	{ 0x00, 0x10, 0xe0 } },
	{ tlp_pci_algor_21142_quirks,	{ 0x00, 0x40, 0xbc } },
	{ tlp_pci_adaptec_quirks,	{ 0x00, 0x00, 0xd1 } },
	{ tlp_pci_netwinder_21142_quirks,{ 0x00, 0x10, 0x57 } },
	{ tlp_pci_phobos_21142_quirks,	{ 0x00, 0x60, 0xf5 } },
	{ tlp_pci_znyx_21142_quirks,	{ 0x00, 0xc0, 0x95 } },
	{ NULL,				{ 0, 0, 0 } }
};

static int	tlp_pci_shared_intr(void *);

static const struct tulip_pci_product *
tlp_pci_lookup(const struct pci_attach_args *pa)
{
	const struct tulip_pci_product *tpp;

	/* Don't match lmc cards */
	if (PCI_VENDOR(pci_conf_read(pa->pa_pc, pa->pa_tag,
	    PCI_SUBSYS_ID_REG)) == PCI_VENDOR_LMC)
		return NULL;

	for (tpp = tlp_pci_products; tpp->tpp_chip != TULIP_CHIP_INVALID;
	    tpp++) {
		if (PCI_VENDOR(pa->pa_id) == tpp->tpp_vendor &&
		    PCI_PRODUCT(pa->pa_id) == tpp->tpp_product)
			return tpp;
	}
	return NULL;
}

static void
tlp_pci_get_quirks(struct tulip_pci_softc *psc, const uint8_t *enaddr,
    const struct tlp_pci_quirks *tpq)
{

	for (; tpq->tpq_func != NULL; tpq++) {
		if (tpq->tpq_oui[0] == enaddr[0] &&
		    tpq->tpq_oui[1] == enaddr[1] &&
		    tpq->tpq_oui[2] == enaddr[2]) {
			(*tpq->tpq_func)(psc, enaddr);
			return;
		}
	}
}

static void
tlp_pci_check_slaved(struct tulip_pci_softc *psc, int shared, int slaved)
{
	extern struct cfdriver tlp_cd;
	struct tulip_pci_softc *cur, *best = NULL;
	struct tulip_softc *sc = &psc->sc_tulip;
	int i;

	/*
	 * First of all, find the lowest pcidev numbered device on our
	 * bus marked as shared.  That should be our master.
	 */
	for (i = 0; i < tlp_cd.cd_ndevs; i++) {
		if ((cur = device_lookup_private(&tlp_cd, i)) == NULL)
			continue;
		if (device_parent(cur->sc_tulip.sc_dev) !=
		    device_parent(sc->sc_dev))
			continue;
		if ((cur->sc_flags & shared) == 0)
			continue;
		if (cur == psc)
			continue;
		if (best == NULL ||
		    best->sc_tulip.sc_devno > cur->sc_tulip.sc_devno)
			best = cur;
	}

	if (best != NULL) {
		psc->sc_master = best;
		psc->sc_flags |= (shared | slaved);
	}
}

static int
tlp_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (tlp_pci_lookup(pa) != NULL)
		return 10;	/* beat if_de.c */

	return 0;
}

static void
tlp_pci_attach(device_t parent, device_t self, void *aux)
{
	struct tulip_pci_softc *psc = device_private(self);
	struct tulip_softc *sc = &psc->sc_tulip;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	bus_space_tag_t iot, memt;
	bus_space_handle_t ioh, memh;
	int ioh_valid, memh_valid, i, j;
	const struct tulip_pci_product *tpp;
	prop_data_t ea;
	uint8_t enaddr[ETHER_ADDR_LEN];
	uint32_t val = 0;
	pcireg_t reg;
	int error;
	bus_size_t iosize = 0, memsize = 0;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;
	sc->sc_devno = pa->pa_device;
	psc->sc_pc = pa->pa_pc;
	psc->sc_pcitag = pa->pa_tag;

	LIST_INIT(&psc->sc_intrslaves);

	tpp = tlp_pci_lookup(pa);
	if (tpp == NULL) {
		printf("\n");
		panic("tlp_pci_attach: impossible");
	}
	sc->sc_chip = tpp->tpp_chip;

	/*
	 * By default, Tulip registers are 8 bytes long (4 bytes
	 * followed by a 4 byte pad).
	 */
	sc->sc_regshift = 3;

	/*
	 * No power management hooks.
	 * XXX Maybe we should add some!
	 */
	sc->sc_flags |= TULIPF_ENABLED;

	/*
	 * Get revision info, and set some chip-specific variables.
	 */
	sc->sc_rev = PCI_REVISION(pa->pa_class);
	switch (sc->sc_chip) {
	case TULIP_CHIP_21140:
		if (sc->sc_rev >= 0x20)
			sc->sc_chip = TULIP_CHIP_21140A;
		break;

	case TULIP_CHIP_21142:
		if (sc->sc_rev >= 0x20)
			sc->sc_chip = TULIP_CHIP_21143;
		break;

	case TULIP_CHIP_82C168:
		if (sc->sc_rev >= 0x20)
			sc->sc_chip = TULIP_CHIP_82C169;
		break;

	case TULIP_CHIP_MX98713:
		if (sc->sc_rev >= 0x10)
			sc->sc_chip = TULIP_CHIP_MX98713A;
		break;

	case TULIP_CHIP_MX98715:
		if (sc->sc_rev >= 0x20)
			sc->sc_chip = TULIP_CHIP_MX98715A;
		if (sc->sc_rev >= 0x25)
			sc->sc_chip = TULIP_CHIP_MX98715AEC_X;
		if (sc->sc_rev >= 0x30)
			sc->sc_chip = TULIP_CHIP_MX98725;
		break;

	case TULIP_CHIP_WB89C840F:
		sc->sc_regshift = 2;
		break;

	case TULIP_CHIP_AN985:
		/*
		 * The AN983 and AN985 are very similar, and are
		 * differentiated by a "signature" register that
		 * is like, but not identical, to a PCI ID register.
		 */
		reg = pci_conf_read(pc, pa->pa_tag, 0x80);
		switch (reg) {
		case 0x09811317:
			sc->sc_chip = TULIP_CHIP_AN985;
			break;

		case 0x09851317:
			sc->sc_chip = TULIP_CHIP_AN983;
			break;

		default:
			/* Unknown -- use default. */
			break;
		}
		break;

	case TULIP_CHIP_AX88140:
		if (sc->sc_rev >= 0x10)
			sc->sc_chip = TULIP_CHIP_AX88141;
		break;

	case TULIP_CHIP_DM9102:
		if (sc->sc_rev >= 0x30)
			sc->sc_chip = TULIP_CHIP_DM9102A;
		break;

	default:
		/* Nothing. */
		break;
	}

	aprint_normal(": %s Ethernet, pass %d.%d\n",
	    tlp_chip_name(sc->sc_chip),
	    (sc->sc_rev >> 4) & 0xf, sc->sc_rev & 0xf);

	switch (sc->sc_chip) {
	case TULIP_CHIP_21040:
		if (sc->sc_rev < 0x20) {
			aprint_normal_dev(self,
			    "21040 must be at least pass 2.0\n");
			return;
		}
		break;

	case TULIP_CHIP_21140:
		if (sc->sc_rev < 0x11) {
			aprint_normal_dev(self,
			    "21140 must be at least pass 1.1\n");
			return;
		}
		break;

	default:
		/* Nothing. */
		break;
	}

	/*
	 * Check to see if the device is in power-save mode, and
	 * being it out if necessary.
	 */
	switch (sc->sc_chip) {
	case TULIP_CHIP_21140:
	case TULIP_CHIP_21140A:
	case TULIP_CHIP_21142:
	case TULIP_CHIP_21143:
	case TULIP_CHIP_MX98713A:
	case TULIP_CHIP_MX98715:
	case TULIP_CHIP_MX98715A:
	case TULIP_CHIP_MX98715AEC_X:
	case TULIP_CHIP_MX98725:
	case TULIP_CHIP_DM9102:
	case TULIP_CHIP_DM9102A:
	case TULIP_CHIP_AX88140:
	case TULIP_CHIP_AX88141:
	case TULIP_CHIP_RS7112:
		/*
		 * Clear the "sleep mode" bit in the CFDA register.
		 */
		reg = pci_conf_read(pc, pa->pa_tag, TULIP_PCI_CFDA);
		if (reg & (CFDA_SLEEP|CFDA_SNOOZE))
			pci_conf_write(pc, pa->pa_tag, TULIP_PCI_CFDA,
			    reg & ~(CFDA_SLEEP|CFDA_SNOOZE));
		break;

	default:
		/* Nothing. */
		break;
	}

	/* power up chip */
	if ((error = pci_activate(pa->pa_pc, pa->pa_tag, self,
	    NULL)) && error != EOPNOTSUPP) {
		aprint_error_dev(self, "cannot activate %d\n",
		    error);
		return;
	}

	/*
	 * Map the device.
	 */

	ioh_valid = (pci_mapreg_map(pa, TULIP_PCI_IOBA,
	    PCI_MAPREG_TYPE_IO, 0,
	    &iot, &ioh, NULL, &iosize) == 0);
	memh_valid = (pci_mapreg_map(pa, TULIP_PCI_MMBA,
	    PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &memt, &memh, NULL, &memsize) == 0);
	if (memh_valid) {
		sc->sc_st = memt;
		sc->sc_sh = memh;
		psc->sc_mapsize = memsize;
		if (ioh_valid) {
			bus_space_unmap(iot, ioh, iosize);
			ioh_valid = 0;
		}
	} else if (ioh_valid) {
		sc->sc_st = iot;
		sc->sc_sh = ioh;
		psc->sc_mapsize = iosize;
		if (memh_valid) {
			bus_space_unmap(memt, memh, memsize);
			memh_valid = 0;
		}
	} else {
		aprint_error_dev(self, "unable to map device registers\n");
		goto fail;
	}

	sc->sc_dmat = pa->pa_dmat;

	/*
	 * Make sure bus mastering is enabled.
	 */
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG) |
	    PCI_COMMAND_MASTER_ENABLE);

	/*
	 * Get the cacheline size.
	 */
	sc->sc_cacheline = PCI_CACHELINE(pci_conf_read(pc, pa->pa_tag,
	    PCI_BHLC_REG));

	/*
	 * Get PCI data moving command info.
	 */
	if (pa->pa_flags & PCI_FLAGS_MRL_OKAY)
		sc->sc_flags |= TULIPF_MRL;
	if (pa->pa_flags & PCI_FLAGS_MRM_OKAY)
		sc->sc_flags |= TULIPF_MRM;
	if (pa->pa_flags & PCI_FLAGS_MWI_OKAY)
		sc->sc_flags |= TULIPF_MWI;

	/*
	 * Read the contents of the Ethernet Address ROM/SROM.
	 */
	switch (sc->sc_chip) {
	case TULIP_CHIP_21040:
		sc->sc_srom_addrbits = 6;
		sc->sc_srom = malloc(TULIP_ROM_SIZE(6), M_DEVBUF, M_NOWAIT);
		TULIP_WRITE(sc, CSR_MIIROM, MIIROM_SROMCS);
		for (i = 0; i < TULIP_ROM_SIZE(6); i++) {
			for (j = 0; j < 10000; j++) {
				val = TULIP_READ(sc, CSR_MIIROM);
				if ((val & MIIROM_DN) == 0)
					break;
			}
			sc->sc_srom[i] = val & MIIROM_DATA;
		}
		break;

	case TULIP_CHIP_82C168:
	case TULIP_CHIP_82C169:
	    {
		sc->sc_srom_addrbits = 2;
		sc->sc_srom = malloc(TULIP_ROM_SIZE(2), M_DEVBUF, M_NOWAIT);

		/*
		 * The Lite-On PNIC stores the Ethernet address in
		 * the first 3 words of the EEPROM.  EEPROM access
		 * is not like the other Tulip chips.
		 */
		for (i = 0; i < 6; i += 2) {
			TULIP_WRITE(sc, CSR_PNIC_SROMCTL,
			    PNIC_SROMCTL_READ | (i >> 1));
			for (j = 0; j < 500; j++) {
				delay(2);
				val = TULIP_READ(sc, CSR_MIIROM);
				if ((val & PNIC_MIIROM_BUSY) == 0)
					break;
			}
			if (val & PNIC_MIIROM_BUSY) {
				aprint_error_dev(self, "EEPROM timed out\n");
				goto fail;
			}
			val &= PNIC_MIIROM_DATA;
			sc->sc_srom[i] = val >> 8;
			sc->sc_srom[i + 1] = val & 0xff;
		}
		break;
	    }

	default:
		/*
		 * XXX This isn't quite the right way to do this; we should
		 * XXX be attempting to fetch the mac-addr property in the
		 * XXX bus-agnostic part of the driver independently.  But
		 * XXX that requires a larger change in the SROM handling
		 * XXX logic, and for now we can at least remove a machine-
		 * XXX dependent wart from the PCI front-end.
		 */
		ea = prop_dictionary_get(device_properties(self),
					 "mac-address");
		if (ea != NULL) {
			extern int tlp_srom_debug;
			KASSERT(prop_object_type(ea) == PROP_TYPE_DATA);
			KASSERT(prop_data_size(ea) == ETHER_ADDR_LEN);

			memcpy(enaddr, prop_data_data_nocopy(ea),
			       ETHER_ADDR_LEN);

			sc->sc_srom_addrbits = 6;
			sc->sc_srom = malloc(TULIP_ROM_SIZE(6), M_DEVBUF,
			    M_NOWAIT|M_ZERO);
			memcpy(sc->sc_srom, enaddr, sizeof(enaddr));
			if (tlp_srom_debug) {
				aprint_normal("SROM CONTENTS:");
				for (i = 0; i < TULIP_ROM_SIZE(6); i++) {
					if ((i % 8) == 0)
						aprint_normal("\n\t");
					aprint_normal("0x%02x ", sc->sc_srom[i]);
				}
				aprint_normal("\n");
			}
			break;
		}

		/* Check for a slaved ROM on a multi-port board. */
		tlp_pci_check_slaved(psc, TULIP_PCI_SHAREDROM,
		    TULIP_PCI_SLAVEROM);
		if (psc->sc_flags & TULIP_PCI_SLAVEROM) {
			sc->sc_srom_addrbits =
			    psc->sc_master->sc_tulip.sc_srom_addrbits;
			sc->sc_srom = psc->sc_master->sc_tulip.sc_srom;
			enaddr[5] +=
			    sc->sc_devno - psc->sc_master->sc_tulip.sc_devno;
		}
		else if (tlp_read_srom(sc) == 0)
			goto cant_cope;
		break;
	}

	/*
	 * Deal with chip/board quirks.  This includes setting up
	 * the mediasw, and extracting the Ethernet address from
	 * the rombuf.
	 */
	switch (sc->sc_chip) {
	case TULIP_CHIP_21040:
		/*
		 * Parse the Ethernet Address ROM.
		 */
		if (tlp_parse_old_srom(sc, enaddr) == 0)
			goto cant_cope;


		/*
		 * All 21040 boards start out with the same
		 * media switch.
		 */
		sc->sc_mediasw = &tlp_21040_mediasw;

		/*
		 * Deal with any quirks this board might have.
		 */
		tlp_pci_get_quirks(psc, enaddr, tlp_pci_21040_quirks);
		break;

	case TULIP_CHIP_21041:
		/* Check for new format SROM. */
		if (tlp_isv_srom_enaddr(sc, enaddr) == 0) {
			/*
			 * Not an ISV SROM; try the old DEC Ethernet Address
			 * ROM format.
			 */
			if (tlp_parse_old_srom(sc, enaddr) == 0)
				goto cant_cope;
		}

		/*
		 * All 21041 boards use the same media switch; they all
		 * work basically the same!  Yippee!
		 */
		sc->sc_mediasw = &tlp_21041_mediasw;

		/*
		 * Deal with any quirks this board might have.
		 */
		tlp_pci_get_quirks(psc, enaddr, tlp_pci_21041_quirks);
		break;

	case TULIP_CHIP_21140:
	case TULIP_CHIP_21140A:
		/* Check for new format SROM. */
		if (tlp_isv_srom_enaddr(sc, enaddr) == 0) {
			/*
			 * Not an ISV SROM; try the old DEC Ethernet Address
			 * ROM format.
			 */
			if (tlp_parse_old_srom(sc, enaddr) == 0)
				goto cant_cope;
		} else {
			/*
			 * We start out with the 2114x ISV media switch.
			 * When we search for quirks, we may change to
			 * a different switch.
			 */
			sc->sc_mediasw = &tlp_2114x_isv_mediasw;
		}

		/*
		 * Deal with any quirks this board might have.
		 */
		tlp_pci_get_quirks(psc, enaddr, tlp_pci_21140_quirks);

		/*
		 * Bail out now if we can't deal with this board.
		 */
		if (sc->sc_mediasw == NULL)
			goto cant_cope;
		break;

	case TULIP_CHIP_21142:
	case TULIP_CHIP_21143:
		/* Check for new format SROM. */
		if (tlp_isv_srom_enaddr(sc, enaddr) == 0) {
			/*
			 * Not an ISV SROM; try the old DEC Ethernet Address
			 * ROM format.
			 */
			if (tlp_parse_old_srom(sc, enaddr) == 0) {
				/*
				 * One last try: just copy the address
				 * from offset 20 and try to look
				 * up quirks.
				 */
				memcpy(enaddr, &sc->sc_srom[20],
				    ETHER_ADDR_LEN);
			}
		} else {
			/*
			 * We start out with the 2114x ISV media switch.
			 * When we search for quirks, we may change to
			 * a different switch.
			 */
			sc->sc_mediasw = &tlp_2114x_isv_mediasw;
		}

		/*
		 * Deal with any quirks this board might have.
		 */
		tlp_pci_get_quirks(psc, enaddr, tlp_pci_21142_quirks);

		/*
		 * Bail out now if we can't deal with this board.
		 */
		if (sc->sc_mediasw == NULL)
			goto cant_cope;
		break;

	case TULIP_CHIP_82C168:
	case TULIP_CHIP_82C169:
		/*
		 * Lite-On PNIC's Ethernet address is the first 6
		 * bytes of its EEPROM.
		 */
		memcpy(enaddr, sc->sc_srom, ETHER_ADDR_LEN);

		/*
		 * Lite-On PNICs always use the same mediasw; we
		 * select MII vs. internal NWAY automatically.
		 */
		sc->sc_mediasw = &tlp_pnic_mediasw;
		break;

	case TULIP_CHIP_MX98713:
		/*
		 * The Macronix MX98713 has an MII and GPIO, but no
		 * internal Nway block.  This chip is basically a
		 * perfect 21140A clone, with the exception of the
		 * a magic register frobbing in order to make the
		 * interface function.
		 */
		if (tlp_isv_srom_enaddr(sc, enaddr)) {
			sc->sc_mediasw = &tlp_2114x_isv_mediasw;
			break;
		}
		/* FALLTHROUGH */

	case TULIP_CHIP_82C115:
		/*
		 * Yippee!  The Lite-On 82C115 is a clone of
		 * the MX98725 (the data sheet even says `MXIC'
		 * on it)!  Imagine that, a clone of a clone.
		 *
		 * The differences are really minimal:
		 *
		 *	- Wake-On-LAN support
		 *	- 128-bit multicast hash table, rather than
		 *	  the standard 512-bit hash table
		 */
		/* FALLTHROUGH */

	case TULIP_CHIP_MX98713A:
	case TULIP_CHIP_MX98715A:
	case TULIP_CHIP_MX98715AEC_X:
	case TULIP_CHIP_MX98725:
		/*
		 * The MX98713A has an MII as well as an internal Nway block,
		 * but no GPIO.  The MX98715 and MX98725 have an internal
		 * Nway block only.
		 *
		 * The internal Nway block, unlike the Lite-On PNIC's, does
		 * just that - performs Nway.  Once autonegotiation completes,
		 * we must program the GPR media information into the chip.
		 *
		 * The byte offset of the Ethernet address is stored at
		 * offset 0x70.
		 */
		memcpy(enaddr, &sc->sc_srom[sc->sc_srom[0x70]], ETHER_ADDR_LEN);
		sc->sc_mediasw = &tlp_pmac_mediasw;
		break;

	case TULIP_CHIP_WB89C840F:
		/*
		 * Winbond 89C840F's Ethernet address is the first
		 * 6 bytes of its EEPROM.
		 */
		memcpy(enaddr, sc->sc_srom, ETHER_ADDR_LEN);

		/*
		 * Winbond 89C840F has an MII attached to the SIO.
		 */
		sc->sc_mediasw = &tlp_sio_mii_mediasw;
		break;

	case TULIP_CHIP_AL981:
		/*
		 * The ADMtek AL981's Ethernet address is located
		 * at offset 8 of its EEPROM.
		 */
		memcpy(enaddr, &sc->sc_srom[8], ETHER_ADDR_LEN);

		/*
		 * ADMtek AL981 has a built-in PHY accessed through
		 * special registers.
		 */
		sc->sc_mediasw = &tlp_al981_mediasw;
		break;

	case TULIP_CHIP_AN983:
	case TULIP_CHIP_AN985:
		/*
		 * The ADMtek AN985's Ethernet address is located
		 * at offset 8 of its EEPROM.
		 */
		memcpy(enaddr, &sc->sc_srom[8], ETHER_ADDR_LEN);

		/*
		 * The ADMtek AN985 can be configured in Single-Chip
		 * mode or MAC-only mode.  Single-Chip uses the built-in
		 * PHY, MAC-only has an external PHY (usually HomePNA).
		 * The selection is based on an EEPROM setting, and both
		 * PHYs are accessed via MII attached to SIO.
		 *
		 * The AN985 "ghosts" the internal PHY onto all
		 * MII addresses, so we have to use a media init
		 * routine that limits the search.
		 * XXX How does this work with MAC-only mode?
		 */
		sc->sc_mediasw = &tlp_an985_mediasw;
		break;

	case TULIP_CHIP_DM9102:
	case TULIP_CHIP_DM9102A:
		/*
		 * Some boards with the Davicom chip have an ISV
		 * SROM (mostly DM9102A boards -- trying to describe
		 * the HomePNA PHY, probably) although the data in
		 * them is generally wrong.  Check for ISV format
		 * and grab the Ethernet address that way, and if
		 * that fails, fall back on grabbing it from an
		 * observed offset of 20 (which is where it would
		 * be in an ISV SROM anyhow, tho ISV can cope with
		 * multi-port boards).
		 */
		if (!tlp_isv_srom_enaddr(sc, enaddr)) {

			prop_data_t eaddrprop;

			eaddrprop = prop_dictionary_get(
				device_properties(self), "mac-address");

			if (eaddrprop != NULL
			    && prop_data_size(eaddrprop) == ETHER_ADDR_LEN)
				memcpy(enaddr,
				    prop_data_data_nocopy(eaddrprop),
				    ETHER_ADDR_LEN);
			else
				memcpy(enaddr, &sc->sc_srom[20],
				    ETHER_ADDR_LEN);
		}

		/*
		 * Davicom chips all have an internal MII interface
		 * and a built-in PHY.  DM9102A also has a an external
		 * MII interface, usually with a HomePNA PHY attached
		 * to it.
		 */
		sc->sc_mediasw = &tlp_dm9102_mediasw;
		break;

	case TULIP_CHIP_AX88140:
	case TULIP_CHIP_AX88141:
		/*
		 * ASIX AX88140/AX88141 Ethernet Address is located at offset
		 * 20 of the SROM.
		 */
		memcpy(enaddr, &sc->sc_srom[20], ETHER_ADDR_LEN);

		/*
		 * ASIX AX88140A/AX88141 chip can have a built-in PHY or
		 * an external MII interface.
		 */
		sc->sc_mediasw = &tlp_asix_mediasw;
		break;

	case TULIP_CHIP_RS7112:
		/*
		 * RS7112 Ethernet Address is located of offset 0x19a
		 * of the SROM
		 */
		memcpy(enaddr, &sc->sc_srom[0x19a], ETHER_ADDR_LEN);

		/* RS7112 chip has a PHY at MII address 1 */
		sc->sc_mediasw = &tlp_rs7112_mediasw;
		break;

	default:
 cant_cope:
		aprint_error_dev(self, "sorry, unable to handle your board\n");
		goto fail;
	}

	/*
	 * Handle shared interrupts.
	 */
	if (psc->sc_flags & TULIP_PCI_SHAREDINTR) {
		if (psc->sc_master)
			psc->sc_flags |= TULIP_PCI_SLAVEINTR;
		else {
			tlp_pci_check_slaved(psc, TULIP_PCI_SHAREDINTR,
			    TULIP_PCI_SLAVEINTR);
			if (psc->sc_master == NULL)
				psc->sc_master = psc;
		}
		LIST_INSERT_HEAD(&psc->sc_master->sc_intrslaves,
		    psc, sc_intrq);
	}

	if (psc->sc_flags & TULIP_PCI_SLAVEINTR) {
		aprint_normal_dev(self, "sharing interrupt with %s\n",
		    device_xname(psc->sc_master->sc_tulip.sc_dev));
	} else {
		/*
		 * Map and establish our interrupt.
		 */
		if (pci_intr_map(pa, &ih)) {
			aprint_error_dev(self, "unable to map interrupt\n");
			goto fail;
		}
		intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
		psc->sc_ih = pci_intr_establish(pc, ih, IPL_NET,
		    (psc->sc_flags & TULIP_PCI_SHAREDINTR) ?
		    tlp_pci_shared_intr : tlp_intr, sc);
		if (psc->sc_ih == NULL) {
			aprint_error_dev(self, "unable to establish interrupt");
			if (intrstr != NULL)
				aprint_error(" at %s", intrstr);
			aprint_error("\n");
			goto fail;
		}
		aprint_normal_dev(self, "interrupting at %s\n",
		    intrstr);
	}

	/*
	 * Finish off the attach.
	 */
	error = tlp_attach(sc, enaddr);
	if (error)
		goto fail;
	return;

fail:
	if (psc->sc_ih != NULL) {
		pci_intr_disestablish(psc->sc_pc, psc->sc_ih);
		psc->sc_ih = NULL;
	}

	if (ioh_valid)
		bus_space_unmap(iot, ioh, iosize);
	if (memh_valid)
		bus_space_unmap(memt, memh, memsize);
	psc->sc_mapsize = 0;
	return;
}

static int
tlp_pci_detach(device_t self, int flags)
{
	struct tulip_pci_softc *psc = device_private(self);
	struct tulip_softc *sc = &psc->sc_tulip;
	int rv;

	rv = tlp_detach(sc);
	if (rv)
		return rv;

	if (psc->sc_ih != NULL) {
		pci_intr_disestablish(psc->sc_pc, psc->sc_ih);
		psc->sc_ih = NULL;
	}

	if (psc->sc_mapsize) {
		bus_space_unmap(sc->sc_st, sc->sc_sh, psc->sc_mapsize);
		psc->sc_mapsize = 0;
	}

	return 0;
}

static int
tlp_pci_shared_intr(void *arg)
{
	struct tulip_pci_softc *master = arg, *slave;
	int rv = 0;

	for (slave = LIST_FIRST(&master->sc_intrslaves);
	     slave != NULL;
	     slave = LIST_NEXT(slave, sc_intrq))
		rv |= tlp_intr(&slave->sc_tulip);

	return rv;
}

static void
tlp_pci_dec_quirks(struct tulip_pci_softc *psc, const uint8_t *enaddr)
{
	struct tulip_softc *sc = &psc->sc_tulip;

	/*
	 * This isn't really a quirk-gathering device, really.  We
	 * just want to get the spiffy DEC board name from the SROM.
	 */
	strcpy(sc->sc_name, "DEC ");

	if (memcmp(&sc->sc_srom[29], "DE500", 5) == 0 ||
	    memcmp(&sc->sc_srom[29], "DE450", 5) == 0)
		memcpy(&sc->sc_name[4], &sc->sc_srom[29], 8);
	else
		sc->sc_name[3] = '\0';
}

static void
tlp_pci_znyx_21040_quirks(struct tulip_pci_softc *psc, const uint8_t *enaddr)
{
	struct tulip_softc *sc = &psc->sc_tulip;
	uint16_t id = 0;

	/*
	 * If we have a slaved ROM, just copy the bits from the master.
	 * This is in case we fail the ROM ID check (older boards) and
	 * need to fall back on Ethernet address model checking; that
	 * will fail for slave chips.
	 */
	if (psc->sc_flags & TULIP_PCI_SLAVEROM) {
		strcpy(sc->sc_name, psc->sc_master->sc_tulip.sc_name);
		sc->sc_mediasw = psc->sc_master->sc_tulip.sc_mediasw;
		psc->sc_flags |=
		    psc->sc_master->sc_flags & TULIP_PCI_SHAREDINTR;
		return;
	}

	if (sc->sc_srom[32] == 0x4a && sc->sc_srom[33] == 0x52) {
		id = sc->sc_srom[37] | (sc->sc_srom[36] << 8);
		switch (id) {
 zx312:
		case 0x0602:	/* ZX312 */
			strcpy(sc->sc_name, "ZNYX ZX312");
			return;

		case 0x0622:	/* ZX312T */
			strcpy(sc->sc_name, "ZNYX ZX312T");
			sc->sc_mediasw = &tlp_21040_tp_mediasw;
			return;

 zx314_inta:
		case 0x0701:	/* ZX314 INTA */
			psc->sc_flags |= TULIP_PCI_SHAREDINTR;
			/* FALLTHROUGH */
		case 0x0711:	/* ZX314 */
			strcpy(sc->sc_name, "ZNYX ZX314");
			psc->sc_flags |= TULIP_PCI_SHAREDROM;
			sc->sc_mediasw = &tlp_21040_tp_mediasw;
			return;

 zx315_inta:
		case 0x0801:	/* ZX315 INTA */
			psc->sc_flags |= TULIP_PCI_SHAREDINTR;
			/* FALLTHROUGH */
		case 0x0811:	/* ZX315 */
			strcpy(sc->sc_name, "ZNYX ZX315");
			psc->sc_flags |= TULIP_PCI_SHAREDROM;
			return;

		default:
			id = 0;
			break;
		}
	}

	/*
	 * Deal with boards that have broken ROMs.
	 */
	if (id == 0) {
		if ((enaddr[3] & ~3) == 0xf0 && (enaddr[5] & 3) == 0x00)
			goto zx314_inta;
		if ((enaddr[3] & ~3) == 0xf4 && (enaddr[5] & 1) == 0x00)
			goto zx315_inta;
		if ((enaddr[3] & ~3) == 0xec)
			goto zx312;
	}

	strcpy(sc->sc_name, "ZNYX ZX31x");
}

static void	tlp_pci_znyx_21142_qs6611_reset(struct tulip_softc *);

static void
tlp_pci_znyx_21142_quirks(struct tulip_pci_softc *psc, const uint8_t *enaddr)
{
	struct tulip_softc *sc = &psc->sc_tulip;
	pcireg_t subid;

	subid = pci_conf_read(psc->sc_pc, psc->sc_pcitag, PCI_SUBSYS_ID_REG);

	if (PCI_VENDOR(subid) != PCI_VENDOR_ZNYX)
		return;		/* ? */

	switch (PCI_PRODUCT(subid) & 0xff) {
	/*
	 * ZNYX 21143 boards with QS6611 PHY
	 */
	case 0x12:	/* ZX345Q */
	case 0x13:	/* ZX346Q */
	case 0x14:	/* ZX348Q */
	case 0x18:	/* ZX414 */
	case 0x19:	/* ZX412 */
	case 0x1a:	/* ZX444 */
	case 0x1b:	/* ZX442 */
	case 0x23:	/* ZX212 */
	case 0x24:	/* ZX214 */
	case 0x29:	/* ZX374 */
	case 0x2d:	/* ZX372 */
	case 0x2b:	/* ZX244 */
	case 0x2c:	/* ZX424 */
	case 0x2e:	/* ZX422 */
		aprint_normal_dev(sc->sc_dev, "QS6611 PHY\n");
		sc->sc_reset = tlp_pci_znyx_21142_qs6611_reset;
		break;
	}
}

static void
tlp_pci_znyx_21142_qs6611_reset(struct tulip_softc *sc)
{

	/*
	 * Reset QS6611 PHY.
	 */
	TULIP_WRITE(sc, CSR_SIAGEN,
	    SIAGEN_CWE | SIAGEN_LGS1 | SIAGEN_ABM | (0xf << 16));
	delay(200);
	TULIP_WRITE(sc, CSR_SIAGEN, (0x4 << 16));
	delay(10000);
}

static void
tlp_pci_smc_21040_quirks(struct tulip_pci_softc *psc, const uint8_t *enaddr)
{
	struct tulip_softc *sc = &psc->sc_tulip;
	uint16_t id1, id2, ei;
	int auibnc = 0, utp = 0;
	char *cp;

	id1 = sc->sc_srom[0x60] | (sc->sc_srom[0x61] << 8);
	id2 = sc->sc_srom[0x62] | (sc->sc_srom[0x63] << 8);
	ei  = sc->sc_srom[0x66] | (sc->sc_srom[0x67] << 8);

	strcpy(sc->sc_name, "SMC 8432");
	cp = &sc->sc_name[8];

	if ((id1 & 1) == 0) {
		*cp++ = 'B';
		auibnc = 1;
	}
	if ((id1 & 0xff) > 0x32) {
		*cp++ = 'T';
		utp = 1;
	}
	if ((id1 & 0x4000) == 0) {
		*cp++ = 'A';
		auibnc = 1;
	}
	if (id2 == 0x15) {
		sc->sc_name[7] = '4';
		*cp++ = '-';
		*cp++ = 'C';
		*cp++ = 'H';
		*cp++ = ei ? '2' : '1';
	}
	*cp = '\0';

	if (utp != 0 && auibnc == 0)
		sc->sc_mediasw = &tlp_21040_tp_mediasw;
	else if (utp == 0 && auibnc != 0)
		sc->sc_mediasw = &tlp_21040_auibnc_mediasw;
}

static void
tlp_pci_cogent_21040_quirks(struct tulip_pci_softc *psc, const uint8_t *enaddr)
{

	strcpy(psc->sc_tulip.sc_name, "Cogent multi-port");
	psc->sc_flags |= TULIP_PCI_SHAREDINTR|TULIP_PCI_SHAREDROM;
}

static void
tlp_pci_accton_21040_quirks(struct tulip_pci_softc *psc, const uint8_t *enaddr)
{

	strcpy(psc->sc_tulip.sc_name, "ACCTON EN1203");
}

static void	tlp_pci_asante_21140_reset(struct tulip_softc *);

static void
tlp_pci_asante_21140_quirks(struct tulip_pci_softc *psc, const uint8_t *enaddr)
{
	struct tulip_softc *sc = &psc->sc_tulip;

	/*
	 * Some Asante boards don't use the ISV SROM format.  For
	 * those that don't, we initialize the GPIO direction bits,
	 * and provide our own reset hook, which resets the MII.
	 *
	 * All of these boards use SIO-attached-MII media.
	 */
	if (sc->sc_mediasw == &tlp_2114x_isv_mediasw)
		return;

	strcpy(sc->sc_name, "Asante");

	sc->sc_gp_dir = 0xbf;
	sc->sc_reset = tlp_pci_asante_21140_reset;
	sc->sc_mediasw = &tlp_sio_mii_mediasw;
}

static void
tlp_pci_e100_quirks(struct tulip_pci_softc *psc, const uint8_t *enaddr)
{
	struct tulip_softc *sc = &psc->sc_tulip;

	if (sc->sc_mediasw == &tlp_2114x_isv_mediasw)
		return;

	strcpy(sc->sc_name, "UMAX E100");

	sc->sc_gp_dir = 0xbf;
	sc->sc_reset = tlp_pci_asante_21140_reset;
	sc->sc_mediasw = &tlp_sio_mii_mediasw;
}

static void
tlp_pci_asante_21140_reset(struct tulip_softc *sc)
{

	TULIP_WRITE(sc, CSR_GPP, GPP_GPC | sc->sc_gp_dir);
	TULIP_WRITE(sc, CSR_GPP, 0x8);
	delay(100);
	TULIP_WRITE(sc, CSR_GPP, 0);
}

static void	tlp_pci_phobos_21140_reset(struct tulip_softc *);

static void
tlp_pci_phobos_21140_quirks(struct tulip_pci_softc *psc, const uint8_t *enaddr)
{
	struct tulip_softc *sc = &psc->sc_tulip;

	/*
	 * Phobos boards just use MII-on-SIO.
	 */
	sc->sc_mediasw = &tlp_sio_mii_mediasw;
	sc->sc_reset = tlp_pci_phobos_21140_reset;

	/*
	 * These boards appear solely on sgimips machines behind a special
	 * GIO<->PCI ASIC and require the DBO and BLE bits to be set in CSR0.
	 */
	sc->sc_flags |= (TULIPF_BLE | TULIPF_DBO);
}

static void
tlp_pci_phobos_21140_reset(struct tulip_softc *sc)
{

	TULIP_WRITE(sc, CSR_GPP, GPP_GPC | 0xfd);
	delay(10);
	TULIP_WRITE(sc, CSR_GPP, 0xfd);
	delay(10);
	TULIP_WRITE(sc, CSR_GPP, 0);
}

/*
 * SMC 9332DST media switch.
 */
static void	tlp_smc9332dst_tmsw_init(struct tulip_softc *);

static const struct tulip_mediasw tlp_smc9332dst_mediasw = {
	tlp_smc9332dst_tmsw_init,
	tlp_21140_gpio_get,
	tlp_21140_gpio_set
};

static void
tlp_pci_smc_21140_quirks(struct tulip_pci_softc *psc, const uint8_t *enaddr)
{
	struct tulip_softc *sc = &psc->sc_tulip;

	strcpy(psc->sc_tulip.sc_name, "SMC 9332DST");
	sc->sc_mediasw = &tlp_smc9332dst_mediasw;
}

static void
tlp_smc9332dst_tmsw_init(struct tulip_softc *sc)
{
	struct tulip_21x4x_media *tm;
	const char *sep = "";
	uint32_t reg;
	int i, cnt;

	sc->sc_gp_dir = GPP_SMC9332DST_PINS;
	sc->sc_opmode = OPMODE_MBO | OPMODE_PS;
	TULIP_WRITE(sc, CSR_OPMODE, sc->sc_opmode);

	ifmedia_init(&sc->sc_mii.mii_media, 0, tlp_mediachange,
	    tlp_mediastatus);
	aprint_normal_dev(sc->sc_dev, "");

#define	ADD(m, c) \
	tm = malloc(sizeof(*tm), M_DEVBUF, M_WAITOK|M_ZERO);		\
	tm->tm_opmode = (c);						\
	tm->tm_gpdata = GPP_SMC9332DST_INIT;				\
	ifmedia_add(&sc->sc_mii.mii_media, (m), 0, tm)
#define	PRINT(str)	aprint_normal("%s%s", sep, str); sep = ", "

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, 0, 0), OPMODE_TTM);
	PRINT("10baseT");

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, IFM_FDX, 0),
	    OPMODE_TTM | OPMODE_FD);
	PRINT("10baseT-FDX");

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, 0, 0),
	    OPMODE_PS | OPMODE_PCS | OPMODE_SCR);
	PRINT("100baseTX");

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_FDX, 0),
	    OPMODE_PS | OPMODE_PCS | OPMODE_SCR | OPMODE_FD);
	PRINT("100baseTX-FDX");

#undef ADD
#undef PRINT

	aprint_normal("\n");

	tlp_reset(sc);
	TULIP_WRITE(sc, CSR_OPMODE, sc->sc_opmode | OPMODE_PCS | OPMODE_SCR);
	TULIP_WRITE(sc, CSR_GPP, GPP_GPC | sc->sc_gp_dir);
	delay(10);
	TULIP_WRITE(sc, CSR_GPP, GPP_SMC9332DST_INIT);
	delay(200000);
	cnt = 0;
	for (i = 1000; i > 0; i--) {
		reg = TULIP_READ(sc, CSR_GPP);
		if ((~reg & (GPP_SMC9332DST_OK10 |
			     GPP_SMC9332DST_OK100)) == 0) {
			if (cnt++ > 100) {
				break;
			}
		} else if ((reg & GPP_SMC9332DST_OK10) == 0) {
			break;
		} else {
			cnt = 0;
		}
		delay(1000);
	}
	if (cnt > 100) {
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_100_TX);
	} else {
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_10_T);
	}
}

static void
tlp_pci_vpc_21140_quirks(struct tulip_pci_softc *psc, const uint8_t *enaddr)
{
	struct tulip_softc *sc = &psc->sc_tulip;
	char *p1 = (char *) &sc->sc_srom[32];
	char *p2 = &sc->sc_name[0];

	do {
		if ((unsigned char) *p1 & 0x80)
			*p2++ = ' ';
		else
			*p2++ = *p1;
	} while (*p1++);
}

static void	tlp_pci_cobalt_21142_reset(struct tulip_softc *);

static void
tlp_pci_cobalt_21142_quirks(struct tulip_pci_softc *psc, const uint8_t *enaddr)
{
	struct tulip_softc *sc = &psc->sc_tulip;

	/*
	 * Cobalt Networks interfaces are just MII-on-SIO.
	 */
	sc->sc_reset = tlp_pci_cobalt_21142_reset;
	sc->sc_mediasw = &tlp_sio_mii_mediasw;

	/*
	 * The Cobalt systems tend to fall back to store-and-forward
	 * pretty quickly, so we select that from the beginning to
	 * avoid initial timeouts.
	 */
	sc->sc_txthresh = TXTH_SF;
}

static void
tlp_pci_cobalt_21142_reset(struct tulip_softc *sc)
{

	/*
	 * Reset PHY.
	 */
	TULIP_WRITE(sc, CSR_SIAGEN, SIAGEN_CWE | (1 << 16));
	delay(10);
	TULIP_WRITE(sc, CSR_SIAGEN, SIAGEN_CWE);
	delay(10);
}

static void
tlp_pci_algor_21142_quirks(struct tulip_pci_softc *psc, const uint8_t *enaddr)
{
	struct tulip_softc *sc = &psc->sc_tulip;

	/*
	 * Algorithmics boards just have MII-on-SIO.
	 *
	 * XXX They also have AUI on the serial interface.
	 * XXX Deal with this.
	 */
	sc->sc_mediasw = &tlp_sio_mii_mediasw;
}

/*
 * Cogent EM1x0 (aka. Adaptec ANA-6910) media switch.
 */
static void	tlp_cogent_em1x0_tmsw_init(struct tulip_softc *);

static const struct tulip_mediasw tlp_cogent_em1x0_mediasw = {
	tlp_cogent_em1x0_tmsw_init,
	tlp_21140_gpio_get,
	tlp_21140_gpio_set
};

static void
tlp_pci_adaptec_quirks(struct tulip_pci_softc *psc, const uint8_t *enaddr)
{
	struct tulip_softc *sc = &psc->sc_tulip;
	uint8_t *srom = sc->sc_srom, id0;
	uint16_t id1, id2;

	if (sc->sc_mediasw == NULL) {
		id0 = srom[32];
		switch (id0) {
		case 0x12:
			strcpy(psc->sc_tulip.sc_name, "Cogent EM100TX");
			sc->sc_mediasw = &tlp_cogent_em1x0_mediasw;
			break;

		case 0x13:
			strcpy(psc->sc_tulip.sc_name, "Cogent ???");
 			sc->sc_mediasw = &tlp_cogent_em1x0_mediasw;
			psc->sc_flags |= TULIP_PCI_SHAREDINTR |
			    TULIP_PCI_SHAREDROM;
			break;

		case 0x15:
			strcpy(psc->sc_tulip.sc_name, "Cogent EM100FX");
			sc->sc_mediasw = &tlp_cogent_em1x0_mediasw;
			break;

#if 0
		case XXX:
			strcpy(psc->sc_tulip.sc_name, "Cogent EM110TX");
			sc->sc_mediasw = &tlp_cogent_em1x0_mediasw;
			break;
#endif

		default:
			printf("%s: unknown Cogent board ID 0x%02x\n",
			    device_xname(sc->sc_dev), id0);
		}
		return;
	}

	id1 = TULIP_ROM_GETW(srom, 0);
	id2 = TULIP_ROM_GETW(srom, 2);
	if (id1 != 0x1109) {
		goto unknown;
	}

	switch (id2) {
	case 0x1900:
		strcpy(psc->sc_tulip.sc_name, "Adaptec ANA-6911");
		break;

	case 0x2400:
		strcpy(psc->sc_tulip.sc_name, "Adaptec ANA-6944A");
		psc->sc_flags |= TULIP_PCI_SHAREDINTR|TULIP_PCI_SHAREDROM;
		break;

	case 0x2b00:
		strcpy(psc->sc_tulip.sc_name, "Adaptec ANA-6911A");
		break;

	case 0x3000:
		strcpy(psc->sc_tulip.sc_name, "Adaptec ANA-6922");
		psc->sc_flags |= TULIP_PCI_SHAREDINTR|TULIP_PCI_SHAREDROM;
		break;

	default:
 unknown:
		printf("%s: unknown Adaptec/Cogent board ID 0x%04x/0x%04x\n",
		    device_xname(sc->sc_dev), id1, id2);
	}
}

static void
tlp_cogent_em1x0_tmsw_init(struct tulip_softc *sc)
{
	struct tulip_21x4x_media *tm;
	const char *sep = "";

	sc->sc_gp_dir = GPP_COGENT_EM1x0_PINS;
	sc->sc_opmode = OPMODE_MBO | OPMODE_PS;
	TULIP_WRITE(sc, CSR_OPMODE, sc->sc_opmode);

	ifmedia_init(&sc->sc_mii.mii_media, 0, tlp_mediachange,
	    tlp_mediastatus);
	aprint_normal_dev(sc->sc_dev, "");

#define	ADD(m, c) \
	tm = malloc(sizeof(*tm), M_DEVBUF, M_WAITOK|M_ZERO);		\
	tm->tm_opmode = (c);						\
	tm->tm_gpdata = GPP_COGENT_EM1x0_INIT;				\
	ifmedia_add(&sc->sc_mii.mii_media, (m), 0, tm)
#define	PRINT(str)	aprint_normal("%s%s", sep, str); sep = ", "

	if (sc->sc_srom[32] == 0x15) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_FX, 0, 0),
		    OPMODE_PS | OPMODE_PCS);
		PRINT("100baseFX");

		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_FX, IFM_FDX, 0),
		    OPMODE_PS | OPMODE_PCS | OPMODE_FD);
		PRINT("100baseFX-FDX");
		aprint_normal("\n");

		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_100_FX);
	} else {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, 0, 0),
		    OPMODE_PS | OPMODE_PCS | OPMODE_SCR);
		PRINT("100baseTX");

		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_FX, IFM_FDX, 0),
		    OPMODE_PS | OPMODE_PCS | OPMODE_SCR | OPMODE_FD);
		PRINT("100baseTX-FDX");
		aprint_normal("\n");

		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_100_TX);
	}

#undef ADD
#undef PRINT
}

static void	tlp_pci_netwinder_21142_reset(struct tulip_softc *);

static void
tlp_pci_netwinder_21142_quirks(struct tulip_pci_softc *psc,
    const uint8_t *enaddr)
{
	struct tulip_softc *sc = &psc->sc_tulip;

	/*
	 * Netwinders just use MII-on-SIO.
	 */
	sc->sc_mediasw = &tlp_sio_mii_mediasw;
	sc->sc_reset = tlp_pci_netwinder_21142_reset;
}

void
tlp_pci_netwinder_21142_reset(struct tulip_softc *sc)
{

	/*
	 * Reset the PHY.
	 */
	TULIP_WRITE(sc, CSR_SIAGEN, 0x0821 << 16);
	delay(10);
	TULIP_WRITE(sc, CSR_SIAGEN, 0x0000 << 16);
	delay(10);
	TULIP_WRITE(sc, CSR_SIAGEN, 0x0001 << 16);
	delay(10);
}

static void	tlp_pci_phobos_21142_reset(struct tulip_softc *);

static void
tlp_pci_phobos_21142_quirks(struct tulip_pci_softc *psc, const uint8_t *enaddr)
{
	struct tulip_softc *sc = &psc->sc_tulip;

	/*
	 * Phobos boards just use MII-on-SIO.
	 */
	sc->sc_mediasw = &tlp_sio_mii_mediasw;
	sc->sc_reset = tlp_pci_phobos_21142_reset;

	/*
	 * These boards appear solely on sgimips machines behind a special
	 * GIO<->PCI ASIC and require the DBO and BLE bits to be set in CSR0.
	 */
	sc->sc_flags |= (TULIPF_BLE | TULIPF_DBO);
}

static void
tlp_pci_phobos_21142_reset(struct tulip_softc *sc)
{
	/*
	 * Reset PHY.
	 */
	TULIP_WRITE(sc, CSR_SIAGEN, (0x880f << 16));
	delay(10);
	TULIP_WRITE(sc, CSR_SIAGEN, (0x800f << 16));
	delay(10);
}

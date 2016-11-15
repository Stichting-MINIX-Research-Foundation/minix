/*	$NetBSD: mvsata_pci.c,v 1.8 2014/03/29 19:28:25 christos Exp $	*/
/*
 * Copyright (c) 2008 KIYOHARA Takashi
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mvsata_pci.c,v 1.8 2014/03/29 19:28:25 christos Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/pmf.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>

#include <dev/ic/mvsatareg.h>
#include <dev/ic/mvsatavar.h>

#define MVSATA_PCI_HCARBITER_SPACE_OFFSET	0x20000

#define MVSATA_PCI_COMMAND	0x00c00
#define MVSATA_PCI_COMMAND_MWRITECOMBINE	(1 << 4)
#define MVSATA_PCI_COMMAND_MREADCOMBINE		(1 << 5)
#define MVSATA_PCI_SERRMASK	0x00c28
#define MVSATA_PCI_MSITRIGGER	0x00c38
#define MVSATA_PCI_MODE		0x00d00
#define MVSATA_PCI_DISCTIMER	0x00d04
#define MVSATA_PCI_EROMBAR	0x00d2c
#define MVSATA_PCI_MAINCS	0x00d30
#define MVSATA_PCI_MAINCS_SPM		(1 << 2)	/* stop pci master */
#define MVSATA_PCI_MAINCS_PME		(1 << 3)	/* pci master empty */
#define MVSATA_PCI_MAINCS_GSR		(1 << 4)	/* glab soft reset */
#define MVSATA_PCI_E_IRQCAUSE	0x01900
#define MVSATA_PCI_E_IRQMASK	0x01910
#define MVSATA_PCI_XBARTIMEOUT	0x01d04
#define MVSATA_PCI_ERRLOWADDR	0x01d40
#define MVSATA_PCI_ERRHIGHADDR	0x01d44
#define MVSATA_PCI_ERRATTRIBUTE	0x01d48
#define MVSATA_PCI_ERRCOMMAND	0x01d50
#define MVSATA_PCI_IRQCAUSE	0x01d58
#define MVSATA_PCI_IRQMASK	0x01d5c
#define MVSATA_PCI_MAINIRQCAUSE	0x01d60
#define MVSATA_PCI_MAINIRQMASK	0x01d64
#define MVSATA_PCI_MAINIRQ_SATAERR(hc, port) \
					(1 << (((port) << 1) + (hc) * 9))
#define MVSATA_PCI_MAINIRQ_SATADONE(hc, port) \
					(1 << (((port) << 1) + (hc) * 9 + 1))
#define MVSATA_PCI_MAINIRQ_SATACOALDONE(hc)	(1 << ((hc) * 9 + 8))
#define MVSATA_PCI_MAINIRQ_PCI		(1 << 18)
#define MVSATA_PCI_FLASHCTL	0x1046c
#define MVSATA_PCI_GPIOPORTCTL	0x104f0
#define MVSATA_PCI_RESETCFG	0x180d8

#define MVSATA_PCI_DEV(psc)	(psc->psc_sc.sc_wdcdev.sc_atac.atac_dev)


struct mvsata_pci_softc {
	struct mvsata_softc psc_sc;

	pci_chipset_tag_t psc_pc;
	pcitag_t psc_tag;

	bus_space_tag_t psc_iot;
	bus_space_handle_t psc_ioh;

	void *psc_ih;
};


static int  mvsata_pci_match(device_t, struct cfdata *, void *);
static void mvsata_pci_attach(device_t, device_t, void *);
static int mvsata_pci_detach(device_t, int);

static int mvsata_pci_intr(void *);
static bool mvsata_pci_resume(device_t, const pmf_qual_t *qual);

static int mvsata_pci_sreset(struct mvsata_softc *);
static int mvsata_pci_misc_reset(struct mvsata_softc *);
static void mvsata_pci_enable_intr(struct mvsata_port *, int);


CFATTACH_DECL_NEW(mvsata_pci, sizeof(struct mvsata_pci_softc),
    mvsata_pci_match, mvsata_pci_attach, mvsata_pci_detach, NULL);

struct mvsata_product mvsata_pci_products[] = {
#define PCI_VP(v, p)	PCI_VENDOR_ ## v, PCI_PRODUCT_ ## v ## _ ## p
	{ PCI_VP(MARVELL, 88SX5040),		1, 4, gen1, 0 },
	{ PCI_VP(MARVELL, 88SX5041),		1, 4, gen1, 0 },
	{ PCI_VP(MARVELL, 88SX5080),		2, 4, gen1, 0 },
	{ PCI_VP(MARVELL, 88SX5081),		2, 4, gen1, 0 },
	{ PCI_VP(MARVELL, 88SX6040),		1, 4, gen2, 0 },
	{ PCI_VP(MARVELL, 88SX6041),		1, 4, gen2, 0 },
	{ PCI_VP(ADP2, 1420SA),			1, 4, gen2, 0 }, /* 88SX6041 */
	{ PCI_VP(MARVELL, 88SX6042),		1, 4, gen2e, 0 },
	{ PCI_VP(MARVELL, 88SX6080),		2, 4, gen2, MVSATA_FLAGS_PCIE },
	{ PCI_VP(MARVELL, 88SX6081),		2, 4, gen2, MVSATA_FLAGS_PCIE },
	{ PCI_VP(MARVELL, 88SX7042),		1, 4, gen2e, 0 },
	{ PCI_VP(ADP2, 1430SA),			1, 4, gen2e, 0 }, /* 88SX7042 */
	{ PCI_VP(TRIONES, ROCKETRAID_2310),	1, 4, gen2e, 0 },
#undef PCI_VP
};


/*
 * mvsata_pci_match()
 *    This function returns 2, because mvsata is high priority more than pciide.
 */
static int
mvsata_pci_match(device_t parent, struct cfdata *match, void *aux)
{
	struct pci_attach_args *pa = aux;
	int i;

	for (i = 0; i < __arraycount(mvsata_pci_products); i++)
		if (PCI_VENDOR(pa->pa_id) == mvsata_pci_products[i].vendor &&
		    PCI_PRODUCT(pa->pa_id) == mvsata_pci_products[i].model)
			return 2;
	return 0;
}

static void
mvsata_pci_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct mvsata_pci_softc *psc = device_private(self);
	struct mvsata_softc *sc = &psc->psc_sc;
	pci_intr_handle_t intrhandle;
	pcireg_t csr;
	bus_size_t size;
	uint32_t reg, mask;
	int read_pre_amps, hc, port, rv, i;
	const char *intrstr;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_wdcdev.sc_atac.atac_dev = self;
	sc->sc_model = PCI_PRODUCT(pa->pa_id);
	sc->sc_rev = PCI_REVISION(pa->pa_class);
	sc->sc_dmat = pa->pa_dmat;
	sc->sc_enable_intr = mvsata_pci_enable_intr;

	pci_aprint_devinfo(pa, "Marvell Serial-ATA Host Controller");
	
	/* Map I/O register */
	if (pci_mapreg_map(pa, PCI_MAPREG_START,
	    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &psc->psc_iot, &psc->psc_ioh, NULL, &size) != 0) {
		aprint_error_dev(self, "can't map registers\n");
		return;
	}
	psc->psc_pc = pa->pa_pc;
	psc->psc_tag = pa->pa_tag;

	if (bus_space_subregion(psc->psc_iot, psc->psc_ioh,
	    MVSATA_PCI_HCARBITER_SPACE_OFFSET,
	    size - MVSATA_PCI_HCARBITER_SPACE_OFFSET, &sc->sc_ioh)) {
		aprint_error_dev(self, "can't subregion registers\n");
		return;
	}
	sc->sc_iot = psc->psc_iot;

	/* Enable device */
	csr = pci_conf_read(psc->psc_pc, psc->psc_tag, PCI_COMMAND_STATUS_REG);
	csr |= PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(psc->psc_pc, psc->psc_tag, PCI_COMMAND_STATUS_REG, csr);

	if (pci_intr_map(pa, &intrhandle) != 0) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(psc->psc_pc, intrhandle, intrbuf, sizeof(intrbuf));
	psc->psc_ih = pci_intr_establish(psc->psc_pc, intrhandle, IPL_BIO,
	    mvsata_pci_intr, sc);
	if (psc->psc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n",
	    intrstr ? intrstr : "unknown interrupt");

	/*
	 * Check if TWSI serial ROM initialization was triggered.
	 * If so, then PRE/AMP configuration probably are set after
	 * reset by serial ROM. If not then override the PRE/AMP
	 * values.
	 */
	reg = bus_space_read_4(psc->psc_iot, psc->psc_ioh, MVSATA_PCI_RESETCFG);
	read_pre_amps = (reg & 0x00000001) ? 1 : 0;

	for (i = 0; i < __arraycount(mvsata_pci_products); i++)
		if (PCI_VENDOR(pa->pa_id) == mvsata_pci_products[i].vendor &&
		    PCI_PRODUCT(pa->pa_id) == mvsata_pci_products[i].model)
			break;
	KASSERT(i < __arraycount(mvsata_pci_products));

	rv = mvsata_attach(sc, &mvsata_pci_products[i],
	    mvsata_pci_sreset, mvsata_pci_misc_reset, read_pre_amps);
	if (rv != 0) {
		pci_intr_disestablish(psc->psc_pc, psc->psc_ih);
		return;
	}

	mask = MVSATA_PCI_MAINIRQ_PCI;
	for (hc = 0; hc < sc->sc_hc; hc++)
		for (port = 0; port < sc->sc_port; port++)
			mask |=
			    MVSATA_PCI_MAINIRQ_SATAERR(hc, port) |
			    MVSATA_PCI_MAINIRQ_SATADONE(hc, port);
	bus_space_write_4(psc->psc_iot, psc->psc_ioh, MVSATA_PCI_MAINIRQMASK,
	    mask);

	if (!pmf_device_register(self, NULL, mvsata_pci_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static int
mvsata_pci_detach(device_t self, int flags)
{
	struct mvsata_pci_softc *psc = device_private(self);

/* XXXX: needs reset ? */

	pci_intr_disestablish(psc->psc_pc, psc->psc_ih);
	pmf_device_deregister(self);
	return 0;
}

static int
mvsata_pci_intr(void *arg)
{
	struct mvsata_pci_softc *psc = (struct mvsata_pci_softc *)arg;
	struct mvsata_softc *sc = &psc->psc_sc;
	uint32_t cause;
	int hc, port, handled = 0;

	cause = bus_space_read_4(psc->psc_iot, psc->psc_ioh,
	    MVSATA_PCI_MAINIRQCAUSE);
	for (hc = 0; hc < sc->sc_hc; hc++)
		for (port = 0; port < sc->sc_port; port++)
			if (cause & MVSATA_PCI_MAINIRQ_SATAERR(hc, port)) {
				struct mvsata_port *mvport;

				mvport = sc->sc_hcs[hc].hc_ports[port];
				handled |= mvsata_error(mvport);
			}
	for (hc = 0; hc < sc->sc_hc; hc++)
		if (cause &
		    (MVSATA_PCI_MAINIRQ_SATADONE(hc, 0) |
		     MVSATA_PCI_MAINIRQ_SATADONE(hc, 1) |
		     MVSATA_PCI_MAINIRQ_SATADONE(hc, 2) |
		     MVSATA_PCI_MAINIRQ_SATADONE(hc, 3)))
			handled |= mvsata_intr(&sc->sc_hcs[hc]);

	if (cause & MVSATA_PCI_MAINIRQ_PCI) {
		uint32_t pe_cause;

		if (sc->sc_flags & MVSATA_FLAGS_PCIE) {
			pe_cause = bus_space_read_4(psc->psc_iot, psc->psc_ioh,
			    MVSATA_PCI_E_IRQCAUSE);
			aprint_error_dev(MVSATA_PCI_DEV(psc),
			    "PCIe error: 0x%x\n", pe_cause);
			bus_space_write_4(psc->psc_iot, psc->psc_ioh,
			    MVSATA_PCI_E_IRQCAUSE, ~pe_cause);
		} else {
			pe_cause = bus_space_read_4(psc->psc_iot, psc->psc_ioh,
			    MVSATA_PCI_IRQCAUSE);
			aprint_error_dev(MVSATA_PCI_DEV(psc),
			    "PCI error: 0x%x\n", pe_cause);
			bus_space_write_4(psc->psc_iot, psc->psc_ioh,
			    MVSATA_PCI_IRQCAUSE, ~pe_cause);
		}

		handled = 1;	/* XXXXX */
	}

	return handled;
}

static bool
mvsata_pci_resume(device_t dev, const pmf_qual_t *qual)
{

	/* not yet... */

	return true;
}


static int
mvsata_pci_sreset(struct mvsata_softc *sc)
{
	struct mvsata_pci_softc *psc = (struct mvsata_pci_softc *)sc;
	uint32_t val;
	int i;

	val = bus_space_read_4(psc->psc_iot, psc->psc_ioh, MVSATA_PCI_MAINCS);
	val |= MVSATA_PCI_MAINCS_SPM;
	bus_space_write_4(psc->psc_iot, psc->psc_ioh, MVSATA_PCI_MAINCS, val);

	for (i = 0; i < 1000; i++) {
		delay(1);
		val = bus_space_read_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_MAINCS);
		if (val & MVSATA_PCI_MAINCS_PME)
			break;
	}
	if (!(val & MVSATA_PCI_MAINCS_PME)) {
		aprint_error_dev(MVSATA_PCI_DEV(psc),
		    "PCI master won't flush\n");
		return -1;
	}

	/* reset */
	bus_space_write_4(psc->psc_iot, psc->psc_ioh, MVSATA_PCI_MAINCS,
	    val | MVSATA_PCI_MAINCS_GSR);
	val = bus_space_read_4(psc->psc_iot, psc->psc_ioh, MVSATA_PCI_MAINCS);
	delay(5);
	if (!(val & MVSATA_PCI_MAINCS_GSR)) {
		aprint_error_dev(MVSATA_PCI_DEV(psc),
		    "can't set global reset\n");
		return -1;
	}

	/* clear reset and *reenable the PCI master* (not mentioned in spec) */
	val &= ~(MVSATA_PCI_MAINCS_GSR | MVSATA_PCI_MAINCS_SPM);
	bus_space_write_4(psc->psc_iot, psc->psc_ioh, MVSATA_PCI_MAINCS, val);
	val = bus_space_read_4(psc->psc_iot, psc->psc_ioh, MVSATA_PCI_MAINCS);
	delay(5);
	if (val & MVSATA_PCI_MAINCS_GSR) {
		aprint_error_dev(MVSATA_PCI_DEV(psc),
		    "can't set global reset\n");
		return -1;
	}

	return 0;
}

static int
mvsata_pci_misc_reset(struct mvsata_softc *sc)
{
	struct mvsata_pci_softc *psc = (struct mvsata_pci_softc *)sc;
#define MVSATA_PCI_COMMAND_DEFAULT			0x0107e371
#define MVSATA_PCI_COMMAND_PCI_CONVENTIONAL_ONLY	0x800003e0
	uint32_t val, pci_command = MVSATA_PCI_COMMAND_DEFAULT;

	bus_space_write_4(psc->psc_iot, psc->psc_ioh, MVSATA_PCI_FLASHCTL,
	    0x0fcfffff);

	if (sc->sc_gen == gen2 || sc->sc_gen == gen2e) {
		val = bus_space_read_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_GPIOPORTCTL);
		val &= 0x3;
#if 0
		val |= 0x00000060;
#else	/* XXXX */
		val |= 0x00000070;
#endif
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_GPIOPORTCTL, val);
	}

	if (sc->sc_gen == gen1) {
		/* Expansion ROM BAR Enable */
		val = bus_space_read_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_EROMBAR);
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_EROMBAR, val | 0x00000001);
	}

	if (sc->sc_flags & MVSATA_FLAGS_PCIE) {
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_MAINIRQMASK, 0);
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_E_IRQCAUSE, 0);
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_E_IRQMASK, 0);
	} else {
		val = bus_space_read_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_MODE);
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_MODE, val & 0xff00ffff);
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_DISCTIMER, 0);
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_MSITRIGGER, 0);
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_XBARTIMEOUT, 0x000100ff);
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_MAINIRQMASK, 0);
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_SERRMASK, 0);
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_IRQCAUSE, 0);
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_IRQMASK, 0);
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_ERRLOWADDR, 0);
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_ERRHIGHADDR, 0);
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_ERRATTRIBUTE, 0);
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_ERRCOMMAND, 0);
	}

	/* Enable LED */
	if (sc->sc_gen == gen1) {
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_GPIOPORTCTL, 0);

/* XXXX: 50xxB2 errata ? */
#if 0
		if (sc->sc_rev == 3) {
			int port;

			val = bus_space_read_4(psc->psc_iot, psc->psc_ioh,
			    MVSATA_PCI_GPIOPORTCTL);

			/* XXXX: check HDD connected  */

			bus_space_write_4(psc->psc_iot, psc->psc_ioh,
			    MVSATA_PCI_GPIOPORTCTL, val);
		}
#endif

		/* Disable Flash controller clock */
		val = bus_space_read_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_EROMBAR);
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_EROMBAR, val & ~0x00000001);
	} else
#if 0
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_GPIOPORTCTL, 0x00000060);
#else	/* XXXX */
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_GPIOPORTCTL, 0x00000070);
#endif

	if (sc->sc_flags & MVSATA_FLAGS_PCIE)
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_E_IRQMASK, 0x0000070a);
	else {
		val = bus_space_read_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_MODE);
		if ((val & 0x30) >> 4) {	/* PCI-X */
			int mv60x1b2 =
			    ((sc->sc_model == PCI_PRODUCT_MARVELL_88SX6041 ||
			    sc->sc_model == PCI_PRODUCT_MARVELL_88SX6081) &&
			    sc->sc_rev == 7);

			pci_command &=
			    ~MVSATA_PCI_COMMAND_PCI_CONVENTIONAL_ONLY;
			if (sc->sc_gen == gen1 || mv60x1b2)
				pci_command &=
				    ~MVSATA_PCI_COMMAND_MWRITECOMBINE;
		} else
			if (sc->sc_gen == gen1)
				pci_command &=
				    ~(MVSATA_PCI_COMMAND_MWRITECOMBINE |
				    MVSATA_PCI_COMMAND_MREADCOMBINE);
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_COMMAND, pci_command);

#define MVSATA_PCI_INTERRUPT_MASK	0x00d77fe6
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_SERRMASK, MVSATA_PCI_INTERRUPT_MASK);
		bus_space_write_4(psc->psc_iot, psc->psc_ioh,
		    MVSATA_PCI_IRQMASK, MVSATA_PCI_INTERRUPT_MASK);
	}

	return 0;
}

static void
mvsata_pci_enable_intr(struct mvsata_port *mvport, int on)
{
	struct mvsata_pci_softc *psc =
	    device_private(mvport->port_ata_channel.ch_atac->atac_dev);
	uint32_t mask;
	int hc = mvport->port_hc->hc, port = mvport->port;

	mask = bus_space_read_4(psc->psc_iot, psc->psc_ioh,
	    MVSATA_PCI_MAINIRQMASK);
	if (on)
		mask |= MVSATA_PCI_MAINIRQ_SATADONE(hc, port);
	else
		mask &= ~MVSATA_PCI_MAINIRQ_SATADONE(hc, port);
	bus_space_write_4(psc->psc_iot, psc->psc_ioh, MVSATA_PCI_MAINIRQMASK,
	    mask);
}

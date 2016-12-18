/*	$NetBSD: if_fpa.c,v 1.60 2015/01/25 07:33:24 martin Exp $	*/

/*-
 * Copyright (c) 1995, 1996 Matt Thomas <matt@3am-software.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
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
 *
 * Id: if_fpa.c,v 1.11 1997/06/05 01:56:35 thomas Exp
 *
 */

/*
 * DEC PDQ FDDI Controller; code for BSD derived operating systems
 *
 *   This module supports the DEC DEFPA PCI FDDI Controller
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_fpa.c,v 1.60 2015/01/25 07:33:24 martin Exp $");

#ifdef __NetBSD__
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#if defined(__FreeBSD__) && BSD < 199401
#include <sys/devconf.h>
#elif defined(__bsdi__) || defined(__NetBSD__)
#include <sys/device.h>
#endif

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <net/bpf.h>
#include <net/bpfdesc.h>

#if defined(__FreeBSD__)
#include <netinet/if_fddi.h>
#else
#include <net/if_fddi.h>
#endif

#if defined(__FreeBSD__)
#include <vm/vm.h>
#include "fpa.h"
#include <netinet/if_ether.h>
#include <pci/pcivar.h>
#include <i386/isa/icu.h>
#include <dev/pdq/pdqvar.h>
#include <dev/pdq/pdqreg.h>
#elif defined(__bsdi__)
#include <netinet/if_ether.h>
#include <i386/isa/isavar.h>
#include <i386/isa/icu.h>
#ifndef DRQNONE
#define	DRQNONE		0
#endif
#if _BSDI_VERSION < 199401
#define IRQSHARE	0
#endif
#include <i386/pci/pci.h>
#include <dev/pdq/pdqvar.h>
#include <dev/pdq/pdqreg.h>
#elif defined(__NetBSD__)
#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>
#include <dev/ic/pdqvar.h>
#include <dev/ic/pdqreg.h>
#endif /* __NetBSD__ */


#define	DEC_VENDORID		0x1011
#define	DEFPA_CHIPID		0x000F
#define	PCI_VENDORID(x)		((x) & 0xFFFF)
#define	PCI_CHIPID(x)		(((x) >> 16) & 0xFFFF)

#define	DEFPA_LATENCY	0x88

#define	PCI_CFLT	0x0C	/* Configuration Latency */
#define	PCI_CBMA	0x10	/* Configuration Base Memory Address */
#define	PCI_CBIO	0x14	/* Configuration Base I/O Address */

#if defined(__FreeBSD__)
#if NFPA < 4
#undef NFPA
#define NFPA	4
#endif
static pdq_softc_t *pdqs_pci[NFPA];
#define	PDQ_PCI_UNIT_TO_SOFTC(unit)	(pdqs_pci[unit])
#if BSD >= 199506
#define	pdq_pci_ifwatchdog		NULL
static void pdq_pci_shutdown(int howto, void *sc);
#endif

#elif defined(__bsdi__)
extern struct cfdriver fpacd;
#define	PDQ_PCI_UNIT_TO_SOFTC(unit)	((pdq_softc_t *)device_lookup_private(&fpa_cd, unit))

#elif defined(__NetBSD__)
extern struct cfdriver fpa_cd;
#define	PDQ_PCI_UNIT_TO_SOFTC(unit)	((pdq_softc_t *)device_lookup_private(&fpa_cd, unit))
#define	pdq_pci_ifwatchdog		NULL
#endif

#ifndef pdq_pci_ifwatchdog
static ifnet_ret_t
pdq_pci_ifwatchdog(
    int unit)
{
    pdq_ifwatchdog(&PDQ_PCI_UNIT_TO_SOFTC(unit)->sc_if);
}
#endif

#if defined(__FreeBSD__) && BSD >= 199506
static void
pdq_pci_ifintr(
    void *arg)
{
    (void) pdq_interrupt(((pdq_softc_t *) arg)->sc_pdq);
}
#else
static int
pdq_pci_ifintr(
    void *arg)
{
    pdq_softc_t * const sc = (pdq_softc_t *) arg;
#ifdef __FreeBSD__
    return pdq_interrupt(sc->sc_pdq);
#elif defined(__bsdi__) || defined(__NetBSD__)
    (void) pdq_interrupt(sc->sc_pdq);
    return 1;
#endif
}
#endif /* __FreeBSD && BSD */

#if defined(__FreeBSD__)
/*
 * This is the PCI configuration support.  Since the PDQ is available
 * on both EISA and PCI boards, one must be careful in how defines the
 * PDQ in the config file.
 */
static char *
pdq_pci_probe(
    pcici_t config_id,
    pcidi_t device_id)
{
    if (PCI_VENDORID(device_id) == DEC_VENDORID &&
	    PCI_CHIPID(device_id) == DEFPA_CHIPID)
	return "Digital DEFPA PCI FDDI Controller";
    return NULL;
}

static void
pdq_pci_attach(
    pcici_t config_id,
    int unit)
{
    pdq_softc_t *sc;
    vaddr_t va_csrs;
    paddr_t pa_csrs;
    pdq_uint32_t data;

    if (unit == NFPA) {
	printf("fpa%d: not configured; kernel is built for only %d device%s.\n",
	       unit, NFPA, NFPA == 1 ? "" : "s");
	return;
    }

    data = pci_conf_read(config_id, PCI_CFLT);
    if ((data & 0xFF00) < (DEFPA_LATENCY << 8)) {
	data &= ~0xFF00;
	data |= DEFPA_LATENCY << 8;
	pci_conf_write(config_id, PCI_CFLT, data);
    }

    sc = (pdq_softc_t *) malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT|M_ZERO);
    if (sc == NULL)
	return;

    if (!pci_map_mem(config_id, PCI_CBMA, &va_csrs, &pa_csrs)) {
	free((void *) sc, M_DEVBUF);
	return;
    }

    sc->sc_if.if_name = "fpa";
    sc->sc_if.if_unit = unit;
    sc->sc_membase = (pdq_bus_memaddr_t) va_csrs;
    sc->sc_pdq = pdq_initialize(PDQ_BUS_PCI, sc->sc_membase,
				sc->sc_if.if_name, sc->sc_if.if_unit,
				(void *) sc, PDQ_DEFPA);
    if (sc->sc_pdq == NULL) {
	free((void *) sc, M_DEVBUF);
	return;
    }
    memcpy(sc->sc_ac.ac_enaddr, (void *) sc->sc_pdq->pdq_hwaddr.lanaddr_bytes, 6);
    pdqs_pci[unit] = sc;
    pdq_ifattach(sc, pdq_pci_ifwatchdog);
    pci_map_int(config_id, pdq_pci_ifintr, (void*) sc, &net_imask);
#if BSD >= 199506
    at_shutdown(pdq_pci_shutdown, (void *) sc, SHUTDOWN_POST_SYNC);
#endif
}

#if BSD < 199401
static int
pdq_pci_shutdown(
    struct kern_devconf *kdc,
    int force)
{
    if (kdc->kdc_unit < NFPA)
	pdq_hwreset(PDQ_PCI_UNIT_TO_SOFTC(kdc->kdc_unit)->sc_pdq);
    (void) dev_detach(kdc);
    return 0;
}
#else
static void
pdq_pci_shutdown(
    int howto,
    void *sc)
{
    pdq_hwreset(((pdq_softc_t *)sc)->sc_pdq);
}
#endif

static u_long pdq_pci_count;

struct pci_device fpadevice = {
    "fpa",
    pdq_pci_probe,
    pdq_pci_attach,
    &pdq_pci_count,
#if BSD < 199401
    pdq_pci_shutdown,
#endif
};

#ifdef DATA_SET
DATA_SET (pcidevice_set, fpadevice);
#endif
#elif defined(__bsdi__)

static int
pdq_pci_match(
    pci_devaddr_t *pa)
{
    int irq;
    int id;

    id = pci_inl(pa, PCI_VENDOR_ID);
    if (PCI_VENDORID(id) != DEC_VENDORID || PCI_CHIPID(id) != DEFPA_CHIPID)
	return 0;

    irq = pci_inl(pa, PCI_I_LINE) & 0xFF;
    if (irq == 0 || irq >= 16)
	return 0;

    return 1;
}

int
pdq_pci_probe(device_t parent, cfdata_t cf, void *aux)
{
    struct isa_attach_args *ia = (struct isa_attach_args *) aux;
    pdq_uint32_t irq, data;
    pci_devaddr_t *pa;

    pa = pci_scan(pdq_pci_match);
    if (pa == NULL)
	return 0;

    irq = (1 << (pci_inl(pa, PCI_I_LINE) & 0xFF));

    if (ia->ia_irq != IRQUNK && irq != ia->ia_irq) {
	printf("fpa%d: error: desired IRQ of %d does not match device's actual IRQ of %d\n",
	       cf->cf_unit,
	       ffs(ia->ia_irq) - 1, ffs(irq) - 1);
	return 0;
    }
    if (ia->ia_irq == IRQUNK) {
	(void) isa_irqalloc(irq);
	ia->ia_irq = irq;
    }

    /* PCI bus masters don't use host DMA channels */
    ia->ia_drq = DRQNONE;

    /* Get the memory base address; assume the BIOS set it up correctly */
    ia->ia_maddr = (void *) (pci_inl(pa, PCI_CBMA) & ~7);
    pci_outl(pa, PCI_CBMA, 0xFFFFFFFF);
    ia->ia_msize = ((~pci_inl(pa, PCI_CBMA)) | 7) + 1;
    pci_outl(pa, PCI_CBMA, (int) ia->ia_maddr);

    /* Disable I/O space access */
    pci_outl(pa, PCI_COMMAND, pci_inl(pa, PCI_COMMAND) & ~1);
    ia->ia_iobase = 0;
    ia->ia_iosize = 0;

    /* Make sure the latency timer is what the DEFPA likes */
    data = pci_inl(pa, PCI_CFLT);
    if ((data & 0xFF00) < (DEFPA_LATENCY << 8)) {
	data &= ~0xFF00;
	data |= DEFPA_LATENCY << 8;
	pci_outl(pa, PCI_CFLT, data);
    }
    ia->ia_irq |= IRQSHARE;

    return 1;
}

void
pdq_pci_attach(device_t parent, device_t self, void *aux)
{
    pdq_softc_t *sc = device_private(self);
    struct isa_attach_args *ia = (struct isa_attach_args *) aux;
    struct ifnet *ifp = &sc->sc_if;
    int i;

    sc->sc_dev = self;
    sc->sc_if.if_unit = sc->sc_dev.dv_unit;
    sc->sc_if.if_name = "fpa";
    sc->sc_if.if_flags = 0;
    sc->sc_membase = (pdq_bus_memaddr_t) mapphys((vaddr_t)ia->ia_maddr, ia->ia_msize);

    sc->sc_pdq = pdq_initialize(PDQ_BUS_PCI, sc->sc_membase,
				sc->sc_if.if_name, sc->sc_if.if_unit,
				(void *) sc, PDQ_DEFPA);
    if (sc->sc_pdq == NULL) {
	printf("fpa%d: initialization failed\n", sc->sc_if.if_unit);
	return;
    }

    memcpy(sc->sc_ac.ac_enaddr, (void *) sc->sc_pdq->pdq_hwaddr.lanaddr_bytes, 6);

    pdq_ifattach(sc, pdq_pci_ifwatchdog);

    isa_establish(&sc->sc_id, &sc->sc_dev);

    sc->sc_ih.ih_fun = pdq_pci_ifintr;
    sc->sc_ih.ih_arg = (void *)sc;
    intr_establish(ia->ia_irq, &sc->sc_ih, DV_NET);

    sc->sc_ats.func = (void (*)(void *)) pdq_hwreset;
    sc->sc_ats.arg = (void *) sc->sc_pdq;
    atshutdown(&sc->sc_ats, ATSH_ADD);
}

struct cfdriver fpacd = {
    0, "fpa", pdq_pci_probe, pdq_pci_attach,
#if _BSDI_VERSION >= 199401
    DV_IFNET,
#endif
    sizeof(pdq_softc_t)
};

#elif defined(__NetBSD__)

static int
pdq_pci_match(device_t parent, cfdata_t match, void *aux)
{
    struct pci_attach_args *pa = (struct pci_attach_args *) aux;

    if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_DEC)
	return 0;
    if (PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_DEC_DEFPA)
	return 0;

    return 1;
}

static void
pdq_pci_attach(device_t const parent, device_t const self, void *const aux)
{
    pdq_softc_t * const sc = device_private(self);
    struct pci_attach_args * const pa = (struct pci_attach_args *) aux;
    pdq_uint32_t data;
    pci_intr_handle_t intrhandle;
    const char *intrstr;
    bus_space_tag_t iot, memt;
    bus_space_handle_t ioh, memh;
    int ioh_valid, memh_valid;
    char intrbuf[PCI_INTRSTR_LEN];

    aprint_naive(": FDDI controller\n");

    sc->sc_dev = self;

    data = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CFLT);
    if ((data & 0xFF00) < (DEFPA_LATENCY << 8)) {
	data &= ~0xFF00;
	data |= DEFPA_LATENCY << 8;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_CFLT, data);
    }

    strlcpy(sc->sc_if.if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
    sc->sc_if.if_flags = 0;
    sc->sc_if.if_softc = sc;

    ioh_valid = (pci_mapreg_map(pa, PCI_CBIO, PCI_MAPREG_TYPE_IO, 0,
		 &iot, &ioh, NULL, NULL) == 0);
    memh_valid = (pci_mapreg_map(pa, PCI_CBMA,
		  PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0,
		  &memt, &memh, NULL, NULL) == 0);

#if defined(DEFPA_IOMAPED)
    if (ioh_valid) {
	sc->sc_csrtag = iot;
	sc->sc_membase = ioh;
    } else if (memh_valid) {
	sc->sc_csrtag = memt;
	sc->sc_membase = memh;
    }
#else /* defined(DEFPA_IOMAPPED) */
    if (memh_valid) {
	sc->sc_csrtag = memt;
	sc->sc_membase = memh;
    } else if (ioh_valid) {
	sc->sc_csrtag = iot;
	sc->sc_membase = ioh;
    }
#endif /* DEFPA_IOMAPPED */
    else {
        aprint_error(": unable to map device registers\n");
        return;
    }

    sc->sc_dmatag = pa->pa_dmat;

    /* Make sure bus mastering is enabled. */
    pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
		   pci_conf_read(pa->pa_pc, pa->pa_tag,
				 PCI_COMMAND_STATUS_REG) |
		   PCI_COMMAND_MASTER_ENABLE);

    sc->sc_pdq = pdq_initialize(sc->sc_csrtag, sc->sc_membase,
				sc->sc_if.if_xname, 0,
				(void *) sc, PDQ_DEFPA);
    if (sc->sc_pdq == NULL) {
	aprint_error_dev(sc->sc_dev, "initialization failed\n");
	return;
    }

    pdq_ifattach(sc, pdq_pci_ifwatchdog);

    if (pci_intr_map(pa, &intrhandle)) {
	aprint_error_dev(self, "couldn't map interrupt\n");
	return;
    }
    intrstr = pci_intr_string(pa->pa_pc, intrhandle, intrbuf, sizeof(intrbuf));
    sc->sc_ih = pci_intr_establish(pa->pa_pc, intrhandle, IPL_NET, pdq_pci_ifintr, sc);
    if (sc->sc_ih == NULL) {
	aprint_error_dev(self, "couldn't establish interrupt");
	if (intrstr != NULL)
	    aprint_error(" at %s", intrstr);
	aprint_error("\n");
	return;
    }

    sc->sc_ats = shutdownhook_establish((void (*)(void *)) pdq_hwreset, sc->sc_pdq);
    if (sc->sc_ats == NULL)
	aprint_error_dev(self, "warning: couldn't establish shutdown hook\n");
    if (intrstr != NULL)
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);
}

CFATTACH_DECL_NEW(fpa, sizeof(pdq_softc_t),
    pdq_pci_match, pdq_pci_attach, NULL, NULL);

#endif /* __NetBSD__ */

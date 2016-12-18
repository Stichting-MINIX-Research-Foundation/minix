/*	$NetBSD: if_fea.c,v 1.46 2014/03/29 19:28:24 christos Exp $	*/

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
 * Id: if_fea.c,v 1.8 1997/03/21 13:45:45 thomas Exp
 */

/*
 * DEC PDQ FDDI Controller
 *
 *	This module support the DEFEA EISA FDDI Controller.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_fea.c,v 1.46 2014/03/29 19:28:24 christos Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#if defined(__FreeBSD__)
#include <sys/devconf.h>
#elif defined(__bsdi__) || defined(__NetBSD__)
#include <sys/device.h>
#endif

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#if defined(__FreeBSD__)
#include <netinet/if_fddi.h>
#else
#include <net/if_fddi.h>
#endif

#if defined(__FreeBSD__)
#include <netinet/if_ether.h>
#include <i386/eisa/eisaconf.h>
#include <i386/isa/icu.h>
#include <dev/pdq/pdqvar.h>
#include <dev/pdq/pdqreg.h>
#elif defined(__bsdi__)
#include <netinet/if_ether.h>
#include <i386/isa/isa.h>
#include <i386/isa/icu.h>
#include <i386/isa/dma.h>
#include <i386/isa/isavar.h>
#include <i386/eisa/eisa.h>
#include <dev/pdq/pdqvar.h>
#include <dev/pdq/pdqreg.h>
#elif defined(__NetBSD__)
#include <sys/cpu.h>
#include <sys/bus.h>

#include <dev/ic/pdqvar.h>
#include <dev/ic/pdqreg.h>

#include <dev/eisa/eisareg.h>
#include <dev/eisa/eisavar.h>
#include <dev/eisa/eisadevs.h>
#endif

/*
 *
 */

#define	DEFEA_IRQS		0x0000FBA9U

#if defined(__FreeBSD__)
static pdq_softc_t *pdqs_eisa[16];
#define	PDQ_EISA_UNIT_TO_SOFTC(unit)	(pdqs_eisa[unit])
#define	DEFEA_INTRENABLE		0x8	/* level interrupt */
#define	pdq_eisa_ifwatchdog		NULL
#define	DEFEA_DECODE_IRQ(n)		((DEFEA_IRQS >> ((n) << 2)) & 0x0f)

#elif defined(__bsdi__)
extern struct cfdriver feacd;
#define	PDQ_EISA_UNIT_TO_SOFTC(unit)	((pdq_softc_t *)device_lookup_private(&feacd, unit))
#define	DEFEA_INTRENABLE		0x28	/* edge interrupt */
static const int pdq_eisa_irqs[4] = { IRQ9, IRQ10, IRQ11, IRQ15 };
#define	DEFEA_DECODE_IRQ(n)		(pdq_eisa_irqs[(n)])

#elif defined(__NetBSD__)
#define	DEFEA_INTRENABLE		0x8	/* level interrupt */
#define	pdq_eisa_ifwatchdog		NULL
#define	DEFEA_DECODE_IRQ(n)		((DEFEA_IRQS >> ((n) << 2)) & 0x0f)

#else
#error unknown system
#endif

#ifndef pdq_eisa_ifwatchdog
static ifnet_ret_t
pdq_eisa_ifwatchdog(
    int unit)
{
    pdq_ifwatchdog(&PDQ_EISA_UNIT_TO_SOFTC(unit)->sc_if);
}
#endif

static void
pdq_eisa_subprobe(
    pdq_bus_t bc,
    pdq_bus_ioport_t iobase,
    pdq_uint32_t *maddr,
    pdq_uint32_t *msiz,
    pdq_uint32_t *irq)
{
    if (irq != NULL)
	*irq = DEFEA_DECODE_IRQ(PDQ_OS_IORD_8(bc, iobase, PDQ_EISA_IO_CONFIG_STAT_0) & 3);
    *maddr = (PDQ_OS_IORD_8(bc, iobase, PDQ_EISA_MEM_ADD_CMP_0) << 8)
	| (PDQ_OS_IORD_8(bc, iobase, PDQ_EISA_MEM_ADD_CMP_1) << 16);
    *msiz = (PDQ_OS_IORD_8(bc, iobase, PDQ_EISA_MEM_ADD_MASK_0) + 4) << 8;
}

static void
pdq_eisa_devinit(
    pdq_softc_t *sc)
{
    pdq_uint8_t data;

    /*
     * Do the standard initialization for the DEFEA registers.
     */
    PDQ_OS_IOWR_8(sc->sc_iotag, sc->sc_iobase, PDQ_EISA_FUNCTION_CTRL, 0x23);
    PDQ_OS_IOWR_8(sc->sc_iotag, sc->sc_iobase, PDQ_EISA_IO_CMP_1_1, (sc->sc_iobase >> 8) & 0xF0);
    PDQ_OS_IOWR_8(sc->sc_iotag, sc->sc_iobase, PDQ_EISA_IO_CMP_0_1, (sc->sc_iobase >> 8) & 0xF0);
    PDQ_OS_IOWR_8(sc->sc_iotag, sc->sc_iobase, PDQ_EISA_SLOT_CTRL, 0x01);
    data = PDQ_OS_IORD_8(sc->sc_iotag, sc->sc_iobase, PDQ_EISA_BURST_HOLDOFF);
#if defined(__NetBSD__)
    PDQ_OS_IOWR_8(sc->sc_iotag, sc->sc_iobase, PDQ_EISA_BURST_HOLDOFF,
		  !sc->sc_csr_memmapped ? data & ~1 : data | 1);
#elif defined(PDQ_IOMAPPED)
    PDQ_OS_IOWR_8(sc->sc_iotag, sc->sc_iobase, PDQ_EISA_BURST_HOLDOFF, data & ~1);
#else
    PDQ_OS_IOWR_8(sc->sc_iotag, sc->sc_iobase, PDQ_EISA_BURST_HOLDOFF, data | 1);
#endif
    data = PDQ_OS_IORD_8(sc->sc_iotag, sc->sc_iobase, PDQ_EISA_IO_CONFIG_STAT_0);
    PDQ_OS_IOWR_8(sc->sc_iotag, sc->sc_iobase, PDQ_EISA_IO_CONFIG_STAT_0, data | DEFEA_INTRENABLE);
}

#if defined(__FreeBSD__)
static int pdq_eisa_shutdown(struct kern_devconf *kdc, int force);
static int pdq_eisa_probe(void);
static int pdq_eisa_attach(struct eisa_device *ed);

static unsigned long pdq_eisa_unit;

static struct eisa_driver pdq_eisa_driver = {
    "fea", pdq_eisa_probe, pdq_eisa_attach, NULL, &pdq_eisa_unit
};

DATA_SET(eisadriver_set, pdq_eisa_driver);

static struct kern_devconf kdc_pdq_eisa = {
    0, 0, 0,			/* filled in by dev_attach */
    "fea", 0, { MDDT_EISA, 0, "net" },
    eisa_generic_externalize, 0, pdq_eisa_shutdown, EISA_EXTERNALLEN,
    &kdc_eisa0,			/* parent */
    0,				/* parentdata */
    DC_BUSY,			/* host adapters are always ``in use'' */
    "DEC DEFEA EISA FDDI Controller",
    DC_CLS_NETIF
};

static const char *
pdq_eisa_match(
    eisa_id_t type)
{
    if ((type >> 8) == 0x10a330)
	return kdc_pdq_eisa.kdc_description;
    return NULL;
}

static int
pdq_eisa_probe(
    void)
{
    struct eisa_device *ed = NULL;
    int count;

    for (count = 0; (ed = eisa_match_dev(ed, pdq_eisa_match)) != NULL; count++) {
	pdq_bus_ioport_t iobase = ed->ioconf.slot * EISA_SLOT_SIZE;
	pdq_uint32_t irq, maddr, msiz;

	eisa_add_iospace(ed, iobase, 0x200, RESVADDR_NONE);
	pdq_eisa_subprobe(PDQ_BUS_EISA, iobase, &maddr, &msiz, &irq);
	eisa_add_mspace(ed, maddr, msiz, RESVADDR_NONE);
	eisa_add_intr(ed, irq);
	eisa_registerdev(ed, &pdq_eisa_driver, &kdc_pdq_eisa);
    }
    return count;
}

static void
pdq_eisa_interrupt(
    void *arg)
{
    pdq_softc_t * const sc = (pdq_softc_t *) arg;
    (void) pdq_interrupt(sc->sc_pdq);
}

static int
pdq_eisa_attach(
    struct eisa_device *ed)
{
    pdq_softc_t *sc;
    resvaddr_t *iospace;
    resvaddr_t *mspace;
    int irq = ffs(ed->ioconf.irq) - 1;

    sc = (pdq_softc_t *) malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_ZERO);
    if (sc == NULL) {
	printf("fea%d: malloc failed!\n", sc->sc_if.if_unit);
	return -1;
    }
    pdqs_eisa[ed->unit] = sc;

    sc->sc_if.if_name = "fea";
    sc->sc_if.if_unit = ed->unit;

    if ((iospace = ed->ioconf.ioaddrs.lh_first) == NULL) {
	printf("fea%d: no iospace??\n", sc->sc_if.if_unit);
	return -1;
    }
    if ((mspace = ed->ioconf.maddrs.lh_first) == NULL) {
	printf("fea%d: no memory space??\n", sc->sc_if.if_unit);
	return -1;
    }

    sc->sc_iobase = (pdq_bus_ioport_t) iospace->addr;
    sc->sc_membase = (pdq_bus_memaddr_t) pmap_mapdev(mspace->addr, mspace->size);
    if (sc->sc_membase == NULL) {
	printf("fea%d: failed to map memory 0x%x-0x%x!\n",
	       sc->sc_if.if_unit, mspace->addr, mspace->addr + mspace->size - 1);
	return -1;
    }

    eisa_reg_start(ed);
    if (eisa_reg_iospace(ed, iospace)) {
	printf("fea%d: failed to register iospace 0x%x-0x%x!\n",
	       sc->sc_if.if_unit, iospace->addr, iospace->addr + iospace->size - 1);
	return -1;
    }
    if (eisa_reg_mspace(ed, mspace)) {
	printf("fea%d: failed to register memory 0x%x-0x%x!\n",
	       sc->sc_if.if_unit, mspace->addr, mspace->addr + mspace->size - 1);
	return -1;
    }

    if (eisa_reg_intr(ed, irq, pdq_eisa_interrupt, sc, &net_imask, 1)) {
	printf("fea%d: interrupt registration failed\n", sc->sc_if.if_unit);
	return -1;
    }

    eisa_reg_end(ed);

    pdq_eisa_devinit(sc);
    sc->sc_pdq = pdq_initialize(PDQ_BUS_EISA, sc->sc_membase,
				sc->sc_if.if_name, sc->sc_if.if_unit,
				(void *) sc, PDQ_DEFEA);
    if (sc->sc_pdq == NULL) {
	printf("fea%d: initialization failed\n", sc->sc_if.if_unit);
	return -1;
    }

    if (eisa_enable_intr(ed, irq)) {
	printf("fea%d: failed to enable interrupt\n", sc->sc_if.if_unit);
	return -1;
    }

    memcpy(sc->sc_ac.ac_enaddr, (void *) sc->sc_pdq->pdq_hwaddr.lanaddr_bytes, 6);
    pdq_ifattach(sc, pdq_eisa_ifwatchdog);

    ed->kdc->kdc_state = DC_BUSY;	 /* host adapters always busy */

    return 0;
}

static int
pdq_eisa_shutdown(
    struct kern_devconf *kdc,
    int force)
{
    pdq_hwreset(PDQ_EISA_UNIT_TO_SOFTC(kdc->kdc_unit)->sc_pdq);
    (void) dev_detach(kdc);
    return 0;
}
#endif /* __FreeBSD__ */

#if defined(__bsdi__)
static int
pdq_eisa_probe(
    device_t parent,
    cfdata_t cf,
    void *aux)
{
    struct isa_attach_args *ia = (struct isa_attach_args *) aux;
    int slot;
    pdq_uint32_t irq, maddr, msiz;

    if (isa_bustype != BUS_EISA)
	return 0;

    if ((slot = eisa_match(cf, ia)) == 0)
	return 0;
    ia->ia_iobase = slot << 12;
    ia->ia_iosize = EISA_NPORT;
    eisa_slotalloc(slot);

    pdq_eisa_subprobe(PDQ_BUS_EISA, ia->ia_iobase, &maddr, &msiz, &irq);
    if (ia->ia_irq != IRQUNK && irq != ia->ia_irq) {
	printf("fea%d: error: desired IRQ of %d does not match device's actual IRQ (%d),\n",
	       cf->cf_unit,
	       ffs(ia->ia_irq) - 1, ffs(irq) - 1);
	return 0;
    }
    if (ia->ia_irq == IRQUNK) {
	if ((ia->ia_irq = isa_irqalloc(irq)) == 0) {
	    if ((ia->ia_irq = isa_irqalloc(IRQ9|IRQ10|IRQ11|IRQ15)) == 0) {
		printf("fea%d: error: IRQ %d is already in use\n", cf->cf_unit,
		       ffs(irq) - 1);
		return 0;
	    }
	    irq = PDQ_OS_IORD_8(PDQ_BUS_EISA, ia->ia_iobase, PDQ_EISA_IO_CONFIG_STAT_0) & ~3;
	    switch (ia->ia_irq) {
		case IRQ9:  irq |= 0;
		case IRQ10: irq |= 1;
		case IRQ11: irq |= 2;
		case IRQ15: irq |= 3;
	    }
	    PDQ_OS_IOWR_8(PDQ_BUS_EISA, ia->ia_iobase, PDQ_EISA_IO_CONFIG_STAT_0, irq);
	}
    }
    if (maddr == 0) {
	printf("fea%d: error: memory not enabled! ECU reconfiguration required\n",
	       cf->cf_unit);
	return 0;
    }

    /* EISA bus masters don't use host DMA channels */
    ia->ia_drq = DRQNONE;

    ia->ia_maddr = (void *) maddr;
    ia->ia_msize = msiz;
    return 1;
}

static void
pdq_eisa_attach(
    device_t parent,
    device_t self,
    void *aux)
{
    pdq_softc_t *sc = device_private(self);
    struct isa_attach_args *ia = (struct isa_attach_args *) aux;
    struct ifnet *ifp = &sc->sc_if;

    sc->sc_dev = self;
    sc->sc_if.if_unit = sc->sc_dev.dv_unit;
    sc->sc_if.if_name = "fea";
    sc->sc_if.if_flags = 0;

    sc->sc_iobase = ia->ia_iobase;

    pdq_eisa_devinit(sc);
    sc->sc_pdq = pdq_initialize(PDQ_BUS_EISA,
				(pdq_bus_memaddr_t) ISA_HOLE_VADDR(ia->ia_maddr),
				sc->sc_if.if_name, sc->sc_if.if_unit,
				(void *) sc, PDQ_DEFEA);
    if (sc->sc_pdq == NULL) {
	printf("fea%d: initialization failed\n", sc->sc_if.if_unit);
	return;
    }

    memcpy(sc->sc_ac.ac_enaddr, (void *) sc->sc_pdq->pdq_hwaddr.lanaddr_bytes, 6);

    pdq_ifattach(sc, pdq_eisa_ifwatchdog);

    isa_establish(&sc->sc_id, &sc->sc_dev);

    sc->sc_ih.ih_fun = pdq_interrupt;
    sc->sc_ih.ih_arg = (void *) sc->sc_pdq;
    intr_establish(ia->ia_irq, &sc->sc_ih, DV_NET);

    sc->sc_ats.func = (void (*)(void *)) pdq_hwreset;
    sc->sc_ats.arg = (void *) sc->sc_pdq;
    atshutdown(&sc->sc_ats, ATSH_ADD);
}

static char *pdq_eisa_ids[] = {
    "DEC3001",	/* 0x0130A310 */
    "DEC3002",	/* 0x0230A310 */
    "DEC3003",	/* 0x0330A310 */
    "DEC3004",	/* 0x0430A310 */
};

struct cfdriver feacd = {
    0, "fea", pdq_eisa_probe, pdq_eisa_attach, DV_IFNET, sizeof(pdq_softc_t),
    pdq_eisa_ids
};
#endif /* __bsdi__ */

#if defined(__NetBSD__)
static int
pdq_eisa_match(
    device_t parent,
    cfdata_t match,
    void *aux)
{
    const struct eisa_attach_args * const ea = (struct eisa_attach_args *) aux;

    if (strncmp(ea->ea_idstring, "DEC300", 6) == 0)
	return 1;

    return 0;
}

static void
pdq_eisa_attach(
    device_t parent,
    device_t self,
    void *aux)
{
    pdq_softc_t * const sc = device_private(self);
    struct eisa_attach_args * const ea = (struct eisa_attach_args *) aux;
    pdq_uint32_t irq, maddr, msiz;
    eisa_intr_handle_t ih;
    const char *intrstr;
    char intrbuf[EISA_INTRSTR_LEN];

    sc->sc_iotag = ea->ea_iot;
    sc->sc_dmatag = ea->ea_dmat;
    memcpy(sc->sc_if.if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
    sc->sc_if.if_flags = 0;
    sc->sc_if.if_softc = sc;

    if (bus_space_map(sc->sc_iotag, EISA_SLOT_ADDR(ea->ea_slot), EISA_SLOT_SIZE, 0, &sc->sc_iobase)) {
	aprint_normal("\n");
	aprint_error_dev(sc->sc_dev, "failed to map I/O!\n");
	return;
    }

    pdq_eisa_subprobe(sc->sc_iotag, sc->sc_iobase, &maddr, &msiz, &irq);

    if (maddr != 0 && msiz != 0) {
	sc->sc_csr_memmapped = true;
	sc->sc_csrtag = ea->ea_memt;
	if (bus_space_map(sc->sc_csrtag, maddr, msiz, 0, &sc->sc_membase)) {
	    bus_space_unmap(sc->sc_iotag, sc->sc_iobase, EISA_SLOT_SIZE);
	    aprint_normal("\n");
	    aprint_error_dev(sc->sc_dev, "failed to map memory (0x%x-0x%x)!\n",
			maddr, maddr + msiz - 1);
	    return;
	}
    } else {
	sc->sc_csr_memmapped = false;
	sc->sc_csrtag = sc->sc_iotag;
	sc->sc_membase = sc->sc_iobase;
    }

    pdq_eisa_devinit(sc);
    sc->sc_pdq = pdq_initialize(sc->sc_csrtag, sc->sc_membase,
				sc->sc_if.if_xname, 0,
				(void *) sc, PDQ_DEFEA);
    if (sc->sc_pdq == NULL) {
	aprint_error_dev(sc->sc_dev, "initialization failed\n");
	return;
    }

    pdq_ifattach(sc, pdq_eisa_ifwatchdog);

    if (eisa_intr_map(ea->ea_ec, irq, &ih)) {
	aprint_error_dev(sc->sc_dev, "couldn't map interrupt (%d)\n", irq);
	return;
    }
    intrstr = eisa_intr_string(ea->ea_ec, ih, intrbuf, sizeof(intrbuf));
    sc->sc_ih = eisa_intr_establish(ea->ea_ec, ih, IST_LEVEL, IPL_NET,
				    (int (*)(void *)) pdq_interrupt, sc->sc_pdq);
    if (sc->sc_ih == NULL) {
	aprint_error_dev(sc->sc_dev, "couldn't establish interrupt");
	if (intrstr != NULL)
	    aprint_error(" at %s", intrstr);
	aprint_error("\n");
	return;
    }
    sc->sc_ats = shutdownhook_establish((void (*)(void *)) pdq_hwreset, sc->sc_pdq);
    if (sc->sc_ats == NULL)
	aprint_error_dev(self, "warning: couldn't establish shutdown hook\n");
    if (sc->sc_csr_memmapped)
	printf("%s: using iomem 0x%x-0x%x\n", device_xname(sc->sc_dev), maddr, maddr + msiz - 1);
    if (intrstr != NULL)
	printf("%s: interrupting at %s\n", device_xname(sc->sc_dev), intrstr);
}

CFATTACH_DECL_NEW(fea, sizeof(pdq_softc_t),
    pdq_eisa_match, pdq_eisa_attach, NULL, NULL);
#endif

/*	$NetBSD: pccbb.c,v 1.208 2015/03/26 20:13:28 nakayama Exp $	*/

/*
 * Copyright (c) 1998, 1999 and 2000
 *      HAYAKAWA Koichi.  All rights reserved.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pccbb.c,v 1.208 2015/03/26 20:13:28 nakayama Exp $");

/*
#define CBB_DEBUG
#define SHOW_REGS
*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>		/* for bootverbose */
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <sys/intr.h>
#include <sys/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/pccbbreg.h>

#include <dev/cardbus/cardslotvar.h>

#include <dev/cardbus/cardbusvar.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>

#include <dev/ic/i82365reg.h>
#include <dev/pci/pccbbvar.h>

#ifndef __NetBSD_Version__
struct cfdriver cbb_cd = {
	NULL, "cbb", DV_DULL
};
#endif

#ifdef CBB_DEBUG
#define DPRINTF(x) printf x
#define STATIC
#else
#define DPRINTF(x)
#define STATIC static
#endif

int pccbb_burstup = 1;

/*
 * delay_ms() is wait in milliseconds.  It should be used instead
 * of delay() if you want to wait more than 1 ms.
 */
static inline void
delay_ms(int millis, struct pccbb_softc *sc)
{
	if (cold)
		delay(millis * 1000);
	else
		kpause("pccbb", false, mstohz(millis), NULL);
}

int pcicbbmatch(device_t, cfdata_t, void *);
void pccbbattach(device_t, device_t, void *);
void pccbbchilddet(device_t, device_t);
int pccbbdetach(device_t, int);
int pccbbintr(void *);
static void pci113x_insert(void *);
static int pccbbintr_function(struct pccbb_softc *);

static int pccbb_detect_card(struct pccbb_softc *);

static void pccbb_pcmcia_write(struct pccbb_softc *, int, u_int8_t);
static u_int8_t pccbb_pcmcia_read(struct pccbb_softc *, int);
#define Pcic_read(sc, reg) pccbb_pcmcia_read((sc), (reg))
#define Pcic_write(sc, reg, val) pccbb_pcmcia_write((sc), (reg), (val))

STATIC int cb_reset(struct pccbb_softc *);
STATIC int cb_detect_voltage(struct pccbb_softc *);
STATIC int cbbprint(void *, const char *);

static int cb_chipset(u_int32_t, int *);
STATIC void pccbb_pcmcia_attach_setup(struct pccbb_softc *,
    struct pcmciabus_attach_args *);

STATIC int pccbb_ctrl(cardbus_chipset_tag_t, int);
STATIC int pccbb_power(struct pccbb_softc *sc, int);
STATIC int pccbb_power_ct(cardbus_chipset_tag_t, int);
STATIC int pccbb_cardenable(struct pccbb_softc * sc, int function);
static void *pccbb_intr_establish(struct pccbb_softc *,
    int level, int (*ih) (void *), void *sc);
static void pccbb_intr_disestablish(struct pccbb_softc *, void *ih);

static void *pccbb_cb_intr_establish(cardbus_chipset_tag_t,
    int level, int (*ih) (void *), void *sc);
static void pccbb_cb_intr_disestablish(cardbus_chipset_tag_t ct, void *ih);

static pcitag_t pccbb_make_tag(cardbus_chipset_tag_t, int, int);
static pcireg_t pccbb_conf_read(cardbus_chipset_tag_t, pcitag_t, int);
static void pccbb_conf_write(cardbus_chipset_tag_t, pcitag_t, int,
    pcireg_t);
static void pccbb_chipinit(struct pccbb_softc *);
static void pccbb_intrinit(struct pccbb_softc *);

STATIC int pccbb_pcmcia_mem_alloc(pcmcia_chipset_handle_t, bus_size_t,
    struct pcmcia_mem_handle *);
STATIC void pccbb_pcmcia_mem_free(pcmcia_chipset_handle_t,
    struct pcmcia_mem_handle *);
STATIC int pccbb_pcmcia_mem_map(pcmcia_chipset_handle_t, int, bus_addr_t,
    bus_size_t, struct pcmcia_mem_handle *, bus_size_t *, int *);
STATIC void pccbb_pcmcia_mem_unmap(pcmcia_chipset_handle_t, int);
STATIC int pccbb_pcmcia_io_alloc(pcmcia_chipset_handle_t, bus_addr_t,
    bus_size_t, bus_size_t, struct pcmcia_io_handle *);
STATIC void pccbb_pcmcia_io_free(pcmcia_chipset_handle_t,
    struct pcmcia_io_handle *);
STATIC int pccbb_pcmcia_io_map(pcmcia_chipset_handle_t, int, bus_addr_t,
    bus_size_t, struct pcmcia_io_handle *, int *);
STATIC void pccbb_pcmcia_io_unmap(pcmcia_chipset_handle_t, int);
STATIC void *pccbb_pcmcia_intr_establish(pcmcia_chipset_handle_t,
    struct pcmcia_function *, int, int (*)(void *), void *);
STATIC void pccbb_pcmcia_intr_disestablish(pcmcia_chipset_handle_t, void *);
STATIC void pccbb_pcmcia_socket_enable(pcmcia_chipset_handle_t);
STATIC void pccbb_pcmcia_socket_disable(pcmcia_chipset_handle_t);
STATIC void pccbb_pcmcia_socket_settype(pcmcia_chipset_handle_t, int);
STATIC int pccbb_pcmcia_card_detect(pcmcia_chipset_handle_t pch);

static int pccbb_pcmcia_wait_ready(struct pccbb_softc *);
static void pccbb_pcmcia_delay(struct pccbb_softc *, int, const char *);

static void pccbb_pcmcia_do_io_map(struct pccbb_softc *, int);
static void pccbb_pcmcia_do_mem_map(struct pccbb_softc *, int);

/* bus-space allocation and deallocation functions */

static int pccbb_rbus_cb_space_alloc(cardbus_chipset_tag_t, rbus_tag_t,
    bus_addr_t addr, bus_size_t size, bus_addr_t mask, bus_size_t align,
    int flags, bus_addr_t * addrp, bus_space_handle_t * bshp);
static int pccbb_rbus_cb_space_free(cardbus_chipset_tag_t, rbus_tag_t,
    bus_space_handle_t, bus_size_t);



static int pccbb_open_win(struct pccbb_softc *, bus_space_tag_t,
    bus_addr_t, bus_size_t, bus_space_handle_t, int flags);
static int pccbb_close_win(struct pccbb_softc *, bus_space_tag_t,
    bus_space_handle_t, bus_size_t);
static int pccbb_winlist_insert(struct pccbb_win_chain_head *, bus_addr_t,
    bus_size_t, bus_space_handle_t, int);
static int pccbb_winlist_delete(struct pccbb_win_chain_head *,
    bus_space_handle_t, bus_size_t);
static void pccbb_winset(bus_addr_t align, struct pccbb_softc *,
    bus_space_tag_t);
void pccbb_winlist_show(struct pccbb_win_chain *);


/* for config_defer */
static void pccbb_pci_callback(device_t);

static bool pccbb_suspend(device_t, const pmf_qual_t *);
static bool pccbb_resume(device_t, const pmf_qual_t *);

#if defined SHOW_REGS
static void cb_show_regs(pci_chipset_tag_t pc, pcitag_t tag,
    bus_space_tag_t memt, bus_space_handle_t memh);
#endif

CFATTACH_DECL3_NEW(cbb_pci, sizeof(struct pccbb_softc),
    pcicbbmatch, pccbbattach, pccbbdetach, NULL, NULL, pccbbchilddet,
    DVF_DETACH_SHUTDOWN);

static const struct pcmcia_chip_functions pccbb_pcmcia_funcs = {
	pccbb_pcmcia_mem_alloc,
	pccbb_pcmcia_mem_free,
	pccbb_pcmcia_mem_map,
	pccbb_pcmcia_mem_unmap,
	pccbb_pcmcia_io_alloc,
	pccbb_pcmcia_io_free,
	pccbb_pcmcia_io_map,
	pccbb_pcmcia_io_unmap,
	pccbb_pcmcia_intr_establish,
	pccbb_pcmcia_intr_disestablish,
	pccbb_pcmcia_socket_enable,
	pccbb_pcmcia_socket_disable,
	pccbb_pcmcia_socket_settype,
	pccbb_pcmcia_card_detect
};

static const struct cardbus_functions pccbb_funcs = {
	pccbb_rbus_cb_space_alloc,
	pccbb_rbus_cb_space_free,
	pccbb_cb_intr_establish,
	pccbb_cb_intr_disestablish,
	pccbb_ctrl,
	pccbb_power_ct,
	pccbb_make_tag,
	pccbb_conf_read,
	pccbb_conf_write,
};

int
pcicbbmatch(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_BRIDGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_BRIDGE_CARDBUS &&
	    PCI_INTERFACE(pa->pa_class) == 0) {
		return 1;
	}

	return 0;
}

#define MAKEID(vendor, prod) (((vendor) << PCI_VENDOR_SHIFT) \
                              | ((prod) << PCI_PRODUCT_SHIFT))

const struct yenta_chipinfo {
	pcireg_t yc_id;		       /* vendor tag | product tag */
	int yc_chiptype;
	int yc_flags;
} yc_chipsets[] = {
	/* Texas Instruments chips */
	{ MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI1130), CB_TI113X,
	    PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI1131), CB_TI113X,
	    PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI1250), CB_TI125X,
	    PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI1220), CB_TI12XX,
	    PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI1221), CB_TI12XX,
	    PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI1225), CB_TI12XX,
	    PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI1251), CB_TI125X,
	    PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI1251B), CB_TI125X,
	    PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI1211), CB_TI12XX,
	    PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI1410), CB_TI12XX,
	    PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI1420), CB_TI1420,
	    PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI1450), CB_TI125X,
	    PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI1451), CB_TI12XX,
	    PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI1510), CB_TI12XX,
	    PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI1520), CB_TI12XX,
	    PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI4410YENTA), CB_TI12XX,
	    PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI4520YENTA), CB_TI12XX,
	    PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI7420YENTA), CB_TI12XX,
	    PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},

	/* Ricoh chips */
	{ MAKEID(PCI_VENDOR_RICOH, PCI_PRODUCT_RICOH_Rx5C475), CB_RX5C47X,
	    PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_RICOH, PCI_PRODUCT_RICOH_RL5C476), CB_RX5C47X,
	    PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_RICOH, PCI_PRODUCT_RICOH_Rx5C477), CB_RX5C47X,
	    PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_RICOH, PCI_PRODUCT_RICOH_Rx5C478), CB_RX5C47X,
	    PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_RICOH, PCI_PRODUCT_RICOH_Rx5C465), CB_RX5C46X,
	    PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_RICOH, PCI_PRODUCT_RICOH_Rx5C466), CB_RX5C46X,
	    PCCBB_PCMCIA_MEM_32},

	/* Toshiba products */
	{ MAKEID(PCI_VENDOR_TOSHIBA2, PCI_PRODUCT_TOSHIBA2_ToPIC95),
	    CB_TOPIC95, PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_TOSHIBA2, PCI_PRODUCT_TOSHIBA2_ToPIC95B),
	    CB_TOPIC95B, PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_TOSHIBA2, PCI_PRODUCT_TOSHIBA2_ToPIC97),
	    CB_TOPIC97, PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_TOSHIBA2, PCI_PRODUCT_TOSHIBA2_ToPIC100),
	    CB_TOPIC97, PCCBB_PCMCIA_MEM_32},

	/* Cirrus Logic products */
	{ MAKEID(PCI_VENDOR_CIRRUS, PCI_PRODUCT_CIRRUS_CL_PD6832),
	    CB_CIRRUS, PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_CIRRUS, PCI_PRODUCT_CIRRUS_CL_PD6833),
	    CB_CIRRUS, PCCBB_PCMCIA_MEM_32},

	/* O2 Micro products */
	{ MAKEID(PCI_VENDOR_O2MICRO, PCI_PRODUCT_O2MICRO_OZ6729),
	  CB_O2MICRO, PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_O2MICRO, PCI_PRODUCT_O2MICRO_OZ6730),
	  CB_O2MICRO, PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_O2MICRO, PCI_PRODUCT_O2MICRO_OZ6832),
	  CB_O2MICRO, PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_O2MICRO, PCI_PRODUCT_O2MICRO_OZ6836),
	  CB_O2MICRO, PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_O2MICRO, PCI_PRODUCT_O2MICRO_OZ6872),
	  CB_O2MICRO, PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_O2MICRO, PCI_PRODUCT_O2MICRO_OZ6922),
	  CB_O2MICRO, PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_O2MICRO, PCI_PRODUCT_O2MICRO_OZ6933),
	  CB_O2MICRO, PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_O2MICRO, PCI_PRODUCT_O2MICRO_OZ6972),
	  CB_O2MICRO, PCCBB_PCMCIA_MEM_32},
	{ MAKEID(PCI_VENDOR_O2MICRO, PCI_PRODUCT_O2MICRO_7223),
	  CB_O2MICRO, PCCBB_PCMCIA_MEM_32},

	/* sentinel, or Generic chip */
	{ 0 /* null id */ , CB_UNKNOWN, PCCBB_PCMCIA_MEM_32},
};

static int
cb_chipset(u_int32_t pci_id, int *flagp)
{
	const struct yenta_chipinfo *yc;

	/* Loop over except the last default entry. */
	for (yc = yc_chipsets; yc < yc_chipsets +
	    __arraycount(yc_chipsets) - 1; yc++)
		if (pci_id == yc->yc_id)
			break;

	if (flagp != NULL)
		*flagp = yc->yc_flags;

	return (yc->yc_chiptype);
}

void
pccbbchilddet(device_t self, device_t child)
{
	struct pccbb_softc *sc = device_private(self);
	int s;

	KASSERT(sc->sc_csc == device_private(child));

	s = splbio();
	if (sc->sc_csc == device_private(child))
		sc->sc_csc = NULL;
	splx(s);
}

void
pccbbattach(device_t parent, device_t self, void *aux)
{
	struct pccbb_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcireg_t reg, sock_base;
	bus_addr_t sockbase;
	int flags;

#ifdef __HAVE_PCCBB_ATTACH_HOOK
	pccbb_attach_hook(parent, self, pa);
#endif

	sc->sc_dev = self;

	mutex_init(&sc->sc_pwr_mtx, MUTEX_DEFAULT, IPL_BIO);
	cv_init(&sc->sc_pwr_cv, "pccpwr");

	callout_init(&sc->sc_insert_ch, 0);
	callout_setfunc(&sc->sc_insert_ch, pci113x_insert, sc);

	sc->sc_chipset = cb_chipset(pa->pa_id, &flags);

	pci_aprint_devinfo(pa, NULL);
	DPRINTF(("(chipflags %x)", flags));

	TAILQ_INIT(&sc->sc_memwindow);
	TAILQ_INIT(&sc->sc_iowindow);

	sc->sc_rbus_iot = rbus_pccbb_parent_io(pa);
	sc->sc_rbus_memt = rbus_pccbb_parent_mem(pa);

#if 0
	printf("pa->pa_memt: %08x vs rbus_mem->rb_bt: %08x\n",
	       pa->pa_memt, sc->sc_rbus_memt->rb_bt);
#endif

	sc->sc_flags &= ~CBB_MEMHMAPPED;

	/*
	 * MAP socket registers and ExCA registers on memory-space
	 * When no valid address is set on socket base registers (on pci
	 * config space), get it not polite way.
	 */
	sock_base = pci_conf_read(pc, pa->pa_tag, PCI_SOCKBASE);

	if (PCI_MAPREG_MEM_ADDR(sock_base) >= 0x100000 &&
	    PCI_MAPREG_MEM_ADDR(sock_base) != 0xfffffff0) {
		/* The address must be valid. */
		if (pci_mapreg_map(pa, PCI_SOCKBASE, PCI_MAPREG_TYPE_MEM, 0,
		    &sc->sc_base_memt, &sc->sc_base_memh, &sockbase, &sc->sc_base_size)) {
			aprint_error_dev(self,
			    "can't map socket base address 0x%lx\n",
			    (unsigned long)sock_base);
			/*
			 * I think it's funny: socket base registers must be
			 * mapped on memory space, but ...
			 */
			if (pci_mapreg_map(pa, PCI_SOCKBASE, PCI_MAPREG_TYPE_IO,
			    0, &sc->sc_base_memt, &sc->sc_base_memh, &sockbase,
			    &sc->sc_base_size)) {
				aprint_error_dev(self,
				    "can't map socket base address"
				    " 0x%lx: io mode\n",
				    (unsigned long)sockbase);
				/* give up... allocate reg space via rbus. */
				pci_conf_write(pc, pa->pa_tag, PCI_SOCKBASE, 0);
			} else
				sc->sc_flags |= CBB_MEMHMAPPED;
		} else {
			DPRINTF(("%s: socket base address 0x%lx\n",
			    device_xname(self),
			    (unsigned long)sockbase));
			sc->sc_flags |= CBB_MEMHMAPPED;
		}
	}

	sc->sc_mem_start = 0;	       /* XXX */
	sc->sc_mem_end = 0xffffffff;   /* XXX */

	/* pccbb_machdep.c end */

#if defined CBB_DEBUG
	{
		static const char *intrname[] = { "NON", "A", "B", "C", "D" };
		aprint_debug_dev(self, "intrpin %s, intrtag %d\n",
		    intrname[pa->pa_intrpin], pa->pa_intrline);
	}
#endif

	/* setup softc */
	sc->sc_pc = pc;
	sc->sc_iot = pa->pa_iot;
	sc->sc_memt = pa->pa_memt;
	sc->sc_dmat = pa->pa_dmat;
	sc->sc_tag = pa->pa_tag;

	memcpy(&sc->sc_pa, pa, sizeof(*pa));

	sc->sc_pcmcia_flags = flags;   /* set PCMCIA facility */

	/* Disable legacy register mapping. */
	switch (sc->sc_chipset) {
	case CB_RX5C46X:	       /* fallthrough */
#if 0
	/* The RX5C47X-series requires writes to the PCI_LEGACY register. */
	case CB_RX5C47X:
#endif
		/*
		 * The legacy pcic io-port on Ricoh RX5C46X CardBus bridges
		 * cannot be disabled by substituting 0 into PCI_LEGACY
		 * register.  Ricoh CardBus bridges have special bits on Bridge
		 * control reg (addr 0x3e on PCI config space).
		 */
		reg = pci_conf_read(pc, pa->pa_tag, PCI_BRIDGE_CONTROL_REG);
		reg &= ~(CB_BCRI_RL_3E0_ENA | CB_BCRI_RL_3E2_ENA);
		pci_conf_write(pc, pa->pa_tag, PCI_BRIDGE_CONTROL_REG, reg);
		break;

	default:
		/* XXX I don't know proper way to kill legacy I/O. */
		pci_conf_write(pc, pa->pa_tag, PCI_LEGACY, 0x0);
		break;
	}

	if (!pmf_device_register(self, pccbb_suspend, pccbb_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");

	config_defer(self, pccbb_pci_callback);
}

int
pccbbdetach(device_t self, int flags)
{
	struct pccbb_softc *sc = device_private(self);
	pci_chipset_tag_t pc = sc->sc_pa.pa_pc;
	bus_space_tag_t bmt = sc->sc_base_memt;
	bus_space_handle_t bmh = sc->sc_base_memh;
	uint32_t sockmask;
	int rc;

	if ((rc = config_detach_children(self, flags)) != 0)
		return rc;

	if (!LIST_EMPTY(&sc->sc_pil)) {
		panic("%s: interrupt handlers still registered",
		    device_xname(self));
		return EBUSY;
	}

	if (sc->sc_ih != NULL) {
		pci_intr_disestablish(pc, sc->sc_ih);
		sc->sc_ih = NULL;
	}

	/* CSC Interrupt: turn off card detect and power cycle interrupts */
	sockmask = bus_space_read_4(bmt, bmh, CB_SOCKET_MASK);
	sockmask &= ~(CB_SOCKET_MASK_CSTS | CB_SOCKET_MASK_CD |
		      CB_SOCKET_MASK_POWER);
	bus_space_write_4(bmt, bmh, CB_SOCKET_MASK, sockmask);
	/* reset interrupt */
	bus_space_write_4(bmt, bmh, CB_SOCKET_EVENT,
	    bus_space_read_4(bmt, bmh, CB_SOCKET_EVENT));

	switch (sc->sc_flags & (CBB_MEMHMAPPED|CBB_SPECMAPPED)) {
	case CBB_MEMHMAPPED:
		bus_space_unmap(bmt, bmh, sc->sc_base_size);
		break;
	case CBB_MEMHMAPPED|CBB_SPECMAPPED:
#if rbus
	{
		rbus_space_free(sc->sc_rbus_memt, bmh, 0x1000,
		    NULL);
	}
#else
		bus_space_free(bmt, bmh, 0x1000);
#endif
	}
	sc->sc_flags &= ~(CBB_MEMHMAPPED|CBB_SPECMAPPED);

	if (!TAILQ_EMPTY(&sc->sc_iowindow))
		aprint_error_dev(self, "i/o windows not empty\n");
	if (!TAILQ_EMPTY(&sc->sc_memwindow))
		aprint_error_dev(self, "memory windows not empty\n");

	callout_halt(&sc->sc_insert_ch, NULL);
	callout_destroy(&sc->sc_insert_ch);

	mutex_destroy(&sc->sc_pwr_mtx);
	cv_destroy(&sc->sc_pwr_cv);

	return 0;
}

/*
 * static void pccbb_pci_callback(device_t self)
 *
 *   The actual attach routine: get memory space for YENTA register
 *   space, setup YENTA register and route interrupt.
 *
 *   This function should be deferred because this device may obtain
 *   memory space dynamically.  This function must avoid obtaining
 *   memory area which has already kept for another device.
 */
static void
pccbb_pci_callback(device_t self)
{
	struct pccbb_softc *sc = device_private(self);
	pci_chipset_tag_t pc = sc->sc_pc;
	bus_addr_t sockbase;
	struct cbslot_attach_args cba;
	struct pcmciabus_attach_args paa;
	struct cardslot_attach_args caa;
	device_t csc;

	if (!(sc->sc_flags & CBB_MEMHMAPPED)) {
		/* The socket registers aren't mapped correctly. */
#if rbus
		if (rbus_space_alloc(sc->sc_rbus_memt, 0, 0x1000, 0x0fff,
		    (sc->sc_chipset == CB_RX5C47X
		    || sc->sc_chipset == CB_TI113X) ? 0x10000 : 0x1000,
		    0, &sockbase, &sc->sc_base_memh)) {
			return;
		}
		sc->sc_base_memt = sc->sc_memt;
		pci_conf_write(pc, sc->sc_tag, PCI_SOCKBASE, sockbase);
		DPRINTF(("%s: CardBus register address 0x%lx -> 0x%lx\n",
		    device_xname(self), (unsigned long)sockbase,
		    (unsigned long)pci_conf_read(pc, sc->sc_tag,
		    PCI_SOCKBASE)));
#else
		sc->sc_base_memt = sc->sc_memt;
#if !defined CBB_PCI_BASE
#define CBB_PCI_BASE 0x20000000
#endif
		if (bus_space_alloc(sc->sc_base_memt, CBB_PCI_BASE, 0xffffffff,
		    0x1000, 0x1000, 0, 0, &sockbase, &sc->sc_base_memh)) {
			/* cannot allocate memory space */
			return;
		}
		pci_conf_write(pc, sc->sc_tag, PCI_SOCKBASE, sockbase);
		DPRINTF(("%s: CardBus register address 0x%lx -> 0x%lx\n",
		    device_xname(self), (unsigned long)sock_base,
		    (unsigned long)pci_conf_read(pc,
		    sc->sc_tag, PCI_SOCKBASE)));
#endif
		sc->sc_flags |= CBB_MEMHMAPPED|CBB_SPECMAPPED;
	}

	/* clear data structure for child device interrupt handlers */
	LIST_INIT(&sc->sc_pil);

	/* bus bridge initialization */
	pccbb_chipinit(sc);

	sc->sc_pil_intr_enable = true;

	{
		u_int32_t sockstat;

		sockstat = bus_space_read_4(sc->sc_base_memt,
		    sc->sc_base_memh, CB_SOCKET_STAT);
		if (0 == (sockstat & CB_SOCKET_STAT_CD)) {
			sc->sc_flags |= CBB_CARDEXIST;
		}
	}

	/*
	 * attach cardbus
	 */
	{
		pcireg_t busreg = pci_conf_read(pc, sc->sc_tag, PCI_BUSNUM);
		pcireg_t bhlc = pci_conf_read(pc, sc->sc_tag, PCI_BHLC_REG);

		/* initialize cbslot_attach */
		cba.cba_iot = sc->sc_iot;
		cba.cba_memt = sc->sc_memt;
		cba.cba_dmat = sc->sc_dmat;
		cba.cba_bus = (busreg >> 8) & 0x0ff;
		cba.cba_cc = (void *)sc;
		cba.cba_cf = &pccbb_funcs;

#if rbus
		cba.cba_rbus_iot = sc->sc_rbus_iot;
		cba.cba_rbus_memt = sc->sc_rbus_memt;
#endif

		cba.cba_cacheline = PCI_CACHELINE(bhlc);
		cba.cba_max_lattimer = PCI_LATTIMER(bhlc);

		aprint_verbose_dev(self,
		    "cacheline 0x%x lattimer 0x%x\n",
		    cba.cba_cacheline,
		    cba.cba_max_lattimer);
		aprint_verbose_dev(self, "bhlc 0x%x\n", bhlc);
#if defined SHOW_REGS
		cb_show_regs(sc->sc_pc, sc->sc_tag, sc->sc_base_memt,
		    sc->sc_base_memh);
#endif
	}

	pccbb_pcmcia_attach_setup(sc, &paa);
	caa.caa_cb_attach = NULL;
	if (cba.cba_bus == 0)
		aprint_error_dev(self,
		    "secondary bus number uninitialized; try PCI_BUS_FIXUP\n");
	else
		caa.caa_cb_attach = &cba;
	caa.caa_16_attach = &paa;

	pccbb_intrinit(sc);

	if (NULL != (csc = config_found_ia(self, "pcmciaslot", &caa,
					   cbbprint))) {
		DPRINTF(("%s: found cardslot\n", __func__));
		sc->sc_csc = device_private(csc);
	}

	return;
}





/*
 * static void pccbb_chipinit(struct pccbb_softc *sc)
 *
 *   This function initialize YENTA chip registers listed below:
 *     1) PCI command reg,
 *     2) PCI and CardBus latency timer,
 *     3) route PCI interrupt,
 *     4) close all memory and io windows.
 *     5) turn off bus power.
 *     6) card detect and power cycle interrupts on.
 *     7) clear interrupt
 */
static void
pccbb_chipinit(struct pccbb_softc *sc)
{
	pci_chipset_tag_t pc = sc->sc_pc;
	pcitag_t tag = sc->sc_tag;
	bus_space_tag_t bmt = sc->sc_base_memt;
	bus_space_handle_t bmh = sc->sc_base_memh;
	pcireg_t bcr, bhlc, cbctl, csr, lscp, mfunc, mrburst, slotctl, sockctl,
	    sysctrl;

	/*
	 * Set PCI command reg.
	 * Some laptop's BIOSes (i.e. TICO) do not enable CardBus chip.
	 */
	csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	/* I believe it is harmless. */
	csr |= (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
	    PCI_COMMAND_MASTER_ENABLE);

	/* All O2 Micro chips have broken parity-error reporting
	 * until proven otherwise.  The OZ6933 PCI-CardBus Bridge
	 * is known to have the defect---see PR kern/38698.
	 */
	if (sc->sc_chipset != CB_O2MICRO)
		csr |= PCI_COMMAND_PARITY_ENABLE;

	csr |= PCI_COMMAND_SERR_ENABLE;
	pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, csr);

	/*
	 * Set CardBus latency timer.
	 */
	lscp = pci_conf_read(pc, tag, PCI_CB_LSCP_REG);
	if (PCI_CB_LATENCY(lscp) < 0x20) {
		lscp &= ~(PCI_CB_LATENCY_MASK << PCI_CB_LATENCY_SHIFT);
		lscp |= (0x20 << PCI_CB_LATENCY_SHIFT);
		pci_conf_write(pc, tag, PCI_CB_LSCP_REG, lscp);
	}
	DPRINTF(("CardBus latency timer 0x%x (%x)\n",
	    PCI_CB_LATENCY(lscp), pci_conf_read(pc, tag, PCI_CB_LSCP_REG)));

	/*
	 * Set PCI latency timer.
	 */
	bhlc = pci_conf_read(pc, tag, PCI_BHLC_REG);
	if (PCI_LATTIMER(bhlc) < 0x10) {
		bhlc &= ~(PCI_LATTIMER_MASK << PCI_LATTIMER_SHIFT);
		bhlc |= (0x10 << PCI_LATTIMER_SHIFT);
		pci_conf_write(pc, tag, PCI_BHLC_REG, bhlc);
	}
	DPRINTF(("PCI latency timer 0x%x (%x)\n",
	    PCI_LATTIMER(bhlc), pci_conf_read(pc, tag, PCI_BHLC_REG)));


	/* Route functional interrupts to PCI. */
	bcr = pci_conf_read(pc, tag, PCI_BRIDGE_CONTROL_REG);
	bcr |= CB_BCR_INTR_IREQ_ENABLE;		/* disable PCI Intr */
	bcr |= CB_BCR_WRITE_POST_ENABLE;	/* enable write post */
	/* assert reset */
	bcr |= PCI_BRIDGE_CONTROL_SECBR	<< PCI_BRIDGE_CONTROL_SHIFT;
        /* Set master abort mode to 1, forward SERR# from secondary
         * to primary, and detect parity errors on secondary.
	 */
	bcr |= PCI_BRIDGE_CONTROL_MABRT	<< PCI_BRIDGE_CONTROL_SHIFT;
	bcr |= PCI_BRIDGE_CONTROL_SERR << PCI_BRIDGE_CONTROL_SHIFT;
	bcr |= PCI_BRIDGE_CONTROL_PERE << PCI_BRIDGE_CONTROL_SHIFT;
	pci_conf_write(pc, tag, PCI_BRIDGE_CONTROL_REG, bcr);

	switch (sc->sc_chipset) {
	case CB_TI113X:
		cbctl = pci_conf_read(pc, tag, PCI_CBCTRL);
		/* This bit is shared, but may read as 0 on some chips, so set
		   it explicitly on both functions. */
		cbctl |= PCI113X_CBCTRL_PCI_IRQ_ENA;
		/* CSC intr enable */
		cbctl |= PCI113X_CBCTRL_PCI_CSC;
		/* functional intr prohibit | prohibit ISA routing */
		cbctl &= ~(PCI113X_CBCTRL_PCI_INTR | PCI113X_CBCTRL_INT_MASK);
		pci_conf_write(pc, tag, PCI_CBCTRL, cbctl);
		break;

	case CB_TI1420:
		sysctrl = pci_conf_read(pc, tag, PCI_SYSCTRL);
		mrburst = pccbb_burstup
		    ? PCI1420_SYSCTRL_MRBURST : PCI1420_SYSCTRL_MRBURSTDN;
		if ((sysctrl & PCI1420_SYSCTRL_MRBURST) == mrburst) {
			printf("%s: %swrite bursts enabled\n",
			    device_xname(sc->sc_dev),
			    pccbb_burstup ? "read/" : "");
		} else if (pccbb_burstup) {
			printf("%s: enabling read/write bursts\n",
			    device_xname(sc->sc_dev));
			sysctrl |= PCI1420_SYSCTRL_MRBURST;
			pci_conf_write(pc, tag, PCI_SYSCTRL, sysctrl);
		} else {
			printf("%s: disabling read bursts, "
			    "enabling write bursts\n",
			    device_xname(sc->sc_dev));
			sysctrl |= PCI1420_SYSCTRL_MRBURSTDN;
			sysctrl &= ~PCI1420_SYSCTRL_MRBURSTUP;
			pci_conf_write(pc, tag, PCI_SYSCTRL, sysctrl);
		}
		/*FALLTHROUGH*/
	case CB_TI12XX:
		/*
		 * Some TI 12xx (and [14][45]xx) based pci cards
		 * sometimes have issues with the MFUNC register not
		 * being initialized due to a bad EEPROM on board.
		 * Laptops that this matters on have this register
		 * properly initialized.
		 *
		 * The TI125X parts have a different register.
		 */
		mfunc = pci_conf_read(pc, tag, PCI12XX_MFUNC);
		if ((mfunc & (PCI12XX_MFUNC_PIN0 | PCI12XX_MFUNC_PIN1)) == 0) {
			/* Enable PCI interrupt /INTA */
			mfunc |= PCI12XX_MFUNC_PIN0_INTA;

			/* XXX this is TI1520 only */
			if ((pci_conf_read(pc, tag, PCI_SYSCTRL) &
			     PCI12XX_SYSCTRL_INTRTIE) == 0)
				/* Enable PCI interrupt /INTB */
				mfunc |= PCI12XX_MFUNC_PIN1_INTB;

			pci_conf_write(pc, tag, PCI12XX_MFUNC, mfunc);
		}
		/* fallthrough */

	case CB_TI125X:
		/*
		 * Disable zoom video.  Some machines initialize this
		 * improperly and experience has shown that this helps
		 * prevent strange behavior.
		 */
		pci_conf_write(pc, tag, PCI12XX_MMCTRL, 0);

		sysctrl = pci_conf_read(pc, tag, PCI_SYSCTRL);
		sysctrl |= PCI12XX_SYSCTRL_VCCPROT;
		pci_conf_write(pc, tag, PCI_SYSCTRL, sysctrl);
		cbctl = pci_conf_read(pc, tag, PCI_CBCTRL);
		cbctl |= PCI12XX_CBCTRL_CSC;
		pci_conf_write(pc, tag, PCI_CBCTRL, cbctl);
		break;

	case CB_TOPIC95B:
		sockctl = pci_conf_read(pc, tag, TOPIC_SOCKET_CTRL);
		sockctl |= TOPIC_SOCKET_CTRL_SCR_IRQSEL;
		pci_conf_write(pc, tag, TOPIC_SOCKET_CTRL, sockctl);
		slotctl = pci_conf_read(pc, tag, TOPIC_SLOT_CTRL);
		DPRINTF(("%s: topic slot ctrl reg 0x%x -> ",
		    device_xname(sc->sc_dev), slotctl));
		slotctl |= (TOPIC_SLOT_CTRL_SLOTON | TOPIC_SLOT_CTRL_SLOTEN |
		    TOPIC_SLOT_CTRL_ID_LOCK | TOPIC_SLOT_CTRL_CARDBUS);
		slotctl &= ~TOPIC_SLOT_CTRL_SWDETECT;
		DPRINTF(("0x%x\n", slotctl));
		pci_conf_write(pc, tag, TOPIC_SLOT_CTRL, slotctl);
		break;

	case CB_TOPIC97:
		slotctl = pci_conf_read(pc, tag, TOPIC_SLOT_CTRL);
		DPRINTF(("%s: topic slot ctrl reg 0x%x -> ",
		    device_xname(sc->sc_dev), slotctl));
		slotctl |= (TOPIC_SLOT_CTRL_SLOTON | TOPIC_SLOT_CTRL_SLOTEN |
		    TOPIC_SLOT_CTRL_ID_LOCK | TOPIC_SLOT_CTRL_CARDBUS);
		slotctl &= ~TOPIC_SLOT_CTRL_SWDETECT;
		slotctl |= TOPIC97_SLOT_CTRL_PCIINT;
		slotctl &= ~(TOPIC97_SLOT_CTRL_STSIRQP | TOPIC97_SLOT_CTRL_IRQP);
		DPRINTF(("0x%x\n", slotctl));
		pci_conf_write(pc, tag, TOPIC_SLOT_CTRL, slotctl);
		/* make sure to assert LV card support bits */
		bus_space_write_1(sc->sc_base_memt, sc->sc_base_memh,
		    0x800 + 0x3e,
		    bus_space_read_1(sc->sc_base_memt, sc->sc_base_memh,
			0x800 + 0x3e) | 0x03);
		break;
	}

	/* Close all memory and I/O windows. */
	pci_conf_write(pc, tag, PCI_CB_MEMBASE0, 0xffffffff);
	pci_conf_write(pc, tag, PCI_CB_MEMLIMIT0, 0);
	pci_conf_write(pc, tag, PCI_CB_MEMBASE1, 0xffffffff);
	pci_conf_write(pc, tag, PCI_CB_MEMLIMIT1, 0);
	pci_conf_write(pc, tag, PCI_CB_IOBASE0, 0xffffffff);
	pci_conf_write(pc, tag, PCI_CB_IOLIMIT0, 0);
	pci_conf_write(pc, tag, PCI_CB_IOBASE1, 0xffffffff);
	pci_conf_write(pc, tag, PCI_CB_IOLIMIT1, 0);

	/* reset 16-bit pcmcia bus */
	bus_space_write_1(bmt, bmh, 0x800 + PCIC_INTR,
	    bus_space_read_1(bmt, bmh, 0x800 + PCIC_INTR) & ~PCIC_INTR_RESET);

	/* turn off power */
	pccbb_power(sc, CARDBUS_VCC_0V | CARDBUS_VPP_0V);
}

static void
pccbb_intrinit(struct pccbb_softc *sc)
{
	pcireg_t sockmask;
	const char *intrstr = NULL;
	pci_intr_handle_t ih;
	pci_chipset_tag_t pc = sc->sc_pc;
	bus_space_tag_t bmt = sc->sc_base_memt;
	bus_space_handle_t bmh = sc->sc_base_memh;
	char intrbuf[PCI_INTRSTR_LEN];

	/* Map and establish the interrupt. */
	if (pci_intr_map(&sc->sc_pa, &ih)) {
		aprint_error_dev(sc->sc_dev, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));

	/*
	 * XXX pccbbintr should be called under the priority lower
	 * than any other hard interrupts.
	 */
	KASSERT(sc->sc_ih == NULL);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_BIO, pccbbintr, sc);

	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s\n", intrstr);
		else
			aprint_error("\n");
		return;
	}

	aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);

	/* CSC Interrupt: Card detect and power cycle interrupts on */
	sockmask = bus_space_read_4(bmt, bmh, CB_SOCKET_MASK);
	sockmask |= CB_SOCKET_MASK_CSTS | CB_SOCKET_MASK_CD |
	    CB_SOCKET_MASK_POWER;
	bus_space_write_4(bmt, bmh, CB_SOCKET_MASK, sockmask);
	/* reset interrupt */
	bus_space_write_4(bmt, bmh, CB_SOCKET_EVENT,
	    bus_space_read_4(bmt, bmh, CB_SOCKET_EVENT));
}

/*
 * STATIC void pccbb_pcmcia_attach_setup(struct pccbb_softc *sc,
 *					 struct pcmciabus_attach_args *paa)
 *
 *   This function attaches 16-bit PCcard bus.
 */
STATIC void
pccbb_pcmcia_attach_setup(struct pccbb_softc *sc,
    struct pcmciabus_attach_args *paa)
{
	/*
	 * We need to do a few things here:
	 * 1) Disable routing of CSC and functional interrupts to ISA IRQs by
	 *    setting the IRQ numbers to 0.
	 * 2) Set bit 4 of PCIC_INTR, which is needed on some chips to enable
	 *    routing of CSC interrupts (e.g. card removal) to PCI while in
	 *    PCMCIA mode.  We just leave this set all the time.
	 * 3) Enable card insertion/removal interrupts in case the chip also
	 *    needs that while in PCMCIA mode.
	 * 4) Clear any pending CSC interrupt.
	 */
	Pcic_write(sc, PCIC_INTR, PCIC_INTR_ENABLE);
	if (sc->sc_chipset == CB_TI113X) {
		Pcic_write(sc, PCIC_CSC_INTR, 0);
	} else {
		Pcic_write(sc, PCIC_CSC_INTR, PCIC_CSC_INTR_CD_ENABLE);
		Pcic_read(sc, PCIC_CSC);
	}

	/* initialize pcmcia bus attachment */
	paa->paa_busname = "pcmcia";
	paa->pct = &pccbb_pcmcia_funcs;
	paa->pch = sc;
	return;
}

/*
 * int pccbbintr(arg)
 *    void *arg;
 *   This routine handles the interrupt from Yenta PCI-CardBus bridge
 *   itself.
 */
int
pccbbintr(void *arg)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)arg;
	struct cardslot_softc *csc;
	u_int32_t sockevent, sockstate;
	bus_space_tag_t memt = sc->sc_base_memt;
	bus_space_handle_t memh = sc->sc_base_memh;

	if (!device_has_power(sc->sc_dev))
		return 0;

	sockevent = bus_space_read_4(memt, memh, CB_SOCKET_EVENT);
	bus_space_write_4(memt, memh, CB_SOCKET_EVENT, sockevent);
	Pcic_read(sc, PCIC_CSC);

	if (sockevent != 0) {
		DPRINTF(("%s: enter sockevent %" PRIx32 "\n",
			__func__, sockevent));
	}

	/* XXX sockevent == CB_SOCKET_EVENT_CSTS|CB_SOCKET_EVENT_POWER
	 * does occur in the wild.  Check for a _POWER event before
	 * possibly exiting because of an _CSTS event.
	 */
	if (sockevent & CB_SOCKET_EVENT_POWER) {
		DPRINTF(("Powercycling because of socket event\n"));
		/* XXX: Does not happen when attaching a 16-bit card */
		mutex_enter(&sc->sc_pwr_mtx);
		sc->sc_pwrcycle++;
		cv_signal(&sc->sc_pwr_cv);
		mutex_exit(&sc->sc_pwr_mtx);
	}

	/* Sometimes a change of CSTSCHG# accompanies the first
	 * interrupt from an Atheros WLAN.  That generates a
	 * CB_SOCKET_EVENT_CSTS event on the bridge.  The event
	 * isn't interesting to pccbb(4), so we used to ignore the
	 * interrupt.  Now, let the child devices try to handle
	 * the interrupt, instead.  The Atheros NIC produces
	 * interrupts more reliably, now: used to be that it would
	 * only interrupt if the driver avoided powering down the
	 * NIC's cardslot, and then the NIC would only work after
	 * it was reset a second time.
	 */
	if (sockevent == 0 ||
	    (sockevent & ~(CB_SOCKET_EVENT_POWER|CB_SOCKET_EVENT_CD)) != 0) {
		/* This intr is not for me: it may be for my child devices. */
		if (sc->sc_pil_intr_enable) {
			return pccbbintr_function(sc);
		} else {
			return 0;
		}
	}

	if (sockevent & CB_SOCKET_EVENT_CD) {
		sockstate = bus_space_read_4(memt, memh, CB_SOCKET_STAT);
		if (0x00 != (sockstate & CB_SOCKET_STAT_CD)) {
			/* A card should be removed. */
			if (sc->sc_flags & CBB_CARDEXIST) {
				DPRINTF(("%s: 0x%08x",
				    device_xname(sc->sc_dev), sockevent));
				DPRINTF((" card removed, 0x%08x\n", sockstate));
				sc->sc_flags &= ~CBB_CARDEXIST;
				if ((csc = sc->sc_csc) == NULL)
					;
				else if (csc->sc_status &
				    CARDSLOT_STATUS_CARD_16) {
					cardslot_event_throw(csc,
					    CARDSLOT_EVENT_REMOVAL_16);
				} else if (csc->sc_status &
				    CARDSLOT_STATUS_CARD_CB) {
					/* Cardbus intr removed */
					cardslot_event_throw(csc,
					    CARDSLOT_EVENT_REMOVAL_CB);
				}
			} else if (sc->sc_flags & CBB_INSERTING) {
				sc->sc_flags &= ~CBB_INSERTING;
				callout_stop(&sc->sc_insert_ch);
			}
		} else if (0x00 == (sockstate & CB_SOCKET_STAT_CD) &&
		    /*
		     * The pccbbintr may called from powerdown hook when
		     * the system resumed, to detect the card
		     * insertion/removal during suspension.
		     */
		    (sc->sc_flags & CBB_CARDEXIST) == 0) {
			if (sc->sc_flags & CBB_INSERTING) {
				callout_stop(&sc->sc_insert_ch);
			}
			callout_schedule(&sc->sc_insert_ch, mstohz(200));
			sc->sc_flags |= CBB_INSERTING;
		}
	}

	return (1);
}

/*
 * static int pccbbintr_function(struct pccbb_softc *sc)
 *
 *    This function calls each interrupt handler registered at the
 *    bridge.  The interrupt handlers are called in registered order.
 */
static int
pccbbintr_function(struct pccbb_softc *sc)
{
	int retval = 0, val;
	struct pccbb_intrhand_list *pil;
	int s;

	LIST_FOREACH(pil, &sc->sc_pil, pil_next) {
		s = splraiseipl(pil->pil_icookie);
		val = (*pil->pil_func)(pil->pil_arg);
		splx(s);

		retval = retval == 1 ? 1 :
		    retval == 0 ? val : val != 0 ? val : retval;
	}

	return retval;
}

static void
pci113x_insert(void *arg)
{
	struct pccbb_softc *sc = arg;
	struct cardslot_softc *csc;
	u_int32_t sockevent, sockstate;

	if (!(sc->sc_flags & CBB_INSERTING)) {
		/* We add a card only under inserting state. */
		return;
	}
	sc->sc_flags &= ~CBB_INSERTING;

	sockevent = bus_space_read_4(sc->sc_base_memt, sc->sc_base_memh,
	    CB_SOCKET_EVENT);
	sockstate = bus_space_read_4(sc->sc_base_memt, sc->sc_base_memh,
	    CB_SOCKET_STAT);

	if (0 == (sockstate & CB_SOCKET_STAT_CD)) {	/* card exist */
#ifdef CBB_DEBUG
		DPRINTF(("%s: 0x%08x", device_xname(sc->sc_dev), sockevent));
#else
		__USE(sockevent);
#endif

		DPRINTF((" card inserted, 0x%08x\n", sockstate));
		sc->sc_flags |= CBB_CARDEXIST;
		/* call pccard interrupt handler here */
		if ((csc = sc->sc_csc) == NULL)
			;
		else if (sockstate & CB_SOCKET_STAT_16BIT) {
			/* 16-bit card found */
			cardslot_event_throw(csc, CARDSLOT_EVENT_INSERTION_16);
		} else if (sockstate & CB_SOCKET_STAT_CB) {
			/* cardbus card found */
			cardslot_event_throw(csc, CARDSLOT_EVENT_INSERTION_CB);
		} else {
			/* who are you? */
		}
	} else {
		callout_schedule(&sc->sc_insert_ch, mstohz(100));
	}
}

#define PCCBB_PCMCIA_OFFSET 0x800
static u_int8_t
pccbb_pcmcia_read(struct pccbb_softc *sc, int reg)
{
	bus_space_barrier(sc->sc_base_memt, sc->sc_base_memh,
	    PCCBB_PCMCIA_OFFSET + reg, 1, BUS_SPACE_BARRIER_READ);

	return bus_space_read_1(sc->sc_base_memt, sc->sc_base_memh,
	    PCCBB_PCMCIA_OFFSET + reg);
}

static void
pccbb_pcmcia_write(struct pccbb_softc *sc, int reg, u_int8_t val)
{
	bus_space_write_1(sc->sc_base_memt, sc->sc_base_memh,
			  PCCBB_PCMCIA_OFFSET + reg, val);

	bus_space_barrier(sc->sc_base_memt, sc->sc_base_memh,
	    PCCBB_PCMCIA_OFFSET + reg, 1, BUS_SPACE_BARRIER_WRITE);
}

/*
 * STATIC int pccbb_ctrl(cardbus_chipset_tag_t, int)
 */
STATIC int
pccbb_ctrl(cardbus_chipset_tag_t ct, int command)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)ct;

	switch (command) {
	case CARDBUS_CD:
		if (2 == pccbb_detect_card(sc)) {
			int retval = 0;
			int status = cb_detect_voltage(sc);
			if (PCCARD_VCC_5V & status) {
				retval |= CARDBUS_5V_CARD;
			}
			if (PCCARD_VCC_3V & status) {
				retval |= CARDBUS_3V_CARD;
			}
			if (PCCARD_VCC_XV & status) {
				retval |= CARDBUS_XV_CARD;
			}
			if (PCCARD_VCC_YV & status) {
				retval |= CARDBUS_YV_CARD;
			}
			return retval;
		} else {
			return 0;
		}
	case CARDBUS_RESET:
		return cb_reset(sc);
	case CARDBUS_IO_ENABLE:       /* fallthrough */
	case CARDBUS_IO_DISABLE:      /* fallthrough */
	case CARDBUS_MEM_ENABLE:      /* fallthrough */
	case CARDBUS_MEM_DISABLE:     /* fallthrough */
	case CARDBUS_BM_ENABLE:       /* fallthrough */
	case CARDBUS_BM_DISABLE:      /* fallthrough */
		/* XXX: I think we don't need to call this function below. */
		return pccbb_cardenable(sc, command);
	}

	return 0;
}

STATIC int
pccbb_power_ct(cardbus_chipset_tag_t ct, int command)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)ct;

	return pccbb_power(sc, command);
}

/*
 * STATIC int pccbb_power(cardbus_chipset_tag_t, int)
 *   This function returns true when it succeeds and returns false when
 *   it fails.
 */
STATIC int
pccbb_power(struct pccbb_softc *sc, int command)
{
	u_int32_t status, osock_ctrl, sock_ctrl, reg_ctrl;
	bus_space_tag_t memt = sc->sc_base_memt;
	bus_space_handle_t memh = sc->sc_base_memh;
	int on = 0, pwrcycle, times;
	struct timeval before, after, diff;

	DPRINTF(("pccbb_power: %s and %s [0x%x]\n",
	    (command & CARDBUS_VCCMASK) == CARDBUS_VCC_UC ? "CARDBUS_VCC_UC" :
	    (command & CARDBUS_VCCMASK) == CARDBUS_VCC_5V ? "CARDBUS_VCC_5V" :
	    (command & CARDBUS_VCCMASK) == CARDBUS_VCC_3V ? "CARDBUS_VCC_3V" :
	    (command & CARDBUS_VCCMASK) == CARDBUS_VCC_XV ? "CARDBUS_VCC_XV" :
	    (command & CARDBUS_VCCMASK) == CARDBUS_VCC_YV ? "CARDBUS_VCC_YV" :
	    (command & CARDBUS_VCCMASK) == CARDBUS_VCC_0V ? "CARDBUS_VCC_0V" :
	    "UNKNOWN",
	    (command & CARDBUS_VPPMASK) == CARDBUS_VPP_UC ? "CARDBUS_VPP_UC" :
	    (command & CARDBUS_VPPMASK) == CARDBUS_VPP_12V ? "CARDBUS_VPP_12V" :
	    (command & CARDBUS_VPPMASK) == CARDBUS_VPP_VCC ? "CARDBUS_VPP_VCC" :
	    (command & CARDBUS_VPPMASK) == CARDBUS_VPP_0V ? "CARDBUS_VPP_0V" :
	    "UNKNOWN", command));

	status = bus_space_read_4(memt, memh, CB_SOCKET_STAT);
	osock_ctrl = sock_ctrl = bus_space_read_4(memt, memh, CB_SOCKET_CTRL);

	switch (command & CARDBUS_VCCMASK) {
	case CARDBUS_VCC_UC:
		break;
	case CARDBUS_VCC_5V:
		on++;
		if (CB_SOCKET_STAT_5VCARD & status) {	/* check 5 V card */
			sock_ctrl &= ~CB_SOCKET_CTRL_VCCMASK;
			sock_ctrl |= CB_SOCKET_CTRL_VCC_5V;
		} else {
			aprint_error_dev(sc->sc_dev,
			    "BAD voltage request: no 5 V card\n");
			return 0;
		}
		break;
	case CARDBUS_VCC_3V:
		on++;
		if (CB_SOCKET_STAT_3VCARD & status) {
			sock_ctrl &= ~CB_SOCKET_CTRL_VCCMASK;
			sock_ctrl |= CB_SOCKET_CTRL_VCC_3V;
		} else {
			aprint_error_dev(sc->sc_dev,
			    "BAD voltage request: no 3.3 V card\n");
			return 0;
		}
		break;
	case CARDBUS_VCC_0V:
		sock_ctrl &= ~CB_SOCKET_CTRL_VCCMASK;
		break;
	default:
		return 0;	       /* power NEVER changed */
	}

	switch (command & CARDBUS_VPPMASK) {
	case CARDBUS_VPP_UC:
		break;
	case CARDBUS_VPP_0V:
		sock_ctrl &= ~CB_SOCKET_CTRL_VPPMASK;
		break;
	case CARDBUS_VPP_VCC:
		sock_ctrl &= ~CB_SOCKET_CTRL_VPPMASK;
		sock_ctrl |= ((sock_ctrl >> 4) & 0x07);
		break;
	case CARDBUS_VPP_12V:
		sock_ctrl &= ~CB_SOCKET_CTRL_VPPMASK;
		sock_ctrl |= CB_SOCKET_CTRL_VPP_12V;
		break;
	}
	aprint_debug_dev(sc->sc_dev, "osock_ctrl %#" PRIx32
	    " sock_ctrl %#" PRIx32 "\n", osock_ctrl, sock_ctrl);

	microtime(&before);
	mutex_enter(&sc->sc_pwr_mtx);
	pwrcycle = sc->sc_pwrcycle;

	bus_space_write_4(memt, memh, CB_SOCKET_CTRL, sock_ctrl);

	/*
	 * Wait as long as 200ms for a power-cycle interrupt.  If
	 * interrupts are enabled, but the socket has already
	 * changed to the desired status, keep waiting for the
	 * interrupt.  "Consuming" the interrupt in this way keeps
	 * the interrupt from prematurely waking some subsequent
	 * pccbb_power call.
	 *
	 * XXX Not every bridge interrupts on the ->OFF transition.
	 * XXX That's ok, we will time-out after 200ms.
	 *
	 * XXX The power cycle event will never happen when attaching
	 * XXX a 16-bit card.  That's ok, we will time-out after
	 * XXX 200ms.
	 */
	for (times = 5; --times >= 0; ) {
		if (cold)
			DELAY(40 * 1000);
		else {
			(void)cv_timedwait(&sc->sc_pwr_cv, &sc->sc_pwr_mtx,
			    mstohz(40));
			if (pwrcycle == sc->sc_pwrcycle)
				continue;
		}
		status = bus_space_read_4(memt, memh, CB_SOCKET_STAT);
		if ((status & CB_SOCKET_STAT_PWRCYCLE) != 0 && on)
			break;
		if ((status & CB_SOCKET_STAT_PWRCYCLE) == 0 && !on)
			break;
	}
	mutex_exit(&sc->sc_pwr_mtx);
	microtime(&after);
	timersub(&after, &before, &diff);
	aprint_debug_dev(sc->sc_dev, "wait took%s %lld.%06lds\n",
	    (on && times < 0) ? " too long" : "", (long long)diff.tv_sec,
	    (long)diff.tv_usec);

	/*
	 * Ok, wait a bit longer for things to settle.
	 */
	if (on && sc->sc_chipset == CB_TOPIC95B)
		delay_ms(100, sc);

	status = bus_space_read_4(memt, memh, CB_SOCKET_STAT);

	if (on && sc->sc_chipset != CB_TOPIC95B) {
		if ((status & CB_SOCKET_STAT_PWRCYCLE) == 0)
			aprint_error_dev(sc->sc_dev, "power on failed?\n");
	}

	if (status & CB_SOCKET_STAT_BADVCC) {	/* bad Vcc request */
		aprint_error_dev(sc->sc_dev,
		    "bad Vcc request. sock_ctrl 0x%x, sock_status 0x%x\n",
		    sock_ctrl, status);
		aprint_error_dev(sc->sc_dev, "disabling socket\n");
		sock_ctrl &= ~CB_SOCKET_CTRL_VCCMASK;
		sock_ctrl &= ~CB_SOCKET_CTRL_VPPMASK;
		bus_space_write_4(memt, memh, CB_SOCKET_CTRL, sock_ctrl);
		status &= ~CB_SOCKET_STAT_BADVCC;
		bus_space_write_4(memt, memh, CB_SOCKET_FORCE, status);
		printf("new status 0x%x\n", bus_space_read_4(memt, memh,
		    CB_SOCKET_STAT));
		return 0;
	}

	if (sc->sc_chipset == CB_TOPIC97) {
		reg_ctrl = pci_conf_read(sc->sc_pc, sc->sc_tag, TOPIC_REG_CTRL);
		reg_ctrl &= ~TOPIC97_REG_CTRL_TESTMODE;
		if ((command & CARDBUS_VCCMASK) == CARDBUS_VCC_0V)
			reg_ctrl &= ~TOPIC97_REG_CTRL_CLKRUN_ENA;
		else
			reg_ctrl |= TOPIC97_REG_CTRL_CLKRUN_ENA;
		pci_conf_write(sc->sc_pc, sc->sc_tag, TOPIC_REG_CTRL, reg_ctrl);
	}

	return 1;		       /* power changed correctly */
}

/*
 * static int pccbb_detect_card(struct pccbb_softc *sc)
 *   return value:  0 if no card exists.
 *                  1 if 16-bit card exists.
 *                  2 if cardbus card exists.
 */
static int
pccbb_detect_card(struct pccbb_softc *sc)
{
	bus_space_handle_t base_memh = sc->sc_base_memh;
	bus_space_tag_t base_memt = sc->sc_base_memt;
	u_int32_t sockstat =
	    bus_space_read_4(base_memt, base_memh, CB_SOCKET_STAT);
	int retval = 0;

	/* CD1 and CD2 asserted */
	if (0x00 == (sockstat & CB_SOCKET_STAT_CD)) {
		/* card must be present */
		if (!(CB_SOCKET_STAT_NOTCARD & sockstat)) {
			/* NOTACARD DEASSERTED */
			if (CB_SOCKET_STAT_CB & sockstat) {
				/* CardBus mode */
				retval = 2;
			} else if (CB_SOCKET_STAT_16BIT & sockstat) {
				/* 16-bit mode */
				retval = 1;
			}
		}
	}
	return retval;
}

/*
 * STATIC int cb_reset(struct pccbb_softc *sc)
 *   This function resets CardBus card.
 */
STATIC int
cb_reset(struct pccbb_softc *sc)
{
	/*
	 * Reset Assert at least 20 ms
	 * Some machines request longer duration.
	 */
	int reset_duration =
	    (sc->sc_chipset == CB_RX5C47X ? 400 : 50);
	u_int32_t bcr = pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_BRIDGE_CONTROL_REG);
	aprint_debug("%s: enter bcr %" PRIx32 "\n", __func__, bcr);

	/* Reset bit Assert (bit 6 at 0x3E) */
	bcr |= PCI_BRIDGE_CONTROL_SECBR << PCI_BRIDGE_CONTROL_SHIFT;
	pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_BRIDGE_CONTROL_REG, bcr);
	aprint_debug("%s: wrote bcr %" PRIx32 "\n", __func__, bcr);
	delay_ms(reset_duration, sc);

	if (CBB_CARDEXIST & sc->sc_flags) {	/* A card exists.  Reset it! */
		/* Reset bit Deassert (bit 6 at 0x3E) */
		bcr &= ~(PCI_BRIDGE_CONTROL_SECBR << PCI_BRIDGE_CONTROL_SHIFT);
		pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_BRIDGE_CONTROL_REG,
		    bcr);
		aprint_debug("%s: wrote bcr %" PRIx32 "\n", __func__, bcr);
		delay_ms(reset_duration, sc);
		aprint_debug("%s: end of delay\n", __func__);
	}
	/* No card found on the slot. Keep Reset. */
	return 1;
}

/*
 * STATIC int cb_detect_voltage(struct pccbb_softc *sc)
 *  This function detect card Voltage.
 */
STATIC int
cb_detect_voltage(struct pccbb_softc *sc)
{
	u_int32_t psr;		       /* socket present-state reg */
	bus_space_tag_t iot = sc->sc_base_memt;
	bus_space_handle_t ioh = sc->sc_base_memh;
	int vol = PCCARD_VCC_UKN;      /* set 0 */

	psr = bus_space_read_4(iot, ioh, CB_SOCKET_STAT);

	if (0x400u & psr) {
		vol |= PCCARD_VCC_5V;
	}
	if (0x800u & psr) {
		vol |= PCCARD_VCC_3V;
	}

	return vol;
}

STATIC int
cbbprint(void *aux, const char *pcic)
{
#if 0
	struct cbslot_attach_args *cba = aux;

	if (cba->cba_slot >= 0) {
		aprint_normal(" slot %d", cba->cba_slot);
	}
#endif
	return UNCONF;
}

/*
 * STATIC int pccbb_cardenable(struct pccbb_softc *sc, int function)
 *   This function enables and disables the card
 */
STATIC int
pccbb_cardenable(struct pccbb_softc *sc, int function)
{
	u_int32_t command =
	    pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_COMMAND_STATUS_REG);

	DPRINTF(("pccbb_cardenable:"));
	switch (function) {
	case CARDBUS_IO_ENABLE:
		command |= PCI_COMMAND_IO_ENABLE;
		break;
	case CARDBUS_IO_DISABLE:
		command &= ~PCI_COMMAND_IO_ENABLE;
		break;
	case CARDBUS_MEM_ENABLE:
		command |= PCI_COMMAND_MEM_ENABLE;
		break;
	case CARDBUS_MEM_DISABLE:
		command &= ~PCI_COMMAND_MEM_ENABLE;
		break;
	case CARDBUS_BM_ENABLE:
		command |= PCI_COMMAND_MASTER_ENABLE;
		break;
	case CARDBUS_BM_DISABLE:
		command &= ~PCI_COMMAND_MASTER_ENABLE;
		break;
	default:
		return 0;
	}

	pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_COMMAND_STATUS_REG, command);
	DPRINTF((" command reg 0x%x\n", command));
	return 1;
}

#if !rbus
static int
pccbb_io_open(cardbus_chipset_tag_t ct, int win, uint32_t start, uint32_t end)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)ct;
	int basereg;
	int limitreg;

	if ((win < 0) || (win > 2)) {
#if defined DIAGNOSTIC
		printf("cardbus_io_open: window out of range %d\n", win);
#endif
		return 0;
	}

	basereg = win * 8 + PCI_CB_IOBASE0;
	limitreg = win * 8 + PCI_CB_IOLIMIT0;

	DPRINTF(("pccbb_io_open: 0x%x[0x%x] - 0x%x[0x%x]\n",
	    start, basereg, end, limitreg));

	pci_conf_write(sc->sc_pc, sc->sc_tag, basereg, start);
	pci_conf_write(sc->sc_pc, sc->sc_tag, limitreg, end);
	return 1;
}

/*
 * int pccbb_io_close(cardbus_chipset_tag_t, int)
 */
static int
pccbb_io_close(cardbus_chipset_tag_t ct, int win)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)ct;
	int basereg;
	int limitreg;

	if ((win < 0) || (win > 2)) {
#if defined DIAGNOSTIC
		printf("cardbus_io_close: window out of range %d\n", win);
#endif
		return 0;
	}

	basereg = win * 8 + PCI_CB_IOBASE0;
	limitreg = win * 8 + PCI_CB_IOLIMIT0;

	pci_conf_write(sc->sc_pc, sc->sc_tag, basereg, 0);
	pci_conf_write(sc->sc_pc, sc->sc_tag, limitreg, 0);
	return 1;
}

static int
pccbb_mem_open(cardbus_chipset_tag_t ct, int win, uint32_t start, uint32_t end)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)ct;
	int basereg;
	int limitreg;

	if ((win < 0) || (win > 2)) {
#if defined DIAGNOSTIC
		printf("cardbus_mem_open: window out of range %d\n", win);
#endif
		return 0;
	}

	basereg = win * 8 + PCI_CB_MEMBASE0;
	limitreg = win * 8 + PCI_CB_MEMLIMIT0;

	pci_conf_write(sc->sc_pc, sc->sc_tag, basereg, start);
	pci_conf_write(sc->sc_pc, sc->sc_tag, limitreg, end);
	return 1;
}

static int
pccbb_mem_close(cardbus_chipset_tag_t ct, int win)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)ct;
	int basereg;
	int limitreg;

	if ((win < 0) || (win > 2)) {
#if defined DIAGNOSTIC
		printf("cardbus_mem_close: window out of range %d\n", win);
#endif
		return 0;
	}

	basereg = win * 8 + PCI_CB_MEMBASE0;
	limitreg = win * 8 + PCI_CB_MEMLIMIT0;

	pci_conf_write(sc->sc_pc, sc->sc_tag, basereg, 0);
	pci_conf_write(sc->sc_pc, sc->sc_tag, limitreg, 0);
	return 1;
}
#endif

/*
 * static void *pccbb_cb_intr_establish(cardbus_chipset_tag_t ct,
 *					int level,
 *					int (* func)(void *),
 *					void *arg)
 *
 *   This function registers an interrupt handler at the bridge, in
 *   order not to call the interrupt handlers of child devices when
 *   a card-deletion interrupt occurs.
 *
 *   The argument level is not used.
 */
static void *
pccbb_cb_intr_establish(cardbus_chipset_tag_t ct, int level,
    int (*func)(void *), void *arg)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)ct;

	return pccbb_intr_establish(sc, level, func, arg);
}


/*
 * static void *pccbb_cb_intr_disestablish(cardbus_chipset_tag_t ct,
 *					   void *ih)
 *
 *   This function removes an interrupt handler pointed by ih.
 */
static void
pccbb_cb_intr_disestablish(cardbus_chipset_tag_t ct, void *ih)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)ct;

	pccbb_intr_disestablish(sc, ih);
}


void
pccbb_intr_route(struct pccbb_softc *sc)
{
	pcireg_t bcr, cbctrl;

	/* initialize bridge intr routing */
	bcr = pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_BRIDGE_CONTROL_REG);
	bcr &= ~CB_BCR_INTR_IREQ_ENABLE;
	pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_BRIDGE_CONTROL_REG, bcr);

	switch (sc->sc_chipset) {
	case CB_TI113X:
		cbctrl = pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_CBCTRL);
		/* functional intr enabled */
		cbctrl |= PCI113X_CBCTRL_PCI_INTR;
		pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_CBCTRL, cbctrl);
		break;
	default:
		break;
	}
}

/*
 * static void *pccbb_intr_establish(struct pccbb_softc *sc,
 *				     int irq,
 *				     int level,
 *				     int (* func)(void *),
 *				     void *arg)
 *
 *   This function registers an interrupt handler at the bridge, in
 *   order not to call the interrupt handlers of child devices when
 *   a card-deletion interrupt occurs.
 *
 */
static void *
pccbb_intr_establish(struct pccbb_softc *sc, int level,
    int (*func)(void *), void *arg)
{
	struct pccbb_intrhand_list *pil, *newpil;

	DPRINTF(("pccbb_intr_establish start. %p\n", LIST_FIRST(&sc->sc_pil)));

	if (LIST_EMPTY(&sc->sc_pil)) {
		pccbb_intr_route(sc);
	}

	/*
	 * Allocate a room for interrupt handler structure.
	 */
	if (NULL == (newpil =
	    (struct pccbb_intrhand_list *)malloc(sizeof(struct
	    pccbb_intrhand_list), M_DEVBUF, M_WAITOK))) {
		return NULL;
	}

	newpil->pil_func = func;
	newpil->pil_arg = arg;
	newpil->pil_icookie = makeiplcookie(level);

	if (LIST_EMPTY(&sc->sc_pil)) {
		LIST_INSERT_HEAD(&sc->sc_pil, newpil, pil_next);
	} else {
		for (pil = LIST_FIRST(&sc->sc_pil);
		     LIST_NEXT(pil, pil_next) != NULL;
		     pil = LIST_NEXT(pil, pil_next));
		LIST_INSERT_AFTER(pil, newpil, pil_next);
	}

	DPRINTF(("pccbb_intr_establish add pil. %p\n",
	    LIST_FIRST(&sc->sc_pil)));

	return newpil;
}

/*
 * static void *pccbb_intr_disestablish(struct pccbb_softc *sc,
 *					void *ih)
 *
 *	This function removes an interrupt handler pointed by ih.  ih
 *	should be the value returned by cardbus_intr_establish() or
 *	NULL.
 *
 *	When ih is NULL, this function will do nothing.
 */
static void
pccbb_intr_disestablish(struct pccbb_softc *sc, void *ih)
{
	struct pccbb_intrhand_list *pil;
	pcireg_t reg;

	DPRINTF(("pccbb_intr_disestablish start. %p\n",
	    LIST_FIRST(&sc->sc_pil)));

	if (ih == NULL) {
		/* intr handler is not set */
		DPRINTF(("pccbb_intr_disestablish: no ih\n"));
		return;
	}

#ifdef DIAGNOSTIC
	LIST_FOREACH(pil, &sc->sc_pil, pil_next) {
		DPRINTF(("pccbb_intr_disestablish: pil %p\n", pil));
		if (pil == ih) {
			DPRINTF(("pccbb_intr_disestablish frees one pil\n"));
			break;
		}
	}
	if (pil == NULL) {
		panic("pccbb_intr_disestablish: %s cannot find pil %p",
		    device_xname(sc->sc_dev), ih);
	}
#endif

	pil = (struct pccbb_intrhand_list *)ih;
	LIST_REMOVE(pil, pil_next);
	free(pil, M_DEVBUF);
	DPRINTF(("pccbb_intr_disestablish frees one pil\n"));

	if (LIST_EMPTY(&sc->sc_pil)) {
		/* No interrupt handlers */

		DPRINTF(("pccbb_intr_disestablish: no interrupt handler\n"));

		/* stop routing PCI intr */
		reg = pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_BRIDGE_CONTROL_REG);
		reg |= CB_BCR_INTR_IREQ_ENABLE;
		pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_BRIDGE_CONTROL_REG, reg);

		switch (sc->sc_chipset) {
		case CB_TI113X:
			reg = pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_CBCTRL);
			/* functional intr disabled */
			reg &= ~PCI113X_CBCTRL_PCI_INTR;
			pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_CBCTRL, reg);
			break;
		default:
			break;
		}
	}
}

#if defined SHOW_REGS
static void
cb_show_regs(pci_chipset_tag_t pc, pcitag_t tag, bus_space_tag_t memt,
    bus_space_handle_t memh)
{
	int i;
	printf("PCI config regs:");
	for (i = 0; i < 0x50; i += 4) {
		if (i % 16 == 0)
			printf("\n 0x%02x:", i);
		printf(" %08x", pci_conf_read(pc, tag, i));
	}
	for (i = 0x80; i < 0xb0; i += 4) {
		if (i % 16 == 0)
			printf("\n 0x%02x:", i);
		printf(" %08x", pci_conf_read(pc, tag, i));
	}

	if (memh == 0) {
		printf("\n");
		return;
	}

	printf("\nsocket regs:");
	for (i = 0; i <= 0x10; i += 0x04)
		printf(" %08x", bus_space_read_4(memt, memh, i));
	printf("\nExCA regs:");
	for (i = 0; i < 0x08; ++i)
		printf(" %02x", bus_space_read_1(memt, memh, 0x800 + i));
	printf("\n");
	return;
}
#endif

/*
 * static pcitag_t pccbb_make_tag(cardbus_chipset_tag_t cc,
 *                                    int busno, int function)
 *   This is the function to make a tag to access config space of
 *  a CardBus Card.  It works same as pci_conf_read.
 */
static pcitag_t
pccbb_make_tag(cardbus_chipset_tag_t cc, int busno, int function)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)cc;

	return pci_make_tag(sc->sc_pc, busno, 0, function);
}

/*
 * pccbb_conf_read
 *
 * This is the function to read the config space of a CardBus card.
 * It works the same as pci_conf_read(9).
 */
static pcireg_t
pccbb_conf_read(cardbus_chipset_tag_t cc, pcitag_t tag, int offset)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)cc;
	pcitag_t brtag = sc->sc_tag;
	pcireg_t reg;

	/*
	 * clear cardbus master abort status; it is OK to write without
	 * reading before because all bits are r/o or w1tc
	 */
	pci_conf_write(sc->sc_pc, brtag, PCI_CBB_SECSTATUS,
		       CBB_SECSTATUS_CBMABORT);
	reg = pci_conf_read(sc->sc_pc, tag, offset);
	/* check cardbus master abort status */
	if (pci_conf_read(sc->sc_pc, brtag, PCI_CBB_SECSTATUS)
			  & CBB_SECSTATUS_CBMABORT)
		return (0xffffffff);
	return reg;
}

/*
 * pccbb_conf_write
 *
 * This is the function to write the config space of a CardBus
 * card.  It works the same as pci_conf_write(9).
 */
static void
pccbb_conf_write(cardbus_chipset_tag_t cc, pcitag_t tag, int reg, pcireg_t val)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)cc;

	pci_conf_write(sc->sc_pc, tag, reg, val);
}

#if 0
STATIC int
pccbb_new_pcmcia_io_alloc(pcmcia_chipset_handle_t pch,
    bus_addr_t start, bus_size_t size, bus_size_t align, bus_addr_t mask,
    int speed, int flags,
    bus_space_handle_t * iohp)
#endif
/*
 * STATIC int pccbb_pcmcia_io_alloc(pcmcia_chipset_handle_t pch,
 *                                  bus_addr_t start, bus_size_t size,
 *                                  bus_size_t align,
 *                                  struct pcmcia_io_handle *pcihp
 *
 * This function only allocates I/O region for pccard. This function
 * never maps the allocated region to pccard I/O area.
 *
 * XXX: The interface of this function is not very good, I believe.
 */
STATIC int
pccbb_pcmcia_io_alloc(pcmcia_chipset_handle_t pch, bus_addr_t start,
    bus_size_t size, bus_size_t align, struct pcmcia_io_handle *pcihp)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)pch;
	bus_addr_t ioaddr;
	int flags = 0;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_addr_t mask;
#if rbus
	rbus_tag_t rb;
#endif
	if (align == 0) {
		align = size;	       /* XXX: funny??? */
	}

	if (start != 0) {
		/* XXX: assume all card decode lower 10 bits by its hardware */
		mask = 0x3ff;
		/* enforce to use only masked address */
		start &= mask;
	} else {
		/*
		 * calculate mask:
		 *  1. get the most significant bit of size (call it msb).
		 *  2. compare msb with the value of size.
		 *  3. if size is larger, shift msb left once.
		 *  4. obtain mask value to decrement msb.
		 */
		bus_size_t size_tmp = size;
		int shifts = 0;

		mask = 1;
		while (size_tmp) {
			++shifts;
			size_tmp >>= 1;
		}
		mask = (1 << shifts);
		if (mask < size) {
			mask <<= 1;
		}
		--mask;
	}

	/*
	 * Allocate some arbitrary I/O space.
	 */

	iot = sc->sc_iot;

#if rbus
	rb = sc->sc_rbus_iot;
	if (rbus_space_alloc(rb, start, size, mask, align, 0, &ioaddr, &ioh)) {
		return 1;
	}
	DPRINTF(("pccbb_pcmcia_io_alloc alloc port 0x%lx+0x%lx\n",
	    (u_long) ioaddr, (u_long) size));
#else
	if (start) {
		ioaddr = start;
		if (bus_space_map(iot, start, size, 0, &ioh)) {
			return 1;
		}
		DPRINTF(("pccbb_pcmcia_io_alloc map port 0x%lx+0x%lx\n",
		    (u_long) ioaddr, (u_long) size));
	} else {
		flags |= PCMCIA_IO_ALLOCATED;
		if (bus_space_alloc(iot, 0x700 /* ph->sc->sc_iobase */ ,
		    0x800,	/* ph->sc->sc_iobase + ph->sc->sc_iosize */
		    size, align, 0, 0, &ioaddr, &ioh)) {
			/* No room be able to be get. */
			return 1;
		}
		DPRINTF(("pccbb_pcmmcia_io_alloc alloc port 0x%lx+0x%lx\n",
		    (u_long) ioaddr, (u_long) size));
	}
#endif

	pcihp->iot = iot;
	pcihp->ioh = ioh;
	pcihp->addr = ioaddr;
	pcihp->size = size;
	pcihp->flags = flags;

	return 0;
}

/*
 * STATIC int pccbb_pcmcia_io_free(pcmcia_chipset_handle_t pch,
 *                                 struct pcmcia_io_handle *pcihp)
 *
 * This function only frees I/O region for pccard.
 *
 * XXX: The interface of this function is not very good, I believe.
 */
void
pccbb_pcmcia_io_free(pcmcia_chipset_handle_t pch,
    struct pcmcia_io_handle *pcihp)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)pch;
#if !rbus
	bus_space_tag_t iot = pcihp->iot;
#endif
	bus_space_handle_t ioh = pcihp->ioh;
	bus_size_t size = pcihp->size;

#if rbus
	rbus_tag_t rb = sc->sc_rbus_iot;

	rbus_space_free(rb, ioh, size, NULL);
#else
	if (pcihp->flags & PCMCIA_IO_ALLOCATED)
		bus_space_free(iot, ioh, size);
	else
		bus_space_unmap(iot, ioh, size);
#endif
}

/*
 * STATIC int pccbb_pcmcia_io_map(pcmcia_chipset_handle_t pch, int width,
 *                                bus_addr_t offset, bus_size_t size,
 *                                struct pcmcia_io_handle *pcihp,
 *                                int *windowp)
 *
 * This function maps the allocated I/O region to pccard. This function
 * never allocates any I/O region for pccard I/O area.  I don't
 * understand why the original authors of pcmciabus separated alloc and
 * map.  I believe the two must be unite.
 *
 * XXX: no wait timing control?
 */
int
pccbb_pcmcia_io_map(pcmcia_chipset_handle_t pch, int width, bus_addr_t offset,
    bus_size_t size, struct pcmcia_io_handle *pcihp, int *windowp)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)pch;
	struct pcic_handle *ph = &sc->sc_pcmcia_h;
	bus_addr_t ioaddr = pcihp->addr + offset;
	int i, win;
#if defined CBB_DEBUG
	static const char *width_names[] = { "dynamic", "io8", "io16" };
#endif

	/* Sanity check I/O handle. */

	if (!bus_space_is_equal(sc->sc_iot, pcihp->iot)) {
		panic("pccbb_pcmcia_io_map iot is bogus");
	}

	/* XXX Sanity check offset/size. */

	win = -1;
	for (i = 0; i < PCIC_IO_WINS; i++) {
		if ((ph->ioalloc & (1 << i)) == 0) {
			win = i;
			ph->ioalloc |= (1 << i);
			break;
		}
	}

	if (win == -1) {
		return 1;
	}

	*windowp = win;

	/* XXX this is pretty gross */

	DPRINTF(("pccbb_pcmcia_io_map window %d %s port %lx+%lx\n",
	    win, width_names[width], (u_long) ioaddr, (u_long) size));

	/* XXX wtf is this doing here? */

#if 0
	printf(" port 0x%lx", (u_long) ioaddr);
	if (size > 1) {
		printf("-0x%lx", (u_long) ioaddr + (u_long) size - 1);
	}
#endif

	ph->io[win].addr = ioaddr;
	ph->io[win].size = size;
	ph->io[win].width = width;

	/* actual dirty register-value changing in the function below. */
	pccbb_pcmcia_do_io_map(sc, win);

	return 0;
}

/*
 * STATIC void pccbb_pcmcia_do_io_map(struct pcic_handle *h, int win)
 *
 * This function changes register-value to map I/O region for pccard.
 */
static void
pccbb_pcmcia_do_io_map(struct pccbb_softc *sc, int win)
{
	static u_int8_t pcic_iowidth[3] = {
		PCIC_IOCTL_IO0_IOCS16SRC_CARD,
		PCIC_IOCTL_IO0_IOCS16SRC_DATASIZE |
		    PCIC_IOCTL_IO0_DATASIZE_8BIT,
		PCIC_IOCTL_IO0_IOCS16SRC_DATASIZE |
		    PCIC_IOCTL_IO0_DATASIZE_16BIT,
	};

#define PCIC_SIA_START_LOW 0
#define PCIC_SIA_START_HIGH 1
#define PCIC_SIA_STOP_LOW 2
#define PCIC_SIA_STOP_HIGH 3

	int regbase_win = 0x8 + win * 0x04;
	u_int8_t ioctl, enable;
	struct pcic_handle *ph = &sc->sc_pcmcia_h;

	DPRINTF(("pccbb_pcmcia_do_io_map win %d addr 0x%lx size 0x%lx "
	    "width %d\n", win, (unsigned long)ph->io[win].addr,
	    (unsigned long)ph->io[win].size, ph->io[win].width * 8));

	Pcic_write(sc, regbase_win + PCIC_SIA_START_LOW,
	    ph->io[win].addr & 0xff);
	Pcic_write(sc, regbase_win + PCIC_SIA_START_HIGH,
	    (ph->io[win].addr >> 8) & 0xff);

	Pcic_write(sc, regbase_win + PCIC_SIA_STOP_LOW,
	    (ph->io[win].addr + ph->io[win].size - 1) & 0xff);
	Pcic_write(sc, regbase_win + PCIC_SIA_STOP_HIGH,
	    ((ph->io[win].addr + ph->io[win].size - 1) >> 8) & 0xff);

	ioctl = Pcic_read(sc, PCIC_IOCTL);
	enable = Pcic_read(sc, PCIC_ADDRWIN_ENABLE);
	switch (win) {
	case 0:
		ioctl &= ~(PCIC_IOCTL_IO0_WAITSTATE | PCIC_IOCTL_IO0_ZEROWAIT |
		    PCIC_IOCTL_IO0_IOCS16SRC_MASK |
		    PCIC_IOCTL_IO0_DATASIZE_MASK);
		ioctl |= pcic_iowidth[ph->io[win].width];
		enable |= PCIC_ADDRWIN_ENABLE_IO0;
		break;
	case 1:
		ioctl &= ~(PCIC_IOCTL_IO1_WAITSTATE | PCIC_IOCTL_IO1_ZEROWAIT |
		    PCIC_IOCTL_IO1_IOCS16SRC_MASK |
		    PCIC_IOCTL_IO1_DATASIZE_MASK);
		ioctl |= (pcic_iowidth[ph->io[win].width] << 4);
		enable |= PCIC_ADDRWIN_ENABLE_IO1;
		break;
	}
	Pcic_write(sc, PCIC_IOCTL, ioctl);
	Pcic_write(sc, PCIC_ADDRWIN_ENABLE, enable);
#if defined(CBB_DEBUG)
	{
		u_int8_t start_low =
		    Pcic_read(sc, regbase_win + PCIC_SIA_START_LOW);
		u_int8_t start_high =
		    Pcic_read(sc, regbase_win + PCIC_SIA_START_HIGH);
		u_int8_t stop_low =
		    Pcic_read(sc, regbase_win + PCIC_SIA_STOP_LOW);
		u_int8_t stop_high =
		    Pcic_read(sc, regbase_win + PCIC_SIA_STOP_HIGH);
		printf("pccbb_pcmcia_do_io_map start %02x %02x, "
		    "stop %02x %02x, ioctl %02x enable %02x\n",
		    start_low, start_high, stop_low, stop_high, ioctl, enable);
	}
#endif
}

/*
 * STATIC void pccbb_pcmcia_io_unmap(pcmcia_chipset_handle_t *h, int win)
 *
 * This function unmaps I/O region.  No return value.
 */
STATIC void
pccbb_pcmcia_io_unmap(pcmcia_chipset_handle_t pch, int win)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)pch;
	struct pcic_handle *ph = &sc->sc_pcmcia_h;
	int reg;

	if (win >= PCIC_IO_WINS || win < 0) {
		panic("pccbb_pcmcia_io_unmap: window out of range");
	}

	reg = Pcic_read(sc, PCIC_ADDRWIN_ENABLE);
	switch (win) {
	case 0:
		reg &= ~PCIC_ADDRWIN_ENABLE_IO0;
		break;
	case 1:
		reg &= ~PCIC_ADDRWIN_ENABLE_IO1;
		break;
	}
	Pcic_write(sc, PCIC_ADDRWIN_ENABLE, reg);

	ph->ioalloc &= ~(1 << win);
}

static int
pccbb_pcmcia_wait_ready(struct pccbb_softc *sc)
{
	u_int8_t stat;
	int i;

	/* wait an initial 10ms for quick cards */
	stat = Pcic_read(sc, PCIC_IF_STATUS);
	if (stat & PCIC_IF_STATUS_READY)
		return (0);
	pccbb_pcmcia_delay(sc, 10, "pccwr0");
	for (i = 0; i < 50; i++) {
		stat = Pcic_read(sc, PCIC_IF_STATUS);
		if (stat & PCIC_IF_STATUS_READY)
			return (0);
		if ((stat & PCIC_IF_STATUS_CARDDETECT_MASK) !=
		    PCIC_IF_STATUS_CARDDETECT_PRESENT)
			return (ENXIO);
		/* wait .1s (100ms) each iteration now */
		pccbb_pcmcia_delay(sc, 100, "pccwr1");
	}

	printf("pccbb_pcmcia_wait_ready: ready never happened, status=%02x\n", stat);
	return (EWOULDBLOCK);
}

/*
 * Perform long (msec order) delay.  timo is in milliseconds.
 */
static void
pccbb_pcmcia_delay(struct pccbb_softc *sc, int timo, const char *wmesg)
{
#ifdef DIAGNOSTIC
	if (timo <= 0)
		panic("pccbb_pcmcia_delay: called with timeout %d", timo);
	if (!curlwp)
		panic("pccbb_pcmcia_delay: called in interrupt context");
#endif
	DPRINTF(("pccbb_pcmcia_delay: \"%s\", sleep %d ms\n", wmesg, timo));
	kpause(wmesg, false, max(mstohz(timo), 1), NULL);
}

/*
 * STATIC void pccbb_pcmcia_socket_enable(pcmcia_chipset_handle_t pch)
 *
 * This function enables the card.  All information is stored in
 * the first argument, pcmcia_chipset_handle_t.
 */
STATIC void
pccbb_pcmcia_socket_enable(pcmcia_chipset_handle_t pch)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)pch;
	struct pcic_handle *ph = &sc->sc_pcmcia_h;
	pcireg_t spsr;
	int voltage;
	int win;
	u_int8_t power, intr;
#ifdef DIAGNOSTIC
	int reg;
#endif

	/* this bit is mostly stolen from pcic_attach_card */

	DPRINTF(("pccbb_pcmcia_socket_enable: "));

	/* get card Vcc info */
	spsr =
	    bus_space_read_4(sc->sc_base_memt, sc->sc_base_memh,
	    CB_SOCKET_STAT);
	if (spsr & CB_SOCKET_STAT_5VCARD) {
		DPRINTF(("5V card\n"));
		voltage = CARDBUS_VCC_5V | CARDBUS_VPP_VCC;
	} else if (spsr & CB_SOCKET_STAT_3VCARD) {
		DPRINTF(("3V card\n"));
		voltage = CARDBUS_VCC_3V | CARDBUS_VPP_VCC;
	} else {
		DPRINTF(("?V card, 0x%x\n", spsr));	/* XXX */
		return;
	}

	/* disable interrupts; assert RESET */
	intr = Pcic_read(sc, PCIC_INTR);
	intr &= PCIC_INTR_ENABLE;
	Pcic_write(sc, PCIC_INTR, intr);

	/* zero out the address windows */
	Pcic_write(sc, PCIC_ADDRWIN_ENABLE, 0);

	/* power down the socket to reset it, clear the card reset pin */
	pccbb_power(sc, CARDBUS_VCC_0V | CARDBUS_VPP_0V);

	/* power off; assert output enable bit */
	power = PCIC_PWRCTL_OE;
	Pcic_write(sc, PCIC_PWRCTL, power);

	/* power up the socket */
	if (pccbb_power(sc, voltage) == 0)
		return;

	/*
	 * Table 4-18 and figure 4-6 of the PC Card specifiction say:
	 * Vcc Rising Time (Tpr) = 100ms (handled in pccbb_power() above)
	 * RESET Width (Th (Hi-z RESET)) = 1ms
	 * RESET Width (Tw (RESET)) = 10us
	 *
	 * some machines require some more time to be settled
	 * for example old toshiba topic bridges!
	 * (100ms is added here).
	 */
	pccbb_pcmcia_delay(sc, 200 + 1, "pccen1");

	/* negate RESET */
	intr |= PCIC_INTR_RESET;
	Pcic_write(sc, PCIC_INTR, intr);

	/*
	 * RESET Setup Time (Tsu (RESET)) = 20ms
	 */
	pccbb_pcmcia_delay(sc, 20, "pccen2");

#ifdef DIAGNOSTIC
	reg = Pcic_read(sc, PCIC_IF_STATUS);
	if ((reg & PCIC_IF_STATUS_POWERACTIVE) == 0)
		printf("pccbb_pcmcia_socket_enable: no power, status=%x\n", reg);
#endif

	/* wait for the chip to finish initializing */
	if (pccbb_pcmcia_wait_ready(sc)) {
#ifdef DIAGNOSTIC
		printf("pccbb_pcmcia_socket_enable: never became ready\n");
#endif
		/* XXX return a failure status?? */
		pccbb_power(sc, CARDBUS_VCC_0V | CARDBUS_VPP_0V);
		Pcic_write(sc, PCIC_PWRCTL, 0);
		return;
	}

	/* reinstall all the memory and io mappings */
	for (win = 0; win < PCIC_MEM_WINS; ++win)
		if (ph->memalloc & (1 << win))
			pccbb_pcmcia_do_mem_map(sc, win);
	for (win = 0; win < PCIC_IO_WINS; ++win)
		if (ph->ioalloc & (1 << win))
			pccbb_pcmcia_do_io_map(sc, win);
}

/*
 * STATIC void pccbb_pcmcia_socket_disable(pcmcia_chipset_handle_t *ph)
 *
 * This function disables the card.  All information is stored in
 * the first argument, pcmcia_chipset_handle_t.
 */
STATIC void
pccbb_pcmcia_socket_disable(pcmcia_chipset_handle_t pch)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)pch;
	u_int8_t intr;

	DPRINTF(("pccbb_pcmcia_socket_disable\n"));

	/* disable interrupts; assert RESET */
	intr = Pcic_read(sc, PCIC_INTR);
	intr &= PCIC_INTR_ENABLE;
	Pcic_write(sc, PCIC_INTR, intr);

	/* zero out the address windows */
	Pcic_write(sc, PCIC_ADDRWIN_ENABLE, 0);

	/* power down the socket to reset it, clear the card reset pin */
	pccbb_power(sc, CARDBUS_VCC_0V | CARDBUS_VPP_0V);

	/* disable socket: negate output enable bit and power off */
	Pcic_write(sc, PCIC_PWRCTL, 0);

	/*
	 * Vcc Falling Time (Tpf) = 300ms
	 */
	pccbb_pcmcia_delay(sc, 300, "pccwr1");
}

STATIC void
pccbb_pcmcia_socket_settype(pcmcia_chipset_handle_t pch, int type)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)pch;
	u_int8_t intr;

	/* set the card type */

	intr = Pcic_read(sc, PCIC_INTR);
	intr &= ~(PCIC_INTR_IRQ_MASK | PCIC_INTR_CARDTYPE_MASK);
	if (type == PCMCIA_IFTYPE_IO)
		intr |= PCIC_INTR_CARDTYPE_IO;
	else
		intr |= PCIC_INTR_CARDTYPE_MEM;
	Pcic_write(sc, PCIC_INTR, intr);

	DPRINTF(("%s: pccbb_pcmcia_socket_settype type %s %02x\n",
	    device_xname(sc->sc_dev),
	    ((type == PCMCIA_IFTYPE_IO) ? "io" : "mem"), intr));
}

/*
 * STATIC int pccbb_pcmcia_card_detect(pcmcia_chipset_handle_t *ph)
 *
 * This function detects whether a card is in the slot or not.
 * If a card is inserted, return 1.  Otherwise, return 0.
 */
STATIC int
pccbb_pcmcia_card_detect(pcmcia_chipset_handle_t pch)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)pch;

	DPRINTF(("pccbb_pcmcia_card_detect\n"));
	return pccbb_detect_card(sc) == 1 ? 1 : 0;
}

#if 0
STATIC int
pccbb_new_pcmcia_mem_alloc(pcmcia_chipset_handle_t pch,
    bus_addr_t start, bus_size_t size, bus_size_t align, int speed, int flags,
    bus_space_tag_t * memtp bus_space_handle_t * memhp)
#endif
/*
 * STATIC int pccbb_pcmcia_mem_alloc(pcmcia_chipset_handle_t pch,
 *                                   bus_size_t size,
 *                                   struct pcmcia_mem_handle *pcmhp)
 *
 * This function only allocates memory region for pccard. This
 * function never maps the allocated region to pccard memory area.
 *
 * XXX: Why the argument of start address is not in?
 */
STATIC int
pccbb_pcmcia_mem_alloc(pcmcia_chipset_handle_t pch, bus_size_t size,
    struct pcmcia_mem_handle *pcmhp)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)pch;
	bus_space_handle_t memh;
	bus_addr_t addr;
	bus_size_t sizepg;
#if rbus
	rbus_tag_t rb;
#endif

	/* Check that the card is still there. */
	if ((Pcic_read(sc, PCIC_IF_STATUS) & PCIC_IF_STATUS_CARDDETECT_MASK) !=
		    PCIC_IF_STATUS_CARDDETECT_PRESENT)
		return 1;

	/* out of sc->memh, allocate as many pages as necessary */

	/* convert size to PCIC pages */
	/*
	 * This is not enough; when the requested region is on the page
	 * boundaries, this may calculate wrong result.
	 */
	sizepg = (size + (PCIC_MEM_PAGESIZE - 1)) / PCIC_MEM_PAGESIZE;
#if 0
	if (sizepg > PCIC_MAX_MEM_PAGES) {
		return 1;
	}
#endif

	if (!(sc->sc_pcmcia_flags & PCCBB_PCMCIA_MEM_32)) {
		return 1;
	}

	addr = 0;		       /* XXX gcc -Wuninitialized */

#if rbus
	rb = sc->sc_rbus_memt;
	if (rbus_space_alloc(rb, 0, sizepg * PCIC_MEM_PAGESIZE,
	    sizepg * PCIC_MEM_PAGESIZE - 1, PCIC_MEM_PAGESIZE, 0,
	    &addr, &memh)) {
		return 1;
	}
#else
	if (bus_space_alloc(sc->sc_memt, sc->sc_mem_start, sc->sc_mem_end,
	    sizepg * PCIC_MEM_PAGESIZE, PCIC_MEM_PAGESIZE,
	    0, /* boundary */
	    0,	/* flags */
	    &addr, &memh)) {
		return 1;
	}
#endif

	DPRINTF(("pccbb_pcmcia_alloc_mem: addr 0x%lx size 0x%lx, "
	    "realsize 0x%lx\n", (unsigned long)addr, (unsigned long)size,
	    (unsigned long)sizepg * PCIC_MEM_PAGESIZE));

	pcmhp->memt = sc->sc_memt;
	pcmhp->memh = memh;
	pcmhp->addr = addr;
	pcmhp->size = size;
	pcmhp->realsize = sizepg * PCIC_MEM_PAGESIZE;
	/* What is mhandle?  I feel it is very dirty and it must go trush. */
	pcmhp->mhandle = 0;
	/* No offset???  Funny. */

	return 0;
}

/*
 * STATIC void pccbb_pcmcia_mem_free(pcmcia_chipset_handle_t pch,
 *                                   struct pcmcia_mem_handle *pcmhp)
 *
 * This function release the memory space allocated by the function
 * pccbb_pcmcia_mem_alloc().
 */
STATIC void
pccbb_pcmcia_mem_free(pcmcia_chipset_handle_t pch,
    struct pcmcia_mem_handle *pcmhp)
{
#if rbus
	struct pccbb_softc *sc = (struct pccbb_softc *)pch;

	rbus_space_free(sc->sc_rbus_memt, pcmhp->memh, pcmhp->realsize, NULL);
#else
	bus_space_free(pcmhp->memt, pcmhp->memh, pcmhp->realsize);
#endif
}

/*
 * STATIC void pccbb_pcmcia_do_mem_map(struct pcic_handle *ph, int win)
 *
 * This function release the memory space allocated by the function
 * pccbb_pcmcia_mem_alloc().
 */
STATIC void
pccbb_pcmcia_do_mem_map(struct pccbb_softc *sc, int win)
{
	int regbase_win;
	bus_addr_t phys_addr;
	bus_addr_t phys_end;
	struct pcic_handle *ph = &sc->sc_pcmcia_h;

#define PCIC_SMM_START_LOW 0
#define PCIC_SMM_START_HIGH 1
#define PCIC_SMM_STOP_LOW 2
#define PCIC_SMM_STOP_HIGH 3
#define PCIC_CMA_LOW 4
#define PCIC_CMA_HIGH 5

	u_int8_t start_low, start_high = 0;
	u_int8_t stop_low, stop_high;
	u_int8_t off_low, off_high;
	u_int8_t mem_window;
	int reg;

	int kind = ph->mem[win].kind & ~PCMCIA_WIDTH_MEM_MASK;
	int mem8 =
	    (ph->mem[win].kind & PCMCIA_WIDTH_MEM_MASK) == PCMCIA_WIDTH_MEM8
	    || (kind == PCMCIA_MEM_ATTR);

	regbase_win = 0x10 + win * 0x08;

	phys_addr = ph->mem[win].addr;
	phys_end = phys_addr + ph->mem[win].size;

	DPRINTF(("pccbb_pcmcia_do_mem_map: start 0x%lx end 0x%lx off 0x%lx\n",
	    (unsigned long)phys_addr, (unsigned long)phys_end,
	    (unsigned long)ph->mem[win].offset));

#define PCIC_MEMREG_LSB_SHIFT PCIC_SYSMEM_ADDRX_SHIFT
#define PCIC_MEMREG_MSB_SHIFT (PCIC_SYSMEM_ADDRX_SHIFT + 8)
#define PCIC_MEMREG_WIN_SHIFT (PCIC_SYSMEM_ADDRX_SHIFT + 12)

	/* bit 19:12 */
	start_low = (phys_addr >> PCIC_MEMREG_LSB_SHIFT) & 0xff;
	/* bit 23:20 and bit 7 on */
	start_high = ((phys_addr >> PCIC_MEMREG_MSB_SHIFT) & 0x0f)
	    |(mem8 ? 0 : PCIC_SYSMEM_ADDRX_START_MSB_DATASIZE_16BIT);
	/* bit 31:24, for 32-bit address */
	mem_window = (phys_addr >> PCIC_MEMREG_WIN_SHIFT) & 0xff;

	Pcic_write(sc, regbase_win + PCIC_SMM_START_LOW, start_low);
	Pcic_write(sc, regbase_win + PCIC_SMM_START_HIGH, start_high);

	if (sc->sc_pcmcia_flags & PCCBB_PCMCIA_MEM_32) {
		Pcic_write(sc, 0x40 + win, mem_window);
	}

	stop_low = (phys_end >> PCIC_MEMREG_LSB_SHIFT) & 0xff;
	stop_high = ((phys_end >> PCIC_MEMREG_MSB_SHIFT) & 0x0f)
	    | PCIC_SYSMEM_ADDRX_STOP_MSB_WAIT2;	/* wait 2 cycles */
	/* XXX Geee, WAIT2!! Crazy!!  I must rewrite this routine. */

	Pcic_write(sc, regbase_win + PCIC_SMM_STOP_LOW, stop_low);
	Pcic_write(sc, regbase_win + PCIC_SMM_STOP_HIGH, stop_high);

	off_low = (ph->mem[win].offset >> PCIC_CARDMEM_ADDRX_SHIFT) & 0xff;
	off_high = ((ph->mem[win].offset >> (PCIC_CARDMEM_ADDRX_SHIFT + 8))
	    & PCIC_CARDMEM_ADDRX_MSB_ADDR_MASK)
	    | ((kind == PCMCIA_MEM_ATTR) ?
	    PCIC_CARDMEM_ADDRX_MSB_REGACTIVE_ATTR : 0);

	Pcic_write(sc, regbase_win + PCIC_CMA_LOW, off_low);
	Pcic_write(sc, regbase_win + PCIC_CMA_HIGH, off_high);

	reg = Pcic_read(sc, PCIC_ADDRWIN_ENABLE);
	reg |= ((1 << win) | PCIC_ADDRWIN_ENABLE_MEMCS16);
	Pcic_write(sc, PCIC_ADDRWIN_ENABLE, reg);

#if defined(CBB_DEBUG)
	{
		int r1, r2, r3, r4, r5, r6, r7 = 0;

		r1 = Pcic_read(sc, regbase_win + PCIC_SMM_START_LOW);
		r2 = Pcic_read(sc, regbase_win + PCIC_SMM_START_HIGH);
		r3 = Pcic_read(sc, regbase_win + PCIC_SMM_STOP_LOW);
		r4 = Pcic_read(sc, regbase_win + PCIC_SMM_STOP_HIGH);
		r5 = Pcic_read(sc, regbase_win + PCIC_CMA_LOW);
		r6 = Pcic_read(sc, regbase_win + PCIC_CMA_HIGH);
		if (sc->sc_pcmcia_flags & PCCBB_PCMCIA_MEM_32) {
			r7 = Pcic_read(sc, 0x40 + win);
		}

		printf("pccbb_pcmcia_do_mem_map window %d: %02x%02x %02x%02x "
		    "%02x%02x", win, r1, r2, r3, r4, r5, r6);
		if (sc->sc_pcmcia_flags & PCCBB_PCMCIA_MEM_32) {
			printf(" %02x", r7);
		}
		printf("\n");
	}
#endif
}

/*
 * STATIC int pccbb_pcmcia_mem_map(pcmcia_chipset_handle_t pch, int kind,
 *                                 bus_addr_t card_addr, bus_size_t size,
 *                                 struct pcmcia_mem_handle *pcmhp,
 *                                 bus_addr_t *offsetp, int *windowp)
 *
 * This function maps memory space allocated by the function
 * pccbb_pcmcia_mem_alloc().
 */
STATIC int
pccbb_pcmcia_mem_map(pcmcia_chipset_handle_t pch, int kind,
    bus_addr_t card_addr, bus_size_t size, struct pcmcia_mem_handle *pcmhp,
    bus_size_t *offsetp, int *windowp)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)pch;
	struct pcic_handle *ph = &sc->sc_pcmcia_h;
	bus_addr_t busaddr;
	long card_offset;
	int win;

	/* Check that the card is still there. */
	if ((Pcic_read(sc, PCIC_IF_STATUS) & PCIC_IF_STATUS_CARDDETECT_MASK) !=
		    PCIC_IF_STATUS_CARDDETECT_PRESENT)
		return 1;

	for (win = 0; win < PCIC_MEM_WINS; ++win) {
		if ((ph->memalloc & (1 << win)) == 0) {
			ph->memalloc |= (1 << win);
			break;
		}
	}

	if (win == PCIC_MEM_WINS) {
		return 1;
	}

	*windowp = win;

	/* XXX this is pretty gross */

	if (!bus_space_is_equal(sc->sc_memt, pcmhp->memt)) {
		panic("pccbb_pcmcia_mem_map memt is bogus");
	}

	busaddr = pcmhp->addr;

	/*
	 * compute the address offset to the pcmcia address space for the
	 * pcic.  this is intentionally signed.  The masks and shifts below
	 * will cause TRT to happen in the pcic registers.  Deal with making
	 * sure the address is aligned, and return the alignment offset.
	 */

	*offsetp = card_addr % PCIC_MEM_PAGESIZE;
	card_addr -= *offsetp;

	DPRINTF(("pccbb_pcmcia_mem_map window %d bus %lx+%lx+%lx at card addr "
	    "%lx\n", win, (u_long) busaddr, (u_long) * offsetp, (u_long) size,
	    (u_long) card_addr));

	/*
	 * include the offset in the size, and decrement size by one, since
	 * the hw wants start/stop
	 */
	size += *offsetp - 1;

	card_offset = (((long)card_addr) - ((long)busaddr));

	ph->mem[win].addr = busaddr;
	ph->mem[win].size = size;
	ph->mem[win].offset = card_offset;
	ph->mem[win].kind = kind;

	pccbb_pcmcia_do_mem_map(sc, win);

	return 0;
}

/*
 * STATIC int pccbb_pcmcia_mem_unmap(pcmcia_chipset_handle_t pch,
 *                                   int window)
 *
 * This function unmaps memory space which mapped by the function
 * pccbb_pcmcia_mem_map().
 */
STATIC void
pccbb_pcmcia_mem_unmap(pcmcia_chipset_handle_t pch, int window)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)pch;
	struct pcic_handle *ph = &sc->sc_pcmcia_h;
	int reg;

	if (window >= PCIC_MEM_WINS) {
		panic("pccbb_pcmcia_mem_unmap: window out of range");
	}

	reg = Pcic_read(sc, PCIC_ADDRWIN_ENABLE);
	reg &= ~(1 << window);
	Pcic_write(sc, PCIC_ADDRWIN_ENABLE, reg);

	ph->memalloc &= ~(1 << window);
}

/*
 * STATIC void *pccbb_pcmcia_intr_establish(pcmcia_chipset_handle_t pch,
 *                                          struct pcmcia_function *pf,
 *                                          int ipl,
 *                                          int (*func)(void *),
 *                                          void *arg);
 *
 * This function enables PC-Card interrupt.  PCCBB uses PCI interrupt line.
 */
STATIC void *
pccbb_pcmcia_intr_establish(pcmcia_chipset_handle_t pch,
    struct pcmcia_function *pf, int ipl, int (*func)(void *), void *arg)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)pch;

	if (!(pf->cfe->flags & PCMCIA_CFE_IRQLEVEL)) {
		/* what should I do? */
		if ((pf->cfe->flags & PCMCIA_CFE_IRQLEVEL)) {
			DPRINTF(("%s does not provide edge nor pulse "
			    "interrupt\n", device_xname(sc->sc_dev)));
			return NULL;
		}
		/*
		 * XXX Noooooo!  The interrupt flag must set properly!!
		 * dumb pcmcia driver!!
		 */
	}

	return pccbb_intr_establish(sc, ipl, func, arg);
}

/*
 * STATIC void pccbb_pcmcia_intr_disestablish(pcmcia_chipset_handle_t pch,
 *                                            void *ih)
 *
 * This function disables PC-Card interrupt.
 */
STATIC void
pccbb_pcmcia_intr_disestablish(pcmcia_chipset_handle_t pch, void *ih)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)pch;

	pccbb_intr_disestablish(sc, ih);
}

#if rbus
/*
 * static int
 * pccbb_rbus_cb_space_alloc(cardbus_chipset_tag_t ct, rbus_tag_t rb,
 *			    bus_addr_t addr, bus_size_t size,
 *			    bus_addr_t mask, bus_size_t align,
 *			    int flags, bus_addr_t *addrp;
 *			    bus_space_handle_t *bshp)
 *
 *   This function allocates a portion of memory or io space for
 *   clients.  This function is called from CardBus card drivers.
 */
static int
pccbb_rbus_cb_space_alloc(cardbus_chipset_tag_t ct, rbus_tag_t rb,
    bus_addr_t addr, bus_size_t size, bus_addr_t mask, bus_size_t align,
    int flags, bus_addr_t *addrp, bus_space_handle_t *bshp)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)ct;

	DPRINTF(("pccbb_rbus_cb_space_alloc: addr 0x%lx, size 0x%lx, "
	    "mask 0x%lx, align 0x%lx\n", (unsigned long)addr,
	    (unsigned long)size, (unsigned long)mask, (unsigned long)align));

	if (align == 0) {
		align = size;
	}

	if (bus_space_is_equal(rb->rb_bt, sc->sc_memt)) {
		if (align < 16) {
			return 1;
		}
		/*
		 * XXX: align more than 0x1000 to avoid overwrapping
		 * memory windows for two or more devices.  0x1000
		 * means memory window's granularity.
		 *
		 * Two or more devices should be able to share same
		 * memory window region.  However, overrapping memory
		 * window is not good because some devices, such as
		 * 3Com 3C575[BC], have a broken address decoder and
		 * intrude other's memory region.
		 */
		if (align < 0x1000) {
			align = 0x1000;
		}
	} else if (bus_space_is_equal(rb->rb_bt, sc->sc_iot)) {
		if (align < 4) {
			return 1;
		}
		/* XXX: hack for avoiding ISA image */
		if (mask < 0x0100) {
			mask = 0x3ff;
			addr = 0x300;
		}

	} else {
		DPRINTF(("pccbb_rbus_cb_space_alloc: Bus space tag 0x%lx is "
		    "NOT used. io: 0x%lx, mem: 0x%lx\n",
		    (unsigned long)rb->rb_bt, (unsigned long)sc->sc_iot,
		    (unsigned long)sc->sc_memt));
		return 1;
		/* XXX: panic here? */
	}

	if (rbus_space_alloc(rb, addr, size, mask, align, flags, addrp, bshp)) {
		aprint_normal_dev(sc->sc_dev, "<rbus> no bus space\n");
		return 1;
	}

	pccbb_open_win(sc, rb->rb_bt, *addrp, size, *bshp, 0);

	return 0;
}

/*
 * static int
 * pccbb_rbus_cb_space_free(cardbus_chipset_tag_t *ct, rbus_tag_t rb,
 *			   bus_space_handle_t *bshp, bus_size_t size);
 *
 *   This function is called from CardBus card drivers.
 */
static int
pccbb_rbus_cb_space_free(cardbus_chipset_tag_t ct, rbus_tag_t rb,
    bus_space_handle_t bsh, bus_size_t size)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)ct;
	bus_space_tag_t bt = rb->rb_bt;

	pccbb_close_win(sc, bt, bsh, size);

	if (bus_space_is_equal(bt, sc->sc_memt)) {
	} else if (bus_space_is_equal(bt, sc->sc_iot)) {
	} else {
		return 1;
		/* XXX: panic here? */
	}

	return rbus_space_free(rb, bsh, size, NULL);
}
#endif /* rbus */

#if rbus

static int
pccbb_open_win(struct pccbb_softc *sc, bus_space_tag_t bst, bus_addr_t addr,
    bus_size_t size, bus_space_handle_t bsh, int flags)
{
	struct pccbb_win_chain_head *head;
	bus_addr_t align;

	head = &sc->sc_iowindow;
	align = 0x04;
	if (bus_space_is_equal(sc->sc_memt, bst)) {
		head = &sc->sc_memwindow;
		align = 0x1000;
		DPRINTF(("using memory window, 0x%lx 0x%lx 0x%lx\n\n",
		    (unsigned long)sc->sc_iot, (unsigned long)sc->sc_memt,
		    (unsigned long)bst));
	}

	if (pccbb_winlist_insert(head, addr, size, bsh, flags)) {
		aprint_error_dev(sc->sc_dev,
		    "pccbb_open_win: %s winlist insert failed\n",
		    (head == &sc->sc_memwindow) ? "mem" : "io");
	}
	pccbb_winset(align, sc, bst);

	return 0;
}

static int
pccbb_close_win(struct pccbb_softc *sc, bus_space_tag_t bst,
    bus_space_handle_t bsh, bus_size_t size)
{
	struct pccbb_win_chain_head *head;
	bus_addr_t align;

	head = &sc->sc_iowindow;
	align = 0x04;
	if (bus_space_is_equal(sc->sc_memt, bst)) {
		head = &sc->sc_memwindow;
		align = 0x1000;
	}

	if (pccbb_winlist_delete(head, bsh, size)) {
		aprint_error_dev(sc->sc_dev,
		    "pccbb_close_win: %s winlist delete failed\n",
		    (head == &sc->sc_memwindow) ? "mem" : "io");
	}
	pccbb_winset(align, sc, bst);

	return 0;
}

static int
pccbb_winlist_insert(struct pccbb_win_chain_head *head, bus_addr_t start,
    bus_size_t size, bus_space_handle_t bsh, int flags)
{
	struct pccbb_win_chain *chainp, *elem;

	if ((elem = malloc(sizeof(struct pccbb_win_chain), M_DEVBUF,
	    M_NOWAIT)) == NULL)
		return (1);		/* fail */

	elem->wc_start = start;
	elem->wc_end = start + (size - 1);
	elem->wc_handle = bsh;
	elem->wc_flags = flags;

	TAILQ_FOREACH(chainp, head, wc_list) {
		if (chainp->wc_end >= start)
			break;
	}
	if (chainp != NULL)
		TAILQ_INSERT_AFTER(head, chainp, elem, wc_list);
	else
		TAILQ_INSERT_TAIL(head, elem, wc_list);
	return (0);
}

static int
pccbb_winlist_delete(struct pccbb_win_chain_head *head, bus_space_handle_t bsh,
    bus_size_t size)
{
	struct pccbb_win_chain *chainp;

	TAILQ_FOREACH(chainp, head, wc_list) {
		if (memcmp(&chainp->wc_handle, &bsh, sizeof(bsh)) == 0)
			break;
	}
	if (chainp == NULL)
		return 1;	       /* fail: no candidate to remove */

	if ((chainp->wc_end - chainp->wc_start) != (size - 1)) {
		printf("pccbb_winlist_delete: window 0x%lx size "
		    "inconsistent: 0x%lx, 0x%lx\n",
		    (unsigned long)chainp->wc_start,
		    (unsigned long)(chainp->wc_end - chainp->wc_start),
		    (unsigned long)(size - 1));
		return 1;
	}

	TAILQ_REMOVE(head, chainp, wc_list);
	free(chainp, M_DEVBUF);

	return 0;
}

static void
pccbb_winset(bus_addr_t align, struct pccbb_softc *sc, bus_space_tag_t bst)
{
	pci_chipset_tag_t pc;
	pcitag_t tag;
	bus_addr_t mask = ~(align - 1);
	struct {
		pcireg_t win_start;
		pcireg_t win_limit;
		int win_flags;
	} win[2];
	struct pccbb_win_chain *chainp;
	int offs;

	win[0].win_start = win[1].win_start = 0xffffffff;
	win[0].win_limit = win[1].win_limit = 0;
	win[0].win_flags = win[1].win_flags = 0;

	chainp = TAILQ_FIRST(&sc->sc_iowindow);
	offs = PCI_CB_IOBASE0;
	if (bus_space_is_equal(sc->sc_memt, bst)) {
		chainp = TAILQ_FIRST(&sc->sc_memwindow);
		offs = PCI_CB_MEMBASE0;
	}

	if (chainp != NULL) {
		win[0].win_start = chainp->wc_start & mask;
		win[0].win_limit = chainp->wc_end & mask;
		win[0].win_flags = chainp->wc_flags;
		chainp = TAILQ_NEXT(chainp, wc_list);
	}

	for (; chainp != NULL; chainp = TAILQ_NEXT(chainp, wc_list)) {
		if (win[1].win_start == 0xffffffff) {
			/* window 1 is not used */
			if ((win[0].win_flags == chainp->wc_flags) &&
			    (win[0].win_limit + align >=
			    (chainp->wc_start & mask))) {
				/* concatenate */
				win[0].win_limit = chainp->wc_end & mask;
			} else {
				/* make new window */
				win[1].win_start = chainp->wc_start & mask;
				win[1].win_limit = chainp->wc_end & mask;
				win[1].win_flags = chainp->wc_flags;
			}
			continue;
		}

		/* Both windows are engaged. */
		if (win[0].win_flags == win[1].win_flags) {
			/* same flags */
			if (win[0].win_flags == chainp->wc_flags) {
				if (win[1].win_start - (win[0].win_limit +
				    align) <
				    (chainp->wc_start & mask) -
				    ((chainp->wc_end & mask) + align)) {
					/*
					 * merge window 0 and 1, and set win1
					 * to chainp
					 */
					win[0].win_limit = win[1].win_limit;
					win[1].win_start =
					    chainp->wc_start & mask;
					win[1].win_limit =
					    chainp->wc_end & mask;
				} else {
					win[1].win_limit =
					    chainp->wc_end & mask;
				}
			} else {
				/* different flags */

				/* concatenate win0 and win1 */
				win[0].win_limit = win[1].win_limit;
				/* allocate win[1] to new space */
				win[1].win_start = chainp->wc_start & mask;
				win[1].win_limit = chainp->wc_end & mask;
				win[1].win_flags = chainp->wc_flags;
			}
		} else {
			/* the flags of win[0] and win[1] is different */
			if (win[0].win_flags == chainp->wc_flags) {
				win[0].win_limit = chainp->wc_end & mask;
				/*
				 * XXX this creates overlapping windows, so
				 * what should the poor bridge do if one is
				 * cachable, and the other is not?
				 */
				aprint_error_dev(sc->sc_dev,
				    "overlapping windows\n");
			} else {
				win[1].win_limit = chainp->wc_end & mask;
			}
		}
	}

	pc = sc->sc_pc;
	tag = sc->sc_tag;
	pci_conf_write(pc, tag, offs, win[0].win_start);
	pci_conf_write(pc, tag, offs + 4, win[0].win_limit);
	pci_conf_write(pc, tag, offs + 8, win[1].win_start);
	pci_conf_write(pc, tag, offs + 12, win[1].win_limit);
	DPRINTF(("--pccbb_winset: win0 [0x%lx, 0x%lx), win1 [0x%lx, 0x%lx)\n",
	    (unsigned long)pci_conf_read(pc, tag, offs),
	    (unsigned long)pci_conf_read(pc, tag, offs + 4) + align,
	    (unsigned long)pci_conf_read(pc, tag, offs + 8),
	    (unsigned long)pci_conf_read(pc, tag, offs + 12) + align));

	if (bus_space_is_equal(bst, sc->sc_memt)) {
		pcireg_t bcr = pci_conf_read(pc, tag, PCI_BRIDGE_CONTROL_REG);

		bcr &= ~(CB_BCR_PREFETCH_MEMWIN0 | CB_BCR_PREFETCH_MEMWIN1);
		if (win[0].win_flags & PCCBB_MEM_CACHABLE)
			bcr |= CB_BCR_PREFETCH_MEMWIN0;
		if (win[1].win_flags & PCCBB_MEM_CACHABLE)
			bcr |= CB_BCR_PREFETCH_MEMWIN1;
		pci_conf_write(pc, tag, PCI_BRIDGE_CONTROL_REG, bcr);
	}
}

#endif /* rbus */

static bool
pccbb_suspend(device_t dv, const pmf_qual_t *qual)
{
	struct pccbb_softc *sc = device_private(dv);
	bus_space_tag_t base_memt = sc->sc_base_memt;	/* socket regs memory */
	bus_space_handle_t base_memh = sc->sc_base_memh;
	pcireg_t reg;

	if (sc->sc_pil_intr_enable)
		(void)pccbbintr_function(sc);
	sc->sc_pil_intr_enable = false;

	reg = bus_space_read_4(base_memt, base_memh, CB_SOCKET_MASK);
	/* Disable interrupts. */
	reg &= ~(CB_SOCKET_MASK_CSTS | CB_SOCKET_MASK_CD | CB_SOCKET_MASK_POWER);
	bus_space_write_4(base_memt, base_memh, CB_SOCKET_MASK, reg);
	/* XXX joerg Disable power to the socket? */

	/* XXX flush PCI write */
	bus_space_read_4(base_memt, base_memh, CB_SOCKET_EVENT);

	/* reset interrupt */
	bus_space_write_4(base_memt, base_memh, CB_SOCKET_EVENT,
	    bus_space_read_4(base_memt, base_memh, CB_SOCKET_EVENT));
	/* XXX flush PCI write */
	bus_space_read_4(base_memt, base_memh, CB_SOCKET_EVENT);

	if (sc->sc_ih != NULL) {
		pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
		sc->sc_ih = NULL;
	}

	return true;
}

static bool
pccbb_resume(device_t dv, const pmf_qual_t *qual)
{
	struct pccbb_softc *sc = device_private(dv);
	bus_space_tag_t base_memt = sc->sc_base_memt;	/* socket regs memory */
	bus_space_handle_t base_memh = sc->sc_base_memh;
	pcireg_t reg;

	pccbb_chipinit(sc);
	pccbb_intrinit(sc);
	/* setup memory and io space window for CB */
	pccbb_winset(0x1000, sc, sc->sc_memt);
	pccbb_winset(0x04, sc, sc->sc_iot);

	/* CSC Interrupt: Card detect interrupt on */
	reg = bus_space_read_4(base_memt, base_memh, CB_SOCKET_MASK);
	/* Card detect intr is turned on. */
	reg |= CB_SOCKET_MASK_CSTS | CB_SOCKET_MASK_CD | CB_SOCKET_MASK_POWER;
	bus_space_write_4(base_memt, base_memh, CB_SOCKET_MASK, reg);
	/* reset interrupt */
	reg = bus_space_read_4(base_memt, base_memh, CB_SOCKET_EVENT);
	bus_space_write_4(base_memt, base_memh, CB_SOCKET_EVENT, reg);

	/*
	 * check for card insertion or removal during suspend period.
	 * XXX: the code can't cope with card swap (remove then
	 * insert).  how can we detect such situation?
	 */
	(void)pccbbintr(sc);

	sc->sc_pil_intr_enable = true;

	return true;
}

/*	$NetBSD: cardbus.c,v 1.108 2011/08/01 11:20:27 drochner Exp $	*/

/*
 * Copyright (c) 1997, 1998, 1999 and 2000
 *     HAYAKAWA Koichi.  All rights reserved.
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
__KERNEL_RCSID(0, "$NetBSD: cardbus.c,v 1.108 2011/08/01 11:20:27 drochner Exp $");

#include "opt_cardbus.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/proc.h>
#include <sys/reboot.h>		/* for AB_* needed by bootverbose */

#include <sys/bus.h>

#include <dev/cardbus/cardbusvar.h>
#include <dev/pci/pcidevs.h>

#include <dev/cardbus/cardbus_exrom.h>

#include <dev/pci/pcivar.h>	/* XXX */
#include <dev/pci/pcireg.h>	/* XXX */

#include <dev/pcmcia/pcmciareg.h>

#include "locators.h"

#if defined CARDBUS_DEBUG
#define STATIC
#define DPRINTF(a) printf a
#else
#define STATIC static
#define DPRINTF(a)
#endif


STATIC void cardbusattach(device_t, device_t, void *);
STATIC int cardbusdetach(device_t, int);
STATIC int cardbusmatch(device_t, cfdata_t, void *);
int cardbus_rescan(device_t, const char *, const int *);
void cardbus_childdetached(device_t, device_t);
static int cardbusprint(void *, const char *);

typedef void (*tuple_decode_func)(u_int8_t*, int, void*);

static int decode_tuples(u_int8_t *, int, tuple_decode_func, void*);
#ifdef CARDBUS_DEBUG
static void print_tuple(u_int8_t*, int, void*);
#endif

static int cardbus_read_tuples(struct cardbus_attach_args *,
    pcireg_t, u_int8_t *, size_t);

static void enable_function(struct cardbus_softc *, int, int);
static void disable_function(struct cardbus_softc *, int);

static bool cardbus_child_register(device_t);

CFATTACH_DECL3_NEW(cardbus, sizeof(struct cardbus_softc),
    cardbusmatch, cardbusattach, cardbusdetach, NULL,
    cardbus_rescan, cardbus_childdetached, DVF_DETACH_SHUTDOWN);

#ifndef __NetBSD_Version__
struct cfdriver cardbus_cd = {
	NULL, "cardbus", DV_DULL
};
#endif


STATIC int
cardbusmatch(device_t parent, cfdata_t cf, void *aux)
{

	return (1);
}

STATIC void
cardbusattach(device_t parent, device_t self, void *aux)
{
	struct cardbus_softc *sc = device_private(self);
	struct cbslot_attach_args *cba = aux;

	sc->sc_dev = self;

	sc->sc_bus = cba->cba_bus;
	sc->sc_cacheline = cba->cba_cacheline;
	sc->sc_max_lattimer = MIN(0xf8, cba->cba_max_lattimer);

	aprint_naive("\n");
	aprint_normal(": bus %d", sc->sc_bus);
	if (bootverbose)
		aprint_normal(" cacheline 0x%x, lattimer 0x%x",
		    sc->sc_cacheline, sc->sc_max_lattimer);
	aprint_normal("\n");

	sc->sc_iot = cba->cba_iot;	/* CardBus I/O space tag */
	sc->sc_memt = cba->cba_memt;	/* CardBus MEM space tag */
	sc->sc_dmat = cba->cba_dmat;	/* DMA tag */
	sc->sc_cc = cba->cba_cc;
	sc->sc_cf = cba->cba_cf;

	sc->sc_rbus_iot = cba->cba_rbus_iot;
	sc->sc_rbus_memt = cba->cba_rbus_memt;

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

STATIC int
cardbusdetach(device_t self, int flags)
{
	int rc;

	if ((rc = config_detach_children(self, flags)) != 0)
		return rc;

	pmf_device_deregister(self);
	return 0;
}

static int
cardbus_read_tuples(struct cardbus_attach_args *ca, pcireg_t cis_ptr,
    u_int8_t *tuples, size_t len)
{
	struct cardbus_softc *sc = ca->ca_ct->ct_sc;
	cardbus_chipset_tag_t cc = ca->ca_ct->ct_cc;
	cardbus_function_tag_t cf = ca->ca_ct->ct_cf;
	pcitag_t tag = ca->ca_tag;
	pcireg_t command;
	bus_space_tag_t bar_tag;
	bus_space_handle_t bar_memh;
	bus_size_t bar_size;
	bus_addr_t bar_addr;
	pcireg_t reg;
	int found = 0;
	int cardbus_space = cis_ptr & CARDBUS_CIS_ASIMASK;
	int i, j;

	memset(tuples, 0, len);

	cis_ptr = cis_ptr & CARDBUS_CIS_ADDRMASK;

	switch (cardbus_space) {
	case CARDBUS_CIS_ASI_TUPLE:
		DPRINTF(("%s: reading CIS data from configuration space\n",
		    device_xname(sc->sc_dev)));
		for (i = cis_ptr, j = 0; i < 0xff; i += 4) {
			u_int32_t e = (*cf->cardbus_conf_read)(cc, tag, i);
			tuples[j] = 0xff & e;
			e >>= 8;
			tuples[j + 1] = 0xff & e;
			e >>= 8;
			tuples[j + 2] = 0xff & e;
			e >>= 8;
			tuples[j + 3] = 0xff & e;
			j += 4;
		}
		found++;
		break;

	case CARDBUS_CIS_ASI_BAR0:
	case CARDBUS_CIS_ASI_BAR1:
	case CARDBUS_CIS_ASI_BAR2:
	case CARDBUS_CIS_ASI_BAR3:
	case CARDBUS_CIS_ASI_BAR4:
	case CARDBUS_CIS_ASI_BAR5:
	case CARDBUS_CIS_ASI_ROM:
		if (cardbus_space == CARDBUS_CIS_ASI_ROM) {
			reg = CARDBUS_ROM_REG;
			DPRINTF(("%s: reading CIS data from ROM\n",
			    device_xname(sc->sc_dev)));
		} else {
			reg = CARDBUS_CIS_ASI_BAR(cardbus_space);
			DPRINTF(("%s: reading CIS data from BAR%d\n",
			    device_xname(sc->sc_dev), cardbus_space - 1));
		}

		/*
		 * XXX zero register so mapreg_map doesn't get confused by old
		 * contents.
		 */
		cardbus_conf_write(cc, cf, tag, reg, 0);
		if (Cardbus_mapreg_map(ca->ca_ct, reg,
		    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT,
		    0, &bar_tag, &bar_memh, &bar_addr, &bar_size)) {
			aprint_error_dev(sc->sc_dev, "failed to map memory\n");
			return (1);
		}
		aprint_debug_dev(sc->sc_dev, "mapped %ju bytes at 0x%jx\n",
		    (uintmax_t)bar_size, (uintmax_t)bar_addr);

		if (cardbus_space == CARDBUS_CIS_ASI_ROM) {
			pcireg_t exrom;
			int save;
			struct cardbus_rom_image_head rom_image;
			struct cardbus_rom_image *p;

			save = splhigh();
			/* enable rom address decoder */
			exrom = cardbus_conf_read(cc, cf, tag, reg);
			cardbus_conf_write(cc, cf, tag, reg, exrom | 1);

			command = cardbus_conf_read(cc, cf, tag,
			    PCI_COMMAND_STATUS_REG);
			cardbus_conf_write(cc, cf, tag,
			    PCI_COMMAND_STATUS_REG,
			    command | PCI_COMMAND_MEM_ENABLE);

			if (cardbus_read_exrom(bar_tag, bar_memh, &rom_image))
				goto out;

			SIMPLEQ_FOREACH(p, &rom_image, next) {
				if (p->rom_image ==
				    CARDBUS_CIS_ASI_ROM_IMAGE(cis_ptr)) {
					bus_space_read_region_1(p->romt,
					    p->romh, CARDBUS_CIS_ADDR(cis_ptr),
					    tuples, MIN(p->image_size, len));
					found++;
					break;
				}
			}
			while ((p = SIMPLEQ_FIRST(&rom_image)) != NULL) {
				SIMPLEQ_REMOVE_HEAD(&rom_image, next);
				free(p, M_DEVBUF);
			}
		out:
			exrom = cardbus_conf_read(cc, cf, tag, reg);
			cardbus_conf_write(cc, cf, tag, reg, exrom & ~1);
			splx(save);
		} else {
			command = cardbus_conf_read(cc, cf, tag,
			    PCI_COMMAND_STATUS_REG);
			cardbus_conf_write(cc, cf, tag,
			    PCI_COMMAND_STATUS_REG,
			    command | PCI_COMMAND_MEM_ENABLE);
			/* XXX byte order? */
			bus_space_read_region_1(bar_tag, bar_memh,
			    cis_ptr, tuples,
			    MIN(bar_size - MIN(bar_size, cis_ptr), len));
			found++;
		}
		command = cardbus_conf_read(cc, cf, tag,
		    PCI_COMMAND_STATUS_REG);
		cardbus_conf_write(cc, cf, tag, PCI_COMMAND_STATUS_REG,
		    command & ~PCI_COMMAND_MEM_ENABLE);
		cardbus_conf_write(cc, cf, tag, reg, 0);

		Cardbus_mapreg_unmap(ca->ca_ct, reg, bar_tag, bar_memh,
		    bar_size);
		break;

#ifdef DIAGNOSTIC
	default:
		panic("%s: bad CIS space (%d)", device_xname(sc->sc_dev),
		    cardbus_space);
#endif
	}
	return (!found);
}

static void
parse_tuple(u_int8_t *tuple, int len, void *data)
{
	struct cardbus_cis_info *cis = data;
	char *p;
	int i, bar_index;

	switch (tuple[0]) {
	case PCMCIA_CISTPL_MANFID:
		if (tuple[1] != 4) {
			DPRINTF(("%s: wrong length manufacturer id (%d)\n",
			    __func__, tuple[1]));
			break;
		}
		cis->manufacturer = tuple[2] | (tuple[3] << 8);
		cis->product = tuple[4] | (tuple[5] << 8);
		break;

	case PCMCIA_CISTPL_VERS_1:
		memcpy(cis->cis1_info_buf, tuple + 2, tuple[1]);
		i = 0;
		p = cis->cis1_info_buf + 2;
		while (i <
		    sizeof(cis->cis1_info) / sizeof(cis->cis1_info[0])) {
			if (p >= cis->cis1_info_buf + tuple[1] || *p == '\xff')
				break;
			cis->cis1_info[i++] = p;
			while (*p != '\0' && *p != '\xff')
				p++;
			if (*p == '\0')
				p++;
		}
		break;

	case PCMCIA_CISTPL_BAR:
		if (tuple[1] != 6) {
			DPRINTF(("%s: BAR with short length (%d)\n",
			    __func__, tuple[1]));
			break;
		}
		bar_index = tuple[2] & 7;
		if (bar_index == 0) {
			DPRINTF(("%s: invalid ASI in BAR tuple\n", __func__));
			break;
		}
		bar_index--;
		cis->bar[bar_index].flags = tuple[2];
		cis->bar[bar_index].size =
		    (tuple[4] << 0) |
		    (tuple[5] << 8) |
		    (tuple[6] << 16) |
		    (tuple[7] << 24);
		break;

	case PCMCIA_CISTPL_FUNCID:
		cis->funcid = tuple[2];
		break;

	case PCMCIA_CISTPL_FUNCE:
		switch (cis->funcid) {
		case PCMCIA_FUNCTION_SERIAL:
			if (tuple[1] >= 2 &&
			    /* XXX PCMCIA_TPLFE_TYPE_SERIAL_??? */
			    tuple[2] == 0) {
				cis->funce.serial.uart_type = tuple[3] & 0x1f;
				cis->funce.serial.uart_present = 1;
			}
			break;

		case PCMCIA_FUNCTION_NETWORK:
			if (tuple[1] >= 8 &&
			    tuple[2] == PCMCIA_TPLFE_TYPE_LAN_NID) {
				if (tuple[3] >
				    sizeof(cis->funce.network.netid)) {
					DPRINTF(("%s: unknown network id type "
					    "(len = %d)\n",
					    __func__, tuple[3]));
				} else {
					cis->funce.network.netid_present = 1;
					memcpy(cis->funce.network.netid,
					    tuple + 4, tuple[3]);
				}
			}
			break;
		}
		break;
	}
}

/*
 * int cardbus_attach_card(struct cardbus_softc *sc)
 *
 *    This function attaches the card on the slot: turns on power,
 *    reads and analyses tuple, sets configuration index.
 *
 *    This function returns the number of recognised device functions.
 *    If no functions are recognised, return 0.
 */
int
cardbus_attach_card(struct cardbus_softc *sc)
{
	cardbus_chipset_tag_t cc;
	cardbus_function_tag_t cf;
	int cdstatus;
	static int wildcard[CARDBUSCF_NLOCS] = {
		CARDBUSCF_FUNCTION_DEFAULT
	};

	cc = sc->sc_cc;
	cf = sc->sc_cf;

	DPRINTF(("cardbus_attach_card: cb%d start\n",
		 device_unit(sc->sc_dev)));

	/* inspect initial voltage */
	if ((cdstatus = (*cf->cardbus_ctrl)(cc, CARDBUS_CD)) == 0) {
		DPRINTF(("%s: no CardBus card on cb%d\n", __func__,
		    device_unit(sc->sc_dev)));
		return (0);
	}

	device_pmf_driver_set_child_register(sc->sc_dev, cardbus_child_register);
	cardbus_rescan(sc->sc_dev, "cardbus", wildcard);
	return (1); /* XXX */
}

int
cardbus_rescan(device_t self, const char *ifattr,
    const int *locators)
{
	struct cardbus_softc *sc = device_private(self);
	cardbus_chipset_tag_t cc;
	cardbus_function_tag_t cf;
	pcitag_t tag;
	pcireg_t id, class, cis_ptr;
	pcireg_t bhlc, icr, lattimer;
	int cdstatus;
	int function, nfunction;
	device_t csc;
	cardbus_devfunc_t ct;

	cc = sc->sc_cc;
	cf = sc->sc_cf;

	/* inspect initial voltage */
	if ((cdstatus = (*cf->cardbus_ctrl)(cc, CARDBUS_CD)) == 0) {
		DPRINTF(("%s: no CardBus card on cb%d\n", __func__,
		    device_unit(sc->sc_dev)));
		return (0);
	}

	/*
	 * XXX use fake function 8 to keep power on during whole
	 * configuration.
	 */
	enable_function(sc, cdstatus, 8);
	function = 0;

	tag = cardbus_make_tag(cc, cf, sc->sc_bus, function);

	/*
	 * Wait until power comes up.  Maxmum 500 ms.
	 *
	 * XXX What is this for?  The bridge driver ought to have waited
	 * XXX already.
	 */
	{
		int i;

		for (i = 0; i < 5; ++i) {
			id = cardbus_conf_read(cc, cf, tag, PCI_ID_REG);
			if (id != 0xffffffff && id != 0) {
				break;
			}
			if (cold) {	/* before kernel thread invoked */
				delay(100 * 1000);
			} else {	/* thread context */
				if (tsleep((void *)sc, PCATCH, "cardbus",
				    hz / 10) != EWOULDBLOCK) {
					break;
				}
			}
		}
		aprint_debug_dev(self, "id reg valid in %d iterations\n", i);
		if (i == 5) {
			return (EIO);
		}
	}

	bhlc = cardbus_conf_read(cc, cf, tag, PCI_BHLC_REG);
	DPRINTF(("%s bhlc 0x%08x -> ", device_xname(sc->sc_dev), bhlc));
	nfunction = PCI_HDRTYPE_MULTIFN(bhlc) ? 8 : 1;

	for (function = 0; function < nfunction; function++) {
		struct cardbus_attach_args ca;
		int locs[CARDBUSCF_NLOCS];

		if (locators[CARDBUSCF_FUNCTION] !=
		    CARDBUSCF_FUNCTION_DEFAULT &&
		    locators[CARDBUSCF_FUNCTION] != function)
			continue;

		if (sc->sc_funcs[function])
			continue;

		tag = cardbus_make_tag(cc, cf, sc->sc_bus, function);

		id = cardbus_conf_read(cc, cf, tag, PCI_ID_REG);
		class = cardbus_conf_read(cc, cf, tag, PCI_CLASS_REG);
		cis_ptr = cardbus_conf_read(cc, cf, tag, CARDBUS_CIS_REG);

		/* Invalid vendor ID value? */
		if (PCI_VENDOR(id) == PCI_VENDOR_INVALID) {
			continue;
		}

		DPRINTF(("cardbus_attach_card: "
		    "Vendor 0x%x, Product 0x%x, CIS 0x%x\n",
		    PCI_VENDOR(id), PCI_PRODUCT(id), cis_ptr));

		enable_function(sc, cdstatus, function);

		/* clean up every BAR */
		cardbus_conf_write(cc, cf, tag, PCI_BAR0, 0);
		cardbus_conf_write(cc, cf, tag, PCI_BAR1, 0);
		cardbus_conf_write(cc, cf, tag, PCI_BAR2, 0);
		cardbus_conf_write(cc, cf, tag, PCI_BAR3, 0);
		cardbus_conf_write(cc, cf, tag, PCI_BAR4, 0);
		cardbus_conf_write(cc, cf, tag, PCI_BAR5, 0);
		cardbus_conf_write(cc, cf, tag, CARDBUS_ROM_REG, 0);

		/* set initial latency and cacheline size */
		bhlc = cardbus_conf_read(cc, cf, tag, PCI_BHLC_REG);
		icr = cardbus_conf_read(cc, cf, tag, PCI_INTERRUPT_REG);
		DPRINTF(("%s func%d icr 0x%08x bhlc 0x%08x -> ",
		    device_xname(sc->sc_dev), function, icr, bhlc));
		bhlc &= ~(PCI_CACHELINE_MASK << PCI_CACHELINE_SHIFT);
		bhlc |= (sc->sc_cacheline & PCI_CACHELINE_MASK) <<
		    PCI_CACHELINE_SHIFT;
		/*
		 * Set the initial value of the Latency Timer.
		 *
		 * While a PCI device owns the bus, its Latency
		 * Timer counts down bus cycles from its initial
		 * value to 0.  Minimum Grant tells for how long
		 * the device wants to own the bus once it gets
		 * access, in units of 250ns.
		 *
		 * On a 33 MHz bus, there are 8 cycles per 250ns.
		 * So I multiply the Minimum Grant by 8 to find
		 * out the initial value of the Latency Timer.
		 *
		 * Avoid setting a Latency Timer less than 0x10,
		 * since the old code did not do that.
		 */
		lattimer =
		    MIN(sc->sc_max_lattimer, MAX(0x10, 8 * PCI_MIN_GNT(icr)));
		if (PCI_LATTIMER(bhlc) < lattimer) {
			bhlc &= ~(PCI_LATTIMER_MASK << PCI_LATTIMER_SHIFT);
			bhlc |= (lattimer << PCI_LATTIMER_SHIFT);
		}

		cardbus_conf_write(cc, cf, tag, PCI_BHLC_REG, bhlc);
		bhlc = cardbus_conf_read(cc, cf, tag, PCI_BHLC_REG);
		DPRINTF(("0x%08x\n", bhlc));

		/*
		 * We need to allocate the ct here, since we might
		 * need it when reading the CIS
		 */
		if ((ct = malloc(sizeof(struct cardbus_devfunc),
		    M_DEVBUF, M_NOWAIT)) == NULL) {
			panic("no room for cardbus_tag");
		}

		ct->ct_bhlc = bhlc;
		ct->ct_cc = sc->sc_cc;
		ct->ct_cf = sc->sc_cf;
		ct->ct_bus = sc->sc_bus;
		ct->ct_func = function;
		ct->ct_sc = sc;
		sc->sc_funcs[function] = ct;

		memset(&ca, 0, sizeof(ca));

		ca.ca_ct = ct;

		ca.ca_iot = sc->sc_iot;
		ca.ca_memt = sc->sc_memt;
		ca.ca_dmat = sc->sc_dmat;

		ca.ca_rbus_iot = sc->sc_rbus_iot;
		ca.ca_rbus_memt= sc->sc_rbus_memt;

		ca.ca_tag = tag;
		ca.ca_bus = sc->sc_bus;
		ca.ca_function = function;
		ca.ca_id = id;
		ca.ca_class = class;

		if (cis_ptr != 0) {
#define TUPLESIZE 2048
			u_int8_t *tuple = malloc(TUPLESIZE, M_DEVBUF, M_WAITOK);
			if (cardbus_read_tuples(&ca, cis_ptr,
			    tuple, TUPLESIZE)) {
				printf("cardbus_attach_card: "
				    "failed to read CIS\n");
			} else {
#ifdef CARDBUS_DEBUG
				decode_tuples(tuple, TUPLESIZE,
				    print_tuple, NULL);
#endif
				decode_tuples(tuple, TUPLESIZE,
				    parse_tuple, &ca.ca_cis);
			}
			free(tuple, M_DEVBUF);
		}

		locs[CARDBUSCF_FUNCTION] = function;

		if ((csc = config_found_sm_loc(sc->sc_dev, "cardbus", locs,
		    &ca, cardbusprint, config_stdsubmatch)) == NULL) {
			/* do not match */
			disable_function(sc, function);
			sc->sc_funcs[function] = NULL;
			free(ct, M_DEVBUF);
		} else {
			/* found */
			ct->ct_device = csc;
		}
	}
	/*
	 * XXX power down pseudo function 8 (this will power down the card
	 * if no functions were attached).
	 */
	disable_function(sc, 8);

	return (0);
}

static int
cardbusprint(void *aux, const char *pnp)
{
	struct cardbus_attach_args *ca = aux;
	char devinfo[256];
	int i;

	if (pnp) {
		pci_devinfo(ca->ca_id, ca->ca_class, 1, devinfo,
		    sizeof(devinfo));
		for (i = 0; i < 4; i++) {
			if (ca->ca_cis.cis1_info[i] == NULL)
				break;
			if (i)
				aprint_normal(", ");
			aprint_normal("%s", ca->ca_cis.cis1_info[i]);
		}
		aprint_verbose("%s(manufacturer 0x%x, product 0x%x)",
		    i ? " " : "",
		    ca->ca_cis.manufacturer, ca->ca_cis.product);
		aprint_normal(" %s at %s", devinfo, pnp);
	}
	aprint_normal(" function %d", ca->ca_function);

	return (UNCONF);
}

/*
 * void cardbus_detach_card(struct cardbus_softc *sc)
 *
 *    This function detaches the card on the slot: detach device data
 *    structure and turns off the power.
 *
 *    This function must not be called under interrupt context.
 */
void
cardbus_detach_card(struct cardbus_softc *sc)
{
	int f;
	struct cardbus_devfunc *ct;

	for (f = 0; f < 8; f++) {
		ct = sc->sc_funcs[f];
		if (!ct)
			continue;

		DPRINTF(("%s: detaching %s\n", device_xname(sc->sc_dev),
		    device_xname(ct->ct_device)));
		/* call device detach function */

		if (config_detach(ct->ct_device, 0) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "cannot detach dev %s, function %d\n",
			    device_xname(ct->ct_device), ct->ct_func);
		}
	}

	sc->sc_poweron_func = 0;
	(*sc->sc_cf->cardbus_power)(sc->sc_cc,
	    CARDBUS_VCC_0V | CARDBUS_VPP_0V);
}

void
cardbus_childdetached(device_t self, device_t child)
{
	struct cardbus_softc *sc = device_private(self);
	struct cardbus_devfunc *ct;

	ct = sc->sc_funcs[device_locator(child, CARDBUSCF_FUNCTION)];
	KASSERT(ct->ct_device == child);

	sc->sc_poweron_func &= ~(1 << ct->ct_func);
	sc->sc_funcs[ct->ct_func] = NULL;
	free(ct, M_DEVBUF);
}

void *
Cardbus_intr_establish(cardbus_devfunc_t ct,
    int level, int (*func)(void *), void *arg)
{
	return cardbus_intr_establish(ct->ct_cc, ct->ct_cf, level, func,
	    arg);
}

/*
 * void *cardbus_intr_establish(cc, cf, irq, level, func, arg)
 *   Interrupt handler of pccard.
 *  args:
 *   cardbus_chipset_tag_t *cc
 *   int irq:
 */
void *
cardbus_intr_establish(cardbus_chipset_tag_t cc, cardbus_function_tag_t cf,
    int level, int (*func)(void *), void *arg)
{

	DPRINTF(("- cardbus_intr_establish\n"));
	return ((*cf->cardbus_intr_establish)(cc, level, func, arg));
}

void
Cardbus_intr_disestablish(cardbus_devfunc_t ct, void *handler)
{
	cardbus_intr_disestablish(ct->ct_cc, ct->ct_cf, handler);
}

/*
 * void cardbus_intr_disestablish(cc, cf, handler)
 *   Interrupt handler of pccard.
 *  args:
 *   cardbus_chipset_tag_t *cc
 */
void
cardbus_intr_disestablish(cardbus_chipset_tag_t cc, cardbus_function_tag_t cf,
    void *handler)
{

	DPRINTF(("- pccard_intr_disestablish\n"));
	(*cf->cardbus_intr_disestablish)(cc, handler);
}

/*
 * XXX this should be merged with cardbus_function_{enable,disable},
 * but we don't have a ct when these functions are called.
 */
static void
enable_function(struct cardbus_softc *sc, int cdstatus, int function)
{

	if (sc->sc_poweron_func == 0) {
		/* switch to 3V and/or wait for power to stabilize */
		if (cdstatus & CARDBUS_3V_CARD) {
			/*
			 * sc_poweron_func must be substituted before
			 * entering sleep, in order to avoid turn on
			 * power twice.
			 */
			sc->sc_poweron_func |= (1 << function);
			(*sc->sc_cf->cardbus_power)(sc->sc_cc, CARDBUS_VCC_3V);
		} else {
			/* No cards other than 3.3V cards. */
			return;
		}
		(*sc->sc_cf->cardbus_ctrl)(sc->sc_cc, CARDBUS_RESET);
	}
	sc->sc_poweron_func |= (1 << function);
}

static void
disable_function(struct cardbus_softc *sc, int function)
{
	bool powerdown;
	cardbus_devfunc_t ct;
	device_t dv;
	int i;

	sc->sc_poweron_func &= ~(1 << function);
	if (sc->sc_poweron_func != 0)
		return;
	for (i = 0; i < __arraycount(sc->sc_funcs); i++) {
		if ((ct = sc->sc_funcs[i]) == NULL)
			continue;
		dv = ct->ct_device;
		if (prop_dictionary_get_bool(device_properties(dv),
		    "pmf-powerdown", &powerdown) && !powerdown)
			return;
	}
	/* power-off because no functions are enabled */
	(*sc->sc_cf->cardbus_power)(sc->sc_cc, CARDBUS_VCC_0V);
}

/*
 * int cardbus_function_enable(struct cardbus_softc *sc, int func)
 *
 *   This function enables a function on a card.  When no power is
 *  applied on the card, power will be applied on it.
 */
int
cardbus_function_enable(struct cardbus_softc *sc, int func)
{
	cardbus_chipset_tag_t cc = sc->sc_cc;
	cardbus_function_tag_t cf = sc->sc_cf;
	cardbus_devfunc_t ct;
	pcireg_t command;
	pcitag_t tag;

	DPRINTF(("entering cardbus_function_enable...  "));

	/* entering critical area */

	/* XXX: sc_vold should be used */
	enable_function(sc, CARDBUS_3V_CARD, func);

	/* exiting critical area */

	tag = cardbus_make_tag(cc, cf, sc->sc_bus, func);

	command = cardbus_conf_read(cc, cf, tag, PCI_COMMAND_STATUS_REG);
	command |= (PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_IO_ENABLE |
	    PCI_COMMAND_MASTER_ENABLE); /* XXX: good guess needed */

	cardbus_conf_write(cc, cf, tag, PCI_COMMAND_STATUS_REG, command);

	if ((ct = sc->sc_funcs[func]) != NULL)
		Cardbus_conf_write(ct, tag, PCI_BHLC_REG, ct->ct_bhlc);

	DPRINTF(("%x\n", sc->sc_poweron_func));

	return (0);
}

/*
 * int cardbus_function_disable(struct cardbus_softc *, int func)
 *
 *   This function disable a function on a card.  When no functions are
 *  enabled, it turns off the power.
 */
int
cardbus_function_disable(struct cardbus_softc *sc, int func)
{

	DPRINTF(("entering cardbus_function_disable...  "));

	disable_function(sc, func);

	return (0);
}

/*
 * int cardbus_get_capability(cardbus_chipset_tag_t cc,
 *	cardbus_function_tag_t cf, pcitag_t tag, int capid, int *offset,
 *	pcireg_t *value)
 *
 *	Find the specified PCI capability.
 */
int
cardbus_get_capability(cardbus_chipset_tag_t cc, cardbus_function_tag_t cf,
    pcitag_t tag, int capid, int *offset, pcireg_t *value)
{
	pcireg_t reg;
	unsigned int ofs;

	reg = cardbus_conf_read(cc, cf, tag, PCI_COMMAND_STATUS_REG);
	if (!(reg & PCI_STATUS_CAPLIST_SUPPORT))
		return (0);

	ofs = PCI_CAPLIST_PTR(cardbus_conf_read(cc, cf, tag,
	    PCI_CAPLISTPTR_REG));
	while (ofs != 0) {
#ifdef DIAGNOSTIC
		if ((ofs & 3) || (ofs < 0x40))
			panic("cardbus_get_capability");
#endif
		reg = cardbus_conf_read(cc, cf, tag, ofs);
		if (PCI_CAPLIST_CAP(reg) == capid) {
			if (offset)
				*offset = ofs;
			if (value)
				*value = reg;
			return (1);
		}
		ofs = PCI_CAPLIST_NEXT(reg);
	}

	return (0);
}

/*
 * below this line, there are some functions for decoding tuples.
 * They should go out from this file.
 */

static u_int8_t *
decode_tuple(u_int8_t *, u_int8_t *, tuple_decode_func, void *);

static int
decode_tuples(u_int8_t *tuple, int buflen, tuple_decode_func func, void *data)
{
	u_int8_t *tp = tuple;

	if (PCMCIA_CISTPL_LINKTARGET != *tuple) {
		DPRINTF(("WRONG TUPLE: 0x%x\n", *tuple));
		return (0);
	}

	while ((tp = decode_tuple(tp, tuple + buflen, func, data)) != NULL)
		;

	return (1);
}

static u_int8_t *
decode_tuple(u_int8_t *tuple, u_int8_t *end,
    tuple_decode_func func, void *data)
{
	u_int8_t type;
	u_int8_t len;

	type = tuple[0];
	switch (type) {
	case PCMCIA_CISTPL_NULL:
	case PCMCIA_CISTPL_END:
		len = 1;
		break;
	default:
		if (tuple + 2 > end)
			return (NULL);
		len = tuple[1] + 2;
		break;
	}

	if (tuple + len > end)
		return (NULL);

	(*func)(tuple, len, data);

	if (type == PCMCIA_CISTPL_END || tuple + len == end)
		return (NULL);

	return (tuple + len);
}

/*
 * XXX: this is another reason why this code should be shared with PCI.
 */
static int
cardbus_get_powerstate_int(cardbus_devfunc_t ct, pcitag_t tag,
    pcireg_t *state, int offset)
{
	pcireg_t value, now;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;

	value = cardbus_conf_read(cc, cf, tag, offset + PCI_PMCSR);
	now = value & PCI_PMCSR_STATE_MASK;
	switch (now) {
	case PCI_PMCSR_STATE_D0:
	case PCI_PMCSR_STATE_D1:
	case PCI_PMCSR_STATE_D2:
	case PCI_PMCSR_STATE_D3:
		*state = now;
		return 0;
	default:
		return EINVAL;
	}
}

int
cardbus_get_powerstate(cardbus_devfunc_t ct, pcitag_t tag, pcireg_t *state)
{
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;
	int offset;
	pcireg_t value;

	if (!cardbus_get_capability(cc, cf, tag, PCI_CAP_PWRMGMT, &offset, &value))
		return EOPNOTSUPP;

	return cardbus_get_powerstate_int(ct, tag, state, offset);
}

static int
cardbus_set_powerstate_int(cardbus_devfunc_t ct, pcitag_t tag,
    pcireg_t state, int offset, pcireg_t cap_reg)
{
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;

	pcireg_t value, cap, now;

	KASSERT((offset & 0x3) == 0);

	cap = cap_reg >> PCI_PMCR_SHIFT;
	value = cardbus_conf_read(cc, cf, tag, offset + PCI_PMCSR);
	now = value & PCI_PMCSR_STATE_MASK;
	value &= ~PCI_PMCSR_STATE_MASK;	

	if (now == state)
		return 0;
	switch (state) {
	case PCI_PMCSR_STATE_D0:
		break;
	case PCI_PMCSR_STATE_D1:
		if (now == PCI_PMCSR_STATE_D2 || now == PCI_PMCSR_STATE_D3) {
			printf("invalid transition from %d to D1\n", (int)now);
			return EINVAL;
		}
		if (!(cap & PCI_PMCR_D1SUPP)) {
			printf("D1 not supported\n");
			return EOPNOTSUPP;
		}
		break;
	case PCI_PMCSR_STATE_D2:
		if (now == PCI_PMCSR_STATE_D3) {
			printf("invalid transition from %d to D2\n", (int)now);
			return EINVAL;
		}
		if (!(cap & PCI_PMCR_D2SUPP)) {
			printf("D2 not supported\n");
			return EOPNOTSUPP;
		}
		break;
	case PCI_PMCSR_STATE_D3:
		break;
	default:
		return EINVAL;
	}
	value |= state;
	cardbus_conf_write(cc, cf, tag, offset + PCI_PMCSR, value);
	if (state == PCI_PMCSR_STATE_D3 || now == PCI_PMCSR_STATE_D3)
		DELAY(10000);
	else if (state == PCI_PMCSR_STATE_D2 || now == PCI_PMCSR_STATE_D2)
		DELAY(200);

	return 0;
}

int
cardbus_set_powerstate(cardbus_devfunc_t ct, pcitag_t tag, pcireg_t state)
{
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;
	int offset;
	pcireg_t value;

	if (!cardbus_get_capability(cc, cf, tag, PCI_CAP_PWRMGMT, &offset,
	    &value))
		return EOPNOTSUPP;

	return cardbus_set_powerstate_int(ct, tag, state, offset, value);
}

#ifdef CARDBUS_DEBUG
static const char *tuple_name(int);
static const char *tuple_names[] = {
	"TPL_NULL", "TPL_DEVICE", "Reserved", "Reserved", /* 0-3 */
	"CONFIG_CB", "CFTABLE_ENTRY_CB", "Reserved", "BAR", /* 4-7 */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 8-B */
	"Reserved", "Reserved", "Reserved", "Reserved", /* C-F */
	"CHECKSUM", "LONGLINK_A", "LONGLINK_C", "LINKTARGET", /* 10-13 */
	"NO_LINK", "VERS_1", "ALTSTR", "DEVICE_A",
	"JEDEC_C", "JEDEC_A", "CONFIG", "CFTABLE_ENTRY",
	"DEVICE_OC", "DEVICE_OA", "DEVICE_GEO", "DEVICE_GEO_A",
	"MANFID", "FUNCID", "FUNCE", "SWIL", /* 20-23 */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 24-27 */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 28-2B */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 2C-2F */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 30-33 */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 34-37 */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 38-3B */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 3C-3F */
	"VERS_2", "FORMAT", "GEOMETRY", "BYTEORDER",
	"DATE", "BATTERY", "ORG"
};
#define NAME_LEN(x) (sizeof x / sizeof(x[0]))

static const char *
tuple_name(int type)
{

	if (0 <= type && type < NAME_LEN(tuple_names)) {
		return (tuple_names[type]);
	} else if (type == 0xff) {
		return ("END");
	} else {
		return ("Reserved");
	}
}

static void
print_tuple(u_int8_t *tuple, int len, void *data)
{
	int i;

	printf("tuple: %s len %d\n", tuple_name(tuple[0]), len);

	for (i = 0; i < len; ++i) {
		if (i % 16 == 0) {
			printf("  0x%2x:", i);
		}
		printf(" %x", tuple[i]);
		if (i % 16 == 15) {
			printf("\n");
		}
	}
	if (i % 16 != 0) {
		printf("\n");
	}
}
#endif

void
cardbus_conf_capture(cardbus_chipset_tag_t cc, cardbus_function_tag_t cf,
    pcitag_t tag, struct cardbus_conf_state *pcs)
{
	int off;

	for (off = 0; off < 16; off++)
		pcs->reg[off] = cardbus_conf_read(cc, cf, tag, (off * 4));
}

void
cardbus_conf_restore(cardbus_chipset_tag_t cc, cardbus_function_tag_t cf,
    pcitag_t tag, struct cardbus_conf_state *pcs)
{
	int off;
	pcireg_t val;

	for (off = 15; off >= 0; off--) {
		val = cardbus_conf_read(cc, cf, tag, (off * 4));
		if (val != pcs->reg[off])
			cardbus_conf_write(cc, cf,tag, (off * 4), pcs->reg[off]);
	}
}

struct cardbus_child_power {
	struct cardbus_conf_state p_cardbusconf;
	cardbus_devfunc_t p_ct;
	pcitag_t p_tag;
	cardbus_chipset_tag_t p_cc;
	cardbus_function_tag_t p_cf;
	pcireg_t p_pm_cap;
	bool p_has_pm;
	int p_pm_offset;
};

static bool
cardbus_child_suspend(device_t dv, const pmf_qual_t *qual)
{
	struct cardbus_child_power *priv = device_pmf_bus_private(dv);

	cardbus_conf_capture(priv->p_cc, priv->p_cf, priv->p_tag,
	    &priv->p_cardbusconf);

	if (priv->p_has_pm &&
	    cardbus_set_powerstate_int(priv->p_ct, priv->p_tag,
	    PCI_PMCSR_STATE_D3, priv->p_pm_offset, priv->p_pm_cap)) {
		aprint_error_dev(dv, "unsupported state, continuing.\n");
		return false;
	}

	Cardbus_function_disable(priv->p_ct);

	return true;
}

static bool
cardbus_child_resume(device_t dv, const pmf_qual_t *qual)
{
	struct cardbus_child_power *priv = device_pmf_bus_private(dv);

	Cardbus_function_enable(priv->p_ct);

	if (priv->p_has_pm &&
	    cardbus_set_powerstate_int(priv->p_ct, priv->p_tag,
	    PCI_PMCSR_STATE_D0, priv->p_pm_offset, priv->p_pm_cap)) {
		aprint_error_dev(dv, "unsupported state, continuing.\n");
		return false;
	}

	cardbus_conf_restore(priv->p_cc, priv->p_cf, priv->p_tag,
	    &priv->p_cardbusconf);

	return true;
}

static void
cardbus_child_deregister(device_t dv)
{
	struct cardbus_child_power *priv = device_pmf_bus_private(dv);

	free(priv, M_DEVBUF);
}

static bool
cardbus_child_register(device_t child)
{
	device_t self = device_parent(child);
	struct cardbus_softc *sc = device_private(self);
	struct cardbus_devfunc *ct;
	struct cardbus_child_power *priv;
	int off;
	pcireg_t reg;

	ct = sc->sc_funcs[device_locator(child, CARDBUSCF_FUNCTION)];

	priv = malloc(sizeof(*priv), M_DEVBUF, M_WAITOK);

	priv->p_ct = ct;
	priv->p_cc = ct->ct_cc;
	priv->p_cf = ct->ct_cf;
	priv->p_tag = cardbus_make_tag(priv->p_cc, priv->p_cf, ct->ct_bus,
	    ct->ct_func);

	if (cardbus_get_capability(priv->p_cc, priv->p_cf, priv->p_tag,
				   PCI_CAP_PWRMGMT, &off, &reg)) {
		priv->p_has_pm = true;
		priv->p_pm_offset = off;
		priv->p_pm_cap = reg;
	} else {
		priv->p_has_pm = false;
		priv->p_pm_offset = -1;
	}

	device_pmf_bus_register(child, priv, cardbus_child_suspend,
	    cardbus_child_resume, 0, cardbus_child_deregister);

	return true;
}

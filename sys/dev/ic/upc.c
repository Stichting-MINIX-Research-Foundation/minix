/* $NetBSD: upc.c,v 1.15 2012/10/27 17:18:23 chs Exp $ */
/*-
 * Copyright (c) 2000, 2003 Ben Harris
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
/*
 * upc - driver for C&T Universal Peripheral Controllers
 *
 * Supports:
 * 82C710 Universal Peripheral Controller
 * 82C711 Universal Peripheral Controller II
 * 82C721 Universal Peripheral Controller III (untested)
 *
 * The 82C710 is substantially different from its successors.
 * Functions that just handle the 82C710 are named upc1_*, which those
 * that handle the 82C711 and 82C721 are named upc2_*.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: upc.c,v 1.15 2012/10/27 17:18:23 chs Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <sys/bus.h>

#include <dev/ata/atavar.h> /* XXX needed by wdcvar.h */
#include <dev/ic/comreg.h>
#include <dev/ic/lptreg.h>
#include <dev/ic/lptvar.h>
#include <dev/ic/wdcreg.h>
#include <dev/ic/wdcvar.h>
#include <dev/ic/upcreg.h>
#include <dev/ic/upcvar.h>

#include "locators.h"

/* Conventional port to use for 82C710 configuration */
#define UPC1_PORT_CRI	0x390
#define UPC1_PORT_CAP	(UPC1_PORT_CRI + 1)

static int upc1_probe(struct upc_softc *);
static void upc1_attach(struct upc_softc *);
static void upc2_attach(struct upc_softc *);
static void upc_found(struct upc_softc *, char const *, int, int,
		      struct upc_irqhandle *);
static void upc_found2(struct upc_softc *, char const *, int, int, int, int,
		       struct upc_irqhandle *);
static int upc_print(void *, char const *);
static int upc2_com3_addr(int);
static int upc2_com4_addr(int);

void
upc_attach(struct upc_softc *sc)
{

	if (upc1_probe(sc))
		upc1_attach(sc);
	else
		upc2_attach(sc);
}

static int
upc1_probe(struct upc_softc *sc)
{

	return upc1_read_config(sc, UPC1_CFGADDR_CONFBASE) ==
	    UPC1_PORT_CRI >> UPC1_CONFBASE_SHIFT;
}

static void
upc1_attach(struct upc_softc *sc)
{
	u_int8_t cr[16];
	int i;

	aprint_normal(": 82C710\n");
	/* Dump configuration */
	for (i = 0; i < 16; i++)
		cr[i] = upc1_read_config(sc, i);

	aprint_verbose_dev(sc->sc_dev, "config state");
	for (i = 0; i < 16; i++)
		aprint_verbose(" %02x", cr[i]);
	aprint_verbose("\n");

	/* FDC */
	if (cr[UPC1_CFGADDR_CRC] & UPC1_CRC_FDCEN)
		upc_found(sc, "fdc", UPC_PORT_FDCBASE, 2, &sc->sc_fintr);
	/* IDE */
	if (cr[UPC1_CFGADDR_CRC] & UPC1_CRC_IDEEN)
		upc_found2(sc, "wdc", UPC_PORT_IDECMDBASE, 8,
			   UPC_PORT_IDECTLBASE, 2, &sc->sc_wintr);
	/* Parallel */
	if (cr[UPC1_CFGADDR_CR0] & UPC1_CR0_PEN)
		upc_found(sc, "lpt",
		    cr[UPC1_CFGADDR_PARBASE] << UPC1_PARBASE_SHIFT,
		    LPT_NPORTS, &sc->sc_pintr);
	/* UART */
	if (cr[UPC1_CFGADDR_CR0] & UPC1_CR0_SEN)
		upc_found(sc, "com",
		    cr[UPC1_CFGADDR_UARTBASE] << UPC1_UARTBASE_SHIFT,
		    COM_NPORTS, &sc->sc_irq4);
	/* Mouse */
	/* XXX not yet supported */
}

static void
upc2_attach(struct upc_softc *sc)
{
	u_int8_t cr[5];
	int i;

	aprint_normal(": 82C711/82C721");
	/* Dump configuration */
	for (i = 0; i < 5; i++)
		cr[i] = upc2_read_config(sc, i);

	aprint_verbose(", config state %02x %02x %02x %02x %02x",
	       cr[0], cr[1], cr[2], cr[3], cr[4]);
	aprint_normal("\n");

	/* "Find" the attached devices */
	/* FDC */
	if (cr[0] & UPC2_CR0_FDC_ENABLE)
		upc_found(sc, "fdc", UPC_PORT_FDCBASE, 2, &sc->sc_fintr);
	/* IDE */
	if (cr[0] & UPC2_CR0_IDE_ENABLE)
		upc_found2(sc, "wdc", UPC_PORT_IDECMDBASE, 8,
			   UPC_PORT_IDECTLBASE, 2, &sc->sc_wintr);
	/* Parallel */
	switch (cr[1] & UPC2_CR1_LPT_MASK) {
	case UPC2_CR1_LPT_3BC:
		upc_found(sc, "lpt", 0x3bc, LPT_NPORTS, &sc->sc_pintr);
		break;
	case UPC2_CR1_LPT_378:
		upc_found(sc, "lpt", 0x378, LPT_NPORTS, &sc->sc_pintr);
		break;
	case UPC2_CR1_LPT_278:
		upc_found(sc, "lpt", 0x278, LPT_NPORTS, &sc->sc_pintr);
		break;
	}
	/* UART1 */
	if (cr[2] & UPC2_CR2_UART1_ENABLE) {
		switch (cr[2] & UPC2_CR2_UART1_MASK) {
		case UPC2_CR2_UART1_3F8:
			upc_found(sc, "com", 0x3f8, COM_NPORTS, &sc->sc_irq4);
			break;
		case UPC2_CR2_UART1_2F8:
			upc_found(sc, "com", 0x2f8, COM_NPORTS, &sc->sc_irq3);
			break;
		case UPC2_CR2_UART1_COM3:
			upc_found(sc, "com", upc2_com3_addr(cr[1]), COM_NPORTS,
				  &sc->sc_irq4);
			break;
		case UPC2_CR2_UART1_COM4:
			upc_found(sc, "com", upc2_com4_addr(cr[1]), COM_NPORTS,
				  &sc->sc_irq3);
			break;
		}
	}
	/* UART2 */
	if (cr[2] & UPC2_CR2_UART2_ENABLE) {
		switch (cr[2] & UPC2_CR2_UART2_MASK) {
		case UPC2_CR2_UART2_3F8:
			upc_found(sc, "com", 0x3f8, COM_NPORTS, &sc->sc_irq4);
			break;
		case UPC2_CR2_UART2_2F8:
			upc_found(sc, "com", 0x2f8, COM_NPORTS, &sc->sc_irq3);
			break;
		case UPC2_CR2_UART2_COM3:
			upc_found(sc, "com", upc2_com3_addr(cr[1]), COM_NPORTS,
				  &sc->sc_irq4);
			break;
		case UPC2_CR2_UART2_COM4:
			upc_found(sc, "com", upc2_com4_addr(cr[1]), COM_NPORTS,
				  &sc->sc_irq3);
			break;
		}
	}

}

static void
upc_found(struct upc_softc *sc, char const *devtype, int offset, int size,
	  struct upc_irqhandle *uih)
{
	struct upc_attach_args ua;
	int locs[UPCCF_NLOCS];

	ua.ua_devtype = devtype;
	ua.ua_offset = offset;
	ua.ua_iot = sc->sc_iot;
	bus_space_subregion(sc->sc_iot, sc->sc_ioh, offset, size, &ua.ua_ioh);
	ua.ua_irqhandle = uih;

	locs[UPCCF_OFFSET] = offset;

	config_found_sm_loc(sc->sc_dev, "upc", locs, &ua,
			    upc_print, config_stdsubmatch);
}

static void
upc_found2(struct upc_softc *sc, char const *devtype, int offset, int size,
	   int offset2, int size2, struct upc_irqhandle *uih)
{
	struct upc_attach_args ua;
	int locs[UPCCF_NLOCS];

	ua.ua_devtype = devtype;
	ua.ua_offset = offset;
	ua.ua_iot = sc->sc_iot;
	bus_space_subregion(sc->sc_iot, sc->sc_ioh, offset, size, &ua.ua_ioh);
	bus_space_subregion(sc->sc_iot, sc->sc_ioh, offset2, size2,
			    &ua.ua_ioh2);
	ua.ua_irqhandle = uih;

	locs[UPCCF_OFFSET] = offset;

	config_found_sm_loc(sc->sc_dev, "upc", locs, &ua,
			    upc_print, config_stdsubmatch);
}

void
upc_intr_establish(struct upc_irqhandle *uih, int level, int (*func)(void *),
		   void *arg) {

	uih->uih_level = level;
	uih->uih_func = func;
	uih->uih_arg = arg;
	/* Actual MD establishment will be handled later by bus attachment. */
}

static int
upc2_com3_addr(int cr1)
{

	switch (cr1 & UPC2_CR1_COM34_MASK) {
	case UPC2_CR1_COM34_338_238:
		return 0x338;
	case UPC2_CR1_COM34_3E8_2E8:
		return 0x3e8;
	case UPC2_CR1_COM34_2E8_2E0:
		return 0x2e8;
	case UPC2_CR1_COM34_220_228:
		return 0x220;
	}
	return -1;
}

static int
upc2_com4_addr(int cr1)
{

	switch (cr1 & UPC2_CR1_COM34_MASK) {
	case UPC2_CR1_COM34_338_238:
		return 0x238;
	case UPC2_CR1_COM34_3E8_2E8:
		return 0x2e8;
	case UPC2_CR1_COM34_2E8_2E0:
		return 0x2e0;
	case UPC2_CR1_COM34_220_228:
		return 0x228;
	}
	return -1;
}

static int
upc_print(void *aux, char const *pnp)
{
	struct upc_attach_args *ua = aux;

	if (pnp)
		aprint_normal("%s at %s", ua->ua_devtype, pnp);
	aprint_normal(" offset 0x%x", ua->ua_offset);
	return UNCONF;
}

int
upc1_read_config(struct upc_softc *sc, int reg)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int retval;

	/* Switch into configuration mode. */
	bus_space_write_1(iot, ioh, UPC1_PORT_CFG1, UPC1_CFGMAGIC_1);
	bus_space_write_1(iot, ioh, UPC1_PORT_CFG2, UPC1_CFGMAGIC_2);
	bus_space_write_1(iot, ioh, UPC1_PORT_CFG2, UPC1_CFGMAGIC_3);
	bus_space_write_1(iot, ioh, UPC1_PORT_CFG2,
	    UPC1_PORT_CRI >> UPC1_CONFBASE_SHIFT);
	bus_space_write_1(iot, ioh, UPC1_PORT_CFG1,
	    (UPC1_PORT_CRI >> UPC1_CONFBASE_SHIFT) ^ 0xff);

	/* Read register. */
	bus_space_write_1(iot, ioh, UPC1_PORT_CRI, reg);
	retval = bus_space_read_1(iot, ioh, UPC1_PORT_CAP);

	/* Leave configuration mode. */
	bus_space_write_1(iot, ioh, UPC1_PORT_CRI, UPC1_CFGADDR_EXIT);
	bus_space_write_1(iot, ioh, UPC1_PORT_CAP, 0);
	return retval;
}

void
upc1_write_config(struct upc_softc *sc, int reg, int val)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	/* Switch into configuration mode. */
	bus_space_write_1(iot, ioh, UPC1_PORT_CFG1, UPC1_CFGMAGIC_1);
	bus_space_write_1(iot, ioh, UPC1_PORT_CFG2, UPC1_CFGMAGIC_2);
	bus_space_write_1(iot, ioh, UPC1_PORT_CFG2, UPC1_CFGMAGIC_3);
	bus_space_write_1(iot, ioh, UPC1_PORT_CFG2,
	    UPC1_PORT_CRI >> UPC1_CONFBASE_SHIFT);
	bus_space_write_1(iot, ioh, UPC1_PORT_CFG1,
	    (UPC1_PORT_CRI >> UPC1_CONFBASE_SHIFT) ^ 0xff);

	/* Read register. */
	bus_space_write_1(iot, ioh, UPC1_PORT_CRI, reg);
	bus_space_write_1(iot, ioh, UPC1_PORT_CAP, val);

	/* Leave configuration mode. */
	bus_space_write_1(iot, ioh, UPC1_PORT_CRI, UPC1_CFGADDR_EXIT);
	bus_space_write_1(iot, ioh, UPC1_PORT_CAP, 0);
}

int
upc2_read_config(struct upc_softc *sc, int reg)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int retval;

	/* Switch into configuration mode. */
	bus_space_write_1(iot, ioh, UPC2_PORT_CFGADDR, UPC2_CFGMAGIC_ENTER);
	bus_space_write_1(iot, ioh, UPC2_PORT_CFGADDR, UPC2_CFGMAGIC_ENTER);

	/* Read register. */
	bus_space_write_1(iot, ioh, UPC2_PORT_CFGADDR, reg);
	retval = bus_space_read_1(iot, ioh, UPC2_PORT_CFGDATA);

	/* Leave configuration mode. */
	bus_space_write_1(iot, ioh, UPC2_PORT_CFGADDR, UPC2_CFGMAGIC_EXIT);
	return retval;
}

void
upc2_write_config(struct upc_softc *sc, int reg, int val)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	/* Switch into configuration mode. */
	bus_space_write_1(iot, ioh, UPC2_PORT_CFGADDR, UPC2_CFGMAGIC_ENTER);
	bus_space_write_1(iot, ioh, UPC2_PORT_CFGADDR, UPC2_CFGMAGIC_ENTER);

	/* Write register. */
	bus_space_write_1(iot, ioh, UPC2_PORT_CFGADDR, reg);
	bus_space_write_1(iot, ioh, UPC2_PORT_CFGDATA, val);

	/* Leave configuration mode. */
	bus_space_write_1(iot, ioh, UPC2_PORT_CFGADDR, UPC2_CFGMAGIC_EXIT);
}

/*	$NetBSD: wbsio.c,v 1.9 2012/01/18 00:23:30 jakllsch Exp $	*/
/*	$OpenBSD: wbsio.c,v 1.5 2009/03/29 21:53:52 sthen Exp $	*/
/*
 * Copyright (c) 2008 Mark Kettenis <kettenis@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Winbond LPC Super I/O driver.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <sys/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

/* ISA bus registers */
#define WBSIO_INDEX		0x00	/* Configuration Index Register */
#define WBSIO_DATA		0x01	/* Configuration Data Register */

#define WBSIO_IOSIZE		0x02	/* ISA I/O space size */

#define WBSIO_CONF_EN_MAGIC	0x87	/* enable configuration mode */
#define WBSIO_CONF_DS_MAGIC	0xaa	/* disable configuration mode */

/* Configuration Space Registers */
#define WBSIO_LDN		0x07	/* Logical Device Number */
#define WBSIO_ID		0x20	/* Device ID */
#define WBSIO_REV		0x21	/* Device Revision */

#define WBSIO_ID_W83627HF	0x52
#define WBSIO_ID_W83627THF	0x82
#define WBSIO_ID_W83627EHF	0x88
#define WBSIO_ID_W83627DHG	0xa0
#define WBSIO_ID_W83627SF	0x59
#define WBSIO_ID_W83637HF	0x70
#define WBSIO_ID_W83667HG	0xa5
#define WBSIO_ID_W83697HF	0x60

/* Logical Device Number (LDN) Assignments */
#define WBSIO_LDN_HM		0x0b

/* Hardware Monitor Control Registers (LDN B) */
#define WBSIO_HM_ADDR_MSB	0x60	/* Address [15:8] */
#define WBSIO_HM_ADDR_LSB	0x61	/* Address [7:0] */

struct wbsio_softc {
	device_t	sc_dev;
	device_t	sc_lm_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct isa_attach_args	sc_ia;
	struct isa_io		sc_io;
};

int	wbsio_probe(device_t, cfdata_t, void *);
void	wbsio_attach(device_t, device_t, void *);
int	wbsio_detach(device_t, int);
int	wbsio_rescan(device_t, const char *, const int *);
void	wbsio_childdet(device_t, device_t);
int	wbsio_print(void *, const char *);

static int wbsio_search(device_t, cfdata_t, const int *, void *);

CFATTACH_DECL2_NEW(wbsio, sizeof(struct wbsio_softc),
    wbsio_probe, wbsio_attach, wbsio_detach, NULL,
    wbsio_rescan, wbsio_childdet);

static __inline void
wbsio_conf_enable(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	bus_space_write_1(iot, ioh, WBSIO_INDEX, WBSIO_CONF_EN_MAGIC);
	bus_space_write_1(iot, ioh, WBSIO_INDEX, WBSIO_CONF_EN_MAGIC);
}

static __inline void
wbsio_conf_disable(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	bus_space_write_1(iot, ioh, WBSIO_INDEX, WBSIO_CONF_DS_MAGIC);
}

static __inline uint8_t
wbsio_conf_read(bus_space_tag_t iot, bus_space_handle_t ioh, uint8_t index)
{
	bus_space_write_1(iot, ioh, WBSIO_INDEX, index);
	return (bus_space_read_1(iot, ioh, WBSIO_DATA));
}

static __inline void
wbsio_conf_write(bus_space_tag_t iot, bus_space_handle_t ioh, uint8_t index,
    uint8_t data)
{
	bus_space_write_1(iot, ioh, WBSIO_INDEX, index);
	bus_space_write_1(iot, ioh, WBSIO_DATA, data);
}

int
wbsio_probe(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	uint8_t reg;

	/* Must supply an address */
	if (ia->ia_nio < 1)
		return 0;

	if (ISA_DIRECT_CONFIG(ia))
		return 0;

	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return 0;

	/* Match by device ID */
	iot = ia->ia_iot;
	if (bus_space_map(iot, ia->ia_io[0].ir_addr, WBSIO_IOSIZE, 0, &ioh))
		return 0;
	wbsio_conf_enable(iot, ioh);
	reg = wbsio_conf_read(iot, ioh, WBSIO_ID);
	aprint_debug("wbsio_probe: id 0x%02x\n", reg);
	wbsio_conf_disable(iot, ioh);
	bus_space_unmap(iot, ioh, WBSIO_IOSIZE);
	switch (reg) {
	case WBSIO_ID_W83627HF:
	case WBSIO_ID_W83627THF:
	case WBSIO_ID_W83627EHF:
	case WBSIO_ID_W83627DHG:
	case WBSIO_ID_W83637HF:
	case WBSIO_ID_W83697HF:
		ia->ia_nio = 1;
		ia->ia_io[0].ir_size = WBSIO_IOSIZE;
		ia->ia_niomem = 0;
		ia->ia_nirq = 0;
		ia->ia_ndrq = 0;
		return 1;
	}

	return 0;
}

void
wbsio_attach(device_t parent, device_t self, void *aux)
{
	struct wbsio_softc *sc = device_private(self);
	struct isa_attach_args *ia = aux;
	const char *desc = NULL;
	uint8_t reg;

	sc->sc_dev = self;

	sc->sc_ia = *ia;

	/* Map ISA I/O space */
	sc->sc_iot = ia->ia_iot;
	if (bus_space_map(sc->sc_iot, ia->ia_io[0].ir_addr,
	    WBSIO_IOSIZE, 0, &sc->sc_ioh)) {
		aprint_error(": can't map i/o space\n");
		return;
	}

	/* Enter configuration mode */
	wbsio_conf_enable(sc->sc_iot, sc->sc_ioh);

	/* Read device ID */
	reg = wbsio_conf_read(sc->sc_iot, sc->sc_ioh, WBSIO_ID);
	switch (reg) {
	case WBSIO_ID_W83627HF:
		desc = "W83627HF";
		break;
	case WBSIO_ID_W83627THF:
		desc = "W83627THF";
		break;
	case WBSIO_ID_W83627EHF:
		desc = "W83627EHF";
		break;
	case WBSIO_ID_W83627DHG:
		desc = "W83627DHG";
		break;
	case WBSIO_ID_W83637HF:
		desc = "W83637HF";
		break;
	case WBSIO_ID_W83667HG:
		desc = "W83667HG";
		break;
	case WBSIO_ID_W83697HF:
		desc = "W83697HF";
		break;
	}

	/* Read device revision */
	reg = wbsio_conf_read(sc->sc_iot, sc->sc_ioh, WBSIO_REV);

	aprint_naive("\n");
	aprint_normal(": Winbond LPC Super I/O %s rev 0x%02x\n", desc, reg);

	/* Escape from configuration mode */
	wbsio_conf_disable(sc->sc_iot, sc->sc_ioh);

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	wbsio_rescan(self, "wbsio", NULL);
}

int
wbsio_detach(device_t self, int flags)
{
	struct wbsio_softc *sc = device_private(self);
	int rc;

	if ((rc = config_detach_children(self, flags)) != 0)
		return rc;
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, WBSIO_IOSIZE);
	pmf_device_deregister(self);
	return 0;
}

int
wbsio_rescan(device_t self, const char *ifattr, const int *locators)
{

	config_search_loc(wbsio_search, self, ifattr, locators, NULL);

	return 0;
}

void
wbsio_childdet(device_t self, device_t child)
{
	struct wbsio_softc *sc = device_private(self);

	if (sc->sc_lm_dev == child)
		sc->sc_lm_dev = NULL;
}

static int
wbsio_search(device_t parent, cfdata_t cf, const int *slocs, void *aux)
{
	struct wbsio_softc *sc = device_private(parent);
	uint16_t iobase;
	uint8_t reg0, reg1;

	/* Enter configuration mode */
	wbsio_conf_enable(sc->sc_iot, sc->sc_ioh);

	/* Select HM logical device */
	wbsio_conf_write(sc->sc_iot, sc->sc_ioh, WBSIO_LDN, WBSIO_LDN_HM);

	/*
	 * The address should be 8-byte aligned, but it seems some
	 * BIOSes ignore this.  They get away with it, because
	 * Apparently the hardware simply ignores the lower three
	 * bits.  We do the same here.
	 */
	reg0 = wbsio_conf_read(sc->sc_iot, sc->sc_ioh, WBSIO_HM_ADDR_LSB);
	reg1 = wbsio_conf_read(sc->sc_iot, sc->sc_ioh, WBSIO_HM_ADDR_MSB);

	/* Escape from configuration mode */
	wbsio_conf_disable(sc->sc_iot, sc->sc_ioh);

	iobase = (reg1 << 8) | (reg0 & ~0x7);

	if (iobase == 0)
		return -1;

	sc->sc_ia.ia_nio = 1;
	sc->sc_ia.ia_io = &sc->sc_io;
	sc->sc_ia.ia_io[0].ir_addr = iobase;
	sc->sc_ia.ia_io[0].ir_size = 8;
	sc->sc_ia.ia_niomem = 0;
	sc->sc_ia.ia_nirq = 0;
	sc->sc_ia.ia_ndrq = 0;
	sc->sc_lm_dev = config_attach(parent, cf, &sc->sc_ia, wbsio_print);

	return 0;
}

int
wbsio_print(void *aux, const char *pnp)
{
	struct isa_attach_args *ia = aux;

	if (pnp)
		aprint_normal("%s", pnp);
	if (ia->ia_io[0].ir_size)
		aprint_normal(" port 0x%x", ia->ia_io[0].ir_addr);
	if (ia->ia_io[0].ir_size > 1)
		aprint_normal("-0x%x", ia->ia_io[0].ir_addr +
		    ia->ia_io[0].ir_size - 1);
	return (UNCONF);
}

MODULE(MODULE_CLASS_DRIVER, wbsio, NULL);

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
wbsio_modcmd(modcmd_t cmd, void *opaque)
{
	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		return config_init_component(cfdriver_ioconf_wbsio,
		    cfattach_ioconf_wbsio, cfdata_ioconf_wbsio);
#else
		return 0;
#endif
	case MODULE_CMD_FINI:
#ifdef _MODULE
		return config_fini_component(cfdriver_ioconf_wbsio,
		    cfattach_ioconf_wbsio, cfdata_ioconf_wbsio);
#else
		return 0;
#endif
	default:
		return ENOTTY;
	}
}

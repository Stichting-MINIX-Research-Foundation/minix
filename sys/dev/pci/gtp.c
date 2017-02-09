/* $NetBSD: gtp.c,v 1.19 2012/10/27 17:18:32 chs Exp $ */
/*	$OpenBSD: gtp.c,v 1.1 2002/06/03 16:13:21 mickey Exp $	*/

/*
 * Copyright (c) 2002 Vladimir Popov <jumbo@narod.ru>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Gemtek PCI Radio Card Device Driver */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: gtp.c,v 1.19 2012/10/27 17:18:32 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/radioio.h>

#include <sys/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/tea5757.h>
#include <dev/radio_if.h>

#define PCI_CBIO 0x10

static int	gtp_match(device_t, cfdata_t, void *);
static void	gtp_attach(device_t, device_t, void *);

static int     gtp_get_info(void *, struct radio_info *);
static int     gtp_set_info(void *, struct radio_info *);
static int     gtp_search(void *, int);

#define GEMTEK_PCI_CAPS	RADIO_CAPS_DETECT_SIGNAL |			\
			RADIO_CAPS_DETECT_STEREO |			\
			RADIO_CAPS_SET_MONO |				\
			RADIO_CAPS_HW_SEARCH |				\
			RADIO_CAPS_HW_AFC |				\
			RADIO_CAPS_LOCK_SENSITIVITY

#define GEMTEK_PCI_MUTE		0x00
#define GEMTEK_PCI_RSET		0x10

#define GEMTEK_PCI_SIGNAL	0x08
#define GEMTEK_PCI_STEREO	0x08

#define GTP_WREN_ON		(1 << 2)
#define GTP_WREN_OFF		(0 << 2)

#define GTP_DATA_ON		(1 << 1)
#define GTP_DATA_OFF		(0 << 1)

#define GTP_CLCK_ON		(1 << 0)
#define GTP_CLCK_OFF		(0 << 0)

#define GTP_READ_CLOCK_LOW	(GTP_WREN_OFF | GTP_DATA_ON | GTP_CLCK_OFF)
#define GTP_READ_CLOCK_HIGH	(GTP_WREN_OFF | GTP_DATA_ON | GTP_CLCK_ON)

/* define our interface to the high-level radio driver */

static const struct radio_hw_if gtp_hw_if = {
	NULL, /* open */
	NULL, /* close */
	gtp_get_info,
	gtp_set_info,
	gtp_search
};

struct gtp_softc {
	int	mute;
	u_int8_t	vol;
	u_int32_t	freq;
	u_int32_t	stereo;
	u_int32_t	lock;

	struct tea5757_t	tea;
};

CFATTACH_DECL_NEW(gtp, sizeof(struct gtp_softc),
    gtp_match, gtp_attach, NULL, NULL);

static void	gtp_set_mute(struct gtp_softc *);
static void	gtp_write_bit(bus_space_tag_t, bus_space_handle_t, bus_size_t,
			      int);
static void	gtp_init(bus_space_tag_t, bus_space_handle_t, bus_size_t,
			 u_int32_t);
static void	gtp_rset(bus_space_tag_t, bus_space_handle_t, bus_size_t,
			 u_int32_t);
static int	gtp_state(bus_space_tag_t, bus_space_handle_t);
static uint32_t	gtp_hardware_read(bus_space_tag_t, bus_space_handle_t,
				  bus_size_t);

static int
gtp_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pci_attach_args *pa = aux;
	/* FIXME:
	 * Guillemot produces the card that
	 * was originally developed by Gemtek
	 */
#if 0
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_GEMTEK &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_GEMTEK_PR103)
		return (1);
#else
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_GUILLEMOT &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_GUILLEMOT_MAXIRADIO)
		return (1);
#endif
	return (0);
}

static void
gtp_attach(device_t parent, device_t self, void *aux)
{
	struct gtp_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	cfdata_t cf = device_cfdata(self);
	pci_chipset_tag_t pc = pa->pa_pc;
	bus_size_t iosize;
	pcireg_t csr;

	pci_aprint_devinfo(pa, "Radio controller");

	/* Map I/O registers */
	if (pci_mapreg_map(pa, PCI_CBIO, PCI_MAPREG_TYPE_IO, 0, &sc->tea.iot,
	    &sc->tea.ioh, NULL, &iosize)) {
		aprint_error(": can't map i/o space\n");
		return;
	}

	/* Enable the card */
	csr = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MASTER_ENABLE);

	sc->vol = 0;
	sc->mute = 0;
	sc->freq = MIN_FM_FREQ;
	sc->stereo = TEA5757_STEREO;
	sc->lock = TEA5757_S030;
	sc->tea.offset = 0;
	sc->tea.flags = cf->cf_flags;
	sc->tea.init = gtp_init;
	sc->tea.rset = gtp_rset;
	sc->tea.write_bit = gtp_write_bit;
	sc->tea.read = gtp_hardware_read;

	aprint_normal(": Gemtek PR103\n");

	radio_attach_mi(&gtp_hw_if, sc, self);
}

static int
gtp_get_info(void *v, struct radio_info *ri)
{
	struct gtp_softc *sc = v;

	ri->mute = sc->mute;
	ri->volume = sc->vol ? 255 : 0;
	ri->stereo = sc->stereo == TEA5757_STEREO ? 1 : 0;
	ri->caps = GEMTEK_PCI_CAPS;
	ri->rfreq = 0;
	ri->lock = tea5757_decode_lock(sc->lock);

	/* Frequency read unsupported */
	ri->freq = sc->freq;

	ri->info = gtp_state(sc->tea.iot, sc->tea.ioh);
	gtp_set_mute(sc);

	return (0);
}

static int
gtp_set_info(void *v, struct radio_info *ri)
{
	struct gtp_softc *sc = v;

	sc->mute = ri->mute ? 1 : 0;
	sc->vol = ri->volume ? 255 : 0;
	sc->stereo = ri->stereo ? TEA5757_STEREO: TEA5757_MONO;
	sc->lock = tea5757_encode_lock(ri->lock);
	ri->freq = sc->freq = tea5757_set_freq(&sc->tea,
	    sc->lock, sc->stereo, ri->freq);
	gtp_set_mute(sc);

	return (0);
}

static int
gtp_search(void *v, int f)
{
	struct gtp_softc *sc = v;

	tea5757_search(&sc->tea, sc->lock, sc->stereo, f);
	gtp_set_mute(sc);

	return (0);
}

static void
gtp_set_mute(struct gtp_softc *sc)
{
	if (sc->mute || !sc->vol)
		bus_space_write_2(sc->tea.iot, sc->tea.ioh, 0, GEMTEK_PCI_MUTE);
	else
		sc->freq = tea5757_set_freq(&sc->tea,
		    sc->lock, sc->stereo, sc->freq);
}

static void
gtp_write_bit(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off,
		int bit)
{
	u_int8_t data;

	data = bit ? GTP_DATA_ON : GTP_DATA_OFF;
	bus_space_write_1(iot, ioh, off, GTP_WREN_ON | GTP_CLCK_OFF | data);
	bus_space_write_1(iot, ioh, off, GTP_WREN_ON | GTP_CLCK_ON  | data);
	bus_space_write_1(iot, ioh, off, GTP_WREN_ON | GTP_CLCK_OFF | data);
}

static void
gtp_init(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off,
    u_int32_t d)
{
	bus_space_write_1(iot, ioh, off, GTP_WREN_ON | GTP_DATA_ON | GTP_CLCK_OFF);
}

static void
gtp_rset(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off,
    u_int32_t d)
{
	bus_space_write_1(iot, ioh, off, GEMTEK_PCI_RSET);
}

static uint32_t
gtp_hardware_read(bus_space_tag_t iot, bus_space_handle_t ioh,
    bus_size_t off)
{
	/* UNSUPPORTED */
	return 0;
}

static int
gtp_state(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	int ret;

	bus_space_write_2(iot, ioh, 0,
	    GTP_DATA_ON | GTP_WREN_OFF | GTP_CLCK_OFF);
	ret  = bus_space_read_2(iot, ioh, 0) &
	    GEMTEK_PCI_STEREO?  0 : RADIO_INFO_STEREO;
	bus_space_write_2(iot, ioh, 0,
	    GTP_DATA_ON | GTP_WREN_OFF | GTP_CLCK_ON);
	ret |= bus_space_read_2(iot, ioh, 0) &
	    GEMTEK_PCI_SIGNAL?  0 : RADIO_INFO_SIGNAL;

	return ret;
}

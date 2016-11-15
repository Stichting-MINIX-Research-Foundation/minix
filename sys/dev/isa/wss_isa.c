/*	$NetBSD: wss_isa.c,v 1.29 2011/06/02 14:12:25 tsutsui Exp $	*/

/*
 * Copyright (c) 1994 John Brezak
 * Copyright (c) 1991-1993 Regents of the University of California.
 * All rights reserved.
 *
 * MAD support:
 * Copyright (c) 1996 Lennart Augustsson
 * Based on code which is
 * Copyright (c) 1994 Hannu Savolainen
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: wss_isa.c,v 1.29 2011/06/02 14:12:25 tsutsui Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/errno.h>

#include <sys/cpu.h>
#include <sys/intr.h>
#include <sys/bus.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/ad1848reg.h>
#include <dev/isa/ad1848var.h>
#include <dev/isa/wssreg.h>
#include <dev/isa/wssvar.h>
#include <dev/isa/madreg.h>

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (wssdebug) printf x
extern int	wssdebug;
#else
#define DPRINTF(x)
#endif

static int	wssfind(device_t, struct wss_softc *, int,
		    struct isa_attach_args *);

static void	madprobe(struct wss_softc *, int);
static void	madunmap(struct wss_softc *);
static int	detect_mad16(struct wss_softc *, int);

int		wss_isa_probe(device_t, cfdata_t, void *);
void		wss_isa_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(wss_isa, sizeof(struct wss_softc),
    wss_isa_probe, wss_isa_attach, NULL, NULL);

/*
 * Probe for the Microsoft Sound System hardware.
 */
int
wss_isa_probe(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia;
	struct device probedev;
	struct wss_softc probesc, *sc;
	struct ad1848_softc *ac;

	ia = aux;
	if (ia->ia_nio < 1)
		return 0;
	if (ia->ia_nirq < 1)
		return 0;
	if (ia->ia_ndrq < 1)
		return 0;

	if (ISA_DIRECT_CONFIG(ia))
		return 0;

	memset(&probedev, 0, sizeof probedev);
	memset(&probesc, 0, sizeof probesc);
	sc = &probesc;
	ac = &sc->sc_ad1848.sc_ad1848;
	ac->sc_dev = &probedev;
	ac->sc_dev->dv_cfdata = match;
	if (wssfind(parent, sc, 1, aux)) {
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, WSS_CODEC);
		ad1848_isa_unmap(&sc->sc_ad1848);
		madunmap(sc);
		return 1;
	} else
		/* Everything is already unmapped */
		return 0;
}

static int
wssfind(device_t parent, struct wss_softc *sc, int probing,
    struct isa_attach_args *ia)
{
	static u_char interrupt_bits[12] = {
		-1, -1, -1, -1, -1, -1, -1, 0x08, -1, 0x10, 0x18, 0x20
	};
	static u_char dma_bits[4] = {1, 2, 0, 3};
	struct ad1848_softc *ac;
	int ndrq, playdrq, recdrq;

	ac = &sc->sc_ad1848.sc_ad1848;
	sc->sc_iot = ia->ia_iot;
	if (device_cfdata(ac->sc_dev)->cf_flags & 1)
		madprobe(sc, ia->ia_io[0].ir_addr);
	else
		sc->mad_chip_type = MAD_NONE;

#if 0
	if (!WSS_BASE_VALID(ia->ia_io[0].ir_addr)) {
		DPRINTF(("wss: configured iobase %x invalid\n", ia->ia_iobase));
		goto bad1;
	}
#endif

	/* Map the ports upto the AD1848 port */
	if (bus_space_map(sc->sc_iot, ia->ia_io[0].ir_addr, WSS_CODEC,
	    0, &sc->sc_ioh))
		goto bad1;

	ac->sc_iot = sc->sc_iot;

	/* Is there an ad1848 chip at (WSS iobase + WSS_CODEC)? */
	if (ad1848_isa_mapprobe(&sc->sc_ad1848,
	    ia->ia_io[0].ir_addr + WSS_CODEC) == 0)
		goto bad;

#if 0
	/* Setup WSS interrupt and DMA */
	if (!WSS_DRQ_VALID(ia->ia_drq[0].ir_drq)) {
		DPRINTF(("wss: configured DMA chan %d invalid\n",
		    ia->ia_drq[0].ir_drq));
		goto bad;
	}
#endif
	sc->wss_playdrq = ia->ia_drq[0].ir_drq;
	sc->wss_ic      = ia->ia_ic;

	if (sc->wss_playdrq != ISA_UNKNOWN_DRQ &&
	    !isa_drq_isfree(sc->wss_ic, sc->wss_playdrq))
		goto bad;

#if 0
	if (!WSS_IRQ_VALID(ia->ia_irq[0].ir_irq)) {
		DPRINTF(("wss: configured interrupt %d invalid\n",
		    ia->ia_irq[0].ir_irq));
		goto bad;
	}
#endif

	sc->wss_irq = ia->ia_irq[0].ir_irq;

	playdrq = ia->ia_drq[0].ir_drq;
	if (ia->ia_ndrq > 1) {
		ndrq = 2;
		recdrq = ia->ia_drq[1].ir_drq;
	} else {
		ndrq = 1;
		recdrq = ISA_UNKNOWN_DRQ;
	}

	if (ac->mode <= 1)
		ndrq = 1;
	sc->wss_recdrq =
	    ac->mode > 1 && ndrq > 1 &&
	    recdrq != ISA_UNKNOWN_DRQ ? recdrq : playdrq;
	if (sc->wss_recdrq != sc->wss_playdrq && !isa_drq_isfree(sc->wss_ic,
	    sc->wss_recdrq))
		goto bad;

	if (probing) {
		ia->ia_nio = 1;
		ia->ia_io[0].ir_size = WSS_NPORT;

		ia->ia_nirq = 1;

		ia->ia_ndrq = ndrq;
		ia->ia_drq[0].ir_drq = playdrq;
		if (ndrq > 1)
			ia->ia_drq[1].ir_drq = recdrq;

		ia->ia_niomem = 0;
	}

	/* XXX recdrq */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, WSS_CONFIG,
	    (interrupt_bits[ia->ia_irq[0].ir_irq] |
		dma_bits[ia->ia_drq[0].ir_drq]));

	return 1;

bad:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, WSS_CODEC);
bad1:
	madunmap(sc);
	return 0;
}

/*
 * Attach hardware to driver, attach hardware driver to audio
 * pseudo-device driver .
 */
void
wss_isa_attach(device_t parent, device_t self, void *aux)
{
	struct wss_softc *sc;
	struct ad1848_softc *ac;
	struct isa_attach_args *ia;

	sc = device_private(self);
	ac = &sc->sc_ad1848.sc_ad1848;
	ac->sc_dev = self;
	ia = aux;
	if (!wssfind(parent, sc, 0, ia)) {
		aprint_error_dev(self, "wssfind failed\n");
		return;
	}

	sc->wss_ic = ia->ia_ic;

	wssattach(sc);
}

/*
 * Copyright by Hannu Savolainen 1994
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Initialization code for OPTi MAD16 compatible audio chips. Including
 *
 *      OPTi 82C928     MAD16           (replaced by C929)
 *      OAK OTI-601D    Mozart
 *      OPTi 82C929     MAD16 Pro
 *	OPTi 82C931
 */

static int
detect_mad16(struct wss_softc *sc, int chip_type)
{
	unsigned char tmp, tmp2;

	sc->mad_chip_type = chip_type;
	/*
	 * Check that reading a register doesn't return bus float (0xff)
	 * when the card is accessed using password. This may fail in case
	 * the card is in low power mode. Normally at least the power saving mode
	 * bit should be 0.
	 */
	if ((tmp = mad_read(sc, MC1_PORT)) == 0xff) {
		DPRINTF(("MC1_PORT returned 0xff\n"));
		return 0;
	}

	/*
	 * Now check that the gate is closed on first I/O after writing
	 * the password. (This is how a MAD16 compatible card works).
	 */
	if ((tmp2 = bus_space_read_1(sc->sc_iot, sc->mad_ioh, MC1_PORT)) == tmp) {
		DPRINTF(("MC1_PORT didn't close after read (0x%02x)\n", tmp2));
		return 0;
	}

	mad_write(sc, MC1_PORT, tmp ^ 0x80);	/* Toggle a bit */

	/* Compare the bit */
	if ((tmp2 = mad_read(sc, MC1_PORT)) != (tmp ^ 0x80)) {
		mad_write(sc, MC1_PORT, tmp);	/* Restore */
		DPRINTF(("Bit revert test failed (0x%02x, 0x%02x)\n", tmp, tmp2));
		return 0;
	}

	mad_write(sc, MC1_PORT, tmp);	/* Restore */
	return 1;
}

static void
madprobe(struct wss_softc *sc, int iobase)
{
	static int valid_ports[M_WSS_NPORTS] =
	    { M_WSS_PORT0, M_WSS_PORT1, M_WSS_PORT2, M_WSS_PORT3 };
	int i;

	/* Allocate bus space that the MAD chip wants */
	if (bus_space_map(sc->sc_iot, MAD_BASE, MAD_NPORT, 0, &sc->mad_ioh))
		goto bad0;
	if (bus_space_map(sc->sc_iot, MAD_REG1, MAD_LEN1, 0, &sc->mad_ioh1))
		goto bad1;
	if (bus_space_map(sc->sc_iot, MAD_REG2, MAD_LEN2, 0, &sc->mad_ioh2))
		goto bad2;
	if (bus_space_map(sc->sc_iot, MAD_REG3, MAD_LEN3, 0, &sc->sc_opl_ioh))
		goto bad3;

	DPRINTF(("mad: Detect using password = 0xE2\n"));
	if (!detect_mad16(sc, MAD_82C928)) {
		/* No luck. Try different model */
		DPRINTF(("mad: Detect using password = 0xE3\n"));
		if (!detect_mad16(sc, MAD_82C929))
			goto bad;
		sc->mad_chip_type = MAD_82C929;
		DPRINTF(("mad: 82C929 detected\n"));
	} else {
		sc->mad_chip_type = MAD_82C928;
		if ((mad_read(sc, MC3_PORT) & 0x03) == 0x03) {
			DPRINTF(("mad: Mozart detected\n"));
			sc->mad_chip_type = MAD_OTI601D;
		} else {
			DPRINTF(("mad: 82C928 detected?\n"));
			sc->mad_chip_type = MAD_82C928;
		}
	}

#ifdef AUDIO_DEBUG
	if (wssdebug)
		for (i = MC1_PORT; i <= MC7_PORT; i++)
			printf("mad: port %03x = %02x\n", i, mad_read(sc, i));
#endif

	/* Set the WSS address. */
	for (i = 0; i < M_WSS_NPORTS; i++)
		if (valid_ports[i] == iobase)
			break;
	if (i >= M_WSS_NPORTS) {		/* Not a valid port */
		printf("mad: Bad WSS base address 0x%x\n", iobase);
		goto bad;
	}
	sc->mad_ioindex = i;
	/* enable WSS emulation at the I/O port, no joystick */
	mad_write(sc, MC1_PORT, M_WSS_PORT_SELECT(i) | MC1_JOYDISABLE);
	mad_write(sc, MC2_PORT, 0x03); /* ? */
	mad_write(sc, MC3_PORT, 0xf0); /* Disable SB */
	return;

bad:
	bus_space_unmap(sc->sc_iot, sc->sc_opl_ioh, MAD_LEN3);
bad3:
	bus_space_unmap(sc->sc_iot, sc->mad_ioh2, MAD_LEN2);
bad2:
	bus_space_unmap(sc->sc_iot, sc->mad_ioh1, MAD_LEN1);
bad1:
	bus_space_unmap(sc->sc_iot, sc->mad_ioh, MAD_NPORT);
bad0:
	sc->mad_chip_type = MAD_NONE;
}

static void
madunmap(struct wss_softc *sc)
{

	if (sc->mad_chip_type == MAD_NONE)
		return;
	bus_space_unmap(sc->sc_iot, sc->mad_ioh, MAD_NPORT);
	bus_space_unmap(sc->sc_iot, sc->mad_ioh1, MAD_LEN1);
	bus_space_unmap(sc->sc_iot, sc->mad_ioh2, MAD_LEN2);
	bus_space_unmap(sc->sc_iot, sc->sc_opl_ioh, MAD_LEN3);
}

/*	$NetBSD: mpu_isa.c,v 1.22 2011/11/24 03:35:58 mrg Exp $	*/

/*-
 * Copyright (c) 1999, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson and by Andrew Doran.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mpu_isa.c,v 1.22 2011/11/24 03:35:58 mrg Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/midiio.h>

#include <sys/bus.h>

#include <dev/midi_if.h>

#include <dev/isa/isavar.h>
#include <dev/ic/mpuvar.h>

struct mpu_isa_softc {
	device_t sc_dev;
	struct mpu_softc sc_mpu;	/* generic part */
	void	*sc_ih;			/* ISA interrupt handler */
	kmutex_t sc_lock;
};

static int	mpu_isa_match(device_t, cfdata_t, void *);
static void	mpu_isa_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(mpu_isa, sizeof(struct mpu_isa_softc),
    mpu_isa_match, mpu_isa_attach, NULL, NULL);

static int
mpu_isa_match(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	struct mpu_isa_softc sc;
	int r;

	if (ia->ia_nio < 1)
		return 0;
	if (ia->ia_nirq < 1)
		return 0;

	if (ISA_DIRECT_CONFIG(ia))
		return 0;

	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return 0;
	if (ia->ia_irq[0].ir_irq == ISA_UNKNOWN_IRQ)
		return 0;

	memset(&sc, 0, sizeof sc);
	sc.sc_mpu.iot = ia->ia_iot;
	if (bus_space_map(sc.sc_mpu.iot, ia->ia_io[0].ir_addr, MPU401_NPORT, 0,
			  &sc.sc_mpu.ioh))
		return 0;
	r = mpu_find(&sc.sc_mpu);
        bus_space_unmap(sc.sc_mpu.iot, sc.sc_mpu.ioh, MPU401_NPORT);
	if (r) {
		ia->ia_nio = 1;
		ia->ia_io[0].ir_size = MPU401_NPORT;

		ia->ia_nirq = 1;

		ia->ia_niomem = 0;
		ia->ia_ndrq = 0;
	}
	return r;
}

static void
mpu_isa_attach(device_t parent, device_t self, void *aux)
{
	struct mpu_isa_softc *sc = device_private(self);
	struct isa_attach_args *ia = aux;

	aprint_normal("\n");

	if (bus_space_map(sc->sc_mpu.iot, ia->ia_io[0].ir_addr, MPU401_NPORT,
	    0, &sc->sc_mpu.ioh)) {
		printf("mpu_isa_attach: bus_space_map failed\n");
		return;
	}

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq,
	    IST_EDGE, IPL_AUDIO, mpu_intr, &sc->sc_mpu);

	sc->sc_mpu.model = "Roland MPU-401 MIDI UART";
	sc->sc_dev = self;
	sc->sc_mpu.lock = &sc->sc_lock;
	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_AUDIO);
	mpu_attach(&sc->sc_mpu);
}

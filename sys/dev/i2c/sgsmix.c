/*	$NetBSD: sgsmix.c,v 1.7 2010/11/13 13:51:59 uebayasi Exp $	*/

/*-
 * Copyright (C) 2005 Michael Lorenz.
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* 
 * a driver for the SGS TDA7433 mixer chip found in Beige PowerMac G3 and
 * probably others
 */


#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sgsmix.c,v 1.7 2010/11/13 13:51:59 uebayasi Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>

#include <dev/i2c/i2cvar.h>

#include <dev/i2c/sgsmixvar.h>
#include "opt_sgsmix.h"

#ifdef SGSMIX_DEBUG
#define DPRINTF printf
#else
#define DPRINTF while (0) printf
#endif

struct sgsmix_softc {
	device_t sc_dev;
	device_t sc_parent;
	i2c_tag_t sc_i2c;
	int sc_node, sc_address;
	uint8_t sc_regs[7];
};

#define SGSREG_INPUT_SELECT	0
#define SGSREG_MASTER_GAIN	1
#define SGSREG_BASS_TREBLE	2
#define SGSREG_SPEAKER_L	3
#define SGSREG_HEADPHONES_L	4
#define SGSREG_SPEAKER_R	5
#define SGSREG_HEADPHONES_R	6

static void sgsmix_attach(device_t, device_t, void *);
static int sgsmix_match(device_t, cfdata_t, void *);
static void sgsmix_setup(struct sgsmix_softc *);
static void sgsmix_writereg(struct sgsmix_softc *, int, uint8_t);

CFATTACH_DECL_NEW(sgsmix, sizeof(struct sgsmix_softc),
    sgsmix_match, sgsmix_attach, NULL, NULL);

static int
sgsmix_match(device_t parent, cfdata_t cf, void *aux)
{
	struct i2c_attach_args *args = aux;
	int ret = -1;
	uint8_t out[2] = {1, 0x20};

	/* see if we can talk to something at address 0x8a */
	if (args->ia_addr == 0x8a) {
		iic_acquire_bus(args->ia_tag, 0);
		ret = iic_exec(args->ia_tag, I2C_OP_WRITE, args->ia_addr,
		    out, 2, NULL, 0, 0);
		iic_release_bus(args->ia_tag, 0);
	}
	return (ret >= 0);
}

static void
sgsmix_attach(device_t parent, device_t self, void *aux)
{
	struct sgsmix_softc *sc = device_private(self);
	struct i2c_attach_args *args = aux;

	sc->sc_dev = self;
	sc->sc_parent = parent;
	sc->sc_address = args->ia_addr;
	aprint_normal(": SGS TDA7433 Basic Audio Processor\n");
	sc->sc_i2c = args->ia_tag;
	sgsmix_setup(sc);
}

static void
sgsmix_setup(struct sgsmix_softc *sc)
{
	int i;
	uint8_t out[2];

	sc->sc_regs[0] = 9;	/* input 1 */
	sc->sc_regs[1] = 0x20;	/* master gain 0dB */
	sc->sc_regs[2] = 0x77;	/* flat bass / treble */	
	sc->sc_regs[3] = 0;	/* all speakers full volume */
	sc->sc_regs[4] = 0;
	sc->sc_regs[5] = 0;
	sc->sc_regs[6] = 0;
	
	iic_acquire_bus(sc->sc_i2c, 0);

	for (i = 0; i < 7; i++) {
		out[0] = i;
		out[1] = sc->sc_regs[i];
		iic_exec(sc->sc_i2c, I2C_OP_WRITE, sc->sc_address, out, 2,
		    NULL, 0, 0);
	}

	iic_release_bus(sc->sc_i2c, 0);
}

static void
sgsmix_writereg(struct sgsmix_softc *sc, int reg, uint8_t val)
{
	uint8_t out[2] = {reg, val};

	if ((reg < 0) || (reg >= 7))
		return;

	if (sc->sc_regs[reg] == val)
		return;

	if (iic_exec(sc->sc_i2c, I2C_OP_WRITE, sc->sc_address, out, 2, NULL,
	    0, 0) == 0) {
	    	sc->sc_regs[reg] = val;
	}
}

void
sgsmix_set_speaker_vol(void *cookie, int left, int right)
{
	struct sgsmix_softc *sc = device_private((device_t)cookie);

	DPRINTF("%s: speaker %d %d\n", device_xname(sc->sc_dev), left, right);
	if (left == 0) {
		sgsmix_writereg(sc, SGSREG_SPEAKER_L, 0x20);
	} else {
		sgsmix_writereg(sc, SGSREG_SPEAKER_L,
		    ((255 - left) >> 3) & 0x1f);
	}

	if (right == 0) {
		sgsmix_writereg(sc, SGSREG_SPEAKER_R, 0x20);
	} else {
		sgsmix_writereg(sc, SGSREG_SPEAKER_R,
		    ((255 - right) >> 3) & 0x1f);
	}
}

void
sgsmix_set_headphone_vol(void *cookie, int left, int right)
{
	struct sgsmix_softc *sc = device_private((device_t)cookie);

	DPRINTF("%s: headphones %d %d\n", device_xname(sc->sc_dev), left, right);
	if (left == 0) {
		sgsmix_writereg(sc, SGSREG_HEADPHONES_L, 0x20);
	} else {
		sgsmix_writereg(sc, SGSREG_HEADPHONES_L,
		    ((255 - left) >> 3) & 0x1f);
	}

	if (right == 0) {
		sgsmix_writereg(sc, SGSREG_HEADPHONES_R, 0x20);
	} else {
		sgsmix_writereg(sc, SGSREG_HEADPHONES_R,
		    ((255 - right) >> 3) & 0x1f);
	}
}

void
sgsmix_set_bass_treble(void *cookie, int bass, int treble)
{
	struct sgsmix_softc *sc = device_private((device_t)cookie);
	uint8_t b, t;

	t = (treble >> 4) & 0xf;
	if (t & 0x8)
		t ^= 7;
	b = bass & 0xf0;
	if (b & 0x80)
		b ^= 0x70;
	DPRINTF("%s: bass/treble %02x %02x\n", device_xname(sc->sc_dev), b, t);
	sgsmix_writereg(sc, SGSREG_BASS_TREBLE, b | t);
}

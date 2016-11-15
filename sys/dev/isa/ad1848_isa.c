/*	$NetBSD: ad1848_isa.c,v 1.38 2011/11/23 23:07:32 jmcneill Exp $	*/

/*-
 * Copyright (c) 1999, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ken Hornstein and John Kohl.
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
/*
 * Copyright (c) 1994 John Brezak
 * Copyright (c) 1991-1993 Regents of the University of California.
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
 * Portions of this code are from the VOXware support for the ad1848
 * by Hannu Savolainen <hannu@voxware.pp.fi>
 *
 * Portions also supplied from the SoundBlaster driver for NetBSD.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ad1848_isa.c,v 1.38 2011/11/23 23:07:32 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/cpu.h>
#include <sys/bus.h>
#include <sys/audioio.h>
#include <sys/malloc.h>

#include <dev/audio_if.h>
#include <dev/auconv.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/ad1848reg.h>
#include <dev/ic/cs4231reg.h>
#include <dev/ic/cs4237reg.h>
#include <dev/isa/ad1848var.h>
#include <dev/isa/cs4231var.h>

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (ad1848debug) printf x
extern int	ad1848debug;
#else
#define DPRINTF(x)
#endif

static int ad1848_isa_read( struct ad1848_softc *, int);
static void ad1848_isa_write( struct ad1848_softc *, int, int);

int
ad1848_isa_read(struct ad1848_softc *sc, int index)
{

	return bus_space_read_1(sc->sc_iot, sc->sc_ioh, index);
}

void
ad1848_isa_write(struct ad1848_softc *sc, int index, int value)
{

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, index, value);
}

/*
 * Map and probe for the ad1848 chip
 */
int
ad1848_isa_mapprobe(struct ad1848_isa_softc *isc, int iobase)
{
	struct ad1848_softc *sc;

	sc = &isc->sc_ad1848;
	if (!AD1848_BASE_VALID(iobase)) {
#ifdef AUDIO_DEBUG
		printf("ad1848: configured iobase %04x invalid\n", iobase);
#endif
		return 0;
	}

	/* Map the AD1848 ports */
	if (bus_space_map(sc->sc_iot, iobase, AD1848_NPORT, 0, &sc->sc_ioh))
		return 0;

	if (!ad1848_isa_probe(isc)) {
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, AD1848_NPORT);
		return 0;
	} else
		return 1;
}

/*
 * Probe for the ad1848 chip
 */
int
ad1848_isa_probe(struct ad1848_isa_softc *isc)
{
	struct ad1848_softc *sc;
	u_char tmp, tmp1 = 0xff, tmp2 = 0xff;
	int i, t;

	sc = &isc->sc_ad1848;
	sc->sc_readreg = ad1848_isa_read;
	sc->sc_writereg = ad1848_isa_write;

	/* Is there an ad1848 chip ? */
	sc->MCE_bit = MODE_CHANGE_ENABLE;
	sc->mode = 1;	/* MODE 1 = original ad1848/ad1846/cs4248 */

	/*
	 * Check that the I/O address is in use.
	 *
	 * The SP_IN_INIT bit of the base I/O port is known to be 0 after the
	 * chip has performed its power-on initialization. Just assume
	 * this has happened before the OS is starting.
	 *
	 * If the I/O address is unused, inb() typically returns 0xff.
	 */
	tmp = ADREAD(sc, AD1848_IADDR);
	if (tmp & SP_IN_INIT) { /* Not a AD1848 */
		DPRINTF(("ad_detect_A %x\n", tmp));
		goto bad;
	}

	/*
	 * Test if it's possible to change contents of the indirect registers.
	 * Registers 0 and 1 are ADC volume registers.  The bit 0x10 is read
	 * only so try to avoid using it.  The bit 0x20 is the mic preamp
	 * enable; on some chips it is always the same in both registers, so
	 * we avoid tests where they are different.
	 */
	ad_write(sc, 0, 0x8a);
	ad_write(sc, 1, 0x45);	/* 0x55 with bit 0x10 clear */
	tmp1 = ad_read(sc, 0);
	tmp2 = ad_read(sc, 1);

	if (tmp1 != 0x8a || tmp2 != 0x45) {
		DPRINTF(("ad_detect_B (%x/%x)\n", tmp1, tmp2));
		goto bad;
	}

	ad_write(sc, 0, 0x65);
	ad_write(sc, 1, 0xaa);
	tmp1 = ad_read(sc, 0);
	tmp2 = ad_read(sc, 1);

	if (tmp1 != 0x65 || tmp2 != 0xaa) {
		DPRINTF(("ad_detect_C (%x/%x)\n", tmp1, tmp2));
		goto bad;
	}

	/*
	 * The indirect register I12 has some read only bits. Lets
	 * try to change them.
	 */
	tmp = ad_read(sc, SP_MISC_INFO);
	ad_write(sc, SP_MISC_INFO, (~tmp) & 0x0f);

	/* Here, AD1845 may sometimes be busy.  Wait til it becomes ready. */
	for (t = 0; t < 100000 && ADREAD(sc, AD1848_IADDR) & SP_IN_INIT; t++)
		;
#ifdef AUDIO_DEBUG
	if (t)
		DPRINTF(("ad1848_isa_probe: t %d\n", t));
#endif

	if ((tmp & 0x0f) != ((tmp1 = ad_read(sc, SP_MISC_INFO)) & 0x0f)) {
		DPRINTF(("ad_detect_D (%x)\n", tmp1));
		goto bad;
	}

	/*
	 * MSB and 4 LSBs of the reg I12 tell the chip revision.
	 *
	 * A preliminary version of the AD1846 data sheet stated that it
	 * used an ID field of 0x0B.  The current version, however,
	 * states that the AD1846 uses ID 0x0A, just like the AD1848K.
	 *
	 * this switch statement will need updating as newer clones arrive....
	 */
	switch (tmp1 & 0x8f) {
	case 0x09:
		sc->chip_name = "AD1848J";
		break;
	case 0x0A:
		sc->chip_name = "AD1848K";
		break;
#if 0	/* See above */
	case 0x0B:
		sc->chip_name = "AD1846";
		break;
#endif
	case 0x81:
		sc->chip_name = "CS4248revB"; /* or CS4231 rev B; see below */
		break;
	case 0x89:
		sc->chip_name = "CS4248";
		break;
	case 0x8A:
		sc->chip_name = "broken"; /* CS4231/AD1845; see below */
		break;
	default:
		sc->chip_name = "unknown";
		DPRINTF(("ad1848: unknown codec version 0x%02x\n",
			 tmp1 & 0x8f));
		break;
	}

	/*
	 * The original AD1848/CS4248 has just 16 indirect registers. This
	 * means that I0 and I16 should return the same value (etc.).
	 * Ensure that the Mode2 enable bit of I12 is 0. Otherwise this test
	 * fails with CS4231, AD1845, etc.
	 */
	ad_write(sc, SP_MISC_INFO, 0);	/* Mode2 = disabled */

	for (i = 0; i < 16; i++)
		if ((tmp1 = ad_read(sc, i)) != (tmp2 = ad_read(sc, i + 16))) {
			if (i != SP_TEST_AND_INIT) {
				DPRINTF(("ad_detect_F(%d/%x/%x)\n", i, tmp1, tmp2));
				goto bad;
			}
		}

	/*
	 * Try to switch the chip to mode2 (CS4231) by setting the MODE2 bit
	 * The bit 0x80 is always 1 in CS4248, CS4231, and AD1845.
	 */
	ad_write(sc, SP_MISC_INFO, MODE2);	/* Set mode2, clear 0x80 */

	tmp1 = ad_read(sc, SP_MISC_INFO);
	if ((tmp1 & 0xc0) == (0x80 | MODE2)) {
		/*
		 *      CS4231 or AD1845 detected - is it?
		 *
		 *	Verify that setting I2 doesn't change I18.
		 */
		ad_write(sc, 18, 0x88); /* Set I18 to known value */

		ad_write(sc, 2, 0x45);
		if ((tmp2 = ad_read(sc, 18)) != 0x45) { /* No change -> CS4231? */
			ad_write(sc, 2, 0xaa);
			if ((tmp2 = ad_read(sc, 18)) == 0xaa) {     /* Rotten bits? */
				DPRINTF(("ad_detect_H(%x)\n", tmp2));
				goto bad;
			}

			sc->mode = 2;

			/*
			 *  It's a CS4231, or another clone with 32 registers.
			 *  Let's find out which by checking I25.
			 */
			if ((tmp1 & 0x8f) == 0x8a) {
				tmp1 = ad_read(sc, CS_VERSION_ID);
				switch (tmp1 & 0xe7) {
				case 0xA0:
					sc->chip_name = "CS4231A";
					break;
				case 0x80:
					/*  XXX I25 no good, AD1845 same as CS4231 */
					/*
					 * XXX
					 * This test is correct only after reset
					 */
					if (ad_read(sc, 17) & 0xf0) {
						sc->chip_name = "AD1845";
						sc->is_ad1845 = 1;
					} else
						sc->chip_name = "CS4231";
					break;
				case 0x82:
					sc->chip_name = "CS4232";
					break;
				case 0xa2:
					sc->chip_name = "CS4232C";
					break;
				case 0x03:
				case 0x83:
					sc->chip_name = "CS4236";

					/*
					 * Try to switch to mode3 (CS4236B or
					 * CS4237B) by setting CMS to 3.  A
					 * plain CS4236 will not react to
					 * LLBM settings.
					 */
					ad_write(sc, SP_MISC_INFO, MODE3);

					tmp1 = ad_read(sc, CS_LEFT_LINE_CONTROL);
					ad_write(sc, CS_LEFT_LINE_CONTROL, 0xe0);
					tmp2 = ad_read(sc, CS_LEFT_LINE_CONTROL);
					if (tmp2 == 0xe0) {
						/*
						 * it's a CS4237B or another
						 * clone supporting mode 3.
						 * Let's determine which by
						 * enabling extended registers
						 * and checking X25.
						 */
						tmp2 = ad_xread(sc, CS_X_CHIP_VERSION);
						switch (tmp2 & X_CHIP_VERSIONF_CID) {
						case X_CHIP_CID_CS4236BB:
							sc->chip_name = "CS4236BrevB";
							break;
						case X_CHIP_CID_CS4236B:
							sc->chip_name = "CS4236B";
							break;
						case X_CHIP_CID_CS4237B:
							sc->chip_name = "CS4237B";
							break;
						default:
							sc->chip_name = "CS4236B compatible";
							DPRINTF(("cs4236: unknown mode 3 compatible codec, version 0x%02x\n", tmp2));
							break;
						}
						sc->mode = 3;
					}

					/* restore volume control information */
					ad_write(sc, CS_LEFT_LINE_CONTROL, tmp1);
					break;
				}
			}
		}
	}

	/* Wait for 1848 to init */
	while (ADREAD(sc, AD1848_IADDR) & SP_IN_INIT)
		;

	/* Wait for 1848 to autocal */
	ADWRITE(sc, AD1848_IADDR, SP_TEST_AND_INIT);
	while (ADREAD(sc, AD1848_IDATA) & AUTO_CAL_IN_PROG)
		;

	return 1;
bad:
	return 0;
}

/* Unmap the I/O ports */
void
ad1848_isa_unmap(struct ad1848_isa_softc *isc)
{
	struct ad1848_softc *sc;

	sc = &isc->sc_ad1848;
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, AD1848_NPORT);
}

/*
 * Attach hardware to driver, attach hardware driver to audio
 * pseudo-device driver .
 */
void
ad1848_isa_attach(struct ad1848_isa_softc *isc)
{
	struct ad1848_softc *sc;
	int error;

	sc = &isc->sc_ad1848;
	sc->sc_readreg = ad1848_isa_read;
	sc->sc_writereg = ad1848_isa_write;

	if (isc->sc_playdrq != -1) {
		isc->sc_play_maxsize = isa_dmamaxsize(isc->sc_ic,
		    isc->sc_playdrq);
		error = isa_dmamap_create(isc->sc_ic, isc->sc_playdrq,
		    isc->sc_play_maxsize, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW);
		if (error) {
			aprint_error_dev(sc->sc_dev, "can't create map for drq %d\n",
			    isc->sc_playdrq);
			return;
		}
	}
	if (isc->sc_recdrq != -1 && isc->sc_recdrq != isc->sc_playdrq) {
		isc->sc_rec_maxsize = isa_dmamaxsize(isc->sc_ic,
		    isc->sc_recdrq);
		error = isa_dmamap_create(isc->sc_ic, isc->sc_recdrq,
		    isc->sc_rec_maxsize, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW);
		if (error) {
			aprint_error_dev(sc->sc_dev, "can't create map for drq %d\n",
			    isc->sc_recdrq);
			isa_dmamap_destroy(isc->sc_ic, isc->sc_playdrq);
			return;
		}
	}

	ad1848_attach(sc);
}

int
ad1848_isa_open(void *addr, int flags)
{
	struct ad1848_isa_softc *isc;
	struct ad1848_softc *sc;
	int error, state;

	isc = addr;
	sc = &isc->sc_ad1848;
	DPRINTF(("ad1848_isa_open: sc=%p\n", isc));
	state = 0;

	if (isc->sc_playdrq != -1) {
		error = isa_drq_alloc(isc->sc_ic, isc->sc_playdrq);
		if (error != 0)
			return EBUSY;
		state |= 1;
	}
	if (isc->sc_recdrq != -1 && isc->sc_recdrq != isc->sc_playdrq) {
		error = isa_drq_alloc(isc->sc_ic, isc->sc_recdrq);
		if (error != 0)
			goto bad;
		state |= 2;
	}

#ifndef AUDIO_NO_POWER_CTL
	/* Power-up chip */
	if (isc->powerctl) {
		KASSERT(mutex_owned(&sc->sc_intr_lock));
		isc->powerctl(isc->powerarg, flags);
	}
#endif

	/* Init and mute wave output */
	ad1848_mute_wave_output(sc, WAVE_MUTE2_INIT, 1);

	error = ad1848_open(sc, flags);
	if (error) {
#ifndef AUDIO_NO_POWER_CTL
		if (isc->powerctl) {
			KASSERT(mutex_owned(&sc->sc_intr_lock));
			isc->powerctl(isc->powerarg, 0);
		}
#endif
		goto bad;
	}

	DPRINTF(("ad1848_isa_open: opened\n"));
	return 0;

bad:
	if (state & 1)
		isa_drq_free(isc->sc_ic, isc->sc_playdrq);
	if (state & 2)
		isa_drq_free(isc->sc_ic, isc->sc_recdrq);

	return error;
}

void
ad1848_isa_close(void *addr)
{
	struct ad1848_isa_softc *isc;
	struct ad1848_softc *sc;

	DPRINTF(("ad1848_isa_close: stop DMA\n"));
	isc = addr;
	sc = &isc->sc_ad1848;
	ad1848_close(sc);

#ifndef AUDIO_NO_POWER_CTL
	/* Power-down chip */
	if (isc->powerctl) {
		KASSERT(mutex_owned(&sc->sc_intr_lock));
		isc->powerctl(isc->powerarg, 0);
	}
#endif

	if (isc->sc_playdrq != -1)
		isa_drq_free(isc->sc_ic, isc->sc_playdrq);
	if (isc->sc_recdrq != -1 && isc->sc_recdrq != isc->sc_playdrq)
		isa_drq_free(isc->sc_ic, isc->sc_recdrq);
}

int
ad1848_isa_trigger_input(
	void *addr,
	void *start, void *end,
	int blksize,
	void (*intr)(void *),
	void *arg,
	const audio_params_t *param)
{
	struct ad1848_isa_softc *isc;
	struct ad1848_softc *sc;
	uint8_t reg;

	isc = addr;
	sc = &isc->sc_ad1848;
	isa_dmastart(isc->sc_ic, isc->sc_recdrq, start,
	    (char *)end - (char *)start, NULL,
	    DMAMODE_READ | DMAMODE_LOOPDEMAND, BUS_DMA_NOWAIT);

	isc->sc_recrun = 1;
	if (sc->mode == 2 && isc->sc_playdrq != isc->sc_recdrq) {
		isc->sc_rintr = intr;
		isc->sc_rarg = arg;
	} else {
		isc->sc_pintr = intr;
		isc->sc_parg = arg;
	}

	/*
	 * Calculate number of transfers.
	 * Note that ADPCM is always transferred 4 bytes at at a time.
	 */
	blksize = (param->encoding == AUDIO_ENCODING_ADPCM) ? blksize / 4 - 1 :
	    (blksize * 8) / (param->precision * param->channels) - 1;

	if (sc->mode >= 2) {
		ad_write(sc, CS_LOWER_REC_CNT, blksize & 0xff);
		ad_write(sc, CS_UPPER_REC_CNT, blksize >> 8);
	} else {
		ad_write(sc, SP_LOWER_BASE_COUNT, blksize & 0xff);
		ad_write(sc, SP_UPPER_BASE_COUNT, blksize >> 8);
	}

	reg = ad_read(sc, SP_INTERFACE_CONFIG);
	ad_write(sc, SP_INTERFACE_CONFIG, CAPTURE_ENABLE|reg);

	return 0;
}

int
ad1848_isa_trigger_output(
	void *addr,
	void *start, void *end,
	int blksize,
	void (*intr)(void *),
	void *arg,
	const audio_params_t *param)
{
	struct ad1848_isa_softc *isc;
	struct ad1848_softc *sc;
	uint8_t reg;

	isc = addr;
	sc = &isc->sc_ad1848;
	isa_dmastart(isc->sc_ic, isc->sc_playdrq, start,
	    (char *)end - (char *)start, NULL,
	    DMAMODE_WRITE | DMAMODE_LOOPDEMAND, BUS_DMA_NOWAIT);

	isc->sc_playrun = 1;
	isc->sc_pintr = intr;
	isc->sc_parg = arg;

	/*
	 * Calculate number of transfers.
	 * Note that ADPCM is always transferred 4 bytes at at a time.
	 */
	blksize = (param->encoding == AUDIO_ENCODING_ADPCM) ? blksize / 4 - 1 :
	    (blksize * 8) / (param->precision * param->channels) - 1;

	ad_write(sc, SP_LOWER_BASE_COUNT, blksize & 0xff);
	ad_write(sc, SP_UPPER_BASE_COUNT, blksize >> 8);

	/* Unmute wave output */
	ad1848_mute_wave_output(sc, WAVE_MUTE2, 0);

	reg = ad_read(sc, SP_INTERFACE_CONFIG);
	ad_write(sc, SP_INTERFACE_CONFIG, PLAYBACK_ENABLE|reg);

	return 0;
}

int
ad1848_isa_halt_input(void *addr)
{
	struct ad1848_isa_softc *isc;
	struct ad1848_softc *sc;

	isc = addr;
	sc = &isc->sc_ad1848;
	if (isc->sc_recrun) {
		ad1848_halt_input(sc);
		isa_dmaabort(isc->sc_ic, isc->sc_recdrq);
		isc->sc_recrun = 0;
	}

	return 0;
}

int
ad1848_isa_halt_output(void *addr)
{
	struct ad1848_isa_softc *isc;
	struct ad1848_softc *sc;

	isc = addr;
	sc = &isc->sc_ad1848;
	if (isc->sc_playrun) {
		/* Mute wave output */
		ad1848_mute_wave_output(sc, WAVE_MUTE2, 1);

		ad1848_halt_output(sc);
		isa_dmaabort(isc->sc_ic, isc->sc_playdrq);
		isc->sc_playrun = 0;
	}

	return 0;
}

int
ad1848_isa_intr(void *arg)
{
	struct ad1848_isa_softc *isc;
	struct ad1848_softc *sc;
	int retval;
	u_char status;

	isc = arg;
	sc = &isc->sc_ad1848;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

	retval = 0;
	/* Get intr status */
	status = ADREAD(sc, AD1848_STATUS);

#ifdef AUDIO_DEBUG
	if (ad1848debug > 1)
		printf("ad1848_isa_intr: pintr=%p rintr=%p status=%x\n",
		    isc->sc_pintr, isc->sc_rintr, status);
#endif
	isc->sc_interrupts++;

	/* Handle interrupt */
	if ((status & INTERRUPT_STATUS) != 0) {
		if (sc->mode == 2 && isc->sc_playdrq != isc->sc_recdrq) {
			status = ad_read(sc, CS_IRQ_STATUS);
			if ((status & CS_IRQ_PI) && isc->sc_playrun) {
				(*isc->sc_pintr)(isc->sc_parg);
				retval = 1;
			}
			if ((status & CS_IRQ_CI) && isc->sc_recrun) {
				(*isc->sc_rintr)(isc->sc_rarg);
				retval = 1;
			}
		} else {
			if (isc->sc_playrun) {
				(*isc->sc_pintr)(isc->sc_parg);
				retval = 1;
			}
		}

		/* Clear interrupt */
		ADWRITE(sc, AD1848_STATUS, 0);
	}
	return retval;
}

void *
ad1848_isa_malloc(
	void *addr,
	int direction,
	size_t size)
{
	struct ad1848_isa_softc *isc;
	int drq;

	isc = addr;
	if (direction == AUMODE_PLAY)
		drq = isc->sc_playdrq;
	else
		drq = isc->sc_recdrq;
	return isa_malloc(isc->sc_ic, drq, size, M_DEVBUF, M_WAITOK);
}

void
ad1848_isa_free(void *addr, void *ptr, size_t size)
{

	isa_free(ptr, M_DEVBUF);
}

size_t
ad1848_isa_round_buffersize(void *addr, int direction, size_t size)
{
	struct ad1848_isa_softc *isc;
	bus_size_t maxsize;

	isc = addr;
	if (direction == AUMODE_PLAY)
		maxsize = isc->sc_play_maxsize;
	else if (isc->sc_recdrq == isc->sc_playdrq)
		maxsize = isc->sc_play_maxsize;
	else
		maxsize = isc->sc_rec_maxsize;

	if (size > maxsize)
		size = maxsize;
	return size;
}

paddr_t
ad1848_isa_mappage(void *addr, void *mem, off_t off, int prot)
{
	return isa_mappage(mem, off, prot);
}

int
ad1848_isa_get_props(void *addr)
{
	struct ad1848_isa_softc *isc;

	isc = addr;
	return AUDIO_PROP_MMAP |
		(isc->sc_playdrq != isc->sc_recdrq ? AUDIO_PROP_FULLDUPLEX : 0);
}

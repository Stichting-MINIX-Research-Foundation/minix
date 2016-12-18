/*	$NetBSD: ad1848.c,v 1.31 2011/11/23 23:07:32 jmcneill Exp $	*/

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
 * Portions also ripped from the SoundBlaster driver for NetBSD.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ad1848.c,v 1.31 2011/11/23 23:07:32 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/fcntl.h>
/*#include <sys/syslog.h>*/
/*#include <sys/proc.h>*/

#include <sys/cpu.h>
#include <sys/bus.h>

#include <sys/audioio.h>

#include <dev/audio_if.h>
#include <dev/auconv.h>

#include <dev/ic/ad1848reg.h>
#include <dev/ic/cs4231reg.h>
#include <dev/ic/cs4237reg.h>
#include <dev/ic/ad1848var.h>
#if 0
#include <dev/isa/cs4231var.h>
#endif

/*
 * AD1845 on some machines don't match the AD1845 doc
 * and defining AD1845_HACK to 1 works around the problems.
 * options AD1845_HACK=0  should work if you have ``correct'' one.
 */
#ifndef AD1845_HACK
#define AD1845_HACK	1	/* weird mixer, can't play slinear_be */
#endif

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (ad1848debug) printf x
int	ad1848debug = 0;
void ad1848_dump_regs(struct ad1848_softc *);
#else
#define DPRINTF(x)
#endif

/*
 * Initial values for the indirect registers of CS4248/AD1848.
 */
static const int ad1848_init_values[] = {
    GAIN_12|INPUT_MIC_GAIN_ENABLE,	/* Left Input Control */
    GAIN_12|INPUT_MIC_GAIN_ENABLE,	/* Right Input Control */
    ATTEN_12,				/* Left Aux #1 Input Control */
    ATTEN_12,				/* Right Aux #1 Input Control */
    ATTEN_12,				/* Left Aux #2 Input Control */
    ATTEN_12,				/* Right Aux #2 Input Control */
    /* bits 5-0 are attenuation select */
    ATTEN_12,				/* Left DAC output Control */
    ATTEN_12,				/* Right DAC output Control */
    CLOCK_XTAL1|FMT_PCM8,		/* Clock and Data Format */
    SINGLE_DMA|AUTO_CAL_ENABLE,		/* Interface Config */
    INTERRUPT_ENABLE,			/* Pin control */
    0x00,				/* Test and Init */
    MODE2,				/* Misc control */
    ATTEN_0<<2,				/* Digital Mix Control */
    0,					/* Upper base Count */
    0,					/* Lower base Count */

    /* These are for CS4231 &c. only (additional registers): */
    0,					/* Alt feature 1 */
    0,					/* Alt feature 2 */
    ATTEN_12,				/* Left line in */
    ATTEN_12,				/* Right line in */
    0,					/* Timer low */
    0,					/* Timer high */
    0,					/* unused */
    0,					/* unused */
    0,					/* IRQ status */
    0,					/* unused */
			/* Mono input (a.k.a speaker) (mic) Control */
    MONO_INPUT_MUTE|ATTEN_6,		/* mute speaker by default */
    0,					/* unused */
    0,					/* record format */
    0,					/* Crystal Clock Select */
    0,					/* upper record count */
    0					/* lower record count */
};


int
ad1848_to_vol(mixer_ctrl_t *cp, struct ad1848_volume *vol)
{

	if (cp->un.value.num_channels == 1) {
		vol->left =
		vol->right = cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		return 1;
	}
	else if (cp->un.value.num_channels == 2) {
		vol->left  = cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
		vol->right = cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
		return 1;
	}
	return 0;
}

int
ad1848_from_vol(mixer_ctrl_t *cp, struct ad1848_volume *vol)
{

	if (cp->un.value.num_channels == 1) {
		cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] = vol->left;
		return 1;
	}
	else if (cp->un.value.num_channels == 2) {
		cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] = vol->left;
		cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = vol->right;
		return 1;
	}
	return 0;
}


int
ad_read(struct ad1848_softc *sc, int reg)
{
	int x;

	ADWRITE(sc, AD1848_IADDR, (reg & 0xff) | sc->MCE_bit);
	x = ADREAD(sc, AD1848_IDATA);
	/*  printf("(%02x<-%02x) ", reg|sc->MCE_bit, x); */
	return x;
}

void
ad_write(struct ad1848_softc *sc, int reg, int data)
{

	ADWRITE(sc, AD1848_IADDR, (reg & 0xff) | sc->MCE_bit);
	ADWRITE(sc, AD1848_IDATA, data & 0xff);
	/* printf("(%02x->%02x) ", reg|sc->MCE_bit, data); */
}

/*
 * extended registers (mode 3) require an additional level of
 * indirection through CS_XREG (I23).
 */

int
ad_xread(struct ad1848_softc *sc, int reg)
{
	int x;

	ADWRITE(sc, AD1848_IADDR, CS_XREG | sc->MCE_bit);
	ADWRITE(sc, AD1848_IDATA, (reg | ALT_F3_XRAE) & 0xff);
	x = ADREAD(sc, AD1848_IDATA);

	return x;
}

void
ad_xwrite(struct ad1848_softc *sc, int reg, int val)
{

	ADWRITE(sc, AD1848_IADDR, CS_XREG | sc->MCE_bit);
	ADWRITE(sc, AD1848_IDATA, (reg | ALT_F3_XRAE) & 0xff);
	ADWRITE(sc, AD1848_IDATA, val & 0xff);
}

static void
ad_set_MCE(struct ad1848_softc *sc, int state)
{

	if (state)
		sc->MCE_bit = MODE_CHANGE_ENABLE;
	else
		sc->MCE_bit = 0;
	ADWRITE(sc, AD1848_IADDR, sc->MCE_bit);
}

static void
wait_for_calibration(struct ad1848_softc *sc)
{
	int timeout;

	DPRINTF(("ad1848: Auto calibration started.\n"));
	/*
	 * Wait until the auto calibration process has finished.
	 *
	 * 1) Wait until the chip becomes ready (reads don't return 0x80).
	 * 2) Wait until the ACI bit of I11 gets on and then off.
	 *    Because newer chips are fast we may never see the ACI
	 *    bit go on.  Just delay a little instead.
	 */
	timeout = 10000;
	while (timeout > 0 && ADREAD(sc, AD1848_IADDR) == SP_IN_INIT) {
		delay(10);
		timeout--;
	}
	if (timeout <= 0) {
		DPRINTF(("ad1848: Auto calibration timed out(1).\n"));
	}

	/* Set register addr */
	ADWRITE(sc, AD1848_IADDR, SP_TEST_AND_INIT);
	/* Wait for address to appear when read back. */
	timeout = 100000;
	while (timeout > 0 &&
	       (ADREAD(sc, AD1848_IADDR)&SP_IADDR_MASK) != SP_TEST_AND_INIT) {
		delay(10);
		timeout--;
	}
	if (timeout <= 0) {
		DPRINTF(("ad1848: Auto calibration timed out(1.5).\n"));
	}

	if (!(ad_read(sc, SP_TEST_AND_INIT) & AUTO_CAL_IN_PROG)) {
		if (sc->mode > 1) {
			/* A new chip, just delay a little. */
			delay(100);	/* XXX what should it be? */
		} else {
			timeout = 10000;
			while (timeout > 0 &&
			       !(ad_read(sc, SP_TEST_AND_INIT) &
				 AUTO_CAL_IN_PROG)) {
				delay(10);
				timeout--;
			}
			if (timeout <= 0) {
				DPRINTF(("ad1848: Auto calibration timed out(2).\n"));
			}
		}
	}

	timeout = 10000;
	while (timeout > 0 &&
	       ad_read(sc, SP_TEST_AND_INIT) & AUTO_CAL_IN_PROG) {
		delay(10);
		timeout--;
	}
	if (timeout <= 0) {
		DPRINTF(("ad1848: Auto calibration timed out(3).\n"));
	}
}

#ifdef AUDIO_DEBUG
void
ad1848_dump_regs(struct ad1848_softc *sc)
{
	int i;
	u_char r;

	printf("ad1848 status=%02x", ADREAD(sc, AD1848_STATUS));
	printf(" regs: ");
	for (i = 0; i < 16; i++) {
		r = ad_read(sc, i);
		printf("%02x ", r);
	}
	if (sc->mode >= 2) {
		for (i = 16; i < 32; i++) {
			r = ad_read(sc, i);
			printf("%02x ", r);
		}
	}
	printf("\n");
}
#endif /* AUDIO_DEBUG */


/*
 * Attach hardware to driver, attach hardware driver to audio
 * pseudo-device driver .
 */
void
ad1848_attach(struct ad1848_softc *sc)
{
	static struct ad1848_volume vol_mid = {220, 220};
	static struct ad1848_volume vol_0   = {0, 0};
	int i;
	int timeout;

	/* Initialize the ad1848... */
	for (i = 0; i < 0x10; i++) {
		ad_write(sc, i, ad1848_init_values[i]);
		timeout = 100000;
		while (timeout > 0 && ADREAD(sc, AD1848_IADDR) & SP_IN_INIT)
			timeout--;
	}
	/* ...and additional CS4231 stuff too */
	if (sc->mode >= 2) {
		ad_write(sc, SP_INTERFACE_CONFIG, 0); /* disable SINGLE_DMA */
		for (i = 0x10; i < 0x20; i++)
			if (ad1848_init_values[i] != 0) {
				ad_write(sc, i, ad1848_init_values[i]);
				timeout = 100000;
				while (timeout > 0 &&
				       ADREAD(sc, AD1848_IADDR) & SP_IN_INIT)
					timeout--;
			}
	}
	ad1848_reset(sc);

	/* Set default gains */
	ad1848_set_rec_gain(sc, &vol_mid);
	ad1848_set_channel_gain(sc, AD1848_DAC_CHANNEL, &vol_mid);
	ad1848_set_channel_gain(sc, AD1848_MONITOR_CHANNEL, &vol_0);
	ad1848_set_channel_gain(sc, AD1848_AUX1_CHANNEL, &vol_mid);	/* CD volume */
	sc->mute[AD1848_MONITOR_CHANNEL] = MUTE_ALL;
	if (sc->mode >= 2
#if AD1845_HACK
	    && sc->is_ad1845 == 0
#endif
		) {
		ad1848_set_channel_gain(sc, AD1848_AUX2_CHANNEL, &vol_mid); /* CD volume */
		ad1848_set_channel_gain(sc, AD1848_LINE_CHANNEL, &vol_mid);
		ad1848_set_channel_gain(sc, AD1848_MONO_CHANNEL, &vol_0);
		sc->mute[AD1848_MONO_CHANNEL] = MUTE_ALL;
	} else
		ad1848_set_channel_gain(sc, AD1848_AUX2_CHANNEL, &vol_0);

	/* Set default port */
	ad1848_set_rec_port(sc, MIC_IN_PORT);

	printf(": %s", sc->chip_name);
}

/*
 * Various routines to interface to higher level audio driver
 */
static const struct ad1848_mixerinfo {
	int  left_reg;
	int  right_reg;
	int  atten_bits;
	int  atten_mask;
} mixer_channel_info[] =
{
  { SP_LEFT_AUX2_CONTROL, SP_RIGHT_AUX2_CONTROL, AUX_INPUT_ATTEN_BITS,
    AUX_INPUT_ATTEN_MASK },
  { SP_LEFT_AUX1_CONTROL, SP_RIGHT_AUX1_CONTROL, AUX_INPUT_ATTEN_BITS,
    AUX_INPUT_ATTEN_MASK },
  { SP_LEFT_OUTPUT_CONTROL, SP_RIGHT_OUTPUT_CONTROL,
    OUTPUT_ATTEN_BITS, OUTPUT_ATTEN_MASK },
  { CS_LEFT_LINE_CONTROL, CS_RIGHT_LINE_CONTROL, LINE_INPUT_ATTEN_BITS,
    LINE_INPUT_ATTEN_MASK },
  { CS_MONO_IO_CONTROL, 0, MONO_INPUT_ATTEN_BITS, MONO_INPUT_ATTEN_MASK },
  { CS_MONO_IO_CONTROL, 0, 0, 0 },
  { SP_DIGITAL_MIX, 0, OUTPUT_ATTEN_BITS, MIX_ATTEN_MASK }
};

/*
 *  This function doesn't set the mute flags but does use them.
 *  The mute flags reflect the mutes that have been applied by the user.
 *  However, the driver occasionally wants to mute devices (e.g. when chaing
 *  sampling rate). These operations should not affect the mute flags.
 */

void
ad1848_mute_channel(struct ad1848_softc *sc, int device, int mute)
{
	u_char reg;

	reg = ad_read(sc, mixer_channel_info[device].left_reg);

	if (mute & MUTE_LEFT) {
		if (device == AD1848_MONITOR_CHANNEL) {
			if (sc->open_mode & FREAD)
				ad1848_mute_wave_output(sc, WAVE_UNMUTE1, 0);
			ad_write(sc, mixer_channel_info[device].left_reg,
				 reg & ~DIGITAL_MIX1_ENABLE);
		} else if (device == AD1848_OUT_CHANNEL)
			ad_write(sc, mixer_channel_info[device].left_reg,
				 reg | MONO_OUTPUT_MUTE);
		else
			ad_write(sc, mixer_channel_info[device].left_reg,
				 reg | 0x80);
	} else if (!(sc->mute[device] & MUTE_LEFT)) {
		if (device == AD1848_MONITOR_CHANNEL) {
			ad_write(sc, mixer_channel_info[device].left_reg,
				 reg | DIGITAL_MIX1_ENABLE);
			if (sc->open_mode & FREAD)
				ad1848_mute_wave_output(sc, WAVE_UNMUTE1, 1);
		} else if (device == AD1848_OUT_CHANNEL)
			ad_write(sc, mixer_channel_info[device].left_reg,
				 reg & ~MONO_OUTPUT_MUTE);
		else
			ad_write(sc, mixer_channel_info[device].left_reg,
				 reg & ~0x80);
	}

	if (!mixer_channel_info[device].right_reg)
		return;

	reg = ad_read(sc, mixer_channel_info[device].right_reg);

	if (mute & MUTE_RIGHT) {
		ad_write(sc, mixer_channel_info[device].right_reg, reg | 0x80);
	} else if (!(sc->mute[device] & MUTE_RIGHT)) {
		ad_write(sc, mixer_channel_info[device].right_reg, reg &~0x80);
	}
}

int
ad1848_set_channel_gain(struct ad1848_softc *sc, int device,
    struct ad1848_volume *gp)
{
	const struct ad1848_mixerinfo *info;
	u_char reg;
	u_int atten;

	info = &mixer_channel_info[device];
	sc->gains[device] = *gp;

	atten = (AUDIO_MAX_GAIN - gp->left) * (info->atten_bits + 1) /
		(AUDIO_MAX_GAIN + 1);

	reg = ad_read(sc, info->left_reg) & (info->atten_mask);
	if (device == AD1848_MONITOR_CHANNEL)
		reg |= ((atten & info->atten_bits) << 2);
	else
		reg |= ((atten & info->atten_bits));

	ad_write(sc, info->left_reg, reg);

	if (!info->right_reg)
		return 0;

	atten = (AUDIO_MAX_GAIN - gp->right) * (info->atten_bits + 1) /
		(AUDIO_MAX_GAIN + 1);
	reg = ad_read(sc, info->right_reg);
	reg &= info->atten_mask;
	ad_write(sc, info->right_reg, (atten & info->atten_bits) | reg);

	return 0;
}

int
ad1848_get_device_gain(struct ad1848_softc *sc, int device,
    struct ad1848_volume *gp)
{

	*gp = sc->gains[device];
	return 0;
}

int
ad1848_get_rec_gain(struct ad1848_softc *sc, struct ad1848_volume *gp)
{

	*gp = sc->rec_gain;
	return 0;
}

int
ad1848_set_rec_gain(struct ad1848_softc *sc, struct ad1848_volume *gp)
{
	u_char reg, gain;

	DPRINTF(("ad1848_set_rec_gain: %d:%d\n", gp->left, gp->right));

	sc->rec_gain = *gp;

	gain = (gp->left * (GAIN_22_5 + 1)) / (AUDIO_MAX_GAIN + 1);
	reg = ad_read(sc, SP_LEFT_INPUT_CONTROL);
	reg &= INPUT_GAIN_MASK;
	ad_write(sc, SP_LEFT_INPUT_CONTROL, (gain & 0x0f) | reg);

	gain = (gp->right * (GAIN_22_5 + 1)) / (AUDIO_MAX_GAIN + 1);
	reg = ad_read(sc, SP_RIGHT_INPUT_CONTROL);
	reg &= INPUT_GAIN_MASK;
	ad_write(sc, SP_RIGHT_INPUT_CONTROL, (gain & 0x0f) | reg);

	return 0;
}

void
ad1848_mute_wave_output(struct ad1848_softc *sc, int mute, int set)
{
	int m;

	DPRINTF(("ad1848_mute_wave_output: %d, %d\n", mute, set));

	if (mute == WAVE_MUTE2_INIT) {
		sc->wave_mute_status = 0;
		mute = WAVE_MUTE2;
	}
	if (set)
		m = sc->wave_mute_status |= mute;
	else
		m = sc->wave_mute_status &= ~mute;

	if (m & WAVE_MUTE0 || ((m & WAVE_UNMUTE1) == 0 && m & WAVE_MUTE2))
		ad1848_mute_channel(sc, AD1848_DAC_CHANNEL, MUTE_ALL);
	else
		ad1848_mute_channel(sc, AD1848_DAC_CHANNEL,
					    sc->mute[AD1848_DAC_CHANNEL]);
}

int
ad1848_set_mic_gain(struct ad1848_softc *sc, struct ad1848_volume *gp)
{
	u_char reg;

	DPRINTF(("cs4231_set_mic_gain: %d\n", gp->left));

	if (gp->left > AUDIO_MAX_GAIN/2) {
		sc->mic_gain_on = 1;
		reg = ad_read(sc, SP_LEFT_INPUT_CONTROL);
		ad_write(sc, SP_LEFT_INPUT_CONTROL,
			 reg | INPUT_MIC_GAIN_ENABLE);
	} else {
		sc->mic_gain_on = 0;
		reg = ad_read(sc, SP_LEFT_INPUT_CONTROL);
		ad_write(sc, SP_LEFT_INPUT_CONTROL,
			 reg & ~INPUT_MIC_GAIN_ENABLE);
	}

	return 0;
}

int
ad1848_get_mic_gain(struct ad1848_softc *sc, struct ad1848_volume *gp)
{
	if (sc->mic_gain_on)
		gp->left = gp->right = AUDIO_MAX_GAIN;
	else
		gp->left = gp->right = AUDIO_MIN_GAIN;
	return 0;
}

static const ad1848_devmap_t *
ad1848_mixer_find_dev(const ad1848_devmap_t *map, int cnt, mixer_ctrl_t *cp)
{
	int i;

	for (i = 0; i < cnt; i++) {
		if (map[i].id == cp->dev) {
			return (&map[i]);
		}
	}
	return 0;
}

int
ad1848_mixer_get_port(struct ad1848_softc *ac, const struct ad1848_devmap *map,
    int cnt, mixer_ctrl_t *cp)
{
	const ad1848_devmap_t *entry;
	struct ad1848_volume vol;
	int error;
	int dev;

	error = EINVAL;
	if (!(entry = ad1848_mixer_find_dev(map, cnt, cp)))
		return ENXIO;

	dev = entry->dev;

	switch (entry->kind) {
	case AD1848_KIND_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;

		if (dev < AD1848_AUX2_CHANNEL ||
		    dev > AD1848_MONITOR_CHANNEL)
			break;

		if (cp->un.value.num_channels != 1 &&
		    mixer_channel_info[dev].right_reg == 0)
			break;

		error = ad1848_get_device_gain(ac, dev, &vol);
		if (!error)
			ad1848_from_vol(cp, &vol);

		break;

	case AD1848_KIND_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM) break;

		cp->un.ord = ac->mute[dev] ? 1 : 0;
		error = 0;
		break;

	case AD1848_KIND_RECORDGAIN:
		if (cp->type != AUDIO_MIXER_VALUE) break;

		error = ad1848_get_rec_gain(ac, &vol);
		if (!error)
			ad1848_from_vol(cp, &vol);

		break;

	case AD1848_KIND_MICGAIN:
		if (cp->type != AUDIO_MIXER_VALUE) break;

		error = ad1848_get_mic_gain(ac, &vol);
		if (!error)
			ad1848_from_vol(cp, &vol);

		break;

	case AD1848_KIND_RECORDSOURCE:
		if (cp->type != AUDIO_MIXER_ENUM) break;
		cp->un.ord = ad1848_get_rec_port(ac);
		error = 0;
		break;

	default:
		printf ("Invalid kind\n");
		break;
	}

	return error;
}

int
ad1848_mixer_set_port(struct ad1848_softc *ac, const struct ad1848_devmap *map,
    int cnt, mixer_ctrl_t *cp)
{
	const ad1848_devmap_t *entry;
	struct ad1848_volume vol;
	int error;
	int dev;

	error = EINVAL;
	if (!(entry = ad1848_mixer_find_dev(map, cnt, cp)))
		return ENXIO;

	dev = entry->dev;

	switch (entry->kind) {
	case AD1848_KIND_LVL:
		if (cp->type != AUDIO_MIXER_VALUE)
			break;

		if (dev < AD1848_AUX2_CHANNEL ||
		    dev > AD1848_MONITOR_CHANNEL)
			break;

		if (cp->un.value.num_channels != 1 &&
		    mixer_channel_info[dev].right_reg == 0)
			break;

		ad1848_to_vol(cp, &vol);
		error = ad1848_set_channel_gain(ac, dev, &vol);
		break;

	case AD1848_KIND_MUTE:
		if (cp->type != AUDIO_MIXER_ENUM) break;

		ac->mute[dev] = (cp->un.ord ? MUTE_ALL : 0);
		ad1848_mute_channel(ac, dev, ac->mute[dev]);
		error = 0;
		break;

	case AD1848_KIND_RECORDGAIN:
		if (cp->type != AUDIO_MIXER_VALUE) break;

		ad1848_to_vol(cp, &vol);
		error = ad1848_set_rec_gain(ac, &vol);
		break;

	case AD1848_KIND_MICGAIN:
		if (cp->type != AUDIO_MIXER_VALUE) break;

		ad1848_to_vol(cp, &vol);
		error = ad1848_set_mic_gain(ac, &vol);
		break;

	case AD1848_KIND_RECORDSOURCE:
		if (cp->type != AUDIO_MIXER_ENUM) break;

		error = ad1848_set_rec_port(ac,  cp->un.ord);
		break;

	default:
		printf ("Invalid kind\n");
		break;
	}

	return error;
}

int
ad1848_query_encoding(void *addr, struct audio_encoding *fp)
{
	struct ad1848_softc *sc;

	sc = addr;
	switch (fp->index) {
	case 0:
		strcpy(fp->name, AudioEmulaw);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 1:
		strcpy(fp->name, AudioEalaw);
		fp->encoding = AUDIO_ENCODING_ALAW;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 2:
		strcpy(fp->name, AudioEslinear_le);
		fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	case 3:
		strcpy(fp->name, AudioEulinear);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = 0;
		break;

	case 4: /* only on CS4231 */
		strcpy(fp->name, AudioEslinear_be);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = sc->mode == 1
#if AD1845_HACK
		    || sc->is_ad1845
#endif
			? AUDIO_ENCODINGFLAG_EMULATED : 0;
		break;

		/* emulate some modes */
	case 5:
		strcpy(fp->name, AudioEslinear);
		fp->encoding = AUDIO_ENCODING_SLINEAR;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 6:
		strcpy(fp->name, AudioEulinear_le);
		fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 7:
		strcpy(fp->name, AudioEulinear_be);
		fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;

	case 8: /* only on CS4231 */
		if (sc->mode == 1 || sc->is_ad1845)
			return EINVAL;
		strcpy(fp->name, AudioEadpcm);
		fp->encoding = AUDIO_ENCODING_ADPCM;
		fp->precision = 4;
		fp->flags = 0;
		break;
	default:
		return EINVAL;
		/*NOTREACHED*/
	}
	return 0;
}

int
ad1848_set_params(void *addr, int setmode, int usemode,
    audio_params_t *p, audio_params_t *r, stream_filter_list_t *pfil,
    stream_filter_list_t *rfil)
{
	audio_params_t phw, rhw;
	struct ad1848_softc *sc;
	int error, bits, enc;
	stream_filter_factory_t *pswcode;
	stream_filter_factory_t *rswcode;

	DPRINTF(("ad1848_set_params: %u %u %u %u\n",
		 p->encoding, p->precision, p->channels, p->sample_rate));

	sc = addr;
	enc = p->encoding;
	pswcode = rswcode = 0;
	phw = *p;
	rhw = *r;
	switch (enc) {
	case AUDIO_ENCODING_SLINEAR_LE:
		if (p->precision == 8) {
			enc = AUDIO_ENCODING_ULINEAR_LE;
			phw.encoding = AUDIO_ENCODING_ULINEAR_LE;
			rhw.encoding = AUDIO_ENCODING_ULINEAR_LE;
			pswcode = rswcode = change_sign8;
		}
		break;
	case AUDIO_ENCODING_SLINEAR_BE:
		if (p->precision == 16 && (sc->mode == 1
#if AD1845_HACK
		    || sc->is_ad1845
#endif
			)) {
			enc = AUDIO_ENCODING_SLINEAR_LE;
			phw.encoding = AUDIO_ENCODING_SLINEAR_LE;
			rhw.encoding = AUDIO_ENCODING_SLINEAR_LE;
			pswcode = rswcode = swap_bytes;
		}
		break;
	case AUDIO_ENCODING_ULINEAR_LE:
		if (p->precision == 16) {
			enc = AUDIO_ENCODING_SLINEAR_LE;
			phw.encoding = AUDIO_ENCODING_SLINEAR_LE;
			rhw.encoding = AUDIO_ENCODING_SLINEAR_LE;
			pswcode = rswcode = change_sign16;
		}
		break;
	case AUDIO_ENCODING_ULINEAR_BE:
		if (p->precision == 16) {
			if (sc->mode == 1
#if AD1845_HACK
			    || sc->is_ad1845
#endif
				) {
				enc = AUDIO_ENCODING_SLINEAR_LE;
				phw.encoding = AUDIO_ENCODING_SLINEAR_LE;
				rhw.encoding = AUDIO_ENCODING_SLINEAR_LE;
				pswcode = swap_bytes_change_sign16;
				rswcode = swap_bytes_change_sign16;
			} else {
				enc = AUDIO_ENCODING_SLINEAR_BE;
				phw.encoding = AUDIO_ENCODING_SLINEAR_BE;
				rhw.encoding = AUDIO_ENCODING_SLINEAR_BE;
				pswcode = rswcode = change_sign16;
			}
		}
		break;
	}
	switch (enc) {
	case AUDIO_ENCODING_ULAW:
		bits = FMT_ULAW >> 5;
		break;
	case AUDIO_ENCODING_ALAW:
		bits = FMT_ALAW >> 5;
		break;
	case AUDIO_ENCODING_ADPCM:
		bits = FMT_ADPCM >> 5;
		break;
	case AUDIO_ENCODING_SLINEAR_LE:
		if (p->precision == 16)
			bits = FMT_TWOS_COMP >> 5;
		else
			return EINVAL;
		break;
	case AUDIO_ENCODING_SLINEAR_BE:
		if (p->precision == 16)
			bits = FMT_TWOS_COMP_BE >> 5;
		else
			return EINVAL;
		break;
	case AUDIO_ENCODING_ULINEAR_LE:
		if (p->precision == 8)
			bits = FMT_PCM8 >> 5;
		else
			return EINVAL;
		break;
	default:
		return EINVAL;
	}

	if (p->channels < 1 || p->channels > 2)
		return EINVAL;

	error = ad1848_set_speed(sc, &p->sample_rate);
	if (error)
		return error;
	phw.sample_rate = p->sample_rate;

	if (pswcode != NULL)
		pfil->append(pfil, pswcode, &phw);
	if (rswcode != NULL)
		rfil->append(rfil, rswcode, &rhw);

	sc->format_bits = bits;
	sc->channels = p->channels;
	sc->precision = p->precision;
	sc->need_commit = 1;

	DPRINTF(("ad1848_set_params succeeded, bits=%x\n", bits));
	return 0;
}

int
ad1848_set_rec_port(struct ad1848_softc *sc, int port)
{
	u_char inp, reg;

	DPRINTF(("ad1848_set_rec_port: 0x%x\n", port));

	if (port == MIC_IN_PORT)
		inp = MIC_INPUT;
	else if (port == LINE_IN_PORT)
		inp = LINE_INPUT;
	else if (port == DAC_IN_PORT)
		inp = MIXED_DAC_INPUT;
	else if (sc->mode >= 2 && port == AUX1_IN_PORT)
		inp = AUX_INPUT;
	else
		return EINVAL;

	reg = ad_read(sc, SP_LEFT_INPUT_CONTROL);
	reg &= INPUT_SOURCE_MASK;
	ad_write(sc, SP_LEFT_INPUT_CONTROL, (inp|reg));

	reg = ad_read(sc, SP_RIGHT_INPUT_CONTROL);
	reg &= INPUT_SOURCE_MASK;
	ad_write(sc, SP_RIGHT_INPUT_CONTROL, (inp|reg));

	sc->rec_port = port;

	return 0;
}

int
ad1848_get_rec_port(struct ad1848_softc *sc)
{
	return sc->rec_port;
}

int
ad1848_round_blocksize(void *addr, int blk,
    int mode, const audio_params_t *param)
{

	/* Round to a multiple of the biggest sample size. */
	return blk &= -4;
}

int
ad1848_open(void *addr, int flags)
{
	struct ad1848_softc *sc;
	u_char reg;

	sc = addr;
	DPRINTF(("ad1848_open: sc=%p\n", sc));

	sc->open_mode = flags;

	/* Enable interrupts */
	DPRINTF(("ad1848_open: enable intrs\n"));
	reg = ad_read(sc, SP_PIN_CONTROL);
	ad_write(sc, SP_PIN_CONTROL, reg | INTERRUPT_ENABLE);

	/* If recording && monitoring, the playback part is also used. */
	if (flags & FREAD && sc->mute[AD1848_MONITOR_CHANNEL] == 0)
		ad1848_mute_wave_output(sc, WAVE_UNMUTE1, 1);

#ifdef AUDIO_DEBUG
	if (ad1848debug)
		ad1848_dump_regs(sc);
#endif

	return 0;
}

void
ad1848_close(void *addr)
{
	struct ad1848_softc *sc;
	u_char reg;

	sc = addr;
	sc->open_mode = 0;

	ad1848_mute_wave_output(sc, WAVE_UNMUTE1, 0);

	/* Disable interrupts */
	DPRINTF(("ad1848_close: disable intrs\n"));
	reg = ad_read(sc, SP_PIN_CONTROL);
	ad_write(sc, SP_PIN_CONTROL, reg & ~INTERRUPT_ENABLE);

#ifdef AUDIO_DEBUG
	if (ad1848debug)
		ad1848_dump_regs(sc);
#endif
}

/*
 * Lower-level routines
 */
int
ad1848_commit_settings(void *addr)
{
	struct ad1848_softc *sc;
	int timeout;
	u_char fs;

	sc = addr;
	if (!sc->need_commit)
		return 0;

	mutex_spin_enter(&sc->sc_intr_lock);

	ad1848_mute_wave_output(sc, WAVE_MUTE0, 1);

	ad_set_MCE(sc, 1);	/* Enables changes to the format select reg */

	fs = sc->speed_bits | (sc->format_bits << 5);

	if (sc->channels == 2)
		fs |= FMT_STEREO;

	/*
	 * OPL3-SA2 (YMF711) is sometimes busy here.
	 * Wait until it becomes ready.
	 */
	for (timeout = 0;
	    timeout < 1000 && ADREAD(sc, AD1848_IADDR) & SP_IN_INIT; timeout++)
		delay(10);

	ad_write(sc, SP_CLOCK_DATA_FORMAT, fs);

	/*
	 * If mode >= 2 (CS4231), set I28 also.
	 * It's the capture format register.
	 */
	if (sc->mode >= 2) {
		/*
		 * Gravis Ultrasound MAX SDK sources says something about
		 * errata sheets, with the implication that these inb()s
		 * are necessary.
		 */
		(void)ADREAD(sc, AD1848_IDATA);
		(void)ADREAD(sc, AD1848_IDATA);
		/* Write to I8 starts resynchronization. Wait for completion. */
		timeout = 100000;
		while (timeout > 0 && ADREAD(sc, AD1848_IADDR) == SP_IN_INIT)
			timeout--;

		ad_write(sc, CS_REC_FORMAT, fs);
		(void)ADREAD(sc, AD1848_IDATA);
		(void)ADREAD(sc, AD1848_IDATA);
		/* Now wait for resync for capture side of the house */
	}
	/*
	 * Write to I8 starts resynchronization. Wait until it completes.
	 */
	timeout = 100000;
	while (timeout > 0 && ADREAD(sc, AD1848_IADDR) == SP_IN_INIT) {
		delay(10);
		timeout--;
	}

	if (ADREAD(sc, AD1848_IADDR) == SP_IN_INIT)
		printf("ad1848_commit: Auto calibration timed out\n");

	/*
	 * Starts the calibration process and
	 * enters playback mode after it.
	 */
	ad_set_MCE(sc, 0);
	wait_for_calibration(sc);

	ad1848_mute_wave_output(sc, WAVE_MUTE0, 0);

	mutex_spin_exit(&sc->sc_intr_lock);

	sc->need_commit = 0;

	return 0;
}

void
ad1848_reset(struct ad1848_softc *sc)
{
	u_char r;

	DPRINTF(("ad1848_reset\n"));

	/* Clear the PEN and CEN bits */
	r = ad_read(sc, SP_INTERFACE_CONFIG);
	r &= ~(CAPTURE_ENABLE | PLAYBACK_ENABLE);
	ad_write(sc, SP_INTERFACE_CONFIG, r);

	if (sc->mode >= 2) {
		ADWRITE(sc, AD1848_IADDR, CS_IRQ_STATUS);
		ADWRITE(sc, AD1848_IDATA, 0);
	}
	/* Clear interrupt status */
	ADWRITE(sc, AD1848_STATUS, 0);
#ifdef AUDIO_DEBUG
	if (ad1848debug)
		ad1848_dump_regs(sc);
#endif
}

int
ad1848_set_speed(struct ad1848_softc *sc, u_int *argp)
{
	/*
	 * The sampling speed is encoded in the least significant nible of I8.
	 * The LSB selects the clock source (0=24.576 MHz, 1=16.9344 MHz) and
	 * other three bits select the divisor (indirectly):
	 *
	 * The available speeds are in the following table. Keep the speeds in
	 * the increasing order.
	 */
	typedef struct {
		int	speed;
		u_char	bits;
	} speed_struct;
	u_long arg;

	static const speed_struct speed_table[] =  {
		{5510, (0 << 1) | 1},
		{5510, (0 << 1) | 1},
		{6620, (7 << 1) | 1},
		{8000, (0 << 1) | 0},
		{9600, (7 << 1) | 0},
		{11025, (1 << 1) | 1},
		{16000, (1 << 1) | 0},
		{18900, (2 << 1) | 1},
		{22050, (3 << 1) | 1},
		{27420, (2 << 1) | 0},
		{32000, (3 << 1) | 0},
		{33075, (6 << 1) | 1},
		{37800, (4 << 1) | 1},
		{44100, (5 << 1) | 1},
		{48000, (6 << 1) | 0}
	};

	int i, n, selected;

	arg = *argp;
	selected = -1;
	n = sizeof(speed_table) / sizeof(speed_struct);

	if (arg < speed_table[0].speed)
		selected = 0;
	if (arg > speed_table[n - 1].speed)
		selected = n - 1;

	for (i = 1 /*really*/ ; selected == -1 && i < n; i++)
		if (speed_table[i].speed == arg)
			selected = i;
		else if (speed_table[i].speed > arg) {
			int diff1, diff2;

			diff1 = arg - speed_table[i - 1].speed;
			diff2 = speed_table[i].speed - arg;

			if (diff1 < diff2)
				selected = i - 1;
			else
				selected = i;
		}

	if (selected == -1) {
		printf("ad1848: Can't find speed???\n");
		selected = 3;
	}

	sc->speed_bits = speed_table[selected].bits;
	sc->need_commit = 1;
	*argp = speed_table[selected].speed;

	return 0;
}

/*
 * Halt I/O
 */
int
ad1848_halt_output(void *addr)
{
	struct ad1848_softc *sc;
	u_char reg;

	DPRINTF(("ad1848: ad1848_halt_output\n"));
	sc = addr;
	reg = ad_read(sc, SP_INTERFACE_CONFIG);
	ad_write(sc, SP_INTERFACE_CONFIG, reg & ~PLAYBACK_ENABLE);

	return 0;
}

int
ad1848_halt_input(void *addr)
{
	struct ad1848_softc *sc;
	u_char reg;

	DPRINTF(("ad1848: ad1848_halt_input\n"));
	sc = addr;
	reg = ad_read(sc, SP_INTERFACE_CONFIG);
	ad_write(sc, SP_INTERFACE_CONFIG, reg & ~CAPTURE_ENABLE);

	return 0;
}

void
ad1848_get_locks(void *addr, kmutex_t **intr, kmutex_t **thread)
{
	struct ad1848_softc *sc;

	sc = addr;
	*intr = &sc->sc_intr_lock;
	*thread = &sc->sc_lock;
}

void
ad1848_init_locks(struct ad1848_softc *sc, int ipl)
{

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, ipl);
}

void
ad1848_destroy_locks(struct ad1848_softc *sc)
{

	mutex_destroy(&sc->sc_lock);
	mutex_destroy(&sc->sc_intr_lock);
}

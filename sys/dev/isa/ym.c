/*	$NetBSD: ym.c,v 1.44 2013/11/08 03:12:17 christos Exp $	*/

/*-
 * Copyright (c) 1999-2002, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by ITOH Yasufumi.
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
 * Copyright (c) 1998 Constantine Sapuntzakis. All rights reserved.
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
 *  Original code from OpenBSD.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ym.c,v 1.44 2013/11/08 03:12:17 christos Exp $");

#include "mpu_ym.h"
#include "opt_ym.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/proc.h>

#include <sys/cpu.h>
#include <sys/intr.h>
#include <sys/bus.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/ad1848reg.h>
#include <dev/isa/ad1848var.h>
#include <dev/ic/opl3sa3reg.h>
#include <dev/isa/wssreg.h>
#if NMPU_YM > 0
#include <dev/ic/mpuvar.h>
#endif
#include <dev/isa/ymvar.h>
#include <dev/isa/sbreg.h>

/* Power management mode. */
#ifndef YM_POWER_MODE
#define YM_POWER_MODE		YM_POWER_POWERSAVE
#endif

/* Time in second before power down the chip. */
#ifndef YM_POWER_OFF_SEC
#define YM_POWER_OFF_SEC	5
#endif

/* Default mixer settings. */
#ifndef YM_VOL_MASTER
#define YM_VOL_MASTER		208
#endif

#ifndef YM_VOL_DAC
#define YM_VOL_DAC		224
#endif

#ifndef YM_VOL_OPL3
#define YM_VOL_OPL3		184
#endif

/*
 * Default position of the equalizer.
 */
#ifndef YM_DEFAULT_TREBLE
#define YM_DEFAULT_TREBLE	YM_EQ_FLAT_OFFSET
#endif
#ifndef YM_DEFAULT_BASS
#define YM_DEFAULT_BASS		YM_EQ_FLAT_OFFSET
#endif

#ifdef __i386__		/* XXX */
# include "joy.h"
#else
# define NJOY	0
#endif

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (ymdebug) printf x
int	ymdebug = 0;
#else
#define DPRINTF(x)
#endif
#define DVNAME(softc)	(device_xname((softc)->sc_ad1848.sc_ad1848.sc_dev))

int	ym_getdev(void *, struct audio_device *);
int	ym_mixer_set_port(void *, mixer_ctrl_t *);
int	ym_mixer_get_port(void *, mixer_ctrl_t *);
int	ym_query_devinfo(void *, mixer_devinfo_t *);
int	ym_intr(void *);
#ifndef AUDIO_NO_POWER_CTL
static void ym_save_codec_regs(struct ym_softc *);
static void ym_restore_codec_regs(struct ym_softc *);
int	ym_codec_power_ctl(void *, int);
static void ym_chip_powerdown(struct ym_softc *);
static void ym_chip_powerup(struct ym_softc *, int);
static void	ym_powerdown_blocks(struct ym_softc *);
static void	ym_powerdown_callout(void *);
void	ym_power_ctl(struct ym_softc *, int, int);
#endif

static void ym_init(struct ym_softc *);
static void ym_mute(struct ym_softc *, int, int);
static void ym_set_master_gain(struct ym_softc *, struct ad1848_volume*);
static void ym_hvol_to_master_gain(struct ym_softc *);
static void ym_set_mic_gain(struct ym_softc *, int);
static void ym_set_3d(struct ym_softc *, mixer_ctrl_t *,
	struct ad1848_volume *, int);
static bool ym_suspend(device_t, const pmf_qual_t *);
static bool ym_resume(device_t, const pmf_qual_t *);


const struct audio_hw_if ym_hw_if = {
	ad1848_isa_open,
	ad1848_isa_close,
	NULL,
	ad1848_query_encoding,
	ad1848_set_params,
	ad1848_round_blocksize,
	ad1848_commit_settings,
	NULL,
	NULL,
	NULL,
	NULL,
	ad1848_isa_halt_output,
	ad1848_isa_halt_input,
	NULL,
	ym_getdev,
	NULL,
	ym_mixer_set_port,
	ym_mixer_get_port,
	ym_query_devinfo,
	ad1848_isa_malloc,
	ad1848_isa_free,
	ad1848_isa_round_buffersize,
	ad1848_isa_mappage,
	ad1848_isa_get_props,
	ad1848_isa_trigger_output,
	ad1848_isa_trigger_input,
	NULL,
	ad1848_get_locks,
};

static inline int ym_read(struct ym_softc *, int);
static inline void ym_write(struct ym_softc *, int, int);

void
ym_attach(struct ym_softc *sc)
{
	static struct ad1848_volume vol_master = {YM_VOL_MASTER, YM_VOL_MASTER};
	static struct ad1848_volume vol_dac    = {YM_VOL_DAC,    YM_VOL_DAC};
	static struct ad1848_volume vol_opl3   = {YM_VOL_OPL3,   YM_VOL_OPL3};
	struct ad1848_softc *ac;
	mixer_ctrl_t mctl;
	struct audio_attach_args arg;

	ac = &sc->sc_ad1848.sc_ad1848;
	callout_init(&sc->sc_powerdown_ch, CALLOUT_MPSAFE);
	cv_init(&sc->sc_cv, "ym");
	ad1848_init_locks(ac, IPL_AUDIO);

	/* Mute the output to reduce noise during initialization. */
	ym_mute(sc, SA3_VOL_L, 1);
	ym_mute(sc, SA3_VOL_R, 1);

	sc->sc_version = ym_read(sc, SA3_MISC) & SA3_MISC_VER;
	ac->chip_name = YM_IS_SA3(sc) ? "OPL3-SA3" : "OPL3-SA2";

	sc->sc_ad1848.sc_ih = isa_intr_establish(sc->sc_ic, sc->ym_irq,
	    IST_EDGE, IPL_AUDIO, ym_intr, sc);

#ifndef AUDIO_NO_POWER_CTL
	sc->sc_ad1848.powerctl = ym_codec_power_ctl;
	sc->sc_ad1848.powerarg = sc;
#endif
	ad1848_isa_attach(&sc->sc_ad1848);
	printf("\n");
	ac->parent = sc;

	/* Establish chip in well known mode */
	ym_set_master_gain(sc, &vol_master);
	ym_set_mic_gain(sc, 0);
	sc->master_mute = 0;

	/* Override ad1848 settings. */
	ad1848_set_channel_gain(ac, AD1848_DAC_CHANNEL, &vol_dac);
	ad1848_set_channel_gain(ac, AD1848_AUX2_CHANNEL, &vol_opl3);

	/*
	 * Mute all external sources.  If you change this, you must
	 * also change the initial value of sc->sc_external_sources
	 * (currently 0 --- no external source is active).
	 */
	sc->mic_mute = 1;
	ym_mute(sc, SA3_MIC_VOL, sc->mic_mute);
	ad1848_mute_channel(ac, AD1848_AUX1_CHANNEL, MUTE_ALL);	/* CD */
	ad1848_mute_channel(ac, AD1848_LINE_CHANNEL, MUTE_ALL);	/* line */
	ac->mute[AD1848_AUX1_CHANNEL] = MUTE_ALL;
	ac->mute[AD1848_LINE_CHANNEL] = MUTE_ALL;
	/* speaker is muted by default */

	/* We use only one IRQ (IRQ-A). */
	ym_write(sc, SA3_IRQ_CONF, SA3_IRQ_CONF_MPU_A | SA3_IRQ_CONF_WSS_A);
	ym_write(sc, SA3_HVOL_INTR_CNF, SA3_HVOL_INTR_CNF_A);

	/* audio at ym attachment */
	sc->sc_audiodev = audio_attach_mi(&ym_hw_if, ac, ac->sc_dev);

	/* opl at ym attachment */
	if (sc->sc_opl_ioh) {
		arg.type = AUDIODEV_TYPE_OPL;
		arg.hwif = 0;
		arg.hdl = 0;
		(void)config_found(ac->sc_dev, &arg, audioprint);
	}

#if NMPU_YM > 0
	/* mpu at ym attachment */
	if (sc->sc_mpu_ioh) {
		arg.type = AUDIODEV_TYPE_MPU;
		arg.hwif = 0;
		arg.hdl = 0;
		sc->sc_mpudev = config_found(ac->sc_dev, &arg, audioprint);
	}
#endif

	/* This must be AFTER the attachment of sub-devices. */
	mutex_spin_enter(&sc->sc_ad1848.sc_ad1848.sc_intr_lock);
	ym_init(sc);

#ifndef AUDIO_NO_POWER_CTL
	/*
	 * Initialize power control.
	 */
	sc->sc_pow_mode = YM_POWER_MODE;
	sc->sc_pow_timeout = YM_POWER_OFF_SEC;

	sc->sc_on_blocks = sc->sc_turning_off =
	    YM_POWER_CODEC_P | YM_POWER_CODEC_R |
	    YM_POWER_OPL3 | YM_POWER_MPU401 | YM_POWER_3D |
	    YM_POWER_CODEC_DA | YM_POWER_CODEC_AD | YM_POWER_OPL3_DA;
#if NJOY > 0
	sc->sc_on_blocks |= YM_POWER_JOYSTICK;	/* prevents chip powerdown */
#endif
	ym_powerdown_blocks(sc);
	mutex_spin_exit(&sc->sc_ad1848.sc_ad1848.sc_intr_lock);

	if (!pmf_device_register(ac->sc_dev, ym_suspend, ym_resume)) {
		aprint_error_dev(ac->sc_dev,
		    "cannot set power mgmt handler\n");
	}
#endif

	/* Set tone control to the default position. */
	mctl.un.value.num_channels = 1;
	mctl.un.value.level[AUDIO_MIXER_LEVEL_MONO] = YM_DEFAULT_TREBLE;
	mctl.dev = YM_MASTER_TREBLE;
	ym_mixer_set_port(sc, &mctl);
	mctl.un.value.level[AUDIO_MIXER_LEVEL_MONO] = YM_DEFAULT_BASS;
	mctl.dev = YM_MASTER_BASS;
	ym_mixer_set_port(sc, &mctl);

	/* Unmute the output now if the chip is on. */
#ifndef AUDIO_NO_POWER_CTL
	if (sc->sc_on_blocks & YM_POWER_ACTIVE)
#endif
	{
		ym_mute(sc, SA3_VOL_L, sc->master_mute);
		ym_mute(sc, SA3_VOL_R, sc->master_mute);
	}
}

static inline int
ym_read(struct ym_softc *sc, int reg)
{

	bus_space_write_1(sc->sc_iot, sc->sc_controlioh,
	    SA3_CTL_INDEX, (reg & 0xff));
	return bus_space_read_1(sc->sc_iot, sc->sc_controlioh, SA3_CTL_DATA);
}

static inline void
ym_write(struct ym_softc *sc, int reg, int data)
{

	bus_space_write_1(sc->sc_iot, sc->sc_controlioh,
	    SA3_CTL_INDEX, (reg & 0xff));
	bus_space_write_1(sc->sc_iot, sc->sc_controlioh,
	    SA3_CTL_DATA, (data & 0xff));
}

static void
ym_init(struct ym_softc *sc)
{
	uint8_t dpd, apd;

	KASSERT(mutex_owned(&sc->sc_ad1848.sc_ad1848.sc_intr_lock));

	/* Mute SoundBlaster output if possible. */
	if (sc->sc_sb_ioh) {
		bus_space_write_1(sc->sc_iot, sc->sc_sb_ioh, SBP_MIXER_ADDR,
		    SBP_MASTER_VOL);
		bus_space_write_1(sc->sc_iot, sc->sc_sb_ioh, SBP_MIXER_DATA,
		    0x00);
	}

	if (!YM_IS_SA3(sc)) {
		/* OPL3-SA2 */
		ym_write(sc, SA3_PWR_MNG, SA2_PWR_MNG_CLKO |
		    (sc->sc_opl_ioh == 0 ? SA2_PWR_MNG_FMPS : 0));
		return;
	}

	/* OPL3-SA3 */
	/* Figure out which part can be power down. */
	dpd = SA3_DPWRDWN_SB		/* we never use SB */
#if NMPU_YM > 0
	    | (sc->sc_mpu_ioh ? 0 : SA3_DPWRDWN_MPU)
#else
	    | SA3_DPWRDWN_MPU
#endif
#if NJOY == 0
	    | SA3_DPWRDWN_JOY
#endif
	    | SA3_DPWRDWN_PNP	/* ISA Plug and Play is done */
	    /*
	     * The master clock is for external wavetable synthesizer
	     * OPL4-ML (YMF704) or OPL4-ML2 (YMF721),
	     * and is currently unused.
	     */
	    | SA3_DPWRDWN_MCLKO;

	apd = SA3_APWRDWN_SBDAC;	/* we never use SB */

	/* Power down OPL3 if not attached. */
	if (sc->sc_opl_ioh == 0) {
		dpd |= SA3_DPWRDWN_FM;
		apd |= SA3_APWRDWN_FMDAC;
	}
	/* CODEC is always attached. */

	/* Power down unused digital parts. */
	ym_write(sc, SA3_DPWRDWN, dpd);

	/* Power down unused analog parts. */
	ym_write(sc, SA3_APWRDWN, apd);
}


int
ym_getdev(void *addr, struct audio_device *retp)
{
	struct ym_softc *sc;
	struct ad1848_softc *ac;

	sc = addr;
	ac = &sc->sc_ad1848.sc_ad1848;
	strlcpy(retp->name, ac->chip_name, sizeof(retp->name));
	snprintf(retp->version, sizeof(retp->version), "%d", sc->sc_version);
	strlcpy(retp->config, "ym", sizeof(retp->config));

	return 0;
}


static ad1848_devmap_t mappings[] = {
	{ YM_DAC_LVL, AD1848_KIND_LVL, AD1848_DAC_CHANNEL },
	{ YM_MIDI_LVL, AD1848_KIND_LVL, AD1848_AUX2_CHANNEL },
	{ YM_CD_LVL, AD1848_KIND_LVL, AD1848_AUX1_CHANNEL },
	{ YM_LINE_LVL, AD1848_KIND_LVL, AD1848_LINE_CHANNEL },
	{ YM_SPEAKER_LVL, AD1848_KIND_LVL, AD1848_MONO_CHANNEL },
	{ YM_MONITOR_LVL, AD1848_KIND_LVL, AD1848_MONITOR_CHANNEL },
	{ YM_DAC_MUTE, AD1848_KIND_MUTE, AD1848_DAC_CHANNEL },
	{ YM_MIDI_MUTE, AD1848_KIND_MUTE, AD1848_AUX2_CHANNEL },
	{ YM_CD_MUTE, AD1848_KIND_MUTE, AD1848_AUX1_CHANNEL },
	{ YM_LINE_MUTE, AD1848_KIND_MUTE, AD1848_LINE_CHANNEL },
	{ YM_SPEAKER_MUTE, AD1848_KIND_MUTE, AD1848_MONO_CHANNEL },
	{ YM_MONITOR_MUTE, AD1848_KIND_MUTE, AD1848_MONITOR_CHANNEL },
	{ YM_REC_LVL, AD1848_KIND_RECORDGAIN, -1 },
	{ YM_RECORD_SOURCE, AD1848_KIND_RECORDSOURCE, -1}
};

#define NUMMAP	(sizeof(mappings) / sizeof(mappings[0]))


static void
ym_mute(struct ym_softc *sc, int left_reg, int mute)
{
	uint8_t reg;

	reg = ym_read(sc, left_reg);
	if (mute)
		ym_write(sc, left_reg, reg | 0x80);
	else
		ym_write(sc, left_reg, reg & ~0x80);
}


static void
ym_set_master_gain(struct ym_softc *sc, struct ad1848_volume *vol)
{
	u_int atten;

	sc->master_gain = *vol;

	atten = ((AUDIO_MAX_GAIN - vol->left) * (SA3_VOL_MV + 1)) /
		(AUDIO_MAX_GAIN + 1);

	ym_write(sc, SA3_VOL_L, (ym_read(sc, SA3_VOL_L) & ~SA3_VOL_MV) | atten);

	atten = ((AUDIO_MAX_GAIN - vol->right) * (SA3_VOL_MV + 1)) /
		(AUDIO_MAX_GAIN + 1);

	ym_write(sc, SA3_VOL_R, (ym_read(sc, SA3_VOL_R) & ~SA3_VOL_MV) | atten);
}

/*
 * Read current setting of master volume from hardware
 * and update the software value if changed.
 * [SA3] This function clears hardware volume interrupt.
 */
static void
ym_hvol_to_master_gain(struct ym_softc *sc)
{
	u_int prevval, val;
	int changed;

	changed = 0;
	val = SA3_VOL_MV & ~ym_read(sc, SA3_VOL_L);
	prevval = (sc->master_gain.left * (SA3_VOL_MV + 1)) /
	    (AUDIO_MAX_GAIN + 1);
	if (val != prevval) {
		sc->master_gain.left =
		    val * ((AUDIO_MAX_GAIN + 1) / (SA3_VOL_MV + 1));
		changed = 1;
	}

	val = SA3_VOL_MV & ~ym_read(sc, SA3_VOL_R);
	prevval = (sc->master_gain.right * (SA3_VOL_MV + 1)) /
	    (AUDIO_MAX_GAIN + 1);
	if (val != prevval) {
		sc->master_gain.right =
		    val * ((AUDIO_MAX_GAIN + 1) / (SA3_VOL_MV + 1));
		changed = 1;
	}

#if 0	/* XXX NOT YET */
	/* Notify the change to async processes. */
	if (changed && sc->sc_audiodev)
		mixer_signal(sc->sc_audiodev);
#else
	__USE(changed);
#endif
}

static void
ym_set_mic_gain(struct ym_softc *sc, int vol)
{
	u_int atten;

	sc->mic_gain = vol;

	atten = ((AUDIO_MAX_GAIN - vol) * (SA3_MIC_MCV + 1)) /
		(AUDIO_MAX_GAIN + 1);

	ym_write(sc, SA3_MIC_VOL,
		 (ym_read(sc, SA3_MIC_VOL) & ~SA3_MIC_MCV) | atten);
}

static void
ym_set_3d(struct ym_softc *sc, mixer_ctrl_t *cp,
    struct ad1848_volume *val, int reg)
{
	uint8_t l, r, e;

	KASSERT(mutex_owned(&sc->sc_ad1848.sc_ad1848.sc_intr_lock));

	ad1848_to_vol(cp, val);

	l = val->left;
	r = val->right;
	if (reg != SA3_3D_WIDE) {
		/* flat on center */
		l = YM_EQ_EXPAND_VALUE(l);
		r = YM_EQ_EXPAND_VALUE(r);
	}

	e = (l * (SA3_3D_BITS + 1) + (SA3_3D_BITS + 1) / 2) /
	    (AUDIO_MAX_GAIN + 1) << SA3_3D_LSHIFT |
	    (r * (SA3_3D_BITS + 1) + (SA3_3D_BITS + 1) / 2) /
	    (AUDIO_MAX_GAIN + 1) << SA3_3D_RSHIFT;

#ifndef AUDIO_NO_POWER_CTL
	/* turn wide stereo on if necessary */
	if (e)
		ym_power_ctl(sc, YM_POWER_3D, 1);
#endif

	ym_write(sc, reg, e);

#ifndef AUDIO_NO_POWER_CTL
	/* turn wide stereo off if necessary */
	if (YM_EQ_OFF(&sc->sc_treble) && YM_EQ_OFF(&sc->sc_bass) &&
	    YM_WIDE_OFF(&sc->sc_wide))
		ym_power_ctl(sc, YM_POWER_3D, 0);
#endif
}

int
ym_mixer_set_port(void *addr, mixer_ctrl_t *cp)
{
	struct ad1848_softc *ac;
	struct ym_softc *sc;
	struct ad1848_volume vol;
	int error;
	uint8_t extsources;

	ac = addr;
	sc = ac->parent;
	error = 0;
	DPRINTF(("%s: ym_mixer_set_port: dev 0x%x, type 0x%x, 0x%x (%d; %d, %d)\n",
		DVNAME(sc), cp->dev, cp->type, cp->un.ord,
		cp->un.value.num_channels, cp->un.value.level[0],
		cp->un.value.level[1]));

	/* SA2 doesn't have equalizer */
	if (!YM_IS_SA3(sc) && YM_MIXER_SA3_ONLY(cp->dev))
		return ENXIO;

	mutex_spin_enter(&ac->sc_intr_lock);

#ifndef AUDIO_NO_POWER_CTL
	/* Power-up chip */
	ym_power_ctl(sc, YM_POWER_CODEC_CTL, 1);
#endif

	switch (cp->dev) {
	case YM_OUTPUT_LVL:
		ad1848_to_vol(cp, &vol);
		ym_set_master_gain(sc, &vol);
		goto out;

	case YM_OUTPUT_MUTE:
		sc->master_mute = (cp->un.ord != 0);
		ym_mute(sc, SA3_VOL_L, sc->master_mute);
		ym_mute(sc, SA3_VOL_R, sc->master_mute);
		goto out;

	case YM_MIC_LVL:
		if (cp->un.value.num_channels != 1)
			error = EINVAL;
		else
			ym_set_mic_gain(sc,
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]);
		goto out;

	case YM_MASTER_EQMODE:
		sc->sc_eqmode = cp->un.ord & SA3_SYS_CTL_YMODE;
		ym_write(sc, SA3_SYS_CTL, (ym_read(sc, SA3_SYS_CTL) &
			     ~SA3_SYS_CTL_YMODE) | sc->sc_eqmode);
		goto out;

	case YM_MASTER_TREBLE:
		ym_set_3d(sc, cp, &sc->sc_treble, SA3_3D_TREBLE);
		goto out;

	case YM_MASTER_BASS:
		ym_set_3d(sc, cp, &sc->sc_bass, SA3_3D_BASS);
		goto out;

	case YM_MASTER_WIDE:
		ym_set_3d(sc, cp, &sc->sc_wide, SA3_3D_WIDE);
		goto out;

#ifndef AUDIO_NO_POWER_CTL
	case YM_PWR_MODE:
		if ((unsigned) cp->un.ord > YM_POWER_NOSAVE)
			error = EINVAL;
		else
			sc->sc_pow_mode = cp->un.ord;
		goto out;

	case YM_PWR_TIMEOUT:
		if (cp->un.value.num_channels != 1)
			error = EINVAL;
		else
			sc->sc_pow_timeout =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		goto out;

	/*
	 * Needs power-up to hear external sources.
	 */
	case YM_CD_MUTE:
	case YM_LINE_MUTE:
	case YM_SPEAKER_MUTE:
	case YM_MIC_MUTE:
		extsources = YM_MIXER_TO_XS(cp->dev);
		if (cp->un.ord) {
			if ((sc->sc_external_sources &= ~extsources) == 0) {
				/*
				 * All the external sources are muted
				 *  --- no need to keep the chip on.
				 */
				ym_power_ctl(sc, YM_POWER_EXT_SRC, 0);
				DPRINTF(("%s: ym_mixer_set_port: off for ext\n",
					DVNAME(sc)));
			}
		} else {
			/* mute off - power-up the chip */
			sc->sc_external_sources |= extsources;
			ym_power_ctl(sc, YM_POWER_EXT_SRC, 1);
			DPRINTF(("%s: ym_mixer_set_port: on for ext\n",
				DVNAME(sc)));
		}
		break;	/* fall to ad1848_mixer_set_port() */

	/*
	 * Power on/off the playback part for monitoring.
	 */
	case YM_MONITOR_MUTE:
		if ((ac->open_mode & (FREAD | FWRITE)) == FREAD)
			ym_power_ctl(sc, YM_POWER_CODEC_P | YM_POWER_CODEC_DA,
			    cp->un.ord == 0);
		break;	/* fall to ad1848_mixer_set_port() */
#endif
	}

	error = ad1848_mixer_set_port(ac, mappings, NUMMAP, cp);

	if (error != ENXIO)
		goto out;

	error = 0;

	switch (cp->dev) {
	case YM_MIC_MUTE:
		sc->mic_mute = (cp->un.ord != 0);
		ym_mute(sc, SA3_MIC_VOL, sc->mic_mute);
		break;

	default:
		error = ENXIO;
		break;
	}

out:
#ifndef AUDIO_NO_POWER_CTL
	/* Power-down chip */
	ym_power_ctl(sc, YM_POWER_CODEC_CTL, 0);
#endif
	mutex_spin_exit(&ac->sc_intr_lock);

	return error;
}

int
ym_mixer_get_port(void *addr, mixer_ctrl_t *cp)
{
	struct ad1848_softc *ac;
	struct ym_softc *sc;
	int error;

	ac = addr;
	sc = ac->parent;
	/* SA2 doesn't have equalizer */
	if (!YM_IS_SA3(sc) && YM_MIXER_SA3_ONLY(cp->dev))
		return ENXIO;

	switch (cp->dev) {
	case YM_OUTPUT_LVL:
		if (!YM_IS_SA3(sc)) {
			/*
			 * SA2 doesn't have hardware volume interrupt.
			 * Read current value and update every time.
			 */
			mutex_spin_enter(&ac->sc_intr_lock);
#ifndef AUDIO_NO_POWER_CTL
			/* Power-up chip */
			ym_power_ctl(sc, YM_POWER_CODEC_CTL, 1);
#endif
			ym_hvol_to_master_gain(sc);
#ifndef AUDIO_NO_POWER_CTL
			/* Power-down chip */
			ym_power_ctl(sc, YM_POWER_CODEC_CTL, 0);
#endif
			mutex_spin_exit(&ac->sc_intr_lock);
		}
		ad1848_from_vol(cp, &sc->master_gain);
		return 0;

	case YM_OUTPUT_MUTE:
		cp->un.ord = sc->master_mute;
		return 0;

	case YM_MIC_LVL:
		if (cp->un.value.num_channels != 1)
			return EINVAL;
		cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] = sc->mic_gain;
		return 0;

	case YM_MASTER_EQMODE:
		cp->un.ord = sc->sc_eqmode;
		return 0;

	case YM_MASTER_TREBLE:
		ad1848_from_vol(cp, &sc->sc_treble);
		return 0;

	case YM_MASTER_BASS:
		ad1848_from_vol(cp, &sc->sc_bass);
		return 0;

	case YM_MASTER_WIDE:
		ad1848_from_vol(cp, &sc->sc_wide);
		return 0;

#ifndef AUDIO_NO_POWER_CTL
	case YM_PWR_MODE:
		cp->un.ord = sc->sc_pow_mode;
		return 0;

	case YM_PWR_TIMEOUT:
		if (cp->un.value.num_channels != 1)
			return EINVAL;
		cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] = sc->sc_pow_timeout;
		return 0;
#endif
	}

	error = ad1848_mixer_get_port(ac, mappings, NUMMAP, cp);

	if (error != ENXIO)
		return error;

	error = 0;

	switch (cp->dev) {
	case YM_MIC_MUTE:
		cp->un.ord = sc->mic_mute;
		break;

	default:
		error = ENXIO;
		break;
	}

	return error;
}

static const char *mixer_classes[] = {
	AudioCinputs, AudioCrecord, AudioCoutputs, AudioCmonitor,
#ifndef AUDIO_NO_POWER_CTL
	AudioCpower,
#endif
	AudioCequalization
};

int
ym_query_devinfo(void *addr, mixer_devinfo_t *dip)
{
	static const char *mixer_port_names[] = {
		AudioNdac, AudioNmidi, AudioNcd, AudioNline, AudioNspeaker,
		AudioNmicrophone, AudioNmonitor
	};
	struct ad1848_softc *ac;
	struct ym_softc *sc;

	ac = addr;
	sc = ac->parent;
	/* SA2 doesn't have equalizer */
	if (!YM_IS_SA3(sc) && YM_MIXER_SA3_ONLY(dip->index))
		return ENXIO;

	dip->next = dip->prev = AUDIO_MIXER_LAST;

	switch(dip->index) {
	case YM_INPUT_CLASS:
	case YM_OUTPUT_CLASS:
	case YM_MONITOR_CLASS:
	case YM_RECORD_CLASS:
#ifndef AUDIO_NO_POWER_CTL
	case YM_PWR_CLASS:
#endif
	case YM_EQ_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = dip->index;
		strcpy(dip->label.name,
		       mixer_classes[dip->index - YM_INPUT_CLASS]);
		break;

	case YM_DAC_LVL:
	case YM_MIDI_LVL:
	case YM_CD_LVL:
	case YM_LINE_LVL:
	case YM_SPEAKER_LVL:
	case YM_MIC_LVL:
	case YM_MONITOR_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		if (dip->index == YM_MONITOR_LVL)
			dip->mixer_class = YM_MONITOR_CLASS;
		else
			dip->mixer_class = YM_INPUT_CLASS;

		dip->next = dip->index + 7;

		strcpy(dip->label.name,
		       mixer_port_names[dip->index - YM_DAC_LVL]);

		if (dip->index == YM_SPEAKER_LVL ||
		    dip->index == YM_MIC_LVL)
			dip->un.v.num_channels = 1;
		else
			dip->un.v.num_channels = 2;

		if (dip->index == YM_SPEAKER_LVL)
			dip->un.v.delta = 1 << (8 - 4 /* valid bits */);
		else if (dip->index == YM_DAC_LVL ||
		    dip->index == YM_MONITOR_LVL)
			dip->un.v.delta = 1 << (8 - 6 /* valid bits */);
		else
			dip->un.v.delta = 1 << (8 - 5 /* valid bits */);

		strcpy(dip->un.v.units.name, AudioNvolume);
		break;

	case YM_DAC_MUTE:
	case YM_MIDI_MUTE:
	case YM_CD_MUTE:
	case YM_LINE_MUTE:
	case YM_SPEAKER_MUTE:
	case YM_MIC_MUTE:
	case YM_MONITOR_MUTE:
		if (dip->index == YM_MONITOR_MUTE)
			dip->mixer_class = YM_MONITOR_CLASS;
		else
			dip->mixer_class = YM_INPUT_CLASS;
		dip->type = AUDIO_MIXER_ENUM;
		dip->prev = dip->index - 7;
	mute:
		strcpy(dip->label.name, AudioNmute);
		dip->un.e.num_mem = 2;
		strcpy(dip->un.e.member[0].label.name, AudioNoff);
		dip->un.e.member[0].ord = 0;
		strcpy(dip->un.e.member[1].label.name, AudioNon);
		dip->un.e.member[1].ord = 1;
		break;


	case YM_OUTPUT_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = YM_OUTPUT_CLASS;
		dip->next = YM_OUTPUT_MUTE;
		strcpy(dip->label.name, AudioNmaster);
		dip->un.v.num_channels = 2;
		dip->un.v.delta = (AUDIO_MAX_GAIN + 1) / (SA3_VOL_MV + 1);
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;

	case YM_OUTPUT_MUTE:
		dip->mixer_class = YM_OUTPUT_CLASS;
		dip->type = AUDIO_MIXER_ENUM;
		dip->prev = YM_OUTPUT_LVL;
		goto mute;


	case YM_REC_LVL:	/* record level */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = YM_RECORD_CLASS;
		dip->next = YM_RECORD_SOURCE;
		strcpy(dip->label.name, AudioNrecord);
		dip->un.v.num_channels = 2;
		dip->un.v.delta = 1 << (8 - 4 /* valid bits */);
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;

	case YM_RECORD_SOURCE:
		dip->mixer_class = YM_RECORD_CLASS;
		dip->type = AUDIO_MIXER_ENUM;
		dip->prev = YM_REC_LVL;
		strcpy(dip->label.name, AudioNsource);
		dip->un.e.num_mem = 4;
		strcpy(dip->un.e.member[0].label.name, AudioNmicrophone);
		dip->un.e.member[0].ord = MIC_IN_PORT;
		strcpy(dip->un.e.member[1].label.name, AudioNline);
		dip->un.e.member[1].ord = LINE_IN_PORT;
		strcpy(dip->un.e.member[2].label.name, AudioNdac);
		dip->un.e.member[2].ord = DAC_IN_PORT;
		strcpy(dip->un.e.member[3].label.name, AudioNcd);
		dip->un.e.member[3].ord = AUX1_IN_PORT;
		break;


	case YM_MASTER_EQMODE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = YM_EQ_CLASS;
		strcpy(dip->label.name, AudioNmode);
		strcpy(dip->un.v.units.name, AudioNmode);
		dip->un.e.num_mem = 4;
		strcpy(dip->un.e.member[0].label.name, AudioNdesktop);
		dip->un.e.member[0].ord = SA3_SYS_CTL_YMODE0;
		strcpy(dip->un.e.member[1].label.name, AudioNlaptop);
		dip->un.e.member[1].ord = SA3_SYS_CTL_YMODE1;
		strcpy(dip->un.e.member[2].label.name, AudioNsubnote);
		dip->un.e.member[2].ord = SA3_SYS_CTL_YMODE2;
		strcpy(dip->un.e.member[3].label.name, AudioNhifi);
		dip->un.e.member[3].ord = SA3_SYS_CTL_YMODE3;
		break;

	case YM_MASTER_TREBLE:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = YM_EQ_CLASS;
		strcpy(dip->label.name, AudioNtreble);
		dip->un.v.num_channels = 2;
		dip->un.v.delta = (AUDIO_MAX_GAIN + 1) / (SA3_3D_BITS + 1)
		    >> YM_EQ_REDUCE_BIT;
		strcpy(dip->un.v.units.name, AudioNtreble);
		break;

	case YM_MASTER_BASS:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = YM_EQ_CLASS;
		strcpy(dip->label.name, AudioNbass);
		dip->un.v.num_channels = 2;
		dip->un.v.delta = (AUDIO_MAX_GAIN + 1) / (SA3_3D_BITS + 1)
		    >> YM_EQ_REDUCE_BIT;
		strcpy(dip->un.v.units.name, AudioNbass);
		break;

	case YM_MASTER_WIDE:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = YM_EQ_CLASS;
		strcpy(dip->label.name, AudioNsurround);
		dip->un.v.num_channels = 2;
		dip->un.v.delta = (AUDIO_MAX_GAIN + 1) / (SA3_3D_BITS + 1);
		strcpy(dip->un.v.units.name, AudioNsurround);
		break;


#ifndef AUDIO_NO_POWER_CTL
	case YM_PWR_MODE:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = YM_PWR_CLASS;
		dip->next = YM_PWR_TIMEOUT;
		strcpy(dip->label.name, AudioNsave);
		dip->un.e.num_mem = 3;
		strcpy(dip->un.e.member[0].label.name, AudioNpowerdown);
		dip->un.e.member[0].ord = YM_POWER_POWERDOWN;
		strcpy(dip->un.e.member[1].label.name, AudioNpowersave);
		dip->un.e.member[1].ord = YM_POWER_POWERSAVE;
		strcpy(dip->un.e.member[2].label.name, AudioNnosave);
		dip->un.e.member[2].ord = YM_POWER_NOSAVE;
		break;

	case YM_PWR_TIMEOUT:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = YM_PWR_CLASS;
		dip->prev = YM_PWR_MODE;
		strcpy(dip->label.name, AudioNtimeout);
		dip->un.v.num_channels = 1;
		strcpy(dip->un.v.units.name, AudioNtimeout);
		break;
#endif /* not AUDIO_NO_POWER_CTL */

	default:
		return ENXIO;
		/*NOTREACHED*/
	}

	return 0;
}

int
ym_intr(void *arg)
{
	struct ym_softc *sc = arg;
#if NMPU_YM > 0
	struct mpu_softc *sc_mpu = device_private(sc->sc_mpudev);
#endif
	u_int8_t ist;
	int processed;

	mutex_spin_enter(&sc->sc_ad1848.sc_ad1848.sc_intr_lock);

	/* OPL3 timer is currently unused. */
	if (((ist = ym_read(sc, SA3_IRQA_STAT)) &
	     ~(SA3_IRQ_STAT_SB|SA3_IRQ_STAT_OPL3)) == 0) {
		DPRINTF(("%s: ym_intr: spurious interrupt\n", DVNAME(sc)));
		mutex_spin_exit(&sc->sc_ad1848.sc_ad1848.sc_intr_lock);
		return 0;
	}

	/* Process pending interrupts. */
	do {
		processed = 0;
		/*
		 * CODEC interrupts.
		 */
		if (ist & (SA3_IRQ_STAT_TI|SA3_IRQ_STAT_CI|SA3_IRQ_STAT_PI)) {
			ad1848_isa_intr(&sc->sc_ad1848);
			processed = 1;
		}
#if NMPU_YM > 0
		/*
		 * MPU401 interrupt.
		 */
		if (ist & SA3_IRQ_STAT_MPU) {
			mpu_intr(sc_mpu);
			processed = 1;
		}
#endif
		/*
		 * Hardware volume interrupt (SA3 only).
		 * Recalculate master volume from the hardware setting.
		 */
		if ((ist & SA3_IRQ_STAT_MV) && YM_IS_SA3(sc)) {
			ym_hvol_to_master_gain(sc);
			processed = 1;
		}
	} while (processed && (ist = ym_read(sc, SA3_IRQA_STAT)));

	mutex_spin_exit(&sc->sc_ad1848.sc_ad1848.sc_intr_lock);
	return 1;
}


#ifndef AUDIO_NO_POWER_CTL
static void
ym_save_codec_regs(struct ym_softc *sc)
{
	struct ad1848_softc *ac;
	int i;

	DPRINTF(("%s: ym_save_codec_regs\n", DVNAME(sc)));
	ac = &sc->sc_ad1848.sc_ad1848;
	for (i = 0; i <= 0x1f; i++)
		sc->sc_codec_scan[i] = ad_read(ac, i);
}

static void
ym_restore_codec_regs(struct ym_softc *sc)
{
	struct ad1848_softc *ac;
	int i, t;

	DPRINTF(("%s: ym_restore_codec_regs\n", DVNAME(sc)));
	ac = &sc->sc_ad1848.sc_ad1848;
	for (i = 0; i <= 0x1f; i++) {
		/*
		 * Wait til the chip becomes ready.
		 * This is required after suspend/resume.
		 */
		for (t = 0;
		    t < 100000 && ADREAD(ac, AD1848_IADDR) & SP_IN_INIT; t++)
			;
#ifdef AUDIO_DEBUG
		if (t)
			DPRINTF(("%s: ym_restore_codec_regs: reg %d, t %d\n",
				 DVNAME(sc), i, t));
#endif
		ad_write(ac, i, sc->sc_codec_scan[i]);
	}
}

/*
 * Save and restore the state on suspending / resumning.
 *
 * XXX This is not complete.
 * Currently only the parameters, such as output gain, are restored.
 * DMA state should also be restored.  FIXME.
 */
static bool
ym_suspend(device_t self, const pmf_qual_t *qual)
{
	struct ym_softc *sc = device_private(self);

	DPRINTF(("%s: ym_power_hook: suspend\n", DVNAME(sc)));

	mutex_spin_enter(&sc->sc_ad1848.sc_ad1848.sc_intr_lock);

	/*
	 * suspending...
	 */
	callout_halt(&sc->sc_powerdown_ch,
	    &sc->sc_ad1848.sc_ad1848.sc_intr_lock);
	if (sc->sc_turning_off)
		ym_powerdown_blocks(sc);

	/*
	 * Save CODEC registers.
	 * Note that the registers read incorrect
	 * if the CODEC part is in power-down mode.
	 */
	if (sc->sc_on_blocks & YM_POWER_CODEC_DIGITAL)
		ym_save_codec_regs(sc);

	/*
	 * Save OPL3-SA3 control registers and power-down the chip.
	 * Note that the registers read incorrect
	 * if the chip is in global power-down mode.
	 */
	sc->sc_sa3_scan[SA3_PWR_MNG] = ym_read(sc, SA3_PWR_MNG);
	if (sc->sc_on_blocks)
		ym_chip_powerdown(sc);
	mutex_spin_exit(&sc->sc_ad1848.sc_ad1848.sc_intr_lock);
	return true;
}

static bool
ym_resume(device_t self, const pmf_qual_t *qual)
{
	struct ym_softc *sc = device_private(self);
	int i, xmax;

	DPRINTF(("%s: ym_power_hook: resume\n", DVNAME(sc)));

	mutex_spin_enter(&sc->sc_ad1848.sc_ad1848.sc_intr_lock);
	/*
	 * resuming...
	 */
	ym_chip_powerup(sc, 1);
	ym_init(sc);		/* power-on CODEC */

	/* Restore control registers. */
	xmax = YM_IS_SA3(sc)? YM_SAVE_REG_MAX_SA3 : YM_SAVE_REG_MAX_SA2;
	for (i = SA3_PWR_MNG + 1; i <= xmax; i++) {
		if (i == SA3_SB_SCAN || i == SA3_SB_SCAN_DATA ||
		    i == SA3_DPWRDWN)
			continue;
		ym_write(sc, i, sc->sc_sa3_scan[i]);
	}

	/* Restore CODEC registers (including mixer). */
	ym_restore_codec_regs(sc);

	/* Restore global/digital power-down state. */
	ym_write(sc, SA3_PWR_MNG, sc->sc_sa3_scan[SA3_PWR_MNG]);
	if (YM_IS_SA3(sc))
		ym_write(sc, SA3_DPWRDWN, sc->sc_sa3_scan[SA3_DPWRDWN]);
	mutex_spin_exit(&sc->sc_ad1848.sc_ad1848.sc_intr_lock);
	return true;
}

int
ym_codec_power_ctl(void *arg, int flags)
{
	struct ym_softc *sc;
	struct ad1848_softc *ac;
	int parts;

	sc = arg;
	ac = &sc->sc_ad1848.sc_ad1848;
	DPRINTF(("%s: ym_codec_power_ctl: flags = 0x%x\n", DVNAME(sc), flags));
	KASSERT(mutex_owned(&ac->sc_intr_lock));

	if (flags != 0) {
		parts = 0;
		if (flags & FREAD) {
			parts |= YM_POWER_CODEC_R | YM_POWER_CODEC_AD;
			if (ac->mute[AD1848_MONITOR_CHANNEL] == 0)
				parts |= YM_POWER_CODEC_P | YM_POWER_CODEC_DA;
		}
		if (flags & FWRITE)
			parts |= YM_POWER_CODEC_P | YM_POWER_CODEC_DA;
	} else
		parts = YM_POWER_CODEC_P | YM_POWER_CODEC_R |
			YM_POWER_CODEC_DA | YM_POWER_CODEC_AD;

	ym_power_ctl(sc, parts, flags);

	return 0;
}

/*
 * Enter Power Save mode or Global Power Down mode.
 * Total dissipation becomes 5mA and 10uA (typ.) respective.
 */
static void
ym_chip_powerdown(struct ym_softc *sc)
{
	int i, xmax;

	DPRINTF(("%s: ym_chip_powerdown\n", DVNAME(sc)));
	KASSERT(mutex_owned(&sc->sc_ad1848.sc_ad1848.sc_intr_lock));

	xmax = YM_IS_SA3(sc) ? YM_SAVE_REG_MAX_SA3 : YM_SAVE_REG_MAX_SA2;

	/* Save control registers. */
	for (i = SA3_PWR_MNG + 1; i <= xmax; i++) {
		if (i == SA3_SB_SCAN || i == SA3_SB_SCAN_DATA)
			continue;
		sc->sc_sa3_scan[i] = ym_read(sc, i);
	}
	ym_write(sc, SA3_PWR_MNG,
		 (sc->sc_pow_mode == YM_POWER_POWERDOWN ?
			SA3_PWR_MNG_PDN : SA3_PWR_MNG_PSV) | SA3_PWR_MNG_PDX);
}

/*
 * Power up from Power Save / Global Power Down Mode.
 */
static void
ym_chip_powerup(struct ym_softc *sc, int nosleep)
{
	uint8_t pw;

	DPRINTF(("%s: ym_chip_powerup\n", DVNAME(sc)));
	KASSERT(mutex_owned(&sc->sc_ad1848.sc_ad1848.sc_intr_lock));

	pw = ym_read(sc, SA3_PWR_MNG);

	if ((pw & (SA3_PWR_MNG_PSV | SA3_PWR_MNG_PDN | SA3_PWR_MNG_PDX)) == 0)
		return;		/* already on */

	pw &= ~SA3_PWR_MNG_PDX;
	ym_write(sc, SA3_PWR_MNG, pw);

	/* wait 100 ms */
	if (nosleep)
		delay(100000);
	else
		kpause("ym_pu1", false, hz / 10, 
		    &sc->sc_ad1848.sc_ad1848.sc_intr_lock);

	pw &= ~(SA3_PWR_MNG_PSV | SA3_PWR_MNG_PDN);
	ym_write(sc, SA3_PWR_MNG, pw);

	/* wait 70 ms */
	if (nosleep)
		delay(70000);
	else
		kpause("ym_pu1", false, hz / 10, 
		    &sc->sc_ad1848.sc_ad1848.sc_intr_lock);

	/* The chip is muted automatically --- unmute it now. */
	ym_mute(sc, SA3_VOL_L, sc->master_mute);
	ym_mute(sc, SA3_VOL_R, sc->master_mute);
}

/* callout handler for power-down */
static void
ym_powerdown_callout(void *arg)
{
	struct ym_softc *sc;

	sc = arg;

	mutex_spin_enter(&sc->sc_ad1848.sc_ad1848.sc_intr_lock);
	if ((sc->sc_in_power_ctl & YM_POWER_CTL_INUSE) == 0) {
		ym_powerdown_blocks(sc);
	}
	mutex_spin_exit(&sc->sc_ad1848.sc_ad1848.sc_intr_lock);
}

static void
ym_powerdown_blocks(struct ym_softc *sc)
{
	uint16_t parts;
	uint16_t on_blocks;
	uint8_t sv;

	on_blocks = sc->sc_on_blocks;
	DPRINTF(("%s: ym_powerdown_blocks: turning_off 0x%x\n",
		DVNAME(sc), sc->sc_turning_off));
	KASSERT(mutex_owned(&sc->sc_ad1848.sc_ad1848.sc_intr_lock));

	on_blocks = sc->sc_on_blocks;

	/* Be sure not to change the state of the chip.  Save it first. */
	sv =  bus_space_read_1(sc->sc_iot, sc->sc_controlioh, SA3_CTL_INDEX);

	parts = sc->sc_turning_off;

	if (on_blocks & ~parts & YM_POWER_CODEC_CTL)
		parts &= ~(YM_POWER_CODEC_P | YM_POWER_CODEC_R);
	if (parts & YM_POWER_CODEC_CTL) {
		if ((on_blocks & YM_POWER_CODEC_P) == 0)
			parts |= YM_POWER_CODEC_P;
		if ((on_blocks & YM_POWER_CODEC_R) == 0)
			parts |= YM_POWER_CODEC_R;
	}
	parts &= ~YM_POWER_CODEC_PSEUDO;

	/* If CODEC is being off, save the state. */
	if ((sc->sc_on_blocks & YM_POWER_CODEC_DIGITAL) &&
	    (sc->sc_on_blocks & ~sc->sc_turning_off &
				YM_POWER_CODEC_DIGITAL) == 0)
		ym_save_codec_regs(sc);

	if (YM_IS_SA3(sc)) {
		/* OPL3-SA3 */
		ym_write(sc, SA3_DPWRDWN,
		    ym_read(sc, SA3_DPWRDWN) | (u_int8_t) parts);
		ym_write(sc, SA3_APWRDWN,
		    ym_read(sc, SA3_APWRDWN) | (parts >> 8));
	} else {
		/* OPL3-SA2 (only OPL3 can be off partially) */
		if (parts & YM_POWER_OPL3)
			ym_write(sc, SA3_PWR_MNG,
			    ym_read(sc, SA3_PWR_MNG) | SA2_PWR_MNG_FMPS);
	}

	if (((sc->sc_on_blocks &= ~sc->sc_turning_off) & YM_POWER_ACTIVE) == 0)
		ym_chip_powerdown(sc);

	sc->sc_turning_off = 0;

	/* Restore the state of the chip. */
	bus_space_write_1(sc->sc_iot, sc->sc_controlioh, SA3_CTL_INDEX, sv);
}

/*
 * Power control entry point.
 */
void
ym_power_ctl(struct ym_softc *sc, int parts, int onoff)
{
	int need_restore_codec;

	KASSERT(mutex_owned(&sc->sc_ad1848.sc_ad1848.sc_intr_lock));

	DPRINTF(("%s: ym_power_ctl: parts = 0x%x, %s\n",
		DVNAME(sc), parts, onoff ? "on" : "off"));

	/* This function may sleep --- needs locking. */
	while (sc->sc_in_power_ctl & YM_POWER_CTL_INUSE) {
		sc->sc_in_power_ctl |= YM_POWER_CTL_WANTED;
		DPRINTF(("%s: ym_power_ctl: sleeping\n", DVNAME(sc)));
		cv_wait(&sc->sc_cv, &sc->sc_ad1848.sc_ad1848.sc_intr_lock);
		DPRINTF(("%s: ym_power_ctl: awaken\n", DVNAME(sc)));
	}
	sc->sc_in_power_ctl |= YM_POWER_CTL_INUSE;

	/* If ON requested to parts which are scheduled to OFF, cancel it. */
	if (onoff && sc->sc_turning_off && (sc->sc_turning_off &= ~parts) == 0)
		callout_halt(&sc->sc_powerdown_ch,
		    &sc->sc_ad1848.sc_ad1848.sc_intr_lock);

	if (!onoff && sc->sc_turning_off)
		parts &= ~sc->sc_turning_off;

	/* Discard bits which are currently {on,off}. */
	parts &= onoff ? ~sc->sc_on_blocks : sc->sc_on_blocks;

	/* Cancel previous timeout if needed. */
	if (parts != 0 && sc->sc_turning_off)
		callout_halt(&sc->sc_powerdown_ch,
		    &sc->sc_ad1848.sc_ad1848.sc_intr_lock);

	if (parts == 0)
		goto unlock;		/* no work to do */

	if (onoff) {
		/* Turning on is done immediately. */

		/* If the chip is off, turn it on. */
		if ((sc->sc_on_blocks & YM_POWER_ACTIVE) == 0)
			ym_chip_powerup(sc, 0);

		need_restore_codec = (parts & YM_POWER_CODEC_DIGITAL) &&
		    (sc->sc_on_blocks & YM_POWER_CODEC_DIGITAL) == 0;

		sc->sc_on_blocks |= parts;
		if (parts & YM_POWER_CODEC_CTL)
			parts |= YM_POWER_CODEC_P | YM_POWER_CODEC_R;

		if (YM_IS_SA3(sc)) {
			/* OPL3-SA3 */
			ym_write(sc, SA3_DPWRDWN,
			    ym_read(sc, SA3_DPWRDWN) & (u_int8_t)~parts);
			ym_write(sc, SA3_APWRDWN,
			    ym_read(sc, SA3_APWRDWN) & ~(parts >> 8));
		} else {
			/* OPL3-SA2 (only OPL3 can be off partially) */
			if (parts & YM_POWER_OPL3)
				ym_write(sc, SA3_PWR_MNG,
				    ym_read(sc, SA3_PWR_MNG)
					& ~SA2_PWR_MNG_FMPS);
		}
		if (need_restore_codec)
			ym_restore_codec_regs(sc);
	} else {
		/* Turning off is delayed. */
		sc->sc_turning_off |= parts;
	}

	/* Schedule turning off. */
	if (sc->sc_pow_mode != YM_POWER_NOSAVE && sc->sc_turning_off)
		callout_reset(&sc->sc_powerdown_ch, hz * sc->sc_pow_timeout,
		    ym_powerdown_callout, sc);

unlock:
	if (sc->sc_in_power_ctl & YM_POWER_CTL_WANTED)
		cv_broadcast(&sc->sc_cv);
	sc->sc_in_power_ctl = 0;
}
#endif /* not AUDIO_NO_POWER_CTL */

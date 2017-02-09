/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Fleischer <paul@xpg.dk>
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

#include <sys/param.h>
#include <sys/device.h>
#include <sys/audioio.h>
#include <sys/fcntl.h>

#include <dev/audio_if.h>

#include <dev/ic/uda1341var.h>
#include <dev/ic/uda1341reg.h>

/*#define UDA1341_DEBUG*/

#ifdef UDA1341_DEBUG
#define DPRINTF(x) do {printf x; } while (/*CONSTCOND*/0)
#else
#define DPRINTF(s) do {} while (/*CONSTCOND*/0)
#endif

const struct audio_format uda1341_formats[UDA1341_NFORMATS] =
{
	{NULL, AUMODE_PLAY|AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 8, 8, 2,
	 AUFMT_STEREO, 0, {8000, 48000}
	},
	{NULL, AUMODE_PLAY|AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16, 2,
	 AUFMT_STEREO, 0, {8000, 48000}
	},
	{NULL, AUMODE_PLAY|AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 8, 8, 2,
	 AUFMT_STEREO, 0, {8000, 48000}
	},
	{NULL, AUMODE_PLAY|AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 16, 16, 2,
	 AUFMT_STEREO, 0, {8000, 48000}
	},
};

static void uda1341_update_sound_settings(struct uda1341_softc *sc);


int
uda1341_attach(struct uda1341_softc *sc)
{
	sc->sc_system_clock = UDA1341_CLOCK_NA;
	sc->sc_l3_write = NULL;
	sc->sc_volume = 127;
	sc->sc_bass = 0;
	sc->sc_treble = 0;
	sc->sc_mode = 0;
	sc->sc_mute = 0;
	sc->sc_ogain = 0;
	sc->sc_deemphasis = UDA1341_DEEMPHASIS_AUTO;
	sc->sc_dac_power = 0;
	sc->sc_adc_power = 0;
	sc->sc_inmix1 = 0;
	sc->sc_inmix2 = 0;
	sc->sc_micvol = 0;
	sc->sc_inmode = 0;
	sc->sc_agc = 0;
	sc->sc_agc_lvl = 0;
	sc->sc_ch2_gain = 0;

	return 0;
}

int
uda1341_query_encodings(void *handle, audio_encoding_t *ae)
{
	switch(ae->index) {
	case 0:
		strlcpy(ae->name, AudioEmulaw, sizeof(ae->name));
		ae->encoding = AUDIO_ENCODING_ULAW;
		ae->precision = 8;
		ae->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 1:
		strlcpy(ae->name, AudioEslinear_le, sizeof(ae->name));
		ae->encoding = AUDIO_ENCODING_SLINEAR_LE;
		ae->precision = 8;
		ae->flags = 0;
		break;
	case 2:
		strlcpy(ae->name, AudioEslinear_le, sizeof(ae->name));
		ae->encoding = AUDIO_ENCODING_SLINEAR_LE;
		ae->precision = 16;
		ae->flags = 0;
		break;
	case 3:
		strlcpy(ae->name, AudioEulinear_le, sizeof(ae->name));
		ae->encoding = AUDIO_ENCODING_ULINEAR_LE;
		ae->precision = 8;
		ae->flags = 0;
		break;
	case 4:
		strlcpy(ae->name, AudioEulinear_le, sizeof(ae->name));
		ae->encoding = AUDIO_ENCODING_ULINEAR_LE;
		ae->precision = 16;
		ae->flags = 0;
		break;

	default:
		return EINVAL;
	}

	return 0;
}

int
uda1341_open(void *handle, int flags)
{
	struct uda1341_softc *sc = handle;

	/* Reset the UDA1341 */
	sc->sc_l3_write(sc, 0, UDA1341_L3_ADDR_DEVICE |
			UDA1341_L3_ADDR_STATUS);
	sc->sc_l3_write(sc, 1,
			UDA1341_L3_STATUS0 |
			UDA1341_L3_STATUS0_RST);

	if (flags & FREAD) {
		sc->sc_adc_power = 1;
	}
	if (flags & FWRITE) {
		sc->sc_dac_power = 1;
	}

#if 0
	/* Power on DAC */
	sc->sc_l3_write(sc, 1,
			UDA1341_L3_STATUS1 | UDA1341_L3_STATUS1_PC_DAC);
#endif
	uda1341_update_sound_settings(sc);

#if 0
	/* TODO: Add mixer support */
	sc->sc_l3_write(sc, 0, 0x14 | 0x0);
	sc->sc_l3_write(sc, 1, 0x15);  /* Volume */
#endif

	return 0;
}

void
uda1341_close(void *handle)
{
	struct uda1341_softc *sc = handle;
	/* Reset the UDA1341 */
	sc->sc_l3_write(sc, 0, UDA1341_L3_ADDR_DEVICE |
			UDA1341_L3_ADDR_STATUS);

	/* Power off DAC and ADC*/
	sc->sc_l3_write(sc, 1,
			UDA1341_L3_STATUS1);

	sc->sc_dac_power = 0;
	sc->sc_adc_power = 0;
}

int
uda1341_set_params(void *handle, int setmode, int usemode,
		   audio_params_t *play, audio_params_t *rec,
		   stream_filter_list_t *pfil, stream_filter_list_t *rfil)
{
	struct uda1341_softc *sc = handle;
	if (sc->sc_system_clock == UDA1341_CLOCK_NA)
		panic("uda1341_set_params was called without sc_system_clock set!\n");

	/* Select status register */
	sc->sc_l3_write(sc, 0, UDA1341_L3_ADDR_DEVICE |
			UDA1341_L3_ADDR_STATUS);

	sc->sc_l3_write(sc, 1, UDA1341_L3_STATUS0 |
			sc->sc_system_clock << UDA1341_L3_STATUS0_SC_SHIFT |
			sc->sc_bus_format << UDA1341_L3_STATUS0_IF_SHIFT
			);

	if (sc->sc_sample_rate_approx != play->sample_rate) {
		sc->sc_sample_rate_approx = play->sample_rate;
		uda1341_update_sound_settings(sc);
	}

	return 0;
}

#define AUDIO_LEVELS	(AUDIO_MAX_GAIN-AUDIO_MIN_GAIN+1)
static void
uda1341_update_sound_settings(struct uda1341_softc *sc)
{
	/* TODO: Refactor this function into smaller parts, such that
	 * a volume change does not trigger updates of all the
	 * other -- unrelated -- registers.
	 */

	uint8_t val, volume, bass, treble, deemphasis;

	sc->sc_l3_write(sc, 0, UDA1341_L3_ADDR_DEVICE | UDA1341_L3_ADDR_STATUS);
	val = UDA1341_L3_STATUS1;
	if (sc->sc_dac_power)
		val |= UDA1341_L3_STATUS1_PC_DAC;
	if (sc->sc_adc_power)
		val |= UDA1341_L3_STATUS1_PC_ADC;
	if (sc->sc_ogain)
		val |= UDA1341_L3_STATUS1_OGS_6DB;

	sc->sc_l3_write(sc, 1, val);

	sc->sc_l3_write(sc, 0, UDA1341_L3_ADDR_DEVICE | UDA1341_L3_ADDR_DATA0);

	/* Update volume */
	/* On the UDA1341 maximal volume is 0x0,
	   while minimal volume is 0x3f */
	volume = (0x3f) - ((sc->sc_volume*(0x3f+1)) / (AUDIO_LEVELS));

	val = UDA1341_L3_DATA0_VOLUME;
	val |= volume & UDA1341_L3_DATA0_VOLUME_MASK;
	sc->sc_l3_write(sc, 1, val);

	/* Update bass and treble */
	bass = (sc->sc_bass*(0xf+1)) / AUDIO_LEVELS;
	treble = (sc->sc_treble*(0x3+1)) / AUDIO_LEVELS;
	val = UDA1341_L3_DATA0_BASS_TREBLE;
	val |= (bass << UDA1341_L3_DATA0_BASS_SHIFT) &
		UDA1341_L3_DATA0_BASS_MASK;
	val |= (treble << UDA1341_L3_DATA0_TREBLE_SHIFT) &
		UDA1341_L3_DATA0_TREBLE_MASK;
	sc->sc_l3_write(sc, 1, val);

	/* Update the remaining output sound controls:
	 * - Peak-detect position
	 * - De-emphasis
	 * - Mute
	 * - Mode Switch
	 * XXX: Only Mode-switch, de-emphasis, and mute is currently supported.
	 */
	val = UDA1341_L3_DATA0_SOUNDC;

	deemphasis = sc->sc_deemphasis;
	if( deemphasis == UDA1341_DEEMPHASIS_AUTO) {
		/* Set deemphasis according to current sample rate */
		switch (sc->sc_sample_rate_approx) {
		case 32000:
			deemphasis = 0x1;
			break;
		case 44100:
			deemphasis = 0x2;
			break;
		case 48000:
			deemphasis = 0x3;
			break;
		default:
			deemphasis = 0x0;
		}
	}

	DPRINTF(("Deemphasis: %d\n", deemphasis));
	val |= (deemphasis << UDA1341_L3_DATA0_SOUNDC_DE_SHIFT) &
		UDA1341_L3_DATA0_SOUNDC_DE_MASK;

	if (sc->sc_mute)
		val |= UDA1341_L3_DATA0_SOUNDC_MUTE;
	val |= sc->sc_mode & UDA1341_L3_DATA0_SOUNDC_MODE_MASK;
	sc->sc_l3_write(sc, 1, val);

	/* Extended Register 0: MA */
	val = UDA1341_L3_DATA0_ED;
	val |= (sc->sc_inmix1 & UDA1341_L3_DATA0_MA_MASK);
	sc->sc_l3_write(sc, 1, UDA1341_L3_DATA0_EA | 0x0);
	sc->sc_l3_write(sc, 1, val);

	/* Extended Register 1: MB */
	val = UDA1341_L3_DATA0_ED;
	val |= (sc->sc_inmix2 & UDA1341_L3_DATA0_MB_MASK);
	sc->sc_l3_write(sc, 1, UDA1341_L3_DATA0_EA | 0x01);
	sc->sc_l3_write(sc, 1, val);

	/* Extended Register 2: MIC sensitivity and mixer mode  */
	val = UDA1341_L3_DATA0_ED;
	val |= (sc->sc_micvol << UDA1341_L3_DATA0_MS_SHIFT) &
		UDA1341_L3_DATA0_MS_MASK;
	val |= sc->sc_inmode & UDA1341_L3_DATA0_MM_MASK;
	sc->sc_l3_write(sc, 1, UDA1341_L3_DATA0_EA | 0x02);
	sc->sc_l3_write(sc, 1, val);

	/* Extended Register 4: AGC and IG (ch2_gain) */
	val = UDA1341_L3_DATA0_ED;

	val |= (sc->sc_agc << UDA1341_L3_DATA0_AGC_SHIFT) &
		UDA1341_L3_DATA0_AGC_MASK;
	val |= (sc->sc_ch2_gain & 0x03) & UDA1341_L3_DATA0_IG_LOW_MASK;
	sc->sc_l3_write(sc, 1, UDA1341_L3_DATA0_EA | 0x04);
	sc->sc_l3_write(sc, 1, val);

	/* Extended Register 5: IG (ch2_gain) */
	val = UDA1341_L3_DATA0_ED;
	val |= (sc->sc_ch2_gain >> 2 ) & UDA1341_L3_DATA0_IG_HIGH_MASK;
	sc->sc_l3_write(sc, 1, UDA1341_L3_DATA0_EA | 0x05);
	sc->sc_l3_write(sc, 1, val);

	/* Extended Register 6: AT and AL */
	/* XXX: Only AL is supported at this point */
	val = UDA1341_L3_DATA0_ED;
	val |= sc->sc_agc_lvl & UDA1341_L3_DATA0_AL_MASK;
	sc->sc_l3_write(sc, 1, UDA1341_L3_DATA0_EA | 0x06);
	sc->sc_l3_write(sc, 1, val);
}

#define UDA1341_MIXER_VOL	0
#define UDA1341_MIXER_BASS	1
#define UDA1341_MIXER_TREBLE	2
#define UDA1341_MIXER_MODE	3
#define UDA1341_MIXER_MUTE	4
#define UDA1341_MIXER_OGAIN	5
#define UDA1341_MIXER_DE	6
#define UDA1341_OUTPUT_CLASS	7

#define UDA1341_MIXER_INMIX1	8
#define UDA1341_MIXER_INMIX2	9
#define UDA1341_MIXER_MICVOL	10
#define UDA1341_MIXER_INMODE	11
#define UDA1341_MIXER_AGC	12
#define UDA1341_MIXER_AGC_LVL	13
#define UDA1341_MIXER_IN_GAIN2	14
/*#define UDA1341_MIXER_AGC_SETTINGS 15*/
#define UDA1341_INPUT_CLASS	15

int
uda1341_query_devinfo(void *handle, mixer_devinfo_t *mi)
{

	switch(mi->index) {
	case UDA1341_MIXER_VOL:
		strlcpy(mi->label.name, AudioNspeaker,
			sizeof(mi->label.name));
		mi->type = AUDIO_MIXER_VALUE;
		mi->mixer_class = UDA1341_OUTPUT_CLASS;
		mi->next = UDA1341_MIXER_BASS;
		mi->prev = AUDIO_MIXER_LAST;
		strlcpy(mi->un.v.units.name, AudioNvolume,
			sizeof(mi->un.v.units.name));
		mi->un.v.num_channels = 1;
		mi->un.v.delta = 256/64;
		break;
	case UDA1341_MIXER_BASS:
		strlcpy(mi->label.name, AudioNbass,
			sizeof(mi->label.name));
		mi->type = AUDIO_MIXER_VALUE;
		mi->mixer_class = UDA1341_OUTPUT_CLASS;
		mi->next = UDA1341_MIXER_TREBLE;
		mi->prev = UDA1341_MIXER_VOL;
		strlcpy(mi->un.v.units.name, AudioNbass,
			sizeof(mi->un.v.units.name));
		mi->un.v.num_channels = 1;
		mi->un.v.delta = 256/16;
		break;
	case UDA1341_MIXER_TREBLE:
		strlcpy(mi->label.name, AudioNtreble,
			sizeof(mi->label.name));
		mi->type = AUDIO_MIXER_VALUE;
		mi->mixer_class = UDA1341_OUTPUT_CLASS;
		mi->next = UDA1341_MIXER_MODE;
		mi->prev = UDA1341_MIXER_BASS;
		strlcpy(mi->un.v.units.name, AudioNtreble,
			sizeof(mi->un.v.units.name));
		mi->un.v.num_channels = 1;
		mi->un.v.delta = 256/4;
		break;
	case UDA1341_MIXER_MODE:
		strlcpy(mi->label.name, AudioNmode,
			sizeof(mi->label.name));
		mi->type = AUDIO_MIXER_ENUM;
		mi->mixer_class = UDA1341_OUTPUT_CLASS;
		mi->next = UDA1341_MIXER_MUTE;
		mi->prev = UDA1341_MIXER_TREBLE;
		mi->un.e.num_mem = 3;

		strlcpy(mi->un.e.member[0].label.name,
			"flat", sizeof(mi->un.e.member[0].label.name));
		mi->un.e.member[0].ord = 0;

		strlcpy(mi->un.e.member[1].label.name,
			"minimum", sizeof(mi->un.e.member[1].label.name));
		mi->un.e.member[1].ord = 1;

		strlcpy(mi->un.e.member[2].label.name,
			"maximum", sizeof(mi->un.e.member[2].label.name));
		mi->un.e.member[2].ord = 3;

		break;
	case UDA1341_MIXER_MUTE:
		strlcpy(mi->label.name, AudioNmute,
			sizeof(mi->label.name));
		mi->type = AUDIO_MIXER_ENUM;
		mi->mixer_class = UDA1341_OUTPUT_CLASS;
		mi->next = UDA1341_MIXER_OGAIN;
		mi->prev = UDA1341_MIXER_MODE;
		mi->un.e.num_mem = 2;

		strlcpy(mi->un.e.member[0].label.name,
			"off", sizeof(mi->un.e.member[0].label.name));
		mi->un.e.member[0].ord = 0;

		strlcpy(mi->un.e.member[1].label.name,
			"on", sizeof(mi->un.e.member[1].label.name));
		mi->un.e.member[1].ord = 1;
		break;
	case UDA1341_MIXER_OGAIN:
		strlcpy(mi->label.name, "gain",
			sizeof(mi->label.name));
		mi->type = AUDIO_MIXER_ENUM;
		mi->mixer_class = UDA1341_OUTPUT_CLASS;
		mi->next = UDA1341_MIXER_DE;
		mi->prev = UDA1341_MIXER_MUTE;
		mi->un.e.num_mem = 2;

		strlcpy(mi->un.e.member[0].label.name,
			"off", sizeof(mi->un.e.member[0].label.name));
		mi->un.e.member[0].ord = 0;

		strlcpy(mi->un.e.member[1].label.name,
			"on", sizeof(mi->un.e.member[1].label.name));
		mi->un.e.member[1].ord = 1;
		break;
	case UDA1341_MIXER_DE:
		strlcpy(mi->label.name, "deemphasis",
			sizeof(mi->label.name));
		mi->type = AUDIO_MIXER_ENUM;
		mi->mixer_class = UDA1341_OUTPUT_CLASS;
		mi->next = AUDIO_MIXER_LAST;
		mi->prev = UDA1341_MIXER_OGAIN;
		mi->un.e.num_mem = 5;

		strlcpy(mi->un.e.member[0].label.name,
			"none", sizeof(mi->un.e.member[0].label.name));
		mi->un.e.member[0].ord = 0;

		strlcpy(mi->un.e.member[1].label.name,
			"32KHz", sizeof(mi->un.e.member[1].label.name));
		mi->un.e.member[1].ord = 1;

		strlcpy(mi->un.e.member[2].label.name,
			"44.1KHz", sizeof(mi->un.e.member[2].label.name));
		mi->un.e.member[2].ord = 2;

		strlcpy(mi->un.e.member[3].label.name,
			"48KHz", sizeof(mi->un.e.member[3].label.name));
		mi->un.e.member[3].ord = 3;

		strlcpy(mi->un.e.member[4].label.name,
			"auto", sizeof(mi->un.e.member[4].label.name));
		mi->un.e.member[4].ord = 4;

		break;
	case UDA1341_OUTPUT_CLASS:
		mi->type = AUDIO_MIXER_CLASS;
		mi->mixer_class = UDA1341_OUTPUT_CLASS;
		mi->prev = AUDIO_MIXER_LAST;
		mi->next = AUDIO_MIXER_LAST;
		strlcpy(mi->label.name, AudioCoutputs,
			sizeof(mi->label.name));
		break;
	case UDA1341_MIXER_INMIX1:
		strlcpy(mi->label.name, "inmix1",
			sizeof(mi->label.name));
		mi->type = AUDIO_MIXER_VALUE;
		mi->mixer_class = UDA1341_INPUT_CLASS;
		mi->next = AUDIO_MIXER_LAST;
		mi->prev = AUDIO_MIXER_LAST;
		strlcpy(mi->un.v.units.name, AudioNvolume,
			sizeof(mi->un.v.units.name));
		mi->un.v.num_channels = 1;
		mi->un.v.delta = 256/64;
		break;
	case UDA1341_MIXER_INMIX2:
		strlcpy(mi->label.name, "inmix2",
			sizeof(mi->label.name));
		mi->type = AUDIO_MIXER_VALUE;
		mi->mixer_class = UDA1341_INPUT_CLASS;
		mi->next = AUDIO_MIXER_LAST;
		mi->prev = AUDIO_MIXER_LAST;
		strlcpy(mi->un.v.units.name, AudioNvolume,
			sizeof(mi->un.v.units.name));
		mi->un.v.num_channels = 1;
		mi->un.v.delta = 256/64;
		break;
	case UDA1341_MIXER_MICVOL:
		strlcpy(mi->label.name, AudioNmicrophone,
			sizeof(mi->label.name));
		mi->type = AUDIO_MIXER_VALUE;
		mi->mixer_class = UDA1341_INPUT_CLASS;
		mi->next = AUDIO_MIXER_LAST;
		mi->prev = AUDIO_MIXER_LAST;
		strlcpy(mi->un.v.units.name, AudioNvolume,
			sizeof(mi->un.v.units.name));
		mi->un.v.num_channels = 1;
		mi->un.v.delta = 256/8;
		break;
	case UDA1341_MIXER_INMODE:
		strlcpy(mi->label.name, "inmode",
			sizeof(mi->label.name));
		mi->type = AUDIO_MIXER_ENUM;
		mi->mixer_class = UDA1341_INPUT_CLASS;
		mi->next = AUDIO_MIXER_LAST;
		mi->prev = AUDIO_MIXER_LAST;
		mi->un.e.num_mem = 4;

		strlcpy(mi->un.e.member[0].label.name,
			"dd", sizeof(mi->un.e.member[0].label.name));
		mi->un.e.member[0].ord = 0;

		strlcpy(mi->un.e.member[1].label.name,
			"ch1", sizeof(mi->un.e.member[1].label.name));
		mi->un.e.member[1].ord = 1;

		strlcpy(mi->un.e.member[2].label.name,
			"ch2", sizeof(mi->un.e.member[2].label.name));
		mi->un.e.member[2].ord = 2;

		strlcpy(mi->un.e.member[3].label.name,
			"mix", sizeof(mi->un.e.member[3].label.name));
		mi->un.e.member[3].ord = 3;
		break;
	case UDA1341_MIXER_AGC:
		strlcpy(mi->label.name, "agc",
			sizeof(mi->label.name));
		mi->type = AUDIO_MIXER_ENUM;
		mi->mixer_class = UDA1341_INPUT_CLASS;
		mi->next = AUDIO_MIXER_LAST;
		mi->prev = AUDIO_MIXER_LAST;
		mi->un.e.num_mem = 2;

		strlcpy(mi->un.e.member[0].label.name,
			"off", sizeof(mi->un.e.member[0].label.name));
		mi->un.e.member[0].ord = 0;

		strlcpy(mi->un.e.member[1].label.name,
			"on", sizeof(mi->un.e.member[1].label.name));
		mi->un.e.member[1].ord = 1;
		break;
	case UDA1341_MIXER_AGC_LVL:
		strlcpy(mi->label.name, "agclevel",
			sizeof(mi->label.name));
		mi->type = AUDIO_MIXER_VALUE;
		mi->mixer_class = UDA1341_INPUT_CLASS;
		mi->next = AUDIO_MIXER_LAST;
		mi->prev = AUDIO_MIXER_LAST;
		strlcpy(mi->un.v.units.name, AudioNvolume,
			sizeof(mi->un.v.units.name));
		mi->un.v.num_channels = 1;
		mi->un.v.delta = 256/4;
		break;
	case UDA1341_MIXER_IN_GAIN2:
		strlcpy(mi->label.name, "ch2gain",
			sizeof(mi->label.name));
		mi->type = AUDIO_MIXER_VALUE;
		mi->mixer_class = UDA1341_INPUT_CLASS;
		mi->next = AUDIO_MIXER_LAST;
		mi->prev = AUDIO_MIXER_LAST;
		strlcpy(mi->un.v.units.name, AudioNvolume,
			sizeof(mi->un.v.units.name));
		mi->un.v.num_channels = 1;
		mi->un.v.delta = 256/128;
		break;
	case UDA1341_INPUT_CLASS:
		mi->type = AUDIO_MIXER_CLASS;
		mi->mixer_class = UDA1341_INPUT_CLASS;
		mi->prev = AUDIO_MIXER_LAST;
		mi->next = AUDIO_MIXER_LAST;
		strlcpy(mi->label.name, AudioCinputs,
			sizeof(mi->label.name));
		break;
	default:
		return ENXIO;
	}

	return 0;
}

int
uda1341_get_port(void *handle, mixer_ctrl_t *mixer)
{
	struct uda1341_softc *sc = handle;

	switch(mixer->dev) {
	case UDA1341_MIXER_VOL:
		if (mixer->type != AUDIO_MIXER_VALUE)
			return EINVAL;
		if (mixer->un.value.num_channels != 1)
			return EINVAL;
		mixer->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			sc->sc_volume;
		break;
	case UDA1341_MIXER_BASS:
		if (mixer->type != AUDIO_MIXER_VALUE ||
		    mixer->un.value.num_channels != 1)
			return EINVAL;

		mixer->un.value.level[0] = sc->sc_bass;
		break;
	case UDA1341_MIXER_TREBLE:
		if (mixer->type != AUDIO_MIXER_VALUE ||
		    mixer->un.value.num_channels != 1)
			return EINVAL;

		mixer->un.value.level[0] = sc->sc_treble;
		break;
	case UDA1341_MIXER_MODE:
		if (mixer->type != AUDIO_MIXER_ENUM)
			return EINVAL;

		mixer->un.ord = sc->sc_mode;
		break;
	case UDA1341_MIXER_MUTE:
		if (mixer->type != AUDIO_MIXER_ENUM)
			return EINVAL;

		mixer->un.ord = sc->sc_mute;
		break;
	case UDA1341_MIXER_OGAIN:
		if (mixer->type != AUDIO_MIXER_ENUM)
			return EINVAL;

		mixer->un.ord = sc->sc_ogain;
		break;
	case UDA1341_MIXER_DE:
		if (mixer->type != AUDIO_MIXER_ENUM)
			return EINVAL;

		mixer->un.ord = sc->sc_deemphasis;
		break;
	case UDA1341_MIXER_INMIX1:
		if (mixer->type != AUDIO_MIXER_VALUE)
			return EINVAL;

		mixer->un.value.level[0] = sc->sc_inmix1;
		break;
	case UDA1341_MIXER_INMIX2:
		if (mixer->type != AUDIO_MIXER_VALUE)
			return EINVAL;

		mixer->un.value.level[0] = sc->sc_inmix2;
		break;
	case UDA1341_MIXER_MICVOL:
		if (mixer->type != AUDIO_MIXER_VALUE)
			return EINVAL;

		mixer->un.value.level[0] = sc->sc_micvol;
		break;
	case UDA1341_MIXER_INMODE:
		if (mixer->type != AUDIO_MIXER_ENUM)
			return EINVAL;

		mixer->un.ord =	sc->sc_inmode;
		break;
	case UDA1341_MIXER_AGC:
		if (mixer->type != AUDIO_MIXER_ENUM)
			return EINVAL;

		mixer->un.ord =	sc->sc_agc;
		break;
	case UDA1341_MIXER_AGC_LVL:
		if (mixer->type != AUDIO_MIXER_VALUE)
			return EINVAL;

		mixer->un.value.level[0] = sc->sc_agc_lvl;
		break;
	case UDA1341_MIXER_IN_GAIN2:
		if (mixer->type != AUDIO_MIXER_VALUE)
			return EINVAL;

		mixer->un.value.level[0] = sc->sc_ch2_gain;
		break;
	default:
		return EINVAL;
	}

	return 0;
}

int
uda1341_set_port(void *handle, mixer_ctrl_t *mixer)
{
	struct uda1341_softc *sc = handle;

	switch(mixer->dev) {
	case UDA1341_MIXER_VOL:
		sc->sc_volume = mixer->un.value.level[0];
		break;
	case UDA1341_MIXER_BASS:
		sc->sc_bass = mixer->un.value.level[0];
		break;
	case UDA1341_MIXER_TREBLE:
		sc->sc_treble = mixer->un.value.level[0];
		break;
	case UDA1341_MIXER_MODE:
		sc->sc_mode = mixer->un.ord;
		break;
	case UDA1341_MIXER_MUTE:
		sc->sc_mute = mixer->un.ord;
		break;
	case UDA1341_MIXER_OGAIN:
		sc->sc_ogain = mixer->un.ord;
		break;
	case UDA1341_MIXER_DE:
		sc->sc_deemphasis = mixer->un.ord;
		break;
	case UDA1341_MIXER_INMIX1:
		sc->sc_inmix1 = mixer->un.value.level[0];
		break;
	case UDA1341_MIXER_INMIX2:
		sc->sc_inmix2 = mixer->un.value.level[0];
		break;
	case UDA1341_MIXER_MICVOL:
		sc->sc_micvol = mixer->un.value.level[0];
		break;
	case UDA1341_MIXER_INMODE:
		sc->sc_inmode = mixer->un.ord;
		break;
	case UDA1341_MIXER_AGC:
		sc->sc_agc = mixer->un.ord;
		break;
	case UDA1341_MIXER_AGC_LVL:
		sc->sc_agc_lvl = mixer->un.value.level[0];
		break;
	case UDA1341_MIXER_IN_GAIN2:
		sc->sc_ch2_gain = mixer->un.value.level[0];
		break;
	default:
		return EINVAL;
	}

	uda1341_update_sound_settings(sc);

	return 0;
}

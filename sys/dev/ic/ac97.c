/*      $NetBSD: ac97.c,v 1.96 2015/04/04 15:09:45 christos Exp $ */
/*	$OpenBSD: ac97.c,v 1.8 2000/07/19 09:01:35 csapuntz Exp $	*/

/*
 * Copyright (c) 1999, 2000 Constantine Sapuntzakis
 *
 * Author:        Constantine Sapuntzakis <csapuntz@stanford.edu>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE
 */

/* Partially inspired by FreeBSD's sys/dev/pcm/ac97.c. It came with
   the following copyright */

/*
 * Copyright (c) 1999 Cameron Grant <gandalf@vilnya.demon.co.uk>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ac97.c,v 1.96 2015/04/04 15:09:45 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/sysctl.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/ic/ac97reg.h>
#include <dev/ic/ac97var.h>

struct ac97_softc;
struct ac97_source_info;
static int	ac97_mixer_get_port(struct ac97_codec_if *, mixer_ctrl_t *);
static int	ac97_mixer_set_port(struct ac97_codec_if *, mixer_ctrl_t *);
static void	ac97_detach(struct ac97_codec_if *);
static void	ac97_lock(struct ac97_codec_if *);
static void	ac97_unlock(struct ac97_codec_if *);
static int	ac97_query_devinfo(struct ac97_codec_if *, mixer_devinfo_t *);
static int	ac97_get_portnum_by_name(struct ac97_codec_if *, const char *,
					 const char *, const char *);
static void	ac97_restore_shadow(struct ac97_codec_if *);
static int	ac97_set_rate(struct ac97_codec_if *, int, u_int *);
static void	ac97_set_clock(struct ac97_codec_if *, unsigned int);
static uint16_t ac97_get_extcaps(struct ac97_codec_if *);
static int	ac97_add_port(struct ac97_softc *,
			      const struct ac97_source_info *);
static int	ac97_str_equal(const char *, const char *);
static int	ac97_check_capability(struct ac97_softc *, int);
static void	ac97_setup_source_info(struct ac97_softc *);
static void	ac97_read(struct ac97_softc *, uint8_t, uint16_t *);
static void	ac97_setup_defaults(struct ac97_softc *);
static int	ac97_write(struct ac97_softc *, uint8_t, uint16_t);

static void	ac97_ad198x_init(struct ac97_softc *);
static void	ac97_alc650_init(struct ac97_softc *);
static void	ac97_ucb1400_init(struct ac97_softc *);
static void	ac97_vt1616_init(struct ac97_softc *);

static int	ac97_modem_offhook_set(struct ac97_softc *, int, int);
static int	ac97_sysctl_verify(SYSCTLFN_ARGS);

#define Ac97Nphone	"phone"
#define Ac97Nline1	"line1"
#define Ac97Nline2	"line2"
#define Ac97Nhandset	"handset"

static const struct audio_mixer_enum
ac97_on_off = { 2, { { { AudioNoff, 0 } , 0 },
		     { { AudioNon, 0 }  , 1 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 }, } };

static const struct audio_mixer_enum
ac97_mic_select = { 2, { { { AudioNmicrophone "0", 0  }, 0 },
			 { { AudioNmicrophone "1", 0  }, 1 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 },
			 { { "", 0 }	, 0 }, } };

static const struct audio_mixer_enum
ac97_mono_select = { 2, { { { AudioNmixerout, 0  }, 0 },
			  { { AudioNmicrophone, 0  }, 1 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 },
			  { { "", 0 }	, 0 }, } };

static const struct audio_mixer_enum
ac97_source = { 8, { { { AudioNmicrophone, 0  } , 0 },
		     { { AudioNcd, 0 }, 1 },
		     { { AudioNvideo, 0 }, 2 },
		     { { AudioNaux, 0 }, 3 },
		     { { AudioNline, 0 }, 4 },
		     { { AudioNmixerout, 0 }, 5 },
		     { { AudioNmixerout AudioNmono, 0 }, 6 },
		     { { Ac97Nphone, 0 }, 7 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 },
		     { { "", 0 }	, 0 }, } };

/*
 * Due to different values for each source that uses these structures,
 * the ac97_query_devinfo function sets delta in mixer_devinfo_t using
 * ac97_source_info.bits.
 */
static const struct audio_mixer_value
ac97_volume_stereo = { { AudioNvolume, 0 }, 2, 0 };

static const struct audio_mixer_value
ac97_volume_mono = { { AudioNvolume, 0 }, 1, 0 };

#define WRAP(a)  &a, sizeof(a)

struct ac97_source_info {
	const char *class;
	const char *device;
	const char *qualifier;

	int  type;
	const void *info;
	int  info_size;

	uint8_t  reg;
	int32_t	 default_value;
	unsigned bits:3;
	unsigned ofs:4;
	unsigned mute:1;
	unsigned polarity:1;   /* Does 0 == MAX or MIN */
	unsigned checkbits:1;
	enum {
		CHECK_NONE = 0,
		CHECK_SURROUND,
		CHECK_CENTER,
		CHECK_LFE,
		CHECK_HEADPHONES,
		CHECK_TONE,
		CHECK_MIC,
		CHECK_LOUDNESS,
		CHECK_3D,
		CHECK_LINE1,
		CHECK_LINE2,
		CHECK_HANDSET,
		CHECK_SPDIF
	} req_feature;

	int  prev;
	int  next;
	int  mixer_class;
};

static const struct ac97_source_info audio_source_info[] = {
	{ AudioCinputs,		NULL,		NULL,
	  AUDIO_MIXER_CLASS,    NULL,		0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{ AudioCoutputs,	NULL,		0,
	  AUDIO_MIXER_CLASS,    NULL,		0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{ AudioCrecord,		NULL,		0,
	  AUDIO_MIXER_CLASS,    NULL,		0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	/* Stereo master volume*/
	{ AudioCoutputs,	AudioNmaster,	0,
	  AUDIO_MIXER_VALUE, WRAP(ac97_volume_stereo),
	  AC97_REG_MASTER_VOLUME, 0x8000, 5, 0, 1, 0, 1, 0, 0, 0, 0,
	},
	/* Mono volume */
	{ AudioCoutputs,	AudioNmono,	NULL,
	  AUDIO_MIXER_VALUE, WRAP(ac97_volume_mono),
	  AC97_REG_MASTER_VOLUME_MONO, 0x8000, 6, 0, 1, 0, 1, 0, 0, 0, 0,
	},
	{ AudioCoutputs,	AudioNmono,	AudioNsource,
	  AUDIO_MIXER_ENUM, WRAP(ac97_mono_select),
	  AC97_REG_GP, 0x0000, 1, 9, 0, 0, 0, 0, 0, 0, 0,
	},
	/* Headphone volume */
	{ AudioCoutputs,	AudioNheadphone, NULL,
	  AUDIO_MIXER_VALUE, WRAP(ac97_volume_stereo),
	  AC97_REG_HEADPHONE_VOLUME, 0x8000, 5, 0, 1, 0, 1, CHECK_HEADPHONES, 0, 0, 0,
	},
	/* Surround volume - logic hard coded for mute */
	{ AudioCoutputs,	AudioNsurround,	NULL,
	  AUDIO_MIXER_VALUE, WRAP(ac97_volume_stereo),
	  AC97_REG_SURR_MASTER, 0x8080, 5, 0, 1, 0, 1, CHECK_SURROUND, 0, 0, 0
	},
	/* Center volume*/
	{ AudioCoutputs,	AudioNcenter,	NULL,
	  AUDIO_MIXER_VALUE, WRAP(ac97_volume_mono),
	  AC97_REG_CENTER_LFE_MASTER, 0x8080, 5, 0, 0, 0, 1, CHECK_CENTER, 0, 0, 0
	},
	{ AudioCoutputs,	AudioNcenter,	AudioNmute,
	  AUDIO_MIXER_ENUM, WRAP(ac97_on_off),
	  AC97_REG_CENTER_LFE_MASTER, 0x8080, 1, 7, 0, 0, 0, CHECK_CENTER, 0, 0, 0
	},
	/* LFE volume*/
	{ AudioCoutputs,	AudioNlfe,	NULL,
	  AUDIO_MIXER_VALUE, WRAP(ac97_volume_mono),
	  AC97_REG_CENTER_LFE_MASTER, 0x8080, 5, 8, 0, 0, 1, CHECK_LFE, 0, 0, 0
	},
	{ AudioCoutputs,	AudioNlfe,	AudioNmute,
	  AUDIO_MIXER_ENUM, WRAP(ac97_on_off),
	  AC97_REG_CENTER_LFE_MASTER, 0x8080, 1, 15, 0, 0, 0, CHECK_LFE, 0, 0, 0
	},
	/* Tone - bass */
	{ AudioCoutputs,	AudioNbass,	NULL,
	  AUDIO_MIXER_VALUE, WRAP(ac97_volume_mono),
	  AC97_REG_MASTER_TONE, 0x0f0f, 4, 8, 0, 0, 0, CHECK_TONE, 0, 0, 0
	},
	/* Tone - treble */
	{ AudioCoutputs,	AudioNtreble,	NULL,
	  AUDIO_MIXER_VALUE, WRAP(ac97_volume_mono),
	  AC97_REG_MASTER_TONE, 0x0f0f, 4, 0, 0, 0, 0, CHECK_TONE, 0, 0, 0
	},
	/* PC Beep Volume */
	{ AudioCinputs,		AudioNspeaker,	NULL,
	  AUDIO_MIXER_VALUE, WRAP(ac97_volume_mono),
	  AC97_REG_PCBEEP_VOLUME, 0x0000, 4, 1, 1, 0, 0, 0, 0, 0, 0,
	},

	/* Phone */
	{ AudioCinputs,		Ac97Nphone,	NULL,
	  AUDIO_MIXER_VALUE, WRAP(ac97_volume_mono),
	  AC97_REG_PHONE_VOLUME, 0x8008, 5, 0, 1, 0, 0, 0, 0, 0, 0,
	},
	/* Mic Volume */
	{ AudioCinputs,		AudioNmicrophone, NULL,
	  AUDIO_MIXER_VALUE, WRAP(ac97_volume_mono),
	  AC97_REG_MIC_VOLUME, 0x8008, 5, 0, 1, 0, 0, 0, 0, 0, 0,
	},
	{ AudioCinputs,		AudioNmicrophone, AudioNpreamp,
	  AUDIO_MIXER_ENUM, WRAP(ac97_on_off),
	  AC97_REG_MIC_VOLUME, 0x8008, 1, 6, 0, 0, 0, 0, 0, 0, 0,
	},
	{ AudioCinputs,		AudioNmicrophone, AudioNsource,
	  AUDIO_MIXER_ENUM, WRAP(ac97_mic_select),
	  AC97_REG_GP, 0x0000, 1, 8, 0, 0, 0, 0, 0, 0, 0,
	},
	/* Line in Volume */
	{ AudioCinputs,		AudioNline,	NULL,
	  AUDIO_MIXER_VALUE, WRAP(ac97_volume_stereo),
	  AC97_REG_LINEIN_VOLUME, 0x8808, 5, 0, 1, 0, 0, 0, 0, 0, 0,
	},
	/* CD Volume */
	{ AudioCinputs,		AudioNcd,	NULL,
	  AUDIO_MIXER_VALUE, WRAP(ac97_volume_stereo),
	  AC97_REG_CD_VOLUME, 0x8808, 5, 0, 1, 0, 0, 0, 0, 0, 0,
	},
	/* Video Volume */
	{ AudioCinputs,		AudioNvideo,	NULL,
	  AUDIO_MIXER_VALUE, WRAP(ac97_volume_stereo),
	  AC97_REG_VIDEO_VOLUME, 0x8808, 5, 0, 1, 0, 0, 0, 0, 0, 0,
	},
	/* AUX volume */
	{ AudioCinputs,		AudioNaux,	NULL,
	  AUDIO_MIXER_VALUE, WRAP(ac97_volume_stereo),
	  AC97_REG_AUX_VOLUME, 0x8808, 5, 0, 1, 0, 0, 0, 0, 0, 0,
	},
	/* PCM out volume */
	{ AudioCinputs,		AudioNdac,	NULL,
	  AUDIO_MIXER_VALUE, WRAP(ac97_volume_stereo),
	  AC97_REG_PCMOUT_VOLUME, 0x8808, 5, 0, 1, 0, 0, 0, 0, 0, 0,
	},
	/* Record Source - some logic for this is hard coded - see below */
	{ AudioCrecord,		AudioNsource,	NULL,
	  AUDIO_MIXER_ENUM, WRAP(ac97_source),
	  AC97_REG_RECORD_SELECT, 0x0000, 3, 0, 0, 0, 0, 0, 0, 0, 0,
	},
	/* Record Gain */
	{ AudioCrecord,		AudioNvolume,	NULL,
	  AUDIO_MIXER_VALUE, WRAP(ac97_volume_stereo),
	  AC97_REG_RECORD_GAIN, 0x8000, 4, 0, 1, 1, 0, 0, 0, 0, 0,
	},
	/* Record Gain mic */
	{ AudioCrecord,		AudioNmicrophone, NULL,
	  AUDIO_MIXER_VALUE, WRAP(ac97_volume_mono),
	  AC97_REG_RECORD_GAIN_MIC, 0x8000, 4, 0, 1, 1, 0, CHECK_MIC, 0, 0, 0
	},
	/* */
	{ AudioCoutputs,	AudioNloudness,	NULL,
	  AUDIO_MIXER_ENUM, WRAP(ac97_on_off),
	  AC97_REG_GP, 0x0000, 1, 12, 0, 0, 0, CHECK_LOUDNESS, 0, 0, 0
	},
	{ AudioCoutputs,	AudioNspatial,	NULL,
	  AUDIO_MIXER_ENUM, WRAP(ac97_on_off),
	  AC97_REG_GP, 0x0000, 1, 13, 0, 1, 0, CHECK_3D, 0, 0, 0
	},
	{ AudioCoutputs,	AudioNspatial,	"center",
	  AUDIO_MIXER_VALUE, WRAP(ac97_volume_mono),
	  AC97_REG_3D_CONTROL, 0x0000, 4, 8, 0, 1, 0, CHECK_3D, 0, 0, 0
	},
	{ AudioCoutputs,	AudioNspatial,	"depth",
	  AUDIO_MIXER_VALUE, WRAP(ac97_volume_mono),
	  AC97_REG_3D_CONTROL, 0x0000, 4, 0, 0, 1, 0, CHECK_3D, 0, 0, 0
	},

	/* SPDIF */
	{ "spdif", NULL, NULL,
	  AUDIO_MIXER_CLASS, NULL, 0,
	  0, 0, 0, 0, 0, 0, 0, CHECK_SPDIF, 0, 0, 0
	},
	{ "spdif", "enable", NULL,
	  AUDIO_MIXER_ENUM, WRAP(ac97_on_off),
	  AC97_REG_EXT_AUDIO_CTRL, -1, 1, 2, 0, 0, 0, CHECK_SPDIF, 0, 0, 0
	},

	/* Missing features: Simulated Stereo, POP, Loopback mode */
};

static const struct ac97_source_info modem_source_info[] = {
	/* Classes */
	{ AudioCinputs,		NULL,		NULL,
	  AUDIO_MIXER_CLASS,    NULL,		0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{ AudioCoutputs,	NULL,		NULL,
	  AUDIO_MIXER_CLASS,    NULL,		0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{ AudioCinputs,		Ac97Nline1,	NULL,
	  AUDIO_MIXER_VALUE, WRAP(ac97_volume_mono),
	  AC97_REG_LINE1_LEVEL, 0x8080, 4, 0, 0, 1, 0, CHECK_LINE1, 0, 0, 0
	},
	{ AudioCoutputs,	Ac97Nline1,	NULL,
	  AUDIO_MIXER_VALUE, WRAP(ac97_volume_mono),
	  AC97_REG_LINE1_LEVEL, 0x8080, 4, 8, 0, 1, 0, CHECK_LINE1, 0, 0, 0
	},
	{ AudioCinputs,		Ac97Nline1,	AudioNmute,
	  AUDIO_MIXER_ENUM, WRAP(ac97_on_off),
	  AC97_REG_LINE1_LEVEL, 0x8080, 1, 7, 0, 0, 0, CHECK_LINE1, 0, 0, 0
	},
	{ AudioCoutputs,	Ac97Nline1,	AudioNmute,
	  AUDIO_MIXER_ENUM, WRAP(ac97_on_off),
	  AC97_REG_LINE1_LEVEL, 0x8080, 1, 15, 0, 0, 0, CHECK_LINE1, 0, 0, 0
	},
};

#define AUDIO_SOURCE_INFO_SIZE \
		(sizeof(audio_source_info)/sizeof(audio_source_info[0]))
#define MODEM_SOURCE_INFO_SIZE \
		(sizeof(modem_source_info)/sizeof(modem_source_info[0]))
#define SOURCE_INFO_SIZE(as) ((as)->type == AC97_CODEC_TYPE_MODEM ? \
		MODEM_SOURCE_INFO_SIZE : AUDIO_SOURCE_INFO_SIZE)

/*
 * Check out http://www.intel.com/support/motherboards/desktop/sb/cs-025406.htm for
 * AC'97 Component Specification
 */

struct ac97_softc {
	/* ac97_codec_if must be at the first of ac97_softc. */
	struct ac97_codec_if codec_if;

	struct ac97_host_if *host_if;

	kmutex_t *lock;

#define AUDIO_MAX_SOURCES	(2 * AUDIO_SOURCE_INFO_SIZE)
#define MODEM_MAX_SOURCES	(2 * MODEM_SOURCE_INFO_SIZE)
	struct ac97_source_info audio_source_info[AUDIO_MAX_SOURCES];
	struct ac97_source_info modem_source_info[MODEM_MAX_SOURCES];
	struct ac97_source_info *source_info;
	int num_source_info;

	enum ac97_host_flags host_flags;
	unsigned int ac97_clock; /* usually 48000 */
#define AC97_STANDARD_CLOCK	48000U
	uint16_t power_all;
	uint16_t power_reg;	/* -> AC97_REG_POWER */
	uint16_t caps;		/* -> AC97_REG_RESET */
	uint16_t ext_id;	/* -> AC97_REG_EXT_AUDIO_ID */
	uint16_t ext_mid;	/* -> AC97_REG_EXT_MODEM_ID */
	uint16_t shadow_reg[128];

	int lock_counter;
	int type;

	/* sysctl */
	struct sysctllog *log;
	int offhook_line1_mib;
	int offhook_line2_mib;
	int offhook_line1;
	int offhook_line2;
};

static struct ac97_codec_if_vtbl ac97civ = {
	ac97_mixer_get_port,
	ac97_mixer_set_port,
	ac97_query_devinfo,
	ac97_get_portnum_by_name,
	ac97_restore_shadow,
	ac97_get_extcaps,
	ac97_set_rate,
	ac97_set_clock,
	ac97_detach,
	ac97_lock,
	ac97_unlock,
};

static const struct ac97_codecid {
	uint32_t id;
	uint32_t mask;
	const char *name;
	void (*init)(struct ac97_softc *);
} ac97codecid[] = {
	/*
	 * Analog Devices SoundMAX
	 * http://www.soundmax.com/products/information/codecs.html
	 * http://www.analog.com/productSelection/pdf/AD1881A_0.pdf
	 * http://www.analog.com/productSelection/pdf/AD1885_0.pdf
	 * http://www.analog.com/UploadedFiles/Data_Sheets/206585810AD1980_0.pdf
	 * http://www.analog.com/productSelection/pdf/AD1981A_0.pdf
	 * http://www.analog.com/productSelection/pdf/AD1981B_0.pdf
	 * http://www.analog.com/UploadedFiles/Data_Sheets/180644528AD1985_0.pdf
	 */
	{ AC97_CODEC_ID('A', 'D', 'S', 3),
	  0xffffffff,			"Analog Devices AD1819B", NULL, },
	{ AC97_CODEC_ID('A', 'D', 'S', 0x40),
	  0xffffffff,			"Analog Devices AD1881", NULL, },
	{ AC97_CODEC_ID('A', 'D', 'S', 0x48),
	  0xffffffff,			"Analog Devices AD1881A", NULL, },
	{ AC97_CODEC_ID('A', 'D', 'S', 0x60),
	  0xffffffff,			"Analog Devices AD1885", NULL, },
	{ AC97_CODEC_ID('A', 'D', 'S', 0x61),
	  0xffffffff,			"Analog Devices AD1886", NULL, },
	{ AC97_CODEC_ID('A', 'D', 'S', 0x63),
	  0xffffffff,			"Analog Devices AD1886A", NULL, },
	{ AC97_CODEC_ID('A', 'D', 'S', 0x68),
	  0xffffffff,			"Analog Devices AD1888", ac97_ad198x_init },
	{ AC97_CODEC_ID('A', 'D', 'S', 0x70),
	  0xffffffff,			"Analog Devices AD1980", ac97_ad198x_init },
	{ AC97_CODEC_ID('A', 'D', 'S', 0x72),
	  0xffffffff,			"Analog Devices AD1981A", NULL, },
	{ AC97_CODEC_ID('A', 'D', 'S', 0x74),
	  0xffffffff,			"Analog Devices AD1981B", NULL, },
	{ AC97_CODEC_ID('A', 'D', 'S', 0x75),
	  0xffffffff,			"Analog Devices AD1985", ac97_ad198x_init },
	{ AC97_CODEC_ID('A', 'D', 'S', 0),
	  AC97_VENDOR_ID_MASK,		"Analog Devices unknown", NULL, },

	/*
	 * Datasheets:
	 *	http://www.asahi-kasei.co.jp/akm/japanese/product/ak4543/ek4543.pdf
	 *	http://www.asahi-kasei.co.jp/akm/japanese/product/ak4544a/ek4544a.pdf
	 *	http://www.asahi-kasei.co.jp/akm/japanese/product/ak4545/ak4545_f00e.pdf
	 */
	{ AC97_CODEC_ID('A', 'K', 'M', 0),
	  0xffffffff,			"Asahi Kasei AK4540", NULL,	},
	{ AC97_CODEC_ID('A', 'K', 'M', 1),
	  0xffffffff,			"Asahi Kasei AK4542", NULL,	},
	{ AC97_CODEC_ID('A', 'K', 'M', 2),
	  0xffffffff,			"Asahi Kasei AK4541/AK4543", NULL, },
	{ AC97_CODEC_ID('A', 'K', 'M', 5),
	  0xffffffff,			"Asahi Kasei AK4544", NULL, },
	{ AC97_CODEC_ID('A', 'K', 'M', 6),
	  0xffffffff,			"Asahi Kasei AK4544A", NULL, },
	{ AC97_CODEC_ID('A', 'K', 'M', 7),
	  0xffffffff,			"Asahi Kasei AK4545", NULL, },
	{ AC97_CODEC_ID('A', 'K', 'M', 0),
	  AC97_VENDOR_ID_MASK,		"Asahi Kasei unknown", NULL, },

	/*
	 * Realtek & Avance Logic
	 *	http://www.realtek.com.tw/downloads/downloads1-3.aspx?lineid=5&famid=All&series=All&Spec=True
	 *
	 * ALC650 and ALC658 support VRA, but it supports only 8000, 11025,
	 * 12000, 16000, 22050, 24000, 32000, 44100, and 48000 Hz.
	 */
	{ AC97_CODEC_ID('A', 'L', 'C', 0x00),
	  0xfffffff0,			"Realtek RL5306", NULL,	},
	{ AC97_CODEC_ID('A', 'L', 'C', 0x10),
	  0xfffffff0,			"Realtek RL5382", NULL,	},
	{ AC97_CODEC_ID('A', 'L', 'C', 0x20),
	  0xfffffff0,			"Realtek RL5383/RL5522/ALC100", NULL,	},
	{ AC97_CODEC_ID('A', 'L', 'G', 0x10),
	  0xffffffff,			"Avance Logic ALC200/ALC201", NULL,	},
	{ AC97_CODEC_ID('A', 'L', 'G', 0x20),
	  0xfffffff0,			"Avance Logic ALC650", ac97_alc650_init },
	{ AC97_CODEC_ID('A', 'L', 'G', 0x30),
	  0xffffffff,			"Avance Logic ALC101", NULL,	},
	{ AC97_CODEC_ID('A', 'L', 'G', 0x40),
	  0xffffffff,			"Avance Logic ALC202", NULL,	},
	{ AC97_CODEC_ID('A', 'L', 'G', 0x50),
	  0xffffffff,			"Avance Logic ALC250", NULL,	},
	{ AC97_CODEC_ID('A', 'L', 'G', 0x60),
	  0xfffffff0,			"Avance Logic ALC655", NULL,	},
	{ AC97_CODEC_ID('A', 'L', 'G', 0x70),
	  0xffffffff,			"Avance Logic ALC203", NULL,	},
	{ AC97_CODEC_ID('A', 'L', 'G', 0x80),
	  0xfffffff0,			"Avance Logic ALC658", NULL,	},
	{ AC97_CODEC_ID('A', 'L', 'G', 0x90),
	  0xfffffff0,			"Avance Logic ALC850", NULL,	},
	{ AC97_CODEC_ID('A', 'L', 'C', 0),
	  AC97_VENDOR_ID_MASK,		"Realtek unknown", NULL,	},
	{ AC97_CODEC_ID('A', 'L', 'G', 0),
	  AC97_VENDOR_ID_MASK,		"Avance Logic unknown", NULL,	},

	/**
	 * C-Media Electronics Inc.
	 * http://www.cmedia.com.tw/doc/CMI9739%206CH%20Audio%20Codec%20SPEC_Ver12.pdf
	 */
	{ AC97_CODEC_ID('C', 'M', 'I', 0x61),
	  0xffffffff,			"C-Media CMI9739", NULL,	},
	{ AC97_CODEC_ID('C', 'M', 'I', 0),
	  AC97_VENDOR_ID_MASK,		"C-Media unknown", NULL,	},

	/* Cirrus Logic, Crystal series:
	 *  'C' 'R' 'Y' 0x0[0-7]  - CS4297
	 *              0x1[0-7]  - CS4297A
	 *              0x2[0-7]  - CS4298
	 *              0x2[8-f]  - CS4294
	 *              0x3[0-7]  - CS4299
	 *              0x4[8-f]  - CS4201
	 *              0x5[8-f]  - CS4205
	 *              0x6[0-7]  - CS4291
	 *              0x7[0-7]  - CS4202
	 * Datasheets:
	 *	http://www.cirrus.com/pubs/cs4297A-5.pdf?DocumentID=593
	 *	http://www.cirrus.com/pubs/cs4294.pdf?DocumentID=32
	 *	http://www.cirrus.com/pubs/cs4299-5.pdf?DocumentID=594
	 *	http://www.cirrus.com/pubs/cs4201-2.pdf?DocumentID=492
	 *	http://www.cirrus.com/pubs/cs4205-2.pdf?DocumentID=492
	 *	http://www.cirrus.com/pubs/cs4202-1.pdf?DocumentID=852
	 */
	{ AC97_CODEC_ID('C', 'R', 'Y', 0x00),
	  0xfffffff8,			"Crystal CS4297", NULL,	},
	{ AC97_CODEC_ID('C', 'R', 'Y', 0x10),
	  0xfffffff8,			"Crystal CS4297A", NULL,	},
	{ AC97_CODEC_ID('C', 'R', 'Y', 0x20),
	  0xfffffff8,			"Crystal CS4298", NULL,	},
	{ AC97_CODEC_ID('C', 'R', 'Y', 0x28),
	  0xfffffff8,			"Crystal CS4294", NULL,	},
	{ AC97_CODEC_ID('C', 'R', 'Y', 0x30),
	  0xfffffff8,			"Crystal CS4299", NULL,	},
	{ AC97_CODEC_ID('C', 'R', 'Y', 0x48),
	  0xfffffff8,			"Crystal CS4201", NULL,	},
	{ AC97_CODEC_ID('C', 'R', 'Y', 0x58),
	  0xfffffff8,			"Crystal CS4205", NULL,	},
	{ AC97_CODEC_ID('C', 'R', 'Y', 0x60),
	  0xfffffff8,			"Crystal CS4291", NULL,	},
	{ AC97_CODEC_ID('C', 'R', 'Y', 0x70),
	  0xfffffff8,			"Crystal CS4202", NULL,	},
	{ AC97_CODEC_ID('C', 'R', 'Y', 0),
	  AC97_VENDOR_ID_MASK,		"Cirrus Logic unknown", NULL,	},

	{ 0x45838308, 0xffffffff,	"ESS Technology ES1921", NULL, },
	{ 0x45838300, AC97_VENDOR_ID_MASK, "ESS Technology unknown", NULL, },

	{ AC97_CODEC_ID('H', 'R', 'S', 0),
	  0xffffffff,			"Intersil HMP9701", NULL,	},
	{ AC97_CODEC_ID('H', 'R', 'S', 0),
	  AC97_VENDOR_ID_MASK,		"Intersil unknown", NULL,	},

	/*
	 * IC Ensemble (VIA)
	 *	http://www.viatech.com/en/datasheet/DS1616.pdf
	 */
	{ AC97_CODEC_ID('I', 'C', 'E', 0x01),
	  0xffffffff,			"ICEnsemble ICE1230/VT1611", NULL,	},
	{ AC97_CODEC_ID('I', 'C', 'E', 0x11),
	  0xffffffff,			"ICEnsemble ICE1232/VT1611A", NULL,	},
	{ AC97_CODEC_ID('I', 'C', 'E', 0x14),
	  0xffffffff,			"ICEnsemble ICE1232A", NULL,	},
	{ AC97_CODEC_ID('I', 'C', 'E', 0x51),
	  0xffffffff,			"VIA Technologies VT1616", ac97_vt1616_init },
	{ AC97_CODEC_ID('I', 'C', 'E', 0x52),
	  0xffffffff,			"VIA Technologies VT1616i", ac97_vt1616_init },
	{ AC97_CODEC_ID('I', 'C', 'E', 0),
	  AC97_VENDOR_ID_MASK,		"ICEnsemble/VIA unknown", NULL,	},

	{ AC97_CODEC_ID('N', 'S', 'C', 0),
	  0xffffffff,			"National Semiconductor LM454[03568]", NULL, },
	{ AC97_CODEC_ID('N', 'S', 'C', 49),
	  0xffffffff,			"National Semiconductor LM4549", NULL, },
	{ AC97_CODEC_ID('N', 'S', 'C', 0),
	  AC97_VENDOR_ID_MASK,		"National Semiconductor unknown", NULL, },

	{ AC97_CODEC_ID('P', 'S', 'C', 4),
	  0xffffffff,			"Philips Semiconductor UCB1400", ac97_ucb1400_init, },
	{ AC97_CODEC_ID('P', 'S', 'C', 0),
	  AC97_VENDOR_ID_MASK,		"Philips Semiconductor unknown", NULL, },

	{ AC97_CODEC_ID('S', 'I', 'L', 34),
	  0xffffffff,			"Silicon Laboratory Si3036", NULL, },
	{ AC97_CODEC_ID('S', 'I', 'L', 35),
	  0xffffffff,			"Silicon Laboratory Si3038", NULL, },
	{ AC97_CODEC_ID('S', 'I', 'L', 0),
	  AC97_VENDOR_ID_MASK,		"Silicon Laboratory unknown", NULL, },

	{ AC97_CODEC_ID('T', 'R', 'A', 2),
	  0xffffffff,			"TriTech TR28022", NULL,	},
	{ AC97_CODEC_ID('T', 'R', 'A', 3),
	  0xffffffff,			"TriTech TR28023", NULL,	},
	{ AC97_CODEC_ID('T', 'R', 'A', 6),
	  0xffffffff,			"TriTech TR28026", NULL,	},
	{ AC97_CODEC_ID('T', 'R', 'A', 8),
	  0xffffffff,			"TriTech TR28028", NULL,	},
	{ AC97_CODEC_ID('T', 'R', 'A', 35),
	  0xffffffff,			"TriTech TR28602", NULL,	},
	{ AC97_CODEC_ID('T', 'R', 'A', 0),
	  AC97_VENDOR_ID_MASK,		"TriTech unknown", NULL,	},

	{ AC97_CODEC_ID('T', 'X', 'N', 0x20),
	  0xffffffff,			"Texas Instruments TLC320AD9xC", NULL, },
	{ AC97_CODEC_ID('T', 'X', 'N', 0),
	  AC97_VENDOR_ID_MASK,		"Texas Instruments unknown", NULL, },

	/*
	 * VIA
	 * http://www.viatech.com/en/multimedia/audio.jsp
	 */
	{ AC97_CODEC_ID('V', 'I', 'A', 0x61),
	  0xffffffff,			"VIA Technologies VT1612A", NULL, },
	{ AC97_CODEC_ID('V', 'I', 'A', 0),
	  AC97_VENDOR_ID_MASK,		"VIA Technologies unknown", NULL, },

	{ AC97_CODEC_ID('W', 'E', 'C', 1),
	  0xffffffff,			"Winbond W83971D", NULL,	},
	{ AC97_CODEC_ID('W', 'E', 'C', 0),
	  AC97_VENDOR_ID_MASK,		"Winbond unknown", NULL,	},

	/*
	 * http://www.wolfsonmicro.com/product_list.asp?cid=64
	 *	http://www.wolfsonmicro.com/download.asp/did.56/WM9701A.pdf - 00
	 *	http://www.wolfsonmicro.com/download.asp/did.57/WM9703.pdf  - 03
	 *	http://www.wolfsonmicro.com/download.asp/did.58/WM9704M.pdf - 04
	 *	http://www.wolfsonmicro.com/download.asp/did.59/WM9704Q.pdf - 04
	 *	http://www.wolfsonmicro.com/download.asp/did.184/WM9705_Rev34.pdf - 05
	 *	http://www.wolfsonmicro.com/download.asp/did.60/WM9707.pdf  - 03
	 *	http://www.wolfsonmicro.com/download.asp/did.136/WM9708.pdf - 03
	 *	http://www.wolfsonmicro.com/download.asp/did.243/WM9710.pdf - 05
	 */
	{ AC97_CODEC_ID('W', 'M', 'L', 0),
	  0xffffffff,			"Wolfson WM9701A", NULL,	},
	{ AC97_CODEC_ID('W', 'M', 'L', 3),
	  0xffffffff,			"Wolfson WM9703/WM9707/WM9708", NULL,	},
	{ AC97_CODEC_ID('W', 'M', 'L', 4),
	  0xffffffff,			"Wolfson WM9704", NULL,	},
	{ AC97_CODEC_ID('W', 'M', 'L', 5),
	  0xffffffff,			"Wolfson WM9705/WM9710", NULL, },
	{ AC97_CODEC_ID('W', 'M', 'L', 0),
	  AC97_VENDOR_ID_MASK,		"Wolfson unknown", NULL,	},

	/*
	 * http://www.yamaha.co.jp/english/product/lsi/us/products/pcaudio.html
	 * Datasheets:
	 *	http://www.yamaha.co.jp/english/product/lsi/us/products/pdf/4MF743A20.pdf
	 *	http://www.yamaha.co.jp/english/product/lsi/us/products/pdf/4MF753A20.pdf
	 */
	{ AC97_CODEC_ID('Y', 'M', 'H', 0),
	  0xffffffff,			"Yamaha YMF743-S", NULL,	},
	{ AC97_CODEC_ID('Y', 'M', 'H', 3),
	  0xffffffff,			"Yamaha YMF753-S", NULL,	},
	{ AC97_CODEC_ID('Y', 'M', 'H', 0),
	  AC97_VENDOR_ID_MASK,		"Yamaha unknown", NULL,	},

	/*
	 * http://www.sigmatel.com/products/technical_docs.htm
	 * and
	 * http://www.sigmatel.com/documents/c-major-brochure-9-0.pdf
	 */
	{ 0x83847600, 0xffffffff,	"SigmaTel STAC9700",	NULL, },
	{ 0x83847604, 0xffffffff,	"SigmaTel STAC9701/3/4/5", NULL, },
	{ 0x83847605, 0xffffffff,	"SigmaTel STAC9704",	NULL, },
	{ 0x83847608, 0xffffffff,	"SigmaTel STAC9708",	NULL, },
	{ 0x83847609, 0xffffffff,	"SigmaTel STAC9721/23",	NULL, },
	{ 0x83847644, 0xffffffff,	"SigmaTel STAC9744/45",	NULL, },
	{ 0x83847650, 0xffffffff,	"SigmaTel STAC9750/51",	NULL, },
	{ 0x83847652, 0xffffffff,	"SigmaTel STAC9752/53",	NULL, },
	{ 0x83847656, 0xffffffff,	"SigmaTel STAC9756/57",	NULL, },
	{ 0x83847658, 0xffffffff,	"SigmaTel STAC9758/59",	NULL, },
	{ 0x83847666, 0xffffffff,	"SigmaTel STAC9766/67",	NULL, },
	{ 0x83847684, 0xffffffff,	"SigmaTel STAC9783/84",	NULL, },
	{ 0x83847600, AC97_VENDOR_ID_MASK, "SigmaTel unknown",	NULL, },

	/* Conexant AC'97 modems -- good luck finding datasheets! */
	{ AC97_CODEC_ID('C', 'X', 'T', 33),
	  0xffffffff,			"Conexant HSD11246", NULL, },
	{ AC97_CODEC_ID('C', 'X', 'T', 34),
	  0xffffffff,			"Conexant D480 MDC V.92 Modem", NULL, },
	{ AC97_CODEC_ID('C', 'X', 'T', 48),
	  0xffffffff,			"Conexant CXT48", NULL, },
	{ AC97_CODEC_ID('C', 'X', 'T', 0),
	  AC97_VENDOR_ID_MASK,		"Conexant unknown", NULL, },

	{ 0,
	  0,			NULL,	NULL, 		},
};

static const char * const ac97enhancement[] = {
	"no 3D stereo",
	"Analog Devices Phat Stereo",
	"Creative",
	"National Semi 3D",
	"Yamaha Ymersion",
	"BBE 3D",
	"Crystal Semi 3D",
	"Qsound QXpander",
	"Spatializer 3D",
	"SRS 3D",
	"Platform Tech 3D",
	"AKM 3D",
	"Aureal",
	"AZTECH 3D",
	"Binaura 3D",
	"ESS Technology",
	"Harman International VMAx",
	"Nvidea 3D",
	"Philips Incredible Sound",
	"Texas Instruments' 3D",
	"VLSI Technology 3D",
	"TriTech 3D",
	"Realtek 3D",
	"Samsung 3D",
	"Wolfson Microelectronics 3D",
	"Delta Integration 3D",
	"SigmaTel 3D",
	"KS Waves 3D",
	"Rockwell 3D",
	"Unknown 3D",
	"Unknown 3D",
	"Unknown 3D",
};

static const char * const ac97feature[] = {
	"dedicated mic channel",
	"reserved",
	"tone",
	"simulated stereo",
	"headphone",
	"bass boost",
	"18 bit DAC",
	"20 bit DAC",
	"18 bit ADC",
	"20 bit ADC"
};


/* #define AC97_DEBUG 10 */
/* #define AC97_IO_DEBUG */

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (ac97debug) printf x
#define DPRINTFN(n,x)	if (ac97debug>(n)) printf x
#ifdef AC97_DEBUG
int	ac97debug = AC97_DEBUG;
#else
int	ac97debug = 0;
#endif
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#ifdef AC97_IO_DEBUG
static const char *ac97_register_names[0x80 / 2] = {
	"RESET", "MASTER_VOLUME", "HEADPHONE_VOLUME", "MASTER_VOLUME_MONO",
	"MASTER_TONE", "PCBEEP_VOLUME", "PHONE_VOLUME", "MIC_VOLUME",
	"LINEIN_VOLUME", "CD_VOLUME", "VIDEO_VOLUME", "AUX_VOLUME",
	"PCMOUT_VOLUME", "RECORD_SELECT", "RECORD_GATIN", "RECORD_GAIN_MIC",
	"GP", "3D_CONTROL", "AUDIO_INT", "POWER",
	"EXT_AUDIO_ID", "EXT_AUDIO_CTRL", "PCM_FRONT_DAC_RATE", "PCM_SURR_DAC_RATE",
	"PCM_LFE_DAC_RATE", "PCM_LR_ADC_RATE", "PCM_MIC_ADC_RATE", "CENTER_LFE_MASTER",
	"SURR_MASTER", "SPDIF_CTRL", "EXT_MODEM_ID", "EXT_MODEM_CTRL",
	"LINE1_RATE", "LINE2_RATE", "HANDSET_RATE", "LINE1_LEVEL",
	"LINE2_LEVEL", "HANDSET_LEVEL", "GPIO_PIN_CONFIG", "GPIO_PIN_POLARITY",
	"GPIO_PIN_STICKY", "GPIO_PIN_WAKEUP", "GPIO_PIN_STATUS", "MISC_MODEM_CTRL",
	"0x58", "VENDOR-5A", "VENDOR-5C", "VENDOR-5E",
	"0x60", "0x62", "0x64", "0x66",
	"0x68", "0x6a", "0x6c", "0x6e",
	"VENDOR-70", "VENDOR-72", "VENDOR-74", "VENDOR-76",
	"VENDOR-78", "VENDOR-7A", "VENDOR_ID1", "VENDOR_ID2"
};
#endif

/*
 * XXX Some cards have an inverted AC97_POWER_EAMP bit.
 * These cards will produce no sound unless AC97_HOST_INVERTED_EAMP is set.
 */

#define POWER_EAMP_ON(as)  ((as->host_flags & AC97_HOST_INVERTED_EAMP) \
			    ? AC97_POWER_EAMP : 0)
#define POWER_EAMP_OFF(as) ((as->host_flags & AC97_HOST_INVERTED_EAMP) \
			    ? 0 : AC97_POWER_EAMP)

static void
ac97_read(struct ac97_softc *as, uint8_t reg, uint16_t *val)
{
	KASSERT(mutex_owned(as->lock));

	if (as->host_flags & AC97_HOST_DONT_READ &&
	    (reg != AC97_REG_VENDOR_ID1 && reg != AC97_REG_VENDOR_ID2 &&
	     reg != AC97_REG_RESET)) {
		*val = as->shadow_reg[reg >> 1];
		return;
	}

	if (as->host_if->read(as->host_if->arg, reg, val)) {
		*val = as->shadow_reg[reg >> 1];
	}
}

static int
ac97_write(struct ac97_softc *as, uint8_t reg, uint16_t val)
{
	KASSERT(mutex_owned(as->lock));

#ifndef AC97_IO_DEBUG
	as->shadow_reg[reg >> 1] = val;
	return as->host_if->write(as->host_if->arg, reg, val);
#else
	int ret;
	uint16_t actual;

	as->shadow_reg[reg >> 1] = val;
	ret = as->host_if->write(as->host_if->arg, reg, val);
	as->host_if->read(as->host_if->arg, reg, &actual);
	if (val != actual && reg < 0x80) {
		printf("ac97_write: reg=%s, written=0x%04x, read=0x%04x\n",
		       ac97_register_names[reg / 2], val, actual);
	}
	return ret;
#endif
}

static void
ac97_setup_defaults(struct ac97_softc *as)
{
	int idx;
	const struct ac97_source_info *si;

	KASSERT(mutex_owned(as->lock));

	memset(as->shadow_reg, 0, sizeof(as->shadow_reg));

	for (idx = 0; idx < AUDIO_SOURCE_INFO_SIZE; idx++) {
		si = &audio_source_info[idx];
		if (si->default_value >= 0)
			ac97_write(as, si->reg, si->default_value);
	}
	for (idx = 0; idx < MODEM_SOURCE_INFO_SIZE; idx++) {
		si = &modem_source_info[idx];
		if (si->default_value >= 0)
			ac97_write(as, si->reg, si->default_value);
	}
}

static void
ac97_restore_shadow(struct ac97_codec_if *codec_if)
{
	struct ac97_softc *as;
	const struct ac97_source_info *si;
	int idx;
	uint16_t val;

	as = (struct ac97_softc *)codec_if;

	KASSERT(mutex_owned(as->lock));

	if (as->type == AC97_CODEC_TYPE_AUDIO) {
		/* restore AC97_REG_POWER */
		ac97_write(as, AC97_REG_POWER, as->power_reg);
		/* make sure chip is fully operational */
		for (idx = 50000; idx >= 0; idx--) {
			ac97_read(as, AC97_REG_POWER, &val);
			if ((val & as->power_all) == as->power_all)
				break;
			DELAY(10);
		}

		/*
		 * actually try changing a value!
		 * The default value of AC97_REG_MASTER_VOLUME is 0x8000.
		 */
		for (idx = 50000; idx >= 0; idx--) {
			ac97_write(as, AC97_REG_MASTER_VOLUME, 0x1010);
			ac97_read(as, AC97_REG_MASTER_VOLUME, &val);
			if (val == 0x1010)
				break;
			DELAY(10);
		}
	}

       for (idx = 0; idx < SOURCE_INFO_SIZE(as); idx++) {
		if (as->type == AC97_CODEC_TYPE_MODEM)
			si = &modem_source_info[idx];
		else
			si = &audio_source_info[idx];
		/* don't "restore" to the reset reg! */
		if (si->reg != AC97_REG_RESET)
			ac97_write(as, si->reg, as->shadow_reg[si->reg >> 1]);
	}

	if (as->ext_id & (AC97_EXT_AUDIO_VRA | AC97_EXT_AUDIO_DRA
			  | AC97_EXT_AUDIO_SPDIF | AC97_EXT_AUDIO_VRM
			  | AC97_EXT_AUDIO_CDAC | AC97_EXT_AUDIO_SDAC
			  | AC97_EXT_AUDIO_LDAC)) {
		ac97_write(as, AC97_REG_EXT_AUDIO_CTRL,
		    as->shadow_reg[AC97_REG_EXT_AUDIO_CTRL >> 1]);
	}
	if (as->ext_mid & (AC97_EXT_MODEM_LINE1 | AC97_EXT_MODEM_LINE2
			  | AC97_EXT_MODEM_HANDSET | AC97_EXT_MODEM_CID1
			  | AC97_EXT_MODEM_CID2 | AC97_EXT_MODEM_ID0
			  | AC97_EXT_MODEM_ID1)) {
		ac97_write(as, AC97_REG_EXT_MODEM_CTRL,
		    as->shadow_reg[AC97_REG_EXT_MODEM_CTRL >> 1]);
	}
}

static int
ac97_str_equal(const char *a, const char *b)
{
	return (a == b) || (a && b && (!strcmp(a, b)));
}

static int
ac97_check_capability(struct ac97_softc *as, int check)
{
	switch (check) {
	case CHECK_NONE:
		return 1;
	case CHECK_SURROUND:
		return as->ext_id & AC97_EXT_AUDIO_SDAC;
	case CHECK_CENTER:
		return as->ext_id & AC97_EXT_AUDIO_CDAC;
	case CHECK_LFE:
		return as->ext_id & AC97_EXT_AUDIO_LDAC;
	case CHECK_SPDIF:
		return as->ext_id & AC97_EXT_AUDIO_SPDIF;
	case CHECK_HEADPHONES:
		return as->caps & AC97_CAPS_HEADPHONES;
	case CHECK_TONE:
		return as->caps & AC97_CAPS_TONECTRL;
	case CHECK_MIC:
		return as->caps & AC97_CAPS_MICIN;
	case CHECK_LOUDNESS:
		return as->caps & AC97_CAPS_LOUDNESS;
	case CHECK_3D:
		return AC97_CAPS_ENHANCEMENT(as->caps) != 0;
	case CHECK_LINE1:
		return as->ext_mid & AC97_EXT_MODEM_LINE1;
	case CHECK_LINE2:
		return as->ext_mid & AC97_EXT_MODEM_LINE2;
	case CHECK_HANDSET:
		return as->ext_mid & AC97_EXT_MODEM_HANDSET;
	default:
		printf("%s: internal error: feature=%d\n", __func__, check);
		return 0;
	}
}

static void
ac97_setup_source_info(struct ac97_softc *as)
{
	int idx, ouridx;
	struct ac97_source_info *si, *si2;
	uint16_t value1, value2, value3;

	KASSERT(mutex_owned(as->lock));

	for (idx = 0, ouridx = 0; idx < SOURCE_INFO_SIZE(as); idx++) {
		si = &as->source_info[ouridx];
		if (as->type == AC97_CODEC_TYPE_MODEM) {
			memcpy(si, &modem_source_info[idx], sizeof(*si));
		} else {
			memcpy(si, &audio_source_info[idx], sizeof(*si));
		}
		if (!ac97_check_capability(as, si->req_feature))
			continue;
		if (si->checkbits) {
			/* read the register value */
			ac97_read(as, si->reg, &value1);
			/* write 0b100000 */
			value2 = value1 & 0xffc0;
			value2 |= 0x20;
			ac97_write(as, si->reg, value2);
			/* verify */
			ac97_read(as, si->reg, &value3);
			if (value2 == value3) {
				si->bits = 6;
			} else {
				si->bits = 5;
			}
			DPRINTF(("%s: register=%02x bits=%d\n",
			    __func__, si->reg, si->bits));
			ac97_write(as, si->reg, value1);
		}

		switch (si->type) {
		case AUDIO_MIXER_CLASS:
			si->mixer_class = ouridx;
			ouridx++;
			break;
		case AUDIO_MIXER_VALUE:
			/* Todo - Test to see if it works */
			ouridx++;

			/* Add an entry for mute, if necessary */
			if (si->mute) {
				si = &as->source_info[ouridx];
				if (as->type == AC97_CODEC_TYPE_MODEM)
					memcpy(si, &modem_source_info[idx],
					    sizeof(*si));
				else
					memcpy(si, &audio_source_info[idx],
					    sizeof(*si));
				si->qualifier = AudioNmute;
				si->type = AUDIO_MIXER_ENUM;
				si->info = &ac97_on_off;
				si->info_size = sizeof(ac97_on_off);
				si->bits = 1;
				si->ofs = 15;
				si->mute = 0;
				si->polarity = 0;
				ouridx++;
			}
			break;
		case AUDIO_MIXER_ENUM:
			/* Todo - Test to see if it works */
			ouridx++;
			break;
		default:
			aprint_error ("ac97: shouldn't get here\n");
			break;
		}
	}

	as->num_source_info = ouridx;

	for (idx = 0; idx < as->num_source_info; idx++) {
		int idx2, previdx;

		si = &as->source_info[idx];

		/* Find mixer class */
		for (idx2 = 0; idx2 < as->num_source_info; idx2++) {
			si2 = &as->source_info[idx2];

			if (si2->type == AUDIO_MIXER_CLASS &&
			    ac97_str_equal(si->class,
					   si2->class)) {
				si->mixer_class = idx2;
			}
		}


		/* Setup prev and next pointers */
		if (si->prev != 0)
			continue;

		if (si->qualifier)
			continue;

		si->prev = AUDIO_MIXER_LAST;
		previdx = idx;

		for (idx2 = 0; idx2 < as->num_source_info; idx2++) {
			if (idx2 == idx)
				continue;

			si2 = &as->source_info[idx2];

			if (!si2->prev &&
			    ac97_str_equal(si->class, si2->class) &&
			    ac97_str_equal(si->device, si2->device)) {
				as->source_info[previdx].next = idx2;
				as->source_info[idx2].prev = previdx;

				previdx = idx2;
			}
		}

		as->source_info[previdx].next = AUDIO_MIXER_LAST;
	}
}

/* backward compatibility */
int
ac97_attach(struct ac97_host_if *host_if, device_t sc_dev, kmutex_t *lk)
{
	return ac97_attach_type(host_if, sc_dev, AC97_CODEC_TYPE_AUDIO, lk);
}

int
ac97_attach_type(struct ac97_host_if *host_if, device_t sc_dev, int type, kmutex_t *lk)
{
	struct ac97_softc *as;
	int error, i, j;
	uint32_t id;
	uint16_t id1, id2;
	uint16_t extstat, rate;
	uint16_t val;
	mixer_ctrl_t ctl;
	void (*initfunc)(struct ac97_softc *);
#define FLAGBUFLEN	140
	char flagbuf[FLAGBUFLEN];

	initfunc = NULL;
	as = malloc(sizeof(struct ac97_softc), M_DEVBUF, M_WAITOK|M_ZERO);

	if (as == NULL)
		return ENOMEM;

	as->codec_if.vtbl = &ac97civ;
	as->host_if = host_if;
	as->type = type;
	as->lock = lk;

	if ((error = host_if->attach(host_if->arg, &as->codec_if))) {
		free(as, M_DEVBUF);
		return error;
	}

	mutex_enter(as->lock);

	if (host_if->reset != NULL) {
		if ((error = host_if->reset(host_if->arg))) {
			mutex_exit(as->lock);
			free(as, M_DEVBUF);
			return error;
		}
	}

	if (host_if->flags)
		as->host_flags = host_if->flags(host_if->arg);

	/*
	 * Assume codec has all four power bits.
	 * XXXSCW: what to do for modems?
	 */
	as->power_all = AC97_POWER_REF | AC97_POWER_ANL | AC97_POWER_DAC |
	    AC97_POWER_ADC;
	if (as->type == AC97_CODEC_TYPE_AUDIO) {
		host_if->write(host_if->arg, AC97_REG_RESET, 0);

		/*
		 * Power-up everything except the analogue mixer.
		 * If this codec doesn't support analogue mixer power-down,
		 * AC97_POWER_MIXER will read back as zero.
		 */
		host_if->write(host_if->arg, AC97_REG_POWER, AC97_POWER_MIXER);
		ac97_read(as, AC97_REG_POWER, &val);
		if ((val & AC97_POWER_MIXER) == 0) {
			/* Codec doesn't support analogue mixer power-down */
			as->power_all &= ~AC97_POWER_ANL;
		}
		host_if->write(host_if->arg, AC97_REG_POWER, POWER_EAMP_ON(as));

		for (i = 500000; i >= 0; i--) {
			ac97_read(as, AC97_REG_POWER, &val);
			if ((val & as->power_all) == as->power_all)
			       break;
			DELAY(1);
		}

		/* save AC97_REG_POWER so that we can restore it later */
		ac97_read(as, AC97_REG_POWER, &as->power_reg);
	} else if (as->type == AC97_CODEC_TYPE_MODEM) {
		host_if->write(host_if->arg, AC97_REG_EXT_MODEM_ID, 0);
	}

	ac97_setup_defaults(as);
	if (as->type == AC97_CODEC_TYPE_AUDIO)
		ac97_read(as, AC97_REG_RESET, &as->caps);
	ac97_read(as, AC97_REG_VENDOR_ID1, &id1);
	ac97_read(as, AC97_REG_VENDOR_ID2, &id2);

	mutex_exit(as->lock);

	id = (id1 << 16) | id2;
	aprint_normal_dev(sc_dev, "ac97: ");

	for (i = 0; ; i++) {
		if (ac97codecid[i].id == 0) {
			char pnp[4];

			AC97_GET_CODEC_ID(id, pnp);
#define ISASCII(c) ((c) >= ' ' && (c) < 0x7f)
			if (ISASCII(pnp[0]) && ISASCII(pnp[1]) &&
			    ISASCII(pnp[2]))
				aprint_normal("%c%c%c%d",
				    pnp[0], pnp[1], pnp[2], pnp[3]);
			else
				aprint_normal("unknown (0x%08x)", id);
			break;
		}
		if (ac97codecid[i].id == (id & ac97codecid[i].mask)) {
			aprint_normal("%s", ac97codecid[i].name);
			if (ac97codecid[i].mask == AC97_VENDOR_ID_MASK) {
				aprint_normal(" (0x%08x)", id);
			}
			initfunc = ac97codecid[i].init;
			break;
		}
	}
	aprint_normal(" codec; ");
	for (i = j = 0; i < 10; i++) {
		if (as->caps & (1 << i)) {
			aprint_normal("%s%s", j ? ", " : "", ac97feature[i]);
			j++;
		}
	}
	aprint_normal("%s%s\n", j ? ", " : "",
	       ac97enhancement[AC97_CAPS_ENHANCEMENT(as->caps)]);

	as->ac97_clock = AC97_STANDARD_CLOCK;

	mutex_enter(as->lock);

	if (as->type == AC97_CODEC_TYPE_AUDIO) {
		ac97_read(as, AC97_REG_EXT_AUDIO_ID, &as->ext_id);
		if (as->ext_id != 0) {
			mutex_exit(as->lock);

			/* Print capabilities */
			snprintb(flagbuf, sizeof(flagbuf),
			     "\20\20SECONDARY10\17SECONDARY01"
			     "\14AC97_23\13AC97_22\12AMAP\11LDAC\10SDAC"
			     "\7CDAC\4VRM\3SPDIF\2DRA\1VRA", as->ext_id);
			aprint_normal_dev(sc_dev, "ac97: ext id %s\n",
				      flagbuf);

			/* Print unusual settings */
			if (as->ext_id & AC97_EXT_AUDIO_DSA_MASK) {
				aprint_normal_dev(sc_dev, "ac97: Slot assignment: ");
				switch (as->ext_id & AC97_EXT_AUDIO_DSA_MASK) {
				case AC97_EXT_AUDIO_DSA01:
					aprint_normal("7&8, 6&9, 10&11.\n");
					break;
				case AC97_EXT_AUDIO_DSA10:
					aprint_normal("6&9, 10&11, 3&4.\n");
					break;
				case AC97_EXT_AUDIO_DSA11:
					aprint_normal("10&11, 3&4, 7&8.\n");
					break;
				}
			}
			if (as->host_flags & AC97_HOST_INVERTED_EAMP) {
				aprint_normal_dev(sc_dev, "ac97: using inverted "
					      "AC97_POWER_EAMP bit\n");
			}

			mutex_enter(as->lock);

			/* Enable and disable features */
			ac97_read(as, AC97_REG_EXT_AUDIO_CTRL, &extstat);
			extstat &= ~AC97_EXT_AUDIO_DRA;
			if (as->ext_id & AC97_EXT_AUDIO_LDAC)
				extstat |= AC97_EXT_AUDIO_LDAC;
			if (as->ext_id & AC97_EXT_AUDIO_SDAC)
				extstat |= AC97_EXT_AUDIO_SDAC;
			if (as->ext_id & AC97_EXT_AUDIO_CDAC)
				extstat |= AC97_EXT_AUDIO_CDAC;
			if (as->ext_id & AC97_EXT_AUDIO_VRM)
				extstat |= AC97_EXT_AUDIO_VRM;
			if (as->ext_id & AC97_EXT_AUDIO_SPDIF) {
				/* Output the same data as DAC to SPDIF output */
				extstat &= ~AC97_EXT_AUDIO_SPSA_MASK;
				extstat |= AC97_EXT_AUDIO_SPSA34;
				ac97_read(as, AC97_REG_SPDIF_CTRL, &val);
				val = (val & ~AC97_SPDIF_SPSR_MASK)
				    | AC97_SPDIF_SPSR_48K;
				ac97_write(as, AC97_REG_SPDIF_CTRL, val);
			}
			if (as->ext_id & AC97_EXT_AUDIO_VRA)
				extstat |= AC97_EXT_AUDIO_VRA;
			ac97_write(as, AC97_REG_EXT_AUDIO_CTRL, extstat);
			if (as->ext_id & AC97_EXT_AUDIO_VRA) {
				/* VRA should be enabled. */
				/* so it claims to do variable rate, let's make sure */
				ac97_write(as, AC97_REG_PCM_FRONT_DAC_RATE,
					   44100);
				ac97_read(as, AC97_REG_PCM_FRONT_DAC_RATE,
					  &rate);
				if (rate != 44100) {
					/* We can't believe ext_id */
					as->ext_id = 0;
					aprint_normal_dev(sc_dev,
					    "Ignore these capabilities.\n");
				}
				/* restore the default value */
				ac97_write(as, AC97_REG_PCM_FRONT_DAC_RATE,
					   AC97_SINGLE_RATE);
			}
		}
	} else if (as->type == AC97_CODEC_TYPE_MODEM) {
		const struct sysctlnode *node;
		const struct sysctlnode *node_line1;
		const struct sysctlnode *node_line2;
		uint16_t xrate = 8000;
		uint16_t xval, reg;
		int err;

		ac97_read(as, AC97_REG_EXT_MODEM_ID, &as->ext_mid);
		mutex_exit(as->lock);

		if (as->ext_mid == 0 || as->ext_mid == 0xffff) {
			aprint_normal_dev(sc_dev, "no modem codec found\n");
			free(as, M_DEVBUF);
			return ENXIO;
		}
		as->type = AC97_CODEC_TYPE_MODEM;

		/* Print capabilities */
		snprintb(flagbuf, sizeof(flagbuf),
		    "\20\5CID2\4CID1\3HANDSET\2LINE2\1LINE1", as->ext_mid);
		aprint_normal_dev(sc_dev, "ac97: ext mid %s",
			      flagbuf);
		aprint_normal(", %s codec\n",
			      (as->ext_mid & 0xc000) == 0 ?
			      "primary" : "secondary");

		/* Setup modem and sysctls */
		err = sysctl_createv(&as->log, 0, NULL, NULL, 0, CTLTYPE_NODE,
				     "hw", NULL, NULL, 0, NULL, 0, CTL_HW,
				     CTL_EOL);
		if (err != 0)
			goto setup_modem;
		err = sysctl_createv(&as->log, 0, NULL, &node, 0,
				     CTLTYPE_NODE, device_xname(sc_dev), NULL,
				     NULL, 0, NULL, 0, CTL_HW, CTL_CREATE,
				     CTL_EOL);
		if (err != 0)
			goto setup_modem;
setup_modem:
		mutex_enter(as->lock);

		/* reset */
		ac97_write(as, AC97_REG_EXT_MODEM_ID, 1);

		/* program rates */
		xval = 0xff00 & ~AC97_EXT_MODEM_CTRL_PRA;
		if (as->ext_mid & AC97_EXT_MODEM_LINE1) {
			ac97_write(as, AC97_REG_LINE1_RATE, xrate);
			xval &= ~(AC97_EXT_MODEM_CTRL_PRC |
			       AC97_EXT_MODEM_CTRL_PRD);
		}
		if (as->ext_mid & AC97_EXT_MODEM_LINE2) {
			ac97_write(as, AC97_REG_LINE2_RATE, xrate);
			xval &= ~(AC97_EXT_MODEM_CTRL_PRE |
			       AC97_EXT_MODEM_CTRL_PRF);
		}
		if (as->ext_mid & AC97_EXT_MODEM_HANDSET) {
			ac97_write(as, AC97_REG_HANDSET_RATE, xrate);
			xval &= ~(AC97_EXT_MODEM_CTRL_PRG |
			       AC97_EXT_MODEM_CTRL_PRH);
		}

		/* power-up everything */
		ac97_write(as, AC97_REG_EXT_MODEM_CTRL, 0);
		for (i = 5000; i >= 0; i--) {
			ac97_read(as, AC97_REG_EXT_MODEM_CTRL, &reg);
			if ((reg & /*XXXval*/0xf) == /*XXXval*/0xf)
			       break;
			DELAY(1);
		}
		if (i <= 0) {
			mutex_exit(as->lock);
			printf("%s: codec not responding, status=0x%x\n",
			    device_xname(sc_dev), reg);
			return ENXIO;
		}

		/* setup sysctls */
		if (as->ext_mid & AC97_EXT_MODEM_LINE1) {
			ac97_read(as, AC97_REG_GPIO_CFG, &reg);
			reg &= ~AC97_GPIO_LINE1_OH;
			ac97_write(as, AC97_REG_GPIO_CFG, reg);
			ac97_read(as, AC97_REG_GPIO_POLARITY, &reg);
			reg &= ~AC97_GPIO_LINE1_OH;
			ac97_write(as, AC97_REG_GPIO_POLARITY, reg);

			mutex_exit(as->lock);
			err = sysctl_createv(&as->log, 0, NULL, &node_line1,
					     CTLFLAG_READWRITE, CTLTYPE_INT,
					     "line1",
					     SYSCTL_DESCR("off-hook line1"),
					     ac97_sysctl_verify, 0, (void *)as, 0,
					     CTL_HW, node->sysctl_num,
					     CTL_CREATE, CTL_EOL);
			mutex_enter(as->lock);

			if (err != 0)
				goto sysctl_err;
			as->offhook_line1_mib = node_line1->sysctl_num;
		}
		if (as->ext_mid & AC97_EXT_MODEM_LINE2) {
			ac97_read(as, AC97_REG_GPIO_CFG, &reg);
			reg &= ~AC97_GPIO_LINE2_OH;
			ac97_write(as, AC97_REG_GPIO_CFG, reg);
			ac97_read(as, AC97_REG_GPIO_POLARITY, &reg);
			reg &= ~AC97_GPIO_LINE2_OH;
			ac97_write(as, AC97_REG_GPIO_POLARITY, reg);

			mutex_exit(as->lock);
			err = sysctl_createv(&as->log, 0, NULL, &node_line2,
					     CTLFLAG_READWRITE, CTLTYPE_INT,
					     "line2",
					     SYSCTL_DESCR("off-hook line2"),
					     ac97_sysctl_verify, 0, (void *)as, 0,
					     CTL_HW, node->sysctl_num,
					     CTL_CREATE, CTL_EOL);
			mutex_enter(as->lock);

			if (err != 0)
				goto sysctl_err;
			as->offhook_line2_mib = node_line2->sysctl_num;
		}
sysctl_err:

		ac97_write(as, AC97_REG_GPIO_STICKY, 0xffff);
		ac97_write(as, AC97_REG_GPIO_WAKEUP, 0x0);
		ac97_write(as, AC97_REG_MISC_AFE, 0x0);
	}

	as->source_info = (as->type == AC97_CODEC_TYPE_MODEM ?
			   as->modem_source_info : as->audio_source_info);
	ac97_setup_source_info(as);

	memset(&ctl, 0, sizeof(ctl));
	/* disable mutes */
	for (i = 0; i < 11; i++) {
		static struct {
			const char *class, *device;
		} d[11] = {
			{ AudioCoutputs, AudioNmaster},
			{ AudioCoutputs, AudioNheadphone},
			{ AudioCoutputs, AudioNsurround},
			{ AudioCoutputs, AudioNcenter},
			{ AudioCoutputs, AudioNlfe},
			{ AudioCinputs, AudioNdac},
			{ AudioCinputs, AudioNcd},
			{ AudioCinputs, AudioNline},
			{ AudioCinputs, AudioNaux},
			{ AudioCinputs, AudioNvideo},
			{ AudioCrecord, AudioNvolume},
		};

		ctl.type = AUDIO_MIXER_ENUM;
		ctl.un.ord = 0;

		ctl.dev = ac97_get_portnum_by_name(&as->codec_if,
			d[i].class, d[i].device, AudioNmute);
		ac97_mixer_set_port(&as->codec_if, &ctl);
	}
	ctl.type = AUDIO_MIXER_ENUM;
	ctl.un.ord = 0;
	ctl.dev = ac97_get_portnum_by_name(&as->codec_if, AudioCrecord,
					   AudioNsource, NULL);
	ac97_mixer_set_port(&as->codec_if, &ctl);

	/* set a reasonable default volume */
	ctl.type = AUDIO_MIXER_VALUE;
	ctl.un.value.num_channels = 2;
	ctl.un.value.level[AUDIO_MIXER_LEVEL_LEFT] = \
	ctl.un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = 127;
	ctl.dev = ac97_get_portnum_by_name(&as->codec_if, AudioCoutputs,
					   AudioNmaster, NULL);
	ac97_mixer_set_port(&as->codec_if, &ctl);
	ctl.dev = ac97_get_portnum_by_name(&as->codec_if, AudioCoutputs,
					   AudioNsurround, NULL);
	ac97_mixer_set_port(&as->codec_if, &ctl);
	ctl.un.value.num_channels = 1;
	ctl.dev = ac97_get_portnum_by_name(&as->codec_if, AudioCoutputs,
					   AudioNcenter, NULL);
	ac97_mixer_set_port(&as->codec_if, &ctl);
	ctl.dev = ac97_get_portnum_by_name(&as->codec_if, AudioCoutputs,
					   AudioNlfe, NULL);
	ac97_mixer_set_port(&as->codec_if, &ctl);

	if (initfunc != NULL)
		initfunc(as);

	/* restore AC97_REG_POWER */
	if (as->type == AC97_CODEC_TYPE_AUDIO)
		ac97_write(as, AC97_REG_POWER, as->power_reg);

	mutex_exit(as->lock);

	return 0;
}

static void
ac97_detach(struct ac97_codec_if *codec_if)
{
	struct ac97_softc *as;

	as = (struct ac97_softc *)codec_if;

	mutex_enter(as->lock);
	ac97_write(as, AC97_REG_POWER, AC97_POWER_IN | AC97_POWER_OUT
		   | AC97_POWER_MIXER | AC97_POWER_MIXER_VREF
		   | AC97_POWER_ACLINK | AC97_POWER_CLK | AC97_POWER_AUX
		   | POWER_EAMP_OFF(as));
	mutex_exit(as->lock);

	free(as, M_DEVBUF);
}

static void
ac97_lock(struct ac97_codec_if *codec_if)
{
	struct ac97_softc *as;

	as = (struct ac97_softc *)codec_if;

	KASSERT(mutex_owned(as->lock));

	as->lock_counter++;
}

static void
ac97_unlock(struct ac97_codec_if *codec_if)
{
	struct ac97_softc *as;

	as = (struct ac97_softc *)codec_if;

	KASSERT(mutex_owned(as->lock));

	as->lock_counter--;
}

static int
ac97_query_devinfo(struct ac97_codec_if *codec_if, mixer_devinfo_t *dip)
{
	struct ac97_softc *as;
	struct ac97_source_info *si;
	const char *name;

	as = (struct ac97_softc *)codec_if;
	if (dip->index < as->num_source_info) {
		si = &as->source_info[dip->index];
		dip->type = si->type;
		dip->mixer_class = si->mixer_class;
		dip->prev = si->prev;
		dip->next = si->next;

		if (si->qualifier)
			name = si->qualifier;
		else if (si->device)
			name = si->device;
		else if (si->class)
			name = si->class;
		else
			name = 0;

		if (name)
			strcpy(dip->label.name, name);

		memcpy(&dip->un, si->info, si->info_size);

		/* Set the delta for volume sources */
		if (dip->type == AUDIO_MIXER_VALUE)
			dip->un.v.delta = 1 << (8 - si->bits);

		return 0;
	}

	return ENXIO;
}

static int
ac97_mixer_set_port(struct ac97_codec_if *codec_if, mixer_ctrl_t *cp)
{
	struct ac97_softc *as;
	struct ac97_source_info *si;
	uint16_t mask;
	uint16_t val, newval;
	int error;
	bool spdif;

	as = (struct ac97_softc *)codec_if;

	KASSERT(mutex_owned(as->lock));

	if (cp->dev < 0 || cp->dev >= as->num_source_info)
		return EINVAL;
	si = &as->source_info[cp->dev];

	if (cp->type == AUDIO_MIXER_CLASS || cp->type != si->type)
		return EINVAL;
	spdif = si->req_feature == CHECK_SPDIF && si->reg == AC97_REG_EXT_AUDIO_CTRL;
	if (spdif && as->lock_counter >= 0) {
		/* When the value of lock_counter is the default 0,
		 * it is not allowed to change the SPDIF mode. */
		return EBUSY;
	}

	ac97_read(as, si->reg, &val);

	DPRINTFN(5, ("read(%x) = %x\n", si->reg, val));

	mask = (1 << si->bits) - 1;

	switch (cp->type) {
	case AUDIO_MIXER_ENUM:
		if (cp->un.ord > mask || cp->un.ord < 0)
			return EINVAL;

		newval = (cp->un.ord << si->ofs);
		if (si->reg == AC97_REG_RECORD_SELECT) {
			newval |= (newval << (8 + si->ofs));
			mask |= (mask << 8);
			mask = mask << si->ofs;
		} else if (si->reg == AC97_REG_SURR_MASTER) {
			newval = cp->un.ord ? 0x8080 : 0x0000;
			mask = 0x8080;
		} else
			mask = mask << si->ofs;
		break;
	case AUDIO_MIXER_VALUE:
	{
		const struct audio_mixer_value *value = si->info;
		uint16_t  l, r, ol, or;
		int deltal, deltar;

		if ((cp->un.value.num_channels <= 0) ||
		    (cp->un.value.num_channels > value->num_channels))
			return EINVAL;

		if (cp->un.value.num_channels == 1) {
			l = r = cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		} else {
			if (!(as->host_flags & AC97_HOST_SWAPPED_CHANNELS)) {
				l = cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
				r = cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
			} else {	/* left/right is reversed here */
				r = cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
				l = cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
			}

		}

		if (!si->polarity) {
			l = 255 - l;
			r = 255 - r;
		}

		ol = (val >> (8+si->ofs)) & mask;
		or = (val >> si->ofs) & mask;

		deltal = (ol << (8 - si->bits)) - l;
		deltar = (or << (8 - si->bits)) - r;

		l = l >> (8 - si->bits);
		r = r >> (8 - si->bits);

		if (deltal && ol == l)
			l += (deltal > 0) ? (l ? -1 : 0) : (l < mask ? 1 : 0);
		if (deltar && or == r)
			r += (deltar > 0) ? (r ? -1 : 0) : (r < mask ? 1 : 0);

		newval = ((r & mask) << si->ofs);
		if (value->num_channels == 2) {
			newval = newval | ((l & mask) << (si->ofs+8));
			mask |= (mask << 8);
		}
		mask = mask << si->ofs;
		break;
	}
	default:
		return EINVAL;
	}

	error = ac97_write(as, si->reg, (val & ~mask) | newval);
	if (error)
		return error;

	if (spdif && as->host_if->spdif_event != NULL) {
		DPRINTF(("%s: call spdif_event(%d)\n", __func__, cp->un.ord));
		as->host_if->spdif_event(as->host_if->arg, cp->un.ord);
	}
	return 0;
}

static int
ac97_get_portnum_by_name(struct ac97_codec_if *codec_if, const char *class,
			 const char *device, const char *qualifier)
{
	struct ac97_softc *as;
	int idx;

	as = (struct ac97_softc *)codec_if;

	KASSERT(mutex_owned(as->lock));

	for (idx = 0; idx < as->num_source_info; idx++) {
		struct ac97_source_info *si = &as->source_info[idx];
		if (ac97_str_equal(class, si->class) &&
		    ac97_str_equal(device, si->device) &&
		    ac97_str_equal(qualifier, si->qualifier))
			return idx;
	}

	return -1;
}

static int
ac97_mixer_get_port(struct ac97_codec_if *codec_if, mixer_ctrl_t *cp)
{
	struct ac97_softc *as;
	struct ac97_source_info *si;
	uint16_t mask;
	uint16_t val;

	as = (struct ac97_softc *)codec_if;

	KASSERT(mutex_owned(as->lock));

	si = &as->source_info[cp->dev];
	if (cp->dev < 0 || cp->dev >= as->num_source_info)
		return EINVAL;

	if (cp->type != si->type)
		return EINVAL;

	ac97_read(as, si->reg, &val);

	DPRINTFN(5, ("read(%x) = %x\n", si->reg, val));

	mask = (1 << si->bits) - 1;

	switch (cp->type) {
	case AUDIO_MIXER_ENUM:
		cp->un.ord = (val >> si->ofs) & mask;
		DPRINTFN(4, ("AUDIO_MIXER_ENUM: %x %d %x %d\n",
			     val, si->ofs, mask, cp->un.ord));
		break;
	case AUDIO_MIXER_VALUE:
	{
		const struct audio_mixer_value *value = si->info;
		uint16_t  l, r;

		if ((cp->un.value.num_channels <= 0) ||
		    (cp->un.value.num_channels > value->num_channels))
			return EINVAL;

		if (value->num_channels == 1) {
			l = r = (val >> si->ofs) & mask;
		} else {
			if (!(as->host_flags & AC97_HOST_SWAPPED_CHANNELS)) {
				l = (val >> (si->ofs + 8)) & mask;
				r = (val >> si->ofs) & mask;
			} else {	/* host has reversed channels */
				r = (val >> (si->ofs + 8)) & mask;
				l = (val >> si->ofs) & mask;
			}
		}

		l = (l << (8 - si->bits));
		r = (r << (8 - si->bits));
		if (!si->polarity) {
			l = 255 - l;
			r = 255 - r;
		}

		/* The EAP driver averages l and r for stereo
		   channels that are requested in MONO mode. Does this
		   make sense? */
		if (cp->un.value.num_channels == 1) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] = l;
		} else if (cp->un.value.num_channels == 2) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] = l;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = r;
		}

		break;
	}
	default:
		return EINVAL;
	}

	return 0;
}


static int
ac97_set_rate(struct ac97_codec_if *codec_if, int target, u_int *rate)
{
	struct ac97_softc *as;
	u_int value;
	uint16_t ext_stat;
	uint16_t actual;
	uint16_t power;
	uint16_t power_bit;

	as = (struct ac97_softc *)codec_if;

	KASSERT(mutex_owned(as->lock));

	if (target == AC97_REG_PCM_MIC_ADC_RATE) {
		if (!(as->ext_id & AC97_EXT_AUDIO_VRM)) {
			*rate = AC97_SINGLE_RATE;
			return 0;
		}
	} else {
		if (!(as->ext_id & AC97_EXT_AUDIO_VRA)) {
			*rate = AC97_SINGLE_RATE;
			return 0;
		}
	}
	value = *rate * AC97_STANDARD_CLOCK / as->ac97_clock;
	ext_stat = 0;
	/*
	 * PCM_FRONT_DAC_RATE/PCM_SURR_DAC_RATE/PCM_LFE_DAC_RATE
	 *	Check VRA, DRA
	 * PCM_LR_ADC_RATE
	 *	Check VRA
	 * PCM_MIC_ADC_RATE
	 *	Check VRM
	 */
	switch (target) {
	case AC97_REG_PCM_FRONT_DAC_RATE:
	case AC97_REG_PCM_SURR_DAC_RATE:
	case AC97_REG_PCM_LFE_DAC_RATE:
		power_bit = AC97_POWER_OUT;
		if (!(as->ext_id & AC97_EXT_AUDIO_VRA)) {
			*rate = AC97_SINGLE_RATE;
			return 0;
		}
		if (as->ext_id & AC97_EXT_AUDIO_DRA) {
			ac97_read(as, AC97_REG_EXT_AUDIO_CTRL, &ext_stat);
			if (value > 0x1ffff) {
				return EINVAL;
			} else if (value > 0xffff) {
				/* Enable DRA */
				ext_stat |= AC97_EXT_AUDIO_DRA;
				ac97_write(as, AC97_REG_EXT_AUDIO_CTRL, ext_stat);
				value /= 2;
			} else {
				/* Disable DRA */
				ext_stat &= ~AC97_EXT_AUDIO_DRA;
				ac97_write(as, AC97_REG_EXT_AUDIO_CTRL, ext_stat);
			}
		} else {
			if (value > 0xffff)
				return EINVAL;
		}
		break;
	case AC97_REG_PCM_LR_ADC_RATE:
		power_bit = AC97_POWER_IN;
		if (!(as->ext_id & AC97_EXT_AUDIO_VRA)) {
			*rate = AC97_SINGLE_RATE;
			return 0;
		}
		if (value > 0xffff)
			return EINVAL;
		break;
	case AC97_REG_PCM_MIC_ADC_RATE:
		power_bit = AC97_POWER_IN;
		if (!(as->ext_id & AC97_EXT_AUDIO_VRM)) {
			*rate = AC97_SINGLE_RATE;
			return 0;
		}
		if (value > 0xffff)
			return EINVAL;
		break;
	default:
		printf("%s: Unknown register: 0x%x\n", __func__, target);
		return EINVAL;
	}

	ac97_read(as, AC97_REG_POWER, &power);
	ac97_write(as, AC97_REG_POWER, power | power_bit);

	ac97_write(as, target, (uint16_t)value);
	ac97_read(as, target, &actual);
	actual = (uint32_t)actual * as->ac97_clock / AC97_STANDARD_CLOCK;

	ac97_write(as, AC97_REG_POWER, power);
	if (ext_stat & AC97_EXT_AUDIO_DRA) {
		*rate = actual * 2;
	} else {
		*rate = actual;
	}
	return 0;
}

static void
ac97_set_clock(struct ac97_codec_if *codec_if, unsigned int clock)
{
	struct ac97_softc *as;

	as = (struct ac97_softc *)codec_if;

	KASSERT(mutex_owned(as->lock));

	as->ac97_clock = clock;
}

static uint16_t
ac97_get_extcaps(struct ac97_codec_if *codec_if)
{
	struct ac97_softc *as;

	as = (struct ac97_softc *)codec_if;

	KASSERT(mutex_owned(as->lock));

	return as->ext_id;
}

static int
ac97_add_port(struct ac97_softc *as, const struct ac97_source_info *src)
{
	struct ac97_source_info *si;
	int ouridx, idx;

	KASSERT(mutex_owned(as->lock));

	if ((as->type == AC97_CODEC_TYPE_AUDIO &&
	     as->num_source_info >= AUDIO_MAX_SOURCES) ||
	    (as->type == AC97_CODEC_TYPE_MODEM &&
	     as->num_source_info >= MODEM_MAX_SOURCES)) {
		printf("%s: internal error: increase MAX_SOURCES in %s\n",
		       __func__, __FILE__);
		return -1;
	}
	if (!ac97_check_capability(as, src->req_feature))
		return -1;
	ouridx = as->num_source_info;
	si = &as->source_info[ouridx];
	memcpy(si, src, sizeof(*si));

	switch (si->type) {
	case AUDIO_MIXER_CLASS:
	case AUDIO_MIXER_VALUE:
		printf("%s: adding class/value is not supported yet.\n",
		       __func__);
		return -1;
	case AUDIO_MIXER_ENUM:
		break;
	default:
		printf("%s: unknown type: %d\n", __func__, si->type);
		return -1;
	}
	as->num_source_info++;

	si->mixer_class = ac97_get_portnum_by_name(&as->codec_if, si->class,
						   NULL, NULL);
	/* Find the root of the device */
	idx = ac97_get_portnum_by_name(&as->codec_if, si->class,
				       si->device, NULL);
	/* Find the last item */
	while (as->source_info[idx].next != AUDIO_MIXER_LAST)
		idx = as->source_info[idx].next;
	/* Append */
	as->source_info[idx].next = ouridx;
	si->prev = idx;
	si->next = AUDIO_MIXER_LAST;

	return 0;
}

/**
 * Codec-dependent initialization
 */

#define	AD1980_REG_MISC	0x76
#define		AD1980_MISC_MBG0	0x0001	/* 0 1888/1980/1981 /1985 */
#define		AD1980_MISC_MBG1	0x0002	/* 1 1888/1980/1981 /1985 */
#define		AD1980_MISC_VREFD	0x0004	/* 2 1888/1980/1981 /1985 */
#define		AD1980_MISC_VREFH	0x0008	/* 3 1888/1980/1981 /1985 */
#define		AD1980_MISC_SRU		0x0010	/* 4 1888/1980      /1985 */
#define		AD1980_MISC_LOSEL	0x0020	/* 5 1888/1980/1981 /1985 */
#define		AD1980_MISC_2CMIC	0x0040	/* 6      1980/1981B/1985 */
#define		AD1980_MISC_SPRD	0x0080	/* 7 1888/1980      /1985 */
#define		AD1980_MISC_DMIX0	0x0100	/* 8 1888/1980      /1985 */
#define		AD1980_MISC_DMIX1	0x0200	/* 9 1888/1980      /1985 */
#define		AD1980_MISC_HPSEL	0x0400	/*10 1888/1980      /1985 */
#define		AD1980_MISC_CLDIS	0x0800	/*11 1888/1980      /1985 */
#define		AD1980_MISC_LODIS	0x1000	/*12 1888/1980/1981 /1985 */
#define		AD1980_MISC_MSPLT	0x2000	/*13 1888/1980/1981 /1985 */
#define		AD1980_MISC_AC97NC	0x4000	/*14 1888/1980      /1985 */
#define		AD1980_MISC_DACZ	0x8000	/*15 1888/1980/1981 /1985 */
#define	AD1981_REG_MISC	0x76
#define		AD1981_MISC_MADST	0x0010  /* 4 */
#define		AD1981A_MISC_MADPD	0x0040  /* 6 */
#define		AD1981B_MISC_MADPD	0x0080  /* 7 */
#define		AD1981_MISC_FMXE	0x0200  /* 9 */
#define		AD1981_MISC_DAM		0x0800  /*11 */
static void
ac97_ad198x_init(struct ac97_softc *as)
{
	int i;
	uint16_t misc;

	KASSERT(mutex_owned(as->lock));

	ac97_read(as, AD1980_REG_MISC, &misc);
	ac97_write(as, AD1980_REG_MISC,
		   misc | AD1980_MISC_LOSEL | AD1980_MISC_HPSEL);

	for (i = 0; i < as->num_source_info; i++) {
		if (as->source_info[i].type != AUDIO_MIXER_VALUE)
			continue;

		if (as->source_info[i].reg == AC97_REG_MASTER_VOLUME)
			as->source_info[i].reg = AC97_REG_SURR_MASTER;
		else if (as->source_info[i].reg == AC97_REG_SURR_MASTER)
			as->source_info[i].reg = AC97_REG_MASTER_VOLUME;
	}
}

#define ALC650_REG_MULTI_CHANNEL_CONTROL	0x6a
#define		ALC650_MCC_SLOT_MODIFY_MASK		0xc000
#define		ALC650_MCC_FRONTDAC_FROM_SPDIFIN	0x2000 /* 13 */
#define		ALC650_MCC_SPDIFOUT_FROM_ADC		0x1000 /* 12 */
#define		ALC650_MCC_PCM_FROM_SPDIFIN		0x0800 /* 11 */
#define		ALC650_MCC_MIC_OR_CENTERLFE		0x0400 /* 10 */
#define		ALC650_MCC_LINEIN_OR_SURROUND		0x0200 /* 9 */
#define		ALC650_MCC_INDEPENDENT_MASTER_L		0x0080 /* 7 */
#define		ALC650_MCC_INDEPENDENT_MASTER_R		0x0040 /* 6 */
#define		ALC650_MCC_ANALOG_TO_CENTERLFE		0x0020 /* 5 */
#define		ALC650_MCC_ANALOG_TO_SURROUND		0x0010 /* 4 */
#define		ALC650_MCC_EXCHANGE_CENTERLFE		0x0008 /* 3 */
#define		ALC650_MCC_CENTERLFE_DOWNMIX		0x0004 /* 2 */
#define		ALC650_MCC_SURROUND_DOWNMIX		0x0002 /* 1 */
#define		ALC650_MCC_LINEOUT_TO_SURROUND		0x0001 /* 0 */
static void
ac97_alc650_init(struct ac97_softc *as)
{
	static const struct ac97_source_info sources[6] = {
		{ AudioCoutputs, AudioNsurround, "lineinjack",
		  AUDIO_MIXER_ENUM, WRAP(ac97_on_off),
		  ALC650_REG_MULTI_CHANNEL_CONTROL,
		  0x0000, 1, 9, 0, 0, 0, CHECK_SURROUND, 0, 0, 0, },
		{ AudioCoutputs, AudioNsurround, "mixtofront",
		  AUDIO_MIXER_ENUM, WRAP(ac97_on_off),
		  ALC650_REG_MULTI_CHANNEL_CONTROL,
		  0x0000, 1, 1, 0, 0, 0, CHECK_SURROUND, 0, 0, 0, },
		{ AudioCoutputs, AudioNcenter, "micjack",
		  AUDIO_MIXER_ENUM, WRAP(ac97_on_off),
		  ALC650_REG_MULTI_CHANNEL_CONTROL,
		  0x0000, 1, 10, 0, 0, 0, CHECK_CENTER, 0, 0, 0, },
		{ AudioCoutputs, AudioNlfe, "micjack",
		  AUDIO_MIXER_ENUM, WRAP(ac97_on_off),
		  ALC650_REG_MULTI_CHANNEL_CONTROL,
		  0x0000, 1, 10, 0, 0, 0, CHECK_LFE, 0, 0, 0, },
		{ AudioCoutputs, AudioNcenter, "mixtofront",
		  AUDIO_MIXER_ENUM, WRAP(ac97_on_off),
		  ALC650_REG_MULTI_CHANNEL_CONTROL,
		  0x0000, 1, 2, 0, 0, 0, CHECK_CENTER, 0, 0, 0, },
		{ AudioCoutputs, AudioNlfe, "mixtofront",
		  AUDIO_MIXER_ENUM, WRAP(ac97_on_off),
		  ALC650_REG_MULTI_CHANNEL_CONTROL,
		  0x0000, 1, 2, 0, 0, 0, CHECK_LFE, 0, 0, 0, },
	};

	ac97_add_port(as, &sources[0]);
	ac97_add_port(as, &sources[1]);
	ac97_add_port(as, &sources[2]);
	ac97_add_port(as, &sources[3]);
	ac97_add_port(as, &sources[4]);
	ac97_add_port(as, &sources[5]);
}

#define UCB1400_REG_FEATURE_CSR1	0x6a
#define		UCB1400_BB(bb)			(((bb) & 0xf) << 11)
#define		UCB1400_TR(tr)			(((tr) & 0x3) << 9)
#define		UCB1400_M_MAXIMUM		(3 << 7)
#define		UCB1400_M_MINIMUM		(1 << 7)
#define		UCB1400_M_FLAT			(0 << 7)
#define		UCB1400_HPEN			(1 << 6)
#define		UCB1400_DE			(1 << 5)
#define		UCB1400_DC			(1 << 4)
#define		UCB1400_HIPS			(1 << 3)
#define		UCB1400_GIEN			(1 << 2)
#define		UCB1400_OVFL			(1 << 0)
#define UCB1400_REG_FEATURE_CSR2	0x6c
#define		UCB1400_SMT			(1 << 15)	/* Must be 0 */
#define		UCB1400_SUEV1			(1 << 14)	/* Must be 0 */
#define		UCB1400_SUEV0			(1 << 13)	/* Must be 0 */
#define		UCB1400_AVE			(1 << 12)
#define		UCB1400_AVEN1			(1 << 11)	/* Must be 0 */
#define		UCB1400_AVEN0			(1 << 10)	/* Must be 0 */
#define		UCB1400_SLP_ON			\
					(UCB1400_SLP_PLL | UCB1400_SLP_CODEC)
#define		UCB1400_SLP_PLL			(2 << 4)
#define		UCB1400_SLP_CODEC		(1 << 4)
#define		UCB1400_SLP_NO			(0 << 4)
#define		UCB1400_EV2			(1 << 2)	/* Must be 0 */
#define		UCB1400_EV1			(1 << 1)	/* Must be 0 */
#define		UCB1400_EV0			(1 << 0)	/* Must be 0 */
static void
ac97_ucb1400_init(struct ac97_softc *as)
{

	ac97_write(as, UCB1400_REG_FEATURE_CSR1,
	    UCB1400_HPEN | UCB1400_DC | UCB1400_HIPS | UCB1400_OVFL);
	ac97_write(as, UCB1400_REG_FEATURE_CSR2, UCB1400_AVE | UCB1400_SLP_ON);
}

#define VT1616_REG_IO_CONTROL	0x5a
#define		VT1616_IC_LVL			(1 << 15)
#define		VT1616_IC_LFECENTER_TO_FRONT	(1 << 12)
#define		VT1616_IC_SURROUND_TO_FRONT	(1 << 11)
#define		VT1616_IC_BPDC			(1 << 10)
#define		VT1616_IC_DC			(1 << 9)
#define		VT1616_IC_IB_MASK		0x000c
static void
ac97_vt1616_init(struct ac97_softc *as)
{
	static const struct ac97_source_info sources[3] = {
		{ AudioCoutputs, AudioNsurround, "mixtofront",
		  AUDIO_MIXER_ENUM, WRAP(ac97_on_off),
		  VT1616_REG_IO_CONTROL,
		  0x0000, 1, 11, 0, 0, 0, CHECK_SURROUND, 0, 0, 0, },
		{ AudioCoutputs, AudioNcenter, "mixtofront",
		  AUDIO_MIXER_ENUM, WRAP(ac97_on_off),
		  VT1616_REG_IO_CONTROL,
		  0x0000, 1, 12, 0, 0, 0, CHECK_CENTER, 0, 0, 0, },
		{ AudioCoutputs, AudioNlfe, "mixtofront",
		  AUDIO_MIXER_ENUM, WRAP(ac97_on_off),
		  VT1616_REG_IO_CONTROL,
		  0x0000, 1, 12, 0, 0, 0, CHECK_LFE, 0, 0, 0, },
	};

	KASSERT(mutex_owned(as->lock));

	ac97_add_port(as, &sources[0]);
	ac97_add_port(as, &sources[1]);
	ac97_add_port(as, &sources[2]);
}

static int
ac97_modem_offhook_set(struct ac97_softc *as, int line, int newval)
{
	uint16_t val;

	KASSERT(mutex_owned(as->lock));

	val = as->shadow_reg[AC97_REG_GPIO_STATUS >> 1];
	switch (newval) {
	case 0:
		val &= ~line;
		break;
	case 1:
		val |= line;
		break;
	}
	ac97_write(as, AC97_REG_GPIO_STATUS, val);

	return 0;
}

static int
ac97_sysctl_verify(SYSCTLFN_ARGS)
{
	int error, tmp;
	struct sysctlnode node;
	struct ac97_softc *as;

	node = *rnode;
	as = rnode->sysctl_data;
	if (node.sysctl_num == as->offhook_line1_mib) {
		tmp = as->offhook_line1;
		node.sysctl_data = &tmp;
		error = sysctl_lookup(SYSCTLFN_CALL(&node));
		if (error || newp == NULL)
			return error;

		if (tmp < 0 || tmp > 1)
			return EINVAL;

		as->offhook_line1 = tmp;
		mutex_enter(as->lock);
		ac97_modem_offhook_set(as, AC97_GPIO_LINE1_OH, tmp);
		mutex_exit(as->lock);
	} else if (node.sysctl_num == as->offhook_line2_mib) {
		tmp = as->offhook_line2;
		node.sysctl_data = &tmp;
		error = sysctl_lookup(SYSCTLFN_CALL(&node));
		if (error || newp == NULL)
			return error;

		if (tmp < 0 || tmp > 1)
			return EINVAL;

		as->offhook_line2 = tmp;
		mutex_enter(as->lock);
		ac97_modem_offhook_set(as, AC97_GPIO_LINE2_OH, tmp);
		mutex_exit(as->lock);
	}

	return 0;
}

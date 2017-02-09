/*	$NetBSD: ymvar.h,v 1.13 2011/11/23 23:07:33 jmcneill Exp $	*/

/*-
 * Copyright (c) 1999-2000, 2002, 2008 The NetBSD Foundation, Inc.
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

#include <sys/callout.h>

/*
 *  Original code from OpenBSD.
 */

/*
 * Mixer devices
 */
#define YM_DAC_LVL		0	/* inputs.dac */
#define YM_MIDI_LVL		1	/* inputs.midi */
#define YM_CD_LVL		2	/* inputs.cd */
#define YM_LINE_LVL		3	/* inputs.line */
#define YM_SPEAKER_LVL		4	/* inputs.speaker */
#define YM_MIC_LVL		5	/* inputs.mic */
#define YM_MONITOR_LVL		6	/* monitor.monitor */
#define YM_DAC_MUTE		7	/* inputs.dac.mute */
#define YM_MIDI_MUTE		8	/* inputs.midi.mute */
#define YM_CD_MUTE		9	/* inputs.cd.mute */
#define YM_LINE_MUTE		10	/* inputs.line.mute */
#define YM_SPEAKER_MUTE		11	/* inputs.speaker.mute */
#define YM_MIC_MUTE		12	/* inputs.mic.mute */
#define YM_MONITOR_MUTE		13	/* monitor.monitor.mute */

#define YM_REC_LVL		14	/* record.record */
#define YM_RECORD_SOURCE	15	/* record.record.source */

#define YM_OUTPUT_LVL		16	/* outputs.master */
#define YM_OUTPUT_MUTE		17	/* outputs.master.mute */

#ifndef AUDIO_NO_POWER_CTL
#define YM_PWR_MODE		18	/* power.save */
#define YM_PWR_TIMEOUT		19	/* power.save.timeout */
#endif

/* Classes - don't change this without looking at mixer_classes array */
#ifdef AUDIO_NO_POWER_CTL
#define YM_CLASS_TOP		18
#else
#define YM_CLASS_TOP		20
#endif
#define YM_INPUT_CLASS		(YM_CLASS_TOP + 0)
#define YM_RECORD_CLASS		(YM_CLASS_TOP + 1)
#define YM_OUTPUT_CLASS		(YM_CLASS_TOP + 2)
#define YM_MONITOR_CLASS	(YM_CLASS_TOP + 3)
#ifdef AUDIO_NO_POWER_CTL
#define YM_EQ_CLASS		(YM_CLASS_TOP + 4)
#else
#define YM_PWR_CLASS		(YM_CLASS_TOP + 4)
#define YM_EQ_CLASS		(YM_CLASS_TOP + 5)
#endif

/* equalizer is SA3 only */
#define YM_MASTER_EQMODE	(YM_EQ_CLASS+1)	/* equalization.mode */
#define YM_MASTER_TREBLE	(YM_EQ_CLASS+2)	/* equalization.treble */
#define YM_MASTER_BASS		(YM_EQ_CLASS+3)	/* equalization.bass */
#define YM_MASTER_WIDE		(YM_EQ_CLASS+4)	/* equalization.surround */

#define YM_MIXER_SA3_ONLY(m)	((m) >= YM_EQ_CLASS)

/* XXX should be in <sys/audioio.h> */
#define AudioNdesktop		"desktop"
#define AudioNlaptop		"laptop"
#define AudioNsubnote		"subnote"
#define AudioNhifi		"hifi"

#ifndef AUDIO_NO_POWER_CTL
#define AudioCpower		"power"
#define AudioNsave		"save"
#define AudioNtimeout		"timeout"
#define AudioNpowerdown		"powerdown"
#define AudioNpowersave		"powersave"
#define AudioNnosave		"nosave"
#endif


struct ym_softc {
	struct	ad1848_isa_softc sc_ad1848;
#define ym_irq		sc_ad1848.sc_irq
#define ym_playdrq	sc_ad1848.sc_playdrq
#define ym_recdrq	sc_ad1848.sc_recdrq

	bus_space_tag_t sc_iot;		/* tag */
	bus_space_handle_t sc_ioh;	/* handle */
	isa_chipset_tag_t sc_ic;

	bus_space_handle_t sc_controlioh;
	bus_space_handle_t sc_opl_ioh;
	bus_space_handle_t sc_sb_ioh;	/* only used to disable it */

	callout_t sc_powerdown_ch;
	kcondvar_t sc_cv;

	int  master_mute, mic_mute;
	struct ad1848_volume master_gain;
	uint8_t mic_gain;

	uint8_t sc_external_sources;	/* non-zero value prevents power down */

	uint8_t sc_version;		/* hardware version */
#define YM_IS_SA3(sc)	((sc)->sc_version > SA3_MISC_VER_711)

	/* 3D encehamcement */
	uint8_t sc_eqmode;
	struct ad1848_volume sc_treble, sc_bass, sc_wide;
	/*
	 * The equalizer of OPL3-SA3 is ``flat'' if it is turned off.
	 * For compatibility with other drivers, however, make it flat
	 * if it is set at center (or smaller).
	 */
#define YM_EQ_REDUCE_BIT	1	/* use only 128 values from 256 */
#define YM_EQ_FLAT_OFFSET	128	/* center */
#define YM_EQ_EXPAND_VALUE(v)	\
    ((v) < YM_EQ_FLAT_OFFSET? 0 : ((v) - YM_EQ_FLAT_OFFSET) << YM_EQ_REDUCE_BIT)

#define YM_3D_ON_MIN	((AUDIO_MAX_GAIN + 1) / (SA3_3D_BITS + 1))
#define YM_EQ_ON_MIN	((YM_3D_ON_MIN >> YM_EQ_REDUCE_BIT) + YM_EQ_FLAT_OFFSET)

#define YM_EQ_OFF(v)	((v)->left < YM_EQ_ON_MIN && (v)->right < YM_EQ_ON_MIN)
#define YM_WIDE_OFF(v)	((v)->left < YM_3D_ON_MIN && (v)->right < YM_3D_ON_MIN)

	device_t sc_audiodev;

#if NMPU_YM > 0
	bus_space_handle_t sc_mpu_ioh;
	device_t sc_mpudev;
#endif

#ifndef AUDIO_NO_POWER_CTL
	enum ym_pow_mode {
		YM_POWER_POWERDOWN, YM_POWER_POWERSAVE, YM_POWER_NOSAVE
	} sc_pow_mode;
	int	sc_pow_timeout;

	uint8_t sc_codec_scan[0x20];
#define YM_SAVE_REG_MAX_SA3	SA3_HVOL_INTR_CNF
#define YM_SAVE_REG_MAX_SA2	SA3_DMA_CNT_REC_HIGH
	uint8_t sc_sa3_scan[YM_SAVE_REG_MAX_SA3 + 1];

	uint16_t sc_on_blocks;
	uint16_t sc_turning_off;

	int	sc_in_power_ctl;
#define YM_POWER_CTL_INUSE	1
#define YM_POWER_CTL_WANTED	2
#endif /* not AUDIO_NO_POWER_CTL */
};

#ifndef AUDIO_NO_POWER_CTL
/* digital */
#define YM_POWER_CODEC_P	SA3_DPWRDWN_WSS_P
#define YM_POWER_CODEC_R	SA3_DPWRDWN_WSS_R
#define YM_POWER_OPL3		SA3_DPWRDWN_FM
#define YM_POWER_MPU401		SA3_DPWRDWN_MPU
#define YM_POWER_JOYSTICK	SA3_DPWRDWN_JOY
/* analog */
#define YM_POWER_3D		(SA3_APWRDWN_WIDE << 8)
#define YM_POWER_CODEC_DA	(SA3_APWRDWN_DA << 8)
#define YM_POWER_CODEC_AD	(SA3_APWRDWN_AD << 8)
#define YM_POWER_OPL3_DA	(SA3_APWRDWN_FMDAC << 8)
/* pseudo */
#define YM_POWER_CODEC_CTL	0x4000
#define YM_POWER_EXT_SRC	0x8000
#define YM_POWER_CODEC_PSEUDO	(YM_POWER_CODEC_CTL | YM_POWER_EXT_SRC)

#define YM_POWER_CODEC_DIGITAL	\
		(YM_POWER_CODEC_P | YM_POWER_CODEC_R | YM_POWER_CODEC_CTL)
/* 3D enhance is passive */
#define YM_POWER_ACTIVE		(0xffff & ~YM_POWER_3D)

/* external input sources */
#define YM_XS_CD	1
#define YM_XS_LINE	2
#define YM_XS_SPEAKER	4
#define YM_XS_MIC	8

#if YM_CD_MUTE + 1 != YM_LINE_MUTE || YM_CD_MUTE + 2 != YM_SPEAKER_MUTE || YM_CD_MUTE + 3 != YM_MIC_MUTE
 #error YM_CD_MUTE, YM_LINE_MUTE and YM_SPEAKER_MUTE should be contiguous
#endif
#define YM_MIXER_TO_XS(m)	(1 << ((m) - YM_CD_MUTE))

#ifdef _KERNEL
void	ym_power_ctl(struct ym_softc *, int, int);
#endif
#endif /* not AUDIO_NO_POWER_CTL */

#ifdef _KERNEL
void	ym_attach(struct ym_softc *);
#endif

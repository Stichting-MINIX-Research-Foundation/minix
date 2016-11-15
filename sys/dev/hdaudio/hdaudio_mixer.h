/* $NetBSD: hdaudio_mixer.h,v 1.1 2015/03/28 14:09:59 jmcneill Exp $ */

/*
 * Copyright (c) 2009 Precedence Technologies Ltd <support@precedence.co.uk>
 * Copyright (c) 2009 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Precedence Technologies Ltd
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _HDAUDIO_MIXER_H
#define _HDAUDIO_MIXER_H

/* From src/lib/libossaudio/soundcard.h */

#define	HDAUDIO_MIXER_NRDEVICES	25
#define	HDAUDIO_MIXER_VOLUME	0
#define	HDAUDIO_MIXER_BASS	1
#define	HDAUDIO_MIXER_TREBLE	2
#define	HDAUDIO_MIXER_SYNTH	3
#define	HDAUDIO_MIXER_PCM	4
#define	HDAUDIO_MIXER_SPEAKER	5
#define	HDAUDIO_MIXER_LINE	6
#define	HDAUDIO_MIXER_MIC	7
#define	HDAUDIO_MIXER_CD	8
#define	HDAUDIO_MIXER_IMIX	9
#define	HDAUDIO_MIXER_ALTPCM	10
#define	HDAUDIO_MIXER_RECLEV	11
#define	HDAUDIO_MIXER_IGAIN	12
#define	HDAUDIO_MIXER_OGAIN	13
#define	HDAUDIO_MIXER_LINE1	14
#define	HDAUDIO_MIXER_LINE2	15
#define	HDAUDIO_MIXER_LINE3	16
#define	HDAUDIO_MIXER_DIGITAL1	17
#define	HDAUDIO_MIXER_DIGITAL2	18
#define	HDAUDIO_MIXER_DIGITAL3	19
#define	HDAUDIO_MIXER_PHONEIN	20
#define	HDAUDIO_MIXER_PHONEOUT	21
#define	HDAUDIO_MIXER_VIDEO	22
#define	HDAUDIO_MIXER_RADIO	23
#define	HDAUDIO_MIXER_MONITOR	24

#define	HDAUDIO_MASK(x)		(1 << (HDAUDIO_MIXER_##x))

#define	HDAUDIO_DEVICE_NAMES	{					   \
	AudioNmaster, AudioNbass, AudioNtreble, AudioNfmsynth, AudioNdac,  \
	"beep", AudioNline, AudioNmicrophone, AudioNcd,		   	   \
	AudioNrecord, AudioNdac"2", "reclvl", AudioNinput, AudioNoutput,   \
	AudioNline"1", AudioNline"2", AudioNline"3",			   \
	"dig1", "dig2", "dig3", "phin", "phout",			   \
	AudioNvideo, "radio", AudioNmonitor				   \
				}

#endif /* !_HDAUDIO_MIXER_H */

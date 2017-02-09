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
#ifndef _DEV_IC_UDA1341VAR_H_
#define _DEV_IC_UDA1341VAR_H_

#include <sys/device.h>
#include <sys/audioio.h>

#include <dev/auconv.h>

#define UDA1341_NFORMATS 4
extern const struct audio_format uda1341_formats[UDA1341_NFORMATS];

struct uda1341_softc {
	/* Pointer to the driver that holds this sc */
	void		*parent;

	/* Pointer to L3 write function */
	void		(*sc_l3_write)(void *,int,int);

	/* Approximate sample rate at which the codec is currently running */
	int		sc_sample_rate_approx;

	int		sc_system_clock;
#define UDA1341_CLOCK_NA  3
#define UDA1341_CLOCK_256 2
#define UDA1341_CLOCK_384 1
#define UDA1341_CLOCK_512 0

	int		sc_bus_format;
#define UDA1341_BUS_I2S		0
#define UDA1341_BUS_LSB16	1
#define UDA1341_BUS_LSB18	2
#define UDA1341_BUS_LSB20	3
#define UDA1341_BUS_MSB		4
#define UDA1341_BUS_LSB16_MSB	5
#define UDA1341_BUS_LSB18_MSB	6
#define UDA1341_BUS_LSB20_MSB	7

	uint8_t		sc_volume;
	uint8_t		sc_bass;
	uint8_t		sc_treble;
	uint8_t		sc_mode;
	uint8_t		sc_mute;
	uint8_t		sc_ogain;
	uint8_t		sc_dac_power;
	uint8_t		sc_adc_power;
	uint8_t		sc_inmix1;
	uint8_t		sc_inmix2;
	uint8_t		sc_micvol;
	uint8_t		sc_inmode;
	uint8_t		sc_agc;
	uint8_t		sc_agc_lvl;
	uint8_t		sc_ch2_gain;

#define UDA1341_DEEMPHASIS_AUTO	4
	uint8_t		sc_deemphasis;

};

int uda1341_attach(struct uda1341_softc *);
int uda1341_query_encodings(void *, audio_encoding_t *);
int uda1341_open(void *, int );
void uda1341_close(void *);
int uda1341_set_params(void *, int, int, audio_params_t*, audio_params_t*, stream_filter_list_t *, stream_filter_list_t*);
int uda1341_query_devinfo(void *, mixer_devinfo_t *);
int uda1341_get_port(void *, mixer_ctrl_t *);
int uda1341_set_port(void *, mixer_ctrl_t *);
#endif

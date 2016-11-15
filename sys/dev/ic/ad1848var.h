/*	$NetBSD: ad1848var.h,v 1.18 2011/11/23 23:07:32 jmcneill Exp $	*/

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


#define MUTE_LEFT       1
#define MUTE_RIGHT      2
#define MUTE_ALL	(MUTE_LEFT | MUTE_RIGHT)
#define MUTE_MONO       MUTE_ALL

#define WAVE_MUTE0	1		/* force mute (overrides UNMUTE1) */
#define WAVE_UNMUTE1	2		/* unmute (overrides MUTE2) */
#define WAVE_MUTE2	4		/* weak mute */
#define WAVE_MUTE2_INIT	0		/* init and MUTE2 */

/*
 * Don't change this ordering without seriously looking around.
 * These are indexes into mute[] array and into a register
 * information array.
 */
#define AD1848_AUX2_CHANNEL	0
#define AD1848_AUX1_CHANNEL	1
#define AD1848_DAC_CHANNEL	2
#define AD1848_LINE_CHANNEL	3
#define AD1848_MONO_CHANNEL	4
#define AD1848_OUT_CHANNEL	5
#define AD1848_MONITOR_CHANNEL	6 /* Doesn't seem to be on all later chips */

#define	AD1848_NUM_CHANNELS	7

struct ad1848_volume {
	u_char	left;
	u_char	right;
};

struct ad1848_softc {
	device_t sc_dev;		/* base device */
	kmutex_t sc_lock;
	kmutex_t sc_intr_lock;
	bus_space_tag_t sc_iot;		/* tag */
	bus_space_handle_t sc_ioh;	/* handle */

	int	(*sc_readreg)(struct ad1848_softc *, int);
	void	(*sc_writereg)(struct ad1848_softc *, int, int);
#define ADREAD(sc, index)		(*(sc)->sc_readreg)(sc, index)
#define ADWRITE(sc, index, data)	(*(sc)->sc_writereg)(sc, index, data)

	void	*parent;

	/* We keep track of these */
	struct ad1848_volume gains[AD1848_NUM_CHANNELS];
	struct ad1848_volume rec_gain;

	int	rec_port;		/* recording port */

	/* ad1848 */
	u_char	MCE_bit;
	char	mic_gain_on;		/* CS4231 only */
	char    mute[AD1848_NUM_CHANNELS];

	const char	*chip_name;
	int	mode;
	int	is_ad1845;

	u_int	precision;		/* 8/16 bits */
	int	channels;

	u_char	speed_bits;
	u_char	format_bits;
	u_char	need_commit;

	u_char	wave_mute_status;
	int	open_mode;
};

/*
 * Ad1848 registers.
 */
#define MIC_IN_PORT	0
#define LINE_IN_PORT	1
#define AUX1_IN_PORT	2
#define DAC_IN_PORT	3

#define AD1848_KIND_LVL		0
#define AD1848_KIND_MUTE	1
#define AD1848_KIND_RECORDGAIN	2
#define AD1848_KIND_MICGAIN	3
#define AD1848_KIND_RECORDSOURCE 4

typedef struct ad1848_devmap {
	int  id;
	int  kind;
	int  dev;
} ad1848_devmap_t;

#ifdef _KERNEL

int	ad_read(struct ad1848_softc *, int);
int	ad_xread(struct ad1848_softc *, int);
void	ad_write(struct ad1848_softc *, int, int);
void	ad_xwrite(struct ad1848_softc *, int, int);

void	ad1848_attach(struct ad1848_softc *);
void	ad1848_reset(struct ad1848_softc *);
int	ad1848_open(void *, int);
void	ad1848_close(void *);

int	ad1848_mixer_get_port(struct ad1848_softc *, const ad1848_devmap_t *,
	    int, mixer_ctrl_t *);
int	ad1848_mixer_set_port(struct ad1848_softc *, const ad1848_devmap_t *,
	    int, mixer_ctrl_t *);
int	ad1848_set_speed(struct ad1848_softc *, u_int *);
void	ad1848_mute_wave_output(struct ad1848_softc *, int, int);
int	ad1848_query_encoding(void *, struct audio_encoding *);
int	ad1848_set_params(void *, int, int, audio_params_t *,
	    audio_params_t *, stream_filter_list_t *,
	    stream_filter_list_t *);
int	ad1848_round_blocksize(void *, int, int, const audio_params_t *);
int	ad1848_commit_settings(void *);
int	ad1848_set_rec_port(struct ad1848_softc *, int);
int	ad1848_get_rec_port(struct ad1848_softc *);
int	ad1848_set_channel_gain(struct ad1848_softc *, int,
	    struct ad1848_volume *);
int	ad1848_get_device_gain(struct ad1848_softc *, int,
	    struct ad1848_volume *);
int	ad1848_set_rec_gain(struct ad1848_softc *, struct ad1848_volume *);
int	ad1848_get_rec_gain(struct ad1848_softc *, struct ad1848_volume *);
/* Note: The mic pre-MUX gain is not a variable gain, it's 20dB or 0dB */
int	ad1848_set_mic_gain(struct ad1848_softc *, struct ad1848_volume *);
int	ad1848_get_mic_gain(struct ad1848_softc *, struct ad1848_volume *);
void	ad1848_mute_channel(struct ad1848_softc *, int, int);
int	ad1848_to_vol(mixer_ctrl_t *, struct ad1848_volume *);
int	ad1848_from_vol(mixer_ctrl_t *, struct ad1848_volume *);
void	ad1848_init_locks(struct ad1848_softc *, int);
void	ad1848_destroy_locks(struct ad1848_softc *);

int	ad1848_halt_output(void *);
int	ad1848_halt_input(void *);
paddr_t	ad1848_mappage(void *, void *, off_t, int);
void	ad1848_get_locks(void *, kmutex_t **, kmutex_t **);

#ifdef AUDIO_DEBUG
void	ad1848_dump_regs(struct ad1848_softc *);
#endif

#endif

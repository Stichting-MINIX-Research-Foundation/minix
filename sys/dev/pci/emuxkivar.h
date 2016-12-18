/*	$NetBSD: emuxkivar.h,v 1.13 2011/11/23 23:07:35 jmcneill Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Yannick Montulet.
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

#ifndef _DEV_PCI_EMU10K1VAR_H_
#define _DEV_PCI_EMU10K1VAR_H_

#define	EMU_PCI_CBIO		0x10
#define EMU_SUBSYS_APS		0x40011102

/*
 * DMA memory management
 */

struct dmamem {
	bus_dma_tag_t   dmat;
	bus_size_t      size;
	bus_size_t      align;
	bus_size_t      bound;
	bus_dma_segment_t *segs;
	int		nsegs;
	int		rsegs;
	void *		kaddr;
	bus_dmamap_t    map;
};

#define	KERNADDR(ptr)		((void *)((ptr)->kaddr))
#define	DMASEGADDR(ptr, segno)	((ptr)->segs[segno].ds_addr)
#define	DMAADDR(ptr)		DMASEGADDR(ptr, 0)
#define DMASIZE(ptr)		((ptr)->size)

/*
 * Emu10k1 hardware limits
 */

#define	EMU_PTESIZE		4096
#define	EMU_MAXPTE ((EMU_CHAN_PSST_LOOPSTARTADDR_MASK + 1) /	\
			EMU_PTESIZE)
#define EMU_NUMCHAN	64
#define EMU_NUMRECSRCS	3

#define	EMU_DMA_ALIGN	4096
#define	EMU_DMAMEM_NSEG	1

/*
 * Emu10k1 memory management
 */

struct emuxki_mem {
	LIST_ENTRY(emuxki_mem) next;
	struct dmamem  *dmamem;
	uint16_t       ptbidx;
#define	EMU_RMEM		0xFFFF		/* recording memory */
};

/*
 * Emu10k1 play channel params
 */

struct emuxki_chanparms_fxsend {
	struct {
		uint8_t	level, dest;
	} a, b, c, d, e, f, g, h;
};

struct emuxki_chanparms_pitch {
	uint16_t	initial;/* 4 bits of octave, 12 bits of fractional
				 * octave */
	uint16_t	current;/* 0x4000 == unity pitch shift */
	uint16_t	target;	/* 0x4000 == unity pitch shift */
	uint8_t		envelope_amount;	/* Signed 2's complement, +/-
						 * one octave peak extremes */
};

struct emuxki_chanparms_envelope {
	uint16_t	current_state;	/* 0x8000-n == 666*n usec delay */
	uint8_t		hold_time;	/* 127-n == n*(volume ? 88.2 :
					 * 42)msec */
	uint8_t		attack_time;	/* 0 = infinite, 1 = (volume ? 11 :
					 * 10.9) msec, 0x7f = 5.5msec */
	uint8_t		sustain_level;	/* 127 = full, 0 = off, 0.75dB
					 * increments */
	uint8_t		decay_time;	/* 0 = 43.7msec, 1 = 21.8msec, 0x7f =
					 * 22msec */
};

struct emuxki_chanparms_volume {
	uint16_t current, target;
	struct emuxki_chanparms_envelope envelope;
};

struct emuxki_chanparms_filter {
	uint16_t       initial_cutoff_frequency;
	/*
	 * 6 most  significant bits are semitones, 2 least significant bits
	 * are fractions
	 */
	uint16_t	current_cutoff_frequency;
	uint16_t	target_cutoff_frequency;
	uint8_t		lowpass_resonance_height;
	uint8_t		interpolation_ROM;	/* 1 = full band, 7 = low
						 * pass */
	uint8_t		envelope_amount;	/* Signed 2's complement, +/-
						 * six octaves peak extremes */
	uint8_t		LFO_modulation_depth;	/* Signed 2's complement, +/-
						 * three octave extremes */
};

struct emuxki_chanparms_loop {
	uint32_t	start;	/* index in the PTB (in samples) */
	uint32_t	end;	/* index in the PTB (in samples) */
};

struct emuxki_chanparms_modulation {
	struct emuxki_chanparms_envelope envelope;
	uint16_t	LFO_state;	/* 0x8000-n = 666*n usec delay */
};

struct emuxki_chanparms_vibrato_LFO {
	uint16_t	state;		/* 0x8000-n == 666*n usec delay */
	uint8_t		modulation_depth;	/* Signed 2's complement, +/-
						 * one octave extremes */
	uint8_t		vibrato_depth;	/* Signed 2's complement, +/- one
					 * octave extremes */
	uint8_t		frequency;	/* 0.039Hz steps, maximum of 9.85 Hz */
};

struct emuxki_channel {
	uint8_t		num;	/* voice number */
	struct emuxki_voice *voice;
	struct emuxki_chanparms_fxsend fxsend;
	struct emuxki_chanparms_pitch pitch;
	uint16_t	initial_attenuation;	/* 0.375dB steps */
	struct emuxki_chanparms_volume volume;
	struct emuxki_chanparms_filter filter;
	struct emuxki_chanparms_loop loop;
	struct emuxki_chanparms_modulation modulation;
	struct emuxki_chanparms_vibrato_LFO vibrato_LFO;
	uint8_t		tremolo_depth;
};

/*
 * Voices, streams
 */

typedef enum {
	EMU_RECSRC_MIC = 0,
	EMU_RECSRC_ADC,
	EMU_RECSRC_FX,
	EMU_RECSRC_NOTSET
} emuxki_recsrc_t;

struct emuxki_voice {
	struct emuxki_softc *sc;	/* our softc */

	uint8_t		use;
#define	EMU_VOICE_USE_PLAY		(1 << 0)
	uint8_t		state;
#define EMU_VOICE_STATE_STARTED	(1 << 0)
	uint8_t		stereo;
#define	EMU_VOICE_STEREO_NOTSET	0xFF
	uint8_t		b16;
	uint32_t	sample_rate;
	union {
		struct emuxki_channel *chan[2];
		emuxki_recsrc_t source;
	} dataloc;
	struct emuxki_mem *buffer;
	uint16_t	blksize;/* in samples */
	uint16_t	trigblk;/* blk on which to trigger inth */
	uint16_t	blkmod;	/* Modulo value to wrap trigblk */
	uint16_t	timerate;
	void		(*inth)(void *);
	void	       *inthparam;
	LIST_ENTRY(emuxki_voice) next;
};

#if 0 /* Not yet */
/*
 * I intend this to be able to manage things like AC-3
 */
struct emuxki_stream {
	struct emu10k1			*emu;
	uint8_t				nmono;
	uint8_t				nstereo;
	struct emuxki_voice		*mono;
	struct emuxki_voice		*stereo;
	LIST_ENTRY(emuxki_stream)	next;
};
#endif /* Not yet */

struct emuxki_softc {
	device_t	sc_dev;
	audio_device_t	sc_audv;
	enum {
		EMUXKI_SBLIVE = 0x00, EMUXKI_AUDIGY = 0x01,
		EMUXKI_AUDIGY2 = 0x02, EMUXKI_LIVE_5_1 = 0x04,
		EMUXKI_APS = 0x08
	} sc_type;

	/* Autoconfig parameters */
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_addr_t		sc_iob;
	bus_size_t		sc_ios;
	pci_chipset_tag_t	sc_pc;		/* PCI tag */
	bus_dma_tag_t		sc_dmat;
	void			*sc_ih;		/* interrupt handler */
	kmutex_t		sc_intr_lock;
	kmutex_t		sc_lock;
	kmutex_t		sc_index_lock;
	kmutex_t		sc_ac97_index_lock;

	/* EMU10K1 device structures */
	LIST_HEAD(, emuxki_mem) mem;

	struct dmamem		*ptb;
	struct dmamem		*silentpage;

	struct emuxki_channel	*channel[EMU_NUMCHAN];
	struct emuxki_voice	*recsrc[EMU_NUMRECSRCS];

	LIST_HEAD(, emuxki_voice) voices;
	/* LIST_HEAD(, emuxki_stream)	streams; */

	uint8_t			timerstate;
#define	EMU_TIMER_STATE_ENABLED	1

	struct ac97_host_if	hostif;
	struct ac97_codec_if	*codecif;
	device_t		sc_audev;

	struct emuxki_voice	*pvoice, *rvoice, *lvoice;
};

#endif				/* !_DEV_PCI_EMU10K1VAR_H_ */

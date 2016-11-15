/*	$NetBSD: sbdspvar.h,v 1.61 2011/11/23 23:07:33 jmcneill Exp $	*/

/*
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

#include "mpu.h"
#if NMPU > 0
#include <dev/ic/mpuvar.h>
#endif

#define SB_MASTER_VOL	0
#define SB_MIDI_VOL	1
#define SB_CD_VOL	2
#define SB_VOICE_VOL	3
#define SB_OUTPUT_CLASS	4

#define SB_MIC_VOL	5
#define SB_LINE_IN_VOL	6
#define	SB_RECORD_SOURCE 7
#define SB_TREBLE	8
#define SB_BASS		9
#define SB_RECORD_CLASS	10
#define SB_INPUT_CLASS	11

#define SB_PCSPEAKER	12
#define SB_INPUT_GAIN	13
#define SB_OUTPUT_GAIN	14
#define SB_AGC		15
#define SB_EQUALIZATION_CLASS 16

#define SB_CD_IN_MUTE	17
#define SB_MIC_IN_MUTE	18
#define SB_LINE_IN_MUTE	19
#define SB_MIDI_IN_MUTE	20

#define SB_CD_SWAP	21
#define SB_MIC_SWAP	22
#define SB_LINE_SWAP	23
#define SB_MIDI_SWAP	24

#define SB_CD_OUT_MUTE	25
#define SB_MIC_OUT_MUTE	26
#define SB_LINE_OUT_MUTE 27

#define SB_NDEVS	28

#define SB_IS_IN_MUTE(x) ((x) < SB_CD_SWAP)

/*
 * Software state, per SoundBlaster card.
 * The soundblaster has multiple functionality, which we must demultiplex.
 * One approach is to have one major device number for the soundblaster card,
 * and use different minor numbers to indicate which hardware function
 * we want.  This would make for one large driver.  Instead our approach
 * is to partition the design into a set of drivers that share an underlying
 * piece of hardware.  Most things are hard to share, for example, the audio
 * and midi ports.  For audio, we might want to mix two processes' signals,
 * and for midi we might want to merge streams (this is hard due to
 * running status).  Moreover, we should be able to re-use the high-level
 * modules with other kinds of hardware.  In this module, we only handle the
 * most basic communications with the sb card.
 */
struct sbdsp_softc {
	device_t sc_dev;		/* base device */
	isa_chipset_tag_t sc_ic;
	bus_space_tag_t sc_iot;		/* tag */
	bus_space_handle_t sc_ioh;	/* handle */
	void	*sc_ih;			/* interrupt vectoring */
	kmutex_t sc_lock;
	kmutex_t sc_intr_lock;

	/* XXX These are only for setting chip configuration registers. */
	int	sc_iobase;		/* I/O port base address */
	int	sc_irq;			/* interrupt */

	int	sc_drq8;		/* DMA (8-bit) */
	bus_size_t sc_drq8_maxsize;
	int	sc_drq16;		/* DMA (16-bit) */
	bus_size_t sc_drq16_maxsize;

	u_int	sc_quirks;		/* minor variations */
#define SB_QUIRK_NO_INIT_DRQ	0x01

	int	sc_open;		/* reference count of open calls */
#define SB_CLOSED 0
#define SB_OPEN_AUDIO 1
#define SB_OPEN_MIDI 2
	u_char	sc_fullduplex;		/* can do full duplex */

	u_char	gain[SB_NDEVS][2];	/* kept in input levels */
#define SB_LEFT 0
#define SB_RIGHT 1
#define SB_LR 0

	u_int	in_mask;		/* input ports */
	u_int	in_port;		/* XXX needed for MI interface */
	u_int	in_filter;		/* one of SB_TREBLE_EQ, SB_BASS_EQ, 0 */

	u_int	spkr_state;		/* non-null is on */

	struct sbdsp_state {
		u_int	rate;		/* Sample rate */
		u_char	tc;		/* Time constant */
		struct	sbmode *modep;
		u_char	bmode;
		int	dmachan;	/* DMA channel */
		int	blksize;	/* Block size, preadjusted */
		u_char	run;
#define SB_NOTRUNNING 0		/* Not running, not initialized */
#define SB_RUNNING 3		/* non-looping mode */
#define SB_LOOPING 2		/* DMA&PCM running (looping mode) */
	} sc_i, sc_o;			/* Input and output state */

	u_long	sc_interrupts;		/* number of interrupts taken */

	int	(*sc_intr8)(void*);	/* DMA completion intr handler */
	int	(*sc_intr16)(void*);	/* DMA completion intr handler */
	void	(*sc_intrp)(void*);	/* PCM output intr handler */
	void	*sc_argp;		/* arg for sc_intrp() */
	void	(*sc_intrr)(void*);	/* PCM input intr handler */
	void	*sc_argr;		/* arg for sc_intrr() */
	void	(*sc_intrm)(void*, int);/* midi input intr handler */
	void	*sc_argm;		/* arg for sc_intrm() */

	u_int	sc_mixer_model;
#define SBM_NONE	0
#define SBM_CT1335	1
#define SBM_CT1345	2
#define SBM_CT1XX5	3
#define SBM_CT1745	4
#define ISSBM1745(x) ((x)->sc_mixer_model >= SBM_CT1XX5)

	u_int	sc_model;		/* DSP model */
#define SB_UNK	-1
#define SB_1	0			/* original SB */
#define SB_20	1			/* SB 2 */
#define SB_2x	2			/* SB 2, new version */
#define SB_PRO	3			/* SB Pro */
#define SB_JAZZ	4			/* Jazz 16 */
#define SB_16	5			/* SB 16 */
#define SB_32	6			/* SB AWE 32 */
#define SB_64	7			/* SB AWE 64 */

#define SB_NAMES { "SB_1", "SB_2.0", "SB_2.x", "SB_Pro", "Jazz_16", "SB_16", \
		"SB_AWE_32", "SB_AWE_64" }

	u_int	sc_version;		/* DSP version */
#define SBVER_MAJOR(v)	(((v)>>8) & 0xff)
#define SBVER_MINOR(v)	((v)&0xff)

#if NMPU > 0
	int	sc_hasmpu;
#define SBMPU_EXTERNAL	1
#define SBMPU_INTERNAL	0
#define SBMPU_NONE	-1
	device_t sc_mpudev;
	bus_space_tag_t sc_mpu_iot;	/* tag */
	bus_space_handle_t sc_mpu_ioh;	/* handle */
#endif
};

#define ISSBPRO(sc) ((sc)->sc_model == SB_PRO || (sc)->sc_model == SB_JAZZ)
#define ISSBPROCLASS(sc) ((sc)->sc_model >= SB_PRO)
#define ISSB16CLASS(sc) ((sc)->sc_model >= SB_16)

#ifdef _KERNEL
struct malloc_type;

int	sbdsp_open(void *, int);
void	sbdsp_close(void *);

int	sbdsp_probe(struct sbdsp_softc *, cfdata_t);
void	sbdsp_attach(struct sbdsp_softc *);

int	sbdsp_set_in_gain(void *, u_int, u_char);
int	sbdsp_set_in_gain_real(void *, u_int, u_char);
int	sbdsp_get_in_gain(void *);
int	sbdsp_set_out_gain(void *, u_int, u_char);
int	sbdsp_set_out_gain_real(void *, u_int, u_char);
int	sbdsp_get_out_gain(void *);
int	sbdsp_set_monitor_gain(void *, u_int);
int	sbdsp_get_monitor_gain(void *);
int	sbdsp_query_encoding(void *, struct audio_encoding *);
int	sbdsp_set_params(void *, int, int, audio_params_t *, audio_params_t *,
	    stream_filter_list_t *, stream_filter_list_t *);
int	sbdsp_round_blocksize(void *, int, int, const audio_params_t *);
int	sbdsp_get_avail_in_ports(void *);
int	sbdsp_get_avail_out_ports(void *);
int	sbdsp_speaker_ctl(void *, int);

int	sbdsp_commit(void *);
int	sbdsp_trigger_output(void *, void *, void *, int, void (*)(void *),
	    void *, const audio_params_t *);
int	sbdsp_trigger_input(void *, void *, void *, int, void (*)(void *),
	    void *, const audio_params_t *);
int	sbdsp_halt_output(void *);
int	sbdsp_halt_input(void *);

void	sbdsp_compress(int, u_char *, int);
void	sbdsp_expand(int, u_char *, int);

int	sbdsp_reset(struct sbdsp_softc *);
void	sbdsp_spkron(struct sbdsp_softc *);
void	sbdsp_spkroff(struct sbdsp_softc *);

int	sbdsp_wdsp(struct sbdsp_softc *, int);
int	sbdsp_rdsp(struct sbdsp_softc *);

int	sbdsp_intr(void *);

int	sbdsp_set_sr(struct sbdsp_softc *, u_long *, int);

void	sbdsp_mix_write(struct sbdsp_softc *, int, int);
int	sbdsp_mix_read(struct sbdsp_softc *, int);

int	sbdsp_mixer_set_port(void *, mixer_ctrl_t *);
int	sbdsp_mixer_get_port(void *, mixer_ctrl_t *);
int	sbdsp_mixer_query_devinfo(void *, mixer_devinfo_t *);

void	*sb_malloc(void *, int, size_t);
void	sb_free(void *, void *, size_t);
size_t	sb_round_buffersize(void *, int, size_t);
paddr_t	sb_mappage(void *, void *, off_t, int);

int	sbdsp_get_props(void *);
void	sbdsp_get_locks(void *, kmutex_t **, kmutex_t **);

int	sbdsp_midi_open(void *, int, void (*iintr)(void *, int),
	    void (*ointr)(void *), void *);
void	sbdsp_midi_close(void *);
int	sbdsp_midi_output(void *, int);
void	sbdsp_midi_getinfo(void *, struct midi_info *);
#endif

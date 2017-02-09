/*	$NetBSD: interwavevar.h,v 1.18 2012/10/27 17:18:21 chs Exp $	*/

/*
 * Copyright (c) 1997, 1999, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Kari Mettinen
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

#ifndef INTERWAVEVAR_H
#define INTERWAVEVAR_H

typedef struct iw_voice_info {
	u_char	bank; /* lower 2 bits select one of 4 4M bank for voice data */
	u_long	start; /* voice data start address */
	u_long	end;
#define SACI_IRQ	0x80
#define SACI_BACKWARDS	0x40
#define SACI_IRQ_ENABLE 0x20
#define SACI_BOUNCE	0x10
#define SACI_LOOP	0x08
#define SACI_16BIT	0x04
#define SACI_STOP_1	0x02
#define SACI_STOP_0	0x01
	u_char	format; /* SACI , voice direction, format */
	u_char	volume;
#define SVCI_VOLUME_IRQ 0x80
#define SVCI_DIRECTION	0x40
#define SVCI_IRQ_ENABLE 0x20
#define SVCI_BOUNCE	0x10
#define SVCI_LOOP	0x08
#define SVCI_PCM	0x04 /* for playback continue over voice end */
#define SVCI_STOP_1	0x02
#define SVCI_STOP_0	0x01
	u_char	volume_control;
	u_short	left_offset;  /* usually fully left */
	u_short	right_offset;
	u_char	effect_acc;
#define SMSI_ROM	0x80
#define SMSI_ULAW	0x40
#define SMSI_OFFSET_ON	0x20
#define SMSI_ALT_EFF	0x10
#define SMSI_OFF	0x02
#define SMSI_EFFECT	0x01
	u_char	mode;

} iw_voice_info_t;

typedef struct iw_port_info {
	char off;
	char pad[3];
	int voll;
	int volr;
} iw_port_info_t;

struct iw_softc {
	device_t sc_dev;
	bus_space_tag_t sc_iot;		/* bus cookie	  */
	isa_chipset_tag_t sc_ic;
	kmutex_t sc_lock;
	kmutex_t sc_intr_lock;

	int	vers;
	int	revision;
	int	sc_fullduplex;
	int	sc_irate;
	int	sc_orate;
	u_long	sc_dma_flags;
	int	sc_irq;
	int	sc_midi_irq;

	void	(*sc_recintr)(void *);
	void	*sc_recarg;
	void	*sc_recdma_bp;
	int	sc_playdrq;
	bus_size_t sc_play_maxsize;
	int	sc_recdrq;
	bus_size_t sc_rec_maxsize;
	int	sc_recdma_cnt;
	int	sc_playing;
	int	sc_maxdma;

	u_long	outfifo;
	u_long	infifo;

	int	fifosize;
	u_short	silence;

	u_long	sc_last_dmasize;
	u_long	sc_last_playsize;
	u_long	sc_last_played;
	u_long	sc_play_pos;	/* position of next free byte in buffer */
	u_long	sc_play_start;	/* address of start of the buffer */
	u_long	sc_play_end;	/* end */

	void	(*sc_playintr)(void *);
	void	*sc_playarg;
	void	*sc_playdma_bp;
	int	sc_playdma_cnt;

	int	play_encoding;
	int	play_channels;
	int	play_precision;
	int	sc_playlastcc;

	int	sc_fillcount;
	int	bytesout;
	int	bytesin;

	int	sc_playbuf_available;
	int	sc_recbuf_available;

	int	rec_encoding;
	int	rec_channels;
	int	rec_precision;
	int	sc_reclastcc;

	u_char	recfmtbits;
	u_char	playfmtbits;
	u_char	sc_recsrcbits;
	int	(*sc_ih)(void *);
	iw_port_info_t sc_mic;
	iw_port_info_t sc_aux1;
	iw_port_info_t sc_aux2;
	iw_port_info_t sc_linein;
	iw_port_info_t sc_lineout;
	iw_port_info_t sc_rec;
	iw_port_info_t sc_dac;
	iw_port_info_t sc_loopback;
	iw_port_info_t sc_monoin;
	volatile u_short flags;		/* InterWave stat flags */

	bus_space_handle_t dir_h;	/* dummy handle for direct access*/
	int	codec_index;		/* Base Port for Codec */
	bus_space_handle_t codec_index_h;
	isa_chipset_tag_t sc_codec_ic;
	int	pcdrar;			/* Base Port for Ext Device */
	int	p2xr;			/* Compatibility Base Port */
	bus_space_handle_t p2xr_h;
	isa_chipset_tag_t sc_p2xr_ic;
	int	p3xr;			/* MIDI and Synth Base Port */
	bus_space_handle_t p3xr_h;
	isa_chipset_tag_t sc_p3xr_ic;
	int	p401ar;			/* Gen Purpose Reg. 1 address */
	int	p201ar;			/* Game Ctrl normally at 0x201 */
	int	pataar;			/* Base Address for ATAPI I/O Space */

	int	p388ar;			/* Base Port for AdLib. It should bbe 388h */
	int	pnprdp;			/* PNP read data port */
	int	igidxr;			/* Gen Index Reg at P3XR+0x03 */
	int	i16dp;			/* 16-bit data port at P3XR+0x04  */
	int	i8dp;			/* 8-bit data port at P3XR+0x05 */
	int	svsr;			/* Synth Voice Select at P3XR+0x02 */
	int	cdatap;			/* Codec Indexed Data Port at PCODAR+0x01 */
	int	csr1r;			/* Codec Stat Reg 1 at PCODAR+0x02 */
	int	cxdr;			/* Play or Record Data Reg at PCODAR+0x03 */
	int	gmxr;			/* GMCR or GMSR at P3XR+0x00 */
	int	gmxdr;			/* GMTDR or GMRDR at P3XR+0x01 */
	int	lmbdr;			/* LMBDR at P3XR+0x07 */
	u_char	csn;			/* Card Select Number */
	u_char	cmode;			/* Codec Operation Mode */
	int	dma1_chan;		/* DMA channel 1 (local DMA & codec rec) */
	int	dma2_chan;		/* DMA channel 2 (codec play) */
	int	ext_chan;		/* Ext Dev DMA channel */
	u_char	voices;			/* Number of active voices */
	u_long	vendor;			/* Vendor ID and Product Identifier */
	long	free_mem;		/* Address of First Free LM Block */
	long	reserved_mem;		/* Amount of LM reserved by app. */
	u_char	smode;			/* Synth Mode */
	long	size_mem;		/* Total LM in bytes */
	struct	cfdriver *iw_cd;
	const struct audio_hw_if *iw_hw_if;
};

void	iwattach(struct iw_softc *);

int	iwopen(struct iw_softc *, int);	/* open hardware */
void	iwclose(void *);		/* close hardware */

	/* Encoding. */
	/* XXX should we have separate in/out? */
int	iw_query_encoding(void *, struct audio_encoding *);
int	iw_set_params(void *, int, int, audio_params_t *, audio_params_t *,
	    stream_filter_list_t *, stream_filter_list_t *);

	/* Hardware may have some say in the blocksize to choose */
int	iw_round_blocksize(void *, int, int, const audio_params_t *);

int	iw_commit_settings(void *);

	/* Software en/decode functions, set if SW coding required by HW */
void	iw_sw_encode(void *, int, u_char *, int);
void	iw_sw_decode(void *, int, u_char *, int);

	/* Start input/output routines. These usually control DMA. */
int	iw_start_output(void *, void *, int, void (*)(void *), void *);
int	iw_start_input(void *, void *, int, void (*)(void *), void *);

int	iw_init_input(void *, void *, int);
int	iw_init_output(void *, void *, int);
int	iw_halt_output(void *);
int	iw_halt_input(void *);

int	iw_speaker_ctl(void *, int);
int	iw_getdev(void *, struct audio_device *);
int	iw_setfd(void *, int);

	/* Mixer (in/out ports) */
int	iw_set_port(void *, mixer_ctrl_t *);
int	iw_get_port(void *, mixer_ctrl_t *);

int	iw_query_devinfo(void *, mixer_devinfo_t *);

struct malloc_type;
void *	iw_malloc(void *, int, size_t);
void	iw_free(void *, void *, size_t);
size_t	iw_round_buffersize(void *, int, size_t);
paddr_t	iw_mappage(void *, void *, off_t, int);
int	iw_get_props(void *);
void	iw_get_locks(void *, kmutex_t **, kmutex_t **);

#endif /* INTERWAVEVAR_H */

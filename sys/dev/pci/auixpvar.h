/* $NetBSD: auixpvar.h,v 1.8 2012/10/27 17:18:28 chs Exp $*/

/*
 * Copyright (c) 2004, 2005 Reinoud Zandijk <reinoud@netbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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


/*
 * NetBSD audio driver for ATI IXP-{150,200,...} audio driver hardware.
 */

#define DMA_DESC_CHAIN	255


/* audio format structure describing our hardware capabilities */
/* XXX min and max sample rates are for AD1888 codec XXX */
#define AUIXP_NFORMATS 6


#define AUIXP_MINRATE  7000
#define AUIXP_MAXRATE 48000


/* current AC'97 driver only supports SPDIF outputting channel 3&4 i.e. STEREO */
static const struct audio_format auixp_formats[AUIXP_NFORMATS] = {
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16, 2, AUFMT_STEREO, 0, {7000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 32, 32, 2, AUFMT_STEREO, 0, {7000, 48000}},
	{NULL, AUMODE_PLAY, AUDIO_ENCODING_SLINEAR_LE, 16, 16, 4, AUFMT_SURROUND4, 0, {7000, 48000}},
	{NULL, AUMODE_PLAY, AUDIO_ENCODING_SLINEAR_LE, 32, 32, 4, AUFMT_SURROUND4, 0, {7000, 48000}},
	{NULL, AUMODE_PLAY, AUDIO_ENCODING_SLINEAR_LE, 16, 16, 6, AUFMT_DOLBY_5_1, 0, {7000, 48000}},
	{NULL, AUMODE_PLAY, AUDIO_ENCODING_SLINEAR_LE, 32, 32, 6, AUFMT_DOLBY_5_1, 0, {7000, 48000}},
};


/* auixp structures; used to record alloced DMA space */
struct auixp_dma {
	/* bus mappings */
	bus_dmamap_t		 map;
	void *			 addr;
	bus_dma_segment_t	 segs[1];
	int			 nsegs;
	size_t			 size;

	/* audio feeder */
	void			 (*intr)(void *);	/* function to call when there is space */
	void			*intrarg;

	/* status and setup bits */
	int			 running;
	uint32_t		 linkptr;
	uint32_t		 dma_enable_bit;

	/* linked list of all mapped area's */
	SLIST_ENTRY(auixp_dma)	 dma_chain;
};


struct auixp_codec {
	struct auixp_softc	*sc;

	int			 present;
	int			 codec_nr;

	struct ac97_codec_if	*codec_if;
	struct ac97_host_if	 host_if;
	enum ac97_host_flags	 codec_flags;
};


struct auixp_softc {
	device_t		sc_dev;
	kmutex_t		sc_lock;
	kmutex_t		sc_intr_lock;

	/* card id */
	int			type;
	int			delay1, delay2;		/* nessisary? */

	/* card properties */
	int			has_4ch, has_6ch, is_fixed, has_spdif;

	/* bus tags */
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_addr_t		sc_iob;
	bus_size_t		sc_ios;

	pcitag_t		sc_tag;
	pci_chipset_tag_t	sc_pct;

	bus_dma_tag_t		sc_dmat;

	/* interrupts */
	void			*sc_ih;

	/* DMA business */
	struct auixp_dma	*sc_output_dma;		/* contains dma-safe chain */
	struct auixp_dma	*sc_input_dma;

	/* list of allocated DMA pieces */
	SLIST_HEAD(auixp_dma_list, auixp_dma) sc_dma_list;

	/* audio formats supported */
	struct audio_format sc_formats[AUIXP_NFORMATS];
	struct audio_encoding_set *sc_encodings;

	/* codecs */
	int			sc_num_codecs;
	struct auixp_codec	sc_codec[ATI_IXP_CODECS];
	int			sc_codec_not_ready_bits;

	/* last set audio parameters */
	struct audio_params	sc_play_params;
	struct audio_params	sc_rec_params;
};



/*	$NetBSD: esmvar.h,v 1.18 2011/11/23 23:07:35 jmcneill Exp $	*/

/*-
 * Copyright (c) 2002, 2003 Matt Fredette
 * All rights reserved.
 *
 * Copyright (c) 2000, 2001 Rene Hexel <rh@NetBSD.org>
 * All rights reserved.
 *
 * Copyright (c) 2000 Taku YAMAMOTO <taku@cent.saitama-u.ac.jp>
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
 * Taku Id: maestro.c,v 1.12 2000/09/06 03:32:34 taku Exp
 * FreeBSD: /c/ncvs/src/sys/dev/sound/pci/maestro.c,v 1.4 2000/12/18 01:36:35 cg Exp
 *
 */

/*
 * Credits:
 *
 * This code is based on the FreeBSD driver written by Taku YAMAMOTO
 *
 *
 * Original credits from the FreeBSD driver:
 *
 * Part of this code (especially in many magic numbers) was heavily inspired
 * by the Linux driver originally written by
 * Alan Cox <alan.cox@linux.org>, modified heavily by
 * Zach Brown <zab@zabbo.net>.
 *
 * busdma()-ize and buffer size reduction were suggested by
 * Cameron Grant <gandalf@vilnya.demon.co.uk>.
 * Also he showed me the way to use busdma() suite.
 *
 * Internal speaker problems on NEC VersaPro's and Dell Inspiron 7500
 * were looked at by
 * Munehiro Matsuda <haro@tk.kubota.co.jp>,
 * who brought patches based on the Linux driver with some simplification.
 */

/* IRQ timer fequency limits */
#define MAESTRO_MINFREQ	24
#define MAESTRO_MAXFREQ	48000

/*
 * This driver allocates a contiguous 256KB region of memory.
 * The Maestro's DMA interface, called the WaveCache, is weak
 * (or at least incorrectly documented), and forces us to keep
 * things very simple.  This region is very carefully divided up
 * into 64KB quarters, making 64KB a fundamental constant for
 * this implementation - and this is as large as we can allow
 * the upper-layer playback and record buffers to become.
 */
#define	MAESTRO_QUARTER_SZ	(64 * 1024)

/*
 * The first quarter of memory is used while recording.  The
 * first 512 bytes of it is reserved as a scratch area for the
 * APUs that want to write (uninteresting, to us) FIFO status
 * information.  After some guard space, another 512 bytes is
 * reserved for the APUs doing mixing.  The remainder of this
 * quarter of memory is wasted.
 */
#define	MAESTRO_FIFO_OFF	(MAESTRO_QUARTER_SZ * 0)
#define	MAESTRO_FIFO_SZ		(512)
#define	MAESTRO_MIXBUF_OFF	(MAESTRO_FIFO_OFF + 4096)
#define	MAESTRO_MIXBUF_SZ	(512)

/*
 * The second quarter of memory is the playback buffer.
 */
#define	MAESTRO_PLAYBUF_OFF	(MAESTRO_QUARTER_SZ * 1)
#define	MAESTRO_PLAYBUF_SZ	MAESTRO_QUARTER_SZ

/*
 * The third quarter of memory is the mono record buffer.
 * This is the only record buffer that the upper layer knows.
 * When recording in stereo, our driver combines (in software)
 * separately recorded left and right buffers here.
 */
#define	MAESTRO_RECBUF_OFF	(MAESTRO_QUARTER_SZ * 2)
#define	MAESTRO_RECBUF_SZ	MAESTRO_QUARTER_SZ

/*
 * The fourth quarter of memory is the stereo record buffer.
 * When recording in stereo, the left and right channels are
 * recorded separately into the two halves of this buffer.
 */
#define	MAESTRO_RECBUF_L_OFF	(MAESTRO_QUARTER_SZ * 3)
#define	MAESTRO_RECBUF_L_SZ	(MAESTRO_QUARTER_SZ / 2)
#define	MAESTRO_RECBUF_R_OFF	(MAESTRO_RECBUF_L_OFF + MAESTRO_RECBUF_L_SZ)
#define	MAESTRO_RECBUF_R_SZ	(MAESTRO_QUARTER_SZ / 2)

/*
 * The size and alignment of the entire region.  We keep
 * the region aligned to a 128KB boundary, since this should
 * force A16..A0 on all chip-generated addresses to correspond
 * exactly to APU register contents.
 */
#define	MAESTRO_DMA_SZ		(MAESTRO_QUARTER_SZ * 4)
#define	MAESTRO_DMA_ALIGN	(128 * 1024)

struct esm_dma {
	bus_dmamap_t		map;
	void *			addr;
	bus_dma_segment_t	segs[1];
	int			nsegs;
	size_t			size;
	struct esm_dma		*next;
};

#define DMAADDR(p) ((p)->map->dm_segs[0].ds_addr)
#define KERNADDR(p) ((void *)((p)->addr))

struct esm_chinfo {
	uint32_t		base;		/* DMA base */
	void *			buffer;		/* upper layer buffer */
	uint32_t		offset;		/* offset into buffer */
	uint32_t		blocksize;	/* block size in bytes */
	uint32_t		bufsize;	/* buffer size in bytes */
	unsigned		num;		/* logical channel number */
	uint16_t		aputype;	/* APU channel type */
	uint16_t		apubase;	/* first sample number */
	uint16_t		apublk;		/* blk size in samples per ch */
	uint16_t		apubuf;		/* buf size in samples per ch */
	uint16_t		nextirq;	/* pos to trigger next IRQ at */
	uint16_t		wcreg_tpl;	/* wavecache tag and format */
	uint16_t		sample_rate;
};

struct esm_softc {
	device_t		sc_dev;
	kmutex_t		sc_lock;
	kmutex_t		sc_intr_lock;

	bus_space_tag_t		st;
	bus_space_handle_t	sh;
	bus_size_t		sz;

	pcitag_t		tag;
	pci_chipset_tag_t	pc;
	bus_dma_tag_t		dmat;
	pcireg_t		subid;

	void			*ih;

	struct ac97_codec_if	*codec_if;
	struct ac97_host_if	host_if;
	enum ac97_host_flags	codec_flags;

	struct esm_dma		sc_dma;
	int			rings_alloced;

	int			pactive, ractive;
	struct esm_chinfo	pch;
	struct esm_chinfo	rch;

	void			(*sc_pintr)(void *);
	void			*sc_parg;

	void			(*sc_rintr)(void *);
	void			*sc_rarg;
};

enum esm_quirk_flags {
	ESM_QUIRKF_GPIO = 0x1,		/* needs GPIO operation */
	ESM_QUIRKF_SWAPPEDCH = 0x2,	/* left/right is reversed */
};

struct esm_quirks {
	pci_vendor_id_t		eq_vendor;	/* subsystem vendor */
	pci_product_id_t	eq_product;	/* and product */

	enum esm_quirk_flags	eq_quirks;	/* needed quirks */
};

int	esm_read_codec(void *, uint8_t, uint16_t *);
int	esm_write_codec(void *, uint8_t, uint16_t);
int	esm_attach_codec(void *, struct ac97_codec_if *);
int	esm_reset_codec(void *);
enum ac97_host_flags	esm_flags_codec(void *);

void	esm_init(struct esm_softc *);
void	esm_initcodec(struct esm_softc *);

int	esm_init_output(void *, void *, int);
int	esm_init_input(void *, void *, int);
int	esm_trigger_output(void *, void *, void *, int, void (*)(void *),
	    void *, const audio_params_t *);
int	esm_trigger_input(void *, void *, void *, int, void (*)(void *),
	    void *, const audio_params_t *);
int	esm_halt_output(void *);
int	esm_halt_input(void *);
int	esm_getdev(void *, struct audio_device *);
int	esm_round_blocksize(void *, int, int, const audio_params_t *);
int	esm_query_encoding(void *, struct audio_encoding *);
int	esm_set_params(void *, int, int, audio_params_t *, audio_params_t *,
	    stream_filter_list_t *, stream_filter_list_t *);
int	esm_set_port(void *, mixer_ctrl_t *);
int	esm_get_port(void *, mixer_ctrl_t *);
int	esm_query_devinfo(void *, mixer_devinfo_t *);
void	*esm_malloc(void *, int, size_t);
void	esm_free(void *, void *, size_t);
size_t	esm_round_buffersize(void *, int, size_t);
paddr_t	esm_mappage(void *, void *, off_t, int);
int	esm_get_props(void *);
void	esm_get_locks(void *, kmutex_t **, kmutex_t **);

enum esm_quirk_flags	esm_get_quirks(pcireg_t);

/* $NetBSD: hdaudiovar.h,v 1.4 2015/07/26 17:54:33 jmcneill Exp $ */

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

#ifndef _HDAUDIOVAR_H
#define _HDAUDIOVAR_H

#include <dev/auconv.h>

#ifdef _KERNEL_OPT
#include "opt_hdaudio.h"
#endif

#define	HDAUDIO_MAX_CODECS	15

#define	hda_print(sc, ...)		\
	aprint_normal_dev((sc)->sc_dev, __VA_ARGS__)
#define	hda_print1(sc, ...)		\
	aprint_normal(__VA_ARGS__)
#define	hda_error(sc, ...)		\
	aprint_error_dev((sc)->sc_dev, __VA_ARGS__)
#ifdef HDAUDIO_DEBUG
#define	hda_trace(sc, ...)		\
	aprint_normal_dev((sc)->sc_dev, __VA_ARGS__)
#define	hda_trace1(sc, ...)		\
	aprint_normal(__VA_ARGS__)
#else
#define hda_trace(sc, ...) do { } while (0)
#define hda_trace1(sc, ...) do { } while (0)
#endif
#define	hda_delay(us)			\
	delay((us))

enum function_group_type {
	HDAUDIO_GROUP_TYPE_UNKNOWN = 0,
	HDAUDIO_GROUP_TYPE_AFG,
	HDAUDIO_GROUP_TYPE_VSM_FG,
};

struct hdaudio_softc;

struct hdaudio_function_group {
	device_t			fg_device;
	struct hdaudio_codec		*fg_codec;
	enum function_group_type	fg_type;
	int				fg_nid;
	uint16_t			fg_vendor;
	uint16_t			fg_product;

	int				(*fg_unsol)(device_t, uint8_t);
};

struct hdaudio_codec {
	bool				co_valid;
	u_int				co_addr;
	u_int				co_nfg;
	struct hdaudio_function_group	*co_fg;
	struct hdaudio_softc		*co_host;
};

#define	DMA_KERNADDR(dma)	((dma)->dma_addr)
#define DMA_DMAADDR(dma)	((dma)->dma_map->dm_segs[0].ds_addr)
	
struct hdaudio_dma {
	bus_dmamap_t		dma_map;
	void			*dma_addr;
	bus_dma_segment_t	dma_segs[1];
	int			dma_nsegs;
	bus_size_t		dma_size;
	bool			dma_valid;
	uint8_t			dma_sizereg;
};

#define	HDAUDIO_MAX_STREAMS	30

struct hdaudio_dma_position {
	uint32_t	position;
	uint32_t	reserved;
} __packed;

struct hdaudio_bdl_entry {
	uint32_t	address_lo;
	uint32_t	address_hi;
	uint32_t	length;
	uint32_t	flags;
#define	HDAUDIO_BDL_ENTRY_IOC	0x00000001
} __packed;

#define	HDAUDIO_BDL_MAX		256

enum hdaudio_stream_type {
	HDAUDIO_STREAM_ISS = 0,
	HDAUDIO_STREAM_OSS = 1,
	HDAUDIO_STREAM_BSS = 2
};

struct hdaudio_stream {
	struct hdaudio_softc		*st_host;
	bool				st_enable;
	enum hdaudio_stream_type	st_type;
	int				st_shift;
	int				st_num;

	int				(*st_intr)(struct hdaudio_stream *);
	void				*st_cookie;

	struct hdaudio_dma		st_data;
	struct hdaudio_dma		st_bdl;
};

struct hdaudio_softc {
	device_t		sc_dev;

	bus_dma_tag_t		sc_dmat;
	bus_space_tag_t		sc_memt;
	bus_space_handle_t	sc_memh;
	bus_addr_t		sc_membase;
	bus_size_t		sc_memsize;
	bool			sc_memvalid;

	uint32_t		sc_subsystem;

	kmutex_t		sc_corb_mtx;
	struct hdaudio_dma	sc_corb;
	struct hdaudio_dma	sc_rirb;
	uint16_t		sc_rirbrp;

	struct hdaudio_codec	sc_codec[HDAUDIO_MAX_CODECS];

	struct hdaudio_stream	sc_stream[HDAUDIO_MAX_STREAMS];
	uint32_t		sc_stream_mask;
	kmutex_t		sc_stream_mtx;

	uint32_t		sc_flags;
#define HDAUDIO_FLAG_NO_STREAM_RESET	0x0001
};

int	hdaudio_attach(device_t, struct hdaudio_softc *);
int	hdaudio_detach(struct hdaudio_softc *, int);
bool	hdaudio_resume(struct hdaudio_softc *);
int	hdaudio_rescan(struct hdaudio_softc *, const char *, const int *);
void	hdaudio_childdet(struct hdaudio_softc *, device_t);

uint32_t hdaudio_command(struct hdaudio_codec *, int, uint32_t, uint32_t);
uint32_t hdaudio_command_unlocked(struct hdaudio_codec *, int, uint32_t,
    uint32_t);
int	hdaudio_intr(struct hdaudio_softc *);

int	hdaudio_dma_alloc(struct hdaudio_softc *, struct hdaudio_dma *, int);
void	hdaudio_dma_free(struct hdaudio_softc *, struct hdaudio_dma *);

struct hdaudio_stream *	hdaudio_stream_establish(struct hdaudio_softc *,
				    enum hdaudio_stream_type,
				    int (*)(struct hdaudio_stream *), void *);
void	hdaudio_stream_disestablish(struct hdaudio_stream *);
void	hdaudio_stream_start(struct hdaudio_stream *, int, bus_size_t,
			     const audio_params_t *);
void	hdaudio_stream_stop(struct hdaudio_stream *);
void	hdaudio_stream_reset(struct hdaudio_stream *);
int	hdaudio_stream_tag(struct hdaudio_stream *);
uint16_t hdaudio_stream_param(struct hdaudio_stream *, const audio_params_t *);

#ifdef HDAUDIO_32BIT_ACCESS
static inline uint8_t
_hda_read1(struct hdaudio_softc *sc, bus_size_t off)
{
	return bus_space_read_4(sc->sc_memt, sc->sc_memh, off & -4) >>
	    (8 * (off & 3));
}
static inline uint16_t
_hda_read2(struct hdaudio_softc *sc, bus_size_t off)
{
	return bus_space_read_4(sc->sc_memt, sc->sc_memh, off & -4) >>
	    (8 * (off & 2));
}
#define hda_read1			_hda_read1
#define hda_read2			_hda_read2
#define	hda_read4(sc, off)		\
	bus_space_read_4((sc)->sc_memt, (sc)->sc_memh, (off))
static inline void
_hda_write1(struct hdaudio_softc *sc, bus_size_t off, uint8_t val)
{
	const size_t shift = 8 * (off & 3);
	off &= -4;
	uint32_t tmp = bus_space_read_4(sc->sc_memt, sc->sc_memh, off);
	tmp = (val << shift) | (tmp & ~(0xff << shift));
	bus_space_write_4(sc->sc_memt, sc->sc_memh, off, tmp);
}
static inline void
_hda_write2(struct hdaudio_softc *sc, bus_size_t off, uint16_t val)
{
	const size_t shift = 8 * (off & 2);
	off &= -4;
	uint32_t tmp = bus_space_read_4(sc->sc_memt, sc->sc_memh, off);
	tmp = (val << shift) | (tmp & ~(0xffff << shift));
	bus_space_write_4(sc->sc_memt, sc->sc_memh, off, tmp);
}
#define hda_write1			_hda_write1
#define hda_write2			_hda_write2
#define	hda_write4(sc, off, val)	\
	bus_space_write_4((sc)->sc_memt, (sc)->sc_memh, (off), (val))
#else
#define	hda_read1(sc, off)		\
	bus_space_read_1((sc)->sc_memt, (sc)->sc_memh, (off))
#define	hda_read2(sc, off)		\
	bus_space_read_2((sc)->sc_memt, (sc)->sc_memh, (off))
#define	hda_read4(sc, off)		\
	bus_space_read_4((sc)->sc_memt, (sc)->sc_memh, (off))
#define	hda_write1(sc, off, val)	\
	bus_space_write_1((sc)->sc_memt, (sc)->sc_memh, (off), (val))
#define	hda_write2(sc, off, val)	\
	bus_space_write_2((sc)->sc_memt, (sc)->sc_memh, (off), (val))
#define	hda_write4(sc, off, val)	\
	bus_space_write_4((sc)->sc_memt, (sc)->sc_memh, (off), (val))
#endif

#endif /* !_HDAUDIOVAR_H */

/*	$NetBSD: cs4231var.h,v 1.10 2011/11/23 23:07:32 jmcneill Exp $	*/

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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

#ifndef _DEV_IC_CS4231VAR_H_
#define _DEV_IC_CS4231VAR_H_

#define AUDIOCS_PROM_NAME	"SUNW,CS4231"

/*
 * List of device memory allocations (see cs4231_malloc/cs4231_free).
 */
struct cs_dma {
	struct	cs_dma	*next;
	void *		addr;
	bus_dmamap_t	dmamap;
	bus_dma_segment_t segs[1];
	int		nsegs;
	size_t		size;
};


/*
 * DMA transfer to/from CS4231.
 */
struct cs_transfer {
	int t_active;		/* this transfer is currently active */
	struct cs_dma *t_dma;	/* dma memory to transfer from/to */
	vsize_t t_segsz;	/* size of the segment */
	vsize_t t_blksz;	/* call audio(9) after this many bytes */
	vsize_t t_cnt;		/* how far are we into the segment */

	void (*t_intr)(void*);	/* audio(9) callback */
	void *t_arg;

	const char *t_name;	/* for error/debug messages */

	struct evcnt t_intrcnt;
	struct evcnt t_ierrcnt;
};


/*
 * Software state, per CS4231 audio chip.
 */
struct cs4231_softc {
	struct ad1848_softc sc_ad1848;	/* base device */

	bus_space_tag_t	sc_bustag;
	bus_dma_tag_t sc_dmatag;

	struct cs_dma *sc_dmas;		/* allocated dma resources */

	struct evcnt sc_intrcnt;	/* parent counter */

	struct cs_transfer sc_playback;
	struct cs_transfer sc_capture;
};


/*
 * Bus independent code shared by sbus and ebus attachments.
 */
void	cs4231_common_attach(struct cs4231_softc *, device_t,
			     bus_space_handle_t);
int	cs4231_transfer_init(struct cs4231_softc *, struct cs_transfer *,
			     bus_addr_t *, bus_size_t *,
			     void *, void *, int, void (*)(void *), void *);
void	cs4231_transfer_advance(struct cs_transfer *,
				bus_addr_t *, bus_size_t *);


/*
 * Bus independent audio(9) methods.
 */
int	cs4231_open(void *, int);
void	cs4231_close(void *);
int	cs4231_getdev(void *, struct audio_device *);
int	cs4231_set_port(void *, mixer_ctrl_t *);
int	cs4231_get_port(void *, mixer_ctrl_t *);
int	cs4231_query_devinfo(void *, mixer_devinfo_t *);
int	cs4231_get_props(void *);

void	*cs4231_malloc(void *, int, size_t);
void	cs4231_free(void *, void *, size_t);

#endif /* _DEV_IC_CS4231VAR_H_ */

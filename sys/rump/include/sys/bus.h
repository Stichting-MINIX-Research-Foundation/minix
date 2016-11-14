/*	$NetBSD: bus.h,v 1.6 2015/06/03 13:55:42 pooka Exp $	*/

/*
 * Copyright (c) 2010 Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _SYS_RUMP_BUS_H_
#define _SYS_RUMP_BUS_H_

/*
 * This is a blanket header since archs are inline/macro-happy.
 *
 * XXX: this file should NOT exist here
 */

/* bus space defs */
typedef unsigned long bus_addr_t;
typedef unsigned long bus_size_t;
typedef unsigned long bus_space_tag_t;
typedef unsigned long bus_space_handle_t;

/* bus dma defs */
typedef void *bus_dma_tag_t;
#define BUS_DMA_TAG_VALID(_tag_) ((_tag_) != NULL)

typedef struct {
	bus_addr_t	ds_addr;
	bus_size_t	ds_len;
	vaddr_t		_ds_vacookie;
	bus_size_t	_ds_sizecookie;
} bus_dma_segment_t;

typedef struct {
	bus_size_t _dm_size;
	int _dm_segcnt;
	bus_size_t _dm_maxmaxsegsz;
	bus_size_t _dm_boundary;
	bus_addr_t _dm_bounce_thresh;
	int _dm_flags;
	void *_dm_cookie;

	bus_size_t dm_maxsegsz;
	bus_size_t dm_mapsize;
	int dm_nsegs;
	bus_dma_segment_t dm_segs[1];
} *bus_dmamap_t;

#include <sys/bus_proto.h>

#endif /* _SYS_RUMP_BUS_H_ */

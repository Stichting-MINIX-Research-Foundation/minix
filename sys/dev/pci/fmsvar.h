/*	$NetBSD: fmsvar.h,v 1.7 2012/10/27 17:18:32 chs Exp $	*/

/*-
 * Copyright (c) 1999, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Witold J. Wnuk.
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


#ifndef _DEV_PCI_FMSVAR_H_
#define _DEV_PCI_FMSVAR_H_

struct fms_softc {
	device_t sc_dev;
	void *sc_ih;
	kmutex_t sc_lock;
	kmutex_t sc_intr_lock;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_addr_t sc_ioaddr;
	bus_size_t sc_iosize;
	bus_dma_tag_t sc_dmat;

	bus_space_handle_t sc_opl_ioh;

	bus_space_handle_t sc_mpu_ioh;
	device_t  sc_mpu_dev;

	struct ac97_codec_if *codec_if;
	struct ac97_host_if host_if;

	struct fms_dma *sc_dmas;

	void (*sc_pintr)(void *);
	void *sc_parg;
	bus_addr_t sc_play_start, sc_play_end, sc_play_nextblk;
	int sc_play_blksize;
	int sc_play_flip;
	uint16_t sc_play_reg;

	void (*sc_rintr)(void *);
	void *sc_rarg;
	bus_addr_t sc_rec_start, sc_rec_end, sc_rec_nextblk;
	int sc_rec_blksize;
	int sc_rec_flip;
	uint16_t sc_rec_reg;
};

#endif	/* !_DEV_PCI_FMSVAR_H_ */

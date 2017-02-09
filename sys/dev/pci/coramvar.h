/* $NetBSD: coramvar.h,v 1.4 2011/08/09 01:42:24 jmcneill Exp $ */

/*
 * Copyright (c) 2008, 2011 Jonathan A. Kollasch
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_PCI_CORAMVAR_H
#define _DEV_PCI_CORAMVAR_H

#include <sys/bus.h>
#include <sys/mutex.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/i2c/i2cvar.h>

#define KERNADDR(p)	((void *)((p)->addr))
#define DMAADDR(p)	((p)->map->dm_segs[0].ds_addr)

struct coram_board {
	uint16_t vendor;
	uint16_t product;
	const char *name;
};

struct coram_sram_ch {
	uint32_t	csc_cmds;
	uint32_t	csc_iq;
	uint32_t	csc_iqsz;
	uint32_t	csc_cdt;
	uint32_t	csc_cdtsz;
	uint32_t	csc_fifo;
	uint32_t	csc_fifosz;
	uint32_t	csc_risc;
	uint32_t	csc_riscsz;
	uint32_t	csc_ptr1;
	uint32_t	csc_ptr2;
	uint32_t	csc_cnt1;
	uint32_t	csc_cnt2;
};

struct coram_dma {
	bus_dmamap_t		map;
	void *			addr;
	bus_dma_segment_t	segs[1];
	int			nsegs;
	size_t			size;
	struct coram_dma *	next;
};

struct coram_softc {
	device_t		sc_dev;
	device_t		sc_dtvdev;

	bus_space_tag_t		sc_memt;
	bus_space_handle_t	sc_memh;
	bus_size_t		sc_mems;
	bus_dma_tag_t		sc_dmat;

	pci_chipset_tag_t	sc_pc;
	void *			sc_ih;

	struct coram_iic_softc {
		struct coram_softc *	cic_sc;
		bus_space_handle_t	cic_regh;
		struct i2c_controller	cic_i2c;
		kmutex_t		cic_busmutex;
		device_t		cic_i2cdev;
	} sc_iic[3];

	struct coram_sram_ch	sc_vidc_sch;

	struct coram_dma *	sc_dma;
	struct coram_dma *	sc_tsbuf;

	uint32_t *		sc_riscbuf;
	uint32_t		sc_riscbufsz;

	void			*sc_tuner;
	void			*sc_demod;

	void			(*sc_dtvsubmitcb)(void *,
				    const struct dtv_payload *);
	void			*sc_dtvsubmitarg;

	const struct coram_board *sc_board;
};

#endif /* !_DEV_PCI_CORAMVAR_H */

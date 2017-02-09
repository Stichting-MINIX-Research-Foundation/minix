/*	$NetBSD: ninjaata32var.h,v 1.5 2011/02/21 02:32:00 itohy Exp $	*/

/*
 * Copyright (c) 2006 ITOH Yasufumi.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _NJATA32VAR_H_
#define _NJATA32VAR_H_

#define NJATA32NAME(sc)		(device_xname(sc->sc_wdcdev.sc_atac.atac_dev))

/* ??? */
#define NJATA32_MAX_XFER	(64 * 1024)

/*
 * DMA page
 */
/* # device */
#define NJATA32_NUM_DEV	2
/* # scatter/gather table entries */
#define NJATA32_NUM_SG	NJATA32_SGT_MAXENTRY

struct njata32_dma_page {
	/*
	 * scatter/gather transfer table
	 */
	struct njata32_sgtable	dp_sg[NJATA32_NUM_DEV][NJATA32_NUM_SG];
};

#define NJATA32_NCHAN	1	/* only one channel */

struct njata32_softc {
	struct wdc_softc	sc_wdcdev;	/* common wdc definitions */

	unsigned		sc_flags;
#define NJATA32_IO_MAPPED		0x00000001
#define NJATA32_MEM_MAPPED		0x00000002
#define NJATA32_CMDPG_MAPPED		0x00000004

	unsigned		sc_devflags;

	/* interrupt handle */
	void			*sc_ih;

	struct ninjaata32_channel {		/* per-channel data */
		struct ata_channel ch_ata_channel; /* generic part */
	} sc_ch[NJATA32_NCHAN];

	struct ata_channel	*sc_wdc_chanarray[NJATA32_NCHAN];
	struct ata_queue	sc_wdc_chqueue;
	struct wdc_regs		sc_wdc_regs;
#define NJATA32_REGT(sc)	(sc)->sc_wdc_regs.cmd_iot
#define NJATA32_REGH(sc)	(sc)->sc_wdc_regs.cmd_baseioh

	/* for DMA */
	bus_dma_tag_t		sc_dmat;
	struct njata32_dma_page	*sc_sgtpg;	/* scatter/gather table page */
#if 0
	bus_addr_t		sc_sgt_dma;
#endif
	bus_dma_segment_t	sc_sgt_seg;
	bus_dmamap_t		sc_dmamap_sgt;
	int			sc_sgt_nsegs;

	int			sc_piobm_nsegs;

	uint8_t			sc_timing_pio;
#if 0	/* ATA DMA is currently unused */
	uint8_t			sc_timing_dma;
#endif

	uint8_t			sc_atawait;

	/* per-device structure */
	struct njata32_device {
		/* DMA resource */
		struct njata32_sgtable	*d_sgt;		/* for host */
		bus_addr_t		d_sgt_dma;	/* for device */
		bus_dmamap_t		d_dmamap_xfer;
		unsigned		d_flags;
#define NJATA32_DEV_DMA_MAPPED		0x0001
#define NJATA32_DEV_DMA_READ		0x0002
#define NJATA32_DEV_DMA_ATAPI		0x0004
#define NJATA32_DEV_XFER_INTR		0x0100	/* only for sc_devflags */
#define NJATA32_DEV_GOT_XFER_INTR	0x0200	/* only for sc_devflags */
#define NJATA32_DEV_DMA_STARTED		0x8000	/* for diag */
	} sc_dev[NJATA32_NUM_DEV];
};

#ifdef _KERNEL
void	njata32_attach(struct njata32_softc *);
int	njata32_detach(struct njata32_softc *, int);
int	njata32_intr(void *);
#endif

#endif	/* _NJATA32VAR_H_ */

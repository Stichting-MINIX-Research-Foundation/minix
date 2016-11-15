/*	$NetBSD: bhavar.h,v 1.25 2012/10/27 17:18:19 chs Exp $	*/

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
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

#ifndef _DEV_IC_BHAVAR_H_
#define	_DEV_IC_BHAVAR_H_

#include <sys/queue.h>

/* XXX adjust hash for large numbers of CCBs */
#define	CCB_HASH_SIZE	32	/* hash table size for phystokv */
#define	CCB_HASH_SHIFT	9
#define CCB_HASH(x)	((((long)(x))>>CCB_HASH_SHIFT) & (CCB_HASH_SIZE - 1))

/*
 * A CCB allocation group.  Each group is a page size.  We can find
 * the allocation group for a CCB by truncating the CCB address to
 * a page boundary, and the offset from the group's DMA mapping
 * by taking the offset of the CCB into the page.
 */
#define	BHA_CCBS_PER_GROUP	((PAGE_SIZE - sizeof(bus_dmamap_t)) /	\
				 sizeof(struct bha_ccb))
struct bha_ccb_group {
	bus_dmamap_t bcg_dmamap;
	struct bha_ccb bcg_ccbs[1];	/* determined at run-time */
};

#define	BHA_CCB_GROUP(ccb)						\
	(struct bha_ccb_group *)(trunc_page((vaddr_t)ccb))
#define	BHA_CCB_OFFSET(ccb)	((vaddr_t)(ccb) & PAGE_MASK)

#define	BHA_CCB_SYNC(sc, ccb, ops)					\
do {									\
	struct bha_ccb_group *bcg = BHA_CCB_GROUP((ccb));		\
									\
	bus_dmamap_sync((sc)->sc_dmat, bcg->bcg_dmamap,			\
	    BHA_CCB_OFFSET(ccb), sizeof(struct bha_ccb), (ops));	\
} while (0)

/*
 * Offset in the DMA mapping for mailboxes.
 * Since all mailboxes are allocated on a single DMA'able memory
 * due to the hardware limitation, an offset of any mailboxes can be
 * calculated using same expression.
 */
#define	BHA_MBX_OFFSET(sc, mbx)	((u_long)(mbx) - (u_long)(sc)->sc_mbo)

#define	BHA_MBI_SYNC(sc, mbi, ops)					\
do {									\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_dmamap_mbox,		\
	    BHA_MBX_OFFSET((sc), (mbi)), sizeof(struct bha_mbx_in), (ops)); \
} while (0)

#define	BHA_MBO_SYNC(sc, mbo, ops)					\
do {									\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_dmamap_mbox,		\
	    BHA_MBX_OFFSET((sc), (mbo)), sizeof(struct bha_mbx_out), (ops)); \
} while (0)

struct bha_softc {
	device_t sc_dev;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_dma_tag_t sc_dmat;
	bus_dmamap_t sc_dmamap_mbox;	/* maps the mailboxes */
	int sc_dmaflags;		/* bus-specific dma map flags */
	void *sc_ih;

	int sc_scsi_id;			/* host adapter SCSI ID */

	int sc_flags;
#define	BHAF_WIDE		0x01	/* device is wide */
#define	BHAF_DIFFERENTIAL	0x02	/* device is differential */
#define	BHAF_ULTRA		0x04	/* device is ultra-scsi */
#define	BHAF_TAGGED_QUEUEING	0x08	/* device supports tagged queueing */
#define	BHAF_WIDE_LUN		0x10	/* device supported wide lun CCBs */
#define	BHAF_STRICT_ROUND_ROBIN	0x20	/* device supports strict RR mode */

	int sc_max_dmaseg;		/* maximum number of DMA segments */
	int sc_max_ccbs;		/* maximum number of CCBs (HW) */
	int sc_cur_ccbs;		/* current number of CCBs */

	int sc_disc_mask;		/* mask of targets allowing discnnct */
	int sc_ultra_mask;		/* mask of targets allowing ultra */
	int sc_fast_mask;		/* mask of targets allowing fast */
	int sc_sync_mask;		/* mask of targets allowing sync */
	int sc_wide_mask;		/* mask of targets allowing wide */
	int sc_tag_mask;		/* mask of targets allowing t/q'ing */

	/*
	 * In and Out mailboxes.
	 */
	struct bha_mbx_out *sc_mbo;
	struct bha_mbx_in *sc_mbi;

	struct bha_mbx_out *sc_cmbo;	/* Collection Mail Box out */
	struct bha_mbx_out *sc_tmbo;	/* Target Mail Box out */

	int sc_mbox_count;		/* number of mailboxes */
	int sc_mbofull;			/* number of full Mail Box Out */

	struct bha_mbx_in *sc_tmbi;	/* Target Mail Box in */

	struct bha_ccb *sc_ccbhash[CCB_HASH_SIZE];
	TAILQ_HEAD(, bha_ccb)	sc_free_ccb,
				sc_waiting_ccb,
				sc_allocating_ccbs;

	struct scsipi_adapter sc_adapter;
	struct scsipi_channel sc_channel;

	char sc_model[7],
	     sc_firmware[6];
};

struct bha_probe_data {
	int sc_irq, sc_drq;
};

int	bha_find(bus_space_tag_t, bus_space_handle_t);
int	bha_inquire_config(bus_space_tag_t, bus_space_handle_t,
	    struct bha_probe_data *);
void	bha_attach(struct bha_softc *);
int	bha_info(struct bha_softc *);
int	bha_intr(void *);

int	bha_disable_isacompat(struct bha_softc *);
int	bha_probe_inquiry(bus_space_tag_t, bus_space_handle_t,
	    struct bha_probe_data *);

#endif /* _DEV_IC_BHAVAR_H_ */

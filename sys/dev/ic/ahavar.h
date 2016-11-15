/*	$NetBSD: ahavar.h,v 1.15 2009/09/21 08:12:47 tsutsui Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
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

#ifndef _DEV_IC_AHAVAR_H_
#define	_DEV_IC_AHAVAR_H_

#include <sys/queue.h>

/*
 * Mail box defs  etc.
 * these could be bigger but we need the aha_softc to fit on a single page..
 */
#define AHA_MBX_SIZE	16	/* mail box size  (MAX 255 MBxs) */
				/* don't need that many really */
#define AHA_CCB_MAX	16	/* store up to 16 CCBs at one time */
#define	CCB_HASH_SIZE	16	/* hash table size for phystokv */
#define	CCB_HASH_SHIFT	9
#define CCB_HASH(x)	((((long)(x))>>CCB_HASH_SHIFT) & (CCB_HASH_SIZE - 1))

#define aha_nextmbx(wmb, mbx, mbio) \
	if ((wmb) == &(mbx)->mbio[AHA_MBX_SIZE - 1])	\
		(wmb) = &(mbx)->mbio[0];		\
	else						\
		(wmb)++;

struct aha_mbx {
	struct aha_mbx_out mbo[AHA_MBX_SIZE];
	struct aha_mbx_in mbi[AHA_MBX_SIZE];
	struct aha_mbx_out *cmbo;	/* Collection Mail Box out */
	struct aha_mbx_out *tmbo;	/* Target Mail Box out */
	struct aha_mbx_in *tmbi;	/* Target Mail Box in */
};

struct aha_control {
	struct aha_mbx ac_mbx;		/* all our mailboxes */
	struct aha_ccb ac_ccbs[AHA_CCB_MAX]; /* all our control blocks */
};

struct aha_softc {
	device_t sc_dev;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_dma_tag_t sc_dmat;
	bus_dmamap_t sc_dmamap_control;	/* maps the control structures */
	void *sc_ih;

	struct aha_control *sc_control;	/* control structures */

#define	wmbx	(&sc->sc_control->ac_mbx)

	struct aha_ccb *sc_ccbhash[CCB_HASH_SIZE];
	TAILQ_HEAD(, aha_ccb) sc_free_ccb, sc_waiting_ccb;
	int sc_mbofull;

	struct scsipi_adapter sc_adapter;
	struct scsipi_channel sc_channel;

	char sc_model[18],
	     sc_firmware[4];
};

/*
 * Offset of a Mail Box In from the beginning of the control DMA mapping.
 */
#define	AHA_MBI_OFF(m)	(offsetof(struct aha_control, ac_mbx.mbi[0]) +	\
			    (((u_long)(m)) - ((u_long)&wmbx->mbi[0])))

/*
 * Offset of a Mail Box Out from the beginning of the control DMA mapping.
 */
#define	AHA_MBO_OFF(m)	(offsetof(struct aha_control, ac_mbx.mbo[0]) +	\
			    (((u_long)(m)) - ((u_long)&wmbx->mbo[0])))

/*
 * Offset of a CCB from the beginning of the control DMA mapping.
 */
#define	AHA_CCB_OFF(c)	(offsetof(struct aha_control, ac_ccbs[0]) +	\
		    (((u_long)(c)) - ((u_long)&sc->sc_control->ac_ccbs[0])))

struct aha_probe_data {
	int sc_irq, sc_drq;
	int sc_scsi_dev;		/* adapters scsi id */
};

int	aha_find(bus_space_tag_t, bus_space_handle_t,
	    struct aha_probe_data *);
void	aha_attach(struct aha_softc *, struct aha_probe_data *);
int	aha_intr(void *);

#endif /* _DEV_IC_AHAVAR_H_ */

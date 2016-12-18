/*      $NetBSD: adv.h,v 1.13 2005/12/11 12:21:25 christos Exp $        */

/*
 * Generic driver definitions and exported functions for the Advanced
 * Systems Inc. Narrow SCSI controllers
 *
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Baldassare Dante Profeta <dante@mclink.it>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#ifndef _ADVANSYS_NARROW_H_
#define _ADVANSYS_NARROW_H_

/******************************************************************************/

/* second level interrupt callback type definition */
typedef int (* ASC_ISR_CALLBACK) (ASC_SOFTC *, ASC_QDONE_INFO *);

struct adv_ccb
{
	ASC_SG_HEAD	sghead;
	ASC_SCSI_Q	scsiq;

	struct scsi_sense_data scsi_sense;

	struct callout ccb_watchdog;

	TAILQ_ENTRY(adv_ccb) chain;
	struct adv_ccb		*nexthash;
	u_long			hashkey;
	struct scsipi_xfer	*xs;	/* the scsipi_xfer for this cmd */
	int			flags;	/* see below */

	int			timeout;
	/*
	 * This DMA map maps the buffer involved in the transfer.
	 */
	bus_dmamap_t		dmamap_xfer;
};

typedef struct adv_ccb ADV_CCB;

/* flags for ADV_CCB */
#define CCB_ALLOC       0x01
#define CCB_ABORT       0x02
#define	CCB_WATCHDOG	0x10


#define ADV_MAX_CCB	32

struct adv_control
{
	ADV_CCB	ccbs[ADV_MAX_CCB];	/* all our control blocks */
	u_int8_t overrun_buf[ASC_OVERRUN_BSIZE];
};

/*
 * Offset of a CCB from the beginning of the control DMA mapping.
 */
#define	ADV_CCB_OFF(c)	(offsetof(struct adv_control, ccbs[0]) +	\
		    (((u_long)(c)) - ((u_long)&sc->sc_control->ccbs[0])))

/******************************************************************************/

int adv_init(ASC_SOFTC *);
void adv_attach(ASC_SOFTC *);
int adv_detach(ASC_SOFTC *, int);
int adv_intr(void *);
ADV_CCB *adv_ccb_phys_kv(ASC_SOFTC *, u_long);

/******************************************************************************/

#endif /* _ADVANSYS_NARROW_H_ */

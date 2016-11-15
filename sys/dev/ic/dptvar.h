/*	$NetBSD: dptvar.h,v 1.16 2012/10/27 17:18:20 chs Exp $	*/

/*
 * Copyright (c) 1999, 2000, 2001 Andrew Doran <ad@NetBSD.org>
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
 */

#ifndef _IC_DPTVAR_H_
#define _IC_DPTVAR_H_ 1

/* Software parameters */
#define DPT_SG_SIZE        	17
#define	DPT_MAX_XFER		65536
#define DPT_MAX_CCBS		256
#define DPT_ABORT_TIMEOUT	2000	/* milliseconds */
#define DPT_SCRATCH_SIZE	256	/* bytes */

#define	CCB_OFF(sc,m)	((u_long)(m) - (u_long)((sc)->sc_ccbs))

#define CCB_ABORT	0x01	/* abort has been issued on this CCB */
#define CCB_INTR	0x02	/* HBA interrupted for this CCB */
#define CCB_PRIVATE	0x04	/* ours; don't talk to scsipi when done */
#define CCB_WAIT	0x08	/* sleeping on completion */

struct dpt_ccb {
	/* Data that will be touched by the HBA */
	struct eata_cp	ccb_eata_cp;		/* EATA command packet */
	struct eata_sg	ccb_sg[DPT_SG_SIZE];	/* SG element list */
	struct scsi_sense_data ccb_sense;	/* SCSI sense data on error */

	/* Data that will not be touched by the HBA */
	volatile int	ccb_flg;		/* CCB flags */
	int		ccb_timeout;		/* timeout in ms */
	u_int32_t	ccb_ccbpa;		/* physical addr of this CCB */
	bus_dmamap_t	ccb_dmamap_xfer;	/* dmamap for data xfers */
	int		ccb_hba_status;		/* from status packet */
	int		ccb_scsi_status;	/* from status packet */
	int		ccb_id;			/* unique ID of this CCB */
	SLIST_ENTRY(dpt_ccb) ccb_chain;		/* link to next CCB */
	struct scsipi_xfer *ccb_xs;		/* initiating SCSI command */
	struct eata_sp	*ccb_savesp;		/* saved status packet */
};

struct dpt_softc {
	device_t	sc_dev;		/* generic device data */
	kmutex_t	sc_lock;
	struct scsipi_adapter sc_adapt;	/* scsipi adapter */
	struct scsipi_channel sc_chans[3]; /* each channel */
	bus_space_handle_t sc_ioh;	/* bus space handle */
	bus_space_tag_t	sc_iot;		/* bus space tag */
	bus_dma_tag_t	sc_dmat;	/* bus DMA tag */
	bus_dmamap_t	sc_dmamap;	/* maps the CCBs */
	void	 	*sc_ih;		/* interrupt handler cookie */
	struct dpt_ccb	*sc_ccbs;	/* all our CCBs */
	struct eata_sp	*sc_stp;	/* EATA status packet */
	int		sc_stpoff;	/* status packet offset in dmamap */
	u_int32_t	sc_stppa;	/* status packet physical address */
	void *		sc_scr;		/* scratch area */
	int		sc_scroff;	/* scratch area offset in dmamap */
	u_int32_t	sc_scrpa;	/* scratch area physical address */
	int		sc_hbaid[3];	/* ID of HBA on each channel */
	int		sc_nccbs;	/* number of CCBs available */
	SLIST_HEAD(, dpt_ccb) sc_ccb_free;/* free ccb list */
	struct eata_cfg sc_ec;		/* EATA configuration data */
	int		sc_bustype;	/* SysInfo bus type */
	int		sc_isadrq;	/* ISA DRQ */
	int		sc_isairq;	/* ISA IRQ */
	int		sc_isaport;	/* ISA port */
};

void	dpt_init(struct dpt_softc *, const char *);
int	dpt_intr(void *);
int	dpt_readcfg(struct dpt_softc *);

#endif	/* !defined _IC_DPTVAR_H_ */

/* $NetBSD: oosiopvar.h,v 1.6 2008/03/29 09:11:35 tsutsui Exp $ */

/*
 * Copyright (c) 2001 Shuichiro URATA.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define	OOSIOP_NTGT	8		/* Max targets */
#define	OOSIOP_NCB	32		/* Initial command buffers */
#define	OOSIOP_NSG	(MIN(btoc(MAXPHYS) + 1, 32)) /* Max S/G operation */
#define	OOSIOP_MAX_XFER	ctob(OOSIOP_NSG - 1)

struct oosiop_xfer {
	/* script for scatter/gather DMA (move*nsg+jump) */
	uint32_t datain_scr[(OOSIOP_NSG + 1) * 2];
	uint32_t dataout_scr[(OOSIOP_NSG + 1) * 2];

	uint8_t msgin[8];
	uint8_t msgout[8];
	uint8_t status;
	uint8_t pad[7];
} __packed;

#define	SCSI_OOSIOP_NOSTATUS	0xff	/* device didn't report status */

#define	OOSIOP_XFEROFF(x)	offsetof(struct oosiop_xfer, x)
#define	OOSIOP_DINSCROFF	OOSIOP_XFEROFF(datain_scr[0])
#define	OOSIOP_DOUTSCROFF	OOSIOP_XFEROFF(dataout_scr[0])
#define	OOSIOP_MSGINOFF		OOSIOP_XFEROFF(msgin[0])
#define	OOSIOP_MSGOUTOFF	OOSIOP_XFEROFF(msgout[0])

#define	OOSIOP_XFERSCR_SYNC(sc, cb, ops)				\
	bus_dmamap_sync((sc)->sc_dmat, (cb)->xferdma, OOSIOP_DINSCROFF,	\
	    OOSIOP_MSGINOFF - OOSIOP_DINSCROFF, (ops))
#define	OOSIOP_DINSCR_SYNC(sc, cb, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (cb)->xferdma, OOSIOP_DINSCROFF,	\
	    OOSIOP_DOUTSCROFF - OOSIOP_DINSCROFF, (ops))
#define	OOSIOP_DOUTSCR_SYNC(sc, cb, ops)				\
	bus_dmamap_sync((sc)->sc_dmat, (cb)->xferdma, OOSIOP_DOUTSCROFF,\
	    OOSIOP_MSGINOFF - OOSIOP_DOUTSCROFF, (ops))
#define	OOSIOP_XFERMSG_SYNC(sc, cb, ops)				\
	bus_dmamap_sync((sc)->sc_dmat, (cb)->xferdma, OOSIOP_MSGINOFF,	\
	    sizeof(struct oosiop_xfer) - OOSIOP_MSGINOFF, (ops))

#define	OOSIOP_SCRIPT_SYNC(sc, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_scrdma,			\
	    0, sizeof(oosiop_script), (ops))

struct oosiop_cb {
	TAILQ_ENTRY(oosiop_cb) chain;

	struct scsipi_xfer *xs;		/* SCSI xfer ctrl block from above */
	int flags;
	int id;				/* target scsi id */
	int lun;			/* target lun */

	bus_dmamap_t cmddma;		/* DMA map for command out */
	bus_dmamap_t datadma;		/* DMA map for data I/O */
	bus_dmamap_t xferdma;		/* DMA map for xfer block */

	int curdp;			/* current data pointer */
	int savedp;			/* saved data pointer */
	int msgoutlen;

	struct oosiop_xfer *xfer;	/* DMA xfer block */
};

/* oosiop_cb flags */
#define	CBF_SELTOUT	0x01	/* Selection timeout */
#define	CBF_TIMEOUT	0x02	/* Command timeout */

TAILQ_HEAD(oosiop_cb_queue, oosiop_cb);

struct oosiop_target {
	struct oosiop_cb *nexus;
	int flags;
	uint8_t scf;		/* synchronous clock divisor */
	uint8_t sxfer;		/* synchronous period and offset */
};

/* target flags */
#define	TGTF_SYNCNEG	0x01	/* Trigger synchronous negotiation */
#define	TGTF_WAITSDTR	0x02	/* Waiting SDTR from target */

struct oosiop_softc {
	device_t sc_dev;

	bus_space_tag_t	sc_bst;		/* bus space tag */
	bus_space_handle_t sc_bsh;	/* bus space handle */

	bus_dma_tag_t sc_dmat;		/* bus DMA tag */
	bus_dmamap_t sc_scrdma;		/* script DMA map */

	bus_addr_t sc_scrbase;		/* script DMA base address */
	uint32_t *sc_scr;		/* ptr to script memory */

	int sc_chip;			/* 700 or 700-66 */
#define	OOSIOP_700	0
#define	OOSIOP_700_66	1

	int		sc_id;		/* SCSI ID of this interface */
	int		sc_freq;	/* SCLK frequency */
	int		sc_ccf;		/* asynchronous divisor (*10) */
	uint8_t		sc_dcntl;
	uint8_t		sc_minperiod;

	struct oosiop_target sc_tgt[OOSIOP_NTGT];

	struct scsipi_adapter sc_adapter;
	struct scsipi_channel sc_channel;

	/* Lists of command blocks */
	struct oosiop_cb_queue sc_free_cb;
	struct oosiop_cb_queue sc_cbq;	/* command issue queue */
	struct oosiop_cb *sc_curcb;	/* current command */
	struct oosiop_cb *sc_lastcb;	/* last activated command */

	bus_addr_t sc_reselbuf;		/* msgin buffer for reselection */
	int sc_resid;			/* reselected target id */

	int sc_active;
	int sc_nextdsp;
};

#define	oosiop_read_1(sc, addr)						\
    bus_space_read_1((sc)->sc_bst, (sc)->sc_bsh, (addr))
#define	oosiop_write_1(sc, addr, data)					\
    bus_space_write_1((sc)->sc_bst, (sc)->sc_bsh, (addr), (data))
/* XXX byte swapping should be handled by MD bus_space(9)? */
#define	oosiop_read_4(sc, addr)						\
    le32toh(bus_space_read_4((sc)->sc_bst, (sc)->sc_bsh, (addr)))
#define	oosiop_write_4(sc, addr, data)					\
    bus_space_write_4((sc)->sc_bst, (sc)->sc_bsh, (addr), htole32(data))

void oosiop_attach(struct oosiop_softc *);
int oosiop_intr(struct oosiop_softc *);

/*	$NetBSD: ninjascsi32var.h,v 1.7 2012/10/27 17:18:22 chs Exp $	*/

/*-
 * Copyright (c) 2004, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by ITOH Yasufumi.
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

#ifndef _NJSC32VAR_H_
#define _NJSC32VAR_H_

typedef unsigned	njsc32_model_t;
#define NJSC32_MODEL_MASK	0xff
#define NJSC32_MODEL_INVALID	0
#define NJSC32_MODEL_32BI	1
#define NJSC32_MODEL_32UDE	2
#define NJSC32_FLAG_DUALEDGE	0x100	/* supports DualEdge */

/*
 * time parameters (25us per unit?)
 */
#define NJSC32_SEL_TIMEOUT_TIME		20000	/* selection timeout (500ms) */
#define NJSC32_ARBITRATION_RETRY_TIME	4	/* 100us */

/* in microseconds */
#define NJSC32_REQ_TIMEOUT		10000	/* 10ms */
#define NJSC32_RESET_HOLD_TIME		26	/* 25us min */

/*
 * DMA page
 */
#ifdef NJSC32_AUTOPARAM
#define NJSC32_NUM_CMD	14	/* # simultaneous commands */
#else
#define NJSC32_NUM_CMD	15	/* # simultaneous commands */
#endif
#define NJSC32_NUM_SG	17	/* # scatter/gather table entries per command */

struct njsc32_dma_page {
	/*
	 * scatter/gather transfer table
	 */
	struct njsc32_sgtable	dp_sg[NJSC32_NUM_CMD][NJSC32_NUM_SG];
#define NJSC32_SIZE_SGT \
	(sizeof(struct njsc32_sgtable) * NJSC32_NUM_SG)

#ifdef NJSC32_AUTOPARAM
	/*
	 * device reads parameters from this structure (autoparam)
	 */
	struct njsc32_autoparam	dp_ap;
#endif
};

/* per command */
struct njsc32_cmd {
	TAILQ_ENTRY(njsc32_cmd)	c_q;
	struct njsc32_softc	*c_sc;

	/* on transfer */
	struct scsipi_xfer	*c_xs;
	struct njsc32_target	*c_target;
	struct njsc32_lu	*c_lu;
	u_int32_t		c_datacnt;	/* I/O buffer length */

	/* command status */
	int		c_flags;
#define NJSC32_CMD_DMA_MAPPED	0x01
#define NJSC32_CMD_TAGGED	0x02
#define NJSC32_CMD_TAGGED_HEAD	0x04

	/* SCSI pointer */
	u_int32_t	c_dp_cur;	/* current (or active) data pointer */
	u_int32_t	c_dp_saved;	/* saved data pointer */
	u_int32_t	c_dp_max;	/* max value of data pointer */

	/* last loaded scatter/gather table */
	unsigned	c_sgoffset;	/* # skip entries */
	u_int32_t	c_sgfixcnt;	/* # skip bytes in the top entry */

	/* command start/restart parameter */
	u_int8_t	c_msg_identify;	/* Identify message */
	u_int16_t	c_xferctl;
	u_int32_t	c_sgtdmaaddr;

	/* DMA resource */
	struct njsc32_sgtable	*c_sgt;		/* for host */
	bus_addr_t		c_sgt_dma;	/* for device */
#define NJSC32_CMD_DMAADDR_SGT(cmd, n)	\
		((cmd)->c_sgt_dma + sizeof(struct njsc32_sgtable) * (n))
	bus_dmamap_t		c_dmamap_xfer;
};

/* -1 for unaligned acccess */
#define NJSC32_MAX_XFER	((NJSC32_NUM_SG - 1) << PGSHIFT)

struct njsc32_softc {
	device_t		sc_dev;

	/* device spec */
	njsc32_model_t		sc_model;

	int			sc_clk;		/* one of following */
#define NJSC32_CLK_40M		NJSC32_CLOCK_DIV_4	/* 20MB/s */
#define NJSC32_CLK_20M		NJSC32_CLOCK_DIV_2	/* 10MB/s */
#define NJSC32_CLK_PCI_33M	NJSC32_CLOCK_PCICLK	/* 16.6MB/s */

	/* device register */
	bus_space_tag_t		sc_regt;
	bus_space_handle_t	sc_regh;

	unsigned		sc_flags;
#define NJSC32_IO_MAPPED		0x00000001
#define NJSC32_MEM_MAPPED		0x00000002
#define NJSC32_CMDPG_MAPPED		0x00000004
#define NJSC32_CANNOT_SUPPLY_TERMPWR	0x00000100

	/*
	 * controller state
	 */
	enum njsc32_stat {
		NJSC32_STAT_IDLE,
		NJSC32_STAT_ARBIT,	/* initiator started arbitration */
		NJSC32_STAT_CONNECT,	/* command is active (connection) */
		NJSC32_STAT_RESEL,	/* a target did Reselection */
		NJSC32_STAT_RESEL_LUN,	/* received Identify message */
		NJSC32_STAT_RECONNECT,	/* command is active (reconnection) */
		NJSC32_STAT_RESET,	/* resetting bus */
		NJSC32_STAT_RESET1,	/* waiting for bus reset release */
		NJSC32_STAT_RESET2,	/* waiting for bus reset release */
		NJSC32_STAT_DETACH	/* detaching */
	} sc_stat;

	/* interrupt handle */
	void			*sc_ih;

	/* for DMA */
	bus_dma_tag_t		sc_dmat;
	struct njsc32_dma_page	*sc_cmdpg;	/* scatter/gather table page */
#if 0
	bus_addr_t		sc_cmdpg_dma;
#endif
	bus_dma_segment_t	sc_cmdpg_seg;
	bus_dmamap_t		sc_dmamap_cmdpg;
	int			sc_cmdpg_nsegs;

#ifdef NJSC32_AUTOPARAM
	u_int32_t		sc_ap_dma;	/* autoparam DMA address */
#endif

	/* for monitoring bus reset */
	struct callout		sc_callout;

	/*
	 * command control structure
	 */
	struct njsc32_cmd	sc_cmds[NJSC32_NUM_CMD];
	TAILQ_HEAD(njsc32_cmd_head, njsc32_cmd)
				sc_freecmd,	/* free list */
				sc_reqcmd;	/* waiting commands */

	struct njsc32_cmd	*sc_curcmd;	/* currently active command */
	int			sc_ncmd;	/* total # commands available */
	int			sc_nusedcmds;	/* # used commands */

	/* reselection */
	int			sc_reselid, sc_resellun;

	/* message in buffer */
#define NJSC32_MSGIN_LEN	20
	u_int8_t		sc_msginbuf[NJSC32_MSGIN_LEN];
	int			sc_msgincnt;

	/* message out buffer */
#define NJSC32_MSGOUT_LEN	16
	u_int8_t		sc_msgout[NJSC32_MSGOUT_LEN];
	size_t			sc_msgoutlen;
	size_t			sc_msgoutidx;

	/* sync timing table */
	const struct njsc32_sync_param {
		u_int8_t	sp_period;	/* transfer period */
		u_int8_t	sp_ackw;	/* ACK width parameter */
		u_int8_t	sp_sample;	/* sampling period */
	} *sc_synct;
	int	sc_sync_max;

	/* for scsipi layer */
	device_t		sc_scsi;
	struct scsipi_adapter	sc_adapter;
	struct scsipi_channel	sc_channel;

	/* per-target */
	struct njsc32_target {
		enum njsc32_tarst {
			NJSC32_TARST_DONE,	/* negotiation done */
			NJSC32_TARST_INIT,
			NJSC32_TARST_DE,	/* negotiating DualEdge */
			NJSC32_TARST_WDTR,	/* negotiating width */
			NJSC32_TARST_SDTR,	/* negotiating sync */
			NJSC32_TARST_ASYNC	/* negotiating async */
		} t_state;
		int	t_flags;
#define NJSC32_TARF_TAG		0x0001	/* tagged queueing is enabled */
#define NJSC32_TARF_SYNC	0x0002	/* negotiate for sync transfer */
#define NJSC32_TARF_DE		0x0004	/* negotiate for DualEdge transfer */

		int		t_syncperiod;
		int		t_syncoffset;

		u_int8_t	t_sync;
		u_int8_t	t_ackwidth;
		u_int8_t	t_targetid;	/* initiator and target id */
		u_int8_t	t_sample;

		u_int16_t	t_xferctl;	/* DualEdge flag */

		/* per logical unit */
		struct njsc32_lu {
			/*
			 * disconnected commands
			 */
			struct njsc32_cmd *lu_cmd;	/* untagged command */
			struct njsc32_cmd_head	lu_q;	/* tagged commands */
		} t_lus[NJSC32_NLU];
	} sc_targets[NJSC32_MAX_TARGET_ID + 1];
};

#ifdef _KERNEL
void	njsc32_attach(struct njsc32_softc *);
int	njsc32_detach(struct njsc32_softc *, int);
int	njsc32_intr(void *);
#endif

#endif	/* _NJSC32VAR_H_ */

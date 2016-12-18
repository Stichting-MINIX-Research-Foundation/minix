/*	$NetBSD: mb89352var.h,v 1.15 2009/11/12 20:15:02 dyoung Exp $	*/
/*	NecBSD: mb89352var.h,v 1.4 1998/03/14 07:31:22 kmatsuda Exp 	*/

/*-
 * Copyright (c) 1996-1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum, Masaru Oki and Kouichi Matsuda.
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
 *
 * Copyright (c) 1994 Jarle Greipsland
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1996, 1997, 1998
 *	Kouich Matsuda. All rights reserved.
 */

#ifndef	_MB89352VAR_H_
#define	_MB89352VAR_H_
/*
 * ACB. Holds additional information for each SCSI command Comments: We
 * need a separate scsi command block because we may need to overwrite it
 * with a request sense command.  Basicly, we refrain from fiddling with
 * the scsi_xfer struct (except do the expected updating of return values).
 * We'll generally update: xs->{flags,resid,error,sense,status} and
 * occasionally xs->retries.
 */
struct spc_acb {
	struct scsipi_generic scsipi_cmd;
	int scsipi_cmd_length;
	uint8_t *data_addr;		/* Saved data pointer */
	int data_length;		/* Residue */

	uint8_t target_stat;		/* SCSI status byte */

#ifdef notdef
	struct spc_dma_seg dma[SPC_NSEG]; /* Physical addresses+len */
#endif

	TAILQ_ENTRY(spc_acb) chain;
	struct scsipi_xfer *xs;	/* SCSI xfer ctrl block from above */
	u_int flags;
#define ACB_ALLOC	0x01
#define ACB_NEXUS	0x02
#define ACB_SENSE	0x04
#define ACB_ABORT	0x40
#define ACB_RESET	0x80
	int timeout;
};

/*
 * Some info about each (possible) target on the SCSI bus.  This should
 * probably have been a "per target+lunit" structure, but we'll leave it at
 * this for now.
 */
struct spc_tinfo {
	int	cmds;		/* #commands processed */
	int	dconns;		/* #disconnects */
	int	touts;		/* #timeouts */
	int	perrs;		/* #parity errors */
	int	senses;		/* #request sense commands sent */
	ushort	lubusy;		/* What local units/subr. are busy? */
	u_char  flags;
#define DO_SYNC		0x01	/* (Re)Negotiate synchronous options */
#define DO_WIDE		0x02	/* (Re)Negotiate wide options */
	uint8_t period;		/* Period suggestion */
	uint8_t offset;		/* Offset suggestion */
	uint8_t width;		/* Width suggestion */
};

struct spc_softc {
	device_t sc_dev;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	struct scsipi_channel sc_channel; /* prototype for subdevs */
	struct scsipi_adapter sc_adapter;

	TAILQ_HEAD(, spc_acb) free_list, ready_list, nexus_list;
	struct spc_acb *sc_nexus;	/* current command */
	struct spc_acb sc_acb[8];
	struct spc_tinfo sc_tinfo[8];

	/* Data about the current nexus (updated for every cmd switch) */
	uint8_t	*sc_dp;		/* Current data pointer */
	size_t	sc_dleft;	/* Data bytes left to transfer */
	uint8_t	*sc_cp;		/* Current command pointer */
	size_t	sc_cleft;	/* Command bytes left to transfer */

	/* Adapter state */
	uint8_t	 sc_phase;	/* Current bus phase */
	uint8_t	 sc_prevphase;	/* Previous bus phase */
	uint8_t	 sc_state;	/* State applicable to the adapter */
#define SPC_INIT	0
#define SPC_IDLE	1
#define SPC_SELECTING	2	/* SCSI command is arbiting  */
#define SPC_RESELECTED	3	/* Has been reselected */
#define SPC_CONNECTED	4	/* Actively using the SCSI bus */
#define SPC_DISCONNECT	5	/* MSG_DISCONNECT received */
#define SPC_CMDCOMPLETE	6	/* MSG_CMDCOMPLETE received */
#define SPC_CLEANING	7
	uint8_t	 sc_flags;
#define SPC_DROP_MSGIN	0x01	/* Discard all msgs (parity err detected) */
#define SPC_ABORTING	0x02	/* Bailing out */
#define SPC_DOINGDMA	0x04	/* doing DMA */
#define SPC_INACTIVE	0x80	/* The FIFO data path is active! */
	uint8_t	sc_selid;	/* Reselection ID */
	device_t sc_child;/* Our child */

	/* Message stuff */
	uint8_t	sc_msgpriq;	/* Messages we want to send */
	uint8_t	sc_msgoutq;	/* Messages sent during last MESSAGE OUT */
	uint8_t	sc_lastmsg;	/* Message last transmitted */
	uint8_t	sc_currmsg;	/* Message currently ready to transmit */
#define SEND_DEV_RESET		0x01
#define SEND_PARITY_ERROR	0x02
#define SEND_INIT_DET_ERR	0x04
#define SEND_REJECT		0x08
#define SEND_IDENTIFY		0x10
#define SEND_ABORT		0x20
#define SEND_SDTR		0x40
#define SEND_WDTR		0x80
#define SPC_MAX_MSG_LEN 8
	uint8_t	sc_omess[SPC_MAX_MSG_LEN];
	uint8_t	*sc_omp;		/* Outgoing message pointer */
	uint8_t	sc_imess[SPC_MAX_MSG_LEN];
	uint8_t	*sc_imp;		/* Incoming message pointer */

	/* Hardware stuff */
	int	sc_initiator;		/* Our scsi id */
	int	sc_freq;		/* Clock frequency in MHz */
	int	sc_minsync;		/* Minimum sync period / 4 */
	int	sc_maxsync;		/* Maximum sync period / 4 */

	/* DMA function set from MD code */
	void (*sc_dma_start)(struct spc_softc *, void *, size_t, int);
	void (*sc_dma_done)(struct spc_softc *);
};

#if SPC_DEBUG
#define SPC_SHOWACBS	0x01
#define SPC_SHOWINTS	0x02
#define SPC_SHOWCMDS	0x04
#define SPC_SHOWMISC	0x08
#define SPC_SHOWTRACE	0x10
#define SPC_SHOWSTART	0x20
#define SPC_DOBREAK	0x40
extern int spc_debug; /* SPC_SHOWSTART|SPC_SHOWMISC|SPC_SHOWTRACE; */
#define SPC_PRINT(b, s)							\
	do {								\
		if ((spc_debug & (b)) != 0)				\
			printf s;					\
	} while (/* CONSTCOND */ 0)
#define SPC_BREAK()							\
	do {								\
		if ((spc_debug & SPC_DOBREAK) != 0)			\
			Debugger();					\
	} while (/* CONSTCOND */ 0)
#define SPC_ASSERT(x)							\
	do {								\
		if (x) {						\
			;						\
		} else {						\
			printf("%s at line %d: assertion failed\n",	\
			    device_xname(sc->sc_dev), __LINE__);	\
			Debugger();					\
		}							\
	} while (/* CONSTCOND */ 0)
#else
#define SPC_PRINT(b, s)
#define SPC_BREAK()
#define SPC_ASSERT(x)
#endif

#define SPC_ACBS(s)	SPC_PRINT(SPC_SHOWACBS, s)
#define SPC_INTS(s)	SPC_PRINT(SPC_SHOWINTS, s)
#define SPC_CMDS(s)	SPC_PRINT(SPC_SHOWCMDS, s)
#define SPC_MISC(s)	SPC_PRINT(SPC_SHOWMISC, s)
#define SPC_TRACE(s)	SPC_PRINT(SPC_SHOWTRACE, s)
#define SPC_START(s)	SPC_PRINT(SPC_SHOWSTART, s)

void	spc_attach(struct spc_softc *);
void	spc_childdet(device_t, device_t);
int	spc_detach(device_t, int);
int	spc_intr(void *);
int	spc_find(bus_space_tag_t, bus_space_handle_t, int);
void	spc_init(struct spc_softc *, int);
void	spc_sched(struct spc_softc *);
#endif	/* _MB89352VAR_H_ */

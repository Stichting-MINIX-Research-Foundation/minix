/*	$NetBSD: amrvar.h,v 1.10 2015/03/02 15:26:57 christos Exp $	*/

/*-
 * Copyright (c) 2002, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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

#ifndef	_PCI_AMRVAR_H_
#define	_PCI_AMRVAR_H_

#define	AMR_MAX_UNITS		16
#define	AMR_WDOG_TICKS		(hz * 5)
#define	AMR_NCCB_RESV		2
#define	AMR_ENQUIRY_BUFSIZE	2048
#define	AMR_SGL_SIZE		(sizeof(struct amr_sgentry) * AMR_MAX_SEGS)
#define	AMR_TIMEOUT		30

/*
 * Logical drive information.
 */
struct amr_logdrive {
	u_int		al_size;
	u_short		al_state;
	u_short		al_properties;
	device_t	al_dv;
};

/*
 * Per-controller state.
 */
struct amr_softc {
	device_t		amr_dv;
	bus_space_tag_t		amr_iot;
	bus_space_handle_t	amr_ioh;
	bus_size_t		amr_ios;
	bus_dma_tag_t		amr_dmat;
	pci_chipset_tag_t	amr_pc;
	void			*amr_ih;
	struct lwp		*amr_thread;
	int			amr_flags;

	struct amr_mailbox	*amr_mbox;
	bus_addr_t		amr_mbox_paddr;
	bus_dmamap_t		amr_dmamap;
        bus_dma_segment_t	amr_dmaseg;
        int			amr_dmasize;
        void			*amr_enqbuf;

	struct amr_sgentry	*amr_sgls;
	bus_addr_t		amr_sgls_paddr;

	struct amr_ccb		*amr_ccbs;
	SLIST_HEAD(, amr_ccb)	amr_ccb_freelist;
	SIMPLEQ_HEAD(, amr_ccb)	amr_ccb_queue;
	TAILQ_HEAD(, amr_ccb)	amr_ccb_active;
	int			amr_maxqueuecnt;

	int	(*amr_get_work)(struct amr_softc *, struct amr_mailbox_resp *);
	int	(*amr_submit)(struct amr_softc *sc, struct amr_ccb *);

	kmutex_t		amr_mutex;

	int			amr_numdrives;
	struct amr_logdrive	amr_drive[AMR_MAX_UNITS];
};

/* What resources are allocated? */
#define	AMRF_INTR		0x00000001
#define	AMRF_DMA_LOAD		0x00000002
#define	AMRF_DMA_MAP		0x00000004
#define	AMRF_DMA_ALLOC		0x00000008
#define	AMRF_DMA_CREATE		0x00000010
#define	AMRF_PCI_INTR		0x00000020
#define	AMRF_PCI_REGS		0x00000040
#define	AMRF_CCBS		0x00000080
#define	AMRF_ENQBUF		0x00000100
#define	AMRF_THREAD		0x00000200

/* General flags. */
#define	AMRF_THREAD_EXIT	0x00010000
#define	AMRF_OPEN		0x00020000

/*
 * Command control block.
 */
struct amr_ccb {
	union {
		SIMPLEQ_ENTRY(amr_ccb) simpleq;
		SLIST_ENTRY(amr_ccb) slist;
		TAILQ_ENTRY(amr_ccb) tailq;
	} ac_chain;

	u_int		ac_flags;
	u_int		ac_status;
	u_int		ac_ident;
	u_int		ac_xfer_size;
	time_t		ac_start_time;
	bus_dmamap_t	ac_xfer_map;
	void		(*ac_handler)(struct amr_ccb *);
	void 		*ac_context;
	device_t	ac_dv;
	struct amr_mailbox_cmd	ac_cmd;
	kmutex_t	ac_mutex;
	kcondvar_t	ac_cv;
};
#define	AC_XFER_IN	0x01	/* Map describes inbound xfer */
#define	AC_XFER_OUT	0x02	/* Map describes outbound xfer */
#define	AC_COMPLETE	0x04	/* Command completed */
#define	AC_ACTIVE	0x08	/* Command active */
#define	AC_NOSGL	0x10	/* No scatter/gather list */
#define AC_MOAN		0x20	/* We have already moaned */

struct amr_attach_args {
	int		amra_unit;
};

int	amr_ccb_alloc(struct amr_softc *, struct amr_ccb **);
void	amr_ccb_enqueue(struct amr_softc *, struct amr_ccb *);
void	amr_ccb_free(struct amr_softc *, struct amr_ccb *);
int	amr_ccb_map(struct amr_softc *, struct amr_ccb *, void *, int, int);
int	amr_ccb_poll(struct amr_softc *, struct amr_ccb *, int);
void	amr_ccb_unmap(struct amr_softc *, struct amr_ccb *);
int	amr_ccb_wait(struct amr_softc *, struct amr_ccb *);
const char	*amr_drive_state(int, int *);

extern int	amr_max_xfer;

#endif	/* !_PCI_AMRVAR_H_ */

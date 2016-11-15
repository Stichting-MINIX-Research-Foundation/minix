/*	$NetBSD: tropicvar.h,v 1.14 2012/10/27 17:18:23 chs Exp $	*/

/*
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
 * Copyright (c) 1991 IBM Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation,
 * and that the name IBM not be used in advertising or publicity
 * pertaining to distribution of the software without specific, written
 * prior permission.
 *
 * CARNEGIE MELLON AND IBM ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND IBM DISCLAIM ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/* $ACIS:if_lanvar.h 12.0$ */

#include <sys/callout.h>

/*
 * This file contains structures used in the "tr" driver for the
 *	IBM TOKEN-RING NETWORK PC ADAPTER
 */

/* Receive buffer control block */
struct rbcb {
	bus_size_t	rbufp;		/* offset of current receive buffer */
	bus_size_t	rbufp_next;	/* offset of next receive buffer */
	bus_size_t	rbuf_datap;	/* offset of data in receive buffer */
	unsigned short  data_len;	/* amount of data in this rec buffer */
};

/*
 *	Token-Ring software status per adapter
 */
struct	tr_softc {
	device_t sc_dev;
	void 	*sc_ih;
	struct ethercom sc_ethercom;
	struct ifmedia	sc_media;
	u_char	sc_xmit_correlator;
	int	sc_xmit_buffers;
#if 1
	int	sc_xmit_head;
	int	sc_xmit_tail;
#endif
	int	sc_minbuf;
	int	sc_nbuf;
	bus_size_t sc_txca;

	bus_space_tag_t sc_piot;
	bus_space_tag_t sc_memt;
	bus_space_handle_t sc_pioh;	/* handle pio area */
	bus_space_handle_t sc_sramh;	/* handle for the shared ram area */
	bus_space_handle_t sc_mmioh;	/* handle for the bios/mmio area */

	struct callout sc_init_callout;
	struct callout sc_reinit_callout;

	int (*sc_mediachange)(struct tr_softc *);
	void (*sc_mediastatus)(struct tr_softc *, struct ifmediareq *);
	struct rbcb rbc;	/* receiver buffer control block */
	bus_size_t sc_aca;	/* offset of adapter ACA */
	bus_size_t sc_ssb;	/* offset of System Status Block */
	bus_size_t sc_arb;	/* offset of Adapter Request Block */
	bus_size_t sc_srb;	/* offset of System Request Block */
	bus_size_t sc_asb;	/* offset of Adapter Status Block */
	u_int sc_maddr;		/* mapped shared memory address */
	u_int sc_memwinsz;	/* mapped shared memory window size */
	u_int sc_memsize;	/* memory installed on adapter */
	u_int sc_memreserved;	/* reserved memory on adapter */
	int sc_dhb4maxsz;	/* max. dhb size at 4MBIT ring speed */
	int sc_dhb16maxsz;	/* max. dbh size at 16MBIT ring speed */
	int sc_maxmtu;		/* max. MTU supported by adapter */
	unsigned char	sc_init_status;
	void * tr_sleepevent;     	/* tr event signalled on successful */
					/* open of adapter  */
	unsigned short exsap_station;	/* station assigned by open sap cmd */

	void *sc_sdhook;

	/* Power management hooks */
	int (*sc_enable)(struct tr_softc *);
	void (*sc_disable)(struct tr_softc *);
	int sc_enabled;
};

int tr_config(struct tr_softc *);
int tr_attach(struct tr_softc *);
int tr_intr(void *);
void tr_init(void *);
int tr_ioctl(struct ifnet *, u_long, void *);
void tr_stop(struct tr_softc *);
int tr_reset(struct tr_softc *);
void tr_sleep(struct tr_softc *);
int tr_setspeed(struct tr_softc *, u_int8_t);
int tr_enable(struct tr_softc *);
void tr_disable(struct tr_softc *);
int tr_activate(device_t, enum devact);
int tr_detach(device_t, int flags);

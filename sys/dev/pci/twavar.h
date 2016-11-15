/*	$NetBSD: twavar.h,v 1.12 2012/10/27 17:18:35 chs Exp $ */
/*	$wasabi: twavar.h,v 1.12 2006/05/01 15:16:59 simonb Exp $	*/

/*-
 * Copyright (c) 2003-04 3ware, Inc.
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
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
 *	$FreeBSD: src/sys/dev/twa/twa.h,v 1.4 2004/06/16 09:47:00 phk Exp $
 */

/*
 * 3ware driver for 9000 series storage controllers.
 *
 * Author: Vinod Kashyap
 */
#ifndef _PCI_TWAVAR_H_
#define	_PCI_TWAVAR_H_

#include "locators.h"

struct twa_callbacks {
	void	(*tcb_openings)(device_t, int);
};

struct twa_drive {
	uint32_t	td_id;
	uint64_t	td_size;
	int		td_openings;
	device_t	td_dev;
	const struct twa_callbacks *td_callbacks;
};

/* Per-controller structure. */
struct twa_softc {
	device_t		twa_dv;
	bus_space_tag_t		twa_bus_iot;	/* bus space tag */
	bus_space_handle_t	twa_bus_ioh;	/* bus space handle */
	bus_dma_tag_t		twa_dma_tag;	/* data buffer DMA tag */
	bus_dmamap_t		twa_cmd_map;	/* DMA map for the array of cmd pkts */
	void			*twa_ih;	/* interrupt handle cookie */
	void *			twa_cmds;
	bus_addr_t		twa_cmd_pkt_phys;/* phys addr of first of array of cmd pkts */

	pci_chipset_tag_t 	pc;
	pcitag_t		tag;
	/* Request queues and arrays. */
	TAILQ_HEAD(, twa_request) twa_free;	/* free request packets */
	TAILQ_HEAD(, twa_request) twa_busy;	/* requests busy in the controller */
	TAILQ_HEAD(, twa_request) twa_pending;	/* internal requests pending */

	struct twa_request	*twa_lookup[TWA_Q_LENGTH];/* requests indexed by request_id */

	struct twa_request	*twa_req_buf;
	struct twa_command_packet *twa_cmd_pkt_buf;

	struct twa_drive	*sc_units;
	/* AEN handler fields. */
	struct tw_cl_event_packet *twa_aen_queue[TWA_Q_LENGTH];/* circular queue of AENs from firmware */
	uint16_t		working_srl;	/* driver & firmware negotiated srl */
	uint16_t		working_branch;	/* branch # of the firmware that the driver is compatible with */
	uint16_t		working_build;	/* build # of the firmware that the driver is compatible with */
	uint32_t		twa_operating_mode; /* base mode/current mode */
	uint32_t		twa_aen_head;	/* AEN queue head */
	uint32_t		twa_aen_tail;	/* AEN queue tail */
	uint32_t		twa_current_sequence_id;/* index of the last event + 1 */
	uint32_t		twa_aen_queue_overflow;	/* indicates if unretrieved events were overwritten */
	uint32_t		twa_aen_queue_wrapped;	/* indicates if AEN queue ever wrapped */
	/* Controller state. */
	uint32_t		twa_state;
	uint32_t		twa_sc_flags;

	struct _twa_ioctl_lock{
		uint32_t	lock;		/* lock state */
		uint32_t	timeout;	/* time at which the lock
						 * will become available,
						 * even if not released
						 */
	} twa_ioctl_lock;			/* lock for use by user
						 * applications,
						 * for synchronization between
						 * ioctl calls
						 */
	int			sc_nunits;

	struct twa_request      *sc_twa_request;
	uint32_t		sc_product_id;
	unsigned		sc_quirks;
};



/* Possible values of tr->tr_status. */
#define TWA_CMD_SETUP		0x0	/* being assembled */
#define TWA_CMD_BUSY		0x1	/* submitted to controller */
#define TWA_CMD_PENDING		0x2	/* in pending queue */
#define TWA_CMD_COMPLETE	0x3	/* completed by controller */

/* Possible values of tr->tr_flags. */
#define TWA_CMD_DATA_IN			(1 << 0)
#define TWA_CMD_DATA_OUT		(1 << 1)
#define TWA_CMD_DATA_COPY_NEEDED	(1 << 2)
#define TWA_CMD_SLEEP_ON_REQUEST	(1 << 3)
#define TWA_CMD_IN_PROGRESS		(1 << 4)
#define TWA_CMD_AEN			(1 << 5)
#define TWA_CMD_AEN_BUSY		(1 << 6)

/* Possible values of tr->tr_cmd_pkt_type. */
#define TWA_CMD_PKT_TYPE_7K		(1<<0)
#define TWA_CMD_PKT_TYPE_9K		(1<<1)
#define TWA_CMD_PKT_TYPE_INTERNAL	(1<<2)
#define TWA_CMD_PKT_TYPE_IOCTL		(1<<3)
#define TWA_CMD_PKT_TYPE_EXTERNAL	(1<<4)

/* Possible values of sc->twa_state. */
#define TWA_STATE_INTR_ENABLED		(1<<0)	/* interrupts have been enabled */
#define TWA_STATE_SHUTDOWN		(1<<1)	/* controller is shut down */
#define TWA_STATE_OPEN			(1<<2)	/* control device is open */
#define TWA_STATE_SIMQ_FROZEN		(1<<3)	/* simq frozen */
#define TWA_STATE_REQUEST_WAIT		(1<<4)
#define TWA_STATE_IN_RESET		(1<<5)	/* controller being reset */

/* Possible values of sc->twa_ioctl_lock.lock. */
#define TWA_LOCK_FREE		0x0	/* lock is free */
#define TWA_LOCK_HELD		0x1	/* lock is held */

/* Possible values of sc->sc_quirks. */
#define TWA_QUIRK_QUEUEFULL_BUG	0x1

/* Driver's request packet. */
struct twa_request {
	struct twa_command_packet *tr_command;
	/* ptr to cmd pkt submitted to controller */
	uint32_t		tr_request_id;
	/* request id for tracking with firmware */
	void			*tr_data;
	/* ptr to data being passed to firmware */
	size_t			tr_length;
	/* length of buffer being passed to firmware */
	void			*tr_real_data;
	/* ptr to, and length of data passed */
	size_t			tr_real_length;
	/* to us from above, in case a buffer copy
	 * was done due to non-compliance to
	 * alignment requirements
	 */
	TAILQ_ENTRY(twa_request) tr_link;
	/* to link this request in a list */
	struct twa_softc	*tr_sc;
	/* controller that owns us */
	uint32_t		tr_status;
	/* command status */
	uint32_t		tr_flags;
	/* request flags */
	uint32_t		tr_error;
	/* error encountered before request submission */
	uint32_t		tr_cmd_pkt_type;
	/* request specific data to use during callback */
	void			(*tr_callback)(struct twa_request *tr);
	/* callback handler */
	bus_addr_t		tr_cmd_phys;
	/* physical address of command in controller space */
	bus_dmamap_t		tr_dma_map;
	/* DMA map for data */
	struct buf		*bp;

	struct ld_twa_softc	*tr_ld_sc;
};

static __inline size_t twa_get_maxsegs(void) {
	size_t max_segs = ((MAXPHYS + PAGE_SIZE - 1) / PAGE_SIZE) + 1;
#ifdef TWA_SG_SIZE
	if (TWA_MAX_SG_ELEMENTS < max_segs)
	    max_segs = TWA_MAX_SG_ELEMENTS;
#endif
	return max_segs;
}

static __inline size_t twa_get_maxxfer(size_t maxsegs) {
	return (maxsegs - 1) * PAGE_SIZE;
}

struct twa_attach_args {
	int	twaa_unit;
};

#define twaacf_unit	cf_loc[TWACF_UNIT]

struct twa_request *twa_get_request(struct twa_softc *, int);
struct twa_request *twa_get_request_wait(struct twa_softc *, int);
int twa_map_request(struct twa_request *);
void	twa_register_callbacks(struct twa_softc *sc, int unit,
		const struct twa_callbacks *);
void	twa_request_wait_handler(struct twa_request *);
void	twa_release_request(struct twa_request *);

/* Error/AEN message structure. */
struct twa_message {
	uint32_t	code;
	const char	*message;
};

#endif	/* !_PCI_TWAVAR_H_ */

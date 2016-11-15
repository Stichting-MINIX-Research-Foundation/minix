/* $NetBSD: seeq8005var.h,v 1.9 2015/04/13 16:33:24 riastradh Exp $ */

/*
 * Copyright (c) 2000 Ben Harris
 * Copyright (c) 1995 Mark Brinicombe
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mark Brinicombe.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _SEEQ8005VAR_H_
#define _SEEQ8005VAR_H_

#include <net/if_media.h>

#include <sys/rndsource.h>

/* Enumerate the possible cip variants */
enum seeq_variant {
	SEEQ_8004, SEEQ_8005
};

/*
 * per-line info and status
 */

struct seeq8005_softc {
	device_t sc_dev;

	/* These fields should be initialised by the board driver. */
	bus_space_tag_t sc_iot;		/* I/O base addr */
	bus_space_handle_t sc_ioh;

	int (*sc_mediachange)(struct seeq8005_softc *);
	void (*sc_mediastatus)(struct seeq8005_softc *, struct ifmediareq *);

	/* These fields are used internally by the seeq8005 driver. */
	struct ethercom sc_ethercom;	/* Ethernet common */
	struct ifmedia sc_media;	/* supported media information */

	int sc_config1;			/* Current config1 bits */
	int sc_config2;			/* Current config2 bits */
	int sc_config3;			/* Current config3 bits 8004 only */
	int sc_command;			/* Current command bits */
	u_int sc_rx_ptr;		/* Receive buffer pointer */

	u_int sc_tx_bufs;		/* Number of tx buffers */
	u_int sc_tx_used;		/* Number of tx buffers in use */
	u_int sc_tx_cur;		/* Next tx buffer to transmit */
	u_int sc_tx_next;		/* Next tx buffer to fill */

	u_int sc_buffersize;		/* Size of packet buffer */
	u_int sc_tx_bufsize;		/* Size of TX packet buffer */
	u_int sc_rx_bufsize;		/* Size of RX packet buffer */

	int sc_enabled;			/* Enabled state */
	enum seeq_variant sc_variant;	/* Chip variant */
	int sc_flags;
#define SF_8BIT		0x01
	krndsource_t rnd_source;
};

extern void seeq8005_attach(struct seeq8005_softc *, const u_int8_t *, int *,
    int, int);
extern int seeq8005intr(void *);

#endif

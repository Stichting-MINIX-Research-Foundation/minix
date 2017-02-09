/*	$NetBSD: iwicvar.h,v 1.6 2012/10/27 17:18:34 chs Exp $	*/

/*
 * Copyright (c) 1999, 2000 Dave Boyce. All rights reserved.
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
 */

#ifndef _IWICVAR_H_
#define _IWICVAR_H_

#include <netisdn/i4b_debug.h>
#include <netisdn/i4b_ioctl.h>
#include <netisdn/i4b_trace.h>

#include <netisdn/i4b_l2.h>
#include <netisdn/i4b_l1l2.h>
#include <netisdn/i4b_l3l4.h>

/*---------------------------------------------------------------------------*
 *	state of a B channel
 *---------------------------------------------------------------------------*/
struct iwic_bchan {
	int channel;		/* channel number	 */
	int offset;		/* offset from iobase	 */
	int bprot;		/* b channel protocol used */
	int state;		/* transceiver state:	 */
#define ST_IDLE		0x00	/* channel idle		 */
#define ST_TX_ACTIVE	0x01	/* tx running		 */

	unsigned int sc_trace_bcount;

	/* receive data from ISDN */

	struct ifqueue rx_queue;/* receiver queue	 */
	int rxcount;		/* rx statistics counter */
	struct mbuf *in_mbuf;	/* rx input buffer	 */
	u_char *in_cbptr;	/* curr buffer pointer	 */
	int in_len;		/* rx input buffer len	 */

	/* transmit data to ISDN */

	struct ifqueue tx_queue;/* transmitter queue		 */
	int txcount;		/* tx statistics counter	 */
	struct mbuf *out_mbuf_head;	/* first mbuf in possible chain */
	struct mbuf *out_mbuf_cur;	/* current mbuf in possbl chain */
	unsigned char *out_mbuf_cur_ptr;	/* data pointer into mbuf    */
	int out_mbuf_cur_len;	/* remaining bytes in mbuf    */

	/* linktab */

	isdn_link_t iwic_isdn_linktab;
	const struct isdn_l4_driver_functions *l4_driver;
	void *l4_driver_softc;
};
/*---------------------------------------------------------------------------*
 *	state of a D channel
 *---------------------------------------------------------------------------*/
struct iwic_dchan {
	int enabled;
	int trace_count;
	struct mbuf *ibuf;
	u_char *ibuf_ptr;	/* Input buffer pointer */
	int ibuf_len;		/* Current length of input buffer */
	int ibuf_max_len;	/* Max length in input buffer */
	int rx_count;

	int tx_ready;		/* Can send next 64 bytes of data. */
	int tx_count;

	struct mbuf *obuf;
	int free_obuf;
	u_char *obuf_ptr;
	int obuf_len;

	struct mbuf *obuf2;
	int free_obuf2;
};
/*---------------------------------------------------------------------------*
 *	state of one iwic unit
 *---------------------------------------------------------------------------*/
struct iwic_softc {
	device_t sc_dev;

	const char *sc_cardname;

	bus_addr_t sc_iobase;
	bus_size_t sc_iosize;
	bus_space_handle_t sc_io_bh;
	bus_space_tag_t sc_io_bt;

	struct iwic_dchan sc_dchan;
	struct iwic_bchan sc_bchan[2];

	void *sc_ih;		/* interrupt handler */
	pci_chipset_tag_t sc_pc;

	void *sc_l3token;	/* pointer to registered L3 instance */
	struct l2_softc sc_l2;	/* D-channel variables */

	int sc_I430state;
	int sc_I430T3;

	int sc_trace;
};
/*---------------------------------------------------------------------------*
 *	rd/wr register/fifo macros
 *---------------------------------------------------------------------------*/

#include <sys/bus.h>

#define IWIC_READ(sc,reg)	bus_space_read_1((sc)->sc_io_bt,(sc)->sc_io_bh,(reg))
#define IWIC_WRITE(sc,reg,val)	bus_space_write_1((sc)->sc_io_bt,(sc)->sc_io_bh,(reg),(val))
#define IWIC_WRDFIFO(sc,p,l)    bus_space_write_multi_1((sc)->sc_io_bt,(sc)->sc_io_bh,D_XFIFO,(p),(l))
#define IWIC_RDDFIFO(sc,p,l)    bus_space_read_multi_1((sc)->sc_io_bt,(sc)->sc_io_bh,D_RFIFO,(p),(l))
#define IWIC_WRBFIFO(sc,b,p,l)  bus_space_write_multi_1((sc)->sc_io_bt,(sc)->sc_io_bh,(b)->offset + B_XFIFO,(p),(l))
#define IWIC_RDBFIFO(sc,b,p,l)  bus_space_read_multi_1((sc)->sc_io_bt,(sc)->sc_io_bh,(b)->offset + B_RFIFO,(p),(l))

/*---------------------------------------------------------------------------*
 *	possible I.430 states
 *---------------------------------------------------------------------------*/
enum I430states {
	ST_F3N,			/* F3 Deactivated, no clock	 */
	ST_F3,			/* F3 Deactivated		 */
	ST_F4,			/* F4 Awaiting Signal		 */
	ST_F5,			/* F5 Identifying Input		 */
	ST_F6,			/* F6 Synchronized		 */
	ST_F7,			/* F7 Activated			 */
	ST_F8,			/* F8 Lost Framing		 */
	ST_ILL,			/* Illegal State		 */
	N_STATES
};
/*---------------------------------------------------------------------------*
 *	possible I.430 events
 *---------------------------------------------------------------------------*/
enum I430events {
	EV_PHAR,		/* PH ACTIVATE REQUEST          */
	EV_CE,			/* Clock enabled                */
	EV_T3,			/* Timer 3 expired              */
	EV_INFO0,		/* receiving INFO0              */
	EV_RSY,			/* receiving any signal         */
	EV_INFO2,		/* receiving INFO2              */
	EV_INFO48,		/* receiving INFO4 pri 8/9      */
	EV_INFO410,		/* receiving INFO4 pri 10/11    */
	EV_DR,			/* Deactivate Request           */
	EV_PU,			/* Power UP                     */
	EV_DIS,			/* Disconnected (only 2085)     */
	EV_EI,			/* Error Indication             */
	EV_ILL,			/* Illegal Event                */
	N_EVENTS
};
/*---------------------------------------------------------------------------*
 *	available commands
 *---------------------------------------------------------------------------*/
enum I430commands {
	CMD_ECK,		/* Enable clock			 */
	CMD_TIM,		/* Timing			 */
	CMD_RT,			/* Reset			 */
	CMD_AR8,		/* Activation request pri 8	 */
	CMD_AR10,		/* Activation request pri 10	 */
	CMD_DIU,		/* Deactivate Indication Upstream */
	CMD_ILL			/* Illegal command		 */
};

void iwic_init(struct iwic_softc *);
void iwic_next_state(struct iwic_softc *, int);

void iwic_dchan_init(struct iwic_softc *);
void iwic_dchan_xirq(struct iwic_softc *);
void iwic_dchan_xfer_irq(struct iwic_softc *, int);
void iwic_dchan_disable(struct iwic_softc *);
int iwic_dchan_data_req(struct iwic_softc *, struct mbuf *, int);
void iwic_dchan_transmit(struct iwic_softc *);

const char *iwic_printstate(struct iwic_softc *);

void iwic_init_linktab(struct iwic_softc *);
void iwic_bchan_xirq(struct iwic_softc *, int);
void iwic_bchannel_setup(isdn_layer1token, int, int, int);

#endif /* _IWICVAR_H_ */

/*	$KAME: dccp_cc_sw.h,v 1.6 2005/02/10 04:25:38 itojun Exp $	*/
/*	$NetBSD: dccp_cc_sw.h,v 1.1 2015/02/10 19:11:52 rjs Exp $ */

/*
 * Copyright (c) 2003  Nils-Erik Mattsson 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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
 *
 * Id: dccp_cc_sw.h,v 1.13 2003/05/13 15:31:50 nilmat-8 Exp
 */

#ifndef _NETINET_DCCP_CC_SW_H_
#define _NETINET_DCCP_CC_SW_H_

/* All functions except inits has the ccb as first argument */

/* Sender side */

/* Init the sender side
 * args: dccpcb of current connection
 * returns: sender ccb
 */
typedef void *  cc_send_init_t     (struct dccpcb *);

/* Free the sender side */
typedef void   cc_send_free_t     (void *);

/* Ask the cc mechanism if dccp is allowed to send a packet
 * args: the packet size (0 == ACK)
 * returns: 1 if allowed to send, otherwise 0
 */
typedef int    cc_send_packet_t   (void *, long);

/* Inform the cc mechanism that a packet was sent
 * args:  if there exists more to send or not
 *        size of packet sent
 */
typedef void   cc_send_packet_sent_t (void *, int, long);

/* Inform the cc mechanism (sender) that a packet has been received
 * args:  options and option length
 */
typedef void   cc_send_packet_recv_t (void *, char *, int);


/* Receiver side */

/* Init the receiver side
 * args: dccpcb of current connection
 * returns: receiver ccb
 */
typedef void *  cc_recv_init_t   (struct dccpcb *);

/* Free the receiver side */
typedef void   cc_recv_free_t   (void *);

/* Inform the cc mechanism (receiver) that a packet was received
 * args:  options and option length
 */
typedef void   cc_recv_packet_recv_t (void *, char *, int);

struct dccp_cc_sw {
	/* Sender side */
	cc_send_init_t   *cc_send_init;
	cc_send_free_t   *cc_send_free;
	cc_send_packet_t *cc_send_packet;
	cc_send_packet_sent_t *cc_send_packet_sent;
	cc_send_packet_recv_t *cc_send_packet_recv;

	/* Receiver side */
	cc_recv_init_t   *cc_recv_init;
	cc_recv_free_t   *cc_recv_free;
	cc_recv_packet_recv_t   *cc_recv_packet_recv;
};

/* Max ccid (i.e. cc_sw has DCCP_CC_MAX_CCID+2 elements) */
#define DCCP_CC_MAX_CCID 2

#endif

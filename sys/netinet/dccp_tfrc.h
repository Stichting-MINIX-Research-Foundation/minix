/*	$KAME: dccp_tfrc.h,v 1.10 2005/10/26 11:36:49 nishida Exp $	*/
/*	$NetBSD: dccp_tfrc.h,v 1.1 2015/02/10 19:11:52 rjs Exp $ */

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
 * Id: dccp_tfrc.h,v 1.34 2003/05/28 17:36:15 nilmat-8 Exp
 */


#ifndef _NETINET_DCCP_TFRC_H_
#define _NETINET_DCCP_TFRC_H_

#define TFRC_STD_PACKET_SIZE    256
#define TFRC_MIN_PACKET_SIZE    16
#define TFRC_MAX_PACKET_SIZE    65535

#define TFRC_OPSYS_TIME_GRAN    10000
#define TFRC_WIN_COUNT_LIMIT    16
#define TFRC_WIN_COUNT_PER_RTT  4
#define TFRC_SMALLEST_P         4	/* 0.00004 */
/* 
 * TFRC sender 
 */

/* TFRC sender states */
#define TFRC_SSTATE_NO_SENT     1
#define TFRC_SSTATE_NO_FBACK	2
#define TFRC_SSTATE_FBACK	3
#define TFRC_SSTATE_TERM	4

/* Mechanism parameters */
#define TFRC_INITIAL_TIMEOUT    2
#define TFRC_MAX_BACK_OFF_TIME  64
#define TFRC_RTT_FILTER_CONST   9000	/* 0.9 */
#define TFRC_SEND_WAIT_TERM     20

/* Packet history */
TAILQ_HEAD(s_hist_head,s_hist_entry); 

struct fixpoint {
	long long num;
	long long denom;
};
 
struct s_hist_entry {
	TAILQ_ENTRY(s_hist_entry) linfo;	/* Tail queue. */
	u_int64_t seq;		/* Sequence number */
	struct timeval t_sent;	/* When the packet was sent */
	u_int8_t win_count;	/* Windowcounter for packet */
};

/* TFRC sender congestion control block (ccb) */
struct tfrc_send_ccb {
	kmutex_t mutex;		/* Lock for this structure */
	struct dccpcb *pcb;	/* Pointer to associated dccpcb */
	u_int8_t state;		/* Sender state */

	struct fixpoint x;		/* Current sending rate */
	struct fixpoint x_recv;	/* Receive rate */
	struct fixpoint x_calc;	/* Calculated send (?) rate */ 

	u_int16_t s;		/* Packet size */

	u_int32_t rtt;		/* Estimate of current round trip time */
	struct fixpoint p;	/* Current loss event rate */  
	u_int8_t last_win_count;	/* Last window counter sent */
	/* Timestamp of earliest packet with last_win_count value sent */
	struct timeval t_last_win_count;
	u_int8_t idle;
	u_int32_t t_rto;	/* Time out value = 4*rtt */
	struct timeval t_ld;	/* Time last doubled during slow start */

	struct timeval t_nom;	/* Nominal send time of next packet */
	struct timeval t_ipi;	/* Interpacket (send) interval */
	struct timeval delta;	/* Send timer delta */    

	struct callout ch_stimer;	/* Handle to scheduled send timer */ 
	struct callout ch_nftimer;	/* Handle to no feedback timer */

	struct s_hist_head hist;	/* Packet history */
};

#ifdef _KERNEL

/* Functions declared in struct dccp_cc_sw */

/*
 * Initialises the sender side
 * args: pcb  - pointer to dccpcb of associated connection
 * returns: pointer to a tfrc_send_ccb struct on success, otherwise 0
 */ 
void *tfrc_send_init(struct dccpcb *); 

/*
 * Free the sender side
 * args: ccb - ccb of sender
 */
void tfrc_send_free(void *);

/*
 * Ask TFRC wheter one can send a packet or not 
 * args: ccb  -  ccb block for current connection
 * returns: 1 if ok, else 0.
 */ 
int tfrc_send_packet(void *, long);

/*
 * Notify sender that a packet has been sent 
 * args: ccb - ccb block for current connection
 *	 moreToSend - if there exists more packets to send
 *       datasize   - packet size
 */
void tfrc_send_packet_sent(void *, int, long);

/*
 * Notify that a an ack package was received (i.e. a feedback packet)
 * args: ccb  -  ccb block for current connection
 */ 
void tfrc_send_packet_recv(void *, char *, int);

#endif

/* 
 * TFRC Receiver 
 */

/* TFRC specific dccp options */
#define TFRC_OPT_LOSS_RATE	192
#define TFRC_OPT_LOSS_INTERVAL	193
//#define TFRC_OPT_ELAPSED_TIME	193
#define TFRC_OPT_RECEIVE_RATE	194

/* TFRC receiver states */
#define TFRC_RSTATE_NO_DATA	1
#define TFRC_RSTATE_DATA	2
#define TFRC_RSTATE_TERM        127

/* Receiver mechanism parameters */
/*
 * seq_num x,y; if y-x is smaller than this number (note, wrap around) then
 * y is newer than x
 */
#define TFRC_RECV_NEW_SEQ_RANGE 10000000
/* number of later packets received before one is considered lost */
#define TFRC_RECV_NUM_LATE_LOSS 3
/* length(w[]) */
#define TFRC_RECV_IVAL_F_LENGTH  8

/* Packet history */
TAILQ_HEAD(r_hist_head,r_hist_entry); 
 
struct r_hist_entry {
	TAILQ_ENTRY(r_hist_entry) linfo;	/* Tail queue. */
	u_int64_t seq;		/* Sequence number */
	struct timeval t_recv;	/* When the packet was received */
	u_int8_t win_count;	/* Window counter for that packet */
	u_int8_t type;		/* Packet type received */
	u_int8_t ndp;		/* no data packets value */
};

/* Loss interval history */
TAILQ_HEAD(li_hist_head,li_hist_entry); 
 
struct li_hist_entry {
	TAILQ_ENTRY(li_hist_entry) linfo;	/* Tail queue. */
	u_int32_t interval;	/* Loss interval */
	u_int64_t seq;		/* Sequence number of the packet that started the interval */
	u_int8_t win_count;	/* Window counter for previous received packet */
};

/* TFRC receiver congestion control block (ccb) */
struct tfrc_recv_ccb {
	kmutex_t mutex;		/* Lock for this structure */
	struct dccpcb *pcb;	/* Pointer to associated dccpcb */
	u_int8_t state;		/* Receiver state */

	struct fixpoint p;	/* Loss event rate */

	struct li_hist_head li_hist;	/* Loss interval history */

	/*
	 * Highest value of the window counter received when last feedback
	 * was sent
	 */
	u_int8_t	last_counter;
	/* Sequence number of the packet above */
	u_int64_t	seq_last_counter;

	/* Timestamp of when last feedback was sent */
	struct timeval t_last_feedback;
	u_int32_t bytes_recv;	/* Bytes received since t_last_feedback */

	struct r_hist_head hist;	/* Packet history */

	u_int16_t s;		/* Packet size */
};

#ifdef _KERNEL

/* Functions declared in struct dccp_cc_sw */

/* Initialises the receiver side
 * args: pcb  -  pointer to dccpcb of associated connection
 * returns: pointer to a tfrc_recv_ccb struct on success, otherwise 0
 */ 
void *tfrc_recv_init(struct dccpcb *); 

/* Free the receiver side
 * args: ccb - ccb of recevier
 */
void tfrc_recv_free(void *);

/*
 * Tell TFRC that a packet has been received
 * args: ccb  -  ccb block for current connection 
 */
void tfrc_recv_packet_recv(void *, char *, int);

#endif

#endif

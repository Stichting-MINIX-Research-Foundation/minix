/*	$KAME: dccp_tcplike.c,v 1.19 2005/07/27 06:27:25 nishida Exp $	*/
/*	$NetBSD: dccp_tcplike.c,v 1.2 2015/08/24 22:21:26 pooka Exp $ */

/*
 * Copyright (c) 2003 Magnus Erixzon
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
 */
/*
 * TCP-like congestion control for DCCP
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dccp_tcplike.c,v 1.2 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_dccp.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>

#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>
#include <netinet/ip_var.h>

#include <netinet/dccp.h>
#include <netinet/dccp_var.h>
#include <netinet/dccp_tcplike.h>

#define TCPLIKE_DEBUG(args) dccp_log args
#define MALLOC_DEBUG(args) log args
#define CWND_DEBUG(args) dccp_log args
#define ACKRATIO_DEBUG(args) dccp_log args
#define LOSS_DEBUG(args) dccp_log args
#define TIMEOUT_DEBUG(args) dccp_log args

#if !defined(__FreeBSD__) || __FreeBSD_version < 500000
#define	INP_INFO_LOCK_INIT(x,y)
#define	INP_INFO_WLOCK(x)
#define INP_INFO_WUNLOCK(x)
#define	INP_INFO_RLOCK(x)
#define INP_INFO_RUNLOCK(x)
#define	INP_LOCK(x)
#define INP_UNLOCK(x)
#endif

/* Sender side */

void tcplike_rto_timeout(void *);
void tcplike_rtt_sample(struct tcplike_send_ccb *, u_int16_t);
void _add_to_cwndvector(struct tcplike_send_ccb *, u_int64_t);
void _remove_from_cwndvector(struct tcplike_send_ccb *, u_int64_t);
int _chop_cwndvector(struct tcplike_send_ccb *, u_int64_t);
int _cwndvector_size(struct tcplike_send_ccb *);
u_char _cwndvector_state(struct tcplike_send_ccb *, u_int64_t);

void tcplike_send_term(void *);
void tcplike_recv_term(void *);

void _avlist_add(struct tcplike_recv_ccb *, u_int64_t, u_int64_t);
u_int64_t _avlist_get(struct tcplike_recv_ccb *, u_int64_t);

/* extern Ack Vector functions */
extern void dccp_use_ackvector(struct dccpcb *);
extern void dccp_update_ackvector(struct dccpcb *, u_int64_t);
extern void dccp_increment_ackvector(struct dccpcb *, u_int64_t);
extern u_int16_t dccp_generate_ackvector(struct dccpcb *, u_char *);
extern u_char dccp_ackvector_state(struct dccpcb *, u_int32_t);

extern int dccp_get_option(char *, int, int, char *, int);
extern int dccp_remove_feature(struct dccpcb *, u_int8_t, u_int8_t);

/*
 * RTO timer activated
 */
void
tcplike_rto_timeout(void *ccb)
{
	struct tcplike_send_ccb *cb = (struct tcplike_send_ccb *) ccb;
	/*struct inpcb *inp;*/
	int s;
	
	mutex_enter(&(cb->mutex));
	
	cb->ssthresh = cb->cwnd >>1;
	cb->cwnd = 1; /* allowing 1 packet to be sent */
	cb->outstanding = 0; /* is this correct? */
	cb->rto_timer_callout = 0;
	cb->rto = cb->rto << 1;
	TIMEOUT_DEBUG((LOG_INFO, "RTO Timeout. New RTO = %u\n", cb->rto));
	
	cb->sample_rtt = 0;
	
	cb->ack_last = 0;
	cb->ack_miss = 0;

	cb->rcvr_ackratio = 1; /* Constraint 2 & 3. We need ACKs asap */
	dccp_remove_feature(cb->pcb, DCCP_OPT_CHANGE_R, DCCP_FEATURE_ACKRATIO);
	dccp_add_feature(cb->pcb, DCCP_OPT_CHANGE_R, DCCP_FEATURE_ACKRATIO,
				 (char *) &cb->rcvr_ackratio, 1);
	cb->acked_in_win = 0;
	cb->acked_windows = 0;
	cb->oldcwnd_ts = cb->pcb->seq_snd;
	
	LOSS_DEBUG((LOG_INFO, "Timeout. CWND value: %u , OUTSTANDING value: %u\n",
	    cb->cwnd, cb->outstanding));
	mutex_exit(&(cb->mutex));

	/* lock'n run dccp_output */
	s = splnet();
	INP_INFO_RLOCK(&dccpbinfo);
	/*inp = cb->pcb->d_inpcb;*/
	INP_LOCK(inp);
	INP_INFO_RUNLOCK(&dccpbinfo);
	
	dccp_output(cb->pcb, 1);
	
	INP_UNLOCK(inp);
	splx(s);
}

void tcplike_rtt_sample(struct tcplike_send_ccb *cb, u_int16_t sample)
{
	u_int16_t err;
	
	if (cb->rtt == 0xffff) {
		/* hmmmmm. */
		cb->rtt = sample;
		cb->rto = cb->rtt << 1;
		return;
	}

	/* This is how the Linux implementation is doing it.. */
	if (sample >= cb->rtt) {
		err = sample - cb->rtt;
		cb->rtt = cb->rtt + (err >> 3);
	} else {
		err = cb->rtt - sample;
		cb->rtt = cb->rtt - (err >> 3);
	}
	cb->rtt_d = cb->rtt_d + ((err - cb->rtt_d) >> 2);
	if (cb->rtt < TCPLIKE_MIN_RTT)
		cb->rtt = TCPLIKE_MIN_RTT;
	cb->rto = cb->rtt + (cb->rtt_d << 2);


	/* 5 million ways to calculate RTT ...*/
#if 0
	cb->srtt = ( 0.8 * cb->srtt ) + (0.2 * sample);
	if (cb->srtt < TCPLIKE_MIN_RTT)
		cb->srtt = TCPLIKE_MIN_RTT;
	cb->rto = cb->srtt << 1;
#endif
	
	LOSS_DEBUG((LOG_INFO, "RTT Sample: %u , New RTO: %u\n", sample, cb->rto));
}

/* Functions declared in struct dccp_cc_sw */

/*
 * Initialises the sender side
 * returns: pointer to a tfrc_send_ccb struct on success, otherwise 0
 */ 
void *
tcplike_send_init(struct dccpcb* pcb)
{
	struct tcplike_send_ccb *cb;
	
	TCPLIKE_DEBUG((LOG_INFO, "Entering tcplike_send_init()\n"));
	
	cb = malloc(sizeof (struct tcplike_send_ccb), M_PCB, M_NOWAIT | M_ZERO);
	if (cb == 0) {
		TCPLIKE_DEBUG((LOG_INFO, "Unable to allocate memory for tcplike_send_ccb!\n"));
		dccpstat.tcplikes_send_memerr++;
		return 0;
	}
	memset(cb, 0, sizeof (struct tcplike_send_ccb));
	
	/* init sender */
	cb->pcb = pcb;
	
	cb->cwnd = TCPLIKE_INITIAL_CWND;
	cb->ssthresh = 0xafff; /* lim-> infinity */
	cb->oldcwnd_ts = 0;
	cb->outstanding = 0;
	cb->rcvr_ackratio = 2; /* Ack Ratio */
	cb->acked_in_win = 0;
	cb->acked_windows = 0;
	
	CWND_DEBUG((LOG_INFO, "Init. CWND value: %u , OUTSTANDING value: %u\n",
		    cb->cwnd, cb->outstanding));
	cb->rtt = 0xffff;
	cb->rto = TIMEOUT_UBOUND;
	callout_init(&cb->rto_timer, 0);
	callout_init(&cb->free_timer, 0);
	cb->rto_timer_callout = 0;
	cb->rtt_d = 0;
	cb->timestamp = 0;
	
	cb->sample_rtt = 1;

	cb->cv_size = TCPLIKE_INITIAL_CWNDVECTOR;
	/* 1 bit per entry */
	cb->cwndvector = malloc(cb->cv_size / 8, M_PCB, M_NOWAIT | M_ZERO);
	if (cb->cwndvector == NULL) {
		MALLOC_DEBUG((LOG_INFO, "Unable to allocate memory for cwndvector\n"));
		/* What to do now? */
		cb->cv_size = 0;
		dccpstat.tcplikes_send_memerr++;
		return 0;
	}
	memset(cb->cwndvector, 0, cb->cv_size / 8);
	cb->cv_hs = cb->cv_ts = 0;
	cb->cv_hp = cb->cwndvector;

	cb->ack_last = 0;
	cb->ack_miss = 0;
	
	mutex_init(&(cb->mutex), MUTEX_DEFAULT, IPL_SOFTNET);
	
	TCPLIKE_DEBUG((LOG_INFO, "TCPlike sender initialised!\n"));
	dccpstat.tcplikes_send_conn++;
	return cb;
} 

void tcplike_send_term(void *ccb)
{
	struct tcplike_send_ccb *cb = (struct tcplike_send_ccb *) ccb;
	if (ccb == 0)
		return;
	
	mutex_destroy(&(cb->mutex));

	free(cb, M_PCB);
	TCPLIKE_DEBUG((LOG_INFO, "TCP-like sender is destroyed\n"));
}

/*
 * Free the sender side
 * args: ccb - ccb of sender
 */
void
tcplike_send_free(void *ccb)
{
	struct tcplike_send_ccb *cb = (struct tcplike_send_ccb *) ccb;
	
	LOSS_DEBUG((LOG_INFO, "Entering tcplike_send_free()\n"));

	if (ccb == 0)
		return;
	
	mutex_enter(&(cb->mutex));
	
	free(cb->cwndvector, M_PCB);
	cb->cv_hs = cb->cv_ts = 0;

	/* untimeout any active timer */
	if (cb->rto_timer_callout) {
		TCPLIKE_DEBUG((LOG_INFO, "Untimeout RTO Timer\n"));
		callout_stop(&cb->rto_timer);
		cb->rto_timer_callout = 0;
	}
	
	mutex_exit(&(cb->mutex));

	callout_reset(&cb->free_timer, 10 * hz, tcplike_send_term, (void *)cb);
}

/*
 * Ask TCPlike wheter one can send a packet or not 
 * args: ccb  -  ccb block for current connection 
 * returns: 0 if ok, else <> 0.
 */ 
int
tcplike_send_packet(void *ccb, long datasize)
{
	/* check if one can send here */
	struct tcplike_send_ccb *cb = (struct tcplike_send_ccb *) ccb;
	long ticks;
	char feature[1];
	
	TCPLIKE_DEBUG((LOG_INFO, "Entering tcplike_send_packet()\n"));

	if (datasize == 0) {
		TCPLIKE_DEBUG((LOG_INFO, "Sending pure ACK. Dont care about CC right now\n"));
		return 1;
	}

	mutex_enter(&(cb->mutex));

	if (cb->cwnd <= cb->outstanding) {
		/* May not send. trigger RTO */
		DCCP_DEBUG((LOG_INFO, "cwnd (%d) < outstanding (%d)\n", cb->cwnd, cb->outstanding));
		if (!cb->rto_timer_callout) {
			LOSS_DEBUG((LOG_INFO, "Trigger TCPlike RTO timeout timer. Ticks = %u\n", cb->rto));
			ticks = (long)cb->rto;
			callout_reset(&cb->rto_timer, ticks,
			    tcplike_rto_timeout, (void *)cb);
			cb->rto_timer_callout = 1;
		}
		mutex_exit(&(cb->mutex));
		return 0;
	}
	
	/* We're allowed to send */

	feature[0] = 1;
	if (cb->pcb->remote_ackvector == 0) {
		ACK_DEBUG((LOG_INFO, "Adding Change(Use Ack Vector, 1) to outgoing packet\n"));
		dccp_remove_feature(cb->pcb, DCCP_OPT_CHANGE_R, DCCP_FEATURE_ACKVECTOR);
		dccp_add_feature(cb->pcb, DCCP_OPT_CHANGE_R, DCCP_FEATURE_ACKVECTOR, feature, 1);
	}
	
	/* untimeout any active timer */
	if (cb->rto_timer_callout) {
		LOSS_DEBUG((LOG_INFO, "Untimeout RTO Timer\n"));
		callout_stop(&cb->rto_timer);
		cb->rto_timer_callout = 0;
	}

	if (!cb->sample_rtt) {
		struct timeval stamp;
		microtime(&stamp);
		cb->timestamp = ((stamp.tv_sec & 0x00000FFF) * 1000000) + stamp.tv_usec;
		dccp_add_option(cb->pcb, DCCP_OPT_TIMESTAMP, (char*) &(cb->timestamp), 4);
		/*LOSS_DEBUG((LOG_INFO, "Adding timestamp %u\n", cb->timestamp));*/
		cb->sample_rtt = 1;
	}
	
	mutex_exit(&(cb->mutex));
	return 1;
	
}

/*
 * Notify sender that a packet has been sent 
 * args: ccb - ccb block for current connection
 *	 moreToSend - if there exists more packets to send
 */
void
tcplike_send_packet_sent(void *ccb, int moreToSend, long datasize)
{
	struct tcplike_send_ccb *cb = (struct tcplike_send_ccb *) ccb;
	
	TCPLIKE_DEBUG((LOG_INFO, "Entering tcplike_send_packet_sent(,%i,%i)\n",moreToSend,(int) datasize));
	
	if (datasize == 0) {
		TCPLIKE_DEBUG((LOG_INFO, "Sent pure ACK. Dont care about cwnd-storing\n"));
		return;
	}

	mutex_enter(&(cb->mutex));
	
	cb->outstanding++;
	TCPLIKE_DEBUG((LOG_INFO, "SENT. cwnd: %d, outstanding: %d\n",cb->cwnd, cb->outstanding));

	/* stash the seqnr in cwndvector */
	/* Dont do this if we're only sending an ACK ! */
	_add_to_cwndvector(cb, cb->pcb->seq_snd);
	CWND_DEBUG((LOG_INFO, "Sent. CWND value: %u , OUTSTANDING value: %u\n",cb->cwnd, cb->outstanding));
	
	dccp_remove_feature(cb->pcb, DCCP_OPT_CHANGE_R, DCCP_FEATURE_ACKRATIO);
	mutex_exit(&(cb->mutex));
}

/*
 * Notify that an ack package was received
 * args: ccb  -  ccb block for current connection
 */
void
tcplike_send_packet_recv(void *ccb, char *options, int optlen)
{
	dccp_seq acknum, lastok;
	u_int16_t numlostpackets, avsize, i, prev_size;
	u_int8_t length, state, numokpackets, ackratiocnt;
	u_char av[10];
	struct tcplike_send_ccb *cb = (struct tcplike_send_ccb *) ccb;

	TCPLIKE_DEBUG((LOG_INFO, "Entering tcplike_send_ack_recv()\n"));
	mutex_enter(&(cb->mutex));

	if (dccp_get_option(options, optlen, DCCP_OPT_TIMESTAMP_ECHO, av,10) > 0) {
		u_int32_t echo, elapsed;

		TCPLIKE_DEBUG((LOG_INFO, "Received TIMESTAMP ECHO\n"));
		bcopy(av, &echo, 4);
		bcopy(av + 4, &elapsed, 4);

		if (echo == cb->timestamp) {
			struct timeval time;
			u_int32_t c_stamp;
			u_int16_t diff;
			
			microtime(&time);
			c_stamp = ((time.tv_sec & 0x00000FFF) * 1000000) + time.tv_usec;
			
			diff = (u_int16_t) c_stamp - cb->timestamp - elapsed;
			diff = (u_int16_t)(diff / 1000);
			TCPLIKE_DEBUG((LOG_INFO, "Got Timestamp Echo; Echo = %u, Elapsed = %u. DIFF = %u\n",
				       echo, elapsed, diff));
			tcplike_rtt_sample(cb, diff);
		}
	}

	if (cb->pcb->ack_rcv == 0) {
		/* There was no Ack. There is no spoon */

		/* We'll clear the missingacks data here, since the other host
		 * is also sending data.
		 * I guess we could deal with this, using the NDP field in the
		 * header. Let's stick a *TODO* mark here for now.
		 * The missingacks mechanism will activate if other host goes to
		 * only sending DCCP-Ack packets.
		 */
		cb->ack_last = 0;
		cb->ack_miss = 0;
		ACKRATIO_DEBUG((LOG_INFO, "Clear Missing Acks state!\n"));
		mutex_exit(&(cb->mutex));
		return;
	}

	cb->sample_rtt = 0;
	
	/* check ackVector for lost packets. cmp with cv_list */
	avsize = dccp_get_option(options, optlen, DCCP_OPT_ACK_VECTOR0, av,10);
	if (avsize == 0)
		avsize = dccp_get_option(options, optlen, DCCP_OPT_ACK_VECTOR1, av,10);

	if (avsize > 0)
		dccpstat.tcplikes_send_ackrecv++;
	
	acknum = cb->pcb->ack_rcv;
	numlostpackets = 0;
	numokpackets = 0;
	lastok = 0;
	prev_size = _cwndvector_size(cb);
	
	TCPLIKE_DEBUG((LOG_INFO, "Start removing from cwndvector %d\n", avsize));
	if (avsize == 0)
		_remove_from_cwndvector(cb, acknum);
	
	for (i=0; i < avsize; i++) {
		state = (av[i] & 0xc0) >> 6;
		length = (av[i] & 0x3f) +1;
		while (length > 0) {
			if (state == 0) {
				CWND_DEBUG((LOG_INFO, "Packet %llu was OK\n", acknum));
				numokpackets++;
				lastok = acknum;
				_remove_from_cwndvector(cb, acknum);
			} else {
				if (acknum > cb->oldcwnd_ts) {
					LOSS_DEBUG((LOG_INFO, "Packet %llu was lost %llu state %d\n", acknum, cb->oldcwnd_ts, state));
					numlostpackets++;
					dccpstat.tcplikes_send_reploss++;
				}
			}
			acknum--;
			length--;
		}
	}
	if (lastok)
		if (_chop_cwndvector(cb, lastok-TCPLIKE_NUMDUPACK)) {
			LOSS_DEBUG((LOG_INFO, "Packets were lost\n"));
			if (lastok-TCPLIKE_NUMDUPACK > cb->oldcwnd_ts) {
				numlostpackets++;
				dccpstat.tcplikes_send_assloss++;
			}
		}

	lastok = cb->cv_hs;
	while (_cwndvector_state(cb, lastok) == 0x00 && lastok < cb->cv_ts)
		lastok++;
	if (lastok != cb->cv_hs)
		_chop_cwndvector(cb, lastok);

	cb->outstanding = _cwndvector_size(cb);
	CWND_DEBUG((LOG_INFO, "Decrease outstanding. was = %u , now = %u\n", prev_size, cb->outstanding));
	if (prev_size == cb->outstanding) {
		/* Nothing dropped from cwndvector  */
		mutex_exit(&(cb->mutex));
		return;
	}
	
	cb->acked_in_win += numokpackets;
	
	if (cb->cwnd < cb->ssthresh) {
		/* Slow start */

		if (numlostpackets > 0) {
			/* Packet loss */
			LOSS_DEBUG((LOG_INFO, "Packet Loss in Slow Start\n"));
			cb->cwnd = cb->cwnd>>1;
			if (cb->cwnd < 1)
				cb->cwnd = 1;
			cb->ssthresh = cb->cwnd;
			cb->acked_in_win = 0;
			cb->acked_windows = 0;
			cb->oldcwnd_ts = cb->pcb->seq_snd;
			
		} else {
			cb->cwnd++;
		}
		
	} else if (cb->cwnd >= cb->ssthresh) {
		
		if (numlostpackets > 0) {
			/* Packet loss */
			LOSS_DEBUG((LOG_INFO, "Packet Loss in action\n"));
			cb->cwnd = cb->cwnd>>1;
			if (cb->cwnd < 1)
				cb->cwnd = 1;
			cb->ssthresh = cb->cwnd;
			cb->acked_in_win = 0;
			cb->acked_windows = 0;
			cb->oldcwnd_ts = cb->pcb->seq_snd;
			
		} else if (cb->acked_in_win > cb->cwnd) {
			cb->cwnd++;
		}
	}

	/* Ok let's check if there are missing Ack packets */
	ACKRATIO_DEBUG((LOG_INFO, "Check Ack. seq_rcv: %u ,ack_last: %u ,ack_miss: %u\n",
			cb->pcb->seq_rcv, cb->ack_last, cb->ack_miss));
	
	if (cb->ack_last == 0) {
		/* First received ack (or first after Data packet). Yey */
		cb->ack_last = cb->pcb->seq_rcv;
		cb->ack_miss = 0;
	} else if (cb->pcb->seq_rcv == (cb->ack_last + 1)) {
		/* This is correct, non-congestion, in-order behaviour */
		cb->ack_last = cb->pcb->seq_rcv;

	} else if (cb->pcb->seq_rcv < (cb->ack_last + 1)) {
		/* Might be an Ack we've been missing */
		/* This code has a flaw; If we miss 2 Ack packets, we only care
		 * about the older one. This means that the next-to-oldest one could
		 * be lost without any action beeing taken.
		 * Time will tell if that is going to be a Giant Problem(r)
		 */
		if (cb->pcb->seq_rcv == cb->ack_miss) {
			/* Yea it was. great */
			cb->ack_miss = 0;
		}
		
	} else if (cb->pcb->seq_rcv > (cb->ack_last + 1)) {
		/* There is a jump in Ack seqnums.. */
		cb->ack_miss = cb->ack_last + 1;
		cb->ack_last = cb->pcb->seq_rcv;
	}

	if (cb->ack_miss && ((cb->ack_miss + TCPLIKE_NUMDUPACK) < cb->ack_last)) {
		/* Alert! Alert! Ack packets are MIA.
		 * Decrease Ack Ratio
		 */
		cb->rcvr_ackratio = cb->rcvr_ackratio<<1;
		if (cb->rcvr_ackratio > (cb->cwnd>>1)) {
			/* Constraint 2 */
			cb->rcvr_ackratio = cb->cwnd>>1;
		}
		if (cb->rcvr_ackratio == 0)
			cb->rcvr_ackratio = 1;
		ACKRATIO_DEBUG((LOG_INFO, "Increase Ack Ratio. Now = %u. (cwnd = %u)\n", cb->rcvr_ackratio, cb->cwnd));
		dccp_remove_feature(cb->pcb, DCCP_OPT_CHANGE_R, DCCP_FEATURE_ACKRATIO);
		dccp_add_feature(cb->pcb, DCCP_OPT_CHANGE_R, DCCP_FEATURE_ACKRATIO,
				 (char *) &cb->rcvr_ackratio, 1);

		cb->ack_miss = 0;
		cb->acked_windows = 0;
		cb->acked_in_win = 0;
		dccpstat.tcplikes_send_missack++;
		
	} else if (cb->acked_in_win > cb->cwnd) {
		cb->acked_in_win = 0;
		cb->acked_windows++;
		if (cb->rcvr_ackratio == 1) {
			/* Ack Ratio is 1. We cant decrease it more.. Lets wait for some
			 * heavy congestion so we can increase it
			 */
			cb->acked_windows = 0;
		}
	}
	
	if (cb->acked_windows >= 1) {
		ackratiocnt = (cb->cwnd / ((cb->rcvr_ackratio*cb->rcvr_ackratio) - cb->rcvr_ackratio));
		if (cb->acked_windows >= ackratiocnt) {
			if (cb->rcvr_ackratio > 2 && cb->cwnd >= 4) {
				/* Constraint 3 - AckRatio at least 2 for a cwnd >= 4 */
				cb->rcvr_ackratio--;
				ACKRATIO_DEBUG((LOG_INFO, "Decrease ackratio by 1, now: %u\n", cb->rcvr_ackratio));
				dccp_remove_feature(cb->pcb, DCCP_OPT_CHANGE_R, DCCP_FEATURE_ACKRATIO);
				dccp_add_feature(cb->pcb, DCCP_OPT_CHANGE_R, DCCP_FEATURE_ACKRATIO,
						 (char *) &cb->rcvr_ackratio, 1);
			}
			cb->acked_in_win = 0;
			cb->acked_windows = 0;
		}
	}
	
	CWND_DEBUG((LOG_INFO, "Recvd. CWND value: %u , OUTSTANDING value: %u\n",
		    cb->cwnd, cb->outstanding));

	if (cb->cwnd > cb->outstanding && cb->rto_timer_callout) {
                LOSS_DEBUG((LOG_INFO, "Force DCCP_OUTPUT, CWND = %u Outstanding = %u\n",
                            cb->cwnd, cb->outstanding));
		callout_stop(&cb->rto_timer);
		cb->rto_timer_callout = 0;
		
		mutex_exit(&(cb->mutex));
                dccp_output(cb->pcb, 1);
		return;
        }
	mutex_exit(&(cb->mutex));
}

int
_cwndvector_size(struct tcplike_send_ccb *cb)
{
	u_int64_t gap, offset, seqnr;
	u_int32_t cnt;
	u_char *t;

	TCPLIKE_DEBUG((LOG_INFO, "Enter cwndvector_size\n"));
	cnt = 0;
	for (seqnr = cb->cv_hs; seqnr < cb->cv_ts; seqnr++) {
		gap = seqnr - cb->cv_hs;

		offset = gap % 8;
		t = cb->cv_hp + (gap/8);
		if (t >= (cb->cwndvector + (cb->cv_size/8)))
			t -= (cb->cv_size / 8); /* wrapped */
	
		if (((*t & (0x01 << offset)) >> offset) == 0x01)
			cnt++;
	}
	return cnt;
}

u_char
_cwndvector_state(struct tcplike_send_ccb *cb, u_int64_t seqnr)
{
	u_int64_t gap, offset;
	u_char *t;

	/* Check for wrapping */
	if (seqnr >= cb->cv_hs) {
		/* Not wrapped */
		gap = seqnr - cb->cv_hs;
	} else {
		/* Wrapped XXXXX */
		gap = seqnr + 0x1000000000000LL - cb->cv_hs; /* seq nr = 48 bits */
	}

	if (gap >= cb->cv_size) {
		/* gap is bigger than cwndvector size? baaad */
		return 0x01;
	}

	offset = gap % 8;
	t = cb->cv_hp + (gap/8);
	if (t >= (cb->cwndvector + (cb->cv_size/8)))
		t -= (cb->cv_size / 8); /* wrapped */

	return ((*t & (0x01 << offset)) >> offset);
}

void
_add_to_cwndvector(struct tcplike_send_ccb *cb, u_int64_t seqnr)
{
	u_int64_t offset, dc, gap;
	u_char *t, *n;
	
	TCPLIKE_DEBUG((LOG_INFO, "Entering add_to_cwndvector\n"));
	
	if (cb->cv_hs == cb->cv_ts) {
		/* Empty cwndvector */
		cb->cv_hs = cb->cv_ts = seqnr;
	}

	/* Check for wrapping */
	if (seqnr >= cb->cv_hs) {
		/* Not wrapped */
		gap = seqnr - cb->cv_hs;
	} else {
		/* Wrapped */
		gap = seqnr + 0x1000000000000LL - cb->cv_hs; /* seq nr = 48 bits */
	}

	if (gap >= cb->cv_size) {
		/* gap is bigger than cwndvector size? baaad */
		/* maybe we should increase the cwndvector here */
		CWND_DEBUG((LOG_INFO, "add cwndvector error. gap: %d, cv_size: %d, seqnr: %d\n",
			    gap, cb->cv_size, seqnr));
		dccpstat.tcplikes_send_badseq++;
		return;
	}
	
	offset = gap % 8; /* bit to mark */
	t = cb->cv_hp + (gap/8);
	if (t >= (cb->cwndvector + (cb->cv_size/8)))
		t -= (cb->cv_size / 8); /* cwndvector wrapped */
	
	*t = *t | (0x01 << offset); /* turn on bit */

	cb->cv_ts = seqnr+1;
	if (cb->cv_ts == 0x1000000000000LL)
		cb->cv_ts = 0;

	if (gap > (cb->cv_size - 128)) {
		MALLOC_DEBUG((LOG_INFO, "INCREASE cwndVECTOR\n"));
		n = malloc(cb->cv_size/4, M_PCB, M_NOWAIT); /* old size * 2 */
		if (n == NULL) {
			MALLOC_DEBUG((LOG_INFO, "Increase cwndvector FAILED\n"));
			dccpstat.tcplikes_send_memerr++;
			return;
		}
		memset (n+cb->cv_size/8,0x00,cb->cv_size/8); /* new half all missing */
		dc = (cb->cwndvector + (cb->cv_size/8)) - cb->cv_hp;
		memcpy (n,cb->cv_hp, dc); /* tail to end */
		memcpy (n+dc,cb->cwndvector,cb->cv_hp - cb->cwndvector); /* start to tail */
		cb->cv_size = cb->cv_size * 2; /* counted in items, so it';s a doubling */
		free (cb->cwndvector, M_PCB);
		cb->cv_hp = cb->cwndvector = n;
	}
}

void
_remove_from_cwndvector(struct tcplike_send_ccb *cb, u_int64_t seqnr)
{
	u_int64_t offset;
	int64_t gap;
	u_char *t;
	
	DCCP_DEBUG((LOG_INFO, "Entering remove_from_cwndvector\n"));
	
	if (cb->cv_hs == cb->cv_ts) {
		/* Empty cwndvector */
		return;
	}

	/* Check for wrapping */
	if (seqnr >= cb->cv_hs) {
		/* Not wrapped */
		gap = seqnr - cb->cv_hs;
	} else {
		/* Wrapped */
		gap = seqnr + 0x1000000000000LL - cb->cv_hs; /* seq nr = 48 bits */
	}

	if (gap >= cb->cv_size) {
		/* gap is bigger than cwndvector size. has already been chopped */
		return;
	}
	
	offset = gap % 8; /* hi or low 2 bits to mark */
	t = cb->cv_hp + (gap/8);
	if (t >= (cb->cwndvector + (cb->cv_size/8)))
		t -= (cb->cv_size / 8); /* cwndvector wrapped */
	
	*t = *t & (~(0x01 << offset)); /* turn off bits */
}

int
_chop_cwndvector(struct tcplike_send_ccb *cb, u_int64_t seqnr)
{
	int64_t gap, bytegap;
	u_char *t;

	CWND_DEBUG((LOG_INFO,"Chop cwndvector at: %u\n", seqnr));

	if (cb->cv_hs == cb->cv_ts)
		return 0;
	
	if (seqnr > cb->cv_hs) {
		gap = seqnr - cb->cv_hs;
	} else {
		/* We received obsolete information */
		return 0;
	}

	bytegap = gap/8;
	if (bytegap == 0)
		return 0;
	
	t = cb->cv_hp + bytegap;
	if (t >= (cb->cwndvector + (cb->cv_size/8)))
		t -= (cb->cv_size / 8); /* ackvector wrapped */
	cb->cv_hp = t;
	cb->cv_hs += bytegap*8;
	return 1;
}


/* Receiver side */


/* Functions declared in struct dccp_cc_sw */

/* Initialises the receiver side
 * returns: pointer to a tcplike_recv_ccb struct on success, otherwise 0
 */ 
void *
tcplike_recv_init(struct dccpcb *pcb)
{
	struct tcplike_recv_ccb *ccb;
  
	TCPLIKE_DEBUG((LOG_INFO, "Entering tcplike_recv_init()\n"));
  
	ccb = malloc(sizeof (struct tcplike_recv_ccb), M_PCB, M_NOWAIT | M_ZERO);
	if (ccb == 0) {
		TCPLIKE_DEBUG((LOG_INFO, "Unable to allocate memory for tcplike_recv_ccb!\n"));
		dccpstat.tcplikes_recv_memerr++;
		return 0;
	}

	memset(ccb, 0, sizeof (struct tcplike_recv_ccb));
	
	ccb->pcb = pcb;
	ccb->unacked = 0;
	ccb->pcb->ack_ratio = 2;

	ccb->pcb->remote_ackvector = 1;
	dccp_use_ackvector(ccb->pcb);

	callout_init(&ccb->free_timer, 0);
	
	mutex_init(&(ccb->mutex), MUTEX_DEFAULT, IPL_SOFTNET);
	
	TCPLIKE_DEBUG((LOG_INFO, "TCPlike receiver initialised!\n"));
	dccpstat.tcplikes_recv_conn++;
	return ccb;
}

void tcplike_recv_term(void *ccb)
{
	struct tcplike_recv_ccb *cb = (struct tcplike_recv_ccb *) ccb;
	if (ccb == 0)
		return;
	
	mutex_destroy(&(cb->mutex));
	free(cb, M_PCB);
	TCPLIKE_DEBUG((LOG_INFO, "TCP-like receiver is destroyed\n"));
}

/* Free the receiver side
 * args: ccb - ccb of recevier
 */
void
tcplike_recv_free(void *ccb)
{
	struct ack_list *a;
	struct tcplike_recv_ccb *cb = (struct tcplike_recv_ccb *) ccb;

	LOSS_DEBUG((LOG_INFO, "Entering tcplike_recv_free()\n"));

	if (ccb == 0)
		return;
	
	mutex_enter(&(cb->mutex));

	a = cb->av_list;
	while (a) {
		cb->av_list = a->next;
		free(a, M_TEMP);
		a = cb->av_list;
	}

	cb->pcb->av_size = 0;
	free(cb->pcb->ackvector, M_PCB);
	
	mutex_exit(&(cb->mutex));
	callout_reset(&cb->free_timer, 10 * hz, tcplike_recv_term, (void *)cb);
}

/*
 * Tell TCPlike that a packet has been received
 * args: ccb  -  ccb block for current connection 
 */
void
tcplike_recv_packet_recv(void *ccb, char *options, int optlen)
{
	struct tcplike_recv_ccb *cb = (struct tcplike_recv_ccb *) ccb;
	u_char ackvector[16];
	u_int16_t avsize;
	u_char av_rcv[10];
	
	TCPLIKE_DEBUG((LOG_INFO, "Entering tcplike_recv_packet()\n"));

	mutex_enter(&(cb->mutex));

	if (cb->pcb->type_rcv == DCCP_TYPE_DATA ||
	    cb->pcb->type_rcv == DCCP_TYPE_DATAACK)
		dccpstat.tcplikes_recv_datarecv++;
	
	/* Grab Ack Vector 0 or 1 */
	avsize = dccp_get_option(options, optlen, DCCP_OPT_ACK_VECTOR0, av_rcv,10);
	if (avsize == 0)
		avsize = dccp_get_option(options, optlen, DCCP_OPT_ACK_VECTOR1, av_rcv,10);

	/* We are only interested in acks-on-acks here.
	 * The "real" ack handling is done be the sender */
	if (avsize == 0 && cb->pcb->ack_rcv) {
		u_int64_t ackthru;
		/* We got an Ack without an ackvector.
		 * This would mean it's an ack on an ack.
		 */
		ackthru = _avlist_get(cb, cb->pcb->ack_rcv);
		ACK_DEBUG((LOG_INFO, "GOT Ack without Ackvector; Ackthru: %u\n", ackthru));
		if (ackthru) {
			dccp_update_ackvector(cb->pcb, ackthru);
			dccpstat.tcplikes_recv_ackack++;
		}
	} else if (avsize > 0 && cb->pcb->ack_rcv) {
		/* We received an AckVector */
		u_int32_t acknum, ackthru;
		int i;
		ACK_DEBUG((LOG_INFO, "GOT Ack with Ackvector\n"));
		/* gotta loop through the ackvector */
		acknum = cb->pcb->ack_rcv;
		for (i=0; i<avsize; i++) {
			u_int8_t state, len;
			state = (av_rcv[i] & 0xc0) >> 6;
			len = (av_rcv[i] & 0x2f) + 1;
			if (state != 0) {
				/* Drops in ackvector! Will be noted and taken care of by the sender part */
				ACK_DEBUG((LOG_INFO, "Packets %u - %u are FUCKED\n",acknum-len, acknum));
				continue;
			}
			
			while (len>0) {
				ackthru = _avlist_get(cb, acknum);
				ACK_DEBUG((LOG_INFO, "Ackthru: %u\n", ackthru));
				if (ackthru) {
					dccp_update_ackvector(cb->pcb, ackthru);
					dccpstat.tcplikes_recv_ackack++;
				}
				acknum--;
				len--;
			} 
		}
	}

	ACK_DEBUG((LOG_INFO, "Adding %llu to local ackvector\n", cb->pcb->seq_rcv));
	dccp_increment_ackvector(cb->pcb, cb->pcb->seq_rcv);
	cb->unacked++;

	if (cb->unacked >= cb->pcb->ack_ratio) {
		/* Time to send an Ack */
		
		avsize = dccp_generate_ackvector(cb->pcb, ackvector);
TCPLIKE_DEBUG((LOG_INFO, "recv_packet avsize %d ackvector %d\n", avsize, ackvector));
		cb->unacked = 0;
		if (avsize > 0) {
			dccp_add_option(cb->pcb, DCCP_OPT_ACK_VECTOR0, ackvector, avsize);
			cb->pcb->ack_snd = cb->pcb->seq_rcv;
			_avlist_add(cb, cb->pcb->seq_snd+1, cb->pcb->ack_snd);
			ACK_DEBUG((LOG_INFO, "Recvr: Sending Ack (%llu) w/ Ack Vector\n", cb->pcb->ack_snd));
			dccpstat.tcplikes_recv_acksent++;
			dccp_output(cb->pcb, 1);
		}
	}
	mutex_exit(&(cb->mutex));
}

void
_avlist_add(struct tcplike_recv_ccb *cb, u_int64_t localseq, u_int64_t ackthru)
{
	struct ack_list *a;
	ACK_DEBUG((LOG_INFO,"Adding localseq %u - ackthru %u to avlist\n", localseq, ackthru));
	/*MALLOC_DEBUG((LOG_INFO, "New ack_list, %u\n", sizeof (struct ack_list)));*/
	a = malloc(sizeof(struct ack_list), M_TEMP, M_NOWAIT);
	if (a == NULL) {
		MALLOC_DEBUG((LOG_INFO, "avlist_add: FAILED\n"));
		dccpstat.tcplikes_recv_memerr++;
		return;
	}
	memset(a, 0, sizeof(struct ack_list));
	a->localseq = localseq;
	a->ackthru = ackthru;
	a->next = cb->av_list;
	cb->av_list = a;
}

/*
 * Searches the av_list. if 'localseq' found, drop it from list and return
 * ackthru
 */
u_int64_t
_avlist_get(struct tcplike_recv_ccb *cb, u_int64_t localseq)
{
	struct ack_list *a, *n, *p;
	u_int64_t ackthru;

	ACK_DEBUG((LOG_INFO,"Getting localseq %u from avlist\n", localseq));
	a = cb->av_list;
	p = 0;
	while (a) {
		n = a->next;
		if (a->localseq == localseq) {
			if (p)
				p->next = n;
			else
				cb->av_list = n;
			ackthru = a->ackthru;
			/*MALLOC_DEBUG((LOG_INFO, "Freeing element %u in ack_list\n", a->localseq));*/
			free(a, M_TEMP);
			return ackthru;
		}
		p = a;
		a = n;
	}
	/* Not found. return 0 */
	return 0;
}

/*
int tcplike_option_recv(void);
*/

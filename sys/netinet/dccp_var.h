/*	$KAME: dccp_var.h,v 1.29 2005/11/03 14:59:28 nishida Exp $	*/
/*	$NetBSD: dccp_var.h,v 1.2 2015/05/02 17:18:03 rtr Exp $ */

/*
 * Copyright (c) 2003 Joacim Häggmark, Magnus Erixzon, Nils-Erik Mattsson 
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
 * Id: dccp_var.h,v 1.25 2003/07/31 11:17:15 joahag-9 Exp
 */

#ifndef _NETINET_DCCP_VAR_H_
#define _NETINET_DCCP_VAR_H_

typedef u_int64_t dccp_seq;

#define DSEQ_TO_DHDR(x, y) { \
	(x)->dh_seq  = htons(y >> 32);\
	(x)->dh_seq2 = htonl(y & 4294967295U);\
}

#define DHDR_TO_DSEQ(x, y) { \
	x = ((u_int64_t)ntohs(y->dh_seq) << 32) | ntohl(y->dh_seq2);\
}

#define DSEQ_TO_DAHDR(x, y) { \
	(x).dah_ack  = htons(y >> 32);\
	(x).dah_ack2 = htonl(y & 4294967295U);\
}

#define DAHDR_TO_DSEQ(x, y) { \
	x = ((u_int64_t)ntohs(y.dah_ack) << 32) | ntohl(y.dah_ack2);\
}

#define CONVERT_TO_LONGSEQ(S, ref) \
    ((((~(S- ref.lo) +1) <= 0x7fffff) && (S < ref.lo))?  \
        (((u_int64_t)(ref.hi + 1) << 24) | S) % 281474976710656ll: \
        (((u_int64_t)ref.hi << 24) | S) % 281474976710656ll)

struct ref_seq {
	u_int32_t hi;
	u_int32_t lo;
};

struct dccpcb {
	u_int8_t	state; /* initial, listening, connecting, established,
				  closing, closed etc */
	u_int8_t	who;	/* undef, server, client, listener */

	struct callout	connect_timer;	/* Connection timer */
	struct callout	retrans_timer;	/* Retransmit timer */
	struct callout	close_timer;	/* Closing timer */
	struct callout	timewait_timer;	/* Time wait timer */

	u_int32_t	retrans;

	dccp_seq	seq_snd;
	dccp_seq	ack_snd; /* ack num to send in Ack or DataAck packet */
	dccp_seq	gsn_rcv; /* Greatest received sequence number */

	/* values representing last incoming packet. are set in dccp_input */
	dccp_seq	seq_rcv;	/* Seq num of received packet */
	dccp_seq	ack_rcv;	/* Ack num received in Ack or DataAck packet */
	u_int8_t	type_rcv;	/* Type of packet received */
	u_int32_t	len_rcv;	/* Length of data received */
	u_int8_t	ndp_rcv;	/* ndp value of received packet */

	u_int8_t	cslen;		/* How much of outgoing packets are covered by the checksum */
	u_int8_t	pref_cc;	/* Client prefered CC */
	u_int8_t	ndp;		/* Number of non data packets */
	u_int32_t	loss_window;	/* Loss window (defaults to 1000)  */
	u_int16_t	ack_ratio;	/* Ack Ratio Feature */
	int8_t		cc_in_use[2];	/* Current CC in use
					   (in each direction) */
	void		*cc_state[2];
	struct inpcb	*d_inpcb;	/* Pointer back to Internet PCB	 */
	struct in6pcb	*d_in6pcb;
	u_int32_t	d_maxseg;	/* Maximum segment size */
	char		options[DCCP_MAX_OPTIONS];
	u_int8_t	optlen;
	char		features[DCCP_MAX_OPTIONS];
	u_int8_t	featlen;
	u_int8_t	ccval;		/* ccval */

	u_int32_t	avgpsize;	/* Average packet size */

	/* variables for the local (receiver-side) ack vector */
	u_char *ackvector;  /* For acks, 2 bits per packet */
	u_char *av_hp;	/* head ptr for ackvector */
	u_int16_t av_size;
	dccp_seq av_hs, av_ts; /* highest/lowest seq no in ackvector */
	
	u_int8_t remote_ackvector; /* Is recv side using AckVector? */
	u_char      shortseq; /* use short seq number */
	u_int32_t	scode;    /* service core */
	struct ref_seq	ref_seq;    /* reference sequence number */
	struct ref_seq	ref_pseq;   /* reference peer sequence number */

#ifndef __FreeBSD__
#ifndef INP_IPV6
#define INP_IPV6	0x1
#endif
#ifndef INP_IPV4
#define INP_IPV4	0x2
#endif
	u_int8_t	inp_vflag;
	u_int8_t	inp_ip_ttl;
	u_int8_t	inp_ip_tos;
#endif
	u_int8_t	pktlen[DCCP_MAX_PKTS];
	u_int16_t	pktlenidx;
	u_int16_t	pktcnt;
};

#ifdef _KERNEL
struct inp_dp {
	struct inpcb inp;
	struct dccpcb dp;
};
#endif

#if defined(_NETINET_IN_PCB_H_) && defined(_SYS_SOCKETVAR_H_)
struct xdccpcb {
	size_t		xd_len;
	struct	inpcb	xd_inp;
	struct	dccpcb	xd_dp;
#ifdef __FreeBSD__
	struct	xsocket	xd_socket;
#endif
};
#endif

#define	intodccpcb(ip)	((struct dccpcb *)((ip)->inp_ppcb))
#define	in6todccpcb(ip)	((struct dccpcb *)((ip)->in6p_ppcb))

#ifdef __NetBSD__
#define	dptosocket(dp)	(((dp)->d_inpcb) ? (dp)->d_inpcb->inp_socket : \
			(((dp)->d_in6pcb) ? (dp)->d_in6pcb->in6p_socket : NULL))
#else
#define	dptosocket(dp)	((dp)->d_inpcb->inp_socket)
#endif

struct	dccpstat {
	u_long	dccps_connattempt;	/* Initiated connections */
	u_long	dccps_connects;		/* Established connections */
	u_long	dccps_ipackets;		/* Total input packets */
	u_long	dccps_ibytes;		/* Total input bytes */
	u_long	dccps_drops;		/* Dropped packets  */
	u_long	dccps_badsum;		/* Checksum error */
	u_long	dccps_badlen;		/* Bad length */
	u_long	dccps_badseq;		/* Sequence number not inside loss_window  */
	u_long	dccps_noport;		/* No socket on port */

	/* TCPlike Sender */
	u_long	tcplikes_send_conn;	/* Connections established */
	u_long	tcplikes_send_reploss;	/* Data packets reported lost */
	u_long	tcplikes_send_assloss;	/* Data packets assumed lost */
	u_long	tcplikes_send_ackrecv;	/* Acknowledgement (w/ Ack Vector) packets received */
	u_long	tcplikes_send_missack;	/* Ack packets assumed lost */
	u_long	tcplikes_send_badseq;	/* Bad sequence number on outgoing packet */
	u_long	tcplikes_send_memerr;	/* Memory allocation errors */
	
	/* TCPlike Receiver */
	u_long	tcplikes_recv_conn;	/* Connections established */
	u_long	tcplikes_recv_datarecv; /* Number of data packets received */
	u_long	tcplikes_recv_ackack;	/* Ack-on-acks received */
	u_long	tcplikes_recv_acksent;	/* Acknowledgement (w/ Ack Vector) packets sent */
	u_long	tcplikes_recv_memerr;	/* Memory allocation errors */
	
	/*	Some CCID statistic should also be here */

	u_long	dccps_opackets;		/* Total output packets */
	u_long	dccps_obytes;		/* Total output bytes */

	/* TFRC Sender */
	u_long	tfrcs_send_conn;	/* Connections established */
	u_long	tfrcs_send_nomem;	/* Not enough memory */
	u_long	tfrcs_send_erropt;	/* option error */
	u_long	tfrcs_send_noopt;	/* no option  */
	u_long	tfrcs_send_fbacks; 	/* sent feedbacks */

	/* TFRC Receiver */
	u_long	tfrcs_recv_conn;	/* established connection  */
	u_long	tfrcs_recv_erropt;	/* option error */
	u_long	tfrcs_recv_losts;	/* lost packets */
	u_long	tfrcs_recv_nomem;	/* no memory */
	u_long	tfrcs_recv_noopt;	/* no option */
	u_long	tfrcs_recv_fbacks; 	/* receipt feedbacks */

};

/*
 * Names for DCCP sysctl objects
 */
#define DCCPCTL_LOGINVAIN       	1 
#define DCCPCTL_DOFEATURENEGO       2 

/*
 *	DCCP States
 */

#define DCCPS_CLOSED	0
#define DCCPS_LISTEN	1
#define DCCPS_REQUEST	2
#define DCCPS_RESPOND	3
#define DCCPS_ESTAB	4
#define DCCPS_SERVER_CLOSE	5
#define DCCPS_CLIENT_CLOSE	6
#define DCCPS_TIME_WAIT 7

#define DCCP_NSTATES	8

#ifdef DCCPSTATES
const char *dccpstates[] = {
	"CLOSED",	"LISTEN",	"REQEST",	"RESPOND",
	"ESTABLISHED",	"SERVER-CLOSE",	"CLIENT-CLOSE", "TIME_WAIT",
};
#else
extern const char *dccpstates[];
#endif

#define DCCP_UNDEF	0
#define DCCP_LISTENER	1
#define DCCP_SERVER	2
#define DCCP_CLIENT	3

#define DCCP_SEQ_LT(a, b)	((int)(((a) << 16) - ((b) << 16)) < 0)
#define DCCP_SEQ_GT(a, b)	((int)(((a) << 16) - ((b) << 16)) > 0)

/*
 * Names for DCCP sysctl objects
 */
#define	DCCPCTL_DEFCCID		1	/* Default CCID */
#define DCCPCTL_STATS		2	/* statistics (read-only) */
#define DCCPCTL_PCBLIST		3
#define DCCPCTL_SENDSPACE	4
#define DCCPCTL_RECVSPACE	5

#define DCCPCTL_NAMES { \
	{ 0, 0 }, \
	{ "defccid", CTLTYPE_INT }, \
	{ "stats", CTLTYPE_STRUCT }, \
	{ "sendspace", CTLTYPE_INT }, \
	{ "recvspace", CTLTYPE_INT }, \
}

#ifdef _KERNEL

#ifdef DCCP_DEBUG_ON
#define DCCP_DEBUG(args)	dccp_log args
#else
#define DCCP_DEBUG(args)
#endif

#ifdef ACKDEBUG
#define ACK_DEBUG(args) dccp_log args
#else
#define ACK_DEBUG(args)
#endif

extern const struct	pr_usrreqs dccp_usrreqs;
extern struct	inpcbhead dccpb;
extern struct	inpcbinfo dccpbinfo;
extern u_long	dccp_sendspace;
extern u_long	dccp_recvspace;
extern struct	dccpstat dccpstat; /* dccp statistics */
extern int	dccp_log_in_vain; /* if we should log connections to
				     ports w/o listeners */
extern int	dccp_do_feature_nego; 

extern struct inpcbtable dccpbtable;

/* These four functions are called from inetsw (in_proto.c) */
void	dccp_init(void);
void	dccp_log(int, const char *, ...);
void	dccp_input(struct mbuf *, ...);
void*	dccp_ctlinput(int, const struct sockaddr *, void *);
int	dccp_ctloutput(int , struct socket *, struct sockopt *);
int	dccp_sysctl(int *, u_int, void *, size_t *, void *, size_t);
int	dccp_usrreq(struct socket *, int, struct mbuf *, struct mbuf *, struct mbuf *, struct lwp *); 

void	dccp_notify(struct inpcb *, int);
struct dccpcb *
	dccp_newdccpcb(int, void *);
int	dccp_shutdown(struct socket *);
int	dccp_output(struct dccpcb *, u_int8_t);
int	dccp_doconnect(struct socket *, struct sockaddr *, struct lwp *, int);
int	dccp_add_option(struct dccpcb *, u_int8_t, char *, u_int8_t);
int	dccp_add_feature(struct dccpcb *, u_int8_t, u_int8_t,  char *, u_int8_t);
int	dccp_detach(struct socket *);
int	dccp_attach(struct socket *, int);
int	dccp_abort(struct socket *);
int	dccp_disconnect(struct socket *);
int	dccp_send(struct socket *, struct mbuf *, struct sockaddr *,
		  struct mbuf *, struct lwp *);
void	dccp_retrans_t(void *);
void	dccp_connect_t(void *);

/* No cc functions */
void* dccp_nocc_init(struct dccpcb *);
void  dccp_nocc_free(void *);
int   dccp_nocc_send_packet(void*, long);
void  dccp_nocc_send_packet_sent(void *, int, long);
void  dccp_nocc_packet_recv(void*, char *, int);

#endif

#endif

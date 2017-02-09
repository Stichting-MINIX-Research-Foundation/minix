/*	$NetBSD: if_spppvar.h,v 1.16 2009/10/05 21:27:36 dyoung Exp $	*/

#ifndef _NET_IF_SPPPVAR_H_
#define _NET_IF_SPPPVAR_H_

/*
 * Defines for synchronous PPP/Cisco link level subroutines.
 *
 * Copyright (C) 1994 Cronyx Ltd.
 * Author: Serge Vakulenko, <vak@cronyx.ru>
 *
 * Heavily revamped to conform to RFC 1661.
 * Copyright (C) 1997, Joerg Wunsch.
 *
 * This software is distributed with NO WARRANTIES, not even the implied
 * warranties for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Authors grant any other persons or organizations permission to use
 * or modify this software as long as this message is kept with the software,
 * all derivative works or modified versions.
 *
 * From: Version 2.0, Fri Oct  6 20:39:21 MSK 1995
 *
 * From: if_sppp.h,v 1.8 1997/10/11 11:25:20 joerg Exp
 *
 * From: Id: if_sppp.h,v 1.7 1998/12/01 20:20:19 hm Exp
 */

#define IDX_LCP 0		/* idx into state table */

struct slcp {
	u_long	opts;		/* LCP options to send (bitfield) */
	u_long  magic;          /* local magic number */
	u_long	mru;		/* our max receive unit */
	u_long	their_mru;	/* their max receive unit */
	u_long	protos;		/* bitmask of protos that are started */
	u_char  echoid;         /* id of last keepalive echo request */
	/* restart max values, see RFC 1661 */
	int	timeout;
	int	max_terminate;
	int	max_configure;
	int	max_failure;
};

#define IDX_IPCP 1		/* idx into state table */
#define IDX_IPV6CP 2		/* idx into state table */

struct sipcp {
	u_long	opts;		/* IPCP options to send (bitfield) */
	u_int	flags;
#define IPCP_HISADDR_SEEN 1	/* have seen his address already */
#define IPCP_MYADDR_SEEN  2	/* have a local address assigned already */
#define IPCP_MYADDR_DYN   4	/* my address is dynamically assigned */
#define	IPCP_HISADDR_DYN  8	/* his address is dynamically assigned */
#ifdef notdef
#define IPV6CP_MYIFID_DYN   2	/* my ifid is dynamically assigned */
#endif
#define IPV6CP_MYIFID_SEEN  4	/* have seen his ifid already */
	uint32_t saved_hisaddr;/* if hisaddr (IPv4) is dynamic, save original one here, in network byte order */
	uint32_t req_hisaddr;	/* remote address requested */
	uint32_t req_myaddr;	/* local address requested */
};

struct sauth {
	u_short	proto;			/* authentication protocol to use */
	u_short	flags;
	char	*name;			/* system identification name */
	char	*secret;		/* secret password */
	u_char	name_len;		/* no need to have a bigger size */
	u_char	secret_len;		/* because proto gives size in a byte */
	char	challenge[16];		/* random challenge [don't change size! it's really hardcoded!] */
};

#define IDX_PAP		3
#define IDX_CHAP	4

#define IDX_COUNT (IDX_CHAP + 1) /* bump this when adding cp's! */

struct sppp {
	/* NB: pp_if _must_ be first */
	struct  ifnet pp_if;    /* network interface data */
	struct  ifqueue pp_fastq; /* fast output queue */
	struct	ifqueue pp_cpq;	/* PPP control protocol queue */
	struct  sppp *pp_next;  /* next interface in keepalive list */
	u_int   pp_flags;       /* use Cisco protocol instead of PPP */
	u_int	pp_framebytes;	/* number of bytes added by (hardware) framing */
	u_int   pp_alivecnt;    /* keepalive packets counter */
	u_int   pp_loopcnt;     /* loopback detection counter */
	u_int	pp_maxalive;	/* number or echo req. w/o reply */
	u_long  pp_seq[IDX_COUNT];	/* local sequence number */
	u_long  pp_rseq[IDX_COUNT];	/* remote sequence number */
	uint64_t	pp_saved_mtu;	/* saved MTU value */
	time_t	pp_last_receive;	/* peer's last "sign of life" */
	time_t	pp_max_noreceive;	/* seconds since last receive before
					   we start to worry and send echo
					   requests */
	time_t	pp_last_activity;	/* second of last payload data s/r */
	time_t	pp_idle_timeout;	/* idle seconds before auto-disconnect,
					 * 0 = disabled */
	int	pp_auth_failures;	/* authorization failures */
	int	pp_max_auth_fail;	/* max. allowed authorization failures */
	int	pp_phase;	/* phase we're currently in */
	int	query_dns;	/* 1 if we want to know the dns addresses */
	uint32_t	dns_addrs[2];
	int	state[IDX_COUNT];	/* state machine */
	u_char  confid[IDX_COUNT];	/* id of last configuration request */
	int	rst_counter[IDX_COUNT];	/* restart counter */
	int	fail_counter[IDX_COUNT]; /* negotiation failure counter */
#if defined(__NetBSD__)
	struct	callout ch[IDX_COUNT];	/* per-proto and if callouts */
	struct	callout pap_my_to_ch;	/* PAP needs one more... */
#endif
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	struct callout_handle ch[IDX_COUNT]; /* per-proto and if callouts */
	struct callout_handle pap_my_to_ch; /* PAP needs one more... */
#endif
	struct slcp lcp;		/* LCP params */
	struct sipcp ipcp;		/* IPCP params */
	struct sipcp ipv6cp;		/* IPv6CP params */
	struct sauth myauth;		/* auth params, i'm peer */
	struct sauth hisauth;		/* auth params, i'm authenticator */
	/*
	 * These functions are filled in by sppp_attach(), and are
	 * expected to be used by the lower layer (hardware) drivers
	 * in order to communicate the (un)availability of the
	 * communication link.  Lower layer drivers that are always
	 * ready to communicate (like hardware HDLC) can shortcut
	 * pp_up from pp_tls, and pp_down from pp_tlf.
	 */
	void	(*pp_up)(struct sppp *);
	void	(*pp_down)(struct sppp *);
	/*
	 * These functions need to be filled in by the lower layer
	 * (hardware) drivers if they request notification from the
	 * PPP layer whether the link is actually required.  They
	 * correspond to the tls and tlf actions.
	 */
	void	(*pp_tls)(struct sppp *);
	void	(*pp_tlf)(struct sppp *);
	/*
	 * These (optional) functions may be filled by the hardware
	 * driver if any notification of established connections
	 * (currently: IPCP up) is desired (pp_con) or any internal
	 * state change of the interface state machine should be
	 * signaled for monitoring purposes (pp_chg).
	 */
	void	(*pp_con)(struct sppp *);
	void	(*pp_chg)(struct sppp *, int);
};

#define PP_KEEPALIVE    0x01    /* use keepalive protocol */
#define PP_CISCO        0x02    /* use Cisco protocol instead of PPP */
				/* 0x04 was PP_TIMO */
#define PP_CALLIN	0x08	/* we are being called */
#define PP_NEEDAUTH	0x10	/* remote requested authentication */
#define	PP_NOFRAMING	0x20	/* do not add/expect encapsulation
				   around PPP frames (i.e. the serial
				   HDLC like encapsulation, RFC1662) */


#define PP_MTU          1500    /* default/minimal MRU */
#define PP_MAX_MRU	2048	/* maximal MRU we want to negotiate */

#ifdef _KERNEL
void sppp_attach (struct ifnet *);
void sppp_detach (struct ifnet *);
void sppp_input (struct ifnet *, struct mbuf *);
int sppp_ioctl(struct ifnet *, u_long, void *);
struct mbuf *sppp_dequeue (struct ifnet *);
int sppp_isempty (struct ifnet *);
void sppp_flush (struct ifnet *);
#endif
#endif /* !_NET_IF_SPPPVAR_H_ */

/*	$NetBSD: if_spppsubr.c,v 1.135 2015/08/20 14:40:19 christos Exp $	 */

/*
 * Synchronous PPP/Cisco link level subroutines.
 * Keepalive protocol implemented in both Cisco and PPP modes.
 *
 * Copyright (C) 1994-1996 Cronyx Engineering Ltd.
 * Author: Serge Vakulenko, <vak@cronyx.ru>
 *
 * Heavily revamped to conform to RFC 1661.
 * Copyright (C) 1997, Joerg Wunsch.
 *
 * RFC2472 IPv6CP support.
 * Copyright (C) 2000, Jun-ichiro itojun Hagino <itojun@iijlab.net>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE FREEBSD PROJECT ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE FREEBSD PROJECT OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * From: Version 2.4, Thu Apr 30 17:17:21 MSD 1997
 *
 * From: if_spppsubr.c,v 1.39 1998/04/04 13:26:03 phk Exp
 *
 * From: Id: if_spppsubr.c,v 1.23 1999/02/23 14:47:50 hm Exp
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_spppsubr.c,v 1.135 2015/08/20 14:40:19 christos Exp $");

#if defined(_KERNEL_OPT)
#include "opt_inet.h"
#include "opt_modular.h"
#include "opt_compat_netbsd.h"
#endif


#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/callout.h>
#include <sys/md5.h>
#include <sys/inttypes.h>
#include <sys/kauth.h>
#include <sys/cprng.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/ppp_defs.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#ifdef INET
#include <netinet/ip.h>
#include <netinet/tcp.h>
#endif
#include <net/ethertypes.h>

#ifdef INET6
#include <netinet6/scope6_var.h>
#endif

#include <net/if_sppp.h>
#include <net/if_spppvar.h>

#define	LCP_KEEPALIVE_INTERVAL		10	/* seconds between checks */
#define LOOPALIVECNT     		3	/* loopback detection tries */
#define DEFAULT_MAXALIVECNT    		3	/* max. missed alive packets */
#define	DEFAULT_NORECV_TIME		15	/* before we get worried */
#define DEFAULT_MAX_AUTH_FAILURES	5	/* max. auth. failures */

/*
 * Interface flags that can be set in an ifconfig command.
 *
 * Setting link0 will make the link passive, i.e. it will be marked
 * as being administrative openable, but won't be opened to begin
 * with.  Incoming calls will be answered, or subsequent calls with
 * -link1 will cause the administrative open of the LCP layer.
 *
 * Setting link1 will cause the link to auto-dial only as packets
 * arrive to be sent.
 *
 * Setting IFF_DEBUG will syslog the option negotiation and state
 * transitions at level kern.debug.  Note: all logs consistently look
 * like
 *
 *   <if-name><unit>: <proto-name> <additional info...>
 *
 * with <if-name><unit> being something like "bppp0", and <proto-name>
 * being one of "lcp", "ipcp", "cisco", "chap", "pap", etc.
 */

#define IFF_PASSIVE	IFF_LINK0	/* wait passively for connection */
#define IFF_AUTO	IFF_LINK1	/* auto-dial on output */

#define CONF_REQ	1		/* PPP configure request */
#define CONF_ACK	2		/* PPP configure acknowledge */
#define CONF_NAK	3		/* PPP configure negative ack */
#define CONF_REJ	4		/* PPP configure reject */
#define TERM_REQ	5		/* PPP terminate request */
#define TERM_ACK	6		/* PPP terminate acknowledge */
#define CODE_REJ	7		/* PPP code reject */
#define PROTO_REJ	8		/* PPP protocol reject */
#define ECHO_REQ	9		/* PPP echo request */
#define ECHO_REPLY	10		/* PPP echo reply */
#define DISC_REQ	11		/* PPP discard request */

#define LCP_OPT_MRU		1	/* maximum receive unit */
#define LCP_OPT_ASYNC_MAP	2	/* async control character map */
#define LCP_OPT_AUTH_PROTO	3	/* authentication protocol */
#define LCP_OPT_QUAL_PROTO	4	/* quality protocol */
#define LCP_OPT_MAGIC		5	/* magic number */
#define LCP_OPT_RESERVED	6	/* reserved */
#define LCP_OPT_PROTO_COMP	7	/* protocol field compression */
#define LCP_OPT_ADDR_COMP	8	/* address/control field compression */

#define IPCP_OPT_ADDRESSES	1	/* both IP addresses; deprecated */
#define IPCP_OPT_COMPRESSION	2	/* IP compression protocol */
#define IPCP_OPT_ADDRESS	3	/* local IP address */
#define	IPCP_OPT_PRIMDNS	129	/* primary remote dns address */
#define	IPCP_OPT_SECDNS		131	/* secondary remote dns address */

#define IPV6CP_OPT_IFID		1	/* interface identifier */
#define IPV6CP_OPT_COMPRESSION	2	/* IPv6 compression protocol */

#define PAP_REQ			1	/* PAP name/password request */
#define PAP_ACK			2	/* PAP acknowledge */
#define PAP_NAK			3	/* PAP fail */

#define CHAP_CHALLENGE		1	/* CHAP challenge request */
#define CHAP_RESPONSE		2	/* CHAP challenge response */
#define CHAP_SUCCESS		3	/* CHAP response ok */
#define CHAP_FAILURE		4	/* CHAP response failed */

#define CHAP_MD5		5	/* hash algorithm - MD5 */

#define CISCO_MULTICAST		0x8f	/* Cisco multicast address */
#define CISCO_UNICAST		0x0f	/* Cisco unicast address */
#define CISCO_KEEPALIVE		0x8035	/* Cisco keepalive protocol */
#define CISCO_ADDR_REQ		0	/* Cisco address request */
#define CISCO_ADDR_REPLY	1	/* Cisco address reply */
#define CISCO_KEEPALIVE_REQ	2	/* Cisco keepalive request */

/* states are named and numbered according to RFC 1661 */
#define STATE_INITIAL	0
#define STATE_STARTING	1
#define STATE_CLOSED	2
#define STATE_STOPPED	3
#define STATE_CLOSING	4
#define STATE_STOPPING	5
#define STATE_REQ_SENT	6
#define STATE_ACK_RCVD	7
#define STATE_ACK_SENT	8
#define STATE_OPENED	9

struct ppp_header {
	uint8_t address;
	uint8_t control;
	uint16_t protocol;
} __packed;
#define PPP_HEADER_LEN          sizeof (struct ppp_header)

struct lcp_header {
	uint8_t type;
	uint8_t ident;
	uint16_t len;
} __packed;
#define LCP_HEADER_LEN          sizeof (struct lcp_header)

struct cisco_packet {
	uint32_t type;
	uint32_t par1;
	uint32_t par2;
	uint16_t rel;
	uint16_t time0;
	uint16_t time1;
} __packed;
#define CISCO_PACKET_LEN 18

/*
 * We follow the spelling and capitalization of RFC 1661 here, to make
 * it easier comparing with the standard.  Please refer to this RFC in
 * case you can't make sense out of these abbreviation; it will also
 * explain the semantics related to the various events and actions.
 */
struct cp {
	u_short	proto;		/* PPP control protocol number */
	u_char protoidx;	/* index into state table in struct sppp */
	u_char flags;
#define CP_LCP		0x01	/* this is the LCP */
#define CP_AUTH		0x02	/* this is an authentication protocol */
#define CP_NCP		0x04	/* this is a NCP */
#define CP_QUAL		0x08	/* this is a quality reporting protocol */
	const char *name;	/* name of this control protocol */
	/* event handlers */
	void	(*Up)(struct sppp *sp);
	void	(*Down)(struct sppp *sp);
	void	(*Open)(struct sppp *sp);
	void	(*Close)(struct sppp *sp);
	void	(*TO)(void *sp);
	int	(*RCR)(struct sppp *sp, struct lcp_header *h, int len);
	void	(*RCN_rej)(struct sppp *sp, struct lcp_header *h, int len);
	void	(*RCN_nak)(struct sppp *sp, struct lcp_header *h, int len);
	/* actions */
	void	(*tlu)(struct sppp *sp);
	void	(*tld)(struct sppp *sp);
	void	(*tls)(struct sppp *sp);
	void	(*tlf)(struct sppp *sp);
	void	(*scr)(struct sppp *sp);
};

static struct sppp *spppq;
static callout_t keepalive_ch;

#ifdef INET
/*
 * The following disgusting hack gets around the problem that IP TOS
 * can't be set yet.  We want to put "interactive" traffic on a high
 * priority queue.  To decide if traffic is interactive, we check that
 * a) it is TCP and b) one of its ports is telnet, rlogin or ftp control.
 *
 * XXX is this really still necessary?  - joerg -
 */
static u_short interactive_ports[8] = {
	0,	513,	0,	0,
	0,	21,	0,	23,
};
#define INTERACTIVE(p)	(interactive_ports[(p) & 7] == (p))
#endif

/* almost every function needs these */
#define STDDCL							\
	struct ifnet *ifp = &sp->pp_if;				\
	int debug = ifp->if_flags & IFF_DEBUG

static int sppp_output(struct ifnet *ifp, struct mbuf *m,
		       const struct sockaddr *dst, struct rtentry *rt);

static void sppp_cisco_send(struct sppp *sp, int type, int32_t par1, int32_t par2);
static void sppp_cisco_input(struct sppp *sp, struct mbuf *m);

static void sppp_cp_input(const struct cp *cp, struct sppp *sp,
			  struct mbuf *m);
static void sppp_cp_send(struct sppp *sp, u_short proto, u_char type,
			 u_char ident, u_short len, void *data);
/* static void sppp_cp_timeout(void *arg); */
static void sppp_cp_change_state(const struct cp *cp, struct sppp *sp,
				 int newstate);
static void sppp_auth_send(const struct cp *cp,
			   struct sppp *sp, unsigned int type, unsigned int id,
			   ...);

static void sppp_up_event(const struct cp *cp, struct sppp *sp);
static void sppp_down_event(const struct cp *cp, struct sppp *sp);
static void sppp_open_event(const struct cp *cp, struct sppp *sp);
static void sppp_close_event(const struct cp *cp, struct sppp *sp);
static void sppp_to_event(const struct cp *cp, struct sppp *sp);

static void sppp_null(struct sppp *sp);

static void sppp_lcp_init(struct sppp *sp);
static void sppp_lcp_up(struct sppp *sp);
static void sppp_lcp_down(struct sppp *sp);
static void sppp_lcp_open(struct sppp *sp);
static void sppp_lcp_close(struct sppp *sp);
static void sppp_lcp_TO(void *sp);
static int sppp_lcp_RCR(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_lcp_RCN_rej(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_lcp_RCN_nak(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_lcp_tlu(struct sppp *sp);
static void sppp_lcp_tld(struct sppp *sp);
static void sppp_lcp_tls(struct sppp *sp);
static void sppp_lcp_tlf(struct sppp *sp);
static void sppp_lcp_scr(struct sppp *sp);
static void sppp_lcp_check_and_close(struct sppp *sp);
static int sppp_ncp_check(struct sppp *sp);

static void sppp_ipcp_init(struct sppp *sp);
static void sppp_ipcp_up(struct sppp *sp);
static void sppp_ipcp_down(struct sppp *sp);
static void sppp_ipcp_open(struct sppp *sp);
static void sppp_ipcp_close(struct sppp *sp);
static void sppp_ipcp_TO(void *sp);
static int sppp_ipcp_RCR(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_ipcp_RCN_rej(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_ipcp_RCN_nak(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_ipcp_tlu(struct sppp *sp);
static void sppp_ipcp_tld(struct sppp *sp);
static void sppp_ipcp_tls(struct sppp *sp);
static void sppp_ipcp_tlf(struct sppp *sp);
static void sppp_ipcp_scr(struct sppp *sp);

static void sppp_ipv6cp_init(struct sppp *sp);
static void sppp_ipv6cp_up(struct sppp *sp);
static void sppp_ipv6cp_down(struct sppp *sp);
static void sppp_ipv6cp_open(struct sppp *sp);
static void sppp_ipv6cp_close(struct sppp *sp);
static void sppp_ipv6cp_TO(void *sp);
static int sppp_ipv6cp_RCR(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_ipv6cp_RCN_rej(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_ipv6cp_RCN_nak(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_ipv6cp_tlu(struct sppp *sp);
static void sppp_ipv6cp_tld(struct sppp *sp);
static void sppp_ipv6cp_tls(struct sppp *sp);
static void sppp_ipv6cp_tlf(struct sppp *sp);
static void sppp_ipv6cp_scr(struct sppp *sp);

static void sppp_pap_input(struct sppp *sp, struct mbuf *m);
static void sppp_pap_init(struct sppp *sp);
static void sppp_pap_open(struct sppp *sp);
static void sppp_pap_close(struct sppp *sp);
static void sppp_pap_TO(void *sp);
static void sppp_pap_my_TO(void *sp);
static void sppp_pap_tlu(struct sppp *sp);
static void sppp_pap_tld(struct sppp *sp);
static void sppp_pap_scr(struct sppp *sp);

static void sppp_chap_input(struct sppp *sp, struct mbuf *m);
static void sppp_chap_init(struct sppp *sp);
static void sppp_chap_open(struct sppp *sp);
static void sppp_chap_close(struct sppp *sp);
static void sppp_chap_TO(void *sp);
static void sppp_chap_tlu(struct sppp *sp);
static void sppp_chap_tld(struct sppp *sp);
static void sppp_chap_scr(struct sppp *sp);

static const char *sppp_auth_type_name(u_short proto, u_char type);
static const char *sppp_cp_type_name(u_char type);
static const char *sppp_dotted_quad(uint32_t addr);
static const char *sppp_ipcp_opt_name(u_char opt);
#ifdef INET6
static const char *sppp_ipv6cp_opt_name(u_char opt);
#endif
static const char *sppp_lcp_opt_name(u_char opt);
static const char *sppp_phase_name(int phase);
static const char *sppp_proto_name(u_short proto);
static const char *sppp_state_name(int state);
static int sppp_params(struct sppp *sp, u_long cmd, void *data);
#ifdef INET
static void sppp_get_ip_addrs(struct sppp *sp, uint32_t *src, uint32_t *dst,
			      uint32_t *srcmask);
static void sppp_set_ip_addrs(struct sppp *sp, uint32_t myaddr, uint32_t hisaddr);
static void sppp_clear_ip_addrs(struct sppp *sp);
#endif
static void sppp_keepalive(void *dummy);
static void sppp_phase_network(struct sppp *sp);
static void sppp_print_bytes(const u_char *p, u_short len);
static void sppp_print_string(const char *p, u_short len);
#ifdef INET6
static void sppp_get_ip6_addrs(struct sppp *sp, struct in6_addr *src,
				struct in6_addr *dst, struct in6_addr *srcmask);
#ifdef IPV6CP_MYIFID_DYN
static void sppp_set_ip6_addr(struct sppp *sp, const struct in6_addr *src);
static void sppp_gen_ip6_addr(struct sppp *sp, const struct in6_addr *src);
#endif
static void sppp_suggest_ip6_addr(struct sppp *sp, struct in6_addr *src);
#endif

/* our control protocol descriptors */
static const struct cp lcp = {
	PPP_LCP, IDX_LCP, CP_LCP, "lcp",
	sppp_lcp_up, sppp_lcp_down, sppp_lcp_open, sppp_lcp_close,
	sppp_lcp_TO, sppp_lcp_RCR, sppp_lcp_RCN_rej, sppp_lcp_RCN_nak,
	sppp_lcp_tlu, sppp_lcp_tld, sppp_lcp_tls, sppp_lcp_tlf,
	sppp_lcp_scr
};

static const struct cp ipcp = {
	PPP_IPCP, IDX_IPCP,
#ifdef INET
	CP_NCP,	/*don't run IPCP if there's no IPv4 support*/
#else
	0,
#endif
	"ipcp",
	sppp_ipcp_up, sppp_ipcp_down, sppp_ipcp_open, sppp_ipcp_close,
	sppp_ipcp_TO, sppp_ipcp_RCR, sppp_ipcp_RCN_rej, sppp_ipcp_RCN_nak,
	sppp_ipcp_tlu, sppp_ipcp_tld, sppp_ipcp_tls, sppp_ipcp_tlf,
	sppp_ipcp_scr
};

static const struct cp ipv6cp = {
	PPP_IPV6CP, IDX_IPV6CP,
#ifdef INET6	/*don't run IPv6CP if there's no IPv6 support*/
	CP_NCP,
#else
	0,
#endif
	"ipv6cp",
	sppp_ipv6cp_up, sppp_ipv6cp_down, sppp_ipv6cp_open, sppp_ipv6cp_close,
	sppp_ipv6cp_TO, sppp_ipv6cp_RCR, sppp_ipv6cp_RCN_rej, sppp_ipv6cp_RCN_nak,
	sppp_ipv6cp_tlu, sppp_ipv6cp_tld, sppp_ipv6cp_tls, sppp_ipv6cp_tlf,
	sppp_ipv6cp_scr
};

static const struct cp pap = {
	PPP_PAP, IDX_PAP, CP_AUTH, "pap",
	sppp_null, sppp_null, sppp_pap_open, sppp_pap_close,
	sppp_pap_TO, 0, 0, 0,
	sppp_pap_tlu, sppp_pap_tld, sppp_null, sppp_null,
	sppp_pap_scr
};

static const struct cp chap = {
	PPP_CHAP, IDX_CHAP, CP_AUTH, "chap",
	sppp_null, sppp_null, sppp_chap_open, sppp_chap_close,
	sppp_chap_TO, 0, 0, 0,
	sppp_chap_tlu, sppp_chap_tld, sppp_null, sppp_null,
	sppp_chap_scr
};

static const struct cp *cps[IDX_COUNT] = {
	&lcp,			/* IDX_LCP */
	&ipcp,			/* IDX_IPCP */
	&ipv6cp,		/* IDX_IPV6CP */
	&pap,			/* IDX_PAP */
	&chap,			/* IDX_CHAP */
};


/*
 * Exported functions, comprising our interface to the lower layer.
 */

/*
 * Process the received packet.
 */
void
sppp_input(struct ifnet *ifp, struct mbuf *m)
{
	struct ppp_header *h = NULL;
	pktqueue_t *pktq = NULL;
	struct ifqueue *inq = NULL;
	uint16_t protocol;
	int s;
	struct sppp *sp = (struct sppp *)ifp;
	int debug = ifp->if_flags & IFF_DEBUG;
	int isr = 0;

	if (ifp->if_flags & IFF_UP) {
		/* Count received bytes, add hardware framing */
		ifp->if_ibytes += m->m_pkthdr.len + sp->pp_framebytes;
		/* Note time of last receive */
		sp->pp_last_receive = time_uptime;
	}

	if (m->m_pkthdr.len <= PPP_HEADER_LEN) {
		/* Too small packet, drop it. */
		if (debug)
			log(LOG_DEBUG,
			    "%s: input packet is too small, %d bytes\n",
			    ifp->if_xname, m->m_pkthdr.len);
	  drop:
		++ifp->if_ierrors;
		++ifp->if_iqdrops;
		m_freem(m);
		return;
	}

	if (sp->pp_flags & PP_NOFRAMING) {
		memcpy(&protocol, mtod(m, void *), 2);
		protocol = ntohs(protocol);
		m_adj(m, 2);
	} else {

		/* Get PPP header. */
		h = mtod(m, struct ppp_header *);
		m_adj(m, PPP_HEADER_LEN);

		switch (h->address) {
		case PPP_ALLSTATIONS:
			if (h->control != PPP_UI)
				goto invalid;
			if (sp->pp_flags & PP_CISCO) {
				if (debug)
					log(LOG_DEBUG,
					    "%s: PPP packet in Cisco mode "
					    "<addr=0x%x ctrl=0x%x proto=0x%x>\n",
					    ifp->if_xname,
					    h->address, h->control, ntohs(h->protocol));
				goto drop;
			}
			break;
		case CISCO_MULTICAST:
		case CISCO_UNICAST:
			/* Don't check the control field here (RFC 1547). */
			if (! (sp->pp_flags & PP_CISCO)) {
				if (debug)
					log(LOG_DEBUG,
					    "%s: Cisco packet in PPP mode "
					    "<addr=0x%x ctrl=0x%x proto=0x%x>\n",
					    ifp->if_xname,
					    h->address, h->control, ntohs(h->protocol));
				goto drop;
			}
			switch (ntohs(h->protocol)) {
			default:
				++ifp->if_noproto;
				goto invalid;
			case CISCO_KEEPALIVE:
				sppp_cisco_input((struct sppp *) ifp, m);
				m_freem(m);
				return;
#ifdef INET
			case ETHERTYPE_IP:
				pktq = ip_pktq;
				break;
#endif
#ifdef INET6
			case ETHERTYPE_IPV6:
				pktq = ip6_pktq;
				break;
#endif
			}
			goto queue_pkt;
		default:        /* Invalid PPP packet. */
		  invalid:
			if (debug)
				log(LOG_DEBUG,
				    "%s: invalid input packet "
				    "<addr=0x%x ctrl=0x%x proto=0x%x>\n",
				    ifp->if_xname,
				    h->address, h->control, ntohs(h->protocol));
			goto drop;
		}
		protocol = ntohs(h->protocol);
	}

	switch (protocol) {
	default:
		if (sp->state[IDX_LCP] == STATE_OPENED) {
			uint16_t prot = htons(protocol);
			sppp_cp_send(sp, PPP_LCP, PROTO_REJ,
			    ++sp->pp_seq[IDX_LCP], m->m_pkthdr.len + 2,
			    &prot);
		}
		if (debug)
			log(LOG_DEBUG,
			    "%s: invalid input protocol "
			    "<proto=0x%x>\n", ifp->if_xname, ntohs(protocol));
		++ifp->if_noproto;
		goto drop;
	case PPP_LCP:
		sppp_cp_input(&lcp, sp, m);
		m_freem(m);
		return;
	case PPP_PAP:
		if (sp->pp_phase >= SPPP_PHASE_AUTHENTICATE)
			sppp_pap_input(sp, m);
		m_freem(m);
		return;
	case PPP_CHAP:
		if (sp->pp_phase >= SPPP_PHASE_AUTHENTICATE)
			sppp_chap_input(sp, m);
		m_freem(m);
		return;
#ifdef INET
	case PPP_IPCP:
		if (sp->pp_phase == SPPP_PHASE_NETWORK)
			sppp_cp_input(&ipcp, sp, m);
		m_freem(m);
		return;
	case PPP_IP:
		if (sp->state[IDX_IPCP] == STATE_OPENED) {
			sp->pp_last_activity = time_uptime;
			pktq = ip_pktq;
		}
		break;
#endif
#ifdef INET6
	case PPP_IPV6CP:
		if (sp->pp_phase == SPPP_PHASE_NETWORK)
			sppp_cp_input(&ipv6cp, sp, m);
		m_freem(m);
		return;

	case PPP_IPV6:
		if (sp->state[IDX_IPV6CP] == STATE_OPENED) {
			sp->pp_last_activity = time_uptime;
			pktq = ip6_pktq;
		}
		break;
#endif
	}

queue_pkt:
	if ((ifp->if_flags & IFF_UP) == 0 || (!inq && !pktq)) {
		goto drop;
	}

	/* Check queue. */
	if (__predict_true(pktq)) {
		if (__predict_false(!pktq_enqueue(pktq, m, 0))) {
			goto drop;
		}
		return;
	}

	s = splnet();
	if (IF_QFULL(inq)) {
		/* Queue overflow. */
		IF_DROP(inq);
		splx(s);
		if (debug)
			log(LOG_DEBUG, "%s: protocol queue overflow\n",
				ifp->if_xname);
		goto drop;
	}
	IF_ENQUEUE(inq, m);
	schednetisr(isr);
	splx(s);
}

/*
 * Enqueue transmit packet.
 */
static int
sppp_output(struct ifnet *ifp, struct mbuf *m,
    const struct sockaddr *dst, struct rtentry *rt)
{
	struct sppp *sp = (struct sppp *) ifp;
	struct ppp_header *h = NULL;
	struct ifqueue *ifq = NULL;		/* XXX */
	int s, error = 0;
	uint16_t protocol;
	ALTQ_DECL(struct altq_pktattr pktattr;)

	s = splnet();

	sp->pp_last_activity = time_uptime;

	if ((ifp->if_flags & IFF_UP) == 0 ||
	    (ifp->if_flags & (IFF_RUNNING | IFF_AUTO)) == 0) {
		m_freem(m);
		splx(s);
		return (ENETDOWN);
	}

	if ((ifp->if_flags & (IFF_RUNNING | IFF_AUTO)) == IFF_AUTO) {
		/*
		 * Interface is not yet running, but auto-dial.  Need
		 * to start LCP for it.
		 */
		ifp->if_flags |= IFF_RUNNING;
		splx(s);
		lcp.Open(sp);
		s = splnet();
	}

	/*
	 * If the queueing discipline needs packet classification,
	 * do it before prepending link headers.
	 */
	IFQ_CLASSIFY(&ifp->if_snd, m, dst->sa_family, &pktattr);

#ifdef INET
	if (dst->sa_family == AF_INET) {
		struct ip *ip = NULL;
		struct tcphdr *th = NULL;

		if (m->m_len >= sizeof(struct ip)) {
			ip = mtod(m, struct ip *);
			if (ip->ip_p == IPPROTO_TCP &&
			    m->m_len >= sizeof(struct ip) + (ip->ip_hl << 2) +
			    sizeof(struct tcphdr)) {
				th = (struct tcphdr *)
				    ((char *)ip + (ip->ip_hl << 2));
			}
		} else
			ip = NULL;

		/*
		 * When using dynamic local IP address assignment by using
		 * 0.0.0.0 as a local address, the first TCP session will
		 * not connect because the local TCP checksum is computed
		 * using 0.0.0.0 which will later become our real IP address
		 * so the TCP checksum computed at the remote end will
		 * become invalid. So we
		 * - don't let packets with src ip addr 0 thru
		 * - we flag TCP packets with src ip 0 as an error
		 */
		if (ip && ip->ip_src.s_addr == INADDR_ANY) {
			uint8_t proto = ip->ip_p;

			m_freem(m);
			splx(s);
			if (proto == IPPROTO_TCP)
				return (EADDRNOTAVAIL);
			else
				return (0);
		}

		/*
		 * Put low delay, telnet, rlogin and ftp control packets
		 * in front of the queue.
		 */

		if (!IF_QFULL(&sp->pp_fastq) &&
		    ((ip && (ip->ip_tos & IPTOS_LOWDELAY)) ||
		     (th && (INTERACTIVE(ntohs(th->th_sport)) ||
		      INTERACTIVE(ntohs(th->th_dport))))))
			ifq = &sp->pp_fastq;
	}
#endif

#ifdef INET6
	if (dst->sa_family == AF_INET6) {
		/* XXX do something tricky here? */
	}
#endif

	if ((sp->pp_flags & PP_NOFRAMING) == 0) {
		/*
		 * Prepend general data packet PPP header. For now, IP only.
		 */
		M_PREPEND(m, PPP_HEADER_LEN, M_DONTWAIT);
		if (! m) {
			if (ifp->if_flags & IFF_DEBUG)
				log(LOG_DEBUG, "%s: no memory for transmit header\n",
					ifp->if_xname);
			++ifp->if_oerrors;
			splx(s);
			return (ENOBUFS);
		}
		/*
		 * May want to check size of packet
		 * (albeit due to the implementation it's always enough)
		 */
		h = mtod(m, struct ppp_header *);
		if (sp->pp_flags & PP_CISCO) {
			h->address = CISCO_UNICAST;        /* unicast address */
			h->control = 0;
		} else {
			h->address = PPP_ALLSTATIONS;        /* broadcast address */
			h->control = PPP_UI;                 /* Unnumbered Info */
		}
	}

	switch (dst->sa_family) {
#ifdef INET
	case AF_INET:   /* Internet Protocol */
		if (sp->pp_flags & PP_CISCO)
			protocol = htons(ETHERTYPE_IP);
		else {
			/*
			 * Don't choke with an ENETDOWN early.  It's
			 * possible that we just started dialing out,
			 * so don't drop the packet immediately.  If
			 * we notice that we run out of buffer space
			 * below, we will however remember that we are
			 * not ready to carry IP packets, and return
			 * ENETDOWN, as opposed to ENOBUFS.
			 */
			protocol = htons(PPP_IP);
			if (sp->state[IDX_IPCP] != STATE_OPENED)
				error = ENETDOWN;
		}
		break;
#endif
#ifdef INET6
	case AF_INET6:   /* Internet Protocol version 6 */
		if (sp->pp_flags & PP_CISCO)
			protocol = htons(ETHERTYPE_IPV6);
		else {
			/*
			 * Don't choke with an ENETDOWN early.  It's
			 * possible that we just started dialing out,
			 * so don't drop the packet immediately.  If
			 * we notice that we run out of buffer space
			 * below, we will however remember that we are
			 * not ready to carry IP packets, and return
			 * ENETDOWN, as opposed to ENOBUFS.
			 */
			protocol = htons(PPP_IPV6);
			if (sp->state[IDX_IPV6CP] != STATE_OPENED)
				error = ENETDOWN;
		}
		break;
#endif
	default:
		m_freem(m);
		++ifp->if_oerrors;
		splx(s);
		return (EAFNOSUPPORT);
	}

	if (sp->pp_flags & PP_NOFRAMING) {
		M_PREPEND(m, 2, M_DONTWAIT);
		if (m == NULL) {
			if (ifp->if_flags & IFF_DEBUG)
				log(LOG_DEBUG, "%s: no memory for transmit header\n",
					ifp->if_xname);
			++ifp->if_oerrors;
			splx(s);
			return (ENOBUFS);
		}
		*mtod(m, uint16_t *) = protocol;
	} else {
		h->protocol = protocol;
	}


	error = ifq_enqueue2(ifp, ifq, m ALTQ_COMMA ALTQ_DECL(&pktattr));

	if (error == 0) {
		/*
		 * Count output packets and bytes.
		 * The packet length includes header + additional hardware
		 * framing according to RFC 1333.
		 */
		if (!(ifp->if_flags & IFF_OACTIVE))
			(*ifp->if_start)(ifp);
		ifp->if_obytes += m->m_pkthdr.len + sp->pp_framebytes;
	}
	splx(s);
	return error;
}

void
sppp_attach(struct ifnet *ifp)
{
	struct sppp *sp = (struct sppp *) ifp;

	/* Initialize keepalive handler. */
	if (! spppq) {
		callout_init(&keepalive_ch, 0);
		callout_reset(&keepalive_ch, hz * LCP_KEEPALIVE_INTERVAL, sppp_keepalive, NULL);
	}

	/* Insert new entry into the keepalive list. */
	sp->pp_next = spppq;
	spppq = sp;

	sp->pp_if.if_type = IFT_PPP;
	sp->pp_if.if_output = sppp_output;
	sp->pp_fastq.ifq_maxlen = 32;
	sp->pp_cpq.ifq_maxlen = 20;
	sp->pp_loopcnt = 0;
	sp->pp_alivecnt = 0;
	sp->pp_last_activity = 0;
	sp->pp_last_receive = 0;
	sp->pp_maxalive = DEFAULT_MAXALIVECNT;
	sp->pp_max_noreceive = DEFAULT_NORECV_TIME;
	sp->pp_idle_timeout = 0;
	memset(&sp->pp_seq[0], 0, sizeof(sp->pp_seq));
	memset(&sp->pp_rseq[0], 0, sizeof(sp->pp_rseq));
	sp->pp_auth_failures = 0;
	sp->pp_max_auth_fail = DEFAULT_MAX_AUTH_FAILURES;
	sp->pp_phase = SPPP_PHASE_DEAD;
	sp->pp_up = lcp.Up;
	sp->pp_down = lcp.Down;

	if_alloc_sadl(ifp);

	memset(&sp->myauth, 0, sizeof sp->myauth);
	memset(&sp->hisauth, 0, sizeof sp->hisauth);
	sppp_lcp_init(sp);
	sppp_ipcp_init(sp);
	sppp_ipv6cp_init(sp);
	sppp_pap_init(sp);
	sppp_chap_init(sp);
}

void
sppp_detach(struct ifnet *ifp)
{
	struct sppp **q, *p, *sp = (struct sppp *) ifp;

	/* Remove the entry from the keepalive list. */
	for (q = &spppq; (p = *q); q = &p->pp_next)
		if (p == sp) {
			*q = p->pp_next;
			break;
		}

	/* Stop keepalive handler. */
	if (! spppq) {
		callout_stop(&keepalive_ch);
	}

	callout_stop(&sp->ch[IDX_LCP]);
	callout_stop(&sp->ch[IDX_IPCP]);
	callout_stop(&sp->ch[IDX_PAP]);
	callout_stop(&sp->ch[IDX_CHAP]);
#ifdef INET6
	callout_stop(&sp->ch[IDX_IPV6CP]);
#endif
	callout_stop(&sp->pap_my_to_ch);

	/* free authentication info */
	if (sp->myauth.name) free(sp->myauth.name, M_DEVBUF);
	if (sp->myauth.secret) free(sp->myauth.secret, M_DEVBUF);
	if (sp->hisauth.name) free(sp->hisauth.name, M_DEVBUF);
	if (sp->hisauth.secret) free(sp->hisauth.secret, M_DEVBUF);
}

/*
 * Flush the interface output queue.
 */
void
sppp_flush(struct ifnet *ifp)
{
	struct sppp *sp = (struct sppp *) ifp;

	IFQ_PURGE(&sp->pp_if.if_snd);
	IF_PURGE(&sp->pp_fastq);
	IF_PURGE(&sp->pp_cpq);
}

/*
 * Check if the output queue is empty.
 */
int
sppp_isempty(struct ifnet *ifp)
{
	struct sppp *sp = (struct sppp *) ifp;
	int empty, s;

	s = splnet();
	empty = IF_IS_EMPTY(&sp->pp_fastq) && IF_IS_EMPTY(&sp->pp_cpq) &&
		IFQ_IS_EMPTY(&sp->pp_if.if_snd);
	splx(s);
	return (empty);
}

/*
 * Get next packet to send.
 */
struct mbuf *
sppp_dequeue(struct ifnet *ifp)
{
	struct sppp *sp = (struct sppp *) ifp;
	struct mbuf *m;
	int s;

	s = splnet();
	/*
	 * Process only the control protocol queue until we have at
	 * least one NCP open.
	 *
	 * Do always serve all three queues in Cisco mode.
	 */
	IF_DEQUEUE(&sp->pp_cpq, m);
	if (m == NULL &&
	    (sppp_ncp_check(sp) || (sp->pp_flags & PP_CISCO) != 0)) {
		IF_DEQUEUE(&sp->pp_fastq, m);
		if (m == NULL)
			IFQ_DEQUEUE(&sp->pp_if.if_snd, m);
	}
	splx(s);
	return m;
}

/*
 * Process an ioctl request.  Called on low priority level.
 */
int
sppp_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct lwp *l = curlwp;	/* XXX */
	struct ifreq *ifr = (struct ifreq *) data;
	struct ifaddr *ifa = (struct ifaddr *) data;
	struct sppp *sp = (struct sppp *) ifp;
	int s, error=0, going_up, going_down, newmode;

	s = splnet();
	switch (cmd) {
	case SIOCINITIFADDR:
		ifa->ifa_rtrequest = p2p_rtrequest;
		break;

	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		going_up = ifp->if_flags & IFF_UP &&
			(ifp->if_flags & IFF_RUNNING) == 0;
		going_down = (ifp->if_flags & IFF_UP) == 0 &&
			ifp->if_flags & IFF_RUNNING;
		newmode = ifp->if_flags & (IFF_AUTO | IFF_PASSIVE);
		if (newmode == (IFF_AUTO | IFF_PASSIVE)) {
			/* sanity */
			newmode = IFF_PASSIVE;
			ifp->if_flags &= ~IFF_AUTO;
		}

		if (going_up || going_down)
			lcp.Close(sp);
		if (going_up && newmode == 0) {
			/* neither auto-dial nor passive */
			ifp->if_flags |= IFF_RUNNING;
			if (!(sp->pp_flags & PP_CISCO))
				lcp.Open(sp);
		} else if (going_down) {
			sppp_flush(ifp);
			ifp->if_flags &= ~IFF_RUNNING;
		}

		break;

	case SIOCSIFMTU:
		if (ifr->ifr_mtu < PPP_MINMRU ||
		    ifr->ifr_mtu > sp->lcp.their_mru) {
			error = EINVAL;
			break;
		}
		/*FALLTHROUGH*/
	case SIOCGIFMTU:
		if ((error = ifioctl_common(ifp, cmd, data)) == ENETRESET)
			error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;

	case SPPPSETAUTHCFG:
	case SPPPSETLCPCFG:
	case SPPPSETIDLETO:
	case SPPPSETAUTHFAILURE:
	case SPPPSETDNSOPTS:
	case SPPPSETKEEPALIVE:
#if defined(COMPAT_50) || defined(MODULAR)
	case __SPPPSETIDLETO50:
	case __SPPPSETKEEPALIVE50:
#endif /* COMPAT_50 || MODULAR */
		error = kauth_authorize_network(l->l_cred,
		    KAUTH_NETWORK_INTERFACE,
		    KAUTH_REQ_NETWORK_INTERFACE_SETPRIV, ifp, (void *)cmd,
		    NULL);
		if (error)
			break;
		error = sppp_params(sp, cmd, data);
		break;

	case SPPPGETAUTHCFG:
	case SPPPGETLCPCFG:
	case SPPPGETAUTHFAILURES:
		error = kauth_authorize_network(l->l_cred,
		    KAUTH_NETWORK_INTERFACE,
		    KAUTH_REQ_NETWORK_INTERFACE_GETPRIV, ifp, (void *)cmd,
		    NULL);
		if (error)
			break;
		error = sppp_params(sp, cmd, data);
		break;

	case SPPPGETSTATUS:
	case SPPPGETSTATUSNCP:
	case SPPPGETIDLETO:
	case SPPPGETDNSOPTS:
	case SPPPGETDNSADDRS:
	case SPPPGETKEEPALIVE:
#if defined(COMPAT_50) || defined(MODULAR)
	case __SPPPGETIDLETO50:
	case __SPPPGETKEEPALIVE50:
#endif /* COMPAT_50 || MODULAR */
		error = sppp_params(sp, cmd, data);
		break;

	default:
		error = ifioctl_common(ifp, cmd, data);
		break;
	}
	splx(s);
	return (error);
}


/*
 * Cisco framing implementation.
 */

/*
 * Handle incoming Cisco keepalive protocol packets.
 */
static void
sppp_cisco_input(struct sppp *sp, struct mbuf *m)
{
	STDDCL;
	struct cisco_packet *h;
#ifdef INET
	uint32_t me, mymask = 0;	/* XXX: GCC */
#endif

	if (m->m_pkthdr.len < CISCO_PACKET_LEN) {
		if (debug)
			log(LOG_DEBUG,
			    "%s: cisco invalid packet length: %d bytes\n",
			    ifp->if_xname, m->m_pkthdr.len);
		return;
	}
	h = mtod(m, struct cisco_packet *);
	if (debug)
		log(LOG_DEBUG,
		    "%s: cisco input: %d bytes "
		    "<0x%x 0x%x 0x%x 0x%x 0x%x-0x%x>\n",
		    ifp->if_xname, m->m_pkthdr.len,
		    ntohl(h->type), h->par1, h->par2, (u_int)h->rel,
		    (u_int)h->time0, (u_int)h->time1);
	switch (ntohl(h->type)) {
	default:
		if (debug)
			addlog("%s: cisco unknown packet type: 0x%x\n",
			       ifp->if_xname, ntohl(h->type));
		break;
	case CISCO_ADDR_REPLY:
		/* Reply on address request, ignore */
		break;
	case CISCO_KEEPALIVE_REQ:
		sp->pp_alivecnt = 0;
		sp->pp_rseq[IDX_LCP] = ntohl(h->par1);
		if (sp->pp_seq[IDX_LCP] == sp->pp_rseq[IDX_LCP]) {
			/* Local and remote sequence numbers are equal.
			 * Probably, the line is in loopback mode. */
			if (sp->pp_loopcnt >= LOOPALIVECNT) {
				printf ("%s: loopback\n",
					ifp->if_xname);
				sp->pp_loopcnt = 0;
				if (ifp->if_flags & IFF_UP) {
					if_down(ifp);
					IF_PURGE(&sp->pp_cpq);
				}
			}
			++sp->pp_loopcnt;

			/* Generate new local sequence number */
			sp->pp_seq[IDX_LCP] = cprng_fast32();
			break;
		}
		sp->pp_loopcnt = 0;
		if (! (ifp->if_flags & IFF_UP) &&
		    (ifp->if_flags & IFF_RUNNING)) {
			if_up(ifp);
		}
		break;
	case CISCO_ADDR_REQ:
#ifdef INET
		sppp_get_ip_addrs(sp, &me, 0, &mymask);
		if (me != 0L)
			sppp_cisco_send(sp, CISCO_ADDR_REPLY, me, mymask);
#endif
		break;
	}
}

/*
 * Send Cisco keepalive packet.
 */
static void
sppp_cisco_send(struct sppp *sp, int type, int32_t par1, int32_t par2)
{
	STDDCL;
	struct ppp_header *h;
	struct cisco_packet *ch;
	struct mbuf *m;
	uint32_t t;

	t = time_uptime * 1000;
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (! m)
		return;
	m->m_pkthdr.len = m->m_len = PPP_HEADER_LEN + CISCO_PACKET_LEN;
	m->m_pkthdr.rcvif = 0;

	h = mtod(m, struct ppp_header *);
	h->address = CISCO_MULTICAST;
	h->control = 0;
	h->protocol = htons(CISCO_KEEPALIVE);

	ch = (struct cisco_packet *)(h + 1);
	ch->type = htonl(type);
	ch->par1 = htonl(par1);
	ch->par2 = htonl(par2);
	ch->rel = -1;

	ch->time0 = htons((u_short)(t >> 16));
	ch->time1 = htons((u_short) t);

	if (debug)
		log(LOG_DEBUG,
		    "%s: cisco output: <0x%x 0x%x 0x%x 0x%x 0x%x-0x%x>\n",
			ifp->if_xname, ntohl(ch->type), ch->par1,
			ch->par2, (u_int)ch->rel, (u_int)ch->time0,
			(u_int)ch->time1);

	if (IF_QFULL(&sp->pp_cpq)) {
		IF_DROP(&sp->pp_fastq);
		IF_DROP(&ifp->if_snd);
		m_freem(m);
		++ifp->if_oerrors;
		return;
	} else
		IF_ENQUEUE(&sp->pp_cpq, m);
	if (! (ifp->if_flags & IFF_OACTIVE))
		(*ifp->if_start)(ifp);
	ifp->if_obytes += m->m_pkthdr.len + sp->pp_framebytes;
}

/*
 * PPP protocol implementation.
 */

/*
 * Send PPP control protocol packet.
 */
static void
sppp_cp_send(struct sppp *sp, u_short proto, u_char type,
	     u_char ident, u_short len, void *data)
{
	STDDCL;
	struct lcp_header *lh;
	struct mbuf *m;
	size_t pkthdrlen;

	pkthdrlen = (sp->pp_flags & PP_NOFRAMING) ? 2 : PPP_HEADER_LEN;

	if (len > MHLEN - pkthdrlen - LCP_HEADER_LEN)
		len = MHLEN - pkthdrlen - LCP_HEADER_LEN;
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (! m)
		return;
	m->m_pkthdr.len = m->m_len = pkthdrlen + LCP_HEADER_LEN + len;
	m->m_pkthdr.rcvif = 0;

	if (sp->pp_flags & PP_NOFRAMING) {
		*mtod(m, uint16_t *) = htons(proto);
		lh = (struct lcp_header *)(mtod(m, uint8_t *) + 2);
	} else {
		struct ppp_header *h;
		h = mtod(m, struct ppp_header *);
		h->address = PPP_ALLSTATIONS;        /* broadcast address */
		h->control = PPP_UI;                 /* Unnumbered Info */
		h->protocol = htons(proto);         /* Link Control Protocol */
		lh = (struct lcp_header *)(h + 1);
	}
	lh->type = type;
	lh->ident = ident;
	lh->len = htons(LCP_HEADER_LEN + len);
	if (len)
		bcopy (data, lh + 1, len);

	if (debug) {
		log(LOG_DEBUG, "%s: %s output <%s id=0x%x len=%d",
		    ifp->if_xname,
		    sppp_proto_name(proto),
		    sppp_cp_type_name(lh->type), lh->ident, ntohs(lh->len));
		if (len)
			sppp_print_bytes((u_char *)(lh + 1), len);
		addlog(">\n");
	}
	if (IF_QFULL(&sp->pp_cpq)) {
		IF_DROP(&sp->pp_fastq);
		IF_DROP(&ifp->if_snd);
		m_freem(m);
		++ifp->if_oerrors;
		return;
	} else
		IF_ENQUEUE(&sp->pp_cpq, m);
	if (! (ifp->if_flags & IFF_OACTIVE))
		(*ifp->if_start)(ifp);
	ifp->if_obytes += m->m_pkthdr.len + sp->pp_framebytes;
}

/*
 * Handle incoming PPP control protocol packets.
 */
static void
sppp_cp_input(const struct cp *cp, struct sppp *sp, struct mbuf *m)
{
	STDDCL;
	struct lcp_header *h;
	int printlen, len = m->m_pkthdr.len;
	int rv;
	u_char *p;
	uint32_t u32;

	if (len < 4) {
		if (debug)
			log(LOG_DEBUG,
			    "%s: %s invalid packet length: %d bytes\n",
			    ifp->if_xname, cp->name, len);
		return;
	}
	h = mtod(m, struct lcp_header *);
	if (debug) {
		printlen = ntohs(h->len);
		log(LOG_DEBUG,
		    "%s: %s input(%s): <%s id=0x%x len=%d",
		    ifp->if_xname, cp->name,
		    sppp_state_name(sp->state[cp->protoidx]),
		    sppp_cp_type_name(h->type), h->ident, printlen);
		if (len < printlen)
			printlen = len;
		if (printlen > 4)
			sppp_print_bytes((u_char *)(h + 1), printlen - 4);
		addlog(">\n");
	}
	if (len > ntohs(h->len))
		len = ntohs(h->len);
	p = (u_char *)(h + 1);
	switch (h->type) {
	case CONF_REQ:
		if (len < 4) {
			if (debug)
				addlog("%s: %s invalid conf-req length %d\n",
				       ifp->if_xname, cp->name,
				       len);
			++ifp->if_ierrors;
			break;
		}
		/* handle states where RCR doesn't get a SCA/SCN */
		switch (sp->state[cp->protoidx]) {
		case STATE_CLOSING:
		case STATE_STOPPING:
			return;
		case STATE_CLOSED:
			sppp_cp_send(sp, cp->proto, TERM_ACK, h->ident,
				     0, 0);
			return;
		}
		rv = (cp->RCR)(sp, h, len);
		if (rv < 0) {
			/* fatal error, shut down */
			(cp->tld)(sp);
			sppp_lcp_tlf(sp);
			return;
		}
		switch (sp->state[cp->protoidx]) {
		case STATE_OPENED:
			(cp->tld)(sp);
			(cp->scr)(sp);
			/* fall through... */
		case STATE_ACK_SENT:
		case STATE_REQ_SENT:
			sppp_cp_change_state(cp, sp, rv?
					     STATE_ACK_SENT: STATE_REQ_SENT);
			break;
		case STATE_STOPPED:
			sp->rst_counter[cp->protoidx] = sp->lcp.max_configure;
			(cp->scr)(sp);
			sppp_cp_change_state(cp, sp, rv?
					     STATE_ACK_SENT: STATE_REQ_SENT);
			break;
		case STATE_ACK_RCVD:
			if (rv) {
				sppp_cp_change_state(cp, sp, STATE_OPENED);
				if (debug)
					log(LOG_DEBUG, "%s: %s tlu\n",
					    ifp->if_xname,
					    cp->name);
				(cp->tlu)(sp);
			} else
				sppp_cp_change_state(cp, sp, STATE_ACK_RCVD);
			break;
		default:
			printf("%s: %s illegal %s in state %s\n",
			       ifp->if_xname, cp->name,
			       sppp_cp_type_name(h->type),
			       sppp_state_name(sp->state[cp->protoidx]));
			++ifp->if_ierrors;
		}
		break;
	case CONF_ACK:
		if (h->ident != sp->confid[cp->protoidx]) {
			if (debug)
				addlog("%s: %s id mismatch 0x%x != 0x%x\n",
				       ifp->if_xname, cp->name,
				       h->ident, sp->confid[cp->protoidx]);
			++ifp->if_ierrors;
			break;
		}
		switch (sp->state[cp->protoidx]) {
		case STATE_CLOSED:
		case STATE_STOPPED:
			sppp_cp_send(sp, cp->proto, TERM_ACK, h->ident, 0, 0);
			break;
		case STATE_CLOSING:
		case STATE_STOPPING:
			break;
		case STATE_REQ_SENT:
			sp->rst_counter[cp->protoidx] = sp->lcp.max_configure;
			sppp_cp_change_state(cp, sp, STATE_ACK_RCVD);
			break;
		case STATE_OPENED:
			(cp->tld)(sp);
			/* fall through */
		case STATE_ACK_RCVD:
			(cp->scr)(sp);
			sppp_cp_change_state(cp, sp, STATE_REQ_SENT);
			break;
		case STATE_ACK_SENT:
			sp->rst_counter[cp->protoidx] = sp->lcp.max_configure;
			sppp_cp_change_state(cp, sp, STATE_OPENED);
			if (debug)
				log(LOG_DEBUG, "%s: %s tlu\n",
				       ifp->if_xname, cp->name);
			(cp->tlu)(sp);
			break;
		default:
			printf("%s: %s illegal %s in state %s\n",
			       ifp->if_xname, cp->name,
			       sppp_cp_type_name(h->type),
			       sppp_state_name(sp->state[cp->protoidx]));
			++ifp->if_ierrors;
		}
		break;
	case CONF_NAK:
	case CONF_REJ:
		if (h->ident != sp->confid[cp->protoidx]) {
			if (debug)
				addlog("%s: %s id mismatch 0x%x != 0x%x\n",
				       ifp->if_xname, cp->name,
				       h->ident, sp->confid[cp->protoidx]);
			++ifp->if_ierrors;
			break;
		}
		if (h->type == CONF_NAK)
			(cp->RCN_nak)(sp, h, len);
		else /* CONF_REJ */
			(cp->RCN_rej)(sp, h, len);

		switch (sp->state[cp->protoidx]) {
		case STATE_CLOSED:
		case STATE_STOPPED:
			sppp_cp_send(sp, cp->proto, TERM_ACK, h->ident, 0, 0);
			break;
		case STATE_REQ_SENT:
		case STATE_ACK_SENT:
			sp->rst_counter[cp->protoidx] = sp->lcp.max_configure;
			(cp->scr)(sp);
			break;
		case STATE_OPENED:
			(cp->tld)(sp);
			/* fall through */
		case STATE_ACK_RCVD:
			sppp_cp_change_state(cp, sp, STATE_ACK_SENT);
			(cp->scr)(sp);
			break;
		case STATE_CLOSING:
		case STATE_STOPPING:
			break;
		default:
			printf("%s: %s illegal %s in state %s\n",
			       ifp->if_xname, cp->name,
			       sppp_cp_type_name(h->type),
			       sppp_state_name(sp->state[cp->protoidx]));
			++ifp->if_ierrors;
		}
		break;

	case TERM_REQ:
		switch (sp->state[cp->protoidx]) {
		case STATE_ACK_RCVD:
		case STATE_ACK_SENT:
			sppp_cp_change_state(cp, sp, STATE_REQ_SENT);
			/* fall through */
		case STATE_CLOSED:
		case STATE_STOPPED:
		case STATE_CLOSING:
		case STATE_STOPPING:
		case STATE_REQ_SENT:
		  sta:
			/* Send Terminate-Ack packet. */
			if (debug)
				log(LOG_DEBUG, "%s: %s send terminate-ack\n",
				    ifp->if_xname, cp->name);
			sppp_cp_send(sp, cp->proto, TERM_ACK, h->ident, 0, 0);
			break;
		case STATE_OPENED:
			(cp->tld)(sp);
			sp->rst_counter[cp->protoidx] = 0;
			sppp_cp_change_state(cp, sp, STATE_STOPPING);
			goto sta;
		default:
			printf("%s: %s illegal %s in state %s\n",
			       ifp->if_xname, cp->name,
			       sppp_cp_type_name(h->type),
			       sppp_state_name(sp->state[cp->protoidx]));
			++ifp->if_ierrors;
		}
		break;
	case TERM_ACK:
		switch (sp->state[cp->protoidx]) {
		case STATE_CLOSED:
		case STATE_STOPPED:
		case STATE_REQ_SENT:
		case STATE_ACK_SENT:
			break;
		case STATE_CLOSING:
			(cp->tlf)(sp);
			sppp_cp_change_state(cp, sp, STATE_CLOSED);
			sppp_lcp_check_and_close(sp);
			break;
		case STATE_STOPPING:
			(cp->tlf)(sp);
			sppp_cp_change_state(cp, sp, STATE_STOPPED);
			sppp_lcp_check_and_close(sp);
			break;
		case STATE_ACK_RCVD:
			sppp_cp_change_state(cp, sp, STATE_REQ_SENT);
			break;
		case STATE_OPENED:
			(cp->tld)(sp);
			(cp->scr)(sp);
			sppp_cp_change_state(cp, sp, STATE_ACK_RCVD);
			break;
		default:
			printf("%s: %s illegal %s in state %s\n",
			       ifp->if_xname, cp->name,
			       sppp_cp_type_name(h->type),
			       sppp_state_name(sp->state[cp->protoidx]));
			++ifp->if_ierrors;
		}
		break;
	case CODE_REJ:
		/* XXX catastrophic rejects (RXJ-) aren't handled yet. */
		log(LOG_INFO,
		    "%s: %s: ignoring RXJ (%s) for code ?, "
		    "danger will robinson\n",
		    ifp->if_xname, cp->name,
		    sppp_cp_type_name(h->type));
		switch (sp->state[cp->protoidx]) {
		case STATE_CLOSED:
		case STATE_STOPPED:
		case STATE_REQ_SENT:
		case STATE_ACK_SENT:
		case STATE_CLOSING:
		case STATE_STOPPING:
		case STATE_OPENED:
			break;
		case STATE_ACK_RCVD:
			sppp_cp_change_state(cp, sp, STATE_REQ_SENT);
			break;
		default:
			printf("%s: %s illegal %s in state %s\n",
			       ifp->if_xname, cp->name,
			       sppp_cp_type_name(h->type),
			       sppp_state_name(sp->state[cp->protoidx]));
			++ifp->if_ierrors;
		}
		break;
	case PROTO_REJ:
	    {
		int catastrophic;
		const struct cp *upper;
		int i;
		uint16_t proto;

		catastrophic = 0;
		upper = NULL;
		proto = p[0] << 8 | p[1];
		for (i = 0; i < IDX_COUNT; i++) {
			if (cps[i]->proto == proto) {
				upper = cps[i];
				break;
			}
		}
		if (upper == NULL)
			catastrophic++;

		if (debug)
			log(LOG_INFO,
			    "%s: %s: RXJ%c (%s) for proto 0x%x (%s/%s)\n",
			    ifp->if_xname, cp->name, catastrophic ? '-' : '+',
			    sppp_cp_type_name(h->type), proto,
			    upper ? upper->name : "unknown",
			    upper ? sppp_state_name(sp->state[upper->protoidx]) : "?");

		/*
		 * if we got RXJ+ against conf-req, the peer does not implement
		 * this particular protocol type.  terminate the protocol.
		 */
		if (upper && !catastrophic) {
			if (sp->state[upper->protoidx] == STATE_REQ_SENT) {
				upper->Close(sp);
				break;
			}
		}

		/* XXX catastrophic rejects (RXJ-) aren't handled yet. */
		switch (sp->state[cp->protoidx]) {
		case STATE_CLOSED:
		case STATE_STOPPED:
		case STATE_REQ_SENT:
		case STATE_ACK_SENT:
		case STATE_CLOSING:
		case STATE_STOPPING:
		case STATE_OPENED:
			break;
		case STATE_ACK_RCVD:
			sppp_cp_change_state(cp, sp, STATE_REQ_SENT);
			break;
		default:
			printf("%s: %s illegal %s in state %s\n",
			       ifp->if_xname, cp->name,
			       sppp_cp_type_name(h->type),
			       sppp_state_name(sp->state[cp->protoidx]));
			++ifp->if_ierrors;
		}
		break;
	    }
	case DISC_REQ:
		if (cp->proto != PPP_LCP)
			goto illegal;
		/* Discard the packet. */
		break;
	case ECHO_REQ:
		if (cp->proto != PPP_LCP)
			goto illegal;
		if (sp->state[cp->protoidx] != STATE_OPENED) {
			if (debug)
				addlog("%s: lcp echo req but lcp closed\n",
				       ifp->if_xname);
			++ifp->if_ierrors;
			break;
		}
		if (len < 8) {
			if (debug)
				addlog("%s: invalid lcp echo request "
				       "packet length: %d bytes\n",
				       ifp->if_xname, len);
			break;
		}
		memcpy(&u32, h + 1, sizeof u32);
		if (ntohl(u32) == sp->lcp.magic) {
			/* Line loopback mode detected. */
			printf("%s: loopback\n", ifp->if_xname);
			if_down(ifp);
			IF_PURGE(&sp->pp_cpq);

			/* Shut down the PPP link. */
			/* XXX */
			lcp.Down(sp);
			lcp.Up(sp);
			break;
		}
		u32 = htonl(sp->lcp.magic);
		memcpy(h + 1, &u32, sizeof u32);
		if (debug)
			addlog("%s: got lcp echo req, sending echo rep\n",
			       ifp->if_xname);
		sppp_cp_send(sp, PPP_LCP, ECHO_REPLY, h->ident, len - 4,
		    h + 1);
		break;
	case ECHO_REPLY:
		if (cp->proto != PPP_LCP)
			goto illegal;
		if (h->ident != sp->lcp.echoid) {
			++ifp->if_ierrors;
			break;
		}
		if (len < 8) {
			if (debug)
				addlog("%s: lcp invalid echo reply "
				       "packet length: %d bytes\n",
				       ifp->if_xname, len);
			break;
		}
		if (debug)
			addlog("%s: lcp got echo rep\n",
			       ifp->if_xname);
		memcpy(&u32, h + 1, sizeof u32);
		if (ntohl(u32) != sp->lcp.magic)
			sp->pp_alivecnt = 0;
		break;
	default:
		/* Unknown packet type -- send Code-Reject packet. */
	  illegal:
		if (debug)
			addlog("%s: %s send code-rej for 0x%x\n",
			       ifp->if_xname, cp->name, h->type);
		sppp_cp_send(sp, cp->proto, CODE_REJ,
		    ++sp->pp_seq[cp->protoidx], m->m_pkthdr.len, h);
		++ifp->if_ierrors;
	}
}


/*
 * The generic part of all Up/Down/Open/Close/TO event handlers.
 * Basically, the state transition handling in the automaton.
 */
static void
sppp_up_event(const struct cp *cp, struct sppp *sp)
{
	STDDCL;

	if (debug)
		log(LOG_DEBUG, "%s: %s up(%s)\n",
		    ifp->if_xname, cp->name,
		    sppp_state_name(sp->state[cp->protoidx]));

	switch (sp->state[cp->protoidx]) {
	case STATE_INITIAL:
		sppp_cp_change_state(cp, sp, STATE_CLOSED);
		break;
	case STATE_STARTING:
		sp->rst_counter[cp->protoidx] = sp->lcp.max_configure;
		(cp->scr)(sp);
		sppp_cp_change_state(cp, sp, STATE_REQ_SENT);
		break;
	default:
		printf("%s: %s illegal up in state %s\n",
		       ifp->if_xname, cp->name,
		       sppp_state_name(sp->state[cp->protoidx]));
	}
}

static void
sppp_down_event(const struct cp *cp, struct sppp *sp)
{
	STDDCL;

	if (debug)
		log(LOG_DEBUG, "%s: %s down(%s)\n",
		    ifp->if_xname, cp->name,
		    sppp_state_name(sp->state[cp->protoidx]));

	switch (sp->state[cp->protoidx]) {
	case STATE_CLOSED:
	case STATE_CLOSING:
		sppp_cp_change_state(cp, sp, STATE_INITIAL);
		break;
	case STATE_STOPPED:
		(cp->tls)(sp);
		/* fall through */
	case STATE_STOPPING:
	case STATE_REQ_SENT:
	case STATE_ACK_RCVD:
	case STATE_ACK_SENT:
		sppp_cp_change_state(cp, sp, STATE_STARTING);
		break;
	case STATE_OPENED:
		(cp->tld)(sp);
		sppp_cp_change_state(cp, sp, STATE_STARTING);
		break;
	default:
		printf("%s: %s illegal down in state %s\n",
		       ifp->if_xname, cp->name,
		       sppp_state_name(sp->state[cp->protoidx]));
	}
}


static void
sppp_open_event(const struct cp *cp, struct sppp *sp)
{
	STDDCL;

	if (debug)
		log(LOG_DEBUG, "%s: %s open(%s)\n",
		    ifp->if_xname, cp->name,
		    sppp_state_name(sp->state[cp->protoidx]));

	switch (sp->state[cp->protoidx]) {
	case STATE_INITIAL:
		sppp_cp_change_state(cp, sp, STATE_STARTING);
		(cp->tls)(sp);
		break;
	case STATE_STARTING:
		break;
	case STATE_CLOSED:
		sp->rst_counter[cp->protoidx] = sp->lcp.max_configure;
		(cp->scr)(sp);
		sppp_cp_change_state(cp, sp, STATE_REQ_SENT);
		break;
	case STATE_STOPPED:
	case STATE_STOPPING:
	case STATE_REQ_SENT:
	case STATE_ACK_RCVD:
	case STATE_ACK_SENT:
	case STATE_OPENED:
		break;
	case STATE_CLOSING:
		sppp_cp_change_state(cp, sp, STATE_STOPPING);
		break;
	}
}


static void
sppp_close_event(const struct cp *cp, struct sppp *sp)
{
	STDDCL;

	if (debug)
		log(LOG_DEBUG, "%s: %s close(%s)\n",
		    ifp->if_xname, cp->name,
		    sppp_state_name(sp->state[cp->protoidx]));

	switch (sp->state[cp->protoidx]) {
	case STATE_INITIAL:
	case STATE_CLOSED:
	case STATE_CLOSING:
		break;
	case STATE_STARTING:
		sppp_cp_change_state(cp, sp, STATE_INITIAL);
		(cp->tlf)(sp);
		break;
	case STATE_STOPPED:
		sppp_cp_change_state(cp, sp, STATE_CLOSED);
		break;
	case STATE_STOPPING:
		sppp_cp_change_state(cp, sp, STATE_CLOSING);
		break;
	case STATE_OPENED:
		(cp->tld)(sp);
		/* fall through */
	case STATE_REQ_SENT:
	case STATE_ACK_RCVD:
	case STATE_ACK_SENT:
		sp->rst_counter[cp->protoidx] = sp->lcp.max_terminate;
		sppp_cp_send(sp, cp->proto, TERM_REQ,
		    ++sp->pp_seq[cp->protoidx], 0, 0);
		sppp_cp_change_state(cp, sp, STATE_CLOSING);
		break;
	}
}

static void
sppp_to_event(const struct cp *cp, struct sppp *sp)
{
	STDDCL;
	int s;

	s = splnet();
	if (debug)
		log(LOG_DEBUG, "%s: %s TO(%s) rst_counter = %d\n",
		    ifp->if_xname, cp->name,
		    sppp_state_name(sp->state[cp->protoidx]),
		    sp->rst_counter[cp->protoidx]);

	if (--sp->rst_counter[cp->protoidx] < 0)
		/* TO- event */
		switch (sp->state[cp->protoidx]) {
		case STATE_CLOSING:
			(cp->tlf)(sp);
			sppp_cp_change_state(cp, sp, STATE_CLOSED);
			sppp_lcp_check_and_close(sp);
			break;
		case STATE_STOPPING:
			(cp->tlf)(sp);
			sppp_cp_change_state(cp, sp, STATE_STOPPED);
			sppp_lcp_check_and_close(sp);
			break;
		case STATE_REQ_SENT:
		case STATE_ACK_RCVD:
		case STATE_ACK_SENT:
			(cp->tlf)(sp);
			sppp_cp_change_state(cp, sp, STATE_STOPPED);
			sppp_lcp_check_and_close(sp);
			break;
		}
	else
		/* TO+ event */
		switch (sp->state[cp->protoidx]) {
		case STATE_CLOSING:
		case STATE_STOPPING:
			sppp_cp_send(sp, cp->proto, TERM_REQ,
			    ++sp->pp_seq[cp->protoidx], 0, 0);
			callout_reset(&sp->ch[cp->protoidx], sp->lcp.timeout,
			    cp->TO, sp);
			break;
		case STATE_REQ_SENT:
		case STATE_ACK_RCVD:
			(cp->scr)(sp);
			/* sppp_cp_change_state() will restart the timer */
			sppp_cp_change_state(cp, sp, STATE_REQ_SENT);
			break;
		case STATE_ACK_SENT:
			(cp->scr)(sp);
			callout_reset(&sp->ch[cp->protoidx], sp->lcp.timeout,
			    cp->TO, sp);
			break;
		}

	splx(s);
}

/*
 * Change the state of a control protocol in the state automaton.
 * Takes care of starting/stopping the restart timer.
 */
void
sppp_cp_change_state(const struct cp *cp, struct sppp *sp, int newstate)
{
	sp->state[cp->protoidx] = newstate;
	callout_stop(&sp->ch[cp->protoidx]);
	switch (newstate) {
	case STATE_INITIAL:
	case STATE_STARTING:
	case STATE_CLOSED:
	case STATE_STOPPED:
	case STATE_OPENED:
		break;
	case STATE_CLOSING:
	case STATE_STOPPING:
	case STATE_REQ_SENT:
	case STATE_ACK_RCVD:
	case STATE_ACK_SENT:
		callout_reset(&sp->ch[cp->protoidx], sp->lcp.timeout,
		    cp->TO, sp);
		break;
	}
}

/*
 *--------------------------------------------------------------------------*
 *                                                                          *
 *                         The LCP implementation.                          *
 *                                                                          *
 *--------------------------------------------------------------------------*
 */
static void
sppp_lcp_init(struct sppp *sp)
{
	sp->lcp.opts = (1 << LCP_OPT_MAGIC);
	sp->lcp.magic = 0;
	sp->state[IDX_LCP] = STATE_INITIAL;
	sp->fail_counter[IDX_LCP] = 0;
	sp->pp_seq[IDX_LCP] = 0;
	sp->pp_rseq[IDX_LCP] = 0;
	sp->lcp.protos = 0;

	/*
	 * Initialize counters and timeout values.  Note that we don't
	 * use the 3 seconds suggested in RFC 1661 since we are likely
	 * running on a fast link.  XXX We should probably implement
	 * the exponential backoff option.  Note that these values are
	 * relevant for all control protocols, not just LCP only.
	 */
	sp->lcp.timeout = 1 * hz;
	sp->lcp.max_terminate = 2;
	sp->lcp.max_configure = 10;
	sp->lcp.max_failure = 10;
	callout_init(&sp->ch[IDX_LCP], 0);
}

static void
sppp_lcp_up(struct sppp *sp)
{
	STDDCL;

	/* Initialize activity timestamp: opening a connection is an activity */
	sp->pp_last_receive = sp->pp_last_activity = time_uptime;

	/*
	 * If this interface is passive or dial-on-demand, and we are
	 * still in Initial state, it means we've got an incoming
	 * call.  Activate the interface.
	 */
	if ((ifp->if_flags & (IFF_AUTO | IFF_PASSIVE)) != 0) {
		if (debug)
			log(LOG_DEBUG,
			    "%s: Up event", ifp->if_xname);
		ifp->if_flags |= IFF_RUNNING;
		if (sp->state[IDX_LCP] == STATE_INITIAL) {
			if (debug)
				addlog("(incoming call)\n");
			sp->pp_flags |= PP_CALLIN;
			lcp.Open(sp);
		} else if (debug)
			addlog("\n");
	} else if ((ifp->if_flags & (IFF_AUTO | IFF_PASSIVE)) == 0 &&
		   (sp->state[IDX_LCP] == STATE_INITIAL)) {
			ifp->if_flags |= IFF_RUNNING;
			lcp.Open(sp);
	}

	sppp_up_event(&lcp, sp);
}

static void
sppp_lcp_down(struct sppp *sp)
{
	STDDCL;

	sppp_down_event(&lcp, sp);

	/*
	 * If this is neither a dial-on-demand nor a passive
	 * interface, simulate an ``ifconfig down'' action, so the
	 * administrator can force a redial by another ``ifconfig
	 * up''.  XXX For leased line operation, should we immediately
	 * try to reopen the connection here?
	 */
	if ((ifp->if_flags & (IFF_AUTO | IFF_PASSIVE)) == 0) {
		if (debug)
			log(LOG_INFO,
			    "%s: Down event (carrier loss), taking interface down.\n",
			    ifp->if_xname);
		if_down(ifp);
	} else {
		if (debug)
			log(LOG_DEBUG,
			    "%s: Down event (carrier loss)\n",
			    ifp->if_xname);
	}
	sp->pp_flags &= ~PP_CALLIN;
	if (sp->state[IDX_LCP] != STATE_INITIAL)
		lcp.Close(sp);
	ifp->if_flags &= ~IFF_RUNNING;
}

static void
sppp_lcp_open(struct sppp *sp)
{
	if (sp->pp_if.if_mtu < PP_MTU) {
		sp->lcp.mru = sp->pp_if.if_mtu;
		sp->lcp.opts |= (1 << LCP_OPT_MRU);
	} else
		sp->lcp.mru = PP_MTU;
	sp->lcp.their_mru = PP_MTU;

	/*
	 * If we are authenticator, negotiate LCP_AUTH
	 */
	if (sp->hisauth.proto != 0)
		sp->lcp.opts |= (1 << LCP_OPT_AUTH_PROTO);
	else
		sp->lcp.opts &= ~(1 << LCP_OPT_AUTH_PROTO);
	sp->pp_flags &= ~PP_NEEDAUTH;
	sppp_open_event(&lcp, sp);
}

static void
sppp_lcp_close(struct sppp *sp)
{
	sppp_close_event(&lcp, sp);
}

static void
sppp_lcp_TO(void *cookie)
{
	sppp_to_event(&lcp, (struct sppp *)cookie);
}

/*
 * Analyze a configure request.  Return true if it was agreeable, and
 * caused action sca, false if it has been rejected or nak'ed, and
 * caused action scn.  (The return value is used to make the state
 * transition decision in the state automaton.)
 */
static int
sppp_lcp_RCR(struct sppp *sp, struct lcp_header *h, int len)
{
	STDDCL;
	u_char *buf, *r, *p;
	int origlen, rlen;
	uint32_t nmagic;
	u_short authproto;

	len -= 4;
	origlen = len;
	buf = r = malloc (len, M_TEMP, M_NOWAIT);
	if (! buf)
		return (0);

	if (debug)
		log(LOG_DEBUG, "%s: lcp parse opts:",
		    ifp->if_xname);

	/* pass 1: check for things that need to be rejected */
	p = (void *)(h + 1);
	for (rlen=0; len>1 && p[1]; len-=p[1], p+=p[1]) {
		/* Sanity check option length */
		if (p[1] > len) {
			/*
			 * Malicious option - drop immediately.
			 * XXX Maybe we should just RXJ it?
			 */
			addlog("%s: received malicious LCP option 0x%02x, "
			    "length 0x%02x, (len: 0x%02x) dropping.\n", ifp->if_xname,
			    p[0], p[1], len);
			goto drop;
		}
		if (debug)
			addlog(" %s", sppp_lcp_opt_name(*p));
		switch (*p) {
		case LCP_OPT_MAGIC:
			/* Magic number. */
			/* fall through, both are same length */
		case LCP_OPT_ASYNC_MAP:
			/* Async control character map. */
			if (len >= 6 || p[1] == 6)
				continue;
			if (debug)
				addlog(" [invalid]");
			break;
		case LCP_OPT_MRU:
			/* Maximum receive unit. */
			if (len >= 4 && p[1] == 4)
				continue;
			if (debug)
				addlog(" [invalid]");
			break;
		case LCP_OPT_AUTH_PROTO:
			if (len < 4) {
				if (debug)
					addlog(" [invalid]");
				break;
			}
			authproto = (p[2] << 8) + p[3];
			if (authproto == PPP_CHAP && p[1] != 5) {
				if (debug)
					addlog(" [invalid chap len]");
				break;
			}
			if (sp->myauth.proto == 0) {
				/* we are not configured to do auth */
				if (debug)
					addlog(" [not configured]");
				break;
			}
			/*
			 * Remote want us to authenticate, remember this,
			 * so we stay in SPPP_PHASE_AUTHENTICATE after LCP got
			 * up.
			 */
			sp->pp_flags |= PP_NEEDAUTH;
			continue;
		default:
			/* Others not supported. */
			if (debug)
				addlog(" [rej]");
			break;
		}
		/* Add the option to rejected list. */
		bcopy (p, r, p[1]);
		r += p[1];
		rlen += p[1];
	}
	if (rlen) {
		if (debug)
			addlog(" send conf-rej\n");
		sppp_cp_send(sp, PPP_LCP, CONF_REJ, h->ident, rlen, buf);
		goto end;
	} else if (debug)
		addlog("\n");

	/*
	 * pass 2: check for option values that are unacceptable and
	 * thus require to be nak'ed.
	 */
	if (debug)
		log(LOG_DEBUG, "%s: lcp parse opt values: ",
		    ifp->if_xname);

	p = (void *)(h + 1);
	len = origlen;
	for (rlen=0; len>1 && p[1]; len-=p[1], p+=p[1]) {
		if (debug)
			addlog(" %s", sppp_lcp_opt_name(*p));
		switch (*p) {
		case LCP_OPT_MAGIC:
			/* Magic number -- extract. */
			nmagic = (uint32_t)p[2] << 24 |
				(uint32_t)p[3] << 16 | p[4] << 8 | p[5];
			if (nmagic != sp->lcp.magic) {
				if (debug)
					addlog(" 0x%x", nmagic);
				continue;
			}
			/*
			 * Local and remote magics equal -- loopback?
			 */
			if (sp->pp_loopcnt >= LOOPALIVECNT*5) {
				printf ("%s: loopback\n",
					ifp->if_xname);
				sp->pp_loopcnt = 0;
				if (ifp->if_flags & IFF_UP) {
					if_down(ifp);
					IF_PURGE(&sp->pp_cpq);
					/* XXX ? */
					lcp.Down(sp);
					lcp.Up(sp);
				}
			} else if (debug)
				addlog(" [glitch]");
			++sp->pp_loopcnt;
			/*
			 * We negate our magic here, and NAK it.  If
			 * we see it later in an NAK packet, we
			 * suggest a new one.
			 */
			nmagic = ~sp->lcp.magic;
			/* Gonna NAK it. */
			p[2] = nmagic >> 24;
			p[3] = nmagic >> 16;
			p[4] = nmagic >> 8;
			p[5] = nmagic;
			break;

		case LCP_OPT_ASYNC_MAP:
			/*
			 * Async control character map -- just ignore it.
			 *
			 * Quote from RFC 1662, chapter 6:
			 * To enable this functionality, synchronous PPP
			 * implementations MUST always respond to the
			 * Async-Control-Character-Map Configuration
			 * Option with the LCP Configure-Ack.  However,
			 * acceptance of the Configuration Option does
			 * not imply that the synchronous implementation
			 * will do any ACCM mapping.  Instead, all such
			 * octet mapping will be performed by the
			 * asynchronous-to-synchronous converter.
			 */
			continue;

		case LCP_OPT_MRU:
			/*
			 * Maximum receive unit.  Always agreeable,
			 * but ignored by now.
			 */
			sp->lcp.their_mru = p[2] * 256 + p[3];
			if (debug)
				addlog(" %ld", sp->lcp.their_mru);
			continue;

		case LCP_OPT_AUTH_PROTO:
			authproto = (p[2] << 8) + p[3];
			if (sp->myauth.proto != authproto) {
				/* not agreed, nak */
				if (debug)
					addlog(" [mine %s != his %s]",
					       sppp_proto_name(sp->myauth.proto),
					       sppp_proto_name(authproto));
				p[2] = sp->myauth.proto >> 8;
				p[3] = sp->myauth.proto;
				break;
			}
			if (authproto == PPP_CHAP && p[4] != CHAP_MD5) {
				if (debug)
					addlog(" [chap not MD5]");
				p[4] = CHAP_MD5;
				break;
			}
			continue;
		}
		/* Add the option to nak'ed list. */
		bcopy (p, r, p[1]);
		r += p[1];
		rlen += p[1];
	}
	if (rlen) {
		if (++sp->fail_counter[IDX_LCP] >= sp->lcp.max_failure) {
			if (debug)
				addlog(" max_failure (%d) exceeded, "
				       "send conf-rej\n",
				       sp->lcp.max_failure);
			sppp_cp_send(sp, PPP_LCP, CONF_REJ, h->ident, rlen, buf);
		} else {
			if (debug)
				addlog(" send conf-nak\n");
			sppp_cp_send(sp, PPP_LCP, CONF_NAK, h->ident, rlen, buf);
		}
		goto end;
	} else {
		if (debug)
			addlog(" send conf-ack\n");
		sp->fail_counter[IDX_LCP] = 0;
		sp->pp_loopcnt = 0;
		sppp_cp_send(sp, PPP_LCP, CONF_ACK, h->ident, origlen, h + 1);
	}

 end:
	free(buf, M_TEMP);
	return (rlen == 0);

 drop:
	free(buf, M_TEMP);
	return -1;
}

/*
 * Analyze the LCP Configure-Reject option list, and adjust our
 * negotiation.
 */
static void
sppp_lcp_RCN_rej(struct sppp *sp, struct lcp_header *h, int len)
{
	STDDCL;
	u_char *buf, *p;

	len -= 4;
	buf = malloc (len, M_TEMP, M_NOWAIT);
	if (!buf)
		return;

	if (debug)
		log(LOG_DEBUG, "%s: lcp rej opts:",
		    ifp->if_xname);

	p = (void *)(h + 1);
	for (; len > 1 && p[1]; len -= p[1], p += p[1]) {
		/* Sanity check option length */
		if (p[1] > len) {
			/*
			 * Malicious option - drop immediately.
			 * XXX Maybe we should just RXJ it?
			 */
			addlog("%s: received malicious LCP option, "
			    "dropping.\n", ifp->if_xname);
			goto drop;
		}
		if (debug)
			addlog(" %s", sppp_lcp_opt_name(*p));
		switch (*p) {
		case LCP_OPT_MAGIC:
			/* Magic number -- can't use it, use 0 */
			sp->lcp.opts &= ~(1 << LCP_OPT_MAGIC);
			sp->lcp.magic = 0;
			break;
		case LCP_OPT_MRU:
			/*
			 * We try to negotiate a lower MRU if the underlying
			 * link's MTU is less than PP_MTU (e.g. PPPoE). If the
			 * peer rejects this lower rate, fallback to the
			 * default.
			 */
			if (debug) {
				addlog("%s: warning: peer rejected our MRU of "
				    "%ld bytes. Defaulting to %d bytes\n",
				    ifp->if_xname, sp->lcp.mru, PP_MTU);
			}
			sp->lcp.opts &= ~(1 << LCP_OPT_MRU);
			sp->lcp.mru = PP_MTU;
			break;
		case LCP_OPT_AUTH_PROTO:
			/*
			 * Peer doesn't want to authenticate himself,
			 * deny unless this is a dialout call, and
			 * SPPP_AUTHFLAG_NOCALLOUT is set.
			 */
			if ((sp->pp_flags & PP_CALLIN) == 0 &&
			    (sp->hisauth.flags & SPPP_AUTHFLAG_NOCALLOUT) != 0) {
				if (debug)
					addlog(" [don't insist on auth "
					       "for callout]");
				sp->lcp.opts &= ~(1 << LCP_OPT_AUTH_PROTO);
				break;
			}
			if (debug)
				addlog("[access denied]\n");
			lcp.Close(sp);
			break;
		}
	}
	if (debug)
		addlog("\n");
drop:
	free(buf, M_TEMP);
	return;
}

/*
 * Analyze the LCP Configure-NAK option list, and adjust our
 * negotiation.
 */
static void
sppp_lcp_RCN_nak(struct sppp *sp, struct lcp_header *h, int len)
{
	STDDCL;
	u_char *buf, *p;
	uint32_t magic;

	len -= 4;
	buf = malloc (len, M_TEMP, M_NOWAIT);
	if (!buf)
		return;

	if (debug)
		log(LOG_DEBUG, "%s: lcp nak opts:",
		    ifp->if_xname);

	p = (void *)(h + 1);
	for (; len > 1 && p[1]; len -= p[1], p += p[1]) {
		/* Sanity check option length */
		if (p[1] > len) {
			/*
			 * Malicious option - drop immediately.
			 * XXX Maybe we should just RXJ it?
			 */
			addlog("%s: received malicious LCP option, "
			    "dropping.\n", ifp->if_xname);
			goto drop;
		}
		if (debug)
			addlog(" %s", sppp_lcp_opt_name(*p));
		switch (*p) {
		case LCP_OPT_MAGIC:
			/* Magic number -- renegotiate */
			if ((sp->lcp.opts & (1 << LCP_OPT_MAGIC)) &&
			    len >= 6 && p[1] == 6) {
				magic = (uint32_t)p[2] << 24 |
					(uint32_t)p[3] << 16 | p[4] << 8 | p[5];
				/*
				 * If the remote magic is our negated one,
				 * this looks like a loopback problem.
				 * Suggest a new magic to make sure.
				 */
				if (magic == ~sp->lcp.magic) {
					if (debug)
						addlog(" magic glitch");
					sp->lcp.magic = cprng_fast32();
				} else {
					sp->lcp.magic = magic;
					if (debug)
						addlog(" %d", magic);
				}
			}
			break;
		case LCP_OPT_MRU:
			/*
			 * Peer wants to advise us to negotiate an MRU.
			 * Agree on it if it's reasonable, or use
			 * default otherwise.
			 */
			if (len >= 4 && p[1] == 4) {
				u_int mru = p[2] * 256 + p[3];
				if (debug)
					addlog(" %d", mru);
				if (mru < PPP_MINMRU || mru > sp->pp_if.if_mtu)
					mru = sp->pp_if.if_mtu;
				sp->lcp.mru = mru;
				sp->lcp.opts |= (1 << LCP_OPT_MRU);
			}
			break;
		case LCP_OPT_AUTH_PROTO:
			/*
			 * Peer doesn't like our authentication method,
			 * deny.
			 */
			if (debug)
				addlog("[access denied]\n");
			lcp.Close(sp);
			break;
		}
	}
	if (debug)
		addlog("\n");
drop:
	free(buf, M_TEMP);
	return;
}

static void
sppp_lcp_tlu(struct sppp *sp)
{
	STDDCL;
	int i;
	uint32_t mask;

	/* XXX ? */
	if (! (ifp->if_flags & IFF_UP) &&
	    (ifp->if_flags & IFF_RUNNING)) {
		/* Coming out of loopback mode. */
		if_up(ifp);
	}

	for (i = 0; i < IDX_COUNT; i++)
		if ((cps[i])->flags & CP_QUAL)
			(cps[i])->Open(sp);

	if ((sp->lcp.opts & (1 << LCP_OPT_AUTH_PROTO)) != 0 ||
	    (sp->pp_flags & PP_NEEDAUTH) != 0)
		sp->pp_phase = SPPP_PHASE_AUTHENTICATE;
	else
		sp->pp_phase = SPPP_PHASE_NETWORK;

	if (debug)
	{
		log(LOG_INFO, "%s: phase %s\n", ifp->if_xname,
		    sppp_phase_name(sp->pp_phase));
	}

	/*
	 * Open all authentication protocols.  This is even required
	 * if we already proceeded to network phase, since it might be
	 * that remote wants us to authenticate, so we might have to
	 * send a PAP request.  Undesired authentication protocols
	 * don't do anything when they get an Open event.
	 */
	for (i = 0; i < IDX_COUNT; i++)
		if ((cps[i])->flags & CP_AUTH)
			(cps[i])->Open(sp);

	if (sp->pp_phase == SPPP_PHASE_NETWORK) {
		/* Notify all NCPs. */
		for (i = 0; i < IDX_COUNT; i++)
			if ((cps[i])->flags & CP_NCP)
				(cps[i])->Open(sp);
	}

	/* Send Up events to all started protos. */
	for (i = 0, mask = 1; i < IDX_COUNT; i++, mask <<= 1)
		if ((sp->lcp.protos & mask) && ((cps[i])->flags & CP_LCP) == 0)
			(cps[i])->Up(sp);

	/* notify low-level driver of state change */
	if (sp->pp_chg)
		sp->pp_chg(sp, (int)sp->pp_phase);

	if (sp->pp_phase == SPPP_PHASE_NETWORK)
		/* if no NCP is starting, close down */
		sppp_lcp_check_and_close(sp);
}

static void
sppp_lcp_tld(struct sppp *sp)
{
	STDDCL;
	int i;
	uint32_t mask;

	sp->pp_phase = SPPP_PHASE_TERMINATE;

	if (debug)
	{
		log(LOG_INFO, "%s: phase %s\n", ifp->if_xname,
			sppp_phase_name(sp->pp_phase));
	}

	/*
	 * Take upper layers down.  We send the Down event first and
	 * the Close second to prevent the upper layers from sending
	 * ``a flurry of terminate-request packets'', as the RFC
	 * describes it.
	 */
	for (i = 0, mask = 1; i < IDX_COUNT; i++, mask <<= 1)
		if ((sp->lcp.protos & mask) && ((cps[i])->flags & CP_LCP) == 0) {
			(cps[i])->Down(sp);
			(cps[i])->Close(sp);
		}
}

static void
sppp_lcp_tls(struct sppp *sp)
{
	STDDCL;

	if (sp->pp_max_auth_fail != 0 && sp->pp_auth_failures >= sp->pp_max_auth_fail) {
	    printf("%s: authentication failed %d times, not retrying again\n",
		sp->pp_if.if_xname, sp->pp_auth_failures);
	    if_down(&sp->pp_if);
	    return;
	}

	sp->pp_phase = SPPP_PHASE_ESTABLISH;

	if (debug)
	{
		log(LOG_INFO, "%s: phase %s\n", ifp->if_xname,
			sppp_phase_name(sp->pp_phase));
	}

	/* Notify lower layer if desired. */
	if (sp->pp_tls)
		(sp->pp_tls)(sp);
}

static void
sppp_lcp_tlf(struct sppp *sp)
{
	STDDCL;

	sp->pp_phase = SPPP_PHASE_DEAD;

	if (debug)
	{
		log(LOG_INFO, "%s: phase %s\n", ifp->if_xname,
			sppp_phase_name(sp->pp_phase));
	}

	/* Notify lower layer if desired. */
	if (sp->pp_tlf)
		(sp->pp_tlf)(sp);
}

static void
sppp_lcp_scr(struct sppp *sp)
{
	char opt[6 /* magicnum */ + 4 /* mru */ + 5 /* chap */];
	int i = 0;
	u_short authproto;

	if (sp->lcp.opts & (1 << LCP_OPT_MAGIC)) {
		if (! sp->lcp.magic)
			sp->lcp.magic = cprng_fast32();
		opt[i++] = LCP_OPT_MAGIC;
		opt[i++] = 6;
		opt[i++] = sp->lcp.magic >> 24;
		opt[i++] = sp->lcp.magic >> 16;
		opt[i++] = sp->lcp.magic >> 8;
		opt[i++] = sp->lcp.magic;
	}

	if (sp->lcp.opts & (1 << LCP_OPT_MRU)) {
		opt[i++] = LCP_OPT_MRU;
		opt[i++] = 4;
		opt[i++] = sp->lcp.mru >> 8;
		opt[i++] = sp->lcp.mru;
	}

	if (sp->lcp.opts & (1 << LCP_OPT_AUTH_PROTO)) {
		authproto = sp->hisauth.proto;
		opt[i++] = LCP_OPT_AUTH_PROTO;
		opt[i++] = authproto == PPP_CHAP? 5: 4;
		opt[i++] = authproto >> 8;
		opt[i++] = authproto;
		if (authproto == PPP_CHAP)
			opt[i++] = CHAP_MD5;
	}

	sp->confid[IDX_LCP] = ++sp->pp_seq[IDX_LCP];
	sppp_cp_send(sp, PPP_LCP, CONF_REQ, sp->confid[IDX_LCP], i, &opt);
}

/*
 * Check the open NCPs, return true if at least one NCP is open.
 */
static int
sppp_ncp_check(struct sppp *sp)
{
	int i, mask;

	for (i = 0, mask = 1; i < IDX_COUNT; i++, mask <<= 1)
		if ((sp->lcp.protos & mask) && (cps[i])->flags & CP_NCP)
			return 1;
	return 0;
}

/*
 * Re-check the open NCPs and see if we should terminate the link.
 * Called by the NCPs during their tlf action handling.
 */
static void
sppp_lcp_check_and_close(struct sppp *sp)
{

	if (sp->pp_phase < SPPP_PHASE_NETWORK)
		/* don't bother, we are already going down */
		return;

	if (sppp_ncp_check(sp))
		return;

	lcp.Close(sp);
}


/*
 *--------------------------------------------------------------------------*
 *                                                                          *
 *                        The IPCP implementation.                          *
 *                                                                          *
 *--------------------------------------------------------------------------*
 */

static void
sppp_ipcp_init(struct sppp *sp)
{
	sp->ipcp.opts = 0;
	sp->ipcp.flags = 0;
	sp->state[IDX_IPCP] = STATE_INITIAL;
	sp->fail_counter[IDX_IPCP] = 0;
	sp->pp_seq[IDX_IPCP] = 0;
	sp->pp_rseq[IDX_IPCP] = 0;
	callout_init(&sp->ch[IDX_IPCP], 0);
}

static void
sppp_ipcp_up(struct sppp *sp)
{
	sppp_up_event(&ipcp, sp);
}

static void
sppp_ipcp_down(struct sppp *sp)
{
	sppp_down_event(&ipcp, sp);
}

static void
sppp_ipcp_open(struct sppp *sp)
{
	STDDCL;
	uint32_t myaddr, hisaddr;

	sp->ipcp.flags &= ~(IPCP_HISADDR_SEEN|IPCP_MYADDR_SEEN|IPCP_MYADDR_DYN|IPCP_HISADDR_DYN);
	sp->ipcp.req_myaddr = 0;
	sp->ipcp.req_hisaddr = 0;
	memset(&sp->dns_addrs, 0, sizeof sp->dns_addrs);

#ifdef INET
	sppp_get_ip_addrs(sp, &myaddr, &hisaddr, 0);
#else
	myaddr = hisaddr = 0;
#endif
	/*
	 * If we don't have his address, this probably means our
	 * interface doesn't want to talk IP at all.  (This could
	 * be the case if somebody wants to speak only IPX, for
	 * example.)  Don't open IPCP in this case.
	 */
	if (hisaddr == 0) {
		/* XXX this message should go away */
		if (debug)
			log(LOG_DEBUG, "%s: ipcp_open(): no IP interface\n",
			    ifp->if_xname);
		return;
	}

	if (myaddr == 0) {
		/*
		 * I don't have an assigned address, so i need to
		 * negotiate my address.
		 */
		sp->ipcp.flags |= IPCP_MYADDR_DYN;
		sp->ipcp.opts |= (1 << IPCP_OPT_ADDRESS);
	}
	if (hisaddr == 1) {
		/*
		 * XXX - remove this hack!
		 * remote has no valid address, we need to get one assigned.
		 */
		sp->ipcp.flags |= IPCP_HISADDR_DYN;
	}
	sppp_open_event(&ipcp, sp);
}

static void
sppp_ipcp_close(struct sppp *sp)
{
	STDDCL;

	sppp_close_event(&ipcp, sp);
#ifdef INET
	if (sp->ipcp.flags & (IPCP_MYADDR_DYN|IPCP_HISADDR_DYN))
		/*
		 * Some address was dynamic, clear it again.
		 */
		sppp_clear_ip_addrs(sp);
#endif

	if (sp->pp_saved_mtu > 0) {
		ifp->if_mtu = sp->pp_saved_mtu;
		sp->pp_saved_mtu = 0;
		if (debug)
			log(LOG_DEBUG,
			    "%s: resetting MTU to %" PRIu64 " bytes\n",
			    ifp->if_xname, ifp->if_mtu);
	}
}

static void
sppp_ipcp_TO(void *cookie)
{
	sppp_to_event(&ipcp, (struct sppp *)cookie);
}

/*
 * Analyze a configure request.  Return true if it was agreeable, and
 * caused action sca, false if it has been rejected or nak'ed, and
 * caused action scn.  (The return value is used to make the state
 * transition decision in the state automaton.)
 */
static int
sppp_ipcp_RCR(struct sppp *sp, struct lcp_header *h, int len)
{
	u_char *buf, *r, *p;
	struct ifnet *ifp = &sp->pp_if;
	int rlen, origlen, debug = ifp->if_flags & IFF_DEBUG;
	uint32_t hisaddr, desiredaddr;

	len -= 4;
	origlen = len;
	/*
	 * Make sure to allocate a buf that can at least hold a
	 * conf-nak with an `address' option.  We might need it below.
	 */
	buf = r = malloc ((len < 6? 6: len), M_TEMP, M_NOWAIT);
	if (! buf)
		return (0);

	/* pass 1: see if we can recognize them */
	if (debug)
		log(LOG_DEBUG, "%s: ipcp parse opts:",
		    ifp->if_xname);
	p = (void *)(h + 1);
	for (rlen=0; len>1 && p[1]; len-=p[1], p+=p[1]) {
		/* Sanity check option length */
		if (p[1] > len) {
			/* XXX should we just RXJ? */
			addlog("%s: malicious IPCP option received, dropping\n",
			    ifp->if_xname);
			goto drop;
		}
		if (debug)
			addlog(" %s", sppp_ipcp_opt_name(*p));
		switch (*p) {
#ifdef notyet
		case IPCP_OPT_COMPRESSION:
			if (len >= 6 && p[1] >= 6) {
				/* correctly formed compress option */
				continue;
			}
			if (debug)
				addlog(" [invalid]");
			break;
#endif
		case IPCP_OPT_ADDRESS:
			if (len >= 6 && p[1] == 6) {
				/* correctly formed address option */
				continue;
			}
			if (debug)
				addlog(" [invalid]");
			break;
		default:
			/* Others not supported. */
			if (debug)
				addlog(" [rej]");
			break;
		}
		/* Add the option to rejected list. */
		bcopy (p, r, p[1]);
		r += p[1];
		rlen += p[1];
	}
	if (rlen) {
		if (debug)
			addlog(" send conf-rej\n");
		sppp_cp_send(sp, PPP_IPCP, CONF_REJ, h->ident, rlen, buf);
		goto end;
	} else if (debug)
		addlog("\n");

	/* pass 2: parse option values */
	if (sp->ipcp.flags & IPCP_HISADDR_SEEN)
		hisaddr = sp->ipcp.req_hisaddr;	/* we already aggreed on that */
	else
#ifdef INET
		sppp_get_ip_addrs(sp, 0, &hisaddr, 0);	/* user configuration */
#else
		hisaddr = 0;
#endif
	if (debug)
		log(LOG_DEBUG, "%s: ipcp parse opt values: ",
		       ifp->if_xname);
	p = (void *)(h + 1);
	len = origlen;
	for (rlen=0; len>1 && p[1]; len-=p[1], p+=p[1]) {
		if (debug)
			addlog(" %s", sppp_ipcp_opt_name(*p));
		switch (*p) {
#ifdef notyet
		case IPCP_OPT_COMPRESSION:
			continue;
#endif
		case IPCP_OPT_ADDRESS:
			desiredaddr = p[2] << 24 | p[3] << 16 |
				p[4] << 8 | p[5];
			if (desiredaddr == hisaddr ||
		    	   ((sp->ipcp.flags & IPCP_HISADDR_DYN) && desiredaddr != 0)) {
				/*
			 	* Peer's address is same as our value,
			 	* this is agreeable.  Gonna conf-ack
			 	* it.
			 	*/
				if (debug)
					addlog(" %s [ack]",
				       		sppp_dotted_quad(hisaddr));
				/* record that we've seen it already */
				sp->ipcp.flags |= IPCP_HISADDR_SEEN;
				sp->ipcp.req_hisaddr = desiredaddr;
				hisaddr = desiredaddr;
				continue;
			}
			/*
		 	* The address wasn't agreeable.  This is either
		 	* he sent us 0.0.0.0, asking to assign him an
		 	* address, or he send us another address not
		 	* matching our value.  Either case, we gonna
		 	* conf-nak it with our value.
		 	*/
			if (debug) {
				if (desiredaddr == 0)
					addlog(" [addr requested]");
				else
					addlog(" %s [not agreed]",
				       		sppp_dotted_quad(desiredaddr));
			}

			p[2] = hisaddr >> 24;
			p[3] = hisaddr >> 16;
			p[4] = hisaddr >> 8;
			p[5] = hisaddr;
			break;
		}
		/* Add the option to nak'ed list. */
		bcopy (p, r, p[1]);
		r += p[1];
		rlen += p[1];
	}

	/*
	 * If we are about to conf-ack the request, but haven't seen
	 * his address so far, gonna conf-nak it instead, with the
	 * `address' option present and our idea of his address being
	 * filled in there, to request negotiation of both addresses.
	 *
	 * XXX This can result in an endless req - nak loop if peer
	 * doesn't want to send us his address.  Q: What should we do
	 * about it?  XXX  A: implement the max-failure counter.
	 */
	if (rlen == 0 && !(sp->ipcp.flags & IPCP_HISADDR_SEEN)) {
		buf[0] = IPCP_OPT_ADDRESS;
		buf[1] = 6;
		buf[2] = hisaddr >> 24;
		buf[3] = hisaddr >> 16;
		buf[4] = hisaddr >> 8;
		buf[5] = hisaddr;
		rlen = 6;
		if (debug)
			addlog(" still need hisaddr");
	}

	if (rlen) {
		if (debug)
			addlog(" send conf-nak\n");
		sppp_cp_send(sp, PPP_IPCP, CONF_NAK, h->ident, rlen, buf);
	} else {
		if (debug)
			addlog(" send conf-ack\n");
		sppp_cp_send(sp, PPP_IPCP, CONF_ACK, h->ident, origlen, h + 1);
	}

 end:
	free(buf, M_TEMP);
	return (rlen == 0);

 drop:
	free(buf, M_TEMP);
	return -1;
}

/*
 * Analyze the IPCP Configure-Reject option list, and adjust our
 * negotiation.
 */
static void
sppp_ipcp_RCN_rej(struct sppp *sp, struct lcp_header *h, int len)
{
	u_char *buf, *p;
	struct ifnet *ifp = &sp->pp_if;
	int debug = ifp->if_flags & IFF_DEBUG;

	len -= 4;
	buf = malloc (len, M_TEMP, M_NOWAIT);
	if (!buf)
		return;

	if (debug)
		log(LOG_DEBUG, "%s: ipcp rej opts:",
		    ifp->if_xname);

	p = (void *)(h + 1);
	for (; len > 1 && p[1]; len -= p[1], p += p[1]) {
		/* Sanity check option length */
		if (p[1] > len) {
			/* XXX should we just RXJ? */
			addlog("%s: malicious IPCP option received, dropping\n",
			    ifp->if_xname);
			goto drop;
		}
		if (debug)
			addlog(" %s", sppp_ipcp_opt_name(*p));
		switch (*p) {
		case IPCP_OPT_ADDRESS:
			/*
			 * Peer doesn't grok address option.  This is
			 * bad.  XXX  Should we better give up here?
			 */
			sp->ipcp.opts &= ~(1 << IPCP_OPT_ADDRESS);
			break;
#ifdef notyet
		case IPCP_OPT_COMPRESS:
			sp->ipcp.opts &= ~(1 << IPCP_OPT_COMPRESS);
			break;
#endif
		}
	}
	if (debug)
		addlog("\n");
drop:
	free(buf, M_TEMP);
	return;
}

/*
 * Analyze the IPCP Configure-NAK option list, and adjust our
 * negotiation.
 */
static void
sppp_ipcp_RCN_nak(struct sppp *sp, struct lcp_header *h, int len)
{
	u_char *p;
	struct ifnet *ifp = &sp->pp_if;
	int debug = ifp->if_flags & IFF_DEBUG;
	uint32_t wantaddr;

	len -= 4;

	if (debug)
		log(LOG_DEBUG, "%s: ipcp nak opts:",
		    ifp->if_xname);

	p = (void *)(h + 1);
	for (; len > 1 && p[1]; len -= p[1], p += p[1]) {
		/* Sanity check option length */
		if (p[1] > len) {
			/* XXX should we just RXJ? */
			addlog("%s: malicious IPCP option received, dropping\n",
			    ifp->if_xname);
			return;
		}
		if (debug)
			addlog(" %s", sppp_ipcp_opt_name(*p));
		switch (*p) {
		case IPCP_OPT_ADDRESS:
			/*
			 * Peer doesn't like our local IP address.  See
			 * if we can do something for him.  We'll drop
			 * him our address then.
			 */
			if (len >= 6 && p[1] == 6) {
				wantaddr = p[2] << 24 | p[3] << 16 |
					p[4] << 8 | p[5];
				sp->ipcp.opts |= (1 << IPCP_OPT_ADDRESS);
				if (debug)
					addlog(" [wantaddr %s]",
					       sppp_dotted_quad(wantaddr));
				/*
				 * When doing dynamic address assignment,
				 * we accept his offer.  Otherwise, we
				 * ignore it and thus continue to negotiate
				 * our already existing value.
				 */
				if (sp->ipcp.flags & IPCP_MYADDR_DYN) {
					if (debug)
						addlog(" [agree]");
					sp->ipcp.flags |= IPCP_MYADDR_SEEN;
					sp->ipcp.req_myaddr = wantaddr;
				}
			}
			break;

		case IPCP_OPT_PRIMDNS:
			if (len >= 6 && p[1] == 6) {
				sp->dns_addrs[0] = p[2] << 24 | p[3] << 16 |
					p[4] << 8 | p[5];
			}
			break;

		case IPCP_OPT_SECDNS:
			if (len >= 6 && p[1] == 6) {
				sp->dns_addrs[1] = p[2] << 24 | p[3] << 16 |
					p[4] << 8 | p[5];
			}
			break;
#ifdef notyet
		case IPCP_OPT_COMPRESS:
			/*
			 * Peer wants different compression parameters.
			 */
			break;
#endif
		}
	}
	if (debug)
		addlog("\n");
}

static void
sppp_ipcp_tlu(struct sppp *sp)
{
#ifdef INET
	/* we are up. Set addresses and notify anyone interested */
	STDDCL;
	uint32_t myaddr, hisaddr;

	sppp_get_ip_addrs(sp, &myaddr, &hisaddr, 0);
	if ((sp->ipcp.flags & IPCP_MYADDR_DYN) && (sp->ipcp.flags & IPCP_MYADDR_SEEN))
		myaddr = sp->ipcp.req_myaddr;
	if ((sp->ipcp.flags & IPCP_HISADDR_DYN) && (sp->ipcp.flags & IPCP_HISADDR_SEEN))
		hisaddr = sp->ipcp.req_hisaddr;
	sppp_set_ip_addrs(sp, myaddr, hisaddr);

	if (ifp->if_mtu > sp->lcp.their_mru) {
		sp->pp_saved_mtu = ifp->if_mtu;
		ifp->if_mtu = sp->lcp.their_mru;
		if (debug)
			log(LOG_DEBUG,
			    "%s: setting MTU to %" PRIu64 " bytes\n",
			    ifp->if_xname, ifp->if_mtu);
	}

	if (sp->pp_con)
		sp->pp_con(sp);
#endif
}

static void
sppp_ipcp_tld(struct sppp *sp)
{
}

static void
sppp_ipcp_tls(struct sppp *sp)
{
	/* indicate to LCP that it must stay alive */
	sp->lcp.protos |= (1 << IDX_IPCP);
}

static void
sppp_ipcp_tlf(struct sppp *sp)
{
	/* we no longer need LCP */
	sp->lcp.protos &= ~(1 << IDX_IPCP);
}

static void
sppp_ipcp_scr(struct sppp *sp)
{
	char opt[6 /* compression */ + 6 /* address */ + 12 /* dns addresses */];
#ifdef INET
	uint32_t ouraddr;
#endif
	int i = 0;

#ifdef notyet
	if (sp->ipcp.opts & (1 << IPCP_OPT_COMPRESSION)) {
		opt[i++] = IPCP_OPT_COMPRESSION;
		opt[i++] = 6;
		opt[i++] = 0;	/* VJ header compression */
		opt[i++] = 0x2d; /* VJ header compression */
		opt[i++] = max_slot_id;
		opt[i++] = comp_slot_id;
	}
#endif

#ifdef INET
	if (sp->ipcp.opts & (1 << IPCP_OPT_ADDRESS)) {
		if (sp->ipcp.flags & IPCP_MYADDR_SEEN)
			ouraddr = sp->ipcp.req_myaddr;	/* not sure if this can ever happen */
		else
			sppp_get_ip_addrs(sp, &ouraddr, 0, 0);
		opt[i++] = IPCP_OPT_ADDRESS;
		opt[i++] = 6;
		opt[i++] = ouraddr >> 24;
		opt[i++] = ouraddr >> 16;
		opt[i++] = ouraddr >> 8;
		opt[i++] = ouraddr;
	}
#endif

	if (sp->query_dns & 1) {
		opt[i++] = IPCP_OPT_PRIMDNS;
		opt[i++] = 6;
		opt[i++] = sp->dns_addrs[0] >> 24;
		opt[i++] = sp->dns_addrs[0] >> 16;
		opt[i++] = sp->dns_addrs[0] >> 8;
		opt[i++] = sp->dns_addrs[0];
	}
	if (sp->query_dns & 2) {
		opt[i++] = IPCP_OPT_SECDNS;
		opt[i++] = 6;
		opt[i++] = sp->dns_addrs[1] >> 24;
		opt[i++] = sp->dns_addrs[1] >> 16;
		opt[i++] = sp->dns_addrs[1] >> 8;
		opt[i++] = sp->dns_addrs[1];
	}

	sp->confid[IDX_IPCP] = ++sp->pp_seq[IDX_IPCP];
	sppp_cp_send(sp, PPP_IPCP, CONF_REQ, sp->confid[IDX_IPCP], i, &opt);
}


/*
 *--------------------------------------------------------------------------*
 *                                                                          *
 *                      The IPv6CP implementation.                          *
 *                                                                          *
 *--------------------------------------------------------------------------*
 */

#ifdef INET6
static void
sppp_ipv6cp_init(struct sppp *sp)
{
	sp->ipv6cp.opts = 0;
	sp->ipv6cp.flags = 0;
	sp->state[IDX_IPV6CP] = STATE_INITIAL;
	sp->fail_counter[IDX_IPV6CP] = 0;
	sp->pp_seq[IDX_IPV6CP] = 0;
	sp->pp_rseq[IDX_IPV6CP] = 0;
	callout_init(&sp->ch[IDX_IPV6CP], 0);
}

static void
sppp_ipv6cp_up(struct sppp *sp)
{
	sppp_up_event(&ipv6cp, sp);
}

static void
sppp_ipv6cp_down(struct sppp *sp)
{
	sppp_down_event(&ipv6cp, sp);
}

static void
sppp_ipv6cp_open(struct sppp *sp)
{
	STDDCL;
	struct in6_addr myaddr, hisaddr;

#ifdef IPV6CP_MYIFID_DYN
	sp->ipv6cp.flags &= ~(IPV6CP_MYIFID_SEEN|IPV6CP_MYIFID_DYN);
#else
	sp->ipv6cp.flags &= ~IPV6CP_MYIFID_SEEN;
#endif

	sppp_get_ip6_addrs(sp, &myaddr, &hisaddr, 0);
	/*
	 * If we don't have our address, this probably means our
	 * interface doesn't want to talk IPv6 at all.  (This could
	 * be the case if somebody wants to speak only IPX, for
	 * example.)  Don't open IPv6CP in this case.
	 */
	if (IN6_IS_ADDR_UNSPECIFIED(&myaddr)) {
		/* XXX this message should go away */
		if (debug)
			log(LOG_DEBUG, "%s: ipv6cp_open(): no IPv6 interface\n",
			    ifp->if_xname);
		return;
	}

	sp->ipv6cp.flags |= IPV6CP_MYIFID_SEEN;
	sp->ipv6cp.opts |= (1 << IPV6CP_OPT_IFID);
	sppp_open_event(&ipv6cp, sp);
}

static void
sppp_ipv6cp_close(struct sppp *sp)
{
	sppp_close_event(&ipv6cp, sp);
}

static void
sppp_ipv6cp_TO(void *cookie)
{
	sppp_to_event(&ipv6cp, (struct sppp *)cookie);
}

/*
 * Analyze a configure request.  Return true if it was agreeable, and
 * caused action sca, false if it has been rejected or nak'ed, and
 * caused action scn.  (The return value is used to make the state
 * transition decision in the state automaton.)
 */
static int
sppp_ipv6cp_RCR(struct sppp *sp, struct lcp_header *h, int len)
{
	u_char *buf, *r, *p;
	struct ifnet *ifp = &sp->pp_if;
	int rlen, origlen, debug = ifp->if_flags & IFF_DEBUG;
	struct in6_addr myaddr, desiredaddr, suggestaddr;
	int ifidcount;
	int type;
	int collision, nohisaddr;

	len -= 4;
	origlen = len;
	/*
	 * Make sure to allocate a buf that can at least hold a
	 * conf-nak with an `address' option.  We might need it below.
	 */
	buf = r = malloc ((len < 6? 6: len), M_TEMP, M_NOWAIT);
	if (! buf)
		return (0);

	/* pass 1: see if we can recognize them */
	if (debug)
		log(LOG_DEBUG, "%s: ipv6cp parse opts:",
		    ifp->if_xname);
	p = (void *)(h + 1);
	ifidcount = 0;
	for (rlen=0; len>1 && p[1]; len-=p[1], p+=p[1]) {
		/* Sanity check option length */
		if (p[1] > len) {
			/* XXX just RXJ? */
			addlog("%s: received malicious IPCPv6 option, "
			    "dropping\n", ifp->if_xname);
			goto drop;
		}
		if (debug)
			addlog(" %s", sppp_ipv6cp_opt_name(*p));
		switch (*p) {
		case IPV6CP_OPT_IFID:
			if (len >= 10 && p[1] == 10 && ifidcount == 0) {
				/* correctly formed address option */
				ifidcount++;
				continue;
			}
			if (debug)
				addlog(" [invalid]");
			break;
#ifdef notyet
		case IPV6CP_OPT_COMPRESSION:
			if (len >= 4 && p[1] >= 4) {
				/* correctly formed compress option */
				continue;
			}
			if (debug)
				addlog(" [invalid]");
			break;
#endif
		default:
			/* Others not supported. */
			if (debug)
				addlog(" [rej]");
			break;
		}
		/* Add the option to rejected list. */
		bcopy (p, r, p[1]);
		r += p[1];
		rlen += p[1];
	}
	if (rlen) {
		if (debug)
			addlog(" send conf-rej\n");
		sppp_cp_send(sp, PPP_IPV6CP, CONF_REJ, h->ident, rlen, buf);
		goto end;
	} else if (debug)
		addlog("\n");

	/* pass 2: parse option values */
	sppp_get_ip6_addrs(sp, &myaddr, 0, 0);
	if (debug)
		log(LOG_DEBUG, "%s: ipv6cp parse opt values: ",
		       ifp->if_xname);
	p = (void *)(h + 1);
	len = origlen;
	type = CONF_ACK;
	for (rlen=0; len>1 && p[1]; len-=p[1], p+=p[1]) {
		if (debug)
			addlog(" %s", sppp_ipv6cp_opt_name(*p));
		switch (*p) {
#ifdef notyet
		case IPV6CP_OPT_COMPRESSION:
			continue;
#endif
		case IPV6CP_OPT_IFID:
			memset(&desiredaddr, 0, sizeof(desiredaddr));
			memcpy(&desiredaddr.s6_addr[8], &p[2], 8);
			collision = (memcmp(&desiredaddr.s6_addr[8],
					&myaddr.s6_addr[8], 8) == 0);
			nohisaddr = IN6_IS_ADDR_UNSPECIFIED(&desiredaddr);

			desiredaddr.s6_addr16[0] = htons(0xfe80);
			(void)in6_setscope(&desiredaddr, &sp->pp_if, NULL);

			if (!collision && !nohisaddr) {
				/* no collision, hisaddr known - Conf-Ack */
				type = CONF_ACK;

				if (debug) {
					addlog(" %s [%s]",
					    ip6_sprintf(&desiredaddr),
					    sppp_cp_type_name(type));
				}
				continue;
			}

			memset(&suggestaddr, 0, sizeof(suggestaddr));
			if (collision && nohisaddr) {
				/* collision, hisaddr unknown - Conf-Rej */
				type = CONF_REJ;
				memset(&p[2], 0, 8);
			} else {
				/*
				 * - no collision, hisaddr unknown, or
				 * - collision, hisaddr known
				 * Conf-Nak, suggest hisaddr
				 */
				type = CONF_NAK;
				sppp_suggest_ip6_addr(sp, &suggestaddr);
				memcpy(&p[2], &suggestaddr.s6_addr[8], 8);
			}
			if (debug)
				addlog(" %s [%s]", ip6_sprintf(&desiredaddr),
				    sppp_cp_type_name(type));
			break;
		}
		/* Add the option to nak'ed list. */
		bcopy (p, r, p[1]);
		r += p[1];
		rlen += p[1];
	}

	if (rlen == 0 && type == CONF_ACK) {
		if (debug)
			addlog(" send %s\n", sppp_cp_type_name(type));
		sppp_cp_send(sp, PPP_IPV6CP, type, h->ident, origlen, h + 1);
	} else {
#ifdef notdef
		if (type == CONF_ACK)
			panic("IPv6CP RCR: CONF_ACK with non-zero rlen");
#endif

		if (debug) {
			addlog(" send %s suggest %s\n",
			    sppp_cp_type_name(type), ip6_sprintf(&suggestaddr));
		}
		sppp_cp_send(sp, PPP_IPV6CP, type, h->ident, rlen, buf);
	}

 end:
	free(buf, M_TEMP);
	return (rlen == 0);

 drop:
	free(buf, M_TEMP);
	return -1;
}

/*
 * Analyze the IPv6CP Configure-Reject option list, and adjust our
 * negotiation.
 */
static void
sppp_ipv6cp_RCN_rej(struct sppp *sp, struct lcp_header *h, int len)
{
	u_char *buf, *p;
	struct ifnet *ifp = &sp->pp_if;
	int debug = ifp->if_flags & IFF_DEBUG;

	len -= 4;
	buf = malloc (len, M_TEMP, M_NOWAIT);
	if (!buf)
		return;

	if (debug)
		log(LOG_DEBUG, "%s: ipv6cp rej opts:",
		    ifp->if_xname);

	p = (void *)(h + 1);
	for (; len > 1 && p[1]; len -= p[1], p += p[1]) {
		if (p[1] > len) {
			/* XXX just RXJ? */
			addlog("%s: received malicious IPCPv6 option, "
			    "dropping\n", ifp->if_xname);
			goto drop;
		}
		if (debug)
			addlog(" %s", sppp_ipv6cp_opt_name(*p));
		switch (*p) {
		case IPV6CP_OPT_IFID:
			/*
			 * Peer doesn't grok address option.  This is
			 * bad.  XXX  Should we better give up here?
			 */
			sp->ipv6cp.opts &= ~(1 << IPV6CP_OPT_IFID);
			break;
#ifdef notyet
		case IPV6CP_OPT_COMPRESS:
			sp->ipv6cp.opts &= ~(1 << IPV6CP_OPT_COMPRESS);
			break;
#endif
		}
	}
	if (debug)
		addlog("\n");
drop:
	free(buf, M_TEMP);
	return;
}

/*
 * Analyze the IPv6CP Configure-NAK option list, and adjust our
 * negotiation.
 */
static void
sppp_ipv6cp_RCN_nak(struct sppp *sp, struct lcp_header *h, int len)
{
	u_char *buf, *p;
	struct ifnet *ifp = &sp->pp_if;
	int debug = ifp->if_flags & IFF_DEBUG;
	struct in6_addr suggestaddr;

	len -= 4;
	buf = malloc (len, M_TEMP, M_NOWAIT);
	if (!buf)
		return;

	if (debug)
		log(LOG_DEBUG, "%s: ipv6cp nak opts:",
		    ifp->if_xname);

	p = (void *)(h + 1);
	for (; len > 1 && p[1]; len -= p[1], p += p[1]) {
		if (p[1] > len) {
			/* XXX just RXJ? */
			addlog("%s: received malicious IPCPv6 option, "
			    "dropping\n", ifp->if_xname);
			goto drop;
		}
		if (debug)
			addlog(" %s", sppp_ipv6cp_opt_name(*p));
		switch (*p) {
		case IPV6CP_OPT_IFID:
			/*
			 * Peer doesn't like our local ifid.  See
			 * if we can do something for him.  We'll drop
			 * him our address then.
			 */
			if (len < 10 || p[1] != 10)
				break;
			memset(&suggestaddr, 0, sizeof(suggestaddr));
			suggestaddr.s6_addr16[0] = htons(0xfe80);
			(void)in6_setscope(&suggestaddr, &sp->pp_if, NULL);
			memcpy(&suggestaddr.s6_addr[8], &p[2], 8);

			sp->ipv6cp.opts |= (1 << IPV6CP_OPT_IFID);
			if (debug)
				addlog(" [suggestaddr %s]",
				       ip6_sprintf(&suggestaddr));
#ifdef IPV6CP_MYIFID_DYN
			/*
			 * When doing dynamic address assignment,
			 * we accept his offer.
			 */
			if (sp->ipv6cp.flags & IPV6CP_MYIFID_DYN) {
				struct in6_addr lastsuggest;
				/*
				 * If <suggested myaddr from peer> equals to
				 * <hisaddr we have suggested last time>,
				 * we have a collision.  generate new random
				 * ifid.
				 */
				sppp_suggest_ip6_addr(&lastsuggest);
				if (IN6_ARE_ADDR_EQUAL(&suggestaddr,
						 lastsuggest)) {
					if (debug)
						addlog(" [random]");
					sppp_gen_ip6_addr(sp, &suggestaddr);
				}
				sppp_set_ip6_addr(sp, &suggestaddr, 0);
				if (debug)
					addlog(" [agree]");
				sp->ipv6cp.flags |= IPV6CP_MYIFID_SEEN;
			}
#else
			/*
			 * Since we do not do dynamic address assignment,
			 * we ignore it and thus continue to negotiate
			 * our already existing value.  This can possibly
			 * go into infinite request-reject loop.
			 *
			 * This is not likely because we normally use
			 * ifid based on MAC-address.
			 * If you have no ethernet card on the node, too bad.
			 * XXX should we use fail_counter?
			 */
#endif
			break;
#ifdef notyet
		case IPV6CP_OPT_COMPRESS:
			/*
			 * Peer wants different compression parameters.
			 */
			break;
#endif
		}
	}
	if (debug)
		addlog("\n");
drop:
	free(buf, M_TEMP);
	return;
}

static void
sppp_ipv6cp_tlu(struct sppp *sp)
{
	/* we are up - notify isdn daemon */
	if (sp->pp_con)
		sp->pp_con(sp);
}

static void
sppp_ipv6cp_tld(struct sppp *sp)
{
}

static void
sppp_ipv6cp_tls(struct sppp *sp)
{
	/* indicate to LCP that it must stay alive */
	sp->lcp.protos |= (1 << IDX_IPV6CP);
}

static void
sppp_ipv6cp_tlf(struct sppp *sp)
{
	/* we no longer need LCP */
	sp->lcp.protos &= ~(1 << IDX_IPV6CP);
}

static void
sppp_ipv6cp_scr(struct sppp *sp)
{
	char opt[10 /* ifid */ + 4 /* compression, minimum */];
	struct in6_addr ouraddr;
	int i = 0;

	if (sp->ipv6cp.opts & (1 << IPV6CP_OPT_IFID)) {
		sppp_get_ip6_addrs(sp, &ouraddr, 0, 0);
		opt[i++] = IPV6CP_OPT_IFID;
		opt[i++] = 10;
		memcpy(&opt[i], &ouraddr.s6_addr[8], 8);
		i += 8;
	}

#ifdef notyet
	if (sp->ipv6cp.opts & (1 << IPV6CP_OPT_COMPRESSION)) {
		opt[i++] = IPV6CP_OPT_COMPRESSION;
		opt[i++] = 4;
		opt[i++] = 0;	/* TBD */
		opt[i++] = 0;	/* TBD */
		/* variable length data may follow */
	}
#endif

	sp->confid[IDX_IPV6CP] = ++sp->pp_seq[IDX_IPV6CP];
	sppp_cp_send(sp, PPP_IPV6CP, CONF_REQ, sp->confid[IDX_IPV6CP], i, &opt);
}
#else /*INET6*/
static void
sppp_ipv6cp_init(struct sppp *sp)
{
}

static void
sppp_ipv6cp_up(struct sppp *sp)
{
}

static void
sppp_ipv6cp_down(struct sppp *sp)
{
}

static void
sppp_ipv6cp_open(struct sppp *sp)
{
}

static void
sppp_ipv6cp_close(struct sppp *sp)
{
}

static void
sppp_ipv6cp_TO(void *sp)
{
}

static int
sppp_ipv6cp_RCR(struct sppp *sp, struct lcp_header *h,
		int len)
{
	return 0;
}

static void
sppp_ipv6cp_RCN_rej(struct sppp *sp, struct lcp_header *h,
		    int len)
{
}

static void
sppp_ipv6cp_RCN_nak(struct sppp *sp, struct lcp_header *h,
		    int len)
{
}

static void
sppp_ipv6cp_tlu(struct sppp *sp)
{
}

static void
sppp_ipv6cp_tld(struct sppp *sp)
{
}

static void
sppp_ipv6cp_tls(struct sppp *sp)
{
}

static void
sppp_ipv6cp_tlf(struct sppp *sp)
{
}

static void
sppp_ipv6cp_scr(struct sppp *sp)
{
}
#endif /*INET6*/


/*
 *--------------------------------------------------------------------------*
 *                                                                          *
 *                        The CHAP implementation.                          *
 *                                                                          *
 *--------------------------------------------------------------------------*
 */

/*
 * The authentication protocols don't employ a full-fledged state machine as
 * the control protocols do, since they do have Open and Close events, but
 * not Up and Down, nor are they explicitly terminated.  Also, use of the
 * authentication protocols may be different in both directions (this makes
 * sense, think of a machine that never accepts incoming calls but only
 * calls out, it doesn't require the called party to authenticate itself).
 *
 * Our state machine for the local authentication protocol (we are requesting
 * the peer to authenticate) looks like:
 *
 *						    RCA-
 *	      +--------------------------------------------+
 *	      V					    scn,tld|
 *	  +--------+			       Close   +---------+ RCA+
 *	  |	   |<----------------------------------|	 |------+
 *   +--->| Closed |				TO*    | Opened	 | sca	|
 *   |	  |	   |-----+		       +-------|	 |<-----+
 *   |	  +--------+ irc |		       |       +---------+
 *   |	    ^		 |		       |	   ^
 *   |	    |		 |		       |	   |
 *   |	    |		 |		       |	   |
 *   |	 TO-|		 |		       |	   |
 *   |	    |tld  TO+	 V		       |	   |
 *   |	    |	+------->+		       |	   |
 *   |	    |	|	 |		       |	   |
 *   |	  +--------+	 V		       |	   |
 *   |	  |	   |<----+<--------------------+	   |
 *   |	  | Req-   | scr				   |
 *   |	  | Sent   |					   |
 *   |	  |	   |					   |
 *   |	  +--------+					   |
 *   | RCA- |	| RCA+					   |
 *   +------+	+------------------------------------------+
 *   scn,tld	  sca,irc,ict,tlu
 *
 *
 *   with:
 *
 *	Open:	LCP reached authentication phase
 *	Close:	LCP reached terminate phase
 *
 *	RCA+:	received reply (pap-req, chap-response), acceptable
 *	RCN:	received reply (pap-req, chap-response), not acceptable
 *	TO+:	timeout with restart counter >= 0
 *	TO-:	timeout with restart counter < 0
 *	TO*:	reschedule timeout for CHAP
 *
 *	scr:	send request packet (none for PAP, chap-challenge)
 *	sca:	send ack packet (pap-ack, chap-success)
 *	scn:	send nak packet (pap-nak, chap-failure)
 *	ict:	initialize re-challenge timer (CHAP only)
 *
 *	tlu:	this-layer-up, LCP reaches network phase
 *	tld:	this-layer-down, LCP enters terminate phase
 *
 * Note that in CHAP mode, after sending a new challenge, while the state
 * automaton falls back into Req-Sent state, it doesn't signal a tld
 * event to LCP, so LCP remains in network phase.  Only after not getting
 * any response (or after getting an unacceptable response), CHAP closes,
 * causing LCP to enter terminate phase.
 *
 * With PAP, there is no initial request that can be sent.  The peer is
 * expected to send one based on the successful negotiation of PAP as
 * the authentication protocol during the LCP option negotiation.
 *
 * Incoming authentication protocol requests (remote requests
 * authentication, we are peer) don't employ a state machine at all,
 * they are simply answered.  Some peers [Ascend P50 firmware rev
 * 4.50] react allergically when sending IPCP/IPv6CP requests while they are
 * still in authentication phase (thereby violating the standard that
 * demands that these NCP packets are to be discarded), so we keep
 * track of the peer demanding us to authenticate, and only proceed to
 * phase network once we've seen a positive acknowledge for the
 * authentication.
 */

/*
 * Handle incoming CHAP packets.
 */
void
sppp_chap_input(struct sppp *sp, struct mbuf *m)
{
	STDDCL;
	struct lcp_header *h;
	int len, x;
	u_char *value, *name, digest[sizeof(sp->myauth.challenge)], dsize;
	int value_len, name_len;
	MD5_CTX ctx;

	len = m->m_pkthdr.len;
	if (len < 4) {
		if (debug)
			log(LOG_DEBUG,
			    "%s: chap invalid packet length: %d bytes\n",
			    ifp->if_xname, len);
		return;
	}
	h = mtod(m, struct lcp_header *);
	if (len > ntohs(h->len))
		len = ntohs(h->len);

	switch (h->type) {
	/* challenge, failure and success are his authproto */
	case CHAP_CHALLENGE:
		if (sp->myauth.secret == NULL || sp->myauth.name == NULL) {
		    /* can't do anything useful */
		    sp->pp_auth_failures++;
		    printf("%s: chap input without my name and my secret being set\n",
		    	ifp->if_xname);
		    break;
		}
		value = 1 + (u_char *)(h + 1);
		value_len = value[-1];
		name = value + value_len;
		name_len = len - value_len - 5;
		if (name_len < 0) {
			if (debug) {
				log(LOG_DEBUG,
				    "%s: chap corrupted challenge "
				    "<%s id=0x%x len=%d",
				    ifp->if_xname,
				    sppp_auth_type_name(PPP_CHAP, h->type),
				    h->ident, ntohs(h->len));
				if (len > 4)
					sppp_print_bytes((u_char *)(h + 1),
					    len - 4);
				addlog(">\n");
			}
			break;
		}

		if (debug) {
			log(LOG_DEBUG,
			    "%s: chap input <%s id=0x%x len=%d name=",
			    ifp->if_xname,
			    sppp_auth_type_name(PPP_CHAP, h->type), h->ident,
			    ntohs(h->len));
			sppp_print_string((char *) name, name_len);
			addlog(" value-size=%d value=", value_len);
			sppp_print_bytes(value, value_len);
			addlog(">\n");
		}

		/* Compute reply value. */
		MD5Init(&ctx);
		MD5Update(&ctx, &h->ident, 1);
		MD5Update(&ctx, sp->myauth.secret, sp->myauth.secret_len);
		MD5Update(&ctx, value, value_len);
		MD5Final(digest, &ctx);
		dsize = sizeof digest;

		sppp_auth_send(&chap, sp, CHAP_RESPONSE, h->ident,
			       sizeof dsize, (const char *)&dsize,
			       sizeof digest, digest,
			       sp->myauth.name_len,
			       sp->myauth.name,
			       0);
		break;

	case CHAP_SUCCESS:
		if (debug) {
			log(LOG_DEBUG, "%s: chap success",
			    ifp->if_xname);
			if (len > 4) {
				addlog(": ");
				sppp_print_string((char *)(h + 1), len - 4);
			}
			addlog("\n");
		}
		x = splnet();
		sp->pp_auth_failures = 0;
		sp->pp_flags &= ~PP_NEEDAUTH;
		if (sp->myauth.proto == PPP_CHAP &&
		    (sp->lcp.opts & (1 << LCP_OPT_AUTH_PROTO)) &&
		    (sp->lcp.protos & (1 << IDX_CHAP)) == 0) {
			/*
			 * We are authenticator for CHAP but didn't
			 * complete yet.  Leave it to tlu to proceed
			 * to network phase.
			 */
			splx(x);
			break;
		}
		splx(x);
		sppp_phase_network(sp);
		break;

	case CHAP_FAILURE:
		x = splnet();
		sp->pp_auth_failures++;
		splx(x);
		if (debug) {
			log(LOG_INFO, "%s: chap failure",
			    ifp->if_xname);
			if (len > 4) {
				addlog(": ");
				sppp_print_string((char *)(h + 1), len - 4);
			}
			addlog("\n");
		} else
			log(LOG_INFO, "%s: chap failure\n",
			    ifp->if_xname);
		/* await LCP shutdown by authenticator */
		break;

	/* response is my authproto */
	case CHAP_RESPONSE:
		if (sp->hisauth.secret == NULL) {
		    /* can't do anything useful */
		    printf("%s: chap input without his secret being set\n",
		    	ifp->if_xname);
		    break;
		}
		value = 1 + (u_char *)(h + 1);
		value_len = value[-1];
		name = value + value_len;
		name_len = len - value_len - 5;
		if (name_len < 0) {
			if (debug) {
				log(LOG_DEBUG,
				    "%s: chap corrupted response "
				    "<%s id=0x%x len=%d",
				    ifp->if_xname,
				    sppp_auth_type_name(PPP_CHAP, h->type),
				    h->ident, ntohs(h->len));
				if (len > 4)
					sppp_print_bytes((u_char *)(h + 1),
					    len - 4);
				addlog(">\n");
			}
			break;
		}
		if (h->ident != sp->confid[IDX_CHAP]) {
			if (debug)
				log(LOG_DEBUG,
				    "%s: chap dropping response for old ID "
				    "(got %d, expected %d)\n",
				    ifp->if_xname,
				    h->ident, sp->confid[IDX_CHAP]);
			break;
		}
		if (sp->hisauth.name != NULL &&
		    (name_len != sp->hisauth.name_len
		    || memcmp(name, sp->hisauth.name, name_len) != 0)) {
			log(LOG_INFO, "%s: chap response, his name ",
			    ifp->if_xname);
			sppp_print_string(name, name_len);
			addlog(" != expected ");
			sppp_print_string(sp->hisauth.name,
					  sp->hisauth.name_len);
			addlog("\n");
		    goto chap_failure;
		}
		if (debug) {
			log(LOG_DEBUG, "%s: chap input(%s) "
			    "<%s id=0x%x len=%d name=",
			    ifp->if_xname,
			    sppp_state_name(sp->state[IDX_CHAP]),
			    sppp_auth_type_name(PPP_CHAP, h->type),
			    h->ident, ntohs(h->len));
			sppp_print_string((char *)name, name_len);
			addlog(" value-size=%d value=", value_len);
			sppp_print_bytes(value, value_len);
			addlog(">\n");
		}
		if (value_len != sizeof(sp->myauth.challenge)) {
			if (debug)
				log(LOG_DEBUG,
				    "%s: chap bad hash value length: "
				    "%d bytes, should be %ld\n",
				    ifp->if_xname, value_len,
				    (long) sizeof(sp->myauth.challenge));
			goto chap_failure;
		}

		MD5Init(&ctx);
		MD5Update(&ctx, &h->ident, 1);
		MD5Update(&ctx, sp->hisauth.secret, sp->hisauth.secret_len);
		MD5Update(&ctx, sp->myauth.challenge, sizeof(sp->myauth.challenge));
		MD5Final(digest, &ctx);

#define FAILMSG "Failed..."
#define SUCCMSG "Welcome!"

		if (value_len != sizeof digest ||
		    memcmp(digest, value, value_len) != 0) {
chap_failure:
			/* action scn, tld */
			x = splnet();
			sp->pp_auth_failures++;
			splx(x);
			sppp_auth_send(&chap, sp, CHAP_FAILURE, h->ident,
				       sizeof(FAILMSG) - 1, (const u_char *)FAILMSG,
				       0);
			chap.tld(sp);
			break;
		}
		sp->pp_auth_failures = 0;
		/* action sca, perhaps tlu */
		if (sp->state[IDX_CHAP] == STATE_REQ_SENT ||
		    sp->state[IDX_CHAP] == STATE_OPENED)
			sppp_auth_send(&chap, sp, CHAP_SUCCESS, h->ident,
				       sizeof(SUCCMSG) - 1, (const u_char *)SUCCMSG,
				       0);
		if (sp->state[IDX_CHAP] == STATE_REQ_SENT) {
			sppp_cp_change_state(&chap, sp, STATE_OPENED);
			chap.tlu(sp);
		}
		break;

	default:
		/* Unknown CHAP packet type -- ignore. */
		if (debug) {
			log(LOG_DEBUG, "%s: chap unknown input(%s) "
			    "<0x%x id=0x%xh len=%d",
			    ifp->if_xname,
			    sppp_state_name(sp->state[IDX_CHAP]),
			    h->type, h->ident, ntohs(h->len));
			if (len > 4)
				sppp_print_bytes((u_char *)(h + 1), len - 4);
			addlog(">\n");
		}
		break;

	}
}

static void
sppp_chap_init(struct sppp *sp)
{
	/* Chap doesn't have STATE_INITIAL at all. */
	sp->state[IDX_CHAP] = STATE_CLOSED;
	sp->fail_counter[IDX_CHAP] = 0;
	sp->pp_seq[IDX_CHAP] = 0;
	sp->pp_rseq[IDX_CHAP] = 0;
	callout_init(&sp->ch[IDX_CHAP], 0);
}

static void
sppp_chap_open(struct sppp *sp)
{
	if (sp->myauth.proto == PPP_CHAP &&
	    (sp->lcp.opts & (1 << LCP_OPT_AUTH_PROTO)) != 0) {
		/* we are authenticator for CHAP, start it */
		chap.scr(sp);
		sp->rst_counter[IDX_CHAP] = sp->lcp.max_configure;
		sppp_cp_change_state(&chap, sp, STATE_REQ_SENT);
	}
	/* nothing to be done if we are peer, await a challenge */
}

static void
sppp_chap_close(struct sppp *sp)
{
	if (sp->state[IDX_CHAP] != STATE_CLOSED)
		sppp_cp_change_state(&chap, sp, STATE_CLOSED);
}

static void
sppp_chap_TO(void *cookie)
{
	struct sppp *sp = (struct sppp *)cookie;
	STDDCL;
	int s;

	s = splnet();
	if (debug)
		log(LOG_DEBUG, "%s: chap TO(%s) rst_counter = %d\n",
		    ifp->if_xname,
		    sppp_state_name(sp->state[IDX_CHAP]),
		    sp->rst_counter[IDX_CHAP]);

	if (--sp->rst_counter[IDX_CHAP] < 0)
		/* TO- event */
		switch (sp->state[IDX_CHAP]) {
		case STATE_REQ_SENT:
			chap.tld(sp);
			sppp_cp_change_state(&chap, sp, STATE_CLOSED);
			break;
		}
	else
		/* TO+ (or TO*) event */
		switch (sp->state[IDX_CHAP]) {
		case STATE_OPENED:
			/* TO* event */
			sp->rst_counter[IDX_CHAP] = sp->lcp.max_configure;
			/* fall through */
		case STATE_REQ_SENT:
			chap.scr(sp);
			/* sppp_cp_change_state() will restart the timer */
			sppp_cp_change_state(&chap, sp, STATE_REQ_SENT);
			break;
		}

	splx(s);
}

static void
sppp_chap_tlu(struct sppp *sp)
{
	STDDCL;
	int i, x;

	i = 0;
	sp->rst_counter[IDX_CHAP] = sp->lcp.max_configure;

	/*
	 * Some broken CHAP implementations (Conware CoNet, firmware
	 * 4.0.?) don't want to re-authenticate their CHAP once the
	 * initial challenge-response exchange has taken place.
	 * Provide for an option to avoid rechallenges.
	 */
	if ((sp->hisauth.flags & SPPP_AUTHFLAG_NORECHALLENGE) == 0) {
		/*
		 * Compute the re-challenge timeout.  This will yield
		 * a number between 300 and 810 seconds.
		 */
		i = 300 + ((unsigned)(cprng_fast32() & 0xff00) >> 7);

		callout_reset(&sp->ch[IDX_CHAP], i * hz, chap.TO, sp);
	}

	if (debug) {
		log(LOG_DEBUG,
		    "%s: chap %s, ",
		    ifp->if_xname,
		    sp->pp_phase == SPPP_PHASE_NETWORK? "reconfirmed": "tlu");
		if ((sp->hisauth.flags & SPPP_AUTHFLAG_NORECHALLENGE) == 0)
			addlog("next re-challenge in %d seconds\n", i);
		else
			addlog("re-challenging supressed\n");
	}

	x = splnet();
	sp->pp_auth_failures = 0;
	/* indicate to LCP that we need to be closed down */
	sp->lcp.protos |= (1 << IDX_CHAP);

	if (sp->pp_flags & PP_NEEDAUTH) {
		/*
		 * Remote is authenticator, but his auth proto didn't
		 * complete yet.  Defer the transition to network
		 * phase.
		 */
		splx(x);
		return;
	}
	splx(x);

	/*
	 * If we are already in phase network, we are done here.  This
	 * is the case if this is a dummy tlu event after a re-challenge.
	 */
	if (sp->pp_phase != SPPP_PHASE_NETWORK)
		sppp_phase_network(sp);
}

static void
sppp_chap_tld(struct sppp *sp)
{
	STDDCL;

	if (debug)
		log(LOG_DEBUG, "%s: chap tld\n", ifp->if_xname);
	callout_stop(&sp->ch[IDX_CHAP]);
	sp->lcp.protos &= ~(1 << IDX_CHAP);

	lcp.Close(sp);
}

static void
sppp_chap_scr(struct sppp *sp)
{
	uint32_t *ch;
	u_char clen = 4 * sizeof(uint32_t);

	if (sp->myauth.name == NULL) {
	    /* can't do anything useful */
	    printf("%s: chap starting without my name being set\n",
	    	sp->pp_if.if_xname);
	    return;
	}

	/* Compute random challenge. */
	ch = (uint32_t *)sp->myauth.challenge;
	cprng_strong(kern_cprng, ch, clen, 0);

	sp->confid[IDX_CHAP] = ++sp->pp_seq[IDX_CHAP];

	sppp_auth_send(&chap, sp, CHAP_CHALLENGE, sp->confid[IDX_CHAP],
		       sizeof clen, (const char *)&clen,
		       sizeof(sp->myauth.challenge), sp->myauth.challenge,
		       sp->myauth.name_len,
		       sp->myauth.name,
		       0);
}

/*
 *--------------------------------------------------------------------------*
 *                                                                          *
 *                        The PAP implementation.                           *
 *                                                                          *
 *--------------------------------------------------------------------------*
 */
/*
 * For PAP, we need to keep a little state also if we are the peer, not the
 * authenticator.  This is since we don't get a request to authenticate, but
 * have to repeatedly authenticate ourself until we got a response (or the
 * retry counter is expired).
 */

/*
 * Handle incoming PAP packets.  */
static void
sppp_pap_input(struct sppp *sp, struct mbuf *m)
{
	STDDCL;
	struct lcp_header *h;
	int len, x;
	u_char mlen;
	char *name, *secret;
	int name_len, secret_len;

	/*
	 * Malicious input might leave this uninitialized, so
	 * init to an impossible value.
	 */
	secret_len = -1;

	len = m->m_pkthdr.len;
	if (len < 5) {
		if (debug)
			log(LOG_DEBUG,
			    "%s: pap invalid packet length: %d bytes\n",
			    ifp->if_xname, len);
		return;
	}
	h = mtod(m, struct lcp_header *);
	if (len > ntohs(h->len))
		len = ntohs(h->len);
	switch (h->type) {
	/* PAP request is my authproto */
	case PAP_REQ:
		if (sp->hisauth.name == NULL || sp->hisauth.secret == NULL) {
		    /* can't do anything useful */
		    printf("%s: pap request without his name and his secret being set\n",
		    	ifp->if_xname);
		    break;
		}
		name = 1 + (u_char *)(h + 1);
		name_len = name[-1];
		secret = name + name_len + 1;
		if (name_len > len - 6 ||
		    (secret_len = secret[-1]) > len - 6 - name_len) {
			if (debug) {
				log(LOG_DEBUG, "%s: pap corrupted input "
				    "<%s id=0x%x len=%d",
				    ifp->if_xname,
				    sppp_auth_type_name(PPP_PAP, h->type),
				    h->ident, ntohs(h->len));
				if (len > 4)
					sppp_print_bytes((u_char *)(h + 1),
					    len - 4);
				addlog(">\n");
			}
			break;
		}
		if (debug) {
			log(LOG_DEBUG, "%s: pap input(%s) "
			    "<%s id=0x%x len=%d name=",
			    ifp->if_xname,
			    sppp_state_name(sp->state[IDX_PAP]),
			    sppp_auth_type_name(PPP_PAP, h->type),
			    h->ident, ntohs(h->len));
			sppp_print_string((char *)name, name_len);
			addlog(" secret=");
			sppp_print_string((char *)secret, secret_len);
			addlog(">\n");
		}
		if (name_len != sp->hisauth.name_len ||
		    secret_len != sp->hisauth.secret_len ||
		    memcmp(name, sp->hisauth.name, name_len) != 0 ||
		    memcmp(secret, sp->hisauth.secret, secret_len) != 0) {
			/* action scn, tld */
			sp->pp_auth_failures++;
			mlen = sizeof(FAILMSG) - 1;
			sppp_auth_send(&pap, sp, PAP_NAK, h->ident,
				       sizeof mlen, (const char *)&mlen,
				       sizeof(FAILMSG) - 1, (const u_char *)FAILMSG,
				       0);
			pap.tld(sp);
			break;
		}
		/* action sca, perhaps tlu */
		if (sp->state[IDX_PAP] == STATE_REQ_SENT ||
		    sp->state[IDX_PAP] == STATE_OPENED) {
			mlen = sizeof(SUCCMSG) - 1;
			sppp_auth_send(&pap, sp, PAP_ACK, h->ident,
				       sizeof mlen, (const char *)&mlen,
				       sizeof(SUCCMSG) - 1, (const u_char *)SUCCMSG,
				       0);
		}
		if (sp->state[IDX_PAP] == STATE_REQ_SENT) {
			sppp_cp_change_state(&pap, sp, STATE_OPENED);
			pap.tlu(sp);
		}
		break;

	/* ack and nak are his authproto */
	case PAP_ACK:
		callout_stop(&sp->pap_my_to_ch);
		if (debug) {
			log(LOG_DEBUG, "%s: pap success",
			    ifp->if_xname);
			name = 1 + (u_char *)(h + 1);
			name_len = name[-1];
			if (len > 5 && name_len < len+4) {
				addlog(": ");
				sppp_print_string(name, name_len);
			}
			addlog("\n");
		}
		x = splnet();
		sp->pp_auth_failures = 0;
		sp->pp_flags &= ~PP_NEEDAUTH;
		if (sp->myauth.proto == PPP_PAP &&
		    (sp->lcp.opts & (1 << LCP_OPT_AUTH_PROTO)) &&
		    (sp->lcp.protos & (1 << IDX_PAP)) == 0) {
			/*
			 * We are authenticator for PAP but didn't
			 * complete yet.  Leave it to tlu to proceed
			 * to network phase.
			 */
			splx(x);
			break;
		}
		splx(x);
		sppp_phase_network(sp);
		break;

	case PAP_NAK:
		callout_stop(&sp->pap_my_to_ch);
		sp->pp_auth_failures++;
		if (debug) {
			log(LOG_INFO, "%s: pap failure",
			    ifp->if_xname);
			name = 1 + (u_char *)(h + 1);
			name_len = name[-1];
			if (len > 5 && name_len < len+4) {
				addlog(": ");
				sppp_print_string(name, name_len);
			}
			addlog("\n");
		} else
			log(LOG_INFO, "%s: pap failure\n",
			    ifp->if_xname);
		/* await LCP shutdown by authenticator */
		break;

	default:
		/* Unknown PAP packet type -- ignore. */
		if (debug) {
			log(LOG_DEBUG, "%s: pap corrupted input "
			    "<0x%x id=0x%x len=%d",
			    ifp->if_xname,
			    h->type, h->ident, ntohs(h->len));
			if (len > 4)
				sppp_print_bytes((u_char *)(h + 1), len - 4);
			addlog(">\n");
		}
		break;

	}
}

static void
sppp_pap_init(struct sppp *sp)
{
	/* PAP doesn't have STATE_INITIAL at all. */
	sp->state[IDX_PAP] = STATE_CLOSED;
	sp->fail_counter[IDX_PAP] = 0;
	sp->pp_seq[IDX_PAP] = 0;
	sp->pp_rseq[IDX_PAP] = 0;
	callout_init(&sp->ch[IDX_PAP], 0);
	callout_init(&sp->pap_my_to_ch, 0);
}

static void
sppp_pap_open(struct sppp *sp)
{
	if (sp->hisauth.proto == PPP_PAP &&
	    (sp->lcp.opts & (1 << LCP_OPT_AUTH_PROTO)) != 0) {
		/* we are authenticator for PAP, start our timer */
		sp->rst_counter[IDX_PAP] = sp->lcp.max_configure;
		sppp_cp_change_state(&pap, sp, STATE_REQ_SENT);
	}
	if (sp->myauth.proto == PPP_PAP) {
		/* we are peer, send a request, and start a timer */
		pap.scr(sp);
		callout_reset(&sp->pap_my_to_ch, sp->lcp.timeout,
		    sppp_pap_my_TO, sp);
	}
}

static void
sppp_pap_close(struct sppp *sp)
{
	if (sp->state[IDX_PAP] != STATE_CLOSED)
		sppp_cp_change_state(&pap, sp, STATE_CLOSED);
}

/*
 * That's the timeout routine if we are authenticator.  Since the
 * authenticator is basically passive in PAP, we can't do much here.
 */
static void
sppp_pap_TO(void *cookie)
{
	struct sppp *sp = (struct sppp *)cookie;
	STDDCL;
	int s;

	s = splnet();
	if (debug)
		log(LOG_DEBUG, "%s: pap TO(%s) rst_counter = %d\n",
		    ifp->if_xname,
		    sppp_state_name(sp->state[IDX_PAP]),
		    sp->rst_counter[IDX_PAP]);

	if (--sp->rst_counter[IDX_PAP] < 0)
		/* TO- event */
		switch (sp->state[IDX_PAP]) {
		case STATE_REQ_SENT:
			pap.tld(sp);
			sppp_cp_change_state(&pap, sp, STATE_CLOSED);
			break;
		}
	else
		/* TO+ event, not very much we could do */
		switch (sp->state[IDX_PAP]) {
		case STATE_REQ_SENT:
			/* sppp_cp_change_state() will restart the timer */
			sppp_cp_change_state(&pap, sp, STATE_REQ_SENT);
			break;
		}

	splx(s);
}

/*
 * That's the timeout handler if we are peer.  Since the peer is active,
 * we need to retransmit our PAP request since it is apparently lost.
 * XXX We should impose a max counter.
 */
static void
sppp_pap_my_TO(void *cookie)
{
	struct sppp *sp = (struct sppp *)cookie;
	STDDCL;

	if (debug)
		log(LOG_DEBUG, "%s: pap peer TO\n",
		    ifp->if_xname);

	pap.scr(sp);
}

static void
sppp_pap_tlu(struct sppp *sp)
{
	STDDCL;
	int x;

	sp->rst_counter[IDX_PAP] = sp->lcp.max_configure;

	if (debug)
		log(LOG_DEBUG, "%s: %s tlu\n",
		    ifp->if_xname, pap.name);

	x = splnet();
	sp->pp_auth_failures = 0;
	/* indicate to LCP that we need to be closed down */
	sp->lcp.protos |= (1 << IDX_PAP);

	if (sp->pp_flags & PP_NEEDAUTH) {
		/*
		 * Remote is authenticator, but his auth proto didn't
		 * complete yet.  Defer the transition to network
		 * phase.
		 */
		splx(x);
		return;
	}
	splx(x);
	sppp_phase_network(sp);
}

static void
sppp_pap_tld(struct sppp *sp)
{
	STDDCL;

	if (debug)
		log(LOG_DEBUG, "%s: pap tld\n", ifp->if_xname);
	callout_stop(&sp->ch[IDX_PAP]);
	callout_stop(&sp->pap_my_to_ch);
	sp->lcp.protos &= ~(1 << IDX_PAP);

	lcp.Close(sp);
}

static void
sppp_pap_scr(struct sppp *sp)
{
	u_char idlen, pwdlen;

	if (sp->myauth.secret == NULL || sp->myauth.name == NULL) {
	    /* can't do anything useful */
	    printf("%s: pap starting without my name and secret being set\n",
	    	sp->pp_if.if_xname);
	    return;
	}

	sp->confid[IDX_PAP] = ++sp->pp_seq[IDX_PAP];
	pwdlen = sp->myauth.secret_len;
	idlen = sp->myauth.name_len;

	sppp_auth_send(&pap, sp, PAP_REQ, sp->confid[IDX_PAP],
		       sizeof idlen, (const char *)&idlen,
		       idlen, sp->myauth.name,
		       sizeof pwdlen, (const char *)&pwdlen,
		       pwdlen, sp->myauth.secret,
		       0);
}

/*
 * Random miscellaneous functions.
 */

/*
 * Send a PAP or CHAP proto packet.
 *
 * Varadic function, each of the elements for the ellipsis is of type
 * ``size_t mlen, const u_char *msg''.  Processing will stop iff
 * mlen == 0.
 * NOTE: never declare variadic functions with types subject to type
 * promotion (i.e. u_char). This is asking for big trouble depending
 * on the architecture you are on...
 */

static void
sppp_auth_send(const struct cp *cp, struct sppp *sp,
               unsigned int type, unsigned int id,
	       ...)
{
	STDDCL;
	struct lcp_header *lh;
	struct mbuf *m;
	u_char *p;
	int len;
	size_t pkthdrlen;
	unsigned int mlen;
	const char *msg;
	va_list ap;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (! m)
		return;
	m->m_pkthdr.rcvif = 0;

	if (sp->pp_flags & PP_NOFRAMING) {
		*mtod(m, uint16_t *) = htons(cp->proto);
		pkthdrlen = 2;
		lh = (struct lcp_header *)(mtod(m, uint8_t *)+2);
	} else {
		struct ppp_header *h;
		h = mtod(m, struct ppp_header *);
		h->address = PPP_ALLSTATIONS;		/* broadcast address */
		h->control = PPP_UI;			/* Unnumbered Info */
		h->protocol = htons(cp->proto);
		pkthdrlen = PPP_HEADER_LEN;

		lh = (struct lcp_header *)(h + 1);
	}

	lh->type = type;
	lh->ident = id;
	p = (u_char *)(lh + 1);

	va_start(ap, id);
	len = 0;

	while ((mlen = (unsigned int)va_arg(ap, size_t)) != 0) {
		msg = va_arg(ap, const char *);
		len += mlen;
		if (len > MHLEN - pkthdrlen - LCP_HEADER_LEN) {
			va_end(ap);
			m_freem(m);
			return;
		}

		memcpy(p, msg, mlen);
		p += mlen;
	}
	va_end(ap);

	m->m_pkthdr.len = m->m_len = pkthdrlen + LCP_HEADER_LEN + len;
	lh->len = htons(LCP_HEADER_LEN + len);

	if (debug) {
		log(LOG_DEBUG, "%s: %s output <%s id=0x%x len=%d",
		    ifp->if_xname, cp->name,
		    sppp_auth_type_name(cp->proto, lh->type),
		    lh->ident, ntohs(lh->len));
		if (len)
			sppp_print_bytes((u_char *)(lh + 1), len);
		addlog(">\n");
	}
	if (IF_QFULL(&sp->pp_cpq)) {
		IF_DROP(&sp->pp_fastq);
		IF_DROP(&ifp->if_snd);
		m_freem(m);
		++ifp->if_oerrors;
		return;
	} else
		IF_ENQUEUE(&sp->pp_cpq, m);
	if (! (ifp->if_flags & IFF_OACTIVE))
		(*ifp->if_start)(ifp);
	ifp->if_obytes += m->m_pkthdr.len + 3;
}

/*
 * Send keepalive packets, every 10 seconds.
 */
static void
sppp_keepalive(void *dummy)
{
	struct sppp *sp;
	int s;
	time_t now;

	s = splnet();
	now = time_uptime;
	for (sp=spppq; sp; sp=sp->pp_next) {
		struct ifnet *ifp = &sp->pp_if;

		/* check idle timeout */
		if ((sp->pp_idle_timeout != 0) && (ifp->if_flags & IFF_RUNNING)
		    && (sp->pp_phase == SPPP_PHASE_NETWORK)) {
		    /* idle timeout is enabled for this interface */
		    if ((now-sp->pp_last_activity) >= sp->pp_idle_timeout) {
		    	if (ifp->if_flags & IFF_DEBUG)
			    printf("%s: no activity for %lu seconds\n",
				sp->pp_if.if_xname,
				(unsigned long)(now-sp->pp_last_activity));
			lcp.Close(sp);
			continue;
		    }
		}

		/* Keepalive mode disabled or channel down? */
		if (! (sp->pp_flags & PP_KEEPALIVE) ||
		    ! (ifp->if_flags & IFF_RUNNING))
			continue;

		/* No keepalive in PPP mode if LCP not opened yet. */
		if (! (sp->pp_flags & PP_CISCO) &&
		    sp->pp_phase < SPPP_PHASE_AUTHENTICATE)
			continue;

		/* No echo reply, but maybe user data passed through? */
		if ((now - sp->pp_last_receive) < sp->pp_max_noreceive) {
			sp->pp_alivecnt = 0;
			continue;
		}

		if (sp->pp_alivecnt >= sp->pp_maxalive) {
			/* No keepalive packets got.  Stop the interface. */
			if_down (ifp);
			IF_PURGE(&sp->pp_cpq);
			if (! (sp->pp_flags & PP_CISCO)) {
				printf("%s: LCP keepalive timed out, going to restart the connection\n",
					ifp->if_xname);
				sp->pp_alivecnt = 0;

				/* we are down, close all open protocols */
				lcp.Close(sp);

				/* And now prepare LCP to reestablish the link, if configured to do so. */
				sppp_cp_change_state(&lcp, sp, STATE_STOPPED);

				/* Close connection immediately, completition of this
				 * will summon the magic needed to reestablish it. */
				if (sp->pp_tlf)
					sp->pp_tlf(sp);
				continue;
			}
		}
		if (sp->pp_alivecnt < sp->pp_maxalive)
			++sp->pp_alivecnt;
		if (sp->pp_flags & PP_CISCO)
			sppp_cisco_send(sp, CISCO_KEEPALIVE_REQ,
			    ++sp->pp_seq[IDX_LCP], sp->pp_rseq[IDX_LCP]);
		else if (sp->pp_phase >= SPPP_PHASE_AUTHENTICATE) {
			int32_t nmagic = htonl(sp->lcp.magic);
			sp->lcp.echoid = ++sp->pp_seq[IDX_LCP];
			sppp_cp_send(sp, PPP_LCP, ECHO_REQ,
				sp->lcp.echoid, 4, &nmagic);
		}
	}
	splx(s);
	callout_reset(&keepalive_ch, hz * LCP_KEEPALIVE_INTERVAL, sppp_keepalive, NULL);
}

#ifdef INET
/*
 * Get both IP addresses.
 */
static void
sppp_get_ip_addrs(struct sppp *sp, uint32_t *src, uint32_t *dst, uint32_t *srcmask)
{
	struct ifnet *ifp = &sp->pp_if;
	struct ifaddr *ifa;
	struct sockaddr_in *si, *sm;
	uint32_t ssrc, ddst;

	sm = NULL;
	ssrc = ddst = 0;
	/*
	 * Pick the first AF_INET address from the list,
	 * aliases don't make any sense on a p2p link anyway.
	 */
	si = 0;
	IFADDR_FOREACH(ifa, ifp) {
		if (ifa->ifa_addr->sa_family == AF_INET) {
			si = (struct sockaddr_in *)ifa->ifa_addr;
			sm = (struct sockaddr_in *)ifa->ifa_netmask;
			if (si)
				break;
		}
	}
	if (ifa) {
		if (si && si->sin_addr.s_addr) {
			ssrc = si->sin_addr.s_addr;
			if (srcmask)
				*srcmask = ntohl(sm->sin_addr.s_addr);
		}

		si = (struct sockaddr_in *)ifa->ifa_dstaddr;
		if (si && si->sin_addr.s_addr)
			ddst = si->sin_addr.s_addr;
	}

	if (dst) *dst = ntohl(ddst);
	if (src) *src = ntohl(ssrc);
}

/*
 * Set IP addresses.  Must be called at splnet.
 * If an address is 0, leave it the way it is.
 */
static void
sppp_set_ip_addrs(struct sppp *sp, uint32_t myaddr, uint32_t hisaddr)
{
	STDDCL;
	struct ifaddr *ifa;
	struct sockaddr_in *si, *dest;

	/*
	 * Pick the first AF_INET address from the list,
	 * aliases don't make any sense on a p2p link anyway.
	 */

	IFADDR_FOREACH(ifa, ifp) {
		if (ifa->ifa_addr->sa_family == AF_INET) {
			si = (struct sockaddr_in *)ifa->ifa_addr;
			dest = (struct sockaddr_in *)ifa->ifa_dstaddr;
			goto found;
		}
	}
	return;

found:
	{
		int error, hostIsNew;
		struct sockaddr_in new_sin = *si;
		struct sockaddr_in new_dst = *dest;

		/*
		 * Scrub old routes now instead of calling in_ifinit with
		 * scrub=1, because we may change the dstaddr
		 * before the call to in_ifinit.
		 */
		in_ifscrub(ifp, ifatoia(ifa));

		hostIsNew = 0;
		if (myaddr != 0) {
			if (new_sin.sin_addr.s_addr != htonl(myaddr)) {
				new_sin.sin_addr.s_addr = htonl(myaddr);
				hostIsNew = 1;
			}
		}
		if (hisaddr != 0) {
			new_dst.sin_addr.s_addr = htonl(hisaddr);
			if (new_dst.sin_addr.s_addr != dest->sin_addr.s_addr) {
				sp->ipcp.saved_hisaddr = dest->sin_addr.s_addr;
				*dest = new_dst; /* fix dstaddr in place */
			}
		}
		error = in_ifinit(ifp, ifatoia(ifa), &new_sin, 0, hostIsNew);
		if (debug && error)
		{
			log(LOG_DEBUG, "%s: sppp_set_ip_addrs: in_ifinit "
			" failed, error=%d\n", ifp->if_xname, error);
		}
		if (!error) {
			(void)pfil_run_hooks(if_pfil,
			    (struct mbuf **)SIOCAIFADDR, ifp, PFIL_IFADDR);
		}
	}
}

/*
 * Clear IP addresses.  Must be called at splnet.
 */
static void
sppp_clear_ip_addrs(struct sppp *sp)
{
	struct ifnet *ifp = &sp->pp_if;
	struct ifaddr *ifa;
	struct sockaddr_in *si, *dest;

	uint32_t remote;
	if (sp->ipcp.flags & IPCP_HISADDR_DYN)
		remote = sp->ipcp.saved_hisaddr;
	else
		sppp_get_ip_addrs(sp, 0, &remote, 0);

	/*
	 * Pick the first AF_INET address from the list,
	 * aliases don't make any sense on a p2p link anyway.
	 */

	IFADDR_FOREACH(ifa, ifp) {
		if (ifa->ifa_addr->sa_family == AF_INET) {
			si = (struct sockaddr_in *)ifa->ifa_addr;
			dest = (struct sockaddr_in *)ifa->ifa_dstaddr;
			goto found;
		}
	}
	return;

found:
	{
		struct sockaddr_in new_sin = *si;

		in_ifscrub(ifp, ifatoia(ifa));
		if (sp->ipcp.flags & IPCP_MYADDR_DYN)
			new_sin.sin_addr.s_addr = 0;
		if (sp->ipcp.flags & IPCP_HISADDR_DYN)
			/* replace peer addr in place */
			dest->sin_addr.s_addr = sp->ipcp.saved_hisaddr;
		in_ifinit(ifp, ifatoia(ifa), &new_sin, 0, 0);
		(void)pfil_run_hooks(if_pfil,
		    (struct mbuf **)SIOCDIFADDR, ifp, PFIL_IFADDR);
	}
}
#endif

#ifdef INET6
/*
 * Get both IPv6 addresses.
 */
static void
sppp_get_ip6_addrs(struct sppp *sp, struct in6_addr *src, struct in6_addr *dst,
		   struct in6_addr *srcmask)
{
	struct ifnet *ifp = &sp->pp_if;
	struct ifaddr *ifa;
	struct sockaddr_in6 *si, *sm;
	struct in6_addr ssrc, ddst;

	sm = NULL;
	memset(&ssrc, 0, sizeof(ssrc));
	memset(&ddst, 0, sizeof(ddst));
	/*
	 * Pick the first link-local AF_INET6 address from the list,
	 * aliases don't make any sense on a p2p link anyway.
	 */
	si = 0;
	IFADDR_FOREACH(ifa, ifp)
		if (ifa->ifa_addr->sa_family == AF_INET6) {
			si = (struct sockaddr_in6 *)ifa->ifa_addr;
			sm = (struct sockaddr_in6 *)ifa->ifa_netmask;
			if (si && IN6_IS_ADDR_LINKLOCAL(&si->sin6_addr))
				break;
		}
	if (ifa) {
		if (si && !IN6_IS_ADDR_UNSPECIFIED(&si->sin6_addr)) {
			memcpy(&ssrc, &si->sin6_addr, sizeof(ssrc));
			if (srcmask) {
				memcpy(srcmask, &sm->sin6_addr,
				    sizeof(*srcmask));
			}
		}

		si = (struct sockaddr_in6 *)ifa->ifa_dstaddr;
		if (si && !IN6_IS_ADDR_UNSPECIFIED(&si->sin6_addr))
			memcpy(&ddst, &si->sin6_addr, sizeof(ddst));
	}

	if (dst)
		memcpy(dst, &ddst, sizeof(*dst));
	if (src)
		memcpy(src, &ssrc, sizeof(*src));
}

#ifdef IPV6CP_MYIFID_DYN
/*
 * Generate random ifid.
 */
static void
sppp_gen_ip6_addr(struct sppp *sp, struct in6_addr *addr)
{
	/* TBD */
}

/*
 * Set my IPv6 address.  Must be called at splnet.
 */
static void
sppp_set_ip6_addr(struct sppp *sp, const struct in6_addr *src)
{
	STDDCL;
	struct ifaddr *ifa;
	struct sockaddr_in6 *sin6;

	/*
	 * Pick the first link-local AF_INET6 address from the list,
	 * aliases don't make any sense on a p2p link anyway.
	 */

	sin6 = NULL;
	IFADDR_FOREACH(ifa, ifp)
	{
		if (ifa->ifa_addr->sa_family == AF_INET6)
		{
			sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
			if (sin6 && IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr))
				break;
		}
	}

	if (ifa && sin6)
	{
		int error;
		struct sockaddr_in6 new_sin6 = *sin6;

		memcpy(&new_sin6.sin6_addr, src, sizeof(new_sin6.sin6_addr));
		error = in6_ifinit(ifp, ifatoia6(ifa), &new_sin6, 1);
		if (debug && error)
		{
			log(LOG_DEBUG, "%s: sppp_set_ip6_addr: in6_ifinit "
			" failed, error=%d\n", ifp->if_xname, error);
		}
		if (!error) {
			(void)pfil_run_hooks(if_pfil,
			    (struct mbuf **)SIOCAIFADDR_IN6, ifp, PFIL_IFADDR);
		}
	}
}
#endif

/*
 * Suggest a candidate address to be used by peer.
 */
static void
sppp_suggest_ip6_addr(struct sppp *sp, struct in6_addr *suggest)
{
	struct in6_addr myaddr;
	struct timeval tv;

	sppp_get_ip6_addrs(sp, &myaddr, 0, 0);

	myaddr.s6_addr[8] &= ~0x02;	/* u bit to "local" */
	microtime(&tv);
	if ((tv.tv_usec & 0xff) == 0 && (tv.tv_sec & 0xff) == 0) {
		myaddr.s6_addr[14] ^= 0xff;
		myaddr.s6_addr[15] ^= 0xff;
	} else {
		myaddr.s6_addr[14] ^= (tv.tv_usec & 0xff);
		myaddr.s6_addr[15] ^= (tv.tv_sec & 0xff);
	}
	if (suggest)
		memcpy(suggest, &myaddr, sizeof(myaddr));
}
#endif /*INET6*/

/*
 * Process ioctl requests specific to the PPP interface.
 * Permissions have already been checked.
 */
static int
sppp_params(struct sppp *sp, u_long cmd, void *data)
{
	switch (cmd) {
	case SPPPGETAUTHCFG:
	    {
		struct spppauthcfg *cfg = (struct spppauthcfg *)data;
		int error;
		size_t len;

		cfg->myauthflags = sp->myauth.flags;
		cfg->hisauthflags = sp->hisauth.flags;
		strncpy(cfg->ifname, sp->pp_if.if_xname, IFNAMSIZ);
		cfg->hisauth = 0;
		if (sp->hisauth.proto)
		    cfg->hisauth = (sp->hisauth.proto == PPP_PAP) ? SPPP_AUTHPROTO_PAP : SPPP_AUTHPROTO_CHAP;
		cfg->myauth = 0;
		if (sp->myauth.proto)
		    cfg->myauth = (sp->myauth.proto == PPP_PAP) ? SPPP_AUTHPROTO_PAP : SPPP_AUTHPROTO_CHAP;
		if (cfg->myname_length == 0) {
		    if (sp->myauth.name != NULL)
			cfg->myname_length = sp->myauth.name_len + 1;
		} else {
		    if (sp->myauth.name == NULL) {
			cfg->myname_length = 0;
		    } else {
			len = sp->myauth.name_len + 1;
			if (cfg->myname_length < len)
			    return (ENAMETOOLONG);
			error = copyout(sp->myauth.name, cfg->myname, len);
			if (error) return error;
		    }
		}
		if (cfg->hisname_length == 0) {
		    if (sp->hisauth.name != NULL)
			cfg->hisname_length = sp->hisauth.name_len + 1;
		} else {
		    if (sp->hisauth.name == NULL) {
		    	cfg->hisname_length = 0;
		    } else {
			len = sp->hisauth.name_len + 1;
			if (cfg->hisname_length < len)
			    return (ENAMETOOLONG);
			error = copyout(sp->hisauth.name, cfg->hisname, len);
			if (error) return error;
		    }
		}
	    }
	    break;
	case SPPPSETAUTHCFG:
	    {
		struct spppauthcfg *cfg = (struct spppauthcfg *)data;
		int error;

		if (sp->myauth.name) {
			free(sp->myauth.name, M_DEVBUF);
			sp->myauth.name = NULL;
		}
		if (sp->myauth.secret) {
			free(sp->myauth.secret, M_DEVBUF);
			sp->myauth.secret = NULL;
		}
		if (sp->hisauth.name) {
			free(sp->hisauth.name, M_DEVBUF);
			sp->hisauth.name = NULL;
		}
		if (sp->hisauth.secret) {
			free(sp->hisauth.secret, M_DEVBUF);
			sp->hisauth.secret = NULL;
		}

		if (cfg->hisname != NULL && cfg->hisname_length > 0) {
		    if (cfg->hisname_length >= MCLBYTES)
			return (ENAMETOOLONG);
		    sp->hisauth.name = malloc(cfg->hisname_length, M_DEVBUF, M_WAITOK);
		    error = copyin(cfg->hisname, sp->hisauth.name, cfg->hisname_length);
		    if (error) {
			free(sp->hisauth.name, M_DEVBUF);
			sp->hisauth.name = NULL;
			return error;
		    }
		    sp->hisauth.name_len = cfg->hisname_length - 1;
		    sp->hisauth.name[sp->hisauth.name_len] = 0;
		}
		if (cfg->hissecret != NULL && cfg->hissecret_length > 0) {
		    if (cfg->hissecret_length >= MCLBYTES)
			return (ENAMETOOLONG);
		    sp->hisauth.secret = malloc(cfg->hissecret_length, M_DEVBUF, M_WAITOK);
		    error = copyin(cfg->hissecret, sp->hisauth.secret, cfg->hissecret_length);
		    if (error) {
		    	free(sp->hisauth.secret, M_DEVBUF);
		    	sp->hisauth.secret = NULL;
			return error;
		    }
		    sp->hisauth.secret_len = cfg->hissecret_length - 1;
		    sp->hisauth.secret[sp->hisauth.secret_len] = 0;
		}
		if (cfg->myname != NULL && cfg->myname_length > 0) {
		    if (cfg->myname_length >= MCLBYTES)
			return (ENAMETOOLONG);
		    sp->myauth.name = malloc(cfg->myname_length, M_DEVBUF, M_WAITOK);
		    error = copyin(cfg->myname, sp->myauth.name, cfg->myname_length);
		    if (error) {
			free(sp->myauth.name, M_DEVBUF);
			sp->myauth.name = NULL;
			return error;
		    }
		    sp->myauth.name_len = cfg->myname_length - 1;
		    sp->myauth.name[sp->myauth.name_len] = 0;
		}
		if (cfg->mysecret != NULL && cfg->mysecret_length > 0) {
		    if (cfg->mysecret_length >= MCLBYTES)
			return (ENAMETOOLONG);
		    sp->myauth.secret = malloc(cfg->mysecret_length, M_DEVBUF, M_WAITOK);
		    error = copyin(cfg->mysecret, sp->myauth.secret, cfg->mysecret_length);
		    if (error) {
		    	free(sp->myauth.secret, M_DEVBUF);
		    	sp->myauth.secret = NULL;
			return error;
		    }
		    sp->myauth.secret_len = cfg->mysecret_length - 1;
		    sp->myauth.secret[sp->myauth.secret_len] = 0;
		}
		sp->myauth.flags = cfg->myauthflags;
		if (cfg->myauth)
		    sp->myauth.proto = (cfg->myauth == SPPP_AUTHPROTO_PAP) ? PPP_PAP : PPP_CHAP;
		sp->hisauth.flags = cfg->hisauthflags;
		if (cfg->hisauth)
		    sp->hisauth.proto = (cfg->hisauth == SPPP_AUTHPROTO_PAP) ? PPP_PAP : PPP_CHAP;
		sp->pp_auth_failures = 0;
		if (sp->hisauth.proto != 0)
		    sp->lcp.opts |= (1 << LCP_OPT_AUTH_PROTO);
		else
		    sp->lcp.opts &= ~(1 << LCP_OPT_AUTH_PROTO);
	    }
	    break;
	case SPPPGETLCPCFG:
	    {
	    	struct sppplcpcfg *lcpp = (struct sppplcpcfg *)data;
	    	lcpp->lcp_timeout = sp->lcp.timeout;
	    }
	    break;
	case SPPPSETLCPCFG:
	    {
	    	struct sppplcpcfg *lcpp = (struct sppplcpcfg *)data;
	    	sp->lcp.timeout = lcpp->lcp_timeout;
	    }
	    break;
	case SPPPGETSTATUS:
	    {
		struct spppstatus *status = (struct spppstatus *)data;
		status->phase = sp->pp_phase;
	    }
	    break;
	case SPPPGETSTATUSNCP:
	    {
		struct spppstatusncp *status = (struct spppstatusncp *)data;
		status->phase = sp->pp_phase;
		status->ncpup = sppp_ncp_check(sp);
	    }
	    break;
	case SPPPGETIDLETO:
	    {
	    	struct spppidletimeout *to = (struct spppidletimeout *)data;
		to->idle_seconds = sp->pp_idle_timeout;
	    }
	    break;
	case SPPPSETIDLETO:
	    {
	    	struct spppidletimeout *to = (struct spppidletimeout *)data;
	    	sp->pp_idle_timeout = to->idle_seconds;
	    }
	    break;
	case SPPPSETAUTHFAILURE:
	    {
	    	struct spppauthfailuresettings *afsettings = (struct spppauthfailuresettings *)data;
	    	sp->pp_max_auth_fail = afsettings->max_failures;
	    	sp->pp_auth_failures = 0;
	    }
	    break;
	case SPPPGETAUTHFAILURES:
	    {
	    	struct spppauthfailurestats *stats = (struct spppauthfailurestats *)data;
	    	stats->auth_failures = sp->pp_auth_failures;
	    	stats->max_failures = sp->pp_max_auth_fail;
	    }
	    break;
	case SPPPSETDNSOPTS:
	    {
		struct spppdnssettings *req = (struct spppdnssettings *)data;
		sp->query_dns = req->query_dns & 3;
	    }
	    break;
	case SPPPGETDNSOPTS:
	    {
		struct spppdnssettings *req = (struct spppdnssettings *)data;
		req->query_dns = sp->query_dns;
	    }
	    break;
	case SPPPGETDNSADDRS:
	    {
	    	struct spppdnsaddrs *addrs = (struct spppdnsaddrs *)data;
	    	memcpy(&addrs->dns, &sp->dns_addrs, sizeof addrs->dns);
	    }
	    break;
	case SPPPGETKEEPALIVE:
	    {
	    	struct spppkeepalivesettings *settings =
		     (struct spppkeepalivesettings*)data;
		settings->maxalive = sp->pp_maxalive;
		settings->max_noreceive = sp->pp_max_noreceive;
	    }
	    break;
	case SPPPSETKEEPALIVE:
	    {
	    	struct spppkeepalivesettings *settings =
		     (struct spppkeepalivesettings*)data;
		sp->pp_maxalive = settings->maxalive;
		sp->pp_max_noreceive = settings->max_noreceive;
	    }
	    break;
#if defined(COMPAT_50) || defined(MODULAR)
	case __SPPPGETIDLETO50:
	    {
	    	struct spppidletimeout50 *to = (struct spppidletimeout50 *)data;
		to->idle_seconds = (uint32_t)sp->pp_idle_timeout;
	    }
	    break;
	case __SPPPSETIDLETO50:
	    {
	    	struct spppidletimeout50 *to = (struct spppidletimeout50 *)data;
	    	sp->pp_idle_timeout = (time_t)to->idle_seconds;
	    }
	    break;
	case __SPPPGETKEEPALIVE50:
	    {
	    	struct spppkeepalivesettings50 *settings =
		     (struct spppkeepalivesettings50*)data;
		settings->maxalive = sp->pp_maxalive;
		settings->max_noreceive = (uint32_t)sp->pp_max_noreceive;
	    }
	    break;
	case __SPPPSETKEEPALIVE50:
	    {
	    	struct spppkeepalivesettings50 *settings =
		     (struct spppkeepalivesettings50*)data;
		sp->pp_maxalive = settings->maxalive;
		sp->pp_max_noreceive = (time_t)settings->max_noreceive;
	    }
	    break;
#endif /* COMPAT_50 || MODULAR */
	default:
		return (EINVAL);
	}

	return (0);
}

static void
sppp_phase_network(struct sppp *sp)
{
	STDDCL;
	int i;
	uint32_t mask;

	sp->pp_phase = SPPP_PHASE_NETWORK;

	if (debug)
	{
		log(LOG_INFO, "%s: phase %s\n", ifp->if_xname,
			sppp_phase_name(sp->pp_phase));
	}

	/* Notify NCPs now. */
	for (i = 0; i < IDX_COUNT; i++)
		if ((cps[i])->flags & CP_NCP)
			(cps[i])->Open(sp);

	/* Send Up events to all NCPs. */
	for (i = 0, mask = 1; i < IDX_COUNT; i++, mask <<= 1)
		if ((sp->lcp.protos & mask) && ((cps[i])->flags & CP_NCP))
			(cps[i])->Up(sp);

	/* if no NCP is starting, all this was in vain, close down */
	sppp_lcp_check_and_close(sp);
}


static const char *
sppp_cp_type_name(u_char type)
{
	static char buf[12];
	switch (type) {
	case CONF_REQ:   return "conf-req";
	case CONF_ACK:   return "conf-ack";
	case CONF_NAK:   return "conf-nak";
	case CONF_REJ:   return "conf-rej";
	case TERM_REQ:   return "term-req";
	case TERM_ACK:   return "term-ack";
	case CODE_REJ:   return "code-rej";
	case PROTO_REJ:  return "proto-rej";
	case ECHO_REQ:   return "echo-req";
	case ECHO_REPLY: return "echo-reply";
	case DISC_REQ:   return "discard-req";
	}
	snprintf(buf, sizeof(buf), "0x%x", type);
	return buf;
}

static const char *
sppp_auth_type_name(u_short proto, u_char type)
{
	static char buf[12];
	switch (proto) {
	case PPP_CHAP:
		switch (type) {
		case CHAP_CHALLENGE:	return "challenge";
		case CHAP_RESPONSE:	return "response";
		case CHAP_SUCCESS:	return "success";
		case CHAP_FAILURE:	return "failure";
		}
	case PPP_PAP:
		switch (type) {
		case PAP_REQ:		return "req";
		case PAP_ACK:		return "ack";
		case PAP_NAK:		return "nak";
		}
	}
	snprintf(buf, sizeof(buf), "0x%x", type);
	return buf;
}

static const char *
sppp_lcp_opt_name(u_char opt)
{
	static char buf[12];
	switch (opt) {
	case LCP_OPT_MRU:		return "mru";
	case LCP_OPT_ASYNC_MAP:		return "async-map";
	case LCP_OPT_AUTH_PROTO:	return "auth-proto";
	case LCP_OPT_QUAL_PROTO:	return "qual-proto";
	case LCP_OPT_MAGIC:		return "magic";
	case LCP_OPT_PROTO_COMP:	return "proto-comp";
	case LCP_OPT_ADDR_COMP:		return "addr-comp";
	}
	snprintf(buf, sizeof(buf), "0x%x", opt);
	return buf;
}

static const char *
sppp_ipcp_opt_name(u_char opt)
{
	static char buf[12];
	switch (opt) {
	case IPCP_OPT_ADDRESSES:	return "addresses";
	case IPCP_OPT_COMPRESSION:	return "compression";
	case IPCP_OPT_ADDRESS:		return "address";
	}
	snprintf(buf, sizeof(buf), "0x%x", opt);
	return buf;
}

#ifdef INET6
static const char *
sppp_ipv6cp_opt_name(u_char opt)
{
	static char buf[12];
	switch (opt) {
	case IPV6CP_OPT_IFID:		return "ifid";
	case IPV6CP_OPT_COMPRESSION:	return "compression";
	}
	snprintf(buf, sizeof(buf), "0x%x", opt);
	return buf;
}
#endif

static const char *
sppp_state_name(int state)
{
	switch (state) {
	case STATE_INITIAL:	return "initial";
	case STATE_STARTING:	return "starting";
	case STATE_CLOSED:	return "closed";
	case STATE_STOPPED:	return "stopped";
	case STATE_CLOSING:	return "closing";
	case STATE_STOPPING:	return "stopping";
	case STATE_REQ_SENT:	return "req-sent";
	case STATE_ACK_RCVD:	return "ack-rcvd";
	case STATE_ACK_SENT:	return "ack-sent";
	case STATE_OPENED:	return "opened";
	}
	return "illegal";
}

static const char *
sppp_phase_name(int phase)
{
	switch (phase) {
	case SPPP_PHASE_DEAD:		return "dead";
	case SPPP_PHASE_ESTABLISH:	return "establish";
	case SPPP_PHASE_TERMINATE:	return "terminate";
	case SPPP_PHASE_AUTHENTICATE: 	return "authenticate";
	case SPPP_PHASE_NETWORK:	return "network";
	}
	return "illegal";
}

static const char *
sppp_proto_name(u_short proto)
{
	static char buf[12];
	switch (proto) {
	case PPP_LCP:	return "lcp";
	case PPP_IPCP:	return "ipcp";
	case PPP_PAP:	return "pap";
	case PPP_CHAP:	return "chap";
	case PPP_IPV6CP: return "ipv6cp";
	}
	snprintf(buf, sizeof(buf), "0x%x", (unsigned)proto);
	return buf;
}

static void
sppp_print_bytes(const u_char *p, u_short len)
{
	addlog(" %02x", *p++);
	while (--len > 0)
		addlog("-%02x", *p++);
}

static void
sppp_print_string(const char *p, u_short len)
{
	u_char c;

	while (len-- > 0) {
		c = *p++;
		/*
		 * Print only ASCII chars directly.  RFC 1994 recommends
		 * using only them, but we don't rely on it.  */
		if (c < ' ' || c > '~')
			addlog("\\x%x", c);
		else
			addlog("%c", c);
	}
}

static const char *
sppp_dotted_quad(uint32_t addr)
{
	static char s[16];
	snprintf(s, sizeof(s), "%d.%d.%d.%d",
		(int)((addr >> 24) & 0xff),
		(int)((addr >> 16) & 0xff),
		(int)((addr >> 8) & 0xff),
		(int)(addr & 0xff));
	return s;
}

/* a dummy, used to drop uninteresting events */
static void
sppp_null(struct sppp *unused)
{
	/* do just nothing */
}
/*
 * This file is large.  Tell emacs to highlight it nevertheless.
 *
 * Local Variables:
 * hilit-auto-highlight-maxout: 120000
 * End:
 */

/*	$NetBSD: if_pppvar.h,v 1.27 2008/02/20 17:05:53 matt Exp $	*/
/*	Id: if_pppvar.h,v 1.3 1996/07/01 01:04:37 paulus Exp	 */

/*
 * if_pppvar.h - private structures and declarations for PPP.
 *
 * Copyright (c) 1989-2002 Paul Mackerras. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name(s) of the authors of this software must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Paul Mackerras
 *     <paulus@samba.org>".
 *
 * THE AUTHORS OF THIS SOFTWARE DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Copyright (c) 1984-2000 Carnegie Mellon University. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any legal
 *    details, please contact
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _NET_IF_PPPVAR_H_
#define _NET_IF_PPPVAR_H_

#include <sys/callout.h>

/*
 * Supported network protocols.  These values are used for
 * indexing sc_npmode.
 */
#define NP_IP	0		/* Internet Protocol */
#define NP_IPV6	1		/* Internet Protocol version 6 */
#define NUM_NP	2		/* Number of NPs. */

/*
 * Structure describing each ppp unit.
 */
struct ppp_softc {
	struct	ifnet sc_if;		/* network-visible interface */
	int	sc_unit;		/* XXX unit number */
	u_int	sc_flags;		/* control/status bits; see if_ppp.h */
	void	*sc_devp;		/* pointer to device-dep structure */
	void	(*sc_start)(struct ppp_softc *);	/* start output proc */
	void	(*sc_ctlp)(struct ppp_softc *); /* rcvd control pkt */
	void	(*sc_relinq)(struct ppp_softc *); /* relinquish ifunit */
	struct	callout sc_timo_ch;	/* timeout callout */
	uint16_t sc_mru;		/* max receive unit */
	pid_t	sc_xfer;		/* used in transferring unit */
	struct	ifqueue sc_rawq;	/* received packets */
	struct	ifqueue sc_inq;		/* queue of input packets for daemon */
	struct	ifqueue sc_fastq;	/* interactive output packet q */
	struct	mbuf *sc_togo;		/* output packet ready to go */
	struct	mbuf *sc_npqueue;	/* output packets not to be sent yet */
	struct	mbuf **sc_npqtail;	/* ptr to last next ptr in npqueue */
	struct	pppstat sc_stats;	/* count of bytes/pkts sent/rcvd */
	enum	NPmode sc_npmode[NUM_NP]; /* what to do with each NP */
	struct	compressor *sc_xcomp;	/* transmit compressor */
	void	*sc_xc_state;		/* transmit compressor state */
	struct	compressor *sc_rcomp;	/* receive decompressor */
	void	*sc_rc_state;		/* receive decompressor state */
	time_t	sc_last_sent;		/* time (secs) last NP pkt sent */
	time_t	sc_last_recv;		/* time (secs) last NP pkt rcvd */
	void	*sc_si;			/* software interrupt handle */
#ifdef PPP_FILTER
	/* Filter for packets to pass. */
	struct	bpf_program sc_pass_filt_in;
	struct	bpf_program sc_pass_filt_out;

	/* Filter for "non-idle" packets. */
	struct	bpf_program sc_active_filt_in;
	struct	bpf_program sc_active_filt_out;
#endif /* PPP_FILTER */
#ifdef	VJC
	struct	slcompress *sc_comp; 	/* vjc control buffer */
#endif

	/* Device-dependent part for async lines. */
	ext_accm sc_asyncmap;		/* async control character map */
	uint32_t sc_rasyncmap;		/* receive async control char map */
	struct	mbuf *sc_outm;		/* mbuf chain currently being output */
	struct	mbuf *sc_m;		/* pointer to input mbuf chain */
	struct	mbuf *sc_mc;		/* pointer to current input mbuf */
	char	*sc_mp;			/* ptr to next char in input mbuf */
	uint16_t sc_ilen;		/* length of input packet so far */
	uint16_t sc_fcs;		/* FCS so far (input) */
	uint16_t sc_outfcs;		/* FCS so far for output packet */
	uint16_t sc_maxfastq;		/* Maximum number of packets that
					 * can be received back-to-back in
					 * the high priority queue */
	uint8_t sc_nfastq;		/* Number of packets received
					 * back-to-back in the high priority
					 * queue */
	u_char sc_rawin_start;		/* current char start */
	struct ppp_rawin sc_rawin;	/* chars as received */
	LIST_ENTRY(ppp_softc) sc_iflist;
};

#ifdef _KERNEL

struct	ppp_softc *pppalloc(pid_t);
void	pppdealloc(struct ppp_softc *);
int	pppioctl(struct ppp_softc *, u_long, void *, int, struct lwp *);
void	ppp_restart(struct ppp_softc *);
void	ppppktin(struct ppp_softc *, struct mbuf *, int);
struct	mbuf *ppp_dequeue(struct ppp_softc *);
int	pppoutput(struct ifnet *, struct mbuf *, const struct sockaddr *,
	    struct rtentry *);
#endif /* _KERNEL */

#endif /* !_NET_IF_PPPVAR_H_ */

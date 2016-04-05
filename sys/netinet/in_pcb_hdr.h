/*	$NetBSD: in_pcb_hdr.h,v 1.11 2014/05/30 01:39:03 christos Exp $	*/

/*
 * Copyright (C) 2003 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)in_pcb.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _NETINET_IN_PCB_HDR_H_
#define _NETINET_IN_PCB_HDR_H_

#include <sys/queue.h>

struct inpcbpolicy;

/*
 * align it with inpcb and in6pcb!
 */
struct inpcb_hdr {
	LIST_ENTRY(inpcb_hdr) inph_hash;
	LIST_ENTRY(inpcb_hdr) inph_lhash;
	TAILQ_ENTRY(inpcb_hdr) inph_queue;
	int	  inph_af;		/* address family - AF_INET */
	void *	  inph_ppcb;		/* pointer to per-protocol pcb */
	int	  inph_state;		/* bind/connect state */
	int       inph_portalgo;
	struct	  socket *inph_socket;	/* back pointer to socket */
	struct	  inpcbtable *inph_table;
	struct	  inpcbpolicy *inph_sp;	/* security policy */
};

#define	sotoinpcb_hdr(so)	((struct inpcb_hdr *)(so)->so_pcb)

LIST_HEAD(inpcbhead, inpcb_hdr);

struct vestigial_inpcb;

/* Hooks for vestigial pcb entries.
 * If vestigial entries exist for a table (TCP only)
 * the vestigial pointer is set.
 */
typedef struct vestigial_hooks {
	/* IPv4 hooks */
	void	*(*init_ports4)(struct in_addr, u_int, int);
	int	(*next_port4)(void *, struct vestigial_inpcb *);
	int	(*lookup4)(struct in_addr, uint16_t,
			   struct in_addr, uint16_t,
			   struct vestigial_inpcb *);
	/* IPv6 hooks */
	void	*(*init_ports6)(const struct in6_addr*, u_int, int);
	int	(*next_port6)(void *, struct vestigial_inpcb *);
	int	(*lookup6)(const struct in6_addr *, uint16_t,
			   const struct in6_addr *, uint16_t,
			   struct vestigial_inpcb *);
} vestigial_hooks_t;

TAILQ_HEAD(inpcbqueue, inpcb_hdr);

struct inpcbtable {
	struct	  inpcbqueue inpt_queue;
	struct	  inpcbhead *inpt_porthashtbl;
	struct	  inpcbhead *inpt_bindhashtbl;
	struct	  inpcbhead *inpt_connecthashtbl;
	u_long	  inpt_porthash;
	u_long	  inpt_bindhash;
	u_long	  inpt_connecthash;
	u_int16_t inpt_lastport;
	u_int16_t inpt_lastlow;

	vestigial_hooks_t *vestige;
};
#define inpt_lasthi inpt_lastport

/* states in inp_state: */
#define	INP_ATTACHED		0
#define	INP_BOUND		1
#define	INP_CONNECTED		2

#endif /* !_NETINET_IN_PCB_HDR_H_ */

/*	$NetBSD: portalgo.c,v 1.9 2015/08/24 22:21:26 pooka Exp $	*/

/*
 * Copyright 2011 Vlad Balan
 *
 * Written by Vlad Balan for the NetBSD Foundation.
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
 */

/*
 * see:
 *	RFC 6056 Recommendations for Transport-Protocol Port Randomization
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: portalgo.c,v 1.9 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/kauth.h>
#include <sys/uidinfo.h>
#include <sys/domain.h>
#include <sys/md5.h>
#include <sys/cprng.h>
#include <sys/bitops.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_pcb.h>
#endif

#include <netinet/tcp_vtw.h>

#include "portalgo.h"

#define NPROTO 2
#define PORTALGO_TCP 0
#define PORTALGO_UDP 1

#define NAF 2
#define PORTALGO_IPV4 0
#define PORTALGO_IPV6 1

#define NRANGES 2
#define PORTALGO_LOWPORT 0
#define PORTALGO_HIGHPORT 1

#if PORTALGO_DEBUG
static bool portalgo_debug = true;
#define DPRINTF if (portalgo_debug) printf
#else
#define DPRINTF while (/*CONSTCOND*/0) printf
#endif

#ifndef PORTALGO_INET4_DEFAULT
#define PORTALGO_INET4_DEFAULT PORTALGO_BSD
#endif
#ifndef PORTALGO_INET6_DEFAULT
#define PORTALGO_INET6_DEFAULT PORTALGO_BSD
#endif

typedef __BITMAP_TYPE(, uint32_t, 0x10000) bitmap;
#ifdef INET
static int inet4_portalgo = PORTALGO_INET4_DEFAULT;
static bitmap inet4_reserve;
#endif
#ifdef INET6
static int inet6_portalgo = PORTALGO_INET6_DEFAULT;
static bitmap inet6_reserve;
#endif

typedef struct {
	const char *name;
	int (*func)(int, uint16_t *, struct inpcb_hdr *, kauth_cred_t);
} portalgo_algorithm_t;

static int algo_bsd(int, uint16_t *, struct inpcb_hdr *, kauth_cred_t);
static int algo_random_start(int, uint16_t *, struct inpcb_hdr *, kauth_cred_t);
static int algo_random_pick(int, uint16_t *, struct inpcb_hdr *, kauth_cred_t);
static int algo_hash(int, uint16_t *, struct inpcb_hdr *, kauth_cred_t);
static int algo_doublehash(int, uint16_t *, struct inpcb_hdr *, kauth_cred_t);
static int algo_randinc(int, uint16_t *, struct inpcb_hdr *, kauth_cred_t);

static const portalgo_algorithm_t algos[] = {
	{
		.name = "bsd",
		.func = algo_bsd
	},
	{
		.name = "random_start",
		.func = algo_random_start
	},
	{
		.name = "random_pick",
		.func = algo_random_pick
	},
	{
		.name = "hash",
		.func = algo_hash
	},
	{
		.name = "doublehash",
		.func = algo_doublehash
	},
	{
		.name = "randinc",
		.func = algo_randinc
	}
};

#define NALGOS __arraycount(algos)

static uint16_t portalgo_next_ephemeral[NPROTO][NAF][NRANGES][NALGOS];

/*
 * Access the pcb and copy the values of the last port and the ends of
 * the port range.
 */
static int
pcb_getports(struct inpcb_hdr *inp_hdr, uint16_t *lastport,
    uint16_t *mymin, uint16_t *mymax, uint16_t **pnext_ephemeral, int algo)
{
	struct inpcbtable * const table = inp_hdr->inph_table;
	struct socket *so;
	int portalgo_proto;
	int portalgo_af;
	int portalgo_range;

	so = inp_hdr->inph_socket;
	switch (so->so_type) {
	case SOCK_DGRAM: /* UDP or DCCP */
	case SOCK_CONN_DGRAM:
		portalgo_proto = PORTALGO_UDP;
		break;
	case SOCK_STREAM: /* TCP or SCTP */
		portalgo_proto = PORTALGO_TCP;
		break;
	default:
		return EPFNOSUPPORT;
	}

	switch (inp_hdr->inph_af) {
#ifdef INET
	case AF_INET: {
		struct inpcb *inp = (struct inpcb *)(void *)inp_hdr;

		portalgo_af = PORTALGO_IPV4;
		if (inp->inp_flags & INP_LOWPORT) {
			*mymin = lowportmin;
			*mymax = lowportmax;
			*lastport = table->inpt_lastlow;
			portalgo_range = PORTALGO_LOWPORT;
		} else {
			*mymin = anonportmin;
			*mymax = anonportmax;
			*lastport = table->inpt_lastport;
			portalgo_range = PORTALGO_HIGHPORT;
		}
		break;
	}
#endif
#ifdef INET6
	case AF_INET6: {
		struct in6pcb *in6p = (struct in6pcb *)(void *)inp_hdr;

		portalgo_af = PORTALGO_IPV6;
		if (in6p->in6p_flags & IN6P_LOWPORT) {
			*mymin = ip6_lowportmin;
			*mymax = ip6_lowportmax;
			*lastport = table->inpt_lastlow;
			portalgo_range = PORTALGO_LOWPORT;
		} else {
			*mymin = ip6_anonportmin;
			*mymax = ip6_anonportmax;
			*lastport = table->inpt_lastport;
			portalgo_range = PORTALGO_HIGHPORT;
		}
		break;
	}
#endif
	default:
		return EAFNOSUPPORT;
	}

	if (*mymin > *mymax) {	/* sanity check */
		u_int16_t swp;

		swp = *mymin;
		*mymin = *mymax;
		*mymax = swp;
	}

	DPRINTF("%s mymin:%d mymax:%d lastport:%d\n", __func__,
	    *mymin, *mymax, *lastport);

	*pnext_ephemeral = &portalgo_next_ephemeral[portalgo_proto]
	    [portalgo_af][portalgo_range][algo];

	DPRINTF("%s portalgo_proto:%d portalgo_af:%d portalgo_range:%d\n",
	    __func__, portalgo_proto, portalgo_af, portalgo_range);
	return 0;
}

/*
 * Check whether the port picked by the port randomizer is available
 * and whether KAUTH approves of our choice. This part of the code
 * shamelessly copied from in_pcb.c.
 */
static bool
check_suitable_port(uint16_t port, struct inpcb_hdr *inp_hdr, kauth_cred_t cred)
{
	struct inpcbtable * const table = inp_hdr->inph_table;
#ifdef INET
	vestigial_inpcb_t vestigial;
#endif
	int error;
#ifdef INET6
	struct socket *so;
	int wild = 0;
#endif

	DPRINTF("%s called for argument %d\n", __func__, port);

	switch (inp_hdr->inph_af) {
#ifdef INET
	case AF_INET: { /* IPv4 */
		struct inpcb *inp = (struct inpcb *)(void *)inp_hdr;
		struct inpcb *pcb;
		struct sockaddr_in sin;

		if (__BITMAP_ISSET(port, &inet4_reserve))
			return false;

		sin.sin_addr = inp->inp_laddr;
		pcb = in_pcblookup_port(table, sin.sin_addr, htons(port), 1,
		    &vestigial);

		DPRINTF("%s in_pcblookup_port returned %p and "
		    "vestigial.valid %d\n",
		    __func__, pcb, vestigial.valid);

		if ((!pcb) && (!vestigial.valid)) {
			enum kauth_network_req req;

			/* We have a free port. Check with the secmodel. */
			if (inp->inp_flags & INP_LOWPORT) {
#ifndef IPNOPRIVPORTS
				req = KAUTH_REQ_NETWORK_BIND_PRIVPORT;
#else
				req = KAUTH_REQ_NETWORK_BIND_PORT;
#endif
			} else
				req = KAUTH_REQ_NETWORK_BIND_PORT;

			sin.sin_port = port;
			error = kauth_authorize_network(cred,
			    KAUTH_NETWORK_BIND,
			    req, inp->inp_socket, &sin, NULL);
			DPRINTF("%s kauth_authorize_network returned %d\n",
			    __func__, error);

			if (error == 0) {
				DPRINTF("%s port approved\n", __func__);
				return true;	/* KAUTH agrees */
			}
		}
		break;
	}
#endif
#ifdef INET6
	case AF_INET6: { /* IPv6 */
		struct in6pcb *in6p = (struct in6pcb *)(void *)inp_hdr;
		struct sockaddr_in6 sin6;
		void *t;

		if (__BITMAP_ISSET(port, &inet6_reserve))
			return false;

		sin6.sin6_addr = in6p->in6p_laddr;
		so = in6p->in6p_socket;

		/* XXX: this is redundant when called from in6_pcbbind */
		if ((so->so_options & (SO_REUSEADDR|SO_REUSEPORT)) == 0 &&
		    ((so->so_proto->pr_flags & PR_CONNREQUIRED) == 0 ||
			(so->so_options & SO_ACCEPTCONN) == 0))
			wild = 1;

#ifdef INET
		if (IN6_IS_ADDR_V4MAPPED(&sin6.sin6_addr)) {
			t = in_pcblookup_port(table,
			    *(struct in_addr *)&sin6.sin6_addr.s6_addr32[3],
			    htons(port), wild, &vestigial);
			if (!t && vestigial.valid) {
				DPRINTF("%s in_pcblookup_port returned "
				    "a result\n", __func__);
				return false;
			}
		} else
#endif
		{
			t = in6_pcblookup_port(table, &sin6.sin6_addr,
			    htons(port), wild, &vestigial);
			if (!t && vestigial.valid) {
				DPRINTF("%s in6_pcblookup_port returned "
				    "a result\n", __func__);
				return false;
			}
		}
		if (t == NULL) {
			enum kauth_network_req req;

			/* We have a free port. Check with the secmodel. */
			if (in6p->in6p_flags & IN6P_LOWPORT) {
#ifndef IPNOPRIVPORTS
				req = KAUTH_REQ_NETWORK_BIND_PRIVPORT;
#else
				req = KAUTH_REQ_NETWORK_BIND_PORT;
#endif
			} else {
				req = KAUTH_REQ_NETWORK_BIND_PORT;
			}

			sin6.sin6_port = port;
			error = kauth_authorize_network(cred,
			    KAUTH_NETWORK_BIND, req, so, &sin6, NULL);
			if (error) {
				/* Secmodel says no. Keep looking. */
				DPRINTF("%s secmodel says no\n", __func__);
				return false;
			}
			DPRINTF("%s port approved\n", __func__);
			return true;
		}
		break;
	}
#endif
	default:
		DPRINTF("%s unknown address family\n", __func__);
		return false;
	}
	return false;
}

/* This is the default BSD algorithm, as described in RFC 6056 */
static int
algo_bsd(int algo, uint16_t *port, struct inpcb_hdr *inp_hdr, kauth_cred_t cred)
{
	uint16_t count;
	uint16_t mymin, mymax, lastport;
	uint16_t *next_ephemeral;
	int error;

	DPRINTF("%s called\n", __func__);
	error = pcb_getports(inp_hdr, &lastport, &mymin, &mymax,
	    &next_ephemeral, algo);
	if (error)
		return error;
	count = mymax - mymin + 1;
	do {
		uint16_t myport = *next_ephemeral;

		if (myport < mymin || mymax < myport)
			myport = mymax;
		*next_ephemeral = myport - 1;
		if (check_suitable_port(myport, inp_hdr, cred)) {
			*port = myport;
			DPRINTF("%s returning port %d\n", __func__, *port);
			return 0;
		}
		count--;
	} while (count > 0);

	DPRINTF("%s returning EAGAIN\n", __func__);
	return EAGAIN;
}

/*
 * The straightforward algorithm that increments the port number
 * by a random amount.
 */
static int
algo_random_start(int algo, uint16_t *port, struct inpcb_hdr *inp_hdr,
    kauth_cred_t cred)
{
	uint16_t count, num_ephemeral;
	uint16_t mymin, mymax, lastport;
	uint16_t *next_ephemeral;
	int error;

	DPRINTF("%s called\n", __func__);

	error = pcb_getports(inp_hdr, &lastport, &mymin, &mymax,
	    &next_ephemeral, algo);
	if (error)
		return error;

	num_ephemeral = mymax - mymin + 1;

	DPRINTF("num_ephemeral: %u\n", num_ephemeral);

	*next_ephemeral = mymin + (cprng_fast32() % num_ephemeral);

	DPRINTF("next_ephemeral initially: %u\n", *next_ephemeral);

	count = num_ephemeral;

	do {
		if (check_suitable_port(*next_ephemeral, inp_hdr, cred)) {
			*port = *next_ephemeral;
			DPRINTF("%s returning port %d\n", __func__, *port);
			return 0;
		}
		if (*next_ephemeral == mymax) {
			*next_ephemeral = mymin;
		} else
			(*next_ephemeral)++;

		count--;


		DPRINTF("next_ephemeral: %u count: %u\n", *next_ephemeral,
		    count);

	} while (count > 0);

	DPRINTF("%s returning EINVAL\n", __func__);

	return EINVAL;
}

/*
 * Since there is no state kept on the ports tried, we might actually
 * give up before exhausting the free ports.
 */
static int
algo_random_pick(int algo, uint16_t *port, struct inpcb_hdr *inp_hdr,
    kauth_cred_t cred)
{
	uint16_t count, num_ephemeral;
	uint16_t mymin, mymax, lastport;
	uint16_t *next_ephemeral;
	int error;

	DPRINTF("%s called\n", __func__);

	error = pcb_getports(inp_hdr, &lastport, &mymin, &mymax,
	    &next_ephemeral, algo);
	if (error)
		return error;

	num_ephemeral = mymax - mymin + 1;

	DPRINTF("num_ephemeral: %u\n", num_ephemeral);
	*next_ephemeral = mymin + (cprng_fast32() % num_ephemeral);

	DPRINTF("next_ephemeral initially: %u\n", *next_ephemeral);

	count = num_ephemeral;

	do {
		if (check_suitable_port(*next_ephemeral, inp_hdr, cred)) {
			*port = *next_ephemeral;
			DPRINTF("%s returning port %d\n", __func__, *port);
			return 0;
		}
		*next_ephemeral = mymin +
		    (cprng_fast32() % num_ephemeral);

		count--;

		DPRINTF("next_ephemeral: %u count: %u\n",
		    *next_ephemeral, count);
	} while (count > 0);

	DPRINTF("%s returning EINVAL\n", __func__);

	return EINVAL;
}

/* This is the implementation from FreeBSD, with tweaks */
static uint16_t
Fhash(const struct inpcb_hdr *inp_hdr)
{
	MD5_CTX f_ctx;
	uint32_t Ff[4];
	uint32_t secret_f[4];
	uint32_t offset;
	uint16_t soffset[2];

	cprng_fast(secret_f, sizeof(secret_f));

	MD5Init(&f_ctx);
	switch (inp_hdr->inph_af) {
#ifdef INET
	case AF_INET: {
		const struct inpcb *inp =
		    (const struct inpcb *)(const void *)inp_hdr;
		MD5Update(&f_ctx, (const u_char *)&inp->inp_laddr,
		    sizeof(inp->inp_laddr));
		MD5Update(&f_ctx, (const u_char *)&inp->inp_faddr,
		    sizeof(inp->inp_faddr));
		MD5Update(&f_ctx, (const u_char *)&inp->inp_fport,
		    sizeof(inp->inp_fport));
		break;
	}
#endif
#ifdef INET6
	case AF_INET6: {
		const struct in6pcb *in6p =
		    (const struct in6pcb *)(const void *)inp_hdr;
		MD5Update(&f_ctx, (const u_char *)&in6p->in6p_laddr,
		    sizeof(in6p->in6p_laddr));
		MD5Update(&f_ctx, (const u_char *)&in6p->in6p_faddr,
		    sizeof(in6p->in6p_faddr));
		MD5Update(&f_ctx, (const u_char *)&in6p->in6p_fport,
		    sizeof(in6p->in6p_fport));
		break;
	}
#endif
	default:
		break;
	}
	MD5Update(&f_ctx, (const u_char *)secret_f, sizeof(secret_f));
	MD5Final((u_char *)&Ff, &f_ctx);

	offset = (Ff[0] ^ Ff[1]) ^ (Ff[2] ^ Ff[3]);

	memcpy(&soffset, &offset, sizeof(soffset));

	return soffset[0] ^ soffset[1];
}

/*
 * Checks whether the tuple is complete. If not, marks the pcb for
 * late binding.
 */
static bool
iscompletetuple(struct inpcb_hdr *inp_hdr)
{
#ifdef INET6
	struct in6pcb *in6p;
#endif

	switch (inp_hdr->inph_af) {
#ifdef INET
	case AF_INET: {
		struct inpcb *inp = (struct inpcb *)(void *)inp_hdr;
		if (inp->inp_fport == 0 || in_nullhost(inp->inp_faddr)) {
			DPRINTF("%s fport or faddr missing, delaying port "
			    "to connect/send\n", __func__);
			inp->inp_bindportonsend = true;
			return false;
		} else {
			inp->inp_bindportonsend = false;
		}
		break;
	}
#endif
#ifdef INET6
	case AF_INET6: {
		in6p = (struct in6pcb *)(void *)inp_hdr;
		if (in6p->in6p_fport == 0 || memcmp(&in6p->in6p_faddr,
		    &in6addr_any, sizeof(in6p->in6p_faddr)) == 0) {
			DPRINTF("%s fport or faddr missing, delaying port "
			    "to connect/send\n", __func__);
			in6p->in6p_bindportonsend = true;
			return false;
		} else {
			in6p->in6p_bindportonsend = false;
		}
		break;
	}
#endif
	default:
		DPRINTF("%s incorrect address family\n", __func__);
		return false;
	}

	return true;
}

static int
algo_hash(int algo, uint16_t *port, struct inpcb_hdr *inp_hdr,
    kauth_cred_t cred)
{
	uint16_t count, num_ephemeral;
	uint16_t mymin, mymax, lastport;
	uint16_t *next_ephemeral;
	uint16_t offset, myport;
	int error;

	DPRINTF("%s called\n", __func__);

	error = pcb_getports(inp_hdr, &lastport, &mymin, &mymax,
	    &next_ephemeral, algo);
	if (error)
		return error;

	if (!iscompletetuple(inp_hdr)) {
		*port = 0;
		return 0;
	}

	/* Ephemeral port selection function */
	num_ephemeral = mymax - mymin + 1;

	DPRINTF("num_ephemeral: %d\n", num_ephemeral);

	offset = Fhash(inp_hdr);

	count = num_ephemeral;
	do {
		myport = mymin + (*next_ephemeral + offset)
		    % num_ephemeral;

		(*next_ephemeral)++;

		if (check_suitable_port(myport, inp_hdr, cred)) {
			*port = myport;
			DPRINTF("%s returning port %d\n", __func__, *port);
			return 0;
		}
		count--;
	} while (count > 0);

	DPRINTF("%s returning EINVAL\n", __func__);

	return EINVAL;
}

static int
algo_doublehash(int algo, uint16_t *port, struct inpcb_hdr *inp_hdr,
    kauth_cred_t cred)
{
	uint16_t count, num_ephemeral;
	uint16_t mymin, mymax, lastport;
	uint16_t *next_ephemeral;
	uint16_t offset, myport;
	static uint16_t dhtable[8];
	size_t idx;
	int error;

	DPRINTF("%s called\n", __func__);

	error = pcb_getports(inp_hdr, &lastport, &mymin, &mymax,
	    &next_ephemeral, algo);
	if (error)
		return error;

	if (!iscompletetuple(inp_hdr)) {
		*port = 0;
		return 0;
	}
	/* first time initialization */
	if (dhtable[0] == 0)
		for (size_t i = 0; i < __arraycount(dhtable); i++)
			dhtable[i] = cprng_fast32() & 0xffff;

	/* Ephemeral port selection function */
	num_ephemeral = mymax - mymin + 1;
	offset = Fhash(inp_hdr);
	idx = Fhash(inp_hdr) % __arraycount(dhtable);	/* G */
	count = num_ephemeral;

	do {
		myport = mymin + (offset + dhtable[idx])
		    % num_ephemeral;
		dhtable[idx]++;

		if (check_suitable_port(myport, inp_hdr, cred)) {
			*port = myport;
			DPRINTF("%s returning port %d\n", __func__, *port);
			return 0;
		}
		count--;

	} while (count > 0);

	DPRINTF("%s returning EINVAL\n", __func__);

	return EINVAL;
}

static int
algo_randinc(int algo, uint16_t *port, struct inpcb_hdr *inp_hdr,
    kauth_cred_t cred)
{
	static const uint16_t N = 500;	/* Determines the trade-off */
	uint16_t count, num_ephemeral;
	uint16_t mymin, mymax, lastport;
	uint16_t *next_ephemeral;
	uint16_t myport;
	int error;

	DPRINTF("%s called\n", __func__);

	error = pcb_getports(inp_hdr, &lastport, &mymin, &mymax,
	    &next_ephemeral, algo);
	if (error)
		return error;

	if (*next_ephemeral == 0)
		*next_ephemeral = cprng_fast32() & 0xffff;

	/* Ephemeral port selection function */
	num_ephemeral = mymax - mymin + 1;

	count = num_ephemeral;
	do {
		*next_ephemeral = *next_ephemeral +
		    (cprng_fast32() % N) + 1;
		myport = mymin +
		    (*next_ephemeral % num_ephemeral);

		if (check_suitable_port(myport, inp_hdr, cred)) {
			*port = myport;
			DPRINTF("%s returning port %d\n", __func__, *port);
			return 0;
		}
		count--;
	} while (count > 0);

	return EINVAL;
}

/* The generic function called in order to pick a port. */
int
portalgo_randport(uint16_t *port, struct inpcb_hdr *inp_hdr, kauth_cred_t cred)
{
	int algo, error;
	uint16_t lport;
	int default_algo;

	DPRINTF("%s called\n", __func__);

	if (inp_hdr->inph_portalgo == PORTALGO_DEFAULT) {
		switch (inp_hdr->inph_af) {
#ifdef INET
		case AF_INET:
			default_algo = inet4_portalgo;
			break;
#endif
#ifdef INET6
		case AF_INET6:
			default_algo = inet6_portalgo;
			break;
#endif
		default:
			return EINVAL;
		}

		if (default_algo == PORTALGO_DEFAULT)
			algo = PORTALGO_BSD;
		else
			algo = default_algo;
	}
	else /* socket specifies the algorithm */
		algo = inp_hdr->inph_portalgo;

	KASSERT(algo >= 0);
	KASSERT(algo < NALGOS);

	switch (inp_hdr->inph_af) {
#ifdef INET
	case AF_INET: {
		char buf[INET_ADDRSTRLEN];
		struct inpcb *inp = (struct inpcb *)(void *)inp_hdr;
		DPRINTF("local addr: %s\n", IN_PRINT(buf, &inp->inp_laddr));
		DPRINTF("local port: %d\n", inp->inp_lport);
		DPRINTF("foreign addr: %s\n", IN_PRINT(buf, &inp->inp_faddr));
		DPRINTF("foreign port: %d\n", inp->inp_fport);
		break;
	}
#endif
#ifdef INET6
	case AF_INET6: {
		char buf[INET6_ADDRSTRLEN];
		struct in6pcb *in6p = (struct in6pcb *)(void *)inp_hdr;

		DPRINTF("local addr: %s\n", IN6_PRINT(buf, &in6p->in6p_laddr));
		DPRINTF("local port: %d\n", in6p->in6p_lport);
		DPRINTF("foreign addr: %s\n", IN6_PRINT(buf,
		    &in6p->in6p_laddr));
		DPRINTF("foreign port: %d\n", in6p->in6p_fport);
		break;
	}
#endif
	default:
		break;
	}

	DPRINTF("%s portalgo = %d\n", __func__, algo);

	error = (*algos[algo].func)(algo, &lport, inp_hdr, cred);
	if (error == 0) {
		*port = lport;
	} else if (error != EAGAIN) {
		uint16_t lastport, mymin, mymax, *pnext_ephemeral;

		error = pcb_getports(inp_hdr, &lastport, &mymin,
		    &mymax, &pnext_ephemeral, algo);
		if (error)
			return error;
		*port = lastport - 1;
	}
	return error;
}

/* Sets the algorithm to be used globally */
static int
portalgo_algo_name_select(const char *name, int *algo)
{
	size_t ai;

	DPRINTF("%s called\n", __func__);

	for (ai = 0; ai < NALGOS; ai++)
		if (strcmp(algos[ai].name, name) == 0) {
			DPRINTF("%s: found idx %zu\n", __func__, ai);
			*algo = ai;
			return 0;
		}
	return EINVAL;
}

/* Sets the algorithm to be used by the pcb inp. */
int
portalgo_algo_index_select(struct inpcb_hdr *inp, int algo)
{

	DPRINTF("%s called with algo %d for pcb %p\n", __func__, algo, inp );

	if ((algo < 0 || algo >= NALGOS) &&
	    (algo != PORTALGO_DEFAULT))
		return EINVAL;

	inp->inph_portalgo = algo;
	return 0;
}

/*
 * The sysctl hook that is supposed to check that we are picking one
 * of the valid algorithms.
 */
static int
sysctl_portalgo_selected(SYSCTLFN_ARGS, int *algo)
{
	struct sysctlnode node;
	int error;
	char newalgo[PORTALGO_MAXLEN];

	DPRINTF("%s called\n", __func__);

	strlcpy(newalgo, algos[*algo].name, sizeof(newalgo));

	node = *rnode;
	node.sysctl_data = newalgo;
	node.sysctl_size = sizeof(newalgo);

	error = sysctl_lookup(SYSCTLFN_CALL(&node));

	DPRINTF("newalgo: %s\n", newalgo);

	if (error || newp == NULL ||
	    strncmp(newalgo, algos[*algo].name, sizeof(newalgo)) == 0)
		return error;

#ifdef KAUTH_NETWORK_SOCKET_PORT_RANDOMIZE
	if (l != NULL && (error = kauth_authorize_system(l->l_cred,
	    KAUTH_NETWORK_SOCKET, KAUTH_NETWORK_SOCKET_PORT_RANDOMIZE, newname,
	    NULL, NULL)) != 0)
		return error;
#endif

	mutex_enter(softnet_lock);
	error = portalgo_algo_name_select(newalgo, algo);
	mutex_exit(softnet_lock);
	return error;
}

static int
sysctl_portalgo_reserve(SYSCTLFN_ARGS, bitmap *bt)
{
	struct sysctlnode node;
	int error;

	DPRINTF("%s called\n", __func__);

	node = *rnode;
	node.sysctl_data = bt;
	node.sysctl_size = sizeof(*bt);

	error = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (error || newp == NULL)
		return error;

#ifdef KAUTH_NETWORK_SOCKET_PORT_RESERVE
	if (l != NULL && (error = kauth_authorize_system(l->l_cred,
	    KAUTH_NETWORK_SOCKET, KAUTH_NETWORK_SOCKET_PORT_RESERVE, bt,
	    NULL, NULL)) != 0)
		return error;
#endif
	return error;
}

#ifdef INET
/*
 * The sysctl hook that is supposed to check that we are picking one
 * of the valid algorithms.
 */
int
sysctl_portalgo_selected4(SYSCTLFN_ARGS)
{

	return sysctl_portalgo_selected(SYSCTLFN_CALL(rnode), &inet4_portalgo);
}

int
sysctl_portalgo_reserve4(SYSCTLFN_ARGS)
{

	return sysctl_portalgo_reserve(SYSCTLFN_CALL(rnode), &inet4_reserve);
}
#endif

#ifdef INET6
int
sysctl_portalgo_selected6(SYSCTLFN_ARGS)
{

	return sysctl_portalgo_selected(SYSCTLFN_CALL(rnode), &inet6_portalgo);
}

int
sysctl_portalgo_reserve6(SYSCTLFN_ARGS)
{
	return sysctl_portalgo_reserve(SYSCTLFN_CALL(rnode), &inet6_reserve);
}
#endif

/*
 * The sysctl hook that returns the available
 * algorithms.
 */
int
sysctl_portalgo_available(SYSCTLFN_ARGS)
{
	size_t ai, len = 0;
	struct sysctlnode node;
	char availalgo[NALGOS * PORTALGO_MAXLEN];

	DPRINTF("%s called\n", __func__);

	availalgo[0] = '\0';

	for (ai = 0; ai < NALGOS; ai++) {
		len = strlcat(availalgo, algos[ai].name, sizeof(availalgo));
		if (ai < NALGOS - 1)
			strlcat(availalgo, " ", sizeof(availalgo));
	}

	DPRINTF("available algos: %s\n", availalgo);

	node = *rnode;
	node.sysctl_data = availalgo;
	node.sysctl_size = len;

	return sysctl_lookup(SYSCTLFN_CALL(&node));
}

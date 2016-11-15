/* $NetBSD: tcp_sack.c,v 1.32 2015/08/24 22:21:26 pooka Exp $ */

/*
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Kentaro A. Kurahone.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1994, 1995
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
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)tcp_sack.c	8.12 (Berkeley) 5/24/95
 * $FreeBSD: src/sys/netinet/tcp_sack.c,v 1.3.2.2 2004/12/25 23:02:57 rwatson Exp $
 */

/*
 *	@@(#)COPYRIGHT	1.1 (NRL) 17 January 1995
 *
 * NRL grants permission for redistribution and use in source and binary
 * forms, with or without modification, of the software and documentation
 * created at NRL provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgements:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 *	This product includes software developed at the Information
 *	Technology Division, US Naval Research Laboratory.
 * 4. Neither the name of the NRL nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THE SOFTWARE PROVIDED BY NRL IS PROVIDED BY NRL AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL NRL OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the US Naval
 * Research Laboratory (NRL).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tcp_sack.c,v 1.32 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#include "opt_inet_csum.h"
#include "opt_tcp_debug.h"
#include "opt_ddb.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/pool.h>
#include <sys/domain.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_var.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>
#endif

#ifndef INET6
/* always need ip6.h for IP6_EXTHDR_GET */
#include <netinet/ip6.h>
#endif

#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_debug.h>

/* SACK block pool. */
static struct pool sackhole_pool;

void
tcp_sack_init(void)
{

	pool_init(&sackhole_pool, sizeof(struct sackhole), 0, 0, 0,
	    "sackholepl", NULL, IPL_SOFTNET);
}

static struct sackhole *
sack_allochole(struct tcpcb *tp)
{
	struct sackhole *hole;

	if (tp->snd_numholes >= tcp_sack_tp_maxholes ||
	    tcp_sack_globalholes >= tcp_sack_globalmaxholes) {
		return NULL;
	}
	hole = pool_get(&sackhole_pool, PR_NOWAIT);
	if (hole == NULL) {
		return NULL;
	}
	tp->snd_numholes++;
	tcp_sack_globalholes++;

	return hole;
}

static struct sackhole *
sack_inserthole(struct tcpcb *tp, tcp_seq start, tcp_seq end,
    struct sackhole *prev)
{
	struct sackhole *hole;

	hole = sack_allochole(tp);
	if (hole == NULL) {
		return NULL;
	}
	hole->start = hole->rxmit = start;
	hole->end = end;
	if (prev != NULL) {
		TAILQ_INSERT_AFTER(&tp->snd_holes, prev, hole, sackhole_q);
	} else {
		TAILQ_INSERT_TAIL(&tp->snd_holes, hole, sackhole_q);
	}
	return hole;
}

static struct sackhole *
sack_removehole(struct tcpcb *tp, struct sackhole *hole)
{
	struct sackhole *next;

	next = TAILQ_NEXT(hole, sackhole_q);
	tp->snd_numholes--;
	tcp_sack_globalholes--;
	TAILQ_REMOVE(&tp->snd_holes, hole, sackhole_q);
	pool_put(&sackhole_pool, hole);

	return next;
}

/*
 * tcp_new_dsack: record the reception of a duplicated segment.
 */

void
tcp_new_dsack(struct tcpcb *tp, tcp_seq seq, u_int32_t len)
{

	if (TCP_SACK_ENABLED(tp)) {
		tp->rcv_dsack_block.left = seq;
		tp->rcv_dsack_block.right = seq + len;
		tp->rcv_sack_flags |= TCPSACK_HAVED;
	}
}

/*
 * tcp_sack_option: parse the given SACK option and update the scoreboard.
 */

void
tcp_sack_option(struct tcpcb *tp, const struct tcphdr *th, const u_char *cp,
    int optlen)
{
	struct sackblk
	    t_sack_block[(MAX_TCPOPTLEN - 2) / (sizeof(u_int32_t) * 2)];
	struct sackblk *sack = NULL;
	struct sackhole *cur = NULL;
	struct sackhole *tmp = NULL;
	const char *lp = cp + 2;
	int i, j, num_sack_blks;
	tcp_seq left, right, acked;

	/*
	 * If we aren't processing SACK responses, this is not an ACK
	 * or the peer sends us a sack option with invalid length, don't
	 * update the scoreboard.
	 */
	if (!TCP_SACK_ENABLED(tp) || ((th->th_flags & TH_ACK) == 0) ||
			(optlen % 8 != 2 || optlen < 10)) {
		return;
	}

	/*
	 * If we don't want any SACK holes to be allocated, just return.
	 */
	if (tcp_sack_globalmaxholes == 0 || tcp_sack_tp_maxholes == 0) {
		return;
	}

	/* If the ACK is outside [snd_una, snd_max], ignore the SACK options. */
	if (SEQ_LT(th->th_ack, tp->snd_una) || SEQ_GT(th->th_ack, tp->snd_max))
		return;

	/*
	 * Extract SACK blocks.
	 *
	 * Note that t_sack_block is sorted so that we only need to do
	 * one pass over the sequence number space. (SACK "fast-path")
	 */
	num_sack_blks = optlen / 8;
	acked = (SEQ_GT(th->th_ack, tp->snd_una)) ? th->th_ack : tp->snd_una;
	for (i = 0; i < num_sack_blks; i++, lp += sizeof(uint32_t) * 2) {
		memcpy(&left, lp, sizeof(uint32_t));
		memcpy(&right, lp + sizeof(uint32_t), sizeof(uint32_t));
		left = ntohl(left);
		right = ntohl(right);

		if (SEQ_LEQ(right, acked) || SEQ_GT(right, tp->snd_max) ||
		    SEQ_GEQ(left, right)) {
			/* SACK entry that's old, or invalid. */
			i--;
			num_sack_blks--;
			continue;
		}

		/* Insertion sort. */
		for (j = i; (j > 0) && SEQ_LT(left, t_sack_block[j - 1].left);
		    j--) {
			t_sack_block[j].left = t_sack_block[j - 1].left;
			t_sack_block[j].right = t_sack_block[j - 1].right;
		}
		t_sack_block[j].left = left;
		t_sack_block[j].right = right;
	}

	/* Update the scoreboard. */
	cur = TAILQ_FIRST(&tp->snd_holes);
	for (i = 0; i < num_sack_blks; i++) {
		sack = &t_sack_block[i];
		/*
		 * FACK TCP.  Update snd_fack so we can enter Fast
		 * Recovery early.
		 */
		if (SEQ_GEQ(sack->right, tp->snd_fack))
			tp->snd_fack = sack->right;

		if (TAILQ_EMPTY(&tp->snd_holes)) {
			/* First hole. */
			cur = sack_inserthole(tp, th->th_ack, sack->left, NULL);
			if (cur == NULL) {
				/* ENOBUFS, bail out*/
				return;
			}
			tp->rcv_lastsack = sack->right;
			continue; /* With next sack block */
		}

		/* Go through the list of holes. */
		while (cur) {
			if (SEQ_LEQ(sack->right, cur->start))
				/* SACKs data before the current hole */
				break; /* No use going through more holes */

			if (SEQ_GEQ(sack->left, cur->end)) {
				/* SACKs data beyond the current hole */
				cur = TAILQ_NEXT(cur, sackhole_q);
				continue;
			}

			if (SEQ_LEQ(sack->left, cur->start)) {
				/* Data acks at least the beginning of hole */
				if (SEQ_GEQ(sack->right, cur->end)) {
					/* Acks entire hole, so delete hole */
					cur = sack_removehole(tp, cur);
					break;
				}

				/* Otherwise, move start of hole forward */
				cur->start = sack->right;
				cur->rxmit = SEQ_MAX(cur->rxmit, cur->start);
				break;
			}

			if (SEQ_GEQ(sack->right, cur->end)) {
				/* Move end of hole backward. */
				cur->end = sack->left;
				cur->rxmit = SEQ_MIN(cur->rxmit, cur->end);
				cur = TAILQ_NEXT(cur, sackhole_q);
				break;
			}

			if (SEQ_LT(cur->start, sack->left) &&
			    SEQ_GT(cur->end, sack->right)) {
				/*
				 * ACKs some data in middle of a hole; need to
				 * split current hole
				 */
				tmp = sack_inserthole(tp, sack->right, cur->end,
				    cur);
				if (tmp == NULL) {
					return;
				}
				tmp->rxmit = SEQ_MAX(cur->rxmit, tmp->start);
				cur->end = sack->left;
				cur->rxmit = SEQ_MIN(cur->rxmit, cur->end);
				cur = tmp;
				break;
			}
		}

		/* At this point, we have reached the tail of the list. */
		if (SEQ_LT(tp->rcv_lastsack, sack->left)) {
			/*
			 * Need to append new hole at end.
			 */
			cur = sack_inserthole(tp, tp->rcv_lastsack, sack->left,
			    NULL);
			if (cur == NULL) {
				return;
			}
		}
		if (SEQ_LT(tp->rcv_lastsack, sack->right)) {
			tp->rcv_lastsack = sack->right;
		}
	}
}

/*
 * tcp_del_sackholes: remove holes covered by a cumulative ACK.
 */

void
tcp_del_sackholes(struct tcpcb *tp, const struct tcphdr *th)
{
	/* Max because this could be an older ack that just arrived. */
	tcp_seq lastack = SEQ_GT(th->th_ack, tp->snd_una) ?
		th->th_ack : tp->snd_una;
	struct sackhole *cur = TAILQ_FIRST(&tp->snd_holes);

	while (cur) {
		if (SEQ_LEQ(cur->end, lastack)) {
			cur = sack_removehole(tp, cur);
		} else if (SEQ_LT(cur->start, lastack)) {
			cur->start = lastack;
			if (SEQ_LT(cur->rxmit, cur->start))
				cur->rxmit = cur->start;
			break;
		} else
			break;
	}
}

/*
 * tcp_free_sackholes: clear the scoreboard.
 */

void
tcp_free_sackholes(struct tcpcb *tp)
{
	struct sackhole *sack;

	/* Free up the SACK hole list. */
	while ((sack = TAILQ_FIRST(&tp->snd_holes)) != NULL) {
		sack_removehole(tp, sack);
	}
	KASSERT(tp->snd_numholes == 0);
}

/*
 * Returns pointer to a sackhole if there are any pending retransmissions;
 * NULL otherwise.
 */
struct sackhole *
tcp_sack_output(struct tcpcb *tp, int *sack_bytes_rexmt)
{
	struct sackhole *cur = NULL;

	if (!TCP_SACK_ENABLED(tp))
		return (NULL);

	*sack_bytes_rexmt = 0;
	TAILQ_FOREACH(cur, &tp->snd_holes, sackhole_q) {
		if (SEQ_LT(cur->rxmit, cur->end)) {
			if (SEQ_LT(cur->rxmit, tp->snd_una)) {
				/* old SACK hole */
				continue;
			}
			*sack_bytes_rexmt += (cur->rxmit - cur->start);
			break;
		}
		*sack_bytes_rexmt += (cur->rxmit - cur->start);
	}

	return (cur);
}

/*
 * After a timeout, the SACK list may be rebuilt.  This SACK information
 * should be used to avoid retransmitting SACKed data.  This function
 * traverses the SACK list to see if snd_nxt should be moved forward.
 */
void
tcp_sack_adjust(struct tcpcb *tp)
{
	struct sackhole *cur = TAILQ_FIRST(&tp->snd_holes);
	struct sackhole *n = NULL;

	if (TAILQ_EMPTY(&tp->snd_holes))
		return; /* No holes */
	if (SEQ_GEQ(tp->snd_nxt, tp->rcv_lastsack))
		return; /* We're already beyond any SACKed blocks */

	/*
	 * Two cases for which we want to advance snd_nxt:
	 * i) snd_nxt lies between end of one hole and beginning of another
	 * ii) snd_nxt lies between end of last hole and rcv_lastsack
	 */
	while ((n = TAILQ_NEXT(cur, sackhole_q)) != NULL) {
		if (SEQ_LT(tp->snd_nxt, cur->end))
			return;
		if (SEQ_GEQ(tp->snd_nxt, n->start))
			cur = n;
		else {
			tp->snd_nxt = n->start;
			return;
		}
	}
	if (SEQ_LT(tp->snd_nxt, cur->end))
		return;
	tp->snd_nxt = tp->rcv_lastsack;

	return;
}

/*
 * tcp_sack_numblks: return the number of SACK blocks to send.
 */

int
tcp_sack_numblks(const struct tcpcb *tp)
{
	int numblks;

	if (!TCP_SACK_ENABLED(tp)) {
		return 0;
	}

	numblks = (((tp->rcv_sack_flags & TCPSACK_HAVED) != 0) ? 1 : 0) +
	    tp->t_segqlen;

	if (numblks == 0) {
		return 0;
	}

	if (numblks > TCP_SACK_MAX) {
		numblks = TCP_SACK_MAX;
	}

	return numblks;
}

#if defined(DDB)
void sack_dump(const struct tcpcb *);

void
sack_dump(const struct tcpcb *tp)
{
	const struct sackhole *cur;

	printf("snd_una=%" PRIu32 ", snd_max=%" PRIu32 "\n",
	    tp->snd_una, tp->snd_max);
	printf("rcv_lastsack=%" PRIu32 ", snd_fack=%" PRIu32 "\n",
	    tp->rcv_lastsack, tp->snd_fack);
	printf("numholes=%d\n", tp->snd_numholes);
	TAILQ_FOREACH(cur, &tp->snd_holes, sackhole_q) {
		printf("\t%" PRIu32 "-%" PRIu32 ", rxmit=%" PRIu32 "\n",
		    cur->start, cur->end, cur->rxmit);
	}
}
#endif /* defined(DDB) */

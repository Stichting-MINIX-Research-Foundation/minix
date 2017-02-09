/*	$NetBSD: ip_id.c,v 1.15 2011/11/19 22:51:25 tls Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by the 3am Software Foundry ("3am").  It was developed by Matt Thomas.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ip_id.c,v 1.15 2011/11/19 22:51:25 tls Exp $");

#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/cprng.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_var.h>

#include <lib/libkern/libkern.h>

#define	IPID_MAXID	65535
#define	IPID_NUMIDS	32768

struct ipid_state {
	kmutex_t	ids_lock;
	uint16_t	ids_start_slot;
	uint16_t	ids_slots[IPID_MAXID];
};

static inline uint32_t
ipid_random(void)
{
	return cprng_fast32();
}

/*
 * Initalizes the  
 * the msb flag. The msb flag is used to generate two distinct
 * cycles of random numbers and thus avoiding reuse of ids.
 *
 * This function is called from id_randomid() when needed, an
 * application does not have to worry about it.
 */
ipid_state_t *
ip_id_init(void)
{
	ipid_state_t *ids;
	size_t i;

	ids = kmem_alloc(sizeof(ipid_state_t), KM_SLEEP);
	mutex_init(&ids->ids_lock, MUTEX_DEFAULT, IPL_SOFTNET);

	ids->ids_start_slot = ipid_random();
	for (i = 0; i < __arraycount(ids->ids_slots); i++) {
		ids->ids_slots[i] = i;
	}

	/*
	 * Shuffle the array.
	 */
	for (i = __arraycount(ids->ids_slots); --i > 0;) {
		size_t k = ipid_random() % (i + 1);
		uint16_t t = ids->ids_slots[i];
		ids->ids_slots[i] = ids->ids_slots[k];
		ids->ids_slots[k] = t;
	}
	return ids;
}

void
ip_id_fini(ipid_state_t *ids)
{

	mutex_destroy(&ids->ids_lock);
	kmem_free(ids, sizeof(ipid_state_t));
}

uint16_t
ip_randomid(ipid_state_t *ids, uint16_t salt)
{
	uint32_t r, k, id;

	/* A random number. */
	r = ipid_random();

	/*
	 * We do a modified Fisher-Yates shuffle but only one position at a
	 * time. Instead of the last entry, we swap with the first entry and
	 * then advance the start of the window by 1.  The next time that 
	 * swapped-out entry can be used is at least 32768 iterations in the
	 * future.
	 *
	 * The easiest way to visual this is to imagine a card deck with 52
	 * cards.  First thing we do is split that into two sets, each with
	 * half of the cards; call them deck A and deck B.  Pick a card
	 * randomly from deck A and remember it, then place it at the
	 * bottom of deck B.  Then take the top card from deck B and add it
	 * to deck A.  Pick another card randomly from deck A and ...
	 */
	mutex_enter(&ids->ids_lock);
	k = (r & (IPID_NUMIDS - 1)) + ids->ids_start_slot;
	if (k >= IPID_MAXID) {
		k -= IPID_MAXID;
	}
	id = ids->ids_slots[k];
	if (k != ids->ids_start_slot) {
		ids->ids_slots[k] = ids->ids_slots[ids->ids_start_slot];
		ids->ids_slots[ids->ids_start_slot] = id;
	}
	if (++ids->ids_start_slot == IPID_MAXID) {
		ids->ids_start_slot = 0;
	}
	mutex_exit(&ids->ids_lock);

	/*
	 * Add an optional salt to the id to further obscure it.
	 */
	id += salt;
	if (id >= IPID_MAXID) {
		id -= IPID_MAXID;
	}
	return (uint16_t)htons(id + 1);
}

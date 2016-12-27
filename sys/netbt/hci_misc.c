/*	$NetBSD: hci_misc.c,v 1.3 2007/09/16 19:59:30 plunky Exp $	*/

/*-
 * Copyright (c) 2005 Iain Hibbert.
 * Copyright (c) 2006 Itronix Inc.
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
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hci_misc.c,v 1.3 2007/09/16 19:59:30 plunky Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/systm.h>

#include <netbt/bluetooth.h>
#include <netbt/hci.h>

/*
 * cache Inquiry Responses for this number of seconds for routing
 * purposes [sysctl]
 */
int hci_memo_expiry = 600;

/*
 * set 'src' address for routing to 'dest'
 */
int
hci_route_lookup(bdaddr_t *src, bdaddr_t *dest)
{
	struct hci_unit *unit;
	struct hci_link *link;
	struct hci_memo *memo;

	/*
	 * Walk the ACL connections, if we have a connection
	 * to 'dest' already then thats best..
	 */
	SIMPLEQ_FOREACH(unit, &hci_unit_list, hci_next) {
		if ((unit->hci_flags & BTF_UP) == 0)
			continue;

		TAILQ_FOREACH(link, &unit->hci_links, hl_next) {
			if (link->hl_type != HCI_LINK_ACL)
				continue;

			if (bdaddr_same(&link->hl_bdaddr, dest))
				goto found;
		}
	}

	/*
	 * Now check all the memos to see if there has been an
	 * inquiry repsonse..
	 */
	SIMPLEQ_FOREACH(unit, &hci_unit_list, hci_next) {
		if ((unit->hci_flags & BTF_UP) == 0)
			continue;

		memo = hci_memo_find(unit, dest);
		if (memo)
			goto found;
	}

	/*
	 * Last ditch effort, lets use the first unit we find
	 * thats up and running. (XXX settable default route?)
	 */
	SIMPLEQ_FOREACH(unit, &hci_unit_list, hci_next) {
		if ((unit->hci_flags & BTF_UP) == 0)
			continue;

		goto found;
	}

	return EHOSTUNREACH;

found:
	bdaddr_copy(src, &unit->hci_bdaddr);
	return 0;
}

/*
 * find unit memo from bdaddr
 */
struct hci_memo *
hci_memo_find(struct hci_unit *unit, bdaddr_t *bdaddr)
{
	struct hci_memo *memo, *m0;
	struct timeval now;

	microtime(&now);

	m0 = LIST_FIRST(&unit->hci_memos);
	while ((memo = m0) != NULL) {
		m0 = LIST_NEXT(memo, next);

		if (now.tv_sec > memo->time.tv_sec + hci_memo_expiry) {
			DPRINTF("memo %p too old (expiring)\n", memo);
			hci_memo_free(memo);
			continue;
		}

		if (bdaddr_same(bdaddr, &memo->bdaddr)) {
			DPRINTF("memo %p found\n", memo);
			return memo;
		}
	}

	DPRINTF("no memo found\n");
	return NULL;
}

/*
 * Make a new memo on unit for bdaddr. If a memo exists, just
 * update the timestamp.
 */
struct hci_memo *
hci_memo_new(struct hci_unit *unit, bdaddr_t *bdaddr)
{
	struct hci_memo *memo;

	memo = hci_memo_find(unit, bdaddr);
	if (memo == NULL) {
		memo = malloc(sizeof(struct hci_memo),
			M_BLUETOOTH, M_NOWAIT | M_ZERO);

		if (memo == NULL) {
			DPRINTFN(0, "no memory for memo!\n");
			return NULL;
		}

		DPRINTF("memo created for %02x:%02x:%02x:%02x:%02x:%02x\n",
			bdaddr->b[5], bdaddr->b[4], bdaddr->b[3],
			bdaddr->b[2], bdaddr->b[1], bdaddr->b[0]);

		bdaddr_copy(&memo->bdaddr, bdaddr);
		LIST_INSERT_HEAD(&unit->hci_memos, memo, next);
	}
	else
		DPRINTF("memo updated for %02x:%02x:%02x:%02x:%02x:%02x\n",
			bdaddr->b[5], bdaddr->b[4], bdaddr->b[3],
			bdaddr->b[2], bdaddr->b[1], bdaddr->b[0]);

	microtime(&memo->time);
	return memo;
}

void
hci_memo_free(struct hci_memo *memo)
{

	LIST_REMOVE(memo, next);
	free(memo, M_BLUETOOTH);
}

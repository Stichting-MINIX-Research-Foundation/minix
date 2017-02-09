/*	$NetBSD: kern_uidinfo.c,v 1.8 2013/03/10 17:55:42 pooka Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_uidinfo.c,v 1.8 2013/03/10 17:55:42 pooka Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/atomic.h>
#include <sys/uidinfo.h>
#include <sys/sysctl.h>
#include <sys/kauth.h>
#include <sys/cpu.h>

static SLIST_HEAD(uihashhead, uidinfo) *uihashtbl;
static u_long 		uihash;

#define	UIHASH(uid)	(&uihashtbl[(uid) & uihash])

static int
sysctl_kern_uidinfo_cnt(SYSCTLFN_ARGS)
{  
	static const struct {
		const char *name;
		u_int value;
	} nv[] = {
#define _MEM(n) { # n, offsetof(struct uidinfo, ui_ ## n) }
		_MEM(proccnt),
		_MEM(lwpcnt),
		_MEM(lockcnt),
		_MEM(sbsize),
#undef _MEM
	};

	for (size_t i = 0; i < __arraycount(nv); i++)
		if (strcmp(nv[i].name, rnode->sysctl_name) == 0) {
			uint64_t cnt;
			struct sysctlnode node = *rnode;
			struct uidinfo *uip;

			node.sysctl_data = &cnt;
			uip = uid_find(kauth_cred_geteuid(l->l_cred));

			*(uint64_t *)node.sysctl_data = 
			    *(u_long *)((char *)uip + nv[i].value);

			return sysctl_lookup(SYSCTLFN_CALL(&node));
		}

	return EINVAL;
}

static struct sysctllog *kern_uidinfo_sysctllog;

static void
sysctl_kern_uidinfo_setup(void)
{
	const struct sysctlnode *rnode, *cnode;

	sysctl_createv(&kern_uidinfo_sysctllog, 0, NULL, &rnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "uidinfo",
		       SYSCTL_DESCR("Resource usage per uid"),
		       NULL, 0, NULL, 0,
		       CTL_KERN, CTL_CREATE, CTL_EOL);

	sysctl_createv(&kern_uidinfo_sysctllog, 0, &rnode, &cnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_QUAD, "proccnt",
		       SYSCTL_DESCR("Number of processes for the current user"),
		       sysctl_kern_uidinfo_cnt, 0, NULL, 0,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(&kern_uidinfo_sysctllog, 0, &rnode, &cnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_QUAD, "lwpcnt",
		       SYSCTL_DESCR("Number of lwps for the current user"),
		       sysctl_kern_uidinfo_cnt, 0, NULL, 0,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(&kern_uidinfo_sysctllog, 0, &rnode, &cnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_QUAD, "lockcnt",
		       SYSCTL_DESCR("Number of locks for the current user"),
		       sysctl_kern_uidinfo_cnt, 0, NULL, 0,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(&kern_uidinfo_sysctllog, 0, &rnode, &cnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_QUAD, "sbsize",
		       SYSCTL_DESCR("Socket buffers used for the current user"),
		       sysctl_kern_uidinfo_cnt, 0, NULL, 0,
		       CTL_CREATE, CTL_EOL);
}

void
uid_init(void)
{

	/*
	 * In case of MP system, SLIST_FOREACH would force a cache line
	 * write-back for every modified 'uidinfo', thus we try to keep the
	 * lists short.
	 */
	const u_int uihash_sz = (maxcpus > 1 ? 1024 : 64);

	uihashtbl = hashinit(uihash_sz, HASH_SLIST, true, &uihash);

	/*
	 * Ensure that uid 0 is always in the user hash table, as
	 * sbreserve() expects it available from interrupt context.
	 */
	(void)uid_find(0);
	sysctl_kern_uidinfo_setup();
}

struct uidinfo *
uid_find(uid_t uid)
{
	struct uidinfo *uip, *uip_first, *newuip;
	struct uihashhead *uipp;

	uipp = UIHASH(uid);
	newuip = NULL;

	/*
	 * To make insertion atomic, abstraction of SLIST will be violated.
	 */
	uip_first = uipp->slh_first;
 again:
	SLIST_FOREACH(uip, uipp, ui_hash) {
		if (uip->ui_uid != uid)
			continue;
		if (newuip != NULL)
			kmem_free(newuip, sizeof(*newuip));
		return uip;
	}
	if (newuip == NULL)
		newuip = kmem_zalloc(sizeof(*newuip), KM_SLEEP);
	newuip->ui_uid = uid;

	/*
	 * If atomic insert is unsuccessful, another thread might be
	 * allocated this 'uid', thus full re-check is needed.
	 */
	newuip->ui_hash.sle_next = uip_first;
	membar_producer();
	uip = atomic_cas_ptr(&uipp->slh_first, uip_first, newuip);
	if (uip != uip_first) {
		uip_first = uip;
		goto again;
	}

	return newuip;
}

/*
 * Change the count associated with number of processes
 * a given user is using.
 */
int
chgproccnt(uid_t uid, int diff)
{
	struct uidinfo *uip;
	long proccnt;

	uip = uid_find(uid);
	proccnt = atomic_add_long_nv(&uip->ui_proccnt, diff);
	KASSERT(proccnt >= 0);
	return proccnt;
}

/*
 * Change the count associated with number of lwps
 * a given user is using.
 */
int
chglwpcnt(uid_t uid, int diff)
{
	struct uidinfo *uip;
	long lwpcnt;

	uip = uid_find(uid);
	lwpcnt = atomic_add_long_nv(&uip->ui_lwpcnt, diff);
	KASSERT(lwpcnt >= 0);
	return lwpcnt;
}

int
chgsbsize(struct uidinfo *uip, u_long *hiwat, u_long to, rlim_t xmax)
{
	rlim_t nsb;
	const long diff = to - *hiwat;

	nsb = (rlim_t)atomic_add_long_nv((long *)&uip->ui_sbsize, diff);
	if (diff > 0 && nsb > xmax) {
		atomic_add_long((long *)&uip->ui_sbsize, -diff);
		return 0;
	}
	*hiwat = to;
	return 1;
}

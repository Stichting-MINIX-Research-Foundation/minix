/*	$NetBSD: pnode.c,v 1.13 2012/08/16 09:25:43 manu Exp $	*/

/*
 * Copyright (c) 2006 Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if !defined(lint)
__RCSID("$NetBSD: pnode.c,v 1.13 2012/08/16 09:25:43 manu Exp $");
#endif /* !lint */

#include <sys/types.h>

#include <assert.h>
#include <puffs.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "puffs_priv.h"

/*
 * Well, you're probably wondering why this isn't optimized.
 * The reason is simple: my available time is not optimized for
 * size ... so please be patient ;)
 */
struct puffs_node *
puffs_pn_new(struct puffs_usermount *pu, void *privdata)
{
	struct puffs_node *pn;

	pn = calloc(1, sizeof(struct puffs_node));
	if (pn == NULL)
		return NULL;

	pn->pn_data = privdata;
	pn->pn_mnt = pu;
	puffs_vattr_null(&pn->pn_va);

	LIST_INSERT_HEAD(&pu->pu_pnodelst, pn, pn_entries);

	pu->pu_flags |= PUFFS_FLAG_PNCOOKIE;

	return pn;
}

void
puffs_pn_remove(struct puffs_node *pn)
{

	LIST_REMOVE(pn, pn_entries);
	pn->pn_flags |= PUFFS_NODE_REMOVED;
#if defined(__minix)
	if (pn->pn_count != 0) {
		struct puffs_usermount *pu = pn->pn_mnt;
		assert(pu != NULL);

		/* XXX FS removes this pn from the list to prevent further
		 * lookups from finding node after remove/rm/rename op.
		 * But VFS still uses it, i.e. pnode is still open, and
		 * will put it later. Keep it in separate list to do reclaim
		 * in fs_put().
		 */
		LIST_INSERT_HEAD(&pu->pu_pnode_removed_lst, pn, pn_entries);
	}
#endif /* defined(__minix) */
}

void
puffs_pn_put(struct puffs_node *pn)
{
	struct puffs_usermount *pu = pn->pn_mnt;

	pu->pu_pathfree(pu, &pn->pn_po);
	if ((pn->pn_flags & PUFFS_NODE_REMOVED) == 0)
		LIST_REMOVE(pn, pn_entries);
	free(pn);
}

/* walk list, rv can be used either to halt or to return a value */
void *
puffs_pn_nodewalk(struct puffs_usermount *pu, puffs_nodewalk_fn fn, void *arg)
{
	struct puffs_node *pn_cur, *pn_next;
	void *rv;

	pn_cur = LIST_FIRST(&pu->pu_pnodelst);
	while (pn_cur) {
		pn_next = LIST_NEXT(pn_cur, pn_entries);
		rv = fn(pu, pn_cur, arg);
		if (rv)
			return rv;
		pn_cur = pn_next;
	}

	return NULL;
}

struct vattr *
puffs_pn_getvap(struct puffs_node *pn)
{

	return &pn->pn_va;
}

void *
puffs_pn_getpriv(struct puffs_node *pn)
{

	return pn->pn_data;
}

void
puffs_pn_setpriv(struct puffs_node *pn, void *priv)
{

	pn->pn_data = priv;
}

struct puffs_pathobj *
puffs_pn_getpo(struct puffs_node *pn)
{

	return &pn->pn_po;
}

struct puffs_usermount *
puffs_pn_getmnt(struct puffs_node *pn)
{

	return pn->pn_mnt;
}

/* convenience / shortcut */
void *
puffs_pn_getmntspecific(struct puffs_node *pn)
{

	return pn->pn_mnt->pu_privdata;
}

/*
 * newnode parameters
 */
void
puffs_newinfo_setcookie(struct puffs_newinfo *pni, puffs_cookie_t cookie)
{

	*pni->pni_cookie = cookie;
}

void
puffs_newinfo_setvtype(struct puffs_newinfo *pni, enum vtype vt)
{

	*pni->pni_vtype = vt;
}

void
puffs_newinfo_setsize(struct puffs_newinfo *pni, voff_t size)
{

	*pni->pni_size = size;
}

void
puffs_newinfo_setrdev(struct puffs_newinfo *pni, dev_t rdev)
{

	*pni->pni_rdev = rdev;
}

void
puffs_newinfo_setva(struct puffs_newinfo *pni, struct vattr *va)
{

	(void)memcpy(pni->pni_va, va, sizeof(struct vattr));
}

void
puffs_newinfo_setvattl(struct puffs_newinfo *pni, struct timespec *va_ttl)
{

	pni->pni_va_ttl->tv_sec = va_ttl->tv_sec;
	pni->pni_va_ttl->tv_nsec = va_ttl->tv_nsec;
}

void
puffs_newinfo_setcnttl(struct puffs_newinfo *pni, struct timespec *cn_ttl)
{

	pni->pni_cn_ttl->tv_sec = cn_ttl->tv_sec;
	pni->pni_cn_ttl->tv_nsec = cn_ttl->tv_nsec;
}


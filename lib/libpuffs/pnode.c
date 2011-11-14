/*	$NetBSD: pnode.c,v 1.10 2008/08/12 19:44:39 pooka Exp $	*/

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
__RCSID("$NetBSD: pnode.c,v 1.10 2008/08/12 19:44:39 pooka Exp $");
#endif /* !lint */

#include <minix/type.h>
#include <sys/types.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "puffs.h"
#include "puffs_priv.h"
#include "proto.h"

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

	return pn;
}

void
puffs_pn_remove(struct puffs_node *pn)
{
	struct puffs_usermount *pu = pn->pn_mnt;
	assert(pu != NULL);

	LIST_REMOVE(pn, pn_entries);
	pn->pn_flags |= PUFFS_NODE_REMOVED;
	if (pn->pn_count != 0) {
		/* XXX FS removes this pn from the list to prevent further
		 * lookups from finding node after remove/rm/rename op.
		 * But VFS still uses it, i.e. pnode is still open, and
		 * will put it later. Keep it in separate list to do reclaim
		 * in fs_put().
		 */
		LIST_INSERT_HEAD(&pu->pu_pnode_removed_lst, pn, pn_entries);
	}
}

void
puffs_pn_put(struct puffs_node *pn)
{
	struct puffs_usermount *pu = pn->pn_mnt;

	pu->pu_pathfree(pu, &pn->pn_po);
	/* Removes either from pu_pnodelst or pu_pnode_removed_lst */
	LIST_REMOVE(pn, pn_entries);
	free(pn);
}

/* walk list, rv can be used either to halt or to return a value
 * XXX (MINIX note): if fn is 0, then arg is ino_t and we search
 * node with ino_t. TODO: modify docs.
 */
void *
puffs_pn_nodewalk(struct puffs_usermount *pu, puffs_nodewalk_fn fn, void *arg)
{
	struct puffs_node *pn_cur, *pn_next;
	void *rv;

	pn_cur = LIST_FIRST(&pu->pu_pnodelst);
	while (pn_cur) {
		pn_next = LIST_NEXT(pn_cur, pn_entries);
		if (fn) {
			rv = fn(pu, pn_cur, arg);
			if (rv)
				return rv;
		} else {
			if (pn_cur->pn_va.va_fileid == *((ino_t*) arg))
				return pn_cur;
		}
		pn_cur = pn_next;
	}

	return NULL;
}

void*
puffs_pn_nodeprint(struct puffs_usermount *pu, struct puffs_node *pn, void *arg)
{
	/* If arg is specified, print only pnodes with inum (should be only one,
	 * otherwise - all.
	 */
	if (arg != NULL) {
		ino_t inum = *(ino_t*)arg;
		if (pn->pn_va.va_fileid != inum) {
			return NULL;
		}
	}
	lpuffs_debug(" ino %ld used %d %s\n", pn->pn_va.va_fileid, pn->pn_count,
				pn->pn_po.po_path);
	/* If arg specified, it should be the only one pnode to be printed,
	 * but we walk through the rest of list for debugging purposes.
	 */
	return NULL;
}

void
puffs_pn_nodeprintall(struct puffs_usermount *pu)
{
	puffs_pn_nodewalk(pu, puffs_pn_nodeprint, NULL);
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

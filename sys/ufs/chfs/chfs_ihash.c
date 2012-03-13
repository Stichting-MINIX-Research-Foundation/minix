/*	$NetBSD: chfs_ihash.c,v 1.1 2011/11/24 15:51:31 ahoka Exp $	*/

/*-
 * Copyright (c) 2010 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by the Department of Software Engineering, University of Szeged, Hungary
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "chfs.h"
/*
 * Structures associated with inode cacheing.
 */
static LIST_HEAD(ihashhead, chfs_inode) *chfs_ihashtbl;
static u_long	chfs_ihash;		/* size of hash table - 1 */
#define INOHASH(device, inum)	(((device) + (inum)) & chfs_ihash)

kmutex_t	chfs_ihash_lock;
kmutex_t	chfs_hashlock;

/*
 * Initialize inode hash table.
 */
void
chfs_ihashinit(void)
{
	dbg("initing\n");

	mutex_init(&chfs_hashlock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&chfs_ihash_lock, MUTEX_DEFAULT, IPL_NONE);
	chfs_ihashtbl = hashinit(desiredvnodes,
	    HASH_LIST, true, &chfs_ihash);
}

/*
 * Reinitialize inode hash table.
 */

void
chfs_ihashreinit(void)
{
	struct chfs_inode *ip;
	struct ihashhead *oldhash, *hash;
	u_long oldmask, mask, val;
	int i;

	dbg("reiniting\n");

	hash = hashinit(desiredvnodes, HASH_LIST, true, &mask);
	mutex_enter(&chfs_ihash_lock);
	oldhash = chfs_ihashtbl;
	oldmask = chfs_ihash;
	chfs_ihashtbl = hash;
	chfs_ihash = mask;
	for (i = 0; i <= oldmask; i++) {
		while ((ip = LIST_FIRST(&oldhash[i])) != NULL) {
			LIST_REMOVE(ip, hash_entry);
			val = INOHASH(ip->dev, ip->ino);
			LIST_INSERT_HEAD(&hash[val], ip, hash_entry);
		}
	}
	mutex_exit(&chfs_ihash_lock);
	hashdone(oldhash, HASH_LIST, oldmask);
}

/*
 * Free inode hash table.
 */
void
chfs_ihashdone(void)
{
	dbg("destroying\n");

	hashdone(chfs_ihashtbl, HASH_LIST, chfs_ihash);
	mutex_destroy(&chfs_hashlock);
	mutex_destroy(&chfs_ihash_lock);
}

/*
 * Use the device/inum pair to find the incore inode, and return a pointer
 * to it. If it is in core, return it, even if it is locked.
 */
struct vnode *
chfs_ihashlookup(dev_t dev, ino_t inum)
{
	struct chfs_inode *ip;
	struct ihashhead *ipp;

	dbg("dev: %ju, inum: %ju\n", (uintmax_t )dev, (uintmax_t )inum);

	KASSERT(mutex_owned(&chfs_ihash_lock));

	ipp = &chfs_ihashtbl[INOHASH(dev, inum)];
	LIST_FOREACH(ip, ipp, hash_entry) {
		if (inum == ip->ino && dev == ip->dev) {
			break;
		}
	}

	if (ip) {
		return (ITOV(ip));
	}

	return (NULLVP);
}

/*
 * Use the device/inum pair to find the incore inode, and return a pointer
 * to it. If it is in core, but locked, wait for it.
 */
struct vnode *
chfs_ihashget(dev_t dev, ino_t inum, int flags)
{
	struct ihashhead *ipp;
	struct chfs_inode *ip;
	struct vnode *vp;

	dbg("search for ino\n");

loop:
	mutex_enter(&chfs_ihash_lock);
	ipp = &chfs_ihashtbl[INOHASH(dev, inum)];
	dbg("ipp: %p, chfs_ihashtbl: %p, ihash: %lu\n",
	    ipp, chfs_ihashtbl, chfs_ihash);
	LIST_FOREACH(ip, ipp, hash_entry) {
		dbg("ip: %p\n", ip);
		if (inum == ip->ino && dev == ip->dev) {
//			printf("chfs_ihashget: found inode: %p\n", ip);
			vp = ITOV(ip);
			KASSERT(vp != NULL);
			//dbg("found\n");
			if (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) {
				//dbg("wait for #%llu\n", ip->ino);
				mutex_exit(&chfs_ihash_lock);
				goto loop;
			}
			/*
			if (VOP_ISLOCKED(vp))
				dbg("locked\n");
			else
				dbg("isn't locked\n");
			*/
			if (flags == 0) {
				//dbg("no flags\n");
				mutex_exit(&chfs_ihash_lock);
			} else {
				//dbg("vget\n");
				mutex_enter(vp->v_interlock);
				mutex_exit(&chfs_ihash_lock);
				if (vget(vp, flags)) {
					goto loop;
				}
				//dbg("got it\n");
			}
			//dbg("return\n");
			return (vp);
		}
	}
	//dbg("not found\n");
	mutex_exit(&chfs_ihash_lock);
	return (NULL);
}

/*
 * Insert the inode into the hash table, and return it locked.
 */
void
chfs_ihashins(struct chfs_inode *ip)
{
	struct ihashhead *ipp;

	dbg("ip: %p\n", ip);

	KASSERT(mutex_owned(&chfs_hashlock));

	/* lock the inode, then put it on the appropriate hash list */
	VOP_LOCK(ITOV(ip), LK_EXCLUSIVE);

	mutex_enter(&chfs_ihash_lock);
	ipp = &chfs_ihashtbl[INOHASH(ip->dev, ip->ino)];
	LIST_INSERT_HEAD(ipp, ip, hash_entry);
	mutex_exit(&chfs_ihash_lock);
}

/*
 * Remove the inode from the hash table.
 */
void
chfs_ihashrem(struct chfs_inode *ip)
{
	dbg("ip: %p\n", ip);

	mutex_enter(&chfs_ihash_lock);
	LIST_REMOVE(ip, hash_entry);
	mutex_exit(&chfs_ihash_lock);
}


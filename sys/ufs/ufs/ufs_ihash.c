/*	$NetBSD: ufs_ihash.c,v 1.31 2011/06/12 03:36:02 rmind Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
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
 *	@(#)ufs_ihash.c	8.7 (Berkeley) 5/17/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ufs_ihash.c,v 1.31 2011/06/12 03:36:02 rmind Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/mutex.h>

#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>

/*
 * Structures associated with inode cacheing.
 */
static LIST_HEAD(ihashhead, inode) *ihashtbl;
static u_long	ihash;		/* size of hash table - 1 */
#define INOHASH(device, inum)	(((device) + (inum)) & ihash)

kmutex_t	ufs_ihash_lock;
kmutex_t	ufs_hashlock;

/*
 * Initialize inode hash table.
 */
void
ufs_ihashinit(void)
{

	mutex_init(&ufs_hashlock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&ufs_ihash_lock, MUTEX_DEFAULT, IPL_NONE);
	ihashtbl = hashinit(desiredvnodes, HASH_LIST, true, &ihash);
}

/*
 * Reinitialize inode hash table.
 */

void
ufs_ihashreinit(void)
{
	struct inode *ip;
	struct ihashhead *oldhash, *hash;
	u_long oldmask, mask, val;
	int i;

	hash = hashinit(desiredvnodes, HASH_LIST, true, &mask);
	mutex_enter(&ufs_ihash_lock);
	oldhash = ihashtbl;
	oldmask = ihash;
	ihashtbl = hash;
	ihash = mask;
	for (i = 0; i <= oldmask; i++) {
		while ((ip = LIST_FIRST(&oldhash[i])) != NULL) {
			LIST_REMOVE(ip, i_hash);
			val = INOHASH(ip->i_dev, ip->i_number);
			LIST_INSERT_HEAD(&hash[val], ip, i_hash);
		}
	}
	mutex_exit(&ufs_ihash_lock);
	hashdone(oldhash, HASH_LIST, oldmask);
}

/*
 * Free inode hash table.
 */
void
ufs_ihashdone(void)
{

	hashdone(ihashtbl, HASH_LIST, ihash);
	mutex_destroy(&ufs_hashlock);
	mutex_destroy(&ufs_ihash_lock);
}

/*
 * Use the device/inum pair to find the incore inode, and return a pointer
 * to it. If it is in core, return it, even if it is locked.
 */
struct vnode *
ufs_ihashlookup(dev_t dev, ino_t inum)
{
	struct inode *ip;
	struct ihashhead *ipp;

	KASSERT(mutex_owned(&ufs_ihash_lock));

	ipp = &ihashtbl[INOHASH(dev, inum)];
	LIST_FOREACH(ip, ipp, i_hash) {
		if (inum == ip->i_number && dev == ip->i_dev)
			break;
	}
	if (ip)
		return (ITOV(ip));
	return (NULLVP);
}

/*
 * Use the device/inum pair to find the incore inode, and return a pointer
 * to it. If it is in core, but locked, wait for it.
 */
struct vnode *
ufs_ihashget(dev_t dev, ino_t inum, int flags)
{
	struct ihashhead *ipp;
	struct inode *ip;
	struct vnode *vp;

 loop:
	mutex_enter(&ufs_ihash_lock);
	ipp = &ihashtbl[INOHASH(dev, inum)];
	LIST_FOREACH(ip, ipp, i_hash) {
		if (inum == ip->i_number && dev == ip->i_dev) {
			vp = ITOV(ip);
			if (flags == 0) {
				mutex_exit(&ufs_ihash_lock);
			} else {
				mutex_enter(vp->v_interlock);
				mutex_exit(&ufs_ihash_lock);
				if (vget(vp, flags))
					goto loop;
			}
			return (vp);
		}
	}
	mutex_exit(&ufs_ihash_lock);
	return (NULL);
}

/*
 * Insert the inode into the hash table, and return it locked.
 */
void
ufs_ihashins(struct inode *ip)
{
	struct ihashhead *ipp;

	KASSERT(mutex_owned(&ufs_hashlock));

	/* lock the inode, then put it on the appropriate hash list */
	VOP_LOCK(ITOV(ip), LK_EXCLUSIVE);

	mutex_enter(&ufs_ihash_lock);
	ipp = &ihashtbl[INOHASH(ip->i_dev, ip->i_number)];
	LIST_INSERT_HEAD(ipp, ip, i_hash);
	mutex_exit(&ufs_ihash_lock);
}

/*
 * Remove the inode from the hash table.
 */
void
ufs_ihashrem(struct inode *ip)
{
	mutex_enter(&ufs_ihash_lock);
	LIST_REMOVE(ip, i_hash);
	mutex_exit(&ufs_ihash_lock);
}

/*	$NetBSD: ptyfs_subr.c,v 1.33 2014/10/15 15:00:03 christos Exp $	*/

/*
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)ptyfs_subr.c	8.6 (Berkeley) 5/14/95
 */

/*
 * Copyright (c) 1994 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1993 Jan-Simon Pendry
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)procfs_subr.c	8.6 (Berkeley) 5/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ptyfs_subr.c,v 1.33 2014/10/15 15:00:03 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/malloc.h>
#include <sys/file.h>
#include <sys/namei.h>
#include <sys/filedesc.h>
#include <sys/select.h>
#include <sys/tty.h>
#include <sys/pty.h>
#include <sys/kauth.h>
#include <sys/lwp.h>

#include <fs/ptyfs/ptyfs.h>

static kmutex_t ptyfs_hashlock;

static SLIST_HEAD(ptyfs_hashhead, ptyfsnode) *ptyfs_node_tbl;
static u_long ptyfs_node_mask; /* size of hash table - 1 */

/*
 * allocate a ptyfsnode/vnode pair.  the vnode is referenced.
 *
 * the pty, ptyfs_type, and mount point uniquely
 * identify a ptyfsnode.  the mount point is needed
 * because someone might mount this filesystem
 * twice.
 */
int
ptyfs_allocvp(struct mount *mp, struct vnode **vpp, ptyfstype type, int pty)
{
	struct ptyfskey key;

	memset(&key, 0, sizeof(key));
	key.ptk_pty = pty;
	key.ptk_type = type;
	return vcache_get(mp, &key, sizeof(key), vpp);
}

/*
 * Initialize ptyfsnode hash table.
 */
void
ptyfs_hashinit(void)
{

	ptyfs_node_tbl = hashinit(16, HASH_SLIST, true, &ptyfs_node_mask);
	mutex_init(&ptyfs_hashlock, MUTEX_DEFAULT, IPL_NONE);
}

/*
 * Free ptyfsnode hash table.
 */
void
ptyfs_hashdone(void)
{
	
	mutex_destroy(&ptyfs_hashlock);
	hashdone(ptyfs_node_tbl, HASH_SLIST, ptyfs_node_mask);
}

/*
 * Get a ptyfsnode from the hash table, or allocate one.
 */
struct ptyfsnode *
ptyfs_get_node(ptyfstype type, int pty)
{
	struct ptyfs_hashhead *ppp;
	struct ptyfsnode *pp;

	ppp = &ptyfs_node_tbl[PTYFS_FILENO(type, pty) & ptyfs_node_mask];

	mutex_enter(&ptyfs_hashlock);
	SLIST_FOREACH(pp, ppp, ptyfs_hash) {
		if (pty == pp->ptyfs_pty && pp->ptyfs_type == type) {
			mutex_exit(&ptyfs_hashlock);
			return pp;
		}
	}
	mutex_exit(&ptyfs_hashlock);

	pp = malloc(sizeof(struct ptyfsnode), M_TEMP, M_WAITOK);
	pp->ptyfs_pty = pty;
	pp->ptyfs_type = type;
	pp->ptyfs_fileno = PTYFS_FILENO(pty, type);
	if (pp->ptyfs_type == PTYFSroot)
		pp->ptyfs_mode = S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|
		    S_IROTH|S_IXOTH;
	else
		pp->ptyfs_mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|
		    S_IROTH|S_IWOTH;

	pp->ptyfs_uid = pp->ptyfs_gid = 0;
	pp->ptyfs_status = PTYFS_CHANGE;
	PTYFS_ITIMES(pp, NULL, NULL, NULL);
	pp->ptyfs_birthtime = pp->ptyfs_mtime =
	    pp->ptyfs_atime = pp->ptyfs_ctime;
	pp->ptyfs_flags = 0;
	mutex_enter(&ptyfs_hashlock);
	/*
	 * XXX We have minimum race condition when opening master side
	 * first time, if other threads through other mount points, trying
	 * opening the same device. As follow we have little chance have
	 * unused list entries.
	 */
	SLIST_INSERT_HEAD(ppp, pp, ptyfs_hash);
	mutex_exit(&ptyfs_hashlock);
	return pp;
}

/*
 * Mark this controlling pty as active.
 */
void
ptyfs_set_active(struct mount *mp, int pty)
{
	struct ptyfsmount *pmnt = VFSTOPTY(mp);

	KASSERT(pty >= 0);
	/* Reallocate map if needed. */
	if (pty >= pmnt->pmnt_bitmap_size * NBBY) {
		int osize, nsize;
		uint8_t *obitmap, *nbitmap;

		nsize = roundup(howmany(pty + 1, NBBY), 64);
		nbitmap = kmem_alloc(nsize, KM_SLEEP);
		mutex_enter(&pmnt->pmnt_lock);
		if (pty < pmnt->pmnt_bitmap_size * NBBY) {
			mutex_exit(&pmnt->pmnt_lock);
			kmem_free(nbitmap, nsize);
		} else {
			osize = pmnt->pmnt_bitmap_size;
			obitmap = pmnt->pmnt_bitmap;
			pmnt->pmnt_bitmap_size = nsize;
			pmnt->pmnt_bitmap = nbitmap;
			if (osize > 0)
				memcpy(pmnt->pmnt_bitmap, obitmap, osize);
			memset(pmnt->pmnt_bitmap + osize, 0, nsize - osize);
			mutex_exit(&pmnt->pmnt_lock);
			if (osize > 0)
				kmem_free(obitmap, osize);
		}
	}

	mutex_enter(&pmnt->pmnt_lock);
	setbit(pmnt->pmnt_bitmap, pty);
	mutex_exit(&pmnt->pmnt_lock);
}

/*
 * Mark this controlling pty as inactive.
 */
void
ptyfs_clr_active(struct mount *mp, int pty)
{
	struct ptyfsmount *pmnt = VFSTOPTY(mp);

	KASSERT(pty >= 0);
	mutex_enter(&pmnt->pmnt_lock);
	if (pty >= 0 && pty < pmnt->pmnt_bitmap_size * NBBY)
		clrbit(pmnt->pmnt_bitmap, pty);
	mutex_exit(&pmnt->pmnt_lock);
}

/*
 * Lookup the next active controlling pty greater or equal "pty".
 * Return -1 if not found.
 */
int
ptyfs_next_active(struct mount *mp, int pty)
{
	struct ptyfsmount *pmnt = VFSTOPTY(mp);

	KASSERT(pty >= 0);
	mutex_enter(&pmnt->pmnt_lock);
	while (pty < pmnt->pmnt_bitmap_size * NBBY) {
		if (isset(pmnt->pmnt_bitmap, pty)) {
			mutex_exit(&pmnt->pmnt_lock);
			return pty;
		}
		pty++;
	}
	mutex_exit(&pmnt->pmnt_lock);
	return -1;
}

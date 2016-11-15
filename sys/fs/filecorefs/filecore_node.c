/*	$NetBSD: filecore_node.c,v 1.27 2014/10/04 13:27:24 hannken Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1989, 1994
 *           The Regents of the University of California.
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
 *	filecore_node.c		1.0	1998/6/4
 */

/*-
 * Copyright (c) 1998 Andrew McMurry
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
 *	filecore_node.c		1.0	1998/6/4
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: filecore_node.c,v 1.27 2014/10/04 13:27:24 hannken Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/kernel.h>
#include <sys/pool.h>
#include <sys/stat.h>
#include <sys/mutex.h>

#include <fs/filecorefs/filecore.h>
#include <fs/filecorefs/filecore_extern.h>
#include <fs/filecorefs/filecore_node.h>
#include <fs/filecorefs/filecore_mount.h>

struct pool		filecore_node_pool;

extern int prtactive;	/* 1 => print out reclaim of active vnodes */

static const struct genfs_ops filecore_genfsops = {
        .gop_size = genfs_size,
};

/*
 * Initialize hash links for inodes and dnodes.
 */
void
filecore_init(void)
{

	pool_init(&filecore_node_pool, sizeof(struct filecore_node), 0, 0, 0,
	    "filecrnopl", &pool_allocator_nointr, IPL_NONE);
}

/*
 * Reinitialize inode hash table.
 */
void
filecore_reinit(void)
{

}

/*
 * Destroy node pool and hash table.
 */
void
filecore_done(void)
{

	pool_destroy(&filecore_node_pool);
}

/*
 * Initialize this vnode / filecore node pair.
 * Caller assures no other thread will try to load this node.
 */
int
filecore_loadvnode(struct mount *mp, struct vnode *vp,
    const void *key, size_t key_len, const void **new_key)
{
	ino_t ino;
	struct filecore_mnt *fcmp;
	struct filecore_node *ip;
	struct buf *bp;
	int error;

	KASSERT(key_len == sizeof(ino));
	memcpy(&ino, key, key_len);
	fcmp = VFSTOFILECORE(mp);

	ip = pool_get(&filecore_node_pool, PR_WAITOK);
	memset(ip, 0, sizeof(struct filecore_node));
	ip->i_vnode = vp;
	ip->i_dev = fcmp->fc_dev;
	ip->i_number = ino;
	ip->i_block = -1;
	ip->i_parent = -2;

	if (ino == FILECORE_ROOTINO) {
		/* Here we need to construct a root directory inode */
		memcpy(ip->i_dirent.name, "root", 4);
		ip->i_dirent.load = 0;
		ip->i_dirent.exec = 0;
		ip->i_dirent.len = FILECORE_DIR_SIZE;
		ip->i_dirent.addr = fcmp->drec.root;
		ip->i_dirent.attr = FILECORE_ATTR_DIR | FILECORE_ATTR_READ;

	} else {
		/* Read in Data from Directory Entry */
		if ((error = filecore_bread(fcmp, ino & FILECORE_INO_MASK,
		    FILECORE_DIR_SIZE, NOCRED, &bp)) != 0) {
			pool_put(&filecore_node_pool, ip);
			return error;
		}

		memcpy(&ip->i_dirent,
		    fcdirentry(bp->b_data, ino >> FILECORE_INO_INDEX),
		    sizeof(struct filecore_direntry));
#ifdef FILECORE_DEBUG_BR
		printf("brelse(%p) vf5\n", bp);
#endif
		brelse(bp, 0);
	}

	ip->i_mnt = fcmp;
	ip->i_devvp = fcmp->fc_devvp;
	ip->i_diroff = 0;
	vref(ip->i_devvp);

	/*
	 * Initialize the associated vnode
	 */

	vp->v_tag = VT_FILECORE;
	vp->v_op = filecore_vnodeop_p;
	vp->v_data = ip;
	if (ip->i_dirent.attr & FILECORE_ATTR_DIR)
		vp->v_type = VDIR;
	else
		vp->v_type = VREG;
	if (ino == FILECORE_ROOTINO)
		vp->v_vflag |= VV_ROOT;
	genfs_node_init(vp, &filecore_genfsops);

	/*
	 * XXX need generation number?
	 */

	uvm_vnp_setsize(vp, ip->i_size);
	*new_key = &ip->i_number;
	return 0;
}

/*
 * Last reference to an inode, write the inode out and if necessary,
 * truncate and deallocate the file.
 */
int
filecore_inactive(void *v)
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		bool *a_recycle;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct filecore_node *ip = VTOI(vp);
	int error = 0;

	/*
	 * If we are done with the inode, reclaim it
	 * so that it can be reused immediately.
	 */
	ip->i_flag = 0;
	*ap->a_recycle = (filecore_staleinode(ip) != 0);
	VOP_UNLOCK(vp);
	return error;
}

/*
 * Reclaim an inode so that it can be used for other purposes.
 */
int
filecore_reclaim(void *v)
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
		struct lwp *a_l;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct filecore_node *ip = VTOI(vp);

	if (prtactive && vp->v_usecount > 1)
		vprint("filecore_reclaim: pushing active", vp);
	/*
	 * Remove the inode from the vnode cache.
	 */
	vcache_remove(vp->v_mount, &ip->i_number, sizeof(ip->i_number));

	/*
	 * Purge old data structures associated with the inode.
	 */
	if (ip->i_devvp) {
		vrele(ip->i_devvp);
		ip->i_devvp = 0;
	}
	genfs_node_destroy(vp);
	pool_put(&filecore_node_pool, vp->v_data);
	vp->v_data = NULL;
	return (0);
}

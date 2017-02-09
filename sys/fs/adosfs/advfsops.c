/*	$NetBSD: advfsops.c,v 1.74 2015/04/20 13:44:16 riastradh Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
 * Copyright (c) 1996 Matthias Scheler
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christian E. Hopps.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: advfsops.c,v 1.74 2015/04/20 13:44:16 riastradh Exp $");

#if defined(_KERNEL_OPT)
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h> /* XXX */
#include <sys/fcntl.h>
#include <sys/namei.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/module.h>
#include <fs/adosfs/adosfs.h>

MODULE(MODULE_CLASS_VFS, adosfs, NULL);

VFS_PROTOS(adosfs);

static struct sysctllog *adosfs_sysctl_log;

int adosfs_mountfs(struct vnode *, struct mount *, struct lwp *);
int adosfs_loadbitmap(struct adosfsmount *);

struct pool adosfs_node_pool;

MALLOC_JUSTDEFINE(M_ANODE, "adosfs anode","adosfs anode structures and tables");

static const struct genfs_ops adosfs_genfsops = {
	.gop_size = genfs_size,
};

int (**adosfs_vnodeop_p)(void *);

int
adosfs_mount(struct mount *mp, const char *path, void *data, size_t *data_len)
{
	struct lwp *l = curlwp;
	struct vnode *devvp;
	struct adosfs_args *args = data;
	struct adosfsmount *amp;
	int error;
	mode_t accessmode;

	if (args == NULL)
		return EINVAL;
	if (*data_len < sizeof *args)
		return EINVAL;

	if (mp->mnt_flag & MNT_GETARGS) {
		amp = VFSTOADOSFS(mp);
		if (amp == NULL)
			return EIO;
		args->uid = amp->uid;
		args->gid = amp->gid;
		args->mask = amp->mask;
		args->fspec = NULL;
		*data_len = sizeof *args;
		return 0;
	}

	if ((mp->mnt_flag & MNT_RDONLY) == 0)
		return (EROFS);

	if ((mp->mnt_flag & MNT_UPDATE) && args->fspec == NULL)
		return EOPNOTSUPP;

	/*
	 * Not an update, or updating the name: look up the name
	 * and verify that it refers to a sensible block device.
	 */
	error = namei_simple_user(args->fspec,
				NSM_FOLLOW_NOEMULROOT, &devvp);
	if (error != 0)
		return (error);

	if (devvp->v_type != VBLK) {
		vrele(devvp);
		return (ENOTBLK);
	}
	if (bdevsw_lookup(devvp->v_rdev) == NULL) {
		vrele(devvp);
		return (ENXIO);
	}
	/*
	 * If mount by non-root, then verify that user has necessary
	 * permissions on the device.
	 */
	accessmode = VREAD;
	if ((mp->mnt_flag & MNT_RDONLY) == 0)
		accessmode |= VWRITE;
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
	error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_MOUNT,
	    KAUTH_REQ_SYSTEM_MOUNT_DEVICE, mp, devvp, KAUTH_ARG(accessmode));
	VOP_UNLOCK(devvp);
	if (error) {
		vrele(devvp);
		return (error);
	}
/* MNT_UPDATE? */
	if ((error = adosfs_mountfs(devvp, mp, l)) != 0) {
		vrele(devvp);
		return (error);
	}
	amp = VFSTOADOSFS(mp);
	amp->uid = args->uid;
	amp->gid = args->gid;
	amp->mask = args->mask;
	return set_statvfs_info(path, UIO_USERSPACE, args->fspec, UIO_USERSPACE,
	    mp->mnt_op->vfs_name, mp, l);
}

int
adosfs_mountfs(struct vnode *devvp, struct mount *mp, struct lwp *l)
{
	struct disklabel dl;
	struct partition *parp;
	struct adosfsmount *amp;
	struct buf *bp;
	struct vnode *rvp;
	size_t bitmap_sz = 0;
	int error;
	uint64_t numsecs;
	unsigned secsize;
	unsigned long secsperblk, blksperdisk, resvblks;

	amp = NULL;

	if ((error = vinvalbuf(devvp, V_SAVE, l->l_cred, l, 0, 0)) != 0)
		return (error);

	/*
	 * open blkdev and read boot and root block
	 */
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
	if ((error = VOP_OPEN(devvp, FREAD, NOCRED)) != 0) {
		VOP_UNLOCK(devvp);
		return (error);
	}

	error = getdisksize(devvp, &numsecs, &secsize);
	if (error)
		goto fail;

	amp = kmem_zalloc(sizeof(struct adosfsmount), KM_SLEEP);

	/*
	 * compute filesystem parameters from disklabel
	 * on arch/amiga the disklabel is computed from the native
	 * partition tables
	 * - p_fsize is the filesystem block size
	 * - p_frag is the number of sectors per filesystem block
	 * - p_cpg is the number of reserved blocks (boot blocks)
	 * - p_psize is reduced by the number of preallocated blocks
	 *           at the end of a partition
	 *
	 * XXX
	 * - bsize and secsperblk could be computed from the first sector
	 *   of the root block
	 * - resvblks (the number of boot blocks) can only be guessed
	 *   by scanning for the root block as its position moves
	 *   with resvblks
	 */
	error = VOP_IOCTL(devvp, DIOCGDINFO, &dl, FREAD, NOCRED);
	VOP_UNLOCK(devvp);
	if (error)
		goto fail;
	parp = &dl.d_partitions[DISKPART(devvp->v_rdev)];
	if (dl.d_type == DKTYPE_FLOPPY) {
		amp->bsize = secsize;
		secsperblk = 1;
		resvblks   = 2;
	} else if (parp->p_fsize > 0 && parp->p_frag > 0) {
		amp->bsize = parp->p_fsize * parp->p_frag;
		secsperblk = parp->p_frag;
		resvblks   = parp->p_cpg;
	} else {
		error = EINVAL;
		goto fail;
	}
	blksperdisk = numsecs / secsperblk;


	/* The filesytem variant ('dostype') is stored in the boot block */
	bp = NULL;
	if ((error = bread(devvp, (daddr_t)BBOFF,
			   amp->bsize, 0, &bp)) != 0) {
		goto fail;
	}
	amp->dostype = adoswordn(bp, 0);
	brelse(bp, 0);

	/* basic sanity checks */
	if (amp->dostype < 0x444f5300 || amp->dostype > 0x444f5305) {
		error = EINVAL;
		goto fail;
	}

	amp->rootb = (blksperdisk - 1 + resvblks) / 2;
	amp->numblks = blksperdisk - resvblks;

	amp->nwords = amp->bsize >> 2;
	amp->dbsize = amp->bsize - (IS_FFS(amp) ? 0 : OFS_DATA_OFFSET);
	amp->devvp = devvp;

	amp->mp = mp;
	mp->mnt_data = amp;
	mp->mnt_stat.f_fsidx.__fsid_val[0] = (long)devvp->v_rdev;
	mp->mnt_stat.f_fsidx.__fsid_val[1] = makefstype(MOUNT_ADOSFS);
	mp->mnt_stat.f_fsid = mp->mnt_stat.f_fsidx.__fsid_val[0];
	mp->mnt_stat.f_namemax = ADMAXNAMELEN;
	mp->mnt_fs_bshift = ffs(amp->bsize) - 1;
	mp->mnt_dev_bshift = DEV_BSHIFT;
	mp->mnt_flag |= MNT_LOCAL;

	/*
	 * get the root anode, if not a valid fs this will fail.
	 */
	if ((error = VFS_ROOT(mp, &rvp)) != 0)
		goto fail;
	/* allocate and load bitmap, set free space */
	bitmap_sz = ((amp->numblks + 31) / 32) * sizeof(*amp->bitmap);
	amp->bitmap = kmem_alloc(bitmap_sz, KM_SLEEP);
	if (amp->bitmap)
		adosfs_loadbitmap(amp);
	if (mp->mnt_flag & MNT_RDONLY && amp->bitmap) {
		/*
		 * Don't need the bitmap any more if it's read-only.
		 */
		kmem_free(amp->bitmap, bitmap_sz);
		amp->bitmap = NULL;
	}
	vput(rvp);

	return(0);

fail:
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
	(void) VOP_CLOSE(devvp, FREAD, NOCRED);
	VOP_UNLOCK(devvp);
	if (amp && amp->bitmap)
		kmem_free(amp->bitmap, bitmap_sz);
	if (amp)
		kmem_free(amp, sizeof(*amp));
	return (error);
}

int
adosfs_start(struct mount *mp, int flags)
{

	return (0);
}

int
adosfs_unmount(struct mount *mp, int mntflags)
{
	struct adosfsmount *amp;
	int error, flags;

	flags = 0;
	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;
	if ((error = vflush(mp, NULLVP, flags)) != 0)
		return (error);
	amp = VFSTOADOSFS(mp);
	if (amp->devvp->v_type != VBAD)
		spec_node_setmountedfs(amp->devvp, NULL);
	vn_lock(amp->devvp, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_CLOSE(amp->devvp, FREAD, NOCRED);
	vput(amp->devvp);
	if (amp->bitmap) {
		size_t bitmap_sz = ((amp->numblks + 31) / 32) *
		    sizeof(*amp->bitmap);
		kmem_free(amp->bitmap, bitmap_sz);
	}
	kmem_free(amp, sizeof(*amp));
	mp->mnt_data = NULL;
	mp->mnt_flag &= ~MNT_LOCAL;
	return (error);
}

int
adosfs_root(struct mount *mp, struct vnode **vpp)
{
	struct vnode *nvp;
	int error;

	if ((error = VFS_VGET(mp, (ino_t)VFSTOADOSFS(mp)->rootb, &nvp)) != 0)
		return (error);
	/* XXX verify it's a root block? */
	*vpp = nvp;
	return (0);
}

int
adosfs_statvfs(struct mount *mp, struct statvfs *sbp)
{
	struct adosfsmount *amp;

	amp = VFSTOADOSFS(mp);
	sbp->f_bsize = amp->bsize;
	sbp->f_frsize = amp->bsize;
	sbp->f_iosize = amp->dbsize;
	sbp->f_blocks = amp->numblks;
	sbp->f_bfree = amp->freeblks;
	sbp->f_bavail = amp->freeblks;
	sbp->f_bresvd = 0;
	sbp->f_files = 0;		/* who knows */
	sbp->f_ffree = 0;		/* " " */
	sbp->f_favail = 0;		/* " " */
	sbp->f_fresvd = 0;
	copy_statvfs_info(sbp, mp);
	return (0);
}

/*
 * lookup an anode, if not found, create
 * return locked and referenced
 */
int
adosfs_vget(struct mount *mp, ino_t an, struct vnode **vpp)
{
	int error;

	error = vcache_get(mp, &an, sizeof(an), vpp);
	if (error)
		return error;
	error = vn_lock(*vpp, LK_EXCLUSIVE);
	if (error) {
		vrele(*vpp);
		*vpp = NULL;
		return error;
	}
	return 0;
}

/*
 * Initialize this vnode / anode pair.
 * Caller assures no other thread will try to load this inode.
 */
int
adosfs_loadvnode(struct mount *mp, struct vnode *vp,
    const void *key, size_t key_len, const void **new_key)
{
	struct adosfsmount *amp;
	struct anode *ap;
	struct buf *bp;
	ino_t an;
	char *nam, *tmp;
	int namlen, error;

	KASSERT(key_len == sizeof(an));
	memcpy(&an, key, key_len);
	amp = VFSTOADOSFS(mp);

	if ((error = bread(amp->devvp, an * amp->bsize / DEV_BSIZE,
			   amp->bsize, 0, &bp)) != 0)
		return error;

	ap = pool_get(&adosfs_node_pool, PR_WAITOK);
	memset(ap, 0, sizeof(struct anode));
	ap->vp = vp;
	ap->amp = amp;
	ap->block = an;
	ap->nwords = amp->nwords;

	/*
	 * get type and fill rest in based on that.
	 */
	switch (ap->type = adosfs_getblktype(amp, bp)) {
	case AROOT:
		vp->v_type = VDIR;
		vp->v_vflag |= VV_ROOT;
		ap->mtimev.days = adoswordn(bp, ap->nwords - 10);
		ap->mtimev.mins = adoswordn(bp, ap->nwords - 9);
		ap->mtimev.ticks = adoswordn(bp, ap->nwords - 8);
		ap->created.days = adoswordn(bp, ap->nwords - 7);
		ap->created.mins = adoswordn(bp, ap->nwords - 6);
		ap->created.ticks = adoswordn(bp, ap->nwords - 5);
		break;
	case ALDIR:
	case ADIR:
		vp->v_type = VDIR;
		break;
	case ALFILE:
	case AFILE:
		vp->v_type = VREG;
		ap->fsize = adoswordn(bp, ap->nwords - 47);
		break;
	case ASLINK:		/* XXX soft link */
		vp->v_type = VLNK;
		/*
		 * convert from BCPL string and
		 * from: "part:dir/file" to: "/part/dir/file"
		 */
		nam = (char *)bp->b_data + (6 * sizeof(long));
		namlen = strlen(nam);
		tmp = nam;
		while (*tmp && *tmp != ':')
			tmp++;
		if (*tmp == 0) {
			ap->slinkto = malloc(namlen + 1, M_ANODE, M_WAITOK);
			memcpy(ap->slinkto, nam, namlen);
		} else if (*nam == ':') {
			ap->slinkto = malloc(namlen + 1, M_ANODE, M_WAITOK);
			memcpy(ap->slinkto, nam, namlen);
			ap->slinkto[0] = '/';
		} else {
			ap->slinkto = malloc(namlen + 2, M_ANODE, M_WAITOK);
			ap->slinkto[0] = '/';
			memcpy(&ap->slinkto[1], nam, namlen);
			ap->slinkto[tmp - nam + 1] = '/';
			namlen++;
		}
		ap->slinkto[namlen] = 0;
		ap->fsize = namlen;
		break;
	default:
		error = EINVAL;
		goto bad;
	}

	/*
	 * Get appropriate data from this block;  hard link needs
	 * to get other data from the "real" block.
	 */

	/*
	 * copy in name (from original block)
	 */
	nam = (char *)bp->b_data + (ap->nwords - 20) * sizeof(u_int32_t);
	namlen = *(u_char *)nam++;
	if (namlen > 30) {
#ifdef DIAGNOSTIC
		printf("adosfs: aget: name length too long blk %llu\n",
		    (unsigned long long)an);
#endif
		error = EINVAL;
		goto bad;
	}
	memcpy(ap->name, nam, namlen);
	ap->name[namlen] = 0;

	/*
	 * if dir alloc hash table and copy it in
	 */
	if (vp->v_type == VDIR) {
		int i;

		ap->tab = malloc(ANODETABSZ(ap) * 2, M_ANODE, M_WAITOK);
		ap->ntabent = ANODETABENT(ap);
		ap->tabi = (int *)&ap->tab[ap->ntabent];
		memset(ap->tabi, 0, ANODETABSZ(ap));
		for (i = 0; i < ap->ntabent; i++)
			ap->tab[i] = adoswordn(bp, i + 6);
	}

	/*
	 * misc.
	 */
	ap->pblock = adoswordn(bp, ap->nwords - 3);
	ap->hashf = adoswordn(bp, ap->nwords - 4);
	ap->linknext = adoswordn(bp, ap->nwords - 10);
	ap->linkto = adoswordn(bp, ap->nwords - 11);

	/*
	 * setup last indirect block cache.
	 */
	ap->lastlindblk = 0;
	if (ap->type == AFILE)  {
		ap->lastindblk = ap->block;
		if (adoswordn(bp, ap->nwords - 10))
			ap->linkto = ap->block;
	} else if (ap->type == ALFILE) {
		ap->lastindblk = ap->linkto;
		brelse(bp, 0);
		bp = NULL;
		error = bread(amp->devvp, ap->linkto * amp->bsize / DEV_BSIZE,
		    amp->bsize, 0, &bp);
		if (error)
			goto bad;
		ap->fsize = adoswordn(bp, ap->nwords - 47);
	}

	if (ap->type == AROOT) {
		ap->adprot = 15;
		ap->uid = amp->uid;
		ap->gid = amp->gid;
	} else {
		ap->adprot = adoswordn(bp, ap->nwords - 48) ^ 15;
		/*
		 * ADOS directories do not have a `x' protection bit as
		 * it is known in VFS; this functionality is fulfilled
		 * by the ADOS `r' bit.
		 *
		 * To retain the ADOS behaviour, fake execute permissions
		 * in that case.
		 */
		if ((ap->type == ADIR || ap->type == ALDIR) &&
		    (ap->adprot & 0x00000008) == 0)
			ap->adprot &= ~0x00000002;

		/*
		 * Get uid/gid from extensions in file header
		 * (really need to know if this is a muFS partition)
		 */
		ap->uid = (adoswordn(bp, ap->nwords - 49) >> 16) & 0xffff;
		ap->gid = adoswordn(bp, ap->nwords - 49) & 0xffff;
		if (ap->uid || ap->gid) {
			if (ap->uid == 0xffff)
				ap->uid = 0;
			if (ap->gid == 0xffff)
				ap->gid = 0;
			ap->adprot |= 0x40000000;	/* Kludge */
		}
		else {
			/*
			 * uid & gid extension don't exist,
			 * so use the mount-point uid/gid
			 */
			ap->uid = amp->uid;
			ap->gid = amp->gid;
		}
	}
	ap->mtime.days = adoswordn(bp, ap->nwords - 23);
	ap->mtime.mins = adoswordn(bp, ap->nwords - 22);
	ap->mtime.ticks = adoswordn(bp, ap->nwords - 21);

	brelse(bp, 0);
	vp->v_tag = VT_ADOSFS;
	vp->v_op = adosfs_vnodeop_p;
	vp->v_data = ap;
	genfs_node_init(vp, &adosfs_genfsops);
	uvm_vnp_setsize(vp, ap->fsize);
	*new_key = &ap->block;
	return 0;

bad:
	if (bp)
		brelse(bp, 0);
	pool_put(&adosfs_node_pool, ap);
	return error;
}

/*
 * Load the bitmap into memory, and count the number of available
 * blocks.
 * The bitmap will be released if the filesystem is read-only;  it's
 * only needed to find the free space.
 */
int
adosfs_loadbitmap(struct adosfsmount *amp)
{
	struct buf *bp, *mapbp;
	u_long bn;
	int blkix, endix, mapix;
	int bmsize;
	int error;

	bp = mapbp = NULL;
	bn = amp->rootb;
	if ((error = bread(amp->devvp, bn * amp->bsize / DEV_BSIZE, amp->bsize,
	    0, &bp)) != 0) {
		return (error);
	}
	blkix = amp->nwords - 49;
	endix = amp->nwords - 24;
	mapix = 0;
	bmsize = (amp->numblks + 31) / 32;
	while (mapix < bmsize) {
		int n;
		u_long bits;

		if (adoswordn(bp, blkix) == 0)
			break;
		if (mapbp != NULL)
			brelse(mapbp, 0);
		if ((error = bread(amp->devvp,
		    adoswordn(bp, blkix) * amp->bsize / DEV_BSIZE, amp->bsize,
		     0, &mapbp)) != 0)
			break;
		if (adoscksum(mapbp, amp->nwords)) {
#ifdef DIAGNOSTIC
			printf("adosfs: loadbitmap - cksum of blk %d failed\n",
			    adoswordn(bp, blkix));
#endif
			/* XXX Force read-only?  Set free space 0? */
			break;
		}
		n = 1;
		while (n < amp->nwords && mapix < bmsize) {
			amp->bitmap[mapix++] = bits = adoswordn(mapbp, n);
			++n;
			if (mapix == bmsize && amp->numblks & 31)
				bits &= ~(0xffffffff << (amp->numblks & 31));
			while (bits) {
				if (bits & 1)
					++amp->freeblks;
				bits >>= 1;
			}
		}
		++blkix;
		if (mapix < bmsize && blkix == endix) {
			bn = adoswordn(bp, blkix);
			brelse(bp, 0);
			if ((error = bread(amp->devvp, bn * amp->bsize / DEV_BSIZE,
			    amp->bsize, 0, &bp)) != 0)
				break;
			/*
			 * Why is there no checksum on these blocks?
			 */
			blkix = 0;
			endix = amp->nwords - 1;
		}
	}
	if (bp)
		brelse(bp, 0);
	if (mapbp)
		brelse(mapbp, 0);
	return (error);
}


/*
 * File handle to vnode
 *
 * Have to be really careful about stale file handles:
 * - check that the inode number is in range
 * - call iget() to get the locked inode
 * - check for an unallocated inode (i_mode == 0)
 * - check that the generation number matches
 */

struct ifid {
	ushort	ifid_len;
	ushort	ifid_pad;
	int	ifid_ino;
	long	ifid_start;
};

int
adosfs_fhtovp(struct mount *mp, struct fid *fhp, struct vnode **vpp)
{
	struct ifid ifh;
#if 0
	struct anode *ap;
#endif
	struct vnode *nvp;
	int error;

	if (fhp->fid_len != sizeof(struct ifid))
		return EINVAL;

#ifdef ADOSFS_DIAGNOSTIC
	printf("adfhtovp(%p, %p, %p)\n", mp, fhp, vpp);
#endif

	memcpy(&ifh, fhp, sizeof(ifh));

	if ((error = VFS_VGET(mp, ifh.ifid_ino, &nvp)) != 0) {
		*vpp = NULLVP;
		return (error);
	}
#if 0
	ap = VTOA(nvp);
	if (ap->inode.iso_mode == 0) {
		vput(nvp);
		*vpp = NULLVP;
		return (ESTALE);
	}
#endif
	*vpp = nvp;
	return(0);
}

int
adosfs_vptofh(struct vnode *vp, struct fid *fhp, size_t *fh_size)
{
	struct anode *ap = VTOA(vp);
	struct ifid ifh;

	if (*fh_size < sizeof(struct ifid)) {
		*fh_size = sizeof(struct ifid);
		return E2BIG;
	}
	*fh_size = sizeof(struct ifid);

	memset(&ifh, 0, sizeof(ifh));
	ifh.ifid_len = sizeof(struct ifid);
	ifh.ifid_ino = ap->block;
	ifh.ifid_start = ap->block;
	memcpy(fhp, &ifh, sizeof(ifh));

#ifdef ADOSFS_DIAGNOSTIC
	printf("advptofh(%p, %p)\n", vp, fhp);
#endif
	return(0);
}

int
adosfs_sync(struct mount *mp, int waitfor, kauth_cred_t uc)
{
#ifdef ADOSFS_DIAGNOSTIC
	printf("ad_sync(%p, %d)\n", mp, waitfor);
#endif
	return(0);
}

void
adosfs_init(void)
{

	malloc_type_attach(M_ANODE);
	pool_init(&adosfs_node_pool, sizeof(struct anode), 0, 0, 0, "adosndpl",
	    &pool_allocator_nointr, IPL_NONE);
}

void
adosfs_done(void)
{

	pool_destroy(&adosfs_node_pool);
	malloc_type_detach(M_ANODE);
}

/*
 * vfs generic function call table
 */

extern const struct vnodeopv_desc adosfs_vnodeop_opv_desc;

const struct vnodeopv_desc *adosfs_vnodeopv_descs[] = {
	&adosfs_vnodeop_opv_desc,
	NULL,
};

struct vfsops adosfs_vfsops = {
	.vfs_name = MOUNT_ADOSFS,
	.vfs_min_mount_data = sizeof (struct adosfs_args),
	.vfs_mount = adosfs_mount,
	.vfs_start = adosfs_start,
	.vfs_unmount = adosfs_unmount,
	.vfs_root = adosfs_root,
	.vfs_quotactl = (void *)eopnotsupp,
	.vfs_statvfs = adosfs_statvfs,
	.vfs_sync = adosfs_sync,
	.vfs_vget = adosfs_vget,
	.vfs_loadvnode = adosfs_loadvnode,
	.vfs_fhtovp = adosfs_fhtovp,
	.vfs_vptofh = adosfs_vptofh,
	.vfs_init = adosfs_init,
	.vfs_done = adosfs_done,
	.vfs_snapshot = (void *)eopnotsupp,
	.vfs_extattrctl = vfs_stdextattrctl,
	.vfs_suspendctl = (void *)eopnotsupp,
	.vfs_renamelock_enter = genfs_renamelock_enter,
	.vfs_renamelock_exit = genfs_renamelock_exit,
	.vfs_fsync = (void *)eopnotsupp,
	.vfs_opv_descs = adosfs_vnodeopv_descs
};

static int
adosfs_modcmd(modcmd_t cmd, void *arg)
{
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = vfs_attach(&adosfs_vfsops);
		if (error != 0)
			break;
		sysctl_createv(&adosfs_sysctl_log, 0, NULL, NULL,
			       CTLFLAG_PERMANENT,
			       CTLTYPE_NODE, "adosfs",
			       SYSCTL_DESCR("AmigaDOS file system"),
			       NULL, 0, NULL, 0,
			       CTL_VFS, 16, CTL_EOL);
		/*
		 * XXX the "16" above could be dynamic, thereby eliminating
		 * one more instance of the "number to vfs" mapping problem,
		 * but "16" is the order as taken from sys/mount.h
		 */
		break;
	case MODULE_CMD_FINI:
		error = vfs_detach(&adosfs_vfsops);
		if (error != 0)
			break;
		sysctl_teardown(&adosfs_sysctl_log);
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

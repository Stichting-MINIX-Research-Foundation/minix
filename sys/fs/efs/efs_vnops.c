/*	$NetBSD: efs_vnops.c,v 1.33 2014/08/07 08:24:23 hannken Exp $	*/

/*
 * Copyright (c) 2006 Stephen M. Rumble <rumble@ephemeral.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: efs_vnops.c,v 1.33 2014/08/07 08:24:23 hannken Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/dirent.h>
#include <sys/lockf.h>
#include <sys/unistd.h>
#include <sys/buf.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/genfs/genfs_node.h>
#include <miscfs/fifofs/fifo.h>
#include <miscfs/specfs/specdev.h>

#include <fs/efs/efs.h>
#include <fs/efs/efs_sb.h>
#include <fs/efs/efs_dir.h>
#include <fs/efs/efs_genfs.h>
#include <fs/efs/efs_mount.h>
#include <fs/efs/efs_extent.h>
#include <fs/efs/efs_dinode.h>
#include <fs/efs/efs_inode.h>
#include <fs/efs/efs_subr.h>

MALLOC_DECLARE(M_EFSTMP);

/*
 * Lookup a pathname component in the given directory.
 *
 * Returns 0 on success.
 */
static int
efs_lookup(void *v)
{
	struct vop_lookup_v2_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct componentname *cnp = ap->a_cnp;
	struct vnode *vp;
	ino_t ino;
	int err, nameiop = cnp->cn_nameiop;

	/* ensure that the directory can be accessed first */
        err = VOP_ACCESS(ap->a_dvp, VEXEC, cnp->cn_cred);
	if (err)
		return (err);

	if (cache_lookup(ap->a_dvp, cnp->cn_nameptr, cnp->cn_namelen,
			 cnp->cn_nameiop, cnp->cn_flags, NULL, ap->a_vpp)) {
		return *ap->a_vpp == NULLVP ? ENOENT : 0;
	}

	/*
	 * Handle the lookup types: '.' or everything else.
	 */
	if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
		vref(ap->a_dvp);
		*ap->a_vpp = ap->a_dvp;
	} else {
		err = efs_inode_lookup(VFSTOEFS(ap->a_dvp->v_mount),
		    EFS_VTOI(ap->a_dvp), ap->a_cnp, &ino);
		if (err) {
			if (cnp->cn_flags & ISDOTDOT)
				return (err);
			if (err == ENOENT && nameiop != CREATE)
				cache_enter(ap->a_dvp, NULL, cnp->cn_nameptr,
					    cnp->cn_namelen, cnp->cn_flags);
			if (err == ENOENT && (nameiop == CREATE ||
			    nameiop == RENAME)) {
				err = VOP_ACCESS(ap->a_dvp, VWRITE,
				    cnp->cn_cred);
				if (err)
					return (err);
				return (EJUSTRETURN);
			}
			return (err);
		}
		err = vcache_get(ap->a_dvp->v_mount, &ino, sizeof(ino), &vp);
		if (err)
			return (err);
		*ap->a_vpp = vp;
	}

	cache_enter(ap->a_dvp, *ap->a_vpp, cnp->cn_nameptr, cnp->cn_namelen,
		    cnp->cn_flags);

	return 0;
}

static int
efs_check_possible(struct vnode *vp, struct efs_inode *eip, mode_t mode)
{

	if ((mode & VWRITE) && (vp->v_mount->mnt_flag & MNT_RDONLY))
		return (EROFS);

	return 0;
}

/*
 * Determine the accessiblity of a file based on the permissions allowed by the
 * specified credentials.
 *
 * Returns 0 on success.
 */
static int
efs_check_permitted(struct vnode *vp, struct efs_inode *eip, mode_t mode,
    kauth_cred_t cred)
{

	return kauth_authorize_vnode(cred, KAUTH_ACCESS_ACTION(mode,
	    vp->v_type, eip->ei_mode), vp, NULL, genfs_can_access(vp->v_type,
	    eip->ei_mode, eip->ei_uid, eip->ei_gid, mode, cred));
}

static int
efs_access(void *v)
{
	struct vop_access_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int a_mode;
		struct ucred *a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct efs_inode *eip = EFS_VTOI(vp);
	int error;

	error = efs_check_possible(vp, eip, ap->a_mode);
	if (error)
		return error;

	error = efs_check_permitted(vp, eip, ap->a_mode, ap->a_cred);

	return error;
}

/*
 * Get specific vnode attributes on a file. See vattr(9).
 *
 * Returns 0 on success.
 */
static int
efs_getattr(void *v)
{
	struct vop_getattr_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct vattr *a_vap; 
		struct ucred *a_cred;
	} */ *ap = v;

	struct vattr *vap = ap->a_vap;
	struct efs_inode *eip = EFS_VTOI(ap->a_vp);

	vattr_null(ap->a_vap);
	vap->va_type		= ap->a_vp->v_type;
	vap->va_mode		= eip->ei_mode;
	vap->va_nlink		= eip->ei_nlink;
	vap->va_uid		= eip->ei_uid;
	vap->va_gid		= eip->ei_gid;
	vap->va_fsid 		= ap->a_vp->v_mount->mnt_stat.f_fsid;
	vap->va_fileid 		= eip->ei_number;
	vap->va_size 		= eip->ei_size;

	if (ap->a_vp->v_type == VBLK)
		vap->va_blocksize = BLKDEV_IOSIZE;
	else if (ap->a_vp->v_type == VCHR)
		vap->va_blocksize = MAXBSIZE;
	else
		vap->va_blocksize = EFS_BB_SIZE;

	vap->va_atime.tv_sec	= eip->ei_atime;
	vap->va_mtime.tv_sec	= eip->ei_mtime;
	vap->va_ctime.tv_sec	= eip->ei_ctime;
/*	vap->va_birthtime 	= */
	vap->va_gen		= eip->ei_gen;
	vap->va_flags		= ap->a_vp->v_vflag |
	    ap->a_vp->v_iflag | ap->a_vp->v_uflag;

	if (ap->a_vp->v_type == VBLK || ap->a_vp->v_type == VCHR) {
		uint32_t dmaj, dmin;

		if (be16toh(eip->ei_di.di_odev) != EFS_DINODE_ODEV_INVALID) {
			dmaj = EFS_DINODE_ODEV_MAJ(be16toh(eip->ei_di.di_odev));
			dmin = EFS_DINODE_ODEV_MIN(be16toh(eip->ei_di.di_odev));
		} else {
			dmaj = EFS_DINODE_NDEV_MAJ(be32toh(eip->ei_di.di_ndev));
			dmin = EFS_DINODE_NDEV_MIN(be32toh(eip->ei_di.di_ndev));
		}

		vap->va_rdev = makedev(dmaj, dmin);
	}

	vap->va_bytes		= eip->ei_size;
/*	vap->va_filerev		= */
/*	vap->va_vaflags		= */
	
	return (0);
}

/*
 * Read a file.
 *
 * Returns 0 on success.
 */
static int
efs_read(void *v)
{
	struct vop_read_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct uio *a_uio; 
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	struct efs_extent ex;
	struct efs_extent_iterator exi;
	struct uio *uio = ap->a_uio;
	struct efs_inode *eip = EFS_VTOI(ap->a_vp);
	off_t start;
	vsize_t len;
	int err, ret;
	const int advice = IO_ADV_DECODE(ap->a_ioflag);

	if (ap->a_vp->v_type == VDIR)
		return (EISDIR);

	if (ap->a_vp->v_type != VREG)
		return (EINVAL);

	efs_extent_iterator_init(&exi, eip, uio->uio_offset);
	ret = efs_extent_iterator_next(&exi, &ex);
	while (ret == 0) {
		if (uio->uio_offset < 0 || uio->uio_offset >= eip->ei_size ||
		    uio->uio_resid == 0)
			break;

		start = ex.ex_offset * EFS_BB_SIZE;
		len   = ex.ex_length * EFS_BB_SIZE;

		if (!(uio->uio_offset >= start &&
		      uio->uio_offset < (start + len))) {
			ret = efs_extent_iterator_next(&exi, &ex);
			continue;
		}

		start = uio->uio_offset - start;

		len = MIN(len - start, uio->uio_resid);
		len = MIN(len, eip->ei_size - uio->uio_offset);

		err = ubc_uiomove(&ap->a_vp->v_uobj, uio, len, advice,
		    UBC_READ | UBC_PARTIALOK | UBC_UNMAP_FLAG(ap->a_vp));
		if (err) {
			EFS_DPRINTF(("efs_read: uiomove error %d\n",
			    err));
			return (err);
		}
	}

	return ((ret == -1) ? 0 : ret);
}

static int
efs_readdir(void *v)
{
	struct vop_readdir_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp; 
		struct uio *a_uio; 
		struct ucred *a_cred;
		int *a_eofflag;
		off_t **a_cookies;
		int *a_ncookies;
	} */ *ap = v;
	struct dirent *dp;
	struct efs_dinode edi;
	struct efs_extent ex;
	struct efs_extent_iterator exi;
	struct buf *bp;
	struct efs_dirent *de;
	struct efs_dirblk *db;
	struct uio *uio = ap->a_uio;
	struct efs_inode *ei = EFS_VTOI(ap->a_vp);
	off_t *cookies = NULL;
	off_t offset;
	int i, j, err, ret, s, slot, ncookies, maxcookies = 0;

	if (ap->a_vp->v_type != VDIR)
		return (ENOTDIR);

	if (ap->a_eofflag != NULL)
		*ap->a_eofflag = false;

	if (ap->a_ncookies != NULL) {
		ncookies = 0;
		maxcookies =
		    uio->uio_resid / _DIRENT_MINSIZE((struct dirent *)0);
		cookies = malloc(maxcookies * sizeof(off_t), M_TEMP, M_WAITOK);
 	}

	dp = malloc(sizeof(struct dirent), M_EFSTMP, M_WAITOK | M_ZERO);

	offset = 0;
	efs_extent_iterator_init(&exi, ei, 0);
	while ((ret = efs_extent_iterator_next(&exi, &ex)) == 0) {
		for (i = 0; i < ex.ex_length; i++) {
			err = efs_bread(VFSTOEFS(ap->a_vp->v_mount),
			    ex.ex_bn + i, NULL, &bp);
			if (err) {
				goto exit_err;
			}

			db = (struct efs_dirblk *)bp->b_data;

			if (be16toh(db->db_magic) != EFS_DIRBLK_MAGIC) {
				printf("efs_readdir: bad dirblk\n");
				brelse(bp, 0);
				continue;
			}

			for (j = 0; j < db->db_slots; j++) {
				slot = EFS_DIRENT_OFF_EXPND(db->db_space[j]);
				if (slot == EFS_DIRBLK_SLOT_FREE)
					continue;

				if (!EFS_DIRENT_OFF_VALID(slot)) {
					printf("efs_readdir: bad dirent\n");
					continue;
				}

				de = EFS_DIRBLK_TO_DIRENT(db, slot);
				s = _DIRENT_RECLEN(dp, de->de_namelen);

				if (offset < uio->uio_offset) {
					offset += s;
					continue;
				}

				/* XXX - shouldn't happen, right? */
				if (offset > uio->uio_offset ||
				    s > uio->uio_resid) {
					brelse(bp, 0);
					goto exit_ok;
				}

				/* de_namelen is uint8_t, d.d_name is 512b */
				KASSERT(sizeof(dp->d_name)-de->de_namelen > 0);
				dp->d_fileno = be32toh(de->de_inumber);
				dp->d_reclen = s;
				dp->d_namlen = de->de_namelen;
				memcpy(dp->d_name, de->de_name,
				    de->de_namelen);
				dp->d_name[de->de_namelen] = '\0';

				/* look up inode to get type */
				err = efs_read_inode(
				    VFSTOEFS(ap->a_vp->v_mount),
				    dp->d_fileno, NULL, &edi);
				if (err) {
					brelse(bp, 0);
					goto exit_err;
				}

				switch (be16toh(edi.di_mode) & EFS_IFMT) {
				case EFS_IFIFO:
					dp->d_type = DT_FIFO;
					break;
				case EFS_IFCHR:
					dp->d_type = DT_CHR;
					break;
				case EFS_IFDIR:
					dp->d_type = DT_DIR;
					break;
				case EFS_IFBLK:
					dp->d_type = DT_BLK;
					break;
				case EFS_IFREG:
					dp->d_type = DT_REG;
					break;
				case EFS_IFLNK:
					dp->d_type = DT_LNK;
					break;
				case EFS_IFSOCK:
					dp->d_type = DT_SOCK;
					break;
				default:
					dp->d_type = DT_UNKNOWN;
					break;
				}

				err = uiomove(dp, s, uio);
				if (err) {
					brelse(bp, 0);
					goto exit_err;	
				}

				offset += s;

				if (cookies != NULL && maxcookies != 0) {
					cookies[ncookies++] = offset;
					if (ncookies == maxcookies) {
						brelse(bp, 0);
						goto exit_ok;
					}
				}
			}

			brelse(bp, 0);
		}
	}

	if (ret != -1) {
		err = ret;
		goto exit_err;
	}

	if (ap->a_eofflag != NULL)
		*ap->a_eofflag = true;

 exit_ok:
	if (cookies != NULL) {
		*ap->a_cookies = cookies;
		*ap->a_ncookies = ncookies;
	}

	uio->uio_offset = offset;

	free(dp, M_EFSTMP);

	return (0);

 exit_err:
	if (cookies != NULL)
		free(cookies, M_TEMP);

	free(dp, M_EFSTMP);
	
	return (err);
}

static int
efs_readlink(void *v)
{
	struct vop_readlink_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *ap = v;
	struct uio *uio = ap->a_uio;
	struct efs_inode *eip = EFS_VTOI(ap->a_vp);
	char *buf;
	size_t len;
	int err, i;

	if ((eip->ei_mode & EFS_IFMT) != EFS_IFLNK)
		return (EINVAL);

	if (uio->uio_resid < 1)
		return (EINVAL);

	buf = malloc(eip->ei_size + 1, M_EFSTMP, M_ZERO | M_WAITOK);

	/* symlinks are either inlined in the inode, or in extents */
	if (eip->ei_numextents == 0) {
		if (eip->ei_size > sizeof(eip->ei_di.di_symlink)) {
			EFS_DPRINTF(("efs_readlink: too big for inline\n"));
			free(buf, M_EFSTMP);
			return (EBADF);
		}

		memcpy(buf, eip->ei_di.di_symlink, eip->ei_size);
		len = MIN(uio->uio_resid, eip->ei_size + 1);
	} else {
		struct efs_extent_iterator exi;
		struct efs_extent ex;
		struct buf *bp;
		int resid, off, ret;

		off = 0;
		resid = eip->ei_size;

		efs_extent_iterator_init(&exi, eip, 0);
		while ((ret = efs_extent_iterator_next(&exi, &ex)) == 0) {
			for (i = 0; i < ex.ex_length; i++) {
				err = efs_bread(VFSTOEFS(ap->a_vp->v_mount),
				    ex.ex_bn + i, NULL, &bp);
				if (err) {
					free(buf, M_EFSTMP);
					return (err);
				}

				len = MIN(resid, bp->b_bcount);
				memcpy(buf + off, bp->b_data, len);
				brelse(bp, 0);

				off += len;
				resid -= len;

				if (resid == 0)
					break;
			}

			if (resid == 0)
				break;
		}

		if (ret != 0 && ret != -1) {
			free(buf, M_EFSTMP);
			return (ret);
		}

		len = off + 1;
	}

	KASSERT(len >= 1 && len <= (eip->ei_size + 1));
	buf[len - 1] = '\0';
	err = uiomove(buf, len, uio);
	free(buf, M_EFSTMP);

	return (err);
}

/*
 * Release an inactive vnode. The vnode _must_ be unlocked on return.
 * It is either nolonger being used by the kernel, or an unmount is being
 * forced.
 *
 * Returns 0 on success.
 */
static int
efs_inactive(void *v)
{
	struct vop_inactive_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		bool *a_recycle
	} */ *ap = v;
	struct efs_inode *eip = EFS_VTOI(ap->a_vp);

	*ap->a_recycle = (eip->ei_mode == 0);
	VOP_UNLOCK(ap->a_vp);

	return (0);
}

static int
efs_reclaim(void *v)
{
	struct vop_reclaim_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct efs_inode *eip = EFS_VTOI(vp);

	vcache_remove(vp->v_mount, &eip->ei_number, sizeof(eip->ei_number));
	genfs_node_destroy(vp);
	pool_put(&efs_inode_pool, eip);
	vp->v_data = NULL;

	return (0);
}

static int
efs_bmap(void *v)
{
	struct vop_bmap_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		daddr_t a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *ap = v;
	struct efs_extent ex;
	struct efs_extent_iterator exi;
	struct vnode *vp = ap->a_vp;
	struct efs_inode *eip = EFS_VTOI(vp);
	bool found;
	int ret;

	if (ap->a_vpp != NULL)
		*ap->a_vpp = VFSTOEFS(vp->v_mount)->em_devvp;

	found = false;
	efs_extent_iterator_init(&exi, eip, ap->a_bn * EFS_BB_SIZE);
	while ((ret = efs_extent_iterator_next(&exi, &ex)) == 0) {
		if (ap->a_bn >= ex.ex_offset &&
		    ap->a_bn < (ex.ex_offset + ex.ex_length)) {
			found = true;
			break;
		}
	}

	KASSERT(!found || (found && ret == 0));

	if (!found) {
		EFS_DPRINTF(("efs_bmap: ap->a_bn not in extents\n"));
		return ((ret == -1) ? EIO : ret);
	}

	if (ex.ex_magic != EFS_EXTENT_MAGIC) {
		EFS_DPRINTF(("efs_bmap: exn.ex_magic != EFS_EXTENT_MAGIC\n"));
		return (EIO);
	}

	if (ap->a_bn < ex.ex_offset) {
		EFS_DPRINTF(("efs_bmap: ap->a_bn < exn.ex_offset\n"));
		return (EIO);
	}

	KASSERT(ap->a_bn >= ex.ex_offset);
	KASSERT(ex.ex_length > ap->a_bn - ex.ex_offset);

	*ap->a_bnp = ex.ex_bn + (ap->a_bn - ex.ex_offset);
	if (ap->a_runp != NULL)
		*ap->a_runp = ex.ex_length - (ap->a_bn - ex.ex_offset) - 1;

	return (0);
}

static int
efs_strategy(void *v)
{
	struct vop_strategy_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct buf *bp = ap->a_bp;
	int error;

	if (vp == NULL) {
		bp->b_error = EIO;
		biodone(bp);
		return (EIO);
	}

	if (bp->b_blkno == bp->b_lblkno) {
		error = VOP_BMAP(vp, bp->b_lblkno, NULL, &bp->b_blkno, NULL);
		if (error) {
			bp->b_error = error;
			biodone(bp);
			return (error);
		}
		if ((long)bp->b_blkno == -1)
			clrbuf(bp);
	}

	if ((long)bp->b_blkno == -1) {
		biodone(bp);
		return (0);
	}

	return (VOP_STRATEGY(VFSTOEFS(vp->v_mount)->em_devvp, bp));
}

static int
efs_print(void *v)
{
	struct vop_print_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
	} */ *ap = v;
	struct efs_inode *eip = EFS_VTOI(ap->a_vp);

	printf(	"efs_inode (ino %lu):\n"
		"    ei_mode:         %07o\n"
		"    ei_nlink:        %d\n"
		"    ei_uid:          %d\n"
		"    ei_gid:          %d\n"
		"    ei_size:         %d\n"
		"    ei_atime:        %d\n"
		"    ei_mtime:        %d\n"
		"    ei_ctime:        %d\n"
		"    ei_gen:          %d\n"
		"    ei_numextents:   %d\n"
		"    ei_version:      %d\n",
		(unsigned long)eip->ei_number,
		(unsigned int)eip->ei_mode,
		eip->ei_nlink,
		eip->ei_uid,
		eip->ei_gid,
		eip->ei_size,
		(int32_t)eip->ei_atime,
		(int32_t)eip->ei_mtime,
		(int32_t)eip->ei_ctime,
		eip->ei_gen,
		eip->ei_numextents,
		eip->ei_version);

	return (0);
}

static int
efs_pathconf(void *v)
{
	struct vop_pathconf_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;

	/* IRIX 4 values */
	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = 30000; 
		break;
	case _PC_NAME_MAX:
		*ap->a_retval = 255;
		break;
	case _PC_PATH_MAX:
		*ap->a_retval = 1024;
		break;
	case _PC_NO_TRUNC:
		*ap->a_retval = 1;
		break;
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		break;
	case _PC_SYNC_IO:
		*ap->a_retval = 1;
		break;
	case _PC_FILESIZEBITS:
		*ap->a_retval = 32;
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

static int
efs_advlock(void *v)
{
	struct vop_advlock_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		void *a_id;
		int a_op;
		struct flock *a_fl;
		int a_flags;
	} */ *ap = v;
	struct efs_inode *eip = EFS_VTOI(ap->a_vp);

	return (lf_advlock(ap, &eip->ei_lockf, eip->ei_size));
}

/* Global vfs data structures for efs */
int (**efs_vnodeop_p)(void *);
const struct vnodeopv_entry_desc efs_vnodeop_entries[] = {
	{ &vop_default_desc,	vn_default_error},	/* error handler */
	{ &vop_lookup_desc,	efs_lookup	},	/* lookup */
	{ &vop_create_desc,	genfs_eopnotsupp},	/* create */
	{ &vop_mknod_desc,	genfs_eopnotsupp},	/* mknod */
	{ &vop_open_desc,	genfs_nullop	},	/* open */
	{ &vop_close_desc,	genfs_nullop	},	/* close */
	{ &vop_access_desc,	efs_access	},	/* access */
	{ &vop_getattr_desc,	efs_getattr	},	/* getattr */
	{ &vop_setattr_desc,	genfs_eopnotsupp},	/* setattr */
	{ &vop_read_desc,	efs_read	},	/* read */
	{ &vop_write_desc,	genfs_eopnotsupp},	/* write */
	{ &vop_fallocate_desc,	genfs_eopnotsupp},	/* fallocate */
	{ &vop_fdiscard_desc,	genfs_eopnotsupp},	/* fdiscard */
	{ &vop_ioctl_desc,	genfs_enoioctl	},	/* ioctl */
	{ &vop_fcntl_desc,	genfs_fcntl	},	/* fcntl */
	{ &vop_poll_desc,	genfs_poll	},	/* poll */
	{ &vop_kqfilter_desc,	genfs_kqfilter	},	/* kqfilter */
	{ &vop_revoke_desc,	genfs_revoke	},	/* revoke */
	{ &vop_mmap_desc,	genfs_mmap	},	/* mmap */
	{ &vop_fsync_desc,	genfs_eopnotsupp},	/* fsync */
	{ &vop_seek_desc,	genfs_seek	},	/* seek */
	{ &vop_remove_desc,	genfs_eopnotsupp},	/* remove */
	{ &vop_link_desc,	genfs_eopnotsupp},	/* link */
	{ &vop_rename_desc,	genfs_eopnotsupp},	/* rename */
	{ &vop_mkdir_desc,	genfs_eopnotsupp},	/* mkdir */
	{ &vop_rmdir_desc,	genfs_eopnotsupp},	/* rmdir */
	{ &vop_symlink_desc,	genfs_eopnotsupp},	/* symlink */
	{ &vop_readdir_desc,	efs_readdir	},	/* readdir */
	{ &vop_readlink_desc,	efs_readlink	},	/* readlink */
	{ &vop_abortop_desc,	genfs_abortop	},	/* abortop */
	{ &vop_inactive_desc,	efs_inactive	},	/* inactive */
	{ &vop_reclaim_desc,	efs_reclaim	},	/* reclaim */
	{ &vop_lock_desc,	genfs_lock,	},	/* lock */
	{ &vop_unlock_desc,	genfs_unlock,	},	/* unlock */
	{ &vop_islocked_desc,	genfs_islocked,	},	/* islocked */
	{ &vop_bmap_desc,	efs_bmap	},	/* bmap */
	{ &vop_print_desc,	efs_print	},	/* print */
	{ &vop_pathconf_desc,	efs_pathconf	},	/* pathconf */
	{ &vop_advlock_desc,	efs_advlock	},	/* advlock */
							/* blkatoff */
							/* valloc */
							/* balloc */
							/* vfree */
							/* truncate */
							/* whiteout */
	{ &vop_getpages_desc,	genfs_getpages	},	/* getpages */
	{ &vop_putpages_desc,	genfs_putpages	},	/* putpages */
	{ &vop_bwrite_desc,	vn_bwrite	},	/* bwrite */
	{ &vop_strategy_desc,	efs_strategy	},	/* strategy */
	{ NULL, NULL }
};
const struct vnodeopv_desc efs_vnodeop_opv_desc = {
	&efs_vnodeop_p,
	efs_vnodeop_entries
};

int (**efs_specop_p)(void *);
const struct vnodeopv_entry_desc efs_specop_entries[] = {
	{ &vop_default_desc,	vn_default_error},	/* error handler */
	{ &vop_lookup_desc,	spec_lookup	},	/* lookup */
	{ &vop_create_desc,	spec_create	},	/* create */
	{ &vop_mknod_desc,	spec_mknod	},	/* mknod */
	{ &vop_open_desc,	spec_open	},	/* open */
	{ &vop_close_desc,	spec_close	},	/* close */
	{ &vop_access_desc,	efs_access	},	/* access */
	{ &vop_getattr_desc,	efs_getattr	},	/* getattr */
	{ &vop_setattr_desc,	genfs_eopnotsupp},	/* setattr */
	{ &vop_read_desc,	spec_read	},	/* read */
	{ &vop_write_desc,	spec_write	},	/* write */
	{ &vop_fallocate_desc,	spec_fallocate	},	/* fallocate */
	{ &vop_fdiscard_desc,	spec_fdiscard	},	/* fdiscard */
	{ &vop_ioctl_desc,	spec_ioctl	},	/* ioctl */
	{ &vop_fcntl_desc,	genfs_fcntl	},	/* fcntl */
	{ &vop_poll_desc,	spec_poll	},	/* poll */
	{ &vop_kqfilter_desc,	spec_kqfilter	},	/* kqfilter */
	{ &vop_revoke_desc,	spec_revoke	},	/* revoke */
	{ &vop_mmap_desc,	spec_mmap	},	/* mmap */
	{ &vop_fsync_desc,	spec_fsync	},	/* fsync */
	{ &vop_seek_desc,	spec_seek	},	/* seek */
	{ &vop_remove_desc,	spec_remove	},	/* remove */
	{ &vop_link_desc,	spec_link	},	/* link */
	{ &vop_rename_desc,	spec_rename	},	/* rename */
	{ &vop_mkdir_desc,	spec_mkdir	},	/* mkdir */
	{ &vop_rmdir_desc,	spec_rmdir	},	/* rmdir */
	{ &vop_symlink_desc,	spec_symlink	},	/* symlink */
	{ &vop_readdir_desc,	spec_readdir	},	/* readdir */
	{ &vop_readlink_desc,	spec_readlink	},	/* readlink */
	{ &vop_abortop_desc,	spec_abortop	},	/* abortop */
	{ &vop_inactive_desc,	efs_inactive	},	/* inactive */
	{ &vop_reclaim_desc,	efs_reclaim	},	/* reclaim */
	{ &vop_lock_desc,	genfs_lock,	},	/* lock */
	{ &vop_unlock_desc,	genfs_unlock,	},	/* unlock */
	{ &vop_islocked_desc,	genfs_islocked,	},	/* islocked */
	{ &vop_bmap_desc,	spec_bmap	},	/* bmap */
	{ &vop_print_desc,	efs_print	},	/* print */
	{ &vop_pathconf_desc,	spec_pathconf	},	/* pathconf */
	{ &vop_advlock_desc,	spec_advlock	},	/* advlock */
							/* blkatoff */
							/* valloc */
							/* balloc */
							/* vfree */
							/* truncate */
							/* whiteout */
	{ &vop_getpages_desc,	spec_getpages	},	/* getpages */
	{ &vop_putpages_desc,	spec_putpages	},	/* putpages */
	{ &vop_bwrite_desc,	vn_bwrite	},	/* bwrite */
	{ &vop_strategy_desc,	spec_strategy	},	/* strategy */
	{ NULL, NULL }
};
const struct vnodeopv_desc efs_specop_opv_desc = {
	&efs_specop_p,
	efs_specop_entries
};

int (**efs_fifoop_p)(void *);
const struct vnodeopv_entry_desc efs_fifoop_entries[] = {
	{ &vop_default_desc,	vn_default_error},	/* error handler */
	{ &vop_lookup_desc,	vn_fifo_bypass	},	/* lookup */
	{ &vop_create_desc,	vn_fifo_bypass	},	/* create */
	{ &vop_mknod_desc,	vn_fifo_bypass	},	/* mknod */
	{ &vop_open_desc,	vn_fifo_bypass	},	/* open */
	{ &vop_close_desc,	vn_fifo_bypass	},	/* close */
	{ &vop_access_desc,	efs_access	},	/* access */
	{ &vop_getattr_desc,	efs_getattr	},	/* getattr */
	{ &vop_setattr_desc,	genfs_eopnotsupp},	/* setattr */
	{ &vop_read_desc,	vn_fifo_bypass	},	/* read */
	{ &vop_write_desc,	vn_fifo_bypass	},	/* write */
	{ &vop_fallocate_desc,	vn_fifo_bypass	},	/* fallocate */
	{ &vop_fdiscard_desc,	vn_fifo_bypass	},	/* fdiscard */
	{ &vop_ioctl_desc,	vn_fifo_bypass	},	/* ioctl */
	{ &vop_fcntl_desc,	genfs_fcntl	},	/* fcntl */
	{ &vop_poll_desc,	vn_fifo_bypass	},	/* poll */
	{ &vop_kqfilter_desc,	vn_fifo_bypass	},	/* kqfilter */
	{ &vop_revoke_desc,	vn_fifo_bypass	},	/* revoke */
	{ &vop_mmap_desc,	vn_fifo_bypass	},	/* mmap */
	{ &vop_fsync_desc,	vn_fifo_bypass	},	/* fsync */
	{ &vop_seek_desc,	vn_fifo_bypass	},	/* seek */
	{ &vop_remove_desc,	vn_fifo_bypass	},	/* remove */
	{ &vop_link_desc,	vn_fifo_bypass	},	/* link */
	{ &vop_rename_desc,	vn_fifo_bypass	},	/* rename */
	{ &vop_mkdir_desc,	vn_fifo_bypass	},	/* mkdir */
	{ &vop_rmdir_desc,	vn_fifo_bypass	},	/* rmdir */
	{ &vop_symlink_desc,	vn_fifo_bypass	},	/* symlink */
	{ &vop_readdir_desc,	vn_fifo_bypass	},	/* readdir */
	{ &vop_readlink_desc,	vn_fifo_bypass	},	/* readlink */
	{ &vop_abortop_desc,	vn_fifo_bypass	},	/* abortop */
	{ &vop_inactive_desc,	efs_inactive	},	/* inactive */
	{ &vop_reclaim_desc,	efs_reclaim	},	/* reclaim */
	{ &vop_lock_desc,	genfs_lock,	},	/* lock */
	{ &vop_unlock_desc,	genfs_unlock,	},	/* unlock */
	{ &vop_islocked_desc,	genfs_islocked,	},	/* islocked */
	{ &vop_bmap_desc,	vn_fifo_bypass	},	/* bmap */
	{ &vop_print_desc,	efs_print	},	/* print */
	{ &vop_pathconf_desc,	vn_fifo_bypass	},	/* pathconf */
	{ &vop_advlock_desc,	vn_fifo_bypass	},	/* advlock */
							/* blkatoff */
							/* valloc */
							/* balloc */
							/* vfree */
							/* truncate */
							/* whiteout */
	{ &vop_bwrite_desc,	vn_bwrite	},	/* bwrite */
	{ &vop_strategy_desc,	vn_fifo_bypass	},	/* strategy */
	{ NULL, NULL }
};
const struct vnodeopv_desc efs_fifoop_opv_desc = {
	&efs_fifoop_p,
	efs_fifoop_entries
};

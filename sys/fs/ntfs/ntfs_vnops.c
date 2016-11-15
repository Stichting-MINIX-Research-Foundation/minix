/*	$NetBSD: ntfs_vnops.c,v 1.59 2014/11/13 16:51:53 hannken Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * John Heidemann of the UCLA Ficus project.
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
 *	Id: ntfs_vnops.c,v 1.5 1999/05/12 09:43:06 semenu Exp
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ntfs_vnops.c,v 1.59 2014/11/13 16:51:53 hannken Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/dirent.h>
#include <sys/kauth.h>
#include <sys/sysctl.h>


#include <fs/ntfs/ntfs.h>
#include <fs/ntfs/ntfs_inode.h>
#include <fs/ntfs/ntfs_subr.h>
#include <fs/ntfs/ntfs_vfsops.h>
#include <miscfs/specfs/specdev.h>
#include <miscfs/genfs/genfs.h>

#include <sys/unistd.h> /* for pathconf(2) constants */

static int	ntfs_bypass(void *);
static int	ntfs_read(void *);
static int	ntfs_write(void *);
static int	ntfs_getattr(void *);
static int	ntfs_inactive(void *);
static int	ntfs_print(void *);
static int	ntfs_reclaim(void *);
static int	ntfs_strategy(void *);
static int	ntfs_access(void *);
static int	ntfs_open(void *);
static int	ntfs_close(void *);
static int	ntfs_readdir(void *);
static int	ntfs_lookup(void *);
static int	ntfs_bmap(void *);
static int	ntfs_fsync(void *);
static int	ntfs_pathconf(void *);

extern int prtactive;

/*
 * This is a noop, simply returning what one has been given.
 */
int
ntfs_bmap(void *v)
{
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
		int *a_runb;
	} */ *ap = v;
	dprintf(("ntfs_bmap: vn: %p, blk: %d\n", ap->a_vp,(u_int32_t)ap->a_bn));
	if (ap->a_vpp != NULL)
		*ap->a_vpp = ap->a_vp;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn;
	if (ap->a_runp != NULL)
		*ap->a_runp = 0;
	return (0);
}

static int
ntfs_read(void *v)
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct fnode *fp = VTOF(vp);
	struct ntnode *ip = FTONT(fp);
	struct uio *uio = ap->a_uio;
	struct ntfsmount *ntmp = ip->i_mp;
	u_int64_t toread;
	int error;

	dprintf(("ntfs_read: ino: %llu, off: %qd resid: %qd\n",
	    (unsigned long long)ip->i_number, (long long)uio->uio_offset,
	    (long long)uio->uio_resid));

	dprintf(("ntfs_read: filesize: %qu",(long long)fp->f_size));

	/* don't allow reading after end of file */
	if (uio->uio_offset > fp->f_size)
		toread = 0;
	else
		toread = MIN(uio->uio_resid, fp->f_size - uio->uio_offset );

	dprintf((", toread: %qu\n",(long long)toread));

	if (toread == 0)
		return (0);

	error = ntfs_readattr(ntmp, ip, fp->f_attrtype,
		fp->f_attrname, uio->uio_offset, toread, NULL, uio);
	if (error) {
		printf("ntfs_read: ntfs_readattr failed: %d\n",error);
		return (error);
	}

	return (0);
}

static int
ntfs_bypass(void *v)
{
	struct vop_generic_args /* {
		struct vnodeop_desc *a_desc;
		<other random data follows, presumably>
	} */ *ap __unused = v;
	int error = ENOTTY;
	dprintf(("ntfs_bypass: %s\n", ap->a_desc->vdesc_name));
	return (error);
}


static int
ntfs_getattr(void *v)
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct fnode *fp = VTOF(vp);
	struct ntnode *ip = FTONT(fp);
	struct vattr *vap = ap->a_vap;

	dprintf(("ntfs_getattr: %llu, flags: %d\n",
	    (unsigned long long)ip->i_number, ip->i_flag));

	vap->va_fsid = ip->i_dev;
	vap->va_fileid = ip->i_number;
	vap->va_mode = ip->i_mp->ntm_mode;
	vap->va_nlink = ip->i_nlink;
	vap->va_uid = ip->i_mp->ntm_uid;
	vap->va_gid = ip->i_mp->ntm_gid;
	vap->va_rdev = 0;				/* XXX UNODEV ? */
	vap->va_size = fp->f_size;
	vap->va_bytes = fp->f_allocated;
	vap->va_atime = ntfs_nttimetounix(fp->f_times.t_access);
	vap->va_mtime = ntfs_nttimetounix(fp->f_times.t_write);
	vap->va_ctime = ntfs_nttimetounix(fp->f_times.t_create);
	vap->va_flags = ip->i_flag;
	vap->va_gen = 0;
	vap->va_blocksize = ip->i_mp->ntm_spc * ip->i_mp->ntm_bps;
	vap->va_type = vp->v_type;
	vap->va_filerev = 0;
	return (0);
}


/*
 * Last reference to an ntnode.  If necessary, write or delete it.
 */
int
ntfs_inactive(void *v)
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
#ifdef NTFS_DEBUG
	struct ntnode *ip = VTONT(vp);
#endif

	dprintf(("ntfs_inactive: vnode: %p, ntnode: %llu\n", vp,
	    (unsigned long long)ip->i_number));

	VOP_UNLOCK(vp);

	/* XXX since we don't support any filesystem changes
	 * right now, nothing more needs to be done
	 */
	return (0);
}

/*
 * Reclaim an fnode/ntnode so that it can be used for other purposes.
 */
int
ntfs_reclaim(void *v)
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct fnode *fp = VTOF(vp);
	struct ntnode *ip = FTONT(fp);
	const int attrlen = strlen(fp->f_attrname);
	int error;

	dprintf(("ntfs_reclaim: vnode: %p, ntnode: %llu\n", vp,
	    (unsigned long long)ip->i_number));

	if (prtactive && vp->v_usecount > 1)
		vprint("ntfs_reclaim: pushing active", vp);

	if ((error = ntfs_ntget(ip)) != 0)
		return (error);

	vcache_remove(vp->v_mount, fp->f_key, NTKEY_SIZE(attrlen));

	if (ip->i_devvp) {
		vrele(ip->i_devvp);
		ip->i_devvp = NULL;
	}
	genfs_node_destroy(vp);
	vp->v_data = NULL;

	/* Destroy fnode. */
	if (fp->f_key != &fp->f_smallkey)
		kmem_free(fp->f_key, NTKEY_SIZE(attrlen));
	if (fp->f_dirblbuf)
		free(fp->f_dirblbuf, M_NTFSDIR);
	kmem_free(fp, sizeof(*fp));
	ntfs_ntrele(ip);

	ntfs_ntput(ip);

	return (0);
}

static int
ntfs_print(void *v)
{
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct ntnode *ip = VTONT(ap->a_vp);

	printf("tag VT_NTFS, ino %llu, flag %#x, usecount %d, nlink %ld\n",
	    (unsigned long long)ip->i_number, ip->i_flag, ip->i_usecount,
	    ip->i_nlink);
	printf("       ");
	printf("\n");
	return (0);
}

/*
 * Calculate the logical to physical mapping if not done already,
 * then call the device strategy routine.
 */
int
ntfs_strategy(void *v)
{
	struct vop_strategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap = v;
	struct buf *bp = ap->a_bp;
	struct vnode *vp = ap->a_vp;
	struct fnode *fp = VTOF(vp);
	struct ntnode *ip = FTONT(fp);
	struct ntfsmount *ntmp = ip->i_mp;
	int error;

	dprintf(("ntfs_strategy: blkno: %d, lblkno: %d\n",
		(u_int32_t)bp->b_blkno,
		(u_int32_t)bp->b_lblkno));

	dprintf(("strategy: bcount: %u flags: 0x%x\n",
		(u_int32_t)bp->b_bcount,bp->b_flags));

	if (bp->b_flags & B_READ) {
		u_int32_t toread;

		if (ntfs_cntob(bp->b_blkno) >= fp->f_size) {
			clrbuf(bp);
			error = 0;
		} else {
			toread = MIN(bp->b_bcount,
				 fp->f_size - ntfs_cntob(bp->b_blkno));
			dprintf(("ntfs_strategy: toread: %d, fsize: %d\n",
				toread,(u_int32_t)fp->f_size));

			error = ntfs_readattr(ntmp, ip, fp->f_attrtype,
				fp->f_attrname, ntfs_cntob(bp->b_blkno),
				toread, bp->b_data, NULL);

			if (error) {
				printf("ntfs_strategy: ntfs_readattr failed\n");
				bp->b_error = error;
			}

			memset((char *)bp->b_data + toread, 0,
			    bp->b_bcount - toread);
		}
	} else {
		size_t tmp;
		u_int32_t towrite;

		if (ntfs_cntob(bp->b_blkno) + bp->b_bcount >= fp->f_size) {
			printf("ntfs_strategy: CAN'T EXTEND FILE\n");
			bp->b_error = error = EFBIG;
		} else {
			towrite = MIN(bp->b_bcount,
				fp->f_size - ntfs_cntob(bp->b_blkno));
			dprintf(("ntfs_strategy: towrite: %d, fsize: %d\n",
				towrite,(u_int32_t)fp->f_size));

			error = ntfs_writeattr_plain(ntmp, ip, fp->f_attrtype,
				fp->f_attrname, ntfs_cntob(bp->b_blkno),towrite,
				bp->b_data, &tmp, NULL);

			if (error) {
				printf("ntfs_strategy: ntfs_writeattr fail\n");
				bp->b_error = error;
			}
		}
	}
	biodone(bp);
	return (error);
}

static int
ntfs_write(void *v)
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct fnode *fp = VTOF(vp);
	struct ntnode *ip = FTONT(fp);
	struct uio *uio = ap->a_uio;
	struct ntfsmount *ntmp = ip->i_mp;
	u_int64_t towrite;
	size_t written;
	int error;

	dprintf(("ntfs_write: ino: %llu, off: %qd resid: %qd\n",
	    (unsigned long long)ip->i_number, (long long)uio->uio_offset,
	    (long long)uio->uio_resid));
	dprintf(("ntfs_write: filesize: %qu",(long long)fp->f_size));

	if (uio->uio_resid + uio->uio_offset > fp->f_size) {
		printf("ntfs_write: CAN'T WRITE BEYOND END OF FILE\n");
		return (EFBIG);
	}

	towrite = MIN(uio->uio_resid, fp->f_size - uio->uio_offset);

	dprintf((", towrite: %qu\n",(long long)towrite));

	error = ntfs_writeattr_plain(ntmp, ip, fp->f_attrtype,
		fp->f_attrname, uio->uio_offset, towrite, NULL, &written, uio);
#ifdef NTFS_DEBUG
	if (error)
		printf("ntfs_write: ntfs_writeattr failed: %d\n", error);
#endif

	return (error);
}

static int
ntfs_check_possible(struct vnode *vp, struct ntnode *ip, mode_t mode)
{

	/*
	 * Disallow write attempts on read-only file systems;
	 * unless the file is a socket, fifo, or a block or
	 * character device resident on the file system.
	 */
	if (mode & VWRITE) {
		switch ((int)vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
			break;
		}
	}

	return 0;
}

static int
ntfs_check_permitted(struct vnode *vp, struct ntnode *ip, mode_t mode,
    kauth_cred_t cred)
{
	mode_t file_mode;

	file_mode = ip->i_mp->ntm_mode | (S_IXUSR|S_IXGRP|S_IXOTH);

	return kauth_authorize_vnode(cred, KAUTH_ACCESS_ACTION(mode, vp->v_type,
	    file_mode), vp, NULL, genfs_can_access(vp->v_type, file_mode,
	    ip->i_mp->ntm_uid, ip->i_mp->ntm_gid, mode, cred));
}

int
ntfs_access(void *v)
{
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct ntnode *ip = VTONT(vp);
	int error;

	dprintf(("ntfs_access: %llu\n", (unsigned long long)ip->i_number));

	error = ntfs_check_possible(vp, ip, ap->a_mode);
	if (error)
		return error;

	error = ntfs_check_permitted(vp, ip, ap->a_mode, ap->a_cred);

	return error;
}

/*
 * Open called.
 *
 * Nothing to do.
 */
/* ARGSUSED */
static int
ntfs_open(void *v)
{
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		kauth_cred_t a_cred;
	} */ *ap __unused = v;
#ifdef NTFS_DEBUG
	struct vnode *vp = ap->a_vp;
	struct ntnode *ip = VTONT(vp);

	dprintf(("ntfs_open: %llu\n", (unsigned long long)ip->i_number));
#endif

	/*
	 * Files marked append-only must be opened for appending.
	 */

	return (0);
}

/*
 * Close called.
 *
 * Update the times on the inode.
 */
/* ARGSUSED */
static int
ntfs_close(void *v)
{
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		kauth_cred_t a_cred;
	} */ *ap __unused = v;
#ifdef NTFS_DEBUG
	struct vnode *vp = ap->a_vp;
	struct ntnode *ip = VTONT(vp);

	dprintf(("ntfs_close: %llu\n", (unsigned long long)ip->i_number));
#endif

	return (0);
}

int
ntfs_readdir(void *v)
{
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		kauth_cred_t a_cred;
		int *a_ncookies;
		u_int **cookies;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct fnode *fp = VTOF(vp);
	struct ntnode *ip = FTONT(fp);
	struct uio *uio = ap->a_uio;
	struct ntfsmount *ntmp = ip->i_mp;
	int i, error = 0;
	u_int32_t faked = 0, num;
	int ncookies = 0;
	struct dirent *cde;
	off_t off;

	dprintf(("ntfs_readdir %llu off: %qd resid: %qd\n",
	    (unsigned long long)ip->i_number, (long long)uio->uio_offset,
	    (long long)uio->uio_resid));

	off = uio->uio_offset;

	cde = malloc(sizeof(struct dirent), M_TEMP, M_WAITOK);

	/* Simulate . in every dir except ROOT */
	if (ip->i_number != NTFS_ROOTINO
	    && uio->uio_offset < sizeof(struct dirent)) {
		cde->d_fileno = ip->i_number;
		cde->d_reclen = sizeof(struct dirent);
		cde->d_type = DT_DIR;
		cde->d_namlen = 1;
		strncpy(cde->d_name, ".", 2);
		error = uiomove((void *)cde, sizeof(struct dirent), uio);
		if (error)
			goto out;

		ncookies++;
	}

	/* Simulate .. in every dir including ROOT */
	if (uio->uio_offset < 2 * sizeof(struct dirent)) {
		cde->d_fileno = NTFS_ROOTINO;	/* XXX */
		cde->d_reclen = sizeof(struct dirent);
		cde->d_type = DT_DIR;
		cde->d_namlen = 2;
		strncpy(cde->d_name, "..", 3);

		error = uiomove((void *) cde, sizeof(struct dirent), uio);
		if (error)
			goto out;

		ncookies++;
	}

	faked = (ip->i_number == NTFS_ROOTINO) ? 1 : 2;
	num = uio->uio_offset / sizeof(struct dirent) - faked;

	while (uio->uio_resid >= sizeof(struct dirent)) {
		struct attr_indexentry *iep;
		char *fname;
		size_t remains;
		int sz;

		error = ntfs_ntreaddir(ntmp, fp, num, &iep);
		if (error)
			goto out;

		if (NULL == iep)
			break;

		for(; !(iep->ie_flag & NTFS_IEFLAG_LAST) && (uio->uio_resid >= sizeof(struct dirent));
			iep = NTFS_NEXTREC(iep, struct attr_indexentry *))
		{
			if(!ntfs_isnamepermitted(ntmp,iep))
				continue;

			remains = sizeof(cde->d_name) - 1;
			fname = cde->d_name;
			for(i=0; i<iep->ie_fnamelen; i++) {
				sz = (*ntmp->ntm_wput)(fname, remains,
						iep->ie_fname[i]);
				fname += sz;
				remains -= sz;
			}
			*fname = '\0';
			dprintf(("ntfs_readdir: elem: %d, fname:[%s] type: %d, flag: %d, ",
				num, cde->d_name, iep->ie_fnametype,
				iep->ie_flag));
			cde->d_namlen = fname - (char *) cde->d_name;
			cde->d_fileno = iep->ie_number;
			cde->d_type = (iep->ie_fflag & NTFS_FFLAG_DIR) ? DT_DIR : DT_REG;
			cde->d_reclen = sizeof(struct dirent);
			dprintf(("%s\n", (cde->d_type == DT_DIR) ? "dir":"reg"));

			error = uiomove((void *)cde, sizeof(struct dirent), uio);
			if (error)
				goto out;

			ncookies++;
			num++;
		}
	}

	dprintf(("ntfs_readdir: %d entries (%d bytes) read\n",
		ncookies,(u_int)(uio->uio_offset - off)));
	dprintf(("ntfs_readdir: off: %qd resid: %qu\n",
		(long long)uio->uio_offset,(long long)uio->uio_resid));

	if (!error && ap->a_ncookies != NULL) {
		struct dirent* dpStart;
		struct dirent* dp;
		off_t *cookies;
		off_t *cookiep;

		dprintf(("ntfs_readdir: %d cookies\n",ncookies));
		dpStart = (struct dirent *)
		     ((char *)uio->uio_iov->iov_base -
			 (uio->uio_offset - off));
		cookies = malloc(ncookies * sizeof(off_t), M_TEMP, M_WAITOK);
		for (dp = dpStart, cookiep = cookies, i=0;
		     i < ncookies;
		     dp = (struct dirent *)((char *) dp + dp->d_reclen), i++) {
			off += dp->d_reclen;
			*cookiep++ = (u_int) off;
		}
		*ap->a_ncookies = ncookies;
		*ap->a_cookies = cookies;
	}
/*
	if (ap->a_eofflag)
	    *ap->a_eofflag = VTONT(ap->a_vp)->i_size <= uio->uio_offset;
*/
    out:
	free(cde, M_TEMP);
	return (error);
}

int
ntfs_lookup(void *v)
{
	struct vop_lookup_v2_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct ntnode *dip = VTONT(dvp);
	struct ntfsmount *ntmp = dip->i_mp;
	struct componentname *cnp = ap->a_cnp;
	kauth_cred_t cred = cnp->cn_cred;
	int error;

	dprintf(("ntfs_lookup: \"%.*s\" (%lld bytes) in %llu\n",
	    (int)cnp->cn_namelen, cnp->cn_nameptr, (long long)cnp->cn_namelen,
	    (unsigned long long)dip->i_number));

	error = VOP_ACCESS(dvp, VEXEC, cred);
	if(error)
		return (error);

	if ((cnp->cn_flags & ISLASTCN) &&
	    (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);

	/*
	 * We now have a segment name to search for, and a directory
	 * to search.
	 *
	 * Before tediously performing a linear scan of the directory,
	 * check the name cache to see if the directory/name pair
	 * we are looking for is known already.
	 */
	if (cache_lookup(ap->a_dvp, cnp->cn_nameptr, cnp->cn_namelen,
			 cnp->cn_nameiop, cnp->cn_flags, NULL, ap->a_vpp)) {
		return *ap->a_vpp == NULLVP ? ENOENT : 0;
	}

	if(cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
		dprintf(("ntfs_lookup: faking . directory in %llu\n",
		    (unsigned long long)dip->i_number));

		vref(dvp);
		*ap->a_vpp = dvp;
		error = 0;
	} else if (cnp->cn_flags & ISDOTDOT) {
		struct ntvattr *vap;

		dprintf(("ntfs_lookup: faking .. directory in %llu\n",
		    (unsigned long long)dip->i_number));

		error = ntfs_ntvattrget(ntmp, dip, NTFS_A_NAME, NULL, 0, &vap);
		if (error) {
			return (error);
		}

		dprintf(("ntfs_lookup: parentdir: %d\n",
			 vap->va_a_name->n_pnumber));
		error = ntfs_vgetex(ntmp->ntm_mountp,
				 vap->va_a_name->n_pnumber,
				 NTFS_A_DATA, "", 0, ap->a_vpp);
		ntfs_ntvattrrele(vap);
		if (error) {
			return (error);
		}
	} else {
		error = ntfs_ntlookupfile(ntmp, dvp, cnp, ap->a_vpp);
		if (error) {
			dprintf(("ntfs_ntlookupfile: returned %d\n", error));
			return (error);
		}

		dprintf(("ntfs_lookup: found ino: %llu\n",
		    (unsigned long long)VTONT(*ap->a_vpp)->i_number));
	}

	cache_enter(dvp, *ap->a_vpp, cnp->cn_nameptr, cnp->cn_namelen,
		    cnp->cn_flags);

	return error;
}

/*
 * Flush the blocks of a file to disk.
 *
 * This function is worthless for vnodes that represent directories. Maybe we
 * could just do a sync if they try an fsync on a directory file.
 */
static int
ntfs_fsync(void *v)
{
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		kauth_cred_t a_cred;
		int a_flags;
		off_t offlo;
		off_t offhi;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	if (ap->a_flags & FSYNC_CACHE) {
		return EOPNOTSUPP;
	}

	return vflushbuf(vp, ap->a_flags);
}

/*
 * Return POSIX pathconf information applicable to NTFS filesystem
 */
static int
ntfs_pathconf(void *v)
{
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = 1;
		return (0);
	case _PC_NAME_MAX:
		*ap->a_retval = ap->a_vp->v_mount->mnt_stat.f_namemax;
		return (0);
	case _PC_PATH_MAX:
		*ap->a_retval = PATH_MAX;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_NO_TRUNC:
		*ap->a_retval = 0;
		return (0);
	case _PC_SYNC_IO:
		*ap->a_retval = 1;
		return (0);
	case _PC_FILESIZEBITS:
		*ap->a_retval = 64;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Global vfs data structures
 */
vop_t **ntfs_vnodeop_p;

const struct vnodeopv_entry_desc ntfs_vnodeop_entries[] = {
	{ &vop_default_desc, (vop_t *) ntfs_bypass },
	{ &vop_lookup_desc, (vop_t *) ntfs_lookup },	/* lookup */
	{ &vop_create_desc, genfs_eopnotsupp },		/* create */
	{ &vop_mknod_desc, genfs_eopnotsupp },		/* mknod */
	{ &vop_open_desc, (vop_t *) ntfs_open },	/* open */
	{ &vop_close_desc,(vop_t *)  ntfs_close },	/* close */
	{ &vop_access_desc, (vop_t *) ntfs_access },	/* access */
	{ &vop_getattr_desc, (vop_t *) ntfs_getattr },	/* getattr */
	{ &vop_setattr_desc, genfs_eopnotsupp },	/* setattr */
	{ &vop_read_desc, (vop_t *) ntfs_read },	/* read */
	{ &vop_write_desc, (vop_t *) ntfs_write },	/* write */
	{ &vop_fallocate_desc, genfs_eopnotsupp },	/* fallocate */
	{ &vop_fdiscard_desc, genfs_eopnotsupp },	/* fdiscard */
	{ &vop_fcntl_desc, genfs_fcntl },		/* fcntl */
	{ &vop_ioctl_desc, genfs_enoioctl },		/* ioctl */
	{ &vop_poll_desc, genfs_poll },			/* poll */
	{ &vop_kqfilter_desc, genfs_kqfilter },		/* kqfilter */
	{ &vop_revoke_desc, genfs_revoke },		/* revoke */
	{ &vop_mmap_desc, genfs_mmap },			/* mmap */
	{ &vop_fsync_desc, (vop_t *) ntfs_fsync },	/* fsync */
	{ &vop_seek_desc, genfs_seek },			/* seek */
	{ &vop_remove_desc, genfs_eopnotsupp },		/* remove */
	{ &vop_link_desc, genfs_eopnotsupp },		/* link */
	{ &vop_rename_desc, genfs_eopnotsupp },		/* rename */
	{ &vop_mkdir_desc, genfs_eopnotsupp },		/* mkdir */
	{ &vop_rmdir_desc, genfs_eopnotsupp },		/* rmdir */
	{ &vop_symlink_desc, genfs_eopnotsupp },	/* symlink */
	{ &vop_readdir_desc, (vop_t *) ntfs_readdir },	/* readdir */
	{ &vop_readlink_desc, genfs_eopnotsupp },	/* readlink */
	{ &vop_abortop_desc, genfs_abortop },		/* abortop */
	{ &vop_inactive_desc, (vop_t *) ntfs_inactive },	/* inactive */
	{ &vop_reclaim_desc, (vop_t *) ntfs_reclaim },	/* reclaim */
	{ &vop_lock_desc, genfs_lock },			/* lock */
	{ &vop_unlock_desc, genfs_unlock },		/* unlock */
	{ &vop_bmap_desc, (vop_t *) ntfs_bmap },	/* bmap */
	{ &vop_strategy_desc, (vop_t *) ntfs_strategy },	/* strategy */
	{ &vop_print_desc, (vop_t *) ntfs_print },	/* print */
	{ &vop_islocked_desc, genfs_islocked },		/* islocked */
	{ &vop_pathconf_desc, ntfs_pathconf },		/* pathconf */
	{ &vop_advlock_desc, genfs_nullop },		/* advlock */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ &vop_getpages_desc, genfs_compat_getpages },	/* getpages */
	{ &vop_putpages_desc, genfs_putpages },		/* putpages */
	{ NULL, NULL }
};
const struct vnodeopv_desc ntfs_vnodeop_opv_desc =
	{ &ntfs_vnodeop_p, ntfs_vnodeop_entries };

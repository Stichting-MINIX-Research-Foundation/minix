/*	$NetBSD: advnops.c,v 1.47 2015/04/20 23:03:07 riastradh Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: advnops.c,v 1.47 2015/04/20 23:03:07 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/dirent.h>
#include <sys/inttypes.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <sys/proc.h>
#include <sys/kauth.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>
#include <fs/adosfs/adosfs.h>

extern struct vnodeops adosfs_vnodeops;

#define	adosfs_open	genfs_nullop
int	adosfs_getattr(void *);
int	adosfs_read(void *);
int	adosfs_write(void *);
#define	adosfs_fcntl	genfs_fcntl
#define	adosfs_ioctl	genfs_enoioctl
#define	adosfs_poll	genfs_poll
int	adosfs_strategy(void *);
int	adosfs_link(void *);
int	adosfs_symlink(void *);
#define	adosfs_abortop	genfs_abortop
int	adosfs_bmap(void *);
int	adosfs_print(void *);
int	adosfs_readdir(void *);
int	adosfs_access(void *);
int	adosfs_readlink(void *);
int	adosfs_inactive(void *);
int	adosfs_reclaim(void *);
int	adosfs_pathconf(void *);

#define adosfs_close 	genfs_nullop
#define adosfs_fsync 	genfs_nullop
#define adosfs_seek 	genfs_seek

#define adosfs_advlock 	genfs_einval
#define adosfs_bwrite 	genfs_eopnotsupp
#define adosfs_create 	genfs_eopnotsupp
#define adosfs_mkdir 	genfs_eopnotsupp
#define adosfs_mknod 	genfs_eopnotsupp
#define adosfs_revoke	genfs_revoke
#define adosfs_mmap 	genfs_mmap
#define adosfs_remove 	genfs_eopnotsupp
#define adosfs_rename 	genfs_eopnotsupp
#define adosfs_rmdir 	genfs_eopnotsupp
#define adosfs_setattr 	genfs_eopnotsupp

const struct vnodeopv_entry_desc adosfs_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, adosfs_lookup },		/* lookup */
	{ &vop_create_desc, adosfs_create },		/* create */
	{ &vop_mknod_desc, adosfs_mknod },		/* mknod */
	{ &vop_open_desc, adosfs_open },		/* open */
	{ &vop_close_desc, adosfs_close },		/* close */
	{ &vop_access_desc, adosfs_access },		/* access */
	{ &vop_getattr_desc, adosfs_getattr },		/* getattr */
	{ &vop_setattr_desc, adosfs_setattr },		/* setattr */
	{ &vop_read_desc, adosfs_read },		/* read */
	{ &vop_write_desc, adosfs_write },		/* write */
	{ &vop_fallocate_desc, genfs_eopnotsupp },	/* fallocate */
	{ &vop_fdiscard_desc, genfs_eopnotsupp },	/* fdiscard */
	{ &vop_fcntl_desc, adosfs_fcntl },		/* fcntl */
	{ &vop_ioctl_desc, adosfs_ioctl },		/* ioctl */
	{ &vop_poll_desc, adosfs_poll },		/* poll */
	{ &vop_kqfilter_desc, genfs_kqfilter },		/* kqfilter */
	{ &vop_revoke_desc, adosfs_revoke },		/* revoke */
	{ &vop_mmap_desc, adosfs_mmap },		/* mmap */
	{ &vop_fsync_desc, adosfs_fsync },		/* fsync */
	{ &vop_seek_desc, adosfs_seek },		/* seek */
	{ &vop_remove_desc, adosfs_remove },		/* remove */
	{ &vop_link_desc, adosfs_link },		/* link */
	{ &vop_rename_desc, adosfs_rename },		/* rename */
	{ &vop_mkdir_desc, adosfs_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, adosfs_rmdir },		/* rmdir */
	{ &vop_symlink_desc, adosfs_symlink },		/* symlink */
	{ &vop_readdir_desc, adosfs_readdir },		/* readdir */
	{ &vop_readlink_desc, adosfs_readlink },	/* readlink */
	{ &vop_abortop_desc, adosfs_abortop },		/* abortop */
	{ &vop_inactive_desc, adosfs_inactive },	/* inactive */
	{ &vop_reclaim_desc, adosfs_reclaim },		/* reclaim */
	{ &vop_lock_desc, genfs_lock },			/* lock */
	{ &vop_unlock_desc, genfs_unlock },		/* unlock */
	{ &vop_bmap_desc, adosfs_bmap },		/* bmap */
	{ &vop_strategy_desc, adosfs_strategy },	/* strategy */
	{ &vop_print_desc, adosfs_print },		/* print */
	{ &vop_islocked_desc, genfs_islocked },		/* islocked */
	{ &vop_pathconf_desc, adosfs_pathconf },	/* pathconf */
	{ &vop_advlock_desc, adosfs_advlock },		/* advlock */
	{ &vop_bwrite_desc, adosfs_bwrite },		/* bwrite */
	{ &vop_getpages_desc, genfs_getpages },		/* getpages */
	{ &vop_putpages_desc, genfs_putpages },		/* putpages */
	{ NULL, NULL }
};

const struct vnodeopv_desc adosfs_vnodeop_opv_desc =
	{ &adosfs_vnodeop_p, adosfs_vnodeop_entries };

int
adosfs_getattr(void *v)
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
	} */ *sp = v;
	struct vattr *vap;
	struct adosfsmount *amp;
	struct anode *ap;
	u_long fblks;

#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif
	vap = sp->a_vap;
	ap = VTOA(sp->a_vp);
	amp = ap->amp;
	vattr_null(vap);
	vap->va_uid = ap->uid;
	vap->va_gid = ap->gid;
	vap->va_fsid = sp->a_vp->v_mount->mnt_stat.f_fsidx.__fsid_val[0];
	vap->va_atime.tv_sec = vap->va_mtime.tv_sec = vap->va_ctime.tv_sec =
		ap->mtime.days * 24 * 60 * 60 + ap->mtime.mins * 60 +
		ap->mtime.ticks / 50 + (8 * 365 + 2) * 24 * 60 * 60;
	vap->va_atime.tv_nsec = vap->va_mtime.tv_nsec = vap->va_ctime.tv_nsec = 0;
	vap->va_gen = 0;
	vap->va_flags = 0;
	vap->va_rdev = NODEV;
	vap->va_fileid = ap->block;
	vap->va_type = sp->a_vp->v_type;
	vap->va_mode = adunixprot(ap->adprot) & amp->mask;
	if (sp->a_vp->v_type == VDIR) {
		vap->va_nlink = 1;	/* XXX bogus, oh well */
		vap->va_bytes = amp->bsize;
		vap->va_size = amp->bsize;
	} else {
		/*
		 * XXX actually we can track this if we were to walk the list
		 * of links if it exists.
		 * XXX for now, just set nlink to 2 if this is a hard link
		 * to a file, or a file with a hard link.
		 */
		vap->va_nlink = 1 + (ap->linkto != 0);
		/*
		 * round up to nearest blocks add number of file list
		 * blocks needed and mutiply by number of bytes per block.
		 */
		fblks = howmany(ap->fsize, amp->dbsize);
		fblks += howmany(fblks, ANODENDATBLKENT(ap));
		vap->va_bytes = fblks * amp->dbsize;
		vap->va_size = ap->fsize;

		vap->va_blocksize = amp->dbsize;
	}
#ifdef ADOSFS_DIAGNOSTIC
	printf(" 0)");
#endif
	return(0);
}
/*
 * are things locked??? they need to be to avoid this being
 * deleted or changed (data block pointer blocks moving about.)
 */
int
adosfs_read(void *v)
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *sp = v;
	struct vnode *vp = sp->a_vp;
	struct adosfsmount *amp;
	struct anode *ap;
	struct uio *uio;
	struct buf *bp;
	daddr_t lbn;
	int size, diff, error;
	long n, on;

#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif
	error = 0;
	uio = sp->a_uio;
	ap = VTOA(sp->a_vp);
	amp = ap->amp;
	/*
	 * Return EOF for character devices, EIO for others
	 */
	if (sp->a_vp->v_type != VREG) {
		error = EIO;
		goto reterr;
	}
	if (uio->uio_resid == 0)
		goto reterr;
	if (uio->uio_offset < 0) {
		error = EINVAL;
		goto reterr;
	}

	/*
	 * to expensive to let general algorithm figure out that
	 * we are beyond the file.  Do it now.
	 */
	if (uio->uio_offset >= ap->fsize)
		goto reterr;

	/*
	 * taken from ufs_read()
	 */

	if (vp->v_type == VREG && IS_FFS(amp)) {
		const int advice = IO_ADV_DECODE(sp->a_ioflag);
		error = 0;

		while (uio->uio_resid > 0) {
			vsize_t bytelen = MIN(ap->fsize - uio->uio_offset,
					      uio->uio_resid);

			if (bytelen == 0) {
				break;
			}
			error = ubc_uiomove(&vp->v_uobj, uio, bytelen, advice,
			    UBC_READ | UBC_PARTIALOK | UBC_UNMAP_FLAG(vp));
			if (error) {
				break;
			}
		}
		goto out;
	}

	do {
		size = amp->dbsize;
		lbn = uio->uio_offset / size;
		on = uio->uio_offset % size;
		n = MIN(size - on, uio->uio_resid);
		diff = ap->fsize - uio->uio_offset;
		/*
		 * check for EOF
		 */
		if (diff <= 0)
			return(0);
		if (diff < n)
			n = diff;
		/*
		 * read ahead could possibly be worth something
		 * but not much as ados makes little attempt to
		 * make things contigous
		 */
		error = bread(sp->a_vp, lbn, amp->bsize, 0, &bp);
		if (error) {
			goto reterr;
		}
		if (!IS_FFS(amp)) {
			if (bp->b_resid > 0)
				error = EIO; /* OFS needs the complete block */
			else if (adoswordn(bp, 0) != BPT_DATA) {
#ifdef DIAGNOSTIC
				printf("adosfs: bad primary type blk %" PRId64 "\n",
				    bp->b_blkno / (amp->bsize / DEV_BSIZE));
#endif
				error = EINVAL;
			} else if (adoscksum(bp, ap->nwords)) {
#ifdef DIAGNOSTIC
				printf("adosfs: blk %" PRId64 " failed cksum.\n",
				    bp->b_blkno / (amp->bsize / DEV_BSIZE));
#endif
				error = EINVAL;
			}
		}

		if (error) {
			brelse(bp, 0);
			goto reterr;
		}
#ifdef ADOSFS_DIAGNOSTIC
		printf(" %" PRId64 "+%ld-%" PRId64 "+%ld", lbn, on, lbn, n);
#endif
		n = MIN(n, size - bp->b_resid);
		error = uiomove((char *)bp->b_data + on +
				amp->bsize - amp->dbsize, (int)n, uio);
		brelse(bp, 0);
	} while (error == 0 && uio->uio_resid > 0 && n != 0);

out:
reterr:
#ifdef ADOSFS_DIAGNOSTIC
	printf(" %d)", error);
#endif
	return(error);
}

int
adosfs_write(void *v)
{
#ifdef ADOSFS_DIAGNOSTIC
#if 0
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *sp = v;
	advopprint(sp);
#endif
	printf(" EOPNOTSUPP)");
#endif
	return(EOPNOTSUPP);
}

/*
 * Just call the device strategy routine
 */
int
adosfs_strategy(void *v)
{
	struct vop_strategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *sp = v;
	struct buf *bp;
	struct anode *ap;
	struct vnode *vp;
	int error;

#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif
	bp = sp->a_bp;
	if (bp->b_vp == NULL) {
		bp->b_error = EIO;
		biodone(bp);
		error = EIO;
		goto reterr;
	}
	vp = sp->a_vp;
	ap = VTOA(vp);
	if (bp->b_blkno == bp->b_lblkno) {
		error = VOP_BMAP(vp, bp->b_lblkno, NULL, &bp->b_blkno, NULL);
		if (error) {
			bp->b_flags = error;
			biodone(bp);
			goto reterr;
		}
	}
	if ((long)bp->b_blkno == -1) {
		biodone(bp);
		error = 0;
		goto reterr;
	}
	vp = ap->amp->devvp;
	error = VOP_STRATEGY(vp, bp);
reterr:
#ifdef ADOSFS_DIAGNOSTIC
	printf(" %d)", error);
#endif
	return(error);
}

int
adosfs_link(void *v)
{
	struct vop_link_v2_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;

	VOP_ABORTOP(ap->a_dvp, ap->a_cnp);
	return (EROFS);
}

int
adosfs_symlink(void *v)
{
	struct vop_symlink_v3_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap = v;

	VOP_ABORTOP(ap->a_dvp, ap->a_cnp);
	return (EROFS);
}

/*
 * Wait until the vnode has finished changing state.
 */
int
adosfs_bmap(void *v)
{
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *sp = v;
	struct anode *ap;
	struct buf *flbp;
	long nb, flblk, flblkoff, fcnt;
	daddr_t *bnp;
	daddr_t bn;
	int error;

#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif
	ap = VTOA(sp->a_vp);
	bn = sp->a_bn;
	bnp = sp->a_bnp;
	if (sp->a_runp) {
		*sp->a_runp = 0;
	}
	error = 0;

	if (sp->a_vpp != NULL)
		*sp->a_vpp = ap->amp->devvp;
	if (bnp == NULL)
		goto reterr;
	if (bn < 0) {
		error = EFBIG;
		goto reterr;
	}
	if (sp->a_vp->v_type != VREG) {
		error = EINVAL;
		goto reterr;
	}

	/*
	 * walk the chain of file list blocks until we find
	 * the one that will yield the block pointer we need.
	 */
	if (ap->type == AFILE)
		nb = ap->block;			/* pointer to ourself */
	else if (ap->type == ALFILE)
		nb = ap->linkto;		/* pointer to real file */
	else {
		error = EINVAL;
		goto reterr;
	}

	flblk = bn / ANODENDATBLKENT(ap);
	flbp = NULL;

	/*
	 * check last indirect block cache
	 */
	if (flblk < ap->lastlindblk)
		fcnt = 0;
	else {
		flblk -= ap->lastlindblk;
		fcnt = ap->lastlindblk;
		nb = ap->lastindblk;
	}
	while (flblk >= 0) {
		if (flbp)
			brelse(flbp, 0);
		if (nb == 0) {
#ifdef DIAGNOSTIC
			printf("adosfs: bad file list chain.\n");
#endif
			error = EINVAL;
			goto reterr;
		}
		error = bread(ap->amp->devvp, nb * ap->amp->bsize / DEV_BSIZE,
			      ap->amp->bsize, 0, &flbp);
		if (error) {
			goto reterr;
		}
		if (adoscksum(flbp, ap->nwords)) {
#ifdef DIAGNOSTIC
			printf("adosfs: blk %ld failed cksum.\n", nb);
#endif
			brelse(flbp, 0);
			error = EINVAL;
			goto reterr;
		}
		/*
		 * update last indirect block cache
		 */
		ap->lastlindblk = fcnt++;
		ap->lastindblk = nb;

		nb = adoswordn(flbp, ap->nwords - 2);
		flblk--;
	}
	/*
	 * calculate offset of block number in table.  The table starts
	 * at nwords - 51 and goes to offset 6 or less if indicated by the
	 * valid table entries stored at offset ADBI_NBLKTABENT.
	 */
	flblkoff = bn % ANODENDATBLKENT(ap);
	if (flblkoff < adoswordn(flbp, 2 /* ADBI_NBLKTABENT */)) {
		flblkoff = (ap->nwords - 51) - flblkoff;
		*bnp = adoswordn(flbp, flblkoff) * ap->amp->bsize / DEV_BSIZE;
	} else {
#ifdef DIAGNOSTIC
		printf("flblk offset %ld too large in lblk %ld blk %" PRId64 "\n",
		    flblkoff, (long)bn, flbp->b_blkno);
#endif
		error = EINVAL;
	}
	brelse(flbp, 0);
reterr:
#ifdef ADOSFS_DIAGNOSTIC
	if (error == 0 && bnp)
		printf(" %lld => %lld", (long long)bn, (long long)*bnp);
	printf(" %d)\n", error);
#endif
	return(error);
}

/*
 * Print out the contents of a adosfs vnode.
 */
/* ARGSUSED */
int
adosfs_print(void *v)
{
#if 0
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *sp = v;
#endif
	return(0);
}

int
adosfs_readdir(void *v)
{
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		kauth_cred_t a_cred;
		int *a_eofflag;
		off_t **a_cookies;
		int *a_ncookies;
	} */ *sp = v;
	int error, first, useri, chainc, hashi, scanned;
	u_long nextbn;
	struct dirent ad, *adp;
	struct anode *pap, *ap;
	struct vnode *vp;
	struct uio *uio = sp->a_uio;
	off_t uoff = uio->uio_offset;
	off_t *cookies = NULL;
	int ncookies = 0;

#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif

	if (sp->a_vp->v_type != VDIR) {
		error = ENOTDIR;
		goto reterr;
	}

	if (uoff < 0) {
		error = EINVAL;
		goto reterr;
	}

	pap = VTOA(sp->a_vp);
	adp = &ad;
	error = nextbn = hashi = chainc = scanned = 0;
	first = useri = uoff / sizeof ad;

	/*
	 * If offset requested is not on a slot boundary
	 */
	if (uoff % sizeof ad) {
		error = EINVAL;
		goto reterr;
	}

	for (;;) {
		if (hashi == pap->ntabent) {
			*sp->a_eofflag = 1;
			break;
		}
		if (pap->tab[hashi] == 0) {
			hashi++;
			continue;
		}
		if (nextbn == 0)
			nextbn = pap->tab[hashi];

		/*
		 * First determine if we can skip this chain
		 */
		if (chainc == 0) {
			int skip;

			skip = useri - scanned;
			if (pap->tabi[hashi] > 0 && pap->tabi[hashi] <= skip) {
				scanned += pap->tabi[hashi];
				hashi++;
				nextbn = 0;
				continue;
			}
		}

		/*
		 * Now [continue to] walk the chain
		 */
		ap = NULL;
		do {
			error = VFS_VGET(pap->amp->mp, (ino_t)nextbn, &vp);
			if (error)
				goto reterr;
			ap = VTOA(vp);
			scanned++;
			chainc++;
			nextbn = ap->hashf;

			/*
			 * check for end of chain.
			 */
			if (nextbn == 0) {
				pap->tabi[hashi] = chainc;
				hashi++;
				chainc = 0;
			} else if (pap->tabi[hashi] <= 0 &&
			    -chainc < pap->tabi[hashi])
				pap->tabi[hashi] = -chainc;

			if (useri >= scanned) {
				vput(vp);
				ap = NULL;
			}
		} while (ap == NULL && nextbn != 0);

		/*
		 * We left the loop but without a result so do main over.
		 */
		if (ap == NULL)
			continue;
		/*
		 * Fill in dirent record
		 */
		memset(adp, 0, sizeof *adp);
		adp->d_fileno = ap->block;
		/*
		 * This deserves a function in kern/vfs_subr.c
		 */
		switch (ATOV(ap)->v_type) {
		case VREG:
			adp->d_type = DT_REG;
			break;
		case VDIR:
			adp->d_type = DT_DIR;
			break;
		case VLNK:
			adp->d_type = DT_LNK;
			break;
		default:
			adp->d_type = DT_UNKNOWN;
			break;
		}
		adp->d_namlen = strlen(ap->name);
		memcpy(adp->d_name, ap->name, adp->d_namlen);
		adp->d_reclen = _DIRENT_SIZE(adp);
		vput(vp);

		if (adp->d_reclen > uio->uio_resid) {
			if (useri == first)	/* no room for even one entry */
				error = EINVAL;
			break;
		}
		error = uiomove(adp, adp->d_reclen, uio);
		if (error)
			break;
		useri++;
	}
	ncookies = useri - first;
	uio->uio_offset = uoff + ncookies * sizeof ad;
reterr:
#ifdef ADOSFS_DIAGNOSTIC
	printf(" %d)", error);
#endif
	if (sp->a_ncookies != NULL) {
		*sp->a_ncookies = ncookies;
		if (!error) {
			*sp->a_cookies = cookies =
			   malloc(ncookies * sizeof *cookies, M_TEMP, M_WAITOK);

			while (ncookies--) {
				uoff += sizeof ad;
				*cookies++ = uoff;
			}
		} else
			*sp->a_cookies = NULL;
	}

	return(error);
}

static int
adosfs_check_possible(struct vnode *vp, struct anode *ap, mode_t mode)
{

	/*
	 * Disallow write attempts unless the file is a socket,
	 * fifo, or a block or character device resident on the
	 * file system.
	 */
	if (mode & VWRITE) {
		switch (vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			return (EROFS);
		default:
			break;
		}
	}

	return 0;
}

static int
adosfs_check_permitted(struct vnode *vp, struct anode *ap, mode_t mode,
    kauth_cred_t cred)
{
	mode_t file_mode = adunixprot(ap->adprot) & ap->amp->mask;

	return kauth_authorize_vnode(cred, KAUTH_ACCESS_ACTION(mode,
	    vp->v_type, file_mode), vp, NULL, genfs_can_access(vp->v_type,
	    file_mode, ap->uid, ap->gid, mode, cred));
}

int
adosfs_access(void *v)
{
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		kauth_cred_t a_cred;
	} */ *sp = v;
	struct anode *ap;
	struct vnode *vp = sp->a_vp;
	int error;

#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif

	ap = VTOA(vp);
#ifdef DIAGNOSTIC
	if (!VOP_ISLOCKED(vp)) {
		vprint("adosfs_access: not locked", sp->a_vp);
		panic("adosfs_access: not locked");
	}
#endif

	error = adosfs_check_possible(vp, ap, sp->a_mode);
	if (error)
		return error;

	error = adosfs_check_permitted(vp, ap, sp->a_mode, sp->a_cred);

#ifdef ADOSFS_DIAGNOSTIC
	printf(" %d)", error);
#endif
	return(error);
}

int
adosfs_readlink(void *v)
{
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		kauth_cred_t a_cred;
	} */ *sp = v;
	struct anode *ap;
	int error;

#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif
	ap = VTOA(sp->a_vp);
	error = uiomove(ap->slinkto, strlen(ap->slinkto), sp->a_uio);
#ifdef ADOSFS_DIAGNOSTIC
	printf(" %d)", error);
#endif
	return (error);
}

/*ARGSUSED*/
int
adosfs_inactive(void *v)
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		bool *a_recycle;
	} */ *sp = v;
	struct vnode *vp = sp->a_vp;
#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif
	VOP_UNLOCK(vp);
	/* XXX this needs to check if file was deleted */
	*sp->a_recycle = true;

#ifdef ADOSFS_DIAGNOSTIC
	printf(" 0)");
#endif
	return(0);
}

/*
 * the kernel wants its vnode back.
 * no lock needed we are being called from vclean()
 */
int
adosfs_reclaim(void *v)
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *sp = v;
	struct vnode *vp;
	struct anode *ap;

#ifdef ADOSFS_DIAGNOSTIC
	printf("(reclaim 0)");
#endif
	vp = sp->a_vp;
	ap = VTOA(vp);
	vcache_remove(vp->v_mount, &ap->block, sizeof(ap->block));
	if (vp->v_type == VDIR && ap->tab)
		free(ap->tab, M_ANODE);
	else if (vp->v_type == VLNK && ap->slinkto)
		free(ap->slinkto, M_ANODE);
	genfs_node_destroy(vp);
	pool_put(&adosfs_node_pool, ap);
	vp->v_data = NULL;
	return(0);
}

/*
 * POSIX pathconf info, grabbed from kern/u fs, probably need to
 * investigate exactly what each return type means as they are probably
 * not valid currently
 */
int
adosfs_pathconf(void *v)
{
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = LINK_MAX;
		return (0);
	case _PC_NAME_MAX:
		*ap->a_retval = ap->a_vp->v_mount->mnt_stat.f_namemax;
		return (0);
	case _PC_PATH_MAX:
		*ap->a_retval = PATH_MAX;
		return (0);
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_VDISABLE:
		*ap->a_retval = _POSIX_VDISABLE;
		return (0);
	case _PC_SYNC_IO:
		*ap->a_retval = 1;
		return (0);
	case _PC_FILESIZEBITS:
		*ap->a_retval = 32;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

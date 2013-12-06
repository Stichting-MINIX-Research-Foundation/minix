/*	$NetBSD: subr.c,v 1.27 2011/02/17 17:55:36 pooka Exp $	*/

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
__RCSID("$NetBSD: subr.c,v 1.27 2011/02/17 17:55:36 pooka Exp $");
#endif /* !lint */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/dirent.h>

#include <assert.h>
#include <errno.h>
#include <puffs.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "puffs_priv.h"

int
puffs_gendotdent(struct dirent **dent, ino_t id, int dotdot, size_t *reslen)
{
	const char *name;

	assert(dotdot == 0 || dotdot == 1);
	name = dotdot == 0 ? "." : "..";

	return puffs_nextdent(dent, name, id, DT_DIR, reslen);
}

int
puffs_nextdent(struct dirent **dent, const char *name, ino_t id, uint8_t dtype,
	size_t *reslen)
{
	struct dirent *d = *dent;

	/* check if we have enough room for our dent-aligned dirent */
	if (_DIRENT_RECLEN(d, strlen(name)) > *reslen)
		return 0;

	d->d_fileno = id;
	d->d_type = dtype;
	d->d_namlen = (uint16_t)strlen(name);
	(void)memcpy(&d->d_name, name, (size_t)d->d_namlen);
	d->d_name[d->d_namlen] = '\0';
	d->d_reclen = (uint16_t)_DIRENT_SIZE(d);

	*reslen -= d->d_reclen;
	*dent = _DIRENT_NEXT(d);

	return 1;
}

/*ARGSUSED*/
int
puffs_fsnop_unmount(struct puffs_usermount *dontuse1, int dontuse2)
{

	/* would you like to see puffs rule again, my friend? */
	return 0;
}

/*ARGSUSED*/
int
puffs_fsnop_sync(struct puffs_usermount *dontuse1, int dontuse2,
	const struct puffs_cred *dontuse3)
{

	return 0;
}

/*ARGSUSED*/
int
puffs_fsnop_statvfs(struct puffs_usermount *dontuse1, struct statvfs *sbp)
{

	sbp->f_bsize = sbp->f_frsize = sbp->f_iosize = DEV_BSIZE;

	sbp->f_bfree=sbp->f_bavail=sbp->f_bresvd=sbp->f_blocks = (fsblkcnt_t)0;
	sbp->f_ffree=sbp->f_favail=sbp->f_fresvd=sbp->f_files = (fsfilcnt_t)0;

	sbp->f_namemax = MAXNAMLEN;

	return 0;
}

/*ARGSUSED3*/
int
puffs_genfs_node_getattr(struct puffs_usermount *pu, puffs_cookie_t opc,
	struct vattr *va, const struct puffs_cred *pcr)
{
	struct puffs_node *pn = PU_CMAP(pu, opc);

	memcpy(va, &pn->pn_va, sizeof(struct vattr));
	return 0;
}

/*
 * Just put the node, don't do anything else.  Don't use this if
 * your fs needs more cleanup.
 */
/*ARGSUSED2*/
int
puffs_genfs_node_reclaim(struct puffs_usermount *pu, puffs_cookie_t opc)
{

	puffs_pn_put(PU_CMAP(pu, opc));

	return 0;
}

/*
 * Just a wrapper to make calling the above nicer without having to pass
 * NULLs from application code
 */
void
puffs_zerostatvfs(struct statvfs *sbp)
{

	puffs_fsnop_statvfs(NULL, sbp);
}

/*
 * Set vattr values for those applicable (i.e. not PUFFS_VNOVAL).
 */
void
puffs_setvattr(struct vattr *vap, const struct vattr *sva)
{

#define SETIFVAL(a, t) if (sva->a != (t)PUFFS_VNOVAL) vap->a = sva->a
	if (sva->va_type != VNON)
		vap->va_type = sva->va_type;
	SETIFVAL(va_mode, mode_t);
	SETIFVAL(va_nlink, nlink_t);
	SETIFVAL(va_uid, uid_t);
	SETIFVAL(va_gid, gid_t);
	SETIFVAL(va_fsid, dev_t);
	SETIFVAL(va_size, u_quad_t);
	SETIFVAL(va_fileid, ino_t);
	SETIFVAL(va_blocksize, long);
	SETIFVAL(va_atime.tv_sec, time_t);
	SETIFVAL(va_ctime.tv_sec, time_t);
	SETIFVAL(va_mtime.tv_sec, time_t);
	SETIFVAL(va_birthtime.tv_sec, time_t);
	SETIFVAL(va_atime.tv_nsec, long);
	SETIFVAL(va_ctime.tv_nsec, long);
	SETIFVAL(va_mtime.tv_nsec, long);
	SETIFVAL(va_birthtime.tv_nsec, long);
	SETIFVAL(va_gen, u_long);
	SETIFVAL(va_flags, u_long);
	SETIFVAL(va_rdev, dev_t);
	SETIFVAL(va_bytes, u_quad_t);
#undef SETIFVAL
	/* ignore va->va_vaflags */
}

void
puffs_vattr_null(struct vattr *vap)
{

	vap->va_type = VNON;

	/*
	 * Assign individually so that it is safe even if size and
	 * sign of each member are varied.
	 */
	vap->va_mode = (mode_t)PUFFS_VNOVAL;
	vap->va_nlink = (nlink_t)PUFFS_VNOVAL;
	vap->va_uid = (uid_t)PUFFS_VNOVAL;
	vap->va_gid = (gid_t)PUFFS_VNOVAL;
	vap->va_fsid = (dev_t)PUFFS_VNOVAL;
	vap->va_fileid = (ino_t)PUFFS_VNOVAL;
	vap->va_size = (u_quad_t)PUFFS_VNOVAL;
	vap->va_blocksize = sysconf(_SC_PAGESIZE);
	    vap->va_atime.tv_sec =
	    vap->va_mtime.tv_sec =
	    vap->va_ctime.tv_sec =
	vap->va_birthtime.tv_sec = PUFFS_VNOVAL;
	    vap->va_atime.tv_nsec =
	    vap->va_mtime.tv_nsec =
	    vap->va_ctime.tv_nsec =
	vap->va_birthtime.tv_nsec = PUFFS_VNOVAL;
	vap->va_rdev = (dev_t)PUFFS_VNOVAL;
	vap->va_bytes = (u_quad_t)PUFFS_VNOVAL;

	vap->va_flags = 0;
	vap->va_gen = 0;
	vap->va_vaflags = 0;
}

static int vdmap[] = {
	DT_UNKNOWN,	/* VNON */
	DT_REG,		/* VREG */
	DT_DIR,		/* VDIR */
	DT_BLK,		/* VBLK */
	DT_CHR,		/* VCHR */
	DT_LNK,		/* VLNK */
	DT_SOCK,	/* VSUCK*/
	DT_FIFO,	/* VFIFO*/
	DT_UNKNOWN	/* VBAD */
};
/* XXX: DT_WHT ? */
int
puffs_vtype2dt(enum vtype vt)
{

	if ((int)vt >= VNON && vt < (sizeof(vdmap)/sizeof(vdmap[0])))
		return vdmap[vt];

	return DT_UNKNOWN;
}

enum vtype
puffs_mode2vt(mode_t mode)
{

	switch (mode & S_IFMT) {
	case S_IFIFO:
		return VFIFO;
	case S_IFCHR:
		return VCHR;
	case S_IFDIR:
		return VDIR;
	case S_IFBLK:
		return VBLK;
	case S_IFREG:
		return VREG;
	case S_IFLNK:
		return VLNK;
	case S_IFSOCK:
		return VSOCK;
	default:
		return VBAD; /* XXX: not really true, but ... */
	}
}

void
puffs_stat2vattr(struct vattr *va, const struct stat *sb)
{

	va->va_type = puffs_mode2vt(sb->st_mode);
	va->va_mode = sb->st_mode;
	va->va_nlink = sb->st_nlink;
	va->va_uid = sb->st_uid;
	va->va_gid = sb->st_gid;
	va->va_fsid = sb->st_dev;
	va->va_fileid = sb->st_ino;
	va->va_size = sb->st_size;
	va->va_blocksize = sb->st_blksize;
	va->va_atime = sb->st_atimespec;
	va->va_ctime = sb->st_ctimespec;
	va->va_mtime = sb->st_mtimespec;
	va->va_birthtime = sb->st_birthtimespec;
	va->va_gen = sb->st_gen;
	va->va_flags = sb->st_flags;
	va->va_rdev = sb->st_rdev;
	va->va_bytes = sb->st_blocks << DEV_BSHIFT;
	va->va_filerev = 0;
	va->va_vaflags = 0;
}

mode_t
puffs_addvtype2mode(mode_t mode, enum vtype type)
{

	switch (type) {
	case VCHR:
		mode |= S_IFCHR;
		break;
	case VBLK:
		mode |= S_IFBLK;
		break;
	case VSOCK:
		mode |= S_IFSOCK;
		break;
	case VFIFO:
		mode |= S_IFIFO;
		break;
	case VREG:
		mode |= S_IFREG;
		break;
	case VLNK:
		mode |= S_IFLNK;
		break;
	case VDIR:
		mode |= S_IFDIR;
		break;
	default:
		break;
	}

	return mode;
}

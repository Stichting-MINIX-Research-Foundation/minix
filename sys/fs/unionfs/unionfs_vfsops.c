/*-
 * Copyright (c) 1994, 1995 The Regents of the University of California.
 * Copyright (c) 1994, 1995 Jan-Simon Pendry.
 * Copyright (c) 2005, 2006 Masanori Ozawa <ozawa@ongs.co.jp>, ONGS Inc.
 * Copyright (c) 2006 Daichi Goto <daichi@freebsd.org>
 * All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
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
 *	@(#)union_vfsops.c	8.20 (Berkeley) 5/20/95
 * $FreeBSD: src/sys/fs/unionfs/union_vfsops.c,v 1.89 2008/01/13 14:44:06 attilio Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/module.h>

#include <fs/unionfs/unionfs.h>

MODULE(MODULE_CLASS_VFS, unionfs, "layerfs");

MALLOC_DEFINE(M_UNIONFSMNT, "UNIONFS mount", "UNIONFS mount structure");

struct vfsops unionfs_vfsops;

VFS_PROTOS(unionfs);

static struct sysctllog *unionfs_sysctl_log;

/*
 * Mount unionfs layer.
 */
int
unionfs_mount(struct mount *mp, const char *path, void *data, size_t *data_len)
{
	int		error;
	struct vnode   *lowerrootvp;
	struct vnode   *upperrootvp;
	struct unionfs_mount *ump;
	int		below;
	uid_t		uid;
	gid_t		gid;
	u_short		udir;
	u_short		ufile;
	unionfs_copymode copymode;
	unionfs_whitemode whitemode;
	struct pathbuf *pb;
	struct componentname fakecn;
	struct nameidata nd, *ndp;
	struct vattr	va;
	struct union_args *args = data;
	kauth_cred_t	cred;
	size_t		size;
	size_t		len;
	const char	*cp;
	char		*xp;

	if (args == NULL)
		return EINVAL;
	if (*data_len < sizeof *args)
		return EINVAL;

	UNIONFSDEBUG("unionfs_mount(mp = %p)\n", (void *)mp);

	error = 0;
	below = 0;
	uid = 0;
	gid = 0;
	udir = 0;
	ufile = 0;
	copymode = UNIONFS_TRANSPARENT;	/* default */
	whitemode = UNIONFS_WHITE_ALWAYS;
	ndp = &nd;
	cred = kauth_cred_get();

	if (mp->mnt_flag & MNT_ROOTFS) {
		printf("union_mount: cannot union mount root filesystem\n");
		return (EOPNOTSUPP);
	}

	if (mp->mnt_flag & MNT_GETARGS) {
		ump = MOUNTTOUNIONFSMOUNT(mp);
		if (ump == NULL)
			return EIO;
		args->target = NULL;
		args->mntflags = ump->um_op;
		*data_len = sizeof *args;
		return 0;
	}

	/*
	 * Update is a no operation.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		printf("union_mount: cannot update union mount\n");
		return (EOPNOTSUPP);
	}

	vn_lock(mp->mnt_vnodecovered, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_GETATTR(mp->mnt_vnodecovered, &va, cred);
	if (!error) {
		if (udir == 0)
			udir = va.va_mode;
		if (ufile == 0)
			ufile = va.va_mode;
		uid = va.va_uid;
		gid = va.va_gid;
	}
	VOP_UNLOCK(mp->mnt_vnodecovered);
	if (error)
		return (error);

	switch (args->mntflags & UNMNT_OPMASK) {
	case UNMNT_ABOVE:
		below = 0;
		break;

	case UNMNT_BELOW:
		below = 1;
		break;

	case UNMNT_REPLACE:
	default:
		return EINVAL;
	}

	/* If copymode is UNIONFS_TRADITIONAL, uid/gid is mounted user. */
	if (copymode == UNIONFS_TRADITIONAL) {
		uid = kauth_cred_getuid(cred);
		gid = kauth_cred_getgid(cred);
	}

	UNIONFSDEBUG("unionfs_mount: uid=%d, gid=%d\n", uid, gid);
	UNIONFSDEBUG("unionfs_mount: udir=0%03o, ufile=0%03o\n", udir, ufile);
	UNIONFSDEBUG("unionfs_mount: copymode=%d\n", copymode);

	/*
	 * Find upper node
	 */
	error = pathbuf_copyin(args->target, &pb);
	if (error) {
		return error;
	}
	NDINIT(ndp, LOOKUP, FOLLOW | LOCKLEAF, pb);
	if ((error = namei(ndp))) {
		pathbuf_destroy(pb);
		return error;
	}

	/* get root vnodes */
	lowerrootvp = mp->mnt_vnodecovered;
	upperrootvp = ndp->ni_vp;

	vrele(ndp->ni_dvp);
	ndp->ni_dvp = NULLVP;
	pathbuf_destroy(pb);

	/* create unionfs_mount */
	ump = (struct unionfs_mount *)malloc(sizeof(struct unionfs_mount),
	    M_UNIONFSMNT, M_WAITOK | M_ZERO);

	/*
	 * Save reference
	 */
	if (below) {
		VOP_UNLOCK(upperrootvp);
		vn_lock(lowerrootvp, LK_EXCLUSIVE | LK_RETRY);
		ump->um_lowervp = upperrootvp;
		ump->um_uppervp = lowerrootvp;
	} else {
		ump->um_lowervp = lowerrootvp;
		ump->um_uppervp = upperrootvp;
	}
	ump->um_rootvp = NULLVP;
	ump->um_uid = uid;
	ump->um_gid = gid;
	ump->um_udir = udir;
	ump->um_ufile = ufile;
	ump->um_copymode = copymode;
	ump->um_whitemode = whitemode;

	if ((lowerrootvp->v_mount->mnt_iflag & IMNT_MPSAFE) &&
	    (upperrootvp->v_mount->mnt_flag & IMNT_MPSAFE))
		mp->mnt_iflag |= IMNT_MPSAFE;
	mp->mnt_data = ump;

	/*
	 * Copy upper layer's RDONLY flag.
	 */
	mp->mnt_flag |= ump->um_uppervp->v_mount->mnt_flag & MNT_RDONLY;

	/*
	 * Check whiteout
	 */
	if ((mp->mnt_flag & MNT_RDONLY) == 0) {
		memset(&fakecn, 0, sizeof(fakecn));
		fakecn.cn_nameiop = LOOKUP;
		error = VOP_WHITEOUT(ump->um_uppervp, &fakecn, LOOKUP);
		if (error) {
			if (below) {
				VOP_UNLOCK(ump->um_uppervp);
				vrele(upperrootvp);
			} else
				vput(ump->um_uppervp);
			free(ump, M_UNIONFSMNT);
			mp->mnt_data = NULL;
			return (error);
		}
	}

	/*
	 * Unlock the node
	 */
	VOP_UNLOCK(ump->um_uppervp);

	ump->um_op = args->mntflags & UNMNT_OPMASK;

	/*
	 * Get the unionfs root vnode.
	 */
	error = unionfs_nodeget(mp, ump->um_uppervp, ump->um_lowervp,
	    NULLVP, &(ump->um_rootvp), NULL);
	vrele(upperrootvp);
	if (error) {
		free(ump, M_UNIONFSMNT);
		mp->mnt_data = NULL;
		return (error);
	}

	/*
	 * Check mnt_flag
	 */
	if ((ump->um_lowervp->v_mount->mnt_flag & MNT_LOCAL) &&
	    (ump->um_uppervp->v_mount->mnt_flag & MNT_LOCAL))
		mp->mnt_flag |= MNT_LOCAL;

	/*
	 * Get new fsid
	 */
	vfs_getnewfsid(mp);

	error = set_statvfs_info(path, UIO_USERSPACE, NULL, UIO_USERSPACE,
	    mp->mnt_op->vfs_name, mp, curlwp);
	if (error) { 
		unionfs_noderem(ump->um_rootvp);
		free(ump, M_UNIONFSMNT);
		mp->mnt_data = NULL;
		return (error);
	}

	switch (ump->um_op) {
	case UNMNT_ABOVE:
		cp = "<above>:";
		break;
	case UNMNT_BELOW:
		cp = "<below>:";
		break;
	default:
		panic("union_mount: bad um_op");
		break;
	}
	len = strlen(cp);
	memcpy(mp->mnt_stat.f_mntfromname, cp, len);
	xp = mp->mnt_stat.f_mntfromname + len;
	len = MNAMELEN - len;
	(void) copyinstr(args->target, xp, len - 1, &size);
	memset(xp + size, 0, len - size);

	UNIONFSDEBUG("unionfs_mount: from %s, on %s\n",
	    mp->mnt_stat.f_mntfromname, mp->mnt_stat.f_mntonname);

	return (0);
}

/*
 * Free reference to unionfs layer
 */
int
unionfs_unmount(struct mount *mp, int mntflags)
{
	struct unionfs_mount *ump;
	int		error;
	int		freeing;
	int		flags;

	UNIONFSDEBUG("unionfs_unmount: mp = %p\n", (void *)mp);

	ump = MOUNTTOUNIONFSMOUNT(mp);
	flags = 0;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	/* vflush (no need to call vrele) */
	for (freeing = 0; (error = vflush(mp, NULL, flags)) != 0;) {
		struct vnode *vp;
		int n;

		/* count #vnodes held on mount list */
		n = 0;
		TAILQ_FOREACH(vp, &mp->mnt_vnodelist, v_mntvnodes)
			n++;

		/* if this is unchanged then stop */
		if (n == freeing)
			break;

		/* otherwise try once more time */
		freeing = n;
	}

	if (error)
		return (error);

	free(ump, M_UNIONFSMNT);
	mp->mnt_data = NULL;

	return (0);
}

int
unionfs_root(struct mount *mp, struct vnode **vpp)
{
	struct unionfs_mount *ump;
	struct vnode   *vp;

	ump = MOUNTTOUNIONFSMOUNT(mp);
	vp = ump->um_rootvp;

	UNIONFSDEBUG("unionfs_root: rootvp=%p locked=%x\n",
	    vp, VOP_ISLOCKED(vp));

	vref(vp);
	vn_lock(vp, LK_EXCLUSIVE);

	*vpp = vp;

	return (0);
}

int
unionfs_quotactl(struct mount *mp, struct quotactl_args *args)
{
	struct unionfs_mount *ump;

	ump = MOUNTTOUNIONFSMOUNT(mp);

	/*
	 * Writing is always performed to upper vnode.
	 */
	return (VFS_QUOTACTL(ump->um_uppervp->v_mount, args));
}

int
unionfs_statvfs(struct mount *mp, struct statvfs *sbp)
{
	struct unionfs_mount *ump;
	int		error;
	uint64_t	lbsize;
	struct statvfs *sbuf = malloc(sizeof(*sbuf), M_TEMP, M_WAITOK | M_ZERO);

	ump = MOUNTTOUNIONFSMOUNT(mp);

	UNIONFSDEBUG("unionfs_statvfs(mp = %p, lvp = %p, uvp = %p)\n",
	    (void *)mp, (void *)ump->um_lowervp, (void *)ump->um_uppervp);

	error = VFS_STATVFS(ump->um_lowervp->v_mount, sbuf);
	if (error)
		goto done;

	/* now copy across the "interesting" information and fake the rest */
	sbp->f_blocks = sbuf->f_blocks;
	sbp->f_files = sbuf->f_files;

	lbsize = sbuf->f_bsize;

	error = VFS_STATVFS(ump->um_uppervp->v_mount, sbuf);
	if (error)
		goto done;

	/*
	 * The FS type etc is copy from upper vfs.
	 * (write able vfs have priority)
	 */
	sbp->f_flag = sbuf->f_flag;
	sbp->f_bsize = sbuf->f_bsize;
	sbp->f_iosize = sbuf->f_iosize;

	if (sbuf->f_bsize != lbsize)
		sbp->f_blocks = ((off_t)sbp->f_blocks * lbsize) / sbuf->f_bsize;

	sbp->f_blocks += sbuf->f_blocks;
	sbp->f_bfree = sbuf->f_bfree;
	sbp->f_bavail = sbuf->f_bavail;
	sbp->f_files += sbuf->f_files;
	sbp->f_ffree = sbuf->f_ffree;

 done:
	free(sbuf, M_TEMP);
	return (error);
}

int
unionfs_sync(struct mount *mp, int waitfor, kauth_cred_t cred)
{
	/* nothing to do */
	return (0);
}

int
unionfs_vget(struct mount *mp, ino_t ino, struct vnode **vpp)
{
	return (EOPNOTSUPP);
}

int
unionfs_fhtovp(struct mount *mp, struct fid *fidp, struct vnode **vpp)
{
	return (EOPNOTSUPP);
}

int
unionfs_extattrctl(struct mount *mp, int cmd, struct vnode *filename_vp,
    int namespace, const char *attrname)
{
	struct unionfs_mount *ump;
	struct unionfs_node *unp;

	ump = MOUNTTOUNIONFSMOUNT(mp);
	unp = VTOUNIONFS(filename_vp);

	if (unp->un_uppervp != NULLVP) {
		return (VFS_EXTATTRCTL(ump->um_uppervp->v_mount, cmd,
		    unp->un_uppervp, namespace, attrname));
	} else {
		return (VFS_EXTATTRCTL(ump->um_lowervp->v_mount, cmd,
		    unp->un_lowervp, namespace, attrname));
	}
}

/*
 * Initialize
 */
void 
unionfs_init(void)
{
	UNIONFSDEBUG("unionfs_init\n");	/* printed during system boot */
}

static int
unionfs_renamelock_enter(struct mount *mp)
{
	struct unionfs_mount *um = MOUNTTOUNIONFSMOUNT(mp);

	/* Lock just the upper fs, where the action happens. */
	return VFS_RENAMELOCK_ENTER(um->um_uppervp->v_mount);
}

static void
unionfs_renamelock_exit(struct mount *mp)
{
	struct unionfs_mount *um = MOUNTTOUNIONFSMOUNT(mp);

	VFS_RENAMELOCK_EXIT(um->um_uppervp->v_mount);
}

int
unionfs_start(struct mount *mp, int flags)
{

	return (0);
}

void
unionfs_done(void)
{

	/* Make sure to unset the readdir hook. */
	vn_union_readdir_hook = NULL;
}

extern const struct vnodeopv_desc unionfs_vnodeop_opv_desc;

const struct vnodeopv_desc * const unionfs_vnodeopv_descs[] = {
	&unionfs_vnodeop_opv_desc,
	NULL,
};

struct vfsops unionfs_vfsops = {
	.vfs_name = MOUNT_UNION,
	.vfs_min_mount_data = sizeof (struct unionfs_args),
	.vfs_mount = unionfs_mount,
	.vfs_start = unionfs_start,
	.vfs_unmount = unionfs_unmount,
	.vfs_root = unionfs_root,
	.vfs_quotactl = (void *)eopnotsupp,
	.vfs_statvfs = unionfs_statvfs,
	.vfs_sync = unionfs_sync,
	.vfs_vget = unionfs_vget,
	.vfs_fhtovp = (void *)eopnotsupp,
	.vfs_vptofh = (void *)eopnotsupp,
	.vfs_init = unionfs_init,
	.vfs_done = unionfs_done,
	.vfs_snapshot = (void *)eopnotsupp,
	.vfs_extattrctl = vfs_stdextattrctl,
	.vfs_suspendctl = (void *)eopnotsupp,
	.vfs_renamelock_enter = unionfs_renamelock_enter,
	.vfs_renamelock_exit = unionfs_renamelock_exit,
	.vfs_fsync = (void *)eopnotsupp,
	.vfs_opv_descs = unionfs_vnodeopv_descs
};

static int
unionfs_modcmd(modcmd_t cmd, void *arg)
{
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = vfs_attach(&unionfs_vfsops);
		if (error != 0)
			break;
		sysctl_createv(&unionfs_sysctl_log, 0, NULL, NULL,
			       CTLFLAG_PERMANENT,
			       CTLTYPE_NODE, "union",
			       SYSCTL_DESCR("Union file system"),
			       NULL, 0, NULL, 0,
			       CTL_VFS, 15, CTL_EOL);
		/*
		 * XXX the "15" above could be dynamic, thereby eliminating
		 * one more instance of the "number to vfs" mapping problem,
		 * but "15" is the order as taken from sys/mount.h
		 */
		break;
	case MODULE_CMD_FINI:
		error = vfs_detach(&unionfs_vfsops);
		if (error != 0)
			break;
		sysctl_teardown(&unionfs_sysctl_log);
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

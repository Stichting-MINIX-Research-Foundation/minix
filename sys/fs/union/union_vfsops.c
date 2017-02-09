/*	$NetBSD: union_vfsops.c,v 1.75 2015/07/23 09:45:21 hannken Exp $	*/

/*
 * Copyright (c) 1994 The Regents of the University of California.
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
 *	@(#)union_vfsops.c	8.20 (Berkeley) 5/20/95
 */

/*
 * Copyright (c) 1994 Jan-Simon Pendry.
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
 *	@(#)union_vfsops.c	8.20 (Berkeley) 5/20/95
 */

/*
 * Union Layer
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: union_vfsops.c,v 1.75 2015/07/23 09:45:21 hannken Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/filedesc.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/kauth.h>
#include <sys/module.h>

#include <fs/union/union.h>

MODULE(MODULE_CLASS_VFS, union, NULL);

static struct sysctllog *union_sysctl_log;

/*
 * Mount union filesystem
 */
int
union_mount(struct mount *mp, const char *path, void *data, size_t *data_len)
{
	struct lwp *l = curlwp;
	int error = 0;
	struct union_args *args = data;
	struct vnode *lowerrootvp = NULLVP;
	struct vnode *upperrootvp = NULLVP;
	struct union_mount *um = 0;
	const char *cp;
	char *xp;
	int len;
	size_t size;

	if (args == NULL)
		return EINVAL;
	if (*data_len < sizeof *args)
		return EINVAL;

#ifdef UNION_DIAGNOSTIC
	printf("union_mount(mp = %p)\n", mp);
#endif

	if (mp->mnt_flag & MNT_GETARGS) {
		um = MOUNTTOUNIONMOUNT(mp);
		if (um == NULL)
			return EIO;
		args->target = NULL;
		args->mntflags = um->um_op;
		*data_len = sizeof *args;
		return 0;
	}
	/*
	 * Update is a no-op
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		/*
		 * Need to provide.
		 * 1. a way to convert between rdonly and rdwr mounts.
		 * 2. support for nfs exports.
		 */
		error = EOPNOTSUPP;
		goto bad;
	}

	lowerrootvp = mp->mnt_vnodecovered;
	vref(lowerrootvp);

	/*
	 * Find upper node.
	 */
	error = namei_simple_user(args->target,
				NSM_FOLLOW_NOEMULROOT, &upperrootvp);
	if (error != 0)
		goto bad;

	if (upperrootvp->v_type != VDIR) {
		error = EINVAL;
		goto bad;
	}

	um = kmem_zalloc(sizeof(struct union_mount), KM_SLEEP);

	/*
	 * Keep a held reference to the target vnodes.
	 * They are vrele'd in union_unmount.
	 *
	 * Depending on the _BELOW flag, the filesystems are
	 * viewed in a different order.  In effect, this is the
	 * same as providing a mount under option to the mount syscall.
	 */

	um->um_op = args->mntflags & UNMNT_OPMASK;
	switch (um->um_op) {
	case UNMNT_ABOVE:
		um->um_lowervp = lowerrootvp;
		um->um_uppervp = upperrootvp;
		break;

	case UNMNT_BELOW:
		um->um_lowervp = upperrootvp;
		um->um_uppervp = lowerrootvp;
		break;

	case UNMNT_REPLACE:
		vrele(lowerrootvp);
		lowerrootvp = NULLVP;
		um->um_uppervp = upperrootvp;
		um->um_lowervp = lowerrootvp;
		break;

	default:
		error = EINVAL;
		goto bad;
	}

	mp->mnt_iflag |= IMNT_MPSAFE;

	/*
	 * Unless the mount is readonly, ensure that the top layer
	 * supports whiteout operations
	 */
	if ((mp->mnt_flag & MNT_RDONLY) == 0) {
		vn_lock(um->um_uppervp, LK_EXCLUSIVE | LK_RETRY);
		error = VOP_WHITEOUT(um->um_uppervp,
		    (struct componentname *) 0, LOOKUP);
		VOP_UNLOCK(um->um_uppervp);
		if (error)
			goto bad;
	}

	um->um_cred = l->l_cred;
	kauth_cred_hold(um->um_cred);
	um->um_cmode = UN_DIRMODE &~ l->l_proc->p_cwdi->cwdi_cmask;

	/*
	 * Depending on what you think the MNT_LOCAL flag might mean,
	 * you may want the && to be || on the conditional below.
	 * At the moment it has been defined that the filesystem is
	 * only local if it is all local, ie the MNT_LOCAL flag implies
	 * that the entire namespace is local.  If you think the MNT_LOCAL
	 * flag implies that some of the files might be stored locally
	 * then you will want to change the conditional.
	 */
	if (um->um_op == UNMNT_ABOVE) {
		if (((um->um_lowervp == NULLVP) ||
		     (um->um_lowervp->v_mount->mnt_flag & MNT_LOCAL)) &&
		    (um->um_uppervp->v_mount->mnt_flag & MNT_LOCAL))
			mp->mnt_flag |= MNT_LOCAL;
	}

	/*
	 * Copy in the upper layer's RDONLY flag.  This is for the benefit
	 * of lookup() which explicitly checks the flag, rather than asking
	 * the filesystem for its own opinion.  This means, that an update
	 * mount of the underlying filesystem to go from rdonly to rdwr
	 * will leave the unioned view as read-only.
	 */
	mp->mnt_flag |= (um->um_uppervp->v_mount->mnt_flag & MNT_RDONLY);

	mp->mnt_data = um;
	vfs_getnewfsid(mp);

	error = set_statvfs_info( path, UIO_USERSPACE, NULL, UIO_USERSPACE,
	    mp->mnt_op->vfs_name, mp, l);
	if (error)
		goto bad;

	switch (um->um_op) {
	case UNMNT_ABOVE:
		cp = "<above>:";
		break;
	case UNMNT_BELOW:
		cp = "<below>:";
		break;
	case UNMNT_REPLACE:
		cp = "";
		break;
	default:
		cp = "<invalid>:";
#ifdef DIAGNOSTIC
		panic("union_mount: bad um_op");
#endif
		break;
	}
	len = strlen(cp);
	memcpy(mp->mnt_stat.f_mntfromname, cp, len);

	xp = mp->mnt_stat.f_mntfromname + len;
	len = MNAMELEN - len;

	(void) copyinstr(args->target, xp, len - 1, &size);
	memset(xp + size, 0, len - size);

#ifdef UNION_DIAGNOSTIC
	printf("union_mount: from %s, on %s\n",
	    mp->mnt_stat.f_mntfromname, mp->mnt_stat.f_mntonname);
#endif

	/* Setup the readdir hook if it's not set already */
	if (!vn_union_readdir_hook)
		vn_union_readdir_hook = union_readdirhook;

	return (0);

bad:
	if (um)
		kmem_free(um, sizeof(struct union_mount));
	if (upperrootvp)
		vrele(upperrootvp);
	if (lowerrootvp)
		vrele(lowerrootvp);
	return (error);
}

/*
 * VFS start.  Nothing needed here - the start routine
 * on the underlying filesystem(s) will have been called
 * when that filesystem was mounted.
 */
 /*ARGSUSED*/
int
union_start(struct mount *mp, int flags)
{

	return (0);
}

/*
 * Free reference to union layer
 */
static bool
union_unmount_selector(void *cl, struct vnode *vp)
{
	int *count = cl;

	*count += 1;
	return false;
}

int
union_unmount(struct mount *mp, int mntflags)
{
	struct union_mount *um = MOUNTTOUNIONMOUNT(mp);
	int freeing;
	int error;

#ifdef UNION_DIAGNOSTIC
	printf("union_unmount(mp = %p)\n", mp);
#endif

	/*
	 * Keep flushing vnodes from the mount list.
	 * This is needed because of the un_pvp held
	 * reference to the parent vnode.
	 * If more vnodes have been freed on a given pass,
	 * the try again.  The loop will iterate at most
	 * (d) times, where (d) is the maximum tree depth
	 * in the filesystem.
	 */
	for (freeing = 0; (error = vflush(mp, NULL, 0)) != 0;) {
		struct vnode_iterator *marker;
		int n;

		/* count #vnodes held on mount list */
		n = 0;
		vfs_vnode_iterator_init(mp, &marker);
		vfs_vnode_iterator_next(marker, union_unmount_selector, &n);
		vfs_vnode_iterator_destroy(marker);

		/* if this is unchanged then stop */
		if (n == freeing)
			break;

		/* otherwise try once more time */
		freeing = n;
	}

	/*
	 * Ok, now that we've tried doing it gently, get out the hammer.
	 */

	if (mntflags & MNT_FORCE)
		error = vflush(mp, NULL, FORCECLOSE);

	if (error)
		return error;

	/*
	 * Discard references to upper and lower target vnodes.
	 */
	if (um->um_lowervp)
		vrele(um->um_lowervp);
	vrele(um->um_uppervp);
	kauth_cred_free(um->um_cred);
	/*
	 * Finally, throw away the union_mount structure
	 */
	kmem_free(um, sizeof(struct union_mount));
	mp->mnt_data = NULL;
	return 0;
}

int
union_root(struct mount *mp, struct vnode **vpp)
{
	struct union_mount *um = MOUNTTOUNIONMOUNT(mp);
	int error;

	/*
	 * Return locked reference to root.
	 */
	vref(um->um_uppervp);
	if (um->um_lowervp)
		vref(um->um_lowervp);
	error = union_allocvp(vpp, mp, NULL, NULL, NULL,
			      um->um_uppervp, um->um_lowervp, 1);

	if (error) {
		vrele(um->um_uppervp);
		if (um->um_lowervp)
			vrele(um->um_lowervp);
		return error;
	}

	vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY);

	return 0;
}

int
union_statvfs(struct mount *mp, struct statvfs *sbp)
{
	int error;
	struct union_mount *um = MOUNTTOUNIONMOUNT(mp);
	struct statvfs *sbuf = malloc(sizeof(*sbuf), M_TEMP, M_WAITOK | M_ZERO);
	unsigned long lbsize;

#ifdef UNION_DIAGNOSTIC
	printf("union_statvfs(mp = %p, lvp = %p, uvp = %p)\n", mp,
	    um->um_lowervp, um->um_uppervp);
#endif

	if (um->um_lowervp) {
		error = VFS_STATVFS(um->um_lowervp->v_mount, sbuf);
		if (error)
			goto done;
	}

	/* now copy across the "interesting" information and fake the rest */
	lbsize = sbuf->f_bsize;
	sbp->f_blocks = sbuf->f_blocks - sbuf->f_bfree;
	sbp->f_files = sbuf->f_files - sbuf->f_ffree;

	error = VFS_STATVFS(um->um_uppervp->v_mount, sbuf);
	if (error)
		goto done;

	sbp->f_flag = sbuf->f_flag;
	sbp->f_bsize = sbuf->f_bsize;
	sbp->f_frsize = sbuf->f_frsize;
	sbp->f_iosize = sbuf->f_iosize;

	/*
	 * The "total" fields count total resources in all layers,
	 * the "free" fields count only those resources which are
	 * free in the upper layer (since only the upper layer
	 * is writable).
	 */

	if (sbuf->f_bsize != lbsize)
		sbp->f_blocks = sbp->f_blocks * lbsize / sbuf->f_bsize;
	sbp->f_blocks += sbuf->f_blocks;
	sbp->f_bfree = sbuf->f_bfree;
	sbp->f_bavail = sbuf->f_bavail;
	sbp->f_bresvd = sbuf->f_bresvd;
	sbp->f_files += sbuf->f_files;
	sbp->f_ffree = sbuf->f_ffree;
	sbp->f_favail = sbuf->f_favail;
	sbp->f_fresvd = sbuf->f_fresvd;

	copy_statvfs_info(sbp, mp);
done:
	free(sbuf, M_TEMP);
	return error;
}

/*ARGSUSED*/
int
union_sync(struct mount *mp, int waitfor,
    kauth_cred_t cred)
{

	/*
	 * XXX - Assumes no data cached at union layer.
	 */
	return (0);
}

/*ARGSUSED*/
int
union_vget(struct mount *mp, ino_t ino,
    struct vnode **vpp)
{

	return (EOPNOTSUPP);
}

static int
union_renamelock_enter(struct mount *mp)
{
	struct union_mount *um = MOUNTTOUNIONMOUNT(mp);

	/* Lock just the upper fs, where the action happens. */
	return VFS_RENAMELOCK_ENTER(um->um_uppervp->v_mount);
}

static void
union_renamelock_exit(struct mount *mp)
{
	struct union_mount *um = MOUNTTOUNIONMOUNT(mp);

	VFS_RENAMELOCK_EXIT(um->um_uppervp->v_mount);
}

extern const struct vnodeopv_desc union_vnodeop_opv_desc;

const struct vnodeopv_desc * const union_vnodeopv_descs[] = {
	&union_vnodeop_opv_desc,
	NULL,
};

struct vfsops union_vfsops = {
	.vfs_name = MOUNT_UNION,
	.vfs_min_mount_data = sizeof (struct union_args),
	.vfs_mount = union_mount,
	.vfs_start = union_start,
	.vfs_unmount = union_unmount,
	.vfs_root = union_root,
	.vfs_quotactl = (void *)eopnotsupp,
	.vfs_statvfs = union_statvfs,
	.vfs_sync = union_sync,
	.vfs_vget = union_vget,
	.vfs_loadvnode = union_loadvnode,
	.vfs_fhtovp = (void *)eopnotsupp,
	.vfs_vptofh = (void *)eopnotsupp,
	.vfs_init = union_init,
	.vfs_reinit = union_reinit,
	.vfs_done = union_done,
	.vfs_snapshot = (void *)eopnotsupp,
	.vfs_extattrctl = vfs_stdextattrctl,
	.vfs_suspendctl = (void *)eopnotsupp,
	.vfs_renamelock_enter = union_renamelock_enter,
	.vfs_renamelock_exit = union_renamelock_exit,
	.vfs_fsync = (void *)eopnotsupp,
	.vfs_opv_descs = union_vnodeopv_descs
};

static int
union_modcmd(modcmd_t cmd, void *arg)
{
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = vfs_attach(&union_vfsops);
		if (error != 0)
			break;
		sysctl_createv(&union_sysctl_log, 0, NULL, NULL,
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
		error = vfs_detach(&union_vfsops);
		if (error != 0)
			break;
		sysctl_teardown(&union_sysctl_log);
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

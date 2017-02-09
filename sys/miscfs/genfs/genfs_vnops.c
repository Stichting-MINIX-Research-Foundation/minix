/*	$NetBSD: genfs_vnops.c,v 1.192 2014/03/24 13:42:40 hannken Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: genfs_vnops.c,v 1.192 2014/03/24 13:42:40 hannken Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/fstrans.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/kmem.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/kauth.h>
#include <sys/stat.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/genfs/genfs_node.h>
#include <miscfs/specfs/specdev.h>

#include <uvm/uvm.h>
#include <uvm/uvm_pager.h>

static void filt_genfsdetach(struct knote *);
static int filt_genfsread(struct knote *, long);
static int filt_genfsvnode(struct knote *, long);

int
genfs_poll(void *v)
{
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int a_events;
		struct lwp *a_l;
	} */ *ap = v;

	return (ap->a_events & (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM));
}

int
genfs_seek(void *v)
{
	struct vop_seek_args /* {
		struct vnode *a_vp;
		off_t a_oldoff;
		off_t a_newoff;
		kauth_cred_t cred;
	} */ *ap = v;

	if (ap->a_newoff < 0)
		return (EINVAL);

	return (0);
}

int
genfs_abortop(void *v)
{
	struct vop_abortop_args /* {
		struct vnode *a_dvp;
		struct componentname *a_cnp;
	} */ *ap = v;

	(void)ap;

	return (0);
}

int
genfs_fcntl(void *v)
{
	struct vop_fcntl_args /* {
		struct vnode *a_vp;
		u_int a_command;
		void *a_data;
		int a_fflag;
		kauth_cred_t a_cred;
		struct lwp *a_l;
	} */ *ap = v;

	if (ap->a_command == F_SETFL)
		return (0);
	else
		return (EOPNOTSUPP);
}

/*ARGSUSED*/
int
genfs_badop(void *v)
{

	panic("genfs: bad op");
}

/*ARGSUSED*/
int
genfs_nullop(void *v)
{

	return (0);
}

/*ARGSUSED*/
int
genfs_einval(void *v)
{

	return (EINVAL);
}

/*
 * Called when an fs doesn't support a particular vop.
 * This takes care to vrele, vput, or vunlock passed in vnodes
 * and calls VOP_ABORTOP for a componentname (in non-rename VOP).
 */
int
genfs_eopnotsupp(void *v)
{
	struct vop_generic_args /*
		struct vnodeop_desc *a_desc;
		/ * other random data follows, presumably * /
	} */ *ap = v;
	struct vnodeop_desc *desc = ap->a_desc;
	struct vnode *vp, *vp_last = NULL;
	int flags, i, j, offset_cnp, offset_vp;

	KASSERT(desc->vdesc_offset != VOP_LOOKUP_DESCOFFSET);
	KASSERT(desc->vdesc_offset != VOP_ABORTOP_DESCOFFSET);

	/*
	 * Abort any componentname that lookup potentially left state in.
	 *
	 * As is logical, componentnames for VOP_RENAME are handled by
	 * the caller of VOP_RENAME.  Yay, rename!
	 */
	if (desc->vdesc_offset != VOP_RENAME_DESCOFFSET &&
	    (offset_vp = desc->vdesc_vp_offsets[0]) != VDESC_NO_OFFSET &&
	    (offset_cnp = desc->vdesc_componentname_offset) != VDESC_NO_OFFSET){
		struct componentname *cnp;
		struct vnode *dvp;

		dvp = *VOPARG_OFFSETTO(struct vnode **, offset_vp, ap);
		cnp = *VOPARG_OFFSETTO(struct componentname **, offset_cnp, ap);

		VOP_ABORTOP(dvp, cnp);
	}

	flags = desc->vdesc_flags;
	for (i = 0; i < VDESC_MAX_VPS; flags >>=1, i++) {
		if ((offset_vp = desc->vdesc_vp_offsets[i]) == VDESC_NO_OFFSET)
			break;	/* stop at end of list */
		if ((j = flags & VDESC_VP0_WILLPUT)) {
			vp = *VOPARG_OFFSETTO(struct vnode **, offset_vp, ap);

			/* Skip if NULL */
			if (!vp)
				continue;

			switch (j) {
			case VDESC_VP0_WILLPUT:
				/* Check for dvp == vp cases */
				if (vp == vp_last)
					vrele(vp);
				else {
					vput(vp);
					vp_last = vp;
				}
				break;
			case VDESC_VP0_WILLUNLOCK:
				VOP_UNLOCK(vp);
				break;
			case VDESC_VP0_WILLRELE:
				vrele(vp);
				break;
			}
		}
	}

	return (EOPNOTSUPP);
}

/*ARGSUSED*/
int
genfs_ebadf(void *v)
{

	return (EBADF);
}

/* ARGSUSED */
int
genfs_enoioctl(void *v)
{

	return (EPASSTHROUGH);
}


/*
 * Eliminate all activity associated with the requested vnode
 * and with all vnodes aliased to the requested vnode.
 */
int
genfs_revoke(void *v)
{
	struct vop_revoke_args /* {
		struct vnode *a_vp;
		int a_flags;
	} */ *ap = v;

#ifdef DIAGNOSTIC
	if ((ap->a_flags & REVOKEALL) == 0)
		panic("genfs_revoke: not revokeall");
#endif
	vrevoke(ap->a_vp);
	return (0);
}

/*
 * Lock the node (for deadfs).
 */
int
genfs_deadlock(void *v)
{
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	int flags = ap->a_flags;
	krw_t op;
	int error;

	op = (ISSET(flags, LK_EXCLUSIVE) ? RW_WRITER : RW_READER);
	if (ISSET(flags, LK_NOWAIT)) {
		if (! rw_tryenter(&vp->v_lock, op))
			return EBUSY;
		if (mutex_tryenter(vp->v_interlock)) {
			error = vdead_check(vp, VDEAD_NOWAIT);
			if (error == ENOENT && ISSET(flags, LK_RETRY))
				error = 0;
			mutex_exit(vp->v_interlock);
		} else
			error = EBUSY;
		if (error)
			rw_exit(&vp->v_lock);
		return error;
	}

	rw_enter(&vp->v_lock, op);
	mutex_enter(vp->v_interlock);
	error = vdead_check(vp, VDEAD_NOWAIT);
	if (error == EBUSY) {
		rw_exit(&vp->v_lock);
		error = vdead_check(vp, 0);
		KASSERT(error == ENOENT);
		mutex_exit(vp->v_interlock);
		rw_enter(&vp->v_lock, op);
		mutex_enter(vp->v_interlock);
	}
	KASSERT(error == ENOENT);
	mutex_exit(vp->v_interlock);
	if (! ISSET(flags, LK_RETRY)) {
		rw_exit(&vp->v_lock);
		return ENOENT;
	}
	return 0;
}

/*
 * Unlock the node (for deadfs).
 */
int
genfs_deadunlock(void *v)
{
	struct vop_unlock_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	rw_exit(&vp->v_lock);

	return 0;
}

/*
 * Lock the node.
 */
int
genfs_lock(void *v)
{
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct mount *mp = vp->v_mount;
	int flags = ap->a_flags;
	krw_t op;
	int error;

	op = (ISSET(flags, LK_EXCLUSIVE) ? RW_WRITER : RW_READER);
	if (ISSET(flags, LK_NOWAIT)) {
		if (fstrans_start_nowait(mp, FSTRANS_SHARED))
			return EBUSY;
		if (! rw_tryenter(&vp->v_lock, op)) {
			fstrans_done(mp);
			return EBUSY;
		}
		if (mutex_tryenter(vp->v_interlock)) {
			error = vdead_check(vp, VDEAD_NOWAIT);
			mutex_exit(vp->v_interlock);
		} else
			error = EBUSY;
		if (error) {
			rw_exit(&vp->v_lock);
			fstrans_done(mp);
		}
		return error;
	}

	fstrans_start(mp, FSTRANS_SHARED);
	rw_enter(&vp->v_lock, op);
	mutex_enter(vp->v_interlock);
	error = vdead_check(vp, VDEAD_NOWAIT);
	if (error) {
		rw_exit(&vp->v_lock);
		fstrans_done(mp);
		error = vdead_check(vp, 0);
		KASSERT(error == ENOENT);
	}
	mutex_exit(vp->v_interlock);
	return error;
}

/*
 * Unlock the node.
 */
int
genfs_unlock(void *v)
{
	struct vop_unlock_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct mount *mp = vp->v_mount;

	rw_exit(&vp->v_lock);
	fstrans_done(mp);

	return 0;
}

/*
 * Return whether or not the node is locked.
 */
int
genfs_islocked(void *v)
{
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	if (rw_write_held(&vp->v_lock))
		return LK_EXCLUSIVE;

	if (rw_read_held(&vp->v_lock))
		return LK_SHARED;

	return 0;
}

/*
 * Stubs to use when there is no locking to be done on the underlying object.
 */
int
genfs_nolock(void *v)
{

	return (0);
}

int
genfs_nounlock(void *v)
{

	return (0);
}

int
genfs_noislocked(void *v)
{

	return (0);
}

int
genfs_mmap(void *v)
{

	return (0);
}

/*
 * VOP_PUTPAGES() for vnodes which never have pages.
 */

int
genfs_null_putpages(void *v)
{
	struct vop_putpages_args /* {
		struct vnode *a_vp;
		voff_t a_offlo;
		voff_t a_offhi;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	KASSERT(vp->v_uobj.uo_npages == 0);
	mutex_exit(vp->v_interlock);
	return (0);
}

void
genfs_node_init(struct vnode *vp, const struct genfs_ops *ops)
{
	struct genfs_node *gp = VTOG(vp);

	rw_init(&gp->g_glock);
	gp->g_op = ops;
}

void
genfs_node_destroy(struct vnode *vp)
{
	struct genfs_node *gp = VTOG(vp);

	rw_destroy(&gp->g_glock);
}

void
genfs_size(struct vnode *vp, off_t size, off_t *eobp, int flags)
{
	int bsize;

	bsize = 1 << vp->v_mount->mnt_fs_bshift;
	*eobp = (size + bsize - 1) & ~(bsize - 1);
}

static void
filt_genfsdetach(struct knote *kn)
{
	struct vnode *vp = (struct vnode *)kn->kn_hook;

	mutex_enter(vp->v_interlock);
	SLIST_REMOVE(&vp->v_klist, kn, knote, kn_selnext);
	mutex_exit(vp->v_interlock);
}

static int
filt_genfsread(struct knote *kn, long hint)
{
	struct vnode *vp = (struct vnode *)kn->kn_hook;
	int rv;

	/*
	 * filesystem is gone, so set the EOF flag and schedule
	 * the knote for deletion.
	 */
	switch (hint) {
	case NOTE_REVOKE:
		KASSERT(mutex_owned(vp->v_interlock));
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);
		return (1);
	case 0:
		mutex_enter(vp->v_interlock);
		kn->kn_data = vp->v_size - ((file_t *)kn->kn_obj)->f_offset;
		rv = (kn->kn_data != 0);
		mutex_exit(vp->v_interlock);
		return rv;
	default:
		KASSERT(mutex_owned(vp->v_interlock));
		kn->kn_data = vp->v_size - ((file_t *)kn->kn_obj)->f_offset;
		return (kn->kn_data != 0);
	}
}

static int
filt_genfsvnode(struct knote *kn, long hint)
{
	struct vnode *vp = (struct vnode *)kn->kn_hook;
	int fflags;

	switch (hint) {
	case NOTE_REVOKE:
		KASSERT(mutex_owned(vp->v_interlock));
		kn->kn_flags |= EV_EOF;
		if ((kn->kn_sfflags & hint) != 0)
			kn->kn_fflags |= hint;
		return (1);
	case 0:
		mutex_enter(vp->v_interlock);
		fflags = kn->kn_fflags;
		mutex_exit(vp->v_interlock);
		break;
	default:
		KASSERT(mutex_owned(vp->v_interlock));
		if ((kn->kn_sfflags & hint) != 0)
			kn->kn_fflags |= hint;
		fflags = kn->kn_fflags;
		break;
	}

	return (fflags != 0);
}

static const struct filterops genfsread_filtops =
	{ 1, NULL, filt_genfsdetach, filt_genfsread };
static const struct filterops genfsvnode_filtops =
	{ 1, NULL, filt_genfsdetach, filt_genfsvnode };

int
genfs_kqfilter(void *v)
{
	struct vop_kqfilter_args /* {
		struct vnode	*a_vp;
		struct knote	*a_kn;
	} */ *ap = v;
	struct vnode *vp;
	struct knote *kn;

	vp = ap->a_vp;
	kn = ap->a_kn;
	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &genfsread_filtops;
		break;
	case EVFILT_VNODE:
		kn->kn_fop = &genfsvnode_filtops;
		break;
	default:
		return (EINVAL);
	}

	kn->kn_hook = vp;

	mutex_enter(vp->v_interlock);
	SLIST_INSERT_HEAD(&vp->v_klist, kn, kn_selnext);
	mutex_exit(vp->v_interlock);

	return (0);
}

void
genfs_node_wrlock(struct vnode *vp)
{
	struct genfs_node *gp = VTOG(vp);

	rw_enter(&gp->g_glock, RW_WRITER);
}

void
genfs_node_rdlock(struct vnode *vp)
{
	struct genfs_node *gp = VTOG(vp);

	rw_enter(&gp->g_glock, RW_READER);
}

int
genfs_node_rdtrylock(struct vnode *vp)
{
	struct genfs_node *gp = VTOG(vp);

	return rw_tryenter(&gp->g_glock, RW_READER);
}

void
genfs_node_unlock(struct vnode *vp)
{
	struct genfs_node *gp = VTOG(vp);

	rw_exit(&gp->g_glock);
}

int
genfs_node_wrlocked(struct vnode *vp)
{
	struct genfs_node *gp = VTOG(vp);

	return rw_write_held(&gp->g_glock);
}

/*
 * Do the usual access checking.
 * file_mode, uid and gid are from the vnode in question,
 * while acc_mode and cred are from the VOP_ACCESS parameter list
 */
int
genfs_can_access(enum vtype type, mode_t file_mode, uid_t uid, gid_t gid,
    mode_t acc_mode, kauth_cred_t cred)
{
	mode_t mask;
	int error, ismember;

	mask = 0;

	/* Otherwise, check the owner. */
	if (kauth_cred_geteuid(cred) == uid) {
		if (acc_mode & VEXEC)
			mask |= S_IXUSR;
		if (acc_mode & VREAD)
			mask |= S_IRUSR;
		if (acc_mode & VWRITE)
			mask |= S_IWUSR;
		return ((file_mode & mask) == mask ? 0 : EACCES);
	}

	/* Otherwise, check the groups. */
	error = kauth_cred_ismember_gid(cred, gid, &ismember);
	if (error)
		return (error);
	if (kauth_cred_getegid(cred) == gid || ismember) {
		if (acc_mode & VEXEC)
			mask |= S_IXGRP;
		if (acc_mode & VREAD)
			mask |= S_IRGRP;
		if (acc_mode & VWRITE)
			mask |= S_IWGRP;
		return ((file_mode & mask) == mask ? 0 : EACCES);
	}

	/* Otherwise, check everyone else. */
	if (acc_mode & VEXEC)
		mask |= S_IXOTH;
	if (acc_mode & VREAD)
		mask |= S_IROTH;
	if (acc_mode & VWRITE)
		mask |= S_IWOTH;
	return ((file_mode & mask) == mask ? 0 : EACCES);
}

/*
 * Common routine to check if chmod() is allowed.
 *
 * Policy:
 *   - You must own the file, and
 *     - You must not set the "sticky" bit (meaningless, see chmod(2))
 *     - You must be a member of the group if you're trying to set the
 *       SGIDf bit
 *
 * cred - credentials of the invoker
 * vp - vnode of the file-system object
 * cur_uid, cur_gid - current uid/gid of the file-system object
 * new_mode - new mode for the file-system object
 *
 * Returns 0 if the change is allowed, or an error value otherwise.
 */
int
genfs_can_chmod(enum vtype type, kauth_cred_t cred, uid_t cur_uid,
    gid_t cur_gid, mode_t new_mode)
{
	int error;

	/* The user must own the file. */
	if (kauth_cred_geteuid(cred) != cur_uid)
		return (EPERM);

	/*
	 * Unprivileged users can't set the sticky bit on files.
	 */
	if ((type != VDIR) && (new_mode & S_ISTXT))
		return (EFTYPE);

	/*
	 * If the invoker is trying to set the SGID bit on the file,
	 * check group membership.
	 */
	if (new_mode & S_ISGID) {
		int ismember;

		error = kauth_cred_ismember_gid(cred, cur_gid,
		    &ismember);
		if (error || !ismember)
			return (EPERM);
	}

	return (0);
}

/*
 * Common routine to check if chown() is allowed.
 *
 * Policy:
 *   - You must own the file, and
 *     - You must not try to change ownership, and
 *     - You must be member of the new group
 *
 * cred - credentials of the invoker
 * cur_uid, cur_gid - current uid/gid of the file-system object
 * new_uid, new_gid - target uid/gid of the file-system object
 *
 * Returns 0 if the change is allowed, or an error value otherwise.
 */
int	
genfs_can_chown(kauth_cred_t cred, uid_t cur_uid,
    gid_t cur_gid, uid_t new_uid, gid_t new_gid)
{
	int error, ismember;

	/*
	 * You can only change ownership of a file if:
	 * You own the file and...
	 */
	if (kauth_cred_geteuid(cred) == cur_uid) {
		/*
		 * You don't try to change ownership, and...
		 */
		if (new_uid != cur_uid)
			return (EPERM);

		/*
		 * You don't try to change group (no-op), or...
		 */
		if (new_gid == cur_gid)
			return (0);

		/*
		 * Your effective gid is the new gid, or...
		 */
		if (kauth_cred_getegid(cred) == new_gid)
			return (0);

		/*
		 * The new gid is one you're a member of.
		 */
		ismember = 0;
		error = kauth_cred_ismember_gid(cred, new_gid,
		    &ismember);
		if (!error && ismember)
			return (0);
	}

	return (EPERM);
}

int
genfs_can_chtimes(vnode_t *vp, u_int vaflags, uid_t owner_uid,
    kauth_cred_t cred)
{
	int error;

	/* Must be owner, or... */
	if (kauth_cred_geteuid(cred) == owner_uid)
		return (0);

	/* set the times to the current time, and... */
	if ((vaflags & VA_UTIMES_NULL) == 0)
		return (EPERM);

	/* have write access. */
	error = VOP_ACCESS(vp, VWRITE, cred);
	if (error)
		return (error);

	return (0);
}

/*
 * Common routine to check if chflags() is allowed.
 *
 * Policy:
 *   - You must own the file, and
 *   - You must not change system flags, and
 *   - You must not change flags on character/block devices.
 *
 * cred - credentials of the invoker
 * owner_uid - uid of the file-system object
 * changing_sysflags - true if the invoker wants to change system flags
 */
int
genfs_can_chflags(kauth_cred_t cred, enum vtype type, uid_t owner_uid,
    bool changing_sysflags)
{

	/* The user must own the file. */
	if (kauth_cred_geteuid(cred) != owner_uid) {
		return EPERM;
	}

	if (changing_sysflags) {
		return EPERM;
	}

	/*
	 * Unprivileged users cannot change the flags on devices, even if they
	 * own them.
	 */
	if (type == VCHR || type == VBLK) {
		return EPERM;
	}

	return 0;
}

/*
 * Common "sticky" policy.
 *
 * When a directory is "sticky" (as determined by the caller), this
 * function may help implementing the following policy:
 * - Renaming a file in it is only possible if the user owns the directory
 *   or the file being renamed.
 * - Deleting a file from it is only possible if the user owns the
 *   directory or the file being deleted.
 */
int
genfs_can_sticky(kauth_cred_t cred, uid_t dir_uid, uid_t file_uid)
{
	if (kauth_cred_geteuid(cred) != dir_uid &&
	    kauth_cred_geteuid(cred) != file_uid)
		return EPERM;

	return 0;
}

int
genfs_can_extattr(kauth_cred_t cred, int access_mode, vnode_t *vp,
    const char *attr)
{
	/* We can't allow privileged namespaces. */
	if (strncasecmp(attr, "system", 6) == 0)
		return EPERM;

	return VOP_ACCESS(vp, access_mode, cred);
}

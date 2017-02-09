/*	$NetBSD: dead_vnops.c,v 1.59 2015/04/20 23:30:58 riastradh Exp $	*/

/*
 * Copyright (c) 1989, 1993
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
 *	@(#)dead_vnops.c	8.2 (Berkeley) 11/21/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dead_vnops.c,v 1.59 2015/04/20 23:30:58 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/proc.h>

#include <miscfs/genfs/genfs.h>

/*
 * Prototypes for dead operations on vnodes.
 */
#define dead_bwrite	genfs_nullop
int	dead_lookup(void *);
int	dead_open(void *);
#define dead_close	genfs_nullop
int	dead_read(void *);
int	dead_write(void *);
#define dead_fcntl	genfs_nullop
int	dead_ioctl(void *);
int	dead_poll(void *);
int	dead_remove(void *);
int	dead_link(void *);
int	dead_rename(void *);
int	dead_rmdir(void *);
#define dead_fsync	genfs_nullop
#define dead_seek	genfs_nullop
int	dead_inactive(void *);
#define dead_reclaim	genfs_nullop
#define dead_lock	genfs_deadlock
#define dead_unlock	genfs_deadunlock
int	dead_bmap(void *);
int	dead_strategy(void *);
int	dead_print(void *);
#define dead_islocked	genfs_deadislocked
#define dead_revoke	genfs_nullop
int	dead_getpages(void *);
int	dead_putpages(void *);

int	dead_default_error(void *);

int (**dead_vnodeop_p)(void *);

const struct vnodeopv_entry_desc dead_vnodeop_entries[] = {
	{ &vop_default_desc, dead_default_error },
	{ &vop_bwrite_desc, dead_bwrite },		/* bwrite */
	{ &vop_lookup_desc, dead_lookup },		/* lookup */
	{ &vop_open_desc, dead_open },			/* open */
	{ &vop_close_desc, dead_close },		/* close */
	{ &vop_read_desc, dead_read },			/* read */
	{ &vop_write_desc, dead_write },		/* write */
	{ &vop_fallocate_desc, genfs_eopnotsupp },	/* fallocate */
	{ &vop_fdiscard_desc, genfs_eopnotsupp },	/* fdiscard */
	{ &vop_fcntl_desc, dead_fcntl },		/* fcntl */
	{ &vop_ioctl_desc, dead_ioctl },		/* ioctl */
	{ &vop_poll_desc, dead_poll },			/* poll */
	{ &vop_remove_desc, dead_remove },		/* remove */
	{ &vop_link_desc, dead_link },			/* link */
	{ &vop_rename_desc, dead_rename },		/* rename */
	{ &vop_rmdir_desc, dead_rmdir },		/* rmdir */
	{ &vop_fsync_desc, dead_fsync },		/* fsync */
	{ &vop_seek_desc, dead_seek },			/* seek */
	{ &vop_inactive_desc, dead_inactive },		/* inactive */
	{ &vop_reclaim_desc, dead_reclaim },		/* reclaim */
	{ &vop_lock_desc, dead_lock },			/* lock */
	{ &vop_unlock_desc, dead_unlock },		/* unlock */
	{ &vop_bmap_desc, dead_bmap },			/* bmap */
	{ &vop_strategy_desc, dead_strategy },		/* strategy */
	{ &vop_print_desc, dead_print },		/* print */
	{ &vop_islocked_desc, dead_islocked },		/* islocked */
	{ &vop_revoke_desc, dead_revoke },		/* revoke */
	{ &vop_getpages_desc, dead_getpages },		/* getpages */
	{ &vop_putpages_desc, dead_putpages },		/* putpages */
	{ NULL, NULL }
};
const struct vnodeopv_desc dead_vnodeop_opv_desc =
	{ &dead_vnodeop_p, dead_vnodeop_entries };

/* ARGSUSED */
int
dead_default_error(void *v)
{

	return EBADF;
}

int
dead_bmap(void *v)
{
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *ap = v;

	(void)ap;

	return (EIO);
}

int
dead_lookup(void *v)
{
	struct vop_lookup_v2_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap = v;

	*(ap->a_vpp) = NULL;

	return ENOENT;
}

int
dead_open(void *v)
{
	struct vop_open_args /* {
		struct vnode *a_vp;
		int a_mode;
		kauth_cred_t a_cred;
	} */ *ap = v;

	(void)ap;

	return (ENXIO);
}

int
dead_read(void *v)
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;

	/*
	 * Return EOF for tty devices, EIO for others
	 */
	if ((ap->a_vp->v_vflag & VV_ISTTY) == 0)
		return (EIO);
	return (0);
}

int
dead_write(void *v)
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;

	(void)ap;

	return (EIO);
}

int
dead_ioctl(void *v)
{
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		u_long a_command;
		void *a_data;
		int  a_fflag;
		kauth_cred_t a_cred;
		struct lwp *a_l;
	} */ *ap = v;

	(void)ap;

	return (EBADF);
}

int
dead_poll(void *v)
{
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int a_events;
		struct lwp *a_l;
	} */ *ap = v;

	/*
	 * Let the user find out that the descriptor is gone.
	 */
	return (ap->a_events);
}

int
dead_remove(void *v)
{
	struct vop_remove_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;

	vput(ap->a_dvp);
	vput(ap->a_vp);

	return EIO;
}

int
dead_link(void *v)
{
	struct vop_link_v2_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;

	(void)ap;

	return EIO;
}

int
dead_rename(void *v)
{
	struct vop_rename_args /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap = v;

	vrele(ap->a_fdvp);
	vrele(ap->a_fvp);
	if (ap->a_tvp != NULL && ap->a_tvp != ap->a_tdvp)
		VOP_UNLOCK(ap->a_tvp);
	vput(ap->a_tdvp);
	if (ap->a_tvp != NULL)
		vrele(ap->a_tvp);

	return EIO;
}

int
dead_rmdir(void *v)
{
	struct vop_rmdir_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;

	vput(ap->a_dvp);
	vput(ap->a_vp);

	return EIO;
}

int
dead_inactive(void *v)
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		bool *a_recycle;
	} */ *ap = v;

	*ap->a_recycle = false;
	VOP_UNLOCK(ap->a_vp);

	return 0;
}

int
dead_strategy(void *v)
{
	struct vop_strategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap = v;
	struct buf *bp;

	bp = ap->a_bp;
	bp->b_error = EIO;
	bp->b_resid = bp->b_bcount;
	biodone(ap->a_bp);
	return (EIO);
}

/* ARGSUSED */
int
dead_print(void *v)
{
	printf("tag VT_NON, dead vnode\n");
	return 0;
}

int
dead_getpages(void *v)
{
	struct vop_getpages_args /* {
		struct vnode *a_vp;
		voff_t a_offset;
		struct vm_page **a_m;
		int *a_count;
		int a_centeridx;
		vm_prot_t a_access_type;
		int a_advice;
		int a_flags;
	} */ *ap = v;

	if ((ap->a_flags & PGO_LOCKED) == 0)
		mutex_exit(ap->a_vp->v_interlock);

	return (EFAULT);
}

int
dead_putpages(void *v)
{
        struct vop_putpages_args /* {
		struct vnode *a_vp;
		voff_t a_offlo;
		voff_t a_offhi;
		int a_flags;
	} */ *ap = v;

	mutex_exit(ap->a_vp->v_interlock);
	return (EFAULT);
}

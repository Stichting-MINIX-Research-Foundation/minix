/*	$NetBSD: puffs_vnops.c,v 1.203 2015/04/20 23:03:08 riastradh Exp $	*/

/*
 * Copyright (c) 2005, 2006, 2007  Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Google Summer of Code program and the Ulla Tuominen Foundation.
 * The Google SoC project was mentored by Bill Studenmund.
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
__KERNEL_RCSID(0, "$NetBSD: puffs_vnops.c,v 1.203 2015/04/20 23:03:08 riastradh Exp $");

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/lockf.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/kernel.h> /* For hz, hardclock_ticks */

#include <uvm/uvm.h>

#include <fs/puffs/puffs_msgif.h>
#include <fs/puffs/puffs_sys.h>

#include <miscfs/fifofs/fifo.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>

int	puffs_vnop_lookup(void *);
int	puffs_vnop_create(void *);
int	puffs_vnop_access(void *);
int	puffs_vnop_mknod(void *);
int	puffs_vnop_open(void *);
int	puffs_vnop_close(void *);
int	puffs_vnop_getattr(void *);
int	puffs_vnop_setattr(void *);
int	puffs_vnop_reclaim(void *);
int	puffs_vnop_readdir(void *);
int	puffs_vnop_poll(void *);
int	puffs_vnop_fsync(void *);
int	puffs_vnop_seek(void *);
int	puffs_vnop_remove(void *);
int	puffs_vnop_mkdir(void *);
int	puffs_vnop_rmdir(void *);
int	puffs_vnop_link(void *);
int	puffs_vnop_readlink(void *);
int	puffs_vnop_symlink(void *);
int	puffs_vnop_rename(void *);
int	puffs_vnop_read(void *);
int	puffs_vnop_write(void *);
int	puffs_vnop_fallocate(void *);
int	puffs_vnop_fdiscard(void *);
int	puffs_vnop_fcntl(void *);
int	puffs_vnop_ioctl(void *);
int	puffs_vnop_inactive(void *);
int	puffs_vnop_print(void *);
int	puffs_vnop_pathconf(void *);
int	puffs_vnop_advlock(void *);
int	puffs_vnop_strategy(void *);
int	puffs_vnop_bmap(void *);
int	puffs_vnop_mmap(void *);
int	puffs_vnop_getpages(void *);
int	puffs_vnop_abortop(void *);
int	puffs_vnop_getextattr(void *);
int	puffs_vnop_setextattr(void *);
int	puffs_vnop_listextattr(void *);
int	puffs_vnop_deleteextattr(void *);

int	puffs_vnop_spec_read(void *);
int	puffs_vnop_spec_write(void *);
int	puffs_vnop_fifo_read(void *);
int	puffs_vnop_fifo_write(void *);

int	puffs_vnop_checkop(void *);

#define puffs_vnop_lock genfs_lock
#define puffs_vnop_unlock genfs_unlock
#define puffs_vnop_islocked genfs_islocked

int (**puffs_vnodeop_p)(void *);
const struct vnodeopv_entry_desc puffs_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, puffs_vnop_lookup },	/* REAL lookup */
	{ &vop_create_desc, puffs_vnop_checkop },	/* create */
        { &vop_mknod_desc, puffs_vnop_checkop },	/* mknod */
        { &vop_open_desc, puffs_vnop_open },		/* REAL open */
        { &vop_close_desc, puffs_vnop_checkop },	/* close */
        { &vop_access_desc, puffs_vnop_access },	/* REAL access */
        { &vop_getattr_desc, puffs_vnop_checkop },	/* getattr */
        { &vop_setattr_desc, puffs_vnop_checkop },	/* setattr */
        { &vop_read_desc, puffs_vnop_checkop },		/* read */
        { &vop_write_desc, puffs_vnop_checkop },	/* write */
	{ &vop_fallocate_desc, puffs_vnop_fallocate },	/* fallocate */
	{ &vop_fdiscard_desc, puffs_vnop_fdiscard },	/* fdiscard */
        { &vop_fsync_desc, puffs_vnop_fsync },		/* REAL fsync */
        { &vop_seek_desc, puffs_vnop_checkop },		/* seek */
        { &vop_remove_desc, puffs_vnop_checkop },	/* remove */
        { &vop_link_desc, puffs_vnop_checkop },		/* link */
        { &vop_rename_desc, puffs_vnop_checkop },	/* rename */
        { &vop_mkdir_desc, puffs_vnop_checkop },	/* mkdir */
        { &vop_rmdir_desc, puffs_vnop_checkop },	/* rmdir */
        { &vop_symlink_desc, puffs_vnop_checkop },	/* symlink */
        { &vop_readdir_desc, puffs_vnop_checkop },	/* readdir */
        { &vop_readlink_desc, puffs_vnop_checkop },	/* readlink */
        { &vop_getpages_desc, puffs_vnop_checkop },	/* getpages */
        { &vop_putpages_desc, genfs_putpages },		/* REAL putpages */
        { &vop_pathconf_desc, puffs_vnop_checkop },	/* pathconf */
        { &vop_advlock_desc, puffs_vnop_advlock },	/* advlock */
        { &vop_strategy_desc, puffs_vnop_strategy },	/* REAL strategy */
        { &vop_revoke_desc, genfs_revoke },		/* REAL revoke */
        { &vop_abortop_desc, puffs_vnop_abortop },	/* REAL abortop */
        { &vop_inactive_desc, puffs_vnop_inactive },	/* REAL inactive */
        { &vop_reclaim_desc, puffs_vnop_reclaim },	/* REAL reclaim */
        { &vop_lock_desc, puffs_vnop_lock },		/* REAL lock */
        { &vop_unlock_desc, puffs_vnop_unlock },	/* REAL unlock */
        { &vop_bmap_desc, puffs_vnop_bmap },		/* REAL bmap */
        { &vop_print_desc, puffs_vnop_print },		/* REAL print */
        { &vop_islocked_desc, puffs_vnop_islocked },	/* REAL islocked */
        { &vop_bwrite_desc, genfs_nullop },		/* REAL bwrite */
        { &vop_mmap_desc, puffs_vnop_mmap },		/* REAL mmap */
        { &vop_poll_desc, puffs_vnop_poll },		/* REAL poll */
	{ &vop_getextattr_desc, puffs_vnop_getextattr },	/* getextattr */
	{ &vop_setextattr_desc, puffs_vnop_setextattr },	/* setextattr */
	{ &vop_listextattr_desc, puffs_vnop_listextattr },	/* listextattr */
	{ &vop_deleteextattr_desc, puffs_vnop_deleteextattr },/* deleteextattr */
#if 0
	{ &vop_openextattr_desc, puffs_vnop_checkop },	/* openextattr */
	{ &vop_closeextattr_desc, puffs_vnop_checkop },	/* closeextattr */
#endif
        { &vop_kqfilter_desc, genfs_eopnotsupp },	/* kqfilter XXX */
	{ NULL, NULL }
};
const struct vnodeopv_desc puffs_vnodeop_opv_desc =
	{ &puffs_vnodeop_p, puffs_vnodeop_entries };


int (**puffs_specop_p)(void *);
const struct vnodeopv_entry_desc puffs_specop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, spec_lookup },		/* lookup, ENOTDIR */
	{ &vop_create_desc, spec_create },		/* genfs_badop */
	{ &vop_mknod_desc, spec_mknod },		/* genfs_badop */
	{ &vop_open_desc, spec_open },			/* spec_open */
	{ &vop_close_desc, spec_close },		/* spec_close */
	{ &vop_access_desc, puffs_vnop_checkop },	/* access */
	{ &vop_getattr_desc, puffs_vnop_checkop },	/* getattr */
	{ &vop_setattr_desc, puffs_vnop_checkop },	/* setattr */
	{ &vop_read_desc, puffs_vnop_spec_read },	/* update, read */
	{ &vop_write_desc, puffs_vnop_spec_write },	/* update, write */
	{ &vop_fallocate_desc, spec_fallocate },	/* fallocate */
	{ &vop_fdiscard_desc, spec_fdiscard },		/* fdiscard */
	{ &vop_ioctl_desc, spec_ioctl },		/* spec_ioctl */
	{ &vop_fcntl_desc, genfs_fcntl },		/* dummy */
	{ &vop_poll_desc, spec_poll },			/* spec_poll */
	{ &vop_kqfilter_desc, spec_kqfilter },		/* spec_kqfilter */
	{ &vop_revoke_desc, spec_revoke },		/* genfs_revoke */
	{ &vop_mmap_desc, spec_mmap },			/* spec_mmap */
	{ &vop_fsync_desc, spec_fsync },		/* vflushbuf */
	{ &vop_seek_desc, spec_seek },			/* genfs_nullop */
	{ &vop_remove_desc, spec_remove },		/* genfs_badop */
	{ &vop_link_desc, spec_link },			/* genfs_badop */
	{ &vop_rename_desc, spec_rename },		/* genfs_badop */
	{ &vop_mkdir_desc, spec_mkdir },		/* genfs_badop */
	{ &vop_rmdir_desc, spec_rmdir },		/* genfs_badop */
	{ &vop_symlink_desc, spec_symlink },		/* genfs_badop */
	{ &vop_readdir_desc, spec_readdir },		/* genfs_badop */
	{ &vop_readlink_desc, spec_readlink },		/* genfs_badop */
	{ &vop_abortop_desc, spec_abortop },		/* genfs_badop */
	{ &vop_inactive_desc, puffs_vnop_inactive },	/* REAL inactive */
	{ &vop_reclaim_desc, puffs_vnop_reclaim },	/* REAL reclaim */
	{ &vop_lock_desc, puffs_vnop_lock },		/* REAL lock */
	{ &vop_unlock_desc, puffs_vnop_unlock },	/* REAL unlock */
	{ &vop_bmap_desc, spec_bmap },			/* dummy */
	{ &vop_strategy_desc, spec_strategy },		/* dev strategy */
	{ &vop_print_desc, puffs_vnop_print },		/* REAL print */
	{ &vop_islocked_desc, puffs_vnop_islocked },	/* REAL islocked */
	{ &vop_pathconf_desc, spec_pathconf },		/* pathconf */
	{ &vop_advlock_desc, spec_advlock },		/* lf_advlock */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ &vop_getpages_desc, spec_getpages },		/* genfs_getpages */
	{ &vop_putpages_desc, spec_putpages },		/* genfs_putpages */
	{ &vop_getextattr_desc, puffs_vnop_checkop },	/* getextattr */
	{ &vop_setextattr_desc, puffs_vnop_checkop },	/* setextattr */
	{ &vop_listextattr_desc, puffs_vnop_checkop },	/* listextattr */
	{ &vop_deleteextattr_desc, puffs_vnop_checkop },/* deleteextattr */
#if 0
	{ &vop_openextattr_desc, _openextattr },	/* openextattr */
	{ &vop_closeextattr_desc, _closeextattr },	/* closeextattr */
#endif
	{ NULL, NULL }
};
const struct vnodeopv_desc puffs_specop_opv_desc =
	{ &puffs_specop_p, puffs_specop_entries };


int (**puffs_fifoop_p)(void *);
const struct vnodeopv_entry_desc puffs_fifoop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, vn_fifo_bypass },		/* lookup, ENOTDIR */
	{ &vop_create_desc, vn_fifo_bypass },		/* genfs_badop */
	{ &vop_mknod_desc, vn_fifo_bypass },		/* genfs_badop */
	{ &vop_open_desc, vn_fifo_bypass },		/* open */
	{ &vop_close_desc, vn_fifo_bypass },		/* close */
	{ &vop_access_desc, puffs_vnop_checkop },	/* access */
	{ &vop_getattr_desc, puffs_vnop_checkop },	/* getattr */
	{ &vop_setattr_desc, puffs_vnop_checkop },	/* setattr */
	{ &vop_read_desc, puffs_vnop_fifo_read },	/* read, update */
	{ &vop_write_desc, puffs_vnop_fifo_write },	/* write, update */
	{ &vop_fallocate_desc, vn_fifo_bypass },	/* fallocate */
	{ &vop_fdiscard_desc, vn_fifo_bypass },		/* fdiscard */
	{ &vop_ioctl_desc, vn_fifo_bypass },		/* ioctl */
	{ &vop_fcntl_desc, genfs_fcntl },		/* dummy */
	{ &vop_poll_desc, vn_fifo_bypass },		/* poll */
	{ &vop_kqfilter_desc, vn_fifo_bypass },		/* kqfilter */
	{ &vop_revoke_desc, vn_fifo_bypass },		/* genfs_revoke */
	{ &vop_mmap_desc, vn_fifo_bypass },		/* genfs_badop */
	{ &vop_fsync_desc, vn_fifo_bypass },		/* genfs_nullop*/
	{ &vop_seek_desc, vn_fifo_bypass },		/* genfs_badop */
	{ &vop_remove_desc, vn_fifo_bypass },		/* genfs_badop */
	{ &vop_link_desc, vn_fifo_bypass },		/* genfs_badop */
	{ &vop_rename_desc, vn_fifo_bypass },		/* genfs_badop */
	{ &vop_mkdir_desc, vn_fifo_bypass },		/* genfs_badop */
	{ &vop_rmdir_desc, vn_fifo_bypass },		/* genfs_badop */
	{ &vop_symlink_desc, vn_fifo_bypass },		/* genfs_badop */
	{ &vop_readdir_desc, vn_fifo_bypass },		/* genfs_badop */
	{ &vop_readlink_desc, vn_fifo_bypass },		/* genfs_badop */
	{ &vop_abortop_desc, vn_fifo_bypass },		/* genfs_badop */
	{ &vop_inactive_desc, puffs_vnop_inactive },	/* REAL inactive */
	{ &vop_reclaim_desc, puffs_vnop_reclaim },	/* REAL reclaim */
	{ &vop_lock_desc, puffs_vnop_lock },		/* REAL lock */
	{ &vop_unlock_desc, puffs_vnop_unlock },	/* REAL unlock */
	{ &vop_bmap_desc, vn_fifo_bypass },		/* dummy */
	{ &vop_strategy_desc, vn_fifo_bypass },		/* genfs_badop */
	{ &vop_print_desc, puffs_vnop_print },		/* REAL print */
	{ &vop_islocked_desc, puffs_vnop_islocked },	/* REAL islocked */
	{ &vop_pathconf_desc, vn_fifo_bypass },		/* pathconf */
	{ &vop_advlock_desc, vn_fifo_bypass },		/* genfs_einval */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ &vop_putpages_desc, vn_fifo_bypass }, 	/* genfs_null_putpages*/
#if 0
	{ &vop_openextattr_desc, _openextattr },	/* openextattr */
	{ &vop_closeextattr_desc, _closeextattr },	/* closeextattr */
#endif
	{ &vop_getextattr_desc, puffs_vnop_checkop },		/* getextattr */
	{ &vop_setextattr_desc, puffs_vnop_checkop },		/* setextattr */
	{ &vop_listextattr_desc, puffs_vnop_checkop },	/* listextattr */
	{ &vop_deleteextattr_desc, puffs_vnop_checkop },	/* deleteextattr */
	{ NULL, NULL }
};
const struct vnodeopv_desc puffs_fifoop_opv_desc =
	{ &puffs_fifoop_p, puffs_fifoop_entries };


/* "real" vnode operations */
int (**puffs_msgop_p)(void *);
const struct vnodeopv_entry_desc puffs_msgop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_create_desc, puffs_vnop_create },	/* create */
        { &vop_mknod_desc, puffs_vnop_mknod },		/* mknod */
        { &vop_open_desc, puffs_vnop_open },		/* open */
        { &vop_close_desc, puffs_vnop_close },		/* close */
        { &vop_access_desc, puffs_vnop_access },	/* access */
        { &vop_getattr_desc, puffs_vnop_getattr },	/* getattr */
        { &vop_setattr_desc, puffs_vnop_setattr },	/* setattr */
        { &vop_read_desc, puffs_vnop_read },		/* read */
        { &vop_write_desc, puffs_vnop_write },		/* write */
        { &vop_seek_desc, puffs_vnop_seek },		/* seek */
        { &vop_remove_desc, puffs_vnop_remove },	/* remove */
        { &vop_link_desc, puffs_vnop_link },		/* link */
        { &vop_rename_desc, puffs_vnop_rename },	/* rename */
        { &vop_mkdir_desc, puffs_vnop_mkdir },		/* mkdir */
        { &vop_rmdir_desc, puffs_vnop_rmdir },		/* rmdir */
        { &vop_symlink_desc, puffs_vnop_symlink },	/* symlink */
        { &vop_readdir_desc, puffs_vnop_readdir },	/* readdir */
        { &vop_readlink_desc, puffs_vnop_readlink },	/* readlink */
        { &vop_print_desc, puffs_vnop_print },		/* print */
        { &vop_islocked_desc, puffs_vnop_islocked },	/* islocked */
        { &vop_pathconf_desc, puffs_vnop_pathconf },	/* pathconf */
        { &vop_getpages_desc, puffs_vnop_getpages },	/* getpages */
	{ NULL, NULL }
};
const struct vnodeopv_desc puffs_msgop_opv_desc =
	{ &puffs_msgop_p, puffs_msgop_entries };

/*
 * for dosetattr / update_va 
 */
#define SETATTR_CHSIZE	0x01
#define SETATTR_ASYNC	0x02

#define ERROUT(err)							\
do {									\
	error = err;							\
	goto out;							\
} while (/*CONSTCOND*/0)

/*
 * This is a generic vnode operation handler.  It checks if the necessary
 * operations for the called vnode operation are implemented by userspace
 * and either returns a dummy return value or proceeds to call the real
 * vnode operation from puffs_msgop_v.
 *
 * XXX: this should described elsewhere and autogenerated, the complexity
 * of the vnode operations vectors and their interrelationships is also
 * getting a bit out of hand.  Another problem is that we need this same
 * information in the fs server code, so keeping the two in sync manually
 * is not a viable (long term) plan.
 */

/* not supported, handle locking protocol */
#define CHECKOP_NOTSUPP(op)						\
case VOP_##op##_DESCOFFSET:						\
	if (pmp->pmp_vnopmask[PUFFS_VN_##op] == 0)			\
		return genfs_eopnotsupp(v);				\
	break

/* always succeed, no locking */
#define CHECKOP_SUCCESS(op)						\
case VOP_##op##_DESCOFFSET:						\
	if (pmp->pmp_vnopmask[PUFFS_VN_##op] == 0)			\
		return 0;						\
	break

int
puffs_vnop_checkop(void *v)
{
	struct vop_generic_args /* {
		struct vnodeop_desc *a_desc;
		spooky mystery contents;
	} */ *ap = v;
	struct vnodeop_desc *desc = ap->a_desc;
	struct puffs_mount *pmp;
	struct vnode *vp;
	int offset, rv;

	offset = ap->a_desc->vdesc_vp_offsets[0];
#ifdef DIAGNOSTIC
	if (offset == VDESC_NO_OFFSET)
		panic("puffs_checkop: no vnode, why did you call me?");
#endif
	vp = *VOPARG_OFFSETTO(struct vnode **, offset, ap);
	pmp = MPTOPUFFSMP(vp->v_mount);

	DPRINTF_VERBOSE(("checkop call %s (%d), vp %p\n",
	    ap->a_desc->vdesc_name, ap->a_desc->vdesc_offset, vp));

	if (!ALLOPS(pmp)) {
		switch (desc->vdesc_offset) {
			CHECKOP_NOTSUPP(CREATE);
			CHECKOP_NOTSUPP(MKNOD);
			CHECKOP_NOTSUPP(GETATTR);
			CHECKOP_NOTSUPP(SETATTR);
			CHECKOP_NOTSUPP(READ);
			CHECKOP_NOTSUPP(WRITE);
			CHECKOP_NOTSUPP(FCNTL);
			CHECKOP_NOTSUPP(IOCTL);
			CHECKOP_NOTSUPP(REMOVE);
			CHECKOP_NOTSUPP(LINK);
			CHECKOP_NOTSUPP(RENAME);
			CHECKOP_NOTSUPP(MKDIR);
			CHECKOP_NOTSUPP(RMDIR);
			CHECKOP_NOTSUPP(SYMLINK);
			CHECKOP_NOTSUPP(READDIR);
			CHECKOP_NOTSUPP(READLINK);
			CHECKOP_NOTSUPP(PRINT);
			CHECKOP_NOTSUPP(PATHCONF);
			CHECKOP_NOTSUPP(GETEXTATTR);
			CHECKOP_NOTSUPP(SETEXTATTR);
			CHECKOP_NOTSUPP(LISTEXTATTR);
			CHECKOP_NOTSUPP(DELETEEXTATTR);

			CHECKOP_SUCCESS(ACCESS);
			CHECKOP_SUCCESS(CLOSE);
			CHECKOP_SUCCESS(SEEK);

		case VOP_GETPAGES_DESCOFFSET:
			if (!EXISTSOP(pmp, READ))
				return genfs_eopnotsupp(v);
			break;

		default:
			panic("puffs_checkop: unhandled vnop %d",
			    desc->vdesc_offset);
		}
	}

	rv = VOCALL(puffs_msgop_p, ap->a_desc->vdesc_offset, v);

	DPRINTF_VERBOSE(("checkop return %s (%d), vp %p: %d\n",
	    ap->a_desc->vdesc_name, ap->a_desc->vdesc_offset, vp, rv));

	return rv;
}

static int callremove(struct puffs_mount *, puffs_cookie_t, puffs_cookie_t,
			    struct componentname *);
static int callrmdir(struct puffs_mount *, puffs_cookie_t, puffs_cookie_t,
			   struct componentname *);
static void callinactive(struct puffs_mount *, puffs_cookie_t, int);
static void callreclaim(struct puffs_mount *, puffs_cookie_t, int);
static int  flushvncache(struct vnode *, off_t, off_t, bool);
static void update_va(struct vnode *, struct vattr *, struct vattr *,
		      struct timespec *, struct timespec *, int);
static void update_parent(struct vnode *, struct vnode *);


#define PUFFS_ABORT_LOOKUP	1
#define PUFFS_ABORT_CREATE	2
#define PUFFS_ABORT_MKNOD	3
#define PUFFS_ABORT_MKDIR	4
#define PUFFS_ABORT_SYMLINK	5

/*
 * Press the pani^Wabort button!  Kernel resource allocation failed.
 */
static void
puffs_abortbutton(struct puffs_mount *pmp, int what,
	puffs_cookie_t dck, puffs_cookie_t ck, struct componentname *cnp)
{

	switch (what) {
	case PUFFS_ABORT_CREATE:
	case PUFFS_ABORT_MKNOD:
	case PUFFS_ABORT_SYMLINK:
		callremove(pmp, dck, ck, cnp);
		break;
	case PUFFS_ABORT_MKDIR:
		callrmdir(pmp, dck, ck, cnp);
		break;
	}

	callinactive(pmp, ck, 0);
	callreclaim(pmp, ck, 1);
}

/*
 * Begin vnode operations.
 *
 * A word from the keymaster about locks: generally we don't want
 * to use the vnode locks at all: it creates an ugly dependency between
 * the userlandia file server and the kernel.  But we'll play along with
 * the kernel vnode locks for now.  However, even currently we attempt
 * to release locks as early as possible.  This is possible for some
 * operations which a) don't need a locked vnode after the userspace op
 * and b) return with the vnode unlocked.  Theoretically we could
 * unlock-do op-lock for others and order the graph in userspace, but I
 * don't want to think of the consequences for the time being.
 */

#define TTL_TO_TIMEOUT(ts) \
    (hardclock_ticks + (ts->tv_sec * hz) + (ts->tv_nsec * hz / 1000000000))
#define TTL_VALID(ts) \
    ((ts != NULL) && !((ts->tv_sec == 0) && (ts->tv_nsec == 0)))
#define TIMED_OUT(expire) \
    ((int)((unsigned int)hardclock_ticks - (unsigned int)expire) > 0)
int
puffs_vnop_lookup(void *v)
{
        struct vop_lookup_v2_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
        } */ *ap = v;
	PUFFS_MSG_VARS(vn, lookup);
	struct puffs_mount *pmp;
	struct componentname *cnp;
	struct vnode *vp, *dvp, *cvp;
	struct puffs_node *dpn, *cpn;
	int isdot;
	int error;

	pmp = MPTOPUFFSMP(ap->a_dvp->v_mount);
	cnp = ap->a_cnp;
	dvp = ap->a_dvp;
	cvp = NULL;
	cpn = NULL;
	*ap->a_vpp = NULL;

	/* r/o fs?  we check create later to handle EEXIST */
	if ((cnp->cn_flags & ISLASTCN)
	    && (dvp->v_mount->mnt_flag & MNT_RDONLY)
	    && (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return EROFS;

	isdot = cnp->cn_namelen == 1 && *cnp->cn_nameptr == '.';

	DPRINTF(("puffs_lookup: \"%s\", parent vnode %p, op: %x\n",
	    cnp->cn_nameptr, dvp, cnp->cn_nameiop));

	/*
	 * If dotdot cache is enabled, add reference to .. and return.
	 */
	if (PUFFS_USE_DOTDOTCACHE(pmp) && (cnp->cn_flags & ISDOTDOT)) {
		vp = VPTOPP(ap->a_dvp)->pn_parent;
		vref(vp);

		*ap->a_vpp = vp;
		return 0;
	}

	/*
	 * Check if someone fed it into the cache
	 */
	if (!isdot && PUFFS_USE_NAMECACHE(pmp)) {
		int found, iswhiteout;

		found = cache_lookup(dvp, cnp->cn_nameptr, cnp->cn_namelen,
				     cnp->cn_nameiop, cnp->cn_flags,
				     &iswhiteout, ap->a_vpp);
		if (iswhiteout) {
			cnp->cn_flags |= ISWHITEOUT;
		}

		if (found && *ap->a_vpp != NULLVP && PUFFS_USE_FS_TTL(pmp)) {
			cvp = *ap->a_vpp;
			cpn = VPTOPP(cvp);

			if (TIMED_OUT(cpn->pn_cn_timeout)) {
				cache_purge(cvp);
				/*
				 * cached vnode (cvp) is still referenced
				 * so that we can reuse it upon a new
				 * successful lookup. 
				 */
				*ap->a_vpp = NULL;
				found = 0;
			}
		}

		/*
		 * Do not use negative caching, since the filesystem
		 * provides no TTL for it.
		 */
		if (found && *ap->a_vpp == NULLVP && PUFFS_USE_FS_TTL(pmp))
			found = 0;

		if (found) {
			return *ap->a_vpp == NULLVP ? ENOENT : 0;
		}

		/*
		 * This is what would have been left in ERROR before
		 * the rearrangement of cache_lookup(). What with all
		 * the macros, I am not sure if this is a dead value
		 * below or not.
		 */
		error = -1;
	}

	if (isdot) {
		/* deal with rename lookup semantics */
		if (cnp->cn_nameiop == RENAME && (cnp->cn_flags & ISLASTCN))
			return EISDIR;

		vp = ap->a_dvp;
		vref(vp);
		*ap->a_vpp = vp;
		return 0;
	}

	if (cvp != NULL) {
		if (vn_lock(cvp, LK_EXCLUSIVE) != 0) {
			vrele(cvp);
			cvp = NULL;
		} else
			mutex_enter(&cpn->pn_sizemtx);
	}

	PUFFS_MSG_ALLOC(vn, lookup);
	puffs_makecn(&lookup_msg->pvnr_cn, &lookup_msg->pvnr_cn_cred,
	    cnp, PUFFS_USE_FULLPNBUF(pmp));

	if (cnp->cn_flags & ISDOTDOT)
		VOP_UNLOCK(dvp);

	puffs_msg_setinfo(park_lookup, PUFFSOP_VN,
	    PUFFS_VN_LOOKUP, VPTOPNC(dvp));
	PUFFS_MSG_ENQUEUEWAIT2(pmp, park_lookup, dvp->v_data, NULL, error);
	DPRINTF(("puffs_lookup: return of the userspace, part %d\n", error));

	/*
	 * In case of error, there is no new vnode to play with, so be
	 * happy with the NULL value given to vpp in the beginning.
	 * Also, check if this really was an error or the target was not
	 * present.  Either treat it as a non-error for CREATE/RENAME or
	 * enter the component into the negative name cache (if desired).
	 */
	if (error) {
		error = checkerr(pmp, error, __func__);
		if (error == ENOENT) {
			/* don't allow to create files on r/o fs */
			if ((dvp->v_mount->mnt_flag & MNT_RDONLY)
			    && cnp->cn_nameiop == CREATE) {
				error = EROFS;

			/* adjust values if we are creating */
			} else if ((cnp->cn_flags & ISLASTCN)
			    && (cnp->cn_nameiop == CREATE
			      || cnp->cn_nameiop == RENAME)) {
				error = EJUSTRETURN;

			/* save negative cache entry */
			} else {
				if (PUFFS_USE_NAMECACHE(pmp) &&
				    !PUFFS_USE_FS_TTL(pmp))
					cache_enter(dvp, NULL, cnp->cn_nameptr,
						cnp->cn_namelen, cnp->cn_flags);
			}
		}
		goto out;
	}

	/*
	 * Check that we don't get our parent node back, that would cause
	 * a pretty obvious deadlock.
	 */
	dpn = dvp->v_data;
	if (lookup_msg->pvnr_newnode == dpn->pn_cookie) {
		puffs_senderr(pmp, PUFFS_ERR_LOOKUP, EINVAL,
		    "lookup produced parent cookie", lookup_msg->pvnr_newnode);
		error = EPROTO;
		goto out;
	}

	/*
	 * Check if we looked up the cached vnode
	 */
	vp = NULL;
	if (cvp && (VPTOPP(cvp)->pn_cookie == lookup_msg->pvnr_newnode)) {
		int grace;

		/*
		 * Bump grace time of this node so that it does not get 
		 * reclaimed too fast. We try to increase a bit more the
		 * lifetime of busiest * nodes - with some limits.
		 */
		grace = 10 * puffs_sopreq_expire_timeout;
		cpn->pn_cn_grace = hardclock_ticks + grace;
		vp = cvp;
	}

	/*
	 * No cached vnode available, or the cached vnode does not
	 * match the userland cookie anymore: is the node known?
	 */
	if (vp == NULL) {
		error = puffs_getvnode(dvp->v_mount,
		    lookup_msg->pvnr_newnode, lookup_msg->pvnr_vtype,
		    lookup_msg->pvnr_size, lookup_msg->pvnr_rdev, &vp);
		if (error) {
			puffs_abortbutton(pmp, PUFFS_ABORT_LOOKUP,
			    VPTOPNC(dvp), lookup_msg->pvnr_newnode,
			    ap->a_cnp);
			goto out;
		}

		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	}

	/*
	 * Update cache and TTL
	 */
	if (PUFFS_USE_FS_TTL(pmp)) {
		struct timespec *va_ttl = &lookup_msg->pvnr_va_ttl;
		struct timespec *cn_ttl = &lookup_msg->pvnr_cn_ttl;
		update_va(vp, NULL, &lookup_msg->pvnr_va, 
			  va_ttl, cn_ttl, SETATTR_CHSIZE);
	}

	KASSERT(lookup_msg->pvnr_newnode == VPTOPP(vp)->pn_cookie);
	*ap->a_vpp = vp;

	if (PUFFS_USE_NAMECACHE(pmp))
		cache_enter(dvp, vp, cnp->cn_nameptr, cnp->cn_namelen,
			    cnp->cn_flags);

	/* XXX */
	if ((lookup_msg->pvnr_cn.pkcn_flags & REQUIREDIR) == 0)
		cnp->cn_flags &= ~REQUIREDIR;
	if (lookup_msg->pvnr_cn.pkcn_consume)
		cnp->cn_consume = MIN(lookup_msg->pvnr_cn.pkcn_consume,
		    strlen(cnp->cn_nameptr) - cnp->cn_namelen);

	VPTOPP(vp)->pn_nlookup++;

	if (PUFFS_USE_DOTDOTCACHE(pmp) &&
	    (VPTOPP(vp)->pn_parent != dvp))
		update_parent(vp, dvp);

 out:
	if (cvp != NULL) {
		mutex_exit(&cpn->pn_sizemtx);

		if (error || (cvp != vp))
			vput(cvp);
	}
	if (error == 0)
		VOP_UNLOCK(*ap->a_vpp);

	if (cnp->cn_flags & ISDOTDOT)
		vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);

	DPRINTF(("puffs_lookup: returning %d %p\n", error, *ap->a_vpp));
	PUFFS_MSG_RELEASE(lookup);
	return error;
}

#define REFPN_AND_UNLOCKVP(a, b)					\
do {									\
	mutex_enter(&b->pn_mtx);					\
	puffs_referencenode(b);						\
	mutex_exit(&b->pn_mtx);						\
	VOP_UNLOCK(a);						\
} while (/*CONSTCOND*/0)

#define REFPN(b)							\
do {									\
	mutex_enter(&b->pn_mtx);					\
	puffs_referencenode(b);						\
	mutex_exit(&b->pn_mtx);						\
} while (/*CONSTCOND*/0)

#define RELEPN_AND_VP(a, b)						\
do {									\
	puffs_releasenode(b);						\
	vrele(a);							\
} while (/*CONSTCOND*/0)

int
puffs_vnop_create(void *v)
{
	struct vop_create_v3_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, create);
	struct vnode *dvp = ap->a_dvp;
	struct puffs_node *dpn = VPTOPP(dvp);
	struct componentname *cnp = ap->a_cnp;
	struct mount *mp = dvp->v_mount;
	struct puffs_mount *pmp = MPTOPUFFSMP(mp);
	int error;

	DPRINTF(("puffs_create: dvp %p, cnp: %s\n",
	    dvp, ap->a_cnp->cn_nameptr));

	PUFFS_MSG_ALLOC(vn, create);
	puffs_makecn(&create_msg->pvnr_cn, &create_msg->pvnr_cn_cred,
	    cnp, PUFFS_USE_FULLPNBUF(pmp));
	create_msg->pvnr_va = *ap->a_vap;
	puffs_msg_setinfo(park_create, PUFFSOP_VN,
	    PUFFS_VN_CREATE, VPTOPNC(dvp));
	PUFFS_MSG_ENQUEUEWAIT2(pmp, park_create, dvp->v_data, NULL, error);

	error = checkerr(pmp, error, __func__);
	if (error)
		goto out;

	error = puffs_newnode(mp, dvp, ap->a_vpp,
	    create_msg->pvnr_newnode, cnp, ap->a_vap->va_type, 0);
	if (error) {
		puffs_abortbutton(pmp, PUFFS_ABORT_CREATE, dpn->pn_cookie,
		    create_msg->pvnr_newnode, cnp);
		goto out;
	}

	if (PUFFS_USE_FS_TTL(pmp)) {
		struct timespec *va_ttl = &create_msg->pvnr_va_ttl;
		struct timespec *cn_ttl = &create_msg->pvnr_cn_ttl;
		struct vattr *rvap = &create_msg->pvnr_va;

		update_va(*ap->a_vpp, NULL, rvap, 
			  va_ttl, cn_ttl, SETATTR_CHSIZE);
	}

	VPTOPP(*ap->a_vpp)->pn_nlookup++;

	if (PUFFS_USE_DOTDOTCACHE(pmp) &&
	    (VPTOPP(*ap->a_vpp)->pn_parent != dvp))
		update_parent(*ap->a_vpp, dvp);

 out:
	DPRINTF(("puffs_create: return %d\n", error));
	PUFFS_MSG_RELEASE(create);
	return error;
}

int
puffs_vnop_mknod(void *v)
{
	struct vop_mknod_v3_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, mknod);
	struct vnode *dvp = ap->a_dvp;
	struct puffs_node *dpn = VPTOPP(dvp);
	struct componentname *cnp = ap->a_cnp;
	struct mount *mp = dvp->v_mount;
	struct puffs_mount *pmp = MPTOPUFFSMP(mp);
	int error;

	PUFFS_MSG_ALLOC(vn, mknod);
	puffs_makecn(&mknod_msg->pvnr_cn, &mknod_msg->pvnr_cn_cred,
	    cnp, PUFFS_USE_FULLPNBUF(pmp));
	mknod_msg->pvnr_va = *ap->a_vap;
	puffs_msg_setinfo(park_mknod, PUFFSOP_VN,
	    PUFFS_VN_MKNOD, VPTOPNC(dvp));

	PUFFS_MSG_ENQUEUEWAIT2(pmp, park_mknod, dvp->v_data, NULL, error);

	error = checkerr(pmp, error, __func__);
	if (error)
		goto out;

	error = puffs_newnode(mp, dvp, ap->a_vpp,
	    mknod_msg->pvnr_newnode, cnp, ap->a_vap->va_type,
	    ap->a_vap->va_rdev);
	if (error) {
		puffs_abortbutton(pmp, PUFFS_ABORT_MKNOD, dpn->pn_cookie,
		    mknod_msg->pvnr_newnode, cnp);
		goto out;
	}

	if (PUFFS_USE_FS_TTL(pmp)) {
		struct timespec *va_ttl = &mknod_msg->pvnr_va_ttl;
		struct timespec *cn_ttl = &mknod_msg->pvnr_cn_ttl;
		struct vattr *rvap = &mknod_msg->pvnr_va;

		update_va(*ap->a_vpp, NULL, rvap, 
			   va_ttl, cn_ttl, SETATTR_CHSIZE);
	}

	VPTOPP(*ap->a_vpp)->pn_nlookup++;

	if (PUFFS_USE_DOTDOTCACHE(pmp) &&
	    (VPTOPP(*ap->a_vpp)->pn_parent != dvp))
		update_parent(*ap->a_vpp, dvp);

 out:
	PUFFS_MSG_RELEASE(mknod);
	return error;
}

int
puffs_vnop_open(void *v)
{
	struct vop_open_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int a_mode;
		kauth_cred_t a_cred;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, open);
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	struct puffs_node *pn = VPTOPP(vp);
	int mode = ap->a_mode;
	int error;

	DPRINTF(("puffs_open: vp %p, mode 0x%x\n", vp, mode));

	if (vp->v_type == VREG && mode & FWRITE && !EXISTSOP(pmp, WRITE))
		ERROUT(EROFS);

	if (!EXISTSOP(pmp, OPEN))
		ERROUT(0);

	PUFFS_MSG_ALLOC(vn, open);
	open_msg->pvnr_mode = mode;
	puffs_credcvt(&open_msg->pvnr_cred, ap->a_cred);
	puffs_msg_setinfo(park_open, PUFFSOP_VN,
	    PUFFS_VN_OPEN, VPTOPNC(vp));

	PUFFS_MSG_ENQUEUEWAIT2(pmp, park_open, vp->v_data, NULL, error);
	error = checkerr(pmp, error, __func__);

	if (open_msg->pvnr_oflags & PUFFS_OPEN_IO_DIRECT) {
		/*
		 * Flush cache:
		 * - we do not want to discard cached write by direct write
		 * - read cache is now useless and should be freed
		 */
		flushvncache(vp, 0, 0, true);
		if (mode & FREAD)
			pn->pn_stat |= PNODE_RDIRECT;
		if (mode & FWRITE)
			pn->pn_stat |= PNODE_WDIRECT;
	}
 out:
	DPRINTF(("puffs_open: returning %d\n", error));
	PUFFS_MSG_RELEASE(open);
	return error;
}

int
puffs_vnop_close(void *v)
{
	struct vop_close_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int a_fflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, close);
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);

	PUFFS_MSG_ALLOC(vn, close);
	puffs_msg_setfaf(park_close);
	close_msg->pvnr_fflag = ap->a_fflag;
	puffs_credcvt(&close_msg->pvnr_cred, ap->a_cred);
	puffs_msg_setinfo(park_close, PUFFSOP_VN,
	    PUFFS_VN_CLOSE, VPTOPNC(vp));

	puffs_msg_enqueue(pmp, park_close);
	PUFFS_MSG_RELEASE(close);
	return 0;
}

int
puffs_vnop_access(void *v)
{
	struct vop_access_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int a_mode;
		kauth_cred_t a_cred;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, access);
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	int mode = ap->a_mode;
	int error;

	if (mode & VWRITE) {
		switch (vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			if ((vp->v_mount->mnt_flag & MNT_RDONLY)
			    || !EXISTSOP(pmp, WRITE))
				return EROFS;
			break;
		default:
			break;
		}
	}

	if (!EXISTSOP(pmp, ACCESS))
		return 0;

	PUFFS_MSG_ALLOC(vn, access);
	access_msg->pvnr_mode = ap->a_mode;
	puffs_credcvt(&access_msg->pvnr_cred, ap->a_cred);
	puffs_msg_setinfo(park_access, PUFFSOP_VN,
	    PUFFS_VN_ACCESS, VPTOPNC(vp));

	PUFFS_MSG_ENQUEUEWAIT2(pmp, park_access, vp->v_data, NULL, error);
	error = checkerr(pmp, error, __func__);
	PUFFS_MSG_RELEASE(access);

	return error;
}

static void
update_va(struct vnode *vp, struct vattr *vap, struct vattr *rvap,
	  struct timespec *va_ttl, struct timespec *cn_ttl, int flags)
{
	struct puffs_node *pn = VPTOPP(vp);
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	int use_metacache;

	if (TTL_VALID(cn_ttl)) {
		pn->pn_cn_timeout = TTL_TO_TIMEOUT(cn_ttl);
		pn->pn_cn_grace = MAX(pn->pn_cn_timeout, pn->pn_cn_grace);
	}

	/*
	 * Don't listen to the file server regarding special device
	 * size info, the file server doesn't know anything about them.
	 */
	if (vp->v_type == VBLK || vp->v_type == VCHR)
		rvap->va_size = vp->v_size;

	/* Ditto for blocksize (ufs comment: this doesn't belong here) */
	if (vp->v_type == VBLK)
		rvap->va_blocksize = BLKDEV_IOSIZE;
	else if (vp->v_type == VCHR)
		rvap->va_blocksize = MAXBSIZE;

	if (vap != NULL) {
		(void) memcpy(vap, rvap, sizeof(struct vattr));
		vap->va_fsid = vp->v_mount->mnt_stat.f_fsidx.__fsid_val[0];

		if (PUFFS_USE_METAFLUSH(pmp)) {
			if (pn->pn_stat & PNODE_METACACHE_ATIME)
				vap->va_atime = pn->pn_mc_atime;
			if (pn->pn_stat & PNODE_METACACHE_CTIME)
				vap->va_ctime = pn->pn_mc_ctime;
			if (pn->pn_stat & PNODE_METACACHE_MTIME)
				vap->va_mtime = pn->pn_mc_mtime;
			if (pn->pn_stat & PNODE_METACACHE_SIZE)
				vap->va_size = pn->pn_mc_size;
		}
	}

	use_metacache = PUFFS_USE_METAFLUSH(pmp) &&
			(pn->pn_stat & PNODE_METACACHE_SIZE);
	if (!use_metacache && (flags & SETATTR_CHSIZE)) {
		if (rvap->va_size != VNOVAL
		    && vp->v_type != VBLK && vp->v_type != VCHR) {
			uvm_vnp_setsize(vp, rvap->va_size);
			pn->pn_serversize = rvap->va_size;
		}
	}

	if ((va_ttl != NULL) && TTL_VALID(va_ttl)) {
		if (pn->pn_va_cache == NULL)
			pn->pn_va_cache = pool_get(&puffs_vapool, PR_WAITOK);

		(void)memcpy(pn->pn_va_cache, rvap, sizeof(*rvap));

		pn->pn_va_timeout = TTL_TO_TIMEOUT(va_ttl);
	}
}

static void 
update_parent(struct vnode *vp, struct vnode *dvp)
{
	struct puffs_node *pn = VPTOPP(vp);

	if (pn->pn_parent != NULL) {
		KASSERT(pn->pn_parent != dvp);
		vrele(pn->pn_parent);
	}

	vref(dvp);
	pn->pn_parent = dvp;
}

int
puffs_vnop_getattr(void *v)
{
	struct vop_getattr_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, getattr);
	struct vnode *vp = ap->a_vp;
	struct mount *mp = vp->v_mount;
	struct puffs_mount *pmp = MPTOPUFFSMP(mp);
	struct vattr *vap, *rvap;
	struct puffs_node *pn = VPTOPP(vp);
	struct timespec *va_ttl = NULL;
	int error = 0;

	/*
	 * A lock is required so that we do not race with 
	 * setattr, write and fsync when changing vp->v_size.
	 * This is critical, since setting a stall smaler value
	 * triggers a file truncate in uvm_vnp_setsize(), which
	 * most of the time means data corruption (a chunk of
	 * data is replaced by zeroes). This can be removed if
	 * we decide one day that VOP_GETATTR must operate on 
	 * a locked vnode.
	 *
	 * XXX Should be useless now that VOP_GETATTR has been
	 *     fixed to always require a shared lock at least.
	 */
	mutex_enter(&pn->pn_sizemtx);

	REFPN(pn);
	vap = ap->a_vap;

	if (PUFFS_USE_FS_TTL(pmp)) {
		if (!TIMED_OUT(pn->pn_va_timeout)) {
			update_va(vp, vap, pn->pn_va_cache, 
				  NULL, NULL, SETATTR_CHSIZE);
			goto out2;
		}
	}

	PUFFS_MSG_ALLOC(vn, getattr);
	vattr_null(&getattr_msg->pvnr_va);
	puffs_credcvt(&getattr_msg->pvnr_cred, ap->a_cred);
	puffs_msg_setinfo(park_getattr, PUFFSOP_VN,
	    PUFFS_VN_GETATTR, VPTOPNC(vp));

	PUFFS_MSG_ENQUEUEWAIT2(pmp, park_getattr, vp->v_data, NULL, error);
	error = checkerr(pmp, error, __func__);
	if (error)
		goto out;

	rvap = &getattr_msg->pvnr_va;

	if (PUFFS_USE_FS_TTL(pmp))
		va_ttl = &getattr_msg->pvnr_va_ttl;

	update_va(vp, vap, rvap, va_ttl, NULL, SETATTR_CHSIZE);

 out:
	PUFFS_MSG_RELEASE(getattr);

 out2:
	puffs_releasenode(pn);
	
	mutex_exit(&pn->pn_sizemtx);

	return error;
}

static void
zerofill_lastpage(struct vnode *vp, voff_t off)
{
	char zbuf[PAGE_SIZE];
	struct iovec iov;
	struct uio uio;
	vsize_t len;
	int error;

	if (trunc_page(off) == off)
		return;
 
	if (vp->v_writecount == 0)
		return;

	len = round_page(off) - off;
	memset(zbuf, 0, len);

	iov.iov_base = zbuf;
	iov.iov_len = len;
	UIO_SETUP_SYSSPACE(&uio);
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = off;
	uio.uio_resid = len;
	uio.uio_rw = UIO_WRITE;

	error = ubc_uiomove(&vp->v_uobj, &uio, len,
			    UVM_ADV_SEQUENTIAL, UBC_WRITE|UBC_UNMAP_FLAG(vp));
	if (error) {
		DPRINTF(("zero-fill 0x%" PRIxVSIZE "@0x%" PRIx64 
			 " failed: error = %d\n", len, off, error));
	}

	return;
}

static int
dosetattr(struct vnode *vp, struct vattr *vap, kauth_cred_t cred, int flags)
{
	PUFFS_MSG_VARS(vn, setattr);
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	struct puffs_node *pn = vp->v_data;
	vsize_t oldsize = vp->v_size;
	int error = 0;

	KASSERT(!(flags & SETATTR_CHSIZE) || mutex_owned(&pn->pn_sizemtx));

	if ((vp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (vap->va_uid != (uid_t)VNOVAL || vap->va_gid != (gid_t)VNOVAL
	    || vap->va_atime.tv_sec != VNOVAL || vap->va_mtime.tv_sec != VNOVAL
	    || vap->va_mode != (mode_t)VNOVAL))
		return EROFS;

	if ((vp->v_mount->mnt_flag & MNT_RDONLY)
	    && vp->v_type == VREG && vap->va_size != VNOVAL)
		return EROFS;

	/*
	 * Flush metacache first.  If we are called with some explicit
	 * parameters, treat them as information overriding metacache
	 * information.
	 */
	if (PUFFS_USE_METAFLUSH(pmp) && pn->pn_stat & PNODE_METACACHE_MASK) {
		if ((pn->pn_stat & PNODE_METACACHE_ATIME)
		    && vap->va_atime.tv_sec == VNOVAL)
			vap->va_atime = pn->pn_mc_atime;
		if ((pn->pn_stat & PNODE_METACACHE_CTIME)
		    && vap->va_ctime.tv_sec == VNOVAL)
			vap->va_ctime = pn->pn_mc_ctime;
		if ((pn->pn_stat & PNODE_METACACHE_MTIME)
		    && vap->va_mtime.tv_sec == VNOVAL)
			vap->va_mtime = pn->pn_mc_mtime;
		if ((pn->pn_stat & PNODE_METACACHE_SIZE)
		    && vap->va_size == VNOVAL)
			vap->va_size = pn->pn_mc_size;

		pn->pn_stat &= ~PNODE_METACACHE_MASK;
	}

	/*
	 * Flush attribute cache so that another thread do 
	 * not get a stale value during the operation.
	 */
	if (PUFFS_USE_FS_TTL(pmp))
		pn->pn_va_timeout = 0;

	PUFFS_MSG_ALLOC(vn, setattr);
	(void)memcpy(&setattr_msg->pvnr_va, vap, sizeof(struct vattr));
	puffs_credcvt(&setattr_msg->pvnr_cred, cred);
	puffs_msg_setinfo(park_setattr, PUFFSOP_VN,
	    PUFFS_VN_SETATTR, VPTOPNC(vp));
	if (flags & SETATTR_ASYNC)
		puffs_msg_setfaf(park_setattr);

	puffs_msg_enqueue(pmp, park_setattr);
	if ((flags & SETATTR_ASYNC) == 0) {
		error = puffs_msg_wait2(pmp, park_setattr, vp->v_data, NULL);

		if ((error == 0) && PUFFS_USE_FS_TTL(pmp)) {
			struct timespec *va_ttl = &setattr_msg->pvnr_va_ttl;
			struct vattr *rvap = &setattr_msg->pvnr_va;

			update_va(vp, NULL, rvap, va_ttl, NULL, flags);
		}
	}

	PUFFS_MSG_RELEASE(setattr);
	if ((flags & SETATTR_ASYNC) == 0) {
		error = checkerr(pmp, error, __func__);
		if (error)
			return error;
	} else {
		error = 0;
	}

	if (vap->va_size != VNOVAL) {
		/*
		 * If we truncated the file, make sure the data beyond 
		 * EOF in last page does not remain in cache, otherwise 
		 * if the file is later truncated to a larger size (creating
		 * a hole), that area will not return zeroes as it
		 * should. 
		 */
		if ((flags & SETATTR_CHSIZE) && PUFFS_USE_PAGECACHE(pmp) && 
		    (vap->va_size < oldsize))
			zerofill_lastpage(vp, vap->va_size);

		pn->pn_serversize = vap->va_size;
		if (flags & SETATTR_CHSIZE)
			uvm_vnp_setsize(vp, vap->va_size);
	}

	return 0;
}

int
puffs_vnop_setattr(void *v)
{
	struct vop_getattr_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct puffs_node *pn = ap->a_vp->v_data;
	int error;

	mutex_enter(&pn->pn_sizemtx);
	error = dosetattr(ap->a_vp, ap->a_vap, ap->a_cred, SETATTR_CHSIZE);
	mutex_exit(&pn->pn_sizemtx);

	return error;
}

static __inline int
doinact(struct puffs_mount *pmp, int iaflag)
{

	if (EXISTSOP(pmp, INACTIVE))
		if (pmp->pmp_flags & PUFFS_KFLAG_IAONDEMAND)
			if (iaflag || ALLOPS(pmp))
				return 1;
			else
				return 0;
		else
			return 1;
	else
		return 0;
}

static void
callinactive(struct puffs_mount *pmp, puffs_cookie_t ck, int iaflag)
{
	PUFFS_MSG_VARS(vn, inactive);

	if (doinact(pmp, iaflag)) {
		PUFFS_MSG_ALLOC(vn, inactive);
		puffs_msg_setinfo(park_inactive, PUFFSOP_VN,
		    PUFFS_VN_INACTIVE, ck);
		PUFFS_MSG_ENQUEUEWAIT_NOERROR(pmp, park_inactive);
		PUFFS_MSG_RELEASE(inactive);
	}
}

/* XXX: callinactive can't setback */
int
puffs_vnop_inactive(void *v)
{
	struct vop_inactive_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, inactive);
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	struct puffs_node *pnode;
	bool recycle = false;

	/*
	 * When puffs_cookie2vnode() misses an entry, vcache_get()
	 * creates a new node (puffs_vfsop_loadvnode being called to
	 * initialize the PUFFS part), then it discovers it is VNON,
	 * and tries to vrele() it. This leads us there, while the 
	 * cookie was stall and the node likely already reclaimed. 
	 */
	if (vp->v_type == VNON) {
		VOP_UNLOCK(vp);
		return 0;
	}

	pnode = vp->v_data;
	mutex_enter(&pnode->pn_sizemtx);

	if (doinact(pmp, pnode->pn_stat & PNODE_DOINACT)) {
		flushvncache(vp, 0, 0, false);
		PUFFS_MSG_ALLOC(vn, inactive);
		puffs_msg_setinfo(park_inactive, PUFFSOP_VN,
		    PUFFS_VN_INACTIVE, VPTOPNC(vp));
		PUFFS_MSG_ENQUEUEWAIT2_NOERROR(pmp, park_inactive, vp->v_data,
		    NULL);
		PUFFS_MSG_RELEASE(inactive);
	}
	pnode->pn_stat &= ~PNODE_DOINACT;

	/*
	 * file server thinks it's gone?  then don't be afraid care,
	 * node's life was already all it would ever be
	 */
	if (pnode->pn_stat & PNODE_NOREFS) {
		pnode->pn_stat |= PNODE_DYING;
		recycle = true;
	}

	/*
	 * Handle node TTL. 
	 * If grace has already timed out, make it reclaimed.
	 * Otherwise, we queue its expiration by sop thread, so
	 * that it does not remain for ages in the freelist, 
	 * holding memory in userspace, while we will have 
	 * to look it up again anyway.
	 */ 
	if (PUFFS_USE_FS_TTL(pmp) && !(vp->v_vflag & VV_ROOT) && !recycle) {
		bool incache = !TIMED_OUT(pnode->pn_cn_timeout);
		bool ingrace = !TIMED_OUT(pnode->pn_cn_grace);
		bool reclaimqueued = pnode->pn_stat & PNODE_SOPEXP;

		if (!incache && !ingrace && !reclaimqueued) {
			pnode->pn_stat |= PNODE_DYING;
			recycle = true;
		}

		if (!recycle && !reclaimqueued) {
			struct puffs_sopreq *psopr;
			int at = MAX(pnode->pn_cn_grace, pnode->pn_cn_timeout);

			KASSERT(curlwp != uvm.pagedaemon_lwp);
			psopr = kmem_alloc(sizeof(*psopr), KM_SLEEP);
			psopr->psopr_ck = VPTOPNC(pnode->pn_vp);
			psopr->psopr_sopreq = PUFFS_SOPREQ_EXPIRE;
			psopr->psopr_at = at;

			mutex_enter(&pmp->pmp_sopmtx);

			/*
			 * If thread has disapeared, just give up. The
			 * fs is being unmounted and the node will be 
			 * be reclaimed anyway.
			 *
			 * Otherwise, we queue the request but do not
			 * immediatly signal the thread, as the node
			 * has not been expired yet.
			 */
			if (pmp->pmp_sopthrcount == 0) {
				kmem_free(psopr, sizeof(*psopr));
			} else {
				TAILQ_INSERT_TAIL(&pmp->pmp_sopnodereqs,
				    psopr, psopr_entries); 
				pnode->pn_stat |= PNODE_SOPEXP;
			}

			mutex_exit(&pmp->pmp_sopmtx);
		}
	}

	/*
	 * Wipe direct I/O flags
	 */
	pnode->pn_stat &= ~(PNODE_RDIRECT|PNODE_WDIRECT);

	*ap->a_recycle = recycle;

	mutex_exit(&pnode->pn_sizemtx);
	VOP_UNLOCK(vp);

	return 0;
}

static void
callreclaim(struct puffs_mount *pmp, puffs_cookie_t ck, int nlookup)
{
	PUFFS_MSG_VARS(vn, reclaim);

	if (!EXISTSOP(pmp, RECLAIM))
		return;

	PUFFS_MSG_ALLOC(vn, reclaim);
	reclaim_msg->pvnr_nlookup = nlookup;
	puffs_msg_setfaf(park_reclaim);
	puffs_msg_setinfo(park_reclaim, PUFFSOP_VN, PUFFS_VN_RECLAIM, ck);

	puffs_msg_enqueue(pmp, park_reclaim);
	PUFFS_MSG_RELEASE(reclaim);
	return;
}

/*
 * always FAF, we don't really care if the server wants to fail to
 * reclaim the node or not
 */
int
puffs_vnop_reclaim(void *v)
{
	struct vop_reclaim_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	bool notifyserver = true;

	/*
	 * first things first: check if someone is trying to reclaim the
	 * root vnode.  do not allow that to travel to userspace.
	 * Note that we don't need to take the lock similarly to
	 * puffs_root(), since there is only one of us.
	 */
	if (vp->v_vflag & VV_ROOT) {
		mutex_enter(&pmp->pmp_lock);
		KASSERT(pmp->pmp_root != NULL);
		pmp->pmp_root = NULL;
		mutex_exit(&pmp->pmp_lock);
		notifyserver = false;
	}

	/*
	 * purge info from kernel before issueing FAF, since we
	 * don't really know when we'll get around to it after
	 * that and someone might race us into node creation
	 */
	mutex_enter(&pmp->pmp_lock);
	if (PUFFS_USE_NAMECACHE(pmp))
		cache_purge(vp);
	mutex_exit(&pmp->pmp_lock);

	if (notifyserver) {
		int nlookup = VPTOPP(vp)->pn_nlookup;

		callreclaim(MPTOPUFFSMP(vp->v_mount), VPTOPNC(vp), nlookup);
	}

	if (PUFFS_USE_DOTDOTCACHE(pmp)) {
		if (__predict_true(VPTOPP(vp)->pn_parent != NULL))
			vrele(VPTOPP(vp)->pn_parent);
		else
			KASSERT(vp->v_type == VNON || (vp->v_vflag & VV_ROOT));
	}

	puffs_putvnode(vp);

	return 0;
}

#define CSIZE sizeof(**ap->a_cookies)
int
puffs_vnop_readdir(void *v)
{
	struct vop_readdir_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct uio *a_uio;
		kauth_cred_t a_cred;
		int *a_eofflag;
		off_t **a_cookies;
		int *a_ncookies;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, readdir);
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	size_t argsize, tomove, cookiemem, cookiesmax;
	struct uio *uio = ap->a_uio;
	size_t howmuch, resid;
	int error;

	/*
	 * ok, so we need: resid + cookiemem = maxreq
	 * => resid + cookiesize * (resid/minsize) = maxreq
	 * => resid + cookiesize/minsize * resid = maxreq
	 * => (cookiesize/minsize + 1) * resid = maxreq
	 * => resid = maxreq / (cookiesize/minsize + 1)
	 * 
	 * Since cookiesize <= minsize and we're not very big on floats,
	 * we approximate that to be 1.  Therefore:
	 * 
	 * resid = maxreq / 2;
	 *
	 * Well, at least we didn't have to use differential equations
	 * or the Gram-Schmidt process.
	 *
	 * (yes, I'm very afraid of this)
	 */
	KASSERT(CSIZE <= _DIRENT_MINSIZE((struct dirent *)0));

	if (ap->a_cookies) {
		KASSERT(ap->a_ncookies != NULL);
		if (pmp->pmp_args.pa_fhsize == 0)
			return EOPNOTSUPP;
		resid = PUFFS_TOMOVE(uio->uio_resid, pmp) / 2;
		cookiesmax = resid/_DIRENT_MINSIZE((struct dirent *)0);
		cookiemem = ALIGN(cookiesmax*CSIZE); /* play safe */
	} else {
		resid = PUFFS_TOMOVE(uio->uio_resid, pmp);
		cookiesmax = 0;
		cookiemem = 0;
	}

	argsize = sizeof(struct puffs_vnmsg_readdir);
	tomove = resid + cookiemem;
	puffs_msgmem_alloc(argsize + tomove, &park_readdir,
	    (void *)&readdir_msg, 1);

	puffs_credcvt(&readdir_msg->pvnr_cred, ap->a_cred);
	readdir_msg->pvnr_offset = uio->uio_offset;
	readdir_msg->pvnr_resid = resid;
	readdir_msg->pvnr_ncookies = cookiesmax;
	readdir_msg->pvnr_eofflag = 0;
	readdir_msg->pvnr_dentoff = cookiemem;
	puffs_msg_setinfo(park_readdir, PUFFSOP_VN,
	    PUFFS_VN_READDIR, VPTOPNC(vp));
	puffs_msg_setdelta(park_readdir, tomove);

	PUFFS_MSG_ENQUEUEWAIT2(pmp, park_readdir, vp->v_data, NULL, error);
	error = checkerr(pmp, error, __func__);
	if (error)
		goto out;

	/* userspace is cheating? */
	if (readdir_msg->pvnr_resid > resid) {
		puffs_senderr(pmp, PUFFS_ERR_READDIR, E2BIG,
		    "resid grew", VPTOPNC(vp));
		ERROUT(EPROTO);
	}
	if (readdir_msg->pvnr_ncookies > cookiesmax) {
		puffs_senderr(pmp, PUFFS_ERR_READDIR, E2BIG,
		    "too many cookies", VPTOPNC(vp));
		ERROUT(EPROTO);
	}

	/* check eof */
	if (readdir_msg->pvnr_eofflag)
		*ap->a_eofflag = 1;

	/* bouncy-wouncy with the directory data */
	howmuch = resid - readdir_msg->pvnr_resid;

	/* force eof if no data was returned (getcwd() needs this) */
	if (howmuch == 0) {
		*ap->a_eofflag = 1;
		goto out;
	}

	error = uiomove(readdir_msg->pvnr_data + cookiemem, howmuch, uio);
	if (error)
		goto out;

	/* provide cookies to caller if so desired */
	if (ap->a_cookies) {
		KASSERT(curlwp != uvm.pagedaemon_lwp);
		*ap->a_cookies = malloc(readdir_msg->pvnr_ncookies*CSIZE,
		    M_TEMP, M_WAITOK);
		*ap->a_ncookies = readdir_msg->pvnr_ncookies;
		memcpy(*ap->a_cookies, readdir_msg->pvnr_data,
		    *ap->a_ncookies*CSIZE);
	}

	/* next readdir starts here */
	uio->uio_offset = readdir_msg->pvnr_offset;

 out:
	puffs_msgmem_release(park_readdir);
	return error;
}
#undef CSIZE

/*
 * poll works by consuming the bitmask in pn_revents.  If there are
 * events available, poll returns immediately.  If not, it issues a
 * poll to userspace, selrecords itself and returns with no available
 * events.  When the file server returns, it executes puffs_parkdone_poll(),
 * where available events are added to the bitmask.  selnotify() is
 * then also executed by that function causing us to enter here again
 * and hopefully find the missing bits (unless someone got them first,
 * in which case it starts all over again).
 */
int
puffs_vnop_poll(void *v)
{
	struct vop_poll_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int a_events;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, poll);
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	struct puffs_node *pn = vp->v_data;
	int events;

	if (EXISTSOP(pmp, POLL)) {
		mutex_enter(&pn->pn_mtx);
		events = pn->pn_revents & ap->a_events;
		if (events & ap->a_events) {
			pn->pn_revents &= ~ap->a_events;
			mutex_exit(&pn->pn_mtx);

			return events;
		} else {
			puffs_referencenode(pn);
			mutex_exit(&pn->pn_mtx);

			PUFFS_MSG_ALLOC(vn, poll);
			poll_msg->pvnr_events = ap->a_events;
			puffs_msg_setinfo(park_poll, PUFFSOP_VN,
			    PUFFS_VN_POLL, VPTOPNC(vp));
			puffs_msg_setcall(park_poll, puffs_parkdone_poll, pn);
			selrecord(curlwp, &pn->pn_sel);

			PUFFS_MSG_ENQUEUEWAIT2_NOERROR(pmp, park_poll,
			    vp->v_data, NULL);
			PUFFS_MSG_RELEASE(poll);

			return 0;
		}
	} else {
		return genfs_poll(v);
	}
}

static int
flushvncache(struct vnode *vp, off_t offlo, off_t offhi, bool wait)
{
	struct puffs_node *pn = VPTOPP(vp);
	struct vattr va;
	int pflags, error;

	/* flush out information from our metacache, see vop_setattr */
	if (pn->pn_stat & PNODE_METACACHE_MASK
	    && (pn->pn_stat & PNODE_DYING) == 0) {
		vattr_null(&va);
		error = dosetattr(vp, &va, FSCRED,
		    SETATTR_CHSIZE | (wait ? 0 : SETATTR_ASYNC));
		if (error)
			return error;
	}

	/*
	 * flush pages to avoid being overly dirty
	 */
	pflags = PGO_CLEANIT;
	if (wait)
		pflags |= PGO_SYNCIO;

	mutex_enter(vp->v_interlock);
	return VOP_PUTPAGES(vp, trunc_page(offlo), round_page(offhi), pflags);
}

int
puffs_vnop_fsync(void *v)
{
	struct vop_fsync_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		kauth_cred_t a_cred;
		int a_flags;
		off_t a_offlo;
		off_t a_offhi;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, fsync);
	struct vnode *vp;
	struct puffs_node *pn;
	struct puffs_mount *pmp;
	int error, dofaf;

	vp = ap->a_vp;
	KASSERT(vp != NULL);
	pn = VPTOPP(vp);
	KASSERT(pn != NULL);
	pmp = MPTOPUFFSMP(vp->v_mount);
	if (ap->a_flags & FSYNC_WAIT) {
		mutex_enter(&pn->pn_sizemtx);
	} else {
		if (mutex_tryenter(&pn->pn_sizemtx) == 0)
			return EDEADLK;
	}

	error = flushvncache(vp, ap->a_offlo, ap->a_offhi,
	    (ap->a_flags & FSYNC_WAIT) == FSYNC_WAIT);
	if (error)
		goto out;

	/*
	 * HELLO!  We exit already here if the user server does not
	 * support fsync OR if we should call fsync for a node which
	 * has references neither in the kernel or the fs server.
	 * Otherwise we continue to issue fsync() forward.
	 */
	error = 0;
	if (!EXISTSOP(pmp, FSYNC) || (pn->pn_stat & PNODE_DYING))
		goto out;

	dofaf = (ap->a_flags & FSYNC_WAIT) == 0 || ap->a_flags == FSYNC_LAZY;
	/*
	 * We abuse VXLOCK to mean "vnode is going to die", so we issue
	 * only FAFs for those.  Otherwise there's a danger of deadlock,
	 * since the execution context here might be the user server
	 * doing some operation on another fs, which in turn caused a
	 * vnode to be reclaimed from the freelist for this fs.
	 */
	if (dofaf == 0) {
		mutex_enter(vp->v_interlock);
		if (vdead_check(vp, VDEAD_NOWAIT) != 0)
			dofaf = 1;
		mutex_exit(vp->v_interlock);
	}

	PUFFS_MSG_ALLOC(vn, fsync);
	if (dofaf)
		puffs_msg_setfaf(park_fsync);

	puffs_credcvt(&fsync_msg->pvnr_cred, ap->a_cred);
	fsync_msg->pvnr_flags = ap->a_flags;
	fsync_msg->pvnr_offlo = ap->a_offlo;
	fsync_msg->pvnr_offhi = ap->a_offhi;
	puffs_msg_setinfo(park_fsync, PUFFSOP_VN,
	    PUFFS_VN_FSYNC, VPTOPNC(vp));

	PUFFS_MSG_ENQUEUEWAIT2(pmp, park_fsync, vp->v_data, NULL, error);
	PUFFS_MSG_RELEASE(fsync);

	error = checkerr(pmp, error, __func__);

out:
	mutex_exit(&pn->pn_sizemtx);
	return error;
}

int
puffs_vnop_seek(void *v)
{
	struct vop_seek_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		off_t a_oldoff;
		off_t a_newoff;
		kauth_cred_t a_cred;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, seek);
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	int error;

	PUFFS_MSG_ALLOC(vn, seek);
	seek_msg->pvnr_oldoff = ap->a_oldoff;
	seek_msg->pvnr_newoff = ap->a_newoff;
	puffs_credcvt(&seek_msg->pvnr_cred, ap->a_cred);
	puffs_msg_setinfo(park_seek, PUFFSOP_VN,
	    PUFFS_VN_SEEK, VPTOPNC(vp));

	PUFFS_MSG_ENQUEUEWAIT2(pmp, park_seek, vp->v_data, NULL, error);
	PUFFS_MSG_RELEASE(seek);
	return checkerr(pmp, error, __func__);
}

static int
callremove(struct puffs_mount *pmp, puffs_cookie_t dck, puffs_cookie_t ck,
	struct componentname *cnp)
{
	PUFFS_MSG_VARS(vn, remove);
	int error;

	PUFFS_MSG_ALLOC(vn, remove);
	remove_msg->pvnr_cookie_targ = ck;
	puffs_makecn(&remove_msg->pvnr_cn, &remove_msg->pvnr_cn_cred,
	    cnp, PUFFS_USE_FULLPNBUF(pmp));
	puffs_msg_setinfo(park_remove, PUFFSOP_VN, PUFFS_VN_REMOVE, dck);

	PUFFS_MSG_ENQUEUEWAIT(pmp, park_remove, error);
	PUFFS_MSG_RELEASE(remove);

	return checkerr(pmp, error, __func__);
}

/*
 * XXX: can't use callremove now because can't catch setbacks with
 * it due to lack of a pnode argument.
 */
int
puffs_vnop_remove(void *v)
{
	struct vop_remove_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, remove);
	struct vnode *dvp = ap->a_dvp;
	struct vnode *vp = ap->a_vp;
	struct puffs_node *dpn = VPTOPP(dvp);
	struct puffs_node *pn = VPTOPP(vp);
	struct componentname *cnp = ap->a_cnp;
	struct mount *mp = dvp->v_mount;
	struct puffs_mount *pmp = MPTOPUFFSMP(mp);
	int error;

	PUFFS_MSG_ALLOC(vn, remove);
	remove_msg->pvnr_cookie_targ = VPTOPNC(vp);
	puffs_makecn(&remove_msg->pvnr_cn, &remove_msg->pvnr_cn_cred,
	    cnp, PUFFS_USE_FULLPNBUF(pmp));
	puffs_msg_setinfo(park_remove, PUFFSOP_VN,
	    PUFFS_VN_REMOVE, VPTOPNC(dvp));

	puffs_msg_enqueue(pmp, park_remove);
	REFPN_AND_UNLOCKVP(dvp, dpn);
	if (dvp == vp)
		REFPN(pn);
	else
		REFPN_AND_UNLOCKVP(vp, pn);
	error = puffs_msg_wait2(pmp, park_remove, dpn, pn);

	PUFFS_MSG_RELEASE(remove);

	puffs_updatenode(VPTOPP(dvp), PUFFS_UPDATECTIME|PUFFS_UPDATEMTIME, 0);

	RELEPN_AND_VP(dvp, dpn);
	RELEPN_AND_VP(vp, pn);

	error = checkerr(pmp, error, __func__);
	return error;
}

int
puffs_vnop_mkdir(void *v)
{
	struct vop_mkdir_v3_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, mkdir);
	struct vnode *dvp = ap->a_dvp;
	struct puffs_node *dpn = VPTOPP(dvp);
	struct componentname *cnp = ap->a_cnp;
	struct mount *mp = dvp->v_mount;
	struct puffs_mount *pmp = MPTOPUFFSMP(mp);
	int error;

	PUFFS_MSG_ALLOC(vn, mkdir);
	puffs_makecn(&mkdir_msg->pvnr_cn, &mkdir_msg->pvnr_cn_cred,
	    cnp, PUFFS_USE_FULLPNBUF(pmp));
	mkdir_msg->pvnr_va = *ap->a_vap;
	puffs_msg_setinfo(park_mkdir, PUFFSOP_VN,
	    PUFFS_VN_MKDIR, VPTOPNC(dvp));

	PUFFS_MSG_ENQUEUEWAIT2(pmp, park_mkdir, dvp->v_data, NULL, error);

	error = checkerr(pmp, error, __func__);
	if (error)
		goto out;

	error = puffs_newnode(mp, dvp, ap->a_vpp,
	    mkdir_msg->pvnr_newnode, cnp, VDIR, 0);
	if (error) {
		puffs_abortbutton(pmp, PUFFS_ABORT_MKDIR, dpn->pn_cookie,
		    mkdir_msg->pvnr_newnode, cnp);
		goto out;
	}

	if (PUFFS_USE_FS_TTL(pmp)) {
		struct timespec *va_ttl = &mkdir_msg->pvnr_va_ttl;
		struct timespec *cn_ttl = &mkdir_msg->pvnr_cn_ttl;
		struct vattr *rvap = &mkdir_msg->pvnr_va;

		update_va(*ap->a_vpp, NULL, rvap, 
			  va_ttl, cn_ttl, SETATTR_CHSIZE);
	}

	VPTOPP(*ap->a_vpp)->pn_nlookup++;

	if (PUFFS_USE_DOTDOTCACHE(pmp) &&
	    (VPTOPP(*ap->a_vpp)->pn_parent != dvp))
		update_parent(*ap->a_vpp, dvp);

 out:
	PUFFS_MSG_RELEASE(mkdir);
	return error;
}

static int
callrmdir(struct puffs_mount *pmp, puffs_cookie_t dck, puffs_cookie_t ck,
	struct componentname *cnp)
{
	PUFFS_MSG_VARS(vn, rmdir);
	int error;

	PUFFS_MSG_ALLOC(vn, rmdir);
	rmdir_msg->pvnr_cookie_targ = ck;
	puffs_makecn(&rmdir_msg->pvnr_cn, &rmdir_msg->pvnr_cn_cred,
	    cnp, PUFFS_USE_FULLPNBUF(pmp));
	puffs_msg_setinfo(park_rmdir, PUFFSOP_VN, PUFFS_VN_RMDIR, dck);

	PUFFS_MSG_ENQUEUEWAIT(pmp, park_rmdir, error);
	PUFFS_MSG_RELEASE(rmdir);

	return checkerr(pmp, error, __func__);
}

int
puffs_vnop_rmdir(void *v)
{
	struct vop_rmdir_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, rmdir);
	struct vnode *dvp = ap->a_dvp;
	struct vnode *vp = ap->a_vp;
	struct puffs_node *dpn = VPTOPP(dvp);
	struct puffs_node *pn = VPTOPP(vp);
	struct puffs_mount *pmp = MPTOPUFFSMP(dvp->v_mount);
	struct componentname *cnp = ap->a_cnp;
	int error;

	PUFFS_MSG_ALLOC(vn, rmdir);
	rmdir_msg->pvnr_cookie_targ = VPTOPNC(vp);
	puffs_makecn(&rmdir_msg->pvnr_cn, &rmdir_msg->pvnr_cn_cred,
	    cnp, PUFFS_USE_FULLPNBUF(pmp));
	puffs_msg_setinfo(park_rmdir, PUFFSOP_VN,
	    PUFFS_VN_RMDIR, VPTOPNC(dvp));

	puffs_msg_enqueue(pmp, park_rmdir);
	REFPN_AND_UNLOCKVP(dvp, dpn);
	REFPN_AND_UNLOCKVP(vp, pn);
	error = puffs_msg_wait2(pmp, park_rmdir, dpn, pn);

	PUFFS_MSG_RELEASE(rmdir);

	puffs_updatenode(VPTOPP(dvp), PUFFS_UPDATECTIME|PUFFS_UPDATEMTIME, 0);

	/* XXX: some call cache_purge() *for both vnodes* here, investigate */
	RELEPN_AND_VP(dvp, dpn);
	RELEPN_AND_VP(vp, pn);

	return error;
}

int
puffs_vnop_link(void *v)
{
	struct vop_link_v2_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, link);
	struct vnode *dvp = ap->a_dvp;
	struct vnode *vp = ap->a_vp;
	struct puffs_node *dpn = VPTOPP(dvp);
	struct puffs_node *pn = VPTOPP(vp);
	struct puffs_mount *pmp = MPTOPUFFSMP(dvp->v_mount);
	struct componentname *cnp = ap->a_cnp;
	int error;

	PUFFS_MSG_ALLOC(vn, link);
	link_msg->pvnr_cookie_targ = VPTOPNC(vp);
	puffs_makecn(&link_msg->pvnr_cn, &link_msg->pvnr_cn_cred,
	    cnp, PUFFS_USE_FULLPNBUF(pmp));
	puffs_msg_setinfo(park_link, PUFFSOP_VN,
	    PUFFS_VN_LINK, VPTOPNC(dvp));

	puffs_msg_enqueue(pmp, park_link);
	error = puffs_msg_wait2(pmp, park_link, dpn, pn);

	PUFFS_MSG_RELEASE(link);

	error = checkerr(pmp, error, __func__);

	/*
	 * XXX: stay in touch with the cache.  I don't like this, but
	 * don't have a better solution either.  See also puffs_rename().
	 */
	if (error == 0) {
		puffs_updatenode(pn, PUFFS_UPDATECTIME, 0);
		puffs_updatenode(VPTOPP(dvp),
				 PUFFS_UPDATECTIME|PUFFS_UPDATEMTIME, 0);
	}

	return error;
}

int
puffs_vnop_symlink(void *v)
{
	struct vop_symlink_v3_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, symlink);
	struct vnode *dvp = ap->a_dvp;
	struct puffs_node *dpn = VPTOPP(dvp);
	struct mount *mp = dvp->v_mount;
	struct puffs_mount *pmp = MPTOPUFFSMP(dvp->v_mount);
	struct componentname *cnp = ap->a_cnp;
	int error;

	*ap->a_vpp = NULL;

	PUFFS_MSG_ALLOC(vn, symlink);
	puffs_makecn(&symlink_msg->pvnr_cn, &symlink_msg->pvnr_cn_cred,
		cnp, PUFFS_USE_FULLPNBUF(pmp));
	symlink_msg->pvnr_va = *ap->a_vap;
	(void)strlcpy(symlink_msg->pvnr_link, ap->a_target,
	    sizeof(symlink_msg->pvnr_link));
	puffs_msg_setinfo(park_symlink, PUFFSOP_VN,
	    PUFFS_VN_SYMLINK, VPTOPNC(dvp));

	PUFFS_MSG_ENQUEUEWAIT2(pmp, park_symlink, dvp->v_data, NULL, error);

	error = checkerr(pmp, error, __func__);
	if (error)
		goto out;

	error = puffs_newnode(mp, dvp, ap->a_vpp,
	    symlink_msg->pvnr_newnode, cnp, VLNK, 0);
	if (error) {
		puffs_abortbutton(pmp, PUFFS_ABORT_SYMLINK, dpn->pn_cookie,
		    symlink_msg->pvnr_newnode, cnp);
		goto out;
	}

	if (PUFFS_USE_FS_TTL(pmp)) {
		struct timespec *va_ttl = &symlink_msg->pvnr_va_ttl;
		struct timespec *cn_ttl = &symlink_msg->pvnr_cn_ttl;
		struct vattr *rvap = &symlink_msg->pvnr_va;

		update_va(*ap->a_vpp, NULL, rvap, 
			  va_ttl, cn_ttl, SETATTR_CHSIZE);
	}

	VPTOPP(*ap->a_vpp)->pn_nlookup++;

	if (PUFFS_USE_DOTDOTCACHE(pmp) && 
	    (VPTOPP(*ap->a_vpp)->pn_parent != dvp))
		update_parent(*ap->a_vpp, dvp);

 out:
	PUFFS_MSG_RELEASE(symlink);

	return error;
}

int
puffs_vnop_readlink(void *v)
{
	struct vop_readlink_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct uio *a_uio;
		kauth_cred_t a_cred;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, readlink);
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(ap->a_vp->v_mount);
	size_t linklen;
	int error;

	PUFFS_MSG_ALLOC(vn, readlink);
	puffs_credcvt(&readlink_msg->pvnr_cred, ap->a_cred);
	linklen = sizeof(readlink_msg->pvnr_link);
	readlink_msg->pvnr_linklen = linklen;
	puffs_msg_setinfo(park_readlink, PUFFSOP_VN,
	    PUFFS_VN_READLINK, VPTOPNC(vp));

	PUFFS_MSG_ENQUEUEWAIT2(pmp, park_readlink, vp->v_data, NULL, error);
	error = checkerr(pmp, error, __func__);
	if (error)
		goto out;

	/* bad bad user file server */
	if (readlink_msg->pvnr_linklen > linklen) {
		puffs_senderr(pmp, PUFFS_ERR_READLINK, E2BIG,
		    "linklen too big", VPTOPNC(ap->a_vp));
		error = EPROTO;
		goto out;
	}

	error = uiomove(&readlink_msg->pvnr_link, readlink_msg->pvnr_linklen,
	    ap->a_uio);
 out:
	PUFFS_MSG_RELEASE(readlink);
	return error;
}

int
puffs_vnop_rename(void *v)
{
	struct vop_rename_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, rename);
	struct vnode *fdvp = ap->a_fdvp, *fvp = ap->a_fvp;
	struct vnode *tdvp = ap->a_tdvp, *tvp = ap->a_tvp;
	struct puffs_node *fpn = ap->a_fvp->v_data;
	struct puffs_mount *pmp = MPTOPUFFSMP(fdvp->v_mount);
	int error;
	bool doabort = true;

	if ((fvp->v_mount != tdvp->v_mount) ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		ERROUT(EXDEV);
	}

	PUFFS_MSG_ALLOC(vn, rename);
	rename_msg->pvnr_cookie_src = VPTOPNC(fvp);
	rename_msg->pvnr_cookie_targdir = VPTOPNC(tdvp);
	if (tvp)
		rename_msg->pvnr_cookie_targ = VPTOPNC(tvp);
	else
		rename_msg->pvnr_cookie_targ = NULL;
	puffs_makecn(&rename_msg->pvnr_cn_src, &rename_msg->pvnr_cn_src_cred,
	    ap->a_fcnp, PUFFS_USE_FULLPNBUF(pmp));
	puffs_makecn(&rename_msg->pvnr_cn_targ, &rename_msg->pvnr_cn_targ_cred,
	    ap->a_tcnp, PUFFS_USE_FULLPNBUF(pmp));
	puffs_msg_setinfo(park_rename, PUFFSOP_VN,
	    PUFFS_VN_RENAME, VPTOPNC(fdvp));

	PUFFS_MSG_ENQUEUEWAIT2(pmp, park_rename, fdvp->v_data, NULL, error);
	doabort = false;
	PUFFS_MSG_RELEASE(rename);
	error = checkerr(pmp, error, __func__);

	/*
	 * XXX: stay in touch with the cache.  I don't like this, but
	 * don't have a better solution either.  See also puffs_link().
	 */
	if (error == 0) {
		puffs_updatenode(fpn, PUFFS_UPDATECTIME, 0);
		puffs_updatenode(VPTOPP(fdvp),
				 PUFFS_UPDATECTIME|PUFFS_UPDATEMTIME, 0);
		if (fdvp != tdvp)
			puffs_updatenode(VPTOPP(tdvp),
					 PUFFS_UPDATECTIME|PUFFS_UPDATEMTIME,
					 0);

		if (PUFFS_USE_DOTDOTCACHE(pmp) &&
		    (VPTOPP(fvp)->pn_parent != tdvp))
			update_parent(fvp, tdvp);
	}


 out:
	if (doabort)
		VOP_ABORTOP(tdvp, ap->a_tcnp);
	if (tvp != NULL)
		vput(tvp);
	if (tdvp == tvp)
		vrele(tdvp);
	else
		vput(tdvp);

	if (doabort)
		VOP_ABORTOP(fdvp, ap->a_fcnp);
	vrele(fdvp);
	vrele(fvp);

	return error;
}

#define RWARGS(cont, iofl, move, offset, creds)				\
	(cont)->pvnr_ioflag = (iofl);					\
	(cont)->pvnr_resid = (move);					\
	(cont)->pvnr_offset = (offset);					\
	puffs_credcvt(&(cont)->pvnr_cred, creds)

int
puffs_vnop_read(void *v)
{
	struct vop_read_args /* { 
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, read);
	struct vnode *vp = ap->a_vp;
	struct puffs_node *pn = VPTOPP(vp);
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	struct uio *uio = ap->a_uio;
	size_t tomove, argsize;
	vsize_t bytelen;
	int error;

	read_msg = NULL;
	error = 0;

	/* std sanity */
	if (uio->uio_resid == 0)
		return 0;
	if (uio->uio_offset < 0)
		return EFBIG;

	/*
	 * On the case of reading empty files and (vp->v_size != 0) below:
	 * some filesystems (hint: FUSE and distributed filesystems) still
	 * expect to get the READ in order to update atime. Reading through
	 * the case filters empty files, therefore we prefer to bypass the
	 * cache here.
	 */
	if (vp->v_type == VREG &&
	    PUFFS_USE_PAGECACHE(pmp) &&
	    !(pn->pn_stat & PNODE_RDIRECT) &&
	    (vp->v_size != 0)) {
		const int advice = IO_ADV_DECODE(ap->a_ioflag);

		while (uio->uio_resid > 0) {
			if (vp->v_size <= uio->uio_offset) {
				break;
			}
			bytelen = MIN(uio->uio_resid,
			    vp->v_size - uio->uio_offset);
			if (bytelen == 0)
				break;

			error = ubc_uiomove(&vp->v_uobj, uio, bytelen, advice,
			    UBC_READ | UBC_PARTIALOK | UBC_UNMAP_FLAG(vp));
			if (error)
				break;
		}

		if ((vp->v_mount->mnt_flag & MNT_NOATIME) == 0)
			puffs_updatenode(VPTOPP(vp), PUFFS_UPDATEATIME, 0);
	} else {
		/*
		 * in case it's not a regular file or we're operating
		 * uncached, do read in the old-fashioned style,
		 * i.e. explicit read operations
		 */

		tomove = PUFFS_TOMOVE(uio->uio_resid, pmp);
		argsize = sizeof(struct puffs_vnmsg_read);
		puffs_msgmem_alloc(argsize + tomove, &park_read,
		    (void *)&read_msg, 1);

		error = 0;
		while (uio->uio_resid > 0) {
			tomove = PUFFS_TOMOVE(uio->uio_resid, pmp);
			memset(read_msg, 0, argsize); /* XXX: touser KASSERT */
			RWARGS(read_msg, ap->a_ioflag, tomove,
			    uio->uio_offset, ap->a_cred);
			puffs_msg_setinfo(park_read, PUFFSOP_VN,
			    PUFFS_VN_READ, VPTOPNC(vp));
			puffs_msg_setdelta(park_read, tomove);

			PUFFS_MSG_ENQUEUEWAIT2(pmp, park_read, vp->v_data,
			    NULL, error);
			error = checkerr(pmp, error, __func__);
			if (error)
				break;

			if (read_msg->pvnr_resid > tomove) {
				puffs_senderr(pmp, PUFFS_ERR_READ,
				    E2BIG, "resid grew", VPTOPNC(ap->a_vp));
				error = EPROTO;
				break;
			}

			error = uiomove(read_msg->pvnr_data,
			    tomove - read_msg->pvnr_resid, uio);

			/*
			 * in case the file is out of juice, resid from
			 * userspace is != 0.  and the error-case is
			 * quite obvious
			 */
			if (error || read_msg->pvnr_resid)
				break;
		}

		puffs_msgmem_release(park_read);
	}

	return error;
}

/*
 * XXX: in case of a failure, this leaves uio in a bad state.
 * We could theoretically copy the uio and iovecs and "replay"
 * them the right amount after the userspace trip, but don't
 * bother for now.
 */
int
puffs_vnop_write(void *v)
{
	struct vop_write_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, write);
	struct vnode *vp = ap->a_vp;
	struct puffs_node *pn = VPTOPP(vp);
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	struct uio *uio = ap->a_uio;
	size_t tomove, argsize;
	off_t oldoff, newoff, origoff;
	vsize_t bytelen;
	int error, uflags;
	int ubcflags;

	error = uflags = 0;
	write_msg = NULL;

	/* std sanity */
	if (uio->uio_resid == 0)
		return 0;
	if (uio->uio_offset < 0)
		return EFBIG;

	mutex_enter(&pn->pn_sizemtx);

	/*
	 * userspace *should* be allowed to control this,
	 * but with UBC it's a bit unclear how to handle it
	 */
	if (ap->a_ioflag & IO_APPEND)
		uio->uio_offset = vp->v_size;

	origoff = uio->uio_offset;

	if (vp->v_type == VREG && 
	    PUFFS_USE_PAGECACHE(pmp) &&
	    !(pn->pn_stat & PNODE_WDIRECT)) {
		ubcflags = UBC_WRITE | UBC_PARTIALOK | UBC_UNMAP_FLAG(vp);

		while (uio->uio_resid > 0) {
			oldoff = uio->uio_offset;
			bytelen = uio->uio_resid;

			newoff = oldoff + bytelen;
			if (vp->v_size < newoff) {
				uvm_vnp_setwritesize(vp, newoff);
			}
			error = ubc_uiomove(&vp->v_uobj, uio, bytelen,
			    UVM_ADV_RANDOM, ubcflags);

			/*
			 * In case of a ubc_uiomove() error,
			 * opt to not extend the file at all and
			 * return an error.  Otherwise, if we attempt
			 * to clear the memory we couldn't fault to,
			 * we might generate a kernel page fault.
			 */
			if (vp->v_size < newoff) {
				if (error == 0) {
					uflags |= PUFFS_UPDATESIZE;
					uvm_vnp_setsize(vp, newoff);
				} else {
					uvm_vnp_setwritesize(vp, vp->v_size);
				}
			}
			if (error)
				break;

			/*
			 * If we're writing large files, flush to file server
			 * every 64k.  Otherwise we can very easily exhaust
			 * kernel and user memory, as the file server cannot
			 * really keep up with our writing speed.
			 *
			 * Note: this does *NOT* honor MNT_ASYNC, because
			 * that gives userland too much say in the kernel.
			 */
			if (oldoff >> 16 != uio->uio_offset >> 16) {
				mutex_enter(vp->v_interlock);
				error = VOP_PUTPAGES(vp, oldoff & ~0xffff,
				    uio->uio_offset & ~0xffff,
				    PGO_CLEANIT | PGO_SYNCIO);
				if (error)
					break;
			}
		}

		/* synchronous I/O? */
		if (error == 0 && ap->a_ioflag & IO_SYNC) {
			mutex_enter(vp->v_interlock);
			error = VOP_PUTPAGES(vp, trunc_page(origoff),
			    round_page(uio->uio_offset),
			    PGO_CLEANIT | PGO_SYNCIO);

		/* write through page cache? */
		} else if (error == 0 && pmp->pmp_flags & PUFFS_KFLAG_WTCACHE) {
			mutex_enter(vp->v_interlock);
			error = VOP_PUTPAGES(vp, trunc_page(origoff),
			    round_page(uio->uio_offset), PGO_CLEANIT);
		}
	} else {
		/* tomove is non-increasing */
		tomove = PUFFS_TOMOVE(uio->uio_resid, pmp);
		argsize = sizeof(struct puffs_vnmsg_write) + tomove;
		puffs_msgmem_alloc(argsize, &park_write, (void *)&write_msg,1);

		while (uio->uio_resid > 0) {
			/* move data to buffer */
			tomove = PUFFS_TOMOVE(uio->uio_resid, pmp);
			memset(write_msg, 0, argsize); /* XXX: touser KASSERT */
			RWARGS(write_msg, ap->a_ioflag, tomove,
			    uio->uio_offset, ap->a_cred);
			error = uiomove(write_msg->pvnr_data, tomove, uio);
			if (error)
				break;

			/* move buffer to userspace */
			puffs_msg_setinfo(park_write, PUFFSOP_VN,
			    PUFFS_VN_WRITE, VPTOPNC(vp));
			PUFFS_MSG_ENQUEUEWAIT2(pmp, park_write, vp->v_data,
			    NULL, error);
			error = checkerr(pmp, error, __func__);
			if (error)
				break;

			if (write_msg->pvnr_resid > tomove) {
				puffs_senderr(pmp, PUFFS_ERR_WRITE,
				    E2BIG, "resid grew", VPTOPNC(ap->a_vp));
				error = EPROTO;
				break;
			}

			/* adjust file size */
			if (vp->v_size < uio->uio_offset) {
				uflags |= PUFFS_UPDATESIZE;
				uvm_vnp_setsize(vp, uio->uio_offset);
			}

			/* didn't move everything?  bad userspace.  bail */
			if (write_msg->pvnr_resid != 0) {
				error = EIO;
				break;
			}
		}
		puffs_msgmem_release(park_write);

		/*
		 * Direct I/O on write but not on read: we must
		 * invlidate the written pages so that we read
		 * the written data and not the stalled cache.
		 */
		if ((error == 0) && 
		    (vp->v_type == VREG) && PUFFS_USE_PAGECACHE(pmp) &&
		    (pn->pn_stat & PNODE_WDIRECT) &&
		    !(pn->pn_stat & PNODE_RDIRECT)) {
			voff_t off_lo = trunc_page(origoff);
			voff_t off_hi = round_page(uio->uio_offset);

			mutex_enter(vp->v_uobj.vmobjlock);
			error = VOP_PUTPAGES(vp, off_lo, off_hi, PGO_FREE);
		}
	}

	if (vp->v_mount->mnt_flag & MNT_RELATIME)
		uflags |= PUFFS_UPDATEATIME;
	uflags |= PUFFS_UPDATECTIME;
	uflags |= PUFFS_UPDATEMTIME;
	puffs_updatenode(VPTOPP(vp), uflags, vp->v_size);

	/*
	 * If we do not use meta flush, we need to update the
	 * filesystem now, otherwise we will get a stale value
	 * on the next GETATTR
	 */
	if (!PUFFS_USE_METAFLUSH(pmp) && (uflags & PUFFS_UPDATESIZE)) {
		struct vattr va;
		int ret;

		vattr_null(&va);
		va.va_size = vp->v_size;
		ret = dosetattr(vp, &va, FSCRED, 0);
		if (ret) {
			DPRINTF(("dosetattr set size to %jd failed: %d\n",
			    (intmax_t)vp->v_size, ret));
		}
	}
	mutex_exit(&pn->pn_sizemtx);
	return error;
}

int
puffs_vnop_fallocate(void *v)
{
	struct vop_fallocate_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		off_t a_pos;
		off_t a_len;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct puffs_node *pn = VPTOPP(vp);
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	PUFFS_MSG_VARS(vn, fallocate);
	int error;

	mutex_enter(&pn->pn_sizemtx);

	PUFFS_MSG_ALLOC(vn, fallocate);
	fallocate_msg->pvnr_off = ap->a_pos;
	fallocate_msg->pvnr_len = ap->a_len;
	puffs_msg_setinfo(park_fallocate, PUFFSOP_VN,
	    PUFFS_VN_FALLOCATE, VPTOPNC(vp));

	PUFFS_MSG_ENQUEUEWAIT2(pmp, park_fallocate, vp->v_data, NULL, error);
	error = checkerr(pmp, error, __func__);
	PUFFS_MSG_RELEASE(fallocate);

	switch (error) {
	case 0:
		break;
	case EAGAIN:
		error = EIO;
		/* FALLTHROUGH */
	default:
		goto out;
	}

	if (ap->a_pos + ap->a_len > vp->v_size) {
		uvm_vnp_setsize(vp, ap->a_pos + ap->a_len);
		puffs_updatenode(pn, PUFFS_UPDATESIZE, vp->v_size);
	}
out:
 	mutex_exit(&pn->pn_sizemtx);

 	return error;
}

int
puffs_vnop_fdiscard(void *v)
{
	struct vop_fdiscard_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		off_t a_pos;
		off_t a_len;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	PUFFS_MSG_VARS(vn, fdiscard);
	int error;

	PUFFS_MSG_ALLOC(vn, fdiscard);
	fdiscard_msg->pvnr_off = ap->a_pos;
	fdiscard_msg->pvnr_len = ap->a_len;
	puffs_msg_setinfo(park_fdiscard, PUFFSOP_VN,
	    PUFFS_VN_FALLOCATE, VPTOPNC(vp));

	PUFFS_MSG_ENQUEUEWAIT2(pmp, park_fdiscard, vp->v_data, NULL, error);
	error = checkerr(pmp, error, __func__);
	PUFFS_MSG_RELEASE(fdiscard);

 	return error;
}

int
puffs_vnop_print(void *v)
{
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, print);
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	struct puffs_node *pn = vp->v_data;

	/* kernel portion */
	printf("tag VT_PUFFS, vnode %p, puffs node: %p,\n"
	    "\tuserspace cookie: %p", vp, pn, pn->pn_cookie);
	if (vp->v_type == VFIFO)
		VOCALL(fifo_vnodeop_p, VOFFSET(vop_print), v);
	printf("\n");

	/* userspace portion */
	if (EXISTSOP(pmp, PRINT)) {
		PUFFS_MSG_ALLOC(vn, print);
		puffs_msg_setinfo(park_print, PUFFSOP_VN,
		    PUFFS_VN_PRINT, VPTOPNC(vp));
		PUFFS_MSG_ENQUEUEWAIT2_NOERROR(pmp, park_print, vp->v_data,
		    NULL);
		PUFFS_MSG_RELEASE(print);
	}
	
	return 0;
}

int
puffs_vnop_pathconf(void *v)
{
	struct vop_pathconf_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, pathconf);
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	int error;

	PUFFS_MSG_ALLOC(vn, pathconf);
	pathconf_msg->pvnr_name = ap->a_name;
	puffs_msg_setinfo(park_pathconf, PUFFSOP_VN,
	    PUFFS_VN_PATHCONF, VPTOPNC(vp));
	PUFFS_MSG_ENQUEUEWAIT2(pmp, park_pathconf, vp->v_data, NULL, error);
	error = checkerr(pmp, error, __func__);
	if (!error)
		*ap->a_retval = pathconf_msg->pvnr_retval;
	PUFFS_MSG_RELEASE(pathconf);

	return error;
}

int
puffs_vnop_advlock(void *v)
{
	struct vop_advlock_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		void *a_id;
		int a_op;
		struct flock *a_fl;
		int a_flags;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, advlock);
	struct vnode *vp = ap->a_vp;
	struct puffs_node *pn = VPTOPP(vp);
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	int error;

	if (!EXISTSOP(pmp, ADVLOCK))
		return lf_advlock(ap, &pn->pn_lockf, vp->v_size); 
	
	PUFFS_MSG_ALLOC(vn, advlock);
	(void)memcpy(&advlock_msg->pvnr_fl, ap->a_fl, 
		     sizeof(advlock_msg->pvnr_fl));
	advlock_msg->pvnr_id = ap->a_id;
	advlock_msg->pvnr_op = ap->a_op;
	advlock_msg->pvnr_flags = ap->a_flags;
	puffs_msg_setinfo(park_advlock, PUFFSOP_VN,
	    PUFFS_VN_ADVLOCK, VPTOPNC(vp));
	PUFFS_MSG_ENQUEUEWAIT2(pmp, park_advlock, vp->v_data, NULL, error);
	error = checkerr(pmp, error, __func__);
	PUFFS_MSG_RELEASE(advlock);

	return error;
}

int
puffs_vnop_abortop(void *v)
{
	struct vop_abortop_args /* {
		struct vnode *a_dvp;
		struct componentname *a_cnp;
	}; */ *ap = v;
	PUFFS_MSG_VARS(vn, abortop);
	struct vnode *dvp = ap->a_dvp;
	struct puffs_mount *pmp = MPTOPUFFSMP(dvp->v_mount);
	struct componentname *cnp = ap->a_cnp;

	if (EXISTSOP(pmp, ABORTOP)) {
		PUFFS_MSG_ALLOC(vn, abortop);
		puffs_makecn(&abortop_msg->pvnr_cn, &abortop_msg->pvnr_cn_cred,
		    cnp, PUFFS_USE_FULLPNBUF(pmp));
		puffs_msg_setfaf(park_abortop);
		puffs_msg_setinfo(park_abortop, PUFFSOP_VN,
		    PUFFS_VN_ABORTOP, VPTOPNC(dvp));

		puffs_msg_enqueue(pmp, park_abortop);
		PUFFS_MSG_RELEASE(abortop);
	}

	return genfs_abortop(v);
}

#define BIOASYNC(bp) (bp->b_flags & B_ASYNC)

/*
 * This maps itself to PUFFS_VN_READ/WRITE for data transfer.
 */
int
puffs_vnop_strategy(void *v)
{
	struct vop_strategy_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, rw);
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	struct puffs_node *pn;
	struct buf *bp;
	size_t argsize;
	size_t tomove, moved;
	int error, dofaf, cansleep, dobiodone;

	pmp = MPTOPUFFSMP(vp->v_mount);
	bp = ap->a_bp;
	error = 0;
	dofaf = 0;
	cansleep = 0;
	pn = VPTOPP(vp);
	park_rw = NULL; /* explicit */
	dobiodone = 1;

	if ((BUF_ISREAD(bp) && !EXISTSOP(pmp, READ))
	    || (BUF_ISWRITE(bp) && !EXISTSOP(pmp, WRITE)))
		ERROUT(EOPNOTSUPP);

	/*
	 * Short-circuit optimization: don't flush buffer in between
	 * VOP_INACTIVE and VOP_RECLAIM in case the node has no references.
	 */
	if (pn->pn_stat & PNODE_DYING) {
		KASSERT(BUF_ISWRITE(bp));
		bp->b_resid = 0;
		goto out;
	}

#ifdef DIAGNOSTIC
	if (bp->b_bcount > pmp->pmp_msg_maxsize - PUFFS_MSGSTRUCT_MAX)
		panic("puffs_strategy: wildly inappropriate buf bcount %d",
		    bp->b_bcount);
#endif

	/*
	 * See explanation for the necessity of a FAF in puffs_fsync.
	 *
	 * Also, do FAF in case we're suspending.
	 * See puffs_vfsops.c:pageflush()
	 */
	if (BUF_ISWRITE(bp)) {
		mutex_enter(vp->v_interlock);
		if (vdead_check(vp, VDEAD_NOWAIT) != 0)
			dofaf = 1;
		if (pn->pn_stat & PNODE_FAF)
			dofaf = 1;
		mutex_exit(vp->v_interlock);
	}

	cansleep = (curlwp == uvm.pagedaemon_lwp || dofaf) ? 0 : 1;

	KASSERT(curlwp != uvm.pagedaemon_lwp || dofaf || BIOASYNC(bp));

	/* allocate transport structure */
	tomove = PUFFS_TOMOVE(bp->b_bcount, pmp);
	argsize = sizeof(struct puffs_vnmsg_rw);
	error = puffs_msgmem_alloc(argsize + tomove, &park_rw,
	    (void *)&rw_msg, cansleep);
	if (error)
		goto out;
	RWARGS(rw_msg, 0, tomove, bp->b_blkno << DEV_BSHIFT, FSCRED);

	/* 2x2 cases: read/write, faf/nofaf */
	if (BUF_ISREAD(bp)) {
		puffs_msg_setinfo(park_rw, PUFFSOP_VN,
		    PUFFS_VN_READ, VPTOPNC(vp));
		puffs_msg_setdelta(park_rw, tomove);
		if (BIOASYNC(bp)) {
			puffs_msg_setcall(park_rw,
			    puffs_parkdone_asyncbioread, bp);
			puffs_msg_enqueue(pmp, park_rw);
			dobiodone = 0;
		} else {
			PUFFS_MSG_ENQUEUEWAIT2(pmp, park_rw, vp->v_data,
			    NULL, error);
			error = checkerr(pmp, error, __func__);
			if (error)
				goto out;

			if (rw_msg->pvnr_resid > tomove) {
				puffs_senderr(pmp, PUFFS_ERR_READ,
				    E2BIG, "resid grew", VPTOPNC(vp));
				ERROUT(EPROTO);
			}

			moved = tomove - rw_msg->pvnr_resid;

			(void)memcpy(bp->b_data, rw_msg->pvnr_data, moved);
			bp->b_resid = bp->b_bcount - moved;
		}
	} else {
		puffs_msg_setinfo(park_rw, PUFFSOP_VN,
		    PUFFS_VN_WRITE, VPTOPNC(vp));
		/*
		 * make pages read-only before we write them if we want
		 * write caching info
		 */
		if (PUFFS_WCACHEINFO(pmp)) {
			struct uvm_object *uobj = &vp->v_uobj;
			int npages = (bp->b_bcount + PAGE_SIZE-1) >> PAGE_SHIFT;
			struct vm_page *vmp;
			int i;

			for (i = 0; i < npages; i++) {
				vmp= uvm_pageratop((vaddr_t)bp->b_data
				    + (i << PAGE_SHIFT));
				DPRINTF(("puffs_strategy: write-protecting "
				    "vp %p page %p, offset %" PRId64"\n",
				    vp, vmp, vmp->offset));
				mutex_enter(uobj->vmobjlock);
				vmp->flags |= PG_RDONLY;
				pmap_page_protect(vmp, VM_PROT_READ);
				mutex_exit(uobj->vmobjlock);
			}
		}

		(void)memcpy(&rw_msg->pvnr_data, bp->b_data, tomove);
		if (dofaf) {
			puffs_msg_setfaf(park_rw);
		} else if (BIOASYNC(bp)) {
			puffs_msg_setcall(park_rw,
			    puffs_parkdone_asyncbiowrite, bp);
			dobiodone = 0;
		}

		PUFFS_MSG_ENQUEUEWAIT2(pmp, park_rw, vp->v_data, NULL, error);

		if (dobiodone == 0)
			goto out;

		error = checkerr(pmp, error, __func__);
		if (error)
			goto out;

		if (rw_msg->pvnr_resid > tomove) {
			puffs_senderr(pmp, PUFFS_ERR_WRITE,
			    E2BIG, "resid grew", VPTOPNC(vp));
			ERROUT(EPROTO);
		}

		/*
		 * FAF moved everything.  Frankly, we don't
		 * really have a choice.
		 */
		if (dofaf && error == 0)
			moved = tomove;
		else 
			moved = tomove - rw_msg->pvnr_resid;

		bp->b_resid = bp->b_bcount - moved;
		if (bp->b_resid != 0) {
			ERROUT(EIO);
		}
	}

 out:
	if (park_rw)
		puffs_msgmem_release(park_rw);

	if (error)
		bp->b_error = error;

	if (error || dobiodone)
		biodone(bp);

	return error;
}

int
puffs_vnop_mmap(void *v)
{
	struct vop_mmap_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		vm_prot_t a_prot;
		kauth_cred_t a_cred;
	} */ *ap = v;
	PUFFS_MSG_VARS(vn, mmap);
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	int error;

	if (!PUFFS_USE_PAGECACHE(pmp))
		return genfs_eopnotsupp(v);

	if (EXISTSOP(pmp, MMAP)) {
		PUFFS_MSG_ALLOC(vn, mmap);
		mmap_msg->pvnr_prot = ap->a_prot;
		puffs_credcvt(&mmap_msg->pvnr_cred, ap->a_cred);
		puffs_msg_setinfo(park_mmap, PUFFSOP_VN,
		    PUFFS_VN_MMAP, VPTOPNC(vp));

		PUFFS_MSG_ENQUEUEWAIT2(pmp, park_mmap, vp->v_data, NULL, error);
		error = checkerr(pmp, error, __func__);
		PUFFS_MSG_RELEASE(mmap);
	} else {
		error = genfs_mmap(v);
	}

	return error;
}


/*
 * The rest don't get a free trip to userspace and back, they
 * have to stay within the kernel.
 */

/*
 * bmap doesn't really make any sense for puffs, so just 1:1 map it.
 * well, maybe somehow, somewhere, some day ....
 */
int
puffs_vnop_bmap(void *v)
{
	struct vop_bmap_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		daddr_t a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *ap = v;
	struct puffs_mount *pmp;

	pmp = MPTOPUFFSMP(ap->a_vp->v_mount);

	if (ap->a_vpp)
		*ap->a_vpp = ap->a_vp;
	if (ap->a_bnp)
		*ap->a_bnp = ap->a_bn;
	if (ap->a_runp)
		*ap->a_runp
		    = (PUFFS_TOMOVE(pmp->pmp_msg_maxsize, pmp)>>DEV_BSHIFT) - 1;

	return 0;
}

/*
 * Handle getpages faults in puffs.  We let genfs_getpages() do most
 * of the dirty work, but we come in this route to do accounting tasks.
 * If the user server has specified functions for cache notifications
 * about reads and/or writes, we record which type of operation we got,
 * for which page range, and proceed to issue a FAF notification to the
 * server about it.
 */
int
puffs_vnop_getpages(void *v)
{
	struct vop_getpages_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		voff_t a_offset;
		struct vm_page **a_m;
		int *a_count;
		int a_centeridx;
		vm_prot_t a_access_type;
		int a_advice;
		int a_flags;
	} */ *ap = v;
	struct puffs_mount *pmp;
	struct puffs_node *pn;
	struct vnode *vp;
	struct vm_page **pgs;
	struct puffs_cacheinfo *pcinfo = NULL;
	struct puffs_cacherun *pcrun;
	void *parkmem = NULL;
	size_t runsizes;
	int i, npages, si, streakon;
	int error, locked, write;

	pmp = MPTOPUFFSMP(ap->a_vp->v_mount);
	npages = *ap->a_count;
	pgs = ap->a_m;
	vp = ap->a_vp;
	pn = vp->v_data;
	locked = (ap->a_flags & PGO_LOCKED) != 0;
	write = (ap->a_access_type & VM_PROT_WRITE) != 0;

	/* ccg xnaht - gets Wuninitialized wrong */
	pcrun = NULL;
	runsizes = 0;

	/*
	 * Check that we aren't trying to fault in pages which our file
	 * server doesn't know about.  This happens if we extend a file by
	 * skipping some pages and later try to fault in pages which
	 * are between pn_serversize and vp_size.  This check optimizes
	 * away the common case where a file is being extended.
	 */
	if (ap->a_offset >= pn->pn_serversize && ap->a_offset < vp->v_size) {
		struct vattr va;

		/* try again later when we can block */
		if (locked)
			ERROUT(EBUSY);

		mutex_exit(vp->v_interlock);
		vattr_null(&va);
		va.va_size = vp->v_size;
		error = dosetattr(vp, &va, FSCRED, 0);
		if (error)
			ERROUT(error);
		mutex_enter(vp->v_interlock);
	}

	if (write && PUFFS_WCACHEINFO(pmp)) {
#ifdef notnowjohn
		/* allocate worst-case memory */
		runsizes = ((npages / 2) + 1) * sizeof(struct puffs_cacherun);
		KASSERT(curlwp != uvm.pagedaemon_lwp || locked);
		pcinfo = kmem_zalloc(sizeof(struct puffs_cacheinfo) + runsize,
		    locked ? KM_NOSLEEP : KM_SLEEP);

		/*
		 * can't block if we're locked and can't mess up caching
		 * information for fs server.  so come back later, please
		 */
		if (pcinfo == NULL)
			ERROUT(ENOMEM);

		parkmem = puffs_park_alloc(locked == 0);
		if (parkmem == NULL)
			ERROUT(ENOMEM);

		pcrun = pcinfo->pcache_runs;
#else
		(void)parkmem;
#endif
	}

	error = genfs_getpages(v);
	if (error)
		goto out;

	if (PUFFS_WCACHEINFO(pmp) == 0)
		goto out;

	/*
	 * Let's see whose fault it was and inform the user server of
	 * possibly read/written pages.  Map pages from read faults
	 * strictly read-only, since otherwise we might miss info on
	 * when the page is actually write-faulted to.
	 */
	if (!locked)
		mutex_enter(vp->v_uobj.vmobjlock);
	for (i = 0, si = 0, streakon = 0; i < npages; i++) {
		if (pgs[i] == NULL || pgs[i] == PGO_DONTCARE) {
			if (streakon && write) {
				streakon = 0;
				pcrun[si].pcache_runend
				    = trunc_page(pgs[i]->offset) + PAGE_MASK;
				si++;
			}
			continue;
		}
		if (streakon == 0 && write) {
			streakon = 1;
			pcrun[si].pcache_runstart = pgs[i]->offset;
		}
			
		if (!write)
			pgs[i]->flags |= PG_RDONLY;
	}
	/* was the last page part of our streak? */
	if (streakon) {
		pcrun[si].pcache_runend
		    = trunc_page(pgs[i-1]->offset) + PAGE_MASK;
		si++;
	}
	if (!locked)
		mutex_exit(vp->v_uobj.vmobjlock);

	KASSERT(si <= (npages / 2) + 1);

#ifdef notnowjohn
	/* send results to userspace */
	if (write)
		puffs_cacheop(pmp, parkmem, pcinfo,
		    sizeof(struct puffs_cacheinfo) + runsizes, VPTOPNC(vp));
#endif

 out:
	if (error) {
		if (pcinfo != NULL)
			kmem_free(pcinfo,
			    sizeof(struct puffs_cacheinfo) + runsizes);
#ifdef notnowjohn
		if (parkmem != NULL)
			puffs_park_release(parkmem, 1);
#endif
	}

	return error;
}

/*
 * Extended attribute support.
 */

int
puffs_vnop_getextattr(void *v)
{
	struct vop_getextattr_args /* 
		struct vnode *a_vp;
		int a_attrnamespace;
		const char *a_name;
		struct uio *a_uio;
		size_t *a_size;
		kauth_cred_t a_cred;
	}; */ *ap = v;
	PUFFS_MSG_VARS(vn, getextattr);
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	int attrnamespace = ap->a_attrnamespace;
	const char *name = ap->a_name;
	struct uio *uio = ap->a_uio;
	size_t *sizep = ap->a_size;
	size_t tomove, resid;
	int error;

	if (uio)
		resid = uio->uio_resid;
	else
		resid = 0;

	tomove = PUFFS_TOMOVE(resid, pmp);
	if (tomove != resid) {
		error = E2BIG;
		goto out;
	}

	puffs_msgmem_alloc(sizeof(struct puffs_vnmsg_getextattr) + tomove,
	    &park_getextattr, (void *)&getextattr_msg, 1);

	getextattr_msg->pvnr_attrnamespace = attrnamespace;
	strlcpy(getextattr_msg->pvnr_attrname, name,
	    sizeof(getextattr_msg->pvnr_attrname));
	puffs_credcvt(&getextattr_msg->pvnr_cred, ap->a_cred);
	if (sizep)
		getextattr_msg->pvnr_datasize = 1;
	getextattr_msg->pvnr_resid = tomove;

	puffs_msg_setinfo(park_getextattr,
	    PUFFSOP_VN, PUFFS_VN_GETEXTATTR, VPTOPNC(vp));
	puffs_msg_setdelta(park_getextattr, tomove);
	PUFFS_MSG_ENQUEUEWAIT2(pmp, park_getextattr, vp->v_data, NULL, error);

	error = checkerr(pmp, error, __func__);
	if (error)
		goto out;

	resid = getextattr_msg->pvnr_resid;
	if (resid > tomove) {
		puffs_senderr(pmp, PUFFS_ERR_GETEXTATTR, E2BIG,
		    "resid grew", VPTOPNC(vp));
		error = EPROTO;
		goto out;
	}

	if (sizep)
		*sizep = getextattr_msg->pvnr_datasize;
	if (uio)
		error = uiomove(getextattr_msg->pvnr_data, tomove - resid, uio);

 out:
	PUFFS_MSG_RELEASE(getextattr);
	return error;
}

int
puffs_vnop_setextattr(void *v)
{
	struct vop_setextattr_args /* {
		struct vnode *a_vp;
		int a_attrnamespace;
		const char *a_name;
		struct uio *a_uio;
		kauth_cred_t a_cred;
	}; */ *ap = v;
	PUFFS_MSG_VARS(vn, setextattr);
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	int attrnamespace = ap->a_attrnamespace;
	const char *name = ap->a_name;
	struct uio *uio = ap->a_uio;
	size_t tomove, resid;
	int error;

	if (uio)
		resid = uio->uio_resid;
	else
		resid = 0;

	tomove = PUFFS_TOMOVE(resid, pmp);
	if (tomove != resid) {
		error = E2BIG;
		goto out;
	}

	puffs_msgmem_alloc(sizeof(struct puffs_vnmsg_setextattr) + tomove,
	    &park_setextattr, (void *)&setextattr_msg, 1);

	setextattr_msg->pvnr_attrnamespace = attrnamespace;
	strlcpy(setextattr_msg->pvnr_attrname, name,
	    sizeof(setextattr_msg->pvnr_attrname));
	puffs_credcvt(&setextattr_msg->pvnr_cred, ap->a_cred);
	setextattr_msg->pvnr_resid = tomove;

	if (uio) {
		error = uiomove(setextattr_msg->pvnr_data, tomove, uio);
		if (error)
			goto out;
	}

	puffs_msg_setinfo(park_setextattr,
	    PUFFSOP_VN, PUFFS_VN_SETEXTATTR, VPTOPNC(vp));
	PUFFS_MSG_ENQUEUEWAIT2(pmp, park_setextattr, vp->v_data, NULL, error);

	error = checkerr(pmp, error, __func__);
	if (error)
		goto out;

	if (setextattr_msg->pvnr_resid != 0)
		error = EIO;

 out:
	PUFFS_MSG_RELEASE(setextattr);

	return error;
}

int
puffs_vnop_listextattr(void *v)
{
	struct vop_listextattr_args /* {
		struct vnode *a_vp;
		int a_attrnamespace;
		struct uio *a_uio;
		size_t *a_size;
		int a_flag,
		kauth_cred_t a_cred;
	}; */ *ap = v;
	PUFFS_MSG_VARS(vn, listextattr);
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	int attrnamespace = ap->a_attrnamespace;
	struct uio *uio = ap->a_uio;
	size_t *sizep = ap->a_size;
	int flag = ap->a_flag;
	size_t tomove, resid;
	int error;

	if (uio)
		resid = uio->uio_resid;
	else
		resid = 0;

	tomove = PUFFS_TOMOVE(resid, pmp);
	if (tomove != resid) {
		error = E2BIG;
		goto out;
	}

	puffs_msgmem_alloc(sizeof(struct puffs_vnmsg_listextattr) + tomove,
	    &park_listextattr, (void *)&listextattr_msg, 1);

	listextattr_msg->pvnr_attrnamespace = attrnamespace;
	listextattr_msg->pvnr_flag = flag;
	puffs_credcvt(&listextattr_msg->pvnr_cred, ap->a_cred);
	listextattr_msg->pvnr_resid = tomove;
	if (sizep)
		listextattr_msg->pvnr_datasize = 1;

	puffs_msg_setinfo(park_listextattr,
	    PUFFSOP_VN, PUFFS_VN_LISTEXTATTR, VPTOPNC(vp));
	puffs_msg_setdelta(park_listextattr, tomove);
	PUFFS_MSG_ENQUEUEWAIT2(pmp, park_listextattr, vp->v_data, NULL, error);

	error = checkerr(pmp, error, __func__);
	if (error)
		goto out;

	resid = listextattr_msg->pvnr_resid;
	if (resid > tomove) {
		puffs_senderr(pmp, PUFFS_ERR_LISTEXTATTR, E2BIG,
		    "resid grew", VPTOPNC(vp));
		error = EPROTO;
		goto out;
	}

	if (sizep)
		*sizep = listextattr_msg->pvnr_datasize;
	if (uio)
		error = uiomove(listextattr_msg->pvnr_data, tomove-resid, uio);

 out:
	PUFFS_MSG_RELEASE(listextattr);
	return error;
}

int
puffs_vnop_deleteextattr(void *v)
{
	struct vop_deleteextattr_args /* {
		struct vnode *a_vp;
		int a_attrnamespace;
		const char *a_name;
		kauth_cred_t a_cred;
	}; */ *ap = v;
	PUFFS_MSG_VARS(vn, deleteextattr);
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	int attrnamespace = ap->a_attrnamespace;
	const char *name = ap->a_name;
	int error;

	PUFFS_MSG_ALLOC(vn, deleteextattr);
	deleteextattr_msg->pvnr_attrnamespace = attrnamespace;
	strlcpy(deleteextattr_msg->pvnr_attrname, name,
	    sizeof(deleteextattr_msg->pvnr_attrname));
	puffs_credcvt(&deleteextattr_msg->pvnr_cred, ap->a_cred);

	puffs_msg_setinfo(park_deleteextattr,
	    PUFFSOP_VN, PUFFS_VN_DELETEEXTATTR, VPTOPNC(vp));
	PUFFS_MSG_ENQUEUEWAIT2(pmp, park_deleteextattr,
	    vp->v_data, NULL, error);

	error = checkerr(pmp, error, __func__);

	PUFFS_MSG_RELEASE(deleteextattr);
	return error;
}

/*
 * spec & fifo.  These call the miscfs spec and fifo vectors, but issue
 * FAF update information for the puffs node first.
 */
int
puffs_vnop_spec_read(void *v)
{
	struct vop_read_args /* { 
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;

	puffs_updatenode(VPTOPP(ap->a_vp), PUFFS_UPDATEATIME, 0);
	return VOCALL(spec_vnodeop_p, VOFFSET(vop_read), v);
}

int
puffs_vnop_spec_write(void *v)
{
	struct vop_write_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;

	puffs_updatenode(VPTOPP(ap->a_vp), PUFFS_UPDATEMTIME, 0);
	return VOCALL(spec_vnodeop_p, VOFFSET(vop_write), v);
}

int
puffs_vnop_fifo_read(void *v)
{
	struct vop_read_args /* { 
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;

	puffs_updatenode(VPTOPP(ap->a_vp), PUFFS_UPDATEATIME, 0);
	return VOCALL(fifo_vnodeop_p, VOFFSET(vop_read), v);
}

int
puffs_vnop_fifo_write(void *v)
{
	struct vop_write_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;

	puffs_updatenode(VPTOPP(ap->a_vp), PUFFS_UPDATEMTIME, 0);
	return VOCALL(fifo_vnodeop_p, VOFFSET(vop_write), v);
}

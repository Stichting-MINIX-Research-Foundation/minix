/*	$NetBSD: ptyfs_vfsops.c,v 1.55 2014/10/21 16:05:01 christos Exp $	*/

/*
 * Copyright (c) 1992, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
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
 */

/*
 * Pseudo-tty Filesystem
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ptyfs_vfsops.c,v 1.55 2014/10/21 16:05:01 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/stat.h>
#include <sys/dirent.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/filedesc.h>
#include <sys/tty.h>
#include <sys/pty.h>
#include <sys/kauth.h>
#include <sys/module.h>

#include <fs/ptyfs/ptyfs.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>

MODULE(MODULE_CLASS_VFS, ptyfs, NULL);

MALLOC_JUSTDEFINE(M_PTYFSMNT, "ptyfs mount", "ptyfs mount structures");
MALLOC_JUSTDEFINE(M_PTYFSTMP, "ptyfs temp", "ptyfs temporary structures");

VFS_PROTOS(ptyfs);

static struct sysctllog *ptyfs_sysctl_log;

static int ptyfs__allocvp(struct mount *, struct lwp *, struct vnode **,
    dev_t, char);
static int ptyfs__makename(struct mount *, struct lwp *, char *, size_t,
    dev_t, char);
static void ptyfs__getvattr(struct mount *, struct lwp *, struct vattr *);
static int ptyfs__getmp(struct lwp *, struct mount **);

/*
 * ptm glue: When we mount, we make ptm point to us.
 */
struct ptm_pty *ptyfs_save_ptm;
static int ptyfs_count;

static TAILQ_HEAD(, ptyfsmount) ptyfs_head;

struct ptm_pty ptm_ptyfspty = {
	ptyfs__allocvp,
	ptyfs__makename,
	ptyfs__getvattr,
	ptyfs__getmp,
};

static int
ptyfs__getmp(struct lwp *l, struct mount **mpp)
{
 	struct cwdinfo *cwdi = l->l_proc->p_cwdi;
 	struct mount *mp;
	struct ptyfsmount *pmnt;
 
	TAILQ_FOREACH(pmnt, &ptyfs_head, pmnt_le) {
		mp = pmnt->pmnt_mp;
		if (cwdi->cwdi_rdir == NULL)
			goto ok;

		if (vn_isunder(mp->mnt_vnodecovered, cwdi->cwdi_rdir, l))
			goto ok;
	}
 	*mpp = NULL;
 	return EOPNOTSUPP;
ok:
	*mpp = mp;
	return 0;
}

static const char *
ptyfs__getpath(struct lwp *l, const struct mount *mp)
{
#define MAXBUF (sizeof(mp->mnt_stat.f_mntonname) + 32)
	struct cwdinfo *cwdi = l->l_proc->p_cwdi;
	char *buf;
	const char *rv;
	size_t len;
	char *bp;
	int error;

	rv = mp->mnt_stat.f_mntonname;
	if (cwdi->cwdi_rdir == NULL)
		return rv;

	buf = malloc(MAXBUF, M_TEMP, M_WAITOK);
	bp = buf + MAXBUF;
	*--bp = '\0';
	error = getcwd_common(mp->mnt_vnodecovered, cwdi->cwdi_rdir, &bp,
	    buf, MAXBUF / 2, 0, l);
	if (error) {	/* Mount point is out of rdir */
		rv = NULL;
		goto out;
	}

	len = strlen(bp);
	if (len < sizeof(mp->mnt_stat.f_mntonname))	/* XXX */
		rv += strlen(rv) - len;
out:
	free(buf, M_TEMP);
	return rv;
}

static int
ptyfs__makename(struct mount *mp, struct lwp *l, char *tbuf, size_t bufsiz,
    dev_t dev, char ms)
{
	size_t len;
	const char *np;
	int pty = minor(dev);

	switch (ms) {
	case 'p':
		/* We don't provide access to the master, should we? */
		len = snprintf(tbuf, bufsiz, "/dev/null");
		break;
	case 't':
		/*
		 * We support traditional ptys, so we can get here,
		 * if pty had been opened before PTYFS was mounted,
		 * or was opened through /dev/ptyXX devices.
		 * Return it only outside chroot for more security .
		 */
		if (l->l_proc->p_cwdi->cwdi_rdir == NULL
		    && ptyfs_save_ptm != NULL 
		    && ptyfs_next_active(mp, pty) != pty)
			return (*ptyfs_save_ptm->makename)(mp, l,
			    tbuf, bufsiz, dev, ms);

		np = ptyfs__getpath(l, mp);
		if (np == NULL)
			return EOPNOTSUPP;
		len = snprintf(tbuf, bufsiz, "%s/%llu", np,
			(unsigned long long)minor(dev));
		break;
	default:
		return EINVAL;
	}

	return len >= bufsiz ? ENOSPC : 0;
}


static int
/*ARGSUSED*/
ptyfs__allocvp(struct mount *mp, struct lwp *l, struct vnode **vpp,
    dev_t dev, char ms)
{
	int error;
	ptyfstype type;

	switch (ms) {
	case 'p':
		type = PTYFSptc;
		break;
	case 't':
		type = PTYFSpts;
		break;
	default:
		return EINVAL;
	}

	error = ptyfs_allocvp(mp, vpp, type, minor(dev));
	if (error)
		return error;
	error = vn_lock(*vpp, LK_EXCLUSIVE);
	if (error) {
		vrele(*vpp);
		*vpp = NULL;
		return error;
	}
	if (type == PTYFSptc)
		ptyfs_set_active(mp, minor(dev));
	return 0;
}


static void
ptyfs__getvattr(struct mount *mp, struct lwp *l, struct vattr *vattr)
{
	struct ptyfsmount *pmnt = VFSTOPTY(mp);
	vattr_null(vattr);
	/* get real uid */
	vattr->va_uid = kauth_cred_getuid(l->l_cred);
	vattr->va_gid = pmnt->pmnt_gid;
	vattr->va_mode = pmnt->pmnt_mode;
}


void
ptyfs_init(void)
{

	TAILQ_INIT(&ptyfs_head);
	malloc_type_attach(M_PTYFSMNT);
	malloc_type_attach(M_PTYFSTMP);
	ptyfs_hashinit();
}

void
ptyfs_reinit(void)
{

}

void
ptyfs_done(void)
{

	ptyfs_hashdone();
	malloc_type_detach(M_PTYFSTMP);
	malloc_type_detach(M_PTYFSMNT);
}

#define OSIZE sizeof(struct { int f; gid_t g; mode_t m; })
/*
 * Mount the Pseudo tty params filesystem
 */
int
ptyfs_mount(struct mount *mp, const char *path, void *data, size_t *data_len)
{
	struct lwp *l = curlwp;
	int error = 0;
	struct ptyfsmount *pmnt;
	struct ptyfs_args *args = data;

	if (args == NULL)
		return EINVAL;
	if (*data_len != sizeof *args) {
		if (*data_len != OSIZE || args->version >= PTYFS_ARGSVERSION)
			return EINVAL;
	}

	if (UIO_MX & (UIO_MX - 1)) {
		log(LOG_ERR, "ptyfs: invalid directory entry size");
		return EINVAL;
	}

	if (mp->mnt_flag & MNT_GETARGS) {
		pmnt = VFSTOPTY(mp);
		if (pmnt == NULL)
			return EIO;
		args->mode = pmnt->pmnt_mode;
		args->gid = pmnt->pmnt_gid;
		if (args->version >= PTYFS_ARGSVERSION) {
			args->flags = pmnt->pmnt_flags;
			*data_len = sizeof *args;
		} else {
			*data_len = OSIZE;
		}
		return 0;
	}

#if 0
	/* Don't allow more than one mount */
	if (ptyfs_count)
		return EBUSY;
#endif

	if (mp->mnt_flag & MNT_UPDATE)
		return EOPNOTSUPP;

	if (args->version > PTYFS_ARGSVERSION)
		return EINVAL;

	pmnt = malloc(sizeof(struct ptyfsmount), M_PTYFSMNT, M_WAITOK);

	mp->mnt_data = pmnt;
	mutex_init(&pmnt->pmnt_lock, MUTEX_DEFAULT, IPL_NONE);
	pmnt->pmnt_gid = args->gid;
	pmnt->pmnt_mode = args->mode;
	if (args->version >= PTYFS_ARGSVERSION)
		pmnt->pmnt_flags = args->flags;
	else
		pmnt->pmnt_flags = 0;
	pmnt->pmnt_bitmap_size = 0;
	pmnt->pmnt_bitmap = NULL;
	mp->mnt_flag |= MNT_LOCAL;
	vfs_getnewfsid(mp);

	if ((error = set_statvfs_info(path, UIO_USERSPACE, "ptyfs",
	    UIO_SYSSPACE, mp->mnt_op->vfs_name, mp, l)) != 0) {
		free(pmnt, M_PTYFSMNT);
		return error;
	}

	pmnt->pmnt_mp = mp;
	TAILQ_INSERT_TAIL(&ptyfs_head, pmnt, pmnt_le);
	if (ptyfs_count++ == 0) {
		/* Point pty access to us */
		ptyfs_save_ptm = pty_sethandler(&ptm_ptyfspty);
	}
	return 0;
}

/*ARGSUSED*/
int
ptyfs_start(struct mount *mp, int flags)
{
	return 0;
}

/*ARGSUSED*/
int
ptyfs_unmount(struct mount *mp, int mntflags)
{
	int error;
	int flags = 0;
	struct ptyfsmount *pmnt;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	if ((error = vflush(mp, 0, flags)) != 0)
		return error;

	ptyfs_count--;
	if (ptyfs_count == 0) {
		/* Restore where pty access was pointing */
		(void)pty_sethandler(ptyfs_save_ptm);
		ptyfs_save_ptm = NULL;
	}
	TAILQ_FOREACH(pmnt, &ptyfs_head, pmnt_le) {
		if (pmnt->pmnt_mp == mp) {
			TAILQ_REMOVE(&ptyfs_head, pmnt, pmnt_le);
			break;
		}
 	}

	/*
	 * Finally, throw away the ptyfsmount structure
	 */
	if (pmnt->pmnt_bitmap_size > 0)
		kmem_free(pmnt->pmnt_bitmap, pmnt->pmnt_bitmap_size);
	mutex_destroy(&pmnt->pmnt_lock);
	free(mp->mnt_data, M_PTYFSMNT);
	mp->mnt_data = NULL;

	return 0;
}

int
ptyfs_root(struct mount *mp, struct vnode **vpp)
{
	int error;

	/* setup "." */
	error = ptyfs_allocvp(mp, vpp, PTYFSroot, 0);
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

/*ARGSUSED*/
int
ptyfs_sync(struct mount *mp, int waitfor,
    kauth_cred_t uc)
{
	return 0;
}

/*
 * Initialize this vnode / ptynode pair.
 * Only for the slave side of a pty, caller assures
 * no other thread will try to load this node.
 */
int
ptyfs_loadvnode(struct mount *mp, struct vnode *vp,
    const void *key, size_t key_len, const void **new_key)
{
	struct ptyfskey pkey;
	struct ptyfsnode *ptyfs;

	KASSERT(key_len == sizeof(pkey));
	memcpy(&pkey, key, key_len);

	ptyfs = ptyfs_get_node(pkey.ptk_type, pkey.ptk_pty);
	KASSERT(memcmp(&ptyfs->ptyfs_key, &pkey, sizeof(pkey)) == 0);

	switch (pkey.ptk_type) {
	case PTYFSroot:	/* /pts = dr-xr-xr-x */
		vp->v_type = VDIR;
		vp->v_vflag = VV_ROOT;
		break;

	case PTYFSpts:	/* /pts/N = cxxxxxxxxx */
	case PTYFSptc:	/* controlling side = cxxxxxxxxx */
		vp->v_type = VCHR;
		spec_node_init(vp, PTYFS_MAKEDEV(ptyfs));
		break;
	default:
		panic("ptyfs_loadvnode");
	}

	vp->v_tag = VT_PTYFS;
	vp->v_op = ptyfs_vnodeop_p;
	vp->v_data = ptyfs;
	uvm_vnp_setsize(vp, 0);
	*new_key = &ptyfs->ptyfs_key;
	return 0;
}

/*
 * Kernfs flat namespace lookup.
 * Currently unsupported.
 */
/*ARGSUSED*/
int
ptyfs_vget(struct mount *mp, ino_t ino,
    struct vnode **vpp)
{
	return EOPNOTSUPP;
}

extern const struct vnodeopv_desc ptyfs_vnodeop_opv_desc;

const struct vnodeopv_desc * const ptyfs_vnodeopv_descs[] = {
	&ptyfs_vnodeop_opv_desc,
	NULL,
};

struct vfsops ptyfs_vfsops = {
	.vfs_name = MOUNT_PTYFS,
	.vfs_min_mount_data = sizeof (struct ptyfs_args),
	.vfs_mount = ptyfs_mount,
	.vfs_start = ptyfs_start,
	.vfs_unmount = ptyfs_unmount,
	.vfs_root = ptyfs_root,
	.vfs_quotactl = (void *)eopnotsupp,
	.vfs_statvfs = genfs_statvfs,
	.vfs_sync = ptyfs_sync,
	.vfs_vget = ptyfs_vget,
	.vfs_loadvnode = ptyfs_loadvnode,
	.vfs_fhtovp = (void *)eopnotsupp,
	.vfs_vptofh = (void *)eopnotsupp,
	.vfs_init = ptyfs_init,
	.vfs_reinit = ptyfs_reinit,
	.vfs_done = ptyfs_done,
	.vfs_snapshot = (void *)eopnotsupp,
	.vfs_extattrctl = (void *)eopnotsupp,
	.vfs_suspendctl = (void *)eopnotsupp,
	.vfs_renamelock_enter = genfs_renamelock_enter,
	.vfs_renamelock_exit = genfs_renamelock_exit,
	.vfs_fsync = (void *)eopnotsupp,
	.vfs_opv_descs = ptyfs_vnodeopv_descs
};

static int
ptyfs_modcmd(modcmd_t cmd, void *arg)
{
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = vfs_attach(&ptyfs_vfsops);
		if (error != 0)
			break;
		sysctl_createv(&ptyfs_sysctl_log, 0, NULL, NULL,
			       CTLFLAG_PERMANENT,
			       CTLTYPE_NODE, "ptyfs",
			       SYSCTL_DESCR("Pty file system"),
			       NULL, 0, NULL, 0,
			       CTL_VFS, 23, CTL_EOL);
		/*
		 * XXX the "23" above could be dynamic, thereby eliminating
		 * one more instance of the "number to vfs" mapping problem,
		 * but "23" is the order as taken from sys/mount.h
		 */
		break;
	case MODULE_CMD_FINI:
		error = vfs_detach(&ptyfs_vfsops);
		if (error != 0)
			break;
		sysctl_teardown(&ptyfs_sysctl_log);
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

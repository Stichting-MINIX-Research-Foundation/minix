/*	$NetBSD: smbfs_vfsops.c,v 1.104 2014/12/21 10:48:53 hannken Exp $	*/

/*
 * Copyright (c) 2000-2001, Boris Popov
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
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * FreeBSD: src/sys/fs/smbfs/smbfs_vfsops.c,v 1.5 2001/12/13 13:08:34 sheldonh Exp
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: smbfs_vfsops.c,v 1.104 2014/12/21 10:48:53 hannken Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/dirent.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/malloc.h>
#include <sys/kauth.h>
#include <sys/module.h>
#include <miscfs/genfs/genfs.h>


#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_dev.h>

#include <fs/smbfs/smbfs.h>
#include <fs/smbfs/smbfs_node.h>
#include <fs/smbfs/smbfs_subr.h>

MODULE(MODULE_CLASS_VFS, smbfs, "nsmb");

VFS_PROTOS(smbfs);

static struct sysctllog *smbfs_sysctl_log;

static int smbfs_setroot(struct mount *);

extern struct vnodeopv_desc smbfs_vnodeop_opv_desc;

static const struct vnodeopv_desc *smbfs_vnodeopv_descs[] = {
	&smbfs_vnodeop_opv_desc,
	NULL,
};

struct vfsops smbfs_vfsops = {
	.vfs_name = MOUNT_SMBFS,
	.vfs_min_mount_data = sizeof (struct smbfs_args),
	.vfs_mount = smbfs_mount,
	.vfs_start = smbfs_start,
	.vfs_unmount = smbfs_unmount,
	.vfs_root = smbfs_root,
	.vfs_quotactl = (void *)eopnotsupp,
	.vfs_statvfs = smbfs_statvfs,
	.vfs_sync = smbfs_sync,
	.vfs_vget = smbfs_vget,
	.vfs_loadvnode = smbfs_loadvnode,
	.vfs_fhtovp = (void *)eopnotsupp,
	.vfs_vptofh = (void *)eopnotsupp,
	.vfs_init = smbfs_init,
	.vfs_reinit = smbfs_reinit,
	.vfs_done = smbfs_done,
	.vfs_mountroot = (void *)eopnotsupp,
	.vfs_snapshot = (void *)eopnotsupp,
	.vfs_extattrctl = vfs_stdextattrctl,
	.vfs_suspendctl = (void *)eopnotsupp,
	.vfs_renamelock_enter = genfs_renamelock_enter,
	.vfs_renamelock_exit = genfs_renamelock_exit,
	.vfs_fsync = (void *)eopnotsupp,
	.vfs_opv_descs = smbfs_vnodeopv_descs
};

static int
smbfs_modcmd(modcmd_t cmd, void *arg)
{
	const struct sysctlnode *smb = NULL;
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = vfs_attach(&smbfs_vfsops);
		if (error != 0)
			break;
		sysctl_createv(&smbfs_sysctl_log, 0, NULL, &smb,
			       CTLFLAG_PERMANENT,
			       CTLTYPE_NODE, "samba",
			       SYSCTL_DESCR("SMB/CIFS remote file system"),
			       NULL, 0, NULL, 0,
			       CTL_VFS, CTL_CREATE, CTL_EOL);

		if (smb != NULL) {
			sysctl_createv(&smbfs_sysctl_log, 0, &smb, NULL,
				       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
				       CTLTYPE_INT, "version",
				       SYSCTL_DESCR("smbfs version"),
				       NULL, SMBFS_VERSION, NULL, 0,
				       CTL_CREATE, CTL_EOL);
		}
		break;
	case MODULE_CMD_FINI:
		error = vfs_detach(&smbfs_vfsops);
		if (error != 0)
			break;
		sysctl_teardown(&smbfs_sysctl_log);
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

int
smbfs_mount(struct mount *mp, const char *path, void *data, size_t *data_len)
{
	struct lwp *l = curlwp;
	struct smbfs_args *args = data; 	  /* holds data from mount request */
	struct smbmount *smp = NULL;
	struct smb_vc *vcp;
	struct smb_share *ssp = NULL;
	struct smb_cred scred;
	char *fromname;
	int error;

	if (args == NULL)
		return EINVAL;
	if (*data_len < sizeof *args)
		return EINVAL;

	if (mp->mnt_flag & MNT_GETARGS) {
		smp = VFSTOSMBFS(mp);
		if (smp == NULL)
			return EIO;
		*args = smp->sm_args;
		*data_len = sizeof *args;
		return 0;
	}

	if (mp->mnt_flag & MNT_UPDATE)
		return EOPNOTSUPP;

	if (args->version != SMBFS_VERSION) {
		SMBVDEBUG("mount version mismatch: kernel=%d, mount=%d\n",
		    SMBFS_VERSION, args->version);
		return EINVAL;
	}

	smb_makescred(&scred, l, l->l_cred);
	error = smb_dev2share(args->dev_fd, SMBM_EXEC, &scred, &ssp);
	if (error)
		return error;
	smb_share_unlock(ssp);	/* keep ref, but unlock */
	vcp = SSTOVC(ssp);

	fromname = kmem_zalloc(MNAMELEN, KM_SLEEP);
	snprintf(fromname, MNAMELEN,
	    "//%s@%s/%s", vcp->vc_username, vcp->vc_srvname, ssp->ss_name);
	error = set_statvfs_info(path, UIO_USERSPACE, fromname, UIO_SYSSPACE,
	    mp->mnt_op->vfs_name, mp, l);
	kmem_free(fromname, MNAMELEN);
	if (error) {
		smb_share_lock(ssp);
		smb_share_put(ssp, &scred);
		return error;
	}

	mp->mnt_stat.f_iosize = vcp->vc_txmax;
	mp->mnt_stat.f_namemax =
	    (vcp->vc_hflags2 & SMB_FLAGS2_KNOWS_LONG_NAMES) ? 255 : 12;

	smp = malloc(sizeof(*smp), M_SMBFSDATA, M_WAITOK|M_ZERO);
	mp->mnt_data = smp;

	smp->sm_share = ssp;
	smp->sm_root = NULL;
	smp->sm_args = *args;
	smp->sm_caseopt = args->caseopt;
	smp->sm_args.file_mode = (smp->sm_args.file_mode &
			    (S_IRWXU|S_IRWXG|S_IRWXO)) | S_IFREG;
	smp->sm_args.dir_mode  = (smp->sm_args.dir_mode &
			    (S_IRWXU|S_IRWXG|S_IRWXO)) | S_IFDIR;

	vfs_getnewfsid(mp);
	return (0);
}

/* Unmount the filesystem described by mp. */
int
smbfs_unmount(struct mount *mp, int mntflags)
{
	struct lwp *l = curlwp;
	struct smbmount *smp = VFSTOSMBFS(mp);
	struct smb_cred scred;
	struct vnode *smbfs_rootvp = SMBTOV(smp->sm_root);
	int error, flags;

	SMBVDEBUG("smbfs_unmount: flags=%04x\n", mntflags);
	flags = 0;
	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	if (smbfs_rootvp->v_usecount > 1 && (mntflags & MNT_FORCE) == 0)
		return EBUSY;

	/* Flush all vnodes.
	 * Keep trying to flush the vnode list for the mount while 
	 * some are still busy and we are making progress towards
	 * making them not busy. This is needed because smbfs vnodes
	 * reference their parent directory but may appear after their
	 * parent in the list; one pass over the vnode list is not
	 * sufficient in this case. */
	do {
		smp->sm_didrele = 0;
		error = vflush(mp, smbfs_rootvp, flags);
	} while (error == EBUSY && smp->sm_didrele != 0);
	if (error)
		return error;

	vgone(smbfs_rootvp);

	smb_makescred(&scred, l, l->l_cred);
	smb_share_lock(smp->sm_share);
	smb_share_put(smp->sm_share, &scred);
	mp->mnt_data = NULL;

	free(smp, M_SMBFSDATA);
	return 0;
}

/*
 * Get root vnode of the smbfs filesystem, and store it in sm_root.
 */
static int
smbfs_setroot(struct mount *mp)
{
	struct smbmount *smp = VFSTOSMBFS(mp);
	struct vnode *vp;
	struct smbfattr fattr;
	struct lwp *l = curlwp;
	kauth_cred_t cred = l->l_cred;
	struct smb_cred scred;
	int error;

	KASSERT(smp->sm_root == NULL);

	smb_makescred(&scred, l, cred);
	error = smbfs_smb_lookup(NULL, NULL, 0, &fattr, &scred);
	if (error)
		return error;
	error = smbfs_nget(mp, NULL, "TheRooT", 7, &fattr, &vp);
	if (error)
		return error;

	/*
	 * Someone might have already set sm_root while we slept
	 * in smb_lookup or vnode allocation.
	 */
	if (smp->sm_root) {
		KASSERT(smp->sm_root == VTOSMB(vp));
		vrele(vp);
	} else {
		vp->v_vflag |= VV_ROOT;
		smp->sm_root = VTOSMB(vp);
	}

	return (0);
}

/*
 * Return locked root vnode of a filesystem.
 */
int
smbfs_root(struct mount *mp, struct vnode **vpp)
{
	struct smbmount *smp = VFSTOSMBFS(mp);
	int error;

	if (__predict_false(!smp->sm_root)) {
		error = smbfs_setroot(mp);
		if (error)
			return (error);
		/* fallthrough */
	}

	KASSERT(smp->sm_root != NULL && SMBTOV(smp->sm_root) != NULL);
	*vpp = SMBTOV(smp->sm_root);
	vref(*vpp);
	error = vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY);
	if (error)
		vrele(*vpp);
	return error;
}

/*
 * Make a filesystem operational.
 * Nothing to do at the moment.
 */
/* ARGSUSED */
int
smbfs_start(struct mount *mp, int flags)
{
	SMBVDEBUG("flags=%04x\n", flags);
	return 0;
}

void
smbfs_init(void)
{

	malloc_type_attach(M_SMBNODENAME);
	malloc_type_attach(M_SMBFSDATA);
	pool_init(&smbfs_node_pool, sizeof(struct smbnode), 0, 0, 0,
	    "smbfsnopl", &pool_allocator_nointr, IPL_NONE);

	SMBVDEBUG0("init.\n");
}

void
smbfs_reinit(void)
{

	SMBVDEBUG0("reinit.\n");
}

void
smbfs_done(void)
{

	pool_destroy(&smbfs_node_pool);
	malloc_type_detach(M_SMBNODENAME);
	malloc_type_detach(M_SMBFSDATA);

	SMBVDEBUG0("done.\n");
}

/*
 * smbfs_statvfs call
 */
int
smbfs_statvfs(struct mount *mp, struct statvfs *sbp)
{
	struct lwp *l = curlwp;
	struct smbmount *smp = VFSTOSMBFS(mp);
	struct smb_share *ssp = smp->sm_share;
	struct smb_cred scred;
	int error = 0;

	sbp->f_iosize = SSTOVC(ssp)->vc_txmax;		/* optimal transfer block size */
	smb_makescred(&scred, l, l->l_cred);

	error = smbfs_smb_statvfs(ssp, sbp, &scred);
	if (error)
		return error;

	sbp->f_flag = 0;		/* copy of mount exported flags */
	sbp->f_owner = mp->mnt_stat.f_owner;	/* user that mounted the filesystem */
	copy_statvfs_info(sbp, mp);
	return 0;
}

static bool
smbfs_sync_selector(void *cl, struct vnode *vp)
{
	struct smbnode *np;

	np = VTOSMB(vp);
	if (np == NULL)
		return false;

	if ((vp->v_type == VNON || (np->n_flag & NMODIFIED) == 0) &&
	     LIST_EMPTY(&vp->v_dirtyblkhd) &&
	     UVM_OBJ_IS_CLEAN(&vp->v_uobj))
		return false;

	return true;
}

/*
 * Flush out the buffer cache
 */
int
smbfs_sync(struct mount *mp, int waitfor, kauth_cred_t cred)
{
	struct vnode *vp;
	struct vnode_iterator *marker;
	int error, allerror = 0;

	vfs_vnode_iterator_init(mp, &marker);
	while ((vp = vfs_vnode_iterator_next(marker, smbfs_sync_selector,
	    NULL)))
	{
		error = vn_lock(vp, LK_EXCLUSIVE);
		if (error) {
			vrele(vp);
			continue;
		}
		error = VOP_FSYNC(vp, cred,
		    waitfor == MNT_WAIT ? FSYNC_WAIT : 0, 0, 0);
		if (error)
			allerror = error;
		vput(vp);
	}
	vfs_vnode_iterator_destroy(marker);
	return (allerror);
}

#if __FreeBSD_version < 400009
/*
 * smbfs flat namespace lookup. Unsupported.
 */
/* ARGSUSED */
int smbfs_vget(struct mount *mp, ino_t ino,
    struct vnode **vpp)
{
	return (EOPNOTSUPP);
}

#endif /* __FreeBSD_version < 400009 */

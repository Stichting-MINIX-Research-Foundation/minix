/*	$NetBSD: ufs_quota1.c,v 1.21 2014/11/25 19:48:24 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1990, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Robert Elz at The University of Melbourne.
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
 *	@(#)ufs_quota.c	8.5 (Berkeley) 5/20/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ufs_quota1.c,v 1.21 2014/11/25 19:48:24 christos Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/kauth.h>

#include <ufs/ufs/quota1.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ufs/ufs_quota.h>

static int chkdqchg(struct inode *, int64_t, kauth_cred_t, int);
static int chkiqchg(struct inode *, int32_t, kauth_cred_t, int);

/*
 * Update disk usage, and take corrective action.
 */
int
chkdq1(struct inode *ip, int64_t change, kauth_cred_t cred, int flags)
{
	struct dquot *dq;
	int i;
	int ncurblocks, error;

	if ((error = getinoquota(ip)) != 0)
		return error;
	if (change == 0)
		return (0);
	if (change < 0) {
		for (i = 0; i < MAXQUOTAS; i++) {
			if ((dq = ip->i_dquot[i]) == NODQUOT)
				continue;
			mutex_enter(&dq->dq_interlock);
			ncurblocks = dq->dq_curblocks + change;
			if (ncurblocks >= 0)
				dq->dq_curblocks = ncurblocks;
			else
				dq->dq_curblocks = 0;
			dq->dq_flags &= ~DQ_WARN(QL_BLOCK);
			dq->dq_flags |= DQ_MOD;
			mutex_exit(&dq->dq_interlock);
		}
		return (0);
	}
	for (i = 0; i < MAXQUOTAS; i++) {
		if ((dq = ip->i_dquot[i]) == NODQUOT)
			continue;
		if ((flags & FORCE) == 0 &&
		    kauth_authorize_system(cred, KAUTH_SYSTEM_FS_QUOTA,
		    KAUTH_REQ_SYSTEM_FS_QUOTA_NOLIMIT, KAUTH_ARG(i),
		    KAUTH_ARG(QL_BLOCK), NULL) != 0) {
			mutex_enter(&dq->dq_interlock);
			error = chkdqchg(ip, change, cred, i);
			mutex_exit(&dq->dq_interlock);
			if (error != 0)
				return (error);
		}
	}
	for (i = 0; i < MAXQUOTAS; i++) {
		if ((dq = ip->i_dquot[i]) == NODQUOT)
			continue;
		mutex_enter(&dq->dq_interlock);
		dq->dq_curblocks += change;
		dq->dq_flags |= DQ_MOD;
		mutex_exit(&dq->dq_interlock);
	}
	return (0);
}

/*
 * Check for a valid change to a users allocation.
 * Issue an error message if appropriate.
 */
static int
chkdqchg(struct inode *ip, int64_t change, kauth_cred_t cred, int type)
{
	struct dquot *dq = ip->i_dquot[type];
	long ncurblocks = dq->dq_curblocks + change;

	KASSERT(mutex_owned(&dq->dq_interlock));
	/*
	 * If user would exceed their hard limit, disallow space allocation.
	 */
	if (ncurblocks >= dq->dq_bhardlimit && dq->dq_bhardlimit) {
		if ((dq->dq_flags & DQ_WARN(QL_BLOCK)) == 0 &&
		    ip->i_uid == kauth_cred_geteuid(cred)) {
			uprintf("\n%s: write failed, %s disk limit reached\n",
			    ITOV(ip)->v_mount->mnt_stat.f_mntonname,
			    quotatypes[type]);
			dq->dq_flags |= DQ_WARN(QL_BLOCK);
		}
		return (EDQUOT);
	}
	/*
	 * If user is over their soft limit for too long, disallow space
	 * allocation. Reset time limit as they cross their soft limit.
	 */
	if (ncurblocks >= dq->dq_bsoftlimit && dq->dq_bsoftlimit) {
		if (dq->dq_curblocks < dq->dq_bsoftlimit) {
			dq->dq_btime =
			    time_second + ip->i_ump->umq1_btime[type];
			if (ip->i_uid == kauth_cred_geteuid(cred))
				uprintf("\n%s: warning, %s %s\n",
				    ITOV(ip)->v_mount->mnt_stat.f_mntonname,
				    quotatypes[type], "disk quota exceeded");
			return (0);
		}
		if (time_second > dq->dq_btime) {
			if ((dq->dq_flags & DQ_WARN(QL_BLOCK)) == 0 &&
			    ip->i_uid == kauth_cred_geteuid(cred)) {
				uprintf("\n%s: write failed, %s %s\n",
				    ITOV(ip)->v_mount->mnt_stat.f_mntonname,
				    quotatypes[type],
				    "disk quota exceeded for too long");
				dq->dq_flags |= DQ_WARN(QL_BLOCK);
			}
			return (EDQUOT);
		}
	}
	return (0);
}

/*
 * Check the inode limit, applying corrective action.
 */
int
chkiq1(struct inode *ip, int32_t change, kauth_cred_t cred, int flags)
{
	struct dquot *dq;
	int i;
	int ncurinodes, error;

	if ((error = getinoquota(ip)) != 0)
		return error;
	if (change == 0)
		return (0);
	if (change < 0) {
		for (i = 0; i < MAXQUOTAS; i++) {
			if ((dq = ip->i_dquot[i]) == NODQUOT)
				continue;
			mutex_enter(&dq->dq_interlock);
			ncurinodes = dq->dq_curinodes + change;
			if (ncurinodes >= 0)
				dq->dq_curinodes = ncurinodes;
			else
				dq->dq_curinodes = 0;
			dq->dq_flags &= ~DQ_WARN(QL_FILE);
			dq->dq_flags |= DQ_MOD;
			mutex_exit(&dq->dq_interlock);
		}
		return (0);
	}
	for (i = 0; i < MAXQUOTAS; i++) {
		if ((dq = ip->i_dquot[i]) == NODQUOT)
			continue;
		if ((flags & FORCE) == 0 && kauth_authorize_system(cred,
		    KAUTH_SYSTEM_FS_QUOTA, KAUTH_REQ_SYSTEM_FS_QUOTA_NOLIMIT,
		    KAUTH_ARG(i), KAUTH_ARG(QL_FILE), NULL) != 0) {
			mutex_enter(&dq->dq_interlock);
			error = chkiqchg(ip, change, cred, i);
			mutex_exit(&dq->dq_interlock);
			if (error != 0)
				return (error);
		}
	}
	for (i = 0; i < MAXQUOTAS; i++) {
		if ((dq = ip->i_dquot[i]) == NODQUOT)
			continue;
		mutex_enter(&dq->dq_interlock);
		dq->dq_curinodes += change;
		dq->dq_flags |= DQ_MOD;
		mutex_exit(&dq->dq_interlock);
	}
	return (0);
}

/*
 * Check for a valid change to a users allocation.
 * Issue an error message if appropriate.
 */
static int
chkiqchg(struct inode *ip, int32_t change, kauth_cred_t cred, int type)
{
	struct dquot *dq = ip->i_dquot[type];
	long ncurinodes = dq->dq_curinodes + change;

	KASSERT(mutex_owned(&dq->dq_interlock));
	/*
	 * If user would exceed their hard limit, disallow inode allocation.
	 */
	if (ncurinodes >= dq->dq_ihardlimit && dq->dq_ihardlimit) {
		if ((dq->dq_flags & DQ_WARN(QL_FILE)) == 0 &&
		    ip->i_uid == kauth_cred_geteuid(cred)) {
			uprintf("\n%s: write failed, %s inode limit reached\n",
			    ITOV(ip)->v_mount->mnt_stat.f_mntonname,
			    quotatypes[type]);
			dq->dq_flags |= DQ_WARN(QL_FILE);
		}
		return (EDQUOT);
	}
	/*
	 * If user is over their soft limit for too long, disallow inode
	 * allocation. Reset time limit as they cross their soft limit.
	 */
	if (ncurinodes >= dq->dq_isoftlimit && dq->dq_isoftlimit) {
		if (dq->dq_curinodes < dq->dq_isoftlimit) {
			dq->dq_itime =
			    time_second + ip->i_ump->umq1_itime[type];
			if (ip->i_uid == kauth_cred_geteuid(cred))
				uprintf("\n%s: warning, %s %s\n",
				    ITOV(ip)->v_mount->mnt_stat.f_mntonname,
				    quotatypes[type], "inode quota exceeded");
			return (0);
		}
		if (time_second > dq->dq_itime) {
			if ((dq->dq_flags & DQ_WARN(QL_FILE)) == 0 &&
			    ip->i_uid == kauth_cred_geteuid(cred)) {
				uprintf("\n%s: write failed, %s %s\n",
				    ITOV(ip)->v_mount->mnt_stat.f_mntonname,
				    quotatypes[type],
				    "inode quota exceeded for too long");
				dq->dq_flags |= DQ_WARN(QL_FILE);
			}
			return (EDQUOT);
		}
	}
	return (0);
}

int
quota1_umount(struct mount *mp, int flags)
{
	int i, error;
	struct ufsmount *ump = VFSTOUFS(mp);
	struct lwp *l = curlwp;

	if ((ump->um_flags & UFS_QUOTA) == 0)
		return 0;

	if ((error = vflush(mp, NULLVP, SKIPSYSTEM | flags)) != 0)
		return (error);

	for (i = 0; i < MAXQUOTAS; i++) {
		if (ump->um_quotas[i] != NULLVP) {
			quota1_handle_cmd_quotaoff(l, ump, i);
		}
	}
	return 0;
}

/*
 * Code to process quotactl commands.
 */

/*
 * set up a quota file for a particular file system.
 */
int
quota1_handle_cmd_quotaon(struct lwp *l, struct ufsmount *ump, int type,
    const char *fname)
{
	struct mount *mp = ump->um_mountp;
	struct vnode *vp, **vpp;
	struct vnode_iterator *marker;
	struct dquot *dq;
	int error;
	struct pathbuf *pb;
	struct nameidata nd;

	if (ump->um_flags & UFS_QUOTA2) {
		uprintf("%s: quotas v2 already enabled\n",
		    mp->mnt_stat.f_mntonname);
		return (EBUSY);
	}
		
	if (mp->mnt_wapbl != NULL) {
		printf("%s: quota v1 cannot be used with -o log\n",
		    mp->mnt_stat.f_mntonname);
		return (EOPNOTSUPP);
	}

	vpp = &ump->um_quotas[type];

	pb = pathbuf_create(fname);
	if (pb == NULL) {
		return ENOMEM;
	}
	NDINIT(&nd, LOOKUP, FOLLOW, pb);
	if ((error = vn_open(&nd, FREAD|FWRITE, 0)) != 0) {
		pathbuf_destroy(pb);
		return error;
	}
	vp = nd.ni_vp;
	pathbuf_destroy(pb);

	VOP_UNLOCK(vp);
	if (vp->v_type != VREG) {
		(void) vn_close(vp, FREAD|FWRITE, l->l_cred);
		return (EACCES);
	}
	if (*vpp != vp)
		quota1_handle_cmd_quotaoff(l, ump, type);
	mutex_enter(&dqlock);
	while ((ump->umq1_qflags[type] & (QTF_CLOSING | QTF_OPENING)) != 0)
		cv_wait(&dqcv, &dqlock);
	ump->umq1_qflags[type] |= QTF_OPENING;
	mutex_exit(&dqlock);
	mp->mnt_flag |= MNT_QUOTA;
	vp->v_vflag |= VV_SYSTEM;	/* XXXSMP */
	*vpp = vp;
	/*
	 * Save the credential of the process that turned on quotas.
	 * Set up the time limits for this quota.
	 */
	kauth_cred_hold(l->l_cred);
	ump->um_cred[type] = l->l_cred;
	ump->umq1_btime[type] = MAX_DQ_TIME;
	ump->umq1_itime[type] = MAX_IQ_TIME;
	if (dqget(NULLVP, 0, ump, type, &dq) == 0) {
		if (dq->dq_btime > 0)
			ump->umq1_btime[type] = dq->dq_btime;
		if (dq->dq_itime > 0)
			ump->umq1_itime[type] = dq->dq_itime;
		dqrele(NULLVP, dq);
	}
	/*
	 * Search vnodes associated with this mount point,
	 * adding references to quota file being opened.
	 * NB: only need to add dquot's for inodes being modified.
	 */
	vfs_vnode_iterator_init(mp, &marker);
	while ((vp = vfs_vnode_iterator_next(marker, NULL, NULL))) {
		error = vn_lock(vp, LK_EXCLUSIVE);
		if (error) {
			vrele(vp);
			continue;
		}
		mutex_enter(vp->v_interlock);
		if (VTOI(vp) == NULL || vp->v_type == VNON ||
		    vp->v_writecount == 0) {
			mutex_exit(vp->v_interlock);
			vput(vp);
			continue;
		}
		mutex_exit(vp->v_interlock);
		if ((error = getinoquota(VTOI(vp))) != 0) {
			vput(vp);
			break;
		}
		vput(vp);
	}
	vfs_vnode_iterator_destroy(marker);

	mutex_enter(&dqlock);
	ump->umq1_qflags[type] &= ~QTF_OPENING;
	cv_broadcast(&dqcv);
	if (error == 0)
		ump->um_flags |= UFS_QUOTA;
	mutex_exit(&dqlock);
	if (error)
		quota1_handle_cmd_quotaoff(l, ump, type);
	return (error);
}

/*
 * turn off disk quotas for a filesystem.
 */
int
quota1_handle_cmd_quotaoff(struct lwp *l, struct ufsmount *ump, int type)
{
	struct mount *mp = ump->um_mountp;
	struct vnode *vp;
	struct vnode *qvp;
	struct vnode_iterator *marker;
	struct dquot *dq;
	struct inode *ip;
	kauth_cred_t cred;
	int i, error;

	mutex_enter(&dqlock);
	while ((ump->umq1_qflags[type] & (QTF_CLOSING | QTF_OPENING)) != 0)
		cv_wait(&dqcv, &dqlock);
	if ((qvp = ump->um_quotas[type]) == NULLVP) {
		mutex_exit(&dqlock);
		return (0);
	}
	ump->umq1_qflags[type] |= QTF_CLOSING;
	ump->um_flags &= ~UFS_QUOTA;
	mutex_exit(&dqlock);
	/*
	 * Search vnodes associated with this mount point,
	 * deleting any references to quota file being closed.
	 */
	vfs_vnode_iterator_init(mp, &marker);
	while ((vp = vfs_vnode_iterator_next(marker, NULL, NULL))) {
		error = vn_lock(vp, LK_EXCLUSIVE);
		if (error) {
			vrele(vp);
			continue;
		}
		ip = VTOI(vp);
		if (ip == NULL || vp->v_type == VNON) {
			vput(vp);
			continue;
		}
		dq = ip->i_dquot[type];
		ip->i_dquot[type] = NODQUOT;
		dqrele(vp, dq);
		vput(vp);
	}
	vfs_vnode_iterator_destroy(marker);
#ifdef DIAGNOSTIC
	dqflush(qvp);
#endif
	qvp->v_vflag &= ~VV_SYSTEM;
	error = vn_close(qvp, FREAD|FWRITE, l->l_cred);
	mutex_enter(&dqlock);
	ump->um_quotas[type] = NULLVP;
	cred = ump->um_cred[type];
	ump->um_cred[type] = NOCRED;
	for (i = 0; i < MAXQUOTAS; i++)
		if (ump->um_quotas[i] != NULLVP)
			break;
	ump->umq1_qflags[type] &= ~QTF_CLOSING;
	cv_broadcast(&dqcv);
	mutex_exit(&dqlock);
	kauth_cred_free(cred);
	if (i == MAXQUOTAS)
		mp->mnt_flag &= ~MNT_QUOTA;
	return (error);
}

int             
quota1_handle_cmd_get(struct ufsmount *ump, const struct quotakey *qk,
    struct quotaval *qv)
{
	struct dquot *dq;
	int error;
	struct quotaval blocks, files;
	int idtype;
	id_t id;

	idtype = qk->qk_idtype;
	id = qk->qk_id;

	if (ump->um_quotas[idtype] == NULLVP)
		return ENODEV;

	if (id == QUOTA_DEFAULTID) { /* we want the grace period of id 0 */
		if ((error = dqget(NULLVP, 0, ump, idtype, &dq)) != 0)
			return error;

	} else {
		if ((error = dqget(NULLVP, id, ump, idtype, &dq)) != 0)
			return error;
	}
	dqblk_to_quotavals(&dq->dq_un.dq1_dqb, &blocks, &files);
	dqrele(NULLVP, dq);
	if (id == QUOTA_DEFAULTID) {
		if (blocks.qv_expiretime > 0)
			blocks.qv_grace = blocks.qv_expiretime;
		else
			blocks.qv_grace = MAX_DQ_TIME;
		if (files.qv_expiretime > 0)
			files.qv_grace = files.qv_expiretime;
		else
			files.qv_grace = MAX_DQ_TIME;
	}

	switch (qk->qk_objtype) {
	    case QUOTA_OBJTYPE_BLOCKS:
		*qv = blocks;
		break;
	    case QUOTA_OBJTYPE_FILES:
		*qv = files;
		break;
	    default:
		return EINVAL;
	}

	return 0;
}

static uint32_t
quota1_encode_limit(uint64_t lim)
{
	if (lim == QUOTA_NOLIMIT || lim >= 0xffffffff) {
		return 0;
	}
	return lim;
}

int
quota1_handle_cmd_put(struct ufsmount *ump, const struct quotakey *key,
    const struct quotaval *val)
{
	struct dquot *dq;
	struct dqblk dqb;
	int error;

	switch (key->qk_idtype) {
	    case QUOTA_IDTYPE_USER:
	    case QUOTA_IDTYPE_GROUP:
		break;
	    default:
		return EINVAL;
	}

	switch (key->qk_objtype) {
	    case QUOTA_OBJTYPE_BLOCKS:
	    case QUOTA_OBJTYPE_FILES:
		break;
	    default:
		return EINVAL;
	}

	if (ump->um_quotas[key->qk_idtype] == NULLVP)
		return ENODEV;

	if (key->qk_id == QUOTA_DEFAULTID) {
		/* just update grace times */
		id_t id = 0;

		if ((error = dqget(NULLVP, id, ump, key->qk_idtype, &dq)) != 0)
			return error;
		mutex_enter(&dq->dq_interlock);
		if (val->qv_grace != QUOTA_NOTIME) {
			if (key->qk_objtype == QUOTA_OBJTYPE_BLOCKS)
				ump->umq1_btime[key->qk_idtype] = dq->dq_btime =
					val->qv_grace;
			if (key->qk_objtype == QUOTA_OBJTYPE_FILES)
				ump->umq1_itime[key->qk_idtype] = dq->dq_itime =
					val->qv_grace;
		}
		dq->dq_flags |= DQ_MOD;
		mutex_exit(&dq->dq_interlock);
		dqrele(NULLVP, dq);
		return 0;
	}

	if ((error = dqget(NULLVP, key->qk_id, ump, key->qk_idtype, &dq)) != 0)
		return (error);
	mutex_enter(&dq->dq_interlock);
	/*
	 * Copy all but the current values.
	 * Reset time limit if previously had no soft limit or were
	 * under it, but now have a soft limit and are over it.
	 */
	dqb.dqb_curblocks = dq->dq_curblocks;
	dqb.dqb_curinodes = dq->dq_curinodes;
	dqb.dqb_btime = dq->dq_btime;
	dqb.dqb_itime = dq->dq_itime;
	if (key->qk_objtype == QUOTA_OBJTYPE_BLOCKS) {
		dqb.dqb_bsoftlimit = quota1_encode_limit(val->qv_softlimit);
		dqb.dqb_bhardlimit = quota1_encode_limit(val->qv_hardlimit);
		dqb.dqb_isoftlimit = dq->dq_isoftlimit;
		dqb.dqb_ihardlimit = dq->dq_ihardlimit;
	} else {
		KASSERT(key->qk_objtype == QUOTA_OBJTYPE_FILES);
		dqb.dqb_bsoftlimit = dq->dq_bsoftlimit;
		dqb.dqb_bhardlimit = dq->dq_bhardlimit;
		dqb.dqb_isoftlimit = quota1_encode_limit(val->qv_softlimit);
		dqb.dqb_ihardlimit = quota1_encode_limit(val->qv_hardlimit);
	}
	if (dq->dq_id == 0 && val->qv_grace != QUOTA_NOTIME) {
		/* also update grace time if available */
		if (key->qk_objtype == QUOTA_OBJTYPE_BLOCKS) {
			ump->umq1_btime[key->qk_idtype] = dqb.dqb_btime =
				val->qv_grace;
		}
		if (key->qk_objtype == QUOTA_OBJTYPE_FILES) {
			ump->umq1_itime[key->qk_idtype] = dqb.dqb_itime =
				val->qv_grace;
		}
	}
	if (dqb.dqb_bsoftlimit &&
	    dq->dq_curblocks >= dqb.dqb_bsoftlimit &&
	    (dq->dq_bsoftlimit == 0 || dq->dq_curblocks < dq->dq_bsoftlimit))
		dqb.dqb_btime = time_second + ump->umq1_btime[key->qk_idtype];
	if (dqb.dqb_isoftlimit &&
	    dq->dq_curinodes >= dqb.dqb_isoftlimit &&
	    (dq->dq_isoftlimit == 0 || dq->dq_curinodes < dq->dq_isoftlimit))
		dqb.dqb_itime = time_second + ump->umq1_itime[key->qk_idtype];
	dq->dq_un.dq1_dqb = dqb;
	if (dq->dq_curblocks < dq->dq_bsoftlimit)
		dq->dq_flags &= ~DQ_WARN(QL_BLOCK);
	if (dq->dq_curinodes < dq->dq_isoftlimit)
		dq->dq_flags &= ~DQ_WARN(QL_FILE);
	if (dq->dq_isoftlimit == 0 && dq->dq_bsoftlimit == 0 &&
	    dq->dq_ihardlimit == 0 && dq->dq_bhardlimit == 0)
		dq->dq_flags |= DQ_FAKE;
	else
		dq->dq_flags &= ~DQ_FAKE;
	dq->dq_flags |= DQ_MOD;
	mutex_exit(&dq->dq_interlock);
	dqrele(NULLVP, dq);
	return (0);
}


#if 0
/*
 * Q_SETQUOTA - assign an entire dqblk structure.
 */
int
setquota1(struct mount *mp, u_long id, int type, struct dqblk *dqb)
{
	struct dquot *dq;
	struct dquot *ndq;
	struct ufsmount *ump = VFSTOUFS(mp);
	

	if ((error = dqget(NULLVP, id, ump, type, &ndq)) != 0)
		return (error);
	dq = ndq;
	mutex_enter(&dq->dq_interlock);
	/*
	 * Copy all but the current values.
	 * Reset time limit if previously had no soft limit or were
	 * under it, but now have a soft limit and are over it.
	 */
	dqb->dqb_curblocks = dq->dq_curblocks;
	dqb->dqb_curinodes = dq->dq_curinodes;
	if (dq->dq_id != 0) {
		dqb->dqb_btime = dq->dq_btime;
		dqb->dqb_itime = dq->dq_itime;
	}
	if (dqb->dqb_bsoftlimit &&
	    dq->dq_curblocks >= dqb->dqb_bsoftlimit &&
	    (dq->dq_bsoftlimit == 0 || dq->dq_curblocks < dq->dq_bsoftlimit))
		dqb->dqb_btime = time_second + ump->umq1_btime[type];
	if (dqb->dqb_isoftlimit &&
	    dq->dq_curinodes >= dqb->dqb_isoftlimit &&
	    (dq->dq_isoftlimit == 0 || dq->dq_curinodes < dq->dq_isoftlimit))
		dqb->dqb_itime = time_second + ump->umq1_itime[type];
	dq->dq_un.dq1_dqb = *dqb;
	if (dq->dq_curblocks < dq->dq_bsoftlimit)
		dq->dq_flags &= ~DQ_WARN(QL_BLOCK);
	if (dq->dq_curinodes < dq->dq_isoftlimit)
		dq->dq_flags &= ~DQ_WARN(QL_FILE);
	if (dq->dq_isoftlimit == 0 && dq->dq_bsoftlimit == 0 &&
	    dq->dq_ihardlimit == 0 && dq->dq_bhardlimit == 0)
		dq->dq_flags |= DQ_FAKE;
	else
		dq->dq_flags &= ~DQ_FAKE;
	dq->dq_flags |= DQ_MOD;
	mutex_exit(&dq->dq_interlock);
	dqrele(NULLVP, dq);
	return (0);
}

/*
 * Q_SETUSE - set current inode and block usage.
 */
int
setuse(struct mount *mp, u_long id, int type, void *addr)
{
	struct dquot *dq;
	struct ufsmount *ump = VFSTOUFS(mp);
	struct dquot *ndq;
	struct dqblk usage;
	int error;

	error = copyin(addr, (void *)&usage, sizeof (struct dqblk));
	if (error)
		return (error);
	if ((error = dqget(NULLVP, id, ump, type, &ndq)) != 0)
		return (error);
	dq = ndq;
	mutex_enter(&dq->dq_interlock);
	/*
	 * Reset time limit if have a soft limit and were
	 * previously under it, but are now over it.
	 */
	if (dq->dq_bsoftlimit && dq->dq_curblocks < dq->dq_bsoftlimit &&
	    usage.dqb_curblocks >= dq->dq_bsoftlimit)
		dq->dq_btime = time_second + ump->umq1_btime[type];
	if (dq->dq_isoftlimit && dq->dq_curinodes < dq->dq_isoftlimit &&
	    usage.dqb_curinodes >= dq->dq_isoftlimit)
		dq->dq_itime = time_second + ump->umq1_itime[type];
	dq->dq_curblocks = usage.dqb_curblocks;
	dq->dq_curinodes = usage.dqb_curinodes;
	if (dq->dq_curblocks < dq->dq_bsoftlimit)
		dq->dq_flags &= ~DQ_WARN(QL_BLOCK);
	if (dq->dq_curinodes < dq->dq_isoftlimit)
		dq->dq_flags &= ~DQ_WARN(QL_FILE);
	dq->dq_flags |= DQ_MOD;
	mutex_exit(&dq->dq_interlock);
	dqrele(NULLVP, dq);
	return (0);
}
#endif

/*
 * Q_SYNC - sync quota files to disk.
 */
int
q1sync(struct mount *mp)
{
	struct ufsmount *ump = VFSTOUFS(mp);
	struct vnode *vp;
	struct vnode_iterator *marker;
	struct dquot *dq;
	int i, error;

	/*
	 * Check if the mount point has any quotas.
	 * If not, simply return.
	 */
	for (i = 0; i < MAXQUOTAS; i++)
		if (ump->um_quotas[i] != NULLVP)
			break;
	if (i == MAXQUOTAS)
		return (0);

	/*
	 * Search vnodes associated with this mount point,
	 * synchronizing any modified dquot structures.
	 */
	vfs_vnode_iterator_init(mp, &marker);
	while ((vp = vfs_vnode_iterator_next(marker, NULL, NULL))) {
		error = vn_lock(vp, LK_EXCLUSIVE);
		if (error) {
			vrele(vp);
			continue;
		}
		if (VTOI(vp) == NULL || vp->v_type == VNON) {
			vput(vp);
			continue;
		}
		for (i = 0; i < MAXQUOTAS; i++) {
			dq = VTOI(vp)->i_dquot[i];
			if (dq == NODQUOT)
				continue;
			mutex_enter(&dq->dq_interlock);
			if (dq->dq_flags & DQ_MOD)
				dq1sync(vp, dq);
			mutex_exit(&dq->dq_interlock);
		}
		vput(vp);
	}
	vfs_vnode_iterator_destroy(marker);
	return (0);
}

/*
 * Obtain a dquot structure for the specified identifier and quota file
 * reading the information from the file if necessary.
 */
int
dq1get(struct vnode *dqvp, u_long id, struct ufsmount *ump, int type,
    struct dquot *dq)
{
	struct iovec aiov;
	struct uio auio;
	int error;

	KASSERT(mutex_owned(&dq->dq_interlock));
	vn_lock(dqvp, LK_EXCLUSIVE | LK_RETRY);
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	aiov.iov_base = (void *)&dq->dq_un.dq1_dqb;
	aiov.iov_len = sizeof (struct dqblk);
	auio.uio_resid = sizeof (struct dqblk);
	auio.uio_offset = (off_t)(id * sizeof (struct dqblk));
	auio.uio_rw = UIO_READ;
	UIO_SETUP_SYSSPACE(&auio);
	error = VOP_READ(dqvp, &auio, 0, ump->um_cred[type]);
	if (auio.uio_resid == sizeof(struct dqblk) && error == 0)
		memset((void *)&dq->dq_un.dq1_dqb, 0, sizeof(struct dqblk));
	VOP_UNLOCK(dqvp);
	/*
	 * I/O error in reading quota file, release
	 * quota structure and reflect problem to caller.
	 */
	if (error)
		return (error);
	/*
	 * Check for no limit to enforce.
	 * Initialize time values if necessary.
	 */
	if (dq->dq_isoftlimit == 0 && dq->dq_bsoftlimit == 0 &&
	    dq->dq_ihardlimit == 0 && dq->dq_bhardlimit == 0)
		dq->dq_flags |= DQ_FAKE;
	if (dq->dq_id != 0) {
		if (dq->dq_btime == 0)
			dq->dq_btime = time_second + ump->umq1_btime[type];
		if (dq->dq_itime == 0)
			dq->dq_itime = time_second + ump->umq1_itime[type];
	}
	return (0);
}

/*
 * Update the disk quota in the quota file.
 */
int
dq1sync(struct vnode *vp, struct dquot *dq)
{
	struct vnode *dqvp;
	struct iovec aiov;
	struct uio auio;
	int error;

	if (dq == NODQUOT)
		panic("dq1sync: dquot");
	KASSERT(mutex_owned(&dq->dq_interlock));
	if ((dq->dq_flags & DQ_MOD) == 0)
		return (0);
	if ((dqvp = dq->dq_ump->um_quotas[dq->dq_type]) == NULLVP)
		panic("dq1sync: file");
	KASSERT(dqvp != vp);
	vn_lock(dqvp, LK_EXCLUSIVE | LK_RETRY);
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	aiov.iov_base = (void *)&dq->dq_un.dq1_dqb;
	aiov.iov_len = sizeof (struct dqblk);
	auio.uio_resid = sizeof (struct dqblk);
	auio.uio_offset = (off_t)dq->dq_id * sizeof (struct dqblk);
	auio.uio_rw = UIO_WRITE;
	UIO_SETUP_SYSSPACE(&auio);
	error = VOP_WRITE(dqvp, &auio, 0, dq->dq_ump->um_cred[dq->dq_type]);
	if (auio.uio_resid && error == 0)
		error = EIO;
	dq->dq_flags &= ~DQ_MOD;
	VOP_UNLOCK(dqvp);
	return (error);
}

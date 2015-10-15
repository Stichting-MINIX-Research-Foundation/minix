/*	$NetBSD: ulfs_quota.c,v 1.12 2014/06/28 22:27:51 dholland Exp $	*/
/*  from NetBSD: ufs_quota.c,v 1.115 2013/11/16 17:04:53 dholland Exp  */

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
__KERNEL_RCSID(0, "$NetBSD: ulfs_quota.c,v 1.12 2014/06/28 22:27:51 dholland Exp $");

#if defined(_KERNEL_OPT)
#include "opt_quota.h"
#endif 
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/kauth.h>

#include <sys/quotactl.h>
#include <ufs/lfs/ulfs_quotacommon.h>
#include <ufs/lfs/ulfs_inode.h>
#include <ufs/lfs/ulfsmount.h>
#include <ufs/lfs/ulfs_extern.h>
#include <ufs/lfs/ulfs_quota.h>

kmutex_t lfs_dqlock;
kcondvar_t lfs_dqcv;
const char *lfs_quotatypes[ULFS_MAXQUOTAS] = INITQFNAMES;

/*
 * Code pertaining to management of the in-core dquot data structures.
 */
#define DQHASH(dqvp, id) \
	(((((long)(dqvp)) >> 8) + id) & dqhash)
static LIST_HEAD(dqhashhead, dquot) *dqhashtbl;
static u_long dqhash;
static pool_cache_t dquot_cache;


static int quota_handle_cmd_stat(struct mount *, struct lwp *,
    struct quotactl_args *args);
static int quota_handle_cmd_idtypestat(struct mount *, struct lwp *,
    struct quotactl_args *args);
static int quota_handle_cmd_objtypestat(struct mount *, struct lwp *,
    struct quotactl_args *args);
static int quota_handle_cmd_get(struct mount *, struct lwp *,
    struct quotactl_args *args);
static int quota_handle_cmd_put(struct mount *, struct lwp *,
    struct quotactl_args *args);
static int quota_handle_cmd_cursorget(struct mount *, struct lwp *,
    struct quotactl_args *args);
static int quota_handle_cmd_del(struct mount *, struct lwp *,
    struct quotactl_args *args);
static int quota_handle_cmd_quotaon(struct mount *, struct lwp *, 
    struct quotactl_args *args);
static int quota_handle_cmd_quotaoff(struct mount *, struct lwp *, 
    struct quotactl_args *args);
static int quota_handle_cmd_cursoropen(struct mount *, struct lwp *,
    struct quotactl_args *args);
static int quota_handle_cmd_cursorclose(struct mount *, struct lwp *,
    struct quotactl_args *args);
static int quota_handle_cmd_cursorskipidtype(struct mount *, struct lwp *,
    struct quotactl_args *args);
static int quota_handle_cmd_cursoratend(struct mount *, struct lwp *,
    struct quotactl_args *args);
static int quota_handle_cmd_cursorrewind(struct mount *, struct lwp *,
    struct quotactl_args *args);

/*
 * Initialize the quota fields of an inode.
 */
void
ulfsquota_init(struct inode *ip)
{
	int i;

	for (i = 0; i < ULFS_MAXQUOTAS; i++)
		ip->i_dquot[i] = NODQUOT;
}

/*
 * Release the quota fields from an inode.
 */
void
ulfsquota_free(struct inode *ip)
{
	int i;

	for (i = 0; i < ULFS_MAXQUOTAS; i++) {
		lfs_dqrele(ITOV(ip), ip->i_dquot[i]);
		ip->i_dquot[i] = NODQUOT;
	}
}

/*
 * Update disk usage, and take corrective action.
 */
int
lfs_chkdq(struct inode *ip, int64_t change, kauth_cred_t cred, int flags)
{
	/* do not track snapshot usage, or we will deadlock */
	if ((ip->i_flags & SF_SNAPSHOT) != 0)
		return 0;

#ifdef LFS_QUOTA
	if (ip->i_lfs->um_flags & ULFS_QUOTA)
		return lfs_chkdq1(ip, change, cred, flags);
#endif
#ifdef LFS_QUOTA2
	if (ip->i_lfs->um_flags & ULFS_QUOTA2)
		return lfs_chkdq2(ip, change, cred, flags);
#endif
	return 0;
}

/*
 * Check the inode limit, applying corrective action.
 */
int
lfs_chkiq(struct inode *ip, int32_t change, kauth_cred_t cred, int flags)
{
	/* do not track snapshot usage, or we will deadlock */
	if ((ip->i_flags & SF_SNAPSHOT) != 0)
		return 0;
#ifdef LFS_QUOTA
	if (ip->i_lfs->um_flags & ULFS_QUOTA)
		return lfs_chkiq1(ip, change, cred, flags);
#endif
#ifdef LFS_QUOTA2
	if (ip->i_lfs->um_flags & ULFS_QUOTA2)
		return lfs_chkiq2(ip, change, cred, flags);
#endif
	return 0;
}

int
lfsquota_handle_cmd(struct mount *mp, struct lwp *l,
		 struct quotactl_args *args)
{
	int error = 0;

	switch (args->qc_op) {
	    case QUOTACTL_STAT:
		error = quota_handle_cmd_stat(mp, l, args);
		break;
	    case QUOTACTL_IDTYPESTAT:
		error = quota_handle_cmd_idtypestat(mp, l, args);
		break;
	    case QUOTACTL_OBJTYPESTAT:
		error = quota_handle_cmd_objtypestat(mp, l, args);
		break;
	    case QUOTACTL_QUOTAON:
		error = quota_handle_cmd_quotaon(mp, l, args);
		break;
	    case QUOTACTL_QUOTAOFF:
		error = quota_handle_cmd_quotaoff(mp, l, args);
		break;
	    case QUOTACTL_GET:
		error = quota_handle_cmd_get(mp, l, args);
		break;
	    case QUOTACTL_PUT:
		error = quota_handle_cmd_put(mp, l, args);
		break;
	    case QUOTACTL_CURSORGET:
		error = quota_handle_cmd_cursorget(mp, l, args);
		break;
	    case QUOTACTL_DEL:
		error = quota_handle_cmd_del(mp, l, args);
		break;
	    case QUOTACTL_CURSOROPEN:
		error = quota_handle_cmd_cursoropen(mp, l, args);
		break;
	    case QUOTACTL_CURSORCLOSE:
		error = quota_handle_cmd_cursorclose(mp, l, args);
		break;
	    case QUOTACTL_CURSORSKIPIDTYPE:
		error = quota_handle_cmd_cursorskipidtype(mp, l, args);
		break;
	    case QUOTACTL_CURSORATEND:
		error = quota_handle_cmd_cursoratend(mp, l, args);
		break;
	    case QUOTACTL_CURSORREWIND:
		error = quota_handle_cmd_cursorrewind(mp, l, args);
		break;
	    default:
		panic("Invalid quotactl operation %d\n", args->qc_op);
	}

	return error;
}

static int 
quota_handle_cmd_stat(struct mount *mp, struct lwp *l, 
    struct quotactl_args *args)
{
	struct ulfsmount *ump = VFSTOULFS(mp);
	struct lfs *fs = ump->um_lfs;

	KASSERT(args->qc_op == QUOTACTL_STAT);

	if ((fs->um_flags & (ULFS_QUOTA|ULFS_QUOTA2)) == 0)
		return EOPNOTSUPP;

#ifdef LFS_QUOTA
	if (fs->um_flags & ULFS_QUOTA) {
		struct quotastat *info = args->u.stat.qc_info;
		strcpy(info->qs_implname, "lfs quota v1");
		info->qs_numidtypes = ULFS_MAXQUOTAS;
		/* XXX no define for this */
		info->qs_numobjtypes = 2;
		info->qs_restrictions = 0;
		info->qs_restrictions |= QUOTA_RESTRICT_NEEDSQUOTACHECK;
		info->qs_restrictions |= QUOTA_RESTRICT_UNIFORMGRACE;
		info->qs_restrictions |= QUOTA_RESTRICT_32BIT;
	} else
#endif
#ifdef LFS_QUOTA2
	if (fs->um_flags & ULFS_QUOTA2) {
		struct quotastat *info = args->u.stat.qc_info;
		strcpy(info->qs_implname, "lfs quota v2");
		info->qs_numidtypes = ULFS_MAXQUOTAS;
		info->qs_numobjtypes = N_QL;
		info->qs_restrictions = 0;
	} else
#endif
		return EOPNOTSUPP;

	return 0;
}

static int 
quota_handle_cmd_idtypestat(struct mount *mp, struct lwp *l, 
    struct quotactl_args *args)
{
	struct ulfsmount *ump = VFSTOULFS(mp);
	struct lfs *fs = ump->um_lfs;
	int idtype;
	struct quotaidtypestat *info;
	const char *name;

	KASSERT(args->qc_op == QUOTACTL_IDTYPESTAT);
	idtype = args->u.idtypestat.qc_idtype;
	info = args->u.idtypestat.qc_info;

	if ((fs->um_flags & (ULFS_QUOTA|ULFS_QUOTA2)) == 0)
		return EOPNOTSUPP;

	/*
	 * These are the same for both QUOTA and QUOTA2.
	 */
	switch (idtype) {
	    case QUOTA_IDTYPE_USER:
		name = "user";
		break;
	    case QUOTA_IDTYPE_GROUP:
		name = "group";
		break;
	    default:
		return EINVAL;
	}
	strlcpy(info->qis_name, name, sizeof(info->qis_name));
	return 0;
}

static int 
quota_handle_cmd_objtypestat(struct mount *mp, struct lwp *l, 
    struct quotactl_args *args)
{
	struct ulfsmount *ump = VFSTOULFS(mp);
	struct lfs *fs = ump->um_lfs;
	int objtype;
	struct quotaobjtypestat *info;
	const char *name;
	int isbytes;

	KASSERT(args->qc_op == QUOTACTL_OBJTYPESTAT);
	objtype = args->u.objtypestat.qc_objtype;
	info = args->u.objtypestat.qc_info;

	if ((fs->um_flags & (ULFS_QUOTA|ULFS_QUOTA2)) == 0)
		return EOPNOTSUPP;

	/*
	 * These are the same for both QUOTA and QUOTA2.
	 */
	switch (objtype) {
	    case QUOTA_OBJTYPE_BLOCKS:
		name = "block";
		isbytes = 1;
		break;
	    case QUOTA_OBJTYPE_FILES:
		name = "file";
		isbytes = 0;
		break;
	    default:
		return EINVAL;
	}
	strlcpy(info->qos_name, name, sizeof(info->qos_name));
	info->qos_isbytes = isbytes;
	return 0;
}

/* XXX shouldn't all this be in kauth ? */
static int
quota_get_auth(struct mount *mp, struct lwp *l, uid_t id) {
	/* The user can always query about his own quota. */
	if (id == kauth_cred_geteuid(l->l_cred))
		return 0;
	return kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_FS_QUOTA,
	    KAUTH_REQ_SYSTEM_FS_QUOTA_GET, mp, KAUTH_ARG(id), NULL);
}

static int 
quota_handle_cmd_get(struct mount *mp, struct lwp *l, 
    struct quotactl_args *args)
{
	struct ulfsmount *ump = VFSTOULFS(mp);
	struct lfs *fs = ump->um_lfs;
	int error;
	const struct quotakey *qk;

	KASSERT(args->qc_op == QUOTACTL_GET);
	qk = args->u.get.qc_key;

	if ((fs->um_flags & (ULFS_QUOTA|ULFS_QUOTA2)) == 0)
		return EOPNOTSUPP;
	
	error = quota_get_auth(mp, l, qk->qk_id);
	if (error != 0) 
		return error;
#ifdef LFS_QUOTA
	if (fs->um_flags & ULFS_QUOTA) {
		struct quotaval *qv = args->u.get.qc_val;
		error = lfsquota1_handle_cmd_get(ump, qk, qv);
	} else
#endif
#ifdef LFS_QUOTA2
	if (fs->um_flags & ULFS_QUOTA2) {
		struct quotaval *qv = args->u.get.qc_val;
		error = lfsquota2_handle_cmd_get(ump, qk, qv);
	} else
#endif
		panic("quota_handle_cmd_get: no support ?");
		
	if (error != 0)
		return error;

	return error;
}

static int 
quota_handle_cmd_put(struct mount *mp, struct lwp *l, 
    struct quotactl_args *args)
{
	struct ulfsmount *ump = VFSTOULFS(mp);
	struct lfs *fs = ump->um_lfs;
	const struct quotakey *qk;
	id_t kauth_id;
	int error;

	KASSERT(args->qc_op == QUOTACTL_PUT);
	qk = args->u.put.qc_key;

	if ((fs->um_flags & (ULFS_QUOTA|ULFS_QUOTA2)) == 0)
		return EOPNOTSUPP;

	kauth_id = qk->qk_id;
	if (kauth_id == QUOTA_DEFAULTID) {
		kauth_id = 0;
	}

	error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_FS_QUOTA,
	    KAUTH_REQ_SYSTEM_FS_QUOTA_MANAGE, mp, KAUTH_ARG(kauth_id),
	    NULL);
	if (error != 0) {
		return error;
	}

#ifdef LFS_QUOTA
	if (fs->um_flags & ULFS_QUOTA) {
		const struct quotaval *qv = args->u.put.qc_val;
		error = lfsquota1_handle_cmd_put(ump, qk, qv);
	} else
#endif
#ifdef LFS_QUOTA2
	if (fs->um_flags & ULFS_QUOTA2) {
		const struct quotaval *qv = args->u.put.qc_val;
		error = lfsquota2_handle_cmd_put(ump, qk, qv);
	} else
#endif
		panic("quota_handle_cmd_get: no support ?");
		
	if (error == ENOENT) {
		error = 0;
	}

	return error;
}

static int 
quota_handle_cmd_del(struct mount *mp, struct lwp *l, 
    struct quotactl_args *args)
{
	struct ulfsmount *ump = VFSTOULFS(mp);
	struct lfs *fs = ump->um_lfs;
	const struct quotakey *qk;
	id_t kauth_id;
	int error;

	KASSERT(args->qc_op == QUOTACTL_DEL);
	qk = args->u.del.qc_key;

	kauth_id = qk->qk_id;
	if (kauth_id == QUOTA_DEFAULTID) {
		kauth_id = 0;
	}

	if ((fs->um_flags & ULFS_QUOTA2) == 0)
		return EOPNOTSUPP;

	/* avoid whitespace changes */
	{
		error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_FS_QUOTA,
		    KAUTH_REQ_SYSTEM_FS_QUOTA_MANAGE, mp, KAUTH_ARG(kauth_id),
		    NULL);
		if (error != 0)
			goto err;
#ifdef LFS_QUOTA2
		if (fs->um_flags & ULFS_QUOTA2) {
			error = lfsquota2_handle_cmd_del(ump, qk);
		} else
#endif
			panic("quota_handle_cmd_get: no support ?");
		
		if (error && error != ENOENT)
			goto err;
	}

	return 0;
 err:
	return error;
}

static int 
quota_handle_cmd_cursorget(struct mount *mp, struct lwp *l, 
    struct quotactl_args *args)
{
	struct ulfsmount *ump = VFSTOULFS(mp);
	struct lfs *fs = ump->um_lfs;
	int error;

	KASSERT(args->qc_op == QUOTACTL_CURSORGET);

	if ((fs->um_flags & ULFS_QUOTA2) == 0)
		return EOPNOTSUPP;
	
	error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_FS_QUOTA,
	    KAUTH_REQ_SYSTEM_FS_QUOTA_GET, mp, NULL, NULL);
	if (error)
		return error;
		
#ifdef LFS_QUOTA2
	if (fs->um_flags & ULFS_QUOTA2) {
		struct quotakcursor *cursor = args->u.cursorget.qc_cursor;
		struct quotakey *keys = args->u.cursorget.qc_keys;
		struct quotaval *vals = args->u.cursorget.qc_vals;
		unsigned maxnum = args->u.cursorget.qc_maxnum;
		unsigned *ret = args->u.cursorget.qc_ret;

		error = lfsquota2_handle_cmd_cursorget(ump, cursor, keys, vals,
		    maxnum, ret);
	} else
#endif
		panic("quota_handle_cmd_cursorget: no support ?");

	return error;
}

static int 
quota_handle_cmd_cursoropen(struct mount *mp, struct lwp *l, 
    struct quotactl_args *args)
{
#ifdef LFS_QUOTA2
	struct ulfsmount *ump = VFSTOULFS(mp);
	struct lfs *fs = ump->um_lfs;
	struct quotakcursor *cursor;
#endif
	int error;

	KASSERT(args->qc_op == QUOTACTL_CURSOROPEN);

	error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_FS_QUOTA,
	    KAUTH_REQ_SYSTEM_FS_QUOTA_GET, mp, NULL, NULL);
	if (error)
		return error;

#ifdef LFS_QUOTA2
	if (fs->um_flags & ULFS_QUOTA2) {
		cursor = args->u.cursoropen.qc_cursor;
		error = lfsquota2_handle_cmd_cursoropen(ump, cursor);
	} else
#endif
		error = EOPNOTSUPP;

	return error;
}

static int 
quota_handle_cmd_cursorclose(struct mount *mp, struct lwp *l, 
    struct quotactl_args *args)
{
#ifdef LFS_QUOTA2
	struct ulfsmount *ump = VFSTOULFS(mp);
	struct lfs *fs = ump->um_lfs;
#endif
	int error;

	KASSERT(args->qc_op == QUOTACTL_CURSORCLOSE);

	error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_FS_QUOTA,
	    KAUTH_REQ_SYSTEM_FS_QUOTA_GET, mp, NULL, NULL);
	if (error)
		return error;

#ifdef LFS_QUOTA2
	if (fs->um_flags & ULFS_QUOTA2) {
		struct quotakcursor *cursor = args->u.cursorclose.qc_cursor;
		error = lfsquota2_handle_cmd_cursorclose(ump, cursor);
	} else
#endif
		error = EOPNOTSUPP;

	return error;
}

static int 
quota_handle_cmd_cursorskipidtype(struct mount *mp, struct lwp *l, 
    struct quotactl_args *args)
{
#ifdef LFS_QUOTA2
	struct ulfsmount *ump = VFSTOULFS(mp);
	struct lfs *fs = ump->um_lfs;
#endif
	int error;

	KASSERT(args->qc_op == QUOTACTL_CURSORSKIPIDTYPE);

#ifdef LFS_QUOTA2
	if (fs->um_flags & ULFS_QUOTA2) {
		struct quotakcursor *cursor = args->u.cursorskipidtype.qc_cursor;
		int idtype = args->u.cursorskipidtype.qc_idtype;
		error = lfsquota2_handle_cmd_cursorskipidtype(ump, cursor, idtype);
	} else
#endif
		error = EOPNOTSUPP;

	return error;
}

static int 
quota_handle_cmd_cursoratend(struct mount *mp, struct lwp *l, 
    struct quotactl_args *args)
{
#ifdef LFS_QUOTA2
	struct ulfsmount *ump = VFSTOULFS(mp);
	struct lfs *fs = ump->um_lfs;
#endif
	int error;

	KASSERT(args->qc_op == QUOTACTL_CURSORATEND);

#ifdef LFS_QUOTA2
	if (fs->um_flags & ULFS_QUOTA2) {
		struct quotakcursor *cursor = args->u.cursoratend.qc_cursor;
		int *ret = args->u.cursoratend.qc_ret;
		error = lfsquota2_handle_cmd_cursoratend(ump, cursor, ret);
	} else
#endif
		error = EOPNOTSUPP;

	return error;
}

static int 
quota_handle_cmd_cursorrewind(struct mount *mp, struct lwp *l, 
    struct quotactl_args *args)
{
#ifdef LFS_QUOTA2
	struct ulfsmount *ump = VFSTOULFS(mp);
	struct lfs *fs = ump->um_lfs;
#endif
	int error;

	KASSERT(args->qc_op == QUOTACTL_CURSORREWIND);

#ifdef LFS_QUOTA2
	if (fs->um_flags & ULFS_QUOTA2) {
		struct quotakcursor *cursor = args->u.cursorrewind.qc_cursor;
		error = lfsquota2_handle_cmd_cursorrewind(ump, cursor);
	} else
#endif
		error = EOPNOTSUPP;

	return error;
}

static int 
quota_handle_cmd_quotaon(struct mount *mp, struct lwp *l, 
    struct quotactl_args *args)
{
	struct ulfsmount *ump = VFSTOULFS(mp);
	struct lfs *fs = ump->um_lfs;
	int error;

	KASSERT(args->qc_op == QUOTACTL_QUOTAON);

	if ((fs->um_flags & ULFS_QUOTA2) != 0)
		return EBUSY;
	
	error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_FS_QUOTA,
	    KAUTH_REQ_SYSTEM_FS_QUOTA_ONOFF, mp, NULL, NULL);
	if (error != 0) {
		return error;
	}
#ifdef LFS_QUOTA
	int idtype = args->u.quotaon.qc_idtype;
	const char *qfile = args->u.quotaon.qc_quotafile;
	error = lfsquota1_handle_cmd_quotaon(l, ump, idtype, qfile);
#else
	error = EOPNOTSUPP;
#endif
	
	return error;
}

static int 
quota_handle_cmd_quotaoff(struct mount *mp, struct lwp *l, 
    struct quotactl_args *args)
{
	struct ulfsmount *ump = VFSTOULFS(mp);
	struct lfs *fs = ump->um_lfs;
	int error;

	KASSERT(args->qc_op == QUOTACTL_QUOTAOFF);

	if ((fs->um_flags & ULFS_QUOTA2) != 0)
		return EOPNOTSUPP;
	
	error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_FS_QUOTA,
	    KAUTH_REQ_SYSTEM_FS_QUOTA_ONOFF, mp, NULL, NULL);
	if (error != 0) {
		return error;
	}
#ifdef LFS_QUOTA
	int idtype = args->u.quotaoff.qc_idtype;
	error = lfsquota1_handle_cmd_quotaoff(l, ump, idtype);
#else
	error = EOPNOTSUPP;
#endif
	
	return error;
}

/*
 * Initialize the quota system.
 */
void
lfs_dqinit(void)
{

	mutex_init(&lfs_dqlock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&lfs_dqcv, "quota");
	dqhashtbl = hashinit(desiredvnodes, HASH_LIST, true, &dqhash);
	dquot_cache = pool_cache_init(sizeof(struct dquot), 0, 0, 0, "lfsdq",
	    NULL, IPL_NONE, NULL, NULL, NULL);
}

void
lfs_dqreinit(void)
{
	struct dquot *dq;
	struct dqhashhead *oldhash, *hash;
	struct vnode *dqvp;
	u_long oldmask, mask, hashval;
	int i;

	hash = hashinit(desiredvnodes, HASH_LIST, true, &mask);
	mutex_enter(&lfs_dqlock);
	oldhash = dqhashtbl;
	oldmask = dqhash;
	dqhashtbl = hash;
	dqhash = mask;
	for (i = 0; i <= oldmask; i++) {
		while ((dq = LIST_FIRST(&oldhash[i])) != NULL) {
			dqvp = dq->dq_ump->um_quotas[dq->dq_type];
			LIST_REMOVE(dq, dq_hash);
			hashval = DQHASH(dqvp, dq->dq_id);
			LIST_INSERT_HEAD(&dqhashtbl[hashval], dq, dq_hash);
		}
	}
	mutex_exit(&lfs_dqlock);
	hashdone(oldhash, HASH_LIST, oldmask);
}

/*
 * Free resources held by quota system.
 */
void
lfs_dqdone(void)
{

	pool_cache_destroy(dquot_cache);
	hashdone(dqhashtbl, HASH_LIST, dqhash);
	cv_destroy(&lfs_dqcv);
	mutex_destroy(&lfs_dqlock);
}

/*
 * Set up the quotas for an inode.
 *
 * This routine completely defines the semantics of quotas.
 * If other criteria want to be used to establish quotas, the
 * ULFS_MAXQUOTAS value in quotas.h should be increased, and the
 * additional dquots set up here.
 */
int
lfs_getinoquota(struct inode *ip)
{
	struct ulfsmount *ump = ip->i_ump;
	//struct lfs *fs = ump->um_lfs; // notyet
	struct vnode *vp = ITOV(ip);
	int i, error;
	u_int32_t ino_ids[ULFS_MAXQUOTAS];

	/*
	 * To avoid deadlocks never update quotas for quota files
	 * on the same file system
	 */
	for (i = 0; i < ULFS_MAXQUOTAS; i++)
		if (vp == ump->um_quotas[i])
			return 0;

	ino_ids[ULFS_USRQUOTA] = ip->i_uid;
	ino_ids[ULFS_GRPQUOTA] = ip->i_gid;
	for (i = 0; i < ULFS_MAXQUOTAS; i++) {
		/*
		 * If the file id changed the quota needs update.
		 */
		if (ip->i_dquot[i] != NODQUOT &&
		    ip->i_dquot[i]->dq_id != ino_ids[i]) {
			lfs_dqrele(ITOV(ip), ip->i_dquot[i]);
			ip->i_dquot[i] = NODQUOT;
		}
		/*
		 * Set up the quota based on file id.
		 * ENODEV means that quotas are not enabled.
		 */
		if (ip->i_dquot[i] == NODQUOT &&
		    (error = lfs_dqget(vp, ino_ids[i], ump, i, &ip->i_dquot[i])) &&
		    error != ENODEV)
			return (error);
	}
	return 0;
}

/*
 * Obtain a dquot structure for the specified identifier and quota file
 * reading the information from the file if necessary.
 */
int
lfs_dqget(struct vnode *vp, u_long id, struct ulfsmount *ump, int type,
    struct dquot **dqp)
{
	struct lfs *fs = ump->um_lfs;
	struct dquot *dq, *ndq;
	struct dqhashhead *dqh;
	struct vnode *dqvp;
	int error = 0; /* XXX gcc */

	/* Lock to see an up to date value for QTF_CLOSING. */
	mutex_enter(&lfs_dqlock);
	if ((fs->um_flags & (ULFS_QUOTA|ULFS_QUOTA2)) == 0) {
		mutex_exit(&lfs_dqlock);
		*dqp = NODQUOT;
		return (ENODEV);
	}
	dqvp = ump->um_quotas[type];
#ifdef LFS_QUOTA
	if (fs->um_flags & ULFS_QUOTA) {
		if (dqvp == NULLVP || (ump->umq1_qflags[type] & QTF_CLOSING)) {
			mutex_exit(&lfs_dqlock);
			*dqp = NODQUOT;
			return (ENODEV);
		}
	}
#endif
#ifdef LFS_QUOTA2
	if (fs->um_flags & ULFS_QUOTA2) {
		if (dqvp == NULLVP) {
			mutex_exit(&lfs_dqlock);
			*dqp = NODQUOT;
			return (ENODEV);
		}
	}
#endif
	KASSERT(dqvp != vp);
	/*
	 * Check the cache first.
	 */
	dqh = &dqhashtbl[DQHASH(dqvp, id)];
	LIST_FOREACH(dq, dqh, dq_hash) {
		if (dq->dq_id != id ||
		    dq->dq_ump->um_quotas[dq->dq_type] != dqvp)
			continue;
		KASSERT(dq->dq_cnt > 0);
		lfs_dqref(dq);
		mutex_exit(&lfs_dqlock);
		*dqp = dq;
		return (0);
	}
	/*
	 * Not in cache, allocate a new one.
	 */
	mutex_exit(&lfs_dqlock);
	ndq = pool_cache_get(dquot_cache, PR_WAITOK);
	/*
	 * Initialize the contents of the dquot structure.
	 */
	memset((char *)ndq, 0, sizeof *ndq);
	ndq->dq_flags = 0;
	ndq->dq_id = id;
	ndq->dq_ump = ump;
	ndq->dq_type = type;
	mutex_init(&ndq->dq_interlock, MUTEX_DEFAULT, IPL_NONE);
	mutex_enter(&lfs_dqlock);
	dqh = &dqhashtbl[DQHASH(dqvp, id)];
	LIST_FOREACH(dq, dqh, dq_hash) {
		if (dq->dq_id != id ||
		    dq->dq_ump->um_quotas[dq->dq_type] != dqvp)
			continue;
		/*
		 * Another thread beat us allocating this dquot.
		 */
		KASSERT(dq->dq_cnt > 0);
		lfs_dqref(dq);
		mutex_exit(&lfs_dqlock);
		mutex_destroy(&ndq->dq_interlock);
		pool_cache_put(dquot_cache, ndq);
		*dqp = dq;
		return 0;
	}
	dq = ndq;
	LIST_INSERT_HEAD(dqh, dq, dq_hash);
	lfs_dqref(dq);
	mutex_enter(&dq->dq_interlock);
	mutex_exit(&lfs_dqlock);
#ifdef LFS_QUOTA
	if (fs->um_flags & ULFS_QUOTA)
		error = lfs_dq1get(dqvp, id, ump, type, dq);
#endif
#ifdef LFS_QUOTA2
	if (fs->um_flags & ULFS_QUOTA2)
		error = lfs_dq2get(dqvp, id, ump, type, dq);
#endif
	/*
	 * I/O error in reading quota file, release
	 * quota structure and reflect problem to caller.
	 */
	if (error) {
		mutex_enter(&lfs_dqlock);
		LIST_REMOVE(dq, dq_hash);
		mutex_exit(&lfs_dqlock);
		mutex_exit(&dq->dq_interlock);
		lfs_dqrele(vp, dq);
		*dqp = NODQUOT;
		return (error);
	}
	mutex_exit(&dq->dq_interlock);
	*dqp = dq;
	return (0);
}

/*
 * Obtain a reference to a dquot.
 */
void
lfs_dqref(struct dquot *dq)
{

	KASSERT(mutex_owned(&lfs_dqlock));
	dq->dq_cnt++;
	KASSERT(dq->dq_cnt > 0);
}

/*
 * Release a reference to a dquot.
 */
void
lfs_dqrele(struct vnode *vp, struct dquot *dq)
{

	if (dq == NODQUOT)
		return;
	mutex_enter(&dq->dq_interlock);
	for (;;) {
		mutex_enter(&lfs_dqlock);
		if (dq->dq_cnt > 1) {
			dq->dq_cnt--;
			mutex_exit(&lfs_dqlock);
			mutex_exit(&dq->dq_interlock);
			return;
		}
		if ((dq->dq_flags & DQ_MOD) == 0)
			break;
		mutex_exit(&lfs_dqlock);
#ifdef LFS_QUOTA
		if (dq->dq_ump->um_lfs->um_flags & ULFS_QUOTA)
			(void) lfs_dq1sync(vp, dq);
#endif
#ifdef LFS_QUOTA2
		if (dq->dq_ump->um_lfs->um_flags & ULFS_QUOTA2)
			(void) lfs_dq2sync(vp, dq);
#endif
	}
	KASSERT(dq->dq_cnt == 1 && (dq->dq_flags & DQ_MOD) == 0);
	LIST_REMOVE(dq, dq_hash);
	mutex_exit(&lfs_dqlock);
	mutex_exit(&dq->dq_interlock);
	mutex_destroy(&dq->dq_interlock);
	pool_cache_put(dquot_cache, dq);
}

int
lfs_qsync(struct mount *mp)
{
	struct ulfsmount *ump = VFSTOULFS(mp);
	struct lfs *fs = ump->um_lfs;

	/* avoid compiler warning when quotas aren't enabled */
	(void)ump;
	(void)fs;

#ifdef LFS_QUOTA
	if (fs->um_flags & ULFS_QUOTA)
		return lfs_q1sync(mp);
#endif
#ifdef LFS_QUOTA2
	if (fs->um_flags & ULFS_QUOTA2)
		return lfs_q2sync(mp);
#endif
	return 0;
}

#ifdef DIAGNOSTIC
/*
 * Check the hash chains for stray dquot's.
 */
void
lfs_dqflush(struct vnode *vp)
{
	struct dquot *dq;
	int i;

	mutex_enter(&lfs_dqlock);
	for (i = 0; i <= dqhash; i++)
		LIST_FOREACH(dq, &dqhashtbl[i], dq_hash)
			KASSERT(dq->dq_ump->um_quotas[dq->dq_type] != vp);
	mutex_exit(&lfs_dqlock);
}
#endif

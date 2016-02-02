/*	$NetBSD: ulfs_quota2.c,v 1.21 2015/07/28 05:09:35 dholland Exp $	*/
/*  from NetBSD: ufs_quota2.c,v 1.35 2012/09/27 07:47:56 bouyer Exp  */
/*  from NetBSD: ffs_quota2.c,v 1.4 2011/06/12 03:36:00 rmind Exp  */

/*-
  * Copyright (c) 2010 Manuel Bouyer
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ulfs_quota2.c,v 1.21 2015/07/28 05:09:35 dholland Exp $");

#include <sys/buf.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/fstrans.h>
#include <sys/kauth.h>
#include <sys/wapbl.h>
#include <sys/quota.h>
#include <sys/quotactl.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_accessors.h>
#include <ufs/lfs/lfs_extern.h>

#include <ufs/lfs/ulfs_quota2.h>
#include <ufs/lfs/ulfs_inode.h>
#include <ufs/lfs/ulfsmount.h>
#include <ufs/lfs/ulfs_bswap.h>
#include <ufs/lfs/ulfs_extern.h>
#include <ufs/lfs/ulfs_quota.h>

/*
 * LOCKING:
 * Data in the entries are protected by the associated struct dquot's
 * dq_interlock (this means we can't read or change a quota entry without
 * grabing a dquot for it).
 * The header and lists (including pointers in the data entries, and q2e_uid)
 * are protected by the global dqlock.
 * the locking order is dq_interlock -> dqlock
 */

static int quota2_bwrite(struct mount *, struct buf *);
static int getinoquota2(struct inode *, bool, bool, struct buf **,
    struct quota2_entry **);
static int getq2h(struct ulfsmount *, int, struct buf **,
    struct quota2_header **, int);
static int getq2e(struct ulfsmount *, int, daddr_t, int, struct buf **,
    struct quota2_entry **, int);
static int quota2_walk_list(struct ulfsmount *, struct buf *, int,
    uint64_t *, int, void *,
    int (*func)(struct ulfsmount *, uint64_t *, struct quota2_entry *,
      uint64_t, void *));

static const char *limnames[] = INITQLNAMES;

static void
quota2_dict_update_q2e_limits(int objtype, const struct quotaval *val,
    struct quota2_entry *q2e)
{
	/* make sure we can index q2e_val[] by the fs-independent objtype */
	CTASSERT(QUOTA_OBJTYPE_BLOCKS == QL_BLOCK);
	CTASSERT(QUOTA_OBJTYPE_FILES == QL_FILE);

	q2e->q2e_val[objtype].q2v_hardlimit = val->qv_hardlimit;
	q2e->q2e_val[objtype].q2v_softlimit = val->qv_softlimit;
	q2e->q2e_val[objtype].q2v_grace = val->qv_grace;
}

/*
 * Convert internal representation to FS-independent representation.
 * (Note that while the two types are currently identical, the
 * internal representation is an on-disk struct and the FS-independent
 * representation is not, and they might diverge in the future.)
 */
static void
q2val_to_quotaval(struct quota2_val *q2v, struct quotaval *qv)
{
	qv->qv_softlimit = q2v->q2v_softlimit;
	qv->qv_hardlimit = q2v->q2v_hardlimit;
	qv->qv_usage = q2v->q2v_cur;
	qv->qv_expiretime = q2v->q2v_time;
	qv->qv_grace = q2v->q2v_grace;
}

/*
 * Convert a quota2entry and default-flag to the FS-independent
 * representation.
 */
static void
q2e_to_quotaval(struct quota2_entry *q2e, int def,
	       id_t *id, int objtype, struct quotaval *ret)
{
	if (def) {
		*id = QUOTA_DEFAULTID;
	} else {
		*id = q2e->q2e_uid;
	}

	KASSERT(objtype >= 0 && objtype < N_QL);
	q2val_to_quotaval(&q2e->q2e_val[objtype], ret);
}


static int
quota2_bwrite(struct mount *mp, struct buf *bp)
{
	if (mp->mnt_flag & MNT_SYNCHRONOUS)
		return bwrite(bp);
	else {
		bdwrite(bp);
		return 0;
	}
}

static int
getq2h(struct ulfsmount *ump, int type,
    struct buf **bpp, struct quota2_header **q2hp, int flags)
{
	struct lfs *fs = ump->um_lfs;
	const int needswap = ULFS_MPNEEDSWAP(fs);
	int error;
	struct buf *bp;
	struct quota2_header *q2h;

	KASSERT(mutex_owned(&lfs_dqlock));
	error = bread(ump->um_quotas[type], 0, ump->umq2_bsize, flags, &bp);
	if (error)
		return error;
	if (bp->b_resid != 0) 
		panic("dq2get: %s quota file truncated", lfs_quotatypes[type]);

	q2h = (void *)bp->b_data;
	if (ulfs_rw32(q2h->q2h_magic_number, needswap) != Q2_HEAD_MAGIC ||
	    q2h->q2h_type != type)
		panic("dq2get: corrupted %s quota header", lfs_quotatypes[type]);
	*bpp = bp;
	*q2hp = q2h;
	return 0;
}

static int
getq2e(struct ulfsmount *ump, int type, daddr_t lblkno, int blkoffset,
    struct buf **bpp, struct quota2_entry **q2ep, int flags)
{
	int error;
	struct buf *bp;

	if (blkoffset & (sizeof(uint64_t) - 1)) {
		panic("dq2get: %s quota file corrupted",
		    lfs_quotatypes[type]);
	}
	error = bread(ump->um_quotas[type], lblkno, ump->umq2_bsize, flags, &bp);
	if (error)
		return error;
	if (bp->b_resid != 0) {
		panic("dq2get: %s quota file corrupted",
		    lfs_quotatypes[type]);
	}
	*q2ep = (void *)((char *)bp->b_data + blkoffset);
	*bpp = bp;
	return 0;
}

/* walk a quota entry list, calling the callback for each entry */
#define Q2WL_ABORT 0x10000000

static int
quota2_walk_list(struct ulfsmount *ump, struct buf *hbp, int type,
    uint64_t *offp, int flags, void *a,
    int (*func)(struct ulfsmount *, uint64_t *, struct quota2_entry *, uint64_t, void *))
{
	struct lfs *fs = ump->um_lfs;
	const int needswap = ULFS_MPNEEDSWAP(fs);
	daddr_t off = ulfs_rw64(*offp, needswap);
	struct buf *bp, *obp = hbp;
	int ret = 0, ret2 = 0;
	struct quota2_entry *q2e;
	daddr_t lblkno, blkoff, olblkno = 0;

	KASSERT(mutex_owner(&lfs_dqlock));

	while (off != 0) {
		lblkno = (off >> ump->um_mountp->mnt_fs_bshift);
		blkoff = (off & ump->umq2_bmask);
		if (lblkno == 0) {
			/* in the header block */
			bp = hbp;
		} else if (lblkno == olblkno) {
			/* still in the same buf */
			bp = obp;
		} else {
			ret = bread(ump->um_quotas[type], lblkno, 
			    ump->umq2_bsize, flags, &bp);
			if (ret)
				return ret;
			if (bp->b_resid != 0) {
				panic("quota2_walk_list: %s quota file corrupted",
				    lfs_quotatypes[type]);
			}
		}
		q2e = (void *)((char *)(bp->b_data) + blkoff);
		ret = (*func)(ump, offp, q2e, off, a);
		if (off != ulfs_rw64(*offp, needswap)) {
			/* callback changed parent's pointer, redo */
			off = ulfs_rw64(*offp, needswap);
			if (bp != hbp && bp != obp)
				ret2 = bwrite(bp);
		} else {
			/* parent if now current */
			if (obp != bp && obp != hbp) {
				if (flags & B_MODIFY)
					ret2 = bwrite(obp);
				else
					brelse(obp, 0);
			}
			obp = bp;
			olblkno = lblkno;
			offp = &(q2e->q2e_next);
			off = ulfs_rw64(*offp, needswap);
		}
		if (ret)
			break;
		if (ret2) {
			ret = ret2;
			break;
		}
	}
	if (obp != hbp) {
		if (flags & B_MODIFY)
			ret2 = bwrite(obp);
		else
			brelse(obp, 0);
	}
	if (ret & Q2WL_ABORT)
		return 0;
	if (ret == 0)
		return ret2;
	return ret;
}

int
lfsquota2_umount(struct mount *mp, int flags)
{
	int i, error;
	struct ulfsmount *ump = VFSTOULFS(mp);
	struct lfs *fs = ump->um_lfs;

	if ((fs->um_flags & ULFS_QUOTA2) == 0)
		return 0;

	for (i = 0; i < ULFS_MAXQUOTAS; i++) {
		if (ump->um_quotas[i] != NULLVP) {
			error = vn_close(ump->um_quotas[i], FREAD|FWRITE,
			    ump->um_cred[i]);
			if (error) {
				printf("quota2_umount failed: close(%p) %d\n",
				    ump->um_quotas[i], error);
				return error;
			}
		}
		ump->um_quotas[i] = NULLVP;
	}
	return 0;
}

static int 
quota2_q2ealloc(struct ulfsmount *ump, int type, uid_t uid, struct dquot *dq)
{
	int error, error2;
	struct buf *hbp, *bp;
	struct quota2_header *q2h;
	struct quota2_entry *q2e;
	daddr_t offset;
	u_long hash_mask;
	struct lfs *fs = ump->um_lfs;
	const int needswap = ULFS_MPNEEDSWAP(fs);

	KASSERT(mutex_owned(&dq->dq_interlock));
	KASSERT(mutex_owned(&lfs_dqlock));
	error = getq2h(ump, type, &hbp, &q2h, B_MODIFY);
	if (error)
		return error;
	offset = ulfs_rw64(q2h->q2h_free, needswap);
	if (offset == 0) {
		struct vnode *vp = ump->um_quotas[type];
		struct inode *ip = VTOI(vp);
		uint64_t size = ip->i_size;
		/* need to alocate a new disk block */
		error = lfs_balloc(vp, size, ump->umq2_bsize,
		    ump->um_cred[type], B_CLRBUF | B_SYNC, &bp);
		if (error) {
			brelse(hbp, 0);
			return error;
		}
		KASSERT((ip->i_size % ump->umq2_bsize) == 0);
		ip->i_size += ump->umq2_bsize;
		DIP_ASSIGN(ip, size, ip->i_size);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		uvm_vnp_setsize(vp, ip->i_size);
		lfsquota2_addfreeq2e(q2h, bp->b_data, size, ump->umq2_bsize,
		    needswap);
		error = bwrite(bp);
		error2 = lfs_update(vp, NULL, NULL, UPDATE_WAIT);
		if (error || error2) {
			brelse(hbp, 0);
			if (error)
				return error;
			return error2;
		}
		offset = ulfs_rw64(q2h->q2h_free, needswap);
		KASSERT(offset != 0);
	}
	dq->dq2_lblkno = (offset >> ump->um_mountp->mnt_fs_bshift);
	dq->dq2_blkoff = (offset & ump->umq2_bmask);
	if (dq->dq2_lblkno == 0) {
		bp = hbp;
		q2e = (void *)((char *)bp->b_data + dq->dq2_blkoff);
	} else {
		error = getq2e(ump, type, dq->dq2_lblkno,
		    dq->dq2_blkoff, &bp, &q2e, B_MODIFY);
		if (error) {
			brelse(hbp, 0);
			return error;
		}
	}
	hash_mask = ((1 << q2h->q2h_hash_shift) - 1);
	/* remove from free list */
	q2h->q2h_free = q2e->q2e_next;

	memcpy(q2e, &q2h->q2h_defentry, sizeof(*q2e));
	q2e->q2e_uid = ulfs_rw32(uid, needswap);
	/* insert in hash list */ 
	q2e->q2e_next = q2h->q2h_entries[uid & hash_mask];
	q2h->q2h_entries[uid & hash_mask] = ulfs_rw64(offset, needswap);
	if (hbp != bp) {
		bwrite(hbp);
	}
	bwrite(bp);
	return 0;
}

static int
getinoquota2(struct inode *ip, bool alloc, bool modify, struct buf **bpp,
    struct quota2_entry **q2ep)
{
	int error;
	int i;
	struct dquot *dq;
	struct ulfsmount *ump = ip->i_ump;
	u_int32_t ino_ids[ULFS_MAXQUOTAS];

	error = lfs_getinoquota(ip);
	if (error)
		return error;

        ino_ids[ULFS_USRQUOTA] = ip->i_uid;
        ino_ids[ULFS_GRPQUOTA] = ip->i_gid;
	/* first get the interlock for all dquot */
	for (i = 0; i < ULFS_MAXQUOTAS; i++) {
		dq = ip->i_dquot[i];
		if (dq == NODQUOT)
			continue;
		mutex_enter(&dq->dq_interlock);
	}
	/* now get the corresponding quota entry */
	for (i = 0; i < ULFS_MAXQUOTAS; i++) {
		bpp[i] = NULL;
		q2ep[i] = NULL;
		dq = ip->i_dquot[i];
		if (dq == NODQUOT)
			continue;
		if (__predict_false(ump->um_quotas[i] == NULL)) {
			/*
			 * quotas have been turned off. This can happen
			 * at umount time.
			 */
			mutex_exit(&dq->dq_interlock);
			lfs_dqrele(NULLVP, dq);
			ip->i_dquot[i] = NULL;
			continue;
		}

		if ((dq->dq2_lblkno | dq->dq2_blkoff) == 0) {
			if (!alloc) {
				continue;
			}
			/* need to alloc a new on-disk quot */
			mutex_enter(&lfs_dqlock);
			error = quota2_q2ealloc(ump, i, ino_ids[i], dq);
			mutex_exit(&lfs_dqlock);
			if (error)
				return error;
		}
		KASSERT(dq->dq2_lblkno != 0 || dq->dq2_blkoff != 0);
		error = getq2e(ump, i, dq->dq2_lblkno,
		    dq->dq2_blkoff, &bpp[i], &q2ep[i],
		    modify ? B_MODIFY : 0);
		if (error)
			return error;
	}
	return 0;
}

__inline static int __unused
lfsquota2_check_limit(struct quota2_val *q2v, uint64_t change, time_t now)
{
	return lfsquota_check_limit(q2v->q2v_cur, change, q2v->q2v_softlimit,
	    q2v->q2v_hardlimit, q2v->q2v_time, now);
}

static int
quota2_check(struct inode *ip, int vtype, int64_t change, kauth_cred_t cred,
    int flags)
{
	int error;
	struct buf *bp[ULFS_MAXQUOTAS];
	struct quota2_entry *q2e[ULFS_MAXQUOTAS];
	struct quota2_val *q2vp;
	struct dquot *dq;
	uint64_t ncurblks;
	struct ulfsmount *ump = ip->i_ump;
	struct lfs *fs = ip->i_lfs;
	struct mount *mp = ump->um_mountp;
	const int needswap = ULFS_MPNEEDSWAP(fs);
	int i;

	if ((error = getinoquota2(ip, change > 0, change != 0, bp, q2e)) != 0)
		return error;
	if (change == 0) {
		for (i = 0; i < ULFS_MAXQUOTAS; i++) {
			dq = ip->i_dquot[i];
			if (dq == NODQUOT)
				continue;
			if (bp[i])
				brelse(bp[i], 0);
			mutex_exit(&dq->dq_interlock);
		}
		return 0;
	}
	if (change < 0) {
		for (i = 0; i < ULFS_MAXQUOTAS; i++) {
			dq = ip->i_dquot[i];
			if (dq == NODQUOT)
				continue;
			if (q2e[i] == NULL) {
				mutex_exit(&dq->dq_interlock);
				continue;
			}
			q2vp = &q2e[i]->q2e_val[vtype];
			ncurblks = ulfs_rw64(q2vp->q2v_cur, needswap);
			if (ncurblks < -change)
				ncurblks = 0;
			else
				ncurblks += change;
			q2vp->q2v_cur = ulfs_rw64(ncurblks, needswap);
			quota2_bwrite(mp, bp[i]);
			mutex_exit(&dq->dq_interlock);
		}
		return 0;
	}
	/* see if the allocation is allowed */
	for (i = 0; i < ULFS_MAXQUOTAS; i++) {
		struct quota2_val q2v;
		int ql_stat;
		dq = ip->i_dquot[i];
		if (dq == NODQUOT)
			continue;
		KASSERT(q2e[i] != NULL);
		lfsquota2_ulfs_rwq2v(&q2e[i]->q2e_val[vtype], &q2v, needswap);
		ql_stat = lfsquota2_check_limit(&q2v, change, time_second);

		if ((flags & FORCE) == 0 &&
		    kauth_authorize_system(cred, KAUTH_SYSTEM_FS_QUOTA,
		    KAUTH_REQ_SYSTEM_FS_QUOTA_NOLIMIT,
		    KAUTH_ARG(i), KAUTH_ARG(vtype), NULL) != 0) {
			/* enforce this limit */
			switch(QL_STATUS(ql_stat)) {
			case QL_S_DENY_HARD:
				if ((dq->dq_flags & DQ_WARN(vtype)) == 0) {
					uprintf("\n%s: write failed, %s %s "
					    "limit reached\n",
					    mp->mnt_stat.f_mntonname,
					    lfs_quotatypes[i], limnames[vtype]);
					dq->dq_flags |= DQ_WARN(vtype);
				}
				error = EDQUOT;
				break;
			case QL_S_DENY_GRACE:
				if ((dq->dq_flags & DQ_WARN(vtype)) == 0) {
					uprintf("\n%s: write failed, %s %s "
					    "limit reached\n",
					    mp->mnt_stat.f_mntonname,
					    lfs_quotatypes[i], limnames[vtype]);
					dq->dq_flags |= DQ_WARN(vtype);
				}
				error = EDQUOT;
				break;
			case QL_S_ALLOW_SOFT:
				if ((dq->dq_flags & DQ_WARN(vtype)) == 0) {
					uprintf("\n%s: warning, %s %s "
					    "quota exceeded\n",
					    mp->mnt_stat.f_mntonname,
					    lfs_quotatypes[i], limnames[vtype]);
					dq->dq_flags |= DQ_WARN(vtype);
				}
				break;
			}
		}
		/*
		 * always do this; we don't know if the allocation will
		 * succed or not in the end. if we don't do the allocation
		 * q2v_time will be ignored anyway
		 */
		if (ql_stat & QL_F_CROSS) {
			q2v.q2v_time = time_second + q2v.q2v_grace;
			lfsquota2_ulfs_rwq2v(&q2v, &q2e[i]->q2e_val[vtype],
			    needswap);
		}
	}

	/* now do the allocation if allowed */
	for (i = 0; i < ULFS_MAXQUOTAS; i++) {
		dq = ip->i_dquot[i];
		if (dq == NODQUOT)
			continue;
		KASSERT(q2e[i] != NULL);
		if (error == 0) {
			q2vp = &q2e[i]->q2e_val[vtype];
			ncurblks = ulfs_rw64(q2vp->q2v_cur, needswap);
			q2vp->q2v_cur = ulfs_rw64(ncurblks + change, needswap);
			quota2_bwrite(mp, bp[i]);
		} else
			brelse(bp[i], 0);
		mutex_exit(&dq->dq_interlock);
	}
	return error;
}

int
lfs_chkdq2(struct inode *ip, int64_t change, kauth_cred_t cred, int flags)
{
	return quota2_check(ip, QL_BLOCK, change, cred, flags);
}

int
lfs_chkiq2(struct inode *ip, int32_t change, kauth_cred_t cred, int flags)
{
	return quota2_check(ip, QL_FILE, change, cred, flags);
}

int
lfsquota2_handle_cmd_put(struct ulfsmount *ump, const struct quotakey *key,
    const struct quotaval *val)
{
	int error;
	struct dquot *dq;
	struct quota2_header *q2h;
	struct quota2_entry q2e, *q2ep;
	struct buf *bp;
	struct lfs *fs = ump->um_lfs;
	const int needswap = ULFS_MPNEEDSWAP(fs);

	/* make sure we can index by the fs-independent idtype */
	CTASSERT(QUOTA_IDTYPE_USER == ULFS_USRQUOTA);
	CTASSERT(QUOTA_IDTYPE_GROUP == ULFS_GRPQUOTA);

	if (ump->um_quotas[key->qk_idtype] == NULLVP)
		return ENODEV;
	
	if (key->qk_id == QUOTA_DEFAULTID) {
		mutex_enter(&lfs_dqlock);
		error = getq2h(ump, key->qk_idtype, &bp, &q2h, B_MODIFY);
		if (error) {
			mutex_exit(&lfs_dqlock);
			goto out_wapbl;
		}
		lfsquota2_ulfs_rwq2e(&q2h->q2h_defentry, &q2e, needswap);
		quota2_dict_update_q2e_limits(key->qk_objtype, val, &q2e);
		lfsquota2_ulfs_rwq2e(&q2e, &q2h->q2h_defentry, needswap);
		mutex_exit(&lfs_dqlock);
		quota2_bwrite(ump->um_mountp, bp);
		goto out_wapbl;
	}

	error = lfs_dqget(NULLVP, key->qk_id, ump, key->qk_idtype, &dq);
	if (error)
		goto out_wapbl;

	mutex_enter(&dq->dq_interlock);
	if (dq->dq2_lblkno == 0 && dq->dq2_blkoff == 0) {
		/* need to alloc a new on-disk quot */
		mutex_enter(&lfs_dqlock);
		error = quota2_q2ealloc(ump, key->qk_idtype, key->qk_id, dq);
		mutex_exit(&lfs_dqlock);
		if (error)
			goto out_il;
	}
	KASSERT(dq->dq2_lblkno != 0 || dq->dq2_blkoff != 0);
	error = getq2e(ump, key->qk_idtype, dq->dq2_lblkno,
	    dq->dq2_blkoff, &bp, &q2ep, B_MODIFY);
	if (error)
		goto out_il;
	
	lfsquota2_ulfs_rwq2e(q2ep, &q2e, needswap);
	quota2_dict_update_q2e_limits(key->qk_objtype, val, &q2e);
	lfsquota2_ulfs_rwq2e(&q2e, q2ep, needswap);
	quota2_bwrite(ump->um_mountp, bp);

out_il:
	mutex_exit(&dq->dq_interlock);
	lfs_dqrele(NULLVP, dq);
out_wapbl:
	return error;
}

struct dq2clear_callback {
	uid_t id;
	struct dquot *dq;
	struct quota2_header *q2h;
};

static int
dq2clear_callback(struct ulfsmount *ump, uint64_t *offp, struct quota2_entry *q2e,
    uint64_t off, void *v)
{
	struct dq2clear_callback *c = v;
	struct lfs *fs = ump->um_lfs;
	const int needswap = ULFS_MPNEEDSWAP(fs);
	uint64_t myoff;

	if (ulfs_rw32(q2e->q2e_uid, needswap) == c->id) {
		KASSERT(mutex_owned(&c->dq->dq_interlock));
		c->dq->dq2_lblkno = 0;
		c->dq->dq2_blkoff = 0;
		myoff = *offp;
		/* remove from hash list */
		*offp = q2e->q2e_next;
		/* add to free list */
		q2e->q2e_next = c->q2h->q2h_free;
		c->q2h->q2h_free = myoff;
		return Q2WL_ABORT;
	}
	return 0;
}
int
lfsquota2_handle_cmd_del(struct ulfsmount *ump, const struct quotakey *qk)
{
	int idtype;
	id_t id;
	int objtype;
	int error, i, canfree;
	struct dquot *dq;
	struct quota2_header *q2h;
	struct quota2_entry q2e, *q2ep;
	struct buf *hbp, *bp;
	u_long hash_mask;
	struct dq2clear_callback c;

	idtype = qk->qk_idtype;
	id = qk->qk_id;
	objtype = qk->qk_objtype;

	if (ump->um_quotas[idtype] == NULLVP)
		return ENODEV;
	if (id == QUOTA_DEFAULTID)
		return EOPNOTSUPP;

	/* get the default entry before locking the entry's buffer */
	mutex_enter(&lfs_dqlock);
	error = getq2h(ump, idtype, &hbp, &q2h, 0);
	if (error) {
		mutex_exit(&lfs_dqlock);
		return error;
	}
	/* we'll copy to another disk entry, so no need to swap */
	memcpy(&q2e, &q2h->q2h_defentry, sizeof(q2e));
	mutex_exit(&lfs_dqlock);
	brelse(hbp, 0);

	error = lfs_dqget(NULLVP, id, ump, idtype, &dq);
	if (error)
		return error;

	mutex_enter(&dq->dq_interlock);
	if (dq->dq2_lblkno == 0 && dq->dq2_blkoff == 0) {
		/* already clear, nothing to do */
		error = ENOENT;
		goto out_il;
	}

	error = getq2e(ump, idtype, dq->dq2_lblkno, dq->dq2_blkoff,
	    &bp, &q2ep, B_MODIFY);
	if (error)
		goto out_wapbl;

	/* make sure we can index by the objtype passed in */
	CTASSERT(QUOTA_OBJTYPE_BLOCKS == QL_BLOCK);
	CTASSERT(QUOTA_OBJTYPE_FILES == QL_FILE);

	/* clear the requested objtype by copying from the default entry */
	q2ep->q2e_val[objtype].q2v_softlimit =
		q2e.q2e_val[objtype].q2v_softlimit;
	q2ep->q2e_val[objtype].q2v_hardlimit =
		q2e.q2e_val[objtype].q2v_hardlimit;
	q2ep->q2e_val[objtype].q2v_grace =
		q2e.q2e_val[objtype].q2v_grace;
	q2ep->q2e_val[objtype].q2v_time = 0;

	/* if this entry now contains no information, we can free it */
	canfree = 1;
	for (i = 0; i < N_QL; i++) {
		if (q2ep->q2e_val[i].q2v_cur != 0 ||
		    (q2ep->q2e_val[i].q2v_softlimit != 
		     q2e.q2e_val[i].q2v_softlimit) ||
		    (q2ep->q2e_val[i].q2v_hardlimit != 
		     q2e.q2e_val[i].q2v_hardlimit) ||
		    (q2ep->q2e_val[i].q2v_grace != 
		     q2e.q2e_val[i].q2v_grace)) {
			canfree = 0;
			break;
		}
		/* note: do not need to check q2v_time */
	}

	if (canfree == 0) {
		quota2_bwrite(ump->um_mountp, bp);
		goto out_wapbl;
	}
	/* we can free it. release bp so we can walk the list */
	brelse(bp, 0);
	mutex_enter(&lfs_dqlock);
	error = getq2h(ump, idtype, &hbp, &q2h, 0);
	if (error)
		goto out_dqlock;

	hash_mask = ((1 << q2h->q2h_hash_shift) - 1);
	c.dq = dq;
	c.id = id;
	c.q2h = q2h;
	error = quota2_walk_list(ump, hbp, idtype,
	    &q2h->q2h_entries[id & hash_mask], B_MODIFY, &c,
	    dq2clear_callback);

	bwrite(hbp);

out_dqlock:
	mutex_exit(&lfs_dqlock);
out_wapbl:
out_il:
	mutex_exit(&dq->dq_interlock);
	lfs_dqrele(NULLVP, dq);
	return error;
}

static int
quota2_fetch_q2e(struct ulfsmount *ump, const struct quotakey *qk,
    struct quota2_entry *ret)
{
	struct dquot *dq;
	int error;
	struct quota2_entry *q2ep;
	struct buf *bp;
	struct lfs *fs = ump->um_lfs;
	const int needswap = ULFS_MPNEEDSWAP(fs);

	error = lfs_dqget(NULLVP, qk->qk_id, ump, qk->qk_idtype, &dq);
	if (error)
		return error;

	mutex_enter(&dq->dq_interlock);
	if (dq->dq2_lblkno == 0 && dq->dq2_blkoff == 0) {
		mutex_exit(&dq->dq_interlock);
		lfs_dqrele(NULLVP, dq);
		return ENOENT;
	}
	error = getq2e(ump, qk->qk_idtype, dq->dq2_lblkno, dq->dq2_blkoff,
	    &bp, &q2ep, 0);
	if (error) {
		mutex_exit(&dq->dq_interlock);
		lfs_dqrele(NULLVP, dq);
		return error;
	}
	lfsquota2_ulfs_rwq2e(q2ep, ret, needswap);
	brelse(bp, 0);
	mutex_exit(&dq->dq_interlock);
	lfs_dqrele(NULLVP, dq);

	return 0;
}

static int
quota2_fetch_quotaval(struct ulfsmount *ump, const struct quotakey *qk,
    struct quotaval *ret)
{
	struct dquot *dq;
	int error;
	struct quota2_entry *q2ep, q2e;
	struct buf  *bp;
	struct lfs *fs = ump->um_lfs;
	const int needswap = ULFS_MPNEEDSWAP(fs);
	id_t id2;

	error = lfs_dqget(NULLVP, qk->qk_id, ump, qk->qk_idtype, &dq);
	if (error)
		return error;

	mutex_enter(&dq->dq_interlock);
	if (dq->dq2_lblkno == 0 && dq->dq2_blkoff == 0) {
		mutex_exit(&dq->dq_interlock);
		lfs_dqrele(NULLVP, dq);
		return ENOENT;
	}
	error = getq2e(ump, qk->qk_idtype, dq->dq2_lblkno, dq->dq2_blkoff,
	    &bp, &q2ep, 0);
	if (error) {
		mutex_exit(&dq->dq_interlock);
		lfs_dqrele(NULLVP, dq);
		return error;
	}
	lfsquota2_ulfs_rwq2e(q2ep, &q2e, needswap);
	brelse(bp, 0);
	mutex_exit(&dq->dq_interlock);
	lfs_dqrele(NULLVP, dq);

	q2e_to_quotaval(&q2e, 0, &id2, qk->qk_objtype, ret);
	KASSERT(id2 == qk->qk_id);
	return 0;
}

int
lfsquota2_handle_cmd_get(struct ulfsmount *ump, const struct quotakey *qk,
    struct quotaval *qv)
{
	int error;
	struct quota2_header *q2h;
	struct quota2_entry q2e;
	struct buf *bp;
	struct lfs *fs = ump->um_lfs;
	const int needswap = ULFS_MPNEEDSWAP(fs);
	id_t id2;

	/*
	 * Make sure the FS-independent codes match the internal ones,
	 * so we can use the passed-in objtype without having to
	 * convert it explicitly to QL_BLOCK/QL_FILE.
	 */
	CTASSERT(QL_BLOCK == QUOTA_OBJTYPE_BLOCKS);
	CTASSERT(QL_FILE == QUOTA_OBJTYPE_FILES);
	CTASSERT(N_QL == 2);

	if (qk->qk_objtype < 0 || qk->qk_objtype >= N_QL) {
		return EINVAL;
	}

	if (ump->um_quotas[qk->qk_idtype] == NULLVP)
		return ENODEV;
	if (qk->qk_id == QUOTA_DEFAULTID) {
		mutex_enter(&lfs_dqlock);
		error = getq2h(ump, qk->qk_idtype, &bp, &q2h, 0);
		if (error) {
			mutex_exit(&lfs_dqlock);
			return error;
		}
		lfsquota2_ulfs_rwq2e(&q2h->q2h_defentry, &q2e, needswap);
		mutex_exit(&lfs_dqlock);
		brelse(bp, 0);
		q2e_to_quotaval(&q2e, qk->qk_id == QUOTA_DEFAULTID, &id2,
				qk->qk_objtype, qv);
		(void)id2;
	} else
		error = quota2_fetch_quotaval(ump, qk, qv);
	
	return error;
}

/*
 * Cursor structure we used.
 *
 * This will get stored in userland between calls so we must not assume
 * it isn't arbitrarily corrupted.
 */
struct ulfsq2_cursor {
	uint32_t q2c_magic;	/* magic number */
	int q2c_hashsize;	/* size of hash table at last go */

	int q2c_users_done;	/* true if we've returned all user data */
	int q2c_groups_done;	/* true if we've returned all group data */
	int q2c_defaults_done;	/* true if we've returned the default values */
	int q2c_hashpos;	/* slot to start at in hash table */
	int q2c_uidpos;		/* number of ids we've handled */
	int q2c_blocks_done;	/* true if we've returned the blocks value */
};

/*
 * State of a single cursorget call, or at least the part of it that
 * needs to be passed around.
 */
struct q2cursor_state {
	/* data return pointers */
	struct quotakey *keys;
	struct quotaval *vals;

	/* key/value counters */
	unsigned maxkeyvals;
	unsigned numkeys;	/* number of keys assigned */

	/* ID to key/value conversion state */
	int skipfirst;		/* if true skip first key/value */
	int skiplast;		/* if true skip last key/value */

	/* ID counters */
	unsigned maxids;	/* maximum number of IDs to handle */
	unsigned numids;	/* number of IDs handled */
};

/*
 * Additional structure for getids callback.
 */
struct q2cursor_getids {
	struct q2cursor_state *state;
	int idtype;
	unsigned skip;		/* number of ids to skip over */
	unsigned new_skip;	/* number of ids to skip over next time */
	unsigned skipped;	/* number skipped so far */
	int stopped;		/* true if we stopped quota_walk_list early */
};

/*
 * Cursor-related functions
 */

/* magic number */
#define Q2C_MAGIC (0xbeebe111)

/* extract cursor from caller form */
#define Q2CURSOR(qkc) ((struct ulfsq2_cursor *)&qkc->u.qkc_space[0])

/*
 * Check that a cursor we're handed is something like valid. If
 * someone munges it and it still passes these checks, they'll get
 * partial or odd results back but won't break anything.
 */
static int
q2cursor_check(struct ulfsq2_cursor *cursor)
{
	if (cursor->q2c_magic != Q2C_MAGIC) {
		return EINVAL;
	}
	if (cursor->q2c_hashsize < 0) {
		return EINVAL;
	}

	if (cursor->q2c_users_done != 0 && cursor->q2c_users_done != 1) {
		return EINVAL;
	}
	if (cursor->q2c_groups_done != 0 && cursor->q2c_groups_done != 1) {
		return EINVAL;
	}
	if (cursor->q2c_defaults_done != 0 && cursor->q2c_defaults_done != 1) {
		return EINVAL;
	}
	if (cursor->q2c_hashpos < 0 || cursor->q2c_uidpos < 0) {
		return EINVAL;
	}
	if (cursor->q2c_blocks_done != 0 && cursor->q2c_blocks_done != 1) {
		return EINVAL;
	}
	return 0;
}

/*
 * Set up the q2cursor state.
 */
static void
q2cursor_initstate(struct q2cursor_state *state, struct quotakey *keys,
    struct quotaval *vals, unsigned maxkeyvals, int blocks_done)
{
	state->keys = keys;
	state->vals = vals;

	state->maxkeyvals = maxkeyvals;
	state->numkeys = 0;

	/*
	 * For each ID there are two quotavals to return. If the
	 * maximum number of entries to return is odd, we might want
	 * to skip the first quotaval of the first ID, or the last
	 * quotaval of the last ID, but not both. So the number of IDs
	 * we want is (up to) half the number of return slots we have,
	 * rounded up.
	 */

	state->maxids = (state->maxkeyvals + 1) / 2;
	state->numids = 0;
	if (state->maxkeyvals % 2) {
		if (blocks_done) {
			state->skipfirst = 1;
			state->skiplast = 0;
		} else {
			state->skipfirst = 0;
			state->skiplast = 1;
		}
	} else {
		state->skipfirst = 0;
		state->skiplast = 0;
	}
}

/*
 * Choose which idtype we're going to work on. If doing a full
 * iteration, we do users first, then groups, but either might be
 * disabled or marked to skip via cursorsetidtype(), so don't make
 * silly assumptions.
 */
static int
q2cursor_pickidtype(struct ulfsq2_cursor *cursor, int *idtype_ret)
{
	if (cursor->q2c_users_done == 0) {
		*idtype_ret = QUOTA_IDTYPE_USER;
	} else if (cursor->q2c_groups_done == 0) {
		*idtype_ret = QUOTA_IDTYPE_GROUP;
	} else {
		return EAGAIN;
	}
	return 0;
}

/*
 * Add an ID to the current state. Sets up either one or two keys to
 * refer to it, depending on whether it's first/last and the setting
 * of skipfirst. (skiplast does not need to be explicitly tested)
 */
static void
q2cursor_addid(struct q2cursor_state *state, int idtype, id_t id)
{
	KASSERT(state->numids < state->maxids);
	KASSERT(state->numkeys < state->maxkeyvals);

	if (!state->skipfirst || state->numkeys > 0) {
		state->keys[state->numkeys].qk_idtype = idtype;
		state->keys[state->numkeys].qk_id = id;
		state->keys[state->numkeys].qk_objtype = QUOTA_OBJTYPE_BLOCKS;
		state->numkeys++;
	}
	if (state->numkeys < state->maxkeyvals) {
		state->keys[state->numkeys].qk_idtype = idtype;
		state->keys[state->numkeys].qk_id = id;
		state->keys[state->numkeys].qk_objtype = QUOTA_OBJTYPE_FILES;
		state->numkeys++;
	} else {
		KASSERT(state->skiplast);
	}
	state->numids++;
}

/*
 * Callback function for getting IDs. Update counting and call addid.
 */
static int
q2cursor_getids_callback(struct ulfsmount *ump, uint64_t *offp,
    struct quota2_entry *q2ep, uint64_t off, void *v)
{
	struct q2cursor_getids *gi = v;
	id_t id;
	struct lfs *fs = ump->um_lfs;
	const int needswap = ULFS_MPNEEDSWAP(fs);

	if (gi->skipped < gi->skip) {
		gi->skipped++;
		return 0;
	}
	id = ulfs_rw32(q2ep->q2e_uid, needswap);
	q2cursor_addid(gi->state, gi->idtype, id);
	gi->new_skip++;
	if (gi->state->numids >= gi->state->maxids) {
		/* got enough ids, stop now */
		gi->stopped = 1;
		return Q2WL_ABORT;
	}
	return 0;
}

/*
 * Fill in a batch of quotakeys by scanning one or more hash chains.
 */
static int
q2cursor_getkeys(struct ulfsmount *ump, int idtype, struct ulfsq2_cursor *cursor,
    struct q2cursor_state *state,
    int *hashsize_ret, struct quota2_entry *default_q2e_ret)
{
	struct lfs *fs = ump->um_lfs;
	const int needswap = ULFS_MPNEEDSWAP(fs);
	struct buf *hbp;
	struct quota2_header *q2h;
	int quota2_hash_size;
	struct q2cursor_getids gi;
	uint64_t offset;
	int error;

	/*
	 * Read the header block.
	 */

	mutex_enter(&lfs_dqlock);
	error = getq2h(ump, idtype, &hbp, &q2h, 0);
	if (error) {
		mutex_exit(&lfs_dqlock);
		return error;
	}

	/* if the table size has changed, make the caller start over */
	quota2_hash_size = ulfs_rw16(q2h->q2h_hash_size, needswap);
	if (cursor->q2c_hashsize == 0) {
		cursor->q2c_hashsize = quota2_hash_size;
	} else if (cursor->q2c_hashsize != quota2_hash_size) {
		error = EDEADLK;
		goto scanfail;
	}

	/* grab the entry with the default values out of the header */
	lfsquota2_ulfs_rwq2e(&q2h->q2h_defentry, default_q2e_ret, needswap);

	/* If we haven't done the defaults yet, that goes first. */
	if (cursor->q2c_defaults_done == 0) {
		q2cursor_addid(state, idtype, QUOTA_DEFAULTID);
		/* if we read both halves, mark it done */
		if (state->numids < state->maxids || !state->skiplast) {
			cursor->q2c_defaults_done = 1;
		}
	}

	gi.state = state;
	gi.idtype = idtype;

	while (state->numids < state->maxids) {
		if (cursor->q2c_hashpos >= quota2_hash_size) {
			/* nothing more left */
			break;
		}

		/* scan this hash chain */
		gi.skip = cursor->q2c_uidpos;
		gi.new_skip = gi.skip;
		gi.skipped = 0;
		gi.stopped = 0;
		offset = q2h->q2h_entries[cursor->q2c_hashpos];

		error = quota2_walk_list(ump, hbp, idtype, &offset, 0, &gi,
		    q2cursor_getids_callback);
		KASSERT(error != Q2WL_ABORT);
		if (error) {
			break;
		}
		if (gi.stopped) {
			/* callback stopped before reading whole chain */
			cursor->q2c_uidpos = gi.new_skip;
			/* if we didn't get both halves, back up */
			if (state->numids == state->maxids && state->skiplast){
				KASSERT(cursor->q2c_uidpos > 0);
				cursor->q2c_uidpos--;
			}
		} else {
			/* read whole chain */
			/* if we got both halves of the last id, advance */
			if (state->numids < state->maxids || !state->skiplast){
				cursor->q2c_uidpos = 0;
				cursor->q2c_hashpos++;
			}
		}
	}

scanfail:
	mutex_exit(&lfs_dqlock);
	brelse(hbp, 0);
	if (error)
		return error;

	*hashsize_ret = quota2_hash_size;
	return 0;
}

/*
 * Fetch the quotavals for the quotakeys.
 */
static int
q2cursor_getvals(struct ulfsmount *ump, struct q2cursor_state *state,
    const struct quota2_entry *default_q2e)
{
	int hasid;
	id_t loadedid, id;
	unsigned pos;
	struct quota2_entry q2e;
	int objtype;
	int error;

	hasid = 0;
	loadedid = 0;
	for (pos = 0; pos < state->numkeys; pos++) {
		id = state->keys[pos].qk_id;
		if (!hasid || id != loadedid) {
			hasid = 1;
			loadedid = id;
			if (id == QUOTA_DEFAULTID) {
				q2e = *default_q2e;
			} else {
				error = quota2_fetch_q2e(ump,
							 &state->keys[pos],
							 &q2e);
				if (error == ENOENT) {
					/* something changed - start over */
					error = EDEADLK;
				}
				if (error) {
					return error;
				}
 			}
		}


		objtype = state->keys[pos].qk_objtype;
		KASSERT(objtype >= 0 && objtype < N_QL);
		q2val_to_quotaval(&q2e.q2e_val[objtype], &state->vals[pos]);
	}

	return 0;
}

/*
 * Handle cursorget.
 *
 * We can't just read keys and values directly, because we can't walk
 * the list with qdlock and grab dq_interlock to read the entries at
 * the same time. So we're going to do two passes: one to figure out
 * which IDs we want and fill in the keys, and then a second to use
 * the keys to fetch the values.
 */
int
lfsquota2_handle_cmd_cursorget(struct ulfsmount *ump, struct quotakcursor *qkc,
    struct quotakey *keys, struct quotaval *vals, unsigned maxreturn,
    unsigned *ret)
{
	int error;
	struct ulfsq2_cursor *cursor;
	struct ulfsq2_cursor newcursor;
	struct q2cursor_state state;
	struct quota2_entry default_q2e;
	int idtype;
	int quota2_hash_size = 0; /* XXXuninit */

	/*
	 * Convert and validate the cursor.
	 */
	cursor = Q2CURSOR(qkc);
	error = q2cursor_check(cursor);
	if (error) {
		return error;
	}

	/*
	 * Make sure our on-disk codes match the values of the
	 * FS-independent ones. This avoids the need for explicit
	 * conversion (which would be a NOP anyway and thus easily
	 * left out or called in the wrong places...)
	 */
	CTASSERT(QUOTA_IDTYPE_USER == ULFS_USRQUOTA);
	CTASSERT(QUOTA_IDTYPE_GROUP == ULFS_GRPQUOTA);
	CTASSERT(QUOTA_OBJTYPE_BLOCKS == QL_BLOCK);
	CTASSERT(QUOTA_OBJTYPE_FILES == QL_FILE);

	/*
	 * If some of the idtypes aren't configured/enabled, arrange
	 * to skip over them.
	 */
	if (cursor->q2c_users_done == 0 &&
	    ump->um_quotas[ULFS_USRQUOTA] == NULLVP) {
		cursor->q2c_users_done = 1;
	}
	if (cursor->q2c_groups_done == 0 &&
	    ump->um_quotas[ULFS_GRPQUOTA] == NULLVP) {
		cursor->q2c_groups_done = 1;
	}

	/* Loop over, potentially, both idtypes */
	while (1) {

		/* Choose id type */
		error = q2cursor_pickidtype(cursor, &idtype);
		if (error == EAGAIN) {
			/* nothing more to do, return 0 */
			*ret = 0;
			return 0;
		}
		KASSERT(ump->um_quotas[idtype] != NULLVP);

		/*
		 * Initialize the per-call iteration state. Copy the
		 * cursor state so we can update it in place but back
		 * out on error.
		 */
		q2cursor_initstate(&state, keys, vals, maxreturn,
				   cursor->q2c_blocks_done);
		newcursor = *cursor;

		/* Assign keys */
		error = q2cursor_getkeys(ump, idtype, &newcursor, &state,
					 &quota2_hash_size, &default_q2e);
		if (error) {
			return error;
		}

		/* Now fill in the values. */
		error = q2cursor_getvals(ump, &state, &default_q2e);
		if (error) {
			return error;
		}

		/*
		 * Now that we aren't going to fail and lose what we
		 * did so far, we can update the cursor state.
		 */

		if (newcursor.q2c_hashpos >= quota2_hash_size) {
			if (idtype == QUOTA_IDTYPE_USER)
				cursor->q2c_users_done = 1;
			else
				cursor->q2c_groups_done = 1;

			/* start over on another id type */
			cursor->q2c_hashsize = 0;
			cursor->q2c_defaults_done = 0;
			cursor->q2c_hashpos = 0;
			cursor->q2c_uidpos = 0;
			cursor->q2c_blocks_done = 0;
		} else {
			*cursor = newcursor;
			cursor->q2c_blocks_done = state.skiplast;
		}

		/*
		 * If we have something to return, return it.
		 * Otherwise, continue to the other idtype, if any,
		 * and only return zero at end of iteration.
		 */
		if (state.numkeys > 0) {
			break;
		}
	}

	*ret = state.numkeys;
	return 0;
}

int
lfsquota2_handle_cmd_cursoropen(struct ulfsmount *ump, struct quotakcursor *qkc)
{
	struct ulfsq2_cursor *cursor;

	CTASSERT(sizeof(*cursor) <= sizeof(qkc->u.qkc_space));
	cursor = Q2CURSOR(qkc);

	cursor->q2c_magic = Q2C_MAGIC;
	cursor->q2c_hashsize = 0;

	cursor->q2c_users_done = 0;
	cursor->q2c_groups_done = 0;
	cursor->q2c_defaults_done = 0;
	cursor->q2c_hashpos = 0;
	cursor->q2c_uidpos = 0;
	cursor->q2c_blocks_done = 0;
	return 0;
}

int
lfsquota2_handle_cmd_cursorclose(struct ulfsmount *ump, struct quotakcursor *qkc)
{
	struct ulfsq2_cursor *cursor;
	int error;

	cursor = Q2CURSOR(qkc);
	error = q2cursor_check(cursor);
	if (error) {
		return error;
	}

	/* nothing to do */

	return 0;
}

int
lfsquota2_handle_cmd_cursorskipidtype(struct ulfsmount *ump,
    struct quotakcursor *qkc, int idtype)
{
	struct ulfsq2_cursor *cursor;
	int error;

	cursor = Q2CURSOR(qkc);
	error = q2cursor_check(cursor);
	if (error) {
		return error;
	}

	switch (idtype) {
	    case QUOTA_IDTYPE_USER:
		cursor->q2c_users_done = 1;
		break;
	    case QUOTA_IDTYPE_GROUP:
		cursor->q2c_groups_done = 1;
		break;
	    default:
		return EINVAL;
	}

	return 0;
}

int
lfsquota2_handle_cmd_cursoratend(struct ulfsmount *ump, struct quotakcursor *qkc,
    int *ret)
{
	struct ulfsq2_cursor *cursor;
	int error;

	cursor = Q2CURSOR(qkc);
	error = q2cursor_check(cursor);
	if (error) {
		return error;
	}

	*ret = (cursor->q2c_users_done && cursor->q2c_groups_done);
	return 0;
}

int
lfsquota2_handle_cmd_cursorrewind(struct ulfsmount *ump, struct quotakcursor *qkc)
{
	struct ulfsq2_cursor *cursor;
	int error;

	cursor = Q2CURSOR(qkc);
	error = q2cursor_check(cursor);
	if (error) {
		return error;
	}

	cursor->q2c_hashsize = 0;

	cursor->q2c_users_done = 0;
	cursor->q2c_groups_done = 0;
	cursor->q2c_defaults_done = 0;
	cursor->q2c_hashpos = 0;
	cursor->q2c_uidpos = 0;
	cursor->q2c_blocks_done = 0;

	return 0;
}

int
lfs_q2sync(struct mount *mp)
{
	return 0;
}

struct dq2get_callback {
	uid_t id;
	struct dquot *dq;
};

static int
dq2get_callback(struct ulfsmount *ump, uint64_t *offp, struct quota2_entry *q2e,
    uint64_t off, void *v)
{
	struct dq2get_callback *c = v;
	daddr_t lblkno;
	int blkoff;
	struct lfs *fs = ump->um_lfs;
	const int needswap = ULFS_MPNEEDSWAP(fs);

	if (ulfs_rw32(q2e->q2e_uid, needswap) == c->id) {
		KASSERT(mutex_owned(&c->dq->dq_interlock));
		lblkno = (off >> ump->um_mountp->mnt_fs_bshift);
		blkoff = (off & ump->umq2_bmask);
		c->dq->dq2_lblkno = lblkno;
		c->dq->dq2_blkoff = blkoff;
		return Q2WL_ABORT;
	}
	return 0;
}

int
lfs_dq2get(struct vnode *dqvp, u_long id, struct ulfsmount *ump, int type,
    struct dquot *dq)
{
	struct buf *bp;
	struct quota2_header *q2h;
	int error;
	daddr_t offset;
	u_long hash_mask;
	struct dq2get_callback c = {
		.id = id,
		.dq = dq
	};

	KASSERT(mutex_owned(&dq->dq_interlock));
	mutex_enter(&lfs_dqlock);
	error = getq2h(ump, type, &bp, &q2h, 0);
	if (error)
		goto out_mutex;
	/* look for our entry */
	hash_mask = ((1 << q2h->q2h_hash_shift) - 1);
	offset = q2h->q2h_entries[id & hash_mask];
	error = quota2_walk_list(ump, bp, type, &offset, 0, (void *)&c,
	    dq2get_callback);
	brelse(bp, 0);
out_mutex:
	mutex_exit(&lfs_dqlock);
	return error;
}

int
lfs_dq2sync(struct vnode *vp, struct dquot *dq)
{
	return 0;
}

int
lfs_quota2_mount(struct mount *mp)
{
	struct ulfsmount *ump = VFSTOULFS(mp);
	struct lfs *fs = ump->um_lfs;
	int error = 0;
	struct vnode *vp;
	struct lwp *l = curlwp;

	if ((fs->lfs_use_quota2) == 0)
		return 0;

	fs->um_flags |= ULFS_QUOTA2;
	ump->umq2_bsize = lfs_sb_getbsize(fs);
	ump->umq2_bmask = lfs_sb_getbmask(fs);
	if (fs->lfs_quota_magic != Q2_HEAD_MAGIC) {
		printf("%s: Invalid quota magic number\n",
		    mp->mnt_stat.f_mntonname);
		return EINVAL;
	}
        if ((fs->lfs_quota_flags & FS_Q2_DO_TYPE(ULFS_USRQUOTA)) &&
            fs->lfs_quotaino[ULFS_USRQUOTA] == 0) {
                printf("%s: no user quota inode\n",
		    mp->mnt_stat.f_mntonname); 
                error = EINVAL;
        }
        if ((fs->lfs_quota_flags & FS_Q2_DO_TYPE(ULFS_GRPQUOTA)) &&
            fs->lfs_quotaino[ULFS_GRPQUOTA] == 0) {
                printf("%s: no group quota inode\n",
		    mp->mnt_stat.f_mntonname);
                error = EINVAL;
        }
	if (error)
		return error;

        if (fs->lfs_quota_flags & FS_Q2_DO_TYPE(ULFS_USRQUOTA) &&
	    ump->um_quotas[ULFS_USRQUOTA] == NULLVP) {
		error = VFS_VGET(mp, fs->lfs_quotaino[ULFS_USRQUOTA], &vp);
		if (error) {
			printf("%s: can't vget() user quota inode: %d\n",
			    mp->mnt_stat.f_mntonname, error);
			return error;
		}
		ump->um_quotas[ULFS_USRQUOTA] = vp;
		ump->um_cred[ULFS_USRQUOTA] = l->l_cred;
		mutex_enter(vp->v_interlock);
		vp->v_writecount++;
		mutex_exit(vp->v_interlock);
		VOP_UNLOCK(vp);
	}
        if (fs->lfs_quota_flags & FS_Q2_DO_TYPE(ULFS_GRPQUOTA) &&
	    ump->um_quotas[ULFS_GRPQUOTA] == NULLVP) {
		error = VFS_VGET(mp, fs->lfs_quotaino[ULFS_GRPQUOTA], &vp);
		if (error) {
			vn_close(ump->um_quotas[ULFS_USRQUOTA],
			    FREAD|FWRITE, l->l_cred);
			printf("%s: can't vget() group quota inode: %d\n",
			    mp->mnt_stat.f_mntonname, error);
			return error;
		}
		ump->um_quotas[ULFS_GRPQUOTA] = vp;
		ump->um_cred[ULFS_GRPQUOTA] = l->l_cred;
		mutex_enter(vp->v_interlock);
		vp->v_vflag |= VV_SYSTEM;
		vp->v_writecount++;
		mutex_exit(vp->v_interlock);
		VOP_UNLOCK(vp);
	}
	mp->mnt_flag |= MNT_QUOTA;
	return 0;
}

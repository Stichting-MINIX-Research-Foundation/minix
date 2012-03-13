/* $NetBSD: ufs_quota2.c,v 1.4 2011/06/07 14:56:13 bouyer Exp $ */
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
__KERNEL_RCSID(0, "$NetBSD: ufs_quota2.c,v 1.4 2011/06/07 14:56:13 bouyer Exp $");

#include <sys/buf.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/fstrans.h>
#include <sys/kauth.h>
#include <sys/wapbl.h>

#include <ufs/ufs/quota2.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_bswap.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ufs/ufs_quota.h>
#include <ufs/ufs/ufs_wapbl.h>
#include <quota/quotaprop.h>

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
static int getq2h(struct ufsmount *, int, struct buf **,
    struct quota2_header **, int);
static int getq2e(struct ufsmount *, int, daddr_t, int, struct buf **,
    struct quota2_entry **, int);
static int quota2_walk_list(struct ufsmount *, struct buf *, int,
    uint64_t *, int, void *,
    int (*func)(struct ufsmount *, uint64_t *, struct quota2_entry *,
      uint64_t, void *));

static int quota2_dict_update_q2e_limits(prop_dictionary_t,
    struct quota2_entry *);
static prop_dictionary_t q2etoprop(struct quota2_entry *, int);

static const char *limnames[] = INITQLNAMES;

static int
quota2_dict_update_q2e_limits(prop_dictionary_t data,
    struct quota2_entry *q2e)
{
	const char *val_limitsonly_names[] = INITQVNAMES_LIMITSONLY;

	int i, error;
	prop_dictionary_t val;

	for (i = 0; i < N_QL; i++) {
		if (!prop_dictionary_get_dict(data, limnames[i], &val))
			return EINVAL;
		error = quotaprop_dict_get_uint64(val,
		    &q2e->q2e_val[i].q2v_hardlimit,
		    val_limitsonly_names, N_QV, true);
		if (error)
			return error;
	}
	return 0;
}
static prop_dictionary_t
q2etoprop(struct quota2_entry *q2e, int def)
{
	const char *val_names[] = INITQVNAMES_ALL;
	prop_dictionary_t dict1 = prop_dictionary_create();
	prop_dictionary_t dict2;
	int i;

	if (dict1 == NULL)
		return NULL;

	if (def) {
		if (!prop_dictionary_set_cstring_nocopy(dict1, "id",
		    "default")) {
			goto err;
		}
	} else {
		if (!prop_dictionary_set_uint32(dict1, "id", q2e->q2e_uid)) {
			goto err;
		}
	}
	for (i = 0; i < N_QL; i++) {
		dict2 = limits64toprop(&q2e->q2e_val[i].q2v_hardlimit,
		    val_names, N_QV);
		if (dict2 == NULL)
			goto err;
		if (!prop_dictionary_set_and_rel(dict1, limnames[i], dict2))
			goto err;
	}
	return dict1;

err:
	prop_object_release(dict1);
	return NULL;
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
getq2h(struct ufsmount *ump, int type,
    struct buf **bpp, struct quota2_header **q2hp, int flags)
{
#ifdef FFS_EI
	const int needswap = UFS_MPNEEDSWAP(ump);
#endif
	int error;
	struct buf *bp;
	struct quota2_header *q2h;

	KASSERT(mutex_owned(&dqlock));
	error = bread(ump->um_quotas[type], 0, ump->umq2_bsize,
	    ump->um_cred[type], flags, &bp);
	if (error)
		return error;
	if (bp->b_resid != 0) 
		panic("dq2get: %s quota file truncated", quotatypes[type]);

	q2h = (void *)bp->b_data;
	if (ufs_rw32(q2h->q2h_magic_number, needswap) != Q2_HEAD_MAGIC ||
	    q2h->q2h_type != type)
		panic("dq2get: corrupted %s quota header", quotatypes[type]);
	*bpp = bp;
	*q2hp = q2h;
	return 0;
}

static int
getq2e(struct ufsmount *ump, int type, daddr_t lblkno, int blkoffset,
    struct buf **bpp, struct quota2_entry **q2ep, int flags)
{
	int error;
	struct buf *bp;

	if (blkoffset & (sizeof(uint64_t) - 1)) {
		panic("dq2get: %s quota file corrupted",
		    quotatypes[type]);
	}
	error = bread(ump->um_quotas[type], lblkno, ump->umq2_bsize,
	    ump->um_cred[type], flags, &bp);
	if (error)
		return error;
	if (bp->b_resid != 0) {
		panic("dq2get: %s quota file corrupted",
		    quotatypes[type]);
	}
	*q2ep = (void *)((char *)bp->b_data + blkoffset);
	*bpp = bp;
	return 0;
}

/* walk a quota entry list, calling the callback for each entry */
#define Q2WL_ABORT 0x10000000

static int
quota2_walk_list(struct ufsmount *ump, struct buf *hbp, int type,
    uint64_t *offp, int flags, void *a,
    int (*func)(struct ufsmount *, uint64_t *, struct quota2_entry *, uint64_t, void *))
{
#ifdef FFS_EI
	const int needswap = UFS_MPNEEDSWAP(ump);
#endif
	daddr_t off = ufs_rw64(*offp, needswap);
	struct buf *bp, *obp = hbp;
	int ret = 0, ret2 = 0;
	struct quota2_entry *q2e;
	daddr_t lblkno, blkoff, olblkno = 0;

	KASSERT(mutex_owner(&dqlock));

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
			    ump->umq2_bsize,
			    ump->um_cred[type], flags, &bp);
			if (ret)
				return ret;
			if (bp->b_resid != 0) {
				panic("quota2_walk_list: %s quota file corrupted",
				    quotatypes[type]);
			}
		}
		q2e = (void *)((char *)(bp->b_data) + blkoff);
		ret = (*func)(ump, offp, q2e, off, a);
		if (off != ufs_rw64(*offp, needswap)) {
			/* callback changed parent's pointer, redo */
			off = ufs_rw64(*offp, needswap);
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
			off = ufs_rw64(*offp, needswap);
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
quota2_umount(struct mount *mp, int flags)
{
	int i, error;
	struct ufsmount *ump = VFSTOUFS(mp);

	if ((ump->um_flags & UFS_QUOTA2) == 0)
		return 0;

	for (i = 0; i < MAXQUOTAS; i++) {
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
quota2_q2ealloc(struct ufsmount *ump, int type, uid_t uid, struct dquot *dq,
    struct buf **bpp, struct quota2_entry **q2ep)
{
	int error, error2;
	struct buf *hbp, *bp;
	struct quota2_header *q2h;
	struct quota2_entry *q2e;
	daddr_t offset;
	u_long hash_mask;
	const int needswap = UFS_MPNEEDSWAP(ump);

	KASSERT(mutex_owned(&dq->dq_interlock));
	KASSERT(mutex_owned(&dqlock));
	error = getq2h(ump, type, &hbp, &q2h, B_MODIFY);
	if (error)
		return error;
	offset = ufs_rw64(q2h->q2h_free, needswap);
	if (offset == 0) {
		struct vnode *vp = ump->um_quotas[type];
		struct inode *ip = VTOI(vp);
		uint64_t size = ip->i_size;
		/* need to alocate a new disk block */
		error = UFS_BALLOC(vp, size, ump->umq2_bsize,
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
		quota2_addfreeq2e(q2h, bp->b_data, size, ump->umq2_bsize,
		    needswap);
		error = bwrite(bp);
		error2 = UFS_UPDATE(vp, NULL, NULL, UPDATE_WAIT);
		if (error || error2) {
			brelse(hbp, 0);
			if (error)
				return error;
			return error2;
		}
		offset = ufs_rw64(q2h->q2h_free, needswap);
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
	q2e->q2e_uid = ufs_rw32(uid, needswap);
	/* insert in hash list */ 
	q2e->q2e_next = q2h->q2h_entries[uid & hash_mask];
	q2h->q2h_entries[uid & hash_mask] = ufs_rw64(offset, needswap);
	if (hbp != bp) {
		bwrite(hbp);
	}
	*q2ep = q2e;
	*bpp = bp;
	return 0;
}

static int
getinoquota2(struct inode *ip, bool alloc, bool modify, struct buf **bpp,
    struct quota2_entry **q2ep)
{
	int error;
	int i;
	struct dquot *dq;
	struct ufsmount *ump = ip->i_ump;
	u_int32_t ino_ids[MAXQUOTAS];

	error = getinoquota(ip);
	if (error)
		return error;

	if (alloc) {
		UFS_WAPBL_JLOCK_ASSERT(ump->um_mountp);
	}
        ino_ids[USRQUOTA] = ip->i_uid;
        ino_ids[GRPQUOTA] = ip->i_gid;
	/* first get the interlock for all dquot */
	for (i = 0; i < MAXQUOTAS; i++) {
		dq = ip->i_dquot[i];
		if (dq == NODQUOT)
			continue;
		mutex_enter(&dq->dq_interlock);
	}
	/* now get the corresponding quota entry */
	for (i = 0; i < MAXQUOTAS; i++) {
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
			dqrele(NULLVP, dq);
			ip->i_dquot[i] = NULL;
			continue;
		}

		if ((dq->dq2_lblkno | dq->dq2_blkoff) == 0) {
			if (!alloc) {
				continue;
			}
			/* need to alloc a new on-disk quot */
			mutex_enter(&dqlock);
			error = quota2_q2ealloc(ump, i, ino_ids[i], dq,
			    &bpp[i], &q2ep[i]);
			mutex_exit(&dqlock);
			if (error)
				return error;
		} else {
			error = getq2e(ump, i, dq->dq2_lblkno,
			    dq->dq2_blkoff, &bpp[i], &q2ep[i],
			    modify ? B_MODIFY : 0);
			if (error)
				return error;
		}
	}
	return 0;
}

static int
quota2_check(struct inode *ip, int vtype, int64_t change, kauth_cred_t cred,
    int flags)
{
	int error;
	struct buf *bp[MAXQUOTAS];
	struct quota2_entry *q2e[MAXQUOTAS];
	struct quota2_val *q2vp;
	struct dquot *dq;
	uint64_t ncurblks;
	struct ufsmount *ump = ip->i_ump;
	struct mount *mp = ump->um_mountp;
	const int needswap = UFS_MPNEEDSWAP(ump);
	int i;

	if ((error = getinoquota2(ip, change > 0, change != 0, bp, q2e)) != 0)
		return error;
	if (change == 0) {
		for (i = 0; i < MAXQUOTAS; i++) {
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
		for (i = 0; i < MAXQUOTAS; i++) {
			dq = ip->i_dquot[i];
			if (dq == NODQUOT)
				continue;
			if (q2e[i] == NULL) {
				mutex_exit(&dq->dq_interlock);
				continue;
			}
			q2vp = &q2e[i]->q2e_val[vtype];
			ncurblks = ufs_rw64(q2vp->q2v_cur, needswap);
			if (ncurblks < -change)
				ncurblks = 0;
			else
				ncurblks += change;
			q2vp->q2v_cur = ufs_rw64(ncurblks, needswap);
			quota2_bwrite(mp, bp[i]);
			mutex_exit(&dq->dq_interlock);
		}
		return 0;
	}
	/* see if the allocation is allowed */
	for (i = 0; i < MAXQUOTAS; i++) {
		struct quota2_val q2v;
		int ql_stat;
		dq = ip->i_dquot[i];
		if (dq == NODQUOT)
			continue;
		KASSERT(q2e[i] != NULL);
		quota2_ufs_rwq2v(&q2e[i]->q2e_val[vtype], &q2v, needswap);
		ql_stat = quota2_check_limit(&q2v, change, time_second);

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
					    quotatypes[i], limnames[vtype]);
					dq->dq_flags |= DQ_WARN(vtype);
				}
				error = EDQUOT;
				break;
			case QL_S_DENY_GRACE:
				if ((dq->dq_flags & DQ_WARN(vtype)) == 0) {
					uprintf("\n%s: write failed, %s %s "
					    "limit reached\n",
					    mp->mnt_stat.f_mntonname,
					    quotatypes[i], limnames[vtype]);
					dq->dq_flags |= DQ_WARN(vtype);
				}
				error = EDQUOT;
				break;
			case QL_S_ALLOW_SOFT:
				if ((dq->dq_flags & DQ_WARN(vtype)) == 0) {
					uprintf("\n%s: warning, %s %s "
					    "quota exceeded\n",
					    mp->mnt_stat.f_mntonname,
					    quotatypes[i], limnames[vtype]);
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
			quota2_ufs_rwq2v(&q2v, &q2e[i]->q2e_val[vtype],
			    needswap);
		}
	}

	/* now do the allocation if allowed */
	for (i = 0; i < MAXQUOTAS; i++) {
		dq = ip->i_dquot[i];
		if (dq == NODQUOT)
			continue;
		KASSERT(q2e[i] != NULL);
		if (error == 0) {
			q2vp = &q2e[i]->q2e_val[vtype];
			ncurblks = ufs_rw64(q2vp->q2v_cur, needswap);
			q2vp->q2v_cur = ufs_rw64(ncurblks + change, needswap);
			quota2_bwrite(mp, bp[i]);
		} else
			brelse(bp[i], 0);
		mutex_exit(&dq->dq_interlock);
	}
	return error;
}

int
chkdq2(struct inode *ip, int64_t change, kauth_cred_t cred, int flags)
{
	return quota2_check(ip, QL_BLOCK, change, cred, flags);
}

int
chkiq2(struct inode *ip, int32_t change, kauth_cred_t cred, int flags)
{
	return quota2_check(ip, QL_FILE, change, cred, flags);
}

int
quota2_handle_cmd_set(struct ufsmount *ump, int type, int id,
    int defaultq, prop_dictionary_t data)
{
	int error;
	struct dquot *dq;
	struct quota2_header *q2h;
	struct quota2_entry q2e, *q2ep;
	struct buf *bp;
	const int needswap = UFS_MPNEEDSWAP(ump);

	if (ump->um_quotas[type] == NULLVP)
		return ENODEV;
	error = UFS_WAPBL_BEGIN(ump->um_mountp);
	if (error)
		return error;
	
	if (defaultq) {
		mutex_enter(&dqlock);
		error = getq2h(ump, type, &bp, &q2h, B_MODIFY);
		if (error) {
			mutex_exit(&dqlock);
			goto out_wapbl;
		}
		quota2_ufs_rwq2e(&q2h->q2h_defentry, &q2e, needswap);
		error = quota2_dict_update_q2e_limits(data, &q2e);
		if (error) {
			mutex_exit(&dqlock);
			brelse(bp, 0);
			goto out_wapbl;
		}
		quota2_ufs_rwq2e(&q2e, &q2h->q2h_defentry, needswap);
		mutex_exit(&dqlock);
		quota2_bwrite(ump->um_mountp, bp);
		goto out_wapbl;
	}

	error = dqget(NULLVP, id, ump, type, &dq);
	if (error)
		goto out_wapbl;

	mutex_enter(&dq->dq_interlock);
	if (dq->dq2_lblkno == 0 && dq->dq2_blkoff == 0) {
		/* need to alloc a new on-disk quot */
		mutex_enter(&dqlock);
		error = quota2_q2ealloc(ump, type, id, dq, &bp, &q2ep);
		mutex_exit(&dqlock);
	} else {
		error = getq2e(ump, type, dq->dq2_lblkno, dq->dq2_blkoff,
		    &bp, &q2ep, B_MODIFY);
	}
	if (error)
		goto out_il;
	
	quota2_ufs_rwq2e(q2ep, &q2e, needswap);
	error = quota2_dict_update_q2e_limits(data, &q2e);
	if (error) {
		brelse(bp, 0);
		goto out_il;
	}
	quota2_ufs_rwq2e(&q2e, q2ep, needswap);
	quota2_bwrite(ump->um_mountp, bp);

out_il:
	mutex_exit(&dq->dq_interlock);
	dqrele(NULLVP, dq);
out_wapbl:
	UFS_WAPBL_END(ump->um_mountp);
	return error;
}

struct dq2clear_callback {
	uid_t id;
	struct dquot *dq;
	struct quota2_header *q2h;
};

static int
dq2clear_callback(struct ufsmount *ump, uint64_t *offp, struct quota2_entry *q2e,
    uint64_t off, void *v)
{
	struct dq2clear_callback *c = v;
#ifdef FFS_EI
	const int needswap = UFS_MPNEEDSWAP(ump);
#endif
	uint64_t myoff;

	if (ufs_rw32(q2e->q2e_uid, needswap) == c->id) {
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
quota2_handle_cmd_clear(struct ufsmount *ump, int type, int id,
    int defaultq, prop_dictionary_t data)
{
	int error, i;
	struct dquot *dq;
	struct quota2_header *q2h;
	struct quota2_entry q2e, *q2ep;
	struct buf *hbp, *bp;
	u_long hash_mask;
	struct dq2clear_callback c;

	if (ump->um_quotas[type] == NULLVP)
		return ENODEV;
	if (defaultq)
		return EOPNOTSUPP;

	/* get the default entry before locking the entry's buffer */
	mutex_enter(&dqlock);
	error = getq2h(ump, type, &hbp, &q2h, 0);
	if (error) {
		mutex_exit(&dqlock);
		return error;
	}
	/* we'll copy to another disk entry, so no need to swap */
	memcpy(&q2e, &q2h->q2h_defentry, sizeof(q2e));
	mutex_exit(&dqlock);
	brelse(hbp, 0);

	error = dqget(NULLVP, id, ump, type, &dq);
	if (error)
		return error;

	mutex_enter(&dq->dq_interlock);
	if (dq->dq2_lblkno == 0 && dq->dq2_blkoff == 0) {
		/* already clear, nothing to do */
		error = ENOENT;
		goto out_il;
	}
	error = UFS_WAPBL_BEGIN(ump->um_mountp);
	if (error)
		goto out_dq;
	
	error = getq2e(ump, type, dq->dq2_lblkno, dq->dq2_blkoff,
	    &bp, &q2ep, B_MODIFY);
	if (error)
		goto out_wapbl;

	if (q2ep->q2e_val[QL_BLOCK].q2v_cur != 0 ||
	    q2ep->q2e_val[QL_FILE].q2v_cur != 0) {
		/* can't free this entry; revert to default */
		for (i = 0; i < N_QL; i++) {
			q2ep->q2e_val[i].q2v_softlimit =
			    q2e.q2e_val[i].q2v_softlimit;
			q2ep->q2e_val[i].q2v_hardlimit =
			    q2e.q2e_val[i].q2v_hardlimit;
			q2ep->q2e_val[i].q2v_grace =
			    q2e.q2e_val[i].q2v_grace;
			q2ep->q2e_val[i].q2v_time = 0;
		}
		quota2_bwrite(ump->um_mountp, bp);
		goto out_wapbl;
	}
	/* we can free it. release bp so we can walk the list */
	brelse(bp, 0);
	mutex_enter(&dqlock);
	error = getq2h(ump, type, &hbp, &q2h, 0);
	if (error)
		goto out_dqlock;

	hash_mask = ((1 << q2h->q2h_hash_shift) - 1);
	c.dq = dq;
	c.id = id;
	c.q2h = q2h;
	error = quota2_walk_list(ump, hbp, type,
	    &q2h->q2h_entries[id & hash_mask], B_MODIFY, &c,
	    dq2clear_callback);

	bwrite(hbp);

out_dqlock:
	mutex_exit(&dqlock);
out_wapbl:
	UFS_WAPBL_END(ump->um_mountp);
out_il:
	mutex_exit(&dq->dq_interlock);
out_dq:
	dqrele(NULLVP, dq);
	return error;
}

static int
quota2_array_add_q2e(struct ufsmount *ump, int type,
    int id, prop_array_t replies)
{
	struct dquot *dq;
	int error;
	struct quota2_entry *q2ep, q2e;
	struct buf  *bp;
	const int needswap = UFS_MPNEEDSWAP(ump);
	prop_dictionary_t dict;

	error = dqget(NULLVP, id, ump, type, &dq);
	if (error)
		return error;

	mutex_enter(&dq->dq_interlock);
	if (dq->dq2_lblkno == 0 && dq->dq2_blkoff == 0) {
		mutex_exit(&dq->dq_interlock);
		dqrele(NULLVP, dq);
		return ENOENT;
	}
	error = getq2e(ump, type, dq->dq2_lblkno, dq->dq2_blkoff,
	    &bp, &q2ep, 0);
	if (error) {
		mutex_exit(&dq->dq_interlock);
		dqrele(NULLVP, dq);
		return error;
	}
	quota2_ufs_rwq2e(q2ep, &q2e, needswap);
	brelse(bp, 0);
	mutex_exit(&dq->dq_interlock);
	dqrele(NULLVP, dq);
	dict = q2etoprop(&q2e, 0);
	if (dict == NULL)
		return ENOMEM;
	if (!prop_array_add_and_rel(replies, dict))
		return ENOMEM;
	return 0;
}

int
quota2_handle_cmd_get(struct ufsmount *ump, int type, int id,
    int defaultq, prop_array_t replies)
{
	int error;
	struct quota2_header *q2h;
	struct quota2_entry q2e;
	struct buf *bp;
	prop_dictionary_t dict;
	const int needswap = UFS_MPNEEDSWAP(ump);

	if (ump->um_quotas[type] == NULLVP)
		return ENODEV;
	if (defaultq) {
		mutex_enter(&dqlock);
		error = getq2h(ump, type, &bp, &q2h, 0);
		if (error) {
			mutex_exit(&dqlock);
			return error;
		}
		quota2_ufs_rwq2e(&q2h->q2h_defentry, &q2e, needswap);
		mutex_exit(&dqlock);
		brelse(bp, 0);
		dict = q2etoprop(&q2e, defaultq);
		if (dict == NULL)
			return ENOMEM;
		if (!prop_array_add_and_rel(replies, dict))
			return ENOMEM;
	} else
		error = quota2_array_add_q2e(ump, type, id, replies);
	
	return error;
}

struct getuids {
	long nuids; /* number of uids in array */
	long size;  /* size of array */
	uid_t *uids; /* array of uids, dynamically allocated */
};

static int
quota2_getuids_callback(struct ufsmount *ump, uint64_t *offp,
    struct quota2_entry *q2ep, uint64_t off, void *v)
{
	struct getuids *gu = v;
	uid_t *newuids;
#ifdef FFS_EI
	const int needswap = UFS_MPNEEDSWAP(ump);
#endif

	if (gu->nuids == gu->size) {
		newuids = realloc(gu->uids, gu->size + PAGE_SIZE, M_TEMP,
		    M_WAITOK);
		if (newuids == NULL) {
			free(gu->uids, M_TEMP);
			return ENOMEM;
		}
		gu->uids = newuids;
		gu->size += (PAGE_SIZE / sizeof(uid_t));
	}
	gu->uids[gu->nuids] = ufs_rw32(q2ep->q2e_uid, needswap);
	gu->nuids++;
	return 0;
}

int
quota2_handle_cmd_getall(struct ufsmount *ump, int type, prop_array_t replies)
{
	int error;
	struct quota2_header *q2h;
	struct quota2_entry  q2e;
	struct buf *hbp;
	prop_dictionary_t dict;
	uint64_t offset;
	int i, j;
	int quota2_hash_size;
	const int needswap = UFS_MPNEEDSWAP(ump);
	struct getuids gu;

	if (ump->um_quotas[type] == NULLVP)
		return ENODEV;
	mutex_enter(&dqlock);
	error = getq2h(ump, type, &hbp, &q2h, 0);
	if (error) {
		mutex_exit(&dqlock);
		return error;
	}
	quota2_ufs_rwq2e(&q2h->q2h_defentry, &q2e, needswap);
	dict = q2etoprop(&q2e, 1);
	if (!prop_array_add_and_rel(replies, dict)) {
		error = ENOMEM;
		goto error_bp;
	}
	/*
	 * we can't directly get entries as we can't walk the list
	 * with qdlock and grab dq_interlock to read the entries
	 * at the same time. So just walk the lists to build a list of uid,
	 * and then read entries for these uids
	 */
	memset(&gu, 0, sizeof(gu));
	quota2_hash_size = ufs_rw16(q2h->q2h_hash_size, needswap);
	for (i = 0; i < quota2_hash_size ; i++) {
		offset = q2h->q2h_entries[i];
		error = quota2_walk_list(ump, hbp, type, &offset, 0, &gu,
		    quota2_getuids_callback);
		if (error) {
			if (gu.uids != NULL)
				free(gu.uids, M_TEMP);
			break;
		}
	}
error_bp:
	mutex_exit(&dqlock);
	brelse(hbp, 0);
	if (error)
		return error;
	for (j = 0; j < gu.nuids; j++) {
		error = quota2_array_add_q2e(ump, type,
		    gu.uids[j], replies);
		if (error && error != ENOENT)
			break;
	}
	free(gu.uids, M_TEMP);
	return error;
}

int
q2sync(struct mount *mp)
{
	return 0;
}

struct dq2get_callback {
	uid_t id;
	struct dquot *dq;
};

static int
dq2get_callback(struct ufsmount *ump, uint64_t *offp, struct quota2_entry *q2e,
    uint64_t off, void *v)
{
	struct dq2get_callback *c = v;
	daddr_t lblkno;
	int blkoff;
#ifdef FFS_EI
	const int needswap = UFS_MPNEEDSWAP(ump);
#endif

	if (ufs_rw32(q2e->q2e_uid, needswap) == c->id) {
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
dq2get(struct vnode *dqvp, u_long id, struct ufsmount *ump, int type,
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
	mutex_enter(&dqlock);
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
	mutex_exit(&dqlock);
	return error;
}

int
dq2sync(struct vnode *vp, struct dquot *dq)
{
	return 0;
}

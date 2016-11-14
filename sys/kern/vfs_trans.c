/*	$NetBSD: vfs_trans.c,v 1.34 2015/08/24 22:50:32 pooka Exp $	*/

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Juergen Hannken-Illjes.
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
__KERNEL_RCSID(0, "$NetBSD: vfs_trans.c,v 1.34 2015/08/24 22:50:32 pooka Exp $");

/*
 * File system transaction operations.
 */

#ifdef _KERNEL_OPT
#include "opt_ddb.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/atomic.h>
#include <sys/buf.h>
#include <sys/kmem.h>
#include <sys/mount.h>
#include <sys/pserialize.h>
#include <sys/vnode.h>
#define _FSTRANS_API_PRIVATE
#include <sys/fstrans.h>
#include <sys/proc.h>

#include <miscfs/specfs/specdev.h>

struct fscow_handler {
	LIST_ENTRY(fscow_handler) ch_list;
	int (*ch_func)(void *, struct buf *, bool);
	void *ch_arg;
};
struct fstrans_lwp_info {
	struct fstrans_lwp_info *fli_succ;
	struct lwp *fli_self;
	struct mount *fli_mount;
	int fli_trans_cnt;
	int fli_cow_cnt;
	enum fstrans_lock_type fli_lock_type;
	LIST_ENTRY(fstrans_lwp_info) fli_list;
};
struct fstrans_mount_info {
	enum fstrans_state fmi_state;
	unsigned int fmi_ref_cnt;
	bool fmi_cow_change;
	LIST_HEAD(, fscow_handler) fmi_cow_handler;
};

static specificdata_key_t lwp_data_key;	/* Our specific data key. */
static kmutex_t vfs_suspend_lock;	/* Serialize suspensions. */
static kmutex_t fstrans_lock;		/* Fstrans big lock. */
static kcondvar_t fstrans_state_cv;	/* Fstrans or cow state changed. */
static kcondvar_t fstrans_count_cv;	/* Fstrans or cow count changed. */
static pserialize_t fstrans_psz;	/* Pserialize state. */
static LIST_HEAD(fstrans_lwp_head, fstrans_lwp_info) fstrans_fli_head;
					/* List of all fstrans_lwp_info. */

static void fstrans_lwp_dtor(void *);
static void fstrans_mount_dtor(struct mount *);
static struct fstrans_lwp_info *fstrans_get_lwp_info(struct mount *, bool);
static bool grant_lock(const enum fstrans_state, const enum fstrans_lock_type);
static bool state_change_done(const struct mount *);
static bool cow_state_change_done(const struct mount *);
static void cow_change_enter(const struct mount *);
static void cow_change_done(const struct mount *);

/*
 * Initialize.
 */
void
fstrans_init(void)
{
	int error __diagused;

	error = lwp_specific_key_create(&lwp_data_key, fstrans_lwp_dtor);
	KASSERT(error == 0);

	mutex_init(&vfs_suspend_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&fstrans_lock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&fstrans_state_cv, "fstchg");
	cv_init(&fstrans_count_cv, "fstcnt");
	fstrans_psz = pserialize_create();
	LIST_INIT(&fstrans_fli_head);
}

/*
 * Deallocate lwp state.
 */
static void
fstrans_lwp_dtor(void *arg)
{
	struct fstrans_lwp_info *fli, *fli_next;

	for (fli = arg; fli; fli = fli_next) {
		KASSERT(fli->fli_trans_cnt == 0);
		KASSERT(fli->fli_cow_cnt == 0);
		if (fli->fli_mount != NULL)
			fstrans_mount_dtor(fli->fli_mount);
		fli_next = fli->fli_succ;
		fli->fli_mount = NULL;
		membar_sync();
		fli->fli_self = NULL;
	}
}

/*
 * Dereference mount state.
 */
static void
fstrans_mount_dtor(struct mount *mp)
{
	struct fstrans_mount_info *fmi;

	fmi = mp->mnt_transinfo;
	if (atomic_dec_uint_nv(&fmi->fmi_ref_cnt) > 0)
		return;

	KASSERT(fmi->fmi_state == FSTRANS_NORMAL);
	KASSERT(LIST_FIRST(&fmi->fmi_cow_handler) == NULL);

	kmem_free(fmi, sizeof(*fmi));
	mp->mnt_iflag &= ~IMNT_HAS_TRANS;
	mp->mnt_transinfo = NULL;

	vfs_destroy(mp);
}

/*
 * Allocate mount state.
 */
int
fstrans_mount(struct mount *mp)
{
	int error;
	struct fstrans_mount_info *newfmi;

	error = vfs_busy(mp, NULL);
	if (error)
		return error;
	newfmi = kmem_alloc(sizeof(*newfmi), KM_SLEEP);
	newfmi->fmi_state = FSTRANS_NORMAL;
	newfmi->fmi_ref_cnt = 1;
	LIST_INIT(&newfmi->fmi_cow_handler);
	newfmi->fmi_cow_change = false;

	mp->mnt_transinfo = newfmi;
	mp->mnt_iflag |= IMNT_HAS_TRANS;

	vfs_unbusy(mp, true, NULL);

	return 0;
}

/*
 * Deallocate mount state.
 */
void
fstrans_unmount(struct mount *mp)
{

	KASSERT(mp->mnt_transinfo != NULL);

	fstrans_mount_dtor(mp);
}

/*
 * Retrieve the per lwp info for this mount allocating if necessary.
 */
static struct fstrans_lwp_info *
fstrans_get_lwp_info(struct mount *mp, bool do_alloc)
{
	struct fstrans_lwp_info *fli, *res;
	struct fstrans_mount_info *fmi;

	/*
	 * Scan our list for a match clearing entries whose mount is gone.
	 */
	res = NULL;
	for (fli = lwp_getspecific(lwp_data_key); fli; fli = fli->fli_succ) {
		if (fli->fli_mount == mp) {
			KASSERT(res == NULL);
			res = fli;
		} else if (fli->fli_mount != NULL &&
		    (fli->fli_mount->mnt_iflag & IMNT_GONE) != 0 &&
		    fli->fli_trans_cnt == 0 && fli->fli_cow_cnt == 0) {
			fstrans_mount_dtor(fli->fli_mount);
			fli->fli_mount = NULL;
		}
	}
	if (__predict_true(res != NULL))
		return res;

	if (! do_alloc)
		return NULL;

	/*
	 * Try to reuse a cleared entry or allocate a new one.
	 */
	for (fli = lwp_getspecific(lwp_data_key); fli; fli = fli->fli_succ) {
		if (fli->fli_mount == NULL) {
			KASSERT(fli->fli_trans_cnt == 0);
			KASSERT(fli->fli_cow_cnt == 0);
			break;
		}
	}
	if (fli == NULL) {
		mutex_enter(&fstrans_lock);
		LIST_FOREACH(fli, &fstrans_fli_head, fli_list) {
			if (fli->fli_self == NULL) {
				KASSERT(fli->fli_trans_cnt == 0);
				KASSERT(fli->fli_cow_cnt == 0);
				fli->fli_self = curlwp;
				fli->fli_succ = lwp_getspecific(lwp_data_key);
				lwp_setspecific(lwp_data_key, fli);
				break;
			}
		}
		mutex_exit(&fstrans_lock);
	}
	if (fli == NULL) {
		fli = kmem_alloc(sizeof(*fli), KM_SLEEP);
		mutex_enter(&fstrans_lock);
		memset(fli, 0, sizeof(*fli));
		fli->fli_self = curlwp;
		LIST_INSERT_HEAD(&fstrans_fli_head, fli, fli_list);
		mutex_exit(&fstrans_lock);
		fli->fli_succ = lwp_getspecific(lwp_data_key);
		lwp_setspecific(lwp_data_key, fli);
	}

	/*
	 * Attach the entry to the mount.
	 */
	fmi = mp->mnt_transinfo;
	fli->fli_mount = mp;
	atomic_inc_uint(&fmi->fmi_ref_cnt);

	return fli;
}

/*
 * Check if this lock type is granted at this state.
 */
static bool
grant_lock(const enum fstrans_state state, const enum fstrans_lock_type type)
{

	if (__predict_true(state == FSTRANS_NORMAL))
		return true;
	if (type == FSTRANS_EXCL)
		return true;
	if  (state == FSTRANS_SUSPENDING && type == FSTRANS_LAZY)
		return true;

	return false;
}

/*
 * Start a transaction.  If this thread already has a transaction on this
 * file system increment the reference counter.
 */
int
_fstrans_start(struct mount *mp, enum fstrans_lock_type lock_type, int wait)
{
	int s;
	struct fstrans_lwp_info *fli;
	struct fstrans_mount_info *fmi;

	ASSERT_SLEEPABLE();

	if (mp == NULL || (mp->mnt_iflag & IMNT_HAS_TRANS) == 0)
		return 0;

	fli = fstrans_get_lwp_info(mp, true);

	if (fli->fli_trans_cnt > 0) {
		KASSERT(lock_type != FSTRANS_EXCL);
		fli->fli_trans_cnt += 1;

		return 0;
	}

	s = pserialize_read_enter();
	fmi = mp->mnt_transinfo;
	if (__predict_true(grant_lock(fmi->fmi_state, lock_type))) {
		fli->fli_trans_cnt = 1;
		fli->fli_lock_type = lock_type;
		pserialize_read_exit(s);

		return 0;
	}
	pserialize_read_exit(s);

	if (! wait)
		return EBUSY;

	mutex_enter(&fstrans_lock);
	while (! grant_lock(fmi->fmi_state, lock_type))
		cv_wait(&fstrans_state_cv, &fstrans_lock);
	fli->fli_trans_cnt = 1;
	fli->fli_lock_type = lock_type;
	mutex_exit(&fstrans_lock);

	return 0;
}

/*
 * Finish a transaction.
 */
void
fstrans_done(struct mount *mp)
{
	int s;
	struct fstrans_lwp_info *fli;
	struct fstrans_mount_info *fmi;

	if (mp == NULL || (mp->mnt_iflag & IMNT_HAS_TRANS) == 0)
		return;

	fli = fstrans_get_lwp_info(mp, false);
	KASSERT(fli != NULL);
	KASSERT(fli->fli_trans_cnt > 0);

	if (fli->fli_trans_cnt > 1) {
		fli->fli_trans_cnt -= 1;

		return;
	}

	s = pserialize_read_enter();
	fmi = mp->mnt_transinfo;
	if (__predict_true(fmi->fmi_state == FSTRANS_NORMAL)) {
		fli->fli_trans_cnt = 0;
		pserialize_read_exit(s);

		return;
	}
	pserialize_read_exit(s);

	mutex_enter(&fstrans_lock);
	fli->fli_trans_cnt = 0;
	cv_signal(&fstrans_count_cv);
	mutex_exit(&fstrans_lock);
}

/*
 * Check if this thread has an exclusive lock.
 */
int
fstrans_is_owner(struct mount *mp)
{
	struct fstrans_lwp_info *fli;

	if (mp == NULL || (mp->mnt_iflag & IMNT_HAS_TRANS) == 0)
		return 0;

	fli = fstrans_get_lwp_info(mp, false);
	if (fli == NULL || fli->fli_trans_cnt == 0)
		return 0;

	KASSERT(fli->fli_mount == mp);
	KASSERT(fli->fli_trans_cnt > 0);

	return (fli->fli_lock_type == FSTRANS_EXCL);
}

/*
 * True, if no thread is in a transaction not granted at the current state.
 */
static bool
state_change_done(const struct mount *mp)
{
	struct fstrans_lwp_info *fli;
	struct fstrans_mount_info *fmi;

	KASSERT(mutex_owned(&fstrans_lock));

	fmi = mp->mnt_transinfo;
	LIST_FOREACH(fli, &fstrans_fli_head, fli_list) {
		if (fli->fli_mount != mp)
			continue;
		if (fli->fli_trans_cnt == 0)
			continue;
		if (grant_lock(fmi->fmi_state, fli->fli_lock_type))
			continue;

		return false;
	}

	return true;
}

/*
 * Set new file system state.
 */
int
fstrans_setstate(struct mount *mp, enum fstrans_state new_state)
{
	int error;
	enum fstrans_state old_state;
	struct fstrans_mount_info *fmi;

	fmi = mp->mnt_transinfo;
	old_state = fmi->fmi_state;
	if (old_state == new_state)
		return 0;

	mutex_enter(&fstrans_lock);
	fmi->fmi_state = new_state;
	pserialize_perform(fstrans_psz);

	/*
	 * All threads see the new state now.
	 * Wait for transactions invalid at this state to leave.
	 */
	error = 0;
	while (! state_change_done(mp)) {
		error = cv_wait_sig(&fstrans_count_cv, &fstrans_lock);
		if (error) {
			new_state = fmi->fmi_state = FSTRANS_NORMAL;
			break;
		}
	}
	cv_broadcast(&fstrans_state_cv);
	mutex_exit(&fstrans_lock);

	if (old_state != new_state) {
		if (old_state == FSTRANS_NORMAL)
			fstrans_start(mp, FSTRANS_EXCL);
		if (new_state == FSTRANS_NORMAL)
			fstrans_done(mp);
	}

	return error;
}

/*
 * Get current file system state.
 */
enum fstrans_state
fstrans_getstate(struct mount *mp)
{
	struct fstrans_mount_info *fmi;

	fmi = mp->mnt_transinfo;
	KASSERT(fmi != NULL);

	return fmi->fmi_state;
}

/*
 * Request a filesystem to suspend all operations.
 */
int
vfs_suspend(struct mount *mp, int nowait)
{
	int error;

	if (nowait) {
		if (!mutex_tryenter(&vfs_suspend_lock))
			return EWOULDBLOCK;
	} else
		mutex_enter(&vfs_suspend_lock);

	mutex_enter(&syncer_mutex);
	if ((error = VFS_SUSPENDCTL(mp, SUSPEND_SUSPEND)) != 0) {
		mutex_exit(&syncer_mutex);
		mutex_exit(&vfs_suspend_lock);
	}

	return error;
}

/*
 * Request a filesystem to resume all operations.
 */
void
vfs_resume(struct mount *mp)
{

	VFS_SUSPENDCTL(mp, SUSPEND_RESUME);
	mutex_exit(&syncer_mutex);
	mutex_exit(&vfs_suspend_lock);
}


/*
 * True, if no thread is running a cow handler.
 */
static bool
cow_state_change_done(const struct mount *mp)
{
	struct fstrans_lwp_info *fli;
	struct fstrans_mount_info *fmi __diagused;

	fmi = mp->mnt_transinfo;

	KASSERT(mutex_owned(&fstrans_lock));
	KASSERT(fmi->fmi_cow_change);

	LIST_FOREACH(fli, &fstrans_fli_head, fli_list) {
		if (fli->fli_mount != mp)
			continue;
		if (fli->fli_cow_cnt == 0)
			continue;

		return false;
	}

	return true;
}

/*
 * Prepare for changing this mounts cow list.
 * Returns with fstrans_lock locked.
 */
static void
cow_change_enter(const struct mount *mp)
{
	struct fstrans_mount_info *fmi;

	fmi = mp->mnt_transinfo;

	mutex_enter(&fstrans_lock);

	/*
	 * Wait for other threads changing the list.
	 */
	while (fmi->fmi_cow_change)
		cv_wait(&fstrans_state_cv, &fstrans_lock);

	/*
	 * Wait until all threads are aware of a state change.
	 */
	fmi->fmi_cow_change = true;
	pserialize_perform(fstrans_psz);

	while (! cow_state_change_done(mp))
		cv_wait(&fstrans_count_cv, &fstrans_lock);
}

/*
 * Done changing this mounts cow list.
 */
static void
cow_change_done(const struct mount *mp)
{
	struct fstrans_mount_info *fmi;

	KASSERT(mutex_owned(&fstrans_lock));

	fmi = mp->mnt_transinfo;

	fmi->fmi_cow_change = false;
	pserialize_perform(fstrans_psz);

	cv_broadcast(&fstrans_state_cv);

	mutex_exit(&fstrans_lock);
}

/*
 * Add a handler to this mount.
 */
int
fscow_establish(struct mount *mp, int (*func)(void *, struct buf *, bool),
    void *arg)
{
	struct fstrans_mount_info *fmi;
	struct fscow_handler *newch;

	if ((mp->mnt_iflag & IMNT_HAS_TRANS) == 0)
		return EINVAL;

	fmi = mp->mnt_transinfo;
	KASSERT(fmi != NULL);

	newch = kmem_alloc(sizeof(*newch), KM_SLEEP);
	newch->ch_func = func;
	newch->ch_arg = arg;

	cow_change_enter(mp);
	LIST_INSERT_HEAD(&fmi->fmi_cow_handler, newch, ch_list);
	cow_change_done(mp);

	return 0;
}

/*
 * Remove a handler from this mount.
 */
int
fscow_disestablish(struct mount *mp, int (*func)(void *, struct buf *, bool),
    void *arg)
{
	struct fstrans_mount_info *fmi;
	struct fscow_handler *hp = NULL;

	if ((mp->mnt_iflag & IMNT_HAS_TRANS) == 0)
		return EINVAL;

	fmi = mp->mnt_transinfo;
	KASSERT(fmi != NULL);

	cow_change_enter(mp);
	LIST_FOREACH(hp, &fmi->fmi_cow_handler, ch_list)
		if (hp->ch_func == func && hp->ch_arg == arg)
			break;
	if (hp != NULL) {
		LIST_REMOVE(hp, ch_list);
		kmem_free(hp, sizeof(*hp));
	}
	cow_change_done(mp);

	return hp ? 0 : EINVAL;
}

/*
 * Check for need to copy block that is about to be written.
 */
int
fscow_run(struct buf *bp, bool data_valid)
{
	int error, s;
	struct mount *mp;
	struct fstrans_lwp_info *fli;
	struct fstrans_mount_info *fmi;
	struct fscow_handler *hp;

	/*
	 * First check if we need run the copy-on-write handler.
	 */
	if ((bp->b_flags & B_COWDONE))
		return 0;
	if (bp->b_vp == NULL) {
		bp->b_flags |= B_COWDONE;
		return 0;
	}
	if (bp->b_vp->v_type == VBLK)
		mp = spec_node_getmountedfs(bp->b_vp);
	else
		mp = bp->b_vp->v_mount;
	if (mp == NULL || (mp->mnt_iflag & IMNT_HAS_TRANS) == 0) {
		bp->b_flags |= B_COWDONE;
		return 0;
	}

	fli = fstrans_get_lwp_info(mp, true);
	fmi = mp->mnt_transinfo;

	/*
	 * On non-recursed run check if other threads
	 * want to change the list.
	 */
	if (fli->fli_cow_cnt == 0) {
		s = pserialize_read_enter();
		if (__predict_false(fmi->fmi_cow_change)) {
			pserialize_read_exit(s);
			mutex_enter(&fstrans_lock);
			while (fmi->fmi_cow_change)
				cv_wait(&fstrans_state_cv, &fstrans_lock);
			fli->fli_cow_cnt = 1;
			mutex_exit(&fstrans_lock);
		} else {
			fli->fli_cow_cnt = 1;
			pserialize_read_exit(s);
		}
	} else
		fli->fli_cow_cnt += 1;

	/*
	 * Run all copy-on-write handlers, stop on error.
	 */
	error = 0;
	LIST_FOREACH(hp, &fmi->fmi_cow_handler, ch_list)
		if ((error = (*hp->ch_func)(hp->ch_arg, bp, data_valid)) != 0)
			break;
 	if (error == 0)
 		bp->b_flags |= B_COWDONE;

	/*
	 * Check if other threads want to change the list.
	 */
	if (fli->fli_cow_cnt > 1) {
		fli->fli_cow_cnt -= 1;
	} else {
		s = pserialize_read_enter();
		if (__predict_false(fmi->fmi_cow_change)) {
			pserialize_read_exit(s);
			mutex_enter(&fstrans_lock);
			fli->fli_cow_cnt = 0;
			cv_signal(&fstrans_count_cv);
			mutex_exit(&fstrans_lock);
		} else {
			fli->fli_cow_cnt = 0;
			pserialize_read_exit(s);
		}
	}

	return error;
}

#if defined(DDB)
void fstrans_dump(int);

static void
fstrans_print_lwp(struct proc *p, struct lwp *l, int verbose)
{
	char prefix[9];
	struct fstrans_lwp_info *fli;

	snprintf(prefix, sizeof(prefix), "%d.%d", p->p_pid, l->l_lid);
	LIST_FOREACH(fli, &fstrans_fli_head, fli_list) {
		if (fli->fli_self != l)
			continue;
		if (fli->fli_trans_cnt == 0 && fli->fli_cow_cnt == 0) {
			if (! verbose)
				continue;
		}
		printf("%-8s", prefix);
		if (verbose)
			printf(" @%p", fli);
		if (fli->fli_mount != NULL)
			printf(" (%s)", fli->fli_mount->mnt_stat.f_mntonname);
		else
			printf(" NULL");
		if (fli->fli_trans_cnt == 0) {
			printf(" -");
		} else {
			switch (fli->fli_lock_type) {
			case FSTRANS_LAZY:
				printf(" lazy");
				break;
			case FSTRANS_SHARED:
				printf(" shared");
				break;
			case FSTRANS_EXCL:
				printf(" excl");
				break;
			default:
				printf(" %#x", fli->fli_lock_type);
				break;
			}
		}
		printf(" %d cow %d\n", fli->fli_trans_cnt, fli->fli_cow_cnt);
		prefix[0] = '\0';
	}
}

static void
fstrans_print_mount(struct mount *mp, int verbose)
{
	struct fstrans_mount_info *fmi;

	fmi = mp->mnt_transinfo;
	if (!verbose && (fmi == NULL || fmi->fmi_state == FSTRANS_NORMAL))
		return;

	printf("%-16s ", mp->mnt_stat.f_mntonname);
	if (fmi == NULL) {
		printf("(null)\n");
		return;
	}
	switch (fmi->fmi_state) {
	case FSTRANS_NORMAL:
		printf("state normal\n");
		break;
	case FSTRANS_SUSPENDING:
		printf("state suspending\n");
		break;
	case FSTRANS_SUSPENDED:
		printf("state suspended\n");
		break;
	default:
		printf("state %#x\n", fmi->fmi_state);
		break;
	}
}

void
fstrans_dump(int full)
{
	const struct proclist_desc *pd;
	struct proc *p;
	struct lwp *l;
	struct mount *mp;

	printf("Fstrans locks by lwp:\n");
	for (pd = proclists; pd->pd_list != NULL; pd++)
		PROCLIST_FOREACH(p, pd->pd_list)
			LIST_FOREACH(l, &p->p_lwps, l_sibling)
				fstrans_print_lwp(p, l, full == 1);

	printf("Fstrans state by mount:\n");
	TAILQ_FOREACH(mp, &mountlist, mnt_list)
		fstrans_print_mount(mp, full == 1);
}
#endif /* defined(DDB) */

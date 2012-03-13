/*	$NetBSD: chfs_gc.c,v 1.2 2011/11/24 21:09:37 agc Exp $	*/

/*-
 * Copyright (c) 2010 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * Copyright (c) 2010 Tamas Toth <ttoth@inf.u-szeged.hu>
 * Copyright (c) 2010 Adam Hoka <ahoka@NetBSD.org>
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by the Department of Software Engineering, University of Szeged, Hungary
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "chfs.h"

void chfs_gc_release_inode(struct chfs_mount *,
    struct chfs_inode *);
struct chfs_inode *chfs_gc_fetch_inode(struct chfs_mount *,
    ino_t, uint32_t);
int chfs_check(struct chfs_mount *, struct chfs_vnode_cache *);
void chfs_clear_inode(struct chfs_mount *, struct chfs_inode *);


struct chfs_eraseblock *find_gc_block(struct chfs_mount *);
int chfs_gcollect_pristine(struct chfs_mount *,
    struct chfs_eraseblock *,
    struct chfs_vnode_cache *, struct chfs_node_ref *);
int chfs_gcollect_live(struct chfs_mount *,
    struct chfs_eraseblock *, struct chfs_node_ref *,
    struct chfs_inode *);
int chfs_gcollect_vnode(struct chfs_mount *, struct chfs_inode *);
int chfs_gcollect_dirent(struct chfs_mount *,
    struct chfs_eraseblock *, struct chfs_inode *,
    struct chfs_dirent *);
int chfs_gcollect_deletion_dirent(struct chfs_mount *,
    struct chfs_eraseblock *, struct chfs_inode *,
    struct chfs_dirent *);
int chfs_gcollect_dnode(struct chfs_mount *,
    struct chfs_eraseblock *, struct chfs_inode *,
    struct chfs_full_dnode *, uint32_t, uint32_t);

/* must be called with chm_lock_mountfields held */
void
chfs_gc_trigger(struct chfs_mount *chmp)
{
	struct garbage_collector_thread *gc = &chmp->chm_gc_thread;

	//mutex_enter(&chmp->chm_lock_sizes);
	if (gc->gcth_running &&
	    chfs_gc_thread_should_wake(chmp)) {
		cv_signal(&gc->gcth_wakeup);
	}
	//mutex_exit(&chmp->chm_lock_sizes);
}


void
chfs_gc_thread(void *data)
{
	struct chfs_mount *chmp = data;
	struct garbage_collector_thread *gc = &chmp->chm_gc_thread;

	dbg_gc("[GC THREAD] thread started\n");

	mutex_enter(&chmp->chm_lock_mountfields);
	while (gc->gcth_running) {
		/* we must call chfs_gc_thread_should_wake with chm_lock_mountfields
		 * held, which is a bit awkwardly done here, but we cant relly
		 * do it otherway with the current design...
		 */
		if (chfs_gc_thread_should_wake(chmp)) {
//			mutex_exit(&chmp->chm_lock_mountfields);
			if (chfs_gcollect_pass(chmp) == ENOSPC) {
				dbg_gc("No space for garbage collection\n");
				panic("No space for garbage collection\n");
				/* XXX why break here? i have added a panic
				 * here to see if it gets triggered -ahoka
				 */
				break;
			}
			/* XXX gcollect_pass drops the mutex */
			mutex_enter(&chmp->chm_lock_mountfields);
		}

		cv_timedwait_sig(&gc->gcth_wakeup,
		    &chmp->chm_lock_mountfields, mstohz(100));
	}
	mutex_exit(&chmp->chm_lock_mountfields);

	dbg_gc("[GC THREAD] thread stopped\n");
	kthread_exit(0);
}

void
chfs_gc_thread_start(struct chfs_mount *chmp)
{
	struct garbage_collector_thread *gc = &chmp->chm_gc_thread;

	cv_init(&gc->gcth_wakeup, "chfsgccv");

	gc->gcth_running = true;
	kthread_create(PRI_NONE, /*KTHREAD_MPSAFE |*/ KTHREAD_MUSTJOIN,
	    NULL, chfs_gc_thread, chmp, &gc->gcth_thread,
	    "chfsgcth");
}

void
chfs_gc_thread_stop(struct chfs_mount *chmp)
{
	struct garbage_collector_thread *gc = &chmp->chm_gc_thread;

	/* check if it is actually running. if not, do nothing */
	if (gc->gcth_running) {
		gc->gcth_running = false;
	} else {
		return;
	}
	cv_signal(&gc->gcth_wakeup);
	dbg_gc("[GC THREAD] stop signal sent\n");

	kthread_join(gc->gcth_thread);
#ifdef BROKEN_KTH_JOIN
	kpause("chfsthjoin", false, mstohz(1000), NULL);
#endif

	cv_destroy(&gc->gcth_wakeup);
}

/* must be called with chm_lock_mountfields held */
int
chfs_gc_thread_should_wake(struct chfs_mount *chmp)
{
	int nr_very_dirty = 0;
	struct chfs_eraseblock *cheb;
	uint32_t dirty;

	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));

	if (!TAILQ_EMPTY(&chmp->chm_erase_pending_queue)) {
		dbg_gc("erase_pending\n");
		return 1;
	}

	if (chmp->chm_unchecked_size) {
		dbg_gc("unchecked\n");
		return 1;
	}

	dirty = chmp->chm_dirty_size - chmp->chm_nr_erasable_blocks *
	    chmp->chm_ebh->eb_size;

	if (chmp->chm_nr_free_blocks + chmp->chm_nr_erasable_blocks <
	    chmp->chm_resv_blocks_gctrigger && (dirty > chmp->chm_nospc_dirty)) {
		dbg_gc("free: %d + erasable: %d < resv: %d\n",
		    chmp->chm_nr_free_blocks, chmp->chm_nr_erasable_blocks,
		    chmp->chm_resv_blocks_gctrigger);
		dbg_gc("dirty: %d > nospc_dirty: %d\n",
		    dirty, chmp->chm_nospc_dirty);

		return 1;
	}

	TAILQ_FOREACH(cheb, &chmp->chm_very_dirty_queue, queue) {
		nr_very_dirty++;
		if (nr_very_dirty == chmp->chm_vdirty_blocks_gctrigger) {
			dbg_gc("nr_very_dirty\n");
			return 1;
		}
	}

	return 0;
}

void
chfs_gc_release_inode(struct chfs_mount *chmp,
    struct chfs_inode *ip)
{
	dbg_gc("release inode\n");
	//mutex_exit(&ip->inode_lock);
	//vput(ITOV(ip));
}

struct chfs_inode *
chfs_gc_fetch_inode(struct chfs_mount *chmp, ino_t vno,
    uint32_t unlinked)
{
	struct vnode *vp = NULL;
	struct chfs_vnode_cache *vc;
	struct chfs_inode *ip;
	dbg_gc("fetch inode %llu\n", (unsigned long long)vno);

	if (unlinked) {
		dbg_gc("unlinked\n");
		vp = chfs_vnode_lookup(chmp, vno);
		if (!vp) {
			mutex_enter(&chmp->chm_lock_vnocache);
			vc = chfs_vnode_cache_get(chmp, vno);
			if (!vc) {
				mutex_exit(&chmp->chm_lock_vnocache);
				return NULL;
			}
			if (vc->state != VNO_STATE_CHECKEDABSENT) {
				//sleep_on_spinunlock(&chmp->chm_lock_vnocache);
				mutex_exit(&chmp->chm_lock_vnocache);
				/* XXX why do we need the delay here?! */
//				kpause("chvncabs", true, mstohz(50), NULL);
				KASSERT(mutex_owned(&chmp->chm_lock_mountfields));
				cv_timedwait_sig(
					&chmp->chm_gc_thread.gcth_wakeup,
					&chmp->chm_lock_mountfields, mstohz(50));

//				KASSERT(!mutex_owned(&chmp->chm_lock_vnocache));
			} else {
				mutex_exit(&chmp->chm_lock_vnocache);
			}
			return NULL;
		}
	} else {
		dbg_gc("vnode lookup\n");
		vp = chfs_vnode_lookup(chmp, vno);
		//VFS_VGET(chmp->chm_fsmp, vno, &vp);
	}
	dbg_gc("vp to ip\n");
	ip = VTOI(vp);
	KASSERT(ip);
	//mutex_enter(&ip->inode_lock);

	return ip;
}

extern rb_tree_ops_t frag_rbtree_ops;

int
chfs_check(struct chfs_mount *chmp, struct  chfs_vnode_cache *chvc)
{
	struct chfs_inode *ip;
	struct vnode *vp;
	int ret;

	ip = pool_get(&chfs_inode_pool, PR_WAITOK);
	if (!ip) {
		return ENOMEM;
	}

	vp = kmem_zalloc(sizeof(struct vnode), KM_SLEEP);

	ip->chvc = chvc;
	ip->vp = vp;

	vp->v_data = ip;

	rb_tree_init(&ip->fragtree, &frag_rbtree_ops);
	TAILQ_INIT(&ip->dents);

	ret = chfs_read_inode_internal(chmp, ip);
	if (!ret) {
		chfs_clear_inode(chmp, ip);
	}

	pool_put(&chfs_inode_pool, ip);

	return ret;
}

void
chfs_clear_inode(struct chfs_mount *chmp, struct chfs_inode *ip)
{
	struct chfs_dirent *fd, *tmpfd;
	struct chfs_vnode_cache *chvc;


	/* XXX not sure if this is the correct locking */
//	mutex_enter(&chmp->chm_lock_vnocache);
	chvc = ip->chvc;
	/* shouldnt this be: */
	//bool deleted = (chvc && !(chvc->pvno || chvc->nlink));
	int deleted = (chvc && !(chvc->pvno | chvc->nlink));

	if (chvc && chvc->state != VNO_STATE_CHECKING) {
//		chfs_vnode_cache_state_set(chmp, chvc, VNO_STATE_CLEARING);
		chvc->state = VNO_STATE_CLEARING;
	}

	if (chvc->v && ((struct  chfs_vnode_cache *)chvc->v != chvc)) {
		if (deleted)
			chfs_mark_node_obsolete(chmp, chvc->v);
		//chfs_free_refblock(chvc->v);
	}
//	mutex_enter(&chmp->chm_lock_vnocache);

	chfs_kill_fragtree(&ip->fragtree);
/*
	fd = TAILQ_FIRST(&ip->dents);
	while (fd) {
		TAILQ_REMOVE(&ip->dents, fd, fds);
		chfs_free_dirent(fd);
		fd = TAILQ_FIRST(&ip->dents);
	}
*/

	TAILQ_FOREACH_SAFE(fd, &ip->dents, fds, tmpfd) {
		chfs_free_dirent(fd);
	}

	if (chvc && chvc->state == VNO_STATE_CHECKING) {
		chfs_vnode_cache_set_state(chmp,
		    chvc, VNO_STATE_CHECKEDABSENT);
		if ((struct chfs_vnode_cache *)chvc->v == chvc &&
		    (struct chfs_vnode_cache *)chvc->dirents == chvc &&
		    (struct chfs_vnode_cache *)chvc->dnode == chvc)
			chfs_vnode_cache_remove(chmp, chvc);
	}

}

struct chfs_eraseblock *
find_gc_block(struct chfs_mount *chmp)
{
	struct chfs_eraseblock *ret;
	struct chfs_eraseblock_queue *nextqueue;

	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));

	struct timespec now;
	vfs_timestamp(&now);

	int n = now.tv_nsec % 128;

	//dbg_gc("n = %d\n", n);
again:
/*    if (!TAILQ_EMPTY(&chmp->chm_bad_used_queue) && chmp->chm_nr_free_blocks > chmp->chm_nr_resv_blocks_gcbad) {
      dbg_gc("Picking block from bad_used_queue to GC next\n");
      nextqueue = &chmp->chm_bad_used_queue;
      } else */if (n<50 && !TAILQ_EMPTY(&chmp->chm_erase_pending_queue)) {
		dbg_gc("Picking block from erase_pending_queue to GC next\n");
		nextqueue = &chmp->chm_erase_pending_queue;
	} else if (n<110 && !TAILQ_EMPTY(&chmp->chm_very_dirty_queue) ) {
		dbg_gc("Picking block from very_dirty_queue to GC next\n");
		nextqueue = &chmp->chm_very_dirty_queue;
	} else if (n<126 && !TAILQ_EMPTY(&chmp->chm_dirty_queue) ) {
		dbg_gc("Picking block from dirty_queue to GC next\n");
		nextqueue = &chmp->chm_dirty_queue;
	} else if (!TAILQ_EMPTY(&chmp->chm_clean_queue)) {
		dbg_gc("Picking block from clean_queue to GC next\n");
		nextqueue = &chmp->chm_clean_queue;
	} else if (!TAILQ_EMPTY(&chmp->chm_dirty_queue)) {
		dbg_gc("Picking block from dirty_queue to GC next"
		    " (clean_queue was empty)\n");
		nextqueue = &chmp->chm_dirty_queue;
	} else if (!TAILQ_EMPTY(&chmp->chm_very_dirty_queue)) {
		dbg_gc("Picking block from very_dirty_queue to GC next"
		    " (clean_queue and dirty_queue were empty)\n");
		nextqueue = &chmp->chm_very_dirty_queue;
	} else if (!TAILQ_EMPTY(&chmp->chm_erase_pending_queue)) {
		dbg_gc("Picking block from erase_pending_queue to GC next"
		    " (clean_queue and {very_,}dirty_queue were empty)\n");
		nextqueue = &chmp->chm_erase_pending_queue;
	} else if (!TAILQ_EMPTY(&chmp->chm_erasable_pending_wbuf_queue)) {
		dbg_gc("Synching wbuf in order to reuse "
		    "erasable_pendig_wbuf_queue blocks\n");
		rw_enter(&chmp->chm_lock_wbuf, RW_WRITER);
		chfs_flush_pending_wbuf(chmp);
		rw_exit(&chmp->chm_lock_wbuf);
		goto again;
	} else {
		dbg_gc("CHFS: no clean, dirty _or_ erasable"
		    " blocks to GC from! Where are they all?\n");
		return NULL;
	}

	ret = TAILQ_FIRST(nextqueue);
	if (chmp->chm_nextblock) {
		dbg_gc("nextblock num: %u - gcblock num: %u\n",
		    chmp->chm_nextblock->lnr, ret->lnr);
		if (ret == chmp->chm_nextblock)
			goto again;
		//KASSERT(ret != chmp->chm_nextblock);
		//dbg_gc("first node lnr: %u ofs: %u\n", ret->first_node->lnr, ret->first_node->offset);
		//dbg_gc("last node lnr: %u ofs: %u\n", ret->last_node->lnr, ret->last_node->offset);
	}
	TAILQ_REMOVE(nextqueue, ret, queue);
	chmp->chm_gcblock = ret;
	ret->gc_node = ret->first_node;

	if (!ret->gc_node) {
		dbg_gc("Oops! ret->gc_node at LEB: %u is NULL\n", ret->lnr);
		panic("CHFS BUG - one LEB's gc_node is NULL\n");
	}

	/* TODO wasted size? */
	return ret;
}


int
chfs_gcollect_pass(struct chfs_mount *chmp)
{
	struct chfs_vnode_cache *vc;
	struct chfs_eraseblock *eb;
	struct chfs_node_ref *nref;
	uint32_t gcblock_dirty;
	struct chfs_inode *ip;
	ino_t vno, pvno;
	uint32_t nlink;
	int ret = 0;

	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));

//	mutex_enter(&chmp->chm_lock_mountfields);
	for (;;) {
		mutex_enter(&chmp->chm_lock_sizes);

		dbg_gc("unchecked size == %u\n", chmp->chm_unchecked_size);
		if (!chmp->chm_unchecked_size)
			break;

		if (chmp->chm_checked_vno > chmp->chm_max_vno) {
			mutex_exit(&chmp->chm_lock_sizes);
			mutex_exit(&chmp->chm_lock_mountfields);
			dbg_gc("checked_vno (#%llu) > max_vno (#%llu)\n",
			    (unsigned long long)chmp->chm_checked_vno,
			    (unsigned long long)chmp->chm_max_vno);
			return ENOSPC;
		}

		mutex_exit(&chmp->chm_lock_sizes);

		mutex_enter(&chmp->chm_lock_vnocache);
		dbg_gc("checking vno #%llu\n",
			(unsigned long long)chmp->chm_checked_vno);
		dbg_gc("get vnode cache\n");
		vc = chfs_vnode_cache_get(chmp, chmp->chm_checked_vno++);

		if (!vc) {
			dbg_gc("!vc\n");
			mutex_exit(&chmp->chm_lock_vnocache);
			continue;
		}

		if ((vc->pvno | vc->nlink) == 0) {
			dbg_gc("(pvno | nlink) == 0\n");
			mutex_exit(&chmp->chm_lock_vnocache);
			continue;
		}

		dbg_gc("switch\n");
		switch (vc->state) {
		case VNO_STATE_CHECKEDABSENT:
		case VNO_STATE_PRESENT:
			mutex_exit(&chmp->chm_lock_vnocache);
			continue;

		case VNO_STATE_GC:
		case VNO_STATE_CHECKING:
			mutex_exit(&chmp->chm_lock_vnocache);
			mutex_exit(&chmp->chm_lock_mountfields);
			dbg_gc("VNO_STATE GC or CHECKING\n");
			panic("CHFS BUG - vc state gc or checking\n");

		case VNO_STATE_READING:
			chmp->chm_checked_vno--;
			mutex_exit(&chmp->chm_lock_vnocache);
			/* XXX why do we need the delay here?! */
			kpause("chvncrea", true, mstohz(50), NULL);

//			sleep_on_spinunlock(&chmp->chm_lock_vnocache);
//			KASSERT(!mutex_owned(&chmp->chm_lock_vnocache));
			mutex_exit(&chmp->chm_lock_mountfields);
			return 0;

		default:
			mutex_exit(&chmp->chm_lock_vnocache);
			mutex_exit(&chmp->chm_lock_mountfields);
			dbg_gc("default\n");
			panic("CHFS BUG - vc state is other what we"
			    " checked\n");

		case VNO_STATE_UNCHECKED:
			;
		}

		chfs_vnode_cache_set_state(chmp, vc, VNO_STATE_CHECKING);

		/* XXX check if this is too heavy to call under
		 * chm_lock_vnocache
		 */
		ret = chfs_check(chmp, vc);
		dbg_gc("set state\n");
		chfs_vnode_cache_set_state(chmp,
		    vc, VNO_STATE_CHECKEDABSENT);

		mutex_exit(&chmp->chm_lock_vnocache);
		mutex_exit(&chmp->chm_lock_mountfields);

		return ret;
	}


	eb = chmp->chm_gcblock;

	if (!eb) {
		eb = find_gc_block(chmp);
	}

	if (!eb) {
		dbg_gc("!eb\n");
		if (!TAILQ_EMPTY(&chmp->chm_erase_pending_queue)) {
			mutex_exit(&chmp->chm_lock_sizes);
			mutex_exit(&chmp->chm_lock_mountfields);
			return EAGAIN;
		}
		mutex_exit(&chmp->chm_lock_sizes);
		mutex_exit(&chmp->chm_lock_mountfields);
		return EIO;
	}

	if (!eb->used_size) {
		dbg_gc("!eb->used_size\n");
		goto eraseit;
	}

	nref = eb->gc_node;
	//dbg_gc("gc use: %u\n", chmp->chm_nextblock->lnr);
	//dbg_gc("nref: %u %u\n", nref->nref_lnr, nref->nref_offset);
	gcblock_dirty = eb->dirty_size;

	while(CHFS_REF_OBSOLETE(nref)) {
		//dbg_gc("obsoleted nref lnr: %u - offset: %u\n", nref->nref_lnr, nref->nref_offset);
#ifdef DBG_MSG_GC
		if (nref == chmp->chm_blocks[nref->nref_lnr].last_node) {
			dbg_gc("THIS NODE IS THE LAST NODE OF ITS EB\n");
		}
#endif
		nref = node_next(nref);
		if (!nref) {
			//dbg_gc("!nref\n");
			eb->gc_node = nref;
			mutex_exit(&chmp->chm_lock_sizes);
			mutex_exit(&chmp->chm_lock_mountfields);
			panic("CHFS BUG - nref is NULL)\n");
		}
	}
	eb->gc_node = nref;
	//dbg_gc("nref the chosen one lnr: %u - offset: %u\n", nref->nref_lnr, nref->nref_offset);
	KASSERT(nref->nref_lnr == chmp->chm_gcblock->lnr);

	if (!nref->nref_next) {
		//dbg_gc("!nref->nref_next\n");
		mutex_exit(&chmp->chm_lock_sizes);
		if (CHFS_REF_FLAGS(nref) == CHFS_PRISTINE_NODE_MASK) {
			chfs_gcollect_pristine(chmp, eb, NULL, nref);
		} else {
			chfs_mark_node_obsolete(chmp, nref);
		}
		goto lock_size;
	}

	dbg_gc("nref lnr: %u - offset: %u\n", nref->nref_lnr, nref->nref_offset);
	vc = chfs_nref_to_vc(nref);

	mutex_exit(&chmp->chm_lock_sizes);

	//dbg_gc("enter vnocache lock on #%llu\n", vc->vno);
	mutex_enter(&chmp->chm_lock_vnocache);

	dbg_gc("switch\n");
	switch(vc->state) {
        case VNO_STATE_CHECKEDABSENT:
		if (CHFS_REF_FLAGS(nref) == CHFS_PRISTINE_NODE_MASK) {
			chfs_vnode_cache_set_state(chmp, vc, VNO_STATE_GC);
		}
		break;

        case VNO_STATE_PRESENT:
		break;

        case VNO_STATE_UNCHECKED:
        case VNO_STATE_CHECKING:
        case VNO_STATE_GC:
		mutex_exit(&chmp->chm_lock_vnocache);
		mutex_exit(&chmp->chm_lock_mountfields);
		panic("CHFS BUG - vc state unchecked,"
		    " checking or gc (vno #%llu, num #%d)\n",
		    (unsigned long long)vc->vno, vc->state);

        case VNO_STATE_READING:
		mutex_exit(&chmp->chm_lock_vnocache);
		/* XXX why do we need the delay here?! */
		kpause("chvncrea", true, mstohz(50), NULL);

//		sleep_on_spinunlock(&chmp->chm_lock_vnocache);
//		KASSERT(!mutex_owned(&chmp->chm_lock_vnocache));
		mutex_exit(&chmp->chm_lock_mountfields);
		return 0;
	}

	if (vc->state == VNO_STATE_GC) {
		dbg_gc("vc->state == VNO_STATE_GC\n");
		mutex_exit(&chmp->chm_lock_vnocache);
		ret = chfs_gcollect_pristine(chmp, eb, NULL, nref);

//		chfs_vnode_cache_state_set(chmp,
//		    vc, VNO_STATE_CHECKEDABSENT);
		/* XXX locking? */
		vc->state = VNO_STATE_CHECKEDABSENT;
		//TODO wake_up(&chmp->chm_vnocache_wq);
		if (ret != EBADF)
			goto test_gcnode;
		mutex_enter(&chmp->chm_lock_vnocache);
	}

	vno = vc->vno;
	pvno = vc->pvno;
	nlink = vc->nlink;
	mutex_exit(&chmp->chm_lock_vnocache);

	ip = chfs_gc_fetch_inode(chmp, vno, !(pvno | nlink));

	if (!ip) {
		dbg_gc("!ip\n");
		ret = 0;
		goto lock_size;
	}

	chfs_gcollect_live(chmp, eb, nref, ip);

	chfs_gc_release_inode(chmp, ip);

test_gcnode:
	if (eb->dirty_size == gcblock_dirty &&
	    !CHFS_REF_OBSOLETE(eb->gc_node)) {
		dbg_gc("ERROR collecting node at %u failed.\n",
		    CHFS_GET_OFS(eb->gc_node->nref_offset));

		ret = ENOSPC;
	}

lock_size:
	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));
	mutex_enter(&chmp->chm_lock_sizes);
eraseit:
	dbg_gc("eraseit\n");

	if (chmp->chm_gcblock) {
		dbg_gc("eb used size = %u\n", chmp->chm_gcblock->used_size);
		dbg_gc("eb free size = %u\n", chmp->chm_gcblock->free_size);
		dbg_gc("eb dirty size = %u\n", chmp->chm_gcblock->dirty_size);
		dbg_gc("eb unchecked size = %u\n",
		    chmp->chm_gcblock->unchecked_size);
		dbg_gc("eb wasted size = %u\n", chmp->chm_gcblock->wasted_size);

		KASSERT(chmp->chm_gcblock->used_size + chmp->chm_gcblock->free_size +
		    chmp->chm_gcblock->dirty_size +
		    chmp->chm_gcblock->unchecked_size +
		    chmp->chm_gcblock->wasted_size == chmp->chm_ebh->eb_size);

	}

	if (chmp->chm_gcblock && chmp->chm_gcblock->dirty_size +
	    chmp->chm_gcblock->wasted_size == chmp->chm_ebh->eb_size) {
		dbg_gc("Block at leb #%u completely obsoleted by GC, "
		    "Moving to erase_pending_queue\n", chmp->chm_gcblock->lnr);
		TAILQ_INSERT_TAIL(&chmp->chm_erase_pending_queue,
		    chmp->chm_gcblock, queue);
		chmp->chm_gcblock = NULL;
		chmp->chm_nr_erasable_blocks++;
		if (!TAILQ_EMPTY(&chmp->chm_erase_pending_queue)) {
			ret = chfs_remap_leb(chmp);
		}
	}

	mutex_exit(&chmp->chm_lock_sizes);
	mutex_exit(&chmp->chm_lock_mountfields);
	dbg_gc("return\n");
	return ret;
}


int
chfs_gcollect_pristine(struct chfs_mount *chmp, struct chfs_eraseblock *cheb,
    struct chfs_vnode_cache *chvc, struct chfs_node_ref *nref)
{
	struct chfs_node_ref *newnref;
	struct chfs_flash_node_hdr *nhdr;
	struct chfs_flash_vnode *fvnode;
	struct chfs_flash_dirent_node *fdirent;
	struct chfs_flash_data_node *fdata;
	int ret, retries = 0;
	uint32_t ofs, crc;
	size_t totlen = chfs_nref_len(chmp, cheb, nref);
	char *data;
	struct iovec vec;
	size_t retlen;

	dbg_gc("gcollect_pristine\n");

	data = kmem_alloc(totlen, KM_SLEEP);
	if (!data)
		return ENOMEM;

	ofs = CHFS_GET_OFS(nref->nref_offset);

	ret = chfs_read_leb(chmp, nref->nref_lnr, data, ofs, totlen, &retlen);
	if (ret) {
		dbg_gc("reading error\n");
		return ret;
	}
	if (retlen != totlen) {
		dbg_gc("read size error\n");
		return EIO;
	}
	nhdr = (struct chfs_flash_node_hdr *)data;
	/* check the header */
	if (le16toh(nhdr->magic) != CHFS_FS_MAGIC_BITMASK) {
		dbg_gc("node header magic number error\n");
		return EBADF;
	}
	crc = crc32(0, (uint8_t *)nhdr, CHFS_NODE_HDR_SIZE - 4);
	if (crc != le32toh(nhdr->hdr_crc)) {
		dbg_gc("node header crc error\n");
		return EBADF;
	}

	switch(le16toh(nhdr->type)) {
        case CHFS_NODETYPE_VNODE:
		fvnode = (struct chfs_flash_vnode *)data;
	        crc = crc32(0, (uint8_t *)fvnode, sizeof(struct chfs_flash_vnode) - 4);
	        if (crc != le32toh(fvnode->node_crc)) {
			dbg_gc("vnode crc error\n");
			return EBADF;
		}
		break;
        case CHFS_NODETYPE_DIRENT:
		fdirent = (struct chfs_flash_dirent_node *)data;
	        crc = crc32(0, (uint8_t *)fdirent, sizeof(struct chfs_flash_dirent_node) - 4);
	        if (crc != le32toh(fdirent->node_crc)) {
			dbg_gc("dirent crc error\n");
			return EBADF;
		}
	        crc = crc32(0, fdirent->name, fdirent->nsize);
	        if (crc != le32toh(fdirent->name_crc)) {
			dbg_gc("dirent name crc error\n");
			return EBADF;
		}
		break;
        case CHFS_NODETYPE_DATA:
		fdata = (struct chfs_flash_data_node *)data;
	        crc = crc32(0, (uint8_t *)fdata, sizeof(struct chfs_flash_data_node) - 4);
	        if (crc != le32toh(fdata->node_crc)) {
			dbg_gc("data node crc error\n");
			return EBADF;
		}
		break;
        default:
		if (chvc) {
			dbg_gc("unknown node have vnode cache\n");
			return EBADF;
		}
	}
	/* CRC's OK, write node to its new place */
retry:
	ret = chfs_reserve_space_gc(chmp, totlen);
	if (ret)
		return ret;

	newnref = chfs_alloc_node_ref(chmp->chm_nextblock);
	if (!newnref)
		return ENOMEM;

	ofs = chmp->chm_ebh->eb_size - chmp->chm_nextblock->free_size;
	newnref->nref_offset = ofs;

	vec.iov_base = (void *)data;
	vec.iov_len = totlen;
	mutex_enter(&chmp->chm_lock_sizes);
	ret = chfs_write_wbuf(chmp, &vec, 1, ofs, &retlen);

	if (ret || retlen != totlen) {
		chfs_err("error while writing out to the media\n");
		chfs_err("err: %d | size: %zu | retlen : %zu\n",
		    ret, totlen, retlen);

		chfs_change_size_dirty(chmp, chmp->chm_nextblock, totlen);
		if (retries) {
			mutex_exit(&chmp->chm_lock_sizes);
			return EIO;
		}

		retries++;
		mutex_exit(&chmp->chm_lock_sizes);
		goto retry;
	}

	mutex_exit(&chmp->chm_lock_sizes);
	//TODO should we set free_size?
	chfs_mark_node_obsolete(chmp, nref);
	chfs_add_vnode_ref_to_vc(chmp, chvc, newnref);
	return 0;
}


int
chfs_gcollect_live(struct chfs_mount *chmp,
    struct chfs_eraseblock *cheb, struct chfs_node_ref *nref,
    struct chfs_inode *ip)
{
	struct chfs_node_frag *frag;
	struct chfs_full_dnode *fn = NULL;
	int start = 0, end = 0, nrfrags = 0;
	struct chfs_dirent *fd = NULL;
	int ret = 0;
	bool is_dirent;

	dbg_gc("gcollect_live\n");

	if (chmp->chm_gcblock != cheb) {
		dbg_gc("GC block is no longer gcblock. Restart.\n");
		goto upnout;
	}

	if (CHFS_REF_OBSOLETE(nref)) {
		dbg_gc("node to be GC'd was obsoleted in the meantime.\n");
		goto upnout;
	}

	/* It's a vnode? */
	if (ip->chvc->v == nref) {
		chfs_gcollect_vnode(chmp, ip);
		goto upnout;
	}

	/* find fn */
	dbg_gc("find full dnode\n");
	for(frag = frag_first(&ip->fragtree);
	    frag; frag = frag_next(&ip->fragtree, frag)) {
		if (frag->node && frag->node->nref == nref) {
			fn = frag->node;
			end = frag->ofs + frag->size;
			if (!nrfrags++)
				start = frag->ofs;
			if (nrfrags == frag->node->frags)
				break;
		}
	}

	/* It's a pristine node, or dnode (or hole? XXX have we hole nodes?) */
	if (fn) {
		if (CHFS_REF_FLAGS(nref) == CHFS_PRISTINE_NODE_MASK) {
			ret = chfs_gcollect_pristine(chmp,
			    cheb, ip->chvc, nref);
			if (!ret) {
				frag->node->nref = ip->chvc->v;
			}
			if (ret != EBADF)
				goto upnout;
		}
		//ret = chfs_gcollect_hole(chmp, cheb, ip, fn, start, end);
		ret = chfs_gcollect_dnode(chmp, cheb, ip, fn, start, end);
		goto upnout;
	}


	/* It's a dirent? */
	dbg_gc("find full dirent\n");
	is_dirent = false;
	TAILQ_FOREACH(fd, &ip->dents, fds) {
		if (fd->nref == nref) {
			is_dirent = true;
			break;
		}
	}

	if (is_dirent && fd->vno) {
		ret = chfs_gcollect_dirent(chmp, cheb, ip, fd);
	} else if (is_dirent) {
		ret = chfs_gcollect_deletion_dirent(chmp, cheb, ip, fd);
	} else {
		dbg_gc("Nref at leb #%u offset 0x%08x wasn't in node list"
		    " for ino #%llu\n",
		    nref->nref_lnr, CHFS_GET_OFS(nref->nref_offset),
		    (unsigned long long)ip->ino);
		if (CHFS_REF_OBSOLETE(nref)) {
			dbg_gc("But it's obsolete so we don't mind"
			    " too much.\n");
		}
	}

upnout:
	return ret;
}

int
chfs_gcollect_vnode(struct chfs_mount *chmp, struct chfs_inode *ip)
{
	int ret;
	dbg_gc("gcollect_vnode\n");

	ret = chfs_write_flash_vnode(chmp, ip, ALLOC_GC);

	return ret;
}

int
chfs_gcollect_dirent(struct chfs_mount *chmp,
    struct chfs_eraseblock *cheb, struct chfs_inode *parent,
    struct chfs_dirent *fd)
{
	struct vnode *vnode = NULL;
	struct chfs_inode *ip;
	struct chfs_node_ref *prev;
	dbg_gc("gcollect_dirent\n");

	vnode = chfs_vnode_lookup(chmp, fd->vno);

	/* XXX maybe KASSERT or panic on this? */
	if (vnode == NULL) {
		return ENOENT;
	}

	ip = VTOI(vnode);

	prev = parent->chvc->dirents;
	if (prev == fd->nref) {
		parent->chvc->dirents = prev->nref_next;
		dbg_gc("fd nref removed from dirents list\n");
		prev = NULL;
	}
	while (prev) {
		if (prev->nref_next == fd->nref) {
			prev->nref_next = fd->nref->nref_next;
			dbg_gc("fd nref removed from dirents list\n");
			break;
		}
		prev = prev->nref_next;
	}

	prev = fd->nref;
	chfs_mark_node_obsolete(chmp, fd->nref);
	return chfs_write_flash_dirent(chmp,
	    parent, ip, fd, fd->vno, ALLOC_GC);
}

/* Check dirents what are marked as deleted. */
int
chfs_gcollect_deletion_dirent(struct chfs_mount *chmp,
    struct chfs_eraseblock *cheb, struct chfs_inode *parent,
    struct chfs_dirent *fd)
{
	struct chfs_flash_dirent_node chfdn;
	struct chfs_node_ref *nref;
	size_t retlen, name_len, nref_len;
	uint32_t name_crc;

	int ret;

	struct vnode *vnode = NULL;

	dbg_gc("gcollect_deletion_dirent\n");

	name_len = strlen(fd->name);
	name_crc = crc32(0, fd->name, name_len);

	nref_len = chfs_nref_len(chmp, cheb, fd->nref);

	vnode = chfs_vnode_lookup(chmp, fd->vno);

	//dbg_gc("ip from vnode\n");
	//VFS_VGET(chmp->chm_fsmp, fd->vno, &vnode);
	//ip = VTOI(vnode);
	//vput(vnode);

	//dbg_gc("mutex enter erase_completion_lock\n");

//	dbg_gc("alloc chfdn\n");
//	chfdn = kmem_alloc(nref_len, KM_SLEEP);
//	if (!chfdn)
//		return ENOMEM;

	for (nref = parent->chvc->dirents;
	     nref != (void*)parent->chvc;
	     nref = nref->nref_next) {

		if (!CHFS_REF_OBSOLETE(nref))
			continue;

		/* if node refs have different length, skip */
		if (chfs_nref_len(chmp, NULL, nref) != nref_len)
			continue;

		if (CHFS_GET_OFS(nref->nref_offset) ==
		    CHFS_GET_OFS(fd->nref->nref_offset)) {
			continue;
		}

		ret = chfs_read_leb(chmp,
		    nref->nref_lnr, (void*)&chfdn, CHFS_GET_OFS(nref->nref_offset),
		    nref_len, &retlen);

		if (ret) {
			dbg_gc("Read error: %d\n", ret);
			continue;
		}

		if (retlen != nref_len) {
			dbg_gc("Error reading node:"
			    " read: %zu insted of: %zu\n", retlen, nref_len);
			continue;
		}

		/* if node type doesn't match, skip */
		if (le16toh(chfdn.type) != CHFS_NODETYPE_DIRENT)
			continue;

		/* if crc doesn't match, skip */
		if (le32toh(chfdn.name_crc) != name_crc)
			continue;

		/* if length of name different, or this is an another deletion
		 * dirent, skip
		 */
		if (chfdn.nsize != name_len || !le64toh(chfdn.vno))
			continue;

		/* check actual name */
		if (memcmp(chfdn.name, fd->name, name_len))
			continue;

//		kmem_free(chfdn, nref_len);

		chfs_mark_node_obsolete(chmp, fd->nref);
		return chfs_write_flash_dirent(chmp,
		    parent, NULL, fd, fd->vno, ALLOC_GC);
	}

//	kmem_free(chfdn, nref_len);

	TAILQ_REMOVE(&parent->dents, fd, fds);
	chfs_free_dirent(fd);
	return 0;
}

int
chfs_gcollect_dnode(struct chfs_mount *chmp,
    struct chfs_eraseblock *orig_cheb, struct chfs_inode *ip,
    struct chfs_full_dnode *fn, uint32_t orig_start, uint32_t orig_end)
{
	struct chfs_node_ref *nref, *prev;
	struct chfs_full_dnode *newfn;
	struct chfs_flash_data_node *fdnode;
	int ret = 0, retries = 0;
	uint32_t totlen;
	char *data = NULL;
	struct iovec vec;
	size_t retlen;
	dbg_gc("gcollect_dnode\n");

	//uint32_t used_size;

/* TODO GC merging frags, should we use it?

   uint32_t start, end;

   start = orig_start;
   end = orig_end;

   if (chmp->chm_nr_free_blocks + chmp->chm_nr_erasable_blocks > chmp->chm_resv_blocks_gcmerge) {
   struct chfs_node_frag *frag;
   uint32_t min, max;

   min = start & (PAGE_CACHE_SIZE-1);
   max = min + PAGE_CACHE_SIZE;

   frag = (struct chfs_node_frag *)rb_tree_find_node_leq(&ip->i_chfs_ext.fragtree, &start);
   KASSERT(frag->ofs == start);

   while ((frag = frag_prev(&ip->i_chfs_ext.fragtree, frag)) && frag->ofs >= min) {
   if (frag->ofs > min) {
   start = frag->ofs;
   continue;
   }

   if (!frag->node || !frag->node->nref) {
   break;
   } else {
   struct chfs_node_ref *nref = frag->node->nref;
   struct chfs_eraseblock *cheb;

   cheb = &chmp->chm_blocks[nref->nref_lnr];

   if (cheb == chmp->chm_gcblock)
   start = frag->ofs;

   //TODO is this a clean block?

   start = frag->ofs;
   break;
   }
   }

   end--;
   frag = (struct chfs_node_frag *)rb_tree_find_node_leq(&ip->i_chfs_ext.fragtree, &(end));

   while ((frag = frag_next(&ip->i_chfs_ext.fragtree, frag)) && (frag->ofs + frag->size <= max)) {
   if (frag->ofs + frag->size < max) {
   end = frag->ofs + frag->size;
   continue;
   }

   if (!frag->node || !frag->node->nref) {
   break;
   } else {
   struct chfs_node_ref *nref = frag->node->nref;
   struct chfs_eraseblock *cheb;

   cheb = &chmp->chm_blocks[nref->nref_lnr];

   if (cheb == chmp->chm_gcblock)
   end = frag->ofs + frag->size;

   //TODO is this a clean block?

   end = frag->ofs + frag->size;
   break;
   }
   }

   KASSERT(end <=
   frag_last(&ip->i_chfs_ext.fragtree)->ofs +
   frag_last(&ip->i_chfs_ext.fragtree)->size);
   KASSERT(end >= orig_end);
   KASSERT(start <= orig_start);
   }
*/
	KASSERT(orig_cheb->lnr == fn->nref->nref_lnr);
	totlen = chfs_nref_len(chmp, orig_cheb, fn->nref);
	data = kmem_alloc(totlen, KM_SLEEP);

	ret = chfs_read_leb(chmp, fn->nref->nref_lnr, data, fn->nref->nref_offset,
	    totlen, &retlen);

	fdnode = (struct chfs_flash_data_node *)data;
	fdnode->version = htole64(++ip->chvc->highest_version);
	fdnode->node_crc = htole32(crc32(0, (uint8_t *)fdnode,
		sizeof(*fdnode) - 4));

	vec.iov_base = (void *)data;
	vec.iov_len = totlen;

retry:
	ret = chfs_reserve_space_gc(chmp, totlen);
	if (ret)
		goto out;

	nref = chfs_alloc_node_ref(chmp->chm_nextblock);
	if (!nref) {
		ret = ENOMEM;
		goto out;
	}

	mutex_enter(&chmp->chm_lock_sizes);

	nref->nref_offset = chmp->chm_ebh->eb_size - chmp->chm_nextblock->free_size;
	KASSERT(nref->nref_offset % 4 == 0);
	chfs_change_size_free(chmp, chmp->chm_nextblock, -totlen);

	ret = chfs_write_wbuf(chmp, &vec, 1, nref->nref_offset, &retlen);
	if (ret || retlen != totlen) {
		chfs_err("error while writing out to the media\n");
		chfs_err("err: %d | size: %d | retlen : %zu\n",
		    ret, totlen, retlen);
		chfs_change_size_dirty(chmp, chmp->chm_nextblock, totlen);
		if (retries) {
			ret = EIO;
			mutex_exit(&chmp->chm_lock_sizes);
			goto out;
		}

		retries++;
		mutex_exit(&chmp->chm_lock_sizes);
		goto retry;
	}

	dbg_gc("new nref lnr: %u - offset: %u\n", nref->nref_lnr, nref->nref_offset);

	chfs_change_size_used(chmp, &chmp->chm_blocks[nref->nref_lnr], totlen);
	mutex_exit(&chmp->chm_lock_sizes);
	KASSERT(chmp->chm_blocks[nref->nref_lnr].used_size <= chmp->chm_ebh->eb_size);

	newfn = chfs_alloc_full_dnode();
	newfn->nref = nref;
	newfn->ofs = fn->ofs;
	newfn->size = fn->size;
	newfn->frags = fn->frags;

	//TODO should we remove fd from dnode list?

	prev = ip->chvc->dnode;
	if (prev == fn->nref) {
		ip->chvc->dnode = prev->nref_next;
		prev = NULL;
	}
	while (prev) {
		if (prev->nref_next == fn->nref) {
			prev->nref_next = fn->nref->nref_next;
			break;
		}
		prev = prev->nref_next;
	}

	chfs_add_full_dnode_to_inode(chmp, ip, newfn);
	chfs_add_node_to_list(chmp,
	    ip->chvc, newfn->nref, &ip->chvc->dnode);

out:
	kmem_free(data, totlen);
	return ret;
}

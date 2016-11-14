/*	$NetBSD: vfs_wapbl.c,v 1.62 2015/08/09 07:40:59 mlelstv Exp $	*/

/*-
 * Copyright (c) 2003, 2008, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Wasabi Systems, Inc.
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

/*
 * This implements file system independent write ahead filesystem logging.
 */

#define WAPBL_INTERNAL

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vfs_wapbl.c,v 1.62 2015/08/09 07:40:59 mlelstv Exp $");

#include <sys/param.h>
#include <sys/bitops.h>

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/module.h>
#include <sys/resourcevar.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/kernel.h>
#include <sys/kauth.h>
#include <sys/mutex.h>
#include <sys/atomic.h>
#include <sys/wapbl.h>
#include <sys/wapbl_replay.h>

#include <miscfs/specfs/specdev.h>

#define	wapbl_alloc(s) kmem_alloc((s), KM_SLEEP)
#define	wapbl_free(a, s) kmem_free((a), (s))
#define	wapbl_calloc(n, s) kmem_zalloc((n)*(s), KM_SLEEP)

static struct sysctllog *wapbl_sysctl;
static int wapbl_flush_disk_cache = 1;
static int wapbl_verbose_commit = 0;

static inline size_t wapbl_space_free(size_t, off_t, off_t);

#else /* !_KERNEL */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <sys/time.h>
#include <sys/wapbl.h>
#include <sys/wapbl_replay.h>

#define	KDASSERT(x) assert(x)
#define	KASSERT(x) assert(x)
#define	wapbl_alloc(s) malloc(s)
#define	wapbl_free(a, s) free(a)
#define	wapbl_calloc(n, s) calloc((n), (s))

#endif /* !_KERNEL */

/*
 * INTERNAL DATA STRUCTURES
 */

/* 
 * This structure holds per-mount log information.
 *
 * Legend:	a = atomic access only
 *		r = read-only after init
 *		l = rwlock held
 *		m = mutex held
 *		lm = rwlock held writing or mutex held
 *		u = unlocked access ok
 *		b = bufcache_lock held
 */
LIST_HEAD(wapbl_ino_head, wapbl_ino);
struct wapbl {
	struct vnode *wl_logvp;	/* r:	log here */
	struct vnode *wl_devvp;	/* r:	log on this device */
	struct mount *wl_mount;	/* r:	mountpoint wl is associated with */
	daddr_t wl_logpbn;	/* r:	Physical block number of start of log */
	int wl_log_dev_bshift;	/* r:	logarithm of device block size of log
					device */
	int wl_fs_dev_bshift;	/* r:	logarithm of device block size of
					filesystem device */

	unsigned wl_lock_count;	/* m:	Count of transactions in progress */

	size_t wl_circ_size; 	/* r:	Number of bytes in buffer of log */
	size_t wl_circ_off;	/* r:	Number of bytes reserved at start */

	size_t wl_bufcount_max;	/* r:	Number of buffers reserved for log */
	size_t wl_bufbytes_max;	/* r:	Number of buf bytes reserved for log */

	off_t wl_head;		/* l:	Byte offset of log head */
	off_t wl_tail;		/* l:	Byte offset of log tail */
	/*
	 * head == tail == 0 means log is empty
	 * head == tail != 0 means log is full
	 * see assertions in wapbl_advance() for other boundary conditions.
	 * only truncate moves the tail, except when flush sets it to
	 * wl_header_size only flush moves the head, except when truncate
	 * sets it to 0.
	 */

	struct wapbl_wc_header *wl_wc_header;	/* l	*/
	void *wl_wc_scratch;	/* l:	scratch space (XXX: por que?!?) */

	kmutex_t wl_mtx;	/* u:	short-term lock */
	krwlock_t wl_rwlock;	/* u:	File system transaction lock */

	/*
	 * Must be held while accessing
	 * wl_count or wl_bufs or head or tail
	 */

	/*
	 * Callback called from within the flush routine to flush any extra
	 * bits.  Note that flush may be skipped without calling this if
	 * there are no outstanding buffers in the transaction.
	 */
#if _KERNEL
	wapbl_flush_fn_t wl_flush;	/* r	*/
	wapbl_flush_fn_t wl_flush_abort;/* r	*/
#endif

	size_t wl_bufbytes;	/* m:	Byte count of pages in wl_bufs */
	size_t wl_bufcount;	/* m:	Count of buffers in wl_bufs */
	size_t wl_bcount;	/* m:	Total bcount of wl_bufs */

	LIST_HEAD(, buf) wl_bufs; /* m:	Buffers in current transaction */

	kcondvar_t wl_reclaimable_cv;	/* m (obviously) */
	size_t wl_reclaimable_bytes; /* m:	Amount of space available for
						reclamation by truncate */
	int wl_error_count;	/* m:	# of wl_entries with errors */
	size_t wl_reserved_bytes; /* never truncate log smaller than this */

#ifdef WAPBL_DEBUG_BUFBYTES
	size_t wl_unsynced_bufbytes; /* Byte count of unsynced buffers */
#endif

	daddr_t *wl_deallocblks;/* lm:	address of block */
	int *wl_dealloclens;	/* lm:	size of block */
	int wl_dealloccnt;	/* lm:	total count */
	int wl_dealloclim;	/* l:	max count */

	/* hashtable of inode numbers for allocated but unlinked inodes */
	/* synch ??? */
	struct wapbl_ino_head *wl_inohash;
	u_long wl_inohashmask;
	int wl_inohashcnt;

	SIMPLEQ_HEAD(, wapbl_entry) wl_entries; /* On disk transaction
						   accounting */

	u_char *wl_buffer;	/* l:   buffer for wapbl_buffered_write() */
	daddr_t wl_buffer_dblk;	/* l:   buffer disk block address */
	size_t wl_buffer_used;	/* l:   buffer current use */
};

#ifdef WAPBL_DEBUG_PRINT
int wapbl_debug_print = WAPBL_DEBUG_PRINT;
#endif

/****************************************************************/
#ifdef _KERNEL

#ifdef WAPBL_DEBUG
struct wapbl *wapbl_debug_wl;
#endif

static int wapbl_write_commit(struct wapbl *wl, off_t head, off_t tail);
static int wapbl_write_blocks(struct wapbl *wl, off_t *offp);
static int wapbl_write_revocations(struct wapbl *wl, off_t *offp);
static int wapbl_write_inodes(struct wapbl *wl, off_t *offp);
#endif /* _KERNEL */

static int wapbl_replay_process(struct wapbl_replay *wr, off_t, off_t);

static inline size_t wapbl_space_used(size_t avail, off_t head,
	off_t tail);

#ifdef _KERNEL

static struct pool wapbl_entry_pool;

#define	WAPBL_INODETRK_SIZE 83
static int wapbl_ino_pool_refcount;
static struct pool wapbl_ino_pool;
struct wapbl_ino {
	LIST_ENTRY(wapbl_ino) wi_hash;
	ino_t wi_ino;
	mode_t wi_mode;
};

static void wapbl_inodetrk_init(struct wapbl *wl, u_int size);
static void wapbl_inodetrk_free(struct wapbl *wl);
static struct wapbl_ino *wapbl_inodetrk_get(struct wapbl *wl, ino_t ino);

static size_t wapbl_transaction_len(struct wapbl *wl);
static inline size_t wapbl_transaction_inodes_len(struct wapbl *wl);

#if 0
int wapbl_replay_verify(struct wapbl_replay *, struct vnode *);
#endif

static int wapbl_replay_isopen1(struct wapbl_replay *);

/*
 * This is useful for debugging.  If set, the log will
 * only be truncated when necessary.
 */
int wapbl_lazy_truncate = 0;

struct wapbl_ops wapbl_ops = {
	.wo_wapbl_discard	= wapbl_discard,
	.wo_wapbl_replay_isopen	= wapbl_replay_isopen1,
	.wo_wapbl_replay_can_read = wapbl_replay_can_read,
	.wo_wapbl_replay_read	= wapbl_replay_read,
	.wo_wapbl_add_buf	= wapbl_add_buf,
	.wo_wapbl_remove_buf	= wapbl_remove_buf,
	.wo_wapbl_resize_buf	= wapbl_resize_buf,
	.wo_wapbl_begin		= wapbl_begin,
	.wo_wapbl_end		= wapbl_end,
	.wo_wapbl_junlock_assert= wapbl_junlock_assert,

	/* XXX: the following is only used to say "this is a wapbl buf" */
	.wo_wapbl_biodone	= wapbl_biodone,
};

static int
wapbl_sysctl_init(void)
{
	int rv;
	const struct sysctlnode *rnode, *cnode;

	wapbl_sysctl = NULL;

	rv = sysctl_createv(&wapbl_sysctl, 0, NULL, &rnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "wapbl",
		       SYSCTL_DESCR("WAPBL journaling options"),
		       NULL, 0, NULL, 0,
		       CTL_VFS, CTL_CREATE, CTL_EOL);
	if (rv)
		return rv;

	rv = sysctl_createv(&wapbl_sysctl, 0, &rnode, &cnode,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "flush_disk_cache",
		       SYSCTL_DESCR("flush disk cache"),
		       NULL, 0, &wapbl_flush_disk_cache, 0,
		       CTL_CREATE, CTL_EOL);
	if (rv)
		return rv;

	rv = sysctl_createv(&wapbl_sysctl, 0, &rnode, &cnode,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "verbose_commit",
		       SYSCTL_DESCR("show time and size of wapbl log commits"),
		       NULL, 0, &wapbl_verbose_commit, 0,
		       CTL_CREATE, CTL_EOL);
	return rv;
}

static void
wapbl_init(void)
{

	pool_init(&wapbl_entry_pool, sizeof(struct wapbl_entry), 0, 0, 0,
	    "wapblentrypl", &pool_allocator_kmem, IPL_VM);

	wapbl_sysctl_init();
}

#ifdef notyet
static int
wapbl_fini(bool interface)
{

	if (aio_sysctl != NULL)
		 sysctl_teardown(&aio_sysctl);

	pool_destroy(&wapbl_entry_pool);

	return 0;
}
#endif

static int
wapbl_start_flush_inodes(struct wapbl *wl, struct wapbl_replay *wr)
{
	int error, i;

	WAPBL_PRINTF(WAPBL_PRINT_REPLAY,
	    ("wapbl_start: reusing log with %d inodes\n", wr->wr_inodescnt));

	/*
	 * Its only valid to reuse the replay log if its
	 * the same as the new log we just opened.
	 */
	KDASSERT(!wapbl_replay_isopen(wr));
	KASSERT(wl->wl_devvp->v_type == VBLK);
	KASSERT(wr->wr_devvp->v_type == VBLK);
	KASSERT(wl->wl_devvp->v_rdev == wr->wr_devvp->v_rdev);
	KASSERT(wl->wl_logpbn == wr->wr_logpbn);
	KASSERT(wl->wl_circ_size == wr->wr_circ_size);
	KASSERT(wl->wl_circ_off == wr->wr_circ_off);
	KASSERT(wl->wl_log_dev_bshift == wr->wr_log_dev_bshift);
	KASSERT(wl->wl_fs_dev_bshift == wr->wr_fs_dev_bshift);

	wl->wl_wc_header->wc_generation = wr->wr_generation + 1;

	for (i = 0; i < wr->wr_inodescnt; i++)
		wapbl_register_inode(wl, wr->wr_inodes[i].wr_inumber,
		    wr->wr_inodes[i].wr_imode);

	/* Make sure new transaction won't overwrite old inodes list */
	KDASSERT(wapbl_transaction_len(wl) <= 
	    wapbl_space_free(wl->wl_circ_size, wr->wr_inodeshead,
	    wr->wr_inodestail));

	wl->wl_head = wl->wl_tail = wr->wr_inodeshead;
	wl->wl_reclaimable_bytes = wl->wl_reserved_bytes =
	    wapbl_transaction_len(wl);

	error = wapbl_write_inodes(wl, &wl->wl_head);
	if (error)
		return error;

	KASSERT(wl->wl_head != wl->wl_tail);
	KASSERT(wl->wl_head != 0);

	return 0;
}

int
wapbl_start(struct wapbl ** wlp, struct mount *mp, struct vnode *vp,
	daddr_t off, size_t count, size_t blksize, struct wapbl_replay *wr,
	wapbl_flush_fn_t flushfn, wapbl_flush_fn_t flushabortfn)
{
	struct wapbl *wl;
	struct vnode *devvp;
	daddr_t logpbn;
	int error;
	int log_dev_bshift = ilog2(blksize);
	int fs_dev_bshift = log_dev_bshift;
	int run;

	WAPBL_PRINTF(WAPBL_PRINT_OPEN, ("wapbl_start: vp=%p off=%" PRId64
	    " count=%zu blksize=%zu\n", vp, off, count, blksize));

	if (log_dev_bshift > fs_dev_bshift) {
		WAPBL_PRINTF(WAPBL_PRINT_OPEN,
			("wapbl: log device's block size cannot be larger "
			 "than filesystem's\n"));
		/*
		 * Not currently implemented, although it could be if
		 * needed someday.
		 */
		return ENOSYS;
	}

	if (off < 0)
		return EINVAL;

	if (blksize < DEV_BSIZE)
		return EINVAL;
	if (blksize % DEV_BSIZE)
		return EINVAL;

	/* XXXTODO: verify that the full load is writable */

	/*
	 * XXX check for minimum log size
	 * minimum is governed by minimum amount of space
	 * to complete a transaction. (probably truncate)
	 */
	/* XXX for now pick something minimal */
	if ((count * blksize) < MAXPHYS) {
		return ENOSPC;
	}

	if ((error = VOP_BMAP(vp, off, &devvp, &logpbn, &run)) != 0) {
		return error;
	}

	wl = wapbl_calloc(1, sizeof(*wl));
	rw_init(&wl->wl_rwlock);
	mutex_init(&wl->wl_mtx, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&wl->wl_reclaimable_cv, "wapblrec");
	LIST_INIT(&wl->wl_bufs);
	SIMPLEQ_INIT(&wl->wl_entries);

	wl->wl_logvp = vp;
	wl->wl_devvp = devvp;
	wl->wl_mount = mp;
	wl->wl_logpbn = logpbn;
	wl->wl_log_dev_bshift = log_dev_bshift;
	wl->wl_fs_dev_bshift = fs_dev_bshift;

	wl->wl_flush = flushfn;
	wl->wl_flush_abort = flushabortfn;

	/* Reserve two log device blocks for the commit headers */
	wl->wl_circ_off = 2<<wl->wl_log_dev_bshift;
	wl->wl_circ_size = ((count * blksize) - wl->wl_circ_off);
	/* truncate the log usage to a multiple of log_dev_bshift */
	wl->wl_circ_size >>= wl->wl_log_dev_bshift;
	wl->wl_circ_size <<= wl->wl_log_dev_bshift;

	/*
	 * wl_bufbytes_max limits the size of the in memory transaction space.
	 * - Since buffers are allocated and accounted for in units of
	 *   PAGE_SIZE it is required to be a multiple of PAGE_SIZE
	 *   (i.e. 1<<PAGE_SHIFT)
	 * - Since the log device has to be written in units of
	 *   1<<wl_log_dev_bshift it is required to be a mulitple of
	 *   1<<wl_log_dev_bshift.
	 * - Since filesystem will provide data in units of 1<<wl_fs_dev_bshift,
	 *   it is convenient to be a multiple of 1<<wl_fs_dev_bshift.
	 * Therefore it must be multiple of the least common multiple of those
	 * three quantities.  Fortunately, all of those quantities are
	 * guaranteed to be a power of two, and the least common multiple of
	 * a set of numbers which are all powers of two is simply the maximum
	 * of those numbers.  Finally, the maximum logarithm of a power of two
	 * is the same as the log of the maximum power of two.  So we can do
	 * the following operations to size wl_bufbytes_max:
	 */

	/* XXX fix actual number of pages reserved per filesystem. */
	wl->wl_bufbytes_max = MIN(wl->wl_circ_size, buf_memcalc() / 2);

	/* Round wl_bufbytes_max to the largest power of two constraint */
	wl->wl_bufbytes_max >>= PAGE_SHIFT;
	wl->wl_bufbytes_max <<= PAGE_SHIFT;
	wl->wl_bufbytes_max >>= wl->wl_log_dev_bshift;
	wl->wl_bufbytes_max <<= wl->wl_log_dev_bshift;
	wl->wl_bufbytes_max >>= wl->wl_fs_dev_bshift;
	wl->wl_bufbytes_max <<= wl->wl_fs_dev_bshift;

	/* XXX maybe use filesystem fragment size instead of 1024 */
	/* XXX fix actual number of buffers reserved per filesystem. */
	wl->wl_bufcount_max = (nbuf / 2) * 1024;

	/* XXX tie this into resource estimation */
	wl->wl_dealloclim = wl->wl_bufbytes_max / mp->mnt_stat.f_bsize / 2;
	
	wl->wl_deallocblks = wapbl_alloc(sizeof(*wl->wl_deallocblks) *
	    wl->wl_dealloclim);
	wl->wl_dealloclens = wapbl_alloc(sizeof(*wl->wl_dealloclens) *
	    wl->wl_dealloclim);

	wl->wl_buffer = wapbl_alloc(MAXPHYS);
	wl->wl_buffer_used = 0;

	wapbl_inodetrk_init(wl, WAPBL_INODETRK_SIZE);

	/* Initialize the commit header */
	{
		struct wapbl_wc_header *wc;
		size_t len = 1 << wl->wl_log_dev_bshift;
		wc = wapbl_calloc(1, len);
		wc->wc_type = WAPBL_WC_HEADER;
		wc->wc_len = len;
		wc->wc_circ_off = wl->wl_circ_off;
		wc->wc_circ_size = wl->wl_circ_size;
		/* XXX wc->wc_fsid */
		wc->wc_log_dev_bshift = wl->wl_log_dev_bshift;
		wc->wc_fs_dev_bshift = wl->wl_fs_dev_bshift;
		wl->wl_wc_header = wc;
		wl->wl_wc_scratch = wapbl_alloc(len);
	}

	/*
	 * if there was an existing set of unlinked but
	 * allocated inodes, preserve it in the new
	 * log.
	 */
	if (wr && wr->wr_inodescnt) {
		error = wapbl_start_flush_inodes(wl, wr);
		if (error)
			goto errout;
	}

	error = wapbl_write_commit(wl, wl->wl_head, wl->wl_tail);
	if (error) {
		goto errout;
	}

	*wlp = wl;
#if defined(WAPBL_DEBUG)
	wapbl_debug_wl = wl;
#endif

	return 0;
 errout:
	wapbl_discard(wl);
	wapbl_free(wl->wl_wc_scratch, wl->wl_wc_header->wc_len);
	wapbl_free(wl->wl_wc_header, wl->wl_wc_header->wc_len);
	wapbl_free(wl->wl_deallocblks,
	    sizeof(*wl->wl_deallocblks) * wl->wl_dealloclim);
	wapbl_free(wl->wl_dealloclens,
	    sizeof(*wl->wl_dealloclens) * wl->wl_dealloclim);
	wapbl_free(wl->wl_buffer, MAXPHYS);
	wapbl_inodetrk_free(wl);
	wapbl_free(wl, sizeof(*wl));

	return error;
}

/*
 * Like wapbl_flush, only discards the transaction
 * completely
 */

void
wapbl_discard(struct wapbl *wl)
{
	struct wapbl_entry *we;
	struct buf *bp;
	int i;

	/*
	 * XXX we may consider using upgrade here
	 * if we want to call flush from inside a transaction
	 */
	rw_enter(&wl->wl_rwlock, RW_WRITER);
	wl->wl_flush(wl->wl_mount, wl->wl_deallocblks, wl->wl_dealloclens,
	    wl->wl_dealloccnt);

#ifdef WAPBL_DEBUG_PRINT
	{
		pid_t pid = -1;
		lwpid_t lid = -1;
		if (curproc)
			pid = curproc->p_pid;
		if (curlwp)
			lid = curlwp->l_lid;
#ifdef WAPBL_DEBUG_BUFBYTES
		WAPBL_PRINTF(WAPBL_PRINT_DISCARD,
		    ("wapbl_discard: thread %d.%d discarding "
		    "transaction\n"
		    "\tbufcount=%zu bufbytes=%zu bcount=%zu "
		    "deallocs=%d inodes=%d\n"
		    "\terrcnt = %u, reclaimable=%zu reserved=%zu "
		    "unsynced=%zu\n",
		    pid, lid, wl->wl_bufcount, wl->wl_bufbytes,
		    wl->wl_bcount, wl->wl_dealloccnt,
		    wl->wl_inohashcnt, wl->wl_error_count,
		    wl->wl_reclaimable_bytes, wl->wl_reserved_bytes,
		    wl->wl_unsynced_bufbytes));
		SIMPLEQ_FOREACH(we, &wl->wl_entries, we_entries) {
			WAPBL_PRINTF(WAPBL_PRINT_DISCARD,
			    ("\tentry: bufcount = %zu, reclaimable = %zu, "
			     "error = %d, unsynced = %zu\n",
			     we->we_bufcount, we->we_reclaimable_bytes,
			     we->we_error, we->we_unsynced_bufbytes));
		}
#else /* !WAPBL_DEBUG_BUFBYTES */
		WAPBL_PRINTF(WAPBL_PRINT_DISCARD,
		    ("wapbl_discard: thread %d.%d discarding transaction\n"
		    "\tbufcount=%zu bufbytes=%zu bcount=%zu "
		    "deallocs=%d inodes=%d\n"
		    "\terrcnt = %u, reclaimable=%zu reserved=%zu\n",
		    pid, lid, wl->wl_bufcount, wl->wl_bufbytes,
		    wl->wl_bcount, wl->wl_dealloccnt,
		    wl->wl_inohashcnt, wl->wl_error_count,
		    wl->wl_reclaimable_bytes, wl->wl_reserved_bytes));
		SIMPLEQ_FOREACH(we, &wl->wl_entries, we_entries) {
			WAPBL_PRINTF(WAPBL_PRINT_DISCARD,
			    ("\tentry: bufcount = %zu, reclaimable = %zu, "
			     "error = %d\n",
			     we->we_bufcount, we->we_reclaimable_bytes,
			     we->we_error));
		}
#endif /* !WAPBL_DEBUG_BUFBYTES */
	}
#endif /* WAPBL_DEBUG_PRINT */

	for (i = 0; i <= wl->wl_inohashmask; i++) {
		struct wapbl_ino_head *wih;
		struct wapbl_ino *wi;

		wih = &wl->wl_inohash[i];
		while ((wi = LIST_FIRST(wih)) != NULL) {
			LIST_REMOVE(wi, wi_hash);
			pool_put(&wapbl_ino_pool, wi);
			KASSERT(wl->wl_inohashcnt > 0);
			wl->wl_inohashcnt--;
		}
	}

	/*
	 * clean buffer list
	 */
	mutex_enter(&bufcache_lock);
	mutex_enter(&wl->wl_mtx);
	while ((bp = LIST_FIRST(&wl->wl_bufs)) != NULL) {
		if (bbusy(bp, 0, 0, &wl->wl_mtx) == 0) {
			/*
			 * The buffer will be unlocked and
			 * removed from the transaction in brelse
			 */
			mutex_exit(&wl->wl_mtx);
			brelsel(bp, 0);
			mutex_enter(&wl->wl_mtx);
		}
	}
	mutex_exit(&wl->wl_mtx);
	mutex_exit(&bufcache_lock);

	/*
	 * Remove references to this wl from wl_entries, free any which
	 * no longer have buffers, others will be freed in wapbl_biodone
	 * when they no longer have any buffers.
	 */
	while ((we = SIMPLEQ_FIRST(&wl->wl_entries)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&wl->wl_entries, we_entries);
		/* XXX should we be accumulating wl_error_count
		 * and increasing reclaimable bytes ? */
		we->we_wapbl = NULL;
		if (we->we_bufcount == 0) {
#ifdef WAPBL_DEBUG_BUFBYTES
			KASSERT(we->we_unsynced_bufbytes == 0);
#endif
			pool_put(&wapbl_entry_pool, we);
		}
	}

	/* Discard list of deallocs */
	wl->wl_dealloccnt = 0;
	/* XXX should we clear wl_reserved_bytes? */

	KASSERT(wl->wl_bufbytes == 0);
	KASSERT(wl->wl_bcount == 0);
	KASSERT(wl->wl_bufcount == 0);
	KASSERT(LIST_EMPTY(&wl->wl_bufs));
	KASSERT(SIMPLEQ_EMPTY(&wl->wl_entries));
	KASSERT(wl->wl_inohashcnt == 0);

	rw_exit(&wl->wl_rwlock);
}

int
wapbl_stop(struct wapbl *wl, int force)
{
	int error;

	WAPBL_PRINTF(WAPBL_PRINT_OPEN, ("wapbl_stop called\n"));
	error = wapbl_flush(wl, 1);
	if (error) {
		if (force)
			wapbl_discard(wl);
		else
			return error;
	}

	/* Unlinked inodes persist after a flush */
	if (wl->wl_inohashcnt) {
		if (force) {
			wapbl_discard(wl);
		} else {
			return EBUSY;
		}
	}

	KASSERT(wl->wl_bufbytes == 0);
	KASSERT(wl->wl_bcount == 0);
	KASSERT(wl->wl_bufcount == 0);
	KASSERT(LIST_EMPTY(&wl->wl_bufs));
	KASSERT(wl->wl_dealloccnt == 0);
	KASSERT(SIMPLEQ_EMPTY(&wl->wl_entries));
	KASSERT(wl->wl_inohashcnt == 0);

	wapbl_free(wl->wl_wc_scratch, wl->wl_wc_header->wc_len);
	wapbl_free(wl->wl_wc_header, wl->wl_wc_header->wc_len);
	wapbl_free(wl->wl_deallocblks,
	    sizeof(*wl->wl_deallocblks) * wl->wl_dealloclim);
	wapbl_free(wl->wl_dealloclens,
	    sizeof(*wl->wl_dealloclens) * wl->wl_dealloclim);
	wapbl_free(wl->wl_buffer, MAXPHYS);
	wapbl_inodetrk_free(wl);

	cv_destroy(&wl->wl_reclaimable_cv);
	mutex_destroy(&wl->wl_mtx);
	rw_destroy(&wl->wl_rwlock);
	wapbl_free(wl, sizeof(*wl));

	return 0;
}

static int
wapbl_doio(void *data, size_t len, struct vnode *devvp, daddr_t pbn, int flags)
{
	struct pstats *pstats = curlwp->l_proc->p_stats;
	struct buf *bp;
	int error;

	KASSERT((flags & ~(B_WRITE | B_READ)) == 0);
	KASSERT(devvp->v_type == VBLK);

	if ((flags & (B_WRITE | B_READ)) == B_WRITE) {
		mutex_enter(devvp->v_interlock);
		devvp->v_numoutput++;
		mutex_exit(devvp->v_interlock);
		pstats->p_ru.ru_oublock++;
	} else {
		pstats->p_ru.ru_inblock++;
	}

	bp = getiobuf(devvp, true);
	bp->b_flags = flags;
	bp->b_cflags = BC_BUSY; /* silly & dubious */
	bp->b_dev = devvp->v_rdev;
	bp->b_data = data;
	bp->b_bufsize = bp->b_resid = bp->b_bcount = len;
	bp->b_blkno = pbn;
	BIO_SETPRIO(bp, BPRIO_TIMECRITICAL);

	WAPBL_PRINTF(WAPBL_PRINT_IO,
	    ("wapbl_doio: %s %d bytes at block %"PRId64" on dev 0x%"PRIx64"\n",
	    BUF_ISWRITE(bp) ? "write" : "read", bp->b_bcount,
	    bp->b_blkno, bp->b_dev));

	VOP_STRATEGY(devvp, bp);

	error = biowait(bp);
	putiobuf(bp);

	if (error) {
		WAPBL_PRINTF(WAPBL_PRINT_ERROR,
		    ("wapbl_doio: %s %zu bytes at block %" PRId64
		    " on dev 0x%"PRIx64" failed with error %d\n",
		    (((flags & (B_WRITE | B_READ)) == B_WRITE) ?
		     "write" : "read"),
		    len, pbn, devvp->v_rdev, error));
	}

	return error;
}

int
wapbl_write(void *data, size_t len, struct vnode *devvp, daddr_t pbn)
{

	return wapbl_doio(data, len, devvp, pbn, B_WRITE);
}

int
wapbl_read(void *data, size_t len, struct vnode *devvp, daddr_t pbn)
{

	return wapbl_doio(data, len, devvp, pbn, B_READ);
}

/*
 * Flush buffered data if any.
 */
static int
wapbl_buffered_flush(struct wapbl *wl)
{
	int error;

	if (wl->wl_buffer_used == 0)
		return 0;

	error = wapbl_doio(wl->wl_buffer, wl->wl_buffer_used,
	    wl->wl_devvp, wl->wl_buffer_dblk, B_WRITE);
	wl->wl_buffer_used = 0;

	return error;
}

/*
 * Write data to the log.
 * Try to coalesce writes and emit MAXPHYS aligned blocks.
 */
static int
wapbl_buffered_write(void *data, size_t len, struct wapbl *wl, daddr_t pbn)
{
	int error;
	size_t resid;

	/*
	 * If not adjacent to buffered data flush first.  Disk block
	 * address is always valid for non-empty buffer.
	 */
	if (wl->wl_buffer_used > 0 &&
	    pbn != wl->wl_buffer_dblk + btodb(wl->wl_buffer_used)) {
		error = wapbl_buffered_flush(wl);
		if (error)
			return error;
	}
	/*
	 * If this write goes to an empty buffer we have to
	 * save the disk block address first.
	 */
	if (wl->wl_buffer_used == 0)
		wl->wl_buffer_dblk = pbn;
	/*
	 * Remaining space so this buffer ends on a MAXPHYS boundary.
	 *
	 * Cannot become less or equal zero as the buffer would have been
	 * flushed on the last call then.
	 */
	resid = MAXPHYS - dbtob(wl->wl_buffer_dblk % btodb(MAXPHYS)) -
	    wl->wl_buffer_used;
	KASSERT(resid > 0);
	KASSERT(dbtob(btodb(resid)) == resid);
	if (len >= resid) {
		memcpy(wl->wl_buffer + wl->wl_buffer_used, data, resid);
		wl->wl_buffer_used += resid;
		error = wapbl_doio(wl->wl_buffer, wl->wl_buffer_used,
		    wl->wl_devvp, wl->wl_buffer_dblk, B_WRITE);
		data = (uint8_t *)data + resid;
		len -= resid;
		wl->wl_buffer_dblk = pbn + btodb(resid);
		wl->wl_buffer_used = 0;
		if (error)
			return error;
	}
	KASSERT(len < MAXPHYS);
	if (len > 0) {
		memcpy(wl->wl_buffer + wl->wl_buffer_used, data, len);
		wl->wl_buffer_used += len;
	}

	return 0;
}

/*
 * Off is byte offset returns new offset for next write
 * handles log wraparound
 */
static int
wapbl_circ_write(struct wapbl *wl, void *data, size_t len, off_t *offp)
{
	size_t slen;
	off_t off = *offp;
	int error;
	daddr_t pbn;

	KDASSERT(((len >> wl->wl_log_dev_bshift) <<
	    wl->wl_log_dev_bshift) == len);

	if (off < wl->wl_circ_off)
		off = wl->wl_circ_off;
	slen = wl->wl_circ_off + wl->wl_circ_size - off;
	if (slen < len) {
		pbn = wl->wl_logpbn + (off >> wl->wl_log_dev_bshift);
#ifdef _KERNEL
		pbn = btodb(pbn << wl->wl_log_dev_bshift);
#endif
		error = wapbl_buffered_write(data, slen, wl, pbn);
		if (error)
			return error;
		data = (uint8_t *)data + slen;
		len -= slen;
		off = wl->wl_circ_off;
	}
	pbn = wl->wl_logpbn + (off >> wl->wl_log_dev_bshift);
#ifdef _KERNEL
	pbn = btodb(pbn << wl->wl_log_dev_bshift);
#endif
	error = wapbl_buffered_write(data, len, wl, pbn);
	if (error)
		return error;
	off += len;
	if (off >= wl->wl_circ_off + wl->wl_circ_size)
		off = wl->wl_circ_off;
	*offp = off;
	return 0;
}

/****************************************************************/

int
wapbl_begin(struct wapbl *wl, const char *file, int line)
{
	int doflush;
	unsigned lockcount;

	KDASSERT(wl);

	/*
	 * XXX this needs to be made much more sophisticated.
	 * perhaps each wapbl_begin could reserve a specified
	 * number of buffers and bytes.
	 */
	mutex_enter(&wl->wl_mtx);
	lockcount = wl->wl_lock_count;
	doflush = ((wl->wl_bufbytes + (lockcount * MAXPHYS)) >
		   wl->wl_bufbytes_max / 2) ||
		  ((wl->wl_bufcount + (lockcount * 10)) >
		   wl->wl_bufcount_max / 2) ||
		  (wapbl_transaction_len(wl) > wl->wl_circ_size / 2) ||
		  (wl->wl_dealloccnt >= (wl->wl_dealloclim / 2));
	mutex_exit(&wl->wl_mtx);

	if (doflush) {
		WAPBL_PRINTF(WAPBL_PRINT_FLUSH,
		    ("force flush lockcnt=%d bufbytes=%zu "
		    "(max=%zu) bufcount=%zu (max=%zu) "
		    "dealloccnt %d (lim=%d)\n",
		    lockcount, wl->wl_bufbytes,
		    wl->wl_bufbytes_max, wl->wl_bufcount,
		    wl->wl_bufcount_max,
		    wl->wl_dealloccnt, wl->wl_dealloclim));
	}

	if (doflush) {
		int error = wapbl_flush(wl, 0);
		if (error)
			return error;
	}

	rw_enter(&wl->wl_rwlock, RW_READER);
	mutex_enter(&wl->wl_mtx);
	wl->wl_lock_count++;
	mutex_exit(&wl->wl_mtx);

#if defined(WAPBL_DEBUG_PRINT)
	WAPBL_PRINTF(WAPBL_PRINT_TRANSACTION,
	    ("wapbl_begin thread %d.%d with bufcount=%zu "
	    "bufbytes=%zu bcount=%zu at %s:%d\n",
	    curproc->p_pid, curlwp->l_lid, wl->wl_bufcount,
	    wl->wl_bufbytes, wl->wl_bcount, file, line));
#endif

	return 0;
}

void
wapbl_end(struct wapbl *wl)
{

#if defined(WAPBL_DEBUG_PRINT)
	WAPBL_PRINTF(WAPBL_PRINT_TRANSACTION,
	     ("wapbl_end thread %d.%d with bufcount=%zu "
	      "bufbytes=%zu bcount=%zu\n",
	      curproc->p_pid, curlwp->l_lid, wl->wl_bufcount,
	      wl->wl_bufbytes, wl->wl_bcount));
#endif

#ifdef DIAGNOSTIC
	size_t flushsize = wapbl_transaction_len(wl);
	if (flushsize > (wl->wl_circ_size - wl->wl_reserved_bytes)) {
		/*
		 * XXX this could be handled more gracefully, perhaps place
		 * only a partial transaction in the log and allow the
		 * remaining to flush without the protection of the journal.
		 */
		panic("wapbl_end: current transaction too big to flush\n");
	}
#endif

	mutex_enter(&wl->wl_mtx);
	KASSERT(wl->wl_lock_count > 0);
	wl->wl_lock_count--;
	mutex_exit(&wl->wl_mtx);

	rw_exit(&wl->wl_rwlock);
}

void
wapbl_add_buf(struct wapbl *wl, struct buf * bp)
{

	KASSERT(bp->b_cflags & BC_BUSY);
	KASSERT(bp->b_vp);

	wapbl_jlock_assert(wl);

#if 0
	/*
	 * XXX this might be an issue for swapfiles.
	 * see uvm_swap.c:1702
	 *
	 * XXX2 why require it then?  leap of semantics?
	 */
	KASSERT((bp->b_cflags & BC_NOCACHE) == 0);
#endif

	mutex_enter(&wl->wl_mtx);
	if (bp->b_flags & B_LOCKED) {
		LIST_REMOVE(bp, b_wapbllist);
		WAPBL_PRINTF(WAPBL_PRINT_BUFFER2,
		   ("wapbl_add_buf thread %d.%d re-adding buf %p "
		    "with %d bytes %d bcount\n",
		    curproc->p_pid, curlwp->l_lid, bp, bp->b_bufsize,
		    bp->b_bcount));
	} else {
		/* unlocked by dirty buffers shouldn't exist */
		KASSERT(!(bp->b_oflags & BO_DELWRI));
		wl->wl_bufbytes += bp->b_bufsize;
		wl->wl_bcount += bp->b_bcount;
		wl->wl_bufcount++;
		WAPBL_PRINTF(WAPBL_PRINT_BUFFER,
		   ("wapbl_add_buf thread %d.%d adding buf %p "
		    "with %d bytes %d bcount\n",
		    curproc->p_pid, curlwp->l_lid, bp, bp->b_bufsize,
		    bp->b_bcount));
	}
	LIST_INSERT_HEAD(&wl->wl_bufs, bp, b_wapbllist);
	mutex_exit(&wl->wl_mtx);

	bp->b_flags |= B_LOCKED;
}

static void
wapbl_remove_buf_locked(struct wapbl * wl, struct buf *bp)
{

	KASSERT(mutex_owned(&wl->wl_mtx));
	KASSERT(bp->b_cflags & BC_BUSY);
	wapbl_jlock_assert(wl);

#if 0
	/*
	 * XXX this might be an issue for swapfiles.
	 * see uvm_swap.c:1725
	 *
	 * XXXdeux: see above
	 */
	KASSERT((bp->b_flags & BC_NOCACHE) == 0);
#endif
	KASSERT(bp->b_flags & B_LOCKED);

	WAPBL_PRINTF(WAPBL_PRINT_BUFFER,
	   ("wapbl_remove_buf thread %d.%d removing buf %p with "
	    "%d bytes %d bcount\n",
	    curproc->p_pid, curlwp->l_lid, bp, bp->b_bufsize, bp->b_bcount));

	KASSERT(wl->wl_bufbytes >= bp->b_bufsize);
	wl->wl_bufbytes -= bp->b_bufsize;
	KASSERT(wl->wl_bcount >= bp->b_bcount);
	wl->wl_bcount -= bp->b_bcount;
	KASSERT(wl->wl_bufcount > 0);
	wl->wl_bufcount--;
	KASSERT((wl->wl_bufcount == 0) == (wl->wl_bufbytes == 0));
	KASSERT((wl->wl_bufcount == 0) == (wl->wl_bcount == 0));
	LIST_REMOVE(bp, b_wapbllist);

	bp->b_flags &= ~B_LOCKED;
}

/* called from brelsel() in vfs_bio among other places */
void
wapbl_remove_buf(struct wapbl * wl, struct buf *bp)
{

	mutex_enter(&wl->wl_mtx);
	wapbl_remove_buf_locked(wl, bp);
	mutex_exit(&wl->wl_mtx);
}

void
wapbl_resize_buf(struct wapbl *wl, struct buf *bp, long oldsz, long oldcnt)
{

	KASSERT(bp->b_cflags & BC_BUSY);

	/*
	 * XXX: why does this depend on B_LOCKED?  otherwise the buf
	 * is not for a transaction?  if so, why is this called in the
	 * first place?
	 */
	if (bp->b_flags & B_LOCKED) {
		mutex_enter(&wl->wl_mtx);
		wl->wl_bufbytes += bp->b_bufsize - oldsz;
		wl->wl_bcount += bp->b_bcount - oldcnt;
		mutex_exit(&wl->wl_mtx);
	}
}

#endif /* _KERNEL */

/****************************************************************/
/* Some utility inlines */

static inline size_t
wapbl_space_used(size_t avail, off_t head, off_t tail)
{

	if (tail == 0) {
		KASSERT(head == 0);
		return 0;
	}
	return ((head + (avail - 1) - tail) % avail) + 1;
}

#ifdef _KERNEL
/* This is used to advance the pointer at old to new value at old+delta */
static inline off_t
wapbl_advance(size_t size, size_t off, off_t oldoff, size_t delta)
{
	off_t newoff;

	/* Define acceptable ranges for inputs. */
	KASSERT(delta <= (size_t)size);
	KASSERT((oldoff == 0) || ((size_t)oldoff >= off));
	KASSERT(oldoff < (off_t)(size + off));

	if ((oldoff == 0) && (delta != 0))
		newoff = off + delta;
	else if ((oldoff + delta) < (size + off))
		newoff = oldoff + delta;
	else
		newoff = (oldoff + delta) - size;

	/* Note some interesting axioms */
	KASSERT((delta != 0) || (newoff == oldoff));
	KASSERT((delta == 0) || (newoff != 0));
	KASSERT((delta != (size)) || (newoff == oldoff));

	/* Define acceptable ranges for output. */
	KASSERT((newoff == 0) || ((size_t)newoff >= off));
	KASSERT((size_t)newoff < (size + off));
	return newoff;
}

static inline size_t
wapbl_space_free(size_t avail, off_t head, off_t tail)
{

	return avail - wapbl_space_used(avail, head, tail);
}

static inline void
wapbl_advance_head(size_t size, size_t off, size_t delta, off_t *headp,
		   off_t *tailp)
{
	off_t head = *headp;
	off_t tail = *tailp;

	KASSERT(delta <= wapbl_space_free(size, head, tail));
	head = wapbl_advance(size, off, head, delta);
	if ((tail == 0) && (head != 0))
		tail = off;
	*headp = head;
	*tailp = tail;
}

static inline void
wapbl_advance_tail(size_t size, size_t off, size_t delta, off_t *headp,
		   off_t *tailp)
{
	off_t head = *headp;
	off_t tail = *tailp;

	KASSERT(delta <= wapbl_space_used(size, head, tail));
	tail = wapbl_advance(size, off, tail, delta);
	if (head == tail) {
		head = tail = 0;
	}
	*headp = head;
	*tailp = tail;
}


/****************************************************************/

/*
 * Remove transactions whose buffers are completely flushed to disk.
 * Will block until at least minfree space is available.
 * only intended to be called from inside wapbl_flush and therefore
 * does not protect against commit races with itself or with flush.
 */
static int
wapbl_truncate(struct wapbl *wl, size_t minfree, int waitonly)
{
	size_t delta;
	size_t avail;
	off_t head;
	off_t tail;
	int error = 0;

	KASSERT(minfree <= (wl->wl_circ_size - wl->wl_reserved_bytes));
	KASSERT(rw_write_held(&wl->wl_rwlock));

	mutex_enter(&wl->wl_mtx);

	/*
	 * First check to see if we have to do a commit
	 * at all.
	 */
	avail = wapbl_space_free(wl->wl_circ_size, wl->wl_head, wl->wl_tail);
	if (minfree < avail) {
		mutex_exit(&wl->wl_mtx);
		return 0;
	}
	minfree -= avail;
	while ((wl->wl_error_count == 0) &&
	    (wl->wl_reclaimable_bytes < minfree)) {
        	WAPBL_PRINTF(WAPBL_PRINT_TRUNCATE,
                   ("wapbl_truncate: sleeping on %p wl=%p bytes=%zd "
		    "minfree=%zd\n",
                    &wl->wl_reclaimable_bytes, wl, wl->wl_reclaimable_bytes,
		    minfree));

		cv_wait(&wl->wl_reclaimable_cv, &wl->wl_mtx);
	}
	if (wl->wl_reclaimable_bytes < minfree) {
		KASSERT(wl->wl_error_count);
		/* XXX maybe get actual error from buffer instead someday? */
		error = EIO;
	}
	head = wl->wl_head;
	tail = wl->wl_tail;
	delta = wl->wl_reclaimable_bytes;

	/* If all of of the entries are flushed, then be sure to keep
	 * the reserved bytes reserved.  Watch out for discarded transactions,
	 * which could leave more bytes reserved than are reclaimable.
	 */
	if (SIMPLEQ_EMPTY(&wl->wl_entries) && 
	    (delta >= wl->wl_reserved_bytes)) {
		delta -= wl->wl_reserved_bytes;
	}
	wapbl_advance_tail(wl->wl_circ_size, wl->wl_circ_off, delta, &head,
			   &tail);
	KDASSERT(wl->wl_reserved_bytes <=
		wapbl_space_used(wl->wl_circ_size, head, tail));
	mutex_exit(&wl->wl_mtx);

	if (error)
		return error;

	if (waitonly)
		return 0;

	/*
	 * This is where head, tail and delta are unprotected
	 * from races against itself or flush.  This is ok since
	 * we only call this routine from inside flush itself.
	 *
	 * XXX: how can it race against itself when accessed only
	 * from behind the write-locked rwlock?
	 */
	error = wapbl_write_commit(wl, head, tail);
	if (error)
		return error;

	wl->wl_head = head;
	wl->wl_tail = tail;

	mutex_enter(&wl->wl_mtx);
	KASSERT(wl->wl_reclaimable_bytes >= delta);
	wl->wl_reclaimable_bytes -= delta;
	mutex_exit(&wl->wl_mtx);
	WAPBL_PRINTF(WAPBL_PRINT_TRUNCATE,
	    ("wapbl_truncate thread %d.%d truncating %zu bytes\n",
	    curproc->p_pid, curlwp->l_lid, delta));

	return 0;
}

/****************************************************************/

void
wapbl_biodone(struct buf *bp)
{
	struct wapbl_entry *we = bp->b_private;
	struct wapbl *wl = we->we_wapbl;
#ifdef WAPBL_DEBUG_BUFBYTES
	const int bufsize = bp->b_bufsize;
#endif

	/*
	 * Handle possible flushing of buffers after log has been
	 * decomissioned.
	 */
	if (!wl) {
		KASSERT(we->we_bufcount > 0);
		we->we_bufcount--;
#ifdef WAPBL_DEBUG_BUFBYTES
		KASSERT(we->we_unsynced_bufbytes >= bufsize);
		we->we_unsynced_bufbytes -= bufsize;
#endif

		if (we->we_bufcount == 0) {
#ifdef WAPBL_DEBUG_BUFBYTES
			KASSERT(we->we_unsynced_bufbytes == 0);
#endif
			pool_put(&wapbl_entry_pool, we);
		}

		brelse(bp, 0);
		return;
	}

#ifdef ohbother
	KDASSERT(bp->b_oflags & BO_DONE);
	KDASSERT(!(bp->b_oflags & BO_DELWRI));
	KDASSERT(bp->b_flags & B_ASYNC);
	KDASSERT(bp->b_cflags & BC_BUSY);
	KDASSERT(!(bp->b_flags & B_LOCKED));
	KDASSERT(!(bp->b_flags & B_READ));
	KDASSERT(!(bp->b_cflags & BC_INVAL));
	KDASSERT(!(bp->b_cflags & BC_NOCACHE));
#endif

	if (bp->b_error) {
#ifdef notyet /* Can't currently handle possible dirty buffer reuse */
		/*
		 * XXXpooka: interfaces not fully updated
		 * Note: this was not enabled in the original patch
		 * against netbsd4 either.  I don't know if comment
		 * above is true or not.
		 */

		/*
		 * If an error occurs, report the error and leave the
		 * buffer as a delayed write on the LRU queue.
		 * restarting the write would likely result in
		 * an error spinloop, so let it be done harmlessly
		 * by the syncer.
		 */
		bp->b_flags &= ~(B_DONE);
		simple_unlock(&bp->b_interlock);

		if (we->we_error == 0) {
			mutex_enter(&wl->wl_mtx);
			wl->wl_error_count++;
			mutex_exit(&wl->wl_mtx);
			cv_broadcast(&wl->wl_reclaimable_cv);
		}
		we->we_error = bp->b_error;
		bp->b_error = 0;
		brelse(bp);
		return;
#else
		/* For now, just mark the log permanently errored out */

		mutex_enter(&wl->wl_mtx);
		if (wl->wl_error_count == 0) {
			wl->wl_error_count++;
			cv_broadcast(&wl->wl_reclaimable_cv);
		}
		mutex_exit(&wl->wl_mtx);
#endif
	}

	/*
	 * Release the buffer here. wapbl_flush() may wait for the
	 * log to become empty and we better unbusy the buffer before
	 * wapbl_flush() returns.
	 */
	brelse(bp, 0);

	mutex_enter(&wl->wl_mtx);

	KASSERT(we->we_bufcount > 0);
	we->we_bufcount--;
#ifdef WAPBL_DEBUG_BUFBYTES
	KASSERT(we->we_unsynced_bufbytes >= bufsize);
	we->we_unsynced_bufbytes -= bufsize;
	KASSERT(wl->wl_unsynced_bufbytes >= bufsize);
	wl->wl_unsynced_bufbytes -= bufsize;
#endif

	/*
	 * If the current transaction can be reclaimed, start
	 * at the beginning and reclaim any consecutive reclaimable
	 * transactions.  If we successfully reclaim anything,
	 * then wakeup anyone waiting for the reclaim.
	 */
	if (we->we_bufcount == 0) {
		size_t delta = 0;
		int errcnt = 0;
#ifdef WAPBL_DEBUG_BUFBYTES
		KDASSERT(we->we_unsynced_bufbytes == 0);
#endif
		/*
		 * clear any posted error, since the buffer it came from
		 * has successfully flushed by now
		 */
		while ((we = SIMPLEQ_FIRST(&wl->wl_entries)) &&
		       (we->we_bufcount == 0)) {
			delta += we->we_reclaimable_bytes;
			if (we->we_error)
				errcnt++;
			SIMPLEQ_REMOVE_HEAD(&wl->wl_entries, we_entries);
			pool_put(&wapbl_entry_pool, we);
		}

		if (delta) {
			wl->wl_reclaimable_bytes += delta;
			KASSERT(wl->wl_error_count >= errcnt);
			wl->wl_error_count -= errcnt;
			cv_broadcast(&wl->wl_reclaimable_cv);
		}
	}

	mutex_exit(&wl->wl_mtx);
}

/*
 * Write transactions to disk + start I/O for contents
 */
int
wapbl_flush(struct wapbl *wl, int waitfor)
{
	struct buf *bp;
	struct wapbl_entry *we;
	off_t off;
	off_t head;
	off_t tail;
	size_t delta = 0;
	size_t flushsize;
	size_t reserved;
	int error = 0;

	/*
	 * Do a quick check to see if a full flush can be skipped
	 * This assumes that the flush callback does not need to be called
	 * unless there are other outstanding bufs.
	 */
	if (!waitfor) {
		size_t nbufs;
		mutex_enter(&wl->wl_mtx);	/* XXX need mutex here to
						   protect the KASSERTS */
		nbufs = wl->wl_bufcount;
		KASSERT((wl->wl_bufcount == 0) == (wl->wl_bufbytes == 0));
		KASSERT((wl->wl_bufcount == 0) == (wl->wl_bcount == 0));
		mutex_exit(&wl->wl_mtx);
		if (nbufs == 0)
			return 0;
	}

	/*
	 * XXX we may consider using LK_UPGRADE here
	 * if we want to call flush from inside a transaction
	 */
	rw_enter(&wl->wl_rwlock, RW_WRITER);
	wl->wl_flush(wl->wl_mount, wl->wl_deallocblks, wl->wl_dealloclens,
	    wl->wl_dealloccnt);

	/*
	 * Now that we are fully locked and flushed,
	 * do another check for nothing to do.
	 */
	if (wl->wl_bufcount == 0) {
		goto out;
	}

#if 0
	WAPBL_PRINTF(WAPBL_PRINT_FLUSH,
		     ("wapbl_flush thread %d.%d flushing entries with "
		      "bufcount=%zu bufbytes=%zu\n",
		      curproc->p_pid, curlwp->l_lid, wl->wl_bufcount,
		      wl->wl_bufbytes));
#endif

	/* Calculate amount of space needed to flush */
	flushsize = wapbl_transaction_len(wl);
	if (wapbl_verbose_commit) {
		struct timespec ts;
		getnanotime(&ts);
		printf("%s: %lld.%09ld this transaction = %zu bytes\n",
		    __func__, (long long)ts.tv_sec,
		    (long)ts.tv_nsec, flushsize);
	}

	if (flushsize > (wl->wl_circ_size - wl->wl_reserved_bytes)) {
		/*
		 * XXX this could be handled more gracefully, perhaps place
		 * only a partial transaction in the log and allow the
		 * remaining to flush without the protection of the journal.
		 */
		panic("wapbl_flush: current transaction too big to flush\n");
	}

	error = wapbl_truncate(wl, flushsize, 0);
	if (error)
		goto out2;

	off = wl->wl_head;
	KASSERT((off == 0) || ((off >= wl->wl_circ_off) && 
	                      (off < wl->wl_circ_off + wl->wl_circ_size)));
	error = wapbl_write_blocks(wl, &off);
	if (error)
		goto out2;
	error = wapbl_write_revocations(wl, &off);
	if (error)
		goto out2;
	error = wapbl_write_inodes(wl, &off);
	if (error)
		goto out2;

	reserved = 0;
	if (wl->wl_inohashcnt)
		reserved = wapbl_transaction_inodes_len(wl);

	head = wl->wl_head;
	tail = wl->wl_tail;

	wapbl_advance_head(wl->wl_circ_size, wl->wl_circ_off, flushsize,
	    &head, &tail);
#ifdef WAPBL_DEBUG
	if (head != off) {
		panic("lost head! head=%"PRIdMAX" tail=%" PRIdMAX
		      " off=%"PRIdMAX" flush=%zu\n",
		      (intmax_t)head, (intmax_t)tail, (intmax_t)off,
		      flushsize);
	}
#else
	KASSERT(head == off);
#endif

	/* Opportunistically move the tail forward if we can */
	if (!wapbl_lazy_truncate) {
		mutex_enter(&wl->wl_mtx);
		delta = wl->wl_reclaimable_bytes;
		mutex_exit(&wl->wl_mtx);
		wapbl_advance_tail(wl->wl_circ_size, wl->wl_circ_off, delta,
		    &head, &tail);
	}

	error = wapbl_write_commit(wl, head, tail);
	if (error)
		goto out2;

	we = pool_get(&wapbl_entry_pool, PR_WAITOK);

#ifdef WAPBL_DEBUG_BUFBYTES
	WAPBL_PRINTF(WAPBL_PRINT_FLUSH,
		("wapbl_flush: thread %d.%d head+=%zu tail+=%zu used=%zu"
		 " unsynced=%zu"
		 "\n\tbufcount=%zu bufbytes=%zu bcount=%zu deallocs=%d "
		 "inodes=%d\n",
		 curproc->p_pid, curlwp->l_lid, flushsize, delta,
		 wapbl_space_used(wl->wl_circ_size, head, tail),
		 wl->wl_unsynced_bufbytes, wl->wl_bufcount,
		 wl->wl_bufbytes, wl->wl_bcount, wl->wl_dealloccnt,
		 wl->wl_inohashcnt));
#else
	WAPBL_PRINTF(WAPBL_PRINT_FLUSH,
		("wapbl_flush: thread %d.%d head+=%zu tail+=%zu used=%zu"
		 "\n\tbufcount=%zu bufbytes=%zu bcount=%zu deallocs=%d "
		 "inodes=%d\n",
		 curproc->p_pid, curlwp->l_lid, flushsize, delta,
		 wapbl_space_used(wl->wl_circ_size, head, tail),
		 wl->wl_bufcount, wl->wl_bufbytes, wl->wl_bcount,
		 wl->wl_dealloccnt, wl->wl_inohashcnt));
#endif


	mutex_enter(&bufcache_lock);
	mutex_enter(&wl->wl_mtx);

	wl->wl_reserved_bytes = reserved;
	wl->wl_head = head;
	wl->wl_tail = tail;
	KASSERT(wl->wl_reclaimable_bytes >= delta);
	wl->wl_reclaimable_bytes -= delta;
	wl->wl_dealloccnt = 0;
#ifdef WAPBL_DEBUG_BUFBYTES
	wl->wl_unsynced_bufbytes += wl->wl_bufbytes;
#endif

	we->we_wapbl = wl;
	we->we_bufcount = wl->wl_bufcount;
#ifdef WAPBL_DEBUG_BUFBYTES
	we->we_unsynced_bufbytes = wl->wl_bufbytes;
#endif
	we->we_reclaimable_bytes = flushsize;
	we->we_error = 0;
	SIMPLEQ_INSERT_TAIL(&wl->wl_entries, we, we_entries);

	/*
	 * this flushes bufs in reverse order than they were queued
	 * it shouldn't matter, but if we care we could use TAILQ instead.
	 * XXX Note they will get put on the lru queue when they flush
	 * so we might actually want to change this to preserve order.
	 */
	while ((bp = LIST_FIRST(&wl->wl_bufs)) != NULL) {
		if (bbusy(bp, 0, 0, &wl->wl_mtx)) {
			continue;
		}
		bp->b_iodone = wapbl_biodone;
		bp->b_private = we;
		bremfree(bp);
		wapbl_remove_buf_locked(wl, bp);
		mutex_exit(&wl->wl_mtx);
		mutex_exit(&bufcache_lock);
		bawrite(bp);
		mutex_enter(&bufcache_lock);
		mutex_enter(&wl->wl_mtx);
	}
	mutex_exit(&wl->wl_mtx);
	mutex_exit(&bufcache_lock);

#if 0
	WAPBL_PRINTF(WAPBL_PRINT_FLUSH,
		     ("wapbl_flush thread %d.%d done flushing entries...\n",
		     curproc->p_pid, curlwp->l_lid));
#endif

 out:

	/*
	 * If the waitfor flag is set, don't return until everything is
	 * fully flushed and the on disk log is empty.
	 */
	if (waitfor) {
		error = wapbl_truncate(wl, wl->wl_circ_size - 
			wl->wl_reserved_bytes, wapbl_lazy_truncate);
	}

 out2:
	if (error) {
		wl->wl_flush_abort(wl->wl_mount, wl->wl_deallocblks,
		    wl->wl_dealloclens, wl->wl_dealloccnt);
	}

#ifdef WAPBL_DEBUG_PRINT
	if (error) {
		pid_t pid = -1;
		lwpid_t lid = -1;
		if (curproc)
			pid = curproc->p_pid;
		if (curlwp)
			lid = curlwp->l_lid;
		mutex_enter(&wl->wl_mtx);
#ifdef WAPBL_DEBUG_BUFBYTES
		WAPBL_PRINTF(WAPBL_PRINT_ERROR,
		    ("wapbl_flush: thread %d.%d aborted flush: "
		    "error = %d\n"
		    "\tbufcount=%zu bufbytes=%zu bcount=%zu "
		    "deallocs=%d inodes=%d\n"
		    "\terrcnt = %d, reclaimable=%zu reserved=%zu "
		    "unsynced=%zu\n",
		    pid, lid, error, wl->wl_bufcount,
		    wl->wl_bufbytes, wl->wl_bcount,
		    wl->wl_dealloccnt, wl->wl_inohashcnt,
		    wl->wl_error_count, wl->wl_reclaimable_bytes,
		    wl->wl_reserved_bytes, wl->wl_unsynced_bufbytes));
		SIMPLEQ_FOREACH(we, &wl->wl_entries, we_entries) {
			WAPBL_PRINTF(WAPBL_PRINT_ERROR,
			    ("\tentry: bufcount = %zu, reclaimable = %zu, "
			     "error = %d, unsynced = %zu\n",
			     we->we_bufcount, we->we_reclaimable_bytes,
			     we->we_error, we->we_unsynced_bufbytes));
		}
#else
		WAPBL_PRINTF(WAPBL_PRINT_ERROR,
		    ("wapbl_flush: thread %d.%d aborted flush: "
		     "error = %d\n"
		     "\tbufcount=%zu bufbytes=%zu bcount=%zu "
		     "deallocs=%d inodes=%d\n"
		     "\terrcnt = %d, reclaimable=%zu reserved=%zu\n",
		     pid, lid, error, wl->wl_bufcount,
		     wl->wl_bufbytes, wl->wl_bcount,
		     wl->wl_dealloccnt, wl->wl_inohashcnt,
		     wl->wl_error_count, wl->wl_reclaimable_bytes,
		     wl->wl_reserved_bytes));
		SIMPLEQ_FOREACH(we, &wl->wl_entries, we_entries) {
			WAPBL_PRINTF(WAPBL_PRINT_ERROR,
			    ("\tentry: bufcount = %zu, reclaimable = %zu, "
			     "error = %d\n", we->we_bufcount,
			     we->we_reclaimable_bytes, we->we_error));
		}
#endif
		mutex_exit(&wl->wl_mtx);
	}
#endif

	rw_exit(&wl->wl_rwlock);
	return error;
}

/****************************************************************/

void
wapbl_jlock_assert(struct wapbl *wl)
{

	KASSERT(rw_lock_held(&wl->wl_rwlock));
}

void
wapbl_junlock_assert(struct wapbl *wl)
{

	KASSERT(!rw_write_held(&wl->wl_rwlock));
}

/****************************************************************/

/* locks missing */
void
wapbl_print(struct wapbl *wl,
		int full,
		void (*pr)(const char *, ...))
{
	struct buf *bp;
	struct wapbl_entry *we;
	(*pr)("wapbl %p", wl);
	(*pr)("\nlogvp = %p, devvp = %p, logpbn = %"PRId64"\n",
	      wl->wl_logvp, wl->wl_devvp, wl->wl_logpbn);
	(*pr)("circ = %zu, header = %zu, head = %"PRIdMAX" tail = %"PRIdMAX"\n",
	      wl->wl_circ_size, wl->wl_circ_off,
	      (intmax_t)wl->wl_head, (intmax_t)wl->wl_tail);
	(*pr)("fs_dev_bshift = %d, log_dev_bshift = %d\n",
	      wl->wl_log_dev_bshift, wl->wl_fs_dev_bshift);
#ifdef WAPBL_DEBUG_BUFBYTES
	(*pr)("bufcount = %zu, bufbytes = %zu bcount = %zu reclaimable = %zu "
	      "reserved = %zu errcnt = %d unsynced = %zu\n",
	      wl->wl_bufcount, wl->wl_bufbytes, wl->wl_bcount,
	      wl->wl_reclaimable_bytes, wl->wl_reserved_bytes,
				wl->wl_error_count, wl->wl_unsynced_bufbytes);
#else
	(*pr)("bufcount = %zu, bufbytes = %zu bcount = %zu reclaimable = %zu "
	      "reserved = %zu errcnt = %d\n", wl->wl_bufcount, wl->wl_bufbytes,
	      wl->wl_bcount, wl->wl_reclaimable_bytes, wl->wl_reserved_bytes,
				wl->wl_error_count);
#endif
	(*pr)("\tdealloccnt = %d, dealloclim = %d\n",
	      wl->wl_dealloccnt, wl->wl_dealloclim);
	(*pr)("\tinohashcnt = %d, inohashmask = 0x%08x\n",
	      wl->wl_inohashcnt, wl->wl_inohashmask);
	(*pr)("entries:\n");
	SIMPLEQ_FOREACH(we, &wl->wl_entries, we_entries) {
#ifdef WAPBL_DEBUG_BUFBYTES
		(*pr)("\tbufcount = %zu, reclaimable = %zu, error = %d, "
		      "unsynced = %zu\n",
		      we->we_bufcount, we->we_reclaimable_bytes,
		      we->we_error, we->we_unsynced_bufbytes);
#else
		(*pr)("\tbufcount = %zu, reclaimable = %zu, error = %d\n",
		      we->we_bufcount, we->we_reclaimable_bytes, we->we_error);
#endif
	}
	if (full) {
		int cnt = 0;
		(*pr)("bufs =");
		LIST_FOREACH(bp, &wl->wl_bufs, b_wapbllist) {
			if (!LIST_NEXT(bp, b_wapbllist)) {
				(*pr)(" %p", bp);
			} else if ((++cnt % 6) == 0) {
				(*pr)(" %p,\n\t", bp);
			} else {
				(*pr)(" %p,", bp);
			}
		}
		(*pr)("\n");

		(*pr)("dealloced blks = ");
		{
			int i;
			cnt = 0;
			for (i = 0; i < wl->wl_dealloccnt; i++) {
				(*pr)(" %"PRId64":%d,",
				      wl->wl_deallocblks[i],
				      wl->wl_dealloclens[i]);
				if ((++cnt % 4) == 0) {
					(*pr)("\n\t");
				}
			}
		}
		(*pr)("\n");

		(*pr)("registered inodes = ");
		{
			int i;
			cnt = 0;
			for (i = 0; i <= wl->wl_inohashmask; i++) {
				struct wapbl_ino_head *wih;
				struct wapbl_ino *wi;

				wih = &wl->wl_inohash[i];
				LIST_FOREACH(wi, wih, wi_hash) {
					if (wi->wi_ino == 0)
						continue;
					(*pr)(" %"PRIu64"/0%06"PRIo32",",
					    wi->wi_ino, wi->wi_mode);
					if ((++cnt % 4) == 0) {
						(*pr)("\n\t");
					}
				}
			}
			(*pr)("\n");
		}
	}
}

#if defined(WAPBL_DEBUG) || defined(DDB)
void
wapbl_dump(struct wapbl *wl)
{
#if defined(WAPBL_DEBUG)
	if (!wl)
		wl = wapbl_debug_wl;
#endif
	if (!wl)
		return;
	wapbl_print(wl, 1, printf);
}
#endif

/****************************************************************/

void
wapbl_register_deallocation(struct wapbl *wl, daddr_t blk, int len)
{

	wapbl_jlock_assert(wl);

	mutex_enter(&wl->wl_mtx);
	/* XXX should eventually instead tie this into resource estimation */
	/*
	 * XXX this panic needs locking/mutex analysis and the
	 * ability to cope with the failure.
	 */
	/* XXX this XXX doesn't have enough XXX */
	if (__predict_false(wl->wl_dealloccnt >= wl->wl_dealloclim))
		panic("wapbl_register_deallocation: out of resources");

	wl->wl_deallocblks[wl->wl_dealloccnt] = blk;
	wl->wl_dealloclens[wl->wl_dealloccnt] = len;
	wl->wl_dealloccnt++;
	WAPBL_PRINTF(WAPBL_PRINT_ALLOC,
	    ("wapbl_register_deallocation: blk=%"PRId64" len=%d\n", blk, len));
	mutex_exit(&wl->wl_mtx);
}

/****************************************************************/

static void
wapbl_inodetrk_init(struct wapbl *wl, u_int size)
{

	wl->wl_inohash = hashinit(size, HASH_LIST, true, &wl->wl_inohashmask);
	if (atomic_inc_uint_nv(&wapbl_ino_pool_refcount) == 1) {
		pool_init(&wapbl_ino_pool, sizeof(struct wapbl_ino), 0, 0, 0,
		    "wapblinopl", &pool_allocator_nointr, IPL_NONE);
	}
}

static void
wapbl_inodetrk_free(struct wapbl *wl)
{

	/* XXX this KASSERT needs locking/mutex analysis */
	KASSERT(wl->wl_inohashcnt == 0);
	hashdone(wl->wl_inohash, HASH_LIST, wl->wl_inohashmask);
	if (atomic_dec_uint_nv(&wapbl_ino_pool_refcount) == 0) {
		pool_destroy(&wapbl_ino_pool);
	}
}

static struct wapbl_ino *
wapbl_inodetrk_get(struct wapbl *wl, ino_t ino)
{
	struct wapbl_ino_head *wih;
	struct wapbl_ino *wi;

	KASSERT(mutex_owned(&wl->wl_mtx));

	wih = &wl->wl_inohash[ino & wl->wl_inohashmask];
	LIST_FOREACH(wi, wih, wi_hash) {
		if (ino == wi->wi_ino)
			return wi;
	}
	return 0;
}

void
wapbl_register_inode(struct wapbl *wl, ino_t ino, mode_t mode)
{
	struct wapbl_ino_head *wih;
	struct wapbl_ino *wi;

	wi = pool_get(&wapbl_ino_pool, PR_WAITOK);

	mutex_enter(&wl->wl_mtx);
	if (wapbl_inodetrk_get(wl, ino) == NULL) {
		wi->wi_ino = ino;
		wi->wi_mode = mode;
		wih = &wl->wl_inohash[ino & wl->wl_inohashmask];
		LIST_INSERT_HEAD(wih, wi, wi_hash);
		wl->wl_inohashcnt++;
		WAPBL_PRINTF(WAPBL_PRINT_INODE,
		    ("wapbl_register_inode: ino=%"PRId64"\n", ino));
		mutex_exit(&wl->wl_mtx);
	} else {
		mutex_exit(&wl->wl_mtx);
		pool_put(&wapbl_ino_pool, wi);
	}
}

void
wapbl_unregister_inode(struct wapbl *wl, ino_t ino, mode_t mode)
{
	struct wapbl_ino *wi;

	mutex_enter(&wl->wl_mtx);
	wi = wapbl_inodetrk_get(wl, ino);
	if (wi) {
		WAPBL_PRINTF(WAPBL_PRINT_INODE,
		    ("wapbl_unregister_inode: ino=%"PRId64"\n", ino));
		KASSERT(wl->wl_inohashcnt > 0);
		wl->wl_inohashcnt--;
		LIST_REMOVE(wi, wi_hash);
		mutex_exit(&wl->wl_mtx);

		pool_put(&wapbl_ino_pool, wi);
	} else {
		mutex_exit(&wl->wl_mtx);
	}
}

/****************************************************************/

static inline size_t
wapbl_transaction_inodes_len(struct wapbl *wl)
{
	int blocklen = 1<<wl->wl_log_dev_bshift;
	int iph;

	/* Calculate number of inodes described in a inodelist header */
	iph = (blocklen - offsetof(struct wapbl_wc_inodelist, wc_inodes)) /
	    sizeof(((struct wapbl_wc_inodelist *)0)->wc_inodes[0]);

	KASSERT(iph > 0);

	return MAX(1, howmany(wl->wl_inohashcnt, iph)) * blocklen;
}


/* Calculate amount of space a transaction will take on disk */
static size_t
wapbl_transaction_len(struct wapbl *wl)
{
	int blocklen = 1<<wl->wl_log_dev_bshift;
	size_t len;
	int bph;

	/* Calculate number of blocks described in a blocklist header */
	bph = (blocklen - offsetof(struct wapbl_wc_blocklist, wc_blocks)) /
	    sizeof(((struct wapbl_wc_blocklist *)0)->wc_blocks[0]);

	KASSERT(bph > 0);

	len = wl->wl_bcount;
	len += howmany(wl->wl_bufcount, bph) * blocklen;
	len += howmany(wl->wl_dealloccnt, bph) * blocklen;
	len += wapbl_transaction_inodes_len(wl);

	return len;
}

/*
 * wapbl_cache_sync: issue DIOCCACHESYNC
 */
static int
wapbl_cache_sync(struct wapbl *wl, const char *msg)
{
	const bool verbose = wapbl_verbose_commit >= 2;
	struct bintime start_time;
	int force = 1;
	int error;

	if (!wapbl_flush_disk_cache) {
		return 0;
	}
	if (verbose) {
		bintime(&start_time);
	}
	error = VOP_IOCTL(wl->wl_devvp, DIOCCACHESYNC, &force,
	    FWRITE, FSCRED);
	if (error) {
		WAPBL_PRINTF(WAPBL_PRINT_ERROR,
		    ("wapbl_cache_sync: DIOCCACHESYNC on dev 0x%x "
		    "returned %d\n", wl->wl_devvp->v_rdev, error));
	}
	if (verbose) {
		struct bintime d;
		struct timespec ts;

		bintime(&d);
		bintime_sub(&d, &start_time);
		bintime2timespec(&d, &ts);
		printf("wapbl_cache_sync: %s: dev 0x%jx %ju.%09lu\n",
		    msg, (uintmax_t)wl->wl_devvp->v_rdev,
		    (uintmax_t)ts.tv_sec, ts.tv_nsec);
	}
	return error;
}

/*
 * Perform commit operation
 *
 * Note that generation number incrementation needs to
 * be protected against racing with other invocations
 * of wapbl_write_commit.  This is ok since this routine
 * is only invoked from wapbl_flush
 */
static int
wapbl_write_commit(struct wapbl *wl, off_t head, off_t tail)
{
	struct wapbl_wc_header *wc = wl->wl_wc_header;
	struct timespec ts;
	int error;
	daddr_t pbn;

	error = wapbl_buffered_flush(wl);
	if (error)
		return error;
	/*
	 * flush disk cache to ensure that blocks we've written are actually
	 * written to the stable storage before the commit header.
	 *
	 * XXX Calc checksum here, instead we do this for now
	 */
	wapbl_cache_sync(wl, "1");

	wc->wc_head = head;
	wc->wc_tail = tail;
	wc->wc_checksum = 0;
	wc->wc_version = 1;
	getnanotime(&ts);
	wc->wc_time = ts.tv_sec;
	wc->wc_timensec = ts.tv_nsec;

	WAPBL_PRINTF(WAPBL_PRINT_WRITE,
	    ("wapbl_write_commit: head = %"PRIdMAX "tail = %"PRIdMAX"\n",
	    (intmax_t)head, (intmax_t)tail));

	/*
	 * write the commit header.
	 *
	 * XXX if generation will rollover, then first zero
	 * over second commit header before trying to write both headers.
	 */

	pbn = wl->wl_logpbn + (wc->wc_generation % 2);
#ifdef _KERNEL
	pbn = btodb(pbn << wc->wc_log_dev_bshift);
#endif
	error = wapbl_buffered_write(wc, wc->wc_len, wl, pbn);
	if (error)
		return error;
	error = wapbl_buffered_flush(wl);
	if (error)
		return error;

	/*
	 * flush disk cache to ensure that the commit header is actually
	 * written before meta data blocks.
	 */
	wapbl_cache_sync(wl, "2");

	/*
	 * If the generation number was zero, write it out a second time.
	 * This handles initialization and generation number rollover
	 */
	if (wc->wc_generation++ == 0) {
		error = wapbl_write_commit(wl, head, tail);
		/*
		 * This panic should be able to be removed if we do the
		 * zero'ing mentioned above, and we are certain to roll
		 * back generation number on failure.
		 */
		if (error)
			panic("wapbl_write_commit: error writing duplicate "
			      "log header: %d\n", error);
	}
	return 0;
}

/* Returns new offset value */
static int
wapbl_write_blocks(struct wapbl *wl, off_t *offp)
{
	struct wapbl_wc_blocklist *wc =
	    (struct wapbl_wc_blocklist *)wl->wl_wc_scratch;
	int blocklen = 1<<wl->wl_log_dev_bshift;
	int bph;
	struct buf *bp;
	off_t off = *offp;
	int error;
	size_t padding;

	KASSERT(rw_write_held(&wl->wl_rwlock));

	bph = (blocklen - offsetof(struct wapbl_wc_blocklist, wc_blocks)) /
	    sizeof(((struct wapbl_wc_blocklist *)0)->wc_blocks[0]);

	bp = LIST_FIRST(&wl->wl_bufs);

	while (bp) {
		int cnt;
		struct buf *obp = bp;

		KASSERT(bp->b_flags & B_LOCKED);

		wc->wc_type = WAPBL_WC_BLOCKS;
		wc->wc_len = blocklen;
		wc->wc_blkcount = 0;
		while (bp && (wc->wc_blkcount < bph)) {
			/*
			 * Make sure all the physical block numbers are up to
			 * date.  If this is not always true on a given
			 * filesystem, then VOP_BMAP must be called.  We
			 * could call VOP_BMAP here, or else in the filesystem
			 * specific flush callback, although neither of those
			 * solutions allow us to take the vnode lock.  If a
			 * filesystem requires that we must take the vnode lock
			 * to call VOP_BMAP, then we can probably do it in
			 * bwrite when the vnode lock should already be held
			 * by the invoking code.
			 */
			KASSERT((bp->b_vp->v_type == VBLK) ||
				 (bp->b_blkno != bp->b_lblkno));
			KASSERT(bp->b_blkno > 0);

			wc->wc_blocks[wc->wc_blkcount].wc_daddr = bp->b_blkno;
			wc->wc_blocks[wc->wc_blkcount].wc_dlen = bp->b_bcount;
			wc->wc_len += bp->b_bcount;
			wc->wc_blkcount++;
			bp = LIST_NEXT(bp, b_wapbllist);
		}
		if (wc->wc_len % blocklen != 0) {
			padding = blocklen - wc->wc_len % blocklen;
			wc->wc_len += padding;
		} else {
			padding = 0;
		}

		WAPBL_PRINTF(WAPBL_PRINT_WRITE,
		    ("wapbl_write_blocks: len = %u (padding %zu) off = %"PRIdMAX"\n",
		    wc->wc_len, padding, (intmax_t)off));

		error = wapbl_circ_write(wl, wc, blocklen, &off);
		if (error)
			return error;
		bp = obp;
		cnt = 0;
		while (bp && (cnt++ < bph)) {
			error = wapbl_circ_write(wl, bp->b_data,
			    bp->b_bcount, &off);
			if (error)
				return error;
			bp = LIST_NEXT(bp, b_wapbllist);
		}
		if (padding) {
			void *zero;
			
			zero = wapbl_alloc(padding);
			memset(zero, 0, padding);
			error = wapbl_circ_write(wl, zero, padding, &off);
			wapbl_free(zero, padding);
			if (error)
				return error;
		}
	}
	*offp = off;
	return 0;
}

static int
wapbl_write_revocations(struct wapbl *wl, off_t *offp)
{
	struct wapbl_wc_blocklist *wc =
	    (struct wapbl_wc_blocklist *)wl->wl_wc_scratch;
	int i;
	int blocklen = 1<<wl->wl_log_dev_bshift;
	int bph;
	off_t off = *offp;
	int error;

	if (wl->wl_dealloccnt == 0)
		return 0;

	bph = (blocklen - offsetof(struct wapbl_wc_blocklist, wc_blocks)) /
	    sizeof(((struct wapbl_wc_blocklist *)0)->wc_blocks[0]);

	i = 0;
	while (i < wl->wl_dealloccnt) {
		wc->wc_type = WAPBL_WC_REVOCATIONS;
		wc->wc_len = blocklen;
		wc->wc_blkcount = 0;
		while ((i < wl->wl_dealloccnt) && (wc->wc_blkcount < bph)) {
			wc->wc_blocks[wc->wc_blkcount].wc_daddr =
			    wl->wl_deallocblks[i];
			wc->wc_blocks[wc->wc_blkcount].wc_dlen =
			    wl->wl_dealloclens[i];
			wc->wc_blkcount++;
			i++;
		}
		WAPBL_PRINTF(WAPBL_PRINT_WRITE,
		    ("wapbl_write_revocations: len = %u off = %"PRIdMAX"\n",
		    wc->wc_len, (intmax_t)off));
		error = wapbl_circ_write(wl, wc, blocklen, &off);
		if (error)
			return error;
	}
	*offp = off;
	return 0;
}

static int
wapbl_write_inodes(struct wapbl *wl, off_t *offp)
{
	struct wapbl_wc_inodelist *wc =
	    (struct wapbl_wc_inodelist *)wl->wl_wc_scratch;
	int i;
	int blocklen = 1 << wl->wl_log_dev_bshift;
	off_t off = *offp;
	int error;

	struct wapbl_ino_head *wih;
	struct wapbl_ino *wi;
	int iph;

	iph = (blocklen - offsetof(struct wapbl_wc_inodelist, wc_inodes)) /
	    sizeof(((struct wapbl_wc_inodelist *)0)->wc_inodes[0]);

	i = 0;
	wih = &wl->wl_inohash[0];
	wi = 0;
	do {
		wc->wc_type = WAPBL_WC_INODES;
		wc->wc_len = blocklen;
		wc->wc_inocnt = 0;
		wc->wc_clear = (i == 0);
		while ((i < wl->wl_inohashcnt) && (wc->wc_inocnt < iph)) {
			while (!wi) {
				KASSERT((wih - &wl->wl_inohash[0])
				    <= wl->wl_inohashmask);
				wi = LIST_FIRST(wih++);
			}
			wc->wc_inodes[wc->wc_inocnt].wc_inumber = wi->wi_ino;
			wc->wc_inodes[wc->wc_inocnt].wc_imode = wi->wi_mode;
			wc->wc_inocnt++;
			i++;
			wi = LIST_NEXT(wi, wi_hash);
		}
		WAPBL_PRINTF(WAPBL_PRINT_WRITE,
		    ("wapbl_write_inodes: len = %u off = %"PRIdMAX"\n",
		    wc->wc_len, (intmax_t)off));
		error = wapbl_circ_write(wl, wc, blocklen, &off);
		if (error)
			return error;
	} while (i < wl->wl_inohashcnt);
	
	*offp = off;
	return 0;
}

#endif /* _KERNEL */

/****************************************************************/

struct wapbl_blk {
	LIST_ENTRY(wapbl_blk) wb_hash;
	daddr_t wb_blk;
	off_t wb_off; /* Offset of this block in the log */
};
#define	WAPBL_BLKPOOL_MIN 83

static void
wapbl_blkhash_init(struct wapbl_replay *wr, u_int size)
{
	if (size < WAPBL_BLKPOOL_MIN)
		size = WAPBL_BLKPOOL_MIN;
	KASSERT(wr->wr_blkhash == 0);
#ifdef _KERNEL
	wr->wr_blkhash = hashinit(size, HASH_LIST, true, &wr->wr_blkhashmask);
#else /* ! _KERNEL */
	/* Manually implement hashinit */
	{
		unsigned long i, hashsize;
		for (hashsize = 1; hashsize < size; hashsize <<= 1)
			continue;
		wr->wr_blkhash = wapbl_alloc(hashsize * sizeof(*wr->wr_blkhash));
		for (i = 0; i < hashsize; i++)
			LIST_INIT(&wr->wr_blkhash[i]);
		wr->wr_blkhashmask = hashsize - 1;
	}
#endif /* ! _KERNEL */
}

static void
wapbl_blkhash_free(struct wapbl_replay *wr)
{
	KASSERT(wr->wr_blkhashcnt == 0);
#ifdef _KERNEL
	hashdone(wr->wr_blkhash, HASH_LIST, wr->wr_blkhashmask);
#else /* ! _KERNEL */
	wapbl_free(wr->wr_blkhash,
	    (wr->wr_blkhashmask + 1) * sizeof(*wr->wr_blkhash));
#endif /* ! _KERNEL */
}

static struct wapbl_blk *
wapbl_blkhash_get(struct wapbl_replay *wr, daddr_t blk)
{
	struct wapbl_blk_head *wbh;
	struct wapbl_blk *wb;
	wbh = &wr->wr_blkhash[blk & wr->wr_blkhashmask];
	LIST_FOREACH(wb, wbh, wb_hash) {
		if (blk == wb->wb_blk)
			return wb;
	}
	return 0;
}

static void
wapbl_blkhash_ins(struct wapbl_replay *wr, daddr_t blk, off_t off)
{
	struct wapbl_blk_head *wbh;
	struct wapbl_blk *wb;
	wb = wapbl_blkhash_get(wr, blk);
	if (wb) {
		KASSERT(wb->wb_blk == blk);
		wb->wb_off = off;
	} else {
		wb = wapbl_alloc(sizeof(*wb));
		wb->wb_blk = blk;
		wb->wb_off = off;
		wbh = &wr->wr_blkhash[blk & wr->wr_blkhashmask];
		LIST_INSERT_HEAD(wbh, wb, wb_hash);
		wr->wr_blkhashcnt++;
	}
}

static void
wapbl_blkhash_rem(struct wapbl_replay *wr, daddr_t blk)
{
	struct wapbl_blk *wb = wapbl_blkhash_get(wr, blk);
	if (wb) {
		KASSERT(wr->wr_blkhashcnt > 0);
		wr->wr_blkhashcnt--;
		LIST_REMOVE(wb, wb_hash);
		wapbl_free(wb, sizeof(*wb));
	}
}

static void
wapbl_blkhash_clear(struct wapbl_replay *wr)
{
	unsigned long i;
	for (i = 0; i <= wr->wr_blkhashmask; i++) {
		struct wapbl_blk *wb;

		while ((wb = LIST_FIRST(&wr->wr_blkhash[i]))) {
			KASSERT(wr->wr_blkhashcnt > 0);
			wr->wr_blkhashcnt--;
			LIST_REMOVE(wb, wb_hash);
			wapbl_free(wb, sizeof(*wb));
		}
	}
	KASSERT(wr->wr_blkhashcnt == 0);
}

/****************************************************************/

static int
wapbl_circ_read(struct wapbl_replay *wr, void *data, size_t len, off_t *offp)
{
	size_t slen;
	off_t off = *offp;
	int error;
	daddr_t pbn;

	KASSERT(((len >> wr->wr_log_dev_bshift) <<
	    wr->wr_log_dev_bshift) == len);

	if (off < wr->wr_circ_off)
		off = wr->wr_circ_off;
	slen = wr->wr_circ_off + wr->wr_circ_size - off;
	if (slen < len) {
		pbn = wr->wr_logpbn + (off >> wr->wr_log_dev_bshift);
#ifdef _KERNEL
		pbn = btodb(pbn << wr->wr_log_dev_bshift);
#endif
		error = wapbl_read(data, slen, wr->wr_devvp, pbn);
		if (error)
			return error;
		data = (uint8_t *)data + slen;
		len -= slen;
		off = wr->wr_circ_off;
	}
	pbn = wr->wr_logpbn + (off >> wr->wr_log_dev_bshift);
#ifdef _KERNEL
	pbn = btodb(pbn << wr->wr_log_dev_bshift);
#endif
	error = wapbl_read(data, len, wr->wr_devvp, pbn);
	if (error)
		return error;
	off += len;
	if (off >= wr->wr_circ_off + wr->wr_circ_size)
		off = wr->wr_circ_off;
	*offp = off;
	return 0;
}

static void
wapbl_circ_advance(struct wapbl_replay *wr, size_t len, off_t *offp)
{
	size_t slen;
	off_t off = *offp;

	KASSERT(((len >> wr->wr_log_dev_bshift) <<
	    wr->wr_log_dev_bshift) == len);

	if (off < wr->wr_circ_off)
		off = wr->wr_circ_off;
	slen = wr->wr_circ_off + wr->wr_circ_size - off;
	if (slen < len) {
		len -= slen;
		off = wr->wr_circ_off;
	}
	off += len;
	if (off >= wr->wr_circ_off + wr->wr_circ_size)
		off = wr->wr_circ_off;
	*offp = off;
}

/****************************************************************/

int
wapbl_replay_start(struct wapbl_replay **wrp, struct vnode *vp,
	daddr_t off, size_t count, size_t blksize)
{
	struct wapbl_replay *wr;
	int error;
	struct vnode *devvp;
	daddr_t logpbn;
	uint8_t *scratch;
	struct wapbl_wc_header *wch;
	struct wapbl_wc_header *wch2;
	/* Use this until we read the actual log header */
	int log_dev_bshift = ilog2(blksize);
	size_t used;
	daddr_t pbn;

	WAPBL_PRINTF(WAPBL_PRINT_REPLAY,
	    ("wapbl_replay_start: vp=%p off=%"PRId64 " count=%zu blksize=%zu\n",
	    vp, off, count, blksize));

	if (off < 0)
		return EINVAL;

	if (blksize < DEV_BSIZE)
		return EINVAL;
	if (blksize % DEV_BSIZE)
		return EINVAL;

#ifdef _KERNEL
#if 0
	/* XXX vp->v_size isn't reliably set for VBLK devices,
	 * especially root.  However, we might still want to verify
	 * that the full load is readable */
	if ((off + count) * blksize > vp->v_size)
		return EINVAL;
#endif
	if ((error = VOP_BMAP(vp, off, &devvp, &logpbn, 0)) != 0) {
		return error;
	}
#else /* ! _KERNEL */
	devvp = vp;
	logpbn = off;
#endif /* ! _KERNEL */

	scratch = wapbl_alloc(MAXBSIZE);

	pbn = logpbn;
#ifdef _KERNEL
	pbn = btodb(pbn << log_dev_bshift);
#endif
	error = wapbl_read(scratch, 2<<log_dev_bshift, devvp, pbn);
	if (error)
		goto errout;

	wch = (struct wapbl_wc_header *)scratch;
	wch2 =
	    (struct wapbl_wc_header *)(scratch + (1<<log_dev_bshift));
	/* XXX verify checksums and magic numbers */
	if (wch->wc_type != WAPBL_WC_HEADER) {
		printf("Unrecognized wapbl magic: 0x%08x\n", wch->wc_type);
		error = EFTYPE;
		goto errout;
	}

	if (wch2->wc_generation > wch->wc_generation)
		wch = wch2;

	wr = wapbl_calloc(1, sizeof(*wr));

	wr->wr_logvp = vp;
	wr->wr_devvp = devvp;
	wr->wr_logpbn = logpbn;

	wr->wr_scratch = scratch;

	wr->wr_log_dev_bshift = wch->wc_log_dev_bshift;
	wr->wr_fs_dev_bshift = wch->wc_fs_dev_bshift;
	wr->wr_circ_off = wch->wc_circ_off;
	wr->wr_circ_size = wch->wc_circ_size;
	wr->wr_generation = wch->wc_generation;

	used = wapbl_space_used(wch->wc_circ_size, wch->wc_head, wch->wc_tail);

	WAPBL_PRINTF(WAPBL_PRINT_REPLAY,
	    ("wapbl_replay: head=%"PRId64" tail=%"PRId64" off=%"PRId64
	    " len=%"PRId64" used=%zu\n",
	    wch->wc_head, wch->wc_tail, wch->wc_circ_off,
	    wch->wc_circ_size, used));

	wapbl_blkhash_init(wr, (used >> wch->wc_fs_dev_bshift));

	error = wapbl_replay_process(wr, wch->wc_head, wch->wc_tail);
	if (error) {
		wapbl_replay_stop(wr);
		wapbl_replay_free(wr);
		return error;
	}

	*wrp = wr;
	return 0;

 errout:
	wapbl_free(scratch, MAXBSIZE);
	return error;
}

void
wapbl_replay_stop(struct wapbl_replay *wr)
{

	if (!wapbl_replay_isopen(wr))
		return;

	WAPBL_PRINTF(WAPBL_PRINT_REPLAY, ("wapbl_replay_stop called\n"));

	wapbl_free(wr->wr_scratch, MAXBSIZE);
	wr->wr_scratch = NULL;

	wr->wr_logvp = NULL;

	wapbl_blkhash_clear(wr);
	wapbl_blkhash_free(wr);
}

void
wapbl_replay_free(struct wapbl_replay *wr)
{

	KDASSERT(!wapbl_replay_isopen(wr));

	if (wr->wr_inodes)
		wapbl_free(wr->wr_inodes,
		    wr->wr_inodescnt * sizeof(wr->wr_inodes[0]));
	wapbl_free(wr, sizeof(*wr));
}

#ifdef _KERNEL
int
wapbl_replay_isopen1(struct wapbl_replay *wr)
{

	return wapbl_replay_isopen(wr);
}
#endif

/*
 * calculate the disk address for the i'th block in the wc_blockblist
 * offset by j blocks of size blen.
 *
 * wc_daddr is always a kernel disk address in DEV_BSIZE units that
 * was written to the journal.
 *
 * The kernel needs that address plus the offset in DEV_BSIZE units.
 *
 * Userland needs that address plus the offset in blen units.
 *
 */
static daddr_t
wapbl_block_daddr(struct wapbl_wc_blocklist *wc, int i, int j, int blen)
{
	daddr_t pbn;

#ifdef _KERNEL
	pbn = wc->wc_blocks[i].wc_daddr + btodb(j * blen);
#else
	pbn = dbtob(wc->wc_blocks[i].wc_daddr) / blen + j;
#endif

	return pbn;
}

static void
wapbl_replay_process_blocks(struct wapbl_replay *wr, off_t *offp)
{
	struct wapbl_wc_blocklist *wc =
	    (struct wapbl_wc_blocklist *)wr->wr_scratch;
	int fsblklen = 1 << wr->wr_fs_dev_bshift;
	int i, j, n;

	for (i = 0; i < wc->wc_blkcount; i++) {
		/*
		 * Enter each physical block into the hashtable independently.
		 */
		n = wc->wc_blocks[i].wc_dlen >> wr->wr_fs_dev_bshift;
		for (j = 0; j < n; j++) {
			wapbl_blkhash_ins(wr, wapbl_block_daddr(wc, i, j, fsblklen),
			    *offp);
			wapbl_circ_advance(wr, fsblklen, offp);
		}
	}
}

static void
wapbl_replay_process_revocations(struct wapbl_replay *wr)
{
	struct wapbl_wc_blocklist *wc =
	    (struct wapbl_wc_blocklist *)wr->wr_scratch;
	int fsblklen = 1 << wr->wr_fs_dev_bshift;
	int i, j, n;

	for (i = 0; i < wc->wc_blkcount; i++) {
		/*
		 * Remove any blocks found from the hashtable.
		 */
		n = wc->wc_blocks[i].wc_dlen >> wr->wr_fs_dev_bshift;
		for (j = 0; j < n; j++)
			wapbl_blkhash_rem(wr, wapbl_block_daddr(wc, i, j, fsblklen));
	}
}

static void
wapbl_replay_process_inodes(struct wapbl_replay *wr, off_t oldoff, off_t newoff)
{
	struct wapbl_wc_inodelist *wc =
	    (struct wapbl_wc_inodelist *)wr->wr_scratch;
	void *new_inodes;
	const size_t oldsize = wr->wr_inodescnt * sizeof(wr->wr_inodes[0]);

	KASSERT(sizeof(wr->wr_inodes[0]) == sizeof(wc->wc_inodes[0]));

	/*
	 * Keep track of where we found this so location won't be
	 * overwritten.
	 */
	if (wc->wc_clear) {
		wr->wr_inodestail = oldoff;
		wr->wr_inodescnt = 0;
		if (wr->wr_inodes != NULL) {
			wapbl_free(wr->wr_inodes, oldsize);
			wr->wr_inodes = NULL;
		}
	}
	wr->wr_inodeshead = newoff;
	if (wc->wc_inocnt == 0)
		return;

	new_inodes = wapbl_alloc((wr->wr_inodescnt + wc->wc_inocnt) *
	    sizeof(wr->wr_inodes[0]));
	if (wr->wr_inodes != NULL) {
		memcpy(new_inodes, wr->wr_inodes, oldsize);
		wapbl_free(wr->wr_inodes, oldsize);
	}
	wr->wr_inodes = new_inodes;
	memcpy(&wr->wr_inodes[wr->wr_inodescnt], wc->wc_inodes,
	    wc->wc_inocnt * sizeof(wr->wr_inodes[0]));
	wr->wr_inodescnt += wc->wc_inocnt;
}

static int
wapbl_replay_process(struct wapbl_replay *wr, off_t head, off_t tail)
{
	off_t off;
	int error;

	int logblklen = 1 << wr->wr_log_dev_bshift;

	wapbl_blkhash_clear(wr);

	off = tail;
	while (off != head) {
		struct wapbl_wc_null *wcn;
		off_t saveoff = off;
		error = wapbl_circ_read(wr, wr->wr_scratch, logblklen, &off);
		if (error)
			goto errout;
		wcn = (struct wapbl_wc_null *)wr->wr_scratch;
		switch (wcn->wc_type) {
		case WAPBL_WC_BLOCKS:
			wapbl_replay_process_blocks(wr, &off);
			break;

		case WAPBL_WC_REVOCATIONS:
			wapbl_replay_process_revocations(wr);
			break;

		case WAPBL_WC_INODES:
			wapbl_replay_process_inodes(wr, saveoff, off);
			break;

		default:
			printf("Unrecognized wapbl type: 0x%08x\n",
			       wcn->wc_type);
 			error = EFTYPE;
			goto errout;
		}
		wapbl_circ_advance(wr, wcn->wc_len, &saveoff);
		if (off != saveoff) {
			printf("wapbl_replay: corrupted records\n");
			error = EFTYPE;
			goto errout;
		}
	}
	return 0;

 errout:
	wapbl_blkhash_clear(wr);
	return error;
}

#if 0
int
wapbl_replay_verify(struct wapbl_replay *wr, struct vnode *fsdevvp)
{
	off_t off;
	int mismatchcnt = 0;
	int logblklen = 1 << wr->wr_log_dev_bshift;
	int fsblklen = 1 << wr->wr_fs_dev_bshift;
	void *scratch1 = wapbl_alloc(MAXBSIZE);
	void *scratch2 = wapbl_alloc(MAXBSIZE);
	int error = 0;

	KDASSERT(wapbl_replay_isopen(wr));

	off = wch->wc_tail;
	while (off != wch->wc_head) {
		struct wapbl_wc_null *wcn;
#ifdef DEBUG
		off_t saveoff = off;
#endif
		error = wapbl_circ_read(wr, wr->wr_scratch, logblklen, &off);
		if (error)
			goto out;
		wcn = (struct wapbl_wc_null *)wr->wr_scratch;
		switch (wcn->wc_type) {
		case WAPBL_WC_BLOCKS:
			{
				struct wapbl_wc_blocklist *wc =
				    (struct wapbl_wc_blocklist *)wr->wr_scratch;
				int i;
				for (i = 0; i < wc->wc_blkcount; i++) {
					int foundcnt = 0;
					int dirtycnt = 0;
					int j, n;
					/*
					 * Check each physical block into the
					 * hashtable independently
					 */
					n = wc->wc_blocks[i].wc_dlen >>
					    wch->wc_fs_dev_bshift;
					for (j = 0; j < n; j++) {
						struct wapbl_blk *wb =
						   wapbl_blkhash_get(wr,
						   wapbl_block_daddr(wc, i, j, fsblklen));
						if (wb && (wb->wb_off == off)) {
							foundcnt++;
							error =
							    wapbl_circ_read(wr,
							    scratch1, fsblklen,
							    &off);
							if (error)
								goto out;
							error =
							    wapbl_read(scratch2,
							    fsblklen, fsdevvp,
							    wb->wb_blk);
							if (error)
								goto out;
							if (memcmp(scratch1,
								   scratch2,
								   fsblklen)) {
								printf(
		"wapbl_verify: mismatch block %"PRId64" at off %"PRIdMAX"\n",
		wb->wb_blk, (intmax_t)off);
								dirtycnt++;
								mismatchcnt++;
							}
						} else {
							wapbl_circ_advance(wr,
							    fsblklen, &off);
						}
					}
#if 0
					/*
					 * If all of the blocks in an entry
					 * are clean, then remove all of its
					 * blocks from the hashtable since they
					 * never will need replay.
					 */
					if ((foundcnt != 0) &&
					    (dirtycnt == 0)) {
						off = saveoff;
						wapbl_circ_advance(wr,
						    logblklen, &off);
						for (j = 0; j < n; j++) {
							struct wapbl_blk *wb =
							   wapbl_blkhash_get(wr,
							   wapbl_block_daddr(wc, i, j, fsblklen));
							if (wb &&
							  (wb->wb_off == off)) {
								wapbl_blkhash_rem(wr, wb->wb_blk);
							}
							wapbl_circ_advance(wr,
							    fsblklen, &off);
						}
					}
#endif
				}
			}
			break;
		case WAPBL_WC_REVOCATIONS:
		case WAPBL_WC_INODES:
			break;
		default:
			KASSERT(0);
		}
#ifdef DEBUG
		wapbl_circ_advance(wr, wcn->wc_len, &saveoff);
		KASSERT(off == saveoff);
#endif
	}
 out:
	wapbl_free(scratch1, MAXBSIZE);
	wapbl_free(scratch2, MAXBSIZE);
	if (!error && mismatchcnt)
		error = EFTYPE;
	return error;
}
#endif

int
wapbl_replay_write(struct wapbl_replay *wr, struct vnode *fsdevvp)
{
	struct wapbl_blk *wb;
	size_t i;
	off_t off;
	void *scratch;
	int error = 0;
	int fsblklen = 1 << wr->wr_fs_dev_bshift;

	KDASSERT(wapbl_replay_isopen(wr));

	scratch = wapbl_alloc(MAXBSIZE);

	for (i = 0; i <= wr->wr_blkhashmask; ++i) {
		LIST_FOREACH(wb, &wr->wr_blkhash[i], wb_hash) {
			off = wb->wb_off;
			error = wapbl_circ_read(wr, scratch, fsblklen, &off);
			if (error)
				break;
			error = wapbl_write(scratch, fsblklen, fsdevvp,
			    wb->wb_blk);
			if (error)
				break;
		}
	}

	wapbl_free(scratch, MAXBSIZE);
	return error;
}

int
wapbl_replay_can_read(struct wapbl_replay *wr, daddr_t blk, long len)
{
	int fsblklen = 1 << wr->wr_fs_dev_bshift;

	KDASSERT(wapbl_replay_isopen(wr));
	KASSERT((len % fsblklen) == 0);

	while (len != 0) {
		struct wapbl_blk *wb = wapbl_blkhash_get(wr, blk);
		if (wb)
			return 1;
		len -= fsblklen;
	}
	return 0;
}

int
wapbl_replay_read(struct wapbl_replay *wr, void *data, daddr_t blk, long len)
{
	int fsblklen = 1 << wr->wr_fs_dev_bshift;

	KDASSERT(wapbl_replay_isopen(wr));

	KASSERT((len % fsblklen) == 0);

	while (len != 0) {
		struct wapbl_blk *wb = wapbl_blkhash_get(wr, blk);
		if (wb) {
			off_t off = wb->wb_off;
			int error;
			error = wapbl_circ_read(wr, data, fsblklen, &off);
			if (error)
				return error;
		}
		data = (uint8_t *)data + fsblklen;
		len -= fsblklen;
		blk++;
	}
	return 0;
}

#ifdef _KERNEL
/*
 * This is not really a module now, but maybe on its way to
 * being one some day.
 */
MODULE(MODULE_CLASS_VFS, wapbl, NULL);

static int
wapbl_modcmd(modcmd_t cmd, void *arg)
{

	switch (cmd) {
	case MODULE_CMD_INIT:
		wapbl_init();
		return 0;
	case MODULE_CMD_FINI:
#ifdef notyet
		return wapbl_fini(true);
#endif
		return EOPNOTSUPP;
	default:
		return ENOTTY;
	}
}
#endif /* _KERNEL */

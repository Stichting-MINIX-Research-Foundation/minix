/* $NetBSD: udf_strat_rmw.c,v 1.27 2015/10/06 08:57:34 hannken Exp $ */

/*
 * Copyright (c) 2006, 2008 Reinoud Zandijk
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#include <sys/cdefs.h>
#ifndef lint
__KERNEL_RCSID(0, "$NetBSD: udf_strat_rmw.c,v 1.27 2015/10/06 08:57:34 hannken Exp $");
#endif /* not lint */


#if defined(_KERNEL_OPT)
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <miscfs/genfs/genfs_node.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/kthread.h>
#include <dev/clock_subr.h>

#include <fs/udf/ecma167-udf.h>
#include <fs/udf/udf_mount.h>

#include "udf.h"
#include "udf_subr.h"
#include "udf_bswap.h"


#define VTOI(vnode) ((struct udf_node *) (vnode)->v_data)
#define PRIV(ump) ((struct strat_private *) (ump)->strategy_private)
#define BTOE(buf) ((struct udf_eccline *) ((buf)->b_private))

/* --------------------------------------------------------------------- */

#define UDF_MAX_PACKET_SIZE	64			/* DONT change this */

/* sheduler states */
#define UDF_SHED_WAITING	1			/* waiting on timeout */
#define UDF_SHED_READING	2
#define UDF_SHED_WRITING	3
#define UDF_SHED_SEQWRITING	4
#define UDF_SHED_IDLE		5			/* refcnt'd */
#define UDF_SHED_FREE		6			/* recycleable */
#define UDF_SHED_MAX		6+1

/* flags */
#define ECC_LOCKED		0x01			/* prevent access   */
#define ECC_WANTED		0x02			/* trying access    */
#define ECC_SEQWRITING		0x04			/* sequential queue */
#define ECC_FLOATING		0x08			/* not queued yet   */

#define ECC_WAITTIME		10


TAILQ_HEAD(ecclineq, udf_eccline);
struct udf_eccline {
	struct udf_mount	 *ump;
	uint64_t		  present;		/* preserve these */
	uint64_t		  readin;		/* bitmap */
	uint64_t		  dirty;		/* bitmap */
	uint64_t		  error;		/* bitmap */
	uint32_t		  refcnt;

	struct timespec		  wait_time;
	uint32_t		  flags;
	uint32_t		  start_sector;		/* physical */

	const char		 *fname;
	int			  sline;

	struct buf		 *buf;
	void			 *blob;

	struct buf		 *bufs[UDF_MAX_PACKET_SIZE];
	uint32_t		  bufs_bpos[UDF_MAX_PACKET_SIZE];
	int			  bufs_len[UDF_MAX_PACKET_SIZE];

	int			  queued_on;		/* on which BUFQ list */
	LIST_ENTRY(udf_eccline)   hashchain;		/* on sector lookup  */
};


struct strat_private {
	lwp_t			 *queue_lwp;
	kcondvar_t		  discstrat_cv;		/* to wait on       */
	kmutex_t		  discstrat_mutex;	/* disc strategy    */
	kmutex_t		  seqwrite_mutex;	/* protect mappings */

	int			  thread_running;	/* thread control */
	int			  run_thread;		/* thread control */
	int			  thread_finished;	/* thread control */
	int			  cur_queue;

	int			  num_floating;
	int			  num_queued[UDF_SHED_MAX];
	struct bufq_state	 *queues[UDF_SHED_MAX];
	struct timespec		  last_queued[UDF_SHED_MAX];
	struct disk_strategy	  old_strategy_setting;

	struct pool		  eccline_pool;
	struct pool		  ecclineblob_pool;
	LIST_HEAD(, udf_eccline)  eccline_hash[UDF_ECCBUF_HASHSIZE];
};

/* --------------------------------------------------------------------- */

#define UDF_LOCK_ECCLINE(eccline) udf_lock_eccline(eccline, __FILE__, __LINE__)
#define UDF_UNLOCK_ECCLINE(eccline) udf_unlock_eccline(eccline, __FILE__, __LINE__)

/* can be called with or without discstrat lock */
static void
udf_lock_eccline(struct udf_eccline *eccline, const char *fname, int sline)
{
	struct strat_private *priv = PRIV(eccline->ump);
	int waslocked, ret;

	KASSERT(mutex_owned(&priv->discstrat_mutex));

	waslocked = mutex_owned(&priv->discstrat_mutex);
	if (!waslocked)
		mutex_enter(&priv->discstrat_mutex);

	/* wait until its unlocked first */
	eccline->refcnt++;
	while (eccline->flags & ECC_LOCKED) {
		DPRINTF(ECCLINE, ("waiting for lock at %s:%d\n",
					fname, sline));
		DPRINTF(ECCLINE, ("was locked at %s:%d\n",
					eccline->fname, eccline->sline));
		eccline->flags |= ECC_WANTED;
		ret = cv_timedwait(&priv->discstrat_cv, &priv->discstrat_mutex,
			hz/8);
		if (ret == EWOULDBLOCK)
			DPRINTF(LOCKING, ("eccline lock helt, waiting for "
				"release"));
	}
	eccline->flags |= ECC_LOCKED;
	eccline->flags &= ~ECC_WANTED;
	eccline->refcnt--;

	eccline->fname = fname;
	eccline->sline = sline;

	if (!waslocked)
		mutex_exit(&priv->discstrat_mutex);
}


/* can be called with or without discstrat lock */
static void
udf_unlock_eccline(struct udf_eccline *eccline, const char *fname, int sline)
{
	struct strat_private *priv = PRIV(eccline->ump);
	int waslocked;

	KASSERT(mutex_owned(&priv->discstrat_mutex));

	waslocked = mutex_owned(&priv->discstrat_mutex);
	if (!waslocked)
		mutex_enter(&priv->discstrat_mutex);

	eccline->flags &= ~ECC_LOCKED;
	cv_broadcast(&priv->discstrat_cv);

	if (!waslocked)
		mutex_exit(&priv->discstrat_mutex);
}


/* NOTE discstrat_mutex should be held! */
static void
udf_dispose_eccline(struct udf_eccline *eccline)
{
	struct strat_private *priv = PRIV(eccline->ump);

	KASSERT(mutex_owned(&priv->discstrat_mutex));

	DPRINTF(ECCLINE, ("dispose eccline with start sector %d, "
		"present %0"PRIx64"\n", eccline->start_sector,
		eccline->present));

	KASSERT(eccline->refcnt == 0);
	KASSERT(eccline->dirty  == 0);
	KASSERT(eccline->queued_on == 0);
	KASSERT(eccline->flags & ECC_FLOATING);
	KASSERT(eccline->flags & ECC_LOCKED);

	LIST_REMOVE(eccline, hashchain);
	priv->num_floating--;

	putiobuf(eccline->buf);
	pool_put(&priv->ecclineblob_pool, eccline->blob);
	pool_put(&priv->eccline_pool, eccline);
}


/* NOTE discstrat_mutex should be held! */
static void
udf_push_eccline(struct udf_eccline *eccline, int newqueue)
{
	struct strat_private *priv = PRIV(eccline->ump);

	KASSERT(mutex_owned(&priv->discstrat_mutex));

	DPRINTF(PARANOIA, ("DEBUG: buf %p pushed on queue %d\n", eccline->buf, newqueue));

	KASSERT(eccline->queued_on == 0);
	KASSERT(eccline->flags & ECC_FLOATING);

	/* set buffer block numbers to make sure its queued correctly */
	eccline->buf->b_lblkno   = eccline->start_sector;
	eccline->buf->b_blkno    = eccline->start_sector;
	eccline->buf->b_rawblkno = eccline->start_sector;

	vfs_timestamp(&priv->last_queued[newqueue]);
	eccline->flags &= ~ECC_FLOATING;
	priv->num_floating--;
	eccline->queued_on = newqueue;
	priv->num_queued[newqueue]++;
	bufq_put(priv->queues[newqueue], eccline->buf);

	UDF_UNLOCK_ECCLINE(eccline);

	/* XXX tickle disc strategy statemachine */
	if (newqueue != UDF_SHED_IDLE)
		cv_signal(&priv->discstrat_cv);
}


static struct udf_eccline *
udf_peek_eccline(struct strat_private *priv, int queued_on)
{
	struct udf_eccline *eccline;
	struct buf *buf;

	KASSERT(mutex_owned(&priv->discstrat_mutex));

	for(;;) {
		buf = bufq_peek(priv->queues[queued_on]);
		/* could have been a race, but we'll revisit later */
		if (buf == NULL)
			return NULL;

		eccline = BTOE(buf);
		UDF_LOCK_ECCLINE(eccline);

		/* might have changed before we obtained the lock */
		if (eccline->queued_on == queued_on)
			break;

		UDF_UNLOCK_ECCLINE(eccline);
	}

	KASSERT(eccline->queued_on == queued_on);
	KASSERT((eccline->flags & ECC_FLOATING) == 0);

	DPRINTF(PARANOIA, ("DEBUG: buf %p peeked at queue %d\n",
		eccline->buf, queued_on));

	return eccline;
}


static struct udf_eccline *
udf_pop_eccline(struct strat_private *priv, int queued_on)
{
	struct udf_eccline *eccline;
	struct buf *buf;

	KASSERT(mutex_owned(&priv->discstrat_mutex));

	for(;;) {
		buf = bufq_get(priv->queues[queued_on]);
		if (buf == NULL) {
			// KASSERT(priv->num_queued[queued_on] == 0);
			return NULL;
		}

		eccline = BTOE(buf);
		UDF_LOCK_ECCLINE(eccline);

		/* might have changed before we obtained the lock */
		if (eccline->queued_on == queued_on)
			break;

		UDF_UNLOCK_ECCLINE(eccline);
	}

	KASSERT(eccline->queued_on == queued_on);
	KASSERT((eccline->flags & ECC_FLOATING) == 0);

	priv->num_queued[queued_on]--;
	eccline->queued_on = 0;

	eccline->flags |= ECC_FLOATING;
	priv->num_floating++;

	DPRINTF(PARANOIA, ("DEBUG: buf %p popped from queue %d\n",
		eccline->buf, queued_on));

	return eccline;
}


static void
udf_unqueue_eccline(struct strat_private *priv, struct udf_eccline *eccline)
{
	struct buf *ret __diagused;

	UDF_LOCK_ECCLINE(eccline);
	if (eccline->queued_on == 0) {
		KASSERT(eccline->flags & ECC_FLOATING);
		return;
	}

	ret = bufq_cancel(priv->queues[eccline->queued_on], eccline->buf);
	KASSERT(ret == eccline->buf);

	priv->num_queued[eccline->queued_on]--;
	eccline->queued_on = 0;

	eccline->flags |= ECC_FLOATING;
	priv->num_floating++;
}


static struct udf_eccline *
udf_geteccline(struct udf_mount *ump, uint32_t sector, int flags)
{
	struct strat_private *priv = PRIV(ump);
	struct udf_eccline *eccline;
	uint32_t start_sector, lb_size, blobsize;
	uint8_t *eccline_blob;
	int line, line_offset;
	int num_busy;

	mutex_enter(&priv->discstrat_mutex);

	/* lookup in our line cache hashtable */
	line_offset  = sector % ump->packet_size;
	start_sector = sector - line_offset;
	line = (start_sector/ump->packet_size) & UDF_ECCBUF_HASHMASK;

	KASSERT(priv->thread_running);

retry:
	DPRINTF(ECCLINE, ("get line sector %d, line %d\n", sector, line));
	LIST_FOREACH(eccline, &priv->eccline_hash[line], hashchain) {
		if (eccline->start_sector == start_sector) {
			DPRINTF(ECCLINE, ("\tfound eccline, start_sector %d\n",
				eccline->start_sector));
			udf_unqueue_eccline(priv, eccline);

			mutex_exit(&priv->discstrat_mutex);
			return eccline;
		}
	}

	/* not found in eccline cache */
	DPRINTF(ECCLINE, ("\tnot found in eccline cache\n"));

	lb_size  = udf_rw32(ump->logical_vol->lb_size);
	blobsize = ump->packet_size * lb_size;

	/* dont allow too many pending requests */
	DPRINTF(ECCLINE, ("\tallocating new eccline\n"));
	num_busy = (priv->num_queued[UDF_SHED_SEQWRITING] + priv->num_floating);
	if ((flags & ECC_SEQWRITING) && (num_busy > UDF_ECCLINE_MAXBUSY)) {
		cv_timedwait(&priv->discstrat_cv,
			&priv->discstrat_mutex, hz/8);
		goto retry;
	}

	eccline_blob = pool_get(&priv->ecclineblob_pool, PR_NOWAIT);
	eccline = pool_get(&priv->eccline_pool, PR_NOWAIT);
	if ((eccline_blob == NULL) || (eccline == NULL)) {
		if (eccline_blob)
			pool_put(&priv->ecclineblob_pool, eccline_blob);
		if (eccline)
			pool_put(&priv->eccline_pool, eccline);

		/* out of memory for now; canibalise freelist */
		eccline = udf_pop_eccline(priv, UDF_SHED_FREE);
		if (eccline == NULL) {
			/* serious trouble; wait and retry */
			cv_timedwait(&priv->discstrat_cv,
				&priv->discstrat_mutex, hz/8);
			goto retry;
		}

		/* push back line if we're waiting for it or its locked */
		if (eccline->flags & ECC_WANTED) {
			/* we won a race, but someone else needed it */
			udf_push_eccline(eccline, UDF_SHED_FREE);
			goto retry;
		}

		/* unlink this entry */
		LIST_REMOVE(eccline, hashchain);
		KASSERT(eccline->flags & ECC_FLOATING);
		KASSERT(eccline->queued_on == 0);

		eccline_blob = eccline->blob;
		eccline->flags = ECC_FLOATING | ECC_LOCKED;
	} else {
		eccline->flags = ECC_FLOATING | ECC_LOCKED;
		priv->num_floating++;
	}

	eccline->queued_on = 0;
	eccline->blob = eccline_blob;
	eccline->buf  = getiobuf(NULL, true);
	eccline->buf->b_private = eccline;	/* IMPORTANT */

	/* initialise eccline blob */
	/* XXX memset expensive and strictly not needed XXX */
	memset(eccline->blob, 0, blobsize);

	eccline->ump = ump;
	eccline->present = eccline->readin = eccline->dirty = 0;
	eccline->error = 0;
	eccline->refcnt = 0;
	memset(eccline->bufs, 0, UDF_MAX_PACKET_SIZE * sizeof(struct buf *));

	eccline->start_sector    = start_sector;
	eccline->buf->b_lblkno   = start_sector;
	eccline->buf->b_blkno    = start_sector;
	eccline->buf->b_rawblkno = start_sector;

	LIST_INSERT_HEAD(&priv->eccline_hash[line], eccline, hashchain);

	/*
	 * TODO possible optimalisation for checking overlap with partitions
	 * to get a clue on future eccline usage
	 */

	KASSERT(eccline->refcnt == 0);
	KASSERT(eccline->flags & ECC_FLOATING);
	KASSERT(eccline->flags & ECC_LOCKED);
	mutex_exit(&priv->discstrat_mutex);

	return eccline;
}


static void
udf_puteccline(struct udf_eccline *eccline)
{
	struct strat_private *priv = PRIV(eccline->ump);
	struct udf_mount *ump = eccline->ump;
	uint64_t allbits = ((uint64_t) 1 << ump->packet_size)-1;
	int new_queue;

	mutex_enter(&priv->discstrat_mutex);

	DPRINTF(ECCLINE, ("put eccline start sector %d, refcnt %d\n",
		eccline->start_sector, eccline->refcnt));

	KASSERT(eccline->flags & ECC_LOCKED);
	KASSERT(eccline->flags & ECC_FLOATING);

	/* clear all read bits that are already read in */
	if (eccline->readin & eccline->present)
		eccline->readin &= (~eccline->present) & allbits;

	/* if we have active nodes we dont set it on seqwriting */
	if (eccline->refcnt > 1)
		eccline->flags &= ~ECC_SEQWRITING;

	/* select state */
	new_queue = UDF_SHED_FREE;
	if (eccline->refcnt > 0)
		new_queue = UDF_SHED_IDLE;
	if (eccline->flags & ECC_WANTED)
		new_queue = UDF_SHED_IDLE;
	if (eccline->readin)
		new_queue = UDF_SHED_READING;
	if (eccline->dirty) {
		new_queue = UDF_SHED_WAITING;
		vfs_timestamp(&eccline->wait_time);
		eccline->wait_time.tv_sec += ECC_WAITTIME;

		if (eccline->present == allbits) {
			new_queue = UDF_SHED_WRITING;
			if (eccline->flags & ECC_SEQWRITING)
				new_queue = UDF_SHED_SEQWRITING;
		}
	}
	udf_push_eccline(eccline, new_queue);

	mutex_exit(&priv->discstrat_mutex);
}

/* --------------------------------------------------------------------- */

static int
udf_create_nodedscr_rmw(struct udf_strat_args *args)
{
	union dscrptr   **dscrptr  = &args->dscr;
	struct udf_mount *ump      = args->ump;
	struct long_ad   *icb      = args->icb;
	struct udf_eccline *eccline;
	uint64_t bit;
	uint32_t sectornr, lb_size, dummy;
	uint8_t *mem;
	int error, eccsect;

	error = udf_translate_vtop(ump, icb, &sectornr, &dummy);
	if (error)
		return error;

	lb_size  = udf_rw32(ump->logical_vol->lb_size);

	/* get our eccline */
	eccline = udf_geteccline(ump, sectornr, 0);
	eccsect = sectornr - eccline->start_sector;

	bit = (uint64_t) 1 << eccsect;
	eccline->readin  &= ~bit;	/* just in case */
	eccline->present |=  bit;
	eccline->dirty   &= ~bit;	/* Err... euhm... clean? */

	eccline->refcnt++;

	/* clear space */
	mem = ((uint8_t *) eccline->blob) + eccsect * lb_size;
	memset(mem, 0, lb_size);

	udf_puteccline(eccline);

	*dscrptr = (union dscrptr *) mem;
	return 0;
}


static void
udf_free_nodedscr_rmw(struct udf_strat_args *args)
{
	struct udf_mount *ump  = args->ump;
	struct long_ad   *icb  = args->icb;
	struct udf_eccline *eccline;
	uint64_t bit;
	uint32_t sectornr, dummy;
	int error, eccsect;

	error = udf_translate_vtop(ump, icb, &sectornr, &dummy);
	if (error)
		return;

	/* get our eccline */
	eccline = udf_geteccline(ump, sectornr, 0);
	eccsect = sectornr - eccline->start_sector;

	bit = (uint64_t) 1 << eccsect;
	KASSERT(eccline->present & bit);

	eccline->readin &= ~bit;	/* just in case */
	/* XXX eccline->dirty? */

	KASSERT(eccline->refcnt >= 1);
	eccline->refcnt--;

	udf_puteccline(eccline);
}


static int
udf_read_nodedscr_rmw(struct udf_strat_args *args)
{
	union dscrptr   **dscrptr = &args->dscr;
	struct udf_mount *ump = args->ump;
	struct long_ad   *icb = args->icb;
	struct strat_private *priv;
	struct udf_eccline *eccline;
	uint64_t bit;
	uint32_t sectornr, dummy;
	uint8_t *pos;
	int sector_size = ump->discinfo.sector_size;
	int lb_size __diagused = udf_rw32(ump->logical_vol->lb_size);
	int i, error, dscrlen, eccsect;

	KASSERT(sector_size == lb_size);
	error = udf_translate_vtop(ump, icb, &sectornr, &dummy);
	if (error)
		return error;

	/* get our eccline */
	eccline = udf_geteccline(ump, sectornr, 0);
	eccsect = sectornr - eccline->start_sector;

	bit = (uint64_t) 1 << eccsect;
	if ((eccline->present & bit) == 0) {
		/* mark bit for readin */
		eccline->readin |= bit;
		eccline->refcnt++;	/* prevent recycling */
		KASSERT(eccline->bufs[eccsect] == NULL);
		udf_puteccline(eccline);

		/* wait for completion */
		priv = PRIV(eccline->ump);
		mutex_enter(&priv->discstrat_mutex);
		while (((eccline->present | eccline->error) & bit) == 0) {
			error = cv_timedwait(&priv->discstrat_cv,
				&priv->discstrat_mutex,
				hz/8);
			if (error == EWOULDBLOCK)
				DPRINTF(LOCKING, ("eccline waiting for read\n"));
		}
		mutex_exit(&priv->discstrat_mutex);

		/* reget our line */
		eccline = udf_geteccline(ump, sectornr, 0);
		KASSERT(eccline->refcnt >= 1);
		eccline->refcnt--;	/* undo refcnt */

		if (eccline->error & bit) {
			*dscrptr = NULL;
			udf_puteccline(eccline);
			return EIO;		/* XXX error code */
		}
	}

	*dscrptr = (union dscrptr *)
		(((uint8_t *) eccline->blob) + eccsect * sector_size);

	/* code from read_phys_descr */
	/* check if its a valid tag */
	error = udf_check_tag(*dscrptr);
	if (error) {
		/* check if its an empty block */
		pos = (uint8_t *) *dscrptr;
		for (i = 0; i < sector_size; i++, pos++) {
			if (*pos) break;
		}
		if (i == sector_size) {
			/* return no error but with no dscrptr */
			error = 0;
		}
		*dscrptr = NULL;
		udf_puteccline(eccline);
		return error;
	}

	/* calculate descriptor size */
	dscrlen = udf_tagsize(*dscrptr, sector_size);
	error = udf_check_tag_payload(*dscrptr, dscrlen);
	if (error) {
		*dscrptr = NULL;
		udf_puteccline(eccline);
		return error;
	}

	/* we have a hold since it has a node descriptor */
	eccline->refcnt++;
	udf_puteccline(eccline);

	return 0;
}


static int
udf_write_nodedscr_rmw(struct udf_strat_args *args)
{
	union dscrptr    *dscrptr = args->dscr;
	struct udf_mount *ump = args->ump;
	struct long_ad   *icb = args->icb;
	struct udf_node *udf_node = args->udf_node;
	struct udf_eccline *eccline;
	uint64_t bit;
	uint32_t sectornr, logsectornr, dummy;
	// int waitfor  = args->waitfor;
	int sector_size = ump->discinfo.sector_size;
	int lb_size __diagused = udf_rw32(ump->logical_vol->lb_size);
	int error, eccsect;

	KASSERT(sector_size == lb_size);
	sectornr    = 0;
	error = udf_translate_vtop(ump, icb, &sectornr, &dummy);
	if (error)
		return error;

	/* get our eccline */
	eccline = udf_geteccline(ump, sectornr, 0);
	eccsect = sectornr - eccline->start_sector;

	bit = (uint64_t) 1 << eccsect;

	/* old callback still pending? */
	if (eccline->bufs[eccsect]) {
		DPRINTF(WRITE, ("udf_write_nodedscr_rmw: writing descriptor"
					" over buffer?\n"));
		nestiobuf_done(eccline->bufs[eccsect],
				eccline->bufs_len[eccsect],
				0);
		eccline->bufs[eccsect] = NULL;
	}

	/* set sector number in the descriptor and validate */
	dscrptr = (union dscrptr *)
		(((uint8_t *) eccline->blob) + eccsect * sector_size);
	KASSERT(dscrptr == args->dscr);

	logsectornr = udf_rw32(icb->loc.lb_num);
	dscrptr->tag.tag_loc = udf_rw32(logsectornr);
	udf_validate_tag_and_crc_sums(dscrptr);

	udf_fixup_node_internals(ump, (uint8_t *) dscrptr, UDF_C_NODE);

	/* set our flags */
	KASSERT(eccline->present & bit);
	eccline->dirty |= bit;

	KASSERT(udf_tagsize(dscrptr, sector_size) <= sector_size);

	udf_node->outstanding_nodedscr--;
	if (udf_node->outstanding_nodedscr == 0) {
		/* XXX still using wakeup! */
		UDF_UNLOCK_NODE(udf_node, 0);
		wakeup(&udf_node->outstanding_nodedscr);
	}
	udf_puteccline(eccline);

	/* XXX waitfor not used */
	return 0;
}


static void
udf_queuebuf_rmw(struct udf_strat_args *args)
{
	struct udf_mount *ump = args->ump;
	struct buf *buf = args->nestbuf;
	struct desc_tag *tag;
	struct strat_private *priv = PRIV(ump);
	struct udf_eccline *eccline;
	struct long_ad *node_ad_cpy;
	uint64_t bit, *lmapping, *pmapping, *lmappos, *pmappos, blknr;
	uint32_t buf_len, len, sectors, sectornr, our_sectornr;
	uint32_t bpos;
	uint16_t vpart_num;
	uint8_t *fidblk, *src, *dst;
	int sector_size = ump->discinfo.sector_size;
	int blks = sector_size / DEV_BSIZE;
	int eccsect, what, queue, error;

	KASSERT(ump);
	KASSERT(buf);
	KASSERT(buf->b_iodone == nestiobuf_iodone);

	blknr        = buf->b_blkno;
	our_sectornr = blknr / blks;

	what = buf->b_udf_c_type;
	queue = UDF_SHED_READING;
	if ((buf->b_flags & B_READ) == 0) {
		/* writing */
		queue = UDF_SHED_SEQWRITING;
		if (what == UDF_C_ABSOLUTE)
			queue = UDF_SHED_WRITING;
		if (what == UDF_C_DSCR)
			queue = UDF_SHED_WRITING;
		if (what == UDF_C_NODE)
			queue = UDF_SHED_WRITING;
	}

	if (queue == UDF_SHED_READING) {
		DPRINTF(SHEDULE, ("\nudf_queuebuf_rmw READ %p : sector %d type %d,"
			"b_resid %d, b_bcount %d, b_bufsize %d\n",
			buf, (uint32_t) buf->b_blkno / blks, buf->b_udf_c_type,
			buf->b_resid, buf->b_bcount, buf->b_bufsize));

		/* mark bits for reading */
		buf_len = buf->b_bcount;
		sectornr = our_sectornr;
		eccline = udf_geteccline(ump, sectornr, 0);
		eccsect = sectornr - eccline->start_sector;
		bpos = 0;
		while (buf_len) {
			len = MIN(buf_len, sector_size);
			if ((eccsect < 0) || (eccsect >= ump->packet_size)) {
				udf_puteccline(eccline);
				eccline = udf_geteccline(ump, sectornr, 0);
				eccsect = sectornr - eccline->start_sector;
			}
			bit = (uint64_t) 1 << eccsect;
			error = eccline->error & bit ? EIO : 0;
			if (eccline->present & bit) {
				src = (uint8_t *) eccline->blob + 
					eccsect * sector_size;
				dst = (uint8_t *) buf->b_data + bpos;
				if (!error)
					memcpy(dst, src, len);
				nestiobuf_done(buf, len, error);
			} else {
				eccline->readin |= bit;
				KASSERT(eccline->bufs[eccsect] == NULL);
				eccline->bufs[eccsect] = buf;
				eccline->bufs_bpos[eccsect] = bpos;
				eccline->bufs_len[eccsect] = len;
			}
			bpos += sector_size;
			eccsect++;
			sectornr++;
			buf_len -= len;
		}
		udf_puteccline(eccline);
		return;
	}

	if (queue == UDF_SHED_WRITING) {
		DPRINTF(SHEDULE, ("\nudf_queuebuf_rmw WRITE %p : sector %d "
			"type %d, b_resid %d, b_bcount %d, b_bufsize %d\n",
			buf, (uint32_t) buf->b_blkno / blks, buf->b_udf_c_type,
			buf->b_resid, buf->b_bcount, buf->b_bufsize));

		/* if we have FIDs fixup using buffer's sector number(s) */
		if (buf->b_udf_c_type == UDF_C_FIDS)
			panic("UDF_C_FIDS in SHED_WRITING!\n");

		udf_fixup_node_internals(ump, buf->b_data, buf->b_udf_c_type);

		/* copy parts into the bufs and set for writing */
		buf_len = buf->b_bcount;
		sectornr = our_sectornr;
		eccline = udf_geteccline(ump, sectornr, 0);
		eccsect = sectornr - eccline->start_sector;
		bpos = 0;
		while (buf_len) {
			len = MIN(buf_len, sector_size);
			if ((eccsect < 0) || (eccsect >= ump->packet_size)) {
				udf_puteccline(eccline);
				eccline = udf_geteccline(ump, sectornr, 0);
				eccsect = sectornr - eccline->start_sector;
			}
			bit = (uint64_t) 1 << eccsect;
			KASSERT((eccline->readin & bit) == 0);
			eccline->present |= bit;
			eccline->dirty   |= bit;
			if (eccline->bufs[eccsect]) {
				/* old callback still pending */
				nestiobuf_done(eccline->bufs[eccsect],
						eccline->bufs_len[eccsect],
						0);
				eccline->bufs[eccsect] = NULL;
			}

			src = (uint8_t *) buf->b_data + bpos;
			dst = (uint8_t *) eccline->blob + eccsect * sector_size;
			if (len != sector_size)
				memset(dst, 0, sector_size);
			memcpy(dst, src, len);

			/* note that its finished for this extent */
			eccline->bufs[eccsect] = NULL;
			nestiobuf_done(buf, len, 0);

			bpos += sector_size;
			eccsect++;
			sectornr++;
			buf_len -= len;
		}
		udf_puteccline(eccline);
		return;

	}

	/* sequential writing */
	KASSERT(queue == UDF_SHED_SEQWRITING);
	DPRINTF(SHEDULE, ("\nudf_queuebuf_rmw SEQWRITE %p : sector XXXX "
		"type %d, b_resid %d, b_bcount %d, b_bufsize %d\n",
		buf, buf->b_udf_c_type, buf->b_resid, buf->b_bcount,
		buf->b_bufsize));
	/*
	 * Buffers should not have been allocated to disc addresses yet on
	 * this queue. Note that a buffer can get multiple extents allocated.
	 * Note that it *looks* like the normal writing but its different in
	 * the details.
	 *
	 * lmapping contains lb_num relative to base partition.
	 *
	 * XXX should we try to claim/organize the allocated memory to
	 * block-aligned pieces?
	 */
	mutex_enter(&priv->seqwrite_mutex);

	lmapping    = ump->la_lmapping;
	node_ad_cpy = ump->la_node_ad_cpy;

	/* logically allocate buf and map it in the file */
	udf_late_allocate_buf(ump, buf, lmapping, node_ad_cpy, &vpart_num);

	/* if we have FIDs, fixup using the new allocation table */
	if (buf->b_udf_c_type == UDF_C_FIDS) {
		buf_len = buf->b_bcount;
		bpos = 0;
		lmappos = lmapping;
		while (buf_len) {
			sectornr = *lmappos++;
			len = MIN(buf_len, sector_size);
			fidblk = (uint8_t *) buf->b_data + bpos;
			udf_fixup_fid_block(fidblk, sector_size,
				0, len, sectornr);
			bpos += len;
			buf_len -= len;
		}
	}
	if (buf->b_udf_c_type == UDF_C_METADATA_SBM) {
		if (buf->b_lblkno == 0) {
			/* update the tag location inside */
			tag = (struct desc_tag *) buf->b_data;
			tag->tag_loc = udf_rw32(*lmapping);
			udf_validate_tag_and_crc_sums(buf->b_data);
		}
	}
	udf_fixup_node_internals(ump, buf->b_data, buf->b_udf_c_type);

	/*
	 * Translate new mappings in lmapping to pmappings.
	 * pmapping to contain lb_nums as used for disc adressing.
	 */
	pmapping = ump->la_pmapping;
	sectors  = (buf->b_bcount + sector_size -1) / sector_size;
	udf_translate_vtop_list(ump, sectors, vpart_num, lmapping, pmapping);

	/* copy parts into the bufs and set for writing */
	pmappos = pmapping;
	buf_len = buf->b_bcount;
	sectornr = *pmappos++;
	eccline = udf_geteccline(ump, sectornr, ECC_SEQWRITING);
	eccsect = sectornr - eccline->start_sector;
	bpos = 0;
	while (buf_len) {
		len = MIN(buf_len, sector_size);
		eccsect = sectornr - eccline->start_sector;
		if ((eccsect < 0) || (eccsect >= ump->packet_size)) {
			eccline->flags |= ECC_SEQWRITING;
			udf_puteccline(eccline);
			eccline = udf_geteccline(ump, sectornr, ECC_SEQWRITING);
			eccsect = sectornr - eccline->start_sector;
		}
		bit = (uint64_t) 1 << eccsect;
		KASSERT((eccline->readin & bit) == 0);
		eccline->present |= bit;
		eccline->dirty   |= bit;
		eccline->bufs[eccsect] = NULL;

		src = (uint8_t *) buf->b_data + bpos;
		dst = (uint8_t *)
			eccline->blob + eccsect * sector_size;
		if (len != sector_size)
			memset(dst, 0, sector_size);
		memcpy(dst, src, len);

		/* note that its finished for this extent */
		nestiobuf_done(buf, len, 0);

		bpos += sector_size;
		sectornr = *pmappos++;
		buf_len -= len;
	}
	eccline->flags |= ECC_SEQWRITING;
	udf_puteccline(eccline);
	mutex_exit(&priv->seqwrite_mutex);
}

/* --------------------------------------------------------------------- */

static void 
udf_shedule_read_callback(struct buf *buf)
{
	struct udf_eccline *eccline = BTOE(buf);
	struct udf_mount *ump = eccline->ump;
	uint64_t bit;
	uint8_t *src, *dst;
	int sector_size = ump->discinfo.sector_size;
	int error, i, len;

	DPRINTF(ECCLINE, ("read callback called on buf %p\n", buf));

	/* post process read action */
	KASSERT(eccline->flags & ECC_LOCKED);
	error = buf->b_error;
	for (i = 0; i < ump->packet_size; i++) {
		bit = (uint64_t) 1 << i;
		src = (uint8_t *) buf->b_data +   i * sector_size;
		dst = (uint8_t *) eccline->blob + i * sector_size;
		if (eccline->present & bit)
			continue;
		eccline->present |= bit;
		if (error)
			eccline->error |= bit;
		if (eccline->bufs[i]) {
			dst = (uint8_t *) eccline->bufs[i]->b_data +
				eccline->bufs_bpos[i];
			len = eccline->bufs_len[i];
			if (!error)
				memcpy(dst, src, len);
			nestiobuf_done(eccline->bufs[i], len, error);
			eccline->bufs[i] = NULL;
		}
	
	}
	KASSERT(buf->b_data == eccline->blob);
	KASSERT(eccline->present == ((uint64_t) 1 << ump->packet_size)-1);

	/*
	 * XXX TODO what to do on read errors? read in all sectors
	 * synchronously and allocate a sparable entry?
	 */

	udf_puteccline(eccline);
	DPRINTF(ECCLINE, ("read callback finished\n"));
}


static void
udf_shedule_write_callback(struct buf *buf)
{
	struct udf_eccline *eccline = BTOE(buf);
	struct udf_mount *ump = eccline->ump;
	uint64_t bit;
	int error, i;

	DPRINTF(ECCLINE, ("write callback called on buf %p\n", buf));

	/* post process write action */
	KASSERT(eccline->flags & ECC_LOCKED);
	error = buf->b_error;
	for (i = 0; i < ump->packet_size; i++) {
		bit = (uint64_t) 1 << i;
		if ((eccline->dirty & bit) == 0)
			continue;
		if (error) {
			eccline->error |= bit;
		} else {
			eccline->dirty &= ~bit;
		}

		KASSERT(eccline->bufs[i] == 0);
	}
	KASSERT(eccline->dirty == 0);
	KASSERT(error == 0);

	/*
	 * XXX TODO on write errors allocate a sparable entry and reissue
	 */

	udf_puteccline(eccline);
	DPRINTF(ECCLINE, ("write callback finished\n"));
}


static void
udf_issue_eccline(struct udf_eccline *eccline, int queued_on)
{
	struct udf_mount *ump = eccline->ump;
	struct strat_private *priv = PRIV(ump);
	struct buf *buf, *nestbuf;
	uint64_t bit, allbits = ((uint64_t) 1 << ump->packet_size)-1;
	uint32_t start;
	int sector_size = ump->discinfo.sector_size;
	int blks = sector_size / DEV_BSIZE;
	int i;

	KASSERT(eccline->flags & ECC_LOCKED);

	if (queued_on == UDF_SHED_READING) {
		DPRINTF(SHEDULE, ("udf_issue_eccline reading : "));
		/* read all bits that are not yet present */
		eccline->readin = (~eccline->present) & allbits;
		KASSERT(eccline->readin);
		start = eccline->start_sector;
		buf = eccline->buf;
		buf->b_flags    = B_READ | B_ASYNC;
		SET(buf->b_cflags, BC_BUSY);	/* mark buffer busy */
		buf->b_oflags   = 0;
		buf->b_iodone   = udf_shedule_read_callback;
		buf->b_data     = eccline->blob;
		buf->b_bcount   = ump->packet_size * sector_size;
		buf->b_resid    = buf->b_bcount;
		buf->b_bufsize  = buf->b_bcount;
		buf->b_private  = eccline;
		BIO_SETPRIO(buf, BPRIO_DEFAULT);
		buf->b_lblkno   = buf->b_blkno = buf->b_rawblkno = start * blks;
		buf->b_proc     = NULL;

		if (eccline->present != 0) {
			for (i = 0; i < ump->packet_size; i++) {
				bit = (uint64_t) 1 << i;
				if (eccline->present & bit) {
					nestiobuf_done(buf, sector_size, 0);
					continue;
				}
				nestbuf = getiobuf(NULL, true);
				nestiobuf_setup(buf, nestbuf, i * sector_size,
					sector_size);
				/* adjust blocknumber to read */
				nestbuf->b_blkno = buf->b_blkno + i*blks;
				nestbuf->b_rawblkno = buf->b_rawblkno + i*blks;

				DPRINTF(SHEDULE, ("sector %d ", start + i));

				/* mutex dance since it could lock */
				mutex_exit(&priv->discstrat_mutex);
					/* call asynchronous */
					VOP_STRATEGY(ump->devvp, nestbuf);
				mutex_enter(&priv->discstrat_mutex);
			}
			DPRINTF(SHEDULE, ("\n"));
			return;
		}
	} else {
		/* write or seqwrite */
		DPRINTF(SHEDULE, ("udf_issue_eccline writing or seqwriting : "));
		DPRINTF(SHEDULE, ("\n\tpresent %"PRIx64", readin %"PRIx64", "
			"dirty %"PRIx64"\n\t", eccline->present, eccline->readin,
			eccline->dirty));
		KASSERT(eccline->present == allbits);

		start = eccline->start_sector;
		buf = eccline->buf;
		buf->b_flags    = B_WRITE | B_ASYNC;
		SET(buf->b_cflags, BC_BUSY);	/* mark buffer busy */
		buf->b_oflags   = 0;
		buf->b_iodone   = udf_shedule_write_callback;
		buf->b_data     = eccline->blob;
		buf->b_bcount   = ump->packet_size * sector_size;
		buf->b_resid    = buf->b_bcount;
		buf->b_bufsize  = buf->b_bcount;
		buf->b_private  = eccline;
		BIO_SETPRIO(buf, BPRIO_DEFAULT);
		buf->b_lblkno   = buf->b_blkno = buf->b_rawblkno = start * blks;
		buf->b_proc     = NULL;
	}

	/* mutex dance since it could lock */
	mutex_exit(&priv->discstrat_mutex);
		/* call asynchronous */
		DPRINTF(SHEDULE, ("sector %d for %d\n",
			start, ump->packet_size));
		VOP_STRATEGY(ump->devvp, buf);
	mutex_enter(&priv->discstrat_mutex);
}


static void
udf_discstrat_thread(void *arg)
{
	struct udf_mount *ump = (struct udf_mount *) arg;
	struct strat_private *priv = PRIV(ump);
	struct udf_eccline *eccline;
	struct timespec now, *last;
	uint64_t allbits = ((uint64_t) 1 << ump->packet_size)-1;
	int new_queue, wait, work;

	work = 1;
	priv->thread_running = 1;
	mutex_enter(&priv->discstrat_mutex);
	priv->num_floating = 0;
	while (priv->run_thread || work || priv->num_floating) {
		/* get our time */
		vfs_timestamp(&now);

		/* maintenance: handle eccline state machine */
		for(;;) {
			/* only peek at it */
			eccline = udf_peek_eccline(priv, UDF_SHED_WAITING);
			if (eccline == NULL)
				break;

			/* if not reading, wait until the time has come */
			if ((priv->cur_queue != UDF_SHED_READING) &&
				(eccline->wait_time.tv_sec - now.tv_sec > 0)) {
					UDF_UNLOCK_ECCLINE(eccline);
					/* all others are later, so break off */
					break;
			}

			/* release */
			UDF_UNLOCK_ECCLINE(eccline);

			/* do get it */
			eccline = udf_pop_eccline(priv, UDF_SHED_WAITING);

			/* requeue according to state */
			new_queue = UDF_SHED_FREE;	/* unlikely */
			if (eccline->refcnt > 0)
				new_queue = UDF_SHED_IDLE;
			if (eccline->flags & ECC_WANTED)
				new_queue = UDF_SHED_IDLE;
			if (eccline->readin)
				new_queue = UDF_SHED_READING;
			if (eccline->dirty) {
				new_queue = UDF_SHED_READING;
				if (eccline->present == allbits) {
					new_queue = UDF_SHED_WRITING;
					if (eccline->flags & ECC_SEQWRITING)
						new_queue = UDF_SHED_SEQWRITING;
				}
			}
			udf_push_eccline(eccline, new_queue);
		}

		/* maintenance: free excess ecclines */
		while (priv->num_queued[UDF_SHED_FREE] > UDF_ECCLINE_MAXFREE) {
			eccline = udf_pop_eccline(priv, UDF_SHED_FREE);
			KASSERT(eccline);
			KASSERT(eccline->refcnt == 0);
			if (eccline->flags & ECC_WANTED) {
				/* we won the race, but we dont want to win */
				DPRINTF(ECCLINE, ("Tried removing, pushed back to free list\n"));
				udf_push_eccline(eccline, UDF_SHED_IDLE);
			} else {
				DPRINTF(ECCLINE, ("Removing entry from free list\n"));
				udf_dispose_eccline(eccline);
			}
		}

		/* process the current selected queue */
		/* get our time */
		vfs_timestamp(&now);
		last = &priv->last_queued[priv->cur_queue];

		/* get our line */
		eccline = udf_pop_eccline(priv, priv->cur_queue);
		if (eccline) {
			wait = 0;
			new_queue = priv->cur_queue;
			DPRINTF(ECCLINE, ("UDF_ISSUE_ECCLINE\n"));

			udf_issue_eccline(eccline, priv->cur_queue);
		} else {
			/* don't switch too quickly */
			if (now.tv_sec - last->tv_sec < 2) {
				/* wait some time */
				cv_timedwait(&priv->discstrat_cv,
					&priv->discstrat_mutex, hz);
				/* we assume there is work to be done */
				work = 1;
				continue;
			}

			/* XXX select on queue lengths ? */
			wait = 1;
			/* check if we can/should switch */
			new_queue = priv->cur_queue;
			if (bufq_peek(priv->queues[UDF_SHED_READING]))
				new_queue = UDF_SHED_READING;
			if (bufq_peek(priv->queues[UDF_SHED_WRITING]))
				new_queue = UDF_SHED_WRITING;
			if (bufq_peek(priv->queues[UDF_SHED_SEQWRITING]))
				new_queue = UDF_SHED_SEQWRITING;
		}

		/* give room */
		mutex_exit(&priv->discstrat_mutex);

		if (new_queue != priv->cur_queue) {
			wait = 0;
			DPRINTF(SHEDULE, ("switching from %d to %d\n",
				priv->cur_queue, new_queue));
			priv->cur_queue = new_queue;
		}
		mutex_enter(&priv->discstrat_mutex);

		/* wait for more if needed */
		if (wait)
			cv_timedwait(&priv->discstrat_cv,
				&priv->discstrat_mutex, hz/4);	/* /8 */

		work  = (bufq_peek(priv->queues[UDF_SHED_WAITING]) != NULL);
		work |= (bufq_peek(priv->queues[UDF_SHED_READING]) != NULL);
		work |= (bufq_peek(priv->queues[UDF_SHED_WRITING]) != NULL);
		work |= (bufq_peek(priv->queues[UDF_SHED_SEQWRITING]) != NULL);

		DPRINTF(PARANOIA, ("work : (%d, %d, %d) -> work %d, float %d\n",
			(bufq_peek(priv->queues[UDF_SHED_READING]) != NULL),
			(bufq_peek(priv->queues[UDF_SHED_WRITING]) != NULL),
			(bufq_peek(priv->queues[UDF_SHED_SEQWRITING]) != NULL),
			work, priv->num_floating));
	}

	mutex_exit(&priv->discstrat_mutex);

	/* tear down remaining ecclines */
	mutex_enter(&priv->discstrat_mutex);
	KASSERT(bufq_peek(priv->queues[UDF_SHED_WAITING]) == NULL);
	KASSERT(bufq_peek(priv->queues[UDF_SHED_IDLE]) == NULL);
	KASSERT(bufq_peek(priv->queues[UDF_SHED_READING]) == NULL);
	KASSERT(bufq_peek(priv->queues[UDF_SHED_WRITING]) == NULL);
	KASSERT(bufq_peek(priv->queues[UDF_SHED_SEQWRITING]) == NULL);

	KASSERT(priv->num_queued[UDF_SHED_WAITING] == 0);
	KASSERT(priv->num_queued[UDF_SHED_IDLE] == 0);
	KASSERT(priv->num_queued[UDF_SHED_READING] == 0);
	KASSERT(priv->num_queued[UDF_SHED_WRITING] == 0);
	KASSERT(priv->num_queued[UDF_SHED_SEQWRITING] == 0);

	eccline = udf_pop_eccline(priv, UDF_SHED_FREE);
	while (eccline) {
		udf_dispose_eccline(eccline);
		eccline = udf_pop_eccline(priv, UDF_SHED_FREE);
	}
	KASSERT(priv->num_queued[UDF_SHED_FREE] == 0);
	mutex_exit(&priv->discstrat_mutex);

	priv->thread_running  = 0;
	priv->thread_finished = 1;
	wakeup(&priv->run_thread);
	kthread_exit(0);
	/* not reached */
}

/* --------------------------------------------------------------------- */

/*
 * Buffer memory pool allocator.
 */

static void *
ecclinepool_page_alloc(struct pool *pp, int flags)
{
        return (void *)uvm_km_alloc(kernel_map,
            MAXBSIZE, MAXBSIZE,
            ((flags & PR_WAITOK) ? 0 : UVM_KMF_NOWAIT | UVM_KMF_TRYLOCK)
	    	| UVM_KMF_WIRED /* UVM_KMF_PAGABLE? */);
}

static void
ecclinepool_page_free(struct pool *pp, void *v)
{
        uvm_km_free(kernel_map, (vaddr_t)v, MAXBSIZE, UVM_KMF_WIRED);
}

static struct pool_allocator ecclinepool_allocator = {
        .pa_alloc = ecclinepool_page_alloc,
        .pa_free  = ecclinepool_page_free,
        .pa_pagesz = MAXBSIZE,
};


static void
udf_discstrat_init_rmw(struct udf_strat_args *args)
{
	struct udf_mount *ump = args->ump;
	struct strat_private *priv = PRIV(ump);
	uint32_t lb_size, blobsize, hashline;
	int i;

	KASSERT(ump);
	KASSERT(ump->logical_vol);
	KASSERT(priv == NULL);

	lb_size = udf_rw32(ump->logical_vol->lb_size);
	blobsize = ump->packet_size * lb_size;
	KASSERT(lb_size > 0);
	KASSERT(ump->packet_size <= 64);

	/* initialise our memory space */
	ump->strategy_private = malloc(sizeof(struct strat_private),
		M_UDFTEMP, M_WAITOK);
	priv = ump->strategy_private;
	memset(priv, 0 , sizeof(struct strat_private));

	/* initialise locks */
	cv_init(&priv->discstrat_cv, "udfstrat");
	mutex_init(&priv->discstrat_mutex, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&priv->seqwrite_mutex, MUTEX_DEFAULT, IPL_NONE);

	/* initialise struct eccline pool */
	pool_init(&priv->eccline_pool, sizeof(struct udf_eccline),
		0, 0, 0, "udf_eccline_pool", NULL, IPL_NONE);

	/* initialise eccline blob pool */
        ecclinepool_allocator.pa_pagesz = blobsize;
	pool_init(&priv->ecclineblob_pool, blobsize, 
		0, 0, 0, "udf_eccline_blob", &ecclinepool_allocator, IPL_NONE);

	/* initialise main queues */
	for (i = 0; i < UDF_SHED_MAX; i++) {
		priv->num_queued[i] = 0;
		vfs_timestamp(&priv->last_queued[i]);
	}
	bufq_alloc(&priv->queues[UDF_SHED_WAITING], "fcfs",
		BUFQ_SORT_RAWBLOCK);
	bufq_alloc(&priv->queues[UDF_SHED_READING], "disksort",
		BUFQ_SORT_RAWBLOCK);
	bufq_alloc(&priv->queues[UDF_SHED_WRITING], "disksort",
		BUFQ_SORT_RAWBLOCK);
	bufq_alloc(&priv->queues[UDF_SHED_SEQWRITING], "disksort", 0);

	/* initialise administrative queues */
	bufq_alloc(&priv->queues[UDF_SHED_IDLE], "fcfs", 0);
	bufq_alloc(&priv->queues[UDF_SHED_FREE], "fcfs", 0);

	for (hashline = 0; hashline < UDF_ECCBUF_HASHSIZE; hashline++) {
		LIST_INIT(&priv->eccline_hash[hashline]);
	}

	/* create our disk strategy thread */
	priv->cur_queue = UDF_SHED_READING;
	priv->thread_finished = 0;
	priv->thread_running  = 0;
	priv->run_thread      = 1;
	if (kthread_create(PRI_NONE, 0 /* KTHREAD_MPSAFE*/, NULL /* cpu_info*/,
		udf_discstrat_thread, ump, &priv->queue_lwp,
		"%s", "udf_rw")) {
		panic("fork udf_rw");
	}

	/* wait for thread to spin up */
	while (!priv->thread_running) {
		tsleep(&priv->thread_running, PRIBIO+1, "udfshedstart", hz);
	}
}


static void
udf_discstrat_finish_rmw(struct udf_strat_args *args)
{
	struct udf_mount *ump = args->ump;
	struct strat_private *priv = PRIV(ump);

	if (ump == NULL)
		return;

	/* stop our sheduling thread */
	KASSERT(priv->run_thread == 1);
	priv->run_thread = 0;
	wakeup(priv->queue_lwp);
	while (!priv->thread_finished) {
		tsleep(&priv->run_thread, PRIBIO + 1, "udfshedfin", hz);
	}
	/* kthread should be finished now */

	/* cleanup our pools */
	pool_destroy(&priv->eccline_pool);
	pool_destroy(&priv->ecclineblob_pool);

	cv_destroy(&priv->discstrat_cv);
	mutex_destroy(&priv->discstrat_mutex);
	mutex_destroy(&priv->seqwrite_mutex);

	/* free our private space */
	free(ump->strategy_private, M_UDFTEMP);
	ump->strategy_private = NULL;
}

/* --------------------------------------------------------------------- */

struct udf_strategy udf_strat_rmw =
{
	udf_create_nodedscr_rmw,
	udf_free_nodedscr_rmw,
	udf_read_nodedscr_rmw,
	udf_write_nodedscr_rmw,
	udf_queuebuf_rmw,
	udf_discstrat_init_rmw,
	udf_discstrat_finish_rmw
};


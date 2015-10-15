/* $NetBSD: udf_strat_direct.c,v 1.13 2015/10/06 08:57:34 hannken Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: udf_strat_direct.c,v 1.13 2015/10/06 08:57:34 hannken Exp $");
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


#define VTOI(vnode) ((struct udf_node *) vnode->v_data)
#define PRIV(ump) ((struct strat_private *) ump->strategy_private)

/* --------------------------------------------------------------------- */

/* BUFQ's */
#define UDF_SHED_MAX 3

#define UDF_SHED_READING	0
#define UDF_SHED_WRITING	1
#define UDF_SHED_SEQWRITING	2


struct strat_private {
	struct pool		desc_pool;	 /* node descriptors */
};

/* --------------------------------------------------------------------- */

static void
udf_wr_nodedscr_callback(struct buf *buf)
{
	struct udf_node *udf_node;

	KASSERT(buf);
	KASSERT(buf->b_data);

	/* called when write action is done */
	DPRINTF(WRITE, ("udf_wr_nodedscr_callback(): node written out\n"));

	udf_node = VTOI(buf->b_vp);
	if (udf_node == NULL) {
		putiobuf(buf);
		printf("udf_wr_node_callback: NULL node?\n");
		return;
	}

	/* XXX right flags to mark dirty again on error? */
	if (buf->b_error) {
		/* write error on `defect free' media??? how to solve? */
		/* XXX lookup UDF standard for unallocatable space */
		udf_node->i_flags |= IN_MODIFIED | IN_ACCESSED;
	}

	/* decrement outstanding_nodedscr */
	KASSERT(udf_node->outstanding_nodedscr >= 1);
	udf_node->outstanding_nodedscr--;
	if (udf_node->outstanding_nodedscr == 0) {
		/* unlock the node */
		UDF_UNLOCK_NODE(udf_node, 0);
		wakeup(&udf_node->outstanding_nodedscr);
	}

	putiobuf(buf);
}

/* --------------------------------------------------------------------- */

static int
udf_getblank_nodedscr_direct(struct udf_strat_args *args)
{
	union dscrptr   **dscrptr = &args->dscr;
	struct udf_mount *ump = args->ump;
	struct strat_private *priv = PRIV(ump);
	uint32_t lb_size;

	lb_size = udf_rw32(ump->logical_vol->lb_size);
	*dscrptr = pool_get(&priv->desc_pool, PR_WAITOK);
	memset(*dscrptr, 0, lb_size);

	return 0;
}


static void
udf_free_nodedscr_direct(struct udf_strat_args *args)
{
	union dscrptr    *dscr = args->dscr;
	struct udf_mount *ump  = args->ump;
	struct strat_private *priv = PRIV(ump);

	pool_put(&priv->desc_pool, dscr);
}


static int
udf_read_nodedscr_direct(struct udf_strat_args *args)
{
	union dscrptr   **dscrptr = &args->dscr;
	union dscrptr    *tmpdscr;
	struct udf_mount *ump = args->ump;
	struct long_ad   *icb = args->icb;
	struct strat_private *priv = PRIV(ump);
	uint32_t lb_size;
	uint32_t sector, dummy;
	int error;

	lb_size = udf_rw32(ump->logical_vol->lb_size);

	error = udf_translate_vtop(ump, icb, &sector, &dummy);
	if (error)
		return error;

	/* try to read in fe/efe */
	error = udf_read_phys_dscr(ump, sector, M_UDFTEMP, &tmpdscr);
	if (error)
		return error;

	*dscrptr = pool_get(&priv->desc_pool, PR_WAITOK);
	memcpy(*dscrptr, tmpdscr, lb_size);
	free(tmpdscr, M_UDFTEMP);

	return 0;
}


static int
udf_write_nodedscr_direct(struct udf_strat_args *args)
{
	struct udf_mount *ump      = args->ump;
	struct udf_node  *udf_node = args->udf_node;
	union dscrptr    *dscr     = args->dscr;
	struct long_ad   *icb      = args->icb;
	int               waitfor  = args->waitfor;
	uint32_t logsector, sector, dummy;
	int error, vpart __diagused;

	/*
	 * we have to decide if we write it out sequential or at its fixed 
	 * position by examining the partition its (to be) written on.
	 */
	vpart     = udf_rw16(udf_node->loc.loc.part_num);
	logsector = udf_rw32(icb->loc.lb_num);
	KASSERT(ump->vtop_tp[vpart] != UDF_VTOP_TYPE_VIRT);

	sector = 0;
	error  = udf_translate_vtop(ump, icb, &sector, &dummy);
	if (error)
		goto out;

	if (waitfor) {
		DPRINTF(WRITE, ("udf_write_nodedscr: sync write\n"));

		error = udf_write_phys_dscr_sync(ump, udf_node, UDF_C_NODE,
			dscr, sector, logsector);
	} else {
		DPRINTF(WRITE, ("udf_write_nodedscr: no wait, async write\n"));

		error = udf_write_phys_dscr_async(ump, udf_node, UDF_C_NODE,
			dscr, sector, logsector, udf_wr_nodedscr_callback);
		/* will be UNLOCKED in call back */
		return error;
	}
out:
	udf_node->outstanding_nodedscr--;
	if (udf_node->outstanding_nodedscr == 0) {
		UDF_UNLOCK_NODE(udf_node, 0);
		wakeup(&udf_node->outstanding_nodedscr);
	}

	return error;
}

/* --------------------------------------------------------------------- */

static void
udf_queue_buf_direct(struct udf_strat_args *args)
{
	struct udf_mount *ump = args->ump;
	struct buf *buf = args->nestbuf;
	struct buf *nestbuf;
	struct desc_tag *tag;
	struct long_ad *node_ad_cpy;
	uint64_t *lmapping, *pmapping, *lmappos, run_start;
	uint32_t sectornr;
	uint32_t buf_offset, rbuflen, bpos;
	uint16_t vpart_num;
	uint8_t *fidblk;
	off_t rblk;
	int sector_size = ump->discinfo.sector_size;
	int len, buf_len, sector, sectors, run_length;
	int blks = sector_size / DEV_BSIZE;
	int what, class __diagused, queue;

	KASSERT(ump);
	KASSERT(buf);
	KASSERT(buf->b_iodone == nestiobuf_iodone);

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

	/* use disc sheduler */
	class = ump->discinfo.mmc_class;
	KASSERT((class == MMC_CLASS_UNKN) || (class == MMC_CLASS_DISC) ||
		(ump->discinfo.mmc_cur & MMC_CAP_HW_DEFECTFREE) ||
		(ump->vfs_mountp->mnt_flag & MNT_RDONLY));

#ifndef UDF_DEBUG
	__USE(blks);
#endif
	if (queue == UDF_SHED_READING) {
		DPRINTF(SHEDULE, ("\nudf_issue_buf READ %p : sector %d type %d,"
			"b_resid %d, b_bcount %d, b_bufsize %d\n",
			buf, (uint32_t) buf->b_blkno / blks, buf->b_udf_c_type,
			buf->b_resid, buf->b_bcount, buf->b_bufsize));
		VOP_STRATEGY(ump->devvp, buf);
		return;
	}


	if (queue == UDF_SHED_WRITING) {
		DPRINTF(SHEDULE, ("\nudf_issue_buf WRITE %p : sector %d "
			"type %d, b_resid %d, b_bcount %d, b_bufsize %d\n",
			buf, (uint32_t) buf->b_blkno / blks, buf->b_udf_c_type,
			buf->b_resid, buf->b_bcount, buf->b_bufsize));
		KASSERT(buf->b_udf_c_type == UDF_C_DSCR || 
			buf->b_udf_c_type == UDF_C_ABSOLUTE ||
			buf->b_udf_c_type == UDF_C_NODE);
		 udf_fixup_node_internals(ump, buf->b_data, buf->b_udf_c_type);
		VOP_STRATEGY(ump->devvp, buf);
		return;
	}

	/* UDF_SHED_SEQWRITING */
	KASSERT(queue == UDF_SHED_SEQWRITING);
	DPRINTF(SHEDULE, ("\nudf_issue_buf SEQWRITE %p : sector XXXX "
		"type %d, b_resid %d, b_bcount %d, b_bufsize %d\n",
		buf, buf->b_udf_c_type, buf->b_resid, buf->b_bcount,
		buf->b_bufsize));

	/*
	 * Buffers should not have been allocated to disc addresses yet on
	 * this queue. Note that a buffer can get multiple extents allocated.
	 *
	 * lmapping contains lb_num relative to base partition.
	 */
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
	 * Translate new mappings in lmapping to pmappings and try to
	 * conglomerate extents to reduce the number of writes.
	 *
	 * pmapping to contain lb_nums as used for disc adressing.
	 */
	pmapping = ump->la_pmapping;
	sectors  = (buf->b_bcount + sector_size -1) / sector_size;
	udf_translate_vtop_list(ump, sectors, vpart_num, lmapping, pmapping);

	for (sector = 0; sector < sectors; sector++) {
		buf_offset = sector * sector_size;
		DPRINTF(WRITE, ("\tprocessing rel sector %d\n", sector));

		DPRINTF(WRITE, ("\tissue write sector %"PRIu64"\n",
			pmapping[sector]));

		run_start  = pmapping[sector];
		run_length = 1;
		while (sector < sectors-1) {
			if (pmapping[sector+1] != pmapping[sector]+1)
				break;
			run_length++;
			sector++;
		}

		/* nest an iobuf for the extent */
		rbuflen = run_length *  sector_size;
		rblk    = run_start  * (sector_size/DEV_BSIZE);

		nestbuf = getiobuf(NULL, true);
		nestiobuf_setup(buf, nestbuf, buf_offset, rbuflen);
		/* nestbuf is B_ASYNC */

		/* identify this nestbuf */
		nestbuf->b_lblkno   = sector;
		assert(nestbuf->b_vp == buf->b_vp);

		/* CD shedules on raw blkno */
		nestbuf->b_blkno      = rblk;
		nestbuf->b_proc       = NULL;
		nestbuf->b_rawblkno   = rblk;
		nestbuf->b_udf_c_type = UDF_C_PROCESSED;

		VOP_STRATEGY(ump->devvp, nestbuf);
	}
}


static void
udf_discstrat_init_direct(struct udf_strat_args *args)
{
	struct udf_mount  *ump = args->ump;
	struct strat_private *priv = PRIV(ump);
	uint32_t lb_size;

	KASSERT(priv == NULL);
	ump->strategy_private = malloc(sizeof(struct strat_private),
		M_UDFTEMP, M_WAITOK);
	priv = ump->strategy_private;
	memset(priv, 0 , sizeof(struct strat_private));

	/*
	 * Initialise pool for descriptors associated with nodes. This is done
	 * in lb_size units though currently lb_size is dictated to be
	 * sector_size.
	 */
	memset(&priv->desc_pool, 0, sizeof(struct pool));

	lb_size = udf_rw32(ump->logical_vol->lb_size);
	pool_init(&priv->desc_pool, lb_size, 0, 0, 0, "udf_desc_pool", NULL,
	    IPL_NONE);
}


static void
udf_discstrat_finish_direct(struct udf_strat_args *args)
{
	struct udf_mount *ump = args->ump;
	struct strat_private *priv = PRIV(ump);

	/* destroy our pool */
	pool_destroy(&priv->desc_pool);

	/* free our private space */
	free(ump->strategy_private, M_UDFTEMP);
	ump->strategy_private = NULL;
}

/* --------------------------------------------------------------------- */

struct udf_strategy udf_strat_direct =
{
	udf_getblank_nodedscr_direct,
	udf_free_nodedscr_direct,
	udf_read_nodedscr_direct,
	udf_write_nodedscr_direct,
	udf_queue_buf_direct,
	udf_discstrat_init_direct,
	udf_discstrat_finish_direct
};


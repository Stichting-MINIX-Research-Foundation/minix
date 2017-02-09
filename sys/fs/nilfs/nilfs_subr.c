/* $NetBSD: nilfs_subr.c,v 1.14 2015/03/29 14:12:28 riastradh Exp $ */

/*
 * Copyright (c) 2008, 2009 Reinoud Zandijk
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
__KERNEL_RCSID(0, "$NetBSD: nilfs_subr.c,v 1.14 2015/03/29 14:12:28 riastradh Exp $");
#endif /* not lint */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/resourcevar.h>	/* defines plimit structure in proc struct */
#include <sys/kernel.h>
#include <sys/file.h>		/* define FWRITE ... */
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/signalvar.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/lockf.h>
#include <sys/kauth.h>
#include <sys/dirhash.h>

#include <miscfs/genfs/genfs.h>
#include <uvm/uvm_extern.h>

#include <fs/nilfs/nilfs_mount.h>
#include "nilfs.h"
#include "nilfs_subr.h"
#include "nilfs_bswap.h"


#define VTOI(vnode) ((struct nilfs_node *) (vnode)->v_data)

/* forwards */
static int nilfs_btree_lookup(struct nilfs_node *node, uint64_t lblocknr,
	uint64_t *vblocknr);

/* basic calculators */
uint64_t nilfs_get_segnum_of_block(struct nilfs_device *nilfsdev,
	uint64_t blocknr)
{
	return blocknr / nilfs_rw32(nilfsdev->super.s_blocks_per_segment);
}


void
nilfs_get_segment_range(struct nilfs_device *nilfsdev, uint64_t segnum,
        uint64_t *seg_start, uint64_t *seg_end)
{
        uint64_t blks_per_seg;

        blks_per_seg = nilfs_rw64(nilfsdev->super.s_blocks_per_segment);
        *seg_start = blks_per_seg * segnum;
        *seg_end   = *seg_start + blks_per_seg -1;
        if (segnum == 0)
                *seg_start = nilfs_rw64(nilfsdev->super.s_first_data_block);
}


void nilfs_calc_mdt_consts(struct nilfs_device *nilfsdev,
	struct nilfs_mdt *mdt, int entry_size)
{
	uint32_t blocksize = nilfsdev->blocksize;

	mdt->entries_per_group = blocksize * 8;	   /* bits in sector */
	mdt->entries_per_block = blocksize / entry_size;

	mdt->blocks_per_group  =
		(mdt->entries_per_group -1) / mdt->entries_per_block + 1 + 1;
	mdt->groups_per_desc_block =
		blocksize / sizeof(struct nilfs_block_group_desc);
	mdt->blocks_per_desc_block =
		mdt->groups_per_desc_block * mdt->blocks_per_group + 1;
}


/* from NetBSD's src/sys/net/if_ethersubr.c */
uint32_t
crc32_le(uint32_t crc, const uint8_t *buf, size_t len)
{
        static const uint32_t crctab[] = {
                0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
                0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
                0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
                0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
        };
        size_t i;

        for (i = 0; i < len; i++) {
                crc ^= buf[i];
                crc = (crc >> 4) ^ crctab[crc & 0xf];
                crc = (crc >> 4) ^ crctab[crc & 0xf];
        }

        return (crc);
}


/* dev reading */
static int
nilfs_dev_bread(struct nilfs_device *nilfsdev, uint64_t blocknr,
	int flags, struct buf **bpp)
{
	int blk2dev = nilfsdev->blocksize / DEV_BSIZE;

	return bread(nilfsdev->devvp, blocknr * blk2dev, nilfsdev->blocksize,
		0, bpp);
}


/* read on a node */
int
nilfs_bread(struct nilfs_node *node, uint64_t blocknr,
	int flags, struct buf **bpp)
{
	struct nilfs_device *nilfsdev = node->nilfsdev;
	uint64_t vblocknr, pblockno;
	int error;

	error = nilfs_btree_lookup(node, blocknr, &vblocknr);
	if (error)
		return error;

	/* Read special files through devvp as they have no vnode attached. */
	if (node->ino < NILFS_USER_INO && node->ino != NILFS_ROOT_INO) {
		error = nilfs_nvtop(node, 1, &vblocknr, &pblockno);
		if (error)
			return error;
		return nilfs_dev_bread(nilfsdev, pblockno, flags, bpp);
	}

	return bread(node->vnode, vblocknr, node->nilfsdev->blocksize,
		flags, bpp);
}


/* segment-log reading */
int
nilfs_get_segment_log(struct nilfs_device *nilfsdev, uint64_t *blocknr,
	uint64_t *offset, struct buf **bpp, int len, void *blob)
{
	int blocksize = nilfsdev->blocksize;
	int error;

	KASSERT(len <= blocksize);

	if (*offset + len > blocksize) {
		*blocknr = *blocknr + 1;
		*offset = 0;
	}
	if (*offset == 0) {
		if (*bpp)
			brelse(*bpp, BC_AGE);
		/* read in block */
		error = nilfs_dev_bread(nilfsdev, *blocknr, 0, bpp);
		if (error)
			return error;
	}
	memcpy(blob, ((uint8_t *) (*bpp)->b_data) + *offset, len);
	*offset += len;

	return 0;
}

/* -------------------------------------------------------------------------- */

/* btree operations */

static int
nilfs_btree_lookup_level(struct nilfs_node *node, uint64_t lblocknr,
		uint64_t btree_vblknr, int level, uint64_t *vblocknr)
{
	struct nilfs_device *nilfsdev = node->nilfsdev;
	struct nilfs_btree_node *btree_hdr;
	struct buf *bp;
	uint64_t btree_blknr;
	uint64_t *dkeys, *dptrs, child_btree_blk;
	uint8_t  *pos;
	int i, error, selected;

	DPRINTF(TRANSLATE, ("nilfs_btree_lookup_level ino %"PRIu64", "
		"lblocknr %"PRIu64", btree_vblknr %"PRIu64", level %d\n",
		node->ino, lblocknr, btree_vblknr, level));

	/* translate btree_vblknr */
	error = nilfs_nvtop(node, 1, &btree_vblknr, &btree_blknr);
	if (error)
		return error;

	/* get our block */
	error = nilfs_dev_bread(nilfsdev, btree_blknr, 0, &bp);
	if (error) {
		return error;
	}

	btree_hdr = (struct nilfs_btree_node *) bp->b_data;
	pos =   (uint8_t *) bp->b_data +
		sizeof(struct nilfs_btree_node) +
		NILFS_BTREE_NODE_EXTRA_PAD_SIZE;
	dkeys = (uint64_t *) pos;
	dptrs = dkeys + NILFS_BTREE_NODE_NCHILDREN_MAX(nilfsdev->blocksize);

	assert((btree_hdr->bn_flags & NILFS_BTREE_NODE_ROOT) == 0);

	/* select matching child XXX could use binary search */
	selected = 0;
	for (i = 0; i < nilfs_rw16(btree_hdr->bn_nchildren); i++) {
		if (dkeys[i] > lblocknr)
			break;
		selected = i;
	}

	if (level == 1) {
		/* if found it mapped */
		if (dkeys[selected] == lblocknr)
			*vblocknr = dptrs[selected];
		brelse(bp, BC_AGE);
		return 0;
	}

	/* lookup in selected child */
	assert(dkeys[selected] <= lblocknr);
	child_btree_blk = dptrs[selected];
	brelse(bp, BC_AGE);

	return nilfs_btree_lookup_level(node, lblocknr,
			child_btree_blk, btree_hdr->bn_level-1, vblocknr);
}


/* internal function */
static int
nilfs_btree_lookup(struct nilfs_node *node, uint64_t lblocknr,
		uint64_t *vblocknr)
{
	struct nilfs_inode  *inode    = &node->inode;
	struct nilfs_btree_node  *btree_hdr;
	uint64_t *dkeys, *dptrs, *dtrans;
	int i, selected;
	int error;

	DPRINTF(TRANSLATE, ("nilfs_btree_lookup ino %"PRIu64", "
		"lblocknr %"PRIu64"\n", node->ino, lblocknr));

	btree_hdr  = (struct nilfs_btree_node *) &inode->i_bmap[0];
	dkeys  = &inode->i_bmap[1];
	dptrs  = dkeys + NILFS_BTREE_ROOT_NCHILDREN_MAX;
	dtrans = &inode->i_bmap[1];

	/* SMALL, direct lookup */
	*vblocknr = 0;
	if ((btree_hdr->bn_flags & NILFS_BMAP_LARGE) == 0) {
		if (lblocknr < NILFS_DIRECT_NBLOCKS) {
			*vblocknr = dtrans[lblocknr];
			return 0;
		}
		/* not mapped XXX could be considered error here */
		return 0;
	}

	/* LARGE, select matching child; XXX could use binary search */
	dtrans = NULL;
	error = 0;
	selected = 0;
	for (i = 0; i < nilfs_rw16(btree_hdr->bn_nchildren); i++) {
		if (dkeys[i] > lblocknr)
			break;
		selected = i;
	}

	/* if selected key > lblocknr, its not mapped */
	if (dkeys[selected] > lblocknr)
		return 0;

	/* overshooting? then not mapped */
	if (selected == nilfs_rw16(btree_hdr->bn_nchildren))
		return 0;

	/* level should be > 1 or otherwise it should be a direct one */
	assert(btree_hdr->bn_level > 1);

	/* lookup in selected child */
	assert(dkeys[selected] <= lblocknr);
	error = nilfs_btree_lookup_level(node, lblocknr, 
			dptrs[selected], btree_hdr->bn_level-1, vblocknr);

	return error;
}


/* node should be locked on entry to prevent btree changes (unlikely) */
int
nilfs_btree_nlookup(struct nilfs_node *node, uint64_t from, uint64_t blks,
		uint64_t *l2vmap)
{
	uint64_t lblocknr, *vblocknr;
	int i, error;

	/* TODO / OPTI multiple translations in one go possible */
	error = EINVAL;
	for (i = 0; i < blks; i++) {
		lblocknr  = from + i;
		vblocknr  = l2vmap + i;
		error = nilfs_btree_lookup(node, lblocknr, vblocknr);

		DPRINTF(TRANSLATE, ("btree_nlookup ino %"PRIu64", "
			"lblocknr %"PRIu64" -> %"PRIu64"\n",
			node->ino, lblocknr, *vblocknr));
		if (error)
			break;
	}

	return error;
}

/* --------------------------------------------------------------------- */

/* vtop operations */

/* translate index to a file block number and an entry */
void
nilfs_mdt_trans(struct nilfs_mdt *mdt, uint64_t index,
	uint64_t *blocknr, uint32_t *entry_in_block)
{
	uint64_t blknr;
	uint64_t group, group_offset, blocknr_in_group;
	uint64_t desc_block, desc_offset;

	/* calculate our offset in the file */
	group             = index / mdt->entries_per_group;
	group_offset      = index % mdt->entries_per_group;
	desc_block        = group / mdt->groups_per_desc_block;
	desc_offset       = group % mdt->groups_per_desc_block;
	blocknr_in_group  = group_offset / mdt->entries_per_block;

	/* to descgroup offset */
	blknr = 1 + desc_block * mdt->blocks_per_desc_block;

	/* to group offset */
	blknr += desc_offset * mdt->blocks_per_group;

	/* to actual file block */
	blknr += 1 + blocknr_in_group;

	*blocknr        = blknr;
	*entry_in_block = group_offset % mdt->entries_per_block;
}


static int
nilfs_vtop(struct nilfs_device *nilfsdev, uint64_t vblocknr, uint64_t *pblocknr)
{
	struct nilfs_dat_entry *entry;
	struct buf *bp;
	uint64_t  ldatblknr;
	uint32_t  entry_in_block;
	int error;

	nilfs_mdt_trans(&nilfsdev->dat_mdt, vblocknr,
		&ldatblknr, &entry_in_block);

	error = nilfs_bread(nilfsdev->dat_node, ldatblknr, 0, &bp);
	if (error) {
		printf("vtop: can't read in DAT block %"PRIu64"!\n", ldatblknr);
		return error;
	}

	/* get our translation */
	entry = ((struct nilfs_dat_entry *) bp->b_data) + entry_in_block;
#if 0
	printf("\tvblk %4"PRIu64" -> %"PRIu64" for "
		"checkpoint %"PRIu64" to %"PRIu64"\n",
		vblocknr,
		nilfs_rw64(entry->de_blocknr),
		nilfs_rw64(entry->de_start),
		nilfs_rw64(entry->de_end));
#endif

	*pblocknr = nilfs_rw64(entry->de_blocknr);
	brelse(bp, BC_AGE);

	return 0;
}


int
nilfs_nvtop(struct nilfs_node *node, uint64_t blks, uint64_t *l2vmap,
		uint64_t *v2pmap)
{
	uint64_t vblocknr, *pblocknr;
	int i, error;

	/* the DAT inode is the only one not mapped virtual */
	if (node->ino == NILFS_DAT_INO) {
		memcpy(v2pmap, l2vmap, blks * sizeof(uint64_t));
		return 0;
	}

	/* TODO / OPTI more translations in one go */
	error = 0;
	for (i = 0; i < blks; i++) {
		vblocknr  = l2vmap[i];
		pblocknr  = v2pmap + i;
		*pblocknr = 0;

		/* only translate valid vblocknrs */
		if (vblocknr == 0)
			continue;
		error = nilfs_vtop(node->nilfsdev, vblocknr, pblocknr);
		if (error)
			break;
	}

	return error;
}

/* --------------------------------------------------------------------- */

struct nilfs_recover_info {
	uint64_t segnum;
	uint64_t pseg;

	struct nilfs_segment_summary segsum;
	struct nilfs_super_root      super_root;
	STAILQ_ENTRY(nilfs_recover_info) next;
};


/*
 * Helper functions of nilfs_mount() that actually mounts the disc.
 */
static int
nilfs_load_segsum(struct nilfs_device *nilfsdev,
	struct nilfs_recover_info *ri)
{
	struct buf *bp;
	uint64_t blocknr, offset;
	uint32_t segsum_struct_size;
	uint32_t magic;
	int error;

	segsum_struct_size = sizeof(struct nilfs_segment_summary);

	/* read in segsum structure */
	bp      = NULL;
	blocknr = ri->pseg;
	offset  = 0;
	error = nilfs_get_segment_log(nilfsdev,
			&blocknr, &offset, &bp,
			segsum_struct_size, (void *) &ri->segsum);
	if (error)
		goto out;

	/* sanity checks */
	magic = nilfs_rw32(ri->segsum.ss_magic);
	if (magic != NILFS_SEGSUM_MAGIC) {
		DPRINTF(VOLUMES, ("nilfs: bad magic in pseg %"PRIu64"\n",
			ri->pseg));
		error = EINVAL;
		goto out;
	}

	/* TODO check segment summary checksum */
	/* TODO check data checksum */

out:
	if (bp)
		brelse(bp, BC_AGE);

	return error;
}


static int
nilfs_load_super_root(struct nilfs_device *nilfsdev,
	struct nilfs_recover_info *ri)
{
	struct nilfs_segment_summary *segsum = &ri->segsum;
	struct nilfs_super_root *super_root;
	struct buf *bp;
	uint64_t blocknr, offset;
	uint32_t segsum_size, size;
	uint32_t nsumblk, nfileblk;
	uint32_t super_root_crc, comp_crc;
	int off, error;

	/* process segment summary */
	segsum_size = nilfs_rw32(segsum->ss_sumbytes);
	nsumblk     = (segsum_size - 1) / nilfsdev->blocksize + 1;
	nfileblk    = nilfs_rw32(segsum->ss_nblocks) - nsumblk;

	/* check if there is a superroot */
	if ((nilfs_rw16(segsum->ss_flags) & NILFS_SS_SR) == 0) {
		DPRINTF(VOLUMES, ("nilfs: no super root in pseg %"PRIu64"\n",
			ri->pseg));
		return ENOENT;
	}

	/* get our super root, located at the end of the pseg */
	blocknr = ri->pseg + nsumblk + nfileblk - 1;
	offset = 0;
	size = sizeof(struct nilfs_super_root);
	bp = NULL;
	error = nilfs_get_segment_log(nilfsdev,
			&blocknr, &offset, &bp,
			size, (void *) &nilfsdev->super_root);
	if (bp)
		brelse(bp, BC_AGE);
	if (error) {
		printf("read in of superroot failed\n");
		return EIO;
	}

	/* check super root crc */
	super_root = &nilfsdev->super_root;
	super_root_crc = nilfs_rw32(super_root->sr_sum);
	off = sizeof(super_root->sr_sum);
	comp_crc = crc32_le(nilfs_rw32(nilfsdev->super.s_crc_seed),
		(uint8_t *) super_root + off,
		NILFS_SR_BYTES - off);
	if (super_root_crc != comp_crc) {
		DPRINTF(VOLUMES, ("    invalid superroot, likely from old format\n"));
		return EINVAL;
	}

	DPRINTF(VOLUMES, ("    got valid superroot\n"));

	return 0;
}

/* 
 * Search for the last super root recorded.
 */
void
nilfs_search_super_root(struct nilfs_device *nilfsdev)
{
	struct nilfs_super_block *super;
	struct nilfs_segment_summary *segsum;
	struct nilfs_recover_info *ri, *ori, *i_ri;
	STAILQ_HEAD(,nilfs_recover_info) ri_list;
	uint64_t seg_start, seg_end, cno;
	uint32_t segsum_size;
	uint32_t nsumblk, nfileblk;
	int error;

	STAILQ_INIT(&ri_list);

	/* search for last super root */
	ri = malloc(sizeof(struct nilfs_recover_info), M_NILFSTEMP, M_WAITOK);
	memset(ri, 0, sizeof(struct nilfs_recover_info));

	/* if enabled, start from the specified position */
	if (0) {
		/* start from set start */
		nilfsdev->super.s_last_pseg = nilfsdev->super.s_first_data_block;
		nilfsdev->super.s_last_cno  = nilfs_rw64(1);
	}

	ri->pseg   = nilfs_rw64(nilfsdev->super.s_last_pseg); /* blknr */
	ri->segnum = nilfs_get_segnum_of_block(nilfsdev, ri->pseg);

	error = 0;
	cno = nilfs_rw64(nilfsdev->super.s_last_cno);
	DPRINTF(VOLUMES, ("nilfs: seach_super_root start in pseg %"PRIu64"\n",
			ri->pseg));
	for (;;) {
		DPRINTF(VOLUMES, (" at pseg %"PRIu64"\n", ri->pseg));
		error = nilfs_load_segsum(nilfsdev, ri);
		if (error)
			break;

		segsum = &ri->segsum;

		/* try to load super root */
		if (nilfs_rw16(segsum->ss_flags) & NILFS_SS_SR) {
			DPRINTF(VOLUMES, (" try super root\n"));
			error = nilfs_load_super_root(nilfsdev, ri);
			if (error)
				break;	/* confused */
			/* wipe current list of ri */
			while (!STAILQ_EMPTY(&ri_list)) {
				i_ri = STAILQ_FIRST(&ri_list);
				STAILQ_REMOVE_HEAD(&ri_list, next);
				free(i_ri, M_NILFSTEMP);
			}
			super = &nilfsdev->super;

			super->s_last_pseg = nilfs_rw64(ri->pseg);
			super->s_last_cno  = cno++;
			super->s_last_seq  = segsum->ss_seq;
			super->s_state     = nilfs_rw16(NILFS_VALID_FS);
		} else {
			STAILQ_INSERT_TAIL(&ri_list, ri, next);
			ori = ri;
			ri = malloc(sizeof(struct nilfs_recover_info),
				M_NILFSTEMP, M_WAITOK);
			memset(ri, 0, sizeof(struct nilfs_recover_info));
			ri->segnum = ori->segnum;
			ri->pseg   = ori->pseg;
			/* segsum keeps pointing to the `old' ri */
		}

		/* continue to the next pseg */
		segsum_size = nilfs_rw32(segsum->ss_sumbytes);
		nsumblk     = (segsum_size - 1) / nilfsdev->blocksize + 1;
		nfileblk    = nilfs_rw32(segsum->ss_nblocks) - nsumblk;

		/* calculate next partial segment location */
		ri->pseg += nsumblk + nfileblk;

		/* did we reach the end of the segment? if so, go to the next */
		nilfs_get_segment_range(nilfsdev, ri->segnum, &seg_start, &seg_end);
		if (ri->pseg >= seg_end)
			ri->pseg = nilfs_rw64(segsum->ss_next);
		ri->segnum = nilfs_get_segnum_of_block(nilfsdev, ri->pseg);
	}
 
	/* 
	 * XXX No roll-forward yet of the remaining partial segments.
	 */

	/* wipe current list of ri */
	while (!STAILQ_EMPTY(&ri_list)) {
		i_ri = STAILQ_FIRST(&ri_list);
		STAILQ_REMOVE_HEAD(&ri_list, next);
		printf("nilfs: ignoring pseg at %"PRIu64"\n", i_ri->pseg);
		free(i_ri, M_NILFSTEMP);
	}
	free(ri, M_NILFSTEMP);
}

/* --------------------------------------------------------------------- */

int
nilfs_get_node_raw(struct nilfs_device *nilfsdev, struct nilfs_mount *ump,
	uint64_t ino, struct nilfs_inode *inode, struct nilfs_node **nodep)
{
	struct nilfs_node *node;

	*nodep = NULL;

	node = pool_get(&nilfs_node_pool, PR_WAITOK);
	memset(node, 0, sizeof(struct nilfs_node));

	/* crosslink */
	node->ump      = ump;
	node->nilfsdev = nilfsdev;

	/* initiase nilfs node */
	node->ino   = ino;
	node->inode = *inode;
	node->lockf = NULL;

	/* initialise locks */
	mutex_init(&node->node_mutex, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&node->node_lock, "nilfsnlk");

	/* fixup inode size for system nodes */
	if ((ino < NILFS_USER_INO) && (ino != NILFS_ROOT_INO)) {
		DPRINTF(VOLUMES, ("NEED TO GET my size for inode %"PRIu64"\n",
			ino));
		/* for now set it to maximum, -1 is illegal */
		inode->i_size = nilfs_rw64(((uint64_t) -2));
	}

	/* return node */
	*nodep = node;
	return 0;
}

void
nilfs_dispose_node(struct nilfs_node **nodep)
{
	struct nilfs_node *node;

	/* protect against rogue values */
	if (!*nodep)
		return;

	node = *nodep;

	/* remove dirhash if present */
	dirhash_purge(&node->dir_hash);

	/* destroy our locks */
	mutex_destroy(&node->node_mutex);
	cv_destroy(&node->node_lock);

	/* free our associated memory */
	pool_put(&nilfs_node_pool, node);

	*nodep = NULL;
}


void
nilfs_itimes(struct nilfs_node *node, struct timespec *acc,
	struct timespec *mod, struct timespec *birth)
{
}


int
nilfs_update(struct vnode *node, struct timespec *acc,
	struct timespec *mod, struct timespec *birth, int updflags)
{
	return EROFS;
}


int
nilfs_chsize(struct vnode *vp, u_quad_t newsize, kauth_cred_t cred)
{
	return EROFS;
}



int
nilfs_grow_node(struct nilfs_node *node, uint64_t new_size)
{
	return EROFS;
}


int
nilfs_shrink_node(struct nilfs_node *node, uint64_t new_size)
{
	return EROFS;
}


static int
dirhash_fill(struct nilfs_node *dir_node)
{
	struct vnode *dvp = dir_node->vnode;
	struct dirhash *dirh;
	struct nilfs_dir_entry *ndirent;
	struct dirent dirent;
	struct buf *bp;
	uint64_t file_size, diroffset, blkoff;
	uint64_t blocknr;
	uint32_t blocksize = dir_node->nilfsdev->blocksize;
	uint8_t *pos, name_len;
	int error;

	DPRINTF(CALL, ("dirhash_fill called\n"));

	if (dvp->v_type != VDIR)
		return ENOTDIR;

	/* make sure we have a dirhash to work on */
	dirh = dir_node->dir_hash;
	KASSERT(dirh);
	KASSERT(dirh->refcnt > 0);

	if (dirh->flags & DIRH_BROKEN)
		return EIO;

	if (dirh->flags & DIRH_COMPLETE)
		return 0;

	DPRINTF(DIRHASH, ("Filling directory hash\n"));

	/* make sure we have a clean dirhash to add to */
	dirhash_purge_entries(dirh);

	/* get directory filesize */
	file_size = nilfs_rw64(dir_node->inode.i_size);

	/* walk the directory */
	error = 0;
	diroffset = 0;

	blocknr = diroffset / blocksize;
	blkoff  = diroffset % blocksize;
	error = nilfs_bread(dir_node, blocknr, 0, &bp);
	if (error) {
		dirh->flags |= DIRH_BROKEN;
		dirhash_purge_entries(dirh);
		return EIO;
	}
	while (diroffset < file_size) {
		DPRINTF(READDIR, ("filldir : offset = %"PRIu64"\n",
			diroffset));
		if (blkoff >= blocksize) {
			blkoff = 0; blocknr++;
			brelse(bp, BC_AGE);
			error = nilfs_bread(dir_node, blocknr, 0, &bp);
			if (error) {
				dirh->flags |= DIRH_BROKEN;
				dirhash_purge_entries(dirh);
				return EIO;
			}
		}

		/* read in one dirent */
		pos = (uint8_t *) bp->b_data + blkoff;
		ndirent = (struct nilfs_dir_entry *) pos;
		name_len = ndirent->name_len;

		memset(&dirent, 0, sizeof(struct dirent));
		dirent.d_fileno = nilfs_rw64(ndirent->inode);
		dirent.d_type   = ndirent->file_type;	/* 1:1 ? */
		dirent.d_namlen = name_len;
		strncpy(dirent.d_name, ndirent->name, name_len);
		dirent.d_reclen = _DIRENT_SIZE(&dirent);
		DPRINTF(DIRHASH, ("copying `%*.*s`\n", name_len,
			name_len, dirent.d_name));

		/* XXX is it deleted? extra free space? */
		dirhash_enter(dirh, &dirent, diroffset,
			nilfs_rw16(ndirent->rec_len), 0);

		/* advance */
		diroffset += nilfs_rw16(ndirent->rec_len);
		blkoff    += nilfs_rw16(ndirent->rec_len);
	}
	brelse(bp, BC_AGE);

	dirh->flags |= DIRH_COMPLETE;

	return 0;
}


int
nilfs_lookup_name_in_dir(struct vnode *dvp, const char *name, int namelen,
		uint64_t *ino, int *found)
{
	struct nilfs_node	*dir_node = VTOI(dvp);
	struct nilfs_dir_entry *ndirent;
	struct dirhash		*dirh;
	struct dirhash_entry	*dirh_ep;
	struct buf *bp;
	uint64_t diroffset, blkoff;
	uint64_t blocknr;
	uint32_t blocksize = dir_node->nilfsdev->blocksize;
	uint8_t *pos;
	int hit, error;

	/* set default return */
	*found = 0;

	/* get our dirhash and make sure its read in */
	dirhash_get(&dir_node->dir_hash);
	error = dirhash_fill(dir_node);
	if (error) {
		dirhash_put(dir_node->dir_hash);
		return error;
	}
	dirh = dir_node->dir_hash;

	/* allocate temporary space for fid */

	DPRINTF(DIRHASH, ("dirhash_lookup looking for `%*.*s`\n",
		namelen, namelen, name));

	/* search our dirhash hits */
	*ino = 0;
	dirh_ep = NULL;
	for (;;) {
		hit = dirhash_lookup(dirh, name, namelen, &dirh_ep);
		/* if no hit, abort the search */
		if (!hit)
			break;

		/* check this hit */
		diroffset = dirh_ep->offset;

		blocknr = diroffset / blocksize;
		blkoff  = diroffset % blocksize;
		error = nilfs_bread(dir_node, blocknr, 0, &bp);
		if (error)
			return EIO;

		/* read in one dirent */
		pos = (uint8_t *) bp->b_data + blkoff;
		ndirent = (struct nilfs_dir_entry *) pos;

		DPRINTF(DIRHASH, ("dirhash_lookup\tchecking `%*.*s`\n",
			ndirent->name_len, ndirent->name_len, ndirent->name));

		/* see if its our entry */
		KASSERT(ndirent->name_len == namelen);
		if (strncmp(ndirent->name, name, namelen) == 0) {
			*found = 1;
			*ino = nilfs_rw64(ndirent->inode);
			brelse(bp, BC_AGE);
			break;
		}
		brelse(bp, BC_AGE);
	}

	dirhash_put(dir_node->dir_hash);

	return error;
}


int
nilfs_dir_detach(struct nilfs_mount *ump, struct nilfs_node *dir_node, struct nilfs_node *node, struct componentname *cnp)
{
	return EROFS;
}


int
nilfs_dir_attach(struct nilfs_mount *ump, struct nilfs_node *dir_node, struct nilfs_node *node, struct vattr *vap, struct componentname *cnp)
{
	return EROFS;
}


/* XXX return vnode? */
int
nilfs_create_node(struct vnode *dvp, struct vnode **vpp, struct vattr *vap, struct componentname *cnp)
{
	return EROFS;
}


void
nilfs_delete_node(struct nilfs_node *node)
{
}



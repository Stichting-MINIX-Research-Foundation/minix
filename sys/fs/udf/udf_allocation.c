/* $NetBSD: udf_allocation.c,v 1.38 2015/08/24 08:30:17 hannken Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: udf_allocation.c,v 1.38 2015/08/24 08:30:17 hannken Exp $");
#endif /* not lint */


#if defined(_KERNEL_OPT)
#include "opt_compat_netbsd.h"
#endif

/* TODO strip */
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

static void udf_record_allocation_in_node(struct udf_mount *ump,
	struct buf *buf, uint16_t vpart_num, uint64_t *mapping,
	struct long_ad *node_ad_cpy);

static void udf_collect_free_space_for_vpart(struct udf_mount *ump,
	uint16_t vpart_num, uint32_t num_lb);

static int udf_ads_merge(uint32_t max_len, uint32_t lb_size, struct long_ad *a1, struct long_ad *a2);
static void udf_wipe_adslots(struct udf_node *udf_node);
static void udf_count_alloc_exts(struct udf_node *udf_node);


/* --------------------------------------------------------------------- */

#if 0
#if 1
static void
udf_node_dump(struct udf_node *udf_node) {
	struct file_entry    *fe;
	struct extfile_entry *efe;
	struct icb_tag *icbtag;
	struct long_ad s_ad;
	uint64_t inflen;
	uint32_t icbflags, addr_type;
	uint32_t len, lb_num;
	uint32_t flags;
	int part_num;
	int lb_size, eof, slot;

	if ((udf_verbose & UDF_DEBUG_NODEDUMP) == 0)
		return;

	lb_size = udf_rw32(udf_node->ump->logical_vol->lb_size);

	fe  = udf_node->fe;
	efe = udf_node->efe;
	if (fe) {
		icbtag = &fe->icbtag;
		inflen = udf_rw64(fe->inf_len);
	} else {
		icbtag = &efe->icbtag;
		inflen = udf_rw64(efe->inf_len);
	}

	icbflags   = udf_rw16(icbtag->flags);
	addr_type  = icbflags & UDF_ICB_TAG_FLAGS_ALLOC_MASK;

	printf("udf_node_dump %p :\n", udf_node);

	if (addr_type == UDF_ICB_INTERN_ALLOC) {
		printf("\tIntern alloc, len = %"PRIu64"\n", inflen);
		return;
	}

	printf("\tInflen  = %"PRIu64"\n", inflen);
	printf("\t\t");

	slot = 0;
	for (;;) {
		udf_get_adslot(udf_node, slot, &s_ad, &eof);
		if (eof)
			break;
		part_num = udf_rw16(s_ad.loc.part_num);
		lb_num = udf_rw32(s_ad.loc.lb_num);
		len   = udf_rw32(s_ad.len);
		flags = UDF_EXT_FLAGS(len);
		len   = UDF_EXT_LEN(len);

		printf("[");
		if (part_num >= 0)
			printf("part %d, ", part_num);
		printf("lb_num %d, len %d", lb_num, len);
		if (flags)
			printf(", flags %d", flags>>30);
		printf("] ");

		if (flags == UDF_EXT_REDIRECT) {
			printf("\n\textent END\n\tallocation extent\n\t\t");
		}

		slot++;
	}
	printf("\n\tl_ad END\n\n");
}
#else
#define udf_node_dump(a)
#endif


static void
udf_assert_allocated(struct udf_mount *ump, uint16_t vpart_num,
	uint32_t lb_num, uint32_t num_lb)
{
	struct udf_bitmap *bitmap;
	struct part_desc *pdesc;
	uint32_t ptov;
	uint32_t bitval;
	uint8_t *bpos;
	int bit;
	int phys_part;
	int ok;

	DPRINTF(PARANOIA, ("udf_assert_allocated: check virt lbnum %d "
			  "part %d + %d sect\n", lb_num, vpart_num, num_lb));

	/* get partition backing up this vpart_num */
	pdesc = ump->partitions[ump->vtop[vpart_num]];

	switch (ump->vtop_tp[vpart_num]) {
	case UDF_VTOP_TYPE_PHYS :
	case UDF_VTOP_TYPE_SPARABLE :
		/* free space to freed or unallocated space bitmap */
		ptov      = udf_rw32(pdesc->start_loc);
		phys_part = ump->vtop[vpart_num];

		/* use unallocated bitmap */
		bitmap = &ump->part_unalloc_bits[phys_part];

		/* if no bitmaps are defined, bail out */
		if (bitmap->bits == NULL)
			break;

		/* check bits */
		KASSERT(bitmap->bits);
		ok = 1;
		bpos = bitmap->bits + lb_num/8;
		bit  = lb_num % 8;
		while (num_lb > 0) {
			bitval = (1 << bit);
			DPRINTF(PARANOIA, ("XXX : check %d, %p, bit %d\n",
				lb_num, bpos, bit));
			KASSERT(bitmap->bits + lb_num/8 == bpos);
			if (*bpos & bitval) {
				printf("\tlb_num %d is NOT marked busy\n",
					lb_num);
				ok = 0;
			}
			lb_num++; num_lb--;
			bit = (bit + 1) % 8;
			if (bit == 0)
				bpos++;
		}
		if (!ok) {
			/* KASSERT(0); */
		}

		break;
	case UDF_VTOP_TYPE_VIRT :
		/* TODO check space */
		KASSERT(num_lb == 1);
		break;
	case UDF_VTOP_TYPE_META :
		/* TODO check space in the metadata bitmap */
	default:
		/* not implemented */
		break;
	}
}


static void
udf_node_sanity_check(struct udf_node *udf_node,
		uint64_t *cnt_inflen, uint64_t *cnt_logblksrec)
{
	union dscrptr *dscr;
	struct file_entry    *fe;
	struct extfile_entry *efe;
	struct icb_tag *icbtag;
	struct long_ad  s_ad;
	uint64_t inflen, logblksrec;
	uint32_t icbflags, addr_type;
	uint32_t len, lb_num, l_ea, l_ad, max_l_ad;
	uint16_t part_num;
	uint8_t *data_pos;
	int dscr_size, lb_size, flags, whole_lb;
	int i, slot, eof;

//	KASSERT(mutex_owned(&udf_node->ump->allocate_mutex));

	if (1)
		udf_node_dump(udf_node);

	lb_size = udf_rw32(udf_node->ump->logical_vol->lb_size);

	fe  = udf_node->fe;
	efe = udf_node->efe;
	if (fe) {
		dscr       = (union dscrptr *) fe;
		icbtag     = &fe->icbtag;
		inflen     = udf_rw64(fe->inf_len);
		dscr_size  = sizeof(struct file_entry) -1;
		logblksrec = udf_rw64(fe->logblks_rec);
		l_ad       = udf_rw32(fe->l_ad);
		l_ea       = udf_rw32(fe->l_ea);
	} else {
		dscr       = (union dscrptr *) efe;
		icbtag     = &efe->icbtag;
		inflen     = udf_rw64(efe->inf_len);
		dscr_size  = sizeof(struct extfile_entry) -1;
		logblksrec = udf_rw64(efe->logblks_rec);
		l_ad       = udf_rw32(efe->l_ad);
		l_ea       = udf_rw32(efe->l_ea);
	}
	data_pos  = (uint8_t *) dscr + dscr_size + l_ea;
	max_l_ad   = lb_size - dscr_size - l_ea;
	icbflags   = udf_rw16(icbtag->flags);
	addr_type  = icbflags & UDF_ICB_TAG_FLAGS_ALLOC_MASK;

	/* check if tail is zero */
	DPRINTF(PARANOIA, ("Sanity check blank tail\n"));
	for (i = l_ad; i < max_l_ad; i++) {
		if (data_pos[i] != 0)
			printf( "sanity_check: violation: node byte %d "
				"has value %d\n", i, data_pos[i]);
	}

	/* reset counters */
	*cnt_inflen     = 0;
	*cnt_logblksrec = 0;

	if (addr_type == UDF_ICB_INTERN_ALLOC) {
		KASSERT(l_ad <= max_l_ad);
		KASSERT(l_ad == inflen);
		*cnt_inflen = inflen;
		return;
	}

	/* start counting */
	whole_lb = 1;
	slot = 0;
	for (;;) {
		udf_get_adslot(udf_node, slot, &s_ad, &eof);
		if (eof)
			break;
		KASSERT(whole_lb == 1);

		part_num = udf_rw16(s_ad.loc.part_num);
		lb_num = udf_rw32(s_ad.loc.lb_num);
		len   = udf_rw32(s_ad.len);
		flags = UDF_EXT_FLAGS(len);
		len   = UDF_EXT_LEN(len);

		if (flags != UDF_EXT_REDIRECT) {
			*cnt_inflen += len;
			if (flags == UDF_EXT_ALLOCATED) {
				*cnt_logblksrec += (len + lb_size -1) / lb_size;
			}
		} else {
			KASSERT(len == lb_size);
		}
		/* check allocation */
		if (flags == UDF_EXT_ALLOCATED)
			udf_assert_allocated(udf_node->ump, part_num, lb_num,
				(len + lb_size - 1) / lb_size);

		/* check whole lb */
		whole_lb = ((len % lb_size) == 0);

		slot++;
	}
	/* rest should be zero (ad_off > l_ad < max_l_ad - adlen) */

	KASSERT(*cnt_inflen == inflen);
	KASSERT(*cnt_logblksrec == logblksrec);

//	KASSERT(mutex_owned(&udf_node->ump->allocate_mutex));
}
#else
static void
udf_node_sanity_check(struct udf_node *udf_node,
		uint64_t *cnt_inflen, uint64_t *cnt_logblksrec) {
	struct file_entry    *fe;
	struct extfile_entry *efe;
	uint64_t inflen, logblksrec;

	fe  = udf_node->fe;
	efe = udf_node->efe;
	if (fe) {
		inflen = udf_rw64(fe->inf_len);
		logblksrec = udf_rw64(fe->logblks_rec);
	} else {
		inflen = udf_rw64(efe->inf_len);
		logblksrec = udf_rw64(efe->logblks_rec);
	}
	*cnt_logblksrec = logblksrec;
	*cnt_inflen     = inflen;
}
#endif

/* --------------------------------------------------------------------- */

void
udf_calc_freespace(struct udf_mount *ump, uint64_t *sizeblks, uint64_t *freeblks)
{
	struct logvol_int_desc *lvid;
	uint32_t *pos1, *pos2;
	int vpart, num_vpart;

	lvid = ump->logvol_integrity;
	*freeblks = *sizeblks = 0;

	/*
	 * Sequentials media report free space directly (CD/DVD/BD-R), for the
	 * other media we need the logical volume integrity.
	 *
	 * We sum all free space up here regardless of type.
	 */

	KASSERT(lvid);
	num_vpart = udf_rw32(lvid->num_part);

	if (ump->discinfo.mmc_cur & MMC_CAP_SEQUENTIAL) {
		/* use track info directly summing if there are 2 open */
		/* XXX assumption at most two tracks open */
		*freeblks = ump->data_track.free_blocks;
		if (ump->data_track.tracknr != ump->metadata_track.tracknr)
			*freeblks += ump->metadata_track.free_blocks;
		*sizeblks = ump->discinfo.last_possible_lba;
	} else {
		/* free and used space for mountpoint based on logvol integrity */
		for (vpart = 0; vpart < num_vpart; vpart++) {
			pos1 = &lvid->tables[0] + vpart;
			pos2 = &lvid->tables[0] + num_vpart + vpart;
			if (udf_rw32(*pos1) != (uint32_t) -1) {
				*freeblks += udf_rw32(*pos1);
				*sizeblks += udf_rw32(*pos2);
			}
		}
	}
	/* adjust for accounted uncommitted blocks */
	for (vpart = 0; vpart < num_vpart; vpart++)
		*freeblks -= ump->uncommitted_lbs[vpart];

	if (*freeblks > UDF_DISC_SLACK) {
		*freeblks -= UDF_DISC_SLACK;
	} else {
		*freeblks = 0;
	}
}


static void
udf_calc_vpart_freespace(struct udf_mount *ump, uint16_t vpart_num, uint64_t *freeblks)
{
	struct logvol_int_desc *lvid;
	uint32_t *pos1;

	lvid = ump->logvol_integrity;
	*freeblks = 0;

	/*
	 * Sequentials media report free space directly (CD/DVD/BD-R), for the
	 * other media we need the logical volume integrity.
	 *
	 * We sum all free space up here regardless of type.
	 */

	KASSERT(lvid);
	if (ump->discinfo.mmc_cur & MMC_CAP_SEQUENTIAL) {
		/* XXX assumption at most two tracks open */
		if (vpart_num == ump->data_part) {
			*freeblks = ump->data_track.free_blocks;
		} else {
			*freeblks = ump->metadata_track.free_blocks;
		}
	} else {
		/* free and used space for mountpoint based on logvol integrity */
		pos1 = &lvid->tables[0] + vpart_num;
		if (udf_rw32(*pos1) != (uint32_t) -1)
			*freeblks += udf_rw32(*pos1);
	}

	/* adjust for accounted uncommitted blocks */
	if (*freeblks > ump->uncommitted_lbs[vpart_num]) {
		*freeblks -= ump->uncommitted_lbs[vpart_num];
	} else {
		*freeblks = 0;
	}
}

/* --------------------------------------------------------------------- */

int
udf_translate_vtop(struct udf_mount *ump, struct long_ad *icb_loc,
		   uint32_t *lb_numres, uint32_t *extres)
{
	struct part_desc       *pdesc;
	struct spare_map_entry *sme;
	struct long_ad s_icb_loc;
	uint64_t foffset, end_foffset;
	uint32_t lb_size, len;
	uint32_t lb_num, lb_rel, lb_packet;
	uint32_t udf_rw32_lbmap, ext_offset;
	uint16_t vpart;
	int rel, part, error, eof, slot, flags;

	assert(ump && icb_loc && lb_numres);

	vpart  = udf_rw16(icb_loc->loc.part_num);
	lb_num = udf_rw32(icb_loc->loc.lb_num);
	if (vpart > UDF_VTOP_RAWPART)
		return EINVAL;

translate_again:
	part = ump->vtop[vpart];
	pdesc = ump->partitions[part];

	switch (ump->vtop_tp[vpart]) {
	case UDF_VTOP_TYPE_RAW :
		/* 1:1 to the end of the device */
		*lb_numres = lb_num;
		*extres = INT_MAX;
		return 0;
	case UDF_VTOP_TYPE_PHYS :
		/* transform into its disc logical block */
		if (lb_num > udf_rw32(pdesc->part_len))
			return EINVAL;
		*lb_numres = lb_num + udf_rw32(pdesc->start_loc);

		/* extent from here to the end of the partition */
		*extres = udf_rw32(pdesc->part_len) - lb_num;
		return 0;
	case UDF_VTOP_TYPE_VIRT :
		/* only maps one logical block, lookup in VAT */
		if (lb_num >= ump->vat_entries)		/* XXX > or >= ? */
			return EINVAL;

		/* lookup in virtual allocation table file */
		mutex_enter(&ump->allocate_mutex);
		error = udf_vat_read(ump->vat_node,
				(uint8_t *) &udf_rw32_lbmap, 4,
				ump->vat_offset + lb_num * 4);
		mutex_exit(&ump->allocate_mutex);

		if (error)
			return error;

		lb_num = udf_rw32(udf_rw32_lbmap);

		/* transform into its disc logical block */
		if (lb_num > udf_rw32(pdesc->part_len))
			return EINVAL;
		*lb_numres = lb_num + udf_rw32(pdesc->start_loc);

		/* just one logical block */
		*extres = 1;
		return 0;
	case UDF_VTOP_TYPE_SPARABLE :
		/* check if the packet containing the lb_num is remapped */
		lb_packet = lb_num / ump->sparable_packet_size;
		lb_rel    = lb_num % ump->sparable_packet_size;

		for (rel = 0; rel < udf_rw16(ump->sparing_table->rt_l); rel++) {
			sme = &ump->sparing_table->entries[rel];
			if (lb_packet == udf_rw32(sme->org)) {
				/* NOTE maps to absolute disc logical block! */
				*lb_numres = udf_rw32(sme->map) + lb_rel;
				*extres    = ump->sparable_packet_size - lb_rel;
				return 0;
			}
		}

		/* transform into its disc logical block */
		if (lb_num > udf_rw32(pdesc->part_len))
			return EINVAL;
		*lb_numres = lb_num + udf_rw32(pdesc->start_loc);

		/* rest of block */
		*extres = ump->sparable_packet_size - lb_rel;
		return 0;
	case UDF_VTOP_TYPE_META :
		/* we have to look into the file's allocation descriptors */

		/* use metadatafile allocation mutex */
		lb_size = udf_rw32(ump->logical_vol->lb_size);

		UDF_LOCK_NODE(ump->metadata_node, 0);

		/* get first overlapping extent */
		foffset = 0;
		slot    = 0;
		for (;;) {
			udf_get_adslot(ump->metadata_node,
				slot, &s_icb_loc, &eof);
			DPRINTF(ADWLK, ("slot %d, eof = %d, flags = %d, "
				"len = %d, lb_num = %d, part = %d\n",
				slot, eof,
				UDF_EXT_FLAGS(udf_rw32(s_icb_loc.len)),
				UDF_EXT_LEN(udf_rw32(s_icb_loc.len)),
				udf_rw32(s_icb_loc.loc.lb_num),
				udf_rw16(s_icb_loc.loc.part_num)));
			if (eof) {
				DPRINTF(TRANSLATE,
					("Meta partition translation "
					 "failed: can't seek location\n"));
				UDF_UNLOCK_NODE(ump->metadata_node, 0);
				return EINVAL;
			}
			len   = udf_rw32(s_icb_loc.len);
			flags = UDF_EXT_FLAGS(len);
			len   = UDF_EXT_LEN(len);

			if (flags == UDF_EXT_REDIRECT) {
				slot++;
				continue;
			}

			end_foffset = foffset + len;

			if (end_foffset > (uint64_t) lb_num * lb_size)
				break;	/* found */
			foffset = end_foffset;
			slot++;
		}
		/* found overlapping slot */
		ext_offset = lb_num * lb_size - foffset;

		/* process extent offset */
		lb_num   = udf_rw32(s_icb_loc.loc.lb_num);
		vpart    = udf_rw16(s_icb_loc.loc.part_num);
		lb_num  += (ext_offset + lb_size -1) / lb_size;
		ext_offset = 0;

		UDF_UNLOCK_NODE(ump->metadata_node, 0);
		if (flags != UDF_EXT_ALLOCATED) {
			DPRINTF(TRANSLATE, ("Metadata partition translation "
					    "failed: not allocated\n"));
			return EINVAL;
		}

		/*
		 * vpart and lb_num are updated, translate again since we
		 * might be mapped on sparable media
		 */
		goto translate_again;
	default:
		printf("UDF vtop translation scheme %d unimplemented yet\n",
			ump->vtop_tp[vpart]);
	}

	return EINVAL;
}


/* XXX  provisional primitive braindead version */
/* TODO use ext_res */
void
udf_translate_vtop_list(struct udf_mount *ump, uint32_t sectors,
	uint16_t vpart_num, uint64_t *lmapping, uint64_t *pmapping)
{
	struct long_ad loc;
	uint32_t lb_numres, ext_res;
	int sector;

	for (sector = 0; sector < sectors; sector++) {
		memset(&loc, 0, sizeof(struct long_ad));
		loc.loc.part_num = udf_rw16(vpart_num);
		loc.loc.lb_num   = udf_rw32(*lmapping);
		udf_translate_vtop(ump, &loc, &lb_numres, &ext_res);
		*pmapping = lb_numres;
		lmapping++; pmapping++;
	}
}


/* --------------------------------------------------------------------- */

/*
 * Translate an extent (in logical_blocks) into logical block numbers; used
 * for read and write operations. DOESNT't check extents.
 */

int
udf_translate_file_extent(struct udf_node *udf_node,
		          uint32_t from, uint32_t num_lb,
			  uint64_t *map)
{
	struct udf_mount *ump;
	struct icb_tag *icbtag;
	struct long_ad t_ad, s_ad;
	uint64_t transsec;
	uint64_t foffset, end_foffset;
	uint32_t transsec32;
	uint32_t lb_size;
	uint32_t ext_offset;
	uint32_t lb_num, len;
	uint32_t overlap, translen;
	uint16_t vpart_num;
	int eof, error, flags;
	int slot, addr_type, icbflags;

	if (!udf_node)
		return ENOENT;

	KASSERT(num_lb > 0);

	UDF_LOCK_NODE(udf_node, 0);

	/* initialise derivative vars */
	ump = udf_node->ump;
	lb_size = udf_rw32(ump->logical_vol->lb_size);

	if (udf_node->fe) {
		icbtag = &udf_node->fe->icbtag;
	} else {
		icbtag = &udf_node->efe->icbtag;
	}
	icbflags  = udf_rw16(icbtag->flags);
	addr_type = icbflags & UDF_ICB_TAG_FLAGS_ALLOC_MASK;

	/* do the work */
	if (addr_type == UDF_ICB_INTERN_ALLOC) {
		*map = UDF_TRANS_INTERN;
		UDF_UNLOCK_NODE(udf_node, 0);
		return 0;
	}

	/* find first overlapping extent */
	foffset = 0;
	slot    = 0;
	for (;;) {
		udf_get_adslot(udf_node, slot, &s_ad, &eof);
		DPRINTF(ADWLK, ("slot %d, eof = %d, flags = %d, len = %d, "
			"lb_num = %d, part = %d\n", slot, eof,
			UDF_EXT_FLAGS(udf_rw32(s_ad.len)),
			UDF_EXT_LEN(udf_rw32(s_ad.len)),
			udf_rw32(s_ad.loc.lb_num),
			udf_rw16(s_ad.loc.part_num)));
		if (eof) {
			DPRINTF(TRANSLATE,
				("Translate file extent "
				 "failed: can't seek location\n"));
			UDF_UNLOCK_NODE(udf_node, 0);
			return EINVAL;
		}
		len    = udf_rw32(s_ad.len);
		flags  = UDF_EXT_FLAGS(len);
		len    = UDF_EXT_LEN(len);
		lb_num = udf_rw32(s_ad.loc.lb_num);

		if (flags == UDF_EXT_REDIRECT) {
			slot++;
			continue;
		}

		end_foffset = foffset + len;

		if (end_foffset > (uint64_t) from * lb_size)
			break;	/* found */
		foffset = end_foffset;
		slot++;
	}
	/* found overlapping slot */
	ext_offset = (uint64_t) from * lb_size - foffset;

	for (;;) {
		udf_get_adslot(udf_node, slot, &s_ad, &eof);
		DPRINTF(ADWLK, ("slot %d, eof = %d, flags = %d, len = %d, "
			"lb_num = %d, part = %d\n", slot, eof,
			UDF_EXT_FLAGS(udf_rw32(s_ad.len)),
			UDF_EXT_LEN(udf_rw32(s_ad.len)),
			udf_rw32(s_ad.loc.lb_num),
			udf_rw16(s_ad.loc.part_num)));
		if (eof) {
			DPRINTF(TRANSLATE,
				("Translate file extent "
				 "failed: past eof\n"));
			UDF_UNLOCK_NODE(udf_node, 0);
			return EINVAL;
		}
	
		len    = udf_rw32(s_ad.len);
		flags  = UDF_EXT_FLAGS(len);
		len    = UDF_EXT_LEN(len);

		lb_num    = udf_rw32(s_ad.loc.lb_num);
		vpart_num = udf_rw16(s_ad.loc.part_num);

		end_foffset = foffset + len;

		/* process extent, don't forget to advance on ext_offset! */
		lb_num  += (ext_offset + lb_size -1) / lb_size;
		overlap  = (len - ext_offset + lb_size -1) / lb_size;
		ext_offset = 0;

		/*
		 * note that the while(){} is nessisary for the extent that
		 * the udf_translate_vtop() returns doens't have to span the
		 * whole extent.
		 */
	
		overlap = MIN(overlap, num_lb);
		while (overlap && (flags != UDF_EXT_REDIRECT)) {
			switch (flags) {
			case UDF_EXT_FREE :
			case UDF_EXT_ALLOCATED_BUT_NOT_USED :
				transsec = UDF_TRANS_ZERO;
				translen = overlap;
				while (overlap && num_lb && translen) {
					*map++ = transsec;
					lb_num++;
					overlap--; num_lb--; translen--;
				}
				break;
			case UDF_EXT_ALLOCATED :
				t_ad.loc.lb_num   = udf_rw32(lb_num);
				t_ad.loc.part_num = udf_rw16(vpart_num);
				error = udf_translate_vtop(ump,
						&t_ad, &transsec32, &translen);
				transsec = transsec32;
				if (error) {
					UDF_UNLOCK_NODE(udf_node, 0);
					return error;
				}
				while (overlap && num_lb && translen) {
					*map++ = transsec;
					lb_num++; transsec++;
					overlap--; num_lb--; translen--;
				}
				break;
			default:
				DPRINTF(TRANSLATE,
					("Translate file extent "
					 "failed: bad flags %x\n", flags));
				UDF_UNLOCK_NODE(udf_node, 0);
				return EINVAL;
			}
		}
		if (num_lb == 0)
			break;

		if (flags != UDF_EXT_REDIRECT)
			foffset = end_foffset;
		slot++;
	}
	UDF_UNLOCK_NODE(udf_node, 0);

	return 0;
}

/* --------------------------------------------------------------------- */

static int
udf_search_free_vatloc(struct udf_mount *ump, uint32_t *lbnumres)
{
	uint32_t lb_size, lb_num, lb_map, udf_rw32_lbmap;
	uint8_t *blob;
	int entry, chunk, found, error;

	KASSERT(ump);
	KASSERT(ump->logical_vol);

	lb_size = udf_rw32(ump->logical_vol->lb_size);
	blob = malloc(lb_size, M_UDFTEMP, M_WAITOK);

	/* TODO static allocation of search chunk */

	lb_num = MIN(ump->vat_entries, ump->vat_last_free_lb);
	found  = 0;
	error  = 0;
	entry  = 0;
	do {
		chunk = MIN(lb_size, (ump->vat_entries - lb_num) * 4);
		if (chunk <= 0)
			break;
		/* load in chunk */
		error = udf_vat_read(ump->vat_node, blob, chunk,
				ump->vat_offset + lb_num * 4);

		if (error)
			break;

		/* search this chunk */
		for (entry=0; entry < chunk /4; entry++, lb_num++) {
			udf_rw32_lbmap = *((uint32_t *) (blob + entry * 4));
			lb_map = udf_rw32(udf_rw32_lbmap);
			if (lb_map == 0xffffffff) {
				found = 1;
				break;
			}
		}
	} while (!found);
	if (error) {
		printf("udf_search_free_vatloc: error reading in vat chunk "
			"(lb %d, size %d)\n", lb_num, chunk);
	}

	if (!found) {
		/* extend VAT */
		DPRINTF(WRITE, ("udf_search_free_vatloc: extending\n"));
		lb_num = ump->vat_entries;
		ump->vat_entries++;
	}

	/* mark entry with initialiser just in case */
	lb_map = udf_rw32(0xfffffffe);
	udf_vat_write(ump->vat_node, (uint8_t *) &lb_map, 4,
		ump->vat_offset + lb_num *4);
	ump->vat_last_free_lb = lb_num;

	free(blob, M_UDFTEMP);
	*lbnumres = lb_num;
	return 0;
}


static void
udf_bitmap_allocate(struct udf_bitmap *bitmap, int ismetadata,
	uint32_t *num_lb, uint64_t *lmappos)
{
	uint32_t offset, lb_num, bit;
	int32_t  diff;
	uint8_t *bpos;
	int pass;

	if (!ismetadata) {
		/* heuristic to keep the two pointers not too close */
		diff = bitmap->data_pos - bitmap->metadata_pos;
		if ((diff >= 0) && (diff < 1024))
			bitmap->data_pos = bitmap->metadata_pos + 1024;
	}
	offset = ismetadata ? bitmap->metadata_pos : bitmap->data_pos;
	offset &= ~7;
	for (pass = 0; pass < 2; pass++) {
		if (offset >= bitmap->max_offset)
			offset = 0;

		while (offset < bitmap->max_offset) {
			if (*num_lb == 0)
				break;

			/* use first bit not set */
			bpos  = bitmap->bits + offset/8;
			bit = ffs(*bpos);	/* returns 0 or 1..8 */
			if (bit == 0) {
				offset += 8;
				continue;
			}

			/* check for ffs overshoot */
			if (offset + bit-1 >= bitmap->max_offset) {
				offset = bitmap->max_offset;
				break;
			}

			DPRINTF(PARANOIA, ("XXX : allocate %d, %p, bit %d\n",
				offset + bit -1, bpos, bit-1));
			*bpos &= ~(1 << (bit-1));
			lb_num = offset + bit-1;
			*lmappos++ = lb_num;
			*num_lb = *num_lb - 1;
			// offset = (offset & ~7);
		}
	}

	if (ismetadata) {
		bitmap->metadata_pos = offset;
	} else {
		bitmap->data_pos = offset;
	}
}


static void
udf_bitmap_free(struct udf_bitmap *bitmap, uint32_t lb_num, uint32_t num_lb)
{
	uint32_t offset;
	uint32_t bit, bitval;
	uint8_t *bpos;

	offset = lb_num;

	/* starter bits */
	bpos = bitmap->bits + offset/8;
	bit = offset % 8;
	while ((bit != 0) && (num_lb > 0)) {
		bitval = (1 << bit);
		KASSERT((*bpos & bitval) == 0);
		DPRINTF(PARANOIA, ("XXX : free %d, %p, %d\n",
			offset, bpos, bit));
		*bpos |= bitval;
		offset++; num_lb--;
		bit = (bit + 1) % 8;
	}
	if (num_lb == 0)
		return;

	/* whole bytes */
	KASSERT(bit == 0);
	bpos = bitmap->bits + offset / 8;
	while (num_lb >= 8) {
		KASSERT((*bpos == 0));
		DPRINTF(PARANOIA, ("XXX : free %d + 8, %p\n", offset, bpos));
		*bpos = 255;
		offset += 8; num_lb -= 8;
		bpos++;
	}

	/* stop bits */
	KASSERT(num_lb < 8);
	bit = 0;
	while (num_lb > 0) {
		bitval = (1 << bit);
		KASSERT((*bpos & bitval) == 0);
		DPRINTF(PARANOIA, ("XXX : free %d, %p, %d\n",
			offset, bpos, bit));
		*bpos |= bitval;
		offset++; num_lb--;
		bit = (bit + 1) % 8;
	}
}


static uint32_t
udf_bitmap_check_trunc_free(struct udf_bitmap *bitmap, uint32_t to_trunc)
{
	uint32_t seq_free, offset;
	uint8_t *bpos;
	uint8_t  bit, bitval;

	DPRINTF(RESERVE, ("\ttrying to trunc %d bits from bitmap\n", to_trunc));
	offset = bitmap->max_offset - to_trunc;

	/* starter bits (if any) */
	bpos = bitmap->bits + offset/8;
	bit = offset % 8;
	seq_free = 0;
	while (to_trunc > 0) {
		seq_free++;
		bitval = (1 << bit);
		if (!(*bpos & bitval))
			seq_free = 0;
		offset++; to_trunc--;
		bit++;
		if (bit == 8) {
			bpos++;
			bit = 0;
		}
	}

	DPRINTF(RESERVE, ("\tfound %d sequential free bits in bitmap\n", seq_free));
	return seq_free;
}

/* --------------------------------------------------------------------- */

/*
 * We check for overall disc space with a margin to prevent critical
 * conditions.  If disc space is low we try to force a sync() to improve our
 * estimates.  When confronted with meta-data partition size shortage we know
 * we have to check if it can be extended and we need to extend it when
 * needed.
 *
 * A 2nd strategy we could use when disc space is getting low on a disc
 * formatted with a meta-data partition is to see if there are sparse areas in
 * the meta-data partition and free blocks there for extra data.
 */

void
udf_do_reserve_space(struct udf_mount *ump, struct udf_node *udf_node,
	uint16_t vpart_num, uint32_t num_lb)
{
	ump->uncommitted_lbs[vpart_num] += num_lb;
	if (udf_node)
		udf_node->uncommitted_lbs += num_lb;
}


void
udf_do_unreserve_space(struct udf_mount *ump, struct udf_node *udf_node,
	uint16_t vpart_num, uint32_t num_lb)
{
	ump->uncommitted_lbs[vpart_num] -= num_lb;
	if (ump->uncommitted_lbs[vpart_num] < 0) {
		DPRINTF(RESERVE, ("UDF: underflow on partition reservation, "
			"part %d: %d\n", vpart_num,
			ump->uncommitted_lbs[vpart_num]));
		ump->uncommitted_lbs[vpart_num] = 0;
	}
	if (udf_node) {
		udf_node->uncommitted_lbs -= num_lb;
		if (udf_node->uncommitted_lbs < 0) {
			DPRINTF(RESERVE, ("UDF: underflow of node "
				"reservation : %d\n",
				udf_node->uncommitted_lbs));
			udf_node->uncommitted_lbs = 0;
		}
	}
}


int
udf_reserve_space(struct udf_mount *ump, struct udf_node *udf_node,
	int udf_c_type, uint16_t vpart_num, uint32_t num_lb, int can_fail)
{
	uint64_t freeblks;
	uint64_t slack;
	int i, error;

	slack = 0;
	if (can_fail)
		slack = UDF_DISC_SLACK;

	error = 0;
	mutex_enter(&ump->allocate_mutex);

	/* check if there is enough space available */
	for (i = 0; i < 3; i++) {	/* XXX arbitrary number */
		udf_calc_vpart_freespace(ump, vpart_num, &freeblks);
		if (num_lb + slack < freeblks)
			break;
		/* issue SYNC */
		DPRINTF(RESERVE, ("udf_reserve_space: issuing sync\n"));
		mutex_exit(&ump->allocate_mutex);
		udf_do_sync(ump, FSCRED, 0);
		/* 1/8 second wait */
		kpause("udfsync2", false, hz/8, NULL);
		mutex_enter(&ump->allocate_mutex);
	}

	/* check if there is enough space available now */
	udf_calc_vpart_freespace(ump, vpart_num, &freeblks);
	if (num_lb + slack >= freeblks) {
		DPRINTF(RESERVE, ("udf_reserve_space: try to redistribute "
				  "partition space\n"));
		DPRINTF(RESERVE, ("\tvpart %d, type %d is full\n",
				vpart_num, ump->vtop_alloc[vpart_num]));
		/* Try to redistribute space if possible */
		udf_collect_free_space_for_vpart(ump, vpart_num, num_lb + slack);
	}

	/* check if there is enough space available now */
	udf_calc_vpart_freespace(ump, vpart_num, &freeblks);
	if (num_lb + slack <= freeblks) {
		udf_do_reserve_space(ump, udf_node, vpart_num, num_lb);
	} else {
		DPRINTF(RESERVE, ("udf_reserve_space: out of disc space\n"));
		error = ENOSPC;
	}

	mutex_exit(&ump->allocate_mutex);
	return error;
}


void
udf_cleanup_reservation(struct udf_node *udf_node)
{
	struct udf_mount *ump = udf_node->ump;
	int vpart_num;

	mutex_enter(&ump->allocate_mutex);

	/* compensate for overlapping blocks */
	DPRINTF(RESERVE, ("UDF: overlapped %d blocks in count\n", udf_node->uncommitted_lbs));

	vpart_num = udf_get_record_vpart(ump, udf_get_c_type(udf_node));
	udf_do_unreserve_space(ump, udf_node, vpart_num, udf_node->uncommitted_lbs);

	DPRINTF(RESERVE, ("\ttotal now %d\n", ump->uncommitted_lbs[vpart_num]));

	/* sanity */
	if (ump->uncommitted_lbs[vpart_num] < 0)
		ump->uncommitted_lbs[vpart_num] = 0;

	mutex_exit(&ump->allocate_mutex);
}

/* --------------------------------------------------------------------- */

/*
 * Allocate an extent of given length on given virt. partition. It doesn't
 * have to be one stretch.
 */

int
udf_allocate_space(struct udf_mount *ump, struct udf_node *udf_node,
	int udf_c_type, uint16_t vpart_num, uint32_t num_lb, uint64_t *lmapping)
{
	struct mmc_trackinfo *alloc_track, *other_track;
	struct udf_bitmap *bitmap;
	struct part_desc *pdesc;
	struct logvol_int_desc *lvid;
	uint64_t *lmappos;
	uint32_t ptov, lb_num, *freepos, free_lbs;
	int lb_size __diagused, alloc_num_lb;
	int alloc_type, error;
	int is_node;

	DPRINTF(CALL, ("udf_allocate_space(ctype %d, vpart %d, num_lb %d\n",
		udf_c_type, vpart_num, num_lb));
	mutex_enter(&ump->allocate_mutex);

	lb_size = udf_rw32(ump->logical_vol->lb_size);
	KASSERT(lb_size == ump->discinfo.sector_size);

	alloc_type =  ump->vtop_alloc[vpart_num];
	is_node    = (udf_c_type == UDF_C_NODE);

	lmappos = lmapping;
	error = 0;
	switch (alloc_type) {
	case UDF_ALLOC_VAT :
		/* search empty slot in VAT file */
		KASSERT(num_lb == 1);
		error = udf_search_free_vatloc(ump, &lb_num);
		if (!error) {
			*lmappos = lb_num;

			/* reserve on the backing sequential partition since
			 * that partition is credited back later */
			udf_do_reserve_space(ump, udf_node,
				ump->vtop[vpart_num], num_lb);
		}
		break;
	case UDF_ALLOC_SEQUENTIAL :
		/* sequential allocation on recordable media */
		/* get partition backing up this vpart_num_num */
		pdesc = ump->partitions[ump->vtop[vpart_num]];

		/* calculate offset from physical base partition */
		ptov  = udf_rw32(pdesc->start_loc);

		/* get our track descriptors */
		if (vpart_num == ump->node_part) {
			alloc_track = &ump->metadata_track;
			other_track = &ump->data_track;
		} else {
			alloc_track = &ump->data_track;
			other_track = &ump->metadata_track;
		}

		/* allocate */
		for (lb_num = 0; lb_num < num_lb; lb_num++) {
			*lmappos++ = alloc_track->next_writable - ptov;
			alloc_track->next_writable++;
			alloc_track->free_blocks--;
		}

		/* keep other track up-to-date */
		if (alloc_track->tracknr == other_track->tracknr)
			memcpy(other_track, alloc_track,
				sizeof(struct mmc_trackinfo));
		break;
	case UDF_ALLOC_SPACEMAP :
		/* try to allocate on unallocated bits */
		alloc_num_lb = num_lb;
		bitmap = &ump->part_unalloc_bits[vpart_num];
		udf_bitmap_allocate(bitmap, is_node, &alloc_num_lb, lmappos);
		ump->lvclose |= UDF_WRITE_PART_BITMAPS;

		/* have we allocated all? */
		if (alloc_num_lb) {
			/* TODO convert freed to unalloc and try again */
			/* free allocated piece for now */
			lmappos = lmapping;
			for (lb_num=0; lb_num < num_lb-alloc_num_lb; lb_num++) {
				udf_bitmap_free(bitmap, *lmappos++, 1);
			}
			error = ENOSPC;
		}
		if (!error) {
			/* adjust freecount */
			lvid = ump->logvol_integrity;
			freepos = &lvid->tables[0] + vpart_num;
			free_lbs = udf_rw32(*freepos);
			*freepos = udf_rw32(free_lbs - num_lb);
		}
		break;
	case UDF_ALLOC_METABITMAP :		/* UDF 2.50, 2.60 BluRay-RE */
		/* allocate on metadata unallocated bits */
		alloc_num_lb = num_lb;
		bitmap = &ump->metadata_unalloc_bits;
		udf_bitmap_allocate(bitmap, is_node, &alloc_num_lb, lmappos);
		ump->lvclose |= UDF_WRITE_PART_BITMAPS;

		/* have we allocated all? */
		if (alloc_num_lb) {
			/* YIKES! TODO we need to extend the metadata partition */
			/* free allocated piece for now */
			lmappos = lmapping;
			for (lb_num=0; lb_num < num_lb-alloc_num_lb; lb_num++) {
				udf_bitmap_free(bitmap, *lmappos++, 1);
			}
			error = ENOSPC;
		}
		if (!error) {
			/* adjust freecount */
			lvid = ump->logvol_integrity;
			freepos = &lvid->tables[0] + vpart_num;
			free_lbs = udf_rw32(*freepos);
			*freepos = udf_rw32(free_lbs - num_lb);
		}
		break;
	case UDF_ALLOC_METASEQUENTIAL :		/* UDF 2.60       BluRay-R  */
	case UDF_ALLOC_RELAXEDSEQUENTIAL :	/* UDF 2.50/~meta BluRay-R  */
		printf("ALERT: udf_allocate_space : allocation %d "
				"not implemented yet!\n", alloc_type);
		/* TODO implement, doesn't have to be contiguous */
		error = ENOSPC;
		break;
	}

	if (!error) {
		/* credit our partition since we have committed the space */
		udf_do_unreserve_space(ump, udf_node, vpart_num, num_lb);
	}

#ifdef DEBUG
	if (udf_verbose & UDF_DEBUG_ALLOC) {
		lmappos = lmapping;
		printf("udf_allocate_space, allocated logical lba :\n");
		for (lb_num = 0; lb_num < num_lb; lb_num++) {
			printf("%s %"PRIu64, (lb_num > 0)?",":"", 
				*lmappos++);
		}
		printf("\n");
	}
#endif
	mutex_exit(&ump->allocate_mutex);

	return error;
}

/* --------------------------------------------------------------------- */

void
udf_free_allocated_space(struct udf_mount *ump, uint32_t lb_num,
	uint16_t vpart_num, uint32_t num_lb)
{
	struct udf_bitmap *bitmap;
	struct logvol_int_desc *lvid;
	uint32_t lb_map, udf_rw32_lbmap;
	uint32_t *freepos, free_lbs;
	int phys_part;
	int error __diagused;

	DPRINTF(ALLOC, ("udf_free_allocated_space: freeing virt lbnum %d "
			  "part %d + %d sect\n", lb_num, vpart_num, num_lb));

	/* no use freeing zero length */
	if (num_lb == 0)
		return;

	mutex_enter(&ump->allocate_mutex);

	switch (ump->vtop_tp[vpart_num]) {
	case UDF_VTOP_TYPE_PHYS :
	case UDF_VTOP_TYPE_SPARABLE :
		/* free space to freed or unallocated space bitmap */
		phys_part = ump->vtop[vpart_num];

		/* first try freed space bitmap */
		bitmap    = &ump->part_freed_bits[phys_part];

		/* if not defined, use unallocated bitmap */
		if (bitmap->bits == NULL)
			bitmap = &ump->part_unalloc_bits[phys_part];

		/* if no bitmaps are defined, bail out; XXX OK? */
		if (bitmap->bits == NULL)
			break;

		/* free bits if its defined */
		KASSERT(bitmap->bits);
		ump->lvclose |= UDF_WRITE_PART_BITMAPS;
		udf_bitmap_free(bitmap, lb_num, num_lb);

		/* adjust freecount */
		lvid = ump->logvol_integrity;
		freepos = &lvid->tables[0] + vpart_num;
		free_lbs = udf_rw32(*freepos);
		*freepos = udf_rw32(free_lbs + num_lb);
		break;
	case UDF_VTOP_TYPE_VIRT :
		/* free this VAT entry */
		KASSERT(num_lb == 1);

		lb_map = 0xffffffff;
		udf_rw32_lbmap = udf_rw32(lb_map);
		error = udf_vat_write(ump->vat_node,
			(uint8_t *) &udf_rw32_lbmap, 4,
			ump->vat_offset + lb_num * 4);
		KASSERT(error == 0);
		ump->vat_last_free_lb = MIN(ump->vat_last_free_lb, lb_num);
		break;
	case UDF_VTOP_TYPE_META :
		/* free space in the metadata bitmap */
		bitmap = &ump->metadata_unalloc_bits;
		KASSERT(bitmap->bits);

		ump->lvclose |= UDF_WRITE_PART_BITMAPS;
		udf_bitmap_free(bitmap, lb_num, num_lb);

		/* adjust freecount */
		lvid = ump->logvol_integrity;
		freepos = &lvid->tables[0] + vpart_num;
		free_lbs = udf_rw32(*freepos);
		*freepos = udf_rw32(free_lbs + num_lb);
		break;
	default:
		printf("ALERT: udf_free_allocated_space : allocation %d "
			"not implemented yet!\n", ump->vtop_tp[vpart_num]);
		break;
	}

	mutex_exit(&ump->allocate_mutex);
}

/* --------------------------------------------------------------------- */

/*
 * Special function to synchronise the metadatamirror file when they change on
 * resizing. When the metadatafile is actually duplicated, this action is a
 * no-op since they describe different extents on the disc.
 */

void
udf_synchronise_metadatamirror_node(struct udf_mount *ump)
{
	struct udf_node *meta_node, *metamirror_node;
	struct long_ad s_ad;
	uint32_t len, flags;
	int slot, cpy_slot;
	int error, eof;

	if (ump->metadata_flags & METADATA_DUPLICATED)
		return;

	meta_node       = ump->metadata_node;
	metamirror_node = ump->metadatamirror_node;

	/* 1) wipe mirror node */
	udf_wipe_adslots(metamirror_node);

	/* 2) copy all node descriptors from the meta_node */
	slot     = 0;
	cpy_slot = 0;
	for (;;) {
		udf_get_adslot(meta_node, slot, &s_ad, &eof);
		if (eof)
			break;
		len   = udf_rw32(s_ad.len);
		flags = UDF_EXT_FLAGS(len);
		len   = UDF_EXT_LEN(len);

		if (flags == UDF_EXT_REDIRECT) {
			slot++;
			continue;
		}

		error = udf_append_adslot(metamirror_node, &cpy_slot, &s_ad);
		if (error) {
			/* WTF, this shouldn't happen, what to do now? */
			panic("udf_synchronise_metadatamirror_node failed!");
		}
		cpy_slot++;
		slot++;
	}

	/* 3) adjust metamirror_node size */
	if (meta_node->fe) {
		KASSERT(metamirror_node->fe);
		metamirror_node->fe->inf_len = meta_node->fe->inf_len;
	} else {
		KASSERT(meta_node->efe);
		KASSERT(metamirror_node->efe);
		metamirror_node->efe->inf_len  = meta_node->efe->inf_len;
		metamirror_node->efe->obj_size = meta_node->efe->obj_size;
	}

	/* for sanity */
	udf_count_alloc_exts(metamirror_node);
}

/* --------------------------------------------------------------------- */

/*
 * When faced with an out of space but there is still space available on other
 * partitions, try to redistribute the space. This is only defined for media
 * using Metadata partitions.
 *
 * There are two formats to deal with. Either its a `normal' metadata
 * partition and we can move blocks between a metadata bitmap and its
 * companion data spacemap OR its a UDF 2.60 formatted BluRay-R disc with POW
 * and a metadata partition.
 */

/* implementation limit: ump->datapart is the companion partition */
static uint32_t
udf_trunc_metadatapart(struct udf_mount *ump, uint32_t num_lb)
{
	struct udf_node *bitmap_node;
	struct udf_bitmap *bitmap;
	struct space_bitmap_desc *sbd, *new_sbd;
	struct logvol_int_desc *lvid;
	uint64_t inf_len;
	uint64_t meta_free_lbs, data_free_lbs, to_trunc;
	uint32_t *freepos, *sizepos;
	uint32_t unit, lb_size;
	uint16_t meta_vpart_num, data_vpart_num, num_vpart;
	int err __diagused;

	unit = ump->metadata_alloc_unit_size;
	lb_size = udf_rw32(ump->logical_vol->lb_size);
	lvid = ump->logvol_integrity;

	/* XXX
	 *
	 * the following checks will fail for BD-R UDF 2.60! but they are
	 * read-only for now anyway! Its even doubtfull if it is to be allowed
	 * for these discs.
	 */

	/* lookup vpart for metadata partition */
	meta_vpart_num = ump->node_part;
	KASSERT(ump->vtop_alloc[meta_vpart_num] == UDF_ALLOC_METABITMAP);

	/* lookup vpart for data partition */
	data_vpart_num = ump->data_part;
	KASSERT(ump->vtop_alloc[data_vpart_num] == UDF_ALLOC_SPACEMAP);

	udf_calc_vpart_freespace(ump, data_vpart_num, &data_free_lbs);
	udf_calc_vpart_freespace(ump, meta_vpart_num, &meta_free_lbs);

	DPRINTF(RESERVE, ("\tfree space on data partition     %"PRIu64" blks\n", data_free_lbs));
	DPRINTF(RESERVE, ("\tfree space on metadata partition %"PRIu64" blks\n", meta_free_lbs));

	/* give away some of the free meta space, in unit block sizes */
	to_trunc = meta_free_lbs/4;			/* give out a quarter */
	to_trunc = MAX(to_trunc, num_lb);
	to_trunc = unit * ((to_trunc + unit-1) / unit);	/* round up */

	/* scale down if needed and bail out when out of space */
	if (to_trunc >= meta_free_lbs)
		return num_lb;

	/* check extent of bits marked free at the end of the map */
	bitmap = &ump->metadata_unalloc_bits;
	to_trunc = udf_bitmap_check_trunc_free(bitmap, to_trunc);
	to_trunc = unit * (to_trunc / unit);		/* round down again */
	if (to_trunc == 0)
		return num_lb;

	DPRINTF(RESERVE, ("\ttruncating %"PRIu64" lbs from the metadata bitmap\n",
		to_trunc));

	/* get length of the metadata bitmap node file */
	bitmap_node = ump->metadatabitmap_node;
	if (bitmap_node->fe) {
		inf_len = udf_rw64(bitmap_node->fe->inf_len);
	} else {
		KASSERT(bitmap_node->efe);
		inf_len = udf_rw64(bitmap_node->efe->inf_len);
	}
	inf_len -= to_trunc/8;

	/* as per [UDF 2.60/2.2.13.6] : */
	/* 1) update the SBD in the metadata bitmap file */
	sbd = (struct space_bitmap_desc *) bitmap->blob;
	sbd->num_bits  = udf_rw32(udf_rw32(sbd->num_bits)  - to_trunc);
	sbd->num_bytes = udf_rw32(udf_rw32(sbd->num_bytes) - to_trunc/8);
	bitmap->max_offset = udf_rw32(sbd->num_bits);

	num_vpart = udf_rw32(lvid->num_part);
	freepos = &lvid->tables[0] + meta_vpart_num;
	sizepos = &lvid->tables[0] + num_vpart + meta_vpart_num;
	*freepos = udf_rw32(*freepos) - to_trunc;
	*sizepos = udf_rw32(*sizepos) - to_trunc;

	/* realloc bitmap for better memory usage */
	new_sbd = realloc(sbd, inf_len, M_UDFVOLD,
		M_CANFAIL | M_WAITOK);
	if (new_sbd) {
		/* update pointers */
		ump->metadata_unalloc_dscr = new_sbd;
		bitmap->blob = (uint8_t *) new_sbd;
	}
	ump->lvclose |= UDF_WRITE_PART_BITMAPS;

	/*
	 * The truncated space is secured now and can't be allocated anymore.
	 * Release the allocate mutex so we can shrink the nodes the normal
	 * way.
	 */
	mutex_exit(&ump->allocate_mutex);

	/* 2) trunc the metadata bitmap information file, freeing blocks */
	err = udf_shrink_node(bitmap_node, inf_len);
	KASSERT(err == 0);

	/* 3) trunc the metadata file and mirror file, freeing blocks */
	inf_len = (uint64_t) udf_rw32(sbd->num_bits) * lb_size;	/* [4/14.12.4] */
	err = udf_shrink_node(ump->metadata_node, inf_len);
	KASSERT(err == 0);
	if (ump->metadatamirror_node) {
		if (ump->metadata_flags & METADATA_DUPLICATED) {
			err = udf_shrink_node(ump->metadatamirror_node, inf_len);
		} else {
			/* extents will be copied on writeout */
		}
		KASSERT(err == 0);
	}
	ump->lvclose |= UDF_WRITE_METAPART_NODES;

	/* relock before exit */
	mutex_enter(&ump->allocate_mutex);

	if (to_trunc > num_lb)
		return 0;
	return num_lb - to_trunc;
}


static void
udf_sparsify_metadatapart(struct udf_mount *ump, uint32_t num_lb)
{
	/* NOT IMPLEMENTED, fail */
}


static void
udf_collect_free_space_for_vpart(struct udf_mount *ump,
	uint16_t vpart_num, uint32_t num_lb)
{
	/* allocate mutex is helt */

	/* only defined for metadata partitions */
	if (ump->vtop_tp[ump->node_part] != UDF_VTOP_TYPE_META) {
		DPRINTF(RESERVE, ("\tcan't grow/shrink; no metadata partitioning\n"));
		return;
	}

	/* UDF 2.60 BD-R+POW? */
	if (ump->vtop_alloc[ump->node_part] == UDF_ALLOC_METASEQUENTIAL) {
		DPRINTF(RESERVE, ("\tUDF 2.60 BD-R+POW track grow not implemented yet\n"));
		return;
	}

	if (ump->vtop_tp[vpart_num] == UDF_VTOP_TYPE_META) {
		/* try to grow the meta partition */
		DPRINTF(RESERVE, ("\ttrying to grow the meta partition\n"));
		/* as per [UDF 2.60/2.2.13.5] : extend bitmap and metadata file(s) */
		DPRINTF(NOTIMPL, ("\tgrowing meta partition not implemented yet\n"));
	} else {
		/* try to shrink the metadata partition */
		DPRINTF(RESERVE, ("\ttrying to shrink the meta partition\n"));
		/* as per [UDF 2.60/2.2.13.6] : either trunc or make sparse */
		num_lb = udf_trunc_metadatapart(ump, num_lb);
		if (num_lb)
			udf_sparsify_metadatapart(ump, num_lb);
	}

	/* allocate mutex should still be helt */
}

/* --------------------------------------------------------------------- */

/*
 * Allocate a buf on disc for direct write out. The space doesn't have to be
 * contiguous as the caller takes care of this.
 */

void
udf_late_allocate_buf(struct udf_mount *ump, struct buf *buf,
	uint64_t *lmapping, struct long_ad *node_ad_cpy, uint16_t *vpart_nump)
{
	struct udf_node  *udf_node = VTOI(buf->b_vp);
	int lb_size, udf_c_type;
	int vpart_num, num_lb;
	int error, s;

	/*
	 * for each sector in the buf, allocate a sector on disc and record
	 * its position in the provided mapping array.
	 *
	 * If its userdata or FIDs, record its location in its node.
	 */

	lb_size    = udf_rw32(ump->logical_vol->lb_size);
	num_lb     = (buf->b_bcount + lb_size -1) / lb_size;
	udf_c_type = buf->b_udf_c_type;

	KASSERT(lb_size == ump->discinfo.sector_size);

	/* select partition to record the buffer on */
	vpart_num = *vpart_nump = udf_get_record_vpart(ump, udf_c_type);

	if (udf_c_type == UDF_C_NODE) {
		/* if not VAT, its allready allocated */
		if (ump->vtop_alloc[ump->node_part] != UDF_ALLOC_VAT)
			return;

		/* allocate on its backing sequential partition */
		vpart_num = ump->data_part;
	}

	/* XXX can this still happen? */
	/* do allocation on the selected partition */
	error = udf_allocate_space(ump, udf_node, udf_c_type,
			vpart_num, num_lb, lmapping);
	if (error) {
		/*
		 * ARGH! we haven't done our accounting right! it should
		 * allways succeed.
		 */
		panic("UDF disc allocation accounting gone wrong");
	}

	/* If its userdata or FIDs, record its allocation in its node. */
	if ((udf_c_type == UDF_C_USERDATA) ||
	    (udf_c_type == UDF_C_FIDS) ||
	    (udf_c_type == UDF_C_METADATA_SBM))
	{
		udf_record_allocation_in_node(ump, buf, vpart_num, lmapping,
			node_ad_cpy);
		/* decrement our outstanding bufs counter */
		s = splbio();
			udf_node->outstanding_bufs--;
		splx(s);
	}
}

/* --------------------------------------------------------------------- */

/*
 * Try to merge a1 with the new piece a2. udf_ads_merge returns error when not
 * possible (anymore); a2 returns the rest piece.
 */

static int
udf_ads_merge(uint32_t max_len, uint32_t lb_size, struct long_ad *a1, struct long_ad *a2)
{
	uint32_t merge_len;
	uint32_t a1_len, a2_len;
	uint32_t a1_flags, a2_flags;
	uint32_t a1_lbnum, a2_lbnum;
	uint16_t a1_part, a2_part;

	a1_flags = UDF_EXT_FLAGS(udf_rw32(a1->len));
	a1_len   = UDF_EXT_LEN(udf_rw32(a1->len));
	a1_lbnum = udf_rw32(a1->loc.lb_num);
	a1_part  = udf_rw16(a1->loc.part_num);

	a2_flags = UDF_EXT_FLAGS(udf_rw32(a2->len));
	a2_len   = UDF_EXT_LEN(udf_rw32(a2->len));
	a2_lbnum = udf_rw32(a2->loc.lb_num);
	a2_part  = udf_rw16(a2->loc.part_num);

	/* defines same space */
	if (a1_flags != a2_flags)
		return 1;

	if (a1_flags != UDF_EXT_FREE) {
		/* the same partition */
		if (a1_part != a2_part)
			return 1;

		/* a2 is successor of a1 */
		if (a1_lbnum * lb_size + a1_len != a2_lbnum * lb_size)
			return 1;
	}

	/* merge as most from a2 if possible */
	merge_len = MIN(a2_len, max_len - a1_len);
	a1_len   += merge_len;
	a2_len   -= merge_len;
	a2_lbnum += merge_len/lb_size;

	a1->len = udf_rw32(a1_len | a1_flags);
	a2->len = udf_rw32(a2_len | a2_flags);
	a2->loc.lb_num = udf_rw32(a2_lbnum);

	if (a2_len > 0)
		return 1;

	/* there is space over to merge */
	return 0;
}

/* --------------------------------------------------------------------- */

static void
udf_wipe_adslots(struct udf_node *udf_node)
{
	struct file_entry      *fe;
	struct extfile_entry   *efe;
	struct alloc_ext_entry *ext;
	uint32_t lb_size, dscr_size, l_ea, max_l_ad, crclen;
	uint8_t *data_pos;
	int extnr;

	lb_size = udf_rw32(udf_node->ump->logical_vol->lb_size);

	fe  = udf_node->fe;
	efe = udf_node->efe;
	if (fe) {
		dscr_size  = sizeof(struct file_entry) -1;
		l_ea       = udf_rw32(fe->l_ea);
		data_pos = (uint8_t *) fe + dscr_size + l_ea;
	} else {
		dscr_size  = sizeof(struct extfile_entry) -1;
		l_ea       = udf_rw32(efe->l_ea);
		data_pos = (uint8_t *) efe + dscr_size + l_ea;
	}
	max_l_ad = lb_size - dscr_size - l_ea;

	/* wipe fe/efe */
	memset(data_pos, 0, max_l_ad);
	crclen = dscr_size - UDF_DESC_TAG_LENGTH + l_ea;
	if (fe) {
		fe->l_ad         = udf_rw32(0);
		fe->logblks_rec  = udf_rw64(0);
		fe->tag.desc_crc_len = udf_rw16(crclen);
	} else {
		efe->l_ad        = udf_rw32(0);
		efe->logblks_rec = udf_rw64(0);
		efe->tag.desc_crc_len = udf_rw16(crclen);
	}

	/* wipe all allocation extent entries */
	for (extnr = 0; extnr < udf_node->num_extensions; extnr++) {
		ext = udf_node->ext[extnr];
		dscr_size  = sizeof(struct alloc_ext_entry) -1;
		data_pos = (uint8_t *) ext->data;
		max_l_ad = lb_size - dscr_size;
		memset(data_pos, 0, max_l_ad);
		ext->l_ad = udf_rw32(0);

		crclen = dscr_size - UDF_DESC_TAG_LENGTH;
		ext->tag.desc_crc_len = udf_rw16(crclen);
	}
	udf_node->i_flags |= IN_NODE_REBUILD;
}

/* --------------------------------------------------------------------- */

void
udf_get_adslot(struct udf_node *udf_node, int slot, struct long_ad *icb,
	int *eof) {
	struct file_entry      *fe;
	struct extfile_entry   *efe;
	struct alloc_ext_entry *ext;
	struct icb_tag *icbtag;
	struct short_ad *short_ad;
	struct long_ad *long_ad, l_icb;
	uint32_t offset;
	uint32_t dscr_size, l_ea, l_ad, flags;
	uint8_t *data_pos;
	int icbflags, addr_type, adlen, extnr;

	fe  = udf_node->fe;
	efe = udf_node->efe;
	if (fe) {
		icbtag  = &fe->icbtag;
		dscr_size  = sizeof(struct file_entry) -1;
		l_ea       = udf_rw32(fe->l_ea);
		l_ad       = udf_rw32(fe->l_ad);
		data_pos = (uint8_t *) fe + dscr_size + l_ea;
	} else {
		icbtag  = &efe->icbtag;
		dscr_size  = sizeof(struct extfile_entry) -1;
		l_ea       = udf_rw32(efe->l_ea);
		l_ad       = udf_rw32(efe->l_ad);
		data_pos = (uint8_t *) efe + dscr_size + l_ea;
	}

	icbflags  = udf_rw16(icbtag->flags);
	addr_type = icbflags & UDF_ICB_TAG_FLAGS_ALLOC_MASK;

	/* just in case we're called on an intern, its EOF */
	if (addr_type == UDF_ICB_INTERN_ALLOC) {
		memset(icb, 0, sizeof(struct long_ad));
		*eof = 1;
		return;
	}

	adlen = 0;
	if (addr_type == UDF_ICB_SHORT_ALLOC) {
		adlen = sizeof(struct short_ad);
	} else if (addr_type == UDF_ICB_LONG_ALLOC) {
		adlen = sizeof(struct long_ad);
	}

	/* if offset too big, we go to the allocation extensions */
	offset = slot * adlen;
	extnr  = -1;
	while (offset >= l_ad) {
		/* check if our last entry is a redirect */
		if (addr_type == UDF_ICB_SHORT_ALLOC) {
			short_ad = (struct short_ad *) (data_pos + l_ad-adlen);
			l_icb.len          = short_ad->len;
			l_icb.loc.part_num = udf_node->loc.loc.part_num;
			l_icb.loc.lb_num   = short_ad->lb_num;
		} else {
			KASSERT(addr_type == UDF_ICB_LONG_ALLOC);
			long_ad = (struct long_ad *) (data_pos + l_ad-adlen);
			l_icb = *long_ad;
		}
		flags = UDF_EXT_FLAGS(udf_rw32(l_icb.len));
		if (flags != UDF_EXT_REDIRECT) {
			l_ad = 0;	/* force EOF */
			break;
		}

		/* advance to next extent */
		extnr++;
		if (extnr >= udf_node->num_extensions) {
			l_ad = 0;	/* force EOF */
			break;
		}
		offset = offset - l_ad;
		ext  = udf_node->ext[extnr];
		dscr_size  = sizeof(struct alloc_ext_entry) -1;
		l_ad = udf_rw32(ext->l_ad);
		data_pos = (uint8_t *) ext + dscr_size;
	}

	/* XXX l_ad == 0 should be enough to check */
	*eof = (offset >= l_ad) || (l_ad == 0);
	if (*eof) {
		DPRINTF(PARANOIDADWLK, ("returning EOF, extnr %d, offset %d, "
			"l_ad %d\n", extnr, offset, l_ad));
		memset(icb, 0, sizeof(struct long_ad));
		return;
	}

	/* get the element */
	if (addr_type == UDF_ICB_SHORT_ALLOC) {
		short_ad = (struct short_ad *) (data_pos + offset);
		icb->len          = short_ad->len;
		icb->loc.part_num = udf_node->loc.loc.part_num;
		icb->loc.lb_num   = short_ad->lb_num;
	} else if (addr_type == UDF_ICB_LONG_ALLOC) {
		long_ad = (struct long_ad *) (data_pos + offset);
		*icb = *long_ad;
	}
	DPRINTF(PARANOIDADWLK, ("returning element : v %d, lb %d, len %d, "
		"flags %d\n", icb->loc.part_num, icb->loc.lb_num,
		UDF_EXT_LEN(icb->len), UDF_EXT_FLAGS(icb->len)));
}

/* --------------------------------------------------------------------- */

int
udf_append_adslot(struct udf_node *udf_node, int *slot, struct long_ad *icb) {
	struct udf_mount *ump = udf_node->ump;
	union dscrptr          *dscr, *extdscr;
	struct file_entry      *fe;
	struct extfile_entry   *efe;
	struct alloc_ext_entry *ext;
	struct icb_tag *icbtag;
	struct short_ad *short_ad;
	struct long_ad *long_ad, o_icb, l_icb;
	uint64_t logblks_rec, *logblks_rec_p;
	uint64_t lmapping;
	uint32_t offset, rest, len, lb_num;
	uint32_t lb_size, dscr_size, l_ea, l_ad, *l_ad_p, max_l_ad, crclen;
	uint32_t flags;
	uint16_t vpart_num;
	uint8_t *data_pos;
	int icbflags, addr_type, adlen, extnr;
	int error;

	lb_size = udf_rw32(ump->logical_vol->lb_size);
	vpart_num = udf_rw16(udf_node->loc.loc.part_num);

	/* determine what descriptor we are in */
	fe  = udf_node->fe;
	efe = udf_node->efe;
	if (fe) {
		icbtag  = &fe->icbtag;
		dscr      = (union dscrptr *) fe;
		dscr_size = sizeof(struct file_entry) -1;

		l_ea      = udf_rw32(fe->l_ea);
		l_ad_p    = &fe->l_ad;
		logblks_rec_p = &fe->logblks_rec;
	} else {
		icbtag    = &efe->icbtag;
		dscr      = (union dscrptr *) efe;
		dscr_size = sizeof(struct extfile_entry) -1;

		l_ea      = udf_rw32(efe->l_ea);
		l_ad_p    = &efe->l_ad;
		logblks_rec_p = &efe->logblks_rec;
	}
	data_pos  = (uint8_t *) dscr + dscr_size + l_ea;
	max_l_ad = lb_size - dscr_size - l_ea;

	icbflags  = udf_rw16(icbtag->flags);
	addr_type = icbflags & UDF_ICB_TAG_FLAGS_ALLOC_MASK;

	/* just in case we're called on an intern, its EOF */
	if (addr_type == UDF_ICB_INTERN_ALLOC) {
		panic("udf_append_adslot on UDF_ICB_INTERN_ALLOC\n");
	}

	adlen = 0;
	if (addr_type == UDF_ICB_SHORT_ALLOC) {
		adlen = sizeof(struct short_ad);
	} else if (addr_type == UDF_ICB_LONG_ALLOC) {
		adlen = sizeof(struct long_ad);
	}

	/* clean up given long_ad since it can be a synthesized one */
	flags = UDF_EXT_FLAGS(udf_rw32(icb->len));
	if (flags == UDF_EXT_FREE) {
		icb->loc.part_num = udf_rw16(0);
		icb->loc.lb_num   = udf_rw32(0);
	}

	/* if offset too big, we go to the allocation extensions */
	l_ad   = udf_rw32(*l_ad_p);
	offset = (*slot) * adlen;
	extnr  = -1;
	while (offset >= l_ad) {
		/* check if our last entry is a redirect */
		if (addr_type == UDF_ICB_SHORT_ALLOC) {
			short_ad = (struct short_ad *) (data_pos + l_ad-adlen);
			l_icb.len          = short_ad->len;
			l_icb.loc.part_num = udf_node->loc.loc.part_num;
			l_icb.loc.lb_num   = short_ad->lb_num;
		} else {
			KASSERT(addr_type == UDF_ICB_LONG_ALLOC);
			long_ad = (struct long_ad *) (data_pos + l_ad-adlen);
			l_icb = *long_ad;
		}
		flags = UDF_EXT_FLAGS(udf_rw32(l_icb.len));
		if (flags != UDF_EXT_REDIRECT) {
			/* only one past the last one is adressable */
			break;
		}

		/* advance to next extent */
		extnr++;
		KASSERT(extnr < udf_node->num_extensions);
		offset = offset - l_ad;

		ext  = udf_node->ext[extnr];
		dscr = (union dscrptr *) ext;
		dscr_size  = sizeof(struct alloc_ext_entry) -1;
		max_l_ad = lb_size - dscr_size;
		l_ad_p = &ext->l_ad;
		l_ad   = udf_rw32(*l_ad_p);
		data_pos = (uint8_t *) ext + dscr_size;
	}
	DPRINTF(PARANOIDADWLK, ("append, ext %d, offset %d, l_ad %d\n",
		extnr, offset, udf_rw32(*l_ad_p)));
	KASSERT(l_ad == udf_rw32(*l_ad_p));

	/* offset is offset within the current (E)FE/AED */
	l_ad   = udf_rw32(*l_ad_p);
	crclen = udf_rw16(dscr->tag.desc_crc_len);
	logblks_rec = udf_rw64(*logblks_rec_p);

	/* overwriting old piece? */
	if (offset < l_ad) {
		/* overwrite entry; compensate for the old element */
		if (addr_type == UDF_ICB_SHORT_ALLOC) {
			short_ad = (struct short_ad *) (data_pos + offset);
			o_icb.len          = short_ad->len;
			o_icb.loc.part_num = udf_rw16(0);	/* ignore */
			o_icb.loc.lb_num   = short_ad->lb_num;
		} else if (addr_type == UDF_ICB_LONG_ALLOC) {
			long_ad = (struct long_ad *) (data_pos + offset);
			o_icb = *long_ad;
		} else {
			panic("Invalid address type in udf_append_adslot\n");
		}

		len = udf_rw32(o_icb.len);
		if (UDF_EXT_FLAGS(len) == UDF_EXT_ALLOCATED) {
			/* adjust counts */
			len = UDF_EXT_LEN(len);
			logblks_rec -= (len + lb_size -1) / lb_size;
		}
	}

	/* check if we're not appending a redirection */
	flags = UDF_EXT_FLAGS(udf_rw32(icb->len));
	KASSERT(flags != UDF_EXT_REDIRECT);

	/* round down available space */
	rest = adlen * ((max_l_ad - offset) / adlen);
	if (rest <= adlen) {
		/* have to append aed, see if we already have a spare one */
		extnr++;
		ext = udf_node->ext[extnr];
		l_icb = udf_node->ext_loc[extnr];
		if (ext == NULL) {
			DPRINTF(ALLOC,("adding allocation extent %d\n", extnr));

			error = udf_reserve_space(ump, NULL, UDF_C_NODE,
					vpart_num, 1, /* can fail */ false);
			if (error) {
				printf("UDF: couldn't reserve space for AED!\n");
				return error;
			}
			error = udf_allocate_space(ump, NULL, UDF_C_NODE,
					vpart_num, 1, &lmapping);
			lb_num = lmapping;
			if (error)
				panic("UDF: couldn't allocate AED!\n");

			/* initialise pointer to location */
			memset(&l_icb, 0, sizeof(struct long_ad));
			l_icb.len = udf_rw32(lb_size | UDF_EXT_REDIRECT);
			l_icb.loc.lb_num   = udf_rw32(lb_num);
			l_icb.loc.part_num = udf_rw16(vpart_num);

			/* create new aed descriptor */
			udf_create_logvol_dscr(ump, udf_node, &l_icb, &extdscr);
			ext = &extdscr->aee;

			udf_inittag(ump, &ext->tag, TAGID_ALLOCEXTENT, lb_num);
			dscr_size  = sizeof(struct alloc_ext_entry) -1;
			max_l_ad = lb_size - dscr_size;
			memset(ext->data, 0, max_l_ad);
			ext->l_ad = udf_rw32(0);
			ext->tag.desc_crc_len =
				udf_rw16(dscr_size - UDF_DESC_TAG_LENGTH);

			/* declare aed */
			udf_node->num_extensions++;
			udf_node->ext_loc[extnr] = l_icb;
			udf_node->ext[extnr] = ext;
		}
		/* add redirect and adjust l_ad and crclen for old descr */
		if (addr_type == UDF_ICB_SHORT_ALLOC) {
			short_ad = (struct short_ad *) (data_pos + offset);
			short_ad->len    = l_icb.len;
			short_ad->lb_num = l_icb.loc.lb_num;
		} else if (addr_type == UDF_ICB_LONG_ALLOC) {
			long_ad = (struct long_ad *) (data_pos + offset);
			*long_ad = l_icb;
		}
		l_ad   += adlen;
		crclen += adlen;
		dscr->tag.desc_crc_len = udf_rw16(crclen);
		*l_ad_p = udf_rw32(l_ad);

		/* advance to the new extension */
		KASSERT(ext != NULL);
		dscr = (union dscrptr *) ext;
		dscr_size  = sizeof(struct alloc_ext_entry) -1;
		max_l_ad = lb_size - dscr_size;
		data_pos = (uint8_t *) dscr + dscr_size;

		l_ad_p = &ext->l_ad;
		l_ad   = udf_rw32(*l_ad_p);
		crclen = udf_rw16(dscr->tag.desc_crc_len);
		offset = 0;

		/* adjust callees slot count for link insert */
		*slot += 1;
	}

	/* write out the element */
	DPRINTF(PARANOIDADWLK, ("adding element : %p : v %d, lb %d, "
			"len %d, flags %d\n", data_pos + offset,
			icb->loc.part_num, icb->loc.lb_num,
			UDF_EXT_LEN(icb->len), UDF_EXT_FLAGS(icb->len)));
	if (addr_type == UDF_ICB_SHORT_ALLOC) {
		short_ad = (struct short_ad *) (data_pos + offset);
		short_ad->len    = icb->len;
		short_ad->lb_num = icb->loc.lb_num;
	} else if (addr_type == UDF_ICB_LONG_ALLOC) {
		long_ad = (struct long_ad *) (data_pos + offset);
		*long_ad = *icb;
	}

	/* adjust logblks recorded count */
	len = udf_rw32(icb->len);
	flags = UDF_EXT_FLAGS(len);
	if (flags == UDF_EXT_ALLOCATED)
		logblks_rec += (UDF_EXT_LEN(len) + lb_size -1) / lb_size;
	*logblks_rec_p = udf_rw64(logblks_rec);

	/* adjust l_ad and crclen when needed */
	if (offset >= l_ad) {
		l_ad   += adlen;
		crclen += adlen;
		dscr->tag.desc_crc_len = udf_rw16(crclen);
		*l_ad_p = udf_rw32(l_ad);
	}

	return 0;
}

/* --------------------------------------------------------------------- */

static void
udf_count_alloc_exts(struct udf_node *udf_node)
{
	struct long_ad s_ad;
	uint32_t lb_num, len, flags;
	uint16_t vpart_num;
	int slot, eof;
	int num_extents, extnr;

	if (udf_node->num_extensions == 0)
		return;

	/* count number of allocation extents in use */
	num_extents = 0;
	slot = 0;
	for (;;) {
		udf_get_adslot(udf_node, slot, &s_ad, &eof);
		if (eof)
			break;
		len   = udf_rw32(s_ad.len);
		flags = UDF_EXT_FLAGS(len);

		if (flags == UDF_EXT_REDIRECT)
			num_extents++;

		slot++;
	}

	DPRINTF(ALLOC, ("udf_count_alloc_ext counted %d live extents\n",
		num_extents));

	/* XXX choice: we could delay freeing them on node writeout */
	/* free excess entries */
	extnr = num_extents;
	for (;extnr < udf_node->num_extensions; extnr++) {
		DPRINTF(ALLOC, ("freeing alloc ext %d\n", extnr));
		/* free dscriptor */
		s_ad = udf_node->ext_loc[extnr];
		udf_free_logvol_dscr(udf_node->ump, &s_ad,
			udf_node->ext[extnr]);
		udf_node->ext[extnr] = NULL;

		/* free disc space */
		lb_num    = udf_rw32(s_ad.loc.lb_num);
		vpart_num = udf_rw16(s_ad.loc.part_num);
		udf_free_allocated_space(udf_node->ump, lb_num, vpart_num, 1);

		memset(&udf_node->ext_loc[extnr], 0, sizeof(struct long_ad));
	}

	/* set our new number of allocation extents */
	udf_node->num_extensions = num_extents;
}


/* --------------------------------------------------------------------- */

/*
 * Adjust the node's allocation descriptors to reflect the new mapping; do
 * take note that we might glue to existing allocation descriptors.
 *
 * XXX Note there can only be one allocation being recorded/mount; maybe
 * explicit allocation in shedule thread?
 */

static void
udf_record_allocation_in_node(struct udf_mount *ump, struct buf *buf,
	uint16_t vpart_num, uint64_t *mapping, struct long_ad *node_ad_cpy)
{
	struct vnode    *vp = buf->b_vp;
	struct udf_node *udf_node = VTOI(vp);
	struct file_entry      *fe;
	struct extfile_entry   *efe;
	struct icb_tag  *icbtag;
	struct long_ad   s_ad, c_ad;
	uint64_t inflen, from, till;
	uint64_t foffset, end_foffset, restart_foffset;
	uint64_t orig_inflen, orig_lbrec, new_inflen, new_lbrec;
	uint32_t max_len;
	uint32_t num_lb, len, flags, lb_num;
	uint32_t run_start;
	uint32_t slot_offset, replace_len, replace;
	int addr_type, icbflags;
//	int udf_c_type = buf->b_udf_c_type;
	int lb_size, run_length, eof;
	int slot, cpy_slot, cpy_slots, restart_slot;
	int error;

	DPRINTF(ALLOC, ("udf_record_allocation_in_node\n"));

#if 0
	/* XXX disable sanity check for now */
	/* sanity check ... should be panic ? */
	if ((udf_c_type != UDF_C_USERDATA) && (udf_c_type != UDF_C_FIDS))
		return;
#endif

	lb_size = udf_rw32(udf_node->ump->logical_vol->lb_size);
	max_len = ((UDF_EXT_MAXLEN / lb_size) * lb_size);

	/* do the job */
	UDF_LOCK_NODE(udf_node, 0);	/* XXX can deadlock ? */
	udf_node_sanity_check(udf_node, &orig_inflen, &orig_lbrec);

	fe  = udf_node->fe;
	efe = udf_node->efe;
	if (fe) {
		icbtag = &fe->icbtag;
		inflen = udf_rw64(fe->inf_len);
	} else {
		icbtag = &efe->icbtag;
		inflen = udf_rw64(efe->inf_len);
	}

	/* do check if `till' is not past file information length */
	from = buf->b_lblkno * lb_size;
	till = MIN(inflen, from + buf->b_resid);

	num_lb = (till - from + lb_size -1) / lb_size;

	DPRINTF(ALLOC, ("record allocation from %"PRIu64" + %d\n", from, buf->b_bcount));

	icbflags  = udf_rw16(icbtag->flags);
	addr_type = icbflags & UDF_ICB_TAG_FLAGS_ALLOC_MASK;

	if (addr_type == UDF_ICB_INTERN_ALLOC) {
		/* nothing to do */
		/* XXX clean up rest of node? just in case? */
		UDF_UNLOCK_NODE(udf_node, 0);
		return;
	}

	slot     = 0;
	cpy_slot = 0;
	foffset  = 0;

	/* 1) copy till first overlap piece to the rewrite buffer */
	for (;;) {
		udf_get_adslot(udf_node, slot, &s_ad, &eof);
		if (eof) {
			DPRINTF(WRITE,
				("Record allocation in node "
				 "failed: encountered EOF\n"));
			UDF_UNLOCK_NODE(udf_node, 0);
			buf->b_error = EINVAL;
			return;
		}
		len   = udf_rw32(s_ad.len);
		flags = UDF_EXT_FLAGS(len);
		len   = UDF_EXT_LEN(len);

		if (flags == UDF_EXT_REDIRECT) {
			slot++;
			continue;
		}

		end_foffset = foffset + len;
		if (end_foffset > from)
			break;	/* found */

		node_ad_cpy[cpy_slot++] = s_ad;

		DPRINTF(ALLOC, ("\t1: vp %d, lb %d, len %d, flags %d "
			"-> stack\n",
			udf_rw16(s_ad.loc.part_num),
			udf_rw32(s_ad.loc.lb_num),
			UDF_EXT_LEN(udf_rw32(s_ad.len)),
			UDF_EXT_FLAGS(udf_rw32(s_ad.len)) >> 30));

		foffset = end_foffset;
		slot++;
	}
	restart_slot    = slot;
	restart_foffset = foffset;

	/* 2) trunc overlapping slot at overlap and copy it */
	slot_offset = from - foffset;
	if (slot_offset > 0) {
		DPRINTF(ALLOC, ("\tslot_offset = %d, flags = %d (%d)\n",
				slot_offset, flags >> 30, flags));

		s_ad.len = udf_rw32(slot_offset | flags);
		node_ad_cpy[cpy_slot++] = s_ad;

		DPRINTF(ALLOC, ("\t2: vp %d, lb %d, len %d, flags %d "
			"-> stack\n",
			udf_rw16(s_ad.loc.part_num),
			udf_rw32(s_ad.loc.lb_num),
			UDF_EXT_LEN(udf_rw32(s_ad.len)),
			UDF_EXT_FLAGS(udf_rw32(s_ad.len)) >> 30));
	}
	foffset += slot_offset;

	/* 3) insert new mappings */
	memset(&s_ad, 0, sizeof(struct long_ad));
	lb_num = 0;
	for (lb_num = 0; lb_num < num_lb; lb_num++) {
		run_start  = mapping[lb_num];
		run_length = 1;
		while (lb_num < num_lb-1) {
			if (mapping[lb_num+1] != mapping[lb_num]+1)
				if (mapping[lb_num+1] != mapping[lb_num])
					break;
			run_length++;
			lb_num++;
		}
		/* insert slot for this mapping */
		len = run_length * lb_size;

		/* bounds checking */
		if (foffset + len > till)
			len = till - foffset;
		KASSERT(foffset + len <= inflen);

		s_ad.len = udf_rw32(len | UDF_EXT_ALLOCATED);
		s_ad.loc.part_num = udf_rw16(vpart_num);
		s_ad.loc.lb_num   = udf_rw32(run_start);

		foffset += len;

		/* paranoia */
		if (len == 0) {
			DPRINTF(WRITE,
				("Record allocation in node "
				 "failed: insert failed\n"));
			UDF_UNLOCK_NODE(udf_node, 0);
			buf->b_error = EINVAL;
			return;
		}
		node_ad_cpy[cpy_slot++] = s_ad;

		DPRINTF(ALLOC, ("\t3: insert new mapping vp %d lb %d, len %d, "
				"flags %d -> stack\n",
			udf_rw16(s_ad.loc.part_num), udf_rw32(s_ad.loc.lb_num),
			UDF_EXT_LEN(udf_rw32(s_ad.len)),
			UDF_EXT_FLAGS(udf_rw32(s_ad.len)) >> 30));
	}

	/* 4) pop replaced length */
	slot    = restart_slot;
	foffset = restart_foffset;

	replace_len = till - foffset;	/* total amount of bytes to pop */
	slot_offset = from - foffset;	/* offset in first encounted slot */
	KASSERT((slot_offset % lb_size) == 0);

	for (;;) {
		udf_get_adslot(udf_node, slot, &s_ad, &eof);
		if (eof)
			break;

		len    = udf_rw32(s_ad.len);
		flags  = UDF_EXT_FLAGS(len);
		len    = UDF_EXT_LEN(len);
		lb_num = udf_rw32(s_ad.loc.lb_num);

		if (flags == UDF_EXT_REDIRECT) {
			slot++;
			continue;
		}

		DPRINTF(ALLOC, ("\t4i: got slot %d, slot_offset %d, "
				"replace_len %d, "
				"vp %d, lb %d, len %d, flags %d\n",
			slot, slot_offset, replace_len,
			udf_rw16(s_ad.loc.part_num),
			udf_rw32(s_ad.loc.lb_num),
			UDF_EXT_LEN(udf_rw32(s_ad.len)),
			UDF_EXT_FLAGS(udf_rw32(s_ad.len)) >> 30));

		/* adjust for slot offset */
		if (slot_offset) {
			DPRINTF(ALLOC, ("\t4s: skipping %d\n", slot_offset));
			lb_num += slot_offset / lb_size;
			len    -= slot_offset;
			foffset += slot_offset;
			replace_len -= slot_offset;

			/* mark adjusted */
			slot_offset = 0;
		}

		/* advance for (the rest of) this slot */
		replace = MIN(len, replace_len);
		DPRINTF(ALLOC, ("\t4d: replacing %d\n", replace));

		/* advance for this slot */
		if (replace) {
			/* note: dont round DOWN on num_lb since we then
			 * forget the last partial one */
			num_lb = (replace + lb_size - 1) / lb_size;
			if (flags != UDF_EXT_FREE) {
				udf_free_allocated_space(ump, lb_num,
					udf_rw16(s_ad.loc.part_num), num_lb);
			}
			lb_num      += num_lb;
			len         -= replace;
			foffset     += replace;
			replace_len -= replace;
		}

		/* do we have a slot tail ? */
		if (len) {
			KASSERT(foffset % lb_size == 0);

			/* we arrived at our point, push remainder */
			s_ad.len        = udf_rw32(len | flags);
			s_ad.loc.lb_num = udf_rw32(lb_num);
			if (flags == UDF_EXT_FREE)
				s_ad.loc.lb_num = udf_rw32(0);
			node_ad_cpy[cpy_slot++] = s_ad;
			foffset += len;
			slot++;

			DPRINTF(ALLOC, ("\t4: vp %d, lb %d, len %d, flags %d "
				"-> stack\n",
				udf_rw16(s_ad.loc.part_num),
				udf_rw32(s_ad.loc.lb_num),
				UDF_EXT_LEN(udf_rw32(s_ad.len)),
				UDF_EXT_FLAGS(udf_rw32(s_ad.len)) >> 30));
			break;
		}

		slot++;
	}

	/* 5) copy remainder */
	for (;;) {
		udf_get_adslot(udf_node, slot, &s_ad, &eof);
		if (eof)
			break;

		len   = udf_rw32(s_ad.len);
		flags = UDF_EXT_FLAGS(len);
		len   = UDF_EXT_LEN(len);

		if (flags == UDF_EXT_REDIRECT) {
			slot++;
			continue;
		}

		node_ad_cpy[cpy_slot++] = s_ad;

		DPRINTF(ALLOC, ("\t5: insert new mapping "
			"vp %d lb %d, len %d, flags %d "
			"-> stack\n",
		udf_rw16(s_ad.loc.part_num),
		udf_rw32(s_ad.loc.lb_num),
		UDF_EXT_LEN(udf_rw32(s_ad.len)),
		UDF_EXT_FLAGS(udf_rw32(s_ad.len)) >> 30));

		slot++;
	}

	/* 6) reset node descriptors */
	udf_wipe_adslots(udf_node);

	/* 7) copy back extents; merge when possible. Recounting on the fly */
	cpy_slots = cpy_slot;

	c_ad = node_ad_cpy[0];
	slot = 0;
	DPRINTF(ALLOC, ("\t7s: stack -> got mapping vp %d "
		"lb %d, len %d, flags %d\n",
	udf_rw16(c_ad.loc.part_num),
	udf_rw32(c_ad.loc.lb_num),
	UDF_EXT_LEN(udf_rw32(c_ad.len)),
	UDF_EXT_FLAGS(udf_rw32(c_ad.len)) >> 30));

	for (cpy_slot = 1; cpy_slot < cpy_slots; cpy_slot++) {
		s_ad = node_ad_cpy[cpy_slot];

		DPRINTF(ALLOC, ("\t7i: stack -> got mapping vp %d "
			"lb %d, len %d, flags %d\n",
		udf_rw16(s_ad.loc.part_num),
		udf_rw32(s_ad.loc.lb_num),
		UDF_EXT_LEN(udf_rw32(s_ad.len)),
		UDF_EXT_FLAGS(udf_rw32(s_ad.len)) >> 30));

		/* see if we can merge */
		if (udf_ads_merge(max_len, lb_size, &c_ad, &s_ad)) {
			/* not mergable (anymore) */
			DPRINTF(ALLOC, ("\t7: appending vp %d lb %d, "
				"len %d, flags %d\n",
			udf_rw16(c_ad.loc.part_num),
			udf_rw32(c_ad.loc.lb_num),
			UDF_EXT_LEN(udf_rw32(c_ad.len)),
			UDF_EXT_FLAGS(udf_rw32(c_ad.len)) >> 30));

			error = udf_append_adslot(udf_node, &slot, &c_ad);
			if (error) {
				buf->b_error = error;
				goto out;
			}
			c_ad = s_ad;
			slot++;
		}
	}

	/* 8) push rest slot (if any) */
	if (UDF_EXT_LEN(c_ad.len) > 0) {
		DPRINTF(ALLOC, ("\t8: last append vp %d lb %d, "
				"len %d, flags %d\n",
		udf_rw16(c_ad.loc.part_num),
		udf_rw32(c_ad.loc.lb_num),
		UDF_EXT_LEN(udf_rw32(c_ad.len)),
		UDF_EXT_FLAGS(udf_rw32(c_ad.len)) >> 30));

		error = udf_append_adslot(udf_node, &slot, &c_ad);
		if (error) {
			buf->b_error = error;
			goto out;
		}
	}

out:
	udf_count_alloc_exts(udf_node);

	/* the node's descriptors should now be sane */
	udf_node_sanity_check(udf_node, &new_inflen, &new_lbrec);
	UDF_UNLOCK_NODE(udf_node, 0);

	KASSERT(orig_inflen == new_inflen);
	KASSERT(new_lbrec >= orig_lbrec);

	return;
}

/* --------------------------------------------------------------------- */

int
udf_grow_node(struct udf_node *udf_node, uint64_t new_size)
{
	struct vnode *vp = udf_node->vnode;
	struct udf_mount *ump = udf_node->ump;
	struct file_entry    *fe;
	struct extfile_entry *efe;
	struct icb_tag  *icbtag;
	struct long_ad c_ad, s_ad;
	uint64_t size_diff, old_size, inflen, objsize, chunk, append_len;
	uint64_t foffset, end_foffset;
	uint64_t orig_inflen, orig_lbrec, new_inflen, new_lbrec;
	uint32_t lb_size, unit_size, dscr_size, crclen, lastblock_grow;
	uint32_t icbflags, len, flags, max_len;
	uint32_t max_l_ad, l_ad, l_ea;
	uint16_t my_part, dst_part;
	uint8_t *evacuated_data;
	int addr_type;
	int slot;
	int eof, error;

	DPRINTF(ALLOC, ("udf_grow_node\n"));

	UDF_LOCK_NODE(udf_node, 0);
	udf_node_sanity_check(udf_node, &orig_inflen, &orig_lbrec);

	lb_size = udf_rw32(ump->logical_vol->lb_size);

	/* max_len in unit's IFF its a metadata node or metadata mirror node */
	unit_size = lb_size;
	if ((udf_node == ump->metadata_node) || (udf_node == ump->metadatamirror_node))
		unit_size = ump->metadata_alloc_unit_size * lb_size;
	max_len = ((UDF_EXT_MAXLEN / unit_size) * unit_size);

	fe  = udf_node->fe;
	efe = udf_node->efe;
	if (fe) {
		icbtag  = &fe->icbtag;
		inflen  = udf_rw64(fe->inf_len);
		objsize = inflen;
		dscr_size  = sizeof(struct file_entry) -1;
		l_ea       = udf_rw32(fe->l_ea);
		l_ad       = udf_rw32(fe->l_ad);
	} else {
		icbtag  = &efe->icbtag;
		inflen  = udf_rw64(efe->inf_len);
		objsize = udf_rw64(efe->obj_size);
		dscr_size  = sizeof(struct extfile_entry) -1;
		l_ea       = udf_rw32(efe->l_ea);
		l_ad       = udf_rw32(efe->l_ad);
	}
	max_l_ad = lb_size - dscr_size - l_ea;

	icbflags   = udf_rw16(icbtag->flags);
	addr_type  = icbflags & UDF_ICB_TAG_FLAGS_ALLOC_MASK;

	old_size  = inflen;
	size_diff = new_size - old_size;

	DPRINTF(ALLOC, ("\tfrom %"PRIu64" to %"PRIu64"\n", old_size, new_size));

	evacuated_data = NULL;
	if (addr_type == UDF_ICB_INTERN_ALLOC) {
		if (l_ad + size_diff <= max_l_ad) {
			/* only reflect size change directly in the node */
			inflen  += size_diff;
			objsize += size_diff;
			l_ad    += size_diff;
			crclen = dscr_size - UDF_DESC_TAG_LENGTH + l_ea + l_ad;
			if (fe) {
				fe->inf_len   = udf_rw64(inflen);
				fe->l_ad      = udf_rw32(l_ad);
				fe->tag.desc_crc_len = udf_rw16(crclen);
			} else {
				efe->inf_len  = udf_rw64(inflen);
				efe->obj_size = udf_rw64(objsize);
				efe->l_ad     = udf_rw32(l_ad);
				efe->tag.desc_crc_len = udf_rw16(crclen);
			}
			error = 0;

			/* set new size for uvm */
			uvm_vnp_setwritesize(vp, new_size);
			uvm_vnp_setsize(vp, new_size);

#if 0
			/* zero append space in buffer */
			ubc_zerorange(&vp->v_uobj, old_size,
			    new_size - old_size, UBC_UNMAP_FLAG(vp));
#endif
	
			udf_node_sanity_check(udf_node, &new_inflen, &new_lbrec);

			/* unlock */
			UDF_UNLOCK_NODE(udf_node, 0);

			KASSERT(new_inflen == orig_inflen + size_diff);
			KASSERT(new_lbrec == orig_lbrec);
			KASSERT(new_lbrec == 0);
			return 0;
		}

		DPRINTF(ALLOC, ("\tCONVERT from internal\n"));

		if (old_size > 0) {
			/* allocate some space and copy in the stuff to keep */
			evacuated_data = malloc(lb_size, M_UDFTEMP, M_WAITOK);
			memset(evacuated_data, 0, lb_size);

			/* node is locked, so safe to exit mutex */
			UDF_UNLOCK_NODE(udf_node, 0);

			/* read in using the `normal' vn_rdwr() */
			error = vn_rdwr(UIO_READ, udf_node->vnode,
					evacuated_data, old_size, 0, 
					UIO_SYSSPACE, IO_ALTSEMANTICS | IO_NODELOCKED,
					FSCRED, NULL, NULL);

			/* enter again */
			UDF_LOCK_NODE(udf_node, 0);
		}

		/* convert to a normal alloc and select type */
		my_part  = udf_rw16(udf_node->loc.loc.part_num);
		dst_part = udf_get_record_vpart(ump, udf_get_c_type(udf_node));
		addr_type = UDF_ICB_SHORT_ALLOC;
		if (dst_part != my_part)
			addr_type = UDF_ICB_LONG_ALLOC;

		icbflags &= ~UDF_ICB_TAG_FLAGS_ALLOC_MASK;
		icbflags |= addr_type;
		icbtag->flags = udf_rw16(icbflags);

		/* wipe old descriptor space */
		udf_wipe_adslots(udf_node);

		memset(&c_ad, 0, sizeof(struct long_ad));
		c_ad.len          = udf_rw32(old_size | UDF_EXT_FREE);
		c_ad.loc.part_num = udf_rw16(0); /* not relevant */
		c_ad.loc.lb_num   = udf_rw32(0); /* not relevant */

		slot = 0;
	} else {
		/* goto the last entry (if any) */
		slot     = 0;
		foffset  = 0;
		memset(&c_ad, 0, sizeof(struct long_ad));
		for (;;) {
			udf_get_adslot(udf_node, slot, &c_ad, &eof);
			if (eof)
				break;

			len   = udf_rw32(c_ad.len);
			flags = UDF_EXT_FLAGS(len);
			len   = UDF_EXT_LEN(len);

			end_foffset = foffset + len;
			if (flags != UDF_EXT_REDIRECT)
				foffset = end_foffset;

			slot++;
		}
		/* at end of adslots */

		/* special case if the old size was zero, then there is no last slot */
		if (old_size == 0) {
			c_ad.len          = udf_rw32(0 | UDF_EXT_FREE);
			c_ad.loc.part_num = udf_rw16(0); /* not relevant */
			c_ad.loc.lb_num   = udf_rw32(0); /* not relevant */
		} else {
			/* refetch last slot */
			slot--;
			udf_get_adslot(udf_node, slot, &c_ad, &eof);
		}
	}

	/*
	 * If the length of the last slot is not a multiple of lb_size, adjust
	 * length so that it is; don't forget to adjust `append_len'! relevant for
	 * extending existing files
	 */
	len   = udf_rw32(c_ad.len);
	flags = UDF_EXT_FLAGS(len);
	len   = UDF_EXT_LEN(len);

	lastblock_grow = 0;
	if (len % lb_size > 0) {
		lastblock_grow = lb_size - (len % lb_size);
		lastblock_grow = MIN(size_diff, lastblock_grow);
		len += lastblock_grow;
		c_ad.len = udf_rw32(len | flags);

		/* TODO zero appened space in buffer! */
		/* using ubc_zerorange(&vp->v_uobj, old_size, */
		/*    new_size - old_size, UBC_UNMAP_FLAG(vp)); ? */
	}
	memset(&s_ad, 0, sizeof(struct long_ad));

	/* size_diff can be bigger than allowed, so grow in chunks */
	append_len = size_diff - lastblock_grow;
	while (append_len > 0) {
		chunk = MIN(append_len, max_len);
		s_ad.len = udf_rw32(chunk | UDF_EXT_FREE);
		s_ad.loc.part_num = udf_rw16(0);
		s_ad.loc.lb_num   = udf_rw32(0);

		if (udf_ads_merge(max_len, lb_size, &c_ad, &s_ad)) {
			/* not mergable (anymore) */
			error = udf_append_adslot(udf_node, &slot, &c_ad);
			if (error)
				goto errorout;
			slot++;
			c_ad = s_ad;
			memset(&s_ad, 0, sizeof(struct long_ad));
		}
		append_len -= chunk;
	}

	/* if there is a rest piece in the accumulator, append it */
	if (UDF_EXT_LEN(udf_rw32(c_ad.len)) > 0) {
		error = udf_append_adslot(udf_node, &slot, &c_ad);
		if (error)
			goto errorout;
		slot++;
	}

	/* if there is a rest piece that didn't fit, append it */
	if (UDF_EXT_LEN(udf_rw32(s_ad.len)) > 0) {
		error = udf_append_adslot(udf_node, &slot, &s_ad);
		if (error)
			goto errorout;
		slot++;
	}

	inflen  += size_diff;
	objsize += size_diff;
	if (fe) {
		fe->inf_len   = udf_rw64(inflen);
	} else {
		efe->inf_len  = udf_rw64(inflen);
		efe->obj_size = udf_rw64(objsize);
	}
	error = 0;

	if (evacuated_data) {
		/* set new write size for uvm */
		uvm_vnp_setwritesize(vp, old_size);

		/* write out evacuated data */
		error = vn_rdwr(UIO_WRITE, udf_node->vnode,
				evacuated_data, old_size, 0, 
				UIO_SYSSPACE, IO_ALTSEMANTICS | IO_NODELOCKED,
				FSCRED, NULL, NULL);
		uvm_vnp_setsize(vp, old_size);
	}

errorout:
	if (evacuated_data)
		free(evacuated_data, M_UDFTEMP);

	udf_count_alloc_exts(udf_node);

	udf_node_sanity_check(udf_node, &new_inflen, &new_lbrec);
	UDF_UNLOCK_NODE(udf_node, 0);

	KASSERT(new_inflen == orig_inflen + size_diff);
	KASSERT(new_lbrec == orig_lbrec);

	return error;
}

/* --------------------------------------------------------------------- */

int
udf_shrink_node(struct udf_node *udf_node, uint64_t new_size)
{
	struct vnode *vp = udf_node->vnode;
	struct udf_mount *ump = udf_node->ump;
	struct file_entry    *fe;
	struct extfile_entry *efe;
	struct icb_tag  *icbtag;
	struct long_ad c_ad, s_ad, *node_ad_cpy;
	uint64_t size_diff, old_size, inflen, objsize;
	uint64_t foffset, end_foffset;
	uint64_t orig_inflen, orig_lbrec, new_inflen, new_lbrec;
	uint32_t lb_size, unit_size, dscr_size, crclen;
	uint32_t slot_offset, slot_offset_lb;
	uint32_t len, flags, max_len;
	uint32_t num_lb, lb_num;
	uint32_t max_l_ad, l_ad, l_ea;
	uint16_t vpart_num;
	uint8_t *data_pos;
	int icbflags, addr_type;
	int slot, cpy_slot, cpy_slots;
	int eof, error;

	DPRINTF(ALLOC, ("udf_shrink_node\n"));

	UDF_LOCK_NODE(udf_node, 0);
	udf_node_sanity_check(udf_node, &orig_inflen, &orig_lbrec);

	lb_size = udf_rw32(ump->logical_vol->lb_size);

	/* max_len in unit's IFF its a metadata node or metadata mirror node */
	unit_size = lb_size;
	if ((udf_node == ump->metadata_node) || (udf_node == ump->metadatamirror_node))
		unit_size = ump->metadata_alloc_unit_size * lb_size;
	max_len = ((UDF_EXT_MAXLEN / unit_size) * unit_size);

	/* do the work */
	fe  = udf_node->fe;
	efe = udf_node->efe;
	if (fe) {
		icbtag  = &fe->icbtag;
		inflen  = udf_rw64(fe->inf_len);
		objsize = inflen;
		dscr_size  = sizeof(struct file_entry) -1;
		l_ea       = udf_rw32(fe->l_ea);
		l_ad       = udf_rw32(fe->l_ad);
		data_pos = (uint8_t *) fe + dscr_size + l_ea;
	} else {
		icbtag  = &efe->icbtag;
		inflen  = udf_rw64(efe->inf_len);
		objsize = udf_rw64(efe->obj_size);
		dscr_size  = sizeof(struct extfile_entry) -1;
		l_ea       = udf_rw32(efe->l_ea);
		l_ad       = udf_rw32(efe->l_ad);
		data_pos = (uint8_t *) efe + dscr_size + l_ea;
	}
	max_l_ad = lb_size - dscr_size - l_ea;

	icbflags   = udf_rw16(icbtag->flags);
	addr_type  = icbflags & UDF_ICB_TAG_FLAGS_ALLOC_MASK;

	old_size  = inflen;
	size_diff = old_size - new_size;

	DPRINTF(ALLOC, ("\tfrom %"PRIu64" to %"PRIu64"\n", old_size, new_size));

	/* shrink the node to its new size */
	if (addr_type == UDF_ICB_INTERN_ALLOC) {
		/* only reflect size change directly in the node */
		KASSERT(new_size <= max_l_ad);
		inflen  -= size_diff;
		objsize -= size_diff;
		l_ad    -= size_diff;
		crclen = dscr_size - UDF_DESC_TAG_LENGTH + l_ea + l_ad;
		if (fe) {
			fe->inf_len   = udf_rw64(inflen);
			fe->l_ad      = udf_rw32(l_ad);
			fe->tag.desc_crc_len = udf_rw16(crclen);
		} else {
			efe->inf_len  = udf_rw64(inflen);
			efe->obj_size = udf_rw64(objsize);
			efe->l_ad     = udf_rw32(l_ad);
			efe->tag.desc_crc_len = udf_rw16(crclen);
		}
		error = 0;

		/* clear the space in the descriptor */
		KASSERT(old_size >= new_size);
		memset(data_pos + new_size, 0, old_size - new_size);

		/* TODO zero appened space in buffer! */
		/* using ubc_zerorange(&vp->v_uobj, old_size, */
		/*    old_size - new_size, UBC_UNMAP_FLAG(vp)); ? */

		/* set new size for uvm */
		uvm_vnp_setsize(vp, new_size);

		udf_node_sanity_check(udf_node, &new_inflen, &new_lbrec);
		UDF_UNLOCK_NODE(udf_node, 0);

		KASSERT(new_inflen == orig_inflen - size_diff);
		KASSERT(new_lbrec == orig_lbrec);
		KASSERT(new_lbrec == 0);

		return 0;
	}

	/* setup node cleanup extents copy space */
	node_ad_cpy = malloc(lb_size * UDF_MAX_ALLOC_EXTENTS,
		M_UDFMNT, M_WAITOK);
	memset(node_ad_cpy, 0, lb_size * UDF_MAX_ALLOC_EXTENTS);

	/*
	 * Shrink the node by releasing the allocations and truncate the last
	 * allocation to the new size. If the new size fits into the
	 * allocation descriptor itself, transform it into an
	 * UDF_ICB_INTERN_ALLOC.
	 */
	slot     = 0;
	cpy_slot = 0;
	foffset  = 0;

	/* 1) copy till first overlap piece to the rewrite buffer */
	for (;;) {
		udf_get_adslot(udf_node, slot, &s_ad, &eof);
		if (eof) {
			DPRINTF(WRITE,
				("Shrink node failed: "
				 "encountered EOF\n"));
			error = EINVAL;
			goto errorout; /* panic? */
		}
		len   = udf_rw32(s_ad.len);
		flags = UDF_EXT_FLAGS(len);
		len   = UDF_EXT_LEN(len);

		if (flags == UDF_EXT_REDIRECT) {
			slot++;
			continue;
		}

		end_foffset = foffset + len;
		if (end_foffset > new_size)
			break;	/* found */

		node_ad_cpy[cpy_slot++] = s_ad;

		DPRINTF(ALLOC, ("\t1: vp %d, lb %d, len %d, flags %d "
			"-> stack\n",
			udf_rw16(s_ad.loc.part_num),
			udf_rw32(s_ad.loc.lb_num),
			UDF_EXT_LEN(udf_rw32(s_ad.len)),
			UDF_EXT_FLAGS(udf_rw32(s_ad.len)) >> 30));

		foffset = end_foffset;
		slot++;
	}
	slot_offset = new_size - foffset;

	/* 2) trunc overlapping slot at overlap and copy it */
	if (slot_offset > 0) {
		lb_num    = udf_rw32(s_ad.loc.lb_num);
		vpart_num = udf_rw16(s_ad.loc.part_num);

		if (flags == UDF_EXT_ALLOCATED) {
			/* calculate extent in lb, and offset in lb */
			num_lb = (len + lb_size -1) / lb_size;
			slot_offset_lb = (slot_offset + lb_size -1) / lb_size;

			/* adjust our slot */
			lb_num += slot_offset_lb;
			num_lb -= slot_offset_lb;

			udf_free_allocated_space(ump, lb_num, vpart_num, num_lb);
		}

		s_ad.len = udf_rw32(slot_offset | flags);
		node_ad_cpy[cpy_slot++] = s_ad;
		slot++;

		DPRINTF(ALLOC, ("\t2: vp %d, lb %d, len %d, flags %d "
			"-> stack\n",
			udf_rw16(s_ad.loc.part_num),
			udf_rw32(s_ad.loc.lb_num),
			UDF_EXT_LEN(udf_rw32(s_ad.len)),
			UDF_EXT_FLAGS(udf_rw32(s_ad.len)) >> 30));
	}

	/* 3) delete remainder */
	for (;;) {
		udf_get_adslot(udf_node, slot, &s_ad, &eof);
		if (eof)
			break;

		len       = udf_rw32(s_ad.len);
		flags     = UDF_EXT_FLAGS(len);
		len       = UDF_EXT_LEN(len);

		if (flags == UDF_EXT_REDIRECT) {
			slot++;
			continue;
		}

		DPRINTF(ALLOC, ("\t3: delete remainder "
			"vp %d lb %d, len %d, flags %d\n",
		udf_rw16(s_ad.loc.part_num),
		udf_rw32(s_ad.loc.lb_num),
		UDF_EXT_LEN(udf_rw32(s_ad.len)),
		UDF_EXT_FLAGS(udf_rw32(s_ad.len)) >> 30));

		if (flags == UDF_EXT_ALLOCATED) {
			lb_num    = udf_rw32(s_ad.loc.lb_num);
			vpart_num = udf_rw16(s_ad.loc.part_num);
			num_lb    = (len + lb_size - 1) / lb_size;

			udf_free_allocated_space(ump, lb_num, vpart_num,
				num_lb);
		}

		slot++;
	}

	/* 4) if it will fit into the descriptor then convert */
	if (new_size < max_l_ad) {
		/*
		 * resque/evacuate old piece by reading it in, and convert it
		 * to internal alloc.
		 */
		if (new_size == 0) {
			/* XXX/TODO only for zero sizing now */
			udf_wipe_adslots(udf_node);

			icbflags &= ~UDF_ICB_TAG_FLAGS_ALLOC_MASK;
			icbflags |=  UDF_ICB_INTERN_ALLOC;
			icbtag->flags = udf_rw16(icbflags);

			inflen  -= size_diff;	KASSERT(inflen == 0);
			objsize -= size_diff;
			l_ad     = new_size;
			crclen = dscr_size - UDF_DESC_TAG_LENGTH + l_ea + l_ad;
			if (fe) {
				fe->inf_len   = udf_rw64(inflen);
				fe->l_ad      = udf_rw32(l_ad);
				fe->tag.desc_crc_len = udf_rw16(crclen);
			} else {
				efe->inf_len  = udf_rw64(inflen);
				efe->obj_size = udf_rw64(objsize);
				efe->l_ad     = udf_rw32(l_ad);
				efe->tag.desc_crc_len = udf_rw16(crclen);
			}
			/* eventually copy in evacuated piece */
			/* set new size for uvm */
			uvm_vnp_setsize(vp, new_size);

			free(node_ad_cpy, M_UDFMNT);
			udf_node_sanity_check(udf_node, &new_inflen, &new_lbrec);

			UDF_UNLOCK_NODE(udf_node, 0);

			KASSERT(new_inflen == orig_inflen - size_diff);
			KASSERT(new_inflen == 0);
			KASSERT(new_lbrec == 0);

			return 0;
		}

		printf("UDF_SHRINK_NODE: could convert to internal alloc!\n");
	}

	/* 5) reset node descriptors */
	udf_wipe_adslots(udf_node);

	/* 6) copy back extents; merge when possible. Recounting on the fly */
	cpy_slots = cpy_slot;

	c_ad = node_ad_cpy[0];
	slot = 0;
	for (cpy_slot = 1; cpy_slot < cpy_slots; cpy_slot++) {
		s_ad = node_ad_cpy[cpy_slot];

		DPRINTF(ALLOC, ("\t6: stack -> got mapping vp %d "
			"lb %d, len %d, flags %d\n",
		udf_rw16(s_ad.loc.part_num),
		udf_rw32(s_ad.loc.lb_num),
		UDF_EXT_LEN(udf_rw32(s_ad.len)),
		UDF_EXT_FLAGS(udf_rw32(s_ad.len)) >> 30));

		/* see if we can merge */
		if (udf_ads_merge(max_len, lb_size, &c_ad, &s_ad)) {
			/* not mergable (anymore) */
			DPRINTF(ALLOC, ("\t6: appending vp %d lb %d, "
				"len %d, flags %d\n",
			udf_rw16(c_ad.loc.part_num),
			udf_rw32(c_ad.loc.lb_num),
			UDF_EXT_LEN(udf_rw32(c_ad.len)),
			UDF_EXT_FLAGS(udf_rw32(c_ad.len)) >> 30));

			error = udf_append_adslot(udf_node, &slot, &c_ad);
			if (error)
				goto errorout; /* panic? */
			c_ad = s_ad;
			slot++;
		}
	}

	/* 7) push rest slot (if any) */
	if (UDF_EXT_LEN(c_ad.len) > 0) {
		DPRINTF(ALLOC, ("\t7: last append vp %d lb %d, "
				"len %d, flags %d\n",
		udf_rw16(c_ad.loc.part_num),
		udf_rw32(c_ad.loc.lb_num),
		UDF_EXT_LEN(udf_rw32(c_ad.len)),
		UDF_EXT_FLAGS(udf_rw32(c_ad.len)) >> 30));

		error = udf_append_adslot(udf_node, &slot, &c_ad);
		if (error)
			goto errorout; /* panic? */
		;
	}

	inflen  -= size_diff;
	objsize -= size_diff;
	if (fe) {
		fe->inf_len   = udf_rw64(inflen);
	} else {
		efe->inf_len  = udf_rw64(inflen);
		efe->obj_size = udf_rw64(objsize);
	}
	error = 0;

	/* set new size for uvm */
	uvm_vnp_setsize(vp, new_size);

errorout:
	free(node_ad_cpy, M_UDFMNT);

	udf_count_alloc_exts(udf_node);

	udf_node_sanity_check(udf_node, &new_inflen, &new_lbrec);
	UDF_UNLOCK_NODE(udf_node, 0);

	KASSERT(new_inflen == orig_inflen - size_diff);

	return error;
}


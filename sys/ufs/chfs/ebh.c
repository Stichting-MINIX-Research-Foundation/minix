/*	$NetBSD: ebh.c,v 1.6 2015/02/07 04:21:11 christos Exp $	*/

/*-
 * Copyright (c) 2010 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * Copyright (C) 2009 Ferenc Havasi <havasi@inf.u-szeged.hu>
 * Copyright (C) 2009 Zoltan Sogor <weth@inf.u-szeged.hu>
 * Copyright (C) 2009 David Tengeri <dtengeri@inf.u-szeged.hu>
 * Copyright (C) 2009 Tamas Toth <ttoth@inf.u-szeged.hu>
 * Copyright (C) 2010 Adam Hoka <ahoka@NetBSD.org>
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

#include "ebh.h"

/*****************************************************************************/
/* Flash specific operations						     */
/*****************************************************************************/
int nor_create_eb_hdr(struct chfs_eb_hdr *ebhdr, int lnr);
int nand_create_eb_hdr(struct chfs_eb_hdr *ebhdr, int lnr);
int nor_calc_data_offs(struct chfs_ebh *ebh, int pebnr, int offset);
int nand_calc_data_offs(struct chfs_ebh *ebh, int pebnr, int offset);
int nor_read_eb_hdr(struct chfs_ebh *ebh, int pebnr, struct chfs_eb_hdr *ebhdr);
int nand_read_eb_hdr(struct chfs_ebh *ebh, int pebnr, struct chfs_eb_hdr *ebhdr);
int nor_write_eb_hdr(struct chfs_ebh *ebh, int pebnr, struct chfs_eb_hdr *ebhdr);
int nand_write_eb_hdr(struct chfs_ebh *ebh, int pebnr,struct chfs_eb_hdr *ebhdr);
int nor_check_eb_hdr(struct chfs_ebh *ebh, void *buf);
int nand_check_eb_hdr(struct chfs_ebh *ebh, void *buf);
int nor_mark_eb_hdr_dirty_flash(struct chfs_ebh *ebh, int pebnr, int lid);
int nor_invalidate_eb_hdr(struct chfs_ebh *ebh, int pebnr);
int mark_eb_hdr_free(struct chfs_ebh *ebh, int pebnr, int ec);

int ltree_entry_cmp(struct chfs_ltree_entry *le1, struct chfs_ltree_entry *le2);
int peb_in_use_cmp(struct chfs_peb *peb1, struct chfs_peb *peb2);
int peb_free_cmp(struct chfs_peb *peb1, struct chfs_peb *peb2);
int add_peb_to_erase_queue(struct chfs_ebh *ebh, int pebnr, int ec,struct peb_queue *queue);
struct chfs_peb * find_peb_in_use(struct chfs_ebh *ebh, int pebnr);
int add_peb_to_free(struct chfs_ebh *ebh, int pebnr, int ec);
int add_peb_to_in_use(struct chfs_ebh *ebh, int pebnr, int ec);
void erase_callback(struct flash_erase_instruction *ei);
int free_peb(struct chfs_ebh *ebh);
int release_peb(struct chfs_ebh *ebh, int pebnr);
void erase_thread(void *data);
static void erase_thread_start(struct chfs_ebh *ebh);
static void erase_thread_stop(struct chfs_ebh *ebh);
int scan_leb_used_cmp(struct chfs_scan_leb *sleb1, struct chfs_scan_leb *sleb2);
int nor_scan_add_to_used(struct chfs_ebh *ebh, struct chfs_scan_info *si,struct chfs_eb_hdr *ebhdr, int pebnr, int leb_status);
int nor_process_eb(struct chfs_ebh *ebh, struct chfs_scan_info *si,
    int pebnr, struct chfs_eb_hdr *ebhdr);
int nand_scan_add_to_used(struct chfs_ebh *ebh, struct chfs_scan_info *si,struct chfs_eb_hdr *ebhdr, int pebnr);
int nand_process_eb(struct chfs_ebh *ebh, struct chfs_scan_info *si,
    int pebnr, struct chfs_eb_hdr *ebhdr);
struct chfs_scan_info *chfs_scan(struct chfs_ebh *ebh);
void scan_info_destroy(struct chfs_scan_info *si);
int scan_media(struct chfs_ebh *ebh);
int get_peb(struct chfs_ebh *ebh);
/**
 * nor_create_eb_hdr - creates an eraseblock header for NOR flash
 * @ebhdr: ebhdr to set
 * @lnr: LEB number
 */
int
nor_create_eb_hdr(struct chfs_eb_hdr *ebhdr, int lnr)
{
	ebhdr->u.nor_hdr.lid = htole32(lnr);
	return 0;
}

/**
 * nand_create_eb_hdr - creates an eraseblock header for NAND flash
 * @ebhdr: ebhdr to set
 * @lnr: LEB number
 */
int
nand_create_eb_hdr(struct chfs_eb_hdr *ebhdr, int lnr)
{
	ebhdr->u.nand_hdr.lid = htole32(lnr);
	return 0;
}

/**
 * nor_calc_data_offs - calculates data offset on NOR flash
 * @ebh: chfs eraseblock handler
 * @pebnr: eraseblock number
 * @offset: offset within the eraseblock
 */
int
nor_calc_data_offs(struct chfs_ebh *ebh, int pebnr, int offset)
{
	return pebnr * ebh->flash_if->erasesize + offset +
	    CHFS_EB_EC_HDR_SIZE + CHFS_EB_HDR_NOR_SIZE;
}

/**
 * nand_calc_data_offs - calculates data offset on NAND flash
 * @ebh: chfs eraseblock handler
 * @pebnr: eraseblock number
 * @offset: offset within the eraseblock
 */
int
nand_calc_data_offs(struct chfs_ebh *ebh, int pebnr, int offset)
{
	return pebnr * ebh->flash_if->erasesize + offset +
	    2 * ebh->flash_if->page_size;
}

/**
 * nor_read_eb_hdr - read ereaseblock header from NOR flash
 *
 * @ebh: chfs eraseblock handler
 * @pebnr: eraseblock number
 * @ebhdr: whereto store the data
 *
 * Reads the eraseblock header from media.
 * Returns zero in case of success, error code in case of fail.
 */
int
nor_read_eb_hdr(struct chfs_ebh *ebh,
    int pebnr, struct chfs_eb_hdr *ebhdr)
{
	int ret;
	size_t retlen;
	off_t ofs = pebnr * ebh->flash_if->erasesize;

	KASSERT(pebnr >= 0 && pebnr < ebh->peb_nr);

	ret = flash_read(ebh->flash_dev,
	    ofs, CHFS_EB_EC_HDR_SIZE,
	    &retlen, (unsigned char *) &ebhdr->ec_hdr);

	if (ret || retlen != CHFS_EB_EC_HDR_SIZE)
		return ret;

	ofs += CHFS_EB_EC_HDR_SIZE;
	ret = flash_read(ebh->flash_dev,
	    ofs, CHFS_EB_HDR_NOR_SIZE,
	    &retlen, (unsigned char *) &ebhdr->u.nor_hdr);

	if (ret || retlen != CHFS_EB_HDR_NOR_SIZE)
		return ret;

	return 0;
}

/**
 * nand_read_eb_hdr - read ereaseblock header from NAND flash
 *
 * @ebh: chfs eraseblock handler
 * @pebnr: eraseblock number
 * @ebhdr: whereto store the data
 *
 * Reads the eraseblock header from media. It is on the first two page.
 * Returns zero in case of success, error code in case of fail.
 */
int
nand_read_eb_hdr(struct chfs_ebh *ebh, int pebnr,
    struct chfs_eb_hdr *ebhdr)
{
	int ret;
	size_t retlen;
	off_t ofs;

	KASSERT(pebnr >= 0 && pebnr < ebh->peb_nr);

	/* Read erase counter header from the first page. */
	ofs = pebnr * ebh->flash_if->erasesize;
	ret = flash_read(ebh->flash_dev,
	    ofs, CHFS_EB_EC_HDR_SIZE, &retlen,
	    (unsigned char *) &ebhdr->ec_hdr);
	if (ret || retlen != CHFS_EB_EC_HDR_SIZE)
		return ret;

	/* Read NAND eraseblock header from the second page */
	ofs += ebh->flash_if->page_size;
	ret = flash_read(ebh->flash_dev,
	    ofs, CHFS_EB_HDR_NAND_SIZE, &retlen,
	    (unsigned char *) &ebhdr->u.nand_hdr);
	if (ret || retlen != CHFS_EB_HDR_NAND_SIZE)
		return ret;

	return 0;
}

/**
 * nor_write_eb_hdr - write ereaseblock header to NOR flash
 *
 * @ebh: chfs eraseblock handler
 * @pebnr: eraseblock number whereto write
 * @ebh: ebh to write
 *
 * Writes the eraseblock header to media.
 * Returns zero in case of success, error code in case of fail.
 */
int
nor_write_eb_hdr(struct chfs_ebh *ebh, int pebnr, struct chfs_eb_hdr *ebhdr)
{
	int ret, crc;
	size_t retlen;

	off_t ofs = pebnr * ebh->flash_if->erasesize + CHFS_EB_EC_HDR_SIZE;

	ebhdr->u.nor_hdr.lid = ebhdr->u.nor_hdr.lid
	    | htole32(CHFS_LID_NOT_DIRTY_BIT);

	crc = crc32(0, (uint8_t *)&ebhdr->u.nor_hdr + 4,
	    CHFS_EB_HDR_NOR_SIZE - 4);
	ebhdr->u.nand_hdr.crc = htole32(crc);

	KASSERT(pebnr >= 0 && pebnr < ebh->peb_nr);

	ret = flash_write(ebh->flash_dev,
	    ofs, CHFS_EB_HDR_NOR_SIZE, &retlen,
	    (unsigned char *) &ebhdr->u.nor_hdr);

	if (ret || retlen != CHFS_EB_HDR_NOR_SIZE)
		return ret;

	return 0;
}

/**
 * nand_write_eb_hdr - write ereaseblock header to NAND flash
 *
 * @ebh: chfs eraseblock handler
 * @pebnr: eraseblock number whereto write
 * @ebh: ebh to write
 *
 * Writes the eraseblock header to media.
 * Returns zero in case of success, error code in case of fail.
 */
int
nand_write_eb_hdr(struct chfs_ebh *ebh, int pebnr,
    struct chfs_eb_hdr *ebhdr)
{
	int ret, crc;
	size_t retlen;
	flash_off_t ofs;

	KASSERT(pebnr >= 0 && pebnr < ebh->peb_nr);

	ofs = pebnr * ebh->flash_if->erasesize +
	    ebh->flash_if->page_size;

	ebhdr->u.nand_hdr.serial = htole64(++(*ebh->max_serial));

	crc = crc32(0, (uint8_t *)&ebhdr->u.nand_hdr + 4,
	    CHFS_EB_HDR_NAND_SIZE - 4);
	ebhdr->u.nand_hdr.crc = htole32(crc);

	ret = flash_write(ebh->flash_dev, ofs,
	    CHFS_EB_HDR_NAND_SIZE, &retlen,
	    (unsigned char *) &ebhdr->u.nand_hdr);

	if (ret || retlen != CHFS_EB_HDR_NAND_SIZE)
		return ret;

	return 0;
}

/**
 * nor_check_eb_hdr - check ereaseblock header read from NOR flash
 *
 * @ebh: chfs eraseblock handler
 * @buf: eraseblock header to check
 *
 * Returns eraseblock header status.
 */
int
nor_check_eb_hdr(struct chfs_ebh *ebh, void *buf)
{
	uint32_t magic, crc, hdr_crc;
	struct chfs_eb_hdr *ebhdr = buf;
	le32 lid_save;

	//check is there a header
	if (check_pattern((void *) &ebhdr->ec_hdr,
		0xFF, 0, CHFS_EB_EC_HDR_SIZE)) {
		dbg_ebh("no header found\n");
		return EBHDR_LEB_NO_HDR;
	}

	// check magic
	magic = le32toh(ebhdr->ec_hdr.magic);
	if (magic != CHFS_MAGIC_BITMASK) {
		dbg_ebh("bad magic bitmask(exp: %x found %x)\n",
		    CHFS_MAGIC_BITMASK, magic);
		return EBHDR_LEB_BADMAGIC;
	}

	// check CRC_EC
	hdr_crc = le32toh(ebhdr->ec_hdr.crc_ec);
	crc = crc32(0, (uint8_t *) &ebhdr->ec_hdr + 8, 4);
	if (hdr_crc != crc) {
		dbg_ebh("bad crc_ec found\n");
		return EBHDR_LEB_BADCRC;
	}

	/* check if the PEB is free: magic, crc_ec and erase_cnt is good and
	 * everything else is FFF..
	 */
	if (check_pattern((void *) &ebhdr->u.nor_hdr, 0xFF, 0,
		CHFS_EB_HDR_NOR_SIZE)) {
		dbg_ebh("free peb found\n");
		return EBHDR_LEB_FREE;
	}

	// check invalidated (CRC == LID == 0)
	if (ebhdr->u.nor_hdr.crc == 0 && ebhdr->u.nor_hdr.lid == 0) {
		dbg_ebh("invalidated ebhdr found\n");
		return EBHDR_LEB_INVALIDATED;
	}

	// check CRC
	hdr_crc = le32toh(ebhdr->u.nor_hdr.crc);
	lid_save = ebhdr->u.nor_hdr.lid;

	// mark lid as not dirty for crc calc
	ebhdr->u.nor_hdr.lid = ebhdr->u.nor_hdr.lid | htole32(
		CHFS_LID_NOT_DIRTY_BIT);
	crc = crc32(0, (uint8_t *) &ebhdr->u.nor_hdr + 4,
	    CHFS_EB_HDR_NOR_SIZE - 4);
	// restore the original lid value in ebh
	ebhdr->u.nor_hdr.lid = lid_save;

	if (crc != hdr_crc) {
		dbg_ebh("bad crc found\n");
		return EBHDR_LEB_BADCRC;
	}

	// check dirty
	if (!(le32toh(lid_save) & CHFS_LID_NOT_DIRTY_BIT)) {
		dbg_ebh("dirty ebhdr found\n");
		return EBHDR_LEB_DIRTY;
	}

	return EBHDR_LEB_OK;
}

/**
 * nand_check_eb_hdr - check ereaseblock header read from NAND flash
 *
 * @ebh: chfs eraseblock handler
 * @buf: eraseblock header to check
 *
 * Returns eraseblock header status.
 */
int
nand_check_eb_hdr(struct chfs_ebh *ebh, void *buf)
{
	uint32_t magic, crc, hdr_crc;
	struct chfs_eb_hdr *ebhdr = buf;

	//check is there a header
	if (check_pattern((void *) &ebhdr->ec_hdr,
		0xFF, 0, CHFS_EB_EC_HDR_SIZE)) {
		dbg_ebh("no header found\n");
		return EBHDR_LEB_NO_HDR;
	}

	// check magic
	magic = le32toh(ebhdr->ec_hdr.magic);
	if (magic != CHFS_MAGIC_BITMASK) {
		dbg_ebh("bad magic bitmask(exp: %x found %x)\n",
		    CHFS_MAGIC_BITMASK, magic);
		return EBHDR_LEB_BADMAGIC;
	}

	// check CRC_EC
	hdr_crc = le32toh(ebhdr->ec_hdr.crc_ec);
	crc = crc32(0, (uint8_t *) &ebhdr->ec_hdr + 8, 4);
	if (hdr_crc != crc) {
		dbg_ebh("bad crc_ec found\n");
		return EBHDR_LEB_BADCRC;
	}

	/* check if the PEB is free: magic, crc_ec and erase_cnt is good and
	 * everything else is FFF..
	 */
	if (check_pattern((void *) &ebhdr->u.nand_hdr, 0xFF, 0,
		CHFS_EB_HDR_NAND_SIZE)) {
		dbg_ebh("free peb found\n");
		return EBHDR_LEB_FREE;
	}

	// check CRC
	hdr_crc = le32toh(ebhdr->u.nand_hdr.crc);

	crc = crc32(0, (uint8_t *) &ebhdr->u.nand_hdr + 4,
	    CHFS_EB_HDR_NAND_SIZE - 4);

	if (crc != hdr_crc) {
		dbg_ebh("bad crc found\n");
		return EBHDR_LEB_BADCRC;
	}

	return EBHDR_LEB_OK;
}

/**
 * nor_mark_eb_hdr_dirty_flash- mark ereaseblock header dirty on NOR flash
 *
 * @ebh: chfs eraseblock handler
 * @pebnr: eraseblock number
 * @lid: leb id (its bit number 31 will be set to 0)
 *
 * It pulls the CHFS_LID_NOT_DIRTY_BIT to zero on flash.
 *
 * Returns zero in case of success, error code in case of fail.
 */
int
nor_mark_eb_hdr_dirty_flash(struct chfs_ebh *ebh, int pebnr, int lid)
{
	int ret;
	size_t retlen;
	off_t ofs;

	/* mark leb id dirty */
	lid = htole32(lid & CHFS_LID_DIRTY_BIT_MASK);

	/* calculate position */
	ofs = pebnr * ebh->flash_if->erasesize + CHFS_EB_EC_HDR_SIZE
	    + CHFS_GET_MEMBER_POS(struct chfs_nor_eb_hdr , lid);

	ret = flash_write(ebh->flash_dev, ofs, sizeof(lid), &retlen,
	    (unsigned char *) &lid);
	if (ret || retlen != sizeof(lid)) {
		chfs_err("can't mark peb dirty");
		return ret;
	}

	return 0;
}

/**
 * nor_invalidate_eb_hdr - invalidate ereaseblock header on NOR flash
 *
 * @ebh: chfs eraseblock handler
 * @pebnr: eraseblock number
 *
 * Sets crc and lip field to zero.
 * Returns zero in case of success, error code in case of fail.
 */
int
nor_invalidate_eb_hdr(struct chfs_ebh *ebh, int pebnr)
{
	int ret;
	size_t retlen;
	off_t ofs;
	char zero_buf[CHFS_INVALIDATE_SIZE];

	/* fill with zero */
	memset(zero_buf, 0x0, CHFS_INVALIDATE_SIZE);

	/* calculate position (!!! lid is directly behind crc !!!) */
	ofs = pebnr * ebh->flash_if->erasesize + CHFS_EB_EC_HDR_SIZE
	    + CHFS_GET_MEMBER_POS(struct chfs_nor_eb_hdr, crc);

	ret = flash_write(ebh->flash_dev,
	    ofs, CHFS_INVALIDATE_SIZE, &retlen,
	    (unsigned char *) &zero_buf);
	if (ret || retlen != CHFS_INVALIDATE_SIZE) {
		chfs_err("can't invalidate peb");
		return ret;
	}

	return 0;
}

/**
 * mark_eb_hdr_free - free ereaseblock header on NOR or NAND flash
 *
 * @ebh: chfs eraseblock handler
 * @pebnr: eraseblock number
 * @ec: erase counter of PEB
 *
 * Write out the magic and erase counter to the physical eraseblock.
 * Returns zero in case of success, error code in case of fail.
 */
int
mark_eb_hdr_free(struct chfs_ebh *ebh, int pebnr, int ec)
{
	int ret, crc;
	size_t retlen;
	off_t ofs;
	struct chfs_eb_hdr *ebhdr;
	ebhdr = kmem_alloc(sizeof(struct chfs_eb_hdr), KM_SLEEP);

	ebhdr->ec_hdr.magic = htole32(CHFS_MAGIC_BITMASK);
	ebhdr->ec_hdr.erase_cnt = htole32(ec);
	crc = crc32(0, (uint8_t *) &ebhdr->ec_hdr + 8, 4);
	ebhdr->ec_hdr.crc_ec = htole32(crc);

	ofs = pebnr * ebh->flash_if->erasesize;

	KASSERT(sizeof(ebhdr->ec_hdr) == CHFS_EB_EC_HDR_SIZE);

	ret = flash_write(ebh->flash_dev,
	    ofs, CHFS_EB_EC_HDR_SIZE, &retlen,
	    (unsigned char *) &ebhdr->ec_hdr);

	if (ret || retlen != CHFS_EB_EC_HDR_SIZE) {
		chfs_err("can't mark peb as free: %d\n", pebnr);
		kmem_free(ebhdr, sizeof(struct chfs_eb_hdr));
		return ret;
	}

	kmem_free(ebhdr, sizeof(struct chfs_eb_hdr));
	return 0;
}

/*****************************************************************************/
/* End of Flash specific operations					     */
/*****************************************************************************/

/*****************************************************************************/
/* Lock Tree								     */
/*****************************************************************************/

int
ltree_entry_cmp(struct chfs_ltree_entry *le1,
    struct chfs_ltree_entry *le2)
{
	return (le1->lnr - le2->lnr);
}

/* Generate functions for Lock tree's red-black tree */
RB_PROTOTYPE( ltree_rbtree, chfs_ltree_entry, rb, ltree_entry_cmp);
RB_GENERATE( ltree_rbtree, chfs_ltree_entry, rb, ltree_entry_cmp);


/**
 * ltree_lookup - looks up a logical eraseblock in the lock tree
 * @ebh: chfs eraseblock handler
 * @lid: identifier of the logical eraseblock
 *
 * This function returns a pointer to the wanted &struct chfs_ltree_entry
 * if the logical eraseblock is in the lock tree, so it is locked, NULL
 * otherwise.
 * @ebh->ltree_lock has to be locked!
 */
static struct chfs_ltree_entry *
ltree_lookup(struct chfs_ebh *ebh, int lnr)
{
	struct chfs_ltree_entry le, *result;
	le.lnr = lnr;
	result = RB_FIND(ltree_rbtree, &ebh->ltree, &le);
	return result;
}

/**
 * ltree_add_entry - add an entry to the lock tree
 * @ebh: chfs eraseblock handler
 * @lnr: identifier of the logical eraseblock
 *
 * This function adds a new logical eraseblock entry identified with @lnr to the
 * lock tree. If the entry is already in the tree, it increases the user
 * counter.
 * Returns NULL if can not allocate memory for lock tree entry, or a pointer
 * to the inserted entry otherwise.
 */
static struct chfs_ltree_entry *
ltree_add_entry(struct chfs_ebh *ebh, int lnr)
{
	struct chfs_ltree_entry *le, *result;

	le = kmem_alloc(sizeof(struct chfs_ltree_entry), KM_SLEEP);

	le->lnr = lnr;
	le->users = 1;
	rw_init(&le->mutex);

	//dbg_ebh("enter ltree lock\n");
	mutex_enter(&ebh->ltree_lock);
	//dbg_ebh("insert\n");
	result = RB_INSERT(ltree_rbtree, &ebh->ltree, le);
	//dbg_ebh("inserted\n");
	if (result) {
		//The entry is already in the tree
		result->users++;
		kmem_free(le, sizeof(struct chfs_ltree_entry));
	}
	else {
		result = le;
	}
	mutex_exit(&ebh->ltree_lock);

	return result;
}

/**
 * leb_read_lock - lock a logical eraseblock for read
 * @ebh: chfs eraseblock handler
 * @lnr: identifier of the logical eraseblock
 *
 * Returns zero in case of success, error code in case of fail.
 */
static int
leb_read_lock(struct chfs_ebh *ebh, int lnr)
{
	struct chfs_ltree_entry *le;

	le = ltree_add_entry(ebh, lnr);
	if (!le)
		return ENOMEM;

	rw_enter(&le->mutex, RW_READER);
	return 0;
}

/**
 * leb_read_unlock - unlock a logical eraseblock from read
 * @ebh: chfs eraseblock handler
 * @lnr: identifier of the logical eraseblock
 *
 * This function unlocks a logical eraseblock from read and delete it from the
 * lock tree is there are no more users of it.
 */
static void
leb_read_unlock(struct chfs_ebh *ebh, int lnr)
{
	struct chfs_ltree_entry *le;

	mutex_enter(&ebh->ltree_lock);
	//dbg_ebh("LOCK: ebh->ltree_lock spin locked in leb_read_unlock()\n");
	le = ltree_lookup(ebh, lnr);
	if (!le)
		goto out;

	le->users -= 1;
	KASSERT(le->users >= 0);
	rw_exit(&le->mutex);
	if (le->users == 0) {
		le = RB_REMOVE(ltree_rbtree, &ebh->ltree, le);
		if (le) {
			KASSERT(!rw_lock_held(&le->mutex));
			rw_destroy(&le->mutex);

			kmem_free(le, sizeof(struct chfs_ltree_entry));
		}
	}

out:
	mutex_exit(&ebh->ltree_lock);
	//dbg_ebh("UNLOCK: ebh->ltree_lock spin unlocked in leb_read_unlock()\n");
}

/**
 * leb_write_lock - lock a logical eraseblock for write
 * @ebh: chfs eraseblock handler
 * @lnr: identifier of the logical eraseblock
 *
 * Returns zero in case of success, error code in case of fail.
 */
static int
leb_write_lock(struct chfs_ebh *ebh, int lnr)
{
	struct chfs_ltree_entry *le;

	le = ltree_add_entry(ebh, lnr);
	if (!le)
		return ENOMEM;

	rw_enter(&le->mutex, RW_WRITER);
	return 0;
}

/**
 * leb_write_unlock - unlock a logical eraseblock from write
 * @ebh: chfs eraseblock handler
 * @lnr: identifier of the logical eraseblock
 *
 * This function unlocks a logical eraseblock from write and delete it from the
 * lock tree is there are no more users of it.
 */
static void
leb_write_unlock(struct chfs_ebh *ebh, int lnr)
{
	struct chfs_ltree_entry *le;

	mutex_enter(&ebh->ltree_lock);
	//dbg_ebh("LOCK: ebh->ltree_lock spin locked in leb_write_unlock()\n");
	le = ltree_lookup(ebh, lnr);
	if (!le)
		goto out;

	le->users -= 1;
	KASSERT(le->users >= 0);
	rw_exit(&le->mutex);
	if (le->users == 0) {
		RB_REMOVE(ltree_rbtree, &ebh->ltree, le);

		KASSERT(!rw_lock_held(&le->mutex));
		rw_destroy(&le->mutex);

		kmem_free(le, sizeof(struct chfs_ltree_entry));
	}

out:
	mutex_exit(&ebh->ltree_lock);
	//dbg_ebh("UNLOCK: ebh->ltree_lock spin unlocked in leb_write_unlock()\n");
}

/*****************************************************************************/
/* End of Lock Tree							     */
/*****************************************************************************/

/*****************************************************************************/
/* Erase related operations						     */
/*****************************************************************************/

/**
 * If the first argument is smaller than the second, the function
 * returns a value smaller than zero. If they are equal, the function re-
 * turns zero. Otherwise, it should return a value greater than zero.
 */
int
peb_in_use_cmp(struct chfs_peb *peb1, struct chfs_peb *peb2)
{
	return (peb1->pebnr - peb2->pebnr);
}

int
peb_free_cmp(struct chfs_peb *peb1, struct chfs_peb *peb2)
{
	int comp;

	comp = peb1->erase_cnt - peb2->erase_cnt;
	if (0 == comp)
		comp = peb1->pebnr - peb2->pebnr;

	return comp;
}

/* Generate functions for in use PEB's red-black tree */
RB_PROTOTYPE(peb_in_use_rbtree, chfs_peb, u.rb, peb_in_use_cmp);
RB_GENERATE(peb_in_use_rbtree, chfs_peb, u.rb, peb_in_use_cmp);
RB_PROTOTYPE(peb_free_rbtree, chfs_peb, u.rb, peb_free_cmp);
RB_GENERATE(peb_free_rbtree, chfs_peb, u.rb, peb_free_cmp);

/**
 * add_peb_to_erase_queue: adds a PEB to to_erase/fully_erased queue
 * @ebh - chfs eraseblock handler
 * @pebnr - physical eraseblock's number
 * @ec - erase counter of PEB
 * @queue: the queue to add to
 *
 * This function adds a PEB to the erase queue specified by @queue.
 * The @ebh->erase_lock must be locked before using this.
 * Returns zero in case of success, error code in case of fail.
 */
int
add_peb_to_erase_queue(struct chfs_ebh *ebh, int pebnr, int ec,
    struct peb_queue *queue)
{
	struct chfs_peb *peb;

	peb = kmem_alloc(sizeof(struct chfs_peb), KM_SLEEP);

	peb->erase_cnt = ec;
	peb->pebnr = pebnr;

	TAILQ_INSERT_TAIL(queue, peb, u.queue);

	return 0;

}
//TODO
/**
 * find_peb_in_use - looks up a PEB in the RB-tree of used blocks
 * @ebh - chfs eraseblock handler
 *
 * This function returns a pointer to the PEB found in the tree,
 * NULL otherwise.
 * The @ebh->erase_lock must be locked before using this.
 */
struct chfs_peb *
find_peb_in_use(struct chfs_ebh *ebh, int pebnr)
{
	struct chfs_peb peb, *result;
	peb.pebnr = pebnr;
	result = RB_FIND(peb_in_use_rbtree, &ebh->in_use, &peb);
	return result;
}

/**
 * add_peb_to_free - adds a PEB to the RB-tree of free PEBs
 * @ebh - chfs eraseblock handler
 * @pebnr - physical eraseblock's number
 * @ec - erase counter of PEB
 *
 *
 * This function adds a physical eraseblock to the RB-tree of free PEBs
 * stored in the @ebh. The key is the erase counter and pebnr.
 * The @ebh->erase_lock must be locked before using this.
 * Returns zero in case of success, error code in case of fail.
 */
int
add_peb_to_free(struct chfs_ebh *ebh, int pebnr, int ec)
{
	struct chfs_peb *peb, *result;

	peb = kmem_alloc(sizeof(struct chfs_peb), KM_SLEEP);

	peb->erase_cnt = ec;
	peb->pebnr = pebnr;
	result = RB_INSERT(peb_free_rbtree, &ebh->free, peb);
	if (result) {
		kmem_free(peb, sizeof(struct chfs_peb));
		return 1;
	}

	return 0;
}

/**
 * add_peb_to_in_use - adds a PEB to the RB-tree of used PEBs
 * @ebh - chfs eraseblock handler
 * @pebnr - physical eraseblock's number
 * @ec - erase counter of PEB
 *
 *
 * This function adds a physical eraseblock to the RB-tree of used PEBs
 * stored in the @ebh. The key is pebnr.
 * The @ebh->erase_lock must be locked before using this.
 * Returns zero in case of success, error code in case of fail.
 */
int
add_peb_to_in_use(struct chfs_ebh *ebh, int pebnr, int ec)
{
	struct chfs_peb *peb, *result;

	peb = kmem_alloc(sizeof(struct chfs_peb), KM_SLEEP);

	peb->erase_cnt = ec;
	peb->pebnr = pebnr;
	result = RB_INSERT(peb_in_use_rbtree, &ebh->in_use, peb);
	if (result) {
		kmem_free(peb, sizeof(struct chfs_peb));
		return 1;
	}

	return 0;
}

/**
 * erase_callback - callback function for flash erase
 * @ei: erase information
 */
void
erase_callback(struct flash_erase_instruction *ei)
{
	int err;
	struct chfs_erase_info_priv *priv = (void *) ei->ei_priv;
	//dbg_ebh("ERASE_CALLBACK() CALLED\n");
	struct chfs_ebh *ebh = priv->ebh;
	struct chfs_peb *peb = priv->peb;

	peb->erase_cnt += 1;

	if (ei->ei_state == FLASH_ERASE_DONE) {

		/* Write out erase counter */
		err = ebh->ops->mark_eb_hdr_free(ebh,
		    peb->pebnr, peb->erase_cnt);
		if (err) {
			/* cannot mark PEB as free,so erase it again */
			chfs_err(
				"cannot mark eraseblock as free, PEB: %d\n",
				peb->pebnr);
			mutex_enter(&ebh->erase_lock);
			/*dbg_ebh("LOCK: ebh->erase_lock spin locked in erase_callback() "
			  "after mark ebhdr free\n");*/
			add_peb_to_erase_queue(ebh, peb->pebnr, peb->erase_cnt,
			    &ebh->to_erase);
			mutex_exit(&ebh->erase_lock);
			/*dbg_ebh("UNLOCK: ebh->erase_lock spin unlocked in erase_callback() "
			  "after mark ebhdr free\n");*/
			kmem_free(peb, sizeof(struct chfs_peb));
			return;
		}

		mutex_enter(&ebh->erase_lock);
		/*dbg_ebh("LOCK: ebh->erase_lock spin locked in erase_callback()\n");*/
		err = add_peb_to_free(ebh, peb->pebnr, peb->erase_cnt);
		mutex_exit(&ebh->erase_lock);
		/*dbg_ebh("UNLOCK: ebh->erase_lock spin unlocked in erase_callback()\n");*/
		kmem_free(peb, sizeof(struct chfs_peb));
	} else {
		/*
		 * Erase is finished, but there was a problem,
		 * so erase PEB again
		 */
		chfs_err("erase failed, state is: 0x%x\n", ei->ei_state);
		add_peb_to_erase_queue(ebh, peb->pebnr, peb->erase_cnt, &ebh->to_erase);
		kmem_free(peb, sizeof(struct chfs_peb));
	}
}

/**
 * free_peb: free a PEB
 * @ebh: chfs eraseblock handler
 *
 * This function erases the first physical eraseblock from one of the erase
 * lists and adds to the RB-tree of free PEBs.
 * Returns zero in case of succes, error code in case of fail.
 */
int
free_peb(struct chfs_ebh *ebh)
{
	int err, retries = 0;
	off_t ofs;
	struct chfs_peb *peb = NULL;
	struct flash_erase_instruction *ei;

	KASSERT(mutex_owned(&ebh->erase_lock));

	if (!TAILQ_EMPTY(&ebh->fully_erased)) {
		//dbg_ebh("[FREE PEB] got a fully erased block\n");
		peb = TAILQ_FIRST(&ebh->fully_erased);
		TAILQ_REMOVE(&ebh->fully_erased, peb, u.queue);
		err = ebh->ops->mark_eb_hdr_free(ebh,
		    peb->pebnr, peb->erase_cnt);
		if (err) {
			goto out_free;
		}
		err = add_peb_to_free(ebh, peb->pebnr, peb->erase_cnt);
		goto out_free;
	}
	/* Erase PEB */
	//dbg_ebh("[FREE PEB] eraseing a block\n");
	peb = TAILQ_FIRST(&ebh->to_erase);
	TAILQ_REMOVE(&ebh->to_erase, peb, u.queue);
	mutex_exit(&ebh->erase_lock);
	//dbg_ebh("UNLOCK: ebh->erase_lock spin unlocked in free_peb()\n");
	ofs = peb->pebnr * ebh->flash_if->erasesize;

	/* XXX where do we free this? */
	ei = kmem_alloc(sizeof(struct flash_erase_instruction)
	    + sizeof(struct chfs_erase_info_priv), KM_SLEEP);
retry:
	memset(ei, 0, sizeof(*ei));

//	ei->ei_if = ebh->flash_if;
	ei->ei_addr = ofs;
	ei->ei_len = ebh->flash_if->erasesize;
	ei->ei_callback = erase_callback;
	ei->ei_priv = (unsigned long) (&ei[1]);

	((struct chfs_erase_info_priv *) ei->ei_priv)->ebh = ebh;
	((struct chfs_erase_info_priv *) ei->ei_priv)->peb = peb;

	err = flash_erase(ebh->flash_dev, ei);
	dbg_ebh("erased peb: %d\n", peb->pebnr);

	/* einval would mean we did something wrong */
	KASSERT(err != EINVAL);

	if (err) {
		dbg_ebh("errno: %d, ei->ei_state: %d\n", err, ei->ei_state);
		if (CHFS_MAX_GET_PEB_RETRIES < ++retries &&
		    ei->ei_state == FLASH_ERASE_FAILED) {
			/* The block went bad mark it */
			dbg_ebh("ebh markbad! 0x%jx\n", (uintmax_t )ofs);
			err = flash_block_markbad(ebh->flash_dev, ofs);
			if (!err) {
				ebh->peb_nr--;
			}

			goto out;
		}
		chfs_err("can not erase PEB: %d, try again\n", peb->pebnr);
		goto retry;
	}

out:
	/* lock the erase_lock, because it was locked
	 * when the function was called */
	mutex_enter(&ebh->erase_lock);
	return err;

out_free:
	kmem_free(peb, sizeof(struct chfs_peb));
	return err;
}

/**
 * release_peb - schedule an erase for the PEB
 * @ebh: chfs eraseblock handler
 * @pebnr: physical eraseblock number
 *
 * This function get the peb identified by @pebnr from the in_use RB-tree of
 * @ebh, removes it and schedule an erase for it.
 *
 * Returns zero on success, error code in case of fail.
 */
int
release_peb(struct chfs_ebh *ebh, int pebnr)
{
	int err = 0;
	struct chfs_peb *peb;

	mutex_enter(&ebh->erase_lock);

	//dbg_ebh("LOCK: ebh->erase_lock spin locked in release_peb()\n");
	peb = find_peb_in_use(ebh, pebnr);
	if (!peb) {
		chfs_err("LEB is mapped, but is not in the 'in_use' "
		    "tree of ebh\n");
		goto out_unlock;
	}
	err = add_peb_to_erase_queue(ebh, peb->pebnr, peb->erase_cnt,
	    &ebh->to_erase);

	if (err)
		goto out_unlock;

	RB_REMOVE(peb_in_use_rbtree, &ebh->in_use, peb);
out_unlock:
	mutex_exit(&ebh->erase_lock);
	//dbg_ebh("UNLOCK: ebh->erase_lock spin unlocked in release_peb()"
	//		" at out_unlock\n");
	return err;
}

/**
 * erase_thread - background thread for erasing PEBs
 * @data: pointer to the eraseblock handler
 */
/*void
  erase_thread(void *data)
  {
  struct chfs_ebh *ebh = data;

  dbg_ebh("erase thread started\n");
  while (ebh->bg_erase.eth_running) {
  int err;

  mutex_enter(&ebh->erase_lock);
  dbg_ebh("LOCK: ebh->erase_lock spin locked in erase_thread()\n");
  if (TAILQ_EMPTY(&ebh->to_erase) && TAILQ_EMPTY(&ebh->fully_erased)) {
  dbg_ebh("thread has nothing to do\n");
  mutex_exit(&ebh->erase_lock);
  mutex_enter(&ebh->bg_erase.eth_thread_mtx);
  cv_timedwait_sig(&ebh->bg_erase.eth_wakeup,
  &ebh->bg_erase.eth_thread_mtx, mstohz(100));
  mutex_exit(&ebh->bg_erase.eth_thread_mtx);

  dbg_ebh("UNLOCK: ebh->erase_lock spin unlocked in erase_thread()\n");
  continue;
  }
  mutex_exit(&ebh->erase_lock);
  dbg_ebh("UNLOCK: ebh->erase_lock spin unlocked in erase_thread()\n");

  err = free_peb(ebh);
  if (err)
  chfs_err("freeing PEB failed in the background thread: %d\n", err);

  }
  dbg_ebh("erase thread stopped\n");
  kthread_exit(0);
  }*/

/**
 * erase_thread - background thread for erasing PEBs
 * @data: pointer to the eraseblock handler
 */
void
erase_thread(void *data) {
	dbg_ebh("[EBH THREAD] erase thread started\n");

	struct chfs_ebh *ebh = data;
	int err;

	mutex_enter(&ebh->erase_lock);
	while (ebh->bg_erase.eth_running) {
		if (TAILQ_EMPTY(&ebh->to_erase) &&
		    TAILQ_EMPTY(&ebh->fully_erased)) {
			cv_timedwait_sig(&ebh->bg_erase.eth_wakeup,
			    &ebh->erase_lock, mstohz(100));
		} else {
			/* XXX exiting this mutex is a bit odd here as
			 * free_peb instantly reenters it...
			 */
			err = free_peb(ebh);
			mutex_exit(&ebh->erase_lock);
			if (err) {
				chfs_err("freeing PEB failed in the"
				    " background thread: %d\n", err);
			}
			mutex_enter(&ebh->erase_lock);
		}
	}
	mutex_exit(&ebh->erase_lock);

	dbg_ebh("[EBH THREAD] erase thread stopped\n");
	kthread_exit(0);
}

/**
 * erase_thread_start - init and start erase thread
 * @ebh: eraseblock handler
 */
static void
erase_thread_start(struct chfs_ebh *ebh)
{
	cv_init(&ebh->bg_erase.eth_wakeup, "ebheracv");

	ebh->bg_erase.eth_running = true;
	kthread_create(PRI_NONE, KTHREAD_MPSAFE | KTHREAD_MUSTJOIN, NULL,
	    erase_thread, ebh, &ebh->bg_erase.eth_thread, "ebherase");
}

/**
 * erase_thread_stop - stop background erase thread
 * @ebh: eraseblock handler
 */
static void
erase_thread_stop(struct chfs_ebh *ebh)
{
	ebh->bg_erase.eth_running = false;
	cv_signal(&ebh->bg_erase.eth_wakeup);
	dbg_ebh("[EBH THREAD STOP] signaled\n");

	kthread_join(ebh->bg_erase.eth_thread);
#ifdef BROKEN_KTH_JOIN
	kpause("chfsebhjointh", false, mstohz(1000), NULL);
#endif

	cv_destroy(&ebh->bg_erase.eth_wakeup);
}

/*****************************************************************************/
/* End of Erase related operations					     */
/*****************************************************************************/

/*****************************************************************************/
/* Scan related operations						     */
/*****************************************************************************/
int
scan_leb_used_cmp(struct chfs_scan_leb *sleb1, struct chfs_scan_leb *sleb2)
{
	return (sleb1->lnr - sleb2->lnr);
}

RB_PROTOTYPE(scan_leb_used_rbtree, chfs_scan_leb, u.rb, scan_leb_used_cmp);
RB_GENERATE(scan_leb_used_rbtree, chfs_scan_leb, u.rb, scan_leb_used_cmp);

/**
 * scan_add_to_queue - adds a physical eraseblock to one of the
 *                     eraseblock queue
 * @si: chfs scanning information
 * @pebnr: physical eraseblock number
 * @erase_cnt: erase counter of the physical eraseblock
 * @list: the list to add to
 *
 * This function adds a physical eraseblock to one of the lists in the scanning
 * information.
 * Returns zero in case of success, negative error code in case of fail.
 */
static int
scan_add_to_queue(struct chfs_scan_info *si, int pebnr, int erase_cnt,
    struct scan_leb_queue *queue)
{
	struct chfs_scan_leb *sleb;

	sleb = kmem_alloc(sizeof(struct chfs_scan_leb), KM_SLEEP);

	sleb->pebnr = pebnr;
	sleb->erase_cnt = erase_cnt;
	TAILQ_INSERT_TAIL(queue, sleb, u.queue);
	return 0;
}

/*
 * nor_scan_add_to_used - add a physical eraseblock to the
 *                        used tree of scan info
 * @ebh: chfs eraseblock handler
 * @si: chfs scanning information
 * @ebhdr: eraseblock header
 * @pebnr: physical eraseblock number
 * @leb_status: the status of the PEB's eraseblock header
 *
 * This function adds a PEB to the used tree of the scanning information.
 * It handles the situations if there are more physical eraseblock referencing
 * to the same logical eraseblock.
 * Returns zero in case of success, error code in case of fail.
 */
int
nor_scan_add_to_used(struct chfs_ebh *ebh, struct chfs_scan_info *si,
    struct chfs_eb_hdr *ebhdr, int pebnr, int leb_status)
{
	int err, lnr, ec;
	struct chfs_scan_leb *sleb, *old;

	lnr = CHFS_GET_LID(ebhdr->u.nor_hdr.lid);
	ec = le32toh(ebhdr->ec_hdr.erase_cnt);

	sleb = kmem_alloc(sizeof(struct chfs_scan_leb), KM_SLEEP);

	sleb->erase_cnt = ec;
	sleb->lnr = lnr;
	sleb->pebnr = pebnr;
	sleb->info = leb_status;

	old = RB_INSERT(scan_leb_used_rbtree, &si->used, sleb);
	if (old) {
		kmem_free(sleb, sizeof(struct chfs_scan_leb));
		/* There is already an eraseblock in the used tree */
		/* If the new one is bad */
		if (EBHDR_LEB_DIRTY == leb_status &&
		    EBHDR_LEB_OK == old->info) {
			return scan_add_to_queue(si, pebnr, ec, &si->erase);
		} else {
			err = scan_add_to_queue(si, old->pebnr,
			    old->erase_cnt, &si->erase);
			if (err) {
				return err;
			}

			old->erase_cnt = ec;
			old->lnr = lnr;
			old->pebnr = pebnr;
			old->info = leb_status;
			return 0;
		}
	}
	return 0;
}

/**
 * nor_process eb -read the headers from NOR flash, check them and add to
 * 				   the scanning information
 * @ebh: chfs eraseblock handler
 * @si: chfs scanning information
 * @pebnr: physical eraseblock number
 *
 * Returns zero in case of success, error code in case of fail.
 */
int
nor_process_eb(struct chfs_ebh *ebh, struct chfs_scan_info *si,
    int pebnr, struct chfs_eb_hdr *ebhdr)
{
	int err, erase_cnt, leb_status;

	err = ebh->ops->read_eb_hdr(ebh, pebnr, ebhdr);
	if (err)
		return err;

	erase_cnt = le32toh(ebhdr->ec_hdr.erase_cnt);
	dbg_ebh("erase_cnt: %d\n", erase_cnt);
	leb_status = ebh->ops->check_eb_hdr(ebh, ebhdr);
	if (EBHDR_LEB_BADMAGIC == leb_status ||
	    EBHDR_LEB_BADCRC == leb_status) {
		err = scan_add_to_queue(si, pebnr, erase_cnt, &si->corrupted);
		return err;
	}
	else if (EBHDR_LEB_FREE == leb_status) {
		err = scan_add_to_queue(si, pebnr, erase_cnt, &si->free);
		goto count_mean;
	}
	else if (EBHDR_LEB_NO_HDR == leb_status) {
		err = scan_add_to_queue(si, pebnr, erase_cnt, &si->erased);
		return err;
	}
	else if (EBHDR_LEB_INVALIDATED == leb_status) {
		err = scan_add_to_queue(si, pebnr, erase_cnt, &si->erase);
		return err;
	}

	err = nor_scan_add_to_used(ebh, si, ebhdr, pebnr, leb_status);
	if (err)
		return err;


count_mean:
	si->sum_of_ec += erase_cnt;
	si->num_of_eb++;

	return err;
}

/*
 * nand_scan_add_to_used - add a physical eraseblock to the
 *                         used tree of scan info
 * @ebh: chfs eraseblock handler
 * @si: chfs scanning information
 * @ebhdr: eraseblock header
 * @pebnr: physical eraseblock number
 * @leb_status: the status of the PEB's eraseblock header
 *
 * This function adds a PEB to the used tree of the scanning information.
 * It handles the situations if there are more physical eraseblock referencing
 * to the same logical eraseblock.
 * Returns zero in case of success, error code in case of fail.
 */
int
nand_scan_add_to_used(struct chfs_ebh *ebh, struct chfs_scan_info *si,
    struct chfs_eb_hdr *ebhdr, int pebnr)
{
	int err, lnr, ec;
	struct chfs_scan_leb *sleb, *old;
	uint64_t serial = le64toh(ebhdr->u.nand_hdr.serial);

	lnr = CHFS_GET_LID(ebhdr->u.nor_hdr.lid);
	ec = le32toh(ebhdr->ec_hdr.erase_cnt);

	sleb = kmem_alloc(sizeof(struct chfs_scan_leb), KM_SLEEP);

	sleb->erase_cnt = ec;
	sleb->lnr = lnr;
	sleb->pebnr = pebnr;
	sleb->info = serial;

	old = RB_INSERT(scan_leb_used_rbtree, &si->used, sleb);
	if (old) {
		kmem_free(sleb, sizeof(struct chfs_scan_leb));
		/* There is already an eraseblock in the used tree */
		/* If the new one is bad */
		if (serial < old->info)
			return scan_add_to_queue(si, pebnr, ec, &si->erase);
		else {
			err = scan_add_to_queue(si,
			    old->pebnr, old->erase_cnt, &si->erase);
			if (err)
				return err;

			old->erase_cnt = ec;
			old->lnr = lnr;
			old->pebnr = pebnr;
			old->info = serial;
			return 0;
		}
	}
	return 0;
}

/**
 * nand_process eb -read the headers from NAND flash, check them and add to the
 * 					scanning information
 * @ebh: chfs eraseblock handler
 * @si: chfs scanning information
 * @pebnr: physical eraseblock number
 *
 * Returns zero in case of success, error code in case of fail.
 */
int
nand_process_eb(struct chfs_ebh *ebh, struct chfs_scan_info *si,
    int pebnr, struct chfs_eb_hdr *ebhdr)
{
	int err, erase_cnt, leb_status;
	uint64_t max_serial;
	/* isbad() is defined on some ancient platforms, heh */
	bool is_bad;

	/* Check block is bad */
	err = flash_block_isbad(ebh->flash_dev,
	    pebnr * ebh->flash_if->erasesize, &is_bad);
	if (err) {
		chfs_err("checking block is bad failed\n");
		return err;
	}
	if (is_bad) {
		si->bad_peb_cnt++;
		return 0;
	}

	err = ebh->ops->read_eb_hdr(ebh, pebnr, ebhdr);
	if (err)
		return err;

	erase_cnt = le32toh(ebhdr->ec_hdr.erase_cnt);
	leb_status = ebh->ops->check_eb_hdr(ebh, ebhdr);
	if (EBHDR_LEB_BADMAGIC == leb_status ||
	    EBHDR_LEB_BADCRC == leb_status) {
		err = scan_add_to_queue(si, pebnr, erase_cnt, &si->corrupted);
		return err;
	}
	else if (EBHDR_LEB_FREE == leb_status) {
		err = scan_add_to_queue(si, pebnr, erase_cnt, &si->free);
		goto count_mean;
	}
	else if (EBHDR_LEB_NO_HDR == leb_status) {
		err = scan_add_to_queue(si, pebnr, erase_cnt, &si->erased);
		return err;
	}

	err = nand_scan_add_to_used(ebh, si, ebhdr, pebnr);
	if (err)
		return err;

	max_serial = le64toh(ebhdr->u.nand_hdr.serial);
	if (max_serial > *ebh->max_serial) {
		*ebh->max_serial = max_serial;
	}

count_mean:
	si->sum_of_ec += erase_cnt;
	si->num_of_eb++;

	return err;
}

/**
 * chfs_scan - scans the media and returns informations about it
 * @ebh: chfs eraseblock handler
 *
 * This function scans through the media and returns information about it or if
 * it fails NULL will be returned.
 */
struct chfs_scan_info *
chfs_scan(struct chfs_ebh *ebh)
{
	struct chfs_scan_info *si;
	struct chfs_eb_hdr *ebhdr;
	int pebnr, err;

	si = kmem_alloc(sizeof(*si), KM_SLEEP);

	TAILQ_INIT(&si->corrupted);
	TAILQ_INIT(&si->free);
	TAILQ_INIT(&si->erase);
	TAILQ_INIT(&si->erased);
	RB_INIT(&si->used);
	si->bad_peb_cnt = 0;
	si->num_of_eb = 0;
	si->sum_of_ec = 0;

	ebhdr = kmem_alloc(sizeof(*ebhdr), KM_SLEEP);

	for (pebnr = 0; pebnr < ebh->peb_nr; pebnr++) {
		dbg_ebh("processing PEB %d\n", pebnr);
		err = ebh->ops->process_eb(ebh, si, pebnr, ebhdr);
		if (err < 0)
			goto out_ebhdr;
	}
	kmem_free(ebhdr, sizeof(*ebhdr));
	dbg_ebh("[CHFS_SCAN] scanning information collected\n");
	return si;

out_ebhdr:
	kmem_free(ebhdr, sizeof(*ebhdr));
	kmem_free(si, sizeof(*si));
	return NULL;
}

/**
 * scan_info_destroy - frees all lists and trees in the scanning information
 * @si: the scanning information
 */
void
scan_info_destroy(struct chfs_scan_info *si)
{
	EBH_QUEUE_DESTROY(&si->corrupted,
	    struct chfs_scan_leb, u.queue);

	EBH_QUEUE_DESTROY(&si->erase,
	    struct chfs_scan_leb, u.queue);

	EBH_QUEUE_DESTROY(&si->erased,
	    struct chfs_scan_leb, u.queue);

	EBH_QUEUE_DESTROY(&si->free,
	    struct chfs_scan_leb, u.queue);

	EBH_TREE_DESTROY(scan_leb_used_rbtree,
	    &si->used, struct chfs_scan_leb);

	kmem_free(si, sizeof(*si));
	dbg_ebh("[SCAN_INFO_DESTROY] scanning information destroyed\n");
}

/**
 * scan_media - scan media
 *
 * @ebh - chfs eraseblock handler
 *
 * Returns zero in case of success, error code in case of fail.
 */

int
scan_media(struct chfs_ebh *ebh)
{
	int err, i, avg_ec;
	struct chfs_scan_info *si;
	struct chfs_scan_leb *sleb;

	si = chfs_scan(ebh);
	/*
	 * Process the scan info, manage the eraseblock lists
	 */
	mutex_init(&ebh->ltree_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&ebh->erase_lock, MUTEX_DEFAULT, IPL_NONE);
	RB_INIT(&ebh->ltree);
	RB_INIT(&ebh->free);
	RB_INIT(&ebh->in_use);
	TAILQ_INIT(&ebh->to_erase);
	TAILQ_INIT(&ebh->fully_erased);
	mutex_init(&ebh->alc_mutex, MUTEX_DEFAULT, IPL_NONE);

	ebh->peb_nr -= si->bad_peb_cnt;

	/*
	 * Create background thread for erasing
	 */
	erase_thread_start(ebh);

	ebh->lmap = kmem_alloc(ebh->peb_nr * sizeof(int), KM_SLEEP);

	for (i = 0; i < ebh->peb_nr; i++) {
		ebh->lmap[i] = EBH_LEB_UNMAPPED;
	}

	if (si->num_of_eb == 0) {
		/* The flash contains no data. */
		avg_ec = 0;
	}
	else {
		avg_ec = (int) (si->sum_of_ec / si->num_of_eb);
	}
	dbg_ebh("num_of_eb: %d\n", si->num_of_eb);

	mutex_enter(&ebh->erase_lock);

	RB_FOREACH(sleb, scan_leb_used_rbtree, &si->used) {
		ebh->lmap[sleb->lnr] = sleb->pebnr;
		err = add_peb_to_in_use(ebh, sleb->pebnr, sleb->erase_cnt);
		if (err)
			goto out_free;
	}

	TAILQ_FOREACH(sleb, &si->erased, u.queue) {
		err = add_peb_to_erase_queue(ebh, sleb->pebnr, avg_ec,
		    &ebh->fully_erased);
		if (err)
			goto out_free;
	}

	TAILQ_FOREACH(sleb, &si->erase, u.queue) {
		err = add_peb_to_erase_queue(ebh, sleb->pebnr, avg_ec,
		    &ebh->to_erase);
		if (err)
			goto out_free;
	}

	TAILQ_FOREACH(sleb, &si->free, u.queue) {
		err = add_peb_to_free(ebh, sleb->pebnr, sleb->erase_cnt);
		if (err)
			goto out_free;
	}

	TAILQ_FOREACH(sleb, &si->corrupted, u.queue) {
		err = add_peb_to_erase_queue(ebh, sleb->pebnr, avg_ec,
		    &ebh->to_erase);
		if (err)
			goto out_free;
	}
	mutex_exit(&ebh->erase_lock);
	scan_info_destroy(si);
	return 0;

out_free:
	mutex_exit(&ebh->erase_lock);
	kmem_free(ebh->lmap, ebh->peb_nr * sizeof(int));
	scan_info_destroy(si);
	dbg_ebh("[SCAN_MEDIA] returning with error: %d\n", err);
	return err;
}

/*****************************************************************************/
/* End of Scan related operations					     */
/*****************************************************************************/

/**
 * ebh_open - opens mtd device and init ereaseblock header
 * @ebh: eraseblock handler
 * @flash_nr: flash device number to use
 *
 * Returns zero in case of success, error code in case of fail.
 */
int
ebh_open(struct chfs_ebh *ebh, dev_t dev)
{
	int err;

	ebh->flash_dev = flash_get_device(dev);
	if (!ebh->flash_dev) {
		aprint_error("ebh_open: cant get flash device\n");
		return ENODEV;
	}

	ebh->flash_if = flash_get_interface(dev);
	if (!ebh->flash_if) {
		aprint_error("ebh_open: cant get flash interface\n");
		return ENODEV;
	}

	ebh->flash_size = flash_get_size(dev);
	ebh->peb_nr = ebh->flash_size / ebh->flash_if->erasesize;
//	ebh->peb_nr = ebh->flash_if->size / ebh->flash_if->erasesize;
	/* Set up flash operations based on flash type */
	ebh->ops = kmem_alloc(sizeof(struct chfs_ebh_ops), KM_SLEEP);

	switch (ebh->flash_if->type) {
	case FLASH_TYPE_NOR:
		ebh->eb_size = ebh->flash_if->erasesize -
		    CHFS_EB_EC_HDR_SIZE - CHFS_EB_HDR_NOR_SIZE;

		ebh->ops->read_eb_hdr = nor_read_eb_hdr;
		ebh->ops->write_eb_hdr = nor_write_eb_hdr;
		ebh->ops->check_eb_hdr = nor_check_eb_hdr;
		ebh->ops->mark_eb_hdr_dirty_flash =
		    nor_mark_eb_hdr_dirty_flash;
		ebh->ops->invalidate_eb_hdr = nor_invalidate_eb_hdr;
		ebh->ops->mark_eb_hdr_free = mark_eb_hdr_free;

		ebh->ops->process_eb = nor_process_eb;

		ebh->ops->create_eb_hdr = nor_create_eb_hdr;
		ebh->ops->calc_data_offs = nor_calc_data_offs;

		ebh->max_serial = NULL;
		break;
	case FLASH_TYPE_NAND:
		ebh->eb_size = ebh->flash_if->erasesize -
		    2 * ebh->flash_if->page_size;

		ebh->ops->read_eb_hdr = nand_read_eb_hdr;
		ebh->ops->write_eb_hdr = nand_write_eb_hdr;
		ebh->ops->check_eb_hdr = nand_check_eb_hdr;
		ebh->ops->mark_eb_hdr_free = mark_eb_hdr_free;
		ebh->ops->mark_eb_hdr_dirty_flash = NULL;
		ebh->ops->invalidate_eb_hdr = NULL;

		ebh->ops->process_eb = nand_process_eb;

		ebh->ops->create_eb_hdr = nand_create_eb_hdr;
		ebh->ops->calc_data_offs = nand_calc_data_offs;

		ebh->max_serial = kmem_alloc(sizeof(uint64_t), KM_SLEEP);

		*ebh->max_serial = 0;
		break;
	default:
		return 1;
	}
	printf("opening ebh: eb_size: %zu\n", ebh->eb_size);
	err = scan_media(ebh);
	if (err) {
		dbg_ebh("Scan failed.");
		kmem_free(ebh->ops, sizeof(struct chfs_ebh_ops));
		kmem_free(ebh, sizeof(struct chfs_ebh));
		return err;
	}
	return 0;
}

/**
 * ebh_close - close ebh
 * @ebh: eraseblock handler
 * Returns zero in case of success, error code in case of fail.
 */
int
ebh_close(struct chfs_ebh *ebh)
{
	erase_thread_stop(ebh);

	EBH_TREE_DESTROY(peb_free_rbtree, &ebh->free, struct chfs_peb);
	EBH_TREE_DESTROY(peb_in_use_rbtree, &ebh->in_use, struct chfs_peb);

	EBH_QUEUE_DESTROY(&ebh->fully_erased, struct chfs_peb, u.queue);
	EBH_QUEUE_DESTROY(&ebh->to_erase, struct chfs_peb, u.queue);

	/* XXX HACK, see ebh.h */
	EBH_TREE_DESTROY_MUTEX(ltree_rbtree, &ebh->ltree,
	    struct chfs_ltree_entry);

	KASSERT(!mutex_owned(&ebh->ltree_lock));
	KASSERT(!mutex_owned(&ebh->alc_mutex));
	KASSERT(!mutex_owned(&ebh->erase_lock));

	mutex_destroy(&ebh->ltree_lock);
	mutex_destroy(&ebh->alc_mutex);
	mutex_destroy(&ebh->erase_lock);

	kmem_free(ebh->ops, sizeof(struct chfs_ebh_ops));
	kmem_free(ebh, sizeof(struct chfs_ebh));

	return 0;
}

/**
 * ebh_read_leb - read data from leb
 * @ebh: eraseblock handler
 * @lnr: logical eraseblock number
 * @buf: buffer to read to
 * @offset: offset from where to read
 * @len: bytes number to read
 *
 * Returns zero in case of success, error code in case of fail.
 */
int
ebh_read_leb(struct chfs_ebh *ebh, int lnr, char *buf, uint32_t offset,
    size_t len, size_t *retlen)
{
	int err, pebnr;
	off_t data_offset;

	KASSERT(offset + len <= ebh->eb_size);

	err = leb_read_lock(ebh, lnr);
	if (err)
		return err;

	pebnr = ebh->lmap[lnr];
	/* If PEB is not mapped the buffer is filled with 0xFF */
	if (EBH_LEB_UNMAPPED == pebnr) {
		leb_read_unlock(ebh, lnr);
		memset(buf, 0xFF, len);
		return 0;
	}

	/* Read data */
	data_offset = ebh->ops->calc_data_offs(ebh, pebnr, offset);
	err = flash_read(ebh->flash_dev, data_offset, len, retlen,
	    (unsigned char *) buf);
	if (err)
		goto out_free;

	KASSERT(len == *retlen);

out_free:
	leb_read_unlock(ebh, lnr);
	return err;
}

/**
 * get_peb: get a free physical eraseblock
 * @ebh - chfs eraseblock handler
 *
 * This function gets a free eraseblock from the ebh->free RB-tree.
 * The fist entry will be returned and deleted from the tree.
 * The entries sorted by the erase counters, so the PEB with the smallest
 * erase counter will be added back.
 * If something goes bad a negative value will be returned.
 */
int
get_peb(struct chfs_ebh *ebh)
{
	int err, pebnr;
	struct chfs_peb *peb;

retry:
	mutex_enter(&ebh->erase_lock);
	//dbg_ebh("LOCK: ebh->erase_lock spin locked in get_peb()\n");
	if (RB_EMPTY(&ebh->free)) {
		/*There is no more free PEBs in the tree*/
		if (TAILQ_EMPTY(&ebh->to_erase) &&
		    TAILQ_EMPTY(&ebh->fully_erased)) {
			mutex_exit(&ebh->erase_lock);
			//dbg_ebh("UNLOCK: ebh->erase_lock spin unlocked in get_peb()\n");
			return ENOSPC;
		}
		err = free_peb(ebh);

		mutex_exit(&ebh->erase_lock);
		//dbg_ebh("UNLOCK: ebh->erase_lock spin unlocked in get_peb()\n");

		if (err)
			return err;
		goto retry;
	}
	peb = RB_MIN(peb_free_rbtree, &ebh->free);
	pebnr = peb->pebnr;
	RB_REMOVE(peb_free_rbtree, &ebh->free, peb);
	err = add_peb_to_in_use(ebh, peb->pebnr, peb->erase_cnt);
	if (err)
		pebnr = err;

	kmem_free(peb, sizeof(struct chfs_peb));

	mutex_exit(&ebh->erase_lock);
	//dbg_ebh("UNLOCK: ebh->erase_lock spin unlocked in get_peb()\n");

	return pebnr;
}

/**
 * ebh_write_leb - write data to leb
 * @ebh: eraseblock handler
 * @lnr: logical eraseblock number
 * @buf: data to write
 * @offset: offset where to write
 * @len: bytes number to write
 *
 * Returns zero in case of success, error code in case of fail.
 */
int
ebh_write_leb(struct chfs_ebh *ebh, int lnr, char *buf, uint32_t offset,
    size_t len, size_t *retlen)
{
	int err, pebnr, retries = 0;
	off_t data_offset;
	struct chfs_eb_hdr *ebhdr;

	dbg("offset: %d | len: %zu | (offset+len): %zu "
	    " | ebsize: %zu\n", offset, len, (offset+len), ebh->eb_size);

	KASSERT(offset + len <= ebh->eb_size);

	err = leb_write_lock(ebh, lnr);
	if (err)
		return err;

	pebnr = ebh->lmap[lnr];
	/* If the LEB is mapped write out data */
	if (pebnr != EBH_LEB_UNMAPPED) {
		data_offset = ebh->ops->calc_data_offs(ebh, pebnr, offset);
		err = flash_write(ebh->flash_dev, data_offset, len, retlen,
		    (unsigned char *) buf);

		if (err) {
			chfs_err("error %d while writing %zu bytes to PEB "
			    "%d:%ju, written %zu bytes\n",
			    err, len, pebnr, (uintmax_t )offset, *retlen);
		} else {
			KASSERT(len == *retlen);
		}

		leb_write_unlock(ebh, lnr);
		return err;
	}

	/*
	 * If the LEB is unmapped, get a free PEB and write the
	 * eraseblock header first
	 */
	ebhdr = kmem_alloc(sizeof(struct chfs_eb_hdr), KM_SLEEP);

	/* Setting up eraseblock header properties */
	ebh->ops->create_eb_hdr(ebhdr, lnr);

retry:
	/* Getting a physical eraseblock from the wear leveling system */
	pebnr = get_peb(ebh);
	if (pebnr < 0) {
		leb_write_unlock(ebh, lnr);
		kmem_free(ebhdr, sizeof(struct chfs_eb_hdr));
		return pebnr;
	}

	/* Write the eraseblock header to the media */
	err = ebh->ops->write_eb_hdr(ebh, pebnr, ebhdr);
	if (err) {
		chfs_warn(
			"error writing eraseblock header: LEB %d , PEB %d\n",
			lnr, pebnr);
		goto write_error;
	}

	/* Write out data */
	if (len) {
		data_offset = ebh->ops->calc_data_offs(ebh, pebnr, offset);
		err = flash_write(ebh->flash_dev,
		    data_offset, len, retlen, (unsigned char *) buf);
		if (err) {
			chfs_err("error %d while writing %zu bytes to PEB "
			    " %d:%ju, written %zu bytes\n",
			    err, len, pebnr, (uintmax_t )offset, *retlen);
			goto write_error;
		}
	}

	ebh->lmap[lnr] = pebnr;
	leb_write_unlock(ebh, lnr);
	kmem_free(ebhdr, sizeof(struct chfs_eb_hdr));

	return 0;

write_error: err = release_peb(ebh, pebnr);
	// max retries (NOW: 2)
	if (err || CHFS_MAX_GET_PEB_RETRIES < ++retries) {
		leb_write_unlock(ebh, lnr);
		kmem_free(ebhdr, sizeof(struct chfs_eb_hdr));
		return err;
	}
	goto retry;
}

/**
 * ebh_erase_leb - erase a leb
 * @ebh: eraseblock handler
 * @lnr: leb number
 *
 * Returns zero in case of success, error code in case of fail.
 */
int
ebh_erase_leb(struct chfs_ebh *ebh, int lnr)
{
	int err, pebnr;

	leb_write_lock(ebh, lnr);

	pebnr = ebh->lmap[lnr];
	if (pebnr < 0) {
		leb_write_unlock(ebh, lnr);
		return EBH_LEB_UNMAPPED;
	}
	err = release_peb(ebh, pebnr);
	if (err)
		goto out_unlock;

	ebh->lmap[lnr] = EBH_LEB_UNMAPPED;
	cv_signal(&ebh->bg_erase.eth_wakeup);
out_unlock:
	leb_write_unlock(ebh, lnr);
	return err;
}

/**
 * ebh_map_leb - maps a PEB to LEB
 * @ebh: eraseblock handler
 * @lnr: leb number
 *
 * Returns zero on success, error code in case of fail
 */
int
ebh_map_leb(struct chfs_ebh *ebh, int lnr)
{
	int err, pebnr, retries = 0;
	struct chfs_eb_hdr *ebhdr;

	ebhdr = kmem_alloc(sizeof(struct chfs_eb_hdr), KM_SLEEP);

	err = leb_write_lock(ebh, lnr);
	if (err) {
		kmem_free(ebhdr, sizeof(struct chfs_eb_hdr));
		return err;
	}

retry:
	pebnr = get_peb(ebh);
	if (pebnr < 0) {
		err = pebnr;
		goto out_unlock;
	}

	ebh->ops->create_eb_hdr(ebhdr, lnr);

	err = ebh->ops->write_eb_hdr(ebh, pebnr, ebhdr);
	if (err) {
		chfs_warn(
			"error writing eraseblock header: LEB %d , PEB %d\n",
			lnr, pebnr);
		goto write_error;
	}

	ebh->lmap[lnr] = pebnr;

out_unlock:
	leb_write_unlock(ebh, lnr);
	return err;

write_error:
	err = release_peb(ebh, pebnr);
	// max retries (NOW: 2)
	if (err || CHFS_MAX_GET_PEB_RETRIES < ++retries) {
		leb_write_unlock(ebh, lnr);
		kmem_free(ebhdr, sizeof(struct chfs_eb_hdr));
		return err;
	}
	goto retry;
}

/**
 * ebh_unmap_leb -
 * @ebh: eraseblock handler
 * @lnr: leb number
 *
 * Retruns zero on success, error code in case of fail.
 */
int
ebh_unmap_leb(struct chfs_ebh *ebh, int lnr)
{
	int err;

	if (ebh_is_mapped(ebh, lnr) < 0)
		/* If the eraseblock already unmapped */
		return 0;

	err = ebh_erase_leb(ebh, lnr);

	return err;
}

/**
 * ebh_is_mapped - check if a PEB is mapped to @lnr
 * @ebh: eraseblock handler
 * @lnr: leb number
 *
 * Retruns 0 if the logical eraseblock is mapped, negative error code otherwise.
 */
int
ebh_is_mapped(struct chfs_ebh *ebh, int lnr)
{
	int err, result;
	err = leb_read_lock(ebh, lnr);
	if (err)
		return err;

	result = ebh->lmap[lnr];
	leb_read_unlock(ebh, lnr);

	return result;
}

/**
 * ebh_change_leb - write the LEB to another PEB
 * @ebh: eraseblock handler
 * @lnr: leb number
 * @buf: data to write
 * @len: length of data
 * Returns zero in case of success, error code in case of fail.
 */
int
ebh_change_leb(struct chfs_ebh *ebh, int lnr, char *buf, size_t len,
    size_t *retlen)
{
	int err, pebnr, pebnr_old, retries = 0;
	off_t data_offset;

	struct chfs_peb *peb = NULL;
	struct chfs_eb_hdr *ebhdr;

	if (ebh_is_mapped(ebh, lnr) < 0)
		return EBH_LEB_UNMAPPED;

	if (len == 0) {
		err = ebh_unmap_leb(ebh, lnr);
		if (err)
			return err;
		return ebh_map_leb(ebh, lnr);
	}

	ebhdr = kmem_alloc(sizeof(struct chfs_eb_hdr), KM_SLEEP);

	pebnr_old = ebh->lmap[lnr];

	mutex_enter(&ebh->alc_mutex);
	err = leb_write_lock(ebh, lnr);
	if (err)
		goto out_mutex;

	if (ebh->ops->mark_eb_hdr_dirty_flash) {
		err = ebh->ops->mark_eb_hdr_dirty_flash(ebh, pebnr_old, lnr);
		if (err)
			goto out_unlock;
	}

	/* Setting up eraseblock header properties */
	ebh->ops->create_eb_hdr(ebhdr, lnr);

retry:
	/* Getting a physical eraseblock from the wear leveling system */
	pebnr = get_peb(ebh);
	if (pebnr < 0) {
		leb_write_unlock(ebh, lnr);
		mutex_exit(&ebh->alc_mutex);
		kmem_free(ebhdr, sizeof(struct chfs_eb_hdr));
		return pebnr;
	}

	err = ebh->ops->write_eb_hdr(ebh, pebnr, ebhdr);
	if (err) {
		chfs_warn(
			"error writing eraseblock header: LEB %d , PEB %d",
			lnr, pebnr);
		goto write_error;
	}

	/* Write out data */
	data_offset = ebh->ops->calc_data_offs(ebh, pebnr, 0);
	err = flash_write(ebh->flash_dev, data_offset, len, retlen,
	    (unsigned char *) buf);
	if (err) {
		chfs_err("error %d while writing %zu bytes to PEB %d:%ju,"
		    " written %zu bytes",
		    err, len, pebnr, (uintmax_t)data_offset, *retlen);
		goto write_error;
	}

	ebh->lmap[lnr] = pebnr;

	if (ebh->ops->invalidate_eb_hdr) {
		err = ebh->ops->invalidate_eb_hdr(ebh, pebnr_old);
		if (err)
			goto out_unlock;
	}
	peb = find_peb_in_use(ebh, pebnr_old);
	err = release_peb(ebh, peb->pebnr);

out_unlock:
	leb_write_unlock(ebh, lnr);

out_mutex:
	mutex_exit(&ebh->alc_mutex);
	kmem_free(ebhdr, sizeof(struct chfs_eb_hdr));
	kmem_free(peb, sizeof(struct chfs_peb));
	return err;

write_error:
	err = release_peb(ebh, pebnr);
	//max retries (NOW: 2)
	if (err || CHFS_MAX_GET_PEB_RETRIES < ++retries) {
		leb_write_unlock(ebh, lnr);
		mutex_exit(&ebh->alc_mutex);
		kmem_free(ebhdr, sizeof(struct chfs_eb_hdr));
		return err;
	}
	goto retry;
}


/*	$NetBSD: chfs_wbuf.c,v 1.2 2011/11/24 20:50:33 agc Exp $	*/

/*-
 * Copyright (c) 2010 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * Copyright (C) 2010 Tamas Toth <ttoth@inf.u-szeged.hu>
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

#include <dev/flash/flash.h>
#include <sys/uio.h>
#include "chfs.h"
//#include </root/xipffs/netbsd.chfs/chfs.h>

#define DBG_WBUF 1

#define PAD(x) (((x)+3)&~3)

#define EB_ADDRESS(x) ( ((unsigned long)(x) / chmp->chm_ebh->eb_size) * chmp->chm_ebh->eb_size )

#define PAGE_DIV(x) ( ((unsigned long)(x) / (unsigned long)(chmp->chm_wbuf_pagesize)) * (unsigned long)(chmp->chm_wbuf_pagesize) )
#define PAGE_MOD(x) ( (unsigned long)(x) % (unsigned long)(chmp->chm_wbuf_pagesize) )

/*
// test functions
int wbuf_test(void);
void wbuf_test_erase_flash(struct chfs_mount*);
void wbuf_test_callback(struct erase_instruction*);
*/

#define NOPAD	0
#define SETPAD	1


/**
 * chfs_flush_wbuf - write wbuf to the flash
 * @chmp: super block info
 * @pad: padding (NOPAD / SETPAD)
 * Returns zero in case of success.
 */
static int
chfs_flush_wbuf(struct chfs_mount *chmp, int pad)
{
	int ret=0;
	size_t retlen = 0;

	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));
	KASSERT(mutex_owned(&chmp->chm_lock_sizes));
	KASSERT(rw_write_held(&chmp->chm_lock_wbuf));

	if (pad) {
		chmp->chm_wbuf_len = PAD(chmp->chm_wbuf_len);
		memset(chmp->chm_wbuf + chmp->chm_wbuf_len, 0, chmp->chm_wbuf_pagesize - chmp->chm_wbuf_len);

		struct chfs_flash_padding_node* padnode = (void*)(chmp->chm_wbuf + chmp->chm_wbuf_len);
		padnode->magic = htole16(CHFS_FS_MAGIC_BITMASK);
		padnode->type = htole16(CHFS_NODETYPE_PADDING);
		padnode->length = htole32(chmp->chm_wbuf_pagesize - chmp->chm_wbuf_len);
		padnode->hdr_crc = htole32(crc32(0, (uint8_t *)padnode, sizeof(*padnode)-4));

		struct chfs_node_ref *nref;
		nref = chfs_alloc_node_ref(chmp->chm_nextblock);
		nref->nref_offset = chmp->chm_wbuf_ofs + chmp->chm_wbuf_len;
		nref->nref_offset = CHFS_GET_OFS(nref->nref_offset) |
		    CHFS_OBSOLETE_NODE_MASK;
		chmp->chm_wbuf_len = chmp->chm_wbuf_pagesize;

		chfs_change_size_free(chmp, chmp->chm_nextblock, -padnode->length);
		chfs_change_size_wasted(chmp, chmp->chm_nextblock, padnode->length);
	}

	ret = chfs_write_leb(chmp, chmp->chm_nextblock->lnr, chmp->chm_wbuf, chmp->chm_wbuf_ofs, chmp->chm_wbuf_len, &retlen);
	if(ret) {
		return ret;
	}

	memset(chmp->chm_wbuf,0xff,chmp->chm_wbuf_pagesize);
	chmp->chm_wbuf_ofs += chmp->chm_wbuf_pagesize;
	chmp->chm_wbuf_len = 0;
	return 0;
}


/**
 * chfs_fill_wbuf - write to wbuf
 * @chmp: super block info
 * @buf: buffer
 * @len: buffer length
 * Return the len of the buf what we didn't write to the wbuf.
 */
static size_t
chfs_fill_wbuf(struct chfs_mount *chmp, const u_char *buf, size_t len)
{
	if (len && !chmp->chm_wbuf_len && (len >= chmp->chm_wbuf_pagesize)) {
		return 0;
	}
	if (len > (chmp->chm_wbuf_pagesize - chmp->chm_wbuf_len)) {
		len = chmp->chm_wbuf_pagesize - chmp->chm_wbuf_len;
	}
	memcpy(chmp->chm_wbuf + chmp->chm_wbuf_len, buf, len);

	chmp->chm_wbuf_len += (int) len;
	return len;
}

/**
 * chfs_write_wbuf - write to wbuf and then the flash
 * @chmp: super block info
 * @invecs: io vectors
 * @count: num of vectors
 * @to: offset of target
 * @retlen: writed bytes
 * Returns zero in case of success.
 */
int
chfs_write_wbuf(struct chfs_mount* chmp, const struct iovec *invecs, long count,
    off_t to, size_t *retlen)
{
	int invec, ret = 0;
	size_t wbuf_retlen, donelen = 0;
	int outvec_to = to;

	int lnr = chmp->chm_nextblock->lnr;

	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));
	KASSERT(mutex_owned(&chmp->chm_lock_sizes));
	KASSERT(!rw_write_held(&chmp->chm_lock_wbuf));

	rw_enter(&chmp->chm_lock_wbuf, RW_WRITER);

	//dbg("1. wbuf ofs: %zu, len: %zu\n", chmp->chm_wbuf_ofs, chmp->chm_wbuf_len);

	if (chmp->chm_wbuf_ofs == 0xffffffff) {
		chmp->chm_wbuf_ofs = PAGE_DIV(to);
		chmp->chm_wbuf_len = PAGE_MOD(to);
		memset(chmp->chm_wbuf, 0xff, chmp->chm_wbuf_pagesize);
	}

	//dbg("2. wbuf ofs: %zu, len: %zu\n", chmp->chm_wbuf_ofs, chmp->chm_wbuf_len);

	if (EB_ADDRESS(to) != EB_ADDRESS(chmp->chm_wbuf_ofs)) {
		if (chmp->chm_wbuf_len) {
			ret = chfs_flush_wbuf(chmp, SETPAD);
			if (ret)
				goto outerr;
		}
		chmp->chm_wbuf_ofs = PAGE_DIV(to);
		chmp->chm_wbuf_len = PAGE_MOD(to);
	}

	//dbg("3. wbuf ofs: %zu, len: %zu\n", chmp->chm_wbuf_ofs, chmp->chm_wbuf_len);

	if (to != PAD(chmp->chm_wbuf_ofs + chmp->chm_wbuf_len)) {
		dbg("to: %llu != %zu\n", (unsigned long long)to,
			PAD(chmp->chm_wbuf_ofs + chmp->chm_wbuf_len));
		dbg("Non-contiguous write\n");
		panic("BUG\n");
	}

	/* adjust alignment offset */
	if (chmp->chm_wbuf_len != PAGE_MOD(to)) {
		chmp->chm_wbuf_len = PAGE_MOD(to);
		/* take care of alignement to next page*/
		if (!chmp->chm_wbuf_len) {
			chmp->chm_wbuf_len += chmp->chm_wbuf_pagesize;
			ret = chfs_flush_wbuf(chmp, NOPAD);
			if (ret)
				goto outerr;
		}
	}

	for (invec = 0; invec < count; invec++) {
		int vlen = invecs[invec].iov_len;
		u_char* v = invecs[invec].iov_base;

		//dbg("invec:%d len:%d\n", invec, vlen);

		wbuf_retlen = chfs_fill_wbuf(chmp, v, vlen);
		if (chmp->chm_wbuf_len == chmp->chm_wbuf_pagesize) {
			ret = chfs_flush_wbuf(chmp, NOPAD);
			if (ret) {
				goto outerr;
			}
		}
		vlen -= wbuf_retlen;
		outvec_to += wbuf_retlen;
		v += wbuf_retlen;
		donelen += wbuf_retlen;
		if (vlen >= chmp->chm_wbuf_pagesize) {
			ret = chfs_write_leb(chmp, lnr, v, outvec_to, PAGE_DIV(vlen), &wbuf_retlen);
			//dbg("fd->write: %zu\n", wbuf_retlen);
			vlen -= wbuf_retlen;
			outvec_to += wbuf_retlen;
			chmp->chm_wbuf_ofs = outvec_to;
			v += wbuf_retlen;
			donelen += wbuf_retlen;
		}
		wbuf_retlen = chfs_fill_wbuf(chmp, v, vlen);
		if (chmp->chm_wbuf_len == chmp->chm_wbuf_pagesize) {
			ret = chfs_flush_wbuf(chmp, NOPAD);
			if (ret)
				goto outerr;
		}

		// if we write the last vector, we flush with padding
		/*if (invec == count-1) {
		  ret = chfs_flush_wbuf(chmp, SETPAD);
		  if (ret)
		  goto outerr;
		  }*/
		outvec_to += wbuf_retlen;
		donelen += wbuf_retlen;
	}
	*retlen = donelen;
	rw_exit(&chmp->chm_lock_wbuf);
	return ret;

outerr:
	*retlen = 0;
	return ret;
}

int chfs_flush_pending_wbuf(struct chfs_mount *chmp)
{
	//dbg("flush pending wbuf\n");
	int err;
	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));
	mutex_enter(&chmp->chm_lock_sizes);
	rw_enter(&chmp->chm_lock_wbuf, RW_WRITER);
	err = chfs_flush_wbuf(chmp, SETPAD);
	rw_exit(&chmp->chm_lock_wbuf);
	mutex_exit(&chmp->chm_lock_sizes);
	return err;
}

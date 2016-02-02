/*	$NetBSD: chfs_wbuf.c,v 1.7 2014/10/18 08:33:29 snj Exp $	*/

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

#define DBG_WBUF 1		/* XXX unused, but should be */

#define PAD(x) (((x)+3)&~3)

#define EB_ADDRESS(x) ( rounddown((x), chmp->chm_ebh->eb_size) )

#define PAGE_DIV(x) ( rounddown((x), chmp->chm_wbuf_pagesize) )
#define PAGE_MOD(x) ( (x) % (chmp->chm_wbuf_pagesize) )

/* writebuffer options */
enum {
	WBUF_NOPAD,
	WBUF_SETPAD
};

/*
 * chfs_flush_wbuf - write wbuf to the flash
 * Returns zero in case of success.
 */
static int
chfs_flush_wbuf(struct chfs_mount *chmp, int pad)
{
	int ret;
	size_t retlen;
	struct chfs_node_ref *nref;
	struct chfs_flash_padding_node* padnode;

	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));
	KASSERT(mutex_owned(&chmp->chm_lock_sizes));
	KASSERT(rw_write_held(&chmp->chm_lock_wbuf));
	KASSERT(pad == WBUF_SETPAD || pad == WBUF_NOPAD);

	/* check padding option */
	if (pad == WBUF_SETPAD) {
		chmp->chm_wbuf_len = PAD(chmp->chm_wbuf_len);
		memset(chmp->chm_wbuf + chmp->chm_wbuf_len, 0,
		    chmp->chm_wbuf_pagesize - chmp->chm_wbuf_len);

		/* add a padding node */
		padnode = (void *)(chmp->chm_wbuf + chmp->chm_wbuf_len);
		padnode->magic = htole16(CHFS_FS_MAGIC_BITMASK);
		padnode->type = htole16(CHFS_NODETYPE_PADDING);
		padnode->length = htole32(chmp->chm_wbuf_pagesize
		    - chmp->chm_wbuf_len);
		padnode->hdr_crc = htole32(crc32(0, (uint8_t *)padnode,
			sizeof(*padnode)-4));

		nref = chfs_alloc_node_ref(chmp->chm_nextblock);
		nref->nref_offset = chmp->chm_wbuf_ofs + chmp->chm_wbuf_len;
		nref->nref_offset = CHFS_GET_OFS(nref->nref_offset) |
		    CHFS_OBSOLETE_NODE_MASK;
		chmp->chm_wbuf_len = chmp->chm_wbuf_pagesize;

		/* change sizes after padding node */
		chfs_change_size_free(chmp, chmp->chm_nextblock,
		    -padnode->length);
		chfs_change_size_wasted(chmp, chmp->chm_nextblock,
		    padnode->length);
	}

	/* write out the buffer */
	ret = chfs_write_leb(chmp, chmp->chm_nextblock->lnr, chmp->chm_wbuf,
	    chmp->chm_wbuf_ofs, chmp->chm_wbuf_len, &retlen);
	if (ret) {
		return ret;
	}

	/* reset the buffer */
	memset(chmp->chm_wbuf, 0xff, chmp->chm_wbuf_pagesize);
	chmp->chm_wbuf_ofs += chmp->chm_wbuf_pagesize;
	chmp->chm_wbuf_len = 0;

	return 0;
}


/*
 * chfs_fill_wbuf - write data to wbuf
 * Return the len of the buf what we didn't write to the wbuf.
 */
static size_t
chfs_fill_wbuf(struct chfs_mount *chmp, const u_char *buf, size_t len)
{
	/* check available space */
	if (len && !chmp->chm_wbuf_len && (len >= chmp->chm_wbuf_pagesize)) {
		return 0;
	}
	/* check buffer's length */
	if (len > (chmp->chm_wbuf_pagesize - chmp->chm_wbuf_len)) {
		len = chmp->chm_wbuf_pagesize - chmp->chm_wbuf_len;
	}
	/* write into the wbuf */
	memcpy(chmp->chm_wbuf + chmp->chm_wbuf_len, buf, len);

	/* update the actual length of writebuffer */
	chmp->chm_wbuf_len += (int) len;
	return len;
}

/*
 * chfs_write_wbuf - write to wbuf and then the flash
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

	if (chmp->chm_wbuf_ofs == 0xffffffff) {
		chmp->chm_wbuf_ofs = PAGE_DIV(to);
		chmp->chm_wbuf_len = PAGE_MOD(to);
		memset(chmp->chm_wbuf, 0xff, chmp->chm_wbuf_pagesize);
	}

	if (EB_ADDRESS(to) != EB_ADDRESS(chmp->chm_wbuf_ofs)) {
		if (chmp->chm_wbuf_len) {
			ret = chfs_flush_wbuf(chmp, WBUF_SETPAD);
			if (ret)
				goto outerr;
		}
		chmp->chm_wbuf_ofs = PAGE_DIV(to);
		chmp->chm_wbuf_len = PAGE_MOD(to);
	}

	if (to != PAD(chmp->chm_wbuf_ofs + chmp->chm_wbuf_len)) {
		dbg("to: %llu != %zu\n", (unsigned long long)to,
			PAD(chmp->chm_wbuf_ofs + chmp->chm_wbuf_len));
		dbg("Non-contiguous write\n");
		panic("BUG\n");
	}

	/* adjust alignment offset */
	if (chmp->chm_wbuf_len != PAGE_MOD(to)) {
		chmp->chm_wbuf_len = PAGE_MOD(to);
		/* take care of alignment to next page */
		if (!chmp->chm_wbuf_len) {
			chmp->chm_wbuf_len += chmp->chm_wbuf_pagesize;
			ret = chfs_flush_wbuf(chmp, WBUF_NOPAD);
			if (ret)
				goto outerr;
		}
	}

	for (invec = 0; invec < count; invec++) {
		int vlen = invecs[invec].iov_len;
		u_char* v = invecs[invec].iov_base;

		/* fill the whole wbuf */
		wbuf_retlen = chfs_fill_wbuf(chmp, v, vlen);
		if (chmp->chm_wbuf_len == chmp->chm_wbuf_pagesize) {
			ret = chfs_flush_wbuf(chmp, WBUF_NOPAD);
			if (ret) {
				goto outerr;
			}
		}

		vlen -= wbuf_retlen;
		outvec_to += wbuf_retlen;
		v += wbuf_retlen;
		donelen += wbuf_retlen;

		/* if there is more residual data than the length of the wbuf
		 * write it out directly until it fits in the wbuf */
		if (vlen >= chmp->chm_wbuf_pagesize) {
			ret = chfs_write_leb(chmp, lnr, v, outvec_to, PAGE_DIV(vlen), &wbuf_retlen);
			vlen -= wbuf_retlen;
			outvec_to += wbuf_retlen;
			chmp->chm_wbuf_ofs = outvec_to;
			v += wbuf_retlen;
			donelen += wbuf_retlen;
		}

		/* write the residual data to the wbuf */
		wbuf_retlen = chfs_fill_wbuf(chmp, v, vlen);
		if (chmp->chm_wbuf_len == chmp->chm_wbuf_pagesize) {
			ret = chfs_flush_wbuf(chmp, WBUF_NOPAD);
			if (ret)
				goto outerr;
		}

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

/*
 * chfs_flush_peding_wbuf - write wbuf to the flash
 * Used when we must flush wbuf right now.
 * If wbuf has free space, pad it to the size of wbuf and write out.
 */
int chfs_flush_pending_wbuf(struct chfs_mount *chmp)
{
	int err;
	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));
	mutex_enter(&chmp->chm_lock_sizes);
	rw_enter(&chmp->chm_lock_wbuf, RW_WRITER);
	err = chfs_flush_wbuf(chmp, WBUF_SETPAD);
	rw_exit(&chmp->chm_lock_wbuf);
	mutex_exit(&chmp->chm_lock_sizes);
	return err;
}

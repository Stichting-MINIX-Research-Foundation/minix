/*	$NetBSD: blist.h,v 1.7 2005/12/11 12:25:20 christos Exp $	*/

/*-
 * Copyright (c) 1998 Matthew Dillon.  All Rights Reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Implements bitmap resource lists.
 *
 *	Usage:
 *		blist = blist_create(blocks)
 *		(void)  blist_destroy(blist)
 *		blkno = blist_alloc(blist, count)
 *		(void)  blist_free(blist, blkno, count)
 *		nblks = blist_fill(blist, blkno, count)
 *		(void)  blist_resize(&blist, count, freeextra)
 *		
 *
 *	Notes:
 *		on creation, the entire list is marked reserved.  You should
 *		first blist_free() the sections you want to make available
 *		for allocation before doing general blist_alloc()/free()
 *		ops.
 *
 *		BLIST_NONE is returned on failure.  This module is typically
 *		capable of managing up to (2^31) blocks per blist, though
 *		the memory utilization would be insane if you actually did
 *		that.  Managing something like 512MB worth of 4K blocks 
 *		eats around 32 KBytes of memory. 
 *
 * $FreeBSD: src/sys/sys/blist.h,v 1.9 2005/01/07 02:29:23 imp Exp $
 */

#ifndef _SYS_BLIST_H_
#define _SYS_BLIST_H_

/*
 * for space efficiency, sizeof(blist_bitmap_t) should be
 * greater than or equal to sizeof(blist_blkno_t).
 */

typedef uint32_t blist_bitmap_t;
typedef uint32_t blist_blkno_t;

/*
 * note: currently use BLIST_NONE as an absolute value rather then 
 * a flag bit.
 */

#define BLIST_NONE	((blist_blkno_t)-1)

typedef struct blist *blist_t;

#define BLIST_BMAP_RADIX	(sizeof(blist_bitmap_t)*8)
#define BLIST_MAX_ALLOC		BLIST_BMAP_RADIX

extern blist_t blist_create(blist_blkno_t blocks);
extern void blist_destroy(blist_t blist);
extern blist_blkno_t blist_alloc(blist_t blist, blist_blkno_t count);
extern void blist_free(blist_t blist, blist_blkno_t blkno, blist_blkno_t count);
extern blist_blkno_t blist_fill(blist_t bl, blist_blkno_t blkno,
    blist_blkno_t count);
extern void blist_print(blist_t blist);
extern void blist_resize(blist_t *pblist, blist_blkno_t count, int freenew);

#endif	/* _SYS_BLIST_H_ */


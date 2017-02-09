/*	$NetBSD: subr_blist.c,v 1.12 2013/12/09 09:35:17 wiz Exp $	*/

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
 * BLIST.C -	Bitmap allocator/deallocator, using a radix tree with hinting
 *
 *	This module implements a general bitmap allocator/deallocator.  The
 *	allocator eats around 2 bits per 'block'.  The module does not 
 *	try to interpret the meaning of a 'block' other than to return 
 *	BLIST_NONE on an allocation failure.
 *
 *	A radix tree is used to maintain the bitmap.  Two radix constants are
 *	involved:  One for the bitmaps contained in the leaf nodes (typically
 *	32), and one for the meta nodes (typically 16).  Both meta and leaf
 *	nodes have a hint field.  This field gives us a hint as to the largest
 *	free contiguous range of blocks under the node.  It may contain a
 *	value that is too high, but will never contain a value that is too 
 *	low.  When the radix tree is searched, allocation failures in subtrees
 *	update the hint. 
 *
 *	The radix tree also implements two collapsed states for meta nodes:
 *	the ALL-ALLOCATED state and the ALL-FREE state.  If a meta node is
 *	in either of these two states, all information contained underneath
 *	the node is considered stale.  These states are used to optimize
 *	allocation and freeing operations.
 *
 * 	The hinting greatly increases code efficiency for allocations while
 *	the general radix structure optimizes both allocations and frees.  The
 *	radix tree should be able to operate well no matter how much 
 *	fragmentation there is and no matter how large a bitmap is used.
 *
 *	Unlike the rlist code, the blist code wires all necessary memory at
 *	creation time.  Neither allocations nor frees require interaction with
 *	the memory subsystem.  In contrast, the rlist code may allocate memory 
 *	on an rlist_free() call.  The non-blocking features of the blist code
 *	are used to great advantage in the swap code (vm/nswap_pager.c).  The
 *	rlist code uses a little less overall memory than the blist code (but
 *	due to swap interleaving not all that much less), but the blist code 
 *	scales much, much better.
 *
 *	LAYOUT: The radix tree is layed out recursively using a
 *	linear array.  Each meta node is immediately followed (layed out
 *	sequentially in memory) by BLIST_META_RADIX lower level nodes.  This
 *	is a recursive structure but one that can be easily scanned through
 *	a very simple 'skip' calculation.  In order to support large radixes, 
 *	portions of the tree may reside outside our memory allocation.  We 
 *	handle this with an early-termination optimization (when bighint is 
 *	set to -1) on the scan.  The memory allocation is only large enough 
 *	to cover the number of blocks requested at creation time even if it
 *	must be encompassed in larger root-node radix.
 *
 *	NOTE: the allocator cannot currently allocate more than 
 *	BLIST_BMAP_RADIX blocks per call.  It will panic with 'allocation too 
 *	large' if you try.  This is an area that could use improvement.  The 
 *	radix is large enough that this restriction does not effect the swap 
 *	system, though.  Currently only the allocation code is effected by
 *	this algorithmic unfeature.  The freeing code can handle arbitrary
 *	ranges.
 *
 *	This code can be compiled stand-alone for debugging.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_blist.c,v 1.12 2013/12/09 09:35:17 wiz Exp $");
#if 0
__FBSDID("$FreeBSD: src/sys/kern/subr_blist.c,v 1.17 2004/06/04 04:03:25 alc Exp $");
#endif

#ifdef _KERNEL

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/blist.h>
#include <sys/kmem.h>

#else

#ifndef BLIST_NO_DEBUG
#define BLIST_DEBUG
#endif

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>

#define	KM_SLEEP 1
#define	kmem_zalloc(a,b,c) calloc(1, (a))
#define	kmem_alloc(a,b,c) malloc(a)
#define	kmem_free(a,b) free(a)

#include "../sys/blist.h"

void panic(const char *ctl, ...) __printflike(1, 2);

#endif

/*
 * blmeta and bl_bitmap_t MUST be a power of 2 in size.
 */

typedef struct blmeta {
	union {
		blist_blkno_t	bmu_avail; /* space available under us	*/
		blist_bitmap_t	bmu_bitmap; /* bitmap if we are a leaf	*/
	} u;
	blist_blkno_t	bm_bighint;	/* biggest contiguous block hint*/
} blmeta_t;

struct blist {
	blist_blkno_t		bl_blocks;	/* area of coverage		*/
	blist_blkno_t		bl_radix;	/* coverage radix		*/
	blist_blkno_t		bl_skip;	/* starting skip		*/
	blist_blkno_t		bl_free;	/* number of free blocks	*/
	blmeta_t	*bl_root;	/* root of radix tree		*/
	blist_blkno_t		bl_rootblks;	/* blks allocated for tree */
};

#define BLIST_META_RADIX	16

/*
 * static support functions
 */

static blist_blkno_t blst_leaf_alloc(blmeta_t *scan, blist_blkno_t blk,
    int count);
static blist_blkno_t blst_meta_alloc(blmeta_t *scan, blist_blkno_t blk, 
    blist_blkno_t count, blist_blkno_t radix, blist_blkno_t skip);
static void blst_leaf_free(blmeta_t *scan, blist_blkno_t relblk, int count);
static void blst_meta_free(blmeta_t *scan, blist_blkno_t freeBlk,
    blist_blkno_t count, blist_blkno_t radix, blist_blkno_t skip,
    blist_blkno_t blk);
static void blst_copy(blmeta_t *scan, blist_blkno_t blk, blist_blkno_t radix, 
    blist_blkno_t skip, blist_t dest, blist_blkno_t count);
static int blst_leaf_fill(blmeta_t *scan, blist_blkno_t blk, int count);
static blist_blkno_t blst_meta_fill(blmeta_t *scan, blist_blkno_t allocBlk,
    blist_blkno_t count, blist_blkno_t radix, blist_blkno_t skip,
    blist_blkno_t blk);
static blist_blkno_t blst_radix_init(blmeta_t *scan, blist_blkno_t radix, 
    blist_blkno_t skip, blist_blkno_t count);
#ifndef _KERNEL
static void blst_radix_print(blmeta_t *scan, blist_blkno_t blk,
    blist_blkno_t radix, blist_blkno_t skip, int tab);
#endif

/*
 * blist_create() - create a blist capable of handling up to the specified
 *		    number of blocks
 *
 *	blocks must be greater than 0
 *
 *	The smallest blist consists of a single leaf node capable of 
 *	managing BLIST_BMAP_RADIX blocks.
 */

blist_t 
blist_create(blist_blkno_t blocks)
{
	blist_t bl;
	blist_blkno_t radix;
	blist_blkno_t skip = 0;

	/*
	 * Calculate radix and skip field used for scanning.
	 *
	 * XXX check overflow
	 */
	radix = BLIST_BMAP_RADIX;

	while (radix < blocks) {
		radix *= BLIST_META_RADIX;
		skip = (skip + 1) * BLIST_META_RADIX;
	}

	bl = kmem_zalloc(sizeof(struct blist), KM_SLEEP);

	bl->bl_blocks = blocks;
	bl->bl_radix = radix;
	bl->bl_skip = skip;
	bl->bl_rootblks = 1 +
	    blst_radix_init(NULL, bl->bl_radix, bl->bl_skip, blocks);
	bl->bl_root = kmem_alloc(sizeof(blmeta_t) * bl->bl_rootblks, KM_SLEEP);

#if defined(BLIST_DEBUG)
	printf(
		"BLIST representing %" PRIu64 " blocks (%" PRIu64 " MB of swap)"
		", requiring %" PRIu64 "K of ram\n",
		(uint64_t)bl->bl_blocks,
		(uint64_t)bl->bl_blocks * 4 / 1024,
		((uint64_t)bl->bl_rootblks * sizeof(blmeta_t) + 1023) / 1024
	);
	printf("BLIST raw radix tree contains %" PRIu64 " records\n",
	    (uint64_t)bl->bl_rootblks);
#endif
	blst_radix_init(bl->bl_root, bl->bl_radix, bl->bl_skip, blocks);

	return(bl);
}

void 
blist_destroy(blist_t bl)
{

	kmem_free(bl->bl_root, sizeof(blmeta_t) * bl->bl_rootblks);
	kmem_free(bl, sizeof(struct blist));
}

/*
 * blist_alloc() - reserve space in the block bitmap.  Return the base
 *		     of a contiguous region or BLIST_NONE if space could
 *		     not be allocated.
 */

blist_blkno_t 
blist_alloc(blist_t bl, blist_blkno_t count)
{
	blist_blkno_t blk = BLIST_NONE;

	if (bl) {
		if (bl->bl_radix == BLIST_BMAP_RADIX)
			blk = blst_leaf_alloc(bl->bl_root, 0, count);
		else
			blk = blst_meta_alloc(bl->bl_root, 0, count, bl->bl_radix, bl->bl_skip);
		if (blk != BLIST_NONE)
			bl->bl_free -= count;
	}
	return(blk);
}

/*
 * blist_free() -	free up space in the block bitmap.  Return the base
 *		     	of a contiguous region.  Panic if an inconsistancy is
 *			found.
 */

void 
blist_free(blist_t bl, blist_blkno_t blkno, blist_blkno_t count)
{
	if (bl) {
		if (bl->bl_radix == BLIST_BMAP_RADIX)
			blst_leaf_free(bl->bl_root, blkno, count);
		else
			blst_meta_free(bl->bl_root, blkno, count, bl->bl_radix, bl->bl_skip, 0);
		bl->bl_free += count;
	}
}

/*
 * blist_fill() -	mark a region in the block bitmap as off-limits
 *			to the allocator (i.e. allocate it), ignoring any
 *			existing allocations.  Return the number of blocks
 *			actually filled that were free before the call.
 */

blist_blkno_t
blist_fill(blist_t bl, blist_blkno_t blkno, blist_blkno_t count)
{
	blist_blkno_t filled;

	if (bl) {
		if (bl->bl_radix == BLIST_BMAP_RADIX)
			filled = blst_leaf_fill(bl->bl_root, blkno, count);
		else
			filled = blst_meta_fill(bl->bl_root, blkno, count,
			    bl->bl_radix, bl->bl_skip, 0);
		bl->bl_free -= filled;
		return filled;
	} else
		return 0;
}

/*
 * blist_resize() -	resize an existing radix tree to handle the
 *			specified number of blocks.  This will reallocate
 *			the tree and transfer the previous bitmap to the new
 *			one.  When extending the tree you can specify whether
 *			the new blocks are to left allocated or freed.
 */

void
blist_resize(blist_t *pbl, blist_blkno_t count, int freenew)
{
    blist_t newbl = blist_create(count);
    blist_t save = *pbl;

    *pbl = newbl;
    if (count > save->bl_blocks)
	    count = save->bl_blocks;
    blst_copy(save->bl_root, 0, save->bl_radix, save->bl_skip, newbl, count);

    /*
     * If resizing upwards, should we free the new space or not?
     */
    if (freenew && count < newbl->bl_blocks) {
	    blist_free(newbl, count, newbl->bl_blocks - count);
    }
    blist_destroy(save);
}

#ifdef BLIST_DEBUG

/*
 * blist_print()    - dump radix tree
 */

void
blist_print(blist_t bl)
{
	printf("BLIST {\n");
	blst_radix_print(bl->bl_root, 0, bl->bl_radix, bl->bl_skip, 4);
	printf("}\n");
}

#endif

/************************************************************************
 *			  ALLOCATION SUPPORT FUNCTIONS			*
 ************************************************************************
 *
 *	These support functions do all the actual work.  They may seem 
 *	rather longish, but that's because I've commented them up.  The
 *	actual code is straight forward.
 *
 */

/*
 * blist_leaf_alloc() -	allocate at a leaf in the radix tree (a bitmap).
 *
 *	This is the core of the allocator and is optimized for the 1 block
 *	and the BLIST_BMAP_RADIX block allocation cases.  Other cases are
 *	somewhat slower.  The 1 block allocation case is log2 and extremely
 *	quick.
 */

static blist_blkno_t
blst_leaf_alloc(
	blmeta_t *scan,
	blist_blkno_t blk,
	int count
) {
	blist_bitmap_t orig = scan->u.bmu_bitmap;

	if (orig == 0) {
		/*
		 * Optimize bitmap all-allocated case.  Also, count = 1
		 * case assumes at least 1 bit is free in the bitmap, so
		 * we have to take care of this case here.
		 */
		scan->bm_bighint = 0;
		return(BLIST_NONE);
	}
	if (count == 1) {
		/*
		 * Optimized code to allocate one bit out of the bitmap
		 */
		blist_bitmap_t mask;
		int j = BLIST_BMAP_RADIX/2;
		int r = 0;

		mask = (blist_bitmap_t)-1 >> (BLIST_BMAP_RADIX/2);

		while (j) {
			if ((orig & mask) == 0) {
			    r += j;
			    orig >>= j;
			}
			j >>= 1;
			mask >>= j;
		}
		scan->u.bmu_bitmap &= ~((blist_bitmap_t)1 << r);
		return(blk + r);
	}
	if (count <= BLIST_BMAP_RADIX) {
		/*
		 * non-optimized code to allocate N bits out of the bitmap.
		 * The more bits, the faster the code runs.  It will run
		 * the slowest allocating 2 bits, but since there aren't any
		 * memory ops in the core loop (or shouldn't be, anyway),
		 * you probably won't notice the difference.
		 */
		int j;
		int n = BLIST_BMAP_RADIX - count;
		blist_bitmap_t mask;

		mask = (blist_bitmap_t)-1 >> n;

		for (j = 0; j <= n; ++j) {
			if ((orig & mask) == mask) {
				scan->u.bmu_bitmap &= ~mask;
				return(blk + j);
			}
			mask = (mask << 1);
		}
	}
	/*
	 * We couldn't allocate count in this subtree, update bighint.
	 */
	scan->bm_bighint = count - 1;
	return(BLIST_NONE);
}

/*
 * blist_meta_alloc() -	allocate at a meta in the radix tree.
 *
 *	Attempt to allocate at a meta node.  If we can't, we update
 *	bighint and return a failure.  Updating bighint optimize future
 *	calls that hit this node.  We have to check for our collapse cases
 *	and we have a few optimizations strewn in as well.
 */

static blist_blkno_t
blst_meta_alloc(
	blmeta_t *scan, 
	blist_blkno_t blk,
	blist_blkno_t count,
	blist_blkno_t radix, 
	blist_blkno_t skip
) {
	blist_blkno_t i;
	blist_blkno_t next_skip = (skip / BLIST_META_RADIX);

	if (scan->u.bmu_avail == 0)  {
		/*
		 * ALL-ALLOCATED special case
		 */
		scan->bm_bighint = count;
		return(BLIST_NONE);
	}

	if (scan->u.bmu_avail == radix) {
		radix /= BLIST_META_RADIX;

		/*
		 * ALL-FREE special case, initialize uninitialize
		 * sublevel.
		 */
		for (i = 1; i <= skip; i += next_skip) {
			if (scan[i].bm_bighint == (blist_blkno_t)-1)
				break;
			if (next_skip == 1) {
				scan[i].u.bmu_bitmap = (blist_bitmap_t)-1;
				scan[i].bm_bighint = BLIST_BMAP_RADIX;
			} else {
				scan[i].bm_bighint = radix;
				scan[i].u.bmu_avail = radix;
			}
		}
	} else {
		radix /= BLIST_META_RADIX;
	}

	for (i = 1; i <= skip; i += next_skip) {
		if (scan[i].bm_bighint == (blist_blkno_t)-1) {
			/*
			 * Terminator
			 */
			break;
		} else if (count <= scan[i].bm_bighint) {
			/*
			 * count fits in object
			 */
			blist_blkno_t r;
			if (next_skip == 1) {
				r = blst_leaf_alloc(&scan[i], blk, count);
			} else {
				r = blst_meta_alloc(&scan[i], blk, count, radix, next_skip - 1);
			}
			if (r != BLIST_NONE) {
				scan->u.bmu_avail -= count;
				if (scan->bm_bighint > scan->u.bmu_avail)
					scan->bm_bighint = scan->u.bmu_avail;
				return(r);
			}
		} else if (count > radix) {
			/*
			 * count does not fit in object even if it were
			 * complete free.
			 */
			panic("blist_meta_alloc: allocation too large");
		}
		blk += radix;
	}

	/*
	 * We couldn't allocate count in this subtree, update bighint.
	 */
	if (scan->bm_bighint >= count)
		scan->bm_bighint = count - 1;
	return(BLIST_NONE);
}

/*
 * BLST_LEAF_FREE() -	free allocated block from leaf bitmap
 *
 */

static void
blst_leaf_free(
	blmeta_t *scan,
	blist_blkno_t blk,
	int count
) {
	/*
	 * free some data in this bitmap
	 *
	 * e.g.
	 *	0000111111111110000
	 *          \_________/\__/
	 *		v        n
	 */
	int n = blk & (BLIST_BMAP_RADIX - 1);
	blist_bitmap_t mask;

	mask = ((blist_bitmap_t)-1 << n) &
	    ((blist_bitmap_t)-1 >> (BLIST_BMAP_RADIX - count - n));

	if (scan->u.bmu_bitmap & mask)
		panic("blst_radix_free: freeing free block");
	scan->u.bmu_bitmap |= mask;

	/*
	 * We could probably do a better job here.  We are required to make
	 * bighint at least as large as the biggest contiguous block of 
	 * data.  If we just shoehorn it, a little extra overhead will
	 * be incured on the next allocation (but only that one typically).
	 */
	scan->bm_bighint = BLIST_BMAP_RADIX;
}

/*
 * BLST_META_FREE() - free allocated blocks from radix tree meta info
 *
 *	This support routine frees a range of blocks from the bitmap.
 *	The range must be entirely enclosed by this radix node.  If a
 *	meta node, we break the range down recursively to free blocks
 *	in subnodes (which means that this code can free an arbitrary
 *	range whereas the allocation code cannot allocate an arbitrary
 *	range).
 */

static void 
blst_meta_free(
	blmeta_t *scan, 
	blist_blkno_t freeBlk,
	blist_blkno_t count,
	blist_blkno_t radix, 
	blist_blkno_t skip,
	blist_blkno_t blk
) {
	blist_blkno_t i;
	blist_blkno_t next_skip = (skip / BLIST_META_RADIX);

#if 0
	printf("FREE (%" PRIx64 ",%" PRIu64
	    ") FROM (%" PRIx64 ",%" PRIu64 ")\n",
	    (uint64_t)freeBlk, (uint64_t)count,
	    (uint64_t)blk, (uint64_t)radix
	);
#endif

	if (scan->u.bmu_avail == 0) {
		/*
		 * ALL-ALLOCATED special case, with possible
		 * shortcut to ALL-FREE special case.
		 */
		scan->u.bmu_avail = count;
		scan->bm_bighint = count;

		if (count != radix)  {
			for (i = 1; i <= skip; i += next_skip) {
				if (scan[i].bm_bighint == (blist_blkno_t)-1)
					break;
				scan[i].bm_bighint = 0;
				if (next_skip == 1) {
					scan[i].u.bmu_bitmap = 0;
				} else {
					scan[i].u.bmu_avail = 0;
				}
			}
			/* fall through */
		}
	} else {
		scan->u.bmu_avail += count;
		/* scan->bm_bighint = radix; */
	}

	/*
	 * ALL-FREE special case.
	 */

	if (scan->u.bmu_avail == radix)
		return;
	if (scan->u.bmu_avail > radix)
		panic("blst_meta_free: freeing already free blocks (%"
		    PRIu64 ") %" PRIu64 "/%" PRIu64,
		    (uint64_t)count,
		    (uint64_t)scan->u.bmu_avail,
		    (uint64_t)radix);

	/*
	 * Break the free down into its components
	 */

	radix /= BLIST_META_RADIX;

	i = (freeBlk - blk) / radix;
	blk += i * radix;
	i = i * next_skip + 1;

	while (i <= skip && blk < freeBlk + count) {
		blist_blkno_t v;

		v = blk + radix - freeBlk;
		if (v > count)
			v = count;

		if (scan->bm_bighint == (blist_blkno_t)-1)
			panic("blst_meta_free: freeing unexpected range");

		if (next_skip == 1) {
			blst_leaf_free(&scan[i], freeBlk, v);
		} else {
			blst_meta_free(&scan[i], freeBlk, v, radix, next_skip - 1, blk);
		}
		if (scan->bm_bighint < scan[i].bm_bighint)
		    scan->bm_bighint = scan[i].bm_bighint;
		count -= v;
		freeBlk += v;
		blk += radix;
		i += next_skip;
	}
}

/*
 * BLIST_RADIX_COPY() - copy one radix tree to another
 *
 *	Locates free space in the source tree and frees it in the destination
 *	tree.  The space may not already be free in the destination.
 */

static void blst_copy(
	blmeta_t *scan, 
	blist_blkno_t blk,
	blist_blkno_t radix, 
	blist_blkno_t skip, 
	blist_t dest,
	blist_blkno_t count
) {
	blist_blkno_t next_skip;
	blist_blkno_t i;

	/*
	 * Leaf node
	 */

	if (radix == BLIST_BMAP_RADIX) {
		blist_bitmap_t v = scan->u.bmu_bitmap;

		if (v == (blist_bitmap_t)-1) {
			blist_free(dest, blk, count);
		} else if (v != 0) {
			int j;

			for (j = 0; j < BLIST_BMAP_RADIX && j < count; ++j) {
				if (v & (1 << j))
					blist_free(dest, blk + j, 1);
			}
		}
		return;
	}

	/*
	 * Meta node
	 */

	if (scan->u.bmu_avail == 0) {
		/*
		 * Source all allocated, leave dest allocated
		 */
		return;
	} 
	if (scan->u.bmu_avail == radix) {
		/*
		 * Source all free, free entire dest
		 */
		if (count < radix)
			blist_free(dest, blk, count);
		else
			blist_free(dest, blk, radix);
		return;
	}


	radix /= BLIST_META_RADIX;
	next_skip = (skip / BLIST_META_RADIX);

	for (i = 1; count && i <= skip; i += next_skip) {
		if (scan[i].bm_bighint == (blist_blkno_t)-1)
			break;

		if (count >= radix) {
			blst_copy(
			    &scan[i],
			    blk,
			    radix,
			    next_skip - 1,
			    dest,
			    radix
			);
			count -= radix;
		} else {
			if (count) {
				blst_copy(
				    &scan[i],
				    blk,
				    radix,
				    next_skip - 1,
				    dest,
				    count
				);
			}
			count = 0;
		}
		blk += radix;
	}
}

/*
 * BLST_LEAF_FILL() -	allocate specific blocks in leaf bitmap
 *
 *	This routine allocates all blocks in the specified range
 *	regardless of any existing allocations in that range.  Returns
 *	the number of blocks allocated by the call.
 */

static int
blst_leaf_fill(blmeta_t *scan, blist_blkno_t blk, int count)
{
	int n = blk & (BLIST_BMAP_RADIX - 1);
	int nblks;
	blist_bitmap_t mask, bitmap;

	mask = ((blist_bitmap_t)-1 << n) &
	    ((blist_bitmap_t)-1 >> (BLIST_BMAP_RADIX - count - n));

	/* Count the number of blocks we're about to allocate */
	bitmap = scan->u.bmu_bitmap & mask;
	for (nblks = 0; bitmap != 0; nblks++)
		bitmap &= bitmap - 1;

	scan->u.bmu_bitmap &= ~mask;
	return nblks;
}

/*
 * BLIST_META_FILL() -	allocate specific blocks at a meta node
 *
 *	This routine allocates the specified range of blocks,
 *	regardless of any existing allocations in the range.  The
 *	range must be within the extent of this node.  Returns the
 *	number of blocks allocated by the call.
 */
static blist_blkno_t
blst_meta_fill(
	blmeta_t *scan,
	blist_blkno_t allocBlk,
	blist_blkno_t count,
	blist_blkno_t radix, 
	blist_blkno_t skip,
	blist_blkno_t blk
) {
	blist_blkno_t i;
	blist_blkno_t next_skip = (skip / BLIST_META_RADIX);
	blist_blkno_t nblks = 0;

	if (count == radix || scan->u.bmu_avail == 0)  {
		/*
		 * ALL-ALLOCATED special case
		 */
		nblks = scan->u.bmu_avail;
		scan->u.bmu_avail = 0;
		scan->bm_bighint = count;
		return nblks;
	}

	if (count > radix)
		panic("blist_meta_fill: allocation too large");

	if (scan->u.bmu_avail == radix) {
		radix /= BLIST_META_RADIX;

		/*
		 * ALL-FREE special case, initialize sublevel
		 */
		for (i = 1; i <= skip; i += next_skip) {
			if (scan[i].bm_bighint == (blist_blkno_t)-1)
				break;
			if (next_skip == 1) {
				scan[i].u.bmu_bitmap = (blist_bitmap_t)-1;
				scan[i].bm_bighint = BLIST_BMAP_RADIX;
			} else {
				scan[i].bm_bighint = radix;
				scan[i].u.bmu_avail = radix;
			}
		}
	} else {
		radix /= BLIST_META_RADIX;
	}

	i = (allocBlk - blk) / radix;
	blk += i * radix;
	i = i * next_skip + 1;

	while (i <= skip && blk < allocBlk + count) {
		blist_blkno_t v;

		v = blk + radix - allocBlk;
		if (v > count)
			v = count;

		if (scan->bm_bighint == (blist_blkno_t)-1)
			panic("blst_meta_fill: filling unexpected range");

		if (next_skip == 1) {
			nblks += blst_leaf_fill(&scan[i], allocBlk, v);
		} else {
			nblks += blst_meta_fill(&scan[i], allocBlk, v,
			    radix, next_skip - 1, blk);
		}
		count -= v;
		allocBlk += v;
		blk += radix;
		i += next_skip;
	}
	scan->u.bmu_avail -= nblks;
	return nblks;
}

/*
 * BLST_RADIX_INIT() - initialize radix tree
 *
 *	Initialize our meta structures and bitmaps and calculate the exact
 *	amount of space required to manage 'count' blocks - this space may
 *	be considerably less than the calculated radix due to the large
 *	RADIX values we use.
 */

static blist_blkno_t	
blst_radix_init(blmeta_t *scan, blist_blkno_t radix, blist_blkno_t skip,
    blist_blkno_t count)
{
	blist_blkno_t i;
	blist_blkno_t next_skip;
	blist_blkno_t memindex = 0;

	/*
	 * Leaf node
	 */

	if (radix == BLIST_BMAP_RADIX) {
		if (scan) {
			scan->bm_bighint = 0;
			scan->u.bmu_bitmap = 0;
		}
		return(memindex);
	}

	/*
	 * Meta node.  If allocating the entire object we can special
	 * case it.  However, we need to figure out how much memory
	 * is required to manage 'count' blocks, so we continue on anyway.
	 */

	if (scan) {
		scan->bm_bighint = 0;
		scan->u.bmu_avail = 0;
	}

	radix /= BLIST_META_RADIX;
	next_skip = (skip / BLIST_META_RADIX);

	for (i = 1; i <= skip; i += next_skip) {
		if (count >= radix) {
			/*
			 * Allocate the entire object
			 */
			memindex = i + blst_radix_init(
			    ((scan) ? &scan[i] : NULL),
			    radix,
			    next_skip - 1,
			    radix
			);
			count -= radix;
		} else if (count > 0) {
			/*
			 * Allocate a partial object
			 */
			memindex = i + blst_radix_init(
			    ((scan) ? &scan[i] : NULL),
			    radix,
			    next_skip - 1,
			    count
			);
			count = 0;
		} else {
			/*
			 * Add terminator and break out
			 */
			if (scan)
				scan[i].bm_bighint = (blist_blkno_t)-1;
			break;
		}
	}
	if (memindex < i)
		memindex = i;
	return(memindex);
}

#ifdef BLIST_DEBUG

static void	
blst_radix_print(blmeta_t *scan, blist_blkno_t blk, blist_blkno_t radix,
    blist_blkno_t skip, int tab)
{
	blist_blkno_t i;
	blist_blkno_t next_skip;
	int lastState = 0;

	if (radix == BLIST_BMAP_RADIX) {
		printf(
		    "%*.*s(%0*" PRIx64 ",%" PRIu64
		    "): bitmap %0*" PRIx64 " big=%" PRIu64 "\n", 
		    tab, tab, "",
		    sizeof(blk) * 2,
		    (uint64_t)blk,
		    (uint64_t)radix,
		    sizeof(scan->u.bmu_bitmap) * 2,
		    (uint64_t)scan->u.bmu_bitmap,
		    (uint64_t)scan->bm_bighint
		);
		return;
	}

	if (scan->u.bmu_avail == 0) {
		printf(
		    "%*.*s(%0*" PRIx64 ",%" PRIu64") ALL ALLOCATED\n",
		    tab, tab, "",
		    sizeof(blk) * 2,
		    (uint64_t)blk,
		    (uint64_t)radix
		);
		return;
	}
	if (scan->u.bmu_avail == radix) {
		printf(
		    "%*.*s(%0*" PRIx64 ",%" PRIu64 ") ALL FREE\n",
		    tab, tab, "",
		    sizeof(blk) * 2,
		    (uint64_t)blk,
		    (uint64_t)radix
		);
		return;
	}

	printf(
	    "%*.*s(%0*" PRIx64 ",%" PRIu64 "): subtree (%" PRIu64 "/%"
	    PRIu64 ") big=%" PRIu64 " {\n",
	    tab, tab, "",
	    sizeof(blk) * 2,
	    (uint64_t)blk,
	    (uint64_t)radix,
	    (uint64_t)scan->u.bmu_avail,
	    (uint64_t)radix,
	    (uint64_t)scan->bm_bighint
	);

	radix /= BLIST_META_RADIX;
	next_skip = (skip / BLIST_META_RADIX);
	tab += 4;

	for (i = 1; i <= skip; i += next_skip) {
		if (scan[i].bm_bighint == (blist_blkno_t)-1) {
			printf(
			    "%*.*s(%0*" PRIx64 ",%" PRIu64 "): Terminator\n",
			    tab, tab, "",
			    sizeof(blk) * 2,
			    (uint64_t)blk,
			    (uint64_t)radix
			);
			lastState = 0;
			break;
		}
		blst_radix_print(
		    &scan[i],
		    blk,
		    radix,
		    next_skip - 1,
		    tab
		);
		blk += radix;
	}
	tab -= 4;

	printf(
	    "%*.*s}\n",
	    tab, tab, ""
	);
}

#endif

#ifdef BLIST_DEBUG

int
main(int ac, char **av)
{
	blist_blkno_t size = 1024;
	int i;
	blist_t bl;

	for (i = 1; i < ac; ++i) {
		const char *ptr = av[i];
		if (*ptr != '-') {
			size = strtol(ptr, NULL, 0);
			continue;
		}
		ptr += 2;
		fprintf(stderr, "Bad option: %s\n", ptr - 2);
		exit(1);
	}
	bl = blist_create(size);
	blist_free(bl, 0, size);

	for (;;) {
		char buf[1024];
		uint64_t da = 0;
		uint64_t count = 0;

		printf("%" PRIu64 "/%" PRIu64 "/%" PRIu64 "> ",
		    (uint64_t)bl->bl_free,
		    (uint64_t)size,
		    (uint64_t)bl->bl_radix);
		fflush(stdout);
		if (fgets(buf, sizeof(buf), stdin) == NULL)
			break;
		switch(buf[0]) {
		case 'r':
			if (sscanf(buf + 1, "%" SCNu64, &count) == 1) {
				blist_resize(&bl, count, 1);
			} else {
				printf("?\n");
			}
		case 'p':
			blist_print(bl);
			break;
		case 'a':
			if (sscanf(buf + 1, "%" SCNu64, &count) == 1) {
				blist_blkno_t blk = blist_alloc(bl, count);
				printf("    R=%0*" PRIx64 "\n",
				    sizeof(blk) * 2,
				    (uint64_t)blk);
			} else {
				printf("?\n");
			}
			break;
		case 'f':
			if (sscanf(buf + 1, "%" SCNx64 " %" SCNu64,
			    &da, &count) == 2) {
				blist_free(bl, da, count);
			} else {
				printf("?\n");
			}
			break;
		case 'l':
			if (sscanf(buf + 1, "%" SCNx64 " %" SCNu64,
			    &da, &count) == 2) {
				printf("    n=%" PRIu64 "\n",
				    (uint64_t)blist_fill(bl, da, count));
			} else {
				printf("?\n");
			}
			break;
		case '?':
		case 'h':
			puts(
			    "p          -print\n"
			    "a %d       -allocate\n"
			    "f %x %d    -free\n"
			    "l %x %d    -fill\n"
			    "r %d       -resize\n"
			    "h/?        -help"
			);
			break;
		default:
			printf("?\n");
			break;
		}
	}
	return(0);
}

void
panic(const char *ctl, ...)
{
	va_list va;

	va_start(va, ctl);
	vfprintf(stderr, ctl, va);
	fprintf(stderr, "\n");
	va_end(va);
	exit(1);
}

#endif


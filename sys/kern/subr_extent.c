/*	$NetBSD: subr_extent.c,v 1.79 2015/08/24 22:50:32 pooka Exp $	*/

/*-
 * Copyright (c) 1996, 1998, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe and Matthias Drochner.
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
 * General purpose extent manager.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_extent.c,v 1.79 2015/08/24 22:50:32 pooka Exp $");

#ifdef _KERNEL
#ifdef _KERNEL_OPT
#include "opt_lockdebug.h"
#endif

#include <sys/param.h>
#include <sys/extent.h>
#include <sys/kmem.h>
#include <sys/pool.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <uvm/uvm_extern.h>

#elif defined(_EXTENT_TESTING)

/*
 * user-land definitions, so it can fit into a testing harness.
 */
#include <sys/param.h>
#include <sys/pool.h>
#include <sys/extent.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
 * Use multi-line #defines to avoid screwing up the kernel tags file;
 * without this, ctags produces a tags file where panic() shows up
 * in subr_extent.c rather than subr_prf.c.
 */
#define	\
kmem_alloc(s, flags)		malloc(s)
#define	\
kmem_free(p, s)			free(p)
#define	\
cv_wait_sig(cv, lock)		(EWOULDBLOCK)
#define	\
pool_get(pool, flags)		kmem_alloc((pool)->pr_size,0)
#define	\
pool_put(pool, rp)		kmem_free(rp,0)
#define	\
panic(a)			printf(a)
#define	mutex_init(a, b, c)
#define	mutex_destroy(a)
#define	mutex_enter(l)
#define	mutex_exit(l)
#define	cv_wait(cv, lock)
#define	cv_broadcast(cv)
#define	cv_init(a, b)
#define	cv_destroy(a)
#define	KMEM_IS_RUNNING			(1)
#define	IPL_VM				(0)
#define	MUTEX_DEFAULT			(0)
#endif

static struct pool expool;

/*
 * Macro to align to an arbitrary power-of-two boundary.
 */
#define EXTENT_ALIGN(_start, _align, _skew)		\
	(((((_start) - (_skew)) + ((_align) - 1)) & (-(_align))) + (_skew))

/*
 * Create the extent_region pool.
 */
void
extent_init(void)
{

#if defined(_KERNEL)
	pool_init(&expool, sizeof(struct extent_region), 0, 0, 0,
	    "extent", NULL, IPL_VM);
#else
	expool.pr_size = sizeof(struct extent_region);
#endif
}

/*
 * Allocate an extent region descriptor.  EXTENT MUST NOT BE LOCKED.
 * We will handle any locking we may need.
 */
static struct extent_region *
extent_alloc_region_descriptor(struct extent *ex, int flags)
{
	struct extent_region *rp;
	int exflags, error;

	/*
	 * XXX Make a static, create-time flags word, so we don't
	 * XXX have to lock to read it!
	 */
	mutex_enter(&ex->ex_lock);
	exflags = ex->ex_flags;
	mutex_exit(&ex->ex_lock);

	if (exflags & EXF_FIXED) {
		struct extent_fixed *fex = (struct extent_fixed *)ex;

		mutex_enter(&ex->ex_lock);
		for (;;) {
			if ((rp = LIST_FIRST(&fex->fex_freelist)) != NULL) {
				/*
				 * Don't muck with flags after pulling it off
				 * the freelist; it may have been dynamically
				 * allocated, and kindly given to us.  We
				 * need to remember that information.
				 */
				LIST_REMOVE(rp, er_link);
				mutex_exit(&ex->ex_lock);
				return (rp);
			}
			if (flags & EX_MALLOCOK) {
				mutex_exit(&ex->ex_lock);
				goto alloc;
			}
			if ((flags & EX_WAITOK) == 0) {
				mutex_exit(&ex->ex_lock);
				return (NULL);
			}
			ex->ex_flags |= EXF_FLWANTED;
			if ((flags & EX_CATCH) != 0)
				error = cv_wait_sig(&ex->ex_cv, &ex->ex_lock);
			else {
				cv_wait(&ex->ex_cv, &ex->ex_lock);
				error = 0;
			}
			if (error != 0) {
				mutex_exit(&ex->ex_lock);
				return (NULL);
			}
		}
	}

 alloc:
	rp = pool_get(&expool, (flags & EX_WAITOK) ? PR_WAITOK : 0);

	if (rp != NULL)
		rp->er_flags = ER_ALLOC;

	return (rp);
}

/*
 * Free an extent region descriptor.  EXTENT _MUST_ BE LOCKED!
 */
static void
extent_free_region_descriptor(struct extent *ex, struct extent_region *rp)
{

	if (ex->ex_flags & EXF_FIXED) {
		struct extent_fixed *fex = (struct extent_fixed *)ex;

		/*
		 * If someone's waiting for a region descriptor,
		 * be nice and give them this one, rather than
		 * just free'ing it back to the system.
		 */
		if (rp->er_flags & ER_ALLOC) {
			if (ex->ex_flags & EXF_FLWANTED) {
				/* Clear all but ER_ALLOC flag. */
				rp->er_flags = ER_ALLOC;
				LIST_INSERT_HEAD(&fex->fex_freelist, rp,
				    er_link);
				goto wake_em_up;
			} else
				pool_put(&expool, rp);
		} else {
			/* Clear all flags. */
			rp->er_flags = 0;
			LIST_INSERT_HEAD(&fex->fex_freelist, rp, er_link);
		}

 wake_em_up:
		ex->ex_flags &= ~EXF_FLWANTED;
		cv_broadcast(&ex->ex_cv);
		return;
	}

	/*
	 * We know it's dynamically allocated if we get here.
	 */
	pool_put(&expool, rp);
}

/*
 * Allocate and initialize an extent map.
 */
struct extent *
extent_create(const char *name, u_long start, u_long end,
    void *storage, size_t storagesize, int flags)
{
	struct extent *ex;
	char *cp = storage;
	size_t sz = storagesize;
	struct extent_region *rp;
	int fixed_extent = (storage != NULL);

#ifndef _KERNEL
	extent_init();
#endif

#ifdef DIAGNOSTIC
	/* Check arguments. */
	if (name == NULL)
		panic("extent_create: name == NULL");
	if (end < start) {
		printf("extent_create: extent `%s', start 0x%lx, end 0x%lx\n",
		    name, start, end);
		panic("extent_create: end < start");
	}
	if (fixed_extent && (storagesize < sizeof(struct extent_fixed)))
		panic("extent_create: fixed extent, bad storagesize 0x%lx",
		    (u_long)storagesize);
	if (fixed_extent == 0 && (storagesize != 0 || storage != NULL))
		panic("extent_create: storage provided for non-fixed");
#endif

	/* Allocate extent descriptor. */
	if (fixed_extent) {
		struct extent_fixed *fex;

		memset(storage, 0, storagesize);

		/*
		 * Align all descriptors on "long" boundaries.
		 */
		fex = (struct extent_fixed *)cp;
		ex = (struct extent *)fex;
		cp += ALIGN(sizeof(struct extent_fixed));
		sz -= ALIGN(sizeof(struct extent_fixed));
		fex->fex_storage = storage;
		fex->fex_storagesize = storagesize;

		/*
		 * In a fixed extent, we have to pre-allocate region
		 * descriptors and place them in the extent's freelist.
		 */
		LIST_INIT(&fex->fex_freelist);
		while (sz >= ALIGN(sizeof(struct extent_region))) {
			rp = (struct extent_region *)cp;
			cp += ALIGN(sizeof(struct extent_region));
			sz -= ALIGN(sizeof(struct extent_region));
			LIST_INSERT_HEAD(&fex->fex_freelist, rp, er_link);
		}
	} else {
		ex = kmem_alloc(sizeof(*ex),
		    (flags & EX_WAITOK) ? KM_SLEEP : KM_NOSLEEP);
		if (ex == NULL)
			return (NULL);
	}

	/* Fill in the extent descriptor and return it to the caller. */
	mutex_init(&ex->ex_lock, MUTEX_DEFAULT, IPL_VM);
	cv_init(&ex->ex_cv, "extent");
	LIST_INIT(&ex->ex_regions);
	ex->ex_name = name;
	ex->ex_start = start;
	ex->ex_end = end;
	ex->ex_flags = 0;
	if (fixed_extent)
		ex->ex_flags |= EXF_FIXED;
	if (flags & EX_NOCOALESCE)
		ex->ex_flags |= EXF_NOCOALESCE;
	return (ex);
}

/*
 * Destroy an extent map.
 * Since we're freeing the data, there can't be any references
 * so we don't need any locking.
 */
void
extent_destroy(struct extent *ex)
{
	struct extent_region *rp, *orp;

#ifdef DIAGNOSTIC
	/* Check arguments. */
	if (ex == NULL)
		panic("extent_destroy: NULL extent");
#endif

	/* Free all region descriptors in extent. */
	for (rp = LIST_FIRST(&ex->ex_regions); rp != NULL; ) {
		orp = rp;
		rp = LIST_NEXT(rp, er_link);
		LIST_REMOVE(orp, er_link);
		extent_free_region_descriptor(ex, orp);
	}

	cv_destroy(&ex->ex_cv);
	mutex_destroy(&ex->ex_lock);

	/* If we're not a fixed extent, free the extent descriptor itself. */
	if ((ex->ex_flags & EXF_FIXED) == 0)
		kmem_free(ex, sizeof(*ex));
}

/*
 * Insert a region descriptor into the sorted region list after the
 * entry "after" or at the head of the list (if "after" is NULL).
 * The region descriptor we insert is passed in "rp".  We must
 * allocate the region descriptor before calling this function!
 * If we don't need the region descriptor, it will be freed here.
 */
static void
extent_insert_and_optimize(struct extent *ex, u_long start, u_long size,
    int flags, struct extent_region *after, struct extent_region *rp)
{
	struct extent_region *nextr;
	int appended = 0;

	if (after == NULL) {
		/*
		 * We're the first in the region list.  If there's
		 * a region after us, attempt to coalesce to save
		 * descriptor overhead.
		 */
		if (((ex->ex_flags & EXF_NOCOALESCE) == 0) &&
		    (LIST_FIRST(&ex->ex_regions) != NULL) &&
		    ((start + size) == LIST_FIRST(&ex->ex_regions)->er_start)) {
			/*
			 * We can coalesce.  Prepend us to the first region.
			 */
			LIST_FIRST(&ex->ex_regions)->er_start = start;
			extent_free_region_descriptor(ex, rp);
			return;
		}

		/*
		 * Can't coalesce.  Fill in the region descriptor
		 * in, and insert us at the head of the region list.
		 */
		rp->er_start = start;
		rp->er_end = start + (size - 1);
		LIST_INSERT_HEAD(&ex->ex_regions, rp, er_link);
		return;
	}

	/*
	 * If EXF_NOCOALESCE is set, coalescing is disallowed.
	 */
	if (ex->ex_flags & EXF_NOCOALESCE)
		goto cant_coalesce;

	/*
	 * Attempt to coalesce with the region before us.
	 */
	if ((after->er_end + 1) == start) {
		/*
		 * We can coalesce.  Append ourselves and make
		 * note of it.
		 */
		after->er_end = start + (size - 1);
		appended = 1;
	}

	/*
	 * Attempt to coalesce with the region after us.
	 */
	if ((LIST_NEXT(after, er_link) != NULL) &&
	    ((start + size) == LIST_NEXT(after, er_link)->er_start)) {
		/*
		 * We can coalesce.  Note that if we appended ourselves
		 * to the previous region, we exactly fit the gap, and
		 * can free the "next" region descriptor.
		 */
		if (appended) {
			/*
			 * Yup, we can free it up.
			 */
			after->er_end = LIST_NEXT(after, er_link)->er_end;
			nextr = LIST_NEXT(after, er_link);
			LIST_REMOVE(nextr, er_link);
			extent_free_region_descriptor(ex, nextr);
		} else {
			/*
			 * Nope, just prepend us to the next region.
			 */
			LIST_NEXT(after, er_link)->er_start = start;
		}

		extent_free_region_descriptor(ex, rp);
		return;
	}

	/*
	 * We weren't able to coalesce with the next region, but
	 * we don't need to allocate a region descriptor if we
	 * appended ourselves to the previous region.
	 */
	if (appended) {
		extent_free_region_descriptor(ex, rp);
		return;
	}

 cant_coalesce:

	/*
	 * Fill in the region descriptor and insert ourselves
	 * into the region list.
	 */
	rp->er_start = start;
	rp->er_end = start + (size - 1);
	LIST_INSERT_AFTER(after, rp, er_link);
}

/*
 * Allocate a specific region in an extent map.
 */
int
extent_alloc_region(struct extent *ex, u_long start, u_long size, int flags)
{
	struct extent_region *rp, *last, *myrp;
	u_long end = start + (size - 1);
	int error;

#ifdef DIAGNOSTIC
	/* Check arguments. */
	if (ex == NULL)
		panic("extent_alloc_region: NULL extent");
	if (size < 1) {
		printf("extent_alloc_region: extent `%s', size 0x%lx\n",
		    ex->ex_name, size);
		panic("extent_alloc_region: bad size");
	}
	if (end < start) {
		printf(
		 "extent_alloc_region: extent `%s', start 0x%lx, size 0x%lx\n",
		 ex->ex_name, start, size);
		panic("extent_alloc_region: overflow");
	}
#endif
#ifdef LOCKDEBUG
	if (flags & EX_WAITSPACE) {
		ASSERT_SLEEPABLE();
	}
#endif

	/*
	 * Make sure the requested region lies within the
	 * extent.
	 *
	 * We don't lock to check the range, because those values
	 * are never modified, and if another thread deletes the
	 * extent, we're screwed anyway.
	 */
	if ((start < ex->ex_start) || (end > ex->ex_end)) {
#ifdef DIAGNOSTIC
		printf("extent_alloc_region: extent `%s' (0x%lx - 0x%lx)\n",
		    ex->ex_name, ex->ex_start, ex->ex_end);
		printf("extent_alloc_region: start 0x%lx, end 0x%lx\n",
		    start, end);
		panic("extent_alloc_region: region lies outside extent");
#else
		return (EINVAL);
#endif
	}

	/*
	 * Allocate the region descriptor.  It will be freed later
	 * if we can coalesce with another region.  Don't lock before
	 * here!  This could block.
	 */
	myrp = extent_alloc_region_descriptor(ex, flags);
	if (myrp == NULL) {
#ifdef DIAGNOSTIC
		printf(
		    "extent_alloc_region: can't allocate region descriptor\n");
#endif
		return (ENOMEM);
	}

	mutex_enter(&ex->ex_lock);
 alloc_start:

	/*
	 * Attempt to place ourselves in the desired area of the
	 * extent.  We save ourselves some work by keeping the list sorted.
	 * In other words, if the start of the current region is greater
	 * than the end of our region, we don't have to search any further.
	 */

	/*
	 * Keep a pointer to the last region we looked at so
	 * that we don't have to traverse the list again when
	 * we insert ourselves.  If "last" is NULL when we
	 * finally insert ourselves, we go at the head of the
	 * list.  See extent_insert_and_optimize() for details.
	 */
	last = NULL;

	LIST_FOREACH(rp, &ex->ex_regions, er_link) {
		if (rp->er_start > end) {
			/*
			 * We lie before this region and don't
			 * conflict.
			 */
			break;
		}

		/*
		 * The current region begins before we end.
		 * Check for a conflict.
		 */
		if (rp->er_end >= start) {
			/*
			 * We conflict.  If we can (and want to) wait,
			 * do so.
			 */
			if (flags & EX_WAITSPACE) {
				if ((flags & EX_CATCH) != 0)
					error = cv_wait_sig(&ex->ex_cv,
					    &ex->ex_lock);
				else {
					cv_wait(&ex->ex_cv, &ex->ex_lock);
					error = 0;
				}
				if (error == 0)
					goto alloc_start;
				mutex_exit(&ex->ex_lock);
			} else {
				mutex_exit(&ex->ex_lock);
				error = EAGAIN;
			}
			extent_free_region_descriptor(ex, myrp);
			return error;
		}
		/*
		 * We don't conflict, but this region lies before
		 * us.  Keep a pointer to this region, and keep
		 * trying.
		 */
		last = rp;
	}

	/*
	 * We don't conflict with any regions.  "last" points
	 * to the region we fall after, or is NULL if we belong
	 * at the beginning of the region list.  Insert ourselves.
	 */
	extent_insert_and_optimize(ex, start, size, flags, last, myrp);
	mutex_exit(&ex->ex_lock);
	return (0);
}

/*
 * Macro to check (x + y) <= z.  This check is designed to fail
 * if an overflow occurs.
 */
#define LE_OV(x, y, z)	((((x) + (y)) >= (x)) && (((x) + (y)) <= (z)))

/*
 * Allocate a region in an extent map subregion.
 *
 * If EX_FAST is specified, we return the first fit in the map.
 * Otherwise, we try to minimize fragmentation by finding the
 * smallest gap that will hold the request.
 *
 * The allocated region is aligned to "alignment", which must be
 * a power of 2.
 */
int
extent_alloc_subregion1(struct extent *ex, u_long substart, u_long subend,
    u_long size, u_long alignment, u_long skew, u_long boundary,
    int flags, u_long *result)
{
	struct extent_region *rp, *myrp, *last, *bestlast;
	u_long newstart, newend, exend, beststart, bestovh, ovh;
	u_long dontcross;
	int error;

#ifdef DIAGNOSTIC
	/*
	 * Check arguments.
	 *
	 * We don't lock to check these, because these values
	 * are never modified, and if another thread deletes the
	 * extent, we're screwed anyway.
	 */
	if (ex == NULL)
		panic("extent_alloc_subregion: NULL extent");
	if (result == NULL)
		panic("extent_alloc_subregion: NULL result pointer");
	if ((substart < ex->ex_start) || (substart > ex->ex_end) ||
	    (subend > ex->ex_end) || (subend < ex->ex_start)) {
  printf("extent_alloc_subregion: extent `%s', ex_start 0x%lx, ex_end 0x%lx\n",
		    ex->ex_name, ex->ex_start, ex->ex_end);
		printf("extent_alloc_subregion: substart 0x%lx, subend 0x%lx\n",
		    substart, subend);
		panic("extent_alloc_subregion: bad subregion");
	}
	if ((size < 1) || ((size - 1) > (subend - substart))) {
		printf("extent_alloc_subregion: extent `%s', size 0x%lx\n",
		    ex->ex_name, size);
		panic("extent_alloc_subregion: bad size");
	}
	if (alignment == 0)
		panic("extent_alloc_subregion: bad alignment");
	if (boundary && (boundary < size)) {
		printf(
		    "extent_alloc_subregion: extent `%s', size 0x%lx, "
		    "boundary 0x%lx\n", ex->ex_name, size, boundary);
		panic("extent_alloc_subregion: bad boundary");
	}
#endif
#ifdef LOCKDEBUG
	if (flags & EX_WAITSPACE) {
		ASSERT_SLEEPABLE();
	}
#endif

	/*
	 * Allocate the region descriptor.  It will be freed later
	 * if we can coalesce with another region.  Don't lock before
	 * here!  This could block.
	 */
	myrp = extent_alloc_region_descriptor(ex, flags);
	if (myrp == NULL) {
#ifdef DIAGNOSTIC
		printf(
		 "extent_alloc_subregion: can't allocate region descriptor\n");
#endif
		return (ENOMEM);
	}

 alloc_start:
	mutex_enter(&ex->ex_lock);

	/*
	 * Keep a pointer to the last region we looked at so
	 * that we don't have to traverse the list again when
	 * we insert ourselves.  If "last" is NULL when we
	 * finally insert ourselves, we go at the head of the
	 * list.  See extent_insert_and_optimize() for deatails.
	 */
	last = NULL;

	/*
	 * Keep track of size and location of the smallest
	 * chunk we fit in.
	 *
	 * Since the extent can be as large as the numeric range
	 * of the CPU (0 - 0xffffffff for 32-bit systems), the
	 * best overhead value can be the maximum unsigned integer.
	 * Thus, we initialize "bestovh" to 0, since we insert ourselves
	 * into the region list immediately on an exact match (which
	 * is the only case where "bestovh" would be set to 0).
	 */
	bestovh = 0;
	beststart = 0;
	bestlast = NULL;

	/*
	 * Keep track of end of free region.  This is either the end of extent
	 * or the start of a region past the subend.
	 */
	exend = ex->ex_end;

	/*
	 * For N allocated regions, we must make (N + 1)
	 * checks for unallocated space.  The first chunk we
	 * check is the area from the beginning of the subregion
	 * to the first allocated region after that point.
	 */
	newstart = EXTENT_ALIGN(substart, alignment, skew);
	if (newstart < ex->ex_start) {
#ifdef DIAGNOSTIC
		printf(
      "extent_alloc_subregion: extent `%s' (0x%lx - 0x%lx), alignment 0x%lx\n",
		 ex->ex_name, ex->ex_start, ex->ex_end, alignment);
		mutex_exit(&ex->ex_lock);
		panic("extent_alloc_subregion: overflow after alignment");
#else
		extent_free_region_descriptor(ex, myrp);
		mutex_exit(&ex->ex_lock);
		return (EINVAL);
#endif
	}

	/*
	 * Find the first allocated region that begins on or after
	 * the subregion start, advancing the "last" pointer along
	 * the way.
	 */
	LIST_FOREACH(rp, &ex->ex_regions, er_link) {
		if (rp->er_start >= newstart)
			break;
		last = rp;
	}

	/*
	 * Relocate the start of our candidate region to the end of
	 * the last allocated region (if there was one overlapping
	 * our subrange).
	 */
	if (last != NULL && last->er_end >= newstart)
		newstart = EXTENT_ALIGN((last->er_end + 1), alignment, skew);

	for (; rp != NULL; rp = LIST_NEXT(rp, er_link)) {
		/*
		 * If the region pasts the subend, bail out and see
		 * if we fit against the subend.
		 */
		if (rp->er_start > subend) {
			exend = rp->er_start;
			break;
		}

		/*
		 * Check the chunk before "rp".  Note that our
		 * comparison is safe from overflow conditions.
		 */
		if (LE_OV(newstart, size, rp->er_start)) {
			/*
			 * Do a boundary check, if necessary.  Note
			 * that a region may *begin* on the boundary,
			 * but it must end before the boundary.
			 */
			if (boundary) {
				newend = newstart + (size - 1);

				/*
				 * Calculate the next boundary after the start
				 * of this region.
				 */
				dontcross = EXTENT_ALIGN(newstart+1, boundary,
				    (flags & EX_BOUNDZERO) ? 0 : ex->ex_start)
				    - 1;

#if 0
				printf("newstart=%lx newend=%lx ex_start=%lx ex_end=%lx boundary=%lx dontcross=%lx\n",
				    newstart, newend, ex->ex_start, ex->ex_end,
				    boundary, dontcross);
#endif

				/* Check for overflow */
				if (dontcross < ex->ex_start)
					dontcross = ex->ex_end;
				else if (newend > dontcross) {
					/*
					 * Candidate region crosses boundary.
					 * Throw away the leading part and see
					 * if we still fit.
					 */
					newstart = dontcross + 1;
					newend = newstart + (size - 1);
					dontcross += boundary;
					if (!LE_OV(newstart, size, rp->er_start))
						goto skip;
				}

				/*
				 * If we run past the end of
				 * the extent or the boundary
				 * overflows, then the request
				 * can't fit.
				 */
				if (newstart + size - 1 > ex->ex_end ||
				    dontcross < newstart)
					goto fail;
			}

			/*
			 * We would fit into this space.  Calculate
			 * the overhead (wasted space).  If we exactly
			 * fit, or we're taking the first fit, insert
			 * ourselves into the region list.
			 */
			ovh = rp->er_start - newstart - size;
			if ((flags & EX_FAST) || (ovh == 0))
				goto found;

			/*
			 * Don't exactly fit, but check to see
			 * if we're better than any current choice.
			 */
			if ((bestovh == 0) || (ovh < bestovh)) {
				bestovh = ovh;
				beststart = newstart;
				bestlast = last;
			}
		}

skip:
		/*
		 * Skip past the current region and check again.
		 */
		newstart = EXTENT_ALIGN((rp->er_end + 1), alignment, skew);
		if (newstart < rp->er_end) {
			/*
			 * Overflow condition.  Don't error out, since
			 * we might have a chunk of space that we can
			 * use.
			 */
			goto fail;
		}

		last = rp;
	}

	/*
	 * The final check is from the current starting point to the
	 * end of the subregion.  If there were no allocated regions,
	 * "newstart" is set to the beginning of the subregion, or
	 * just past the end of the last allocated region, adjusted
	 * for alignment in either case.
	 */
	if (LE_OV(newstart, (size - 1), subend)) {
		/*
		 * Do a boundary check, if necessary.  Note
		 * that a region may *begin* on the boundary,
		 * but it must end before the boundary.
		 */
		if (boundary) {
			newend = newstart + (size - 1);

			/*
			 * Calculate the next boundary after the start
			 * of this region.
			 */
			dontcross = EXTENT_ALIGN(newstart+1, boundary,
			    (flags & EX_BOUNDZERO) ? 0 : ex->ex_start)
			    - 1;

#if 0
			printf("newstart=%lx newend=%lx ex_start=%lx ex_end=%lx boundary=%lx dontcross=%lx\n",
			    newstart, newend, ex->ex_start, ex->ex_end,
			    boundary, dontcross);
#endif

			/* Check for overflow */
			if (dontcross < ex->ex_start)
				dontcross = ex->ex_end;
			else if (newend > dontcross) {
				/*
				 * Candidate region crosses boundary.
				 * Throw away the leading part and see
				 * if we still fit.
				 */
				newstart = dontcross + 1;
				newend = newstart + (size - 1);
				dontcross += boundary;
				if (!LE_OV(newstart, (size - 1), subend))
					goto fail;
			}

			/*
			 * If we run past the end of
			 * the extent or the boundary
			 * overflows, then the request
			 * can't fit.
			 */
			if (newstart + size - 1 > ex->ex_end ||
			    dontcross < newstart)
				goto fail;
		}

		/*
		 * We would fit into this space.  Calculate
		 * the overhead (wasted space).  If we exactly
		 * fit, or we're taking the first fit, insert
		 * ourselves into the region list.
		 */
		ovh = exend - newstart - (size - 1);
		if ((flags & EX_FAST) || (ovh == 0))
			goto found;

		/*
		 * Don't exactly fit, but check to see
		 * if we're better than any current choice.
		 */
		if ((bestovh == 0) || (ovh < bestovh)) {
			bestovh = ovh;
			beststart = newstart;
			bestlast = last;
		}
	}

 fail:
	/*
	 * One of the following two conditions have
	 * occurred:
	 *
	 *	There is no chunk large enough to hold the request.
	 *
	 *	If EX_FAST was not specified, there is not an
	 *	exact match for the request.
	 *
	 * Note that if we reach this point and EX_FAST is
	 * set, then we know there is no space in the extent for
	 * the request.
	 */
	if (((flags & EX_FAST) == 0) && (bestovh != 0)) {
		/*
		 * We have a match that's "good enough".
		 */
		newstart = beststart;
		last = bestlast;
		goto found;
	}

	/*
	 * No space currently available.  Wait for it to free up,
	 * if possible.
	 */
	if (flags & EX_WAITSPACE) {
		if ((flags & EX_CATCH) != 0) {
			error = cv_wait_sig(&ex->ex_cv, &ex->ex_lock);
		} else {
			cv_wait(&ex->ex_cv, &ex->ex_lock);
			error = 0;
		}
		if (error == 0)
			goto alloc_start;
		mutex_exit(&ex->ex_lock);
	} else {
		mutex_exit(&ex->ex_lock);
		error = EAGAIN;
	}

	extent_free_region_descriptor(ex, myrp);
	return error;

 found:
	/*
	 * Insert ourselves into the region list.
	 */
	extent_insert_and_optimize(ex, newstart, size, flags, last, myrp);
	mutex_exit(&ex->ex_lock);
	*result = newstart;
	return (0);
}

int
extent_alloc_subregion(struct extent *ex, u_long start, u_long end, u_long size,
    u_long alignment, u_long boundary, int flags, u_long *result)
{

	return (extent_alloc_subregion1(ex, start, end, size, alignment,
					0, boundary, flags, result));
}

int
extent_alloc(struct extent *ex, u_long size, u_long alignment, u_long boundary,
    int flags, u_long *result)
{

	return (extent_alloc_subregion1(ex, ex->ex_start, ex->ex_end,
					size, alignment, 0, boundary,
					flags, result));
}

int
extent_alloc1(struct extent *ex, u_long size, u_long alignment, u_long skew,
    u_long boundary, int flags, u_long *result)
{

	return (extent_alloc_subregion1(ex, ex->ex_start, ex->ex_end,
					size, alignment, skew, boundary,
					flags, result));
}

int
extent_free(struct extent *ex, u_long start, u_long size, int flags)
{
	struct extent_region *rp, *nrp = NULL;
	u_long end = start + (size - 1);
	int coalesce;

#ifdef DIAGNOSTIC
	/*
	 * Check arguments.
	 *
	 * We don't lock to check these, because these values
	 * are never modified, and if another thread deletes the
	 * extent, we're screwed anyway.
	 */
	if (ex == NULL)
		panic("extent_free: NULL extent");
	if ((start < ex->ex_start) || (end > ex->ex_end)) {
		extent_print(ex);
		printf("extent_free: extent `%s', start 0x%lx, size 0x%lx\n",
		    ex->ex_name, start, size);
		panic("extent_free: extent `%s', region not within extent",
		    ex->ex_name);
	}
	/* Check for an overflow. */
	if (end < start) {
		extent_print(ex);
		printf("extent_free: extent `%s', start 0x%lx, size 0x%lx\n",
		    ex->ex_name, start, size);
		panic("extent_free: overflow");
	}
#endif

	/*
	 * If we're allowing coalescing, we must allocate a region
	 * descriptor now, since it might block.
	 *
	 * XXX Make a static, create-time flags word, so we don't
	 * XXX have to lock to read it!
	 */
	mutex_enter(&ex->ex_lock);
	coalesce = (ex->ex_flags & EXF_NOCOALESCE) == 0;
	mutex_exit(&ex->ex_lock);

	if (coalesce) {
		/* Allocate a region descriptor. */
		nrp = extent_alloc_region_descriptor(ex, flags);
		if (nrp == NULL)
			return (ENOMEM);
	}

	mutex_enter(&ex->ex_lock);

	/*
	 * Find region and deallocate.  Several possibilities:
	 *
	 *	1. (start == er_start) && (end == er_end):
	 *	   Free descriptor.
	 *
	 *	2. (start == er_start) && (end < er_end):
	 *	   Adjust er_start.
	 *
	 *	3. (start > er_start) && (end == er_end):
	 *	   Adjust er_end.
	 *
	 *	4. (start > er_start) && (end < er_end):
	 *	   Fragment region.  Requires descriptor alloc.
	 *
	 * Cases 2, 3, and 4 require that the EXF_NOCOALESCE flag
	 * is not set.
	 */
	LIST_FOREACH(rp, &ex->ex_regions, er_link) {
		/*
		 * Save ourselves some comparisons; does the current
		 * region end before chunk to be freed begins?  If so,
		 * then we haven't found the appropriate region descriptor.
		 */
		if (rp->er_end < start)
			continue;

		/*
		 * Save ourselves some traversal; does the current
		 * region begin after the chunk to be freed ends?  If so,
		 * then we've already passed any possible region descriptors
		 * that might have contained the chunk to be freed.
		 */
		if (rp->er_start > end)
			break;

		/* Case 1. */
		if ((start == rp->er_start) && (end == rp->er_end)) {
			LIST_REMOVE(rp, er_link);
			extent_free_region_descriptor(ex, rp);
			goto done;
		}

		/*
		 * The following cases all require that EXF_NOCOALESCE
		 * is not set.
		 */
		if (!coalesce)
			continue;

		/* Case 2. */
		if ((start == rp->er_start) && (end < rp->er_end)) {
			rp->er_start = (end + 1);
			goto done;
		}

		/* Case 3. */
		if ((start > rp->er_start) && (end == rp->er_end)) {
			rp->er_end = (start - 1);
			goto done;
		}

		/* Case 4. */
		if ((start > rp->er_start) && (end < rp->er_end)) {
			/* Fill in new descriptor. */
			nrp->er_start = end + 1;
			nrp->er_end = rp->er_end;

			/* Adjust current descriptor. */
			rp->er_end = start - 1;

			/* Insert new descriptor after current. */
			LIST_INSERT_AFTER(rp, nrp, er_link);

			/* We used the new descriptor, so don't free it below */
			nrp = NULL;
			goto done;
		}
	}

	/* Region not found, or request otherwise invalid. */
	mutex_exit(&ex->ex_lock);
	extent_print(ex);
	printf("extent_free: start 0x%lx, end 0x%lx\n", start, end);
	panic("extent_free: region not found");

 done:
	if (nrp != NULL)
		extent_free_region_descriptor(ex, nrp);
	cv_broadcast(&ex->ex_cv);
	mutex_exit(&ex->ex_lock);
	return (0);
}

void
extent_print(struct extent *ex)
{
	struct extent_region *rp;

	if (ex == NULL)
		panic("extent_print: NULL extent");

	mutex_enter(&ex->ex_lock);

	printf("extent `%s' (0x%lx - 0x%lx), flags = 0x%x\n", ex->ex_name,
	    ex->ex_start, ex->ex_end, ex->ex_flags);

	LIST_FOREACH(rp, &ex->ex_regions, er_link)
		printf("     0x%lx - 0x%lx\n", rp->er_start, rp->er_end);

	mutex_exit(&ex->ex_lock);
}

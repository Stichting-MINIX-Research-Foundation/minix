/*	$NetBSD: xmalloc.c,v 1.11 2011/05/25 14:41:46 christos Exp $	*/

/*
 * Copyright 1996 John D. Polstra.
 * Copyright 1996 Matt Thomas <matt@3am-software.com>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by John Polstra.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

/*
 * Copyright (c) 1983 Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef __minix
/* Minix mmap can do this. */
#define mmap minix_mmap
#define munmap minix_munmap
#endif

#if defined(LIBC_SCCS) && !defined(lint)
/*static char *sccsid = "from: @(#)malloc.c	5.11 (Berkeley) 2/23/91";*/
#endif /* LIBC_SCCS and not lint */

/*
 * malloc.c (Caltech) 2/21/82
 * Chris Kingsley, kingsley@cit-20.
 *
 * This is a very fast storage allocator.  It allocates blocks of a small
 * number of different sizes, and keeps free lists of each size.  Blocks that
 * don't exactly fit are passed up to the next larger size.  In this
 * implementation, the available sizes are 2^n-4 (or 2^n-10) bytes long.
 * This is designed for use in a virtual memory environment.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: xmalloc.c,v 1.11 2011/05/25 14:41:46 christos Exp $");
#endif /* not lint */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "rtld.h"

/*
 * Pre-allocate mmap'ed pages
 */
#define	NPOOLPAGES	(32*1024/pagesz)
static char 		*pagepool_start, *pagepool_end;
static int		morepages(int);
#define PAGEPOOL_SIZE	(size_t)(pagepool_end - pagepool_start)

/*
 * The overhead on a block is at least 4 bytes.  When free, this space
 * contains a pointer to the next free block, and the bottom two bits must
 * be zero.  When in use, the first byte is set to MAGIC, and the second
 * byte is the size index.  The remaining bytes are for alignment.
 * If range checking is enabled then a second word holds the size of the
 * requested block, less 1, rounded up to a multiple of sizeof(RMAGIC).
 * The order of elements is critical: ov_magic must overlay the low order
 * bits of ov_next, and ov_magic can not be a valid ov_next bit pattern.
 */
union	overhead {
	union	overhead *ov_next;	/* when free */
	struct {
		u_char	ovu_magic;	/* magic number */
		u_char	ovu_index;	/* bucket # */
#ifdef RCHECK
		u_short	ovu_rmagic;	/* range magic number */
		u_int	ovu_size;	/* actual block size */
#endif
	} ovu;
#define	ov_magic	ovu.ovu_magic
#define	ov_index	ovu.ovu_index
#define	ov_rmagic	ovu.ovu_rmagic
#define	ov_size		ovu.ovu_size
};

static void morecore(size_t);
static void *imalloc(size_t);

#define	MAGIC		0xef		/* magic # on accounting info */
#define RMAGIC		0x5555		/* magic # on range info */

#ifdef RCHECK
#define	RSLOP		(sizeof (u_short))
#else
#define	RSLOP		0
#endif

/*
 * nextf[i] is the pointer to the next free block of size 2^(i+3).  The
 * smallest allocatable block is 8 bytes.  The overhead information
 * precedes the data area returned to the user.
 */
#define	NBUCKETS 30
static	union overhead *nextf[NBUCKETS];

static	size_t pagesz;			/* page size */
static	size_t pagebucket;		/* page size bucket */
static	size_t pageshift;		/* page size shift */

#ifdef MSTATS
/*
 * nmalloc[i] is the difference between the number of mallocs and frees
 * for a given block size.
 */
static	u_int nmalloc[NBUCKETS];
#endif

#if defined(MALLOC_DEBUG) || defined(RCHECK)
#define	ASSERT(p)   if (!(p)) botch("p")
static void
botch(const char *s)
{
    xwarnx("\r\nassertion botched: %s\r\n", s);
    abort();
}
#else
#define	ASSERT(p)
#endif

#define TRACE()	xprintf("TRACE %s:%d\n", __FILE__, __LINE__)

static void *
imalloc(size_t nbytes)
{
  	union overhead *op;
  	size_t bucket;
	size_t n, m;
	unsigned amt;

	/*
	 * First time malloc is called, setup page size and
	 * align break pointer so all data will be page aligned.
	 */
	if (pagesz == 0) {
		pagesz = n = _rtld_pagesz;
		if (morepages(NPOOLPAGES) == 0)
			return NULL;
		op = (union overhead *)(pagepool_start);
		m = sizeof (*op) - (((char *)op - (char *)NULL) & (n - 1));
		if (n < m)
			n += pagesz - m;
		else
			n -= m;
  		if (n) {
			pagepool_start += n;
		}
		bucket = 0;
		amt = sizeof(union overhead);
		while (pagesz > amt) {
			amt <<= 1;
			bucket++;
		}
		pagebucket = bucket;
		pageshift = ffs(pagesz) - 1;
	}
	/*
	 * Convert amount of memory requested into closest block size
	 * stored in hash buckets which satisfies request.
	 * Account for space used per block for accounting.
	 */
	if (nbytes <= (n = pagesz - sizeof (*op) - RSLOP)) {
		if (sizeof(union overhead) & (sizeof(union overhead) - 1)) {
		    amt = sizeof(union overhead) * 2;
		    bucket = 1;
		} else {
		    amt = sizeof(union overhead); /* size of first bucket */
		    bucket = 0;
		}
		n = -(sizeof (*op) + RSLOP);
	} else {
		amt = pagesz;
		bucket = pagebucket;
	}
	while (nbytes > amt + n) {
		amt <<= 1;
		if (amt == 0)
			return (NULL);
		bucket++;
	}
	/*
	 * If nothing in hash bucket right now,
	 * request more memory from the system.
	 */
  	if ((op = nextf[bucket]) == NULL) {
  		morecore(bucket);
  		if ((op = nextf[bucket]) == NULL)
  			return (NULL);
	}
	/* remove from linked list */
  	nextf[bucket] = op->ov_next;
	op->ov_magic = MAGIC;
	op->ov_index = bucket;
#ifdef MSTATS
  	nmalloc[bucket]++;
#endif
#ifdef RCHECK
	/*
	 * Record allocated size of block and
	 * bound space with magic numbers.
	 */
	op->ov_size = (nbytes + RSLOP - 1) & ~(RSLOP - 1);
	op->ov_rmagic = RMAGIC;
  	*(u_short *)((caddr_t)(op + 1) + op->ov_size) = RMAGIC;
#endif
  	return ((char *)(op + 1));
}

/*
 * Allocate more memory to the indicated bucket.
 */
static void
morecore(size_t bucket)
{
  	union overhead *op;
	size_t sz;		/* size of desired block */
  	size_t amt;		/* amount to allocate */
  	size_t nblks;		/* how many blocks we get */

	/*
	 * sbrk_size <= 0 only for big, FLUFFY, requests (about
	 * 2^30 bytes on a VAX, I think) or for a negative arg.
	 */
	sz = 1 << (bucket + 3);
#ifdef MALLOC_DEBUG
	ASSERT(sz > 0);
#endif
	if (sz < pagesz) {
		amt = pagesz;
		nblks = amt >> (bucket + 3);
	} else {
		amt = sz + pagesz;
		nblks = 1;
	}
	if (amt > PAGEPOOL_SIZE)
		if (morepages((amt >> pageshift) + NPOOLPAGES) == 0)
			return;
	op = (union overhead *)pagepool_start;
	pagepool_start += amt;

	/*
	 * Add new memory allocated to that on
	 * free list for this hash bucket.
	 */
  	nextf[bucket] = op;
  	while (--nblks > 0) {
		op->ov_next = (union overhead *)((caddr_t)op + sz);
		op = (union overhead *)((caddr_t)op + sz);
  	}
}

void
xfree(void *cp)
{
  	int size;
	union overhead *op;

  	if (cp == NULL)
  		return;
	op = (union overhead *)((caddr_t)cp - sizeof (union overhead));
#ifdef MALLOC_DEBUG
  	ASSERT(op->ov_magic == MAGIC);		/* make sure it was in use */
#else
	if (op->ov_magic != MAGIC)
		return;				/* sanity */
#endif
#ifdef RCHECK
  	ASSERT(op->ov_rmagic == RMAGIC);
	ASSERT(*(u_short *)((caddr_t)(op + 1) + op->ov_size) == RMAGIC);
#endif
  	size = op->ov_index;
  	ASSERT(size < NBUCKETS);
	op->ov_next = nextf[size];	/* also clobbers ov_magic */
  	nextf[size] = op;
#ifdef MSTATS
  	nmalloc[size]--;
#endif
}

static void *
irealloc(void *cp, size_t nbytes)
{
  	size_t onb;
	size_t i;
	union overhead *op;
  	char *res;

  	if (cp == NULL)
  		return (imalloc(nbytes));
	op = (union overhead *)((caddr_t)cp - sizeof (union overhead));
	if (op->ov_magic != MAGIC) {
		static const char *err_str =
		    "memory corruption or double free in realloc\n";
		extern char *__progname;
	        write(STDERR_FILENO, __progname, strlen(__progname));
		write(STDERR_FILENO, err_str, strlen(err_str));
		abort();
	}

	i = op->ov_index;
	onb = 1 << (i + 3);
	if (onb < pagesz)
		onb -= sizeof (*op) + RSLOP;
	else
		onb += pagesz - sizeof (*op) - RSLOP;
	/* avoid the copy if same size block */
	if (i) {
		i = 1 << (i + 2);
		if (i < pagesz)
			i -= sizeof (*op) + RSLOP;
		else
			i += pagesz - sizeof (*op) - RSLOP;
	}
	if (nbytes <= onb && nbytes > i) {
#ifdef RCHECK
		op->ov_size = (nbytes + RSLOP - 1) & ~(RSLOP - 1);
		*(u_short *)((caddr_t)(op + 1) + op->ov_size) = RMAGIC;
#endif
		return(cp);
	} else
		xfree(cp);
  	if ((res = imalloc(nbytes)) == NULL)
  		return (NULL);
  	if (cp != res)		/* common optimization if "compacting" */
		memcpy(res, cp, (nbytes < onb) ? nbytes : onb);
  	return (res);
}

#ifdef MSTATS
/*
 * mstats - print out statistics about malloc
 *
 * Prints two lines of numbers, one showing the length of the free list
 * for each size category, the second showing the number of mallocs -
 * frees for each size category.
 */
void
mstats(char *s)
{
  	int i, j;
  	union overhead *p;
  	int totfree = 0,
  	totused = 0;

  	xprintf("Memory allocation statistics %s\nfree:\t", s);
  	for (i = 0; i < NBUCKETS; i++) {
  		for (j = 0, p = nextf[i]; p; p = p->ov_next, j++)
  			;
  		xprintf(" %d", j);
  		totfree += j * (1 << (i + 3));
  	}
  	xprintf("\nused:\t");
  	for (i = 0; i < NBUCKETS; i++) {
  		xprintf(" %d", nmalloc[i]);
  		totused += nmalloc[i] * (1 << (i + 3));
  	}
  	xprintf("\n\tTotal in use: %d, total free: %d\n",
	    totused, totfree);
}
#endif


static int
morepages(int n)
{
	int	fd = -1;
	int	offset;

#ifdef NEED_DEV_ZERO
	fd = open("/dev/zero", O_RDWR, 0);
	if (fd == -1)
		xerr(1, "/dev/zero");
#endif

	if (PAGEPOOL_SIZE > pagesz) {
		caddr_t	addr = (caddr_t)
			(((long)pagepool_start + pagesz - 1) & ~(pagesz - 1));
		if (munmap(addr, pagepool_end - addr) != 0)
			xwarn("morepages: munmap %p", addr);
	}

	offset = (long)pagepool_start - ((long)pagepool_start & ~(pagesz - 1));

	if ((pagepool_start = mmap(0, n * pagesz,
			PROT_READ|PROT_WRITE,
			MAP_ANON|MAP_PRIVATE, fd, 0)) == (caddr_t)-1) {
		xprintf("Cannot map anonymous memory");
		return 0;
	}
	pagepool_end = pagepool_start + n * pagesz;
	pagepool_start += offset;

#ifdef NEED_DEV_ZERO
	close(fd);
#endif
	return n;
}

void *
xcalloc(size_t size)
{

	return memset(xmalloc(size), 0, size);
}

void *
xmalloc(size_t size)
{
	void *p = imalloc(size);

	if (p == NULL)
		xerr(1, "%s", xstrerror(errno));
	return p;
}

void *
xrealloc(void *p, size_t size)
{
	p = irealloc(p, size);

	if (p == NULL)
		xerr(1, "%s", xstrerror(errno));
	return p;
}

char *
xstrdup(const char *str)
{
	size_t len;
	char *copy;

	len = strlen(str) + 1;
	copy = xmalloc(len);
	memcpy(copy, str, len);
	return (copy);
}

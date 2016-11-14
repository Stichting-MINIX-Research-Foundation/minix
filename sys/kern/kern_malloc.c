/*	$NetBSD: kern_malloc.c,v 1.145 2015/02/06 18:21:29 maxv Exp $	*/

/*
 * Copyright (c) 1987, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *
 *	@(#)kern_malloc.c	8.4 (Berkeley) 5/20/95
 */

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)kern_malloc.c	8.4 (Berkeley) 5/20/95
 */

/*
 * Wrapper interface for obsolete malloc(9).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_malloc.c,v 1.145 2015/02/06 18:21:29 maxv Exp $");

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/kmem.h>

/*
 * Built-in malloc types.  Note: ought to be removed.
 */
MALLOC_DEFINE(M_DEVBUF, "devbuf", "device driver memory");
MALLOC_DEFINE(M_DMAMAP, "DMA map", "bus_dma(9) structures");
MALLOC_DEFINE(M_FREE, "free", "should be on free list");
MALLOC_DEFINE(M_TEMP, "temp", "misc. temporary data buffers");
MALLOC_DEFINE(M_RTABLE, "routetbl", "routing tables");
MALLOC_DEFINE(M_FTABLE, "fragtbl", "fragment reassembly header");
MALLOC_DEFINE(M_UFSMNT, "UFS mount", "UFS mount structure");
MALLOC_DEFINE(M_NETADDR, "Export Host", "Export host address structure");
MALLOC_DEFINE(M_MRTABLE, "mrt", "multicast routing tables");

/*
 * Header contains total size, including the header itself.
 */
struct malloc_header {
	size_t		mh_size;
} __aligned(ALIGNBYTES + 1);

void *
kern_malloc(unsigned long size, int flags)
{
	const int kmflags = (flags & M_NOWAIT) ? KM_NOSLEEP : KM_SLEEP;
	size_t allocsize, hdroffset;
	struct malloc_header *mh;
	void *p;

	if (size >= PAGE_SIZE) {
		allocsize = PAGE_SIZE + size; /* for page alignment */
		hdroffset = PAGE_SIZE - sizeof(struct malloc_header);
	} else {
		allocsize = sizeof(struct malloc_header) + size;
		hdroffset = 0;
	}

	p = kmem_intr_alloc(allocsize, kmflags);
	if (p == NULL)
		return NULL;

	if ((flags & M_ZERO) != 0) {
		memset(p, 0, allocsize);
	}
	mh = (void *)((char *)p + hdroffset);
	mh->mh_size = allocsize - hdroffset;

	return mh + 1;
}

void
kern_free(void *addr)
{
	struct malloc_header *mh;

	mh = addr;
	mh--;

	if (mh->mh_size >= PAGE_SIZE + sizeof(struct malloc_header))
		kmem_intr_free((char *)addr - PAGE_SIZE,
		    mh->mh_size + PAGE_SIZE - sizeof(struct malloc_header));
	else
		kmem_intr_free(mh, mh->mh_size);
}

void *
kern_realloc(void *curaddr, unsigned long newsize, int flags)
{
	struct malloc_header *mh;
	unsigned long cursize;
	void *newaddr;

	/*
	 * realloc() with a NULL pointer is the same as malloc().
	 */
	if (curaddr == NULL)
		return malloc(newsize, ksp, flags);

	/*
	 * realloc() with zero size is the same as free().
	 */
	if (newsize == 0) {
		free(curaddr, ksp);
		return NULL;
	}

	if ((flags & M_NOWAIT) == 0) {
		ASSERT_SLEEPABLE();
	}

	mh = curaddr;
	mh--;

	cursize = mh->mh_size - sizeof(struct malloc_header);

	/*
	 * If we already actually have as much as they want, we're done.
	 */
	if (newsize <= cursize)
		return curaddr;

	/*
	 * Can't satisfy the allocation with the existing block.
	 * Allocate a new one and copy the data.
	 */
	newaddr = malloc(newsize, ksp, flags);
	if (__predict_false(newaddr == NULL)) {
		/*
		 * malloc() failed, because flags included M_NOWAIT.
		 * Return NULL to indicate that failure.  The old
		 * pointer is still valid.
		 */
		return NULL;
	}
	memcpy(newaddr, curaddr, cursize);

	/*
	 * We were successful: free the old allocation and return
	 * the new one.
	 */
	free(curaddr, ksp);
	return newaddr;
}

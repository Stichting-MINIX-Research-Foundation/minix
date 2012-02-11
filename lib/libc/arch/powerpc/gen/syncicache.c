/*	$NetBSD: syncicache.c,v 1.15 2008/03/18 20:11:43 he Exp $	*/

/*
 * Copyright (C) 1995-1997, 1999 Wolfgang Solfrank.
 * Copyright (C) 1995-1997, 1999 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#if	defined(_KERNEL)
#include <sys/time.h>
#include <sys/proc.h>
#include <uvm/uvm_extern.h>
#endif
#if	!defined(_STANDALONE)
#include <sys/sysctl.h>
#endif

#include <machine/cpu.h>


#if defined(_STANDALONE)
#ifndef	CACHELINESIZE
#error "Must know the size of a cache line"
#endif
static struct cache_info _cache_info = {
	CACHELINESIZE,
	CACHELINESIZE,
	CACHELINESIZE,
	CACHELINESIZE
};
#define CACHEINFO	_cache_info
#elif defined(_KERNEL)
#define	CACHEINFO	(curcpu()->ci_ci)
#else
#include <stdlib.h>

size_t __getcachelinesize (void);

static int _cachelinesize = 0;

static struct cache_info _cache_info;
#define CACHEINFO	_cache_info

size_t
__getcachelinesize(void)
{
	static int cachemib[] = { CTL_MACHDEP, CPU_CACHELINE };
	static int cacheinfomib[] = { CTL_MACHDEP, CPU_CACHEINFO };
	size_t clen = sizeof(_cache_info);

	if (_cachelinesize)
		return _cachelinesize;

	if (sysctl(cacheinfomib, sizeof(cacheinfomib) / sizeof(cacheinfomib[0]),
		&_cache_info, &clen, NULL, 0) == 0) {
		_cachelinesize = _cache_info.dcache_line_size;
		return _cachelinesize;
	}

	/* Try older deprecated sysctl */
	clen = sizeof(_cachelinesize);
	if (sysctl(cachemib, sizeof(cachemib) / sizeof(cachemib[0]),
		   &_cachelinesize, &clen, NULL, 0) < 0
	    || !_cachelinesize)
		abort();

	_cache_info.dcache_size = _cachelinesize;
	_cache_info.dcache_line_size = _cachelinesize;
	_cache_info.icache_size = _cachelinesize;
	_cache_info.icache_line_size = _cachelinesize;

	/* If there is no cache, indicate we have issued the sysctl. */
	if (!_cachelinesize)
		_cachelinesize = 1;

	return _cachelinesize;
}
#endif

void
__syncicache(void *from, size_t len)
{
	size_t l, off;
	size_t linesz;
	char *p;

#if	!defined(_KERNEL) && !defined(_STANDALONE)
	if (!_cachelinesize)
		__getcachelinesize();
#endif	

	if (CACHEINFO.dcache_size > 0) {
		linesz = CACHEINFO.dcache_line_size;
		off = (uintptr_t)from & (linesz - 1);
		l = (len + off + linesz - 1) & ~(linesz - 1);
		p = (char *)from - off;
		do {
			__asm volatile ("dcbst 0,%0" :: "r"(p));
			p += linesz;
		} while ((l -= linesz) != 0);
	}
	__asm volatile ("sync");

	if (CACHEINFO.icache_size > 0 ) {
		linesz = CACHEINFO.icache_line_size;
		off = (uintptr_t)from & (linesz - 1);
		l = (len + off + linesz - 1) & ~(linesz - 1);
		p = (char *)from - off;
		do {
			__asm volatile ("icbi 0,%0" :: "r"(p));
			p += linesz;
		} while ((l -= linesz) != 0);
	}
	__asm volatile ("sync; isync");
}

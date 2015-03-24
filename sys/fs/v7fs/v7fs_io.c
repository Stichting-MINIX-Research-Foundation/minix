/*	$NetBSD: v7fs_io.c,v 1.3 2013/06/28 14:49:14 christos Exp $	*/

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: v7fs_io.c,v 1.3 2013/06/28 14:49:14 christos Exp $");
#if defined _KERNEL_OPT
#include "opt_v7fs.h"
#endif

#ifdef _KERNEL
#include <sys/param.h>
#else
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#endif

#include "v7fs.h"
#include "v7fs_impl.h"

#if defined _KERNEL
#define	STATIC_BUFFER
#endif

#ifdef V7FS_IO_DEBUG
#define	DPRINTF(fmt, args...)	printf("%s: " fmt, __func__, ##args)
#else
#define	DPRINTF(fmt, args...)	((void)0)
#endif

void *
scratch_read(struct v7fs_self *fs, daddr_t blk)
{
#ifdef STATIC_BUFFER
	int i;
	MEM_LOCK(fs);
	for (i = 0; i < V7FS_SELF_NSCRATCH; i++) {
		if (fs->scratch_free & (1 << i)) {
			fs->scratch_free &= ~(1 << i);
			break;
		}
	}
	if (i == V7FS_SELF_NSCRATCH) {
		DPRINTF("No scratch area. increase V7FS_SELF_NSCRATCH\n");
		assert(0);
		MEM_UNLOCK(fs);
		return NULL;
	}

	if (!fs->io.read(fs->io.cookie, fs->scratch[i], blk)) {
		DPRINTF("*** I/O error block %ld\n", (long)blk);
		fs->scratch_free |= (1 << i);
		MEM_UNLOCK(fs);
		return NULL;
	}
	MEM_UNLOCK(fs);
	/* Statistic */
	int n;
	if ((n = scratch_remain(fs)) < fs->scratch_remain)
		fs->scratch_remain = n;

	return fs->scratch[i];
#else
	uint8_t *buf = malloc(V7FS_BSIZE);
	if (!fs->io.read(fs->io.cookie, buf, blk)) {
		DPRINTF("*** I/O error block %ld\n",(long)blk);
		free(buf);
		return NULL;
	}
	return buf;
#endif
}

int
scratch_remain(const struct v7fs_self *fs)
{
#ifdef STATIC_BUFFER
	int nfree;
	int i;
	MEM_LOCK(fs);
	for (i = 0, nfree = 0; i < V7FS_SELF_NSCRATCH; i++) {
		if (fs->scratch_free & (1 << i)) {
			nfree++;
		}
	}
	MEM_UNLOCK(fs);
	return nfree;
#else
	return -1;
#endif
}

void
scratch_free(struct v7fs_self *fs __unused, void *p)
{
#ifdef STATIC_BUFFER
	int i;
	MEM_LOCK(fs);
	for (i = 0; i < V7FS_SELF_NSCRATCH; i++)
		if (fs->scratch[i] == p) {
			fs->scratch_free |= (1 << i);
			break;
		}
	MEM_UNLOCK(fs);
	assert(i != V7FS_SELF_NSCRATCH);
#else
	free(p);
#endif
}

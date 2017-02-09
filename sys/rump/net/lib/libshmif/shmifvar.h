/*	$NetBSD: shmifvar.h,v 1.10 2014/09/17 04:20:58 ozaki-r Exp $	*/

/*-
 * Copyright (c) 2009, 2010 Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by The Nokia Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _RUMP_NET_SHMIFVAR_H_
#define _RUMP_NET_SHMIFVAR_H_

#define SHMIF_MAGIC 0xca11d054
#define SHMIF_VERSION 3

struct shmif_mem {
	uint32_t shm_magic;
	uint32_t shm_version;

	uint64_t shm_gen;

	uint32_t shm_first;
	uint32_t shm_last;

	uint32_t shm_lock;
	uint32_t shm_spare[1];

	uint8_t shm_data[0];
};

#define IFMEM_DATA	(offsetof(struct shmif_mem, shm_data))
#define IFMEM_WAKEUP	(offsetof(struct shmif_mem, shm_version))

struct shmif_pkthdr {
	uint32_t sp_len;

	uint32_t sp_sec;
	uint32_t sp_usec;

	uint64_t sp_sender;
};

#define BUSMEM_SIZE (1024*1024)
#define BUSMEM_DATASIZE (BUSMEM_SIZE - sizeof(struct shmif_mem))

#if 0
#ifdef _KERNEL
#include <rump/rumpuser.h>
#define DPRINTF(x) rumpuser_dprintf x
#else
#include <stdio.h>
#define DPRINTF(x) printf x
#endif
#else
#define DPRINTF(x)
#endif

uint32_t	shmif_advance(uint32_t, uint32_t);
uint32_t	shmif_busread(struct shmif_mem *,
			      void *, uint32_t, size_t, bool *);
void		shmif_advancefirst(struct shmif_mem *, uint32_t, size_t);
uint32_t	shmif_buswrite(struct shmif_mem *, uint32_t,
			       void *, size_t, bool *);
uint32_t	shmif_nextpktoff(struct shmif_mem *, uint32_t);

#endif

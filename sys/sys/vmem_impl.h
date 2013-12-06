/*	$NetBSD: vmem_impl.h,v 1.3 2013/11/22 21:04:11 christos Exp $	*/

/*-
 * Copyright (c)2006 YAMAMOTO Takashi,
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Data structures private to vmem.
 */

#ifndef _SYS_VMEM_IMPL_H_
#define	_SYS_VMEM_IMPL_H_

#include <sys/types.h>

#if defined(_KERNEL)
#define	QCACHE
#include <sys/vmem.h>

#define	LOCK_DECL(name)		\
    kmutex_t name; char lockpad[COHERENCY_UNIT - sizeof(kmutex_t)]

#define CONDVAR_DECL(name)	\
    kcondvar_t name

#else /* defined(_KERNEL) */
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "vmem.h"

#define	LOCK_DECL(name)		/* nothing */
#define	CONDVAR_DECL(name)	/* nothing */
#endif /* defined(_KERNEL) */

#define	VMEM_MAXORDER		(sizeof(vmem_size_t) * CHAR_BIT)

typedef struct vmem_btag bt_t;

TAILQ_HEAD(vmem_seglist, vmem_btag);
LIST_HEAD(vmem_freelist, vmem_btag);
LIST_HEAD(vmem_hashlist, vmem_btag);

#if defined(QCACHE)
#define	VMEM_QCACHE_IDX_MAX	16

#define	QC_NAME_MAX	16

struct qcache {
	pool_cache_t qc_cache;
	vmem_t *qc_vmem;
	char qc_name[QC_NAME_MAX];
};
typedef struct qcache qcache_t;
#define	QC_POOL_TO_QCACHE(pool)	((qcache_t *)(pool->pr_qcache))
#endif /* defined(QCACHE) */

#define	VMEM_NAME_MAX	16

/* vmem arena */
struct vmem {
	CONDVAR_DECL(vm_cv);
	LOCK_DECL(vm_lock);
	vm_flag_t vm_flags;
	vmem_import_t *vm_importfn;
	vmem_release_t *vm_releasefn;
	size_t vm_nfreetags;
	LIST_HEAD(, vmem_btag) vm_freetags;
	void *vm_arg;
	struct vmem_seglist vm_seglist;
	struct vmem_freelist vm_freelist[VMEM_MAXORDER];
	size_t vm_hashsize;
	size_t vm_nbusytag;
	struct vmem_hashlist *vm_hashlist;
	struct vmem_hashlist vm_hash0;
	size_t vm_quantum_mask;
	int vm_quantum_shift;
	size_t vm_size;
	size_t vm_inuse;
	char vm_name[VMEM_NAME_MAX+1];
	LIST_ENTRY(vmem) vm_alllist;

#if defined(QCACHE)
	/* quantum cache */
	size_t vm_qcache_max;
	struct pool_allocator vm_qcache_allocator;
	qcache_t vm_qcache_store[VMEM_QCACHE_IDX_MAX];
	qcache_t *vm_qcache[VMEM_QCACHE_IDX_MAX];
#endif /* defined(QCACHE) */
};

/* boundary tag */
struct vmem_btag {
	TAILQ_ENTRY(vmem_btag) bt_seglist;
	union {
		LIST_ENTRY(vmem_btag) u_freelist; /* BT_TYPE_FREE */
		LIST_ENTRY(vmem_btag) u_hashlist; /* BT_TYPE_BUSY */
	} bt_u;
#define	bt_hashlist	bt_u.u_hashlist
#define	bt_freelist	bt_u.u_freelist
	vmem_addr_t bt_start;
	vmem_size_t bt_size;
	int bt_type;
};

#define	BT_TYPE_SPAN		1
#define	BT_TYPE_SPAN_STATIC	2
#define	BT_TYPE_FREE		3
#define	BT_TYPE_BUSY		4
#define	BT_ISSPAN_P(bt)	((bt)->bt_type <= BT_TYPE_SPAN_STATIC)

#define	BT_END(bt)	((bt)->bt_start + (bt)->bt_size - 1)

#endif /* !_SYS_VMEM_IMPL_H_ */

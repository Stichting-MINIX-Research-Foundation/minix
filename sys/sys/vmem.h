/*	$NetBSD: vmem.h,v 1.20 2013/01/29 21:26:24 para Exp $	*/

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

#ifndef _SYS_VMEM_H_
#define	_SYS_VMEM_H_

#include <sys/types.h>

#if defined(_KERNEL)
#else /* defined(_KERNEL) */
#include <stdbool.h>
#endif /* defined(_KERNEL) */

typedef struct vmem vmem_t;

typedef unsigned int vm_flag_t;

typedef	uintptr_t vmem_addr_t;
typedef size_t vmem_size_t;
#define	VMEM_ADDR_MIN	0
#define	VMEM_ADDR_MAX	(~(vmem_addr_t)0)

typedef int (vmem_import_t)(vmem_t *, vmem_size_t, vm_flag_t, vmem_addr_t *);
typedef void (vmem_release_t)(vmem_t *, vmem_addr_t, vmem_size_t);

typedef int (vmem_ximport_t)(vmem_t *, vmem_size_t, vmem_size_t *,
    vm_flag_t, vmem_addr_t *);

extern vmem_t *kmem_arena;
extern vmem_t *kmem_meta_arena;
extern vmem_t *kmem_va_arena;

void vmem_subsystem_init(vmem_t *vm);

vmem_t *vmem_create(const char *, vmem_addr_t, vmem_size_t, vmem_size_t,
    vmem_import_t *, vmem_release_t *, vmem_t *, vmem_size_t,
    vm_flag_t, int);
vmem_t *vmem_xcreate(const char *, vmem_addr_t, vmem_size_t, vmem_size_t,
    vmem_ximport_t *, vmem_release_t *, vmem_t *, vmem_size_t,
    vm_flag_t, int);
vmem_t *vmem_init(vmem_t *, const char *, vmem_addr_t, vmem_size_t, vmem_size_t,
    vmem_import_t *, vmem_release_t *, vmem_t *, vmem_size_t,
    vm_flag_t, int);
void vmem_destroy(vmem_t *);
int vmem_alloc(vmem_t *, vmem_size_t, vm_flag_t, vmem_addr_t *);
void vmem_free(vmem_t *, vmem_addr_t, vmem_size_t);
int vmem_xalloc(vmem_t *, vmem_size_t, vmem_size_t, vmem_size_t,
    vmem_size_t, vmem_addr_t, vmem_addr_t, vm_flag_t, vmem_addr_t *);
void vmem_xfree(vmem_t *, vmem_addr_t, vmem_size_t);
int vmem_add(vmem_t *, vmem_addr_t, vmem_size_t, vm_flag_t);
vmem_size_t vmem_roundup_size(vmem_t *, vmem_size_t);
vmem_size_t vmem_size(vmem_t *, int typemask);
void vmem_rehash_start(void);
void vmem_whatis(uintptr_t, void (*)(const char *, ...) __printflike(1, 2));
void vmem_print(uintptr_t, const char *, void (*)(const char *, ...)
    __printflike(1, 2));
void vmem_printall(const char *, void (*)(const char *, ...)
    __printflike(1, 2));

/* vm_flag_t */
#define	VM_SLEEP	0x00000001
#define	VM_NOSLEEP	0x00000002
#define	VM_INSTANTFIT	0x00001000
#define	VM_BESTFIT	0x00002000
#define	VM_BOOTSTRAP	0x00010000
#define	VM_POPULATING	0x00040000
#define	VM_LARGEIMPORT	0x00080000
#define	VM_XIMPORT	0x00100000

/* vmem_size typemask */
#define VMEM_ALLOC	0x01
#define VMEM_FREE	0x02

#endif /* !_SYS_VMEM_H_ */

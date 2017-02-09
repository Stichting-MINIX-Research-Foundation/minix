/*	$NetBSD: mm.h,v 1.2 2011/06/12 03:35:51 rmind Exp $	*/

/*-
 * Copyright (c) 2008 Joerg Sonnenberger <joerg@NetBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _SYS_DEV_MM_H_
#define _SYS_DEV_MM_H_

#include <sys/uio.h>
#include <uvm/uvm_prot.h>

/*
 * Required access check for the physical address.
 */
int	mm_md_physacc(paddr_t, vm_prot_t);

/*
 * Optional open() hook for MD.  Used by i386 for /dev/io emulation.
 *
 * machine/types.h must define __HAVE_MM_MD_OPEN to use this.
 */
int	mm_md_open(dev_t, int, int, struct lwp *);

/*
 * Optional read/write hook for additional minor devices.
 * Must handle the complete uio, not execute in a loop.
 *
 * machine/types.h must define __HAVE_MM_MD_READWRITE to use this.
 */
int	mm_md_readwrite(dev_t, struct uio *);

/*
 * Optional mmap hook for additional MD minor devices.
 *
 * machine/types.h must define __HAVE_MM_MD_READWRITE to use this.
 */
paddr_t	mm_md_mmap(dev_t, off_t, int);

/*
 * Optional access check for the virtual address. The third argument tells
 * mm that the check was done and uvm_kernacc is overriden.
 *
 * machine/types.h must define __HAVE_MM_MD_KERNACC to use this.
 */
int	mm_md_kernacc(void *, vm_prot_t, bool *);

/*
 * Optional hook to map physical address back to pre-mapped virtual space.
 * This is used e.g. when physical memory is lineary mapped.
 *
 * machine/types.h must define __HAVE_MM_MD_DIRECT_MAPPED_PHYS to use this.
 */
bool	mm_md_direct_mapped_phys(paddr_t, vaddr_t *);

/*
 * Optional hook to map virtual address back to physical address for explicit
 * access check. This is used e.g. when physical memory is lineary mapped.
 *
 * machine/types.h must define __HAVE_MM_MD_DIRECT_MAPPED_IO to use this.
 */
bool	mm_md_direct_mapped_io(void *, paddr_t *);

/*
 * Some architectures may need to deal with cache aliasing issues.
 *
 * machine/types.h must define __HAVE_MM_MD_CACHE_ALIASING to note that.
 */

#endif /* _SYS_DEV_MM_H_ */

/*	$NetBSD: rbus_machdep.h,v 1.9 2010/03/01 01:15:24 jym Exp $	*/

/*
 * Copyright (c) 1999
 *     HAYAKAWA Koichi.  All rights reserved.
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

#if !defined _ARCH_I386_I386_RBUS_MACHDEP_H_
#define _ARCH_I386_I386_RBUS_MACHDEP_H_

struct pci_attach_args;		/* XXX */

#define md_space_map(bt, physaddr, size, flags, bshp) \
	_x86_memio_map((bt), (physaddr), (size), (flags), (bshp))

#define md_space_unmap(bt, bsh, size, adrp) \
	_x86_memio_unmap((bt), (bsh), (size), (adrp))


rbus_tag_t rbus_pccbb_parent_io(struct pci_attach_args *);
rbus_tag_t rbus_pccbb_parent_mem(struct pci_attach_args *);

void rbus_min_start_hint(psize_t);

#endif /* _ARCH_I386_I386_RBUS_MACHDEP_H_ */

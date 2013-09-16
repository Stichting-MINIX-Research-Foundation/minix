/*	$NetBSD: kobj.h,v 1.16 2011/08/13 21:04:07 christos Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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

#ifndef _SYS_KOBJ_H_
#define	_SYS_KOBJ_H_

#define ELFSIZE ARCH_ELFSIZE
#include <sys/exec.h>
#include <sys/exec_elf.h>

typedef struct kobj *kobj_t;

/* External interface. */
int		kobj_load_vfs(kobj_t *, const char *, const bool);
int		kobj_load_mem(kobj_t *, const char *, void *, ssize_t);
int		kobj_affix(kobj_t, const char *);
void		kobj_unload(kobj_t);
int		kobj_stat(kobj_t, vaddr_t *, size_t *);
int		kobj_find_section(kobj_t, const char *, void **, size_t *);

/* MI-MD interface. */
uintptr_t	kobj_sym_lookup(kobj_t, uintptr_t);
int		kobj_reloc(kobj_t, uintptr_t, const void *, bool, bool);
int		kobj_machdep(kobj_t, void *, size_t, bool);

/* implementation interface. */
int		kobj_renamespace(Elf_Sym *, size_t, char **, size_t *);

#endif /* !_SYS_KOBJ_H_ */

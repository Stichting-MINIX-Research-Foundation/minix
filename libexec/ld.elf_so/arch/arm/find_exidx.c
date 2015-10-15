/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: find_exidx.c,v 1.4 2014/08/10 23:35:27 matt Exp $");
#endif /* not lint */

#include "debug.h"
#include "rtld.h"

#if defined(__ARM_EABI__) && !defined(__ARM_DWARF_EH__)

_Unwind_Ptr
__gnu_Unwind_Find_exidx(_Unwind_Ptr pc, int * pcount)
{
	const Obj_Entry *obj;
	_Unwind_Ptr start = NULL;
	int count = 0;

	dbg(("__gnu_Unwind_Find_exidx"));

	_rtld_shared_enter();

	vaddr_t va = (vaddr_t)pc;
	for (obj = _rtld_objlist; obj != NULL; obj = obj->next) {
		/*
		 * If the address we are looking for is inside this object,
		 * we've found the object to inspect.
		 */
		if ((vaddr_t)obj->mapbase <= va
		    && va < (vaddr_t)obj->mapbase + obj->mapsize)
			break;
	}

	/*
	 * If we found an object and it has some exception data, we
	 * need to see if the address matches a PT_LOAD section.
	 */
	if (obj != NULL && obj->exidx_start != NULL) {
		va -= (vaddr_t)obj->relocbase;
		const Elf_Phdr *ph = obj->phdr;
		const Elf_Phdr * const phlimit = ph + obj->phsize / sizeof(*ph);
		for (; ph < phlimit; ph++) {
			if (ph->p_type == PT_LOAD
			    && ph->p_vaddr <= va
			    && va < ph->p_vaddr + ph->p_memsz) {
				count = obj->exidx_sz / 8;
				start = obj->exidx_start;
				break;
			}
		}
	}

	_rtld_shared_exit();

	/*
	 * deal with the return values.
	 */
	*pcount = count;
	return start;
}

#endif

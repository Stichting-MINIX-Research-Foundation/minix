/*	$NetBSD: rump_x86_pmap.c,v 1.3 2015/04/17 12:43:16 pooka Exp $	*/

/*
 * Copyright (c) 2010 Antti Kantee.  All Rights Reserved.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rump_x86_pmap.c,v 1.3 2015/04/17 12:43:16 pooka Exp $");

#include <sys/param.h>

#include <uvm/uvm_extern.h>

#include "rump_private.h"

void
pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot, u_int fl)
{

	panic("%s: unavailable", __func__);
}

void
pmap_kremove(vaddr_t va, vsize_t size)
{

	panic("%s: unavailable", __func__);
}

int
pmap_enter(pmap_t pmap, vaddr_t va, paddr_t pa, vm_prot_t prot, u_int flags)
{

	panic("%s: unavailable", __func__);
}

bool
pmap_clear_attrs(struct vm_page *pg, unsigned what)
{

	return false;
}

void
pmap_page_remove(struct vm_page *pg)
{

}

bool
pmap_test_attrs(struct vm_page *pg, unsigned what)
{

	return true;
}

paddr_t
vtophys(vaddr_t va)
{

	return (paddr_t)va;
}

void
pmap_update(pmap_t pmap)
{

}

void
pmap_remove(pmap_t pmap, vaddr_t sva, vaddr_t eva)
{

	panic("%s: unavailable", __func__);
}

bool
pmap_extract(pmap_t pmap, vaddr_t va, paddr_t *pap)
{

	*pap = va;
	return true;
}

void
pmap_write_protect(pmap_t pmap, vaddr_t sva, vaddr_t eva, vm_prot_t prot)
{
}

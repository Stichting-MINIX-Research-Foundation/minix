/*	$NetBSD: prekern.c,v 1.6 2022/08/21 14:05:52 mlelstv Exp $	*/

/*
 * Copyright (c) 2017 The NetBSD Foundation, Inc. All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Maxime Villard.
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

#include "opt_realmem.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/cpu.h>
#include <sys/conf.h>

#include <uvm/uvm.h>
#include <machine/pmap.h>
#include <machine/pmap_private.h>
#include <machine/bootinfo.h>
#include <machine/cpufunc.h>

#include <x86/bootspace.h>

#include <dev/isa/isareg.h>
#include <machine/isa_machdep.h>

#define PREKERN_API_VERSION	2

struct prekern_args {
	int version;
	int boothowto;
	void *bootinfo;
	void *bootspace;
	int esym;
	int biosextmem;
	int biosbasemem;
	int cpuid_level;
	uint32_t nox_flag;
	uint64_t PDPpaddr;
	vaddr_t atdevbase;
	vaddr_t lwp0uarea;
	paddr_t first_avail;
};

void main(void);
void init_slotspace(void);
void init_x86_64(paddr_t);

static void prekern_copy_args(struct prekern_args *);
static void prekern_unmap(void);
int start_prekern(struct prekern_args *);

static void
prekern_copy_args(struct prekern_args *pkargs)
{
	extern int boothowto;
	extern struct bootinfo bootinfo;
	extern struct bootspace bootspace;
	extern int esym;
	extern int biosextmem;
	extern int biosbasemem;
	extern int cpuid_level;
	extern uint32_t nox_flag;
	extern uint64_t PDPpaddr;
	extern vaddr_t lwp0uarea;

	boothowto = pkargs->boothowto;
	memcpy(&bootinfo, pkargs->bootinfo, sizeof(bootinfo));
	memcpy(&bootspace, pkargs->bootspace, sizeof(bootspace));
	esym = pkargs->esym;

#ifndef REALEXTMEM
	biosextmem = pkargs->biosextmem;
#else
	biosextmem = REALEXTMEM;
#endif

#ifndef REALBASEMEM
	biosbasemem = pkargs->biosbasemem;
#else
	biosbasemem = REALBASEMEM;
#endif

	cpuid_level = pkargs->cpuid_level;
	nox_flag = pkargs->nox_flag;
	PDPpaddr = pkargs->PDPpaddr;
	atdevbase = pkargs->atdevbase;
	lwp0uarea = pkargs->lwp0uarea;
}

static void
prekern_unmap_pte(void)
{
	extern struct bootspace bootspace;
	pd_entry_t *pdir = (pd_entry_t *)bootspace.pdir;

	/* Unmap the prekern recursive PTE slot. */
	pdir[509] = 0;
	tlbflushg();
}

static void
prekern_unmap(void)
{
	/* Unmap the prekern itself. */
	L4_BASE[0] = 0;
	tlbflushg();
}

/*
 * The prekern jumps here.
 */
int
start_prekern(struct prekern_args *pkargs)
{
	paddr_t first_avail;

	if (pkargs->version != PREKERN_API_VERSION) {
		return -1;
	}

	prekern_copy_args(pkargs);
	first_avail = pkargs->first_avail;

	prekern_unmap_pte();
	init_slotspace();
	init_x86_64(first_avail);
	prekern_unmap();

	main();

	panic("main returned");

	return 0;
}

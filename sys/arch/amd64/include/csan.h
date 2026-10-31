/*	$NetBSD: csan.h,v 1.4 2020/09/10 14:04:45 maxv Exp $	*/

/*
 * Copyright (c) 2019-2020 Maxime Villard, m00nbsd.net
 * All rights reserved.
 *
 * This code is part of the KCSAN subsystem of the NetBSD kernel.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/ksyms.h>
#include <uvm/uvm.h>
#include <amd64/pmap.h>
#include <x86/cpufunc.h>

static inline bool
kcsan_md_unsupported(vaddr_t addr)
{
	return (addr >= (vaddr_t)PTE_BASE &&
	    addr < ((vaddr_t)PTE_BASE + NBPD_L4));
}

static inline bool
kcsan_md_is_avail(void)
{
	return true;
}

static inline void
kcsan_md_disable_intrs(uint64_t *state)
{
	*state = x86_read_psl();
	x86_disable_intr();
}

static inline void
kcsan_md_enable_intrs(uint64_t *state)
{
	x86_write_psl(*state);
}

static inline void
kcsan_md_delay(uint64_t us)
{
	DELAY(us);
}

static inline bool
__md_unwind_end(const char *name)
{
	if (!strcmp(name, "syscall") ||
	    !strcmp(name, "alltraps") ||
	    !strcmp(name, "handle_syscall") ||
	    !strncmp(name, "Xtrap", 5) ||
	    !strncmp(name, "Xintr", 5) ||
	    !strncmp(name, "Xhandle", 7) ||
	    !strncmp(name, "Xresume", 7) ||
	    !strncmp(name, "Xstray", 6) ||
	    !strncmp(name, "Xhold", 5) ||
	    !strncmp(name, "Xrecurse", 8) ||
	    !strcmp(name, "Xdoreti") ||
	    !strncmp(name, "Xsoft", 5)) {
		return true;
	}

	return false;
}

static void
kcsan_md_unwind(void)
{
	uint64_t *rbp, rip;
	const char *mod;
	const char *sym;
	size_t nsym;
	int error;

	rbp = (uint64_t *)__builtin_frame_address(0);
	nsym = 0;

	while (1) {
		/* 8(%rbp) contains the saved %rip. */
		rip = *(rbp + 1);

		if (rip < KERNBASE) {
			break;
		}
		error = ksyms_getname(&mod, &sym, (vaddr_t)rip, KSYMS_PROC);
		if (error) {
			break;
		}
		printf("#%zu %p in %s <%s>\n", nsym, (void *)rip, sym, mod);
		if (__md_unwind_end(sym)) {
			break;
		}

		rbp = (uint64_t *)*(rbp);
		if (rbp == 0) {
			break;
		}
		nsym++;

		if (nsym >= 15) {
			break;
		}
	}
}

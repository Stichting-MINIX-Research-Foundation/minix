/*	$NetBSD: sysarch.h,v 1.9 2010/07/07 01:14:53 chs Exp $	*/

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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

#ifndef _X86_SYSARCH_H_
#define _X86_SYSARCH_H_

#define X86_GET_LDT		0
#define X86_SET_LDT		1
#define	X86_IOPL		2
#define	X86_GET_IOPERM		3
#define	X86_SET_IOPERM		4
#define	X86_OLD_VM86		5
#define	X86_PMC_INFO		8
#define	X86_PMC_STARTSTOP	9
#define	X86_PMC_READ		10
#define X86_GET_MTRR		11
#define X86_SET_MTRR		12
#define	X86_VM86		13
#define	X86_GET_GSBASE		14
#define	X86_GET_FSBASE		15
#define	X86_SET_GSBASE		16
#define	X86_SET_FSBASE		17

#ifdef _KERNEL
#define	_X86_SYSARCH_L(x)	x86_##x
#define	_X86_SYSARCH_U(x)	X86_##x
#elif defined(__i386__)
#define	_X86_SYSARCH_L(x)	i386_##x
#define	_X86_SYSARCH_U(x)	I386_##x
#define I386_GET_LDT		X86_GET_LDT
#define I386_SET_LDT		X86_SET_LDT
#define	I386_IOPL		X86_IOPL
#define	I386_GET_IOPERM		X86_GET_IOPERM
#define	I386_SET_IOPERM		X86_SET_IOPERM
#define	I386_OLD_VM86		X86_OLD_VM86
#define	I386_PMC_INFO		X86_PMC_INFO
#define	I386_PMC_STARTSTOP	X86_PMC_STARTSTOP
#define	I386_PMC_READ		X86_PMC_READ
#define I386_GET_MTRR		X86_GET_MTRR
#define I386_SET_MTRR		X86_SET_MTRR
#define	I386_VM86		X86_VM86
#define	I386_GET_GSBASE		X86_GET_GSBASE
#define	I386_GET_FSBASE		X86_GET_FSBASE
#define	I386_SET_GSBASE		X86_SET_GSBASE
#define	I386_SET_FSBASE		X86_SET_FSBASE
#else
#define	_X86_SYSARCH_L(x)	x86_64_##x
#define	_X86_SYSARCH_U(x)	X86_64_##x
#define X86_64_GET_LDT		X86_GET_LDT
#define X86_64_SET_LDT		X86_SET_LDT
#define	X86_64_IOPL		X86_IOPL
#define	X86_64_GET_IOPERM	X86_GET_IOPERM
#define	X86_64_SET_IOPERM	X86_SET_IOPERM
#define	X86_64_OLD_VM86		X86_OLD_VM86
#define	X86_64_PMC_INFO		X86_PMC_INFO
#define	X86_64_PMC_STARTSTOP	X86_PMC_STARTSTOP
#define	X86_64_PMC_READ		X86_PMC_READ
#define X86_64_GET_MTRR		X86_GET_MTRR
#define X86_64_SET_MTRR		X86_SET_MTRR
#define	X86_64_VM86		X86_VM86
#define X86_64_GET_GSBASE	X86_GET_GSBASE
#define	X86_64_GET_FSBASE	X86_GET_FSBASE
#define X86_64_SET_GSBASE	X86_SET_GSBASE
#define	X86_64_SET_FSBASE	X86_SET_FSBASE
#endif

/*
 * Architecture specific syscalls (x86)
 */

struct _X86_SYSARCH_L(get_ldt_args) {
	int start;
	union descriptor *desc;
	int num;
};

struct _X86_SYSARCH_L(set_ldt_args) {
	int start;
	union descriptor *desc;
	int num;
};

struct _X86_SYSARCH_L(get_mtrr_args) {
	struct mtrr *mtrrp;
	int *n;
};

struct _X86_SYSARCH_L(set_mtrr_args) {
	struct mtrr *mtrrp;
	int *n;
};

struct _X86_SYSARCH_L(iopl_args) {
	int iopl;
};

struct _X86_SYSARCH_L(get_ioperm_args) {
	u_long *iomap;
};

struct _X86_SYSARCH_L(set_ioperm_args) {
	u_long *iomap;
};

struct _X86_SYSARCH_L(pmc_info_args) {
	int	type;
	int	flags;
};

#define	PMC_TYPE_NONE		0
#define	PMC_TYPE_I586		1
#define	PMC_TYPE_I686		2
#define	PMC_TYPE_K7		3

#define	PMC_INFO_HASTSC		0x01

#ifdef __i386__
#define	PMC_NCOUNTERS		4
#else
#define	PMC_NCOUNTERS		2
#endif

struct _X86_SYSARCH_L(pmc_startstop_args) {
	int counter;
	uint64_t val;
	uint8_t event;
	uint8_t unit;
	uint8_t compare;
	uint8_t flags;
};

#define	PMC_SETUP_KERNEL	0x01
#define	PMC_SETUP_USER		0x02
#define	PMC_SETUP_EDGE		0x04
#define	PMC_SETUP_INV		0x08

struct _X86_SYSARCH_L(pmc_read_args) {
	int counter;
	uint64_t val;
	uint64_t time;
};

struct mtrr;

#ifdef _KERNEL
int x86_iopl(struct lwp *, void *, register_t *);
int x86_get_mtrr(struct lwp *, void *, register_t *);
int x86_set_mtrr(struct lwp *, void *, register_t *);
int x86_get_ldt(struct lwp *, void *, register_t *);
int x86_get_ldt1(struct lwp *, struct x86_get_ldt_args *, union descriptor *);
int x86_set_ldt(struct lwp *, void *, register_t *);
int x86_set_ldt1(struct lwp *, struct x86_set_ldt_args *, union descriptor *);
int x86_set_sdbase(void *, char, lwp_t *, bool);
int x86_get_sdbase(void *, char);
#else
#include <sys/cdefs.h>
__BEGIN_DECLS
int _X86_SYSARCH_L(get_ldt)(int, union descriptor *, int);
int _X86_SYSARCH_L(set_ldt)(int, union descriptor *, int);
int _X86_SYSARCH_L(iopl)(int);
int _X86_SYSARCH_L(pmc_info)(struct _X86_SYSARCH_L(pmc_info_args *));
int _X86_SYSARCH_L(pmc_startstop)(struct _X86_SYSARCH_L(pmc_startstop_args *));
int _X86_SYSARCH_L(pmc_read)(struct _X86_SYSARCH_L(pmc_read_args *));
int _X86_SYSARCH_L(set_mtrr)(struct mtrr *, int *);
int _X86_SYSARCH_L(get_mtrr)(struct mtrr *, int *);
int sysarch(int, void *);
__END_DECLS
#endif

#endif /* !_X86_SYSARCH_H_ */

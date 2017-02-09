/*	$NetBSD: kloader.h,v 1.7 2008/09/08 23:36:54 gmcgarry Exp $	*/

/*-
 * Copyright (c) 2001, 2002, 2004 The NetBSD Foundation, Inc.
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

#ifndef _DEV_KLOADER_H_
#define	_DEV_KLOADER_H_

#ifdef KLOADER_NO_BOOTINFO
struct bootinfo {
	int dummy;
};
#else
#include <machine/bootinfo.h>
#endif

struct kloader_ops;
struct kloader_page_tag;
struct kloader_bootinfo;

/*
 * kloader_bootfunc_t load new kernel into existing kernel area from
 * lined list of new kernel pieces.  and then, jump to kernel
 * entry. This function must be PIC.
 */
typedef void kloader_bootfunc_t(struct kloader_bootinfo *,
    struct kloader_page_tag *);
/*
 * koader_jumpfunc_t jump to boot loader described abobe.
 */
typedef void kloader_jumpfunc_t(kloader_bootfunc_t *, vaddr_t,
    struct kloader_bootinfo *, struct kloader_page_tag *);
/*
 * reset func is optional. may be called when kloader reboot is failed.
 */
struct kloader_ops {
	kloader_jumpfunc_t *jump;
	kloader_bootfunc_t *boot;
	void (*reset)(void);
};

/*
 * new kernel is primary loaded into discrete pages.
 */
struct kloader_page_tag {
	uint32_t next;
	uint32_t src;
	uint32_t dst;
	uint32_t sz;
} __packed __aligned(4);

#define KLOADER_KERNELARGS_MAX		256

struct kloader_bootinfo {
	/* kernel entry point */
	vaddr_t entry;

	/* argc, argv type boot argument */
	int argc;
	char **argv;

	/* struct type boot argument */
	struct bootinfo bootinfo;

	/* argv buffer */
	char _argbuf[KLOADER_KERNELARGS_MAX];
} __packed __aligned(4);

/*
 * kloader_reboot_setup sets machine dependent kloader_ops to
 * kloader. (call __kloader_reboot_setup here.) and load new kernel.
 */
void kloader_reboot_setup(const char *);
void __kloader_reboot_setup(struct kloader_ops *, const char *);

/*
 * kloader_reboot jumps to 2nd boot loader.
 * call after all shutdown hooks done.
 */
void kloader_reboot(void) __dead;

/*
 * kloader_bootinfo_set sets arguments of new kernel to boot. this is optional.
 * theses parameter is passed to kloader_bootfunc_t.
 */
void kloader_bootinfo_set(struct kloader_bootinfo *, int, char *[],
    struct bootinfo *, int);

#endif /* _DEV_KLOADER_H_ */

/* $NetBSD: module.c,v 1.2 2022/09/21 14:30:01 riastradh Exp $ */

/*-
 * Copyright (c) 2020 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jared McNeill <jmcneill@invisible.ca>.
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

#include "efiboot.h"
#include "module.h"

#include <sys/queue.h>

int module_enabled = 1;
char module_prefix[128];

struct boot_module {
	char *module_name;
	TAILQ_ENTRY(boot_module) entries;
};
TAILQ_HEAD(, boot_module) boot_modules = TAILQ_HEAD_INITIALIZER(boot_modules);

static const char *
module_machine(void)
{
	return EFIBOOT_MODULE_MACHINE;
}

static void
module_set_prefix(const char *kernel_path)
{
#ifdef KERNEL_DIR
	char *ptr = strrchr(kernel_path, '/');
	if (ptr)
		*ptr = '\0';
	snprintf(module_prefix, sizeof(module_prefix), "%s/modules", kernel_path);
	if (ptr)
		*ptr = '/';
#else
	const u_int vmajor = netbsd_version / 100000000;
	const u_int vminor = netbsd_version / 1000000 % 100;
	const u_int vpatch = netbsd_version / 100 % 10000;

	if (vminor == 99) {
		snprintf(module_prefix, sizeof(module_prefix),
		    "/stand/%s/%u.%u.%u/modules", module_machine(),
		    vmajor, vminor, vpatch);
	} else if (vmajor != 0) {
		snprintf(module_prefix, sizeof(module_prefix),
		    "/stand/%s/%u.%u/modules", module_machine(),
		    vmajor, vminor);
	}
#endif
}

void
module_init(const char *kernel_path)
{
	module_set_prefix(kernel_path);
}

void
module_foreach(void (*fn)(const char *))
{
	struct boot_module *bm;

	TAILQ_FOREACH(bm, &boot_modules, entries) {
		fn(bm->module_name);
	}
}

void
module_add(const char *module_name)
{
	struct boot_module *bm;

	/* Trim leading whitespace */
	while (*module_name == ' ' || *module_name == '\t')
		++module_name;

	/* Duplicate check */
	TAILQ_FOREACH(bm, &boot_modules, entries) {
		if (strcmp(bm->module_name, module_name) == 0)
			return;
	}

	/* Add to list of modules */
	bm = alloc(sizeof(*bm));
	const int slen = strlen(module_name) + 1;
	bm->module_name = alloc(slen);
	memcpy(bm->module_name, module_name, slen);
	TAILQ_INSERT_TAIL(&boot_modules, bm, entries);
}

/*
 * Glob expansion for load_mods: the pattern's directory part is kept
 * as prefix, the last component is matched by dir_foreach (fnmatch).
 * Entries come back in name order, which is what keeps the MINIX
 * mod01..modNN boot order.
 */
static char module_glob_prefix[256];
static int module_glob_nadded;

static void
module_add_glob_cb(const char *name, const char *type)
{
	char path[512];

	if (strcmp(type, "DIR") == 0)
		return;

	snprintf(path, sizeof(path), "%s%s", module_glob_prefix, name);
	module_add(path);
	module_glob_nadded++;
}

void
module_add_glob(const char *pattern)
{
	const char *sep;
	size_t len;

	/* Trim leading whitespace */
	while (*pattern == ' ' || *pattern == '\t')
		++pattern;

	/* keep the prefix up to and including the last separator */
	sep = strrchr(pattern, '/');
	if (sep == NULL)
		sep = strrchr(pattern, ':');
	if (sep != NULL) {
		len = sep - pattern + 1;
		if (len >= sizeof(module_glob_prefix)) {
			printf("load_mods: %s: %s\n", pattern,
			    strerror(ENAMETOOLONG));
			return;
		}
		memcpy(module_glob_prefix, pattern, len);
	} else {
		len = 0;
	}
	module_glob_prefix[len] = '\0';

	module_glob_nadded = 0;
	dir_foreach(pattern, module_add_glob_cb);
	if (module_glob_nadded == 0)
		printf("load_mods: no match for %s\n", pattern);
}

void
module_remove(const char *module_name)
{
	struct boot_module *bm;

	/* Trim leading whitespace */
	while (*module_name == ' ' || *module_name == '\t')
		++module_name;

	TAILQ_FOREACH(bm, &boot_modules, entries) {
		if (strcmp(bm->module_name, module_name) == 0) {
			TAILQ_REMOVE(&boot_modules, bm, entries);
			dealloc(bm->module_name, strlen(bm->module_name) + 1);
			dealloc(bm, sizeof(*bm));
			return;
		}
	}
}

void
module_remove_all(void)
{
	struct boot_module *bm;

	while ((bm = TAILQ_FIRST(&boot_modules)) != NULL) {
		TAILQ_REMOVE(&boot_modules, bm, entries);
		dealloc(bm->module_name, strlen(bm->module_name) + 1);
		dealloc(bm, sizeof(*bm));
	}
}

void
module_enable(int onoff)
{
	module_enabled = onoff;
}

/*	$NetBSD: gpt_uuid.h,v 1.9 2019/06/30 11:38:16 sevan Exp $	*/

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
#ifndef _GPT_UUID_H
#define _GPT_UUID_H

#include <string.h>
#include <inttypes.h>
#ifndef HAVE_NBTOOL_CONFIG_H
#include <sys/disklabel_gpt.h>
#else
#include <nbinclude/sys/disklabel_gpt.h>
#endif

/*
 * We define our own uuid type so that we don't have to mess around
 * with different uuid implementation (linux+macosx which use an
 * array, and {Free,Net}BSD who use a struct. We just need minimal
 * support anyway
 */

// Must match the array in gpt_uuid.c
typedef enum {
	GPT_TYPE_INVALID = -1,
	GPT_TYPE_APPLE_HFS = 0,
	GPT_TYPE_APPLE_UFS,
	GPT_TYPE_BIOS,
	GPT_TYPE_EFI,
	GPT_TYPE_FREEBSD,
	GPT_TYPE_FREEBSD_SWAP,
	GPT_TYPE_FREEBSD_UFS,
	GPT_TYPE_FREEBSD_VINUM,
	GPT_TYPE_FREEBSD_ZFS,
	GPT_TYPE_LINUX_DATA,
	GPT_TYPE_LINUX_SWAP,
	GPT_TYPE_LINUX_RAID,
	GPT_TYPE_LINUX_LVM,
	GPT_TYPE_MS_BASIC_DATA,
	GPT_TYPE_MS_RESERVED,
	GPT_TYPE_NETBSD_CCD,
	GPT_TYPE_NETBSD_CGD,
	GPT_TYPE_NETBSD_FFS,
	GPT_TYPE_NETBSD_LFS,
	GPT_TYPE_NETBSD_RAIDFRAME,
	GPT_TYPE_NETBSD_SWAP,
	GPT_TYPE_MINIX_MFS,
	GPT_TYPE_VMWARE_VMKCORE,
	GPT_TYPE_VMWARE_VMFS,
	GPT_TYPE_VMWARE_RESERVED
} gpt_type_t;

typedef uint8_t gpt_uuid_t[16];
extern const gpt_uuid_t gpt_uuid_nil;

__BEGIN_DECLS
static inline int
gpt_uuid_is_nil(const gpt_uuid_t u) {
	return memcmp(u, gpt_uuid_nil, sizeof(gpt_uuid_t)) == 0;
}

static inline int
gpt_uuid_equal(const gpt_uuid_t u1, const gpt_uuid_t u2) {
	return memcmp(u1, u2, sizeof(gpt_uuid_t)) == 0;
}

static inline void
gpt_uuid_copy(gpt_uuid_t u1, const gpt_uuid_t u2) {
	memcpy(u1, u2, sizeof(gpt_uuid_t));
}

int gpt_uuid_snprintf(char *, size_t, const char *, const gpt_uuid_t);

void gpt_uuid_create(gpt_type_t, gpt_uuid_t, uint16_t *, size_t);

int gpt_uuid_parse(const char *, gpt_uuid_t);

struct gpt;
int gpt_uuid_generate(struct gpt *, gpt_uuid_t);

/* returns number of entries, callback func may be NULL */
size_t gpt_uuid_query(
    void (*func)(const char *uuid, const char *short_name, const char *desc));

void gpt_uuid_help(const char *);

__END_DECLS

#endif /* _GPT_UUID_T */

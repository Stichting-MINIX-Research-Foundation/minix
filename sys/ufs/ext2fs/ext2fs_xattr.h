/*	$NetBSD: ext2fs_xattr.h,v 1.2 2016/08/12 20:30:15 macallan Exp $	*/

/*-
 * Copyright (c) 2016 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
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

#ifndef _UFS_EXT2FS_EXT2FS_XATTR_H_
#define _UFS_EXT2FS_EXT2FS_XATTR_H_

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ext2fs_xattr.h,v 1.2 2016/08/12 20:30:15 macallan Exp $");

#ifdef _KERNEL

#define EXT2FS_XATTR_MAGIC 	0xEA020000

#define EXT2FS_XATTR_NAME_LEN_MAX	255
#define EXT2FS_XATTR_REFCOUNT_MAX	1024

/*
 * This is used as header for extended attribute block within inode
 */
struct ext2fs_xattr_ibody_header {
	uint32_t h_magic;	/* Magic number - 0xEA020000 */
};

/*
 * This is used as header for extended attribute on separate disk block
 */
struct ext2fs_xattr_header {
	uint32_t h_magic;	/* Magic number - 0xEA020000 */
	uint32_t h_refcount;	/* Reference count */
	uint32_t h_blocks;	/* Number of blocks - only 1 supported */
	uint32_t h_hash;	/* Hash of all attributes */
 	uint32_t h_checksum;	/* Checksum of the extended attribute block */
	uint32_t h_reserved[3];
};

/*
 * Extended attribute on-disk header structure
 */
struct ext2fs_xattr_entry {
        uint8_t  e_name_len;		/* Name length */
        uint8_t  e_name_index;		/* Name prefix index (see below) */
        uint16_t e_value_offs;		/* Offset of value within block */
        uint32_t e_value_block;		/* Value block - not supported (always zero) */
        uint32_t e_value_size;		/* Length of value */
        uint32_t e_hash;		/* Hash (not supported) */
        char e_name[0];			/* Name string (e_name_len bytes) */
};

/*
 * Linux kernel checks only the 0, we also check that the current entry
 * doesn't overflow past end.
 */
#define EXT2FS_XATTR_IS_LAST_ENTRY(entry, end) \
	(*((uint32_t *)(entry)) == 0 || (uintptr_t)EXT2FS_XATTR_NEXT(entry) > (uintptr_t)end)

/*
 * Each ext2fs_xattr_entry starts on next 4-byte boundary, pad if necessary.
 */
#define EXT2FS_XATTR_PAD	4
#define EXT2FS_XATTR_ROUND	(EXT2FS_XATTR_PAD - 1)
#define EXT2FS_XATTR_LEN(name_len) \
	(((name_len) + EXT2FS_XATTR_ROUND + \
	sizeof(struct ext2fs_xattr_entry)) & ~EXT2FS_XATTR_ROUND)
#define EXT2FS_XATTR_NEXT(entry) \
	(struct ext2fs_xattr_entry *)(((uint8_t *)(entry)) + EXT2FS_XATTR_LEN((entry)->e_name_len))

#define EXT2FS_XATTR_IFIRST(h) (void *)&(h)[1]
#define EXT2FS_XATTR_BFIRST(h) EXT2FS_XATTR_IFIRST(h)

/*
 * Name prefixes
 */
#define EXT2FS_XATTR_PREFIX_NONE		0 /* no prefix */
#define EXT2FS_XATTR_PREFIX_USER		1 /* "user." */
#define EXT2FS_XATTR_PREFIX_POSIX_ACCESS	2 /* "system.posix_acl_access" */
#define EXT2FS_XATTR_PREFIX_POSIX_DEFAULT1	3 /* "system.posix_acl_default" */
#define EXT2FS_XATTR_PREFIX_TRUSTED		4 /* "trusted." */
#define EXT2FS_XATTR_PREFIX_SECURITY		6 /* "security." */
#define EXT2FS_XATTR_PREFIX_SYSTEM		7 /* "system." */
#define EXT2FS_XATTR_PREFIX_SYSTEM_RICHACL	8 /* "system.richacl" */
#define EXT2FS_XATTR_PREFIX_ENCRYPTION		9 /* "c" */

int ext2fs_getextattr(void *);
int ext2fs_setextattr(void *);
int ext2fs_listextattr(void *);
int ext2fs_deleteextattr(void *);
#endif /* _KERNEL */

#endif /* _UFS_EXT2FS_EXT2FS_XATTR_H_ */

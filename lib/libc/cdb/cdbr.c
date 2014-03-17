/*	$NetBSD: cdbr.c,v 1.4 2012/09/27 00:37:43 joerg Exp $	*/
/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Joerg Sonnenberger.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
__RCSID("$NetBSD: cdbr.c,v 1.4 2012/09/27 00:37:43 joerg Exp $");

#include "namespace.h"

#if !HAVE_NBTOOL_CONFIG_H
#include <sys/bitops.h>
#endif
#if !HAVE_NBTOOL_CONFIG_H || HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif

#include <sys/mman.h>
#include <sys/stat.h>
#include <cdbr.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(cdbr_close,_cdbr_close)
__weak_alias(cdbr_find,_cdbr_find)
__weak_alias(cdbr_get,_cdbr_get)
__weak_alias(cdbr_open,_cdbr_open)
#endif

#if HAVE_NBTOOL_CONFIG_H
#define	fast_divide32_prepare(d,m,s1,s2)	(void)0
#define	fast_remainder32(v,d,m,s1,s2)		(v%d)
#endif

struct cdbr {
	uint8_t *mmap_base;
	size_t mmap_size;

	uint8_t *hash_base;
	uint8_t *offset_base;
	uint8_t *data_base;

	uint32_t data_size;
	uint32_t entries;
	uint32_t entries_index;
	uint32_t seed;

	uint8_t offset_size;
	uint8_t index_size;

	uint32_t entries_m;
	uint32_t entries_index_m;
	uint8_t entries_s1, entries_s2;
	uint8_t entries_index_s1, entries_index_s2;
};

/* ARGSUSED */
struct cdbr *
cdbr_open(const char *path, int flags)
{
	uint8_t buf[40];
	int fd;
	struct cdbr *cdbr;
	struct stat sb;

	if ((fd = open(path, O_RDONLY)) == -1)
		return NULL;

	errno = EINVAL;
	if (fstat(fd, &sb) == -1 ||
	    read(fd, buf, sizeof(buf)) != sizeof(buf) ||
	    memcmp(buf, "NBCDB\n\0\001", 8) ||
	    (cdbr = malloc(sizeof(*cdbr))) == NULL) {
		close(fd);
		return NULL;
	}

	cdbr->data_size = le32dec(buf + 24);
	cdbr->entries = le32dec(buf + 28);
	cdbr->entries_index = le32dec(buf + 32);
	cdbr->seed = le32dec(buf + 36);

	if (cdbr->data_size < 0x100)
		cdbr->offset_size = 1;
	else if (cdbr->data_size < 0x10000)
		cdbr->offset_size = 2;
	else
		cdbr->offset_size = 4;

	if (cdbr->entries_index < 0x100)
		cdbr->index_size = 1;
	else if (cdbr->entries_index < 0x10000)
		cdbr->index_size = 2;
	else
		cdbr->index_size = 4;

	cdbr->mmap_size = (size_t)sb.st_size;
	cdbr->mmap_base = mmap(NULL, cdbr->mmap_size, PROT_READ, MAP_FILE|MAP_SHARED, fd, 0);
	close(fd);

	if (cdbr->mmap_base == MAP_FAILED) {
		free(cdbr);
		return NULL;
	}

	cdbr->hash_base = cdbr->mmap_base + 40;
	cdbr->offset_base = cdbr->hash_base + cdbr->entries_index * cdbr->index_size;
	if (cdbr->entries_index * cdbr->index_size % cdbr->offset_size)
		cdbr->offset_base += cdbr->offset_size -
		    cdbr->entries_index * cdbr->index_size % cdbr->offset_size;
	cdbr->data_base = cdbr->offset_base + (cdbr->entries + 1) * cdbr->offset_size;

	if (cdbr->hash_base < cdbr->mmap_base ||
	    cdbr->offset_base < cdbr->mmap_base ||
	    cdbr->data_base < cdbr->mmap_base ||
	    cdbr->data_base + cdbr->data_size < cdbr->mmap_base ||
	    cdbr->data_base + cdbr->data_size >
	    cdbr->mmap_base + cdbr->mmap_size) {
		errno = EINVAL;
		cdbr_close(cdbr);
		return NULL;
	}

	if (cdbr->entries) {
		fast_divide32_prepare(cdbr->entries, &cdbr->entries_m,
		    &cdbr->entries_s1, &cdbr->entries_s2);
	}
	if (cdbr->entries_index) {
		fast_divide32_prepare(cdbr->entries_index,
		    &cdbr->entries_index_m,
		    &cdbr->entries_index_s1, &cdbr->entries_index_s2);
	}

	return cdbr;
}

static inline uint32_t
get_uintX(const uint8_t *addr, uint32_t idx, int size)
{
	addr += idx * size;

	if (size == 4)
		return /* LINTED */le32toh(*(const uint32_t *)addr);
	else if (size == 2)
		return /* LINTED */le16toh(*(const uint16_t *)addr);
	else
		return *addr;
}

uint32_t
cdbr_entries(struct cdbr *cdbr)
{

	return cdbr->entries;
}

int
cdbr_get(struct cdbr *cdbr, uint32_t idx, const void **data, size_t *data_len)
{
	uint32_t start, end;

	if (idx >= cdbr->entries) {
		errno = EINVAL;
		return -1;
	}

	start = get_uintX(cdbr->offset_base, idx, cdbr->offset_size);
	end = get_uintX(cdbr->offset_base, idx + 1, cdbr->offset_size);

	if (start > end) {
		errno = EIO;
		return -1;
	}

	if (end > cdbr->data_size) {
		errno = EIO;
		return -1;
	}

	*data = cdbr->data_base + start;
	*data_len = end - start;

	return 0;
}

int
cdbr_find(struct cdbr *cdbr, const void *key, size_t key_len,
    const void **data, size_t *data_len)
{
	uint32_t hashes[3], idx;

	if (cdbr->entries_index == 0) {
		errno = EINVAL;
		return -1;
	}

	mi_vector_hash(key, key_len, cdbr->seed, hashes);

	hashes[0] = fast_remainder32(hashes[0], cdbr->entries_index,
	    cdbr->entries_index_m, cdbr->entries_index_s1,
	    cdbr->entries_index_s2);
	hashes[1] = fast_remainder32(hashes[1], cdbr->entries_index,
	    cdbr->entries_index_m, cdbr->entries_index_s1,
	    cdbr->entries_index_s2);
	hashes[2] = fast_remainder32(hashes[2], cdbr->entries_index,
	    cdbr->entries_index_m, cdbr->entries_index_s1,
	    cdbr->entries_index_s2);

	idx = get_uintX(cdbr->hash_base, hashes[0], cdbr->index_size);
	idx += get_uintX(cdbr->hash_base, hashes[1], cdbr->index_size);
	idx += get_uintX(cdbr->hash_base, hashes[2], cdbr->index_size);

	return cdbr_get(cdbr, fast_remainder32(idx, cdbr->entries,
	    cdbr->entries_m, cdbr->entries_s1, cdbr->entries_s2), data,
	    data_len);
}

void
cdbr_close(struct cdbr *cdbr)
{
	munmap(cdbr->mmap_base, cdbr->mmap_size);
	free(cdbr);
}

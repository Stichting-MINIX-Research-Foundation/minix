/*	$NetBSD: v7fs_io_user.c,v 1.4 2011/08/08 11:42:30 uch Exp $	*/

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: v7fs_io_user.c,v 1.4 2011/08/08 11:42:30 uch Exp $");
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <err.h>
#include <sys/mman.h>
#include "v7fs.h"
#include "v7fs_endian.h"
#include "v7fs_impl.h"

#ifdef V7FS_IO_DEBUG
#define	DPRINTF(fmt, args...)	printf("%s: " fmt, __func__, ##args)
#else
#define	DPRINTF(fmt, args...)	((void)0)
#endif

struct local_io {
	int fd;
	size_t size;
	size_t blksz;
	uint8_t *addr;
} local;

static bool read_sector(void *, uint8_t *, daddr_t);
static bool write_sector(void *, uint8_t *, daddr_t);
static bool read_mmap(void *, uint8_t *, daddr_t);
static bool write_mmap(void *, uint8_t *, daddr_t);

int
v7fs_io_init(struct v7fs_self **fs, const struct v7fs_mount_device *mount,
    size_t block_size)
{
	struct v7fs_self *p;

	if (!(p = (struct v7fs_self *)malloc(sizeof(*p))))
		return ENOMEM;
	memset(p, 0, sizeof(*p));

	/* Endian */
	p->endian = mount->endian;
#ifdef V7FS_EI
	v7fs_endian_init(p);
#endif
	local.blksz = block_size;
	local.fd = mount->device.fd;
	local.size = mount->sectors * block_size;
	local.addr = mmap(NULL, local.size, PROT_READ | PROT_WRITE | PROT_NONE,
	    MAP_FILE | MAP_SHARED/*writeback*/, local.fd,  0);
	if (local.addr == MAP_FAILED) {
		local.addr = 0;
		p->io.read = read_sector;
		p->io.write = write_sector;
	} else {
		DPRINTF("mmaped addr=%p\n", local.addr);
		p->io.read = read_mmap;
		p->io.write = write_mmap;
	}

	p->io.cookie = &local;
	*fs = p;

	return 0;
}

void
v7fs_io_fini(struct v7fs_self *fs)
{
	struct local_io *lio = (struct local_io *)fs->io.cookie;

	if (lio->addr) {
		if (munmap(lio->addr, lio->size) != 0)
			warn(0);
	}
	fsync(lio->fd);

	free(fs);
}

static bool
read_sector(void *ctx, uint8_t *buf, daddr_t sector)
{
	struct local_io *lio = (struct local_io *)ctx;
	size_t blksz = lio->blksz;
	int fd = lio->fd;

	if ((lseek(fd, (off_t)sector * blksz, SEEK_SET) < 0) ||
	    (read(fd, buf, blksz) < (ssize_t)blksz)) {
		warn("sector=%ld\n", (long)sector);
		return false;
	}

	return true;
}

static bool
write_sector(void *ctx, uint8_t *buf, daddr_t sector)
{
	struct local_io *lio = (struct local_io *)ctx;
	size_t blksz = lio->blksz;
	int fd = lio->fd;

	if ((lseek(fd, (off_t)sector * blksz, SEEK_SET) < 0) ||
	    (write(fd, buf, blksz) < (ssize_t)blksz)) {
		warn("sector=%ld\n", (long)sector);
		return false;
	}

	return true;
}

static bool
read_mmap(void *ctx, uint8_t *buf, daddr_t sector)
{
	struct local_io *lio = (struct local_io *)ctx;
	size_t blksz = lio->blksz;

	memcpy(buf, lio->addr + sector * blksz, blksz);

	return true;
}

static bool
write_mmap(void *ctx, uint8_t *buf, daddr_t sector)
{
	struct local_io *lio = (struct local_io *)ctx;
	size_t blksz = lio->blksz;

	memcpy(lio->addr + sector * blksz, buf, blksz);

	return true;
}

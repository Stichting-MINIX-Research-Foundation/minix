/*	$NetBSD: bfs.c,v 1.18 2014/12/26 15:22:15 hannken Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
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

#include <sys/cdefs.h>

__KERNEL_RCSID(0, "$NetBSD: bfs.c,v 1.18 2014/12/26 15:22:15 hannken Exp $");
#define	BFS_DEBUG

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/time.h>

#ifdef _KERNEL
MALLOC_JUSTDEFINE(M_BFS, "sysvbfs core", "sysvbfs internal structures");
#define	__MALLOC(s, t, f)	malloc(s, t, f)
#define	__FREE(a, s, t)		free(a, t)
#elif defined _STANDALONE
#include <lib/libsa/stand.h>
#include <lib/libkern/libkern.h>
#define	__MALLOC(s, t, f)	alloc(s)
#define	__FREE(a, s, t)		dealloc(a, s)
#else
#include "local.h"
#define	__MALLOC(s, t, f)	malloc(s)
#define	__FREE(a, s, t)		free(a)
#endif
#include <fs/sysvbfs/bfs.h>

#ifdef BFS_DEBUG
#define	DPRINTF(on, fmt, args...)	if (on) printf(fmt, ##args)
#else
#define	DPRINTF(arg...)		((void)0)
#endif

#define	ROUND_SECTOR(x)		(((x) + 511) & ~511)
#define	TRUNC_SECTOR(x)		((x) & ~511)

#define	STATIC

STATIC int bfs_init_superblock(struct bfs *, int, size_t *);
STATIC int bfs_init_inode(struct bfs *, uint8_t *, size_t *);
STATIC int bfs_init_dirent(struct bfs *, uint8_t *);

/* super block ops. */
STATIC bool bfs_superblock_valid(const struct bfs_super_block *);
STATIC bool bfs_writeback_dirent(const struct bfs *, struct bfs_dirent *,
    bool);
STATIC bool bfs_writeback_inode(const struct bfs *, struct bfs_inode *);

int
bfs_init2(struct bfs **bfsp, int bfs_sector, struct sector_io_ops *io,
    bool debug)
{
	struct bfs *bfs;
	size_t memsize;
	uint8_t *p;
	int err;

	/* 1. */
	DPRINTF(debug, "bfs sector = %d\n", bfs_sector);
	if ((bfs = (void *)__MALLOC(sizeof(struct bfs), M_BFS, M_NOWAIT)) == 0)
		return ENOMEM;
	memset(bfs, 0, sizeof *bfs);
	bfs->io = io;
	bfs->debug = debug;

	/* 2. */
	if ((err = bfs_init_superblock(bfs, bfs_sector, &memsize)) != 0) {
		bfs_fini(bfs);
		return err;
	}
	DPRINTF(debug, "bfs super block + inode area = %zd\n", memsize);
	bfs->super_block_size = memsize;
	if ((p = (void *)__MALLOC(memsize, M_BFS, M_NOWAIT)) == 0) {
		bfs_fini(bfs);
		return ENOMEM;
	}
	/* 3. */
	if ((err = bfs_init_inode(bfs, p, &memsize)) != 0) {
		bfs_fini(bfs);
		return err;
	}
	DPRINTF(debug, "bfs dirent area = %zd\n", memsize);
	bfs->dirent_size = memsize;
	if ((p = (void *)__MALLOC(memsize, M_BFS, M_NOWAIT)) == 0) {
		bfs_fini(bfs);
		return ENOMEM;
	}
	/* 4. */
	if ((err = bfs_init_dirent(bfs, p)) != 0) {
		bfs_fini(bfs);
		return err;
	}

#ifdef BFS_DEBUG
	bfs_dump(bfs);
#endif
	*bfsp = bfs;

	return 0;
}

void
bfs_fini(struct bfs *bfs)
{

	if (bfs == 0)
		return;
	if (bfs->super_block)
		__FREE(bfs->super_block, bfs->super_block_size, M_BFS);
	if (bfs->dirent)
		__FREE(bfs->dirent, bfs->dirent_size, M_BFS);
	__FREE(bfs, sizeof(struct bfs), M_BFS);
}

STATIC int
bfs_init_superblock(struct bfs *bfs, int bfs_sector, size_t *required_memory)
{
	struct bfs_super_block super;

	bfs->start_sector = bfs_sector;

	/* Read super block */
	if (!bfs->io->read(bfs->io, (uint8_t *)&super, bfs_sector))
		return EIO;

	if (!bfs_superblock_valid(&super))
		return EINVAL;

	/* i-node table size */
	bfs->data_start = super.header.data_start_byte;
	bfs->data_end = super.header.data_end_byte;

	bfs->max_inode = (bfs->data_start - sizeof(struct bfs_super_block)) /
	    sizeof(struct bfs_inode);

	*required_memory = ROUND_SECTOR(bfs->data_start);

	return 0;
}

STATIC int
bfs_init_inode(struct bfs *bfs, uint8_t *p, size_t *required_memory)
{
	struct bfs_inode *inode, *root_inode;
	int i;

	if (!bfs->io->read_n(bfs->io, p, bfs->start_sector,
	    bfs->data_start >> DEV_BSHIFT))
		return EIO;

	bfs->super_block = (struct bfs_super_block *)p;
	bfs->inode = (struct bfs_inode *)(p + sizeof(struct bfs_super_block));
	p += bfs->data_start;

	bfs->n_inode = 0;
	inode = bfs->inode;
	root_inode = 0;
	for (i = 0; i < bfs->max_inode; i++, inode++) {
		if (inode->number != 0) {
			bfs->n_inode++;
			if (inode->number == BFS_ROOT_INODE)
				root_inode = inode;
		}
	}
	DPRINTF(bfs->debug, "inode: %d/%d\n", bfs->n_inode, bfs->max_inode);

	if (root_inode == 0) {
		DPRINTF(bfs->debug, "no root directory.\n");
		return ENOTDIR;
	}
	/* dirent table size */
	DPRINTF(bfs->debug, "root inode: %d-%d\n", root_inode->start_sector,
	    root_inode->end_sector);
	bfs->root_inode = root_inode;

	*required_memory = (root_inode->end_sector -
	    root_inode->start_sector + 1) << DEV_BSHIFT;

	return 0;
}

STATIC int
bfs_init_dirent(struct bfs *bfs, uint8_t *p)
{
	struct bfs_dirent *file;
	struct bfs_inode *inode = bfs->root_inode;
	int i, n;

	n = inode->end_sector - inode->start_sector + 1;

	if (!bfs->io->read_n(bfs->io, p,
	    bfs->start_sector + inode->start_sector, n))
		return EIO;

	bfs->dirent = (struct bfs_dirent *)p;
	bfs->max_dirent = (n << DEV_BSHIFT) / sizeof(struct bfs_dirent);

	file = bfs->dirent;
	bfs->n_dirent = 0;
	for (i = 0; i < bfs->max_dirent; i++, file++)
		if (file->inode != 0)
			bfs->n_dirent++;

	DPRINTF(bfs->debug, "dirent: %d/%d\n", bfs->n_dirent, bfs->max_dirent);

	return 0;
}

int
bfs_file_read(const struct bfs *bfs, const char *fname, void *buf, size_t bufsz,
    size_t *read_size)
{
	int start, end, n;
	size_t sz;
	uint8_t tmpbuf[DEV_BSIZE];
	uint8_t *p;

	if (!bfs_file_lookup(bfs, fname, &start, &end, &sz))
		return ENOENT;

	if (sz > bufsz)
		return ENOMEM;

	p = buf;
	n = end - start;
	if (!bfs->io->read_n(bfs->io, p, start, n))
		return EIO;
	/* last sector */
	n *= DEV_BSIZE;
	if (!bfs->io->read(bfs->io, tmpbuf, end))
		return EIO;
	memcpy(p + n, tmpbuf, sz - n);

	if (read_size)
		*read_size = sz;

	return 0;
}

int
bfs_file_write(struct bfs *bfs, const char *fname, void *buf,
    size_t bufsz)
{
	struct bfs_fileattr attr;
	struct bfs_dirent *dirent;
	char name[BFS_FILENAME_MAXLEN];
	int err;

	strncpy(name, fname, BFS_FILENAME_MAXLEN);

	if (bfs_dirent_lookup_by_name(bfs, name, &dirent)) {
		struct bfs_inode *inode;
		if (!bfs_inode_lookup(bfs, dirent->inode, &inode)) {
			DPRINTF(bfs->debug, "%s: dirent found, but inode "
			    "not found. inconsistent filesystem.\n",
			    __func__);
			return ENOENT;
		}
		attr = inode->attr;	/* copy old attribute */
		bfs_file_delete(bfs, name, false);
		if ((err = bfs_file_create(bfs, name, buf, bufsz, &attr)) != 0)
			return err;
	} else {
		memset(&attr, 0xff, sizeof attr);	/* Set VNOVAL all */
#ifdef _KERNEL
		attr.atime = time_second;
		attr.ctime = time_second;
		attr.mtime = time_second;
#endif
		if ((err = bfs_file_create(bfs, name, buf, bufsz, &attr)) != 0)
			return err;
	}

	return 0;
}

int
bfs_file_delete(struct bfs *bfs, const char *fname, bool keep_inode)
{
	struct bfs_inode *inode;
	struct bfs_dirent *dirent;

	if (!bfs_dirent_lookup_by_name(bfs, fname, &dirent))
		return ENOENT;

	if (!keep_inode && !bfs_inode_lookup(bfs, dirent->inode, &inode))
		return ENOENT;

	memset(dirent, 0, sizeof *dirent);
	bfs->n_dirent--;
	bfs_writeback_dirent(bfs, dirent, false);

	if (!keep_inode) {
		memset(inode, 0, sizeof *inode);
		bfs->n_inode--;
		bfs_writeback_inode(bfs, inode);
	}
	DPRINTF(bfs->debug, "%s: \"%s\" deleted.\n", __func__, fname);

	return 0;
}

int
bfs_file_rename(struct bfs *bfs, const char *from_name, const char *to_name)
{
	struct bfs_dirent *dirent;
	int err = 0;

	if (!bfs_dirent_lookup_by_name(bfs, from_name, &dirent)) {
		err = ENOENT;
		goto out;
	}

	strncpy(dirent->name, to_name, BFS_FILENAME_MAXLEN);
	bfs_writeback_dirent(bfs, dirent, false);

 out:
	DPRINTF(bfs->debug, "%s: \"%s\" -> \"%s\" error=%d.\n", __func__,
	    from_name, to_name, err);

	return err;
}

int
bfs_file_create(struct bfs *bfs, const char *fname, void *buf, size_t bufsz,
    const struct bfs_fileattr *attr)
{
	struct bfs_inode *inode;
	struct bfs_dirent *file;
	int i, j, n, start;
	uint8_t *p, tmpbuf[DEV_BSIZE];
	int err;

	/* Find free i-node and data block */
	if ((err = bfs_inode_alloc(bfs, &inode, &j, &start)) != 0)
		return err;

	/* File size (unit block) */
	n = (ROUND_SECTOR(bufsz) >> DEV_BSHIFT) - 1;
	if (n < 0)	/* bufsz == 0 */
		n = 0;

	if ((start + n) * DEV_BSIZE >= bfs->data_end) {
		DPRINTF(bfs->debug, "disk full.\n");
		return ENOSPC;
	}

	/* Find free dirent */
	for (file = bfs->dirent, i = 0; i < bfs->max_dirent; i++, file++)
		if (file->inode == 0)
			break;
	if (i == bfs->max_dirent) {
		DPRINTF(bfs->debug, "dirent full.\n");
		return ENOSPC;
	}

	/* i-node */
	memset(inode, 0, sizeof *inode);
	inode->number = j;
	inode->start_sector = start;
	inode->end_sector = start + n;
	inode->eof_offset_byte = start * DEV_BSIZE + bufsz - 1;
	/* i-node attribute */
	inode->attr.type = 1;
	inode->attr.mode = 0;
	inode->attr.nlink = 1;
	bfs_inode_set_attr(bfs, inode, attr);

	/* Dirent */
	memset(file, 0, sizeof *file);
	file->inode = inode->number;
	strncpy(file->name, fname, BFS_FILENAME_MAXLEN);

	DPRINTF(bfs->debug, "%s: start %d end %d\n", __func__,
	    inode->start_sector, inode->end_sector);

	if (buf != 0) {
		p = (uint8_t *)buf;
		/* Data block */
		n = 0;
		for (i = inode->start_sector; i < inode->end_sector; i++) {
			if (!bfs->io->write(bfs->io, p, bfs->start_sector + i))
				return EIO;
			p += DEV_BSIZE;
			n += DEV_BSIZE;
		}
		/* last sector */
		memset(tmpbuf, 0, DEV_BSIZE);
		memcpy(tmpbuf, p, bufsz - n);
		if (!bfs->io->write(bfs->io, tmpbuf, bfs->start_sector + i))
			return EIO;
	}
	/* Update */
	bfs->n_inode++;
	bfs->n_dirent++;
	bfs_writeback_dirent(bfs, file, true);
	bfs_writeback_inode(bfs, inode);

	return 0;
}

STATIC bool
bfs_writeback_dirent(const struct bfs *bfs, struct bfs_dirent *dir,
    bool create)
{
	struct bfs_dirent *dir_base = bfs->dirent;
	struct bfs_inode *root_inode = bfs->root_inode;
	uintptr_t eof;
	int i;

	i = ((dir - dir_base) * sizeof *dir) >> DEV_BSHIFT;

	eof = (uintptr_t)(dir + 1) - 1;
	eof = eof - (uintptr_t)dir_base +
	    (root_inode->start_sector << DEV_BSHIFT);

	/* update root directory inode */
#if 0
	printf("eof new=%d old=%d\n", eof, root_inode->eof_offset_byte);
#endif
	if (create) {
		if (eof > root_inode->eof_offset_byte) {
			root_inode->eof_offset_byte = eof;
		}
	} else {
		/* delete the last entry */
		if (eof == root_inode->eof_offset_byte) {
			root_inode->eof_offset_byte = eof - sizeof *dir;
		}
	}
	bfs_writeback_inode(bfs, root_inode);

	/* update dirent */
	return bfs->io->write(bfs->io, (uint8_t *)dir_base + (i << DEV_BSHIFT),
	    bfs->start_sector + bfs->root_inode->start_sector + i);
}

STATIC bool
bfs_writeback_inode(const struct bfs *bfs, struct bfs_inode *inode)
{
	struct bfs_inode *inode_base = bfs->inode;
	int i;

	i = ((inode - inode_base) * sizeof *inode) >> DEV_BSHIFT;

	return bfs->io->write(bfs->io,
	    (uint8_t *)inode_base + (i << DEV_BSHIFT),
	    bfs->start_sector + 1/*super block*/ + i);
}

bool
bfs_file_lookup(const struct bfs *bfs, const char *fname, int *start, int *end,
    size_t *size)
{
	struct bfs_inode *inode;
	struct bfs_dirent *dirent;

	if (!bfs_dirent_lookup_by_name(bfs, fname, &dirent))
		return false;
	if (!bfs_inode_lookup(bfs, dirent->inode, &inode))
		return false;

	if (start)
		*start = inode->start_sector + bfs->start_sector;
	if (end)
		*end = inode->end_sector + bfs->start_sector;
	if (size)
		*size = bfs_file_size(inode);

	DPRINTF(bfs->debug, "%s: %d + %d -> %d (%zd)\n",
	    fname, bfs->start_sector, inode->start_sector,
	    inode->end_sector, *size);

	return true;
}

bool
bfs_dirent_lookup_by_inode(const struct bfs *bfs, int inode,
    struct bfs_dirent **dirent)
{
	struct bfs_dirent *file;
	int i;

	for (file = bfs->dirent, i = 0; i < bfs->max_dirent; i++, file++)
		if (file->inode == inode)
			break;

	if (i == bfs->max_dirent)
		return false;

	*dirent = file;

	return true;
}

bool
bfs_dirent_lookup_by_name(const struct bfs *bfs, const char *fname,
    struct bfs_dirent **dirent)
{
	struct bfs_dirent *file;
	int i;

	for (file = bfs->dirent, i = 0; i < bfs->max_dirent; i++, file++)
		if ((file->inode != 0) &&
		    (strncmp(file->name, fname, BFS_FILENAME_MAXLEN) ==0))
			break;

	if (i == bfs->max_dirent)
		return false;

	*dirent = file;

	return true;
}

bool
bfs_inode_lookup(const struct bfs *bfs, ino_t n, struct bfs_inode **iinode)
{
	struct bfs_inode *inode;
	int i;

	for (inode = bfs->inode, i = 0; i < bfs->max_inode; i++, inode++)
		if (inode->number == n)
			break;

	if (i == bfs->max_inode)
		return false;

	*iinode = inode;

	return true;
}

int
bfs_inode_delete(struct bfs *bfs, ino_t ino)
{
	struct bfs_inode *inode;

	if (!bfs_inode_lookup(bfs, ino, &inode))
		return ENOENT;

	memset(inode, 0, sizeof *inode);
	bfs->n_inode--;

	bfs_writeback_inode(bfs, inode);
	DPRINTF(bfs->debug, "%s: %lld deleted.\n", __func__, (long long)ino);

	return 0;
}


size_t
bfs_file_size(const struct bfs_inode *inode)
{

	return inode->eof_offset_byte - inode->start_sector * DEV_BSIZE + 1;
}

STATIC int
bfs_inode_alloc(const struct bfs *bfs, struct bfs_inode **free_inode,
    int *free_inode_number, int *free_block)
{
	struct bfs_inode *jnode, *inode;
	int i, j, start;

	j = start = 0;
	inode = bfs->inode;
	jnode = 0;

	for (i = BFS_ROOT_INODE; i < bfs->max_inode; i++, inode++) {
		/* Steal i-node # */
		if (j == 0)
			j = i;

		/* Get free i-node */
		if (jnode == 0 && (inode->number == 0))
			jnode = inode;

		/* Get free i-node # and data block */
		if (inode->number != 0) {
			if (inode->end_sector > start)
				start = inode->end_sector;
			if (inode->number == j)
				j = 0;	/* conflict */
		}
	}
	start++;

	if (jnode ==  0) {
		DPRINTF(bfs->debug, "i-node full.\n");
		return ENOSPC;
	}

	if (start * DEV_BSIZE >= bfs->data_end) {
		DPRINTF(bfs->debug, "data block full.\n");
		/* compaction here ? */
		return ENOSPC;
	}
	if (free_inode)
		*free_inode = jnode;
	if (free_inode_number)
		*free_inode_number = j;
	if (free_block)
		*free_block = start;

	return 0;
}

void
bfs_inode_set_attr(const struct bfs *bfs, struct bfs_inode *inode,
    const struct bfs_fileattr *from)
{
	struct bfs_fileattr *to = &inode->attr;

	if (from != NULL) {
		if (from->uid != (uid_t)-1)
			to->uid = from->uid;
		if (from->gid != (gid_t)-1)
			to->gid = from->gid;
		if (from->mode != (mode_t)-1)
			to->mode = from->mode;
		if (from->atime != -1)
			to->atime = from->atime;
		if (from->ctime != -1)
			to->ctime = from->ctime;
		if (from->mtime != -1)
			to->mtime = from->mtime;
	}
	bfs_writeback_inode(bfs, inode);
}

STATIC bool
bfs_superblock_valid(const struct bfs_super_block *super)
{

	return super->header.magic == BFS_MAGIC;
}

bool
bfs_dump(const struct bfs *bfs)
{
	const struct bfs_super_block_header *h;
	const struct bfs_compaction *compaction;
	const struct bfs_inode *inode;
	struct bfs_dirent *file;
	int i, j, s, e;
	size_t bytes;

	if (!bfs_superblock_valid(bfs->super_block)) {
		DPRINTF(bfs->debug, "invalid bfs super block.\n");
		return false;
	}
	h = &bfs->super_block->header;
	compaction = &bfs->super_block->compaction;

	DPRINTF(bfs->debug, "super block %zdbyte, inode %zdbyte, dirent %zdbyte\n",
	    sizeof *bfs->super_block, sizeof *inode, sizeof *file);

	DPRINTF(bfs->debug, "magic=%x\n", h->magic);
	DPRINTF(bfs->debug, "data_start_byte=0x%x\n", h->data_start_byte);
	DPRINTF(bfs->debug, "data_end_byte=0x%x\n", h->data_end_byte);
	DPRINTF(bfs->debug, "from=%#x\n", compaction->from);
	DPRINTF(bfs->debug, "to=%#x\n", compaction->to);
	DPRINTF(bfs->debug, "from_backup=%#x\n", compaction->from_backup);
	DPRINTF(bfs->debug, "to_backup=%#x\n", compaction->to_backup);
	DPRINTF(bfs->debug, "fsname=%s\n", bfs->super_block->fsname);
	DPRINTF(bfs->debug, "volume=%s\n", bfs->super_block->volume);

	/* inode list */
	DPRINTF(bfs->debug, "[inode index list]\n");
	for (inode = bfs->inode, i = j = 0; i < bfs->max_inode; inode++, i++) {
		if (inode->number != 0) {
			const struct bfs_fileattr *attr = &inode->attr;
			DPRINTF(bfs->debug, "%3d  %8d %8d %8d (%d) ",
			    inode->number,
			    inode->eof_offset_byte -
			    (inode->start_sector * DEV_BSIZE) + 1,/* file size*/
			    inode->start_sector,
			    inode->end_sector, i);

			DPRINTF(bfs->debug, "%d %d %d %d %d %08x %08x %08x\n",
			    attr->type, attr->mode, attr->uid, attr->gid,
			    attr->nlink, attr->atime, attr->mtime, attr->ctime);
			j++;
		}
	}
	if (j != bfs->n_inode) {
		DPRINTF(bfs->debug, "inconsistent cached data. (i-node)\n");
		return false;
	}
	DPRINTF(bfs->debug, "total %d i-node.\n", j);

	/* file list */
	DPRINTF(bfs->debug, "[dirent index list]\n");
	DPRINTF(bfs->debug, "%d file entries.\n", bfs->max_dirent);
	file = bfs->dirent;
	for (i = j = 0; i < bfs->max_dirent; i++, file++) {
		if (file->inode != 0) {
			if (bfs_file_lookup(bfs, file->name, &s, &e, &bytes))
				DPRINTF(bfs->debug, "%3d %14s %8d %8d %8zd\n",
				    file->inode, file->name, s, e, bytes);
			j++;
		}
	}
	if (j != bfs->n_dirent) {
		DPRINTF(bfs->debug, "inconsistent cached data. (dirent)\n");
		return false;
	}
	DPRINTF(bfs->debug, "%d files.\n", j);

	return true;
}

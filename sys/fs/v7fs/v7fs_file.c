/*	$NetBSD: v7fs_file.c,v 1.6 2014/12/29 15:28:58 hannken Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: v7fs_file.c,v 1.6 2014/12/29 15:28:58 hannken Exp $");
#if defined _KERNEL_OPT
#include "opt_v7fs.h"
#endif

#include <sys/param.h>
#ifdef _KERNEL
#include <sys/systm.h>
#else
#include <stdio.h>
#include <string.h>
#include <errno.h>
#endif

#include "v7fs.h"
#include "v7fs_impl.h"
#include "v7fs_endian.h"
#include "v7fs_inode.h"
#include "v7fs_dirent.h"
#include "v7fs_file.h"
#include "v7fs_datablock.h"

#ifdef V7FS_FILE_DEBUG
#define	DPRINTF(fmt, args...)	printf("%s: " fmt, __func__, ##args)
#else
#define	DPRINTF(fmt, args...)	((void)0)
#endif

static int lookup_subr(struct v7fs_self *, void *, v7fs_daddr_t, size_t);
static int remove_subr(struct v7fs_self *, void *, v7fs_daddr_t, size_t);

int
v7fs_file_lookup_by_name(struct v7fs_self *fs, struct v7fs_inode *parent_dir,
    const char *name, v7fs_ino_t *ino)
{
	char filename[V7FS_NAME_MAX + 1];
	char *q;
	int error;
	size_t len;

	if ((q = strchr(name, '/'))) {
		/* Zap following path. */
		len = MIN(V7FS_NAME_MAX, q - name);
		memcpy(filename, name, len);
		filename[len] = '\0';	/* '/' -> '\0' */
	} else {
		v7fs_dirent_filename(filename, name);
	}
	DPRINTF("%s(%s) dir=%d\n", filename, name, parent_dir->inode_number);

	struct v7fs_lookup_arg lookup_arg = { .name = filename,
					      .inode_number = 0 };
	if ((error = v7fs_datablock_foreach(fs, parent_dir, lookup_subr,
		    &lookup_arg)) != V7FS_ITERATOR_BREAK) {
		DPRINTF("not found.\n");
		return ENOENT;
	}

	*ino = lookup_arg.inode_number;
	DPRINTF("done. ino=%d\n", *ino);

	return 0;
}

static int
lookup_subr(struct v7fs_self *fs, void *ctx, v7fs_daddr_t blk, size_t sz)
{
	struct v7fs_lookup_arg *p = (struct v7fs_lookup_arg *)ctx;
	struct v7fs_dirent *dir;
	const char *name = p->name;
	void *buf;
	size_t i, n;
	int ret = 0;

	if (!(buf = scratch_read(fs, blk)))
		return EIO;

	dir = (struct v7fs_dirent *)buf;
	n = sz / sizeof(*dir);
	v7fs_dirent_endian_convert(fs, dir, n);

	for (i = 0; i < n; i++, dir++) {
		if (dir->inode_number < 1) {
			DPRINTF("*** bad inode #%d ***\n", dir->inode_number);
			continue;
		}

		if (strncmp((const char *)dir->name, name, V7FS_NAME_MAX) == 0)
		{
			p->inode_number = dir->inode_number;
			ret =  V7FS_ITERATOR_BREAK; /* found */
			break;
		}
	}
	scratch_free(fs, buf);

	return ret;
}

int
v7fs_file_allocate(struct v7fs_self *fs, struct v7fs_inode *parent_dir,
    const char *srcname, struct v7fs_fileattr *attr, v7fs_ino_t *ino)
{
	struct v7fs_inode inode;
	char filename[V7FS_NAME_MAX + 1];
	struct v7fs_dirent *dir;
	int error;

	/* Truncate filename. */
	v7fs_dirent_filename(filename, srcname);
	DPRINTF("%s(%s)\n", filename, srcname);

	/* Check filename. */
	if (v7fs_file_lookup_by_name(fs, parent_dir, filename, ino) == 0) {
		DPRINTF("%s exists\n", filename);
		return EEXIST;
	}

	/* Get new inode. */
	if ((error = v7fs_inode_allocate(fs, ino)))
		return error;

	/* Set initial attribute. */
	memset(&inode, 0, sizeof(inode));
	inode.inode_number = *ino;
	inode.mode = attr->mode;
	inode.uid = attr->uid;
	inode.gid = attr->gid;
	if (attr->ctime)
		inode.ctime = attr->ctime;
	if (attr->mtime)
		inode.mtime = attr->mtime;
	if (attr->atime)
		inode.atime = attr->atime;

	switch (inode.mode & V7FS_IFMT)	{
	default:
		DPRINTF("Can't allocate %o type.\n", inode.mode);
		v7fs_inode_deallocate(fs, *ino);
		return EINVAL;
	case V7FS_IFCHR:
		/* FALLTHROUGH */
	case V7FS_IFBLK:
		inode.nlink = 1;
		inode.device = attr->device;
		inode.addr[0] = inode.device;
		break;
	case V7FSBSD_IFFIFO:
		/* FALLTHROUGH */
	case V7FSBSD_IFSOCK:
		/* FALLTHROUGH */
	case V7FSBSD_IFLNK:
		/* FALLTHROUGH */
	case V7FS_IFREG:
		inode.nlink = 1;
		break;
	case V7FS_IFDIR:
		inode.nlink = 2;	/* . + .. */
		if ((error = v7fs_datablock_expand(fs, &inode, sizeof(*dir) * 2
		    ))) {
			v7fs_inode_deallocate(fs, *ino);
			return error;
		}
		v7fs_daddr_t blk = inode.addr[0];
		void *buf;
		if (!(buf = scratch_read(fs, blk))) {
			v7fs_inode_deallocate(fs, *ino);
			return EIO;
		}
		dir = (struct v7fs_dirent *)buf;
		strcpy(dir[0].name, ".");
		dir[0].inode_number = V7FS_VAL16(fs, *ino);
		strcpy(dir[1].name, "..");
		dir[1].inode_number = V7FS_VAL16(fs, parent_dir->inode_number);
		if (!fs->io.write(fs->io.cookie, buf, blk)) {
			scratch_free(fs, buf);
			return EIO;
		}
		scratch_free(fs, buf);
		break;
	}

	v7fs_inode_writeback(fs, &inode);

	/* Link this inode to parent directory. */
	if ((error = v7fs_directory_add_entry(fs, parent_dir, *ino, filename)))
	{
		DPRINTF("can't add dirent.\n");
		return error;
	}

	return 0;
}

int
v7fs_file_deallocate(struct v7fs_self *fs, struct v7fs_inode *parent_dir,
    const char *name)
{
	v7fs_ino_t ino;
	struct v7fs_inode inode;
	int error;

	DPRINTF("%s\n", name);
	if ((error = v7fs_file_lookup_by_name(fs, parent_dir, name, &ino))) {
		DPRINTF("no such a file: %s\n", name);
		return error;
	}
	DPRINTF("%s->#%d\n", name, ino);
	if ((error = v7fs_inode_load(fs, &inode, ino)))
		return error;

	if (v7fs_inode_isdir(&inode)) {
		char filename[V7FS_NAME_MAX + 1];
		v7fs_dirent_filename(filename, name);
		/* Check parent */
		if (strncmp(filename, "..", V7FS_NAME_MAX) == 0) {
			DPRINTF("can not remove '..'\n");
			return EINVAL; /* t_vnops rename_dotdot */
		}
		/* Check empty */
		if (v7fs_inode_filesize(&inode) !=
		    sizeof(struct v7fs_dirent) * 2 /*"." + ".."*/) {
			DPRINTF("directory not empty.\n");
			return ENOTEMPTY;/* t_vnops dir_noempty, rename_dir(6)*/
		}
		error = v7fs_datablock_size_change(fs, 0, &inode);
		if (error)
			return error;
		inode.nlink = 0;	/* remove this. */
	} else {
		/* Decrement reference count. */
		--inode.nlink;	/* regular file. */
		DPRINTF("%s nlink=%d\n", name, inode.nlink);
	}


	if ((error = v7fs_directory_remove_entry(fs, parent_dir, name)))
		return error;
	DPRINTF("remove dirent\n");

	v7fs_inode_writeback(fs, &inode);

	return 0;
}

int
v7fs_directory_add_entry(struct v7fs_self *fs, struct v7fs_inode *parent_dir,
    v7fs_ino_t ino, const char *srcname)
{
	struct v7fs_inode inode;
	struct v7fs_dirent *dir;
	int error = 0;
	v7fs_daddr_t blk;
	void *buf;
	char filename[V7FS_NAME_MAX + 1];

	/* Truncate filename. */
	v7fs_dirent_filename(filename, srcname);
	DPRINTF("%s(%s) %d\n", filename, srcname, ino);

	/* Target inode */
	if ((error = v7fs_inode_load(fs, &inode, ino)))
		return error;

	/* Expand datablock. */
	if ((error = v7fs_datablock_expand(fs, parent_dir, sizeof(*dir))))
		return error;

	/* Read last entry. */
	if (!(blk = v7fs_datablock_last(fs, parent_dir,
	    v7fs_inode_filesize(parent_dir))))
		return EIO;

	/* Load dirent block. This vnode(parent dir) is locked by VFS layer. */
	if (!(buf = scratch_read(fs, blk)))
		return EIO;

	size_t sz = v7fs_inode_filesize(parent_dir);
	sz = V7FS_RESIDUE_BSIZE(sz);	/* last block payload. */
	int n = sz / sizeof(*dir) - 1;
	/* Add dirent. */
	dir = (struct v7fs_dirent *)buf;
	dir[n].inode_number = V7FS_VAL16(fs, ino);
	memcpy((char *)dir[n].name, filename, V7FS_NAME_MAX);
	/* Write back datablock */
	if (!fs->io.write(fs->io.cookie, buf, blk))
		error = EIO;
	scratch_free(fs, buf);

	if (v7fs_inode_isdir(&inode)) {
		parent_dir->nlink++;
		v7fs_inode_writeback(fs, parent_dir);
	}

	DPRINTF("done. (dirent size=%dbyte)\n", parent_dir->filesize);

	return error;
}

int
v7fs_directory_remove_entry(struct v7fs_self *fs, struct v7fs_inode *parent_dir,
    const char *name)
{
	struct v7fs_inode inode;
	int error;
	struct v7fs_dirent lastdirent;
	v7fs_daddr_t lastblk;
	size_t sz, lastsz;
	v7fs_off_t pos;
	void *buf;

	/* Setup replaced entry. */
	sz = parent_dir->filesize;
	lastblk = v7fs_datablock_last(fs, parent_dir,
	    v7fs_inode_filesize(parent_dir));
	lastsz = V7FS_RESIDUE_BSIZE(sz);
	pos = lastsz - sizeof(lastdirent);

	if (!(buf = scratch_read(fs, lastblk)))
		return EIO;
	lastdirent = *((struct v7fs_dirent *)((uint8_t *)buf + pos));
	scratch_free(fs, buf);
	DPRINTF("last dirent=%d %s pos=%d\n",
	    V7FS_VAL16(fs, lastdirent.inode_number), lastdirent.name, pos);

	struct v7fs_lookup_arg lookup_arg =
	    { .name = name, .replace = &lastdirent/*disk endian */ };
	/* Search entry that removed. replace it to last dirent. */
	if ((error = v7fs_datablock_foreach(fs, parent_dir, remove_subr,
	    &lookup_arg)) != V7FS_ITERATOR_BREAK)
		return ENOENT;

	/* Contract dirent entries. */
	v7fs_datablock_contract(fs, parent_dir, sizeof(lastdirent));
	DPRINTF("done. (dirent size=%dbyte)\n", parent_dir->filesize);

	/* Target inode */
	if ((error = v7fs_inode_load(fs, &inode, lookup_arg.inode_number)))
		return error;

	if (v7fs_inode_isdir(&inode)) {
		parent_dir->nlink--;
		v7fs_inode_writeback(fs, parent_dir);
	}

	return 0;
}

static int
remove_subr(struct v7fs_self *fs, void *ctx, v7fs_daddr_t blk, size_t sz)
{
	struct v7fs_lookup_arg *p = (struct v7fs_lookup_arg *)ctx;
	struct v7fs_dirent *dir;
	void *buf;
	size_t i;
	int ret = 0;

	DPRINTF("match start blk=%x\n", blk);
	if (!(buf = scratch_read(fs, blk)))
		return EIO;

	dir = (struct v7fs_dirent *)buf;

	for (i = 0; i < sz / sizeof(*dir); i++, dir++) {
		DPRINTF("%d\n", V7FS_VAL16(fs, dir->inode_number));
		if (strncmp(p->name,
			(const char *)dir->name, V7FS_NAME_MAX) == 0) {
			p->inode_number = V7FS_VAL16(fs, dir->inode_number);
			/* Replace to last dirent. */
			*dir = *(p->replace); /* disk endian */
			/* Write back. */
			if (!fs->io.write(fs->io.cookie, buf, blk))
				ret = EIO;
			else
				ret = V7FS_ITERATOR_BREAK;
			break;
		}
	}
	scratch_free(fs, buf);

	return ret;
}

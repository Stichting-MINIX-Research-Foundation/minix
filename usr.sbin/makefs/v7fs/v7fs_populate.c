/*	$NetBSD: v7fs_populate.c,v 1.3 2011/08/10 11:31:49 uch Exp $	*/

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
#if defined(__RCSID) && !defined(__lint)
__RCSID("$NetBSD: v7fs_populate.c,v 1.3 2011/08/10 11:31:49 uch Exp $");
#endif	/* !__lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>

#if !HAVE_NBTOOL_CONFIG_H
#include <sys/mount.h>
#endif

#include "makefs.h"
#include "v7fs.h"
#include "v7fs_impl.h"
#include "v7fs_inode.h"
#include "v7fs_superblock.h"
#include "v7fs_datablock.h"
#include "v7fs_endian.h"
#include "v7fs_file.h"
#include "v7fs_makefs.h"
#include "newfs_v7fs.h"

#define	VPRINTF(fmt, args...)	{ if (v7fs_newfs_verbose) printf(fmt, ##args); }

static void
attr_setup(fsnode *node, struct v7fs_fileattr *attr)
{
	struct stat *st = &node->inode->st;

	attr->mode = node->type | st->st_mode;
	attr->uid = st->st_uid;
	attr->gid = st->st_gid;
	attr->device = 0;
	attr->ctime = st->st_ctime;
	attr->atime = st->st_atime;
	attr->mtime = st->st_mtime;
}

static int
allocate(struct v7fs_self *fs, struct v7fs_inode *parent_inode, fsnode *node,
    dev_t dev, struct v7fs_inode *inode)
{
	int error;
	v7fs_ino_t ino;
	struct v7fs_fileattr attr;

	memset(inode, 0, sizeof(*inode));

	attr_setup(node, &attr);
	attr.device = dev;
	if ((error = v7fs_file_allocate(fs, parent_inode, node->name, &attr,
	    &ino))) {
		errno = error;
		warn("%s", node->name);
		return error;
	}
	node->inode->ino = ino;
	node->inode->flags |= FI_ALLOCATED;
	if ((error = v7fs_inode_load(fs, inode, ino))) {
		errno = error;
		warn("%s", node->name);
		return error;
	}

	return 0;
}

struct copy_arg {
	int fd;
	uint8_t buf[V7FS_BSIZE];
};

static int
copy_subr(struct v7fs_self *fs, void *ctx, v7fs_daddr_t blk, size_t sz)
{
	struct copy_arg *p = ctx;

	if (read(p->fd, p->buf, sz) != (ssize_t)sz) {
		return V7FS_ITERATOR_ERROR;
	}

	if (!fs->io.write(fs->io.cookie, p->buf, blk)) {
		errno = EIO;
		return V7FS_ITERATOR_ERROR;
	}
	progress(0);

	return 0;
}

static int
file_copy(struct v7fs_self *fs, struct v7fs_inode *parent_inode, fsnode *node,
	const char *filepath)
{
	struct v7fs_inode inode;
	const char *errmsg;
	fsinode *fnode = node->inode;
	int error = 0;
	int fd;

	/* Check hard-link */
	if ((fnode->nlink > 1) && (fnode->flags & FI_ALLOCATED)) {
		if ((error = v7fs_inode_load(fs, &inode, fnode->ino))) {
			errmsg = "inode load";
			goto err_exit;
		}
		if ((error = v7fs_file_link(fs, parent_inode, &inode,
			    node->name))) {
			errmsg = "hard link";
			goto err_exit;
		}
		return 0;
	}

	/* Allocate file */
	if ((error = allocate(fs, parent_inode, node, 0, &inode))) {
		errmsg = "file allocate";
		goto err_exit;
	}
	if ((error = v7fs_datablock_expand(fs, &inode, fnode->st.st_size))) {
		errmsg = "datablock expand";
		goto err_exit;
	}

	/* Data copy */
	if ((fd = open(filepath, O_RDONLY)) == -1) {
		error = errno;
		errmsg = "source file";
		goto err_exit;
	}

	error = v7fs_datablock_foreach(fs, &inode, copy_subr,
	    &(struct copy_arg){ .fd = fd });
	if (error != V7FS_ITERATOR_END) {
		errmsg = "data copy";
		close(fd);
		goto err_exit;
	} else {
		error = 0;
	}
	close(fd);

	return error;

err_exit:
	errno = error;
	warn("%s %s", node->name, errmsg);

	return error;
}

static int
populate_walk(struct v7fs_self *fs, struct v7fs_inode *parent_inode,
    fsnode *root, char *dir)
{
	fsnode *cur;
	char *mydir = dir + strlen(dir);
	char srcpath[MAXPATHLEN + 1];
	struct v7fs_inode inode;
	int error = 0;
	bool failed = false;

	for (cur = root; cur != NULL; cur = cur->next) {
		switch (cur->type & S_IFMT) {
		default:
			VPRINTF("%x\n", cur->flags & S_IFMT);
			break;
		case S_IFCHR:
			/*FALLTHROUGH*/
		case S_IFBLK:
			if ((error = allocate(fs, parent_inode, cur,
			    cur->inode->st.st_rdev, &inode))) {
				errno = error;
				warn("%s", cur->name);
			}
			break;
		case S_IFDIR:
			if (!cur->child)	/*'.'*/
				break;
			/* Allocate this directory. */
			if ((error = allocate(fs, parent_inode, cur, 0,
			    &inode))) {
				errno = error;
				warn("%s", cur->name);
			} else {
				/* Populate children. */
				mydir[0] = '/';
				strcpy(&mydir[1], cur->name);
				error = populate_walk(fs, &inode, cur->child,
				    dir);
				mydir[0] = '\0';
			}
			break;
		case S_IFREG:
			snprintf(srcpath, sizeof(srcpath), "%s/%s", dir,
			    cur->name);
			error = file_copy(fs, parent_inode, cur, srcpath);
			break;
		case S_IFLNK:
			if ((error = allocate(fs, parent_inode, cur, 0,
			    &inode))) {
				errno = error;
				warn("%s", cur->name);
			} else {
				v7fs_file_symlink(fs, &inode, cur->symlink);
			}
			break;
		}
		if (error)
			failed = true;
	}

	return failed ? 2 : 0;
}

int
v7fs_populate(const char *dir, fsnode *root, fsinfo_t *fsopts,
    const struct v7fs_mount_device *device)
{
	v7fs_opt_t *v7fs_opts = fsopts->fs_specific;
	static char path[MAXPATHLEN + 1];
	struct v7fs_inode root_inode;
	struct v7fs_self *fs;
	int error;

	if ((error = v7fs_io_init(&fs, device, V7FS_BSIZE))) {
		errno = error;
		warn("I/O setup failed.");
		return error;
	}
	fs->endian = device->endian;
	v7fs_endian_init(fs);

	if ((error = v7fs_superblock_load(fs))) {
		errno = error;
		warn("Can't load superblock.");
		return error;
	}
	fsopts->superblock = &fs->superblock;	/* not used. */

	if ((error = v7fs_inode_load(fs, &root_inode, V7FS_ROOT_INODE))) {
		errno = error;
		warn("Can't load root inode.");
		return error;
	}

	progress(&(struct progress_arg){ .label = "populate", .tick =
	    v7fs_opts->npuredatablk / PROGRESS_BAR_GRANULE });

	strncpy(path, dir, sizeof(path));
	error = populate_walk(fs, &root_inode, root, path);

	v7fs_inode_writeback(fs, &root_inode);
	v7fs_superblock_writeback(fs);

	return error;
}

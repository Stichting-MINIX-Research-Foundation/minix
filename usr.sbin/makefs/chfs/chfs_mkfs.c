/*-
 * Copyright (c) 2012 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * Copyright (c) 2012 Tamas Toth <ttoth@inf.u-szeged.hu>
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by the Department of Software Engineering, University of Szeged, Hungary
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/param.h>
#include <sys/stat.h>

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <util.h>
#include <unistd.h>

#include "makefs.h"
#include "chfs_makefs.h"

#include "media.h"
#include "ebh.h"

#include "chfs/chfs_mkfs.h"

static uint32_t img_ofs = 0;
static uint64_t version = 0;
static uint64_t max_serial = 0;
static int lebnumber = 0;

static const unsigned char ffbuf[16] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

static void
buf_write(fsinfo_t *fsopts, const void *buf, size_t len)
{
	ssize_t retval;
	const char *charbuf = buf;

	while (len > 0) {
		retval = write(fsopts->fd, charbuf, len);

		if (retval == -1) {
			err(EXIT_FAILURE, "ERROR while writing");
		}

		len -= retval;
		charbuf += retval;
		img_ofs += retval;
	}
}

void
padblock(fsinfo_t *fsopts)
{
	chfs_opt_t *chfs_opts = fsopts->fs_specific;
	while (img_ofs % chfs_opts->eraseblock) {
		buf_write(fsopts, ffbuf, MIN(sizeof(ffbuf),
		    chfs_opts->eraseblock - (img_ofs % chfs_opts->eraseblock)));
	}
}

static void
padword(fsinfo_t *fsopts)
{
	if (img_ofs % 4) {
		buf_write(fsopts, ffbuf, 4 - img_ofs % 4);
	}
}

static void
pad_block_if_less_than(fsinfo_t *fsopts, int req)
{
	chfs_opt_t *chfs_opts = fsopts->fs_specific;
	if ((img_ofs % chfs_opts->eraseblock) + req >
	    (uint32_t)chfs_opts->eraseblock) {
		padblock(fsopts);
		write_eb_header(fsopts);
	}
}

void
write_eb_header(fsinfo_t *fsopts)
{
	chfs_opt_t *opts;
	struct chfs_eb_hdr ebhdr;
	char *buf;

	opts = fsopts->fs_specific;

#define MINSIZE MAX(MAX(CHFS_EB_EC_HDR_SIZE, CHFS_EB_HDR_NOR_SIZE), \
    CHFS_EB_HDR_NAND_SIZE)
	if ((uint32_t)opts->pagesize < MINSIZE)
		errx(EXIT_FAILURE, "pagesize cannot be less than %zu", MINSIZE);
	buf = emalloc(opts->pagesize);

	ebhdr.ec_hdr.magic = htole32(CHFS_MAGIC_BITMASK);
	ebhdr.ec_hdr.erase_cnt = htole32(1);
	ebhdr.ec_hdr.crc_ec = htole32(crc32(0,
	    (uint8_t *)&ebhdr.ec_hdr + 8, 4));

	memcpy(buf, &ebhdr.ec_hdr, CHFS_EB_EC_HDR_SIZE);
	memset(buf + CHFS_EB_EC_HDR_SIZE, 0xFF,
	    opts->pagesize - CHFS_EB_EC_HDR_SIZE);

	buf_write(fsopts, buf, opts->pagesize);

	memset(buf, 0xFF, opts->pagesize);

	if (opts->mediatype == TYPE_NAND) {
		ebhdr.u.nand_hdr.lid = htole32(lebnumber++);
		ebhdr.u.nand_hdr.serial = htole64(++(max_serial));
		ebhdr.u.nand_hdr.crc = htole32(crc32(0,
		    (uint8_t *)&ebhdr.u.nand_hdr + 4,
		    CHFS_EB_HDR_NAND_SIZE - 4));
		memcpy(buf, &ebhdr.u.nand_hdr, CHFS_EB_HDR_NAND_SIZE);
	} else {
		ebhdr.u.nor_hdr.lid = htole32(lebnumber++);
		ebhdr.u.nor_hdr.crc = htole32(crc32(0,
		    (uint8_t *)&ebhdr.u.nor_hdr + 4,
		    CHFS_EB_HDR_NOR_SIZE - 4));
		memcpy(buf, &ebhdr.u.nor_hdr, CHFS_EB_HDR_NOR_SIZE);
	}

	buf_write(fsopts, buf, opts->pagesize);
	free(buf);
}

void
write_vnode(fsinfo_t *fsopts, fsnode *node)
{
	struct chfs_flash_vnode fvnode;
	memset(&fvnode, 0, sizeof(fvnode));

	fvnode.magic = htole16(CHFS_FS_MAGIC_BITMASK);
	fvnode.type = htole16(CHFS_NODETYPE_VNODE);
	fvnode.length = htole32(CHFS_PAD(sizeof(fvnode)));
	fvnode.hdr_crc = htole32(crc32(0, (uint8_t *)&fvnode,
	    CHFS_NODE_HDR_SIZE - 4));
	fvnode.vno = htole64(node->inode->ino);
	fvnode.version = htole64(version++);
	fvnode.mode = htole32(node->inode->st.st_mode);
	fvnode.dn_size = htole32(node->inode->st.st_size);
	fvnode.atime = htole32(node->inode->st.st_atime);
	fvnode.ctime = htole32(node->inode->st.st_ctime);
	fvnode.mtime = htole32(node->inode->st.st_mtime);
	fvnode.gid = htole32(node->inode->st.st_uid);
	fvnode.uid = htole32(node->inode->st.st_gid);
	fvnode.node_crc = htole32(crc32(0, (uint8_t *)&fvnode,
	    sizeof(fvnode) - 4));

	pad_block_if_less_than(fsopts, sizeof(fvnode));
	buf_write(fsopts, &fvnode, sizeof(fvnode));
	padword(fsopts);
}

void
write_dirent(fsinfo_t *fsopts, fsnode *node)
{
	struct chfs_flash_dirent_node fdirent;
	char *name;

	name = emalloc(strlen(node->name));
	memcpy(name, node->name, strlen(node->name));

	memset(&fdirent, 0, sizeof(fdirent));
	fdirent.magic = htole16(CHFS_FS_MAGIC_BITMASK);
	fdirent.type = htole16(CHFS_NODETYPE_DIRENT);
	fdirent.length = htole32(CHFS_PAD(sizeof(fdirent) + strlen(name)));
	fdirent.hdr_crc = htole32(crc32(0, (uint8_t *)&fdirent,
	    CHFS_NODE_HDR_SIZE - 4));
	fdirent.vno = htole64(node->inode->ino);
	if (node->parent != NULL) {
		fdirent.pvno = htole64(node->parent->inode->ino);
	} else {
		fdirent.pvno = htole64(node->inode->ino);
	}

	fdirent.version = htole64(version++);
	fdirent.mctime = 0;
	fdirent.nsize = htole32(strlen(name));
	fdirent.dtype = htole32(IFTOCHT(node->type & S_IFMT));
	fdirent.name_crc = htole32(crc32(0, (uint8_t *)name, fdirent.nsize));
	fdirent.node_crc = htole32(crc32(0, (uint8_t *)&fdirent,
	    sizeof(fdirent) - 4));

	pad_block_if_less_than(fsopts, sizeof(fdirent) + fdirent.nsize);
	buf_write(fsopts, &fdirent, sizeof(fdirent));
	buf_write(fsopts, name, fdirent.nsize);
	padword(fsopts);
}

void
write_file(fsinfo_t *fsopts, fsnode *node, const char *dir)
{
	int fd;
	ssize_t len;
	char *name = node->name;
	chfs_opt_t *opts;
	unsigned char *buf;
	uint32_t fileofs = 0;

	opts = fsopts->fs_specific;
	buf = emalloc(opts->pagesize);
	if (node->type == S_IFREG || node->type == S_IFSOCK) {
		char *longname;
		if (asprintf(&longname, "%s/%s", dir, name) == 1)
			goto out;

		fd = open(longname, O_RDONLY, 0444);
		if (fd == -1)
			err(EXIT_FAILURE, "Cannot open `%s'", longname);

		while ((len = read(fd, buf, opts->pagesize))) {
			if (len < 0) {
				warn("ERROR while reading %s", longname);
				free(longname);
				free(buf);
				close(fd);
				return;
			}

			write_data(fsopts, node, buf, len, fileofs);
			fileofs += len;
		}
		free(longname);
		close(fd);
	} else if (node->type == S_IFLNK) {
		len = strlen(node->symlink);
		memcpy(buf, node->symlink, len);
		write_data(fsopts, node, buf, len, 0);
	} else if (node->type == S_IFCHR || node->type == S_IFBLK ||
		node->type == S_IFIFO) {
		len = sizeof(dev_t);
		memcpy(buf, &(node->inode->st.st_rdev), len);
		write_data(fsopts, node, buf, len, 0);
	}

	free(buf);
	return;
out:
	err(EXIT_FAILURE, "Memory allocation failed");
}

void
write_data(fsinfo_t *fsopts, fsnode *node, unsigned char *buf, size_t len,
    uint32_t ofs)
{
	struct chfs_flash_data_node fdata;

	memset(&fdata, 0, sizeof(fdata));
	if (len == 0) {
		return;
	}

	pad_block_if_less_than(fsopts, sizeof(fdata) + len);

	fdata.magic = htole16(CHFS_FS_MAGIC_BITMASK);
	fdata.type = htole16(CHFS_NODETYPE_DATA);
	fdata.length = htole32(CHFS_PAD(sizeof(fdata) + len));
	fdata.hdr_crc = htole32(crc32(0, (uint8_t *)&fdata,
	    CHFS_NODE_HDR_SIZE - 4));
	fdata.vno = htole64(node->inode->ino);
	fdata.data_length = htole32(len);
	fdata.offset = htole32(ofs);
	fdata.data_crc = htole32(crc32(0, (uint8_t *)buf, len));
	fdata.node_crc = htole32(crc32(0,
	    (uint8_t *)&fdata, sizeof(fdata) - 4));

	buf_write(fsopts, &fdata, sizeof(fdata));
	buf_write(fsopts, buf, len);
	padword(fsopts);
}

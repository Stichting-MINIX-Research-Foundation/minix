/*	$NetBSD: v7fs_estimate.c,v 1.3 2012/04/19 17:28:26 christos Exp $	*/

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
__RCSID("$NetBSD: v7fs_estimate.c,v 1.3 2012/04/19 17:28:26 christos Exp $");
#endif	/* !__lint */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#if !HAVE_NBTOOL_CONFIG_H
#include <sys/mount.h>	/*MAXPATHLEN */
#endif

#include "makefs.h"
#include "v7fs.h"
#include "v7fs_impl.h"
#include "v7fs_inode.h"
#include "v7fs_datablock.h"
#include "v7fs_makefs.h"

struct v7fs_geometry {
	v7fs_daddr_t ndatablock;
	v7fs_ino_t ninode;
	v7fs_daddr_t npuredatablk;
};

#define	VPRINTF(fmt, args...)	{ if (v7fs_newfs_verbose) printf(fmt, ##args); }

static int
v7fs_datablock_size(off_t sz, v7fs_daddr_t *nblk)
{
	struct v7fs_daddr_map map;
	int error = 0;

	if (sz == 0) {
		*nblk = 0;
		return 0;
	}

	if ((error = v7fs_datablock_addr(sz, &map))) {
		return error;
	}
	switch (map.level) {
	case 0:	/* Direct */
		*nblk = map.index[0] + 1;
		break;
	case 1:
		*nblk = V7FS_NADDR_DIRECT +	/*direct */
		    1/*addr[V7FS_NADDR_INDEX1]*/ + map.index[0] + 1;
		break;
	case 2:
		*nblk = V7FS_NADDR_DIRECT +	/*direct */
		    1/*addr[V7FS_NADDR_INDEX1]*/ +
		    V7FS_DADDR_PER_BLOCK +/*idx1 */
		    1/*addr[V7FS_NADDR_INDEX2]*/ +
		    map.index[0] + /* # of idx2 index block(filled) */
		    map.index[0] * V7FS_DADDR_PER_BLOCK + /* of its datablocks*/
		    1 + /*current idx2 indexblock */
		    map.index[1] + 1;
		break;
	case 3:
		*nblk = V7FS_NADDR_DIRECT +	/*direct */
		    1/*addr[V7FS_NADDR_INDEX1]*/ +
		    V7FS_DADDR_PER_BLOCK +/*idx1 */
		    1/*addr[V7FS_NADDR_INDEX2]*/ +
		    V7FS_DADDR_PER_BLOCK + /* # of idx2 index block */
		    V7FS_DADDR_PER_BLOCK * V7FS_DADDR_PER_BLOCK +
		    /*idx2 datablk */
		    1/*addr[v7FS_NADDR_INDEX3*/ +
		    map.index[0] + /* # of lv1 index block(filled) */
		    map.index[0] * V7FS_DADDR_PER_BLOCK * V7FS_DADDR_PER_BLOCK +
		    1 +	/*lv1 */
		    map.index[1] + /* #of lv2 index block(filled) */
		    map.index[1] * V7FS_DADDR_PER_BLOCK + /*lv2 datablock */
		    1 + /* current lv2 index block */
		    map.index[2] + 1; /*filled datablock */
		break;
	default:
		*nblk = 0;
		error = EINVAL;
		break;
	}

	return error;
}

static int
estimate_size_walk(fsnode *root, char *dir, struct v7fs_geometry *geom)
{
	fsnode *cur;
	int nentries;
	size_t pathlen = strlen(dir);
	char *mydir = dir + pathlen;
	fsinode *fnode;
	v7fs_daddr_t nblk;
	int n;
	off_t sz;

#if defined(__minix)
	/* LSC: -Werror=maybe-uninitialized, when compiling with -O3. */
	nblk = 0;
#endif /* defined(__minix) */
	for (cur = root, nentries = 0; cur != NULL; cur = cur->next,
		 nentries++, geom->ninode++) {
		switch (cur->type & S_IFMT) {
		default:
			break;
		case S_IFDIR:
			if (!cur->child)
				break;
			mydir[0] = '/';
			strncpy(&mydir[1], cur->name, MAXPATHLEN - pathlen);
			n = estimate_size_walk(cur->child, dir, geom);
			sz = (n + 1/*..*/) * sizeof(struct v7fs_dirent);
			v7fs_datablock_size(sz, &nblk);
			mydir[0] = '\0';
			geom->ndatablock += nblk;
			geom->npuredatablk +=
			    V7FS_ROUND_BSIZE(sz) >> V7FS_BSHIFT;
			break;
		case S_IFREG:
			fnode = cur->inode;
			if (!(fnode->flags & FI_SIZED)) { /*Skip hard-link */
				sz = fnode->st.st_size;
				v7fs_datablock_size(sz, &nblk);
				geom->ndatablock += nblk;
				geom->npuredatablk +=
				    V7FS_ROUND_BSIZE(sz) >> V7FS_BSHIFT;
				fnode->flags |= FI_SIZED;
			}

			break;
		case S_IFLNK:
			nblk = V7FSBSD_MAXSYMLINKLEN >> V7FS_BSHIFT;
			geom->ndatablock += nblk;
			geom->npuredatablk += nblk;
			break;
		}
	}

	return nentries;
}

static v7fs_daddr_t
calculate_fs_size(const struct v7fs_geometry *geom)
{
	v7fs_daddr_t fs_blk, ilist_blk;

	ilist_blk = V7FS_ROUND_BSIZE(geom->ninode *
	    sizeof(struct v7fs_inode_diskimage)) >> V7FS_BSHIFT;
	fs_blk = geom->ndatablock + ilist_blk + V7FS_ILIST_SECTOR;

	VPRINTF("datablock:%d ilistblock:%d total:%d\n", geom->ndatablock,
	    ilist_blk, fs_blk);

	return fs_blk;
}

static void
determine_fs_size(fsinfo_t *fsopts, struct v7fs_geometry *geom)
{
	v7fs_daddr_t nblk = geom->ndatablock;
	v7fs_daddr_t fsblk, n;
	int32_t	nfiles = geom->ninode;

	VPRINTF("size=%lld inodes=%lld fd=%d superb=%p onlyspec=%d\n",
	    (long long)fsopts->size, (long long)fsopts->inodes, fsopts->fd,
	    fsopts->superblock, fsopts->onlyspec);
	VPRINTF("minsize=%lld maxsize=%lld freefiles=%lld freefilepc=%d "
	    "freeblocks=%lld freeblockpc=%d sectorseize=%d\n",
	    (long long)fsopts->minsize, (long long)fsopts->maxsize,
	    (long long)fsopts->freefiles, fsopts->freefilepc,
	    (long long)fsopts->freeblocks,  fsopts->freeblockpc,
	    fsopts->sectorsize);

	if ((fsopts->sectorsize > 0) && (fsopts->sectorsize != V7FS_BSIZE))
		warnx("v7fs sector size is 512byte only. '-S %d' is ignored.",
		    fsopts->sectorsize);

	/* Free inode */
	if (fsopts->freefiles) {
		nfiles += fsopts->freefiles;
	} else if ((n = fsopts->freefilepc)) {
		nfiles += (nfiles * n) / (100 - n);
	}
	if (nfiles >= V7FS_INODE_MAX) {
		errx(EXIT_FAILURE, "# of files(%d) over v7fs limit(%d).",
		    nfiles, V7FS_INODE_MAX);
	}

	/* Free datablock */
	if (fsopts->freeblocks) {
		nblk += fsopts->freeblocks;
	} else if ((n = fsopts->freeblockpc)) {
		nblk += (nblk * n) / (100 - n);
	}

	/* Total size */
	geom->ndatablock = nblk;
	geom->ninode = nfiles;
	fsblk = calculate_fs_size(geom);

	if (fsblk >= V7FS_DADDR_MAX) {
		errx(EXIT_FAILURE, "filesystem size(%d) over v7fs limit(%d).",
		    fsblk, V7FS_DADDR_MAX);
	}

	n = fsopts->minsize >> V7FS_BSHIFT;
	if (fsblk < n)
		geom->ndatablock += (n - fsblk);

	n = fsopts->maxsize >> V7FS_BSHIFT;
	if (fsopts->maxsize > 0 && fsblk > n) {
		errx(EXIT_FAILURE, "# of datablocks %d > %d", fsblk, n);
	}
}

void
v7fs_estimate(const char *dir, fsnode *root, fsinfo_t *fsopts)
{
	v7fs_opt_t *v7fs_opts = fsopts->fs_specific;
	char path[MAXPATHLEN + 1];
	int ndir;
	off_t sz;
	v7fs_daddr_t nblk;
	struct v7fs_geometry geom;

#if defined(__minix)
	/* LSC: -Werror=maybe-uninitialized, when compiling with -O3. */
	nblk = 0;
#endif /* defined(__minix) */
	memset(&geom , 0, sizeof(geom));
	strncpy(path, dir, sizeof(path));

	/* Calculate strict size. */
	ndir = estimate_size_walk(root, path, &geom);
	sz = (ndir + 1/*..*/) * sizeof(struct v7fs_dirent);
	v7fs_datablock_size(sz, &nblk);
	geom.ndatablock += nblk;

	/* Consider options. */
	determine_fs_size(fsopts, &geom);

	fsopts->size = calculate_fs_size(&geom) << V7FS_BSHIFT;
	fsopts->inodes = geom.ninode;
	v7fs_opts->npuredatablk = geom.npuredatablk; /* for progress bar */
}

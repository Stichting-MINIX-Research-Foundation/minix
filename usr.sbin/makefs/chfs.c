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

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "makefs.h"
#include "chfs_makefs.h"

#include "chfs/chfs_mkfs.h"

static void chfs_validate(const char *, fsnode *, fsinfo_t *);
static int chfs_create_image(const char *, fsinfo_t *);
static int chfs_populate_dir(const char *, fsnode *, fsnode *, fsinfo_t *);


void
chfs_prep_opts(fsinfo_t *fsopts)
{
	chfs_opt_t *chfs_opts = ecalloc(1, sizeof(*chfs_opts));

	const option_t chfs_options[] = {
		{ 'p', "pagesize", &chfs_opts->pagesize, OPT_INT32,
		  1, INT_MAX, "page size" },
		{ 'e', "eraseblock", &chfs_opts->eraseblock, OPT_INT32,
		  1, INT_MAX, "eraseblock size" },
		{ 'm', "mediatype", &chfs_opts->mediatype, OPT_INT32,
		  0, 1, "type of the media, 0 (nor) or 1 (nand)" },
		{ .name = NULL }
	};

	chfs_opts->pagesize = -1;
	chfs_opts->eraseblock = -1;
	chfs_opts->mediatype = -1;

	fsopts->size = 0;
	fsopts->fs_specific = chfs_opts;
	fsopts->fs_options = copy_opts(chfs_options);
}

void
chfs_cleanup_opts(fsinfo_t *fsopts)
{
	free(fsopts->fs_specific);
	free(fsopts->fs_options);
}

int
chfs_parse_opts(const char *option, fsinfo_t *fsopts)
{

	assert(option != NULL);
	assert(fsopts != NULL);

	return set_option(fsopts->fs_options, option, NULL, 0) != -1;
}

void
chfs_makefs(const char *image, const char *dir, fsnode *root, fsinfo_t *fsopts)
{
	struct timeval	start;

	assert(image != NULL);
	assert(dir != NULL);
	assert(root != NULL);
	assert(fsopts != NULL);	

	TIMER_START(start);
	chfs_validate(dir, root, fsopts);
	TIMER_RESULTS(start, "chfs_validate");

	printf("Creating `%s'\n", image);
	TIMER_START(start);
	if (chfs_create_image(image, fsopts) == -1) {
		errx(EXIT_FAILURE, "Image file `%s' not created", image);
	}
	TIMER_RESULTS(start, "chfs_create_image");
	
	fsopts->curinode = CHFS_ROOTINO;
	root->inode->ino = CHFS_ROOTINO;

	printf("Populating `%s'\n", image);
	TIMER_START(start);
	write_eb_header(fsopts);
	if (!chfs_populate_dir(dir, root, root, fsopts)) {
		errx(EXIT_FAILURE, "Image file `%s' not populated", image);
	}
	TIMER_RESULTS(start, "chfs_populate_dir");

	padblock(fsopts);

	if (close(fsopts->fd) == -1) {
		err(EXIT_FAILURE, "Closing `%s'", image);
	}
	fsopts->fd = -1;

	printf("Image `%s' complete\n", image);
}

static void
chfs_validate(const char* dir, fsnode *root, fsinfo_t *fsopts)
{
	chfs_opt_t *chfs_opts;
	assert(dir != NULL);
	assert(root != NULL);
	assert(fsopts != NULL);

	chfs_opts = fsopts->fs_specific;

	if (chfs_opts->pagesize == -1) {
		chfs_opts->pagesize = DEFAULT_PAGESIZE;
	}
	if (chfs_opts->eraseblock == -1) {
		chfs_opts->eraseblock = DEFAULT_ERASEBLOCK;
	}
	if (chfs_opts->mediatype == -1) {
		chfs_opts->mediatype = DEFAULT_MEDIATYPE;
	}
}

static int
chfs_create_image(const char *image, fsinfo_t *fsopts)
{
	assert(image != NULL);
	assert(fsopts != NULL);
	
	if ((fsopts->fd = open(image, O_RDWR | O_CREAT | O_TRUNC, 0666)) == -1) {
		warn("Can't open `%s' for writing", image);
		return -1;
	}

	return fsopts->fd;
}

static int
chfs_populate_dir(const char *dir, fsnode *root, fsnode *parent,
    fsinfo_t *fsopts)
{
	fsnode *cur;
	char path[MAXPATHLEN + 1];

	assert(dir != NULL);
	assert(root != NULL);
	assert(fsopts != NULL);	
	
	for (cur = root->next; cur != NULL; cur = cur->next) {
		if ((cur->inode->flags & FI_ALLOCATED) == 0) {
			cur->inode->flags |= FI_ALLOCATED;
			if (cur != root) {
				fsopts->curinode++;
				cur->inode->ino = fsopts->curinode;
				cur->parent = parent;
			}
		}

		if (cur->inode->flags & FI_WRITTEN) {
			continue;	// hard link
		}
		cur->inode->flags |= FI_WRITTEN;

		write_vnode(fsopts, cur);
		write_dirent(fsopts, cur);
		if (!S_ISDIR(cur->type & S_IFMT)) {
			write_file(fsopts, cur, dir);
		}
	}
	
	for (cur = root; cur != NULL; cur = cur->next) {
		if (cur->child == NULL) {
			continue;
		}
		if ((size_t)snprintf(path, sizeof(path), "%s/%s", dir,
		    cur->name) >= sizeof(path)) {
			errx(EXIT_FAILURE, "Pathname too long");
		}
		if (!chfs_populate_dir(path, cur->child, cur, fsopts)) {
			return 0;
		}
	}

	return 1;
}


/* $NetBSD: mdsetimage.c,v 1.2 2010/11/06 16:03:23 uebayasi Exp $ */
/* from: NetBSD: mdsetimage.c,v 1.15 2001/03/21 23:46:48 cgd Exp $ */

/*
 * Copyright (c) 1996, 2002 Christopher G. Demetriou
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * <<Id: LICENSE_GC,v 1.1 2001/10/01 23:24:05 cgd Exp>>
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if !defined(lint)
__COPYRIGHT("@(#) Copyright (c) 1996\
 Christopher G. Demetriou.  All rights reserved.");
__RCSID("$NetBSD: mdsetimage.c,v 1.2 2010/11/06 16:03:23 uebayasi Exp $");
#endif /* not lint */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <bfd.h>

struct symbols {
	char *name;
	size_t offset;
};
#define	X_MD_ROOT_IMAGE	0
#define	X_MD_ROOT_SIZE	1

#define	CHUNKSIZE	(64 * 1024)

int		main(int, char *[]);
static void	usage(void) __attribute__((noreturn));
static int	find_md_root(bfd *, struct symbols symbols[]);

int	verbose;
int	extract;
int	setsize;

static const char *progname;
#undef setprogname
#define	setprogname(x)	(void)(progname = (x))
#undef getprogname
#define	getprogname()	(progname)

int
main(int argc, char *argv[])
{
	int ch, kfd, fsfd, rv;
	struct stat ksb, fssb;
	size_t md_root_offset, md_root_size_offset;
	u_int32_t md_root_size;
	const char *kfile, *fsfile;
	char *mappedkfile;
	char *bfdname = NULL;
	bfd *abfd;
	ssize_t left_to_copy;
	struct symbols md_root_symbols[3] = { { 0 } };

	md_root_symbols[X_MD_ROOT_IMAGE].name = "_md_root_image";
	md_root_symbols[X_MD_ROOT_SIZE].name = "_md_root_size";

	setprogname(argv[0]);

	while ((ch = getopt(argc, argv, "I:S:b:svx")) != -1)
		switch (ch) {
		case 'I':
			md_root_symbols[X_MD_ROOT_IMAGE].name = optarg;
			break;
		case 'S':
			md_root_symbols[X_MD_ROOT_SIZE].name = optarg;
			break;
		case 'b':
			bfdname = optarg;
			break;
		case 's':
			setsize = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'x':
			extract = 1;
			break;
		case '?':
		default:
			usage();
	}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();
	kfile = argv[0];
	fsfile = argv[1];

	if (extract) {
		if ((kfd = open(kfile, O_RDONLY, 0))  == -1)
			err(1, "open %s", kfile);
	} else {
		if ((kfd = open(kfile, O_RDWR, 0))  == -1)
			err(1, "open %s", kfile);
	}

	bfd_init();
	if ((abfd = bfd_fdopenr(kfile, bfdname, kfd)) == NULL) {
		bfd_perror("open");
		exit(1);
	}
	if (!bfd_check_format(abfd, bfd_object)) {
		bfd_perror("check format");
		exit(1);
	}

	if (find_md_root(abfd, md_root_symbols) != 0)
		errx(1, "could not find symbols in %s", kfile);
	if (verbose)
		fprintf(stderr, "got symbols from %s\n", kfile);

	if (fstat(kfd, &ksb) == -1)
		err(1, "fstat %s", kfile);
	if (ksb.st_size != (size_t)ksb.st_size)
		errx(1, "%s too big to map", kfile);

	if ((mappedkfile = mmap(NULL, ksb.st_size, PROT_READ,
	    MAP_FILE | MAP_PRIVATE, kfd, 0)) == (caddr_t)-1)
		err(1, "mmap %s", kfile);
	if (verbose)
		fprintf(stderr, "mapped %s\n", kfile);

	md_root_offset = md_root_symbols[X_MD_ROOT_IMAGE].offset;
	md_root_size_offset = md_root_symbols[X_MD_ROOT_SIZE].offset;
	md_root_size = bfd_get_32(abfd, &mappedkfile[md_root_size_offset]);

	munmap(mappedkfile, ksb.st_size);

	if (extract) {
		if ((fsfd = open(fsfile, O_WRONLY|O_CREAT, 0777)) == -1)
			err(1, "open %s", fsfile);
		left_to_copy = md_root_size;
	} else {
		if ((fsfd = open(fsfile, O_RDONLY, 0)) == -1)
			err(1, "open %s", fsfile);
		if (fstat(fsfd, &fssb) == -1)
			err(1, "fstat %s", fsfile);
		if (fssb.st_size != (size_t)fssb.st_size)
			errx(1, "fs image is too big");
		if (fssb.st_size > md_root_size)
			errx(1, "fs image (%lld bytes) too big for buffer (%lu bytes)",
			    (long long)fssb.st_size, (unsigned long)md_root_size);
		left_to_copy = fssb.st_size;
	}

	if (verbose)
		fprintf(stderr, "copying image %s %s %s\n", fsfile,
		    (extract ? "from" : "into"), kfile);

	if (lseek(kfd, md_root_offset, SEEK_SET) != md_root_offset)
		err(1, "seek %s", kfile);
	while (left_to_copy > 0) {
		char buf[CHUNKSIZE];
		ssize_t todo;
		int rfd;
		int wfd;
		const char *rfile;
		const char *wfile;
		if (extract) {
			rfd = kfd;
			rfile = kfile;
			wfd = fsfd;
			wfile = fsfile;
		} else {
			rfd = fsfd;
			rfile = fsfile;
			wfd = kfd;
			wfile = kfile;
		}

		todo = (left_to_copy > CHUNKSIZE) ? CHUNKSIZE : left_to_copy;
		if ((rv = read(rfd, buf, todo)) != todo) {
			if (rv == -1)
				err(1, "read %s", rfile);
			else
				errx(1, "unexpected EOF reading %s", rfile);
		}
		if ((rv = write(wfd, buf, todo)) != todo) {
			if (rv == -1)
				err(1, "write %s", wfile);
			else
				errx(1, "short write writing %s", wfile);
		}
		left_to_copy -= todo;
	}
	if (verbose)
		fprintf(stderr, "done copying image\n");
	if (setsize && !extract) {
		char buf[sizeof(uint32_t)];

		if (verbose)
			fprintf(stderr, "setting md_root_size to %llu\n",
			    (unsigned long long) fssb.st_size);
		if (lseek(kfd, md_root_size_offset, SEEK_SET) !=
		    md_root_size_offset)
			err(1, "seek %s", kfile);
		bfd_put_32(abfd, fssb.st_size, buf);
		if (write(kfd, buf, sizeof(buf)) != sizeof(buf))
			err(1, "write %s", kfile);
	}

	close(fsfd);
	close(kfd);

	if (verbose)
		fprintf(stderr, "exiting\n");

	bfd_close_all_done(abfd);
	exit(0);
}

static void
usage(void)
{
	const char **list;

	fprintf(stderr,
	    "usage: %s [-svx] [-b bfdname] kernel image\n",
	    getprogname());
	fprintf(stderr, "supported targets:");
	for (list = bfd_target_list(); *list != NULL; list++)
		fprintf(stderr, " %s", *list);
	fprintf(stderr, "\n");
	exit(1);
}

static int
find_md_root(bfd *abfd, struct symbols symbols[])
{
	long i;
	long storage_needed;
	long number_of_symbols;
	asymbol **symbol_table = NULL;
	struct symbols *s;

	storage_needed = bfd_get_symtab_upper_bound(abfd);
	if (storage_needed <= 0)
		return (1);

	symbol_table = (asymbol **)malloc(storage_needed);
	if (symbol_table == NULL)
		return (1);

	number_of_symbols = bfd_canonicalize_symtab(abfd, symbol_table);
	if (number_of_symbols <= 0) {
		free(symbol_table);
		return (1);
	}

	for (i = 0; i < number_of_symbols; i++) {
		for (s = symbols; s->name != NULL; s++) {
			const char *sym = symbol_table[i]->name;

			/*
			 * match symbol prefix '_' or ''.
			 */
			if (!strcmp(s->name, sym) ||
			    !strcmp(s->name + 1, sym)) {
				s->offset =
				    (size_t)(symbol_table[i]->section->filepos
				    + symbol_table[i]->value);
			}
		}
	}

	free(symbol_table);

	for (s = symbols; s->name != NULL; s++) {
		if (s->offset == 0)
			return (1);
	}

	return (0);
}

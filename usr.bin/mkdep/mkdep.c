/* $NetBSD: mkdep.c,v 1.44 2015/06/16 22:54:10 christos Exp $ */

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matthias Scheler.
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
#if !defined(lint)
__COPYRIGHT("@(#) Copyright (c) 1999 The NetBSD Foundation, Inc.\
 All rights reserved.");
__RCSID("$NetBSD: mkdep.c,v 1.44 2015/06/16 22:54:10 christos Exp $");
#endif /* not lint */

#include <sys/mman.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <locale.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "findcc.h"

typedef struct opt opt_t;
struct opt {
	opt_t	*left;
	opt_t	*right;
	int	len;
	int	count;
	char	name[4];
};

typedef struct suff_list {
	size_t	len;
	char	*suff;
	struct suff_list *next;
} suff_list_t;

/* tree of includes for -o processing */
static opt_t *opt;
static int width;
static int verbose;

#define DEFAULT_PATH		_PATH_DEFPATH
#define DEFAULT_FILENAME	".depend"

static void save_for_optional(const char *, const char *);
static size_t write_optional(int, opt_t *, size_t);

static inline void *
deconst(const void *p)
{
	return (const char *)p - (const char *)0 + (char *)0;
}

__dead static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: %s [-aDdiopqv] [-f file] [-P prefix] [-s suffixes] "
	    "-- [flags] file ...\n",
	    getprogname());
	exit(EXIT_FAILURE);
}

static int
run_cc(int argc, char **argv, const char **fname)
{
	const char *CC, *tmpdir;
	char * volatile pathname;
	static char tmpfilename[MAXPATHLEN];
	char **args;
	int tmpfd;
	pid_t pid, cpid;
	int status;

	if ((CC = getenv("CC")) == NULL)
		CC = DEFAULT_CC;
	if ((pathname = findcc(CC)) == NULL)
		if (!setenv("PATH", DEFAULT_PATH, 1))
			pathname = findcc(CC);
	if (pathname == NULL)
		err(EXIT_FAILURE, "%s: not found", CC);
	if ((args = malloc((argc + 3) * sizeof(char *))) == NULL)
		err(EXIT_FAILURE, "malloc");

	args[0] = deconst(CC);
	args[1] = deconst("-M");
	(void)memcpy(&args[2], argv, (argc + 1) * sizeof(char *));

	if ((tmpdir = getenv("TMPDIR")) == NULL)
		tmpdir = _PATH_TMP;
	(void)snprintf(tmpfilename, sizeof (tmpfilename), "%s/%s", tmpdir,
	    "mkdepXXXXXX");
	if ((tmpfd = mkstemp(tmpfilename)) < 0)
		err(EXIT_FAILURE,  "Unable to create temporary file %s",
		    tmpfilename);
	(void)unlink(tmpfilename);
	*fname = tmpfilename;

	if (verbose) {
		char **a;
		for (a = args; *a; a++)
			printf("%s ", *a);
		printf("\n");
	}

	switch (cpid = vfork()) {
	case 0:
		(void)dup2(tmpfd, STDOUT_FILENO);
		(void)close(tmpfd);

		(void)execv(pathname, args);
		_exit(EXIT_FAILURE);
		/* NOTREACHED */

	case -1:
		err(EXIT_FAILURE, "unable to fork");
	}

	free(pathname);
	free(args);

	while (((pid = wait(&status)) != cpid) && (pid >= 0))
		continue;

	if (status)
		errx(EXIT_FAILURE, "compile failed.");

	return tmpfd;
}

static const char *
read_fname(void)
{
	static char *fbuf;
	static int fbuflen;
	int len, ch;

	for (len = 0; (ch = getchar()) != EOF; len++) {
		if (isspace(ch)) {
			if (len != 0)
				break;
			len--;
			continue;
		}
		if (len >= fbuflen - 1) {
			fbuf = realloc(fbuf, fbuflen += 32);
			if (fbuf == NULL)
				err(EXIT_FAILURE, "no memory");
		}
		fbuf[len] = ch;
	}
	if (len == 0)
		return NULL;
	fbuf[len] = 0;
	return fbuf;
}

static struct option longopt[] = {
	{ "sysroot", 1, NULL, 'R' },
	{ NULL, 0, NULL, '\0' },
};

static void
addsuff(suff_list_t **l, const char *s, size_t len)
{
	suff_list_t *p = calloc(1, sizeof(*p));
	if (p == NULL)
		err(1, "calloc");
	p->suff = malloc(len + 1);
	if (p->suff == NULL)
		err(1, "malloc");
	memcpy(p->suff, s, len);
	p->suff[len] = '\0';
	p->len = len;
	p->next = *l;
	*l = p;
}

int
main(int argc, char **argv)
{
	int 	aflag, dflag, iflag, oflag, qflag;
	const char *filename;
	int	dependfile;
	char	*buf, *lim, *ptr, *line, *suf, *colon, *eol;
	int	ok_ind, ch;
	size_t	sz;
	int	fd;
	size_t  slen;
	const char *fname;
	const char *prefix = NULL;
	const char *suffixes = NULL, *s;
	suff_list_t *suff_list = NULL, *sl;
#if !defined(__minix)
	size_t nr;
#else
	/* triggers a 'may be used uninitialized', when compiled with gcc,
	 * asserts off, and -Os. */
	slen = 0;
#endif /* !defined(__minix) */

	suf = NULL;		/* XXXGCC -Wuninitialized [sun2] */
	sl = NULL;		/* XXXGCC -Wuninitialized [sun2] */

	setlocale(LC_ALL, "");
	setprogname(argv[0]);

	aflag = O_WRONLY | O_APPEND | O_CREAT | O_TRUNC;
	dflag = 0;
	iflag = 0;
	oflag = 0;
	qflag = 0;
	filename = DEFAULT_FILENAME;
	dependfile = -1;

	opterr = 0;	/* stop getopt() bleating about errors. */
	for (;;) {
		ok_ind = optind;
		ch = getopt_long(argc, argv, "aDdf:ioP:pqRs:v", longopt, NULL);
		switch (ch) {
		case -1:
			ok_ind = optind;
			break;
		case 'a':	/* Append to output file */
			aflag &= ~O_TRUNC;
			continue;
		case 'D':	/* Process *.d files (don't run cc -M) */
			dflag = 2;	/* Read names from stdin */
			opterr = 1;
			continue;
		case 'd':	/* Process *.d files (don't run cc -M) */
			dflag = 1;
			opterr = 1;
			continue;
		case 'f':	/* Name of output file */
			filename = optarg;
			continue;
		case 'i':
			iflag = 1;
			continue;
		case 'o':	/* Mark dependent files .OPTIONAL */
			oflag = 1;
			continue;
		case 'P':	/* Prefix for each target filename */
			prefix = optarg;
			continue;
		case 'p':	/* Program mode (x.o: -> x:) */
			suffixes = "";
			continue;
		case 'q':	/* Quiet */
			qflag = 1;
			continue;
		case 'R':
			/* sysroot = optarg */
			continue;
		case 's':	/* Suffix list */
			suffixes = optarg;
			continue;
		case 'v':
			verbose = 1;
			continue;
		default:
			if (dflag)
				usage();
			/* Unknown arguments are passed to "${CC} -M" */
			break;
		}
		break;
	}

	argc -= ok_ind;
	argv += ok_ind;
	if ((argc == 0 && !dflag) || (argc != 0 && dflag == 2))
		usage();

	if (suffixes != NULL) {
		if (*suffixes) {
			for (s = suffixes; (sz = strcspn(s, ", ")) != 0;) {
				addsuff(&suff_list, s, sz);
				s += sz;
				while (*s && strchr(", ", *s))
					s++;
			}
		} else
			addsuff(&suff_list, "", 0);
	}

	dependfile = open(filename, aflag, 0666);
	if (dependfile == -1)
		goto wrerror;

	while (dflag == 2 || *argv != NULL) {
		if (dflag) {
			if (dflag == 2) {
				fname = read_fname();
				if (fname == NULL)
					break;
			} else
				fname = *argv++;
			if (iflag) {
				if (dprintf(dependfile, ".-include \"%s\"\n",
				    fname) < 0)
					goto wrerror;
				continue;
			}
			fd = open(fname, O_RDONLY, 0);
			if (fd == -1) {
				if (!qflag)
					warn("ignoring %s", fname);
				continue;
			}
		} else {
			fd = run_cc(argc, argv, &fname);
			/* consume all args... */
			argv += argc;
		}

		sz = lseek(fd, 0, SEEK_END);
		if (sz == 0) {
			close(fd);
			continue;
		}
		buf = mmap(NULL, sz, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
		close(fd);

		if (buf == MAP_FAILED)
			err(EXIT_FAILURE, "unable to mmap file %s", fname);
		lim = buf + sz - 1;

		/* Remove leading "./" from filenames */
		for (ptr = buf; ptr < lim; ptr++) {
			if (ptr[1] != '.' || ptr[2] != '/'
			    || !isspace((unsigned char)ptr[0]))
				continue;
			ptr[1] = ' ';
			ptr[2] = ' ';
		}

		for (line = eol = buf; eol <= lim;) {
			while (eol <= lim && *eol++ != '\n')
				/* Find end of this line */
				continue;
			if (line == eol - 1) {
				/* empty line - ignore */
				line = eol;
				continue;
			}
			if (eol[-2] == '\\')
				/* Assemble continuation lines */
				continue;
			for (colon = line; *colon != ':'; colon++) {
				if (colon >= eol) {
					colon = NULL;
					break;
				}
			}
			if (isspace((unsigned char)*line) || colon == NULL) {
				/* No dependency - just transcribe line */
				if (write(dependfile, line, eol - line) < 0)
					goto wrerror;
				line = eol;
				continue;
			}
			if (suff_list != NULL) {
				/* Find the .o: */
				/* First allow for any whitespace */
				for (suf = colon; suf > buf; suf--) {
					if (!isspace((unsigned char)suf[-1]))
						break;
				}
				if (suf == buf)
					errx(EXIT_FAILURE,
					    "Corrupted file `%s'", fname);
				/* Then look for any valid suffix */
				for (sl = suff_list; sl != NULL;
				    sl = sl->next) {
					if (sl->len && buf <= suf - sl->len &&
					    !memcmp(suf - sl->len, sl->suff,
						    sl->len))
						break;
				}
				/*
				 * Not found, check for .o, since the
				 * original file will have it.
				 */
				if (sl == NULL) {
					if (memcmp(suf - 2, ".o", 2) == 0)
						slen = 2;
					else
						slen = 0;
				} else
					slen = sl->len;
			}
			if (suff_list != NULL && slen != 0) {
				suf -= slen;
				for (sl = suff_list; sl != NULL; sl = sl->next)
				{
					if (sl != suff_list)
						if (write(dependfile, " ", 1)
						    < 0)
							goto wrerror;
					if (prefix != NULL)
						if (write(dependfile, prefix,
						    strlen(prefix)) < 0)
							goto wrerror;
					if (write(dependfile, line,
					    suf - line) < 0)
						goto wrerror;
					if (write(dependfile, sl->suff,
					    sl->len) < 0)
						goto wrerror;
				}
				if (write(dependfile, colon, eol - colon) < 0)
					goto wrerror;
			} else {
				if (prefix != NULL)
					if (write(dependfile, prefix,
					    strlen(prefix)) < 0)
						goto wrerror;
				if (write(dependfile, line, eol - line) < 0)
					goto wrerror;
			}

			if (oflag)
				save_for_optional(colon + 1, eol);
			line = eol;
		}
		munmap(buf, sz);
	}

	if (oflag && opt != NULL) {
		if (write(dependfile, ".OPTIONAL:", 10) < 0)
			goto wrerror;
		width = 9;
		sz = write_optional(dependfile, opt, 0);
		if (sz == (size_t)-1)
			goto wrerror;
		/* 'depth' is about 39 for an i386 kernel */
		/* fprintf(stderr, "Recursion depth %d\n", sz); */
	}
	close(dependfile);

	exit(EXIT_SUCCESS);
wrerror:
	err(EXIT_FAILURE, "unable to %s to file %s",
	    aflag & O_TRUNC ? "write" : "append", filename);
}


/*
 * Only save each file once - the kernel .depend is 3MB and there is
 * no point doubling its size.
 * The data seems to be 'random enough' so the simple binary tree
 * only has a reasonable depth.
 */
static void
save_for_optional(const char *start, const char *limit)
{
	opt_t **l, *n;
	const char *name, *end;
	int c;

	while (start < limit && strchr(" \t\n\\", *start)) 
		start++;
	for (name = start; ; name = end) {
		while (name < limit && strchr(" \t\n\\", *name)) 
			name++;
		for (end = name; end < limit && !strchr(" \t\n\\", *end);)
			end++;
		if (name >= limit)
			break;
		if (end[-1] == 'c' && end[-2] == '.' && name == start)
			/* ignore dependency on the files own .c */
			continue;
		for (l = &opt;;) {
			n = *l;
			if (n == NULL) {
				n = malloc(sizeof *n + (end - name));
				n->left = n->right = 0;
				n->len = end - name;
				n->count = 1;
				n->name[0] = ' ';
				memcpy(n->name + 1, name, end - name);
				*l = n;
				break;
			}
			c = (end - name) - n->len;
			if (c == 0)
				c = memcmp(n->name + 1, name, (end - name));
			if (c == 0) {
				/* Duplicate */
				n->count++;
				break;
			}
			if (c < 0)
				l = &n->left;
			else
				l = &n->right;
		}
	}
}

static size_t
write_optional(int fd, opt_t *node, size_t depth)
{
	size_t d1 = ++depth;

	if (node->left)
		d1 = write_optional(fd, node->left, d1);
	if (width > 76 - node->len) {
		if (write(fd, " \\\n ", 4) < 0)
			return (size_t)-1;
		width = 1;
	}
	width += 1 + node->len;
	if (write(fd, node->name, 1 + node->len) < 0)
		return (size_t)-1;
	if (node->right)
		depth = write_optional(fd, node->right, depth);
	return d1 > depth ? d1 : depth;
}

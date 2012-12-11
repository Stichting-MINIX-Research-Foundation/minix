/*	$NetBSD: sort.c,v 1.61 2011/09/16 15:39:29 joerg Exp $	*/

/*-
 * Copyright (c) 2000-2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ben Harris and Jaromir Dolecek.
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

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Peter McIlroy.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Sort sorts a file using an optional user-defined key.
 * Sort uses radix sort for internal sorting, and allows
 * a choice of merge sort and radix sort for external sorting.
 */

#include <util.h>
#include "sort.h"
#include "fsort.h"
#include "pathnames.h"

#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

__RCSID("$NetBSD: sort.c,v 1.61 2011/09/16 15:39:29 joerg Exp $");

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <paths.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>

int REC_D = '\n';
u_char d_mask[NBINS];		/* flags for rec_d, field_d, <blank> */

/*
 * weight tables.  Gweights is one of ascii, Rascii..
 * modified to weight rec_d = 0 (or 255)
 */
u_char *const weight_tables[4] = { ascii, Rascii, Ftable, RFtable };
u_char ascii[NBINS], Rascii[NBINS], RFtable[NBINS], Ftable[NBINS];

int SINGL_FLD = 0, SEP_FLAG = 0, UNIQUE = 0;
int REVERSE = 0;
int posix_sort;

unsigned int debug_flags = 0;

static char toutpath[MAXPATHLEN];

const char *tmpdir;	/* where temporary files should be put */

static void cleanup(void);
static void onsignal(int);
__dead static void usage(const char *);

int
main(int argc, char *argv[])
{
	int ch, i, stdinflag = 0;
	char cflag = 0, mflag = 0;
	char *outfile, *outpath = 0;
	struct field *fldtab;
	size_t fldtab_sz, fld_cnt;
	struct filelist filelist;
	int num_input_files;
	FILE *outfp = NULL;
#if !defined(__minix)
	struct rlimit rl;
#endif /* !defined(__minix) */
	struct stat st;

	setlocale(LC_ALL, "");

#if !defined(__minix)
	/* bump RLIMIT_NOFILE to maximum our hard limit allows */
	if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
		err(2, "getrlimit");
	rl.rlim_cur = rl.rlim_max;
	if (setrlimit(RLIMIT_NOFILE, &rl) < 0)
		err(2, "setrlimit");
#endif /* !defined(__minix) */
	
	d_mask[REC_D = '\n'] = REC_D_F;
	d_mask['\t'] = d_mask[' '] = BLANK | FLD_D;

	/* fldtab[0] is the global options. */
	fldtab_sz = 3;
	fld_cnt = 0;
	fldtab = emalloc(fldtab_sz * sizeof(*fldtab));
	memset(fldtab, 0, fldtab_sz * sizeof(*fldtab));

#define SORT_OPTS "bcdD:fHik:lmno:rR:sSt:T:ux"

	/* Convert "+field" args to -f format */
	fixit(&argc, argv, SORT_OPTS);

	if (!(tmpdir = getenv("TMPDIR")))
		tmpdir = _PATH_TMP;

	while ((ch = getopt(argc, argv, SORT_OPTS)) != -1) {
		switch (ch) {
		case 'b':
			fldtab[0].flags |= BI | BT;
			break;
		case 'c':
			cflag = 1;
			break;
		case 'D': /* Debug flags */
			for (i = 0; optarg[i]; i++)
			    debug_flags |= 1 << (optarg[i] & 31);
			break;
		case 'd': case 'f': case 'i': case 'n': case 'l':
#ifdef __minix
		case 'x':
#endif
			fldtab[0].flags |= optval(ch, 0);
			break;
		case 'H':
			/* -H was ; use merge sort for blocks of large files' */
			/* That is now the default. */
			break;
		case 'k':
			fldtab = erealloc(fldtab, (fldtab_sz + 1) * sizeof(*fldtab));
			memset(&fldtab[fldtab_sz], 0, sizeof(fldtab[0]));
			fldtab_sz++;

			setfield(optarg, &fldtab[++fld_cnt], fldtab[0].flags);
			break;
		case 'm':
			mflag = 1;
			break;
		case 'o':
			outpath = optarg;
			break;
		case 'r':
			REVERSE = 1;
			break;
		case 's':
			/*
			 * Nominally 'stable sort', keep lines with equal keys
			 * in input file order. (Default for NetBSD)
			 * (-s for GNU sort compatibility.)
			 */
			posix_sort = 0;
			break;
		case 'S':
			/*
			 * Reverse of -s!
			 * This needs to enforce a POSIX sort where records
			 * with equal keys are then sorted by the raw data.
			 * Currently not implemented!
			 * (using libc radixsort() v sradixsort() doesn't
			 * have the desired effect.)
			 */
			posix_sort = 1;
			break;
		case 't':
			if (SEP_FLAG)
				usage("multiple field delimiters");
			SEP_FLAG = 1;
			d_mask[' '] &= ~FLD_D;
			d_mask['\t'] &= ~FLD_D;
			d_mask[(u_char)*optarg] |= FLD_D;
			if (d_mask[(u_char)*optarg] & REC_D_F)
				errx(2, "record/field delimiter clash");
			break;
		case 'R':
			if (REC_D != '\n')
				usage("multiple record delimiters");
			REC_D = *optarg;
			if (REC_D == '\n')
				break;
			if (optarg[1] != '\0') {
				char *ep;
				int t = 0;
				if (optarg[0] == '\\')
					optarg++, t = 8;
				REC_D = (int)strtol(optarg, &ep, t);
				if (*ep != '\0' || REC_D < 0 ||
				    REC_D >= (int)__arraycount(d_mask))
					errx(2, "invalid record delimiter %s",
					    optarg);
			}
			d_mask['\n'] = d_mask[' '];
			d_mask[REC_D] = REC_D_F;
			break;
		case 'T':
			/* -T tmpdir */
			tmpdir = optarg;
			break;
		case 'u':
			UNIQUE = 1;
			break;
		case '?':
		default:
			usage(NULL);
		}
	}

	if (UNIQUE)
		/* Don't sort on raw record if keys match */
		posix_sort = 0;

	if (cflag && argc > optind+1)
		errx(2, "too many input files for -c option");
	if (argc - 2 > optind && !strcmp(argv[argc-2], "-o")) {
		outpath = argv[argc-1];
		argc -= 2;
	}
	if (mflag && argc - optind > (MAXFCT - (16+1))*16)
		errx(2, "too many input files for -m option");

	for (i = optind; i < argc; i++) {
		/* allow one occurrence of /dev/stdin */
		if (!strcmp(argv[i], "-") || !strcmp(argv[i], _PATH_STDIN)) {
			if (stdinflag)
				warnx("ignoring extra \"%s\" in file list",
				    argv[i]);
			else
				stdinflag = 1;

			/* change to /dev/stdin if '-' */
			if (argv[i][0] == '-') {
				static char path_stdin[] = _PATH_STDIN;
				argv[i] = path_stdin;
			}

		} else if ((ch = access(argv[i], R_OK)))
			err(2, "%s", argv[i]);
	}

	if (fldtab[1].icol.num == 0) {
		/* No sort key specified */
		if (fldtab[0].flags & (I|D|F|N|L)) {
			/* Modified - generate a key that covers the line */
			fldtab[0].flags &= ~(BI|BT);
			setfield("1", &fldtab[++fld_cnt], fldtab->flags);
			fldreset(fldtab);
		} else {
			/* Unmodified, just compare the line */
			SINGL_FLD = 1;
			fldtab[0].icol.num = 1;
		}
	} else {
		fldreset(fldtab);
	}

	settables();

	if (optind == argc) {
		static const char * const names[] = { _PATH_STDIN, NULL };
		filelist.names = names;
		num_input_files = 1;
	} else {
		filelist.names = (const char * const *) &argv[optind];
		num_input_files = argc - optind;
	}

	if (cflag) {
		order(&filelist, fldtab);
		/* NOT REACHED */
	}

	if (!outpath) {
		toutpath[0] = '\0';	/* path not used in this case */
		outfile = outpath = toutpath;
		outfp = stdout;
	} else if (lstat(outpath, &st) == 0
	    && !S_ISCHR(st.st_mode) && !S_ISBLK(st.st_mode)) {
		/* output file exists and isn't character or block device */
		struct sigaction act;
		static const int sigtable[] = {SIGHUP, SIGINT, SIGPIPE,
#if defined(__minix)
		    SIGVTALRM, SIGPROF, 0};
#else
		    SIGXCPU, SIGXFSZ, SIGVTALRM, SIGPROF, 0};
#endif /* defined(__minix) */
		int outfd;
		errno = 0;
		if (access(outpath, W_OK))
			err(2, "%s", outpath);
		(void)snprintf(toutpath, sizeof(toutpath), "%sXXXXXX",
		    outpath);
		if ((outfd = mkstemp(toutpath)) == -1)
			err(2, "Cannot create temporary file `%s'", toutpath);
		(void)atexit(cleanup);
		act.sa_handler = onsignal;
		(void) sigemptyset(&act.sa_mask);
		act.sa_flags = SA_RESTART | SA_RESETHAND;
		for (i = 0; sigtable[i]; ++i)	/* always unlink toutpath */
			sigaction(sigtable[i], &act, 0);
		outfile = toutpath;
		if ((outfp = fdopen(outfd, "w")) == NULL)
			err(2, "Cannot open temporary file `%s'", toutpath);
	} else {
		outfile = outpath;

		if ((outfp = fopen(outfile, "w")) == NULL)
			err(2, "output file %s", outfile);
	}

	if (mflag)
		fmerge(&filelist, num_input_files, outfp, fldtab);
	else
		fsort(&filelist, num_input_files, outfp, fldtab);

	if (outfile != outpath) {
		if (access(outfile, F_OK))
			err(2, "%s", outfile);

		/*
		 * Copy file permissions bits of the original file.
		 * st is initialized above, when we create the
		 * temporary spool file.
		 */
		if (lchmod(outfile, st.st_mode & ALLPERMS) != 0) {
			err(2, "cannot chmod %s: output left in %s",
			    outpath, outfile);
		}

		(void)unlink(outpath);
		if (link(outfile, outpath))
			err(2, "cannot link %s: output left in %s",
			    outpath, outfile);
		(void)unlink(outfile);
		toutpath[0] = 0;
	}
	exit(0);
}

static void
onsignal(int sig)
{
	cleanup();
}

static void
cleanup(void)
{
	if (toutpath[0])
		(void)unlink(toutpath);
}

static void
usage(const char *msg)
{
	if (msg != NULL)
		(void)fprintf(stderr, "%s: %s\n", getprogname(), msg);
	(void)fprintf(stderr,
	    "usage: %s [-bcdfHilmnrSsu] [-k field1[,field2]] [-o output]"
	    " [-R char] [-T dir]", getprogname());
	(void)fprintf(stderr,
	    "             [-t char] [file ...]\n");
	exit(2);
}

RECHEADER *
allocrec(RECHEADER *rec, size_t size)
{

	return (erealloc(rec, size + sizeof(long) - 1));
}

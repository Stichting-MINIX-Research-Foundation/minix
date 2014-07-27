/* $NetBSD: cp.c,v 1.58 2012/01/04 15:58:37 christos Exp $ */

/*
 * Copyright (c) 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * David Hitz of Auspex Systems Inc.
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

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT(
"@(#) Copyright (c) 1988, 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)cp.c	8.5 (Berkeley) 4/29/95";
#else
__RCSID("$NetBSD: cp.c,v 1.58 2012/01/04 15:58:37 christos Exp $");
#endif
#endif /* not lint */

/*
 * Cp copies source files to target files.
 * 
 * The global PATH_T structure "to" always contains the path to the
 * current target file.  Since fts(3) does not change directories,
 * this path can be either absolute or dot-relative.
 * 
 * The basic algorithm is to initialize "to" and use fts(3) to traverse
 * the file hierarchy rooted in the argument list.  A trivial case is the
 * case of 'cp file1 file2'.  The more interesting case is the case of
 * 'cp file1 file2 ... fileN dir' where the hierarchy is traversed and the
 * path (relative to the root of the traversal) is appended to dir (stored
 * in "to") to form the final target path.
 */

#include <sys/param.h>
#include <sys/stat.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <locale.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

#define	STRIP_TRAILING_SLASH(p) {					\
        while ((p).p_end > (p).p_path + 1 && (p).p_end[-1] == '/')	\
                *--(p).p_end = '\0';					\
}

static char empty[] = "";
PATH_T to = { .p_end = to.p_path, .target_end = empty  };

uid_t myuid;
int Hflag, Lflag, Rflag, Pflag, fflag, iflag, lflag, pflag, rflag, vflag, Nflag;
mode_t myumask;
sig_atomic_t pinfo;

enum op { FILE_TO_FILE, FILE_TO_DIR, DIR_TO_DNE };

static int copy(char *[], enum op, int);

static void
progress(int sig __unused)
{

	pinfo++;
}

int
main(int argc, char *argv[])
{
	struct stat to_stat, tmp_stat;
	enum op type;
	int ch, fts_options, r, have_trailing_slash;
	char *target, **src;

	setprogname(argv[0]);
	(void)setlocale(LC_ALL, "");

	Hflag = Lflag = Pflag = Rflag = 0;
	while ((ch = getopt(argc, argv, "HLNPRfailprv")) != -1) 
		switch (ch) {
		case 'H':
			Hflag = 1;
			Lflag = Pflag = 0;
			break;
		case 'L':
			Lflag = 1;
			Hflag = Pflag = 0;
			break;
		case 'N':
			Nflag = 1;
			break;
		case 'P':
			Pflag = 1;
			Hflag = Lflag = 0;
			break;
		case 'R':
			Rflag = 1;
			break;
		case 'a':
			Pflag = 1;
			pflag = 1;
			Rflag = 1;
			Hflag = Lflag = 0;
			break;
		case 'f':
			fflag = 1;
			iflag = 0;
			break;
		case 'i':
			iflag = isatty(fileno(stdin));
			fflag = 0;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'p':
			pflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	argc -= optind;
	argv += optind;

	if (argc < 2)
		usage();

	fts_options = FTS_NOCHDIR | FTS_PHYSICAL;
	if (rflag) {
		if (Rflag) {
			errx(EXIT_FAILURE,
		    "the -R and -r options may not be specified together.");
			/* NOTREACHED */
		}
		if (Hflag || Lflag || Pflag) {
			errx(EXIT_FAILURE,
	"the -H, -L, and -P options may not be specified with the -r option.");
			/* NOTREACHED */
		}
		fts_options &= ~FTS_PHYSICAL;
		fts_options |= FTS_LOGICAL;
	}

	if (Rflag) {
		if (Hflag)
			fts_options |= FTS_COMFOLLOW;
		if (Lflag) {
			fts_options &= ~FTS_PHYSICAL;
			fts_options |= FTS_LOGICAL;
		}
	} else if (!Pflag) {
		fts_options &= ~FTS_PHYSICAL;
		fts_options |= FTS_LOGICAL | FTS_COMFOLLOW;
	}

	myuid = getuid();

	/* Copy the umask for explicit mode setting. */
	myumask = umask(0);
	(void)umask(myumask);

	/* Save the target base in "to". */
	target = argv[--argc];
	if (strlcpy(to.p_path, target, sizeof(to.p_path)) >= sizeof(to.p_path))
		errx(EXIT_FAILURE, "%s: name too long", target);
	to.p_end = to.p_path + strlen(to.p_path);
	have_trailing_slash = (to.p_end[-1] == '/');
	if (have_trailing_slash)
		STRIP_TRAILING_SLASH(to);
	to.target_end = to.p_end;

	/* Set end of argument list for fts(3). */
	argv[argc] = NULL;     
	
	(void)signal(SIGINFO, progress);
	
	/*
	 * Cp has two distinct cases:
	 *
	 * cp [-R] source target
	 * cp [-R] source1 ... sourceN directory
	 *
	 * In both cases, source can be either a file or a directory.
	 *
	 * In (1), the target becomes a copy of the source. That is, if the
	 * source is a file, the target will be a file, and likewise for
	 * directories.
	 *
	 * In (2), the real target is not directory, but "directory/source".
	 */
	if (Pflag)
		r = lstat(to.p_path, &to_stat);
	else
		r = stat(to.p_path, &to_stat);
	if (r == -1 && errno != ENOENT) {
		err(EXIT_FAILURE, "%s", to.p_path);
		/* NOTREACHED */
	}
	if (r == -1 || !S_ISDIR(to_stat.st_mode)) {
		/*
		 * Case (1).  Target is not a directory.
		 */ 
		if (argc > 1)
			usage();
		/*
		 * Need to detect the case:
		 *	cp -R dir foo
		 * Where dir is a directory and foo does not exist, where
		 * we want pathname concatenations turned on but not for
		 * the initial mkdir().
		 */
		if (r == -1) {
			if (rflag || (Rflag && (Lflag || Hflag)))
				r = stat(*argv, &tmp_stat);
			else
				r = lstat(*argv, &tmp_stat);
			if (r == -1) {
				err(EXIT_FAILURE, "%s", *argv);
				/* NOTREACHED */
			}
			
			if (S_ISDIR(tmp_stat.st_mode) && (Rflag || rflag))
				type = DIR_TO_DNE;
			else
				type = FILE_TO_FILE;
		} else
			type = FILE_TO_FILE;

		if (have_trailing_slash && type == FILE_TO_FILE) {
			if (r == -1)
				errx(1, "directory %s does not exist",
				     to.p_path);
			else
				errx(1, "%s is not a directory", to.p_path);
		}
	} else {
		/*
		 * Case (2).  Target is a directory.
		 */
		type = FILE_TO_DIR;
	}

	/*
	 * make "cp -rp src/ dst" behave like "cp -rp src dst" not
	 * like "cp -rp src/. dst"
	 */
	for (src = argv; *src; src++) {
		size_t len = strlen(*src);
		while (len-- > 1 && (*src)[len] == '/')
			(*src)[len] = '\0';
	}

	exit(copy(argv, type, fts_options));
	/* NOTREACHED */
}

static int dnestack[MAXPATHLEN]; /* unlikely we'll have more nested dirs */
static ssize_t dnesp;
static void
pushdne(int dne)
{

	dnestack[dnesp++] = dne;
	assert(dnesp < MAXPATHLEN);
}

static int
popdne(void)
{
	int rv;

	rv = dnestack[--dnesp];
	assert(dnesp >= 0);
	return rv;
}

static int
copy(char *argv[], enum op type, int fts_options)
{
	struct stat to_stat;
	FTS *ftsp;
	FTSENT *curr;
	int base, dne, sval;
	int this_failed, any_failed;
	size_t nlen;
	char *p, *target_mid;

	base = 0;	/* XXX gcc -Wuninitialized (see comment below) */

	if ((ftsp = fts_open(argv, fts_options, NULL)) == NULL)
		err(EXIT_FAILURE, "%s", argv[0]);
		/* NOTREACHED */
	for (any_failed = 0; (curr = fts_read(ftsp)) != NULL;) {
		this_failed = 0;
		switch (curr->fts_info) {
		case FTS_NS:
		case FTS_DNR:
		case FTS_ERR:
			warnx("%s: %s", curr->fts_path,
					strerror(curr->fts_errno));
			this_failed = any_failed = 1;
			continue;
		case FTS_DC:			/* Warn, continue. */
			warnx("%s: directory causes a cycle", curr->fts_path);
			this_failed = any_failed = 1;
			continue;
		}

		/*
		 * If we are in case (2) or (3) above, we need to append the 
                 * source name to the target name.  
                 */
		if (type != FILE_TO_FILE) {
			if ((curr->fts_namelen +
			    to.target_end - to.p_path + 1) > MAXPATHLEN) {
				warnx("%s/%s: name too long (not copied)",
						to.p_path, curr->fts_name);
				this_failed = any_failed = 1;
				continue;
			}

			/*
			 * Need to remember the roots of traversals to create
			 * correct pathnames.  If there's a directory being
			 * copied to a non-existent directory, e.g.
			 *	cp -R a/dir noexist
			 * the resulting path name should be noexist/foo, not
			 * noexist/dir/foo (where foo is a file in dir), which
			 * is the case where the target exists.
			 *
			 * Also, check for "..".  This is for correct path
			 * concatentation for paths ending in "..", e.g.
			 *	cp -R .. /tmp
			 * Paths ending in ".." are changed to ".".  This is
			 * tricky, but seems the easiest way to fix the problem.
			 *
			 * XXX
			 * Since the first level MUST be FTS_ROOTLEVEL, base
			 * is always initialized.
			 */
			if (curr->fts_level == FTS_ROOTLEVEL) {
				if (type != DIR_TO_DNE) {
					p = strrchr(curr->fts_path, '/');
					base = (p == NULL) ? 0 : 
					    (int)(p - curr->fts_path + 1);

					if (!strcmp(&curr->fts_path[base], 
					    ".."))
						base += 1;
				} else
					base = curr->fts_pathlen;
			}

			p = &curr->fts_path[base];
			nlen = curr->fts_pathlen - base;
			target_mid = to.target_end;
			if (*p != '/' && target_mid[-1] != '/')
				*target_mid++ = '/';
			*target_mid = 0;

			if (target_mid - to.p_path + nlen >= PATH_MAX) {
				warnx("%s%s: name too long (not copied)",
				    to.p_path, p);
				this_failed = any_failed = 1;
				continue;
			}
			(void)strncat(target_mid, p, nlen);
			to.p_end = target_mid + nlen;
			*to.p_end = 0;
			STRIP_TRAILING_SLASH(to);
		}

		sval = Pflag ? lstat(to.p_path, &to_stat) : stat(to.p_path, &to_stat);
		/* Not an error but need to remember it happened */
		if (sval == -1)
			dne = 1;
		else {
			if (to_stat.st_dev == curr->fts_statp->st_dev &&
			    to_stat.st_ino == curr->fts_statp->st_ino) {
				warnx("%s and %s are identical (not copied).",
				    to.p_path, curr->fts_path);
				this_failed = any_failed = 1;
				if (S_ISDIR(curr->fts_statp->st_mode))
					(void)fts_set(ftsp, curr, FTS_SKIP);
				continue;
			}
			if (!S_ISDIR(curr->fts_statp->st_mode) &&
			    S_ISDIR(to_stat.st_mode)) {
		warnx("cannot overwrite directory %s with non-directory %s",
				    to.p_path, curr->fts_path);
				this_failed = any_failed = 1;
				continue;
			}
			dne = 0;
		}

		switch (curr->fts_statp->st_mode & S_IFMT) {
		case S_IFLNK:
			/* Catch special case of a non dangling symlink */
			if((fts_options & FTS_LOGICAL) ||
			   ((fts_options & FTS_COMFOLLOW) && curr->fts_level == 0)) {
				if (copy_file(curr, dne))
					this_failed = any_failed = 1;
			} else {	
				if (copy_link(curr, !dne))
					this_failed = any_failed = 1;
			}
			break;
		case S_IFDIR:
			if (!Rflag && !rflag) {
				if (curr->fts_info == FTS_D)
					warnx("%s is a directory (not copied).",
					    curr->fts_path);
				(void)fts_set(ftsp, curr, FTS_SKIP);
				this_failed = any_failed = 1;
				break;
			}

                        /*
                         * Directories get noticed twice:
                         *  In the first pass, create it if needed.
                         *  In the second pass, after the children have been copied, set the permissions.
                         */
			if (curr->fts_info == FTS_D) /* First pass */
			{
				/*
				 * If the directory doesn't exist, create the new
				 * one with the from file mode plus owner RWX bits,
				 * modified by the umask.  Trade-off between being
				 * able to write the directory (if from directory is
				 * 555) and not causing a permissions race.  If the
				 * umask blocks owner writes, we fail..
				 */
				pushdne(dne);
				if (dne) {
					if (mkdir(to.p_path, 
					    curr->fts_statp->st_mode | S_IRWXU) < 0)
						err(EXIT_FAILURE, "%s",
						    to.p_path);
						/* NOTREACHED */
				} else if (!S_ISDIR(to_stat.st_mode)) {
					errno = ENOTDIR;
					err(EXIT_FAILURE, "%s",
						to.p_path);
					/* NOTREACHED */
				}
			}
			else if (curr->fts_info == FTS_DP) /* Second pass */
			{
	                        /*
				 * If not -p and directory didn't exist, set it to be
				 * the same as the from directory, umodified by the 
                        	 * umask; arguably wrong, but it's been that way 
                        	 * forever.
				 */
				if (pflag && setfile(curr->fts_statp, 0))
					this_failed = any_failed = 1;
				else if ((dne = popdne()))
					(void)chmod(to.p_path, 
					    curr->fts_statp->st_mode);
			}
			else
			{
				warnx("directory %s encountered when not expected.",
				    curr->fts_path);
				this_failed = any_failed = 1;
				break;
			}

			break;
		case S_IFBLK:
		case S_IFCHR:
			if (Rflag) {
				if (copy_special(curr->fts_statp, !dne))
					this_failed = any_failed = 1;
			} else
				if (copy_file(curr, dne))
					this_failed = any_failed = 1;
			break;
		case S_IFIFO:
			if (Rflag) {
				if (copy_fifo(curr->fts_statp, !dne))
					this_failed = any_failed = 1;
			} else 
				if (copy_file(curr, dne))
					this_failed = any_failed = 1;
			break;
		default:
			if (copy_file(curr, dne))
				this_failed = any_failed = 1;
			break;
		}
		if (vflag && !this_failed)
			(void)printf("%s -> %s\n", curr->fts_path, to.p_path);
	}
	if (errno) {
		err(EXIT_FAILURE, "fts_read");
		/* NOTREACHED */
	}
	(void)fts_close(ftsp);
	return (any_failed);
}

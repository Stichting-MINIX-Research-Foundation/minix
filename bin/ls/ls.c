/*	$NetBSD: ls.c,v 1.69 2011/08/29 14:44:21 joerg Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Michael Fischbein.
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
__COPYRIGHT("@(#) Copyright (c) 1989, 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)ls.c	8.7 (Berkeley) 8/5/94";
#else
__RCSID("$NetBSD: ls.c,v 1.69 2011/08/29 14:44:21 joerg Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <pwd.h>
#include <grp.h>
#include <util.h>

#include "ls.h"
#include "extern.h"

static void	 display(FTSENT *, FTSENT *);
static int	 mastercmp(const FTSENT **, const FTSENT **);
static void	 traverse(int, char **, int);

static void (*printfcn)(DISPLAY *);
static int (*sortfcn)(const FTSENT *, const FTSENT *);

#define	BY_NAME 0
#define	BY_SIZE 1
#define	BY_TIME	2

long blocksize;			/* block size units */
int termwidth = 80;		/* default terminal width */
int sortkey = BY_NAME;
int rval = EXIT_SUCCESS;	/* exit value - set if error encountered */

/* flags */
int f_accesstime;		/* use time of last access */
int f_column;			/* columnated format */
int f_columnacross;		/* columnated format, sorted across */
int f_flags;			/* show flags associated with a file */
int f_grouponly;		/* long listing without owner */
int f_humanize;			/* humanize the size field */
int f_commas;           /* separate size field with comma */
int f_inode;			/* print inode */
int f_listdir;			/* list actual directory, not contents */
int f_listdot;			/* list files beginning with . */
int f_longform;			/* long listing format */
int f_nonprint;			/* show unprintables as ? */
int f_nosort;			/* don't sort output */
int f_numericonly;		/* don't convert uid/gid to name */
int f_octal;			/* print octal escapes for nongraphic characters */
int f_octal_escape;		/* like f_octal but use C escapes if possible */
int f_recursive;		/* ls subdirectories also */
int f_reversesort;		/* reverse whatever sort is used */
int f_sectime;			/* print the real time for all files */
int f_singlecol;		/* use single column output */
int f_size;			/* list size in short listing */
int f_statustime;		/* use time of last mode change */
int f_stream;			/* stream format */
int f_type;			/* add type character for non-regular files */
int f_typedir;			/* add type character for directories */
int f_whiteout;			/* show whiteout entries */

__dead static void
usage(void)
{

	(void)fprintf(stderr,
	    "usage: %s [-AaBbCcdFfghikLlMmnopqRrSsTtuWwx1] [file ...]\n",
	    getprogname());
	exit(EXIT_FAILURE);
	/* NOTREACHED */
}

int
ls_main(int argc, char *argv[])
{
	static char dot[] = ".", *dotav[] = { dot, NULL };
	struct winsize win;
	int ch, fts_options;
	int kflag = 0;
	const char *p;

	setprogname(argv[0]);
	(void)setlocale(LC_ALL, "");

	/* Terminal defaults to -Cq, non-terminal defaults to -1. */
	if (isatty(STDOUT_FILENO)) {
		if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) == 0 &&
		    win.ws_col > 0)
			termwidth = win.ws_col;
		f_column = f_nonprint = 1;
	} else
		f_singlecol = 1;

	/* Root is -A automatically. */
	if (!getuid())
		f_listdot = 1;

	fts_options = FTS_PHYSICAL;
	while ((ch = getopt(argc, argv, "1ABCFLMRSTWabcdfghiklmnopqrstuwx")) != -1) {
		switch (ch) {
		/*
		 * The -1, -C, -l, -m and -x options all override each other so
		 * shell aliasing works correctly.
		 */
		case '1':
			f_singlecol = 1;
			f_column = f_columnacross = f_longform = f_stream = 0;
			break;
		case 'C':
			f_column = 1;
			f_columnacross = f_longform = f_singlecol = f_stream =
			    0;
			break;
		case 'g':
			if (f_grouponly != -1)
				f_grouponly = 1;
			f_longform = 1;
			f_column = f_columnacross = f_singlecol = f_stream = 0;
			break;
		case 'l':
			f_longform = 1;
			f_column = f_columnacross = f_singlecol = f_stream = 0;
			/* Never let -g take precedence over -l. */
			f_grouponly = -1;
			break;
		case 'm':
			f_stream = 1;
			f_column = f_columnacross = f_longform = f_singlecol =
			    0;
			break;
		case 'x':
			f_columnacross = 1;
			f_column = f_longform = f_singlecol = f_stream = 0;
			break;
		/* The -c and -u options override each other. */
		case 'c':
			f_statustime = 1;
			f_accesstime = 0;
			break;
		case 'u':
			f_accesstime = 1;
			f_statustime = 0;
			break;
		case 'F':
			f_type = 1;
			break;
		case 'L':
			fts_options &= ~FTS_PHYSICAL;
			fts_options |= FTS_LOGICAL;
			break;
		case 'R':
			f_recursive = 1;
			break;
		case 'a':
			fts_options |= FTS_SEEDOT;
			/* FALLTHROUGH */
		case 'A':
			f_listdot = 1;
			break;
		/* The -B option turns off the -b, -q and -w options. */
		case 'B':
			f_nonprint = 0;
			f_octal = 1;
			f_octal_escape = 0;
			break;
		/* The -b option turns off the -B, -q and -w options. */
		case 'b':
			f_nonprint = 0;
			f_octal = 0;
			f_octal_escape = 1;
			break;
		/* The -d option turns off the -R option. */
		case 'd':
			f_listdir = 1;
			f_recursive = 0;
			break;
		case 'f':
			f_nosort = 1;
			break;
		case 'i':
			f_inode = 1;
			break;
		case 'k':
			blocksize = 1024;
			kflag = 1;
			f_humanize = 0;
			break;
		/* The -h option forces all sizes to be measured in bytes. */
		case 'h':
			f_humanize = 1;
			kflag = 0;
			f_commas = 0;
			break;
		case 'M':
			f_humanize = 0;
			f_commas = 1;
			break;
		case 'n':
			f_numericonly = 1;
			f_longform = 1;
			f_column = f_columnacross = f_singlecol = f_stream = 0;
			break;
		case 'o':
			f_flags = 1;
			break;
		case 'p':
			f_typedir = 1;
			break;
		/* The -q option turns off the -B, -b and -w options. */
		case 'q':
			f_nonprint = 1;
			f_octal = 0;
			f_octal_escape = 0;
			break;
		case 'r':
			f_reversesort = 1;
			break;
		case 'S':
			sortkey = BY_SIZE;
			break;
		case 's':
			f_size = 1;
			break;
		case 'T':
			f_sectime = 1;
			break;
		case 't':
			sortkey = BY_TIME;
			break;
		case 'W':
			f_whiteout = 1;
			break;
		/* The -w option turns off the -B, -b and -q options. */
		case 'w':
			f_nonprint = 0;
			f_octal = 0;
			f_octal_escape = 0;
			break;
		default:
		case '?':
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (f_column || f_columnacross || f_stream) {
		if ((p = getenv("COLUMNS")) != NULL)
			termwidth = atoi(p);
	}

	/*
	 * If both -g and -l options, let -l take precedence.
	 */
	if (f_grouponly == -1)
		f_grouponly = 0;

	/*
	 * If not -F, -i, -l, -p, -S, -s or -t options, don't require stat
	 * information.
	 */
	if (!f_inode && !f_longform && !f_size && !f_type && !f_typedir &&
	    sortkey == BY_NAME)
		fts_options |= FTS_NOSTAT;

	/*
	 * If not -F, -d or -l options, follow any symbolic links listed on
	 * the command line.
	 */
	if (!f_longform && !f_listdir && !f_type)
		fts_options |= FTS_COMFOLLOW;

	/*
	 * If -W, show whiteout entries
	 */
#ifdef FTS_WHITEOUT
	if (f_whiteout)
		fts_options |= FTS_WHITEOUT;
#endif

	/* If -l or -s, figure out block size. */
	if (f_inode || f_longform || f_size) {
		if (!kflag)
			(void)getbsize(NULL, &blocksize);
		blocksize /= 512;
	}

	/* Select a sort function. */
	if (f_reversesort) {
		switch (sortkey) {
		case BY_NAME:
			sortfcn = revnamecmp;
			break;
		case BY_SIZE:
			sortfcn = revsizecmp;
			break;
		case BY_TIME:
			if (f_accesstime)
				sortfcn = revacccmp;
			else if (f_statustime)
				sortfcn = revstatcmp;
			else /* Use modification time. */
				sortfcn = revmodcmp;
			break;
		}
	} else {
		switch (sortkey) {
		case BY_NAME:
			sortfcn = namecmp;
			break;
		case BY_SIZE:
			sortfcn = sizecmp;
			break;
		case BY_TIME:
			if (f_accesstime)
				sortfcn = acccmp;
			else if (f_statustime)
				sortfcn = statcmp;
			else /* Use modification time. */
				sortfcn = modcmp;
			break;
		}
	}

	/* Select a print function. */
	if (f_singlecol)
		printfcn = printscol;
	else if (f_columnacross)
		printfcn = printacol;
	else if (f_longform)
		printfcn = printlong;
	else if (f_stream)
		printfcn = printstream;
	else
		printfcn = printcol;

	if (argc)
		traverse(argc, argv, fts_options);
	else
		traverse(1, dotav, fts_options);
	return rval;
	/* NOTREACHED */
}

static int output;			/* If anything output. */

/*
 * Traverse() walks the logical directory structure specified by the argv list
 * in the order specified by the mastercmp() comparison function.  During the
 * traversal it passes linked lists of structures to display() which represent
 * a superset (may be exact set) of the files to be displayed.
 */
static void
traverse(int argc, char *argv[], int options)
{
	FTS *ftsp;
	FTSENT *p, *chp;
	int ch_options, error;

	if ((ftsp =
	    fts_open(argv, options, f_nosort ? NULL : mastercmp)) == NULL)
		err(EXIT_FAILURE, NULL);

	display(NULL, fts_children(ftsp, 0));
	if (f_listdir) {
		(void)fts_close(ftsp);
		return;
	}

	/*
	 * If not recursing down this tree and don't need stat info, just get
	 * the names.
	 */
	ch_options = !f_recursive && options & FTS_NOSTAT ? FTS_NAMEONLY : 0;

	while ((p = fts_read(ftsp)) != NULL)
		switch (p->fts_info) {
		case FTS_DC:
			warnx("%s: directory causes a cycle", p->fts_name);
			break;
		case FTS_DNR:
		case FTS_ERR:
			warnx("%s: %s", p->fts_name, strerror(p->fts_errno));
			rval = EXIT_FAILURE;
			break;
		case FTS_D:
			if (p->fts_level != FTS_ROOTLEVEL &&
			    p->fts_name[0] == '.' && !f_listdot)
				break;

			/*
			 * If already output something, put out a newline as
			 * a separator.  If multiple arguments, precede each
			 * directory with its name.
			 */
			if (output)
				(void)printf("\n%s:\n", p->fts_path);
			else if (argc > 1) {
				(void)printf("%s:\n", p->fts_path);
				output = 1;
			}

			chp = fts_children(ftsp, ch_options);
			display(p, chp);

			if (!f_recursive && chp != NULL)
				(void)fts_set(ftsp, p, FTS_SKIP);
			break;
		}
	error = errno;
	(void)fts_close(ftsp);
	errno = error;
	if (errno)
		err(EXIT_FAILURE, "fts_read");
}

/*
 * Display() takes a linked list of FTSENT structures and passes the list
 * along with any other necessary information to the print function.  P
 * points to the parent directory of the display list.
 */
static void
display(FTSENT *p, FTSENT *list)
{
	struct stat *sp;
	DISPLAY d;
	FTSENT *cur;
	NAMES *np;
	u_int64_t btotal, stotal;
	off_t maxsize;
	blkcnt_t maxblock;
	ino_t maxinode;
	int maxmajor, maxminor;
	uint32_t maxnlink;
	int bcfile, entries, flen, glen, ulen, maxflags, maxgroup;
	unsigned int maxlen;
	int maxuser, needstats;
	const char *user, *group;
	char buf[21];		/* 64 bits == 20 digits, +1 for NUL */
	char nuser[12], ngroup[12];
	char *flags = NULL;

	/*
	 * If list is NULL there are two possibilities: that the parent
	 * directory p has no children, or that fts_children() returned an
	 * error.  We ignore the error case since it will be replicated
	 * on the next call to fts_read() on the post-order visit to the
	 * directory p, and will be signalled in traverse().
	 */
	if (list == NULL)
		return;

	needstats = f_inode || f_longform || f_size;
	flen = 0;
	maxinode = maxnlink = 0;
	bcfile = 0;
	maxuser = maxgroup = maxflags = maxlen = 0;
	btotal = stotal = maxblock = maxsize = 0;
	maxmajor = maxminor = 0;
	for (cur = list, entries = 0; cur; cur = cur->fts_link) {
		if (cur->fts_info == FTS_ERR || cur->fts_info == FTS_NS) {
			warnx("%s: %s",
			    cur->fts_name, strerror(cur->fts_errno));
			cur->fts_number = NO_PRINT;
			rval = EXIT_FAILURE;
			continue;
		}

		/*
		 * P is NULL if list is the argv list, to which different rules
		 * apply.
		 */
		if (p == NULL) {
			/* Directories will be displayed later. */
			if (cur->fts_info == FTS_D && !f_listdir) {
				cur->fts_number = NO_PRINT;
				continue;
			}
		} else {
			/* Only display dot file if -a/-A set. */
			if (cur->fts_name[0] == '.' && !f_listdot) {
				cur->fts_number = NO_PRINT;
				continue;
			}
		}
		if (cur->fts_namelen > maxlen)
			maxlen = cur->fts_namelen;
		if (needstats) {
			sp = cur->fts_statp;
			if (sp->st_blocks > maxblock)
				maxblock = sp->st_blocks;
			if (sp->st_ino > maxinode)
				maxinode = sp->st_ino;
			if (sp->st_nlink > maxnlink)
				maxnlink = sp->st_nlink;
			if (sp->st_size > maxsize)
				maxsize = sp->st_size;
			if (S_ISCHR(sp->st_mode) || S_ISBLK(sp->st_mode)) {
				bcfile = 1;
				if (major(sp->st_rdev) > maxmajor)
					maxmajor = major(sp->st_rdev);
				if (minor(sp->st_rdev) > maxminor)
					maxminor = minor(sp->st_rdev);
			}

			btotal += sp->st_blocks;
			stotal += sp->st_size;
			if (f_longform) {
				if (f_numericonly ||
				    (user = user_from_uid(sp->st_uid, 0)) ==
				    NULL) {
					(void)snprintf(nuser, sizeof(nuser),
					    "%u", sp->st_uid);
					user = nuser;
				}
				if (f_numericonly ||
				    (group = group_from_gid(sp->st_gid, 0)) ==
				    NULL) {
					(void)snprintf(ngroup, sizeof(ngroup),
					    "%u", sp->st_gid);
					group = ngroup;
				}
				if ((ulen = strlen(user)) > maxuser)
					maxuser = ulen;
				if ((glen = strlen(group)) > maxgroup)
					maxgroup = glen;
				if (f_flags) {
					flags =
					    flags_to_string((u_long)sp->st_flags, "-");
					if ((flen = strlen(flags)) > maxflags)
						maxflags = flen;
				} else
					flen = 0;

				if ((np = malloc(sizeof(NAMES) +
				    ulen + glen + flen + 2)) == NULL)
					err(EXIT_FAILURE, NULL);

				np->user = &np->data[0];
				(void)strcpy(np->user, user);
				np->group = &np->data[ulen + 1];
				(void)strcpy(np->group, group);

				if (f_flags) {
					np->flags = &np->data[ulen + glen + 2];
				  	(void)strcpy(np->flags, flags);
					free(flags);
				}
				cur->fts_pointer = np;
			}
		}
		++entries;
	}

	if (!entries)
		return;

	d.list = list;
	d.entries = entries;
	d.maxlen = maxlen;
	if (needstats) {
		d.btotal = btotal;
		d.stotal = stotal;
		if (f_humanize) {
			d.s_block = 4; /* min buf length for humanize_number */
		} else {
			(void)snprintf(buf, sizeof(buf), "%llu",
			    (long long)howmany(maxblock, blocksize));
			d.s_block = strlen(buf);
			if (f_commas) /* allow for commas before every third digit */
				d.s_block += (d.s_block - 1) / 3;
		}
		d.s_flags = maxflags;
		d.s_group = maxgroup;
		(void)snprintf(buf, sizeof(buf), "%llu",
		    (unsigned long long)maxinode);
		d.s_inode = strlen(buf);
		(void)snprintf(buf, sizeof(buf), "%u", maxnlink);
		d.s_nlink = strlen(buf);
		if (f_humanize) {
			d.s_size = 4; /* min buf length for humanize_number */
		} else {
			(void)snprintf(buf, sizeof(buf), "%llu",
			    (long long)maxsize);
			d.s_size = strlen(buf);
			if (f_commas) /* allow for commas before every third digit */
				d.s_size += (d.s_size - 1) / 3;
		}
		d.s_user = maxuser;
		if (bcfile) {
			(void)snprintf(buf, sizeof(buf), "%u", maxmajor);
			d.s_major = strlen(buf);
			(void)snprintf(buf, sizeof(buf), "%u", maxminor);
			d.s_minor = strlen(buf);
			if (d.s_major + d.s_minor + 2 > d.s_size)
				d.s_size = d.s_major + d.s_minor + 2;
			else if (d.s_size - d.s_minor - 2 > d.s_major)
				d.s_major = d.s_size - d.s_minor - 2;
		} else {
			d.s_major = 0;
			d.s_minor = 0;
		}
	}

	printfcn(&d);
	output = 1;

	if (f_longform)
		for (cur = list; cur; cur = cur->fts_link)
			free(cur->fts_pointer);
}

/*
 * Ordering for mastercmp:
 * If ordering the argv (fts_level = FTS_ROOTLEVEL) return non-directories
 * as larger than directories.  Within either group, use the sort function.
 * All other levels use the sort function.  Error entries remain unsorted.
 */
static int
mastercmp(const FTSENT **a, const FTSENT **b)
{
	int a_info, b_info;

	a_info = (*a)->fts_info;
	if (a_info == FTS_ERR)
		return (0);
	b_info = (*b)->fts_info;
	if (b_info == FTS_ERR)
		return (0);

	if (a_info == FTS_NS || b_info == FTS_NS) {
		if (b_info != FTS_NS)
			return (1);
		else if (a_info != FTS_NS)
			return (-1);
		else
			return (namecmp(*a, *b));
	}

	if (a_info != b_info && !f_listdir &&
	    (*a)->fts_level == FTS_ROOTLEVEL) {
		if (a_info == FTS_D)
			return (1);
		else if (b_info == FTS_D)
			return (-1);
	}
	return (sortfcn(*a, *b));
}

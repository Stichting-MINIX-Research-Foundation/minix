/*	$NetBSD: ftree.c,v 1.42 2012/09/27 00:44:59 christos Exp $	*/

/*-
 * Copyright (c) 1992 Keith Muller.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego.
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

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn of Wasabi Systems.
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
#if 0
static char sccsid[] = "@(#)ftree.c	8.2 (Berkeley) 4/18/94";
#else
__RCSID("$NetBSD: ftree.c,v 1.42 2012/09/27 00:44:59 christos Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <ctype.h>
#include <errno.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "pax.h"
#include "ftree.h"
#include "extern.h"
#include "options.h"
#ifndef SMALL
#include "mtree.h"
#endif	/* SMALL */

/*
 * routines to interface with the fts library function.
 *
 * file args supplied to pax are stored on a single linked list (of type FTREE)
 * and given to fts to be processed one at a time. pax "selects" files from
 * the expansion of each arg into the corresponding file tree (if the arg is a
 * directory, otherwise the node itself is just passed to pax). The selection
 * is modified by the -n and -u flags. The user is informed when a specific
 * file arg does not generate any selected files. -n keeps expanding the file
 * tree arg until one of its files is selected, then skips to the next file
 * arg. when the user does not supply the file trees as command line args to
 * pax, they are read from stdin
 */

static FTS *ftsp = NULL;		/* current FTS handle */
static int ftsopts;			/* options to be used on fts_open */
static char *farray[2];			/* array for passing each arg to fts */
static FTREE *fthead = NULL;		/* head of linked list of file args */
static FTREE *fttail = NULL;		/* tail of linked list of file args */
static FTREE *ftcur = NULL;		/* current file arg being processed */
static FTSENT *ftent = NULL;		/* current file tree entry */
static int ftree_skip;			/* when set skip to next file arg */
#ifndef SMALL
static NODE *ftnode = NULL;		/* mtree(8) specfile; used by -M */
#endif	/* SMALL */

static int ftree_arg(void);

#define	FTS_ERRNO(x)	(x)->fts_errno

/*
 * ftree_start()
 *	initialize the options passed to fts_open() during this run of pax
 *	options are based on the selection of pax options by the user
 *	fts_start() also calls fts_arg() to open the first valid file arg. We
 *	also attempt to reset directory access times when -t (tflag) is set.
 * Return:
 *	0 if there is at least one valid file arg to process, -1 otherwise
 */

int
ftree_start(void)
{

#ifndef SMALL
	/*
	 * if -M is given, the list of filenames on stdin is actually
	 * an mtree(8) specfile, so parse the specfile into a NODE *
	 * tree at ftnode, for use by next_file()
	 */
	if (Mflag) {
		if (fthead != NULL) {
			tty_warn(1,
	    "The -M flag is only supported when reading file list from stdin");
			return -1;
		}
		ftnode = spec(stdin);
		if (ftnode != NULL &&
		    (ftnode->type != F_DIR || strcmp(ftnode->name, ".") != 0)) {
			tty_warn(1,
			    "First node of specfile is not `.' directory");
			return -1;
		}
		return 0;
	}
#endif	/* SMALL */

	/*
	 * set up the operation mode of fts, open the first file arg. We must
	 * use FTS_NOCHDIR, as the user may have to open multiple archives and
	 * if fts did a chdir off into the boondocks, we may create an archive
	 * volume in an place where the user did not expect to.
	 */
	ftsopts = FTS_NOCHDIR;

	/*
	 * optional user flags that effect file traversal
	 * -H command line symlink follow only (half follow)
	 * -L follow sylinks (logical)
	 * -P do not follow sylinks (physical). This is the default.
	 * -X do not cross over mount points
	 * -t preserve access times on files read.
	 * -n select only the first member of a file tree when a match is found
	 * -d do not extract subtrees rooted at a directory arg.
	 */
	if (Lflag)
		ftsopts |= FTS_LOGICAL;
	else
		ftsopts |= FTS_PHYSICAL;
	if (Hflag)
		ftsopts |= FTS_COMFOLLOW;
	if (Xflag)
		ftsopts |= FTS_XDEV;

	if ((fthead == NULL) && ((farray[0] = malloc(PAXPATHLEN+2)) == NULL)) {
		tty_warn(1, "Unable to allocate memory for file name buffer");
		return -1;
	}

	if (ftree_arg() < 0)
		return -1;
	if (tflag && (atdir_start() < 0))
		return -1;
	return 0;
}

/*
 * ftree_add()
 *	add the arg to the linked list of files to process. Each will be
 *	processed by fts one at a time
 * Return:
 *	0 if added to the linked list, -1 if failed
 */

int
ftree_add(char *str, int isdir)
{
	FTREE *ft;
	int len;

	/*
	 * simple check for bad args
	 */
	if ((str == NULL) || (*str == '\0')) {
		tty_warn(0, "Invalid file name argument");
		return -1;
	}

	/*
	 * allocate FTREE node and add to the end of the linked list (args are
	 * processed in the same order they were passed to pax). Get rid of any
	 * trailing / the user may pass us. (watch out for / by itself).
	 */
	if ((ft = (FTREE *)malloc(sizeof(FTREE))) == NULL) {
		tty_warn(0, "Unable to allocate memory for filename");
		return -1;
	}

	if (((len = strlen(str) - 1) > 0) && (str[len] == '/'))
		str[len] = '\0';
	ft->fname = str;
	ft->refcnt = -isdir;
	ft->fow = NULL;
	if (fthead == NULL) {
		fttail = fthead = ft;
		return 0;
	}
	fttail->fow = ft;
	fttail = ft;
	return 0;
}

/*
 * ftree_sel()
 *	this entry has been selected by pax. bump up reference count and handle
 *	-n and -d processing.
 */

void
ftree_sel(ARCHD *arcn)
{
	/*
	 * set reference bit for this pattern. This linked list is only used
	 * when file trees are supplied pax as args. The list is not used when
	 * the trees are read from stdin.
	 */
	if (ftcur != NULL)
		ftcur->refcnt = 1;

	/*
	 * if -n we are done with this arg, force a skip to the next arg when
	 * pax asks for the next file in next_file().
	 * if -M we don't use fts(3), so the rest of this function is moot.
	 * if -d we tell fts only to match the directory (if the arg is a dir)
	 * and not the entire file tree rooted at that point.
	 */
	if (nflag)
		ftree_skip = 1;

	if (Mflag || !dflag || (arcn->type != PAX_DIR))
		return;

	if (ftent != NULL)
		(void)fts_set(ftsp, ftent, FTS_SKIP);
}

/*
 * ftree_chk()
 *	called at end on pax execution. Prints all those file args that did not
 *	have a selected member (reference count still 0)
 */

void
ftree_chk(void)
{
	FTREE *ft;
	int wban = 0;

	/*
	 * make sure all dir access times were reset.
	 */
	if (tflag)
		atdir_end();

	/*
	 * walk down list and check reference count. Print out those members
	 * that never had a match
	 */
	for (ft = fthead; ft != NULL; ft = ft->fow) {
		if (ft->refcnt != 0)
			continue;
		if (wban == 0) {
			tty_warn(1,
			    "WARNING! These file names were not selected:");
			++wban;
		}
		(void)fprintf(stderr, "%s\n", ft->fname);
	}
}

/*
 * ftree_arg()
 *	Get the next file arg for fts to process. Can be from either the linked
 *	list or read from stdin when the user did not them as args to pax. Each
 *	arg is processed until the first successful fts_open().
 * Return:
 *	0 when the next arg is ready to go, -1 if out of file args (or EOF on
 *	stdin).
 */

static int
ftree_arg(void)
{
	/*
	 * close off the current file tree
	 */
	if (ftsp != NULL) {
		(void)fts_close(ftsp);
		ftsp = NULL;
		ftent = NULL;
	}

	/*
	 * keep looping until we get a valid file tree to process. Stop when we
	 * reach the end of the list (or get an eof on stdin)
	 */
	for(;;) {
		if (fthead == NULL) {
			int i, c = EOF;
			/*
			 * the user didn't supply any args, get the file trees
			 * to process from stdin;
			 */
			for (i = 0; i < PAXPATHLEN + 2;) {
				c = getchar();
				if (c == EOF)
					break;
				else if (c == sep) {
					if (i != 0)
						break;
				} else
					farray[0][i++] = c;
			}
			if (i == 0)
				return -1;
			farray[0][i] = '\0';
		} else {
			/*
			 * the user supplied the file args as arguments to pax
			 */
			if (ftcur == NULL)
				ftcur = fthead;
			else if ((ftcur = ftcur->fow) == NULL)
				return -1;

			if (ftcur->refcnt < 0) {
				/*
				 * chdir entry.
				 * Change directory and retry loop.
				 */
				if (ar_dochdir(ftcur->fname))
					return (-1);
				continue;
			}
			farray[0] = ftcur->fname;
		}

		/*
		 * watch it, fts wants the file arg stored in a array of char
		 * ptrs, with the last one a null. we use a two element array
		 * and set farray[0] to point at the buffer with the file name
		 * in it. We cannot pass all the file args to fts at one shot
		 * as we need to keep a handle on which file arg generates what
		 * files (the -n and -d flags need this). If the open is
		 * successful, return a 0.
		 */
		if ((ftsp = fts_open(farray, ftsopts, NULL)) != NULL)
			break;
	}
	return 0;
}

/*
 * next_file()
 *	supplies the next file to process in the supplied archd structure.
 * Return:
 *	0 when contents of arcn have been set with the next file, -1 when done.
 */

int
next_file(ARCHD *arcn)
{
#ifndef SMALL
	static	char	curdir[PAXPATHLEN+2], curpath[PAXPATHLEN+2];
	static	int	curdirlen;

	struct stat	statbuf;
	FTSENT		Mftent;
#endif	/* SMALL */
	int		cnt;
	time_t		atime, mtime;
	char		*curlink;
#define MFTENT_DUMMY_DEV	UINT_MAX

	curlink = NULL;
#ifndef SMALL
	/*
	 * if parsing an mtree(8) specfile, build up `dummy' ftsent
	 * from specfile info, and jump below to complete setup of arcn.
	 */
	if (Mflag) {
		int	skipoptional;

 next_ftnode:
		skipoptional = 0;
		if (ftnode == NULL)		/* tree is empty */
			return (-1);

						/* get current name */
		if (snprintf(curpath, sizeof(curpath), "%s%s%s",
		    curdir, curdirlen ? "/" : "", ftnode->name)
		    >= (int)sizeof(curpath)) {
			tty_warn(1, "line %lu: %s: %s", (u_long)ftnode->lineno,
			    curdir, strerror(ENAMETOOLONG));
			return (-1);
		}
		ftnode->flags |= F_VISIT;	/* mark node visited */

						/* construct dummy FTSENT */
		Mftent.fts_path = curpath;
		Mftent.fts_statp = &statbuf;
		Mftent.fts_pointer = ftnode;
		ftent = &Mftent;
						/* look for existing file */
		if (lstat(Mftent.fts_path, &statbuf) == -1) {
			if (ftnode->flags & F_OPT)
				skipoptional = 1;

						/* missing: fake up stat info */
			memset(&statbuf, 0, sizeof(statbuf));
			statbuf.st_dev = MFTENT_DUMMY_DEV;
			statbuf.st_ino = ftnode->lineno;
			statbuf.st_size = 0;
#define NODETEST(t, m)							\
			if (!(t)) {					\
				tty_warn(1, "line %lu: %s: %s not specified", \
				    (u_long)ftnode->lineno,		\
				    ftent->fts_path, m);		\
				return -1;				\
			}
			statbuf.st_mode = nodetoino(ftnode->type);
			NODETEST(ftnode->flags & F_TYPE, "type");
			NODETEST(ftnode->flags & F_MODE, "mode");
			if (!(ftnode->flags & F_TIME))
				statbuf.st_mtime = starttime;
			NODETEST(ftnode->flags & (F_GID | F_GNAME), "group");
			NODETEST(ftnode->flags & (F_UID | F_UNAME), "user");
			if (ftnode->type == F_BLOCK || ftnode->type == F_CHAR)
				NODETEST(ftnode->flags & F_DEV,
				    "device number");
			if (ftnode->type == F_LINK)
				NODETEST(ftnode->flags & F_SLINK, "symlink");
			/* don't require F_FLAGS or F_SIZE */
#undef NODETEST
		} else {
			if (ftnode->flags & F_TYPE && nodetoino(ftnode->type)
			    != (statbuf.st_mode & S_IFMT)) {
				tty_warn(1,
			    "line %lu: %s: type mismatch: specfile %s, tree %s",
				    (u_long)ftnode->lineno, ftent->fts_path,
				    inotype(nodetoino(ftnode->type)),
				    inotype(statbuf.st_mode));
				return -1;
			}
			if (ftnode->type == F_DIR && (ftnode->flags & F_OPT))
				skipoptional = 1;
		}
		/*
		 * override settings with those from specfile
		 */
		if (ftnode->flags & F_MODE) {
			statbuf.st_mode &= ~ALLPERMS; 
			statbuf.st_mode |= (ftnode->st_mode & ALLPERMS);
		}
		if (ftnode->flags & (F_GID | F_GNAME))
			statbuf.st_gid = ftnode->st_gid;
		if (ftnode->flags & (F_UID | F_UNAME))
			statbuf.st_uid = ftnode->st_uid;
#if HAVE_STRUCT_STAT_ST_FLAGS
		if (ftnode->flags & F_FLAGS)
			statbuf.st_flags = ftnode->st_flags;
#endif
		if (ftnode->flags & F_TIME)
#if BSD4_4 && !HAVE_NBTOOL_CONFIG_H
			statbuf.st_mtimespec = ftnode->st_mtimespec;
#else
			statbuf.st_mtime = ftnode->st_mtimespec.tv_sec;
#endif
		if (ftnode->flags & F_DEV)
			statbuf.st_rdev = ftnode->st_rdev;
		if (ftnode->flags & F_SLINK)
			curlink = ftnode->slink;
				/* ignore F_SIZE */

		/*
		 * find next node
		 */
		if (ftnode->type == F_DIR && ftnode->child != NULL) {
					/* directory with unseen child */
			ftnode = ftnode->child;
			curdirlen = strlcpy(curdir, curpath, sizeof(curdir));
		} else do {
			if (ftnode->next != NULL) {
					/* next node at current level */
				ftnode = ftnode->next;
			} else {	/* move back to parent */
					/* reset time only on first cd.. */
				if (Mftent.fts_pointer == ftnode && tflag &&
				    (get_atdir(MFTENT_DUMMY_DEV, ftnode->lineno,
				    &mtime, &atime) == 0)) {
					set_ftime(ftent->fts_path,
					    mtime, atime, 1, 0);
				}
				ftnode = ftnode->parent;
				if (ftnode->parent == ftnode)
					ftnode = NULL;
				else {
					curdirlen -= strlen(ftnode->name) + 1;
					curdir[curdirlen] = '\0';
				}
			}
		} while (ftnode != NULL && ftnode->flags & F_VISIT);
		if (skipoptional)	/* skip optional entries */
			goto next_ftnode;
		goto got_ftent;
	}
#endif	/* SMALL */

	/*
	 * ftree_sel() might have set the ftree_skip flag if the user has the
	 * -n option and a file was selected from this file arg tree. (-n says
	 * only one member is matched for each pattern) ftree_skip being 1
	 * forces us to go to the next arg now.
	 */
	if (ftree_skip) {
		/*
		 * clear and go to next arg
		 */
		ftree_skip = 0;
		if (ftree_arg() < 0)
			return -1;
	}

	if (ftsp == NULL)
		return -1;
	/*
	 * loop until we get a valid file to process
	 */
	for(;;) {
		if ((ftent = fts_read(ftsp)) == NULL) {
			/*
			 * out of files in this tree, go to next arg, if none
			 * we are done
			 */
			if (ftree_arg() < 0)
				return -1;
			continue;
		}

		/*
		 * handle each type of fts_read() flag
		 */
		switch(ftent->fts_info) {
		case FTS_D:
		case FTS_DEFAULT:
		case FTS_F:
		case FTS_SL:
		case FTS_SLNONE:
			/*
			 * these are all ok
			 */
			break;
		case FTS_DP:
			/*
			 * already saw this directory. If the user wants file
			 * access times reset, we use this to restore the
			 * access time for this directory since this is the
			 * last time we will see it in this file subtree
			 * remember to force the time (this is -t on a read
			 * directory, not a created directory).
			 */
			if (!tflag || (get_atdir(
			    ftent->fts_statp->st_dev, ftent->fts_statp->st_ino,
			    &mtime, &atime) < 0))
				continue;
			set_ftime(ftent->fts_path, mtime, atime, 1, 0);
			continue;
		case FTS_DC:
			/*
			 * fts claims a file system cycle
			 */
			tty_warn(1,"File system cycle found at %s",
			    ftent->fts_path);
			continue;
		case FTS_DNR:
			syswarn(1, FTS_ERRNO(ftent),
			    "Unable to read directory %s", ftent->fts_path);
			continue;
		case FTS_ERR:
			syswarn(1, FTS_ERRNO(ftent),
			    "File system traversal error");
			continue;
		case FTS_NS:
		case FTS_NSOK:
			syswarn(1, FTS_ERRNO(ftent),
			    "Unable to access %s", ftent->fts_path);
			continue;
		}

#ifndef SMALL
 got_ftent:
#endif	/* SMALL */
		/*
		 * ok got a file tree node to process. copy info into arcn
		 * structure (initialize as required)
		 */
		arcn->skip = 0;
		arcn->pad = 0;
		arcn->ln_nlen = 0;
		arcn->ln_name[0] = '\0';
		arcn->sb = *(ftent->fts_statp);

		/*
		 * file type based set up and copy into the arcn struct
		 * SIDE NOTE:
		 * we try to reset the access time on all files and directories
		 * we may read when the -t flag is specified. files are reset
		 * when we close them after copying. we reset the directories
		 * when we are done with their file tree (we also clean up at
		 * end in case we cut short a file tree traversal). However
		 * there is no way to reset access times on symlinks.
		 */
		switch(S_IFMT & arcn->sb.st_mode) {
		case S_IFDIR:
			arcn->type = PAX_DIR;
			if (!tflag)
				break;
			add_atdir(ftent->fts_path, arcn->sb.st_dev,
			    arcn->sb.st_ino, arcn->sb.st_mtime,
			    arcn->sb.st_atime);
			break;
		case S_IFCHR:
			arcn->type = PAX_CHR;
			break;
		case S_IFBLK:
			arcn->type = PAX_BLK;
			break;
		case S_IFREG:
			/*
			 * only regular files with have data to store on the
			 * archive. all others will store a zero length skip.
			 * the skip field is used by pax for actual data it has
			 * to read (or skip over).
			 */
			arcn->type = PAX_REG;
			arcn->skip = arcn->sb.st_size;
			break;
		case S_IFLNK:
			arcn->type = PAX_SLK;
			if (curlink != NULL) {
				cnt = strlcpy(arcn->ln_name, curlink,
				    sizeof(arcn->ln_name));
			/*
			 * have to read the symlink path from the file
			 */
			} else if ((cnt =
			    readlink(ftent->fts_path, arcn->ln_name,
			    sizeof(arcn->ln_name) - 1)) < 0) {
				syswarn(1, errno, "Unable to read symlink %s",
				    ftent->fts_path);
				continue;
			}
			/*
			 * set link name length, watch out readlink does not
			 * always null terminate the link path
			 */
			arcn->ln_name[cnt] = '\0';
			arcn->ln_nlen = cnt;
			break;
#ifdef S_IFSOCK
		case S_IFSOCK:
			/*
			 * under BSD storing a socket is senseless but we will
			 * let the format specific write function make the
			 * decision of what to do with it.
			 */
			arcn->type = PAX_SCK;
			break;
#endif
		case S_IFIFO:
			arcn->type = PAX_FIF;
			break;
		}
		break;
	}

	/*
	 * copy file name, set file name length
	 */
	arcn->nlen = strlcpy(arcn->name, ftent->fts_path, sizeof(arcn->name));
	arcn->org_name = arcn->fts_name;
	strlcpy(arcn->fts_name, ftent->fts_path, sizeof arcn->fts_name);
	if (strcmp(NM_CPIO, argv0) == 0) {
		/*
		 * cpio does *not* descend directories listed in the
		 * arguments, unlike pax/tar, so needs special handling
		 * here.  failure to do so results in massive amounts
		 * of duplicated files in the output. We kill fts after
		 * the first name is extracted, what a waste.
		 */
		ftcur->refcnt = 1;
		(void)ftree_arg();
	}
	return 0;
}

/*	$NetBSD: print.c,v 1.51 2012/06/29 12:51:38 yamt Exp $	*/

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
#if 0
static char sccsid[] = "@(#)print.c	8.5 (Berkeley) 7/28/94";
#else
__RCSID("$NetBSD: print.c,v 1.51 2012/06/29 12:51:38 yamt Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fts.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>
#include <util.h>

#include "ls.h"
#include "extern.h"

extern int termwidth;

static int	printaname(FTSENT *, int, int);
static void	printlink(FTSENT *);
static void	printtime(time_t);
static void	printtotal(DISPLAY *dp);
static int	printtype(u_int);

static time_t	now;

#define	IS_NOPRINT(p)	((p)->fts_number == NO_PRINT)

void
printscol(DISPLAY *dp)
{
	FTSENT *p;

	for (p = dp->list; p; p = p->fts_link) {
		if (IS_NOPRINT(p))
			continue;
		(void)printaname(p, dp->s_inode, dp->s_block);
		(void)putchar('\n');
	}
}

void
printlong(DISPLAY *dp)
{
	struct stat *sp;
	FTSENT *p;
	NAMES *np;
	char buf[20], szbuf[5];

	now = time(NULL);

	printtotal(dp);		/* "total: %u\n" */
	
	for (p = dp->list; p; p = p->fts_link) {
		if (IS_NOPRINT(p))
			continue;
		sp = p->fts_statp;
		if (f_inode)
			(void)printf("%*lu ", dp->s_inode,
			    (unsigned long)sp->st_ino);
		if (f_size) {
			if (f_humanize) {
				if ((humanize_number(szbuf, sizeof(szbuf),
				    sp->st_blocks * S_BLKSIZE,
				    "", HN_AUTOSCALE,
				    (HN_DECIMAL | HN_B | HN_NOSPACE))) == -1)
					err(1, "humanize_number");
				(void)printf("%*s ", dp->s_block, szbuf);
			} else {
				(void)printf(f_commas ? "%'*llu " : "%*llu ",
				    dp->s_block,
				    (unsigned long long)howmany(sp->st_blocks,
				    blocksize));
			}
		}
		(void)strmode(sp->st_mode, buf);
		np = p->fts_pointer;
		(void)printf("%s %*lu ", buf, dp->s_nlink,
		    (unsigned long)sp->st_nlink);
		if (!f_grouponly)
			(void)printf("%-*s  ", dp->s_user, np->user);
		(void)printf("%-*s  ", dp->s_group, np->group);
		if (f_flags)
			(void)printf("%-*s ", dp->s_flags, np->flags);
		if (S_ISCHR(sp->st_mode) || S_ISBLK(sp->st_mode))
			(void)printf("%*lld, %*lld ",
			    dp->s_major, (long long)major(sp->st_rdev),
			    dp->s_minor, (long long)minor(sp->st_rdev));
		else
			if (f_humanize) {
				if ((humanize_number(szbuf, sizeof(szbuf),
				    sp->st_size, "", HN_AUTOSCALE,
				    (HN_DECIMAL | HN_B | HN_NOSPACE))) == -1)
					err(1, "humanize_number");
				(void)printf("%*s ", dp->s_size, szbuf);
			} else {
				(void)printf(f_commas ? "%'*llu " : "%*llu ", 
				    dp->s_size, (unsigned long long)
				    sp->st_size);
			}
		if (f_accesstime)
			printtime(sp->st_atime);
		else if (f_statustime)
			printtime(sp->st_ctime);
		else
			printtime(sp->st_mtime);
		if (f_octal || f_octal_escape)
			(void)safe_print(p->fts_name);
		else if (f_nonprint)
			(void)printescaped(p->fts_name);
		else
			(void)printf("%s", p->fts_name);

		if (f_type || (f_typedir && S_ISDIR(sp->st_mode)))
			(void)printtype(sp->st_mode);
		if (S_ISLNK(sp->st_mode))
			printlink(p);
		(void)putchar('\n');
	}
}

void
printcol(DISPLAY *dp)
{
	static FTSENT **array;
	static int lastentries = -1;
	FTSENT *p;
	int base, chcnt, col, colwidth, num;
	int numcols, numrows, row;

	colwidth = dp->maxlen;
	if (f_inode)
		colwidth += dp->s_inode + 1;
	if (f_size) {
		if (f_humanize)
			colwidth += dp->s_size + 1;
		else
			colwidth += dp->s_block + 1;
	}
	if (f_type || f_typedir)
		colwidth += 1;

	colwidth += 1;

	if (termwidth < 2 * colwidth) {
		printscol(dp);
		return;
	}

	/*
	 * Have to do random access in the linked list -- build a table
	 * of pointers.
	 */
	if (dp->entries > lastentries) {
		FTSENT **newarray;

		newarray = realloc(array, dp->entries * sizeof(FTSENT *));
		if (newarray == NULL) {
			warn(NULL);
			printscol(dp);
			return;
		}
		lastentries = dp->entries;
		array = newarray;
	}
	for (p = dp->list, num = 0; p; p = p->fts_link)
		if (p->fts_number != NO_PRINT)
			array[num++] = p;

	numcols = termwidth / colwidth;
	colwidth = termwidth / numcols;		/* spread out if possible */
	numrows = num / numcols;
	if (num % numcols)
		++numrows;

	printtotal(dp);				/* "total: %u\n" */

	for (row = 0; row < numrows; ++row) {
		for (base = row, chcnt = col = 0; col < numcols; ++col) {
			chcnt = printaname(array[base], dp->s_inode,
			    f_humanize ? dp->s_size : dp->s_block);
			if ((base += numrows) >= num)
				break;
			while (chcnt++ < colwidth)
				(void)putchar(' ');
		}
		(void)putchar('\n');
	}
}

void
printacol(DISPLAY *dp)
{
	FTSENT *p;
	int chcnt, col, colwidth;
	int numcols;

	colwidth = dp->maxlen;
	if (f_inode)
		colwidth += dp->s_inode + 1;
	if (f_size) {
		if (f_humanize)
			colwidth += dp->s_size + 1;
		else
			colwidth += dp->s_block + 1;
	}
	if (f_type || f_typedir)
		colwidth += 1;

	colwidth += 1;

	if (termwidth < 2 * colwidth) {
		printscol(dp);
		return;
	}

	numcols = termwidth / colwidth;
	colwidth = termwidth / numcols;		/* spread out if possible */

	printtotal(dp);				/* "total: %u\n" */

	chcnt = col = 0;
	for (p = dp->list; p; p = p->fts_link) {
		if (IS_NOPRINT(p))
			continue;
		if (col >= numcols) {
			chcnt = col = 0;
			(void)putchar('\n');
		}
		chcnt = printaname(p, dp->s_inode,
		    f_humanize ? dp->s_size : dp->s_block);
		while (chcnt++ < colwidth)
			(void)putchar(' ');
		col++;
	}
	(void)putchar('\n');
}

void
printstream(DISPLAY *dp)
{
	FTSENT *p;
	int col;
	int extwidth;

	extwidth = 0;
	if (f_inode)
		extwidth += dp->s_inode + 1;
	if (f_size) {
		if (f_humanize)
			extwidth += dp->s_size + 1;
		else 
			extwidth += dp->s_block + 1;
	}
	if (f_type)
		extwidth += 1;

	for (col = 0, p = dp->list; p != NULL; p = p->fts_link) {
		if (IS_NOPRINT(p))
			continue;
		if (col > 0) {
			(void)putchar(','), col++;
			if (col + 1 + extwidth + (int)p->fts_namelen >= termwidth)
				(void)putchar('\n'), col = 0;
			else
				(void)putchar(' '), col++;
		}
		col += printaname(p, dp->s_inode,
		    f_humanize ? dp->s_size : dp->s_block);
	}
	(void)putchar('\n');
}

/*
 * print [inode] [size] name
 * return # of characters printed, no trailing characters.
 */
static int
printaname(FTSENT *p, int inodefield, int sizefield)
{
	struct stat *sp;
	int chcnt;
	char szbuf[5];

	sp = p->fts_statp;
	chcnt = 0;
	if (f_inode)
		chcnt += printf("%*lu ", inodefield, (unsigned long)sp->st_ino);
	if (f_size) {
		if (f_humanize) {
			if ((humanize_number(szbuf, sizeof(szbuf), sp->st_size,
			    "", HN_AUTOSCALE,
			    (HN_DECIMAL | HN_B | HN_NOSPACE))) == -1)
				err(1, "humanize_number");
			chcnt += printf("%*s ", sizefield, szbuf);
		} else {
			chcnt += printf(f_commas ? "%'*llu " : "%*llu ",
			    sizefield, (unsigned long long)
			    howmany(sp->st_blocks, blocksize));
		}
	}
	if (f_octal || f_octal_escape)
		chcnt += safe_print(p->fts_name);
	else if (f_nonprint)
		chcnt += printescaped(p->fts_name);
	else
		chcnt += printf("%s", p->fts_name);
	if (f_type || (f_typedir && S_ISDIR(sp->st_mode)))
		chcnt += printtype(sp->st_mode);
	return (chcnt);
}

static void
printtime(time_t ftime)
{
	int i;
	const char *longstring;

	if ((longstring = ctime(&ftime)) == NULL) {
			   /* 012345678901234567890123 */
		longstring = "????????????????????????";
	}
	for (i = 4; i < 11; ++i)
		(void)putchar(longstring[i]);

#define	SIXMONTHS	((DAYSPERNYEAR / 2) * SECSPERDAY)
	if (f_sectime)
		for (i = 11; i < 24; i++)
			(void)putchar(longstring[i]);
	else if (ftime + SIXMONTHS > now && ftime - SIXMONTHS < now)
		for (i = 11; i < 16; ++i)
			(void)putchar(longstring[i]);
	else {
		(void)putchar(' ');
		for (i = 20; i < 24; ++i)
			(void)putchar(longstring[i]);
	}
	(void)putchar(' ');
}

/*
 * Display total used disk space in the form "total: %u\n".
 * Note: POSIX (IEEE Std 1003.1-2001) says this should be always in 512 blocks,
 * but we humanise it with -h, or separate it with commas with -M, and use 1024
 * with -k.
 */
static void
printtotal(DISPLAY *dp)
{
	char szbuf[5];
	
	if (dp->list->fts_level != FTS_ROOTLEVEL && (f_longform || f_size)) {
		if (f_humanize) {
			if ((humanize_number(szbuf, sizeof(szbuf), (int64_t)dp->stotal,
			    "", HN_AUTOSCALE,
			    (HN_DECIMAL | HN_B | HN_NOSPACE))) == -1)
				err(1, "humanize_number");
			(void)printf("total %s\n", szbuf);
		} else {
			(void)printf(f_commas ? "total %'llu\n" :
			    "total %llu\n", (unsigned long long)
			    howmany(dp->btotal, blocksize));
		}
	}
}

static int
printtype(u_int mode)
{
	switch (mode & S_IFMT) {
	case S_IFDIR:
		(void)putchar('/');
		return (1);
	case S_IFIFO:
		(void)putchar('|');
		return (1);
	case S_IFLNK:
		(void)putchar('@');
		return (1);
	case S_IFSOCK:
		(void)putchar('=');
		return (1);
	case S_IFWHT:
		(void)putchar('%');
		return (1);
	}
	if (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
		(void)putchar('*');
		return (1);
	}
	return (0);
}

static void
printlink(FTSENT *p)
{
	int lnklen;
	char name[MAXPATHLEN + 1], path[MAXPATHLEN + 1];

	if (p->fts_level == FTS_ROOTLEVEL)
		(void)snprintf(name, sizeof(name), "%s", p->fts_name);
	else
		(void)snprintf(name, sizeof(name),
		    "%s/%s", p->fts_parent->fts_accpath, p->fts_name);
	if ((lnklen = readlink(name, path, sizeof(path) - 1)) == -1) {
		(void)fprintf(stderr, "\nls: %s: %s\n", name, strerror(errno));
		return;
	}
	path[lnklen] = '\0';
	(void)printf(" -> ");
	if (f_octal || f_octal_escape)
		(void)safe_print(path);
	else if (f_nonprint)
		(void)printescaped(path);
	else
		(void)printf("%s", path);
}


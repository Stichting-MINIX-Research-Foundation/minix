/*	$NetBSD: du.c,v 1.36 2012/03/11 11:23:20 shattered Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Newcomb.
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
static char sccsid[] = "@(#)du.c	8.5 (Berkeley) 5/4/95";
#else
__RCSID("$NetBSD: du.c,v 1.36 2012/03/11 11:23:20 shattered Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <inttypes.h>
#include <util.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

/* Count inodes or file size */
#define	COUNT	(iflag ? 1 : p->fts_statp->st_blocks)

static int	linkchk(dev_t, ino_t);
static void	prstat(const char *, int64_t);
__dead static void	usage(void);

static int hflag, iflag;
static long blocksize;

int
main(int argc, char *argv[])
{
	FTS *fts;
	FTSENT *p;
	int64_t totalblocks;
	int ftsoptions, listfiles;
	int depth;
	int Hflag, Lflag, aflag, ch, cflag, dflag, gkmflag, nflag, rval, sflag;
	const char *noargv[2];

	Hflag = Lflag = aflag = cflag = dflag = gkmflag = nflag = sflag = 0;
	totalblocks = 0;
	ftsoptions = FTS_PHYSICAL;
	depth = INT_MAX;
	while ((ch = getopt(argc, argv, "HLPacd:ghikmnrsx")) != -1)
		switch (ch) {
		case 'H':
			Hflag = 1;
			Lflag = 0;
			break;
		case 'L':
			Lflag = 1;
			Hflag = 0;
			break;
		case 'P':
			Hflag = Lflag = 0;
			break;
		case 'a':
			aflag = 1;
			break;
		case 'c':
			cflag = 1;
			break;
		case 'd':
			dflag = 1;
			depth = atoi(optarg);
			if (depth < 0 || depth > SHRT_MAX) {
				warnx("invalid argument to option d: %s", 
					optarg);
				usage();
			}
			break;
		case 'g':
			blocksize = 1024 * 1024 * 1024;
			gkmflag = 1;
			break;
		case 'h':
			hflag = 1;
			break;
		case 'i':
			iflag = 1;
			break;
		case 'k':
			blocksize = 1024;
			gkmflag = 1;
			break;
		case 'm':
			blocksize = 1024 * 1024;
			gkmflag = 1;
			break; 
		case 'n':
			nflag = 1;
			break;
		case 'r':
			break;
		case 's':
			sflag = 1;
			break;
		case 'x':
			ftsoptions |= FTS_XDEV;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/*
	 * XXX
	 * Because of the way that fts(3) works, logical walks will not count
	 * the blocks actually used by symbolic links.  We rationalize this by
	 * noting that users computing logical sizes are likely to do logical
	 * copies, so not counting the links is correct.  The real reason is
	 * that we'd have to re-implement the kernel's symbolic link traversing
	 * algorithm to get this right.  If, for example, you have relative
	 * symbolic links referencing other relative symbolic links, it gets
	 * very nasty, very fast.  The bottom line is that it's documented in
	 * the man page, so it's a feature.
	 */
	if (Hflag)
		ftsoptions |= FTS_COMFOLLOW;
	if (Lflag) {
		ftsoptions &= ~FTS_PHYSICAL;
		ftsoptions |= FTS_LOGICAL;
	}

	listfiles = 0;
	if (aflag) {
		if (sflag || dflag)
			usage();
		listfiles = 1;
	} else if (sflag) {
		if (dflag)
			usage();
		depth = 0;
	}

	if (!*argv) {
		noargv[0] = ".";
		noargv[1] = NULL;
		argv = __UNCONST(noargv);
	}

	if (!gkmflag)
		(void)getbsize(NULL, &blocksize);
	blocksize /= 512;

	if ((fts = fts_open(argv, ftsoptions, NULL)) == NULL)
		err(1, "fts_open `%s'", *argv);

	for (rval = 0; (p = fts_read(fts)) != NULL;) {
		if (nflag) {
			switch (p->fts_info) {
			case FTS_NS:
			case FTS_SLNONE:
				/* nothing */
				break;
			default:
				if (p->fts_statp->st_flags & UF_NODUMP) {
					fts_set(fts, p, FTS_SKIP);
					continue;
				}
			}
		}
		switch (p->fts_info) {
		case FTS_D:			/* Ignore. */
			break;
		case FTS_DP:
			p->fts_parent->fts_number += 
			    p->fts_number += COUNT;
			if (cflag)
				totalblocks += COUNT;
			/*
			 * If listing each directory, or not listing files
			 * or directories and this is post-order of the
			 * root of a traversal, display the total.
			 */
			if (p->fts_level <= depth
			    || (!listfiles && !p->fts_level))
				prstat(p->fts_path, p->fts_number);
			break;
		case FTS_DC:			/* Ignore. */
			break;
		case FTS_DNR:			/* Warn, continue. */
		case FTS_ERR:
		case FTS_NS:
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			break;
		default:
			if (p->fts_statp->st_nlink > 1 &&
			    linkchk(p->fts_statp->st_dev, p->fts_statp->st_ino))
				break;
			/*
			 * If listing each file, or a non-directory file was
			 * the root of a traversal, display the total.
			 */
			if (listfiles || !p->fts_level)
				prstat(p->fts_path, COUNT);
			p->fts_parent->fts_number += COUNT;
			if (cflag)
				totalblocks += COUNT;
		}
	}
	if (errno)
		err(1, "fts_read");
	if (cflag)
		prstat("total", totalblocks);
	exit(rval);
}

static void
prstat(const char *fname, int64_t blocks)
{
	if (iflag) {
		(void)printf("%" PRId64 "\t%s\n", blocks, fname);
		return;
	}

	if (hflag) {
		char buf[5];
		int64_t sz = blocks * 512;

		humanize_number(buf, sizeof(buf), sz, "", HN_AUTOSCALE,
		    HN_B | HN_NOSPACE | HN_DECIMAL);

		(void)printf("%s\t%s\n", buf, fname);
	} else
		(void)printf("%" PRId64 "\t%s\n",
		    howmany(blocks, (int64_t)blocksize),
		    fname);
}

static int
linkchk(dev_t dev, ino_t ino)
{
	static struct entry {
		dev_t	dev;
		ino_t	ino;
	} *htable;
	static int htshift;  /* log(allocated size) */
	static int htmask;   /* allocated size - 1 */
	static int htused;   /* 2*number of insertions */
	static int sawzero;  /* Whether zero is in table or not */
	int h, h2;
	uint64_t tmp;
	/* this constant is (1<<64)/((1+sqrt(5))/2)
	 * aka (word size)/(golden ratio)
	 */
	const uint64_t HTCONST = 11400714819323198485ULL;
	const int HTBITS = CHAR_BIT * sizeof(tmp);

	/* Never store zero in hashtable */
	if (dev == 0 && ino == 0) {
		h = sawzero;
		sawzero = 1;
		return h;
	}

	/* Extend hash table if necessary, keep load under 0.5 */
	if (htused<<1 >= htmask) {
		struct entry *ohtable;

		if (!htable)
			htshift = 10;   /* starting hashtable size */
		else
			htshift++;   /* exponential hashtable growth */

		htmask  = (1 << htshift) - 1;
		htused = 0;

		ohtable = htable;
		htable = calloc(htmask+1, sizeof(*htable));
		if (!htable)
			err(1, "calloc");

		/* populate newly allocated hashtable */
		if (ohtable) {
			int i;
			for (i = 0; i <= htmask>>1; i++)
				if (ohtable[i].ino || ohtable[i].dev)
					linkchk(ohtable[i].dev, ohtable[i].ino);
			free(ohtable);
		}
	}

	/* multiplicative hashing */
	tmp = dev;
	tmp <<= HTBITS>>1;
	tmp |=  ino;
	tmp *= HTCONST;
	h  = tmp >> (HTBITS - htshift);
	h2 = 1 | ( tmp >> (HTBITS - (htshift<<1) - 1)); /* must be odd */

	/* open address hashtable search with double hash probing */
	while (htable[h].ino || htable[h].dev) {
		if ((htable[h].ino == ino) && (htable[h].dev == dev))
			return 1;
		h = (h + h2) & htmask;
	}

	/* Insert the current entry into hashtable */
	htable[h].dev = dev;
	htable[h].ino = ino;
	htused++;
	return 0;
}

static void
usage(void)
{

	(void)fprintf(stderr,
		"usage: du [-H | -L | -P] [-a | -d depth | -s] [-cghikmnrx] [file ...]\n");
	exit(1);
}

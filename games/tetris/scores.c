/*	$NetBSD: scores.c,v 1.22 2014/03/22 19:05:30 dholland Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek and Darren F. Provine.
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
 *
 *	@(#)scores.c	8.1 (Berkeley) 5/31/93
 */

/*
 * Score code for Tetris, by Darren Provine (kilroy@gboro.glassboro.edu)
 * modified 22 January 1992, to limit the number of entries any one
 * person has.
 *
 * Major whacks since then.
 */
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <term.h>
#include <unistd.h>

#include "pathnames.h"
#include "screen.h"
#include "scores.h"
#include "tetris.h"

/*
 * Allow updating the high scores unless we're built as part of /rescue.
 */
#ifndef RESCUEDIR
#define ALLOW_SCORE_UPDATES
#endif

/*
 * Within this code, we can hang onto one extra "high score", leaving
 * room for our current score (whether or not it is high).
 *
 * We also sometimes keep tabs on the "highest" score on each level.
 * As long as the scores are kept sorted, this is simply the first one at
 * that level.
 */
#define NUMSPOTS (MAXHISCORES + 1)
#define	NLEVELS (MAXLEVEL + 1)

static time_t now;
static int nscores;
static int gotscores;
static struct highscore scores[NUMSPOTS];

static int checkscores(struct highscore *, int);
static int cmpscores(const void *, const void *);
static void getscores(int *);
static void printem(int, int, struct highscore *, int, const char *);
static char *thisuser(void);

/* contents chosen to be a highly illegal username */
static const char hsh_magic_val[HSH_MAGIC_SIZE] = "//:\0\0://";

#define HSH_ENDIAN_NATIVE	0x12345678
#define HSH_ENDIAN_OPP		0x78563412

/* current file format version */
#define HSH_VERSION		1

/* codes for scorefile_probe return */
#define SCOREFILE_ERROR		(-1)
#define SCOREFILE_CURRENT	0	/* 40-byte */
#define SCOREFILE_CURRENT_OPP	1	/* 40-byte, opposite-endian */
#define SCOREFILE_599		2	/* 36-byte */
#define SCOREFILE_599_OPP	3	/* 36-byte, opposite-endian */
#define SCOREFILE_50		4	/* 32-byte */
#define SCOREFILE_50_OPP	5	/* 32-byte, opposite-endian */

/*
 * Check (or guess) what kind of score file contents we have.
 */
static int
scorefile_probe(int sd)
{
	struct stat st;
	int t1, t2, t3, tx;
	ssize_t result;
	uint32_t numbers[3], offset56, offset60, offset64;

	if (fstat(sd, &st) < 0) {
		warn("Score file %s: fstat", _PATH_SCOREFILE);
		return -1;
	}

	t1 = st.st_size % sizeof(struct highscore_ondisk) == 0;
	t2 = st.st_size % sizeof(struct highscore_ondisk_599) == 0;
	t3 = st.st_size % sizeof(struct highscore_ondisk_50) == 0;
	tx = t1 + t2 + t3;
	if (tx == 1) {
		/* Size matches exact number of one kind of records */
		if (t1) {
			return SCOREFILE_CURRENT;
		} else if (t2) {
			return SCOREFILE_599;
		} else {
			return SCOREFILE_50;
		}
	} else if (tx == 0) {
		/* Size matches nothing, pick most likely as default */
		goto wildguess;
	}

	/*
	 * File size is multiple of more than one structure size.
	 * (For example, 288 bytes could be 9*hso50 or 8*hso599.)
	 * Read the file and see if we can figure out what's going
	 * on. This is the layout of the first two records:
	 *
	 *   offset     hso / current   hso_599         hso_50
	 *              (40-byte)       (36-byte)       (32-byte)
	 *
	 *   0          name #0         name #0         name #0
         *   4            :               :               :
         *   8            :               :               :
         *   12           :               :               :
         *   16           :               :               :
	 *   20         score #0        score #0        score #0
	 *   24         level #0        level #0        level #0
	 *   28          (pad)          time #0         time #0
	 *   32         time #0                         name #1
	 *   36                         name #1           :
         *   40         name #1           :               :
         *   44           :               :               :
         *   48           :               :               :
	 *   52           :               :             score #1
	 *   56           :             score #1        level #1
	 *   60         score #1        level #1        time #1
	 *   64         level #1        time #1         name #2
	 *   68          (pad)            :               :
	 *   72         time #1         name #2           :
	 *   76           :               :               :
	 *   80                  --- end ---
	 *
	 * There are a number of things we could check here, but the
	 * most effective test is based on the following restrictions:
	 *
	 *    - The level must be between 1 and 9 (inclusive)
	 *    - All times must be after 1985 and are before 2038,
	 *      so the high word must be 0 and the low word may not be
	 *      a small value.
	 *    - Integer values of 0 or 1-9 cannot be the beginning of
	 *      a login name string.
	 *    - Values of 1-9 are probably not a score.
	 *
	 * So we read the three words at offsets 56, 60, and 64, and
	 * poke at the values to try to figure things...
	 */

	if (lseek(sd, 56, SEEK_SET) < 0) {
		warn("Score file %s: lseek", _PATH_SCOREFILE);
		return -1;
	}
	result = read(sd, &numbers, sizeof(numbers));
	if (result < 0) {
		warn("Score file %s: read", _PATH_SCOREFILE);
		return -1;
	}
	if ((size_t)result != sizeof(numbers)) {
		/*
		 * The smallest file whose size divides by more than
		 * one of the sizes is substantially larger than 64,
		 * so this should *never* happen.
		 */
		warnx("Score file %s: Unexpected EOF", _PATH_SCOREFILE);
		return -1;
	}

	offset56 = numbers[0];
	offset60 = numbers[1];
	offset64 = numbers[2];

	if (offset64 >= MINLEVEL && offset64 <= MAXLEVEL) {
		/* 40-byte structure */
		return SCOREFILE_CURRENT;
	} else if (offset60 >= MINLEVEL && offset60 <= MAXLEVEL) {
		/* 36-byte structure */
		return SCOREFILE_599;
	} else if (offset56 >= MINLEVEL && offset56 <= MAXLEVEL) {
		/* 32-byte structure */
		return SCOREFILE_50;
	}

	/* None was a valid level; try opposite endian */
	offset64 = bswap32(offset64);
	offset60 = bswap32(offset60);
	offset56 = bswap32(offset56);

	if (offset64 >= MINLEVEL && offset64 <= MAXLEVEL) {
		/* 40-byte structure */
		return SCOREFILE_CURRENT_OPP;
	} else if (offset60 >= MINLEVEL && offset60 <= MAXLEVEL) {
		/* 36-byte structure */
		return SCOREFILE_599_OPP;
	} else if (offset56 >= MINLEVEL && offset56 <= MAXLEVEL) {
		/* 32-byte structure */
		return SCOREFILE_50_OPP;
	}

	/* That didn't work either, dunno what's going on */
 wildguess:
	warnx("Score file %s is likely corrupt", _PATH_SCOREFILE);
	if (sizeof(void *) == 8 && sizeof(time_t) == 8) {
		return SCOREFILE_CURRENT;
	} else if (sizeof(time_t) == 8) {
		return SCOREFILE_599;
	} else {
		return SCOREFILE_50;
	}
}

/*
 * Copy a string safely, making sure it's null-terminated.
 */
static void
readname(char *to, size_t maxto, const char *from, size_t maxfrom)
{
	size_t amt;

	amt = maxto < maxfrom ? maxto : maxfrom;
	memcpy(to, from, amt);
	to[maxto-1] = '\0';
}

/*
 * Copy integers, byte-swapping if desired.
 */
static int32_t
read32(int32_t val, int doflip)
{
	if (doflip) {
		val = bswap32(val);
	}
	return val;
}

static int64_t
read64(int64_t val, int doflip)
{
	if (doflip) {
		val = bswap64(val);
	}
	return val;
}

/*
 * Read up to MAXHISCORES scorefile_ondisk entries.
 */
static int
readscores(int sd, int doflip)
{
	struct highscore_ondisk buf[MAXHISCORES];
	ssize_t result;
	int i;

	result = read(sd, buf, sizeof(buf));
	if (result < 0) {
		warn("Score file %s: read", _PATH_SCOREFILE);
		return -1;
	}
	nscores = result / sizeof(buf[0]);

	for (i=0; i<nscores; i++) {
		readname(scores[i].hs_name, sizeof(scores[i].hs_name),
			 buf[i].hso_name, sizeof(buf[i].hso_name));
		scores[i].hs_score = read32(buf[i].hso_score, doflip);
		scores[i].hs_level = read32(buf[i].hso_level, doflip);
		scores[i].hs_time = read64(buf[i].hso_time, doflip);
	}
	return 0;
}

/*
 * Read up to MAXHISCORES scorefile_ondisk_599 entries.
 */
static int
readscores599(int sd, int doflip)
{
	struct highscore_ondisk_599 buf[MAXHISCORES];
	ssize_t result;
	int i;

	result = read(sd, buf, sizeof(buf));
	if (result < 0) {
		warn("Score file %s: read", _PATH_SCOREFILE);
		return -1;
	}
	nscores = result / sizeof(buf[0]);

	for (i=0; i<nscores; i++) {
		readname(scores[i].hs_name, sizeof(scores[i].hs_name),
			 buf[i].hso599_name, sizeof(buf[i].hso599_name));
		scores[i].hs_score = read32(buf[i].hso599_score, doflip);
		scores[i].hs_level = read32(buf[i].hso599_level, doflip);
		/*
		 * Don't bother pasting the time together into a
		 * 64-bit value; just take whichever half is nonzero.
		 */
		scores[i].hs_time =
			read32(buf[i].hso599_time[buf[i].hso599_time[0] == 0],
			       doflip);
	}
	return 0;
}

/*
 * Read up to MAXHISCORES scorefile_ondisk_50 entries.
 */
static int
readscores50(int sd, int doflip)
{
	struct highscore_ondisk_50 buf[MAXHISCORES];
	ssize_t result;
	int i;

	result = read(sd, buf, sizeof(buf));
	if (result < 0) {
		warn("Score file %s: read", _PATH_SCOREFILE);
		return -1;
	}
	nscores = result / sizeof(buf[0]);

	for (i=0; i<nscores; i++) {
		readname(scores[i].hs_name, sizeof(scores[i].hs_name),
			 buf[i].hso50_name, sizeof(buf[i].hso50_name));
		scores[i].hs_score = read32(buf[i].hso50_score, doflip);
		scores[i].hs_level = read32(buf[i].hso50_level, doflip);
		scores[i].hs_time = read32(buf[i].hso50_time, doflip);
	}
	return 0;
}

/*
 * Read the score file.  Can be called from savescore (before showscores)
 * or showscores (if savescore will not be called).  If the given pointer
 * is not NULL, sets *fdp to an open file handle that corresponds to a
 * read/write score file that is locked with LOCK_EX.  Otherwise, the
 * file is locked with LOCK_SH for the read and closed before return.
 */
static void
getscores(int *fdp)
{
	struct highscore_header header;
	int sd, mint, lck;
	mode_t mask;
	const char *human;
	int doflip;
	int serrno;
	ssize_t result;

#ifdef ALLOW_SCORE_UPDATES
	if (fdp != NULL) {
		mint = O_RDWR | O_CREAT;
		human = "read/write";
		lck = LOCK_EX;
	} else
#endif
	{
		mint = O_RDONLY;
		human = "reading";
		lck = LOCK_SH;
	}
	setegid(egid);
	mask = umask(S_IWOTH);
	sd = open(_PATH_SCOREFILE, mint, 0666);
	serrno = errno;
	(void)umask(mask);
	setegid(gid);
	if (sd < 0) {
		/*
		 * If the file simply isn't there because nobody's
		 * played yet, and we aren't going to be trying to
		 * update it, don't warn. Even if we are going to be
		 * trying to write it, don't fail -- we can still show
		 * the player the score they got.
		 */
		errno = serrno;
		if (fdp != NULL || errno != ENOENT) {
			warn("Cannot open %s for %s", _PATH_SCOREFILE, human);
		}
		goto fail;
	}

	/*
	 * Grab a lock.
	 * XXX: failure here should probably be more fatal than this.
	 */
	if (flock(sd, lck))
		warn("warning: score file %s cannot be locked",
		    _PATH_SCOREFILE);

	/*
	 * The current format (since -current of 20090525) is
	 *
	 *    struct highscore_header
	 *    up to MAXHIGHSCORES x struct highscore_ondisk
	 *
	 * Before this, there is no header, and the contents
	 * might be any of three formats:
	 *
	 *    highscore_ondisk       (64-bit machines with 64-bit time_t)
	 *    highscore_ondisk_599   (32-bit machines with 64-bit time_t)
	 *    highscore_ondisk_50    (32-bit machines with 32-bit time_t)
	 *
	 * The first two appear in 5.99 between the time_t change and
	 * 20090525, depending on whether the compiler inserts
	 * structure padding before an unaligned 64-bit time_t. The
	 * last appears in 5.0 and earlier.
	 *
	 * Any or all of these might also appear on other OSes where
	 * this code has been ported.
	 *
	 * Since the old file has no header, we will have to guess
	 * which of these formats it has.
	 */

	/*
	 * First, look for a header.
	 */
	result = read(sd, &header, sizeof(header));
	if (result < 0) {
		warn("Score file %s: read", _PATH_SCOREFILE);
		goto sdfail;
	}
	if (result != 0 && (size_t)result != sizeof(header)) {
		warnx("Score file %s: read: unexpected EOF", _PATH_SCOREFILE);
		/*
		 * File is hopelessly corrupt, might as well truncate it
		 * and start over with empty scores.
		 */
		if (lseek(sd, 0, SEEK_SET) < 0) {
			/* ? */
			warn("Score file %s: lseek", _PATH_SCOREFILE);
			goto sdfail;
		}
		if (ftruncate(sd, 0) == 0) {
			result = 0;
		} else {
			goto sdfail;
		}
	}

	if (result == 0) {
		/* Empty file; that just means there are no scores. */
		nscores = 0;
	} else {
		/*
		 * Is what we read a header, or the first 16 bytes of
		 * a score entry? hsh_magic_val is chosen to be
		 * something that is extremely unlikely to appear in
		 * hs_name[].
		 */
		if (!memcmp(header.hsh_magic, hsh_magic_val, HSH_MAGIC_SIZE)) {
			/* Yes, we have a header. */

			if (header.hsh_endiantag == HSH_ENDIAN_NATIVE) {
				/* native endian */
				doflip = 0;
			} else if (header.hsh_endiantag == HSH_ENDIAN_OPP) {
				doflip = 1;
			} else {
				warnx("Score file %s: Unknown endian tag %u",
					_PATH_SCOREFILE, header.hsh_endiantag);
				goto sdfail;
			}

			if (header.hsh_version != HSH_VERSION) {
				warnx("Score file %s: Unknown version code %u",
					_PATH_SCOREFILE, header.hsh_version);
				goto sdfail;
			}

			if (readscores(sd, doflip) < 0) {
				goto sdfail;
			}
		} else {
			/*
			 * Ok, it wasn't a header. Try to figure out what
			 * size records we have.
			 */
			result = scorefile_probe(sd);
			if (lseek(sd, 0, SEEK_SET) < 0) {
				warn("Score file %s: lseek", _PATH_SCOREFILE);
				goto sdfail;
			}
			switch (result) {
			case SCOREFILE_CURRENT:
				result = readscores(sd, 0 /* don't flip */);
				break;
			case SCOREFILE_CURRENT_OPP:
				result = readscores(sd, 1 /* do flip */);
				break;
			case SCOREFILE_599:
				result = readscores599(sd, 0 /* don't flip */);
				break;
			case SCOREFILE_599_OPP:
				result = readscores599(sd, 1 /* do flip */);
				break;
			case SCOREFILE_50:
				result = readscores50(sd, 0 /* don't flip */);
				break;
			case SCOREFILE_50_OPP:
				result = readscores50(sd, 1 /* do flip */);
				break;
			default:
				goto sdfail;
			}
			if (result < 0) {
				goto sdfail;
			}
		}
	}
	

	if (fdp)
		*fdp = sd;
	else
		close(sd);

	return;

sdfail:
	close(sd);
 fail:
	if (fdp != NULL) {
		*fdp = -1;
	}
	nscores = 0;
}

#ifdef ALLOW_SCORE_UPDATES
/*
 * Paranoid write wrapper; unlike fwrite() it preserves errno.
 */
static int
dowrite(int sd, const void *vbuf, size_t len)
{
	const char *buf = vbuf;
	ssize_t result;
	size_t done = 0;

	while (done < len) {
		result = write(sd, buf+done, len-done);
		if (result < 0) {
			if (errno == EINTR) {
				continue;
			}
			return -1;
		}
		done += result;
	}
	return 0;
}
#endif /* ALLOW_SCORE_UPDATES */

/*
 * Write the score file out.
 */
static void
putscores(int sd)
{
#ifdef ALLOW_SCORE_UPDATES
	struct highscore_header header;
	struct highscore_ondisk buf[MAXHISCORES];
	int i;

	if (sd == -1) {
		return;
	}

	memcpy(header.hsh_magic, hsh_magic_val, HSH_MAGIC_SIZE);
	header.hsh_endiantag = HSH_ENDIAN_NATIVE;
	header.hsh_version = HSH_VERSION;

	for (i=0; i<nscores; i++) {
		strncpy(buf[i].hso_name, scores[i].hs_name,
			sizeof(buf[i].hso_name));
		buf[i].hso_score = scores[i].hs_score;
		buf[i].hso_level = scores[i].hs_level;
		buf[i].hso_pad = 0xbaadf00d;
		buf[i].hso_time = scores[i].hs_time;
	}

	if (lseek(sd, 0, SEEK_SET) < 0) {
		warn("Score file %s: lseek", _PATH_SCOREFILE);
		goto fail;
	}
	if (dowrite(sd, &header, sizeof(header)) < 0 ||
	    dowrite(sd, buf, sizeof(buf[0]) * nscores) < 0) {
		warn("Score file %s: write", _PATH_SCOREFILE);
		goto fail;
	}
	return;
 fail:
	warnx("high scores may be damaged");
#else
	(void)sd;
#endif /* ALLOW_SCORE_UPDATES */
}

/*
 * Close the score file.
 */
static void
closescores(int sd)
{
	flock(sd, LOCK_UN);
	close(sd);
}

/*
 * Read and update the scores file with the current reults.
 */
void
savescore(int level)
{
	struct highscore *sp;
	int i;
	int change;
	int sd;
	const char *me;

	getscores(&sd);
	gotscores = 1;
	(void)time(&now);

	/*
	 * Allow at most one score per person per level -- see if we
	 * can replace an existing score, or (easiest) do nothing.
	 * Otherwise add new score at end (there is always room).
	 */
	change = 0;
	me = thisuser();
	for (i = 0, sp = &scores[0]; i < nscores; i++, sp++) {
		if (sp->hs_level != level || strcmp(sp->hs_name, me) != 0)
			continue;
		if (score > sp->hs_score) {
			(void)printf("%s bettered %s %d score of %d!\n",
			    "\nYou", "your old level", level,
			    sp->hs_score * sp->hs_level);
			sp->hs_score = score;	/* new score */
			sp->hs_time = now;	/* and time */
			change = 1;
		} else if (score == sp->hs_score) {
			(void)printf("%s tied %s %d high score.\n",
			    "\nYou", "your old level", level);
			sp->hs_time = now;	/* renew it */
			change = 1;		/* gotta rewrite, sigh */
		} /* else new score < old score: do nothing */
		break;
	}
	if (i >= nscores) {
		strcpy(sp->hs_name, me);
		sp->hs_level = level;
		sp->hs_score = score;
		sp->hs_time = now;
		nscores++;
		change = 1;
	}

	if (change) {
		/*
		 * Sort & clean the scores, then rewrite.
		 */
		nscores = checkscores(scores, nscores);
		putscores(sd);
	}
	closescores(sd);
}

/*
 * Get login name, or if that fails, get something suitable.
 * The result is always trimmed to fit in a score.
 */
static char *
thisuser(void)
{
	const char *p;
	struct passwd *pw;
	size_t l;
	static char u[sizeof(scores[0].hs_name)];

	if (u[0])
		return (u);
	p = getlogin();
	if (p == NULL || *p == '\0') {
		pw = getpwuid(getuid());
		if (pw != NULL)
			p = pw->pw_name;
		else
			p = "  ???";
	}
	l = strlen(p);
	if (l >= sizeof(u))
		l = sizeof(u) - 1;
	memcpy(u, p, l);
	u[l] = '\0';
	return (u);
}

/*
 * Score comparison function for qsort.
 *
 * If two scores are equal, the person who had the score first is
 * listed first in the highscore file.
 */
static int
cmpscores(const void *x, const void *y)
{
	const struct highscore *a, *b;
	long l;

	a = x;
	b = y;
	l = (long)b->hs_level * b->hs_score - (long)a->hs_level * a->hs_score;
	if (l < 0)
		return (-1);
	if (l > 0)
		return (1);
	if (a->hs_time < b->hs_time)
		return (-1);
	if (a->hs_time > b->hs_time)
		return (1);
	return (0);
}

/*
 * If we've added a score to the file, we need to check the file and ensure
 * that this player has only a few entries.  The number of entries is
 * controlled by MAXSCORES, and is to ensure that the highscore file is not
 * monopolised by just a few people.  People who no longer have accounts are
 * only allowed the highest score.  Scores older than EXPIRATION seconds are
 * removed, unless they are someone's personal best.
 * Caveat:  the highest score on each level is always kept.
 */
static int
checkscores(struct highscore *hs, int num)
{
	struct highscore *sp;
	int i, j, k, numnames;
	int levelfound[NLEVELS];
	struct peruser {
		char *name;
		int times;
	} count[NUMSPOTS];
	struct peruser *pu;

	/*
	 * Sort so that highest totals come first.
	 *
	 * levelfound[i] becomes set when the first high score for that
	 * level is encountered.  By definition this is the highest score.
	 */
	qsort((void *)hs, nscores, sizeof(*hs), cmpscores);
	for (i = MINLEVEL; i < NLEVELS; i++)
		levelfound[i] = 0;
	numnames = 0;
	for (i = 0, sp = hs; i < num;) {
		/*
		 * This is O(n^2), but do you think we care?
		 */
		for (j = 0, pu = count; j < numnames; j++, pu++)
			if (strcmp(sp->hs_name, pu->name) == 0)
				break;
		if (j == numnames) {
			/*
			 * Add new user, set per-user count to 1.
			 */
			pu->name = sp->hs_name;
			pu->times = 1;
			numnames++;
		} else {
			/*
			 * Two ways to keep this score:
			 * - Not too many (per user), still has acct, &
			 *	score not dated; or
			 * - High score on this level.
			 */
			if ((pu->times < MAXSCORES &&
			     getpwnam(sp->hs_name) != NULL &&
			     sp->hs_time + EXPIRATION >= now) ||
			    levelfound[sp->hs_level] == 0)
				pu->times++;
			else {
				/*
				 * Delete this score, do not count it,
				 * do not pass go, do not collect $200.
				 */
				num--;
				for (k = i; k < num; k++)
					hs[k] = hs[k + 1];
				continue;
			}
		}
        if (sp->hs_level < NLEVELS && sp->hs_level >= 0)
    		levelfound[sp->hs_level] = 1;
		i++, sp++;
	}
	return (num > MAXHISCORES ? MAXHISCORES : num);
}

/*
 * Show current scores.  This must be called after savescore, if
 * savescore is called at all, for two reasons:
 * - Showscores munches the time field.
 * - Even if that were not the case, a new score must be recorded
 *   before it can be shown anyway.
 */
void
showscores(int level)
{
	struct highscore *sp;
	int i, n, c;
	const char *me;
	int levelfound[NLEVELS];

	if (!gotscores)
		getscores(NULL);
	(void)printf("\n\t\t\t    Tetris High Scores\n");

	/*
	 * If level == 0, the person has not played a game but just asked for
	 * the high scores; we do not need to check for printing in highlight
	 * mode.  If SOstr is null, we can't do highlighting anyway.
	 */
	me = level && enter_standout_mode ? thisuser() : NULL;

	/*
	 * Set times to 0 except for high score on each level.
	 */
	for (i = MINLEVEL; i < NLEVELS; i++)
		levelfound[i] = 0;
	for (i = 0, sp = scores; i < nscores; i++, sp++) {
        if (sp->hs_level < NLEVELS && sp->hs_level >= 0) {
    		if (levelfound[sp->hs_level])
	    		sp->hs_time = 0;
		    else {
			    sp->hs_time = 1;
		    	levelfound[sp->hs_level] = 1;
		    }
        }
	}

	/*
	 * Page each screenful of scores.
	 */
	for (i = 0, sp = scores; i < nscores; sp += n) {
		n = 40;
		if (i + n > nscores)
			n = nscores - i;
		printem(level, i + 1, sp, n, me);
		if ((i += n) < nscores) {
			(void)printf("\nHit RETURN to continue.");
			(void)fflush(stdout);
			while ((c = getchar()) != '\n')
				if (c == EOF)
					break;
			(void)printf("\n");
		}
	}
}

static void
printem(int level, int offset, struct highscore *hs, int n, const char *me)
{
	struct highscore *sp;
	int nrows, row, col, item, i, highlight;
	char buf[100];
#define	TITLE "Rank  Score   Name     (points/level)"

	/*
	 * This makes a nice two-column sort with headers, but it's a bit
	 * convoluted...
	 */
	printf("%s   %s\n", TITLE, n > 1 ? TITLE : "");

	highlight = 0;
	nrows = (n + 1) / 2;

	for (row = 0; row < nrows; row++) {
		for (col = 0; col < 2; col++) {
			item = col * nrows + row;
			if (item >= n) {
				/*
				 * Can only occur on trailing columns.
				 */
				(void)putchar('\n');
				continue;
			}
			sp = &hs[item];
			(void)snprintf(buf, sizeof(buf),
			    "%3d%c %6d  %-11s (%6d on %d)",
			    item + offset, sp->hs_time ? '*' : ' ',
			    sp->hs_score * sp->hs_level,
			    sp->hs_name, sp->hs_score, sp->hs_level);
			/*
			 * Highlight if appropriate.  This works because
			 * we only get one score per level.
			 */
			if (me != NULL &&
			    sp->hs_level == level &&
			    sp->hs_score == score &&
			    strcmp(sp->hs_name, me) == 0) {
				putpad(enter_standout_mode);
				highlight = 1;
			}
			(void)printf("%s", buf);
			if (highlight) {
				putpad(exit_standout_mode);
				highlight = 0;
			}

			/* fill in spaces so column 1 lines up */
			if (col == 0)
				for (i = 40 - strlen(buf); --i >= 0;)
					(void)putchar(' ');
			else /* col == 1 */
				(void)putchar('\n');
		}
	}
}

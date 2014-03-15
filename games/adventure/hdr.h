/*	$NetBSD: hdr.h,v 1.13 2009/08/25 06:56:52 dholland Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * The game adventure was originally written in Fortran by Will Crowther
 * and Don Woods.  It was later translated to C and enhanced by Jim
 * Gillogly.  This code is derived from software contributed to Berkeley
 * by Jim Gillogly at The Rand Corporation.
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
 *	@(#)hdr.h	8.1 (Berkeley) 5/31/93
 */

/*   ADVENTURE -- Jim Gillogly, Jul 1977
 * This program is a re-write of ADVENT, written in FORTRAN mostly by
 * Don Woods of SAIL.  In most places it is as nearly identical to the
 * original as possible given the language and word-size differences.
 * A few places, such as the message arrays and travel arrays were changed
 * to reflect the smaller core size and word size.  The labels of the
 * original are reflected in this version, so that the comments of the
 * fortran are still applicable here.
 *
 * The data file distributed with the fortran source is assumed to be called
 * "glorkz" in the directory where the program is first run.
 *
 * The original FORTRAN version can be found at
 * <URL:ftp://ftp.gmd.de/if-archive/games/source/advent-original.tar.gz>.
 */

/* hdr.h: included by c advent files */

#include <signal.h>

extern volatile sig_atomic_t delhit;
extern int     yea;
extern char data_file[];	/* Virtual data file */

#define TAB     011
#define LF      012
#define FLUSHLINE do { int flushline_ch; while ((flushline_ch = getchar()) != EOF && flushline_ch != '\n'); } while (0)
#define FLUSHLF   while (next()!=LF)

extern int     loc, newloc, oldloc, oldloc2, wasdark, gaveup, kq, k, k2;
extern char   *wd1, *wd2;		/* the complete words */
extern int     verb, obj, spk;
extern int blklin;
extern int     saveday, savet, maxscore, latency;

#define SHORT 50		/* How short is a demo game? */

#define MAXSTR  20		/* max length of user's words */

#define HTSIZE  512		/* max number of vocab words */
extern struct hashtab {		/* hash table for vocabulary */
	int     val;		/* word type &index (ktab) */
	char   *atab;		/* pointer to actual string */
}       voc[HTSIZE];
#define SEED 1815622		/* "Encryption" seed */

struct text {
	char   *seekadr;/* Msg start in virtual disk */
	int     txtlen;	/* length of msg starting here */
};

#define RTXSIZE 205
extern	struct text rtext[RTXSIZE];	/* random text messages */

#define MAGSIZE 35
extern	struct text mtext[MAGSIZE];	/* magic messages */

extern	int     classes;
#define CLSMAX  12
extern	struct text ctext[CLSMAX];	/* classes of adventurer */
extern	int     cval[CLSMAX];

extern	struct text ptext[101];		/* object descriptions */

#define LOCSIZE 141			/* number of locations */
extern	struct text ltext[LOCSIZE];	/* long loc description */
extern	struct text stext[LOCSIZE];	/* short loc descriptions */

extern	struct travlist {		/* direcs & conditions of travel */
	struct travlist *next;	/* ptr to next list entry */
	int     conditions;	/* m in writeup (newloc / 1000) */
	int     tloc;		/* n in writeup (newloc % 1000) */
	int     tverb;		/* the verb that takes you there */
}      *travel[LOCSIZE], *tkk;	/* travel is closer to keys(...) */

extern	int     atloc[LOCSIZE];

extern	int     plac[101];		/* initial object placement */
extern	int     fixd[101], fixed[101];	/* location fixed? */

extern	int     actspeak[35];		/* rtext msg for verb <n> */

extern	int     cond[LOCSIZE];		/* various condition bits */

extern	int	setbit[16];		/* bit defn masks 1,2,4,... */

extern	int     hintmax;
extern	int     hints[20][5];		/* info on hints */
extern	int     hinted[20], hintlc[20];

extern	int     place[101], prop[101], links[201];
extern	int     abb[LOCSIZE];

extern	int     maxtrs, tally, tally2;	/* treasure values */

#define FALSE   0
#define TRUE    1

extern	int     keys, lamp, grate, cage, rod, rod2, steps,	/* mnemonics */
	        bird, door, pillow, snake, fissure, tablet, clam, oyster,
	        magazine, dwarf, knife, food, bottle, water, oil, plant, plant2,
	        axe, mirror, dragon, chasm, troll, troll2, bear, message,
	        vend, batter, nugget, coins, chest, eggs, trident, vase,
	        emerald, pyramid, pearl, rug, chain, spices, back, look, cave,
	        null, entrance, depression, /*enter, stream, pour,*/ say, lock,
	        throw, find, invent;

extern	int     chloc, chloc2, dseen[7], dloc[7],	/* dwarf stuff */
	        odloc[7], dflag, daltloc;

extern	int     tk[21], stick, dtotal, attack;
extern	int     turns, lmwarn, iwest, knfloc, detail,	/* various flags and
							 * counters */
	        abbnum, maxdie, numdie, holding, dkill, foobar, bonus, clock1,
	        clock2, saved, isclosing, panic, closed, scoring;

extern	int     demo, limit;

#define DECR(a,b,c,d,e) decr(a+'+',b+'-',c+'#',d+'&',e+'%')

/*	$NetBSD: scores.h,v 1.5 2009/05/25 08:33:57 dholland Exp $	*/

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
 *	@(#)scores.h	8.1 (Berkeley) 5/31/93
 */

/*
 * Tetris scores.
 */

/* Header for high score file. */
#define HSH_MAGIC_SIZE 8
struct highscore_header {
	char hsh_magic[HSH_MAGIC_SIZE];
	uint32_t hsh_endiantag;
	uint32_t hsh_version;
};

/* Current on-disk high score record. */
struct highscore_ondisk {
	char hso_name[20];
	int32_t hso_score;
	int32_t hso_level;
	int32_t hso_pad;
	int64_t hso_time;
};

/* 5.99.x after time_t change, on 32-bit machines */
struct highscore_ondisk_599 {
	char hso599_name[20];
	int32_t hso599_score;
	int32_t hso599_level;
	int32_t hso599_time[2];
};

/* 5.0 and earlier on-disk high score record. */
struct highscore_ondisk_50 {
	char hso50_name[20];
	int32_t hso50_score;
	int32_t hso50_level;
	int32_t hso50_time;
};

/* In-memory high score record. */
struct highscore {
	char	hs_name[20];	/* login name */
	int	hs_score;	/* raw score */
	int	hs_level;	/* play level */
	time_t	hs_time;	/* time at game end */
};

#define MAXHISCORES	80
#define MAXSCORES	9	/* maximum high score entries per person */
#define	EXPIRATION	(5L * 365 * 24 * 60 * 60)

void	savescore(int);
void	showscores(int);

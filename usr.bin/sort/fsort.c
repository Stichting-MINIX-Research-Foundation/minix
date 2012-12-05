/*	$NetBSD: fsort.c,v 1.47 2010/02/05 21:58:41 enami Exp $	*/

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

/*
 * Read in a block of records (until 'enough').
 * sort, write to temp file.
 * Merge sort temp files into output file
 * Small files miss out the temp file stage.
 * Large files might get multiple merges.
 */
#include "sort.h"
#include "fsort.h"

__RCSID("$NetBSD: fsort.c,v 1.47 2010/02/05 21:58:41 enami Exp $");

#include <stdlib.h>
#include <string.h>

#define SALIGN(n) ((n+sizeof(length_t)-1) & ~(sizeof(length_t)-1))

void
fsort(struct filelist *filelist, int nfiles, FILE *outfp, struct field *ftbl)
{
	RECHEADER **keylist;
	RECHEADER **keypos, **keyp;
	RECHEADER *buffer;
	size_t bufsize = DEFBUFSIZE;
	u_char *bufend;
	int mfct = 0;
	int c, nelem;
	get_func_t get;
	RECHEADER *crec;
	RECHEADER *nbuffer;
	FILE *fp, *tmp_fp;
	int file_no;
	int max_recs = DEBUG('m') ? 16 : MAXNUM;

	buffer = allocrec(NULL, bufsize);
	bufend = (u_char *)buffer + bufsize;
	/* Allocate double length keymap for radix_sort */
	keylist = malloc(2 * max_recs * sizeof(*keylist));
	if (buffer == NULL || keylist == NULL)
		err(2, "failed to malloc initial buffer or keylist");

	if (SINGL_FLD)
		/* Key and data are one! */
		get = makeline;
	else
		/* Key (merged key fields) added before data */
		get = makekey;

	file_no = 0;
#if defined(__minix)
	/* LSC FIXME: Not very pretty, but reduce the diff */
#include "pathnames.h"
	if (!strcmp(filelist->names[0], _PATH_STDIN))
		fp = stdin;
	else
#endif /* defined(__minix) */
	fp = fopen(filelist->names[0], "r");
	if (fp == NULL)
		err(2, "%s", filelist->names[0]);

	/* Loop through reads of chunk of input files that get sorted
	 * and then merged together. */
	for (;;) {
		keypos = keylist;
		nelem = 0;
		crec = buffer;
		makeline_copydown(crec);

		/* Loop reading records */
		for (;;) {
			c = get(fp, crec, bufend, ftbl);
			/* 'c' is 0, EOF or BUFFEND */
			if (c == 0) {
				/* Save start of key in input buffer */
				*keypos++ = crec;
				if (++nelem == max_recs) {
					c = BUFFEND;
					break;
				}
				crec = (RECHEADER *)(crec->data + SALIGN(crec->length));
				continue;
			}
			if (c == EOF) {
				/* try next file */
				if (++file_no >= nfiles)
					/* no more files */
					break;
#if defined(__minix)
				if (!strcmp(filelist->names[0], _PATH_STDIN))
					fp = stdin;
				else
#endif /* defined(__minix) */
				fp = fopen(filelist->names[file_no], "r");
				if (fp == NULL)
					err(2, "%s", filelist->names[file_no]);
				continue;
			}
			if (nelem >= max_recs
			    || (bufsize >= MAXBUFSIZE && nelem > 8))
				/* Need to sort and save this lot of data */
				break;

			/* c == BUFFEND, and we can process more data */
			/* Allocate a larger buffer for this lot of data */
			bufsize *= 2;
			nbuffer = allocrec(buffer, bufsize);
			if (!nbuffer) {
				err(2, "failed to realloc buffer to %zu bytes",
					bufsize);
			}

			/* patch up keylist[] */
			for (keyp = &keypos[-1]; keyp >= keylist; keyp--)
				*keyp = nbuffer + (*keyp - buffer);

			crec = nbuffer + (crec - buffer);
			buffer = nbuffer;
			bufend = (u_char *)buffer + bufsize;
		}

		/* Sort this set of records */
		radix_sort(keylist, keylist + max_recs, nelem);

		if (c == EOF && mfct == 0) {
			/* all the data is (sorted) in the buffer */
			append(keylist, nelem, outfp,
			    DEBUG('k') ? putkeydump : putline);
			break;
		}

		/* Save current data to a temporary file for a later merge */
		if (nelem != 0) {
			tmp_fp = ftmp();
			append(keylist, nelem, tmp_fp, putrec);
			save_for_merge(tmp_fp, geteasy, ftbl);
		}
		mfct = 1;

		if (c == EOF) {
			/* merge to output file */
			merge_sort(outfp, 
			    DEBUG('k') ? putkeydump : putline, ftbl);
			break;
		}
	}

	free(keylist);
	keylist = NULL;
	free(buffer);
	buffer = NULL;
}

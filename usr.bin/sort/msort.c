/*	$NetBSD: msort.c,v 1.30 2010/02/05 21:58:42 enami Exp $	*/

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

#include "sort.h"
#include "fsort.h"

__RCSID("$NetBSD: msort.c,v 1.30 2010/02/05 21:58:42 enami Exp $");

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

/* Subroutines using comparisons: merge sort and check order */
#define DELETE (1)

typedef struct mfile {
	FILE         *fp;
	get_func_t   get;
	RECHEADER    *rec;
	u_char       *end;
} MFILE;

static int cmp(RECHEADER *, RECHEADER *);
static int insert(struct mfile **, struct mfile *, int, int);
static void merge_sort_fstack(FILE *, put_func_t, struct field *);

/*
 * Number of files merge() can merge in one pass.
 */
#define MERGE_FNUM      16

static struct mfile fstack[MERGE_FNUM];
static struct mfile fstack_1[MERGE_FNUM];
static struct mfile fstack_2[MERGE_FNUM];
static int fstack_count, fstack_1_count, fstack_2_count;

void
save_for_merge(FILE *fp, get_func_t get, struct field *ftbl)
{
	FILE *mfp, *mfp1, *mfp2;

	if (fstack_count == MERGE_FNUM) {
		/* Must reduce the number of temporary files */
		mfp = ftmp();
		merge_sort_fstack(mfp, putrec, ftbl);
		/* Save output in next layer */
		if (fstack_1_count == MERGE_FNUM) {
			mfp1 = ftmp();
			memcpy(fstack, fstack_1, sizeof fstack);
			merge_sort_fstack(mfp1, putrec, ftbl);
			if (fstack_2_count == MERGE_FNUM) {
				/* More than 4096 files! */
				mfp2 = ftmp();
				memcpy(fstack, fstack_2, sizeof fstack);
				merge_sort_fstack(mfp2, putrec, ftbl);
				fstack_2[0].fp = mfp2;
				fstack_2_count = 1;
			}
			fstack_2[fstack_2_count].fp = mfp1;
			fstack_2[fstack_2_count].get = geteasy;
			fstack_2_count++;
			fstack_1_count = 0;
		}
		fstack_1[fstack_1_count].fp = mfp;
		fstack_1[fstack_1_count].get = geteasy;
		fstack_1_count++;
		fstack_count = 0;
	}

	fstack[fstack_count].fp = fp;
	fstack[fstack_count++].get = get;
}

void
fmerge(struct filelist *filelist, int nfiles, FILE *outfp, struct field *ftbl)
{
	get_func_t get = SINGL_FLD ? makeline : makekey;
	FILE *fp;
	int i;

	for (i = 0; i < nfiles; i++) {
		fp = fopen(filelist->names[i], "r");
		if (fp == NULL)
			err(2, "%s", filelist->names[i]);
		save_for_merge(fp, get, ftbl);
	}

	merge_sort(outfp, putline, ftbl);
}

void
merge_sort(FILE *outfp, put_func_t put, struct field *ftbl)
{
	int count = fstack_1_count + fstack_2_count;
	FILE *mfp;
	int i;

	if (count == 0) {
		/* All files in initial array */
		merge_sort_fstack(outfp, put, ftbl);
		return;
	}

	count += fstack_count;

	/* Too many files for one merge sort */
	for (;;) {
		/* Sort latest 16 files */
		i = count;
		if (i > MERGE_FNUM)
			i = MERGE_FNUM;
		while (fstack_count > 0)
			fstack[--i] = fstack[--fstack_count];
		while (i > 0 && fstack_1_count > 0)
			fstack[--i] = fstack_1[--fstack_1_count];
		while (i > 0)
			fstack[--i] = fstack_2[--fstack_2_count];
		if (count <= MERGE_FNUM) {
			/* Got all the data */
			fstack_count = count;
			merge_sort_fstack(outfp, put, ftbl);
			return;
		}
		mfp = ftmp();
		fstack_count = count > MERGE_FNUM ? MERGE_FNUM : count;
		merge_sort_fstack(mfp, putrec, ftbl);
		fstack[0].fp = mfp;
		fstack[0].get = geteasy;
		fstack_count = 1;
		count -= MERGE_FNUM - 1;
	}
}

static void
merge_sort_fstack(FILE *outfp, put_func_t put, struct field *ftbl)
{
	struct mfile *flistb[MERGE_FNUM], **flist = flistb, *cfile;
	RECHEADER *new_rec;
	u_char *new_end;
	void *tmp;
	int c, i, nfiles;
	size_t sz;

	/* Read one record from each file (read again if a duplicate) */
	for (nfiles = i = 0; i < fstack_count; i++) {
		cfile = &fstack[i];
		if (cfile->rec == NULL) {
			cfile->rec = allocrec(NULL, DEFLLEN);
			cfile->end = (u_char *)cfile->rec + DEFLLEN;
		}
		rewind(cfile->fp);

		for (;;) {
			c = cfile->get(cfile->fp, cfile->rec, cfile->end, ftbl);
			if (c == EOF)
				break;

			if (c == BUFFEND) {
				/* Double buffer size */
				sz = (cfile->end - (u_char *)cfile->rec) * 2;
				cfile->rec = allocrec(cfile->rec, sz);
				cfile->end = (u_char *)cfile->rec + sz;
				continue;
			}

			if (nfiles != 0) {
				if (insert(flist, cfile, nfiles, !DELETE))
					/* Duplicate removed */
					continue;
			} else
				flist[0] = cfile;
			nfiles++;
			break;
		}
	}

	if (nfiles == 0)
		return;

	/*
	 * We now loop reading a new record from the file with the
	 * 'sorted first' existing record.
	 * As each record is added, the 'first' record is written to the
	 * output file - maintaining one record from each file in the sorted
	 * list.
	 */
	new_rec = allocrec(NULL, DEFLLEN);
	new_end = (u_char *)new_rec + DEFLLEN;
	for (;;) {
		cfile = flist[0];
		c = cfile->get(cfile->fp, new_rec, new_end, ftbl);
		if (c == EOF) {
			/* Write out last record from now-empty input */
			put(cfile->rec, outfp);
			if (--nfiles == 0)
				break;
			/* Replace from file with now-first sorted record. */
			/* (Moving base 'flist' saves copying everything!) */
			flist++;
			continue;
		}
		if (c == BUFFEND) {
			/* Buffer not large enough - double in size */
			sz = (new_end - (u_char *)new_rec) * 2;
			new_rec = allocrec(new_rec, sz);
			new_end = (u_char *)new_rec +sz;
			continue;
		}

		/* Swap in new buffer, saving old */
		tmp = cfile->rec;
		cfile->rec = new_rec;
		new_rec = tmp;
		tmp = cfile->end;
		cfile->end = new_end;
		new_end = tmp;

		/* Add into sort, removing the original first entry */
		c = insert(flist, cfile, nfiles, DELETE);
		if (c != 0 || (UNIQUE && cfile == flist[0]
			    && cmp(new_rec, cfile->rec) == 0)) {
			/* Was an unwanted duplicate, restore buffer */
			tmp = cfile->rec;
			cfile->rec = new_rec;
			new_rec = tmp;
			tmp = cfile->end;
			cfile->end = new_end;
			new_end = tmp;
			continue;
		}

		/* Write out 'old' record */
		put(new_rec, outfp);
	}

	free(new_rec);
}

/*
 * if delete: inserts rec in flist, deletes flist[0];
 * otherwise just inserts *rec in flist.
 * Returns 1 if record is a duplicate to be ignored.
 */
static int
insert(struct mfile **flist, struct mfile *rec, int ttop, int delete)
{
	int mid, top = ttop, bot = 0, cmpv = 1;

	for (mid = top / 2; bot + 1 != top; mid = (bot + top) / 2) {
		cmpv = cmp(rec->rec, flist[mid]->rec);
		if (cmpv == 0 ) {
			if (UNIQUE)
				/* Duplicate key, read another record */
				/* NB: This doesn't guarantee to keep any
				 * particular record. */
				return 1;
			/*
			 * Apply sort by input file order.
			 * We could truncate the sort is the fileno are
			 * adjacent - but that is all too hard!
			 * The fileno cannot be equal, since we only have one
			 * record from each file (+ flist[0] which never
			 * comes here).
			 */
			cmpv = rec < flist[mid] ? -1 : 1;
			if (REVERSE)
				cmpv = -cmpv;
		}
		if (cmpv < 0)
			top = mid;
		else
			bot = mid;
	}

	/* At this point we haven't yet compared against flist[0] */

	if (delete) {
		/* flist[0] is ourselves, only the caller knows the old data */
		if (bot != 0) {
			memmove(flist, flist + 1, bot * sizeof(MFILE *));
			flist[bot] = rec;
		}
		return 0;
	}

	/* Inserting original set of records */

	if (bot == 0 && cmpv != 0) {
		/* Doesn't match flist[1], must compare with flist[0] */
		cmpv = cmp(rec->rec, flist[0]->rec);
		if (cmpv == 0 && UNIQUE)
			return 1;
		/* Add matching keys in file order (ie new is later) */
		if (cmpv < 0)
			bot = -1;
	}
	bot++;
	memmove(flist + bot + 1, flist + bot, (ttop - bot) * sizeof(MFILE *));
	flist[bot] = rec;
	return 0;
}

/*
 * check order on one file
 */
void
order(struct filelist *filelist, struct field *ftbl)
{
	get_func_t get = SINGL_FLD ? makeline : makekey;
	RECHEADER *crec, *prec, *trec;
	u_char *crec_end, *prec_end, *trec_end;
	FILE *fp;
	int c;

	fp = fopen(filelist->names[0], "r");
	if (fp == NULL)
		err(2, "%s", filelist->names[0]);

	crec = malloc(offsetof(RECHEADER, data[DEFLLEN]));
	crec_end = crec->data + DEFLLEN;
	prec = malloc(offsetof(RECHEADER, data[DEFLLEN]));
	prec_end = prec->data + DEFLLEN;

	/* XXX this does exit(0) for overlong lines */
	if (get(fp, prec, prec_end, ftbl) != 0)
		exit(0);
	while (get(fp, crec, crec_end, ftbl) == 0) {
		if (0 < (c = cmp(prec, crec))) {
			crec->data[crec->length-1] = 0;
			errx(1, "found disorder: %s", crec->data+crec->offset);
		}
		if (UNIQUE && !c) {
			crec->data[crec->length-1] = 0;
			errx(1, "found non-uniqueness: %s",
			    crec->data+crec->offset);
		}
		/*
		 * Swap pointers so that this record is on place pointed
		 * to by prec and new record is read to place pointed to by
		 * crec.
		 */ 
		trec = prec;
		prec = crec;
		crec = trec;
		trec_end = prec_end;
		prec_end = crec_end;
		crec_end = trec_end;
	}
	exit(0);
}

static int
cmp(RECHEADER *rec1, RECHEADER *rec2)
{
	int len;
	int r;

	/* key is weights */
	len = min(rec1->keylen, rec2->keylen);
	r = memcmp(rec1->data, rec2->data, len);
	if (r == 0)
		r = rec1->keylen - rec2->keylen;
	if (REVERSE)
		r = -r;
	return r;
}

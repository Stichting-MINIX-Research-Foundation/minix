/*	$NetBSD: sort.h,v 1.34 2011/09/16 15:39:29 joerg Exp $	*/

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
 *
 *	@(#)sort.h	8.1 (Berkeley) 6/6/93
 */

#include <sys/param.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NBINS		256

/* values for masks, weights, and other flags. */
/* R and F get used to index weight_tables[] */
#define	R	0x01	/* Field is reversed */
#define	F	0x02	/* weight lower and upper case the same */
#define	I	0x04	/* mask out non-printable characters */
#define	D	0x08	/* sort alphanumeric characters only */
#define	N	0x10	/* Field is a number */
#define	BI	0x20	/* ignore blanks in icol */
#define	BT	0x40	/* ignore blanks in tcol */
#define	L	0x80	/* Sort by field length */
#ifdef __minix
#define	X	0x100	/* Field is a hex number */
#endif

/* masks for delimiters: blanks, fields, and termination. */
#define BLANK 1		/* ' ', '\t'; '\n' if -R is invoked */
#define FLD_D 2		/* ' ', '\t' default; from -t otherwise */
#define REC_D_F 4	/* '\n' default; from -R otherwise */

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#define	FCLOSE(file) {							\
	if (EOF == fclose(file))					\
		err(2, "%p", file);					\
}

#define	EWRITE(ptr, size, n, f) {					\
	if (!fwrite(ptr, size, n, f))					\
		 err(2, NULL);						\
}

/* Records are limited to MAXBUFSIZE (8MB) and less if you want to sort
 * in a sane way.
 * Anyone who wants to sort data records longer than 2GB definitely needs a
 * different program! */
typedef unsigned int length_t;

/* A record is a key/line pair starting at rec.data. It has a total length
 * and an offset to the start of the line half of the pair.
 */
typedef struct recheader {
	length_t length;	/* total length of key and line */
	length_t offset;	/* to line */
	int      keylen;	/* length of key */
	u_char   data[];	/* key then line */
} RECHEADER;

/* This is the column as seen by struct field.  It is used by enterfield.
 * They are matched with corresponding coldescs during initialization.
 */
struct column {
	struct coldesc *p;
	int num;
	int indent;
};

/* a coldesc has a number and pointers to the beginning and end of the
 * corresponding column in the current line.  This is determined in enterkey.
 */
typedef struct coldesc {
	u_char *start;
	u_char *end;
	int num;
} COLDESC;

/* A field has an initial and final column; an omitted final column
 * implies the end of the line.  Flags regulate omission of blanks and
 * numerical sorts; mask determines which characters are ignored (from -i, -d);
 * weights determines the sort weights of a character (from -f, -r).
 *
 * The first field contain the global flags etc.
 * The list terminates when icol = 0.
 */
struct field {
	struct column icol;
	struct column tcol;
	u_int flags;
	u_char *mask;
	u_char *weights;
};

struct filelist {
	const char * const * names;
};

typedef int (*get_func_t)(FILE *, RECHEADER *, u_char *, struct field *);
typedef void (*put_func_t)(const RECHEADER *, FILE *);

extern u_char ascii[NBINS], Rascii[NBINS], Ftable[NBINS], RFtable[NBINS];
extern u_char *const weight_tables[4];   /* ascii, Rascii, Ftable, RFtable */
extern u_char d_mask[NBINS];
extern int SINGL_FLD, SEP_FLAG, UNIQUE, REVERSE;
extern int posix_sort;
extern int REC_D;
extern const char *tmpdir;
extern struct coldesc *clist;
extern int ncols;

#define DEBUG(ch) (debug_flags & (1 << ((ch) & 31)))
extern unsigned int debug_flags;

RECHEADER *allocrec(RECHEADER *, size_t);
void	 append(RECHEADER **, int, FILE *, void (*)(const RECHEADER *, FILE *));
void	 concat(FILE *, FILE *);
length_t enterkey(RECHEADER *, const u_char *, u_char *, size_t, struct field *);
void	 fixit(int *, char **, const char *);
void	 fldreset(struct field *);
FILE	*ftmp(void);
void	 fmerge(struct filelist *, int, FILE *, struct field *);
void	 save_for_merge(FILE *, get_func_t, struct field *);
void	 merge_sort(FILE *, put_func_t, struct field *);
void	 fsort(struct filelist *, int, FILE *, struct field *);
int	 geteasy(FILE *, RECHEADER *, u_char *, struct field *);
int	 makekey(FILE *, RECHEADER *, u_char *, struct field *);
int	 makeline(FILE *, RECHEADER *, u_char *, struct field *);
void	 makeline_copydown(RECHEADER *);
int	 optval(int, int);
__dead void	 order(struct filelist *, struct field *);
void	 putline(const RECHEADER *, FILE *);
void	 putrec(const RECHEADER *, FILE *);
void	 putkeydump(const RECHEADER *, FILE *);
void	 rd_append(int, int, int, FILE *, u_char *, u_char *);
void	 radix_sort(RECHEADER **, RECHEADER **, int);
int	 setfield(const char *, struct field *, int);
void	 settables(void);

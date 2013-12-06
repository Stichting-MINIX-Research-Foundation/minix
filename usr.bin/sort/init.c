/*	$NetBSD: init.c,v 1.29 2013/10/18 20:47:06 christos Exp $	*/

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

__RCSID("$NetBSD: init.c,v 1.29 2013/10/18 20:47:06 christos Exp $");

#include <ctype.h>
#include <string.h>

static void insertcol(struct field *);
static const char *setcolumn(const char *, struct field *);

/*
 * masks of ignored characters.
 */
static u_char dtable[NBINS], itable[NBINS];

/*
 * parsed key options
 */
struct coldesc *clist = NULL;
int ncols = 0;

/*
 * clist (list of columns which correspond to one or more icol or tcol)
 * is in increasing order of columns.
 * Fields are kept in increasing order of fields.
 */

/* 
 * keep clist in order--inserts a column in a sorted array
 */
static void
insertcol(struct field *field)
{
	int i;
	struct coldesc *p;

	/* Make space for new item */
	p = realloc(clist, (ncols + 2) * sizeof(*clist));
	if (!p)
		err(1, "realloc");
	clist = p;
	memset(&clist[ncols], 0, sizeof(clist[ncols]));

	for (i = 0; i < ncols; i++)
		if (field->icol.num <= clist[i].num)
			break;
	if (field->icol.num != clist[i].num) {
		memmove(clist+i+1, clist+i, sizeof(COLDESC)*(ncols-i));
		clist[i].num = field->icol.num;
		ncols++;
	}
	if (field->tcol.num && field->tcol.num != field->icol.num) {
		for (i = 0; i < ncols; i++)
			if (field->tcol.num <= clist[i].num)
				break;
		if (field->tcol.num != clist[i].num) {
			memmove(clist+i+1, clist+i,sizeof(COLDESC)*(ncols-i));
			clist[i].num = field->tcol.num;
			ncols++;
		}
	}
}

/*
 * matches fields with the appropriate columns--n^2 but who cares?
 */
void
fldreset(struct field *fldtab)
{
	int i;

	fldtab[0].tcol.p = clist + ncols - 1;
	for (++fldtab; fldtab->icol.num; ++fldtab) {
		for (i = 0; fldtab->icol.num != clist[i].num; i++)
			;
		fldtab->icol.p = clist + i;
		if (!fldtab->tcol.num)
			continue;
		for (i = 0; fldtab->tcol.num != clist[i].num; i++)
			;
		fldtab->tcol.p = clist + i;
	}
}

/*
 * interprets a column in a -k field
 */
static const char *
setcolumn(const char *pos, struct field *cur_fld)
{
	struct column *col;
	char *npos;
	int tmp;
	col = cur_fld->icol.num ? (&cur_fld->tcol) : (&cur_fld->icol);
	col->num = (int) strtol(pos, &npos, 10);
	pos = npos;
	if (col->num <= 0 && !(col->num == 0 && col == &(cur_fld->tcol)))
		errx(2, "field numbers must be positive");
	if (*pos == '.') {
		if (!col->num)
			errx(2, "cannot indent end of line");
		++pos;
		col->indent = (int) strtol(pos, &npos, 10);
		pos = npos;
		if (&cur_fld->icol == col)
			col->indent--;
		if (col->indent < 0)
			errx(2, "illegal offset");
	}
	for(; (tmp = optval(*pos, cur_fld->tcol.num)); pos++)
		cur_fld->flags |= tmp;
	if (cur_fld->icol.num == 0)
		cur_fld->icol.num = 1;
	return (pos);
}

int
setfield(const char *pos, struct field *cur_fld, int gflag)
{
	cur_fld->mask = NULL;

	pos = setcolumn(pos, cur_fld);
	if (*pos == '\0')			/* key extends to EOL. */
		cur_fld->tcol.num = 0;
	else {
		if (*pos != ',')
			errx(2, "illegal field descriptor");
		setcolumn((++pos), cur_fld);
	}
	if (!cur_fld->flags)
		cur_fld->flags = gflag;
	if (REVERSE)
		/* A local 'r' doesn't invert the global one */
		cur_fld->flags &= ~R;

	/* Assign appropriate mask table and weight table. */
	cur_fld->weights = weight_tables[cur_fld->flags & (R | F)];
	if (cur_fld->flags & I)
		cur_fld->mask = itable;
	else if (cur_fld->flags & D)
		cur_fld->mask = dtable;

	cur_fld->flags |= (gflag & (BI | BT));
	if (!cur_fld->tcol.indent)	/* BT has no meaning at end of field */
		cur_fld->flags &= ~BT;

	if (cur_fld->tcol.num
	    && !(!(cur_fld->flags & BI) && cur_fld->flags & BT)
	    && (cur_fld->tcol.num <= cur_fld->icol.num
		    /* indent if 0 -> end of field, i.e. okay */
		    && cur_fld->tcol.indent != 0
		    && cur_fld->tcol.indent < cur_fld->icol.indent))
		errx(2, "fields out of order");

	insertcol(cur_fld);
	return (cur_fld->tcol.num);
}

int
optval(int desc, int tcolflag)
{
	switch(desc) {
	case 'b':
		if (!tcolflag)
			return BI;
		else
			return BT;
	case 'd': return D;
	case 'f': return F;
	case 'i': return I;
	case 'l': return L;
	case 'n': return N;
#if defined(__minix)
	case 'x': return X;
#endif /* defined(__minix) */
	case 'r': return R;
	default:  return 0;
	}
}

/*
 * Return true if the options found in ARG, according to the getopt
 * spec in OPTS, require an additional argv word as an option
 * argument.
 */
static int
options_need_argument(const char *arg, const char *opts)
{
	size_t pos;
	const char *s;

	/*assert(arg[0] == '-');*/

	pos = 1;
	while (arg[pos]) {
		s = strchr(opts, arg[pos]);
		if (s == NULL) {
			/* invalid option */
			return 0;
		}
		if (s[1] == ':') {
			/* option requires argument */
			if (arg[pos+1] == '\0') {
				/* no argument in this arg */
				return 1;
			}
			else {
				/* argument is in this arg; no more options */
				return 0;
			}
		}
		pos++;
	}
	return 0;
}

/*
 * Replace historic +SPEC arguments with appropriate -kSPEC.
 *
 * The form can be either a single +SPEC or a pair +SPEC -SPEC.
 * The following -SPEC is not recognized unless it follows
 * immediately.
 */ 
void
fixit(int *argc, char **argv, const char *opts)
{
	int i, j, sawplus;
	char *vpos, *tpos, spec[20];
	int col, indent;

	sawplus = 0;
	for (i = 1; i < *argc; i++) {
		/*
		 * This loop must stop exactly where getopt will stop.
		 * Otherwise it turns e.g. "sort x +3" into "sort x
		 * -k4.1", which will croak if +3 was in fact really a
		 * file name. In order to do this reliably we need to
		 * be able to identify argv words that are option
		 * arguments.
		 */

		if (!strcmp(argv[i], "--")) {
			/* End of options; stop. */
			break;
		}

		if (argv[i][0] == '+') {
			/* +POS argument */
			sawplus = 1;
		} else if (argv[i][0] == '-' && sawplus &&
			   isdigit((unsigned char)argv[i][1])) {
			/* -POS argument */
			sawplus = 0;
		} else if (argv[i][0] == '-') {
			/* other option */
			sawplus = 0;
			if (options_need_argument(argv[i], opts)) {
				/* skip over the argument */
				i++;
			}
			continue;
		} else {
			/* not an option at all; stop */
			sawplus = 0;
			break;
		}

		/*
		 * At this point argv[i] is an old-style spec. The
		 * sawplus flag used by the above loop logic also
		 * tells us if it's a +SPEC or -SPEC.
		 */
		
		/* parse spec */
		tpos = argv[i]+1;
		col = (int)strtol(tpos, &tpos, 10);
		if (*tpos == '.') {
			++tpos;
			indent = (int) strtol(tpos, &tpos, 10);
		} else
			indent = 0;
		/* tpos now points to the optional flags */

		/*
		 * In the traditional form, x.0 means beginning of line;
		 * in the new form, x.0 means end of line. Adjust the
		 * value of INDENT accordingly.
		 */
		if (sawplus) {
			/* +POS */
			col += 1;
			indent += 1;
		} else {
			/* -POS */
			if (indent > 0)
				col += 1;
		}

		/* make the new style spec */
		(void)snprintf(spec, sizeof(spec), "%d.%d%s", col, indent,
		    tpos);

		if (sawplus) {
			/* Replace the +POS argument with new-style -kSPEC */
			asprintf(&vpos, "-k%s", spec);
			argv[i] = vpos;
		} else {
			/*
			 * Append the spec to the one from the
			 * preceding +POS argument, and remove the
			 * current argv element entirely.
			 */
			asprintf(&vpos, "%s,%s", argv[i-1], spec);
			free(argv[i-1]);
			argv[i-1] = vpos;
			for (j=i; j < *argc; j++)
				argv[j] = argv[j+1];
			*argc -= 1;
			i--;
		}
	}
}

/*
 * ascii, Rascii, Ftable, and RFtable map
 *
 * Sorting 'weight' tables.
 * Convert 'ascii' characters into their sort order.
 * The 'F' variants fold lower case to upper equivalent
 * The 'R' variants are for reverse sorting.
 *
 * The record separator (REC_D) never needs a weight, this frees one
 * byte value as an 'end of key' marker. This must be 0 for normal
 * weight tables, and 0xff for reverse weight tables - and is used
 * to terminate keys so that short keys sort before (after if reverse)
 * longer keys.
 *
 * The field separator has a normal weight - although it cannot occur
 * within a key unless it is the default (space+tab).
 *
 * All other bytes map to the appropriate value for the sort order.
 * Numeric sorts don't need any tables, they are reversed by negation.
 *
 * Global reverse sorts are done by writing the sorted keys in reverse
 * order - the sort itself is stil forwards.
 * This means that weights are only ever used when generating keys, any
 * sort of the original data bytes is always forwards and unweighted.
 *
 * Note: this is only good for ASCII sorting.  For different LC 's,
 * all bets are off.
 *
 * itable[] and dtable[] are the masks for -i (ignore non-printables)
 * and -d (only sort blank and alphanumerics).
 */
void
settables(void)
{
	int i;
	int next_weight = 1;
	int rev_weight = 254;

	ascii[REC_D] = 0;
	Rascii[REC_D] = 255;
	Ftable[REC_D] = 0;
	RFtable[REC_D] = 255;

	for (i = 0; i < 256; i++) {
		if (i == REC_D)
			continue;
		ascii[i] = next_weight;
		Rascii[i] = rev_weight;
		if (Ftable[i] == 0) {
			Ftable[i] = next_weight;
			RFtable[i] = rev_weight;
			Ftable[tolower(i)] = next_weight;
			RFtable[tolower(i)] = rev_weight;
		}
		next_weight++;
		rev_weight--;

		if (i == '\n' || isprint(i))
			itable[i] = 1;

		if (i == '\n' || i == '\t' || i == ' ' || isalnum(i))
			dtable[i] = 1;
	}
}

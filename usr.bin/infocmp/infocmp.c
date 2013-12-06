/* $NetBSD: infocmp.c,v 1.8 2013/10/01 09:01:49 roy Exp $ */

/*
 * Copyright (c) 2009, 2010 The NetBSD Foundation, Inc.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Roy Marples.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: infocmp.c,v 1.8 2013/10/01 09:01:49 roy Exp $");

#include <sys/ioctl.h>

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <term_private.h>
#include <term.h>
#include <unistd.h>

#define SW 8

typedef struct tient {
	char type;
	const char *id;
	signed char flag;
	short num;
	const char *str;
} TIENT;

static size_t cols;
static int aflag, cflag, nflag, qflag, xflag;

static size_t
outstr(FILE *f, const char *str)
{
	unsigned char ch;
	size_t r, l;

	r = 0;
	l = strlen(str);
	while ((ch = (unsigned char)(*str++)) != '\0') {
		switch (ch) {
		case 128:
			ch = '0';
			break;
		case '\033':
			ch = 'E';
			break;
		case '\014':
			ch = 'f';
			break;
		case '^': /* FALLTHROUGH */
		case ',': /* escape these */
			break;
		case ' ':
			ch = 's';
			break;
		default:
			if (ch == '\177') {
				if (f != NULL)
					fputc('^', f);
				ch = '?';
				r++;
			} else if (iscntrl(ch) &&
			    ch < 128 &&
			    ch != '\\' &&
			    (l < 4 || isdigit((unsigned char)*str)))
			{
				if (f != NULL)
					fputc('^', f);
				ch += '@';
				r++;
			} else if (!isprint(ch)) {
				if (f != NULL)
					fprintf(f, "\\%03o", ch);
				r += 4;
				continue;
			}
			goto prnt;
		}
		
		if (f != NULL)
			fputc('\\', f);
		r++;
prnt:
		if (f != NULL)
			fputc(ch, f);
		r++;
	}
	return r;
}

static int
ent_compare(const void *a, const void *b)
{
	const TIENT *ta, *tb;

	ta = (const TIENT *)a;
	tb = (const TIENT *)b;
	return strcmp(ta->id, tb->id);
}

static void
setdb(char *db)
{
	size_t len;

	len = strlen(db);
	if (len > 3 &&
	    db[len - 3] == '.' &&
	    db[len - 2] == 'd' &&
	    db[len - 1] == 'b')
		db[len - 3] = '\0';
	setenv("TERMINFO", db, 1);
}

static void
print_ent(const TIENT *ents, size_t nents)
{
	size_t col, i, l;
	char nbuf[64];	

	if (nents == 0)
		return;
	
	col = SW;
	printf("\t");
	for (i = 0; i < nents; i++) {
		if (*ents[i].id == '.' && aflag == 0)
			continue;
		switch (ents[i].type) {
		case 'f':
			if (ents[i].flag == ABSENT_BOOLEAN)
				continue;
			l = strlen(ents[i].id) + 2;
			if (ents[i].flag == CANCELLED_BOOLEAN)
				l++;
			break;
		case 'n':
			if (ents[i].num == ABSENT_NUMERIC)
				continue;
			if (VALID_NUMERIC(ents[i].num))
				l = snprintf(nbuf, sizeof(nbuf), "%s#%d,",
				    ents[i].id, ents[i].num);
			else
				l = snprintf(nbuf, sizeof(nbuf), "%s@,",
				    ents[i].id);
			break;
		case 's':
			if (ents[i].str == ABSENT_STRING)
				continue;
			if (VALID_STRING(ents[i].str))
				l = strlen(ents[i].id) +
				    outstr(NULL, ents[i].str) + 7;
			else
				l = strlen(ents[i].id) + 3;
			break;
		default:
			errx(1, "invalid type");
		}
		if (col != SW) {
			if (col + l > cols) {
				printf("\n\t");
				col = SW;
			} else
				col += printf(" ");
		}
		switch (ents[i].type) {
		case 'f':
			col += printf("%s", ents[i].id);
			if (ents[i].flag == ABSENT_BOOLEAN ||
			    ents[i].flag == CANCELLED_BOOLEAN)
				col += printf("@");
			col += printf(",");
			break;
		case 'n':
			col += printf("%s", nbuf);
			break;
		case 's':
			col += printf("%s", ents[i].id);
			if (VALID_STRING(ents[i].str)) {
				col += printf("=");
				col += outstr(stdout, ents[i].str);
			} else
				col += printf("@");
			col += printf(",");
			break;
		}
	}
	printf("\n");
}

static size_t
load_ents(TIENT *ents, TERMINAL *t, char type)
{
	size_t i, n, max;
	TERMUSERDEF *ud;

	switch (type) {
	case 'f':
		max = TIFLAGMAX;
		break;
	case 'n':
		max = TINUMMAX;
		break;
	default:
		max = TISTRMAX;
	}
	
	n = 0;
	for (i = 0; i <= max; i++) {
		switch (type) {
		case 'f':
			if (t->flags[i] == 1 ||
			    (aflag && t->flags[i] == CANCELLED_BOOLEAN))
			{
				ents[n].id = _ti_flagid(i);
				ents[n].type = 'f';
				ents[n++].flag = t->flags[i];
			}
			break;
		case 'n':
			if (VALID_NUMERIC(t->nums[i]) ||
			    (aflag && t->nums[i] == CANCELLED_NUMERIC))
			{
				ents[n].id = _ti_numid(i);
				ents[n].type = 'n';
				ents[n++].num = t->nums[i];
			}
			break;
		default:
			if (VALID_STRING(t->strs[i]) ||
			    (aflag && t->strs[i] == CANCELLED_STRING))
			{
				ents[n].id = _ti_strid(i);
				ents[n].type = 's';
				ents[n++].str = t->strs[i];
			}
			break;
		}
	}
	
	if (xflag != 0 && t->_nuserdefs != 0) {
		for (i = 0; i < t->_nuserdefs; i++) {
			ud = &t->_userdefs[i];
			if (ud->type == type) {
				switch (type) {
				case 'f':
					if (!aflag &&
					    !VALID_BOOLEAN(ud->flag))
						continue;
					break;
				case 'n':
					if (!aflag &&
					    !VALID_NUMERIC(ud->num))
						continue;
					break;
				case 's':
					if (!aflag &&
					    !VALID_STRING(ud->str))
						continue;
					break;
				}
				ents[n].id = ud->id;
				ents[n].type = ud->type;
				ents[n].flag = ud->flag;
				ents[n].num = ud->num;
				ents[n++].str = ud->str;
			}
		}
	}
	
	qsort(ents, n, sizeof(TIENT), ent_compare);
	return n;
}

static void
cprint_ent(TIENT *ent)
{

	if (ent == NULL) {
		if (qflag == 0)
			printf("NULL");
		else
			printf("-");
	}
	
	switch (ent->type) {
	case 'f':
		if (VALID_BOOLEAN(ent->flag))
			printf(ent->flag == 1 ? "T" : "F");
		else if (qflag == 0)
			printf("F");
		else if (ent->flag == CANCELLED_BOOLEAN)
			printf("@");
		else
			printf("-");
		break;
	case 'n':
		if (VALID_NUMERIC(ent->num))
			printf("%d", ent->num);
		else if (qflag == 0)
			printf("NULL");
		else if (ent->num == CANCELLED_NUMERIC)
			printf("@");
		else
			printf("-");
		break;
	case 's':
		if (VALID_STRING(ent->str)) {
			printf("'");
			outstr(stdout, ent->str);
			printf("'");
		} else if (qflag == 0)
			printf("NULL");
		else if (ent->str == CANCELLED_STRING)
			printf("@");
		else
			printf("-");
		break;
	}
}

static void
compare_ents(TIENT *ents1, size_t n1, TIENT *ents2, size_t n2)
{
	size_t i1, i2;
	TIENT *e1, *e2, ee;
	int c;
	
	i1 = i2 = 0;
	ee.type = 'f';
	ee.flag = ABSENT_BOOLEAN;
	ee.num = ABSENT_NUMERIC;
	ee.str = ABSENT_STRING;
	while (i1 != n1 || i2 != n2) {
		if (i1 == n1)
			c = 1;
		else if (i2 == n2)
			c = -1;
		else
			c = strcmp(ents1[i1].id, ents2[i2].id);
		if (c == 0) {
			e1 = &ents1[i1++];
			e2 = &ents2[i2++];
		} else if (c < 0) {
			e1 = &ents1[i1++];
			e2 = &ee;
			ee.id = e1->id;
			ee.type = e1->type;
		} else {
			e1 = &ee;
			e2 = &ents2[i2++];
			ee.id = e2->id;
			ee.type = e2->type;
		}
		switch (e1->type) {
		case 'f':
			if (cflag != 0) {
				if (e1->flag == e2->flag)
					printf("\t%s\n", ents1[i1].id);
				continue;
			}
			if (e1->flag == e2->flag)
				continue;
			break;
		case 'n':
			if (cflag != 0) {
				if (e1->num == e2->num)
					printf("\t%s#%d\n",
					    ents1[i1].id, ents1[i1].num);
				continue;
			}
			if (e1->num == e2->num)
				continue;
			break;
		case 's':
			if (cflag != 0) {
				if (VALID_STRING(e1->str) &&
				    VALID_STRING(e2->str) &&
				    strcmp(e1->str, e2->str) == 0) {
					printf("\t%s=", ents1[i1].id);
					outstr(stdout, ents1[i1].str);
					printf("\n");
				}
				continue;
			}
			if (VALID_STRING(e1->str) &&
			    VALID_STRING(e2->str) &&
			    strcmp(e1->str, e2->str) == 0)
				continue;
			break;
		}
		printf("\t%s: ", e1->id);
		cprint_ent(e1);
		if (e1->type == 'f')
			printf(":");
		else
			printf(", ");
		cprint_ent(e2);
		printf(".\n");
	}
}

static TERMINAL *
load_term(const char *name)
{
	TERMINAL *t;

	t = calloc(1, sizeof(*t));
	if (t == NULL)
		err(1, "calloc");
	if (name == NULL)
		name = getenv("TERM");
	if (name == NULL)
		name = "dumb";
	if (_ti_getterm(t, name, 1) == 1)
		return t;

	if (_ti_database == NULL)
		errx(1, "no terminal definition found in internal database");
	else
		errx(1, "no terminal definition found in %s.db", _ti_database);
}

static void
show_missing(TERMINAL *t1, TERMINAL *t2, char type)
{
	ssize_t i, max;
	const char *id;
	
	switch (type) {
	case 'f':
		max = TIFLAGMAX;
		break;
	case 'n':
		max = TINUMMAX;
		break;
	default:
		max = TISTRMAX;
	}

	for (i = 0; i <= max; i++) {
		switch (type) {
		case 'f':
			if (t1->flags[i] != ABSENT_BOOLEAN ||
			    t2->flags[i] != ABSENT_BOOLEAN)
				continue;
			id = _ti_flagid(i);
			break;
		case 'n':
			if (t1->nums[i] != ABSENT_NUMERIC ||
			    t2->nums[i] != ABSENT_NUMERIC)
				continue;
			id = _ti_numid(i);
			break;
		default:
			if (t1->strs[i] != ABSENT_STRING ||
			    t2->strs[i] != ABSENT_STRING)
				continue;
			id = _ti_strid(i);
			break;
		}
		printf("\t!%s.\n", id);
	}
}

static TERMUSERDEF *
find_userdef(TERMINAL *term, const char *id)
{
	size_t i;

	for (i = 0; i < term->_nuserdefs; i++)
		if (strcmp(term->_userdefs[i].id, id) == 0)
			return &term->_userdefs[i];
	return NULL;
}

static void
use_terms(TERMINAL *term, size_t nuse, char **uterms)
{
	TERMINAL **terms;
	TERMUSERDEF *ud, *tud;
	size_t i, j, agree, absent, data;

	terms = malloc(sizeof(**terms) * nuse);
	if (terms == NULL)
		err(1, "malloc");
	for (i = 0; i < nuse; i++) {
		if (strcmp(term->name, *uterms) == 0)
			errx(1, "cannot use same terminal");
		for (j = 0; j < i; j++)
			if (strcmp(terms[j]->name, *uterms) == 0)
				errx(1, "cannot use same terminal");
		terms[i] = load_term(*uterms++);
	}
	
	for (i = 0; i < TIFLAGMAX + 1; i++) {
		agree = absent = data = 0;
		for (j = 0; j < nuse; j++) {
			if (terms[j]->flags[i] == ABSENT_BOOLEAN ||
			    terms[j]->flags[i] == CANCELLED_BOOLEAN)
				absent++;
			else {
				data++;
				if (term->flags[i] == terms[j]->flags[i])
					agree++;
			}
		}
		if (data == 0)
			continue;
		if (agree > 0 && agree + absent == nuse)
			term->flags[i] = ABSENT_BOOLEAN;
		else if (term->flags[i] == ABSENT_BOOLEAN)
			term->flags[i] = CANCELLED_BOOLEAN;
	}
	
	for (i = 0; i < TINUMMAX + 1; i++) {
		agree = absent = data = 0;
		for (j = 0; j < nuse; j++) {
			if (terms[j]->nums[i] == ABSENT_NUMERIC ||
			    terms[j]->nums[i] == CANCELLED_NUMERIC)
				absent++;
			else {
				data++;
				if (term->nums[i] == terms[j]->nums[i])
					agree++;
			}
		}
		if (data == 0)
			continue;
		if (agree > 0 && agree + absent == nuse)
			term->nums[i] = ABSENT_NUMERIC;
		else if (term->nums[i] == ABSENT_NUMERIC)
			term->nums[i] = CANCELLED_NUMERIC;
	}
	
	for (i = 0; i < TISTRMAX + 1; i++) {
		agree = absent = data = 0;
		for (j = 0; j < nuse; j++) {
			if (terms[j]->strs[i] == ABSENT_STRING ||
			    terms[j]->strs[i] == CANCELLED_STRING)
				absent++;
			else {
				data++;
				if (VALID_STRING(term->strs[i]) &&
				    strcmp(term->strs[i],
					terms[j]->strs[i]) == 0)
					agree++;
			}
		}
		if (data == 0)
			continue;
		if (agree > 0 && agree + absent == nuse)
			term->strs[i] = ABSENT_STRING;
		else if (term->strs[i] == ABSENT_STRING)
			term->strs[i] = CANCELLED_STRING;
	}

	/* User defined caps are more tricky.
	   First we set any to absent that agree. */
	for (i = 0; i < term->_nuserdefs; i++) {
		agree = absent = data = 0;
		ud = &term->_userdefs[i];
		for (j = 0; j < nuse; j++) {
			tud = find_userdef(terms[j], ud->id);
			if (tud == NULL)
				absent++;
			else {
				data++;
				switch (ud->type) {
				case 'f':
					if (tud->type == 'f' &&
					    tud->flag == ud->flag)
						agree++;
					break;
				case 'n':
					if (tud->type == 'n' &&
					    tud->num == ud->num)
						agree++;
					break;
				case 's':
					if (tud->type == 's' &&
					    VALID_STRING(tud->str) &&
					    VALID_STRING(ud->str) &&
					    strcmp(ud->str, tud->str) == 0)
						agree++;
					break;
				}
			}
		}
		if (data == 0)
			continue;
		if (agree > 0 && agree + absent == nuse) {
			ud->flag = ABSENT_BOOLEAN;
			ud->num = ABSENT_NUMERIC;
			ud->str = ABSENT_STRING;
		}
	}

	/* Now add any that we don't have as cancelled */
	for (i = 0; i < nuse; i++) {
		for (j = 0; j < terms[i]->_nuserdefs; j++) {
			ud = find_userdef(term, terms[i]->_userdefs[j].id);
			if (ud != NULL)
				continue; /* We have handled this */
			term->_userdefs = realloc(term->_userdefs,
			    sizeof(*term->_userdefs) * (term->_nuserdefs + 1));
			if (term->_userdefs == NULL)
				err(1, "malloc");
			tud = &term->_userdefs[term->_nuserdefs++];
			tud->id = terms[i]->_userdefs[j].id;
			tud->type = terms[i]->_userdefs[j].flag;
			tud->flag = CANCELLED_BOOLEAN;
			tud->num = CANCELLED_NUMERIC;
			tud->str = CANCELLED_STRING;
		}
	}
}

int
main(int argc, char **argv)
{
	char *term, *Barg;
	int ch, uflag;
	TERMINAL *t, *t2;
	size_t n, n2;
	struct winsize ws;
	TIENT ents[TISTRMAX + 1], ents2[TISTRMAX + 1];

	cols = 80; /* default */
	term = getenv("COLUMNS");
	if (term != NULL)
		cols = strtoul(term, NULL, 10);
	else if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0)
		cols = ws.ws_col;

	uflag = xflag = 0;
	Barg = NULL;
	while ((ch = getopt(argc, argv, "1A:B:acnquw:x")) != -1)
		switch (ch) {
		case '1':
			cols = 1;
			break;
		case 'A':
			setdb(optarg);
			break;
		case 'B':
			Barg = optarg;
			break;
		case 'a':
			aflag = 1;
			break;
		case 'c':
			cflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'q':
			qflag = 1;
			break;
		case 'u':
			uflag = 1;
			aflag = 1;
			break;
		case 'w':
			cols = strtoul(optarg, NULL, 10);
			break;
		case 'x':
			xflag = 1;
			break;
		case '?':
		default:
			fprintf(stderr,
			    "usage: %s [-1acnqux] [-A database] [-B database] "
			    "[-w cols] [term]\n",
			    getprogname());
			return EXIT_FAILURE;
		}
	cols--;

	if (optind + 1 < argc)
		aflag = 1;

	if (optind < argc)
		term = argv[optind++];
	else
		term = NULL;
	t = load_term(term);

	if (uflag != 0)
		use_terms(t, argc - optind, argv + optind);

	if ((optind + 1 != argc && nflag == 0) || uflag != 0) {
		if (uflag == 0) {
			printf("# Reconstructed from ");
			if (_ti_database == NULL)
				printf("internal database\n");
			else
				printf("%s%s\n", _ti_database,
				    *_ti_database == '/' ? ".cdb" : "");
		}
		printf("%s", t->name);
		if (t->_alias != NULL && *t->_alias != '\0')
			printf("|%s", t->_alias);
		if (t->desc != NULL && *t->desc != '\0')
			printf("|%s", t->desc);
		printf(",\n");

		n = load_ents(ents, t, 'f');
		print_ent(ents, n);
		n = load_ents(ents, t, 'n');
		print_ent(ents, n);
		n = load_ents(ents, t, 's');
		print_ent(ents, n);

		if (uflag != 0) {
			printf("\t");
			n = SW;
			for (; optind < argc; optind++) {
				n2 = 5 + strlen(argv[optind]);
				if (n != SW) {
					if (n + n2 > cols) {
						printf("\n\t");
						n = SW;
					} else
						n += printf(" ");
				}
				n += printf("use=%s,", argv[optind]);
			}
			printf("\n");
		}
		return EXIT_SUCCESS;
	}

	if (Barg == NULL)
		unsetenv("TERMINFO");
	else
		setdb(Barg);
	t2 = load_term(argv[optind++]);
	printf("comparing %s to %s.\n", t->name, t2->name);
	if (qflag == 0)
		printf("    comparing booleans.\n");
	if (nflag == 0) {
		n = load_ents(ents, t, 'f');
		n2 = load_ents(ents2, t2, 'f');
		compare_ents(ents, n, ents2, n2);
	} else
		show_missing(t, t2, 'f');
	if (qflag == 0)
		printf("    comparing numbers.\n");
	if (nflag == 0) {
		n = load_ents(ents, t, 'n');
		n2 = load_ents(ents2, t2, 'n');
		compare_ents(ents, n, ents2, n2);
	} else
		show_missing(t, t2, 'n');
	if (qflag == 0)
		printf("    comparing strings.\n");
	if (nflag == 0) {
		n = load_ents(ents, t, 's');
		n2 = load_ents(ents2, t2, 's');
		compare_ents(ents, n, ents2, n2);
	} else
		show_missing(t, t2, 's');
	return EXIT_SUCCESS;
}

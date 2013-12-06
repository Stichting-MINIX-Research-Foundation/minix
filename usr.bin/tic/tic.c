/* $NetBSD: tic.c,v 1.23 2012/12/08 23:29:28 joerg Exp $ */

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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
__RCSID("$NetBSD: tic.c,v 1.23 2012/12/08 23:29:28 joerg Exp $");

#include <sys/types.h>
#include <sys/queue.h>

#if !HAVE_NBTOOL_CONFIG_H || HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif

#include <cdbw.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <fcntl.h>
#include <search.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <term_private.h>
#include <term.h>
#include <util.h>

#define	HASH_SIZE	16384	/* 2012-06-01: 3600 entries */

typedef struct term {
	STAILQ_ENTRY(term) next;
	char *name;
	TIC *tic;
	uint32_t id;
	struct term *base_term;
} TERM;
static STAILQ_HEAD(, term) terms = STAILQ_HEAD_INITIALIZER(terms);

static int error_exit;
static int Sflag;
static size_t nterm, nalias;

static void __printflike(1, 2)
dowarn(const char *fmt, ...)
{
	va_list va;

	error_exit = 1;
	va_start(va, fmt);
	vwarnx(fmt, va);
	va_end(va);
}

static char *
grow_tbuf(TBUF *tbuf, size_t len)
{
	char *buf;

	buf = _ti_grow_tbuf(tbuf, len);
	if (buf == NULL)
		err(1, "_ti_grow_tbuf");
	return buf;
}

static int
save_term(struct cdbw *db, TERM *term)
{
	uint8_t *buf;
	ssize_t len;
	size_t slen = strlen(term->name) + 1;

	if (term->base_term != NULL) {
		len = (ssize_t)slen + 7;
		buf = emalloc(len);
		buf[0] = 2;
		le32enc(buf + 1, term->base_term->id);
		le16enc(buf + 5, slen);
		memcpy(buf + 7, term->name, slen);
		if (cdbw_put(db, term->name, slen, buf, len))
			err(1, "cdbw_put");
		return 0;
	}

	len = _ti_flatten(&buf, term->tic);
	if (len == -1)
		return -1;

	if (cdbw_put_data(db, buf, len, &term->id))
		err(1, "cdbw_put_data");
	if (cdbw_put_key(db, term->name, slen, term->id))
		err(1, "cdbw_put_key");
	free(buf);
	return 0;
}

static TERM *
find_term(const char *name)
{
	ENTRY elem, *elemp;

	elem.key = __UNCONST(name);
	elem.data = NULL;
	elemp = hsearch(elem, FIND);
	return elemp ? (TERM *)elemp->data : NULL;
}

static TERM *
store_term(const char *name, TERM *base_term)
{
	TERM *term;
	ENTRY elem;

	term = ecalloc(1, sizeof(*term));
	term->name = estrdup(name);
	STAILQ_INSERT_TAIL(&terms, term, next);
	elem.key = estrdup(name);
	elem.data = term;
	hsearch(elem, ENTER);

	term->base_term = base_term;
	if (base_term != NULL)
		nalias++;
	else
		nterm++;

	return term;
}

static int
process_entry(TBUF *buf, int flags)
{
	char *p, *e, *alias;
	TERM *term;
	TIC *tic;
	
	if (buf->bufpos == 0)
		return 0;
	/* Terminate the string */
	buf->buf[buf->bufpos - 1] = '\0';
	/* First rewind the buffer for new entries */
	buf->bufpos = 0;

	if (isspace((unsigned char)*buf->buf))
		return 0;

	tic = _ti_compile(buf->buf, flags);
	if (tic == NULL)
		return 0;

	if (find_term(tic->name) != NULL) {
		dowarn("%s: duplicate entry", tic->name);
		_ti_freetic(tic);
		return 0;
	}
	term = store_term(tic->name, NULL);
	term->tic = tic;

	/* Create aliased terms */
	if (tic->alias != NULL) {
		alias = p = estrdup(tic->alias);
		while (p != NULL && *p != '\0') {
			e = strchr(p, '|');
			if (e != NULL)
				*e++ = '\0';
			if (find_term(p) != NULL) {
				dowarn("%s: has alias for already assigned"
				    " term %s", tic->name, p);
			} else {
				store_term(p, term);
			}
			p = e;
		}
		free(alias);
	}
	
	return 0;
}

static void
merge(TIC *rtic, TIC *utic, int flags)
{
	char *cap, flag, *code, type, *str;
	short ind, num;
	size_t n;

	cap = utic->flags.buf;
	for (n = utic->flags.entries; n > 0; n--) {
		ind = le16dec(cap);
		cap += sizeof(uint16_t);
		flag = *cap++;
		if (VALID_BOOLEAN(flag) &&
		    _ti_find_cap(&rtic->flags, 'f', ind) == NULL)
		{
			_ti_grow_tbuf(&rtic->flags, sizeof(uint16_t) + 1);
			le16enc(rtic->flags.buf + rtic->flags.bufpos, ind);
			rtic->flags.bufpos += sizeof(uint16_t);
			rtic->flags.buf[rtic->flags.bufpos++] = flag;
			rtic->flags.entries++;
		}
	}

	cap = utic->nums.buf;
	for (n = utic->nums.entries; n > 0; n--) {
		ind = le16dec(cap);
		cap += sizeof(uint16_t);
		num = le16dec(cap);
		cap += sizeof(uint16_t);
		if (VALID_NUMERIC(num) &&
		    _ti_find_cap(&rtic->nums, 'n', ind) == NULL)
		{
			grow_tbuf(&rtic->nums, sizeof(uint16_t) * 2);
			le16enc(rtic->nums.buf + rtic->nums.bufpos, ind);
			rtic->nums.bufpos += sizeof(uint16_t);
			le16enc(rtic->nums.buf + rtic->nums.bufpos, num);
			rtic->nums.bufpos += sizeof(uint16_t);
			rtic->nums.entries++;
		}
	}

	cap = utic->strs.buf;
	for (n = utic->strs.entries; n > 0; n--) {
		ind = le16dec(cap);
		cap += sizeof(uint16_t);
		num = le16dec(cap);
		cap += sizeof(uint16_t);
		if (num > 0 &&
		    _ti_find_cap(&rtic->strs, 's', ind) == NULL)
		{
			grow_tbuf(&rtic->strs, (sizeof(uint16_t) * 2) + num);
			le16enc(rtic->strs.buf + rtic->strs.bufpos, ind);
			rtic->strs.bufpos += sizeof(uint16_t);
			le16enc(rtic->strs.buf + rtic->strs.bufpos, num);
			rtic->strs.bufpos += sizeof(uint16_t);
			memcpy(rtic->strs.buf + rtic->strs.bufpos,
			    cap, num);
			rtic->strs.bufpos += num;
			rtic->strs.entries++;
		}
		cap += num;
	}

	cap = utic->extras.buf;
	for (n = utic->extras.entries; n > 0; n--) {
		num = le16dec(cap);
		cap += sizeof(uint16_t);
		code = cap;
		cap += num;
		type = *cap++;
		flag = 0;
		str = NULL;
		switch (type) {
		case 'f':
			flag = *cap++;
			if (!VALID_BOOLEAN(flag))
				continue;
			break;
		case 'n':
			num = le16dec(cap);
			cap += sizeof(uint16_t);
			if (!VALID_NUMERIC(num))
				continue;
			break;
		case 's':
			num = le16dec(cap);
			cap += sizeof(uint16_t);
			str = cap;
			cap += num;
			if (num == 0)
				continue;
			break;
		}
		_ti_store_extra(rtic, 0, code, type, flag, num, str, num,
		    flags);
	}
}

static size_t
merge_use(int flags)
{
	size_t skipped, merged, memn;
	char *cap, *scap;
	uint16_t num;
	TIC *rtic, *utic;
	TERM *term, *uterm;;

	skipped = merged = 0;
	STAILQ_FOREACH(term, &terms, next) {
		if (term->base_term != NULL)
			continue;
		rtic = term->tic;
		while ((cap = _ti_find_extra(&rtic->extras, "use")) != NULL) {
			if (*cap++ != 's') {
				dowarn("%s: use is not string", rtic->name);
				break;
			}
			cap += sizeof(uint16_t);
			if (strcmp(rtic->name, cap) == 0) {
				dowarn("%s: uses itself", rtic->name);
				goto remove;
			}
			uterm = find_term(cap);
			if (uterm != NULL && uterm->base_term != NULL)
				uterm = uterm->base_term;
			if (uterm == NULL) {
				dowarn("%s: no use record for %s",
				    rtic->name, cap);
				goto remove;
			}
			utic = uterm->tic;
			if (strcmp(utic->name, rtic->name) == 0) {
				dowarn("%s: uses itself", rtic->name);
				goto remove;
			}
			if (_ti_find_extra(&utic->extras, "use") != NULL) {
				skipped++;
				break;
			}
			cap = _ti_find_extra(&rtic->extras, "use");
			merge(rtic, utic, flags);
	remove:
			/* The pointers may have changed, find the use again */
			cap = _ti_find_extra(&rtic->extras, "use");
			if (cap == NULL)
				dowarn("%s: use no longer exists - impossible",
					rtic->name);
			else {
				scap = cap - (4 + sizeof(uint16_t));
				cap++;
				num = le16dec(cap);
				cap += sizeof(uint16_t) + num;
				memn = rtic->extras.bufpos -
				    (cap - rtic->extras.buf);
				memmove(scap, cap, memn);
				rtic->extras.bufpos -= cap - scap;
				cap = scap;
				rtic->extras.entries--;
				merged++;
			}
		}
	}

	if (merged == 0 && skipped != 0)
		dowarn("circular use detected");
	return merged;
}

static int
print_dump(int argc, char **argv)
{
	TERM *term;
	uint8_t *buf;
	int i, n;
	size_t j, col;
	ssize_t len;

	printf("struct compiled_term {\n");
	printf("\tconst char *name;\n");
	printf("\tconst char *cap;\n");
	printf("\tsize_t caplen;\n");
	printf("};\n\n");

	printf("const struct compiled_term compiled_terms[] = {\n");

	n = 0;
	for (i = 0; i < argc; i++) {
		term = find_term(argv[i]);
		if (term == NULL) {
			warnx("%s: no description for terminal", argv[i]);
			continue;
		}
		if (term->base_term != NULL) {
			warnx("%s: cannot dump alias", argv[i]);
			continue;
		}
		/* Don't compile the aliases in, save space */
		free(term->tic->alias);
		term->tic->alias = NULL;
		len = _ti_flatten(&buf, term->tic);
		if (len == 0 || len == -1)
			continue;

		printf("\t{\n");
		printf("\t\t\"%s\",\n", argv[i]);
		n++;
		for (j = 0, col = 0; j < (size_t)len; j++) {
			if (col == 0) {
				printf("\t\t\"");
				col = 16;
			}
			
			col += printf("\\%03o", (uint8_t)buf[j]);
			if (col > 75) {
				printf("\"%s\n",
				    j + 1 == (size_t)len ? "," : "");
				col = 0;
			}
		}
		if (col != 0)
			printf("\",\n");
		printf("\t\t%zu\n", len);
		printf("\t}");
		if (i + 1 < argc)
			printf(",");
		printf("\n");
		free(buf);
	}
	printf("};\n");

	return n;
}

static void
write_database(const char *dbname)
{
	struct cdbw *db;
	char *tmp_dbname;
	TERM *term;
	int fd;

	db = cdbw_open();
	if (db == NULL)
		err(1, "cdbw_open failed");
	/* Save the terms */
	STAILQ_FOREACH(term, &terms, next)
		save_term(db, term);

	easprintf(&tmp_dbname, "%s.XXXXXX", dbname);
	fd = mkstemp(tmp_dbname);
	if (fd == -1)
		err(1, "creating temporary database %s failed", tmp_dbname);
	if (cdbw_output(db, fd, "NetBSD terminfo", cdbw_stable_seeder))
		err(1, "writing temporary database %s failed", tmp_dbname);
	if (fchmod(fd, DEFFILEMODE))
		err(1, "fchmod failed");
	if (close(fd))
		err(1, "writing temporary database %s failed", tmp_dbname);
	if (rename(tmp_dbname, dbname))
		err(1, "renaming %s to %s failed", tmp_dbname, dbname);
	free(tmp_dbname);
	cdbw_close(db);
}

int
main(int argc, char **argv)
{
	int ch, cflag, sflag, flags;
	char *source, *dbname, *buf, *ofile;
	FILE *f;
	size_t buflen;
	ssize_t len;
	TBUF tbuf;

	cflag = sflag = 0;
	ofile = NULL;
	flags = TIC_ALIAS | TIC_DESCRIPTION | TIC_WARNING;
	while ((ch = getopt(argc, argv, "Saco:sx")) != -1)
	    switch (ch) {
	    case 'S':
		    Sflag = 1;
		    /* We still compile aliases so that use= works.
		     * However, it's removed before we flatten to save space. */
		    flags &= ~TIC_DESCRIPTION;
		    break;
	    case 'a':
		    flags |= TIC_COMMENT;
		    break;
	    case 'c':
		    cflag = 1;
		    break;
	    case 'o':
		    ofile = optarg;
		    break;
	    case 's':
		    sflag = 1;
		    break;
	    case 'x':
		    flags |= TIC_EXTRA;
		    break;
	    case '?': /* FALLTHROUGH */
	    default:
		    fprintf(stderr, "usage: %s [-acSsx] [-o file] source\n",
			getprogname());
		    return EXIT_FAILURE;
	    }

	if (optind == argc)
		errx(1, "No source file given");
	source = argv[optind++];
	f = fopen(source, "r");
	if (f == NULL)
		err(1, "fopen: %s", source);

	hcreate(HASH_SIZE);

	buf = tbuf.buf = NULL;
	buflen = tbuf.buflen = tbuf.bufpos = 0;	
	while ((len = getline(&buf, &buflen, f)) != -1) {
		/* Skip comments */
		if (*buf == '#')
			continue;
		if (buf[len - 1] != '\n') {
			process_entry(&tbuf, flags);
			dowarn("last line is not a comment"
			    " and does not end with a newline");
			continue;
		}
		/*
		  If the first char is space not a space then we have a
		  new entry, so process it.
		*/
		if (!isspace((unsigned char)*buf) && tbuf.bufpos != 0)
			process_entry(&tbuf, flags);
		
		/* Grow the buffer if needed */
		grow_tbuf(&tbuf, len);
		/* Append the string */
		memcpy(tbuf.buf + tbuf.bufpos, buf, len);
		tbuf.bufpos += len;
	}
	free(buf);
	/* Process the last entry if not done already */
	process_entry(&tbuf, flags);
	free(tbuf.buf);

	/* Merge use entries until we have merged all we can */
	while (merge_use(flags) != 0)
		;

	if (Sflag) {
		print_dump(argc - optind, argv + optind);
		return error_exit;
	}

	if (cflag)
		return error_exit;

	if (ofile == NULL)
		easprintf(&dbname, "%s.cdb", source);
	else
		dbname = ofile;
	write_database(dbname);

	if (sflag != 0)
		fprintf(stderr, "%zu entries and %zu aliases written to %s\n",
		    nterm, nalias, dbname);

#ifdef __VALGRIND__
	if (ofile == NULL)
		free(dbname);
	while ((term = STAILQ_FIRST(&terms)) != NULL) {
		STAILQ_REMOVE_HEAD(&terms, next);
		_ti_freetic(term->tic);
		free(term->name);
		free(term);
	}
	hdestroy();
#endif


	return EXIT_SUCCESS;
}

/*	Id: mansearch.c,v 1.17 2014/01/05 04:13:52 schwarze Exp  */
/*
 * Copyright (c) 2012 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2013, 2014 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_OHASH
#include <ohash.h>
#else
#include "compat_ohash.h"
#endif
#include <sqlite3.h>

#include "mandoc.h"
#include "manpath.h"
#include "mansearch.h"

#define	SQL_BIND_TEXT(_db, _s, _i, _v) \
	do { if (SQLITE_OK != sqlite3_bind_text \
		((_s), (_i)++, (_v), -1, SQLITE_STATIC)) \
		fprintf(stderr, "%s\n", sqlite3_errmsg((_db))); \
	} while (0)
#define	SQL_BIND_INT64(_db, _s, _i, _v) \
	do { if (SQLITE_OK != sqlite3_bind_int64 \
		((_s), (_i)++, (_v))) \
		fprintf(stderr, "%s\n", sqlite3_errmsg((_db))); \
	} while (0)
#define	SQL_BIND_BLOB(_db, _s, _i, _v) \
	do { if (SQLITE_OK != sqlite3_bind_blob \
		((_s), (_i)++, (&_v), sizeof(_v), SQLITE_STATIC)) \
		fprintf(stderr, "%s\n", sqlite3_errmsg((_db))); \
	} while (0)

struct	expr {
	uint64_t 	 bits;    /* type-mask */
	const char	*substr;  /* to search for, if applicable */
	regex_t		 regexp;  /* compiled regexp, if applicable */
	int		 open;    /* opening parentheses before */
	int		 and;	  /* logical AND before */
	int		 close;   /* closing parentheses after */
	struct expr	*next;    /* next in sequence */
};

struct	match {
	uint64_t	 id; /* identifier in database */
	char		*desc; /* description of manpage */
	int		 form; /* 0 == catpage */
};

struct	type {
	uint64_t	 bits;
	const char	*name;
};

static	const struct type types[] = {
	{ TYPE_An,  "An" },
	{ TYPE_Ar,  "Ar" },
	{ TYPE_At,  "At" },
	{ TYPE_Bsx, "Bsx" },
	{ TYPE_Bx,  "Bx" },
	{ TYPE_Cd,  "Cd" },
	{ TYPE_Cm,  "Cm" },
	{ TYPE_Dv,  "Dv" },
	{ TYPE_Dx,  "Dx" },
	{ TYPE_Em,  "Em" },
	{ TYPE_Er,  "Er" },
	{ TYPE_Ev,  "Ev" },
	{ TYPE_Fa,  "Fa" },
	{ TYPE_Fl,  "Fl" },
	{ TYPE_Fn,  "Fn" },
	{ TYPE_Fn,  "Fo" },
	{ TYPE_Ft,  "Ft" },
	{ TYPE_Fx,  "Fx" },
	{ TYPE_Ic,  "Ic" },
	{ TYPE_In,  "In" },
	{ TYPE_Lb,  "Lb" },
	{ TYPE_Li,  "Li" },
	{ TYPE_Lk,  "Lk" },
	{ TYPE_Ms,  "Ms" },
	{ TYPE_Mt,  "Mt" },
	{ TYPE_Nd,  "Nd" },
	{ TYPE_Nm,  "Nm" },
	{ TYPE_Nx,  "Nx" },
	{ TYPE_Ox,  "Ox" },
	{ TYPE_Pa,  "Pa" },
	{ TYPE_Rs,  "Rs" },
	{ TYPE_Sh,  "Sh" },
	{ TYPE_Ss,  "Ss" },
	{ TYPE_St,  "St" },
	{ TYPE_Sy,  "Sy" },
	{ TYPE_Tn,  "Tn" },
	{ TYPE_Va,  "Va" },
	{ TYPE_Va,  "Vt" },
	{ TYPE_Xr,  "Xr" },
	{ TYPE_sec, "sec" },
	{ TYPE_arch,"arch" },
	{ ~0ULL,    "any" },
	{ 0ULL, NULL }
};

static	void		 buildnames(struct manpage *, sqlite3 *,
				sqlite3_stmt *, uint64_t, const char *);
static	char		*buildoutput(sqlite3 *, sqlite3_stmt *,
				 uint64_t, uint64_t);
static	void		*hash_alloc(size_t, void *);
static	void		 hash_free(void *, size_t, void *);
static	void		*hash_halloc(size_t, void *);
static	struct expr	*exprcomp(const struct mansearch *, 
				int, char *[]);
static	void		 exprfree(struct expr *);
static	struct expr	*exprspec(struct expr *, uint64_t,
				 const char *, const char *);
static	struct expr	*exprterm(const struct mansearch *, char *, int);
static	void		 sql_append(char **sql, size_t *sz,
				const char *newstr, int count);
static	void		 sql_match(sqlite3_context *context,
				int argc, sqlite3_value **argv);
static	void		 sql_regexp(sqlite3_context *context,
				int argc, sqlite3_value **argv);
static	char		*sql_statement(const struct expr *);

int
mansearch(const struct mansearch *search,
		const struct manpaths *paths,
		int argc, char *argv[],
		const char *outkey,
		struct manpage **res, size_t *sz)
{
	int		 fd, rc, c, ibit;
	int64_t		 id;
	uint64_t	 outbit;
	char		 buf[PATH_MAX];
	char		*sql;
	struct manpage	*mpage;
	struct expr	*e, *ep;
	sqlite3		*db;
	sqlite3_stmt	*s, *s2;
	struct match	*mp;
	struct ohash_info info;
	struct ohash	 htab;
	unsigned int	 idx;
	size_t		 i, j, cur, maxres;

	memset(&info, 0, sizeof(struct ohash_info));

	info.halloc = hash_halloc;
	info.alloc = hash_alloc;
	info.hfree = hash_free;
	info.key_offset = offsetof(struct match, id);

	*sz = cur = maxres = 0;
	sql = NULL;
	*res = NULL;
	fd = -1;
	e = NULL;
	rc = 0;

	if (0 == argc)
		goto out;
	if (NULL == (e = exprcomp(search, argc, argv)))
		goto out;

	outbit = 0;
	if (NULL != outkey) {
		for (ibit = 0; types[ibit].bits; ibit++) {
			if (0 == strcasecmp(types[ibit].name, outkey)) {
				outbit = types[ibit].bits;
				break;
			}
		}
	}

	/*
	 * Save a descriptor to the current working directory.
	 * Since pathnames in the "paths" variable might be relative,
	 * and we'll be chdir()ing into them, we need to keep a handle
	 * on our current directory from which to start the chdir().
	 */

	if (NULL == getcwd(buf, PATH_MAX)) {
		perror(NULL);
		goto out;
	} else if (-1 == (fd = open(buf, O_RDONLY, 0))) {
		perror(buf);
		goto out;
	}

	sql = sql_statement(e);

	/*
	 * Loop over the directories (containing databases) for us to
	 * search.
	 * Don't let missing/bad databases/directories phase us.
	 * In each, try to open the resident database and, if it opens,
	 * scan it for our match expression.
	 */

	for (i = 0; i < paths->sz; i++) {
		if (-1 == fchdir(fd)) {
			perror(buf);
			free(*res);
			break;
		} else if (-1 == chdir(paths->paths[i])) {
			perror(paths->paths[i]);
			continue;
		} 

		c =  sqlite3_open_v2
			(MANDOC_DB, &db, 
			 SQLITE_OPEN_READONLY, NULL);

		if (SQLITE_OK != c) {
			perror(MANDOC_DB);
			sqlite3_close(db);
			continue;
		}

		/*
		 * Define the SQL functions for substring
		 * and regular expression matching.
		 */

		c = sqlite3_create_function(db, "match", 2,
		    SQLITE_ANY, NULL, sql_match, NULL, NULL);
		assert(SQLITE_OK == c);
		c = sqlite3_create_function(db, "regexp", 2,
		    SQLITE_ANY, NULL, sql_regexp, NULL, NULL);
		assert(SQLITE_OK == c);

		j = 1;
		c = sqlite3_prepare_v2(db, sql, -1, &s, NULL);
		if (SQLITE_OK != c)
			fprintf(stderr, "%s\n", sqlite3_errmsg(db));

		for (ep = e; NULL != ep; ep = ep->next) {
			if (NULL == ep->substr) {
				SQL_BIND_BLOB(db, s, j, ep->regexp);
			} else
				SQL_BIND_TEXT(db, s, j, ep->substr);
			SQL_BIND_INT64(db, s, j, ep->bits);
		}

		memset(&htab, 0, sizeof(struct ohash));
		ohash_init(&htab, 4, &info);

		/*
		 * Hash each entry on its [unique] document identifier.
		 * This is a uint64_t.
		 * Instead of using a hash function, simply convert the
		 * uint64_t to a uint32_t, the hash value's type.
		 * This gives good performance and preserves the
		 * distribution of buckets in the table.
		 */
		while (SQLITE_ROW == (c = sqlite3_step(s))) {
			id = sqlite3_column_int64(s, 2);
			idx = ohash_lookup_memory
				(&htab, (char *)&id, 
				 sizeof(uint64_t), (uint32_t)id);

			if (NULL != ohash_find(&htab, idx))
				continue;

			mp = mandoc_calloc(1, sizeof(struct match));
			mp->id = id;
			mp->desc = mandoc_strdup
				((char *)sqlite3_column_text(s, 0));
			mp->form = sqlite3_column_int(s, 1);
			ohash_insert(&htab, idx, mp);
		}

		if (SQLITE_DONE != c)
			fprintf(stderr, "%s\n", sqlite3_errmsg(db));

		sqlite3_finalize(s);

		c = sqlite3_prepare_v2(db, 
		    "SELECT * FROM mlinks WHERE pageid=?",
		    -1, &s, NULL);
		if (SQLITE_OK != c)
			fprintf(stderr, "%s\n", sqlite3_errmsg(db));

		c = sqlite3_prepare_v2(db,
		    "SELECT * FROM keys WHERE pageid=? AND bits & ?",
		    -1, &s2, NULL);
		if (SQLITE_OK != c)
			fprintf(stderr, "%s\n", sqlite3_errmsg(db));

		for (mp = ohash_first(&htab, &idx);
				NULL != mp;
				mp = ohash_next(&htab, &idx)) {
			if (cur + 1 > maxres) {
				maxres += 1024;
				*res = mandoc_realloc
					(*res, maxres * sizeof(struct manpage));
			}
			mpage = *res + cur;
			mpage->desc = mp->desc;
			mpage->form = mp->form;
			buildnames(mpage, db, s, mp->id, paths->paths[i]);
			mpage->output = outbit ?
			    buildoutput(db, s2, mp->id, outbit) : NULL;

			free(mp);
			cur++;
		}

		sqlite3_finalize(s);
		sqlite3_finalize(s2);
		sqlite3_close(db);
		ohash_delete(&htab);
	}
	rc = 1;
out:
	exprfree(e);
	if (-1 != fd)
		close(fd);
	free(sql);
	*sz = cur;
	return(rc);
}

static void
buildnames(struct manpage *mpage, sqlite3 *db, sqlite3_stmt *s,
		uint64_t id, const char *path)
{
	char		*newnames;
	const char	*oldnames, *sep1, *name, *sec, *sep2, *arch;
	size_t		 i;
	int		 c;

	mpage->names = NULL;
	i = 1;
	SQL_BIND_INT64(db, s, i, id);
	while (SQLITE_ROW == (c = sqlite3_step(s))) {

		/* Assemble the list of names. */

		if (NULL == mpage->names) {
			oldnames = "";
			sep1 = "";
		} else {
			oldnames = mpage->names;
			sep1 = ", ";
		}
		sec = sqlite3_column_text(s, 1);
		arch = sqlite3_column_text(s, 2);
		name = sqlite3_column_text(s, 3);
		sep2 = '\0' == *arch ? "" : "/";
		if (-1 == asprintf(&newnames, "%s%s%s(%s%s%s)",
		    oldnames, sep1, name, sec, sep2, arch)) {
			perror(0);
			exit((int)MANDOCLEVEL_SYSERR);
		}
		free(mpage->names);
		mpage->names = newnames;

		/* Also save the first file name encountered. */

		if (NULL != mpage->file)
			continue;

		name = sqlite3_column_text(s, 0);
		if (-1 == asprintf(&mpage->file, "%s/%s", path, name)) {
			perror(0);
			exit((int)MANDOCLEVEL_SYSERR);
		}
	}
	if (SQLITE_DONE != c)
		fprintf(stderr, "%s\n", sqlite3_errmsg(db));
	sqlite3_reset(s);
}

static char *
buildoutput(sqlite3 *db, sqlite3_stmt *s, uint64_t id, uint64_t outbit)
{
	char		*output, *newoutput;
	const char	*oldoutput, *sep1, *data;
	size_t		 i;
	int		 c;

	output = NULL;
	i = 1;
	SQL_BIND_INT64(db, s, i, id);
	SQL_BIND_INT64(db, s, i, outbit);
	while (SQLITE_ROW == (c = sqlite3_step(s))) {
		if (NULL == output) {
			oldoutput = "";
			sep1 = "";
		} else {
			oldoutput = output;
			sep1 = " # ";
		}
		data = sqlite3_column_text(s, 1);
		if (-1 == asprintf(&newoutput, "%s%s%s",
		    oldoutput, sep1, data)) {
			perror(0);
			exit((int)MANDOCLEVEL_SYSERR);
		}
		free(output);
		output = newoutput;
	}
	if (SQLITE_DONE != c)
		fprintf(stderr, "%s\n", sqlite3_errmsg(db));
	sqlite3_reset(s);
	return(output);
}

/*
 * Implement substring match as an application-defined SQL function.
 * Using the SQL LIKE or GLOB operators instead would be a bad idea
 * because that would require escaping metacharacters in the string
 * being searched for.
 */
static void
sql_match(sqlite3_context *context, int argc, sqlite3_value **argv)
{

	assert(2 == argc);
	sqlite3_result_int(context, NULL != strcasestr(
	    (const char *)sqlite3_value_text(argv[1]),
	    (const char *)sqlite3_value_text(argv[0])));
}

/*
 * Implement regular expression match
 * as an application-defined SQL function.
 */
static void
sql_regexp(sqlite3_context *context, int argc, sqlite3_value **argv)
{

	assert(2 == argc);
	sqlite3_result_int(context, !regexec(
	    (regex_t *)sqlite3_value_blob(argv[0]),
	    (const char *)sqlite3_value_text(argv[1]),
	    0, NULL, 0));
}

static void
sql_append(char **sql, size_t *sz, const char *newstr, int count)
{
	size_t		 newsz;

	newsz = 1 < count ? (size_t)count : strlen(newstr);
	*sql = mandoc_realloc(*sql, *sz + newsz + 1);
	if (1 < count)
		memset(*sql + *sz, *newstr, (size_t)count);
	else
		memcpy(*sql + *sz, newstr, newsz);
	*sz += newsz;
	(*sql)[*sz] = '\0';
}

/*
 * Prepare the search SQL statement.
 */
static char *
sql_statement(const struct expr *e)
{
	char		*sql;
	size_t		 sz;
	int		 needop;

	sql = mandoc_strdup("SELECT * FROM mpages WHERE ");
	sz = strlen(sql);

	for (needop = 0; NULL != e; e = e->next) {
		if (e->and)
			sql_append(&sql, &sz, " AND ", 1);
		else if (needop)
			sql_append(&sql, &sz, " OR ", 1);
		if (e->open)
			sql_append(&sql, &sz, "(", e->open);
		sql_append(&sql, &sz, NULL == e->substr ?
		    "id IN (SELECT pageid FROM keys "
		    "WHERE key REGEXP ? AND bits & ?)" :
		    "id IN (SELECT pageid FROM keys "
		    "WHERE key MATCH ? AND bits & ?)", 1);
		if (e->close)
			sql_append(&sql, &sz, ")", e->close);
		needop = 1;
	}

	return(sql);
}

/*
 * Compile a set of string tokens into an expression.
 * Tokens in "argv" are assumed to be individual expression atoms (e.g.,
 * "(", "foo=bar", etc.).
 */
static struct expr *
exprcomp(const struct mansearch *search, int argc, char *argv[])
{
	int		 i, toopen, logic, igncase, toclose;
	struct expr	*first, *next, *cur;

	first = cur = NULL;
	logic = igncase = toclose = 0;
	toopen = 1;

	for (i = 0; i < argc; i++) {
		if (0 == strcmp("(", argv[i])) {
			if (igncase)
				goto fail;
			toopen++;
			toclose++;
			continue;
		} else if (0 == strcmp(")", argv[i])) {
			if (toopen || logic || igncase || NULL == cur)
				goto fail;
			cur->close++;
			if (0 > --toclose)
				goto fail;
			continue;
		} else if (0 == strcmp("-a", argv[i])) {
			if (toopen || logic || igncase || NULL == cur)
				goto fail;
			logic = 1;
			continue;
		} else if (0 == strcmp("-o", argv[i])) {
			if (toopen || logic || igncase || NULL == cur)
				goto fail;
			logic = 2;
			continue;
		} else if (0 == strcmp("-i", argv[i])) {
			if (igncase)
				goto fail;
			igncase = 1;
			continue;
		}
		next = exprterm(search, argv[i], !igncase);
		if (NULL == next)
			goto fail;
		next->open = toopen;
		next->and = (1 == logic);
		if (NULL != first) {
			cur->next = next;
			cur = next;
		} else
			cur = first = next;
		toopen = logic = igncase = 0;
	}
	if (toopen || logic || igncase || toclose)
		goto fail;

	cur->close++;
	cur = exprspec(cur, TYPE_arch, search->arch, "^(%s|any)$");
	exprspec(cur, TYPE_sec, search->sec, "^%s$");

	return(first);

fail:
	if (NULL != first)
		exprfree(first);
	return(NULL);
}

static struct expr *
exprspec(struct expr *cur, uint64_t key, const char *value,
		const char *format)
{
	char	 errbuf[BUFSIZ];
	char	*cp;
	int	 irc;

	if (NULL == value)
		return(cur);

	if (-1 == asprintf(&cp, format, value)) {
		perror(0);
		exit((int)MANDOCLEVEL_SYSERR);
	}
	cur->next = mandoc_calloc(1, sizeof(struct expr));
	cur = cur->next;
	cur->and = 1;
	cur->bits = key;
	if (0 != (irc = regcomp(&cur->regexp, cp,
	    REG_EXTENDED | REG_NOSUB | REG_ICASE))) {
		regerror(irc, &cur->regexp, errbuf, sizeof(errbuf));
		fprintf(stderr, "regcomp: %s\n", errbuf);
		cur->substr = value;
	}
	free(cp);
	return(cur);
}

static struct expr *
exprterm(const struct mansearch *search, char *buf, int cs)
{
	char		 errbuf[BUFSIZ];
	struct expr	*e;
	char		*key, *v;
	size_t		 i;
	int		 irc;

	if ('\0' == *buf)
		return(NULL);

	e = mandoc_calloc(1, sizeof(struct expr));

	/*"whatis" mode uses an opaque string and default fields. */

	if (MANSEARCH_WHATIS & search->flags) {
		e->substr = buf;
		e->bits = search->deftype;
		return(e);
	}

	/*
	 * If no =~ is specified, search with equality over names and
	 * descriptions.
	 * If =~ begins the phrase, use name and description fields.
	 */

	if (NULL == (v = strpbrk(buf, "=~"))) {
		e->substr = buf;
		e->bits = search->deftype;
		return(e);
	} else if (v == buf)
		e->bits = search->deftype;

	if ('~' == *v++) {
		if (0 != (irc = regcomp(&e->regexp, v,
		    REG_EXTENDED | REG_NOSUB | (cs ? 0 : REG_ICASE)))) {
			regerror(irc, &e->regexp, errbuf, sizeof(errbuf));
			fprintf(stderr, "regcomp: %s\n", errbuf);
			free(e);
			return(NULL);
		}
	} else
		e->substr = v;
	v[-1] = '\0';

	/*
	 * Parse out all possible fields.
	 * If the field doesn't resolve, bail.
	 */

	while (NULL != (key = strsep(&buf, ","))) {
		if ('\0' == *key)
			continue;
		i = 0;
		while (types[i].bits && 
			strcasecmp(types[i].name, key))
			i++;
		if (0 == types[i].bits) {
			free(e);
			return(NULL);
		}
		e->bits |= types[i].bits;
	}

	return(e);
}

static void
exprfree(struct expr *p)
{
	struct expr	*pp;

	while (NULL != p) {
		pp = p->next;
		free(p);
		p = pp;
	}
}

static void *
hash_halloc(size_t sz, void *arg)
{

	return(mandoc_calloc(sz, 1));
}

static void *
hash_alloc(size_t sz, void *arg)
{

	return(mandoc_malloc(sz));
}

static void
hash_free(void *p, size_t sz, void *arg)
{

	free(p);
}

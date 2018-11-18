/*	$Id: main.c,v 1.2 2016/12/18 16:56:32 christos Exp $ */
/*
 * Copyright (c) 2016 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifdef __linux__
#define _GNU_SOURCE
#endif
#include <sys/queue.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <time.h>
#include <getopt.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <bsd/stdio.h>
#include <bsd/stdlib.h>
#include <bsd/string.h>
#endif

/*
 * Phase of parsing input file.
 */
enum	phase {
	PHASE_INIT = 0, /* waiting to encounter definition */
	PHASE_KEYS, /* have definition, now keywords */
	PHASE_DESC, /* have keywords, now description */
	PHASE_SEEALSO,
	PHASE_DECL /* have description, now declarations */
};

/*
 * What kind of declaration (preliminary analysis). 
 */
enum	decltype {
	DECLTYPE_CPP, /* pre-processor */
	DECLTYPE_C, /* semicolon-closed non-preprocessor */
	DECLTYPE_NEITHER /* non-preprocessor, no semicolon */
};

/*
 * In variables and function declarations, we toss these.
 */
enum	preproc {
	PREPROC_SQLITE_API,
	PREPROC_SQLITE_DEPRECATED,
	PREPROC_SQLITE_EXPERIMENTAL,
	PREPROC_SQLITE_EXTERN,
	PREPROC__MAX
};

/*
 * HTML tags that we recognise.
 */
enum	tag {
	TAG_B_CLOSE,
	TAG_B_OPEN,
	TAG_BLOCK_CLOSE,
	TAG_BLOCK_OPEN,
	TAG_DD_CLOSE,
	TAG_DD_OPEN,
	TAG_DL_CLOSE,
	TAG_DL_OPEN,
	TAG_DT_CLOSE,
	TAG_DT_OPEN,
	TAG_H3_CLOSE,
	TAG_H3_OPEN,
	TAG_LI_CLOSE,
	TAG_LI_OPEN,
	TAG_OL_CLOSE,
	TAG_OL_OPEN,
	TAG_PRE_CLOSE,
	TAG_PRE_OPEN,
	TAG_UL_CLOSE,
	TAG_UL_OPEN,
	TAG__MAX
};

TAILQ_HEAD(defnq, defn);
TAILQ_HEAD(declq, decl);

/*
 * A declaration of type DECLTYPE_CPP or DECLTYPE_C.
 * These need not be unique (if ifdef'd).
 */
struct	decl {
	enum decltype	 type; /* type of declaration */
	char		*text; /* text */
	size_t		 textsz; /* strlen(text) */
	TAILQ_ENTRY(decl) entries;
};

/*
 * A definition is basically the manpage contents.
 */
struct	defn {
	char		 *name; /* really Nd */
	TAILQ_ENTRY(defn) entries;
	char		 *desc; /* long description */
	size_t		  descsz; /* strlen(desc) */
	struct declq	  dcqhead; /* declarations */
	int		  multiline; /* used when parsing */
	int		  instruct; /* used when parsing */
	const char	 *fn; /* parsed from file */
	size_t		  ln; /* parsed at line */
	int		  postprocessed; /* good for emission? */
	char		 *dt; /* manpage title */
	char		**nms; /* manpage names */
	size_t		  nmsz; /* number of names */
	char		 *fname; /* manpage filename */
	char		 *keybuf; /* raw keywords */
	size_t		  keybufsz; /* length of "keysbuf" */
	char		 *seealso; /* see also tags */
	size_t		  seealsosz; /* length of seealso */
	char		**xrs; /* parsed "see also" references */
	size_t		  xrsz; /* number of references */
	char		**keys; /* parsed keywords */
	size_t		  keysz; /* number of keywords */
};

/*
 * Entire parse routine.
 */
struct	parse {
	enum phase	 phase; /* phase of parse */
	size_t		 ln; /* line number */
	const char	*fn; /* open file */
	struct defnq	 dqhead; /* definitions */
};

/*
 * How to handle HTML tags we find in the text.
 */
struct	taginfo {
	const char	*html; /* HTML to key on */
	const char	*mdoc; /* generate mdoc(7) */
	unsigned int	 flags;
#define	TAGINFO_NOBR	 0x01 /* follow w/space, not newline */
#define	TAGINFO_NOOP	 0x02 /* just strip out */
#define	TAGINFO_NOSP	 0x04 /* follow w/o space or newline */
#define	TAGINFO_INLINE	 0x08 /* inline block (notused) */
};

static	const struct taginfo tags[TAG__MAX] = {
	{ "</b>", "\\fP", TAGINFO_INLINE }, /* TAG_B_CLOSE */
	{ "<b>", "\\fB", TAGINFO_INLINE }, /* TAG_B_OPEN */
	{ "</blockquote>", ".Ed\n.Pp", 0 }, /* TAG_BLOCK_CLOSE */
	{ "<blockquote>", ".Bd -ragged", 0 }, /* TAG_BLOCK_OPEN */
	{ "</dd>", "", TAGINFO_NOOP }, /* TAG_DD_CLOSE */
	{ "<dd>", "", TAGINFO_NOOP }, /* TAG_DD_OPEN */
	{ "</dl>", ".El\n.Pp", 0 }, /* TAG_DL_CLOSE */
	{ "<dl>", ".Bl -tag -width Ds", 0 }, /* TAG_DL_OPEN */
	{ "</dt>", "", TAGINFO_NOBR | TAGINFO_NOSP}, /* TAG_DT_CLOSE */
	{ "<dt>", ".It", TAGINFO_NOBR }, /* TAG_DT_OPEN */
	{ "</h3>", "", TAGINFO_NOBR | TAGINFO_NOSP}, /* TAG_H3_CLOSE */
	{ "<h3>", ".Ss", TAGINFO_NOBR }, /* TAG_H3_OPEN */
	{ "</li>", "", TAGINFO_NOOP }, /* TAG_LI_CLOSE */
	{ "<li>", ".It", 0 }, /* TAG_LI_OPEN */
	{ "</ol>", ".El\n.Pp", 0 }, /* TAG_OL_CLOSE */
	{ "<ol>", ".Bl -enum", 0 }, /* TAG_OL_OPEN */
	{ "</pre>", ".Ed\n.Pp", 0 }, /* TAG_PRE_CLOSE */
	{ "<pre>", ".Bd -literal", 0 }, /* TAG_PRE_OPEN */
	{ "</ul>", ".El\n.Pp", 0 }, /* TAG_UL_CLOSE */
	{ "<ul>", ".Bl -bullet", 0 }, /* TAG_UL_OPEN */
};

static	const char *const preprocs[TAG__MAX] = {
	"SQLITE_API", /* PREPROC_SQLITE_API */
	"SQLITE_DEPRECATED", /* PREPROC_SQLITE_DEPRECATED */
	"SQLITE_EXPERIMENTAL", /* PREPROC_SQLITE_EXPERIMENTAL */
	"SQLITE_EXTERN", /* PREPROC_SQLITE_EXTERN */
};

/* Verbose reporting. */
static	int verbose;
/* Don't output any files: use stdout. */
static	int nofile;

static void
decl_function_add(struct parse *p, char **etext, 
	size_t *etextsz, const char *cp, size_t len)
{

	if (' ' != (*etext)[*etextsz - 1]) {
		*etext = realloc(*etext, *etextsz + 2);
		if (NULL == *etext)
			err(EXIT_FAILURE, "%s:%zu: "
				"realloc", p->fn, p->ln);
		(*etextsz)++;
		strlcat(*etext, " ", *etextsz + 1);
	}
	*etext = realloc(*etext, *etextsz + len + 1);
	if (NULL == *etext)
		err(EXIT_FAILURE, "%s:%zu: realloc", p->fn, p->ln);
	memcpy(*etext + *etextsz, cp, len);
	*etextsz += len;
	(*etext)[*etextsz] = '\0';
}

static void
decl_function_copy(struct parse *p, char **etext,
	size_t *etextsz, const char *cp, size_t len)
{

	*etext = malloc(len + 1);
	if (NULL == *etext)
		err(EXIT_FAILURE, "%s:%zu: strdup", p->fn, p->ln);
	memcpy(*etext, cp, len);
	*etextsz = len;
	(*etext)[*etextsz] = '\0';
}

/*
 * A C function (or variable, or whatever).
 * This is more specifically any non-preprocessor text.
 */
static int
decl_function(struct parse *p, char *cp, size_t len)
{
	char		*ep, *ncp, *lcp, *rcp;
	size_t		 nlen;
	struct defn	*d;
	struct decl	*e;

	/* Fetch current interface definition. */
	d = TAILQ_LAST(&p->dqhead, defnq);
	assert(NULL != d);

	/*
	 * Since C tokens are semicolon-separated, we may be invoked any
	 * number of times per a single line. 
	 */
again:
	while (isspace((int)*cp)) {
		cp++;
		len--;
	}
	if ('\0' == *cp)
		return(1);

	/* Whether we're a continuation clause. */
	if (d->multiline) {
		/* This might be NULL if we're not a continuation. */
		e = TAILQ_LAST(&d->dcqhead, declq);
		assert(DECLTYPE_C == e->type);
		assert(NULL != e);
		assert(NULL != e->text);
		assert(e->textsz);
	} else {
		assert(0 == d->instruct);
		e = calloc(1, sizeof(struct decl));
		e->type = DECLTYPE_C;
		if (NULL == e)
			err(EXIT_FAILURE, "%s:%zu: calloc", p->fn, p->ln);
		TAILQ_INSERT_TAIL(&d->dcqhead, e, entries);
	}

	/*
	 * We begin by seeing if there's a semicolon on this line.
	 * If there is, we'll need to do some special handling.
	 */
	ep = strchr(cp, ';');
	lcp = strchr(cp, '{');
	rcp = strchr(cp, '}');

	/* We're only a partial statement (i.e., no closure). */
	if (NULL == ep && d->multiline) {
		assert(NULL != e->text);
		assert(e->textsz > 0);
		/* Is a struct starting or ending here? */
		if (d->instruct && NULL != rcp)
			d->instruct--;
		else if (NULL != lcp)
			d->instruct++;
		decl_function_add(p, &e->text, &e->textsz, cp, len);
		return(1);
	} else if (NULL == ep && ! d->multiline) {
		d->multiline = 1;
		/* Is a structure starting in this line? */
		if (NULL != lcp && 
		    (NULL == rcp || rcp < lcp))
			d->instruct++;
		decl_function_copy(p, &e->text, &e->textsz, cp, len);
		return(1);
	}

	/* Position ourselves after the semicolon. */
	assert(NULL != ep);
	ncp = cp;
	nlen = (ep - cp) + 1;
	cp = ep + 1;
	len -= nlen;

	if (d->multiline) {
		assert(NULL != e->text);
		/* Don't stop the multi-line if we're in a struct. */
		if (0 == d->instruct) {
			if (NULL == lcp || lcp > cp)
				d->multiline = 0;
		} else if (NULL != rcp && rcp < cp)
			if (0 == --d->instruct)
				d->multiline = 0;
		decl_function_add(p, &e->text, &e->textsz, ncp, nlen);
	} else {
		assert(NULL == e->text);
		if (NULL != lcp && lcp < cp) {
			d->multiline = 1;
			d->instruct++;
		}
		decl_function_copy(p, &e->text, &e->textsz, ncp, nlen);
	}

	goto again;
}

/*
 * A definition is just #define followed by space followed by the name,
 * then the value of that name.
 * We ignore the latter.
 * FIXME: this does not understand multi-line CPP, but I don't think
 * there are any instances of that in sqlite.h.
 */
static int
decl_define(struct parse *p, char *cp, size_t len)
{
	struct defn	*d;
	struct decl	*e;
	size_t		 sz;

	while (isspace((int)*cp)) {
		cp++;
		len--;
	}
	if (0 == len) {
		warnx("%s:%zu: empty pre-processor "
			"constant", p->fn, p->ln);
		return(1);
	}

	d = TAILQ_LAST(&p->dqhead, defnq);
	assert(NULL != d);

	/*
	 * We're parsing a preprocessor definition, but we're still
	 * waiting on a semicolon from a function definition.
	 * It might be a comment or an error.
	 */
	if (d->multiline) {
		warnx("%s:%zu: multiline declaration "
			"still open (harmless?)", p->fn, p->ln);
		e = TAILQ_LAST(&d->dcqhead, declq);
		assert(NULL != e);
		e->type = DECLTYPE_NEITHER;
		d->multiline = d->instruct = 0;
	}

	sz = 0;
	while ('\0' != cp[sz] && ! isspace((int)cp[sz]))
		sz++;

	e = calloc(1, sizeof(struct decl));
	if (NULL == e) 
		err(EXIT_FAILURE, "%s:%zu: calloc", p->fn, p->ln);
	e->type = DECLTYPE_CPP;
	e->text = calloc(1, sz + 1);
	if (NULL == e->text)
		err(EXIT_FAILURE, "%s:%zu: calloc", p->fn, p->ln);
	strlcpy(e->text, cp, sz + 1);
	e->textsz = sz;
	TAILQ_INSERT_TAIL(&d->dcqhead, e, entries);
	return(1);
}

/*
 * A declaration is a function, variable, preprocessor definition, or
 * really anything else until we reach a blank line.
 */
static void
decl(struct parse *p, char *cp, size_t len)
{
	struct defn	*d;
	struct decl	*e;

	while (isspace((int)*cp)) {
		cp++;
		len--;
	}

	/* Check closure. */
	if ('\0' == *cp) {
		p->phase = PHASE_INIT;
		/* Check multiline status. */
		d = TAILQ_LAST(&p->dqhead, defnq);
		assert(NULL != d);
		if (d->multiline) {
			warnx("%s:%zu: multiline declaration "
				"still open (harmless?)", p->fn, p->ln);
			e = TAILQ_LAST(&d->dcqhead, declq);
			assert(NULL != e);
			e->type = DECLTYPE_NEITHER;
			d->multiline = d->instruct = 0;
		}
		return;
	} 
	
	/* 
	 * Catch preprocessor defines, but discard all other types of
	 * preprocessor statements.
	 */
	if ('#' == *cp) {
		len--;
		cp++;
		while (isspace((int)*cp)) {
			len--;
			cp++;
		}
		if (0 == strncmp(cp, "define", 6))
			decl_define(p, cp + 6, len - 6);
		return;
	}

	decl_function(p, cp, len);
}

/*
 * Parse "SEE ALSO" phrases, which can come at any point in the
 * interface description (unlike what they claim).
 */
static void
seealso(struct parse *p, char *cp, size_t len)
{
	struct defn	*d;

	if ('\0' == *cp) {
		warnx("%s:%zu: warn: unexpected end of "
			"interface description", p->fn, p->ln);
		p->phase = PHASE_INIT;
		return;
	} else if (0 == strcmp(cp, "*/")) {
		p->phase = PHASE_DECL;
		return;
	} else if ('*' != cp[0] || '*' != cp[1]) {
		warnx("%s:%zu: warn: unexpected end of "
			"interface description", p->fn, p->ln);
		p->phase = PHASE_INIT;
		return;
	}

	cp += 2;
	len -= 2;
	while (isspace((int)*cp)) {
		cp++;
		len--;
	}

	/* Blank line: back to description part. */
	if (0 == len) {
		p->phase = PHASE_DESC;
		return;
	}

	/* Fetch current interface definition. */
	d = TAILQ_LAST(&p->dqhead, defnq);
	assert(NULL != d);

	d->seealso = realloc(d->seealso,
		d->seealsosz + len + 1);
	memcpy(d->seealso + d->seealsosz, cp, len);
	d->seealsosz += len;
	d->seealso[d->seealsosz] = '\0';
}

/*
 * A definition description is a block of text that we'll later format
 * in mdoc(7).
 * It extends from the name of the definition down to the declarations
 * themselves.
 */
static void
desc(struct parse *p, char *cp, size_t len)
{
	struct defn	*d;
	size_t		 nsz;

	if ('\0' == *cp) {
		warnx("%s:%zu: warn: unexpected end of "
			"interface description", p->fn, p->ln);
		p->phase = PHASE_INIT;
		return;
	} else if (0 == strcmp(cp, "*/")) {
		/* End of comment area, start of declarations. */
		p->phase = PHASE_DECL;
		return;
	} else if ('*' != cp[0] || '*' != cp[1]) {
		warnx("%s:%zu: warn: unexpected end of "
			"interface description", p->fn, p->ln);
		p->phase = PHASE_INIT;
		return;
	}

	cp += 2;
	len -= 2;

	while (isspace((int)*cp)) {
		cp++;
		len--;
	}

	/* Fetch current interface definition. */
	d = TAILQ_LAST(&p->dqhead, defnq);
	assert(NULL != d);

	/* Ignore leading blank lines. */
	if (0 == len && NULL == d->desc)
		return;

	/* Collect SEE ALSO clauses. */
	if (0 == strncasecmp(cp, "see also:", 9)) {
		cp += 9;
		len -= 9;
		while (isspace((int)*cp)) {
			cp++;
			len--;
		}
		p->phase = PHASE_SEEALSO;
		d->seealso = realloc(d->seealso,
			d->seealsosz + len + 1);
		memcpy(d->seealso + d->seealsosz, cp, len);
		d->seealsosz += len;
		d->seealso[d->seealsosz] = '\0';
		return;
	}

	/* White-space padding between lines. */
	if (NULL != d->desc && 
	    ' ' != d->desc[d->descsz - 1] &&
	    '\n' != d->desc[d->descsz - 1]) {
		d->desc = realloc(d->desc, d->descsz + 2);
		if (NULL == d->desc)
			err(EXIT_FAILURE, "%s:%zu: realloc", 
				p->fn, p->ln);
		d->descsz++;
		strlcat(d->desc, " ", d->descsz + 1);
	}

	/* Either append the line of a newline, if blank. */
	nsz = 0 == len ? 1 : len;
	if (NULL == d->desc) {
		d->desc = calloc(1, nsz + 1);
		if (NULL == d->desc)
			err(EXIT_FAILURE, "%s:%zu: calloc", 
				p->fn, p->ln);
	} else {
		d->desc = realloc(d->desc, d->descsz + nsz + 1);
		if (NULL == d->desc)
			err(EXIT_FAILURE, "%s:%zu: realloc", 
				p->fn, p->ln);
	}
	d->descsz += nsz;
	strlcat(d->desc, 0 == len ? "\n" : cp, d->descsz + 1);
}

/*
 * Copy all KEYWORDS into a buffer.
 */
static void
keys(struct parse *p, char *cp, size_t len)
{
	struct defn	*d;

	if ('\0' == *cp) {
		warnx("%s:%zu: warn: unexpected end of "
			"interface keywords", p->fn, p->ln);
		p->phase = PHASE_INIT;
		return;
	} else if (0 == strcmp(cp, "*/")) {
		/* End of comment area, start of declarations. */
		p->phase = PHASE_DECL;
		return;
	} else if ('*' != cp[0] || '*' != cp[1]) {
		if ('\0' != cp[1]) {
			warnx("%s:%zu: warn: unexpected end of "
				"interface keywords", p->fn, p->ln);
			p->phase = PHASE_INIT;
			return;
		} else 
			warnx("%s:%zu: warn: workaround in effect "
				"for unexpected end of "
				"interface keywords", p->fn, p->ln);
	}

	cp += 2;
	len -= 2;
	while (isspace((int)*cp)) {
		cp++;
		len--;
	}

	if (0 == len) {
		p->phase = PHASE_DESC;
		return;
	} else if (strncmp(cp, "KEYWORDS:", 9)) 
		return;

	cp += 9;
	len -= 9;

	d = TAILQ_LAST(&p->dqhead, defnq);
	assert(NULL != d);
	d->keybuf = realloc(d->keybuf, d->keybufsz + len + 1);
	if (NULL == d->keybuf)
		err(EXIT_FAILURE, "%s:%zu: realloc", p->fn, p->ln);
	memcpy(d->keybuf + d->keybufsz, cp, len);
	d->keybufsz += len;
	d->keybuf[d->keybufsz] = '\0';
}

/*
 * Initial state is where we're scanning forward to find commented
 * instances of CAPI3REF.
 */
static void
init(struct parse *p, char *cp)
{
	struct defn	*d;

	/* Look for comment hook. */
	if ('*' != cp[0] || '*' != cp[1])
		return;
	cp += 2;
	while (isspace((int)*cp))
		cp++;

	/* Look for beginning of definition. */
	if (strncmp(cp, "CAPI3REF:", 9))
		return;
	cp += 9;
	while (isspace((int)*cp))
		cp++;
	if ('\0' == *cp) {
		warnx("%s:%zu: warn: unexpected end of "
			"interface definition", p->fn, p->ln);
		return;
	}

	/* Add definition to list of existing ones. */
	d = calloc(1, sizeof(struct defn));
	if (NULL == d)
		err(EXIT_FAILURE, "%s:%zu: calloc", p->fn, p->ln);
	d->name = strdup(cp);
	if (NULL == d->name)
		err(EXIT_FAILURE, "%s:%zu: strdup", p->fn, p->ln);
	d->fn = p->fn;
	d->ln = p->ln;
	p->phase = PHASE_KEYS;
	TAILQ_INIT(&d->dcqhead);
	TAILQ_INSERT_TAIL(&p->dqhead, d, entries);
}

#define	BPOINT(_cp) \
	(';' == (_cp)[0] || \
	 '[' == (_cp)[0] || \
	 ('(' == (_cp)[0] && '*' != (_cp)[1]) || \
	 ')' == (_cp)[0] || \
	 '{' == (_cp)[0])

/*
 * Given a declaration (be it preprocessor or C), try to parse out a
 * reasonable "name" for the affair.
 * For a struct, for example, it'd be the struct name.
 * For a typedef, it'd be the type name.
 * For a function, it'd be the function name.
 */
static void
grok_name(const struct decl *e, 
	const char **start, size_t *sz)
{
	const char	*cp;

	*start = NULL;
	*sz = 0;

	if (DECLTYPE_CPP != e->type) {
		assert(';' == e->text[e->textsz - 1]);
		cp = e->text;
		do {
			while (isspace((int)*cp))
				cp++;
			if (BPOINT(cp))
				break;
			/* Function pointers... */
			if ('(' == *cp)
				cp++;
			/* Pass over pointers. */
			while ('*' == *cp)
				cp++;
			*start = cp;
			*sz = 0;
			while ( ! isspace((int)*cp)) {
				if (BPOINT(cp))
					break;
				cp++;
				(*sz)++;
			}
		} while ( ! BPOINT(cp));
	} else {
		*sz = e->textsz;
		*start = e->text;
	}
}

static int
xrcmp(const void *p1, const void *p2)
{
	const char	*s1 = *(const char **)p1, 
	     	 	*s2 = *(const char **)p2;

	return(strcasecmp(s1, s2));
}

/*
 * Extract information from the interface definition.
 * Mark it as "postprocessed" on success.
 */
static void
postprocess(const char *prefix, struct defn *d)
{
	struct decl	*first;
	const char	*start;
	size_t		 offs, sz, i;
	ENTRY		 ent;

	if (TAILQ_EMPTY(&d->dcqhead))
		return;

	/* Find the first #define or declaration. */
	TAILQ_FOREACH(first, &d->dcqhead, entries)
		if (DECLTYPE_CPP == first->type ||
		    DECLTYPE_C == first->type)
			break;

	if (NULL == first) {
		warnx("%s:%zu: no entry to document", d->fn, d->ln);
		return;
	}

	/* 
	 * Now compute the document name (`Dt').
	 * We'll also use this for the filename.
	 */
	grok_name(first, &start, &sz);
	if (NULL == start) {
		warnx("%s:%zu: couldn't deduce "
			"entry name", d->fn, d->ln);
		return;
	}

	/* Document name needs all-caps. */
	d->dt = malloc(sz + 1);
	if (NULL == d->dt)
		err(EXIT_FAILURE, "malloc");
	memcpy(d->dt, start, sz);
	d->dt[sz] = '\0';
	for (i = 0; i < sz; i++)
		d->dt[i] = toupper((int)d->dt[i]);

	/* Filename needs no special chars. */
	asprintf(&d->fname, "%s/%.*s.3", 
		prefix, (int)sz, start);
	if (NULL == d->fname)
		err(EXIT_FAILURE, "asprintf");

	offs = strlen(prefix) + 1;
	for (i = 0; i < sz; i++) {
		if (isalnum((int)d->fname[offs + i]) ||
		    '_' == d->fname[offs + i] ||
		    '-' == d->fname[offs + i])
			continue;
		d->fname[offs + i] = '_';
	}

	/* 
	 * First, extract all keywords.
	 */
	for (i = 0; i < d->keybufsz; ) {
		while (isspace((int)d->keybuf[i]))
			i++;
		if (i == d->keybufsz)
			break;
		sz = 0;
		start = &d->keybuf[i];
		if ('{' == d->keybuf[i]) {
			start = &d->keybuf[++i];
			for ( ; i < d->keybufsz; i++, sz++) 
				if ('}' == d->keybuf[i])
					break;
			if ('}' == d->keybuf[i])
				i++;
		} else
			for ( ; i < d->keybufsz; i++, sz++)
				if (isspace((int)d->keybuf[i]))
					break;
		if (0 == sz)
			continue;
		d->keys = realloc(d->keys,
			(d->keysz + 1) * sizeof(char *));
		if (NULL == d->keys) 
			err(EXIT_FAILURE, "realloc");
		d->keys[d->keysz] = malloc(sz + 1);
		if (NULL == d->keys[d->keysz]) 
			err(EXIT_FAILURE, "malloc");
		memcpy(d->keys[d->keysz], start, sz);
		d->keys[d->keysz][sz] = '\0';
		d->keysz++;
		
		/* Hash the keyword. */
		ent.key = d->keys[d->keysz - 1];
		ent.data = d;
		(void)hsearch(ent, ENTER);
	}

	/*
	 * Now extract all `Nm' values for this document.
	 * We only use CPP and C references, and hope for the best when
	 * doing so.
	 * Enter each one of these as a searchable keyword.
	 */
	TAILQ_FOREACH(first, &d->dcqhead, entries) {
		if (DECLTYPE_CPP != first->type &&
		    DECLTYPE_C != first->type)
			continue;
		grok_name(first, &start, &sz);
		if (NULL == start) 
			continue;
		d->nms = realloc(d->nms, 
			(d->nmsz + 1) * sizeof(char *));
		if (NULL == d->nms)
			err(EXIT_FAILURE, "realloc");
		d->nms[d->nmsz] = malloc(sz + 1);
		if (NULL == d->nms[d->nmsz])
			err(EXIT_FAILURE, "malloc");
		memcpy(d->nms[d->nmsz], start, sz);
		d->nms[d->nmsz][sz] = '\0';
		d->nmsz++;

		/* Hash the name. */
		ent.key = d->nms[d->nmsz - 1];
		ent.data = d;
		(void)hsearch(ent, ENTER);
	}

	if (0 == d->nmsz) {
		warnx("%s:%zu: couldn't deduce "
			"any names", d->fn, d->ln);
		return;
	}

	/*
	 * Next, scan for all `Xr' values.
	 * We'll add more to this list later.
	 */
	for (i = 0; i < d->seealsosz; i++) {
		/* 
		 * Find next value starting with `['.
		 * There's other stuff in there (whitespace or
		 * free text leading up to these) that we're ok
		 * to ignore.
		 */
		while (i < d->seealsosz && '[' != d->seealso[i])
			i++;
		if (i == d->seealsosz)
			break;

		/* 
		 * Now scan for the matching `]'.
		 * We can also have a vertical bar if we're separating a
		 * keyword and its shown name.
		 */
		start = &d->seealso[++i];
		sz = 0;
		while (i < d->seealsosz &&
		      ']' != d->seealso[i] &&
		      '|' != d->seealso[i]) {
			i++;
			sz++;
		}
		if (i == d->seealsosz)
			break;
		if (0 == sz)
			continue;

		/* 
		 * Continue on to the end-of-reference, if we weren't
		 * there to begin with.
		 */
		if (']' != d->seealso[i]) 
			while (i < d->seealsosz &&
			      ']' != d->seealso[i])
				i++;

		/* Strip trailing whitespace. */
		while (sz > 1 && ' ' == start[sz - 1])
			sz--;

		/* Strip trailing parenthesis. */
		if (sz > 2 && 
		    '(' == start[sz - 2] && 
	 	    ')' == start[sz - 1])
			sz -= 2;

		d->xrs = realloc(d->xrs,
			(d->xrsz + 1) * sizeof(char *));
		if (NULL == d->xrs)
			err(EXIT_FAILURE, "realloc");
		d->xrs[d->xrsz] = malloc(sz + 1);
		if (NULL == d->xrs[d->xrsz])
			err(EXIT_FAILURE, "malloc");
		memcpy(d->xrs[d->xrsz], start, sz);
		d->xrs[d->xrsz][sz] = '\0';
		d->xrsz++;
	}

	/*
	 * Next, extract all references.
	 * We'll accumulate these into a list of SEE ALSO tags, after.
	 * See how these are parsed above for a description: this is
	 * basically the same thing.
	 */
	for (i = 0; i < d->descsz; i++) {
		if ('[' != d->desc[i])
			continue;
		i++;
		if ('[' == d->desc[i])
			continue;

		start = &d->desc[i];
		for (sz = 0; i < d->descsz; i++, sz++)
			if (']' == d->desc[i] ||
			    '|' == d->desc[i])
				break;

		if (i == d->descsz)
			break;
		else if (sz == 0)
			continue;

		if (']' != d->desc[i]) 
			while (i < d->descsz &&
			      ']' != d->desc[i])
				i++;

		while (sz > 1 && ' ' == start[sz - 1])
			sz--;

		if (sz > 2 && 
		    '(' == start[sz - 2] &&
		    ')' == start[sz - 1])
			sz -= 2;

		d->xrs = realloc(d->xrs,
			(d->xrsz + 1) * sizeof(char *));
		if (NULL == d->xrs)
			err(EXIT_FAILURE, "realloc");
		d->xrs[d->xrsz] = malloc(sz + 1);
		if (NULL == d->xrs[d->xrsz])
			err(EXIT_FAILURE, "malloc");
		memcpy(d->xrs[d->xrsz], start, sz);
		d->xrs[d->xrsz][sz] = '\0';
		d->xrsz++;
	}

	qsort(d->xrs, d->xrsz, sizeof(char *), xrcmp);
	d->postprocessed = 1;
}

/*
 * Convenience function to look up a keyword.
 * Returns the keyword's file if found or NULL.
 */
static const char *
lookup(char *key)
{
	ENTRY		 ent;
	ENTRY		*res;
	struct defn	*d;

	ent.key = key;
	res = hsearch(ent, FIND);
	if (NULL == res) 
		return(NULL);
	d = (struct defn *)res->data;
	if (0 == d->nmsz)
		return(NULL);
	assert(NULL != d->nms[0]);
	return(d->nms[0]);
}

/*
 * Emit a valid mdoc(7) document within the given prefix.
 */
static void
emit(const struct defn *d, const char *mdocdate)
{
	struct decl	*first;
	size_t		 sz, i, col, last, ns;
	FILE		*f;
	char		*cp;
	const char	*res, *lastres, *args, *str, *end;
	enum tag	 tag;
	enum preproc	 pre;

	if ( ! d->postprocessed) {
		warnx("%s:%zu: interface has errors, not "
			"producing manpage", d->fn, d->ln);
		return;
	}

	if (0 == nofile) {
		if (NULL == (f = fopen(d->fname, "w"))) {
			warn("%s: fopen", d->fname);
			return;
		}
	} else
		f = stdout;

	/* Begin by outputting the mdoc(7) header. */
#if 0
	fputs(".Dd $" "Mdocdate$\n", f);
#else
	fprintf(f, ".Dd %s\n", mdocdate);
#endif
	fprintf(f, ".Dt %s 3\n", d->dt);
	fputs(".Os\n", f);
	fputs(".Sh NAME\n", f);

	/* Now print the name bits of each declaration. */
	for (i = 0; i < d->nmsz; i++)
		fprintf(f, ".Nm %s%s\n", d->nms[i], 
			i < d->nmsz - 1 ? " ," : "");

	fprintf(f, ".Nd %s\n", d->name);
	fputs(".Sh SYNOPSIS\n", f);

	TAILQ_FOREACH(first, &d->dcqhead, entries) {
		if (DECLTYPE_CPP != first->type &&
		    DECLTYPE_C != first->type)
			continue;

		/* Easy: just print the CPP name. */
		if (DECLTYPE_CPP == first->type) {
			fprintf(f, ".Fd #define %s\n",
				first->text);
			continue;
		}

		/* First, strip out the sqlite CPPs. */
		for (i = 0; i < first->textsz; ) {
			for (pre = 0; pre < PREPROC__MAX; pre++) {
				sz = strlen(preprocs[pre]);
				if (strncmp(preprocs[pre], 
				    &first->text[i], sz))
					continue;
				i += sz;
				while (isspace((int)first->text[i]))
					i++;
				break;
			}
			if (pre == PREPROC__MAX)
				break;
		}

		/* If we're a typedef, immediately print Vt. */
		if (0 == strncmp(&first->text[i], "typedef", 7)) {
			fprintf(f, ".Vt %s\n", &first->text[i]);
			continue;
		}

		/* Are we a struct? */
		if (first->textsz > 2 && 
		    '}' == first->text[first->textsz - 2] &&
		    NULL != (cp = strchr(&first->text[i], '{'))) {
			*cp = '\0';
			fprintf(f, ".Vt %s;\n", &first->text[i]);
			/* Restore brace for later usage. */
			*cp = '{';
			continue;
		}

		/* Catch remaining non-functions. */
		if (first->textsz > 2 &&
		    ')' != first->text[first->textsz - 2]) {
			fprintf(f, ".Vt %s\n", &first->text[i]);
			continue;
		}

		str = &first->text[i];
		if (NULL == (args = strchr(str, '('))) {
			/* What is this? */
			fputs(".Bd -literal\n", f);
			fputs(&first->text[i], f);
			fputs("\n.Ed\n", f);
			continue;
		}

		/* Scroll back to end of function name. */
		end = args - 1;
		while (end > str && isspace((int)*end))
			end--;

		/* Scroll back to what comes before. */
		for ( ; end > str; end--)
			if (isspace((int)*end) || '*' == *end)
				break;

		/* 
		 * If we can't find what came before, then the function
		 * has no type, which is odd... let's just call it void.
		 */
		if (end > str) {
			fprintf(f, ".Ft %.*s\n", 
				(int)(end - str + 1), str);
			fprintf(f, ".Fo %.*s\n", 
				(int)(args - end - 1), end + 1);
		} else {
			fputs(".Ft void\n", f);
			fprintf(f, ".Fo %.*s\n", (int)(args - end), end);
		}

		/*
		 * Convert function arguments into `Fa' clauses.
		 * This also handles nested function pointers, which
		 * would otherwise throw off the delimeters.
		 */
		for (;;) {
			str = ++args;
			while (isspace((int)*str))
				str++;
			fputs(".Fa \"", f);
			ns = 0;
			while ('\0' != *str && 
			       (ns || ',' != *str) && 
			       (ns || ')' != *str)) {
				if ('/' == str[0] && '*' == str[1]) {
					str += 2;
					for ( ; '\0' != str[0]; str++)
						if ('*' == str[0] && '/' == str[1])
							break;
					if ('\0' == *str)
						break;
					str += 2;
					while (isspace((int)*str))
						str++;
					if ('\0' == *str ||
					    (0 == ns && ',' == *str) ||
					    (0 == ns && ')' == *str))
						break;
				}
				if ('(' == *str)
					ns++;
				else if (')' == *str)
					ns--;
				fputc(*str, f);
				str++;
			}
			fputs("\"\n", f);
			if ('\0' == *str || ')' == *str)
				break;
			args = str;
		}

		fputs(".Fc\n", f);
	}

	fputs(".Sh DESCRIPTION\n", f);

	/* 
	 * Strip the crap out of the description.
	 * "Crap" consists of things I don't understand that mess up
	 * parsing of the HTML, for instance,
	 *   <dl>[[foo bar]]<dt>foo bar</dt>...</dl>
	 * These are not well-formed HTML.
	 */
	for (i = 0; i < d->descsz; i++) {
		if ('^' == d->desc[i] && 
		    '(' == d->desc[i + 1]) {
			d->desc[i] = d->desc[i + 1] = ' ';
			i++;
			continue;
		} else if (')' == d->desc[i] && 
			   '^' == d->desc[i + 1]) {
			d->desc[i] = d->desc[i + 1] = ' ';
			i++;
			continue;
		} else if ('^' == d->desc[i]) {
			d->desc[i] = ' ';
			continue;
		} else if ('[' != d->desc[i] || 
			   '[' != d->desc[i + 1]) 
			continue;
		d->desc[i] = d->desc[i + 1] = ' ';
		for (i += 2; i < d->descsz; i++) {
			if (']' == d->desc[i] && 
			    ']' == d->desc[i + 1]) 
				break;
			d->desc[i] = ' ';
		}
		if (i == d->descsz)
			continue;
		d->desc[i] = d->desc[i + 1] = ' ';
		i++;
	}

	/*
	 * Here we go!
	 * Print out the description as best we can.
	 * Do on-the-fly processing of any HTML we encounter into
	 * mdoc(7) and try to break lines up.
	 */
	col = 0;
	for (i = 0; i < d->descsz; ) {
		/* 
		 * Newlines are paragraph breaks.
		 * If we have multiple newlines, then keep to a single
		 * `Pp' to keep it clean.
		 * Only do this if we're not before a block-level HTML,
		 * as this would mean, for instance, a `Pp'-`Bd' pair.
		 */
		if ('\n' == d->desc[i]) {
			while (isspace((int)d->desc[i]))
				i++;
			for (tag = 0; tag < TAG__MAX; tag++) {
				sz = strlen(tags[tag].html);
				if (0 == strncmp(&d->desc[i], tags[tag].html, sz))
					break;
			}
			if (TAG__MAX == tag ||
			    TAGINFO_INLINE & tags[tag].flags) {
				if (col > 0)
					fputs("\n", f);
				fputs(".Pp\n", f);
				/* We're on a new line. */
				col = 0;
			}
			continue;
		}

		/*
		 * New sentence, new line.
		 * We guess whether this is the case by using the
		 * dumbest possible heuristic.
		 */
		if (' ' == d->desc[i] && i &&
		    '.' == d->desc[i - 1]) {
			while (' ' == d->desc[i])
				i++;
			fputs("\n", f);
			col = 0;
			continue;
		}
		/*
		 * After 65 characters, force a break when we encounter
		 * white-space to keep our lines more or less tidy.
		 */
		if (col > 65 && ' ' == d->desc[i]) {
			while (' ' == d->desc[i]) 
				i++;
			fputs("\n", f);
			col = 0;
			continue;
		}

		/*
		 * Parsing HTML tags.
		 * Why, sqlite guys, couldn't you have used something
		 * like markdown or something?  
		 * Sheesh.
		 */
		if ('<' == d->desc[i]) {
			for (tag = 0; tag < TAG__MAX; tag++) {
				sz = strlen(tags[tag].html);
				if (strncmp(&d->desc[i], 
				    tags[tag].html, sz))
					continue;
				/*
				 * NOOP tags don't do anything, such as
				 * the case of `</dd>', which only
				 * serves to end an `It' block that will
				 * be closed out by a subsequent `It' or
				 * end of clause `El' anyway.
				 * Skip the trailing space.
				 */
				if (TAGINFO_NOOP & tags[tag].flags) {
					i += sz;
					while (isspace((int)d->desc[i]))
						i++;
					break;
				} else if (TAGINFO_INLINE & tags[tag].flags) {
					fputs(tags[tag].mdoc, f);
					i += sz;
					break;
				}

				/* 
				 * A breaking mdoc(7) statement.
				 * Break the current line, output the
				 * macro, and conditionally break
				 * following that (or we might do
				 * nothing at all).
				 */
				if (col > 0) {
					fputs("\n", f);
					col = 0;
				}
				fputs(tags[tag].mdoc, f);
				if ( ! (TAGINFO_NOBR & tags[tag].flags)) {
					fputs("\n", f);
					col = 0;
				} else if ( ! (TAGINFO_NOSP & tags[tag].flags)) {
					fputs(" ", f);
					col++;
				}
				i += sz;
				while (isspace((int)d->desc[i]))
					i++;
				break;
			}
			if (tag < TAG__MAX)
				continue;
		} else if ('[' == d->desc[i] && 
			   ']' != d->desc[i + 1]) {
			/* Do we start at the bracket or bar? */
			for (sz = i + 1; sz < d->descsz; sz++) 
				if ('|' == d->desc[sz] ||
				    ']' == d->desc[sz])
					break;

			if (sz == d->descsz)
				continue;
			else if ('|' == d->desc[sz])
				i = sz + 1;
			else
				i = i + 1;

			/*
			 * Now handle in-page references.
			 * Print them out as-is: we've already
			 * accumulated them into our "SEE ALSO" values,
			 * which we'll use below.
			 */
			for ( ; i < d->descsz; i++, col++) {
				if (']' == d->desc[i]) {
					i++;
					break;
				}
				fputc(d->desc[i], f);
				col++;
			}
			continue;
		}

		if (' ' == d->desc[i] && 0 == col) {
			while (' ' == d->desc[i])
				i++;
			continue;
		}

		assert('\n' != d->desc[i]);

		/*
		 * Handle some oddities.
		 * The following HTML escapes exist in the output that I
		 * could find.
		 * There might be others...
		 */
		if (0 == strncmp(&d->desc[i], "&nbsp;", 6)) {
			i += 6;
			fputc(' ', f);
		} else if (0 == strncmp(&d->desc[i], "&lt;", 4)) {
			i += 4;
			fputc('<', f);
		} else if (0 == strncmp(&d->desc[i], "&gt;", 4)) {
			i += 4;
			fputc('>', f);
		} else if (0 == strncmp(&d->desc[i], "&#91;", 5)) {
			i += 5;
			fputc('[', f);
		} else {
			/* Make sure we don't trigger a macro. */
			if (0 == col && '.' == d->desc[i])
				fputs("\\&", f);
			fputc(d->desc[i], f);
			i++;
		}

		col++;
	}

	if (col > 0)
		fputs("\n", f);

	if (d->xrsz > 0) {
		/*
		 * Look up all of our keywords (which are in the xrs
		 * field) in the table of all known keywords.
		 * Don't print duplicates.
		 */
		lastres = NULL;
		for (last = 0, i = 0; i < d->xrsz; i++) {
			res = lookup(d->xrs[i]);
			/* Ignore self-reference. */
			if (res == d->nms[0] && verbose) 
				warnx("%s:%zu: self-reference: %s",
					d->fn, d->ln, d->xrs[i]);
			if (res == d->nms[0] && verbose) 
				continue;
			if (NULL == res && verbose) 
				warnx("%s:%zu: ref not found: %s",  
					d->fn, d->ln, d->xrs[i]);
			if (NULL == res)
				continue;

			/* Ignore duplicates. */
			if (NULL != lastres && lastres == res)
				continue;
			if (last)
				fputs(" ,\n", f);
			else
				fputs(".Sh SEE ALSO\n", f);
			fprintf(f, ".Xr %s 3", res);
			last = 1;
			lastres = res;
		}
		if (last)
			fputs("\n", f);
	}

	if (0 == nofile)
		fclose(f);
}

int
main(int argc, char *argv[])
{
	size_t		 i, len;
	FILE		*f;
	char		*cp;
	const char	*prefix;
	struct parse	 p;
	int		 rc, ch;
	struct defn	*d;
	struct decl	*e;

	rc = 0;
	prefix = ".";
	f = stdin;
	memset(&p, 0, sizeof(struct parse));
	p.fn = "<stdin>";
	p.ln = 0;
	p.phase = PHASE_INIT;
	TAILQ_INIT(&p.dqhead);

	while (-1 != (ch = getopt(argc, argv, "np:v")))
		switch (ch) {
		case ('n'):
			nofile = 1;
			break;
		case ('p'):
			prefix = optarg;
			break;
		case ('v'):
			verbose = 1;
			break;
		default:
			goto usage;
		}

	time_t now = time(NULL);
	struct tm tm;
	char mdocdate[256];
	if (gmtime_r(&now, &tm) == NULL)
		err(EXIT_FAILURE, "gmtime");
	strftime(mdocdate, sizeof(mdocdate), "%B %d, %Y", &tm);
	/*
	 * Read in line-by-line and process in the phase dictated by our
	 * finite state automaton.
	 */
	while (NULL != (cp = fgetln(f, &len))) {
		assert(len > 0);
		p.ln++;
		if ('\n' != cp[len - 1]) {
			warnx("%s:%zu: unterminated line", p.fn, p.ln);
			break;
		}
		cp[--len] = '\0';
		/* Lines are always nil-terminated. */
		switch (p.phase) {
		case (PHASE_INIT):
			init(&p, cp);
			break;
		case (PHASE_KEYS):
			keys(&p, cp, len);
			break;
		case (PHASE_DESC):
			desc(&p, cp, len);
			break;
		case (PHASE_SEEALSO):
			seealso(&p, cp, len);
			break;
		case (PHASE_DECL):
			decl(&p, cp, len);
			break;
		}
	}

	/*
	 * If we hit the last line, then try to process.
	 * Otherwise, we failed along the way.
	 */
	if (NULL == cp) {
		/* 
		 * Allow us to be at the declarations or scanning for
		 * the next clause.
		 */
		if (PHASE_INIT == p.phase ||
		    PHASE_DECL == p.phase) {
			if (0 == hcreate(5000))
				err(EXIT_FAILURE, "hcreate");
			TAILQ_FOREACH(d, &p.dqhead, entries)
				postprocess(prefix, d);
			TAILQ_FOREACH(d, &p.dqhead, entries)
				emit(d, mdocdate);
			rc = 1;
		} else if (PHASE_DECL != p.phase)
			warnx("%s:%zu: exit when not in "
				"initial state", p.fn, p.ln);
	}

	while ( ! TAILQ_EMPTY(&p.dqhead)) {
		d = TAILQ_FIRST(&p.dqhead);
		TAILQ_REMOVE(&p.dqhead, d, entries);
		while ( ! TAILQ_EMPTY(&d->dcqhead)) {
			e = TAILQ_FIRST(&d->dcqhead);
			TAILQ_REMOVE(&d->dcqhead, e, entries);
			free(e->text);
			free(e);
		}
		free(d->name);
		free(d->desc);
		free(d->dt);
		for (i = 0; i < d->nmsz; i++)
			free(d->nms[i]);
		for (i = 0; i < d->xrsz; i++)
			free(d->xrs[i]);
		for (i = 0; i < d->keysz; i++)
			free(d->keys[i]);
		free(d->keys);
		free(d->nms);
		free(d->xrs);
		free(d->fname);
		free(d->seealso);
		free(d->keybuf);
		free(d);
	}

	return(rc ? EXIT_SUCCESS : EXIT_FAILURE);
usage:
	fprintf(stderr, "usage: %s [-nv] [-p prefix]\n", getprogname());
	return(EXIT_FAILURE);
}

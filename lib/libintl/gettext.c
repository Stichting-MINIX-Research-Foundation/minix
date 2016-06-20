/*	$NetBSD: gettext.c,v 1.29 2015/05/29 12:26:28 christos Exp $	*/

/*-
 * Copyright (c) 2000, 2001 Citrus Project,
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Citrus: xpg4dl/FreeBSD/lib/libintl/gettext.c,v 1.31 2001/09/27 15:18:45 yamt Exp $
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: gettext.c,v 1.29 2015/05/29 12:26:28 christos Exp $");

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#if 0
#include <util.h>
#endif
#include <libintl.h>
#include <locale.h>
#include "libintl_local.h"
#include "plural_parser.h"
#include "pathnames.h"

/* GNU gettext added a hack to add some context to messages. If a message is
 * used in multiple locations, it needs some amount of context to make the
 * translation clear to translators. GNU gettext, rather than modifying the
 * message format, concatenates the context, \004 and the message id.
 */
#define	MSGCTXT_ID_SEPARATOR	'\004'

static const char *pgettext_impl(const char *, const char *, const char *,
				const char *, unsigned long int, int);
static char *concatenate_ctxt_id(const char *, const char *);
static const char *lookup_category(int);
static const char *split_locale(const char *);
static const char *lookup_mofile(char *, size_t, const char *, const char *,
				 const char *, const char *,
				 struct domainbinding *);
static uint32_t flip(uint32_t, uint32_t);
static int validate(void *, struct mohandle *);
static int mapit(const char *, struct domainbinding *);
static int unmapit(struct domainbinding *);
static const char *lookup_hash(const char *, struct domainbinding *, size_t *);
static const char *lookup_bsearch(const char *, struct domainbinding *,
				  size_t *);
static const char *lookup(const char *, struct domainbinding *, size_t *);
static const char *get_lang_env(const char *);

/*
 * shortcut functions.  the main implementation resides in dcngettext().
 */
char *
gettext(const char *msgid)
{

	return dcngettext(NULL, msgid, NULL, 1UL, LC_MESSAGES);
}

char *
dgettext(const char *domainname, const char *msgid)
{

	return dcngettext(domainname, msgid, NULL, 1UL, LC_MESSAGES);
}

char *
dcgettext(const char *domainname, const char *msgid, int category)
{

	return dcngettext(domainname, msgid, NULL, 1UL, category);
}

char *
ngettext(const char *msgid1, const char *msgid2, unsigned long int n)
{

	return dcngettext(NULL, msgid1, msgid2, n, LC_MESSAGES);
}

char *
dngettext(const char *domainname, const char *msgid1, const char *msgid2,
	  unsigned long int n)
{

	return dcngettext(domainname, msgid1, msgid2, n, LC_MESSAGES);
}

const char *
pgettext(const char *msgctxt, const char *msgid)
{

	return pgettext_impl(NULL, msgctxt, msgid, NULL, 1UL, LC_MESSAGES);
}

const char *
dpgettext(const char *domainname, const char *msgctxt, const char *msgid)
{

	return pgettext_impl(domainname, msgctxt, msgid, NULL, 1UL, LC_MESSAGES);
}

const char *
dcpgettext(const char *domainname, const char *msgctxt, const char *msgid,
	int category)
{

	return pgettext_impl(domainname, msgctxt, msgid, NULL, 1UL, category);
}

const char *
npgettext(const char *msgctxt, const char *msgid1, const char *msgid2,
	unsigned long int n)
{

	return pgettext_impl(NULL, msgctxt, msgid1, msgid2, n, LC_MESSAGES);
}

const char *
dnpgettext(const char *domainname, const char *msgctxt, const char *msgid1,
	const char *msgid2, unsigned long int n)
{

	return pgettext_impl(domainname, msgctxt, msgid1, msgid2, n, LC_MESSAGES);
}

const char *
dcnpgettext(const char *domainname, const char *msgctxt, const char *msgid1,
	const char *msgid2, unsigned long int n, int category)
{

	return pgettext_impl(domainname, msgctxt, msgid1, msgid2, n, category);
}

static const char *
pgettext_impl(const char *domainname, const char *msgctxt, const char *msgid1,
	const char *msgid2, unsigned long int n, int category)
{
	char *msgctxt_id;
	char *translation;
	char *p;

	if ((msgctxt_id = concatenate_ctxt_id(msgctxt, msgid1)) == NULL)
		return msgid1;

	translation = dcngettext(domainname, msgctxt_id,
		msgid2, n, category);
	free(msgctxt_id);

	p = strchr(translation, '\004');
	if (p)
		return p + 1;
	return translation;
}

/*
 * dcngettext() -
 * lookup internationalized message on database locale/category/domainname
 * (like ja_JP.eucJP/LC_MESSAGES/domainname).
 * if n equals to 1, internationalized message will be looked up for msgid1.
 * otherwise, message will be looked up for msgid2.
 * if the lookup fails, the function will return msgid1 or msgid2 as is.
 *
 * Even though the return type is "char *", caller should not rewrite the
 * region pointed to by the return value (should be "const char *", but can't
 * change it for compatibility with other implementations).
 *
 * by default (if domainname == NULL), domainname is taken from the value set
 * by textdomain().  usually name of the application (like "ls") is used as
 * domainname.  category is usually LC_MESSAGES.
 *
 * the code reads in *.mo files generated by GNU gettext.  *.mo is a host-
 * endian encoded file.  both endians are supported here, as the files are in
 * /usr/share/locale! (or we should move those files into /usr/libdata)
 */

static char *
concatenate_ctxt_id(const char *msgctxt, const char *msgid)
{
	char *ret;

	if (asprintf(&ret, "%s%c%s", msgctxt, MSGCTXT_ID_SEPARATOR, msgid) == -1)
		return NULL;

	return ret;
}

static const char *
lookup_category(int category)
{

	switch (category) {
	case LC_COLLATE:	return "LC_COLLATE";
	case LC_CTYPE:		return "LC_CTYPE";
	case LC_MONETARY:	return "LC_MONETARY";
	case LC_NUMERIC:	return "LC_NUMERIC";
	case LC_TIME:		return "LC_TIME";
	case LC_MESSAGES:	return "LC_MESSAGES";
	}
	return NULL;
}

/*
 * XPG syntax: language[_territory[.codeset]][@modifier]
 * XXX boundary check on "result" is lacking
 */
static const char *
split_locale(const char *lname)
{
	char buf[BUFSIZ], tmp[BUFSIZ];
	char *l, *t, *c, *m;
	static char result[BUFSIZ];

	memset(result, 0, sizeof(result));

	if (strlen(lname) + 1 > sizeof(buf)) {
fail:
		return lname;
	}

	strlcpy(buf, lname, sizeof(buf));
	m = strrchr(buf, '@');
	if (m)
		*m++ = '\0';
	c = strrchr(buf, '.');
	if (c)
		*c++ = '\0';
	t = strrchr(buf, '_');
	if (t)
		*t++ = '\0';
	l = buf;
	if (strlen(l) == 0)
		goto fail;
	if (c && !t)
		goto fail;

	if (m) {
		if (t) {
			if (c) {
				snprintf(tmp, sizeof(tmp), "%s_%s.%s@%s",
				    l, t, c, m);
				strlcat(result, tmp, sizeof(result));
				strlcat(result, ":", sizeof(result));
			}
			snprintf(tmp, sizeof(tmp), "%s_%s@%s", l, t, m);
			strlcat(result, tmp, sizeof(result));
			strlcat(result, ":", sizeof(result));
		}
		snprintf(tmp, sizeof(tmp), "%s@%s", l, m);
		strlcat(result, tmp, sizeof(result));
		strlcat(result, ":", sizeof(result));
	}
	if (t) {
		if (c) {
			snprintf(tmp, sizeof(tmp), "%s_%s.%s", l, t, c);
			strlcat(result, tmp, sizeof(result));
			strlcat(result, ":", sizeof(result));
		}
		snprintf(tmp, sizeof(tmp), "%s_%s", l, t);
		strlcat(result, tmp, sizeof(result));
		strlcat(result, ":", sizeof(result));
	}
	strlcat(result, l, sizeof(result));

	return result;
}

static const char *
lookup_mofile(char *buf, size_t len, const char *dir, const char *lpath,
	      const char *category, const char *domainname,
	      struct domainbinding *db)
{
	struct stat st;
	char *p, *q;
	char lpath_tmp[BUFSIZ];

	/*
	 * LANGUAGE is a colon separated list of locale names.
	 */

	strlcpy(lpath_tmp, lpath, sizeof(lpath_tmp));
	q = lpath_tmp;
	/* CONSTCOND */
	while (1) {
		p = strsep(&q, ":");
		if (!p)
			break;
		if (!*p)
			continue;

		/* don't mess with default locales */
		if (strcmp(p, "C") == 0 || strcmp(p, "POSIX") == 0)
			return NULL;

		/* validate pathname */
		if (strchr(p, '/') || strchr(category, '/'))
			continue;
#if 1	/*?*/
		if (strchr(domainname, '/'))
			continue;
#endif

		snprintf(buf, len, "%s/%s/%s/%s.mo", dir, p,
		    category, domainname);
		if (stat(buf, &st) < 0)
			continue;
		if ((st.st_mode & S_IFMT) != S_IFREG)
			continue;

		if (mapit(buf, db) == 0)
			return buf;
	}

	return NULL;
}

static uint32_t
flip(uint32_t v, uint32_t magic)
{

	if (magic == MO_MAGIC)
		return v;
	else if (magic == MO_MAGIC_SWAPPED) {
		v = ((v >> 24) & 0xff) | ((v >> 8) & 0xff00) |
		    ((v << 8) & 0xff0000) | ((v << 24) & 0xff000000);
		return v;
	} else {
		abort();
		/*NOTREACHED*/
	}
}

static int
validate(void *arg, struct mohandle *mohandle)
{
	char *p;

	p = (char *)arg;
	if (p < (char *)mohandle->addr ||
	    p > (char *)mohandle->addr + mohandle->len)
		return 0;
	else
		return 1;
}

/*
 * calculate the step value if the hash value is conflicted.
 */
static __inline uint32_t
calc_collision_step(uint32_t hashval, uint32_t hashsize)
{
	_DIAGASSERT(hashsize>2);
	return (hashval % (hashsize - 2)) + 1;
}

/*
 * calculate the next index while conflicting.
 */
static __inline uint32_t
calc_next_index(uint32_t curidx, uint32_t hashsize, uint32_t step)
{
	return curidx+step - (curidx >= hashsize-step ? hashsize : 0);
}

static int
get_sysdep_string_table(struct mosysdepstr_h **table_h, uint32_t *ofstable,
			uint32_t nstrings, uint32_t magic, char *base)
{
	unsigned int i;
	int j, count;
	size_t l;
	struct mosysdepstr *table;

	for (i=0; i<nstrings; i++) {
		/* get mosysdepstr record */
		/* LINTED: ignore the alignment problem. */
		table = (struct mosysdepstr *)(base + flip(ofstable[i], magic));
		/* count number of segments */
		count = 0;
		while (flip(table->segs[count++].ref, magic) != MO_LASTSEG)
			;
		/* get table */
		l = sizeof(struct mosysdepstr_h) +
		    sizeof(struct mosysdepsegentry_h) * (count-1);
		table_h[i] = (struct mosysdepstr_h *)malloc(l);
		if (!table_h[i])
			return -1;
		memset(table_h[i], 0, l);
		table_h[i]->off = (const char *)(base + flip(table->off, magic));
		for (j=0; j<count; j++) {
			table_h[i]->segs[j].len =
			    flip(table->segs[j].len, magic);
			table_h[i]->segs[j].ref =
			    flip(table->segs[j].ref, magic);
		}
		/* LINTED: ignore the alignment problem. */
		table = (struct mosysdepstr *)&table->segs[count];
	}
	return 0;
}

static int
expand_sysdep(struct mohandle *mohandle, struct mosysdepstr_h *str)
{
	int i;
	const char *src;
	char *dst;

	/* check whether already expanded */
	if (str->expanded)
		return 0;

	/* calc total length */
	str->expanded_len = 1;
	for (i=0; /*CONSTCOND*/1; i++) {
		str->expanded_len += str->segs[i].len;
		if (str->segs[i].ref == MO_LASTSEG)
			break;
		str->expanded_len +=
		    mohandle->mo.mo_sysdep_segs[str->segs[i].ref].len;
	}
	/* expand */
	str->expanded = malloc(str->expanded_len);
	if (!str->expanded)
		return -1;
	src = str->off;
	dst = str->expanded;
	for (i=0; /*CONSTCOND*/1; i++) {
		memcpy(dst, src, str->segs[i].len);
		src += str->segs[i].len;
		dst += str->segs[i].len;
		if (str->segs[i].ref == MO_LASTSEG)
			break;
		memcpy(dst, mohandle->mo.mo_sysdep_segs[str->segs[i].ref].str,
		       mohandle->mo.mo_sysdep_segs[str->segs[i].ref].len);
		dst += mohandle->mo.mo_sysdep_segs[str->segs[i].ref].len;
	}
	*dst = '\0';

	return 0;
}

static void
insert_to_hash(uint32_t *htable, uint32_t hsize, const char *str, uint32_t ref)
{
	uint32_t hashval, idx, step;

	hashval = __intl_string_hash(str);
	step = calc_collision_step(hashval, hsize);
	idx = hashval % hsize;

	while (htable[idx])
		idx = calc_next_index(idx, hsize, step);

	htable[idx] = ref;
}

static int
setup_sysdep_stuffs(struct mo *mo, struct mohandle *mohandle, char *base)
{
	uint32_t magic;
	struct moentry *stable;
	size_t l;
	unsigned int i;
	char *v;
	uint32_t *ofstable;

	magic = mo->mo_magic;

	mohandle->mo.mo_sysdep_nsegs = flip(mo->mo_sysdep_nsegs, magic);
	mohandle->mo.mo_sysdep_nstring = flip(mo->mo_sysdep_nstring, magic);

	if (mohandle->mo.mo_sysdep_nstring == 0)
		return 0;

	/* check hash size */
	if (mohandle->mo.mo_hsize <= 2 ||
	    mohandle->mo.mo_hsize <
	    (mohandle->mo.mo_nstring + mohandle->mo.mo_sysdep_nstring))
		return -1;

	/* get sysdep segments */
	l = sizeof(struct mosysdepsegs_h) * mohandle->mo.mo_sysdep_nsegs;
	mohandle->mo.mo_sysdep_segs = (struct mosysdepsegs_h *)malloc(l);
	if (!mohandle->mo.mo_sysdep_segs)
		return -1;
	/* LINTED: ignore the alignment problem. */
	stable = (struct moentry *)(base + flip(mo->mo_sysdep_segoff, magic));
	for (i=0; i<mohandle->mo.mo_sysdep_nsegs; i++) {
		v = base + flip(stable[i].off, magic);
		mohandle->mo.mo_sysdep_segs[i].str =
		    __intl_sysdep_get_string_by_tag(
			    v,
			    &mohandle->mo.mo_sysdep_segs[i].len);
	}

	/* get sysdep string table */
	mohandle->mo.mo_sysdep_otable =
	    (struct mosysdepstr_h **)calloc(mohandle->mo.mo_sysdep_nstring,
					    sizeof(struct mosysdepstr_h *));
	if (!mohandle->mo.mo_sysdep_otable)
		return -1;
	/* LINTED: ignore the alignment problem. */
	ofstable = (uint32_t *)(base + flip(mo->mo_sysdep_otable, magic));
	if (get_sysdep_string_table(mohandle->mo.mo_sysdep_otable, ofstable,
				    mohandle->mo.mo_sysdep_nstring, magic,
				    base))
		return -1;
	mohandle->mo.mo_sysdep_ttable =
	    (struct mosysdepstr_h **)calloc(mohandle->mo.mo_sysdep_nstring,
					    sizeof(struct mosysdepstr_h *));
	if (!mohandle->mo.mo_sysdep_ttable)
		return -1;
	/* LINTED: ignore the alignment problem. */
	ofstable = (uint32_t *)(base + flip(mo->mo_sysdep_ttable, magic));
	if (get_sysdep_string_table(mohandle->mo.mo_sysdep_ttable, ofstable,
				    mohandle->mo.mo_sysdep_nstring, magic,
				    base))
		return -1;

	/* update hash */
	for (i=0; i<mohandle->mo.mo_sysdep_nstring; i++) {
		if (expand_sysdep(mohandle, mohandle->mo.mo_sysdep_otable[i]))
			return -1;
		insert_to_hash(mohandle->mo.mo_htable,
			       mohandle->mo.mo_hsize,
			       mohandle->mo.mo_sysdep_otable[i]->expanded,
			       (i+1) | MO_HASH_SYSDEP_MASK);
	}

	return 0;
}

int
mapit(const char *path, struct domainbinding *db)
{
	int fd;
	struct stat st;
	char *base;
	uint32_t magic, revision, flags = 0;
	struct moentry *otable, *ttable;
	const uint32_t *htable;
	struct moentry_h *p;
	struct mo *mo;
	size_t l, headerlen;
	unsigned int i;
	char *v;
	struct mohandle *mohandle = &db->mohandle;

	if (mohandle->addr && mohandle->addr != MAP_FAILED &&
	    mohandle->mo.mo_magic)
		return 0;	/*already opened*/

	unmapit(db);

#if 0
	if (secure_path(path) != 0)
		goto fail;
#endif
	if (stat(path, &st) < 0)
		goto fail;
	if ((st.st_mode & S_IFMT) != S_IFREG || st.st_size > GETTEXT_MMAP_MAX)
		goto fail;
	fd = open(path, O_RDONLY);
	if (fd < 0)
		goto fail;
	if (read(fd, &magic, sizeof(magic)) != sizeof(magic) ||
	    (magic != MO_MAGIC && magic != MO_MAGIC_SWAPPED)) {
		close(fd);
		goto fail;
	}
	if (read(fd, &revision, sizeof(revision)) != sizeof(revision)) {
		close(fd);
		goto fail;
	}
	switch (flip(revision, magic)) {
	case MO_MAKE_REV(0, 0):
		break;
	case MO_MAKE_REV(0, 1):
	case MO_MAKE_REV(1, 1):
		flags |= MO_F_SYSDEP;
		break;
	default:
		close(fd);
		goto fail;
	}
	mohandle->addr = mmap(NULL, (size_t)st.st_size, PROT_READ,
	    MAP_FILE | MAP_SHARED, fd, (off_t)0);
	if (!mohandle->addr || mohandle->addr == MAP_FAILED) {
		close(fd);
		goto fail;
	}
	close(fd);
	mohandle->len = (size_t)st.st_size;

	base = mohandle->addr;
	mo = (struct mo *)mohandle->addr;

	/* flip endian.  do not flip magic number! */
	mohandle->mo.mo_magic = mo->mo_magic;
	mohandle->mo.mo_revision = flip(mo->mo_revision, magic);
	mohandle->mo.mo_nstring = flip(mo->mo_nstring, magic);
	mohandle->mo.mo_hsize = flip(mo->mo_hsize, magic);
	mohandle->mo.mo_flags = flags;

	/* validate otable/ttable */
	/* LINTED: ignore the alignment problem. */
	otable = (struct moentry *)(base + flip(mo->mo_otable, magic));
	/* LINTED: ignore the alignment problem. */
	ttable = (struct moentry *)(base + flip(mo->mo_ttable, magic));
	if (!validate(otable, mohandle) ||
	    !validate(&otable[mohandle->mo.mo_nstring], mohandle)) {
		unmapit(db);
		goto fail;
	}
	if (!validate(ttable, mohandle) ||
	    !validate(&ttable[mohandle->mo.mo_nstring], mohandle)) {
		unmapit(db);
		goto fail;
	}

	/* allocate [ot]table, and convert to normal pointer representation. */
	l = sizeof(struct moentry_h) * mohandle->mo.mo_nstring;
	mohandle->mo.mo_otable = (struct moentry_h *)malloc(l);
	if (!mohandle->mo.mo_otable) {
		unmapit(db);
		goto fail;
	}
	mohandle->mo.mo_ttable = (struct moentry_h *)malloc(l);
	if (!mohandle->mo.mo_ttable) {
		unmapit(db);
		goto fail;
	}
	p = mohandle->mo.mo_otable;
	for (i = 0; i < mohandle->mo.mo_nstring; i++) {
		p[i].len = flip(otable[i].len, magic);
		p[i].off = base + flip(otable[i].off, magic);

		if (!validate(p[i].off, mohandle) ||
		    !validate(p[i].off + p[i].len + 1, mohandle)) {
			unmapit(db);
			goto fail;
		}
	}
	p = mohandle->mo.mo_ttable;
	for (i = 0; i < mohandle->mo.mo_nstring; i++) {
		p[i].len = flip(ttable[i].len, magic);
		p[i].off = base + flip(ttable[i].off, magic);

		if (!validate(p[i].off, mohandle) ||
		    !validate(p[i].off + p[i].len + 1, mohandle)) {
			unmapit(db);
			goto fail;
		}
	}
	/* allocate htable, and convert it to the host order. */
	if (mohandle->mo.mo_hsize > 2) {
		l = sizeof(uint32_t) * mohandle->mo.mo_hsize;
		mohandle->mo.mo_htable = (uint32_t *)malloc(l);
		if (!mohandle->mo.mo_htable) {
			unmapit(db);
			goto fail;
		}
		/* LINTED: ignore the alignment problem. */
		htable = (const uint32_t *)(base+flip(mo->mo_hoffset, magic));
		for (i=0; i < mohandle->mo.mo_hsize; i++) {
			mohandle->mo.mo_htable[i] = flip(htable[i], magic);
			if (mohandle->mo.mo_htable[i] >=
			    mohandle->mo.mo_nstring+1) {
				/* illegal string number. */
				unmapit(db);
				goto fail;
			}
		}
	}
	/* grab MIME-header and charset field */
	mohandle->mo.mo_header = lookup("", db, &headerlen);
	if (mohandle->mo.mo_header)
		v = strstr(mohandle->mo.mo_header, "charset=");
	else
		v = NULL;
	if (v) {
		mohandle->mo.mo_charset = strdup(v + 8);
		if (!mohandle->mo.mo_charset)
			goto fail;
		v = strchr(mohandle->mo.mo_charset, '\n');
		if (v)
			*v = '\0';
	}
	if (!mohandle->mo.mo_header ||
	    _gettext_parse_plural(&mohandle->mo.mo_plural,
				  &mohandle->mo.mo_nplurals,
				  mohandle->mo.mo_header, headerlen))
		mohandle->mo.mo_plural = NULL;

	/*
	 * XXX check charset, reject it if we are unable to support the charset
	 * with the current locale.
	 * for example, if we are using euc-jp locale and we are looking at
	 * *.mo file encoded by euc-kr (charset=euc-kr), we should reject
	 * the *.mo file as we cannot support it.
	 */

	/* system dependent string support */
	if ((mohandle->mo.mo_flags & MO_F_SYSDEP) != 0) {
		if (setup_sysdep_stuffs(mo, mohandle, base)) {
			unmapit(db);
			goto fail;
		}
	}

	return 0;

fail:
	return -1;
}

static void
free_sysdep_table(struct mosysdepstr_h **table, uint32_t nstring)
{

	if (! table)
		return;

	for (uint32_t i = 0; i < nstring; i++) {
		if (table[i]) {
			free(table[i]->expanded);
			free(table[i]);
		}
	}
	free(table);
}

static int
unmapit(struct domainbinding *db)
{
	struct mohandle *mohandle = &db->mohandle;

	/* unmap if there's already mapped region */
	if (mohandle->addr && mohandle->addr != MAP_FAILED)
		munmap(mohandle->addr, mohandle->len);
	mohandle->addr = NULL;
	free(mohandle->mo.mo_otable);
	free(mohandle->mo.mo_ttable);
	free(mohandle->mo.mo_charset);
	free(mohandle->mo.mo_htable);
	free(mohandle->mo.mo_sysdep_segs);
	free_sysdep_table(mohandle->mo.mo_sysdep_otable,
	    mohandle->mo.mo_sysdep_nstring);
	free_sysdep_table(mohandle->mo.mo_sysdep_ttable,
	    mohandle->mo.mo_sysdep_nstring);
	_gettext_free_plural(mohandle->mo.mo_plural);
	memset(&mohandle->mo, 0, sizeof(mohandle->mo));
	return 0;
}

/* ARGSUSED */
static const char *
lookup_hash(const char *msgid, struct domainbinding *db, size_t *rlen)
{
	struct mohandle *mohandle = &db->mohandle;
	uint32_t idx, hashval, step, strno;
	size_t len;
	struct mosysdepstr_h *sysdep_otable, *sysdep_ttable;

	if (mohandle->mo.mo_hsize <= 2 || mohandle->mo.mo_htable == NULL)
		return NULL;

	hashval = __intl_string_hash(msgid);
	step = calc_collision_step(hashval, mohandle->mo.mo_hsize);
	idx = hashval % mohandle->mo.mo_hsize;
	len = strlen(msgid);
	while (/*CONSTCOND*/1) {
		strno = mohandle->mo.mo_htable[idx];
		if (strno == 0) {
			/* unexpected miss */
			return NULL;
		}
		strno--;
		if ((strno & MO_HASH_SYSDEP_MASK) == 0) {
			/* system independent strings */
			if (len <= mohandle->mo.mo_otable[strno].len &&
			    !strcmp(msgid, mohandle->mo.mo_otable[strno].off)) {
				/* hit */
				if (rlen)
					*rlen =
					    mohandle->mo.mo_ttable[strno].len;
				return mohandle->mo.mo_ttable[strno].off;
			}
		} else {
			/* system dependent strings */
			strno &= ~MO_HASH_SYSDEP_MASK;
			sysdep_otable = mohandle->mo.mo_sysdep_otable[strno];
			sysdep_ttable = mohandle->mo.mo_sysdep_ttable[strno];
			if (len <= sysdep_otable->expanded_len &&
			    !strcmp(msgid, sysdep_otable->expanded)) {
				/* hit */
				if (expand_sysdep(mohandle, sysdep_ttable))
					/* memory exhausted */
					return NULL;
				if (rlen)
					*rlen = sysdep_ttable->expanded_len;
				return sysdep_ttable->expanded;
			}
		}
		idx = calc_next_index(idx, mohandle->mo.mo_hsize, step);
	}
	/*NOTREACHED*/
}

static const char *
lookup_bsearch(const char *msgid, struct domainbinding *db, size_t *rlen)
{
	int top, bottom, middle, omiddle;
	int n;
	struct mohandle *mohandle = &db->mohandle;

	top = 0;
	bottom = mohandle->mo.mo_nstring;
	omiddle = -1;
	/* CONSTCOND */
	while (1) {
		if (top > bottom)
			break;
		middle = (top + bottom) / 2;
		/* avoid possible infinite loop, when the data is not sorted */
		if (omiddle == middle)
			break;
		if ((size_t)middle >= mohandle->mo.mo_nstring)
			break;

		n = strcmp(msgid, mohandle->mo.mo_otable[middle].off);
		if (n == 0) {
			if (rlen)
				*rlen = mohandle->mo.mo_ttable[middle].len;
			return (const char *)mohandle->mo.mo_ttable[middle].off;
		}
		else if (n < 0)
			bottom = middle;
		else
			top = middle;
		omiddle = middle;
	}

	return NULL;
}

static const char *
lookup(const char *msgid, struct domainbinding *db, size_t *rlen)
{
	const char *v;

	v = lookup_hash(msgid, db, rlen);
	if (v)
		return v;

	return lookup_bsearch(msgid, db, rlen);
}

static const char *
get_lang_env(const char *category_name)
{
	const char *lang;

	/*
	 * 1. see LANGUAGE variable first.
	 *
	 * LANGUAGE is a GNU extension.
	 * It's a colon separated list of locale names.
	 */
	lang = getenv("LANGUAGE");
	if (lang)
		return lang;

	/*
	 * 2. if LANGUAGE isn't set, see LC_ALL, LC_xxx, LANG.
	 *
	 * It's essentially setlocale(LC_xxx, NULL).
	 */
	lang = getenv("LC_ALL");
	if (!lang)
		lang = getenv(category_name);
	if (!lang)
		lang = getenv("LANG");

	if (!lang)
		return 0; /* error */

	return split_locale(lang);
}

static const char *
get_indexed_string(const char *str, size_t len, unsigned long idx)
{
	while (idx > 0) {
		if (len <= 1)
			return str;
		if (*str == '\0')
			idx--;
		if (len > 0) {
			str++;
			len--;
		}
	}
	return str;
}

#define	_NGETTEXT_DEFAULT(msgid1, msgid2, n)	\
	((char *)__UNCONST((n) == 1 ? (msgid1) : (msgid2)))

char *
dcngettext(const char *domainname, const char *msgid1, const char *msgid2,
	   unsigned long int n, int category)
{
	const char *msgid;
	char path[PATH_MAX];
	const char *lpath;
	static char olpath[PATH_MAX];
	const char *cname = NULL;
	const char *v;
	static char *ocname = NULL;
	static char *odomainname = NULL;
	struct domainbinding *db;
	unsigned long plural_index = 0;
	size_t len;

	if (!domainname)
		domainname = __current_domainname;
	cname = lookup_category(category);
	if (!domainname || !cname)
		goto fail;

	lpath = get_lang_env(cname);
	if (!lpath)
		goto fail;

	for (db = __bindings; db; db = db->next)
		if (strcmp(db->domainname, domainname) == 0)
			break;
	if (!db) {
		if (!bindtextdomain(domainname, _PATH_TEXTDOMAIN))
			goto fail;
		db = __bindings;
	}

	/* resolve relative path */
	/* XXX not necessary? */
	if (db->path[0] != '/') {
		char buf[PATH_MAX];

		if (getcwd(buf, sizeof(buf)) == 0)
			goto fail;
		if (strlcat(buf, "/", sizeof(buf)) >= sizeof(buf))
			goto fail;
		if (strlcat(buf, db->path, sizeof(buf)) >= sizeof(buf))
			goto fail;
		strlcpy(db->path, buf, sizeof(db->path));
	}

	/* don't bother looking it up if the values are the same */
	if (odomainname && strcmp(domainname, odomainname) == 0 &&
	    ocname && strcmp(cname, ocname) == 0 && strcmp(lpath, olpath) == 0 &&
	    db->mohandle.mo.mo_magic)
		goto found;

	/* try to find appropriate file, from $LANGUAGE */
	if (lookup_mofile(path, sizeof(path), db->path, lpath, cname,
	    domainname, db) == NULL)
		goto fail;

	free(odomainname);
	free(ocname);

	odomainname = strdup(domainname);
	ocname = strdup(cname);
	if (!odomainname || !ocname) {
		free(odomainname);
		free(ocname);

		odomainname = ocname = NULL;
	}
	else
		strlcpy(olpath, lpath, sizeof(olpath));

found:
	if (db->mohandle.mo.mo_plural) {
		plural_index =
		    _gettext_calculate_plural(db->mohandle.mo.mo_plural, n);
		if (plural_index >= db->mohandle.mo.mo_nplurals)
			plural_index = 0;
		msgid = msgid1;
	} else
		msgid = _NGETTEXT_DEFAULT(msgid1, msgid2, n);

	if (msgid == NULL)
		return NULL;

	v = lookup(msgid, db, &len);
	if (v) {
		if (db->mohandle.mo.mo_plural)
			v = get_indexed_string(v, len, plural_index);
		/*
		 * convert the translated message's encoding.
		 *
		 * special case:
		 *	a result of gettext("") shouldn't need any conversion.
		 */
		if (msgid[0])
			v = __gettext_iconv(v, db);

		/*
		 * Given the amount of printf-format security issues, it may
		 * be a good idea to validate if the original msgid and the
		 * translated message format string carry the same printf-like
		 * format identifiers.
		 */

		msgid = v;
	}

	return (char *)__UNCONST(msgid);

fail:
	return _NGETTEXT_DEFAULT(msgid1, msgid2, n);
}

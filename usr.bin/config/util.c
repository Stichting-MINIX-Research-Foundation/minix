/*	$NetBSD: util.c,v 1.20 2015/09/01 13:42:48 uebayasi Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratories.
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
 *	from: @(#)util.c	8.1 (Berkeley) 6/6/93
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
__RCSID("$NetBSD: util.c,v 1.20 2015/09/01 13:42:48 uebayasi Exp $");

#include <sys/types.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <util.h>
#include <err.h>
#include "defs.h"

static void cfgvxerror(const char *, int, const char *, va_list)
	     __printflike(3, 0);
static void cfgvxdbg(const char *, int, const char *, va_list)
	     __printflike(3, 0);
static void cfgvxwarn(const char *, int, const char *, va_list)
	     __printflike(3, 0);
static void cfgvxmsg(const char *, int, const char *, const char *, va_list)
     __printflike(4, 0);

/************************************************************/

/*
 * Prefix stack
 */

static void
prefixlist_push(struct prefixlist *pl, const char *path)
{
	struct prefix *prevpf = SLIST_FIRST(pl);
	struct prefix *pf;
	char *cp;

	pf = ecalloc(1, sizeof(struct prefix));

	if (prevpf != NULL) {
		cp = emalloc(strlen(prevpf->pf_prefix) + 1 +
		    strlen(path) + 1);
		(void) sprintf(cp, "%s/%s", prevpf->pf_prefix, path);
		pf->pf_prefix = intern(cp);
		free(cp);
	} else
		pf->pf_prefix = intern(path);

	SLIST_INSERT_HEAD(pl, pf, pf_next);
}

static void
prefixlist_pop(struct prefixlist *allpl, struct prefixlist *pl)
{
	struct prefix *pf;

	if ((pf = SLIST_FIRST(pl)) == NULL) {
		cfgerror("no prefixes on the stack to pop");
		return;
	}

	SLIST_REMOVE_HEAD(pl, pf_next);
	/* Remember this prefix for emitting -I... directives later. */
	SLIST_INSERT_HEAD(allpl, pf, pf_next);
}

/*
 * Push a prefix onto the prefix stack.
 */
void
prefix_push(const char *path)
{
	prefixlist_push(&prefixes, path);
}

/*
 * Pop a prefix off the prefix stack.
 */
void
prefix_pop(void)
{
	prefixlist_pop(&allprefixes, &prefixes);
}

/*
 * Push a buildprefix onto the buildprefix stack.
 */
void
buildprefix_push(const char *path)
{
	prefixlist_push(&buildprefixes, path);
}

/*
 * Pop a buildprefix off the buildprefix stack.
 */
void
buildprefix_pop(void)
{
	prefixlist_pop(&allbuildprefixes, &buildprefixes);
}

/*
 * Prepend the source path to a file name.
 */
char *
sourcepath(const char *file)
{
	size_t len;
	char *cp;
	struct prefix *pf;

	pf = SLIST_EMPTY(&prefixes) ? NULL : SLIST_FIRST(&prefixes);
	if (pf != NULL && *pf->pf_prefix == '/')
		len = strlen(pf->pf_prefix) + 1 + strlen(file) + 1;
	else {
		len = strlen(srcdir) + 1 + strlen(file) + 1;
		if (pf != NULL)
			len += strlen(pf->pf_prefix) + 1;
	}

	cp = emalloc(len);

	if (pf != NULL) {
		if (*pf->pf_prefix == '/')
			(void) sprintf(cp, "%s/%s", pf->pf_prefix, file);
		else
			(void) sprintf(cp, "%s/%s/%s", srcdir,
			    pf->pf_prefix, file);
	} else
		(void) sprintf(cp, "%s/%s", srcdir, file);
	return (cp);
}

/************************************************************/

/*
 * Data structures
 */

/*
 * nvlist
 */

struct nvlist *
newnv(const char *name, const char *str, void *ptr, long long i, struct nvlist *next)
{
	struct nvlist *nv;

	nv = ecalloc(1, sizeof(*nv));
	nv->nv_next = next;
	nv->nv_name = name;
	nv->nv_str = str;
	nv->nv_ptr = ptr;
	nv->nv_num = i;
	return nv;
}

/*
 * Free an nvlist structure (just one).
 */
void
nvfree(struct nvlist *nv)
{

	free(nv);
}

/*
 * Free an nvlist (the whole list).
 */
void
nvfreel(struct nvlist *nv)
{
	struct nvlist *next;

	for (; nv != NULL; nv = next) {
		next = nv->nv_next;
		free(nv);
	}
}

struct nvlist *
nvcat(struct nvlist *nv1, struct nvlist *nv2)
{
	struct nvlist *nv;

	if (nv1 == NULL)
		return nv2;

	for (nv = nv1; nv->nv_next != NULL; nv = nv->nv_next);

	nv->nv_next = nv2;
	return nv1;
}

/*
 * Option definition lists
 */

struct defoptlist *
defoptlist_create(const char *name, const char *val, const char *lintval)
{
	struct defoptlist *dl;

	dl = emalloc(sizeof(*dl));
	dl->dl_next = NULL;
	dl->dl_name = name;
	dl->dl_value = val;
	dl->dl_lintvalue = lintval;
	dl->dl_obsolete = 0;
	dl->dl_depends = NULL;
	return dl;
}

void
defoptlist_destroy(struct defoptlist *dl)
{
	struct defoptlist *next;

	while (dl != NULL) {
		next = dl->dl_next;
		dl->dl_next = NULL;

		// XXX should we assert that dl->dl_deps is null to
		// be sure the deps have already been destroyed?
		free(dl);

		dl = next;
	}
}

struct defoptlist *
defoptlist_append(struct defoptlist *dla, struct defoptlist *dlb)
{
	struct defoptlist *dl;

	if (dla == NULL)
		return dlb;

	for (dl = dla; dl->dl_next != NULL; dl = dl->dl_next)
		;

	dl->dl_next = dlb;
	return dla;
}

/*
 * Locator lists
 */

struct loclist *
loclist_create(const char *name, const char *string, long long num)
{
	struct loclist *ll;

	ll = emalloc(sizeof(*ll));
	ll->ll_name = name;
	ll->ll_string = string;
	ll->ll_num = num;
	ll->ll_next = NULL;
	return ll;
}

void
loclist_destroy(struct loclist *ll)
{
	struct loclist *next;

	while (ll != NULL) {
		next = ll->ll_next;
		ll->ll_next = NULL;
		free(ll);
		ll = next;
	}
}

/*
 * Attribute lists
 */

struct attrlist *
attrlist_create(void)
{
	struct attrlist *al;

	al = emalloc(sizeof(*al));
	al->al_next = NULL;
	al->al_this = NULL;
	return al;
}

struct attrlist *
attrlist_cons(struct attrlist *next, struct attr *a)
{
	struct attrlist *al;

	al = attrlist_create();
	al->al_next = next;
	al->al_this = a;
	return al;
}

void
attrlist_destroy(struct attrlist *al)
{
	assert(al->al_next == NULL);
	assert(al->al_this == NULL);
	free(al);
}

void
attrlist_destroyall(struct attrlist *al)
{
	struct attrlist *next;

	while (al != NULL) {
		next = al->al_next;
		al->al_next = NULL;
		/* XXX should we make the caller guarantee this? */
		al->al_this = NULL;
		attrlist_destroy(al);
		al = next;
	}
}

/*
 * Condition expressions
 */

/*
 * Create an expression node.
 */
struct condexpr *
condexpr_create(enum condexpr_types type)
{
	struct condexpr *cx;

	cx = emalloc(sizeof(*cx));
	cx->cx_type = type;
	switch (type) {

	    case CX_ATOM:
		cx->cx_atom = NULL;
		break;

	    case CX_NOT:
		cx->cx_not = NULL;
		break;

	    case CX_AND:
		cx->cx_and.left = NULL;
		cx->cx_and.right = NULL;
		break;

	    case CX_OR:
		cx->cx_or.left = NULL;
		cx->cx_or.right = NULL;
		break;
	
	    default:
		panic("condexpr_create: invalid expr type %d", (int)type);
	}
	return cx;
}

/*
 * Free an expression tree.
 */
void
condexpr_destroy(struct condexpr *expr)
{
	switch (expr->cx_type) {

	    case CX_ATOM:
		/* nothing */
		break;

	    case CX_NOT:
		condexpr_destroy(expr->cx_not);
		break;

	    case CX_AND:
		condexpr_destroy(expr->cx_and.left);
		condexpr_destroy(expr->cx_and.right);
		break;

	    case CX_OR:
		condexpr_destroy(expr->cx_or.left);
		condexpr_destroy(expr->cx_or.right);
		break;

	    default:
		panic("condexpr_destroy: invalid expr type %d",
		      (int)expr->cx_type);
	}
	free(expr);
}

/************************************************************/

/*
 * Diagnostic messages
 */

void
cfgdbg(const char *fmt, ...)
{
	va_list ap;
	extern const char *yyfile;

	va_start(ap, fmt);
	cfgvxdbg(yyfile, currentline(), fmt, ap);
	va_end(ap);
}

void
cfgwarn(const char *fmt, ...)
{
	va_list ap;
	extern const char *yyfile;

	va_start(ap, fmt);
	cfgvxwarn(yyfile, currentline(), fmt, ap);
	va_end(ap);
}

void
cfgxwarn(const char *file, int line, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	cfgvxwarn(file, line, fmt, ap);
	va_end(ap);
}

static void
cfgvxdbg(const char *file, int line, const char *fmt, va_list ap)
{
	cfgvxmsg(file, line, "debug: ", fmt, ap);
}

static void
cfgvxwarn(const char *file, int line, const char *fmt, va_list ap)
{
	cfgvxmsg(file, line, "warning: ", fmt, ap);
}

/*
 * External (config file) error.  Complain, using current file
 * and line number.
 */
void
cfgerror(const char *fmt, ...)
{
	va_list ap;
	extern const char *yyfile;

	va_start(ap, fmt);
	cfgvxerror(yyfile, currentline(), fmt, ap);
	va_end(ap);
}

/*
 * Delayed config file error (i.e., something was wrong but we could not
 * find out about it until later).
 */
void
cfgxerror(const char *file, int line, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	cfgvxerror(file, line, fmt, ap);
	va_end(ap);
}

/*
 * Internal form of error() and xerror().
 */
static void
cfgvxerror(const char *file, int line, const char *fmt, va_list ap)
{
	cfgvxmsg(file, line, "", fmt, ap);
	errors++;
}


/*
 * Internal error, abort.
 */
__dead void
panic(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void)fprintf(stderr, "%s: panic: ", getprogname());
	(void)vfprintf(stderr, fmt, ap);
	(void)putc('\n', stderr);
	va_end(ap);
	exit(2);
}

/*
 * Internal form of error() and xerror().
 */
static void
cfgvxmsg(const char *file, int line, const char *msgclass, const char *fmt,
      va_list ap)
{

	(void)fprintf(stderr, "%s:%d: %s", file, line, msgclass);
	(void)vfprintf(stderr, fmt, ap);
	(void)putc('\n', stderr);
}

void
autogen_comment(FILE *fp, const char *targetfile)
{

	(void)fprintf(fp,
	    "/*\n"
	    " * MACHINE GENERATED: DO NOT EDIT\n"
	    " *\n"
	    " * %s, from \"%s\"\n"
	    " */\n\n",
	    targetfile, conffile);
}

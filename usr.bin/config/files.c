/*	$NetBSD: files.c,v 1.35 2015/09/04 21:32:54 uebayasi Exp $	*/

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
 *	from: @(#)files.c	8.1 (Berkeley) 6/6/93
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
__RCSID("$NetBSD: files.c,v 1.35 2015/09/04 21:32:54 uebayasi Exp $");

#include <sys/param.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>
#include "defs.h"

extern const char *yyfile;

int nallfiles;
size_t nselfiles;
struct files **selfiles;

/*
 * We check that each full path name is unique.  File base names
 * should generally also be unique, e.g., having both a net/xx.c and
 * a kern/xx.c (or, worse, a net/xx.c and a new/xx.c++) is probably
 * wrong, but is permitted under some conditions.
 */
static struct hashtab *basetab;		/* file base names */
static struct hashtab *pathtab;		/* full path names */

static struct files **unchecked;

static void	addfiletoattr(const char *, struct files *);
static int	checkaux(const char *, void *);
static int	fixcount(const char *, void *);
static int	fixfsel(const char *, void *);
static int	fixsel(const char *, void *);

void
initfiles(void)
{

	basetab = ht_new();
	pathtab = ht_new();
	TAILQ_INIT(&allfiles);
	TAILQ_INIT(&allcfiles);
	TAILQ_INIT(&allsfiles);
	TAILQ_INIT(&allofiles);
	unchecked = &TAILQ_FIRST(&allfiles);
}

void
addfile(const char *path, struct condexpr *optx, u_char flags, const char *rule)
{
	struct files *fi;
	const char *dotp, *tail;
	size_t baselen;
	size_t dirlen;
	int needc, needf;
	char base[200];
	char dir[MAXPATHLEN];

	/* check various errors */
	needc = flags & FI_NEEDSCOUNT;
	needf = flags & FI_NEEDSFLAG;
	if (needc && needf) {
		cfgerror("cannot mix needs-count and needs-flag");
		goto bad;
	}
	if (optx == NULL && (needc || needf)) {
		cfgerror("nothing to %s for %s", needc ? "count" : "flag",
		    path);
		goto bad;
	}
	if (*path == '/') {
		cfgerror("path must be relative");
		goto bad;
	}

	/* find last part of pathname, and same without trailing suffix */
	tail = strrchr(path, '/');
	if (tail == NULL) {
		dirlen = 0;
		tail = path;
	} else {
		dirlen = (size_t)(tail - path);
		tail++;
	}
	memcpy(dir, path, dirlen);
	dir[dirlen] = '\0';

	dotp = strrchr(tail, '.');
	if (dotp == NULL || dotp[1] == 0 ||
	    (baselen = (size_t)(dotp - tail)) >= sizeof(base)) {
		cfgerror("invalid pathname `%s'", path);
		goto bad;
	}

	/*
	 * Commit this file to memory.  We will decide later whether it
	 * will be used after all.
	 */
	fi = ecalloc(1, sizeof *fi);
	if (ht_insert(pathtab, path, fi)) {
		free(fi);
		if ((fi = ht_lookup(pathtab, path)) == NULL)
			panic("addfile: ht_lookup(%s)", path);

		/*
		 * If it's a duplicate entry, it is must specify a make
		 * rule, and only a make rule, and must come from
		 * a different source file than the original entry.
		 * If it does otherwise, it is disallowed.  This allows
		 * machine-dependent files to override the compilation
		 * options for specific files.
		 */
		if (rule != NULL && optx == NULL && flags == 0 &&
		    yyfile != fi->fi_srcfile) {
			fi->fi_mkrule = rule;
			return;
		}
		cfgerror("duplicate file %s", path);
		cfgxerror(fi->fi_srcfile, fi->fi_srcline,
		    "here is the original definition");
		goto bad;
	}
	memcpy(base, tail, baselen);
	base[baselen] = '\0';
	fi->fi_srcfile = yyfile;
	fi->fi_srcline = currentline();
	fi->fi_flags = flags;
	fi->fi_path = path;
	fi->fi_tail = tail;
	fi->fi_base = intern(base);
	fi->fi_dir = intern(dir);
	fi->fi_prefix = SLIST_EMPTY(&prefixes) ? NULL :
			SLIST_FIRST(&prefixes)->pf_prefix;
	fi->fi_buildprefix = SLIST_EMPTY(&buildprefixes) ? NULL :
			SLIST_FIRST(&buildprefixes)->pf_prefix;
	fi->fi_len = strlen(path);
	fi->fi_suffix = path[fi->fi_len - 1];
	fi->fi_optx = optx;
	fi->fi_optf = NULL;
	fi->fi_mkrule = rule;
	fi->fi_attr = NULL;
	fi->fi_order = (int)nallfiles + (includedepth << 16);
	switch (fi->fi_suffix) {
	case 'c':
		TAILQ_INSERT_TAIL(&allcfiles, fi, fi_snext);
		TAILQ_INSERT_TAIL(&allfiles, fi, fi_next);
		break;
	case 'S':
		fi->fi_suffix = 's';
		/* FALLTHRU */
	case 's':
		TAILQ_INSERT_TAIL(&allsfiles, fi, fi_snext);
		TAILQ_INSERT_TAIL(&allfiles, fi, fi_next);
		break;
	case 'o':
		TAILQ_INSERT_TAIL(&allofiles, fi, fi_snext);
		TAILQ_INSERT_TAIL(&allfiles, fi, fi_next);
		break;
	default:
		cfgxerror(fi->fi_srcfile, fi->fi_srcline,
		    "unknown suffix");
		break;
	}
	CFGDBG(3, "file added `%s' at order score %d", fi->fi_path, fi->fi_order);
	nallfiles++;
	return;
 bad:
	if (optx != NULL) {
		condexpr_destroy(optx);
	}
}

static void
addfiletoattr(const char *name, struct files *fi)
{
	struct attr *a;

	a = ht_lookup(attrtab, name);
	if (a == NULL) {
		CFGDBG(1, "attr `%s' not found", name);
	} else {
		fi->fi_attr = a;
		TAILQ_INSERT_TAIL(&a->a_files, fi, fi_anext);
	}
}

/*
 * We have finished reading some "files" file, either ../../conf/files
 * or ./files.$machine.  Make sure that everything that is flagged as
 * needing a count is reasonable.  (This prevents ../../conf/files from
 * depending on some machine-specific device.)
 */
void
checkfiles(void)
{
	struct files *fi, *last;

	last = NULL;
	for (fi = *unchecked; fi != NULL;
	    last = fi, fi = TAILQ_NEXT(fi, fi_next)) {
		if ((fi->fi_flags & FI_NEEDSCOUNT) != 0)
			(void)expr_eval(fi->fi_optx, checkaux, fi);
	}
	if (last != NULL)
		unchecked = &TAILQ_NEXT(last, fi_next);
}

/*
 * Auxiliary function for checkfiles, called from expr_eval.
 * We are not actually interested in the expression's value.
 */
static int
checkaux(const char *name, void *context)
{
	struct files *fi = context;

	if (ht_lookup(devbasetab, name) == NULL) {
		cfgxerror(fi->fi_srcfile, fi->fi_srcline,
		    "`%s' is not a countable device",
		    name);
		/* keep fixfiles() from complaining again */
		fi->fi_flags |= FI_HIDDEN;
	}
	return (0);
}

static int
cmpfiles(const void *a, const void *b)
{
	const struct files * const *fia = a, * const *fib = b;
	int sa = (*fia)->fi_order;
	int sb = (*fib)->fi_order;

	if (sa < sb)
		return -1;
	else if (sa > sb)
		return 1;
	else
		return 0;
}

/*
 * We have finished reading everything.  Tack the files down: calculate
 * selection and counts as needed.  Check that the object files built
 * from the selected sources do not collide.
 */
int
fixfiles(void)
{
	struct files *fi, *ofi;
	struct nvlist *flathead, **flatp;
	int err, sel;
	struct config *cf;
 	char swapname[100];

	/* Place these files at last. */
	int onallfiles = nallfiles;
	nallfiles = 1 << 30;
	addfile("devsw.c", NULL, 0, NULL);
	addfile("ioconf.c", NULL, 0, NULL);

	TAILQ_FOREACH(cf, &allcf, cf_next) {
 		(void)snprintf(swapname, sizeof(swapname), "swap%s.c",
 		    cf->cf_name);
 		addfile(intern(swapname), NULL, 0, NULL);
 	}
	nallfiles = onallfiles;

	err = 0;
	TAILQ_FOREACH(fi, &allfiles, fi_next) {

		/* Skip files that generated counted-device complaints. */
		if (fi->fi_flags & FI_HIDDEN)
			continue;

		if (fi->fi_optx != NULL) {
			if (fi->fi_optx->cx_type == CX_ATOM) {
				addfiletoattr(fi->fi_optx->cx_u.atom, fi);
			}
			flathead = NULL;
			flatp = &flathead;
			sel = expr_eval(fi->fi_optx,
			    fi->fi_flags & FI_NEEDSCOUNT ? fixcount :
			    fi->fi_flags & FI_NEEDSFLAG ? fixfsel :
			    fixsel,
			    &flatp);
			fi->fi_optf = flathead;
			if (!sel)
				continue;
		}

		/* We like this file.  Make sure it generates a unique .o. */
		if (ht_insert(basetab, fi->fi_base, fi)) {
			if ((ofi = ht_lookup(basetab, fi->fi_base)) == NULL)
				panic("fixfiles ht_lookup(%s)", fi->fi_base);
			/*
			 * If the new file comes from a different source,
			 * allow the new one to override the old one.
			 */
			if (fi->fi_path != ofi->fi_path) {
				if (ht_replace(basetab, fi->fi_base, fi) != 1)
					panic("fixfiles ht_replace(%s)",
					    fi->fi_base);
				ofi->fi_flags &= (u_char)~FI_SEL;
				ofi->fi_flags |= FI_HIDDEN;
			} else {
				cfgxerror(fi->fi_srcfile, fi->fi_srcline,
				    "object file collision on %s.o, from %s",
				    fi->fi_base, fi->fi_path);
				cfgxerror(ofi->fi_srcfile, ofi->fi_srcline,
				    "here is the previous file: %s",
				    ofi->fi_path);
				err = 1;
			}
		}
		fi->fi_flags |= FI_SEL;
		nselfiles++;
		CFGDBG(3, "file selected `%s'", fi->fi_path);

		/* Add other files to the default "netbsd" attribute. */
		if (fi->fi_attr == NULL) {
			addfiletoattr(allattr.a_name, fi);
		}
		CFGDBG(3, "file `%s' belongs to attr `%s'", fi->fi_path,
		    fi->fi_attr->a_name);
	}

	/* Order files. */
	selfiles = malloc(nselfiles * sizeof(fi));
	unsigned i = 0;
	TAILQ_FOREACH(fi, &allfiles, fi_next) {
		if ((fi->fi_flags & FI_SEL) == 0)
			continue;
		selfiles[i++] = fi;
	}
	assert(i <= nselfiles);
	nselfiles = i;
	qsort(selfiles, nselfiles, (unsigned)sizeof(fi), cmpfiles);
	return (err);
}


/*
 * We have finished reading everything.  Tack the devsws down: calculate
 * selection.
 */
int
fixdevsw(void)
{
	int error;
	struct devm *dm, *res;
	struct hashtab *fixdevmtab;
	char mstr[16];

	error = 0;
	fixdevmtab = ht_new();

	TAILQ_FOREACH(dm, &alldevms, dm_next) {
		res = ht_lookup(fixdevmtab, intern(dm->dm_name));
		if (res != NULL) {
			if (res->dm_cmajor != dm->dm_cmajor ||
			    res->dm_bmajor != dm->dm_bmajor) {
				cfgxerror(res->dm_srcfile, res->dm_srcline,
					"device-major '%s' "
					"block %d, char %d redefined"
					" at %s:%d as block %d, char %d",
					res->dm_name,
					res->dm_bmajor, res->dm_cmajor,
					dm->dm_srcfile, dm->dm_srcline,
					dm->dm_bmajor, dm->dm_cmajor);
			} else {
				cfgxerror(res->dm_srcfile, res->dm_srcline,
					"device-major '%s' "
					"(block %d, char %d) duplicated"
					" at %s:%d",
					dm->dm_name, dm->dm_bmajor,
					dm->dm_cmajor,
					dm->dm_srcfile, dm->dm_srcline);
			}
			error = 1;
			goto out;
		}
		if (ht_insert(fixdevmtab, intern(dm->dm_name), dm)) {
			panic("fixdevsw: %s char %d block %d",
			      dm->dm_name, dm->dm_cmajor, dm->dm_bmajor);
		}

		if (dm->dm_opts != NULL &&
		    !expr_eval(dm->dm_opts, fixsel, NULL))
			continue;

		if (dm->dm_cmajor != NODEVMAJOR) {
			if (ht_lookup(cdevmtab, intern(dm->dm_name)) != NULL) {
				cfgxerror(dm->dm_srcfile, dm->dm_srcline,
				       "device-major of character device '%s' "
				       "is already defined", dm->dm_name);
				error = 1;
				goto out;
			}
			(void)snprintf(mstr, sizeof(mstr), "%d", dm->dm_cmajor);
			if (ht_lookup(cdevmtab, intern(mstr)) != NULL) {
				cfgxerror(dm->dm_srcfile, dm->dm_srcline,
				       "device-major of character major '%d' "
				       "is already defined", dm->dm_cmajor);
				error = 1;
				goto out;
			}
			if (ht_insert(cdevmtab, intern(dm->dm_name), dm) ||
			    ht_insert(cdevmtab, intern(mstr), dm)) {
				panic("fixdevsw: %s character major %d",
				      dm->dm_name, dm->dm_cmajor);
			}
		}
		if (dm->dm_bmajor != NODEVMAJOR) {
			if (ht_lookup(bdevmtab, intern(dm->dm_name)) != NULL) {
				cfgxerror(dm->dm_srcfile, dm->dm_srcline,
				       "device-major of block device '%s' "
				       "is already defined", dm->dm_name);
				error = 1;
				goto out;
			}
			(void)snprintf(mstr, sizeof(mstr), "%d", dm->dm_bmajor);
			if (ht_lookup(bdevmtab, intern(mstr)) != NULL) {
				cfgxerror(dm->dm_srcfile, dm->dm_srcline,
				       "device-major of block major '%d' "
				       "is already defined", dm->dm_bmajor);
				error = 1;
				goto out;
			}
			if (ht_insert(bdevmtab, intern(dm->dm_name), dm) || 
			    ht_insert(bdevmtab, intern(mstr), dm)) {
				panic("fixdevsw: %s block major %d",
				      dm->dm_name, dm->dm_bmajor);
			}
		}
	}

out:
	ht_free(fixdevmtab);
	return (error);
}

/*
 * Called when evaluating a needs-count expression.  Make sure the
 * atom is a countable device.  The expression succeeds iff there
 * is at least one of them (note that while `xx*' will not always
 * set xx's d_umax > 0, you cannot mix '*' and needs-count).  The
 * mkheaders() routine wants a flattened, in-order list of the
 * atoms for `#define name value' lines, so we build that as we
 * are called to eval each atom.
 */
static int
fixcount(const char *name, void *context)
{
	struct nvlist ***p = context;
	struct devbase *dev;
	struct nvlist *nv;

	dev = ht_lookup(devbasetab, name);
	if (dev == NULL)	/* cannot occur here; we checked earlier */
		panic("fixcount(%s)", name);
	nv = newnv(name, NULL, NULL, dev->d_umax, NULL);
	**p = nv;
	*p = &nv->nv_next;
	(void)ht_insert(needcnttab, name, nv);
	return (dev->d_umax != 0);
}

/*
 * Called from fixfiles when eval'ing a selection expression for a
 * file that will generate a .h with flags.  We will need the flat list.
 */
static int
fixfsel(const char *name, void *context)
{
	struct nvlist ***p = context;
	struct nvlist *nv;
	int sel;

	sel = ht_lookup(selecttab, name) != NULL;
	nv = newnv(name, NULL, NULL, sel, NULL);
	**p = nv;
	*p = &nv->nv_next;
	return (sel);
}

/*
 * As for fixfsel above, but we do not need the flat list.
 */
static int
/*ARGSUSED*/
fixsel(const char *name, void *context)
{

	return (ht_lookup(selecttab, name) != NULL);
}

/*
 * Eval an expression tree.  Calls the given function on each node,
 * passing it the given context & the name; return value is &/|/! of
 * results of evaluating atoms.
 *
 * No short circuiting ever occurs.  fn must return 0 or 1 (otherwise
 * our mixing of C's bitwise & boolean here may give surprises).
 */
int
expr_eval(struct condexpr *expr, int (*fn)(const char *, void *), void *ctx)
{
	int lhs, rhs;

	switch (expr->cx_type) {

	case CX_ATOM:
		return ((*fn)(expr->cx_atom, ctx));

	case CX_NOT:
		return (!expr_eval(expr->cx_not, fn, ctx));

	case CX_AND:
		lhs = expr_eval(expr->cx_and.left, fn, ctx);
		rhs = expr_eval(expr->cx_and.right, fn, ctx);
		return (lhs & rhs);

	case CX_OR:
		lhs = expr_eval(expr->cx_or.left, fn, ctx);
		rhs = expr_eval(expr->cx_or.right, fn, ctx);
		return (lhs | rhs);
	}
	panic("invalid condexpr type %d", (int)expr->cx_type);
	/* NOTREACHED */
	return (0);
}

#ifdef DEBUG
/*
 * Print expression tree.
 */
void
prexpr(struct nvlist *expr)
{
	static void pr0();

	printf("expr =");
	pr0(expr);
	printf("\n");
	(void)fflush(stdout);
}

static void
pr0(struct nvlist *e)
{

	switch (e->nv_num) {
	case FX_ATOM:
		printf(" %s", e->nv_name);
		return;
	case FX_NOT:
		printf(" (!");
		break;
	case FX_AND:
		printf(" (&");
		break;
	case FX_OR:
		printf(" (|");
		break;
	default:
		printf(" (?%lld?", e->nv_num);
		break;
	}
	if (e->nv_ptr)
		pr0(e->nv_ptr);
	pr0(e->nv_next);
	printf(")");
}
#endif

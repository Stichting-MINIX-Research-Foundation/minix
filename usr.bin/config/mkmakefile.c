/*	$NetBSD: mkmakefile.c,v 1.68 2015/09/04 10:16:35 uebayasi Exp $	*/

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
 *	from: @(#)mkmakefile.c	8.1 (Berkeley) 6/6/93
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
__RCSID("$NetBSD: mkmakefile.c,v 1.68 2015/09/04 10:16:35 uebayasi Exp $");

#include <sys/param.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <util.h>
#include "defs.h"
#include "sem.h"

/*
 * Make the Makefile.
 */

static void emitdefs(FILE *);
static void emitallfiles(FILE *);

static void emitofiles(FILE *);
static void emitallkobjs(FILE *);
static int emitallkobjscb(const char *, void *, void *);
static void emitattrkobjs(FILE *);
static int emitattrkobjscb(const char *, void *, void *);
static void emitkobjs(FILE *);
static void emitcfiles(FILE *);
static void emitsfiles(FILE *);
static void emitrules(FILE *);
static void emitload(FILE *);
static void emitincludes(FILE *);
static void emitappmkoptions(FILE *);
static void emitsubs(FILE *, const char *, const char *, int);
static int  selectopt(const char *, void *);

int has_build_kernel;

int
mkmakefile(void)
{
	FILE *ifp, *ofp;
	int lineno;
	void (*fn)(FILE *);
	char line[BUFSIZ], ifname[200];

	/*
	 * Check if conf/Makefile.kern.inc defines "build_kernel".
	 *
	 * (This is usually done by checking "version" in sys/conf/files;
	 * unfortunately the "build_kernel" change done around 2014 Aug didn't
	 * bump that version.  Thus this hack.)
	 */
	(void)snprintf(ifname, sizeof(ifname), "%s/conf/Makefile.kern.inc",
	    srcdir);
	if ((ifp = fopen(ifname, "r")) == NULL) {
		warn("cannot read %s", ifname);
		goto bad2;
	}
	while (fgets(line, sizeof(line), ifp) != NULL) {
		if (strncmp(line, "build_kernel:", 13) == 0) {
			has_build_kernel = 1;
			break;
		}
	}
	(void)fclose(ifp);

	/*
	 * Try a makefile for the port first.
	 */
	(void)snprintf(ifname, sizeof(ifname), "%s/arch/%s/conf/Makefile.%s",
	    srcdir, machine, machine);
	if ((ifp = fopen(ifname, "r")) == NULL) {
		/*
		 * Try a makefile for the architecture second.
		 */
		(void)snprintf(ifname, sizeof(ifname),
		    "%s/arch/%s/conf/Makefile.%s",
		    srcdir, machinearch, machinearch);
		ifp = fopen(ifname, "r");
	}
	if (ifp == NULL) {
		warn("cannot read %s", ifname);
		goto bad2;
	}

	if ((ofp = fopen("Makefile.tmp", "w")) == NULL) {
		warn("cannot write Makefile");
		goto bad1;
	}

	emitdefs(ofp);

	lineno = 0;
	while (fgets(line, sizeof(line), ifp) != NULL) {
		lineno++;
		if ((version < 20090214 && line[0] != '%') || line[0] == '#') {
			fputs(line, ofp);
			continue;
		}
		if (strcmp(line, "%OBJS\n") == 0)
			fn = Mflag ? emitkobjs : emitofiles;
		else if (strcmp(line, "%CFILES\n") == 0)
			fn = emitcfiles;
		else if (strcmp(line, "%SFILES\n") == 0)
			fn = emitsfiles;
		else if (strcmp(line, "%RULES\n") == 0)
			fn = emitrules;
		else if (strcmp(line, "%LOAD\n") == 0)
			fn = emitload;
		else if (strcmp(line, "%INCLUDES\n") == 0)
			fn = emitincludes;
		else if (strcmp(line, "%MAKEOPTIONSAPPEND\n") == 0)
			fn = emitappmkoptions;
		else if (strncmp(line, "%VERSION ", sizeof("%VERSION ")-1) == 0) {
			int newvers;
			if (sscanf(line, "%%VERSION %d\n", &newvers) != 1) {
				cfgxerror(ifname, lineno, "syntax error for "
				    "%%VERSION");
			} else
				setversion(newvers);
			continue;
		} else {
			if (version < 20090214)
				cfgxerror(ifname, lineno,
				    "unknown %% construct ignored: %s", line);
			else
				emitsubs(ofp, line, ifname, lineno);
			continue;
		}
		(*fn)(ofp);
	}

	fflush(ofp);
	if (ferror(ofp))
		goto wrerror;

	if (ferror(ifp)) {
		warn("error reading %s (at line %d)", ifname, lineno);
		goto bad;
	}

	if (fclose(ofp)) {
		ofp = NULL;
		goto wrerror;
	}
	(void)fclose(ifp);

	if (moveifchanged("Makefile.tmp", "Makefile") != 0) {
		warn("error renaming Makefile");
		goto bad2;
	}
	return (0);

 wrerror:
	warn("error writing Makefile");
 bad:
	if (ofp != NULL)
		(void)fclose(ofp);
 bad1:
	(void)fclose(ifp);
	/* (void)unlink("Makefile.tmp"); */
 bad2:
	return (1);
}

static void
emitsubs(FILE *fp, const char *line, const char *file, int lineno)
{
	char *nextpct;
	const char *optname;
	struct nvlist *option;

	while (*line != '\0') {
		if (*line != '%') {
			fputc(*line++, fp);
			continue;
		}

		line++;
		nextpct = strchr(line, '%');
		if (nextpct == NULL) {
			cfgxerror(file, lineno, "unbalanced %% or "
			    "unknown construct");
			return;
		}
		*nextpct = '\0';

		if (*line == '\0')
			fputc('%', fp);
		else {
			optname = intern(line);
			if (!DEFINED_OPTION(optname)) {
				cfgxerror(file, lineno, "unknown option %s",
				    optname);
				return;
			}

			if ((option = ht_lookup(opttab, optname)) == NULL)
				option = ht_lookup(fsopttab, optname);
			if (option != NULL)
				fputs(option->nv_str ? option->nv_str : "1",
				    fp);
			/*
			 * Otherwise it's not a selected option and we don't
			 * output anything.
			 */
		}

		line = nextpct + 1;
	}
}

static void
emitdefs(FILE *fp)
{
	struct nvlist *nv;

	fprintf(fp, "KERNEL_BUILD=%s\n", conffile);
	fputs("IDENT= \\\n", fp);
	for (nv = options; nv != NULL; nv = nv->nv_next) {

		/* Skip any options output to a header file */
		if (DEFINED_OPTION(nv->nv_name))
			continue;
		const char *s = nv->nv_str;
		fprintf(fp, "\t-D%s%s%s%s \\\n", nv->nv_name,
		    s ? "=\"" : "",
		    s ? s : "",
		    s ? "\"" : "");
	}
	putc('\n', fp);
	fprintf(fp, "MACHINE=%s\n", machine);

	const char *subdir = "";
	if (*srcdir != '/' && *srcdir != '.') {
		/*
		 * libkern and libcompat "Makefile.inc"s want relative S
		 * specification to begin with '.'.
		 */
		subdir = "./";
	}
	fprintf(fp, "S=\t%s%s\n", subdir, srcdir);
	if (Sflag) {
		fprintf(fp, ".PATH: $S\n");
		fprintf(fp, "___USE_SUFFIX_RULES___=1\n");
	}
	for (nv = mkoptions; nv != NULL; nv = nv->nv_next)
		fprintf(fp, "%s=%s\n", nv->nv_name, nv->nv_str);
}

static void
emitfile(FILE *fp, struct files *fi)
{
	const char *defprologue = "$S/";
	const char *prologue, *prefix, *sep;

	if (Sflag)
		defprologue = "";
	prologue = prefix = sep = "";
	if (*fi->fi_path != '/') {
		prologue = defprologue;
		if (fi->fi_prefix != NULL) {
			if (*fi->fi_prefix == '/')
				prologue = "";
			prefix = fi->fi_prefix;
			sep = "/";
		}
	}
	fprintf(fp, "%s%s%s%s", prologue, prefix, sep, fi->fi_path);
}

static void
emitfilerel(FILE *fp, struct files *fi)
{
	const char *prefix, *sep;

	prefix = sep = "";
	if (*fi->fi_path != '/') {
		if (fi->fi_prefix != NULL) {
			prefix = fi->fi_prefix;
			sep = "/";
		}
	}
	fprintf(fp, "%s%s%s", prefix, sep, fi->fi_path);
}

static void
emitofiles(FILE *fp)
{

	emitallfiles(fp);
	fprintf(fp, "#%%OFILES\n");
}

static void
emitkobjs(FILE *fp)
{
	emitallkobjs(fp);
	emitattrkobjs(fp);
}

static int emitallkobjsweighcb(const char *name, void *v, void *arg);
static void weighattr(struct attr *a);
static int attrcmp(const void *l, const void *r);

struct attr **attrbuf;
size_t attridx;

static void
emitallkobjs(FILE *fp)
{
	size_t i;

	attrbuf = emalloc(nattrs * sizeof(*attrbuf));

	ht_enumerate(attrtab, emitallkobjsweighcb, NULL);
	ht_enumerate(attrtab, emitallkobjscb, NULL);
	qsort(attrbuf, attridx, sizeof(struct attr *), attrcmp);

	fputs("OBJS= \\\n", fp);
	for (i = 0; i < attridx; i++)
		fprintf(fp, "\t%s.ko \\\n", attrbuf[i]->a_name);
	putc('\n', fp);

	free(attrbuf);
}

static int
emitallkobjscb(const char *name, void *v, void *arg)
{
	struct attr *a = v;

	if (ht_lookup(selecttab, name) == NULL)
		return 0;
	if (TAILQ_EMPTY(&a->a_files))
		return 0;
	attrbuf[attridx++] = a;
	/* XXX nattrs tracking is not exact yet */
	if (attridx == nattrs) {
		nattrs *= 2;
		attrbuf = erealloc(attrbuf, nattrs * sizeof(*attrbuf));
	}
	return 0;
}

static int
emitallkobjsweighcb(const char *name, void *v, void *arg)
{
	struct attr *a = v;

	weighattr(a);
	return 0;
}

static void
weighattr(struct attr *a)
{
	struct attrlist *al;

	for (al = a->a_deps; al != NULL; al = al->al_next) {
		weighattr(al->al_this);
	}
	a->a_weight++;
}

static int
attrcmp(const void *l, const void *r)
{
	const struct attr * const *a = l, * const *b = r;
	const int wa = (*a)->a_weight, wb = (*b)->a_weight;
	return (wa > wb) ? -1 : (wa < wb) ? 1 : 0;
}

static void
emitattrkobjs(FILE *fp)
{
	extern struct	hashtab *attrtab;

	ht_enumerate(attrtab, emitattrkobjscb, fp);
}

static int
emitattrkobjscb(const char *name, void *v, void *arg)
{
	struct attr *a = v;
	struct files *fi;
	FILE *fp = arg;

	if (ht_lookup(selecttab, name) == NULL)
		return 0;
	if (TAILQ_EMPTY(&a->a_files))
		return 0;
	fputc('\n', fp);
	fprintf(fp, "# %s (%d)\n", name, a->a_weight);
	fprintf(fp, "OBJS.%s= \\\n", name);
	TAILQ_FOREACH(fi, &a->a_files, fi_anext) {
		fprintf(fp, "\t%s.o \\\n", fi->fi_base);
	}
	fputc('\n', fp);
	fprintf(fp, "%s.ko: ${OBJS.%s}\n", name, name);
	fprintf(fp, "\t${LINK_O}\n");
	return 0;
}

static void
emitcfiles(FILE *fp)
{

	emitallfiles(fp);
	fprintf(fp, "#%%CFILES\n");
}

static void
emitsfiles(FILE *fp)
{

	emitallfiles(fp);
	fprintf(fp, "#%%SFILES\n");
}

static void
emitallfiles(FILE *fp)
{
	struct files *fi;
	static int called;
	int i;
	int found = 0;

	if (called++ != 0)
		return;
	for (i = 0; i < (int)nselfiles; i++) {
		fi = selfiles[i];
		if (found++ == 0)
			fprintf(fp, "ALLFILES= \\\n");
		putc('\t', fp);
		emitfilerel(fp, fi);
		fputs(" \\\n", fp);
	}
	fputc('\n', fp);
}

/*
 * Emit the make-rules.
 */
static void
emitrules(FILE *fp)
{
	struct files *fi;
	int i;
	int found = 0;

	for (i = 0; i < (int)nselfiles; i++) {
		fi = selfiles[i];
		if (fi->fi_mkrule == NULL)
			continue;
		fprintf(fp, "%s.o: ", fi->fi_base);
		emitfile(fp, fi);
		putc('\n', fp);
		fprintf(fp, "\t%s\n\n", fi->fi_mkrule);
		found++;
	}
	if (found == 0)
		fprintf(fp, "#%%RULES\n");
}

/*
 * Emit the load commands.
 *
 * This function is not to be called `spurt'.
 */
static void
emitload(FILE *fp)
{
	struct config *cf;
	int found = 0;

	/*
	 * Generate the backward-compatible "build_kernel" rule if
	 * sys/conf/Makefile.kern.inc doesn't define any (pre-2014 Aug).
	 */
	if (has_build_kernel == 0) {
		fprintf(fp, "build_kernel: .USE\n"
		    "\t${SYSTEM_LD_HEAD}\n"
		    "\t${SYSTEM_LD}%s\n"
		    "\t${SYSTEM_LD_TAIL}\n"
		    "\n",
		    Sflag ? "" : " swap${.TARGET}.o");
	}
	/*
	 * Generate per-kernel rules.
	 */
	TAILQ_FOREACH(cf, &allcf, cf_next) {
		char swapobj[100];

		if (Sflag) {
			swapobj[0] = '\0';
		} else {
			(void)snprintf(swapobj, sizeof(swapobj), " swap%s.o",
	 		    cf->cf_name);
		}
		fprintf(fp, "KERNELS+=%s\n", cf->cf_name);
		found = 1;
	}
	if (found == 0)
		fprintf(fp, "#%%LOAD\n");
}

/*
 * Emit include headers (for any prefixes encountered)
 */
static void
emitincludes(FILE *fp)
{
	struct prefix *pf;

	SLIST_FOREACH(pf, &allprefixes, pf_next) {
		const char *prologue = (*pf->pf_prefix == '/') ? "" : "$S/";

		fprintf(fp, "EXTRA_INCLUDES+=\t-I%s%s\n",
		    prologue, pf->pf_prefix);
	}
}

/*
 * Emit appending makeoptions.
 */
static void
emitappmkoptions(FILE *fp)
{
	struct nvlist *nv;
	struct condexpr *cond;

	for (nv = appmkoptions; nv != NULL; nv = nv->nv_next)
		fprintf(fp, "%s+=%s\n", nv->nv_name, nv->nv_str);

	for (nv = condmkoptions; nv != NULL; nv = nv->nv_next) {
		cond = nv->nv_ptr;
		if (expr_eval(cond, selectopt, NULL))
			fprintf(fp, "%s+=%s\n", nv->nv_name, nv->nv_str);
		condexpr_destroy(cond);
		nv->nv_ptr = NULL;
	}
}

static int
/*ARGSUSED*/
selectopt(const char *name, void *context)
{

	return (ht_lookup(selecttab, strtolower(name)) != NULL);
}

/*	$NetBSD: mkheaders.c,v 1.29 2015/09/03 13:53:36 uebayasi Exp $	*/

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
 *	from: @(#)mkheaders.c	8.1 (Berkeley) 6/6/93
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
__RCSID("$NetBSD: mkheaders.c,v 1.29 2015/09/03 13:53:36 uebayasi Exp $");

#include <sys/param.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <util.h>
#include <err.h>
#include "defs.h"

#include <crc_extern.h>

static int emitcnt(struct nvlist *);
static int emitopts(void);
static int emittime(void);
static int herr(const char *, const char *, FILE *);
static int defopts_print(const char *, struct defoptlist *, void *);
static char *cntname(const char *);

/*
 * We define a global symbol with the name of each option and its value.
 * This should stop code compiled with different options being linked together.
 */

/* Unlikely constant for undefined options */
#define UNDEFINED ('n' << 24 | 0 << 20 | 't' << 12 | 0xdefU)
/* Value for defined options with value UNDEFINED */
#define	DEFINED (0xdef1U << 16 | 'n' << 8 | 0xed)

/*
 * Make the various config-generated header files.
 */
int
mkheaders(void)
{
	struct files *fi;

	/*
	 * Make headers containing counts, as needed.
	 */
	TAILQ_FOREACH(fi, &allfiles, fi_next) {
		if (fi->fi_flags & FI_HIDDEN)
			continue;
		if (fi->fi_flags & (FI_NEEDSCOUNT | FI_NEEDSFLAG) &&
		    emitcnt(fi->fi_optf))
			return (1);
	}

	if (emitopts() || emitlocs() || emitioconfh())
		return (1);

	/*
	 * If the minimum required version is ever bumped beyond 20090513,
	 * emittime() can be removed.
	 */
	if (version <= 20090513 && emittime())
		return (1);

	return (0);
}

static void
fprint_global(FILE *fp, const char *name, long long value)
{
	/*
	 * We have to doubt the founding fathers here.
	 * The gas syntax for hppa is 'var .equ value', for all? other
	 * instruction sets it is ' .equ var,value'.  both have been used in
	 * various assemblers, but supporting a common syntax would be good.
	 * Fortunately we can use .equiv since it has a consistent syntax,
	 * but requires us to detect multiple assignments - event with the
	 * same value.
	 */
	fprintf(fp, "#ifdef _LOCORE\n"
	    " .ifndef _KERNEL_OPT_%s\n"
	    " .global _KERNEL_OPT_%s\n"
	    " .equiv _KERNEL_OPT_%s,0x%llx\n"
	    " .endif\n"
	    "#else\n"
	    "__asm(\" .ifndef _KERNEL_OPT_%s\\n"
	    " .global _KERNEL_OPT_%s\\n"
	    " .equiv _KERNEL_OPT_%s,0x%llx\\n"
	    " .endif\");\n"
	    "#endif\n",
	    name, name, name, value,
	    name, name, name, value);
}

/* Convert the option argument to a 32bit numder */
static unsigned int
global_hash(const char *str)
{
	unsigned long h;
	char *ep;

	/*
	 * If the value is a valid numeric, just use it
	 * We don't care about negative values here, we
	 * just use the value as a hash.
	 */
	h = strtoul(str, &ep, 0);
	if (*ep != 0)
		/* Otherwise shove through a 32bit CRC function */
		h = crc_buf(0, str, strlen(str));

	/* Avoid colliding with the value used for undefined options. */
	/* At least until I stop any options being set to zero */
	return (unsigned int)(h != UNDEFINED ? h : DEFINED);
}

static void
fprintcnt(FILE *fp, struct nvlist *nv)
{
	const char *name = cntname(nv->nv_name);

	fprintf(fp, "#define\t%s\t%lld\n", name, nv->nv_num);
	fprint_global(fp, name, nv->nv_num);
}

static int
emitcnt(struct nvlist *head)
{
	char nfname[BUFSIZ], tfname[BUFSIZ];
	struct nvlist *nv;
	FILE *fp;

	(void)snprintf(nfname, sizeof(nfname), "%s.h", head->nv_name);
	(void)snprintf(tfname, sizeof(tfname), "tmp_%s", nfname);

	if ((fp = fopen(tfname, "w")) == NULL)
		return (herr("open", tfname, NULL));

	for (nv = head; nv != NULL; nv = nv->nv_next)
		fprintcnt(fp, nv);

	fflush(fp);
	if (ferror(fp))
		return herr("writ", tfname, fp);

	if (fclose(fp) == EOF)
		return (herr("clos", tfname, NULL));

	return (moveifchanged(tfname, nfname));
}

/*
 * Output a string, preceded by a tab and possibly unescaping any quotes.
 * The argument will be output as is if it doesn't start with \".
 * Otherwise the first backslash in a \? sequence will be dropped.
 */
static void
fprintstr(FILE *fp, const char *str)
{

	if (strncmp(str, "\\\"", 2) != 0) {
		(void)fprintf(fp, "\t%s", str);
		return;
	}

	(void)fputc('\t', fp);
	
	for (; *str; str++) {
		switch (*str) {
		case '\\':
			if (!*++str)				/* XXX */
				str--;
			/*FALLTHROUGH*/
		default:
			(void)fputc(*str, fp);
			break;
		}
	}
}

/*
 * Callback function for walking the option file hash table.  We write out
 * the options defined for this file.
 */
static int
/*ARGSUSED*/
defopts_print(const char *name, struct defoptlist *value, void *arg)
{
	char tfname[BUFSIZ];
	struct nvlist *option;
	struct defoptlist *dl;
	const char *opt_value;
	int isfsoption;
	FILE *fp;

	(void)snprintf(tfname, sizeof(tfname), "tmp_%s", name);
	if ((fp = fopen(tfname, "w")) == NULL)
		return (herr("open", tfname, NULL));

	for (dl = value; dl != NULL; dl = dl->dl_next) {
		isfsoption = OPT_FSOPT(dl->dl_name);

		if (dl->dl_obsolete) {
			fprintf(fp, "/* %s `%s' is obsolete */\n",
			    isfsoption ? "file system" : "option",
			    dl->dl_name);
			fprint_global(fp, dl->dl_name, 0xdeadbeef);
			continue;
		}

		if (((option = ht_lookup(opttab, dl->dl_name)) == NULL &&
		    (option = ht_lookup(fsopttab, dl->dl_name)) == NULL) &&
		    (dl->dl_value == NULL)) {
			fprintf(fp, "/* %s `%s' not defined */\n",
			    isfsoption ? "file system" : "option",
			    dl->dl_name);
			fprint_global(fp, dl->dl_name, UNDEFINED);
			continue;
		}

		opt_value = option != NULL ? option->nv_str : dl->dl_value;
		if (isfsoption == 1)
			/* For filesysteme we'd output the lower case name */
			opt_value = NULL;

		fprintf(fp, "#define\t%s", dl->dl_name);
		if (opt_value != NULL)
			fprintstr(fp, opt_value);
		else if (!isfsoption)
			fprintstr(fp, "1");
		fputc('\n', fp);
		fprint_global(fp, dl->dl_name,
		    opt_value == NULL ? 1 : global_hash(opt_value));
	}

	fflush(fp);
	if (ferror(fp))
		return herr("writ", tfname, fp);

	if (fclose(fp) == EOF)
		return (herr("clos", tfname, NULL));

	return (moveifchanged(tfname, name));
}

/*
 * Emit the option header files.
 */
static int
emitopts(void)
{

	return (dlhash_enumerate(optfiletab, defopts_print, NULL));
}

/*
 * A callback function for walking the attribute hash table.
 * Emit CPP definitions of manifest constants for the locators on the
 * "name" attribute node (passed as the "value" parameter).
 */
static int
locators_print(const char *name, void *value, void *arg)
{
	struct attr *a;
	struct loclist *ll;
	int i;
	char *locdup, *namedup;
	char *cp;
	FILE *fp = arg;

	a = value;
	if (a->a_locs) {
		if (strchr(name, ' ') != NULL || strchr(name, '\t') != NULL)
			/*
			 * name contains a space; we can't generate
			 * usable defines, so ignore it.
			 */
			return 0;
		locdup = estrdup(name);
		for (cp = locdup; *cp; cp++)
			if (islower((unsigned char)*cp))
				*cp = (char)toupper((unsigned char)*cp);
		for (i = 0, ll = a->a_locs; ll; ll = ll->ll_next, i++) {
			if (strchr(ll->ll_name, ' ') != NULL ||
			    strchr(ll->ll_name, '\t') != NULL)
				/*
				 * name contains a space; we can't generate
				 * usable defines, so ignore it.
				 */
				continue;
			namedup = estrdup(ll->ll_name);
			for (cp = namedup; *cp; cp++)
				if (islower((unsigned char)*cp))
					*cp = (char)toupper((unsigned char)*cp);
				else if (*cp == ARRCHR)
					*cp = '_';
			fprintf(fp, "#define %sCF_%s %d\n", locdup, namedup, i);
			if (ll->ll_string != NULL)
				fprintf(fp, "#define %sCF_%s_DEFAULT %s\n",
				    locdup, namedup, ll->ll_string);
			free(namedup);
		}
		/* assert(i == a->a_loclen) */
		fprintf(fp, "#define %sCF_NLOCS %d\n", locdup, a->a_loclen);
		free(locdup);
	}
	return 0;
}

/*
 * Build the "locators.h" file with manifest constants for all potential
 * locators in the configuration.  Do this by enumerating the attribute
 * hash table and emitting all the locators for each attribute.
 */
int
emitlocs(void)
{
	const char *tfname;
	int rval;
	FILE *tfp;
	
	tfname = "tmp_locators.h";
	if ((tfp = fopen(tfname, "w")) == NULL)
		return (herr("open", tfname, NULL));

	rval = ht_enumerate(attrtab, locators_print, tfp);

	fflush(tfp);
	if (ferror(tfp))
		return (herr("writ", tfname, NULL));
	if (fclose(tfp) == EOF)
		return (herr("clos", tfname, NULL));
	if (rval)
		return (rval);
	return (moveifchanged(tfname, "locators.h"));
}

/*
 * Build the "ioconf.h" file with extern declarations for all configured
 * cfdrivers.
 */
int
emitioconfh(void)
{
	const char *tfname;
	FILE *tfp;
	struct devbase *d;
	struct devi *i;

	tfname = "tmp_ioconf.h";
	if ((tfp = fopen(tfname, "w")) == NULL)
		return (herr("open", tfname, NULL));

        fputs("\n/* pseudo-devices */\n", tfp);
        TAILQ_FOREACH(i, &allpseudo, i_next) {
                fprintf(tfp, "void %sattach(int);\n",
                    i->i_base->d_name);
        }

        fputs("\n/* driver structs */\n", tfp);
	TAILQ_FOREACH(d, &allbases, d_next) {
		if (!devbase_has_instances(d, WILD))
			continue;
		fprintf(tfp, "extern struct cfdriver %s_cd;\n", d->d_name);
	}

	fflush(tfp);
	if (ferror(tfp))
		return herr("writ", tfname, tfp);

	if (fclose(tfp) == EOF)
		return (herr("clos", tfname, NULL));

	return (moveifchanged(tfname, "ioconf.h"));
}

/*
 * Make a file that config_time.h can use as a source, if required.
 */
static int
emittime(void)
{
	FILE *fp;
	time_t t;
	struct tm *tm;
	char buf[128];

	t = time(NULL);
	tm = gmtime(&t);

	if ((fp = fopen("config_time.src", "w")) == NULL)
		return (herr("open", "config_time.src", NULL));

	if (strftime(buf, sizeof(buf), "%c %Z", tm) == 0)
		return (herr("strftime", "config_time.src", fp));

	fprintf(fp, "/* %s */\n"
	    "#define CONFIG_TIME\t%2lld\n"
	    "#define CONFIG_YEAR\t%2d\n"
	    "#define CONFIG_MONTH\t%2d\n"
	    "#define CONFIG_DATE\t%2d\n"
	    "#define CONFIG_HOUR\t%2d\n"
	    "#define CONFIG_MINS\t%2d\n"		
	    "#define CONFIG_SECS\t%2d\n",
	    buf, (long long)t, 
	    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
	    tm->tm_hour, tm->tm_min, tm->tm_sec);

	fflush(fp);
	if (ferror(fp))
		return (herr("fprintf", "config_time.src", fp));

	if (fclose(fp) != 0)
		return (herr("clos", "config_time.src", NULL));

	/*
	 * *Don't* moveifchanged this file.  Makefile.kern.inc will
	 * handle that if it determines such a move is necessary.
	 */
	return (0);
}

/*
 * Compare two files.  If nfname doesn't exist, or is different from
 * tfname, move tfname to nfname.  Otherwise, delete tfname.
 */
int
moveifchanged(const char *tfname, const char *nfname)
{
	char tbuf[BUFSIZ], nbuf[BUFSIZ];
	FILE *tfp, *nfp;

	if ((tfp = fopen(tfname, "r")) == NULL)
		return (herr("open", tfname, NULL));

	if ((nfp = fopen(nfname, "r")) == NULL)
		goto moveit;

	while (fgets(tbuf, sizeof(tbuf), tfp) != NULL) {
		if (fgets(nbuf, sizeof(nbuf), nfp) == NULL) {
			/*
			 * Old file has fewer lines.
			 */
			goto moveit;
		}
		if (strcmp(tbuf, nbuf) != 0)
			goto moveit;
	}

	/*
	 * We've reached the end of the new file.  Check to see if new file
	 * has fewer lines than old.
	 */
	if (fgets(nbuf, sizeof(nbuf), nfp) != NULL) {
		/*
		 * New file has fewer lines.
		 */
		goto moveit;
	}

	(void) fclose(nfp);
	(void) fclose(tfp);
	if (remove(tfname) == -1)
		return(herr("remov", tfname, NULL));
	return (0);

 moveit:
	/*
	 * They're different, or the file doesn't exist.
	 */
	if (nfp)
		(void) fclose(nfp);
	if (tfp)
		(void) fclose(tfp);
	if (rename(tfname, nfname) == -1)
		return (herr("renam", tfname, NULL));
	return (0);
}

static int
herr(const char *what, const char *fname, FILE *fp)
{

	warn("error %sing %s", what, fname);
	if (fp)
		(void)fclose(fp);
	return (1);
}

static char *
cntname(const char *src)
{
	char *dst;
	char c;
	static char buf[100];

	dst = buf;
	*dst++ = 'N';
	while ((c = *src++) != 0)
		*dst++ = (char)(islower((u_char)c) ? toupper((u_char)c) : c);
	*dst = 0;
	return (buf);
}

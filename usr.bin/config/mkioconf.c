/*	$NetBSD: mkioconf.c,v 1.32 2015/09/03 13:53:36 uebayasi Exp $	*/

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
 *	from: @(#)mkioconf.c	8.1 (Berkeley) 6/6/93
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
__RCSID("$NetBSD: mkioconf.c,v 1.32 2015/09/03 13:53:36 uebayasi Exp $");

#include <sys/param.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"

/*
 * Make ioconf.c.
 */
static int cf_locators_print(const char *, void *, void *);
static int cforder(const void *, const void *);
static void emitcfdata(FILE *);
static void emitcfdrivers(FILE *);
static void emitexterns(FILE *);
static void emitcfattachinit(FILE *);
static void emithdr(FILE *);
static void emitloc(FILE *);
static void emitpseudo(FILE *);
static void emitparents(FILE *);
static void emitroots(FILE *);
static void emitname2blk(FILE *);

#define	SEP(pos, max)	(((u_int)(pos) % (max)) == 0 ? "\n\t" : " ")

#define ARRNAME(n, l) (strchr((n), ARRCHR) && strncmp((n), (l), strlen((l))) == 0)

/*
 * NEWLINE can only be used in the emitXXX functions.
 * In most cases it can be subsumed into an fprintf.
 */
#define	NEWLINE		putc('\n', fp)

int
mkioconf(void)
{
	FILE *fp;

	qsort(packed, npacked, sizeof *packed, cforder);
	if ((fp = fopen("ioconf.c.tmp", "w")) == NULL) {
		warn("cannot write ioconf.c");
		return (1);
	}

	fprintf(fp, "#include \"ioconf.h\"\n");

	emithdr(fp);
	emitcfdrivers(fp);
	emitexterns(fp);
	emitloc(fp);
	emitparents(fp);
	emitcfdata(fp);
	emitcfattachinit(fp);

	if (ioconfname == NULL) {
		emitroots(fp);
		emitpseudo(fp);
		if (!do_devsw)
			emitname2blk(fp);
	}

	fflush(fp);
	if (ferror(fp)) {
		warn("error writing ioconf.c");
		(void)fclose(fp);
#if 0
		(void)unlink("ioconf.c.tmp");
#endif
		return (1);
	}

	(void)fclose(fp);
	if (moveifchanged("ioconf.c.tmp", "ioconf.c") != 0) {
		warn("error renaming ioconf.c");
		return (1);
	}
	return (0);
}

static int
cforder(const void *a, const void *b)
{
	int n1, n2;

	n1 = (*(const struct devi * const *)a)->i_cfindex;
	n2 = (*(const struct devi * const *)b)->i_cfindex;
	return (n1 - n2);
}

static void
emithdr(FILE *ofp)
{
	FILE *ifp;
	size_t n;
	char ifnbuf[200], buf[BUFSIZ];
	char *ifn;

	autogen_comment(ofp, "ioconf.c");

	(void)snprintf(ifnbuf, sizeof(ifnbuf), "%s/arch/%s/conf/ioconf.incl.%s",
	    srcdir,
	    machine ? machine : "(null)", machine ? machine : "(null)");
	ifn = ifnbuf;
	if ((ifp = fopen(ifn, "r")) != NULL) {
		while ((n = fread(buf, 1, sizeof(buf), ifp)) > 0)
			(void)fwrite(buf, 1, n, ofp);
		if (ferror(ifp))
			err(EXIT_FAILURE, "error reading %s", ifn);
		(void)fclose(ifp);
	} else {
		fputs("#include <sys/param.h>\n"
			"#include <sys/conf.h>\n"
			"#include <sys/device.h>\n"
			"#include <sys/mount.h>\n", ofp);
	}
}

/*
 * Emit an initialized array of character strings describing this
 * attribute's locators.
 */
static int
cf_locators_print(const char *name, void *value, void *arg)
{
	struct attr *a;
	struct loclist *ll;
	FILE *fp = arg;

	a = value;
	if (!a->a_iattr)
		return (0);
	if (ht_lookup(selecttab, name) == NULL)
		return (0);

	if (a->a_locs) {
		fprintf(fp,
		    "static const struct cfiattrdata %scf_iattrdata = {\n",
			    name);
		fprintf(fp, "\t\"%s\", %d, {\n", name, a->a_loclen);
		for (ll = a->a_locs; ll; ll = ll->ll_next)
			fprintf(fp, "\t\t{ \"%s\", \"%s\", %s },\n",
				ll->ll_name,
				(ll->ll_string ? ll->ll_string : "NULL"),
				(ll->ll_string ? ll->ll_string : "0"));
		fprintf(fp, "\t}\n};\n");
	} else {
		fprintf(fp,
		    "static const struct cfiattrdata %scf_iattrdata = {\n"
		    "\t\"%s\", 0, {\n\t\t{ NULL, NULL, 0 },\n\t}\n};\n",
		    name, name);
	}

	return 0;
}

static void
emitcfdrivers(FILE *fp)
{
	struct devbase *d;
	struct attrlist *al;
	struct attr *a;
	int has_iattrs;

	NEWLINE;
	ht_enumerate(attrtab, cf_locators_print, fp);

	NEWLINE;
	TAILQ_FOREACH(d, &allbases, d_next) {
		if (!devbase_has_instances(d, WILD))
			continue;
		has_iattrs = 0;
		for (al = d->d_attrs; al != NULL; al = al->al_next) {
			a = al->al_this;
			if (a->a_iattr == 0)
				continue;
			if (has_iattrs == 0)
				fprintf(fp,
			    	    "static const struct cfiattrdata * const %s_attrs[] = { ",
			    	    d->d_name);
			has_iattrs = 1;
			fprintf(fp, "&%scf_iattrdata, ", a->a_name);
		}
		if (has_iattrs)
			fprintf(fp, "NULL };\n");
		fprintf(fp, "CFDRIVER_DECL(%s, %s, ", d->d_name, /* ) */
		    d->d_classattr != NULL ? d->d_classattr->a_devclass
					   : "DV_DULL");
		if (has_iattrs)
			fprintf(fp, "%s_attrs", d->d_name);
		else
			fprintf(fp, "NULL");
		fprintf(fp, /* ( */ ");\n\n");
	}

	NEWLINE;

	fprintf(fp,
	    "%sstruct cfdriver * const cfdriver_%s_%s[] = {\n",
	    ioconfname ? "static " : "",
	    ioconfname ? "ioconf" : "list",
	    ioconfname ? ioconfname : "initial");

	TAILQ_FOREACH(d, &allbases, d_next) {
		if (!devbase_has_instances(d, WILD))
			continue;
		fprintf(fp, "\t&%s_cd,\n", d->d_name);
	}
	fprintf(fp, "\tNULL\n};\n");
}

static void
emitexterns(FILE *fp)
{
	struct deva *da;

	NEWLINE;
	TAILQ_FOREACH(da, &alldevas, d_next) {
		if (!deva_has_instances(da, WILD))
			continue;
		fprintf(fp, "extern struct cfattach %s_ca;\n",
			    da->d_name);
	}
}

static void
emitcfattachinit(FILE *fp)
{
	struct devbase *d;
	struct deva *da;

	NEWLINE;
	TAILQ_FOREACH(d, &allbases, d_next) {
		if (!devbase_has_instances(d, WILD))
			continue;
		if (d->d_ahead == NULL)
			continue;

		fprintf(fp,
		    "static struct cfattach * const %s_cfattachinit[] = {\n\t",
			    d->d_name);
		for (da = d->d_ahead; da != NULL; da = da->d_bsame) {
			if (!deva_has_instances(da, WILD))
				continue;
			fprintf(fp, "&%s_ca, ", da->d_name);
		}
		fprintf(fp, "NULL\n};\n");
	}

	NEWLINE;
	fprintf(fp, "%sconst struct cfattachinit cfattach%s%s[] = {\n",
	    ioconfname ? "static " : "",
	    ioconfname ? "_ioconf_" : "init",
	    ioconfname ? ioconfname : "");

	TAILQ_FOREACH(d, &allbases, d_next) {
		if (!devbase_has_instances(d, WILD))
			continue;
		if (d->d_ahead == NULL)
			continue;

		fprintf(fp, "\t{ \"%s\", %s_cfattachinit },\n",
			    d->d_name, d->d_name);
	}

	fprintf(fp, "\t{ NULL, NULL }\n};\n");
}

static void
emitloc(FILE *fp)
{
	int i;

	if (locators.used != 0) {
		fprintf(fp, "\n/* locators */\n"
			"static int loc[%d] = {", locators.used);
		for (i = 0; i < locators.used; i++)
			fprintf(fp, "%s%s,", SEP(i, 8), locators.vec[i]);
		fprintf(fp, "\n};\n");
	}
}

/*
 * Emit static parent data.
 */
static void
emitparents(FILE *fp)
{
	struct pspec *p;

	NEWLINE;
	TAILQ_FOREACH(p, &allpspecs, p_list) {
		if (p->p_devs == NULL || p->p_active != DEVI_ACTIVE)
			continue;
		fprintf(fp,
		    "static const struct cfparent pspec%d = {\n", p->p_inst);
		fprintf(fp, "\t\"%s\", ", p->p_iattr->a_name);
		if (p->p_atdev != NULL) {
			fprintf(fp, "\"%s\", ", p->p_atdev->d_name);
			if (p->p_atunit == WILD)
				fprintf(fp, "DVUNIT_ANY");
			else
				fprintf(fp, "%d", p->p_atunit);
		} else
			fprintf(fp, "NULL, 0");
		fprintf(fp, "\n};\n");
	}
}

/*
 * Emit the cfdata array.
 */
static void
emitcfdata(FILE *fp)
{
	struct devi **p, *i;
	struct pspec *ps;
	int unit, v;
	const char *state, *basename, *attachment;
	struct loclist *ll;
	struct attr *a;
	const char *loc;
	char locbuf[20];
	const char *lastname = "";

	fprintf(fp, "\n"
		"#define NORM FSTATE_NOTFOUND\n"
		"#define STAR FSTATE_STAR\n"
		"\n"
		"%sstruct cfdata cfdata%s%s[] = {\n"
		"    /* driver           attachment    unit state "
		"     loc   flags  pspec */\n",
		    ioconfname ? "static " : "",
		    ioconfname ? "_ioconf_" : "",
		    ioconfname ? ioconfname : "");
	for (p = packed; (i = *p) != NULL; p++) {
		/* the description */
		fprintf(fp, "/*%3d: %s at ", i->i_cfindex, i->i_name);
		if ((ps = i->i_pspec) != NULL) {
			if (ps->p_atdev != NULL &&
			    ps->p_atunit != WILD) {
				fprintf(fp, "%s%d", ps->p_atdev->d_name,
					    ps->p_atunit);
			} else if (ps->p_atdev != NULL) {
				fprintf(fp, "%s?", ps->p_atdev->d_name);
			} else {
				fprintf(fp, "%s?", ps->p_iattr->a_name);
			}

			a = ps->p_iattr;
			for (ll = a->a_locs, v = 0; ll != NULL;
			     ll = ll->ll_next, v++) {
				if (ARRNAME(ll->ll_name, lastname)) {
					fprintf(fp, " %s %s",
					    ll->ll_name, i->i_locs[v]);
				} else {
					fprintf(fp, " %s %s",
						    ll->ll_name,
						    i->i_locs[v]);
					lastname = ll->ll_name;
				}
			}
		} else {
			a = NULL;
			fputs("root", fp);
		}

		fputs(" */\n", fp);

		/* then the actual defining line */
		basename = i->i_base->d_name;
		attachment = i->i_atdeva->d_name;
		if (i->i_unit == STAR) {
			unit = i->i_base->d_umax;
			state = "STAR";
		} else {
			unit = i->i_unit;
			state = "NORM";
		}
		if (i->i_locoff >= 0) {
			(void)snprintf(locbuf, sizeof(locbuf), "loc+%3d",
			    i->i_locoff);
			loc = locbuf;
		} else
			loc = "NULL";
		fprintf(fp, "    { \"%s\",%s\"%s\",%s%2d, %s, %7s, %#6x, ",
			    basename, strlen(basename) < 7 ? "\t\t"
			    				   : "\t",
			    attachment, strlen(attachment) < 5 ? "\t\t"
			    				       : "\t",
			    unit, state, loc, i->i_cfflags);
		if (ps != NULL)
			fprintf(fp, "&pspec%d },\n", ps->p_inst);
		else
			fputs("NULL },\n", fp);
	}
	fprintf(fp, "    { %s,%s%s,%s%2d, %s, %7s, %#6x, %s }\n};\n",
	    "NULL", "\t\t", "NULL", "\t\t", 0, "   0", "NULL", 0, "NULL");
}

/*
 * Emit the table of potential roots.
 */
static void
emitroots(FILE *fp)
{
	struct devi **p, *i;

	fputs("\nconst short cfroots[] = {\n", fp);
	for (p = packed; (i = *p) != NULL; p++) {
		if (i->i_at != NULL)
			continue;
		if (i->i_unit != 0 &&
		    (i->i_unit != STAR || i->i_base->d_umax != 0))
			warnx("warning: `%s at root' is not unit 0", i->i_name);
		fprintf(fp, "\t%2d /* %s */,\n",
		    i->i_cfindex, i->i_name);
	}
	fputs("\t-1\n};\n", fp);
}

/*
 * Emit pseudo-device initialization.
 */
static void
emitpseudo(FILE *fp)
{
	struct devi *i;
	struct devbase *d;

	fputs("\n/* pseudo-devices */\n", fp);
	fputs("\nconst struct pdevinit pdevinit[] = {\n", fp);
	TAILQ_FOREACH(i, &allpseudo, i_next) {
		d = i->i_base;
		fprintf(fp, "\t{ %sattach, %d },\n",
		    d->d_name, d->d_umax);
	}
	fputs("\t{ 0, 0 }\n};\n", fp);
}

/*
 * Emit name to major block number table.
 */
void
emitname2blk(FILE *fp)
{
	struct devbase *dev;

	fputs("\n/* device name to major block number */\n", fp);

	fprintf(fp, "struct devnametobdevmaj dev_name2blk[] = {\n");

	TAILQ_FOREACH(dev, &allbases, d_next) {
		if (dev->d_major == NODEVMAJOR)
			continue;

		fprintf(fp, "\t{ \"%s\", %d },\n",
			    dev->d_name, dev->d_major);
	}
	fprintf(fp, "\t{ NULL, 0 }\n};\n");
}

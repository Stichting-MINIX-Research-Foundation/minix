/*	$NetBSD: mkdevsw.c,v 1.14 2015/09/03 13:53:36 uebayasi Exp $	*/

/*
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by MAEKAWA Masahide (gehenna@NetBSD.org).
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
__RCSID("$NetBSD: mkdevsw.c,v 1.14 2015/09/03 13:53:36 uebayasi Exp $");

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <err.h>

#include "defs.h"

static void emitconv(FILE *);
static void emitdev(FILE *);
static void emitdevm(FILE *);
static void emitheader(FILE *);

int
mkdevsw(void)
{
	FILE *fp;

	if ((fp = fopen("devsw.c.tmp", "w")) == NULL) {
		warn("cannot create devsw.c");
		return (1);
	}

	emitheader(fp);
	emitdevm(fp);
	emitconv(fp);
	emitdev(fp);

	fflush(fp);
	if (ferror(fp)) {
		warn("error writing devsw.c");
		fclose(fp);
		return 1;
	}

	(void)fclose(fp);

	if (moveifchanged("devsw.c.tmp", "devsw.c") != 0) {
		warn("error renaming devsw.c");
		return (1);
	}

	return (0);
}

static void
emitheader(FILE *fp)
{
	autogen_comment(fp, "devsw.c");

	fputs("#include <sys/param.h>\n"
		  "#include <sys/conf.h>\n", fp);
}

static void
dentry(FILE *fp, struct hashtab *t, devmajor_t i, char p)
{
	const struct devm *dm;
	char mstr[16];

	(void)snprintf(mstr, sizeof(mstr), "%d", i);
	if ((dm = ht_lookup(t, intern(mstr))) == NULL)
		return;

	fprintf(fp, "extern const struct %cdevsw %s_%cdevsw;\n",
	    p, dm->dm_name, p);
}

static void
pentry(FILE *fp, struct hashtab *t, devmajor_t i, char p)
{
	const struct devm *dm;
	char mstr[16];

	(void)snprintf(mstr, sizeof(mstr), "%d", i);
	dm = ht_lookup(t, intern(mstr));

	if (dm)
		fprintf(fp, "\t&%s_%cdevsw", dm->dm_name, p); 
	else
		fputs("\tNULL", fp);

	fprintf(fp, ",\t// %3d\n", i);
}

/*
 * Emit device switch table for character/block device.
 */
static void
emitdevm(FILE *fp)
{
	devmajor_t i;

	fputs("\n/* device switch table for block device */\n", fp);

	for (i = 0; i <= maxbdevm ; i++)
		dentry(fp, cdevmtab, i, 'b');

	fputs("\nconst struct bdevsw *bdevsw0[] = {\n", fp);

	for (i = 0; i <= maxbdevm; i++)
		pentry(fp, bdevmtab, i, 'b');

	fputs("};\n\nconst struct bdevsw **bdevsw = bdevsw0;\n", fp);

	fputs("const int sys_bdevsws = __arraycount(bdevsw0);\n"
		  "int max_bdevsws = __arraycount(bdevsw0);\n", fp);

	fputs("\n/* device switch table for character device */\n", fp);

	for (i = 0; i <= maxcdevm; i++)
		dentry(fp, cdevmtab, i, 'c');

	fputs("\nconst struct cdevsw *cdevsw0[] = {\n", fp);

	for (i = 0; i <= maxcdevm; i++)
		pentry(fp, cdevmtab, i, 'c');

	fputs("};\n\nconst struct cdevsw **cdevsw = cdevsw0;\n", fp);

	fputs("const int sys_cdevsws = __arraycount(cdevsw0);\n"
		  "int max_cdevsws = __arraycount(cdevsw0);\n", fp);
}

/*
 * Emit device major conversion table.
 */
static void
emitconv(FILE *fp)
{
	struct devm *dm;

	fputs("\n/* device conversion table */\n"
		  "struct devsw_conv devsw_conv0[] = {\n", fp);
	TAILQ_FOREACH(dm, &alldevms, dm_next) {
		if (version < 20100430) {
			/* Emit compatible structure */
			fprintf(fp, "\t{ \"%s\", %d, %d },\n", dm->dm_name,
			    dm->dm_bmajor, dm->dm_cmajor);
			continue;
		}
		struct nvlist *nv;
		const char *d_class, *d_flags = "0";
		int d_vec[2] = { 0, 0 };
		int i = 0;

		/*
		 * "parse" info.  currently the rules are simple:
		 *  1) first entry defines class
		 *  2) next ones without n_str are d_vectdim
		 *  3) next one with n_str is d_flags
		 *  4) EOL
		 */
		nv = dm->dm_devnodes;
		d_class = nv->nv_str;
		while ((nv = nv->nv_next) != NULL) {
			if (i > 2)
				panic("invalid devnode definition");
			if (nv->nv_str) {
				d_flags = nv->nv_str;
				break;
			}
			if (nv->nv_num > INT_MAX || nv->nv_num < INT_MIN)
				panic("out of range devnode definition");
			d_vec[i++] = (int)nv->nv_num;
		}

		fprintf(fp, "\t{ \"%s\", %d, %d, %s, %s, { %d, %d }},\n",
			    dm->dm_name, dm->dm_bmajor, dm->dm_cmajor,
			    d_class, d_flags, d_vec[0], d_vec[1]);

	}
	fputs("};\n\n"
		  "struct devsw_conv *devsw_conv = devsw_conv0;\n"
		  "int max_devsw_convs = __arraycount(devsw_conv0);\n",
		  fp);
}

/*
 * Emit specific device major informations.
 */
static void
emitdev(FILE *fp)
{
	struct devm *dm;
	char mstr[16];

	fputs("\n", fp);

	(void)strlcpy(mstr, "swap", sizeof(mstr));
	if ((dm = ht_lookup(bdevmtab, intern(mstr))) != NULL) {
		fprintf(fp, "const dev_t swapdev = makedev(%d, 0);\n",
			    dm->dm_bmajor);
	}

	(void)strlcpy(mstr, "mem", sizeof(mstr));
	if ((dm = ht_lookup(cdevmtab, intern(mstr))) == NULL)
		panic("memory device is not configured");
	fprintf(fp, "const dev_t zerodev = makedev(%d, DEV_ZERO);\n",
		    dm->dm_cmajor);

	fputs("\n/* mem_no is only used in iskmemdev() */\n", fp);
	fprintf(fp, "const int mem_no = %d;\n", dm->dm_cmajor);
}

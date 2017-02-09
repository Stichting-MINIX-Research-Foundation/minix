/*	$NetBSD: lint.c,v 1.15 2014/10/29 17:14:50 christos Exp $	*/

/*
 *  Copyright (c) 2007 The NetBSD Foundation.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 *  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
__RCSID("$NetBSD: lint.c,v 1.15 2014/10/29 17:14:50 christos Exp $");

#include <assert.h>
#include <stdlib.h>

#include "defs.h"

void
emit_params(void)
{

	printf("version\t%d\n", CONFIG_VERSION);
	printf("ident\t\"LINT_%s\"\n", conffile);
	printf("maxusers\t%d\n", defmaxusers);
	printf("config netbsdlint root on ?\n");
	printf("\n");
}

enum opt_types {
	OT_FLAG,
	OT_PARAM,
	OT_FS
};

static struct opt_type {
	enum opt_types ot_type;
	const char *ot_name;
	struct hashtab **ot_ht;
} opt_types[] = {
	{ OT_FLAG, "options", &opttab },
	{ OT_PARAM, "options", &opttab },
	{ OT_FS, "file-system", &fsopttab },
};

static int
do_emit_option(const char *name, struct defoptlist *dl, void *v)
{
	const struct opt_type *ot = v;
	const char *value;

	if (dl->dl_obsolete)
		return 0;

	if (ht_lookup(*(ot->ot_ht), name))
		return 0;

	printf("%s\t%s", ot->ot_name, dl->dl_name);
	if (ot->ot_type == OT_PARAM) {
		struct defoptlist *dl2 = dlhash_lookup(defoptlint, dl->dl_name);
		if (dl2 != NULL)
			value = dl2->dl_lintvalue;
		else
			value = dl->dl_value;
		assert(dl2 == dl);
		printf("=\"%s\"", value ? value : "1");
	}
	printf("\n");

	return 1;
}

/*
 * Same as do_emit_option but for filesystem definitions, which now
 * have a different data type. XXX these should probably be unified
 * again.
 */
static int
do_emit_fs(const char *name, struct nvlist *nv, void *v)
{
	const struct opt_type *ot = v;

	if (ht_lookup(*(ot->ot_ht), name))
		return 0;

	assert(ot->ot_type != OT_PARAM);
	printf("%s\t%s\n", ot->ot_name, nv->nv_name);

	return 1;
}
	

void
emit_options(void)
{

	(void)dlhash_enumerate(defflagtab, do_emit_option, &opt_types[0]);
	printf("\n");
	(void)dlhash_enumerate(defparamtab, do_emit_option, &opt_types[1]);
	printf("\n");
	(void)nvhash_enumerate(deffstab, do_emit_fs, &opt_types[2]);
	printf("\n");
}

static void
do_emit_instances(struct devbase *d, struct attr *at)
{
	struct nvlist *nv1;
	struct loclist *ll;
	struct attrlist *al;
	struct attr *a;
	struct deva *da;

	/*
	 * d_isdef is used to check whether a deva has been seen or not,
	 * for there are devices that can be their own ancestor (e.g.
	 * uhub, pci).
	 */

	if (at != NULL) {
		for (da = d->d_ahead; da != NULL; da = da->d_bsame)
			if (onlist(da->d_atlist, at))
				break;
		if (da == NULL)
			panic("do_emit_instances: no deva found for %s at %s",
			    d->d_name, at->a_name);

		if (da->d_isdef > 1)
			return;
		da->d_isdef = 2;
	}

	if (at == NULL && !d->d_ispseudo && d->d_ihead == NULL)
		printf("%s0\tat\troot\n", d->d_name);
	else if (at != NULL && !d->d_ispseudo && da->d_ihead == NULL) {
		printf("%s0\tat\t%s?", d->d_name, at->a_name);

		for (ll = at->a_locs; ll != NULL; ll = ll->ll_next) {
			if (ll->ll_num == 0)
				printf(" %s %c", ll->ll_name,
				    ll->ll_string ? '?' : '0');
		}

		printf("\n");
	}

	/*
	 * Children attachments are found the same way as in the orphan
	 * detection code in main.c.
	 */
	for (al = d->d_attrs; al != NULL; al = al->al_next) {
		a = al->al_this;
		for (nv1 = a->a_devs; nv1 != NULL; nv1 = nv1->nv_next)
			do_emit_instances(nv1->nv_ptr, a);
	}
}

/* ARGSUSED */
static int
emit_root_instance(const char *name, void *value, void *v)
{

	do_emit_instances((struct devbase *)value, NULL);

	return 1;
}

/* ARGSUSED */
static int
emit_pseudo_instance(const char *name, void *value, void *v)
{
	struct devbase *d = value;

	if (d->d_ispseudo && d->d_ihead == NULL)
		printf("pseudo-device\t%s\n", d->d_name);
	return 0;
}

void
emit_instances(void)
{

	(void)ht_enumerate(devroottab, emit_root_instance, NULL);
	printf("\n");
	(void)ht_enumerate(devbasetab, emit_pseudo_instance, NULL);
}

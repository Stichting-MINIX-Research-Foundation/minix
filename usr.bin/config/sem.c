/*	$NetBSD: sem.c,v 1.73 2015/08/29 07:24:49 uebayasi Exp $	*/

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
 *	from: @(#)sem.c	8.1 (Berkeley) 6/6/93
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
__RCSID("$NetBSD: sem.c,v 1.73 2015/08/29 07:24:49 uebayasi Exp $");

#include <sys/param.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>
#include "defs.h"
#include "sem.h"

/*
 * config semantics.
 */

#define	NAMESIZE	100	/* local name buffers */

const char *s_ifnet;		/* magic attribute */
const char *s_qmark;
const char *s_none;

static struct hashtab *cfhashtab;	/* for config lookup */
struct hashtab *devitab;		/* etc */
struct attr allattr;
size_t nattrs;

static struct attr errattr;
static struct devbase errdev;
static struct deva errdeva;

static int has_errobj(struct attrlist *, struct attr *);
static struct nvlist *addtoattr(struct nvlist *, struct devbase *);
static int resolve(struct nvlist **, const char *, const char *,
		   struct nvlist *, int);
static struct pspec *getpspec(struct attr *, struct devbase *, int);
static struct devi *newdevi(const char *, int, struct devbase *d);
static struct devi *getdevi(const char *);
static void remove_devi(struct devi *);
static const char *concat(const char *, int);
static char *extend(char *, const char *);
static int split(const char *, size_t, char *, size_t, int *);
static void selectbase(struct devbase *, struct deva *);
static const char **fixloc(const char *, struct attr *, struct loclist *);
static const char *makedevstr(devmajor_t, devminor_t);
static const char *major2name(devmajor_t);
static devmajor_t dev2major(struct devbase *);

extern const char *yyfile;
extern int vflag;

void
initsem(void)
{

	attrtab = ht_new();
	attrdeptab = ht_new();

	allattr.a_name = "netbsd";
	TAILQ_INIT(&allattr.a_files);
	(void)ht_insert(attrtab, allattr.a_name, &allattr);
	selectattr(&allattr);

	errattr.a_name = "<internal>";

	TAILQ_INIT(&allbases);

	TAILQ_INIT(&alldevas);

	TAILQ_INIT(&allpspecs);

	cfhashtab = ht_new();
	TAILQ_INIT(&allcf);

	TAILQ_INIT(&alldevi);
	errdev.d_name = "<internal>";

	TAILQ_INIT(&allpseudo);

	TAILQ_INIT(&alldevms);

	s_ifnet = intern("ifnet");
	s_qmark = intern("?");
	s_none = intern("none");
}

/* Name of include file just ended (set in scan.l) */
extern const char *lastfile;

static struct attr *
finddep(struct attr *a, const char *name)
{
	struct attrlist *al;

	for (al = a->a_deps; al != NULL; al = al->al_next) {
		struct attr *this = al->al_this;
		if (strcmp(this->a_name, name) == 0)
			return this;
	}
	return NULL;
}

static void
mergedeps(const char *dname, const char *name)
{
	struct attr *a, *newa;

	CFGDBG(4, "merging attr `%s' to devbase `%s'", name, dname);
	a = refattr(dname);
	if (finddep(a, name) == NULL) {
		newa = refattr(name);
		a->a_deps = attrlist_cons(a->a_deps, newa);
		CFGDBG(3, "attr `%s' merged to attr `%s'", newa->a_name,
		    a->a_name);
	}
}

static void
fixdev(struct devbase *dev)
{
	struct attrlist *al;
	struct attr *devattr, *a;

	devattr = refattr(dev->d_name);
	if (devattr->a_devclass)
		panic("%s: dev %s is devclass!", __func__, devattr->a_name);

	CFGDBG(4, "fixing devbase `%s'", dev->d_name);
	for (al = dev->d_attrs; al != NULL; al = al->al_next) {
		a = al->al_this;
		CFGDBG(4, "fixing devbase `%s' attr `%s'", dev->d_name, a->a_name);
		if (a->a_iattr) {
			a->a_refs = addtoattr(a->a_refs, dev);
			CFGDBG(3, "device `%s' has iattr `%s'", dev->d_name,
			    a->a_name);
		} else if (a->a_devclass != NULL) {
			if (dev->d_classattr != NULL && dev->d_classattr != a) {
				cfgwarn("device `%s' has multiple classes "
				    "(`%s' and `%s')",
				    dev->d_name, dev->d_classattr->a_name,
				    a->a_name);
			}
			if (dev->d_classattr == NULL) {
				dev->d_classattr = a;
				CFGDBG(3, "device `%s' is devclass `%s'", dev->d_name,
				    a->a_name);
			}
		} else {
			if (strcmp(dev->d_name, a->a_name) != 0) {
				mergedeps(dev->d_name, a->a_name);
			}
		}
	}
}

void
enddefs(void)
{
	struct devbase *dev;

	yyfile = "enddefs";

	TAILQ_FOREACH(dev, &allbases, d_next) {
		if (!dev->d_isdef) {
			(void)fprintf(stderr,
			    "%s: device `%s' used but not defined\n",
			    lastfile, dev->d_name);
			errors++;
			continue;
		}
		fixdev(dev);
	}
	if (errors) {
		(void)fprintf(stderr, "*** Stop.\n");
		exit(1);
	}
}

void
setdefmaxusers(int min, int def, int max)
{

	if (min < 1 || min > def || def > max)
		cfgerror("maxusers must have 1 <= min (%d) <= default (%d) "
		    "<= max (%d)", min, def, max);
	else {
		minmaxusers = min;
		defmaxusers = def;
		maxmaxusers = max;
	}
}

void
setmaxusers(int n)
{

	if (maxusers == n) {
		cfgerror("duplicate maxusers parameter");
		return;
	}
	if (vflag && maxusers != 0)
		cfgwarn("warning: maxusers already defined");
	maxusers = n;
	if (n < minmaxusers) {
		cfgerror("warning: minimum of %d maxusers assumed",
		    minmaxusers);
		errors--;	/* take it away */
		maxusers = minmaxusers;
	} else if (n > maxmaxusers) {
		cfgerror("warning: maxusers (%d) > %d", n, maxmaxusers);
		errors--;
	}
}

void
setident(const char *i)
{

	if (i)
		ident = intern(i);
	else
		ident = NULL;
}

/*
 * Define an attribute, optionally with an interface (a locator list)
 * and a set of attribute-dependencies.
 *
 * Attribute dependencies MAY NOT be interface attributes.
 *
 * Since an empty locator list is logically different from "no interface",
 * all locator lists include a dummy head node, which we discard here.
 */
int
defattr0(const char *name, struct loclist *locs, struct attrlist *deps,
    int devclass)
{

	if (locs != NULL)
		return defiattr(name, locs, deps, devclass);
	else if (devclass)
		return defdevclass(name, locs, deps, devclass);
	else
		return defattr(name, locs, deps, devclass);
}

int
defattr(const char *name, struct loclist *locs, struct attrlist *deps,
    int devclass)
{
	struct attr *a, *dep;
	struct attrlist *al;

	/*
	 * If this attribute depends on any others, make sure none of
	 * the dependencies are interface attributes.
	 */
	for (al = deps; al != NULL; al = al->al_next) {
		dep = al->al_this;
		if (dep->a_iattr) {
			cfgerror("`%s' dependency `%s' is an interface "
			    "attribute", name, dep->a_name);
			return (1);
		}
		(void)ht_insert2(attrdeptab, name, dep->a_name, NULL);
		CFGDBG(2, "attr `%s' depends on attr `%s'", name, dep->a_name);
	}

	if (getrefattr(name, &a)) {
		cfgerror("attribute `%s' already defined", name);
		loclist_destroy(locs);
		return (1);
	}
	if (a == NULL)
		a = mkattr(name);

	a->a_deps = deps;
	expandattr(a, NULL);
	CFGDBG(3, "attr `%s' defined", a->a_name);

	return (0);
}

struct attr *
mkattr(const char *name)
{
	struct attr *a;

	a = ecalloc(1, sizeof *a);
	if (ht_insert(attrtab, name, a)) {
		free(a);
		return NULL;
	}
	a->a_name = name;
	TAILQ_INIT(&a->a_files);
	CFGDBG(3, "attr `%s' allocated", name);

	return a;
}

/* "interface attribute" initialization */
int
defiattr(const char *name, struct loclist *locs, struct attrlist *deps,
    int devclass)
{
	struct attr *a;
	int len;
	struct loclist *ll;

	if (devclass)
		panic("defattr(%s): locators and devclass", name);

	if (defattr(name, locs, deps, devclass) != 0)
		return (1);

	a = getattr(name);
	a->a_iattr = 1;
	/* unwrap */
	a->a_locs = locs->ll_next;
	locs->ll_next = NULL;
	loclist_destroy(locs);
	len = 0;
	for (ll = a->a_locs; ll != NULL; ll = ll->ll_next)
		len++;
	a->a_loclen = len;
	if (deps)
		CFGDBG(2, "attr `%s' iface with deps", a->a_name);
	return (0);
}

/* "device class" initialization */
int
defdevclass(const char *name, struct loclist *locs, struct attrlist *deps,
    int devclass)
{
	struct attr *a;
	char classenum[256], *cp;
	int errored = 0;

	if (deps)
		panic("defattr(%s): dependencies and devclass", name);

	if (defattr(name, locs, deps, devclass) != 0)
		return (1);

	a = getattr(name);
	(void)snprintf(classenum, sizeof(classenum), "DV_%s", name);
	for (cp = classenum + 3; *cp; cp++) {
		if (!errored &&
		    (!isalnum((unsigned char)*cp) ||
		      (isalpha((unsigned char)*cp) && !islower((unsigned char)*cp)))) {
			cfgerror("device class names must be "
			    "lower-case alphanumeric characters");
			errored = 1;
		}
		*cp = (char)toupper((unsigned char)*cp);
	}
	a->a_devclass = intern(classenum);

	return (0);
}

/*
 * Return true if the given `error object' is embedded in the given
 * pointer list.
 */
static int
has_errobj(struct attrlist *al, struct attr *obj)
{

	for (; al != NULL; al = al->al_next)
		if (al->al_this == obj)
			return (1);
	return (0);
}

/*
 * Return true if the given attribute is embedded in the given
 * pointer list.
 */
int
has_attr(struct attrlist *al, const char *attr)
{
	struct attr *a;

	if ((a = getattr(attr)) == NULL)
		return (0);

	for (; al != NULL; al = al->al_next)
		if (al->al_this == a)
			return (1);
	return (0);
}

/*
 * Add a device base to a list in an attribute (actually, to any list).
 * Note that this does not check for duplicates, and does reverse the
 * list order, but no one cares anyway.
 */
static struct nvlist *
addtoattr(struct nvlist *l, struct devbase *dev)
{
	struct nvlist *n;

	n = newnv(NULL, NULL, dev, 0, l);
	return (n);
}

/*
 * Define a device.  This may (or may not) also define an interface
 * attribute and/or refer to existing attributes.
 */
void
defdev(struct devbase *dev, struct loclist *loclist, struct attrlist *attrs,
       int ispseudo)
{
	struct loclist *ll;
	struct attrlist *al;

	if (dev == &errdev)
		goto bad;
	if (dev->d_isdef) {
		cfgerror("redefinition of `%s'", dev->d_name);
		goto bad;
	}

	dev->d_isdef = 1;
	if (has_errobj(attrs, &errattr))
		goto bad;

	/*
	 * Handle implicit attribute definition from locator list.  Do
	 * this before scanning the `at' list so that we can have, e.g.:
	 *	device foo at other, foo { slot = -1 }
	 * (where you can plug in a foo-bus extender to a foo-bus).
	 */
	if (loclist != NULL) {
		ll = loclist;
		loclist = NULL;	/* defattr disposes of them for us */
		if (defiattr(dev->d_name, ll, NULL, 0))
			goto bad;
		attrs = attrlist_cons(attrs, getattr(dev->d_name));
		/* This used to be stored but was never used */
		/* attrs->al_name = dev->d_name; */
	}

	/*
	 * Pseudo-devices can have children.  Consider them as
	 * attaching at root.
	 */
	if (ispseudo) {
		for (al = attrs; al != NULL; al = al->al_next)
			if (al->al_this->a_iattr)
				break;
		if (al != NULL) {
			if (ispseudo < 2) {
				if (version >= 20080610)
					cfgerror("interface attribute on "
					 "non-device pseudo `%s'", dev->d_name);
				else {
					ispseudo = 2;
				}
			}
			ht_insert(devroottab, dev->d_name, dev);
		}
	}

	/* Committed!  Set up fields. */
	dev->d_ispseudo = ispseudo;
	dev->d_attrs = attrs;
	dev->d_classattr = NULL;		/* for now */
	CFGDBG(3, "dev `%s' defined", dev->d_name);

	/*
	 * Implicit attribute definition for device.
	 */
	refattr(dev->d_name);

	/*
	 * For each interface attribute this device refers to, add this
	 * device to its reference list.  This makes, e.g., finding all
	 * "scsi"s easier.
	 *
	 * While looking through the attributes, set up the device
	 * class if any are devclass attributes (and error out if the
	 * device has two classes).
	 */
	for (al = attrs; al != NULL; al = al->al_next) {
		/*
		 * Implicit attribute definition for device dependencies.
		 */
		refattr(al->al_this->a_name);
		(void)ht_insert2(attrdeptab, dev->d_name, al->al_this->a_name, NULL);
		CFGDBG(2, "device `%s' depends on attr `%s'", dev->d_name,
		    al->al_this->a_name);
	}
	return;
 bad:
	loclist_destroy(loclist);
	attrlist_destroyall(attrs);
}

/*
 * Look up a devbase.  Also makes sure it is a reasonable name,
 * i.e., does not end in a digit or contain special characters.
 */
struct devbase *
getdevbase(const char *name)
{
	const u_char *p;
	struct devbase *dev;

	p = (const u_char *)name;
	if (!isalpha(*p))
		goto badname;
	while (*++p) {
		if (!isalnum(*p) && *p != '_')
			goto badname;
	}
	if (isdigit(*--p)) {
 badname:
		cfgerror("bad device base name `%s'", name);
		return (&errdev);
	}
	dev = ht_lookup(devbasetab, name);
	if (dev == NULL) {
		dev = ecalloc(1, sizeof *dev);
		dev->d_name = name;
		dev->d_isdef = 0;
		dev->d_major = NODEVMAJOR;
		dev->d_attrs = NULL;
		dev->d_ihead = NULL;
		dev->d_ipp = &dev->d_ihead;
		dev->d_ahead = NULL;
		dev->d_app = &dev->d_ahead;
		dev->d_umax = 0;
		TAILQ_INSERT_TAIL(&allbases, dev, d_next);
		if (ht_insert(devbasetab, name, dev))
			panic("getdevbase(%s)", name);
		CFGDBG(3, "devbase defined `%s'", dev->d_name);
	}
	return (dev);
}

/*
 * Define some of a device's allowable parent attachments.
 * There may be a list of (plain) attributes.
 */
void
defdevattach(struct deva *deva, struct devbase *dev, struct nvlist *atlist,
	     struct attrlist *attrs)
{
	struct nvlist *nv;
	struct attrlist *al;
	struct attr *a;
	struct deva *da;

	if (dev == &errdev)
		goto bad;
	if (deva == NULL)
		deva = getdevattach(dev->d_name);
	if (deva == &errdeva)
		goto bad;
	if (!dev->d_isdef) {
		cfgerror("attaching undefined device `%s'", dev->d_name);
		goto bad;
	}
	if (deva->d_isdef) {
		cfgerror("redefinition of `%s'", deva->d_name);
		goto bad;
	}
	if (dev->d_ispseudo) {
		cfgerror("pseudo-devices can't attach");
		goto bad;
	}

	deva->d_isdef = 1;
	if (has_errobj(attrs, &errattr))
		goto bad;
	for (al = attrs; al != NULL; al = al->al_next) {
		a = al->al_this;
		if (a == &errattr)
			continue;		/* already complained */
		if (a->a_iattr || a->a_devclass != NULL)
			cfgerror("`%s' is not a plain attribute", a->a_name);
	}

	/* Committed!  Set up fields. */
	deva->d_attrs = attrs;
	deva->d_atlist = atlist;
	deva->d_devbase = dev;
	CFGDBG(3, "deva `%s' defined", deva->d_name);

	/*
	 * Implicit attribute definition for device attachment.
	 */
	refattr(deva->d_name);

	/*
	 * Turn the `at' list into interface attributes (map each
	 * nv_name to an attribute, or to NULL for root), and add
	 * this device to those attributes, so that children can
	 * be listed at this particular device if they are supported
	 * by that attribute.
	 */
	for (nv = atlist; nv != NULL; nv = nv->nv_next) {
		if (nv->nv_name == NULL)
			nv->nv_ptr = a = NULL;	/* at root */
		else
			nv->nv_ptr = a = getattr(nv->nv_name);
		if (a == &errattr)
			continue;		/* already complained */

		/*
		 * Make sure that an attachment spec doesn't
		 * already say how to attach to this attribute.
		 */
		for (da = dev->d_ahead; da != NULL; da = da->d_bsame)
			if (onlist(da->d_atlist, a))
				cfgerror("attach at `%s' already done by `%s'",
				     a ? a->a_name : "root", da->d_name);

		if (a == NULL) {
			ht_insert(devroottab, dev->d_name, dev);
			continue;		/* at root; don't add */
		}
		if (!a->a_iattr)
			cfgerror("%s cannot be at plain attribute `%s'",
			    dev->d_name, a->a_name);
		else
			a->a_devs = addtoattr(a->a_devs, dev);
	}

	/* attach to parent */
	*dev->d_app = deva;
	dev->d_app = &deva->d_bsame;
	return;
 bad:
	nvfreel(atlist);
	attrlist_destroyall(attrs);
}

/*
 * Look up a device attachment.  Also makes sure it is a reasonable
 * name, i.e., does not contain digits or special characters.
 */
struct deva *
getdevattach(const char *name)
{
	const u_char *p;
	struct deva *deva;

	p = (const u_char *)name;
	if (!isalpha(*p))
		goto badname;
	while (*++p) {
		if (!isalnum(*p) && *p != '_')
			goto badname;
	}
	if (isdigit(*--p)) {
 badname:
		cfgerror("bad device attachment name `%s'", name);
		return (&errdeva);
	}
	deva = ht_lookup(devatab, name);
	if (deva == NULL) {
		deva = ecalloc(1, sizeof *deva);
		deva->d_name = name;
		deva->d_bsame = NULL;
		deva->d_isdef = 0;
		deva->d_devbase = NULL;
		deva->d_atlist = NULL;
		deva->d_attrs = NULL;
		deva->d_ihead = NULL;
		deva->d_ipp = &deva->d_ihead;
		TAILQ_INSERT_TAIL(&alldevas, deva, d_next);
		if (ht_insert(devatab, name, deva))
			panic("getdeva(%s)", name);
	}
	return (deva);
}

/*
 * Look up an attribute.
 */
struct attr *
getattr(const char *name)
{
	struct attr *a;

	if ((a = ht_lookup(attrtab, name)) == NULL) {
		cfgerror("undefined attribute `%s'", name);
		a = &errattr;
	}
	return (a);
}

/*
 * Implicit attribute definition.
 */
struct attr *
refattr(const char *name)
{
	struct attr *a;

	if ((a = ht_lookup(attrtab, name)) == NULL)
		a = mkattr(name);
	return a;
}

int
getrefattr(const char *name, struct attr **ra)
{
	struct attr *a;

	a = ht_lookup(attrtab, name);
	if (a == NULL) {
		*ra = NULL;
		return (0);
	}
	/*
	 * Check if the existing attr is only referenced, not really defined.
	 */
	if (a->a_deps == NULL &&
	    a->a_iattr == 0 &&
	    a->a_devclass == 0) {
		*ra = a;
		return (0);
	}
	return (1);
}

/*
 * Recursively expand an attribute and its dependencies, checking for
 * cycles, and invoking a callback for each attribute found.
 */
void
expandattr(struct attr *a, void (*callback)(struct attr *))
{
	struct attrlist *al;
	struct attr *dep;

	if (a->a_expanding) {
		cfgerror("circular dependency on attribute `%s'", a->a_name);
		return;
	}

	a->a_expanding = 1;

	/* First expand all of this attribute's dependencies. */
	for (al = a->a_deps; al != NULL; al = al->al_next) {
		dep = al->al_this;
		expandattr(dep, callback);
	}

	/* ...and now invoke the callback for ourself. */
	if (callback != NULL)
		(*callback)(a);

	a->a_expanding = 0;
}

/*
 * Set the major device number for a device, so that it can be used
 * as a root/dumps "on" device in a configuration.
 */
void
setmajor(struct devbase *d, devmajor_t n)
{

	if (d != &errdev && d->d_major != NODEVMAJOR)
		cfgerror("device `%s' is already major %d",
		    d->d_name, d->d_major);
	else
		d->d_major = n;
}

const char *
major2name(devmajor_t maj)
{
	struct devbase *dev;
	struct devm *dm;

	if (!do_devsw) {
		TAILQ_FOREACH(dev, &allbases, d_next) {
			if (dev->d_major == maj)
				return (dev->d_name);
		}
	} else {
		TAILQ_FOREACH(dm, &alldevms, dm_next) {
			if (dm->dm_bmajor == maj)
				return (dm->dm_name);
		}
	}
	return (NULL);
}

devmajor_t
dev2major(struct devbase *dev)
{
	struct devm *dm;

	if (!do_devsw)
		return (dev->d_major);

	TAILQ_FOREACH(dm, &alldevms, dm_next) {
		if (strcmp(dm->dm_name, dev->d_name) == 0)
			return (dm->dm_bmajor);
	}
	return (NODEVMAJOR);
}

/*
 * Make a string description of the device at maj/min.
 */
static const char *
makedevstr(devmajor_t maj, devminor_t min)
{
	const char *devicename;
	char buf[32];

	devicename = major2name(maj);
	if (devicename == NULL)
		(void)snprintf(buf, sizeof(buf), "<%d/%d>", maj, min);
	else
		(void)snprintf(buf, sizeof(buf), "%s%d%c", devicename,
		    min / maxpartitions, (min % maxpartitions) + 'a');

	return (intern(buf));
}

/*
 * Map things like "ra0b" => makedev(major("ra"), 0*maxpartitions + 'b'-'a').
 * Handle the case where the device number is given but there is no
 * corresponding name, and map NULL to the default.
 */
static int
resolve(struct nvlist **nvp, const char *name, const char *what,
	struct nvlist *dflt, int part)
{
	struct nvlist *nv;
	struct devbase *dev;
	const char *cp;
	devmajor_t maj;
	devminor_t min;
	size_t i, l;
	int unit;
	char buf[NAMESIZE];

	if ((part -= 'a') >= maxpartitions || part < 0)
		panic("resolve");
	if ((nv = *nvp) == NULL) {
		dev_t	d = NODEV;
		/*
		 * Apply default.  Easiest to do this by number.
		 * Make sure to retain NODEVness, if this is dflt's disposition.
		 */
		if ((dev_t)dflt->nv_num != NODEV) {
			maj = major(dflt->nv_num);
			min = ((minor(dflt->nv_num) / maxpartitions) *
			    maxpartitions) + part;
			d = makedev(maj, min);
			cp = makedevstr(maj, min);
		} else
			cp = NULL;
		*nvp = nv = newnv(NULL, cp, NULL, (long long)d, NULL);
	}
	if ((dev_t)nv->nv_num != NODEV) {
		/*
		 * By the numbers.  Find the appropriate major number
		 * to make a name.
		 */
		maj = major(nv->nv_num);
		min = minor(nv->nv_num);
		nv->nv_str = makedevstr(maj, min);
		return (0);
	}

	if (nv->nv_str == NULL || nv->nv_str == s_qmark)
		/*
		 * Wildcarded or unspecified; leave it as NODEV.
		 */
		return (0);

	/*
	 * The normal case: things like "ra2b".  Check for partition
	 * suffix, remove it if there, and split into name ("ra") and
	 * unit (2).
	 */
	l = i = strlen(nv->nv_str);
	cp = &nv->nv_str[l];
	if (l > 1 && *--cp >= 'a' && *cp < 'a' + maxpartitions &&
	    isdigit((unsigned char)cp[-1])) {
		l--;
		part = *cp - 'a';
	}
	cp = nv->nv_str;
	if (split(cp, l, buf, sizeof buf, &unit)) {
		cfgerror("%s: invalid %s device name `%s'", name, what, cp);
		return (1);
	}
	dev = ht_lookup(devbasetab, intern(buf));
	if (dev == NULL) {
		cfgerror("%s: device `%s' does not exist", name, buf);
		return (1);
	}

	/*
	 * Check for the magic network interface attribute, and
	 * don't bother making a device number.
	 */
	if (has_attr(dev->d_attrs, s_ifnet)) {
		nv->nv_num = (long long)NODEV;
		nv->nv_ifunit = unit;	/* XXX XXX XXX */
	} else {
		maj = dev2major(dev);
		if (maj == NODEVMAJOR) {
			cfgerror("%s: can't make %s device from `%s'",
			    name, what, nv->nv_str);
			return (1);
		}
		nv->nv_num = (long long)makedev(maj, unit * maxpartitions + part);
	}

	nv->nv_name = dev->d_name;
	return (0);
}

/*
 * Add a completed configuration to the list.
 */
void
addconf(struct config *cf0)
{
	struct config *cf;
	const char *name;

	name = cf0->cf_name;
	cf = ecalloc(1, sizeof *cf);
	if (ht_insert(cfhashtab, name, cf)) {
		cfgerror("configuration `%s' already defined", name);
		free(cf);
		goto bad;
	}
	*cf = *cf0;

	/*
	 * Resolve the root device.
	 */
	if (cf->cf_root == NULL) {
		cfgerror("%s: no root device specified", name);
		goto bad;
	}
	if (cf->cf_root && cf->cf_root->nv_str != s_qmark) {
		struct nvlist *nv;
		nv = cf->cf_root;
		if (resolve(&cf->cf_root, name, "root", nv, 'a'))
			goto bad;
	}

	/*
	 * Resolve the dump device.
	 */
	if (cf->cf_dump == NULL || cf->cf_dump->nv_str == s_qmark) {
		/*
		 * Wildcarded dump device is equivalent to unspecified.
		 */
		cf->cf_dump = NULL;
	} else if (cf->cf_dump->nv_str == s_none) {
		/*
		 * Operator has requested that no dump device should be
		 * configured; do nothing.
		 */
	} else {
		if (resolve(&cf->cf_dump, name, "dumps", cf->cf_dump, 'b'))
			goto bad;
	}

	/* Wildcarded fstype is `unspecified'. */
	if (cf->cf_fstype == s_qmark)
		cf->cf_fstype = NULL;

	TAILQ_INSERT_TAIL(&allcf, cf, cf_next);
	return;
 bad:
	nvfreel(cf0->cf_root);
	nvfreel(cf0->cf_dump);
}

void
setconf(struct nvlist **npp, const char *what, struct nvlist *v)
{

	if (*npp != NULL) {
		cfgerror("duplicate %s specification", what);
		nvfreel(v);
	} else
		*npp = v;
}

void
delconf(const char *name)
{
	struct config *cf;

	CFGDBG(5, "deselecting config `%s'", name);
	if (ht_lookup(cfhashtab, name) == NULL) {
		cfgerror("configuration `%s' undefined", name);
		return;
	}
	(void)ht_remove(cfhashtab, name);

	TAILQ_FOREACH(cf, &allcf, cf_next)
		if (!strcmp(cf->cf_name, name))
			break;
	if (cf == NULL)
		panic("lost configuration `%s'", name);

	TAILQ_REMOVE(&allcf, cf, cf_next);
}

void
setfstype(const char **fstp, const char *v)
{

	if (*fstp != NULL) {
		cfgerror("multiple fstype specifications");
		return;
	}

	if (v != s_qmark && OPT_FSOPT(v)) {
		cfgerror("\"%s\" is not a configured file system", v);
		return;
	}

	*fstp = v;
}

static struct devi *
newdevi(const char *name, int unit, struct devbase *d)
{
	struct devi *i;

	i = ecalloc(1, sizeof *i);
	i->i_name = name;
	i->i_unit = unit;
	i->i_base = d;
	i->i_bsame = NULL;
	i->i_asame = NULL;
	i->i_alias = NULL;
	i->i_at = NULL;
	i->i_pspec = NULL;
	i->i_atdeva = NULL;
	i->i_locs = NULL;
	i->i_cfflags = 0;
	i->i_lineno = currentline();
	i->i_srcfile = yyfile;
	i->i_active = DEVI_ORPHAN; /* Proper analysis comes later */
	i->i_level = devilevel;
	i->i_pseudoroot = 0;
	if (unit >= d->d_umax)
		d->d_umax = unit + 1;
	return (i);
}

/*
 * Add the named device as attaching to the named attribute (or perhaps
 * another device instead) plus unit number.
 */
void
adddev(const char *name, const char *at, struct loclist *loclist, int flags)
{
	struct devi *i;		/* the new instance */
	struct pspec *p;	/* and its pspec */
	struct attr *attr;	/* attribute that allows attach */
	struct devbase *ib;	/* i->i_base */
	struct devbase *ab;	/* not NULL => at another dev */
	struct attrlist *al;
	struct deva *iba;	/* devbase attachment used */
	const char *cp;
	int atunit;
	char atbuf[NAMESIZE];
	int hit;

	ab = NULL;
	iba = NULL;
	if (at == NULL) {
		/* "at root" */
		p = NULL;
		if ((i = getdevi(name)) == NULL)
			goto bad;
		/*
		 * Must warn about i_unit > 0 later, after taking care of
		 * the STAR cases (we could do non-star's here but why
		 * bother?).  Make sure this device can be at root.
		 */
		ib = i->i_base;
		hit = 0;
		for (iba = ib->d_ahead; iba != NULL; iba = iba->d_bsame)
			if (onlist(iba->d_atlist, NULL)) {
				hit = 1;
				break;
			}
		if (!hit) {
			cfgerror("`%s' cannot attach to the root", ib->d_name);
			i->i_active = DEVI_BROKEN;
			goto bad;
		}
		attr = &errattr;	/* a convenient "empty" attr */
	} else {
		if (split(at, strlen(at), atbuf, sizeof atbuf, &atunit)) {
			cfgerror("invalid attachment name `%s'", at);
			/* (void)getdevi(name); -- ??? */
			goto bad;
		}
		if ((i = getdevi(name)) == NULL)
			goto bad;
		ib = i->i_base;

		/*
		 * Devices can attach to two types of things: Attributes,
		 * and other devices (which have the appropriate attributes
		 * to allow attachment).
		 *
		 * (1) If we're attached to an attribute, then we don't need
		 *     look at the parent base device to see what attributes
		 *     it has, and make sure that we can attach to them.    
		 *
		 * (2) If we're attached to a real device (i.e. named in
		 *     the config file), we want to remember that so that
		 *     at cross-check time, if the device we're attached to
		 *     is missing but other devices which also provide the
		 *     attribute are present, we don't get a false "OK."
		 *
		 * (3) If the thing we're attached to is an attribute
		 *     but is actually named in the config file, we still
		 *     have to remember its devbase.
		 */
		cp = intern(atbuf);

		/* Figure out parent's devbase, to satisfy case (3). */
		ab = ht_lookup(devbasetab, cp);

		/* Find out if it's an attribute. */
		attr = ht_lookup(attrtab, cp);

		/* Make sure we're _really_ attached to the attr.  Case (1). */
		if (attr != NULL && onlist(attr->a_devs, ib))
			goto findattachment;

		/*
		 * Else a real device, and not just an attribute.  Case (2).
		 *
		 * Have to work a bit harder to see whether we have
		 * something like "tg0 at esp0" (where esp is merely
		 * not an attribute) or "tg0 at nonesuch0" (where
		 * nonesuch is not even a device).
		 */
		if (ab == NULL) {
			cfgerror("%s at %s: `%s' unknown",
			    name, at, atbuf);
			i->i_active = DEVI_BROKEN;
			goto bad;
		}

		/*
		 * See if the named parent carries an attribute
		 * that allows it to supervise device ib.
		 */
		for (al = ab->d_attrs; al != NULL; al = al->al_next) {
			attr = al->al_this;
			if (onlist(attr->a_devs, ib))
				goto findattachment;
		}
		cfgerror("`%s' cannot attach to `%s'", ib->d_name, atbuf);
		i->i_active = DEVI_BROKEN;
		goto bad;

 findattachment:
		/*
		 * Find the parent spec.  If a matching one has not yet been
		 * created, create one.
		 */
		p = getpspec(attr, ab, atunit);
		p->p_devs = newnv(NULL, NULL, i, 0, p->p_devs);

		/* find out which attachment it uses */
		hit = 0;
		for (iba = ib->d_ahead; iba != NULL; iba = iba->d_bsame)
			if (onlist(iba->d_atlist, attr)) {
				hit = 1;
				break;
			}
		if (!hit)
			panic("adddev: can't figure out attachment");
	}
	if ((i->i_locs = fixloc(name, attr, loclist)) == NULL) {
		i->i_active = DEVI_BROKEN;
		goto bad;
	}
	i->i_at = at;
	i->i_pspec = p;
	i->i_atdeva = iba;
	i->i_cfflags = flags;
	CFGDBG(3, "devi `%s' added", i->i_name);

	*iba->d_ipp = i;
	iba->d_ipp = &i->i_asame;

	/* all done, fall into ... */
 bad:
	loclist_destroy(loclist);
	return;
}

void
deldevi(const char *name, const char *at)
{
	struct devi *firsti, *i;
	struct devbase *d;
	int unit;
	char base[NAMESIZE];

	CFGDBG(5, "deselecting devi `%s'", name);
	if (split(name, strlen(name), base, sizeof base, &unit)) {
		cfgerror("invalid device name `%s'", name);
		return;
	}
	d = ht_lookup(devbasetab, intern(base));
	if (d == NULL) {
		cfgerror("%s: unknown device `%s'", name, base);
		return;
	}
	if (d->d_ispseudo) {
		cfgerror("%s: %s is a pseudo-device", name, base);
		return;
	}
	if ((firsti = ht_lookup(devitab, name)) == NULL) {
		cfgerror("`%s' not defined", name);
		return;
	}
	if (at == NULL && firsti->i_at == NULL) {
		/* 'at root' */
		remove_devi(firsti);
		return;
	} else if (at != NULL)
		for (i = firsti; i != NULL; i = i->i_alias)
			if (i->i_active != DEVI_BROKEN &&
			    strcmp(at, i->i_at) == 0) {
				remove_devi(i);
				return;
			}
	cfgerror("`%s' at `%s' not found", name, at ? at : "root");
}

static void
remove_devi(struct devi *i)
{
	struct devbase *d = i->i_base;
	struct devi *f, *j, **ppi;
	struct deva *iba;

	CFGDBG(5, "removing devi `%s'", i->i_name);
	f = ht_lookup(devitab, i->i_name);
	if (f == NULL)
		panic("remove_devi(): instance %s disappeared from devitab",
		    i->i_name);

	if (i->i_active == DEVI_BROKEN) {
		cfgerror("not removing broken instance `%s'", i->i_name);
		return;
	}

	/*
	 * We have the device instance, i.
	 * We have to:
	 *   - delete the alias
	 *
	 *      If the devi was an alias of an already listed devi, all is
	 *      good we don't have to do more.
	 *      If it was the first alias, we have to replace i's entry in
	 *      d's list by its first alias.
	 *      If it was the only entry, we must remove i's entry from d's
	 *      list.
	 */
	if (i != f) {
		for (j = f; j->i_alias != i; j = j->i_alias)
			continue;
		j->i_alias = i->i_alias;
	} else {
		if (i->i_alias == NULL) {
			/* No alias, must unlink the entry from devitab */
			ht_remove(devitab, i->i_name);
			j = i->i_bsame;
		} else {
			/* Or have the first alias replace i in d's list */
			i->i_alias->i_bsame = i->i_bsame;
			j = i->i_alias;
			if (i == f)
				ht_replace(devitab, i->i_name, i->i_alias);
		}

		/*
		 *   - remove/replace the instance from the devbase's list
		 *
		 * A double-linked list would make this much easier.  Oh, well,
		 * what is done is done.
		 */
		for (ppi = &d->d_ihead;
		    *ppi != NULL && *ppi != i && (*ppi)->i_bsame != i;
		    ppi = &(*ppi)->i_bsame)
			continue;
		if (*ppi == NULL)
			panic("deldev: dev (%s) doesn't list the devi"
			    " (%s at %s)", d->d_name, i->i_name, i->i_at);
		f = *ppi;
		if (f == i)
			/* That implies d->d_ihead == i */
			*ppi = j;
		else
			(*ppi)->i_bsame = j;
		if (d->d_ipp == &i->i_bsame) {
			if (i->i_alias == NULL) {
				if (f == i)
					d->d_ipp = &d->d_ihead;
				else
					d->d_ipp = &f->i_bsame;
			} else
				d->d_ipp = &i->i_alias->i_bsame;
		}
	}
	/*
	 *   - delete the attachment instance
	 */
	iba = i->i_atdeva;
	for (ppi = &iba->d_ihead;
	    *ppi != NULL && *ppi != i && (*ppi)->i_asame != i;
	    ppi = &(*ppi)->i_asame)
		continue;
	if (*ppi == NULL)
		panic("deldev: deva (%s) doesn't list the devi (%s)",
		    iba->d_name, i->i_name);
	f = *ppi;
	if (f == i)
		/* That implies iba->d_ihead == i */
		*ppi = i->i_asame;
	else
		(*ppi)->i_asame = i->i_asame;
	if (iba->d_ipp == &i->i_asame) {
		if (f == i)
			iba->d_ipp = &iba->d_ihead;
		else
			iba->d_ipp = &f->i_asame;
	}
	/*
	 *   - delete the pspec
	 */
	if (i->i_pspec) {
		struct pspec *p = i->i_pspec;
		struct nvlist *nv, *onv;

		/* Double-linked nvlist anyone? */
		for (nv = p->p_devs; nv->nv_next != NULL; nv = nv->nv_next) {
			if (nv->nv_next && nv->nv_next->nv_ptr == i) {
				onv = nv->nv_next;
				nv->nv_next = onv->nv_next;
				nvfree(onv);
				break;
			}
			if (nv->nv_ptr == i) {
				/* nv is p->p_devs in that case */
				p->p_devs = nv->nv_next;
				nvfree(nv);
				break;
			}
		}
		if (p->p_devs == NULL)
			TAILQ_REMOVE(&allpspecs, p, p_list);
	}
	/*
	 *   - delete the alldevi entry
	 */
	TAILQ_REMOVE(&alldevi, i, i_next);
	ndevi--;
	/*
	 * Put it in deaddevitab
	 *
	 * Each time a devi is removed, devilevel is increased so that later on
	 * it is possible to tell if an instance was added before or after the
	 * removal of its parent.
	 *
	 * For active instances, i_level contains the number of devi removed so
	 * far, and for dead devis, it contains its index.
	 */
	i->i_level = devilevel++;
	i->i_alias = NULL;
	f = ht_lookup(deaddevitab, i->i_name);
	if (f == NULL) {
		if (ht_insert(deaddevitab, i->i_name, i))
			panic("remove_devi(%s) - can't add to deaddevitab",
			    i->i_name);
	} else {
		for (j = f; j->i_alias != NULL; j = j->i_alias)
			continue;
		j->i_alias = i;
	}
	/*
	 *   - reconstruct d->d_umax
	 */
	d->d_umax = 0;
	for (i = d->d_ihead; i != NULL; i = i->i_bsame)
		if (i->i_unit >= d->d_umax)
			d->d_umax = i->i_unit + 1;
}

void
deldeva(const char *at)
{
	int unit;
	const char *cp;
	struct devbase *d, *ad;
	struct devi *i, *j;
	struct attr *a;
	struct pspec *p;
	struct nvlist *nv, *stack = NULL;

	if (at == NULL) {
		TAILQ_FOREACH(i, &alldevi, i_next)
			if (i->i_at == NULL)
				stack = newnv(NULL, NULL, i, 0, stack);
	} else {
		size_t l;

		CFGDBG(5, "deselecting deva `%s'", at);
		if (at[0] == '\0')
			goto out;
			
		l = strlen(at) - 1;
		if (at[l] == '?' || isdigit((unsigned char)at[l])) {
			char base[NAMESIZE];

			if (split(at, l+1, base, sizeof base, &unit)) {
out:
				cfgerror("invalid attachment name `%s'", at);
				return;
			}
			cp = intern(base);
		} else {
			cp = intern(at);
			unit = STAR;
		}

		ad = ht_lookup(devbasetab, cp);
		a = ht_lookup(attrtab, cp);
		if (a == NULL) {
			cfgerror("unknown attachment attribute or device `%s'",
			    cp);
			return;
		}
		if (!a->a_iattr) {
			cfgerror("plain attribute `%s' cannot have children",
			    a->a_name);
			return;
		}

		/*
		 * remove_devi() makes changes to the devbase's list and the
		 * alias list, * so the actual deletion of the instances must
		 * be delayed.
		 */
		for (nv = a->a_devs; nv != NULL; nv = nv->nv_next) {
			d = nv->nv_ptr;
			for (i = d->d_ihead; i != NULL; i = i->i_bsame)
				for (j = i; j != NULL; j = j->i_alias) {
					/* Ignore devices at root */
					if (j->i_at == NULL)
						continue;
					p = j->i_pspec;
					/*
					 * There are three cases:
					 *
					 * 1.  unit is not STAR.  Consider 'at'
					 *     to be explicit, even if it
					 *     references an interface
					 *     attribute.
					 *
					 * 2.  unit is STAR and 'at' references
					 *     a real device.  Look for pspec
					 *     that have a matching p_atdev
					 *     field.
					 *
					 * 3.  unit is STAR and 'at' references
					 *     an interface attribute.  Look
					 *     for pspec that have a matching
					 *     p_iattr field.
					 */
					if ((unit != STAR &&        /* Case */
					     !strcmp(j->i_at, at)) ||  /* 1 */
					    (unit == STAR &&
					     ((ad != NULL &&        /* Case */
					       p->p_atdev == ad) ||    /* 2 */
					      (ad == NULL &&        /* Case */
					       p->p_iattr == a))))     /* 3 */
						stack = newnv(NULL, NULL, j, 0,
						    stack);
				}
		}
	}

	for (nv = stack; nv != NULL; nv = nv->nv_next)
		remove_devi(nv->nv_ptr);
	nvfreel(stack);
}

void
deldev(const char *name)
{
	size_t l;
	struct devi *firsti, *i;
	struct nvlist *nv, *stack = NULL;

	CFGDBG(5, "deselecting dev `%s'", name);
	if (name[0] == '\0')
		goto out;

	l = strlen(name) - 1;
	if (name[l] == '*' || isdigit((unsigned char)name[l])) {
		/* `no mydev0' or `no mydev*' */
		firsti = ht_lookup(devitab, name);
		if (firsti == NULL) {
out:
			cfgerror("unknown instance %s", name);
			return;
		}
		for (i = firsti; i != NULL; i = i->i_alias)
			stack = newnv(NULL, NULL, i, 0, stack);
	} else {
		struct devbase *d = ht_lookup(devbasetab, name);

		if (d == NULL) {
			cfgerror("unknown device %s", name);
			return;
		}
		if (d->d_ispseudo) {
			cfgerror("%s is a pseudo-device; "
			    "use \"no pseudo-device %s\" instead", name,
			    name);
			return;
		}

		for (firsti = d->d_ihead; firsti != NULL;
		    firsti = firsti->i_bsame)
			for (i = firsti; i != NULL; i = i->i_alias)
				stack = newnv(NULL, NULL, i, 0, stack);
	}

	for (nv = stack; nv != NULL; nv = nv->nv_next)
		remove_devi(nv->nv_ptr);
	nvfreel(stack);
}

/*
 * Insert given device "name" into devroottab.  In case "name"
 * designates a pure interface attribute, create a fake device
 * instance for the attribute and insert that into the roottab
 * (this scheme avoids mucking around with the orphanage analysis).
 */
void
addpseudoroot(const char *name)
{
	char buf[NAMESIZE];
	int unit;
	struct attr *attr;
	struct devi *i;
	struct deva *iba;
	struct devbase *ib;

	if (split(name, strlen(name), buf, sizeof(buf), &unit)) {
		cfgerror("invalid pseudo-root name `%s'", name);
		return;
	}

	/*
	 * Prefer device because devices with locators define an
	 * implicit interface attribute.  However, if a device is
	 * not available, try to attach to the interface attribute.
	 * This makes sure adddev() doesn't get confused when we
	 * are really attaching to a device (alternatively we maybe
	 * could specify a non-NULL atlist to defdevattach() below).
	 */
	ib = ht_lookup(devbasetab, intern(buf));
	if (ib == NULL) {
		struct devbase *fakedev;
		char fakename[NAMESIZE];

		attr = ht_lookup(attrtab, intern(buf));
		if (!(attr && attr->a_iattr)) {
			cfgerror("pseudo-root `%s' not available", name);
			return;
		}

		/*
		 * here we cheat a bit: create a fake devbase with the
		 * interface attribute and instantiate it.  quick, cheap,
		 * dirty & bad for you, much like the stuff in the fridge.
		 * and, it works, since the pseudoroot device is not included
		 * in ioconf, just used by config to make sure we start from
		 * the right place.
		 */ 
		snprintf(fakename, sizeof(fakename), "%s_devattrs", buf);
		fakedev = getdevbase(intern(fakename));
		fakedev->d_isdef = 1;
		fakedev->d_ispseudo = 0;
		fakedev->d_attrs = attrlist_cons(NULL, attr);
		defdevattach(NULL, fakedev, NULL, NULL);

		if (unit == STAR)
			snprintf(buf, sizeof(buf), "%s*", fakename);
		else
			snprintf(buf, sizeof(buf), "%s%d", fakename, unit);
		name = buf;
	}

	/* ok, everything should be set up, so instantiate a fake device */
	i = getdevi(name);
	if (i == NULL)
		panic("device `%s' expected to be present", name);
	ib = i->i_base;
	iba = ib->d_ahead;

	i->i_atdeva = iba;
	i->i_cfflags = 0;
	i->i_locs = fixloc(name, &errattr, NULL);
	i->i_pseudoroot = 1;
	i->i_active = DEVI_ORPHAN; /* set active by kill_orphans() */

	*iba->d_ipp = i;
	iba->d_ipp = &i->i_asame;

	ht_insert(devroottab, ib->d_name, ib);
}

void
addpseudo(const char *name, int number)
{
	struct devbase *d;
	struct devi *i;

	d = ht_lookup(devbasetab, name);
	if (d == NULL) {
		cfgerror("undefined pseudo-device %s", name);
		return;
	}
	if (!d->d_ispseudo) {
		cfgerror("%s is a real device, not a pseudo-device", name);
		return;
	}
	if (ht_lookup(devitab, name) != NULL) {
		cfgerror("`%s' already defined", name);
		return;
	}
	i = newdevi(name, number - 1, d);	/* foo 16 => "foo0..foo15" */
	if (ht_insert(devitab, name, i))
		panic("addpseudo(%s)", name);
	/* Useful to retrieve the instance from the devbase */
	d->d_ihead = i;
	i->i_active = DEVI_ACTIVE;
	TAILQ_INSERT_TAIL(&allpseudo, i, i_next);
}

void
delpseudo(const char *name)
{
	struct devbase *d;
	struct devi *i;

	CFGDBG(5, "deselecting pseudo `%s'", name);
	d = ht_lookup(devbasetab, name);
	if (d == NULL) {
		cfgerror("undefined pseudo-device %s", name);
		return;
	}
	if (!d->d_ispseudo) {
		cfgerror("%s is a real device, not a pseudo-device", name);
		return;
	}
	if ((i = ht_lookup(devitab, name)) == NULL) {
		cfgerror("`%s' not defined", name);
		return;
	}
	d->d_umax = 0;		/* clear neads-count entries */
	d->d_ihead = NULL;	/* make sure it won't be considered active */
	TAILQ_REMOVE(&allpseudo, i, i_next);
	if (ht_remove(devitab, name))
		panic("delpseudo(%s) - can't remove from devitab", name);
	if (ht_insert(deaddevitab, name, i))
		panic("delpseudo(%s) - can't add to deaddevitab", name);
}

void
adddevm(const char *name, devmajor_t cmajor, devmajor_t bmajor,
	struct condexpr *cond, struct nvlist *nv_nodes)
{
	struct devm *dm;

	if (cmajor != NODEVMAJOR && (cmajor < 0 || cmajor >= 4096)) {
		cfgerror("character major %d is invalid", cmajor);
		condexpr_destroy(cond);
		nvfreel(nv_nodes);
		return;
	}

	if (bmajor != NODEVMAJOR && (bmajor < 0 || bmajor >= 4096)) {
		cfgerror("block major %d is invalid", bmajor);
		condexpr_destroy(cond);
		nvfreel(nv_nodes);
		return;
	}
	if (cmajor == NODEVMAJOR && bmajor == NODEVMAJOR) {
		cfgerror("both character/block majors are not specified");
		condexpr_destroy(cond);
		nvfreel(nv_nodes);
		return;
	}

	dm = ecalloc(1, sizeof(*dm));
	dm->dm_srcfile = yyfile;
	dm->dm_srcline = currentline();
	dm->dm_name = name;
	dm->dm_cmajor = cmajor;
	dm->dm_bmajor = bmajor;
	dm->dm_opts = cond;
	dm->dm_devnodes = nv_nodes;

	TAILQ_INSERT_TAIL(&alldevms, dm, dm_next);

	maxcdevm = MAX(maxcdevm, dm->dm_cmajor);
	maxbdevm = MAX(maxbdevm, dm->dm_bmajor);
}

int
fixdevis(void)
{
	struct devi *i;
	int error = 0;

	TAILQ_FOREACH(i, &alldevi, i_next) {
		CFGDBG(3, "fixing devis `%s'", i->i_name);
		if (i->i_active == DEVI_ACTIVE)
			selectbase(i->i_base, i->i_atdeva);
		else if (i->i_active == DEVI_ORPHAN) {
			/*
			 * At this point, we can't have instances for which
			 * i_at or i_pspec are NULL.
			 */
			++error;
			cfgxerror(i->i_srcfile, i->i_lineno,
			    "`%s at %s' is orphaned (%s `%s' found)", 
			    i->i_name, i->i_at, i->i_pspec->p_atunit == WILD ?
			    "nothing matching" : "no", i->i_at);
		} else if (vflag && i->i_active == DEVI_IGNORED)
			cfgxwarn(i->i_srcfile, i->i_lineno, "ignoring "
			    "explicitly orphaned instance `%s at %s'",
			    i->i_name, i->i_at);
	}

	if (error)
		return error;

	TAILQ_FOREACH(i, &allpseudo, i_next)
		if (i->i_active == DEVI_ACTIVE)
			selectbase(i->i_base, NULL);
	return 0;
}

/*
 * Look up a parent spec, creating a new one if it does not exist.
 */
static struct pspec *
getpspec(struct attr *attr, struct devbase *ab, int atunit)
{
	struct pspec *p;

	TAILQ_FOREACH(p, &allpspecs, p_list) {
		if (p->p_iattr == attr &&
		    p->p_atdev == ab &&
		    p->p_atunit == atunit)
			return (p);
	}

	p = ecalloc(1, sizeof(*p));

	p->p_iattr = attr;
	p->p_atdev = ab;
	p->p_atunit = atunit;
	p->p_inst = npspecs++;
	p->p_active = 0;

	TAILQ_INSERT_TAIL(&allpspecs, p, p_list);

	return (p);
}

/*
 * Define a new instance of a specific device.
 */
static struct devi *
getdevi(const char *name)
{
	struct devi *i, *firsti;
	struct devbase *d;
	int unit;
	char base[NAMESIZE];

	if (split(name, strlen(name), base, sizeof base, &unit)) {
		cfgerror("invalid device name `%s'", name);
		return (NULL);
	}
	d = ht_lookup(devbasetab, intern(base));
	if (d == NULL) {
		cfgerror("%s: unknown device `%s'", name, base);
		return (NULL);
	}
	if (d->d_ispseudo) {
		cfgerror("%s: %s is a pseudo-device", name, base);
		return (NULL);
	}
	firsti = ht_lookup(devitab, name);
	i = newdevi(name, unit, d);
	if (firsti == NULL) {
		if (ht_insert(devitab, name, i))
			panic("getdevi(%s)", name);
		*d->d_ipp = i;
		d->d_ipp = &i->i_bsame;
	} else {
		while (firsti->i_alias)
			firsti = firsti->i_alias;
		firsti->i_alias = i;
	}
	TAILQ_INSERT_TAIL(&alldevi, i, i_next);
	ndevi++;
	return (i);
}

static const char *
concat(const char *name, int c)
{
	size_t len;
	char buf[NAMESIZE];

	len = strlen(name);
	if (len + 2 > sizeof(buf)) {
		cfgerror("device name `%s%c' too long", name, c);
		len = sizeof(buf) - 2;
	}
	memmove(buf, name, len);
	buf[len] = (char)c;
	buf[len + 1] = '\0';
	return (intern(buf));
}

const char *
starref(const char *name)
{

	return (concat(name, '*'));
}

const char *
wildref(const char *name)
{

	return (concat(name, '?'));
}

/*
 * Split a name like "foo0" into base name (foo) and unit number (0).
 * Return 0 on success.  To make this useful for names like "foo0a",
 * the length of the "foo0" part is one of the arguments.
 */
static int
split(const char *name, size_t nlen, char *base, size_t bsize, int *aunit)
{
	const char *cp;
	int c;
	size_t l;

	l = nlen;
	if (l < 2 || l >= bsize || isdigit((unsigned char)*name))
		return (1);
	c = (u_char)name[--l];
	if (!isdigit(c)) {
		if (c == '*')
			*aunit = STAR;
		else if (c == '?')
			*aunit = WILD;
		else
			return (1);
	} else {
		cp = &name[l];
		while (isdigit((unsigned char)cp[-1]))
			l--, cp--;
		*aunit = atoi(cp);
	}
	memmove(base, name, l);
	base[l] = 0;
	return (0);
}

void
addattr(const char *name)
{
	struct attr *a;

	a = refattr(name);
	selectattr(a);
}

void
delattr(const char *name)
{
	struct attr *a;

	a = refattr(name);
	deselectattr(a);
}

void
selectattr(struct attr *a)
{
	struct attrlist *al;
	struct attr *dep;

	CFGDBG(5, "selecting attr `%s'", a->a_name);
	for (al = a->a_deps; al != NULL; al = al->al_next) {
		dep = al->al_this;
		selectattr(dep);
	}
	if (ht_insert(selecttab, a->a_name, __UNCONST(a->a_name)) == 0)
		nattrs++;
	CFGDBG(3, "attr selected `%s'", a->a_name);
}

static int
deselectattrcb2(const char *name1, const char *name2, void *v, void *arg)
{
	const char *name = arg;

	if (strcmp(name, name2) == 0)
		delattr(name1);
	return 0;
}

void
deselectattr(struct attr *a)
{

	CFGDBG(5, "deselecting attr `%s'", a->a_name);
	ht_enumerate2(attrdeptab, deselectattrcb2, __UNCONST(a->a_name));
	if (ht_remove(selecttab, a->a_name) == 0)
		nattrs--;
	CFGDBG(3, "attr deselected `%s'", a->a_name);
}

static int
dumpattrdepcb2(const char *name1, const char *name2, void *v, void *arg)
{

	CFGDBG(3, "attr `%s' depends on attr `%s'", name1, name2);
	return 0;
}

void
dependattrs(void)
{

	ht_enumerate2(attrdeptab, dumpattrdepcb2, NULL);
}

/*
 * We have an instance of the base foo, so select it and all its
 * attributes for "optional foo".
 */
static void
selectbase(struct devbase *d, struct deva *da)
{
	struct attr *a;
	struct attrlist *al;

	(void)ht_insert(selecttab, d->d_name, __UNCONST(d->d_name));
	CFGDBG(3, "devbase selected `%s'", d->d_name);
	CFGDBG(5, "selecting dependencies of devbase `%s'", d->d_name);
	for (al = d->d_attrs; al != NULL; al = al->al_next) {
		a = al->al_this;
		expandattr(a, selectattr);
	}

	struct attr *devattr;
	devattr = refattr(d->d_name);
	expandattr(devattr, selectattr);
	
	if (da != NULL) {
		(void)ht_insert(selecttab, da->d_name, __UNCONST(da->d_name));
		CFGDBG(3, "devattr selected `%s'", da->d_name);
		for (al = da->d_attrs; al != NULL; al = al->al_next) {
			a = al->al_this;
			expandattr(a, selectattr);
		}
	}

	fixdev(d);
}

/*
 * Is the given pointer on the given list of pointers?
 */
int
onlist(struct nvlist *nv, void *ptr)
{
	for (; nv != NULL; nv = nv->nv_next)
		if (nv->nv_ptr == ptr)
			return (1);
	return (0);
}

static char *
extend(char *p, const char *name)
{
	size_t l;

	l = strlen(name);
	memmove(p, name, l);
	p += l;
	*p++ = ',';
	*p++ = ' ';
	return (p);
}

/*
 * Check that we got all required locators, and default any that are
 * given as "?" and have defaults.  Return 0 on success.
 */
static const char **
fixloc(const char *name, struct attr *attr, struct loclist *got)
{
	struct loclist *m, *n;
	int ord;
	const char **lp;
	int nmissing, nextra, nnodefault;
	char *mp, *ep, *ndp;
	char missing[1000], extra[1000], nodefault[1000];
	static const char *nullvec[1];

	/*
	 * Look for all required locators, and number the given ones
	 * according to the required order.  While we are numbering,
	 * set default values for defaulted locators.
	 */
	if (attr->a_loclen == 0)	/* e.g., "at root" */
		lp = nullvec;
	else
		lp = emalloc((size_t)(attr->a_loclen + 1) * sizeof(const char *));
	for (n = got; n != NULL; n = n->ll_next)
		n->ll_num = -1;
	nmissing = 0;
	mp = missing;
	/* yes, this is O(mn), but m and n should be small */
	for (ord = 0, m = attr->a_locs; m != NULL; m = m->ll_next, ord++) {
		for (n = got; n != NULL; n = n->ll_next) {
			if (n->ll_name == m->ll_name) {
				n->ll_num = ord;
				break;
			}
		}
		if (n == NULL && m->ll_num == 0) {
			nmissing++;
			mp = extend(mp, m->ll_name);
		}
		lp[ord] = m->ll_string;
	}
	if (ord != attr->a_loclen)
		panic("fixloc");
	lp[ord] = NULL;
	nextra = 0;
	ep = extra;
	nnodefault = 0;
	ndp = nodefault;
	for (n = got; n != NULL; n = n->ll_next) {
		if (n->ll_num >= 0) {
			if (n->ll_string != NULL)
				lp[n->ll_num] = n->ll_string;
			else if (lp[n->ll_num] == NULL) {
				nnodefault++;
				ndp = extend(ndp, n->ll_name);
			}
		} else {
			nextra++;
			ep = extend(ep, n->ll_name);
		}
	}
	if (nextra) {
		ep[-2] = 0;	/* kill ", " */
		cfgerror("%s: extraneous locator%s: %s",
		    name, nextra > 1 ? "s" : "", extra);
	}
	if (nmissing) {
		mp[-2] = 0;
		cfgerror("%s: must specify %s", name, missing);
	}
	if (nnodefault) {
		ndp[-2] = 0;
		cfgerror("%s: cannot wildcard %s", name, nodefault);
	}
	if (nmissing || nnodefault) {
		free(lp);
		lp = NULL;
	}
	return (lp);
}

void
setversion(int newver)
{
	if (newver > CONFIG_VERSION)
		cfgerror("your sources require a newer version of config(1) "
		    "-- please rebuild it.");
	else if (newver < CONFIG_MINVERSION)
		cfgerror("your sources are out of date -- please update.");
	else
		version = newver;
}

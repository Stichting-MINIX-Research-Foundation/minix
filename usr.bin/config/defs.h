/*	$NetBSD: defs.h,v 1.93 2015/09/04 10:16:35 uebayasi Exp $	*/

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
 *	from: @(#)config.h	8.1 (Berkeley) 6/6/93
 */

/*
 * defs.h:  Global definitions for "config"
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/queue.h>

#if !defined(MAKE_BOOTSTRAP) && defined(BSD)
#include <sys/cdefs.h>
#include <paths.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* These are really for MAKE_BOOTSTRAP but harmless. */
#ifndef __dead
#define __dead
#endif
#ifndef __printflike
#define __printflike(a, b)
#endif
#ifndef _PATH_DEVNULL
#define _PATH_DEVNULL "/dev/null"
#endif

#ifdef	MAKE_BOOTSTRAP
#undef	dev_t
#undef	devmajor_t
#undef	devminor_t
#undef	NODEV
#undef	NODEVMAJOR
#undef	major
#undef	minor
#undef	makedev
#define	dev_t		unsigned int	/* XXX: assumes int is 32 bits */
#define	NODEV		((dev_t)-1)
#define devmajor_t	int
#define devminor_t	int
#define NODEVMAJOR	(-1)
#define major(x)        ((devmajor_t)((((x) & 0x000fff00) >>  8)))
#define minor(x)        ((devminor_t)((((x) & 0xfff00000) >> 12) | \
			       (((x) & 0x000000ff) >>  0)))
#define makedev(x,y)    ((dev_t)((((x) <<  8) & 0x000fff00) | \
                                 (((y) << 12) & 0xfff00000) | \
                                 (((y) <<  0) & 0x000000ff)))
#define __attribute__(x)
#endif	/* MAKE_BOOTSTRAP */

#undef setprogname
#undef getprogname
extern const char *progname;
#define	setprogname(s)	((void)(progname = (s)))
#define	getprogname()	(progname)

#define ARRCHR '#'

/*
 * The next two lines define the current version of the config(1) binary,
 * and the minimum version of the configuration files it supports.
 */
#define CONFIG_VERSION		20150846
#define CONFIG_MINVERSION	0

/*
 * Name/value lists.  Values can be strings or pointers and/or can carry
 * integers.  The names can be NULL, resulting in simple value lists.
 */
struct nvlist {
	struct nvlist	*nv_next;
	const char	*nv_name;
	const char	*nv_str;
	void		*nv_ptr;
	long long	nv_num;
	int		nv_ifunit;		/* XXX XXX XXX */
	int		nv_flags;
#define	NV_DEPENDED	1
};

/*
 * Kernel configurations.
 */
struct config {
	TAILQ_ENTRY(config) cf_next;
	const char *cf_name;		/* "netbsd" */
	int	cf_lineno;		/* source line */
	const char *cf_fstype;		/* file system type */
	struct	nvlist *cf_root;	/* "root on ra0a" */
	struct	nvlist *cf_dump;	/* "dumps on ra0b" */
};

/*
 * Option definition list
 */
struct defoptlist {
	struct defoptlist *dl_next;
	const char *dl_name;
	const char *dl_value;
	const char *dl_lintvalue;
	int dl_obsolete;
	struct nvlist *dl_depends;
};

struct files;
TAILQ_HEAD(filelist, files);

struct module {
	const char		*m_name;
#if 1
	struct attrlist		*m_deps;
#else
	struct attrlist		*m_attrs;
	struct modulelist	*m_deps;
#endif
	int			m_expanding;
	struct filelist		m_files;
	int			m_weight;
};

/*
 * Attributes.  These come in three flavors: "plain", "device class,"
 * and "interface".  Plain attributes (e.g., "ether") simply serve
 * to pull in files.  Device class attributes are like plain
 * attributes, but additionally specify a device class (e.g., the
 * "disk" device class attribute specifies that devices with the
 * attribute belong to the "DV_DISK" class) and are mutually exclusive.
 * Interface attributes (e.g., "scsi") carry three lists: locators,
 * child devices, and references.  The locators are those things
 * that must be specified in order to configure a device instance
 * using this attribute (e.g., "tg0 at scsi0").  The a_devs field
 * lists child devices that can connect here (e.g., "tg"s), while
 * the a_refs are parents that carry the attribute (e.g., actual
 * SCSI host adapter drivers such as the SPARC "esp").
 */
struct attr {
	/* XXX */
	struct module a_m;
#define	a_name		a_m.m_name
#define	a_deps		a_m.m_deps
#define	a_expanding	a_m.m_expanding
#define	a_files		a_m.m_files
#define	a_weight	a_m.m_weight

	/* "interface attribute" */
	int	a_iattr;		/* true => allows children */
	struct	loclist *a_locs;	/* locators required */
	int	a_loclen;		/* length of above list */
	struct	nvlist *a_devs;		/* children */
	struct	nvlist *a_refs;		/* parents */

	/* "device class" */
	const char *a_devclass;		/* device class described */
};

/*
 * List of attributes.
 */
struct attrlist {
	struct attrlist *al_next;
	struct attr *al_this;
};

/*
 * List of locators. (Either definitions or uses...)
 *
 * XXX it would be nice if someone could clarify wtf ll_string and ll_num
 * are actually holding. (This stuff was previously stored in a very ad
 * hoc fashion, and the code is far from clear.)
 */
struct loclist {
	const char *ll_name;
	const char *ll_string;
	long long ll_num;
	struct loclist *ll_next;
};

/*
 * Parent specification.  Multiple device instances may share a
 * given parent spec.  Parent specs are emitted only if there are
 * device instances which actually reference it.
 */
struct pspec {
	TAILQ_ENTRY(pspec) p_list;	/* link on parent spec list */
	struct	attr *p_iattr;		/* interface attribute of parent */
	struct	devbase *p_atdev;	/* optional parent device base */
	int	p_atunit;		/* optional parent device unit */
	struct	nvlist *p_devs;		/* children using it */
	int	p_inst;			/* parent spec instance */
	int	p_active;		/* parent spec is actively used */
};

/*
 * The "base" part (struct devbase) of a device ("uba", "sd"; but not
 * "uba2" or "sd0").  It may be found "at" one or more attributes,
 * including "at root" (this is represented by a NULL attribute), as
 * specified by the device attachments (struct deva).
 *
 * Each device may also export attributes.  If any provide an output
 * interface (e.g., "esp" provides "scsi"), other devices (e.g.,
 * "tg"s) can be found at instances of this one (e.g., "esp"s).
 * Such a connection must provide locators as specified by that
 * interface attribute (e.g., "target").  The base device can
 * export both output (aka `interface') attributes, as well as
 * import input (`plain') attributes.  Device attachments may
 * only import input attributes; it makes no sense to have a
 * specific attachment export a new interface to other devices.
 *
 * Each base carries a list of instances (via d_ihead).  Note that this
 * list "skips over" aliases; those must be found through the instances
 * themselves.  Each base also carries a list of possible attachments,
 * each of which specify a set of devices that the device can attach
 * to, as well as the device instances that are actually using that
 * attachment.
 */
struct devbase {
	const char *d_name;		/* e.g., "sd" */
	TAILQ_ENTRY(devbase) d_next;
	int	d_isdef;		/* set once properly defined */
	int	d_ispseudo;		/* is a pseudo-device */
	devmajor_t d_major;		/* used for "root on sd0", e.g. */
	struct	attrlist *d_attrs;	/* attributes, if any */
	int	d_umax;			/* highest unit number + 1 */
	struct	devi *d_ihead;		/* first instance, if any */
	struct	devi **d_ipp;		/* used for tacking on more instances */
	struct	deva *d_ahead;		/* first attachment, if any */
	struct	deva **d_app;		/* used for tacking on attachments */
	struct	attr *d_classattr;	/* device class attribute (if any) */
};

struct deva {
	const char *d_name;		/* name of attachment, e.g. "com_isa" */
	TAILQ_ENTRY(deva) d_next;	/* list of all instances */
	struct	deva *d_bsame;		/* list on same base */
	int	d_isdef;		/* set once properly defined */
	struct	devbase *d_devbase;	/* the base device */
	struct	nvlist *d_atlist;	/* e.g., "at tg" (attr list) */
	struct	attrlist *d_attrs;	/* attributes, if any */
	struct	devi *d_ihead;		/* first instance, if any */
	struct	devi **d_ipp;		/* used for tacking on more instances */
};

/*
 * An "instance" of a device.  The same instance may be listed more
 * than once, e.g., "xx0 at isa? port FOO" + "xx0 at isa? port BAR".
 *
 * After everything has been read in and verified, the devi's are
 * "packed" to collect all the information needed to generate ioconf.c.
 * In particular, we try to collapse multiple aliases into a single entry.
 * We then assign each "primary" (non-collapsed) instance a cfdata index.
 * Note that there may still be aliases among these.
 */
struct devi {
	/* created while parsing config file */
	const char *i_name;	/* e.g., "sd0" */
	int	i_unit;		/* unit from name, e.g., 0 */
	struct	devbase *i_base;/* e.g., pointer to "sd" base */
	TAILQ_ENTRY(devi) i_next; /* list of all instances */
	struct	devi *i_bsame;	/* list on same base */
	struct	devi *i_asame;	/* list on same base attachment */
	struct	devi *i_alias;	/* other aliases of this instance */
	const char *i_at;	/* where this is "at" (NULL if at root) */
	struct	pspec *i_pspec;	/* parent spec (NULL if at root) */
	struct	deva *i_atdeva;
	const char **i_locs;	/* locators (as given by pspec's iattr) */
	int	i_cfflags;	/* flags from config line */
	int	i_lineno;	/* line # in config, for later errors */
	const char *i_srcfile;	/* file it appears in */
	int	i_level;	/* position between negated instances */
	int	i_active;
#define	DEVI_ORPHAN	0	/* instance has no active parent */
#define	DEVI_ACTIVE	1	/* instance has an active parent */
#define	DEVI_IGNORED	2	/* instance's parent has been removed */
#define DEVI_BROKEN	3	/* instance is broken (syntax error) */
	int	i_pseudoroot;	/* instance is pseudoroot */

	/* created during packing or ioconf.c generation */
	short	i_collapsed;	/* set => this alias no longer needed */
	u_short	i_cfindex;	/* our index in cfdata */
	int	i_locoff;	/* offset in locators.vec */

};
/* special units */
#define	STAR	(-1)		/* unit number for, e.g., "sd*" */
#define	WILD	(-2)		/* unit number for, e.g., "sd?" */

/*
 * Files (*.c, *.S, or *.o).  This structure defines the common fields
 * between the two.
 */
struct files {
	TAILQ_ENTRY(files) fi_next;
	TAILQ_ENTRY(files) fi_snext;	/* per-suffix list */
	const char *fi_srcfile;	/* the name of the "files" file that got us */
	u_short	fi_srcline;	/* and the line number */
	u_char fi_flags;	/* as below */
	const char *fi_tail;	/* name, i.e., strrchr(fi_path, '/') + 1 */
	const char *fi_base;	/* tail minus ".c" (or whatever) */
	const char *fi_dir;	/* path to file */
	const char *fi_path;	/* full file path */
	const char *fi_prefix;	/* any file prefix */
	const char *fi_buildprefix;	/* prefix in builddir */
	int fi_suffix;		/* single char suffix */
	size_t fi_len;		/* path string length */
	struct condexpr *fi_optx; /* options expression */
	struct nvlist *fi_optf; /* flattened version of above, if needed */
	const char *fi_mkrule;	/* special make rule, if any */
	struct attr *fi_attr;	/* owner attr */
	int fi_order;		/* score of order in ${ALLFILES} */
	TAILQ_ENTRY(files) fi_anext;	/* next file in attr */
};

/* flags */
#define	FI_SEL		0x01	/* selected */
#define	FI_NEEDSCOUNT	0x02	/* needs-count */
#define	FI_NEEDSFLAG	0x04	/* needs-flag */
#define	FI_HIDDEN	0x08	/* obscured by other(s), base names overlap */

extern size_t nselfiles;
extern struct files **selfiles;

/*
 * Condition expressions.
 */

enum condexpr_types {
	CX_ATOM,
	CX_NOT,
	CX_AND,
	CX_OR,
};
struct condexpr {
	enum condexpr_types cx_type;
	union {
		const char *atom;
		struct condexpr *not;
		struct {
			struct condexpr *left;
			struct condexpr *right;
		} and, or;
	} cx_u;
};
#define cx_atom	cx_u.atom
#define cx_not	cx_u.not
#define cx_and	cx_u.and
#define cx_or	cx_u.or

/*
 * File/object prefixes.  These are arranged in a stack, and affect
 * the behavior of the source path.
 */

struct prefix;
SLIST_HEAD(prefixlist, prefix);

struct prefix {
	SLIST_ENTRY(prefix)	pf_next;	/* next prefix in stack */
	const char		*pf_prefix;	/* the actual prefix */
};

/*
 * Device major informations.
 */
struct devm {
	TAILQ_ENTRY(devm) dm_next;
	const char	*dm_srcfile;	/* the name of the "majors" file */
	u_short		dm_srcline;	/* the line number */
	const char	*dm_name;	/* [bc]devsw name */
	devmajor_t	dm_cmajor;	/* character major */
	devmajor_t	dm_bmajor;	/* block major */
	struct condexpr	*dm_opts;	/* options */
	struct nvlist	*dm_devnodes;	/* information on /dev nodes */
};

/*
 * Hash tables look up name=value pairs.  The pointer value of the name
 * is assumed to be constant forever; this can be arranged by interning
 * the name.  (This is fairly convenient since our lexer does this for
 * all identifier-like strings---it has to save them anyway, lest yacc's
 * look-ahead wipe out the current one.)
 */
struct hashtab;

int lkmmode;
const char *conffile;		/* source file, e.g., "GENERIC.sparc" */
const char *machine;		/* machine type, e.g., "sparc" or "sun3" */
const char *machinearch;	/* machine arch, e.g., "sparc" or "m68k" */
struct	nvlist *machinesubarches;
				/* machine subarches, e.g., "sun68k" or "hpc" */
const char *ioconfname;		/* ioconf name, mutually exclusive to machine */
const char *srcdir;		/* path to source directory (rel. to build) */
const char *builddir;		/* path to build directory */
const char *defbuilddir;	/* default build directory */
const char *ident;		/* kernel "ident"ification string */
int	errors;			/* counts calls to error() */
int	minmaxusers;		/* minimum "maxusers" parameter */
int	defmaxusers;		/* default "maxusers" parameter */
int	maxmaxusers;		/* default "maxusers" parameter */
int	maxusers;		/* configuration's "maxusers" parameter */
int	maxpartitions;		/* configuration's "maxpartitions" parameter */
int	version;		/* version of the configuration file */
struct	nvlist *options;	/* options */
struct	nvlist *fsoptions;	/* filesystems */
struct	nvlist *mkoptions;	/* makeoptions */
struct	nvlist *appmkoptions;	/* appending mkoptions */
struct	nvlist *condmkoptions;	/* conditional makeoption table */
struct	hashtab *devbasetab;	/* devbase lookup */
struct	hashtab *devroottab;	/* attach at root lookup */
struct	hashtab *devatab;	/* devbase attachment lookup */
struct	hashtab *devitab;	/* device instance lookup */
struct	hashtab *deaddevitab;	/* removed instances lookup */
struct	hashtab *selecttab;	/* selects things that are "optional foo" */
struct	hashtab *needcnttab;	/* retains names marked "needs-count" */
struct	hashtab *opttab;	/* table of configured options */
struct	hashtab *fsopttab;	/* table of configured file systems */
struct	dlhash *defopttab;	/* options that have been "defopt"'d */
struct	dlhash *defflagtab;	/* options that have been "defflag"'d */
struct	dlhash *defparamtab;	/* options that have been "defparam"'d */
struct	dlhash *defoptlint;	/* lint values for options */
struct	nvhash *deffstab;	/* defined file systems */
struct	dlhash *optfiletab;	/* "defopt"'d option .h files */
struct	hashtab *attrtab;	/* attributes (locators, etc.) */
struct	hashtab *attrdeptab;	/* attribute dependencies */
struct	hashtab *bdevmtab;	/* block devm lookup */
struct	hashtab *cdevmtab;	/* character devm lookup */

TAILQ_HEAD(, devbase)	allbases;	/* list of all devbase structures */
TAILQ_HEAD(, deva)	alldevas;	/* list of all devbase attachments */
TAILQ_HEAD(conftq, config) allcf;	/* list of configured kernels */
TAILQ_HEAD(, devi)	alldevi,	/* list of all instances */
			allpseudo;	/* list of all pseudo-devices */
TAILQ_HEAD(, devm)	alldevms;	/* list of all device-majors */
TAILQ_HEAD(, pspec)	allpspecs;	/* list of all parent specs */
int	ndevi;				/* number of devi's (before packing) */
int	npspecs;			/* number of parent specs */
devmajor_t maxbdevm;			/* max number of block major */
devmajor_t maxcdevm;			/* max number of character major */
int	do_devsw;			/* 0 if pre-devsw config */
int	oktopackage;			/* 0 before setmachine() */
int	devilevel;			/* used for devi->i_level */

struct filelist		allfiles;	/* list of all kernel source files */
struct filelist		allcfiles;	/* list of all .c files */
struct filelist		allsfiles;	/* list of all .S files */
struct filelist		allofiles;	/* list of all .o files */

struct prefixlist	prefixes,	/* prefix stack */
			allprefixes;	/* all prefixes used (after popped) */
struct prefixlist	buildprefixes,	/* build prefix stack */
			allbuildprefixes;/* all build prefixes used (after popped) */
SLIST_HEAD(, prefix)	curdirs;	/* curdir stack */

extern struct attr allattr;
struct	devi **packed;		/* arrayified table for packed devi's */
size_t	npacked;		/* size of packed table, <= ndevi */

struct {			/* loc[] table for config */
	const char **vec;
	int	used;
} locators;

struct numconst {
	int64_t	val;
	int fmt;
};

/* files.c */
void	initfiles(void);
void	checkfiles(void);
int	fixfiles(void);		/* finalize */
int	fixdevsw(void);
void	addfile(const char *, struct condexpr *, u_char, const char *);
int	expr_eval(struct condexpr *, int (*)(const char *, void *), void *);

/* hash.c */
struct	hashtab *ht_new(void);
void	ht_free(struct hashtab *);
int	ht_insrep2(struct hashtab *, const char *, const char *, void *, int);
int	ht_insrep(struct hashtab *, const char *, void *, int);
#define	ht_insert2(ht, nam1, nam2, val) ht_insrep2(ht, nam1, nam2, val, 0)
#define	ht_insert(ht, nam, val) ht_insrep(ht, nam, val, 0)
#define	ht_replace(ht, nam, val) ht_insrep(ht, nam, val, 1)
int	ht_remove2(struct hashtab *, const char *, const char *);
int	ht_remove(struct hashtab *, const char *);
void	*ht_lookup2(struct hashtab *, const char *, const char *);
void	*ht_lookup(struct hashtab *, const char *);
void	initintern(void);
const char *intern(const char *);
typedef int (*ht_callback2)(const char *, const char *, void *, void *);
typedef int (*ht_callback)(const char *, void *, void *);
int	ht_enumerate2(struct hashtab *, ht_callback2, void *);
int	ht_enumerate(struct hashtab *, ht_callback, void *);

/* typed hash, named struct HT, whose type is string -> struct VT */
#define DECLHASH(HT, VT) \
	struct HT;							\
	struct HT *HT##_create(void);					\
	int HT##_insert(struct HT *, const char *, struct VT *);	\
	int HT##_replace(struct HT *, const char *, struct VT *);	\
	int HT##_remove(struct HT *, const char *);			\
	struct VT *HT##_lookup(struct HT *, const char *);		\
	int HT##_enumerate(struct HT *,					\
			int (*)(const char *, struct VT *, void *),	\
			void *)
DECLHASH(nvhash, nvlist);
DECLHASH(dlhash, defoptlist);

/* lint.c */
void	emit_instances(void);
void	emit_options(void);
void	emit_params(void);

/* main.c */
extern	int Mflag;
extern	int Sflag;
void	addoption(const char *, const char *);
void	addfsoption(const char *);
void	addmkoption(const char *, const char *);
void	appendmkoption(const char *, const char *);
void	appendcondmkoption(struct condexpr *, const char *, const char *);
void	deffilesystem(struct nvlist *, struct nvlist *);
void	defoption(const char *, struct defoptlist *, struct nvlist *);
void	defflag(const char *, struct defoptlist *, struct nvlist *, int);
void	defparam(const char *, struct defoptlist *, struct nvlist *, int);
void	deloption(const char *);
void	delfsoption(const char *);
void	delmkoption(const char *);
int	devbase_has_instances(struct devbase *, int);
int	is_declared_option(const char *);
int	deva_has_instances(struct deva *, int);
void	setupdirs(void);
void	fixmaxusers(void);
void	fixmkoption(void);
const char *strtolower(const char *);

/* tests on option types */
#define OPT_FSOPT(n)	(nvhash_lookup(deffstab, (n)) != NULL)
#define OPT_DEFOPT(n)	(dlhash_lookup(defopttab, (n)) != NULL)
#define OPT_DEFFLAG(n)	(dlhash_lookup(defflagtab, (n)) != NULL)
#define OPT_DEFPARAM(n)	(dlhash_lookup(defparamtab, (n)) != NULL)
#define OPT_OBSOLETE(n)	(dlhash_lookup(obsopttab, (n)) != NULL)
#define DEFINED_OPTION(n) (is_declared_option((n)))

/* main.c */
void	logconfig_include(FILE *, const char *);

/* mkdevsw.c */
int	mkdevsw(void);

/* mkheaders.c */
int	mkheaders(void);
int	moveifchanged(const char *, const char *);
int	emitlocs(void);
int	emitioconfh(void);

/* mkioconf.c */
int	mkioconf(void);

/* mkmakefile.c */
int	mkmakefile(void);

/* mkswap.c */
int	mkswap(void);

/* pack.c */
void	pack(void);

/* scan.l */
u_short	currentline(void);
int	firstfile(const char *);
void	package(const char *);
int	include(const char *, int, int, int);
extern int includedepth;

/* sem.c, other than for yacc actions */
void	initsem(void);
int	onlist(struct nvlist *, void *);

/* util.c */
void	prefix_push(const char *);
void	prefix_pop(void);
void	buildprefix_push(const char *);
void	buildprefix_pop(void);
char	*sourcepath(const char *);
extern	int dflag;
#define	CFGDBG(n, ...) \
	do { if ((dflag) >= (n)) cfgdbg(__VA_ARGS__); } while (0)
void	cfgdbg(const char *, ...)			/* debug info */
     __printflike(1, 2);
void	cfgwarn(const char *, ...)			/* immediate warns */
     __printflike(1, 2);
void	cfgxwarn(const char *, int, const char *, ...)	/* delayed warns */
     __printflike(3, 4);
void	cfgerror(const char *, ...)			/* immediate errs */
     __printflike(1, 2);
void	cfgxerror(const char *, int, const char *, ...)	/* delayed errs */
     __printflike(3, 4);
__dead void panic(const char *, ...)
     __printflike(1, 2);
struct nvlist *newnv(const char *, const char *, void *, long long, struct nvlist *);
void	nvfree(struct nvlist *);
void	nvfreel(struct nvlist *);
struct nvlist *nvcat(struct nvlist *, struct nvlist *);
void	autogen_comment(FILE *, const char *);
struct defoptlist *defoptlist_create(const char *, const char *, const char *);
void defoptlist_destroy(struct defoptlist *);
struct defoptlist *defoptlist_append(struct defoptlist *, struct defoptlist *);
struct attrlist *attrlist_create(void);
struct attrlist *attrlist_cons(struct attrlist *, struct attr *);
void attrlist_destroy(struct attrlist *);
void attrlist_destroyall(struct attrlist *);
struct loclist *loclist_create(const char *, const char *, long long);
void loclist_destroy(struct loclist *);
struct condexpr *condexpr_create(enum condexpr_types);
void condexpr_destroy(struct condexpr *);

/* liby */
void	yyerror(const char *);
int	yylex(void);

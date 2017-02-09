/*	$NetBSD: main.c,v 1.89 2015/09/04 06:01:40 uebayasi Exp $	*/

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
 *	from: @(#)main.c	8.1 (Berkeley) 6/6/93
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
__RCSID("$NetBSD: main.c,v 1.89 2015/09/04 06:01:40 uebayasi Exp $");

#ifndef MAKE_BOOTSTRAP
#include <sys/cdefs.h>
#define	COPYRIGHT(x)	__COPYRIGHT(x)
#else
#define	COPYRIGHT(x)	static const char copyright[] = x
#endif

#ifndef lint
COPYRIGHT("@(#) Copyright (c) 1992, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mman.h>
#if !HAVE_NBTOOL_CONFIG_H
#include <sys/sysctl.h>
#endif
#include <paths.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>
#include <util.h>

#include "defs.h"
#include "sem.h"

#ifndef LINE_MAX
#define LINE_MAX 1024
#endif

int	vflag;				/* verbose output */
int	Pflag;				/* pack locators */
int	Lflag;				/* lint config generation */
int	Mflag;				/* modular build */
int	Sflag;				/* suffix rules & subdirectory */
int	handling_cmdlineopts;		/* currently processing -D/-U options */

int	yyparse(void);

#ifndef MAKE_BOOTSTRAP
extern int yydebug;
#endif
int	dflag;

static struct dlhash *obsopttab;
static struct hashtab *mkopttab;
static struct nvlist **nextopt;
static struct nvlist **nextmkopt;
static struct nvlist **nextappmkopt;
static struct nvlist **nextcndmkopt;
static struct nvlist **nextfsopt;
static struct nvlist *cmdlinedefs, *cmdlineundefs;

static	void	usage(void) __dead;
static	void	dependopts(void);
static	void	dependopts_one(const char *);
static	void	do_depends(struct nvlist *);
static	void	do_depend(struct nvlist *);
static	void	stop(void);
static	int	do_option(struct hashtab *, struct nvlist **,
		    struct nvlist ***, const char *, const char *,
		    const char *, struct hashtab *);
static	int	undo_option(struct hashtab *, struct nvlist **,
		    struct nvlist ***, const char *, const char *);
static	int	crosscheck(void);
static	int	badstar(void);
	int	main(int, char **);
static	int	mkallsubdirs(void);
static	int	mksymlinks(void);
static	int	mkident(void);
static	int	devbase_has_dead_instances(const char *, void *, void *);
static	int	devbase_has_any_instance(struct devbase *, int, int, int);
static	int	check_dead_devi(const char *, void *, void *);
static	void	add_makeopt(const char *);
static	void	remove_makeopt(const char *);
static	void	handle_cmdline_makeoptions(void);
static	void	kill_orphans(void);
static	void	do_kill_orphans(struct devbase *, struct attr *,
    struct devbase *, int);
static	int	kill_orphans_cb(const char *, void *, void *);
static	int	cfcrosscheck(struct config *, const char *, struct nvlist *);
static void	defopt(struct dlhash *ht, const char *fname,
	     struct defoptlist *opts, struct nvlist *deps, int obs);
static struct defoptlist *find_declared_option_option(const char *name);
static struct nvlist *find_declared_fs_option(const char *name);

#define LOGCONFIG_LARGE "INCLUDE_CONFIG_FILE"
#define LOGCONFIG_SMALL "INCLUDE_JUST_CONFIG"

static	void	logconfig_start(void);
static	void	logconfig_end(void);
static	FILE	*cfg;
static	time_t	cfgtime;

static	int	is_elf(const char *);
static	int	extract_config(const char *, const char *, int);

int badfilename(const char *fname);

const char *progname;
extern const char *yyfile;

int
main(int argc, char **argv)
{
	char *p, cname[PATH_MAX];
	const char *last_component;
	int pflag, xflag, ch, removeit;

	setprogname(argv[0]);

	pflag = 0;
	xflag = 0;
	while ((ch = getopt(argc, argv, "D:LMPSU:dgpvb:s:x")) != -1) {
		switch (ch) {

		case 'd':
#ifndef MAKE_BOOTSTRAP
			yydebug = 1;
#endif
			dflag++;
			break;

		case 'M':
			Mflag = 1;
			break;

		case 'L':
			Lflag = 1;
			break;

		case 'P':
			Pflag = 1;
			break;

		case 'g':
			/*
			 * In addition to DEBUG, you probably wanted to
			 * set "options KGDB" and maybe others.  We could
			 * do that for you, but you really should just
			 * put them in the config file.
			 */
			warnx("-g is obsolete (use -D DEBUG=\"-g\")");
			usage();
			/*NOTREACHED*/

		case 'p':
			/*
			 * Essentially the same as makeoptions PROF="-pg",
			 * but also changes the path from ../../compile/FOO
			 * to ../../compile/FOO.PROF; i.e., compile a
			 * profiling kernel based on a typical "regular"
			 * kernel.
			 *
			 * Note that if you always want profiling, you
			 * can (and should) use a "makeoptions" line.
			 */
			pflag = 1;
			break;

		case 'v':
			vflag = 1;
			break;

		case 'b':
			builddir = optarg;
			break;

		case 's':
			srcdir = optarg;
			break;

		case 'S':
			Sflag = 1;
			break;

		case 'x':
			xflag = 1;
			break;

		case 'D':
			add_makeopt(optarg);
			break;

		case 'U':
			remove_makeopt(optarg);
			break;

		case '?':
		default:
			usage();
		}
	}

	if (xflag && optind != 2) {
		errx(EXIT_FAILURE, "-x must be used alone");
	}

	argc -= optind;
	argv += optind;
	if (argc > 1) {
		usage();
	}

	if (Lflag && (builddir != NULL || Pflag || pflag))
		errx(EXIT_FAILURE, "-L can only be used with -s and -v");

	if (xflag) {
		if (argc == 0) {
#if !HAVE_NBTOOL_CONFIG_H
			char path_unix[MAXPATHLEN];
			size_t len = sizeof(path_unix) - 1;
			path_unix[0] = '/';

			conffile = sysctlbyname("machdep.booted_kernel",
			    &path_unix[1], &len, NULL, 0) == -1 ? _PATH_UNIX :
			    path_unix;
#else
			errx(EXIT_FAILURE, "no kernel supplied");
#endif
		} else
			conffile = argv[0];
		if (!is_elf(conffile))
			errx(EXIT_FAILURE, "%s: not a binary kernel",
			    conffile);
		if (!extract_config(conffile, "stdout", STDOUT_FILENO))
			errx(EXIT_FAILURE, "%s does not contain embedded "
			    "configuration data", conffile);
		exit(0);
	}

	conffile = (argc == 1) ? argv[0] : "CONFIG";
	if (firstfile(conffile)) {
		err(EXIT_FAILURE, "Cannot read `%s'", conffile);
		exit(2);
	}

	/*
	 * Init variables.
	 */
	minmaxusers = 1;
	maxmaxusers = 10000;
	initintern();
	ident = NULL;
	devbasetab = ht_new();
	devroottab = ht_new();
	devatab = ht_new();
	devitab = ht_new();
	deaddevitab = ht_new();
	selecttab = ht_new();
	needcnttab = ht_new();
	opttab = ht_new();
	mkopttab = ht_new();
	fsopttab = ht_new();
	deffstab = nvhash_create();
	defopttab = dlhash_create();
	defparamtab = dlhash_create();
	defoptlint = dlhash_create();
	defflagtab = dlhash_create();
	optfiletab = dlhash_create();
	obsopttab = dlhash_create();
	bdevmtab = ht_new();
	maxbdevm = 0;
	cdevmtab = ht_new();
	maxcdevm = 0;
	nextopt = &options;
	nextmkopt = &mkoptions;
	nextappmkopt = &appmkoptions;
	nextcndmkopt = &condmkoptions;
	nextfsopt = &fsoptions;
	initfiles();
	initsem();

	/*
	 * Handle profiling (must do this before we try to create any
	 * files).
	 */
	last_component = strrchr(conffile, '/');
	last_component = (last_component) ? last_component + 1 : conffile;
	if (pflag) {
		p = emalloc(strlen(last_component) + 17);
		(void)sprintf(p, "../compile/%s.PROF", last_component);
		(void)addmkoption(intern("PROF"), "-pg");
		(void)addoption(intern("GPROF"), NULL);
	} else {
		p = emalloc(strlen(last_component) + 13);
		(void)sprintf(p, "../compile/%s", last_component);
	}
	defbuilddir = (argc == 0) ? "." : p;

	if (Lflag) {
		char resolvedname[MAXPATHLEN];

		if (realpath(conffile, resolvedname) == NULL)
			err(EXIT_FAILURE, "realpath(%s)", conffile);

		if (yyparse())
			stop();

		printf("include \"%s\"\n", resolvedname);

		emit_params();
		emit_options();
		emit_instances();

		exit(EXIT_SUCCESS);
	}

	removeit = 0;
	if (is_elf(conffile)) {
		const char *tmpdir;
		int cfd;

		if (builddir == NULL)
			errx(EXIT_FAILURE, "Build directory must be specified "
			    "with binary kernels");

		/* Open temporary configuration file */
		tmpdir = getenv("TMPDIR");
		if (tmpdir == NULL)
			tmpdir = _PATH_TMP;
		snprintf(cname, sizeof(cname), "%s/config.tmp.XXXXXX", tmpdir);
		cfd = mkstemp(cname);
		if (cfd == -1)
			err(EXIT_FAILURE, "Cannot create `%s'", cname);

		printf("Using configuration data embedded in kernel...\n");
		if (!extract_config(conffile, cname, cfd)) {
			unlink(cname);
			errx(EXIT_FAILURE, "%s does not contain embedded "
			    "configuration data", conffile);
		}

		removeit = 1;
		close(cfd);
		firstfile(cname);
	}

	 /*
	  * Log config file.  We don't know until yyparse() if we're
	  * going to need config_file.h (i.e. if we're doing ioconf-only
	  * or not).  Just start creating the file, and when we know
	  * later, we'll just keep or discard our work here.
	  */
	logconfig_start();

	/*
	 * Parse config file (including machine definitions).
	 */
	if (yyparse())
		stop();

	if (ioconfname && cfg)
		fclose(cfg);
	else
		logconfig_end();

	if (removeit)
		unlink(cname);

	/*
	 * Handle command line overrides
	 */
	yyfile = "handle_cmdline_makeoptions";
	handle_cmdline_makeoptions();

	/*
	 * Detect and properly ignore orphaned devices
	 */
	yyfile = "kill_orphans";
	kill_orphans();

	/*
	 * Select devices and pseudo devices and their attributes
	 */
	yyfile = "fixdevis";
	if (fixdevis())
		stop();

	/*
	 * Copy maxusers to param.
	 */
	yyfile = "fixmaxusers";
	fixmaxusers();

	/*
	 * Copy makeoptions to params
	 */
	yyfile = "fixmkoption";
	fixmkoption();

	/*
	 * If working on an ioconf-only config, process here and exit
	 */
	if (ioconfname) {
		yyfile = "pack";
		pack();
		yyfile = "mkioconf";
		mkioconf();
		yyfile = "emitlocs";
		emitlocs();
		yyfile = "emitioconfh";
		emitioconfh();
		return 0;
	}

	yyfile = "dependattrs";
	dependattrs();

	/*
	 * Deal with option dependencies.
	 */
	yyfile = "dependopts";
	dependopts();

	/*
	 * Fix (as in `set firmly in place') files.
	 */
	yyfile = "fixfiles";
	if (fixfiles())
		stop();

	/*
	 * Fix device-majors.
	 */
	yyfile = "fixdevsw";
	if (fixdevsw())
		stop();

	/*
	 * Perform cross-checking.
	 */
	if (maxusers == 0) {
		if (defmaxusers) {
			(void)printf("maxusers not specified; %d assumed\n",
			    defmaxusers);
			maxusers = defmaxusers;
		} else {
			warnx("need \"maxusers\" line");
			errors++;
		}
	}
	if (crosscheck() || errors)
		stop();

	/*
	 * Squeeze things down and finish cross-checks (STAR checks must
	 * run after packing).
	 */
	yyfile = "pack";
	pack();
	yyfile = "badstar";
	if (badstar())
		stop();

	yyfile = NULL;
	/*
	 * Ready to go.  Build all the various files.
	 */
	if ((Sflag && mkallsubdirs()) || mksymlinks() || mkmakefile() || mkheaders() || mkswap() ||
	    mkioconf() || (do_devsw ? mkdevsw() : 0) || mkident() || errors)
		stop();
	(void)printf("Build directory is %s\n", builddir);
	(void)printf("Don't forget to run \"make depend\"\n");

	return 0;
}

static void
usage(void)
{
	(void)fprintf(stderr, "Usage: %s [-Ppv] [-b builddir] [-D var=value] "
	    "[-s srcdir] [-U var] "
	    "[config-file]\n\t%s -x [kernel-file]\n"
	    "\t%s -L [-v] [-s srcdir] [config-file]\n", 
	    getprogname(), getprogname(), getprogname());
	exit(1);
}

/*
 * Set any options that are implied by other options.
 */
static void
dependopts(void)
{
	struct nvlist *nv;

	for (nv = options; nv != NULL; nv = nv->nv_next) {
		dependopts_one(nv->nv_name);
	}

	for (nv = fsoptions; nv != NULL; nv = nv->nv_next) {
		dependopts_one(nv->nv_name);
	}
}

static void
dependopts_one(const char *name)
{
	struct defoptlist *dl;
	struct nvlist *fs;

	dl = find_declared_option_option(name);
	if (dl != NULL) {
		do_depends(dl->dl_depends);
	}
	fs = find_declared_fs_option(name);
	if (fs != NULL) {
		do_depends(fs->nv_ptr);
	}

	CFGDBG(3, "depend `%s' searched", name);
}

static void
do_depends(struct nvlist *nv)
{
	struct nvlist *opt;

	for (opt = nv; opt != NULL; opt = opt->nv_next) {
		do_depend(opt);
	}
}

static void
do_depend(struct nvlist *nv)
{
	struct attr *a;

	if (nv != NULL && (nv->nv_flags & NV_DEPENDED) == 0) {
		nv->nv_flags |= NV_DEPENDED;
		/*
		 * If the dependency is an attribute, then just add
		 * it to the selecttab.
		 */
		CFGDBG(3, "depend attr `%s'", nv->nv_name);
		if ((a = ht_lookup(attrtab, nv->nv_name)) != NULL) {
			if (a->a_iattr)
				panic("do_depend(%s): dep `%s' is an iattr",
				    nv->nv_name, a->a_name);
			expandattr(a, selectattr);
		} else {
			if (ht_lookup(opttab, nv->nv_name) == NULL)
				addoption(nv->nv_name, NULL);
			dependopts_one(nv->nv_name);
		}
	}
}

static int
recreate(const char *p, const char *q)
{
	int ret;

	if ((ret = unlink(q)) == -1 && errno != ENOENT)
		warn("unlink(%s)", q);
	if ((ret = symlink(p, q)) == -1)
		warn("symlink(%s -> %s)", q, p);
	return ret;
}

static void
mksubdir(char *buf)
{
	char *p;
	struct stat st;

	p = strrchr(buf, '/');
	if (p != NULL && *p == '/') {
		*p = '\0';
		mksubdir(buf);
		*p = '/';
	}
	if (stat(buf, &st) == 0) {
		if (!S_ISDIR(st.st_mode))
			errx(EXIT_FAILURE, "not directory %s", buf);
	} else
		if (mkdir(buf, 0777) == -1)
			errx(EXIT_FAILURE, "cannot create %s", buf);
}

static int
mksubdirs(struct filelist *fl)
{
	struct files *fi;
	const char *prologue, *prefix, *sep;
	char buf[MAXPATHLEN];

	TAILQ_FOREACH(fi, fl, fi_next) {
		if ((fi->fi_flags & FI_SEL) == 0)
			continue;
		prefix = sep = "";
		if (fi->fi_buildprefix != NULL) {
			prefix = fi->fi_buildprefix;
			sep = "/";
		} else {
			if (fi->fi_prefix != NULL) {
				prefix = fi->fi_prefix;
				sep = "/";
			}
		}
		snprintf(buf, sizeof(buf), "%s%s%s", prefix, sep, fi->fi_dir);
		if (buf[0] == '\0')
			continue;
		mksubdir(buf);
		if (fi->fi_prefix != NULL && fi->fi_buildprefix != NULL) {
			char org[MAXPATHLEN];

			if (fi->fi_prefix[0] == '/') {
				prologue = "";
				sep = "";
			} else {
				prologue = srcdir;
				sep = "/";
			}
			snprintf(buf, sizeof(buf), "%s%s%s",
			    fi->fi_buildprefix, "/", fi->fi_path);
			snprintf(org, sizeof(org), "%s%s%s%s%s",
			    prologue, sep, fi->fi_prefix, "/", fi->fi_path);
			recreate(org, buf);
			fi->fi_prefix = fi->fi_buildprefix;
			fi->fi_buildprefix = NULL;
		}
	}

	return 0;
}

static int
mkallsubdirs(void)
{

	mksubdirs(&allfiles);
	mksubdirs(&allofiles);
	return 0;
}

/*
 * Make a symlink for "machine" so that "#include <machine/foo.h>" works,
 * and for the machine's CPU architecture, so that works as well.
 */
static int
mksymlinks(void)
{
	int ret;
	char *p, buf[MAXPATHLEN];
	const char *q;
	struct nvlist *nv;

	p = buf;

	snprintf(buf, sizeof(buf), "%s/arch/%s/include", srcdir, machine);
	ret = recreate(p, "machine");
	ret = recreate(p, machine);

	if (machinearch != NULL) {
		snprintf(buf, sizeof(buf), "%s/arch/%s/include", srcdir, machinearch);
		q = machinearch;
	} else {
		snprintf(buf, sizeof(buf), "machine");
		q = machine;
	}

	ret = recreate(p, q);

	for (nv = machinesubarches; nv != NULL; nv = nv->nv_next) {
		q = nv->nv_name;
		snprintf(buf, sizeof(buf), "%s/arch/%s/include", srcdir, q);
		ret = recreate(p, q);
	}

	return (ret);
}

static __dead void
stop(void)
{
	(void)fprintf(stderr, "*** Stop.\n");
	exit(1);
}

static void
check_dependencies(const char *thing, struct nvlist *deps)
{
	struct nvlist *dep;
	struct attr *a;

	for (dep = deps; dep != NULL; dep = dep->nv_next) {
		/*
		 * If the dependency is an attribute, it must not
		 * be an interface attribute.  Otherwise, it must
		 * be a previously declared option.
		 */
		if ((a = ht_lookup(attrtab, dep->nv_name)) != NULL) {
			if (a->a_iattr)
				cfgerror("option `%s' dependency `%s' "
				    "is an interface attribute",
				    thing, a->a_name);
		} else if (OPT_OBSOLETE(dep->nv_name)) {
			cfgerror("option `%s' dependency `%s' "
			    "is obsolete", thing, dep->nv_name);
		} else if (!is_declared_option(dep->nv_name)) {
			cfgerror("option `%s' dependency `%s' "
			    "is an unknown option",
			    thing, dep->nv_name);
		}
	}
}

static void
add_fs_dependencies(struct nvlist *nv, struct nvlist *deps)
{
	/* Use nv_ptr to link any other options that are implied. */
	nv->nv_ptr = deps;
	check_dependencies(nv->nv_name, deps);
}

static void
add_opt_dependencies(struct defoptlist *dl, struct nvlist *deps)
{
	dl->dl_depends = deps;
	check_dependencies(dl->dl_name, deps);
}

/*
 * Define one or more file systems.
 */
void
deffilesystem(struct nvlist *fses, struct nvlist *deps)
{
	struct nvlist *nv;

	/*
	 * Mark these options as ones to skip when creating the Makefile.
	 */
	for (nv = fses; nv != NULL; nv = nv->nv_next) {
		if (DEFINED_OPTION(nv->nv_name)) {
			cfgerror("file system or option `%s' already defined",
			    nv->nv_name);
			return;
		}

		/*
		 * Also mark it as a valid file system, which may be
		 * used in "file-system" directives in the config
		 * file.
		 */
		if (nvhash_insert(deffstab, nv->nv_name, nv))
			panic("file system `%s' already in table?!",
			    nv->nv_name);

		add_fs_dependencies(nv, deps);

		/*
		 * Implicit attribute definition for filesystem.
		 */
		const char *n; 
		n = strtolower(nv->nv_name);
		refattr(n);
	}
}

/*
 * Sanity check a file name.
 */
int
badfilename(const char *fname)
{
	const char *n;

	/*
	 * We're putting multiple options into one file.  Sanity
	 * check the file name.
	 */
	if (strchr(fname, '/') != NULL) {
		cfgerror("option file name contains a `/'");
		return 1;
	}
	if ((n = strrchr(fname, '.')) == NULL || strcmp(n, ".h") != 0) {
		cfgerror("option file name does not end in `.h'");
		return 1;
	}
	return 0;
}


/*
 * Search for a defined option (defopt, filesystem, etc), and if found,
 * return the option's struct nvlist.
 *
 * This used to be one function (find_declared_option) before options
 * and filesystems became different types.
 */
static struct defoptlist *
find_declared_option_option(const char *name)
{
	struct defoptlist *option;

	if ((option = dlhash_lookup(defopttab, name)) != NULL ||
	    (option = dlhash_lookup(defparamtab, name)) != NULL ||
	    (option = dlhash_lookup(defflagtab, name)) != NULL) {
		return (option);
	}

	return (NULL);
}

static struct nvlist *
find_declared_fs_option(const char *name)
{
	struct nvlist *fs;

	if ((fs = nvhash_lookup(deffstab, name)) != NULL) {
		return fs;
	}

	return (NULL);
}

/*
 * Like find_declared_option but doesn't return what it finds, so it
 * can search both the various kinds of options and also filesystems.
 */
int
is_declared_option(const char *name)
{
	struct defoptlist *option = NULL;
	struct nvlist *fs;

	if ((option = dlhash_lookup(defopttab, name)) != NULL ||
	    (option = dlhash_lookup(defparamtab, name)) != NULL ||
	    (option = dlhash_lookup(defflagtab, name)) != NULL) {
		return 1;
	}
	if ((fs = nvhash_lookup(deffstab, name)) != NULL) {
		return 1;
	}

	return 0;
}

/*
 * Define one or more standard options.  If an option file name is specified,
 * place all options in one file with the specified name.  Otherwise, create
 * an option file for each option.
 * record the option information in the specified table.
 */
void
defopt(struct dlhash *ht, const char *fname, struct defoptlist *opts,
       struct nvlist *deps, int obs)
{
	struct defoptlist *dl, *nextdl, *olddl;
	const char *name;
	char buf[500];

	if (fname != NULL && badfilename(fname)) {
		return;
	}

	/*
	 * Mark these options as ones to skip when creating the Makefile.
	 */
	for (dl = opts; dl != NULL; dl = nextdl) {
		nextdl = dl->dl_next;

		if (dl->dl_lintvalue != NULL) {
			/*
			 * If an entry already exists, then we are about to
			 * complain, so no worry.
			 */
			(void) dlhash_insert(defoptlint, dl->dl_name,
			    dl);
		}

		/* An option name can be declared at most once. */
		if (DEFINED_OPTION(dl->dl_name)) {
			cfgerror("file system or option `%s' already defined",
			    dl->dl_name);
			return;
		}

		if (dlhash_insert(ht, dl->dl_name, dl)) {
			cfgerror("file system or option `%s' already defined",
			    dl->dl_name);
			return;
		}

		if (fname == NULL) {
			/*
			 * Each option will be going into its own file.
			 * Convert the option name to lower case.  This
			 * lower case name will be used as the option
			 * file name.
			 */
			(void) snprintf(buf, sizeof(buf), "opt_%s.h",
			    strtolower(dl->dl_name));
			name = intern(buf);
		} else {
			name = fname;
		}

		add_opt_dependencies(dl, deps);

		/*
		 * Remove this option from the parameter list before adding
		 * it to the list associated with this option file.
		 */
		dl->dl_next = NULL;

		/*
		 * Flag as obsolete, if requested.
		 */
		if (obs) {
			dl->dl_obsolete = 1;
			(void)dlhash_insert(obsopttab, dl->dl_name, dl);
		}

		/*
		 * Add this option file if we haven't seen it yet.
		 * Otherwise, append to the list of options already
		 * associated with this file.
		 */
		if ((olddl = dlhash_lookup(optfiletab, name)) == NULL) {
			(void)dlhash_insert(optfiletab, name, dl);
		} else {
			while (olddl->dl_next != NULL)
				olddl = olddl->dl_next;
			olddl->dl_next = dl;
		}
	}
}

/*
 * Define one or more standard options.  If an option file name is specified,
 * place all options in one file with the specified name.  Otherwise, create
 * an option file for each option.
 */
void
defoption(const char *fname, struct defoptlist *opts, struct nvlist *deps)
{

	cfgwarn("The use of `defopt' is deprecated");
	defopt(defopttab, fname, opts, deps, 0);
}


/*
 * Define an option for which a value is required. 
 */
void
defparam(const char *fname, struct defoptlist *opts, struct nvlist *deps, int obs)
{

	defopt(defparamtab, fname, opts, deps, obs);
}

/*
 * Define an option which must not have a value, and which
 * emits a "needs-flag" style output.
 */
void
defflag(const char *fname, struct defoptlist *opts, struct nvlist *deps, int obs)
{

	defopt(defflagtab, fname, opts, deps, obs);
}


/*
 * Add an option from "options FOO".  Note that this selects things that
 * are "optional foo".
 */
void
addoption(const char *name, const char *value)
{
	const char *n;
	int is_fs, is_param, is_flag, is_undecl, is_obs;

	/* 
	 * Figure out how this option was declared (if at all.)
	 * XXX should use "params" and "flags" in config.
	 * XXX crying out for a type field in a unified hashtab.
	 */
	is_fs = OPT_FSOPT(name);
	is_param = OPT_DEFPARAM(name);
	is_flag =  OPT_DEFFLAG(name);
	is_obs = OPT_OBSOLETE(name);
	is_undecl = !DEFINED_OPTION(name);

	/* Warn and pretend the user had not selected the option  */
	if (is_obs) {
		cfgwarn("obsolete option `%s' will be ignored", name);
		return;
	}

	/* Make sure this is not a defined file system. */
	if (is_fs) {
		cfgerror("`%s' is a defined file system", name);
		return;
	}
	/* A defparam must have a value */
	if (is_param && value == NULL) {
		cfgerror("option `%s' must have a value", name);
		return;
	}
	/* A defflag must not have a value */
	if (is_flag && value != NULL) {
		cfgerror("option `%s' must not have a value", name);
		return;
	}

	if (is_undecl && vflag) {
		cfgwarn("undeclared option `%s' added to IDENT", name);
	}

	if (do_option(opttab, &options, &nextopt, name, value, "options",
	    selecttab))
		return;

	/* make lowercase, then add to select table */
	n = strtolower(name);
	(void)ht_insert(selecttab, n, (void *)__UNCONST(n));
	CFGDBG(3, "option selected `%s'", n);
}

void
deloption(const char *name)
{

	CFGDBG(4, "deselecting opt `%s'", name);
	if (undo_option(opttab, &options, &nextopt, name, "options"))
		return;
	if (undo_option(selecttab, NULL, NULL, strtolower(name), "options"))
		return;
}

/*
 * Add a file system option.  This routine simply inserts the name into
 * a list of valid file systems, which is used to validate the root
 * file system type.  The name is then treated like a standard option.
 */
void
addfsoption(const char *name)
{
	const char *n; 

	/* Make sure this is a defined file system. */
	if (!OPT_FSOPT(name)) {
		cfgerror("`%s' is not a defined file system", name);
		return;
	}

	/*
	 * Convert to lower case.  This will be used in the select
	 * table, to verify root file systems.
	 */
	n = strtolower(name);

	if (do_option(fsopttab, &fsoptions, &nextfsopt, name, n, "file-system",
	    selecttab))
		return;

	/* Add to select table. */
	(void)ht_insert(selecttab, n, __UNCONST(n));
	CFGDBG(3, "fs selected `%s'", name);

	/*
	 * Select attribute if one exists.
	 */
	struct attr *a;
	if ((a = ht_lookup(attrtab, n)) != NULL)
		selectattr(a);
}

void
delfsoption(const char *name)
{
	const char *n;

	CFGDBG(4, "deselecting fs `%s'", name);
	n = strtolower(name);
	if (undo_option(fsopttab, &fsoptions, &nextfsopt, name, "file-system"))
		return;
	if (undo_option(selecttab, NULL, NULL, n, "file-system"))
		return;
}

/*
 * Add a "make" option.
 */
void
addmkoption(const char *name, const char *value)
{

	(void)do_option(mkopttab, &mkoptions, &nextmkopt, name, value,
		        "makeoptions", NULL);
}

void
delmkoption(const char *name)
{

	CFGDBG(4, "deselecting mkopt `%s'", name);
	(void)undo_option(mkopttab, &mkoptions, &nextmkopt, name,
	    "makeoptions");
}

/*
 * Add an appending "make" option.
 */
void
appendmkoption(const char *name, const char *value)
{
	struct nvlist *nv;

	nv = newnv(name, value, NULL, 0, NULL);
	*nextappmkopt = nv;
	nextappmkopt = &nv->nv_next;
}

/*
 * Add a conditional appending "make" option.
 */
void
appendcondmkoption(struct condexpr *cond, const char *name, const char *value)
{
	struct nvlist *nv;

	nv = newnv(name, value, cond, 0, NULL);
	*nextcndmkopt = nv;
	nextcndmkopt = &nv->nv_next;
}

/*
 * Copy maxusers to param "MAXUSERS".
 */
void
fixmaxusers(void)
{
	char str[32];

	snprintf(str, sizeof(str), "%d", maxusers);
	addoption(intern("MAXUSERS"), intern(str));
}

/*
 * Copy makeoptions to params with "makeoptions_" prefix.
 */
void
fixmkoption(void)
{
	struct nvlist *nv;
	char buf[100];
	const char *name;

	for (nv = mkoptions; nv != NULL; nv = nv->nv_next) {
		snprintf(buf, sizeof(buf), "makeoptions_%s", nv->nv_name);
		name = intern(buf);
		if (!DEFINED_OPTION(name) || !OPT_DEFPARAM(name))
			continue;
		addoption(name, intern(nv->nv_str));
	}
}

/*
 * Add a name=value pair to an option list.  The value may be NULL.
 */
static int
do_option(struct hashtab *ht, struct nvlist **npp, struct nvlist ***next,
	  const char *name, const char *value, const char *type,
	  struct hashtab *stab)
{
	struct nvlist *nv, *onv;

	/* assume it will work */
	nv = newnv(name, value, NULL, 0, NULL);
	if (ht_insert(ht, name, nv) != 0) {

		/* oops, already got that option - remove it first */
		if ((onv = ht_lookup(ht, name)) == NULL)
			panic("do_option 1");
		if (onv->nv_str != NULL && !OPT_FSOPT(name))
			cfgwarn("already have %s `%s=%s'", type, name,
			    onv->nv_str);
		else
			cfgwarn("already have %s `%s'", type, name);

		if (undo_option(ht, npp, next, name, type))
			panic("do_option 2");
		if (stab != NULL &&
		    undo_option(stab, NULL, NULL, strtolower(name), type))
			panic("do_option 3");

		/* now try adding it again */
		if (ht_insert(ht, name, nv) != 0)
			panic("do_option 4");

		CFGDBG(2, "opt `%s' replaced", name);
	}
	**next = nv;
	*next = &nv->nv_next;

	return (0);
}

/*
 * Remove a name from a hash table,
 * and optionally, a name=value pair from an option list.
 */
static int
undo_option(struct hashtab *ht, struct nvlist **npp,
    struct nvlist ***next, const char *name, const char *type)
{
	struct nvlist *nv;
	
	if (ht_remove(ht, name)) {
		/*
		 * -U command line option removals are always silent
		 */
		if (!handling_cmdlineopts)
			cfgwarn("%s `%s' is not defined", type, name);
		return (1);
	}
	if (npp == NULL) {
		CFGDBG(2, "opt `%s' deselected", name);
		return (0);
	}

	for ( ; *npp != NULL; npp = &(*npp)->nv_next) {
		if ((*npp)->nv_name != name)
			continue;
		if (next != NULL && *next == &(*npp)->nv_next)
			*next = npp;
		nv = (*npp)->nv_next;
		CFGDBG(2, "opt `%s' deselected", (*npp)->nv_name);
		nvfree(*npp);
		*npp = nv;
		return (0);
	}
	panic("%s `%s' is not defined in nvlist", type, name);
	return (1);
}

/*
 * Return true if there is at least one instance of the given unit
 * on the given device attachment (or any units, if unit == WILD).
 */
int
deva_has_instances(struct deva *deva, int unit)
{
	struct devi *i;

	/*
	 * EHAMMERTOOBIG: we shouldn't check i_pseudoroot here.
	 * What we want by this check is them to appear non-present
	 * except for purposes of other devices being able to attach
	 * to them.
	 */
	for (i = deva->d_ihead; i != NULL; i = i->i_asame)
		if (i->i_active == DEVI_ACTIVE && i->i_pseudoroot == 0 &&
		    (unit == WILD || unit == i->i_unit || i->i_unit == STAR))
			return (1);
	return (0);
}

/*
 * Return true if there is at least one instance of the given unit
 * on the given base (or any units, if unit == WILD).
 */
int
devbase_has_instances(struct devbase *dev, int unit)
{
	struct deva *da;

	/*
	 * Pseudo-devices are a little special.  We consider them
	 * to have instances only if they are both:
	 *
	 *	1. Included in this kernel configuration.
	 *
	 *	2. Be declared "defpseudodev".
	 */
	if (dev->d_ispseudo) {
		return ((ht_lookup(devitab, dev->d_name) != NULL)
			&& (dev->d_ispseudo > 1));
	}

	for (da = dev->d_ahead; da != NULL; da = da->d_bsame)
		if (deva_has_instances(da, unit))
			return (1);
	return (0);
}

static int
cfcrosscheck(struct config *cf, const char *what, struct nvlist *nv)
{
	struct devbase *dev;
	struct devi *pd;
	int errs, devunit;

	if (maxpartitions <= 0)
		panic("cfcrosscheck");

	for (errs = 0; nv != NULL; nv = nv->nv_next) {
		if (nv->nv_name == NULL)
			continue;
		dev = ht_lookup(devbasetab, nv->nv_name);
		if (dev == NULL)
			panic("cfcrosscheck(%s)", nv->nv_name);
		if (has_attr(dev->d_attrs, s_ifnet))
			devunit = nv->nv_ifunit;	/* XXX XXX XXX */
		else
			devunit = (int)(minor(nv->nv_num) / maxpartitions);
		if (devbase_has_instances(dev, devunit))
			continue;
		if (devbase_has_instances(dev, STAR) &&
		    devunit >= dev->d_umax)
			continue;
		TAILQ_FOREACH(pd, &allpseudo, i_next) {
			if (pd->i_base == dev && devunit < dev->d_umax &&
			    devunit >= 0)
				goto loop;
		}
		(void)fprintf(stderr,
		    "%s:%d: %s says %s on %s, but there's no %s\n",
		    conffile, cf->cf_lineno,
		    cf->cf_name, what, nv->nv_str, nv->nv_str);
		errs++;
 loop:
		;
	}
	return (errs);
}

/*
 * Cross-check the configuration: make sure that each target device
 * or attribute (`at foo[0*?]') names at least one real device.  Also
 * see that the root and dump devices for all configurations are there.
 */
int
crosscheck(void)
{
	struct config *cf;
	int errs;

	errs = 0;
	if (TAILQ_EMPTY(&allcf)) {
		warnx("%s has no configurations!", conffile);
		errs++;
	}
	TAILQ_FOREACH(cf, &allcf, cf_next) {
		if (cf->cf_root != NULL) {	/* i.e., not root on ? */
			errs += cfcrosscheck(cf, "root", cf->cf_root);
			errs += cfcrosscheck(cf, "dumps", cf->cf_dump);
		}
	}
	return (errs);
}

/*
 * Check to see if there is a *'d unit with a needs-count file.
 */
int
badstar(void)
{
	struct devbase *d;
	struct deva *da;
	struct devi *i;
	int errs, n;

	errs = 0;
	TAILQ_FOREACH(d, &allbases, d_next) {
		for (da = d->d_ahead; da != NULL; da = da->d_bsame)
			for (i = da->d_ihead; i != NULL; i = i->i_asame) {
				if (i->i_unit == STAR)
					goto aybabtu;
			}
		continue;
 aybabtu:
		if (ht_lookup(needcnttab, d->d_name)) {
			warnx("%s's cannot be *'d until its driver is fixed",
			    d->d_name);
			errs++;
			continue;
		}
		for (n = 0; i != NULL; i = i->i_alias)
			if (!i->i_collapsed)
				n++;
		if (n < 1)
			panic("badstar() n<1");
	}
	return (errs);
}

/*
 * Verify/create builddir if necessary, change to it, and verify srcdir.
 * This will be called when we see the first include.
 */
void
setupdirs(void)
{
	struct stat st;

	/* srcdir must be specified if builddir is not specified or if
	 * no configuration filename was specified. */
	if ((builddir || strcmp(defbuilddir, ".") == 0) && !srcdir) {
		cfgerror("source directory must be specified");
		exit(1);
	}

	if (Lflag) {
		if (srcdir == NULL)
			srcdir = "../../..";
		return;
	}

	if (srcdir == NULL)
		srcdir = "../../../..";
	if (builddir == NULL)
		builddir = defbuilddir;

	if (stat(builddir, &st) == -1) {
		if (mkdir(builddir, 0777) == -1)
			errx(EXIT_FAILURE, "cannot create %s", builddir);
	} else if (!S_ISDIR(st.st_mode))
		errx(EXIT_FAILURE, "%s is not a directory", builddir);
	if (chdir(builddir) == -1)
		err(EXIT_FAILURE, "cannot change to %s", builddir);
	if (stat(srcdir, &st) == -1)
		err(EXIT_FAILURE, "cannot stat %s", srcdir);
	if (!S_ISDIR(st.st_mode))
		errx(EXIT_FAILURE, "%s is not a directory", srcdir);
}

/*
 * Write identifier from "ident" directive into file, for
 * newvers.sh to pick it up.
 */
int
mkident(void)
{
	FILE *fp;
	int error = 0;

	(void)unlink("ident");

	if (ident == NULL)
		return (0);
	
	if ((fp = fopen("ident", "w")) == NULL) {
		warn("cannot write ident");
		return (1);
	}
	if (vflag)
		(void)printf("using ident '%s'\n", ident);
	fprintf(fp, "%s\n", ident);
	fflush(fp);
	if (ferror(fp))
		error = 1;
	(void)fclose(fp);

	return error;
}

void
logconfig_start(void)
{
	extern FILE *yyin;
	char line[1024];
	const char *tmpdir;
	struct stat st;
	int fd;

	if (yyin == NULL || fstat(fileno(yyin), &st) == -1)
		return;
	cfgtime = st.st_mtime;

	tmpdir = getenv("TMPDIR");
	if (tmpdir == NULL)
		tmpdir = _PATH_TMP;
	(void)snprintf(line, sizeof(line), "%s/config.tmp.XXXXXX", tmpdir);
	if ((fd = mkstemp(line)) == -1 ||
	    (cfg = fdopen(fd, "r+")) == NULL) {
		if (fd != -1) {
			(void)unlink(line);
			(void)close(fd);
		}
		cfg = NULL;
		return;
	}
	(void)unlink(line);

	(void)fprintf(cfg, "#include <sys/cdefs.h>\n\n");
	(void)fprintf(cfg, "#include \"opt_config.h\"\n");
	(void)fprintf(cfg, "\n");
	(void)fprintf(cfg, "/*\n");
	(void)fprintf(cfg, " * Add either (or both) of\n");
	(void)fprintf(cfg, " *\n");
	(void)fprintf(cfg, " *\toptions %s\n", LOGCONFIG_LARGE);
	(void)fprintf(cfg, " *\toptions %s\n", LOGCONFIG_SMALL);
	(void)fprintf(cfg, " *\n");
	(void)fprintf(cfg,
	    " * to your kernel config file to embed it in the resulting\n");
	(void)fprintf(cfg,
	    " * kernel.  The latter option does not include files that are\n");
	(void)fprintf(cfg,
	    " * included (recursively) by your config file.  The embedded\n");
	(void)fprintf(cfg,
	    " * data be extracted by using the command:\n");
	(void)fprintf(cfg, " *\n");
	(void)fprintf(cfg,
	    " *\tstrings netbsd | sed -n 's/^_CFG_//p' | unvis\n");
	(void)fprintf(cfg, " */\n");
	(void)fprintf(cfg, "\n");
	(void)fprintf(cfg, "#ifdef CONFIG_FILE\n");
	(void)fprintf(cfg, "#if defined(%s) || defined(%s)\n\n",
	    LOGCONFIG_LARGE, LOGCONFIG_SMALL);
	(void)fprintf(cfg, "static const char config[] __used =\n\n");

	(void)fprintf(cfg, "#ifdef %s\n\n", LOGCONFIG_LARGE);
	(void)fprintf(cfg, "\"_CFG_### START CONFIG FILE \\\"%s\\\"\\n\"\n\n",
	    conffile);
	(void)fprintf(cfg, "#endif /* %s */\n\n", LOGCONFIG_LARGE);

	logconfig_include(yyin, NULL);

	(void)fprintf(cfg, "#ifdef %s\n\n", LOGCONFIG_LARGE);
	(void)fprintf(cfg, "\"_CFG_### END CONFIG FILE \\\"%s\\\"\\n\"\n",
	    conffile);

	rewind(yyin);
}

void
logconfig_include(FILE *cf, const char *filename)
{
	char line[1024], in[2048], *out;
	struct stat st;
	int missingeol;

	if (!cfg)
		return;

	missingeol = 0;
	if (fstat(fileno(cf), &st) == -1)
		return;
	if (cfgtime < st.st_mtime)
		cfgtime = st.st_mtime;

	if (filename)
		(void)fprintf(cfg,
		    "\"_CFG_### (included from \\\"%s\\\")\\n\"\n",
		    filename);
	while (fgets(line, sizeof(line), cf) != NULL) {
		missingeol = 1;
		(void)fprintf(cfg, "\"_CFG_");
		if (filename)
			(void)fprintf(cfg, "###> ");
		strvis(in, line, VIS_TAB);
		for (out = in; *out; out++)
			switch (*out) {
			case '\n':
				(void)fprintf(cfg, "\\n\"\n");
				missingeol = 0;
				break;
			case '"': case '\\':
				(void)fputc('\\', cfg);
				/* FALLTHROUGH */
			default:
				(void)fputc(*out, cfg);
				break;
			}
	}
	if (missingeol) {
		(void)fprintf(cfg, "\\n\"\n");
		warnx("%s: newline missing at EOF",
		    filename != NULL ? filename : conffile);
	}
	if (filename)
		(void)fprintf(cfg, "\"_CFG_### (end include \\\"%s\\\")\\n\"\n",
		    filename);

	rewind(cf);
}

void
logconfig_end(void)
{
	char line[1024];
	FILE *fp;
	struct stat st;

	if (!cfg)
		return;

	(void)fprintf(cfg, "#endif /* %s */\n", LOGCONFIG_LARGE);
	(void)fprintf(cfg, ";\n");
	(void)fprintf(cfg, "#endif /* %s || %s */\n",
	    LOGCONFIG_LARGE, LOGCONFIG_SMALL);
	(void)fprintf(cfg, "#endif /* CONFIG_FILE */\n");
	fflush(cfg);
	if (ferror(cfg))
		err(EXIT_FAILURE, "write to temporary file for config.h failed");
	rewind(cfg);

	if (stat("config_file.h", &st) != -1) {
		if (cfgtime < st.st_mtime) {
			fclose(cfg);
			return;
		}
	}

	fp = fopen("config_file.h", "w");
	if (!fp)
		err(EXIT_FAILURE, "cannot open \"config.h\"");

	while (fgets(line, sizeof(line), cfg) != NULL)
		fputs(line, fp);
	fflush(fp);
	if (ferror(fp))
		err(EXIT_FAILURE, "write to \"config.h\" failed");
	fclose(fp);
	fclose(cfg);
}

const char *
strtolower(const char *name)
{
	const char *n;
	char *p, low[500];
	char c;

	for (n = name, p = low; (c = *n) != '\0'; n++)
		*p++ = (char)(isupper((u_char)c) ? tolower((u_char)c) : c);
	*p = '\0';
	return (intern(low));
}

static int
is_elf(const char *file)
{
	int kernel;
	char hdr[4];

	kernel = open(file, O_RDONLY);
	if (kernel == -1)
		err(EXIT_FAILURE, "cannot open %s", file);
	if (read(kernel, hdr, 4) != 4)
		err(EXIT_FAILURE, "Cannot read from %s", file);
	(void)close(kernel);

	return memcmp("\177ELF", hdr, 4) == 0 ? 1 : 0;
}

static int
extract_config(const char *kname, const char *cname, int cfd)
{
	char *ptr;
	void *base;
	int found, kfd;
	struct stat st;
	off_t i;

	found = 0;

	/* mmap(2) binary kernel */
	kfd = open(conffile, O_RDONLY);
	if (kfd == -1)
		err(EXIT_FAILURE, "cannot open %s", kname);
	if (fstat(kfd, &st) == -1)
		err(EXIT_FAILURE, "cannot stat %s", kname);
	base = mmap(0, (size_t)st.st_size, PROT_READ, MAP_FILE | MAP_SHARED,
	    kfd, 0);
	if (base == MAP_FAILED)
		err(EXIT_FAILURE, "cannot mmap %s", kname);
	ptr = base;

	/* Scan mmap(2)'ed region, extracting kernel configuration */
	for (i = 0; i < st.st_size; i++) {
		if ((*ptr == '_') && (st.st_size - i > 5) && memcmp(ptr,
		    "_CFG_", 5) == 0) {
			/* Line found */
			char *oldptr, line[LINE_MAX + 1], uline[LINE_MAX + 1];
			int j;

			found = 1;

			oldptr = (ptr += 5);
			while (*ptr != '\n' && *ptr != '\0')
				ptr++;
			if (ptr - oldptr > LINE_MAX)
				errx(EXIT_FAILURE, "line too long");
			i += ptr - oldptr + 5;
			(void)memcpy(line, oldptr, (size_t)(ptr - oldptr));
			line[ptr - oldptr] = '\0';
			j = strunvis(uline, line);
			if (j == -1)
				errx(EXIT_FAILURE, "unvis: invalid "
				    "encoded sequence");
			uline[j] = '\n';
			if (write(cfd, uline, (size_t)j + 1) == -1)
				err(EXIT_FAILURE, "cannot write to %s", cname);
		} else
			ptr++;
	}

	(void)close(kfd);
	(void)munmap(base, (size_t)st.st_size);
		
	return found;
}

struct dhdi_params {
	struct devbase *d;
	int unit;
	int level;
};

static int
devbase_has_dead_instances(const char *key, void *value, void *aux)
{
	struct devi *i;
	struct dhdi_params *dhdi = aux;

	for (i = value; i != NULL; i = i->i_alias)
		if (i->i_base == dhdi->d &&
		    (dhdi->unit == WILD || dhdi->unit == i->i_unit ||
		     i->i_unit == STAR) &&
		    i->i_level >= dhdi->level)
			return 1;
	return 0;
}

/*
 * This is almost the same as devbase_has_instances, except it
 * may have special considerations regarding ignored instances.
 */

static int
devbase_has_any_instance(struct devbase *dev, int unit, int state, int level)
{
	struct deva *da;
	struct devi *i;

	if (dev->d_ispseudo) {
		if (dev->d_ihead != NULL)
			return 1;
		else if (state != DEVI_IGNORED)
			return 0;
		if ((i = ht_lookup(deaddevitab, dev->d_name)) == NULL)
			return 0;
		return (i->i_level >= level);
	}

	for (da = dev->d_ahead; da != NULL; da = da->d_bsame)
		for (i = da->d_ihead; i != NULL; i = i->i_asame)
			if ((i->i_active == DEVI_ACTIVE ||
			     i->i_active == state) &&
			    (unit == WILD || unit == i->i_unit ||
			     i->i_unit == STAR))
				return 1;

	if (state == DEVI_IGNORED) {
		struct dhdi_params dhdi = { dev, unit, level };
		/* also check dead devices */
		return ht_enumerate(deaddevitab, devbase_has_dead_instances,
		    &dhdi);
	}
		
	return 0;
}

/*
 * check_dead_devi(), used with ht_enumerate, checks if any of the removed
 * device instances would have been a valid instance considering the devbase,
 * the parent device and the interface attribute.
 *
 * In other words, for a non-active device, it checks if children would be
 * actual orphans or the result of a negative statement in the config file.
 */

struct cdd_params {
	struct devbase *d;
	struct attr *at;
	struct devbase *parent;
};

static int
check_dead_devi(const char *key, void *value, void *aux)
{
	struct cdd_params *cdd = aux;
	struct devi *i = value;
	struct pspec *p;

	if (i->i_base != cdd->d)
		return 0;

	for (; i != NULL; i = i->i_alias) {
		p = i->i_pspec;
		if ((p == NULL && cdd->at == NULL) ||
		    (p != NULL && p->p_iattr == cdd->at &&
		     (p->p_atdev == NULL || p->p_atdev == cdd->parent))) {
			if (p != NULL &&
			    !devbase_has_any_instance(cdd->parent, p->p_atunit,
			    DEVI_IGNORED, i->i_level))
				return 0;
			else
				return 1;
		}
	}
	return 0;
}

static void
do_kill_orphans(struct devbase *d, struct attr *at, struct devbase *parent,
    int state)
{
	struct nvlist *nv1;
	struct attrlist *al;
	struct attr *a;
	struct devi *i, *j = NULL;
	struct pspec *p;
	int active = 0;

	/*
	 * A pseudo-device will always attach at root, and if it has an
	 * instance (it cannot have more than one), it is enough to consider
	 * it active, as there is no real attachment.
	 *
	 * A pseudo device can never be marked DEVI_IGNORED.
	 */
	if (d->d_ispseudo) {
		if (d->d_ihead != NULL)
			d->d_ihead->i_active = active = DEVI_ACTIVE;
		else {
			if (ht_lookup(deaddevitab, d->d_name) != NULL)
				active = DEVI_IGNORED;
			else
				return;
		}
	} else {
		int seen = 0;

		for (i = d->d_ihead; i != NULL; i = i->i_bsame) {
			for (j = i; j != NULL; j = j->i_alias) {
				p = j->i_pspec;
				if ((p == NULL && at == NULL) ||
				    (p != NULL && p->p_iattr == at &&
				    (p->p_atdev == NULL ||
				    p->p_atdev == parent))) {
					if (p != NULL &&
					    !devbase_has_any_instance(parent,
					      p->p_atunit, state, j->i_level))
						continue;
					/*
					 * There are Fry-like devices which can
					 * be their own grand-parent (or even
					 * parent, like uhub).  We don't want
					 * to loop, so if we've already reached
					 * an instance for one reason or
					 * another, stop there.
					 */
					if (j->i_active == DEVI_ACTIVE ||
					    j->i_active == state) {
						/*
						 * Device has already been
						 * seen.  However it might
						 * have siblings who still
						 * have to be activated or
						 * orphaned.
						 */
						seen = 1;
						continue;
					}
					j->i_active = active = state;
					if (p != NULL)
						p->p_active = state;
				}
			}
		}
		/*
		 * If we've been there but have made no change, stop.
		 */
		if (seen && !active)
			return;
		if (!active) {
			struct cdd_params cdd = { d, at, parent };
			/* Look for a matching dead devi */
			if (ht_enumerate(deaddevitab, check_dead_devi, &cdd) &&
			    d != parent)
				/*
				 * That device had its instances removed.
				 * Continue the loop marking descendants
				 * with DEVI_IGNORED instead of DEVI_ACTIVE.
				 *
				 * There is one special case for devices that
				 * are their own parent:  if that instance is
				 * removed (e.g., no uhub* at uhub?), we don't
				 * have to continue looping.
				 */
				active = DEVI_IGNORED;
			else
				return;
		}
	}

	for (al = d->d_attrs; al != NULL; al = al->al_next) {
		a = al->al_this;
		for (nv1 = a->a_devs; nv1 != NULL; nv1 = nv1->nv_next)
			do_kill_orphans(nv1->nv_ptr, a, d, active);
	}
}

static int
/*ARGSUSED*/
kill_orphans_cb(const char *key, void *value, void *aux)
{
	do_kill_orphans((struct devbase *)value, NULL, NULL, DEVI_ACTIVE);
	return 0;
}

static void
kill_orphans(void)
{
	ht_enumerate(devroottab, kill_orphans_cb, NULL);
}

static void
add_makeopt(const char *opt)
{
	struct nvlist *p;
	char *buf = estrdup(opt);
	char *eq = strchr(buf, '=');

	if (!eq)
		errx(EXIT_FAILURE, "-D %s is not in var=value format", opt);

	*eq = 0;
	p = newnv(estrdup(buf), estrdup(eq+1), NULL, 0, NULL);
	free(buf);
	p->nv_next = cmdlinedefs;
	cmdlinedefs = p;
}

static void
remove_makeopt(const char *opt)
{
	struct nvlist *p;

	p = newnv(estrdup(opt), NULL, NULL, 0, NULL);
	p->nv_next = cmdlineundefs;
	cmdlineundefs = p;
}

static void
handle_cmdline_makeoptions(void)
{
	struct nvlist *p, *n;

	handling_cmdlineopts = 1;
	for (p = cmdlineundefs; p; p = n) {
		n = p->nv_next;
		delmkoption(intern(p->nv_name));
		free(__UNCONST(p->nv_name));
		nvfree(p);
	}
	for (p = cmdlinedefs; p; p = n) {
		const char *name = intern(p->nv_name);

		n = p->nv_next;
		delmkoption(name);
		addmkoption(name, intern(p->nv_str));
		free(__UNCONST(p->nv_name));
		free(__UNCONST(p->nv_str));

		nvfree(p);
	}
	handling_cmdlineopts = 0;
}

/*	$NetBSD: man.c,v 1.41 2010/07/07 21:24:34 christos Exp $	*/

/*
 * Copyright (c) 1987, 1993, 1994, 1995
 *	The Regents of the University of California.  All rights reserved.
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
 */

#include <sys/cdefs.h>

#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1987, 1993, 1994, 1995\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)man.c	8.17 (Berkeley) 1/31/95";
#else
__RCSID("$NetBSD: man.c,v 1.41 2010/07/07 21:24:34 christos Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/utsname.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <glob.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>
#include <locale.h>

#include "manconf.h"
#include "pathnames.h"

#ifndef MAN_DEBUG
#define MAN_DEBUG 0		/* debug path output */
#endif

/*
 * manstate: structure collecting the current global state so we can 
 * easily identify it and pass it to helper functions in one arg.
 */
struct manstate {
	/* command line flags */
	int all;		/* -a: show all matches rather than first */
	int cat;		/* -c: do not use a pager */
	char *conffile;		/* -C: use alternate config file */
	int how;		/* -h: show SYNOPSIS only */
	char *manpath;		/* -M: alternate MANPATH */
	char *addpath;		/* -m: add these dirs to front of manpath */
	char *pathsearch;	/* -S: path of man must contain this string */
	char *sectionname;	/* -s: limit search to a given man section */
	int where;		/* -w: just show paths of all matching files */

	/* important tags from the config file */
	TAG *defaultpath;	/* _default: default MANPATH */
	TAG *subdirs;		/* _subdir: default subdir search list */
	TAG *suffixlist;	/* _suffix: for files that can be cat()'d */
	TAG *buildlist;		/* _build: for files that must be built */
	
	/* tags for internal use */
	TAG *intmp;		/* _intmp: tmp files we must cleanup */
	TAG *missinglist;	/* _missing: pages we couldn't find */
	TAG *mymanpath;		/* _new_path: final version of MANPATH */
	TAG *section;		/* <sec>: tag for m.sectionname */

	/* other misc stuff */
	const char *pager;	/* pager to use */
	const char *machine;	/* machine */
	const char *machclass;	/* machine class */
	size_t pagerlen;	/* length of the above */
};

/*
 * prototypes
 */
static void	 build_page(char *, char **, struct manstate *);
static void	 cat(char *);
static const char	*check_pager(const char *);
static int	 cleanup(void);
static void	 how(char *);
static void	 jump(char **, char *, char *);
static int	 manual(char *, struct manstate *, glob_t *);
static void	 onsig(int);
static void	 usage(void) __attribute__((__noreturn__));
static void	 addpath(struct manstate *, const char *, size_t, const char *);
static const char *getclass(const char *);

/*
 * main function
 */
int
main(int argc, char **argv)
{
	static struct manstate m = { 0 }; 	/* init to zero */
	int ch, abs_section, found;
	ENTRY *esubd, *epath;
	char *p, **ap, *cmd;
	size_t len;
	glob_t pg;

	setprogname(argv[0]);
	setlocale(LC_ALL, "");
	/*
	 * parse command line...
	 */
	while ((ch = getopt(argc, argv, "-aC:cfhkM:m:P:s:S:w")) != -1)
		switch (ch) {
		case 'a':
			m.all = 1;
			break;
		case 'C':
			m.conffile = optarg;
			break;
		case 'c':
		case '-':	/* XXX: '-' is a deprecated version of '-c' */
			m.cat = 1;
			break;
		case 'h':
			m.how = 1;
			break;
		case 'm':
			m.addpath = optarg;
			break;
		case 'M':
		case 'P':	/* -P for backward compatibility */
			m.manpath = strdup(optarg);
			break;
		/*
		 * The -f and -k options are backward compatible,
		 * undocumented ways of calling whatis(1) and apropos(1).
		 */
		case 'f':
			jump(argv, "-f", "whatis");
			/* NOTREACHED */
		case 'k':
			jump(argv, "-k", "apropos");
			/* NOTREACHED */
		case 's':
			if (m.sectionname != NULL)
				usage();
			m.sectionname = optarg;
			break;
		case 'S':
			m.pathsearch = optarg;
			break;
		case 'w':
			m.all = m.where = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!argc)
		usage();

	/*
	 * read the configuration file and collect any other information
	 * we will need (machine type, pager, section [if specified
	 * without '-s'], and MANPATH through the environment).
	 */
	config(m.conffile);    /* exits on error ... */

	if ((m.machine = getenv("MACHINE")) == NULL) {
		struct utsname utsname;

		if (uname(&utsname) == -1)
			err(EXIT_FAILURE, "uname");
		m.machine = utsname.machine;
	}

	m.machclass = getclass(m.machine);

	if (!m.cat && !m.how && !m.where) {  /* if we need a pager ... */
		if (!isatty(STDOUT_FILENO)) {
			m.cat = 1;
		} else {
			if ((m.pager = getenv("PAGER")) != NULL &&
			    m.pager[0] != '\0')
				m.pager = check_pager(m.pager);
			else
				m.pager = _PATH_PAGER;
			m.pagerlen = strlen(m.pager);
		}
	}

	/* do we need to set m.section to a non-null value? */
	if (m.sectionname) {

		m.section = gettag(m.sectionname, 0); /* -s must be a section */
		if (m.section == NULL)
			errx(EXIT_FAILURE, "unknown section: %s", m.sectionname);

	} else if (argc > 1) {

		m.section = gettag(*argv, 0);  /* might be a section? */
		if (m.section) {
			argv++;
			argc--;
		}

	} 
	
	if (m.manpath == NULL)
		m.manpath = getenv("MANPATH"); /* note: -M overrides getenv */


	/*
	 * get default values from config file, plus create the tags we
	 * use for keeping internal state.  make sure all our mallocs
	 * go through.
	 */
	/* from cfg file */
	m.defaultpath = gettag("_default", 1);
	m.subdirs = gettag("_subdir", 1);
	m.suffixlist = gettag("_suffix", 1);
	m.buildlist = gettag("_build", 1); 
	/* internal use */
	m.mymanpath = gettag("_new_path", 1);
	m.missinglist = gettag("_missing", 1);
	m.intmp = gettag("_intmp", 1);
	if (!m.defaultpath || !m.subdirs || !m.suffixlist || !m.buildlist ||
	    !m.mymanpath || !m.missinglist || !m.intmp)
		errx(EXIT_FAILURE, "malloc failed");

	/*
	 * are we using a section whose elements are all absolute paths?
	 * (we only need to look at the first entry on the section list,
	 * as config() will ensure that any additional entries will match
	 * the first one.)
	 */
	abs_section = (m.section != NULL && 
		!TAILQ_EMPTY(&m.section->entrylist) &&
	    		*(TAILQ_FIRST(&m.section->entrylist)->s) == '/');

	/*
	 * now that we have all the data we need, we must determine the
	 * manpath we are going to use to find the requested entries using
	 * the following steps...
	 *
	 * [1] if the user specified a section and that section's elements
	 *     from the config file are all absolute paths, then we override
	 *     defaultpath and -M/MANPATH with the section's absolute paths.
	 */
	if (abs_section) {
		m.manpath = NULL;	   	/* ignore -M/MANPATH */
		m.defaultpath = m.section;	/* overwrite _default path */
		m.section = NULL;		/* promoted to defaultpath */
	}

	/*
	 * [2] section can now only be non-null if the user asked for
	 *     a section and that section's elements did not have 
         *     absolute paths.  in this case we use the section's
	 *     elements to override _subdir from the config file.
	 *
	 * after this step, we are done processing "m.section"...
	 */
	if (m.section)
		m.subdirs = m.section;

	/*
	 * [3] we need to setup the path we want to use (m.mymanpath).
	 *     if the user gave us a path (m.manpath) use it, otherwise
	 *     go with the default.   in either case we need to append
	 *     the subdir and machine spec to each element of the path.
	 *
	 *     for absolute section paths that come from the config file, 
	 *     we only append the subdir spec if the path ends in 
	 *     a '/' --- elements that do not end in '/' are assumed to 
	 *     not have subdirectories.  this is mainly for backward compat, 
	 *     but it allows non-subdir configs like:
	 *	sect3       /usr/share/man/{old/,}cat3
	 *	doc         /usr/{pkg,share}/doc/{sendmail/op,sendmail/intro}
	 *
	 *     note that we try and be careful to not put double slashes
	 *     in the path (e.g. we want /usr/share/man/man1, not
	 *     /usr/share/man//man1) because "more" will put the filename
	 *     we generate in its prompt and the double slashes look ugly.
	 */
	if (m.manpath) {

		/* note: strtok is going to destroy m.manpath */
		for (p = strtok(m.manpath, ":") ; p ; p = strtok(NULL, ":")) {
			len = strlen(p);
			if (len < 1)
				continue;
			TAILQ_FOREACH(esubd, &m.subdirs->entrylist, q)
				addpath(&m, p, len, esubd->s);
		}

	} else {

		TAILQ_FOREACH(epath, &m.defaultpath->entrylist, q) {
			/* handle trailing "/" magic here ... */
		  	if (abs_section && epath->s[epath->len - 1] != '/') {
				addpath(&m, "", 1, epath->s);
				continue;
			}

			TAILQ_FOREACH(esubd, &m.subdirs->entrylist, q)
				addpath(&m, epath->s, epath->len, esubd->s);
		}

	}

	/*
	 * [4] finally, prepend the "-m" m.addpath to mymanpath if it 
	 *     was specified.   subdirs and machine are always applied to
	 *     m.addpath. 
	 */
	if (m.addpath) {

		/* note: strtok is going to destroy m.addpath */
		for (p = strtok(m.addpath, ":") ; p ; p = strtok(NULL, ":")) {
			len = strlen(p);
			if (len < 1)
				continue;
			TAILQ_FOREACH(esubd, &m.subdirs->entrylist, q)
				addpath(&m, p, len, esubd->s);
		}

	}

	/*
	 * now m.mymanpath is complete!
	 */
#if MAN_DEBUG
	printf("mymanpath:\n");
	TAILQ_FOREACH(epath, &m.mymanpath->entrylist, q) {
		printf("\t%s\n", epath->s);
	}
#endif

	/*
	 * start searching for matching files and format them if necessary.   
	 * setup an interrupt handler so that we can ensure that temporary 
	 * files go away.
	 */
	(void)signal(SIGINT, onsig);
	(void)signal(SIGHUP, onsig);
	(void)signal(SIGPIPE, onsig);

	memset(&pg, 0, sizeof(pg));
	for (found = 0; *argv; ++argv)
		if (manual(*argv, &m, &pg)) {
			found = 1;
		}

	/* if nothing found, we're done. */
	if (!found) {
		(void)cleanup();
		exit(EXIT_FAILURE);
	}

	/*
	 * handle the simple display cases first (m.cat, m.how, m.where)
	 */
	if (m.cat) {
		for (ap = pg.gl_pathv; *ap != NULL; ++ap) {
			if (**ap == '\0')
				continue;
			cat(*ap);
		}
		exit(cleanup());
	}
	if (m.how) {
		for (ap = pg.gl_pathv; *ap != NULL; ++ap) {
			if (**ap == '\0')
				continue;
			how(*ap);
		}
		exit(cleanup());
	}
	if (m.where) {
		for (ap = pg.gl_pathv; *ap != NULL; ++ap) {
			if (**ap == '\0')
				continue;
			(void)printf("%s\n", *ap);
		}
		exit(cleanup());
	}
		
	/*
	 * normal case - we display things in a single command, so
         * build a list of things to display.  first compute total
	 * length of buffer we will need so we can malloc it.
	 */
	for (ap = pg.gl_pathv, len = m.pagerlen + 1; *ap != NULL; ++ap) {
		if (**ap == '\0')
			continue;
		len += strlen(*ap) + 1;
	}
	if ((cmd = malloc(len)) == NULL) {
		warn("malloc");
		(void)cleanup();
		exit(EXIT_FAILURE);
	}

	/* now build the command string... */
	p = cmd;
	len = m.pagerlen;
	memcpy(p, m.pager, len);
	p += len;
	*p++ = ' ';
	for (ap = pg.gl_pathv; *ap != NULL; ++ap) {
		if (**ap == '\0')
			continue;
		len = strlen(*ap);
		memcpy(p, *ap, len);
		p += len;
		*p++ = ' ';
	}
	*--p = '\0';

	/* Use system(3) in case someone's pager is "pager arg1 arg2". */
	(void)system(cmd);

	exit(cleanup());
}

static int
manual_find_buildkeyword(char *escpage, const char *fmt,
	struct manstate *mp, glob_t *pg, size_t cnt)
{
	ENTRY *suffix;
	int found;
	char *p, buf[MAXPATHLEN];

	found = 0;
	/* Try the _build key words next. */
	TAILQ_FOREACH(suffix, &mp->buildlist->entrylist, q) {
		for (p = suffix->s;
		    *p != '\0' && !isspace((unsigned char)*p);
		    ++p)
			continue;
		if (*p == '\0')
			continue;

		*p = '\0';
		(void)snprintf(buf, sizeof(buf), fmt, escpage, suffix->s);
		if (!fnmatch(buf, pg->gl_pathv[cnt], 0)) {
			if (!mp->where)
				build_page(p + 1, &pg->gl_pathv[cnt], mp);
			*p = ' ';
			found = 1;
			break;
		}      
		*p = ' ';
	}

	return found;
}

/*
 * manual --
 *	Search the manuals for the pages.
 */
static int
manual(char *page, struct manstate *mp, glob_t *pg)
{
	ENTRY *suffix, *mdir;
	int anyfound, error, found;
	size_t cnt;
	char *p, buf[MAXPATHLEN], *escpage, *eptr;
	static const char escglob[] = "\\~?*{}[]";

	anyfound = 0;

	/*
	 * Fixup page which may contain glob(3) special characters, e.g.
	 * the famous "No man page for [" FAQ.
	 */
	if ((escpage = malloc((2 * strlen(page)) + 1)) == NULL) {
		warn("malloc");
		(void)cleanup();
		exit(EXIT_FAILURE);
	}

	p = page;
	eptr = escpage;

	while (*p) {
		if (strchr(escglob, *p) != NULL) {
			*eptr++ = '\\';
			*eptr++ = *p++;
		} else
			*eptr++ = *p++;
	}

	*eptr = '\0';

	/*
	 * If 'page' is given with a full or relative path
	 * then interpret it as a file specification.
	 */
	if ((page[0] == '/') || (page[0] == '.')) {
		/* check if file actually exists */
		(void)strlcpy(buf, escpage, sizeof(buf));
		error = glob(buf, GLOB_APPEND | GLOB_BRACE | GLOB_NOSORT, NULL, pg);
		if (error != 0) {
			if (error == GLOB_NOMATCH) {
				goto notfound;
			} else {
				errx(EXIT_FAILURE, "glob failed");
			}
		}

		if (pg->gl_matchc == 0)
			goto notfound;

		/* clip suffix for the suffix check below */
		p = strrchr(escpage, '.');
		if (p && p[0] == '.' && isdigit((unsigned char)p[1]))
			p[0] = '\0';

		found = 0;
		for (cnt = pg->gl_pathc - pg->gl_matchc;
		    cnt < pg->gl_pathc; ++cnt)
		{
			found = manual_find_buildkeyword(escpage, "%s%s",
				mp, pg, cnt);
			if (found) {
				anyfound = 1;
				if (!mp->all) {
					/* Delete any other matches. */
					while (++cnt< pg->gl_pathc)
						pg->gl_pathv[cnt] = "";
					break;
				}
				continue;
			}

			/* It's not a man page, forget about it. */
			pg->gl_pathv[cnt] = "";
		}

  notfound:
		if (!anyfound) {
			if (addentry(mp->missinglist, page, 0) < 0) {
				warn("malloc");
				(void)cleanup();
				exit(EXIT_FAILURE);
			}
		}
		free(escpage);
		return anyfound;
	}

	/* For each man directory in mymanpath ... */
	TAILQ_FOREACH(mdir, &mp->mymanpath->entrylist, q) {

		/* 
		 * use glob(3) to look in the filesystem for matching files.
		 * match any suffix here, as we will check that later.
		 */
		(void)snprintf(buf, sizeof(buf), "%s/%s.*", mdir->s, escpage);
		if ((error = glob(buf,
		    GLOB_APPEND | GLOB_BRACE | GLOB_NOSORT, NULL, pg)) != 0) {
			if (error == GLOB_NOMATCH)
				continue;
			else {
				warn("globbing");
				(void)cleanup();
				exit(EXIT_FAILURE);
			}
		}
		if (pg->gl_matchc == 0)
			continue;

		/*
		 * start going through the matches glob(3) just found and 
		 * use m.pathsearch (if present) to filter out pages we 
		 * don't want.  then verify the suffix is valid, and build
		 * the page if we have a _build suffix.
		 */
		for (cnt = pg->gl_pathc - pg->gl_matchc;
		    cnt < pg->gl_pathc; ++cnt) {

			/* filter on directory path name */
			if (mp->pathsearch) {
				p = strstr(pg->gl_pathv[cnt], mp->pathsearch);
				if (!p || strchr(p, '/') == NULL) {
					pg->gl_pathv[cnt] = ""; /* zap! */
					continue;
				}
			}

			/*
			 * Try the _suffix key words first.
			 *
			 * XXX
			 * Older versions of man.conf didn't have the suffix
			 * key words, it was assumed that everything was a .0.
			 * We just test for .0 first, it's fast and probably
			 * going to hit.
			 */
			(void)snprintf(buf, sizeof(buf), "*/%s.0", escpage);
			if (!fnmatch(buf, pg->gl_pathv[cnt], 0))
				goto next;

			found = 0;
			TAILQ_FOREACH(suffix, &mp->suffixlist->entrylist, q) {
				(void)snprintf(buf,
				     sizeof(buf), "*/%s%s", escpage,
				     suffix->s);
				if (!fnmatch(buf, pg->gl_pathv[cnt], 0)) {
					found = 1;
					break;
				}
			}
			if (found)
				goto next;

			/* Try the _build key words next. */
			found = manual_find_buildkeyword(escpage, "*/%s%s",
				mp, pg, cnt);
			if (found) {
next:				anyfound = 1;
				if (!mp->all) {
					/* Delete any other matches. */
					while (++cnt< pg->gl_pathc)
						pg->gl_pathv[cnt] = "";
					break;
				}
				continue;
			}

			/* It's not a man page, forget about it. */
			pg->gl_pathv[cnt] = "";
		}

		if (anyfound && !mp->all)
			break;
	}

	/* If not found, enter onto the missing list. */
	if (!anyfound) {
		if (addentry(mp->missinglist, page, 0) < 0) {
			warn("malloc");
			(void)cleanup();
			exit(EXIT_FAILURE);
		}
	}

	free(escpage);
	return anyfound;
}

/* 
 * build_page --
 *	Build a man page for display.
 */
static void
build_page(char *fmt, char **pathp, struct manstate *mp)
{
	static int warned;
	int olddir, fd, n, tmpdirlen;
	char *p, *b;
	char buf[MAXPATHLEN], cmd[MAXPATHLEN], tpath[MAXPATHLEN];
	const char *tmpdir;

	/* Let the user know this may take awhile. */
	if (!warned) {
		warned = 1;
		warnx("Formatting manual page...");
	}

       /*
        * Historically man chdir'd to the root of the man tree. 
        * This was used in man pages that contained relative ".so"
        * directives (including other man pages for command aliases etc.)
        * It even went one step farther, by examining the first line
        * of the man page and parsing the .so filename so it would
        * make hard(?) links to the cat'ted man pages for space savings.
        * (We don't do that here, but we could).
        */
 
       /* copy and find the end */
       for (b = buf, p = *pathp; (*b++ = *p++) != '\0';)
               continue;
 
	/* 
	 * skip the last two path components, page name and man[n] ...
	 * (e.g. buf will be "/usr/share/man" and p will be "man1/man.1")
	 * we also save a pointer to our current directory so that we
	 * can fchdir() back to it.  this allows relative MANDIR paths
	 * to work with multiple man pages... e.g. consider:
	 *   	cd /usr/share && man -M ./man cat ls
	 * when no "cat1" subdir files are present.
	 */
	olddir = -1;
	for (--b, --p, n = 2; b != buf; b--, p--)
		if (*b == '/')
			if (--n == 0) {
				*b = '\0';
				olddir = open(".", O_RDONLY);
				(void) chdir(buf);
				p++;
				break;
			}


	/* advance fmt pass the suffix spec to the printf format string */
	for (; *fmt && isspace((unsigned char)*fmt); ++fmt)
		continue;

	/*
	 * Get a temporary file and build a version of the file
	 * to display.  Replace the old file name with the new one.
	 */
	if ((tmpdir = getenv("TMPDIR")) == NULL)
		tmpdir = _PATH_TMP;
	tmpdirlen = strlen(tmpdir);
	(void)snprintf(tpath, sizeof (tpath), "%s%s%s", tmpdir, 
	    (tmpdirlen && tmpdir[tmpdirlen-1] == '/') ? "" : "/", TMPFILE);
	if ((fd = mkstemp(tpath)) == -1) {
		warn("%s", tpath);
		(void)cleanup();
		exit(EXIT_FAILURE);
	}
	(void)snprintf(buf, sizeof(buf), "%s > %s", fmt, tpath);
	(void)snprintf(cmd, sizeof(cmd), buf, p);
	(void)system(cmd);
	(void)close(fd);
	if ((*pathp = strdup(tpath)) == NULL) {
		warn("malloc");
		(void)cleanup();
		exit(EXIT_FAILURE);
	}

	/* Link the built file into the remove-when-done list. */
	if (addentry(mp->intmp, *pathp, 0) < 0) {
		warn("malloc");
		(void)cleanup();
		exit(EXIT_FAILURE);
	}

	/* restore old directory so relative manpaths still work */
	if (olddir != -1) {
		fchdir(olddir);
		close(olddir);
	}
}

/*
 * how --
 *	display how information
 */
static void
how(char *fname)
{
	FILE *fp;

	int lcnt, print;
	char *p, buf[256];

	if (!(fp = fopen(fname, "r"))) {
		warn("%s", fname);
		(void)cleanup();
		exit(EXIT_FAILURE);
	}
#define	S1	"SYNOPSIS"
#define	S2	"S\bSY\bYN\bNO\bOP\bPS\bSI\bIS\bS"
#define	D1	"DESCRIPTION"
#define	D2	"D\bDE\bES\bSC\bCR\bRI\bIP\bPT\bTI\bIO\bON\bN"
	for (lcnt = print = 0; fgets(buf, sizeof(buf), fp);) {
		if (!strncmp(buf, S1, sizeof(S1) - 1) ||
		    !strncmp(buf, S2, sizeof(S2) - 1)) {
			print = 1;
			continue;
		} else if (!strncmp(buf, D1, sizeof(D1) - 1) ||
		    !strncmp(buf, D2, sizeof(D2) - 1)) {
			if (fp)
				(void)fclose(fp);
			return;
		}
		if (!print)
			continue;
		if (*buf == '\n')
			++lcnt;
		else {
			for(; lcnt; --lcnt)
				(void)putchar('\n');
			for (p = buf; isspace((unsigned char)*p); ++p)
				continue;
			(void)fputs(p, stdout);
		}
	}
	(void)fclose(fp);
}

/*
 * cat --
 *	cat out the file
 */
static void
cat(char *fname)
{
	int fd, n;
	char buf[2048];

	if ((fd = open(fname, O_RDONLY, 0)) < 0) {
		warn("%s", fname);
		(void)cleanup();
		exit(EXIT_FAILURE);
	}
	while ((n = read(fd, buf, sizeof(buf))) > 0)
		if (write(STDOUT_FILENO, buf, n) != n) {
			warn("write");
			(void)cleanup();
			exit(EXIT_FAILURE);
		}
	if (n == -1) {
		warn("read");
		(void)cleanup();
		exit(EXIT_FAILURE);
	}
	(void)close(fd);
}

/*
 * check_pager --
 *	check the user supplied page information
 */
static const char *
check_pager(const char *name)
{
	const char *p;

	/*
	 * if the user uses "more", we make it "more -s"; watch out for
	 * PAGER = "mypager /usr/ucb/more"
	 */
	for (p = name; *p && !isspace((unsigned char)*p); ++p)
		continue;
	for (; p > name && *p != '/'; --p);
	if (p != name)
		++p;

	/* make sure it's "more", not "morex" */
	if (!strncmp(p, "more", 4) && (!p[4] || isspace((unsigned char)p[4]))){
		char *newname;
		(void)asprintf(&newname, "%s %s", p, "-s");
		name = newname;
	}

	return name;
}

/*
 * jump --
 *	strip out flag argument and jump
 */
static void
jump(char **argv, char *flag, char *name)
{
	char **arg;

	argv[0] = name;
	for (arg = argv + 1; *arg; ++arg)
		if (!strcmp(*arg, flag))
			break;
	for (; *arg; ++arg)
		arg[0] = arg[1];
	execvp(name, argv);
	err(EXIT_FAILURE, "Cannot execute `%s'", name);
}

/* 
 * onsig --
 *	If signaled, delete the temporary files.
 */
static void
onsig(int signo)
{

	(void)cleanup();

	(void)raise_default_signal(signo);

	/* NOTREACHED */
	exit(EXIT_FAILURE);
}

/*
 * cleanup --
 *	Clean up temporary files, show any error messages.
 */
static int
cleanup(void)
{
	TAG *intmpp, *missp;
	ENTRY *ep;
	int rval;

	rval = EXIT_SUCCESS;
	/* 
	 * note that _missing and _intmp were created by main(), so
	 * gettag() cannot return NULL here.
	 */
	missp = gettag("_missing", 0);	/* missing man pages */
	intmpp = gettag("_intmp", 0);	/* tmp files we need to unlink */

	TAILQ_FOREACH(ep, &missp->entrylist, q) {
		warnx("no entry for %s in the manual.", ep->s);
		rval = EXIT_FAILURE;
	}

	TAILQ_FOREACH(ep, &intmpp->entrylist, q)
		(void)unlink(ep->s);

	return rval;
}

static const char *
getclass(const char *machine)
{
	char buf[BUFSIZ];
	TAG *t;
	snprintf(buf, sizeof(buf), "_%s", machine);
	t = gettag(buf, 0);
	return t != NULL && !TAILQ_EMPTY(&t->entrylist) ?
	    TAILQ_FIRST(&t->entrylist)->s : NULL;
}

static void
addpath(struct manstate *m, const char *dir, size_t len, const char *sub)
{
	char buf[2 * MAXPATHLEN + 1];
	(void)snprintf(buf, sizeof(buf), "%s%s%s{/%s,%s%s%s}",
	     dir, (dir[len - 1] == '/') ? "" : "/", sub, m->machine,
	     m->machclass ? "/" : "", m->machclass ? m->machclass : "",
	     m->machclass ? "," : "");
	if (addentry(m->mymanpath, buf, 0) < 0)
		errx(EXIT_FAILURE, "malloc failed");
}

/*
 * usage --
 *	print usage message and die
 */
static void
usage(void)
{
	(void)fprintf(stderr, "Usage: %s [-acw|-h] [-C cfg] [-M path] "
	    "[-m path] [-S srch] [[-s] sect] name ...\n", getprogname());
	(void)fprintf(stderr, 
	    "Usage: %s -k [-C cfg] [-M path] [-m path] keyword ...\n", 
	    getprogname());
	exit(EXIT_FAILURE);
}

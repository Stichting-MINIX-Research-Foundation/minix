/*	$NetBSD: makewhatis.c,v 1.49 2013/06/24 20:57:47 christos Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matthias Scheler.
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
#if !defined(lint)
__COPYRIGHT("@(#) Copyright (c) 1999\
 The NetBSD Foundation, Inc.  All rights reserved.");
__RCSID("$NetBSD: makewhatis.c,v 1.49 2013/06/24 20:57:47 christos Exp $");
#endif /* not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <glob.h>
#include <locale.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>
#include <util.h>

#include <man/manconf.h>
#include <man/pathnames.h>

#ifndef NROFF
#define NROFF "nroff"
#endif

typedef struct manpagestruct manpage;
struct manpagestruct {
	manpage *mp_left, *mp_right;
	ino_t	 mp_inode;
	size_t	 mp_sdoff;
	size_t	 mp_sdlen;
	char	 mp_name[1];
};

typedef struct whatisstruct whatis;
struct whatisstruct {
	whatis	*wi_left, *wi_right;
	char	*wi_data;
	char	wi_prefix[1];
};

int		main(int, char * const *);
static char	*findwhitespace(char *);
static char	*strmove(char *, char *);
static char	*GetS(gzFile, char *, size_t);
static int	pathnamesection(const char *, const char *);
static int	manpagesection(char *);
static char	*createsectionstring(char *);
static void	addmanpage(manpage **, ino_t, char *, size_t, size_t);
static void	addwhatis(whatis **, char *, char *);
static char	*makesection(int);
static char	*makewhatisline(const char *, const char *, const char *);
static void	catpreprocess(char *);
static char	*parsecatpage(const char *, gzFile *);
static int	manpreprocess(char *);
static char	*nroff(const char *, gzFile *);
static char	*parsemanpage(const char *, gzFile *, int);
static char	*getwhatisdata(char *);
static void	processmanpages(manpage **, whatis **);
static void	dumpwhatis(FILE *, whatis *);
static int	makewhatis(char * const *manpath);

static char * const default_manpath[] = {
#if defined(__minix)
	"/usr/man",
#endif /* defined(__minix) */
	"/usr/share/man",
	NULL
};

static const char	*sectionext = "0123456789ln";
static const char	*whatisdb   = _PATH_WHATIS;
static const char	*whatisdb_new = _PATH_WHATIS ".new";
static int		dowarn      = 0;

#define	ISALPHA(c)	isalpha((unsigned char)(c))
#define	ISDIGIT(c)	isdigit((unsigned char)(c))
#define	ISSPACE(c)	isspace((unsigned char)(c))

int
main(int argc, char *const *argv)
{
	char * const	*manpath;
	int		c, dofork;
	const char	*conffile;
	ENTRY		*ep;
	TAG		*tp;
	int		rv, jobs, status;
	glob_t		pg;
	char		*paths[2], **p, *sl;
	int		retval;

	dofork = 1;
	conffile = NULL;
	jobs = 0;
	retval = EXIT_SUCCESS;

	(void)setlocale(LC_ALL, "");

	while ((c = getopt(argc, argv, "C:fw")) != -1) {
		switch (c) {
		case 'C':
			conffile = optarg;
			break;
		case 'f':
			/* run all processing on foreground */
			dofork = 0;
			break;
		case 'w':
			dowarn++;
			break;
		default:
			fprintf(stderr, "Usage: %s [-fw] [-C file] [manpath ...]\n",
				getprogname());
			exit(EXIT_FAILURE);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc >= 1) {
		manpath = &argv[0];

	    mkwhatis:
		return makewhatis(manpath);
	}

	/*
	 * Try read config file, fallback to default_manpath[]
	 * if man.conf not available.
	 */
	config(conffile);
	if ((tp = gettag("_whatdb", 0)) == NULL) {
		manpath = default_manpath;
		goto mkwhatis;
	}

	/* Build individual databases */
	paths[1] = NULL;
	TAILQ_FOREACH(ep, &tp->entrylist, q) {
		if ((rv = glob(ep->s,
		    GLOB_BRACE | GLOB_NOSORT | GLOB_ERR | GLOB_NOCHECK,
		    NULL, &pg)) != 0)
			err(EXIT_FAILURE, "glob('%s')", ep->s);

		/* We always have something to work with here */
		for (p = pg.gl_pathv; *p; p++) {
			sl = strrchr(*p, '/');
			if (sl == NULL) {
				err(EXIT_FAILURE, "glob: _whatdb entry '%s' "
				    "doesn't contain slash", ep->s);
			}

			/*
			 * Cut the last component of path, leaving just
			 * the directory. We will use the result as root
			 * for manpage search.
			 * glob malloc()s space for the paths, so it's
			 * okay to change it in-place.
			 */
			*sl = '\0';
			paths[0] = *p;

			if (!dofork) {
				/* Do not fork child */
				makewhatis(paths);
				continue;
			}

			switch (fork()) {
			case 0:
				exit(makewhatis(paths));
				break;
			case -1:
				warn("fork");
				makewhatis(paths);
				break;
			default:
				jobs++;
				break;
			}

		}

		globfree(&pg);
	}

	/* Wait for the childern to finish */
	while (jobs > 0) {
		(void)wait(&status);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS)
			retval = EXIT_FAILURE;
		jobs--;
	}

	return retval;
}

static int
makewhatis(char * const * manpath)
{
	FTS	*fts;
	FTSENT	*fe;
	manpage *source;
	whatis	*dest;
	FILE	*out;
	size_t	sdoff, sdlen;
	int	outfd;
	struct stat st_before, st_after;

	if ((fts = fts_open(manpath, FTS_LOGICAL, NULL)) == NULL)
		err(EXIT_FAILURE, "Cannot open `%s'", *manpath);

	source = NULL;
	while ((fe = fts_read(fts)) != NULL) {
		switch (fe->fts_info) {
		case FTS_F:
			if (manpagesection(fe->fts_path) >= 0) {
				/*
				 * Get manpage subdirectory prefix. Most
				 * commonly, this is arch-specific subdirectory.
				 */
				if (fe->fts_level >= 3) {
					int		sl;
					const char	*s, *lsl;

					lsl = NULL;
					s = &fe->fts_path[fe->fts_pathlen - 1];
					for(sl = fe->fts_level - 1; sl > 0;
					    sl--) {
						s--;
						while (s[0] != '/')
							s--;
						if (lsl == NULL)
							lsl = s;
					}

					/*
					 * Include trailing '/', so we get
					 * 'arch/'.
					 */
					sdoff = s + 1 - fe->fts_path;
					sdlen = lsl - s + 1;
				} else {
					sdoff = 0;
					sdlen = 0;
				}

				addmanpage(&source, fe->fts_statp->st_ino,
				    fe->fts_path, sdoff, sdlen);
			}
			/*FALLTHROUGH*/
		case FTS_D:
		case FTS_DC:
		case FTS_DEFAULT:
		case FTS_DP:
		case FTS_SL:
		case FTS_DOT:
		case FTS_W:
		case FTS_NSOK:
		case FTS_INIT:
			break;
		case FTS_SLNONE:
			warnx("Symbolic link with no target: `%s'",
			    fe->fts_path);
			break;
		case FTS_DNR:
			warnx("Unreadable directory: `%s'", fe->fts_path);
			break;
		case FTS_NS:
			errno = fe->fts_errno;
			warn("Cannot stat `%s'", fe->fts_path);
			break;
		case FTS_ERR:
			errno = fe->fts_errno;
			warn("Error reading `%s'", fe->fts_path);
			break;
		default:
			errx(EXIT_FAILURE, "Unknown info %d returned from fts "
			    " for path: `%s'", fe->fts_info, fe->fts_path);
		}
	}

	(void)fts_close(fts);

	dest = NULL;
	processmanpages(&source, &dest);

	if (chdir(manpath[0]) == -1)
		err(EXIT_FAILURE, "Cannot change dir to `%s'", manpath[0]);

	/*
	 * makewhatis runs unattended, so it needs to be able to
	 * recover if the last run crashed out. Therefore, if
	 * whatisdb_new exists and is more than (arbitrarily) sixteen
	 * hours old, nuke it. If it exists but is not so old, refuse
	 * to run until it's cleaned up, in case another makewhatis is
	 * already running. Also, open the output with O_EXCL to make
	 * sure we get our own, in case two copies start exactly at
	 * once. (Unlikely? Maybe, maybe not, if two copies of cron
	 * end up running.)
	 *
	 * Similarly, before renaming the file after we finish writing
	 * to it, make sure it's still the same file we opened. This
	 * can't be completely race-free, but getting caught by it
	 * would require an unexplained sixteen-hour-or-more lag
	 * between the last mtime update when we wrote to it and when
	 * we get to the stat call *and* another makewhatis starting
	 * out to write at exactly the wrong moment. Not impossible,
	 * but not likely enough to worry about.
	 *
	 * This is maybe unnecessarily elaborate, but generating
	 * corrupted output isn't so good either.
	 */

	if (stat(whatisdb_new, &st_before) == 0) {
		if (st_before.st_mtime - time(NULL) > 16*60*60) {
			/* Don't complain if someone else just removed it. */
			if (unlink(whatisdb_new) == -1 && errno != ENOENT) {
				err(EXIT_FAILURE, "Could not remove `%s'",
				    whatisdb_new);
			} else {
				warnx("Removed stale `%s'", whatisdb_new);
			}
		} else {
			errx(EXIT_FAILURE, "The file `%s' already exists "
			    "-- am I already running?", whatisdb_new);
		}
	} else if (errno != ENOENT) {
		/* Something unexpected happened. */
		err(EXIT_FAILURE, "Cannot stat `%s'", whatisdb_new);
	}

	outfd = open(whatisdb_new, O_WRONLY|O_CREAT|O_EXCL,
	    S_IRUSR|S_IRGRP|S_IROTH);
	if (outfd < 0)
		err(EXIT_FAILURE, "Cannot open `%s'", whatisdb_new);

	if (fstat(outfd, &st_before) == -1)
		err(EXIT_FAILURE, "Cannot fstat `%s'", whatisdb_new);

	if ((out = fdopen(outfd, "w")) == NULL)
		err(EXIT_FAILURE, "Cannot fdopen `%s'", whatisdb_new);

	dumpwhatis(out, dest);
	if (fchmod(fileno(out), S_IRUSR|S_IRGRP|S_IROTH) == -1)
		err(EXIT_FAILURE, "Cannot chmod `%s'", whatisdb_new);
	if (fclose(out) != 0)
		err(EXIT_FAILURE, "Cannot close `%s'", whatisdb_new);

	if (stat(whatisdb_new, &st_after) == -1)
		err(EXIT_FAILURE, "Cannot stat `%s' (after writing)",
		    whatisdb_new);

	if (st_before.st_dev != st_after.st_dev ||
	    st_before.st_ino != st_after.st_ino) {
		errx(EXIT_FAILURE, "The file `%s' changed under me; giving up",
		    whatisdb_new);
	}

	if (rename(whatisdb_new, whatisdb) == -1)
		err(EXIT_FAILURE, "Could not rename `%s' to `%s'",
		    whatisdb_new, whatisdb);

	return EXIT_SUCCESS;
}

static char *
findwhitespace(char *str)
{
	while (!ISSPACE(*str))
		if (*str++ == '\0') {
			str = NULL;
			break;
		}

	return str;
}

static char *
strmove(char *dest, char *src)
{
	return memmove(dest, src, strlen(src) + 1);
}

static char *
GetS(gzFile in, char *buffer, size_t length)
{
	char	*ptr;

	if (((ptr = gzgets(in, buffer, (int)length)) != NULL) && (*ptr == '\0'))
		ptr = NULL;

	return ptr;
}

static char *
makesection(int s)
{
	char sectionbuffer[24];
	if (s == -1)
		return NULL;
	(void)snprintf(sectionbuffer, sizeof(sectionbuffer),
		" (%c) - ", sectionext[s]);
	return estrdup(sectionbuffer);
}

static int
pathnamesection(const char *pat, const char *name)
{
	char *ptr, *ext;
	size_t len = strlen(pat);


	while ((ptr = strstr(name, pat)) != NULL) {
		if ((ext = strchr(sectionext, ptr[len])) != NULL) {
			return ext - sectionext;
		}
		name = ptr + 1;
	}
	return -1;
}


static int
manpagesection(char *name)
{
	char	*ptr;

	if ((ptr = strrchr(name, '/')) != NULL)
		ptr++;
	else
		ptr = name;

	while ((ptr = strchr(ptr, '.')) != NULL) {
		int section;

		ptr++;
		section = 0;
		while (sectionext[section] != '\0')
			if (sectionext[section] == *ptr)
				return section;
			else
				section++;
	}
	return -1;
}

static char *
createsectionstring(char *section_id)
{
	char *section;

	if (asprintf(&section, " (%s) - ", section_id) < 0)
		err(EXIT_FAILURE, "malloc failed");
	return section;
}

static void
addmanpage(manpage **tree, ino_t inode, char *name, size_t sdoff, size_t sdlen)
{
	manpage *mp;

	while ((mp = *tree) != NULL) {
		if (mp->mp_inode == inode)
			return;
		tree = inode < mp->mp_inode ? &mp->mp_left : &mp->mp_right;
	}

	mp = emalloc(sizeof(manpage) + strlen(name));
	mp->mp_left = NULL;
	mp->mp_right = NULL;
	mp->mp_inode = inode;
	mp->mp_sdoff = sdoff;
	mp->mp_sdlen = sdlen;
	(void)strcpy(mp->mp_name, name);
	*tree = mp;
}

static void
addwhatis(whatis **tree, char *data, char *prefix)
{
	whatis *wi;
	int result;

	while (ISSPACE(*data))
		data++;

	if (*data == '/') {
		char *ptr;

		ptr = ++data;
		while ((*ptr != '\0') && !ISSPACE(*ptr))
			if (*ptr++ == '/')
				data = ptr;
	}

	while ((wi = *tree) != NULL) {
		result = strcmp(data, wi->wi_data);
		if (result == 0) result = strcmp(prefix, wi->wi_prefix);
		if (result == 0) return;
		tree = result < 0 ? &wi->wi_left : &wi->wi_right;
	}

	wi = emalloc(sizeof(whatis) + strlen(prefix));

	wi->wi_left = NULL;
	wi->wi_right = NULL;
	wi->wi_data = data;
	if (prefix[0] != '\0')
		(void) strcpy(wi->wi_prefix, prefix);
	else
		wi->wi_prefix[0] = '\0';
	*tree = wi;
}

static void
catpreprocess(char *from)
{
	char	*to;

	to = from;
	while (ISSPACE(*from)) from++;

	while (*from != '\0')
		if (ISSPACE(*from)) {
			while (ISSPACE(*++from));
			if (*from != '\0')
				*to++ = ' ';
		}
		else if (*(from + 1) == '\b')
			from += 2;
		else
			*to++ = *from++;

	*to = '\0';
}

static char *
makewhatisline(const char *file, const char *line, const char *section)
{
	static const char *del[] = {
		" - ",
		" -- ",
		"- ",
		" -",
		NULL
	};
	size_t i, pos;
	size_t llen, slen, dlen;
	char *result, *ptr;

	ptr = NULL;
	if (section == NULL) {
		if (dowarn)
			warnx("%s: No section provided for `%s'", file, line);
		return estrdup(line);
	}

	for (i = 0; del[i]; i++)
		if ((ptr = strstr(line, del[i])) != NULL)
			break;

	if (del[i] == NULL) {
		if (dowarn)
			warnx("%s: Bad format line `%s'", file, line);
		return estrdup(line);
	}

	slen = strlen(section);
	llen = strlen(line);
	dlen = strlen(del[i]);

	result = emalloc(llen - dlen + slen + 1);
	pos = ptr - line;

	(void)memcpy(result, line, pos);
	(void)memcpy(&result[pos], section, slen);
	(void)strcpy(&result[pos + slen], &line[pos + dlen]);
	return result;
}

static char *
parsecatpage(const char *name, gzFile *in)
{
	char	 buffer[8192];
	char	*section, *ptr, *last;
	size_t	 size;

	do {
		if (GetS(in, buffer, sizeof(buffer)) == NULL)
			return NULL;
	}
	while (buffer[0] == '\n');

	section = NULL;
	if ((ptr = strchr(buffer, '(')) != NULL) {
		if ((last = strchr(ptr + 1, ')')) !=NULL) {
			size_t	length;

			length = last - ptr + 1;
			section = emalloc(length + 5);
			*section = ' ';
			(void) memcpy(section + 1, ptr, length);
			(void) strcpy(section + 1 + length, " - ");
		}
	}

	for (;;) {
		if (GetS(in, buffer, sizeof(buffer)) == NULL) {
			free(section);
			return NULL;
		}
		catpreprocess(buffer);
		if (strncmp(buffer, "NAME", 4) == 0)
			break;
	}
	if (section == NULL)
		section = makesection(pathnamesection("/cat", name));

	ptr = last = buffer;
	size = sizeof(buffer) - 1;
	while ((size > 0) && (GetS(in, ptr, size) != NULL)) {
		int	 length;

		catpreprocess(ptr);

		length = strlen(ptr);
		if (length == 0) {
			*last = '\0';

			ptr = makewhatisline(name, buffer, section);
			free(section);
			return ptr;
		}
		if ((length > 1) && (ptr[length - 1] == '-') &&
		    ISALPHA(ptr[length - 2]))
			last = &ptr[--length];
		else {
			last = &ptr[length++];
			*last = ' ';
		}

		ptr += length;
		size -= length;
	}

	free(section);

	return NULL;
}

static int
manpreprocess(char *line)
{
	char	*from, *to;

	to = from = line;
	while (ISSPACE(*from))
		from++;
	if (strncmp(from, ".\\\"", 3) == 0)
		return 1;

	while (*from != '\0')
		if (ISSPACE(*from)) {
			while (ISSPACE(*++from));
			if ((*from != '\0') && (*from != ','))
				*to++ = ' ';
		} else if (*from == '\\') {
			switch (*++from) {
			case '\0':
			case '-':
				break;
			case 'f':
			case 's':
				from++;
				if ((*from=='+') || (*from=='-'))
					from++;
				while (ISDIGIT(*from))
					from++;
				break;
			default:
				from++;
			}
		} else {
			if (*from == '"')
				from++;
			else
				*to++ = *from++;
		}

	*to = '\0';

	if (strncasecmp(line, ".Xr", 3) == 0) {
		char	*sect;

		from = line + 3;
		if (ISSPACE(*from))
			from++;

		if ((sect = findwhitespace(from)) != NULL) {
			size_t	length;
			char	*trail;

			*sect++ = '\0';
			if ((trail = findwhitespace(sect)) != NULL)
				*trail++ = '\0';
			length = strlen(from);
			(void) memmove(line, from, length);
			line[length++] = '(';
			to = &line[length];
			length = strlen(sect);
			(void) memmove(to, sect, length);
			if (trail == NULL) {
				(void) strcpy(&to[length], ")");
			} else {
				to += length;
				*to++ = ')';
				length = strlen(trail);
				(void) memmove(to, trail, length + 1);
			}
		}
	}

	return 0;
}

static char *
nroff(const char *inname, gzFile *in)
{
	char tempname[MAXPATHLEN], buffer[65536], *data;
	int tempfd, bytes, pipefd[2], status;
	static int devnull = -1;
	pid_t child;

	if (gzrewind(in) < 0)
		err(EXIT_FAILURE, "Cannot rewind pipe");

	if ((devnull < 0) &&
	    ((devnull = open(_PATH_DEVNULL, O_WRONLY, 0)) < 0))
		err(EXIT_FAILURE, "Cannot open `/dev/null'");

	(void)strlcpy(tempname, _PATH_TMP "makewhatis.XXXXXX",
	    sizeof(tempname));
	if ((tempfd = mkstemp(tempname)) == -1)
		err(EXIT_FAILURE, "Cannot create temp file");

	while ((bytes = gzread(in, buffer, sizeof(buffer))) > 0)
		if (write(tempfd, buffer, (size_t)bytes) != bytes) {
			bytes = -1;
			break;
		}

	if (bytes < 0) {
		(void)close(tempfd);
		(void)unlink(tempname);
		err(EXIT_FAILURE, "Read from pipe failed");
	}
	if (lseek(tempfd, (off_t)0, SEEK_SET) == (off_t)-1) {
		(void)close(tempfd);
		(void)unlink(tempname);
		err(EXIT_FAILURE, "Cannot rewind temp file");
	}
	if (pipe(pipefd) == -1) {
		(void)close(tempfd);
		(void)unlink(tempname);
		err(EXIT_FAILURE, "Cannot create pipe");
	}

	switch (child = vfork()) {
	case -1:
		(void)close(pipefd[1]);
		(void)close(pipefd[0]);
		(void)close(tempfd);
		(void)unlink(tempname);
		err(EXIT_FAILURE, "Fork failed");
		/* NOTREACHED */
	case 0:
		(void)close(pipefd[0]);
		if (tempfd != STDIN_FILENO) {
			(void)dup2(tempfd, STDIN_FILENO);
			(void)close(tempfd);
		}
		if (pipefd[1] != STDOUT_FILENO) {
			(void)dup2(pipefd[1], STDOUT_FILENO);
			(void)close(pipefd[1]);
		}
		if (devnull != STDERR_FILENO) {
			(void)dup2(devnull, STDERR_FILENO);
			(void)close(devnull);
		}
		(void)execlp(NROFF, NROFF, "-S", "-man", NULL);
		_exit(EXIT_FAILURE);
		/*NOTREACHED*/
	default:
		(void)close(pipefd[1]);
		(void)close(tempfd);
		break;
	}

	if ((in = gzdopen(pipefd[0], "r")) == NULL) {
		if (errno == 0)
			errno = ENOMEM;
		(void)close(pipefd[0]);
		(void)kill(child, SIGTERM);
		while (waitpid(child, NULL, 0) != child);
		(void)unlink(tempname);
		err(EXIT_FAILURE, "Cannot read from pipe");
	}

	data = parsecatpage(inname, in);
	while (gzread(in, buffer, sizeof(buffer)) > 0);
	(void)gzclose(in);

	while (waitpid(child, &status, 0) != child);
	if ((data != NULL) &&
	    !(WIFEXITED(status) && (WEXITSTATUS(status) == 0))) {
		free(data);
		errx(EXIT_FAILURE, NROFF " on `%s' exited with %d status",
		    inname, WEXITSTATUS(status));
	}

	(void)unlink(tempname);
	return data;
}

static char *
parsemanpage(const char *name, gzFile *in, int defaultsection)
{
	char	*section, buffer[8192], *ptr;
	static const char POD[] = ".\\\" Automatically generated by Pod";
	static const char IX[] = ".IX TITLE";

	section = NULL;
	do {
		if (GetS(in, buffer, sizeof(buffer) - 1) == NULL) {
			free(section);
			return NULL;
		}

		/*
		 * Skip over lines in man pages that have been generated
		 * by Pod, until we find the TITLE.
		 */
		if (strncasecmp(buffer, POD, sizeof(POD) - 1) == 0) {
			do {
				if (GetS(in, buffer, sizeof(buffer) - 1)
				    == NULL) {
					free(section);
					return NULL;
				}
			} while (strncasecmp(buffer, IX, sizeof(IX) - 1) != 0);
		} 

		if (manpreprocess(buffer))
			continue;
		if (strncasecmp(buffer, ".Dt", 3) == 0) {
			char	*end;

			ptr = &buffer[3];
			if (ISSPACE(*ptr))
				ptr++;
			if ((ptr = findwhitespace(ptr)) == NULL)
				continue;

			if ((end = findwhitespace(++ptr)) != NULL)
				*end = '\0';

			free(section);
			section = createsectionstring(ptr);
		}
		else if (strncasecmp(buffer, ".TH", 3) == 0) {
			ptr = &buffer[3];
			while (ISSPACE(*ptr))
				ptr++;
			if ((ptr = findwhitespace(ptr)) != NULL) {
				char *next;

				while (ISSPACE(*ptr))
					ptr++;
				if ((next = findwhitespace(ptr)) != NULL)
					*next = '\0';
				free(section);
				section = createsectionstring(ptr);
			}
		}
		else if (strncasecmp(buffer, ".Ds", 3) == 0) {
			free(section);
			return NULL;
		}
	} while (strncasecmp(buffer, ".Sh NAME", 8) != 0);

	do {
		if (GetS(in, buffer, sizeof(buffer) - 1) == NULL) {
			free(section);
			return NULL;
		}
	} while (manpreprocess(buffer));

	if (strncasecmp(buffer, ".Nm", 3) == 0) {
		size_t	length, offset;

		ptr = &buffer[3];
		while (ISSPACE(*ptr))
			ptr++;

		length = strlen(ptr);
		if ((length > 1) && (ptr[length - 1] == ',') &&
		    ISSPACE(ptr[length - 2])) {
			ptr[--length] = '\0';
			ptr[length - 1] = ',';
		}
		(void) memmove(buffer, ptr, length + 1);

		offset = length + 3;
		ptr = &buffer[offset];
		for (;;) {
			size_t	 more;

			if ((sizeof(buffer) == offset) ||
			    (GetS(in, ptr, sizeof(buffer) - offset)
			       == NULL)) {
				free(section);
				return NULL;
			}
			if (manpreprocess(ptr))
				continue;

			if (strncasecmp(ptr, ".Nm", 3) != 0) break;

			ptr += 3;
			if (ISSPACE(*ptr))
				ptr++;

			buffer[length++] = ' ';
			more = strlen(ptr);
			if ((more > 1) && (ptr[more - 1] == ',') &&
			    ISSPACE(ptr[more - 2])) {
				ptr[--more] = '\0';
				ptr[more - 1] = ',';
			}

			(void) memmove(&buffer[length], ptr, more + 1);
			length += more;
			offset = length + 3;

			ptr = &buffer[offset];
		}

		if (strncasecmp(ptr, ".Nd", 3) == 0) {
			(void) strlcpy(&buffer[length], " -",
			    sizeof(buffer) - length);

			while (strncasecmp(ptr, ".Sh", 3) != 0) {
				int	 more;

				if (*ptr == '.') {
					char	*space;

					if (strncasecmp(ptr, ".Nd", 3) != 0 ||
					    strchr(ptr, '[') != NULL) {
						free(section);
						return NULL;
					}
					space = findwhitespace(ptr);
					if (space == NULL) {
						ptr = "";
					} else {
						space++;
						(void) strmove(ptr, space);
					}
				}

				if (*ptr != '\0') {
					buffer[offset - 1] = ' ';
					more = strlen(ptr) + 1;
					offset += more;
				}
				ptr = &buffer[offset];
				if ((sizeof(buffer) == offset) ||
				    (GetS(in, ptr, sizeof(buffer) - offset)
					== NULL)) {
					free(section);
					return NULL;
				}
				if (manpreprocess(ptr))
					*ptr = '\0';
			}
		}
	}
	else {
		int	 offset;

		if (*buffer == '.') {
			char	*space;

			if ((space = findwhitespace(&buffer[1])) == NULL) {
				free(section);
				return NULL;
			}
			space++;
			(void) strmove(buffer, space);
		}

		offset = strlen(buffer) + 1;
		for (;;) {
			int	 more;

			ptr = &buffer[offset];
			if ((sizeof(buffer) == offset) ||
			    (GetS(in, ptr, sizeof(buffer) - offset)
				== NULL)) {
				free(section);
				return NULL;
			}
			if (manpreprocess(ptr) || (*ptr == '\0'))
				continue;

			if ((strncasecmp(ptr, ".Sh", 3) == 0) ||
			    (strncasecmp(ptr, ".Ss", 3) == 0))
				break;

			if (*ptr == '.') {
				char	*space;

				if ((space = findwhitespace(ptr)) == NULL) {
					continue;
				}

				space++;
				(void) memmove(ptr, space, strlen(space) + 1);
			}

			buffer[offset - 1] = ' ';
			more = strlen(ptr);
			if ((more > 1) && (ptr[more - 1] == ',') &&
			    ISSPACE(ptr[more - 2])) {
				ptr[more - 1] = '\0';
				ptr[more - 2] = ',';
			}
			else more++;
			offset += more;
		}
	}

	if (section == NULL)
		section = makesection(defaultsection);

	ptr = makewhatisline(name, buffer, section);
	free(section);
	return ptr;
}

static char *
getwhatisdata(char *name)
{
	gzFile	*in;
	char	*data;
	int	 section;

	if ((in = gzopen(name, "r")) == NULL) {
		if (errno == 0)
			errno = ENOMEM;
		err(EXIT_FAILURE, "Cannot open `%s'", name);
		/* NOTREACHED */
	}

	section = manpagesection(name);
	if (section == 0) {
		data = parsecatpage(name, in);
	} else {
		data = parsemanpage(name, in, section);
		if (data == NULL)
			data = nroff(name, in);
	}

	(void) gzclose(in);
	return data;
}

static void
processmanpages(manpage **source, whatis **dest)
{
	manpage *mp;
	char sd[128];

	mp = *source;
	*source = NULL;

	while (mp != NULL) {
		manpage *obsolete;
		char *data;

		if (mp->mp_left != NULL)
			processmanpages(&mp->mp_left, dest);

		if ((data = getwhatisdata(mp->mp_name)) != NULL) {
			/* Pass eventual directory prefix to addwhatis() */
			if (mp->mp_sdlen > 0 && mp->mp_sdlen < sizeof(sd)-1)
				strlcpy(sd, &mp->mp_name[mp->mp_sdoff],
					mp->mp_sdlen);
			else
				sd[0] = '\0';

			addwhatis(dest, data, sd);
		}

		obsolete = mp;
		mp = mp->mp_right;
		free(obsolete);
	}
}

static void
dumpwhatis(FILE *out, whatis *tree)
{
	while (tree != NULL) {
		if (tree->wi_left)
			dumpwhatis(out, tree->wi_left);

		if ((tree->wi_data[0] && fputs(tree->wi_prefix, out) == EOF) ||
		    (fputs(tree->wi_data, out) == EOF) ||
		    (fputc('\n', out) == EOF))
			err(EXIT_FAILURE, "Write failed");

		tree = tree->wi_right;
	}
}

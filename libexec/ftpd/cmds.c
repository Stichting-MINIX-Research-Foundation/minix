/*	$NetBSD: cmds.c,v 1.31 2012/06/19 06:06:34 dholland Exp $	*/

/*
 * Copyright (c) 1999-2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
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

/*
 * Copyright (c) 1985, 1988, 1990, 1992, 1993, 1994
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

/*
 * Copyright (C) 1997 and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
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
__RCSID("$NetBSD: cmds.c,v 1.31 2012/06/19 06:06:34 dholland Exp $");
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <arpa/ftp.h>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tzfile.h>
#include <unistd.h>
#include <ctype.h>

#ifdef KERBEROS5
#include <krb5/krb5.h>
#endif

#include "extern.h"

typedef enum {
	FE_MLSD		= 1<<0,		/* if op is MLSD (MLST otherwise ) */
	FE_ISCURDIR	= 1<<1,		/* if name is the current directory */
} factflag_t;

typedef struct {
	const char	*path;		/* full pathname */
	const char	*display;	/* name to display */
	struct stat	*stat;		/* stat of path */
	struct stat	*pdirstat;	/* stat of path's parent dir */
	factflag_t	 flags;		/* flags */
} factelem;

static void	ack(const char *);
static void	base64_encode(const char *, size_t, char *, int);
static void	fact_type(const char *, FILE *, factelem *);
static void	fact_size(const char *, FILE *, factelem *);
static void	fact_modify(const char *, FILE *, factelem *);
static void	fact_perm(const char *, FILE *, factelem *);
static void	fact_unique(const char *, FILE *, factelem *);
static int	matchgroup(gid_t);
static void	mlsname(FILE *, factelem *);
static void	replydirname(const char *, const char *);

struct ftpfact {
	const char	 *name;		/* name of fact */
	int		  enabled;	/* if fact is enabled */
	void		(*display)(const char *, FILE *, factelem *);
					/* function to display fact */
};

struct ftpfact facttab[] = {
	{ "Type",	1, fact_type },
#define	FACT_TYPE 0
	{ "Size",	1, fact_size },
	{ "Modify",	1, fact_modify },
	{ "Perm",	1, fact_perm },
	{ "Unique",	1, fact_unique },
	/* "Create" */
	/* "Lang" */
	/* "Media-Type" */
	/* "CharSet" */
};

#define FACTTABSIZE	(sizeof(facttab) / sizeof(struct ftpfact))

static char cached_path[MAXPATHLEN + 1] = "/";
static void discover_path(char *, const char *);

void
cwd(const char *path)
{

	if (chdir(path) < 0)
		perror_reply(550, path);
	else {
		show_chdir_messages(250);
		ack("CWD");
		if (getcwd(cached_path, MAXPATHLEN) == NULL) {
			discover_path(cached_path, path);
		}
	}
}

void
delete(const char *name)
{
	char *p = NULL;

	if (remove(name) < 0) {
		p = strerror(errno);
		perror_reply(550, name);
	} else
		ack("DELE");
	logxfer("delete", -1, name, NULL, NULL, p);
}

void
feat(void)
{
	size_t i;

	reply(-211, "Features supported");
	cprintf(stdout, " MDTM\r\n");
	cprintf(stdout, " MLST ");
	for (i = 0; i < FACTTABSIZE; i++)
		cprintf(stdout, "%s%s;", facttab[i].name,
		    facttab[i].enabled ? "*" : "");
	cprintf(stdout, "\r\n");
	cprintf(stdout, " REST STREAM\r\n");
	cprintf(stdout, " SIZE\r\n");
	cprintf(stdout, " TVFS\r\n");
	reply(211,  "End");
}

void
makedir(const char *name)
{
	char *p = NULL;

	if (mkdir(name, 0777) < 0) {
		p = strerror(errno);
		perror_reply(550, name);
	} else
		replydirname(name, "directory created.");
	logxfer("mkdir", -1, name, NULL, NULL, p);
}

void
mlsd(const char *path)
{
	struct dirent	*dp;
	struct stat	 sb, pdirstat;
	factelem f;
	FILE	*dout;
	DIR	*dirp;
	char	name[MAXPATHLEN];
	int	hastypefact;

	hastypefact = facttab[FACT_TYPE].enabled;
	if (path == NULL)
		path = ".";
	if (stat(path, &pdirstat) == -1) {
 mlsdperror:
		perror_reply(550, path);
		return;
	}
	if (! S_ISDIR(pdirstat.st_mode)) {
		errno = ENOTDIR;
		perror_reply(501, path);
		return;
	}
	if ((dirp = opendir(path)) == NULL)
		goto mlsdperror;

	dout = dataconn("MLSD", (off_t)-1, "w");
	if (dout == NULL)
		return;

	memset(&f, 0, sizeof(f));
	f.stat = &sb;
	f.flags |= FE_MLSD;
	while ((dp = readdir(dirp)) != NULL) {
		snprintf(name, sizeof(name), "%s/%s", path, dp->d_name);
		if (ISDOTDIR(dp->d_name)) {	/* special case curdir: */
			if (! hastypefact)
				continue;
			f.pdirstat = NULL;	/*   require stat of parent */
			f.display = path;	/*   set name to real name */
			f.flags |= FE_ISCURDIR; /*   flag name is curdir */
		} else {
			if (ISDOTDOTDIR(dp->d_name)) {
				if (! hastypefact)
					continue;
				f.pdirstat = NULL;
			} else
				f.pdirstat = &pdirstat;	/* cache parent stat */
			f.display = dp->d_name;
			f.flags &= ~FE_ISCURDIR;
		}
		if (stat(name, &sb) == -1)
			continue;
		f.path = name;
		mlsname(dout, &f);
	}
	(void)closedir(dirp);

	if (ferror(dout) != 0)
		perror_reply(550, "Data connection");
	else
		reply(226, "MLSD complete.");
	closedataconn(dout);
	total_xfers_out++;
	total_xfers++;
}

void
mlst(const char *path)
{
	struct stat sb;
	factelem f;

	if (path == NULL)
		path = ".";
	if (stat(path, &sb) == -1) {
		perror_reply(550, path);
		return;
	}
	reply(-250, "MLST %s", path);
	memset(&f, 0, sizeof(f));
	f.path = path;
	f.display = path;
	f.stat = &sb;
	f.pdirstat = NULL;
	CPUTC(' ', stdout);
	mlsname(stdout, &f);
	reply(250, "End");
}


void
opts(const char *command)
{
	struct tab *c;
	char *ep;

	if ((ep = strchr(command, ' ')) != NULL)
		*ep++ = '\0';
	c = lookup(cmdtab, command);
	if (c == NULL) {
		reply(502, "Unknown command '%s'.", command);
		return;
	}
	if (! CMD_IMPLEMENTED(c)) {
		reply(502, "%s command not implemented.", c->name);
		return;
	}
	if (! CMD_HAS_OPTIONS(c)) {
		reply(501, "%s command does not support persistent options.",
		    c->name);
		return;
	}

			/* special case: MLST */
	if (strcasecmp(command, "MLST") == 0) {
		int	 enabled[FACTTABSIZE];
		size_t	 i, onedone;
		size_t	 len;
		char	*p;

		for (i = 0; i < sizeof(enabled) / sizeof(int); i++)
			enabled[i] = 0;
		if (ep == NULL || *ep == '\0')
			goto displaymlstopts;

				/* don't like spaces, and need trailing ; */
		len = strlen(ep);
		if (strchr(ep, ' ') != NULL || ep[len - 1] != ';') {
 badmlstopt:
			reply(501, "Invalid MLST options");
			return;
		}
		ep[len - 1] = '\0';
		while ((p = strsep(&ep, ";")) != NULL) {
			if (*p == '\0')
				goto badmlstopt;
			for (i = 0; i < FACTTABSIZE; i++)
				if (strcasecmp(p, facttab[i].name) == 0) {
					enabled[i] = 1;
					break;
				}
		}

 displaymlstopts:
		for (i = 0; i < FACTTABSIZE; i++)
			facttab[i].enabled = enabled[i];
		cprintf(stdout, "200 MLST OPTS");
		for (i = onedone = 0; i < FACTTABSIZE; i++) {
			if (facttab[i].enabled) {
				cprintf(stdout, "%s%s;", onedone ? "" : " ",
				    facttab[i].name);
				onedone++;
			}
		}
		cprintf(stdout, "\r\n");
		fflush(stdout);
		return;
	}

			/* default cases */
	if (ep != NULL && *ep != '\0')
		REASSIGN(c->options, ftpd_strdup(ep));
	if (c->options != NULL)
		reply(200, "Options for %s are '%s'.", c->name,
		    c->options);
	else
		reply(200, "No options defined for %s.", c->name);
}

void
pwd(void)
{
	char path[MAXPATHLEN];

	if (getcwd(path, sizeof(path) - 1) == NULL) {
		if (chdir(cached_path) < 0) {
			reply(550, "Can't get the current directory: %s.",
			    strerror(errno));
			return;
		}
		(void)strlcpy(path, cached_path, MAXPATHLEN);
	}
	replydirname(path, "is the current directory.");
}

void
removedir(const char *name)
{
	char *p = NULL;

	if (rmdir(name) < 0) {
		p = strerror(errno);
		perror_reply(550, name);
	} else
		ack("RMD");
	logxfer("rmdir", -1, name, NULL, NULL, p);
}

char *
renamefrom(const char *name)
{
	struct stat st;

	if (stat(name, &st) < 0) {
		perror_reply(550, name);
		return (NULL);
	}
	reply(350, "File exists, ready for destination name");
	return (ftpd_strdup(name));
}

void
renamecmd(const char *from, const char *to)
{
	char *p = NULL;

	if (rename(from, to) < 0) {
		p = strerror(errno);
		perror_reply(550, "rename");
	} else
		ack("RNTO");
	logxfer("rename", -1, from, to, NULL, p);
}

void
sizecmd(const char *filename)
{
	switch (type) {
	case TYPE_L:
	case TYPE_I:
	    {
		struct stat stbuf;
		if (stat(filename, &stbuf) < 0 || !S_ISREG(stbuf.st_mode))
			reply(550, "%s: not a plain file.", filename);
		else
			reply(213, ULLF, (ULLT)stbuf.st_size);
		break;
	    }
	case TYPE_A:
	    {
		FILE *fin;
		int c;
		off_t count;
		struct stat stbuf;
		fin = fopen(filename, "r");
		if (fin == NULL) {
			perror_reply(550, filename);
			return;
		}
		if (fstat(fileno(fin), &stbuf) < 0 || !S_ISREG(stbuf.st_mode)) {
			reply(550, "%s: not a plain file.", filename);
			(void) fclose(fin);
			return;
		}
		if (stbuf.st_size > 10240) {
			reply(550, "%s: file too large for SIZE.", filename);
			(void) fclose(fin);
			return;
		}

		count = 0;
		while((c = getc(fin)) != EOF) {
			if (c == '\n')	/* will get expanded to \r\n */
				count++;
			count++;
		}
		(void) fclose(fin);

		reply(213, LLF, (LLT)count);
		break;
	    }
	default:
		reply(504, "SIZE not implemented for Type %c.", "?AEIL"[type]);
	}
}

void
statfilecmd(const char *filename)
{
	FILE *fin;
	int c;
	int atstart;
	const char *argv[] = { INTERNAL_LS, "-lgA", "", NULL };

	argv[2] = filename;
	fin = ftpd_popen(argv, "r", STDOUT_FILENO);
	reply(-211, "status of %s:", filename);
/* XXX: use fgetln() or fparseln() here? */
	atstart = 1;
	while ((c = getc(fin)) != EOF) {
		if (c == '\n') {
			if (ferror(stdout)){
				perror_reply(421, "control connection");
				(void) ftpd_pclose(fin);
				dologout(1);
				/* NOTREACHED */
			}
			if (ferror(fin)) {
				perror_reply(551, filename);
				(void) ftpd_pclose(fin);
				return;
			}
			CPUTC('\r', stdout);
		}
		if (atstart && isdigit(c))
			CPUTC(' ', stdout);
		CPUTC(c, stdout);
		atstart = (c == '\n');
	}
	(void) ftpd_pclose(fin);
	reply(211, "End of Status");
}

/* -- */

static void
ack(const char *s)
{

	reply(250, "%s command successful.", s);
}

/*
 * Encode len bytes starting at clear using base64 encoding into encoded,
 * which should be at least ((len + 2) * 4 / 3 + 1) in size.
 * If nulterm is non-zero, terminate with \0 otherwise pad to 3 byte boundary
 * with `='.
 */
static void
base64_encode(const char *clear, size_t len, char *encoded, int nulterm)
{
	static const char base64[] =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	const char *c;
	char	*e, termchar;
	int	 i;

			/* determine whether to pad with '=' or NUL terminate */
	termchar = nulterm ? '\0' : '=';
	c = clear;
	e = encoded;
			/* convert all but last 2 bytes */
	for (i = len; i > 2; i -= 3, c += 3) {
		*e++ = base64[(c[0] >> 2) & 0x3f];
		*e++ = base64[((c[0] << 4) & 0x30) | ((c[1] >> 4) & 0x0f)];
		*e++ = base64[((c[1] << 2) & 0x3c) | ((c[2] >> 6) & 0x03)];
		*e++ = base64[(c[2]) & 0x3f];
	}
			/* handle slop at end */
	if (i > 0) {
		*e++ = base64[(c[0] >> 2) & 0x3f];
		*e++ = base64[((c[0] << 4) & 0x30) |
		     (i > 1 ? ((c[1] >> 4) & 0x0f) : 0)];
		*e++ = (i > 1) ? base64[(c[1] << 2) & 0x3c] : termchar;
		*e++ = termchar;
	}
	*e = '\0';
}

static void
fact_modify(const char *fact, FILE *fd, factelem *fe)
{
	struct tm *t;

	t = gmtime(&(fe->stat->st_mtime));
	cprintf(fd, "%s=%04d%02d%02d%02d%02d%02d;", fact,
	    TM_YEAR_BASE + t->tm_year,
	    t->tm_mon+1, t->tm_mday,
	    t->tm_hour, t->tm_min, t->tm_sec);
}

static void
fact_perm(const char *fact, FILE *fd, factelem *fe)
{
	int		rok, wok, xok, pdirwok;
	struct stat	*pdir;

	if (fe->stat->st_uid == geteuid()) {
		rok = ((fe->stat->st_mode & S_IRUSR) != 0);
		wok = ((fe->stat->st_mode & S_IWUSR) != 0);
		xok = ((fe->stat->st_mode & S_IXUSR) != 0);
	} else if (matchgroup(fe->stat->st_gid)) {
		rok = ((fe->stat->st_mode & S_IRGRP) != 0);
		wok = ((fe->stat->st_mode & S_IWGRP) != 0);
		xok = ((fe->stat->st_mode & S_IXGRP) != 0);
	} else {
		rok = ((fe->stat->st_mode & S_IROTH) != 0);
		wok = ((fe->stat->st_mode & S_IWOTH) != 0);
		xok = ((fe->stat->st_mode & S_IXOTH) != 0);
	}

	cprintf(fd, "%s=", fact);

			/*
			 * if parent info not provided, look it up, but
			 * only if the current class has modify rights,
			 * since we only need this info in such a case.
			 */
	pdir = fe->pdirstat;
	if (pdir == NULL && CURCLASS_FLAGS_ISSET(modify)) {
		size_t		len;
		char		realdir[MAXPATHLEN], *p;
		struct stat	dir;

		len = strlcpy(realdir, fe->path, sizeof(realdir));
		if (len < sizeof(realdir) - 4) {
			if (S_ISDIR(fe->stat->st_mode))
				strlcat(realdir, "/..", sizeof(realdir));
			else {
					/* if has a /, move back to it */
					/* otherwise use '..' */
				if ((p = strrchr(realdir, '/')) != NULL) {
					if (p == realdir)
						p++;
					*p = '\0';
				} else
					strlcpy(realdir, "..", sizeof(realdir));
			}
			if (stat(realdir, &dir) == 0)
				pdir = &dir;
		}
	}
	pdirwok = 0;
	if (pdir != NULL) {
		if (pdir->st_uid == geteuid())
			pdirwok = ((pdir->st_mode & S_IWUSR) != 0);
		else if (matchgroup(pdir->st_gid))
			pdirwok = ((pdir->st_mode & S_IWGRP) != 0);
		else
			pdirwok = ((pdir->st_mode & S_IWOTH) != 0);
	}

			/* 'a': can APPE to file */
	if (wok && CURCLASS_FLAGS_ISSET(upload) && S_ISREG(fe->stat->st_mode))
		CPUTC('a', fd);

			/* 'c': can create or append to files in directory */
	if (wok && CURCLASS_FLAGS_ISSET(modify) && S_ISDIR(fe->stat->st_mode))
		CPUTC('c', fd);

			/* 'd': can delete file or directory */
	if (pdirwok && CURCLASS_FLAGS_ISSET(modify)) {
		int candel;

		candel = 1;
		if (S_ISDIR(fe->stat->st_mode)) {
			DIR *dirp;
			struct dirent *dp;

			if ((dirp = opendir(fe->display)) == NULL)
				candel = 0;
			else {
				while ((dp = readdir(dirp)) != NULL) {
					if (ISDOTDIR(dp->d_name) ||
					    ISDOTDOTDIR(dp->d_name))
						continue;
					candel = 0;
					break;
				}
				closedir(dirp);
			}
		}
		if (candel)
			CPUTC('d', fd);
	}

			/* 'e': can enter directory */
	if (xok && S_ISDIR(fe->stat->st_mode))
		CPUTC('e', fd);

			/* 'f': can rename file or directory */
	if (pdirwok && CURCLASS_FLAGS_ISSET(modify))
		CPUTC('f', fd);

			/* 'l': can list directory */
	if (rok && xok && S_ISDIR(fe->stat->st_mode))
		CPUTC('l', fd);

			/* 'm': can create directory */
	if (wok && CURCLASS_FLAGS_ISSET(modify) && S_ISDIR(fe->stat->st_mode))
		CPUTC('m', fd);

			/* 'p': can remove files in directory */
	if (wok && CURCLASS_FLAGS_ISSET(modify) && S_ISDIR(fe->stat->st_mode))
		CPUTC('p', fd);

			/* 'r': can RETR file */
	if (rok && S_ISREG(fe->stat->st_mode))
		CPUTC('r', fd);

			/* 'w': can STOR file */
	if (wok && CURCLASS_FLAGS_ISSET(upload) && S_ISREG(fe->stat->st_mode))
		CPUTC('w', fd);

	CPUTC(';', fd);
}

static void
fact_size(const char *fact, FILE *fd, factelem *fe)
{

	if (S_ISREG(fe->stat->st_mode))
		cprintf(fd, "%s=" LLF ";", fact, (LLT)fe->stat->st_size);
}

static void
fact_type(const char *fact, FILE *fd, factelem *fe)
{

	cprintf(fd, "%s=", fact);
	switch (fe->stat->st_mode & S_IFMT) {
	case S_IFDIR:
		if (fe->flags & FE_MLSD) {
			if ((fe->flags & FE_ISCURDIR) || ISDOTDIR(fe->display))
				cprintf(fd, "cdir");
			else if (ISDOTDOTDIR(fe->display))
				cprintf(fd, "pdir");
			else
				cprintf(fd, "dir");
		} else {
			cprintf(fd, "dir");
		}
		break;
	case S_IFREG:
		cprintf(fd, "file");
		break;
	case S_IFIFO:
		cprintf(fd, "OS.unix=fifo");
		break;
	case S_IFLNK:		/* XXX: probably a NO-OP with stat() */
		cprintf(fd, "OS.unix=slink");
		break;
	case S_IFSOCK:
		cprintf(fd, "OS.unix=socket");
		break;
	case S_IFBLK:
	case S_IFCHR:
		cprintf(fd, "OS.unix=%s-" ULLF "/" ULLF,
		    S_ISBLK(fe->stat->st_mode) ? "blk" : "chr",
		    (ULLT)major(fe->stat->st_rdev),
		    (ULLT)minor(fe->stat->st_rdev));
		break;
	default:
		cprintf(fd, "OS.unix=UNKNOWN(0%o)", fe->stat->st_mode & S_IFMT);
		break;
	}
	CPUTC(';', fd);
}

static void
fact_unique(const char *fact, FILE *fd, factelem *fe)
{
	char obuf[(sizeof(dev_t) + sizeof(ino_t) + 2) * 4 / 3 + 2];
	char tbuf[sizeof(dev_t) + sizeof(ino_t)];

	memcpy(tbuf,
	    (char *)&(fe->stat->st_dev), sizeof(dev_t));
	memcpy(tbuf + sizeof(dev_t),
	    (char *)&(fe->stat->st_ino), sizeof(ino_t));
	base64_encode(tbuf, sizeof(dev_t) + sizeof(ino_t), obuf, 1);
	cprintf(fd, "%s=%s;", fact, obuf);
}

static int
matchgroup(gid_t gid)
{
	int	i;

	for (i = 0; i < gidcount; i++)
		if (gid == gidlist[i])
			return(1);
	return (0);
}

static void
mlsname(FILE *fp, factelem *fe)
{
	char realfile[MAXPATHLEN];
	int userf = 0;
	size_t i;

	for (i = 0; i < FACTTABSIZE; i++) {
		if (facttab[i].enabled)
			(facttab[i].display)(facttab[i].name, fp, fe);
	}
	if ((fe->flags & FE_MLSD) &&
	    !(fe->flags & FE_ISCURDIR) && !ISDOTDIR(fe->display)) {
			/* if MLSD and not "." entry, display as-is */
		userf = 0;
	} else {
			/* if MLST, or MLSD and "." entry, realpath(3) it */
		if (realpath(fe->display, realfile) != NULL)
			userf = 1;
	}
	cprintf(fp, " %s\r\n", userf ? realfile : fe->display);
}

static void
replydirname(const char *name, const char *message)
{
	char *p, *ep;
	char npath[MAXPATHLEN * 2];

	p = npath;
	ep = &npath[sizeof(npath) - 1];
	while (*name) {
		if (*name == '"') {
			if (ep - p < 2)
				break;
			*p++ = *name++;
			*p++ = '"';
		} else {
			if (ep - p < 1)
				break;
			*p++ = *name++;
		}
	}
	*p = '\0';
	reply(257, "\"%s\" %s", npath, message);
}

static void
discover_path(char *last_path, const char *new_path) 
{
	char tp[MAXPATHLEN + 1] = "";
	char tq[MAXPATHLEN + 1] = "";
	char *cp;
	char *cq; 
	int sz1, sz2;
	int nomorelink;
	struct stat st1, st2;
	
	if (new_path[0] != '/') {
		(void)strlcpy(tp, last_path, MAXPATHLEN);
		(void)strlcat(tp, "/", MAXPATHLEN);
	}
	(void)strlcat(tp, new_path, MAXPATHLEN);
	(void)strlcat(tp, "/", MAXPATHLEN);

	/* 
	 * resolve symlinks. A symlink may introduce another symlink, so we
	 * loop trying to resolve symlinks until we don't find any of them.
	 */
	do {
		/* Collapse any // into / */
		while ((cp = strstr(tp, "//")) != NULL)
			(void)memmove(cp, cp + 1, strlen(cp) - 1 + 1);

		/* Collapse any /./ into / */
		while ((cp = strstr(tp, "/./")) != NULL)
			(void)memmove(cp, cp + 2, strlen(cp) - 2 + 1);

		cp = tp;
		nomorelink = 1;
		
		while ((cp = strstr(++cp, "/")) != NULL) {
			sz1 = (unsigned long)cp - (unsigned long)tp;
			if (sz1 > MAXPATHLEN)
				goto bad;
			*cp = 0;
			sz2 = readlink(tp, tq, MAXPATHLEN); 
			*cp = '/';

			/* If this is not a symlink, move to next / */
			if (sz2 <= 0)
				continue;

			/*
			 * We found a symlink, so we will have to 
			 * do one more pass to check there is no 
			 * more symlink in the path
			 */
			nomorelink = 0;

			/* 
			 * Null terminate the string and remove trailing /
			 */
			tq[sz2] = 0;
			sz2 = strlen(tq);
			if (tq[sz2 - 1] == '/') 
				tq[--sz2] = 0;

			/* 
			 * Is this an absolute link or a relative link? 
			 */
			if (tq[0] == '/') {
				/* absolute link */
				if (strlen(cp) + sz2 > MAXPATHLEN)
					goto bad;
				memmove(tp + sz2, cp, strlen(cp) + 1);
				memcpy(tp, tq, sz2);
			} else {			
				/* relative link */
				for (cq = cp - 1; *cq != '/'; cq--);
				if (strlen(tp) -
				    ((unsigned long)cq - (unsigned long)cp)
				    + 1 + sz2 > MAXPATHLEN)
					goto bad;
				(void)memmove(cq + 1 + sz2, 
				    cp, strlen(cp) + 1);
				(void)memcpy(cq + 1, tq, sz2);
			}

			/* 
			 * start over, looking for new symlinks 
			 */
			break;
		}
	} while (nomorelink == 0);

	/* Collapse any /foo/../ into /foo/ */
	while ((cp = strstr(tp, "/../")) != NULL) {
		/* ^/../foo/ becomes ^/foo/ */
		if (cp == tp) {
			(void)memmove(cp, cp + 3,
			    strlen(cp) - 3 + 1);
		} else {
			for (cq = cp - 1; *cq != '/'; cq--);
			(void)memmove(cq, cp + 3,
			    strlen(cp) - 3 + 1);
		}
	}

	/* strip strailing / */
	if (strlen(tp) != 1)
		tp[strlen(tp) - 1] = '\0';

	/* check that the path is correct */
	stat(tp, &st1);
	stat(".", &st2);
	if ((st1.st_dev != st2.st_dev) || (st1.st_ino != st2.st_ino))
		goto bad;

	(void)strlcpy(last_path, tp, MAXPATHLEN);
	return;

bad:
	(void)strlcat(last_path, "/", MAXPATHLEN);
	(void)strlcat(last_path, new_path, MAXPATHLEN);
	return;
}


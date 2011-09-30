/* $NetBSD: user.c,v 1.126 2011/01/04 10:30:21 wiz Exp $ */

/*
 * Copyright (c) 1999 Alistair G. Crooks.  All rights reserved.
 * Copyright (c) 2005 Liam J. Foy.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>

#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1999\
 The NetBSD Foundation, Inc.  All rights reserved.");
__RCSID("$NetBSD: user.c,v 1.126 2011/01/04 10:30:21 wiz Exp $");
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <grp.h>
#ifdef EXTENSIONS
#include <login_cap.h>
#endif
#include <paths.h>
#include <pwd.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <util.h>
#include <errno.h>

#include "defs.h"
#include "usermgmt.h"


/* this struct describes a uid range */
typedef struct range_t {
	int	r_from;		/* low uid */
	int	r_to;		/* high uid */
} range_t;

typedef struct rangelist_t {
	unsigned	rl_rsize;		/* size of range array */
	unsigned	rl_rc;			/* # of ranges */
	range_t	       *rl_rv;			/* the ranges */
	unsigned	rl_defrc;		/* # of ranges in defaults */
} rangelist_t;

/* this struct encapsulates the user and group information */
typedef struct user_t {
	int		u_flags;		/* see below */
	int		u_uid;			/* uid of user */
	char	       *u_password;		/* encrypted password */
	char	       *u_comment;		/* comment field */
	char	       *u_home;			/* home directory */
	mode_t		u_homeperm;		/* permissions of home dir */
	char	       *u_primgrp;		/* primary group */
	int		u_groupc;		/* # of secondary groups */
	const char     *u_groupv[NGROUPS_MAX];	/* secondary groups */
	char	       *u_shell;		/* user's shell */
	char	       *u_basedir;		/* base directory for home */
	char	       *u_expire;		/* when password will expire */
	char	       *u_inactive;		/* when account will expire */
	char	       *u_skeldir;		/* directory for startup files */
	char	       *u_class;		/* login class */
	rangelist_t 	u_r;			/* list of ranges */
	unsigned	u_defrc;		/* # of ranges in defaults */
	int		u_preserve;		/* preserve uids on deletion */
	int		u_allow_samba;		/* allow trailing '$' for samba login names */
	int		u_locked;		/* user account lock */
} user_t;
#define u_rsize u_r.rl_rsize
#define u_rc    u_r.rl_rc
#define u_rv    u_r.rl_rv
#define u_defrc u_r.rl_defrc

/* this struct encapsulates the user and group information */
typedef struct group_t {
	rangelist_t	g_r;			/* list of ranges */
} group_t;
#define g_rsize g_r.rl_rsize
#define g_rc    g_r.rl_rc
#define g_rv    g_r.rl_rv
#define g_defrc g_r.rl_defrc

typedef struct def_t {
	user_t user;
	group_t group;
} def_t;

/* flags for which fields of the user_t replace the passwd entry */
enum {
	F_COMMENT	= 0x0001,
	F_DUPUID  	= 0x0002,
	F_EXPIRE	= 0x0004,
	F_GROUP		= 0x0008,
	F_HOMEDIR	= 0x0010,
	F_MKDIR		= 0x0020,
	F_INACTIVE	= 0x0040,
	F_PASSWORD	= 0x0080,
	F_SECGROUP	= 0x0100,
	F_SHELL 	= 0x0200,
	F_UID		= 0x0400,
	F_USERNAME	= 0x0800,
	F_CLASS		= 0x1000
};

#define	UNLOCK		0
#define LOCK		1
#define LOCKED		"*LOCKED*"

#define	PATH_LOGINCONF	"/etc/login.conf"

#ifndef DEF_GROUP
#define DEF_GROUP	"users"
#endif

#ifndef DEF_BASEDIR
#define DEF_BASEDIR	"/home"
#endif

#ifndef DEF_SKELDIR
#ifdef __minix
#define DEF_SKELDIR	"/usr/ast"
#else
#define DEF_SKELDIR	"/etc/skel"
#endif
#endif

#ifndef DEF_SHELL
#define DEF_SHELL	_PATH_BSHELL
#endif

#ifndef DEF_COMMENT
#define DEF_COMMENT	""
#endif

#ifndef DEF_LOWUID
#define DEF_LOWUID	1000
#endif

#ifndef DEF_HIGHUID
#define DEF_HIGHUID	60000
#endif

#ifndef DEF_INACTIVE
#define DEF_INACTIVE	0
#endif

#ifndef DEF_EXPIRE
#define DEF_EXPIRE	NULL
#endif

#ifndef DEF_CLASS
#define DEF_CLASS	""
#endif

#ifndef WAITSECS
#define WAITSECS	10
#endif

#ifndef NOBODY_UID
#define NOBODY_UID	32767
#endif

#ifndef DEF_HOMEPERM
#define	DEF_HOMEPERM	0755
#endif

/* some useful constants */
enum {
	MaxShellNameLen = 256,
	MaxFileNameLen = MAXPATHLEN,
	MaxUserNameLen = LOGIN_NAME_MAX - 1,
	MaxCommandLen = 2048,
	MaxEntryLen = 2048,
	PasswordLength = 2048,

	DES_Len = 13,
};

/* Full paths of programs used here */
#define CHMOD		"/bin/chmod"
#define CHOWN		"/usr/bin/chown"
#define MKDIR		"/bin/mkdir"
#define MV		"/bin/mv"
#define NOLOGIN		"/sbin/nologin"
#define PAX		"/bin/pax"
#define RM		"/bin/rm"

#define UNSET_INACTIVE	"Null (unset)"
#define UNSET_EXPIRY	"Null (unset)"

static int		asystem(const char *fmt, ...)
			    __attribute__((__format__(__printf__, 1, 2)));
static int		is_number(const char *);
static struct group	*find_group_info(const char *);
static int		verbose;

static char *
skipspace(char *s)
{
	for (; *s && isspace((unsigned char)*s) ; s++) {
	}
	return s;
}

static int
check_numeric(const char *val, const char *name)
{
	if (!is_number(val)) {
		errx(EXIT_FAILURE, "When using [-%c %s], "
		    "the %s must be numeric", *name, name, name);
	}
	return atoi(val);
}

/* resize *cpp appropriately then assign `n' chars of `s' to it */
static void
memsave(char **cpp, const char *s, size_t n)
{
	RENEW(char, *cpp, n + 1, exit(1));
	(void)memcpy(*cpp, s, n);
	(*cpp)[n] = '\0';
}

/* a replacement for system(3) */
static int
asystem(const char *fmt, ...)
{
	va_list	vp;
	char	buf[MaxCommandLen];
	int	ret;

	va_start(vp, fmt);
	(void)vsnprintf(buf, sizeof(buf), fmt, vp);
	va_end(vp);
	if (verbose) {
		(void)printf("Command: %s\n", buf);
	}
	if ((ret = system(buf)) != 0) {
		warn("Error running `%s'", buf);
	}
	return ret;
}

/* remove a users home directory, returning 1 for success (ie, no problems encountered) */
static int
removehomedir(struct passwd *pwp)
{
	struct stat st;

	/* userid not root? */
	if (pwp->pw_uid == 0) {
		warnx("Not deleting home directory `%s'; userid is 0", pwp->pw_dir);
		return 0;
	}

	/* directory exists (and is a directory!) */
	if (stat(pwp->pw_dir, &st) < 0) {
		warn("Cannot access home directory `%s'", pwp->pw_dir);
		return 0;
	}
	if (!S_ISDIR(st.st_mode)) {
		warnx("Home directory `%s' is not a directory", pwp->pw_dir);
		return 0;
	}

	/* userid matches directory owner? */
	if (st.st_uid != pwp->pw_uid) {
		warnx("User `%s' doesn't own directory `%s', not removed",
		    pwp->pw_name, pwp->pw_dir);
		return 0;
	}

	(void)seteuid(pwp->pw_uid);
	/* we add the "|| true" to keep asystem() quiet if there is a non-zero exit status. */
	(void)asystem("%s -rf %s > /dev/null 2>&1 || true", RM, pwp->pw_dir);
	(void)seteuid(0);
	if (rmdir(pwp->pw_dir) < 0) {
		warn("Unable to remove all files in `%s'", pwp->pw_dir);
		return 0;
	}
	return 1;
}

/* return 1 if all of `s' is numeric */
static int
is_number(const char *s)
{
	for ( ; *s ; s++) {
		if (!isdigit((unsigned char) *s)) {
			return 0;
		}
	}
	return 1;
}

/*
 * check that the effective uid is 0 - called from funcs which will
 * modify data and config files.
 */
static void
checkeuid(void)
{
	if (geteuid() != 0) {
		errx(EXIT_FAILURE, "Program must be run as root");
	}
}

/* copy any dot files into the user's home directory */
static int
copydotfiles(char *skeldir, int uid, int gid, char *dir, mode_t homeperm)
{
	struct dirent	*dp;
	DIR		*dirp;
	int		n;

	if ((dirp = opendir(skeldir)) == NULL) {
		warn("Can't open source . files dir `%s'", skeldir);
		return 0;
	}
	for (n = 0; (dp = readdir(dirp)) != NULL && n == 0 ; ) {
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0) {
			continue;
		}
		n = 1;
	}
	(void)closedir(dirp);
	if (n == 0) {
		warnx("No \"dot\" initialisation files found");
	} else {
		(void)asystem("cd %s && %s -rw -pe %s . %s",
				skeldir, PAX, (verbose) ? "-v" : "", dir);
	}
	(void)asystem("%s -R -h %d:%d %s", CHOWN, uid, gid, dir);
	(void)asystem("%s -R u+w %s", CHMOD, dir);
#ifdef EXTENSIONS
	(void)asystem("%s 0%o %s", CHMOD, homeperm, dir);
#endif
	return n;
}

/* create a group entry with gid `gid' */
static int
creategid(char *group, int gid, const char *name)
{
	struct stat	st;
	FILE		*from;
	FILE		*to;
	char		buf[MaxEntryLen];
	char		f[MaxFileNameLen];
	int		fd;
	int		cc;

	if (getgrnam(group) != NULL) {
		warnx("Can't create group `%s': already exists", group);
		return 0;
	}
	if ((from = fopen(_PATH_GROUP, "r+")) == NULL) {
		warn("Can't create group `%s': can't open `%s'", name,
		    _PATH_GROUP);
		return 0;
	}
	if (flock(fileno(from), LOCK_EX | LOCK_NB) < 0) {
		warn("Can't lock `%s'", _PATH_GROUP);
		(void)fclose(from);
		return 0;
	}
	(void)fstat(fileno(from), &st);
	(void)snprintf(f, sizeof(f), "%s.XXXXXX", _PATH_GROUP);
	if ((fd = mkstemp(f)) < 0) {
		warn("Can't create group `%s': mkstemp failed", group);
		(void)fclose(from);
		return 0;
	}
	if ((to = fdopen(fd, "w")) == NULL) {
		warn("Can't create group `%s': fdopen `%s' failed",
		    group, f);
		(void)fclose(from);
		(void)close(fd);
		(void)unlink(f);
		return 0;
	}
	while ((cc = fread(buf, sizeof(char), sizeof(buf), from)) > 0) {
		if (fwrite(buf, sizeof(char), (unsigned) cc, to) != cc) {
			warn("Can't create group `%s': short write to `%s'",
			    group, f);
			(void)fclose(from);
			(void)close(fd);
			(void)unlink(f);
			return 0;
		}
	}
	(void)fprintf(to, "%s:*:%d:%s\n", group, gid, name);
	(void)fclose(from);
	(void)fclose(to);
	if (rename(f, _PATH_GROUP) < 0) {
		warn("Can't create group `%s': can't rename `%s' to `%s'",
		    group, f, _PATH_GROUP);
		(void)unlink(f);
		return 0;
	}
	(void)chmod(_PATH_GROUP, st.st_mode & 07777);
	syslog(LOG_INFO, "New group added: name=%s, gid=%d", group, gid);
	return 1;
}

/* modify the group entry with name `group' to be newent */
static int
modify_gid(char *group, char *newent)
{
	struct stat	st;
	FILE		*from;
	FILE		*to;
	char		buf[MaxEntryLen];
	char		f[MaxFileNameLen];
	char		*colon;
	int		groupc;
	int		entc;
	int		fd;
	int		cc;

	if ((from = fopen(_PATH_GROUP, "r+")) == NULL) {
		warn("Can't modify group `%s': can't open `%s'",
		    group, _PATH_GROUP);
		return 0;
	}
	if (flock(fileno(from), LOCK_EX | LOCK_NB) < 0) {
		warn("Can't modify group `%s': can't lock `%s'",
		    group, _PATH_GROUP);
		(void)fclose(from);
		return 0;
	}
	(void)fstat(fileno(from), &st);
	(void)snprintf(f, sizeof(f), "%s.XXXXXX", _PATH_GROUP);
	if ((fd = mkstemp(f)) < 0) {
		warn("Can't modify group `%s': mkstemp failed", group);
		(void)fclose(from);
		return 0;
	}
	if ((to = fdopen(fd, "w")) == NULL) {
		warn("Can't modify group `%s': fdopen `%s' failed", group, f);
		(void)fclose(from);
		(void)close(fd);
		(void)unlink(f);
		return 0;
	}
	groupc = strlen(group);
	while (fgets(buf, sizeof(buf), from) != NULL) {
		cc = strlen(buf);
		if ((colon = strchr(buf, ':')) == NULL) {
			warnx("Badly formed entry `%s'", buf);
			continue;
		}
		entc = (int)(colon - buf);
		if (entc == groupc &&
		    strncmp(group, buf, (unsigned) entc) == 0) {
			if (newent == NULL) {
				struct group	*grp_rm;
				struct passwd	*user_pwd;

				/*
				 * Check that the group being removed
				 * isn't any user's Primary group. Just
				 * warn if it is. This could cause problems
				 * if the group GID was reused for a
				 * different purpose.
				 */

				grp_rm = find_group_info(group);
				while ((user_pwd = getpwent()) != NULL) {
					if (user_pwd->pw_gid == grp_rm->gr_gid) {
						warnx("Warning: group `%s'(%d)"
						   " is the primary group of"
						   " `%s'. Use caution if you"
						   " later add this GID.",
						   grp_rm->gr_name,
						   grp_rm->gr_gid, user_pwd->pw_name);
					}
				}
				endpwent();
				continue;
			} else {
				cc = strlen(newent);
				(void)strlcpy(buf, newent, sizeof(buf));
			}
		}
		if (fwrite(buf, sizeof(char), (unsigned) cc, to) != cc) {
			warn("Can't modify group `%s': short write to `%s'",
			    group, f);
			(void)fclose(from);
			(void)close(fd);
			(void)unlink(f);
			return 0;
		}
	}
	(void)fclose(from);
	(void)fclose(to);
	if (rename(f, _PATH_GROUP) < 0) {
		warn("Can't modify group `%s': can't rename `%s' to `%s'",
		    group, f, _PATH_GROUP);
		(void)unlink(f);
		return 0;
	}
	(void)chmod(_PATH_GROUP, st.st_mode & 07777);
	if (newent == NULL) {
		syslog(LOG_INFO, "group deleted: name=%s", group);
	} else {
		syslog(LOG_INFO, "group information modified: name=%s", group);
	}
	return 1;
}

/* modify the group entries for all `groups', by adding `user' */
static int
append_group(char *user, int ngroups, const char **groups)
{
	struct group	*grp;
	struct stat	st;
	FILE		*from;
	FILE		*to;
	char		buf[MaxEntryLen];
	char		f[MaxFileNameLen];
	char		*colon;
	int		groupc;
	int		entc;
	int		fd;
	int		nc;
	int		cc;
	int		i;
	int		j;

	for (i = 0 ; i < ngroups ; i++) {
		if ((grp = getgrnam(groups[i])) == NULL) {
			warnx("Can't append group `%s' for user `%s'",
			    groups[i], user);
		} else {
			for (j = 0 ; grp->gr_mem[j] ; j++) {
				if (strcmp(user, grp->gr_mem[j]) == 0) {
					/* already in it */
					groups[i] = "";
				}
			}
		}
	}
	if ((from = fopen(_PATH_GROUP, "r+")) == NULL) {
		warn("Can't append group(s) for `%s': can't open `%s'",
		    user, _PATH_GROUP);
		return 0;
	}
	if (flock(fileno(from), LOCK_EX | LOCK_NB) < 0) {
		warn("Can't append group(s) for `%s': can't lock `%s'",
		    user, _PATH_GROUP);
		(void)fclose(from);
		return 0;
	}
	(void)fstat(fileno(from), &st);
	(void)snprintf(f, sizeof(f), "%s.XXXXXX", _PATH_GROUP);
	if ((fd = mkstemp(f)) < 0) {
		warn("Can't append group(s) for `%s': mkstemp failed",
		    user);
		(void)fclose(from);
		return 0;
	}
	if ((to = fdopen(fd, "w")) == NULL) {
		warn("Can't append group(s) for `%s': fdopen `%s' failed",
		    user, f);
		(void)fclose(from);
		(void)close(fd);
		(void)unlink(f);
		return 0;
	}
	while (fgets(buf, sizeof(buf), from) != NULL) {
		cc = strlen(buf);
		if ((colon = strchr(buf, ':')) == NULL) {
			warnx("Badly formed entry `%s'", buf);
			continue;
		}
		entc = (int)(colon - buf);
		for (i = 0 ; i < ngroups ; i++) {
			if ((groupc = strlen(groups[i])) == 0) {
				continue;
			}
			if (entc == groupc &&
			    strncmp(groups[i], buf, (unsigned) entc) == 0) {
				if ((nc = snprintf(&buf[cc - 1],
				    sizeof(buf) - cc + 1, "%s%s\n",
				    (buf[cc - 2] == ':') ? "" : ",", user)) < 0) {
					warnx("Warning: group `%s' "
					    "entry too long", groups[i]);
				}
				cc += nc - 1;
			}
		}
		if (fwrite(buf, sizeof(char), (unsigned) cc, to) != cc) {
			warn("Can't append group(s) for `%s':"
			    " short write to `%s'", user, f);
			(void)fclose(from);
			(void)close(fd);
			(void)unlink(f);
			return 0;
		}
	}
	(void)fclose(from);
	(void)fclose(to);
	if (rename(f, _PATH_GROUP) < 0) {
		warn("Can't append group(s) for `%s': "
		    "can't rename `%s' to `%s'", user, f, _PATH_GROUP);
		(void)unlink(f);
		return 0;
	}
	(void)chmod(_PATH_GROUP, st.st_mode & 07777);
	return 1;
}

/* the valid characters for login and group names */
#define VALID_CHAR(c)	(isalnum(c) || (c) == '.' || (c) == '_' || (c) == '-')

/* return 1 if `login' is a valid login name */
static int
valid_login(char *login_name, int allow_samba)
{
	unsigned char	*cp;

	/* First character of a login name cannot be '-'. */
	if (*login_name == '-') {
		return 0;
	}
	if (strlen(login_name) >= LOGIN_NAME_MAX) {
		return 0;
	}
	for (cp = (unsigned char *)login_name ; *cp ; cp++) {
		if (!VALID_CHAR(*cp)) {
#ifdef EXTENSIONS
			/* check for a trailing '$' in a Samba user name */
			if (allow_samba && *cp == '$' && *(cp + 1) == 0x0) {
				return 1;
			}
#endif
			return 0;
		}
	}
	return 1;
}

/* return 1 if `group' is a valid group name */
static int
valid_group(char *group)
{
	unsigned char	*cp;

	for (cp = (unsigned char *)group; *cp; cp++) {
		if (!VALID_CHAR(*cp)) {
			return 0;
		}
	}
	return 1;
}

/* find the next gid in the range lo .. hi */
static int
getnextgid(int *gidp, int lo, int hi)
{
	for (*gidp = lo ; *gidp < hi ; *gidp += 1) {
		if (getgrgid((gid_t)*gidp) == NULL) {
			return 1;
		}
	}
	return 0;
}

#ifdef EXTENSIONS
/* save a range of uids */
static int
save_range(rangelist_t *rlp, char *cp)
{
	int	from;
	int	to;
	int	i;

	if (rlp->rl_rsize == 0) {
		rlp->rl_rsize = 32;
		NEWARRAY(range_t, rlp->rl_rv, rlp->rl_rsize, return(0));
	} else if (rlp->rl_rc == rlp->rl_rsize) {
		rlp->rl_rsize *= 2;
		RENEW(range_t, rlp->rl_rv, rlp->rl_rsize, return(0));
	}
	if (rlp->rl_rv && sscanf(cp, "%d..%d", &from, &to) == 2) {
		for (i = rlp->rl_defrc ; i < rlp->rl_rc ; i++) {
			if (rlp->rl_rv[i].r_from == from &&
			    rlp->rl_rv[i].r_to == to) {
				break;
			}
		}
		if (i == rlp->rl_rc) {
			rlp->rl_rv[rlp->rl_rc].r_from = from;
			rlp->rl_rv[rlp->rl_rc].r_to = to;
			rlp->rl_rc += 1;
		}
	} else {
		warnx("Bad range `%s'", cp);
		return 0;
	}
	return 1;
}
#endif

/* set the defaults in the defaults file */
static int
setdefaults(user_t *up)
{
	char	template[MaxFileNameLen];
	FILE	*fp;
	int	ret;
	int	fd;
#ifdef EXTENSIONS
	int	i;
#endif

	(void)snprintf(template, sizeof(template), "%s.XXXXXX",
	    _PATH_USERMGMT_CONF);
	if ((fd = mkstemp(template)) < 0) {
		warn("Can't set defaults: can't mkstemp `%s' for writing",
		    _PATH_USERMGMT_CONF);
		return 0;
	}
	if ((fp = fdopen(fd, "w")) == NULL) {
		warn("Can't set defaults: can't fdopen `%s' for writing",
		    _PATH_USERMGMT_CONF);
		return 0;
	}
	ret = 1;
	if (fprintf(fp, "group\t\t%s\n", up->u_primgrp) <= 0 ||
	    fprintf(fp, "base_dir\t%s\n", up->u_basedir) <= 0 ||
	    fprintf(fp, "skel_dir\t%s\n", up->u_skeldir) <= 0 ||
	    fprintf(fp, "shell\t\t%s\n", up->u_shell) <= 0 ||
#ifdef EXTENSIONS
	    fprintf(fp, "class\t\t%s\n", up->u_class) <= 0 ||
	    fprintf(fp, "homeperm\t0%o\n", up->u_homeperm) <= 0 ||
#endif
	    fprintf(fp, "inactive\t%s\n", (up->u_inactive == NULL) ?
		UNSET_INACTIVE : up->u_inactive) <= 0 ||
	    fprintf(fp, "expire\t\t%s\n", (up->u_expire == NULL) ?
		UNSET_EXPIRY : up->u_expire) <= 0 ||
	    fprintf(fp, "preserve\t%s\n", (up->u_preserve == 0) ?
		"false" : "true") <= 0) {
		warn("Can't write to `%s'", _PATH_USERMGMT_CONF);
		ret = 0;
	}
#ifdef EXTENSIONS
	for (i = (up->u_defrc != up->u_rc) ? up->u_defrc : 0;
	    i < up->u_rc ; i++) {
		if (fprintf(fp, "range\t\t%d..%d\n", up->u_rv[i].r_from,
		    up->u_rv[i].r_to) <= 0) {
			warn("Can't set defaults: can't write to `%s'",
			    _PATH_USERMGMT_CONF);
			ret = 0;
		}
	}
#endif
	(void)fclose(fp);
	if (ret) {
		ret = ((rename(template, _PATH_USERMGMT_CONF) == 0) &&
		    (chmod(_PATH_USERMGMT_CONF, 0644) == 0));
	}
	return ret;
}

/* read the defaults file */
static void
read_defaults(def_t *dp)
{
	struct stat	st;
	size_t		lineno;
	size_t		len;
	FILE		*fp;
	char		*cp;
	char		*s;
	user_t		*up = &dp->user;
	group_t		*gp = &dp->group;

	(void)memset(dp, 0, sizeof(*dp));

	memsave(&up->u_primgrp, DEF_GROUP, strlen(DEF_GROUP));
	memsave(&up->u_basedir, DEF_BASEDIR, strlen(DEF_BASEDIR));
	memsave(&up->u_skeldir, DEF_SKELDIR, strlen(DEF_SKELDIR));
	memsave(&up->u_shell, DEF_SHELL, strlen(DEF_SHELL));
	memsave(&up->u_comment, DEF_COMMENT, strlen(DEF_COMMENT));
#ifdef EXTENSIONS
	memsave(&up->u_class, DEF_CLASS, strlen(DEF_CLASS));
#endif
	up->u_rsize = 16;
	up->u_defrc = 0;
	NEWARRAY(range_t, up->u_rv, up->u_rsize, exit(1));
	up->u_inactive = DEF_INACTIVE;
	up->u_expire = DEF_EXPIRE;
	gp->g_rsize = 16;
	gp->g_defrc = 0;
	NEWARRAY(range_t, gp->g_rv, gp->g_rsize, exit(1));
	if ((fp = fopen(_PATH_USERMGMT_CONF, "r")) == NULL) {
		if (stat(_PATH_USERMGMT_CONF, &st) < 0 && !setdefaults(up)) {
			warn("Can't create `%s' defaults file",
			    _PATH_USERMGMT_CONF);
		}
		fp = fopen(_PATH_USERMGMT_CONF, "r");
	}
	if (fp != NULL) {
		while ((s = fparseln(fp, &len, &lineno, NULL, 0)) != NULL) {
			if (strncmp(s, "group", 5) == 0) {
				cp = skipspace(s + 5);
				memsave(&up->u_primgrp, (char *)cp, strlen(cp));
			} else if (strncmp(s, "base_dir", 8) == 0) {
				cp = skipspace(s + 8);
				memsave(&up->u_basedir, (char *)cp, strlen(cp));
			} else if (strncmp(s, "skel_dir", 8) == 0) {
				cp = skipspace(s + 8);
				memsave(&up->u_skeldir, (char *)cp, strlen(cp));
			} else if (strncmp(s, "shell", 5) == 0) {
				cp = skipspace(s + 5);
				memsave(&up->u_shell, cp, strlen(cp));
#ifdef EXTENSIONS
			} else if (strncmp((char *)s, "class", 5) == 0) {
				cp = skipspace(s + 5);
				memsave(&up->u_class, cp, strlen(cp));
#endif
#ifdef EXTENSIONS
			} else if (strncmp(s, "homeperm", 8) == 0) {
				for (cp = s + 8; *cp &&
				     isspace((unsigned char)*cp); cp++)
					;
				up->u_homeperm = strtoul(cp, NULL, 8);
#endif
			} else if (strncmp(s, "inactive", 8) == 0) {
				cp = skipspace(s + 8);
				if (strcmp(cp, UNSET_INACTIVE) == 0) {
					if (up->u_inactive) {
						FREE(up->u_inactive);
					}
					up->u_inactive = NULL;
				} else {
					memsave(&up->u_inactive, cp, strlen(cp));
				}
#ifdef EXTENSIONS
			} else if (strncmp(s, "range", 5) == 0) {
				cp = skipspace(s + 5);
				(void)save_range(&up->u_r, cp);
#endif
#ifdef EXTENSIONS
			} else if (strncmp(s, "preserve", 8) == 0) {
				cp = skipspace(s + 8);
				up->u_preserve =
				    (strncmp(cp, "true", 4) == 0) ? 1 :
				    (strncmp(cp, "yes", 3) == 0) ? 1 : atoi(cp);
#endif
			} else if (strncmp(s, "expire", 6) == 0) {
				cp = skipspace(s + 6);
				if (strcmp(cp, UNSET_EXPIRY) == 0) {
					if (up->u_expire) {
						FREE(up->u_expire);
					}
					up->u_expire = NULL;
				} else {
					memsave(&up->u_expire, cp, strlen(cp));
				}
#ifdef EXTENSIONS
			} else if (strncmp(s, "gid_range", 9) == 0) {
				cp = skipspace(s + 9);
				(void)save_range(&gp->g_r, cp);
#endif
			}
			(void)free(s);
		}
		(void)fclose(fp);
	}
	if (up->u_rc == 0) {
		up->u_rv[up->u_rc].r_from = DEF_LOWUID;
		up->u_rv[up->u_rc].r_to = DEF_HIGHUID;
		up->u_rc += 1;
	}
	up->u_defrc = up->u_rc;
	up->u_homeperm = DEF_HOMEPERM;
}

/* return the next valid unused uid */
static int
getnextuid(int sync_uid_gid, int *uid, int low_uid, int high_uid)
{
	for (*uid = low_uid ; *uid <= high_uid ; (*uid)++) {
		if (getpwuid((uid_t)(*uid)) == NULL && *uid != NOBODY_UID) {
			if (sync_uid_gid) {
				if (getgrgid((gid_t)(*uid)) == NULL) {
					return 1;
				}
			} else {
				return 1;
			}
		}
	}
	return 0;
}

/* structure which defines a password type */
typedef struct passwd_type_t {
	const char     *type;		/* optional type descriptor */
	size_t		desc_length;	/* length of type descriptor */
	size_t		length;		/* length of password */
	const char     *regex;		/* regexp to output the password */
	size_t		re_sub;		/* subscript of regexp to use */
} passwd_type_t;

static passwd_type_t	passwd_types[] = {
	{ "$sha1",	5,	28,	"\\$[^$]+\\$[^$]+\\$[^$]+\\$(.*)", 1 },	/* SHA1 */
	{ "$2a",	3,	53,	"\\$[^$]+\\$[^$]+\\$(.*)",	1 },	/* Blowfish */
	{ "$1",		2,	34,	NULL,				0 },	/* MD5 */
	{ "",		0,	DES_Len,NULL,				0 },	/* standard DES */
	{ NULL,		(size_t)~0,	(size_t)~0,	NULL,		0 }
	/* none - terminate search */
};

/* return non-zero if it's a valid password - check length for cipher type */
static int
valid_password_length(char *newpasswd)
{
	passwd_type_t  *pwtp;
	regmatch_t	matchv[10];
	regex_t		r;

	for (pwtp = passwd_types; pwtp->desc_length != (size_t)~0; pwtp++) {
		if (strncmp(newpasswd, pwtp->type, pwtp->desc_length) == 0) {
			if (pwtp->regex == NULL) {
				return strlen(newpasswd) == pwtp->length;
			}
			(void)regcomp(&r, pwtp->regex, REG_EXTENDED);
			if (regexec(&r, newpasswd, 10, matchv, 0) == 0) {
				regfree(&r);
				return (int)(matchv[pwtp->re_sub].rm_eo -
				    matchv[pwtp->re_sub].rm_so) ==
				    pwtp->length;
			}
			regfree(&r);
		}
	}
	return 0;
}

#ifdef EXTENSIONS
/* return 1 if `class' is a valid login class */
static int
valid_class(char *class)
{
	login_cap_t *lc;

	if (class == NULL || *class == '\0') {
		return 1;
	}
	/*
	 * Check if /etc/login.conf exists. login_getclass() will
	 * return 1 due to it not existing, so not informing the
	 * user the actual login class does not exist.
	 */

	if (access(PATH_LOGINCONF, R_OK) == -1) {
		warn("Access failed for `%s'; will not validate class `%s'",
		    PATH_LOGINCONF, class);
		return 1;
	}

	if ((lc = login_getclass(class)) != NULL) {
		login_close(lc);
		return 1;
	}
	return 0;
}

/* return 1 if the `shellname' is a valid user shell */
static int 
valid_shell(const char *shellname)
{
	char *shellp;

	if (access(_PATH_SHELLS, R_OK) == -1) {
		/* Don't exit */
		warn("Access failed for `%s'; will not validate shell `%s'",
		    _PATH_SHELLS, shellname);
		return 1;
	} 

	/* if nologin is used as a shell, consider it a valid shell */
	if (strcmp(shellname, NOLOGIN) == 0)
		return 1;

	while ((shellp = getusershell()) != NULL)
		if (strcmp(shellp, shellname) == 0)
			return 1;

	warnx("Shell `%s' not found in `%s'", shellname, _PATH_SHELLS);

	return access(shellname, X_OK) != -1;
}
#endif

/* look for a valid time, return 0 if it was specified but bad */
static int
scantime(time_t *tp, char *s)
{
	struct tm	tm;
	char *ep;
	long val;

	*tp = 0;
	if (s != NULL) {
		(void)memset(&tm, 0, sizeof(tm));
		if (strptime(s, "%c", &tm) != NULL) {
			*tp = mktime(&tm);
			return (*tp == -1) ? 0 : 1;
		} else if (strptime(s, "%B %d %Y", &tm) != NULL) {
			*tp = mktime(&tm);
			return (*tp == -1) ? 0 : 1;
		} else {
			errno = 0;
			*tp = val = strtol(s, &ep, 10);
			if (*ep != '\0' || *tp < -1 || errno == ERANGE) {
				*tp = 0;
				return 0;
			}
			if (*tp != val) {
				return 0;
			}
		}
	}
	return 1;
}

/* add a user */
static int
adduser(char *login_name, user_t *up)
{
	struct group	*grp;
	struct stat	st;
	time_t		expire;
	time_t		inactive;
	char		password[PasswordLength + 1];
	char		home[MaxFileNameLen];
	char		buf[MaxFileNameLen];
	int		sync_uid_gid;
	int		masterfd;
	int		ptmpfd;
	int		gid;
	int		cc;
	int		i;

	if (!valid_login(login_name, up->u_allow_samba)) {
		errx(EXIT_FAILURE, "Can't add user `%s': invalid login name", login_name);
	}
#ifdef EXTENSIONS
	if (!valid_class(up->u_class)) {
		errx(EXIT_FAILURE, "Can't add user `%s': no such login class `%s'",
		    login_name, up->u_class);
	}
#endif
	if ((masterfd = open(_PATH_MASTERPASSWD, O_RDWR)) < 0) {
		err(EXIT_FAILURE, "Can't add user `%s': can't open `%s'",
		    login_name, _PATH_MASTERPASSWD);
	}
	if (flock(masterfd, LOCK_EX | LOCK_NB) < 0) {
		err(EXIT_FAILURE, "Can't add user `%s': can't lock `%s'",
		    login_name, _PATH_MASTERPASSWD);
	}
	pw_init();
	if ((ptmpfd = pw_lock(WAITSECS)) < 0) {
		int serrno = errno;
		(void)close(masterfd);
		errno = serrno;
		err(EXIT_FAILURE, "Can't add user `%s': can't obtain pw_lock",
		    login_name);
	}
	while ((cc = read(masterfd, buf, sizeof(buf))) > 0) {
		if (write(ptmpfd, buf, (size_t)(cc)) != cc) {
			int serrno = errno;
			(void)close(masterfd);
			(void)close(ptmpfd);
			(void)pw_abort();
			errno = serrno;
			err(EXIT_FAILURE, "Can't add user `%s': "
			    "short write to /etc/ptmp", login_name);
		}
	}
	(void)close(masterfd);
	/* if no uid was specified, get next one in [low_uid..high_uid] range */
	sync_uid_gid = (strcmp(up->u_primgrp, "=uid") == 0);
	if (up->u_uid == -1) {
		int	got_id = 0;

		/*
		 * Look for a free UID in the command line ranges (if any).
		 * These start after the ranges specified in the config file.
		 */
		for (i = up->u_defrc; !got_id && i < up->u_rc ; i++) {
			got_id = getnextuid(sync_uid_gid, &up->u_uid,
					up->u_rv[i].r_from, up->u_rv[i].r_to);
		}
		/*
		 * If there were no free UIDs in the command line ranges,
		 * try the ranges from the config file (there will always
		 * be at least one default).
		 */
		for (i = 0; !got_id && i < up->u_defrc; i++) {
			got_id = getnextuid(sync_uid_gid, &up->u_uid,
					up->u_rv[i].r_from, up->u_rv[i].r_to);
		}
		if (!got_id) {
			(void)close(ptmpfd);
			(void)pw_abort();
			errx(EXIT_FAILURE, "Can't add user `%s': "
			    "can't get next uid for %d", login_name,
			    up->u_uid);
		}
	}
	/* check uid isn't already allocated */
	if (!(up->u_flags & F_DUPUID) && getpwuid((uid_t)(up->u_uid)) != NULL) {
		(void)close(ptmpfd);
		(void)pw_abort();
		errx(EXIT_FAILURE, "Can't add user `%s': "
		    "uid %d is already in use", login_name, up->u_uid);
	}
	/* if -g=uid was specified, check gid is unused */
	if (sync_uid_gid) {
		if (getgrgid((gid_t)(up->u_uid)) != NULL) {
			(void)close(ptmpfd);
			(void)pw_abort();
			errx(EXIT_FAILURE, "Can't add user `%s': "
			    "gid %d is already in use", login_name,
			    up->u_uid);
		}
		gid = up->u_uid;
	} else if ((grp = getgrnam(up->u_primgrp)) != NULL) {
		gid = grp->gr_gid;
	} else if (is_number(up->u_primgrp) &&
		   (grp = getgrgid((gid_t)atoi(up->u_primgrp))) != NULL) {
		gid = grp->gr_gid;
	} else {
		(void)close(ptmpfd);
		(void)pw_abort();
		errx(EXIT_FAILURE, "Can't add user `%s': group %s not found",
		    login_name, up->u_primgrp);
	}
	/* check name isn't already in use */
	if (!(up->u_flags & F_DUPUID) && getpwnam(login_name) != NULL) {
		(void)close(ptmpfd);
		(void)pw_abort();
		errx(EXIT_FAILURE, "Can't add user `%s': "
		    "`%s' is already a user", login_name, login_name);
	}
	if (up->u_flags & F_HOMEDIR) {
		(void)strlcpy(home, up->u_home, sizeof(home));
	} else {
		/* if home directory hasn't been given, make it up */
		(void)snprintf(home, sizeof(home), "%s/%s", up->u_basedir,
		    login_name);
	}
	if (up->u_flags & F_SHELL) {
#ifdef EXTENSIONS
		if (!valid_shell(up->u_shell)) {
			int oerrno = errno;
			(void)close(ptmpfd);
			(void)pw_abort();
			errno = oerrno;
			errx(EXIT_FAILURE, "Can't add user `%s': "
			    "Cannot access shell `%s'",
			    login_name, up->u_shell);
		}
#endif
	}

	if (!scantime(&inactive, up->u_inactive)) {
		warnx("Warning: inactive time `%s' invalid, password expiry off",
				up->u_inactive);
	}
	if (!scantime(&expire, up->u_expire) || expire == -1) {
		warnx("Warning: expire time `%s' invalid, account expiry off",
				up->u_expire);
		expire = 0; /* Just in case. */
	}
	if (lstat(home, &st) < 0 && !(up->u_flags & F_MKDIR)) {
		warnx("Warning: home directory `%s' doesn't exist, "
		    "and -m was not specified", home);
	}
	password[sizeof(password) - 1] = '\0';
	if (up->u_password != NULL && valid_password_length(up->u_password)) {
		(void)strlcpy(password, up->u_password, sizeof(password));
	} else {
		(void)memset(password, '*', DES_Len);
		password[DES_Len] = 0;
		if (up->u_password != NULL) {
			warnx("Password `%s' is invalid: setting it to `%s'",
				up->u_password, password);
		}
	}
	cc = snprintf(buf, sizeof(buf), "%s:%s:%d:%d:%s:%ld:%ld:%s:%s:%s\n",
			login_name,
			password,
			up->u_uid,
			gid,
#ifdef EXTENSIONS
			up->u_class,
#else
			"",
#endif
			(long) inactive,
			(long) expire,
			up->u_comment,
			home,
			up->u_shell);
	if (write(ptmpfd, buf, (size_t) cc) != cc) {
		int serrno = errno;
		(void)close(ptmpfd);
		(void)pw_abort();
		errno = serrno;
		err(EXIT_FAILURE, "Can't add user `%s': write failed",
		    login_name);
	}
	if (up->u_flags & F_MKDIR) {
		if (lstat(home, &st) == 0) {
			(void)close(ptmpfd);
			(void)pw_abort();
			errx(EXIT_FAILURE,
			    "Can't add user `%s': home directory `%s' "
			    "already exists", login_name, home);
		} else {
			if (asystem("%s -p %s", MKDIR, home) != 0) {
				(void)close(ptmpfd);
				(void)pw_abort();
				errx(EXIT_FAILURE, "Can't add user `%s': "
				    "can't mkdir `%s'", login_name, home);
			}
			(void)copydotfiles(up->u_skeldir, up->u_uid, gid, home,
			    up->u_homeperm);
		}
	}
	if (strcmp(up->u_primgrp, "=uid") == 0 &&
	    getgrnam(login_name) == NULL &&
	    !creategid(login_name, gid, login_name)) {
		(void)close(ptmpfd);
		(void)pw_abort();
		errx(EXIT_FAILURE, "Can't add user `%s': can't create gid %d ",
		    login_name, gid);
	}
	if (up->u_groupc > 0 &&
	    !append_group(login_name, up->u_groupc, up->u_groupv)) {
		(void)close(ptmpfd);
		(void)pw_abort();
		errx(EXIT_FAILURE, "Can't add user `%s': can't append "
		    "to new groups", login_name);
	}
	(void)close(ptmpfd);
#if PW_MKDB_ARGC == 2
	if (pw_mkdb(login_name, 0) < 0)
#else
	if (pw_mkdb() < 0)
#endif
	{
		(void)pw_abort();
		errx(EXIT_FAILURE, "Can't add user `%s': pw_mkdb failed",
		    login_name);
	}
	syslog(LOG_INFO, "New user added: name=%s, uid=%d, gid=%d, home=%s, "
	    "shell=%s", login_name, up->u_uid, gid, home, up->u_shell);
	return 1;
}

/* remove a user from the groups file */
static int
rm_user_from_groups(char *login_name)
{
	struct stat	st;
	regmatch_t	matchv[10];
	regex_t		r;
	FILE		*from;
	FILE		*to;
	char		line[MaxEntryLen];
	char		buf[MaxEntryLen];
	char		f[MaxFileNameLen];
	int		fd;
	int		cc;
	int		sc;

	(void)snprintf(line, sizeof(line), "(:|,)(%s)(,|$)", login_name);
	if ((sc = regcomp(&r, line, REG_EXTENDED|REG_NEWLINE)) != 0) {
		(void)regerror(sc, &r, buf, sizeof(buf));
		warnx("Can't compile regular expression `%s' (%s)", line,
		    buf);
		return 0;
	}
	if ((from = fopen(_PATH_GROUP, "r+")) == NULL) {
		warn("Can't remove user `%s' from `%s': can't open `%s'",
		    login_name, _PATH_GROUP, _PATH_GROUP);
		return 0;
	}
	if (flock(fileno(from), LOCK_EX | LOCK_NB) < 0) {
		warn("Can't remove user `%s' from `%s': can't lock `%s'",
		    login_name, _PATH_GROUP, _PATH_GROUP);
		(void)fclose(from);
		return 0;
	}
	(void)fstat(fileno(from), &st);
	(void)snprintf(f, sizeof(f), "%s.XXXXXX", _PATH_GROUP);
	if ((fd = mkstemp(f)) < 0) {
		warn("Can't remove user `%s' from `%s': mkstemp failed",
		    login_name, _PATH_GROUP);
		(void)fclose(from);
		return 0;
	}
	if ((to = fdopen(fd, "w")) == NULL) {
		warn("Can't remove user `%s' from `%s': fdopen `%s' failed",
		    login_name, _PATH_GROUP, f);
		(void)fclose(from);
		(void)close(fd);
		(void)unlink(f);
		return 0;
	}
	while (fgets(buf, sizeof(buf), from) != NULL) {
		cc = strlen(buf);
		if (regexec(&r, buf, 10, matchv, 0) == 0) {
			if (buf[(int)matchv[1].rm_so] == ',') {
				matchv[2].rm_so = matchv[1].rm_so;
			} else if (matchv[2].rm_eo != matchv[3].rm_eo) {
				matchv[2].rm_eo = matchv[3].rm_eo;
			}
			cc -= (int) matchv[2].rm_eo;
			sc = (int) matchv[2].rm_so;
			if (fwrite(buf, sizeof(char), (size_t)sc, to) != sc ||
			    fwrite(&buf[(int)matchv[2].rm_eo], sizeof(char),
				(size_t)cc, to) != cc) {
				warn("Can't remove user `%s' from `%s': "
				    "short write to `%s'", login_name,
				    _PATH_GROUP, f);
				(void)fclose(from);
				(void)close(fd);
				(void)unlink(f);
				return 0;
			}
		} else if (fwrite(buf, sizeof(char), (unsigned) cc, to) != cc) {
			warn("Can't remove user `%s' from `%s': "
			    "short write to `%s'", login_name, _PATH_GROUP, f);
			(void)fclose(from);
			(void)close(fd);
			(void)unlink(f);
			return 0;
		}
	}
	(void)fclose(from);
	(void)fclose(to);
	if (rename(f, _PATH_GROUP) < 0) {
		warn("Can't remove user `%s' from `%s': "
		    "can't rename `%s' to `%s'",
		    login_name, _PATH_GROUP, f, _PATH_GROUP);
		(void)unlink(f);
		return 0;
	}
	(void)chmod(_PATH_GROUP, st.st_mode & 07777);
	return 1;
}

/* check that the user or group is local, not from YP/NIS */
static int
is_local(char *name, const char *file)
{
	FILE	       *fp;
	char		buf[MaxEntryLen];
	size_t		len;
	int		ret;

	if ((fp = fopen(file, "r")) == NULL) {
		err(EXIT_FAILURE, "Can't open `%s'", file);
	}
	len = strlen(name);
	for (ret = 0 ; fgets(buf, sizeof(buf), fp) != NULL ; ) {
		if (strncmp(buf, name, len) == 0 && buf[len] == ':') {
			ret = 1;
			break;
		}
	}
	(void)fclose(fp);
	return ret;
}

/* modify a user */
static int
moduser(char *login_name, char *newlogin, user_t *up, int allow_samba)
{
	struct passwd  *pwp, pw;
	struct group   *grp;
	const char     *homedir;
	char	       *locked_pwd;
	size_t		colonc;
	size_t		loginc;
	size_t		len;
	FILE	       *master;
	char		newdir[MaxFileNameLen];
	char	        buf[MaxEntryLen];
	char		pwbuf[MaxEntryLen];
	char	       *colon;
	int		masterfd;
	int		ptmpfd;
	int		error;

	if (!valid_login(newlogin, allow_samba)) {
		errx(EXIT_FAILURE, "Can't modify user `%s': invalid login name",
		    login_name);
	}
	if (getpwnam_r(login_name, &pw, pwbuf, sizeof(pwbuf), &pwp) != 0
	    || pwp == NULL) {
		errx(EXIT_FAILURE, "Can't modify user `%s': no such user",
		    login_name);
	}
	if (!is_local(login_name, _PATH_MASTERPASSWD)) {
		errx(EXIT_FAILURE, "Can't modify user `%s': must be a local user",
		    login_name);
	}
	/* keep dir name in case we need it for '-m' */
	homedir = pwp->pw_dir;

	if ((masterfd = open(_PATH_MASTERPASSWD, O_RDWR)) < 0) {
		err(EXIT_FAILURE, "Can't modify user `%s': can't open `%s'",
		    login_name, _PATH_MASTERPASSWD);
	}
	if (flock(masterfd, LOCK_EX | LOCK_NB) < 0) {
		err(EXIT_FAILURE, "Can't modify user `%s': can't lock `%s'",
		    login_name, _PATH_MASTERPASSWD);
	}
	pw_init();
	if ((ptmpfd = pw_lock(WAITSECS)) < 0) {
		int serrno = errno;
		(void)close(masterfd);
		errno = serrno;
		err(EXIT_FAILURE, "Can't modify user `%s': "
		    "can't obtain pw_lock", login_name);
	}
	if ((master = fdopen(masterfd, "r")) == NULL) {
		int serrno = errno;
		(void)close(masterfd);
		(void)close(ptmpfd);
		(void)pw_abort();
		errno = serrno;
		err(EXIT_FAILURE, "Can't modify user `%s': "
		    "fdopen fd for %s", login_name, _PATH_MASTERPASSWD);
	}
	if (up != NULL) {
		if (up->u_flags & F_USERNAME) {
			/*
			 * If changing name,
			 * check new name isn't already in use
			 */
			if (strcmp(login_name, newlogin) != 0 &&
			    getpwnam(newlogin) != NULL) {
				(void)close(ptmpfd);
				(void)pw_abort();
				errx(EXIT_FAILURE, "Can't modify user `%s': "
				    "`%s' is already a user", login_name,
				    newlogin);
			}
			pwp->pw_name = newlogin;

			/*
			 * Provide a new directory name in case the
			 * home directory is to be moved.
			 */
			if (up->u_flags & F_MKDIR) {
				(void)snprintf(newdir, sizeof(newdir), "%s/%s",
				    up->u_basedir, newlogin);
				pwp->pw_dir = newdir;
			}
		}
		if (up->u_flags & F_PASSWORD) {
			if (up->u_password != NULL) {
				if (!valid_password_length(up->u_password)) {
					(void)close(ptmpfd);
					(void)pw_abort();
					errx(EXIT_FAILURE,
					    "Can't modify user `%s': "
					    "invalid password: `%s'",
					    login_name, up->u_password);
				}
				if ((locked_pwd =
				    strstr(pwp->pw_passwd, LOCKED)) != NULL) {
					/*
					 * account is locked - keep it locked
					 * and just change the password.
					 */
					if (asprintf(&locked_pwd, "%s%s",
					    LOCKED, up->u_password) == -1) {
						(void)close(ptmpfd);
						(void)pw_abort();
						err(EXIT_FAILURE,
						    "Can't modify user `%s': "
						    "asprintf failed",
						    login_name);
					}
					pwp->pw_passwd = locked_pwd;
				} else {
					pwp->pw_passwd = up->u_password;
				}
			}
		}

		/* check whether we should lock the account. */
		if (up->u_locked == LOCK) {
			/* check to see account if already locked. */
			if ((locked_pwd = strstr(pwp->pw_passwd, LOCKED))
			    != NULL) {
				warnx("Account is already locked");
			} else {
				if (asprintf(&locked_pwd, "%s%s", LOCKED,
				    pwp->pw_passwd) == -1) {
					(void)close(ptmpfd);
					(void)pw_abort();
					err(EXIT_FAILURE,
					    "Can't modify user `%s': "
					    "asprintf failed", login_name);
				}
				pwp->pw_passwd = locked_pwd;
			}
		} else if (up->u_locked == UNLOCK) {
			if ((locked_pwd = strstr(pwp->pw_passwd, LOCKED))
			    == NULL) {
				warnx("Can't modify user `%s': "
				    "account is not locked", login_name);
			} else {
				pwp->pw_passwd = locked_pwd + strlen(LOCKED);
			}
		}

		if (up->u_flags & F_UID) {
			/* check uid isn't already allocated */
			if (!(up->u_flags & F_DUPUID) &&
			    getpwuid((uid_t)(up->u_uid)) != NULL) {
				(void)close(ptmpfd);
				(void)pw_abort();
				errx(EXIT_FAILURE, "Can't modify user `%s': "
				    "uid `%d' is already in use", login_name,
				    up->u_uid);
			}
			pwp->pw_uid = up->u_uid;
		}
		if (up->u_flags & F_GROUP) {
			/* if -g=uid was specified, check gid is unused */
			if (strcmp(up->u_primgrp, "=uid") == 0) {
				if (getgrgid((gid_t)(pwp->pw_uid)) != NULL) {
					(void)close(ptmpfd);
					(void)pw_abort();
					errx(EXIT_FAILURE,
					    "Can't modify user `%s': "
					    "gid %d is already in use",
					    login_name, up->u_uid);
				}
				pwp->pw_gid = pwp->pw_uid;
			} else if ((grp = getgrnam(up->u_primgrp)) != NULL) {
				pwp->pw_gid = grp->gr_gid;
			} else if (is_number(up->u_primgrp) &&
				   (grp = getgrgid(
				   (gid_t)atoi(up->u_primgrp))) != NULL) {
				pwp->pw_gid = grp->gr_gid;
			} else {
				(void)close(ptmpfd);
				(void)pw_abort();
				errx(EXIT_FAILURE, "Can't modify user `%s': "
				    "group %s not found", login_name,
				    up->u_primgrp);
			}
		}
		if (up->u_flags & F_INACTIVE) {
			if (!scantime(&pwp->pw_change, up->u_inactive)) {
				warnx("Warning: inactive time `%s' invalid, "
				    "password expiry off",
					up->u_inactive);
			}
		}
		if (up->u_flags & F_EXPIRE) {
			if (!scantime(&pwp->pw_expire, up->u_expire) ||
			      pwp->pw_expire == -1) {
				warnx("Warning: expire time `%s' invalid, "
				    "account expiry off",
					up->u_expire);
				pwp->pw_expire = 0;
			}
		}
		if (up->u_flags & F_COMMENT) {
			pwp->pw_gecos = up->u_comment;
		}
		if (up->u_flags & F_HOMEDIR) {
			pwp->pw_dir = up->u_home;
		}
		if (up->u_flags & F_SHELL) {
#ifdef EXTENSIONS
		if (!valid_shell(up->u_shell)) {
			int oerrno = errno;
			(void)close(ptmpfd);
			(void)pw_abort();
			errno = oerrno;
			errx(EXIT_FAILURE, "Can't modify user `%s': "
			    "Cannot access shell `%s'",
			    login_name, up->u_shell);
		}
		pwp->pw_shell = up->u_shell;
#else
		pwp->pw_shell = up->u_shell;
#endif
		}
#ifdef EXTENSIONS
		if (up->u_flags & F_CLASS) {
			if (!valid_class(up->u_class)) {
				(void)close(ptmpfd);
				(void)pw_abort();
				errx(EXIT_FAILURE, "Can't modify user `%s': "
				    "no such login class `%s'", login_name,
				    up->u_class);
			}
			pwp->pw_class = up->u_class;
		}
#endif
	}
	loginc = strlen(login_name);
	while (fgets(buf, sizeof(buf), master) != NULL) {
		if ((colon = strchr(buf, ':')) == NULL) {
			warnx("Malformed entry `%s'. Skipping", buf);
			continue;
		}
		colonc = (size_t)(colon - buf);
		if (strncmp(login_name, buf, loginc) == 0 && loginc == colonc) {
			if (up != NULL) {
				len = snprintf(buf, sizeof(buf), "%s:%s:%d:%d:"
#ifdef EXTENSIONS
				    "%s"
#endif
				    ":%ld:%ld:%s:%s:%s\n",
				    newlogin,
				    pwp->pw_passwd,
				    pwp->pw_uid,
				    pwp->pw_gid,
#ifdef EXTENSIONS
				    pwp->pw_class,
#endif
				    (long)pwp->pw_change,
				    (long)pwp->pw_expire,
				    pwp->pw_gecos,
				    pwp->pw_dir,
				    pwp->pw_shell);
				if (write(ptmpfd, buf, len) != len) {
					int serrno = errno;
					(void)close(ptmpfd);
					(void)pw_abort();
					errno = serrno;
					err(EXIT_FAILURE, "Can't modify user "
					    "`%s': write", login_name);
				}
			}
		} else {
			len = strlen(buf);
			if (write(ptmpfd, buf, len) != len) {
				int serrno = errno;
				(void)close(masterfd);
				(void)close(ptmpfd);
				(void)pw_abort();
				errno = serrno;
				err(EXIT_FAILURE, "Can't modify `%s': "
				    "write", login_name);
			}
		}
	}
	if (up != NULL) {
		if ((up->u_flags & F_MKDIR) &&
		    asystem("%s %s %s", MV, homedir, pwp->pw_dir) != 0) {
			(void)close(ptmpfd);
			(void)pw_abort();
			errx(EXIT_FAILURE, "Can't modify user `%s': "
			    "can't move `%s' to `%s'",
			    login_name, homedir, pwp->pw_dir);
		}
		if (up->u_groupc > 0 &&
		    !append_group(newlogin, up->u_groupc, up->u_groupv)) {
			(void)close(ptmpfd);
			(void)pw_abort();
			errx(EXIT_FAILURE, "Can't modify user `%s': "
			    "can't append `%s' to new groups",
			    login_name, newlogin);
		}
	}
	(void)close(ptmpfd);
	(void)fclose(master);
#if PW_MKDB_ARGC == 2
	if (up != NULL && strcmp(login_name, newlogin) == 0) {
		error = pw_mkdb(login_name, 0);
	} else {
		error = pw_mkdb(NULL, 0);
	}
#else
	error = pw_mkdb();
#endif
	if (error < 0) {
		(void)pw_abort();
		errx(EXIT_FAILURE, "Can't modify user `%s': pw_mkdb failed",
		    login_name);
	}
	if (up == NULL) {
		syslog(LOG_INFO, "User removed: name=%s", login_name);
	} else if (strcmp(login_name, newlogin) == 0) {
		syslog(LOG_INFO, "User information modified: name=%s, uid=%d, "
		    "gid=%d, home=%s, shell=%s",
		    login_name, pwp->pw_uid, pwp->pw_gid, pwp->pw_dir,
		    pwp->pw_shell);
	} else {
		syslog(LOG_INFO, "User information modified: name=%s, "
		    "new name=%s, uid=%d, gid=%d, home=%s, shell=%s",
		    login_name, newlogin, pwp->pw_uid, pwp->pw_gid,
		    pwp->pw_dir, pwp->pw_shell);
	}
	return 1;
}

#ifdef EXTENSIONS
/* see if we can find out the user struct */
static struct passwd *
find_user_info(const char *name)
{
	struct passwd	*pwp;

	if ((pwp = getpwnam(name)) != NULL) {
		return pwp;
	}
	if (is_number(name) && (pwp = getpwuid((uid_t)atoi(name))) != NULL) {
		return pwp;
	}
	return NULL;
}
#endif

/* see if we can find out the group struct */
static struct group *
find_group_info(const char *name)
{
	struct group	*grp;

	if ((grp = getgrnam(name)) != NULL) {
		return grp;
	}
	if (is_number(name) && (grp = getgrgid((gid_t)atoi(name))) != NULL) {
		return grp;
	}
	return NULL;
}

/* print out usage message, and then exit */
void
usermgmt_usage(const char *prog)
{
	if (strcmp(prog, "useradd") == 0) {
		(void)fprintf(stderr, "usage: %s -D [-F] [-b base-dir] "
		    "[-e expiry-time] [-f inactive-time]\n"
		    "\t[-g gid | name | =uid] [-k skel-dir] [-L login-class]\n"
		    "\t[-M homeperm] [-r lowuid..highuid] [-s shell]\n", prog);
		(void)fprintf(stderr, "usage: %s [-moSv] [-b base-dir] "
		    "[-c comment] [-d home-dir] [-e expiry-time]\n"
		    "\t[-f inactive-time] [-G secondary-group] "
		    "[-g gid | name | =uid]\n"
		    "\t[-k skeletondir] [-L login-class] [-M homeperm] "
		    "[-p password]\n"
		    "\t[-r lowuid..highuid] [-s shell] [-u uid] user\n",
		    prog);
	} else if (strcmp(prog, "usermod") == 0) {
		(void)fprintf(stderr, "usage: %s [-FmoSv] [-C yes/no] "
		    "[-c comment] [-d home-dir] [-e expiry-time]\n"
		    "\t[-f inactive] [-G secondary-group] "
		    "[-g gid | name | =uid]\n"
		    "\t[-L login-class] [-l new-login] [-p password] "
		    "[-s shell] [-u uid]\n"
		    "\tuser\n", prog);
	} else if (strcmp(prog, "userdel") == 0) {
		(void)fprintf(stderr, "usage: %s -D [-p preserve-value]\n", prog);
		(void)fprintf(stderr,
		    "usage: %s [-rSv] [-p preserve-value] user\n", prog);
#ifdef EXTENSIONS
	} else if (strcmp(prog, "userinfo") == 0) {
		(void)fprintf(stderr, "usage: %s [-e] user\n", prog);
#endif
	} else if (strcmp(prog, "groupadd") == 0) {
		(void)fprintf(stderr, "usage: %s [-ov] [-g gid]"
		    " [-r lowgid..highgid] group\n", prog);
	} else if (strcmp(prog, "groupdel") == 0) {
		(void)fprintf(stderr, "usage: %s [-v] group\n", prog);
	} else if (strcmp(prog, "groupmod") == 0) {
		(void)fprintf(stderr,
		    "usage: %s [-ov] [-g gid] [-n newname] group\n", prog);
	} else if (strcmp(prog, "user") == 0 || strcmp(prog, "group") == 0) {
		(void)fprintf(stderr,
		    "usage: %s ( add | del | mod | info ) ...\n", prog);
#ifdef EXTENSIONS
	} else if (strcmp(prog, "groupinfo") == 0) {
		(void)fprintf(stderr, "usage: %s [-ev] group\n", prog);
#endif
	}
	exit(EXIT_FAILURE);
	/* NOTREACHED */
}

#ifdef EXTENSIONS
#define ADD_OPT_EXTENSIONS	"M:p:r:vL:S"
#else
#define ADD_OPT_EXTENSIONS
#endif

int
useradd(int argc, char **argv)
{
	def_t	def;
	user_t	*up = &def.user;
	int	defaultfield;
	int	bigD;
	int	c;
#ifdef EXTENSIONS
	int	i;
#endif

	read_defaults(&def);
	up->u_uid = -1;
	defaultfield = bigD = 0;
	while ((c = getopt(argc, argv, "DFG:b:c:d:e:f:g:k:mou:s:"
	    ADD_OPT_EXTENSIONS)) != -1) {
		switch(c) {
		case 'D':
			bigD = 1;
			break;
		case 'F':
			/*
			 * Setting -1 will force the new user to
			 * change their password as soon as they
			 * next log in - passwd(5).
			 */
			defaultfield = 1;
			memsave(&up->u_inactive, "-1", strlen("-1"));
			break;
		case 'G':
			while (up->u_groupc < NGROUPS_MAX  &&
			       (up->u_groupv[up->u_groupc] = strsep(&optarg, ",")) != NULL) {
				if (up->u_groupv[up->u_groupc][0] != 0) {
					up->u_groupc++;
				}
			}
			if (optarg != NULL) {
				warnx("Truncated list of secondary groups "
				    "to %d entries", NGROUPS_MAX);
			}
			break;
#ifdef EXTENSIONS
		case 'S':
			up->u_allow_samba = 1;
			break;
#endif
		case 'b':
			defaultfield = 1;
			memsave(&up->u_basedir, optarg, strlen(optarg));
			break;
		case 'c':
			memsave(&up->u_comment, optarg, strlen(optarg));
			break;
		case 'd':
			memsave(&up->u_home, optarg, strlen(optarg));
			up->u_flags |= F_HOMEDIR;
			break;
		case 'e':
			defaultfield = 1;
			memsave(&up->u_expire, optarg, strlen(optarg));
			break;
		case 'f':
			defaultfield = 1;
			memsave(&up->u_inactive, optarg, strlen(optarg));
			break;
		case 'g':
			defaultfield = 1;
			memsave(&up->u_primgrp, optarg, strlen(optarg));
			break;
		case 'k':
			defaultfield = 1;
			memsave(&up->u_skeldir, optarg, strlen(optarg));
			break;
#ifdef EXTENSIONS
		case 'L':
			defaultfield = 1;
			memsave(&up->u_class, optarg, strlen(optarg));
			break;
#endif
		case 'm':
			up->u_flags |= F_MKDIR;
			break;
#ifdef EXTENSIONS
		case 'M':
			defaultfield = 1;
			up->u_homeperm = strtoul(optarg, NULL, 8);
			break;
#endif
		case 'o':
			up->u_flags |= F_DUPUID;
			break;
#ifdef EXTENSIONS
		case 'p':
			memsave(&up->u_password, optarg, strlen(optarg));
			break;
#endif
#ifdef EXTENSIONS
		case 'r':
			defaultfield = 1;
			(void)save_range(&up->u_r, optarg);
			break;
#endif
		case 's':
			up->u_flags |= F_SHELL;
			defaultfield = 1;
			memsave(&up->u_shell, optarg, strlen(optarg));
			break;
		case 'u':
			up->u_uid = check_numeric(optarg, "uid");
			break;
#ifdef EXTENSIONS
		case 'v':
			verbose = 1;
			break;
#endif
		default:
			usermgmt_usage("useradd");
			/* NOTREACHED */
		}
	}
	if (bigD) {
		if (defaultfield) {
			checkeuid();
			return setdefaults(up) ? EXIT_SUCCESS : EXIT_FAILURE;
		}
		(void)printf("group\t\t%s\n", up->u_primgrp);
		(void)printf("base_dir\t%s\n", up->u_basedir);
		(void)printf("skel_dir\t%s\n", up->u_skeldir);
		(void)printf("shell\t\t%s\n", up->u_shell);
#ifdef EXTENSIONS
		(void)printf("class\t\t%s\n", up->u_class);
		(void)printf("homeperm\t0%o\n", up->u_homeperm);
#endif
		(void)printf("inactive\t%s\n", (up->u_inactive == NULL) ?
		    UNSET_INACTIVE : up->u_inactive);
		(void)printf("expire\t\t%s\n", (up->u_expire == NULL) ?
		    UNSET_EXPIRY : up->u_expire);
#ifdef EXTENSIONS
		for (i = 0 ; i < up->u_rc ; i++) {
			(void)printf("range\t\t%d..%d\n",
			    up->u_rv[i].r_from, up->u_rv[i].r_to);
		}
#endif
		return EXIT_SUCCESS;
	}
	argc -= optind;
	argv += optind;
	if (argc != 1) {
		usermgmt_usage("useradd");
	}
	checkeuid();
	openlog("useradd", LOG_PID, LOG_USER);
	return adduser(*argv, up) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#ifdef EXTENSIONS
#define MOD_OPT_EXTENSIONS	"p:vL:S"
#else
#define MOD_OPT_EXTENSIONS
#endif

int
usermod(int argc, char **argv)
{
	def_t	def;
	user_t	*up = &def.user;
	char	newuser[MaxUserNameLen + 1];
	int	c, have_new_user;

	(void)memset(newuser, 0, sizeof(newuser));
	read_defaults(&def);
	have_new_user = 0;
	up->u_locked = -1;
	while ((c = getopt(argc, argv, "C:FG:c:d:e:f:g:l:mos:u:"
	    MOD_OPT_EXTENSIONS)) != -1) {
		switch(c) {
		case 'G':
			while (up->u_groupc < NGROUPS_MAX &&
			    (up->u_groupv[up->u_groupc] =
			    strsep(&optarg, ",")) != NULL) {
				if (up->u_groupv[up->u_groupc][0] != 0) {
					up->u_groupc++;
				}
			}
			if (optarg != NULL) {
				warnx("Truncated list of secondary groups "
				    "to %d entries", NGROUPS_MAX);
			}
			up->u_flags |= F_SECGROUP;
			break;
#ifdef EXTENSIONS
		case 'S':
			up->u_allow_samba = 1;
			break;
#endif
		case 'c':
			memsave(&up->u_comment, optarg, strlen(optarg));
			up->u_flags |= F_COMMENT;
			break;
		case 'C':
			if (strcasecmp(optarg, "yes") == 0) {
				up->u_locked = LOCK;
			} else if (strcasecmp(optarg, "no") == 0) {
				up->u_locked = UNLOCK;
			} else {
				/* No idea. */
				errx(EXIT_FAILURE,
					"Please type 'yes' or 'no'");
			}
			break;
		case 'F':
			memsave(&up->u_inactive, "-1", strlen("-1"));
			up->u_flags |= F_INACTIVE;
			break;
		case 'd':
			memsave(&up->u_home, optarg, strlen(optarg));
			up->u_flags |= F_HOMEDIR;
			break;
		case 'e':
			memsave(&up->u_expire, optarg, strlen(optarg));
			up->u_flags |= F_EXPIRE;
			break;
		case 'f':
			memsave(&up->u_inactive, optarg, strlen(optarg));
			up->u_flags |= F_INACTIVE;
			break;
		case 'g':
			memsave(&up->u_primgrp, optarg, strlen(optarg));
			up->u_flags |= F_GROUP;
			break;
		case 'l':
			(void)strlcpy(newuser, optarg, sizeof(newuser));
			have_new_user = 1;
			up->u_flags |= F_USERNAME;
			break;
#ifdef EXTENSIONS
		case 'L':
			memsave(&up->u_class, optarg, strlen(optarg));
			up->u_flags |= F_CLASS;
			break;
#endif
		case 'm':
			up->u_flags |= F_MKDIR;
			break;
		case 'o':
			up->u_flags |= F_DUPUID;
			break;
#ifdef EXTENSIONS
		case 'p':
			memsave(&up->u_password, optarg, strlen(optarg));
			up->u_flags |= F_PASSWORD;
			break;
#endif
		case 's':
			memsave(&up->u_shell, optarg, strlen(optarg));
			up->u_flags |= F_SHELL;
			break;
		case 'u':
			up->u_uid = check_numeric(optarg, "uid");
			up->u_flags |= F_UID;
			break;
#ifdef EXTENSIONS
		case 'v':
			verbose = 1;
			break;
#endif
		default:
			usermgmt_usage("usermod");
			/* NOTREACHED */
		}
	}
	if ((up->u_flags & F_MKDIR) && !(up->u_flags & F_HOMEDIR) &&
	    !(up->u_flags & F_USERNAME)) {
		warnx("Option 'm' useless without 'd' or 'l' -- ignored");
		up->u_flags &= ~F_MKDIR;
	}
	argc -= optind;
	argv += optind;
	if (argc != 1) {
		usermgmt_usage("usermod");
	}
	checkeuid();
	openlog("usermod", LOG_PID, LOG_USER);
	return moduser(*argv, (have_new_user) ? newuser : *argv, up,
	    up->u_allow_samba) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#ifdef EXTENSIONS
#define DEL_OPT_EXTENSIONS	"Dp:vS"
#else
#define DEL_OPT_EXTENSIONS
#endif

int
userdel(int argc, char **argv)
{
	struct passwd	*pwp;
	def_t		def;
	user_t		*up = &def.user;
	char		password[PasswordLength + 1];
	int		defaultfield;
	int		rmhome;
	int		bigD;
	int		c;

	read_defaults(&def);
	defaultfield = bigD = rmhome = 0;
	while ((c = getopt(argc, argv, "r" DEL_OPT_EXTENSIONS)) != -1) {
		switch(c) {
#ifdef EXTENSIONS
		case 'D':
			bigD = 1;
			break;
#endif
#ifdef EXTENSIONS
		case 'S':
			up->u_allow_samba = 1;
			break;
#endif
#ifdef EXTENSIONS
		case 'p':
			defaultfield = 1;
			up->u_preserve = (strcmp(optarg, "true") == 0) ? 1 :
					(strcmp(optarg, "yes") == 0) ? 1 :
					 atoi(optarg);
			break;
#endif
		case 'r':
			rmhome = 1;
			break;
#ifdef EXTENSIONS
		case 'v':
			verbose = 1;
			break;
#endif
		default:
			usermgmt_usage("userdel");
			/* NOTREACHED */
		}
	}
#ifdef EXTENSIONS
	if (bigD) {
		if (defaultfield) {
			checkeuid();
			return setdefaults(up) ? EXIT_SUCCESS : EXIT_FAILURE;
		}
		(void)printf("preserve\t%s\n", (up->u_preserve) ? "true" :
		    "false");
		return EXIT_SUCCESS;
	}
#endif
	argc -= optind;
	argv += optind;
	if (argc != 1) {
		usermgmt_usage("userdel");
	}
	checkeuid();
	if ((pwp = getpwnam(*argv)) == NULL) {
		warnx("No such user `%s'", *argv);
		return EXIT_FAILURE;
	}
	if (rmhome) {
		(void)removehomedir(pwp);
	}
	if (up->u_preserve) {
		up->u_flags |= F_SHELL;
		memsave(&up->u_shell, NOLOGIN, strlen(NOLOGIN));
		(void)memset(password, '*', DES_Len);
		password[DES_Len] = 0;
		memsave(&up->u_password, password, strlen(password));
		up->u_flags |= F_PASSWORD;
		openlog("userdel", LOG_PID, LOG_USER);
		return moduser(*argv, *argv, up, up->u_allow_samba) ?
		    EXIT_SUCCESS : EXIT_FAILURE;
	}
	if (!rm_user_from_groups(*argv)) {
		return 0;
	}
	openlog("userdel", LOG_PID, LOG_USER);
	return moduser(*argv, *argv, NULL, up->u_allow_samba) ?
	    EXIT_SUCCESS : EXIT_FAILURE;
}

#ifdef EXTENSIONS
#define GROUP_ADD_OPT_EXTENSIONS	"r:v"
#else
#define GROUP_ADD_OPT_EXTENSIONS
#endif

/* add a group */
int
groupadd(int argc, char **argv)
{
	def_t	def;
	group_t	*gp = &def.group;
	int	dupgid;
	int	gid;
	int	c;

	gid = -1;
	dupgid = 0;
	read_defaults(&def);
	while ((c = getopt(argc, argv, "g:o" GROUP_ADD_OPT_EXTENSIONS)) != -1) {
		switch(c) {
		case 'g':
			gid = check_numeric(optarg, "gid");
			break;
		case 'o':
			dupgid = 1;
			break;
#ifdef EXTENSIONS
		case 'r':
			(void)save_range(&gp->g_r, optarg);
			break;
		case 'v':
			verbose = 1;
			break;
#endif
		default:
			usermgmt_usage("groupadd");
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1) {
		usermgmt_usage("groupadd");
	}
	if (gp->g_rc == 0) {
		gp->g_rv[gp->g_rc].r_from = DEF_LOWUID;
		gp->g_rv[gp->g_rc].r_to = DEF_HIGHUID;
		gp->g_rc += 1;
	}
	gp->g_defrc = gp->g_rc;
	checkeuid();
	if (gid == -1) {
		int	got_id = 0;
		int	i;

		/*
		 * Look for a free GID in the command line ranges (if any).
		 * These start after the ranges specified in the config file.
		 */
		for (i = gp->g_defrc; !got_id && i < gp->g_rc ; i++) {
			got_id = getnextgid(&gid,
					gp->g_rv[i].r_from, gp->g_rv[i].r_to);
		}
		/*
		 * If there were no free GIDs in the command line ranges,
		 * try the ranges from the config file (there will always
		 * be at least one default).
		 */
		for (i = 0; !got_id && i < gp->g_defrc; i++) {
			got_id = getnextgid(&gid,
					gp->g_rv[i].r_from, gp->g_rv[i].r_to);
		}
		if (!got_id)
			errx(EXIT_FAILURE, "Can't add group: can't get next gid");
	}
	if (!dupgid && getgrgid((gid_t) gid) != NULL) {
		errx(EXIT_FAILURE, "Can't add group: gid %d is a duplicate",
		    gid);
	}
	if (!valid_group(*argv)) {
		warnx("Invalid group name `%s'", *argv);
	}
	openlog("groupadd", LOG_PID, LOG_USER);
	if (!creategid(*argv, gid, ""))
		exit(EXIT_FAILURE);

	return EXIT_SUCCESS;
}

#ifdef EXTENSIONS
#define GROUP_DEL_OPT_EXTENSIONS	"v"
#else
#define GROUP_DEL_OPT_EXTENSIONS
#endif

/* remove a group */
int
groupdel(int argc, char **argv)
{
	int	c;

	while ((c = getopt(argc, argv, "" GROUP_DEL_OPT_EXTENSIONS)) != -1) {
		switch(c) {
#ifdef EXTENSIONS
		case 'v':
			verbose = 1;
			break;
#endif
		default:
			usermgmt_usage("groupdel");
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1) {
		usermgmt_usage("groupdel");
	}
	if (getgrnam(*argv) == NULL) {
		errx(EXIT_FAILURE, "No such group `%s'", *argv);
	}
	checkeuid();
	openlog("groupdel", LOG_PID, LOG_USER);
	if (!modify_gid(*argv, NULL))
		exit(EXIT_FAILURE);

	return EXIT_SUCCESS;
}

#ifdef EXTENSIONS
#define GROUP_MOD_OPT_EXTENSIONS	"v"
#else
#define GROUP_MOD_OPT_EXTENSIONS
#endif

/* modify a group */
int
groupmod(int argc, char **argv)
{
	struct group	*grp;
	char		buf[MaxEntryLen];
	char		*newname;
	char		**cpp;
	int		dupgid;
	int		gid;
	int		cc;
	int		c;

	gid = -1;
	dupgid = 0;
	newname = NULL;
	while ((c = getopt(argc, argv, "g:on:" GROUP_MOD_OPT_EXTENSIONS)) != -1) {
		switch(c) {
		case 'g':
			gid = check_numeric(optarg, "gid");
			break;
		case 'o':
			dupgid = 1;
			break;
		case 'n':
			memsave(&newname, optarg, strlen(optarg));
			break;
#ifdef EXTENSIONS
		case 'v':
			verbose = 1;
			break;
#endif
		default:
			usermgmt_usage("groupmod");
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1) {
		usermgmt_usage("groupmod");
	}
	checkeuid();
	if (gid < 0 && newname == NULL) {
		errx(EXIT_FAILURE, "Nothing to change");
	}
	if (dupgid && gid < 0) {
		errx(EXIT_FAILURE, "Duplicate which gid?");
	}
	if (!dupgid && getgrgid((gid_t) gid) != NULL) {
		errx(EXIT_FAILURE, "Can't modify group: gid %d is a duplicate",
		    gid);
	}
	if ((grp = find_group_info(*argv)) == NULL) {
		errx(EXIT_FAILURE, "Can't find group `%s' to modify", *argv);
	}
	if (!is_local(*argv, _PATH_GROUP)) {
		errx(EXIT_FAILURE, "Group `%s' must be a local group", *argv);
	}
	if (newname != NULL && !valid_group(newname)) {
		warnx("Invalid group name `%s'", newname);
	}
	cc = snprintf(buf, sizeof(buf), "%s:%s:%d:",
			(newname) ? newname : grp->gr_name,
			grp->gr_passwd,
			(gid < 0) ? grp->gr_gid : gid);
	for (cpp = grp->gr_mem ; *cpp && cc < sizeof(buf) ; cpp++) {
		cc += snprintf(&buf[cc], sizeof(buf) - cc, "%s%s", *cpp,
			(cpp[1] == NULL) ? "" : ",");
	}
	cc += snprintf(&buf[cc], sizeof(buf) - cc, "\n");
	if (newname != NULL)
		free(newname);
	openlog("groupmod", LOG_PID, LOG_USER);
	if (!modify_gid(*argv, buf))
		exit(EXIT_FAILURE);

	return EXIT_SUCCESS;
}

#ifdef EXTENSIONS
/* display user information */
int
userinfo(int argc, char **argv)
{
	struct passwd	*pwp;
	struct group	*grp;
	char		buf[MaxEntryLen];
	char		**cpp;
	int		exists;
	int		cc;
	int		i;

	exists = 0;
	buf[0] = '\0';
	while ((i = getopt(argc, argv, "e")) != -1) {
		switch(i) {
		case 'e':
			exists = 1;
			break;
		default:
			usermgmt_usage("userinfo");
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1) {
		usermgmt_usage("userinfo");
	}
	pwp = find_user_info(*argv);
	if (exists) {
		exit((pwp) ? EXIT_SUCCESS : EXIT_FAILURE);
	}
	if (pwp == NULL) {
		errx(EXIT_FAILURE, "Can't find user `%s'", *argv);
	}
	(void)printf("login\t%s\n", pwp->pw_name);
	(void)printf("passwd\t%s\n", pwp->pw_passwd);
	(void)printf("uid\t%d\n", pwp->pw_uid);
	for (cc = 0 ; (grp = getgrent()) != NULL ; ) {
		for (cpp = grp->gr_mem ; *cpp ; cpp++) {
			if (strcmp(*cpp, *argv) == 0 &&
			    grp->gr_gid != pwp->pw_gid) {
				cc += snprintf(&buf[cc], sizeof(buf) - cc,
				    "%s ", grp->gr_name);
			}
		}
	}
	if ((grp = getgrgid(pwp->pw_gid)) == NULL) {
		(void)printf("groups\t%d %s\n", pwp->pw_gid, buf);
	} else {
		(void)printf("groups\t%s %s\n", grp->gr_name, buf);
	}
	(void)printf("change\t%s", pwp->pw_change > 0 ?
	    ctime(&pwp->pw_change) : pwp->pw_change == -1 ?
	    "NEXT LOGIN\n" : "NEVER\n");
	(void)printf("class\t%s\n", pwp->pw_class);
	(void)printf("gecos\t%s\n", pwp->pw_gecos);
	(void)printf("dir\t%s\n", pwp->pw_dir);
	(void)printf("shell\t%s\n", pwp->pw_shell);
	(void)printf("expire\t%s", pwp->pw_expire ?
	    ctime(&pwp->pw_expire) : "NEVER\n");
	return EXIT_SUCCESS;
}
#endif

#ifdef EXTENSIONS
/* display user information */
int
groupinfo(int argc, char **argv)
{
	struct group	*grp;
	char		**cpp;
	int		exists;
	int		i;

	exists = 0;
	while ((i = getopt(argc, argv, "ev")) != -1) {
		switch(i) {
		case 'e':
			exists = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usermgmt_usage("groupinfo");
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1) {
		usermgmt_usage("groupinfo");
	}
	grp = find_group_info(*argv);
	if (exists) {
		exit((grp) ? EXIT_SUCCESS : EXIT_FAILURE);
	}
	if (grp == NULL) {
		errx(EXIT_FAILURE, "Can't find group `%s'", *argv);
	}
	(void)printf("name\t%s\n", grp->gr_name);
	(void)printf("passwd\t%s\n", grp->gr_passwd);
	(void)printf("gid\t%d\n", grp->gr_gid);
	(void)printf("members\t");
	for (cpp = grp->gr_mem ; *cpp ; cpp++) {
		(void)printf("%s", *cpp);
		if (*(cpp + 1)) {
			(void) printf(", ");
		}
	}
	(void)fputc('\n', stdout);
	return EXIT_SUCCESS;
}
#endif

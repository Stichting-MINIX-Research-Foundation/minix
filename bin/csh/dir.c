/* $NetBSD: dir.c,v 1.30 2013/07/16 17:47:43 christos Exp $ */

/*-
 * Copyright (c) 1980, 1991, 1993
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
#if 0
static char sccsid[] = "@(#)dir.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: dir.c,v 1.30 2013/07/16 17:47:43 christos Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "csh.h"
#include "dir.h"
#include "extern.h"

/* Directory management. */

static struct directory *dfind(Char *);
static Char *dfollow(Char *);
static void printdirs(void);
static Char *dgoto(Char *);
static void skipargs(Char ***, const char *);
static void dnewcwd(struct directory *);
static void dset(Char *);

struct directory dhead;		/* "head" of loop */
int printd;			/* force name to be printed */

static int dirflag = 0;

/*
 * dinit - initialize current working directory
 */
void
dinit(Char *hp)
{
    static const char emsg[] = "csh: Trying to start from \"%s\"\n";
    char path[MAXPATHLEN];
    struct directory *dp;
    const char *ecp;
    Char *cp;

    /* Don't believe the login shell home, because it may be a symlink */
    ecp = getcwd(path, MAXPATHLEN);
    if (ecp == NULL || *ecp == '\0') {
	(void)fprintf(csherr, "csh: %s\n", strerror(errno));
	if (hp && *hp) {
	    ecp = short2str(hp);
	    if (chdir(ecp) == -1)
		cp = NULL;
	    else
		cp = Strsave(hp);
	    (void)fprintf(csherr, emsg, vis_str(hp));
	}
	else
	    cp = NULL;
	if (cp == NULL) {
	    (void)fprintf(csherr, emsg, "/");
	    if (chdir("/") == -1) {
		/* I am not even try to print an error message! */
		xexit(1);
	    }
	    cp = SAVE("/");
	}
    }
    else {
	struct stat swd, shp;

	/*
	 * See if $HOME is the working directory we got and use that
	 */
	if (hp && *hp &&
	    stat(ecp, &swd) != -1 && stat(short2str(hp), &shp) != -1 &&
	    swd.st_dev == shp.st_dev && swd.st_ino == shp.st_ino)
	    cp = Strsave(hp);
	else {
	    const char *cwd;

	    /*
	     * use PWD if we have it (for subshells)
	     */
	    if ((cwd = getenv("PWD")) != NULL) {
		if (stat(cwd, &shp) != -1 && swd.st_dev == shp.st_dev &&
		    swd.st_ino == shp.st_ino)
		    ecp = cwd;
	    }
	    cp = dcanon(SAVE(ecp), STRNULL);
	}
    }

    dp = (struct directory *)xcalloc(1, sizeof(struct directory));
    dp->di_name = cp;
    dp->di_count = 0;
    dhead.di_next = dhead.di_prev = dp;
    dp->di_next = dp->di_prev = &dhead;
    printd = 0;
    dnewcwd(dp);
}

static void
dset(Char *dp)
{
    Char **vec;

    /*
     * Don't call set() directly cause if the directory contains ` or
     * other junk characters glob will fail.
     */

    vec = xmalloc((size_t)(2 * sizeof(Char **)));
    vec[0] = Strsave(dp);
    vec[1] = 0;
    setq(STRcwd, vec, &shvhed);
    Setenv(STRPWD, dp);
}

#define DIR_LONG 1
#define DIR_VERT 2
#define DIR_LINE 4

static void
skipargs(Char ***v, const char *str)
{
    Char  **n, *s;

    n = *v;
    dirflag = 0;
    for (n++; *n != NULL && (*n)[0] == '-'; n++)
	for (s = &((*n)[1]); *s; s++)
	    switch (*s) {
	    case 'l':
		dirflag |= DIR_LONG;
		break;
	    case 'n':
		dirflag |= DIR_LINE;
		break;
	    case 'v':
		dirflag |= DIR_VERT;
		break;
	    default:
		stderror(ERR_DIRUS, vis_str(**v), str);
		/* NOTREACHED */
	    }
    *v = n;
}

/*
 * dodirs - list all directories in directory loop
 */
void
/*ARGSUSED*/
dodirs(Char **v, struct command *t)
{
    skipargs(&v, "");

    if (*v != NULL)
	stderror(ERR_DIRUS, "dirs", "");
    printdirs();
}

static void
printdirs(void)
{
    struct directory *dp;
    Char *hp, *s;
    size_t cur, idx, len;

    hp = value(STRhome);
    if (*hp == '\0')
	hp = NULL;
    dp = dcwd;
    idx = 0;
    cur = 0;
    do {
	if (dp == &dhead)
	    continue;
	if (dirflag & DIR_VERT) {
	    (void)fprintf(cshout, "%zu\t", idx++);
	    cur = 0;
	}
	if (!(dirflag & DIR_LONG) && hp != NULL && !eq(hp, STRslash) &&
	    (len = Strlen(hp), Strncmp(hp, dp->di_name, len) == 0) &&
	    (dp->di_name[len] == '\0' || dp->di_name[len] == '/')) 
	    len = Strlen(s = (dp->di_name + len)) + 2;
	else
	    len = Strlen(s = dp->di_name) + 1;

	cur += len;
	if ((dirflag & DIR_LINE) && cur >= 80 - 1 && len < 80) {
	    (void)fprintf(cshout, "\n");
	    cur = len;
	}
	(void) fprintf(cshout, "%s%s%c", (s != dp->di_name)? "~" : "",
	    vis_str(s), (dirflag & DIR_VERT) ? '\n' : ' ');
    } while ((dp = dp->di_prev) != dcwd);
    if (!(dirflag & DIR_VERT))
	(void)fprintf(cshout, "\n");
}

void
dtildepr(Char *home, Char *dir)
{
    if (!eq(home, STRslash) && prefix(home, dir))
	(void)fprintf(cshout, "~%s", vis_str(dir + Strlen(home)));
    else
	(void)fprintf(cshout, "%s", vis_str(dir));
}

void
dtilde(void)
{
    struct directory *d;

    d = dcwd;
    do {
	if (d == &dhead)
	    continue;
	d->di_name = dcanon(d->di_name, STRNULL);
    } while ((d = d->di_prev) != dcwd);

    dset(dcwd->di_name);
}


/* dnormalize():
 *	If the name starts with . or .. then we might need to normalize
 *	it depending on the symbolic link flags
 */
Char *
dnormalize(Char *cp)
{
#define UC (unsigned char)
#define ISDOT(c) (UC(c)[0] == '.' && ((UC(c)[1] == '\0') || (UC(c)[1] == '/')))
#define ISDOTDOT(c) (UC(c)[0] == '.' && ISDOT(&((c)[1])))
    if ((unsigned char) cp[0] == '/')
	return (Strsave(cp));

    if (adrof(STRignore_symlinks)) {
	size_t  dotdot = 0;
	Char   *dp, *cwd;

	cwd = xmalloc((size_t)((Strlen(dcwd->di_name) + 3) *
	    sizeof(Char)));
	(void)Strcpy(cwd, dcwd->di_name);

	/*
	 * Ignore . and count ..'s
	 */
	while (*cp) {
	    if (ISDOT(cp)) {
		if (*++cp)
		    cp++;
	    }
	    else if (ISDOTDOT(cp)) {
		dotdot++;
		cp += 2;
		if (*cp)
		    cp++;
	    }
	    else
		break;
	}
	while (dotdot > 0) {
	    dp = Strrchr(cwd, '/');
	    if (dp) {
		*dp = '\0';
		dotdot--;
	    }
	    else
		break;
	}

	if (*cp) {
	    cwd[dotdot = Strlen(cwd)] = '/';
	    cwd[dotdot + 1] = '\0';
	    dp = Strspl(cwd, cp);
	    xfree((ptr_t) cwd);
	    return dp;
	}
	else {
	    if (!*cwd) {
		cwd[0] = '/';
		cwd[1] = '\0';
	    }
	    return cwd;
	}
    }
    return Strsave(cp);
}

/*
 * dochngd - implement chdir command.
 */
void
/*ARGSUSED*/
dochngd(Char **v, struct command *t)
{
    struct directory *dp;
    Char *cp;

    skipargs(&v, " [<dir>]");
    printd = 0;
    if (*v == NULL) {
	if ((cp = value(STRhome)) == NULL || *cp == 0)
	    stderror(ERR_NAME | ERR_NOHOMEDIR);
	if (chdir(short2str(cp)) < 0)
	    stderror(ERR_NAME | ERR_CANTCHANGE);
	cp = Strsave(cp);
    }
    else if (v[1] != NULL)
	stderror(ERR_NAME | ERR_TOOMANY);
    else if ((dp = dfind(*v)) != 0) {
	char   *tmp;

	printd = 1;
	if (chdir(tmp = short2str(dp->di_name)) < 0)
	    stderror(ERR_SYSTEM, tmp, strerror(errno));
	dcwd->di_prev->di_next = dcwd->di_next;
	dcwd->di_next->di_prev = dcwd->di_prev;
	dfree(dcwd);
	dnewcwd(dp);
	return;
    }
    else
	cp = dfollow(*v);
    dp = (struct directory *)xcalloc(1, sizeof(struct directory));
    dp->di_name = cp;
    dp->di_count = 0;
    dp->di_next = dcwd->di_next;
    dp->di_prev = dcwd->di_prev;
    dp->di_prev->di_next = dp;
    dp->di_next->di_prev = dp;
    dfree(dcwd);
    dnewcwd(dp);
}

static Char *
dgoto(Char *cp)
{
    Char *dp;

    if (*cp != '/') {
	Char *p, *q;
	size_t cwdlen;

	for (p = dcwd->di_name; *p++;)
	    continue;
	if ((cwdlen = (size_t)(p - dcwd->di_name - 1)) == 1)	/* root */
	    cwdlen = 0;
	for (p = cp; *p++;)
	    continue;
	dp = xmalloc((size_t)(cwdlen + (size_t)(p - cp) + 1) * sizeof(Char));
	for (p = dp, q = dcwd->di_name; (*p++ = *q++) != '\0';)
	    continue;
	if (cwdlen)
	    p[-1] = '/';
	else
	    p--;		/* don't add a / after root */
	for (q = cp; (*p++ = *q++) != '\0';)
	    continue;
	xfree((ptr_t) cp);
	cp = dp;
	dp += cwdlen;
    }
    else
	dp = cp;

    cp = dcanon(cp, dp);
    return cp;
}

/*
 * dfollow - change to arg directory; fall back on cdpath if not valid
 */
static Char *
dfollow(Char *cp)
{
    char ebuf[MAXPATHLEN];
    struct varent *c;
    Char *dp;
    int serrno;

    cp = globone(cp, G_ERROR);
    /*
     * if we are ignoring symlinks, try to fix relatives now.
     */
    dp = dnormalize(cp);
    if (chdir(short2str(dp)) >= 0) {
	xfree((ptr_t) cp);
	return dgoto(dp);
    }
    else {
	xfree((ptr_t) dp);
	if (chdir(short2str(cp)) >= 0)
	    return dgoto(cp);
	serrno = errno;
    }

    if (cp[0] != '/' && !prefix(STRdotsl, cp) && !prefix(STRdotdotsl, cp)
	&& (c = adrof(STRcdpath))) {
	Char  **cdp;
	Char *p;
	Char    buf[MAXPATHLEN];

	for (cdp = c->vec; *cdp; cdp++) {
	    for (dp = buf, p = *cdp; (*dp++ = *p++) != '\0';)
		continue;
	    dp[-1] = '/';
	    for (p = cp; (*dp++ = *p++) != '\0';)
		continue;
	    if (chdir(short2str(buf)) >= 0) {
		printd = 1;
		xfree((ptr_t) cp);
		cp = Strsave(buf);
		return dgoto(cp);
	    }
	}
    }
    dp = value(cp);
    if ((dp[0] == '/' || dp[0] == '.') && chdir(short2str(dp)) >= 0) {
	xfree((ptr_t) cp);
	cp = Strsave(dp);
	printd = 1;
	return dgoto(cp);
    }
    (void)strcpy(ebuf, short2str(cp));
    xfree((ptr_t) cp);
    stderror(ERR_SYSTEM, ebuf, strerror(serrno));
    /* NOTREACHED */
}

/*
 * dopushd - push new directory onto directory stack.
 *	with no arguments exchange top and second.
 *	with numeric argument (+n) bring it to top.
 */
void
/*ARGSUSED*/
dopushd(Char **v, struct command *t)
{
    struct directory *dp;

    skipargs(&v, " [<dir>|+<n>]");
    printd = 1;
    if (*v == NULL) {
	char   *tmp;

	if ((dp = dcwd->di_prev) == &dhead)
	    dp = dhead.di_prev;
	if (dp == dcwd)
	    stderror(ERR_NAME | ERR_NODIR);
	if (chdir(tmp = short2str(dp->di_name)) < 0)
	    stderror(ERR_SYSTEM, tmp, strerror(errno));
	dp->di_prev->di_next = dp->di_next;
	dp->di_next->di_prev = dp->di_prev;
	dp->di_next = dcwd->di_next;
	dp->di_prev = dcwd;
	dcwd->di_next->di_prev = dp;
	dcwd->di_next = dp;
    }
    else if (v[1] != NULL)
	stderror(ERR_NAME | ERR_TOOMANY);
    else if ((dp = dfind(*v)) != NULL) {
	char   *tmp;

	if (chdir(tmp = short2str(dp->di_name)) < 0)
	    stderror(ERR_SYSTEM, tmp, strerror(errno));
    }
    else {
	Char *ccp;

	ccp = dfollow(*v);
	dp = (struct directory *)xcalloc(1, sizeof(struct directory));
	dp->di_name = ccp;
	dp->di_count = 0;
	dp->di_prev = dcwd;
	dp->di_next = dcwd->di_next;
	dcwd->di_next = dp;
	dp->di_next->di_prev = dp;
    }
    dnewcwd(dp);
}

/*
 * dfind - find a directory if specified by numeric (+n) argument
 */
static struct directory *
dfind(Char *cp)
{
    struct directory *dp;
    Char *ep;
    int i;

    if (*cp++ != '+')
	return (0);
    for (ep = cp; Isdigit(*ep); ep++)
	continue;
    if (*ep)
	return (0);
    i = getn(cp);
    if (i <= 0)
	return (0);
    for (dp = dcwd; i != 0; i--) {
	if ((dp = dp->di_prev) == &dhead)
	    dp = dp->di_prev;
	if (dp == dcwd)
	    stderror(ERR_NAME | ERR_DEEP);
    }
    return (dp);
}

/*
 * dopopd - pop a directory out of the directory stack
 *	with a numeric argument just discard it.
 */
void
/*ARGSUSED*/
dopopd(Char **v, struct command *t)
{
    struct directory *dp, *p = NULL;

    skipargs(&v, " [+<n>]");
    printd = 1;
    if (*v == NULL)
	dp = dcwd;
    else if (v[1] != NULL)
	stderror(ERR_NAME | ERR_TOOMANY);
    else if ((dp = dfind(*v)) == 0)
	stderror(ERR_NAME | ERR_BADDIR);
    if (dp->di_prev == &dhead && dp->di_next == &dhead)
	stderror(ERR_NAME | ERR_EMPTY);
    if (dp == dcwd) {
	char *tmp;

	if ((p = dp->di_prev) == &dhead)
	    p = dhead.di_prev;
	if (chdir(tmp = short2str(p->di_name)) < 0)
	    stderror(ERR_SYSTEM, tmp, strerror(errno));
    }
    dp->di_prev->di_next = dp->di_next;
    dp->di_next->di_prev = dp->di_prev;
    if (dp == dcwd)
	dnewcwd(p);
    else {
	printdirs();
    }
    dfree(dp);
}

/*
 * dfree - free the directory (or keep it if it still has ref count)
 */
void
dfree(struct directory *dp)
{

    if (dp->di_count != 0) {
	dp->di_next = dp->di_prev = 0;
    }
    else {
	xfree((char *) dp->di_name);
	xfree((ptr_t) dp);
    }
}

/*
 * dcanon - canonicalize the pathname, removing excess ./ and ../ etc.
 *	we are of course assuming that the file system is standardly
 *	constructed (always have ..'s, directories have links)
 */
Char *
dcanon(Char *cp, Char *p)
{
    Char slink[MAXPATHLEN];
    char tlink[MAXPATHLEN];
    Char *newcp, *sp;
    Char *p1, *p2;	/* general purpose */
    ssize_t cc;
    size_t len;
    int slash;

    /*
     * christos: if the path given does not start with a slash prepend cwd. If
     * cwd does not start with a path or the result would be too long abort().
     */
    if (*cp != '/') {
	Char tmpdir[MAXPATHLEN];

	p1 = value(STRcwd);
	if (p1 == NULL || *p1 != '/')
	    abort();
	if (Strlen(p1) + Strlen(cp) + 1 >= MAXPATHLEN)
	    abort();
	(void)Strcpy(tmpdir, p1);
	(void)Strcat(tmpdir, STRslash);
	(void)Strcat(tmpdir, cp);
	xfree((ptr_t) cp);
	cp = p = Strsave(tmpdir);
    }

    while (*p) {		/* for each component */
	sp = p;			/* save slash address */
	while (*++p == '/')	/* flush extra slashes */
	    continue;
	if (p != ++sp)
	    for (p1 = sp, p2 = p; (*p1++ = *p2++) != '\0';)
		continue;
	p = sp;			/* save start of component */
	slash = 0;
	while (*++p)		/* find next slash or end of path */
	    if (*p == '/') {
		slash = 1;
		*p = 0;
		break;
	    }

	if (*sp == '\0') {	/* if component is null */
	    if (--sp == cp)	/* if path is one char (i.e. /) */
		break;
	    else
		*sp = '\0';
	} else if (sp[0] == '.' && sp[1] == 0) {
	    if (slash) {
		for (p1 = sp, p2 = p + 1; (*p1++ = *p2++) != '\0';)
		    continue;
		p = --sp;
	    }
	    else if (--sp != cp)
		*sp = '\0';
	}
	else if (sp[0] == '.' && sp[1] == '.' && sp[2] == 0) {
	    /*
	     * We have something like "yyy/xxx/..", where "yyy" can be null or
	     * a path starting at /, and "xxx" is a single component. Before
	     * compressing "xxx/..", we want to expand "yyy/xxx", if it is a
	     * symbolic link.
	     */
	    *--sp = 0;		/* form the pathname for readlink */
	    if (sp != cp && !adrof(STRignore_symlinks) &&
		(cc = readlink(short2str(cp), tlink,
			       sizeof(tlink) - 1)) >= 0) {
		tlink[cc] = '\0';
		(void)Strcpy(slink, str2short(tlink));

		if (slash)
		    *p = '/';
		/*
		 * Point p to the '/' in "/..", and restore the '/'.
		 */
		*(p = sp) = '/';
		/*
		 * find length of p
		 */
		for (p1 = p; *p1++;)
		    continue;
		if (*slink != '/') {
		    /*
		     * Relative path, expand it between the "yyy/" and the
		     * "/..". First, back sp up to the character past "yyy/".
		     */
		    while (*--sp != '/')
			continue;
		    sp++;
		    *sp = 0;
		    /*
		     * New length is "yyy/" + slink + "/.." and rest
		     */
		    p1 = newcp = xmalloc(
		        (size_t)((sp - cp) + cc + (p1 - p)) * sizeof(Char));
		    /*
		     * Copy new path into newcp
		     */
		    for (p2 = cp; (*p1++ = *p2++) != '\0';)
			continue;
		    for (p1--, p2 = slink; (*p1++ = *p2++) != '\0';)
			continue;
		    for (p1--, p2 = p; (*p1++ = *p2++) != '\0';)
			continue;
		    /*
		     * Restart canonicalization at expanded "/xxx".
		     */
		    p = sp - cp - 1 + newcp;
		}
		else {
		    /*
		     * New length is slink + "/.." and rest
		     */
		    p1 = newcp = xmalloc(
		        (size_t)(cc + (p1 - p)) * sizeof(Char));
		    /*
		     * Copy new path into newcp
		     */
		    for (p2 = slink; (*p1++ = *p2++) != '\0';)
			continue;
		    for (p1--, p2 = p; (*p1++ = *p2++) != '\0';)
			continue;
		    /*
		     * Restart canonicalization at beginning
		     */
		    p = newcp;
		}
		xfree((ptr_t) cp);
		cp = newcp;
		continue;	/* canonicalize the link */
	    }
	    *sp = '/';
	    if (sp != cp)
		while (*--sp != '/')
		    continue;
	    if (slash) {
		for (p1 = sp + 1, p2 = p + 1; (*p1++ = *p2++) != '\0';)
		    continue;
		p = sp;
	    }
	    else if (cp == sp)
		*++sp = '\0';
	    else
		*sp = '\0';
	}
	else {			/* normal dir name (not . or .. or nothing) */

	    if (sp != cp && adrof(STRchase_symlinks) &&
		!adrof(STRignore_symlinks) &&
		(cc = readlink(short2str(cp), tlink, sizeof(tlink)-1)) >= 0) {
		tlink[cc] = '\0';
		(void)Strcpy(slink, str2short(tlink));

		/*
		 * restore the '/'.
		 */
		if (slash)
		    *p = '/';

		/*
		 * point sp to p (rather than backing up).
		 */
		sp = p;

		/*
		 * find length of p
		 */
		for (p1 = p; *p1++;)
		    continue;
		if (*slink != '/') {
		    /*
		     * Relative path, expand it between the "yyy/" and the
		     * remainder. First, back sp up to the character past
		     * "yyy/".
		     */
		    while (*--sp != '/')
			continue;
		    sp++;
		    *sp = 0;
		    /*
		     * New length is "yyy/" + slink + "/.." and rest
		     */
		    p1 = newcp = xmalloc(
		        (size_t)((sp - cp) + cc + (p1 - p)) * sizeof(Char));
		    /*
		     * Copy new path into newcp
		     */
		    for (p2 = cp; (*p1++ = *p2++) != '\0';)
			continue;
		    for (p1--, p2 = slink; (*p1++ = *p2++) != '\0';)
			continue;
		    for (p1--, p2 = p; (*p1++ = *p2++) != '\0';)
			continue;
		    /*
		     * Restart canonicalization at expanded "/xxx".
		     */
		    p = sp - cp - 1 + newcp;
		}
		else {
		    /*
		     * New length is slink + the rest
		     */
		    p1 = newcp = xmalloc(
		        (size_t)(cc + (p1 - p)) * sizeof(Char));
		    /*
		     * Copy new path into newcp
		     */
		    for (p2 = slink; (*p1++ = *p2++) != '\0';)
			continue;
		    for (p1--, p2 = p; (*p1++ = *p2++) != '\0';)
			continue;
		    /*
		     * Restart canonicalization at beginning
		     */
		    p = newcp;
		}
		xfree((ptr_t) cp);
		cp = newcp;
		continue;	/* canonicalize the link */
	    }
	    if (slash)
		*p = '/';
	}
    }

    /*
     * fix home...
     */
    p1 = value(STRhome);
    len = Strlen(p1);
    /*
     * See if we're not in a subdir of STRhome
     */
    if (p1 && *p1 == '/' &&
	(Strncmp(p1, cp, len) != 0 || (cp[len] != '/' && cp[len] != '\0'))) {
	static ino_t home_ino;
	static dev_t home_dev = NODEV;
	static Char *home_ptr = NULL;
	struct stat statbuf;

	/*
	 * Get dev and ino of STRhome
	 */
	if (home_ptr != p1 &&
	    stat(short2str(p1), &statbuf) != -1) {
	    home_dev = statbuf.st_dev;
	    home_ino = statbuf.st_ino;
	    home_ptr = p1;
	}
	/*
	 * Start comparing dev & ino backwards
	 */
	p2 = Strcpy(slink, cp);
	for (sp = NULL; *p2 && stat(short2str(p2), &statbuf) != -1;) {
	    if (statbuf.st_dev == home_dev &&
		statbuf.st_ino == home_ino) {
		sp = (Char *) - 1;
		break;
	    }
	    if ((sp = Strrchr(p2, '/')) != NULL)
		*sp = '\0';
	}
	/*
	 * See if we found it
	 */
	if (*p2 && sp == (Char *) -1) {
	    /*
	     * Use STRhome to make '~' work
	     */
	    newcp = Strspl(p1, cp + Strlen(p2));
	    xfree((ptr_t) cp);
	    cp = newcp;
	}
    }
    return cp;
}


/*
 * dnewcwd - make a new directory in the loop the current one
 */
static void
dnewcwd(struct directory *dp)
{
    dcwd = dp;
    dset(dcwd->di_name);
    if (printd && !(adrof(STRpushdsilent)))
	printdirs();
}

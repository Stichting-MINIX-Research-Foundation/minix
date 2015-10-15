/*	$NetBSD: login_cap.c,v 1.32 2015/07/11 09:21:22 kamil Exp $	*/

/*-
 * Copyright (c) 1995,1997 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Berkeley Software Design,
 *	Inc.
 * 4. The name of Berkeley Software Design, Inc.  may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN, INC. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI login_cap.c,v 2.13 1998/02/07 03:17:05 prb Exp
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: login_cap.c,v 1.32 2015/07/11 09:21:22 kamil Exp $");
#endif /* LIBC_SCCS and not lint */
 
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/param.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <login_cap.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <util.h>

static u_quad_t	multiply(u_quad_t, u_quad_t);
static u_quad_t	strtolimit(const char *, char **, int);
static u_quad_t	strtosize(const char *, char **, int);
static int	gsetrl(login_cap_t *, int, const char *, int type);
static int	isinfinite(const char *);
static int	envset(void *, const char *, const char *, int);

login_cap_t *
login_getclass(const char *class)
{
	const char *classfiles[2];
	login_cap_t *lc;
	int res;

	/* class may be NULL */

	if (secure_path(_PATH_LOGIN_CONF) == 0) {
		classfiles[0] = _PATH_LOGIN_CONF;
		classfiles[1] = NULL;
	} else {
		classfiles[0] = NULL;
	}

	if ((lc = malloc(sizeof(login_cap_t))) == NULL) {
		syslog(LOG_ERR, "%s:%d malloc: %m", __FILE__, __LINE__);
		return (0);
	}

	lc->lc_cap = 0;
	lc->lc_style = 0;

	if (class == NULL || class[0] == '\0')
		class = LOGIN_DEFCLASS;

    	if ((lc->lc_class = strdup(class)) == NULL) {
		syslog(LOG_ERR, "%s:%d strdup: %m", __FILE__, __LINE__);
		free(lc);
		return (0);
	}

	/*
	 * Not having a login.conf file is not an error condition.
	 * The individual routines deal reasonably with missing
	 * capabilities and use default values.
	 */
	if (classfiles[0] == NULL)
		return(lc);

	if ((res = cgetent(&lc->lc_cap, classfiles, lc->lc_class)) != 0) {
		lc->lc_cap = 0;
		switch (res) {
		case 1: 
			syslog(LOG_ERR, "%s: couldn't resolve 'tc'",
				lc->lc_class);
			break;
		case -1:
			if (strcmp(lc->lc_class, LOGIN_DEFCLASS) == 0)
				return (lc);
			syslog(LOG_ERR, "%s: unknown class", lc->lc_class);
			break;
		case -2:
			syslog(LOG_ERR, "%s: getting class information: %m",
				lc->lc_class);
			break;
		case -3:
			syslog(LOG_ERR, "%s: 'tc' reference loop",
				lc->lc_class);
			break;
		default:
			syslog(LOG_ERR, "%s: unexpected cgetent error",
				lc->lc_class);
			break;
		}
		free(lc->lc_class);
		free(lc);
		return (0);
	}
	return (lc);
}

login_cap_t *
login_getpwclass(const struct passwd *pwd)
{

	/* pwd may be NULL */

	return login_getclass(pwd ? pwd->pw_class : NULL);
}

char *
login_getcapstr(login_cap_t *lc, const char *cap, char *def, char *e)
{
	char *res = NULL;
	int status;

	errno = 0;

	_DIAGASSERT(cap != NULL);

	if (!lc || !lc->lc_cap)
		return (def);

	switch (status = cgetstr(lc->lc_cap, cap, &res)) {
	case -1:
		if (res)
			free(res);
		return (def);
	case -2:
		syslog(LOG_ERR, "%s: getting capability %s: %m",
		    lc->lc_class, cap);
		if (res)
			free(res);
		return (e);
	default:
		if (status >= 0) 
			return (res);
		syslog(LOG_ERR, "%s: unexpected error with capability %s",
		    lc->lc_class, cap);
		if (res)
			free(res);
		return (e);
	}
}

quad_t
login_getcaptime(login_cap_t *lc, const char *cap, quad_t def, quad_t e)
{
	char *ep;
	char *res = NULL, *sres;
	int status;
	quad_t q, r;

	_DIAGASSERT(cap != NULL);

	errno = 0;
	if (!lc || !lc->lc_cap)
		return (def);

	switch (status = cgetstr(lc->lc_cap, cap, &res)) {
	case -1:
		if (res)
			free(res);
		return (def);
	case -2:
		syslog(LOG_ERR, "%s: getting capability %s: %m",
		    lc->lc_class, cap);
		errno = ERANGE;
		if (res)
			free(res);
		return (e);
	default:
		if (status >= 0) 
			break;
		syslog(LOG_ERR, "%s: unexpected error with capability %s",
		    lc->lc_class, cap);
		errno = ERANGE;
		if (res)
			free(res);
		return (e);
	}

	if (isinfinite(res))
		return (RLIM_INFINITY);

	errno = 0;

	q = 0;
	sres = res;
	while (*res) {
		r = strtoq(res, &ep, 0);
		if (!ep || ep == res ||
		    ((r == QUAD_MIN || r == QUAD_MAX) && errno == ERANGE)) {
invalid:
			syslog(LOG_ERR, "%s:%s=%s: invalid time",
			    lc->lc_class, cap, sres);
			errno = ERANGE;
			free(sres);
			return (e);
		}
		switch (*ep++) {
		case '\0':
			--ep;
			break;
		case 's': case 'S':
			break;
		case 'm': case 'M':
			r *= 60;
			break;
		case 'h': case 'H':
			r *= 60 * 60;
			break;
		case 'd': case 'D':
			r *= 60 * 60 * 24;
			break;
		case 'w': case 'W':
			r *= 60 * 60 * 24 * 7;
			break;
		case 'y': case 'Y':	/* Pretty absurd */
			r *= 60 * 60 * 24 * 365;
			break;
		default:
			goto invalid;
		}
		res = ep;
		q += r;
	}
	free(sres);
	return (q);
}

quad_t
login_getcapnum(login_cap_t *lc, const char *cap, quad_t def, quad_t e)
{
	char *ep;
	char *res = NULL;
	int status;
	quad_t q;

	_DIAGASSERT(cap != NULL);

	errno = 0;
	if (!lc || !lc->lc_cap)
		return (def);

	switch (status = cgetstr(lc->lc_cap, cap, &res)) {
	case -1:
		if (res)
			free(res);
		return (def);
	case -2:
		syslog(LOG_ERR, "%s: getting capability %s: %m",
		    lc->lc_class, cap);
		errno = ERANGE;
		if (res)
			free(res);
		return (e);
	default:
		if (status >= 0) 
			break;
		syslog(LOG_ERR, "%s: unexpected error with capability %s",
		    lc->lc_class, cap);
		errno = ERANGE;
		if (res)
			free(res);
		return (e);
	}

	if (isinfinite(res))
		return (RLIM_INFINITY);

	errno = 0;
    	q = strtoq(res, &ep, 0);
	if (!ep || ep == res || ep[0] ||
	    ((q == QUAD_MIN || q == QUAD_MAX) && errno == ERANGE)) {
		syslog(LOG_ERR, "%s:%s=%s: invalid number",
		    lc->lc_class, cap, res);
		errno = ERANGE;
		free(res);
		return (e);
	}
	free(res);
	return (q);
}

quad_t
login_getcapsize(login_cap_t *lc, const char *cap, quad_t def, quad_t e)
{
	char *ep;
	char *res = NULL;
	int status;
	quad_t q;

	_DIAGASSERT(cap != NULL);

	errno = 0;

	if (!lc || !lc->lc_cap)
		return (def);

	switch (status = cgetstr(lc->lc_cap, cap, &res)) {
	case -1:
		if (res)
			free(res);
		return (def);
	case -2:
		syslog(LOG_ERR, "%s: getting capability %s: %m",
		    lc->lc_class, cap);
		errno = ERANGE;
		if (res)
			free(res);
		return (e);
	default:
		if (status >= 0) 
			break;
		syslog(LOG_ERR, "%s: unexpected error with capability %s",
		    lc->lc_class, cap);
		errno = ERANGE;
		if (res)
			free(res);
		return (e);
	}

	errno = 0;
	q = strtolimit(res, &ep, 0);
	if (!ep || ep == res || (ep[0] && ep[1]) ||
	    ((q == QUAD_MIN || q == QUAD_MAX) && errno == ERANGE)) {
		syslog(LOG_ERR, "%s:%s=%s: invalid size",
		    lc->lc_class, cap, res);
		errno = ERANGE;
		free(res);
		return (e);
	}
	free(res);
	return (q);
}

int
login_getcapbool(login_cap_t *lc, const char *cap, u_int def)
{

	_DIAGASSERT(cap != NULL);

	if (!lc || !lc->lc_cap)
		return (def);

	return (cgetcap(lc->lc_cap, cap, ':') != NULL);
}

void
login_close(login_cap_t *lc)
{

	if (lc) {
		if (lc->lc_class)
			free(lc->lc_class);
		if (lc->lc_cap)
			free(lc->lc_cap);
		if (lc->lc_style)
			free(lc->lc_style);
		free(lc);
	}
}

#define	R_CTIME	1
#define	R_CSIZE	2
#define	R_CNUMB	3

static struct {
	int	what;
	int	type;
	const char *name;
} r_list[] = {
	{ RLIMIT_CPU,		R_CTIME, "cputime", },
	{ RLIMIT_FSIZE,		R_CSIZE, "filesize", },
	{ RLIMIT_DATA,		R_CSIZE, "datasize", },
	{ RLIMIT_STACK,		R_CSIZE, "stacksize", },
	{ RLIMIT_RSS,		R_CSIZE, "memoryuse", },
	{ RLIMIT_MEMLOCK,	R_CSIZE, "memorylocked", },
	{ RLIMIT_NPROC,		R_CNUMB, "maxproc", },
	{ RLIMIT_NTHR,		R_CNUMB, "maxthread", },
	{ RLIMIT_NOFILE,	R_CNUMB, "openfiles", },
	{ RLIMIT_CORE,		R_CSIZE, "coredumpsize", },
	{ RLIMIT_SBSIZE,	R_CSIZE, "sbsize", },
	{ RLIMIT_AS,		R_CSIZE, "vmemoryuse", },
	{ -1, 0, 0 }
};

static int
gsetrl(login_cap_t *lc, int what, const char *name, int type)
{
	struct rlimit rl;
	struct rlimit r;
	char name_cur[32];
	char name_max[32];

	_DIAGASSERT(name != NULL);

	(void)snprintf(name_cur, sizeof(name_cur), "%s-cur", name);
	(void)snprintf(name_max, sizeof(name_max), "%s-max", name);

	if (getrlimit(what, &r)) {
		syslog(LOG_ERR, "getting resource limit: %m");
		return (-1);
	}

#define	RCUR	((quad_t)r.rlim_cur)
#define	RMAX	((quad_t)r.rlim_max)

	switch (type) {
	case R_CTIME:
		r.rlim_cur = login_getcaptime(lc, name, RCUR, RCUR);
		r.rlim_max = login_getcaptime(lc, name, RMAX, RMAX);
		rl.rlim_cur = login_getcaptime(lc, name_cur, RCUR, RCUR);
		rl.rlim_max = login_getcaptime(lc, name_max, RMAX, RMAX);
		break;
	case R_CSIZE:
		r.rlim_cur = login_getcapsize(lc, name, RCUR, RCUR);
		r.rlim_max = login_getcapsize(lc, name, RMAX, RMAX);
		rl.rlim_cur = login_getcapsize(lc, name_cur, RCUR, RCUR);
		rl.rlim_max = login_getcapsize(lc, name_max, RMAX, RMAX);
		break;
	case R_CNUMB:
		r.rlim_cur = login_getcapnum(lc, name, RCUR, RCUR);
		r.rlim_max = login_getcapnum(lc, name, RMAX, RMAX);
		rl.rlim_cur = login_getcapnum(lc, name_cur, RCUR, RCUR);
		rl.rlim_max = login_getcapnum(lc, name_max, RMAX, RMAX);
		break;
	default:
		syslog(LOG_ERR, "%s: invalid type %d setting resource limit %s",
		    lc->lc_class, type, name);
		return (-1);
	}

	if (setrlimit(what, &rl)) {
		syslog(LOG_ERR, "%s: setting resource limit %s: %m",
		    lc->lc_class, name);
		return (-1);
	}
#undef	RCUR
#undef	RMAX
	return (0);
}

static int
/*ARGSUSED*/
envset(void *envp __unused, const char *name, const char *value, int overwrite)
{
	return setenv(name, value, overwrite);	
}

int
setuserenv(login_cap_t *lc, envfunc_t senv, void *envp)
{
	const char *stop = ", \t";
	size_t i, count;
	char *ptr;
	char **res;
	char *str = login_getcapstr(lc, "setenv", NULL, NULL);
		  
	if (str == NULL || *str == '\0')
		return 0;
	
	/*
	 * count the sub-strings, this may over-count since we don't
	 * account for escaped delimiters.
	 */
	for (i = 1, ptr = str; *ptr; i++) {
		ptr += strcspn(ptr, stop);
		if (*ptr)
			ptr++;
	}

	/* allocate ptr array and string */
	count = i;
	res = malloc(count * sizeof(*res) + strlen(str) + 1);

	if (!res)
		return -1;
	
	ptr = (char *)(void *)&res[count];
	(void)strcpy(ptr, str);

	/* split string */
	for (i = 0; (res[i] = stresep(&ptr, stop, '\\')) != NULL; )
		if (*res[i])
			i++;
	
	count = i;

	for (i = 0; i < count; i++) {
		if ((ptr = strchr(res[i], '=')) != NULL)
			*ptr++ = '\0';
		else 
			ptr = NULL;
		(void)(*senv)(envp, res[i], ptr ? ptr : "", 1);
	}
	
	free(res);
	return 0;
}

int
setclasscontext(const char *class, u_int flags)
{
	int ret;
	login_cap_t *lc;

	flags &= LOGIN_SETRESOURCES | LOGIN_SETPRIORITY | LOGIN_SETUMASK |
	    LOGIN_SETPATH;

	lc = login_getclass(class);
	ret = lc ? setusercontext(lc, NULL, 0, flags) : -1;
	login_close(lc);
	return (ret);
}

int
setusercontext(login_cap_t *lc, struct passwd *pwd, uid_t uid, u_int flags)
{
	char per_user_tmp[MAXPATHLEN + 1];
	const char *component_name;
	login_cap_t *flc;
	quad_t p;
	int i;
	ssize_t len;

	flc = NULL;

	if (!lc)
		flc = lc = login_getclass(pwd ? pwd->pw_class : NULL);

	/*
	 * Without the pwd entry being passed we cannot set either
	 * the group or the login.  We could complain about it.
	 */
	if (pwd == NULL)
		flags &= ~(LOGIN_SETGROUP|LOGIN_SETLOGIN);

#ifdef LOGIN_OSETGROUP
	if (pwd == NULL)
		flags &= ~LOGIN_OSETGROUP;
	if (flags & LOGIN_OSETGROUP)
		flags = (flags & ~LOGIN_OSETGROUP) | LOGIN_SETGROUP;
#endif
	if (flags & LOGIN_SETRESOURCES)
		for (i = 0; r_list[i].name; ++i) 
			(void)gsetrl(lc, r_list[i].what, r_list[i].name,
			    r_list[i].type);

	if (flags & LOGIN_SETPRIORITY) {
		p = login_getcapnum(lc, "priority", (quad_t)0, (quad_t)0);

		if (setpriority(PRIO_PROCESS, 0, (int)p) == -1)
			syslog(LOG_ERR, "%s: setpriority: %m", lc->lc_class);
	}

	if (flags & LOGIN_SETUMASK) {
		p = login_getcapnum(lc, "umask", (quad_t) LOGIN_DEFUMASK,
		    (quad_t)LOGIN_DEFUMASK);
		umask((mode_t)p);
	}

	if (flags & LOGIN_SETGID) {
		if (setgid(pwd->pw_gid) == -1) {
			syslog(LOG_ERR, "setgid(%d): %m", pwd->pw_gid);
			login_close(flc);
			return (-1);
		}
	}

	if (flags & LOGIN_SETGROUPS) {
		if (initgroups(pwd->pw_name, pwd->pw_gid) == -1) {
			syslog(LOG_ERR, "initgroups(%s,%d): %m",
			    pwd->pw_name, pwd->pw_gid);
			login_close(flc);
			return (-1);
		}
	}

	/* Create per-user temporary directories if needed. */
	if ((len = readlink("/tmp", per_user_tmp, 
	    sizeof(per_user_tmp) - 6)) != -1) {

		static const char atuid[] = "/@ruid";
		char *lp;

		/* readlink does not nul-terminate the string */
		per_user_tmp[len] = '\0';

		/* Check if it's magic symlink. */
		lp = strstr(per_user_tmp, atuid);
		if (lp != NULL && *(lp + (sizeof(atuid) - 1)) == '\0') {
			lp++;

			if (snprintf(lp, 11, "/%u", pwd->pw_uid) > 10) {
				syslog(LOG_ERR, "real temporary path too long");
				login_close(flc);
				return (-1);
			}
			if (mkdir(per_user_tmp, S_IRWXU) != -1) {
				if (chown(per_user_tmp, pwd->pw_uid,
				    pwd->pw_gid)) {
					component_name = "chown";
					goto out;
				}

				/* 
			 	 * Must set sticky bit for tmp directory, some
			 	 * programs rely on this.
			 	 */
				if(chmod(per_user_tmp, S_IRWXU | S_ISVTX)) {
					component_name = "chmod";
					goto out;
				}
			} else {
				if (errno != EEXIST) {
					component_name = "mkdir";
					goto out;
				} else {
					/* 
					 * We must ensure that we own the
					 * directory and that is has the correct
					 * permissions, otherwise a DOS attack
					 * is possible.
					 */
					struct stat sb;
					if (stat(per_user_tmp, &sb) == -1) {
						component_name = "stat";
						goto out;
					}

					if (sb.st_uid != pwd->pw_uid) {
						if (chown(per_user_tmp, 
						    pwd->pw_uid, pwd->pw_gid)) {
							component_name = "chown";
							goto out;
						}
					}

					if (sb.st_mode != (S_IRWXU | S_ISVTX)) {
						if (chmod(per_user_tmp, 
						    S_IRWXU | S_ISVTX)) {
							component_name = "chmod";
							goto out;
						}
					}
				}
			}
		}
	}
	errno = 0;

#if !defined(__minix)
	if (flags & LOGIN_SETLOGIN)
		if (setlogin(pwd->pw_name) == -1) {
			syslog(LOG_ERR, "setlogin(%s) failure: %m",
			    pwd->pw_name);
			login_close(flc);
			return (-1);
		}
#endif /* !defined(__minix) */

	if (flags & LOGIN_SETUSER)
		if (setuid(uid) == -1) {
			syslog(LOG_ERR, "setuid(%d): %m", uid);
			login_close(flc);
			return (-1);
		}

	if (flags & LOGIN_SETENV)
		setuserenv(lc, envset, NULL);

	if (flags & LOGIN_SETPATH)
		setuserpath(lc, pwd ? pwd->pw_dir : "", envset, NULL);

	login_close(flc);
	return (0);

out:
	if (component_name != NULL) {
		syslog(LOG_ERR, "%s %s: %m", component_name, per_user_tmp);
		login_close(flc);
		return (-1);
	} else {
		syslog(LOG_ERR, "%s: %m", per_user_tmp);
		login_close(flc);
		return (-1);
	}
}

void
setuserpath(login_cap_t *lc, const char *home, envfunc_t senv, void *envp)
{
	size_t hlen, plen;
	int cnt = 0;
	char *path;
	const char *cpath;
	char *p, *q;

	_DIAGASSERT(home != NULL);

	hlen = strlen(home);

	p = path = login_getcapstr(lc, "path", NULL, NULL);
	if (p) {
		while (*p)
			if (*p++ == '~')
				++cnt;
		plen = (p - path) + cnt * (hlen + 1) + 1;
		p = path;
		q = path = malloc(plen);
		if (q) {
			while (*p) {
				p += strspn(p, " \t");
				if (*p == '\0')
					break;
				plen = strcspn(p, " \t");
				if (hlen == 0 && *p == '~') {
					p += plen;
					continue;
				}
				if (q != path)
					*q++ = ':';
				if (*p == '~') {
					strcpy(q, home);
					q += hlen;
					++p;
					--plen;
				}
				memcpy(q, p, plen);
				p += plen;
				q += plen;
			}
			*q = '\0';
			cpath = path;
		} else
			cpath = _PATH_DEFPATH;
	} else
		cpath = _PATH_DEFPATH;
	if ((*senv)(envp, "PATH", cpath, 1))
		warn("could not set PATH");
}

/*
 * Convert an expression of the following forms
 * 	1) A number.
 *	2) A number followed by a b (mult by 512).
 *	3) A number followed by a k (mult by 1024).
 *	5) A number followed by a m (mult by 1024 * 1024).
 *	6) A number followed by a g (mult by 1024 * 1024 * 1024).
 *	7) A number followed by a t (mult by 1024 * 1024 * 1024 * 1024).
 *	8) Two or more numbers (with/without k,b,m,g, or t).
 *	   separated by x (also * for backwards compatibility), specifying
 *	   the product of the indicated values.
 */
static u_quad_t
strtosize(const char *str, char **endptr, int radix)
{
	u_quad_t num, num2;
	char *expr, *expr2;

	_DIAGASSERT(str != NULL);
	/* endptr may be NULL */

	errno = 0;
	num = strtouq(str, &expr, radix);
	if (errno || expr == str) {
		if (endptr)
			*endptr = expr;
		return (num);
	}

	switch(*expr) {
	case 'b': case 'B':
		num = multiply(num, (u_quad_t)512);
		++expr;
		break;
	case 'k': case 'K':
		num = multiply(num, (u_quad_t)1024);
		++expr;
		break;
	case 'm': case 'M':
		num = multiply(num, (u_quad_t)1024 * 1024);
		++expr;
		break;
	case 'g': case 'G':
		num = multiply(num, (u_quad_t)1024 * 1024 * 1024);
		++expr;
		break;
	case 't': case 'T':
		num = multiply(num, (u_quad_t)1024 * 1024);
		num = multiply(num, (u_quad_t)1024 * 1024);
		++expr;
		break;
	}

	if (errno)
		goto erange;

	switch(*expr) {
	case '*':			/* Backward compatible. */
	case 'x':
		num2 = strtosize(expr+1, &expr2, radix);
		if (errno) {
			expr = expr2;
			goto erange;
		}

		if (expr2 == expr + 1) {
			if (endptr)
				*endptr = expr;
			return (num);
		}
		expr = expr2;
		num = multiply(num, num2);
		if (errno)
			goto erange;
		break;
	}
	if (endptr)
		*endptr = expr;
	return (num);
erange:
	if (endptr)
		*endptr = expr;
	errno = ERANGE;
	return (UQUAD_MAX);
}

static u_quad_t
strtolimit(const char *str, char **endptr, int radix)
{

	_DIAGASSERT(str != NULL);
	/* endptr may be NULL */

	if (isinfinite(str)) {
		if (endptr)
			*endptr = (char *)__UNCONST(str) + strlen(str);
		return ((u_quad_t)RLIM_INFINITY);
	}
	return (strtosize(str, endptr, radix));
}

static int
isinfinite(const char *s)
{
	static const char *infs[] = {
		"infinity",
		"inf",
		"unlimited",
		"unlimit",
		NULL
	};
	const char **i;

	_DIAGASSERT(s != NULL);

	for (i = infs; *i; i++) {
		if (!strcasecmp(s, *i))
			return 1;
	}
	return 0;
}

static u_quad_t
multiply(u_quad_t n1, u_quad_t n2)
{
	static int bpw = 0;
	u_quad_t m;
	u_quad_t r;
	int b1, b2;

	/*
	 * Get rid of the simple cases
	 */
	if (n1 == 0 || n2 == 0)
		return (0);
	if (n1 == 1)
		return (n2);
	if (n2 == 1)
		return (n1);

	/*
	 * sizeof() returns number of bytes needed for storage.
	 * This may be different from the actual number of useful bits.
	 */
	if (!bpw) {
		bpw = sizeof(u_quad_t) * 8;
		while (((u_quad_t)1 << (bpw-1)) == 0)
			--bpw;
	}

	/*
	 * First check the magnitude of each number.  If the sum of the
	 * magnatude is way to high, reject the number.  (If this test
	 * is not done then the first multiply below may overflow.)
	 */
	for (b1 = bpw; (((u_quad_t)1 << (b1-1)) & n1) == 0; --b1)
		; 
	for (b2 = bpw; (((u_quad_t)1 << (b2-1)) & n2) == 0; --b2)
		; 
	if (b1 + b2 - 2 > bpw) {
		errno = ERANGE;
		return (UQUAD_MAX);
	}

	/*
	 * Decompose the multiplication to be:
	 * h1 = n1 & ~1
	 * h2 = n2 & ~1
	 * l1 = n1 & 1
	 * l2 = n2 & 1
	 * (h1 + l1) * (h2 + l2)
	 * (h1 * h2) + (h1 * l2) + (l1 * h2) + (l1 * l2)
	 *
	 * Since h1 && h2 do not have the low bit set, we can then say:
	 *
	 * (h1>>1 * h2>>1 * 4) + ...
	 *
	 * So if (h1>>1 * h2>>1) > (1<<(bpw - 2)) then the result will
	 * overflow.
	 *
	 * Finally, if MAX - ((h1 * l2) + (l1 * h2) + (l1 * l2)) < (h1*h2)
	 * then adding in residual amout will cause an overflow.
	 */

	m = (n1 >> 1) * (n2 >> 1);

	if (m >= ((u_quad_t)1 << (bpw-2))) {
		errno = ERANGE;
		return (UQUAD_MAX);
	}

	m *= 4;

	r = (n1 & n2 & 1)
	  + (n2 & 1) * (n1 & ~(u_quad_t)1)
	  + (n1 & 1) * (n2 & ~(u_quad_t)1);

	if ((u_quad_t)(m + r) < m) {
		errno = ERANGE;
		return (UQUAD_MAX);
	}
	m += r;

	return (m);
}

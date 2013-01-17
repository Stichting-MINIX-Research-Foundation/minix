/*	$NetBSD: passwd.c,v 1.52 2012/06/25 22:32:47 abs Exp $	*/

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
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: passwd.c,v 1.52 2012/06/25 22:32:47 abs Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

static const char      *pw_filename(const char *filename);
static void		pw_cont(int sig);
static const char *	pw_equal(char *buf, struct passwd *old_pw);
static const char      *pw_default(const char *option);
static int		read_line(FILE *fp, char *line, int max);
static void		trim_whitespace(char *line);

static	char	pw_prefix[MAXPATHLEN];

const char *
pw_getprefix(void)
{

	return(pw_prefix);
}

int
pw_setprefix(const char *new_prefix)
{
	size_t length;

	_DIAGASSERT(new_prefix != NULL);

	length = strlen(new_prefix);
	if (length < sizeof(pw_prefix)) {
		(void)strcpy(pw_prefix, new_prefix);
		while (length > 0 && pw_prefix[length - 1] == '/')
			pw_prefix[--length] = '\0';
		return(0);
	}
	errno = ENAMETOOLONG;
	return(-1);
}

static const char *
pw_filename(const char *filename)
{
	static char newfilename[MAXPATHLEN];

	_DIAGASSERT(filename != NULL);

	if (pw_prefix[0] == '\0')
		return filename;

	if (strlen(pw_prefix) + strlen(filename) < sizeof(newfilename))
		return strcat(strcpy(newfilename, pw_prefix), filename);

	errno = ENAMETOOLONG;
	return(NULL);
}

int
pw_lock(int retries)
{
	const char *filename;
	int i, fd;
	mode_t old_mode;
	int oerrno;

	/* Acquire the lock file. */
	filename = pw_filename(_PATH_MASTERPASSWD_LOCK);
	if (filename == NULL)
		return(-1);
	old_mode = umask(0);
	fd = open(filename, O_WRONLY|O_CREAT|O_EXCL, 0600);
	for (i = 0; i < retries && fd < 0 && errno == EEXIST; i++) {
		sleep(1);
		fd = open(filename, O_WRONLY|O_CREAT|O_EXCL,
			  0600);
	}
	oerrno = errno;
	(void)umask(old_mode);
	errno = oerrno;
	return(fd);
}

int
pw_mkdb(const char *username, int secureonly)
{
	const char *args[9];
	int pstat, i;
	pid_t pid;

	pid = vfork();
	if (pid == -1)
		return -1;

	if (pid == 0) {
		args[0] = "pwd_mkdb";
		args[1] = "-d";
		args[2] = pw_prefix;
		args[3] = "-pl";
		i = 4;

		if (secureonly)
			args[i++] = "-s";
		if (username != NULL) {
			args[i++] = "-u";
			args[i++] = username;
		}

		args[i++] = pw_filename(_PATH_MASTERPASSWD_LOCK);
		args[i] = NULL;
		execv(_PATH_PWD_MKDB, (char * const *)__UNCONST(args));
		_exit(1);
	}
	pid = waitpid(pid, &pstat, 0);
	if (pid == -1) {
		warn("error waiting for pid %lu", (unsigned long)pid);
		return -1;
	}
	if (WIFEXITED(pstat)) {
		if (WEXITSTATUS(pstat) != 0) {
			warnx("pwd_mkdb exited with status %d",
			    WEXITSTATUS(pstat));
			return -1;
		}
	} else if (WIFSIGNALED(pstat)) {
		warnx("pwd_mkdb exited with signal %d", WTERMSIG(pstat));
		return -1;
	}
	return 0;
}

int
pw_abort(void)
{
	const char *filename;

	filename = pw_filename(_PATH_MASTERPASSWD_LOCK);
	return((filename == NULL) ? -1 : unlink(filename));
}

/* Everything below this point is intended for the convenience of programs
 * which allow a user to interactively edit the passwd file.  Errors in the
 * routines below will cause the process to abort. */

static pid_t editpid = -1;

static void
pw_cont(int sig)
{

	if (editpid != -1)
		kill(editpid, sig);
}

void
pw_init(void)
{
#ifndef __minix
	struct rlimit rlim;

	/* Unlimited resource limits. */
	rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
	(void)setrlimit(RLIMIT_CPU, &rlim);
	(void)setrlimit(RLIMIT_FSIZE, &rlim);
	(void)setrlimit(RLIMIT_STACK, &rlim);
	(void)setrlimit(RLIMIT_DATA, &rlim);
	(void)setrlimit(RLIMIT_RSS, &rlim);

	/* Don't drop core (not really necessary, but GP's). */
	rlim.rlim_cur = rlim.rlim_max = 0;
	(void)setrlimit(RLIMIT_CORE, &rlim);
#endif

	/* Turn off signals. */
	(void)signal(SIGALRM, SIG_IGN);
	(void)signal(SIGHUP, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGPIPE, SIG_IGN);
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGTERM, SIG_IGN);
	(void)signal(SIGCONT, pw_cont);
}

void
pw_edit(int notsetuid, const char *filename)
{
	int pstat;
	char *p;
	const char * volatile editor;
	const char *argp[] = { "sh", "-c", NULL, NULL };

	if (filename == NULL)
		filename = _PATH_MASTERPASSWD_LOCK;

	filename = pw_filename(filename);
	if (filename == NULL)
		return;

	if ((editor = getenv("EDITOR")) == NULL)
		editor = _PATH_VI;

	p = malloc(strlen(editor) + 1 + strlen(filename) + 1);
	if (p == NULL)
		return;

	sprintf(p, "%s %s", editor, filename);
	argp[2] = p;

	switch(editpid = vfork()) {
	case -1:
		free(p);
		return;
	case 0:
		if (notsetuid) {
			setgid(getgid());
			setuid(getuid());
		}
		execvp(_PATH_BSHELL, (char *const *)__UNCONST(argp));
		_exit(1);
	}

	free(p);

	for (;;) {
		editpid = waitpid(editpid, (int *)&pstat, WUNTRACED);
		if (editpid == -1)
			pw_error(editor, 1, 1);
		else if (WIFSTOPPED(pstat))
			raise(WSTOPSIG(pstat));
		else if (WIFEXITED(pstat) && WEXITSTATUS(pstat) == 0)
			break;
		else
			pw_error(editor, 1, 1);
	}
	editpid = -1;
}

void
pw_prompt(void)
{
	int c;

	(void)printf("re-edit the password file? [y]: ");
	(void)fflush(stdout);
	c = getchar();
	if (c != EOF && c != '\n')
		while (getchar() != '\n');
	if (c == 'n')
		pw_error(NULL, 0, 0);
}

/* for use in pw_copy(). Compare a pw entry to a pw struct. */
/* returns a character string labelling the miscompared field or 0 */
static const char *
pw_equal(char *buf, struct passwd *pw)
{
	struct passwd buf_pw;
	size_t len;

	_DIAGASSERT(buf != NULL);
	_DIAGASSERT(pw != NULL);

	len = strlen (buf);
	if (buf[len-1] == '\n')
		buf[len-1] = '\0';
	if (!pw_scan(buf, &buf_pw, NULL))
		return "corrupt line";
	if (strcmp(pw->pw_name, buf_pw.pw_name) != 0)
		return "name";
	if (pw->pw_uid != buf_pw.pw_uid)
		return "uid";
	if (pw->pw_gid != buf_pw.pw_gid)
		return "gid";
	if (strcmp( pw->pw_class, buf_pw.pw_class) != 0)
		return "class";
	if (pw->pw_change != buf_pw.pw_change)
		return "change";
	if (pw->pw_expire != buf_pw.pw_expire)
		return "expire";
	if (strcmp( pw->pw_gecos, buf_pw.pw_gecos) != 0)
		return "gecos";
	if (strcmp( pw->pw_dir, buf_pw.pw_dir) != 0)
		return "dir";
	if (strcmp( pw->pw_shell, buf_pw.pw_shell) != 0)
		return "shell";
	return (char *)0;
}

void
pw_copy(int ffd, int tfd, struct passwd *pw, struct passwd *old_pw)
{
	char errbuf[200];
	int rv;

	rv = pw_copyx(ffd, tfd, pw, old_pw, errbuf, sizeof(errbuf));
	if (rv == 0) {
		warnx("%s", errbuf);
		pw_error(NULL, 0, 1);
	}
}

static void
pw_print(FILE *to, const struct passwd *pw)
{
	(void)fprintf(to, "%s:%s:%d:%d:%s:%lld:%lld:%s:%s:%s\n",
	    pw->pw_name, pw->pw_passwd, pw->pw_uid, pw->pw_gid,
	    pw->pw_class, (long long)pw->pw_change,
	    (long long)pw->pw_expire,
	    pw->pw_gecos, pw->pw_dir, pw->pw_shell);
}

int
pw_copyx(int ffd, int tfd, struct passwd *pw, struct passwd *old_pw,
    char *errbuf, size_t errbufsz)
{
	const char *filename;
	char mpwd[MAXPATHLEN], mpwdl[MAXPATHLEN], *p, buf[8192];
	FILE *from, *to;
	int done;

	_DIAGASSERT(pw != NULL);
	_DIAGASSERT(errbuf != NULL);
	/* old_pw may be NULL */

	if ((filename = pw_filename(_PATH_MASTERPASSWD)) == NULL) {
		snprintf(errbuf, errbufsz, "%s: %s", pw_prefix,
		    strerror(errno));
		return (0);
	}
	(void)strcpy(mpwd, filename);
	if ((filename = pw_filename(_PATH_MASTERPASSWD_LOCK)) == NULL) {
		snprintf(errbuf, errbufsz, "%s: %s", pw_prefix,
		    strerror(errno));
		return (0);
	}
	(void)strcpy(mpwdl, filename);

	if (!(from = fdopen(ffd, "r"))) {
		snprintf(errbuf, errbufsz, "%s: %s", mpwd, strerror(errno));
		return (0);
	}
	if (!(to = fdopen(tfd, "w"))) {
		snprintf(errbuf, errbufsz, "%s: %s", mpwdl, strerror(errno));
		(void)fclose(from);
		return (0);
	}

	for (done = 0; fgets(buf, (int)sizeof(buf), from);) {
		const char *neq;
		if (!strchr(buf, '\n')) {
			snprintf(errbuf, errbufsz, "%s: line too long", mpwd);
			(void)fclose(from);
			(void)fclose(to);
			return (0);
		}
		if (done) {
			(void)fprintf(to, "%s", buf);
			if (ferror(to)) {
				snprintf(errbuf, errbufsz, "%s",
				    strerror(errno));
				(void)fclose(from);
				(void)fclose(to);
				return (0);
			}
			continue;
		}
		if (!(p = strchr(buf, ':'))) {
			snprintf(errbuf, errbufsz, "%s: corrupted entry", mpwd);
			(void)fclose(from);
			(void)fclose(to);
			return (0);
		}
		*p = '\0';
		if (strcmp(buf, pw->pw_name)) {
			*p = ':';
			(void)fprintf(to, "%s", buf);
			if (ferror(to)) {
				snprintf(errbuf, errbufsz, "%s",
				    strerror(errno));
				(void)fclose(from);
				(void)fclose(to);
				return (0);
			}
			continue;
		}
		*p = ':';
		if (old_pw && (neq = pw_equal(buf, old_pw)) != NULL) {
			if (strcmp(neq, "corrupt line") == 0)
				(void)snprintf(errbuf, errbufsz,
				    "%s: entry %s corrupted", mpwd,
				    pw->pw_name);
			else
				(void)snprintf(errbuf, errbufsz,
				    "%s: entry %s inconsistent %s",
				    mpwd, pw->pw_name, neq);
			(void)fclose(from);
			(void)fclose(to);
			return (0);
		}
		pw_print(to, pw);
		done = 1;
		if (ferror(to)) {
			snprintf(errbuf, errbufsz, "%s", strerror(errno));
			(void)fclose(from);
			(void)fclose(to);
			return (0);
		}
	}
	/* Only append a new entry if real uid is root! */
	if (!done) {
		if (getuid() == 0) {
			pw_print(to, pw);
			done = 1;
		} else {
			snprintf(errbuf, errbufsz,
			    "%s: changes not made, no such entry", mpwd);
		}
	}

	if (ferror(to)) {
		snprintf(errbuf, errbufsz, "%s", strerror(errno));
		(void)fclose(from);
		(void)fclose(to);
		return (0);
	}
	(void)fclose(from);
	(void)fclose(to);

	return (done);
}

void
pw_error(const char *name, int error, int eval)
{

	if (error) {
		if (name)
			warn("%s", name);
		else
			warn(NULL);
	}

	warnx("%s%s: unchanged", pw_prefix, _PATH_MASTERPASSWD);
	pw_abort();
	exit(eval);
}

/* Removes head and/or tail spaces. */
static void
trim_whitespace(char *line)
{
	char *p;

	_DIAGASSERT(line != NULL);

	/* Remove leading spaces */
	p = line;
	while (isspace((unsigned char) *p))
		p++;
	memmove(line, p, strlen(p) + 1);

	/* Remove trailing spaces */
	p = line + strlen(line) - 1;
	while (isspace((unsigned char) *p))
		p--;
	*(p + 1) = '\0';
}


/* Get one line, remove spaces from front and tail */
static int
read_line(FILE *fp, char *line, int max)
{
	char   *p;

	_DIAGASSERT(fp != NULL);
	_DIAGASSERT(line != NULL);

	/* Read one line of config */
	if (fgets(line, max, fp) == NULL)
		return (0);

	if ((p = strchr(line, '\n')) == NULL) {
		warnx("line too long");
		return (0);
	}
	*p = '\0';

	/* Remove comments */
	if ((p = strchr(line, '#')) != NULL)
		*p = '\0';

	trim_whitespace(line);
	return (1);
}

static const char *
pw_default(const char *option)
{
	static const char *options[][2] = {
		{ "localcipher",	"old" },
		{ "ypcipher",		"old" },
	};
	size_t i;

	_DIAGASSERT(option != NULL);
	for (i = 0; i < sizeof(options) / sizeof(options[0]); i++)
		if (strcmp(options[i][0], option) == 0)
			return (options[i][1]);

	return (NULL);
}

/*
 * Retrieve password information from the /etc/passwd.conf file, at the
 * moment this is only for choosing the cipher to use.  It could easily be
 * used for other authentication methods as well.
 */
void
pw_getconf(char *data, size_t max, const char *key, const char *option)
{
	FILE *fp;
	char line[LINE_MAX], *p, *p2;
	static char result[LINE_MAX];
	int got, found;
	const char *cp;

	_DIAGASSERT(data != NULL);
	_DIAGASSERT(key != NULL);
	_DIAGASSERT(option != NULL);

	got = 0;
	found = 0;
	result[0] = '\0';

	if ((fp = fopen(_PATH_PASSWD_CONF, "r")) == NULL) {
		if ((cp = pw_default(option)) != NULL)
			strlcpy(data, cp, max);
		else
			data[0] = '\0';
		return;
	}

	while (!found && (got || read_line(fp, line, LINE_MAX))) {
		got = 0;

		if (strncmp(key, line, strlen(key)) != 0 ||
		    line[strlen(key)] != ':')
			continue;

		/* Now we found our specified key */
		while (read_line(fp, line, LINE_MAX)) {
			/* Leaving key field */
			if (line[0] != '\0' && strchr(line + 1, ':') != NULL) {
				got = 1;
				break;
			}
			p2 = line;
			if ((p = strsep(&p2, "=")) == NULL || p2 == NULL)
				continue;
			trim_whitespace(p);

			if (!strncmp(p, option, strlen(option))) {
				trim_whitespace(p2);
				strcpy(result, p2);
				found = 1;
				break;
			}
		}
	}
	fclose(fp);

	if (!found)
		errno = ENOENT;
	if (!got)
		errno = ENOTDIR;

	/* 
	 * If we got no result and were looking for a default
	 * value, try hard coded defaults.
	 */

	if (strlen(result) == 0 && strcmp(key, "default") == 0 &&
	    (cp = pw_default(option)) != NULL)
		strlcpy(data, cp, max);
	else 
		strlcpy(data, result, max);
}

void
pw_getpwconf(char *data, size_t max, const struct passwd *pwd,
    const char *option)
{
	char grpkey[LINE_MAX];
	struct group grs, *grp;
	char grbuf[1024];

	pw_getconf(data, max, pwd->pw_name, option);

	/* Try to find an entry for the group */
	if (*data == '\0') {
		(void)getgrgid_r(pwd->pw_gid, &grs, grbuf, sizeof(grbuf), &grp);
		if (grp != NULL) {
			(void)snprintf(grpkey, sizeof(grpkey), ":%s",
			    grp->gr_name);
			pw_getconf(data, max, grpkey, option);
		}
		if (*data == '\0')
		        pw_getconf(data, max, "default", option);
	}
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(pw_copy, __pw_copy50)
__weak_alias(pw_copyx, __pw_copyx50)
__weak_alias(pw_getpwconf, __pw_getpwconf50)
#endif

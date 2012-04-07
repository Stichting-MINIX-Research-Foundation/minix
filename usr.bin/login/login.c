/*	$NetBSD: login.c,v 1.97 2009/12/29 19:26:13 christos Exp $	*/

/*-
 * Copyright (c) 1980, 1987, 1988, 1991, 1993, 1994
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
__COPYRIGHT("@(#) Copyright (c) 1980, 1987, 1988, 1991, 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)login.c	8.4 (Berkeley) 4/2/94";
#endif
__RCSID("$NetBSD: login.c,v 1.97 2009/12/29 19:26:13 christos Exp $");
#endif /* not lint */

/*
 * login [ name ]
 * login -h hostname	(for telnetd, etc.)
 * login -f name	(for pre-authenticated login: datakit, xterm, etc.)
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/socket.h>

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <ttyent.h>
#include <tzfile.h>
#include <unistd.h>
#include <sysexits.h>
#ifdef SUPPORT_UTMP
#include <utmp.h>
#endif
#ifdef SUPPORT_UTMPX
#include <utmpx.h>
#endif
#include <util.h>
#ifdef SKEY
#include <skey.h>
#endif
#ifdef KERBEROS5
#include <krb5/krb5.h>
#include <com_err.h>
#endif
#ifdef LOGIN_CAP
#include <login_cap.h>
#endif
#include <vis.h>

#include "pathnames.h"
#include "common.h"

#ifdef KERBEROS5
int login_krb5_forwardable_tgt = 0;
static int login_krb5_get_tickets = 1;
static int login_krb5_retain_ccache = 0;
#endif

static void	 checknologin(char *);
#ifdef KERBEROS5
int	 k5login(struct passwd *, char *, char *, char *);
void	 k5destroy(void);
int	 k5_read_creds(char*);
int	 k5_write_creds(void);
#endif
#if defined(KERBEROS5)
static void	 dofork(void);
#endif
static void	 usage(void);

#define	TTYGRPNAME	"tty"		/* name of group to own ttys */

#define DEFAULT_BACKOFF 3
#define DEFAULT_RETRIES 10

#if defined(KERBEROS5)
int	has_ccache = 0;
static int	notickets = 1;
static char	*instance;
extern krb5_context kcontext;
extern int	have_forward;
extern char	*krb5tkfile_env;
extern int	krb5_configured;
#endif

#if defined(KERBEROS5)
#define	KERBEROS_CONFIGURED	krb5_configured
#endif

extern char **environ;

int
main(int argc, char *argv[])
{
	struct group *gr;
	struct stat st;
	int ask, ch, cnt, fflag, hflag, pflag, sflag, quietlog, rootlogin, rval;
	int Fflag;
	uid_t uid, saved_uid;
	gid_t saved_gid, saved_gids[NGROUPS_MAX];
	int nsaved_gids;
	char *domain, *p, *ttyn, *pwprompt;
	char tbuf[MAXPATHLEN + 2], tname[sizeof(_PATH_TTY) + 10];
	char localhost[MAXHOSTNAMELEN + 1];
	int need_chpass, require_chpass;
	int login_retries = DEFAULT_RETRIES, 
	    login_backoff = DEFAULT_BACKOFF;
	time_t pw_warntime = _PASSWORD_WARNDAYS * SECSPERDAY;
#ifdef KERBEROS5
	krb5_error_code kerror;
#endif
#if defined(KERBEROS5)
	int got_tickets = 0;
#endif
#ifdef LOGIN_CAP
	char *shell = NULL;
	login_cap_t *lc = NULL;
#endif

	tbuf[0] = '\0';
	rval = 0;
	pwprompt = NULL;
	nested = NULL;
	need_chpass = require_chpass = 0;

	(void)signal(SIGALRM, timedout);
	(void)alarm(timeout);
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)setpriority(PRIO_PROCESS, 0, 0);

	openlog("login", 0, LOG_AUTH);

	/*
	 * -p is used by getty to tell login not to destroy the environment
	 * -f is used to skip a second login authentication
	 * -h is used by other servers to pass the name of the remote host to
	 *    login so that it may be placed in utmp/utmpx and wtmp/wtmpx
	 * -a in addition to -h, a server may supply -a to pass the actual
	 *    server address.
	 * -s is used to force use of S/Key or equivalent.
	 */
	domain = NULL;
	if (gethostname(localhost, sizeof(localhost)) < 0)
		syslog(LOG_ERR, "couldn't get local hostname: %m");
	else
		domain = strchr(localhost, '.');
	localhost[sizeof(localhost) - 1] = '\0';

	Fflag = fflag = hflag = pflag = sflag = 0;
	have_ss = 0;
#ifdef KERBEROS5
	have_forward = 0;
#endif
	uid = getuid();
	while ((ch = getopt(argc, argv, "a:Ffh:ps")) != -1)
		switch (ch) {
		case 'a':
			if (uid)
				errx(EXIT_FAILURE, "-a option: %s", strerror(EPERM));
			decode_ss(optarg);
#ifdef notdef
			(void)sockaddr_snprintf(optarg,
			    sizeof(struct sockaddr_storage), "%a", (void *)&ss);
#endif
			break;
		case 'F':
			Fflag = 1;
			/* FALLTHROUGH */
		case 'f':
			fflag = 1;
			break;
		case 'h':
			if (uid)
				errx(EXIT_FAILURE, "-h option: %s", strerror(EPERM));
			hflag = 1;
#ifdef notdef
			if (domain && (p = strchr(optarg, '.')) != NULL &&
			    strcasecmp(p, domain) == 0)
				*p = '\0';
#endif
			hostname = optarg;
			break;
		case 'p':
			pflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		default:
		case '?':
			usage();
			break;
		}

#ifndef __minix
	setproctitle(NULL);
#endif
	argc -= optind;
	argv += optind;

	if (*argv) {
		username = *argv;
		ask = 0;
	} else
		ask = 1;

#ifdef F_CLOSEM
	(void)fcntl(3, F_CLOSEM, 0);
#else
	for (cnt = getdtablesize(); cnt > 2; cnt--)
		(void)close(cnt);
#endif

	ttyn = ttyname(STDIN_FILENO);
	if (ttyn == NULL || *ttyn == '\0') {
		(void)snprintf(tname, sizeof(tname), "%s??", _PATH_TTY);
		ttyn = tname;
	}
	if ((tty = strstr(ttyn, "/pts/")) != NULL)
		++tty;
	else if ((tty = strrchr(ttyn, '/')) != NULL)
		++tty;
	else
		tty = ttyn;

	if (issetugid()) {
		nested = strdup(user_from_uid(getuid(), 0));
		if (nested == NULL) {
			syslog(LOG_ERR, "strdup: %m");
			sleepexit(EXIT_FAILURE);
		}
	}

#ifdef LOGIN_CAP
	/* Get "login-retries" and "login-backoff" from default class */
	if ((lc = login_getclass(NULL)) != NULL) {
		login_retries = (int)login_getcapnum(lc, "login-retries",
		    DEFAULT_RETRIES, DEFAULT_RETRIES);
		login_backoff = (int)login_getcapnum(lc, "login-backoff", 
		    DEFAULT_BACKOFF, DEFAULT_BACKOFF);
		login_close(lc);
		lc = NULL;
	}
#endif

#ifdef KERBEROS5
	kerror = krb5_init_context(&kcontext);
	if (kerror) {
		/*
		 * If Kerberos is not configured, that is, we are
		 * not using Kerberos, do not log the error message.
		 * However, if Kerberos is configured,  and the
		 * context init fails for some other reason, we need
		 * to issue a no tickets warning to the user when the
		 * login succeeds.
		 */
		if (kerror != ENXIO) {	/* XXX NetBSD-local Heimdal hack */
			syslog(LOG_NOTICE,
			    "%s when initializing Kerberos context",
			    error_message(kerror));
			krb5_configured = 1;
		}
		login_krb5_get_tickets = 0;
	}
#endif /* KERBEROS5 */

	for (cnt = 0;; ask = 1) {
#if defined(KERBEROS5)
		if (login_krb5_get_tickets)
			k5destroy();
#endif
		if (ask) {
			fflag = 0;
			getloginname();
		}
		rootlogin = 0;
#ifdef KERBEROS5
		if ((instance = strchr(username, '/')) != NULL)
			*instance++ = '\0';
		else
			instance = "";
#endif
		if (strlen(username) > MAXLOGNAME)
			username[MAXLOGNAME] = '\0';

		/*
		 * Note if trying multiple user names; log failures for
		 * previous user name, but don't bother logging one failure
		 * for nonexistent name (mistyped username).
		 */
		if (failures && strcmp(tbuf, username)) {
			if (failures > (pwd ? 0 : 1))
				badlogin(tbuf);
			failures = 0;
		}
		(void)strlcpy(tbuf, username, sizeof(tbuf));

		pwd = getpwnam(username);

#ifdef LOGIN_CAP
		/*
		 * Establish the class now, before we might goto
		 * within the next block. pwd can be NULL since it
		 * falls back to the "default" class if it is.
		 */
		lc = login_getclass(pwd ? pwd->pw_class : NULL);
#endif
		/*
		 * if we have a valid account name, and it doesn't have a
		 * password, or the -f option was specified and the caller
		 * is root or the caller isn't changing their uid, don't
		 * authenticate.
		 */
		if (pwd) {
			if (pwd->pw_uid == 0)
				rootlogin = 1;

			if (fflag && (uid == 0 || uid == pwd->pw_uid)) {
				/* already authenticated */
#ifdef KERBEROS5
				if (login_krb5_get_tickets && Fflag)
					k5_read_creds(username);
#endif
				break;
			} else if (pwd->pw_passwd[0] == '\0') {
				/* pretend password okay */
				rval = 0;
				goto ttycheck;
			}
		}

		fflag = 0;

		(void)setpriority(PRIO_PROCESS, 0, -4);

#ifdef SKEY
		if (skey_haskey(username) == 0) {
			static char skprompt[80];
			const char *skinfo = skey_keyinfo(username);
				
			(void)snprintf(skprompt, sizeof(skprompt),
			    "Password [ %s ]:",
			    skinfo ? skinfo : "error getting challenge");
			pwprompt = skprompt;
		} else
#endif
			pwprompt = "Password:";

		p = getpass(pwprompt);

		if (pwd == NULL) {
			rval = 1;
			goto skip;
		}
#ifdef KERBEROS5
		if (login_krb5_get_tickets &&
		    k5login(pwd, instance, localhost, p) == 0) {
			rval = 0;
			got_tickets = 1;
		}
#endif
#if defined(KERBEROS5)
		if (got_tickets)
			goto skip;
#endif
#ifdef SKEY
		if (skey_haskey(username) == 0 &&
		    skey_passcheck(username, p) != -1) {
			rval = 0;
			goto skip;
		}
#endif
		if (!sflag && *pwd->pw_passwd != '\0' &&
		    !strcmp(crypt(p, pwd->pw_passwd), pwd->pw_passwd)) {
			rval = 0;
			require_chpass = 1;
			goto skip;
		}
		rval = 1;

	skip:
		memset(p, 0, strlen(p));

		(void)setpriority(PRIO_PROCESS, 0, 0);

	ttycheck:
		/*
		 * If trying to log in as root without Kerberos,
		 * but with insecure terminal, refuse the login attempt.
		 */
		if (pwd && !rval && rootlogin && !rootterm(tty)) {
			(void)printf("Login incorrect or refused on this "
			    "terminal.\n");
			if (hostname)
				syslog(LOG_NOTICE,
				    "LOGIN %s REFUSED FROM %s ON TTY %s",
				    pwd->pw_name, hostname, tty);
			else
				syslog(LOG_NOTICE,
				    "LOGIN %s REFUSED ON TTY %s",
				     pwd->pw_name, tty);
			continue;
		}

		if (pwd && !rval)
			break;

		(void)printf("Login incorrect or refused on this "
		    "terminal.\n");
		failures++;
		cnt++;
		/*
		 * We allow login_retries tries, but after login_backoff
		 * we start backing off.  These default to 10 and 3
		 * respectively.
		 */
		if (cnt > login_backoff) {
			if (cnt >= login_retries) {
				badlogin(username);
				sleepexit(EXIT_FAILURE);
			}
			sleep((u_int)((cnt - login_backoff) * 5));
		}
	}

	/* committed to login -- turn off timeout */
	(void)alarm((u_int)0);

	endpwent();

	/* if user not super-user, check for disabled logins */
#ifdef LOGIN_CAP
	if (!login_getcapbool(lc, "ignorenologin", rootlogin))
		checknologin(login_getcapstr(lc, "nologin", NULL, NULL));
#else
	if (!rootlogin)
		checknologin(NULL);
#endif

#ifdef LOGIN_CAP
	quietlog = login_getcapbool(lc, "hushlogin", 0);
#else
	quietlog = 0;
#endif
	/* Temporarily give up special privileges so we can change */
	/* into NFS-mounted homes that are exported for non-root */
	/* access and have mode 7x0 */
	saved_uid = geteuid();
	saved_gid = getegid();
	nsaved_gids = getgroups(NGROUPS_MAX, saved_gids);
	
	(void)setegid(pwd->pw_gid);
	initgroups(username, pwd->pw_gid);
	(void)seteuid(pwd->pw_uid);
	
	if (chdir(pwd->pw_dir) < 0) {
#ifdef LOGIN_CAP
		if (login_getcapbool(lc, "requirehome", 0)) {
			(void)printf("Home directory %s required\n",
			    pwd->pw_dir);
			sleepexit(EXIT_FAILURE);
		}
#endif	
		(void)printf("No home directory %s!\n", pwd->pw_dir);
		if (chdir("/") == -1)
			exit(EXIT_FAILURE);
		pwd->pw_dir = "/";
		(void)printf("Logging in with home = \"/\".\n");
	}

	if (!quietlog)
		quietlog = access(_PATH_HUSHLOGIN, F_OK) == 0;

	/* regain special privileges */
	(void)seteuid(saved_uid);
	setgroups(nsaved_gids, saved_gids);
	(void)setegid(saved_gid);

#ifdef LOGIN_CAP
	pw_warntime = login_getcaptime(lc, "password-warn",
		_PASSWORD_WARNDAYS * SECSPERDAY,
		_PASSWORD_WARNDAYS * SECSPERDAY);
#endif

	(void)gettimeofday(&now, (struct timezone *)NULL);
	if (pwd->pw_expire) {
		if (now.tv_sec >= pwd->pw_expire) {
			(void)printf("Sorry -- your account has expired.\n");
			sleepexit(EXIT_FAILURE);
		} else if (pwd->pw_expire - now.tv_sec < pw_warntime && 
		    !quietlog)
			(void)printf("Warning: your account expires on %s",
			    ctime(&pwd->pw_expire));
	}
	if (pwd->pw_change) {
		if (pwd->pw_change == _PASSWORD_CHGNOW)
			need_chpass = 1;
		else if (now.tv_sec >= pwd->pw_change) {
			(void)printf("Sorry -- your password has expired.\n");
			sleepexit(EXIT_FAILURE);
		} else if (pwd->pw_change - now.tv_sec < pw_warntime && 
		    !quietlog)
			(void)printf("Warning: your password expires on %s",
			    ctime(&pwd->pw_change));

	}
	/* Nothing else left to fail -- really log in. */
	update_db(quietlog, rootlogin, fflag);

	(void)chown(ttyn, pwd->pw_uid,
	    (gr = getgrnam(TTYGRPNAME)) ? gr->gr_gid : pwd->pw_gid);

	if (ttyaction(ttyn, "login", pwd->pw_name))
		(void)printf("Warning: ttyaction failed.\n");

#if defined(KERBEROS5)
	/* Fork so that we can call kdestroy */
	if (! login_krb5_retain_ccache && has_ccache)
		dofork();
#endif

	/* Destroy environment unless user has requested its preservation. */
	if (!pflag)
		environ = envinit;

#ifdef LOGIN_CAP
	if (nested == NULL && setusercontext(lc, pwd, pwd->pw_uid,
	    LOGIN_SETLOGIN) != 0) {
		syslog(LOG_ERR, "setusercontext failed");
		exit(EXIT_FAILURE);
	}
	if (setusercontext(lc, pwd, pwd->pw_uid,
	    (LOGIN_SETALL & ~(LOGIN_SETPATH|LOGIN_SETLOGIN))) != 0) {
		syslog(LOG_ERR, "setusercontext failed");
		exit(EXIT_FAILURE);
	}
#else
	(void)setgid(pwd->pw_gid);

	initgroups(username, pwd->pw_gid);
	
#ifndef __minix
	if (nested == NULL && setlogin(pwd->pw_name) < 0)
		syslog(LOG_ERR, "setlogin() failure: %m");
#endif

	/* Discard permissions last so can't get killed and drop core. */
	if (rootlogin)
		(void)setuid(0);
	else
		(void)setuid(pwd->pw_uid);
#endif

	if (*pwd->pw_shell == '\0')
		pwd->pw_shell = _PATH_BSHELL;
#ifdef LOGIN_CAP
	if ((shell = login_getcapstr(lc, "shell", NULL, NULL)) != NULL) {
		if ((shell = strdup(shell)) == NULL) {
			syslog(LOG_ERR, "Cannot alloc mem");
			sleepexit(EXIT_FAILURE);
		}
		pwd->pw_shell = shell;
	}
#endif
	
	(void)setenv("HOME", pwd->pw_dir, 1);
	(void)setenv("SHELL", pwd->pw_shell, 1);
	if (term[0] == '\0') {
		char *tt = (char *)stypeof(tty);
#ifdef LOGIN_CAP
		if (tt == NULL)
			tt = login_getcapstr(lc, "term", NULL, NULL);
#endif
		/* unknown term -> "su" */
		(void)strlcpy(term, tt != NULL ? tt : "su", sizeof(term));
	}
	(void)setenv("TERM", term, 0);
	(void)setenv("LOGNAME", pwd->pw_name, 1);
	(void)setenv("USER", pwd->pw_name, 1);

#ifdef LOGIN_CAP
	setusercontext(lc, pwd, pwd->pw_uid, LOGIN_SETPATH);
#else
	(void)setenv("PATH", _PATH_DEFPATH, 0);
#endif

#ifdef KERBEROS5
	if (krb5tkfile_env)
		(void)setenv("KRB5CCNAME", krb5tkfile_env, 1);
#endif

	if (tty[sizeof("tty")-1] == 'd')
		syslog(LOG_INFO, "DIALUP %s, %s", tty, pwd->pw_name);

	/* If fflag is on, assume caller/authenticator has logged root login. */
	if (rootlogin && fflag == 0) {
		if (hostname)
			syslog(LOG_NOTICE, "ROOT LOGIN (%s) ON %s FROM %s",
			    username, tty, hostname);
		else
			syslog(LOG_NOTICE, "ROOT LOGIN (%s) ON %s",
			    username, tty);
	}

#if defined(KERBEROS5)
	if (KERBEROS_CONFIGURED && !quietlog && notickets == 1)
		(void)printf("Warning: no Kerberos tickets issued.\n");
#endif

	if (!quietlog) {
		char *fname;
#ifdef LOGIN_CAP
		fname = login_getcapstr(lc, "copyright", NULL, NULL);
		if (fname != NULL && access(fname, F_OK) == 0)
			motd(fname);
		else
#endif
			(void)printf("%s", copyrightstr);

#ifdef LOGIN_CAP
		fname = login_getcapstr(lc, "welcome", NULL, NULL);
		if (fname == NULL || access(fname, F_OK) != 0)
#endif
			fname = _PATH_MOTDFILE;
		motd(fname);

		(void)snprintf(tbuf,
		    sizeof(tbuf), "%s/%s", _PATH_MAILDIR, pwd->pw_name);
		if (stat(tbuf, &st) == 0 && st.st_size != 0)
			(void)printf("You have %smail.\n",
			    (st.st_mtime > st.st_atime) ? "new " : "");
	}

#ifdef LOGIN_CAP
	login_close(lc);
#endif

	(void)signal(SIGALRM, SIG_DFL);
	(void)signal(SIGQUIT, SIG_DFL);
	(void)signal(SIGINT, SIG_DFL);
	(void)signal(SIGTSTP, SIG_IGN);

	tbuf[0] = '-';
	(void)strlcpy(tbuf + 1, (p = strrchr(pwd->pw_shell, '/')) ?
	    p + 1 : pwd->pw_shell, sizeof(tbuf) - 1);

	/* Wait to change password until we're unprivileged */
	if (need_chpass) {
		if (!require_chpass)
			(void)printf(
"Warning: your password has expired. Please change it as soon as possible.\n");
		else {
			int	status;

			(void)printf(
		    "Your password has expired. Please choose a new one.\n");
			switch (fork()) {
			case -1:
				warn("fork");
				sleepexit(EXIT_FAILURE);
			case 0:
				execl(_PATH_BINPASSWD, "passwd", NULL);
				_exit(EXIT_FAILURE);
			default:
				if (wait(&status) == -1 ||
				    WEXITSTATUS(status))
					sleepexit(EXIT_FAILURE);
			}
		}
	}

#ifdef KERBEROS5
	if (login_krb5_get_tickets)
		k5_write_creds();
#endif
	execlp(pwd->pw_shell, tbuf, NULL);
	err(EXIT_FAILURE, "%s", pwd->pw_shell);
}

#if defined(KERBEROS5)
/*
 * This routine handles cleanup stuff, and the like.
 * It exists only in the child process.
 */
static void
dofork(void)
{
	pid_t child, wchild;

	switch (child = fork()) {
	case 0:
		return; /* Child process */
	case -1:
		err(EXIT_FAILURE, "Can't fork");
		/*NOTREACHED*/
	default:
		break;
	}

	/*
	 * Setup stuff?  This would be things we could do in parallel
	 * with login
	 */
	if (chdir("/") == -1)	/* Let's not keep the fs busy... */
		err(EXIT_FAILURE, "Can't chdir to `/'");

	/* If we're the parent, watch the child until it dies */
	while ((wchild = wait(NULL)) != child)
		if (wchild == -1)
			err(EXIT_FAILURE, "Can't wait");

	/* Cleanup stuff */
	/* Run kdestroy to destroy tickets */
	if (login_krb5_get_tickets)
		k5destroy();

	/* Leave */
	exit(EXIT_SUCCESS);
}
#endif

static void
checknologin(char *fname)
{
	int fd, nchars;
	char tbuf[8192];

	if ((fd = open(fname ? fname : _PATH_NOLOGIN, O_RDONLY, 0)) >= 0) {
		while ((nchars = read(fd, tbuf, sizeof(tbuf))) > 0)
			(void)write(fileno(stdout), tbuf, nchars);
		sleepexit(EXIT_SUCCESS);
	}
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "Usage: %s [-Ffps] [-a address] [-h hostname] [username]\n",
	    getprogname());
	exit(EXIT_FAILURE);
}

/*	$NetBSD: su_pam.c,v 1.20 2015/08/09 09:39:21 shm Exp $	*/

/*
 * Copyright (c) 1988 The Regents of the University of California.
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
__COPYRIGHT("@(#) Copyright (c) 1988\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)su.c	8.3 (Berkeley) 4/2/94";*/
#else
__RCSID("$NetBSD: su_pam.c,v 1.20 2015/08/09 09:39:21 shm Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>
#include <util.h>
#include <login_cap.h>

#include <security/pam_appl.h>
#include <security/openpam.h>   /* for openpam_ttyconv() */

#ifdef ALLOW_GROUP_CHANGE
#include "grutil.h"
#endif
#include "suutil.h"

static const struct pam_conv pamc = { &openpam_ttyconv, NULL };

#define	ARGSTRX	"-dflm"

#ifdef LOGIN_CAP
#define ARGSTR	ARGSTRX "c:"
#else
#define ARGSTR ARGSTRX
#endif

static void logit(const char *, ...) __printflike(1, 2);

static const char *
safe_pam_strerror(pam_handle_t *pamh, int pam_err) {
	const char *msg;

	if ((msg = pam_strerror(pamh, pam_err)) != NULL)
		return msg;

	static char buf[1024];
	snprintf(buf, sizeof(buf), "Unknown pam error %d", pam_err);
	return buf;
}

int
main(int argc, char **argv)
{
	extern char **environ;
	struct passwd *pwd, pwres;
	char *p;
	uid_t ruid;
	int asme, ch, asthem, fastlogin, prio, gohome;
	u_int setwhat;
	enum { UNSET, YES, NO } iscsh = UNSET;
	const char *user, *shell, *avshell;
	char *username, *class;
	char **np;
	char shellbuf[MAXPATHLEN], avshellbuf[MAXPATHLEN];
	int pam_err;
	char hostname[MAXHOSTNAMELEN];
	char *tty;
	const char *func;
	const void *newuser;
	login_cap_t *lc;
	pam_handle_t *pamh = NULL;
	char pwbuf[1024];
#ifdef PAM_DEBUG
	extern int _openpam_debug;

	_openpam_debug = 1;
#endif
#ifdef ALLOW_GROUP_CHANGE
	char *gname;
#endif

	(void)setprogname(argv[0]);
	asme = asthem = fastlogin = 0;
	gohome = 1;
	shell = class = NULL;
	while ((ch = getopt(argc, argv, ARGSTR)) != -1)
		switch((char)ch) {
		case 'c':
			class = optarg;
			break;
		case 'd':
			asme = 0;
			asthem = 1;
			gohome = 0;
			break;
		case 'f':
			fastlogin = 1;
			break;
		case '-':
		case 'l':
			asme = 0;
			asthem = 1;
			break;
		case 'm':
			asme = 1;
			asthem = 0;
			break;
		case '?':
		default:
			(void)fprintf(stderr,
#ifdef ALLOW_GROUP_CHANGE
			    "Usage: %s [%s] [login[:group] [shell arguments]]\n",
#else
			    "Usage: %s [%s] [login [shell arguments]]\n",
#endif
			    getprogname(), ARGSTR);
			exit(EXIT_FAILURE);
		}
	argv += optind;

	/* Lower the priority so su runs faster */
	errno = 0;
	prio = getpriority(PRIO_PROCESS, 0);
	if (errno)
		prio = 0;
	if (prio > -2)
		(void)setpriority(PRIO_PROCESS, 0, -2);
	openlog("su", 0, LOG_AUTH);

	/* get current login name and shell */
	ruid = getuid();
	username = getlogin();
	if (username == NULL ||
	    getpwnam_r(username, &pwres, pwbuf, sizeof(pwbuf), &pwd) != 0 ||
	    pwd == NULL || pwd->pw_uid != ruid) {
		if (getpwuid_r(ruid, &pwres, pwbuf, sizeof(pwbuf), &pwd) != 0)
			pwd = NULL;
	}
	if (pwd == NULL)
		errx(EXIT_FAILURE, "who are you?");
	username = estrdup(pwd->pw_name);

	if (asme) {
		if (pwd->pw_shell && *pwd->pw_shell) {
			(void)estrlcpy(shellbuf, pwd->pw_shell, sizeof(shellbuf));
			shell = shellbuf;
		} else {
			shell = _PATH_BSHELL;
			iscsh = NO;
		}
	}
	/* get target login information, default to root */
	user = *argv ? *argv : "root";
	np = *argv ? argv : argv - 1;

#ifdef ALLOW_GROUP_CHANGE
	if ((p = strchr(user, ':')) != NULL) {
		*p = '\0';
		gname = ++p;
	}
	else
		gname = NULL;

#ifdef ALLOW_EMPTY_USER
	if (user[0] == '\0')
		user = username;
#endif
#endif
	if (getpwnam_r(user, &pwres, pwbuf, sizeof(pwbuf), &pwd) != 0 ||
	    pwd == NULL)
		errx(EXIT_FAILURE, "unknown login %s", user);

	/*
	 * PAM initialization
	 */
#define PAM_END(msg) do { func = msg; goto done;} /* NOTREACHED */ while (/*CONSTCOND*/0)

	if ((pam_err = pam_start("su", user, &pamc, &pamh)) != PAM_SUCCESS) {
		if (pamh != NULL)
			PAM_END("pam_start");
		/* Things went really bad... */
		syslog(LOG_ERR, "pam_start failed: %s",
		    safe_pam_strerror(pamh, pam_err));
		errx(EXIT_FAILURE, "pam_start failed");
	}

#define PAM_END_ITEM(item)	PAM_END("pam_set_item(" # item ")")
#define PAM_SET_ITEM(item, var) 					    \
	if ((pam_err = pam_set_item(pamh, (item), (var))) != PAM_SUCCESS)   \
		PAM_END_ITEM(item)

	/*
	 * Fill hostname, username and tty
	 */
	PAM_SET_ITEM(PAM_RUSER, username);
	if (gethostname(hostname, sizeof(hostname)) != -1)
		PAM_SET_ITEM(PAM_RHOST, hostname);

	if ((tty = ttyname(STDERR_FILENO)) != NULL)
		PAM_SET_ITEM(PAM_TTY, tty);

	/*
	 * Authentication
	 */
	if ((pam_err = pam_authenticate(pamh, 0)) != PAM_SUCCESS) {
		syslog(LOG_WARNING, "BAD SU %s to %s%s: %s",
		    username, user, ontty(), safe_pam_strerror(pamh, pam_err));
		(void)pam_end(pamh, pam_err);
		errx(EXIT_FAILURE, "Sorry: %s", safe_pam_strerror(NULL, pam_err));
	}

	/*
	 * Authorization
	 */
	switch(pam_err = pam_acct_mgmt(pamh, 0)) {
	case PAM_NEW_AUTHTOK_REQD:
		pam_err = pam_chauthtok(pamh, PAM_CHANGE_EXPIRED_AUTHTOK);
		if (pam_err != PAM_SUCCESS)
			PAM_END("pam_chauthok");
		break;
	case PAM_SUCCESS:
		break;
	default:
		PAM_END("pam_acct_mgmt");
		break;
	}

	/*
	 * pam_authenticate might have changed the target user.
	 * refresh pwd and user
	 */
	pam_err = pam_get_item(pamh, PAM_USER, &newuser);
	if (pam_err != PAM_SUCCESS) {
		syslog(LOG_WARNING,
		    "pam_get_item(PAM_USER): %s", safe_pam_strerror(pamh, pam_err));
	} else {
		user = (char *)__UNCONST(newuser);
		if (getpwnam_r(user, &pwres, pwbuf, sizeof(pwbuf), &pwd) != 0 ||
		    pwd == NULL) {
			(void)pam_end(pamh, pam_err);
			syslog(LOG_ERR, "unknown login: %s", username);
			errx(EXIT_FAILURE, "unknown login: %s", username);
		}
	}

#define ERRX_PAM_END(args) do {			\
	(void)pam_end(pamh, pam_err);		\
	errx args;				\
} while (/* CONSTCOND */0)

#define ERR_PAM_END(args) do {			\
	(void)pam_end(pamh, pam_err);		\
	err args;				\
} while (/* CONSTCOND */0)

	/* force the usage of specified class */
	if (class) {
		if (ruid)
			ERRX_PAM_END((EXIT_FAILURE, "Only root may use -c"));

		pwd->pw_class = class;
	}

	if ((lc = login_getclass(pwd->pw_class)) == NULL)
		ERRX_PAM_END((EXIT_FAILURE,
		    "Unknown class %s\n", pwd->pw_class));

	if (asme) {
		/* if asme and non-standard target shell, must be root */
		if (chshell(pwd->pw_shell) == 0 && ruid)
			ERRX_PAM_END((EXIT_FAILURE,
			    "permission denied (shell)."));
	} else if (pwd->pw_shell && *pwd->pw_shell) {
		shell = pwd->pw_shell;
		iscsh = UNSET;
	} else {
		shell = _PATH_BSHELL;
		iscsh = NO;
	}

	if ((p = strrchr(shell, '/')) != NULL)
		avshell = p + 1;
	else
		avshell = shell;

	/* if we're forking a csh, we want to slightly muck the args */
	if (iscsh == UNSET)
		iscsh = strstr(avshell, "csh") ? YES : NO;

	/*
	 * Initialize the supplemental groups before pam gets to them,
	 * so that other pam modules get a chance to add more when
	 * we do setcred. Note, we don't relinguish our set-userid yet
	 */
	/* if we aren't changing users, keep the current group members */
	if (ruid != pwd->pw_uid &&
	    setusercontext(lc, pwd, pwd->pw_uid, LOGIN_SETGROUP) == -1)
		ERR_PAM_END((EXIT_FAILURE, "setting user context"));

#ifdef ALLOW_GROUP_CHANGE
	addgroup(lc, gname, pwd, ruid, "Group Password:");
#endif
	if ((pam_err = pam_setcred(pamh, PAM_ESTABLISH_CRED)) != PAM_SUCCESS)
		PAM_END("pam_setcred");

	/*
	 * Manage session.
	 */
	if (asthem) {
		pid_t pid, xpid;
		int status = 1;
		struct sigaction sa, sa_int, sa_pipe, sa_quit;
		int fds[2];

 		if ((pam_err = pam_open_session(pamh, 0)) != PAM_SUCCESS)
			PAM_END("pam_open_session");

		/*
		 * In order to call pam_close_session after the
		 * command terminates, we need to fork.
		 */
		sa.sa_flags = SA_RESTART;
		sa.sa_handler = SIG_IGN;
		(void)sigemptyset(&sa.sa_mask);
		(void)sigaction(SIGINT, &sa, &sa_int);
		(void)sigaction(SIGQUIT, &sa, &sa_quit);
		(void)sigaction(SIGPIPE, &sa, &sa_pipe);
		sa.sa_handler = SIG_DFL;
		(void)sigaction(SIGTSTP, &sa, NULL);
		/*
		 * Use a pipe to guarantee the order of execution of
		 * the parent and the child.
		 */
		if (pipe(fds) == -1) {
			warn("pipe failed");
			goto out;
		}

		switch (pid = fork()) {
		case -1:
			logit("fork failed (%s)", strerror(errno));
			goto out;

		case 0:	/* Child */
			(void)close(fds[1]);
			(void)read(fds[0], &status, 1);
			(void)close(fds[0]);
			(void)sigaction(SIGINT, &sa_int, NULL);
			(void)sigaction(SIGQUIT, &sa_quit, NULL);
			(void)sigaction(SIGPIPE, &sa_pipe, NULL);
			break;

		default:
			sa.sa_handler = SIG_IGN;
			(void)sigaction(SIGTTOU, &sa, NULL);
			(void)close(fds[0]);
			(void)setpgid(pid, pid);
			(void)tcsetpgrp(STDERR_FILENO, pid);
			(void)close(fds[1]);
			(void)sigaction(SIGPIPE, &sa_pipe, NULL);
			/*
			 * Parent: wait for the child to terminate
			 * and call pam_close_session.
			 */
			while ((xpid = waitpid(pid, &status, WUNTRACED))
			    == pid) {
				if (WIFSTOPPED(status)) {
					(void)kill(getpid(), SIGSTOP);
					(void)tcsetpgrp(STDERR_FILENO,
					    getpgid(pid));
					(void)kill(pid, SIGCONT);
					status = 1;
					continue;
				}
				break;
			}

			(void)tcsetpgrp(STDERR_FILENO, getpgid(0));

			if (xpid == -1) {
			    logit("Error waiting for pid %d (%s)", pid,
				strerror(errno));
			} else if (xpid != pid) {
			    /* Can't happen. */
			    logit("Wrong PID: %d != %d", pid, xpid);
			}
out:
			pam_err = pam_setcred(pamh, PAM_DELETE_CRED);
			if (pam_err != PAM_SUCCESS)
				logit("pam_setcred: %s",
				    safe_pam_strerror(pamh, pam_err));
			pam_err = pam_close_session(pamh, 0);
			if (pam_err != PAM_SUCCESS)
				logit("pam_close_session: %s",
				    safe_pam_strerror(pamh, pam_err));
			(void)pam_end(pamh, pam_err);
			exit(WEXITSTATUS(status));
			break;
		}
	}

	/*
	 * The child: starting here, we don't have to care about
	 * handling PAM issues if we exit, the parent will do the
	 * job when we exit.
	 */
#undef PAM_END
#undef ERR_PAM_END
#undef ERRX_PAM_END

	if (!asme) {
		if (asthem) {
			char **pamenv;

			p = getenv("TERM");
			/*
			 * Create an empty environment
			 */
			environ = emalloc(sizeof(char *));
			environ[0] = NULL;

			/*
			 * Add PAM environement, before the LOGIN_CAP stuff:
			 * if the login class is unspecified, we'll get the
			 * same data from PAM, if -c was used, the specified
			 * class must override PAM.
	 		 */
			if ((pamenv = pam_getenvlist(pamh)) != NULL) {
				char **envitem;

				/*
				 * XXX Here FreeBSD filters out
				 * SHELL, LOGNAME, MAIL, CDPATH, IFS, PATH
				 * how could we get untrusted data here?
				 */
				for (envitem = pamenv; *envitem; envitem++) {
					if (putenv(*envitem) == -1)
						free(*envitem);
				}

				free(pamenv);
			}

			if (setusercontext(lc, pwd, pwd->pw_uid, LOGIN_SETPATH |
				LOGIN_SETENV | LOGIN_SETUMASK) == -1)
				err(EXIT_FAILURE, "setting user context");
			if (p)
				(void)setenv("TERM", p, 1);
		}

		if (asthem || pwd->pw_uid) {
			(void)setenv("LOGNAME", pwd->pw_name, 1);
			(void)setenv("USER", pwd->pw_name, 1);
		}
		(void)setenv("HOME", pwd->pw_dir, 1);
		(void)setenv("SHELL", shell, 1);
	}
	(void)setenv("SU_FROM", username, 1);

	if (iscsh == YES) {
		if (fastlogin)
			*np-- = __UNCONST("-f");
		if (asme)
			*np-- = __UNCONST("-m");
	} else {
		if (fastlogin)
			(void)unsetenv("ENV");
	}

	if (asthem) {
		avshellbuf[0] = '-';
		(void)estrlcpy(avshellbuf + 1, avshell, sizeof(avshellbuf) - 1);
		avshell = avshellbuf;
	} else if (iscsh == YES) {
		/* csh strips the first character... */
		avshellbuf[0] = '_';
		(void)estrlcpy(avshellbuf + 1, avshell, sizeof(avshellbuf) - 1);
		avshell = avshellbuf;
	}
	*np = __UNCONST(avshell);

	if (ruid != 0)
		syslog(LOG_NOTICE, "%s to %s%s",
		    username, pwd->pw_name, ontty());

	/* Raise our priority back to what we had before */
	(void)setpriority(PRIO_PROCESS, 0, prio);

	/*
	 * Set user context, except for umask, and the stuff
	 * we have done before.
	 */
	setwhat = LOGIN_SETALL & ~(LOGIN_SETENV | LOGIN_SETUMASK |
	    LOGIN_SETLOGIN | LOGIN_SETPATH | LOGIN_SETGROUP);

	/*
	 * Don't touch resource/priority settings if -m has been used
	 * or -l and -c hasn't, and we're not su'ing to root.
	 */
	if ((asme || (!asthem && class == NULL)) && pwd->pw_uid)
		setwhat &= ~(LOGIN_SETPRIORITY | LOGIN_SETRESOURCES);

	if (setusercontext(lc, pwd, pwd->pw_uid, setwhat) == -1)
		err(EXIT_FAILURE, "setusercontext");

	if (!asme) {
		if (asthem) {
			if (gohome && chdir(pwd->pw_dir) == -1)
				errx(EXIT_FAILURE, "no directory");
		}
	}

	(void)execv(shell, np);
	err(EXIT_FAILURE, "%s", shell);
done:
	logit("%s: %s", func, safe_pam_strerror(pamh, pam_err));
	(void)pam_end(pamh, pam_err);
	return EXIT_FAILURE;
}

static void
logit(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarnx(fmt, ap);
	va_end(ap);
	va_start(ap, fmt);
	vsyslog(LOG_ERR, fmt, ap);
	va_end(ap);
}

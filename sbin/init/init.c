/*	$NetBSD: init.c,v 1.106 2015/06/16 23:18:55 christos Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Donn Seeley at Berkeley Software Design, Inc.
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
__COPYRIGHT("@(#) Copyright (c) 1991, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)init.c	8.2 (Berkeley) 4/28/95";
#else
__RCSID("$NetBSD: init.c,v 1.106 2015/06/16 23:18:55 christos Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <machine/cpu.h>

#include <db.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <ttyent.h>
#include <unistd.h>
#include <util.h>
#include <paths.h>
#include <err.h>
#ifdef SUPPORT_UTMP
#include <utmp.h>
#endif
#ifdef SUPPORT_UTMPX
#include <utmpx.h>
#endif

#include <stdarg.h>

#ifdef SECURE
#include <pwd.h>
#endif

#include "pathnames.h"

#define XSTR(x) #x
#define STR(x) XSTR(x)

/*
 * Sleep times; used to prevent thrashing.
 */
#define	GETTY_SPACING		 5	/* N secs minimum getty spacing */
#define	GETTY_SLEEP		30	/* sleep N secs after spacing problem */
#define	WINDOW_WAIT		 3	/* wait N secs after starting window */
#define	STALL_TIMEOUT		30	/* wait N secs after warning */
#define	DEATH_WATCH		10	/* wait N secs for procs to die */

static const struct timespec dtrtime = {.tv_sec = 0, .tv_nsec = 250000};

#if defined(RESCUEDIR)
#define	INIT_BSHELL	RESCUEDIR "/sh"
#define	INIT_MOUNT_MFS	RESCUEDIR "/mount_mfs"
#define	INIT_PATH	RESCUEDIR ":" _PATH_STDPATH
#else
#define	INIT_BSHELL	_PATH_BSHELL
#define	INIT_MOUNT_MFS	"/sbin/mount_mfs"
#define	INIT_PATH	_PATH_STDPATH
#endif

static void handle(sig_t, ...);
static void delset(sigset_t *, ...);

static void stall(const char *, ...) __printflike(1, 2);
static void warning(const char *, ...) __printflike(1, 2);
static void emergency(const char *, ...) __printflike(1, 2);
__dead static void disaster(int);

#if defined(__minix)
static void minixreboot(int);
static void minixpowerdown(int);
#else
static void badsys(int);
#endif /* defined(__minix) */

/*
 * We really need a recursive typedef...
 * The following at least guarantees that the return type of (*state_t)()
 * is sufficiently wide to hold a function pointer.
 */
typedef long (*state_func_t)(void);
typedef state_func_t (*state_t)(void);

#define	DEATH		'd'
#define	SINGLE_USER	's'
#define	RUNCOM		'r'
#define	READ_TTYS	't'
#define	MULTI_USER	'm'
#define	CLEAN_TTYS	'T'
#define	CATATONIA	'c'

static state_func_t single_user(void);
#ifndef LETS_GET_SMALL
static state_func_t runcom(void);
static state_func_t read_ttys(void);
static state_func_t multi_user(void);
static state_func_t clean_ttys(void);
static state_func_t catatonia(void);
static state_func_t death(void);
#endif

static enum { AUTOBOOT, FASTBOOT } runcom_mode = AUTOBOOT;

static void transition(state_t);
static void setctty(const char *);

typedef struct init_session {
	int	se_index;		/* index of entry in ttys file */
	pid_t	se_process;		/* controlling process */
	struct timeval	se_started;	/* used to avoid thrashing */
	int	se_flags;		/* status of session */
#define	SE_SHUTDOWN	0x1		/* session won't be restarted */
#define	SE_PRESENT	0x2		/* session is in /etc/ttys */
	char	*se_device;		/* filename of port */
	char	*se_getty;		/* what to run on that port */
	char	**se_getty_argv;	/* pre-parsed argument array */
	char	*se_window;		/* window system (started only once) */
	char	**se_window_argv;	/* pre-parsed argument array */
	struct	init_session *se_prev;
	struct	init_session *se_next;
} session_t;

static void collect_child(pid_t, int);
static int clang;
static void transition_handler(int);
static void alrm_handler(int);
static int has_securelevel(void);
static int securelevel_present;

#ifndef LETS_GET_SMALL
static int do_setttyent(void);
static void start_window_system(session_t *);
static char **construct_argv(char *);
static int setupargv(session_t *, struct ttyent *);
static pid_t start_getty(session_t *);
static void free_session(session_t *);
static session_t *new_session(session_t *, int, struct ttyent *);
static session_t *sessions;
static void setsecuritylevel(int);
static int getsecuritylevel(void);
static int start_session_db(void);
static void add_session(session_t *);
static void del_session(session_t *);
static session_t *find_session(pid_t);
static DB *session_db;
static state_t requested_transition = runcom;

static void clear_session_logs(session_t *, int);
static state_func_t runetcrc(int);
#ifdef SUPPORT_UTMPX
static struct timeval boot_time;
static state_t current_state = death;
static void session_utmpx(const session_t *, int);
static void make_utmpx(const char *, const char *, int, pid_t,
    const struct timeval *, int);
static char get_runlevel(const state_t);
static void utmpx_set_runlevel(char, char);
#endif

#ifdef CHROOT
static int did_multiuser_chroot = 0;
static char rootdir[PATH_MAX];
static int shouldchroot(void);
static int createsysctlnode(void);
#endif /* CHROOT */

#else /* LETS_GET_SMALL */
static state_t requested_transition = single_user;
#endif /* !LETS_GET_SMALL */

#ifdef MFS_DEV_IF_NO_CONSOLE

static int mfs_dev(void);

#endif

/*
 * The mother of all processes.
 */
int
main(int argc, char **argv)
{
	struct sigaction sa;
	sigset_t mask;
#ifndef LETS_GET_SMALL
	int c;

#ifdef SUPPORT_UTMPX
	(void)gettimeofday(&boot_time, NULL);
#endif /* SUPPORT_UTMPX */

	/* Dispose of random users. */
	if (getuid() != 0) {
		errno = EPERM;
		err(1, NULL);
	}

	/* System V users like to reexec init. */
	if (getpid() != 1)
		errx(1, "already running");
#endif

	/*
	 * Create an initial session.
	 */
	if (setsid() < 0)
		warn("initial setsid() failed");

	/*
	 * Establish an initial user so that programs running
	 * single user do not freak out and die (like passwd).
	 */
#if !defined(__minix)
	if (setlogin("root") < 0)
		warn("setlogin() failed");
#endif /* !defined(__minix) */


#ifdef MFS_DEV_IF_NO_CONSOLE
	if (mfs_dev() == -1)
		requested_transition = single_user;
#endif

#ifndef LETS_GET_SMALL
	/*
	 * Note that this does NOT open a file...
	 * Does 'init' deserve its own facility number?
	 */
	openlog("init", LOG_CONS, LOG_AUTH);
#endif /* LETS_GET_SMALL */


#ifndef LETS_GET_SMALL
	/*
	 * This code assumes that we always get arguments through flags,
	 * never through bits set in some random machine register.
	 */
	while ((c = getopt(argc, argv, "sf")) != -1)
		switch (c) {
		case 's':
			requested_transition = single_user;
			break;
		case 'f':
			runcom_mode = FASTBOOT;
			break;
		default:
			warning("unrecognized flag `%c'", c);
			break;
		}

	if (optind != argc)
		warning("ignoring excess arguments");
#else /* LETS_GET_SMALL */
	requested_transition = single_user;
#endif /* LETS_GET_SMALL */

	/*
	 * We catch or block signals rather than ignore them,
	 * so that they get reset on exec.
	 */
#if !defined(__minix)
	handle(badsys, SIGSYS, 0);
	handle(disaster, SIGABRT, SIGFPE, SIGILL, SIGSEGV,
	       SIGBUS, SIGXCPU, SIGXFSZ, 0);
#else
	handle(minixreboot, SIGABRT, 0);
	handle(minixpowerdown, SIGUSR1, 0);
	handle(disaster, SIGFPE, SIGILL, SIGSEGV, SIGBUS, 0);
#endif /* !defined(__minix) */
	handle(transition_handler, SIGHUP, SIGTERM, SIGTSTP, 0);
	handle(alrm_handler, SIGALRM, 0);
	(void)sigfillset(&mask);
#if !defined(__minix)
	delset(&mask, SIGABRT, SIGFPE, SIGILL, SIGSEGV, SIGBUS, SIGSYS,
	    SIGXCPU, SIGXFSZ, SIGHUP, SIGTERM, SIGTSTP, SIGALRM, 0);
#else
	delset(&mask, SIGABRT, SIGFPE, SIGILL, SIGSEGV, SIGBUS,
	    SIGHUP, SIGTERM, SIGTSTP, SIGALRM, 0);
#endif /* !defined(__minix) */
	(void)sigprocmask(SIG_SETMASK, &mask, NULL);
	(void)sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = SIG_IGN;
	(void)sigaction(SIGTTIN, &sa, NULL);
	(void)sigaction(SIGTTOU, &sa, NULL);

	/*
	 * Paranoia.
	 */
	(void)close(0);
	(void)close(1);
	(void)close(2);

#if !defined(LETS_GET_SMALL) && defined(CHROOT)
	/* Create "init.root" sysctl node. */
	(void)createsysctlnode();
#endif /* !LETS_GET_SMALL && CHROOT*/

	/*
	 * Securelevel might not be supported by the kernel. Query for it, and
	 * set a variable indicating whether we should attempt anything with it
	 * or not.
	 */
	securelevel_present = has_securelevel();

	/*
	 * Start the state machine.
	 */
	transition(requested_transition);

	/*
	 * Should never reach here.
	 */
	return 1;
}

/*
 * Associate a function with a signal handler.
 */
static void
handle(sig_t handler, ...)
{
	int sig;
	struct sigaction sa;
	sigset_t mask_everything;
	va_list ap;

	va_start(ap, handler);

	sa.sa_handler = handler;
	(void)sigfillset(&mask_everything);

	while ((sig = va_arg(ap, int)) != 0) {
		sa.sa_mask = mask_everything;
		/* XXX SA_RESTART? */
		sa.sa_flags = sig == SIGCHLD ? SA_NOCLDSTOP : 0;
		(void)sigaction(sig, &sa, NULL);
	}
	va_end(ap);
}

/*
 * Delete a set of signals from a mask.
 */
static void
delset(sigset_t *maskp, ...)
{
	int sig;
	va_list ap;

	va_start(ap, maskp);

	while ((sig = va_arg(ap, int)) != 0)
		(void)sigdelset(maskp, sig);
	va_end(ap);
}

#if 0	/* Enable to get error messages from init ! */
#define vsyslog(level, fmt, ap) print_console(level, fmt, ap)
#define closelog()

static void
print_console(int level, const char *message, va_list ap)
{
	/*
	 * XXX: syslog seems to just plain not work in console-only
	 * XXX: situation... that should be fixed.  Let's leave this
	 * XXX: note + code here in case someone gets in trouble and
	 * XXX: wants to debug. -- Jachym Holecek <freza@liberouter.org>
	 */
	char errbuf[1024];
	int fd, len;

	/* We can't do anything on errors, anyway... */
	fd = open(_PATH_CONSOLE, O_WRONLY);
	if (fd == -1)
		return ;

	/* %m will get lost... */
	len = vsnprintf(errbuf, sizeof(errbuf), message, ap);
	(void)write(fd, (void *)errbuf, len);
	(void)close(fd);
}
#endif

/*
 * Log a message and sleep for a while (to give someone an opportunity
 * to read it and to save log or hardcopy output if the problem is chronic).
 * NB: should send a message to the session logger to avoid blocking.
 */
static void
stall(const char *message, ...)
{
	va_list ap;

	va_start(ap, message);
	vsyslog(LOG_ALERT, message, ap);
	va_end(ap);
	closelog();
	(void)sleep(STALL_TIMEOUT);
}

/*
 * Like stall(), but doesn't sleep.
 * If cpp had variadic macros, the two functions could be #defines for another.
 * NB: should send a message to the session logger to avoid blocking.
 */
static void
warning(const char *message, ...)
{
	va_list ap;

	va_start(ap, message);
	vsyslog(LOG_ALERT, message, ap);
	va_end(ap);
	closelog();
}

/*
 * Log an emergency message.
 * NB: should send a message to the session logger to avoid blocking.
 */
static void
emergency(const char *message, ...)
{
	va_list ap;

	va_start(ap, message);
	vsyslog(LOG_EMERG, message, ap);
	va_end(ap);
	closelog();
}

#if !defined(__minix)
/*
 * Catch a SIGSYS signal.
 *
 * These may arise if a system does not support sysctl.
 * We tolerate up to 25 of these, then throw in the towel.
 */
static void
badsys(int sig)
{
	static int badcount = 0;

	if (badcount++ < 25)
		return;
	disaster(sig);
}
#endif /* !defined(__minix) */

/*
 * Catch an unexpected signal.
 */
static void
disaster(int sig)
{

	emergency("fatal signal: %s", strsignal(sig));
	(void)sleep(STALL_TIMEOUT);
	_exit(sig);		/* reboot */
}

#if defined(__minix)
/*
 * controlled reboot - minix tradition, SIGABRT by tty
 */
static void
minixreboot(int sig)
{
	if(fork() == 0) {
		(void)execl("/sbin/shutdown",
			"shutdown", "-r", "now", "CTRL-ALT_DEL", NULL);
		_exit(1);
	}
}

/*
 * controlled powerdown
 */
static void
minixpowerdown(int sig)
{
	if(fork() == 0) {
		(void)execl("/sbin/shutdown",
			"shutdown", "-p", "now", "CTRL-ALT_DEL", NULL);
		_exit(1);
	}
}
#endif /* defined(__minix) */

/*
 * Check if securelevel is present.
 */
static int
has_securelevel(void)
{
#ifdef KERN_SECURELVL
	int name[2], curlevel;
	size_t len;

	name[0] = CTL_KERN;
	name[1] = KERN_SECURELVL;
	len = sizeof curlevel;
	if (sysctl(name, 2, &curlevel, &len, NULL, 0) == -1) {
		/* If it doesn't exist, it's okay. */
		if (errno == ENOENT) 
			return 0;
	}
	return 1;
#else
	return 0;
#endif
}

/*
 * Get the security level of the kernel.
 */
static int
getsecuritylevel(void)
{
#ifdef KERN_SECURELVL
	int name[2], curlevel;
	size_t len;

	if (!securelevel_present)
		return -1;

	name[0] = CTL_KERN;
	name[1] = KERN_SECURELVL;
	len = sizeof curlevel;
	if (sysctl(name, 2, &curlevel, &len, NULL, 0) == -1) {
		emergency("cannot get kernel security level: %m");
		return -1;
	}
	return curlevel;
#else
	return -1;
#endif
}

/*
 * Set the security level of the kernel.
 */
static void
setsecuritylevel(int newlevel)
{
#ifdef KERN_SECURELVL
	int name[2], curlevel;

	if (!securelevel_present)
		return;

	curlevel = getsecuritylevel();
	if (newlevel == curlevel)
		return;
	name[0] = CTL_KERN;
	name[1] = KERN_SECURELVL;
	if (sysctl(name, 2, NULL, NULL, &newlevel, sizeof newlevel) == -1) {
		emergency("cannot change kernel security level from"
		    " %d to %d: %m", curlevel, newlevel);
		return;
	}
#ifdef SECURE
	warning("kernel security level changed from %d to %d",
	    curlevel, newlevel);
#endif
#endif
}

/*
 * Change states in the finite state machine.
 * The initial state is passed as an argument.
 */
static void
transition(state_t s)
{

	if (s == NULL)
		return;
	for (;;) {
#ifdef SUPPORT_UTMPX
#ifndef LETS_GET_SMALL
		utmpx_set_runlevel(get_runlevel(current_state),
		    get_runlevel(s));
		current_state = s;
#endif
#endif
		s = (state_t)(*s)();
	}
}

#ifndef LETS_GET_SMALL
/*
 * Close out the accounting files for a login session.
 * NB: should send a message to the session logger to avoid blocking.
 */
static void
clear_session_logs(session_t *sp, int status)
{
#if defined(SUPPORT_UTMP) || defined(SUPPORT_UTMPX)
	char *line = sp->se_device + sizeof(_PATH_DEV) - 1;
#endif

#ifdef SUPPORT_UTMPX
	if (logoutx(line, status, DEAD_PROCESS))
		logwtmpx(line, "", "", status, DEAD_PROCESS);
#endif
#ifdef SUPPORT_UTMP
	if (logout(line))
		logwtmp(line, "", "");
#endif
}
#endif

/*
 * Start a session and allocate a controlling terminal.
 * Only called by children of init after forking.
 */
static void
setctty(const char *name)
{
	int fd;

#if !defined(__minix)
	(void)revoke(name);
#else
	if (setsid() < 0)
		warn("child setsid() failed");
#endif /* !defined(__minix) */
	(void)nanosleep(&dtrtime, NULL);	/* leave DTR low for a bit */
	if ((fd = open(name, O_RDWR)) == -1) {
		stall("can't open %s: %m", name);
		_exit(1);
	}
	if (login_tty(fd) == -1) {
		stall("can't get %s for controlling terminal: %m", name);
		_exit(2);
	}
}

/*
 * Bring the system up single user.
 */
static state_func_t
single_user(void)
{
	pid_t pid, wpid;
	int status;
	int from_securitylevel;
	sigset_t mask;
	struct sigaction sa, satstp, sahup;
#ifdef ALTSHELL
	const char *shell = INIT_BSHELL;
#endif
	const char *argv[2];
#ifdef SECURE
	struct ttyent *typ;
	struct passwd *pp;
	char *clear, *password;
#endif
#ifdef ALTSHELL
	char altshell[128];
#endif /* ALTSHELL */

#if !defined(LETS_GET_SMALL) && defined(CHROOT)
	/* Clear previous idea, just in case. */
	did_multiuser_chroot = 0;
#endif /* !LETS_GET_SMALL && CHROOT */

	/*
	 * If the kernel is in secure mode, downgrade it to insecure mode.
	 */
	from_securitylevel = getsecuritylevel();
	if (from_securitylevel > 0)
		setsecuritylevel(0);

	(void)sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = SIG_IGN;
	(void)sigaction(SIGTSTP, &sa, &satstp);
	(void)sigaction(SIGHUP, &sa, &sahup);
	if ((pid = fork()) == 0) {
		/*
		 * Start the single user session.
		 */
		if (access(_PATH_CONSTTY, F_OK) == 0)
			setctty(_PATH_CONSTTY);
		else
			setctty(_PATH_CONSOLE);

#ifdef SECURE
		/*
		 * Check the root password.
		 * We don't care if the console is 'on' by default;
		 * it's the only tty that can be 'off' and 'secure'.
		 */
		typ = getttynam("console");
		pp = getpwnam("root");
		if (typ && (from_securitylevel >=2 || (typ->ty_status
		    & TTY_SECURE) == 0) && pp && *pp->pw_passwd != '\0') {
			(void)fprintf(stderr,
			    "Enter root password, or ^D to go multi-user\n");
			for (;;) {
				clear = getpass("Password:");
				if (clear == 0 || *clear == '\0')
					_exit(0);
				password = crypt(clear, pp->pw_passwd);
				(void)memset(clear, 0, _PASSWORD_LEN);
				if (strcmp(password, pp->pw_passwd) == 0)
					break;
				warning("single-user login failed");
			}
		}
		(void)endttyent();
		endpwent();
#endif /* SECURE */

#ifdef ALTSHELL
		(void)fprintf(stderr,
		    "Enter pathname of shell or RETURN for %s: ", shell);
		if (fgets(altshell, sizeof(altshell), stdin) == NULL) {
			altshell[0] = '\0';
		} else {
			/* nuke \n */
			char *p;

			if ((p = strchr(altshell, '\n')) != NULL)
				*p = '\0';
		}

		if (altshell[0])
			shell = altshell;
#endif /* ALTSHELL */

		/*
		 * Unblock signals.
		 * We catch all the interesting ones,
		 * and those are reset to SIG_DFL on exec.
		 */
		(void)sigemptyset(&mask);
		(void)sigprocmask(SIG_SETMASK, &mask, NULL);

		/*
		 * Fire off a shell.
		 * If the default one doesn't work, try the Bourne shell.
		 */
		argv[0] = "-sh";
		argv[1] = 0;
		(void)setenv("PATH", INIT_PATH, 1);
#ifdef ALTSHELL
		if (altshell[0])
			argv[0] = altshell;
		(void)execv(shell, __UNCONST(argv));
		emergency("can't exec `%s' for single user: %m", shell);
		argv[0] = "-sh";
#endif /* ALTSHELL */
		(void)execv(INIT_BSHELL, __UNCONST(argv));
		emergency("can't exec `%s' for single user: %m", INIT_BSHELL);
		(void)sleep(STALL_TIMEOUT);
		_exit(3);
	}

	if (pid == -1) {
		/*
		 * We are seriously hosed.  Do our best.
		 */
		emergency("can't fork single-user shell: %m, trying again");
		while (waitpid(-1, NULL, WNOHANG) > 0)
			continue;
		(void)sigaction(SIGTSTP, &satstp, NULL);
		(void)sigaction(SIGHUP, &sahup, NULL);
		return (state_func_t)single_user;
	}

	requested_transition = 0;
	do {
		if ((wpid = waitpid(-1, &status, WUNTRACED)) != -1)
			collect_child(wpid, status);
		if (wpid == -1) {
			if (errno == EINTR)
				continue;
			warning("wait for single-user shell failed: %m; "
			    "restarting");
			return (state_func_t)single_user;
		}
		if (wpid == pid && WIFSTOPPED(status)) {
			warning("shell stopped, restarting");
			(void)kill(pid, SIGCONT);
			wpid = -1;
		}
	} while (wpid != pid && !requested_transition);

	if (requested_transition) {
		(void)sigaction(SIGTSTP, &satstp, NULL);
		(void)sigaction(SIGHUP, &sahup, NULL);
		return (state_func_t)requested_transition;
	}

	if (WIFSIGNALED(status)) {
		if (WTERMSIG(status) == SIGKILL) {
			/* executed /sbin/reboot; wait for the end quietly */
			sigset_t s;
	
			(void)sigfillset(&s);
			for (;;)
				(void)sigsuspend(&s);
		} else {	
			warning("single user shell terminated (%x), restarting",
				status);
			(void)sigaction(SIGTSTP, &satstp, NULL);
			(void)sigaction(SIGHUP, &sahup, NULL);
			return (state_func_t)single_user;
		}
	}

	runcom_mode = FASTBOOT;
	(void)sigaction(SIGTSTP, &satstp, NULL);
	(void)sigaction(SIGHUP, &sahup, NULL);
#ifndef LETS_GET_SMALL
	return (state_func_t)runcom;
#else /* LETS_GET_SMALL */
	return (state_func_t)single_user;
#endif /* LETS_GET_SMALL */
}

#ifndef LETS_GET_SMALL

/* ARGSUSED */
static state_func_t
runetcrc(int trychroot)
{
	pid_t pid, wpid;
	int status;
	const char *argv[4];
	struct sigaction sa;

	switch ((pid = fork())) {
	case 0:
		(void)sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sa.sa_handler = SIG_IGN;
		(void)sigaction(SIGTSTP, &sa, NULL);
		(void)sigaction(SIGHUP, &sa, NULL);

		setctty(_PATH_CONSOLE);

		argv[0] = "sh";
		argv[1] = _PATH_RUNCOM;
		argv[2] = (runcom_mode == AUTOBOOT ? "autoboot" : 0);
		argv[3] = 0;

		(void)sigprocmask(SIG_SETMASK, &sa.sa_mask, NULL);

#ifdef CHROOT
		if (trychroot)
			if (chroot(rootdir) != 0) {
				warning("failed to chroot to `%s': %m",
				    rootdir);
				_exit(4); 	/* force single user mode */
			}
#endif /* CHROOT */

		(void)execv(INIT_BSHELL, __UNCONST(argv));
		stall("can't exec `%s' for `%s': %m", INIT_BSHELL, _PATH_RUNCOM);
		_exit(5);	/* force single user mode */
		/*NOTREACHED*/
	case -1:
		emergency("can't fork for `%s' on `%s': %m", INIT_BSHELL,
		    _PATH_RUNCOM);
		while (waitpid(-1, NULL, WNOHANG) > 0)
			continue;
		(void)sleep(STALL_TIMEOUT);
		return (state_func_t)single_user;
	default:
		break;
	}

	/*
	 * Copied from single_user().  This is a bit paranoid.
	 */
	do {
		if ((wpid = waitpid(-1, &status, WUNTRACED)) != -1)
			collect_child(wpid, status);
		if (wpid == -1) {
			if (errno == EINTR)
				continue;
			warning("wait for `%s' on `%s' failed: %m; going to "
			    "single user mode", INIT_BSHELL, _PATH_RUNCOM);
			return (state_func_t)single_user;
		}
		if (wpid == pid && WIFSTOPPED(status)) {
			warning("`%s' on `%s' stopped, restarting",
			    INIT_BSHELL, _PATH_RUNCOM);
			(void)kill(pid, SIGCONT);
			wpid = -1;
		}
	} while (wpid != pid);

	if (WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM &&
	    requested_transition == catatonia) {
		/* /etc/rc executed /sbin/reboot; wait for the end quietly */
		sigset_t s;

		(void)sigfillset(&s);
		for (;;)
			(void)sigsuspend(&s);
	}

	if (!WIFEXITED(status)) {
		warning("`%s' on `%s' terminated abnormally, going to "
		    "single user mode", INIT_BSHELL, _PATH_RUNCOM);
		return (state_func_t)single_user;
	}

	if (WEXITSTATUS(status))
		return (state_func_t)single_user;

	return (state_func_t)read_ttys;
}

/*
 * Run the system startup script.
 */
static state_func_t
runcom(void)
{
	state_func_t next_step;

	/* Run /etc/rc and choose next state depending on the result. */
	next_step = runetcrc(0);
	if (next_step != (state_func_t)read_ttys)
		return (state_func_t)next_step;

#ifdef CHROOT
	/*
	 * If init.root sysctl does not point to "/", we'll chroot and run
	 * The Real(tm) /etc/rc now.  Global variable rootdir will tell us
	 * where to go.
	 */
	if (shouldchroot()) {
		next_step = runetcrc(1);
		if (next_step != (state_func_t)read_ttys)
			return (state_func_t)next_step;

		did_multiuser_chroot = 1;
	} else {
		did_multiuser_chroot = 0;
	}
#endif /* CHROOT */

	/*
	 * Regardless of whether in chroot or not, we booted successfuly.
	 * It's time to spawn gettys (ie. next_step's value at this point).
	 */
	runcom_mode = AUTOBOOT;		/* the default */
	/* NB: should send a message to the session logger to avoid blocking. */
#ifdef SUPPORT_UTMPX
	logwtmpx("~", "reboot", "", 0, INIT_PROCESS);
#endif
#ifdef SUPPORT_UTMP
	logwtmp("~", "reboot", "");
#endif
	return (state_func_t)read_ttys;
}

/*
 * Open the session database.
 *
 * NB: We could pass in the size here; is it necessary?
 */
static int
start_session_db(void)
{

	if (session_db && (*session_db->close)(session_db))
		emergency("session database close: %m");
	if ((session_db = dbopen(NULL, O_RDWR, 0, DB_HASH, NULL)) == 0) {
		emergency("session database open: %m");
		return 1;
	}
	return 0;
		
}

/*
 * Add a new login session.
 */
static void
add_session(session_t *sp)
{
	DBT key;
	DBT data;

	if (session_db == NULL)
		return;

	key.data = &sp->se_process;
	key.size = sizeof sp->se_process;
	data.data = &sp;
	data.size = sizeof sp;

	if ((*session_db->put)(session_db, &key, &data, 0))
		emergency("insert %d: %m", sp->se_process);
#ifdef SUPPORT_UTMPX
	session_utmpx(sp, 1);
#endif
}

/*
 * Delete an old login session.
 */
static void
del_session(session_t *sp)
{
	DBT key;

	key.data = &sp->se_process;
	key.size = sizeof sp->se_process;

	if ((*session_db->del)(session_db, &key, 0))
		emergency("delete %d: %m", sp->se_process);
#ifdef SUPPORT_UTMPX
	session_utmpx(sp, 0);
#endif
}

/*
 * Look up a login session by pid.
 */
static session_t *
find_session(pid_t pid)
{
	DBT key;
	DBT data;
	session_t *ret;

	if (session_db == NULL)
		return NULL;

	key.data = &pid;
	key.size = sizeof pid;
	if ((*session_db->get)(session_db, &key, &data, 0) != 0)
		return 0;
	(void)memmove(&ret, data.data, sizeof(ret));
	return ret;
}

/*
 * Construct an argument vector from a command line.
 */
static char **
construct_argv(char *command)
{
	int argc = 0;
	char **argv = malloc(((strlen(command) + 1) / 2 + 1) * sizeof (char *));
	static const char separators[] = " \t";

	if (argv == NULL)
		return NULL;

	if ((argv[argc++] = strtok(command, separators)) == 0) {
		free(argv);
		return NULL;
	}
	while ((argv[argc++] = strtok(NULL, separators)) != NULL)
		continue;
	return argv;
}

/*
 * Deallocate a session descriptor.
 */
static void
free_session(session_t *sp)
{

	free(sp->se_device);
	if (sp->se_getty) {
		free(sp->se_getty);
		free(sp->se_getty_argv);
	}
	if (sp->se_window) {
		free(sp->se_window);
		free(sp->se_window_argv);
	}
	free(sp);
}

/*
 * Allocate a new session descriptor.
 */
static session_t *
new_session(session_t *sprev, int session_index, struct ttyent *typ)
{
	session_t *sp;

	if ((typ->ty_status & TTY_ON) == 0 || typ->ty_name == NULL ||
	    typ->ty_getty == NULL)
		return NULL;

	sp = malloc(sizeof (session_t));
	if (sp == NULL)
		return NULL;
	(void)memset(sp, 0, sizeof *sp);

	sp->se_flags = SE_PRESENT;
	sp->se_index = session_index;

	(void)asprintf(&sp->se_device, "%s%s", _PATH_DEV, typ->ty_name);
	if (!sp->se_device) {
		free(sp);
		return NULL;
	}

	if (setupargv(sp, typ) == 0) {
		free_session(sp);
		return NULL;
	}

	sp->se_next = NULL;
	if (sprev == NULL) {
		sessions = sp;
		sp->se_prev = NULL;
	} else {
		sprev->se_next = sp;
		sp->se_prev = sprev;
	}

	return sp;
}

/*
 * Calculate getty and if useful window argv vectors.
 */
static int
setupargv(session_t *sp, struct ttyent *typ)
{

	if (sp->se_getty) {
		free(sp->se_getty);
		free(sp->se_getty_argv);
	}
	(void)asprintf(&sp->se_getty, "%s %s", typ->ty_getty, typ->ty_name);
	if (!sp->se_getty)
		return 0;
	sp->se_getty_argv = construct_argv(sp->se_getty);
	if (sp->se_getty_argv == NULL) {
		warning("can't parse getty for port `%s'", sp->se_device);
		free(sp->se_getty);
		sp->se_getty = NULL;
		return 0;
	}
	if (typ->ty_window) {
		if (sp->se_window)
			free(sp->se_window);
		sp->se_window = strdup(typ->ty_window);
		sp->se_window_argv = construct_argv(sp->se_window);
		if (sp->se_window_argv == NULL) {
			warning("can't parse window for port `%s'",
			    sp->se_device);
			free(sp->se_window);
			sp->se_window = NULL;
			return 0;
		}
	}
	return 1;
}

/*
 * Walk the list of ttys and create sessions for each active line.
 */
static state_func_t
read_ttys(void)
{
	int session_index = 0;
	session_t *sp, *snext;
	struct ttyent *typ;

#ifdef SUPPORT_UTMPX
	if (sessions == NULL) {
		struct stat st;

		make_utmpx("", BOOT_MSG, BOOT_TIME, 0, &boot_time, 0);

		/*
		 * If wtmpx is not empty, pick the down time from there
		 */
		if (stat(_PATH_WTMPX, &st) != -1 && st.st_size != 0) {
			struct timeval down_time;

			TIMESPEC_TO_TIMEVAL(&down_time, 
			    st.st_atime > st.st_mtime ?
			    &st.st_atimespec : &st.st_mtimespec);
			make_utmpx("", DOWN_MSG, DOWN_TIME, 0, &down_time, 0);
		}
	}
#endif
	/*
	 * Destroy any previous session state.
	 * There shouldn't be any, but just in case...
	 */
	for (sp = sessions; sp; sp = snext) {
#ifndef LETS_GET_SMALL
		if (sp->se_process)
			clear_session_logs(sp, 0);
#endif
		snext = sp->se_next;
		free_session(sp);
	}
	sessions = NULL;

	if (start_session_db()) {
		warning("start_session_db failed, death");
#ifdef CHROOT
		/* If /etc/rc ran in chroot, we want to kill any survivors. */
		if (did_multiuser_chroot)
			return (state_func_t)death;
		else
#endif /* CHROOT */
			return (state_func_t)single_user;
	}

	(void)do_setttyent();

	/*
	 * Allocate a session entry for each active port.
	 * Note that sp starts at 0.
	 */
	while ((typ = getttyent()) != NULL)
		if ((snext = new_session(sp, ++session_index, typ)) != NULL)
			sp = snext;
	(void)endttyent();

	return (state_func_t)multi_user;
}

/*
 * Start a window system running.
 */
static void
start_window_system(session_t *sp)
{
	pid_t pid;
	sigset_t mask;

	if ((pid = fork()) == -1) {
		emergency("can't fork for window system on port `%s': %m",
		    sp->se_device);
		/* hope that getty fails and we can try again */
		return;
	}

	if (pid)
		return;

	(void)sigemptyset(&mask);
	(void)sigprocmask(SIG_SETMASK, &mask, NULL);

	if (setsid() < 0)
		emergency("setsid failed (window): %m");

	(void)execv(sp->se_window_argv[0], sp->se_window_argv);
	stall("can't exec window system `%s' for port `%s': %m",
	    sp->se_window_argv[0], sp->se_device);
	_exit(6);
}

/*
 * Start a login session running.
 */
static pid_t
start_getty(session_t *sp)
{
	pid_t pid;
	sigset_t mask;
	time_t current_time = time(NULL);

	/*
	 * fork(), not vfork() -- we can't afford to block.
	 */
	if ((pid = fork()) == -1) {
		emergency("can't fork for getty on port `%s': %m",
		    sp->se_device);
		return -1;
	}

	if (pid)
		return pid;

#ifdef CHROOT
	/* If /etc/rc did proceed inside chroot, we have to try as well. */
	if (did_multiuser_chroot)
		if (chroot(rootdir) != 0) {
			stall("can't chroot getty `%s' inside `%s': %m",
			    sp->se_getty_argv[0], rootdir);
			_exit(7);
		}
#endif /* CHROOT */

	if (current_time > sp->se_started.tv_sec &&
	    current_time - sp->se_started.tv_sec < GETTY_SPACING) {
		warning("getty repeating too quickly on port `%s', sleeping",
		    sp->se_device);
		(void)sleep(GETTY_SLEEP);
	}

	if (sp->se_window) {
		start_window_system(sp);
		(void)sleep(WINDOW_WAIT);
	}

	(void)sigemptyset(&mask);
	(void)sigprocmask(SIG_SETMASK, &mask, (sigset_t *) 0);

	(void)execv(sp->se_getty_argv[0], sp->se_getty_argv);
	stall("can't exec getty `%s' for port `%s': %m",
	    sp->se_getty_argv[0], sp->se_device);
	_exit(8);
	/*NOTREACHED*/
}
#ifdef SUPPORT_UTMPX
static void
session_utmpx(const session_t *sp, int add)
{
	const char *name = sp->se_getty ? sp->se_getty :
	    (sp->se_window ? sp->se_window : "");
	const char *line = sp->se_device + sizeof(_PATH_DEV) - 1;

	make_utmpx(name, line, add ? LOGIN_PROCESS : DEAD_PROCESS,
	    sp->se_process, &sp->se_started, sp->se_index);
}

static void
make_utmpx(const char *name, const char *line, int type, pid_t pid,
    const struct timeval *tv, int session)
{
	struct utmpx ut;
	const char *eline;

	(void)memset(&ut, 0, sizeof(ut));
	(void)strlcpy(ut.ut_name, name, sizeof(ut.ut_name));
	ut.ut_type = type;
	(void)strlcpy(ut.ut_line, line, sizeof(ut.ut_line));
	ut.ut_pid = pid;
	if (tv)
		ut.ut_tv = *tv;
	else
		(void)gettimeofday(&ut.ut_tv, NULL);
	ut.ut_session = session;

	eline = line + strlen(line);
	if ((size_t)(eline - line) >= sizeof(ut.ut_id))
		line = eline - sizeof(ut.ut_id);
	(void)strncpy(ut.ut_id, line, sizeof(ut.ut_id));

	if (pututxline(&ut) == NULL)
		warning("can't add utmpx record for `%s': %m", ut.ut_line);
	endutxent();
}

static char
get_runlevel(const state_t s)
{
	if (s == (state_t)single_user)
		return SINGLE_USER;
	if (s == (state_t)runcom)
		return RUNCOM;
	if (s == (state_t)read_ttys)
		return READ_TTYS;
	if (s == (state_t)multi_user)
		return MULTI_USER;
	if (s == (state_t)clean_ttys)
		return CLEAN_TTYS;
	if (s == (state_t)catatonia)
		return CATATONIA;
	return DEATH;
}

static void
utmpx_set_runlevel(char old, char new)
{
	struct utmpx ut;

	/*
	 * Don't record any transitions until we did the first transition
	 * to read ttys, which is when we are guaranteed to have a read-write
	 * /var. Perhaps use a different variable for this?
	 */
	if (sessions == NULL)
		return;

	(void)memset(&ut, 0, sizeof(ut));
	(void)snprintf(ut.ut_line, sizeof(ut.ut_line), RUNLVL_MSG, new);
	ut.ut_type = RUN_LVL;
	(void)gettimeofday(&ut.ut_tv, NULL);
	ut.ut_exit.e_exit = old;
	ut.ut_exit.e_termination = new;
	if (pututxline(&ut) == NULL)
		warning("can't add utmpx record for `runlevel': %m");
	endutxent();
}
#endif /* SUPPORT_UTMPX */

#endif /* LETS_GET_SMALL */

/*
 * Collect exit status for a child.
 * If an exiting login, start a new login running.
 */
static void
collect_child(pid_t pid, int status)
{
#ifndef LETS_GET_SMALL
	session_t *sp, *sprev, *snext;

	if (! sessions)
		return;

	if ((sp = find_session(pid)) == NULL)
		return;

	clear_session_logs(sp, status);
	del_session(sp);
	sp->se_process = 0;

	if (sp->se_flags & SE_SHUTDOWN) {
		if ((sprev = sp->se_prev) != NULL)
			sprev->se_next = sp->se_next;
		else
			sessions = sp->se_next;
		if ((snext = sp->se_next) != NULL)
			snext->se_prev = sp->se_prev;
		free_session(sp);
		return;
	}

	if ((pid = start_getty(sp)) == -1) {
		/* serious trouble */
		requested_transition = clean_ttys;
		return;
	}

	sp->se_process = pid;
	(void)gettimeofday(&sp->se_started, NULL);
	add_session(sp);
#endif /* LETS_GET_SMALL */
}

/*
 * Catch a signal and request a state transition.
 */
static void
transition_handler(int sig)
{

	switch (sig) {
#ifndef LETS_GET_SMALL
	case SIGHUP:
		requested_transition = clean_ttys;
		break;
	case SIGTERM:
		requested_transition = death;
		break;
	case SIGTSTP:
		requested_transition = catatonia;
		break;
#endif /* LETS_GET_SMALL */
	default:
		requested_transition = 0;
		break;
	}
}

#ifndef LETS_GET_SMALL
/*
 * Take the system multiuser.
 */
static state_func_t
multi_user(void)
{
	pid_t pid;
	int status;
	session_t *sp;

	requested_transition = 0;

	/*
	 * If the administrator has not set the security level to -1
	 * to indicate that the kernel should not run multiuser in secure
	 * mode, and the run script has not set a higher level of security 
	 * than level 1, then put the kernel into secure mode.
	 */
	if (getsecuritylevel() == 0)
		setsecuritylevel(1);

	for (sp = sessions; sp; sp = sp->se_next) {
		if (sp->se_process)
			continue;
		if ((pid = start_getty(sp)) == -1) {
			/* serious trouble */
			requested_transition = clean_ttys;
			break;
		}
		sp->se_process = pid;
		(void)gettimeofday(&sp->se_started, NULL);
		add_session(sp);
	}

	while (!requested_transition)
		if ((pid = waitpid(-1, &status, 0)) != -1)
			collect_child(pid, status);

	return (state_func_t)requested_transition;
}

/*
 * This is an n-squared algorithm.  We hope it isn't run often...
 */
static state_func_t
clean_ttys(void)
{
	session_t *sp, *sprev;
	struct ttyent *typ;
	int session_index = 0;
	int devlen;

	for (sp = sessions; sp; sp = sp->se_next)
		sp->se_flags &= ~SE_PRESENT;

	(void)do_setttyent();

	devlen = sizeof(_PATH_DEV) - 1;
	while ((typ = getttyent()) != NULL) {
		++session_index;

		for (sprev = 0, sp = sessions; sp; sprev = sp, sp = sp->se_next)
			if (strcmp(typ->ty_name, sp->se_device + devlen) == 0)
				break;

		if (sp) {
			sp->se_flags |= SE_PRESENT;
			if (sp->se_index != session_index) {
				warning("port `%s' changed utmp index from "
				    "%d to %d", sp->se_device, sp->se_index,
				    session_index);
				sp->se_index = session_index;
			}
			if ((typ->ty_status & TTY_ON) == 0 ||
			    typ->ty_getty == 0) {
				sp->se_flags |= SE_SHUTDOWN;
				if (sp->se_process != 0)
					(void)kill(sp->se_process, SIGHUP);
				continue;
			}
			sp->se_flags &= ~SE_SHUTDOWN;
			if (setupargv(sp, typ) == 0) {
				warning("can't parse getty for port `%s'",
				    sp->se_device);
				sp->se_flags |= SE_SHUTDOWN;
				if (sp->se_process != 0)
					(void)kill(sp->se_process, SIGHUP);
			}
			continue;
		}

		(void)new_session(sprev, session_index, typ);
	}

	(void)endttyent();

	for (sp = sessions; sp; sp = sp->se_next)
		if ((sp->se_flags & SE_PRESENT) == 0) {
			sp->se_flags |= SE_SHUTDOWN;
			if (sp->se_process != 0)
				(void)kill(sp->se_process, SIGHUP);
		}

	return (state_func_t)multi_user;
}

/*
 * Block further logins.
 */
static state_func_t
catatonia(void)
{
	session_t *sp;

	for (sp = sessions; sp; sp = sp->se_next)
		sp->se_flags |= SE_SHUTDOWN;

	return (state_func_t)multi_user;
}
#endif /* LETS_GET_SMALL */

/*
 * Note SIGALRM.
 */
static void
/*ARGSUSED*/
alrm_handler(int sig)
{

	clang = 1;
}

#ifndef LETS_GET_SMALL
/*
 * Bring the system down to single user.
 */
static state_func_t
death(void)
{
	session_t *sp;
	int i, status;
	pid_t pid;
	static const int death_sigs[3] = { SIGHUP, SIGTERM, SIGKILL };

	for (sp = sessions; sp; sp = sp->se_next)
		sp->se_flags |= SE_SHUTDOWN;

	/* NB: should send a message to the session logger to avoid blocking. */
#ifdef SUPPORT_UTMPX
	logwtmpx("~", "shutdown", "", 0, INIT_PROCESS);
#endif
#ifdef SUPPORT_UTMP
	logwtmp("~", "shutdown", "");
#endif

	for (i = 0; i < 3; ++i) {
		if (kill(-1, death_sigs[i]) == -1 && errno == ESRCH)
			return (state_func_t)single_user;

		clang = 0;
		(void)alarm(DEATH_WATCH);
		do
			if ((pid = waitpid(-1, &status, 0)) != -1)
				collect_child(pid, status);
		while (clang == 0 && errno != ECHILD);

		if (errno == ECHILD)
			return (state_func_t)single_user;
	}

	warning("some processes would not die; ps axl advised");

	return (state_func_t)single_user;
}
#endif /* LETS_GET_SMALL */

#ifdef MFS_DEV_IF_NO_CONSOLE

static int
mfs_dev(void)
{
	/*
	 * We cannot print errors so we bail out silently...
	 */
	pid_t pid;
	int status;

	/* If we have /dev/console, assume all is OK  */
	if (access(_PATH_CONSOLE, F_OK) == 0)
		return 0;

#if 0	/* Useful for testing MAKEDEV */
	/* Mount an mfs over /mnt so we can create a console entry */
	switch ((pid = fork())) {
	case 0:
		(void)execl(INIT_MOUNT_MFS, "mount_mfs",
		    "-b", "4096", "-f", "512",
		    "-s", 64, "-n", 10,
		    "-p", "0755",
		    "swap", "/mnt", NULL);
		_exit(9);
		/*NOTREACHED*/

	case -1:
		return(-1);

	default:
		if (waitpid(pid, &status, 0) == -1)
			return(-1);
		if (status != 0)
			return(-1);
		break;
	}

	{
		dev_t dev;
#ifdef CPU_CONSDEV
		static int name[2] = { CTL_MACHDEP, CPU_CONSDEV };
		size_t olen;
		olen = sizeof(dev);
		if (sysctl(name, sizeof(name) / sizeof(name[0]), &dev, &olen,
		    NULL, 0) == -1)
#endif
			dev = makedev(0, 0);

		/* Make a console for us, so we can see things happening */
		if (mknod("/mnt/console", 0666 | S_IFCHR, dev) == -1)
			return(-1);
		(void)freopen("/mnt/console", "a", stderr);
	}

#endif

	/* Run the makedev script to create devices */
	switch ((pid = fork())) {
	case 0:
		(void)dup2(2, 1);	/* Give the script stdout */
		if (chdir("/dev") == 0)
			(void)execl(INIT_BSHELL, "sh",
			    access("./MAKEDEV", X_OK) == 0
				? "./MAKEDEV" : "/etc/MAKEDEV",
			    "-MM", "init", NULL); 
		_exit(10);
		/* NOTREACHED */

	case -1:
		break;

	default:
		if (waitpid(pid, &status, 0) == -1)
			break;
		if (status != 0)
			warn("MAKEDEV exit status %d", status);
		/*
		 * If /dev/console got created, then return 0
		 * regardless of MAKEDEV exit status.
		 */
		if (access(_PATH_CONSOLE, F_OK) == 0)
			return 0;
		_exit(11);
	}
	warn("Unable to run MAKEDEV");
	_exit(12);
}
#endif

#ifndef LETS_GET_SMALL
static int
do_setttyent(void)
{
	(void)endttyent();
#ifdef CHROOT
	if (did_multiuser_chroot) {
		char path[PATH_MAX];

		(void)snprintf(path, sizeof(path), "%s/%s", rootdir, _PATH_TTYS);

		return setttyentpath(path);
	} else
#endif /* CHROOT */
		return setttyent();
}
#endif

#if !defined(LETS_GET_SMALL) && defined(CHROOT)

static int
createsysctlnode(void)
{
	struct sysctlnode node;
	int mib[2];
	size_t len;

	/*
	 * Create top-level dynamic sysctl node.  Its child nodes will only
	 * be readable by the superuser, since regular mortals should not
	 * care ("Sssh, it's a secret!").
	 */
	len = sizeof(struct sysctlnode);
	mib[0] = CTL_CREATE;

	(void)memset(&node, 0, len);
	node.sysctl_flags = SYSCTL_VERSION | CTLFLAG_READWRITE |
	    CTLFLAG_PRIVATE | CTLTYPE_NODE;
	node.sysctl_num = CTL_CREATE;
	(void)snprintf(node.sysctl_name, SYSCTL_NAMELEN, "init");
	if (sysctl(&mib[0], 1, &node, &len, &node, len) == -1) {
		warning("could not create init node: %m");
		return -1;
	}

	/*
	 * Create second level dynamic node capable of holding pathname.
	 * Provide "/" as the default value.
	 */
	len = sizeof(struct sysctlnode);
	mib[0] = node.sysctl_num;
	mib[1] = CTL_CREATE;

	(void)memset(&node, 0, len);
	node.sysctl_flags = SYSCTL_VERSION | CTLFLAG_READWRITE |
	    CTLTYPE_STRING | CTLFLAG_OWNDATA;
	node.sysctl_size = _POSIX_PATH_MAX;
	node.sysctl_data = __UNCONST("/");
	node.sysctl_num = CTL_CREATE;
	(void)snprintf(node.sysctl_name, SYSCTL_NAMELEN, "root");
	if (sysctl(&mib[0], 2, NULL, NULL, &node, len) == -1) {
		warning("could not create init.root node: %m");
		return -1;
	}

	return 0;
}

static int
shouldchroot(void)
{
	struct sysctlnode node;
	size_t len, cnt;
	int mib;

	len = sizeof(struct sysctlnode);

	if (sysctlbyname("init.root", rootdir, &len, NULL, 0) == -1) {
		warning("could not read init.root: %m");

		/* Child killed our node. Recreate it. */
		if (errno == ENOENT) {
			/* Destroy whatever is left, recreate from scratch. */
			if (sysctlnametomib("init", &mib, &cnt) != -1) {
				(void)memset(&node, 0, sizeof(node));
				node.sysctl_flags = SYSCTL_VERSION;
				node.sysctl_num = mib;
				mib = CTL_DESTROY;

				(void)sysctl(&mib, 1, NULL, NULL, &node,
				    sizeof(node));
			}

			(void)createsysctlnode();
		}

		/* We certainly won't chroot. */
		return 0;
	}

	if (rootdir[len] != '\0' || strlen(rootdir) != len - 1) {
		warning("init.root is not a string");
		return 0;
	}

	if (strcmp(rootdir, "/") == 0)
		return 0;

	return 1;
}

#endif /* !LETS_GET_SMALL && CHROOT */

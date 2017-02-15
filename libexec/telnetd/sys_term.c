/*	$NetBSD: sys_term.c,v 1.47 2013/06/28 15:48:02 christos Exp $	*/

/*
 * Copyright (c) 1989, 1993
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
static char sccsid[] = "@(#)sys_term.c	8.4+1 (Berkeley) 5/30/95";
#else
__RCSID("$NetBSD: sys_term.c,v 1.47 2013/06/28 15:48:02 christos Exp $");
#endif
#endif /* not lint */

#include "telnetd.h"
#include "pathnames.h"

#include <util.h>
#include <vis.h>

#ifdef SUPPORT_UTMP
#include <utmp.h>
#endif
#ifdef SUPPORT_UTMPX
#include <utmpx.h>
#endif

#define SCPYN(a, b)	(void) strncpy(a, b, sizeof(a))
#define SCMPN(a, b)	strncmp(a, b, sizeof(a))

struct termios termbuf, termbuf2;	/* pty control structure */

void getptyslave(void);
int cleanopen(char *);
char **addarg(char **, const char *);
void scrub_env(void);
int getent(char *, char *);
char *getstr(const char *, char **);
#ifdef KRB5
extern void kerberos5_cleanup(void);
#endif

/*
 * init_termbuf()
 * copy_termbuf(cp)
 * set_termbuf()
 *
 * These three routines are used to get and set the "termbuf" structure
 * to and from the kernel.  init_termbuf() gets the current settings.
 * copy_termbuf() hands in a new "termbuf" to write to the kernel, and
 * set_termbuf() writes the structure into the kernel.
 */

void
init_termbuf(void)
{
	(void) tcgetattr(pty, &termbuf);
	termbuf2 = termbuf;
}

#if	defined(LINEMODE) && defined(TIOCPKT_IOCTL)
void
copy_termbuf(char *cp, int len)
{
	if ((size_t)len > sizeof(termbuf))
		len = sizeof(termbuf);
	memmove((char *)&termbuf, cp, len);
	termbuf2 = termbuf;
}
#endif	/* defined(LINEMODE) && defined(TIOCPKT_IOCTL) */

void
set_termbuf(void)
{
	/*
	 * Only make the necessary changes.
	 */
	if (memcmp((char *)&termbuf, (char *)&termbuf2, sizeof(termbuf)))
		(void) tcsetattr(pty, TCSANOW, &termbuf);
}


/*
 * spcset(func, valp, valpp)
 *
 * This function takes various special characters (func), and
 * sets *valp to the current value of that character, and
 * *valpp to point to where in the "termbuf" structure that
 * value is kept.
 *
 * It returns the SLC_ level of support for this function.
 */


int
spcset(int func, cc_t *valp, cc_t **valpp)
{

#define	setval(a, b)	*valp = termbuf.c_cc[a]; \
			*valpp = &termbuf.c_cc[a]; \
			return(b);
#define	defval(a) *valp = ((cc_t)a); *valpp = (cc_t *)0; return(SLC_DEFAULT);

	switch(func) {
	case SLC_EOF:
		setval(VEOF, SLC_VARIABLE);
	case SLC_EC:
		setval(VERASE, SLC_VARIABLE);
	case SLC_EL:
		setval(VKILL, SLC_VARIABLE);
	case SLC_IP:
		setval(VINTR, SLC_VARIABLE|SLC_FLUSHIN|SLC_FLUSHOUT);
	case SLC_ABORT:
		setval(VQUIT, SLC_VARIABLE|SLC_FLUSHIN|SLC_FLUSHOUT);
	case SLC_XON:
		setval(VSTART, SLC_VARIABLE);
	case SLC_XOFF:
		setval(VSTOP, SLC_VARIABLE);
	case SLC_EW:
		setval(VWERASE, SLC_VARIABLE);
	case SLC_RP:
		setval(VREPRINT, SLC_VARIABLE);
	case SLC_LNEXT:
		setval(VLNEXT, SLC_VARIABLE);
	case SLC_AO:
		setval(VDISCARD, SLC_VARIABLE|SLC_FLUSHOUT);
	case SLC_SUSP:
		setval(VSUSP, SLC_VARIABLE|SLC_FLUSHIN);
	case SLC_FORW1:
		setval(VEOL, SLC_VARIABLE);
	case SLC_FORW2:
		setval(VEOL2, SLC_VARIABLE);
	case SLC_AYT:
		setval(VSTATUS, SLC_VARIABLE);

	case SLC_BRK:
	case SLC_SYNCH:
	case SLC_EOR:
		defval(0);

	default:
		*valp = 0;
		*valpp = 0;
		return(SLC_NOSUPPORT);
	}
}


/*
 * getpty()
 *
 * Allocate a pty.  As a side effect, the external character
 * array "line" contains the name of the slave side.
 *
 * Returns the file descriptor of the opened pty.
 */
#ifndef	__GNUC__
char *line = NULL16STR;
#else
static char Xline[] = NULL16STR;
char *line = Xline;
#endif


static int ptyslavefd; /* for cleanopen() */

int
getpty(int *ptynum)
{
	int ptyfd;

	ptyfd = openpty(ptynum, &ptyslavefd, line, NULL, NULL);
	if (ptyfd == 0)
		return *ptynum;
	ptyslavefd = -1;
	return (-1);
}

#ifdef	LINEMODE
/*
 * tty_flowmode()	Find out if flow control is enabled or disabled.
 * tty_linemode()	Find out if linemode (external processing) is enabled.
 * tty_setlinemod(on)	Turn on/off linemode.
 * tty_isecho()		Find out if echoing is turned on.
 * tty_setecho(on)	Enable/disable character echoing.
 * tty_israw()		Find out if terminal is in RAW mode.
 * tty_binaryin(on)	Turn on/off BINARY on input.
 * tty_binaryout(on)	Turn on/off BINARY on output.
 * tty_isediting()	Find out if line editing is enabled.
 * tty_istrapsig()	Find out if signal trapping is enabled.
 * tty_setedit(on)	Turn on/off line editing.
 * tty_setsig(on)	Turn on/off signal trapping.
 * tty_issofttab()	Find out if tab expansion is enabled.
 * tty_setsofttab(on)	Turn on/off soft tab expansion.
 * tty_islitecho()	Find out if typed control chars are echoed literally
 * tty_setlitecho()	Turn on/off literal echo of control chars
 * tty_tspeed(val)	Set transmit speed to val.
 * tty_rspeed(val)	Set receive speed to val.
 */


int
tty_linemode(void)
{
	return(termbuf.c_lflag & EXTPROC);
}

void
tty_setlinemode(int on)
{
	set_termbuf();
	(void) ioctl(pty, TIOCEXT, (char *)&on);
	init_termbuf();
}
#endif	/* LINEMODE */

int
tty_isecho(void)
{
	return (termbuf.c_lflag & ECHO);
}

int
tty_flowmode(void)
{
	return((termbuf.c_iflag & IXON) ? 1 : 0);
}

int
tty_restartany(void)
{
	return((termbuf.c_iflag & IXANY) ? 1 : 0);
}

void
tty_setecho(int on)
{
	if (on)
		termbuf.c_lflag |= ECHO;
	else
		termbuf.c_lflag &= ~ECHO;
}

int
tty_israw(void)
{
	return(!(termbuf.c_lflag & ICANON));
}

void
tty_binaryin(int on)
{
	if (on) {
		termbuf.c_iflag &= ~ISTRIP;
	} else {
		termbuf.c_iflag |= ISTRIP;
	}
}

void
tty_binaryout(int on)
{
	if (on) {
		termbuf.c_cflag &= ~(CSIZE|PARENB);
		termbuf.c_cflag |= CS8;
		termbuf.c_oflag &= ~OPOST;
	} else {
		termbuf.c_cflag &= ~CSIZE;
		termbuf.c_cflag |= CS7|PARENB;
		termbuf.c_oflag |= OPOST;
	}
}

int
tty_isbinaryin(void)
{
	return(!(termbuf.c_iflag & ISTRIP));
}

int
tty_isbinaryout(void)
{
	return(!(termbuf.c_oflag&OPOST));
}

#ifdef	LINEMODE
int
tty_isediting(void)
{
	return(termbuf.c_lflag & ICANON);
}

int
tty_istrapsig(void)
{
	return(termbuf.c_lflag & ISIG);
}

void
tty_setedit(int on)
{
	if (on)
		termbuf.c_lflag |= ICANON;
	else
		termbuf.c_lflag &= ~ICANON;
}

void
tty_setsig(int on)
{
	if (on)
		termbuf.c_lflag |= ISIG;
	else
		termbuf.c_lflag &= ~ISIG;
}
#endif	/* LINEMODE */

int
tty_issofttab(void)
{
# ifdef	OXTABS
	return (termbuf.c_oflag & OXTABS);
# endif
# ifdef	TABDLY
	return ((termbuf.c_oflag & TABDLY) == TAB3);
# endif
}

void
tty_setsofttab(int on)
{
	if (on) {
# ifdef	OXTABS
		termbuf.c_oflag |= OXTABS;
# endif
# ifdef	TABDLY
		termbuf.c_oflag &= ~TABDLY;
		termbuf.c_oflag |= TAB3;
# endif
	} else {
# ifdef	OXTABS
		termbuf.c_oflag &= ~OXTABS;
# endif
# ifdef	TABDLY
		termbuf.c_oflag &= ~TABDLY;
		termbuf.c_oflag |= TAB0;
# endif
	}
}

int
tty_islitecho(void)
{
# ifdef	ECHOCTL
	return (!(termbuf.c_lflag & ECHOCTL));
# endif
# ifdef	TCTLECH
	return (!(termbuf.c_lflag & TCTLECH));
# endif
# if	!defined(ECHOCTL) && !defined(TCTLECH)
	return (0);	/* assumes ctl chars are echoed '^x' */
# endif
}

void
tty_setlitecho(int on)
{
# ifdef	ECHOCTL
	if (on)
		termbuf.c_lflag &= ~ECHOCTL;
	else
		termbuf.c_lflag |= ECHOCTL;
# endif
# ifdef	TCTLECH
	if (on)
		termbuf.c_lflag &= ~TCTLECH;
	else
		termbuf.c_lflag |= TCTLECH;
# endif
}

int
tty_iscrnl(void)
{
	return (termbuf.c_iflag & ICRNL);
}

void
tty_tspeed(int val)
{
	cfsetospeed(&termbuf, val);
}

void
tty_rspeed(int val)
{
	cfsetispeed(&termbuf, val);
}




/*
 * getptyslave()
 *
 * Open the slave side of the pty, and do any initialization
 * that is necessary.  The return value is a file descriptor
 * for the slave side.
 */
extern int def_tspeed, def_rspeed;
	extern int def_row, def_col;

void
getptyslave(void)
{
	int t = -1;

#ifdef	LINEMODE
	int waslm;
#endif
	struct winsize ws;
	/*
	 * Opening the slave side may cause initilization of the
	 * kernel tty structure.  We need remember the state of
	 * 	if linemode was turned on
	 *	terminal window size
	 *	terminal speed
	 * so that we can re-set them if we need to.
	 */
#ifdef	LINEMODE
	waslm = tty_linemode();
#endif

	/*
	 * Make sure that we don't have a controlling tty, and
	 * that we are the session (process group) leader.
	 */
	t = open(_PATH_TTY, O_RDWR);
	if (t >= 0) {
		(void) ioctl(t, TIOCNOTTY, (char *)0);
		(void) close(t);
	}



	t = cleanopen(line);
	if (t < 0)
		fatalperror(net, line);


	/*
	 * set up the tty modes as we like them to be.
	 */
	init_termbuf();
	if (def_row || def_col) {
		memset((char *)&ws, 0, sizeof(ws));
		ws.ws_col = def_col;
		ws.ws_row = def_row;
		(void)ioctl(t, TIOCSWINSZ, (char *)&ws);
	}

	/*
	 * Settings for sgtty based systems
	 */

	/*
	 * Settings for all other termios/termio based
	 * systems, other than 4.4BSD.  In 4.4BSD the
	 * kernel does the initial terminal setup.
	 */
	tty_rspeed((def_rspeed > 0) ? def_rspeed : 9600);
	tty_tspeed((def_tspeed > 0) ? def_tspeed : 9600);
#ifdef	LINEMODE
	if (waslm)
		tty_setlinemode(1);
#endif	/* LINEMODE */

	/*
	 * Set the tty modes, and make this our controlling tty.
	 */
	set_termbuf();
	if (login_tty(t) == -1)
		fatalperror(net, "login_tty");
	if (net > 2)
		(void) close(net);
	if (pty > 2) {
		(void) close(pty);
		pty = -1;
	}
}

/*
 * Open the specified slave side of the pty,
 * making sure that we have a clean tty.
 */
int
cleanopen(char *ttyline)
{
	return ptyslavefd;
}

/*
 * startslave(host)
 *
 * Given a hostname, do whatever
 * is necessary to startup the login process on the slave side of the pty.
 */

/* ARGSUSED */
void
startslave(char *host, int autologin, char *autoname)
{
	int i;

#ifdef AUTHENTICATION
	if (!autoname || !autoname[0])
		autologin = 0;

	if (autologin < auth_level) {
		fatal(net, "Authorization failed");
		exit(1);
	}
#endif


	if ((i = fork()) < 0)
		fatalperror(net, "fork");
	if (i) {
	} else {
		getptyslave();
		start_login(host, autologin, autoname);
		/*NOTREACHED*/
	}
}

char	*envinit[3];

void
init_env(void)
{
	char **envp;

	envp = envinit;
	if ((*envp = getenv("TZ")))
		*envp++ -= 3;
	*envp = 0;
	environ = envinit;
}


/*
 * start_login(host)
 *
 * Assuming that we are now running as a child processes, this
 * function will turn us into the login process.
 */
extern char *gettyname;

void
start_login(char *host, int autologin, char *name)
{
	char **argv;
#define	TABBUFSIZ	512
	char	defent[TABBUFSIZ];
	char	defstrs[TABBUFSIZ];
#undef	TABBUFSIZ
	const char *loginprog = NULL;
	extern struct sockaddr_storage from;
	char buf[sizeof(from) * 4 + 1];

	scrub_env();

	/*
	 * -a : pass on the address of the host.
	 * -h : pass on name of host.
	 *	WARNING:  -h and -a are accepted by login
	 *	if and only if getuid() == 0.
	 * -p : don't clobber the environment (so terminal type stays set).
	 *
	 * -f : force this login, he has already been authenticated
	 */
	argv = addarg(0, "login");

	argv = addarg(argv, "-a");
	(void)strvisx(buf, (const char *)(const void *)&from, sizeof(from),
	    VIS_WHITE);
	argv = addarg(argv, buf);

	argv = addarg(argv, "-h");
	argv = addarg(argv, host);

	argv = addarg(argv, "-p");
#ifdef	LINEMODE
	/*
	 * Set the environment variable "LINEMODE" to either
	 * "real" or "kludge" if we are operating in either
	 * real or kludge linemode.
	 */
	if (lmodetype == REAL_LINEMODE)
		setenv("LINEMODE", "real", 1);
# ifdef KLUDGELINEMODE
	else if (lmodetype == KLUDGE_LINEMODE || lmodetype == KLUDGE_OK)
		setenv("LINEMODE", "kludge", 1);
# endif
#endif
#ifdef SECURELOGIN
	/*
	 * don't worry about the -f that might get sent.
	 * A -s is supposed to override it anyhow.
	 */
	if (require_secure_login)
		argv = addarg(argv, "-s");
#endif
#ifdef AUTHENTICATION
	if (auth_level >= 0 && autologin == AUTH_VALID) {
		argv = addarg(argv, "-f");
		argv = addarg(argv, "--");
		argv = addarg(argv, name);
	} else
#endif
	if (getenv("USER")) {
		argv = addarg(argv, "--");
		argv = addarg(argv, getenv("USER"));
		/*
		 * Assume that login will set the USER variable
		 * correctly.  For SysV systems, this means that
		 * USER will no longer be set, just LOGNAME by
		 * login.  (The problem is that if the auto-login
		 * fails, and the user then specifies a different
		 * account name, he can get logged in with both
		 * LOGNAME and USER in his environment, but the
		 * USER value will be wrong.
		 */
		unsetenv("USER");
	}
        if (getent(defent, gettyname) == 1) {
                char *cp = defstrs;

                loginprog = getstr("lo", &cp);
        }
        if (loginprog == NULL)
                loginprog = _PATH_LOGIN;
	closelog();
	/*
	 * This sleep(1) is in here so that telnetd can
	 * finish up with the tty.  There's a race condition
	 * the login banner message gets lost...
	 */
	sleep(1);
        execv(loginprog, argv);

        syslog(LOG_ERR, "%s: %m", loginprog);
        fatalperror(net, loginprog);
	/*NOTREACHED*/
}

char **
addarg(char **argv, const char *val)
{
	char **cpp;
	char **nargv;

	if (argv == NULL) {
		/*
		 * 10 entries, a leading length, and a null
		 */
		argv = malloc(sizeof(*argv) * 12);
		if (argv == NULL)
			return(NULL);
		*argv++ = (char *)10;
		*argv = (char *)0;
	}
	for (cpp = argv; *cpp; cpp++)
		;
	if (cpp == &argv[(long)argv[-1]]) {
		--argv;
		nargv = realloc(argv, sizeof(*argv) * ((long)(*argv) + 10 + 2));
		if (nargv == NULL) {
			fatal(net, "not enough memory");
			/*NOTREACHED*/
		}
		argv = nargv;
		*argv = (char *)((long)(*argv) + 10);
		argv++;
		cpp = &argv[(long)argv[-1] - 10];
	}
	*cpp++ = __UNCONST(val);
	*cpp = 0;
	return(argv);
}

/*
 * scrub_env()
 *
 * We only accept the environment variables listed below.
 */

void
scrub_env(void)
{
	static const char *reject[] = {
		"TERMCAP=/",
		NULL
	};

	static const char *acceptstr[] = {
		"XAUTH=", "XAUTHORITY=", "DISPLAY=",
		"TERM=",
		"EDITOR=",
		"PAGER=",
		"LOGNAME=",
		"POSIXLY_CORRECT=",
		"TERMCAP=",
		"PRINTER=",
		NULL
	};

	char **cpp, **cpp2;
	const char **p;

	for (cpp2 = cpp = environ; *cpp; cpp++) {
		int reject_it = 0;

		for(p = reject; *p; p++)
			if(strncmp(*cpp, *p, strlen(*p)) == 0) {
				reject_it = 1;
				break;
			}
		if (reject_it)
			continue;

		for(p = acceptstr; *p; p++)
			if(strncmp(*cpp, *p, strlen(*p)) == 0)
				break;
		if(*p != NULL)
			*cpp2++ = *cpp;
	}
	*cpp2 = NULL;
}

/*
 * cleanup()
 *
 * This is the routine to call when we are all through, to
 * clean up anything that needs to be cleaned up.
 */
/* ARGSUSED */
void
cleanup(int sig)
{
	char *p, c;

	p = line + sizeof(_PATH_DEV) - 1;
#ifdef SUPPORT_UTMP
	if (logout(p))
		logwtmp(p, "", "");
#endif
#ifdef SUPPORT_UTMPX
	if (logoutx(p, 0, DEAD_PROCESS))
		logwtmpx(p, "", "", 0, DEAD_PROCESS);
#endif
	(void)chmod(line, 0666);
	(void)chown(line, 0, 0);
	c = *p; *p = 'p';
	(void)chmod(line, 0666);
	(void)chown(line, 0, 0);
	*p = c;
	if (ttyaction(line, "telnetd", "root"))
		syslog(LOG_ERR, "%s: ttyaction failed", line);
	(void) shutdown(net, 2);
	exit(1);
}

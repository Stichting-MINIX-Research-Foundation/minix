/*	$NetBSD: ftpd.c,v 1.202 2015/08/10 07:32:49 shm Exp $	*/

/*
 * Copyright (c) 1997-2009 The NetBSD Foundation, Inc.
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
__COPYRIGHT("@(#) Copyright (c) 1985, 1988, 1990, 1992, 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)ftpd.c	8.5 (Berkeley) 4/28/95";
#else
__RCSID("$NetBSD: ftpd.c,v 1.202 2015/08/10 07:32:49 shm Exp $");
#endif
#endif /* not lint */

/*
 * FTP server.
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/resource.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#define	FTP_NAMES
#include <arpa/ftp.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <glob.h>
#include <grp.h>
#include <limits.h>
#include <netdb.h>
#include <pwd.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>
#include <util.h>
#ifdef SUPPORT_UTMP
#include <utmp.h>
#endif
#ifdef SUPPORT_UTMPX
#include <utmpx.h>
#endif
#ifdef SKEY
#include <skey.h>
#endif
#ifdef KERBEROS5
#include <com_err.h>
#include <krb5/krb5.h>
#endif

#ifdef	LOGIN_CAP
#include <login_cap.h>
#endif

#ifdef USE_PAM
#include <security/pam_appl.h>
#endif

#include "pfilter.h"

#define	GLOBAL
#include "extern.h"
#include "pathnames.h"
#include "version.h"

static sig_atomic_t	transflag;
static sig_atomic_t	urgflag;

int	data;
int	Dflag;
int	sflag;
int	stru;			/* avoid C keyword */
int	mode;
int	dataport;		/* use specific data port */
int	dopidfile;		/* maintain pid file */
int	doutmp;			/* update utmp file */
int	dowtmp;			/* update wtmp file */
int	doxferlog;		/* syslog/write wu-ftpd style xferlog entries */
int	xferlogfd;		/* fd to write wu-ftpd xferlog entries to */
int	getnameopts;		/* flags for use with getname() */
int	dropprivs;		/* if privileges should or have been dropped */
int	mapped;			/* IPv4 connection on AF_INET6 socket */
off_t	file_size;
off_t	byte_count;
static char ttyline[20];

#ifdef USE_PAM
static int	auth_pam(void);
pam_handle_t	*pamh = NULL;
#endif

#ifdef SUPPORT_UTMP
static struct utmp utmp;	/* for utmp */
#endif
#ifdef SUPPORT_UTMPX
static struct utmpx utmpx;	/* for utmpx */
#endif

static const char *anondir = NULL;
static const char *confdir = _DEFAULT_CONFDIR;

static char	*curname;		/* current USER name */
static size_t	curname_len;		/* length of curname (include NUL) */

#if defined(KERBEROS) || defined(KERBEROS5)
int	has_ccache = 0;
int	notickets = 1;
char	*krbtkfile_env = NULL;
char	*tty = ttyline;
int	login_krb5_forwardable_tgt = 0;
#endif

int epsvall = 0;

/*
 * Timeout intervals for retrying connections
 * to hosts that don't accept PORT cmds.  This
 * is a kludge, but given the problems with TCP...
 */
#define	SWAITMAX	90	/* wait at most 90 seconds */
#define	SWAITINT	5	/* interval between retries */

int	swaitmax = SWAITMAX;
int	swaitint = SWAITINT;

enum send_status {
	SS_SUCCESS,
	SS_ABORTED,			/* transfer aborted */
	SS_NO_TRANSFER,			/* no transfer made yet */
	SS_FILE_ERROR,			/* file read error */
	SS_DATA_ERROR			/* data send error */
};

static int	 bind_pasv_addr(void);
static int	 checkuser(const char *, const char *, int, int, char **);
static int	 checkaccess(const char *);
static int	 checkpassword(const struct passwd *, const char *);
static void	 do_pass(int, int, const char *);
static void	 end_login(void);
static FILE	*getdatasock(const char *);
static char	*gunique(const char *);
static void	 login_utmp(const char *, const char *, const char *,
		     struct sockinet *);
static void	 logremotehost(struct sockinet *);
__dead static void	 lostconn(int);
__dead static void	 toolong(int);
__dead static void	 sigquit(int);
static void	 sigurg(int);
static int	 handleoobcmd(void);
static int	 receive_data(FILE *, FILE *);
static int	 send_data(FILE *, FILE *, const struct stat *, int);
static struct passwd *sgetpwnam(const char *);
static int	 write_data(int, char *, size_t, off_t *, struct timeval *,
		     int);
static enum send_status
		 send_data_with_read(int, int, const struct stat *, int);
#if !defined(__minix)
static enum send_status
		 send_data_with_mmap(int, int, const struct stat *, int);
#endif /* !defined(__minix) */
static void	 logrusage(const struct rusage *, const struct rusage *);
static void	 logout_utmp(void);

int	main(int, char *[]);

#if defined(KERBEROS)
int	klogin(struct passwd *, char *, char *, char *);
void	kdestroy(void);
#endif
#if defined(KERBEROS5)
int	k5login(struct passwd *, char *, char *, char *);
void	k5destroy(void);
#endif

int
main(int argc, char *argv[])
{
	int		ch, on = 1, tos, keepalive;
	socklen_t	addrlen;
#ifdef KERBEROS5
	krb5_error_code	kerror;
#endif
	char		*p;
	const char	*xferlogname = NULL;
	long		l;
	struct sigaction sa;
	sa_family_t	af = AF_UNSPEC;

	connections = 1;
	ftpd_debug = 0;
	logging = 0;
	pdata = -1;
	Dflag = 0;
	sflag = 0;
	dataport = 0;
	dopidfile = 1;		/* default: DO use a pid file to count users */
	doutmp = 0;		/* default: Do NOT log to utmp */
	dowtmp = 1;		/* default: DO log to wtmp */
	doxferlog = 0;		/* default: Do NOT syslog xferlog */
	xferlogfd = -1;		/* default: Do NOT write xferlog file */
	getnameopts = 0;	/* default: xlate addrs to name */
	dropprivs = 0;
	mapped = 0;
	usedefault = 1;
	emailaddr = NULL;
	hostname[0] = '\0';
	homedir[0] = '\0';
	gidcount = 0;
	is_oob = 0;
	version = FTPD_VERSION;

	/*
	 * LOG_NDELAY sets up the logging connection immediately,
	 * necessary for anonymous ftp's that chroot and can't do it later.
	 */
	openlog("ftpd", LOG_PID | LOG_NDELAY, LOG_FTP);

	while ((ch = getopt(argc, argv,
	    "46a:c:C:Dde:h:HlL:nP:qQrst:T:uUvV:wWX")) != -1) {
		switch (ch) {
		case '4':
			af = AF_INET;
			break;

		case '6':
			af = AF_INET6;
			break;

		case 'a':
			anondir = optarg;
			break;

		case 'c':
			confdir = optarg;
			break;

		case 'C':
			if ((p = strchr(optarg, '@')) != NULL) {
				*p++ = '\0';
				strlcpy(remotehost, p, MAXHOSTNAMELEN + 1);
				if (inet_pton(AF_INET, p,
				    &his_addr.su_addr) == 1) {
					his_addr.su_family = AF_INET;
					his_addr.su_len =
					    sizeof(his_addr.si_su.su_sin);
#ifdef INET6
				} else if (inet_pton(AF_INET6, p,
				    &his_addr.su_6addr) == 1) {
					his_addr.su_family = AF_INET6;
					his_addr.su_len =
					    sizeof(his_addr.si_su.su_sin6);
#endif
				} else
					his_addr.su_family = AF_UNSPEC;
			}
			pw = sgetpwnam(optarg);
			exit(checkaccess(optarg) ? 0 : 1);
			/* NOTREACHED */

		case 'D':
			Dflag = 1;
			break;

		case 'd':
		case 'v':		/* deprecated */
			ftpd_debug = 1;
			break;

		case 'e':
			emailaddr = optarg;
			break;

		case 'h':
			strlcpy(hostname, optarg, sizeof(hostname));
			break;

		case 'H':
			if (gethostname(hostname, sizeof(hostname)) == -1)
				hostname[0] = '\0';
			hostname[sizeof(hostname) - 1] = '\0';
			break;

		case 'l':
			logging++;	/* > 1 == extra logging */
			break;

		case 'L':
			xferlogname = optarg;
			break;

		case 'n':
			getnameopts = NI_NUMERICHOST;
			break;

		case 'P':
			errno = 0;
			p = NULL;
			l = strtol(optarg, &p, 10);
			if (errno || *optarg == '\0' || *p != '\0' ||
			    l < IPPORT_RESERVED ||
			    l > IPPORT_ANONMAX) {
				syslog(LOG_WARNING, "Invalid dataport %s",
				    optarg);
				dataport = 0;
			}
			dataport = (int)l;
			break;

		case 'q':
			dopidfile = 1;
			break;

		case 'Q':
			dopidfile = 0;
			break;

		case 'r':
			dropprivs = 1;
			break;

		case 's':
			sflag = 1;
			break;

		case 't':
		case 'T':
			syslog(LOG_WARNING,
			    "-%c has been deprecated in favour of ftpd.conf",
			    ch);
			break;

		case 'u':
			doutmp = 1;
			break;

		case 'U':
			doutmp = 0;
			break;

		case 'V':
			if (EMPTYSTR(optarg) || strcmp(optarg, "-") == 0)
				version = NULL;
			else
				version = ftpd_strdup(optarg);
			break;

		case 'w':
			dowtmp = 1;
			break;

		case 'W':
			dowtmp = 0;
			break;

		case 'X':
			doxferlog |= 1;
			break;

		default:
			if (optopt == 'a' || optopt == 'C')
				exit(1);
			syslog(LOG_WARNING, "unknown flag -%c ignored", optopt);
			break;
		}
	}
	if (EMPTYSTR(confdir))
		confdir = _DEFAULT_CONFDIR;

	pfilter_open();

	if (dowtmp) {
#ifdef SUPPORT_UTMPX
		ftpd_initwtmpx();
#endif
#ifdef SUPPORT_UTMP
		ftpd_initwtmp();
#endif
	}
	errno = 0;
#if !defined(__minix)
	l = sysconf(_SC_LOGIN_NAME_MAX);
	if (l == -1 && errno != 0) {
		syslog(LOG_ERR, "sysconf _SC_LOGIN_NAME_MAX: %m");
		exit(1);
	} else if (l <= 0) {
		syslog(LOG_WARNING, "using conservative LOGIN_NAME_MAX value");
		curname_len = _POSIX_LOGIN_NAME_MAX;
	} else 
		curname_len = (size_t)l;
#else
	curname_len = _POSIX_LOGIN_NAME_MAX;
#endif /* !defined(__minix) */
	curname = malloc(curname_len);
	if (curname == NULL) {
		syslog(LOG_ERR, "malloc: %m");
		exit(1);
	}
	curname[0] = '\0';

	if (Dflag) {
		int error, fd, i, n, *socks;
		struct pollfd *fds;
		struct addrinfo hints, *res, *res0;

		if (daemon(1, 0) == -1) {
			syslog(LOG_ERR, "failed to daemonize: %m");
			exit(1);
		}
		(void)memset(&sa, 0, sizeof(sa));
		sa.sa_handler = SIG_IGN;
		sa.sa_flags = SA_NOCLDWAIT;
		sigemptyset(&sa.sa_mask);
		(void)sigaction(SIGCHLD, &sa, NULL);

		(void)memset(&hints, 0, sizeof(hints));
		hints.ai_flags = AI_PASSIVE;
		hints.ai_family = af;
		hints.ai_socktype = SOCK_STREAM;
		error = getaddrinfo(NULL, "ftp", &hints, &res0);
		if (error) {
			syslog(LOG_ERR, "getaddrinfo: %s", gai_strerror(error));
			exit(1);
		}

		for (n = 0, res = res0; res != NULL; res = res->ai_next)
			n++;
		if (n == 0) {
			syslog(LOG_ERR, "no addresses available");
			exit(1);
		}
		socks = malloc(n * sizeof(int));
		fds = malloc(n * sizeof(struct pollfd));
		if (socks == NULL || fds == NULL) {
			syslog(LOG_ERR, "malloc: %m");
			exit(1);
		}

		for (n = 0, res = res0; res != NULL; res = res->ai_next) {
			socks[n] = socket(res->ai_family, res->ai_socktype,
			    res->ai_protocol);
			if (socks[n] == -1)
				continue;
			(void)setsockopt(socks[n], SOL_SOCKET, SO_REUSEADDR,
			    &on, sizeof(on));
			if (bind(socks[n], res->ai_addr, res->ai_addrlen)
			    == -1) {
				(void)close(socks[n]);
				continue;
			}
			if (listen(socks[n], 12) == -1) {
				(void)close(socks[n]);
				continue;
			}

			fds[n].fd = socks[n];
			fds[n].events = POLLIN;
			n++;
		}
		if (n == 0) {
			syslog(LOG_ERR, "%m");
			exit(1);
		}
		freeaddrinfo(res0);

		if (pidfile(NULL) == -1)
			syslog(LOG_ERR, "failed to write a pid file: %m");

		for (;;) {
			if (poll(fds, n, INFTIM) == -1) {
				if (errno == EINTR)
					continue;
				syslog(LOG_ERR, "poll: %m");
				exit(1);
			}
			for (i = 0; i < n; i++) {
				if (fds[i].revents & POLLIN) {
					fd = accept(fds[i].fd, NULL, NULL);
					if (fd == -1) {
						syslog(LOG_ERR, "accept: %m");
						continue;
					}
					switch (fork()) {
					case -1:
						syslog(LOG_ERR, "fork: %m");
						break;
					case 0:
						goto child;
						/* NOTREACHED */
					}
					(void)close(fd);
				}
			}
		}
 child:
		(void)dup2(fd, STDIN_FILENO);
		(void)dup2(fd, STDOUT_FILENO);
		(void)dup2(fd, STDERR_FILENO);
		for (i = 0; i < n; i++)
			(void)close(socks[i]);
	}

	memset((char *)&his_addr, 0, sizeof(his_addr));
	addrlen = sizeof(his_addr.si_su);
	if (getpeername(0, (struct sockaddr *)&his_addr.si_su, &addrlen) < 0) {
		syslog((errno == ENOTCONN) ? LOG_NOTICE : LOG_ERR,
		    "getpeername (%s): %m",argv[0]);
		exit(1);
	}
	his_addr.su_len = addrlen;
	memset((char *)&ctrl_addr, 0, sizeof(ctrl_addr));
	addrlen = sizeof(ctrl_addr.si_su);
	if (getsockname(0, (struct sockaddr *)&ctrl_addr, &addrlen) < 0) {
		syslog(LOG_ERR, "getsockname (%s): %m",argv[0]);
		exit(1);
	}
	ctrl_addr.su_len = addrlen;
#ifdef INET6
	if (his_addr.su_family == AF_INET6
	 && IN6_IS_ADDR_V4MAPPED(&his_addr.su_6addr)) {
#if 1
		/*
		 * IPv4 control connection arrived to AF_INET6 socket.
		 * I hate to do this, but this is the easiest solution.
		 *
		 * The assumption is untrue on SIIT environment.
		 */
		struct sockinet tmp_addr;
		const int off = sizeof(struct in6_addr) - sizeof(struct in_addr);

		tmp_addr = his_addr;
		memset(&his_addr, 0, sizeof(his_addr));
		his_addr.su_family = AF_INET;
		his_addr.su_len = sizeof(his_addr.si_su.su_sin);
		memcpy(&his_addr.su_addr, &tmp_addr.su_6addr.s6_addr[off],
		    sizeof(his_addr.su_addr));
		his_addr.su_port = tmp_addr.su_port;

		tmp_addr = ctrl_addr;
		memset(&ctrl_addr, 0, sizeof(ctrl_addr));
		ctrl_addr.su_family = AF_INET;
		ctrl_addr.su_len = sizeof(ctrl_addr.si_su.su_sin);
		memcpy(&ctrl_addr.su_addr, &tmp_addr.su_6addr.s6_addr[off],
		    sizeof(ctrl_addr.su_addr));
		ctrl_addr.su_port = tmp_addr.su_port;
#else
		while (fgets(line, sizeof(line), fd) != NULL) {
			if ((cp = strchr(line, '\n')) != NULL)
				*cp = '\0';
			reply(-530, "%s", line);
		}
		(void) fflush(stdout);
		(void) fclose(fd);
		reply(530,
		    "Connection from IPv4 mapped address is not supported.");
		exit(0);
#endif

		mapped = 1;
	} else
#endif /* INET6 */
		mapped = 0;
#ifdef IP_TOS
	if (!mapped && his_addr.su_family == AF_INET) {
		tos = IPTOS_LOWDELAY;
		if (setsockopt(0, IPPROTO_IP, IP_TOS, (char *)&tos,
			       sizeof(int)) < 0)
			syslog(LOG_WARNING, "setsockopt (IP_TOS): %m");
	}
#endif
	/* if the hostname hasn't been given, attempt to determine it */ 
	if (hostname[0] == '\0') {
		if (getnameinfo((struct sockaddr *)&ctrl_addr.si_su,
		    ctrl_addr.su_len, hostname, sizeof(hostname), NULL, 0, 
			getnameopts) != 0)
			(void)gethostname(hostname, sizeof(hostname));
		hostname[sizeof(hostname) - 1] = '\0';
	}

	/* set this here so klogin can use it... */
	(void)snprintf(ttyline, sizeof(ttyline), "ftp%d", getpid());

	(void) freopen(_PATH_DEVNULL, "w", stderr);

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	sa.sa_flags = SA_RESTART;
	sigemptyset(&sa.sa_mask);
	(void) sigaction(SIGCHLD, &sa, NULL);

	sa.sa_handler = sigquit;
	sa.sa_flags = SA_RESTART;
	sigfillset(&sa.sa_mask);	/* block all sigs in these handlers */
	(void) sigaction(SIGHUP, &sa, NULL);
	(void) sigaction(SIGINT, &sa, NULL);
	(void) sigaction(SIGQUIT, &sa, NULL);
	(void) sigaction(SIGTERM, &sa, NULL);
	sa.sa_handler = lostconn;
	(void) sigaction(SIGPIPE, &sa, NULL);
	sa.sa_handler = toolong;
	(void) sigaction(SIGALRM, &sa, NULL);
	sa.sa_handler = sigurg;
#if !defined(__minix)
	(void) sigaction(SIGURG, &sa, NULL);

	/* Try to handle urgent data inline */
#ifdef SO_OOBINLINE
	if (setsockopt(0, SOL_SOCKET, SO_OOBINLINE, (char *)&on, sizeof(on)) < 0)
		syslog(LOG_WARNING, "setsockopt: %m");
#endif
	/* Set keepalives on the socket to detect dropped connections.  */
#ifdef SO_KEEPALIVE
	keepalive = 1;
	if (setsockopt(0, SOL_SOCKET, SO_KEEPALIVE, (char *)&keepalive,
	    sizeof(int)) < 0)
		syslog(LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");
#endif
#endif /* !defined(__minix) */

#ifdef	F_SETOWN
	if (fcntl(fileno(stdin), F_SETOWN, getpid()) == -1)
		syslog(LOG_WARNING, "fcntl F_SETOWN: %m");
#endif
	logremotehost(&his_addr);
	/*
	 * Set up default state
	 */
	data = -1;
	type = TYPE_A;
	form = FORM_N;
	stru = STRU_F;
	mode = MODE_S;
	tmpline[0] = '\0';
	hasyyerrored = 0;

#ifdef KERBEROS5
	kerror = krb5_init_context(&kcontext);
	if (kerror) {
		syslog(LOG_ERR, "%s when initializing Kerberos context",
		    error_message(kerror));
		exit(0);
	}
#endif /* KERBEROS5 */

	init_curclass();
	curclass.timeout = 300;		/* 5 minutes, as per login(1) */
	curclass.type = CLASS_REAL;

	/* If logins are disabled, print out the message. */
	if (display_file(_PATH_NOLOGIN, 530)) {
		reply(530, "System not available.");
		exit(0);
	}
	(void)display_file(conffilename(_NAME_FTPWELCOME), 220);
		/* reply(220,) must follow */
	if (EMPTYSTR(version))
		reply(220, "%s FTP server ready.", hostname);
	else
		reply(220, "%s FTP server (%s) ready.", hostname, version);

	if (xferlogname != NULL) {
		xferlogfd = open(xferlogname, O_WRONLY | O_APPEND | O_CREAT,
		    0660);
		if (xferlogfd == -1)
			syslog(LOG_WARNING, "open xferlog `%s': %m",
			    xferlogname);
		else
			doxferlog |= 2;
	}

	ftp_loop();
	/* NOTREACHED */
}

static void
lostconn(int signo __unused)
{

	if (ftpd_debug)
		syslog(LOG_DEBUG, "lost connection");
	dologout(1);
}

static void
toolong(int signo __unused)
{

		/* XXXSIGRACE */
	reply(421,
	    "Timeout (" LLF " seconds): closing control connection.",
	    (LLT)curclass.timeout);
	if (logging)
		syslog(LOG_INFO, "User %s timed out after " LLF " seconds",
		    (pw ? pw->pw_name : "unknown"), (LLT)curclass.timeout);
	dologout(1);
}

static void
sigquit(int signo)
{

	if (ftpd_debug)
		syslog(LOG_DEBUG, "got signal %d", signo);
	dologout(1);
}

static void
sigurg(int signo __unused)
{
	
	urgflag = 1;
}


/*
 * Save the result of a getpwnam.  Used for USER command, since
 * the data returned must not be clobbered by any other command
 * (e.g., globbing).
 */
static struct passwd *
sgetpwnam(const char *name)
{
	static struct passwd save;
	struct passwd *p;

	if ((p = getpwnam(name)) == NULL)
		return (p);
	if (save.pw_name) {
		free((char *)save.pw_name);
		memset(save.pw_passwd, 0, strlen(save.pw_passwd));
		free((char *)save.pw_passwd);
		free((char *)save.pw_gecos);
		free((char *)save.pw_dir);
		free((char *)save.pw_shell);
	}
	save = *p;
	save.pw_name = ftpd_strdup(p->pw_name);
	save.pw_passwd = ftpd_strdup(p->pw_passwd);
	save.pw_gecos = ftpd_strdup(p->pw_gecos);
	save.pw_dir = ftpd_strdup(p->pw_dir);
	save.pw_shell = ftpd_strdup(p->pw_shell);
	return (&save);
}

static int	login_attempts;	/* number of failed login attempts */
static int	askpasswd;	/* had USER command, ask for PASSwd */
static int	permitted;	/* USER permitted */

/*
 * USER command.
 * Sets global passwd pointer pw if named account exists and is acceptable;
 * sets askpasswd if a PASS command is expected.  If logged in previously,
 * need to reset state.  If name is "ftp" or "anonymous", the name is not in
 * _NAME_FTPUSERS, and ftp account exists, set guest and pw, then just return.
 * If account doesn't exist, ask for passwd anyway.  Otherwise, check user
 * requesting login privileges.  Disallow anyone who does not have a standard
 * shell as returned by getusershell().  Disallow anyone mentioned in the file
 * _NAME_FTPUSERS to allow people such as root and uucp to be avoided.
 */
void
user(const char *name)
{
	char	*class;
#ifdef	LOGIN_CAP
	login_cap_t *lc = NULL;
#endif
#ifdef USE_PAM
	int e;
#endif

	class = NULL;
	if (logged_in) {
		switch (curclass.type) {
		case CLASS_GUEST:
			reply(530, "Can't change user from guest login.");
			return;
		case CLASS_CHROOT:
			reply(530, "Can't change user from chroot user.");
			return;
		case CLASS_REAL:
			if (dropprivs) {
				reply(530, "Can't change user.");
				return;
			}
			end_login();
			break;
		default:
			abort();
		}
	}

#if defined(KERBEROS)
	kdestroy();
#endif
#if defined(KERBEROS5)
	k5destroy();
#endif

	curclass.type = CLASS_REAL;
	askpasswd = 0;
	permitted = 0;

	if (strcmp(name, "ftp") == 0 || strcmp(name, "anonymous") == 0) {
			/* need `pw' setup for checkaccess() and checkuser () */
		if ((pw = sgetpwnam("ftp")) == NULL)
			reply(530, "User %s unknown.", name);
		else if (! checkaccess("ftp") || ! checkaccess("anonymous"))
			reply(530, "User %s access denied.", name);
		else {
			curclass.type = CLASS_GUEST;
			askpasswd = 1;
			reply(331,
			    "Guest login ok, type your name as password.");
		}
		if (!askpasswd) {
			if (logging)
				syslog(LOG_NOTICE,
				    "ANONYMOUS FTP LOGIN REFUSED FROM %s",
				    remoteloghost);
			end_login();
			goto cleanup_user;
		}
		name = "ftp";
	} else
		pw = sgetpwnam(name);

	strlcpy(curname, name, curname_len);

			/* check user in /etc/ftpusers, and setup class */
	permitted = checkuser(_NAME_FTPUSERS, curname, 1, 0, &class);

			/* check user in /etc/ftpchroot */
#ifdef	LOGIN_CAP
	lc = login_getpwclass(pw);
#endif
	if (checkuser(_NAME_FTPCHROOT, curname, 0, 0, NULL)
#ifdef	LOGIN_CAP	/* Allow login.conf configuration as well */
	    || login_getcapbool(lc, "ftp-chroot", 0)
#endif
	) {
		if (curclass.type == CLASS_GUEST) {
			syslog(LOG_NOTICE,
	    "Can't change guest user to chroot class; remove entry in %s",
			    _NAME_FTPCHROOT);
			exit(1);
		}
		curclass.type = CLASS_CHROOT;
	}

			/* determine default class */
	if (class == NULL) {
		switch (curclass.type) {
		case CLASS_GUEST:
			class = ftpd_strdup("guest");
			break;
		case CLASS_CHROOT:
			class = ftpd_strdup("chroot");
			break;
		case CLASS_REAL:
			class = ftpd_strdup("real");
			break;
		default:
			syslog(LOG_ERR, "unknown curclass.type %d; aborting",
			    curclass.type);
			abort();
		}
	}
			/* parse ftpd.conf, setting up various parameters */
	parse_conf(class);
			/* if not guest user, check for valid shell */
	if (pw == NULL)
		permitted = 0;
	else {
		const char	*cp, *shell;

		if ((shell = pw->pw_shell) == NULL || *shell == 0)
			shell = _PATH_BSHELL;
		while ((cp = getusershell()) != NULL)
			if (strcmp(cp, shell) == 0)
				break;
		endusershell();
		if (cp == NULL && curclass.type != CLASS_GUEST)
			permitted = 0;
	}

			/* deny quickly (after USER not PASS) if requested */
	if (CURCLASS_FLAGS_ISSET(denyquick) && !permitted) {
		reply(530, "User %s may not use FTP.", curname);
		if (logging)
			syslog(LOG_NOTICE, "FTP LOGIN REFUSED FROM %s, %s",
			    remoteloghost, curname);
		end_login();
		goto cleanup_user;
	}

			/* if haven't asked yet (i.e, not anon), ask now */
	if (!askpasswd) {
		askpasswd = 1;
#ifdef USE_PAM
		e = auth_pam();		/* this does reply(331, ...) */
		do_pass(1, e, "");
		goto cleanup_user;
#else /* !USE_PAM */
#ifdef SKEY
		if (skey_haskey(curname) == 0) {
			const char *myskey;

			myskey = skey_keyinfo(curname);
			reply(331, "Password [ %s ] required for %s.",
			    myskey ? myskey : "error getting challenge",
			    curname);
		} else
#endif
			reply(331, "Password required for %s.", curname);
#endif /* !USE_PAM */
	}

 cleanup_user:
#ifdef LOGIN_CAP
	login_close(lc);
#endif
	/*
	 * Delay before reading passwd after first failed
	 * attempt to slow down passwd-guessing programs.
	 */
	if (login_attempts)
		sleep((unsigned) login_attempts);

	if (class)
		free(class);
}

/*
 * Determine whether something is to happen (allow access, chroot)
 * for a user. Each line is a shell-style glob followed by
 * `yes' or `no'.
 *
 * For backward compatibility, `allow' and `deny' are synonymns
 * for `yes' and `no', respectively.
 *
 * Each glob is matched against the username in turn, and the first
 * match found is used. If no match is found, the result is the
 * argument `def'. If a match is found but without and explicit
 * `yes'/`no', the result is the opposite of def.
 *
 * If the file doesn't exist at all, the result is the argument
 * `nofile'
 *
 * Any line starting with `#' is considered a comment and ignored.
 *
 * Returns 0 if the user is denied, or 1 if they are allowed.
 *
 * NOTE: needs struct passwd *pw setup before use.
 */
static int
checkuser(const char *fname, const char *name, int def, int nofile,
	    char **retclass)
{
	FILE	*fd;
	int	 retval;
	char	*word, *perm, *class, *buf, *p;
	size_t	 len, line;

	retval = def;
	if (retclass != NULL)
		*retclass = NULL;
	if ((fd = fopen(conffilename(fname), "r")) == NULL)
		return nofile;

	line = 0;
	for (;
	    (buf = fparseln(fd, &len, &line, NULL, FPARSELN_UNESCCOMM |
			    FPARSELN_UNESCCONT | FPARSELN_UNESCESC)) != NULL;
	    free(buf), buf = NULL) {
		word = perm = class = NULL;
		p = buf;
		if (len < 1)
			continue;
		if (p[len - 1] == '\n')
			p[--len] = '\0';
		if (EMPTYSTR(p))
			continue;

		NEXTWORD(p, word);
		NEXTWORD(p, perm);
		NEXTWORD(p, class);
		if (EMPTYSTR(word))
			continue;
		if (!EMPTYSTR(class)) {
			if (strcasecmp(class, "all") == 0 ||
			    strcasecmp(class, "none") == 0) {
				syslog(LOG_WARNING,
		"%s line %d: illegal user-defined class `%s' - skipping entry",
					    fname, (int)line, class);
				continue;
			}
		}

					/* have a host specifier */
		if ((p = strchr(word, '@')) != NULL) {
			unsigned char	net[16], mask[16], *addr;
			int		addrlen, bits, bytes, a;

			*p++ = '\0';
					/* check against network or CIDR */
			memset(net, 0x00, sizeof(net));
			if ((bits = inet_net_pton(his_addr.su_family, p, net,
			    sizeof(net))) != -1) {
#ifdef INET6
				if (his_addr.su_family == AF_INET) {
#endif
					addrlen = 4;
					addr = (unsigned char *)&his_addr.su_addr;
#ifdef INET6
				} else {
					addrlen = 16;
					addr = (unsigned char *)&his_addr.su_6addr;
				}
#endif
				bytes = bits / 8;
				bits = bits % 8;
				if (bytes > 0)
					memset(mask, 0xFF, bytes);
				if (bytes < addrlen)
					mask[bytes] = 0xFF << (8 - bits);
				if (bytes + 1 < addrlen)
					memset(mask + bytes + 1, 0x00,
					    addrlen - bytes - 1);
				for (a = 0; a < addrlen; a++)
					if ((addr[a] & mask[a]) != net[a])
						break;
				if (a < addrlen)
					continue;

					/* check against hostname glob */
			} else if (fnmatch(p, remotehost, FNM_CASEFOLD) != 0)
				continue;
		}

					/* have a group specifier */
		if ((p = strchr(word, ':')) != NULL) {
			gid_t	*groups, *ng;
			int	 gsize, i, found;

			if (pw == NULL)
				continue;	/* no match for unknown user */
			*p++ = '\0';
			groups = NULL;
			gsize = 16;
			do {
				ng = realloc(groups, gsize * sizeof(gid_t));
				if (ng == NULL)
					fatal(
					    "Local resource failure: realloc");
				groups = ng;
			} while (getgrouplist(pw->pw_name, pw->pw_gid,
						groups, &gsize) == -1);
			found = 0;
			for (i = 0; i < gsize; i++) {
				struct group *g;

				if ((g = getgrgid(groups[i])) == NULL)
					continue;
				if (fnmatch(p, g->gr_name, 0) == 0) {
					found = 1;
					break;
				}
			}
			free(groups);
			if (!found)
				continue;
		}

					/* check against username glob */
		if (fnmatch(word, name, 0) != 0)
			continue;

		if (perm != NULL &&
		    ((strcasecmp(perm, "allow") == 0) ||
		     (strcasecmp(perm, "yes") == 0)))
			retval = 1;
		else if (perm != NULL &&
		    ((strcasecmp(perm, "deny") == 0) ||
		     (strcasecmp(perm, "no") == 0)))
			retval = 0;
		else
			retval = !def;
		if (!EMPTYSTR(class) && retclass != NULL)
			*retclass = ftpd_strdup(class);
		free(buf);
		break;
	}
	(void) fclose(fd);
	return (retval);
}

/*
 * Check if user is allowed by /etc/ftpusers
 * returns 1 for yes, 0 for no
 *
 * NOTE: needs struct passwd *pw setup (for checkuser())
 */
static int
checkaccess(const char *name)
{

	return (checkuser(_NAME_FTPUSERS, name, 1, 0, NULL));
}

static void
login_utmp(const char *line, const char *name, const char *host,
    struct sockinet *haddr)
{
#if defined(SUPPORT_UTMPX) || defined(SUPPORT_UTMP)
	struct timeval tv;
	(void)gettimeofday(&tv, NULL);
#endif
#ifdef SUPPORT_UTMPX
	if (doutmp) {
		(void)memset(&utmpx, 0, sizeof(utmpx));
		utmpx.ut_tv = tv;
		utmpx.ut_pid = getpid();
		utmpx.ut_id[0] = 'f';
		utmpx.ut_id[1] = 't';
		utmpx.ut_id[2] = 'p';
		utmpx.ut_id[3] = '*';
		utmpx.ut_type = USER_PROCESS;
		(void)strncpy(utmpx.ut_name, name, sizeof(utmpx.ut_name));
		(void)strncpy(utmpx.ut_line, line, sizeof(utmpx.ut_line));
		(void)strncpy(utmpx.ut_host, host, sizeof(utmpx.ut_host));
		(void)memcpy(&utmpx.ut_ss, &haddr->si_su, haddr->su_len);
		ftpd_loginx(&utmpx);
	}
	if (dowtmp)
		ftpd_logwtmpx(line, name, host, haddr, 0, USER_PROCESS);
#endif
#ifdef SUPPORT_UTMP
	if (doutmp) {
		(void)memset(&utmp, 0, sizeof(utmp));
		(void)time(&utmp.ut_time);
		(void)strncpy(utmp.ut_name, name, sizeof(utmp.ut_name));
		(void)strncpy(utmp.ut_line, line, sizeof(utmp.ut_line));
		(void)strncpy(utmp.ut_host, host, sizeof(utmp.ut_host));
		ftpd_login(&utmp);
	}
	if (dowtmp)
		ftpd_logwtmp(line, name, host);
#endif
}

static void
logout_utmp(void)
{
#ifdef SUPPORT_UTMPX
	int okwtmpx = dowtmp;
#endif
#ifdef SUPPORT_UTMP
	int okwtmp = dowtmp;
#endif
	if (logged_in) {
#ifdef SUPPORT_UTMPX
		if (doutmp)
			okwtmpx &= ftpd_logoutx(ttyline, 0, DEAD_PROCESS);
		if (okwtmpx)
			ftpd_logwtmpx(ttyline, "", "", NULL, 0, DEAD_PROCESS);
#endif
#ifdef SUPPORT_UTMP
		if (doutmp)
			okwtmp &= ftpd_logout(ttyline);
		if (okwtmp)
			ftpd_logwtmp(ttyline, "", "");
#endif
	}
}

/*
 * Terminate login as previous user (if any), resetting state;
 * used when USER command is given or login fails.
 */
static void
end_login(void)
{
#ifdef USE_PAM
	int e;
#endif
	logout_utmp();
	show_chdir_messages(-1);		/* flush chdir cache */
	if (pw != NULL && pw->pw_passwd != NULL)
		memset(pw->pw_passwd, 0, strlen(pw->pw_passwd));
	pw = NULL;
	logged_in = 0;
	askpasswd = 0;
	permitted = 0;
	quietmessages = 0;
	gidcount = 0;
	curclass.type = CLASS_REAL;
	(void) seteuid((uid_t)0);
#ifdef	LOGIN_CAP
	setusercontext(NULL, getpwuid(0), 0,
		       LOGIN_SETPRIORITY|LOGIN_SETRESOURCES|LOGIN_SETUMASK);
#endif
#ifdef USE_PAM
	if (pamh) {
		if ((e = pam_setcred(pamh, PAM_DELETE_CRED)) != PAM_SUCCESS)
			syslog(LOG_ERR, "pam_setcred: %s",
			    pam_strerror(pamh, e));
		if ((e = pam_close_session(pamh,0)) != PAM_SUCCESS)
			syslog(LOG_ERR, "pam_close_session: %s",
			    pam_strerror(pamh, e));
		if ((e = pam_end(pamh, e)) != PAM_SUCCESS)
			syslog(LOG_ERR, "pam_end: %s", pam_strerror(pamh, e));
		pamh = NULL;
	}
#endif
}

void
pass(const char *passwd)
{
	do_pass(0, 0, passwd);
}

/*
 * Perform the passwd confirmation and login.
 *
 * If pass_checked is zero, confirm passwd is correct, & ignore pass_rval.
 * This is the traditional PASS implementation.
 *
 * If pass_checked is non-zero, use pass_rval and ignore passwd.
 * This is used by auth_pam() which has already parsed PASS.
 * This only applies to curclass.type != CLASS_GUEST.
 */
static void
do_pass(int pass_checked, int pass_rval, const char *passwd)
{
	int		 rval;
	char		 root[MAXPATHLEN];
#ifdef	LOGIN_CAP
	login_cap_t *lc = NULL;
#endif
#ifdef USE_PAM
	int e;
#endif

	rval = 1;

	if (logged_in || askpasswd == 0) {
		reply(503, "Login with USER first.");
		return;
	}
	askpasswd = 0;
	if (curclass.type != CLASS_GUEST) {
			/* "ftp" is the only account allowed with no password */
		if (pw == NULL) {
			rval = 1;	/* failure below */
			goto skip;
		}
		if (pass_checked) {	/* password validated in user() */
			rval = pass_rval;
			goto skip;
		}
#ifdef USE_PAM
		syslog(LOG_ERR, "do_pass: USE_PAM shouldn't get here");
		rval = 1;
		goto skip;
#endif
#if defined(KERBEROS)
		if (klogin(pw, "", hostname, (char *)passwd) == 0) {
			rval = 0;
			goto skip;
		}
#endif
#if defined(KERBEROS5)
		if (k5login(pw, "", hostname, (char *)passwd) == 0) {
			rval = 0;
			goto skip;
		}
#endif
#ifdef SKEY
		if (skey_haskey(pw->pw_name) == 0) {
			char *p;
			int r;

			p = ftpd_strdup(passwd);
			r = skey_passcheck(pw->pw_name, p);
			free(p);
			if (r != -1) {
				rval = 0;
				goto skip;
			}
		}
#endif
		if (!sflag)
			rval = checkpassword(pw, passwd);
		else
			rval = 1;

 skip:

			/*
			 * If rval > 0, the user failed the authentication check
			 * above.  If rval == 0, either Kerberos or local
			 * authentication succeeded.
			 */
		if (rval) {
			reply(530, "%s", rval == 2 ? "Password expired." :
			    "Login incorrect.");
			pfilter_notify(1, rval == 2 ? "exppass" : "badpass");
			if (logging) {
				syslog(LOG_NOTICE,
				    "FTP LOGIN FAILED FROM %s", remoteloghost);
				syslog(LOG_AUTHPRIV | LOG_NOTICE,
				    "FTP LOGIN FAILED FROM %s, %s",
				    remoteloghost, curname);
			}
			pw = NULL;
			if (login_attempts++ >= 5) {
				syslog(LOG_NOTICE,
				    "repeated login failures from %s",
				    remoteloghost);
				exit(0);
			}
			return;
		}
	}

			/* password ok; check if anything else prevents login */
	if (! permitted) {
		reply(530, "User %s may not use FTP.", pw->pw_name);
		if (logging)
			syslog(LOG_NOTICE, "FTP LOGIN REFUSED FROM %s, %s",
			    remoteloghost, pw->pw_name);
		goto bad;
	}

	login_attempts = 0;		/* this time successful */
	if (setegid((gid_t)pw->pw_gid) < 0) {
		reply(550, "Can't set gid.");
		goto bad;
	}
#ifdef	LOGIN_CAP
	if ((lc = login_getpwclass(pw)) != NULL) {
#ifdef notyet
		char	remote_ip[NI_MAXHOST];

		if (getnameinfo((struct sockaddr *)&his_addr, his_addr.su_len,
			remote_ip, sizeof(remote_ip) - 1, NULL, 0,
			NI_NUMERICHOST))
				*remote_ip = 0;
		remote_ip[sizeof(remote_ip) - 1] = 0;
		if (!auth_hostok(lc, remotehost, remote_ip)) {
			pfilter_notify(1, "bannedhost");
			syslog(LOG_INFO|LOG_AUTH,
			    "FTP LOGIN FAILED (HOST) as %s: permission denied.",
			    pw->pw_name);
			reply(530, "Permission denied.");
			pw = NULL;
			return;
		}
		if (!auth_timeok(lc, time(NULL))) {
			reply(530, "Login not available right now.");
			pw = NULL;
			return;
		}
#endif
	}
	setsid();
	setusercontext(lc, pw, 0,
		LOGIN_SETLOGIN|LOGIN_SETGROUP|LOGIN_SETPRIORITY|
		LOGIN_SETRESOURCES|LOGIN_SETUMASK);
#else
	(void) initgroups(pw->pw_name, pw->pw_gid);
			/* cache groups for cmds.c::matchgroup() */
#endif
#ifdef USE_PAM
	if (pamh) {
		if ((e = pam_open_session(pamh, 0)) != PAM_SUCCESS) {
			syslog(LOG_ERR, "pam_open_session: %s",
			    pam_strerror(pamh, e));
		} else if ((e = pam_setcred(pamh, PAM_ESTABLISH_CRED))
		    != PAM_SUCCESS) {
			syslog(LOG_ERR, "pam_setcred: %s",
			    pam_strerror(pamh, e));
		}
	}
#endif
	gidcount = getgroups(0, NULL);
	if (gidlist)
		free(gidlist);
	gidlist = malloc(gidcount * sizeof *gidlist);
	gidcount = getgroups(gidcount, gidlist);

	/* open utmp/wtmp before chroot */
	login_utmp(ttyline, pw->pw_name, remotehost, &his_addr);

	logged_in = 1;

	connections = 1;
	if (dopidfile)
		count_users();
	if (curclass.limit != -1 && connections > curclass.limit) {
		if (! EMPTYSTR(curclass.limitfile))
			(void)display_file(conffilename(curclass.limitfile),
			    530);
		reply(530,
		    "User %s access denied, connection limit of " LLF
		    " reached.",
		    pw->pw_name, (LLT)curclass.limit);
		syslog(LOG_NOTICE,
		    "Maximum connection limit of " LLF
		    " for class %s reached, login refused for %s",
		    (LLT)curclass.limit, curclass.classname, pw->pw_name);
		goto bad;
	}

	homedir[0] = '/';
	switch (curclass.type) {
	case CLASS_GUEST:
			/*
			 * We MUST do a chdir() after the chroot. Otherwise
			 * the old current directory will be accessible as "."
			 * outside the new root!
			 */
		format_path(root,
		    curclass.chroot ? curclass.chroot :
		    anondir ? anondir :
		    pw->pw_dir);
		format_path(homedir,
		    curclass.homedir ? curclass.homedir :
		    "/");
		if (EMPTYSTR(homedir))
			homedir[0] = '/';
		if (EMPTYSTR(root) || chroot(root) < 0) {
			syslog(LOG_NOTICE,
			    "GUEST user %s: can't chroot to %s: %m",
			    pw->pw_name, root);
			goto bad_guest;
		}
		if (chdir(homedir) < 0) {
			syslog(LOG_NOTICE,
			    "GUEST user %s: can't chdir to %s: %m",
			    pw->pw_name, homedir);
 bad_guest:
			reply(550, "Can't set guest privileges.");
			goto bad;
		}
		break;
	case CLASS_CHROOT:
		format_path(root,
		    curclass.chroot ? curclass.chroot :
		    pw->pw_dir);
		format_path(homedir,
		    curclass.homedir ? curclass.homedir :
		    "/");
		if (EMPTYSTR(homedir))
			homedir[0] = '/';
		if (EMPTYSTR(root) || chroot(root) < 0) {
			syslog(LOG_NOTICE,
			    "CHROOT user %s: can't chroot to %s: %m",
			    pw->pw_name, root);
			goto bad_chroot;
		}
		if (chdir(homedir) < 0) {
			syslog(LOG_NOTICE,
			    "CHROOT user %s: can't chdir to %s: %m",
			    pw->pw_name, homedir);
 bad_chroot:
			reply(550, "Can't change root.");
			goto bad;
		}
		break;
	case CLASS_REAL:
			/* only chroot REAL if explicitly requested */
		if (! EMPTYSTR(curclass.chroot)) {
			format_path(root, curclass.chroot);
			if (EMPTYSTR(root) || chroot(root) < 0) {
				syslog(LOG_NOTICE,
				    "REAL user %s: can't chroot to %s: %m",
				    pw->pw_name, root);
				goto bad_chroot;
			}
		}
		format_path(homedir,
		    curclass.homedir ? curclass.homedir :
		    pw->pw_dir);
		if (EMPTYSTR(homedir) || chdir(homedir) < 0) {
			if (chdir("/") < 0) {
				syslog(LOG_NOTICE,
				    "REAL user %s: can't chdir to %s: %m",
				    pw->pw_name,
				    !EMPTYSTR(homedir) ?  homedir : "/");
				reply(530,
				    "User %s: can't change directory to %s.",
				    pw->pw_name,
				    !EMPTYSTR(homedir) ? homedir : "/");
				goto bad;
			} else {
				reply(-230,
				    "No directory! Logging in with home=/");
				homedir[0] = '/';
			}
		}
		break;
	}
#ifndef LOGIN_CAP
	setsid();
#if !defined(__minix)
	setlogin(pw->pw_name);
#endif /* !defined(__minix) */
#endif
	if (dropprivs ||
	    (curclass.type != CLASS_REAL && 
	    ntohs(ctrl_addr.su_port) > IPPORT_RESERVED + 1)) {
		dropprivs++;
		if (setgid((gid_t)pw->pw_gid) < 0) {
			reply(550, "Can't set gid.");
			goto bad;
		}
		if (setuid((uid_t)pw->pw_uid) < 0) {
			reply(550, "Can't set uid.");
			goto bad;
		}
	} else {
		if (seteuid((uid_t)pw->pw_uid) < 0) {
			reply(550, "Can't set uid.");
			goto bad;
		}
	}
	setenv("HOME", homedir, 1);

	if (curclass.type == CLASS_GUEST && passwd[0] == '-')
		quietmessages = 1;

			/*
			 * Display a login message, if it exists.
			 * N.B. reply(230,) must follow the message.
			 */
	if (! EMPTYSTR(curclass.motd))
		(void)display_file(conffilename(curclass.motd), 230);
	show_chdir_messages(230);
	if (curclass.type == CLASS_GUEST) {
		char *p;

		reply(230, "Guest login ok, access restrictions apply.");
#if defined(HAVE_SETPROCTITLE)
		snprintf(proctitle, sizeof(proctitle),
		    "%s: anonymous/%s", remotehost, passwd);
		setproctitle("%s", proctitle);
#endif /* defined(HAVE_SETPROCTITLE) */
		if (logging)
			syslog(LOG_INFO,
			"ANONYMOUS FTP LOGIN FROM %s, %s (class: %s, type: %s)",
			    remoteloghost, passwd,
			    curclass.classname, CURCLASSTYPE);
			/* store guest password reply into pw_passwd */
		REASSIGN(pw->pw_passwd, ftpd_strdup(passwd));
		for (p = pw->pw_passwd; *p; p++)
			if (!isgraph((unsigned char)*p))
				*p = '_';
	} else {
		reply(230, "User %s logged in.", pw->pw_name);
#if defined(HAVE_SETPROCTITLE)
		snprintf(proctitle, sizeof(proctitle),
		    "%s: %s", remotehost, pw->pw_name);
		setproctitle("%s", proctitle);
#endif /* defined(HAVE_SETPROCTITLE) */
		if (logging)
			syslog(LOG_INFO,
			    "FTP LOGIN FROM %s as %s (class: %s, type: %s)",
			    remoteloghost, pw->pw_name,
			    curclass.classname, CURCLASSTYPE);
	}
	(void) umask(curclass.umask);
#ifdef	LOGIN_CAP
	login_close(lc);
#endif
	return;

 bad:
#ifdef	LOGIN_CAP
	login_close(lc);
#endif
			/* Forget all about it... */
	end_login();
}

void
retrieve(const char *argv[], const char *name)
{
	FILE *fin, *dout;
	struct stat st;
	int (*closefunc)(FILE *) = NULL;
	int dolog, sendrv, closerv, stderrfd, isconversion, isdata, isls;
	struct timeval start, finish, td, *tdp;
	struct rusage rusage_before, rusage_after;
	const char *dispname;
	const char *error;

	sendrv = closerv = stderrfd = -1;
	isconversion = isdata = isls = dolog = 0;
	tdp = NULL;
	dispname = name;
	fin = dout = NULL;
	error = NULL;
	if (argv == NULL) {		/* if not running a command ... */
		dolog = 1;
		isdata = 1;
		fin = fopen(name, "r");
		closefunc = fclose;
		if (fin == NULL)	/* doesn't exist?; try a conversion */
			argv = do_conversion(name);
		if (argv != NULL) {
			isconversion++;
			syslog(LOG_DEBUG, "get command: '%s' on '%s'",
			    argv[0], name);
		}
	}
	if (argv != NULL) {
		char temp[MAXPATHLEN];

		if (strcmp(argv[0], INTERNAL_LS) == 0) {
			isls = 1;
			stderrfd = -1;
		} else {
			(void)snprintf(temp, sizeof(temp), "%s", TMPFILE);
			stderrfd = mkstemp(temp);
			if (stderrfd != -1)
				(void)unlink(temp);
		}
		dispname = argv[0];
		fin = ftpd_popen(argv, "r", stderrfd);
		closefunc = ftpd_pclose;
		st.st_size = -1;
		st.st_blksize = BUFSIZ;
	}
	if (fin == NULL) {
		if (errno != 0) {
			perror_reply(550, dispname);
			if (dolog)
				logxfer("get", -1, name, NULL, NULL,
				    strerror(errno));
		}
		goto cleanupretrieve;
	}
	byte_count = -1;
	if (argv == NULL
	    && (fstat(fileno(fin), &st) < 0 || !S_ISREG(st.st_mode))) {
		error = "Not a plain file";
		reply(550, "%s: %s.", dispname, error);
		goto done;
	}
	if (restart_point) {
		if (type == TYPE_A) {
			off_t i;
			int c;

			for (i = 0; i < restart_point; i++) {
				if ((c=getc(fin)) == EOF) {
					error = strerror(errno);
					perror_reply(550, dispname);
					goto done;
				}
				if (c == '\n')
					i++;
			}
		} else if (lseek(fileno(fin), restart_point, SEEK_SET) < 0) {
			error = strerror(errno);
			perror_reply(550, dispname);
			goto done;
		}
	}
	dout = dataconn(dispname, st.st_size, "w");
	if (dout == NULL)
		goto done;

	(void)getrusage(RUSAGE_SELF, &rusage_before);
	(void)gettimeofday(&start, NULL);
	sendrv = send_data(fin, dout, &st, isdata);
	(void)gettimeofday(&finish, NULL);
	(void)getrusage(RUSAGE_SELF, &rusage_after);
	closedataconn(dout);		/* close now to affect timing stats */
	timersub(&finish, &start, &td);
	tdp = &td;
 done:
	if (dolog) {
		logxfer("get", byte_count, name, NULL, tdp, error);
		if (tdp != NULL)
			logrusage(&rusage_before, &rusage_after);
	}
	closerv = (*closefunc)(fin);
	if (sendrv == 0) {
		FILE *errf;
		struct stat sb;

		if (!isls && argv != NULL && closerv != 0) {
			reply(-226,
			    "Command returned an exit status of %d",
			    closerv);
			if (isconversion)
				syslog(LOG_WARNING,
				    "retrieve command: '%s' returned %d",
				    argv[0], closerv);
		}
		if (!isls && argv != NULL && stderrfd != -1 &&
		    (fstat(stderrfd, &sb) == 0) && sb.st_size > 0 &&
		    ((errf = fdopen(stderrfd, "r")) != NULL)) {
			char *cp, line[LINE_MAX];

			reply(-226, "Command error messages:");
			rewind(errf);
			while (fgets(line, sizeof(line), errf) != NULL) {
				if ((cp = strchr(line, '\n')) != NULL)
					*cp = '\0';
				reply(0, "  %s", line);
			}
			(void) fflush(stdout);
			(void) fclose(errf);
				/* a reply(226,) must follow */
		}
		reply(226, "Transfer complete.");
	}
 cleanupretrieve:
	if (stderrfd != -1)
		(void)close(stderrfd);
	if (isconversion)
		free(argv);
}

void
store(const char *name, const char *fmode, int unique)
{
	FILE *fout, *din;
	struct stat st;
	int (*closefunc)(FILE *);
	struct timeval start, finish, td, *tdp;
	const char *desc, *error;

	din = NULL;
	desc = (*fmode == 'w') ? "put" : "append";
	error = NULL;
	if (unique && stat(name, &st) == 0 &&
	    (name = gunique(name)) == NULL) {
		logxfer(desc, -1, name, NULL, NULL,
		    "cannot create unique file");
		goto cleanupstore;
	}

	if (restart_point)
		fmode = "r+";
	fout = fopen(name, fmode);
	closefunc = fclose;
	tdp = NULL;
	if (fout == NULL) {
		perror_reply(553, name);
		logxfer(desc, -1, name, NULL, NULL, strerror(errno));
		goto cleanupstore;
	}
	byte_count = -1;
	if (restart_point) {
		if (type == TYPE_A) {
			off_t i;
			int c;

			for (i = 0; i < restart_point; i++) {
				if ((c=getc(fout)) == EOF) {
					error = strerror(errno);
					perror_reply(550, name);
					goto done;
				}
				if (c == '\n')
					i++;
			}
			/*
			 * We must do this seek to "current" position
			 * because we are changing from reading to
			 * writing.
			 */
			if (fseek(fout, 0L, SEEK_CUR) < 0) {
				error = strerror(errno);
				perror_reply(550, name);
				goto done;
			}
		} else if (lseek(fileno(fout), restart_point, SEEK_SET) < 0) {
			error = strerror(errno);
			perror_reply(550, name);
			goto done;
		}
	}
	din = dataconn(name, (off_t)-1, "r");
	if (din == NULL)
		goto done;
	(void)gettimeofday(&start, NULL);
	if (receive_data(din, fout) == 0) {
		if (unique)
			reply(226, "Transfer complete (unique file name:%s).",
			    name);
		else
			reply(226, "Transfer complete.");
	}
	(void)gettimeofday(&finish, NULL);
	closedataconn(din);		/* close now to affect timing stats */
	timersub(&finish, &start, &td);
	tdp = &td;
 done:
	logxfer(desc, byte_count, name, NULL, tdp, error);
	(*closefunc)(fout);
 cleanupstore:
	;
}

static FILE *
getdatasock(const char *fmode)
{
	int		on, s, t, tries;
	in_port_t	port;

	on = 1;
	if (data >= 0)
		return (fdopen(data, fmode));
	if (! dropprivs)
		(void) seteuid((uid_t)0);
	s = socket(ctrl_addr.su_family, SOCK_STREAM, 0);
	if (s < 0)
		goto bad;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
	    (char *) &on, sizeof(on)) < 0)
		goto bad;
	if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE,
	    (char *) &on, sizeof(on)) < 0)
		goto bad;
			/* anchor socket to avoid multi-homing problems */
	data_source = ctrl_addr;
			/*
			 * By default source port for PORT connctions is
			 * ctrlport-1 (see RFC959 section 5.2).
			 * However, if privs have been dropped and that
			 * would be < IPPORT_RESERVED, use a random port
			 * instead.
			 */
	if (dataport)
		port = dataport;
	else
		port = ntohs(ctrl_addr.su_port) - 1;
	if (dropprivs && port < IPPORT_RESERVED)
		port = 0;		/* use random port */
	data_source.su_port = htons(port);

	for (tries = 1; ; tries++) {
		if (bind(s, (struct sockaddr *)&data_source.si_su,
		    data_source.su_len) >= 0)
			break;
		if (errno != EADDRINUSE || tries > 10)
			goto bad;
		sleep(tries);
	}
	if (! dropprivs)
		(void) seteuid((uid_t)pw->pw_uid);
#ifdef IP_TOS
	if (!mapped && ctrl_addr.su_family == AF_INET) {
		on = IPTOS_THROUGHPUT;
		if (setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&on,
			       sizeof(int)) < 0)
			syslog(LOG_WARNING, "setsockopt (IP_TOS): %m");
	}
#endif
	return (fdopen(s, fmode));
 bad:
		/* Return the real value of errno (close may change it) */
	t = errno;
	if (! dropprivs)
		(void) seteuid((uid_t)pw->pw_uid);
	(void) close(s);
	errno = t;
	return (NULL);
}

FILE *
dataconn(const char *name, off_t size, const char *fmode)
{
	char sizebuf[32];
	FILE *file;
	int retry, tos, keepalive, conerrno;

	file_size = size;
	byte_count = 0;
	if (size != (off_t) -1)
		(void)snprintf(sizebuf, sizeof(sizebuf), " (" LLF " byte%s)",
		    (LLT)size, PLURAL(size));
	else
		sizebuf[0] = '\0';
	if (pdata >= 0) {
		struct sockinet from;
		int s;
		socklen_t fromlen = sizeof(from.su_len);

		(void) alarm(curclass.timeout);
		s = accept(pdata, (struct sockaddr *)&from.si_su, &fromlen);
		(void) alarm(0);
		if (s < 0) {
			reply(425, "Can't open data connection.");
			(void) close(pdata);
			pdata = -1;
			return (NULL);
		}
		(void) close(pdata);
		pdata = s;
#if !defined(__minix)
		switch (from.su_family) {
		case AF_INET:
#ifdef IP_TOS
			if (!mapped) {
				tos = IPTOS_THROUGHPUT;
				(void) setsockopt(s, IPPROTO_IP, IP_TOS,
				    (char *)&tos, sizeof(int));
			}
			break;
#endif
		}
#endif /* !defined(__minix) */
		/* Set keepalives on the socket to detect dropped conns. */
#ifdef SO_KEEPALIVE
		keepalive = 1;
		(void) setsockopt(s, SOL_SOCKET, SO_KEEPALIVE,
		    (char *)&keepalive, sizeof(int));
#endif
		reply(150, "Opening %s mode data connection for '%s'%s.",
		     type == TYPE_A ? "ASCII" : "BINARY", name, sizebuf);
		return (fdopen(pdata, fmode));
	}
	if (data >= 0) {
		reply(125, "Using existing data connection for '%s'%s.",
		    name, sizebuf);
		usedefault = 1;
		return (fdopen(data, fmode));
	}
	if (usedefault)
		data_dest = his_addr;
	usedefault = 1;
	retry = conerrno = 0;
	do {
		file = getdatasock(fmode);
		if (file == NULL) {
			char hbuf[NI_MAXHOST];
			char pbuf[NI_MAXSERV];

			if (getnameinfo((struct sockaddr *)&data_source.si_su,
			    data_source.su_len, hbuf, sizeof(hbuf), pbuf,
			    sizeof(pbuf), NI_NUMERICHOST | NI_NUMERICSERV))
				strlcpy(hbuf, "?", sizeof(hbuf));
			reply(425, "Can't create data socket (%s,%s): %s.",
			      hbuf, pbuf, strerror(errno));
			return (NULL);
		}
		data = fileno(file);
		conerrno = 0;
		if (connect(data, (struct sockaddr *)&data_dest.si_su,
		    data_dest.su_len) == 0)
			break;
		conerrno = errno;
		(void) fclose(file);
		file = NULL;
		data = -1;
		if (conerrno == EADDRINUSE) {
			sleep((unsigned) swaitint);
			retry += swaitint;
		} else {
			break;
		}
	} while (retry <= swaitmax);
	if (conerrno != 0) {
		perror_reply(425, "Can't build data connection");
		return (NULL);
	}
	reply(150, "Opening %s mode data connection for '%s'%s.",
	     type == TYPE_A ? "ASCII" : "BINARY", name, sizebuf);
	return (file);
}

void
closedataconn(FILE *fd)
{

	if (fd == NULL)
		return;
	(void)fclose(fd);
	data = -1;
	if (pdata >= 0)
		(void)close(pdata);
	pdata = -1;
}

int
write_data(int fd, char *buf, size_t size, off_t *bufrem,
    struct timeval *then, int isdata)
{
	struct timeval now, td;
	ssize_t c;

	while (size > 0) {
		c = size;
		if (curclass.writesize) {
			if (curclass.writesize < c)
				c = curclass.writesize;
		}
		if (curclass.rateget) {
			if (*bufrem < c)
				c = *bufrem;
		}
		(void) alarm(curclass.timeout);
		c = write(fd, buf, c);
		if (c <= 0)
			return (1);
		buf += c;
		size -= c;
		byte_count += c;
		if (isdata) {
			total_data_out += c;
			total_data += c;
		}
		total_bytes_out += c;
		total_bytes += c;
		if (curclass.rateget) {
			*bufrem -= c;
			if (*bufrem == 0) {
				(void)gettimeofday(&now, NULL);
				timersub(&now, then, &td);
				if (td.tv_sec == 0) {
					usleep(1000000 - td.tv_usec);
					(void)gettimeofday(then, NULL);
				} else
					*then = now;
				*bufrem = curclass.rateget;
			}
		}
	}
	return (0);
}

static enum send_status
send_data_with_read(int filefd, int netfd, const struct stat *st, int isdata)
{
	struct timeval then;
	off_t bufrem;
	ssize_t readsize;
	char *buf;
	int c, error;

	if (curclass.readsize > 0)
		readsize = curclass.readsize;
	else
		readsize = st->st_blksize;
	if ((buf = malloc(readsize)) == NULL) {
		perror_reply(451, "Local resource failure: malloc");
		return (SS_NO_TRANSFER);
	}

	if (curclass.rateget) {
		bufrem = curclass.rateget;
		(void)gettimeofday(&then, NULL);
	} else
		bufrem = readsize;
	for (;;) {
		(void) alarm(curclass.timeout);
		c = read(filefd, buf, readsize);
		if (c == 0)
			error = SS_SUCCESS;
		else if (c < 0)
			error = SS_FILE_ERROR;
		else if (write_data(netfd, buf, c, &bufrem, &then, isdata))
			error = SS_DATA_ERROR;
		else if (urgflag && handleoobcmd())
			error = SS_ABORTED;
		else
			continue;

		free(buf);
		return (error);
	}
}

static enum send_status
send_data_with_mmap(int filefd, int netfd, const struct stat *st, int isdata)
{
	struct timeval then;
	off_t bufrem, filesize, off, origoff;
	ssize_t mapsize, winsize;
	int error, sendbufsize, sendlowat;
	void *win;

	bufrem = 0;
	if (curclass.sendbufsize) {
		sendbufsize = curclass.sendbufsize;
		if (setsockopt(netfd, SOL_SOCKET, SO_SNDBUF,
		    &sendbufsize, sizeof(int)) == -1)
			syslog(LOG_WARNING, "setsockopt(SO_SNDBUF, %d): %m",
			    sendbufsize);
	}

	if (curclass.sendlowat) {
		sendlowat = curclass.sendlowat;
		if (setsockopt(netfd, SOL_SOCKET, SO_SNDLOWAT,
		    &sendlowat, sizeof(int)) == -1)
			syslog(LOG_WARNING, "setsockopt(SO_SNDLOWAT, %d): %m",
			    sendlowat);
	}

	winsize = curclass.mmapsize;
	filesize = st->st_size;
	if (ftpd_debug)
		syslog(LOG_INFO, "mmapsize = " LLF ", writesize = " LLF,
		    (LLT)winsize, (LLT)curclass.writesize);
	if (winsize <= 0)
		goto try_read;

	off = lseek(filefd, (off_t)0, SEEK_CUR);
	if (off == -1)
		goto try_read;

	origoff = off;
	if (curclass.rateget) {
		bufrem = curclass.rateget;
		(void)gettimeofday(&then, NULL);
	} else
		bufrem = winsize;
	while (1) {
		mapsize = MIN(filesize - off, winsize);
		if (mapsize == 0)
			break;
		win = mmap(NULL, mapsize, PROT_READ,
		    MAP_FILE|MAP_SHARED, filefd, off);
		if (win == MAP_FAILED) {
			if (off == origoff)
				goto try_read;
			return (SS_FILE_ERROR);
		}
#if !defined(__minix)
		(void) madvise(win, mapsize, MADV_SEQUENTIAL);
#endif /* !defined(__minix) */
		error = write_data(netfd, win, mapsize, &bufrem, &then,
		    isdata);
#if !defined(__minix)
		(void) madvise(win, mapsize, MADV_DONTNEED);
#endif /* !defined(__minix) */
		munmap(win, mapsize);
		if (urgflag && handleoobcmd())
			return (SS_ABORTED);
		if (error)
			return (SS_DATA_ERROR);
		off += mapsize;
	}
	return (SS_SUCCESS);

 try_read:
	return (send_data_with_read(filefd, netfd, st, isdata));
}

/*
 * Transfer the contents of "instr" to "outstr" peer using the appropriate
 * encapsulation of the data subject to Mode, Structure, and Type.
 *
 * NB: Form isn't handled.
 */
static int
send_data(FILE *instr, FILE *outstr, const struct stat *st, int isdata)
{
	int	 c, filefd, netfd, rval;

	urgflag = 0;
	transflag = 1;
	rval = -1;

	switch (type) {

	case TYPE_A:
 /* XXXLUKEM: rate limit ascii send (get) */
		(void) alarm(curclass.timeout);
		while ((c = getc(instr)) != EOF) {
			if (urgflag && handleoobcmd())
				goto cleanup_send_data;
			byte_count++;
			if (c == '\n') {
				if (ferror(outstr))
					goto data_err;
				(void) putc('\r', outstr);
				if (isdata) {
					total_data_out++;
					total_data++;
				}
				total_bytes_out++;
				total_bytes++;
			}
			(void) putc(c, outstr);
			if (isdata) {
				total_data_out++;
				total_data++;
			}
			total_bytes_out++;
			total_bytes++;
			if ((byte_count % 4096) == 0)
				(void) alarm(curclass.timeout);
		}
		(void) alarm(0);
		fflush(outstr);
		if (ferror(instr))
			goto file_err;
		if (ferror(outstr))
			goto data_err;
		rval = 0;
		goto cleanup_send_data;

	case TYPE_I:
	case TYPE_L:
		filefd = fileno(instr);
		netfd = fileno(outstr);
		switch (send_data_with_mmap(filefd, netfd, st, isdata)) {

		case SS_SUCCESS:
			break;

		case SS_ABORTED:
		case SS_NO_TRANSFER:
			goto cleanup_send_data;

		case SS_FILE_ERROR:
			goto file_err;

		case SS_DATA_ERROR:
			goto data_err;
		}
		rval = 0;
		goto cleanup_send_data;

	default:
		reply(550, "Unimplemented TYPE %d in send_data", type);
		goto cleanup_send_data;
	}

 data_err:
	(void) alarm(0);
	perror_reply(426, "Data connection");
	goto cleanup_send_data;

 file_err:
	(void) alarm(0);
	perror_reply(551, "Error on input file");
	goto cleanup_send_data;

 cleanup_send_data:
	(void) alarm(0);
	transflag = 0;
	urgflag = 0;
	if (isdata) {
		total_files_out++;
		total_files++;
	}
	total_xfers_out++;
	total_xfers++;
	return (rval);
}

/*
 * Transfer data from peer to "outstr" using the appropriate encapulation of
 * the data subject to Mode, Structure, and Type.
 *
 * N.B.: Form isn't handled.
 */
static int
receive_data(FILE *instr, FILE *outstr)
{
	int	c, netfd, filefd, rval;
	int	volatile bare_lfs;
	off_t	byteswritten;
	char	*buf;
	ssize_t	readsize;
	struct sigaction sa, sa_saved;
	struct stat st;

	memset(&sa, 0, sizeof(sa));
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = lostconn;
	(void) sigaction(SIGALRM, &sa, &sa_saved);

	bare_lfs = 0;
	urgflag = 0;
	transflag = 1;
	rval = -1;
	byteswritten = 0;
	buf = NULL;

#define FILESIZECHECK(x) \
			do { \
				if (curclass.maxfilesize != -1 && \
				    (x) > curclass.maxfilesize) { \
					errno = EFBIG; \
					goto file_err; \
				} \
			} while (0)

	switch (type) {

	case TYPE_I:
	case TYPE_L:
		netfd = fileno(instr);
		filefd = fileno(outstr);
		(void) alarm(curclass.timeout);
		if (curclass.readsize)
			readsize = curclass.readsize;
		else if (fstat(filefd, &st) != -1)
			readsize = (ssize_t)st.st_blksize;
		else
			readsize = BUFSIZ;
		if ((buf = malloc(readsize)) == NULL) {
			perror_reply(451, "Local resource failure: malloc");
			goto cleanup_recv_data;
		}
		if (curclass.rateput) {
			while (1) {
				int d;
				struct timeval then, now, td;
				off_t bufrem;

				(void)gettimeofday(&then, NULL);
				errno = c = d = 0;
				for (bufrem = curclass.rateput; bufrem > 0; ) {
					if ((c = read(netfd, buf,
					    MIN(readsize, bufrem))) <= 0)
						goto recvdone;
					if (urgflag && handleoobcmd())
						goto cleanup_recv_data;
					FILESIZECHECK(byte_count + c);
					if ((d = write(filefd, buf, c)) != c)
						goto file_err;
					(void) alarm(curclass.timeout);
					bufrem -= c;
					byte_count += c;
					total_data_in += c;
					total_data += c;
					total_bytes_in += c;
					total_bytes += c;
				}
				(void)gettimeofday(&now, NULL);
				timersub(&now, &then, &td);
				if (td.tv_sec == 0)
					usleep(1000000 - td.tv_usec);
			}
		} else {
			while ((c = read(netfd, buf, readsize)) > 0) {
				if (urgflag && handleoobcmd())
					goto cleanup_recv_data;
				FILESIZECHECK(byte_count + c);
				if (write(filefd, buf, c) != c)
					goto file_err;
				(void) alarm(curclass.timeout);
				byte_count += c;
				total_data_in += c;
				total_data += c;
				total_bytes_in += c;
				total_bytes += c;
			}
		}
 recvdone:
		if (c < 0)
			goto data_err;
		rval = 0;
		goto cleanup_recv_data;

	case TYPE_E:
		reply(553, "TYPE E not implemented.");
		goto cleanup_recv_data;

	case TYPE_A:
		(void) alarm(curclass.timeout);
 /* XXXLUKEM: rate limit ascii receive (put) */
		while ((c = getc(instr)) != EOF) {
			if (urgflag && handleoobcmd())
				goto cleanup_recv_data;
			byte_count++;
			total_data_in++;
			total_data++;
			total_bytes_in++;
			total_bytes++;
			if ((byte_count % 4096) == 0)
				(void) alarm(curclass.timeout);
			if (c == '\n')
				bare_lfs++;
			while (c == '\r') {
				if (ferror(outstr))
					goto data_err;
				if ((c = getc(instr)) != '\n') {
					byte_count++;
					total_data_in++;
					total_data++;
					total_bytes_in++;
					total_bytes++;
					if ((byte_count % 4096) == 0)
						(void) alarm(curclass.timeout);
					byteswritten++;
					FILESIZECHECK(byteswritten);
					(void) putc ('\r', outstr);
					if (c == '\0' || c == EOF)
						goto contin2;
				}
			}
			byteswritten++;
			FILESIZECHECK(byteswritten);
			(void) putc(c, outstr);
 contin2:	;
		}
		(void) alarm(0);
		fflush(outstr);
		if (ferror(instr))
			goto data_err;
		if (ferror(outstr))
			goto file_err;
		if (bare_lfs) {
			reply(-226,
			    "WARNING! %d bare linefeeds received in ASCII mode",
			    bare_lfs);
			reply(0, "File may not have transferred correctly.");
		}
		rval = 0;
		goto cleanup_recv_data;

	default:
		reply(550, "Unimplemented TYPE %d in receive_data", type);
		goto cleanup_recv_data;
	}
#undef FILESIZECHECK

 data_err:
	(void) alarm(0);
	perror_reply(426, "Data Connection");
	goto cleanup_recv_data;

 file_err:
	(void) alarm(0);
	perror_reply(452, "Error writing file");
	goto cleanup_recv_data;

 cleanup_recv_data:
	(void) alarm(0);
	(void) sigaction(SIGALRM, &sa_saved, NULL);
	if (buf)
		free(buf);
	transflag = 0;
	urgflag = 0;
	total_files_in++;
	total_files++;
	total_xfers_in++;
	total_xfers++;
	return (rval);
}

void
statcmd(void)
{
	struct sockinet *su = NULL;
	static char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	unsigned char *a, *p;
	int ispassive, af;
	off_t otbi, otbo, otb;

	a = p = NULL;

	reply(-211, "%s FTP server status:", hostname);
	reply(0, "Version: %s", EMPTYSTR(version) ? "<suppressed>" : version);
	hbuf[0] = '\0';
	if (!getnameinfo((struct sockaddr *)&his_addr.si_su, his_addr.su_len,
			hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST)
	    && strcmp(remotehost, hbuf) != 0)
		reply(0, "Connected to %s (%s)", remotehost, hbuf);
	else
		reply(0, "Connected to %s", remotehost);

	if (logged_in) {
		if (curclass.type == CLASS_GUEST)
			reply(0, "Logged in anonymously");
		else
			reply(0, "Logged in as %s%s", pw->pw_name,
			    curclass.type == CLASS_CHROOT ? " (chroot)" : "");
	} else if (askpasswd)
		reply(0, "Waiting for password");
	else
		reply(0, "Waiting for user name");
	cprintf(stdout, "    TYPE: %s", typenames[type]);
	if (type == TYPE_A || type == TYPE_E)
		cprintf(stdout, ", FORM: %s", formnames[form]);
	if (type == TYPE_L) {
#if NBBY == 8
		cprintf(stdout, " %d", NBBY);
#else
			/* XXX: `bytesize' needs to be defined in this case */
		cprintf(stdout, " %d", bytesize);
#endif
	}
	cprintf(stdout, "; STRUcture: %s; transfer MODE: %s\r\n",
	    strunames[stru], modenames[mode]);
	ispassive = 0;
	if (data != -1) {
		reply(0, "Data connection open");
		su = NULL;
	} else if (pdata != -1) {
		reply(0, "in Passive mode");
		if (curclass.advertise.su_len != 0)
			su = &curclass.advertise;
		else
			su = &pasv_addr;
		ispassive = 1;
		goto printaddr;
	} else if (usedefault == 0) {
		su = (struct sockinet *)&data_dest;

		if (epsvall) {
			reply(0, "EPSV only mode (EPSV ALL)");
			goto epsvonly;
		}
 printaddr:
							/* PASV/PORT */
		if (su->su_family == AF_INET) {
			a = (unsigned char *) &su->su_addr;
			p = (unsigned char *) &su->su_port;
#define UC(b) (((int) b) & 0xff)
			reply(0, "%s (%d,%d,%d,%d,%d,%d)",
				ispassive ? "PASV" : "PORT" ,
				UC(a[0]), UC(a[1]), UC(a[2]), UC(a[3]),
				UC(p[0]), UC(p[1]));
		}

							/* LPSV/LPRT */
	    {
		int alen, i;

		alen = 0;
		switch (su->su_family) {
		case AF_INET:
			a = (unsigned char *) &su->su_addr;
			p = (unsigned char *) &su->su_port;
			alen = sizeof(su->su_addr);
			af = 4;
			break;
#ifdef INET6
		case AF_INET6:
			a = (unsigned char *) &su->su_6addr;
			p = (unsigned char *) &su->su_port;
			alen = sizeof(su->su_6addr);
			af = 6;
			break;
#endif
		default:
			af = 0;
			break;
		}
		if (af) {
			cprintf(stdout, "    %s (%d,%d",
			    ispassive ? "LPSV" : "LPRT", af, alen);
			for (i = 0; i < alen; i++)
				cprintf(stdout, ",%d", UC(a[i]));
			cprintf(stdout, ",%d,%d,%d)\r\n",
			    2, UC(p[0]), UC(p[1]));
#undef UC
		}
	    }

		/* EPRT/EPSV */
 epsvonly:
		af = af2epsvproto(su->su_family);
		hbuf[0] = '\0';
		if (af > 0) {
			struct sockinet tmp;

			tmp = *su;
#ifdef INET6
			if (tmp.su_family == AF_INET6)
				tmp.su_scope_id = 0;
#endif
			if (getnameinfo((struct sockaddr *)&tmp.si_su,
			    tmp.su_len, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf),
			    NI_NUMERICHOST | NI_NUMERICSERV) == 0)
				reply(0, "%s (|%d|%s|%s|)",
				    ispassive ? "EPSV" : "EPRT",
				    af, hbuf, sbuf);
		}
	} else
		reply(0, "No data connection");

	if (logged_in) {
		reply(0,
		    "Data sent:        " LLF " byte%s in " LLF " file%s",
		    (LLT)total_data_out, PLURAL(total_data_out),
		    (LLT)total_files_out, PLURAL(total_files_out));
		reply(0,
		    "Data received:    " LLF " byte%s in " LLF " file%s",
		    (LLT)total_data_in, PLURAL(total_data_in),
		    (LLT)total_files_in, PLURAL(total_files_in));
		reply(0,
		    "Total data:       " LLF " byte%s in " LLF " file%s",
		    (LLT)total_data, PLURAL(total_data),
		    (LLT)total_files, PLURAL(total_files));
	}
	otbi = total_bytes_in;
	otbo = total_bytes_out;
	otb = total_bytes;
	reply(0, "Traffic sent:     " LLF " byte%s in " LLF " transfer%s",
	    (LLT)otbo, PLURAL(otbo),
	    (LLT)total_xfers_out, PLURAL(total_xfers_out));
	reply(0, "Traffic received: " LLF " byte%s in " LLF " transfer%s",
	    (LLT)otbi, PLURAL(otbi),
	    (LLT)total_xfers_in, PLURAL(total_xfers_in));
	reply(0, "Total traffic:    " LLF " byte%s in " LLF " transfer%s",
	    (LLT)otb, PLURAL(otb),
	    (LLT)total_xfers, PLURAL(total_xfers));

	if (logged_in && !CURCLASS_FLAGS_ISSET(private)) {
		struct ftpconv *cp;

		reply(0, "%s", "");
		reply(0, "Class: %s, type: %s",
		    curclass.classname, CURCLASSTYPE);
		reply(0, "Check PORT/LPRT commands: %sabled",
		    CURCLASS_FLAGS_ISSET(checkportcmd) ? "en" : "dis");
		if (! EMPTYSTR(curclass.display))
			reply(0, "Display file: %s", curclass.display);
		if (! EMPTYSTR(curclass.notify))
			reply(0, "Notify fileglob: %s", curclass.notify);
		reply(0, "Idle timeout: " LLF ", maximum timeout: " LLF,
		    (LLT)curclass.timeout, (LLT)curclass.maxtimeout);
		reply(0, "Current connections: %d", connections);
		if (curclass.limit == -1)
			reply(0, "Maximum connections: unlimited");
		else
			reply(0, "Maximum connections: " LLF,
			    (LLT)curclass.limit);
		if (curclass.limitfile)
			reply(0, "Connection limit exceeded message file: %s",
			    conffilename(curclass.limitfile));
		if (! EMPTYSTR(curclass.chroot))
			reply(0, "Chroot format: %s", curclass.chroot);
		reply(0, "Deny bad ftpusers(5) quickly: %sabled",
		    CURCLASS_FLAGS_ISSET(denyquick) ? "en" : "dis");
		if (! EMPTYSTR(curclass.homedir))
			reply(0, "Homedir format: %s", curclass.homedir);
		if (curclass.maxfilesize == -1)
			reply(0, "Maximum file size: unlimited");
		else
			reply(0, "Maximum file size: " LLF,
			    (LLT)curclass.maxfilesize);
		if (! EMPTYSTR(curclass.motd))
			reply(0, "MotD file: %s", conffilename(curclass.motd));
		reply(0,
	    "Modify commands (CHMOD, DELE, MKD, RMD, RNFR, UMASK): %sabled",
		    CURCLASS_FLAGS_ISSET(modify) ? "en" : "dis");
		reply(0, "Upload commands (APPE, STOR, STOU): %sabled",
		    CURCLASS_FLAGS_ISSET(upload) ? "en" : "dis");
		reply(0, "Sanitize file names: %sabled",
		    CURCLASS_FLAGS_ISSET(sanenames) ? "en" : "dis");
		reply(0, "PASV/LPSV/EPSV connections: %sabled",
		    CURCLASS_FLAGS_ISSET(passive) ? "en" : "dis");
		if (curclass.advertise.su_len != 0) {
			char buf[50];	/* big enough for IPv6 address */
			const char *bp;

			bp = inet_ntop(curclass.advertise.su_family,
			    (void *)&curclass.advertise.su_addr,
			    buf, sizeof(buf));
			if (bp != NULL)
				reply(0, "PASV advertise address: %s", bp);
		}
		if (curclass.portmin && curclass.portmax)
			reply(0, "PASV port range: " LLF " - " LLF,
			    (LLT)curclass.portmin, (LLT)curclass.portmax);
		if (curclass.rateget)
			reply(0, "Rate get limit: " LLF " bytes/sec",
			    (LLT)curclass.rateget);
		else
			reply(0, "Rate get limit: disabled");
		if (curclass.rateput)
			reply(0, "Rate put limit: " LLF " bytes/sec",
			    (LLT)curclass.rateput);
		else
			reply(0, "Rate put limit: disabled");
		if (curclass.mmapsize)
			reply(0, "Mmap size: " LLF, (LLT)curclass.mmapsize);
		else
			reply(0, "Mmap size: disabled");
		if (curclass.readsize)
			reply(0, "Read size: " LLF, (LLT)curclass.readsize);
		else
			reply(0, "Read size: default");
		if (curclass.writesize)
			reply(0, "Write size: " LLF, (LLT)curclass.writesize);
		else
			reply(0, "Write size: default");
		if (curclass.recvbufsize)
			reply(0, "Receive buffer size: " LLF,
			    (LLT)curclass.recvbufsize);
		else
			reply(0, "Receive buffer size: default");
		if (curclass.sendbufsize)
			reply(0, "Send buffer size: " LLF,
			    (LLT)curclass.sendbufsize);
		else
			reply(0, "Send buffer size: default");
		if (curclass.sendlowat)
			reply(0, "Send low water mark: " LLF,
			    (LLT)curclass.sendlowat);
		else
			reply(0, "Send low water mark: default");
		reply(0, "Umask: %.04o", curclass.umask);
		for (cp = curclass.conversions; cp != NULL; cp=cp->next) {
			if (cp->suffix == NULL || cp->types == NULL ||
			    cp->command == NULL)
				continue;
			reply(0, "Conversion: %s [%s] disable: %s, command: %s",
			    cp->suffix, cp->types, cp->disable, cp->command);
		}
	}

	reply(211, "End of status");
}

void
fatal(const char *s)
{

	reply(451, "Error in server: %s\n", s);
	reply(221, "Closing connection due to server error.");
	dologout(0);
	/* NOTREACHED */
}

/*
 * reply() --
 *	depending on the value of n, display fmt with a trailing CRLF and
 *	prefix of:
 *	n < -1		prefix the message with abs(n) + "-"	(initial line)
 *	n == 0		prefix the message with 4 spaces	(middle lines)
 *	n >  0		prefix the message with n + " "		(final line)
 */
void
reply(int n, const char *fmt, ...)
{
	char	msg[MAXPATHLEN * 2 + 100];
	size_t	b;
	va_list	ap;

	if (n == 0)
		b = snprintf(msg, sizeof(msg), "    ");
	else if (n < 0)
		b = snprintf(msg, sizeof(msg), "%d-", -n);
	else
		b = snprintf(msg, sizeof(msg), "%d ", n);
	va_start(ap, fmt);
	vsnprintf(msg + b, sizeof(msg) - b, fmt, ap);
	va_end(ap);
	cprintf(stdout, "%s\r\n", msg);
	(void)fflush(stdout);
	if (ftpd_debug)
		syslog(LOG_DEBUG, "<--- %s", msg);
}

static void
logremotehost(struct sockinet *who)
{

#if defined(HAVE_SOCKADDR_SNPRINTF)
	char abuf[BUFSIZ];
#endif

	struct sockaddr *sa = (struct sockaddr *)&who->si_su;
	if (getnameinfo(sa, who->su_len, remotehost, sizeof(remotehost), NULL,
	    0, getnameopts))
		strlcpy(remotehost, "?", sizeof(remotehost));
#if defined(HAVE_SOCKADDR_SNPRINTF)
	sockaddr_snprintf(abuf, sizeof(abuf), "%a", sa);
	snprintf(remoteloghost, sizeof(remoteloghost), "%s(%s)", remotehost,
	    abuf);
#else
	strlcpy(remoteloghost, remotehost, sizeof(remoteloghost));
#endif
	
#if defined(HAVE_SETPROCTITLE)
	snprintf(proctitle, sizeof(proctitle), "%s: connected", remotehost);
	setproctitle("%s", proctitle);
#endif /* defined(HAVE_SETPROCTITLE) */
	if (logging)
		syslog(LOG_INFO, "connection from %s to %s",
		    remoteloghost, hostname);
}

/*
 * Record logout in wtmp file and exit with supplied status.
 * NOTE: because this is called from signal handlers it cannot
 *       use stdio (or call other functions that use stdio).
 */
void
dologout(int status)
{
	/*
	* Prevent reception of SIGURG from resulting in a resumption
	* back to the main program loop.
	*/
	transflag = 0;
	logout_utmp();
	if (logged_in) {
#ifdef KERBEROS
		if (!notickets && krbtkfile_env)
			unlink(krbtkfile_env);
#endif
	}
	/* beware of flushing buffers after a SIGPIPE */
	if (xferlogfd != -1)
		close(xferlogfd);
	_exit(status);
}

void
abor(void)
{

	if (!transflag)
		return;
	tmpline[0] = '\0';
	is_oob = 0;
	reply(426, "Transfer aborted. Data connection closed.");
	reply(226, "Abort successful");
	transflag = 0;		/* flag that the transfer has aborted */
}

void
statxfer(void)
{

	if (!transflag)
		return;
	tmpline[0] = '\0';
	is_oob = 0;
	if (file_size != (off_t) -1)
		reply(213,
		    "Status: " LLF " of " LLF " byte%s transferred",
		    (LLT)byte_count, (LLT)file_size,
		    PLURAL(byte_count));
	else
		reply(213, "Status: " LLF " byte%s transferred",
		    (LLT)byte_count, PLURAL(byte_count));
}

/*
 * Call when urgflag != 0 to handle Out Of Band commands.
 * Returns non zero if the OOB command aborted the transfer
 * by setting transflag to 0. (c.f., "ABOR").
 */
static int
handleoobcmd(void)
{
	char *cp;
	int ret;

	if (!urgflag)
		return (0);
	urgflag = 0;
	/* only process if transfer occurring */
	if (!transflag)
		return (0);
	cp = tmpline;
	ret = get_line(cp, sizeof(tmpline)-1, stdin);
	if (ret == -1) {
		reply(221, "You could at least say goodbye.");
		dologout(0);
	} else if (ret == -2) {
		/* Ignore truncated command */
		/* XXX: abort xfer with "500 command too long", & return 1 ? */
		return 0;
	}
		/*
		 * Manually parse OOB commands, because we can't
		 * recursively call the yacc parser...
		 */
	if (strcasecmp(cp, "ABOR\r\n") == 0) {
		abor();
	} else if (strcasecmp(cp, "STAT\r\n") == 0) {
		statxfer();
	} else {
		/* XXX: error with "500 unknown command" ? */
	}
	return (transflag == 0);
}

static int
bind_pasv_addr(void)
{
	static int passiveport;
	int port, len;

	len = pasv_addr.su_len;
	if (curclass.portmin == 0 && curclass.portmax == 0) {
		pasv_addr.su_port = 0;
		return (bind(pdata, (struct sockaddr *)&pasv_addr.si_su, len));
	}

	if (passiveport == 0) {
		srand(getpid());
		passiveport = rand() % (curclass.portmax - curclass.portmin)
		    + curclass.portmin;
	}

	port = passiveport;
	while (1) {
		port++;
		if (port > curclass.portmax)
			port = curclass.portmin;
		else if (port == passiveport) {
			errno = EAGAIN;
			return (-1);
		}
		pasv_addr.su_port = htons(port);
		if (bind(pdata, (struct sockaddr *)&pasv_addr.si_su, len) == 0)
			break;
		if (errno != EADDRINUSE)
			return (-1);
	}
	passiveport = port;
	return (0);
}

/*
 * Note: a response of 425 is not mentioned as a possible response to
 *	the PASV command in RFC959. However, it has been blessed as
 *	a legitimate response by Jon Postel in a telephone conversation
 *	with Rick Adams on 25 Jan 89.
 */
void
passive(void)
{
	socklen_t len;
	int recvbufsize;
	char *p, *a;

	if (pdata >= 0)
		close(pdata);
	pdata = socket(AF_INET, SOCK_STREAM, 0);
	if (pdata < 0 || !logged_in) {
		perror_reply(425, "Can't open passive connection");
		return;
	}
	pasv_addr = ctrl_addr;

	if (bind_pasv_addr() < 0)
		goto pasv_error;
	len = pasv_addr.su_len;
	if (getsockname(pdata, (struct sockaddr *) &pasv_addr.si_su, &len) < 0)
		goto pasv_error;
	pasv_addr.su_len = len;
	if (curclass.recvbufsize) {
		recvbufsize = curclass.recvbufsize;
		if (setsockopt(pdata, SOL_SOCKET, SO_RCVBUF, &recvbufsize,
			       sizeof(int)) == -1)
			syslog(LOG_WARNING, "setsockopt(SO_RCVBUF, %d): %m",
			       recvbufsize);
	}
	if (listen(pdata, 1) < 0)
		goto pasv_error;
	if (curclass.advertise.su_len != 0)
		a = (char *) &curclass.advertise.su_addr;
	else
		a = (char *) &pasv_addr.su_addr;
	p = (char *) &pasv_addr.su_port;

#define UC(b) (((int) b) & 0xff)

	reply(227, "Entering Passive Mode (%d,%d,%d,%d,%d,%d)", UC(a[0]),
		UC(a[1]), UC(a[2]), UC(a[3]), UC(p[0]), UC(p[1]));
	return;

 pasv_error:
	(void) close(pdata);
	pdata = -1;
	perror_reply(425, "Can't open passive connection");
	return;
}

/*
 * convert protocol identifier to/from AF
 */
int
lpsvproto2af(int proto)
{

	switch (proto) {
	case 4:
		return AF_INET;
#ifdef INET6
	case 6:
		return AF_INET6;
#endif
	default:
		return -1;
	}
}

int
af2lpsvproto(int af)
{

	switch (af) {
	case AF_INET:
		return 4;
#ifdef INET6
	case AF_INET6:
		return 6;
#endif
	default:
		return -1;
	}
}

int
epsvproto2af(int proto)
{

	switch (proto) {
	case 1:
		return AF_INET;
#ifdef INET6
	case 2:
		return AF_INET6;
#endif
	default:
		return -1;
	}
}

int
af2epsvproto(int af)
{

	switch (af) {
	case AF_INET:
		return 1;
#ifdef INET6
	case AF_INET6:
		return 2;
#endif
	default:
		return -1;
	}
}

/*
 * 228 Entering Long Passive Mode (af, hal, h1, h2, h3,..., pal, p1, p2...)
 * 229 Entering Extended Passive Mode (|||port|)
 */
void
long_passive(const char *cmd, int pf)
{
	socklen_t len;
	char *p, *a;

	if (!logged_in) {
		syslog(LOG_NOTICE, "long passive but not logged in");
		reply(503, "Login with USER first.");
		return;
	}

	if (pf != PF_UNSPEC && ctrl_addr.su_family != pf) {
		/*
		 * XXX: only EPRT/EPSV ready clients will understand this
		 */
		if (strcmp(cmd, "EPSV") != 0)
			reply(501, "Network protocol mismatch"); /*XXX*/
		else
			epsv_protounsupp("Network protocol mismatch");

		return;
	}
 
	if (pdata >= 0)
		close(pdata);
	pdata = socket(ctrl_addr.su_family, SOCK_STREAM, 0);
	if (pdata < 0) {
		perror_reply(425, "Can't open passive connection");
		return;
	}
	pasv_addr = ctrl_addr;
	if (bind_pasv_addr() < 0)
		goto pasv_error;
	len = pasv_addr.su_len;
	if (getsockname(pdata, (struct sockaddr *) &pasv_addr.si_su, &len) < 0)
		goto pasv_error;
	pasv_addr.su_len = len;
	if (listen(pdata, 1) < 0)
		goto pasv_error;
	p = (char *) &pasv_addr.su_port;

#define UC(b) (((int) b) & 0xff)

	if (strcmp(cmd, "LPSV") == 0) {
		struct sockinet *advert;

		if (curclass.advertise.su_len != 0)
			advert = &curclass.advertise;
		else
			advert = &pasv_addr;
		switch (advert->su_family) {
		case AF_INET:
			a = (char *) &advert->su_addr;
			reply(228,
    "Entering Long Passive Mode (%d,%d,%d,%d,%d,%d,%d,%d,%d)",
				4, 4, UC(a[0]), UC(a[1]), UC(a[2]), UC(a[3]),
				2, UC(p[0]), UC(p[1]));
			return;
#ifdef INET6
		case AF_INET6:
			a = (char *) &advert->su_6addr;
			reply(228,
    "Entering Long Passive Mode (%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d)",
				6, 16,
				UC(a[0]), UC(a[1]), UC(a[2]), UC(a[3]),
				UC(a[4]), UC(a[5]), UC(a[6]), UC(a[7]),
				UC(a[8]), UC(a[9]), UC(a[10]), UC(a[11]),
				UC(a[12]), UC(a[13]), UC(a[14]), UC(a[15]),
				2, UC(p[0]), UC(p[1]));
			return;
#endif
		}
#undef UC
	} else if (strcmp(cmd, "EPSV") == 0) {
		switch (pasv_addr.su_family) {
		case AF_INET:
#ifdef INET6
		case AF_INET6:
#endif
			reply(229, "Entering Extended Passive Mode (|||%d|)",
			    ntohs(pasv_addr.su_port));
			return;
		}
	} else {
		/* more proper error code? */
	}

 pasv_error:
	(void) close(pdata);
	pdata = -1;
	perror_reply(425, "Can't open passive connection");
	return;
}

int
extended_port(const char *arg)
{
	char *tmp = NULL;
	char *result[3];
	char *p, *q;
	char delim;
	struct addrinfo hints;
	struct addrinfo *res = NULL;
	int i;
	unsigned long proto;

	tmp = ftpd_strdup(arg);
	p = tmp;
	delim = p[0];
	p++;
	memset(result, 0, sizeof(result));
	for (i = 0; i < 3; i++) {
		q = strchr(p, delim);
		if (!q || *q != delim)
			goto parsefail;
		*q++ = '\0';
		result[i] = p;
		p = q;
	}

			/* some more sanity checks */
	errno = 0;
	p = NULL;
	(void)strtoul(result[2], &p, 10);
	if (errno || !*result[2] || *p)
		goto parsefail;
	errno = 0;
	p = NULL;
	proto = strtoul(result[0], &p, 10);
	if (errno || !*result[0] || *p)
		goto protounsupp;
	 
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = epsvproto2af((int)proto);
	if (hints.ai_family < 0)
		goto protounsupp;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICHOST;
	if (getaddrinfo(result[1], result[2], &hints, &res))
		goto parsefail;
	if (res->ai_next)
		goto parsefail;
	if (sizeof(data_dest) < res->ai_addrlen)
		goto parsefail;
	memcpy(&data_dest.si_su, res->ai_addr, res->ai_addrlen);
	data_dest.su_len = res->ai_addrlen;
#ifdef INET6
	if (his_addr.su_family == AF_INET6 &&
	    data_dest.su_family == AF_INET6) {
			/* XXX: more sanity checks! */
		data_dest.su_scope_id = his_addr.su_scope_id;
	}
#endif

	if (tmp != NULL)
		free(tmp);
	if (res)
		freeaddrinfo(res);
	return 0;

 parsefail:
	reply(500, "Invalid argument, rejected.");
	usedefault = 1;
	if (tmp != NULL)
		free(tmp);
	if (res)
		freeaddrinfo(res);
	return -1;

 protounsupp:
	epsv_protounsupp("Protocol not supported");
	usedefault = 1;
	if (tmp != NULL)
		free(tmp);
	return -1;
}

/*
 * 522 Protocol not supported (proto,...)
 * as we assume address family for control and data connections are the same,
 * we do not return the list of address families we support - instead, we
 * return the address family of the control connection.
 */
void
epsv_protounsupp(const char *message)
{
	int proto;

	proto = af2epsvproto(ctrl_addr.su_family);
	if (proto < 0)
		reply(501, "%s", message);	/* XXX */
	else
		reply(522, "%s, use (%d)", message, proto);
}

/*
 * Generate unique name for file with basename "local".
 * The file named "local" is already known to exist.
 * Generates failure reply on error.
 *
 * XXX:	this function should under go changes similar to
 *	the mktemp(3)/mkstemp(3) changes.
 */
static char *
gunique(const char *local)
{
	static char new[MAXPATHLEN];
	struct stat st;
	char *cp;
	int count;

	cp = strrchr(local, '/');
	if (cp)
		*cp = '\0';
	if (stat(cp ? local : ".", &st) < 0) {
		perror_reply(553, cp ? local : ".");
		return (NULL);
	}
	if (cp)
		*cp = '/';
	for (count = 1; count < 100; count++) {
		(void)snprintf(new, sizeof(new) - 1, "%s.%d", local, count);
		if (stat(new, &st) < 0)
			return (new);
	}
	reply(452, "Unique file name cannot be created.");
	return (NULL);
}

/*
 * Format and send reply containing system error number.
 */
void
perror_reply(int code, const char *string)
{
	int save_errno;

	save_errno = errno;
	reply(code, "%s: %s.", string, strerror(errno));
	errno = save_errno;
}

static char *onefile[] = {
	NULL,
	0
};

void
send_file_list(const char *whichf)
{
	struct stat st;
	DIR *dirp;
	struct dirent *dir;
	FILE *volatile dout;
	char **volatile dirlist;
	char *dirname, *p;
	char *notglob;
	int volatile simple;
	int volatile freeglob;
	glob_t gl;

	dirp = NULL;
	dout = NULL;
	notglob = NULL;
	simple = 0;
	freeglob = 0;
	urgflag = 0;

	p = NULL;
	if (strpbrk(whichf, "~{[*?") != NULL) {
		int flags = GLOB_BRACE|GLOB_NOCHECK|GLOB_TILDE|GLOB_LIMIT;

		memset(&gl, 0, sizeof(gl));
		freeglob = 1;
		if (glob(whichf, flags, 0, &gl)) {
			reply(450, "Not found");
			goto cleanup_send_file_list;
		} else if (gl.gl_pathc == 0) {
			errno = ENOENT;
			perror_reply(450, whichf);
			goto cleanup_send_file_list;
		}
		dirlist = gl.gl_pathv;
	} else {
		notglob = ftpd_strdup(whichf);
		onefile[0] = notglob;
		dirlist = onefile;
		simple = 1;
	}
					/* XXX: } for vi sm */

	while ((dirname = *dirlist++) != NULL) {
		int trailingslash = 0;

		if (stat(dirname, &st) < 0) {
			/*
			 * If user typed "ls -l", etc, and the client
			 * used NLST, do what the user meant.
			 */
			/* XXX: nuke this support? */
			if (dirname[0] == '-' && *dirlist == NULL &&
			    transflag == 0) {
				const char *argv[] = { INTERNAL_LS, "", NULL };

				argv[1] = dirname;
				retrieve(argv, dirname);
				goto cleanup_send_file_list;
			}
			perror_reply(450, whichf);
			goto cleanup_send_file_list;
		}

		if (S_ISREG(st.st_mode)) {
			/*
			 * XXXRFC:
			 *	should we follow RFC959 and not work
			 *	for non directories?
			 */
			if (dout == NULL) {
				dout = dataconn("file list", (off_t)-1, "w");
				if (dout == NULL)
					goto cleanup_send_file_list;
				transflag = 1;
			}
			cprintf(dout, "%s%s\n", dirname,
			    type == TYPE_A ? "\r" : "");
			continue;
		} else if (!S_ISDIR(st.st_mode))
			continue;

		if (dirname[strlen(dirname) - 1] == '/')
			trailingslash++;

		if ((dirp = opendir(dirname)) == NULL)
			continue;

		while ((dir = readdir(dirp)) != NULL) {
			char nbuf[MAXPATHLEN];

			if (urgflag && handleoobcmd()) {
				(void) closedir(dirp);
				goto cleanup_send_file_list;
			}

			if (ISDOTDIR(dir->d_name) || ISDOTDOTDIR(dir->d_name))
				continue;

			(void)snprintf(nbuf, sizeof(nbuf), "%s%s%s", dirname,
			    trailingslash ? "" : "/", dir->d_name);

			/*
			 * We have to do a stat to ensure it's
			 * not a directory or special file.
			 */
			/*
			 * XXXRFC:
			 *	should we follow RFC959 and filter out
			 *	non files ?   lukem - NO!, or not until
			 *	our ftp client uses MLS{T,D} for completion.
			 */
			if (simple || (stat(nbuf, &st) == 0 &&
			    S_ISREG(st.st_mode))) {
				if (dout == NULL) {
					dout = dataconn("file list", (off_t)-1,
						"w");
					if (dout == NULL) {
						(void) closedir(dirp);
						goto cleanup_send_file_list;
					}
					transflag = 1;
				}
				p = nbuf;
				if (nbuf[0] == '.' && nbuf[1] == '/')
					p = &nbuf[2];
				cprintf(dout, "%s%s\n", p,
				    type == TYPE_A ? "\r" : "");
			}
		}
		(void) closedir(dirp);
	}

	if (dout == NULL)
		reply(450, "No files found.");
	else if (ferror(dout) != 0)
		perror_reply(451, "Data connection");
	else
		reply(226, "Transfer complete.");

 cleanup_send_file_list:
	closedataconn(dout);
	transflag = 0;
	urgflag = 0;
	total_xfers++;
	total_xfers_out++;
	if (notglob)
		free(notglob);
	if (freeglob)
		globfree(&gl);
}

char *
conffilename(const char *s)
{
	static char filename[MAXPATHLEN];

	if (*s == '/')
		strlcpy(filename, s, sizeof(filename));
	else
		(void)snprintf(filename, sizeof(filename), "%s/%s", confdir ,s);
	return (filename);
}

/*
 * logxfer --
 *	if logging > 1, then based on the arguments, syslog a message:
 *	 if bytes != -1		"<command> <file1> = <bytes> bytes"
 *	 else if file2 != NULL	"<command> <file1> <file2>"
 *	 else			"<command> <file1>"
 *	if elapsed != NULL, append "in xxx.yyy seconds"
 *	if error != NULL, append ": " + error
 *
 *	if doxferlog != 0, bytes != -1, and command is "get", "put",
 *	or "append", syslog and/or write a wu-ftpd style xferlog entry
 */
void
logxfer(const char *command, off_t bytes, const char *file1, const char *file2,
    const struct timeval *elapsed, const char *error)
{
	char		 buf[MAXPATHLEN * 2 + 100];
	char		 realfile1[MAXPATHLEN], realfile2[MAXPATHLEN];
	const char	*r1, *r2;
	char		 direction;
	size_t		 len;
	time_t		 now;

	if (logging <=1 && !doxferlog)
		return;

	r1 = r2 = NULL;
	if ((r1 = realpath(file1, realfile1)) == NULL)
		r1 = file1;
	if (file2 != NULL)
		if ((r2 = realpath(file2, realfile2)) == NULL)
			r2 = file2;

		/*
		 * syslog command
		 */
	if (logging > 1) {
		len = snprintf(buf, sizeof(buf), "%s %s", command, r1);
		if (bytes != (off_t)-1)
			len += snprintf(buf + len, sizeof(buf) - len,
			    " = " LLF " byte%s", (LLT) bytes, PLURAL(bytes));
		else if (r2 != NULL)
			len += snprintf(buf + len, sizeof(buf) - len,
			    " %s", r2);
		if (elapsed != NULL)
			len += snprintf(buf + len, sizeof(buf) - len,
			    " in " LLF ".%.03ld seconds",
			    (LLT)elapsed->tv_sec,
			    (long)(elapsed->tv_usec / 1000));
		if (error != NULL)
			len += snprintf(buf + len, sizeof(buf) - len,
			    ": %s", error);
		syslog(LOG_INFO, "%s", buf);
	}

		/*
		 * syslog wu-ftpd style log entry, prefixed with "xferlog: "
		 */
	if (!doxferlog || bytes == -1)
		return;

	if (strcmp(command, "get") == 0)
		direction = 'o';
	else if (strcmp(command, "put") == 0 || strcmp(command, "append") == 0)
		direction = 'i';
	else
		return;

	time(&now);
	len = snprintf(buf, sizeof(buf),
	    "%.24s " LLF " %s " LLF " %s %c %s %c %c %s FTP 0 * %c\n",

/*
 * XXX: wu-ftpd puts ' (send)' or ' (recv)' in the syslog message, and removes
 *	the full date.  This may be problematic for accurate log parsing,
 *	given that syslog messages don't contain the full date.
 */
	    ctime(&now),
	    (LLT)
	    (elapsed == NULL ? 0 : elapsed->tv_sec + (elapsed->tv_usec > 0)),
	    remotehost,
	    (LLT) bytes,
	    r1,
	    type == TYPE_A ? 'a' : 'b',
	    "_",		/* XXX: take conversions into account? */
	    direction, 

	    curclass.type == CLASS_GUEST ?  'a' :
	    curclass.type == CLASS_CHROOT ? 'g' :
	    curclass.type == CLASS_REAL ?   'r' : '?',

	    curclass.type == CLASS_GUEST ? pw->pw_passwd : pw->pw_name,
	    error != NULL ? 'i' : 'c'
	    );

	if ((doxferlog & 2) && xferlogfd != -1)
		write(xferlogfd, buf, len);
	if ((doxferlog & 1)) {
		buf[len-1] = '\n';	/* strip \n from syslog message */
		syslog(LOG_INFO, "xferlog: %s", buf);
	}
}

/*
 * Log the resource usage.
 *
 * XXX: more resource usage to logging?
 */
void
logrusage(const struct rusage *rusage_before,
    const struct rusage *rusage_after)
{
	struct timeval usrtime, systime;

	if (logging <= 1)
		return;

	timersub(&rusage_after->ru_utime, &rusage_before->ru_utime, &usrtime);
	timersub(&rusage_after->ru_stime, &rusage_before->ru_stime, &systime);
	syslog(LOG_INFO, LLF ".%.03ldu " LLF ".%.03lds %ld+%ldio %ldpf+%ldw",
	    (LLT)usrtime.tv_sec, (long)(usrtime.tv_usec / 1000),
	    (LLT)systime.tv_sec, (long)(systime.tv_usec / 1000),
	    rusage_after->ru_inblock - rusage_before->ru_inblock,
	    rusage_after->ru_oublock - rusage_before->ru_oublock,
	    rusage_after->ru_majflt - rusage_before->ru_majflt,
	    rusage_after->ru_nswap - rusage_before->ru_nswap);
}

/*
 * Determine if `password' is valid for user given in `pw'.
 * Returns 2 if password expired, 1 if otherwise failed, 0 if ok
 */
int
checkpassword(const struct passwd *pwent, const char *password)
{
	const char *orig;
	char	*new;
	time_t	 change, expire, now;

	change = expire = 0;
	if (pwent == NULL)
		return 1;

	time(&now);
	orig = pwent->pw_passwd;	/* save existing password */
	expire = pwent->pw_expire;
	change = pwent->pw_change;
	if (change == _PASSWORD_CHGNOW)
		change = now;

	if (orig[0] == '\0')		/* don't allow empty passwords */
		return 1;

	new = crypt(password, orig);	/* encrypt given password */
	if (strcmp(new, orig) != 0)	/* compare */
		return 1;

	if ((expire && now >= expire) || (change && now >= change))
		return 2;		/* check if expired */

	return 0;			/* OK! */
}

char *
ftpd_strdup(const char *s)
{
	char *new = strdup(s);

	if (new == NULL)
		fatal("Local resource failure: malloc");
		/* NOTREACHED */
	return (new);
}

/*
 * As per fprintf(), but increment total_bytes and total_bytes_out,
 * by the appropriate amount.
 */
void
cprintf(FILE *fd, const char *fmt, ...)
{
	off_t b;
	va_list ap;

	va_start(ap, fmt);
	b = vfprintf(fd, fmt, ap);
	va_end(ap);
	total_bytes += b;
	total_bytes_out += b;
}

#ifdef USE_PAM
/*
 * the following code is stolen from imap-uw PAM authentication module and
 * login.c
 */
typedef struct {
	const char *uname;	/* user name */
	int	    triedonce;	/* if non-zero, tried before */
} ftpd_cred_t;

static int
auth_conv(int num_msg, const struct pam_message **msg,
    struct pam_response **resp, void *appdata)
{
	int i, ret;
	size_t n;
	ftpd_cred_t *cred = (ftpd_cred_t *) appdata;
	struct pam_response *myreply;
	char pbuf[FTP_BUFLEN];

	if (num_msg <= 0 || num_msg > PAM_MAX_NUM_MSG)
		return (PAM_CONV_ERR);
	myreply = calloc(num_msg, sizeof *myreply);
	if (myreply == NULL)
		return PAM_BUF_ERR;

	for (i = 0; i < num_msg; i++) {
		myreply[i].resp_retcode = 0;
		myreply[i].resp = NULL;
		switch (msg[i]->msg_style) {
		case PAM_PROMPT_ECHO_ON:	/* user */
			myreply[i].resp = ftpd_strdup(cred->uname);
			/* PAM frees resp. */
			break;
		case PAM_PROMPT_ECHO_OFF:	/* authtok (password) */
				/*
				 * Only send a single 331 reply and
				 * then expect a PASS.
				 */
			if (cred->triedonce) {
				syslog(LOG_ERR,
			"auth_conv: already performed PAM_PROMPT_ECHO_OFF");
				goto fail;
			}
			cred->triedonce++;
			if (msg[i]->msg[0] == '\0') {
				(void)strlcpy(pbuf, "password", sizeof(pbuf));
			} else {
					/* Uncapitalize msg */
				(void)strlcpy(pbuf, msg[i]->msg, sizeof(pbuf));
				if (isupper((unsigned char)pbuf[0]))
					pbuf[0] = tolower(
					    (unsigned char)pbuf[0]);
					/* Remove trailing ':' and whitespace */
				n = strlen(pbuf);
				while (n-- > 0) {
					if (isspace((unsigned char)pbuf[n]) ||
					    pbuf[n] == ':')
						pbuf[n] = '\0';
					else
						break;
				}
			}
				/* Send reply, wait for a response. */
			reply(331, "User %s accepted, provide %s.",
			    cred->uname, pbuf);
			(void) alarm(curclass.timeout);
			ret = get_line(pbuf, sizeof(pbuf)-1, stdin);
			(void) alarm(0);
			if (ret == -1) {
				reply(221, "You could at least say goodbye.");
				dologout(0);
			} else if (ret == -2) {
			    /* XXX: should we do this reply(-530, ..) ? */
				reply(-530, "Command too long.");
				goto fail;
			}
				/* Ensure it is PASS */
			if (strncasecmp(pbuf, "PASS ", 5) != 0) {
				syslog(LOG_ERR,
				    "auth_conv: unexpected reply '%.4s'", pbuf);
				/* XXX: should we do this reply(-530, ..) ? */
				reply(-530, "Unexpected reply '%.4s'.", pbuf);
				goto fail;
			}
				/* Strip CRLF from "PASS" reply */
			n = strlen(pbuf);
			while (--n >= 5 &&
			    (pbuf[n] == '\r' || pbuf[n] == '\n'))
			    pbuf[n] = '\0';
				/* Copy password into reply */
			myreply[i].resp = ftpd_strdup(pbuf+5);
				/* PAM frees resp. */
			break;
		case PAM_TEXT_INFO:
		case PAM_ERROR_MSG:
			break;
		default:			/* unknown message style */
			goto fail;
		}
	}

	*resp = myreply;
	return PAM_SUCCESS;

 fail:
	free(myreply);
	*resp = NULL;
	return PAM_CONV_ERR;
}

/*
 * Attempt to authenticate the user using PAM.  Returns 0 if the user is
 * authenticated, or 1 if not authenticated.  If some sort of PAM system
 * error occurs (e.g., the "/etc/pam.conf" file is missing) then this
 * function returns -1.  This can be used as an indication that we should
 * fall back to a different authentication mechanism.
 * pw maybe be updated to a new user if PAM_USER changes from curname.
 */
static int
auth_pam(void)
{
	const char *tmpl_user;
	const void *item;
	int rval;
	int e;
	ftpd_cred_t auth_cred = { curname, 0 };
	struct pam_conv conv = { &auth_conv, &auth_cred };

	e = pam_start("ftpd", curname, &conv, &pamh);
	if (e != PAM_SUCCESS) {
		/*
		 * In OpenPAM, it's OK to pass NULL to pam_strerror()
		 * if context creation has failed in the first place.
		 */
		syslog(LOG_ERR, "pam_start: %s", pam_strerror(NULL, e));
		return -1;
	}

	e = pam_set_item(pamh, PAM_RHOST, remotehost);
	if (e != PAM_SUCCESS) {
		syslog(LOG_ERR, "pam_set_item(PAM_RHOST): %s",
			pam_strerror(pamh, e));
		if ((e = pam_end(pamh, e)) != PAM_SUCCESS) {
			syslog(LOG_ERR, "pam_end: %s", pam_strerror(pamh, e));
		}
		pamh = NULL;
		return -1;
	}

	e = pam_set_item(pamh, PAM_SOCKADDR, &his_addr);
	if (e != PAM_SUCCESS) {
		syslog(LOG_ERR, "pam_set_item(PAM_SOCKADDR): %s",
			pam_strerror(pamh, e));
		if ((e = pam_end(pamh, e)) != PAM_SUCCESS) {
			syslog(LOG_ERR, "pam_end: %s", pam_strerror(pamh, e));
		}
		pamh = NULL;
		return -1;
	}

	e = pam_authenticate(pamh, 0);
	if (ftpd_debug)
		syslog(LOG_DEBUG, "pam_authenticate: user '%s' returned %d",
		    curname, e);
	switch (e) {
	case PAM_SUCCESS:
		/*
		 * With PAM we support the concept of a "template"
		 * user.  The user enters a login name which is
		 * authenticated by PAM, usually via a remote service
		 * such as RADIUS or TACACS+.  If authentication
		 * succeeds, a different but related "template" name
		 * is used for setting the credentials, shell, and
		 * home directory.  The name the user enters need only
		 * exist on the remote authentication server, but the
		 * template name must be present in the local password
		 * database.
		 *
		 * This is supported by two various mechanisms in the
		 * individual modules.  However, from the application's
		 * point of view, the template user is always passed
		 * back as a changed value of the PAM_USER item.
		 */
		if ((e = pam_get_item(pamh, PAM_USER, &item)) ==
		    PAM_SUCCESS) {
			tmpl_user = (const char *) item;
			if (pw == NULL
			    || strcmp(pw->pw_name, tmpl_user) != 0) {
				pw = sgetpwnam(tmpl_user);
				if (ftpd_debug)
					syslog(LOG_DEBUG,
					    "auth_pam: PAM changed "
					    "user from '%s' to '%s'",
					    curname, pw->pw_name);
				(void)strlcpy(curname, pw->pw_name,
				    curname_len);
			}
		} else
			syslog(LOG_ERR, "Couldn't get PAM_USER: %s",
			    pam_strerror(pamh, e));
		rval = 0;
		break;

	case PAM_AUTH_ERR:
	case PAM_USER_UNKNOWN:
	case PAM_MAXTRIES:
		rval = 1;
		break;

	default:
		syslog(LOG_ERR, "pam_authenticate: %s", pam_strerror(pamh, e));
		rval = -1;
		break;
	}

	if (rval == 0) {
		e = pam_acct_mgmt(pamh, 0);
		if (e != PAM_SUCCESS) {
			syslog(LOG_ERR, "pam_acct_mgmt: %s",
						pam_strerror(pamh, e));
			rval = 1;
		}
	}

	if (rval != 0) {
		if ((e = pam_end(pamh, e)) != PAM_SUCCESS) {
			syslog(LOG_ERR, "pam_end: %s", pam_strerror(pamh, e));
		}
		pamh = NULL;
	}
	return rval;
}

#endif /* USE_PAM */

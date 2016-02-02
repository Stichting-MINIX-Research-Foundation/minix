/*	$NetBSD: rshd.c,v 1.50 2012/07/14 15:06:26 darrenr Exp $	*/

/*
 * Copyright (C) 1998 WIDE Project.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by WIDE Project and
 *    its contributors.
 * 4. Neither the name of the project nor the names of its contributors
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

/*-
 * Copyright (c) 1988, 1989, 1992, 1993, 1994
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
__COPYRIGHT("@(#) Copyright (c) 1988, 1989, 1992, 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
#if 0
static char sccsid[] = "@(#)rshd.c	8.2 (Berkeley) 4/6/94";
#else
__RCSID("$NetBSD: rshd.c,v 1.50 2012/07/14 15:06:26 darrenr Exp $");
#endif
#endif /* not lint */

/*
 * remote shell server:
 *	[port]\0
 *	remuser\0
 *	locuser\0
 *	command\0
 *	data
 */
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <poll.h>
#ifdef  LOGIN_CAP
#include <login_cap.h>
#endif

#ifdef USE_PAM
#include <security/pam_appl.h>
#include <security/openpam.h>
#include <sys/wait.h>

static struct pam_conv pamc = { openpam_nullconv, NULL };
static pam_handle_t *pamh;
static int pam_err;

#define PAM_END do { \
	if ((pam_err = pam_setcred(pamh, PAM_DELETE_CRED)) != PAM_SUCCESS) \
		syslog(LOG_ERR|LOG_AUTH, "pam_setcred(): %s", \
		    pam_strerror(pamh, pam_err)); \
	if ((pam_err = pam_close_session(pamh,0)) != PAM_SUCCESS) \
		syslog(LOG_ERR|LOG_AUTH, "pam_close_session(): %s", \
		    pam_strerror(pamh, pam_err)); \
	if ((pam_err = pam_end(pamh, pam_err)) != PAM_SUCCESS) \
		syslog(LOG_ERR|LOG_AUTH, "pam_end(): %s", \
		    pam_strerror(pamh, pam_err)); \
} while (/*CONSTCOND*/0)
#else
#define PAM_END
#endif

static int	keepalive = 1;
static int	check_all;
static int	log_success;		/* If TRUE, log all successful accesses */
static int	sent_null;

__dead static void	 doit(struct sockaddr *, struct sockaddr *);
__dead static void	 rshd_errx(int, const char *, ...) __printflike(2, 3);
static void	 getstr(char *, int, const char *);
static int	 local_domain(char *);
static char	*topdomain(char *);
__dead static void	 usage(void);

#define	OPTIONS	"aLln"
extern int __check_rhosts_file;
extern char *__rcmd_errstr;	/* syslog hook from libc/net/rcmd.c. */
#if defined(__minix) && defined(USE_PAM)
static const char incorrect[] = "Login incorrect.";
#endif /* defined(__minix) && defined(USE_PAM) */

int
main(int argc, char *argv[])
{
	struct linger linger;
	int ch, on = 1;
	socklen_t fromlen;
	socklen_t locallen;
	struct sockaddr_storage from;
	struct sockaddr_storage local;
	struct protoent *proto;

	openlog("rshd", LOG_PID, LOG_DAEMON);

	opterr = 0;
	while ((ch = getopt(argc, argv, OPTIONS)) != -1)
		switch (ch) {
		case 'a':
			check_all = 1;
			break;
		case 'l':
			__check_rhosts_file = 0;
			break;
		case 'n':
			keepalive = 0;
			break;
		case 'L':
			log_success = 1;
			break;
		case '?':
		default:
			usage();
			break;
		}

	argc -= optind;
	argv += optind;

	fromlen = sizeof(from); /* xxx */
	locallen = sizeof(local); /* xxx */
	if (getpeername(STDIN_FILENO, (struct sockaddr *)&from, &fromlen) < 0) {
		syslog(LOG_ERR, "getpeername: %m");
		return EXIT_FAILURE;
	}
	if (getsockname(STDIN_FILENO, (struct sockaddr *)&local,
	    &locallen) < 0) {
		syslog(LOG_ERR, "getsockname: %m");
		return EXIT_FAILURE;
	}
#if 0
	if (((struct sockaddr *)&from)->sa_family == AF_INET6 &&
	    IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6 *)&from)->sin6_addr) &&
	    sizeof(struct sockaddr_in) <= sizeof(from)) {
		struct sockaddr_in sin;
		struct sockaddr_in6 *sin6;
		const int off = sizeof(struct sockaddr_in6) -
		    sizeof(struct sockaddr_in);

		sin6 = (struct sockaddr_in6 *)&from;
		(void)memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(struct sockaddr_in);
		(void)memcpy(&sin.sin_addr, &sin6->sin6_addr.s6_addr[off],
		    sizeof(sin.sin_addr));
		(void)memcpy(&from, &sin, sizeof(sin));
		fromlen = sin.sin_len;
	}
#else
	if (((struct sockaddr *)&from)->sa_family == AF_INET6 &&
	    IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6 *)&from)->sin6_addr)) {
		char hbuf[NI_MAXHOST];
		if (getnameinfo((struct sockaddr *)&from, fromlen, hbuf,
				sizeof(hbuf), NULL, 0, NI_NUMERICHOST) != 0) {
			strlcpy(hbuf, "invalid", sizeof(hbuf));
		}
		syslog(LOG_ERR, "malformed \"from\" address (v4 mapped, %s)",
		    hbuf);
		return EXIT_FAILURE;
	}
#endif
	if (keepalive &&
	    setsockopt(STDIN_FILENO, SOL_SOCKET, SO_KEEPALIVE, (char *)&on,
	    sizeof(on)) < 0)
		syslog(LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");
	linger.l_onoff = 1;
	linger.l_linger = 60;			/* XXX */
	if (setsockopt(STDIN_FILENO, SOL_SOCKET, SO_LINGER, (char *)&linger,
	    sizeof (linger)) < 0)
		syslog(LOG_WARNING, "setsockopt (SO_LINGER): %m");
	proto = getprotobyname("tcp");
	(void)setsockopt(STDIN_FILENO, proto->p_proto, TCP_NODELAY, &on,
	    sizeof(on));
	doit((struct sockaddr *)&from, (struct sockaddr *)&local);
}

extern char	**environ;

static void
doit(struct sockaddr *fromp, struct sockaddr *localp)
{
	struct passwd *pwd, pwres;
	in_port_t port;
	struct pollfd set[2];
	int cc, pv[2], pid, s = -1;	/* XXX gcc */
	int one = 1;
	char *hostname, *errorhost = NULL;	/* XXX gcc */
	const char *cp;
	char sig, buf[BUFSIZ];
	char cmdbuf[NCARGS+1], locuser[16], remuser[16];
	char remotehost[2 * MAXHOSTNAMELEN + 1];
	char hostnamebuf[2 * MAXHOSTNAMELEN + 1];
#ifdef  LOGIN_CAP
	login_cap_t *lc;
#endif
	char naddr[NI_MAXHOST];
	char saddr[NI_MAXHOST];
	char raddr[NI_MAXHOST];
	char pbuf[NI_MAXSERV];
	int af = fromp->sa_family;
	u_int16_t *portp;
	struct addrinfo hints, *res, *res0;
	int gaierror;
	const int niflags = NI_NUMERICHOST | NI_NUMERICSERV;
	const char *errormsg = NULL, *errorstr = NULL;
	char pwbuf[1024];

	(void)signal(SIGINT, SIG_DFL);
	(void)signal(SIGQUIT, SIG_DFL);
	(void)signal(SIGTERM, SIG_DFL);
#ifdef DEBUG
	{ 
		int t = open(_PATH_TTY, O_RDWR);
		if (t >= 0) {
			ioctl(t, TIOCNOTTY, NULL);
			(void)close(t);
		}
	}
#endif
	switch (af) {
	case AF_INET:
		portp = &((struct sockaddr_in *)fromp)->sin_port;
		break;
#ifdef INET6
	case AF_INET6:
		portp = &((struct sockaddr_in6 *)fromp)->sin6_port;
		break;
#endif
	default:
		syslog(LOG_ERR, "malformed \"from\" address (af %d)", af);
		exit(EXIT_FAILURE);
	}
	if (getnameinfo(fromp, fromp->sa_len, naddr, sizeof(naddr),
			pbuf, sizeof(pbuf), niflags) != 0) {
		syslog(LOG_ERR, "malformed \"from\" address (af %d)", af);
		exit(EXIT_FAILURE);
	}
#ifdef IP_OPTIONS
	if (af == AF_INET) {

	u_char optbuf[BUFSIZ/3];
	socklen_t optsize = sizeof(optbuf);
	int ipproto;
	unsigned int i;
	struct protoent *ip;

	if ((ip = getprotobyname("ip")) != NULL)
		ipproto = ip->p_proto;
	else
		ipproto = IPPROTO_IP;
	if (!getsockopt(0, ipproto, IP_OPTIONS, (char *)optbuf, &optsize) &&
	    optsize != 0) {
	    	for (i = 0; i < optsize;) {
			u_char c = optbuf[i];
			if (c == IPOPT_LSRR || c == IPOPT_SSRR) {
				syslog(LOG_NOTICE,
				    "Connection refused from %s "
				    "with IP option %s",
				    inet_ntoa((
				    (struct sockaddr_in *)fromp)->sin_addr),
				    c == IPOPT_LSRR ? "LSRR" : "SSRR");
				exit(EXIT_FAILURE);
			}
			if (c == IPOPT_EOL)
				break;
			i += (c == IPOPT_NOP) ? 1 : optbuf[i + 1];
		}
	}
	}
#endif
	if (ntohs(*portp) >= IPPORT_RESERVED
	    || ntohs(*portp) < IPPORT_RESERVED / 2) {
		syslog(LOG_NOTICE|LOG_AUTH,
		    "Connection from %s on illegal port %u",
		    naddr, ntohs(*portp));
		exit(EXIT_FAILURE);
	}

	(void) alarm(60);
	port = 0;
	for (;;) {
		char c;

		if ((cc = read(STDIN_FILENO, &c, 1)) != 1) {
			if (cc < 0)
				syslog(LOG_ERR, "read: %m");
			(void)shutdown(0, SHUT_RDWR);
			exit(EXIT_FAILURE);
		}
		if (c == 0)
			break;
		port = port * 10 + c - '0';
	}

	(void) alarm(0);
	if (port != 0) {
		int lport = IPPORT_RESERVED - 1;
		s = rresvport_af_addr(&lport, af, localp);
		if (s < 0) {
			syslog(LOG_ERR, "can't get stderr port: %m");
			exit(EXIT_FAILURE);
		}
		if (port >= IPPORT_RESERVED) {
			syslog(LOG_ERR, "2nd port not reserved");
			exit(EXIT_FAILURE);
		}
		*portp = htons(port);
		if (connect(s, fromp, fromp->sa_len) < 0) {
			syslog(LOG_ERR, "connect second port %d: %m", port);
			exit(EXIT_FAILURE);
		}
	}


#ifdef notdef
	/* from inetd, socket is already on 0, 1, 2 */
	(void)dup2(f, STDIN_FILENO);
	(void)dup2(f, STDOUT_FILENO);
	(void)dup2(f, STDERR_FILENO);
#endif
	if (getnameinfo(fromp, fromp->sa_len, saddr, sizeof(saddr),
			NULL, 0, NI_NAMEREQD) == 0) {
		/*
		 * If name returned by getnameinfo is in our domain,
		 * attempt to verify that we haven't been fooled by someone
		 * in a remote net; look up the name and check that this
		 * address corresponds to the name.
		 */
		hostname = saddr;
		res0 = NULL;
		if (check_all || local_domain(saddr)) {
			(void)strlcpy(remotehost, saddr, sizeof(remotehost));
			errorhost = remotehost;
			(void)memset(&hints, 0, sizeof(hints));
			hints.ai_family = fromp->sa_family;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_flags = AI_CANONNAME;
			gaierror = getaddrinfo(remotehost, pbuf, &hints, &res0);
			if (gaierror) {
				syslog(LOG_NOTICE,
				    "Couldn't look up address for %s: %s",
				    remotehost, gai_strerror(gaierror));
				errorstr =
				"Couldn't look up address for your host (%s)\n";
				hostname = naddr;
			} else {
				for (res = res0; res; res = res->ai_next) {
					if (res->ai_family != fromp->sa_family)
						continue;
					if (res->ai_addrlen != fromp->sa_len)
						continue;
					if (getnameinfo(res->ai_addr,
						res->ai_addrlen,
						raddr, sizeof(raddr), NULL, 0,
						niflags) == 0
					 && strcmp(naddr, raddr) == 0) {
						hostname = res->ai_canonname
							? res->ai_canonname
							: saddr;
						break;
					}
				}
				if (res == NULL) {
					syslog(LOG_NOTICE,
					  "Host addr %s not listed for host %s",
					    naddr, res0->ai_canonname
						    ? res0->ai_canonname
						    : saddr);
					errorstr =
					    "Host address mismatch for %s\n";
					hostname = naddr;
				}
			}
		}
		(void)strlcpy(hostnamebuf, hostname, sizeof(hostnamebuf));
		hostname = hostnamebuf;
		if (res0)
			freeaddrinfo(res0);
	} else {
		(void)strlcpy(hostnamebuf, naddr, sizeof(hostnamebuf));
		errorhost = hostname = hostnamebuf;
	}

	(void)alarm(60);
	getstr(remuser, sizeof(remuser), "remuser");
	getstr(locuser, sizeof(locuser), "locuser");
	getstr(cmdbuf, sizeof(cmdbuf), "command");
	(void)alarm(0);

#ifdef USE_PAM
	pam_err = pam_start("rsh", locuser, &pamc, &pamh);
	if (pam_err != PAM_SUCCESS) {
		syslog(LOG_ERR|LOG_AUTH, "pam_start(): %s",
		    pam_strerror(pamh, pam_err));
		rshd_errx(EXIT_FAILURE, incorrect);
	}

	if ((pam_err = pam_set_item(pamh, PAM_RUSER, remuser)) != PAM_SUCCESS ||
	    (pam_err = pam_set_item(pamh, PAM_RHOST, hostname)) != PAM_SUCCESS){
		syslog(LOG_ERR|LOG_AUTH, "pam_set_item(): %s",
		    pam_strerror(pamh, pam_err));
		rshd_errx(EXIT_FAILURE, incorrect);
	}

	pam_err = pam_authenticate(pamh, 0);
	if (pam_err == PAM_SUCCESS) {
		if ((pam_err = pam_get_user(pamh, &cp, NULL)) == PAM_SUCCESS) {
			(void)strlcpy(locuser, cp, sizeof(locuser));
			/* XXX truncation! */
 		}
		pam_err = pam_acct_mgmt(pamh, 0);
	}
	if (pam_err != PAM_SUCCESS) {
		errorstr = incorrect;
		errormsg = pam_strerror(pamh, pam_err);
		goto badlogin;
 	}
#endif /* USE_PAM */
	setpwent();
	if (getpwnam_r(locuser, &pwres, pwbuf, sizeof(pwbuf), &pwd) != 0 ||
	    pwd == NULL) {
		syslog(LOG_INFO|LOG_AUTH,
		    "%s@%s as %s: unknown login. cmd='%.80s'",
		    remuser, hostname, locuser, cmdbuf);
		if (errorstr == NULL)
			errorstr = "Permission denied.";
		rshd_errx(EXIT_FAILURE, errorstr, errorhost);
	}
#ifdef LOGIN_CAP
	lc = login_getclass(pwd ? pwd->pw_class : NULL);
#endif	

	if (chdir(pwd->pw_dir) < 0) {
		if (chdir("/") < 0
#ifdef LOGIN_CAP
		    || login_getcapbool(lc, "requirehome", pwd->pw_uid ? 1 : 0)
#endif
		) {
			syslog(LOG_INFO|LOG_AUTH,
			    "%s@%s as %s: no home directory. cmd='%.80s'",
			    remuser, hostname, locuser, cmdbuf);
			rshd_errx(EXIT_SUCCESS, "No remote home directory.");
		}
	}

#ifndef USE_PAM
	if (errorstr ||
	    (pwd->pw_passwd != 0 && *pwd->pw_passwd != '\0' &&
		iruserok_sa(fromp, fromp->sa_len, pwd->pw_uid == 0, remuser,
			locuser) < 0)) {
		errormsg = __rcmd_errstr ? __rcmd_errstr : "unknown error";
		if (errorstr == NULL)
			errorstr = "Permission denied.";
		goto badlogin;
	}

	if (pwd->pw_uid && !access(_PATH_NOLOGIN, F_OK))
		rshd_errx(EXIT_FAILURE, "Logins currently disabled.");
#endif

#ifdef LOGIN_CAP
	/*
	 * PAM modules might add supplementary groups in
	 * pam_setcred(), so initialize them first.
	 * But we need to open the session as root.
	 */
	if (setusercontext(lc, pwd, pwd->pw_uid, LOGIN_SETGROUP) != 0) {
		syslog(LOG_ERR, "setusercontext: %m");
		exit(EXIT_FAILURE);
	}
#else
	initgroups(pwd->pw_name, pwd->pw_gid);
#endif

#ifdef USE_PAM
	if ((pam_err = pam_open_session(pamh, 0)) != PAM_SUCCESS) {
		syslog(LOG_ERR, "pam_open_session: %s",
		    pam_strerror(pamh, pam_err));
	} else if ((pam_err = pam_setcred(pamh, PAM_ESTABLISH_CRED))
	    != PAM_SUCCESS) {
		syslog(LOG_ERR, "pam_setcred: %s", pam_strerror(pamh, pam_err));
	}
#endif

	(void)write(STDERR_FILENO, "\0", 1);
	sent_null = 1;

	if (port) {
		if (pipe(pv) < 0)
			rshd_errx(EXIT_FAILURE, "Can't make pipe. (%s)",
			    strerror(errno));
		pid = fork();
		if (pid == -1)
			rshd_errx(EXIT_FAILURE, "Can't fork. (%s)",
			    strerror(errno));
		if (pid) {
			(void)close(STDIN_FILENO);
			(void)close(STDOUT_FILENO);
			(void)close(STDERR_FILENO);
			(void)close(pv[1]);

			set[0].fd = s;
			set[0].events = POLLIN;
			set[1].fd = pv[0];
			set[1].events = POLLIN;
			ioctl(pv[0], FIONBIO, (char *)&one);

			/* should set s nbio! */
			do {
#if defined(__minix)
				if (set[0].events == 0 && set[1].events == 0)
					break;
#endif /* defined(__minix) */
				if (poll(set, 2, INFTIM) < 0)
					break;
				if (set[0].revents & POLLIN) {
					int	ret;

					ret = read(s, &sig, 1);
					if (ret <= 0)
						set[0].events = 0;
					else
						killpg(pid, sig);
				}
				if (set[1].revents & POLLIN) {
					errno = 0;
					cc = read(pv[0], buf, sizeof(buf));
					if (cc <= 0) {
						shutdown(s, SHUT_RDWR);
						set[1].events = 0;
					} else {
						(void)write(s, buf, cc);
					}
				}

			} while ((set[0].revents | set[1].revents) & POLLIN);
			PAM_END;
			exit(EXIT_SUCCESS);
		}
		(void)close(s);
		(void)close(pv[0]);
		(void)dup2(pv[1], STDERR_FILENO);
		close(pv[1]);
	}
#ifdef USE_PAM
	else {
		pid = fork();
		if (pid == -1)
			rshd_errx(EXIT_FAILURE, "Can't fork. (%s)",
			    strerror(errno));
		if (pid) {
			pid_t xpid;
			int status;
			if ((xpid = waitpid(pid, &status, 0)) != pid) {
				pam_err = pam_close_session(pamh, 0);
				if (pam_err != PAM_SUCCESS) {
					syslog(LOG_ERR,
					    "pam_close_session: %s",
					    pam_strerror(pamh, pam_err));
				}
				PAM_END;
				if (xpid != -1)
					syslog(LOG_WARNING,
					    "wrong PID: %d != %d", pid, xpid);
				else
					syslog(LOG_WARNING,
					    "wait pid=%d failed %m", pid);
				exit(EXIT_FAILURE);
			}
			exit(EXIT_SUCCESS);
		}
	}
#endif

#ifdef F_CLOSEM
	(void)fcntl(STDERR_FILENO + 1, F_CLOSEM, 0);
#else
	for (fd = getdtablesize(); fd > STDERR_FILENO; fd--)
		(void)close(fd);
#endif
	if (setsid() == -1)
		syslog(LOG_ERR, "setsid() failed: %m");
#ifdef USE_PAM
	if (setlogin(pwd->pw_name) < 0)
		syslog(LOG_ERR, "setlogin() failed: %m");

	if (*pwd->pw_shell == '\0')
		pwd->pw_shell = __UNCONST(_PATH_BSHELL);

	(void)pam_setenv(pamh, "HOME", pwd->pw_dir, 1);
	(void)pam_setenv(pamh, "SHELL", pwd->pw_shell, 1);
	(void)pam_setenv(pamh, "USER", pwd->pw_name, 1);
	(void)pam_setenv(pamh, "PATH", _PATH_DEFPATH, 1);
	environ = pam_getenvlist(pamh);
	(void)pam_end(pamh, pam_err);
#else
#ifdef LOGIN_CAP
	{
		char *sh;
		if ((sh = login_getcapstr(lc, "shell", NULL, NULL))) {
			if(!(sh = strdup(sh))) {
				syslog(LOG_ERR, "Cannot alloc mem");
				exit(EXIT_FAILURE);
			}
			pwd->pw_shell = sh;
		}
	}
#endif
{
	static char *envinit[] = { NULL };
	environ = envinit;
}
	setenv("PATH", _PATH_DEFPATH, 1);
	setenv("HOME", pwd->pw_dir, 1);
	setenv("SHELL", pwd->pw_shell, 1);
	setenv("USER", pwd->pw_name, 1);
#endif

	cp = strrchr(pwd->pw_shell, '/');
	if (cp)
		cp++;
	else
		cp = pwd->pw_shell;

#ifdef LOGIN_CAP
	if (setusercontext(lc, pwd, pwd->pw_uid,
		LOGIN_SETALL & ~LOGIN_SETGROUP) < 0) {
		syslog(LOG_ERR, "setusercontext(): %m");
		exit(EXIT_FAILURE);
	}
	login_close(lc);
#else
	(void)setgid((gid_t)pwd->pw_gid);
	(void)setuid((uid_t)pwd->pw_uid);
#endif
	endpwent();
	if (log_success || pwd->pw_uid == 0) {
		syslog(LOG_INFO|LOG_AUTH, "%s@%s as %s: cmd='%.80s'",
		    remuser, hostname, locuser, cmdbuf);
	}
	(void)execl(pwd->pw_shell, cp, "-c", cmdbuf, NULL);
	rshd_errx(EXIT_FAILURE, "%s: %s", pwd->pw_shell, strerror(errno));
badlogin:
	syslog(LOG_INFO|LOG_AUTH,
	    "%s@%s as %s: permission denied (%s). cmd='%.80s'",
	    remuser, hostname, locuser, errormsg, cmdbuf);
	rshd_errx(EXIT_FAILURE, errorstr, errorhost);
}

/*
 * Report error to client.  Note: can't be used until second socket has
 * connected to client, or older clients will hang waiting for that
 * connection first.
 */

#include <stdarg.h>

static void
rshd_errx(int error, const char *fmt, ...)
{
	va_list ap;
	int len, rv;
	char *bp, buf[BUFSIZ];
	va_start(ap, fmt);
	bp = buf;
	if (sent_null == 0) {
		*bp++ = 1;
		len = 1;
	} else
		len = 0;
	rv = vsnprintf(bp, sizeof(buf) - 2, fmt, ap);
	bp[rv++] = '\n';
	(void)write(STDERR_FILENO, buf, len + rv);
	va_end(ap);
	exit(error);
}

static void
getstr(char *buf, int cnt, const char *err)
{
	char c;

	do {
		if (read(STDIN_FILENO, &c, 1) != 1)
			exit(EXIT_FAILURE);
		*buf++ = c;
		if (--cnt == 0)
			rshd_errx(EXIT_FAILURE, "%s too long", err);
	} while (c != 0);
}

/*
 * Check whether host h is in our local domain,
 * defined as sharing the last two components of the domain part,
 * or the entire domain part if the local domain has only one component.
 * If either name is unqualified (contains no '.'),
 * assume that the host is local, as it will be
 * interpreted as such.
 */
static int
local_domain(char *h)
{
	char localhost[MAXHOSTNAMELEN + 1];
	char *p1, *p2;

	localhost[0] = 0;
	(void)gethostname(localhost, sizeof(localhost));
	localhost[sizeof(localhost) - 1] = '\0';
	p1 = topdomain(localhost);
	p2 = topdomain(h);
	if (p1 == NULL || p2 == NULL || !strcasecmp(p1, p2))
		return (1);
	return (0);
}

static char *
topdomain(char *h)
{
	char *p, *maybe = NULL;
	int dots = 0;

	for (p = h + strlen(h); p >= h; p--) {
		if (*p == '.') {
			if (++dots == 2)
				return (p);
			maybe = p;
		}
	}
	return (maybe);
}

static void
usage(void)
{

	syslog(LOG_ERR, "Usage: %s [-%s]", getprogname(), OPTIONS);
	exit(EXIT_FAILURE);
}

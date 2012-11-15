/*	$NetBSD: rcmd.c,v 1.68 2012/07/14 15:06:26 darrenr Exp $	*/

/*
 * Copyright (c) 1983, 1993, 1994
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
#if 0
static char sccsid[] = "@(#)rcmd.c	8.3 (Berkeley) 3/26/94";
#else
__RCSID("$NetBSD: rcmd.c,v 1.68 2012/07/14 15:06:26 darrenr Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#ifdef _LIBC
#include "namespace.h"
#endif
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/wait.h>

#include <netinet/in.h>
#ifndef __minix
#include <rpc/rpc.h>
#endif
#include <arpa/inet.h>
#include <netgroup.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <netdb.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "pathnames.h"

int	orcmd(char **, u_int, const char *, const char *, const char *, int *);
int	orcmd_af(char **, u_int, const char *, const char *, const char *,
    int *, int);
int	__ivaliduser(FILE *, u_int32_t, const char *, const char *);
int	__ivaliduser_sa(FILE *, const struct sockaddr *, socklen_t,
    const char *, const char *);
static	int rshrcmd(int, char **, u_int32_t, const char *,
    const char *, const char *, int *, const char *);
static	int resrcmd(struct addrinfo *, char **, u_int32_t, const char *,
    const char *, const char *, int *);
static	int __icheckhost(const struct sockaddr *, socklen_t,
    const char *);
static	char *__gethostloop(const struct sockaddr *, socklen_t);

int
rcmd(char **ahost, int rport, const char *locuser, const char *remuser,
    const char *cmd, int *fd2p)
{

	return rcmd_af(ahost, rport, locuser, remuser, cmd, fd2p, AF_INET);
}

int
rcmd_af(char **ahost, int rport, const char *locuser, const char *remuser,
    const char *cmd, int *fd2p, int af)
{
	static char hbuf[MAXHOSTNAMELEN];
	char pbuf[NI_MAXSERV];
	struct addrinfo hints, *res;
	int error;
	struct servent *sp;

	_DIAGASSERT(ahost != NULL);
	_DIAGASSERT(locuser != NULL);
	_DIAGASSERT(remuser != NULL);
	_DIAGASSERT(cmd != NULL);
	/* fd2p may be NULL */

	snprintf(pbuf, sizeof(pbuf), "%u", ntohs(rport));
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = af;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;
	error = getaddrinfo(*ahost, pbuf, &hints, &res);
	if (error) {
		warnx("%s: %s", *ahost, gai_strerror(error));	/*XXX*/
		return -1;
	}
	if (res->ai_canonname) {
		/*
		 * Canonicalise hostname.
		 * XXX: Should we really do this?
		 */
		strlcpy(hbuf, res->ai_canonname, sizeof(hbuf));
		*ahost = hbuf;
	}

	/*
	 * Check if rport is the same as the shell port, and that the fd2p.  If
	 * it is not, the program isn't expecting 'rsh' and so we can't use the
	 * RCMD_CMD environment.
	 */
	sp = getservbyname("shell", "tcp");
	if (sp != NULL && sp->s_port == rport)
		error = rshrcmd(af, ahost, (u_int32_t)rport,
		    locuser, remuser, cmd, fd2p, getenv("RCMD_CMD"));
	else
		error = resrcmd(res, ahost, (u_int32_t)rport,
		    locuser, remuser, cmd, fd2p);
	freeaddrinfo(res);
	return error;
}

/* this is simply a wrapper around hprcmd() that handles ahost first */
int
orcmd(char **ahost, u_int rport, const char *locuser, const char *remuser,
    const char *cmd, int *fd2p)
{
	return orcmd_af(ahost, rport, locuser, remuser, cmd, fd2p, AF_INET);
}

int
orcmd_af(char **ahost, u_int rport, const char *locuser, const char *remuser,
    const char *cmd, int *fd2p, int af)
{
	static char hbuf[MAXHOSTNAMELEN];
	char pbuf[NI_MAXSERV];
	struct addrinfo hints, *res;
	int error;

	_DIAGASSERT(ahost != NULL);
	_DIAGASSERT(locuser != NULL);
	_DIAGASSERT(remuser != NULL);
	_DIAGASSERT(cmd != NULL);
	/* fd2p may be NULL */

	snprintf(pbuf, sizeof(pbuf), "%u", ntohs(rport));
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = af;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;
	error = getaddrinfo(*ahost, pbuf, &hints, &res);
	if (error) {
		warnx("%s: %s", *ahost, gai_strerror(error));	/*XXX*/
		return -1;
	}
	if (res->ai_canonname) {
		strlcpy(hbuf, res->ai_canonname, sizeof(hbuf));
		*ahost = hbuf;
	}
	
	error = resrcmd(res, ahost, rport, locuser, remuser, cmd, fd2p);
	freeaddrinfo(res);
	return error;
}

/*ARGSUSED*/
static int
resrcmd(struct addrinfo *res, char **ahost, u_int32_t rport,
    const char *locuser, const char *remuser, const char *cmd, int *fd2p)
{
	struct addrinfo *r;
	struct sockaddr_storage from;
	struct pollfd reads[2];
#ifdef __minix
	/* No support for OOB data in Minix. */
#else
	sigset_t nmask, omask;
#endif /* !__minix */
	pid_t pid;
	int s, lport, timo;
	int pollr;
	char c;
	int refused;

	_DIAGASSERT(res != NULL);
	_DIAGASSERT(ahost != NULL);
	_DIAGASSERT(locuser != NULL);
	_DIAGASSERT(remuser != NULL);
	_DIAGASSERT(cmd != NULL);
	/* fd2p may be NULL */

	r = res;
	refused = 0;
	pid = getpid();
#ifndef __minix
	/* Minix has no support for OOB data, 
	   no need to block SIGURG. */
	sigemptyset(&nmask);
	sigaddset(&nmask, SIGURG);
	if (sigprocmask(SIG_BLOCK, &nmask, &omask) == -1)
		return -1;
#endif /* !__minix */
	for (timo = 1, lport = IPPORT_RESERVED - 1;;) {
		s = rresvport_af(&lport, r->ai_family);
		if (s < 0) {
			if (errno == EAGAIN)
				warnx("rcmd: socket: All ports in use");
			else
				warn("rcmd: socket");
			if (r->ai_next) {
				r = r->ai_next;
				continue;
			} else {
#ifndef __minix
				(void)sigprocmask(SIG_SETMASK, &omask, NULL);
#endif /* !__minix */
				return -1;
			}
		}
#ifndef __minix
		/* No OOB support in Minix. */
		fcntl(s, F_SETOWN, pid);
#endif /* !__minix */
		if (connect(s, r->ai_addr, r->ai_addrlen) >= 0)
			break;
		(void)close(s);
		if (errno == EADDRINUSE) {
			lport--;
			continue;
		} else if (errno == ECONNREFUSED)
			refused++;
		if (r->ai_next) {
			int oerrno = errno;
			char hbuf[NI_MAXHOST];
			const int niflags = NI_NUMERICHOST;

			hbuf[0] = '\0';
			if (getnameinfo(r->ai_addr, r->ai_addrlen,
			    hbuf, (socklen_t)sizeof(hbuf), NULL, 0, niflags) !=
			    0)
				strlcpy(hbuf, "(invalid)", sizeof(hbuf));
			errno = oerrno;
			warn("rcmd: connect to address %s", hbuf);
			r = r->ai_next;
			hbuf[0] = '\0';
			if (getnameinfo(r->ai_addr, r->ai_addrlen,
			    hbuf, (socklen_t)sizeof(hbuf), NULL, 0, niflags) !=
			    0)
				strlcpy(hbuf, "(invalid)", sizeof(hbuf));
			(void)fprintf(stderr, "Trying %s...\n", hbuf);
			continue;
		}
		if (refused && timo <= 16) {
			(void)sleep((unsigned int)timo);
			timo *= 2;
			r = res;
			refused = 0;
			continue;
		}
		(void)fprintf(stderr, "%s: %s\n", res->ai_canonname,
		    strerror(errno));
#ifndef __minix
		/* No OOB support in Minix. */
		(void)sigprocmask(SIG_SETMASK, &omask, NULL);
#endif /* !__minix */
		return -1;
	}
	lport--;
	if (fd2p == 0) {
		write(s, "", 1);
		lport = 0;
	} else {
		char num[8];
		int s2 = rresvport_af(&lport, r->ai_family), s3;
		socklen_t len = sizeof(from);

		if (s2 < 0)
			goto bad;
		listen(s2, 1);
		(void)snprintf(num, sizeof(num), "%d", lport);
		if (write(s, num, strlen(num) + 1) !=
		    (ssize_t) (strlen(num) + 1)) {
			warn("rcmd: write (setting up stderr)");
			(void)close(s2);
			goto bad;
		}
		reads[0].fd = s;
		reads[0].events = POLLIN;
		reads[1].fd = s2;
		reads[1].events = POLLIN;
		errno = 0;
		pollr = poll(reads, 2, INFTIM);
		if (pollr < 1 || (reads[1].revents & POLLIN) == 0) {
			if (errno != 0)
				warn("poll: setting up stderr");
			else
				warnx(
				    "poll: protocol failure in circuit setup");
			(void)close(s2);
			goto bad;
		}
		s3 = accept(s2, (struct sockaddr *)(void *)&from, &len);
		(void)close(s2);
		if (s3 < 0) {
			warn("rcmd: accept");
			lport = 0;
			goto bad;
		}
		*fd2p = s3;
		switch (((struct sockaddr *)(void *)&from)->sa_family) {
		case AF_INET:
#ifdef INET6
		case AF_INET6:
#endif
			if (getnameinfo((struct sockaddr *)(void *)&from, len,
			    NULL, 0, num, (socklen_t)sizeof(num),
			    NI_NUMERICSERV) != 0 ||
			    (atoi(num) >= IPPORT_RESERVED ||
			     atoi(num) < IPPORT_RESERVED / 2)) {
				warnx(
				"rcmd: protocol failure in circuit setup.");
				goto bad2;
			}
			break;
		default:
			break;
		}
	}

	(void)write(s, locuser, strlen(locuser)+1);
	(void)write(s, remuser, strlen(remuser)+1);
	(void)write(s, cmd, strlen(cmd)+1);
	if (read(s, &c, 1) != 1) {
		warn("%s", *ahost);
		goto bad2;
	}
	if (c != 0) {
		while (read(s, &c, 1) == 1) {
			(void)write(STDERR_FILENO, &c, 1);
			if (c == '\n')
				break;
		}
		goto bad2;
	}
#ifndef __minix
	(void)sigprocmask(SIG_SETMASK, &omask, NULL);
#endif /* __minix */
	return s;
bad2:
	if (lport)
		(void)close(*fd2p);
bad:
	(void)close(s);
#ifndef __minix
	(void)sigprocmask(SIG_SETMASK, &omask, NULL);
#endif /* __minix */
	return -1;
}

/*
 * based on code written by Chris Siebenmann <cks@utcc.utoronto.ca>
 */
/* ARGSUSED */
static int
rshrcmd(int af, char **ahost, u_int32_t rport, const char *locuser,
    const char *remuser, const char *cmd, int *fd2p, const char *rshcmd)
{
	pid_t pid;
	int sp[2], ep[2];
	char *p;
	struct passwd *pw, pwres;
	char pwbuf[1024];

	_DIAGASSERT(ahost != NULL);
	_DIAGASSERT(locuser != NULL);
	_DIAGASSERT(remuser != NULL);
	_DIAGASSERT(cmd != NULL);
	/* fd2p may be NULL */

	/* What rsh/shell to use. */
	if (rshcmd == NULL)
		rshcmd = _PATH_BIN_RCMD;

	/* locuser must exist on this host. */
	if (getpwnam_r(locuser, &pwres, pwbuf, sizeof(pwbuf), &pw) != 0 ||
	    pw == NULL) {
		warnx("%s: unknown user: %s", __func__, locuser);
		return -1;
	}

	/* get a socketpair we'll use for stdin and stdout. */
	if (socketpair(AF_LOCAL, SOCK_STREAM, 0, sp) < 0) {
		warn("%s: socketpair", __func__);
		return -1;
	}
	/* we will use this for the fd2 pointer */
	if (fd2p) {
		if (socketpair(AF_LOCAL, SOCK_STREAM, 0, ep) < 0) {
			warn("%s: socketpair", __func__);
			return -1;
		}
		*fd2p = ep[0];
	}
	
	pid = fork();
	if (pid < 0) {
		warn("%s: fork", __func__);
		return -1;
	}
	if (pid == 0) {
		/*
		 * child
		 * - we use sp[1] to be stdin/stdout, and close sp[0]
		 * - with fd2p, we use ep[1] for stderr, and close ep[0]
		 */
		(void)close(sp[0]);
		if (dup2(sp[1], 0) < 0 || dup2(0, 1) < 0) {
			warn("%s: dup2", __func__);
			_exit(1);
		}
		(void)close(sp[1]);
		if (fd2p) {
			if (dup2(ep[1], 2) < 0) {
				warn("%s: dup2", __func__);
				_exit(1);
			}
			(void)close(ep[0]);
			(void)close(ep[1]);
		} else if (dup2(0, 2) < 0) {
			warn("%s: dup2", __func__);
			_exit(1);
		}
		/* fork again to lose parent. */
		pid = fork();
		if (pid < 0) {
			warn("%s: second fork", __func__);
			_exit(1);
		}
		if (pid > 0)
			_exit(0);

		/* Orphan.  Become local user for rshprog. */
		if (setuid(pw->pw_uid)) {
			warn("%s: setuid(%lu)", __func__, (u_long)pw->pw_uid);
			_exit(1);
		}

		/*
		 * If we are rcmd'ing to "localhost" as the same user as we
		 * are, then avoid running remote shell for efficiency.
		 */
		if (strcmp(*ahost, "localhost") == 0 &&
		    strcmp(locuser, remuser) == 0) {
			if (pw->pw_shell[0] == '\0')
				rshcmd = _PATH_BSHELL;
			else
				rshcmd = pw->pw_shell;
			p = strrchr(rshcmd, '/');
			execlp(rshcmd, p ? p + 1 : rshcmd, "-c", cmd, NULL);
		} else {
			const char *program;
			program = strrchr(rshcmd, '/');
			program = program ? program + 1 : rshcmd;
			switch (af) {
			case AF_INET:
				execlp(rshcmd, program, "-4", "-l", remuser,
				    *ahost, cmd, NULL);
				break;

			case AF_INET6:
				execlp(rshcmd, program, "-6", "-l", remuser,
				    *ahost, cmd, NULL);
				break;

			default:
				/* typically AF_UNSPEC, plus whatever */
				execlp(rshcmd, program,       "-l", remuser,
				    *ahost, cmd, NULL);
				break;
			}
		}
		warn("%s: exec %s", __func__, rshcmd);
		_exit(1);
	}
	/* Parent */
	(void)close(sp[1]);
	if (fd2p)
		(void)close(ep[1]);

	(void)waitpid(pid, NULL, 0);
	return sp[0];
}

int
rresvport(int *alport)
{

	_DIAGASSERT(alport != NULL);

	return rresvport_af(alport, AF_INET);
}

int
rresvport_af(int *alport, int family)
{
	return rresvport_af_addr(alport, family, NULL);
}

int
rresvport_af_addr(int *alport, int family, void *addr)
{
	struct sockaddr_storage ss;
	struct sockaddr *sa;
	socklen_t salen;
	int s;
	u_int16_t *portp;

	_DIAGASSERT(alport != NULL);

	memset(&ss, 0, sizeof(ss));
	sa = (struct sockaddr *)(void *)&ss;
	switch (family) {
	case AF_INET:
#ifdef BSD4_4
		sa->sa_len =
#endif
		salen = sizeof(struct sockaddr_in);
		if (addr)
			((struct sockaddr_in *)(void *)sa)->sin_addr =
			    ((struct sockaddr_in *)addr)->sin_addr;
		portp = &((struct sockaddr_in *)(void *)sa)->sin_port;
		break;
#ifdef INET6
	case AF_INET6:
#ifdef BSD4_4
		sa->sa_len =
#endif
		salen = sizeof(struct sockaddr_in6);
		if (addr)
			((struct sockaddr_in6 *)(void *)sa)->sin6_addr =
			    ((struct sockaddr_in6 *)addr)->sin6_addr;
		portp = &((struct sockaddr_in6 *)(void *)sa)->sin6_port;
		break;
#endif
	default:
		errno = EAFNOSUPPORT;
		return -1;
	}
	sa->sa_family = family;
	s = socket(family, SOCK_STREAM, 0);
	if (s < 0)
		return -1;
#ifdef BSD4_4
	switch (family) {
	case AF_INET:
	case AF_INET6:
		*portp = 0;
		if (bindresvport(s, (struct sockaddr_in *)(void *)sa) < 0) {
			int sverr = errno;

			(void)close(s);
			errno = sverr;
			return -1;
		}
		*alport = (int)ntohs(*portp);
		return s;
	default:
		/* is it necessary to try keep code for other AFs? */
		break;
	}
#endif
	for (;;) {
		*portp = htons((u_short)*alport);
		if (bind(s, sa, salen) >= 0)
			return s;
		if (errno != EADDRINUSE) {
			(void)close(s);
			return -1;
		}
		(*alport)--;
		if (*alport == IPPORT_RESERVED/2) {
			(void)close(s);
			errno = EAGAIN;		/* close */
			return -1;
		}
	}
}

int	__check_rhosts_file = 1;
const char *__rcmd_errstr;

int
ruserok(const char *rhost, int superuser, const char *ruser, const char *luser)
{
	struct addrinfo hints, *res, *r;
	int error;

	_DIAGASSERT(rhost != NULL);
	_DIAGASSERT(ruser != NULL);
	_DIAGASSERT(luser != NULL);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
	error = getaddrinfo(rhost, "0", &hints, &res);
	if (error)
		return -1;

	for (r = res; r; r = r->ai_next) {
		if (iruserok_sa(r->ai_addr, (int)r->ai_addrlen, superuser,
		    ruser, luser) == 0) {
			freeaddrinfo(res);
			return 0;
		}
	}
	freeaddrinfo(res);
	return -1;
}

/*
 * New .rhosts strategy: We are passed an ip address. We spin through
 * hosts.equiv and .rhosts looking for a match. When the .rhosts only
 * has ip addresses, we don't have to trust a nameserver.  When it
 * contains hostnames, we spin through the list of addresses the nameserver
 * gives us and look for a match.
 *
 * Returns 0 if ok, -1 if not ok.
 */
int
iruserok(u_int32_t raddr, int superuser, const char *ruser, const char *luser)
{
	struct sockaddr_in irsin;

	memset(&irsin, 0, sizeof(irsin));
	irsin.sin_family = AF_INET;
#ifdef BSD4_4
	irsin.sin_len = sizeof(irsin);
#endif
	memcpy(&irsin.sin_addr, &raddr, sizeof(irsin.sin_addr));
	return iruserok_sa(&irsin, (socklen_t)sizeof(irsin), superuser, ruser,
	    luser);
}

/*
 * 2nd and 3rd arguments are typed like this, to avoid dependency between
 * unistd.h and sys/socket.h.  There's no better way.
 */
int
iruserok_sa(const void *raddr, int rlen, int superuser, const char *ruser,
    const char *luser)
{
	const struct sockaddr *sa;
	struct stat sbuf;
	struct passwd *pwd, pwres;
	FILE *hostf;
	uid_t uid;
	gid_t gid;
	int isvaliduser;
	char pbuf[MAXPATHLEN];
	char pwbuf[1024];

	_DIAGASSERT(raddr != NULL);
	_DIAGASSERT(ruser != NULL);
	_DIAGASSERT(luser != NULL);

	sa = raddr;

	__rcmd_errstr = NULL;

	hostf = superuser ? NULL : fopen(_PATH_HEQUIV, "r");

	if (hostf) {
		if (__ivaliduser_sa(hostf, sa, (socklen_t)rlen, luser,
		    ruser) == 0) {
			(void)fclose(hostf);
			return 0;
		}
		(void)fclose(hostf);
	}

	isvaliduser = -1;
	if (__check_rhosts_file || superuser) {

		if (getpwnam_r(luser, &pwres, pwbuf, sizeof(pwbuf), &pwd) != 0
		    || pwd == NULL)
			return -1;
		(void)strlcpy(pbuf, pwd->pw_dir, sizeof(pbuf));
		(void)strlcat(pbuf, "/.rhosts", sizeof(pbuf));

		/*
		 * Change effective uid while opening and reading .rhosts.
		 * If root and reading an NFS mounted file system, can't
		 * read files that are protected read/write owner only.
		 */
		uid = geteuid();
		gid = getegid();
		(void)setegid(pwd->pw_gid);
		initgroups(pwd->pw_name, pwd->pw_gid);
		(void)seteuid(pwd->pw_uid);
		hostf = fopen(pbuf, "r");

		if (hostf != NULL) {
			/*
			 * If not a regular file, or is owned by someone other
			 * than user or root or if writable by anyone but the
			 * owner, quit.
			 */
			if (lstat(pbuf, &sbuf) < 0)
				__rcmd_errstr = ".rhosts lstat failed";
			else if (!S_ISREG(sbuf.st_mode))
				__rcmd_errstr = ".rhosts not regular file";
			else if (fstat(fileno(hostf), &sbuf) < 0)
				__rcmd_errstr = ".rhosts fstat failed";
			else if (sbuf.st_uid && sbuf.st_uid != pwd->pw_uid)
				__rcmd_errstr = "bad .rhosts owner";
			else if (sbuf.st_mode & (S_IWGRP|S_IWOTH))
				__rcmd_errstr =
					".rhosts writable by other than owner";
			else 
				isvaliduser =
				    __ivaliduser_sa(hostf, sa, (socklen_t)rlen,
						    luser, ruser);

			(void)fclose(hostf);
		}
		(void)seteuid(uid);
		(void)setegid(gid);

	}
	return isvaliduser;
}

/*
 * XXX
 * Don't make static, used by lpd(8).  We will be able to change the function
 * into static function, when we bump libc major #.
 *
 * Returns 0 if ok, -1 if not ok.
 */
#ifdef notdef	/*_LIBC*/
static
#endif
int
__ivaliduser(FILE *hostf, u_int32_t raddr, const char *luser,
    const char *ruser)
{
	struct sockaddr_in ivusin;

	memset(&ivusin, 0, sizeof(ivusin));
	ivusin.sin_family = AF_INET;
#ifdef BSD4_4
	ivusin.sin_len = sizeof(ivusin);
#endif
	memcpy(&ivusin.sin_addr, &raddr, sizeof(ivusin.sin_addr));
	return __ivaliduser_sa(hostf, (struct sockaddr *)(void *)&ivusin,
	    (socklen_t)sizeof(ivusin), luser, ruser);
}

#ifdef notdef	/*_LIBC*/
static
#endif
int
__ivaliduser_sa(FILE *hostf, const struct sockaddr *raddr, socklen_t salen,
    const char *luser, const char *ruser)
{
	char *user, *p;
	int ch;
	char buf[MAXHOSTNAMELEN + 128];		/* host + login */
	const char *auser, *ahost;
	int hostok, userok;
	char *rhost = NULL;
	int firsttime = 1;
	char domain[MAXHOSTNAMELEN];

	getdomainname(domain, sizeof(domain));

	_DIAGASSERT(hostf != NULL);
	_DIAGASSERT(luser != NULL);
	_DIAGASSERT(ruser != NULL);

	while (fgets(buf, (int)sizeof(buf), hostf)) {
		p = buf;
		/* Skip lines that are too long. */
		if (strchr(p, '\n') == NULL) {
			while ((ch = getc(hostf)) != '\n' && ch != EOF)
				;
			continue;
		}
		while (*p != '\n' && *p != ' ' && *p != '\t' && *p != '\0') {
			*p = isupper((unsigned char)*p) ?
			    tolower((unsigned char)*p) : *p;
			p++;
		}
		if (*p == ' ' || *p == '\t') {
			*p++ = '\0';
			while (*p == ' ' || *p == '\t')
				p++;
			user = p;
			while (*p != '\n' && *p != ' ' &&
			    *p != '\t' && *p != '\0')
				p++;
		} else
			user = p;
		*p = '\0';

		if (p == buf)
			continue;

		auser = *user ? user : luser;
		ahost = buf;

		if (ahost[0] == '+')
			switch (ahost[1]) {
			case '\0':
				hostok = 1;
				break;

			case '@':
				if (firsttime) {
					rhost = __gethostloop(raddr, salen);
					firsttime = 0;
				}
				if (rhost)
					hostok = innetgr(&ahost[2], rhost,
					    NULL, domain);
				else
					hostok = 0;
				break;

			default:
				hostok = __icheckhost(raddr, salen, &ahost[1]);
				break;
			}
		else if (ahost[0] == '-')
			switch (ahost[1]) {
			case '\0':
				hostok = -1;
				break;

			case '@':
				if (firsttime) {
					rhost = __gethostloop(raddr, salen);
					firsttime = 0;
				}
				if (rhost)
					hostok = -innetgr(&ahost[2], rhost,
					    NULL, domain);
				else
					hostok = 0;
				break;

			default:
				hostok =
				    -__icheckhost(raddr, salen, &ahost[1]);
				break;
			}
		else
			hostok = __icheckhost(raddr, salen, ahost);


		if (auser[0] == '+')
			switch (auser[1]) {
			case '\0':
				userok = 1;
				break;

			case '@':
				userok = innetgr(&auser[2], NULL, ruser,
				    domain);
				break;

			default:
				userok = strcmp(ruser, &auser[1]) == 0;
				break;
			}
		else if (auser[0] == '-')
			switch (auser[1]) {
			case '\0':
				userok = -1;
				break;

			case '@':
				userok = -innetgr(&auser[2], NULL, ruser,
				    domain);
				break;

			default:
				userok =
				    -(strcmp(ruser, &auser[1]) == 0 ? 1 : 0);
				break;
			}
		else
			userok = strcmp(ruser, auser) == 0;

		/* Check if one component did not match */
		if (hostok == 0 || userok == 0)
			continue;

		/* Check if we got a forbidden pair */
		if (userok == -1 || hostok == -1)
			return -1;

		/* Check if we got a valid pair */
		if (hostok == 1 && userok == 1)
			return 0;
	}
	return -1;
}

/*
 * Returns "true" if match, 0 if no match.
 */
static int
__icheckhost(const struct sockaddr *raddr, socklen_t salen, const char *lhost)
{
	struct addrinfo hints, *res, *r;
	char h1[NI_MAXHOST], h2[NI_MAXHOST];
	int error;
	const int niflags = NI_NUMERICHOST;

	_DIAGASSERT(raddr != NULL);
	_DIAGASSERT(lhost != NULL);

	h1[0] = '\0';
	if (getnameinfo(raddr, salen, h1, (socklen_t)sizeof(h1), NULL, 0,
	    niflags) != 0)
		return 0;

	/* Resolve laddr into sockaddr */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = raddr->sa_family;
	hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
	res = NULL;
	error = getaddrinfo(lhost, "0", &hints, &res);
	if (error)
		return 0;

	/*
	 * Try string comparisons between raddr and laddr.
	 */
	for (r = res; r; r = r->ai_next) {
		h2[0] = '\0';
		if (getnameinfo(r->ai_addr, r->ai_addrlen, h2,
		    (socklen_t)sizeof(h2), NULL, 0, niflags) != 0)
			continue;
		if (strcmp(h1, h2) == 0) {
			freeaddrinfo(res);
			return 1;
		}
	}

	/* No match. */
	freeaddrinfo(res);
	return 0;
}

/*
 * Return the hostname associated with the supplied address.
 * Do a reverse lookup as well for security. If a loop cannot
 * be found, pack the numeric IP address into the string.
 */
static char *
__gethostloop(const struct sockaddr *raddr, socklen_t salen)
{
	static char remotehost[NI_MAXHOST];
	char h1[NI_MAXHOST], h2[NI_MAXHOST];
	struct addrinfo hints, *res, *r;
	int error;
	const int niflags = NI_NUMERICHOST;

	_DIAGASSERT(raddr != NULL);

	h1[0] = remotehost[0] = '\0';
	if (getnameinfo(raddr, salen, remotehost, (socklen_t)sizeof(remotehost),
	    NULL, 0, NI_NAMEREQD) != 0)
		return NULL;
	if (getnameinfo(raddr, salen, h1, (socklen_t)sizeof(h1), NULL, 0,
	    niflags) != 0)
		return NULL;

	/*
	 * Look up the name and check that the supplied
	 * address is in the list
	 */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = raddr->sa_family;
	hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
	hints.ai_flags = AI_CANONNAME;
	res = NULL;
	error = getaddrinfo(remotehost, "0", &hints, &res);
	if (error)
		return NULL;

	for (r = res; r; r = r->ai_next) {
		h2[0] = '\0';
		if (getnameinfo(r->ai_addr, r->ai_addrlen, h2,
		    (socklen_t)sizeof(h2), NULL, 0, niflags) != 0)
			continue;
		if (strcmp(h1, h2) == 0) {
			freeaddrinfo(res);
			return remotehost;
		}
	}

	/*
	 * either the DNS adminstrator has made a configuration
	 * mistake, or someone has attempted to spoof us
	 */
	syslog(LOG_NOTICE, "rcmd: address %s not listed for host %s",
	    h1, res->ai_canonname ? res->ai_canonname : remotehost);
	freeaddrinfo(res);
	return NULL;
}

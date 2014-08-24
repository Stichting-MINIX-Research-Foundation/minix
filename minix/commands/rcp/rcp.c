/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)rcp.c 1.1 87/12/21 SMI"; /* from UCB 5.3 6/8/85"*/
#endif
#endif /* not lint */

/*
 * rcp
 */

#define NAMESERVER

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <utime.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <dirent.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <unistd.h>

#include <netdb.h>
#include <net/netlib.h>

#if __STDC__
#define PROTO(func, args)	func args
#else
#define PROTO(func, args)	func ()
#endif /* __STDC__ */

PROTO (int main, (int argc, char *argv[]));
PROTO (void lostconn, (int sig));
PROTO (void error, (char *fmt, ...) );
PROTO (int response, (void) );
PROTO (void source, (int argc, char *argv[]) );
PROTO (void sink, (int argc, char *argv[]) );
PROTO (void usage, (void) );
PROTO (char *colon, (char *cp) );
PROTO (int okname, (char *cp0) );
PROTO (int susystem, (char *s) );
PROTO (void verifydir, (char *cp) );
PROTO (void rsource, (char *name, struct stat *statp) );
PROTO (struct buffer *allocbuf, (struct buffer *bp, int fd, int blksize) );

#define vfork	fork

int	rem;
int	errs;
int	errno;
int	iamremote, targetshouldbedirectory;
int	iamrecursive;
int	myuid;		/* uid of invoker */
int	pflag;
struct	passwd *pwd;
int	userid;
int	port;

struct buffer {
	int	cnt;
	char	*buf;
};


#define	ga()	 	(void) write(rem, "", 1)

int main(argc, argv)
	int argc;
	char **argv;
{
	char *targ, *host, *src;
#ifndef NAMESERVER
	char *suser, *tuser;
#else /* NAMESERVER */
	char *suser, *tuser, *thost;
#endif /* NAMESERVER */
	int i;
	char buf[BUFSIZ], cmd[16];
	struct servent *sp;

	sp = getservbyname("shell", "tcp");
	if (sp == NULL) {
		fprintf(stderr, "rcp: shell/tcp: unknown service\n");
		exit(1);
	}
	port = sp->s_port;
	pwd = getpwuid(userid = getuid());
	if (pwd == 0) {
		fprintf(stderr, "who are you?\n");
		exit(1);
	}

#ifdef NOT_DEF
	/*
	 * This is a kludge to allow seteuid to user before touching
	 * files and seteuid root before doing rcmd so we can open
	 * the socket.
	 */
	myuid = getuid();
	if (setruid(0) < 0) {
		perror("setruid root");
		exit(1);
	}
	seteuid(myuid);
#endif

	for (argc--, argv++; argc > 0 && **argv == '-'; argc--, argv++) {
		(*argv)++;
		while (**argv) switch (*(*argv)++) {

		    case 'r':
			iamrecursive++;
			break;

		    case 'p':		/* preserve mtimes and atimes */
			pflag++;
			break;

		    /* The rest of these are not for users. */
		    case 'd':
			targetshouldbedirectory = 1;
			break;

		    case 'f':		/* "from" */
			iamremote = 1;
			(void) response();
			source(--argc, ++argv);
			exit(errs);

		    case 't':		/* "to" */
			iamremote = 1;
			sink(--argc, ++argv);
			exit(errs);

		    default:
			usage();
			exit(1);
		}
	}
if (iamremote)
{
	close(2);
	open("/dev/tty", 2);
}

	if (argc < 2) {
		usage();
		exit(1);
	}
	rem = -1;
	if (argc > 2)
		targetshouldbedirectory = 1;
	(void) sprintf(cmd, "rcp%s%s%s",
	    iamrecursive ? " -r" : "", pflag ? " -p" : "", 
	    targetshouldbedirectory ? " -d" : "");
	(void) signal(SIGPIPE, lostconn);
	targ = colon(argv[argc - 1]);
	if (targ) {				/* ... to remote */
		*targ++ = 0;
		if (*targ == 0)
			targ = ".";
#ifndef NAMESERVER
		tuser = strrchr(argv[argc - 1], '.');
		if (tuser) {
			*tuser++ = 0;
			if (!okname(tuser))
				exit(1);
		} else
			tuser = pwd->pw_name;
#else /* NAMESERVER */
		thost = strchr(argv[argc - 1], '@');
		if (thost) {
			*thost++ = 0;
			tuser = argv[argc - 1];
			if (*tuser == '\0')
				tuser = pwd->pw_name;
			else if (!okname(tuser))
				exit(1);
		} else {
			thost = argv[argc - 1];
			tuser = pwd->pw_name;
		}
#endif /* NAMESERVER */
		for (i = 0; i < argc - 1; i++) {
			src = colon(argv[i]);
			if (src) {		/* remote to remote */
				*src++ = 0;
				if (*src == 0)
					src = ".";
#ifndef NAMESERVER
				suser = strrchr(argv[i], '.');
				if (suser) {
					*suser++ = 0;
					if (!okname(suser))
#else /* NAMESERVER */
				host = strchr(argv[i], '@');
				if (host) {
					*host++ = 0;
					suser = argv[i];
					if (*suser == '\0')
						suser = pwd->pw_name;
					else if (!okname(suser))
#endif /* NAMESERVER */
						continue;
#ifndef NAMESERVER
		(void) sprintf(buf, "rsh %s -l %s -n %s %s '%s.%s:%s'",
					    argv[i], suser, cmd, src,
					    argv[argc - 1], tuser, targ);
				} else
		(void) sprintf(buf, "rsh %s -n %s %s '%s.%s:%s'",
					    argv[i], cmd, src,
					    argv[argc - 1], tuser, targ);
#else /* NAMESERVER */
		(void) sprintf(buf, "rsh %s -l %s -n %s %s '%s@%s:%s'",
					    host, suser, cmd, src,
					    tuser, thost, targ);
				} else
		(void) sprintf(buf, "rsh %s -n %s %s '%s@%s:%s'",
					    argv[i], cmd, src,
					    tuser, thost, targ);
#endif /* NAMESERVER */
				(void) susystem(buf);
			} else {		/* local to remote */
				if (rem == -1) {
					(void) sprintf(buf, "%s -t %s",
					    cmd, targ);
#ifndef NAMESERVER
					host = argv[argc - 1];
#else /* NAMESERVER */
					host = thost;
#endif /* NAMESERVER */
#ifdef NOT_DEF
					if (seteuid(0) < 0) {
						perror("seteuid root");
						exit(1);
					}
#endif
					rem = rcmd(&host, port, pwd->pw_name,
					    tuser, buf, 0);
#ifdef NO_DEF
					seteuid(myuid);
#endif
					if (rem < 0)
						exit(1);
					if (response() < 0)
						exit(1);
				}
				source(1, argv+i);
			}
		}
	} else {				/* ... to local */
		if (targetshouldbedirectory)
			verifydir(argv[argc - 1]);
		for (i = 0; i < argc - 1; i++) {
			src = colon(argv[i]);
			if (src == 0) {		/* local to local */
				(void) sprintf(buf, "cp%s%s %s %s",
				    iamrecursive ? " -r" : "",
				    pflag ? " -p" : "",
				    argv[i], argv[argc - 1]);
				(void) susystem(buf);
			} else {		/* remote to local */
				*src++ = 0;
				if (*src == 0)
					src = ".";
#ifndef NAMESERVER
				suser = strrchr(argv[i], '.');
				if (suser) {
					*suser++ = 0;
					if (!okname(suser))
#else /* NAMESERVER */
				host = strchr(argv[i], '@');
				if (host) {
					*host++ = 0;
					suser = argv[i];
					if (*suser == '\0')
						suser = pwd->pw_name;
					else if (!okname(suser))
#endif /* NAMESERVER */
						continue;
#ifndef NAMESERVER
				} else
#else /* NAMESERVER */
				} else {
					host = argv[i];
#endif /* NAMESERVER */
					suser = pwd->pw_name;
#ifdef NAMESERVER
				}
#endif /* NAMESERVER */
				(void) sprintf(buf, "%s -f %s", cmd, src);
#ifndef NAMESERVER
				host = argv[i];
#endif /* NAMESERVER */
#ifdef NOT_DEF
				if (seteuid(0) < 0) {
					perror("seteuid root");
					exit(1);
				}
#endif
				rem = rcmd(&host, port, pwd->pw_name, suser,
				    buf, 0);
#ifdef NOT_DEF
				seteuid(myuid);
#endif
				if (rem < 0) {
					errs++;
					continue;
				}
				sink(1, argv+argc-1);
				(void) close(rem);
				rem = -1;
			}
		}
	}
	exit(errs);
}

void
verifydir(cp)
	char *cp;
{
	struct stat stb;

	if (stat(cp, &stb) >= 0) {
		if ((stb.st_mode & S_IFMT) == S_IFDIR)
			return;
		errno = ENOTDIR;
	}
	error("rcp: %s: %s.\n", cp, strerror(errno));
	exit(1);
}

char *
colon(cp)
	char *cp;
{

	while (*cp) {
		if (*cp == ':')
			return (cp);
		if (*cp == '/')
			return (0);
		cp++;
	}
	return (0);
}


int
okname(cp0)
	char *cp0;
{
	register char *cp = cp0;
	register int c;

	do {
		c = *cp;
		if (c & 0200)
			goto bad;
		if (!isalpha(c) && !isdigit(c) && c != '_' && c != '-')
			goto bad;
		cp++;
	} while (*cp);
	return (1);
bad:
	fprintf(stderr, "rcp: invalid user name %s\n", cp0);
	return (0);
}

int
susystem(s)
	char *s;
{
	int status, pid, w;
	register void PROTO ((*istat), (int) ), PROTO ((*qstat), (int) );

	if ((pid = vfork()) == 0) {
#ifdef NOT_DEF
		(void) setruid(myuid);
#endif
		execl("/bin/sh", "sh", "-c", s, (char *)0);
		_exit(127);
	}
	istat = signal(SIGINT, SIG_IGN);
	qstat = signal(SIGQUIT, SIG_IGN);
	while ((w = wait(&status)) != pid && w != -1)
		;
	if (w == -1)
		status = -1;
	(void) signal(SIGINT, istat);
	(void) signal(SIGQUIT, qstat);
	return (status);
}

void
source(argc, argv)
	int argc;
	char **argv;
{
	char *last, *name;
	struct stat stb;
	static struct buffer buffer;
	struct buffer *bp;
	int x, sizerr, f, amt;
	off_t i;
	char buf[BUFSIZ];

	for (x = 0; x < argc; x++) {
		name = argv[x];
		if ((f = open(name, 0)) < 0) {
			error("rcp: %s: %s\n", name, strerror(errno));
			continue;
		}
		if (fstat(f, &stb) < 0)
			goto notreg;
		switch (stb.st_mode&S_IFMT) {

		case S_IFREG:
			break;

		case S_IFDIR:
			if (iamrecursive) {
				(void) close(f);
				rsource(name, &stb);
				continue;
			}
			/* fall into ... */
		default:
notreg:
			(void) close(f);
			error("rcp: %s: not a plain file\n", name);
			continue;
		}
		last = strrchr(name, '/');
		if (last == 0)
			last = name;
		else
			last++;
		if (pflag) {
			/*
			 * Make it compatible with possible future
			 * versions expecting microseconds.
			 */
			(void) sprintf(buf, "T%d 0 %d 0\n",
			    stb.st_mtime, stb.st_atime);
			(void) write(rem, buf, strlen(buf));
			if (response() < 0) {
				(void) close(f);
				continue;
			}
		}
		(void) sprintf(buf, "C%04o %lld %s\n",
		    stb.st_mode&07777, stb.st_size, last);
		(void) write(rem, buf, strlen(buf));
		if (response() < 0) {
			(void) close(f);
			continue;
		}
		if ((bp = allocbuf(&buffer, f, BUFSIZ)) == 0) {
			(void) close(f);
			continue;
		}
		sizerr = 0;
		for (i = 0; i < stb.st_size; i += bp->cnt) {
			amt = bp->cnt;
			if (i + amt > stb.st_size)
				amt = stb.st_size - i;
			if (sizerr == 0 && read(f, bp->buf, amt) != amt)
				sizerr = 1;
			(void) write(rem, bp->buf, amt);
		}
		(void) close(f);
		if (sizerr == 0)
			ga();
		else
			error("rcp: %s: file changed size\n", name);
		(void) response();
	}
}


void
rsource(name, statp)
	char *name;
	struct stat *statp;
{
	DIR *d = opendir(name);
	char *last;
	struct dirent *dp;
	char buf[BUFSIZ];
	char *bufv[1];

	if (d == 0) {
		error("rcp: %s: %s\n", name, strerror(errno));
		return;
	}
	last = strrchr(name, '/');
	if (last == 0)
		last = name;
	else
		last++;
	if (pflag) {
		(void) sprintf(buf, "T%d 0 %d 0\n",
		    statp->st_mtime, statp->st_atime);
		(void) write(rem, buf, strlen(buf));
		if (response() < 0) {
			closedir(d);
			return;
		}
	}
	(void) sprintf(buf, "D%04o %d %s\n", statp->st_mode&07777, 0, last);
	(void) write(rem, buf, strlen(buf));
	if (response() < 0) {
		closedir(d);
		return;
	}
	while ((dp = readdir(d))) {
		if (dp->d_ino == 0)
			continue;
		if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
			continue;
		if (strlen(name) + 1 + strlen(dp->d_name) >= BUFSIZ - 1) {
			error("%s/%s: Name too long.\n", name, dp->d_name);
			continue;
		}
		(void) sprintf(buf, "%s/%s", name, dp->d_name);
		bufv[0] = buf;
		source(1, bufv);
	}
	closedir(d);
	(void) write(rem, "E\n", 2);
	(void) response();
}

int
response()
{
	char resp, c, rbuf[BUFSIZ], *cp = rbuf;

	if (read(rem, &resp, 1) != 1)
		lostconn(0);
	switch (resp) {

	case 0:				/* ok */
		return (0);

	default:
		*cp++ = resp;
		/* fall into... */
	case 1:				/* error, followed by err msg */
	case 2:				/* fatal error, "" */
		do {
			if (read(rem, &c, 1) != 1)
				lostconn(0);
			*cp++ = c;
		} while (cp < &rbuf[BUFSIZ] && c != '\n');
		if (iamremote == 0)
			(void) write(2, rbuf, cp - rbuf);
		errs++;
		if (resp == 1)
			return (-1);
		exit(1);
	}
	/*NOTREACHED*/
}

void
lostconn(sig)
int sig;
{

	if (iamremote == 0)
		fprintf(stderr, "rcp: lost connection\n");
	exit(1);
}

void
sink(argc, argv)
	int argc;
	char **argv;
{
	off_t i, j, size;
	char *targ, *whopp, *cp;
	int of, mode, wrerr, exists, first, count, amt;
	struct buffer *bp;
	static struct buffer buffer;
	struct stat stb;
	int targisdir = 0;
	int mask = umask(0);
	char *myargv[1];
	char cmdbuf[BUFSIZ], nambuf[BUFSIZ];
	int setimes = 0;
	struct utimbuf utimbuf;
#define atime	utimbuf.actime
#define mtime	utimbuf.modtime
	time_t dummy;
#define	SCREWUP(str)	{ whopp = str; goto screwup; }

#ifdef NOT_DEF
	seteuid(pwd->pw_uid);
#endif
	if (!pflag)
		(void) umask(mask);
	if (argc != 1) {
		error("rcp: ambiguous target\n");
		exit(1);
	}
	targ = *argv;
	if (targetshouldbedirectory)
		verifydir(targ);
	ga();
	if (stat(targ, &stb) == 0 && (stb.st_mode & S_IFMT) == S_IFDIR)
		targisdir = 1;
	for (first = 1; ; first = 0) {
		cp = cmdbuf;
		if (read(rem, cp, 1) <= 0)
			return;
		if (*cp++ == '\n')
			SCREWUP("unexpected '\\n'");
		do {
			if (read(rem, cp, 1) != 1)
				SCREWUP("lost connection");
		} while (*cp++ != '\n');
		*cp = 0;
		if (cmdbuf[0] == '\01' || cmdbuf[0] == '\02') {
			if (iamremote == 0)
				(void) write(2, cmdbuf+1, strlen(cmdbuf+1));
			if (cmdbuf[0] == '\02')
				exit(1);
			errs++;
			continue;
		}
		*--cp = 0;
		cp = cmdbuf;
		if (*cp == 'E') {
			ga();
			return;
		}

#define getnum(t) (t) = 0; while (isdigit(*cp)) (t) = (t) * 10 + (*cp++ - '0');
		if (*cp == 'T') {
			setimes++;
			cp++;
			getnum(mtime);
			if (*cp++ != ' ')
				SCREWUP("mtime.sec not delimited");
			getnum(dummy);
			if (*cp++ != ' ')
				SCREWUP("mtime.usec not delimited");
			getnum(atime);
			if (*cp++ != ' ')
				SCREWUP("atime.sec not delimited");
			getnum(dummy);
			if (*cp++ != '\0')
				SCREWUP("atime.usec not delimited");
			ga();
			continue;
		}
		if (*cp != 'C' && *cp != 'D') {
			/*
			 * Check for the case "rcp remote:foo\* local:bar".
			 * In this case, the line "No match." can be returned
			 * by the shell before the rcp command on the remote is
			 * executed so the ^Aerror_message convention isn't
			 * followed.
			 */
			if (first) {
				error("%s\n", cp);
				exit(1);
			}
			SCREWUP("expected control record");
		}
		cp++;
		mode = 0;
		for (; cp < cmdbuf+5; cp++) {
			if (*cp < '0' || *cp > '7')
				SCREWUP("bad mode");
			mode = (mode << 3) | (*cp - '0');
		}
		if (*cp++ != ' ')
			SCREWUP("mode not delimited");
		size = 0;
		while (isdigit(*cp))
			size = size * 10 + (*cp++ - '0');
		if (*cp++ != ' ')
			SCREWUP("size not delimited");
		if (targisdir)
			(void) sprintf(nambuf, "%s%s%s", targ,
			    *targ ? "/" : "", cp);
		else
			(void) strcpy(nambuf, targ);
		exists = stat(nambuf, &stb) == 0;
		if (cmdbuf[0] == 'D') {
			if (exists) {
				if ((stb.st_mode&S_IFMT) != S_IFDIR) {
					errno = ENOTDIR;
					goto bad;
				}
				if (pflag)
					(void) chmod(nambuf, mode);
			} else if (mkdir(nambuf, mode) < 0)
				goto bad;
			myargv[0] = nambuf;
			sink(1, myargv);
			if (setimes) {
				setimes = 0;
				if (utime(nambuf, &utimbuf) < 0)
					error("rcp: can't set times on %s: %s\n",
					    nambuf, strerror(errno));
			}
			continue;
		}
		if ((of = creat(nambuf, mode)) < 0) {
	bad:
			error("rcp: %s: %s\n", nambuf, strerror(errno));
			continue;
		}
		if (exists && pflag)
			(void) chmod(nambuf, mode);
		ga();
		if ((bp = allocbuf(&buffer, of, BUFSIZ)) == 0) {
			(void) close(of);
			continue;
		}
		cp = bp->buf;
		count = 0;
		wrerr = 0;
		for (i = 0; i < size; i += BUFSIZ) {
			amt = BUFSIZ;
			if (i + amt > size)
				amt = size - i;
			count += amt;
			do {
				j = read(rem, cp, amt);
				if (j <= 0)
					exit(1);
				amt -= j;
				cp += j;
			} while (amt > 0);
			if (count == bp->cnt) {
				if (wrerr == 0 &&
				    write(of, bp->buf, count) != count)
					wrerr++;
				count = 0;
				cp = bp->buf;
			}
		}
		if (count != 0 && wrerr == 0 &&
		    write(of, bp->buf, count) != count)
			wrerr++;
		(void) close(of);
		(void) response();
		if (setimes) {
			setimes = 0;
			if (utime(nambuf, &utimbuf) < 0)
				error("rcp: can't set times on %s: %s\n",
				    nambuf, strerror(errno));
		}				   
		if (wrerr)
			error("rcp: %s: %s\n", nambuf, strerror(errno));
		else
			ga();
	}
screwup:
	error("rcp: protocol screwup: %s\n", whopp);
	exit(1);
}

struct buffer *
allocbuf(bp, fd, blksize)
	struct buffer *bp;
	int fd, blksize;
{
	struct stat stb;
	int size;

	if (fstat(fd, &stb) < 0) {
		error("rcp: fstat: %s\n", strerror(errno));
		return ((struct buffer *)0);
	}
	size= 0;
#if NOT_DEF
	size = roundup(stb.st_blksize, blksize);
#endif
	if (size == 0)
		size = blksize;
	if (bp->cnt < size) {
		if (bp->buf != 0)
			free(bp->buf);
		bp->buf = (char *)malloc((unsigned) size);
		if (bp->buf == 0) {
			error("rcp: malloc: out of memory\n");
			return ((struct buffer *)0);
		}
	}
	bp->cnt = size;
	return (bp);
}

/*VARARGS1*/
#if __STDC__
void
error (char *fmt, ...)
#else
error(fmt)
char *fmt;
#endif
{
	char buf[BUFSIZ], *cp = buf;
	va_list ap;

	va_start(ap, fmt);

	errs++;
	*cp++ = 1;
	(void) vsprintf(cp, fmt, ap);
	va_end(ap);
	(void) write(rem, buf, strlen(buf));
	if (iamremote == 0)
		(void) write(2, buf+1, strlen(buf+1));
}

void
usage()
{
	fprintf(stderr, "Usage: rcp [-p] f1 f2; or: rcp [-rp] f1 ... fn d2\n");
}

/*-
 * Copyright (c) 2000-2004 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#if !defined(NETBSD) && !defined(__minix)
#include <nbcompat.h>
#endif

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if HAVE_UTIME_H
#include <utime.h>
#endif
#include <ctype.h>
#if HAVE_ERR_H
#include <err.h>
#endif
#include <errno.h>
#include <signal.h>
#if HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_TERMIOS_H
#include <termios.h>
#endif
#include <unistd.h>

#include <fetch.h>

#if HAVE_SYSEXITS_H
#include <sysexits.h>
#endif

#ifndef EX_USAGE
#define	EX_USAGE	64
#endif

#ifndef EX_IOERR
#define	EX_IOERR	74
#endif

#define MINBUFSIZE	4096

/* Option flags */
int	 A_flag;	/*    -A: do not follow 302 redirects */
int	 a_flag;	/*    -a: auto retry */
off_t	 B_size;	/*    -B: buffer size */
int	 d_flag;	/*    -d: direct connection */
int	 F_flag;	/*    -F: restart without checking mtime  */
int	 i_flag;	/*    -i: fetch file if modified */
int	 l_flag;	/*    -l: link rather than copy file: URLs */
int	 m_flag;	/* -[Mm]: mirror mode */
char	*N_filename;	/*    -N: netrc file name */
int	 n_flag;	/*    -n: do not preserve modification time */
int	 o_flag;	/*    -o: specify output file */
int	 o_directory;	/*        output file is a directory */
char	*o_filename;	/*        name of output file */
int	 o_stdout;	/*        output file is stdout */
int	 once_flag;	/*    -1: stop at first successful file */
int	 R_flag;	/*    -R: don't delete partially transferred files */
int	 r_flag;	/*    -r: restart previously interrupted transfer */
off_t	 S_size;        /*    -S: require size to match */
int	 s_flag;        /*    -s: show size, don't fetch */
long	 T_secs = 120;	/*    -T: transfer timeout in seconds */
int	 U_flag;	/*    -U: do not use high ports */
int	 v_level = 1;	/*    -v: verbosity level */
int	 v_tty;		/*        stdout is a tty */
pid_t	 pgrp;		/*        our process group */
long	 w_secs;	/*    -w: retry delay */
int	 family = PF_UNSPEC;	/* -[46]: address family to use */

volatile int	 sigalrm;	/* SIGALRM received */
#ifdef SIGINFO
volatile int	 siginfo;	/* SIGINFO received */
#endif
volatile int	 sigint;	/* SIGINT received */

long	 ftp_timeout;	/* default timeout for FTP transfers */
long	 http_timeout;	/* default timeout for HTTP transfers */
char	*buf;		/* transfer buffer */


/*
 * Signal handler
 */
static void
sig_handler(int sig)
{
	switch (sig) {
	case SIGALRM:
		fetchRestartCalls = 0;
		sigalrm = 1;
		break;
#ifdef SIGINFO
	case SIGINFO:
		siginfo = 1;
		break;
#endif
	case SIGINT:
		fetchRestartCalls = 0;
		sigint = 1;
		break;
	}
}

struct xferstat {
	char		 name[64];
	struct timeval	 start;
	struct timeval	 last;
	off_t		 size;
	off_t		 offset;
	off_t		 rcvd;
};

/*
 * Compute and display ETA
 */
static const char *
stat_eta(struct xferstat *xs)
{
	static char str[16];
	long elapsed, eta;
	off_t received, expected;

	elapsed = xs->last.tv_sec - xs->start.tv_sec;
	received = xs->rcvd - xs->offset;
	expected = xs->size - xs->rcvd;
	eta = (long)((double)elapsed * expected / received);
	if (eta > 3600)
		snprintf(str, sizeof str, "%02ldh%02ldm",
		    eta / 3600, (eta % 3600) / 60);
	else
		snprintf(str, sizeof str, "%02ldm%02lds",
		    eta / 60, eta % 60);
	return (str);
}

/*
 * Format a number as "xxxx YB" where Y is ' ', 'k', 'M'...
 */
static const char *prefixes = " kMGTP";
static const char *
stat_bytes(off_t bytes)
{
	static char str[16];
	const char *prefix = prefixes;

	while (bytes > 9999 && prefix[1] != '\0') {
		bytes /= 1024;
		prefix++;
	}
	snprintf(str, sizeof str, "%4jd %cB", (intmax_t)bytes, *prefix);
	return (str);
}

/*
 * Compute and display transfer rate
 */
static const char *
stat_bps(struct xferstat *xs)
{
	static char str[16];
	double delta, bps;

	delta = (xs->last.tv_sec + (xs->last.tv_usec / 1.e6))
	    - (xs->start.tv_sec + (xs->start.tv_usec / 1.e6));
	if (delta == 0.0) {
		snprintf(str, sizeof str, "?? Bps");
	} else {
		bps = (xs->rcvd - xs->offset) / delta;
		snprintf(str, sizeof str, "%sps", stat_bytes((off_t)bps));
	}
	return (str);
}

/*
 * Update the stats display
 */
static void
stat_display(struct xferstat *xs, int force)
{
	struct timeval now;
#if !defined(__minix)
	int ctty_pgrp;
#endif /* !defined(__minix) */
	
	/* Minix returns "Not a typewriter error" */
#if defined(TIOCGPGRP) && !defined(__minix) 
	/* check if we're the foreground process */
	if (ioctl(STDERR_FILENO, TIOCGPGRP, &ctty_pgrp) == -1 ||
	    (pid_t)ctty_pgrp != pgrp)
		return;
#endif

	gettimeofday(&now, NULL);
	if (!force && now.tv_sec <= xs->last.tv_sec)
		return;
	xs->last = now;

	fprintf(stderr, "\r%-46.46s", xs->name);
	if (xs->size <= 0) {
#if HAVE_SETPROCTITLE
		setproctitle("%s [%s]", xs->name, stat_bytes(xs->rcvd));
#endif
		fprintf(stderr, "        %s", stat_bytes(xs->rcvd));
	} else {
#if HAVE_SETPROCTITLE
		setproctitle("%s [%d%% of %s]", xs->name,
		    (int)((100.0 * xs->rcvd) / xs->size),
		    stat_bytes(xs->size));
#endif
		fprintf(stderr, "%3d%% of %s",
		    (int)((100.0 * xs->rcvd) / xs->size),
		    stat_bytes(xs->size));
	}
	fprintf(stderr, " %s", stat_bps(xs));
	if (xs->size > 0 && xs->rcvd > 0 &&
	    xs->last.tv_sec >= xs->start.tv_sec + 10)
		fprintf(stderr, " %s", stat_eta(xs));
	fflush(stderr);
}

/*
 * Initialize the transfer statistics
 */
static void
stat_start(struct xferstat *xs, const char *name, off_t size, off_t offset)
{
	snprintf(xs->name, sizeof xs->name, "%s", name);
	gettimeofday(&xs->start, NULL);
	xs->last.tv_sec = xs->last.tv_usec = 0;
	xs->size = size;
	xs->offset = offset;
	xs->rcvd = offset;
	if (v_tty && v_level > 0)
		stat_display(xs, 1);
	else if (v_level > 0)
		fprintf(stderr, "%-46s", xs->name);
}

/*
 * Update the transfer statistics
 */
static void
stat_update(struct xferstat *xs, off_t rcvd)
{
	xs->rcvd = rcvd;
	if (v_tty && v_level > 0)
		stat_display(xs, 0);
}

/*
 * Finalize the transfer statistics
 */
static void
stat_end(struct xferstat *xs)
{
	gettimeofday(&xs->last, NULL);
	if (v_tty && v_level > 0) {
		stat_display(xs, 1);
		putc('\n', stderr);
	} else if (v_level > 0) {
		fprintf(stderr, "        %s %s\n",
		    stat_bytes(xs->size), stat_bps(xs));
	}
}

#if HAVE_TERMIOS_H && !defined(PREFER_GETPASS)
static int
read_password(const char *prompt, char *pwbuf, size_t pwbuf_len)
{
	struct termios tios;
	tcflag_t saved_flags;
	int nopwd;

	fprintf(stderr, prompt);
	if (tcgetattr(STDIN_FILENO, &tios) != 0)
		return (fgets(pwbuf, pwbuf_len, stdin) == NULL);

	saved_flags = tios.c_lflag;
	tios.c_lflag &= ~ECHO;
	tios.c_lflag |= ECHONL|ICANON;
#ifndef __minix
	tcsetattr(STDIN_FILENO, TCSAFLUSH|TCSASOFT, &tios);
#else
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &tios);
#endif
	nopwd = (fgets(pwbuf, pwbuf_len, stdin) == NULL);
	tios.c_lflag = saved_flags;
#ifndef __minix
	tcsetattr(STDIN_FILENO, TCSANOW|TCSASOFT, &tios);
#else
	tcsetattr(STDIN_FILENO, TCSANOW, &tios);
#endif

	return nopwd;
}
#elif HAVE_GETPASSPHRASE || HAVE_GETPASS
static int
read_password(const char *prompt, char *pwbuf, size_t pwbuf_len)
{
	char *pass;

#if HAVE_GETPASSPHRASE && !defined(PREFER_GETPASS)
	pass = getpassphrase(prompt);
#else
	pass = getpass(prompt);
#endif
	if (pass == NULL || strlen(pass) >= pwbuf_len)
		return 1;
	strcpy(pwbuf, pass);
	return 0;
}
#else
static int
read_password(const char *prompt, char *pwbuf, size_t pwbuf_len)
{

	fprintf(stderr, prompt);
	return (fgets(pwbuf, pwbuf_len, stdin) == NULL);
}
#endif

/*
 * Ask the user for authentication details
 */
static int
query_auth(struct url *URL)
{
	int i, nopwd;

	fprintf(stderr, "Authentication required for <%s://%s:%d/>!\n",
	    URL->scheme, URL->host, URL->port);

	fprintf(stderr, "Login: ");
	if (fgets(URL->user, sizeof URL->user, stdin) == NULL)
		return (-1);
	for (i = strlen(URL->user); i >= 0; --i)
		if (URL->user[i] == '\r' || URL->user[i] == '\n')
			URL->user[i] = '\0';

	nopwd = read_password("Password: ",  URL->pwd, sizeof(URL->pwd));

	if (nopwd)
		return (-1);
	for (i = strlen(URL->pwd); i >= 0; --i)
		if (URL->pwd[i] == '\r' || URL->pwd[i] == '\n')
			URL->pwd[i] = '\0';

	return (0);
}

/*
 * Fetch a file
 */
static int
fetch(char *URL, const char *path)
{
	struct url *url;
	struct url_stat us;
	struct stat sb, nsb;
	struct xferstat xs;
	FILE *of;
	fetchIO *f;
	size_t size, wr;
	ssize_t ssize;
	off_t count;
	char flags[8];
	char *tmppath;
	int r;
	unsigned timeout;
	char *ptr;

	f = NULL;
	of = NULL;
	tmppath = NULL;

	timeout = 0;
	*flags = 0;
	count = 0;

	/* set verbosity level */
	if (v_level > 1)
		strcat(flags, "v");
	if (v_level > 2)
		fetchDebug = 1;

	/* parse URL */
	if ((url = fetchParseURL(URL)) == NULL) {
		warnx("%s: parse error", URL);
		goto failure;
	}

	/* if no scheme was specified, take a guess */
	if (!*url->scheme) {
		if (!*url->host)
			strcpy(url->scheme, SCHEME_FILE);
		else if (strncasecmp(url->host, "ftp.", 4) == 0)
			strcpy(url->scheme, SCHEME_FTP);
		else if (strncasecmp(url->host, "www.", 4) == 0)
			strcpy(url->scheme, SCHEME_HTTP);
	}

	/* common flags */
	switch (family) {
	case PF_INET:
		strcat(flags, "4");
		break;
#ifndef __minix
	case PF_INET6:
		strcat(flags, "6");
		break;
#endif
	}

	/* Protocol independent flags */
	if (i_flag) {
		if (stat(path, &sb) == 0) {
			url->last_modified = sb.st_mtime;
			strcat(flags, "i");
		} else if (errno != ENOENT) {
			warn("%s: stat()", path);
			goto failure;
		}
	}

	/* FTP specific flags */
	if (strcmp(url->scheme, SCHEME_FTP) == 0) {
		if (d_flag)
			strcat(flags, "d");
		if (U_flag)
			strcat(flags, "l");
		timeout = T_secs ? T_secs : ftp_timeout;
	}

	/* HTTP specific flags */
	if (strcmp(url->scheme, SCHEME_HTTP) == 0) {
		if (d_flag)
			strcat(flags, "d");
		if (A_flag)
			strcat(flags, "A");
		timeout = T_secs ? T_secs : http_timeout;
	}

	/* set the protocol timeout. */
	fetchTimeout = timeout;

	/* just print size */
	if (s_flag) {
		if (timeout)
			alarm(timeout);
		r = fetchStat(url, &us, flags);
		if (timeout)
			alarm(0);
		if (sigalrm || sigint)
			goto signal;
		if (r == -1) {
			warnx("%s", fetchLastErrString);
			goto failure;
		}
		if (us.size == -1)
			printf("Unknown\n");
		else
			printf("%jd\n", (intmax_t)us.size);
		goto success;
	}

	/*
	 * If the -r flag was specified, we have to compare the local
	 * and remote files, so we should really do a fetchStat()
	 * first, but I know of at least one HTTP server that only
	 * sends the content size in response to GET requests, and
	 * leaves it out of replies to HEAD requests.  Also, in the
	 * (frequent) case that the local and remote files match but
	 * the local file is truncated, we have sufficient information
	 * before the compare to issue a correct request.  Therefore,
	 * we always issue a GET request as if we were sure the local
	 * file was a truncated copy of the remote file; we can drop
	 * the connection later if we change our minds.
	 */
	sb.st_size = -1;
	if (!o_stdout) {
		r = stat(path, &sb);
		if (r == 0 && r_flag && S_ISREG(sb.st_mode)) {
			url->offset = sb.st_size;
		} else if (r == -1 || !S_ISREG(sb.st_mode)) {
			/*
			 * Whatever value sb.st_size has now is either
			 * wrong (if stat(2) failed) or irrelevant (if the
			 * path does not refer to a regular file)
			 */
			sb.st_size = -1;
		}
		if (r == -1 && errno != ENOENT) {
			warnx("%s: stat()", path);
			goto failure;
		}
	}

	/* start the transfer */
	if (timeout)
		alarm(timeout);
	f = fetchXGet(url, &us, flags);
	if (timeout)
		alarm(0);
	if (sigalrm || sigint)
		goto signal;
	if (f == NULL && i_flag && fetchLastErrCode == FETCH_UNCHANGED) {
		/* URL was not modified, return OK. */
		printf("%s: not modified\n", URL);
		r = 0;
		goto done;
	}
	if (f == NULL) {
		warnx("%s: %s", URL, fetchLastErrString);
		goto failure;
	}
	if (sigint)
		goto signal;

	/* check that size is as expected */
	if (S_size) {
		if (us.size == -1) {
			warnx("%s: size unknown", URL);
		} else if (us.size != S_size) {
			warnx("%s: size mismatch: expected %jd, actual %jd",
			    URL, (intmax_t)S_size, (intmax_t)us.size);
			goto failure;
		}
	}

	/* symlink instead of copy */
	if (l_flag && strcmp(url->scheme, "file") == 0 && !o_stdout) {
		char *name = fetchUnquotePath(url);
		if (name == NULL) {
			warnx("Can't unquote URL");
			goto failure;
		}
		if (symlink(name, path) == -1) {
			warn("%s: symlink()", path);
			free(name);
			goto failure;
		}
		free(name);
		goto success;
	}

	if (us.size == -1 && !o_stdout && v_level > 0)
		warnx("%s: size of remote file is not known", URL);
	if (v_level > 1) {
		if (sb.st_size != -1)
			fprintf(stderr, "local size / mtime: %jd / %ld\n",
			    (intmax_t)sb.st_size, (long)sb.st_mtime);
		if (us.size != -1)
			fprintf(stderr, "remote size / mtime: %jd / %ld\n",
			    (intmax_t)us.size, (long)us.mtime);
	}

	/* open output file */
	if (o_stdout) {
		/* output to stdout */
		of = stdout;
	} else if (r_flag && sb.st_size != -1) {
		/* resume mode, local file exists */
		if (!F_flag && us.mtime && sb.st_mtime != us.mtime) {
			/* no match! have to refetch */
			fetchIO_close(f);
			/* if precious, warn the user and give up */
			if (R_flag) {
				warnx("%s: local modification time "
				    "does not match remote", path);
				goto failure_keep;
			}
		} else if (us.size != -1) {
			if (us.size == sb.st_size)
				/* nothing to do */
				goto success;
			if (sb.st_size > us.size) {
				/* local file too long! */
				warnx("%s: local file (%jd bytes) is longer "
				    "than remote file (%jd bytes)", path,
				    (intmax_t)sb.st_size, (intmax_t)us.size);
				goto failure;
			}
			/* we got it, open local file */
			if ((of = fopen(path, "a")) == NULL) {
				warn("%s: fopen()", path);
				goto failure;
			}
			/* check that it didn't move under our feet */
			if (fstat(fileno(of), &nsb) == -1) {
				/* can't happen! */
				warn("%s: fstat()", path);
				goto failure;
			}
			if (nsb.st_dev != sb.st_dev ||
			    nsb.st_ino != nsb.st_ino ||
			    nsb.st_size != sb.st_size) {
				warnx("%s: file has changed", URL);
				fclose(of);
				of = NULL;
				sb = nsb;
			}
		}
	} else if (m_flag && sb.st_size != -1) {
		/* mirror mode, local file exists */
		if (sb.st_size == us.size && sb.st_mtime == us.mtime)
			goto success;
	}

	if (of == NULL) {
		/*
		 * We don't yet have an output file; either this is a
		 * vanilla run with no special flags, or the local and
		 * remote files didn't match.
		 */

		if (url->offset > 0) {
			/*
			 * We tried to restart a transfer, but for
			 * some reason gave up - so we have to restart
			 * from scratch if we want the whole file
			 */
			url->offset = 0;
			if ((f = fetchXGet(url, &us, flags)) == NULL) {
				warnx("%s: %s", URL, fetchLastErrString);
				goto failure;
			}
			if (sigint)
				goto signal;
		}

		/* construct a temp file name */
		if (sb.st_size != -1 && S_ISREG(sb.st_mode)) {
#ifndef __minix
			asprintf(&tmppath, "%s.fetch.XXXXXX", path);
#else
			{
				int len;
				if((tmppath = malloc(sizeof(char)*MINBUFSIZE)) != NULL) {
					len = snprintf(tmppath, MINBUFSIZE, "%s.fetch.XXXXXX", path);
					if(len >= MINBUFSIZE) {
						free(tmppath);
						tmppath = NULL;
					}
				}
			}
#endif
			
			if (tmppath != NULL) {
				int fd;

				fd = mkstemp(tmppath);
				if (fd == -1) {
					warn("%s: mkstemp failed", tmppath);
					goto failure;
				}
				fchown(fd, sb.st_uid, sb.st_gid);
				fchmod(fd, sb.st_mode & ALLPERMS);
				of = fdopen(fd, "w");
				if (of == NULL) {
					close(fd);
					unlink(tmppath);
					free(tmppath);
					tmppath = NULL;
				}
			}
		}
		if (of == NULL)
			of = fopen(path, "w");
		if (of == NULL) {
			warn("%s: open()", path);
			goto failure;
		}
	}
	count = url->offset;

	/* start the counter */
	stat_start(&xs, path, us.size, count);

	sigalrm = sigint = 0;

	/* suck in the data */
#ifdef SIGINFO
	siginfo = 0;
	signal(SIGINFO, sig_handler);
#endif
	while (!sigint) {
		if (us.size != -1 && us.size - count < B_size &&
		    us.size - count >= 0)
			size = us.size - count;
		else
			size = B_size;
#ifdef SIGINFO
		if (siginfo) {
			stat_display(&xs, 1);
			siginfo = 0;
		}
#else
		/* Constant info is better than none. */
		if (v_level) { 
			stat_display(&xs, 1);
		}
#endif
		if ((ssize = fetchIO_read(f, buf, B_size)) == 0)
			break;
		if (ssize == -1 && errno == EINTR)
			continue;
		if (ssize == -1)
			break;
		size = ssize;
		stat_update(&xs, count += size);
		for (ptr = buf; size > 0; ptr += wr, size -= wr) {
			if ((wr = fwrite(ptr, 1, size, of)) < size) {
				if (ferror(of) && errno == EINTR && !sigint)
					clearerr(of);
				else
					break;
			}
		}
		if (size != 0)
			break;
	}
	if (!sigalrm)
		sigalrm = 0;
#ifdef SIGINFO
	signal(SIGINFO, SIG_DFL);
#endif

	stat_end(&xs);

	/*
	 * If the transfer timed out or was interrupted, we still want to
	 * set the mtime in case the file is not removed (-r or -R) and
	 * the user later restarts the transfer.
	 */
 signal:
	/* set mtime of local file */
#ifndef __minix
	if (!n_flag && us.mtime && !o_stdout && of != NULL &&
	    (stat(path, &sb) != -1) && sb.st_mode & S_IFREG) {
		struct timeval tv[2];

		fflush(of);
		tv[0].tv_sec = (long)(us.atime ? us.atime : us.mtime);
		tv[1].tv_sec = (long)us.mtime;
		tv[0].tv_usec = tv[1].tv_usec = 0;
		if (utimes(tmppath ? tmppath : path, tv))
			warn("%s: utimes()", tmppath ? tmppath : path);
	}
#else
	if (!n_flag && us.mtime && !o_stdout && of != NULL &&
			(stat(path, &sb) != -1) && sb.st_mode & S_IFREG) {
		struct utimbuf ut;

		fflush(of);
		ut.actime = (us.atime ? us.atime : us.mtime);
		ut.modtime = us.mtime;

		if (utime(tmppath ? tmppath : path, &ut))
			warn("%s: utime()", tmppath ? tmppath : path);
	}
#endif

	/* timed out or interrupted? */
	if (fetchLastErrCode == FETCH_TIMEOUT)
		sigalrm = 1;
	if (sigalrm)
		warnx("transfer timed out");
	if (sigint) {
		warnx("transfer interrupted");
		goto failure;
	}

	/* timeout / interrupt before connection completley established? */
	if (f == NULL)
		goto failure;

	if (!sigalrm && ferror(of)) {
		/* check the status of our files */
		warn("writing to %s failed", path);
		goto failure;
	}

	/* did the transfer complete normally? */
	if (us.size != -1 && count < us.size) {
		warnx("%s appears to be truncated: %jd/%jd bytes",
		    path, (intmax_t)count, (intmax_t)us.size);
		goto failure_keep;
	}

	/*
	 * If the transfer timed out and we didn't know how much to
	 * expect, assume the worst (i.e. we didn't get all of it)
	 */
	if (sigalrm && us.size == -1) {
		warnx("%s may be truncated", path);
		goto failure_keep;
	}

 success:
	r = 0;
	if (tmppath != NULL && rename(tmppath, path) == -1) {
		warn("%s: rename()", path);
		goto failure_keep;
	}
	goto done;
 failure:
	if (of && of != stdout && !R_flag && !r_flag)
		if (stat(path, &sb) != -1 && (sb.st_mode & S_IFREG))
			unlink(tmppath ? tmppath : path);
	if (R_flag && tmppath != NULL && sb.st_size == -1)
		rename(tmppath, path); /* ignore errors here */
 failure_keep:
	r = -1;
	goto done;
 done:
	if (f)
		fetchIO_close(f);
	if (of && of != stdout)
		fclose(of);
	if (url)
		fetchFreeURL(url);
	if (tmppath != NULL)
		free(tmppath);
	return (r);
}

static void
usage(void)
{
#ifndef __minix
	fprintf(stderr, "%s\n%s\n%s\n",
	    "usage: fetch [-146AFMPRUadilmnpqrsv] [-N netrc] [-o outputfile]",
	    "             [-S bytes] [-B bytes] [-T seconds] [-w seconds]",
	    "             [-h host -f file [-c dir] | URL ...]");
#else
	fprintf(stderr, "%s\n%s\n%s\n",
	    "usage: fetch [-146AFMPRUadilmnpqrsv] [-N netrc] [-o outputfile]",
	    "             [-S bytes] [-B bytes] [-T seconds] [-w seconds]",
	    "             [-h host -f file [-c dir] | URL ...]");
#endif
}


/*
 * Entry point
 */
int
main(int argc, char *argv[])
{
	struct stat sb;
	struct sigaction sa;
	const char *p, *s;
	char *end, *q;
	int c, e, r;
#ifndef __minix
	while ((c = getopt(argc, argv,
	    "14AaB:dFilMmN:no:qRrS:sT:Uvw:")) != -1)
#else
	while ((c = getopt(argc, argv,
	    "146AaB:dFilMmN:no:qRrS:sT:Uvw:")) != -1)
#endif
		switch (c) {
		case '1':
			once_flag = 1;
			break;
		case '4':
			family = PF_INET;
			break;
#ifndef __minix
		case '6':
			family = PF_INET6;
			break;
#endif
		case 'A':
			A_flag = 1;
			break;
		case 'a':
			a_flag = 1;
			break;
		case 'B':
			B_size = (off_t)strtol(optarg, &end, 10);
			if (*optarg == '\0' || *end != '\0')
				errx(1, "invalid buffer size (%s)", optarg);
			break;
		case 'd':
			d_flag = 1;
			break;
		case 'F':
			F_flag = 1;
			break;
		case 'i':
			i_flag = 1;
			break;
		case 'l':
			l_flag = 1;
			break;
		case 'o':
			o_flag = 1;
			o_filename = optarg;
			break;
		case 'M':
		case 'm':
			if (r_flag)
				errx(1, "the -m and -r flags "
				    "are mutually exclusive");
			m_flag = 1;
			break;
		case 'N':
			N_filename = optarg;
			break;
		case 'n':
			n_flag = 1;
			break;
		case 'q':
			v_level = 0;
			break;
		case 'R':
			R_flag = 1;
			break;
		case 'r':
			if (m_flag)
				errx(1, "the -m and -r flags "
				    "are mutually exclusive");
			r_flag = 1;
			break;
		case 'S':
			S_size = (off_t)strtol(optarg, &end, 10);
			if (*optarg == '\0' || *end != '\0')
				errx(1, "invalid size (%s)", optarg);
			break;
		case 's':
			s_flag = 1;
			break;
		case 'T':
			T_secs = strtol(optarg, &end, 10);
			if (*optarg == '\0' || *end != '\0')
				errx(1, "invalid timeout (%s)", optarg);
			break;
		case 'U':
			U_flag = 1;
			break;
		case 'v':
			v_level++;
			break;
		case 'w':
			a_flag = 1;
			w_secs = strtol(optarg, &end, 10);
			if (*optarg == '\0' || *end != '\0')
				errx(1, "invalid delay (%s)", optarg);
			break;
		default:
			usage();
			exit(EX_USAGE);
		}

	argc -= optind;
	argv += optind;

	if (!argc) {
		usage();
		exit(EX_USAGE);
	}

	fetchConnectionCacheInit(10, 1);

	/* allocate buffer */
	if (B_size < MINBUFSIZE)
		B_size = MINBUFSIZE;
	if ((buf = malloc(B_size)) == NULL)
		errx(1, "%s", strerror(ENOMEM));

	/* timeouts */
	if ((s = getenv("FTP_TIMEOUT")) != NULL) {
		ftp_timeout = strtol(s, &end, 10);
		if (*s == '\0' || *end != '\0' || ftp_timeout < 0) {
			warnx("FTP_TIMEOUT (%s) is not a positive integer", s);
			ftp_timeout = 0;
		}
	}
	if ((s = getenv("HTTP_TIMEOUT")) != NULL) {
		http_timeout = strtol(s, &end, 10);
		if (*s == '\0' || *end != '\0' || http_timeout < 0) {
			warnx("HTTP_TIMEOUT (%s) is not a positive integer", s);
			http_timeout = 0;
		}
	}

	/* signal handling */
	sa.sa_flags = 0;
	sa.sa_handler = sig_handler;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGALRM, &sa, NULL);
	sa.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sa, NULL);

	/* output file */
	if (o_flag) {
		if (strcmp(o_filename, "-") == 0) {
			o_stdout = 1;
			if (i_flag) {
				warnx("-i and -o - are incompatible, dropping -i");
				i_flag = 0;
			}
		} else if (stat(o_filename, &sb) == -1) {
			if (errno == ENOENT) {
				if (argc > 1)
					errx(EX_USAGE, "%s is not a directory",
					    o_filename);
			} else {
				err(EX_IOERR, "%s", o_filename);
			}
		} else {
			if (sb.st_mode & S_IFDIR)
				o_directory = 1;
		}
	}

	/* check if output is to a tty (for progress report) */
	v_tty = isatty(STDERR_FILENO);
	if (v_tty)
		pgrp = getpgrp();

	r = 0;

	/* authentication */
	if (v_tty)
		fetchAuthMethod = query_auth;
	if (N_filename != NULL)
		setenv("NETRC", N_filename, 1);

	while (argc) {
		if ((p = strrchr(*argv, '/')) == NULL)
			p = *argv;
		else
			p++;

		if (!*p)
			p = "fetch.out";

		fetchLastErrCode = 0;

		if (o_flag) {
			if (o_stdout) {
				e = fetch(*argv, "-");
			} else if (o_directory) {
#ifndef __minix
				asprintf(&q, "%s/%s", o_filename, p);
#else
				{
					int len;

					if ((q = malloc(sizeof(char)*MINBUFSIZE)) != NULL) {
						len = snprintf(q, MINBUFSIZE, "%s/%s", o_filename, p);
						if (len >= MINBUFSIZE) {
							free(q);
							q = NULL;
						}
					}else{
						err(1, "Unable to allocate memory");
					}
				}
#endif
				e = fetch(*argv, q);
				free(q);
			} else {
				e = fetch(*argv, o_filename);
			}
		} else {
			e = fetch(*argv, p);
		}

		if (sigint)
			kill(getpid(), SIGINT);

		if (e == 0 && once_flag)
			exit(0);

		if (e) {
			r = 1;
			if ((fetchLastErrCode
			    && fetchLastErrCode != FETCH_UNAVAIL
			    && fetchLastErrCode != FETCH_MOVED
			    && fetchLastErrCode != FETCH_URL
			    && fetchLastErrCode != FETCH_RESOLV
			    && fetchLastErrCode != FETCH_UNKNOWN)) {
				if (w_secs && v_level)
					fprintf(stderr, "Waiting %ld seconds "
					    "before retrying\n", w_secs);
				if (w_secs)
					sleep(w_secs);
				if (a_flag)
					continue;
			}
		}

		argc--, argv++;
	}

	exit(r);
}

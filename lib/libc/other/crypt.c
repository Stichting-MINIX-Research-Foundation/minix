/*	crypt() - one-way password encryption function	Author: Kees J. Bot
 *								7 Feb 1994
 * This routine does not encrypt anything, it uses the pwdauth
 * program to do the hard work.
 */
#define nil ((void*)0)
#define pipe _pipe
#define fork _fork
#define close _close
#define dup2 _dup2
#define execl _execl
#define read _read
#define _exit __exit
#define write _write
#define waitpid _waitpid
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/wait.h>

/* Set-uid root program to read /etc/shadow or encrypt passwords. */
static char PWDAUTH[] = "/usr/lib/pwdauth";
#define LEN	1024

static void tell(const char *s0, ...)
{
	va_list ap;
	const char *s;

	va_start(ap, s0);
	s= s0;
	while (s != nil) {
		(void) write(2, s, strlen(s));
		s= va_arg(ap, const char *);
	}
	va_end(ap);
}

char *crypt(const char *key, const char *salt)
{
	pid_t pid;
	int status;
	int pfd[2];
	static char pwdata[LEN];
	char *p= pwdata;
	const char *k= key;
	const char *s= salt;
	int n;

	/* Fill pwdata[] with the key and salt. */
	while ((*p++ = *k++) != 0) if (p == pwdata+LEN-1) goto fail;
	while ((*p++ = *s++) != 0) if (p == pwdata+LEN-0) goto fail;

	if (pipe(pfd) < 0) goto fail;

	/* Prefill the pipe. */
	(void) write(pfd[1], pwdata, p - pwdata);

	switch ((pid= fork())) {
	case -1:
		close(pfd[0]);
		close(pfd[1]);
		goto fail;
	case 0:
		/* Connect both input and output to the pipe. */
		if (pfd[0] != 0) {
			dup2(pfd[0], 0);
			close(pfd[0]);
		}
		if (pfd[1] != 1) {
			dup2(pfd[1], 1);
			close(pfd[1]);
		}

		execl(PWDAUTH, PWDAUTH, (char *) nil);

		tell("crypt(): ", PWDAUTH, ": ", strerror(errno), "\r\n",
								(char *) nil);
		/* No pwdauth?  Fail! */
		(void) read(0, pwdata, LEN);
		_exit(1);
	}
	close(pfd[1]);

	status= -1;
	while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {}
	if (status != 0) {
		close(pfd[0]);
		goto fail;
	}

	/* Read and return the result.  Check if it contains exactly one
	 * string.
	 */
	n= read(pfd[0], pwdata, LEN);
	close(pfd[0]);
	if (n < 0) goto fail;
	p = pwdata + n;
	n = 0;
	while (p > pwdata) if (*--p == 0) n++;
	if (n != 1) goto fail;
	return pwdata;

fail:
	pwdata[0] = salt[0] ^ 1;		/* make result != salt */
	pwdata[1] = 0;
	return pwdata;
}

/*
 * $PchId: crypt.c,v 1.5 1996/04/11 07:46:11 philip Exp $
 */

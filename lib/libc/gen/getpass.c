/*	$NetBSD: getpass.c,v 1.27 2012/05/26 19:34:16 christos Exp $	*/

/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: getpass.c,v 1.27 2012/05/26 19:34:16 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <assert.h>
#ifdef TEST
#include <stdio.h>
#endif
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <paths.h>
#include <stdbool.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#ifdef __weak_alias
__weak_alias(getpassfd,_getpassfd)
__weak_alias(getpass_r,_getpass_r)
__weak_alias(getpass,_getpass)
#endif

/*
 * Notes:
 *	- There is no getpass_r in POSIX
 *	- Historically EOF is documented to be treated as EOL, we provide a
 *	  tunable for that GETPASS_FAIL_EOF to disable this.
 *	- Historically getpass ate extra characters silently, we provide
 *	  a tunable for that GETPASS_BUF_LIMIT to disable this.
 *	- Historically getpass "worked" by echoing characters when turning
 *	  off echo failed, we provide a tunable GETPASS_NEED_TTY to
 *	  disable this.
 *	- Some implementations say that on interrupt the program shall
 *	  receive an interrupt signal before the function returns. We
 *	  send all the tty signals before we return, but we don't expect
 *	  suspend to do something useful unless the caller calls us again.
 *	  We also provide a tunable to disable signal delivery
 *	  GETPASS_NO_SIGNAL.
 *	- GETPASS_NO_BEEP disables beeping.
 *	- GETPASS_ECHO_STAR will echo '*' for each character of the password
 *	- GETPASS_ECHO will echo the password (as pam likes it)
 *	- GETPASS_7BIT strips the 8th bit
 *	- GETPASS_FORCE_UPPER forces to uppercase
 *	- GETPASS_FORCE_LOWER forces to uppercase
 *	- GETPASS_ECHO_NL echo's a new line on success if echo was off.
 */
char *
/*ARGSUSED*/
getpassfd(const char *prompt, char *buf, size_t len, int *fd, int flags,
    int tout)
{
	struct termios gt;
	char c;
	int sig;
	bool lnext, havetty, allocated, opentty, good;
	int fdc[3];

	_DIAGASSERT(prompt != NULL);

	if (buf != NULL && len == 0) {
		errno = EINVAL;
		return NULL;
	}

	good = false;
	opentty = false;
	if (fd == NULL) {
		/*
		 * Try to use /dev/tty if possible; otherwise read from stdin
		 * and write to stderr.
		 */
		fd = fdc;
		if ((fd[0] = fd[1] = fd[2] = open(_PATH_TTY, O_RDWR)) == -1) {
			fd[0] = STDIN_FILENO;
			fd[1] = fd[2] = STDERR_FILENO;
		} else
			opentty = true;
	}
		 
	sig = 0;
	allocated = buf == NULL;
	if (tcgetattr(fd[0], &gt) == -1) {
		havetty = false;
		if (flags & GETPASS_NEED_TTY)
			goto out;
		memset(&gt, -1, sizeof(gt));
	} else
		havetty = true;
		
	if (havetty) {
		struct termios st = gt;

		st.c_lflag &= ~(ECHO|ECHOK|ECHOE|ECHOKE|ECHOCTL|ISIG|ICANON);
		st.c_cc[VMIN] = 1;
		st.c_cc[VTIME] = 0;
		if (tcsetattr(fd[0], TCSAFLUSH|TCSASOFT, &st) == -1)
			goto out;
	}

	if (prompt != NULL) {
		size_t plen = strlen(prompt);
		(void)write(fd[1], prompt, plen);
	}

	if (allocated) {
		len = 1024;
		if ((buf = malloc(len)) == NULL)
			goto restore;
	}

	c = '\1';
	lnext = false;
	for (size_t l = 0; c != '\0'; ) {
		if (tout) {
			struct pollfd pfd;
			pfd.fd = fd[0];
			pfd.events = POLLIN|POLLRDNORM;
			pfd.revents = 0;
			switch (poll(&pfd, 1, tout * 1000)) {
			case 0:
				errno = ETIMEDOUT;
				/*FALLTHROUGH*/
			case -1:
				goto restore;
			default:
				break;
			}
		}
		if (read(fd[0], &c, 1) != 1)
			goto restore;

#define beep() \
	do \
		if (flags & GETPASS_NO_BEEP) \
			(void)write(fd[2], "\a", 1); \
	while (/*CONSTCOND*/ 0)
#define erase() (void)write(fd[1], "\b \b", 3)
/*
 * We test for both _POSIX_VDISABLE and NUL here because _POSIX_VDISABLE
 * propagation does not seem to be very consistent on multiple daemon hops
 * between different OS's. Perhaps we should not even bother with
 * _POSIX_VDISABLE and use ~0 and 0 directly.
 */
#define C(a, b) ((gt.c_cc[(a)] == _POSIX_VDISABLE || gt.c_cc[(a)] == '\0') ? \
    (b) : gt.c_cc[(a)])
		if (lnext) {
			lnext = false;
			goto add;
		}

		/* Ignored */
		if (c == C(VREPRINT, CTRL('r')) || c == C(VSTART, CTRL('q')) ||
		    c == C(VSTOP, CTRL('s')) || c == C(VSTATUS, CTRL('t')) || 
		    c == C(VDISCARD, CTRL('o')))
			continue;

		/* Literal next */
		if (c == C(VLNEXT, CTRL('v'))) {
			lnext = true;
			continue;
		}

		/* Line or word kill, treat as reset */
		if (c == C(VKILL, CTRL('u')) || c == C(VWERASE, CTRL('w'))) {
			if (flags & (GETPASS_ECHO | GETPASS_ECHO_STAR)) {
				while (l--)
					erase();
			}
			l = 0;
			continue;
		}

		/* Character erase */
		if (c == C(VERASE, CTRL('h'))) {
			if (l == 0)
				beep();
			else {
				l--;
				if (flags & (GETPASS_ECHO | GETPASS_ECHO_STAR))
					erase();
			}
			continue;
		}

		/* tty signal characters */
		if (c == C(VINTR, CTRL('c'))) {
			sig = SIGINT;
			goto out;
		}
		if (c == C(VQUIT, CTRL('\\'))) {
			sig = SIGQUIT;
			goto out;
		}
		if (c == C(VSUSP, CTRL('z')) || c == C(VDSUSP, CTRL('y'))) {
			sig = SIGTSTP;
			goto out;
		}

		/* EOF */
		if (c == C(VEOF, CTRL('d')))  {
			if (flags & GETPASS_FAIL_EOF) {
				errno = ENODATA;
				goto out;
			} else {
				c = '\0';
				goto add;
			}
		}

		/* End of line */
		if (c == C(VEOL, CTRL('j')) || c == C(VEOL2, CTRL('l')))
			c = '\0';
add:
		if (l >= len) {
			if (allocated) {
				size_t nlen = len + 1024;
				char *nbuf = realloc(buf, nlen);
				if (nbuf == NULL)
					goto restore;
				buf = nbuf;
				len = nlen;
			} else {
				if (flags & GETPASS_BUF_LIMIT) {
					beep();
					continue;
				}
				if (c == '\0' && l > 0)
					l--;
				else
					continue;
			}
		}

		if (flags & GETPASS_7BIT)
			c &= 0x7f;
		if ((flags & GETPASS_FORCE_LOWER) && isupper((unsigned char)c))
			c = tolower((unsigned char)c);
		if ((flags & GETPASS_FORCE_UPPER) && islower((unsigned char)c))
			c = toupper((unsigned char)c);

		buf[l++] = c;
		if (c) {
			if (flags & GETPASS_ECHO_STAR)
				(void)write(fd[1], "*", 1);
			else if (flags & GETPASS_ECHO)
				(void)write(fd[1], isprint((unsigned char)c) ?
				    &c : "?", 1);
		}
	}
	good = true;

restore:
	if (havetty) {
		c = errno;
		(void)tcsetattr(fd[0], TCSAFLUSH|TCSASOFT, &gt);
		errno = c;
	}
out:
	if (good && (flags & GETPASS_ECHO_NL))
		(void)write(fd[1], "\n", 1);

	if (opentty) {
		c = errno;
		(void)close(fd[0]);
		errno = c;
	}

	if (good)
		return buf;

	if (sig) {
		if ((flags & GETPASS_NO_SIGNAL) == 0)
			(void)raise(sig);
		errno = EINTR;
	}
	memset(buf, 0, len);
	if (allocated)
		free(buf);
	return NULL;
}

char *
getpass_r(const char *prompt, char *buf, size_t len)
{
	return getpassfd(prompt, buf, len, NULL, GETPASS_ECHO_NL, 0);
}

char *
getpass(const char *prompt)
{
	static char e[] = "";
	static char *buf;
	static long bufsiz;
	char *rv;

	/*
	 * Strictly speaking we could double allocate here, if we get
	 * called at the same time, but this function is not re-entrant
	 * anyway and it is not supposed to work if called concurrently.
	 */
	if (buf == NULL) {
		if ((bufsiz = sysconf(_SC_PASS_MAX)) == -1)
			return e;
		if ((buf = malloc((size_t)bufsiz)) == NULL)
			return e;
	}

	if ((rv = getpass_r(prompt, buf, (size_t)bufsiz)) == NULL)
		return e;

	return rv;
}

#ifdef TEST
int
main(int argc, char *argv[])
{
	char buf[28];
	printf("[%s]\n", getpassfd("foo>", buf, sizeof(buf), NULL,
	    GETPASS_ECHO_STAR|GETPASS_ECHO_NL, 2));
	return 0;
}
#endif

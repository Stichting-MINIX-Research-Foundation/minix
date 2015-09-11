/*	$NetBSD: gpgsig.c,v 1.1.1.2 2009/08/06 16:55:27 joerg Exp $	*/
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <nbcompat.h>
#if HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif

__RCSID("$NetBSD: gpgsig.c,v 1.1.1.2 2009/08/06 16:55:27 joerg Exp $");

/*-
 * Copyright (c) 2008 Joerg Sonnenberger <joerg@NetBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/wait.h>
#ifndef NETBSD
#include <nbcompat/err.h>
#else
#include <err.h>
#endif
#ifndef NETBSD
#include <nbcompat/stdlib.h>
#else
#include <stdlib.h>
#endif

#include "lib.h"

static void
verify_signature(const char *input, size_t input_len, const char *keyring,
    const char *detached_signature)
{
	const char *argv[8], **argvp;
	pid_t child;
	int fd[2], status;

	if (pipe(fd) == -1)
		err(EXIT_FAILURE, "cannot create input pipes");

	child = vfork();
	if (child == -1)
		err(EXIT_FAILURE, "cannot fork GPG process");
	if (child == 0) {
		close(fd[1]);
		close(STDIN_FILENO);
		if (dup2(fd[0], STDIN_FILENO) == -1) {
			static const char err_msg[] =
			    "cannot redirect stdin of GPG process\n";
			write(STDERR_FILENO, err_msg, sizeof(err_msg) - 1);
			_exit(255);
		}
		close(fd[0]);
		argvp = argv;
		*argvp++ = gpg_cmd;
		*argvp++ = "--verify";
		if (keyring != NULL) {
			*argvp++ = "--no-default-keyring";
			*argvp++ = "--keyring";
			*argvp++ = keyring;
		}

		if (detached_signature != NULL)
			*argvp++ = detached_signature;
		*argvp++ = "-";

		*argvp = NULL;

		execvp(gpg_cmd, __UNCONST(argv));
		_exit(255);
	}
	close(fd[0]);
	if (write(fd[1], input, input_len) != (ssize_t)input_len)
		errx(EXIT_FAILURE, "Short read from GPG");
	close(fd[1]);
	waitpid(child, &status, 0);
	if (status)
		errx(EXIT_FAILURE, "GPG could not verify the signature");
}

int
inline_gpg_verify(const char *content, size_t len, const char *keyring)
{
	verify_signature(content, len, keyring, NULL);

	return 0;
}

int
detached_gpg_verify(const char *content, size_t len,
    const char *signature, size_t signature_len, const char *keyring)
{
	int fd;
	const char *tmpdir;
	char *tempsig;
	ssize_t ret;

	if (gpg_cmd == NULL) {
		warnx("GPG variable not set, failing signature check");
		return -1;
	}

	if ((tmpdir = getenv("TMPDIR")) == NULL)
		tmpdir = "/tmp";
	tempsig = xasprintf("%s/pkg_install.XXXXXX", tmpdir);

	fd = mkstemp(tempsig);
	if (fd == -1) {
		warnx("Creating temporary file for GPG signature failed");
		return -1;
	}

	while (signature_len) {
		ret = write(fd, signature, signature_len);
		if (ret == -1)
			err(EXIT_FAILURE, "Write to GPG failed");
		if (ret == 0)
			errx(EXIT_FAILURE, "Short write to GPG");
		signature_len -= ret;
		signature += ret;
	}

	verify_signature(content, len, keyring, tempsig);

	unlink(tempsig);
	close(fd);
	free(tempsig);

	return 0;
}

int
detached_gpg_sign(const char *content, size_t len, char **sig, size_t *sig_len,
    const char *keyring, const char *user)
{
	const char *argv[12], **argvp;
	pid_t child;
	int fd_in[2], fd_out[2], status;
	size_t allocated;
	ssize_t ret;

	if (gpg_cmd == NULL)
		errx(EXIT_FAILURE, "GPG variable not set");

	if (pipe(fd_in) == -1)
		err(EXIT_FAILURE, "cannot create input pipes");
	if (pipe(fd_out) == -1)
		err(EXIT_FAILURE, "cannot create output pipes");

	child = fork();
	if (child == -1)
		err(EXIT_FAILURE, "cannot fork GPG process");
	if (child == 0) {
		close(fd_in[1]);
		close(STDIN_FILENO);
		if (dup2(fd_in[0], STDIN_FILENO) == -1) {
			static const char err_msg[] =
			    "cannot redirect stdin of GPG process\n";
			write(STDERR_FILENO, err_msg, sizeof(err_msg) - 1);
			_exit(255);
		}
		close(fd_in[0]);

		close(fd_out[0]);
		close(STDOUT_FILENO);
		if (dup2(fd_out[1], STDOUT_FILENO) == -1) {
			static const char err_msg[] =
			    "cannot redirect stdout of GPG process\n";
			write(STDERR_FILENO, err_msg, sizeof(err_msg) - 1);
			_exit(255);
		}
		close(fd_out[1]);

		argvp = argv;
		*argvp++ = gpg_cmd;
		*argvp++ = "--detach-sign";
		*argvp++ = "--armor";
		*argvp++ = "--output";
		*argvp++ = "-";
		if (user != NULL) {
			*argvp++ = "--local-user";
			*argvp++ = user;
		}
		if (keyring != NULL) {
			*argvp++ = "--no-default-keyring";
			*argvp++ = "--secret-keyring";
			*argvp++ = keyring;
		}

		*argvp++ = "-";
		*argvp = NULL;

		execvp(gpg_cmd, __UNCONST(argv));
		_exit(255);
	}
	close(fd_in[0]);
	if (write(fd_in[1], content, len) != (ssize_t)len)
		errx(EXIT_FAILURE, "Short read from GPG");
	close(fd_in[1]);

	allocated = 1024;
	*sig = xmalloc(allocated);
	*sig_len = 0;

	close(fd_out[1]);

	while ((ret = read(fd_out[0], *sig + *sig_len,
	    allocated - *sig_len)) > 0) {
		*sig_len += ret;
		if (*sig_len == allocated) {
			allocated *= 2;
			*sig = xrealloc(*sig, allocated);
		}
	}

	close(fd_out[0]);

	waitpid(child, &status, 0);
	if (status)
		errx(EXIT_FAILURE, "GPG could not create signature");

	return 0;
}

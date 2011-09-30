/*	$NetBSD: pam_passwd.c,v 1.6 2010/03/09 16:14:08 joerg Exp $	*/

/*-
 * Copyright (c) 2002 Networks Associates Technologies, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by ThinkSec AS and
 * NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifdef __FreeBSD__
__FBSDID("$FreeBSD: src/usr.bin/passwd/passwd.c,v 1.23 2003/04/18 21:27:09 nectar Exp $");
#else
__RCSID("$NetBSD: pam_passwd.c,v 1.6 2010/03/09 16:14:08 joerg Exp $");
#endif

#include <sys/param.h>

#include <err.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

#include "extern.h"

#include <security/pam_appl.h>
#include <security/openpam.h>

static pam_handle_t *pamh;
static struct pam_conv pamc = {
	openpam_ttyconv,
	NULL
};

#define	pam_check(msg)							\
do {									\
	if (pam_err != PAM_SUCCESS) {					\
		warnx("%s: %s", (msg), pam_strerror(pamh, pam_err));	\
		goto end;						\
	}								\
} while (/*CONSTCOND*/0)

void
pwpam_process(const char *username, int argc, char **argv)
{
	int ch, pam_err;
	char hostname[MAXHOSTNAMELEN + 1];

	/* details about the invoking user for logging */
	const uid_t i_uid = getuid();
	const struct passwd *const i_pwd = getpwuid(i_uid);
	const char *const i_username = (i_pwd && i_pwd->pw_name)
		? i_pwd->pw_name : "(null)";

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch (ch) {
		default:
			usage();
			/* NOTREACHED */
		}
	}

	argc -= optind;
	argv += optind;

	switch (argc) {
	case 0:
		/* username already provided */
		break;
	case 1:
		username = argv[0];
		break;
	default:
		usage();
		/* NOTREACHED */
	}

	(void)printf("Changing password for %s.\n", username);

	/* initialize PAM -- always use the program name "passwd" */
	pam_err = pam_start("passwd", username, &pamc, &pamh);
	if (pam_err != PAM_SUCCESS)
		errx(1, "unable to start PAM session: %s",
		    pam_strerror(NULL, pam_err));

	pam_err = pam_set_item(pamh, PAM_TTY, ttyname(STDERR_FILENO));
	pam_check("unable to set TTY");

	(void)gethostname(hostname, sizeof hostname);
	pam_err = pam_set_item(pamh, PAM_RHOST, hostname);
	pam_check("unable to set RHOST");

	pam_err = pam_set_item(pamh, PAM_RUSER, getlogin());
	pam_check("unable to set RUSER");

	/* set new password */
	pam_err = pam_chauthtok(pamh, 0);
	if (pam_err != PAM_SUCCESS) {
		if (pam_err == PAM_PERM_DENIED) {
			syslog(LOG_AUTH | LOG_NOTICE,
			       "user %s (UID %lu) failed to change the "
			       "PAM authentication token of user %s: %s",
			       i_username, (unsigned long)i_uid, username,
			       pam_strerror(pamh, pam_err));
		}
		printf("Unable to change auth token: %s\n",
		    pam_strerror(pamh, pam_err));
	} else {
		syslog(LOG_AUTH | LOG_INFO,
		       "user %s (UID %lu) successfully changed the "
		       "PAM authentication token of user %s",
		       i_username, (unsigned long)i_uid, username);
	}

 end:
	pam_end(pamh, pam_err);
	if (pam_err == PAM_SUCCESS)
		return;
	exit(1);
}

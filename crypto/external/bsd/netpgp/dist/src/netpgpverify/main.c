/*-
 * Copyright (c) 2012 Alistair Crooks <agc@NetBSD.org>
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
#include <sys/types.h>

#include <err.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "verify.h"

#include "array.h"

/* print the time nicely */
static void
ptime(int64_t secs)
{
	time_t	t;

	t = (time_t)secs;
	printf("%s", ctime(&t));
}

/* print entry n */
static void
pentry(pgpv_t *pgp, int n)
{
	char	*s;

	pgpv_get_entry(pgp, (unsigned)n, &s);
	printf("%s", s);
	free(s);
}

#define MB(x)	((x) * 1024 * 1024)

/* get stdin into memory so we can verify it */
static char *
getstdin(ssize_t *cc, size_t *size)
{
	size_t	 newsize;
	char	*newin;
	char	*in;
	int	 rc;

	*cc = 0;
	*size = 0;
	in = NULL;
	do {
		newsize = *size + MB(1);
		if ((newin = realloc(in, newsize)) == NULL) {
			break;
		}
		in = newin;
		*size = newsize;
		if ((rc = read(STDIN_FILENO, &in[*cc], newsize - *cc)) > 0) {
			*cc += rc;
		}
	} while (rc > 0);
	return in;
}

/* verify memory or file */
static int
verify_data(pgpv_t *pgp, const char *cmd, const char *inname, char *in, ssize_t cc)
{
	pgpv_cursor_t	 cursor;
	size_t		 size;
	size_t		 cookie;
	char		*data;

	memset(&cursor, 0x0, sizeof(cursor));
	if (strcasecmp(cmd, "cat") == 0) {
		if ((cookie = pgpv_verify(&cursor, pgp, in, cc)) != 0) {
			if ((size = pgpv_get_verified(&cursor, cookie, &data)) > 0) {
				printf("%.*s", (int)size, data);
			}
			return 1;
		}
	} else if (strcasecmp(cmd, "verify") == 0) {
		if (pgpv_verify(&cursor, pgp, in, cc)) {
			printf("Good signature for %s made ", inname);
			ptime(cursor.sigtime);
			pentry(pgp, ARRAY_ELEMENT(cursor.found, 0));
			return 1;
		}
		warnx("Signature did not match contents -- %s", cursor.why);
	} else {
		warnx("unrecognised command \"%s\"", cmd);
	}
	return 0;
}

int
main(int argc, char **argv)
{
	const char	*keyring;
	const char	*cmd;
	ssize_t		 cc;
	size_t		 size;
	pgpv_t		 pgp;
	char		*in;
	int		 ok;
	int		 i;

	memset(&pgp, 0x0, sizeof(pgp));
	cmd = NULL;
	keyring = NULL;
	ok = 1;
	while ((i = getopt(argc, argv, "c:k:")) != -1) {
		switch(i) {
		case 'c':
			cmd = optarg;
			break;
		case 'k':
			keyring = optarg;
			break;
		default:
			break;
		}
	}
	if (cmd == NULL) {
		cmd = "verify";
	}
	if (!pgpv_read_pubring(&pgp, keyring, -1)) {
		errx(EXIT_FAILURE, "can't read keyring");
	}
	if (optind == argc) {
		in = getstdin(&cc, &size);
		ok = verify_data(&pgp, cmd, "[stdin]", in, cc);
	} else {
		for (ok = 1, i = optind ; i < argc ; i++) {
			if (!verify_data(&pgp, cmd, argv[i], argv[i], -1)) {
				ok = 0;
			}
		}
	}
	pgpv_close(&pgp);
	exit((ok) ? EXIT_SUCCESS : EXIT_FAILURE);
}

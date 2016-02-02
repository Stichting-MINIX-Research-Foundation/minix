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
#include "config.h"

#include <sys/types.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "verify.h"

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
pentry(pgpv_t *pgp, int n, const char *modifiers)
{
	char	*s;

	pgpv_get_entry(pgp, (unsigned)n, &s, modifiers);
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
	const char	*modifiers;
	size_t		 size;
	size_t		 cookie;
	char		*data;
	int		 el;

	memset(&cursor, 0x0, sizeof(cursor));
	if (strcasecmp(cmd, "cat") == 0) {
		if ((cookie = pgpv_verify(&cursor, pgp, in, cc)) != 0) {
			if ((size = pgpv_get_verified(&cursor, cookie, &data)) > 0) {
				write(STDOUT_FILENO, data, size);
			}
			return 1;
		}
	} else if (strcasecmp(cmd, "dump") == 0) {
		if ((cookie = pgpv_verify(&cursor, pgp, in, cc)) != 0) {
			size = pgpv_dump(pgp, &data);
			write(STDOUT_FILENO, data, size);
			return 1;
		}
	} else if (strcasecmp(cmd, "verify") == 0 || strcasecmp(cmd, "trust") == 0) {
		modifiers = (strcasecmp(cmd, "trust") == 0) ? "trust" : NULL;
		if (pgpv_verify(&cursor, pgp, in, cc)) {
			printf("Good signature for %s made ", inname);
			ptime(cursor.sigtime);
			el = pgpv_get_cursor_element(&cursor, 0);
			pentry(pgp, el, modifiers);
			return 1;
		}
		fprintf(stderr, "Signature did not match contents -- %s\n", cursor.why);
	} else {
		fprintf(stderr, "unrecognised command \"%s\"\n", cmd);
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
	int		 ssh;
	int		 ok;
	int		 i;

	memset(&pgp, 0x0, sizeof(pgp));
	keyring = NULL;
	ssh = 0;
	ok = 1;
	cmd = "verify";
	while ((i = getopt(argc, argv, "S:c:k:v")) != -1) {
		switch(i) {
		case 'S':
			ssh = 1;
			keyring = optarg;
			break;
		case 'c':
			cmd = optarg;
			break;
		case 'k':
			keyring = optarg;
			break;
		case 'v':
			printf("%s\n", NETPGPVERIFY_VERSION);
			exit(EXIT_SUCCESS);
		default:
			break;
		}
	}
	if (ssh) {
		if (!pgpv_read_ssh_pubkeys(&pgp, keyring, -1)) {
			fprintf(stderr, "can't read ssh keyring\n");
			exit(EXIT_FAILURE);
		}
	} else if (!pgpv_read_pubring(&pgp, keyring, -1)) {
		fprintf(stderr, "can't read keyring\n");
		exit(EXIT_FAILURE);
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

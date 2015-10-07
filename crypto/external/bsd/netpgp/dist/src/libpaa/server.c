/*-
 * Copyright (c) 2010 Alistair Crooks <agc@NetBSD.org>
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

#include <netdb.h>

#include <netpgp.h>
#include <regex.h>
#include <sha1.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "libpaa.h"

#define DEFAULT_HASH_ALG "SHA256"

int
main(int argc, char **argv)
{
	paa_server_info_t	server;
	paa_challenge_t		challenge;
	paa_identity_t		id;
	netpgp_t		netpgp;
	char			buf[2048];
	int			secretc;
	int			cc;
	int			i;

	(void) memset(&server, 0x0, sizeof(server));
	(void) memset(&challenge, 0x0, sizeof(challenge));
	(void) memset(&id, 0x0, sizeof(id));
	(void) memset(&netpgp, 0x0, sizeof(netpgp));
	secretc = 64;
	while ((i = getopt(argc, argv, "S:c:d:r:u:")) != -1) {
		switch(i) {
		case 'S':
			netpgp_setvar(&netpgp, "ssh keys", "1");
			netpgp_setvar(&netpgp, "sshkeyfile", optarg);
			break;
		case 'c':
			secretc = atoi(optarg);
			break;
		case 'd':
			challenge.domain = optarg;
			break;
		case 'r':
			challenge.realm = optarg;
			break;
		case 'u':
			netpgp_setvar(&netpgp, "userid", optarg);
			break;
		}
	}
	netpgp_setvar(&netpgp, "hash", DEFAULT_HASH_ALG);
	netpgp_setvar(&netpgp, "need seckey", "1");
	netpgp_setvar(&netpgp, "need userid", "1");
	netpgp_set_homedir(&netpgp, getenv("HOME"),
			netpgp_getvar(&netpgp, "ssh keys") ? "/.ssh" : "/.gnupg", 1);
	if (!netpgp_init(&netpgp)) {
		(void) fprintf(stderr, "can't initialise netpgp\n");
		exit(EXIT_FAILURE);
	}
	if (!paa_server_init(&server, secretc)) {
		(void) fprintf(stderr, "can't initialise paa server\n");
		exit(EXIT_FAILURE);
	}
	/* format the challenge */
	cc = paa_format_challenge(&challenge, &server, buf, sizeof(buf));
	/* write challenge to temp file */
	paa_write_file("challenge", buf, cc);
	/* get the client to authenticate via paa, writing to temp response file */
	system("client/paaclient -r authentication@bigco.com < challenge > response");
	/* read in response */
	cc = paa_read_file("response", buf, sizeof(buf));
	if (!paa_check_response(&challenge, &id, &netpgp, buf)) {
		(void) fprintf(stderr, "server: paa_check failed\n");
		exit(EXIT_FAILURE);
	}
	printf("paa_check_response verified challenge: signature authenticated:\n");
	paa_print_identity(stdout, &id);
	printf("Changing buf[%d] from '%c' to '%c'\n", cc / 2, buf[cc / 2], buf[cc / 2] - 1);
	buf[cc / 2] = buf[cc / 2] - 1;
	if (paa_check_response(&challenge, &id, &netpgp, buf)) {
		(void) fprintf(stderr, "server: unexpected paa_check pass\n");
		exit(EXIT_FAILURE);
	}
	printf("paa_check_response verified challenge: signature not authenticated:\n");
	exit(EXIT_SUCCESS);
}

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
#include <sys/param.h>

#include <inttypes.h>
#include <netpgp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "b64.h"
#include "hkpc.h"

#define DEFAULT_NUMBITS 2048

#define DEFAULT_HASH_ALG "SHA256"

int
main(int argc, char **argv)
{
	netpgp_t	 netpgp;
	char		*res;
	char		 key[8192];
	char		 asc[8192];
	char	 	 server[BUFSIZ];
	char		*cp;
	int	 	 family;
	int	 	 port;
	int	 	 keyc;
	int	 	 ascc;
	int	 	 ok;
	int	 	 i;

	(void) memset(&netpgp, 0x0, sizeof(netpgp));
	port = 11371;
	family = 4;
	(void) snprintf(server, sizeof(server), "localhost");
	while ((i = getopt(argc, argv, "f:h:p:")) != -1) {
		switch(i) {
		case 'f':
			family = atoi(optarg);
			break;
		case 'h':
			(void) snprintf(server, sizeof(server), optarg);
			break;
		case 'p':
			port = atoi(optarg);
			break;
		default:
			break;
		}
	}
	netpgp_setvar(&netpgp, "ssh keys", "1");
	netpgp_setvar(&netpgp, "hash", DEFAULT_HASH_ALG);
	netpgp_set_homedir(&netpgp, getenv("HOME"), "/.ssh", 1);
	for (ok = 1, i = optind ; i < argc ; i++) {
		if (!hkpc_get(&res, server, port, family, "get", argv[i])) {
			(void) fprintf(stderr, "No such key '%s'\n", argv[i]);
			ok = 0;
		}
		if ((keyc = netpgp_write_sshkey(&netpgp, res, argv[i], key, sizeof(key))) <= 0) {
			(void) fprintf(stderr, "can't netpgp_write_sshkey '%s'\n", argv[i]);
			ok = 0;
		}
		for (cp = &key[keyc - 1] ; cp > key && *cp != ' ' ; --cp) {
		}
		if (cp == key) {
			cp = argv[i];
		} else {
			cp += 1;
		}
		/* btoa */
		ascc = b64encode(key, keyc, asc, sizeof(asc), 0xffffffff);
		/* write to .ssh/id_c0596823.pub */
		printf("ssh-rsa %.*s %s\n", ascc, asc, cp);
	}
	exit((ok) ? EXIT_SUCCESS : EXIT_FAILURE);
}

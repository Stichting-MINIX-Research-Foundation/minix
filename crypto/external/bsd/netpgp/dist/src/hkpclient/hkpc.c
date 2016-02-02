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
#include <sys/socket.h>

#include <netinet/in.h>

#include <errno.h>
#include <inttypes.h>
#include <netdb.h>
#include <netpgp.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hkpc.h"

/* get a socket and connect it to the server */
int
hkpc_connect(const char *hostname, const int port, const int fam)
{
        struct addrinfo  hints;
        struct addrinfo *res;
        char             portstr[32];
	int		 sock;
        int              rc = 0;

        (void) memset(&hints, 0, sizeof(hints));
        hints.ai_family = (fam == 4) ? PF_INET : PF_INET6;
        hints.ai_socktype = SOCK_STREAM;
        (void) snprintf(portstr, sizeof(portstr), "%d", port);
        if ((rc = getaddrinfo(hostname, portstr, &hints, &res)) != 0) {
                hints.ai_flags = 0;
                if ((rc = getaddrinfo(hostname, "hkp", &hints, &res)) != 0) {
                        (void) fprintf(stderr, "getaddrinfo: %s",
					gai_strerror(rc));
                        return -1;
                }
        }
	if ((sock = socket((fam == 4) ? AF_INET : AF_INET6, SOCK_STREAM, 0)) < 0) {
                (void) fprintf(stderr, "socket failed %d\n", errno);
                freeaddrinfo(res);
                return -1;
	}
        if ((rc = connect(sock, res->ai_addr, res->ai_addrlen)) < 0) {
                (void) fprintf(stderr, "connect failed %d\n", errno);
                freeaddrinfo(res);
                return -1;
        }
        freeaddrinfo(res);
        if (rc < 0) {
                (void) fprintf(stderr, "connect() to %s:%d failed (rc %d)\n",
				hostname, port, rc);
        }
        return sock;
}

#define MB(x)	((x) * 1024 * 1024)

/* get required info from the server */
int
hkpc_get(char **info, const char *server, const int port, const int family, const char *type, const char *userid)
{
	char	buf[MB(1)];
	int	sock;
	int	cc;
	int	rc;

	if ((sock = hkpc_connect(server, port, family)) < 0) {
		(void) fprintf(stderr, "hkpc_get: can't connect to server '%s'\n", server);
		return -1;
	}
	cc = snprintf(buf, sizeof(buf), "GET /pks/lookup?op=%s&search=%s&options=json", type, userid);
	if (write(sock, buf, cc) != cc) {
		(void) fprintf(stderr, "hkpc_get: short write\n");
		return -1;
	}
	for (cc = 0 ; (rc = read(sock, &buf[cc], sizeof(buf) - cc)) > 0 ; cc += rc) {
	}
	*info = calloc(1, cc + 1);
	(void) memcpy(*info, buf, cc);
	(*info)[cc] = 0x0;
	(void) close(sock);
	return cc;
}

/* jump over http header, then pass the json to the key-formatting function */
int
hkpc_print_key(FILE *fp, const char *op, const char *res)
{
	static regex_t	text;
	static int	compiled;
	regmatch_t	matches[10];
	int	 	ret;

	if (!compiled) {
		compiled = 1;
		(void) regcomp(&text, "\r\n\r\n", REG_EXTENDED);
	}
	if (regexec(&text, res, 10, matches, 0) != 0) {
		return 0;
	}
	if (strcmp(op, "index") == 0 || strcmp(op, "vindex") == 0) {
		ret = netpgp_format_json(fp, &res[(int)matches[0].rm_eo], 1);
	} else {
		(void) fprintf(fp, "%s\n", &res[(int)matches[0].rm_eo]);
		ret = 1;
	}
	return ret;
}

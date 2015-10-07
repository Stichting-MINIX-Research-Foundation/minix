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
#include <sys/socket.h>

#include <arpa/inet.h>

#include <netinet6/in6.h>

#include <netdb.h>

#include <ifaddrs.h>
#include <netpgp.h>
#include <regex.h>
#include <sha1.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "libpaa.h"
#include "b64.h"

enum {
	MAX_DIGEST_SIZE	= 128
};

/* create an area of random memory */
static int
randomise(char *s, size_t size)
{
	uint32_t	r;
	size_t		i;

	for (i = 0 ; i < size ; i += sizeof(r)) {
		r = random();
		(void) memcpy(&s[i], &r, sizeof(r));
	}
	return i;
}

/* generate a challenge */
static int
genchallenge(paa_challenge_t *challenge, paa_server_info_t *server)
{
	time_t		 t;
	char		 digest[MAX_DIGEST_SIZE];
	char		 raw[PAA_CHALLENGE_SIZE * 2];
	int		 cc;

	t = time(NULL);
	cc = snprintf(raw, sizeof(raw), "%s;%s;%lld;", challenge->realm, server->hostaddress, (int64_t)t);
	cc += randomise(&raw[cc], 64);	/* 64 is arbitrary */
	/* raw now has the raw-challenge in it */
	challenge->encc = b64encode(raw, (const unsigned)cc, challenge->encoded_challenge,
		sizeof(challenge->encoded_challenge), 0);
	cc += snprintf(&raw[cc], sizeof(raw) - cc, ";%.*s", server->secretc, server->secret);
	(void) SHA1Data((uint8_t *)raw, (unsigned)cc, digest);
	server->server_signaturec = b64encode(digest, (const unsigned)strlen(digest),
		server->server_signature, sizeof(server->server_signature), (int)0);
	/* raw has raw-challenge ; server-secret-value, i.e. raw-server-signature */
	challenge->challengec = snprintf(challenge->challenge, sizeof(challenge->challenge),
		"%.*s;%.*s", server->server_signaturec, server->server_signature,
		challenge->encc, challenge->encoded_challenge);
	return challenge->challengec;
}

/* fill in the identity information in the response */
static int
fill_identity(paa_identity_t *id, char *response, char *raw_challenge)
{
	regmatch_t	matches[10];
	regex_t		response_re;
	regex_t		id_re;
	char		t[32];

	/* id="userid" */
	(void) regcomp(&id_re, "id=\"([^\"]+)\"", REG_EXTENDED);
	if (regexec(&id_re, response, 10, matches, 0) != 0) {
		(void) fprintf(stderr, "No identity information found\n");
		return 0;
	}
	(void) snprintf(id->userid, sizeof(id->userid), "%.*s",
		(int)(matches[1].rm_eo - matches[1].rm_so),
		&response[(int)matches[1].rm_so]);
	/* realm;ip;timestamp;seed */
	(void) regcomp(&response_re, "([^;]+);([^;]+);([^;]+);(.*)", REG_EXTENDED);
	if (regexec(&response_re, raw_challenge, 10, matches, 0) != 0) {
		(void) fprintf(stderr, "No identity information found\n");
		return 0;
	}
	(void) snprintf(id->realm, sizeof(id->realm), "%.*s",
		(int)(matches[1].rm_eo - matches[1].rm_so),
		&raw_challenge[(int)matches[1].rm_so]);
	(void) snprintf(id->client, sizeof(id->client), "%.*s",
		(int)(matches[2].rm_eo - matches[2].rm_so),
		&raw_challenge[(int)matches[2].rm_so]);
	(void) snprintf(t, sizeof(t), "%.*s",
		(int)(matches[3].rm_eo - matches[3].rm_so),
		&raw_challenge[(int)matches[3].rm_so]);
	id->timestamp = strtoll(t, NULL, 10);
	return 1;
}

/***************************************************************************/
/* exported functions start here */
/***************************************************************************/

/* initialise the server info */
int
paa_server_init(paa_server_info_t *server, unsigned secretsize)
{
	struct sockaddr_in6	*sin6;
	struct sockaddr_in	*sin;
	struct ifaddrs		*addrs;
	char			 host[512];

	if (getifaddrs(&addrs) < 0) {
		(void) fprintf(stderr, "can't getifaddrs\n");
		return 0;
	}
	for ( ; addrs ; addrs = addrs->ifa_next) {
		if (addrs->ifa_addr->sa_family == AF_INET) {
			sin = (struct sockaddr_in *)(void *)addrs->ifa_addr;
			(void) snprintf(server->hostaddress, sizeof(server->hostaddress), "%s",
				inet_ntoa(sin->sin_addr));
			break;
		}
		if (addrs->ifa_addr->sa_family == AF_INET6) {
			sin6 = (struct sockaddr_in6 *)(void *)addrs->ifa_addr;
			(void) getnameinfo((const struct sockaddr *)(void *)sin6,
				(unsigned)sin6->sin6_len,
				server->hostaddress, sizeof(server->hostaddress),
				NULL, 0, NI_NUMERICHOST);
			break;
		}
	}
	if (addrs == NULL) {
		if (gethostname(host, sizeof(host)) < 0) {
			(void) fprintf(stderr, "can't get hostname\n");
			return 0;
		}
		(void) snprintf(server->hostaddress, sizeof(server->hostaddress), "%s", host);
	}
	if ((server->secret = calloc(1, server->secretc = secretsize)) == NULL) {
		(void) fprintf(stderr, "can't allocate server secret\n");
		return 0;
	}
	server->secretc = randomise(server->secret, secretsize);
	return 1;
}

/*
    challenge         = "PubKey.v1" pubkey-challenge

    pubkey-challenge  = 1#( realm | [domain] | challenge )

    realm             = "realm" "=" quoted-string
    domain            = "domain" "=" <"> URI ( 1*SP URI ) <">
    URI               = absoluteURI | abs_path
    challenge         = "challenge" "=" quoted-string
*/

/* called from server to send the challenge */
int
paa_format_challenge(paa_challenge_t *challenge, paa_server_info_t *server, char *buf, size_t size)
{
	int	cc;

	if (challenge->realm == NULL) {
		(void) fprintf(stderr, "paa_format_challenge: no realm information\n");
		return 0;
	}
	cc = snprintf(buf, size, "401 Unauthorized\r\nWWW-Authenticate: PubKey.v1\r\n");
	(void) genchallenge(challenge, server);
	cc += snprintf(&buf[cc], size - cc, "    challenge=\"%s\"", challenge->challenge);
	if (challenge->realm) {
		cc += snprintf(&buf[cc], size - cc, ",\r\n    realm=\"%s\"", challenge->realm);
	}
	if (challenge->domain) {
		cc += snprintf(&buf[cc], size - cc, ",\r\n    domain=\"%s\"", challenge->domain);
	}
	cc += snprintf(&buf[cc], size - cc, "\r\n");
	return cc;
}

/*
    credentials          = "PubKey.v1" privkey-credentials

    privkey-credentials  = 1#( identifier | realm | challenge | signature )

    identifier           = "id" "=" identifier-value
    identifier-value     = quoted-string
    challenge            = "challenge" "=" challenge-value
    challenge-value      = quoted-string
    signature            = "signature" "=" signature-value
    signature-value      = quoted-string
*/

/* called from client to respond to the challenge */
int
paa_format_response(paa_response_t *response, netpgp_t *netpgp, char *in, char *out, size_t outsize)
{
	regmatch_t	matches[10];
	regex_t		r;
	char		challenge[2048 * 2];
	char		base64_signature[2048 * 2];
	char		sig[2048];
	int		challengec;
	int		sig64c;
	int		sigc;
	int		outc;

	if (response->realm == NULL) {
		(void) fprintf(stderr, "paa_format_response: no realm information\n");
		return 0;
	}
	(void) regcomp(&r, "challenge=\"([^\"]+)\"", REG_EXTENDED);
	if (regexec(&r, in, 10, matches, 0) != 0) {
		(void) fprintf(stderr, "no signature found\n");
		return 0;
	}
	challengec = snprintf(challenge, sizeof(challenge), "%.*s",
		(int)(matches[1].rm_eo - matches[1].rm_so), &in[(int)matches[1].rm_so]);
	/* read challenge string */
	outc = snprintf(out, outsize, "Authorization: PubKey.v1\r\n");
	response->userid = netpgp_getvar(netpgp, "userid");
	outc += snprintf(&out[outc], outsize - outc, "    id=\"%s\"", response->userid);
	outc += snprintf(&out[outc], outsize - outc, ",\r\n    challenge=\"%s\"", challenge);
	outc += snprintf(&out[outc], outsize - outc, ",\r\n    realm=\"%s\"", response->realm);
	/* set up response */
	(void) memset(sig, 0x0, sizeof(sig));
	(void) snprintf(sig, sizeof(sig), "%s;%s;%s;", response->userid, response->realm, challenge);
	sigc = netpgp_sign_memory(netpgp, response->userid, challenge,
		(unsigned)challengec, sig, sizeof(sig), 0, 0);
	sig64c = b64encode(sig, (const unsigned)sigc, base64_signature,
		sizeof(base64_signature), (int)0);
	outc += snprintf(&out[outc], outsize - outc, ",\r\n    signature=\"%.*s\"", sig64c, base64_signature);
	return outc;
}

/* called from server to check the response to the challenge */
int
paa_check_response(paa_challenge_t *challenge, paa_identity_t *id, netpgp_t *netpgp, char *response)
{
	regmatch_t	matches[10];
	regex_t		challenge_regex;
	regex_t		signature_regex;
	regex_t		realm_regex;
	time_t		t;
	char		encoded_challenge[512];
	char		raw_challenge[512];
	char		verified[2048];
	char		realm[128];
	char		buf[2048];
	int		bufc;

	/* grab the signed text from the response */
	(void) regcomp(&signature_regex, "signature=\"([^\"]+)\"", REG_EXTENDED);
	if (regexec(&signature_regex, response, 10, matches, 0) != 0) {
		(void) fprintf(stderr, "paa_check: no signature found\n");
		return 0;
	}
	/* atob the signature itself */
	bufc = b64decode(&response[(int)matches[1].rm_so],
		(size_t)(matches[1].rm_eo - matches[1].rm_so), buf, sizeof(buf));
	/* verify the signature */
	(void) memset(verified, 0x0, sizeof(verified));
	if (netpgp_verify_memory(netpgp, buf, (const unsigned)bufc, verified, sizeof(verified), 0) <= 0) {
		(void) fprintf(stderr, "paa_check: signature cannot be verified\n");
		return 0;
	}
	/* we check the complete signed text against our challenge */
	if (strcmp(challenge->challenge, verified) != 0) {
		(void) fprintf(stderr, "paa_check: signature does not match\n");
		return 0;
	}
	(void) regcomp(&challenge_regex, "^([^;]+);(.+)", REG_EXTENDED);
	if (regexec(&challenge_regex, verified, 10, matches, 0) != 0) {
		(void) fprintf(stderr, "paa_check: no 2 parts to challenge\n");
		return 0;
	}
	/* we know server signature matches from comparison on whole challenge above */
	(void) snprintf(encoded_challenge, sizeof(encoded_challenge), "%.*s",
		(int)(matches[2].rm_eo - matches[2].rm_so), &verified[(int)matches[2].rm_so]);
	(void) b64decode(&verified[(int)matches[2].rm_so],
		(const unsigned)(matches[2].rm_eo - matches[2].rm_so),
		raw_challenge, sizeof(raw_challenge));
	if (!fill_identity(id, response, raw_challenge)) {
		(void) fprintf(stderr, "paa_check: identity problems\n");
		return 0;

	}
	/* check realm info in authentication header matches signed realm */
	(void) regcomp(&realm_regex, "realm=\"([^\"]+)\"", REG_EXTENDED);
	if (regexec(&realm_regex, response, 10, matches, 0) != 0) {
		(void) fprintf(stderr, "paa_check: no realm found\n");
		return 0;
	}
	(void) snprintf(realm, sizeof(realm), "%.*s",
		(int)(matches[1].rm_eo - matches[1].rm_so),
		&response[(int)matches[1].rm_so]);
	if (strcmp(id->realm, realm) != 0) {
		(void) fprintf(stderr, "paa_check: realm mismatch: signed realm '%s' vs '%s'\n",
			id->realm, realm);
		return 0;
	}
	/* check timestamp is within bounds */
	t = time(NULL);
	if (id->timestamp < t - (3 * 60)) {
		(void) fprintf(stderr, "paa_check: timestamp check: %lld seconds ago\n",
			t - id->timestamp);
		return 0;
	}
	if (id->timestamp > t + (3 * 60)) {
		(void) fprintf(stderr, "paa_check: timestamp check: %lld seconds in future\n",
			id->timestamp - t);
		return 0;
	}
	return 1;
}

/* print identity details on a stream */
int
paa_print_identity(FILE *fp, paa_identity_t *id)
{
	(void) fprintf(fp, "\tuserid\t%s\n\tclient\t%s\n\trealm\t%s\n\ttime\t%.24s\n",
		id->userid,
		id->client,
		id->realm,
		ctime(&id->timestamp));
	return 1;
}

/* utility function to write a string to a file */
int
paa_write_file(const char *f, char *s, unsigned cc)
{
	FILE	*fp;

	if ((fp = fopen(f, "w")) == NULL) {
		(void) fprintf(stderr, "can't write file '%s'\n", f);
		return 0;
	}
	write(fileno(fp), s, cc);
	(void) fclose(fp);
	return 1;
}

/* utility function to read a string from a file */
int
paa_read_file(const char *f, char *s, size_t size)
{
	FILE	*fp;
	int	 cc;

	if ((fp = fopen(f, "r")) == NULL) {
		(void) fprintf(stderr, "can't write '%s'\n", f);
		return 0;
	}
	cc = read(fileno(fp), s, size);
	(void) fclose(fp);
	return cc;
}

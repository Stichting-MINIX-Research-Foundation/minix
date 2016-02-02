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
#ifndef LIBPAA_H_
#define LIBPAA_H_	20100908

#include <sys/types.h>

#include <inttypes.h>
#include <stdio.h>

#define DEFAULT_HASH_ALG "SHA256"

enum {
	PAA_CHALLENGE_SIZE	= 128
};

/* constant and secret info for server side */
typedef struct paa_server_info_t {
	char		 hostaddress[128];		/* host ip address */
	char		*secret;			/* raw secret of server */
	unsigned	 secretc;			/* # of characters used */
	char		 server_signature[512];		/* this is the encoded signature */
	int		 server_signaturec;		/* # of chars in encoded sig */
} paa_server_info_t;

/* used in server to formulate challenge */
typedef struct paa_challenge_t {
	const char	*realm;				/* this is realm of challenge */
	const char	*domain;			/* domain of challenge */
	char		 challenge[512];		/* the output challenge */
	int		 challengec;			/* # of chars in challenge */
	/* sub-parts of challenge */
	char		 encoded_challenge[512];	/* encoded challenge part */
	int		 encc;				/* # of chars in encoded challenge */
} paa_challenge_t;

/* used in client to formulate response */
typedef struct paa_response_t {
	const char	*userid;			/* identity to be used for signature */
	const char	*realm;				/* realm that client wants */
	char		 challenge[PAA_CHALLENGE_SIZE];	/* input challenge */
	int		 challengec;			/* # if chars in input */
	char		 response[PAA_CHALLENGE_SIZE * 2];	/* output response */
	int		 respc;				/* # of chars in output */
} paa_response_t;

/* this struct holds the identity information in the paa response */
typedef struct paa_identity_t {
	char		 userid[32];		/* verified identity */
	char		 client[128];		/* client address */
	char		 realm[128];		/* client realm */
	char		 domain[128];		/* client domain */
	int64_t		 timestamp;		/* time of response */
} paa_identity_t;

/* support functions */
int paa_write_file(const char *, char *, unsigned);
int paa_read_file(const char *, char *, size_t);

/* server initialisations - one time */
int paa_server_init(paa_server_info_t *, unsigned);

/* body of pubkey access authentication challenge/response/check functionality */
int paa_format_challenge(paa_challenge_t *, paa_server_info_t *, char *, size_t);
int paa_format_response(paa_response_t *, netpgp_t *, char *, char *, size_t);
int paa_check_response(paa_challenge_t *, paa_identity_t *, netpgp_t *, char *);

/* who are ya? */
int paa_print_identity(FILE *, paa_identity_t *);

#endif

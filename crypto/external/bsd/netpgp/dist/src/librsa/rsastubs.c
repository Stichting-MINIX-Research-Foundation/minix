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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

#include "rsa.h"
#include "rsastubs.h"

#ifndef USE_ARG
#define USE_ARG(x)	/*LINTED*/(void)&(x)
#endif

static int
pass_cb(char *buf, int size, int rwflag, void *u)
{
	char	*passphrase;
	char	 prompt[128];

	USE_ARG(rwflag);
	snprintf(prompt, sizeof(prompt), "\"%s\" passphrase: ", (char *)u);
	if ((passphrase = getpass(prompt)) == NULL) {
		return -1;
	}
	(void) memcpy(buf, passphrase, (size_t)size);
	return (int)strlen(passphrase);
}

RSA *
PEM_read_RSAPrivateKey(FILE *fp, RSA **x, pem_password_cb *cb, void *u)
{
	char	 phrase[128 + 1];
	RSA	*rsa;
	int	 cc;

fprintf(stderr, "Stubbed PEM_read_RSAPrivateKey\n");
	USE_ARG(u);
	if (cb == NULL) {
		cb = pass_cb;
	}
	cc = (*cb)(phrase, sizeof(phrase), 0, u);
	rsa = *x = RSA_new();
	USE_ARG(fp);
	return rsa;
}

DSA *
PEM_read_DSAPrivateKey(FILE *fp, DSA **x, pem_password_cb *cb, void *u)
{
	DSA	*dsa;

	USE_ARG(u);
	if (cb == NULL) {
		cb = pass_cb;
	}
	dsa = *x = DSA_new();
	USE_ARG(fp);
	return dsa;
}

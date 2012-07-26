/*
 * Copyright (C) 2009  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NETWORK ASSOCIATES DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Portions copyright (c) 2008 Nominet UK.  All rights reserved.
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

/* $Id: pkcs11-keygen.c,v 1.9 2009-10-26 23:36:53 each Exp $ */

/* pkcs11-keygen - pkcs11 rsa key generator
*
* create RSASHA1 key in the keystore of an SCA6000
* The calculation of key tag is left to the script
* that converts the key into a DNSKEY RR and inserts 
* it into a zone file.
*
* usage:
* pkcs11-keygen [-P] [-m module] [-s slot] [-e] -b keysize
*               -l label [-i id] [-p pin] 
*
*/

/*! \file */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include "cryptoki.h"

#ifdef WIN32
#include "win32.c"
#else
#ifndef FORCE_STATIC_PROVIDER
#include "unix.c"
#endif
#endif

#if !(defined(HAVE_GETPASSPHRASE) || (defined (__SVR4) && defined (__sun)))
#define getpassphrase(x)	getpass(x)
#endif

/* Define static key template values */
static CK_BBOOL truevalue = TRUE;
static CK_BBOOL falsevalue = FALSE;

int
main(int argc, char *argv[])
{
	CK_RV rv;
	CK_SLOT_ID slot = 0;
	CK_MECHANISM genmech;
	CK_SESSION_HANDLE hSession;
	CK_UTF8CHAR *pin = NULL;
	CK_ULONG modulusbits = 0;
	CK_CHAR *label = NULL;
	CK_OBJECT_HANDLE privatekey, publickey;
	CK_BYTE public_exponent[5];
	CK_ULONG expsize = 3;
	int error = 0;
	int c, errflg = 0;
	int hide = 1;
	int idlen = 0;
	unsigned long id = 0;
	CK_BYTE idbuf[4];
	CK_ULONG ulObjectCount;
	/* Set search template */
	CK_ATTRIBUTE search_template[] = {
		{CKA_LABEL, NULL_PTR, 0}
	};
	CK_ATTRIBUTE publickey_template[] = {
		{CKA_LABEL, NULL_PTR, 0},
		{CKA_VERIFY, &truevalue, sizeof(truevalue)},
		{CKA_TOKEN, &truevalue, sizeof(truevalue)},
		{CKA_MODULUS_BITS, &modulusbits, sizeof(modulusbits)},
		{CKA_PUBLIC_EXPONENT, &public_exponent, expsize},
		{CKA_ID, &idbuf, idlen}
	};
	CK_ULONG publickey_attrcnt = 6;
	CK_ATTRIBUTE privatekey_template[] = {
		{CKA_LABEL, NULL_PTR, 0},
		{CKA_SIGN, &truevalue, sizeof(truevalue)},
		{CKA_TOKEN, &truevalue, sizeof(truevalue)},
		{CKA_PRIVATE, &truevalue, sizeof(truevalue)},
		{CKA_SENSITIVE, &truevalue, sizeof(truevalue)},
		{CKA_EXTRACTABLE, &falsevalue, sizeof(falsevalue)},
		{CKA_ID, &idbuf, idlen}
	};
	CK_ULONG privatekey_attrcnt = 7;
	char *pk11_provider;
	extern char *optarg;
	extern int optopt;

	pk11_provider = getenv("PKCS11_PROVIDER");
	if (pk11_provider != NULL)
		pk11_libname = pk11_provider;

	while ((c = getopt(argc, argv, ":Pm:s:b:ei:l:p:")) != -1) {
		switch (c) {
		case 'P':
			hide = 0;
			break;
		case 'm':
			pk11_libname = optarg;
			break;
		case 's':
			slot = atoi(optarg);
			break;
		case 'e':
			expsize = 5;
			break;
		case 'b':
			modulusbits = atoi(optarg);
			break;
		case 'l':
			label = (CK_CHAR *)optarg;
			break;
		case 'i':
			id = strtoul(optarg, NULL, 0);
			idlen = 4;
			break;
		case 'p':
			pin = (CK_UTF8CHAR *)optarg;
			break;
		case ':':
			fprintf(stderr,
				"Option -%c requires an operand\n",
				optopt);
			errflg++;
			break;
		case '?':
		default:
			fprintf(stderr, "Unrecognised option: -%c\n", optopt);
			errflg++;
		}
	}

	if (errflg || !modulusbits || (label == NULL)) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "\tpkcs11-keygen -b keysize -l label\n");
		fprintf(stderr, "\t              [-P] [-m module] "
				"[-s slot] [-e] [-i id] [-p PIN]\n");
		exit(2);
	}
	
	search_template[0].pValue = label;
	search_template[0].ulValueLen = strlen((char *)label);
	publickey_template[0].pValue = label;
	publickey_template[0].ulValueLen = strlen((char *)label);
	privatekey_template[0].pValue = label;
	privatekey_template[0].ulValueLen = strlen((char *)label);

	/* Set public exponent to F4 or F5 */
	public_exponent[0] = 0x01;
	public_exponent[1] = 0x00;
	if (expsize == 3)
		public_exponent[2] = 0x01;
	else {
		publickey_template[4].ulValueLen = expsize;
		public_exponent[2] = 0x00;
		public_exponent[3] = 0x00;
		public_exponent[4] = 0x01;
	}

	/* Set up mechanism for generating key pair */
	genmech.mechanism = CKM_RSA_PKCS_KEY_PAIR_GEN;
	genmech.pParameter = NULL_PTR;
	genmech.ulParameterLen = 0;

	if (idlen == 0) {
		publickey_attrcnt--;
		privatekey_attrcnt--;
	} else if (id <= 0xffff) {
		idlen = 2;
		publickey_template[5].ulValueLen = idlen;
		privatekey_template[6].ulValueLen = idlen;
		idbuf[0] = (CK_BYTE)(id >> 8);
		idbuf[1] = (CK_BYTE)id;
	} else {
		idbuf[0] = (CK_BYTE)(id >> 24);
		idbuf[1] = (CK_BYTE)(id >> 16);
		idbuf[2] = (CK_BYTE)(id >> 8);
		idbuf[3] = (CK_BYTE)id;
	}

	/* Initialize the CRYPTOKI library */
	rv = C_Initialize(NULL_PTR);

	if (rv != CKR_OK) {
		if (rv == 0xfe)
			fprintf(stderr,
				"Can't load or link module \"%s\"\n",
				pk11_libname);
		else
			fprintf(stderr, "C_Initialize: Error = 0x%.8lX\n", rv);
		exit(1);
	}

	/* Open a session on the slot found */
	rv = C_OpenSession(slot, CKF_RW_SESSION+CKF_SERIAL_SESSION,
			   NULL_PTR, NULL_PTR, &hSession);

	if (rv != CKR_OK) {
		fprintf(stderr, "C_OpenSession: Error = 0x%.8lX\n", rv);
		error = 1;
		goto exit_program;
	}

	/* Login to the Token (Keystore) */
	if (pin == NULL)
		pin = (CK_UTF8CHAR *)getpassphrase("Enter Pin: ");

	rv = C_Login(hSession, CKU_USER, pin, strlen((char *)pin));
	memset(pin, 0, strlen((char *)pin));
	if (rv != CKR_OK) {
		fprintf(stderr, "C_Login: Error = 0x%.8lX\n", rv);
		error = 1;
		goto exit_session;
	}

	/* check if a key with the same id already exists */
	rv = C_FindObjectsInit(hSession, search_template, 1); 
	if (rv != CKR_OK) {
		fprintf(stderr, "C_FindObjectsInit: Error = 0x%.8lX\n", rv);
		error = 1;
		goto exit_session;
	}
	rv = C_FindObjects(hSession, &privatekey, 1, &ulObjectCount);
	if (rv != CKR_OK) {
		fprintf(stderr, "C_FindObjects: Error = 0x%.8lX\n", rv);
		error = 1;
		goto exit_search;
	}
	if (ulObjectCount != 0) {
		fprintf(stderr, "Key already exists.\n");
		error = 1;
		goto exit_search;
	}

	/* Set attributes if the key is not to be hidden */
	if (!hide) {
		privatekey_template[4].pValue = &falsevalue;
		privatekey_template[5].pValue = &truevalue;
	}

	/* Generate Key pair for signing/verifying */
	rv = C_GenerateKeyPair(hSession, &genmech,
			       publickey_template, publickey_attrcnt,
			       privatekey_template, privatekey_attrcnt,
			       &publickey, &privatekey);
	
	if (rv != CKR_OK) {
		fprintf(stderr, "C_GenerateKeyPair: Error = 0x%.8lX\n", rv);
		error = 1;
	}
	
 exit_search:
	rv = C_FindObjectsFinal(hSession);
	if (rv != CKR_OK) {
		fprintf(stderr, "C_FindObjectsFinal: Error = 0x%.8lX\n", rv);
		error = 1;
	}

 exit_session:
	(void)C_CloseSession(hSession);

 exit_program:
	(void)C_Finalize(NULL_PTR);

	exit(error);
}

/*	$NetBSD: verify.c,v 1.1.1.4 2014/12/10 03:34:28 christos Exp $	*/

/*
 * Copyright (C) 2014  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
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

/* Id */

/* verify [-m module] [-s $slot] [-p pin] [-t] [-n count] */

/*! \file */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <isc/commandline.h>
#include <isc/result.h>
#include <isc/types.h>

#include <pk11/pk11.h>
#include <pk11/result.h>

#if !(defined(HAVE_GETPASSPHRASE) || (defined (__SVR4) && defined (__sun)))
#define getpassphrase(x)	getpass(x)
#endif

#ifndef HAVE_CLOCK_GETTIME
#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif

int
clock_gettime(int32_t id, struct timespec *tp)
{
	struct timeval tv;
	int result;

	result = gettimeofday(&tv, NULL);
	if (result)
		return (result);
	tp->tv_sec = tv.tv_sec;
	tp->tv_nsec = (long) tv.tv_usec * 1000;
	return (result);
}
#endif

CK_BYTE modulus[] = {
	0x00, 0xb7, 0x9c, 0x1f, 0x05, 0xa3, 0xc2, 0x99,
	0x44, 0x82, 0x20, 0x78, 0x43, 0x7f, 0x5f, 0x3b,
	0x10, 0xd7, 0x9e, 0x61, 0x42, 0xd2, 0x7a, 0x90,
	0x50, 0x8a, 0x99, 0x33, 0xe7, 0xca, 0xc8, 0x5f,
	0x16, 0x1c, 0x56, 0xf8, 0xc1, 0x06, 0x2f, 0x96,
	0xe7, 0x54, 0xf2, 0x85, 0x89, 0x41, 0x36, 0xf5,
	0x4c, 0xa4, 0x0d, 0x62, 0xd3, 0x42, 0x51, 0x6b,
	0x9f, 0xdc, 0x36, 0xcb, 0xad, 0x56, 0xf4, 0xbd,
	0x2a, 0x60, 0x33, 0xb1, 0x7a, 0x99, 0xad, 0x08,
	0x9f, 0x95, 0xe8, 0xe5, 0x14, 0xd9, 0x68, 0x79,
	0xca, 0x4e, 0x72, 0xeb, 0xfb, 0x2c, 0xf1, 0x45,
	0xd3, 0x33, 0x65, 0xe7, 0xc5, 0x11, 0xdd, 0xe7,
	0x09, 0x83, 0x13, 0xd5, 0x17, 0x1b, 0xf4, 0xbd,
	0x49, 0xdd, 0x8a, 0x3c, 0x3c, 0xf7, 0xa1, 0x5d,
	0x7b, 0xb4, 0xd3, 0x80, 0x25, 0xf4, 0x05, 0x8f,
	0xbc, 0x2c, 0x2a, 0x47, 0xff, 0xd1, 0xc8, 0x34,
	0xbf
};
CK_BYTE exponent[] = { 0x01, 0x00, 0x01 };

CK_BYTE buf[1024];
CK_BYTE sig[128];

static CK_BBOOL truevalue = TRUE;
static CK_BBOOL falsevalue = FALSE;

int
main(int argc, char *argv[]) {
	isc_result_t result;
	CK_RV rv;
	CK_SLOT_ID slot = 0;
	CK_SESSION_HANDLE hSession = CK_INVALID_HANDLE;
	CK_ULONG len;
	CK_ULONG slen;
	CK_OBJECT_HANDLE hKey = CK_INVALID_HANDLE;
	CK_OBJECT_CLASS kClass = CKO_PUBLIC_KEY;
	CK_KEY_TYPE kType = CKK_RSA;
	CK_ATTRIBUTE kTemplate[] =
	{
		{ CKA_CLASS, &kClass, (CK_ULONG) sizeof(kClass) },
		{ CKA_KEY_TYPE, &kType, (CK_ULONG) sizeof(kType) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_VERIFY, &truevalue, (CK_ULONG) sizeof(truevalue) },
		{ CKA_MODULUS, modulus, (CK_ULONG) sizeof(modulus) },
		{ CKA_PUBLIC_EXPONENT, exponent, (CK_ULONG) sizeof(exponent) }
	};
	CK_MECHANISM mech = { CKM_SHA1_RSA_PKCS, NULL, 0 };
	pk11_context_t pctx;
	pk11_optype_t op_type = OP_RSA;
	char *lib_name = NULL;
	char *pin = NULL;
	int error = 0;
	int c, errflg = 0;
	int ontoken  = 0;
	unsigned int count = 1000;
	unsigned int i;
	struct timespec starttime;
	struct timespec endtime;

	while ((c = isc_commandline_parse(argc, argv, ":m:s:p:tn:")) != -1) {
		switch (c) {
		case 'm':
			lib_name = isc_commandline_argument;
			break;
		case 's':
			slot = atoi(isc_commandline_argument);
			op_type = OP_ANY;
			break;
		case 'p':
			pin = isc_commandline_argument;
			break;
		case 't':
			ontoken = 1;
			break;
		case 'n':
			count = atoi(isc_commandline_argument);
			break;
		case ':':
			fprintf(stderr,
				"Option -%c requires an operand\n",
				isc_commandline_option);
			errflg++;
			break;
		case '?':
		default:
			fprintf(stderr, "Unrecognised option: -%c\n",
				isc_commandline_option);
			errflg++;
		}
	}

	if (errflg) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr,
			"\tverify [-m module] [-s slot] [-p pin] "
			"[-t] [-n count]\n");
		exit(1);
	}

	pk11_result_register();

	/* Initialize the CRYPTOKI library */
	if (lib_name != NULL)
		pk11_set_lib_name(lib_name);

	if (pin == NULL)
		pin = getpassphrase("Enter Pin: ");

	result = pk11_get_session(&pctx, op_type, ISC_FALSE, ISC_TRUE,
				  ISC_TRUE, (const char *) pin, slot);
	if ((result != ISC_R_SUCCESS) &&
	    (result != PK11_R_NORANDOMSERVICE) &&
	    (result != PK11_R_NODIGESTSERVICE) &&
	    (result != PK11_R_NOAESSERVICE)) {
		fprintf(stderr, "Error initializing PKCS#11: %s\n",
			isc_result_totext(result));
		exit(1);
	}

	if (pin != NULL)
		memset(pin, 0, strlen((char *)pin));

	hSession = pctx.session;

	/* Create the private RSA key */
	if (ontoken)
		kTemplate[2].pValue = &truevalue;

	rv = pkcs_C_CreateObject(hSession, kTemplate, 7, &hKey);
	if (rv != CKR_OK) {
		fprintf(stderr, "C_CreateObject: Error = 0x%.8lX\n", rv);
		error = 1;
		goto exit_key;
	}

	/* Randomize the buffer */
	len = (CK_ULONG) sizeof(buf);
	rv = pkcs_C_GenerateRandom(hSession, buf, len);
	if (rv != CKR_OK) {
		fprintf(stderr, "C_GenerateRandom: Error = 0x%.8lX\n", rv);
		goto exit_key;
	}

	if (clock_gettime(CLOCK_REALTIME, &starttime) < 0) {
		perror("clock_gettime(start)");
		goto exit_key;
	}

	for (i = 0; i < count; i++) {
		/* Initialize Verify */
		rv = pkcs_C_VerifyInit(hSession, &mech, hKey);
		if (rv != CKR_OK) {
			fprintf(stderr,
				"C_VerifyInit[%u]: Error = 0x%.8lX\n",
				i, rv);
			error = 1;
			break;
		}

		/* Perform Verify */
		slen = (CK_ULONG) sizeof(sig);
		rv = pkcs_C_Verify(hSession, buf, len, sig, slen);
		if ((rv != CKR_OK) && (rv != CKR_SIGNATURE_INVALID)) {
			fprintf(stderr,
				"C_Verify[%u]: Error = 0x%.8lX\n",
				i, rv);
			error = 1;
			break;
		}
	}

	if (clock_gettime(CLOCK_REALTIME, &endtime) < 0) {
		perror("clock_gettime(end)");
		goto exit_key;
	}

	endtime.tv_sec -= starttime.tv_sec;
	endtime.tv_nsec -= starttime.tv_nsec;
	while (endtime.tv_nsec < 0) {
		endtime.tv_sec -= 1;
		endtime.tv_nsec += 1000000000;
	}
	printf("%u RSA verify in %ld.%09lds\n", i,
	       endtime.tv_sec, endtime.tv_nsec);
	if (i > 0)
		printf("%g RSA verify/s\n",
		       1024 * i / ((double) endtime.tv_sec +
				   (double) endtime.tv_nsec / 1000000000.));

    exit_key:
	if (hKey != CK_INVALID_HANDLE) {
		rv = pkcs_C_DestroyObject(hSession, hKey);
		if (rv != CKR_OK) {
			fprintf(stderr,
				"C_DestroyObject: Error = 0x%.8lX\n",
				rv);
			errflg = 1;
		}
	}

	pk11_return_session(&pctx);
	(void) pk11_finalize();

	exit(error);
}

/*	$NetBSD: create.c,v 1.1.1.4 2014/12/10 03:34:28 christos Exp $	*/

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

/* create [-m module] [-s $slot] [-p pin] [-t] [-n count] */

/*! \file */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <isc/commandline.h>
#include <isc/print.h>
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

CK_BYTE buf[1024];
char label[16];

static CK_BBOOL truevalue = TRUE;
static CK_BBOOL falsevalue = FALSE;

int
main(int argc, char *argv[]) {
	isc_result_t result;
	CK_RV rv;
	CK_SLOT_ID slot = 0;
	CK_SESSION_HANDLE hSession = CK_INVALID_HANDLE;
	CK_OBJECT_HANDLE *hKey;
	CK_OBJECT_CLASS kClass = CKO_DATA;
	CK_ULONG len = sizeof(buf);
	CK_ATTRIBUTE kTemplate[] =
	{
		{ CKA_CLASS, &kClass, (CK_ULONG) sizeof(kClass) },
		{ CKA_TOKEN, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_PRIVATE, &falsevalue, (CK_ULONG) sizeof(falsevalue) },
		{ CKA_LABEL, (CK_BYTE_PTR) label, (CK_ULONG) sizeof(label) },
		{ CKA_VALUE, buf, (CK_ULONG) sizeof(buf) }
	};
	pk11_context_t pctx;
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
			break;
		case 't':
			ontoken = 1;
			break;
		case 'p':
			pin = isc_commandline_argument;
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
			"\tcreate [-m module] [-s slot] [-t] [-n count]\n");
		exit(1);
	}

	pk11_result_register();

	/* Allocate hanles */
	hKey = (CK_SESSION_HANDLE *)
		malloc(count * sizeof(CK_SESSION_HANDLE));
	if (hKey == NULL) {
		perror("malloc");
		exit(1);
	}
	for (i = 0; i < count; i++)
		hKey[i] = CK_INVALID_HANDLE;

	/* Initialize the CRYPTOKI library */
	if (lib_name != NULL)
		pk11_set_lib_name(lib_name);

	if (pin == NULL)
		pin = getpassphrase("Enter Pin: ");

	result = pk11_get_session(&pctx, OP_ANY, ISC_TRUE, ISC_TRUE,
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

	/* Randomize the buffer */
	rv = pkcs_C_GenerateRandom(hSession, buf, len);
	if (rv != CKR_OK) {
		fprintf(stderr, "C_GenerateRandom: Error = 0x%.8lX\n", rv);
		goto exit_objects;
	}

	if (ontoken)
		kTemplate[1].pValue = &truevalue;

	if (clock_gettime(CLOCK_REALTIME, &starttime) < 0) {
		perror("clock_gettime(start)");
		goto exit_objects;
	}

	for (i = 0; i < count; i++) {
		(void) snprintf(label, sizeof(label), "obj%u", i);
		kTemplate[3].ulValueLen = strlen(label);
		rv = pkcs_C_CreateObject(hSession, kTemplate, 5, &hKey[i]);
		if (rv != CKR_OK) {
			fprintf(stderr,
				"C_CreateObject[%u]: Error = 0x%.8lX\n",
				i, rv);
			error = 1;
			if (i == 0)
				goto exit_objects;
			break;
		}
	}

	if (clock_gettime(CLOCK_REALTIME, &endtime) < 0) {
		perror("clock_gettime(end)");
		goto exit_objects;
	}

	endtime.tv_sec -= starttime.tv_sec;
	endtime.tv_nsec -= starttime.tv_nsec;
	while (endtime.tv_nsec < 0) {
		endtime.tv_sec -= 1;
		endtime.tv_nsec += 1000000000;
	}
	printf("%u created objects in %ld.%09lds\n", i,
	       endtime.tv_sec, endtime.tv_nsec);
	if (i > 0)
		printf("%g created objects/s\n",
		       1024 * i / ((double) endtime.tv_sec +
				   (double) endtime.tv_nsec / 1000000000.));

    exit_objects:
	for (i = 0; i < count; i++) {
		/* Destroy objects */
		if (hKey[i] == CK_INVALID_HANDLE)
			continue;
		rv = pkcs_C_DestroyObject(hSession, hKey[i]);
		if ((rv != CKR_OK) && !errflg) {
			fprintf(stderr,
				"C_DestroyObject[%u]: Error = 0x%.8lX\n",
				i, rv);
			errflg = 1;
		}
	}

	free(hKey);
	pk11_return_session(&pctx);
	(void) pk11_finalize();

	exit(error);
}

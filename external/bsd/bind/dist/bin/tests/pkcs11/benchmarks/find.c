/*	$NetBSD: find.c,v 1.1.1.4 2014/12/10 03:34:28 christos Exp $	*/

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

/* find [-m module] [-s $slot] [-p pin] [-n count] */

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

CK_BYTE label[] = "foo??bar!!";

int
main(int argc, char *argv[]) {
	isc_result_t result;
	CK_RV rv;
	CK_SLOT_ID slot = 0;
	CK_SESSION_HANDLE hSession = CK_INVALID_HANDLE;
	CK_ATTRIBUTE sTemplate[] =
	{
		{ CKA_LABEL, label, (CK_ULONG) sizeof(label) },
	};
	CK_OBJECT_HANDLE sKey = CK_INVALID_HANDLE;
	CK_ULONG found = 0;
	pk11_context_t pctx;
	pk11_optype_t op_type = OP_RSA;
	char *lib_name = NULL;
	char *pin = NULL;
	int error = 0;
	int c, errflg = 0;
	unsigned int count = 1000;
	unsigned int i;
	struct timespec starttime;
	struct timespec endtime;

	while ((c = isc_commandline_parse(argc, argv, ":m:s:p:n:")) != -1) {
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
			"\tfind [-m module] [-s slot] [-p pin] [-n count]\n");
		exit(1);
	}

	pk11_result_register();

	/* Initialize the CRYPTOKI library */
	if (lib_name != NULL)
		pk11_set_lib_name(lib_name);

	if (pin == NULL)
		pin = getpassphrase("Enter Pin: ");

	result = pk11_get_session(&pctx, op_type, ISC_FALSE, ISC_FALSE,
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

	if (clock_gettime(CLOCK_REALTIME, &starttime) < 0) {
		perror("clock_gettime(start)");
		goto exit_objects;
	}

	for (i = 0; !error && (i < count); i++) {
		rv = pkcs_C_FindObjectsInit(hSession, sTemplate, 1);
		if (rv != CKR_OK) {
			fprintf(stderr,
				"C_FindObjectsInit[%u]: Error = 0x%.8lX\n",
				i, rv);
			error = 1;
			break;
		}

		rv = pkcs_C_FindObjects(hSession, &sKey, 1, &found);
		if (rv != CKR_OK) {
			fprintf(stderr,
				"C_FindObjects[%u]: Error = 0x%.8lX\n",
				i, rv);
			error = 1;
			/* no break here! */
		}

		rv = pkcs_C_FindObjectsFinal(hSession);
		if (rv != CKR_OK) {
			fprintf(stderr,
				"C_FindObjectsFinal[%u]: Error = 0x%.8lX\n",
				i, rv);
			error = 1;
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
	printf("%u object searches in %ld.%09lds\n", i,
	       endtime.tv_sec, endtime.tv_nsec);
	if (i > 0)
		printf("%g object searches/s\n",
		       1024 * i / ((double) endtime.tv_sec +
				   (double) endtime.tv_nsec / 1000000000.));

    exit_objects:
	pk11_return_session(&pctx);
	(void) pk11_finalize();

	exit(error);
}

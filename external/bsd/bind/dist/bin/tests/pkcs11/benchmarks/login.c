/*	$NetBSD: login.c,v 1.1.1.3 2014/12/10 03:34:28 christos Exp $	*/

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

/* login [-m module] [-s $slot] [-p pin] [-n count] */

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
#include <pk11/internal.h>

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

int
main(int argc, char *argv[]) {
	CK_RV rv;
	CK_SLOT_ID slot = 0;
	CK_SESSION_HANDLE *hSession;
	CK_UTF8CHAR *pin = NULL;
	char *lib_name = NULL;
	int error = 0;
	int c, errflg = 0;
	unsigned int count = 1000;
	unsigned int i, j;
	struct timespec starttime;
	struct timespec endtime;

	while ((c = isc_commandline_parse(argc, argv, ":m:s:p:n:")) != -1) {
		switch (c) {
		case 'm':
			lib_name = isc_commandline_argument;
			break;
		case 's':
			slot = atoi(isc_commandline_argument);
			break;
		case 'p':
			pin = (CK_UTF8CHAR *)isc_commandline_argument;
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
			"\tlogin [-m module] [-s slot] [-p pin] [-n count]\n");
		exit(1);
	}

	/* allocate sessions */
	hSession = (CK_SESSION_HANDLE *)
		malloc(count * sizeof(CK_SESSION_HANDLE));
	if (hSession == NULL) {
		perror("malloc");
		exit(1);
	}
	for (i = 0; i < count; i++)
		hSession[i] = CK_INVALID_HANDLE;

	/* Initialize the CRYPTOKI library */
	if (lib_name != NULL)
		pk11_set_lib_name(lib_name);

	if (pin == NULL)
		pin = (CK_UTF8CHAR *)getpassphrase("Enter Pin: ");

	rv = pkcs_C_Initialize(NULL_PTR);
	if (rv != CKR_OK) {
		if (rv == 0xfe)
			fprintf(stderr,
				"Can't load or link module \"%s\"\n",
				pk11_get_lib_name());
		else
			fprintf(stderr, "C_Initialize: Error = 0x%.8lX\n", rv);
		free(hSession);
		exit(1);
	}

	if (clock_gettime(CLOCK_REALTIME, &starttime) < 0) {
		perror("clock_gettime(start)");
		goto exit_program;
	}

	/* loop */
	for (i = 0; i < count; i++) {
		/* Open sessions */
		rv = pkcs_C_OpenSession(slot, CKF_SERIAL_SESSION,
					NULL_PTR, NULL_PTR, &hSession[i]);
		if (rv != CKR_OK) {
			fprintf(stderr,
				"C_OpenSession[%u]: Error = 0x%.8lX\n",
				i, rv);
			error = 1;
			if (i == 0)
				goto exit_program;
			break;
		}

		/* Logon */
		rv = pkcs_C_Login(hSession[i], CKU_USER,
				  pin, strlen((char *)pin));
		if (rv != CKR_OK) {
			fprintf(stderr,
				"C_Login[%u]: Error = 0x%.8lX\n",
				i, rv);
			error = 1;
			if (i == 0)
				goto exit_program;
			break;
		}

		/* Logoff */
		rv = pkcs_C_Logout(hSession[i]);
		if (rv != CKR_OK) {
			fprintf(stderr,
				"C_Logout[%u]: Error = 0x%.8lX\n",
				i, rv);
			error = 1;
			if (i == 0)
				goto exit_program;
			break;
		}
	}

	if (clock_gettime(CLOCK_REALTIME, &endtime) < 0) {
		perror("clock_gettime(end)");
		goto exit_program;
	}

	endtime.tv_sec -= starttime.tv_sec;
	endtime.tv_nsec -= starttime.tv_nsec;
	while (endtime.tv_nsec < 0) {
		endtime.tv_sec -= 1;
		endtime.tv_nsec += 1000000000;
	}
	printf("%u logins in %ld.%09lds\n", i,
	       endtime.tv_sec, endtime.tv_nsec);
	if (i > 0)
		printf("%g logins/s\n",
		       i / ((double) endtime.tv_sec +
			    (double) endtime.tv_nsec / 1000000000.));

	for (j = 0; j < i; j++) {
		if (hSession[j] == CK_INVALID_HANDLE)
			continue;
		/* Close sessions */
		rv = pkcs_C_CloseSession(hSession[j]);
		if ((rv != CKR_OK) && !errflg) {
			fprintf(stderr,
				"C_CloseSession[%u]: Error = 0x%.8lX\n",
				j, rv);
			errflg = 1;
		}
	}

    exit_program:
	free(hSession);

	rv = pkcs_C_Finalize(NULL_PTR);
	if (rv != CKR_OK)
		fprintf(stderr, "C_Finalize: Error = 0x%.8lX\n", rv);

	exit(error);
}

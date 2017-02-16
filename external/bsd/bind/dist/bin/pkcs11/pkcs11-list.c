/*	$NetBSD: pkcs11-list.c,v 1.7 2014/12/10 04:37:52 christos Exp $	*/

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

/* Id: pkcs11-list.c,v 1.7 2009/10/26 23:36:53 each Exp  */

/* pkcs11-list [-P] [-m module] [-s slot] [-i $id | -l $label] [-p $pin] */

/*! \file */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#include <isc/commandline.h>
#include <isc/result.h>
#include <isc/types.h>

#include <pk11/pk11.h>
#include <pk11/result.h>

#if !(defined(HAVE_GETPASSPHRASE) || (defined (__SVR4) && defined (__sun)))
#define getpassphrase(x)		getpass(x)
#endif

int
main(int argc, char *argv[]) {
	isc_result_t result;
	CK_RV rv;
	CK_SLOT_ID slot = 0;
	CK_SESSION_HANDLE hSession;
	CK_BYTE attr_id[2];
	CK_OBJECT_HANDLE akey[50];
	pk11_context_t pctx;
	char *lib_name = NULL;
	char *label = NULL;
	char *pin = NULL;
	isc_boolean_t error = ISC_FALSE, logon = ISC_TRUE, all = ISC_FALSE;
	unsigned int i = 0, id = 0;
	int c, errflg = 0;
	CK_ULONG ulObjectCount;
	CK_ATTRIBUTE search_template[] = {
		{CKA_ID, &attr_id, sizeof(attr_id)}
	};

	while ((c = isc_commandline_parse(argc, argv, ":m:s:i:l:p:P")) != -1) {
		switch (c) {
		case 'P':
			logon = ISC_FALSE;
			break;
		case 'm':
			lib_name = isc_commandline_argument;
			break;
		case 's':
			slot = atoi(isc_commandline_argument);
			break;
		case 'i':
			id = atoi(isc_commandline_argument);
			id &= 0xffff;
			break;
		case 'l':
			label = isc_commandline_argument;
			break;
		case 'p':
			pin = isc_commandline_argument;
			break;
		case ':':
			fprintf(stderr, "Option -%c requires an operand\n",
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
		fprintf(stderr, "\tpkcs11-list [-P] [-m module] [-s slot] "
				"[-i id | -l label] [-p pin]\n");
		exit(1);
	}

	if (!id && (label == NULL))
		all = ISC_TRUE;

	if (slot)
		printf("slot %lu\n", slot);

	if (id) {
		printf("id %i\n", id);
		attr_id[0] = (id >> 8) & 0xff;
		attr_id[1] = id & 0xff;
	} else if (label != NULL) {
		printf("label %s\n", label);
		search_template[0].type = CKA_LABEL;
		search_template[0].pValue = label;
		search_template[0].ulValueLen = strlen(label);
	}

	pk11_result_register();

	/* Initialize the CRYPTOKI library */
	if (lib_name != NULL)
		pk11_set_lib_name(lib_name);

	if (logon && pin == NULL)
		pin = getpassphrase("Enter Pin: ");

	result = pk11_get_session(&pctx, OP_ANY, ISC_FALSE, ISC_FALSE,
				  logon, pin, slot);
	if (result == PK11_R_NORANDOMSERVICE ||
	    result == PK11_R_NODIGESTSERVICE ||
	    result == PK11_R_NOAESSERVICE) {
		fprintf(stderr, "Warning: %s\n", isc_result_totext(result));
		fprintf(stderr, "This HSM will not work with BIND 9 "
				"using native PKCS#11.\n");
	} else if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "Unrecoverable error initializing "
			"PKCS#11: %s\n", isc_result_totext(result));
		fprintf(stderr, "Unrecoverable error initializing "
				"PKCS#11: %s\n", isc_result_totext(result));
		exit(1);
	}

	if (pin != NULL)
		memset(pin, 0, strlen(pin));

	hSession = pctx.session;

	rv = pkcs_C_FindObjectsInit(hSession, search_template, all ? 0 : 1); 
	if (rv != CKR_OK) {
		fprintf(stderr, "C_FindObjectsInit: Error = 0x%.8lX\n", rv);
		error = 1;
		goto exit_session;
	}
	
	ulObjectCount = 1;
	while (ulObjectCount) {
		rv = pkcs_C_FindObjects(hSession, akey, 50, &ulObjectCount);
		if (rv != CKR_OK) {
			fprintf(stderr,
				"C_FindObjects: Error = 0x%.8lX\n",
				rv);
			error = 1;
			goto exit_search;
		}
		for (i = 0; i < ulObjectCount; i++) {
			unsigned int j, len;

			CK_OBJECT_CLASS oclass = 0;
			CK_BYTE labelbuf[64 + 1];
			CK_BYTE idbuf[64];
			CK_ATTRIBUTE template[] = {
				{CKA_CLASS, &oclass, sizeof(oclass)},
				{CKA_LABEL, labelbuf, sizeof(labelbuf) - 1},
				{CKA_ID, idbuf, sizeof(idbuf)}
			};

			memset(labelbuf, 0, sizeof(labelbuf));
			memset(idbuf, 0, sizeof(idbuf));

			rv = pkcs_C_GetAttributeValue(hSession, akey[i],
						 template, 3);
			if (rv != CKR_OK) {
				fprintf(stderr,
					"C_GetAttributeValue[%u]: "
					"rv = 0x%.8lX\n",
					i, rv);
				if (rv == CKR_BUFFER_TOO_SMALL)
					fprintf(stderr,
						"%u too small: %lu %lu %lu\n",
						i,
						template[0].ulValueLen,
						template[1].ulValueLen,
						template[2].ulValueLen);
				error = 1;
				continue;
			}

			len = template[2].ulValueLen;
			printf("object[%u]: handle %lu class %lu "
			       "label[%lu] '%s' id[%lu] ",
			       i, akey[i], oclass,
			       template[1].ulValueLen,
			       labelbuf,
			       template[2].ulValueLen);
			if (len == 2) {
				id = (idbuf[0] << 8) & 0xff00;
				id |= idbuf[1] & 0xff;
				printf("%u\n", id);
			} else {
				if (len > 8)
					len = 8;
				if (len > 0)
					printf("0x");
				for (j = 0; j < len; j++)
					printf("%02x", idbuf[j]);
				if (template[2].ulValueLen > len)
					printf("...\n");
				else
					printf("\n");
			}
		}
	}

 exit_search:
	rv = pkcs_C_FindObjectsFinal(hSession);
	if (rv != CKR_OK) {
		fprintf(stderr, "C_FindObjectsFinal: Error = 0x%.8lX\n", rv);
		error = 1;
	}

 exit_session:
	pk11_return_session(&pctx);
	(void) pk11_finalize();

	exit(error);
}

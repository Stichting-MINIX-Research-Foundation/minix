/*	$NetBSD: pkcs11-keygen.c,v 1.7 2014/12/10 04:37:52 christos Exp $	*/

/*
 * Copyright (C) 2009,2012 Internet Systems Consortium, Inc. ("ISC")
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

/* pkcs11-keygen - PKCS#11 key generator
 *
 * Create a key in the keystore of an HSM
 *
 * The calculation of key tag is left to the script
 * that converts the key into a DNSKEY RR and inserts 
 * it into a zone file.
 *
 * usage:
 * pkcs11-keygen [-P] [-m module] [-s slot] [-e] [-b keysize]
 *               [-i id] [-p pin] -l label
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

#include <isc/commandline.h>
#include <isc/result.h>
#include <isc/types.h>

#include <pk11/pk11.h>
#include <pk11/result.h>
#define WANT_DH_PRIMES
#define WANT_ECC_CURVES
#include <pk11/constants.h>

#if !(defined(HAVE_GETPASSPHRASE) || (defined (__SVR4) && defined (__sun)))
#define getpassphrase(x)	getpass(x)
#endif

/* Define static key template values */
static CK_BBOOL truevalue = TRUE;
static CK_BBOOL falsevalue = FALSE;

/* Key class: RSA, ECC, DSA, DH, or unknown */
typedef enum {
	key_unknown,
	key_rsa,
	key_dsa,
	key_dh,
	key_ecc
} key_class_t;

/*
 * Private key template: usable for most key classes without
 * modificaton; override CKA_SIGN with CKA_DERIVE for DH
 */
#define PRIVATE_LABEL 0
#define PRIVATE_SIGN 1
#define PRIVATE_DERIVE 1
#define PRIVATE_TOKEN 2
#define PRIVATE_PRIVATE 3
#define PRIVATE_SENSITIVE 4
#define PRIVATE_EXTRACTABLE 5
#define PRIVATE_ID 6
#define PRIVATE_ATTRS 7
static CK_ATTRIBUTE private_template[] = {
	{CKA_LABEL, NULL_PTR, 0},
	{CKA_SIGN, &truevalue, sizeof(truevalue)},
	{CKA_TOKEN, &truevalue, sizeof(truevalue)},
	{CKA_PRIVATE, &truevalue, sizeof(truevalue)},
	{CKA_SENSITIVE, &truevalue, sizeof(truevalue)},
	{CKA_EXTRACTABLE, &falsevalue, sizeof(falsevalue)},
	{CKA_ID, NULL_PTR, 0}
};

/*
 * Public key template for RSA keys
 */
#define RSA_LABEL 0
#define RSA_VERIFY 1
#define RSA_TOKEN 2
#define RSA_PRIVATE 3
#define RSA_MODULUS_BITS 4
#define RSA_PUBLIC_EXPONENT 5
#define RSA_ID 6
#define RSA_ATTRS 7
static CK_ATTRIBUTE rsa_template[] = {
	{CKA_LABEL, NULL_PTR, 0},
	{CKA_VERIFY, &truevalue, sizeof(truevalue)},
	{CKA_TOKEN, &truevalue, sizeof(truevalue)},
	{CKA_PRIVATE, &falsevalue, sizeof(falsevalue)},
	{CKA_MODULUS_BITS, NULL_PTR, 0},
	{CKA_PUBLIC_EXPONENT, NULL_PTR, 0},
	{CKA_ID, NULL_PTR, 0}
};

/*
 * Public key template for ECC keys
 */
#define ECC_LABEL 0
#define ECC_VERIFY 1
#define ECC_TOKEN 2
#define ECC_PRIVATE 3
#define ECC_PARAMS 4
#define ECC_ID 5
#define ECC_ATTRS 6
static CK_ATTRIBUTE ecc_template[] = {
	{CKA_LABEL, NULL_PTR, 0},
	{CKA_VERIFY, &truevalue, sizeof(truevalue)},
	{CKA_TOKEN, &truevalue, sizeof(truevalue)},
	{CKA_PRIVATE, &falsevalue, sizeof(falsevalue)},
	{CKA_EC_PARAMS, NULL_PTR, 0},
	{CKA_ID, NULL_PTR, 0}
};

/*
 * Public key template for DSA keys
 */
#define DSA_LABEL 0
#define DSA_VERIFY 1
#define DSA_TOKEN 2
#define DSA_PRIVATE 3
#define DSA_PRIME 4
#define DSA_SUBPRIME 5
#define DSA_BASE 6
#define DSA_ID 7
#define DSA_ATTRS 8
static CK_ATTRIBUTE dsa_template[] = {
	{CKA_LABEL, NULL_PTR, 0},
	{CKA_VERIFY, &truevalue, sizeof(truevalue)},
	{CKA_TOKEN, &truevalue, sizeof(truevalue)},
	{CKA_PRIVATE, &falsevalue, sizeof(falsevalue)},
	{CKA_PRIME, NULL_PTR, 0},
	{CKA_SUBPRIME, NULL_PTR, 0},
	{CKA_BASE, NULL_PTR, 0},
	{CKA_ID, NULL_PTR, 0}
};
#define DSA_PARAM_PRIME 0
#define DSA_PARAM_SUBPRIME 1
#define DSA_PARAM_BASE 2
#define DSA_PARAM_ATTRS 3
static CK_ATTRIBUTE dsa_param_template[] = {
	{CKA_PRIME, NULL_PTR, 0},
	{CKA_SUBPRIME, NULL_PTR, 0},
	{CKA_BASE, NULL_PTR, 0},
};
#define DSA_DOMAIN_PRIMEBITS 0
#define DSA_DOMAIN_PRIVATE 1
#define DSA_DOMAIN_ATTRS 2
static CK_ATTRIBUTE dsa_domain_template[] = {
	{CKA_PRIME_BITS, NULL_PTR, 0},
	{CKA_PRIVATE, &falsevalue, sizeof(falsevalue)},
};

/*
 * Public key template for DH keys
 */
#define DH_LABEL 0
#define DH_VERIFY 1
#define DH_TOKEN 2
#define DH_PRIVATE 3
#define DH_PRIME 4
#define DH_BASE 5
#define DH_ID 6
#define DH_ATTRS 7
static CK_ATTRIBUTE dh_template[] = {
	{CKA_LABEL, NULL_PTR, 0},
	{CKA_VERIFY, &truevalue, sizeof(truevalue)},
	{CKA_TOKEN, &truevalue, sizeof(truevalue)},
	{CKA_PRIVATE, &falsevalue, sizeof(falsevalue)},
	{CKA_PRIME, NULL_PTR, 0},
	{CKA_BASE, NULL_PTR, 0},
	{CKA_ID, NULL_PTR, 0}
};
#define DH_PARAM_PRIME 0
#define DH_PARAM_BASE 1
#define DH_PARAM_ATTRS 2
static CK_ATTRIBUTE dh_param_template[] = {
	{CKA_PRIME, NULL_PTR, 0},
	{CKA_BASE, NULL_PTR, 0},
};
#define DH_DOMAIN_PRIMEBITS 0
#define DH_DOMAIN_ATTRS 1
static CK_ATTRIBUTE dh_domain_template[] = {
	{CKA_PRIME_BITS, NULL_PTR, 0},
};

/*
 * Convert from text to key class.  Accepts the names of DNSSEC
 * signing algorithms, so e.g., ECDSAP256SHA256 maps to ECC and
 * NSEC3RSASHA1 maps to RSA.
 */
static key_class_t
keyclass_fromtext(const char *name) {
	if (name == NULL)
		return (key_unknown);

	if (strncasecmp(name, "rsa", 3) == 0 ||
	    strncasecmp(name, "nsec3rsa", 8) == 0)
		return (key_rsa);
	else if (strncasecmp(name, "dsa", 3) == 0 ||
		 strncasecmp(name, "nsec3dsa", 8) == 0)
		return (key_dsa);
	else if (strcasecmp(name, "dh") == 0)
		return (key_dh);
	else if (strncasecmp(name, "ecc", 3) == 0 ||
		 strncasecmp(name, "ecdsa", 5) == 0)
		return (key_ecc);
	else
		return (key_unknown);
}

static void
usage(void) {
	fprintf(stderr,
		"Usage:\n"
		"\tpkcs11-keygen -a algorithm -b keysize -l label\n"
		"\t              [-P] [-m module] "
			"[-s slot] [-e] [-S] [-i id] [-p PIN]\n");
	exit(2);
}

int
main(int argc, char *argv[]) {
	isc_result_t result;
	CK_RV rv;
	CK_SLOT_ID slot = 0;
	CK_MECHANISM mech, dpmech;
	CK_SESSION_HANDLE hSession;
	char *lib_name = NULL;
	char *pin = NULL;
	CK_ULONG bits = 0;
	CK_CHAR *label = NULL;
	CK_OBJECT_HANDLE privatekey, publickey, domainparams;
	CK_BYTE exponent[5];
	CK_ULONG expsize = 0;
	pk11_context_t pctx;
	int error = 0;
	int c, errflg = 0;
	int hide = 1, special = 0, quiet = 0;
	int idlen = 0, id_offset = 0;
	unsigned int i;
	unsigned long id = 0;
	CK_BYTE idbuf[4];
	CK_ULONG ulObjectCount;
	CK_ATTRIBUTE search_template[] = {
		{CKA_LABEL, NULL_PTR, 0}
	};
	CK_ATTRIBUTE *public_template = NULL;
	CK_ATTRIBUTE *domain_template = NULL;
	CK_ATTRIBUTE *param_template = NULL;
	CK_ULONG public_attrcnt = 0, private_attrcnt = PRIVATE_ATTRS;
	CK_ULONG domain_attrcnt = 0, param_attrcnt = 0;
	key_class_t keyclass = key_rsa;
	pk11_optype_t op_type = OP_ANY;

#define OPTIONS ":a:b:ei:l:m:Pp:qSs:"
	while ((c = isc_commandline_parse(argc, argv, OPTIONS)) != -1) {
		switch (c) {
		case 'a':
			keyclass = keyclass_fromtext(isc_commandline_argument);
			break;
		case 'P':
			hide = 0;
			break;
		case 'm':
			lib_name = isc_commandline_argument;
			break;
		case 's':
			slot = atoi(isc_commandline_argument);
			break;
		case 'e':
			expsize = 5;
			break;
		case 'b':
			bits = atoi(isc_commandline_argument);
			break;
		case 'l':
			/* -l option is retained for backward compatibility * */
			label = (CK_CHAR *)isc_commandline_argument;
			break;
		case 'i':
			id = strtoul(isc_commandline_argument, NULL, 0);
			idlen = 4;
			break;
		case 'p':
			pin = isc_commandline_argument;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'S':
			special = 1;
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

	if (label == NULL && isc_commandline_index < argc)
		label = (CK_CHAR *)argv[isc_commandline_index];

	if (errflg || (label == NULL))
		usage();

	if (expsize != 0 && keyclass != key_rsa) {
		fprintf(stderr, "The -e option is only compatible "
				"with RSA key generation\n");
		exit(2);
	}

	if (special != 0 && keyclass != key_dh) {
		fprintf(stderr, "The -S option is only compatible "
				"with Diffie-Hellman key generation\n");
		exit(2);
	}

	switch (keyclass) {
	case key_rsa:
		op_type = OP_RSA;
		if (expsize == 0)
			expsize = 3;
		if (bits == 0)
			usage();

		mech.mechanism = CKM_RSA_PKCS_KEY_PAIR_GEN;
		mech.pParameter = NULL;
		mech.ulParameterLen = 0;

		public_template = rsa_template;
		public_attrcnt = RSA_ATTRS;
		id_offset = RSA_ID;

		/* Set public exponent to F4 or F5 */
		exponent[0] = 0x01;
		exponent[1] = 0x00;
		if (expsize == 3)
			exponent[2] = 0x01;
		else {
			exponent[2] = 0x00;
			exponent[3] = 0x00;
			exponent[4] = 0x01;
		}

		public_template[RSA_MODULUS_BITS].pValue = &bits;
		public_template[RSA_MODULUS_BITS].ulValueLen = sizeof(bits);
		public_template[RSA_PUBLIC_EXPONENT].pValue = &exponent;
		public_template[RSA_PUBLIC_EXPONENT].ulValueLen = expsize;
		break;
	case key_ecc:
		op_type = OP_EC;
		if (bits == 0)
			bits = 256;
		else if (bits != 256 && bits != 384) {
			fprintf(stderr, "ECC keys only support bit sizes of "
					"256 and 384\n");
			exit(2);
		}

		mech.mechanism = CKM_EC_KEY_PAIR_GEN;
		mech.pParameter = NULL;
		mech.ulParameterLen = 0;

		public_template = ecc_template;
		public_attrcnt = ECC_ATTRS;
		id_offset = ECC_ID;

		if (bits == 256) {
			public_template[4].pValue = pk11_ecc_prime256v1;
			public_template[4].ulValueLen =
				sizeof(pk11_ecc_prime256v1);
		} else {
			public_template[4].pValue = pk11_ecc_secp384r1;
			public_template[4].ulValueLen =
				sizeof(pk11_ecc_secp384r1);
		}

		break;
	case key_dsa:
		op_type = OP_DSA;
		if (bits == 0)
			usage();

		dpmech.mechanism = CKM_DSA_PARAMETER_GEN;
		dpmech.pParameter = NULL;
		dpmech.ulParameterLen = 0;
		mech.mechanism = CKM_DSA_KEY_PAIR_GEN;
		mech.pParameter = NULL;
		mech.ulParameterLen = 0;

		public_template = dsa_template;
		public_attrcnt = DSA_ATTRS;
		id_offset = DSA_ID;

		domain_template = dsa_domain_template;
		domain_attrcnt = DSA_DOMAIN_ATTRS;
		param_template = dsa_param_template;
		param_attrcnt = DSA_PARAM_ATTRS;

		domain_template[DSA_DOMAIN_PRIMEBITS].pValue = &bits;
		domain_template[DSA_DOMAIN_PRIMEBITS].ulValueLen = sizeof(bits);
		break;
	case key_dh:
		op_type = OP_DH;
		if (special && bits == 0)
			bits = 1024;
		else if (special &&
			 bits != 768 && bits != 1024 && bits != 1536)
		{
			fprintf(stderr, "When using the special prime (-S) "
				"option, only key sizes of\n"
				"768, 1024 or 1536 are supported.\n");
			exit(2);
		} else if (bits == 0)
			usage();

		dpmech.mechanism = CKM_DH_PKCS_PARAMETER_GEN;
		dpmech.pParameter = NULL;
		dpmech.ulParameterLen = 0;
		mech.mechanism = CKM_DH_PKCS_KEY_PAIR_GEN;
		mech.pParameter = NULL;
		mech.ulParameterLen = 0;

		/* Override CKA_SIGN attribute */
		private_template[PRIVATE_DERIVE].type = CKA_DERIVE;

		public_template = dh_template;
		public_attrcnt = DH_ATTRS;
		id_offset = DH_ID;

		domain_template = dh_domain_template;
		domain_attrcnt = DH_DOMAIN_ATTRS;
		param_template = dh_param_template;
		param_attrcnt = DH_PARAM_ATTRS;

		domain_template[DH_DOMAIN_PRIMEBITS].pValue = &bits;
		domain_template[DH_DOMAIN_PRIMEBITS].ulValueLen = sizeof(bits);
		break;
	case key_unknown:
		usage();
	}
	
	search_template[0].pValue = label;
	search_template[0].ulValueLen = strlen((char *)label);
	public_template[0].pValue = label;
	public_template[0].ulValueLen = strlen((char *)label);
	private_template[0].pValue = label;
	private_template[0].ulValueLen = strlen((char *)label);

	if (idlen == 0) {
		public_attrcnt--;
		private_attrcnt--;
	} else {
		if (id <= 0xffff) {
			idlen = 2;
			idbuf[0] = (CK_BYTE)(id >> 8);
			idbuf[1] = (CK_BYTE)id;
		} else {
			idbuf[0] = (CK_BYTE)(id >> 24);
			idbuf[1] = (CK_BYTE)(id >> 16);
			idbuf[2] = (CK_BYTE)(id >> 8);
			idbuf[3] = (CK_BYTE)id;
		}

		public_template[id_offset].pValue = idbuf;
		public_template[id_offset].ulValueLen = idlen;
		private_template[PRIVATE_ID].pValue = idbuf;
		private_template[PRIVATE_ID].ulValueLen = idlen;
	}

	pk11_result_register();

	/* Initialize the CRYPTOKI library */
	if (lib_name != NULL)
		pk11_set_lib_name(lib_name);

	if (pin == NULL)
		pin = getpassphrase("Enter Pin: ");

	result = pk11_get_session(&pctx, op_type, ISC_FALSE, ISC_TRUE,
				  ISC_TRUE, (const char *) pin, slot);
	if (result == PK11_R_NORANDOMSERVICE ||
	    result == PK11_R_NODIGESTSERVICE ||
	    result == PK11_R_NOAESSERVICE) {
		fprintf(stderr, "Warning: %s\n", isc_result_totext(result));
		fprintf(stderr, "This HSM will not work with BIND 9 "
				"using native PKCS#11.\n");
	} else if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "Unrecoverable error initializing "
				"PKCS#11: %s\n", isc_result_totext(result));
		exit(1);
	}

	memset(pin, 0, strlen(pin));

	hSession = pctx.session;

	/* check if a key with the same id already exists */
	rv = pkcs_C_FindObjectsInit(hSession, search_template, 1); 
	if (rv != CKR_OK) {
		fprintf(stderr, "C_FindObjectsInit: Error = 0x%.8lX\n", rv);
		error = 1;
		goto exit_session;
	}
	rv = pkcs_C_FindObjects(hSession, &privatekey, 1, &ulObjectCount);
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
		private_template[4].pValue = &falsevalue;
		private_template[5].pValue = &truevalue;
	}

	if (keyclass == key_rsa || keyclass == key_ecc)
		goto generate_keys;

	/*
	 * Special setup for Diffie-Hellman keys
	 */
	if (special != 0) {
		public_template[DH_BASE].pValue = pk11_dh_bn2;
		public_template[DH_BASE].ulValueLen = sizeof(pk11_dh_bn2);
		if (bits == 768) {
			public_template[DH_PRIME].pValue = pk11_dh_bn768;
			public_template[DH_PRIME].ulValueLen =
				sizeof(pk11_dh_bn768);
		} else if (bits == 1024) {
			public_template[DH_PRIME].pValue = pk11_dh_bn1024;
			public_template[DH_PRIME].ulValueLen =
				sizeof(pk11_dh_bn1024);
		} else {
			public_template[DH_PRIME].pValue = pk11_dh_bn1536;
			public_template[DH_PRIME].ulValueLen =
				sizeof(pk11_dh_bn1536);
		}
		param_attrcnt = 0;
		goto generate_keys;
	}

	/* Generate Domain parameters */
	rv = pkcs_C_GenerateKey(hSession, &dpmech, domain_template,
			   domain_attrcnt, &domainparams);

	if (rv != CKR_OK) {
		fprintf(stderr,
			"C_GenerateKey: Error = 0x%.8lX\n", rv);
		error = 1;
		goto exit_search;
	}

	/* Get Domain parameters */
	rv = pkcs_C_GetAttributeValue(hSession, domainparams,
				 param_template, param_attrcnt);

	if (rv != CKR_OK) {
		fprintf(stderr,
			"C_GetAttributeValue0: Error = 0x%.8lX\n", rv);
		error = 1;
		goto exit_domain;
	}

	/* Allocate space for parameter attributes */
	for (i = 0; i < param_attrcnt; i++)
		param_template[i].pValue = malloc(param_template[i].ulValueLen);

	rv = pkcs_C_GetAttributeValue(hSession, domainparams,
				 dsa_param_template, DSA_PARAM_ATTRS);

	if (rv != CKR_OK) {
		fprintf(stderr,
			"C_GetAttributeValue1: Error = 0x%.8lX\n", rv);
		error = 1;
		goto exit_params;
	}

	switch (keyclass) {
	case key_dsa:
		public_template[DSA_PRIME].pValue =
			param_template[DSA_PARAM_PRIME].pValue;
		public_template[DSA_PRIME].ulValueLen =
			param_template[DSA_PARAM_PRIME].ulValueLen;
		public_template[DSA_SUBPRIME].pValue =
			param_template[DSA_PARAM_SUBPRIME].pValue;
		public_template[DSA_SUBPRIME].ulValueLen =
			param_template[DSA_PARAM_SUBPRIME].ulValueLen;
		public_template[DSA_BASE].pValue =
			param_template[DSA_PARAM_BASE].pValue;
		public_template[DSA_BASE].ulValueLen =
			param_template[DSA_PARAM_BASE].ulValueLen;
		break;
	case key_dh:
		public_template[DH_PRIME].pValue =
			param_template[DH_PARAM_PRIME].pValue;
		public_template[DH_PRIME].ulValueLen =
			param_template[DH_PARAM_PRIME].ulValueLen;
		public_template[DH_BASE].pValue =
			param_template[DH_PARAM_BASE].pValue;
		public_template[DH_BASE].ulValueLen =
			param_template[DH_PARAM_BASE].ulValueLen;
	default:
		break;
	}

 generate_keys:
	/* Generate Key pair for signing/verifying */
	rv = pkcs_C_GenerateKeyPair(hSession, &mech,
			       public_template, public_attrcnt,
			       private_template, private_attrcnt,
			       &publickey, &privatekey);
	
	if (rv != CKR_OK) {
		fprintf(stderr, "C_GenerateKeyPair: Error = 0x%.8lX\n", rv);
		error = 1;
	 } else if (!quiet)
		printf("Key pair generation complete.\n");
	
 exit_params:
	/* Free parameter attributes */
	if (keyclass == key_dsa || keyclass == key_dh)
		for (i = 0; i < param_attrcnt; i++)
			free(param_template[i].pValue);

 exit_domain:
	/* Destroy domain parameters */
	if (keyclass == key_dsa || (keyclass == key_dh && !special)) {
		rv = pkcs_C_DestroyObject(hSession, domainparams);
		if (rv != CKR_OK) {
			fprintf(stderr,
				"C_DestroyObject: Error = 0x%.8lX\n", rv);
			error = 1;
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

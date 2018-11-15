/*	$NetBSD: test_canon.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 2011, Secure Endpoints Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "krb5_locl.h"
#include <err.h>
#include <krb5/getarg.h>

#if 0
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <krb5/krb5.h>
#endif

int
main(int argc, char **argv)
{
    krb5_error_code retval;
    krb5_context context;
    krb5_principal princ = NULL;
    krb5_principal me = NULL;
    krb5_principal cmp_to_princ = NULL;
    krb5_ccache cc = NULL;
    krb5_creds *out_creds = NULL;
    krb5_keytab kt = NULL;
    krb5_keytab_entry ktent;
    krb5_creds in_creds;
    char *hostname = NULL;
    char *unparsed = NULL;
    char *unparsed_canon = NULL;
    char *during;
    char *cmp_to = NULL;;
    int do_kt = 0;
    int do_get_creds = 0;
    int opt;
    int ret = 1;

    memset(&ktent, 0, sizeof(ktent));

    while ((opt = getopt(argc, argv, "hgkc:")) != -1) {
	switch (opt) {
	case 'g':
	    do_get_creds++;
	    break;
	case 'k':
	    do_kt++;
	    break;
	case 'c':
	    cmp_to = optarg;
	    break;
	case 'h':
	default:
	    fprintf(stderr, "Usage: %s [-g] [-k] [-c compare-to-principal] "
		    "[principal]\n", argv[0]);
	    return 1;
	}
    }

    if (!do_get_creds && !do_kt && !cmp_to)
	do_get_creds++;

    if (optind < argc)
	hostname = argv[optind];

    during = "init_context";
    retval = krb5_init_context(&context);
    if (retval) goto err;

    during = "sn2p";
    retval = krb5_sname_to_principal(context, hostname, "host", KRB5_NT_SRV_HST, &princ);
    if (retval) goto err;

    during = "unparse of sname2princ";
    retval = krb5_unparse_name(context, princ, &unparsed);
    if (retval) goto err;
    printf("krb5_sname_to_principal() output: %s\n", unparsed);

    if (cmp_to) {
	krb5_boolean eq;

	during = "parsing principal name for comparison compare";
	retval = krb5_parse_name(context, cmp_to, &cmp_to_princ);
	if (retval) goto err;

	eq = krb5_principal_compare(context, princ, cmp_to_princ);
	printf("%s %s %s\n", unparsed, eq ? "==" : "!=", cmp_to);
    }

    if (do_get_creds) {
	during = "ccdefault";
	retval = krb5_cc_default(context, &cc);
	if (retval) goto err;

	during = "ccprinc";
	retval = krb5_cc_get_principal(context, cc, &me);
	if (retval) goto err;

	memset(&in_creds, 0, sizeof(in_creds));
	in_creds.client = me;
	in_creds.server = princ;

	during = "getcreds";
	retval = krb5_get_credentials(context, 0, cc, &in_creds, &out_creds);
	if (retval) goto err;

	during = "unparsing principal name canonicalized by krb5_get_credentials()";
	retval = krb5_unparse_name(context, in_creds.server, &unparsed_canon);
	if (retval) goto err;
	printf("Principal name as canonicalized by krb5_get_credentials() is %s\n", unparsed_canon);
    }

    if (do_kt) {
	during = "getting keytab";
	retval = krb5_kt_default(context, &kt);
	if (retval) goto err;

	during = "getting keytab ktent";
	retval = krb5_kt_get_entry(context, kt, princ, 0, 0, &ktent);
	if (retval) goto err;

	during = "unparsing principal name canonicalized by krb5_kt_get_entry()";
	retval = krb5_unparse_name(context, ktent.principal, &unparsed_canon);
	if (retval) goto err;
	printf("Principal name as canonicalized by krb5_kt_get_entry() is %s\n", unparsed_canon);
    }

    ret = 0;

err:
    krb5_free_principal(context, princ);
    krb5_free_principal(context, me);
    krb5_free_principal(context, cmp_to_princ);
    krb5_xfree(unparsed);
    krb5_xfree(unparsed_canon);
    if (do_get_creds) {
	krb5_free_creds(context, out_creds);
	(void) krb5_cc_close(context, cc);
    }
    krb5_kt_free_entry(context, &ktent);
    if (kt)
	krb5_kt_close(context, kt);
    krb5_free_context(context);
    if (ret)
	fprintf(stderr, "Failed while doing %s (%d)\n", during, retval);
    return (ret);
}


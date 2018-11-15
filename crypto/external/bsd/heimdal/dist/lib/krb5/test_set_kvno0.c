/*	$NetBSD: test_set_kvno0.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

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
    krb5_error_code ret;
    krb5_context context;
    krb5_ccache src_cc = NULL;
    krb5_ccache dst_cc = NULL;
    krb5_cc_cursor cursor;
    krb5_principal me = NULL;
    krb5_creds cred;
    const char *during;
    Ticket t;
    size_t len;
    int make_kvno_absent = 0;
    int opt;

    memset(&cred, 0, sizeof (cred));
    during = "init_context";
    ret = krb5_init_context(&context);
    if (ret) goto err;

    while ((opt = getopt(argc, argv, "c:n")) != -1) {
	switch (opt) {
	case 'c':
	    during = "cc_resolve of source ccache";
	    ret = krb5_cc_resolve(context, optarg, &src_cc);
	    if (ret) goto err;
	    break;
	case 'n':
	    make_kvno_absent++;
	    break;
	case 'h':
	default:
	    fprintf(stderr, "Usage: %s [-n] [-c ccache]\n"
		    "\tThis utility edits a ccache, setting all ticket\n"
		    "\tenc_part kvnos to zero or absent (if -n is set).\n",
		    argv[0]);
	    return 1;
	}
    }

    if (!src_cc) {
	during = "cc_default";
	ret = krb5_cc_default(context, &src_cc);
	if (ret) goto err;
    }

    during = "cc_get_principal";
    ret = krb5_cc_get_principal(context, src_cc, &me);
    if (ret) goto err;

    if (optind != argc) {
	fprintf(stderr, "Usage: %s [-n] [-c ccache]\n"
		"\tThis utility edits a ccache, setting all ticket\n"
		"\tenc_part kvnos to zero or absent (if -n is set).\n",
		argv[0]);
	return 1;
    }

    during = "cc_new_unique of temporary ccache";
    ret = krb5_cc_new_unique(context, krb5_cc_get_type(context, src_cc),
			     NULL, &dst_cc);

    during = "cc_initialize of temporary ccache";
    ret = krb5_cc_initialize(context, dst_cc, me);
    if (ret) goto err;

    during = "cc_start_seq_get";
    ret = krb5_cc_start_seq_get(context, src_cc, &cursor);
    if (ret) goto err;

    while ((ret = krb5_cc_next_cred(context, src_cc, &cursor, &cred)) == 0) {
	krb5_data data;

	during = "decode_Ticket";
	memset(&t, 0, sizeof (t));
	ret = decode_Ticket(cred.ticket.data, cred.ticket.length, &t, &len);
	if (ret == ASN1_MISSING_FIELD)
	    continue;
	if (ret) goto err;
	if (t.enc_part.kvno) {
	    *t.enc_part.kvno = 0;
	    if (make_kvno_absent) {
		free(t.enc_part.kvno);
		t.enc_part.kvno = NULL;
	    }
	    /*
	     * The new Ticket has to need less or same space as before, so
	     * we reuse cred->icket.data.
	     */
	    during = "encode_Ticket";
	    ASN1_MALLOC_ENCODE(Ticket, data.data, data.length, &t, &len, ret);
	    if (ret) {
		free_Ticket(&t);
		goto err;
	    }
	    krb5_data_free(&cred.ticket);
	    cred.ticket = data;
	}
	free_Ticket(&t);
	during = "cc_store_cred";
	ret = krb5_cc_store_cred(context, dst_cc, &cred);
	if (ret) goto err;
	krb5_free_cred_contents(context, &cred);
	memset(&cred, 0, sizeof (cred));
    }
    during = "cc_next_cred";
    if (ret != KRB5_CC_END) goto err;

    during = "cc_end_seq_get";
    ret = krb5_cc_end_seq_get(context, src_cc, &cursor);
    if (ret) goto err;

    during = "cc_move";
    ret = krb5_cc_move(context, dst_cc, src_cc);
    if (ret) goto err;
    dst_cc = NULL;

    during = "cc_switch";
    ret = krb5_cc_switch(context, src_cc);
    if (ret) goto err;

err:
    (void) krb5_free_principal(context, me);
    if (src_cc)
	(void) krb5_cc_close(context, src_cc);
    if (dst_cc)
	(void) krb5_cc_destroy(context, dst_cc);
    if (ret) {
	fprintf(stderr, "Failed while doing %s (%d)\n", during, ret);
	ret = 1;
    }
    return (ret);
}


/*	$NetBSD: kgetcred.c,v 1.2 2017/01/28 21:31:45 christos Exp $	*/

/*
 * Copyright (c) 1997 - 2008 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "kuser_locl.h"

static char *cache_str;
static char *out_cache_str;
static char *delegation_cred_str;
static char *etype_str;
static int transit_flag = 1;
static int forwardable_flag;
static int canonicalize_flag;
static int is_hostbased_flag;
static int is_canonical_flag;
static char *impersonate_str;
static char *nametype_str;
static int store_flag = 1;
static int cached_only_flag;
static int debug_flag;
static int version_flag;
static int help_flag;

struct getargs args[] = {
    { "cache",		'c', arg_string, &cache_str,
      NP_("credential cache to use", ""), "cache"},
    { "out-cache",	0,   arg_string, &out_cache_str,
      NP_("credential cache to store credential in", ""), "cache"},
    { "delegation-credential-cache",0,arg_string, &delegation_cred_str,
      NP_("where to find the ticket use for delegation", ""), "cache"},
    { "canonicalize",	0, arg_flag, &canonicalize_flag,
      NP_("canonicalize the principal (chase referrals)", ""), NULL },
    { "canonical",	0,   arg_flag, &is_canonical_flag,
      NP_("the name components are canonical", ""), NULL },
    { "forwardable",	0,   arg_flag, &forwardable_flag,
      NP_("forwardable ticket requested", ""), NULL},
    { "transit-check",	0,   arg_negative_flag, &transit_flag, NULL, NULL },
    { "enctype",	'e', arg_string, &etype_str,
      NP_("encryption type to use", ""), "enctype"},
    { "impersonate",	0,   arg_string, &impersonate_str,
      NP_("client to impersonate", ""), "principal"},
    { "name-type",	0,   arg_string, &nametype_str,
      NP_("Kerberos name type", ""), NULL },
    { "hostbased",	'H', arg_flag, &is_hostbased_flag,
      NP_("indicate that the name is a host-based service name", ""), NULL },
    { "store", 	        0,   arg_negative_flag, &store_flag,
      NP_("don't store the tickets obtained in the cache", ""), NULL },
    { "cached-only", 	        0,   arg_flag, &cached_only_flag,
      NP_("don't talk to the KDC, just search the cache", ""), NULL },
    { "debug", 	        0,   arg_flag, &debug_flag, NULL, NULL },
    { "version", 	0,   arg_flag, &version_flag, NULL, NULL },
    { "help",		0,   arg_flag, &help_flag, NULL, NULL }
};

static void
usage(int ret)
{
    arg_printusage(args,
		   sizeof(args)/sizeof(*args),
		   NULL,
		   "service");
    exit (ret);
}

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context context;
    krb5_ccache cache;
    krb5_creds *out;
    int optidx = 0;
    int32_t nametype = KRB5_NT_UNKNOWN;
    krb5_get_creds_opt opt;
    krb5_principal server = NULL;
    krb5_principal impersonate;

    setprogname(argv[0]);

    ret = krb5_init_context(&context);
    if (ret)
	errx(1, "krb5_init_context failed: %d", ret);

    if (getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optidx))
	usage(1);

    if (help_flag)
	usage (0);

    if (version_flag) {
	print_version(NULL);
	exit(0);
    }

    argc -= optidx;
    argv += optidx;

    if (debug_flag) {
        ret = krb5_set_debug_dest(context, getprogname(), "STDERR");
        if (ret)
            krb5_warn(context, ret, "krb5_set_debug_dest");
    }

    if (cache_str) {
	ret = krb5_cc_resolve(context, cache_str, &cache);
	if (ret)
	    krb5_err(context, 1, ret, "%s", cache_str);
    } else {
	ret = krb5_cc_default (context, &cache);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_cc_resolve");
    }

    ret = krb5_get_creds_opt_alloc(context, &opt);
    if (ret)
	krb5_err(context, 1, ret, "krb5_get_creds_opt_alloc");

    if (etype_str) {
	krb5_enctype enctype;

	ret = krb5_string_to_enctype(context, etype_str, &enctype);
	if (ret)
	    krb5_errx(context, 1, N_("unrecognized enctype: %s", ""),
		      etype_str);
	krb5_get_creds_opt_set_enctype(context, opt, enctype);
    }

    if (impersonate_str) {
	ret = krb5_parse_name(context, impersonate_str, &impersonate);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_parse_name %s", impersonate_str);
	krb5_get_creds_opt_set_impersonate(context, opt, impersonate);
	krb5_get_creds_opt_add_options(context, opt, KRB5_GC_NO_STORE);
        krb5_free_principal(context, impersonate);
    }

    if (out_cache_str)
	krb5_get_creds_opt_add_options(context, opt, KRB5_GC_NO_STORE);

    if (forwardable_flag)
	krb5_get_creds_opt_add_options(context, opt, KRB5_GC_FORWARDABLE);
    if (!transit_flag)
	krb5_get_creds_opt_add_options(context, opt, KRB5_GC_NO_TRANSIT_CHECK);
    if (canonicalize_flag)
	krb5_get_creds_opt_add_options(context, opt, KRB5_GC_CANONICALIZE);
    if (!store_flag)
	krb5_get_creds_opt_add_options(context, opt, KRB5_GC_NO_STORE);
    if (cached_only_flag)
	krb5_get_creds_opt_add_options(context, opt, KRB5_GC_CACHED);

    if (delegation_cred_str) {
	krb5_ccache id;
	krb5_creds c, mc;
	Ticket ticket;

	krb5_cc_clear_mcred(&mc);
	ret = krb5_cc_get_principal(context, cache, &mc.server);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_cc_get_principal");

	ret = krb5_cc_resolve(context, delegation_cred_str, &id);
	if(ret)
	    krb5_err(context, 1, ret, "krb5_cc_resolve");

	ret = krb5_cc_retrieve_cred(context, id, 0, &mc, &c);
	if(ret)
	    krb5_err(context, 1, ret, "krb5_cc_retrieve_cred");

	ret = decode_Ticket(c.ticket.data, c.ticket.length, &ticket, NULL);
	if (ret) {
	    krb5_clear_error_message(context);
	    krb5_err(context, 1, ret, "decode_Ticket");
	}
	krb5_free_cred_contents(context, &c);

	ret = krb5_get_creds_opt_set_ticket(context, opt, &ticket);
	if(ret)
	    krb5_err(context, 1, ret, "krb5_get_creds_opt_set_ticket");
	free_Ticket(&ticket);

	krb5_cc_close(context, id);
	krb5_free_principal(context, mc.server);

	krb5_get_creds_opt_add_options(context, opt,
				       KRB5_GC_CONSTRAINED_DELEGATION);
    }

    if (nametype_str != NULL) {
        ret = krb5_parse_nametype(context, nametype_str, &nametype);
        if (ret)
            krb5_err(context, 1, ret, "krb5_parse_nametype");
    }

    if (nametype == KRB5_NT_SRV_HST ||
        nametype == KRB5_NT_SRV_HST_NEEDS_CANON)
        is_hostbased_flag = 1;

    if (is_hostbased_flag) {
	const char *sname = NULL;
	const char *hname = NULL;

        if (nametype_str != NULL &&
            nametype != KRB5_NT_SRV_HST &&
            nametype != KRB5_NT_SRV_HST_NEEDS_CANON)
            krb5_errx(context, 1, "--hostbased not compatible with "
                      "non-hostbased --name-type");

        if (is_canonical_flag)
            nametype = KRB5_NT_SRV_HST;
        else
            nametype = KRB5_NT_SRV_HST_NEEDS_CANON;

        /*
         * Host-based service names can have more than one component.
         *
         * RFC5179 did not, but should have, assign a Kerberos name-type
         * corresponding to GSS_C_NT_DOMAINBASED.  But it's basically a
         * host-based service name type with one additional component.
         *
         * So that's how we're treating host-based service names here:
         * two or more components.
         */

        if (argc == 0) {
            usage(1);
        } else if (argc == 1) {
            krb5_principal server2;

            /*
             * In this case the one argument is a principal name, not the
             * service name.
             *
             * We parse the argument as a principal name, extract the service
             * and hostname components, use krb5_sname_to_principal(), then
             * extract the service and hostname components from that.
             */

            ret = krb5_parse_name(context, argv[0], &server);
            if (ret)
                krb5_err(context, 1, ret, "krb5_parse_name %s", argv[0]);
            sname = krb5_principal_get_comp_string(context, server, 0);

            /*
             * If a single-component principal name is given, then we'll
             * default the hostname, as krb5_principal_get_comp_string()
             * returns NULL in this case.
             */
            hname = krb5_principal_get_comp_string(context, server, 1);

	    ret = krb5_sname_to_principal(context, hname, sname,
					   KRB5_NT_SRV_HST, &server2);
            sname = krb5_principal_get_comp_string(context, server2, 0);
            hname = krb5_principal_get_comp_string(context, server2, 1);

            /*
             * Modify the original with the new sname/hname.  This way we
             * retain any additional principal name components from the given
             * principal name.
             *
             * The name-type is set further below.
             */
            ret = krb5_principal_set_comp_string(context, server, 0, sname);
            if (ret)
                krb5_err(context, 1, ret, "krb5_principal_set_comp_string %s", argv[0]);
            ret = krb5_principal_set_comp_string(context, server, 1, hname);
            if (ret)
                krb5_err(context, 1, ret, "krb5_principal_set_comp_string %s", argv[0]);
            krb5_free_principal(context, server2);
        } else {
            size_t i;

            /*
             * In this case the arguments are principal name components.
             *
             * The service and hostname components can be defaulted by passing
             * empty strings.
             */
	    sname = argv[0];
            if (*sname == '\0')
                sname = NULL;
	    hname = argv[1];
            if (hname == NULL || *hname == '\0')
                hname = NULL;
	    ret = krb5_sname_to_principal(context, hname, sname,
                                          KRB5_NT_SRV_HST, &server);
	    if (ret)
		krb5_err(context, 1, ret, "krb5_sname_to_principal");

            for (i = 2; i < argc; i++) {
                ret = krb5_principal_set_comp_string(context, server, i, argv[i]);
                if (ret)
                    krb5_err(context, 1, ret, "krb5_principal_set_comp_string");
            }
	}
    } else if (argc == 1) {
	ret = krb5_parse_name(context, argv[0], &server);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_parse_name %s", argv[0]);
    } else {
	usage(1);
    }

    if (nametype != KRB5_NT_UNKNOWN)
        server->name.name_type = (NAME_TYPE)nametype;

    ret = krb5_get_creds(context, opt, cache, server, &out);
    if (ret)
	krb5_err(context, 1, ret, "krb5_get_creds");

    if (out_cache_str) {
	krb5_ccache id;

	ret = krb5_cc_resolve(context, out_cache_str, &id);
	if(ret)
	    krb5_err(context, 1, ret, "krb5_cc_resolve");

	ret = krb5_cc_initialize(context, id, out->client);
	if(ret)
	    krb5_err(context, 1, ret, "krb5_cc_initialize");

	ret = krb5_cc_store_cred(context, id, out);
	if(ret)
	    krb5_err(context, 1, ret, "krb5_cc_store_cred");
	krb5_cc_close(context, id);
    }

    krb5_free_creds(context, out);
    krb5_free_principal(context, server);
    krb5_get_creds_opt_free(context, opt);
    krb5_cc_close (context, cache);
    krb5_free_context (context);

    return 0;
}

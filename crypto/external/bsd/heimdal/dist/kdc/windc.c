/*	$NetBSD: windc.c,v 1.2 2017/01/28 21:31:44 christos Exp $	*/

/*
 * Copyright (c) 2007 Kungliga Tekniska Högskolan
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

#include "kdc_locl.h"

static int have_plugin = 0;

/*
 * Pick the first WINDC module that we find.
 */

static krb5_error_code KRB5_LIB_CALL
load(krb5_context context, const void *plug, void *plugctx, void *userctx)
{
    have_plugin = 1;
    return KRB5_PLUGIN_NO_HANDLE;
}

krb5_error_code
krb5_kdc_windc_init(krb5_context context)
{
    (void)_krb5_plugin_run_f(context, "krb5", "windc",
			     KRB5_WINDC_PLUGIN_MINOR, 0, NULL, load);
    return 0;
}

struct generate_uc {
    hdb_entry_ex *client;
    krb5_pac *pac;
};

static krb5_error_code KRB5_LIB_CALL
generate(krb5_context context, const void *plug, void *plugctx, void *userctx)
{
    krb5plugin_windc_ftable *ft = (krb5plugin_windc_ftable *)plug;
    struct generate_uc *uc = (struct generate_uc *)userctx;    

    if (ft->pac_generate == NULL)
	return KRB5_PLUGIN_NO_HANDLE;
    return ft->pac_generate((void *)plug, context, uc->client, uc->pac);
}


krb5_error_code
_kdc_pac_generate(krb5_context context,
		  hdb_entry_ex *client,
		  krb5_pac *pac)
{
    struct generate_uc uc;

    if (!have_plugin)
	return 0;

    uc.client = client;
    uc.pac = pac;

    (void)_krb5_plugin_run_f(context, "krb5", "windc",
			     KRB5_WINDC_PLUGIN_MINOR, 0, &uc, generate);
    return 0;
}

struct verify_uc {
    krb5_principal client_principal;
    krb5_principal delegated_proxy_principal;
    hdb_entry_ex *client;
    hdb_entry_ex *server;
    hdb_entry_ex *krbtgt;
    krb5_pac *pac;
    int *verified;
};

static krb5_error_code KRB5_LIB_CALL
verify(krb5_context context, const void *plug, void *plugctx, void *userctx)
{
    krb5plugin_windc_ftable *ft = (krb5plugin_windc_ftable *)plug;
    struct verify_uc *uc = (struct verify_uc *)userctx;    
    krb5_error_code ret;

    if (ft->pac_verify == NULL)
	return KRB5_PLUGIN_NO_HANDLE;
    ret = ft->pac_verify((void *)plug, context,
			 uc->client_principal,
			 uc->delegated_proxy_principal,
			 uc->client, uc->server, uc->krbtgt, uc->pac);
    if (ret == 0)
	(*uc->verified) = 1;

    return 0;
}

krb5_error_code
_kdc_pac_verify(krb5_context context,
		const krb5_principal client_principal,
		const krb5_principal delegated_proxy_principal,
		hdb_entry_ex *client,
		hdb_entry_ex *server,
		hdb_entry_ex *krbtgt,
		krb5_pac *pac,
		int *verified)
{
    struct verify_uc uc;

    if (!have_plugin)
	return 0;

    uc.client_principal = client_principal;
    uc.delegated_proxy_principal = delegated_proxy_principal;
    uc.client = client;
    uc.server = server;
    uc.krbtgt = krbtgt;
    uc.pac = pac;
    uc.verified = verified;

    (void)_krb5_plugin_run_f(context, "krb5", "windc",
			     KRB5_WINDC_PLUGIN_MINOR, 0, &uc, verify);
    return 0;
}

struct check_uc {
    krb5_kdc_configuration *config;
    hdb_entry_ex *client_ex;
    const char *client_name;
    hdb_entry_ex *server_ex;
    const char *server_name;
    KDC_REQ *req;
    METHOD_DATA *method_data;
};

static krb5_error_code KRB5_LIB_CALL
check(krb5_context context, const void *plug, void *plugctx, void *userctx)
{
    krb5plugin_windc_ftable *ft = (krb5plugin_windc_ftable *)plug;
    struct check_uc *uc = (struct check_uc *)userctx;    

    if (ft->client_access == NULL)
	return KRB5_PLUGIN_NO_HANDLE;
    return ft->client_access((void *)plug, context, uc->config, 
			     uc->client_ex, uc->client_name, 
			     uc->server_ex, uc->server_name, 
			     uc->req, uc->method_data);
}


krb5_error_code
_kdc_check_access(krb5_context context,
		  krb5_kdc_configuration *config,
		  hdb_entry_ex *client_ex, const char *client_name,
		  hdb_entry_ex *server_ex, const char *server_name,
		  KDC_REQ *req,
		  METHOD_DATA *method_data)
{
    krb5_error_code ret = KRB5_PLUGIN_NO_HANDLE;
    struct check_uc uc;

    if (have_plugin) {
        uc.config = config;
        uc.client_ex = client_ex;
        uc.client_name = client_name;
        uc.server_ex = server_ex;
        uc.server_name = server_name;
        uc.req = req;
        uc.method_data = method_data;

        ret = _krb5_plugin_run_f(context, "krb5", "windc",
                                 KRB5_WINDC_PLUGIN_MINOR, 0, &uc, check);
    }

    if (ret == KRB5_PLUGIN_NO_HANDLE)
	return kdc_check_flags(context, config,
			       client_ex, client_name,
			       server_ex, server_name,
			       req->msg_type == krb_as_req);
    return ret;
}

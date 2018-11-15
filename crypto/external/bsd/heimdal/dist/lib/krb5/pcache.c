/*	$NetBSD: pcache.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/***********************************************************************
 * Copyright (c) 2010, Secure Endpoints Inc.
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
 **********************************************************************/

#include "krb5_locl.h"
#include "ccache_plugin.h"
#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif
#include <assert.h>

/*
 * cc_plugin_register_to_context is executed once per krb5_init_context().
 * Its job is to register the plugin's krb5_cc_ops structure with the
 * krb5_context.
 */

static krb5_error_code KRB5_LIB_CALL
cc_plugin_register_to_context(krb5_context context, const void *plug, void *plugctx, void *userctx)
{
    krb5_cc_ops *ccops = (krb5_cc_ops *)plugctx;
    krb5_error_code ret;

    if (ccops == NULL || ccops->version < KRB5_CC_OPS_VERSION)
       return KRB5_PLUGIN_NO_HANDLE;

    ret = krb5_cc_register(context, ccops, TRUE);
    if (ret != 0)
       *((krb5_error_code *)userctx) = ret;

    return KRB5_PLUGIN_NO_HANDLE;
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_load_ccache_plugins(krb5_context context)
{
    krb5_error_code userctx = 0;

    (void)_krb5_plugin_run_f(context, "krb5", KRB5_PLUGIN_CCACHE,
			     0, 0, &userctx, cc_plugin_register_to_context);

    return userctx;
}

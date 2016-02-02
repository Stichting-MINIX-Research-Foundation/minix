/*	$NetBSD: delete_sec_context.c,v 1.1.1.1 2011/04/13 18:14:47 elric Exp $	*/

/*
 * Copyright (c) 2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 Apple Inc. All rights reserved.
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

#include "netlogon.h"

OM_uint32
_netlogon_delete_sec_context(OM_uint32 * minor_status,
                             gss_ctx_id_t * context_handle,
                             gss_buffer_t output_token)
{
    if (context_handle != NULL && *context_handle != GSS_C_NO_CONTEXT) {
        gssnetlogon_ctx ctx = (gssnetlogon_ctx)*context_handle;

        *context_handle = GSS_C_NO_CONTEXT;

        _netlogon_release_name(minor_status, (gss_name_t *)&ctx->SourceName);
        _netlogon_release_name(minor_status, (gss_name_t *)&ctx->TargetName);
        HEIMDAL_MUTEX_destroy(&ctx->Mutex);
        memset(ctx, 0, sizeof(*ctx));
        free(ctx);
    }

    if (output_token != GSS_C_NO_BUFFER) {
        output_token->length = 0;
        output_token->value  = NULL;
    }

    *minor_status = 0;
    return GSS_S_COMPLETE;
}

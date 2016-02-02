/*	$NetBSD: init_sec_context.c,v 1.1.1.1 2011/04/13 18:14:47 elric Exp $	*/

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
#include <nameser.h>

static OM_uint32
_netlogon_encode_dns_string(OM_uint32 *minor_status,
                            const gss_buffer_t str,
                            gss_buffer_t buffer)
{
    int ret;

    memset(buffer->value, 0, buffer->length);

    ret = ns_name_compress((const char *)str->value,
                           (uint8_t *)buffer->value, buffer->length,
                           NULL, NULL);
    if (ret < 0) {
        *minor_status = errno;
        return GSS_S_FAILURE;
    }

    buffer->length = ret;

    *minor_status = 0;
    return GSS_S_COMPLETE;
}

static OM_uint32
_netlogon_make_initial_auth_message(OM_uint32 *minor_status,
                                    gssnetlogon_ctx ctx,
                                    gss_buffer_t output_token)
{
    uint32_t flags = 0;
#define MAX_NL_NAMES    5
    gss_buffer_desc names[MAX_NL_NAMES];
    uint8_t comp_names[3][MAXHOSTNAMELEN * 2];
    size_t n = 0, i = 0, len;
    OM_uint32 ret;
    uint8_t *p;

    if (ctx->TargetName->NetbiosName.length) {
        flags |= NL_FLAG_NETBIOS_DOMAIN_NAME;
        names[n] = ctx->TargetName->NetbiosName; /* OEM encoding */
        names[n].length++;
        n++;
    }
    if (ctx->SourceName->NetbiosName.length) {
        flags |= NL_FLAG_NETBIOS_COMPUTER_NAME;
        names[n] = ctx->SourceName->NetbiosName; /* OEM encoding */
        names[n].length++;
        n++;
    }
    if (ctx->TargetName->DnsName.length) {
        flags |= NL_FLAG_DNS_DOMAIN_NAME;
        names[n].value = comp_names[i++];
        names[n].length = MAXHOSTNAMELEN * 2;
        ret = _netlogon_encode_dns_string(minor_status,
                                          &ctx->TargetName->DnsName,
                                          &names[n]);
        if (GSS_ERROR(ret))
            return ret;
        n++;
    }
    if (ctx->SourceName->DnsName.length) {
        flags |= NL_FLAG_DNS_HOST_NAME;
        names[n].value = comp_names[i++];
        names[n].length = MAXHOSTNAMELEN * 2;
        ret = _netlogon_encode_dns_string(minor_status,
                                          &ctx->SourceName->DnsName,
                                          &names[n]);
        if (GSS_ERROR(ret))
            return ret;
        n++;
    }
    if (ctx->SourceName->NetbiosName.length) {
        flags |= NL_FLAG_UTF8_COMPUTER_NAME;
        names[n].value = comp_names[i++];
        names[n].length = MAXHOSTNAMELEN * 2;
        ret = _netlogon_encode_dns_string(minor_status,
                                          &ctx->SourceName->NetbiosName,
                                          &names[n]);
        if (GSS_ERROR(ret))
            return ret;
        n++;
    }

    for (i = 0, len = NL_AUTH_MESSAGE_LENGTH; i < n; i++) {
        len += names[i].length;
    }

    output_token->value = malloc(len);
    if (output_token->value == NULL) {
        *minor_status = ENOMEM;
        return GSS_S_FAILURE;
    }

    p = (uint8_t *)output_token->value;
    _gss_mg_encode_le_uint32(NL_NEGOTIATE_REQUEST_MESSAGE, p);
    _gss_mg_encode_le_uint32(flags, p + 4);
    p += 8;

    for (i = 0; i < n; i++) {
        assert(names[i].length != 0);
        assert(((char *)names[i].value)[names[i].length - 1] == '\0');
        memcpy(p, names[i].value, names[i].length);
        p += names[i].length;
    }

    output_token->length = len;
    assert(p == (uint8_t *)output_token->value + len);

    *minor_status = 0;
    return GSS_S_CONTINUE_NEEDED;
}

static OM_uint32
_netlogon_read_initial_auth_message(OM_uint32 *minor_status,
                                    gssnetlogon_ctx ctx,
                                    const gss_buffer_t input_token)
{
    NL_AUTH_MESSAGE msg;
    const uint8_t *p = (const uint8_t *)input_token->value;

    if (ctx->State != NL_AUTH_NEGOTIATE) {
        *minor_status = EINVAL;
        return GSS_S_FAILURE;
    }

    if (input_token->length < NL_AUTH_MESSAGE_LENGTH)
        return GSS_S_DEFECTIVE_TOKEN;

    _gss_mg_decode_le_uint32(&p[0], &msg.MessageType);
    _gss_mg_decode_le_uint32(&p[4], &msg.Flags);

    if (msg.MessageType != NL_NEGOTIATE_RESPONSE_MESSAGE ||
        msg.Flags != 0)
        return GSS_S_DEFECTIVE_TOKEN;

    ctx->State = NL_AUTH_ESTABLISHED;

    *minor_status = 0;
    return GSS_S_COMPLETE;
}

static OM_uint32
_netlogon_alloc_context(OM_uint32 *minor_status,
                        gssnetlogon_ctx *pContext)
{
    gssnetlogon_ctx ctx;

    ctx = (gssnetlogon_ctx)calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        *minor_status = ENOMEM;
        return GSS_S_FAILURE;
    }

    ctx->State = NL_AUTH_NEGOTIATE;
    ctx->LocallyInitiated = 1;
    ctx->MessageBlockSize = 1;

    HEIMDAL_MUTEX_init(&ctx->Mutex);

    *pContext = ctx;

    return GSS_S_COMPLETE;
}

OM_uint32
_netlogon_init_sec_context(OM_uint32 * minor_status,
			   const gss_cred_id_t initiator_cred_handle,
			   gss_ctx_id_t * context_handle,
			   const gss_name_t target_name,
			   const gss_OID mech_type,
			   OM_uint32 req_flags,
			   OM_uint32 time_req,
			   const gss_channel_bindings_t input_chan_bindings,
			   const gss_buffer_t input_token,
			   gss_OID * actual_mech_type,
			   gss_buffer_t output_token,
			   OM_uint32 * ret_flags,
			   OM_uint32 * time_rec)
{
    const gssnetlogon_cred cred = (const gssnetlogon_cred)initiator_cred_handle;
    gssnetlogon_ctx ctx = (gssnetlogon_ctx)*context_handle;
    const gssnetlogon_name target = (const gssnetlogon_name)target_name;
    OM_uint32 ret;

    *minor_status = 0;

    output_token->value = NULL;
    output_token->length = 0;

    /* Validate arguments */
    if (cred == NULL)
        return GSS_S_NO_CRED;
    else if (target == NULL)
        return GSS_S_BAD_NAME;

    if (ctx == NULL) {
        if (input_token->length != 0)
            return GSS_S_DEFECTIVE_TOKEN;

        ret = _netlogon_alloc_context(minor_status, &ctx);
        if (GSS_ERROR(ret))
            goto cleanup;

        HEIMDAL_MUTEX_lock(&ctx->Mutex);
        *context_handle = (gss_ctx_id_t)ctx;

	ctx->GssFlags = req_flags & (GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG |
				     GSS_C_SEQUENCE_FLAG | GSS_C_CONF_FLAG |
				     GSS_C_INTEG_FLAG | GSS_C_DCE_STYLE);
        ctx->SignatureAlgorithm = cred->SignatureAlgorithm;
        ctx->SealAlgorithm = cred->SealAlgorithm;

        ret = _netlogon_duplicate_name(minor_status, (gss_name_t)cred->Name,
                                       (gss_name_t *)&ctx->SourceName);
        if (GSS_ERROR(ret))
            goto cleanup;

        ret = _netlogon_duplicate_name(minor_status, (gss_name_t)target,
                                       (gss_name_t *)&ctx->TargetName);
        if (GSS_ERROR(ret))
            goto cleanup;

        memcpy(ctx->SessionKey, cred->SessionKey, sizeof(cred->SessionKey));

        ret = _netlogon_make_initial_auth_message(minor_status, ctx,
                                                  output_token);
        if (GSS_ERROR(ret))
            goto cleanup;
    } else {
        HEIMDAL_MUTEX_lock(&ctx->Mutex);
        ret = _netlogon_read_initial_auth_message(minor_status, ctx,
                                                  input_token);
    }

    if (ret_flags != NULL)
	*ret_flags = ctx->GssFlags;
    if (time_rec != NULL)
	*time_rec = GSS_C_INDEFINITE;
    if (actual_mech_type != NULL)
	*actual_mech_type = GSS_NETLOGON_MECHANISM;

cleanup:
    HEIMDAL_MUTEX_unlock(&ctx->Mutex);

    if (ret != GSS_S_COMPLETE && ret != GSS_S_CONTINUE_NEEDED) {
        OM_uint32 tmp;
        _netlogon_delete_sec_context(&tmp, context_handle, NULL);
    }

    return ret;
}


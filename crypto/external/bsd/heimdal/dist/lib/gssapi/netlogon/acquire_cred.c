/*	$NetBSD: acquire_cred.c,v 1.1.1.1 2011/04/13 18:14:47 elric Exp $	*/

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
#include <gssapi_spi.h>

OM_uint32
_netlogon_acquire_cred(OM_uint32 * min_stat,
                       const gss_name_t desired_name,
                       OM_uint32 time_req,
                       const gss_OID_set desired_mechs,
                       gss_cred_usage_t cred_usage,
                       gss_cred_id_t * output_cred_handle,
                       gss_OID_set * actual_mechs,
                       OM_uint32 * time_rec)
{
    OM_uint32 ret;
    gssnetlogon_cred cred;

    /* only initiator support so far */
    if (cred_usage != GSS_C_INITIATE)
        return GSS_S_FAILURE;

    if (desired_name == GSS_C_NO_NAME)
        return GSS_S_BAD_NAME;

    cred = (gssnetlogon_cred)calloc(1, sizeof(*cred));
    if (cred == NULL) {
        *min_stat = ENOMEM;
        return GSS_S_FAILURE;
    }
    cred->SignatureAlgorithm = NL_SIGN_ALG_HMAC_MD5;
    cred->SealAlgorithm = NL_SEAL_ALG_RC4;

    ret = _netlogon_duplicate_name(min_stat, desired_name,
                                   (gss_name_t *)&cred->Name);
    if (GSS_ERROR(ret)) {
        free(cred);
        return ret;
    }

    *output_cred_handle = (gss_cred_id_t)cred;
    if (actual_mechs != NULL)
        *actual_mechs = GSS_C_NO_OID_SET;
    if (time_rec != NULL)
        *time_rec = GSS_C_INDEFINITE;

    return GSS_S_COMPLETE;
}

OM_uint32
_netlogon_acquire_cred_ex(gss_status_id_t status,
                          const gss_name_t desired_name,
                          OM_uint32 flags,
                          OM_uint32 time_req,
                          gss_cred_usage_t cred_usage,
                          gss_auth_identity_t identity,
                          void *ctx,
                          void (*complete)(void *, OM_uint32, gss_status_id_t, gss_cred_id_t, OM_uint32))
{
    return GSS_S_UNAVAILABLE;
}

/*
 * value contains 16 byte session key
 */
static OM_uint32
_netlogon_set_session_key(OM_uint32 *minor_status,
                          gss_cred_id_t *cred_handle,
                          const gss_buffer_t value)
{
    gssnetlogon_cred cred;

    if (*cred_handle == GSS_C_NO_CREDENTIAL) {
        *minor_status = EINVAL;
        return GSS_S_FAILURE;
    }

    cred = (gssnetlogon_cred)*cred_handle;

    if (value->length != sizeof(cred->SessionKey)) {
        *minor_status = ERANGE;
        return GSS_S_FAILURE;
    }

    memcpy(cred->SessionKey, value->value, value->length);

    *minor_status = 0;
    return GSS_S_COMPLETE;
}

/*
 * value contains 16 bit little endian encoded seal algorithm
 */
static OM_uint32
_netlogon_set_sign_algorithm(OM_uint32 *minor_status,
                             gss_cred_id_t *cred_handle,
                             const gss_buffer_t value)
{
    gssnetlogon_cred cred;
    uint16_t alg;
    const uint8_t *p;

    if (*cred_handle == GSS_C_NO_CREDENTIAL) {
        *minor_status = EINVAL;
        return GSS_S_FAILURE;
    }

    cred = (gssnetlogon_cred)*cred_handle;

    if (value->length != 2) {
        *minor_status = ERANGE;
        return GSS_S_FAILURE;
    }

    p = (const uint8_t *)value->value;
    alg = (p[0] << 0) | (p[1] << 8);

    if (alg != NL_SIGN_ALG_HMAC_MD5 && alg != NL_SIGN_ALG_SHA256) {
        *minor_status = EINVAL;
        return GSS_S_FAILURE;
    }

    cred->SignatureAlgorithm = alg;
    if (alg == NL_SIGN_ALG_SHA256)
        cred->SealAlgorithm = NL_SEAL_ALG_AES128;
    else
        cred->SealAlgorithm = NL_SEAL_ALG_RC4;

    *minor_status = 0;
    return GSS_S_COMPLETE;
}

OM_uint32
_netlogon_set_cred_option
           (OM_uint32 *minor_status,
            gss_cred_id_t *cred_handle,
            const gss_OID desired_object,
            const gss_buffer_t value)
{
    if (value == GSS_C_NO_BUFFER) {
        *minor_status = EINVAL;
        return GSS_S_FAILURE;
    }

    if (gss_oid_equal(desired_object, GSS_NETLOGON_SET_SESSION_KEY_X))
        return _netlogon_set_session_key(minor_status, cred_handle, value);
    else if (gss_oid_equal(desired_object, GSS_NETLOGON_SET_SIGN_ALGORITHM_X))
        return _netlogon_set_sign_algorithm(minor_status, cred_handle, value);

    *minor_status = EINVAL;
    return GSS_S_FAILURE;
}


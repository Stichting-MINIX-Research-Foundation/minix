/*	$NetBSD: add_cred.c,v 1.1.1.1 2011/04/13 18:14:47 elric Exp $	*/

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

OM_uint32 _netlogon_add_cred (
     OM_uint32           *minor_status,
     const gss_cred_id_t input_cred_handle,
     const gss_name_t    desired_name,
     const gss_OID       desired_mech,
     gss_cred_usage_t    cred_usage,
     OM_uint32           initiator_time_req,
     OM_uint32           acceptor_time_req,
     gss_cred_id_t       *output_cred_handle,
     gss_OID_set         *actual_mechs,
     OM_uint32           *initiator_time_rec,
     OM_uint32           *acceptor_time_rec)
{
    OM_uint32 ret;
    int equal;
    const gssnetlogon_cred src = (const gssnetlogon_cred)input_cred_handle;
    gssnetlogon_cred dst;

    if (desired_name != GSS_C_NO_NAME) {
        if (input_cred_handle != GSS_C_NO_CREDENTIAL) {
            ret = _netlogon_compare_name(minor_status, desired_name,
                                         (gss_name_t)src->Name, &equal);
            if (GSS_ERROR(ret))
                return ret;

            if (!equal)
                return GSS_S_BAD_NAME;
        }
    }

    ret = _netlogon_acquire_cred(minor_status,
                                 input_cred_handle ? (gss_name_t)src->Name : desired_name,
                                 initiator_time_req, GSS_C_NO_OID_SET, cred_usage,
                                 output_cred_handle, actual_mechs, initiator_time_rec);
    if (GSS_ERROR(ret))
        return ret;

    dst = (gssnetlogon_cred)*output_cred_handle;

    if (src != NULL) {
        dst->SignatureAlgorithm = src->SignatureAlgorithm;
        dst->SealAlgorithm = src->SealAlgorithm;

        memcpy(dst->SessionKey, src->SessionKey, sizeof(src->SessionKey));
    }

    if (acceptor_time_rec != NULL)
        *acceptor_time_rec = 0;

    return GSS_S_COMPLETE;
}


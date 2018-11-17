/*	$NetBSD: pname_to_uid.c,v 1.2 2017/01/28 21:31:46 christos Exp $	*/

/*
 * Copyright (c) 2011, PADL Software Pty Ltd.
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
 * 3. Neither the name of PADL Software nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PADL SOFTWARE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PADL SOFTWARE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "gsskrb5_locl.h"

OM_uint32 GSSAPI_CALLCONV
_gsskrb5_localname(OM_uint32 *minor_status,
                   gss_const_name_t pname,
                   const gss_OID mech_type,
                   gss_buffer_t localname)
{
    krb5_error_code ret;
    krb5_context context;
    krb5_const_principal princ = (krb5_const_principal)pname;
    char lnamebuf[256];

    GSSAPI_KRB5_INIT(&context);

    *minor_status = 0;

    ret = krb5_aname_to_localname(context, princ,
                                  sizeof(lnamebuf), lnamebuf);
    if (ret != 0) {
        *minor_status = ret;
        return GSS_S_FAILURE;
    }

    localname->length = strlen(lnamebuf);

    localname->value = malloc(localname->length + 1);
    if (localname->value == NULL) {
        localname->length = 0;
        *minor_status = ENOMEM;
        return GSS_S_FAILURE;
    }

    memcpy(localname->value, lnamebuf, localname->length + 1);
    *minor_status = 0;

    return GSS_S_COMPLETE;
}

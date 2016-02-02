/*	$NetBSD: duplicate_name.c,v 1.1.1.1 2011/04/13 18:14:47 elric Exp $	*/

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

OM_uint32 _netlogon_duplicate_name (
            OM_uint32 * minor_status,
            const gss_name_t src_name,
            gss_name_t * dest_name
           )
{
    const gssnetlogon_name src = (const gssnetlogon_name)src_name;
    gssnetlogon_name dst = NULL;

    dst = calloc(1, sizeof(*dst));
    if (dst == NULL)
        goto fail;

    dst->NetbiosName.value = malloc(src->NetbiosName.length + 1);
    if (dst->NetbiosName.value == NULL)
        goto fail;
    memcpy(dst->NetbiosName.value, src->NetbiosName.value,
           src->NetbiosName.length);
    dst->NetbiosName.length = src->NetbiosName.length;
    ((char *)dst->NetbiosName.value)[dst->NetbiosName.length] = '\0';

    if (src->DnsName.length != 0) {
        dst->DnsName.value = malloc(src->DnsName.length + 1);
        if (dst->DnsName.value == NULL)
            goto fail;
        memcpy(dst->DnsName.value, src->DnsName.value, src->DnsName.length);
        dst->DnsName.length = src->DnsName.length;
        ((char *)dst->DnsName.value)[dst->DnsName.length] = '\0';
    }

    *minor_status = 0;
    *dest_name = (gss_name_t)dst;
    return GSS_S_COMPLETE;

fail:
    _netlogon_release_name(minor_status, (gss_name_t *)&dst);
    *minor_status = ENOMEM;
    return GSS_S_FAILURE;
}


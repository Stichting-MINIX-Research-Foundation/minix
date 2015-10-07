/*	$NetBSD: import_name.c,v 1.1.1.1 2011/04/13 18:14:47 elric Exp $	*/

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
#include <ctype.h>

OM_uint32 _netlogon_import_name
           (OM_uint32 * minor_status,
            const gss_buffer_t input_name_buffer,
            const gss_OID input_name_type,
            gss_name_t * output_name
           )
{
    gssnetlogon_name name;
    const char *netbiosName;
    const char *dnsName = NULL;
    size_t len, i;

    if (!gss_oid_equal(input_name_type, GSS_NETLOGON_NT_NETBIOS_DNS_NAME)) {
        return GSS_S_BAD_NAME;
    }

    /* encoding is NetBIOS name \0 DNS name \0 */

    netbiosName = input_name_buffer->value;
    len = strlen(netbiosName);
    if (len < input_name_buffer->length)
        dnsName = netbiosName + len + 1;

    name = (gssnetlogon_name)calloc(1, sizeof(*name));
    if (name == NULL)
        goto cleanup;

    name->NetbiosName.value = malloc(len + 1);
    if (name->NetbiosName.value == NULL)
        goto cleanup;
    memcpy(name->NetbiosName.value, netbiosName, len + 1);
    name->NetbiosName.length = len;

    /* normalise name to uppercase XXX UTF-8 OK? */
    for (i = 0; i < len; i++) {
        ((char *)name->NetbiosName.value)[i] =
            toupper(((char *)name->NetbiosName.value)[i]);
    }

    if (dnsName != NULL && dnsName[0] != '\0') {
        name->DnsName.value = strdup(dnsName);
        if (name->DnsName.value == NULL)
            goto cleanup;
        name->DnsName.length = strlen(dnsName);
    }

    *output_name = (gss_name_t)name;
    *minor_status = 0;
    return GSS_S_COMPLETE;

cleanup:
    _netlogon_release_name(minor_status, (gss_name_t *)&name);
    *minor_status = ENOMEM;
    return GSS_S_FAILURE;
}


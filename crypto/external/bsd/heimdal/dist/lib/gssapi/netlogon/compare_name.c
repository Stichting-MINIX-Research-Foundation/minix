/*	$NetBSD: compare_name.c,v 1.3 2017/01/30 20:27:58 christos Exp $	*/

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

OM_uint32 _netlogon_compare_name
           (OM_uint32 * minor_status,
            gss_const_name_t name1,
            gss_const_name_t name2,
            int * name_equal
           )
{
    const gssnetlogon_name n1 = (const gssnetlogon_name)name1;
    const gssnetlogon_name n2 = (const gssnetlogon_name)name2;

    *name_equal = 0;

    if (n1->NetbiosName.value != NULL && n2->NetbiosName.value != NULL)
        *name_equal = (strcasecmp((char *)n1->NetbiosName.value,
                                  (char *)n2->NetbiosName.value) == 0);

    if (n1->DnsName.value != NULL && n2->DnsName.value != NULL)
        *name_equal = (strcasecmp((char *)n1->DnsName.value,
                                  (char *)n2->DnsName.value) == 0);

    *minor_status = 0;
    return GSS_S_COMPLETE;
}


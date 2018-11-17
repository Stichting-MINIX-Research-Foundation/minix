/*	$NetBSD: locate_plugin.h,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
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

/* Id */

#ifndef HEIMDAL_KRB5_LOCATE_PLUGIN_H
#define HEIMDAL_KRB5_LOCATE_PLUGIN_H 1

#define KRB5_PLUGIN_LOCATE "service_locator"
#define KRB5_PLUGIN_LOCATE_VERSION 1
#define KRB5_PLUGIN_LOCATE_VERSION_0 0
#define KRB5_PLUGIN_LOCATE_VERSION_1 1
#define KRB5_PLUGIN_LOCATE_VERSION_2 2

enum locate_service_type {
    locate_service_kdc = 1,
    locate_service_master_kdc,
    locate_service_kadmin,
    locate_service_krb524,
    locate_service_kpasswd
};

typedef krb5_error_code
(*krb5plugin_service_locate_lookup) (void *, unsigned long, enum locate_service_type,
				     const char *, int, int,
				     int (*)(void *,int,struct sockaddr *),
				     void *);

#define KRB5_PLF_ALLOW_HOMEDIR	    1

typedef krb5_error_code
(*krb5plugin_service_locate_lookup_old) (void *, enum locate_service_type,
				     const char *, int, int,
				     int (*)(void *,int,struct sockaddr *),
				     void *);


typedef struct krb5plugin_service_locate_ftable {
    int			minor_version;
    krb5_error_code	(*init)(krb5_context, void **);
    void		(*fini)(void *);
    krb5plugin_service_locate_lookup_old old_lookup;
    krb5plugin_service_locate_lookup lookup; /* version 2 */
} krb5plugin_service_locate_ftable;

#endif /* HEIMDAL_KRB5_LOCATE_PLUGIN_H */


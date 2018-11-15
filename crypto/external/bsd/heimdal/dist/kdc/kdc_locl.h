/*	$NetBSD: kdc_locl.h,v 1.2 2017/01/28 21:31:44 christos Exp $	*/

/*
 * Copyright (c) 1997-2005 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
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

/*
 * Id
 */

#ifndef __KDC_LOCL_H__
#define __KDC_LOCL_H__

#include "headers.h"

typedef struct pk_client_params pk_client_params;
struct DigestREQ;
struct Kx509Request;
typedef struct kdc_request_desc *kdc_request_t;

#include <kdc-private.h>

#define FAST_EXPIRATION_TIME (3 * 60)

struct kdc_request_desc {
    krb5_context context;
    krb5_kdc_configuration *config;

    /* */

    krb5_data request;
    KDC_REQ req;
    METHOD_DATA *padata;

    /* out */

    METHOD_DATA outpadata;
    
    KDC_REP rep;
    EncTicketPart et;
    EncKDCRepPart ek;

    /* PA methods can affect both the reply key and the session key (pkinit) */
    krb5_enctype sessionetype;
    krb5_keyblock reply_key;
    krb5_keyblock session_key;

    const char *e_text;

    /* state */
    krb5_principal client_princ;
    char *client_name;
    hdb_entry_ex *client;
    HDB *clientdb;

    krb5_principal server_princ;
    char *server_name;
    hdb_entry_ex *server;

    krb5_crypto armor_crypto;

    KDCFastState fast;
};


extern sig_atomic_t exit_flag;
extern size_t max_request_udp;
extern size_t max_request_tcp;
extern const char *request_log;
extern const char *port_str;
extern krb5_addresses explicit_addresses;

extern int enable_http;

extern int detach_from_console;
extern int daemon_child;
extern int do_bonjour;

extern int testing_flag;

extern const struct units _kdc_digestunits[];

#define KDC_LOG_FILE		"kdc.log"

extern struct timeval _kdc_now;
#define kdc_time (_kdc_now.tv_sec)

extern char *runas_string;
extern char *chroot_string;

void
start_kdc(krb5_context context, krb5_kdc_configuration *config, const char *argv0);

krb5_kdc_configuration *
configure(krb5_context context, int argc, char **argv, int *optidx);

#ifdef __APPLE__
void bonjour_announce(krb5_context, krb5_kdc_configuration *);
#endif

#endif /* __KDC_LOCL_H__ */

/*	$NetBSD: salt-aes-sha2.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 1997 - 2008 Kungliga Tekniska Högskolan
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

#include "krb5_locl.h"

int _krb5_AES_SHA2_string_to_default_iterator = 32768;

static krb5_error_code
AES_SHA2_string_to_key(krb5_context context,
		       krb5_enctype enctype,
		       krb5_data password,
		       krb5_salt salt,
		       krb5_data opaque,
		       krb5_keyblock *key)
{
    krb5_error_code ret;
    uint32_t iter;
    struct _krb5_encryption_type *et = NULL;
    struct _krb5_key_data kd;
    krb5_data saltp;
    size_t enctypesz;
    const EVP_MD *md = NULL;

    krb5_data_zero(&saltp);
    kd.key = NULL;
    kd.schedule = NULL;

    if (opaque.length == 0) {
	iter = _krb5_AES_SHA2_string_to_default_iterator;
    } else if (opaque.length == 4) {
	unsigned long v;
	_krb5_get_int(opaque.data, &v, 4);
	iter = ((uint32_t)v);
    } else {
	ret = KRB5_PROG_KEYTYPE_NOSUPP; /* XXX */
	goto cleanup;
    }

    et = _krb5_find_enctype(enctype);
    if (et == NULL) {
	ret = KRB5_PROG_KEYTYPE_NOSUPP;
	goto cleanup;
    }

    kd.schedule = NULL;
    ALLOC(kd.key, 1);
    if (kd.key == NULL) {
	ret = krb5_enomem(context);
	goto cleanup;
    }
    kd.key->keytype = enctype;
    ret = krb5_data_alloc(&kd.key->keyvalue, et->keytype->size);
    if (ret) {
	ret = krb5_enomem(context);
	goto cleanup;
    }

    enctypesz = strlen(et->name) + 1;
    ret = krb5_data_alloc(&saltp, enctypesz + salt.saltvalue.length);
    if (ret) {
	ret = krb5_enomem(context);
	goto cleanup;
    }
    memcpy(saltp.data, et->name, enctypesz);
    memcpy((unsigned char *)saltp.data + enctypesz,
	   salt.saltvalue.data, salt.saltvalue.length);

    ret = _krb5_aes_sha2_md_for_enctype(context, enctype, &md);
    if (ret)
	goto cleanup;

    ret = PKCS5_PBKDF2_HMAC(password.data, password.length,
			    saltp.data, saltp.length,
			    iter, md,
			    et->keytype->size, kd.key->keyvalue.data);
    if (ret != 1) {
	krb5_set_error_message(context, KRB5_PROG_KEYTYPE_NOSUPP,
			       "Error calculating s2k");
	ret = KRB5_PROG_KEYTYPE_NOSUPP;
	goto cleanup;
    }

    ret = _krb5_derive_key(context, et, &kd, "kerberos", strlen("kerberos"));
    if (ret)
	goto cleanup;

    ret = krb5_copy_keyblock_contents(context, kd.key, key);
    if (ret)
	goto cleanup;

cleanup:
    krb5_data_free(&saltp);
    _krb5_free_key_data(context, &kd, et);

    return ret;
}

struct salt_type _krb5_AES_SHA2_salt[] = {
    {
	KRB5_PW_SALT,
	"pw-salt",
	AES_SHA2_string_to_key
    },
    { 0, NULL, NULL }
};

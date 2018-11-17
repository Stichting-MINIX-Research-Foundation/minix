/*	$NetBSD: fast.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 2011 Kungliga Tekniska Högskolan
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


KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_fast_cf2(krb5_context context,
	       krb5_keyblock *key1,
	       const char *pepper1,
	       krb5_keyblock *key2,
	       const char *pepper2,
	       krb5_keyblock *armorkey,
	       krb5_crypto *armor_crypto)
{
    krb5_crypto crypto1, crypto2;
    krb5_data pa1, pa2;
    krb5_error_code ret;

    ret = krb5_crypto_init(context, key1, 0, &crypto1);
    if (ret)
	return ret;

    ret = krb5_crypto_init(context, key2, 0, &crypto2);
    if (ret) {
	krb5_crypto_destroy(context, crypto1);
	return ret;
    }

    pa1.data = rk_UNCONST(pepper1);
    pa1.length = strlen(pepper1);
    pa2.data = rk_UNCONST(pepper2);
    pa2.length = strlen(pepper2);

    ret = krb5_crypto_fx_cf2(context, crypto1, crypto2, &pa1, &pa2,
			     key1->keytype, armorkey);
    krb5_crypto_destroy(context, crypto1);
    krb5_crypto_destroy(context, crypto2);
    if (ret)
	return ret;

    if (armor_crypto) {
	ret = krb5_crypto_init(context, armorkey, 0, armor_crypto);
	if (ret)
	    krb5_free_keyblock_contents(context, armorkey);
    }

    return ret;
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_fast_armor_key(krb5_context context,
		     krb5_keyblock *subkey,
		     krb5_keyblock *sessionkey,
		     krb5_keyblock *armorkey,
		     krb5_crypto *armor_crypto)
{
    return _krb5_fast_cf2(context,
			  subkey,
			  "subkeyarmor",
			  sessionkey,
			  "ticketarmor",
			  armorkey,
			  armor_crypto);
}

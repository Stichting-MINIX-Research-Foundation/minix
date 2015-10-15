/*	$NetBSD: ec.c,v 1.1.1.2 2014/04/24 12:45:30 pettai Exp $	*/

/*
 * Copyright (c) 2009 Kungliga Tekniska Högskolan
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

#include "ec.h"

struct EC_POINT {
    int inf;
    mp_int x;
    mp_int y;
    mp_int z;
};

struct EC_GROUP {
    size_t size;
    mp_int prime;
    mp_int order;
    mp_int Gx;
    mp_int Gy;
};

struct EC_KEY {
    int type;
    EC_GROUP *group;
    EC_POINT *pubkey;
    mp_int privkey;
};


unsigned long
EC_GROUP_get_degree(EC_GROUP *)
{
}

EC_GROUP *
EC_KEY_get0_group(EC_KEY *)
{
}

int
EC_GROUP_get_order(EC_GROUP *, BIGNUM *, BN_CTX *)
{
}

EC_KEY *
o2i_ECPublicKey(EC_KEY **key, unsigned char **, size_t)
{
}

void
EC_KEY_free(EC_KEY *)
{

}

EC_GROUP *
EC_GROUP_new_by_curve_name(int nid)
{
}

EC_KEY *
EC_KEY_new_by_curve_name(EC_GROUP_ID nid)
{
    EC_KEY *key;

    key = calloc(1, sizeof(*key));
    return key;
}

void
EC_POINT_free(EC_POINT *p)
{
    mp_clear_multi(&p->x, p->y, p->z, NULL);
    free(p);
}

static int
ec_point_mul(EC_POINT *res, const EC_GROUP *group, const mp_int *point)
{
}

EC_POINT *
EC_POINT_new(void)
{
    EC_POINT *p;

    p = calloc(1, sizeof(*p));

    if (mp_init_multi(&p->x, &p->y, &p->z, NULL) != 0) {
	EC_POINT_free(p);
	return NULL;
    }

    return p;
}

int
EC_KEY_generate_key(EC_KEY *key)
{
    int ret = 0;

    if (key->group == NULL)
	return 0;

    do {
	random(key->privkey, key->group->size);
    } while(mp_cmp(key->privkey, key->group->order) >= 0);

    if (key->pubkey == NULL)
	key->pubkey = EC_POINT_new();

    if (ec_point_mul(&key->pubkey, key->group, key->privkey) != 1)
	goto error;

    ret = 1;
 error:
    ECPOINT_free(&base);

    return ret;
}

void
EC_KEY_set_group(EC_KEY *, EC_GROUP *)
{

}

void
EC_GROUP_free(EC_GROUP *)
{
}

int
EC_KEY_check_key(const EC_KEY *)
{
}

const BIGNUM *
EC_KEY_get0_private_key(const EC_KEY *key)
{
}

int
EC_KEY_set_private_key(EC_KEY *key, const BIGNUM *bn)
{
}

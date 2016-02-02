/*	$NetBSD: dh-tfm.c,v 1.1.1.2 2014/04/24 12:45:30 pettai Exp $	*/

/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <dh.h>

#include <krb5/roken.h>

#ifdef USE_HCRYPTO_TFM

#include "tfm.h"

static void
BN2mpz(fp_int *s, const BIGNUM *bn)
{
    size_t len;
    void *p;

    len = BN_num_bytes(bn);
    p = malloc(len);
    BN_bn2bin(bn, p);
    fp_read_unsigned_bin(s, p, len);
    free(p);
}


static BIGNUM *
mpz2BN(fp_int *s)
{
    size_t size;
    BIGNUM *bn;
    void *p;

    size = fp_unsigned_bin_size(s);
    p = malloc(size);
    if (p == NULL && size != 0)
	return NULL;
    fp_to_unsigned_bin(s, p);

    bn = BN_bin2bn(p, size, NULL);
    free(p);
    return bn;
}

/*
 *
 */

#define DH_NUM_TRIES 10

static int
tfm_dh_generate_key(DH *dh)
{
    fp_int pub, priv_key, g, p;
    int have_private_key = (dh->priv_key != NULL);
    int codes, times = 0;
    int res;

    if (dh->p == NULL || dh->g == NULL)
	return 0;

    while (times++ < DH_NUM_TRIES) {
	if (!have_private_key) {
	    size_t bits = BN_num_bits(dh->p);

	    if (dh->priv_key)
		BN_free(dh->priv_key);

	    dh->priv_key = BN_new();
	    if (dh->priv_key == NULL)
		return 0;
	    if (!BN_rand(dh->priv_key, bits - 1, 0, 0)) {
		BN_clear_free(dh->priv_key);
		dh->priv_key = NULL;
		return 0;
	    }
	}
	if (dh->pub_key)
	    BN_free(dh->pub_key);

	fp_init_multi(&pub, &priv_key, &g, &p, NULL);

	BN2mpz(&priv_key, dh->priv_key);
	BN2mpz(&g, dh->g);
	BN2mpz(&p, dh->p);

	res = fp_exptmod(&g, &priv_key, &p, &pub);

	fp_zero(&priv_key);
	fp_zero(&g);
	fp_zero(&p);
	if (res != 0)
	    continue;

	dh->pub_key = mpz2BN(&pub);
	fp_zero(&pub);
	if (dh->pub_key == NULL)
	    return 0;

	if (DH_check_pubkey(dh, dh->pub_key, &codes) && codes == 0)
	    break;
	if (have_private_key)
	    return 0;
    }

    if (times >= DH_NUM_TRIES) {
	if (!have_private_key && dh->priv_key) {
	    BN_free(dh->priv_key);
	    dh->priv_key = NULL;
	}
	if (dh->pub_key) {
	    BN_free(dh->pub_key);
	    dh->pub_key = NULL;
	}
	return 0;
    }

    return 1;
}

static int
tfm_dh_compute_key(unsigned char *shared, const BIGNUM * pub, DH *dh)
{
    fp_int s, priv_key, p, peer_pub;
    size_t size = 0;
    int ret;

    if (dh->pub_key == NULL || dh->g == NULL || dh->priv_key == NULL)
	return -1;

    fp_init(&p);
    BN2mpz(&p, dh->p);

    fp_init(&peer_pub);
    BN2mpz(&peer_pub, pub);

    /* check if peers pubkey is reasonable */
    if (fp_isneg(&peer_pub)
	|| fp_cmp(&peer_pub, &p) >= 0
	|| fp_cmp_d(&peer_pub, 1) <= 0)
    {
	fp_zero(&p);
	fp_zero(&peer_pub);
	return -1;
    }

    fp_init(&priv_key);
    BN2mpz(&priv_key, dh->priv_key);

    fp_init(&s);

    ret = fp_exptmod(&peer_pub, &priv_key, &p, &s);

    fp_zero(&p);
    fp_zero(&peer_pub);
    fp_zero(&priv_key);

    if (ret != 0)
	return -1;

    size = fp_unsigned_bin_size(&s);
    fp_to_unsigned_bin(&s, shared);
    fp_zero(&s);

    return size;
}

static int
tfm_dh_generate_params(DH *dh, int a, int b, BN_GENCB *callback)
{
    /* groups should already be known, we don't care about this */
    return 0;
}

static int
tfm_dh_init(DH *dh)
{
    return 1;
}

static int
tfm_dh_finish(DH *dh)
{
    return 1;
}


/*
 *
 */

const DH_METHOD _hc_dh_tfm_method = {
    "hcrypto tfm DH",
    tfm_dh_generate_key,
    tfm_dh_compute_key,
    NULL,
    tfm_dh_init,
    tfm_dh_finish,
    0,
    NULL,
    tfm_dh_generate_params
};

/**
 * DH implementation using tfm.
 *
 * @return the DH_METHOD for the DH implementation using tfm.
 *
 * @ingroup hcrypto_dh
 */

const DH_METHOD *
DH_tfm_method(void)
{
    return &_hc_dh_tfm_method;
}

#endif

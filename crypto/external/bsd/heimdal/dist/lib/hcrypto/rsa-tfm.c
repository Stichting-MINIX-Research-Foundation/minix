/*	$NetBSD: rsa-tfm.c,v 1.1.1.2 2014/04/24 12:45:30 pettai Exp $	*/

/*
 * Copyright (c) 2006 - 2007, 2010 Kungliga Tekniska Högskolan
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
#include <krb5/krb5-types.h>
#include <assert.h>

#include <rsa.h>

#include <krb5/roken.h>

#ifdef USE_HCRYPTO_TFM

#include "tfm.h"

static void
BN2mpz(fp_int *s, const BIGNUM *bn)
{
    size_t len;
    void *p;

    fp_init(s);

    len = BN_num_bytes(bn);
    p = malloc(len);
    BN_bn2bin(bn, p);
    fp_read_unsigned_bin(s, p, len);
    free(p);
}

static int
tfm_rsa_private_calculate(fp_int * in, fp_int * p,  fp_int * q,
			  fp_int * dmp1, fp_int * dmq1, fp_int * iqmp,
			  fp_int * out)
{
    fp_int vp, vq, u;

    fp_init_multi(&vp, &vq, &u, NULL);

    /* vq = c ^ (d mod (q - 1)) mod q */
    /* vp = c ^ (d mod (p - 1)) mod p */
    fp_mod(in, p, &u);
    fp_exptmod(&u, dmp1, p, &vp);
    fp_mod(in, q, &u);
    fp_exptmod(&u, dmq1, q, &vq);

    /* C2 = 1/q mod p  (iqmp) */
    /* u = (vp - vq)C2 mod p. */
    fp_sub(&vp, &vq, &u);
    if (fp_isneg(&u))
	fp_add(&u, p, &u);
    fp_mul(&u, iqmp, &u);
    fp_mod(&u, p, &u);

    /* c ^ d mod n = vq + u q */
    fp_mul(&u, q, &u);
    fp_add(&u, &vq, out);

    fp_zero_multi(&vp, &vq, &u, NULL);

    return 0;
}

/*
 *
 */

static int
tfm_rsa_public_encrypt(int flen, const unsigned char* from,
			unsigned char* to, RSA* rsa, int padding)
{
    unsigned char *p, *p0;
    int res;
    size_t size, padlen;
    fp_int enc, dec, n, e;

    if (padding != RSA_PKCS1_PADDING)
	return -1;

    size = RSA_size(rsa);

    if (size < RSA_PKCS1_PADDING_SIZE || size - RSA_PKCS1_PADDING_SIZE < flen)
	return -2;

    BN2mpz(&n, rsa->n);
    BN2mpz(&e, rsa->e);

    p = p0 = malloc(size - 1);
    if (p0 == NULL) {
	fp_zero_multi(&e, &n, NULL);
	return -3;
    }

    padlen = size - flen - 3;

    *p++ = 2;
    if (RAND_bytes(p, padlen) != 1) {
	fp_zero_multi(&e, &n, NULL);
	free(p0);
	return -4;
    }
    while(padlen) {
	if (*p == 0)
	    *p = 1;
	padlen--;
	p++;
    }
    *p++ = 0;
    memcpy(p, from, flen);
    p += flen;
    assert((p - p0) == size - 1);

    fp_init_multi(&enc, &dec, NULL);
    fp_read_unsigned_bin(&dec, p0, size - 1);
    free(p0);

    res = fp_exptmod(&dec, &e, &n, &enc);

    fp_zero_multi(&dec, &e, &n, NULL);

    if (res != 0)
	return -4;

    {
	size_t ssize;
	ssize = fp_unsigned_bin_size(&enc);
	assert(size >= ssize);
	fp_to_unsigned_bin(&enc, to);
	size = ssize;
    }
    fp_zero(&enc);

    return size;
}

static int
tfm_rsa_public_decrypt(int flen, const unsigned char* from,
		       unsigned char* to, RSA* rsa, int padding)
{
    unsigned char *p;
    int res;
    size_t size;
    fp_int s, us, n, e;

    if (padding != RSA_PKCS1_PADDING)
	return -1;

    if (flen > RSA_size(rsa))
	return -2;

    BN2mpz(&n, rsa->n);
    BN2mpz(&e, rsa->e);

#if 0
    /* Check that the exponent is larger then 3 */
    if (mp_int_compare_value(&e, 3) <= 0) {
	fp_zero_multi(&e, &n, NULL);
	return -3;
    }
#endif

    fp_init_multi(&s, &us, NULL);
    fp_read_unsigned_bin(&s, rk_UNCONST(from), flen);

    if (fp_cmp(&s, &n) >= 0) {
	fp_zero_multi(&e, &n, NULL);
	return -4;
    }

    res = fp_exptmod(&s, &e, &n, &us);

    fp_zero_multi(&s, &e, &n, NULL);

    if (res != 0)
	return -5;
    p = to;


    size = fp_unsigned_bin_size(&us);
    assert(size <= RSA_size(rsa));
    fp_to_unsigned_bin(&us, p);

    fp_zero(&us);

    /* head zero was skipped by fp_to_unsigned_bin */
    if (*p == 0)
	return -6;
    if (*p != 1)
	return -7;
    size--; p++;
    while (size && *p == 0xff) {
	size--; p++;
    }
    if (size == 0 || *p != 0)
	return -8;
    size--; p++;

    memmove(to, p, size);

    return size;
}

static int
tfm_rsa_private_encrypt(int flen, const unsigned char* from,
			unsigned char* to, RSA* rsa, int padding)
{
    unsigned char *p, *p0;
    int res;
    int size;
    fp_int in, out, n, e;

    if (padding != RSA_PKCS1_PADDING)
	return -1;

    size = RSA_size(rsa);

    if (size < RSA_PKCS1_PADDING_SIZE || size - RSA_PKCS1_PADDING_SIZE < flen)
	return -2;

    p0 = p = malloc(size);
    *p++ = 0;
    *p++ = 1;
    memset(p, 0xff, size - flen - 3);
    p += size - flen - 3;
    *p++ = 0;
    memcpy(p, from, flen);
    p += flen;
    assert((p - p0) == size);

    BN2mpz(&n, rsa->n);
    BN2mpz(&e, rsa->e);

    fp_init_multi(&in, &out, NULL);
    fp_read_unsigned_bin(&in, p0, size);
    free(p0);

    if(fp_isneg(&in) || fp_cmp(&in, &n) >= 0) {
	size = -3;
	goto out;
    }

    if (rsa->p && rsa->q && rsa->dmp1 && rsa->dmq1 && rsa->iqmp) {
	fp_int p, q, dmp1, dmq1, iqmp;

	BN2mpz(&p, rsa->p);
	BN2mpz(&q, rsa->q);
	BN2mpz(&dmp1, rsa->dmp1);
	BN2mpz(&dmq1, rsa->dmq1);
	BN2mpz(&iqmp, rsa->iqmp);

	res = tfm_rsa_private_calculate(&in, &p, &q, &dmp1, &dmq1, &iqmp, &out);

	fp_zero_multi(&p, &q, &dmp1, &dmq1, &iqmp, NULL);

	if (res != 0) {
	    size = -4;
	    goto out;
	}
    } else {
	fp_int d;

	BN2mpz(&d, rsa->d);
	res = fp_exptmod(&in, &d, &n, &out);
	fp_zero(&d);
	if (res != 0) {
	    size = -5;
	    goto out;
	}
    }

    if (size > 0) {
	size_t ssize;
	ssize = fp_unsigned_bin_size(&out);
	assert(size >= ssize);
	fp_to_unsigned_bin(&out, to);
	size = ssize;
    }

 out:
    fp_zero_multi(&e, &n, &in, &out, NULL);

    return size;
}

static int
tfm_rsa_private_decrypt(int flen, const unsigned char* from,
			unsigned char* to, RSA* rsa, int padding)
{
    unsigned char *ptr;
    int res;
    int size;
    fp_int in, out, n, e;

    if (padding != RSA_PKCS1_PADDING)
	return -1;

    size = RSA_size(rsa);
    if (flen > size)
	return -2;

    fp_init_multi(&in, &out, NULL);

    BN2mpz(&n, rsa->n);
    BN2mpz(&e, rsa->e);

    fp_read_unsigned_bin(&in, rk_UNCONST(from), flen);

    if(fp_isneg(&in) || fp_cmp(&in, &n) >= 0) {
	size = -2;
	goto out;
    }

    if (rsa->p && rsa->q && rsa->dmp1 && rsa->dmq1 && rsa->iqmp) {
	fp_int p, q, dmp1, dmq1, iqmp;

	BN2mpz(&p, rsa->p);
	BN2mpz(&q, rsa->q);
	BN2mpz(&dmp1, rsa->dmp1);
	BN2mpz(&dmq1, rsa->dmq1);
	BN2mpz(&iqmp, rsa->iqmp);

	res = tfm_rsa_private_calculate(&in, &p, &q, &dmp1, &dmq1, &iqmp, &out);

	fp_zero_multi(&p, &q, &dmp1, &dmq1, &iqmp, NULL);

	if (res != 0) {
	    size = -3;
	    goto out;
	}

    } else {
	fp_int d;

	if(fp_isneg(&in) || fp_cmp(&in, &n) >= 0)
	    return -4;

	BN2mpz(&d, rsa->d);
	res = fp_exptmod(&in, &d, &n, &out);
	fp_zero(&d);
	if (res != 0) {
	    size = -5;
	    goto out;
	}
    }

    ptr = to;
    {
	size_t ssize;
	ssize = fp_unsigned_bin_size(&out);
	assert(size >= ssize);
	fp_to_unsigned_bin(&out, ptr);
	size = ssize;
    }

    /* head zero was skipped by mp_int_to_unsigned */
    if (*ptr != 2) {
	size = -6;
	goto out;
    }
    size--; ptr++;
    while (size && *ptr != 0) {
	size--; ptr++;
    }
    if (size == 0)
	return -7;
    size--; ptr++;

    memmove(to, ptr, size);

 out:
    fp_zero_multi(&e, &n, &in, &out, NULL);

    return size;
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

static int
random_num(fp_int *num, size_t len)
{
    unsigned char *p;

    len = (len + 7) / 8;
    p = malloc(len);
    if (p == NULL)
	return 1;
    if (RAND_bytes(p, len) != 1) {
	free(p);
	return 1;
    }
    fp_read_unsigned_bin(num, p, len);
    free(p);
    return 0;
}

#define CHECK(f, v) if ((f) != (v)) { goto out; }

static int
tfm_rsa_generate_key(RSA *rsa, int bits, BIGNUM *e, BN_GENCB *cb)
{
    fp_int el, p, q, n, d, dmp1, dmq1, iqmp, t1, t2, t3;
    int counter, ret, bitsp;

    if (bits < 789)
	return -1;

    bitsp = (bits + 1) / 2;

    ret = -1;

    fp_init_multi(&el, &p, &q, &n, &n, &d, &dmp1, &dmq1, &iqmp, &t1, &t2, &t3, NULL);

    BN2mpz(&el, e);

    /* generate p and q so that p != q and bits(pq) ~ bits */
    counter = 0;
    do {
	BN_GENCB_call(cb, 2, counter++);
	CHECK(random_num(&p, bitsp), 0);
	CHECK(fp_find_prime(&p), FP_YES);

	fp_sub_d(&p, 1, &t1);
	fp_gcd(&t1, &el, &t2);
    } while(fp_cmp_d(&t2, 1) != 0);

    BN_GENCB_call(cb, 3, 0);

    counter = 0;
    do {
	BN_GENCB_call(cb, 2, counter++);
	CHECK(random_num(&q, bits - bitsp), 0);
	CHECK(fp_find_prime(&q), FP_YES);

	if (fp_cmp(&p, &q) == 0) /* don't let p and q be the same */
	    continue;

	fp_sub_d(&q, 1, &t1);
	fp_gcd(&t1, &el, &t2);
    } while(fp_cmp_d(&t2, 1) != 0);

    /* make p > q */
    if (fp_cmp(&p, &q) < 0) {
	fp_int c;
	fp_copy(&p, &c);
	fp_copy(&q, &p);
	fp_copy(&c, &q);
    }

    BN_GENCB_call(cb, 3, 1);

    /* calculate n,  		n = p * q */
    fp_mul(&p, &q, &n);

    /* calculate d, 		d = 1/e mod (p - 1)(q - 1) */
    fp_sub_d(&p, 1, &t1);
    fp_sub_d(&q, 1, &t2);
    fp_mul(&t1, &t2, &t3);
    fp_invmod(&el, &t3, &d);

    /* calculate dmp1		dmp1 = d mod (p-1) */
    fp_mod(&d, &t1, &dmp1);
    /* calculate dmq1		dmq1 = d mod (q-1) */
    fp_mod(&d, &t2, &dmq1);
    /* calculate iqmp 		iqmp = 1/q mod p */
    fp_invmod(&q, &p, &iqmp);

    /* fill in RSA key */

    rsa->e = mpz2BN(&el);
    rsa->p = mpz2BN(&p);
    rsa->q = mpz2BN(&q);
    rsa->n = mpz2BN(&n);
    rsa->d = mpz2BN(&d);
    rsa->dmp1 = mpz2BN(&dmp1);
    rsa->dmq1 = mpz2BN(&dmq1);
    rsa->iqmp = mpz2BN(&iqmp);

    ret = 1;

out:
    fp_zero_multi(&el, &p, &q, &n, &d, &dmp1,
		  &dmq1, &iqmp, &t1, &t2, &t3, NULL);

    return ret;
}

static int
tfm_rsa_init(RSA *rsa)
{
    return 1;
}

static int
tfm_rsa_finish(RSA *rsa)
{
    return 1;
}

const RSA_METHOD hc_rsa_tfm_method = {
    "hcrypto tfm RSA",
    tfm_rsa_public_encrypt,
    tfm_rsa_public_decrypt,
    tfm_rsa_private_encrypt,
    tfm_rsa_private_decrypt,
    NULL,
    NULL,
    tfm_rsa_init,
    tfm_rsa_finish,
    0,
    NULL,
    NULL,
    NULL,
    tfm_rsa_generate_key
};

#endif

const RSA_METHOD *
RSA_tfm_method(void)
{
#ifdef USE_HCRYPTO_TFM
    return &hc_rsa_tfm_method;
#else
    return NULL;
#endif
}


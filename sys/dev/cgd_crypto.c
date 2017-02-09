/* $NetBSD: cgd_crypto.c,v 1.13 2015/04/25 12:55:04 riastradh Exp $ */

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Roland C. Dowdeswell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  Crypto Framework For cgd.c
 *
 *	This framework is temporary and awaits a more complete
 *	kernel wide crypto implementation.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cgd_crypto.c,v 1.13 2015/04/25 12:55:04 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <dev/cgd_crypto.h>

#include <crypto/rijndael/rijndael-api-fst.h>
#include <crypto/des/des.h>
#include <crypto/blowfish/blowfish.h>

#ifdef DIAGNOSTIC
#define DIAGPANIC(x)	panic x
#else
#define DIAGPANIC(x)
#endif

/*
 * The general framework provides only one generic function.
 * It takes the name of an algorith and returns a struct cryptfuncs *
 * for it.  It is up to the initialisation routines of the algorithm
 * to check key size and block size.
 */

static cfunc_init	cgd_cipher_aes_init;
static cfunc_destroy	cgd_cipher_aes_destroy;
static cfunc_cipher	cgd_cipher_aes_cbc;

static cfunc_init	cgd_cipher_3des_init;
static cfunc_destroy	cgd_cipher_3des_destroy;
static cfunc_cipher	cgd_cipher_3des_cbc;

static cfunc_init	cgd_cipher_bf_init;
static cfunc_destroy	cgd_cipher_bf_destroy;
static cfunc_cipher	cgd_cipher_bf_cbc;

static const struct cryptfuncs cf[] = {
	{
		.cf_name	= "aes-cbc",
		.cf_init	= cgd_cipher_aes_init,
		.cf_destroy	= cgd_cipher_aes_destroy,
		.cf_cipher	= cgd_cipher_aes_cbc,
	},
	{
		.cf_name	= "3des-cbc",
		.cf_init	= cgd_cipher_3des_init,
		.cf_destroy	= cgd_cipher_3des_destroy,
		.cf_cipher	= cgd_cipher_3des_cbc,
	},
	{
		.cf_name	= "blowfish-cbc",
		.cf_init	= cgd_cipher_bf_init,
		.cf_destroy	= cgd_cipher_bf_destroy,
		.cf_cipher	= cgd_cipher_bf_cbc,
	},
};
const struct cryptfuncs *
cryptfuncs_find(const char *alg)
{

	for (size_t i = 0; i < __arraycount(cf); i++)
		if (strcmp(cf[i].cf_name, alg) == 0)
			return &cf[i];

	return NULL;
}

typedef void	(*cipher_func)(void *, void *, const void *, size_t);

static void
cgd_cipher_uio_cbc(void *privdata, cipher_func cipher,
	struct uio *dstuio, struct uio *srcuio);

/*
 * cgd_cipher_uio_cbc takes a simple cbc cipher and iterates
 * it over two struct uio's.  It presumes that the cipher function
 * that is passed to it keeps the IV state between calls.
 *
 * We assume that the caller has ensured that each segment is evenly
 * divisible by the block size, which for the cgd is a valid assumption.
 * If we were to make this code more generic, we might need to take care
 * of this case, either by issuing an error or copying the data.
 */

static void
cgd_cipher_uio_cbc(void *privdata, cipher_func cipher,
    struct uio *dstuio, struct uio *srcuio)
{
	const struct iovec	*dst;
	const struct iovec	*src;
	int		 dstnum;
	int		 dstoff = 0;
	int		 srcnum;
	int		 srcoff = 0;

	dst = dstuio->uio_iov;
	dstnum = dstuio->uio_iovcnt;
	src = srcuio->uio_iov;
	srcnum = srcuio->uio_iovcnt;
	for (;;) {
		int	  l = MIN(dst->iov_len - dstoff, src->iov_len - srcoff);
		u_int8_t *d = (u_int8_t *)dst->iov_base + dstoff;
		const u_int8_t *s = (const u_int8_t *)src->iov_base + srcoff;

		cipher(privdata, d, s, l);

		dstoff += l;
		srcoff += l;
		/*
		 * We assume that {dst,src} == {dst,src}->iov_len,
		 * because it should not be possible for it not to be.
		 */
		if (dstoff == dst->iov_len) {
			dstoff = 0;
			dstnum--;
			dst++;
		}
		if (srcoff == src->iov_len) {
			srcoff = 0;
			srcnum--;
			src++;
		}
		if (!srcnum || !dstnum)
			break;
	}
}

/*
 *  AES Framework
 */

/*
 * NOTE: we do not store the blocksize in here, because it is not
 *       variable [yet], we hardcode the blocksize to 16 (128 bits).
 */

struct aes_privdata {
	keyInstance	ap_enckey;
	keyInstance	ap_deckey;
};

struct aes_encdata {
	keyInstance	*ae_key;	/* key for this direction */
	u_int8_t	 ae_iv[16];	/* Initialization Vector */
};

static void *
cgd_cipher_aes_init(size_t keylen, const void *key, size_t *blocksize)
{
	struct	aes_privdata *ap;

	if (!blocksize)
		return NULL;
	if (keylen != 128 && keylen != 192 && keylen != 256)
		return NULL;
	if (*blocksize == (size_t)-1)
		*blocksize = 128;
	if (*blocksize != 128)
		return NULL;
	ap = malloc(sizeof(*ap), M_DEVBUF, 0);
	if (!ap)
		return NULL;
	rijndael_makeKey(&ap->ap_enckey, DIR_ENCRYPT, keylen, key);
	rijndael_makeKey(&ap->ap_deckey, DIR_DECRYPT, keylen, key);
	return ap;
}

static void
cgd_cipher_aes_destroy(void *data)
{
	struct aes_privdata *apd = data;

	explicit_memset(apd, 0, sizeof(*apd));
	free(apd, M_DEVBUF);
}

static void
aes_cbc_enc_int(void *privdata, void *dst, const void *src, size_t len)
{
	struct aes_encdata	*ae = privdata;
	cipherInstance		 cipher;

	rijndael_cipherInit(&cipher, MODE_CBC, ae->ae_iv);
	rijndael_blockEncrypt(&cipher, ae->ae_key, src, len * 8, dst);
	(void)memcpy(ae->ae_iv, (u_int8_t *)dst + (len - 16), 16);
}

static void
aes_cbc_dec_int(void *privdata, void *dst, const void *src, size_t len)
{
	struct aes_encdata	*ae = privdata;
	cipherInstance		 cipher;

	rijndael_cipherInit(&cipher, MODE_CBC, ae->ae_iv);
	rijndael_blockDecrypt(&cipher, ae->ae_key, src, len * 8, dst);
	(void)memcpy(ae->ae_iv, (const u_int8_t *)src + (len - 16), 16);
}

static void
cgd_cipher_aes_cbc(void *privdata, struct uio *dstuio,
    struct uio *srcuio, const void *iv, int dir)
{
	struct aes_privdata	*apd = privdata;
	struct aes_encdata	 encd;

	(void)memcpy(encd.ae_iv, iv, 16);
	switch (dir) {
	case CGD_CIPHER_ENCRYPT:
		encd.ae_key = &apd->ap_enckey;
		cgd_cipher_uio_cbc(&encd, aes_cbc_enc_int, dstuio, srcuio);
		break;
	case CGD_CIPHER_DECRYPT:
		encd.ae_key = &apd->ap_deckey;
		cgd_cipher_uio_cbc(&encd, aes_cbc_dec_int, dstuio, srcuio);
		break;
	default:
		DIAGPANIC(("%s: unrecognised direction %d", __func__, dir));
	}
}

/*
 * 3DES Framework
 */

struct c3des_privdata {
	des_key_schedule	cp_key1;
	des_key_schedule	cp_key2;
	des_key_schedule	cp_key3;
};

struct c3des_encdata {
	des_key_schedule	*ce_key1;
	des_key_schedule	*ce_key2;
	des_key_schedule	*ce_key3;
	u_int8_t		ce_iv[8];
};

static void *
cgd_cipher_3des_init(size_t keylen, const void *key, size_t *blocksize)
{
	struct	c3des_privdata *cp;
	int	error = 0;
	des_cblock *block;

	if (!blocksize)
		return NULL;
	if (*blocksize == (size_t)-1)
		*blocksize = 64;
	if (keylen != (DES_KEY_SZ * 3 * 8) || *blocksize != 64)
		return NULL;
	cp = malloc(sizeof(*cp), M_DEVBUF, 0);
	if (!cp)
		return NULL;
	block = __UNCONST(key);
	error  = des_key_sched(block, cp->cp_key1);
	error |= des_key_sched(block + 1, cp->cp_key2);
	error |= des_key_sched(block + 2, cp->cp_key3);
	if (error) {
		explicit_memset(cp, 0, sizeof(*cp));
		free(cp, M_DEVBUF);
		return NULL;
	}
	return cp;
}

static void
cgd_cipher_3des_destroy(void *data)
{
	struct c3des_privdata *cp = data;

	explicit_memset(cp, 0, sizeof(*cp));
	free(cp, M_DEVBUF);
}

static void
c3des_cbc_enc_int(void *privdata, void *dst, const void *src, size_t len)
{
	struct	c3des_encdata *ce = privdata;

	des_ede3_cbc_encrypt(src, dst, len, *ce->ce_key1, *ce->ce_key2,
	    *ce->ce_key3, (des_cblock *)ce->ce_iv, 1);
	(void)memcpy(ce->ce_iv, (const u_int8_t *)dst + (len - 8), 8);
}

static void
c3des_cbc_dec_int(void *privdata, void *dst, const void *src, size_t len)
{
	struct	c3des_encdata *ce = privdata;

	des_ede3_cbc_encrypt(src, dst, len, *ce->ce_key1, *ce->ce_key2,
	    *ce->ce_key3, (des_cblock *)ce->ce_iv, 0);
	(void)memcpy(ce->ce_iv, (const u_int8_t *)src + (len - 8), 8);
}

static void
cgd_cipher_3des_cbc(void *privdata, struct uio *dstuio,
	struct uio *srcuio, const void *iv, int dir)
{
	struct	c3des_privdata *cp = privdata;
	struct	c3des_encdata ce;

	(void)memcpy(ce.ce_iv, iv, 8);
	ce.ce_key1 = &cp->cp_key1;
	ce.ce_key2 = &cp->cp_key2;
	ce.ce_key3 = &cp->cp_key3;
	switch (dir) {
	case CGD_CIPHER_ENCRYPT:
		cgd_cipher_uio_cbc(&ce, c3des_cbc_enc_int, dstuio, srcuio);
		break;
	case CGD_CIPHER_DECRYPT:
		cgd_cipher_uio_cbc(&ce, c3des_cbc_dec_int, dstuio, srcuio);
		break;
	default:
		DIAGPANIC(("%s: unrecognised direction %d", __func__, dir));
	}
}

/*
 * Blowfish Framework
 */

struct bf_privdata {
	BF_KEY	bp_key;
};

struct bf_encdata {
	BF_KEY		*be_key;
	u_int8_t	 be_iv[8];
};

static void *
cgd_cipher_bf_init(size_t keylen, const void *key, size_t *blocksize)
{
	struct	bf_privdata *bp;

	if (!blocksize)
		return NULL;
	if (keylen < 40 || keylen > 448 || (keylen % 8 != 0))
		return NULL;
	if (*blocksize == (size_t)-1)
		*blocksize = 64;
	if (*blocksize != 64)
		return NULL;
	bp = malloc(sizeof(*bp), M_DEVBUF, 0);
	if (!bp)
		return NULL;
	BF_set_key(&bp->bp_key, keylen / 8, key);
	return bp;
}

static void
cgd_cipher_bf_destroy(void *data)
{
	struct	bf_privdata *bp = data;

	explicit_memset(bp, 0, sizeof(*bp));
	free(bp, M_DEVBUF);
}

static void
bf_cbc_enc_int(void *privdata, void *dst, const void *src, size_t len)
{
	struct	bf_encdata *be = privdata;

	BF_cbc_encrypt(src, dst, len, be->be_key, be->be_iv, 1);
	(void)memcpy(be->be_iv, (u_int8_t *)dst + (len - 8), 8);
}

static void
bf_cbc_dec_int(void *privdata, void *dst, const void *src, size_t len)
{
	struct	bf_encdata *be = privdata;

	BF_cbc_encrypt(src, dst, len, be->be_key, be->be_iv, 0);
	(void)memcpy(be->be_iv, (const u_int8_t *)src + (len - 8), 8);
}

static void
cgd_cipher_bf_cbc(void *privdata, struct uio *dstuio,
    struct uio *srcuio, const void *iv, int dir)
{
	struct	bf_privdata *bp = privdata;
	struct	bf_encdata be;

	(void)memcpy(be.be_iv, iv, 8);
	be.be_key = &bp->bp_key;
	switch (dir) {
	case CGD_CIPHER_ENCRYPT:
		cgd_cipher_uio_cbc(&be, bf_cbc_enc_int, dstuio, srcuio);
		break;
	case CGD_CIPHER_DECRYPT:
		cgd_cipher_uio_cbc(&be, bf_cbc_dec_int, dstuio, srcuio);
		break;
	default:
		DIAGPANIC(("%s: unrecognised direction %d", __func__, dir));
	}

}

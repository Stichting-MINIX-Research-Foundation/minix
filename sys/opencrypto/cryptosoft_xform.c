/*	$NetBSD: cryptosoft_xform.c,v 1.27 2014/11/27 20:30:21 christos Exp $ */
/*	$FreeBSD: src/sys/opencrypto/xform.c,v 1.1.2.1 2002/11/21 23:34:23 sam Exp $	*/
/*	$OpenBSD: xform.c,v 1.19 2002/08/16 22:47:25 dhartmei Exp $	*/

/*
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr) and
 * Niels Provos (provos@physnet.uni-hamburg.de).
 *
 * This code was written by John Ioannidis for BSD/OS in Athens, Greece,
 * in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis.
 *
 * Additional transforms and features in 1997 and 1998 by Angelos D. Keromytis
 * and Niels Provos.
 *
 * Additional features in 1999 by Angelos D. Keromytis.
 *
 * Copyright (C) 1995, 1996, 1997, 1998, 1999 by John Ioannidis,
 * Angelos D. Keromytis and Niels Provos.
 *
 * Copyright (C) 2001, Angelos D. Keromytis.
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 * You may use this code under the GNU public license if you so wish. Please
 * contribute changes back to the authors under this freer than GPL license
 * so that we may further the use of strong encryption without limitations to
 * all.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(1, "$NetBSD: cryptosoft_xform.c,v 1.27 2014/11/27 20:30:21 christos Exp $");

#include <crypto/blowfish/blowfish.h>
#include <crypto/cast128/cast128.h>
#include <crypto/des/des.h>
#include <crypto/rijndael/rijndael.h>
#include <crypto/skipjack/skipjack.h>
#include <crypto/camellia/camellia.h>

#include <opencrypto/deflate.h>

#include <sys/md5.h>
#include <sys/rmd160.h>
#include <sys/sha1.h>
#include <sys/sha2.h>
#include <sys/cprng.h>
#include <opencrypto/aesxcbcmac.h>
#include <opencrypto/gmac.h>

struct swcr_auth_hash {
	const struct auth_hash *auth_hash;
	int ctxsize;
	void (*Init)(void *);
	void (*Setkey)(void *, const uint8_t *, uint16_t);
	void (*Reinit)(void *, const uint8_t *, uint16_t);
	int  (*Update)(void *, const uint8_t *, uint16_t);
	void (*Final)(uint8_t *, void *);
};

struct swcr_enc_xform {
	const struct enc_xform *enc_xform;
	void (*encrypt)(void *, uint8_t *);
	void (*decrypt)(void *, uint8_t *);
	int  (*setkey)(uint8_t **, const uint8_t *, int);
	void (*zerokey)(uint8_t **);
	void (*reinit)(void *, const uint8_t *, uint8_t *);
};

struct swcr_comp_algo {
	const struct comp_algo *unused_comp_algo;
	uint32_t (*compress)(uint8_t *, uint32_t, uint8_t **);
	uint32_t (*decompress)(uint8_t *, uint32_t, uint8_t **, int);
};

static void null_encrypt(void *, u_int8_t *);
static void null_decrypt(void *, u_int8_t *);
static int null_setkey(u_int8_t **, const u_int8_t *, int);
static void null_zerokey(u_int8_t **);

static	int des1_setkey(u_int8_t **, const u_int8_t *, int);
static	int des3_setkey(u_int8_t **, const u_int8_t *, int);
static	int blf_setkey(u_int8_t **, const u_int8_t *, int);
static	int cast5_setkey(u_int8_t **, const u_int8_t *, int);
static  int skipjack_setkey(u_int8_t **, const u_int8_t *, int);
static  int rijndael128_setkey(u_int8_t **, const u_int8_t *, int);
static  int cml_setkey(u_int8_t **, const u_int8_t *, int);
static  int aes_ctr_setkey(u_int8_t **, const u_int8_t *, int);
static	int aes_gmac_setkey(u_int8_t **, const u_int8_t *, int);
static	void des1_encrypt(void *, u_int8_t *);
static	void des3_encrypt(void *, u_int8_t *);
static	void blf_encrypt(void *, u_int8_t *);
static	void cast5_encrypt(void *, u_int8_t *);
static	void skipjack_encrypt(void *, u_int8_t *);
static	void rijndael128_encrypt(void *, u_int8_t *);
static  void cml_encrypt(void *, u_int8_t *);
static	void des1_decrypt(void *, u_int8_t *);
static	void des3_decrypt(void *, u_int8_t *);
static	void blf_decrypt(void *, u_int8_t *);
static	void cast5_decrypt(void *, u_int8_t *);
static	void skipjack_decrypt(void *, u_int8_t *);
static	void rijndael128_decrypt(void *, u_int8_t *);
static  void cml_decrypt(void *, u_int8_t *);
static  void aes_ctr_crypt(void *, u_int8_t *);
static	void des1_zerokey(u_int8_t **);
static	void des3_zerokey(u_int8_t **);
static	void blf_zerokey(u_int8_t **);
static	void cast5_zerokey(u_int8_t **);
static	void skipjack_zerokey(u_int8_t **);
static	void rijndael128_zerokey(u_int8_t **);
static  void cml_zerokey(u_int8_t **);
static  void aes_ctr_zerokey(u_int8_t **);
static	void aes_gmac_zerokey(u_int8_t **);
static  void aes_ctr_reinit(void *, const u_int8_t *, u_int8_t *);
static  void aes_gcm_reinit(void *, const u_int8_t *, u_int8_t *);
static	void aes_gmac_reinit(void *, const u_int8_t *, u_int8_t *);

static	void null_init(void *);
static	int null_update(void *, const u_int8_t *, u_int16_t);
static	void null_final(u_int8_t *, void *);

static int	MD5Update_int(void *, const u_int8_t *, u_int16_t);
static void	SHA1Init_int(void *);
static	int SHA1Update_int(void *, const u_int8_t *, u_int16_t);
static	void SHA1Final_int(u_int8_t *, void *);


static int RMD160Update_int(void *, const u_int8_t *, u_int16_t);
static	int SHA1Update_int(void *, const u_int8_t *, u_int16_t);
static	void SHA1Final_int(u_int8_t *, void *);
static	int RMD160Update_int(void *, const u_int8_t *, u_int16_t);
static	int SHA256Update_int(void *, const u_int8_t *, u_int16_t);
static	int SHA384Update_int(void *, const u_int8_t *, u_int16_t);
static	int SHA512Update_int(void *, const u_int8_t *, u_int16_t);

static u_int32_t deflate_compress(u_int8_t *, u_int32_t, u_int8_t **);
static u_int32_t deflate_decompress(u_int8_t *, u_int32_t, u_int8_t **, int);
static u_int32_t gzip_compress(u_int8_t *, u_int32_t, u_int8_t **);
static u_int32_t gzip_decompress(u_int8_t *, u_int32_t, u_int8_t **, int);

/* Encryption instances */
static const struct swcr_enc_xform swcr_enc_xform_null = {
	&enc_xform_null,
	null_encrypt,
	null_decrypt,
	null_setkey,
	null_zerokey,
	NULL
};

static const struct swcr_enc_xform swcr_enc_xform_des = {
	&enc_xform_des,
	des1_encrypt,
	des1_decrypt,
	des1_setkey,
	des1_zerokey,
	NULL
};

static const struct swcr_enc_xform swcr_enc_xform_3des = {
	&enc_xform_3des,
	des3_encrypt,
	des3_decrypt,
	des3_setkey,
	des3_zerokey,
	NULL
};

static const struct swcr_enc_xform swcr_enc_xform_blf = {
	&enc_xform_blf,
	blf_encrypt,
	blf_decrypt,
	blf_setkey,
	blf_zerokey,
	NULL
};

static const struct swcr_enc_xform swcr_enc_xform_cast5 = {
	&enc_xform_cast5,
	cast5_encrypt,
	cast5_decrypt,
	cast5_setkey,
	cast5_zerokey,
	NULL
};

static const struct swcr_enc_xform swcr_enc_xform_skipjack = {
	&enc_xform_skipjack,
	skipjack_encrypt,
	skipjack_decrypt,
	skipjack_setkey,
	skipjack_zerokey,
	NULL
};

static const struct swcr_enc_xform swcr_enc_xform_rijndael128 = {
	&enc_xform_rijndael128,
	rijndael128_encrypt,
	rijndael128_decrypt,
	rijndael128_setkey,
	rijndael128_zerokey,
	NULL
};

static const struct swcr_enc_xform swcr_enc_xform_aes_ctr = {
	&enc_xform_aes_ctr,
	aes_ctr_crypt,
	aes_ctr_crypt,
	aes_ctr_setkey,
	aes_ctr_zerokey,
	aes_ctr_reinit
};

static const struct swcr_enc_xform swcr_enc_xform_aes_gcm = {
	&enc_xform_aes_gcm,
	aes_ctr_crypt,
	aes_ctr_crypt,
	aes_ctr_setkey,
	aes_ctr_zerokey,
	aes_gcm_reinit
};

static const struct swcr_enc_xform swcr_enc_xform_aes_gmac = {
	&enc_xform_aes_gmac,
	NULL,
	NULL,
	aes_gmac_setkey,
	aes_gmac_zerokey,
	aes_gmac_reinit
};

static const struct swcr_enc_xform swcr_enc_xform_camellia = {
	&enc_xform_camellia,
	cml_encrypt,
	cml_decrypt,
	cml_setkey,
	cml_zerokey,
	NULL
};

/* Authentication instances */
static const struct swcr_auth_hash swcr_auth_hash_null = {
	&auth_hash_null, sizeof(int), /* NB: context isn't used */
	null_init, NULL, NULL, null_update, null_final
};

static const struct swcr_auth_hash swcr_auth_hash_hmac_md5 = {
	&auth_hash_hmac_md5, sizeof(MD5_CTX),
	(void (*) (void *)) MD5Init, NULL, NULL, MD5Update_int,
	(void (*) (u_int8_t *, void *)) MD5Final
};

static const struct swcr_auth_hash swcr_auth_hash_hmac_sha1 = {
	&auth_hash_hmac_sha1, sizeof(SHA1_CTX),
	SHA1Init_int, NULL, NULL, SHA1Update_int, SHA1Final_int
};

static const struct swcr_auth_hash swcr_auth_hash_hmac_ripemd_160 = {
	&auth_hash_hmac_ripemd_160, sizeof(RMD160_CTX),
	(void (*)(void *)) RMD160Init, NULL, NULL, RMD160Update_int,
	(void (*)(u_int8_t *, void *)) RMD160Final
};
static const struct swcr_auth_hash swcr_auth_hash_hmac_md5_96 = {
	&auth_hash_hmac_md5_96, sizeof(MD5_CTX),
	(void (*) (void *)) MD5Init, NULL, NULL, MD5Update_int,
	(void (*) (u_int8_t *, void *)) MD5Final
};

static const struct swcr_auth_hash swcr_auth_hash_hmac_sha1_96 = {
	&auth_hash_hmac_sha1_96, sizeof(SHA1_CTX),
	SHA1Init_int, NULL, NULL, SHA1Update_int, SHA1Final_int
};

static const struct swcr_auth_hash swcr_auth_hash_hmac_ripemd_160_96 = {
	&auth_hash_hmac_ripemd_160_96, sizeof(RMD160_CTX),
	(void (*)(void *)) RMD160Init, NULL, NULL, RMD160Update_int,
	(void (*)(u_int8_t *, void *)) RMD160Final
};

static const struct swcr_auth_hash swcr_auth_hash_key_md5 = {
	&auth_hash_key_md5, sizeof(MD5_CTX),
	(void (*)(void *)) MD5Init, NULL, NULL, MD5Update_int,
	(void (*)(u_int8_t *, void *)) MD5Final
};

static const struct swcr_auth_hash swcr_auth_hash_key_sha1 = {
	&auth_hash_key_sha1, sizeof(SHA1_CTX),
	SHA1Init_int, NULL, NULL, SHA1Update_int, SHA1Final_int
};

static const struct swcr_auth_hash swcr_auth_hash_md5 = {
	&auth_hash_md5, sizeof(MD5_CTX),
	(void (*) (void *)) MD5Init, NULL, NULL, MD5Update_int,
	(void (*) (u_int8_t *, void *)) MD5Final
};

static const struct swcr_auth_hash swcr_auth_hash_sha1 = {
	&auth_hash_sha1, sizeof(SHA1_CTX),
	(void (*)(void *)) SHA1Init, NULL, NULL, SHA1Update_int,
	(void (*)(u_int8_t *, void *)) SHA1Final
};

static const struct swcr_auth_hash swcr_auth_hash_hmac_sha2_256 = {
	&auth_hash_hmac_sha2_256, sizeof(SHA256_CTX),
	(void (*)(void *)) SHA256_Init, NULL, NULL, SHA256Update_int,
	(void (*)(u_int8_t *, void *)) SHA256_Final
};

static const struct swcr_auth_hash swcr_auth_hash_hmac_sha2_384 = {
	&auth_hash_hmac_sha2_384, sizeof(SHA384_CTX),
	(void (*)(void *)) SHA384_Init, NULL, NULL, SHA384Update_int,
	(void (*)(u_int8_t *, void *)) SHA384_Final
};

static const struct swcr_auth_hash swcr_auth_hash_hmac_sha2_512 = {
	&auth_hash_hmac_sha2_512, sizeof(SHA512_CTX),
	(void (*)(void *)) SHA512_Init, NULL, NULL, SHA512Update_int,
	(void (*)(u_int8_t *, void *)) SHA512_Final
};

static const struct swcr_auth_hash swcr_auth_hash_aes_xcbc_mac = {
	&auth_hash_aes_xcbc_mac_96, sizeof(aesxcbc_ctx),
	null_init,
	(void (*)(void *, const u_int8_t *, u_int16_t))aes_xcbc_mac_init,
	NULL, aes_xcbc_mac_loop, aes_xcbc_mac_result
};

static const struct swcr_auth_hash swcr_auth_hash_gmac_aes_128 = {
	&auth_hash_gmac_aes_128, sizeof(AES_GMAC_CTX),
	(void (*)(void *))AES_GMAC_Init,
	(void (*)(void *, const u_int8_t *, u_int16_t))AES_GMAC_Setkey,
	(void (*)(void *, const u_int8_t *, u_int16_t))AES_GMAC_Reinit,
	(int (*)(void *, const u_int8_t *, u_int16_t))AES_GMAC_Update,
	(void (*)(u_int8_t *, void *))AES_GMAC_Final
};

static const struct swcr_auth_hash swcr_auth_hash_gmac_aes_192 = {
	&auth_hash_gmac_aes_192, sizeof(AES_GMAC_CTX),
	(void (*)(void *))AES_GMAC_Init,
	(void (*)(void *, const u_int8_t *, u_int16_t))AES_GMAC_Setkey,
	(void (*)(void *, const u_int8_t *, u_int16_t))AES_GMAC_Reinit,
	(int (*)(void *, const u_int8_t *, u_int16_t))AES_GMAC_Update,
	(void (*)(u_int8_t *, void *))AES_GMAC_Final
};

static const struct swcr_auth_hash swcr_auth_hash_gmac_aes_256 = {
	&auth_hash_gmac_aes_256, sizeof(AES_GMAC_CTX),
	(void (*)(void *))AES_GMAC_Init,
	(void (*)(void *, const u_int8_t *, u_int16_t))AES_GMAC_Setkey,
	(void (*)(void *, const u_int8_t *, u_int16_t))AES_GMAC_Reinit,
	(int (*)(void *, const u_int8_t *, u_int16_t))AES_GMAC_Update,
	(void (*)(u_int8_t *, void *))AES_GMAC_Final
};

/* Compression instance */
static const struct swcr_comp_algo swcr_comp_algo_deflate = {
	&comp_algo_deflate,
	deflate_compress,
	deflate_decompress
};

static const struct swcr_comp_algo swcr_comp_algo_deflate_nogrow = {
	&comp_algo_deflate_nogrow,
	deflate_compress,
	deflate_decompress
};

static const struct swcr_comp_algo swcr_comp_algo_gzip = {
	&comp_algo_deflate,
	gzip_compress,
	gzip_decompress
};

/*
 * Encryption wrapper routines.
 */
static void
null_encrypt(void *key, u_int8_t *blk)
{
}
static void
null_decrypt(void *key, u_int8_t *blk)
{
}
static int
null_setkey(u_int8_t **sched, const u_int8_t *key, int len)
{
	*sched = NULL;
	return 0;
}
static void
null_zerokey(u_int8_t **sched)
{
	*sched = NULL;
}

static void
des1_encrypt(void *key, u_int8_t *blk)
{
	des_cblock *cb = (des_cblock *) blk;
	des_key_schedule *p = (des_key_schedule *) key;

	des_ecb_encrypt(cb, cb, p[0], DES_ENCRYPT);
}

static void
des1_decrypt(void *key, u_int8_t *blk)
{
	des_cblock *cb = (des_cblock *) blk;
	des_key_schedule *p = (des_key_schedule *) key;

	des_ecb_encrypt(cb, cb, p[0], DES_DECRYPT);
}

static int
des1_setkey(u_int8_t **sched, const u_int8_t *key, int len)
{
	des_key_schedule *p;

	p = malloc(sizeof (des_key_schedule),
	    M_CRYPTO_DATA, M_NOWAIT|M_ZERO);
	*sched = (u_int8_t *) p;
	if (p == NULL)
		return ENOMEM;
	des_set_key((des_cblock *)__UNCONST(key), p[0]);
	return 0;
}

static void
des1_zerokey(u_int8_t **sched)
{
	memset(*sched, 0, sizeof (des_key_schedule));
	free(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

static void
des3_encrypt(void *key, u_int8_t *blk)
{
	des_cblock *cb = (des_cblock *) blk;
	des_key_schedule *p = (des_key_schedule *) key;

	des_ecb3_encrypt(cb, cb, p[0], p[1], p[2], DES_ENCRYPT);
}

static void
des3_decrypt(void *key, u_int8_t *blk)
{
	des_cblock *cb = (des_cblock *) blk;
	des_key_schedule *p = (des_key_schedule *) key;

	des_ecb3_encrypt(cb, cb, p[0], p[1], p[2], DES_DECRYPT);
}

static int
des3_setkey(u_int8_t **sched, const u_int8_t *key, int len)
{
	des_key_schedule *p;

	p = malloc(3*sizeof (des_key_schedule),
		M_CRYPTO_DATA, M_NOWAIT|M_ZERO);
	*sched = (u_int8_t *) p;
	if (p == NULL)
		return ENOMEM;
	des_set_key((des_cblock *)__UNCONST(key +  0), p[0]);
	des_set_key((des_cblock *)__UNCONST(key +  8), p[1]);
	des_set_key((des_cblock *)__UNCONST(key + 16), p[2]);
	return 0;
}

static void
des3_zerokey(u_int8_t **sched)
{
	memset(*sched, 0, 3*sizeof (des_key_schedule));
	free(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

static void
blf_encrypt(void *key, u_int8_t *blk)
{

	BF_ecb_encrypt(blk, blk, (BF_KEY *)key, 1);
}

static void
blf_decrypt(void *key, u_int8_t *blk)
{

	BF_ecb_encrypt(blk, blk, (BF_KEY *)key, 0);
}

static int
blf_setkey(u_int8_t **sched, const u_int8_t *key, int len)
{

	*sched = malloc(sizeof(BF_KEY),
		M_CRYPTO_DATA, M_NOWAIT|M_ZERO);
	if (*sched == NULL)
		return ENOMEM;
	BF_set_key((BF_KEY *) *sched, len, key);
	return 0;
}

static void
blf_zerokey(u_int8_t **sched)
{
	memset(*sched, 0, sizeof(BF_KEY));
	free(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

static void
cast5_encrypt(void *key, u_int8_t *blk)
{
	cast128_encrypt((cast128_key *) key, blk, blk);
}

static void
cast5_decrypt(void *key, u_int8_t *blk)
{
	cast128_decrypt((cast128_key *) key, blk, blk);
}

static int
cast5_setkey(u_int8_t **sched, const u_int8_t *key, int len)
{

	*sched = malloc(sizeof(cast128_key), M_CRYPTO_DATA,
	       M_NOWAIT|M_ZERO);
	if (*sched == NULL)
		return ENOMEM;
	cast128_setkey((cast128_key *)*sched, key, len);
	return 0;
}

static void
cast5_zerokey(u_int8_t **sched)
{
	memset(*sched, 0, sizeof(cast128_key));
	free(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

static void
skipjack_encrypt(void *key, u_int8_t *blk)
{
	skipjack_forwards(blk, blk, (u_int8_t **) key);
}

static void
skipjack_decrypt(void *key, u_int8_t *blk)
{
	skipjack_backwards(blk, blk, (u_int8_t **) key);
}

static int
skipjack_setkey(u_int8_t **sched, const u_int8_t *key, int len)
{

	/* NB: allocate all the memory that's needed at once */
	/* XXX assumes bytes are aligned on sizeof(u_char) == 1 boundaries.
	 * Will this break a pdp-10, Cray-1, or GE-645 port?
	 */
	*sched = malloc(10 * (sizeof(u_int8_t *) + 0x100),
		M_CRYPTO_DATA, M_NOWAIT|M_ZERO);

	if (*sched == NULL)
		return ENOMEM;

	u_int8_t** key_tables = (u_int8_t**) *sched;
	u_int8_t* table = (u_int8_t*) &key_tables[10];
	int k;

	for (k = 0; k < 10; k++) {
		key_tables[k] = table;
		table += 0x100;
	}
	subkey_table_gen(key, (u_int8_t **) *sched);
	return 0;
}

static void
skipjack_zerokey(u_int8_t **sched)
{
	memset(*sched, 0, 10 * (sizeof(u_int8_t *) + 0x100));
	free(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

static void
rijndael128_encrypt(void *key, u_int8_t *blk)
{
	rijndael_encrypt((rijndael_ctx *) key, (u_char *) blk, (u_char *) blk);
}

static void
rijndael128_decrypt(void *key, u_int8_t *blk)
{
	rijndael_decrypt((rijndael_ctx *) key, (u_char *) blk,
	    (u_char *) blk);
}

static int
rijndael128_setkey(u_int8_t **sched, const u_int8_t *key, int len)
{

	if (len != 16 && len != 24 && len != 32)
		return EINVAL;
	*sched = malloc(sizeof(rijndael_ctx), M_CRYPTO_DATA,
	    M_NOWAIT|M_ZERO);
	if (*sched == NULL)
		return ENOMEM;
	rijndael_set_key((rijndael_ctx *) *sched, key, len * 8);
	return 0;
}

static void
rijndael128_zerokey(u_int8_t **sched)
{
	memset(*sched, 0, sizeof(rijndael_ctx));
	free(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

static void
cml_encrypt(void *key, u_int8_t *blk)
{

	camellia_encrypt(key, blk, blk);
}

static void
cml_decrypt(void *key, u_int8_t *blk)
{

	camellia_decrypt(key, blk, blk);
}

static int
cml_setkey(u_int8_t **sched, const u_int8_t *key, int len)
{

	if (len != 16 && len != 24 && len != 32)
		return (EINVAL);
	*sched = malloc(sizeof(camellia_ctx), M_CRYPTO_DATA,
			M_NOWAIT|M_ZERO);
	if (*sched == NULL)
		return ENOMEM;

	camellia_set_key((camellia_ctx *) *sched, key, len * 8);
	return 0;
}

static void
cml_zerokey(u_int8_t **sched)
{

	memset(*sched, 0, sizeof(camellia_ctx));
	free(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

#define AESCTR_NONCESIZE	4
#define AESCTR_IVSIZE		8
#define AESCTR_BLOCKSIZE	16

struct aes_ctr_ctx {
	/* need only encryption half */
	u_int32_t ac_ek[4*(RIJNDAEL_MAXNR + 1)];
	u_int8_t ac_block[AESCTR_BLOCKSIZE];
	int ac_nr;
	struct {
		u_int64_t lastiv;
	} ivgenctx;
};

static void
aes_ctr_crypt(void *key, u_int8_t *blk)
{
	struct aes_ctr_ctx *ctx;
	u_int8_t keystream[AESCTR_BLOCKSIZE];
	int i;

	ctx = key;
	/* increment counter */
	for (i = AESCTR_BLOCKSIZE - 1;
	     i >= AESCTR_NONCESIZE + AESCTR_IVSIZE; i--)
		if (++ctx->ac_block[i]) /* continue on overflow */
			break;
	rijndaelEncrypt(ctx->ac_ek, ctx->ac_nr, ctx->ac_block, keystream);
	for (i = 0; i < AESCTR_BLOCKSIZE; i++)
		blk[i] ^= keystream[i];
	memset(keystream, 0, sizeof(keystream));
}

int
aes_ctr_setkey(u_int8_t **sched, const u_int8_t *key, int len)
{
	struct aes_ctr_ctx *ctx;

	if (len < AESCTR_NONCESIZE)
		return EINVAL;

	ctx = malloc(sizeof(struct aes_ctr_ctx), M_CRYPTO_DATA,
		     M_NOWAIT|M_ZERO);
	if (!ctx)
		return ENOMEM;
	ctx->ac_nr = rijndaelKeySetupEnc(ctx->ac_ek, (const u_char *)key,
			(len - AESCTR_NONCESIZE) * 8);
	if (!ctx->ac_nr) { /* wrong key len */
		aes_ctr_zerokey((u_int8_t **)&ctx);
		return EINVAL;
	}
	memcpy(ctx->ac_block, key + len - AESCTR_NONCESIZE, AESCTR_NONCESIZE);
	/* random start value for simple counter */
	cprng_fast(&ctx->ivgenctx.lastiv, sizeof(ctx->ivgenctx.lastiv));
	*sched = (void *)ctx;
	return 0;
}

void
aes_ctr_zerokey(u_int8_t **sched)
{

	memset(*sched, 0, sizeof(struct aes_ctr_ctx));
	free(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

void
aes_ctr_reinit(void *key, const u_int8_t *iv, u_int8_t *ivout)
{
	struct aes_ctr_ctx *ctx = key;

	if (!iv) {
		ctx->ivgenctx.lastiv++;
		iv = (const u_int8_t *)&ctx->ivgenctx.lastiv;
	}
	if (ivout)
		memcpy(ivout, iv, AESCTR_IVSIZE);
	memcpy(ctx->ac_block + AESCTR_NONCESIZE, iv, AESCTR_IVSIZE);
	/* reset counter */
	memset(ctx->ac_block + AESCTR_NONCESIZE + AESCTR_IVSIZE, 0, 4);
}

void
aes_gcm_reinit(void *key, const u_int8_t *iv, u_int8_t *ivout)
{
	struct aes_ctr_ctx *ctx = key;

	if (!iv) {
		ctx->ivgenctx.lastiv++;
		iv = (const u_int8_t *)&ctx->ivgenctx.lastiv;
	}
	if (ivout)
		memcpy(ivout, iv, AESCTR_IVSIZE);
	memcpy(ctx->ac_block + AESCTR_NONCESIZE, iv, AESCTR_IVSIZE);
	/* reset counter */
	memset(ctx->ac_block + AESCTR_NONCESIZE + AESCTR_IVSIZE, 0, 4);
	ctx->ac_block[AESCTR_BLOCKSIZE - 1] = 1; /* GCM starts with 1 */
}

struct aes_gmac_ctx {
	struct {
		u_int64_t lastiv;
	} ivgenctx;
};

int
aes_gmac_setkey(u_int8_t **sched, const u_int8_t *key, int len)
{
	struct aes_gmac_ctx *ctx;

	ctx = malloc(sizeof(struct aes_gmac_ctx), M_CRYPTO_DATA,
		     M_NOWAIT|M_ZERO);
	if (!ctx)
		return ENOMEM;

	/* random start value for simple counter */
	cprng_fast(&ctx->ivgenctx.lastiv, sizeof(ctx->ivgenctx.lastiv));
	*sched = (void *)ctx;
	return 0;
}

void
aes_gmac_zerokey(u_int8_t **sched)
{

	free(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

void
aes_gmac_reinit(void *key, const u_int8_t *iv, u_int8_t *ivout)
{
	struct aes_gmac_ctx *ctx = key;

	if (!iv) {
		ctx->ivgenctx.lastiv++;
		iv = (const u_int8_t *)&ctx->ivgenctx.lastiv;
	}
	if (ivout)
		memcpy(ivout, iv, AESCTR_IVSIZE);
}

/*
 * And now for auth.
 */

static void
null_init(void *ctx)
{
}

static int
null_update(void *ctx, const u_int8_t *buf,
    u_int16_t len)
{
	return 0;
}

static void
null_final(u_int8_t *buf, void *ctx)
{
	if (buf != (u_int8_t *) 0)
		memset(buf, 0, 12);
}

static int
RMD160Update_int(void *ctx, const u_int8_t *buf, u_int16_t len)
{
	RMD160Update(ctx, buf, len);
	return 0;
}

static int
MD5Update_int(void *ctx, const u_int8_t *buf, u_int16_t len)
{
	MD5Update(ctx, buf, len);
	return 0;
}

static void
SHA1Init_int(void *ctx)
{
	SHA1Init(ctx);
}

static int
SHA1Update_int(void *ctx, const u_int8_t *buf, u_int16_t len)
{
	SHA1Update(ctx, buf, len);
	return 0;
}

static void
SHA1Final_int(u_int8_t *blk, void *ctx)
{
	SHA1Final(blk, ctx);
}

static int
SHA256Update_int(void *ctx, const u_int8_t *buf, u_int16_t len)
{
	SHA256_Update(ctx, buf, len);
	return 0;
}

static int
SHA384Update_int(void *ctx, const u_int8_t *buf, u_int16_t len)
{
	SHA384_Update(ctx, buf, len);
	return 0;
}

static int
SHA512Update_int(void *ctx, const u_int8_t *buf, u_int16_t len)
{
	SHA512_Update(ctx, buf, len);
	return 0;
}

/*
 * And compression
 */

static u_int32_t
deflate_compress(u_int8_t *data, u_int32_t size, u_int8_t **out)
{
	return deflate_global(data, size, 0, out, 0);
}

static u_int32_t
deflate_decompress(u_int8_t *data, u_int32_t size, u_int8_t **out,
		   int size_hint)
{
	return deflate_global(data, size, 1, out, size_hint);
}

static u_int32_t
gzip_compress(u_int8_t *data, u_int32_t size, u_int8_t **out)
{
	return gzip_global(data, size, 0, out, 0);
}

static u_int32_t
gzip_decompress(u_int8_t *data, u_int32_t size, u_int8_t **out,
		int size_hint)
{
	return gzip_global(data, size, 1, out, size_hint);
}

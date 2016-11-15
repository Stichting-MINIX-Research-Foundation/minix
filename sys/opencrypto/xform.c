/*	$NetBSD: xform.c,v 1.28 2011/05/26 21:50:03 drochner Exp $ */
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
__KERNEL_RCSID(0, "$NetBSD: xform.c,v 1.28 2011/05/26 21:50:03 drochner Exp $");

#include <sys/param.h>
#include <sys/malloc.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform.h>

MALLOC_DEFINE(M_XDATA, "xform", "xform data buffers");

const u_int8_t hmac_ipad_buffer[128] = {
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36
};

const u_int8_t hmac_opad_buffer[128] = {
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C
};

/* Encryption instances */
const struct enc_xform enc_xform_null = {
	CRYPTO_NULL_CBC, "NULL",
	/* NB: blocksize of 4 is to generate a properly aligned ESP header */
	4, 0, 0, 256 /* 2048 bits, max key */
};

const struct enc_xform enc_xform_des = {
	CRYPTO_DES_CBC, "DES",
	8, 8, 8, 8
};

const struct enc_xform enc_xform_3des = {
	CRYPTO_3DES_CBC, "3DES",
	8, 8, 24, 24
};

const struct enc_xform enc_xform_blf = {
	CRYPTO_BLF_CBC, "Blowfish",
	8, 8, 5, 56 /* 448 bits, max key */
};

const struct enc_xform enc_xform_cast5 = {
	CRYPTO_CAST_CBC, "CAST-128",
	8, 8, 5, 16
};

const struct enc_xform enc_xform_skipjack = {
	CRYPTO_SKIPJACK_CBC, "Skipjack",
	8, 8, 10, 10
};

const struct enc_xform enc_xform_rijndael128 = {
	CRYPTO_RIJNDAEL128_CBC, "Rijndael-128/AES",
	16, 16, 16, 32
};

const struct enc_xform enc_xform_arc4 = {
	CRYPTO_ARC4, "ARC4",
	1, 0, 1, 32
};

const struct enc_xform enc_xform_camellia = {
	CRYPTO_CAMELLIA_CBC, "Camellia",
	16, 16, 8, 32
};

const struct enc_xform enc_xform_aes_ctr = {
	CRYPTO_AES_CTR, "AES-CTR",
	16, 8, 16+4, 32+4
};

const struct enc_xform enc_xform_aes_gcm = {
	CRYPTO_AES_GCM_16, "AES-GCM",
	4 /* ??? */, 8, 16+4, 32+4
};

const struct enc_xform enc_xform_aes_gmac = {
	CRYPTO_AES_GMAC, "AES-GMAC",
	4 /* ??? */, 8, 16+4, 32+4
};

/* Authentication instances */
const struct auth_hash auth_hash_null = {
	CRYPTO_NULL_HMAC, "NULL-HMAC",
	0, 0, 12, 64
};

const struct auth_hash auth_hash_hmac_md5 = {
	CRYPTO_MD5_HMAC, "HMAC-MD5",
	16, 16, 16, 64
};

const struct auth_hash auth_hash_hmac_sha1 = {
	CRYPTO_SHA1_HMAC, "HMAC-SHA1",
	20, 20, 20, 64
};

const struct auth_hash auth_hash_hmac_ripemd_160 = {
	CRYPTO_RIPEMD160_HMAC, "HMAC-RIPEMD-160",
	20, 20, 20, 64
};

const struct auth_hash auth_hash_hmac_md5_96 = {
	CRYPTO_MD5_HMAC_96, "HMAC-MD5-96",
	16, 16, 12, 64
};

const struct auth_hash auth_hash_hmac_sha1_96 = {
	CRYPTO_SHA1_HMAC_96, "HMAC-SHA1-96",
	20, 20, 12, 64
};

const struct auth_hash auth_hash_hmac_ripemd_160_96 = {
	CRYPTO_RIPEMD160_HMAC_96, "HMAC-RIPEMD-160",
	20, 20, 12, 64
};

const struct auth_hash auth_hash_key_md5 = {
	CRYPTO_MD5_KPDK, "Keyed MD5",
	0, 16, 16, 0
};

const struct auth_hash auth_hash_key_sha1 = {
	CRYPTO_SHA1_KPDK, "Keyed SHA1",
	0, 20, 20, 0
};

const struct auth_hash auth_hash_md5 = {
	CRYPTO_MD5, "MD5",
	0, 16, 16, 0
};

const struct auth_hash auth_hash_sha1 = {
	CRYPTO_SHA1, "SHA1",
	0, 20, 20, 0
};

const struct auth_hash auth_hash_hmac_sha2_256 = {
	CRYPTO_SHA2_256_HMAC, "HMAC-SHA2",
	32, 32, 16, 64
};

const struct auth_hash auth_hash_hmac_sha2_384 = {
	CRYPTO_SHA2_384_HMAC, "HMAC-SHA2-384",
	48, 48, 24, 128
};

const struct auth_hash auth_hash_hmac_sha2_512 = {
	CRYPTO_SHA2_512_HMAC, "HMAC-SHA2-512",
	64, 64, 32, 128
};

const struct auth_hash auth_hash_aes_xcbc_mac_96 = {
	CRYPTO_AES_XCBC_MAC_96, "AES-XCBC-MAC-96",
	16, 16, 12, 0
};

const struct auth_hash auth_hash_gmac_aes_128 = {
	CRYPTO_AES_128_GMAC, "GMAC-AES-128",
	16+4, 16, 16, 16 /* ??? */
};

const struct auth_hash auth_hash_gmac_aes_192 = {
	CRYPTO_AES_192_GMAC, "GMAC-AES-192",
	24+4, 16, 16, 16 /* ??? */
};

const struct auth_hash auth_hash_gmac_aes_256 = {
	CRYPTO_AES_256_GMAC, "GMAC-AES-256",
	32+4, 16, 16, 16 /* ??? */
};

/* Compression instance */
const struct comp_algo comp_algo_deflate = {
	CRYPTO_DEFLATE_COMP, "Deflate",
	90
};

const struct comp_algo comp_algo_deflate_nogrow = {
	CRYPTO_DEFLATE_COMP_NOGROW, "Deflate",
	90
};

const struct comp_algo comp_algo_gzip = {
	CRYPTO_GZIP_COMP, "GZIP",
	90
};

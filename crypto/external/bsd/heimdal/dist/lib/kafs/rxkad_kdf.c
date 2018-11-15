/*	$NetBSD: rxkad_kdf.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 1995-2003 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2013-2014 Carnegie Mellon University
 * All rights reserved.
 *
 * Portions Copyright (c) 2013 by the Massachusetts Institute of Technology
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

#include "kafs_locl.h"

static int rxkad_derive_des_key(const void *, size_t, char[8]);
static int compress_parity_bits(void *, size_t *);

/**
 * Use NIST SP800-108 with HMAC(MD5) in counter mode as the PRF to derive a
 * des key from another type of key.
 *
 * L is 64, as we take 64 random bits and turn them into a 56-bit des key.
 * The output of hmac_md5 is 128 bits; we take the first 64 only, so n
 * properly should be 1.  However, we apply a slight variation due to the
 * possibility of producing a weak des key.  If the output key is weak, do NOT
 * simply correct it, instead, the counter is advanced and the next output
 * used.  As such, we code so as to have n be the full 255 permitted by our
 * encoding of the counter i in an 8-bit field.  L itself is encoded as a
 * 32-bit field, big-endian.  We use the constant string "rxkad" as a label
 * for this key derivation, the standard NUL byte separator, and omit a
 * key-derivation context.  The input key is unique to the krb5 service ticket,
 * which is unlikely to be used in an other location.  If it is used in such
 * a fashion, both locations will derive the same des key from the PRF, but
 * this is no different from if a krb5 des key had been used in the same way,
 * as traditional krb5 rxkad uses the ticket session key directly as the token
 * key.
 *
 * @param[in]  in      pointer to input key data
 * @param[in]  insize  length of input key data
 * @param[out] out     8-byte buffer to hold the derived key
 *
 * @return Returns 0 to indicate success, or an error code.
 *
 * @retval KRB5DES_WEAK_KEY  Successive derivation attempts with all
 * 255 possible counter values each produced weak DES keys.  This input
 * cannot be used to produce a usable key.
 */
static int
rxkad_derive_des_key(const void *in, size_t insize, char out[8])
{
    unsigned char i;
    static unsigned char label[] = "rxkad";
    /* bits of output, as 32 bit word, MSB first */
    static unsigned char Lbuf[4] = { 0, 0, 0, 64 };
    /* only needs to be 16 for md5, but lets be sure it fits */
    unsigned char tmp[64];
    unsigned int mdsize;
    DES_cblock ktmp;
    HMAC_CTX mctx;

    /* stop when 8 bit counter wraps to 0 */
    for (i = 1; i; i++) {
	HMAC_CTX_init(&mctx);
	HMAC_Init_ex(&mctx, in, insize, EVP_md5(), NULL);
	HMAC_Update(&mctx, &i, 1);
	HMAC_Update(&mctx, label, sizeof(label));   /* includes label and separator */
	HMAC_Update(&mctx, Lbuf, 4);
	mdsize = sizeof(tmp);
	HMAC_Final(&mctx, tmp, &mdsize);
	memcpy(ktmp, tmp, 8);
	DES_set_odd_parity(&ktmp);
	if (!DES_is_weak_key(&ktmp)) {
	    memcpy(out, ktmp, 8);
	    return 0;
	}
    }
    return KRB5DES_WEAK_KEY;
}

/**
 * This is the inverse of the random-to-key for 3des specified in
 * rfc3961, converting blocks of 8 bytes to blocks of 7 bytes by distributing
 * the bits of each 8th byte as the lsb of the previous 7 bytes.
 *
 * @param[in,out]  buffer  Buffer containing the key to be converted
 * @param[in,out]  bufsiz  Points to the size of the key data.  On
 * return, this is updated to reflect the size of the compressed data.
 *
 * @return Returns 0 to indicate success, or an error code.
 *
 * @retval KRB5_BAD_KEYSIZE  The key size was not a multiple of 8 bytes.
 */
static int
compress_parity_bits(void *buffer, size_t *bufsiz)
{
    unsigned char *cb, tmp;
    int i, j, nk;

    if (*bufsiz % 8 != 0)
	return KRB5_BAD_KEYSIZE;
    cb = (unsigned char *)buffer;
    nk = *bufsiz / 8;
    for (i = 0; i < nk; i++) {
	tmp = cb[8 * i + 7] >> 1;
	for (j = 0; j < 7; j++) {
	    cb[8 * i + j] &= 0xfe;
	    cb[8 * i + j] |= tmp & 0x1;
	    tmp >>= 1;
	}
    }
    for (i = 1; i < nk; i++)
	memmove(cb + 7 * i, cb + 8 * i, 7);
    *bufsiz = 7 * nk;
    return 0;
}

/**
 * Derive a DES key for use with rxkad and fcrypt from a given Kerberos
 * key of (almost) any type.  This function encodes enctype-specific
 * knowledge about how to derive a DES key from a given key type.
 * If given a des key, use it directly; otherwise, perform any parity
 * fixup that may be needed and pass through to the hmad-md5 bits.
 *
 * @param[in]   enctype  Kerberos enctype of the input key
 * @param[in]   keydata  Input key data
 * @param[in]   keylen   Size of input key data
 * @param[out]  output   8-byte buffer to hold the derived key
 *
 * @return Returns 0 to indicate success, or an error code.
 *
 * @retval KRB5_PROG_ETYPE_NOSUPP  The enctype is one for which rxkad-kdf
 * is not supported.  This includes several reserved enctypes, enctype
 * values used in PKINIT to stand for CMS algorithm identifiers, and all
 * private-use (negative) enctypes.
 *
 * @retval KRB5_BAD_KEYSIZE  The key size was not a multiple of 8 bytes
 * (for 3DES key types), exactly 8 bytes (for DES key types), or at least
 * 8 bytes (for other key types).
 *
 * @retval KRB5DES_WEAK_KEY  Successive derivation attempts with all
 * 255 possible counter values each produced weak DES keys.  This input
 * cannot be used to produce a usable key.
 */
int
_kafs_derive_des_key(krb5_enctype enctype, void *keydata, size_t keylen,
		     char output[8])
{
    int ret = 0;

    switch ((int)enctype) {
    case ETYPE_DES_CBC_CRC:
    case ETYPE_DES_CBC_MD4:
    case ETYPE_DES_CBC_MD5:
	if (keylen != 8)
	    return KRB5_BAD_KEYSIZE;

	/* Extract session key */
	memcpy(output, keydata, 8);
	break;
    case ETYPE_NULL:
    case 4:
    case 6:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
	return KRB5_PROG_ETYPE_NOSUPP;
	/*In order to become a "Cryptographic Key" as specified in
	 * SP800-108, it must be indistinguishable from a random bitstring. */
    case ETYPE_DES3_CBC_MD5:
    case ETYPE_OLD_DES3_CBC_SHA1:
    case ETYPE_DES3_CBC_SHA1:
	ret = compress_parity_bits(keydata, &keylen);
	if (ret)
	    return ret;
	/* FALLTHROUGH */
    default:
	if (enctype < 0)
	    return KRB5_PROG_ETYPE_NOSUPP;
	if (keylen < 7)
	    return KRB5_BAD_KEYSIZE;
	ret = rxkad_derive_des_key(keydata, keylen, output);
    }
    return ret;
}

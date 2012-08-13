/*	rijndael-api.c - Rijndael encryption programming interface.
 *							Author: Kees J. Bot
 *								3 Nov 2000
 * Heavily based on the original API code by Antoon Bosselaers,
 * Vincent Rijmen, and Paulo Barreto, but with a different interface.
 *
 * Read this code top to bottom, not all comments are repeated.
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "rijndael-alg.h"
#include "rijndael-api.h"

/* Map a byte (?) address to a word address or vv. */
#define W(a)	((word32 *) (a))
#define B(a)	((word8 *) (a))

#if STRICT_ALIGN
/* This machine checks alignment religiously.  (The code is not proper with
 * respect to alignment.  We need a compiler that doesn't muck about with byte
 * arrays that follow words in structs, and that places automatic variables
 * at word boundaries if not odd-sized.  Most compilers are this nice.)
 */

#define aligned(a)		(((unsigned) (a) & 3) == 0)
#define aligned2(a1, a2)	aligned((unsigned) (a1) | (unsigned) (a2))

static void blockcpy(void *dst, const void *src)
{
    int i= 0;

    do {
	B(dst)[i+0] = B(src)[i+0];
	B(dst)[i+1] = B(src)[i+1];
	B(dst)[i+2] = B(src)[i+2];
	B(dst)[i+3] = B(src)[i+3];
    } while ((i += 4) < 16);
}

#else /* !STRICT_ALIGN */
/* This machine doesn't mind misaligned accesses much. */

#define aligned(a)		((void) (a), 1)
#define aligned2(a1, a2)	((void) (a1), (void) (a2), 1)

#if __GNUC__
__inline
#endif
static void blockcpy(void *dst, const void *src)
{
    W(dst)[0] = W(src)[0];
    W(dst)[1] = W(src)[1];
    W(dst)[2] = W(src)[2];
    W(dst)[3] = W(src)[3];
}

#endif /* !STRICT_ALIGN */

#define between(a, c, z)	((unsigned) (c) - (a) <= (unsigned) (z) - (a))

int rijndael_makekey(rd_keyinstance *key,
	size_t keylen, const void *keymaterial)
{
    word8 k[MAXKC][4];

    /* Initialize key schedule: */
    if (keylen == RD_KEY_HEX) {
	const word8 *kp;
	int c, b;

	kp= keymaterial;
	keylen= 0;

	for (;;) {
	    c= *kp++;
	    if (between('0', c, '9')) b= (c - '0' + 0x0) << 4;
	    else
	    if (between('a', c, 'f')) b= (c - 'a' + 0xa) << 4;
	    else
	    if (between('A', c, 'F')) b= (c - 'A' + 0xA) << 4;
	    else break;

	    c= *kp++;
	    if (between('0', c, '9')) b |= (c - '0' + 0x0);
	    else
	    if (between('a', c, 'f')) b |= (c - 'a' + 0xa);
	    else
	    if (between('A', c, 'F')) b |= (c - 'A' + 0xA);
	    else break;

	    if (keylen >= 256/8) return RD_BAD_KEY_MAT;
	    k[keylen/4][keylen%4] = b;
	    keylen++;
	}
	if (c != 0) return RD_BAD_KEY_MAT;

	if (keylen != 128/8 && keylen != 192/8 && keylen != 256/8) {
	    return RD_BAD_KEY_MAT;
	}
    } else {
	if (keylen != 128/8 && keylen != 192/8 && keylen != 256/8) {
	    return RD_BAD_KEY_MAT;
	}
	memcpy(k, keymaterial, keylen);
    }

    key->rounds= keylen * 8 / 32 + 6;

    rijndael_KeySched(k, key->encsched, key->rounds);
    memcpy(key->decsched, key->encsched, sizeof(key->decsched));
    rijndael_KeyEncToDec(key->decsched, key->rounds);

    return 0;
}

ssize_t rijndael_ecb_encrypt(rd_keyinstance *key,
	const void *input, void *output, size_t length, void *dummyIV)
{
    /* Encrypt blocks of data in Electronic Codebook mode. */
    const word8 *inp= input;
    word8 *outp= output;
    size_t i, nr_blocks, extra;
    word32 in[4], out[4];
    word8 t;

    /* Compute the number of whole blocks, and the extra bytes beyond the
     * last block.  Those extra bytes, if any, are encrypted by stealing
     * enough bytes from the previous encrypted block to make a whole block.
     * This is done by encrypting the last block, exchanging the first few
     * encrypted bytes with the extra bytes, and encrypting the last whole
     * block again.
     */
    nr_blocks= length / 16;
    if ((extra= (length % 16)) > 0) {
	if (nr_blocks == 0) return RD_BAD_BLOCK_LENGTH;
	nr_blocks--;
    }

    /* Encrypt a number of blocks. */
    if (aligned2(inp, outp)) {
	for (i= 0; i < nr_blocks; i++) {
	    rijndael_Encrypt(inp, outp, key->encsched, key->rounds);
	    inp += 16;
	    outp += 16;
	}
    } else {
	for (i= 0; i < nr_blocks; i++) {
	    blockcpy(in, inp);
	    rijndael_Encrypt(in, out, key->encsched, key->rounds);
	    blockcpy(outp, out);
	    inp += 16;
	    outp += 16;
	}
    }

    /* Encrypt extra bytes by stealing from the last full block. */
    if (extra > 0) {
	blockcpy(in, inp);
	rijndael_Encrypt(in, out, key->encsched, key->rounds);
	for (i= 0; i < extra; i++) {
	    t= B(out)[i];
	    B(out)[i] = inp[16 + i];
	    outp[16 + i] = t;
	}
	rijndael_Encrypt(out, out, key->encsched, key->rounds);
	blockcpy(outp, out);
    }
    return length;
}

ssize_t rijndael_ecb_decrypt(rd_keyinstance *key,
	const void *input, void *output, size_t length, void *dummyIV)
{
    /* Decrypt blocks of data in Electronic Codebook mode. */
    const word8 *inp= input;
    word8 *outp= output;
    size_t i, nr_blocks, extra;
    word32 in[4], out[4];
    word8 t;

    nr_blocks= length / 16;
    if ((extra= (length % 16)) > 0) {
	if (nr_blocks == 0) return RD_BAD_BLOCK_LENGTH;
	nr_blocks--;
    }

    /* Decrypt a number of blocks. */
    if (aligned2(inp, outp)) {
	for (i= 0; i < nr_blocks; i++) {
	    rijndael_Decrypt(inp, outp, key->decsched, key->rounds);
	    inp += 16;
	    outp += 16;
	}
    } else {
	for (i= 0; i < nr_blocks; i++) {
	    blockcpy(in, inp);
	    rijndael_Decrypt(in, out, key->decsched, key->rounds);
	    blockcpy(outp, out);
	    inp += 16;
	    outp += 16;
	}
    }

    /* Decrypt extra bytes that stole from the last full block. */
    if (extra > 0) {
	blockcpy(in, inp);
	rijndael_Decrypt(in, out, key->decsched, key->rounds);
	for (i= 0; i < extra; i++) {
	    t= B(out)[i];
	    B(out)[i] = inp[16 + i];
	    outp[16 + i] = t;
	}
	rijndael_Decrypt(out, out, key->decsched, key->rounds);
	blockcpy(outp, out);
    }
    return length;
}

ssize_t rijndael_cbc_encrypt(rd_keyinstance *key,
	const void *input, void *output, size_t length, void *IV)
{
    /* Encrypt blocks of data in Cypher Block Chaining mode. */
    const word8 *inp= input;
    word8 *outp= output;
    size_t i, nr_blocks, extra;
    word32 in[4], out[4], iv[4], *ivp;
    word8 t;

    nr_blocks= length / 16;
    if ((extra= (length % 16)) > 0) {
	if (nr_blocks == 0) return RD_BAD_BLOCK_LENGTH;
	nr_blocks--;
    }

    /* Each input block is first XORed with the previous encryption result.
     * The "Initialization Vector" is used to XOR the first block with.
     * When done the last crypted block is stored back as the new IV to be
     * used for another call to this function.
     */
    ivp= aligned(IV) ? IV : (blockcpy(iv, IV), iv);

    if (aligned2(inp, outp)) {
	for (i= 0; i < nr_blocks; i++) {
	    in[0] = W(inp)[0] ^ ivp[0];
	    in[1] = W(inp)[1] ^ ivp[1];
	    in[2] = W(inp)[2] ^ ivp[2];
	    in[3] = W(inp)[3] ^ ivp[3];
	    rijndael_Encrypt(in, outp, key->encsched, key->rounds);
	    ivp= W(outp);
	    inp += 16;
	    outp += 16;
	}
    } else {
	for (i= 0; i < nr_blocks; i++) {
	    blockcpy(in, inp);
	    in[0] ^= ivp[0];
	    in[1] ^= ivp[1];
	    in[2] ^= ivp[2];
	    in[3] ^= ivp[3];
	    rijndael_Encrypt(in, out, key->encsched, key->rounds);
	    blockcpy(outp, out);
	    ivp= out;
	    inp += 16;
	    outp += 16;
	}
    }
    if (extra > 0) {
	blockcpy(in, inp);
	in[0] ^= ivp[0];
	in[1] ^= ivp[1];
	in[2] ^= ivp[2];
	in[3] ^= ivp[3];
	rijndael_Encrypt(in, out, key->encsched, key->rounds);
	for (i= 0; i < extra; i++) {
	    t= B(out)[i];
	    B(out)[i] ^= inp[16 + i];
	    outp[16 + i] = t;
	}
	rijndael_Encrypt(out, out, key->encsched, key->rounds);
	blockcpy(outp, out);
	ivp= out;
    }
    blockcpy(IV, ivp);		/* Store last IV back. */
    return length;
}

ssize_t rijndael_cbc_decrypt(rd_keyinstance *key,
	const void *input, void *output, size_t length, void *IV)
{
    /* Decrypt blocks of data in Cypher Block Chaining mode. */
    const word8 *inp= input;
    word8 *outp= output;
    size_t i, nr_blocks, extra;
    word32 in[4], out[4], iv[4];
    word8 t;

    nr_blocks= length / 16;
    if ((extra= (length % 16)) > 0) {
	if (nr_blocks == 0) return RD_BAD_BLOCK_LENGTH;
	nr_blocks--;
    }

    blockcpy(iv, IV);

    if (aligned2(inp, outp)) {
	for (i= 0; i < nr_blocks; i++) {
	    rijndael_Decrypt(inp, out, key->decsched, key->rounds);
	    out[0] ^= iv[0];
	    out[1] ^= iv[1];
	    out[2] ^= iv[2];
	    out[3] ^= iv[3];
	    iv[0] = W(inp)[0];
	    iv[1] = W(inp)[1];
	    iv[2] = W(inp)[2];
	    iv[3] = W(inp)[3];
	    W(outp)[0] = out[0];
	    W(outp)[1] = out[1];
	    W(outp)[2] = out[2];
	    W(outp)[3] = out[3];
	    inp += 16;
	    outp += 16;
	}
    } else {
	for (i= 0; i < nr_blocks; i++) {
	    blockcpy(in, inp);
	    rijndael_Decrypt(in, out, key->decsched, key->rounds);
	    out[0] ^= iv[0];
	    out[1] ^= iv[1];
	    out[2] ^= iv[2];
	    out[3] ^= iv[3];
	    iv[0] = in[0];
	    iv[1] = in[1];
	    iv[2] = in[2];
	    iv[3] = in[3];
	    blockcpy(outp, out);
	    inp += 16;
	    outp += 16;
	}
    }
    if (extra > 0) {
	blockcpy(in, inp);
	blockcpy(IV, in);
	rijndael_Decrypt(in, out, key->decsched, key->rounds);
	for (i= 0; i < extra; i++) {
	    t= B(out)[i] ^ inp[16 + i];
	    B(out)[i] = inp[16 + i];
	    outp[16 + i] = t;
	}
	rijndael_Decrypt(out, out, key->decsched, key->rounds);
	out[0] ^= iv[0];
	out[1] ^= iv[1];
	out[2] ^= iv[2];
	out[3] ^= iv[3];
	blockcpy(outp, out);
    } else {
	blockcpy(IV, iv);
    }
    return length;
}

ssize_t rijndael_cfb1_encrypt(rd_keyinstance *key,
	const void *input, void *output, size_t length, void *IV)
{
    /* Encrypt blocks of data in Cypher Feedback mode, 1 bit at a time. */
    const word8 *inp= input;
    word8 *outp= output;
    word8 t;
    size_t i;
    int b;
    word32 iv[4], civ[4];

    blockcpy(iv, IV);

    for (i= 0; i < length; i++) {
	t= *inp++;
	for (b= 0; b < 8; b++) {
	    rijndael_Encrypt(iv, civ, key->encsched, key->rounds);
	    t ^= (B(civ)[0] & 0x80) >> b;
	    B(iv)[ 0] = (B(iv)[ 0] << 1) | (B(iv)[ 1] >> 7);
	    B(iv)[ 1] = (B(iv)[ 1] << 1) | (B(iv)[ 2] >> 7);
	    B(iv)[ 2] = (B(iv)[ 2] << 1) | (B(iv)[ 3] >> 7);
	    B(iv)[ 3] = (B(iv)[ 3] << 1) | (B(iv)[ 4] >> 7);
	    B(iv)[ 4] = (B(iv)[ 4] << 1) | (B(iv)[ 5] >> 7);
	    B(iv)[ 5] = (B(iv)[ 5] << 1) | (B(iv)[ 6] >> 7);
	    B(iv)[ 6] = (B(iv)[ 6] << 1) | (B(iv)[ 7] >> 7);
	    B(iv)[ 7] = (B(iv)[ 7] << 1) | (B(iv)[ 8] >> 7);
	    B(iv)[ 8] = (B(iv)[ 8] << 1) | (B(iv)[ 9] >> 7);
	    B(iv)[ 9] = (B(iv)[ 9] << 1) | (B(iv)[10] >> 7);
	    B(iv)[10] = (B(iv)[10] << 1) | (B(iv)[11] >> 7);
	    B(iv)[11] = (B(iv)[11] << 1) | (B(iv)[12] >> 7);
	    B(iv)[12] = (B(iv)[12] << 1) | (B(iv)[13] >> 7);
	    B(iv)[13] = (B(iv)[13] << 1) | (B(iv)[14] >> 7);
	    B(iv)[14] = (B(iv)[14] << 1) | (B(iv)[15] >> 7);
	    B(iv)[15] = (B(iv)[15] << 1) | ((t >> (7-b)) & 1);
	}
	*outp++ = t;
    }
    blockcpy(IV, iv);
    return length;
}

ssize_t rijndael_cfb1_decrypt(rd_keyinstance *key,
	const void *input, void *output, size_t length, void *IV)
{
    /* Decrypt blocks of data in Cypher Feedback mode, 1 bit at a time. */
    const word8 *inp= input;
    word8 *outp= output;
    word8 t;
    size_t i;
    int b;
    word32 iv[4], civ[4];

    blockcpy(iv, IV);

    for (i= 0; i < length; i++) {
	t= *inp++;
	for (b= 0; b < 8; b++) {
	    rijndael_Encrypt(iv, civ, key->encsched, key->rounds);
	    B(iv)[ 0] = (B(iv)[ 0] << 1) | (B(iv)[ 1] >> 7);
	    B(iv)[ 1] = (B(iv)[ 1] << 1) | (B(iv)[ 2] >> 7);
	    B(iv)[ 2] = (B(iv)[ 2] << 1) | (B(iv)[ 3] >> 7);
	    B(iv)[ 3] = (B(iv)[ 3] << 1) | (B(iv)[ 4] >> 7);
	    B(iv)[ 4] = (B(iv)[ 4] << 1) | (B(iv)[ 5] >> 7);
	    B(iv)[ 5] = (B(iv)[ 5] << 1) | (B(iv)[ 6] >> 7);
	    B(iv)[ 6] = (B(iv)[ 6] << 1) | (B(iv)[ 7] >> 7);
	    B(iv)[ 7] = (B(iv)[ 7] << 1) | (B(iv)[ 8] >> 7);
	    B(iv)[ 8] = (B(iv)[ 8] << 1) | (B(iv)[ 9] >> 7);
	    B(iv)[ 9] = (B(iv)[ 9] << 1) | (B(iv)[10] >> 7);
	    B(iv)[10] = (B(iv)[10] << 1) | (B(iv)[11] >> 7);
	    B(iv)[11] = (B(iv)[11] << 1) | (B(iv)[12] >> 7);
	    B(iv)[12] = (B(iv)[12] << 1) | (B(iv)[13] >> 7);
	    B(iv)[13] = (B(iv)[13] << 1) | (B(iv)[14] >> 7);
	    B(iv)[14] = (B(iv)[14] << 1) | (B(iv)[15] >> 7);
	    B(iv)[15] = (B(iv)[15] << 1) | ((t >> (7-b)) & 1);
	    t ^= (B(civ)[0] & 0x80) >> b;
	}
	*outp++ = t;
    }
    blockcpy(IV, iv);
    return length;
}

ssize_t rijndael_cfb8_encrypt(rd_keyinstance *key,
	const void *input, void *output, size_t length, void *IV)
{
    /* Encrypt blocks of data in Cypher Feedback mode, 8 bits at a time. */
    const word8 *inp= input;
    word8 *outp= output;
    word8 t;
    size_t i;
    word32 iv[4], civ[4];

    blockcpy(iv, IV);

    for (i= 0; i < length; i++) {
	t= *inp++;
	rijndael_Encrypt(iv, civ, key->encsched, key->rounds);
	t ^= B(civ)[0];
	B(iv)[ 0] = B(iv)[ 1];
	B(iv)[ 1] = B(iv)[ 2];
	B(iv)[ 2] = B(iv)[ 3];
	B(iv)[ 3] = B(iv)[ 4];
	B(iv)[ 4] = B(iv)[ 5];
	B(iv)[ 5] = B(iv)[ 6];
	B(iv)[ 6] = B(iv)[ 7];
	B(iv)[ 7] = B(iv)[ 8];
	B(iv)[ 8] = B(iv)[ 9];
	B(iv)[ 9] = B(iv)[10];
	B(iv)[10] = B(iv)[11];
	B(iv)[11] = B(iv)[12];
	B(iv)[12] = B(iv)[13];
	B(iv)[13] = B(iv)[14];
	B(iv)[14] = B(iv)[15];
	B(iv)[15] = t;
	*outp++ = t;
    }
    blockcpy(IV, iv);
    return length;
}

ssize_t rijndael_cfb8_decrypt(rd_keyinstance *key,
	const void *input, void *output, size_t length, void *IV)
{
    /* Decrypt blocks of data in Cypher Feedback mode, 1 byte at a time. */
    const word8 *inp= input;
    word8 *outp= output;
    word8 t;
    size_t i;
    word32 iv[4], civ[4];

    blockcpy(iv, IV);

    for (i= 0; i < length; i++) {
	t= *inp++;
	rijndael_Encrypt(iv, civ, key->encsched, key->rounds);
	B(iv)[ 0] = B(iv)[ 1];
	B(iv)[ 1] = B(iv)[ 2];
	B(iv)[ 2] = B(iv)[ 3];
	B(iv)[ 3] = B(iv)[ 4];
	B(iv)[ 4] = B(iv)[ 5];
	B(iv)[ 5] = B(iv)[ 6];
	B(iv)[ 6] = B(iv)[ 7];
	B(iv)[ 7] = B(iv)[ 8];
	B(iv)[ 8] = B(iv)[ 9];
	B(iv)[ 9] = B(iv)[10];
	B(iv)[10] = B(iv)[11];
	B(iv)[11] = B(iv)[12];
	B(iv)[12] = B(iv)[13];
	B(iv)[13] = B(iv)[14];
	B(iv)[14] = B(iv)[15];
	B(iv)[15] = t;
	t ^= B(civ)[0];
	*outp++ = t;
    }
    blockcpy(IV, iv);
    return length;
}

ssize_t rijndael_pad(void *input, size_t length)
{
    /* Adds at most one block of RFC-2040 style padding to the input to make
     * it a whole number of blocks for easier encryption.  To be used if the
     * input may be less then one block in size, otherwise let the encryption
     * routines use cypher stealing.  The input buffer should allow enough
     * space for the padding.  The new length of the input is returned.
     */
    word8 *inp= input;
    size_t padlen;

    /* Add padding up until the next block boundary. */
    padlen= 16 - (length % 16);
    memset(inp + length, padlen, padlen);
    return length + padlen;
}

ssize_t rijndael_unpad(const void *input, size_t length)
{
    /* Remove RFC-2040 style padding after decryption.  The true length of
     * the input is returned, or the usual errors if the padding is incorrect.
     */
    const word8 *inp= input;
    size_t i, padlen;

    if (length == 0 || (length % 16) != 0) return RD_BAD_BLOCK_LENGTH;
    padlen = inp[length-1];
    if (padlen <= 0 || padlen > 16) return RD_BAD_DATA;
    for (i= 2; i <= padlen; i++) {
	if (inp[length-i] != padlen) return RD_BAD_DATA;
    }
    return length - padlen;
}

#ifdef INTERMEDIATE_VALUE_KAT

void cipherEncryptUpdateRounds(rd_keyinstance *key,
	const void *input, void *output, int rounds)
{
    /* Encrypt a block only a specified number of rounds. */
    word8 block[4][4];

    blockcpy(block, input);

    rijndaelEncryptRound(block, key->encsched, key->rounds, rounds);

    blockcpy(output, block);
}

void cipherDecryptUpdateRounds(rd_keyinstance *key,
	const void *input, void *output, int rounds)
{
    /* Decrypt a block only a specified number of rounds. */
    word8 block[4][4];

    blockcpy(block, input);

    rijndaelDecryptRound(block, key->decsched, key->rounds, rounds);

    blockcpy(output, block);
}
#endif /* INTERMEDIATE_VALUE_KAT */

/*
 * $PchId: rijndael_api.c,v 1.2 2001/01/10 22:01:20 philip Exp $
 */

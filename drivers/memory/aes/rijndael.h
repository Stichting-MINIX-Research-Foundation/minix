/*	rijndael-api.h - Rijndael encryption programming interface.
 *							Author: Kees J. Bot
 *								3 Nov 2000
 * Heavily based on the original API code by Antoon Bosselaers,
 * Vincent Rijmen, and Paulo Barreto, but with a different interface.
 *
 * This code (.h and .c) is in the public domain.
 */

#ifndef _CRYPTO__RIJNDAEL_H
#define _CRYPTO__RIJNDAEL_H

/* Error codes. */
#define RD_BAD_KEY_MAT	    -1	/* Key material not of correct length */
#define RD_BAD_BLOCK_LENGTH -2	/* Data is not a block multiple */
#define RD_BAD_DATA	    -3	/* Data contents are invalid (bad padding?) */

/* Key information. */
#define RD_KEY_HEX	    -1	/* Key is in hex (otherwise octet length) */
#define RD_MAXROUNDS	    14	/* Max number of encryption rounds. */

typedef struct {
	int	rounds;		/* Key-length-dependent number of rounds */
	unsigned char encsched[RD_MAXROUNDS+1][4][4];	/* Encr key schedule */
	unsigned char decsched[RD_MAXROUNDS+1][4][4];	/* Decr key schedule */
} rd_keyinstance;

#define AES_BLOCKSIZE	16

/* Function prototypes. */

int rijndael_makekey(rd_keyinstance *_key,
	size_t _keylen, const void *_keymaterial);

ssize_t rijndael_ecb_encrypt(rd_keyinstance *_key,
	const void *_input, void *_output, size_t _length, void *_dummyIV);

ssize_t rijndael_ecb_decrypt(rd_keyinstance *_key,
	const void *_input, void *_output, size_t _length, void *_dummyIV);

ssize_t rijndael_cbc_encrypt(rd_keyinstance *_key,
	const void *_input, void *_output, size_t _length, void *_IV);

ssize_t rijndael_cbc_decrypt(rd_keyinstance *_key,
	const void *_input, void *_output, size_t _length, void *_IV);

ssize_t rijndael_cfb1_encrypt(rd_keyinstance *_key,
	const void *_input, void *_output, size_t _length, void *_IV);

ssize_t rijndael_cfb1_decrypt(rd_keyinstance *_key,
	const void *_input, void *_output, size_t _length, void *_IV);

ssize_t rijndael_cfb8_encrypt(rd_keyinstance *_key,
	const void *_input, void *_output, size_t _length, void *_IV);

ssize_t rijndael_cfb8_decrypt(rd_keyinstance *_key,
	const void *_input, void *_output, size_t _length, void *_IV);

ssize_t rijndael_pad(void *_input, size_t _length);

ssize_t rijndael_unpad(const void *_input, size_t _length);

typedef ssize_t (*rd_function)(rd_keyinstance *_key,
	const void *_input, void *_output, size_t _length, void *_IV);

#ifdef INTERMEDIATE_VALUE_KAT

void cipherEncryptUpdateRounds(rd_keyinstance *key,
	const void *input, void *output, int rounds);

void cipherDecryptUpdateRounds(rd_keyinstance *key,
	const void *input, void *output, int rounds);

#endif /* INTERMEDIATE_VALUE_KAT */

#endif /* _CRYPTO__RIJNDAEL_H */

/*
 * $PchId: rijndael.h,v 1.1 2005/06/01 10:13:45 philip Exp $
 */

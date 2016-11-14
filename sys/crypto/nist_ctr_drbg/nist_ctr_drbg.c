/*	$NetBSD: nist_ctr_drbg.c,v 1.1 2011/11/19 22:51:22 tls Exp $ */

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Thor Lancelot Simon.
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
 * Copyright (c) 2007 Henric Jungheim <software@henric.info>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * NIST SP 800-90 CTR_DRBG (Random Number Generator)
 */
#include <sys/types.h>
#include <sys/systm.h>

#include <crypto/nist_ctr_drbg/nist_ctr_drbg.h>

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nist_ctr_drbg.c,v 1.1 2011/11/19 22:51:22 tls Exp $");

/*
 * NIST SP 800-90 March 2007
 * 10.4.2 Derivation Function Using a Block Cipher Algorithm
 * Global Constants
 */
static NIST_Key nist_cipher_df_ctx;
static unsigned char nist_cipher_df_encrypted_iv[NIST_BLOCK_SEEDLEN / NIST_BLOCK_OUTLEN][NIST_BLOCK_OUTLEN_BYTES];

/*
 * NIST SP 800-90 March 2007
 * 10.2.1.3.2 The Process Steps for Instantiation When a Derivation
 *            Function is Used
 * Global Constants
 */
static NIST_Key nist_cipher_zero_ctx;

/*
 * NIST SP 800-90 March 2007
 * 10.2.1.5.2 The Process Steps for Generating Pseudorandom Bits When a
 *            Derivation Function is Used for the DRBG Implementation
 * Global Constants
 */
static const unsigned int
    nist_ctr_drgb_generate_null_input[NIST_BLOCK_SEEDLEN_INTS] = { 0 };

/*
 * Utility
 */
/*
 * nist_increment_block
 *    Increment the output block as a big-endian number.
 */
static inline void
nist_increment_block(unsigned long *V)
{
	int i;
	unsigned long x;

	for (i = NIST_BLOCK_OUTLEN_LONGS - 1; i >= 0; --i) {
		x = NIST_NTOHL(V[i]) + 1;
		V[i] = NIST_HTONL(x);
		if (x)	/* There was only a carry if we are zero */
			return;
	}
}

/*
 * NIST SP 800-90 March 2007
 * 10.4.3 BCC Function
 */
static void
nist_ctr_drbg_bcc_update(const NIST_Key *ctx, const unsigned int *data,
			 int n, unsigned int *chaining_value)
{
	int i, j;
	unsigned int input_block[NIST_BLOCK_OUTLEN_INTS];

	/* [4] for i = 1 to n */
	for (i = 0; i < n; ++i) {

		/* [4.1] input_block = chaining_value XOR block_i */
		for (j = 0; j < NIST_BLOCK_OUTLEN_INTS; ++j)
			input_block[j] = chaining_value[j] ^ *data++;

		/* [4.2] chaining_value = Block_Encrypt(Key, input_block) */
		Block_Encrypt(ctx, &input_block[0], &chaining_value[0]);
	}

	/* [5] output_block = chaining_value */
	/* chaining_value already is output_block, so no copy is required */
}

static void
nist_ctr_drbg_bcc(NIST_Key *ctx, const unsigned int *data,
		  int n, unsigned int *output_block)
{
	unsigned int *chaining_value = output_block;

	/* [1] chaining_value = 0^outlen */
	memset(&chaining_value[0], 0, NIST_BLOCK_OUTLEN_BYTES);

	nist_ctr_drbg_bcc_update(ctx, data, n, output_block);
}

/*
 * NIST SP 800-90 March 2007
 * 10.4.2 Derivation Function Using a Block Cipher Algorithm
 */

typedef struct {
	int index;
	unsigned char S[NIST_BLOCK_OUTLEN_BYTES];
} NIST_CTR_DRBG_DF_BCC_CTX;

static inline int
check_int_alignment(const void *p)
{
	intptr_t ip = (const char *)p - (const char *)0;

	if (ip & (sizeof(int) - 1))
		return 0;
	
	return 1;
}

static void
nist_ctr_drbg_df_bcc_init(NIST_CTR_DRBG_DF_BCC_CTX *ctx, int L, int N)
{
	unsigned int *S = (unsigned int *)ctx->S;

	/* [4] S = L || N || input_string || 0x80 */
	S[0] = NIST_HTONL(L);
	S[1] = NIST_HTONL(N);
	ctx->index = 2 * sizeof(S[0]);
}

static void
nist_ctr_drbg_df_bcc_update(NIST_CTR_DRBG_DF_BCC_CTX *ctx,
			    const char *input_string,
			    int input_string_length, unsigned int *temp)
{
	int i, len;
	int index = ctx->index;
	unsigned char *S = ctx->S;

	if (index) {
		KASSERT(index < NIST_BLOCK_OUTLEN_BYTES);
		len = NIST_BLOCK_OUTLEN_BYTES - index;
		if (input_string_length < len)
			len = input_string_length;
		
		memcpy(&S[index], input_string, len);

		index += len;
		input_string += len;
		input_string_length -= len;

		if (index < NIST_BLOCK_OUTLEN_BYTES) {
			ctx->index = index;

			return;
		}

		/* We have a full block in S, so let's process it */
		/* [9.2] BCC */
		nist_ctr_drbg_bcc_update(&nist_cipher_df_ctx,
				         (unsigned int *)&S[0], 1, temp);
		index = 0;
	}

	/* ctx->S is empty, so let's handle as many input blocks as we can */
	len = input_string_length / NIST_BLOCK_OUTLEN_BYTES;
	if (len > 0) {
		if (check_int_alignment(input_string)) {
			/* [9.2] BCC */
			nist_ctr_drbg_bcc_update(&nist_cipher_df_ctx,
						 (const unsigned int *)
						 input_string, len, temp);

			input_string += len * NIST_BLOCK_OUTLEN_BYTES;
			input_string_length -= len * NIST_BLOCK_OUTLEN_BYTES;
		} else {
			for (i = 0; i < len; ++i) {
				memcpy(&S[0], input_string,
				       NIST_BLOCK_OUTLEN_BYTES);

				/* [9.2] BCC */
				nist_ctr_drbg_bcc_update(&nist_cipher_df_ctx,
						         (unsigned int *)
							 &S[0], 1, temp);

				input_string += NIST_BLOCK_OUTLEN_BYTES;
				input_string_length -= NIST_BLOCK_OUTLEN_BYTES;
			}
		}
	}

	KASSERT(input_string_length < NIST_BLOCK_OUTLEN_BYTES);

	if (input_string_length) {
		memcpy(&S[0], input_string, input_string_length);
		index = input_string_length;
	}

	ctx->index = index;
}

static void
nist_ctr_drbg_df_bcc_final(NIST_CTR_DRBG_DF_BCC_CTX *ctx, unsigned int *temp)
{
	int index;
	unsigned char* S = ctx->S;
	static const char endmark[] = { 0x80 };

	nist_ctr_drbg_df_bcc_update(ctx, endmark, sizeof(endmark), temp);

	index = ctx->index;
	if (index) {
		memset(&S[index], 0, NIST_BLOCK_OUTLEN_BYTES - index);

		/* [9.2] BCC */
		nist_ctr_drbg_bcc_update(&nist_cipher_df_ctx,
					 (unsigned int *)&S[0], 1, temp);
	}
}

static int
nist_ctr_drbg_block_cipher_df(const char *input_string[], unsigned int L[],
			      int input_string_count,
			      unsigned char *output_string, unsigned int N)
{
	int j, k, blocks, sum_L;
	unsigned int *temp;
	unsigned int *X;
	NIST_Key ctx;
	NIST_CTR_DRBG_DF_BCC_CTX df_bcc_ctx;
	unsigned int buffer[NIST_BLOCK_SEEDLEN_INTS];
	/*
	 * NIST SP 800-90 March 2007 10.4.2 states that 512 bits is
	 * the maximum length for the approved block cipher algorithms.
	 */
	unsigned int output_buffer[512 / 8 / sizeof(unsigned int)];

	if (N > sizeof(output_buffer) || N < 1)
		return 0;

	sum_L = 0;
	for (j = 0; j < input_string_count; ++j)
		sum_L += L[j];

	/* [6] temp = Null string */
	temp = buffer;

	/* [9] while len(temp) < keylen + outlen, do */
	for (j = 0; j < NIST_BLOCK_SEEDLEN / NIST_BLOCK_OUTLEN; ++j) {
		/* [9.2] temp = temp || BCC(K, (IV || S)) */

		/* Since we have precomputed BCC(K, IV), we start with that... */ 
		memcpy(&temp[0], &nist_cipher_df_encrypted_iv[j][0],
		       NIST_BLOCK_OUTLEN_BYTES);

		nist_ctr_drbg_df_bcc_init(&df_bcc_ctx, sum_L, N);

		/* Compute the rest of BCC(K, (IV || S)) */
		for (k = 0; k < input_string_count; ++k)
			nist_ctr_drbg_df_bcc_update(&df_bcc_ctx,
						    input_string[k],
						    L[k], temp);

		nist_ctr_drbg_df_bcc_final(&df_bcc_ctx, temp);

		temp += NIST_BLOCK_OUTLEN_INTS;
	}

	nist_zeroize(&df_bcc_ctx, sizeof(df_bcc_ctx));

	/* [6] temp = Null string */
	temp = buffer;

	/* [10] K = Leftmost keylen bits of temp */
	Block_Schedule_Encryption(&ctx, &temp[0]);

	/* [11] X = next outlen bits of temp */
	X = &temp[NIST_BLOCK_KEYLEN_INTS];

	/* [12] temp = Null string */
	temp = output_buffer;

	/* [13] While len(temp) < number_of_bits_to_return, do */
	blocks = (int)(N / NIST_BLOCK_OUTLEN_BYTES);
	if (N & (NIST_BLOCK_OUTLEN_BYTES - 1))
		++blocks;
	for (j = 0; j < blocks; ++j) {
		/* [13.1] X = Block_Encrypt(K, X) */
		Block_Encrypt(&ctx, X, temp);
		X = temp;
		temp += NIST_BLOCK_OUTLEN_INTS;
	}

	/* [14] requested_bits = Leftmost number_of_bits_to_return of temp */
	memcpy(output_string, output_buffer, N);

	nist_zeroize(&ctx, sizeof(ctx));

	return 0;
}


static int
nist_ctr_drbg_block_cipher_df_initialize(void)
{
	int i, err;
	unsigned char K[NIST_BLOCK_KEYLEN_BYTES];
	unsigned int IV[NIST_BLOCK_OUTLEN_INTS];

	/* [8] K = Leftmost keylen bits of 0x00010203 ... 1D1E1F */
	for (i = 0; i < sizeof(K); ++i)
		K[i] = (unsigned char)i;

	err = Block_Schedule_Encryption(&nist_cipher_df_ctx, K);
	if (err)
		return err;

	/*
	 * Precompute the partial BCC result from encrypting the IVs:
	 *     nist_cipher_df_encrypted_iv[i] = BCC(K, IV(i))
	 */

	/* [7] i = 0 */
	/* [9.1] IV = i || 0^(outlen - len(i)) */
	memset(&IV[0], 0, sizeof(IV));

		/* [9.3] i = i + 1 */
	for (i = 0; i < NIST_BLOCK_SEEDLEN / NIST_BLOCK_OUTLEN; ++i) {

		/* [9.1] IV = i || 0^(outlen - len(i)) */
		IV[0] = NIST_HTONL(i);

		/*
		 * [9.2] temp = temp || BCC(K, (IV || S))
		 *	 (the IV part, at least)
		 */
		nist_ctr_drbg_bcc(&nist_cipher_df_ctx, &IV[0], 1,
				  (unsigned int *)
				  &nist_cipher_df_encrypted_iv[i][0]); 
	}

	return 0;
}

/*
 * NIST SP 800-90 March 2007
 * 10.2.1.2 The Update Function
 */
static void
nist_ctr_drbg_update(NIST_CTR_DRBG *drbg, const unsigned int *provided_data)
{
	int i;
	unsigned int temp[NIST_BLOCK_SEEDLEN_INTS];
	unsigned int* output_block;

	/* 2. while (len(temp) < seedlen) do */
	for (output_block = temp;
	     output_block < &temp[NIST_BLOCK_SEEDLEN_INTS];
	     output_block += NIST_BLOCK_OUTLEN_INTS) {

		/* 2.1 V = (V + 1) mod 2^outlen */
		nist_increment_block((unsigned long *)&drbg->V[0]);

		/* 2.2 output_block = Block_Encrypt(K, V) */
		Block_Encrypt(&drbg->ctx, drbg->V, output_block);
	}

	/* 3 temp is already of size seedlen (NIST_BLOCK_SEEDLEN_INTS) */

	/* 4 (part 1) temp = temp XOR provided_data */
	for (i = 0; i < NIST_BLOCK_KEYLEN_INTS; ++i)
		temp[i] ^= *provided_data++;

	/* 5 Key = leftmost keylen bits of temp */
	Block_Schedule_Encryption(&drbg->ctx, &temp[0]);

	/* 4 (part 2) combined with 6 V = rightmost outlen bits of temp */
	for (i = 0; i < NIST_BLOCK_OUTLEN_INTS; ++i)
		drbg->V[i] =
		    temp[NIST_BLOCK_KEYLEN_INTS + i] ^ *provided_data++;
}

/*
 * NIST SP 800-90 March 2007
 * 10.2.1.3.2 The Process Steps for Instantiation When a Derivation
 *            Function is Used
 */
int
nist_ctr_drbg_instantiate(NIST_CTR_DRBG* drbg,
	const void *entropy_input, int entropy_input_length,
	const void *nonce, int nonce_length,
	const void *personalization_string, int personalization_string_length)
{
	int err, count;
	unsigned int seed_material[NIST_BLOCK_SEEDLEN_INTS];
	unsigned int length[3];
	const char *input_string[3];

	/* [1] seed_material = entropy_input ||
	 *     nonce || personalization_string
	 */
	
	input_string[0] = entropy_input;
	length[0] = entropy_input_length;

	input_string[1] = nonce;
	length[1] = nonce_length;

	count = 2;
	if (personalization_string) {
		input_string[count] = personalization_string;
		length[count] = personalization_string_length;
		++count;
	}
	/* [2] seed_material = Block_Cipher_df(seed_material, seedlen) */
	err = nist_ctr_drbg_block_cipher_df(input_string, length, count,
					    (unsigned char *)seed_material,
					    sizeof(seed_material));
	if (err)
		return err;

	/* [3] Key = 0^keylen */
	memcpy(&drbg->ctx, &nist_cipher_zero_ctx, sizeof(drbg->ctx));

	/* [4] V = 0^outlen */
	memset(&drbg->V, 0, sizeof(drbg->V));

	/* [5] (Key, V) = Update(seed_material, Key, V) */
	nist_ctr_drbg_update(drbg, seed_material);

	/* [6] reseed_counter = 1 */
	drbg->reseed_counter = 1;

	return 0;
}

static int
nist_ctr_drbg_instantiate_initialize(void)
{
	int err;
	unsigned char K[NIST_BLOCK_KEYLEN_BYTES];

	memset(&K[0], 0, sizeof(K));

	err = Block_Schedule_Encryption(&nist_cipher_zero_ctx, &K[0]);

	return err;
}

/*
 * NIST SP 800-90 March 2007
 * 10.2.1.4.2 The Process Steps for Reseeding When a Derivation
 *            Function is Used
 */
int
nist_ctr_drbg_reseed(NIST_CTR_DRBG *drbg,
		     const void *entropy_input, int entropy_input_length,
		     const void *additional_input,
		     int additional_input_length)
{
	int err, count;
	const char *input_string[2];
	unsigned int length[2];
	unsigned int seed_material[NIST_BLOCK_SEEDLEN_INTS];

	/* [1] seed_material = entropy_input || additional_input */
	input_string[0] = entropy_input;
	length[0] = entropy_input_length;
	count = 1;

	if (additional_input) {
		input_string[count] = additional_input;
		length[count] = additional_input_length;
		
		++count;
	}
	/* [2] seed_material = Block_Cipher_df(seed_material, seedlen) */
	err = nist_ctr_drbg_block_cipher_df(input_string, length, count,
					    (unsigned char *)seed_material,
					    sizeof(seed_material));
	if (err)
		return err;

	/* [3] (Key, V) = Update(seed_material, Key, V) */
	nist_ctr_drbg_update(drbg, seed_material);

	/* [4] reseed_counter = 1 */
	drbg->reseed_counter = 1;

	return 0;
}

/*
 * NIST SP 800-90 March 2007
 * 10.2.1.5.2 The Process Steps for Generating Pseudorandom Bits When a
 *            Derivation Function is Used for the DRBG Implementation
 */
static void
nist_ctr_drbg_generate_block(NIST_CTR_DRBG *drbg, unsigned int *output_block)
{

	/* [4.1] V = (V + 1) mod 2^outlen */
	nist_increment_block((unsigned long *)&drbg->V[0]);

	/* [4.2] output_block = Block_Encrypt(Key, V) */
	Block_Encrypt(&drbg->ctx, &drbg->V[0], output_block);

}

int
nist_ctr_drbg_generate(NIST_CTR_DRBG * drbg,
		       void *output_string, int output_string_length,
		       const void *additional_input,
		       int additional_input_length)
{
	int i, len, err;
	int blocks = output_string_length / NIST_BLOCK_OUTLEN_BYTES;
	unsigned char* p;
	unsigned int* temp;
	const char *input_string[1];
	unsigned int length[1];
	unsigned int buffer[NIST_BLOCK_OUTLEN_BYTES];
	unsigned int additional_input_buffer[NIST_BLOCK_SEEDLEN_INTS];
	int ret = 0;

	if (output_string_length < 1)
		return 1;

	/* [1] If reseed_counter > reseed_interval ... */
	if (drbg->reseed_counter >= NIST_CTR_DRBG_RESEED_INTERVAL) {
		ret = 1;
		goto out;
	}

	/* [2] If (addional_input != Null), then */
	if (additional_input) {
		input_string[0] = additional_input;
		length[0] = additional_input_length;
		/*
		 * [2.1] additional_input =
		 * 		Block_Cipher_df(additional_input, seedlen)
		 */
		err = nist_ctr_drbg_block_cipher_df(input_string, length, 1,
		    (unsigned char *)additional_input_buffer,
		    sizeof(additional_input_buffer));
		if (err) {
			ret = err;
			goto out;
		}

		/* [2.2] (Key, V) = Update(additional_input, Key, V) */
		nist_ctr_drbg_update(drbg, additional_input_buffer);
	}

	if (blocks && check_int_alignment(output_string)) {
		/* [3] temp = Null */
		temp = (unsigned int *)output_string;
		for (i = 0; i < blocks; ++i) {
			nist_ctr_drbg_generate_block(drbg, temp);

			temp += NIST_BLOCK_OUTLEN_INTS;
			output_string_length -= NIST_BLOCK_OUTLEN_BYTES;
		}

		output_string = (unsigned char *)temp;
	}
	
	/* [3] temp = Null */
	temp = buffer;

	len = NIST_BLOCK_OUTLEN_BYTES;

	/* [4] While (len(temp) < requested_number_of_bits) do: */
	p = output_string;
	while (output_string_length > 0) {
		nist_ctr_drbg_generate_block(drbg, temp);

		if (output_string_length < NIST_BLOCK_OUTLEN_BYTES)
			len = output_string_length;

		memcpy(p, temp, len);

		p += len;
		output_string_length -= len;
	}

	/* [6] (Key, V) = Update(additional_input, Key, V) */
	nist_ctr_drbg_update(drbg, additional_input ?
		&additional_input_buffer[0] :
		&nist_ctr_drgb_generate_null_input[0]);

	/* [7] reseed_counter = reseed_counter + 1 */
	++drbg->reseed_counter;

out:
	return ret;
}

int
nist_ctr_initialize(void)
{
	int err;

	err = nist_ctr_drbg_instantiate_initialize();
	if (err)
		return err;
	err = nist_ctr_drbg_block_cipher_df_initialize();
	if (err)
		return err;

	return 0;
}

int
nist_ctr_drbg_destroy(NIST_CTR_DRBG* drbg)
{
	nist_zeroize(drbg, sizeof(*drbg));
	drbg->reseed_counter = ~0U;
	return 1;
}

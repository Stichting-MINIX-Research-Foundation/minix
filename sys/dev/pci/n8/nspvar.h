/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Coyote Point Systems, Inc.
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

#ifndef _NSPVAR_H_
#define _NSPVAR_H_

#define NSP_MAX_KEYLEN		512
#define NSP_MAX_INSTANCES	1

/*
 * crypto key operation request
 * holds the input and output parameter memory and
 * keying information for the asymmetric operations
 */
typedef struct {
	uint32_t magic;
	struct cryptkop *krp;
	N8_Status_t error;
	N8_SizedBuffer_t parm[CRK_MAXPARAM];
	N8_Buffer_t value[CRK_MAXPARAM][NSP_MAX_KEYLEN];	/* BN values, 4KB */
	union {
		struct {
			N8_DSAKeyObject_t   key;
			N8_DSAKeyMaterial_t keymaterial;
			N8_Boolean_t	    verifyok;		/* DSAVerify result */
		} dsa;
		struct {
			N8_RSAKeyObject_t   key;
			N8_RSAKeyMaterial_t keymaterial;
		} rsa;
		struct {
			N8_DH_KeyObject_t key;
			N8_DH_KeyMaterial_t keymaterial;
		} dh;
	} op;
} n8_kreq_t;


/*
 * Parameter checks for crypto key operations
 * indexed by crypyto key id (CRK_*) 
 */
#define BN_ARG(n)	(1<<(n))

/* inputs: dgst dsa->p dsa->q dsa->g dsa->priv_key */
#define NSP_DSA_SIGN_DIGEST	0
#define NSP_DSA_SIGN_P		1
#define NSP_DSA_SIGN_Q		2
#define NSP_DSA_SIGN_G		3
#define NSP_DSA_SIGN_X		4	/* Private Key */
#define NSP_DSA_SIGN_RVALUE	5	/* Result */
#define NSP_DSA_SIGN_SVALUE	6	/* Result */
#define NSP_DSA_SIGN_BIGNUMS	\
	(BN_ARG(NSP_DSA_SIGN_P) | \
	 BN_ARG(NSP_DSA_SIGN_Q) | \
	 BN_ARG(NSP_DSA_SIGN_G) | \
	 BN_ARG(NSP_DSA_SIGN_X) | \
	 BN_ARG(NSP_DSA_SIGN_RVALUE) | \
	 BN_ARG(NSP_DSA_SIGN_SVALUE))

/* inputs: dgst dsa->p dsa->q dsa->g dsa->pub_key sig->r sig->s */
#define NSP_DSA_VERIFY_DIGEST	0
#define NSP_DSA_VERIFY_P	1
#define NSP_DSA_VERIFY_Q	2
#define NSP_DSA_VERIFY_G	3
#define NSP_DSA_VERIFY_Y	4	/* Public Key */
#define NSP_DSA_VERIFY_RVALUE	5
#define NSP_DSA_VERIFY_SVALUE	6
#define NSP_DSA_VERIFY_BIGNUMS \
	(BN_ARG(NSP_DSA_VERIFY_P) | \
	 BN_ARG(NSP_DSA_VERIFY_Q) | \
	 BN_ARG(NSP_DSA_VERIFY_G) | \
	 BN_ARG(NSP_DSA_VERIFY_P) | \
	 BN_ARG(NSP_DSA_VERIFY_Y) | \
	 BN_ARG(NSP_DSA_VERIFY_RVALUE) | \
	 BN_ARG(NSP_DSA_VERIFY_SVALUE))

/* inputs: dh->priv_key (x) pub_key (g^x) dh->p (prime) key output g^x */
#define NSP_DH_COMPUTE_KEY_PRIV	0	/* priv_key */
#define NSP_DH_COMPUTE_KEY_PUB	1	/* pub_key (g^b % p) */
#define NSP_DH_COMPUTE_KEY_P	2	/* Prime (modulus) */
#define NSP_DH_COMPUTE_KEY_K    3	/* Result: key = B^g % p */
#define NSP_DH_COMPUTE_KEY_BIGNUMS \
	(BN_ARG(NSP_DH_COMPUTE_KEY_PRIV) | \
	 BN_ARG(NSP_DH_COMPUTE_KEY_PUB) | \
	 BN_ARG(NSP_DH_COMPUTE_KEY_P))

/* R0 = (A^B) % M */
#define NSP_MOD_EXP_A	0
#define NSP_MOD_EXP_B	1
#define NSP_MOD_EXP_M	2	/* modulus */
#define NSP_MOD_EXP_R0	3	/* Result */
#define NSP_MOD_EXP_BIGNUMS	\
	(BN_ARG(NSP_MOD_EXP_A) | \
	 BN_ARG(NSP_MOD_EXP_B) | \
	 BN_ARG(NSP_MOD_EXP_M) | \
	 BN_ARG(NSP_MOD_EXP_R0))


/* R0 = (A+B) % M */
#define NSP_MOD_ADD_A		0
#define NSP_MOD_ADD_B		1
#define NSP_MOD_ADD_M		2
#define NSP_MOD_ADD_R0		3
#define NSP_MOD_ADD_BIGNUMS \
	(BN_ARG(NSP_MOD_ADD_A) | \
	 BN_ARG(NSP_MOD_ADD_B) | \
	 BN_ARG(NSP_MOD_ADD_M) | \
	 BN_ARG(NSP_MOD_ADD_R0))

/* R0 = -A % M */
#define NSP_MOD_ADDINV_A	0
#define NSP_MOD_ADDINV_M	1
#define NSP_MOD_ADDINV_R0	2
#define NSP_MOD_ADDINV_BIGNUMS \
	(BN_ARG(NSP_MOD_ADDINV_A) | \
	 BN_ARG(NSP_MOD_ADDINV_M) | \
	 BN_ARG(NSP_MOD_ADDINV_R0))

/* R0 = (A-B) % M */
#define NSP_MOD_SUB_A		0
#define NSP_MOD_SUB_B		1
#define NSP_MOD_SUB_M		2
#define NSP_MOD_SUB_R0		3
#define NSP_MOD_SUB_BIGNUMS \
	(BN_ARG(NSP_MOD_SUB_A) | \
	 BN_ARG(NSP_MOD_SUB_B) | \
	 BN_ARG(NSP_MOD_SUB_M) | \
	 BN_ARG(NSP_MOD_SUB_R0))

/* R0 = (A*B) % M */
#define NSP_MOD_MULT_A		0
#define NSP_MOD_MULT_B		1
#define NSP_MOD_MULT_M		2
#define NSP_MOD_MULT_R0		3
#define NSP_MOD_MULT_BIGNUMS \
	(BN_ARG(NSP_MOD_MULT_A) | \
	 BN_ARG(NSP_MOD_MULT_B) | \
	 BN_ARG(NSP_MOD_MULT_M) | \
	 BN_ARG(NSP_MOD_MULT_R0))

/* R0 = (A^-1) % M */
#define NSP_MOD_MULTINV_A	0
#define NSP_MOD_MULTINV_M	1
#define NSP_MOD_MULTINV_R0	2
#define NSP_MOD_MULTINV_BIGNUMS \
	(BN_ARG(NSP_MOD_MULTINV_A) | \
	 BN_ARG(NSP_MOD_MULTINV_M) | \
	 BN_ARG(NSP_MOD_MULTINV_R0))

/* R0 = A % M */
#define NSP_MODULUS_A	0
#define NSP_MODULUS_M	1
#define NSP_MODULUS_R0	2
#define NSP_MODULUS_BIGNUMS \
	(BN_ARG(NSP_MODULUS_A) | \
	 BN_ARG(NSP_MODULUS_M) | \
	 BN_ARG(NSP_MODULUS_R0))

/* inputs: rsa->p rsa->q I rsa->dmp1 rsa->dmq1 rsa->iqmp */
#define NSP_MOD_EXP_CRT_P	0
#define NSP_MOD_EXP_CRT_Q	1
#define NSP_MOD_EXP_CRT_I	2
#define NSP_MOD_EXP_CRT_DP	3
#define NSP_MOD_EXP_CRT_DQ	4
#define NSP_MOD_EXP_CRT_QINV	5
#define NSP_MOD_EXP_CRT_R0	6	/* Result */
#define NSP_MOD_EXP_CRT_BIGNUMS	\
	(BN_ARG(NSP_MOD_EXP_CRT_P) | \
	 BN_ARG(NSP_MOD_EXP_CRT_Q) | \
	 BN_ARG(NSP_MOD_EXP_CRT_I) | \
	 BN_ARG(NSP_MOD_EXP_CRT_DP) | \
	 BN_ARG(NSP_MOD_EXP_CRT_DQ) | \
	 BN_ARG(NSP_MOD_EXP_CRT_QINV) | \
	 BN_ARG(NSP_MOD_EXP_CRT_R0))

#endif /* _NSPVAR_H_ */

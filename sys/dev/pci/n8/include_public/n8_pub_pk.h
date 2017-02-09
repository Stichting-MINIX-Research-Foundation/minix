/*-
 * Copyright (C) 2001-2003 by NBMK Encryption Technologies.
 * All rights reserved.
 *
 * NBMK Encryption Technologies provides no support of any kind for
 * this software.  Questions or concerns about it may be addressed to
 * the members of the relevant open-source community at
 * <tech-crypto@netbsd.org>.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*****************************************************************************
 * @(#) n8_pub_pk.h 1.18@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_pub_pk
 *  @brief Public declarations for public key operations.
 *
 * Public header file for public key operations.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 12/17/03 bac   Added prototype for N8_RSAPublicDecrypt.
 * 05/16/03 brr   Moved DH modulus size constants to this public include file.
 *                Fix Bug 902 by allocating a kernel buffer to hold key,
 *                modulus etc. to avoid copies in N8_DHCompute.
 * 08/01/02 bac   Documented 'modulusSize' for N8_DSAKeyObject_t is in bytes.
 * 05/15/02 mww   Moved over prototypes from n8_pk_ops.h to this header
 * 11/24/01 bac   Changed the signature of N8_DSAVerify to add 'const' where
 *                appropriate. 
 * 11/06/01 hml   Added structureID field to RSA, DSA and DH key objects.
 * 10/12/01 dkm   Original version.
 ****************************************************************************/
#ifndef N8_PUB_PK_H
#define N8_PUB_PK_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "n8_pub_common.h"

/* Max size for PKE base operation operands in bytes */
#define N8_PK_MAX_SIZE          512

/* Modulus size limits for Diffie-Helmann */
#define N8_DH_MIN_MODULUS_SIZE             1       /* bytes */
#define N8_DH_MAX_MODULUS_SIZE             512     /* bytes */

/*****************************************************************************
 * Structures/type definitions
 *****************************************************************************/
/*
 * An RSA key object contains all of the state necessary to perform RSA
 * operations.  It is initialized by calling N8_RSAInitializeKey.  The user
 * should never use or access this structure directly.
 */
typedef struct 
{
   uint32_t               p;                /* secret prime number.  used for
                                             * private operations. */
   uint32_t               q;                /* secret prime number.  used for
                                             * private operations. */
                                            /* d is the private key */
   uint32_t               dp;               /* d mod ((p-1) mod p) */
   uint32_t               dq;               /* d mod ((q-1) mod q) */
   uint32_t               R_mod_p;          /* R mod p.  (private) */
   uint32_t               R_mod_q;          /* R mod q.  (private) */
   uint32_t               R_mod_n;          /* R mod n.  (public) */
   uint32_t               n;                /* modulus:  n=pq */
   uint32_t               u;                /* used for Chinese Remainder
                                             * Theorem: 
                                             * u = p^-1 mod q.  private
                                             * operations only. */
   uint32_t               cp;               /* additive inverse of the
                                             * multiplicative inverse of the
                                             * least significant digit of p, mod
                                             * 2^128.  used for private
                                             * operations. */  
   uint32_t               cq;               /* same as cp but for q.  used for
                                             * private operations. */
   uint32_t               cn;               /* same as cp but for n.  used for
                                             * public operations. */
   uint32_t               key;              /* this is either e, the public key or
                                             * d, the private key, depending upon
                                             * the context this object is being
                                             * used. */
   uint32_t               paramBlock;       /* parameter block in PK format,
                                             * suitable for loading directly into
                                             * the Big Number Cache (BNC). */
   N8_KeyType_t           keyType;
   unsigned int           publicKeyLength;  /* length in bytes of the public key. */
   unsigned int           privateKeyLength; /* length in bytes of the private
                                             * key. */
   unsigned int           publicKeyDigits;  /* length in digits of the public
                                             * key */ 
   unsigned int           privateKeyDigits; /* length in digits of the private
                                             * key */ 
   unsigned int           pLength;          /* length of p in bytes */
   unsigned int           qLength;          /* length of q in bytes */
   unsigned int           pDigits;          /* length of p in digits */
   unsigned int           qDigits;          /* length of q in digits */
   unsigned int           pPad;             /* length of p pad for the parameter
                                             * block */
   unsigned int           qPad;             /* length of q pad for the parameter
                                             * block */
   N8_MemoryHandle_t     *kmem_p;           /* kernel memory structure for the
                                             * allocated space */
   N8_Unit_t              unitID;           /* Unit id on which to execute this
                                               request */
   N8_SKSKeyHandle_t      SKSKeyHandle;     /* contains SKS info */
   unsigned int           structureID;      /* Id field for structure */
} N8_RSAKeyObject_t;

/* DSA */

typedef struct 
{
   N8_Buffer_t        *q;                /* prime divisor. */
   N8_Buffer_t        *cp;               /* additive inverse of the
                                            multiplicative inverse of the least
                                            significant digit of p, mod 2^128.
                                            used for private operations. */ 
   N8_Buffer_t        *gR_mod_p;         /* g*R mod q.  (private) */
   N8_Buffer_t        *p;                /* prime modulus. */
   N8_Buffer_t        *privateKey;       /* private key x */
   N8_Buffer_t        *publicKey;        /* public key y */
   N8_Buffer_t        *g;                /* generator = h^(p-1)/q mod p, where h
                                          * is less than p - 1 and g > 1. */
   N8_Buffer_t        *paramBlock;       /* parameter block in PK format,
                                            suitable for loading directly into
                                            the Big Num Cache (BNC) */
   N8_KeyType_t        keyType;
   unsigned int        modulusLength;    /* in bytes */
   N8_MemoryHandle_t  *kmem_p;           /* kernel memory structure for the
                                            allocated space */
   N8_Unit_t           unitID;           /* Unit id on which to execute this
                                            request */
   N8_SKSKeyHandle_t   SKSKeyHandle;     /* contains SKS info */
   unsigned int        structureID;      /* Id field for structure */
} N8_DSAKeyObject_t;

/* Diffie-Helmann */

typedef struct 
{
   N8_Buffer_t           *g;             /* group generator. */
   N8_Buffer_t           *p;             /* modulus p. */
   unsigned int           modulusLength; /* length in bytes of p and g */
   N8_Buffer_t           *R_mod_p;       /* intermediate result necessary for
                                          * exponentiation computations.  */
   N8_Buffer_t           *gR_mod_p;      /* intermediate value of g*R mod p.
                                          * only to be used when using the value 
                                          * for g in this object. */
   N8_Buffer_t           *cp;            /* intermediate result necessary for
                                          * exponentiation.  useful for
                                          * operations using this value
                                          * of p.  */
   N8_Unit_t              unitID;        /* Execution unit for request */
   unsigned int           structureID;   /* Id field for structure */
   uint32_t               g_a;           /* group generator. */
   uint32_t               p_a;           /* modulus_p.       */
   uint32_t               RmodP_a;
   uint32_t               gRmodP_a;
   uint32_t               cp_a;
   N8_MemoryHandle_t     *kmem_p;        /* kernel memory structure for the
                                            allocated space */

} N8_DH_KeyObject_t;

/*****************************************************************************
 * Function prototypes
 *****************************************************************************/
N8_Status_t N8_RSAInitializeKey(N8_RSAKeyObject_t*,
                                N8_KeyType_t,
                                N8_RSAKeyMaterial_t*,
                                N8_Event_t*);

N8_Status_t N8_RSAEncrypt(N8_RSAKeyObject_t *,
                          N8_Buffer_t *,
                          uint32_t,
                          N8_Buffer_t *,
                          N8_Event_t *);

N8_Status_t N8_RSADecrypt(N8_RSAKeyObject_t *,
                          N8_Buffer_t *,
                          uint32_t,
                          N8_Buffer_t *,
                          N8_Event_t *);

N8_Status_t N8_RSAPublicDecrypt(N8_RSAKeyObject_t   *key_p,
                                const N8_RSAKeyMaterial_t *material_p,
                                const N8_Buffer_t         *msgIn,
                                uint32_t             msgLength,
                                N8_Buffer_t         *msgOut,
                                N8_Event_t          *event_p);

N8_Status_t  N8_RSAFreeKey (N8_RSAKeyObject_t *RSAKeyObject_p);


/**********************************************************************
 * DSA function prototypes
 **********************************************************************/

N8_Status_t N8_DSAInitializeKey(N8_DSAKeyObject_t   *DSAKeyObject_p,
                                N8_KeyType_t         type,
                                N8_DSAKeyMaterial_t *material,
                                N8_Event_t          *event_p);

N8_Status_t N8_DSASign(N8_DSAKeyObject_t *DSAKeyObject_p,
                       N8_Buffer_t       *msgHash_p,
                       N8_Buffer_t       *rValue_p,
                       N8_Buffer_t       *sValue_p,
                       N8_Event_t        *event_p);

N8_Status_t N8_DSAVerify(const N8_DSAKeyObject_t *DSAKeyObject_p,
                         const N8_Buffer_t       *msgHash_p,
                         const N8_Buffer_t       *rValue_p,
                         const N8_Buffer_t       *sValue_p,
                         N8_Boolean_t            *verify,
                         N8_Event_t              *event_p);

N8_Status_t  N8_DSAFreeKey (N8_DSAKeyObject_t *DSAKeyObject_p);

/**********************************************************************
 * DH function prototypes
 **********************************************************************/
N8_Status_t N8_DHCompute(N8_DH_KeyObject_t *DHKeyObject_p,
                         N8_Buffer_t        *GValue,
                         N8_Buffer_t        *XValue,
                         N8_Buffer_t        *GXValue,
                         N8_Event_t         *event_p);

N8_Status_t N8_DHInitializeKey(N8_DH_KeyObject_t   *DHKeyObject_p,
                               N8_DH_KeyMaterial_t *material,
                               N8_Event_t          *event_p);

N8_Status_t N8_DHFreeKey(N8_DH_KeyObject_t *key_p);

/**********************************************************************
 * PKE Base Operation function prototypes
 **********************************************************************/
N8_Status_t N8_ModAdd(const N8_SizedBuffer_t *a_p,
                      const N8_SizedBuffer_t *b_p,
                      const N8_SizedBuffer_t *modulus_p,
                      N8_SizedBuffer_t *result_p,
                      const N8_Unit_t unitID,
                      N8_Event_t *event_p);

N8_Status_t N8_ModSubtract(const N8_SizedBuffer_t *a_p,
                           const N8_SizedBuffer_t *b_p,
                           const N8_SizedBuffer_t *modulus_p,
                           N8_SizedBuffer_t *result_p,
                           const N8_Unit_t unitID,
                           N8_Event_t *event_p);

N8_Status_t N8_ModMultiply(const N8_SizedBuffer_t *a_p,
                           const N8_SizedBuffer_t *b_p,
                           const N8_SizedBuffer_t *modulus_p,
                           N8_SizedBuffer_t *result_p,
                           const N8_Unit_t unitID,
                           N8_Event_t *event_p);

N8_Status_t N8_Modulus(const N8_SizedBuffer_t *a_p,
                       const N8_SizedBuffer_t *modulus_p,
                       N8_SizedBuffer_t *result_p,
                       const N8_Unit_t unitID,
                       N8_Event_t *event_p);

N8_Status_t N8_ModAdditiveInverse(const N8_SizedBuffer_t *a_p,
                                  const N8_SizedBuffer_t *modulus_p,
                                  N8_SizedBuffer_t *result_p,
                                  const N8_Unit_t unitID,
                                  N8_Event_t *event_p);

N8_Status_t N8_ModMultiplicativeInverse(const N8_SizedBuffer_t *a_p,
                                        const N8_SizedBuffer_t *modulus_p,
                                        N8_SizedBuffer_t *result_p,
                                        const N8_Unit_t unitID,
                                        N8_Event_t *event_p);

N8_Status_t N8_ModR(const N8_SizedBuffer_t *modulus_p,
                    N8_SizedBuffer_t *result_p,
                    const N8_Unit_t unitID,
                    N8_Event_t *event_p);

N8_Status_t N8_ModExponentiate(const N8_SizedBuffer_t *a_p,
                               const N8_SizedBuffer_t *b_p,
                               const N8_SizedBuffer_t *modulus_p,
                               N8_SizedBuffer_t *result_p,
                               const N8_Unit_t unitID,
                               N8_Event_t *event_p);
#ifdef __cplusplus
}
#endif

#endif

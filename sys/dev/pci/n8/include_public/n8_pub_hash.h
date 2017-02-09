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
 * @(#) n8_pub_hash.h 1.9@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_pub_hash
 *  @brief Public declarations for hash operations.
 *
 * Public header file for hash operations.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 01/20/04 bac   Fixed __cplusplus beginning directive (Bug 1002).
 * 04/17/03 brr   Moved hash size constants to this public include file. 
 *                (Bug 866)
 * 10/24/01 dkm   Moved HMAC defines from hash.h.
 * 10/15/01 bac   Fixed some signatures to take unsigned ints.
 * 10/12/01 dkm   Original version.
 ****************************************************************************/
#ifndef N8_PUB_HASH_H
#define N8_PUB_HASH_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "n8_pub_common.h"

/*****************************************************************************
 * #defines 
 *****************************************************************************/

#define N8_MAX_HASH_LENGTH (18 * 1024)
#define N8_MAX_KEY_LENGTH  (18 * 1024)
#define N8_MAX_SSL_KEY_MATERIAL_LENGTH 240
#define N8_MAX_TLS_KEY_MATERIAL_LENGTH 224
#define N8_MAX_IKE_ITERATIONS 15
#define N8_IKE_SKEYID_ITERATIONS 3
#define N8_IKE_PRF_ITERATIONS 1
#define N8_IKE_ZERO_BYTE_LEN 1

#define MD5_HASH_RESULT_LENGTH 16
#define SHA1_HASH_RESULT_LENGTH 20
#define HMAC_96_HASH_RESULT_LENGTH 12

/*****************************************************************************
 * Structures/type definitions
 *****************************************************************************/
#define N8_HMAC_KEY_LENGTH_ZERO 0
#define N8_NO_HMAC_KEY NULL

typedef enum
{
   N8_TLS_FINISH = 1,
   N8_TLS_CERT,
   N8_SSL_FINISH,
   N8_SSL_CERT
} N8_HashProtocol_t;

typedef enum
{
   N8_SERVER   = 1,
   N8_CLIENT
} N8_HashRole_t;

typedef struct
{
    uint32_t     keyLength;      /* HMAC key length  */
    N8_Buffer_t *key_p;          /* ptr to key       */
    N8_Unit_t    unitID;
} N8_HashInfo_t;

/*****************************************************************************
 * Function prototypes
 *****************************************************************************/
N8_Status_t N8_HashInitialize(N8_HashObject_t          *hashObj_p,
                              const N8_HashAlgorithm_t  alg,
                              const N8_HashInfo_t      *hashInfo_p,
                              N8_Event_t               *event_p);


N8_Status_t N8_HashPartial(N8_HashObject_t *obj_p,
                           const N8_Buffer_t *msg_p,
                           const unsigned int msgLength,
                           N8_Event_t *event_p);

N8_Status_t N8_HashEnd(N8_HashObject_t *obj_p,
                       N8_Buffer_t *result_p,
                       N8_Event_t *event_p);

N8_Status_t N8_HashClone(const N8_HashObject_t *orig_p,
                         N8_HashObject_t *clone_p);

N8_Status_t N8_HandshakeHashPartial(N8_HashObject_t *md5Obj_p,
                                    N8_HashObject_t *shaObj_p,
                                    const N8_Buffer_t *msg_p,
                                    const unsigned int msgLength,
                                    N8_Event_t *event_p);

N8_Status_t N8_HandshakeHashEnd(N8_HashObject_t *md5Obj_p,
                                N8_HashObject_t *sha1Obj_p,
                                const N8_HashProtocol_t protocol,
                                const N8_Buffer_t *key_p,
                                const unsigned int keyLength,
                                const N8_HashRole_t role,
                                N8_Buffer_t *md5Result_p,
                                N8_Buffer_t *sha1Result_p,
                                N8_Event_t *event_p);

N8_Status_t N8_SSLKeyMaterialHash (N8_HashInfo_t    *obj_p,
                                  const N8_Buffer_t *random_p,
                                  const unsigned int outputLength,
                                  N8_Buffer_t       *keyMaterial_p,
                                  N8_Event_t        *event_p);

N8_Status_t N8_TLSKeyMaterialHash(N8_HashInfo_t     *obj_p,
                                  const N8_Buffer_t *label_p,
                                  const unsigned int labelLength,
                                  const N8_Buffer_t *seed_p,
                                  const unsigned int seedLength,
                                  const unsigned int outputLength,
                                  N8_Buffer_t       *keyMaterial_p,
                                  N8_Event_t        *event_p);

N8_Status_t N8_IKEPrf(const N8_HashAlgorithm_t alg,
                      const N8_HashInfo_t *hashInfo_p,
                      const N8_Buffer_t *msg_p,
                      const uint32_t msgLength,
                      N8_Buffer_t *result_p,
                      N8_Event_t *event_p);

N8_Status_t N8_IKESKEYIDExpand (const N8_HashAlgorithm_t alg,
                                const N8_HashInfo_t *hashInfo_p,
                                const N8_Buffer_t *msg_p,
                                const uint32_t msgLength,
                                N8_Buffer_t *SKEYID_d,
                                N8_Buffer_t *SKEYID_a,
                                N8_Buffer_t *SKEYID_e,
                                N8_Event_t *event_p);

N8_Status_t N8_IKEKeyMaterialExpand(const N8_HashAlgorithm_t alg,
                                    const N8_HashInfo_t *hashInfo_p,
                                    const N8_Buffer_t *msg_p,
                                    const uint32_t msgLength,
                                    N8_Buffer_t *result_p,
                                    const uint32_t result_len,
                                    N8_Event_t *event_p);

N8_Status_t N8_IKEEncryptKeyExpand(const N8_HashAlgorithm_t alg,
                                   const N8_HashInfo_t *hashInfo_p,
                                   N8_Buffer_t *result_p,
                                   const uint32_t result_len,
                                   N8_Event_t *event_p);

N8_Status_t N8_HashCompleteMessage(N8_HashObject_t   *obj_p,
                                   const N8_Buffer_t *msg_p,
                                   const unsigned int msgLength,
                                   N8_Buffer_t       *result_p,
                                   N8_Event_t        *event_p);

N8_Status_t N8_HashCompleteMessage_uio(N8_HashObject_t *obj_p,
			           struct uio          *msg_p,
                                   const unsigned int   msgLength,
                                   N8_Buffer_t         *result_p,
                                   N8_Event_t          *event_p);

#ifdef __cplusplus
}
#endif

#endif



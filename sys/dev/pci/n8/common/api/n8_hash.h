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
 * @(#) n8_hash.h 1.45@(#)
 *****************************************************************************/

/*****************************************************************************
 * n8_hash.h
 *
 * Implementation of all public functions dealing with management of
 * hashs.
 *
 * References:
 * MD5: RFC1321 "The MD5 Message-Digest Algorithm", R. Rivest, 4/92
 * SHA-1: FIPS Pub 180-1,"Secure Hash Standard", US Dept. of Commerce,
 *    4/17/95
 *
 *****************************************************************************
 * Revision history:
 * 05/19/03 brr   Clean up include files.
 * 04/17/03 brr   Moved hash size constants to public include file. (Bug 866)
 * 01/12/02 bac   Added a prototype for the new methods n8_setInitialIVs and
 *                n8_initializeHMAC, corrected the signatures for initMD5 and
 *                initSHA1.  All of these are to support removal of blocking
 *                calls (BUG #313).
 * 11/12/01 hml   Added unitID to initSHA1 and initMD5 protos.
 * 10/24/01 dkm   Moved public portion to n8_pub_hash.h.
 * 10/16/01 spm   Added N8_64BYTE_IKE_KEY_LIMIT because IKE APIs currently
 *                only work for 64 byte keys.
 * 10/12/01 dkm   Moved public portion to n8_pub_common.h and n8_pub_hash.h.
 * 10/11/01 hml   Added prototype for N8_HashCompleteMessage and 
 *                several defines used in N8_HandshakeHashEnd.
 * 09/26/01 hml   Updated to support multiple chips.  The N8_HashInitialize,
 *                N8_TLSKeyMaterialHash and N8_SSLKeyMaterialHash were changed.
 * 09/08/01 spm   IKE APIs: Swapped order of alg and hashInfo args.
 * 09/07/01 spm   Added IKE API Extensions, hash info struct,
 *                N8_MAX_IKE_ITERATIONS
 * 09/06/01 bac   Renumbered enums to start with non-zero (BUG #190).
 * 06/25/01 bac   More changes for QMgr v.1.0.1
 * 06/25/01 mel   Fixed N8_MAX_KEY_LENGTH.
 * 06/05/01 mel   Added prototype for N8_TLSKeyMaterialHash.
 * 05/18/01 bac   Fixed spelling of N8_HMAC_KEY_LENGTH_ZERO.
 * 05/14/01 dws   Changed the byte order of the MD5 initialization values.
 * 05/10/01 bac   Changed N8_MAX_HASH_LENGTH to be 18Kbytes
 * 04/26/01 bac   Changed N8_HashAlgorithm_t to be zero-based and
 *                contiguous for use as an index.
 *                Added N8_HashProtocol_t.
 *                Added N8_HashRole_t.
 *                Specified 'const' where appropriate in prototypes.
 *                Added prototype for N8_HashClone.
 * 04/24/01 bac   Support for MD5 and SHA-1.
 * 03/29/01 bac   Original version.
 *
 ***************************************************************************/
#ifndef N8_HASH_H
#define N8_HASH_H

#include "n8_pub_common.h"
#include "n8_enqueue_common.h"
#include "n8_pub_hash.h"
#include "n8_pub_packet.h"
#include "n8_pub_errors.h"

typedef enum
{
   N8_IV   = 0,                 /* take the IV from the normal iv area */
   N8_IPAD = 1,                 /* take the IV from the ipadHMAC_iv */
   N8_OPAD = 2                  /* take the IV from the opadHMAC_iv */
} n8_IVSrc_t;

/*
 * Initialization values for MD5 per RFC1321 Section 3.3
 * these values may need to be adjusted vis a vis endianness.  These
 * assume a Big Endian host representation of the MD5 little endian
 * values.
 */
#define MD5_INIT_A 0x01234567
#define MD5_INIT_B 0x89abcdef
#define MD5_INIT_C 0xfedcba98
#define MD5_INIT_D 0x76543210

/*
 * Initialization values for SHA-1 per "Secure Hash Standard", Section 7.
 * These values may need to be adjusted vis a vis endianness. 
 */
#define SHA1_INIT_H0 0x67452301
#define SHA1_INIT_H1 0xefcdab89
#define SHA1_INIT_H2 0x98badcfe
#define SHA1_INIT_H3 0x10325476
#define SHA1_INIT_H4 0xc3d2e1f0

#define PAD1 0x36
#define PAD2 0x5C
#define TLS_FINISH_RESULT_LENGTH 12
#define NUM_WORDS_TLS_RESULT     4

/* TODO: handle the >64 byte key case:
 * key must be hashed and truncated
 * to 64 bytes
 */
#define N8_64BYTE_IKE_KEY_LIMIT

/* prototypes */
N8_Status_t n8_setInitialIVs(N8_HashObject_t *hashObj_p,
                             const N8_HashAlgorithm_t alg,
                             const N8_Unit_t  unit);
N8_Status_t initMD5(N8_HashObject_t *obj_p,
                    const N8_HashAlgorithm_t alg,
                    int unitID);
N8_Status_t initSHA1(N8_HashObject_t *obj_p,
                     const N8_HashAlgorithm_t alg,
                     int unitID);
N8_Status_t n8_HashPartial_req(N8_HashObject_t *obj_p,
                               const N8_Buffer_t *msg_p,
                               const unsigned int msgLength,
                               const n8_IVSrc_t ivSrc,
                               API_Request_t **req_pp);
N8_Status_t n8_HashEnd_req(N8_HashObject_t *obj_p,
                           N8_Buffer_t *result_p,
                           API_Request_t **req_pp);
N8_Status_t n8_HashCompleteMessage_req(N8_HashObject_t   *obj_p,
                                       const N8_Buffer_t *msg_p,
                                       const unsigned int msgLength,
                                       N8_Buffer_t       *result_p,
                                       const void        *resultHandler,
                                       API_Request_t    **req_pp);
N8_Status_t n8_HashCompleteMessage_req_uio(N8_HashObject_t *obj_p,
                                       struct uio          *msg_p,
                                       const unsigned int   msgLength,
                                       N8_Buffer_t         *result_p,
                                       const void          *resultHandler,
                                       API_Request_t      **req_pp);
N8_Status_t n8_initializeHMAC_req(N8_Buffer_t             *HMACKey, 
                                  uint32_t                 HMACKeyLength, 
                                  N8_HashObject_t         *hashObj_p,
                                  void                    *result_p,
                                  N8_Buffer_t            **ctx_pp,
                                  uint32_t                *ctxa_p,
                                  API_Request_t          **req_pp);
#endif /* N8_HASH_H */


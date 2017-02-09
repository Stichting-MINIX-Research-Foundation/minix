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
 * @(#) n8_packet.h 1.18@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_packet.h
 *  @brief Contains definitions for Packet Interface.
 *
 *
 *****************************************************************************/
 
/*****************************************************************************
 * Revision history:
 * 08/18/03 brr   Eliminate unused parameters.
 * 07/01/03 brr   Added option to use no hashing algorithm.
 * 05/20/03 brr   Added function ptr types for context load & enc/decrypte ops.
 * 04/17/03 brr   Moved key size constants to public include file.
 * 11/12/01 hml   Changed PI_PROTOCOL_* to N8_PROTOCOL_*.
 * 10/30/01 bac   Removed signatures for handleSSL and handleIPsec.
 * 10/12/01 dkm   Moved public portion to n8_pub_common.h and n8_pub_packet.h.
 * 10/01/01 hml   Added multi-unit support.
 * 09/14/01 msz   Moved some values to n8_common.h to remove a circular
 *                include dependency.
 * 09/07/01 bac   Removed unused defines USE_CONTEXT_MEMORY and
 *                DONOT_USE_CONTEXT_MEMORY
 * 09/06/01 bac   Renumbered enums to start with non-zero (BUG #190).
 * 07/26/01 mel   Added N8_RC4_t structure to make SAPI independent from open SSL.
 * 05/30/01 mel   Changed keys and IVs to be a char[] instead of uint32_t[].
 * 05/23/01 bac   Changed macSecret to be a char[] instead of uint32_t[].
 * 05/21/01 bac   Converted to use N8_ContextHandle_t and N8_Packet_t
 *                with integrated cipher and hash packet.
 * 05/18/01 mel   Change declaration hmacKeyLength to uint32_t.
 *                Change comments.
 * 05/11/01 bac   Fixed some merge problems and naming standardization.
 * 05/10/01 bac   Added preCompute values to keyDES_t, macro-ized computation
 *                of PROTOCOL/CIPHER/HASH, 
 * 05/09/01 bac   Added useContext to N8_Packet_t.
 * 05/04/01 mel   Change function declaration and name for 
 *                N8_PI_PacketInitialize. New name is N8_PacketInitialize
 * 04/18/01 mel   Original version.
 ****************************************************************************/

#ifndef N8_PACKET_H
#define N8_PACKET_H
#include "n8_common.h"
#include "n8_pub_packet.h"
#include "n8_hash.h"

typedef N8_Status_t (*n8_ctxLoadFcn_t)(API_Request_t           *req_p,
                                       EA_CMD_BLOCK_t          *cb_p,
                                       const N8_Packet_t       *packetObject_p,
                                       const N8_CipherInfo_t   *cipherInfo_p,
                                       const N8_HashAlgorithm_t hashAlgorithm,
                                       N8_Buffer_t             *ctx_p,
                                       const uint32_t           ctx_a,
                                       EA_CMD_BLOCK_t          **next_cb_pp);

typedef N8_Status_t (*n8_SSLTLSFcn_t)(EA_CMD_BLOCK_t *cb_p,
                                      N8_Packet_t *packetObj_p,
                                      const N8_SSLTLSPacket_t *packet_p,
                                      const uint32_t input_a,
                                      const uint32_t result_a,
                                      const unsigned int opCode);



/* protocol mask definitions*/
/* SSL */
#define    PROTOCOL_CIPHER_HASH(PROTO, CIPHER, HASH) \
  (PROTO | (CIPHER << 4) | (HASH << 8))
#define PACKET_SSL_ARC4_MD5  \
  PROTOCOL_CIPHER_HASH(N8_PROTOCOL_SSL,N8_CIPHER_ARC4,N8_MD5)
#define PACKET_SSL_ARC4_SHA1 \
  PROTOCOL_CIPHER_HASH(N8_PROTOCOL_SSL,N8_CIPHER_ARC4,N8_SHA1)
#define PACKET_SSL_DES_MD5   \
  PROTOCOL_CIPHER_HASH(N8_PROTOCOL_SSL,N8_CIPHER_DES,N8_MD5)
#define PACKET_SSL_DES_SHA1  \
  PROTOCOL_CIPHER_HASH(N8_PROTOCOL_SSL,N8_CIPHER_DES,N8_SHA1)
/* TLS */
#define PACKET_TLS_ARC4_HMAC_MD5  \
  PROTOCOL_CIPHER_HASH(N8_PROTOCOL_TLS,N8_CIPHER_ARC4,N8_HMAC_MD5) 
#define PACKET_TLS_ARC4_HMAC_SHA1 \
  PROTOCOL_CIPHER_HASH(N8_PROTOCOL_TLS,N8_CIPHER_ARC4,N8_HMAC_SHA1) 
#define PACKET_TLS_DES_HMAC_MD5   \
  PROTOCOL_CIPHER_HASH(N8_PROTOCOL_TLS,N8_CIPHER_DES,N8_HMAC_MD5) 
#define PACKET_TLS_DES_HMAC_SHA1  \
  PROTOCOL_CIPHER_HASH(N8_PROTOCOL_TLS,N8_CIPHER_DES,N8_HMAC_SHA1)
/* IPSec */
#define PACKET_IPSEC_DES_HMAC_MD5_96  \
  PROTOCOL_CIPHER_HASH(N8_PROTOCOL_IPSEC,N8_CIPHER_DES,N8_HMAC_MD5_96)
#define PACKET_IPSEC_DES_HMAC_SHA1_96 \
  PROTOCOL_CIPHER_HASH(N8_PROTOCOL_IPSEC,N8_CIPHER_DES,N8_HMAC_SHA1_96)
#define PACKET_IPSEC_DES_HASH_NONE \
  PROTOCOL_CIPHER_HASH(N8_PROTOCOL_IPSEC,N8_CIPHER_DES,N8_HASH_NONE)
 
#endif /* N8_PACKET_H */

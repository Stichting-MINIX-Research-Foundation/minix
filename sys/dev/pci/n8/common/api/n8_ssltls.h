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
 * @(#) n8_ssltls.h 1.2@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_ssltls.h
 *  @brief Header for SSL/TLS functionality.
 *
 * Protoypes and structures for SSL/TLS functions.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 05/20/03 brr   Moved public constants to n8_pub_packet.h.
 * 10/12/01 dkm   Moved public portion to n8_pub_packet.h.
 * 06/17/01 bac   Added N8_TLS_VERSION.
 * 06/14/01 bac   Changes per code review: comment changes, create defines
 *                for N8_SSLTLS_MAX_DATA_SIZE_[ENCRYPT,DECRYPT] and
 *                N8_DES_BLOCK_MULTIPLE
 * 06/05/01 bac   Changes to not rely on N8_SSLTLSPacket_t being packed (Bug
 *                #31).  This includes changing the definition of
 *                N8_SSLTLSPacket_t, adding SSLTLS_*_OFFSET defines, and adding
 *                macros for SSLTLS_EXTRACT_* and SSLTLS_SET_*.
 * 05/30/01 bac   Changed structure comments.  Changed type of the verify_p in
 *                DecryptVerify.
 * 05/22/01 bac   Changed data definition in N8_SSLTLSPacket_t.  Changed
 *                interfaces to SSLTLSDecrypt and SSLTLSAuthenticate to take and
 *                return packets instead buffers.  Added prototypes for
 *                N8_GetHashLength and N8_ComputeEncryptedLength.
 * 05/18/01 bac   Converted to N8_xMALLOC and N8_xFREE
 * 05/18/01 bac   Fixed N8_SSLTLSEncryptAuthenticate prototype to match new
 *                API. 
 * 05/02/01 bac   Original version.
 ****************************************************************************/

#ifndef N8_SSLTLS_H
#define N8_SSLTLS_H

#include "n8_pub_packet.h"
#include "n8_packet.h"

#define N8_DES_BLOCK_MULTIPLE    8
#define N8_DES_MD5_MIN_LENGTH   24
#define N8_DES_SHA1_MIN_LENGTH  24
#define N8_ARC4_MD5_MIN_LENGTH  16
#define N8_ARC4_SHA1_MIN_LENGTH 20

#define SSLTLS_TYPE_OFFSET      0
#define SSLTLS_VERSION_OFFSET   1
#define SSLTLS_LENGTH_OFFSET    3
#define SSLTLS_DATA_OFFSET      5
#define SSLTLS_HEADER_LEN       5

#define SSLTLS_EXTRACT_TYPE(PACKET_P)    (const uint8_t) (PACKET_P[SSLTLS_TYPE_OFFSET])
#define SSLTLS_EXTRACT_VERSION(PACKET_P)  ntohs(*((const uint16_t *) &PACKET_P[SSLTLS_VERSION_OFFSET]))
#define SSLTLS_EXTRACT_LENGTH(PACKET_P)   ntohs(*((const uint16_t *) &PACKET_P[SSLTLS_LENGTH_OFFSET]))

#define SSLTLS_SET_TYPE(PACKET_P, VALUE)   PACKET_P[SSLTLS_TYPE_OFFSET] = (VALUE)
#define SSLTLS_SET_VERSION(PACKET_P, VALUE) *((uint16_t *) &PACKET_P[SSLTLS_VERSION_OFFSET]) = htons((VALUE))
#define SSLTLS_SET_LENGTH(PACKET_P, VALUE) *((uint16_t *) &PACKET_P[SSLTLS_LENGTH_OFFSET]) = htons((VALUE))

/*
 * SSL/TLS content types as defined by the protocol.  Do not change the values.
 */
typedef enum
{
   N8_CHANGE_CIPHER_SPEC = 20,
   N8_ALERT              = 21,
   N8_HANDSHAKE          = 22,
   N8_APPLICATION_DATA   = 23
} N8_SSLTLS_ContentType_t;

/* prototypes */

short int N8_GetHashLength(N8_HashAlgorithm_t hash);

short int N8_ComputeEncryptedLength(int size, int hashLen, N8_Cipher_t cipher);

#endif /* N8_SSLTLS_H */

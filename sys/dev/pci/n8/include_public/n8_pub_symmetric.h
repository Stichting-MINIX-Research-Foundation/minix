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
 * @(#) n8_pub_symmetric.h 1.7@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_pub_symmetric
 *  @brief Common declarations for symmetric (encrypt/decrypt) operations.
 *
 * Public header file for encrypt/decrypt symmetric (private key) operations.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 05/20/03 brr   Move defines for message length to this public include.
 * 06/12/02 bac   Corrected lengthof keyARC4 in N8_Key_t (Bug #768)
 * 11/28/01 mel   Fixed bug #365 : ARC4 key type N8_RC4_t incorrectly declared 
 * 11/12/01 hml   Added structureID to N8_EncryptObject_t.
 * 10/12/01 dkm   Original version. Adapted from n8_types.h.
 ****************************************************************************/
#ifndef N8_PUB_SYMMETRIC_H
#define N8_PUB_SYMMETRIC_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "n8_pub_common.h"

/*****************************************************************************
 * #defines 
 *****************************************************************************/
#define N8_DES_BLOCK_LENGTH 8
#define N8_ARC4_MAX_LENGTH  256

#define N8_MAX_MESSAGE_LENGTH (18 * 1024)
#define N8_MIN_MESSAGE_LENGTH 0

/*****************************************************************************
 * Structures/type definitions
 *****************************************************************************/

typedef struct
{
   char     IV[N8_DES_KEY_LENGTH];
   char     key1[N8_DES_KEY_LENGTH];
   char     key2[N8_DES_KEY_LENGTH];
   char     key3[N8_DES_KEY_LENGTH];
} N8_DES_t;

/* cipher info */
typedef union 
{
   N8_Buffer_t     keyARC4[N8_ARC4_MAX_LENGTH]; /* specifies ARC4 key to use */
   N8_DES_t        keyDES;            /* specifies data to use with IPSec*/
} N8_Key_t;

typedef struct 
{
   uint32_t  sequence_number[2];
   unsigned int keySize;        /* keys size for verification */
   N8_Key_t key;                /* specific protocol data */
   N8_Unit_t unitID;            /* The unit number for ops using this */
} N8_EncryptCipher_t;

/* packet object */
typedef struct
{
   N8_Cipher_t        cipher;   /* cipher: ARC4, DES*/
   N8_ContextHandle_t contextHandle;
   N8_EncryptCipher_t cipherInfo;   /* keys to use */
   N8_Buffer_t        residual_p[N8_DES_BLOCK_LENGTH];
   unsigned int       residualLength;
   N8_Unit_t          unitID;       /* The unit number for ops using this */
   unsigned int       structureID;
} N8_EncryptObject_t;

/*****************************************************************************
 * Function prototypes
 *****************************************************************************/
N8_Status_t N8_EncryptInitialize(N8_EncryptObject_t       *encryptObject_p,
                                 const N8_ContextHandle_t *contextHandle_p,
                                 const N8_Cipher_t         cipher,
                                 N8_EncryptCipher_t       *cipherInfo_p,
                                 N8_Event_t               *event_p);

N8_Status_t N8_Encrypt(N8_EncryptObject_t *encryptObject_p,
                       const N8_Buffer_t  *message_p,
                       const unsigned int  messageLength,  
                       N8_Buffer_t        *encryptedMessage_p,
                       N8_Event_t         *event_p );

N8_Status_t N8_Encrypt_uio(N8_EncryptObject_t *encryptObject_p,
                       struct uio         *message_p,
                       const unsigned int  messageLength,  
                       struct uio         *encryptedMessage_p,
                       N8_Event_t         *event_p );

N8_Status_t N8_EncryptPartial(N8_EncryptObject_t *encryptObject_p,
                              const N8_Buffer_t  *message_p,
                              const unsigned int  messageLength,  
                              N8_Buffer_t        *encryptedMessage_p,
                              unsigned int       *encryptedMsgLen_p,
                              N8_Event_t         *event_p);

N8_Status_t N8_EncryptEnd(N8_EncryptObject_t *encryptObject_p,
                          N8_Buffer_t        *encryptedMessage_p,
                          unsigned int       *encryptedMessageLength,  
                          N8_Event_t         *event_p );

N8_Status_t N8_Decrypt(N8_EncryptObject_t *encryptObject_p,
                       const N8_Buffer_t  *encryptedMessage_p,
                       const unsigned int  encryptedMessageLength,  
                       N8_Buffer_t        *message_p,
                       N8_Event_t         *event_p );

N8_Status_t N8_Decrypt_uio(N8_EncryptObject_t *encryptObject_p,
                       struct uio         *encryptedMessage_p,
                       const unsigned int  encryptedMessageLength,  
                       struct uio         *message_p,
                       N8_Event_t         *event_p );

N8_Status_t N8_DecryptPartial(N8_EncryptObject_t *encryptObject_p,
                              const N8_Buffer_t  *encryptedMessage_p,
                              const unsigned int  encryptedMessageLength,  
                              N8_Buffer_t        *message_p,
                              unsigned int       *messageLength,  
                              N8_Event_t         *event_p );

N8_Status_t N8_DecryptEnd(N8_EncryptObject_t *encryptObject_p,
                          N8_Buffer_t        *message_p,
                          unsigned int       *messageLength,  
                          N8_Event_t         *event_p );

#ifdef __cplusplus
}
#endif

#endif


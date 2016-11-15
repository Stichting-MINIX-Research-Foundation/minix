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
 * @(#) n8_cb_ea.h 1.65@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_cb_ea.h
 *  @brief Header file for E/A command block generation.
 *
 * Contains functions prototypes for n8_cb_ea.c
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 08/18/03 brr   Combine Encrypt/Decrypt command block generators for SSL,
 *                TLS, & IPsec.
 * 05/20/03 brr   Eliminate obsolete include files.
 * 09/10/02 brr   Set command complete bit on last command block.
 * 03/26/02 brr   Allocate the data buffer as part of the API request. 
 * 01/12/02 bac   Changed signature for cb_ea_hashEnd to add n8_IVSrc_t.
 * 10/30/01 bac   Standardized cb load context function names.
 * 10/16/01 spm   IKE APIs: removed key physical addr parms
 * 10/15/01 spm   IKE APIs: removed virtual pointers to msg from IKE cb arg
 *                lists.  Had to keep the virtual pointers to key, since
 *                there needs to be a copy of the key into the command block
 *                itself.
 * 10/15/01 bac   Changed some signatures to correctly use unsigned ints.
 * 10/11/01 hml   Added the protos for cb_ea_hashCompleteMessage and
 *                cb_ea_TLSHandshakeHash as well as some associated
 *                #defines.
 * 09/21/01 bac   Corrected signature on cb_ea_encrypt to take physical
 *                addresses. 
 * 09/20/01 bac   The interface to the command block generators changed and now
 *                accept the command block buffer.  
 * 09/18/01 bac   Massive changes to support model where the caller allocates
 *                the command buffer.  Lots of reorganization and renaming to be
 *                more standard.
 * 09/17/01 spm   Truncated lines >80 chars.
 * 09/07/01 spm   Added support for IKE API Extensions.
 * 07/26/01 mel   Deleted open SSL dependency.
 * 06/25/01 bac   More on conversion to use physical memory.
 * 06/19/01 bac   Corrected signatures for use of physical addresses.
 * 05/22/01 bac   Changed SSL Encrypt and Decrypt commands to pass
 *                packets instead of buffers.
 * 05/21/01 bac   Converted to use N8_ContextHandle_t and N8_Packet_t
 *                with integrated cipher and hash packet.
 * 05/09/01 bac   Added prototype for cb_ea_SSLEncryptAuthenticate.
 * 04/01/01 bac   Original version.
 ****************************************************************************/
#include "n8_ea_common.h"
#include "n8_hash.h"
#include "n8_enqueue_common.h"  /* contains encryption/authentication queue
                                 * declarations                 */
#include "n8_packet.h"          /* contains packet declarations */
#include "n8_packet_IPSec.h"    /* contains IPSec packet declarations */
#include "n8_ssltls.h"
#include "n8_pub_context.h"
#include "n8_pub_symmetric.h"

/* N8_RC4_t
 * ARC4 key declaration. This structure is the same as 
 * RC4_KEY from rc4.h. OpenSSL configure should not use RC4_CHAR or
 * RC4_LONG when using NetOctave hw.*/
typedef struct 
{
   unsigned int x,y;
   unsigned int data[256];
} N8_RC4_t;


/* define for the number of hashes necessary to create a given output length.
 * __OL is the desired output length.  __HL is the length generated per hash
 * operation. */
#define N8_HASHES_REQUIRED(__OL, __HL)      (CEIL((__OL), (__HL)))
#define N8_SHA1_HASHES_REQUIRED(__OL)       (N8_HASHES_REQUIRED((__OL), EA_SHA1_Hash_Length))
#define N8_MD5_HASHES_REQUIRED(__OL)        (N8_HASHES_REQUIRED((__OL), EA_MD5_Hash_Length))
/* when generating material for TLS, the spec requires that the first result be
 * discarded -- thus the extra hash required. */
#define N8_SHA1_HASHES_REQUIRED_TLS(__OL)   (N8_SHA1_HASHES_REQUIRED((__OL)) + 1)
#define N8_MD5_HASHES_REQUIRED_TLS(__OL)    (N8_MD5_HASHES_REQUIRED((__OL)) + 1)

/* defines indicating the number of command blocks for each computation */
#define N8_CB_EA_HASHCOMPLETEMESSAGE_NUMCMDS        1
#define N8_CB_EA_HASHPARTIAL_NUMCMDS                1
#define N8_CB_EA_HASHEND_NUMCMDS                    1
#define N8_CB_EA_SSLKEYMATERIALHASH_NUMCMDS         1
#define N8_CB_EA_SSLENCRYPTAUTHENTICATE_NUMCMDS     1
#define N8_CB_EA_SSLDECRYPTVERIFY_NUMCMDS           1
#define N8_CB_EA_TLSENCRYPTAUTHENTICATE_NUMCMDS     1
#define N8_CB_EA_TLSDECRYPTVERIFY_NUMCMDS           1
#define N8_CB_EA_TLSKEYMATERIALHASH_NUMCMDS(__L)    \
  (2*(N8_SHA1_HASHES_REQUIRED_TLS(__L) + N8_MD5_HASHES_REQUIRED_TLS(__L)))
#define N8_CB_EA_IKEPRF_NUMCMDS                     1
#define N8_CB_EA_IKESKEYIDEXPAND_NUMCMDS            1
#define N8_CB_EA_IKEKEYMATERIALEXPAND_NUMCMDS       1
#define N8_CB_EA_IKEENCRYPTKEYEXPAND_NUMCMDS        1
#define N8_CB_EA_CLEARCONTEXT_NUMCMDS               1
#define N8_CB_EA_WRITECONTEXT_NUMCMDS               1
#define N8_CB_EA_READCONTEXT_NUMCMDS                1
#define N8_CB_EA_CLEARCONTEXT_NUMCMDS               1
#define N8_CB_EA_ENCRYPT_NUMCMDS                    1
#define N8_CB_EA_DECRYPT_NUMCMDS                    1
#define N8_CB_EA_LOADARC4KEYTOCONTEXT_NUMCMDS       1
#define N8_CB_EA_LOADARC4KEYONLY_NUMCMDS            1
#define N8_CB_EA_LOADDESKEYTOCONTEXT_NUMCMDS        1
#define N8_CB_EA_LOADDESKEYONLY_NUMCMDS             1
#define N8_CB_EA_LOADIPSECKEYTOCONTEXT_NUMCMDS      1
#define N8_CB_EA_IPSECENCRYPTAUTHENTICATE_NUMCMDS   1
#define N8_CB_EA_IPSECDECRYPTVERIFY_NUMCMDS         1
#define N8_CB_EA_IPSECDECRYPTVERIFY_NUMCMDS         1
#define N8_CB_EA_PRECOMPUTE_MD5_NUMCMDS             2
#define N8_CB_EA_HASHHMACEND_NUMCMDS                1
#define N8_CB_EA_FINISHTLSHANDSHAKE_NUMCMDS         4
#define N8_CB_EA_CERTTLSHANDSHAKE_NUMCMDS           2
#define N8_CB_EA_SSLSHANDSHAKEHASH_NUMCMDS          4

/* The length of both "client finished" and 
   "server finished" */
#define N8_TLS_ROLE_STRING_LENGTH                  15


/*  function prototypes */
N8_Status_t  cb_ea_writeContext(API_Request_t       *req_p,
                                EA_CMD_BLOCK_t      *cb_p,
                                const unsigned int   contextIndex,
                                const N8_Buffer_t   *bufferToWrite_p,
                                const unsigned int   length);

N8_Status_t  cb_ea_readContext(API_Request_t      *req_p,
                               EA_CMD_BLOCK_t     *cb_p,
                               const unsigned int  contextIndex,
                               const uint32_t      bufferToRead_a,
                               const unsigned int  length);

N8_Status_t cb_ea_loadARC4KeyToContext(API_Request_t           *req_p,
                                       EA_CMD_BLOCK_t          *cb_p,
                                       const N8_Packet_t    *packetObject_p, 
                                       const N8_CipherInfo_t   *cipher_p,
                                       const N8_HashAlgorithm_t hashAlgorithm,
                                       EA_ARC4_CTX             *ctx_p,
                                       const uint32_t           ctx_a,
                                       EA_CMD_BLOCK_t          **next_cb_pp);

N8_Status_t cb_ea_loadDESKeyToContext(API_Request_t           *req_p,
                                      EA_CMD_BLOCK_t          *cb_p,
                                      const N8_Packet_t    *packetObject_p, 
                                      const N8_CipherInfo_t   *cipherInfo_p,
                                      const N8_HashAlgorithm_t hashAlgorithm,
                                      EA_SSL30_CTX            *ctx_p,
                                      const uint32_t           ctx_a,
                                      EA_CMD_BLOCK_t          **next_cb_pp);

N8_Status_t cb_ea_loadIPsecKeyToContext(API_Request_t          *req_p,
                                        EA_CMD_BLOCK_t          *cb_p,
                                        const unsigned int      contextIndex, 
                                        const N8_CipherInfo_t   *cipherInfo_p,
                                        EA_IPSEC_CTX            *IPSec_ctx_p,
                                        const uint32_t          IPSec_ctx_a,
                                        EA_CMD_BLOCK_t          **next_cb_pp);

N8_Status_t cb_ea_hashPartial(API_Request_t *req_p,
                              EA_CMD_BLOCK_t *cb_p,
                              const N8_HashObject_t *obj_p,
                              const n8_IVSrc_t ivSrc,
                              const uint32_t hashMsg_a,
                              const unsigned int msgLength,
                              const uint32_t result_a,
                              EA_CMD_BLOCK_t **next_cb_pp,
                              int            lastCmdBlock);

N8_Status_t cb_ea_hashEnd(API_Request_t *req_p,
                          EA_CMD_BLOCK_t *cb_p,
                          const N8_HashObject_t *obj_p,
                          const n8_IVSrc_t ivSrc,
                          const uint32_t hashMsg_a,
                          const unsigned int msgLength,
                          const uint32_t result_a,
                          EA_CMD_BLOCK_t **next_cb_pp,
                          int            lastCmdBlock);

N8_Status_t cb_ea_hashHMACEnd(API_Request_t *req_p,
                              EA_CMD_BLOCK_t *cb_p,
                              const N8_HashObject_t *obj_p,
                              const uint32_t hashMsg_a,
                              const unsigned int msgLength,
                              const uint32_t result_a,
                              EA_CMD_BLOCK_t **next_cb_pp);

N8_Status_t cb_ea_SSLKeyMaterialHash(API_Request_t *req_p,
                                     EA_CMD_BLOCK_t *cb_p,
                                     const uint32_t key_a,
                                     const int keyLength,
                                     const N8_Buffer_t *random_p,
                                     const int outputLength,
                                     const uint32_t result_a);

N8_Status_t cb_ea_SSL(EA_CMD_BLOCK_t *cb_p,
                      N8_Packet_t *packetObj_p,
                      const N8_SSLTLSPacket_t *packet_p,
                      const uint32_t input_a,
                      const uint32_t result_a,
                      const unsigned int opCode);

N8_Status_t cb_ea_TLSKeyMaterialHash(API_Request_t *req_p,
                                     EA_CMD_BLOCK_t *cb_p,
                                     const N8_Buffer_t *msg_p,
                                     const uint32_t msg_a,
                                     const int dataLength,
                                     N8_Buffer_t *hmacKey_p,
                                     const uint32_t hmacKey_a,
                                     const int keyLength,
                                     const int outputLength,
                                     const uint32_t pseudorandomStream1_a,
                                     const uint32_t pseudorandomStream2_a,
                                     const int      keyLen);

N8_Status_t cb_ea_IKEPrf(API_Request_t *req_p,
                         EA_CMD_BLOCK_t *cb_p,
                         const N8_HashAlgorithm_t alg,
                         const uint32_t kMsg_a,
                         const uint32_t msgLength,
                         const N8_Buffer_t *kKey_p,
                         const uint32_t keyLength,
                         const uint32_t kRes_a);

N8_Status_t cb_ea_IKESKEYIDExpand(API_Request_t *req_p,
                                  EA_CMD_BLOCK_t *cb_p,
                                  const N8_HashAlgorithm_t alg,
                                  const uint32_t kMsg_a,
                                  const uint32_t msgLength,
                                  const N8_Buffer_t *kKey_p,
                                  const uint32_t keyLength,
                                  const uint32_t kSKEYIDd_a);

N8_Status_t cb_ea_IKEKeyMaterialExpand(API_Request_t *req_p,
                                       EA_CMD_BLOCK_t *cb_p,
                                       const N8_HashAlgorithm_t alg,
                                       const uint32_t kMsg_a,
                                       const uint32_t msgLength,
                                       const N8_Buffer_t *kKey_p,
                                       const uint32_t keyLength,
                                       const uint32_t kRes_a,
                                       const uint32_t i_count);

N8_Status_t cb_ea_IKEEncryptKeyExpand(API_Request_t *req_p,
                                      EA_CMD_BLOCK_t *cb_p,
                                      const N8_HashAlgorithm_t alg,
                                      const uint32_t kMsg_a,
                                      const uint32_t msgLength,
                                      const N8_Buffer_t *kKey_p,
                                      const uint32_t keyLength,
                                      const uint32_t kRes_a,
                                      const uint32_t i_count);

N8_Status_t cb_ea_TLS(EA_CMD_BLOCK_t *cb_p,
                      N8_Packet_t *packetObj_p,
                      const N8_SSLTLSPacket_t *packet_p,
                      const uint32_t input_a,
                      const uint32_t result_a,
                      const unsigned int opCode);

void  cb_ea_IPsec (EA_CMD_BLOCK_t        *cb_p,
                   const N8_Packet_t     *packetObject_p, 
                   const uint32_t         encryptedPacket_a, 
                   const uint32_t         result_a,
                   const unsigned int     encryptedPacketLength,
                   const int              SPI,
                   const unsigned int     opCode);

N8_Status_t  cb_ea_loadARC4keyOnly(API_Request_t *req_p,
                                   EA_CMD_BLOCK_t *cb_p,
                                   const N8_ContextHandle_t *contextHandle_p,
                                   const N8_EncryptCipher_t *cipher_p);

N8_Status_t cb_ea_loadDESkeyOnly(API_Request_t *req_p,
                                 EA_CMD_BLOCK_t *cb_p,
                                 const N8_ContextHandle_t *contextHandle_p,
                                 const N8_EncryptCipher_t *cipherInfo_p);

N8_Status_t cb_ea_encrypt(const API_Request_t     *req_p,
                          EA_CMD_BLOCK_t          *cb_p,
                          N8_EncryptObject_t      *encryptObject_p,
                          const uint32_t           message_a,
                          const uint32_t           encryptedMessage_a,
                          const int                messageLength);

N8_Status_t cb_ea_decrypt(API_Request_t            *req_p,
                          EA_CMD_BLOCK_t           *cb_p,
                          N8_EncryptObject_t       *encryptObject_p,
                          const uint32_t            encryptedMessage_a,
                          const uint32_t            message_a,
                          const unsigned int        encryptedMessageLength);

N8_Status_t cb_ea_hashCompleteMessage(API_Request_t *req_p,
                                      EA_CMD_BLOCK_t *cb_p,
                                      const N8_HashObject_t *obj_p,
                                      const uint32_t hashMsg_a,
                                      const unsigned int msgLength,
                                      const uint32_t result_a);
N8_Status_t 
cb_ea_TLSHandshakeHash(API_Request_t      *req_p,
                       N8_HashProtocol_t   protocol,
                       uint32_t            resMD5_a,
                       uint32_t            hashMsgMD5_a, 
                       N8_HashObject_t     *hashMsgMD5_p,
                       int                 md5Length,
                       uint32_t            resSHA1_a,
                       uint32_t            hashMsgSHA1_a,
                       N8_HashObject_t     *hashMsgSHA1_p,
                       int                 sha1Length,
                       uint32_t            resMD5PRF_a,
                       uint32_t            resSHA1PRF_a,
                       const N8_Buffer_t  *key_p,
                       int                 keyLength,
                       uint32_t            roleStr_a);


N8_Status_t cb_ea_SSLHandshakeHash(API_Request_t       *req_p,
                                   EA_CMD_BLOCK_t      *cb_p,
                                   N8_HashObject_t     *hObjMD5_p,
                                   uint32_t            innerResult_md5_a,
                                   uint32_t            hashMsgMD5_a,
                                   int                 hashingLength_md5,
                                   N8_HashObject_t     *hObjSHA_p,
                                   uint32_t            innerResult_sha_a,
                                   uint32_t            hashMsgSHA_a,
                                   int                 hashingLength_sha,
                                   uint32_t            endresMD5_a,
                                   uint32_t            endresSHA1_a,
                                   uint32_t            outerMsgMD5_a,
                                   unsigned int        outer_md5Length,
                                   uint32_t            outerMsgSHA1_a,
                                   unsigned int        outer_shaLength);


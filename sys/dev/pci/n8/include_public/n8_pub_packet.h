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
 * @(#) n8_pub_packet.h 1.21@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_pub_packet
 *  @brief Public declarations for packet operations.
 *
 * Public header file for packet) operations.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 08/18/03 brr   Combine Encrypt/Decrypt command block generators for SSL,
 *                TLS, & IPsec.
 * 06/06/03 brr   Brought useful IPSEC defines to this public include file.
 * 05/20/03 brr   Modified N8_PacketInitialize to setup function pointers &
 *                lengths used in the Encrypt/Decrypt operations. Eliminated
 *                several switch statements from Encrypt/Decrypt operations.
 * 04/17/03 brr   Moved key size constants to this public include file.
 * 08/06/02 bac   Made cipher infos const.
 * 07/16/02 bac   Removed trailing comma from last enum entry in
 *                N8_PacketMemoryMode.
 * 06/14/02 hml   Deleted request field and other unused fields from the
 *                packet object.  Also added the N8_PacketMemoryMode_t enum
 *                and protos for the N8_SSLTLS*Memory API calls.
 * 06/10/02 hml   Added request field to packet object.
 * 03/18/02 hml   Added include of n8_pub_buffer.h.
 * 02/12/02 hml   Added proto for N8_PacketBuffersSet.
 * 02/07/02 hml   Added some fields for kernel buffers allocated by user.
 * 01/22/02 bac   Added a boolean to N8_Packet_t to control deferred loading of
 *                context memory.
 * 11/28/01 mel   Fixed bug #365 : ARC4 key type N8_RC4_t incorrectly declared 
 * 11/12/01 hml   Added structureID to N8_Packet_t (Bug 261) and changed
 *                PI_PROTOCOL_* to N8_PROTOCOL_*.
 * 10/25/01 dkm   Changed PI_KeyInfo_t to use N8_RC4_t to remove OpenSSL
 *                dependency. 
 * 10/12/01 dkm   Original version. 
 ****************************************************************************/
#ifndef N8_PUB_PACKET_H
#define N8_PUB_PACKET_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "n8_pub_common.h"
#include "n8_pub_buffer.h"
#include "n8_pub_request.h"

/*****************************************************************************
 * #defines 
 *****************************************************************************/
/* Maximum length for mac key */
#define N8_MAC_SECRET_LENGTH 20

#define N8_PRECOMPUTE_SIZE   5

/* definitions for key size */
#define    ARC4_KEY_SIZE_BYTES_MAX   256
#define    DES_KEY_SIZE_BYTES        24

#define N8_SSL_VERSION          0x0300
#define N8_TLS_VERSION          0x0301

#define N8_SSLTLS_MAX_DATA_SIZE_DECRYPT    (18 * 1024)
#define N8_SSLTLS_MAX_DATA_SIZE_ENCRYPT    (17 * 1024)

/* definitions for IPsec packets */
#define IPSEC_DATA_LENGTH_MAX      18*1024
#define IPSEC_DATA_LENGTH_MIN      24
#define HMAC_LENGTH                12
#define IPSEC_PACKET_HEADER_LENGTH 16
#define IPSEC_IV_OFFSET            8
#define IPSEC_DATA_OFFSET          16
/*****************************************************************************
 * Structures/type definitions
 *****************************************************************************/

/*
 * SSL/TLS packet structure as defined by the protocol.
 */
typedef unsigned char N8_SSLTLSPacket_t;

/*
 * IPSec packet structure as defined by the protocol.
 */
typedef unsigned char N8_IPSecPacket_t;

/* Specifies the DES keys and IV  to use with IPSec */
typedef struct
{
   uint32_t ipad[N8_PRECOMPUTE_SIZE];
   uint32_t opad[N8_PRECOMPUTE_SIZE];
   uint32_t SPI;
   uint32_t sequence_number;
} N8_IPsecKeyDES_t;

/* The three permissible values for Protocol are SSL, TLS, and IPSec */
typedef enum
{
   N8_PROTOCOL_SSL    = 1,
   N8_PROTOCOL_TLS,
   N8_PROTOCOL_IPSEC
} N8_Protocol_t;

/* The memory modes for the packet ops */
typedef enum
{
   N8_PACKETMEMORY_NONE    = 1,
   N8_PACKETMEMORY_REQUEST
} N8_PacketMemoryMode_t;


/* cipher info */
typedef union 
{
   unsigned char     keyARC4[32]; /* specifies ARC4 key to use */
   N8_IPsecKeyDES_t  IPsecKeyDES; /* specifies data to use with IPSec */
} N8_KeyInfo_t;

/* N8_CipherInfo_t
   This is the structure for passing key information during packet
   initialization.
   precompute1/2 - For internal use in calculations
   macSecret - Secret for mac calculation in SSL
   sequence_number - Initial sequence number for packet
   IV - Initialization vector
   key1/2/3 - DES key for initialization (SSL/TLS)
   keySize - length of key
   key - Union for initializing RC4 and IPsec DES key
   hmac_key/_length - Secret and length for HMAC
   */
typedef struct 
{
   uint32_t           precompute1[N8_PRECOMPUTE_SIZE];
   uint32_t           precompute2[N8_PRECOMPUTE_SIZE];
   char               macSecret[N8_MAC_SECRET_LENGTH];
   uint32_t           sequence_number[2];
   char               IV[N8_DES_KEY_LENGTH];
   char               key1[N8_DES_KEY_LENGTH];
   char               key2[N8_DES_KEY_LENGTH];
   char               key3[N8_DES_KEY_LENGTH];
   int                keySize;              /* keys size for verification */
   N8_KeyInfo_t       key;                  /* specific protocol data */
   N8_Buffer_t       *hmac_key;
   uint32_t           hmacKeyLength;
   N8_Unit_t      unitID;       /* execution unit */
} N8_CipherInfo_t;

/* packet object */
typedef struct
{
   N8_Protocol_t      packetProtocol; /* packet protocol to use: SSL, TLS, IPSec */
   N8_Cipher_t        packetCipher;   /* cipher: ARC4, DES*/
   N8_HashAlgorithm_t packetHashAlgorithm;
   /* hash algorithm: MD5, SHA-1, HMAC-MD5, HMAC-SHA-1,
    * HMAC-MD5-96, HMAC-SHA-1-96 */
   N8_ContextHandle_t contextHandle;
   N8_CipherInfo_t    cipherInfo;   /* keys to use */
   N8_HashObject_t    hashPacket;   /* hash value and information */
   N8_Unit_t          unitID;       /* execution unit */
   unsigned int       structureID;
   N8_Boolean_t       contextLoadNeeded; /* does the context need to be loaded?
                                          * if so, this is done on the first use
                                          * of the packet post-initialization.*/
   N8_PacketMemoryMode_t  mode;       /* Memory type for this packet */
   unsigned int       encCommands;    /* Number of cmds needed for encrypt */
   unsigned int       decCommands;    /* Number of cmds needed for decrypt */
   int                minLength;      /* Minimum packet length             */
   int                macLength;      /* Hash result length                */
   void              *ctxLoadFcn;     /* Context load cmd block generator  */
   int                ctxLoadCmds;    /* Number of cmds needed to load ctx */
   void              *SSLTLScmdFcn;   /* cmd block generator function      */
   unsigned int       encOpCode;      /* Op Code for encryption operation  */
   unsigned int       decOpCode;      /* Op Code for decryption operation  */
} N8_Packet_t;

/*****************************************************************************
 * Function prototypes
 *****************************************************************************/
N8_Status_t N8_PacketInitializeMemory(N8_Packet_t              *packetObject_p,
                                      const N8_ContextHandle_t *contextHandle_p,
                                      const N8_Protocol_t       protocol,
                                      const N8_Cipher_t         cipher,
                                      const N8_CipherInfo_t          *cipherInfo_p,
                                      const N8_HashAlgorithm_t  hashAlgorithm,
                                      const N8_PacketMemoryMode_t mode,
                                      N8_Event_t               *event_p);

N8_Status_t N8_PacketInitialize(N8_Packet_t              *packetObject_p,
                                const N8_ContextHandle_t    *contextHandle_p,
                                const N8_Protocol_t         protocol,
                                const N8_Cipher_t           cipher,
                                const N8_CipherInfo_t             *cipherInfo_p,
                                const N8_HashAlgorithm_t    hashAlgorithm,
                                N8_Event_t                  *event_p);

N8_Status_t 
N8_SSLTLSEncryptAuthenticateMemory(N8_Packet_t             *packetObj_p,
                                   const N8_SSLTLSPacket_t *packet_p,
                                   N8_SSLTLSPacket_t       *result_p,
                                   N8_RequestHandle_t       request,
                                   N8_Event_t              *event_p);

N8_Status_t 
N8_SSLTLSDecryptVerifyMemory(N8_Packet_t             *packetObj_p,
                             const N8_SSLTLSPacket_t *packet_p,
                             N8_Buffer_t             *computedMAC_p,
                             N8_Boolean_t            *verify_p,
                             N8_SSLTLSPacket_t       *result_p,
                             N8_RequestHandle_t       request,
                             N8_Event_t              *event_p);

N8_Status_t N8_SSLTLSEncryptAuthenticate(N8_Packet_t *packetObj_p,
                                         const N8_SSLTLSPacket_t *packet_p,
                                         N8_SSLTLSPacket_t *result_p,
                                         N8_Event_t *event_p);

N8_Status_t N8_SSLTLSDecryptVerify(N8_Packet_t             *packetObj_p,
                                   const N8_SSLTLSPacket_t *packet_p,
                                   N8_Buffer_t             *computedMAC_p,
                                   N8_Boolean_t               *verify_p,
                                   N8_SSLTLSPacket_t       *result_p,
                                   N8_Event_t              *event_p);

N8_Status_t N8_IPSecEncryptAuthenticate(N8_Packet_t     *packetObject_p, 
                                        N8_IPSecPacket_t   *packet_p, 
                                        int                 packetLength,  
                                        N8_IPSecPacket_t   *result_p, 
                                        N8_Event_t         *event_p );

N8_Status_t N8_IPSecDecryptVerify(N8_Packet_t     *packetObject_p, 
                                  N8_IPSecPacket_t   *encryptedPacket_p, 
                                  int                 encryptedPacketLength,
                                  N8_Buffer_t        *computedHMAC_p,
                                  N8_Boolean_t          *verify,
                                  N8_IPSecPacket_t   *result_p, 
                                  N8_Event_t         *event_p );

#ifdef __cplusplus
}
#endif

#endif



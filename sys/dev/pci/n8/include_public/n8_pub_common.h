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
 * @(#) n8_pub_common.h 1.42@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_pub_common.h
 *  @brief Common type declarations used in public interface.
 *
 * Common header file for NetOctave public includes.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 06/30/03 brr   Added N8_HASH_NONE to supported hash algorithms.
 * 06/06/03 brr   Brought N8_Hardware_t here from n8_common.
 * 05/07/03 brr   Removed type from N8_MemoryHandle_t;
 * 04/21/03 brr   Redefined N8_MemoryType_t to support single memory type 
 *                throughout the SDK.
 * 04/01/03 brr   Include n8_sdk_config to pick up configuration.
 * 03/10/03 brr   Added support for API callbacks.
 * 08/01/02 bac   Documented 'modulusSize' for N8_DH_KeyMaterial_t is in bytes.
 * 07/16/02 bac   Removed trailing comma from last enum entry in N8_SKSKeyType_t
 * 06/27/02 brr   Support two memory types in N8_MemoryStruct_t.
 * 06/14/02 hml   Added N8_MEMORY_KERNEL_BUFFER to N8_MemoryType_t and
 *                added bankPhysAddress to N8_MemoryStruct_t.
 * 06/12/02 hml   Added bankIndex field to N8_MemoryHandle_t.
 * 06/10/02 hml   Added N8_MemoryType_t enum for the type of a memory
 *                allocation.
 * 03/26/02 hml   Changed N8_INVALID_UNIT to N8_UNINITIALIZED_UNIT to
 *                resolve a conflict with our N8_Status_t enum.
 * 03/26/02 brr   Added bufferState to N8_MemoryHandle_t.
 * 02/01/02 brr   Include only public include files.
 * 01/23/02 brr   Added next pointer to N8_MemoryHandle_t.
 * 01/12/02 bac   Removed obsolete items contHMAC and doBlockingHMAC from
 *                N8_HashObject_t. 
 * 01/13/02 brr   Removed obsolete fields from N8_MemoryHandle_t.
 * 12/03/01 bac   Added entry_name to N8_SKSKeyHandle_t.
 * 11/12/01 spm   Added definition for integerized CEIL.
 * 11/14/01 msz   Added N8_QUEUE_REQUEST_CALLBACK
 * 11/12/01 hml   Added structureID to N8_HashObject_t (bug 261).
 * 11/10/01 mmd   Added RawVirtualAddress to N8_MemoryHandle_t, and now includes
 *                n8_device_info.h.
 * 11/07/01 dkm   Corrected header.
 * 11/06/01 hml   Added structureID field to the N8_ContextHandle_t type.
 * 10/25/01 dkm   Added comments to N8_RC4_t definition.
 * 10/23/01 dkm   Moved N8_ANY_UNIT from n8_common.h.
 * 10/22/01 mel   Added doBlockingHMAC to N8_HashObject_t.
 * 10/12/01 dkm   Original version. Adapted from n8_common.h.
 ****************************************************************************/
#ifndef N8_PUB_COMMON_H
#define N8_PUB_COMMON_H

#ifdef __cplusplus
extern "C"
{
#endif

#if 0
#include "n8_sdk_config.h"
#endif
#include "n8_pub_types.h"
#include "n8_pub_errors.h"

/*************************************************************************
 *
 *  General Declarations: Not specific to any computational block.
 *
 ************************************************************************/

#define N8_ANY_UNIT  -1
#define N8_UNINITIALIZED_UNIT -2

#define N8_DEF_MAX_NUM_PARAMS    12 /* Maximum number of parameters a function 
                                     * called from the command line can accept */

#define CEIL(__A,__B) (((__A)+(__B)-1)/(__B))

/******************************************************************************
 * NETOCTAVE GENERIC TYPEDEF'S                                                *
 ******************************************************************************/

typedef unsigned char N8_Buffer_t;

typedef struct
{
   N8_Buffer_t *value_p;
   unsigned int lengthBytes;     /* length of value in bytes */
} N8_SizedBuffer_t;

typedef enum
{
   N8_FALSE = 0,
   N8_TRUE  = 1
} N8_Boolean_t;

/* N8_Component_t
 * Define the different components on the chip.  These may be used as array
 * indices so they must remain zero-based and contiguous. */
typedef enum
{
   N8_PKP = 0,
   N8_RNG = 1,
   N8_EA  = 2,
   N8_NUM_COMPONENTS = 3
} N8_Component_t;

/*
 *  * Hardware types.
 *   */
typedef enum
{
   N8_NSP2000_HW   = 1,
   N8_BM           = 2,
   N8_FPGA         = 3
} N8_Hardware_t;


/*****************************************************************************
 * N8_ConfigAPI_t
 *
 *  This structure defines the API configuration. Pass this structure to the
 *  N8_InitializeAPI function is optional, unless using the callback thread.
 *
 *  NOTE: If this structure is modified, the constant N8_INITIALIZATION_VERSION
 *        must be changed.
 *
 *****************************************************************************/
#define  N8_INITIALIZATION_VERSION 1

typedef struct
{
   uint32_t      structure_version;/* The version of this strucure.         */
   uint32_t      callbackEvents;   /* The maximum number of callback events */
                                   /* queued to the API                     */
   uint32_t      callbackTimeout;  /* Timeout value for the callback event  */
                                   /* completion thread.                    */
} N8_ConfigAPI_t;

/*****************************************************************************
 * N8_QueueStatusCodes_t
 *
 *  This enum contains the status of a request.
 *
 *  N8_QUEUE_REQUEST_SENT_FROM_API      The request is not yet on the queue
 *                                      but has been initialized by SAPI
 *  N8_QUEUE_REQUEST_QUEUED             The request is owned by QMGR
 *  N8_QUEUE_REQUEST_FINISHED           QMGR is done with the request.
 *                                      After setting a request to finished
 *                                      QMGR should no longer look at a request
 *                                      because a separate thread of SAPI
 *                                      code could delete it.
 *  N8_QUEUE_REQUEST_COMMAND_ERROR      The request had an error.
 *
 *****************************************************************************/

/* Status of request/event that is in the queue */
typedef enum 
{
   N8_QUEUE_REQUEST_SENT_FROM_API         = 1,
   N8_QUEUE_REQUEST_QUEUED                = 2,
   N8_QUEUE_REQUEST_FINISHED              = 3,
   N8_QUEUE_REQUEST_COMMAND_ERROR         = 4
} N8_QueueStatusCodes_t;

/* Type of memory allocation */
typedef enum 
{
   N8_MEMBANK_QUEUE        = 0,   /* Allocation bank for command queues */
   N8_MEMBANK_EA           = 1,   /* Allocation bank for EA requests    */
   N8_MEMBANK_PK           = 2,   /* Allocation bank for PK requests    */
   N8_MEMBANK_USERPOOL     = 3,   /* Allocation bank for the user pool  */
   N8_MEMBANK_MAX          = 4    /* Upper bound for memory bank types  */
} N8_MemoryType_t;


/******************************************************************************
 * API RESOURCES -                                                            *
 *                                                                            *
 * N8_MemoryHandle_t is returned by calls to N8_AllocateBuffer and passed to  *
 * N8_FreeBuffer. It simplifies usage of kernel buffer mmap'ed to user space  *
 * by encapsulating everything needed to completely name/describe an          *
 * allocation. The caller needs only maintain a N8_MemoryHandle_t. The fields *
 * serve the following purposes :                                             *
 *                                                                            *
 * PhysicalAddress - Returns the physical base address of the allocation.     *
 * VirtualAddress  - Returns a usable pointer to the base of any allocation,  *
 *                   when allocating. Not needed for freeing memory.          *
 * Size            - Specifies the desired allocation size (in bytes) when    *
 *                   making an allocation call.                               *
 * bufferState     - A state variable used to indicate whether the buffer is  *
 *                   on an NSP's command queue and cannot be freed until the  *
 *                   command has been completed.                              *
 *                                                                            *
 * The most important note about this structure is that the calling           *
 * application must cooperate by using it as a read-only struct.              *
 ******************************************************************************/
 
typedef struct
{
     unsigned long    PhysicalAddress;  /* Physical address of allocation */
     unsigned long   *VirtualAddress;   /* Virtual address of allocation */
     unsigned long    Size;             /* Size of the allocation. */
     unsigned long    bufferState;      /* State of buffer vis-a-vis queues */
     unsigned long    bankAddress;      /* virtual address of this bank 
                                           in the process context it was set */
     unsigned long    bankPhysAddress;  /* Physical address of this bank */
     N8_MemoryType_t  bankIndex;        /* Bank index for the allocation */
} N8_MemoryHandle_t;



/*****************************************************************************
 * typedef N8_Unit_t
 *****************************************************************************/
/** @ingroup n8_common
 *  @typedef N8_Unit_t
 *  @brief This type is used for defining unit numbers.
 *         
 *****************************************************************************/
typedef int  N8_Unit_t;

/*****************************************************************************
 * typedef N8_Callback_t
 *****************************************************************************/
/** @ingroup n8_common
 *  @typedef N8_Callback_t
 *  @brief This is the prototype of functions performed as callbacks from the 
 *         API.
 *         
 *****************************************************************************/
typedef void (*N8_Callback_t)(void *, N8_Status_t);
/*
 * Event.  This is a placeholder definition for an asynchronous event.
 */
typedef struct
{
   N8_Component_t         unit;   /* the execution unit the request for this
                                   * event is targeted. */
   N8_QueueStatusCodes_t  status; /* status of API call this event refers to */
   void                  *state;  /* this is opaque data which is not usable by
                                   * the user. */
#ifdef SUPPORT_CALLBACKS
   N8_Callback_t         usrCallback;     /* User's callback function */
   void                  *usrData;        /* Pointer to user's data   */
#endif
} N8_Event_t;

/*************************************************************************
 *
 *  Hash Common: Common declarations required for hash computations.
 *
 ************************************************************************/

/* N8_HashAlgorithm_t
 * NOTE:  the values used here are not only for identification but are
 * also used as indices into an array, thus they need to be contiguous
 * and zero-based.
 */
typedef enum
{
   N8_MD5               = 1,
   N8_SHA1,
   N8_HMAC_MD5,
   N8_HMAC_SHA1,
   N8_HMAC_MD5_96,
   N8_HMAC_SHA1_96,
   N8_HASH_NONE
} N8_HashAlgorithm_t;

#define N8_HASH_BLOCK_SIZE   64
#define N8_MAX_RESIDUAL_LEN (N8_HASH_BLOCK_SIZE / sizeof(N8_Buffer_t))

/*
 * Hash object for MD5.  Inspired by
 * openssl/crypto/md5/md5.h::MD5state_t and 
 * openssl/crypto/sha/sha.h::SHA_CTX.
 */
typedef struct
{
   uint32_t     iv[5];          /* Interim results. Name map:
                                 * SHA-1: h0 = iv[0] ... h4 = iv[4]
                                 * MD5: A=iv[0] ... D=iv[3]
                                 */
   uint32_t     ipadHMAC_iv[5]; /* HMAC key XOR with 0x36 */
   uint32_t     opadHMAC_iv[5]; /* HMAC key XOR with 0x5C */
   uint32_t     hashedHMACKey[16]; /* hashed/padded HMAC key */
   uint32_t     opad_Nl, opad_Nh;
   uint32_t     Nl, Nh;         /* 64 bit length */
   N8_Buffer_t  residual[N8_MAX_RESIDUAL_LEN];
                                /* residual data from previous hash */
   int          residualLength; /* in bytes */
   N8_HashAlgorithm_t type;     /* type of algorithm initialized */
   int          hashSize;       /* size of resulting hash */
   N8_Unit_t    unitID;         /* execution unit */
   unsigned int structureID;
} N8_HashObject_t;      

/*************************************************************************
 *
 *  PK Common: Common declarations required for public key computations.
 *
 ************************************************************************/
/*
 * Key Types
 */
typedef enum
{
   N8_PUBLIC       = 1,
   N8_PRIVATE      = 2,
   N8_PRIVATE_CRT  = 3,
   N8_PRIVATE_SKS  = 4
} N8_KeyType_t;

typedef enum
{
   N8_RSA_VERSION_1_KEY   = 1,
   N8_DSA_VERSION_1_KEY   = 2
} N8_SKSKeyType_t;

#define N8_SKS_ENTRY_NAME_MAX_LENGTH 256
typedef struct 
{
   N8_SKSKeyType_t     key_type;
   uint32_t            key_length; /* BNC digits */
   N8_Unit_t           unitID;
   uint32_t            sks_offset; /* SKS words */
   char                entry_name[N8_SKS_ENTRY_NAME_MAX_LENGTH];
} N8_SKSKeyHandle_t;

/*
 * RSA Key Material
 */
typedef struct
{
   N8_SizedBuffer_t  publicKey;       /* present for public & private contexts */
   N8_SizedBuffer_t  privateKey;      /* present only for private context */
   N8_SizedBuffer_t  n;               /* modulus n = pq */
   N8_SizedBuffer_t  p;             
   N8_SizedBuffer_t  q;
   N8_Unit_t         unitID;           /* Execution unit for request */
   N8_SKSKeyHandle_t SKSKeyHandle;     /* contains SKS info */
} N8_RSAKeyMaterial_t;

/*
 * DSA Key Material
 */
typedef struct
{
   N8_SizedBuffer_t  publicKey;    /* present for public & private contexts */
   N8_SizedBuffer_t  privateKey;   /* present only for private context */
   N8_SizedBuffer_t  p;            /* prime modulus */
   N8_SizedBuffer_t  q;            /* prime divisor */
   N8_SizedBuffer_t  g;            /* generator */
   N8_Unit_t         unitID;       /* Unit id on which to execute this
                                      request */
   N8_SKSKeyHandle_t SKSKeyHandle; /* contains SKS info */
} N8_DSAKeyMaterial_t;

/*
 * Diffie-Hellman Key Material
 */
typedef struct
{
   N8_Buffer_t        *p;           /* modulus */             
   N8_Buffer_t        *g;           /* generator */
   unsigned int        modulusSize; /* modulus size in bytes */
   N8_Unit_t           unitID;      /* execution unit for this request */
} N8_DH_KeyMaterial_t;

/***************************************************************
 *
 *  Context Common: Common declarations required for 
 *                  context memory management.
 *
 **************************************************************/

/* Structure for context identification */
typedef struct
{
   unsigned int index;
   N8_Boolean_t    inUse;
   N8_Unit_t    unitID;
   int          structureID;
} N8_ContextHandle_t;


/****************************************************************************
 *
 *  Packet Common: Common declarations required for private key computations.
 *
 ***************************************************************************/

/* DES Key length used in RNG and Packet Processing */
#define N8_DES_KEY_LENGTH 8

/* The permissible values for Cipher depend on the value of Protocol, 
    and include the values ARC4 and DES. */
typedef enum
{
   N8_CIPHER_ARC4     = 1,
   N8_CIPHER_DES
} N8_Cipher_t;

#ifdef __cplusplus
}
#endif

#endif



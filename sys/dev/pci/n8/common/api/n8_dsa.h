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
 * @(#) n8_dsa.h 1.25@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_dsa.h
 *  @brief Contains definitions for DSA functions.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 05/19/03 brr   Clean up include files.
 * 02/20/02 brr   Removed references to the queue structure.
 * 01/29/02 bac   Renamed NUMBER_OF_BYTES_IN_VALUE to DSA_SIGN_LENGTH.
 * 11/19/01 bac   Added n8_DSAValidateKey.
 * 11/10/01 spm   Replaced use of ceil() with homemade integer version.
 * 10/12/01 dkm   Moved public portion to n8_pub_common.h and n8_pub_pk.h.
 * 10/01/01 hml   Added UnitID and SKSKeyHandle in the KeyObject and moved
 *                the material struct in here.
 * 09/10/01 bac   Test for public key and modulus lengths in
 *                N8_DSAInitializeKey (BUG #129).  Added
 *                N8_DSA_PUBLIC_KEY_LENGTH_[MIN|MAX] and
 *                N8_DSA_PRIVATE_KEY_LENGTH.
 * 08/16/01 mmd   Now including n8_driver_api.h instead of simon_driver_api.h.
 * 07/30/01 bac   Fixed length calculation macros to accept and use a queue
 *                control pointer.  Lots of logic changes to be seemlessly
 *                portable to the FPGA.
 * 06/28/01 bac   Minor typo fix.
 * 06/25/01 bac   Lots of changes to affect functionality with the v1.0.1 QMgr
 * 05/22/01 mel   Original version.
 ****************************************************************************/
#ifndef N8_DSA_H
#define N8_DSA_H

#include "n8_common.h"
#include "n8_pub_pk.h"
#include "n8_pk_common.h"
#include "n8_pub_errors.h"
#include "n8_driver_api.h"
#include "n8_sks.h"

#define DSA_SIGN_LENGTH 20

/* Macros for extracting DSA parameters from the Big Number Cache.
   These take key_length as a parameter.  Some will behave badly
   if key_length is odd. */

/* Several DSA quanties are of fixed length.  They must fit in the least number of BNC
 * digits to ensure the uppermost digit is not zero.
 */
#define PK_DSA_N_Bit_Length                     160
#define PK_DSA_E1_Bit_Length                    160
#define PK_DSA_Q_Bit_Length                     160
#define PK_DSA_R_Bit_Length                     160
#define PK_DSA_S_Bit_Length                     160
#define PK_DSA_X_Bit_Length                     160

/* length of private key in bytes */
#define N8_DSA_PRIVATE_KEY_LENGTH               20
#define N8_DSA_PUBLIC_KEY_LENGTH_MIN            64
#define N8_DSA_PUBLIC_KEY_LENGTH_MAX           512 

/* BNC Layout:
 *  N                 160 bits (rounded up)
 *  E1                160 bits (rounded up)
 *  R                 160 bits (rounded up)
 *  S                 160 bits (rounded up)
 *  P                 key length
 *  g R mod p         key length
 *  Q                 160 bits (rounded up)
 *  X                 160 bits (rounded up)
 *  cp                1 digit
 */
#define PK_DSA_RoundUp(__L)                     (CEIL((__L) , (SIMON_BITS_PER_DIGIT)))

#define PK_DSA_N_Byte_Length              PKDIGITS_TO_BYTES(PK_DSA_N_BNC_Length)
#define PK_DSA_E1_Byte_Length             PKDIGITS_TO_BYTES(PK_DSA_E1_BNC_Length)
#define PK_DSA_R_Byte_Length              PKDIGITS_TO_BYTES(PK_DSA_R_BNC_Length)
#define PK_DSA_S_Byte_Length              PKDIGITS_TO_BYTES(PK_DSA_S_BNC_Length)

#define PK_DSA_P_Byte_Length(__KL)        ((__KL)*PK_Bytes_Per_BigNum_Digit)
#define PK_DSA_P_Param_Offset             (0)

#define PK_DSA_G_Byte_Length(__KL)         ((__KL)*PK_Bytes_Per_BigNum_Digit)
#define PK_DSA_GR_MOD_P_Byte_Length(__KL)  ((__KL)*PK_Bytes_Per_BigNum_Digit)
#define PK_DSA_GR_MOD_P_Param_Offset(__KL) PKDIGITS_TO_BYTES(__KL)

#define PK_DSA_INVK_Byte_Length           (2*PK_Bytes_Per_BigNum_Digit)
#define PK_DSA_Q_Byte_Length              PKDIGITS_TO_BYTES(PK_DSA_Q_BNC_Length)
#define PK_DSA_Q_Param_Offset(__KL)       PKDIGITS_TO_BYTES(2*(__KL))

#define PK_DSA_X_Byte_Length              PKDIGITS_TO_BYTES(PK_DSA_X_BNC_Length)
#define PK_DSA_X_Param_Offset(__KL)       PKDIGITS_TO_BYTES(2*(__KL)+PK_DSA_Q_BNC_Length)

#define PK_DSA_Y_Byte_Length(__KL)         ((__KL)*PK_Bytes_Per_BigNum_Digit)

#define PK_DSA_CP_Byte_Length             (PK_Bytes_Per_BigNum_Digit)
#define PK_DSA_CP_Param_Offset(__KL)       \
    (PK_DSA_X_Param_Offset((__KL)) + PK_DSA_X_Byte_Length)

#define PK_DSA_Param_Byte_Length(__KL)      \
    PK_DSA_P_Byte_Length((__KL)) +        \
    PK_DSA_GR_MOD_P_Byte_Length((__KL)) + \
    PK_DSA_Q_Byte_Length +               \
    PK_DSA_X_Byte_Length +               \
    PK_DSA_CP_Byte_Length


#define PK_DSA_SKS_Word_Length(__KL)       (8*(__KL)+20)

/* BNC Offsets */
#define PK_DSA_N_BNC_Offset   (0)
#define PK_DSA_E1_BNC_Offset  (PK_DSA_N_BNC_Offset +  PK_DSA_N_BNC_Length)
#define PK_DSA_R_BNC_Offset   (PK_DSA_E1_BNC_Offset + PK_DSA_E1_BNC_Length)
#define PK_DSA_S_BNC_Offset   (PK_DSA_R_BNC_Offset +  PK_DSA_R_BNC_Length)
#define PK_DSA_P_BNC_Offset   (PK_DSA_S_BNC_Offset +  PK_DSA_S_BNC_Length)

/* BNC Lengths */
#define PK_DSA_N_BNC_Length   PK_DSA_RoundUp(PK_DSA_N_Bit_Length)
#define PK_DSA_E1_BNC_Length  PK_DSA_RoundUp(PK_DSA_E1_Bit_Length)
#define PK_DSA_R_BNC_Length   PK_DSA_RoundUp(PK_DSA_R_Bit_Length)
#define PK_DSA_S_BNC_Length   PK_DSA_RoundUp(PK_DSA_S_Bit_Length)
#define PK_DSA_Q_BNC_Length   PK_DSA_RoundUp(PK_DSA_Q_Bit_Length)
#define PK_DSA_X_BNC_Length   PK_DSA_RoundUp(PK_DSA_X_Bit_Length)
#define PK_DSA_P_BNC_Length(__KL)        (__KL)
#define PK_DSA_G_BNC_Length(__KL)        (__KL)
#define PK_DSA_GR_MOD_P_BNC_Length(__KL) (__KL)
#define PK_DSA_Y_BNC_Length(__KL)        (__KL)
#define PK_DSA_CP_BNC_Length              1

typedef struct 
{
    uint32_t    q;                /* secret prime number.  used for
                                     private operations. */
    uint32_t    cp;               /* additive inverse of the
                                     multiplicative inverse of the least
                                     significant digit of p, mod 2^128.
                                     used for private operations. */ 
    uint32_t    gR_mod_p;         /* g*R mod q.  (private) */
    uint32_t    p;                /* secret prime number.  used for
                                     private operations. */
    uint32_t    privateKey;       /* private key x */
    uint32_t    publicKey;        /* public key y */
    uint32_t    g;                /* generator = h^(p-1)/q mod p, where h is less
                                     than p - 1 and g > 1. */
    uint32_t    paramBlock;       /* parameter block in PK format,
                                     suitable for loading directly into
                                     the Big Num Cache (BNC) */
} N8_DSAKeyObjectPhysical_t;

/**********************************************************************
 * function prototypes
 **********************************************************************/
N8_Status_t n8_DSAValidateKey(const N8_DSAKeyMaterial_t *material_p,
                              const N8_KeyType_t type);
#endif /* N8_DSA_H */



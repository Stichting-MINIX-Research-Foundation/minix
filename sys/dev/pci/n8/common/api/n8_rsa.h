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
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_rsa.h
 *  @brief RSA Function support.
 *
 * Support for all RSA functionality.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 05/19/03 brr   Clean up include files.
 * 05/07/02 bac   Added N8_RSA_SKS_KEY_LENGTH_DIGITS_MIN and
 *                N8_RSA_SKS_KEY_LENGTH_DIGITS_MAX for proper key screening for
 *                the SKS.
 * 04/08/02 bac   Reimplemented byte length macros to be more efficient.
 * 02/20/02 brr   Removed references to the queue structure.
 * 10/15/01 bac   Fixed a bug where many macros were somehow multiply defined.
 * 10/14/01 bac   Fixed PK_RSA_?_Param_Length() macros to be more efficient.
 * 10/12/01 dkm   Moved public portion to n8_pub_common.h and n8_pub_pk.h.
 * 10/11/01 bac   Added a prototype for N8_print_rsa_parameters
 * 10/11/01 bac   More correctons for BUG #180.
 * 10/08/01 bac   Changes for the case where len(p) != len(q) (BUG #180).  Most
 *                of the RSA Byte Length and RSA Param Byte Length macros
 *                changed. 
 * 10/01/01 hml   Added unitID and SKSHandle to keyObject and moved 
 *                material structure in here.
 * 08/22/01 bac   Replaced use of ceil() with homemade integer version.
 * 08/21/01 bac   Fixes to allow the use of odd key lengths (Bug #174).
 * 08/17/01 hml   Fixed PK_RSA_HALF_LENGTH macro for bug 172
 * 08/16/01 mmd   Now including n8_driver_api.h instead of simon_driver_api.h.
 * 08/21/01 bac   Fixes to allow the use of odd key lengths (Bug #174).
 * 08/17/01 hml   Fixed PK_RSA_HALF_LENGTH macro for bug 172
 * 07/30/01 bac   Added queue pointer to length calculations.
 * 06/25/01 bac   Changed N8_RSAKeyObject_t to use physical addresses.  Added
 *                N8_RSAKeyObjectVirtual_t to be the virtual address counterpart.
 * 06/19/01 bac   Correct use of kernel memory.
 * 05/30/01 bac   Structure documentation.
 * 04/10/01 bac   Changes to come up to standard.
 * 03/21/01 bac   Original version.
 ****************************************************************************/
#ifndef N8_RSA_H
#define N8_RSA_H

#include "n8_common.h"
#include "n8_pub_pk.h"
#include "n8_enqueue_common.h"
#include "n8_pk_common.h"
#include "n8_pub_errors.h"
#include "n8_driver_api.h"
#include "n8_sks.h"


#define N8_RSA_KEY_LENGTH_MIN           1       /* length in bytes */
#define N8_RSA_KEY_LENGTH_MAX           512     /* length in bytes */

#define N8_RSA_SKS_KEY_LENGTH_DIGITS_MIN 2      /* sks must be at least two BNC digits */
#define N8_RSA_SKS_KEY_LENGTH_DIGITS_MAX 32     /* and less than 32 */

/* PKE Parameter lengths in bytes */
#define PK_RSA_HALF_DIGITS(__KL)               (((__KL) + 1) / 2)
#define PK_RSA_FULL_LENGTH(__KL)               ((__KL)*PK_Bytes_Per_BigNum_Digit)
#define PK_RSA_FULL_LENGTH(__KL)               ((__KL)*PK_Bytes_Per_BigNum_Digit)
#define PK_RSA_HALF_LENGTH(__KL)               ((((__KL)+1)/2) * PK_Bytes_Per_BigNum_Digit)
#define PK_RSA_Input_Byte_Length(__KL)         PK_RSA_FULL_LENGTH((__KL))
#define PK_RSA_Output_Byte_Length(__KL)        PK_RSA_FULL_LENGTH((__KL))

#define PK_RSA_P_Byte_Length(__KP) ((__KP)->pDigits)*PK_Bytes_Per_BigNum_Digit
#define PK_RSA_Q_Byte_Length(__KP) ((__KP)->qDigits)*PK_Bytes_Per_BigNum_Digit
#define PK_RSA_DP_Byte_Length(__KP)            PK_RSA_P_Byte_Length((__KP))
#define PK_RSA_DQ_Byte_Length(__KP)            PK_RSA_Q_Byte_Length((__KP))
#define PK_RSA_R_MOD_P_Byte_Length(__KP)       PK_RSA_P_Byte_Length((__KP))
#define PK_RSA_R_MOD_Q_Byte_Length(__KP)       PK_RSA_Q_Byte_Length((__KP))
#define PK_RSA_R_MOD_N_Byte_Length(__KP)       PK_RSA_FULL_LENGTH((__KP)->privateKeyDigits)
#define PK_RSA_N_Byte_Length(__KP)             PK_RSA_FULL_LENGTH((__KP)->privateKeyDigits)
#define PK_RSA_U_Byte_Length(__KP)             PK_RSA_Q_Byte_Length((__KP))
#define PK_RSA_CP_Byte_Length                  PK_Bytes_Per_BigNum_Digit
#define PK_RSA_CQ_Byte_Length                  PK_Bytes_Per_BigNum_Digit
#define PK_RSA_CN_Byte_Length                  PK_Bytes_Per_BigNum_Digit

#define PK_RSA_Param_Byte_Length(__KP)     \
    PK_RSA_P_Byte_Length((__KP))         + \
    PK_RSA_Q_Byte_Length((__KP))         + \
    PK_RSA_DP_Byte_Length((__KP))        + \
    PK_RSA_DQ_Byte_Length((__KP))        + \
    PK_RSA_R_MOD_P_Byte_Length((__KP))   + \
    PK_RSA_R_MOD_Q_Byte_Length((__KP))   + \
    PK_RSA_N_Byte_Length((__KP))         + \
    PK_RSA_U_Byte_Length((__KP))         + \
    PK_RSA_CP_Byte_Length                + \
    PK_RSA_CQ_Byte_Length

/*
 * Offsets into the PKE RSA Parameter block
 *
 * Note most parameters are KeyLength/2 digits
 * Exceptions are:
 * N (KeyLength digits),
 * CP (1 digit)
 * CQ (1 digit)
 */
/* Parameter lengths in digits.
 * Params:  (__KP)     N8_RSAKeyObject_t pointer.
 *          __QP     QueueControl_t pointer.
 */
#define PK_RSA_P_Param_Length(__KP)        (__KP)->pDigits
#define PK_RSA_Q_Param_Length(__KP)        (__KP)->qDigits
#define PK_RSA_DP_Param_Length(__KP)       PK_RSA_P_Param_Length(__KP)
#define PK_RSA_DQ_Param_Length(__KP)       PK_RSA_Q_Param_Length(__KP)
#define PK_RSA_R_MOD_P_Param_Length(__KP)  PK_RSA_P_Param_Length(__KP)
#define PK_RSA_R_MOD_Q_Param_Length(__KP)  PK_RSA_Q_Param_Length(__KP)
#define PK_RSA_N_Param_Length(__KP)        (__KP)->privateKeyDigits
#define PK_RSA_U_Param_Length(__KP)        PK_RSA_Q_Param_Length(__KP)
#define PK_RSA_CP_Param_Length                  1
#define PK_RSA_CQ_Param_Length                  1

/* parameter offsets in digits */
#define PK_RSA_P_Param_Offset(__KP)       (0)
#define PK_RSA_Q_Param_Offset(__KP) \
(PK_RSA_P_Param_Offset(__KP)       + PK_RSA_P_Param_Length(__KP))
#define PK_RSA_DP_Param_Offset(__KP) \
(PK_RSA_Q_Param_Offset(__KP)       + PK_RSA_Q_Param_Length(__KP))
#define PK_RSA_DQ_Param_Offset(__KP) \
(PK_RSA_DP_Param_Offset(__KP)      + PK_RSA_DP_Param_Length(__KP))
#define PK_RSA_R_MOD_P_Param_Offset(__KP) \
(PK_RSA_DQ_Param_Offset(__KP)      + PK_RSA_DQ_Param_Length(__KP))
#define PK_RSA_R_MOD_Q_Param_Offset(__KP) \
(PK_RSA_R_MOD_P_Param_Offset(__KP) + PK_RSA_R_MOD_P_Param_Length(__KP))
#define PK_RSA_N_Param_Offset(__KP) \
(PK_RSA_R_MOD_Q_Param_Offset(__KP) + PK_RSA_R_MOD_Q_Param_Length(__KP))
#define PK_RSA_U_Param_Offset(__KP) \
(PK_RSA_N_Param_Offset(__KP)       + PK_RSA_N_Param_Length(__KP))
#define PK_RSA_CP_Param_Offset(__KP) \
(PK_RSA_U_Param_Offset(__KP)       + PK_RSA_U_Param_Length(__KP))
#define PK_RSA_CQ_Param_Offset(__KP) \
(PK_RSA_CP_Param_Offset(__KP)      + PK_RSA_CP_Param_Length)
/* parameter offsets in bytes */
#define PK_RSA_P_Param_Byte_Offset(__KP)       (0)

#define PK_RSA_Q_Param_Byte_Offset(__KP) \
(PK_RSA_P_Param_Byte_Offset(__KP)       + PK_RSA_P_Byte_Length(__KP))
#define PK_RSA_DP_Param_Byte_Offset(__KP) \
(PK_RSA_Q_Param_Byte_Offset(__KP)       + PK_RSA_Q_Byte_Length(__KP))
#define PK_RSA_DQ_Param_Byte_Offset(__KP) \
(PK_RSA_DP_Param_Byte_Offset(__KP)      + PK_RSA_DP_Byte_Length(__KP))
#define PK_RSA_R_MOD_P_Param_Byte_Offset(__KP) \
(PK_RSA_DQ_Param_Byte_Offset(__KP)      + PK_RSA_DQ_Byte_Length(__KP))
#define PK_RSA_R_MOD_Q_Param_Byte_Offset(__KP) \
(PK_RSA_R_MOD_P_Param_Byte_Offset(__KP) + PK_RSA_R_MOD_P_Byte_Length(__KP))
#define PK_RSA_N_Param_Byte_Offset(__KP) \
(PK_RSA_R_MOD_Q_Param_Byte_Offset(__KP) + PK_RSA_R_MOD_Q_Byte_Length(__KP))
#define PK_RSA_U_Param_Byte_Offset(__KP)  \
(PK_RSA_N_Param_Byte_Offset(__KP)       + PK_RSA_N_Byte_Length(__KP))
#define PK_RSA_CP_Param_Byte_Offset(__KP) \
(PK_RSA_U_Param_Byte_Offset(__KP)       + PK_RSA_U_Byte_Length(__KP))
#define PK_RSA_CQ_Param_Byte_Offset(__KP) \
(PK_RSA_CP_Param_Byte_Offset(__KP)      + PK_RSA_CP_Byte_Length)

typedef struct 
{
   N8_Buffer_t           *p;               /* secret prime number.  used for
                                            * private operations. */
   N8_Buffer_t           *q;               /* secret prime number.  used for
                                            * private operations. */
                                           /* d is the private key */
   N8_Buffer_t           *dp;              /* d mod ((p-1) mod p) */
   N8_Buffer_t           *dq;              /* d mod ((q-1) mod q) */
   N8_Buffer_t           *R_mod_p;         /* R mod p.  (private) */
   N8_Buffer_t           *R_mod_q;         /* R mod q.  (private) */
   N8_Buffer_t           *R_mod_n;         /* R mod n.  (public) */
   N8_Buffer_t           *n;               /* modulus:  n=pq */
   N8_Buffer_t           *u;               /* used for Chinese Remainder Theorem:
                                            * u = p^-1 mod q.  private
                                            * operations only. */
   N8_Buffer_t           *cp;              /* additive inverse of the
                                            * multiplicative inverse of the least
                                            * significant digit of p, mod 2^128.
                                            * used for private operations. */ 
   N8_Buffer_t           *cq;              /* same as cp but for q.  used for
                                            * private operations. */
   N8_Buffer_t           *cn;              /* same as cp but for n.  used for
                                            * public operations. */
  
   N8_Buffer_t           *key;             /* this is either e, the public key or
                                            * d, the private key, depending upon
                                            * the context this object is being
                                            * used. */
   N8_Buffer_t           *paramBlock;      /* parameter block in PK format,
                                            * suitable for loading directly into
                                            * the Big Num Cache (BNC). */
   N8_MemoryHandle_t     kMemStruct;       /* kernel memory structure for the
                                            * allocated space */
} N8_RSAKeyObjectVirtual_t;

/**********************************************************************
 * function prototypes
 **********************************************************************/
void N8_print_rsa_parameters(unsigned char *p_block,
                             N8_RSAKeyObject_t *key_p);
#endif /* N8_RSA_H */

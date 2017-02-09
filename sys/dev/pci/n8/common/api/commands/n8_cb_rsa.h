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
 * @(#) n8_cb_rsa.h 1.31@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_cb_rsa.h
 *  @brief RSA Command Blocks
 *
 * Command Block Generation for RSA
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 12/17/03 bac   Added cb_rsaPublicDecrypt
 * 09/10/02 brr   Set command complete bit on last command block.
 * 05/07/02 bac   Moved cb_exponentiate to the public interface.
 * 05/09/02 brr   Changed prototype for allocateBNCConstants.
 * 02/21/01 brr   Perform BNC constant initialization once upon startup.
 * 12/12/01 bac   Fixed Bug #371 by adding new define
 *                N8_CB_EXPONENTIATE_WITH_PRECOMPUTES_NUMCMDS.
 * 10/19/01 bac   Added support for SKS operations and RSA Decrypt for non-CRT.
 * 10/08/01 bac   Changes for the case where len(p) != len(q) (BUG #180).
 *                Everywhere a data length was assumed to half the key length,
 *                it has been replaced with the specific dependency on the
 *                length of p or q.
 * 09/14/01 bac   Changed some formal parameters to be 'const'.
 * 09/05/01 bac   Changed formal parameter name in cb_computeCX.
 * 08/24/01 bac   Added *_NUMCMDS macros for all command generators.
 * 08/21/01 bac   Fixes to allow the use of odd key lengths (Bug #174).
 * 06/25/01 bac   Added static allocation of BNC constants, more conversion to
 *                physical addresses.
 * 06/19/01 bac   Corrected signatures for use of physical addresses.
 * 04/24/01 bac   Removed dead prototypes for functions moved to n8_util
 * 04/11/01 bac   Standarization -- mainly _t suffix additions
 * 04/05/01 bac   Added prototype for cb_computeU.
 * 03/05/01 bac   Initial Version
 ****************************************************************************/

#ifndef N8_CB_RSA_H
#define N8_CB_RSA_H

#include "n8_rsa.h"
#include "n8_enqueue_common.h"

/* defines indicating the number of command blocks for each computation */
#define N8_CB_COMPUTE_U_NUMCMDS                      4
#define N8_CB_COMPUTE_DX_NUMCMDS                     6
#define N8_CB_COMPUTE_CX_NUMCMDS                     5
#define N8_CB_COMPUTE_RMODX_NUMCMDS                  3
#define N8_CB_EXPONENTIATE_NUMCMDS                  10
#define N8_CB_EXPONENTIATE_WITH_PRECOMPUTES_NUMCMDS  8
#define N8_CB_RSA_PUBLICDECRYPT_NUMCMDS             10
#define N8_CB_RSA_ENCRYPT_NUMCMDS \
        N8_CB_EXPONENTIATE_WITH_PRECOMPUTES_NUMCMDS

#define N8_CB_RSA_DECRYPT_NUMCMDS(__KP)     cb_RSADecryptOperations((__KP))

/*  function prototypes */
N8_Status_t cb_computeU(API_Request_t       *req_p,
                        const uint32_t       p,
                        const uint32_t       q,
                        uint32_t             result_a,
                        const int            p_length_bytes,
                        const int            q_length_bytes,
                        PK_CMD_BLOCK_t      *cmdBuf_p,
                        PK_CMD_BLOCK_t     **next_cmdBuf_p);
N8_Status_t cb_computeDX(API_Request_t *req_p,
                         const uint32_t X,
                         const uint32_t d,
                         uint32_t       result,
                         const int      key_length_bytes,
                         const int      X_length_bytes,
                         PK_CMD_BLOCK_t *cmdBuf_p,
                         PK_CMD_BLOCK_t **next_cmdBuf_p,
                         const int      chip);
N8_Status_t cb_computeCX(const API_Request_t *req_p,
                         const uint32_t X,
                         const uint32_t result,
                         const int      X_length_bytes,
                         PK_CMD_BLOCK_t *cmdBuf_p,
                         PK_CMD_BLOCK_t **next_cmdBuf_p,
                         const N8_Unit_t unitID,
                         int             lastCmdBlock);
N8_Status_t cb_computeRmodX(const API_Request_t *req_p,
                            const uint32_t      X,
                            const uint32_t      result,
                            const unsigned int  key_length_bytes,
                            PK_CMD_BLOCK_t      *cmdBuf_p,
                            PK_CMD_BLOCK_t      **next_cmdBuf_p,
                            int                 lastCmdBlock);
N8_Status_t cb_rsaEncrypt(API_Request_t           *req_p,
                          const N8_RSAKeyObject_t *key_p,
                          const uint32_t           origMsg_a,
                          const uint32_t           result_a,
                          PK_CMD_BLOCK_t          *cmdBuf_p,
                          const N8_Unit_t          unitID);
N8_Status_t cb_rsaDecrypt(API_Request_t           *req_p,
                          const N8_RSAKeyObject_t *key,
                          const uint32_t           origMsg_a,
                          const uint32_t           result_a,
                          PK_CMD_BLOCK_t          *cmdBuf_p,
                          const N8_Unit_t          unitID);
N8_Status_t cb_exponentiate(API_Request_t           *req_p,
                            const uint32_t           origMsg_a,
                            const uint32_t           modulus_a,
                            const unsigned int       modulus_length_bytes,
                            const uint32_t           exponent_a,
                            const unsigned int       exp_length_bytes,
                            const uint32_t           result_a,
                            PK_CMD_BLOCK_t          *cb_p,
                            const N8_Unit_t          unitID);
N8_Status_t cb_rsaPublicDecrypt(const API_Request_t *req_p,
                                const uint32_t       modulus,
                                const unsigned int   modulus_length_bytes,
                                const uint32_t       origMsg_a,
                                const uint32_t       result_a,
                                const uint32_t       exponent_a,
                                const unsigned int   exp_length_bytes,
                                PK_CMD_BLOCK_t      *cb_p,
                                const N8_Unit_t      unitID);
unsigned int cb_RSADecryptOperations(const N8_RSAKeyObject_t *key_p);

N8_Status_t allocateBNCConstants(unsigned long bcnAddress);

#endif /* N8_CB_RSA_H */


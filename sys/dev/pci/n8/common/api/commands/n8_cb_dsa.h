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
 * @(#) n8_cb_dsa.h 1.14@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_cb_dsa.h
 *  @brief DSA Command Blocks
 *
 * Command Block Generation for DSA
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 10/31/01 bac   Corrected N8_CB_DSA_SIGN_NUMCMDS(__KP) for SKS support.
 * 08/24/01 bac   Added *_NUMCMDS macros for all command generators.
 * 06/28/01 bac   Minor typo corrections.
 * 06/26/01 bac   Even more on conversion to use physical memory.
 * 06/25/01 bac   More on conversion to use physical memory.
 * 05/22/01 mel   Initial Version
 ****************************************************************************/

#ifndef N8_CB_DSA_H
#define N8_CB_DSA_H

#include "n8_dsa.h"
#include "n8_enqueue_common.h"

/* defines indicating the number of command blocks for each computation */
#define N8_CB_COMPUTE_GRMODX_NUMCMDS     5
#define N8_CB_DSA_SIGN_NUMCMDS(__KP)     cb_DSASignOperations((__KP))
#define N8_CB_DSA_VERIFY_NUMCMDS         19

/*  function prototypes */
N8_Status_t cb_computeGRmodX(API_Request_t                   *req_p,
                             const int                        modulusDigits,
                             const uint32_t                   g_a,
                             const uint32_t                   X_a,
                             const uint32_t                   res_a,
                             PK_CMD_BLOCK_t                  *buf_p,
                             PK_CMD_BLOCK_t                  **next_cmdBuf_p);

N8_Status_t cb_dsaSign(API_Request_t *req_p,
                       const N8_DSAKeyObject_t *key,
                       uint32_t n_a,
                       uint32_t paramBlock_a,
                       uint32_t msgHash_a,
                       uint32_t rValue_a,
                       uint32_t sValue_a,
                       PK_CMD_BLOCK_t *buf_p);

N8_Status_t cb_dsaVerify(API_Request_t *req_p,
                         const N8_DSAKeyObject_t *key,
                         uint32_t q_a, 
                         uint32_t cp_a, 
                         uint32_t gR_mod_p_a, 
                         uint32_t p_a, 
                         uint32_t publicKey_a, 
                         uint32_t mh_a,
                         uint32_t r_a,
                         uint32_t s_a,
                         uint32_t res_a,
                         PK_CMD_BLOCK_t *cmdBuf_p);

unsigned int cb_DSASignOperations(const N8_DSAKeyObject_t *key_p);

#endif /* N8_CB_DSA_H */


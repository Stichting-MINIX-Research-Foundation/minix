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
 * @(#) n8_cb_dh.h 1.9@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_cb_dh.h
 *  @brief DSA Command Blocks
 *
 * Command Block Generation for Diffie-Hellman
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 09/14/01 bac   Added computation of gRmodP in cb_precomputeDHValues.
 *                Changed cb_computeGXmodp to be cb_computeGXmodp_short and
 *                cb_computeGXmodp_long.  The former uses the precomputed value
 *                for gRmodP and the latter computes it.  Other changes from the
 *                code review.
 * 08/24/01 bac   Changed method signatures and added *NUMCMDS defines.
 * 07/12/01 bac   Changed signatures for cb_computeGXmodp to accept uint32_t not
 *                pointers.
 * 06/22/01 mel   Initial Version
 ****************************************************************************/

#ifndef N8_CB_DH_H
#define N8_CB_DH_H

#include "n8_cb_dsa.h"
#include "n8_cb_rsa.h"
#include "n8_pub_errors.h"


/* macros indicating the number of command blocks for each computation */
#define N8_CB_PRECOMPUTE_DHVALUES_NUMCMDS \
  (N8_CB_COMPUTE_CX_NUMCMDS + N8_CB_COMPUTE_RMODX_NUMCMDS + 5)
#define N8_CB_COMPUTE_G_XMODP_NUMCMDS_SHORT     6
#define N8_CB_COMPUTE_G_XMODP_NUMCMDS_LONG      8

/*  function prototypes */
N8_Status_t cb_precomputeDHValues(const API_Request_t *req_p,
                                  const uint32_t g_a,
                                  const uint32_t p_a,
                                  const uint32_t RmodP_a,
                                  const uint32_t gRmodP_a,
                                  const uint32_t cp_a,
                                  const unsigned int modulusLength,
                                  PK_CMD_BLOCK_t *cmdBuf_p,
                                  const N8_Unit_t unitID);

N8_Status_t cb_computeGXmodp_long(const API_Request_t *req_p,
                                  const uint32_t g_a,
                                  const uint32_t x_a,
                                  const uint32_t p_a,
                                  const uint32_t cp_a,
                                  const uint32_t RmodP_a,
                                  const uint32_t result_a,
                                  const unsigned int modulusLengthBytes,
                                  PK_CMD_BLOCK_t *cmdBuf_p);

N8_Status_t cb_computeGXmodp_short(const API_Request_t *req_p,
                                   const uint32_t x_a,
                                   const uint32_t p_a,
                                   const uint32_t cp_a,
                                   const uint32_t gRmodP_a,
                                   const uint32_t result_a,
                                   const unsigned int modulusLengthBytes,
                                   PK_CMD_BLOCK_t *cmdBuf_p);
#endif /* N8_CB_DH_H */


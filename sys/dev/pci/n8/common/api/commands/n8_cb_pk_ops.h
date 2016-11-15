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
/** @file n8_cb_pk_ops.h
 *  @brief PKE Base Operation command generators.
 *
 * Command block generators for all PKE Base Operations.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 05/07/02 bac   Original version.
 ****************************************************************************/
#ifndef N8_CB_PK_OPS_H
#define N8_CB_PK_OPS_H

#include "n8_common.h"
#include "n8_pub_pk.h"
#include "n8_pk_common.h"
#include "n8_enqueue_common.h"

/* Define the number of commands required per operation */
#define N8_CB_PK_OP_NUMCMDS                 5

/* Prototypes for external interfaces */
N8_Status_t cb_pk_op(API_Request_t     *req_p,
                     const uint32_t     shifted_opcode,
                     const uint32_t     a_a,
                     const unsigned int a_length_bytes,
                     const uint32_t     b_a,
                     const unsigned int b_length_bytes,
                     const uint32_t     modulus_a,
                     const unsigned int mod_length_bytes,
                     const unsigned int max_length_bytes,
                     const uint32_t     result_a,
                     PK_CMD_BLOCK_t    *cb_p,
                     PK_CMD_BLOCK_t   **next_cb_pp);
#endif /* N8_CB_PK_OPS_H */

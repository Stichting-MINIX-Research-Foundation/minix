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
 * @(#) n8_pub_request.h 1.4@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_pub_request
 *  @brief Common declarations for request operations.
 *
 * Public header file for request operations.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history: 
 * 06/14/02 hml   Deleted proto for N8_PacketRequestSet.
 * 06/10/02 hml   First completed version.
 * 06/05/02 hml   Original version.
 ****************************************************************************/
#ifndef N8_PUB_REQUEST_H
#define N8_PUB_REQUEST_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "n8_pub_common.h"

/*****************************************************************************
 * #defines 
 *****************************************************************************/
#define N8_MAX_REQUEST_DATA_BYTES  40000
#define N8_COMMAND_BLOCK_BYTES 1024
#define N8_CTX_BYTES NEXT_WORD_SIZE(sizeof(EA_ARC4_CTX))

/*****************************************************************************
 * Structures/type definitions
 *****************************************************************************/
typedef unsigned char *N8_RequestHandle_t;

/*****************************************************************************
 * Function prototypes
 *****************************************************************************/
N8_Status_t N8_RequestAllocate(const unsigned int   nDataBytes,
                               N8_RequestHandle_t  *request_p, 
                               N8_Buffer_t        **bufferVirt_pp);

N8_Status_t N8_RequestFree(N8_RequestHandle_t requestHandle);

#ifdef __cplusplus
}
#endif

#endif /* N8_PUB_REQUEST_H */

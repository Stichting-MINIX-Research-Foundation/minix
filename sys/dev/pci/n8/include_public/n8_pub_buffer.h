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
 * @(#) n8_pub_buffer.h 1.6@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_pub_buffer
 *  @brief Common declarations for buffer operations.
 *
 * Public header file for buffer operations.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 06/05/03 brr   Added prototypes for N8_DeleteSizedBuffer & N8_DeleteBuffer.
 * 02/06/02 hml   Original version.
 ****************************************************************************/
#ifndef N8_PUB_BUFFER_H
#define N8_PUB_BUFFER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "n8_pub_common.h"

#define MAX_BC_NUM_BYTE_SIZE     512

typedef unsigned char *N8_BufferHandle_t;

typedef struct{
   int length;
   int offset;
   char value[MAX_BC_NUM_BYTE_SIZE + 1];  /* 4096 bits plus a null */
} ATTR;

/*****************************************************************************
 * Function prototypes
 *****************************************************************************/
N8_Status_t N8_BufferAllocate(N8_BufferHandle_t *bufferHandle_p, 
                              N8_Buffer_t **virtualAddress_pp,
                              const unsigned int size);

N8_Status_t N8_BufferFree(N8_BufferHandle_t bufferHandle);

#ifndef __KERNEL__
N8_Status_t N8_CreateSizedBufferFromATTR(N8_SizedBuffer_t *buf_p,
                                 const ATTR *attr_p);
N8_Status_t N8_CreateSizedBufferFromString(const char *str_p, N8_SizedBuffer_t *buf_p);
N8_Status_t N8_DeleteSizedBuffer(N8_SizedBuffer_t *buf_p);

N8_Buffer_t *N8_CreateBuffer(const char *e);
N8_Status_t N8_DeleteBuffer(N8_Buffer_t *buf_p);
#endif /* __KERNEL__ */

#ifdef __cplusplus
}
#endif

#endif /* N8_PUB_BUFFER_H */



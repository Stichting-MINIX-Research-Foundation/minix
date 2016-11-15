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
 * @(#) n8_pub_sks.h 1.5@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_pub_sks
 *  @brief Common declarations for secure key storage operations.
 *
 * Public header file for secure key storage operations.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 12/07/01 bac   Cleaned up several of the prototypes.
 * 10/12/01 dkm   Original version. Adapted from n8_sks.h.
 ****************************************************************************/
#ifndef N8_PUB_SKS_H
#define N8_PUB_SKS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "n8_pub_common.h"

/*****************************************************************************
 * Function prototypes
 *****************************************************************************/
N8_Status_t N8_SKSFree(N8_SKSKeyHandle_t* keyHandle);

N8_Status_t N8_SKSAllocateRSA(N8_RSAKeyMaterial_t* key_material,
                              const N8_Buffer_t *keyEntryName);

N8_Status_t N8_SKSAllocateDSA(N8_DSAKeyMaterial_t* key_material,
                              const N8_Buffer_t *keyEntryName);

N8_Status_t N8_SKSReset(N8_Unit_t unitSpecifier);

N8_Status_t N8_SKSGetKeyHandle(const N8_Buffer_t* keyEntryName,
                               N8_SKSKeyHandle_t* keyHandle_p);

N8_Status_t N8_SKSDisplay(N8_SKSKeyHandle_t* keyHandle, 
                          char *string, size_t stringlength); 

#ifdef __cplusplus
}
#endif

#endif


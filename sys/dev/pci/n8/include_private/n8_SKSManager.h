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
 * @(#) n8_SKSManager.h 1.2@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_SKSManager.h
 *  @brief NSP2000 SKS Manager
 *
 * This file has declarations for the SKS Kernel resident manager file.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 02/25/02 msz   File created.
 ****************************************************************************/
#ifndef _N8_SKSMANAGER_H
#define _N8_SKSMANAGER_H

#include "n8_pub_common.h"

/*****************************************************************************
 * #defines 
 *****************************************************************************/

/*****************************************************************************
 * Structures/type definitions
 *****************************************************************************/

/* The following two strucutres are used in the ioctl processing to     */
/* make the calls from user space to the kernel functions below.        */
/* n8_SKSResetUnit and n8_SKSAllocate don't require their own           */
/* structures as they are passing only one parameter each.              */

typedef struct
{
   unsigned int    targetSKS;
   const uint32_t *data_p;
   int             data_length;
   uint32_t        offset;
} n8_SKSWriteParams_t;

typedef struct
{
   N8_SKSKeyHandle_t  *keyHandle_p;
   unsigned int       status;
} n8_setStatusParams_t;

/*****************************************************************************
 * Function prototypes
 *****************************************************************************/

N8_Status_t n8_SKSWrite(const unsigned int  targetSKS,
                        const uint32_t     *data_p, 
                        const int           data_length,
                        const uint32_t      offset,
                        const int           fromUser);

N8_Status_t n8_SKSResetUnit(const N8_Unit_t targetSKS);

N8_Status_t n8_SKSAllocate(N8_SKSKeyHandle_t *keyHandle_p);

N8_Status_t n8_SKSsetStatus(N8_SKSKeyHandle_t *keyHandle_p,
                            unsigned int       status);

#endif

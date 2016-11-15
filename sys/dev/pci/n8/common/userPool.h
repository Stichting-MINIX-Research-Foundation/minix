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
 * @(#) userPool.h 1.5@(#)
 *****************************************************************************/
/*****************************************************************************/
/** @file userPool.h
 *  @brief This file contains prototypes for user pool library.
 *
 * This file provides a prototypes for functions used manage the user pool.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history: 
 * 05/08/03 brr   Modified user pools to take advantage of the support for
 *                multiple memory banks.
 * 07/11/02 brr   Do not fail userPoolInit if banks is 0.
 * 07/02/02 brr   Added prototype for userPoolStats.
 * 06/24/02 brr   File created.
 ****************************************************************************/
#ifndef USERPOOL_H
#define USERPOOL_H

#ifdef VX_BUILD

#undef  DEF_USER_POOL_BANKS
#define DEF_USER_POOL_BANKS 0
#define  userPoolCount() 0
#define  userPoolInit(p1, p2, p3)
#define  userPoolRelease()
#define  userPoolFreePool(p1)

#else

void userPoolDisplay(void);
int  userPoolCount(void);

void userPoolInit( unsigned int  banks,
                   unsigned long size,
                   unsigned long granularity);

void userPoolRelease(void);

unsigned long userPoolAlloc(int sessID);

void userPoolFree(int sessID);
void userPoolFreePool(int bankIndex);

void userPoolStats(MemStats_t *memStatsPtr);

#endif

#endif /* USERPOOL_H */

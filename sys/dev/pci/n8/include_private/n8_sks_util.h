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
 * @(#) n8_sks_util.h 1.3@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_sks_util.h
 *
 * Defines useful routines used by n8_sks.c (driver) and
 * n8_daemon_user.c (userspace daemon).
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 01/22/02 spm   Original version.
 ****************************************************************************/
#ifndef _N8_SKS_UTIL_H
#define _N8_SKS_UTIL_H

/*****************************************************************************
 * #defines 
 *****************************************************************************/

/*****************************************************************************
 * Structures/type definitions
 *****************************************************************************/

/*****************************************************************************
 * Function prototypes
 *****************************************************************************/
N8_Status_t n8_ComputeKeyLength(const N8_SKSKeyType_t keyType,
                                const uint32_t keyLength,
                                unsigned int *len_p);

#endif /* _N8_SKS_UTIL_H */


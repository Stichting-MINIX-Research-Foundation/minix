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

/*****************************************************************************/
/** @file n8_precompute.h
 *  @brief Precomputes IPAD and OPAD values for SSL and TLS MACs.
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 08/06/02 bac   Added hashed_key_p to n8_precompute_tls_ipad_opad so that
 *                the key isn't modified in place.
 * 01/23/02 dws   Changed n8_precompute_tls_ipad_opad parameter 
 *                secret_length to a pointer.
 * 01/22/02 dws   Original version.
 ****************************************************************************/
#ifndef N8_PRECOMPUTE_H
#define N8_PRECOMPUTE_H
#include "n8_pub_common.h"

N8_Status_t n8_precompute_ssl_ipad_opad(N8_Buffer_t *secret_p, 
                                        uint32_t *ipad_p, 
                                        uint32_t *opad_p);

N8_Status_t n8_precompute_tls_ipad_opad(const N8_HashAlgorithm_t hash_alg,
                                        const N8_Buffer_t *secret_p,
                                        N8_Buffer_t *hashed_key_p,
                                        uint32_t *secret_length_p,
                                        uint32_t *ipad_p, 
                                        uint32_t *opad_p);
#endif

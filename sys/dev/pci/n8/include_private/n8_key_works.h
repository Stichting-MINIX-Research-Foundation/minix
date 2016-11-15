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
 * @(#) n8_key_works.h 1.6@(#)
 *****************************************************************************/

/*****************************************************************************
 * n8_key_works.h
 *
 * Header file for all Random Number Generator (RNG) functions.
 *****************************************************************************
 * Revision history:
 * 12/11/02 brr   Include n8_pub_common to pick up needed types.
 * 09/20/01 bac   Changed Key_cblock to key_cblock_t to follow coding stds.
 * 06/24/01 bac   Changes to bring up to coding standards.
 * 04/10/01 mel   Original version.
 *
 ***************************************************************************/
#ifndef N8_KEY_WORKS_H
#define N8_KEY_WORKS_H

#include "n8_pub_common.h"

#define NUM_WEAK_KEY    16
/**************************************************
 * Structure Definitions
 **************************************************/

typedef unsigned char key_cblock_t[8];
#define DES_KEY_SIZE    (sizeof(key_cblock_t))

/**************************************************
 * Function Definitions
 **************************************************/
N8_Boolean_t checkKeyForWeakness (key_cblock_t *key_p);
N8_Boolean_t checkKeyParity  (key_cblock_t *key_p);
void forceParity(key_cblock_t *key_p);

#endif /* N8_KEY_WORKS_H */

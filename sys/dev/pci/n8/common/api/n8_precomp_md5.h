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
/** @file n8_precomp_md5.c
 *  @brief Private version of md5 for n8_precompute_*.
 *
 * A more detailed description of the file.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 01/22/02 dws   Original version.
 ****************************************************************************/
#ifndef N8_PRECOMP_MD5_H
#define N8_PRECOMP_MD5_H

/********************/
/* MD5 definitions. */
/********************/
#define MD5_LONG unsigned int
#define MD5_CBLOCK         64
#define MD5_LBLOCK         (MD5_CBLOCK/4)
#define MD5_DIGEST_LENGTH  16

typedef struct
{
   MD5_LONG A,B,C,D;
   MD5_LONG Nl,Nh;
   MD5_LONG data[MD5_LBLOCK];
   int num;
} N8_PRECOMP_MD5_CTX;



void n8_precomp_MD5_Init(N8_PRECOMP_MD5_CTX *c);
void n8_precomp_MD5_Update(N8_PRECOMP_MD5_CTX *c, 
                           const void *data, 
                           unsigned long len);
void n8_precomp_MD5_Final(unsigned char *md, 
                          N8_PRECOMP_MD5_CTX *c);

#endif




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

static char const n8_id[] = "$Id: n8_precomp_md5.c,v 1.1 2008/10/30 12:02:15 darran Exp $";
/*****************************************************************************/
/** @file n8_precomp_md5.c
 *  @brief Private version of md5 for n8_precompute_*.
 *
 * A more detailed description of the file.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 03/06/02 brr   Removed openssl includes.
 * 02/18/02 spm   Removed #includes of usr headers.   Converted printf's to
 *                DBG's.
 * 01/22/02 dws   Original version.
 ****************************************************************************/
/** @defgroup subsystem_name Subsystem Title (not used for a header file)
 */

#define NO_ASM

#include "n8_precomp_md5.h"
#include "n8_precomp_md5_locl.h"

#define INIT_DATA_A (unsigned long)0x67452301L
#define INIT_DATA_B (unsigned long)0xefcdab89L
#define INIT_DATA_C (unsigned long)0x98badcfeL
#define INIT_DATA_D (unsigned long)0x10325476L

void n8_precomp_MD5_Init(N8_PRECOMP_MD5_CTX *c)
{
   c->A=INIT_DATA_A;
   c->B=INIT_DATA_B;
   c->C=INIT_DATA_C;
   c->D=INIT_DATA_D;
   c->Nl=0;
   c->Nh=0;
   c->num=0;
}

/*************************************************/
/* This stuff implements n8_precomp_MD5_Update() */
/*************************************************/
#ifndef md5_block_host_order
void n8_precomp_md5_block_host_order (N8_PRECOMP_MD5_CTX *c, const void *data, int num)
        {
        const MD5_LONG *X=data;
        register unsigned long A,B,C,D;
        /*
         * In case you wonder why A-D are declared as long and not
         * as MD5_LONG. Doing so results in slight performance
         * boost on LP64 architectures. The catch is we don't
         * really care if 32 MSBs of a 64-bit register get polluted
         * with eventual overflows as we *save* only 32 LSBs in
         * *either* case. Now declaring 'em long excuses the compiler
         * from keeping 32 MSBs zeroed resulting in 13% performance
         * improvement under SPARC Solaris7/64 and 5% under AlphaLinux.
         * Well, to be honest it should say that this *prevents* 
         * performance degradation.
         *
         *                              <appro@fy.chalmers.se>
         */

#ifdef HMAC_DEBUG
        DBG(("*********************************************************\n"));
        DBG(("Entering md5_block_host_order ***************************\n"));
#endif

        A=c->A;
        B=c->B;
        C=c->C;
        D=c->D;

        for (;num--;X+=HASH_LBLOCK)
                {
        /* Round 0 */
        R0(A,B,C,D,X[ 0], 7,0xd76aa478L);
        R0(D,A,B,C,X[ 1],12,0xe8c7b756L);
        R0(C,D,A,B,X[ 2],17,0x242070dbL);
        R0(B,C,D,A,X[ 3],22,0xc1bdceeeL);
        R0(A,B,C,D,X[ 4], 7,0xf57c0fafL);
        R0(D,A,B,C,X[ 5],12,0x4787c62aL);
        R0(C,D,A,B,X[ 6],17,0xa8304613L);
        R0(B,C,D,A,X[ 7],22,0xfd469501L);
        R0(A,B,C,D,X[ 8], 7,0x698098d8L);
        R0(D,A,B,C,X[ 9],12,0x8b44f7afL);
        R0(C,D,A,B,X[10],17,0xffff5bb1L);
        R0(B,C,D,A,X[11],22,0x895cd7beL);
        R0(A,B,C,D,X[12], 7,0x6b901122L);
        R0(D,A,B,C,X[13],12,0xfd987193L);
        R0(C,D,A,B,X[14],17,0xa679438eL);
        R0(B,C,D,A,X[15],22,0x49b40821L);
        /* Round 1 */
        R1(A,B,C,D,X[ 1], 5,0xf61e2562L);
        R1(D,A,B,C,X[ 6], 9,0xc040b340L);
        R1(C,D,A,B,X[11],14,0x265e5a51L);
        R1(B,C,D,A,X[ 0],20,0xe9b6c7aaL);
        R1(A,B,C,D,X[ 5], 5,0xd62f105dL);
        R1(D,A,B,C,X[10], 9,0x02441453L);
        R1(C,D,A,B,X[15],14,0xd8a1e681L);
        R1(B,C,D,A,X[ 4],20,0xe7d3fbc8L);
        R1(A,B,C,D,X[ 9], 5,0x21e1cde6L);
        R1(D,A,B,C,X[14], 9,0xc33707d6L);
        R1(C,D,A,B,X[ 3],14,0xf4d50d87L);
        R1(B,C,D,A,X[ 8],20,0x455a14edL);
        R1(A,B,C,D,X[13], 5,0xa9e3e905L);
        R1(D,A,B,C,X[ 2], 9,0xfcefa3f8L);
        R1(C,D,A,B,X[ 7],14,0x676f02d9L);
        R1(B,C,D,A,X[12],20,0x8d2a4c8aL);
        /* Round 2 */
        R2(A,B,C,D,X[ 5], 4,0xfffa3942L);
        R2(D,A,B,C,X[ 8],11,0x8771f681L);
        R2(C,D,A,B,X[11],16,0x6d9d6122L);
        R2(B,C,D,A,X[14],23,0xfde5380cL);
        R2(A,B,C,D,X[ 1], 4,0xa4beea44L);
        R2(D,A,B,C,X[ 4],11,0x4bdecfa9L);
        R2(C,D,A,B,X[ 7],16,0xf6bb4b60L);
        R2(B,C,D,A,X[10],23,0xbebfbc70L);
        R2(A,B,C,D,X[13], 4,0x289b7ec6L);
        R2(D,A,B,C,X[ 0],11,0xeaa127faL);
        R2(C,D,A,B,X[ 3],16,0xd4ef3085L);
        R2(B,C,D,A,X[ 6],23,0x04881d05L);
        R2(A,B,C,D,X[ 9], 4,0xd9d4d039L);
        R2(D,A,B,C,X[12],11,0xe6db99e5L);
        R2(C,D,A,B,X[15],16,0x1fa27cf8L);
        R2(B,C,D,A,X[ 2],23,0xc4ac5665L);
        /* Round 3 */
        R3(A,B,C,D,X[ 0], 6,0xf4292244L);
        R3(D,A,B,C,X[ 7],10,0x432aff97L);
        R3(C,D,A,B,X[14],15,0xab9423a7L);
        R3(B,C,D,A,X[ 5],21,0xfc93a039L);
        R3(A,B,C,D,X[12], 6,0x655b59c3L);
        R3(D,A,B,C,X[ 3],10,0x8f0ccc92L);
        R3(C,D,A,B,X[10],15,0xffeff47dL);
        R3(B,C,D,A,X[ 1],21,0x85845dd1L);
        R3(A,B,C,D,X[ 8], 6,0x6fa87e4fL);
        R3(D,A,B,C,X[15],10,0xfe2ce6e0L);
        R3(C,D,A,B,X[ 6],15,0xa3014314L);
        R3(B,C,D,A,X[13],21,0x4e0811a1L);
        R3(A,B,C,D,X[ 4], 6,0xf7537e82L);
        R3(D,A,B,C,X[11],10,0xbd3af235L);
        R3(C,D,A,B,X[ 2],15,0x2ad7d2bbL);
        R3(B,C,D,A,X[ 9],21,0xeb86d391L);

        A = c->A += A;
        B = c->B += B;
        C = c->C += C;
        D = c->D += D;
#ifdef HMAC_DEBUG
        DBG(("Final      {A=%08lx B=%08lx C=%08lx D=%08lx}\n", A, B, C, D));
#endif
                }
#ifdef HMAC_DEBUG
        DBG(("Leaving md5_block_host_order ****************************\n"));
        DBG(("*********************************************************\n"));
#endif
        }
#endif

#ifndef md5_block_data_order
#ifdef X
#undef X
#endif
void n8_precomp_md5_block_data_order (N8_PRECOMP_MD5_CTX *c, const void *data_, int num)
        {
        const unsigned char *data=data_;
        register unsigned long A,B,C,D,l;
        /*
         * In case you wonder why A-D are declared as long and not
         * as MD5_LONG. Doing so results in slight performance
         * boost on LP64 architectures. The catch is we don't
         * really care if 32 MSBs of a 64-bit register get polluted
         * with eventual overflows as we *save* only 32 LSBs in
         * *either* case. Now declaring 'em long excuses the compiler
         * from keeping 32 MSBs zeroed resulting in 13% performance
         * improvement under SPARC Solaris7/64 and 5% under AlphaLinux.
         * Well, to be honest it should say that this *prevents* 
         * performance degradation.
         *
         *                              <appro@fy.chalmers.se>
         */
#ifndef MD32_XARRAY
        /* See comment in crypto/sha/sha_locl.h for details. */
        unsigned long   XX0, XX1, XX2, XX3, XX4, XX5, XX6, XX7,
                        XX8, XX9,XX10,XX11,XX12,XX13,XX14,XX15;
# define X(i)   XX##i
#else
        MD5_LONG XX[MD5_LBLOCK];
# define X(i)   XX[i]
#endif

#ifdef HMAC_DEBUG
        DBG(("*********************************************************\n"));
        DBG(("Entering md5_block_data_order ***************************\n"));
#endif

        A=c->A;
        B=c->B;
        C=c->C;
        D=c->D;

        for (;num--;)
                {
        HOST_c2l(data,l); X( 0)=l;              HOST_c2l(data,l); X( 1)=l;
        /* Round 0 */
        R0(A,B,C,D,X( 0), 7,0xd76aa478L);       HOST_c2l(data,l); X( 2)=l;
        R0(D,A,B,C,X( 1),12,0xe8c7b756L);       HOST_c2l(data,l); X( 3)=l;
        R0(C,D,A,B,X( 2),17,0x242070dbL);       HOST_c2l(data,l); X( 4)=l;
        R0(B,C,D,A,X( 3),22,0xc1bdceeeL);       HOST_c2l(data,l); X( 5)=l;
        R0(A,B,C,D,X( 4), 7,0xf57c0fafL);       HOST_c2l(data,l); X( 6)=l;
        R0(D,A,B,C,X( 5),12,0x4787c62aL);       HOST_c2l(data,l); X( 7)=l;
        R0(C,D,A,B,X( 6),17,0xa8304613L);       HOST_c2l(data,l); X( 8)=l;
        R0(B,C,D,A,X( 7),22,0xfd469501L);       HOST_c2l(data,l); X( 9)=l;
        R0(A,B,C,D,X( 8), 7,0x698098d8L);       HOST_c2l(data,l); X(10)=l;
        R0(D,A,B,C,X( 9),12,0x8b44f7afL);       HOST_c2l(data,l); X(11)=l;
        R0(C,D,A,B,X(10),17,0xffff5bb1L);       HOST_c2l(data,l); X(12)=l;
        R0(B,C,D,A,X(11),22,0x895cd7beL);       HOST_c2l(data,l); X(13)=l;
        R0(A,B,C,D,X(12), 7,0x6b901122L);       HOST_c2l(data,l); X(14)=l;
        R0(D,A,B,C,X(13),12,0xfd987193L);       HOST_c2l(data,l); X(15)=l;
        R0(C,D,A,B,X(14),17,0xa679438eL);
        R0(B,C,D,A,X(15),22,0x49b40821L);
        /* Round 1 */
        R1(A,B,C,D,X( 1), 5,0xf61e2562L);
        R1(D,A,B,C,X( 6), 9,0xc040b340L);
        R1(C,D,A,B,X(11),14,0x265e5a51L);
        R1(B,C,D,A,X( 0),20,0xe9b6c7aaL);
        R1(A,B,C,D,X( 5), 5,0xd62f105dL);
        R1(D,A,B,C,X(10), 9,0x02441453L);
        R1(C,D,A,B,X(15),14,0xd8a1e681L);
        R1(B,C,D,A,X( 4),20,0xe7d3fbc8L);
        R1(A,B,C,D,X( 9), 5,0x21e1cde6L);
        R1(D,A,B,C,X(14), 9,0xc33707d6L);
        R1(C,D,A,B,X( 3),14,0xf4d50d87L);
        R1(B,C,D,A,X( 8),20,0x455a14edL);
        R1(A,B,C,D,X(13), 5,0xa9e3e905L);
        R1(D,A,B,C,X( 2), 9,0xfcefa3f8L);
        R1(C,D,A,B,X( 7),14,0x676f02d9L);
        R1(B,C,D,A,X(12),20,0x8d2a4c8aL);
        /* Round 2 */
        R2(A,B,C,D,X( 5), 4,0xfffa3942L);
        R2(D,A,B,C,X( 8),11,0x8771f681L);
        R2(C,D,A,B,X(11),16,0x6d9d6122L);
        R2(B,C,D,A,X(14),23,0xfde5380cL);
        R2(A,B,C,D,X( 1), 4,0xa4beea44L);
        R2(D,A,B,C,X( 4),11,0x4bdecfa9L);
        R2(C,D,A,B,X( 7),16,0xf6bb4b60L);
        R2(B,C,D,A,X(10),23,0xbebfbc70L);
        R2(A,B,C,D,X(13), 4,0x289b7ec6L);
        R2(D,A,B,C,X( 0),11,0xeaa127faL);
        R2(C,D,A,B,X( 3),16,0xd4ef3085L);
        R2(B,C,D,A,X( 6),23,0x04881d05L);
        R2(A,B,C,D,X( 9), 4,0xd9d4d039L);
        R2(D,A,B,C,X(12),11,0xe6db99e5L);
        R2(C,D,A,B,X(15),16,0x1fa27cf8L);
        R2(B,C,D,A,X( 2),23,0xc4ac5665L);
        /* Round 3 */
        R3(A,B,C,D,X( 0), 6,0xf4292244L);
        R3(D,A,B,C,X( 7),10,0x432aff97L);
        R3(C,D,A,B,X(14),15,0xab9423a7L);
        R3(B,C,D,A,X( 5),21,0xfc93a039L);
        R3(A,B,C,D,X(12), 6,0x655b59c3L);
        R3(D,A,B,C,X( 3),10,0x8f0ccc92L);
        R3(C,D,A,B,X(10),15,0xffeff47dL);
        R3(B,C,D,A,X( 1),21,0x85845dd1L);
        R3(A,B,C,D,X( 8), 6,0x6fa87e4fL);
        R3(D,A,B,C,X(15),10,0xfe2ce6e0L);
        R3(C,D,A,B,X( 6),15,0xa3014314L);
        R3(B,C,D,A,X(13),21,0x4e0811a1L);
        R3(A,B,C,D,X( 4), 6,0xf7537e82L);
        R3(D,A,B,C,X(11),10,0xbd3af235L);
        R3(C,D,A,B,X( 2),15,0x2ad7d2bbL);
        R3(B,C,D,A,X( 9),21,0xeb86d391L);

        A = c->A += A;
        B = c->B += B;
        C = c->C += C;
        D = c->D += D;
#ifdef HMAC_DEBUG
        DBG(("Final      {A=%08lx B=%08lx C=%08lx D=%08lx}\n", A, B, C, D));
#endif
                }
#ifdef HMAC_DEBUG
        DBG(("Leaving md5_block_data_order ****************************\n"));
        DBG(("*********************************************************\n"));
#endif
        }
#endif




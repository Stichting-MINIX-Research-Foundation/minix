/* crypto/md/md5_dgst.c */
/* Copyright (C) 1995-1997 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

/*
 * Portions Copyright (c) 2007,2009 by Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   https://www.isc.org/
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "md5_locl.h"
#include "cdefs.h"
#include "osdep.h"

#ifdef USE_MD5 /* Added by ogud@tis.com 1998/1/26 */

const char *MD5_version="MD5 part of SSLeay 0.8.1 19-Jul-1997";

/* Implemented from RFC1321 The MD5 Message-Digest Algorithm
 */

#define INIT_DATA_A (unsigned long)0x67452301L
#define INIT_DATA_B (unsigned long)0xefcdab89L
#define INIT_DATA_C (unsigned long)0x98badcfeL
#define INIT_DATA_D (unsigned long)0x10325476L

#ifndef NOPROTO
static void md5_block(MD5_CTX *c, unsigned long *p);
#else
static void md5_block();
#endif

void MD5_Init(c)
MD5_CTX *c;
	{
	c->A=INIT_DATA_A;
	c->B=INIT_DATA_B;
	c->C=INIT_DATA_C;
	c->D=INIT_DATA_D;
	c->Nl=0;
	c->Nh=0;
	c->num=0;
	}

void MD5_Update(c, data, len)
MD5_CTX *c;
const register unsigned char *data;
unsigned long len;
	{
	register ULONG *p;
	int sw,sc;
	ULONG l;

	if (len == 0) return;

	l=(c->Nl+(len<<3))&0xffffffffL;
	/* 95-05-24 eay Fixed a bug with the overflow handling, thanks to
	 * Wei Dai <weidai@eskimo.com> for pointing it out. */
	if (l < c->Nl) /* overflow */
		c->Nh++;
	c->Nh+=(len>>29);
	c->Nl=l;

	if (c->num != 0)
		{
		p=c->data;
		sw=c->num>>2;
		sc=c->num&0x03;

		if ((c->num+len) >= MD5_CBLOCK)
			{
			l= p[sw];
			p_c2l(data,l,sc);
			p[sw++]=l;
			for (; sw<MD5_LBLOCK; sw++)
				{
				c2l(data,l);
				p[sw]=l;
				}
			len-=(MD5_CBLOCK-c->num);

			md5_block(c,p);
			c->num=0;
			/* drop through and do the rest */
			}
		else
			{
			int ew,ec;

			c->num+=(int)len;
			if ((sc+len) < 4) /* ugly, add char's to a word */
				{
				l= p[sw];
				p_c2l_p(data,l,sc,len);
				p[sw]=l;
				}
			else
				{
				ew=(c->num>>2);
				ec=(c->num&0x03);
				l= p[sw];
				p_c2l(data,l,sc);
				p[sw++]=l;
				for (; sw < ew; sw++)
					{ c2l(data,l); p[sw]=l; }
				if (ec)
					{
					c2l_p(data,l,ec);
					p[sw]=l;
					}
				}
			return;
			}
		}
	/* we now can process the input data in blocks of MD5_CBLOCK
	 * chars and save the leftovers to c->data. */
	p=c->data;
	while (len >= MD5_CBLOCK)
		{
#if defined(L_ENDIAN) || defined(B_ENDIAN)
		memcpy(p,data,MD5_CBLOCK);
		data+=MD5_CBLOCK;
#ifdef B_ENDIAN
		for (sw=(MD5_LBLOCK/4); sw; sw--)
			{
			Endian_Reverse32(p[0]);
			Endian_Reverse32(p[1]);
			Endian_Reverse32(p[2]);
			Endian_Reverse32(p[3]);
			p+=4;
			}
#endif
#else
		for (sw=(MD5_LBLOCK/4); sw; sw--)
			{
			c2l(data,l); *(p++)=l;
			c2l(data,l); *(p++)=l;
			c2l(data,l); *(p++)=l;
			c2l(data,l); *(p++)=l; 
			} 
#endif
		p=c->data;
		md5_block(c,p);
		len-=MD5_CBLOCK;
		}
	sc=(int)len;
	c->num=sc;
	if (sc)
		{
		sw=sc>>2;	/* words to copy */
#ifdef L_ENDIAN
		p[sw]=0;
		memcpy(p,data,sc);
#else
		sc&=0x03;
		for ( ; sw; sw--)
			{ c2l(data,l); *(p++)=l; }
		c2l_p(data,l,sc);
		*p=l;
#endif
		}
	}

static void md5_block(c, X)
MD5_CTX *c;
register ULONG *X;
	{
	register ULONG A,B,C,D;

	A=c->A;
	B=c->B;
	C=c->C;
	D=c->D;

	/* Round 0 */
	LOCL_R0(A,B,C,D,X[ 0], 7,0xd76aa478L);
	LOCL_R0(D,A,B,C,X[ 1],12,0xe8c7b756L);
	LOCL_R0(C,D,A,B,X[ 2],17,0x242070dbL);
	LOCL_R0(B,C,D,A,X[ 3],22,0xc1bdceeeL);
	LOCL_R0(A,B,C,D,X[ 4], 7,0xf57c0fafL);
	LOCL_R0(D,A,B,C,X[ 5],12,0x4787c62aL);
	LOCL_R0(C,D,A,B,X[ 6],17,0xa8304613L);
	LOCL_R0(B,C,D,A,X[ 7],22,0xfd469501L);
	LOCL_R0(A,B,C,D,X[ 8], 7,0x698098d8L);
	LOCL_R0(D,A,B,C,X[ 9],12,0x8b44f7afL);
	LOCL_R0(C,D,A,B,X[10],17,0xffff5bb1L);
	LOCL_R0(B,C,D,A,X[11],22,0x895cd7beL);
	LOCL_R0(A,B,C,D,X[12], 7,0x6b901122L);
	LOCL_R0(D,A,B,C,X[13],12,0xfd987193L);
	LOCL_R0(C,D,A,B,X[14],17,0xa679438eL);
	LOCL_R0(B,C,D,A,X[15],22,0x49b40821L);
	/* Round 1 */
	LOCL_R1(A,B,C,D,X[ 1], 5,0xf61e2562L);
	LOCL_R1(D,A,B,C,X[ 6], 9,0xc040b340L);
	LOCL_R1(C,D,A,B,X[11],14,0x265e5a51L);
	LOCL_R1(B,C,D,A,X[ 0],20,0xe9b6c7aaL);
	LOCL_R1(A,B,C,D,X[ 5], 5,0xd62f105dL);
	LOCL_R1(D,A,B,C,X[10], 9,0x02441453L);
	LOCL_R1(C,D,A,B,X[15],14,0xd8a1e681L);
	LOCL_R1(B,C,D,A,X[ 4],20,0xe7d3fbc8L);
	LOCL_R1(A,B,C,D,X[ 9], 5,0x21e1cde6L);
	LOCL_R1(D,A,B,C,X[14], 9,0xc33707d6L);
	LOCL_R1(C,D,A,B,X[ 3],14,0xf4d50d87L);
	LOCL_R1(B,C,D,A,X[ 8],20,0x455a14edL);
	LOCL_R1(A,B,C,D,X[13], 5,0xa9e3e905L);
	LOCL_R1(D,A,B,C,X[ 2], 9,0xfcefa3f8L);
	LOCL_R1(C,D,A,B,X[ 7],14,0x676f02d9L);
	LOCL_R1(B,C,D,A,X[12],20,0x8d2a4c8aL);
	/* Round 2 */
	LOCL_R2(A,B,C,D,X[ 5], 4,0xfffa3942L);
	LOCL_R2(D,A,B,C,X[ 8],11,0x8771f681L);
	LOCL_R2(C,D,A,B,X[11],16,0x6d9d6122L);
	LOCL_R2(B,C,D,A,X[14],23,0xfde5380cL);
	LOCL_R2(A,B,C,D,X[ 1], 4,0xa4beea44L);
	LOCL_R2(D,A,B,C,X[ 4],11,0x4bdecfa9L);
	LOCL_R2(C,D,A,B,X[ 7],16,0xf6bb4b60L);
	LOCL_R2(B,C,D,A,X[10],23,0xbebfbc70L);
	LOCL_R2(A,B,C,D,X[13], 4,0x289b7ec6L);
	LOCL_R2(D,A,B,C,X[ 0],11,0xeaa127faL);
	LOCL_R2(C,D,A,B,X[ 3],16,0xd4ef3085L);
	LOCL_R2(B,C,D,A,X[ 6],23,0x04881d05L);
	LOCL_R2(A,B,C,D,X[ 9], 4,0xd9d4d039L);
	LOCL_R2(D,A,B,C,X[12],11,0xe6db99e5L);
	LOCL_R2(C,D,A,B,X[15],16,0x1fa27cf8L);
	LOCL_R2(B,C,D,A,X[ 2],23,0xc4ac5665L);
	/* Round 3 */
	LOCL_R3(A,B,C,D,X[ 0], 6,0xf4292244L);
	LOCL_R3(D,A,B,C,X[ 7],10,0x432aff97L);
	LOCL_R3(C,D,A,B,X[14],15,0xab9423a7L);
	LOCL_R3(B,C,D,A,X[ 5],21,0xfc93a039L);
	LOCL_R3(A,B,C,D,X[12], 6,0x655b59c3L);
	LOCL_R3(D,A,B,C,X[ 3],10,0x8f0ccc92L);
	LOCL_R3(C,D,A,B,X[10],15,0xffeff47dL);
	LOCL_R3(B,C,D,A,X[ 1],21,0x85845dd1L);
	LOCL_R3(A,B,C,D,X[ 8], 6,0x6fa87e4fL);
	LOCL_R3(D,A,B,C,X[15],10,0xfe2ce6e0L);
	LOCL_R3(C,D,A,B,X[ 6],15,0xa3014314L);
	LOCL_R3(B,C,D,A,X[13],21,0x4e0811a1L);
	LOCL_R3(A,B,C,D,X[ 4], 6,0xf7537e82L);
	LOCL_R3(D,A,B,C,X[11],10,0xbd3af235L);
	LOCL_R3(C,D,A,B,X[ 2],15,0x2ad7d2bbL);
	LOCL_R3(B,C,D,A,X[ 9],21,0xeb86d391L);

	c->A+=A&0xffffffffL;
	c->B+=B&0xffffffffL;
	c->C+=C&0xffffffffL;
	c->D+=D&0xffffffffL;
	}

void MD5_Final(md, c)
unsigned char *md;
MD5_CTX *c;
	{
	register int i,j;
	register ULONG l;
	register ULONG *p;
	static unsigned char end[4]={0x80,0x00,0x00,0x00};
	unsigned char *cp=end;

	/* c->num should definitely have room for at least one more byte. */
	p=c->data;
	j=c->num;
	i=j>>2;

	/* purify often complains about the following line as an
	 * Uninitialized Memory Read.  While this can be true, the
	 * following p_c2l macro will reset l when that case is true.
	 * This is because j&0x03 contains the number of 'valid' bytes
	 * already in p[i].  If and only if j&0x03 == 0, the UMR will
	 * occur but this is also the only time p_c2l will do
	 * l= *(cp++) instead of l|= *(cp++)
	 * Many thanks to Alex Tang <altitude@cic.net> for pickup this
	 * 'potential bug' */
#ifdef PURIFY
	if ((j&0x03) == 0) p[i]=0;
#endif
	l=p[i];
	p_c2l(cp,l,j&0x03);
	p[i]=l;
	i++;
	/* i is the next 'undefined word' */
	if (c->num >= MD5_LAST_BLOCK)
		{
		for (; i<MD5_LBLOCK; i++)
			p[i]=0;
		md5_block(c,p);
		i=0;
		}
	for (; i<(MD5_LBLOCK-2); i++)
		p[i]=0;
	p[MD5_LBLOCK-2]=c->Nl;
	p[MD5_LBLOCK-1]=c->Nh;
	md5_block(c,p);
	cp=md;
	l=c->A; l2c(l,cp);
	l=c->B; l2c(l,cp);
	l=c->C; l2c(l,cp);
	l=c->D; l2c(l,cp);

	/* clear stuff, md5_block may be leaving some stuff on the stack
	 * but I'm not worried :-) */
	c->num=0;
/*	memset((char *)&c,0,sizeof(c));*/
	}

#ifdef undef
int printit(l)
unsigned long *l;
	{
	int i,ii;

	for (i=0; i<2; i++)
		{
		for (ii=0; ii<8; ii++)
			{
			fprintf(stderr,"%08lx ",l[i*8+ii]);
			}
		fprintf(stderr,"\n");
		}
	}
#endif
#endif /* USE_MD5 */

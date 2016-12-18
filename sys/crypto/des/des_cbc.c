/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: des_cbc.c,v 1.8 2005/12/11 12:20:52 christos Exp $");

#include <sys/types.h>

#include <crypto/des/des_locl.h>

void des_ncbc_encrypt(const unsigned char *in, unsigned char *out, long length,
	     des_key_schedule schedule, des_cblock *ivec, int enc)
{
	register DES_LONG tin0,tin1;
	register DES_LONG tout0,tout1,xor0,xor1;
	register long l=length;
	DES_LONG tin[2];
	unsigned char *iv;

	iv = &(*ivec)[0];

	if (enc)
		{
		c2l(iv,tout0);
		c2l(iv,tout1);
		for (l-=8; l>=0; l-=8)
			{
			c2l(in,tin0);
			c2l(in,tin1);
			tin0^=tout0; tin[0]=tin0;
			tin1^=tout1; tin[1]=tin1;
			des_encrypt1((DES_LONG *)tin,schedule,DES_ENCRYPT);
			tout0=tin[0]; l2c(tout0,out);
			tout1=tin[1]; l2c(tout1,out);
			}
		if (l != -8)
			{
			c2ln(in,tin0,tin1,l+8);
			tin0^=tout0; tin[0]=tin0;
			tin1^=tout1; tin[1]=tin1;
			des_encrypt1((DES_LONG *)tin,schedule,DES_ENCRYPT);
			tout0=tin[0]; l2c(tout0,out);
			tout1=tin[1]; l2c(tout1,out);
			}
		iv = &(*ivec)[0];
		l2c(tout0,iv);
		l2c(tout1,iv);
		}
	else
		{
		c2l(iv,xor0);
		c2l(iv,xor1);
		for (l-=8; l>=0; l-=8)
			{
			c2l(in,tin0); tin[0]=tin0;
			c2l(in,tin1); tin[1]=tin1;
			des_encrypt1((DES_LONG *)tin,schedule,DES_DECRYPT);
			tout0=tin[0]^xor0;
			tout1=tin[1]^xor1;
			l2c(tout0,out);
			l2c(tout1,out);
			xor0=tin0;
			xor1=tin1;
			}
		if (l != -8)
			{
			c2l(in,tin0); tin[0]=tin0;
			c2l(in,tin1); tin[1]=tin1;
			des_encrypt1((DES_LONG *)tin,schedule,DES_DECRYPT);
			tout0=tin[0]^xor0;
			tout1=tin[1]^xor1;
			l2cn(tout0,tout1,out,l+8);
			xor0=tin0;
			xor1=tin1;
			}
		iv = &(*ivec)[0];
		l2c(xor0,iv);
		l2c(xor1,iv);
		}
	tin0=tin1=tout0=tout1=xor0=xor1=0;
	tin[0]=tin[1]=0;
}

void des_ede3_cbc_encrypt(const unsigned char *input, unsigned char *output,
	     long length, des_key_schedule ks1, des_key_schedule ks2,
	     des_key_schedule ks3, des_cblock *ivec, int enc)
{
	register DES_LONG tin0,tin1;
	register DES_LONG tout0,tout1,xor0,xor1;
	register const unsigned char *in;
	unsigned char *out;
	register long l=length;
	DES_LONG tin[2];
	unsigned char *iv;

	in=input;
	out=output;
	iv = &(*ivec)[0];

	if (enc)
		{
		c2l(iv,tout0);
		c2l(iv,tout1);
		for (l-=8; l>=0; l-=8)
			{
			c2l(in,tin0);
			c2l(in,tin1);
			tin0^=tout0;
			tin1^=tout1;

			tin[0]=tin0;
			tin[1]=tin1;
			des_encrypt3((DES_LONG *)tin,ks1,ks2,ks3);
			tout0=tin[0];
			tout1=tin[1];

			l2c(tout0,out);
			l2c(tout1,out);
			}
		if (l != -8)
			{
			c2ln(in,tin0,tin1,l+8);
			tin0^=tout0;
			tin1^=tout1;

			tin[0]=tin0;
			tin[1]=tin1;
			des_encrypt3((DES_LONG *)tin,ks1,ks2,ks3);
			tout0=tin[0];
			tout1=tin[1];

			l2c(tout0,out);
			l2c(tout1,out);
			}
		iv = &(*ivec)[0];
		l2c(tout0,iv);
		l2c(tout1,iv);
		}
	else
		{
		register DES_LONG t0,t1;

		c2l(iv,xor0);
		c2l(iv,xor1);
		for (l-=8; l>=0; l-=8)
			{
			c2l(in,tin0);
			c2l(in,tin1);

			t0=tin0;
			t1=tin1;

			tin[0]=tin0;
			tin[1]=tin1;
			des_decrypt3((DES_LONG *)tin,ks1,ks2,ks3);
			tout0=tin[0];
			tout1=tin[1];

			tout0^=xor0;
			tout1^=xor1;
			l2c(tout0,out);
			l2c(tout1,out);
			xor0=t0;
			xor1=t1;
			}
		if (l != -8)
			{
			c2l(in,tin0);
			c2l(in,tin1);

			t0=tin0;
			t1=tin1;

			tin[0]=tin0;
			tin[1]=tin1;
			des_decrypt3((DES_LONG *)tin,ks1,ks2,ks3);
			tout0=tin[0];
			tout1=tin[1];

			tout0^=xor0;
			tout1^=xor1;
			l2cn(tout0,tout1,out,l+8);
			xor0=t0;
			xor1=t1;
			}

		iv = &(*ivec)[0];
		l2c(xor0,iv);
		l2c(xor1,iv);
		}
	tin0=tin1=tout0=tout1=xor0=xor1=0;
	tin[0]=tin[1]=0;
}

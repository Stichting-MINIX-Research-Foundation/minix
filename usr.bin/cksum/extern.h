/*	$NetBSD: extern.h,v 1.19 2006/09/04 20:01:10 dsl Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)extern.h	8.1 (Berkeley) 6/6/93
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#include "crc_extern.h"

__BEGIN_DECLS
void	pcrc(char *, u_int32_t, off_t);
void	psum1(char *, u_int32_t, off_t);
void	psum2(char *, u_int32_t, off_t);
int	csum1(int, u_int32_t *, off_t *);
int	csum2(int, u_int32_t *, off_t *);
int	md5(int, u_int32_t *, u_int32_t *);

void	MD2String(const char *);
void	MD2TimeTrial(void);
void	MD2TestSuite(void);
void	MD2Filter(int);

void	MD4String(const char *);
void	MD4TimeTrial(void);
void	MD4TestSuite(void);
void	MD4Filter(int);

void	MD5String(const char *);
void	MD5TimeTrial(void);
void	MD5TestSuite(void);
void	MD5Filter(int);

void	SHA1String(const char *);
void	SHA1TimeTrial(void);
void	SHA1TestSuite(void);
void	SHA1Filter(int);

void	RMD160String(const char *);
void	RMD160TimeTrial(void);
void	RMD160TestSuite(void);
void	RMD160Filter(int);

void	SHA256_String(const char *);
void	SHA256_TimeTrial(void);
void	SHA256_TestSuite(void);
void	SHA256_Filter(int);

void	SHA384_String(const char *);
void	SHA384_TimeTrial(void);
void	SHA384_TestSuite(void);
void	SHA384_Filter(int);

void	SHA512_String(const char *);
void	SHA512_TimeTrial(void);
void	SHA512_TestSuite(void);
void	SHA512_Filter(int);
__END_DECLS

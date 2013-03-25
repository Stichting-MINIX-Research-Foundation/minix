/*	$NetBSD: mallocvar.h,v 1.12 2012/04/29 20:27:32 dsl Exp $	*/

/*
 * Copyright (c) 1987, 1993
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
 *	@(#)malloc.h	8.5 (Berkeley) 5/3/95
 */

#ifndef _SYS_MALLOCVAR_H_
#define	_SYS_MALLOCVAR_H_

#include <sys/types.h>

/*
 * This structure describes a type of malloc'd memory and carries
 * allocation statistics for that memory.
 */
struct malloc_type;

#ifdef _KERNEL
#define	MALLOC_JUSTDEFINE_LIMIT(type, shortdesc, longdesc, limit)

#define	MALLOC_JUSTDEFINE(type, shortdesc, longdesc)			\
	MALLOC_JUSTDEFINE_LIMIT(type, shortdesc, longdesc, 0)

#define	MALLOC_DEFINE_LIMIT(type, shortdesc, longdesc, limit)		\
	MALLOC_JUSTDEFINE_LIMIT(type, shortdesc, longdesc, limit)

#define	MALLOC_DEFINE(type, shortdesc, longdesc)			\
	MALLOC_DEFINE_LIMIT(type, shortdesc, longdesc, 0)

#define	MALLOC_DECLARE(type)						\
	static struct malloc_type *const __unused type = 0

#define	malloc_type_attach(malloc_type)
#define	malloc_type_detach(malloc_type)
#endif /* _KERNEL */

#endif /* _SYS_MALLOCVAR_H_ */

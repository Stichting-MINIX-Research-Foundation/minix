/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <powerpc/asm.h>

__RCSID("$NetBSD: gprsavrest.S,v 1.4 2011/10/28 02:00:50 christos Exp $")

#ifndef RESTGPR
# define RESTGPR(n)	RESTOREXGPR(_restgpr_,n,_x)
# define RESTGPR_END(n)	RESTOREXGPR_END(_restgpr_,n,_x)
# define SAVEGPR(n)	SAVEXGPR(_savegpr_,n,)
# define SAVEGPR_END(n)	SAVEXGPR_END(_savegpr_,n,)
#endif

#define RESTOREXGPR(a,n,b) \
	.hidden a##n##b; ENTRY_NOPROFILE(a##n##b); lwz	n,(-4*(32-n))(11)
#define SAVEXGPR(a,n,b) \
	.hidden a##n##b; ENTRY_NOPROFILE(a##n##b); stw	n,(-4*(32-n))(11)
#ifdef _LP64
# define SAVEXGPR_END(a,n,b) 
# define RESTOREXGPR_END(a,n,b) 
#else
# define SAVEXGPR_END(a,n,b) \
	.size a##n##b,.-a##n##b
# define RESTOREXGPR_END(a,n,b) \
	.size a##n##b,.-a##n##b
#endif

	RESTGPR(14)
	RESTGPR(15)
	RESTGPR(16)
	RESTGPR(17)
	RESTGPR(18)
	RESTGPR(19)
	RESTGPR(20)
	RESTGPR(21)
	RESTGPR(22)
	RESTGPR(23)
	RESTGPR(24)
	RESTGPR(25)
	RESTGPR(26)
	RESTGPR(27)
	RESTGPR(28)
	RESTGPR(29)
	RESTGPR(30)
	RESTGPR(31)
	lwz	0,4(11)
	mtlr	0
	mr	1,11
	blr
	RESTGPR_END(14)
	RESTGPR_END(15)
	RESTGPR_END(16)
	RESTGPR_END(17)
	RESTGPR_END(18)
	RESTGPR_END(19)
	RESTGPR_END(20)
	RESTGPR_END(21)
	RESTGPR_END(22)
	RESTGPR_END(23)
	RESTGPR_END(24)
	RESTGPR_END(25)
	RESTGPR_END(26)
	RESTGPR_END(27)
	RESTGPR_END(28)
	RESTGPR_END(29)
	RESTGPR_END(30)
	RESTGPR_END(31)

	SAVEGPR(14)
	SAVEGPR(15)
	SAVEGPR(16)
	SAVEGPR(17)
	SAVEGPR(18)
	SAVEGPR(19)
	SAVEGPR(20)
	SAVEGPR(21)
	SAVEGPR(22)
	SAVEGPR(23)
	SAVEGPR(24)
	SAVEGPR(25)
	SAVEGPR(26)
	SAVEGPR(27)
	SAVEGPR(28)
	SAVEGPR(29)
	SAVEGPR(30)
	SAVEGPR(31)
	blr
	SAVEGPR_END(14)
	SAVEGPR_END(15)
	SAVEGPR_END(16)
	SAVEGPR_END(17)
	SAVEGPR_END(18)
	SAVEGPR_END(19)
	SAVEGPR_END(20)
	SAVEGPR_END(21)
	SAVEGPR_END(22)
	SAVEGPR_END(23)
	SAVEGPR_END(24)
	SAVEGPR_END(25)
	SAVEGPR_END(26)
	SAVEGPR_END(27)
	SAVEGPR_END(28)
	SAVEGPR_END(29)
	SAVEGPR_END(30)
	SAVEGPR_END(31)

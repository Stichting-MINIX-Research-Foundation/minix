/* $NetBSD: compident.S,v 1.3 2014/05/14 14:59:14 joerg Exp $ */

/*-
 * Copyright (c) 2002, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Martin Husemann <martin@NetBSD.org>.
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


/*
 * This file is used to generate a note describing the code model in use by
 * the compiler on some architectures where this has consequences for the
 * VA layout applied by the kernel. On sparc64 the topdown VA layout can
 * only be enabled for processes compiled with -mcmodel=medany or greater,
 * but not for processes without this note or -mcmodel=medlow.
 *
 * The note looks similar to the .note.netbsd.march note used (for example)
 * on arm to note the ABI used by the process.
 *
 * [NOTE HEADER]
 *	long		name size
 *	long		content size
 *	long		note type
 *
 * [NOTE DATUM]
 *	string		code model (for sparc64: medlow, medmid, medany,
 *			the embedany model is never PIC, so not used here)
 *
 * The DATUM fields should be padded out such that their actual (not
 * declared) sizes % 4 == 0.
 */

#include "sysident_assym.h"

	.section .note.netbsd.mcmodel,"aM",@note,3*4+CONTENTLENGTH+ELF_NOTE_MCMODEL_NAMESZ+1
	.p2align 2

	.long	ELF_NOTE_MCMODEL_NAMESZ
	.long	CONTENTLENGTH
	.long	ELF_NOTE_TYPE_MCMODEL_TAG
	.ascii	"NetBSD\0\0"
	.ascii	CONTENT
	.p2align 2

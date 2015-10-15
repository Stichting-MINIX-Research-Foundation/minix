/*	$NetBSD: membar_ops.S,v 1.7 2015/01/08 22:27:17 riastradh Exp $	*/

/*-
 * Copyright (c) 2006, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

#include "atomic_op_asm.h"

	.text

#ifdef _KERNEL

/*
 * We start out with no-op versions that do nothing.  We hot-patch when
 * we detect a MP system.
 */
LEAF(_membar_producer, 0)
	RET
	nop
	END(_membar_producer)
EXPORT(_membar_producer_end)

LEAF(_membar_sync, 0)
	RET
	nop
	END(_membar_sync)
EXPORT(_membar_sync_end)

LEAF(_membar_producer_mp, 0)
	wmb
	RET
	END(_membar_producer_mp)
EXPORT(_membar_producer_mp_end)

LEAF(_membar_sync_mp, 0)
	mb
	RET
	END(_membar_sync_mp)
EXPORT(_membar_sync_mp_end)

#else	/* _KERNEL */

LEAF(_membar_producer, 0)
	mb
	RET
	END(_membar_producer)
EXPORT(_membar_producer_end)

LEAF(_membar_sync, 0)
	mb
	RET
	END(_membar_sync)
EXPORT(_membar_sync_end)

#endif	/* _KERNEL */

ATOMIC_OP_ALIAS(membar_producer,_membar_producer)
ATOMIC_OP_ALIAS(membar_sync,_membar_sync)
ATOMIC_OP_ALIAS(membar_enter,_membar_sync)
STRONG_ALIAS(_membar_enter,_membar_sync)
ATOMIC_OP_ALIAS(membar_exit,_membar_sync)
STRONG_ALIAS(_membar_exit,_membar_sync)
ATOMIC_OP_ALIAS(membar_consumer,_membar_sync)
STRONG_ALIAS(_membar_consumer,_membar_sync)
ATOMIC_OP_ALIAS(membar_datadep_consumer,_membar_sync)
STRONG_ALIAS(_membar_datadep_consumer,_membar_sync)

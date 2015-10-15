/*	$NetBSD: atomic_nand_16_cas.c,v 1.2 2014/02/21 16:06:48 martin Exp $	*/

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
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

#include "atomic_op_namespace.h"

#include <sys/atomic.h>

uint16_t fetch_and_nand_2(volatile uint16_t *, uint16_t, ...)
    asm("__sync_fetch_and_nand_2");
uint16_t nand_and_fetch_2(volatile uint16_t *, uint16_t, ...)
    asm("__sync_nand_and_fetch_2");

uint16_t
fetch_and_nand_2(volatile uint16_t *addr, uint16_t val, ...)
{
	uint16_t old, new;

	do {
		old = *addr;
		new = ~(old & val);
	} while (atomic_cas_16(addr, old, new) != old);
	return old;
}

uint16_t
nand_and_fetch_2(volatile uint16_t *addr, uint16_t val, ...)
{
	uint16_t old, new;

	do {
		old = *addr;
		new = ~(old & val);
	} while (atomic_cas_16(addr, old, new) != old);
	return new;
}

/* $NetBSD: atppc_isadma.h,v 1.5 2007/03/04 06:02:10 christos Exp $ */

/*-
 * Copyright (c) 2003 Bruce J.A. Nourish
 * Copyright (c) 2003, 2004 Gary Thorpe <gathorpe@users.sourceforge.net>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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
 */

/* atppc ISA DMA functions */
int atppc_isadma_setup(struct atppc_softc *, isa_chipset_tag_t, int);
int atppc_isadma_start(isa_chipset_tag_t, int, void *, u_int,
	u_int8_t);
int atppc_isadma_finish(isa_chipset_tag_t, int);
int atppc_isadma_abort(isa_chipset_tag_t, int);
int atppc_isadma_malloc(isa_chipset_tag_t, int, void **, bus_addr_t *,
	bus_size_t);
void atppc_isadma_free(isa_chipset_tag_t, int, void **, bus_addr_t *,
	bus_size_t);

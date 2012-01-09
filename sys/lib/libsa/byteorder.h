/*	$NetBSD: byteorder.h,v 1.3 2001/10/31 20:22:22 thorpej Exp $	*/

/*
 * Copyright 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LIBSA_BYTEORDER_H_
#define	_LIBSA_BYTEORDER_H_

#ifdef _STANDALONE
#include <lib/libsa/stand.h>
#else
#include <inttypes.h>
#endif

uint16_t	sa_htobe16(uint16_t);
uint16_t	sa_htole16(uint16_t);
uint16_t	sa_be16toh(uint16_t);
uint16_t	sa_le16toh(uint16_t);

uint32_t	sa_htobe32(uint32_t);
uint32_t	sa_htole32(uint32_t);
uint32_t	sa_be32toh(uint32_t);
uint32_t	sa_le32toh(uint32_t);

uint64_t	sa_htobe64(uint64_t);
uint64_t	sa_htole64(uint64_t);
uint64_t	sa_be64toh(uint64_t);
uint64_t	sa_le64toh(uint64_t);

/* Order of the words in a big-endian 64-bit word. */
#define	BE64_HI	0
#define	BE64_LO	1

/* Order of the words in a little-endian 64-bit word. */
#define	LE64_HI	1
#define	LE64_LO	0

#endif /* _LIBSA_BYTEORDER_H_ */

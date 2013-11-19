/*	$NetBSD: kern_ctf.h,v 1.1 2010/03/12 21:43:10 darran Exp $	*/

/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
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

#ifndef _SYS_CTF_H_
#define	_SYS_CTF_H_

/*
 * Modules CTF section
 */
typedef struct mod_ctf {
	const uint8_t 	*ctftab;	/* Decompressed CTF data. */
	int 		ctfcnt;		/* Number of CTF data bytes. */
	const Elf_Sym	*symtab;	/* Ptr to the symbol table. */
	int		nsym;		/* Number of symbols. */
	uint32_t	*nmap;		/* symbol id map */
	int		nmapsize;	/* Span of id map */
	const char	*strtab;	/* Ptr to the string table. */
	int 		strcnt;		/* Number of string bytes. */
	uint32_t	*ctfoffp;	/* Ptr to array of obj/fnc offsets. */
	uint32_t	*typoffp;	/* Ptr to array of type offsets. */
	long		typlen;		/* number of type data entries. */
	int		ctfalloc;	/* ctftab is alloced */
} mod_ctf_t;

int
mod_ctf_get(struct module *, mod_ctf_t *);

#endif

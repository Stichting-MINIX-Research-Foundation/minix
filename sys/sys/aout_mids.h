/* $NetBSD: aout_mids.h,v 1.2 2012/12/27 06:55:49 martin Exp $ */

/*
 * Copyright (c) 2009, The NetBSD Foundation, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SYS_MACHINE_IDS_H_
#define _SYS_MACHINE_IDS_H_


/*
 * a_mid - keep sorted in numerical order for sanity's sake
 * ensure that: 0 < mid < 0x3ff
 */
#define	MID_ZERO	0	/* unknown - implementation dependent */
#define	MID_SUN010	1	/* sun 68010/68020 binary */
#define	MID_SUN020	2	/* sun 68020-only binary */
#define	MID_PC386	100	/* 386 PC binary. (so quoth BFD) */
#define	MID_HP200	200	/* hp200 (68010) BSD binary */
#define	MID_I386	134	/* i386 BSD binary */
#define	MID_M68K	135	/* m68k BSD binary with 8K page sizes */
#define	MID_M68K4K	136	/* m68k BSD binary with 4K page sizes */
#define	MID_NS32532	137	/* ns32532 */
#define	MID_SPARC	138	/* sparc */
#define	MID_PMAX	139	/* pmax */
#define	MID_VAX1K	140	/* VAX 1K page size binaries */
#define	MID_ALPHA	141	/* Alpha BSD binary */
#define	MID_MIPS	142	/* big-endian MIPS */
#define	MID_ARM6	143	/* ARM6 */
#define	MID_M680002K	144	/* m68000 with 2K page sizes */
#define	MID_SH3		145	/* SH3 */
#define	MID_POWERPC	149	/* big-endian PowerPC */
#define	MID_VAX		150	/* VAX */
				/* 151 - MIPS1 */
				/* 152 - MIPS2 */
#define	MID_M88K	153	/* m88k BSD */
#define	MID_HPPA	154	/* HP PARISC */
#define	MID_SH5_64	155	/* LP64 SH5 */
#define	MID_SPARC64	156	/* LP64 sparc */
#define	MID_X86_64	157	/* AMD x86-64 */
#define	MID_SH5_32	158	/* ILP32 SH5 */
#define	MID_IA64	159	/* Itanium */
#define	MID_HP200	200	/* hp200 (68010) BSD binary */
#define	MID_HP300	300	/* hp300 (68020+68881) BSD binary */
#define	MID_HPUX	0x20C	/* hp200/300 HP-UX binary */
#define	MID_HPUX800     0x20B   /* hp800 HP-UX binary */

#endif /* _SYS_MACHINE_IDS_H_ */

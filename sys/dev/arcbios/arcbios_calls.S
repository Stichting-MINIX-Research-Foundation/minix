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

#include <sys/cdefs.h>
#include <machine/asm.h>

#include "assym.h"

RCSID("$NetBSD: arcbios_calls.S,v 1.1 2011/02/20 08:02:46 matt Exp $")

	.text
	.set noreorder

#ifdef _LP64
#define	FIX_V0		sll	v0, v0, 0
#else
#define FIX_V0		/* nothing */
#endif

#define	CALLFRAME2_SIZ	(CALLFRAME_SIZ + 16)
#define	CALLFRAME2_RA	(CALLFRAME_RA + 16)
#define	CALLFRAME2_S0	(CALLFRAME_S0 + 16)

#ifndef _STANDALONE
NESTED(arcbios_4orless_args, CALLFRAME_SIZ, ra)
	PTR_SUBU sp, CALLFRAME_SIZ

	REG_S	ra, CALLFRAME_RA(sp)
	REG_S	s0, CALLFRAME_S0(sp)

	PTR_L	t9, _C_LABEL(ARCBIOS)
	PTR_ADDU t9, t0
	INT_L	t9, 0(t9)
	nop

	jalr	t9
	 move	s0, t8

	FIX_V0

	move	t8, s0

	REG_L	ra, CALLFRAME_RA(sp)
	REG_L	s0, CALLFRAME_S0(sp)

	jr	ra
	 PTR_ADDU sp, CALLFRAME_SIZ
END(arcbios_4orless_args)

NESTED(arcbios_5to8_args, CALLFRAME2_SIZ, ra)
	PTR_SUBU sp, CALLFRAME2_SIZ

	REG_S	ra, CALLFRAME2_RA(sp)
	REG_S	s0, CALLFRAME2_S0(sp)

#ifdef __mips_o32
	INT_L	ta0, CALLFRAME2_SIZ+16(sp)	# load 5th arg
	INT_L	ta1, CALLFRAME2_SIZ+20(sp)	# load 6th arg
	INT_L	ta2, CALLFRAME2_SIZ+24(sp)	# load 7th arg
	INT_L	ta3, CALLFRAME2_SIZ+28(sp)	# load 8th arg
	INT_S	ta0, 16(sp)			# save 5th arg on stack (o32)
	INT_S	ta1, 20(sp)			# save 6th arg on stack (o32)
	INT_S	ta2, 24(sp)			# save 7th arg on stack (o32)
	INT_S	ta3, 28(sp)			# save 8th arg on stack (o32)
#else
	INT_S	a4, 16(sp)			# save 5th arg on stack (o32)
	INT_S	a5, 20(sp)			# save 6th arg on stack (o32)
	INT_S	a6, 24(sp)			# save 7th arg on stack (o32)
	INT_S	a7, 28(sp)			# save 8th arg on stack (o32)
#endif

	PTR_L	t9, _C_LABEL(ARCBIOS)
	PTR_ADDU t9, t0
	INT_L	t9, 0(t9)
	nop

	jalr	t9
	 nop

	FIX_V0

	move	t8, s0

	REG_L	ra, CALLFRAME2_RA(sp)
	REG_L	s0, CALLFRAME2_S0(sp)

	jr	ra
	 PTR_ADDU sp, CALLFRAME2_SIZ
END(arcbios_5to8_args)
#endif /* !_STANDALONE */

#define AFVDIRECT(name)						\
	.globl	__CONCAT(arcbios_,name);			\
LEAF(__CONCAT(arcbios_,name));					\
	PTR_L	t9, _C_LABEL(ARCBIOS); 				\
	 nop;							\
	INT_L	t9, __CONCAT(AFV_,name)(t9);			\
	 nop;							\
	jr	t9;						\
	 nop;							\
END(__CONCAT(arcbios_,name))

#ifdef _STANDALONE
#define	AFV4ORLESS(name)	AFVDIRECT(name)
#define	AFV5ORMORE(name)	AFVDIRECT(name)
#else
#define	AFV4ORLESS(name)					\
	.globl	__CONCAT(arcbios_,name);			\
NESTED(__CONCAT(arcbios_,name), 0, ra);				\
	b	arcbios_4orless_args;				\
	 li	t0, __CONCAT(AFV_,name);			\
END(__CONCAT(arcbios_,name))

#define	AFV5ORMORE(name)					\
	.globl	__CONCAT(arcbios_,name);			\
NESTED(__CONCAT(arcbios_,name), 0, ra);				\
	b	arcbios_5to8_args;				\
	 li	t0, __CONCAT(AFV_,name);			\
END(__CONCAT(arcbios_,name))
#endif

/*
 * ARC firmware vector
 */
AFV4ORLESS(Load)		/* long (*Load)(char *image, u_long top, u_long entry, u_long *low); */
AFV5ORMORE(Invoke)		/* long (*Invoke)(u_long, u_long, u_long, char **, char **);	*/
AFV4ORLESS(Execute)		/* long (*Execute)(char *, u_long, char **, char **); */
AFVDIRECT(Halt)			/* void (*Halt)(void) __dead; */
AFVDIRECT(PowerDown)		/* void (*PowerDown)(void) __dead; */
AFVDIRECT(Restart)		/* void (*Restart)(void) __dead; */
AFVDIRECT(Reboot)		/* void (*Reboot)(void) __dead; */
AFVDIRECT(EnterInteractiveMode)	/* void (*EnterInteractiveMode)(void) __dead; */
#ifndef sgimips
AFVDIRECT(ReturnFromMain)	/* void (*ReturnFromMain)(void) __dead; */
#endif
AFV4ORLESS(GetPeer)		/* void *(*GetPeer)(void *); */
AFV4ORLESS(GetChild)		/* void *(*GetChild)(void *); */
AFV4ORLESS(GetParent)		/* void *(*GetParent)(void *); */
AFV4ORLESS(GetConfigurationData) /* long (*GetConfigurationData)(void *, void *); */
AFV4ORLESS(AddChild)		/* void *(*AddChild)(void *, void *); */
AFV4ORLESS(DeleteComponent)	/* long (*DeleteComponent)(void *component); */
AFV4ORLESS(GetComponent)	/* void *(*GetComponent)(char *path); */
AFV4ORLESS(SaveConfiguration)	/* long (*SaveConfiguration)(void); */
AFV4ORLESS(GetSystemId)		/* void *(*GetSystemId)(void); */
AFV4ORLESS(GetMemoryDescriptor)	/* void *(*GetMemoryDescriptor)(void *); */
#if !defined(sgimips)
AFV4ORLESS(Signal)		/* void (*Signal)(u_long, void *); */
#endif
AFV4ORLESS(GetTime)		/* void *(*GetTime)(void); */
AFV4ORLESS(GetRelativeTime)	/* u_long (*GetRelativeTime)(void); */
AFV4ORLESS(GetDirectoryEntry)	/* long	(*GetDirectoryEntry)(u_long, void *, u_long, u_long *); */
AFV4ORLESS(Open)		/* long (*Open)(char *, u_long, u_long *); */
AFV4ORLESS(Close)		/* long (*Close)(u_long); */
AFV4ORLESS(Read)		/* long (*Read)(u_long, void *, u_long, u_long *); */
AFV4ORLESS(GetReadStatus)	/* long (*GetReadStatus)(u_long); */
AFV4ORLESS(Write)		/* long (*Write)(u_long, void *, u_long, u_long *); */
AFV4ORLESS(Seek)		/* long (*Seek)(u_long, int64_t *, u_long); */
AFV4ORLESS(Mount)		/* long (*Mount)(char *, u_long); */
AFV4ORLESS(GetEnvironmentVariable) /* const char *(*GetEnvironmentVariable)(const char *); */
AFV4ORLESS(SetEnvironmentVariable) /* long (*SetEnvironmentVariable)(const char *, const char *); */
AFV4ORLESS(GetFileInformation)	/* long (*GetFileInformation)(u_long, void *); */
AFV4ORLESS(SetFileInformation)	/* long (*SetFileInformation)(u_long, u_long, u_long); */
AFV4ORLESS(FlushAllCaches)	/* void (*FlushAllCaches)(void); */
#ifndef sgimips
AFV4ORLESS(TestUnicode)		/* paddr_t (*TestUnicode)(u_long, uint16_t); */
AFV4ORLESS(GetDisplayStatus)	/* void *(*GetDisplayStatus)(u_long); */
#endif

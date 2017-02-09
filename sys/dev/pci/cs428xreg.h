/*	$NetBSD: cs428xreg.h,v 1.4 2005/12/11 12:22:49 christos Exp $	*/

/*
 * Copyright (c) 2000 Tatoku Ogaito.  All rights reserved.
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
 *      This product includes software developed by Tatoku Ogaito
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
#ifndef _CS428X_REG_H_
#define _CS428X_REG_H_
#define CS428X_ACCTL   0x460	/* AC97 Control Register */
#define	 ACCTL_RSTN	   0x00000001 /* Only for CS4280 */
#define	 ACCTL_ESYN	   0x00000002
#define	 ACCTL_VFRM	   0x00000004
#define	 ACCTL_DCV	   0x00000008
#define	 ACCTL_CRW	   0x00000010
#define	 ACCTL_ASYN	   0x00000020 /* Only for CS4280 */
#define	 ACCTL_TC	   0x00000040

#define CS428X_ACSTS   0x464	/* AC97 Status Register */
#define	 ACSTS_CRDY	   0x00000001
#define	 ACSTS_VSTS	   0x00000002

#define CS428X_ACOSV   0x468	/* AC97 Output Slot Valid Register */
#define	 ACOSV_SLV3	   0x00000001
#define	 ACOSV_SLV4	   0x00000002
#define	 ACOSV_SLV5	   0x00000004
#define	 ACOSV_SLV6	   0x00000008
#define	 ACOSV_SLV7	   0x00000010
#define	 ACOSV_SLV8	   0x00000020
#define	 ACOSV_SLV9	   0x00000040
#define	 ACOSV_SLV10	   0x00000080
#define	 ACOSV_SLV11	   0x00000100
#define	 ACOSV_SLV12	   0x00000200

#define CS428X_ACCAD   0x46c	/* AC97 Command Address Register */
#define CS428X_ACCDA   0x470	/* AC97 Command Data Register */

#define CS428X_ACISV   0x474	/* AC97 Input Slot Valid Register */
#define	 ACISV_ISV3	   0x00000001
#define	 ACISV_ISV4	   0x00000002
#define	 ACISV_ISV5	   0x00000004
#define	 ACISV_ISV6	   0x00000008
#define	 ACISV_ISV7	   0x00000010
#define	 ACISV_ISV8	   0x00000020
#define	 ACISV_ISV9	   0x00000040
#define	 ACISV_ISV10	   0x00000080
#define	 ACISV_ISV11	   0x00000100
#define	 ACISV_ISV12	   0x00000200
#define CS428X_ACSAD   0x478	/* AC97 Status Address Register */
#define CS428X_ACSDA   0x47c	/* AC97 Status Data Register */

#endif /* _CS428X_REG_H_ */

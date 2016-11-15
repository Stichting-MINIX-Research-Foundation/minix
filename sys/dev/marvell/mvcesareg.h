/*	$NetBSD: mvcesareg.h,v 1.2 2013/09/28 05:46:51 kiyohara Exp $	*/
/*
 * Copyright (c) 2008 KIYOHARA Takashi
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _MVCESAREG_H_
#define _MVCESAREG_H_

#define MVCESA_SIZE	0x1000


/*
 * Cryptographic Engine and Security Accelerator Registers
 */
/* DES Engine Registers */
#define MVCESA_DESE_DOL		0xd78	/* Data Out Low */
#define MVCESA_DESE_DOH		0xd7c	/* Data Out High */
#define MVCESA_DESE_DBL		0xd70	/* Data Buffer Low */
#define MVCESA_DESE_DBH		0xd74	/* Data Buffer High */
#define MVCESA_DESE_IVL		0xd40	/* Initial Value Low */
#define MVCESA_DESE_IVH		0xd44	/* Initial Value High */
#define MVCESA_DESE_K0L		0xd48	/* Key0 Low */
#define MVCESA_DESE_K0H		0xd4c	/* Key0 High */
#define MVCESA_DESE_K1L		0xd50	/* Key1 Low */
#define MVCESA_DESE_K1H		0xd54	/* Key1 High */
#define MVCESA_DESE_K2L		0xd60	/* Key2 Low */
#define MVCESA_DESE_K2H		0xd64	/* Key2 High */
#define MVCESA_DESE_C		0xd58	/* Command */
#define MVCESA_DESE_C_DIRECTION_ENC	(0 << 0)
#define MVCESA_DESE_C_DIRECTION_DEC	(1 << 0)
#define MVCESA_DESE_C_ALGORITHM_DES	(0 << 1)
#define MVCESA_DESE_C_ALGORITHM_3DES	(1 << 1)
#define MVCESA_DESE_C_3DESMODE_EEE	(0 << 2)
#define MVCESA_DESE_C_3DESMODE_EDE	(1 << 2)
#define MVCESA_DESE_C_DESMODE_ECB	(0 << 3)
#define MVCESA_DESE_C_DESMODE_CBC	(1 << 3)
#define MVCESA_DESE_C_DATABYTESWAP	(1 << 4)
#define MVCESA_DESE_C_IVBYTESWAP	(1 << 6)
#define MVCESA_DESE_C_OUTBYTESWAP	(1 << 8)
#define MVCESA_DESE_C_READALLOW		(1 << 29)
#define MVCESA_DESE_C_ALLTERMINATION	(1 << 30)
#define MVCESA_DESE_C_TERMINATION	(1 << 31)

/* SHA-1 and MD5 Interface Registers */
#define MVCESA_SHA1MD5I_DI	0xd38	/* Data In */
#define MVCESA_SHA1MD5I_BCL	0xd20	/* Bit Count Low */
#define MVCESA_SHA1MD5I_BCH	0xd24	/* Bit Count High */
#define MVCESA_SHA1MD5I_IVDA	0xd00	/* Initial Value/Digest A */
#define MVCESA_SHA1MD5I_IVDB	0xd04	/* Initial Value/Digest B */
#define MVCESA_SHA1MD5I_IVDC	0xd08	/* Initial Value/Digest C */
#define MVCESA_SHA1MD5I_IVDD	0xd0c	/* Initial Value/Digest D */
#define MVCESA_SHA1MD5I_IVDE	0xd10	/* Initial Value/Digest E */
#define MVCESA_SHA1MD5I_AC	0xd18	/* Authentication Command */
#define MVCESA_SHA1MD5I_AC_ALGORITHM_MD5	(0 << 0)
#define MVCESA_SHA1MD5I_AC_ALGORITHM_SHA1	(1 << 0)
#define MVCESA_SHA1MD5I_AC_MODE_USEIV		(0 << 1)
#define MVCESA_SHA1MD5I_AC_MODE_CONTINUE	(1 << 1)
#define MVCESA_SHA1MD5I_AC_DATABYTESWAP		(1 << 2)
#define MVCESA_SHA1MD5I_AC_IVBYTESWAP		(1 << 4)
#define MVCESA_SHA1MD5I_AC_TERMINATION		(1 << 31)

/* AES Encryption/Decription Interface Registers */
#define MVCESA_AES_ENCRYPTION	0xd80
#define MVCESA_AES_DECRYPTION	0xdc0
#define MVCESA_AES_DIOC_OFF	  0x20	/* Data In/Out Column */
#define MVCESA_AES_DIOC_MAX	3
#define MVCESA_AES_KC_OFF	  0x00	/* Key Column */
#define MVCESA_AES_KC_MAX	7
#define MVCESA_AES_C		  0x30	/* Command */
#define MVCESA_AES_C_AESKEYMODE_128		(0 << 0)
#define MVCESA_AES_C_AESKEYMODE_192		(1 << 0)
#define MVCESA_AES_C_AESKEYMODE_256		(2 << 0)
#define MVCESA_AES_C_AESDECMAKEKEY		(1 << 2)
#define MVCESA_AES_C_DATABYTESWAP		(1 << 4)
#define MVCESA_AES_C_OUTBYTESWAP		(1 << 8)
#define MVCESA_AES_C_TERMINATION		(1 << 31)

#define MVCESA_AES_DIOC(c) \
	(MVCESA_AES_DIOC_OFF + ((c) - MVCESA_AES_DIOC_MAX) * 4)
#define MVCESA_AES_KC(c) \
	(MVCESA_AES_KC_OFF + ((c) - MVCESA_AES_KC_MAX) * 4)


/* Security Accelerator Registers */
#define MVCESA_SA_C		0xe00	/* Command */
#define MVCESA_SA_DPS0		0xe04	/* Descriptor Pointer Session 0 */
#define MVCESA_SA_DPS1		0xe14	/* Descriptor Pointer Session 1 */
#define MVCESA_SA_CFG		0xe08	/* Configuration */
#define MVCESA_SA_S		0xe0c	/* Status */

/* Interrupt Cause Registers */
#define MVCESA_IC		0xe20	/* Interrupt Cause */
#define MVCESA_IM		0xe24	/* Interrupt Mask */
#define MVCESA_I_ZINT0			(1 << 0) /* auth termination */
#define MVCESA_I_ZINT1			(1 << 1) /* DES */
#define MVCESA_I_ZINT2			(1 << 2) /* AES encryption */
#define MVCESA_I_ZINT3			(1 << 3) /* AES decryption */
#define MVCESA_I_ZINT4			(1 << 4) /* enc termination */
#define MVCESA_I_ACCINT0		(1 << 5) /* Security accelerator 0 */
#define MVCESA_I_ACCINT1		(1 << 6) /* Security accelerator 1 */
#define MVCESA_I_ACCANDIDMAINT0		(1 << 7) /* Acceleration and IDMA 0 */
#define MVCESA_I_ACCANDIDMAINT1		(1 << 8) /* Acceleration and IDMA 1 */

#endif	/* _MVCESAREG_H_ */

/*	$NetBSD: cardbusreg.h,v 1.7 2011/08/01 11:20:27 drochner Exp $ */

/*
 * Copyright (c) 2001
 *       HAYAKAWA Koichi.  All rights reserved.
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

#ifndef _DEV_CARDBUS_CARDBUSREG_H_
#define _DEV_CARDBUS_CARDBUSREG_H_

#include <dev/pci/pcivar.h>	/* for pcitag_t */

/* Base Resisters */
#define CARDBUS_CIS_REG    0x28
#define CARDBUS_ROM_REG	   0x30
#  define CARDBUS_CIS_ASIMASK 0x07
#    define CARDBUS_CIS_ASI(x) (CARDBUS_CIS_ASIMASK & (x))
#  define CARDBUS_CIS_ASI_TUPLE 0x00
#  define CARDBUS_CIS_ASI_BAR0  0x01
#  define CARDBUS_CIS_ASI_BAR1  0x02
#  define CARDBUS_CIS_ASI_BAR2  0x03
#  define CARDBUS_CIS_ASI_BAR3  0x04
#  define CARDBUS_CIS_ASI_BAR4  0x05
#  define CARDBUS_CIS_ASI_BAR5  0x06
#  define CARDBUS_CIS_ASI_ROM   0x07
#  define CARDBUS_CIS_ADDRMASK 0x0ffffff8
#    define CARDBUS_CIS_ADDR(x) (CARDBUS_CIS_ADDRMASK & (x))
#    define CARDBUS_CIS_ASI_BAR(x) (((CARDBUS_CIS_ASIMASK & (x))-1)*4+PCI_BAR0)
#    define CARDBUS_CIS_ASI_ROM_IMAGE(x) (((x) >> 28) & 0xf)

#endif /* !_DEV_CARDBUS_CARDBUSREG_H_ */

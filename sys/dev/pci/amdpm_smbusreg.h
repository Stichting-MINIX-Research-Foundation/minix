/*	$NetBSD: amdpm_smbusreg.h,v 1.2 2009/02/03 16:27:13 pgoyette Exp $ */

/*
 * Copyright (c) 2005 Anil Gopinath (anil_public@yahoo.com)
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* driver for SMBUS 1.0 host controller found in the
 * AMD-8111 HyperTransport I/O Hub
 */

#ifndef _DEV_PCI_AMDPMSMBUSREG_H_
#define _DEV_PCI_AMDPMSMBUSREG_H_

#define AMDPM_8111_SMBUS_STAT        0xE0  /* SMBus 1.x global status register */
#define AMDPM_8111_SMBUS_CTRL        0xE2  /* SMBus 1.x global control register */
#define AMDPM_8111_SMBUS_HOSTADDR    0xE4  /* SMBus 1.x Host address register */
#define AMDPM_8111_SMBUS_HOSTDATA    0xE6  /* SMBus 1.x Host data register */
#define AMDPM_8111_SMBUS_HOSTCMD     0xE8  /* SMBus 1.x Host command field register */

#define AMDPM_8111_SMBUS_GSR_QUICK   0x0008 /* GSR contents for quick op */
#define AMDPM_8111_SMBUS_GSR_SB      0x0009 /* GSR contents to send a byte */
#define AMDPM_8111_SMBUS_GSR_RXB     0x0009 /* GSR contents to receive a byte */
#define AMDPM_8111_SMBUS_GSR_RB      0x000A /* GSR contents to read a byte */
#define AMDPM_8111_SMBUS_GSR_WB      0x000A /* GSR contents to write a byte */

#define AMDPM_8111_GSR_CYCLE_DONE    0x0010 /* indicates cycle done successfuly */

#define AMDPM_8111_SMBUS_READ        0x0001 /* smbus read cycle indicator */
#define AMDPM_8111_SMBUS_RX          0x0001 /* smbus receive cycle indicator */
#define AMDPM_8111_SMBUS_WRITE       0x0000 /* smbus write cycle indicator */
#define AMDPM_8111_SMBUS_SEND        0x0000 /* smbus send cycle indicator */

void      amdpm_smbus_attach(struct amdpm_softc *sc);

#endif

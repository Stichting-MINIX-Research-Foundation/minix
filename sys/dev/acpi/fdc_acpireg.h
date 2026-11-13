/* $NetBSD: fdc_acpireg.h,v 1.2 2010/03/05 08:30:48 jruoho Exp $ */

/*
 * Copyright (c) 2003 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
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

#ifndef _SYS_DEV_ACPI_FDC_ACPIREG_H
#define _SYS_DEV_ACPI_FDC_ACPIREG_H

/*
 * The ACPI floppy disk interface is similar to the i386 BIOS. Definitions
 * from arch/i386/isa/nvram.h
 */

#define	ACPI_FDC_DISKETTE_NONE		0x00
#define	ACPI_FDC_DISKETTE_360K		0x10
#define	ACPI_FDC_DISKETTE_12M		0x20
#define	ACPI_FDC_DISKETTE_720K		0x30
#define	ACPI_FDC_DISKETTE_144M		0x40
#define	ACPI_FDC_DISKETTE_TYPE5		0x50
#define	ACPI_FDC_DISKETTE_TYPE6		0x60

const struct fd_type fdc_acpi_fdtypes[] = {
	{ 18, 2, 36, 2, 0xff, 0xcf, 0x1b, 0x6c, 80, 2880, 1, FDC_500KBPS,
	  0xf6, 1, "1.44MB" },		/* 1.44MB diskette */
	{ 15, 2, 30, 2, 0xff, 0xdf, 0x1b, 0x54, 80, 2400, 1, FDC_500KBPS,
	  0xf6, 1, "1.2MB" },		/* 1.2MB AT diskette */
	{  9, 2, 18, 2, 0xff, 0xdf, 0x23, 0x50, 40,  720, 2, FDC_300KBPS,
	  0xf6, 1, "360KB/AT" },	/* 360KB in 1.2MB drive */
	{  9, 2, 18, 2, 0xff, 0xdf, 0x2a, 0x50, 40,  720, 1, FDC_250KBPS,
	  0xf6, 1, "360KB/PC" },	/* 360KB PC diskette */
	{  9, 2, 18, 2, 0xff, 0xdf, 0x2a, 0x50, 80, 1440, 1, FDC_250KBPS,
	  0xf6, 1, "720KB" },		/* 3.5" 720KB diskette */
	{  9, 2, 18, 2, 0xff, 0xdf, 0x23, 0x50, 80, 1440, 1, FDC_300KBPS,
	  0xf6, 1, "720KB/x" },		/* 720KB in 1.2MB drive */
	{  9, 2, 18, 2, 0xff, 0xdf, 0x2a, 0x50, 40,  720, 2, FDC_250KBPS,
	  0xf6, 1, "360KB/x" },		/* 320KB in 720KB drive */
};

#endif	/* !_SYS_DEV_ACPI_FDC_ACPIREG_H */

/*	$NetBSD: acpireg.h,v 1.12 2011/08/01 11:25:59 jmcneill Exp $	*/

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

#ifndef _SYS_DEV_ACPI_ACPIREG_H
#define _SYS_DEV_ACPI_ACPIREG_H

/*
 * A common device status mask.
 */
#define ACPI_STA_OK		(ACPI_STA_DEVICE_PRESENT	|	\
				 ACPI_STA_DEVICE_ENABLED	|	\
				 ACPI_STA_DEVICE_FUNCTIONING)

/*
 * Operating System Capabilities, _OSC.
 */
#define ACPI_OSC_QUERY		__BIT(0)
#define ACPI_OSC_ERROR		__BIT(1)
#define ACPI_OSC_ERROR_UUID	__BIT(2)
#define ACPI_OSC_ERROR_REV	__BIT(3)
#define ACPI_OSC_ERROR_MASKED	__BIT(4)

/*
 * PCI functions.
 */
#define	ACPI_ADR_PCI_DEV(x)	(((x) >> 16) & 0xffff)
#define	ACPI_ADR_PCI_FUNC(x)	((x) & 0xffff)
#define	ACPI_ADR_PCI_ALLFUNCS	0xffff

#define	ACPI_GAS_PCI_DEV(x)	(((x) >> 32) & 0xffff)
#define	ACPI_GAS_PCI_FUNC(x)	(((x) >> 16) & 0xffff)
#define	ACPI_GAS_PCI_REGOFF(x)	((x) & 0xffff)

/*
 * ACPI driver components.
 */
#define	ACPI_BUS_COMPONENT	0x00010000
#define	ACPI_ACAD_COMPONENT	0x00020000
#define	ACPI_BAT_COMPONENT	0x00040000
#define	ACPI_BUTTON_COMPONENT	0x00080000
#define	ACPI_EC_COMPONENT	0x00100000
#define	ACPI_LID_COMPONENT	0x00200000
#define	ACPI_RESOURCE_COMPONENT	0x00400000
#define	ACPI_TZ_COMPONENT	0x00800000
#define	ACPI_DISPLAY_COMPONENT	0x01000000

#endif	/* !_SYS_DEV_ACPI_ACPIREG_H */

/*	$NetBSD: acpi_machdep.h,v 1.11 2012/09/23 00:31:05 chs Exp $	*/

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

#ifndef _X86_ACPI_MACHDEP_H_
#define _X86_ACPI_MACHDEP_H_

/*
 * Machine-dependent code for ACPI.
 */
#include <machine/pio.h>
#include <machine/i82489var.h>
#include <machine/i82489reg.h>

ACPI_STATUS		acpi_md_OsInitialize(void);
ACPI_PHYSICAL_ADDRESS	acpi_md_OsGetRootPointer(void);

#define	acpi_md_OsIn8(x)	inb((x))
#define	acpi_md_OsIn16(x)	inw((x))
#define	acpi_md_OsIn32(x)	inl((x))

#define	acpi_md_OsOut8(x, v)	outb((x), (v))
#define	acpi_md_OsOut16(x, v)	outw((x), (v))
#define	acpi_md_OsOut32(x, v)	outl((x), (v))

ACPI_STATUS	acpi_md_OsInstallInterruptHandler(UINT32,
		    ACPI_OSD_HANDLER, void *, void **);
void		acpi_md_OsRemoveInterruptHandler(void *);

ACPI_STATUS	acpi_md_OsMapMemory(ACPI_PHYSICAL_ADDRESS, UINT32, void **);
void		acpi_md_OsUnmapMemory(void *, UINT32);
ACPI_STATUS	acpi_md_OsGetPhysicalAddress(void *LogicalAddress,
		    ACPI_PHYSICAL_ADDRESS *PhysicalAddress);

BOOLEAN		acpi_md_OsReadable(void *, UINT32);
BOOLEAN		acpi_md_OsWritable(void *, UINT32);
void		acpi_md_OsDisableInterrupt(void);
void		acpi_md_OsEnableInterrupt(void);

int		acpi_md_sleep(int);
void		acpi_md_sleep_init(void);

uint32_t	acpi_md_pdc(void);
uint32_t	acpi_md_ncpus(void);
struct acpi_softc;
void		acpi_md_callback(struct acpi_softc *);

#endif /* !_X86_ACPI_MACHDEP_H_ */

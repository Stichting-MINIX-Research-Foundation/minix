/*	$NetBSD: OsdMemory.c,v 1.5 2012/04/22 06:33:04 jruoho Exp $	*/

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

/*
 * OS Services Layer
 *
 * 6.2: Memory management.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: OsdMemory.c,v 1.5 2012/04/22 06:33:04 jruoho Exp $");

#include <sys/param.h>
#include <sys/malloc.h>

#include <dev/acpi/acpica.h>

#include <machine/acpi_machdep.h>

MALLOC_DECLARE(M_ACPI);

/*
 * AcpiOsMapMemory:
 *
 *	Map physical memory into the caller's address space.
 */
void *
AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS PhysicalAddress, ACPI_SIZE Length)
{
	void *LogicalAddress = NULL;
	ACPI_STATUS Status;

	if (PhysicalAddress > ULONG_MAX)
		return NULL;

	Status = acpi_md_OsMapMemory(PhysicalAddress, Length, &LogicalAddress);

	if (ACPI_FAILURE(Status))
		return NULL;

	return LogicalAddress;
}

/*
 * AcpiOsUnmapMemory:
 *
 *	Remove a physical to logical memory mapping.
 */
void
AcpiOsUnmapMemory(void *LogicalAddress, ACPI_SIZE Length)
{

	acpi_md_OsUnmapMemory(LogicalAddress, Length);
}

/*
 * AcpiOsGetPhysicalAddress:
 *
 *	Translate a logical address to a physical address.
 */
ACPI_STATUS
AcpiOsGetPhysicalAddress(void *LogicalAddress,
    ACPI_PHYSICAL_ADDRESS *PhysicalAddress)
{

	return acpi_md_OsGetPhysicalAddress(LogicalAddress, PhysicalAddress);
}

/*
 * AcpiOsAllocate:
 *
 *	Allocate memory from the dynamic memory pool.
 */
void *
AcpiOsAllocate(ACPI_SIZE Size)
{

	return malloc(Size, M_ACPI, M_NOWAIT);
}

/*
 * AcpiOsFree:
 *
 *	Free previously allocated memory.
 */
void
AcpiOsFree(void *Memory)
{

	free(Memory, M_ACPI);
}


/*
 * AcpiOsReadable:
 *
 *	Check if a memory region is readable.
 */
BOOLEAN
AcpiOsReadable(void *Pointer, ACPI_SIZE Length)
{

	return acpi_md_OsReadable(Pointer, Length);
}

/*
 * AcpiOsWritable:
 *
 *	Check if a memory region is writable (and readable).
 */
BOOLEAN
AcpiOsWritable(void *Pointer, ACPI_SIZE Length)
{

	return acpi_md_OsWritable(Pointer, Length);
}

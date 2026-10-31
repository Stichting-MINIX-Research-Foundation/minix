/*	$NetBSD$	*/

/*-
 * Copyright (c) 2016 Kimihiro Nonaka <nonaka@netbsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "../efiboot.h"

#include "efibootx86.h"

static const char *efi_memory_type[] = {
	[EfiReservedMemoryType]		= "Reserved Memory Type",
	[EfiLoaderCode]			= "Loader Code",
	[EfiLoaderData]			= "Loader Data",
	[EfiBootServicesCode]		= "Boot Services Code",
	[EfiBootServicesData]		= "Boot Services Data",
	[EfiRuntimeServicesCode]	= "Runtime Services Code",
	[EfiRuntimeServicesData]	= "Runtime Services Data",
	[EfiConventionalMemory]		= "Conventional Memory",
	[EfiUnusableMemory]		= "Unusable Memory",
	[EfiACPIReclaimMemory]		= "ACPI Reclaim Memory",
	[EfiACPIMemoryNVS]		= "ACPI Memory NVS",
	[EfiMemoryMappedIO]		= "MMIO",
	[EfiMemoryMappedIOPortSpace]	= "MMIO (Port Space)",
	[EfiPalCode]			= "Pal Code",
	[EfiPersistentMemory]		= "Persistent Memory",
};

static int
getmemtype(EFI_MEMORY_DESCRIPTOR *md)
{

	switch (md->Type) {
	case EfiLoaderCode:
	case EfiLoaderData:
	case EfiBootServicesCode:
	case EfiBootServicesData:
	case EfiConventionalMemory:
		return (md->Attribute & EFI_MEMORY_WB) ?
		    BIM_Memory : BIM_Reserved;

	case EfiACPIReclaimMemory:
		return BIM_ACPI;

	case EfiACPIMemoryNVS:
		return BIM_NVS;

	case EfiPersistentMemory:
		return BIM_PMEM;

	case EfiReservedMemoryType:
	case EfiRuntimeServicesCode:
	case EfiRuntimeServicesData:
	case EfiUnusableMemory:
	case EfiMemoryMappedIO:
	case EfiMemoryMappedIOPortSpace:
	case EfiPalCode:
	case EfiMaxMemoryType:
	default:
		return BIM_Reserved;
	}
}

EFI_MEMORY_DESCRIPTOR *
efi_memory_get_map(UINTN *NoEntries, UINTN *MapKey, UINTN *DescriptorSize,
    UINT32 *DescriptorVersion, bool sorted)
{
	EFI_MEMORY_DESCRIPTOR *desc, *md, *next, *target, *tmp;
	UINTN i, j;

	*NoEntries = 0;
	desc = LibMemoryMap(NoEntries, MapKey, DescriptorSize,
	    DescriptorVersion);
	if (desc == NULL)
		panic("efi_memory_get_map failed");

	if (!sorted)
		return desc;

	tmp = alloc(*DescriptorSize);
	if (tmp == NULL)
		return desc;

	for (i = 0, md = desc; i < *NoEntries - 1; i++, md = next) {
		target = next = NextMemoryDescriptor(md, *DescriptorSize);
		for (j = i + 1; j < *NoEntries; j++) {
			if (md->PhysicalStart > target->PhysicalStart) {
				CopyMem(tmp, md, *DescriptorSize);
				CopyMem(md, target, *DescriptorSize);
				CopyMem(target, tmp, *DescriptorSize);
			}
			target = NextMemoryDescriptor(target, *DescriptorSize);
		}
	}
	dealloc(tmp, *DescriptorSize);

	return desc;
}

EFI_MEMORY_DESCRIPTOR *
efi_memory_compact_map(EFI_MEMORY_DESCRIPTOR *desc, UINTN *NoEntries,
    UINTN DescriptorSize)
{
	EFI_MEMORY_DESCRIPTOR *md, *next, *target, *tmp;
	UINTN i, j;
	UINT32 type;
	bool first = true, do_compact;

	for (i = 0, md = target = desc; i < *NoEntries; i++, md = next) {
		type = md->Type;
		switch (type) {
		case EfiLoaderCode:
		case EfiLoaderData:
		case EfiBootServicesCode:
		case EfiBootServicesData:
		case EfiConventionalMemory:
			if ((md->Attribute & EFI_MEMORY_WB) != 0)
				type = EfiConventionalMemory;
			if (md->Attribute == target->Attribute) {
				do_compact = true;
				break;
			}
			/* FALLTHROUGH */
		case EfiACPIReclaimMemory:
		case EfiACPIMemoryNVS:
		case EfiPersistentMemory:
		case EfiReservedMemoryType:
		case EfiRuntimeServicesCode:
		case EfiRuntimeServicesData:
		case EfiUnusableMemory:
		case EfiMemoryMappedIO:
		case EfiMemoryMappedIOPortSpace:
		case EfiPalCode:
		default:
			do_compact = false;
			break;
		}

		if (first) {
			first = false;
		} else if (do_compact &&
		    type == target->Type &&
		    md->Attribute == target->Attribute &&
		    md->PhysicalStart == target->PhysicalStart + target->NumberOfPages * EFI_PAGE_SIZE) {
			/* continuous region */
			target->NumberOfPages += md->NumberOfPages;

			tmp = md;
			for (j = i + 1; j < *NoEntries; j++) {
				next = NextMemoryDescriptor(md, DescriptorSize);
				CopyMem(md, next, DescriptorSize);
				md = next;
			}
			next = tmp;

			i--;
			(*NoEntries)--;
			continue;
		} else {
			target = md;
		}

		target->Type = type;
		next = NextMemoryDescriptor(md, DescriptorSize);
	}

	return desc;
}

/*
 * get memory size below 1MB
 */
int
getbasemem(void)
{
	EFI_MEMORY_DESCRIPTOR *mdtop, *md, *next;
	UINTN i, NoEntries, MapKey, DescriptorSize, MappingSize;
	UINT32 DescriptorVersion;
	EFI_PHYSICAL_ADDRESS basemem = 0, epa;

	mdtop = efi_memory_get_map(&NoEntries, &MapKey, &DescriptorSize,
	    &DescriptorVersion, true);

	for (i = 0, md = mdtop; i < NoEntries; i++, md = next) {
		next = NextMemoryDescriptor(md, DescriptorSize);
		if (getmemtype(md) != BIM_Memory)
			continue;
		if (md->PhysicalStart >= 1 * 1024 * 1024)
			continue;
		if (basemem != md->PhysicalStart)
			continue;

		MappingSize = md->NumberOfPages * EFI_PAGE_SIZE;
		epa = md->PhysicalStart + MappingSize;
		if (epa == 0 || epa > 1 * 1024 * 1024)
			epa = 1 * 1024 * 1024;
		basemem = epa;
	}

	FreePool(mdtop);

	return basemem / 1024;	/* KiB */
}

/*
 * get memory size above 1MB below 4GB
 */
int
getextmemx(void)
{
	EFI_MEMORY_DESCRIPTOR *mdtop, *md, *next;
	UINTN i, NoEntries, MapKey, DescriptorSize, MappingSize;
	UINT32 DescriptorVersion;
	EFI_PHYSICAL_ADDRESS extmem16m = 0;	/* 0-16MB */
	EFI_PHYSICAL_ADDRESS extmem4g = 0;	/* 16MB-4GB */
	EFI_PHYSICAL_ADDRESS pa, epa;
	bool first16m = true, first4g = true;
	int extmem;

	mdtop = efi_memory_get_map(&NoEntries, &MapKey, &DescriptorSize,
	    &DescriptorVersion, true);

	for (i = 0, md = mdtop; i < NoEntries; i++, md = next) {
		next = NextMemoryDescriptor(md, DescriptorSize);
		if (getmemtype(md) == BIM_Reserved)
			continue;
		if (md->PhysicalStart >= 4 * 1024 * 1024 * 1024ULL)
			continue;

		MappingSize = md->NumberOfPages * EFI_PAGE_SIZE;
		epa = md->PhysicalStart + MappingSize;
		if (epa == 0 || epa > 4 * 1024 * 1024 * 1024LL)
			epa = 4 * 1024 * 1024 * 1024LL;

		if (epa <= 1 * 1024 * 1024)
			continue;

		pa = md->PhysicalStart;
		if (pa < 16 * 1024 * 1024) {
			if (first16m || extmem16m == pa) {
				first16m = false;
				if (epa >= 16 * 1024 * 1024) {
					extmem16m = 16 * 1024 * 1024;
					pa = 16 * 1024 * 1024;
				} else
					extmem16m = epa;
			}
		}
		if (pa >= 16 * 1024 * 1024) {
			if (first4g || extmem4g == pa) {
				first4g = false;
				extmem4g = epa;
			}
		}
	}

	FreePool(mdtop);

	if (extmem16m > 1 * 1024 * 1024)
		extmem16m -= 1 * 1024 * 1024;	/* below 1MB */

	extmem = extmem16m / 1024;
	if (extmem == 15 * 1024)
		extmem += extmem4g / 1024;
	return extmem;
}

void
efi_memory_show_map(bool sorted, bool compact)
{
	EFI_STATUS status;
	EFI_MEMORY_DESCRIPTOR *mdtop, *md, *next;
	UINTN i, NoEntries, MapKey, DescriptorSize;
	UINT32 DescriptorVersion;
	char efimemstr[32];
	UINTN cols, rows, row;

	status = uefi_call_wrapper(ST->ConOut->QueryMode, 4, ST->ConOut,
	    ST->ConOut->Mode->Mode, &cols, &rows);
	if (EFI_ERROR(status) || rows <= 2)
		rows = 0;
	else
		rows -= 2;

	mdtop = efi_memory_get_map(&NoEntries, &MapKey, &DescriptorSize,
	    &DescriptorVersion, sorted);
	if (compact)
		efi_memory_compact_map(mdtop, &NoEntries, DescriptorSize);

	printf("%-22s  %-16s  %-16s  %-16s\n", "Type", "Start", "End", "Attributes");
	printf("----------------------  ----------------  ----------------  ----------------\n");
	row = 2;

	for (i = 0, md = mdtop; i < NoEntries; i++, md = next) {
		next = NextMemoryDescriptor(md, DescriptorSize);

		if (md->Type >= __arraycount(efi_memory_type))
			snprintf(efimemstr, sizeof(efimemstr), "unknown (%d)",
			    md->Type);
		printf("%-22s  %016" PRIxMAX "  %016" PRIxMAX "  %016" PRIxMAX "\n",
		    md->Type >= __arraycount(efi_memory_type) ?
		      efimemstr : efi_memory_type[md->Type],
		    (uintmax_t)md->PhysicalStart,
		    (uintmax_t)md->PhysicalStart +
		      md->NumberOfPages * EFI_PAGE_SIZE - 1,
		    (uintmax_t)md->Attribute);

		if (++row >= rows) {
			row = 0;
			printf("Press Any Key to continue :");
			(void) awaitkey(-1, 0);
			printf("\n");
		}
	}

	FreePool(mdtop);
}

physaddr_t
vtophys(void *va)
{
	return (physaddr_t)va;
}

/* $NetBSD: efifdt.c,v 1.38 2024/12/07 19:29:04 jmcneill Exp $ */

/*-
 * Copyright (c) 2019 Jason R. Thorpe
 * Copyright (c) 2018 Jared McNeill <jmcneill@invisible.ca>
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

#include "efiboot.h"
#include "efifdt.h"
#include "efiblock.h"
#include "overlay.h"
#include "module.h"

#ifdef EFIBOOT_ACPI
#include "efiacpi.h"
#endif

#include <libfdt.h>

#define FDT_TABLE_GUID	\
	{ 0xb1b621d5, 0xf19c, 0x41a5, { 0x83, 0x0b, 0xd9, 0x15, 0x2c, 0x69, 0xaa, 0xe0 } }
static EFI_GUID FdtTableGuid = FDT_TABLE_GUID;

#define	FDT_MEMORY_NODE_PATH	"/memory"
#define	FDT_MEMORY_NODE_NAME	"memory"
#define	FDT_CHOSEN_NODE_PATH	"/chosen"
#define	FDT_CHOSEN_NODE_NAME	"chosen"

#define	FDT_MEMORY_USABLE(_md)	\
	((_md)->Type == EfiLoaderCode || (_md)->Type == EfiLoaderData || \
	 (_md)->Type == EfiBootServicesCode || (_md)->Type == EfiBootServicesData || \
	 (_md)->Type == EfiConventionalMemory)

#define	FDT_SPACE	(4 * 1024 * 1024)
#define	FDT_ALIGN	(2 * 1024 * 1024)

#ifdef _LP64
#define PRIdUINTN "ld"
#define PRIxUINTN "lx"
#else
#define PRIdUINTN "d"
#define PRIxUINTN "x"
#endif
static void *fdt_data = NULL;
static size_t fdt_data_size = 512*1024;

static EFI_PHYSICAL_ADDRESS initrd_addr, dtb_addr, rndseed_addr;
static u_long initrd_size = 0, dtb_size = 0, rndseed_size = 0;

/* exec.c */
extern EFI_PHYSICAL_ADDRESS efirng_addr;
extern u_long efirng_size;

#ifdef EFIBOOT_ACPI
#define ACPI_FDT_SIZE	(128 * 1024)
static int efi_fdt_create_acpifdt(void);
#endif

int
efi_fdt_probe(void)
{
	EFI_STATUS status;

	status = LibGetSystemConfigurationTable(&FdtTableGuid, &fdt_data);
	if (EFI_ERROR(status))
		return EIO;

	if (fdt_check_header(fdt_data) != 0) {
		fdt_data = NULL;
		return EINVAL;
	}

	return 0;
}

int
efi_fdt_set_data(void *data)
{
	int err;

	if (fdt_check_header(data) != 0)
		return EINVAL;

	fdt_data = alloc(fdt_data_size);
	if (fdt_data == NULL)
		return ENOMEM;
	memset(fdt_data, 0, fdt_data_size);

	err = fdt_open_into(data, fdt_data, fdt_data_size);
	if (err != 0) {
		dealloc(fdt_data, fdt_data_size);
		fdt_data = NULL;
		return ENXIO;
	}

	return 0;
}

void *
efi_fdt_data(void)
{
	return fdt_data;
}

int
efi_fdt_size(void)
{
	return fdt_data == NULL ? 0 : fdt_totalsize(fdt_data);
}

bool
efi_fdt_overlay_is_compatible(void *dtbo)
{
	const int system_root = fdt_path_offset(fdt_data, "/");
	const int overlay_root = fdt_path_offset(dtbo, "/");

	if (system_root < 0 || overlay_root < 0)
		return false;

	const int system_ncompat = fdt_stringlist_count(fdt_data, system_root,
	    "compatible");
	const int overlay_ncompat = fdt_stringlist_count(dtbo, overlay_root,
	    "compatible");

	if (system_ncompat <= 0 || overlay_ncompat <= 0)
		return false;

	const char *system_compatible, *overlay_compatible;
	int si, oi;

	for (si = 0; si < system_ncompat; si++) {
		system_compatible = fdt_stringlist_get(fdt_data,
		    system_root, "compatible", si, NULL);
		if (system_compatible == NULL)
			continue;
		for (oi = 0; oi < overlay_ncompat; oi++) {
			overlay_compatible = fdt_stringlist_get(dtbo,
			    overlay_root, "compatible", oi, NULL);
			if (overlay_compatible == NULL)
				continue;
			if (strcmp(system_compatible, overlay_compatible) == 0)
				return true;
		}
	}

	return false;
}

int
efi_fdt_overlay_apply(void *dtbo, int *fdterr)
{
	int err = fdt_overlay_apply(fdt_data, dtbo);
	if (fdterr)
		*fdterr = err;
	return err == 0 ? 0 : EIO;
}

void
efi_fdt_init(u_long addr, u_long len)
{
	int error;

	error = fdt_open_into(fdt_data, (void *)addr, len);
	if (error < 0)
		panic("fdt_open_into failed: %d", error);

	fdt_data = (void *)addr;
}

void
efi_fdt_fini(void)
{
	int error;

	error = fdt_pack(fdt_data);
	if (error < 0)
		panic("fdt_pack failed: %d", error);
}

void
efi_fdt_show(void)
{
	const char *model, *compat;
	int n, ncompat;

	if (fdt_data == NULL) {
		return;
	}

	model = fdt_getprop(fdt_data, fdt_path_offset(fdt_data, "/"), "model", NULL);
	if (model) {
		command_printtab("FDT", "%s [", model);
	}
	ncompat = fdt_stringlist_count(fdt_data, fdt_path_offset(fdt_data, "/"), "compatible");
	for (n = 0; n < ncompat; n++) {
		compat = fdt_stringlist_get(fdt_data, fdt_path_offset(fdt_data, "/"),
		    "compatible", n, NULL);
		printf("%s%s", n == 0 ? "" : ", ", compat);
	}
	printf("]\n");
}

static int
efi_fdt_chosen(void)
{
	int chosen;

	chosen = fdt_path_offset(fdt_data, FDT_CHOSEN_NODE_PATH);
	if (chosen < 0)
		chosen = fdt_add_subnode(fdt_data,
		    fdt_path_offset(fdt_data, "/"),
		    FDT_CHOSEN_NODE_NAME);
	if (chosen < 0)
		panic("FDT: Failed to create " FDT_CHOSEN_NODE_PATH " node");

	return chosen;
}

void
efi_fdt_system_table(void)
{
#ifdef EFIBOOT_RUNTIME_ADDRESS
	int chosen;

	chosen = efi_fdt_chosen();

	fdt_setprop_u64(fdt_data, chosen, "netbsd,uefi-system-table", (uint64_t)(uintptr_t)ST);
#endif
}

void
efi_fdt_memory_map(void)
{
	UINTN nentries = 0, mapkey, descsize;
	EFI_MEMORY_DESCRIPTOR *md, *memmap;
	UINT32 descver;
	UINT64 phys_start, phys_size;
	int n, memory;

	memory = fdt_path_offset(fdt_data, FDT_MEMORY_NODE_PATH);
	if (memory < 0)
		memory = fdt_add_subnode(fdt_data, fdt_path_offset(fdt_data, "/"), FDT_MEMORY_NODE_NAME);
	if (memory < 0)
		panic("FDT: Failed to create " FDT_MEMORY_NODE_PATH " node");

	fdt_delprop(fdt_data, memory, "reg");

	const int address_cells = fdt_address_cells(fdt_data, fdt_path_offset(fdt_data, "/"));
	const int size_cells = fdt_size_cells(fdt_data, fdt_path_offset(fdt_data, "/"));

	memmap = LibMemoryMap(&nentries, &mapkey, &descsize, &descver);
	for (n = 0, md = memmap; n < nentries; n++, md = NextMemoryDescriptor(md, descsize)) {
		/*
		 * create / find the chosen node for each iteration as it might have changed
		 * when adding to the memory node
		 */
		int chosen = efi_fdt_chosen();
		fdt_appendprop_u32(fdt_data, chosen, "netbsd,uefi-memmap", md->Type);
		fdt_appendprop_u64(fdt_data, chosen, "netbsd,uefi-memmap", md->PhysicalStart);
		fdt_appendprop_u64(fdt_data, chosen, "netbsd,uefi-memmap", md->NumberOfPages);
		fdt_appendprop_u64(fdt_data, chosen, "netbsd,uefi-memmap", md->Attribute);

		if ((md->Attribute & EFI_MEMORY_RUNTIME) != 0)
			continue;

		if ((md->Attribute & EFI_MEMORY_WB) == 0)
			continue;
		if (!FDT_MEMORY_USABLE(md))
			continue;
		if ((address_cells == 1 || size_cells == 1) && md->PhysicalStart + (md->NumberOfPages * EFI_PAGE_SIZE) > 0xffffffff)
			continue;
		if (md->NumberOfPages <= 1)
			continue;

		phys_start = md->PhysicalStart;
		phys_size = md->NumberOfPages * EFI_PAGE_SIZE;

		if (phys_start & EFI_PAGE_MASK) {
			/*
			 * UEFI spec says these should be 4KB aligned, but
			 * U-Boot doesn't always, so round up to the next
			 * page.
			 */
			phys_start = (phys_start + EFI_PAGE_SIZE) & ~EFI_PAGE_MASK;
			phys_size -= (EFI_PAGE_SIZE * 2);
			if (phys_size == 0)
				continue;
		}

		memory = fdt_path_offset(fdt_data, FDT_MEMORY_NODE_PATH);
		if (address_cells == 1)
			fdt_appendprop_u32(fdt_data, memory, "reg",
			    (uint32_t)phys_start);
		else
			fdt_appendprop_u64(fdt_data, memory, "reg",
			    phys_start);

		if (size_cells == 1)
			fdt_appendprop_u32(fdt_data, memory, "reg",
			    (uint32_t)phys_size);
		else
			fdt_appendprop_u64(fdt_data, memory, "reg",
			    phys_size);
	}
}

void
efi_fdt_gop(void)
{
	EFI_STATUS status;
	EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
	EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *mode;
	EFI_HANDLE *gop_handle;
	UINTN ngop_handle, n;
	char buf[48];
	int fb, chosen;

	status = LibLocateHandle(ByProtocol, &GraphicsOutputProtocol, NULL, &ngop_handle, &gop_handle);
	if (EFI_ERROR(status) || ngop_handle == 0)
		return;

	for (n = 0; n < ngop_handle; n++) {
		status = uefi_call_wrapper(BS->HandleProtocol, 3, gop_handle[n], &GraphicsOutputProtocol, (void **)&gop);
		if (EFI_ERROR(status))
			continue;

		mode = gop->Mode;
		if (mode == NULL)
			continue;

#ifdef EFIBOOT_DEBUG
		printf("GOP: FB @ 0x%" PRIx64 " size 0x%" PRIxUINTN "\n", mode->FrameBufferBase, mode->FrameBufferSize);
		printf("GOP: Version %d\n", mode->Info->Version);
		printf("GOP: HRes %d VRes %d\n", mode->Info->HorizontalResolution, mode->Info->VerticalResolution);
		printf("GOP: PixelFormat %d\n", mode->Info->PixelFormat);
		printf("GOP: PixelBitmask R 0x%x G 0x%x B 0x%x Res 0x%x\n",
		    mode->Info->PixelInformation.RedMask,
		    mode->Info->PixelInformation.GreenMask,
		    mode->Info->PixelInformation.BlueMask,
		    mode->Info->PixelInformation.ReservedMask);
		printf("GOP: Pixels per scanline %d\n", mode->Info->PixelsPerScanLine);
#endif

		if (mode->Info->PixelFormat == PixelBltOnly) {
			printf("GOP: PixelBltOnly pixel format not supported\n");
			continue;
		}

		chosen = efi_fdt_chosen();
		fdt_setprop_u32(fdt_data, chosen, "#address-cells", 2);
		fdt_setprop_u32(fdt_data, chosen, "#size-cells", 2);
		fdt_setprop_empty(fdt_data, chosen, "ranges");

		snprintf(buf, sizeof(buf), "framebuffer@%" PRIx64, mode->FrameBufferBase);
		fb = fdt_add_subnode(fdt_data, chosen, buf);
		if (fb < 0) {
			/* Framebuffer node already exists. No need to create a new one! */
			return;
		}

		fdt_appendprop_string(fdt_data, fb, "compatible", "simple-framebuffer");
		fdt_appendprop_string(fdt_data, fb, "status", "okay");
		fdt_appendprop_u64(fdt_data, fb, "reg", mode->FrameBufferBase);
		fdt_appendprop_u64(fdt_data, fb, "reg", mode->FrameBufferSize);
		fdt_appendprop_u32(fdt_data, fb, "width", mode->Info->HorizontalResolution);
		fdt_appendprop_u32(fdt_data, fb, "height", mode->Info->VerticalResolution);
		fdt_appendprop_u32(fdt_data, fb, "stride", mode->Info->PixelsPerScanLine * 4);	/* XXX */
		fdt_appendprop_string(fdt_data, fb, "format", "a8b8g8r8");

#ifdef EFIBOOT_ACPI
		/*
		 * In ACPI mode, use GOP as console.
		 */
		if (efi_acpi_available()) {
			snprintf(buf, sizeof(buf), "/chosen/framebuffer@%" PRIx64, mode->FrameBufferBase);
			fdt_setprop_string(fdt_data, chosen, "stdout-path", buf);
		}
#endif

		return;
	}
}

void
efi_fdt_bootargs(const char *bootargs)
{
	struct efi_block_part *bpart = efi_block_boot_part();
	uint8_t macaddr[6];
	int chosen;

	chosen = efi_fdt_chosen();

	if (*bootargs)
		fdt_setprop_string(fdt_data, chosen, "bootargs", bootargs);

	if (bpart) {
		switch (bpart->type) {
		case EFI_BLOCK_PART_DISKLABEL:
			fdt_setprop(fdt_data, chosen, "netbsd,mbr",
			    bpart->hash, sizeof(bpart->hash));
			fdt_setprop_u32(fdt_data, chosen, "netbsd,partition",
			    bpart->index);
			break;
		case EFI_BLOCK_PART_GPT:
			if (bpart->gpt.ent.ent_name[0] == 0x0000) {
				fdt_setprop(fdt_data, chosen, "netbsd,gpt-guid",
				    bpart->hash, sizeof(bpart->hash));
			} else {
				char *label = NULL;
				int rv = ucs2_to_utf8(bpart->gpt.ent.ent_name, &label);
				if (rv == 0) {
					fdt_setprop_string(fdt_data, chosen, "netbsd,gpt-label", label);
					FreePool(label);
				}
			}
			break;
		default:
			break;
		}
	} else if (efi_net_get_booted_macaddr(macaddr) == 0) {
		fdt_setprop(fdt_data, chosen, "netbsd,booted-mac-address", macaddr, sizeof(macaddr));
	}
}

static void
efi_fdt_userconf_addprop(const char *cmd)
{
	const int chosen = efi_fdt_chosen();

	fdt_appendprop_string(fdt_data, chosen, "netbsd,userconf", cmd);
}

void
efi_fdt_userconf(void)
{
	userconf_foreach(efi_fdt_userconf_addprop);
}

void
efi_fdt_initrd(u_long initrd_addr, u_long initrd_size)
{
	int chosen;

	if (initrd_size == 0)
		return;

	chosen = efi_fdt_chosen();
	fdt_setprop_u64(fdt_data, chosen, "linux,initrd-start", initrd_addr);
	fdt_setprop_u64(fdt_data, chosen, "linux,initrd-end", initrd_addr + initrd_size);
}

/* pass in the NetBSD on-disk random seed */
void
efi_fdt_rndseed(u_long addr, u_long size)
{
	int chosen;

	if (size == 0)
		return;

	chosen = efi_fdt_chosen();
	fdt_setprop_u64(fdt_data, chosen, "netbsd,rndseed-start", addr);
	fdt_setprop_u64(fdt_data, chosen, "netbsd,rndseed-end", addr + size);
}

/* pass in output from the EFI firmware's RNG from some unknown source */
void
efi_fdt_efirng(u_long efirng_addr, u_long efirng_size)
{
	int chosen;

	if (efirng_size == 0)
		return;

	chosen = efi_fdt_chosen();
	fdt_setprop_u64(fdt_data, chosen, "netbsd,efirng-start",
	    efirng_addr);
	fdt_setprop_u64(fdt_data, chosen, "netbsd,efirng-end",
	    efirng_addr + efirng_size);
}

/* pass in module information */
void
efi_fdt_module(const char *module_name, u_long module_addr, u_long module_size)
{
	int chosen;

	if (module_size == 0)
		return;

	chosen = efi_fdt_chosen();
	fdt_appendprop_string(fdt_data, chosen, "netbsd,module-names", module_name);
	fdt_appendprop_u64(fdt_data, chosen, "netbsd,modules", module_addr);
	fdt_appendprop_u64(fdt_data, chosen, "netbsd,modules", module_size);
}

static const char *
efi_fdt_get_string(int off, const char *prop)
{
	if (strcmp(prop, "name") == 0)
		return fdt_get_name(fdt_data, off, NULL);
	else
		return fdt_getprop(fdt_data, off, prop, NULL);
}

static int
efi_of_getproplen(int off, const char *prop)
{
	const char *name;
	const void *val;
	int len;

	if (strcmp(prop, "name") == 0) {
		val = fdt_get_name(fdt_data, off, &len);
		if (val) {
			const char *p = strchr(val, '@');
			if (p) {
				len = (uintptr_t)p - (uintptr_t)val + 1;
			} else {
				len += 1;
			}
		}
	} else {
		off = fdt_first_property_offset(fdt_data, off);
		if (off < 0) {
			return -1;
		}
		while (off >= 0) {
			val = fdt_getprop_by_offset(fdt_data, off, &name, &len);
			if (val == NULL) {
				return -1;
			}
			if (strcmp(name, prop) == 0) {
				break;
			}
			off = fdt_next_property_offset(fdt_data, off);
			if (off < 0) {
				return -1;
			}
		}
	}
	if (val == NULL) {
		return -1;
	}

	return len;
}

static const char *
efi_fdt_get_string_index(int off, const char *prop, u_int index)
{
	const char *names;
	int len;

	if ((len = efi_of_getproplen(off, prop)) < 0)
		return NULL;

	names = efi_fdt_get_string(off, prop);

	return strlist_string(names, len, index);
}

/* pass out module information */
void
efi_fdt_module_foreach(void (*fn)(const char *, const uint64_t, const uint64_t, void *, void *), void *p1, void *p2)
{
	int chosen;
	const char *module_name;
	const uint64_t *data;
	int dlen;
	u_int index;

	chosen = efi_fdt_chosen();
	if (chosen == -1)
		return;
	data = fdt_getprop(fdt_data, chosen, "netbsd,modules", &dlen);
	if (data == NULL)
		return;
		
	for (index = 0; index < dlen / 16; index++, data += 2) {
		module_name = efi_fdt_get_string_index(chosen,
		    "netbsd,module-names", index);
		if (module_name == NULL)
			break;

		const uint64_t addr = be64dec(data + 0);
		const uint64_t size = be64dec(data + 1);
		fn(module_name, addr, size, p1, p2);
	}
}

static void
apply_overlay(const char *path, void *dtbo)
{

	if (!efi_fdt_overlay_is_compatible(dtbo)) {
		printf("boot: %s: incompatible overlay\n", path);
		return;
	}

	int fdterr;

	if (efi_fdt_overlay_apply(dtbo, &fdterr) != 0) {
		printf("boot: %s: error %d applying overlay\n", path, fdterr);
	}
}

static void
apply_overlay_file(const char *path)
{
	EFI_PHYSICAL_ADDRESS dtbo_addr;
	u_long dtbo_size;

	if (strlen(path) == 0)
		return;

	if (load_file(path, 0, false, &dtbo_addr, &dtbo_size) != 0 ||
	    dtbo_addr == 0) {
		/* Error messages have already been displayed. */
		goto out;
	}

	apply_overlay(path, (void *)(uintptr_t)dtbo_addr);

out:
	if (dtbo_addr) {
		uefi_call_wrapper(BS->FreePages, 2, dtbo_addr,
		    EFI_SIZE_TO_PAGES(dtbo_size));
	}
}

static void
load_fdt_overlays(void)
{
	if (!dtoverlay_enabled)
		return;

	dtoverlay_foreach(apply_overlay_file);
}

static void
load_module(const char *module_name)
{
	EFI_PHYSICAL_ADDRESS addr;
	u_long size;
	char path[PATH_MAX];

	snprintf(path, sizeof(path), "%s/%s/%s.kmod", module_prefix,
	    module_name, module_name);

	if (load_file(path, 0, false, &addr, &size) != 0 || addr == 0 || size == 0)
	    return;

	efi_fdt_module(module_name, (u_long)addr, size);
}

static void
load_modules(const char *kernel_name)
{
	if (!module_enabled)
		return;

	module_init(kernel_name);
	module_foreach(load_module);
}


/*
 * Prepare kernel arguments and shutdown boot services.
 */
int
efi_fdt_prepare_boot(const char *fname, const char *args, u_long *marks)
{
	int error;

	load_file(get_initrd_path(), 0, false, &initrd_addr, &initrd_size);
	load_file(get_dtb_path(), 0, false, &dtb_addr, &dtb_size);

	error = efi_md_prepare_boot(fname, args, marks);
	if (error) {
		return error;
	}
#ifdef EFIBOOT_ACPI
	/* ACPI support only works for little endian kernels */
	if (efi_acpi_available() && netbsd_elf_data == ELFDATA2LSB) {
		error = efi_fdt_create_acpifdt();
		if (error != 0) {
			return error;
		}
	} else
#endif
	if (dtb_addr && efi_fdt_set_data((void *)(uintptr_t)dtb_addr) != 0) {
		return EINVAL;
	}

	if (efi_fdt_size() > 0) {
		/*
		 * Load the rndseed as late as possible -- after we
		 * have committed to using fdt and executing this
		 * kernel -- so that it doesn't hang around in memory
		 * if we have to bail or the kernel won't use it.
		 */
		load_file(get_rndseed_path(), 0, false,
		    &rndseed_addr, &rndseed_size);

		efi_fdt_init((marks[MARK_END] + FDT_ALIGN - 1) & -FDT_ALIGN, FDT_ALIGN);
		load_modules(fname);
		load_fdt_overlays();
		efi_fdt_initrd(initrd_addr, initrd_size);
		efi_fdt_rndseed(rndseed_addr, rndseed_size);
		efi_fdt_efirng(efirng_addr, efirng_size);
		efi_fdt_bootargs(args);
		efi_fdt_userconf();
		efi_fdt_system_table();
		efi_fdt_gop();
		efi_fdt_memory_map();
	}

	efi_cleanup();

	if (efi_fdt_size() > 0) {
		efi_fdt_fini();
	}

	return 0;
}

/*
 * Free memory after a failed boot.
 */
void
efi_fdt_cleanup_boot(void)
{
	if (rndseed_addr) {
		uefi_call_wrapper(BS->FreePages, 2, rndseed_addr, EFI_SIZE_TO_PAGES(rndseed_size));
		rndseed_addr = 0;
		rndseed_size = 0;
	}
	if (initrd_addr) {
		uefi_call_wrapper(BS->FreePages, 2, initrd_addr, EFI_SIZE_TO_PAGES(initrd_size));
		initrd_addr = 0;
		initrd_size = 0;
	}
	if (dtb_addr) {
		uefi_call_wrapper(BS->FreePages, 2, dtb_addr, EFI_SIZE_TO_PAGES(dtb_size));
		dtb_addr = 0;
		dtb_size = 0;
	}
}

size_t
efi_fdt_alloc_size(void)
{
	return FDT_SPACE;
}

/*
 * Native NetBSD boot hooks (exec.c); on FDT platforms the kernel
 * hand-off state is the device tree.
 */
size_t
efi_md_boot_alloc_size(const char *fname)
{
	return efi_fdt_alloc_size();
}

int
efi_md_prepare_netbsd(const char *fname, const char *args, u_long *marks)
{
	return efi_fdt_prepare_boot(fname, args, marks);
}

void
efi_md_cleanup_boot(void)
{
	efi_fdt_cleanup_boot();
}

#ifdef EFIBOOT_ACPI
int
efi_fdt_create_acpifdt(void)
{
	void *acpi_root = efi_acpi_root();
	void *smbios_table = efi_acpi_smbios();
	void *fdt;
	int error;

	if (acpi_root == NULL)
		return EINVAL;

	fdt = AllocatePool(ACPI_FDT_SIZE);
	if (fdt == NULL)
		return ENOMEM;

	error = fdt_create_empty_tree(fdt, ACPI_FDT_SIZE);
	if (error)
		return EIO;

	const char *model = efi_acpi_get_model();

	fdt_setprop_string(fdt, fdt_path_offset(fdt, "/"), "compatible", "netbsd,generic-acpi");
	fdt_setprop_string(fdt, fdt_path_offset(fdt, "/"), "model", model);
	fdt_setprop_cell(fdt, fdt_path_offset(fdt, "/"), "#address-cells", 2);
	fdt_setprop_cell(fdt, fdt_path_offset(fdt, "/"), "#size-cells", 2);

	fdt_add_subnode(fdt, fdt_path_offset(fdt, "/"), "chosen");
	fdt_setprop_u64(fdt, fdt_path_offset(fdt, "/chosen"), "netbsd,acpi-root-table", (uint64_t)(uintptr_t)acpi_root);
	if (smbios_table)
		fdt_setprop_u64(fdt, fdt_path_offset(fdt, "/chosen"), "netbsd,smbios-table", (uint64_t)(uintptr_t)smbios_table);

	fdt_add_subnode(fdt, fdt_path_offset(fdt, "/"), "acpi");
	fdt_setprop_string(fdt, fdt_path_offset(fdt, "/acpi"), "compatible", "netbsd,acpi");

	return efi_fdt_set_data(fdt);
}
#endif

#ifdef EFIBOOT_RUNTIME_ADDRESS
static uint64_t
efi_fdt_runtime_alloc_va(uint64_t pa, uint64_t npages)
{
#if EFIBOOT_RUNTIME_ADDRESS != 0
	static uint64_t va = EFIBOOT_RUNTIME_ADDRESS;
	static uint64_t sz = EFIBOOT_RUNTIME_SIZE;
	uint64_t nva;

	if (sz < (npages * EFI_PAGE_SIZE)) {
		panic("efi_acpi_alloc_va: couldn't allocate %" PRIu64 " pages",
		    npages);
	}

	nva = va;
	va += (npages * EFI_PAGE_SIZE);
	sz -= (npages * EFI_PAGE_SIZE);

	return nva;
#else
	return pa;
#endif
}

void
efi_fdt_set_virtual_address_map(EFI_MEMORY_DESCRIPTOR *memmap, UINTN nentries,
    UINTN mapkey, UINTN descsize, UINT32 descver)
{
	EFI_MEMORY_DESCRIPTOR *md, *vmd, *vmemmap;
	EFI_STATUS status;
	int n, nrt;
	void *fdt;

	fdt = efi_fdt_data();

	vmemmap = alloc(nentries * descsize);
	if (vmemmap == NULL)
		panic("FATAL: couldn't allocate virtual memory map");

	for (n = 0, nrt = 0, vmd = vmemmap, md = memmap;
	     n < nentries;
	     n++, md = NextMemoryDescriptor(md, descsize)) {

		if ((md->Attribute & EFI_MEMORY_RUNTIME) == 0) {
			continue;
		}

		md->VirtualStart =
		    efi_fdt_runtime_alloc_va(md->PhysicalStart, md->NumberOfPages);

		switch (md->Type) {
		case EfiRuntimeServicesCode:
			fdt_appendprop_u64(fdt, fdt_path_offset(fdt, "/chosen"),
			    "netbsd,uefi-runtime-code", md->PhysicalStart);
			fdt_appendprop_u64(fdt, fdt_path_offset(fdt, "/chosen"),
			    "netbsd,uefi-runtime-code", md->VirtualStart);
			fdt_appendprop_u64(fdt, fdt_path_offset(fdt, "/chosen"),
			    "netbsd,uefi-runtime-code",
			    md->NumberOfPages * EFI_PAGE_SIZE);
			break;
		case EfiRuntimeServicesData:
			fdt_appendprop_u64(fdt, fdt_path_offset(fdt, "/chosen"),
			    "netbsd,uefi-runtime-data", md->PhysicalStart);
			fdt_appendprop_u64(fdt, fdt_path_offset(fdt, "/chosen"),
			    "netbsd,uefi-runtime-data", md->VirtualStart);
			fdt_appendprop_u64(fdt, fdt_path_offset(fdt, "/chosen"),
			    "netbsd,uefi-runtime-data",
			    md->NumberOfPages * EFI_PAGE_SIZE);
			break;
		case EfiMemoryMappedIO:
			fdt_appendprop_u64(fdt, fdt_path_offset(fdt, "/chosen"),
			    "netbsd,uefi-runtime-mmio", md->PhysicalStart);
			fdt_appendprop_u64(fdt, fdt_path_offset(fdt, "/chosen"),
			    "netbsd,uefi-runtime-mmio", md->VirtualStart);
			fdt_appendprop_u64(fdt, fdt_path_offset(fdt, "/chosen"),
			    "netbsd,uefi-runtime-mmio",
			    md->NumberOfPages * EFI_PAGE_SIZE);
			break;
		default:
			break;
		}

		*vmd = *md;
		vmd = NextMemoryDescriptor(vmd, descsize);
		++nrt;
	}

	status = uefi_call_wrapper(RT->SetVirtualAddressMap, 4, nrt * descsize,
	    descsize, descver, vmemmap);
	if (EFI_ERROR(status)) {
		printf("WARNING: SetVirtualAddressMap failed\n");
		return;
	}
}
#endif

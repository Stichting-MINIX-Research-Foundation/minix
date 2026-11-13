/* $NetBSD: efigop.c,v 1.3 2022/08/14 11:26:41 jmcneill Exp $ */

/*-
 * Copyright (c) 2021 Jared McNeill <jmcneill@invisible.ca>
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

static EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;

void
efi_gop_probe(void)
{
	EFI_HANDLE *gop_handle;
	UINTN ngop_handle;
	EFI_STATUS status;

	status = LibLocateHandle(ByProtocol, &GraphicsOutputProtocol, NULL,
	    &ngop_handle, &gop_handle);
	if (EFI_ERROR(status) || ngop_handle == 0) {
		return;
	}

	for (size_t n = 0; n < ngop_handle; n++) {
		status = uefi_call_wrapper(BS->HandleProtocol, 3,
		    gop_handle[n], &GraphicsOutputProtocol, (void **)&gop);
		if (EFI_ERROR(status) || gop->Mode == NULL) {
			gop = NULL;
			continue;
		} else {
			break;
		}
	}
}

void *
efi_gop_found(void)
{
	return (void *)gop;
}

static uint32_t
efi_gop_bpp(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info)
{
	if (info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor ||
	    info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
		return 24;
	}

	return popcount32(info->PixelInformation.RedMask) +
	    popcount32(info->PixelInformation.GreenMask) +
	    popcount32(info->PixelInformation.BlueMask);
}

static void
efi_gop_printmode(UINT32 mode, EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info)
{
	char buf[sizeof("NNNNN: HHHHHxVVVVV BB")];

	snprintf(buf, sizeof(buf), "%-5u: %ux%u %u", mode,
	    info->HorizontalResolution, info->VerticalResolution,
	    efi_gop_bpp(info));

	printf("%-21s", buf);
}

void
efi_gop_show(void)
{
	if (gop == NULL) {
		return;
	}

	command_printtab("GOP", "");
	efi_gop_printmode(gop->Mode->Mode, gop->Mode->Info);
	if (gop->Mode->FrameBufferBase != 0) {
		printf(" (0x%lx,0x%lx)",
		    (unsigned long)gop->Mode->FrameBufferBase,
		    (unsigned long)gop->Mode->FrameBufferSize);
	}
	printf("\n");
}

void
efi_gop_dump(void)
{
	EFI_STATUS status;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
	UINTN size;

	if (gop == NULL) {
		return;
	}

	for (UINT32 mode = 0; mode < gop->Mode->MaxMode; mode++) {
		status = uefi_call_wrapper(gop->QueryMode, 4, gop, mode,
		    &size, &info);
		if (EFI_ERROR(status)) {
			continue;
		}
		if (mode == gop->Mode->Mode) {
			printf(" -> ");
		} else {
			printf("    ");
		}
		efi_gop_printmode(mode, info);
		if (mode != gop->Mode->MaxMode - 1 &&
		    mode % 3 == 2) {
			printf("\n");
		}
	}
	printf("\n");
}

void
efi_gop_setmode(UINT32 mode)
{
	EFI_STATUS status;

	if (gop == NULL) {
		return;
	}

	status = uefi_call_wrapper(gop->SetMode, 2, gop, mode);
	if (EFI_ERROR(status)) {
		printf("Failed to set video mode: %ld\n", (long)status);
	}
}

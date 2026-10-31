/*	$NetBSD: bltgrid.c,v 1.1.1.1 2018/08/16 18:17:47 jmcneill Exp $	*/

#include <efi.h>
#include <efilib.h>

extern EFI_GUID GraphicsOutputProtocol;

static void
fill_boxes(UINT32 *PixelBuffer, UINT32 Width, UINT32 Height)
{
	UINT32 y, x = 0;
	/*
	 * This assums BGRR, but it doesn't really matter; we pick red and
	 * green so it'll just be blue/green if the pixel format is backwards.
	 */
	EFI_GRAPHICS_OUTPUT_BLT_PIXEL Red = {0, 0, 0xff, 0},
				      Green = {0, 0xff, 0, 0},
				      *Color;

	for (y = 0; y < Height; y++) {
		Color = ((y / 32) % 2 == 0) ? &Red : &Green;
		for (x = 0; x < Width; x++) {
			if (x % 32 == 0 && x != 0)
				Color = (Color == &Red) ? &Green : &Red;
			PixelBuffer[y * Width + x] = *(UINT32 *)Color;
		}
	}
}

static void
draw_boxes(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop)
{
	int i, imax;
	EFI_STATUS rc;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
	UINTN NumPixels;
	UINT32 *PixelBuffer;
	UINT32 BufferSize;

	if (gop->Mode) {
		imax = gop->Mode->MaxMode;
	} else {
		Print(L"gop->Mode is NULL\n");
		return;
	}

	for (i = 0; i < imax; i++) {
		UINTN SizeOfInfo;
		rc = uefi_call_wrapper(gop->QueryMode, 4, gop, i, &SizeOfInfo,
					&info);
		if (EFI_ERROR(rc) && rc == EFI_NOT_STARTED) {
			Print(L"gop->QueryMode() returned %r\n", rc);
			Print(L"Trying to start GOP with SetMode().\n");
			rc = uefi_call_wrapper(gop->SetMode, 2, gop,
				gop->Mode ? gop->Mode->Mode : 0);
			rc = uefi_call_wrapper(gop->QueryMode, 4, gop, i,
				&SizeOfInfo, &info);
		}

		if (EFI_ERROR(rc)) {
			Print(L"%d: Bad response from QueryMode: %r (%d)\n",
			      i, rc, rc);
			continue;
		}

		if (CompareMem(info, gop->Mode->Info, sizeof (*info)))
			continue;

		NumPixels = info->VerticalResolution * info->HorizontalResolution;
		BufferSize = NumPixels * sizeof(UINT32);

		PixelBuffer = AllocatePool(BufferSize);
		if (!PixelBuffer) {
			Print(L"Allocation of 0x%08lx bytes failed.\n",
			      sizeof(UINT32) * NumPixels);
			return;
		}

		fill_boxes(PixelBuffer,
			   info->HorizontalResolution, info->VerticalResolution);

		uefi_call_wrapper(gop->Blt, 10, gop,
				  (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)PixelBuffer,
				  EfiBltBufferToVideo,
				  0, 0, 0, 0,
				  info->HorizontalResolution,
				  info->VerticalResolution,
				  0);
		return;
	}
	Print(L"Never found the active video mode?\n");
}

static EFI_STATUS
SetWatchdog(UINTN seconds)
{
	EFI_STATUS rc;
	rc = uefi_call_wrapper(BS->SetWatchdogTimer, 4, seconds, 0x1ffff,
				0, NULL);
	if (EFI_ERROR(rc)) {
		CHAR16 Buffer[64];
		StatusToString(Buffer, rc);
		Print(L"Bad response from QueryMode: %s (%d)\n", Buffer, rc);
	}
	return rc;
}

EFI_STATUS
efi_main (EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *systab)
{
	EFI_STATUS rc;
	EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;

	InitializeLib(image_handle, systab);

	SetWatchdog(10);

	rc = LibLocateProtocol(&GraphicsOutputProtocol, (void **)&gop);
	if (EFI_ERROR(rc)) {
		Print(L"Could not locate GOP: %r\n", rc);
		return rc;
	}

	if (!gop) {
		Print(L"LocateProtocol(GOP, &gop) returned %r but GOP is NULL\n", rc);
		return EFI_UNSUPPORTED;
	}

	draw_boxes(gop);

	SetWatchdog(0);
	return EFI_SUCCESS;
}

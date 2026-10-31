/*	$NetBSD: lfbgrid.c,v 1.1.1.1 2018/08/16 18:17:47 jmcneill Exp $	*/

#include <efi.h>
#include <efilib.h>

extern EFI_GUID GraphicsOutputProtocol;

#define be32_to_cpu(x) __builtin_bswap32(x)

static void
fill_boxes(UINT32 *PixelBuffer, UINT32 Width, UINT32 Height, UINT32 Pitch,
	   EFI_GRAPHICS_PIXEL_FORMAT Format, EFI_PIXEL_BITMASK Info )
{
	UINT32 Red, Green;
	UINT32 y, x, color;

	switch(Format) {
	case PixelRedGreenBlueReserved8BitPerColor:
		Red = be32_to_cpu(0xff000000);
		Green = be32_to_cpu(0x00ff0000);
		break;
	case PixelBlueGreenRedReserved8BitPerColor:
		Red = be32_to_cpu(0x0000ff00);
		Green = be32_to_cpu(0x00ff0000);
		break;
	case PixelBitMask:
		Red = Info.RedMask;
		Green = Info.GreenMask;
		break;
	case PixelBltOnly:
		return;
	default:
		Print(L"Invalid pixel format\n");
		return;
	}

	for (y = 0; y < Height; y++) {
		color = ((y / 32) % 2 == 0) ? Red : Green;
		for (x = 0; x < Width; x++) {
			if (x % 32 == 0 && x != 0)
				color = (color == Red) ? Green : Red;
			PixelBuffer[y * Pitch + x] = color;
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
	UINT32 CopySize, BufferSize;
#if defined(__x86_64__) || defined(__aarch64__)
	UINT64 FrameBufferAddr;
#elif defined(__i386__) || defined(__arm__)
	UINT32 FrameBufferAddr;
#else
#error YOUR ARCH HERE
#endif

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

		NumPixels = info->VerticalResolution * info->PixelsPerScanLine;
		BufferSize = NumPixels * sizeof(UINT32);
		if (BufferSize == gop->Mode->FrameBufferSize) {
			CopySize = BufferSize;
		} else {
			CopySize = BufferSize < gop->Mode->FrameBufferSize ?
				BufferSize : gop->Mode->FrameBufferSize;
			Print(L"height * pitch * pixelsize = %lu buf fb size is %lu; using %lu\n",
			      BufferSize, gop->Mode->FrameBufferSize, CopySize);
		}

		PixelBuffer = AllocatePool(BufferSize);
		if (!PixelBuffer) {
			Print(L"Allocation of 0x%08lx bytes failed.\n",
			      sizeof(UINT32) * NumPixels);
			return;
		}

		fill_boxes(PixelBuffer, info->HorizontalResolution,
			   info->VerticalResolution, info->PixelsPerScanLine,
			   info->PixelFormat, info->PixelInformation);

		if (info->PixelFormat == PixelBltOnly) {
			Print(L"No linear framebuffer on this device.\n");
			return;
		}
#if defined(__x86_64__) || defined(__aarch64__)
		FrameBufferAddr = (UINT64)gop->Mode->FrameBufferBase;
#elif defined(__i386__) || defined(__arm__)
		FrameBufferAddr = (UINT32)(UINT64)gop->Mode->FrameBufferBase;
#else
#error YOUR ARCH HERE
#endif

		CopyMem((VOID *)FrameBufferAddr, PixelBuffer, CopySize);
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

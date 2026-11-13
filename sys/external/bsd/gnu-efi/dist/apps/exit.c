/*	$NetBSD: exit.c,v 1.1.1.1 2018/08/16 18:17:47 jmcneill Exp $	*/

#include <efi.h>
#include <efilib.h>

EFI_STATUS
efi_main (EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *systab)
{
	InitializeLib(image_handle, systab);

	Exit(EFI_SUCCESS, 0, NULL);

	return EFI_UNSUPPORTED;
}

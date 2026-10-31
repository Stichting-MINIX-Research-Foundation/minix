/*	$NetBSD: setdbg.c,v 1.1.1.1 2018/08/16 18:17:47 jmcneill Exp $	*/

#include <efi.h>
#include <efilib.h>

EFI_GUID GRUB_EFI_GRUB_VARIABLE_GUID = {0x91376aff,0xcba6,0x42be,{0x94,0x9d,0x06,0xfd,0xe8,0x11,0x28,0xe8}};
EFI_GUID SHIM_GUID = {0x605dab50,0xe046,0x4300,{0xab,0xb6,0x3d,0xd8,0x10,0xdd,0x8b,0x23}};

char grubenv[] = "# GRUB Environment Block\n\
debug=tcp,http,net\n\
####################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################################";

EFI_STATUS
efi_main (EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
	EFI_STATUS status;
	InitializeLib(image, systab);
#if 0
	UINT8 data = 1;

	status = RT->SetVariable(L"SHIM_DEBUG", &SHIM_GUID,
				 EFI_VARIABLE_NON_VOLATILE |
				 EFI_VARIABLE_BOOTSERVICE_ACCESS |
				 EFI_VARIABLE_RUNTIME_ACCESS,
				 sizeof(data), &data);
	if (EFI_ERROR(status))
		Print(L"SetVariable failed: %r\n", status);
#endif

	status = RT->SetVariable(L"GRUB_ENV", &SHIM_GUID,
				 EFI_VARIABLE_NON_VOLATILE |
				 EFI_VARIABLE_BOOTSERVICE_ACCESS |
				 EFI_VARIABLE_RUNTIME_ACCESS,
				 sizeof(grubenv)-1, grubenv);
	if (EFI_ERROR(status))
		Print(L"SetVariable(GRUB_ENV) failed: %r\n", status);

	return EFI_SUCCESS;
}

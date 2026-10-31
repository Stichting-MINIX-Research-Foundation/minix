/*	$NetBSD: printenv.c,v 1.1.1.1 2014/04/01 16:16:06 jakllsch Exp $	*/

#include <efi.h>
#include <efilib.h>

EFI_STATUS
efi_main (EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
	EFI_STATUS status;
	CHAR16 name[256], *val, fmt[20];
	EFI_GUID vendor;
	UINTN size;

	InitializeLib(image, systab);

	name[0] = 0;
	vendor = NullGuid;

	Print(L"GUID                                Variable Name        Value\n");
	Print(L"=================================== ==================== ========\n");

	StrCpy(fmt, L"%.-35g %.-20s %s\n");
	while (1) {
		size = sizeof(name);
		status = uefi_call_wrapper(RT->GetNextVariableName, 3, &size, name, &vendor);
		if (status != EFI_SUCCESS)
			break;

		val = LibGetVariable(name, &vendor);
		Print(fmt, &vendor, name, val);
		FreePool(val);
	}
	return EFI_SUCCESS;
}

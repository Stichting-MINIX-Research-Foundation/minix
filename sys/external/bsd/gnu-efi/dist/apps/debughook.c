/*	$NetBSD: debughook.c,v 1.1.1.1 2018/08/16 18:17:47 jmcneill Exp $	*/

#include <efi.h>
#include <efilib.h>

EFI_STATUS
GetVariableAttr(CHAR16 *var, UINT8 **data, UINTN *len, EFI_GUID owner,
		  UINT32 *attributes)
{
	EFI_STATUS efi_status;

	*len = 0;

	efi_status = uefi_call_wrapper(RT->GetVariable, 5, var, &owner,
				       NULL, len, NULL);
	if (efi_status != EFI_BUFFER_TOO_SMALL)
		return efi_status;

	*data = AllocateZeroPool(*len);
	if (!*data)
		return EFI_OUT_OF_RESOURCES;

	efi_status = uefi_call_wrapper(RT->GetVariable, 5, var, &owner,
				       attributes, len, *data);

	if (efi_status != EFI_SUCCESS) {
		FreePool(*data);
		*data = NULL;
	}
	return efi_status;
}

EFI_STATUS
GetVariable(CHAR16 *var, UINT8 **data, UINTN *len, EFI_GUID owner)
{
	return GetVariableAttr(var, data, len, owner, NULL);
}

EFI_GUID DUMMY_GUID =
{0x55aad538, 0x8f82, 0x4e2a, {0xa4,0xf0,0xbe, 0x59, 0x13, 0xb6, 0x5f, 0x1e}};

#if defined(__clang__)
# define _OPTNONE __attribute__((optnone))
#else
# define _OPTNONE __attribute__((__optimize__("0")))
#endif

static _OPTNONE void
DebugHook(void)
{
	EFI_GUID guid = DUMMY_GUID;
	UINT8 *data = NULL;
	UINTN dataSize = 0;
	EFI_STATUS efi_status;
	register volatile unsigned long long x = 0;
	extern char _text, _data;

	if (x)
		return;

	efi_status = GetVariable(L"DUMMY_DEBUG", &data, &dataSize, guid);
	if (EFI_ERROR(efi_status)) {
		return;
	}

	Print(L"add-symbol-file /usr/lib/debug/boot/efi/debughook.debug "
	      L"0x%08x -s .data 0x%08x\n", &_text, &_data);

	Print(L"Pausing for debugger attachment.\n");
	Print(L"To disable this, remove the EFI variable DUMMY_DEBUG-%g .\n",
	      &guid);
	x = 1;
	while (x++) {
		/* Make this so it can't /totally/ DoS us. */
#if defined(__x86_64__) || defined(__i386__) || defined(__i686__)
		if (x > 4294967294ULL)
			break;
		__asm__ __volatile__("pause");
#elif defined(__aarch64__)
		if (x > 1000)
			break;
		__asm__ __volatile__("wfi");
#else
		if (x > 12000)
			break;
		uefi_call_wrapper(BS->Stall, 1, 5000);
#endif
	}
	x = 1;
}


EFI_STATUS
efi_main (EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
	InitializeLib(image, systab);
	DebugHook();
	return EFI_SUCCESS;
}

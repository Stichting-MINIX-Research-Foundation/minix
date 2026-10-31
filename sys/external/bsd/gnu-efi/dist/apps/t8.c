/*	$NetBSD: t8.c,v 1.1.1.1 2018/08/16 18:17:47 jmcneill Exp $	*/

#include <efi.h>
#include <efilib.h>

EFI_STATUS
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  INTN Argc, i;
  CHAR16 **Argv;

  InitializeLib(ImageHandle, SystemTable);
  Argc = GetShellArgcArgv(ImageHandle, &Argv);

  Print(L"Hello World, started with Argc=%d\n", Argc);
  for (i = 0 ; i < Argc ; ++i)
    Print(L"  Argv[%d] = '%s'\n", i, Argv[i]);

  Print(L"Bye.\n");
  return EFI_SUCCESS;
}

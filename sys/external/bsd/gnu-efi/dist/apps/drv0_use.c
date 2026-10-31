/*	$NetBSD: drv0_use.c,v 1.1.1.1 2018/08/16 18:17:47 jmcneill Exp $	*/

/*
 * Copyright (C) 2013 David Decotigny <decot@googlers.com>
 *
 * See drv0.c for an example session.
 */

#include <efi.h>
#include <efilib.h>
#include "drv0.h"


static EFI_GUID GnuEfiAppsDrv0ProtocolGuid
  = GNU_EFI_APPS_DRV0_PROTOCOL_GUID;


static
EFI_STATUS
PlayWithGnuEfiAppsDrv0Protocol(IN EFI_HANDLE DrvHandle) {
  EFI_STATUS Status;
  GNU_EFI_APPS_DRV0_PROTOCOL *drv = NULL;
  UINTN NumberOfHello = 0;

  Status = uefi_call_wrapper(BS->OpenProtocol, 6,
                             DrvHandle,
                             &GnuEfiAppsDrv0ProtocolGuid,
                             (void**)&drv,
                             DrvHandle,
                             NULL,
                             EFI_OPEN_PROTOCOL_GET_PROTOCOL);
  if (EFI_ERROR(Status)) {
    Print(L"Cannot open proto: %d\n", Status);
    return Status;
  }

  Status = uefi_call_wrapper(drv->SayHello, 2, L"Sample UEFI Driver");
  if (EFI_ERROR(Status)) {
    Print(L"Cannot call SayHello: %d\n", Status);
  }

  Status = uefi_call_wrapper(drv->GetNumberOfHello, 2, &NumberOfHello);
  if (EFI_ERROR(Status)) {
    Print(L"Cannot call GetNumberOfHello: %d\n", Status);
  } else {
    Print(L"Hello was called %d time(s).\n", NumberOfHello);
  }

  return EFI_SUCCESS;
}


EFI_STATUS
efi_main (EFI_HANDLE Image, EFI_SYSTEM_TABLE *SysTab)
{
  EFI_STATUS Status;
  EFI_HANDLE *Handles = NULL;
  UINTN i, NoHandles = 0;

  InitializeLib(Image, SysTab);

  Status = LibLocateHandle(ByProtocol, &GnuEfiAppsDrv0ProtocolGuid,
                           NULL, &NoHandles, &Handles);
  if (EFI_ERROR(Status)) {
    Print(L"Error looking up handles for proto: %d\n", Status);
    return Status;
  }

  for (i = 0 ; i < NoHandles ; ++i)
  {
    Print(L"Playing with driver instance %d...\n", i);
    Status = PlayWithGnuEfiAppsDrv0Protocol(Handles[i]);
    if (EFI_ERROR(Status))
      Print(L"Error playing with instance %d, skipping\n", i);
  }

  if (Handles)
    FreePool(Handles);

  return EFI_SUCCESS;
}

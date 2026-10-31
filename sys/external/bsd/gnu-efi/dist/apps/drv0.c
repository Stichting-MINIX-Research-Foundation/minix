/*	$NetBSD: drv0.c,v 1.1.1.1 2018/08/16 18:17:47 jmcneill Exp $	*/

/*
 * Copyright (C) 2013 David Decotigny <decot@googlers.com>
 *
 * Sample EFI shell session, together with drv0_use.efi:
 *
 *  # Loading first instance:
 *
 *    fs0:\> load drv0.efi
 *    Driver instance loaded successfully.
 *    load: Image fs0:\drv0.efi loaded at 2FD7C000 - Success
 *
 *  # Testing 1st instance:
 *
 *    fs0:\> drv0_use.efi
 *    Playing with driver instance 0...
 *    Hello Sample UEFI Driver!
 *    Hello was called 1 time(s).
 *
 *    fs0:\> drv0_use.efi
 *    Playing with driver instance 0...
 *    Hello Sample UEFI Driver!
 *    Hello was called 2 time(s).
 *
 *  # Loading another instance:
 *
 *    fs0:\> load drv0.efi
 *    Driver instance loaded successfully.
 *    load: Image fs0:\drv0.efi loaded at 2FD6D000 - Success
 *
 *  # Using both instances:
 *
 *    fs0:\> drv0_use.efi
 *    Playing with driver instance 0...
 *    Hello Sample UEFI Driver!
 *    Hello was called 3 time(s).
 *    Playing with driver instance 1...
 *    Hello Sample UEFI Driver!
 *    Hello was called 1 time(s).
 *
 *    fs0:\> drv0_use.efi
 *    Playing with driver instance 0...
 *    Hello Sample UEFI Driver!
 *    Hello was called 4 time(s).
 *    Playing with driver instance 1...
 *    Hello Sample UEFI Driver!
 *    Hello was called 2 time(s).
 *
 *  # Removing 1st instance:
 *
 *    fs0:\> dh
 *    Handle dump
 *      1: Image(DxeCore)
 *    [...]
 *     79: Image(\/drv0.efi) ImageDevPath (..A,0x800,0x17F7DF)/\/drv0.efi)
 *     7A: Image(\/drv0.efi) ImageDevPath (..A,0x800,0x17F7DF)/\/drv0.efi)
 *
 *    fs0:\> unload 79
 *     79: Image(\/drv0.efi) ImageDevPath (..A,0x800,0x17F7DF)/\/drv0.efi)
 *    Unload driver image (y/n)? y
 *    Driver instance unloaded.
 *    unload: Success
 *
 *  # Only 2nd instance remaining:
 *
 *    fs0:\> drv0_use.efi
 *    Playing with driver instance 0...
 *    Hello Sample UEFI Driver!
 *    Hello was called 3 time(s).
 *
 *  # Removing 2nd/last instance:
 *
 *    fs0:\> dh
 *    Handle dump
 *      1: Image(DxeCore)
 *    [...]
 *     79: Image(\/drv0.efi) ImageDevPath (..A,0x800,0x17F7DF)/\/drv0.efi)
 *
 *    fs0:\> unload 79
 *     79: Image(\/drv0.efi) ImageDevPath (..A,0x800,0x17F7DF)/\/drv0.efi)
 *    Unload driver image (y/n)? y
 *    Driver instance unloaded.
 *    unload: Success
 *
 *  # Expect error: no other drv0 instance left
 *
 *    fs0:\> drv0_use.efi
 *    Error looking up handles for proto: 14
 */

#include <efi.h>
#include <efilib.h>
#include "drv0.h"


static const EFI_GUID GnuEfiAppsDrv0ProtocolGuid
  = GNU_EFI_APPS_DRV0_PROTOCOL_GUID;

static struct {
  GNU_EFI_APPS_DRV0_PROTOCOL Proto;
  UINTN Counter;
} InternalGnuEfiAppsDrv0ProtocolData;


static
EFI_STATUS
EFI_FUNCTION
Drv0SayHello(
    IN const CHAR16 *HelloWho
    )
{
  if (! HelloWho)
    return EFI_INVALID_PARAMETER;

  Print(L"Hello %s!\n", HelloWho);
  InternalGnuEfiAppsDrv0ProtocolData.Counter ++;
  return EFI_SUCCESS;
}


static
EFI_STATUS
EFI_FUNCTION
Drv0GetNumberOfHello(
    OUT UINTN *NumberOfHello
    )
{
  if (! NumberOfHello)
    return EFI_INVALID_PARAMETER;

  *NumberOfHello = InternalGnuEfiAppsDrv0ProtocolData.Counter;
  return EFI_SUCCESS;
}


static
EFI_STATUS
EFI_FUNCTION
Drv0Unload(IN EFI_HANDLE ImageHandle)
{
  LibUninstallProtocolInterfaces(ImageHandle,
                                 &GnuEfiAppsDrv0ProtocolGuid,
                                 &InternalGnuEfiAppsDrv0ProtocolData.Proto,
                                 NULL);
  Print(L"Driver instance unloaded.\n", ImageHandle);
  return EFI_SUCCESS;
}


EFI_STATUS
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SysTab)
{
  EFI_STATUS Status;
  EFI_LOADED_IMAGE *LoadedImage = NULL;

  InitializeLib(ImageHandle, SysTab);

  /* Initialize global protocol definition + data */
  InternalGnuEfiAppsDrv0ProtocolData.Proto.SayHello
      = (GNU_EFI_APPS_DRV0_SAY_HELLO) Drv0SayHello;
  InternalGnuEfiAppsDrv0ProtocolData.Proto.GetNumberOfHello
      = (GNU_EFI_APPS_DRV0_GET_NUMBER_OF_HELLO) Drv0GetNumberOfHello;
  InternalGnuEfiAppsDrv0ProtocolData.Counter = 0;

  /* Grab handle to this image: we'll attach our proto instance to it */
  Status = uefi_call_wrapper(BS->OpenProtocol, 6,
                             ImageHandle, &LoadedImageProtocol,
                             (void**)&LoadedImage, ImageHandle,
                             NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
  if (EFI_ERROR(Status)) {
    Print(L"Could not open loaded image protocol: %d\n", Status);
    return Status;
  }

  /* Attach our proto to the current driver image */
  Status = LibInstallProtocolInterfaces(
      &ImageHandle, &GnuEfiAppsDrv0ProtocolGuid,
      &InternalGnuEfiAppsDrv0ProtocolData.Proto, NULL);
  if (EFI_ERROR(Status)) {
    Print(L"Error registering driver instance: %d\n", Status);
    return Status;
  }

  /* Register Unload callback, used to unregister current protocol
   * instance from system */
  LoadedImage->Unload = (EFI_IMAGE_UNLOAD)Drv0Unload;

  Print(L"Driver instance loaded successfully.\n");
  return EFI_SUCCESS;  /* at this point, this instance stays resident
                        * until image is unloaded, eg. with shell's unload,
                        * ExitBootServices() */
}

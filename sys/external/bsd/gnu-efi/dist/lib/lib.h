/*	$NetBSD: lib.h,v 1.1.1.2 2018/08/16 18:17:47 jmcneill Exp $	*/

/*++

Copyright (c) 1998  Intel Corporation

Module Name:

    lib.h

Abstract:

    EFI library header files



Revision History

--*/

#ifdef __GNUC__
#pragma GCC visibility push(hidden)
#endif

#include <lib/libsa/stand.h>

#include "efi.h"
#include "efilib.h"
#include "efirtlib.h"

//
// Include non architectural protocols
//
#include "efivar.h"
#include "legacyboot.h"
#include "intload.h"
#include "vgaclass.h"
#include "eficonsplit.h"
#include "adapterdebug.h"
#include "intload.h"

#include "efigpt.h"
#include "libsmbios.h"

//
// Prototypes
//

VOID
InitializeGuid (
    VOID
    );

INTN EFIAPI
LibStubStriCmp (
    IN EFI_UNICODE_COLLATION_INTERFACE  *This,
    IN CHAR16                           *S1,
    IN CHAR16                           *S2
    );

BOOLEAN EFIAPI
LibStubMetaiMatch (
    IN EFI_UNICODE_COLLATION_INTERFACE  *This,
    IN CHAR16                           *String,
    IN CHAR16                           *Pattern
    );

VOID EFIAPI
LibStubStrLwrUpr (
    IN EFI_UNICODE_COLLATION_INTERFACE  *This,
    IN CHAR16                           *Str
    );

BOOLEAN
LibMatchDevicePaths (
    IN  EFI_DEVICE_PATH *Multi,
    IN  EFI_DEVICE_PATH *Single
    );

EFI_DEVICE_PATH *
LibDuplicateDevicePathInstance (
    IN EFI_DEVICE_PATH  *DevPath
    );


//
// Globals
//
extern BOOLEAN                          LibInitialized;
extern BOOLEAN                          LibFwInstance;
extern EFI_HANDLE			LibImageHandle;
extern SIMPLE_TEXT_OUTPUT_INTERFACE     *LibRuntimeDebugOut;
extern EFI_UNICODE_COLLATION_INTERFACE  *UnicodeInterface;
extern EFI_UNICODE_COLLATION_INTERFACE  LibStubUnicodeInterface;
extern EFI_RAISE_TPL                    LibRuntimeRaiseTPL;
extern EFI_RESTORE_TPL                  LibRuntimeRestoreTPL;

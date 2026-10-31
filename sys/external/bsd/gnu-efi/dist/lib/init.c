/*	$NetBSD: init.c,v 1.2 2018/08/16 18:25:45 jmcneill Exp $	*/

/*++

Copyright (c) 1998  Intel Corporation

Module Name:


Abstract:




Revision History

--*/

#include "lib.h"

VOID
EFIDebugVariable (
    VOID
    );

VOID
InitializeLib (
    IN EFI_HANDLE           ImageHandle,
    IN EFI_SYSTEM_TABLE     *SystemTable
    )
/*++

Routine Description:

    Initializes EFI library for use
    
Arguments:

    Firmware's EFI system table
    
Returns:

    None

--*/ 
{
    EFI_LOADED_IMAGE        *LoadedImage;
    EFI_STATUS              Status;
    CHAR8                   *LangCode;

    if (!LibInitialized) {
        LibInitialized = TRUE;
        LibFwInstance = FALSE;
        LibImageHandle = ImageHandle;


        //
        // Set up global pointer to the system table, boot services table,
        // and runtime services table
        //

        ST = SystemTable;
        BS = SystemTable->BootServices;
        RT = SystemTable->RuntimeServices;
//        ASSERT (CheckCrc(0, &ST->Hdr));
//        ASSERT (CheckCrc(0, &BS->Hdr));
//        ASSERT (CheckCrc(0, &RT->Hdr));


        //
        // Initialize pool allocation type
        //

        if (ImageHandle) {
            Status = uefi_call_wrapper(
                BS->HandleProtocol,
                3,
                ImageHandle, 
                &LoadedImageProtocol,
                (VOID*)&LoadedImage
            );

            if (!EFI_ERROR(Status)) {
                PoolAllocationType = LoadedImage->ImageDataType;
            }
            EFIDebugVariable ();
        }

        //
        // Initialize Guid table
        //

        InitializeGuid();

        InitializeLibPlatform(ImageHandle,SystemTable);
    }

    //
    // 
    //

    if (ImageHandle && UnicodeInterface == &LibStubUnicodeInterface) {
        LangCode = LibGetVariable (VarLanguage, &EfiGlobalVariable);
        InitializeUnicodeSupport (LangCode);
        if (LangCode) {
            FreePool (LangCode);
        }
    }
}

VOID
InitializeUnicodeSupport (
    CHAR8 *LangCode
    )
{
    EFI_UNICODE_COLLATION_INTERFACE *Ui;
    EFI_STATUS                      Status;
    CHAR8                           *Languages;
    UINTN                           Index, Position, Length;
    UINTN                           NoHandles;
    EFI_HANDLE                      *Handles;

    //
    // If we don't know it, lookup the current language code
    //

    LibLocateHandle (ByProtocol, &UnicodeCollationProtocol, NULL, &NoHandles, &Handles);
    if (!LangCode || !NoHandles) {
        goto Done;
    }

    //
    // Check all driver's for a matching language code
    //

    for (Index=0; Index < NoHandles; Index++) {
        Status = uefi_call_wrapper(BS->HandleProtocol, 3, Handles[Index], &UnicodeCollationProtocol, (VOID*)&Ui);
        if (EFI_ERROR(Status)) {
            continue;
        }

        //
        // Check for a matching language code
        //

        Languages = Ui->SupportedLanguages;
        Length = strlena(Languages);
        for (Position=0; Position < Length; Position += ISO_639_2_ENTRY_SIZE) {

            //
            // If this code matches, use this driver
            //

            if (CompareMem (Languages+Position, LangCode, ISO_639_2_ENTRY_SIZE) == 0) {
                UnicodeInterface = Ui;
                goto Done;
            }
        }
    }

Done:
    //
    // Cleanup
    //

    if (Handles) {
        FreePool (Handles);
    }
}

VOID
EFIDebugVariable (
    VOID
    )
{
    EFI_STATUS      Status;
    UINT32          Attributes;
    UINTN           DataSize;
    UINTN           NewEFIDebug;

    DataSize = sizeof(EFIDebug);
    Status = uefi_call_wrapper(RT->GetVariable, 5, L"EFIDebug", &EfiGlobalVariable, &Attributes, &DataSize, &NewEFIDebug);
    if (!EFI_ERROR(Status)) {
        EFIDebug = NewEFIDebug;
    }
}

#if !defined(__NetBSD__)
/*
 * Calls to memset/memcpy may be emitted implicitly by GCC or MSVC
 * even when -ffreestanding or /NODEFAULTLIB are in effect.
 */

#ifndef __SIZE_TYPE__
#define __SIZE_TYPE__ UINTN
#endif

void *memset(void *s, int c, __SIZE_TYPE__ n)
{
    unsigned char *p = s;

    while (n--)
        *p++ = c;

    return s;
}

void *memcpy(void *dest, const void *src, __SIZE_TYPE__ n)
{
    const unsigned char *q = src;
    unsigned char *p = dest;

    while (n--)
        *p++ = *q++;

    return dest;
}
#endif

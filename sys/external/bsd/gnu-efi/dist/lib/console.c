/*	$NetBSD: console.c,v 1.1.1.1 2014/04/01 16:16:06 jakllsch Exp $	*/

/*++

Copyright (c) 1998  Intel Corporation

Module Name:

    console.c

Abstract:




Revision History

--*/

#include "lib.h"



VOID
Output (
    IN CHAR16   *Str
    )
// Write a string to the console at the current cursor location
{
    uefi_call_wrapper(ST->ConOut->OutputString, 2, ST->ConOut, Str);
}


VOID
Input (
    IN CHAR16    *Prompt OPTIONAL,
    OUT CHAR16   *InStr,
    IN UINTN     StrLen
    )
// Input a string at the current cursor location, for StrLen
{
    IInput (
        ST->ConOut,
        ST->ConIn,
        Prompt,
        InStr,
        StrLen
        );
}

VOID
IInput (
    IN SIMPLE_TEXT_OUTPUT_INTERFACE     *ConOut,
    IN SIMPLE_INPUT_INTERFACE           *ConIn,
    IN CHAR16                           *Prompt OPTIONAL,
    OUT CHAR16                          *InStr,
    IN UINTN                            StrLen
    )
// Input a string at the current cursor location, for StrLen
{
    EFI_INPUT_KEY                   Key;
    EFI_STATUS                      Status;
    UINTN                           Len;

    if (Prompt) {
        ConOut->OutputString (ConOut, Prompt);
    }

    Len = 0;
    for (; ;) {
        WaitForSingleEvent (ConIn->WaitForKey, 0);

        Status = uefi_call_wrapper(ConIn->ReadKeyStroke, 2, ConIn, &Key);
        if (EFI_ERROR(Status)) {
            DEBUG((D_ERROR, "Input: error return from ReadKey %x\n", Status));
            break;
        }

        if (Key.UnicodeChar == '\n' ||
            Key.UnicodeChar == '\r') {
            break;
        }
        
        if (Key.UnicodeChar == '\b') {
            if (Len) {
                uefi_call_wrapper(ConOut->OutputString, 2, ConOut, L"\b \b");
                Len -= 1;
            }
            continue;
        }

        if (Key.UnicodeChar >= ' ') {
            if (Len < StrLen-1) {
                InStr[Len] = Key.UnicodeChar;

                InStr[Len+1] = 0;
                uefi_call_wrapper(ConOut->OutputString, 2, ConOut, &InStr[Len]);

                Len += 1;
            }
            continue;
        }
    }

    InStr[Len] = 0;
}

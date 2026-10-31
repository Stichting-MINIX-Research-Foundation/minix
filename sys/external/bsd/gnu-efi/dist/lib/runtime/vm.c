/*	$NetBSD: vm.c,v 1.1.1.1 2014/04/01 16:16:07 jakllsch Exp $	*/

/*++

Copyright (c) 1998  Intel Corporation

Module Name:

    vm.c

Abstract:

    EFI Hell to remap runtime address into the new virual address space 
    that was registered by the OS for RT calls.

    So the code image needs to be relocated. All pointers need to be 
    manually fixed up since the address map changes. 

    GOOD LUCK NOT HAVING BUGS IN YOUR CODE! PLEASE TEST A LOT. MAKE SURE
    EXIT BOOTSERVICES OVER WRITES ALL BOOTSERVICE MEMORY & DATA SPACES WHEN 
    YOU TEST.

Revision History

--*/

#include "lib.h"

#ifndef __GNUC__
#pragma RUNTIME_CODE(RtLibEnableVirtualMappings)
#endif
VOID
RUNTIMEFUNCTION
RtLibEnableVirtualMappings (
    VOID
    )
{
    EFI_CONVERT_POINTER     ConvertPointer;

    //
    // If this copy of the lib is linked into the firmware, then
    // do not update the pointers yet.
    //

    if (!LibFwInstance) {

        //
        // Different components are updating to the new virtual
        // mappings at differnt times.  The only function that
        // is safe to call at this notification is ConvertAddress
        //

        ConvertPointer = RT->ConvertPointer;

        //
        // Fix any pointers that the lib created, that may be needed
        // during runtime.
        //

        ConvertPointer (EFI_INTERNAL_PTR, (VOID **)&RT);
        ConvertPointer (EFI_OPTIONAL_PTR, (VOID **)&LibRuntimeDebugOut);

        ConvertPointer (EFI_INTERNAL_PTR, (VOID **)&LibRuntimeRaiseTPL);
        ConvertPointer (EFI_INTERNAL_PTR, (VOID **)&LibRuntimeRestoreTPL);

        // that was it :^)
    }
}


#ifndef __GNUC__
#pragma RUNTIME_CODE(RtConvertList)
#endif
VOID
RUNTIMEFUNCTION
RtConvertList (
    IN UINTN                DebugDisposition,
    IN OUT LIST_ENTRY       *ListHead
    )
{
    LIST_ENTRY              *Link;
    LIST_ENTRY              *NextLink;
    EFI_CONVERT_POINTER     ConvertPointer;

    ConvertPointer = RT->ConvertPointer;

    //
    // Convert all the Flink & Blink pointers in the list
    //

    Link = ListHead;
    do {
        NextLink = Link->Flink;

        ConvertPointer (
            Link->Flink == ListHead ? DebugDisposition : 0, 
            (VOID **)&Link->Flink
            );

        ConvertPointer (
            Link->Blink == ListHead ? DebugDisposition : 0, 
            (VOID **)&Link->Blink
            );

        Link = NextLink;
    } while (Link != ListHead);
}

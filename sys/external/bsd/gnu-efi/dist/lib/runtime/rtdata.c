/*	$NetBSD: rtdata.c,v 1.1.1.1 2014/04/01 16:16:07 jakllsch Exp $	*/

/*++

Copyright (c) 1998  Intel Corporation

Module Name:

    data.c

Abstract:

    EFI library global data



Revision History

--*/

#include "lib.h"


//
// These globals are runtime globals
//
// N.B. The Microsoft C compiler will only put the data in the
// right data section if it is explicitly initialized..
//

#ifndef __GNUC__
#pragma BEGIN_RUNTIME_DATA()
#endif

//
// RT - pointer to the runtime table
//

EFI_RUNTIME_SERVICES    *RT;

//
// LibStandalone - TRUE if lib is linked in as part of the firmware.
// N.B. The EFI fw sets this value directly
//

BOOLEAN  LibFwInstance;

//
// EFIDebug - Debug mask
//

UINTN    EFIDebug    = EFI_DBUG_MASK;

//
// LibRuntimeDebugOut - Runtime Debug Output device
//

SIMPLE_TEXT_OUTPUT_INTERFACE    *LibRuntimeDebugOut;

//
// LibRuntimeRaiseTPL, LibRuntimeRestoreTPL - pointers to Runtime functions from the 
//                                            Boot Services Table
//

EFI_RAISE_TPL   LibRuntimeRaiseTPL   = NULL;
EFI_RESTORE_TPL LibRuntimeRestoreTPL = NULL;


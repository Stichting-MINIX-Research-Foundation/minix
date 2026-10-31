/*	$NetBSD: rtlock.c,v 1.1.1.1 2014/04/01 16:16:07 jakllsch Exp $	*/

/*++

Copyright (c) 1998  Intel Corporation

Module Name:

    lock.c

Abstract:

    Implements FLOCK



Revision History

--*/


#include "lib.h"



#ifndef __GNUC__
#pragma RUNTIME_CODE(RtAcquireLock)
#endif
VOID
RtAcquireLock (
    IN FLOCK    *Lock
    )
/*++

Routine Description:

    Raising to the task priority level of the mutual exclusion
    lock, and then acquires ownership of the lock.
    
Arguments:

    Lock        - The lock to acquire
    
Returns:

    Lock owned

--*/
{
    if (BS) {
        if (BS->RaiseTPL != NULL) {
            Lock->OwnerTpl = uefi_call_wrapper(BS->RaiseTPL, 1, Lock->Tpl);
        } 
    }
    else {
        if (LibRuntimeRaiseTPL != NULL) {
            Lock->OwnerTpl = LibRuntimeRaiseTPL(Lock->Tpl);
        }
    }
    Lock->Lock += 1;
    ASSERT (Lock->Lock == 1);
}


#ifndef __GNUC__
#pragma RUNTIME_CODE(RtAcquireLock)
#endif
VOID
RtReleaseLock (
    IN FLOCK    *Lock
    )
/*++

Routine Description:

    Releases ownership of the mutual exclusion lock, and
    restores the previous task priority level.
    
Arguments:

    Lock        - The lock to release
    
Returns:

    Lock unowned

--*/
{
    EFI_TPL     Tpl;

    Tpl = Lock->OwnerTpl;
    ASSERT(Lock->Lock == 1);
    Lock->Lock -= 1;
    if (BS) {
        if (BS->RestoreTPL != NULL) {
            uefi_call_wrapper(BS->RestoreTPL, 1, Tpl);
        } 
    }
    else {
        if (LibRuntimeRestoreTPL != NULL) {
            LibRuntimeRestoreTPL(Tpl);
        }
    }
}

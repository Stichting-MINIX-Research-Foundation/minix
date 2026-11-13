/*	$NetBSD: lock.c,v 1.1.1.1 2014/04/01 16:16:06 jakllsch Exp $	*/

/*++

Copyright (c) 1998  Intel Corporation

Module Name:

    lock.c

Abstract:

    Implements FLOCK



Revision History

--*/


#include "lib.h"


VOID
InitializeLock (
    IN OUT FLOCK    *Lock,
    IN EFI_TPL      Priority
    )
/*++

Routine Description:

    Initialize a basic mutual exclusion lock.   Each lock
    provides mutual exclusion access at it's task priority
    level.  Since there is no-premption (at any TPL) or
    multiprocessor support, acquiring the lock only consists
    of raising to the locks TPL.

    Note on a debug build the lock is acquired and released
    to help ensure proper usage.
    
Arguments:

    Lock        - The FLOCK structure to initialize

    Priority    - The task priority level of the lock

    
Returns:

    An initialized F Lock structure.

--*/
{
    Lock->Tpl = Priority;
    Lock->OwnerTpl = 0;
    Lock->Lock = 0;
}


VOID
AcquireLock (
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
    RtAcquireLock (Lock);
}


VOID
ReleaseLock (
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
    RtReleaseLock (Lock);
}

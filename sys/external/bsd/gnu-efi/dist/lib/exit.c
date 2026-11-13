/*	$NetBSD: exit.c,v 1.1.1.1 2018/08/16 18:17:47 jmcneill Exp $	*/

#include "lib.h"

VOID
Exit(
    IN EFI_STATUS   ExitStatus,
    IN UINTN        ExitDataSize,
    IN CHAR16       *ExitData OPTIONAL
    )
{
    uefi_call_wrapper(BS->Exit,
            4,
            LibImageHandle,
            ExitStatus,
            ExitDataSize,
            ExitData);

    // Uh oh, Exit() returned?!
    for (;;) { }
}

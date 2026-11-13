/*	$NetBSD: event.c,v 1.3 2018/08/16 18:22:05 jmcneill Exp $	*/

/*++

Copyright (c) 1998  Intel Corporation

Module Name:

    event.c

Abstract:




Revision History

--*/

#include "lib.h"


EFI_EVENT
LibCreateProtocolNotifyEvent (
    IN EFI_GUID             *ProtocolGuid,
    IN EFI_TPL              NotifyTpl,
    IN EFI_EVENT_NOTIFY     NotifyFunction,
    IN VOID                 *NotifyContext,
    OUT VOID                *Registration
    )
{
#ifdef EFI_DEBUG
    EFI_STATUS              Status;
#else
    EFI_STATUS              Status __unused;
#endif
    EFI_EVENT               Event;

    //
    // Create the event
    //

    Status = uefi_call_wrapper(
		    BS->CreateEvent,
			5,
		    EVT_NOTIFY_SIGNAL,
		    NotifyTpl,
		    NotifyFunction,
		    NotifyContext,
		    &Event
		    );
    if ( EFI_ERROR( Status ) ) return NULL ;
    ASSERT (!EFI_ERROR(Status));

    //
    // Register for protocol notifactions on this event
    //

    Status = uefi_call_wrapper(
		    BS->RegisterProtocolNotify,
			3,
                    ProtocolGuid,
                    Event,
                    Registration
                    );
    if ( EFI_ERROR( Status ) ) return NULL ;
    ASSERT (!EFI_ERROR(Status));

    //
    // Kick the event so we will perform an initial pass of
    // current installed drivers
    //

    uefi_call_wrapper(BS->SignalEvent, 1, Event);
    return Event;
}


EFI_STATUS
WaitForSingleEvent (
    IN EFI_EVENT        Event,
    IN UINT64           Timeout OPTIONAL
    )
{
    EFI_STATUS          Status;
    UINTN               Index;
    EFI_EVENT           TimerEvent;
    EFI_EVENT           WaitList[2];

    if (Timeout) {
        //
        // Create a timer event
        //

        Status = uefi_call_wrapper(BS->CreateEvent, 5, EVT_TIMER, 0, NULL, NULL, &TimerEvent);
        if (!EFI_ERROR(Status)) {

            //
            // Set the timer event
            //

            uefi_call_wrapper(BS->SetTimer, 3, TimerEvent, TimerRelative, Timeout);

            //
            // Wait for the original event or the timer
            //

            WaitList[0] = Event;
            WaitList[1] = TimerEvent;
            Status = uefi_call_wrapper(BS->WaitForEvent, 3, 2, WaitList, &Index);
            uefi_call_wrapper(BS->CloseEvent, 1, TimerEvent);

            //
            // If the timer expired, change the return to timed out
            //

            if (!EFI_ERROR(Status)  &&  Index == 1) {
                Status = EFI_TIMEOUT;
            }
        }

    } else {

        //
        // No timeout... just wait on the event
        //

        Status = uefi_call_wrapper(BS->WaitForEvent, 3, 1, &Event, &Index);
        ASSERT (!EFI_ERROR(Status));
        ASSERT (Index == 0);
    }

    return Status;
}

VOID
WaitForEventWithTimeout (
    IN  EFI_EVENT       Event,
    IN  UINTN           Timeout,
    IN  UINTN           Row,
    IN  UINTN           Column,
    IN  CHAR16          *String,
    IN  EFI_INPUT_KEY   TimeoutKey,
    OUT EFI_INPUT_KEY   *Key
    )
{
    EFI_STATUS      Status;

    do {
        PrintAt (Column, Row, String, Timeout);
        Status = WaitForSingleEvent (Event, 10000000);
        if (Status == EFI_SUCCESS) {
            if (!EFI_ERROR(uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, Key))) {
                return;
            }
        }
    } while (Timeout > 0);
    CopyMem(Key, &TimeoutKey, sizeof(EFI_INPUT_KEY));
}


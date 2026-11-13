/*	$NetBSD: hand.c,v 1.4 2018/08/16 18:22:05 jmcneill Exp $	*/

/*++

Copyright (c) 1998  Intel Corporation

Module Name:

    hand.c

Abstract:




Revision History

--*/

#include "lib.h"
#include "efistdarg.h"                        // !!!


EFI_STATUS
LibLocateProtocol (
    IN  EFI_GUID    *ProtocolGuid,
    OUT VOID        **Interface
    )
//
// Find the first instance of this Protocol in the system and return it's interface
//
{
    EFI_STATUS      Status;
    UINTN           NumberHandles, Index;
    EFI_HANDLE      *Handles;

    
    *Interface = NULL;
    Status = LibLocateHandle (ByProtocol, ProtocolGuid, NULL, &NumberHandles, &Handles);
    if (EFI_ERROR(Status)) {
        DEBUG((D_INFO, "LibLocateProtocol: Handle not found\n"));
        return Status;
    }

    for (Index=0; Index < NumberHandles; Index++) {
        Status = uefi_call_wrapper(BS->HandleProtocol, 3, Handles[Index], ProtocolGuid, Interface);
        if (!EFI_ERROR(Status)) {
            break;
        }
    }

    if (Handles) {
        FreePool (Handles);
    }

    return Status;
}

EFI_STATUS
LibLocateHandle (
    IN EFI_LOCATE_SEARCH_TYPE       SearchType,
    IN EFI_GUID                     *Protocol OPTIONAL,
    IN VOID                         *SearchKey OPTIONAL,
    IN OUT UINTN                    *NoHandles,
    OUT EFI_HANDLE                  **Buffer
    )

{
    EFI_STATUS          Status;
    UINTN               BufferSize;

    //
    // Initialize for GrowBuffer loop
    //

    Status = EFI_SUCCESS;
    *Buffer = NULL;
    BufferSize = 50 * sizeof(EFI_HANDLE);

    //
    // Call the real function
    //

    while (GrowBuffer (&Status, (VOID **) Buffer, BufferSize)) {

        Status = uefi_call_wrapper(
			BS->LocateHandle,
			5,
                        SearchType,
                        Protocol,
                        SearchKey,
                        &BufferSize,
                        *Buffer
                        );

    }

    *NoHandles = BufferSize / sizeof (EFI_HANDLE);
    if (EFI_ERROR(Status)) {
        *NoHandles = 0;
    }

    return Status;
}

EFI_STATUS
LibLocateHandleByDiskSignature (
    IN UINT8                        MBRType,
    IN UINT8                        SignatureType,
    IN VOID                         *Signature,
    IN OUT UINTN                    *NoHandles,
    OUT EFI_HANDLE                  **Buffer
    )

{
    EFI_STATUS            Status;
    UINTN                 BufferSize;
    UINTN                 NoBlockIoHandles;
    EFI_HANDLE            *BlockIoBuffer;
    EFI_DEVICE_PATH       *DevicePath;
    UINTN                 Index;
    EFI_DEVICE_PATH       *Next, *DevPath;
    HARDDRIVE_DEVICE_PATH *HardDriveDevicePath;
    BOOLEAN               Match;
    BOOLEAN               PreviousNodeIsHardDriveDevicePath;

    //
    // Initialize for GrowBuffer loop
    //

    Status = EFI_SUCCESS;
    BlockIoBuffer = NULL;
    BufferSize = 50 * sizeof(EFI_HANDLE);

    //
    // Call the real function
    //

    while (GrowBuffer (&Status, (VOID **)&BlockIoBuffer, BufferSize)) {

        //
        // Get list of device handles that support the BLOCK_IO Protocol.
        //

        Status = uefi_call_wrapper(
			BS->LocateHandle,
			5,
                        ByProtocol,
                        &BlockIoProtocol,
                        NULL,
                        &BufferSize,
                        BlockIoBuffer
                        );

    }

    NoBlockIoHandles = BufferSize / sizeof (EFI_HANDLE);
    if (EFI_ERROR(Status)) {
        NoBlockIoHandles = 0;
    }

    //
    // If there was an error or there are no device handles that support 
    // the BLOCK_IO Protocol, then return.
    //

    if (NoBlockIoHandles == 0) {
        FreePool(BlockIoBuffer);
        *NoHandles = 0;
        *Buffer = NULL;
        return Status;
    }

    //
    // Loop through all the device handles that support the BLOCK_IO Protocol
    //

    *NoHandles = 0;

    for(Index=0;Index<NoBlockIoHandles;Index++) {

        Status = uefi_call_wrapper(
				     BS->HandleProtocol, 
					3,
				     BlockIoBuffer[Index], 
                                     &DevicePathProtocol, 
                                     (VOID*)&DevicePath
                                     );

        //
        // Search DevicePath for a Hard Drive Media Device Path node.
        // If one is found, then see if it matches the signature that was
        // passed in.  If it does match, and the next node is the End of the
        // device path, and the previous node is not a Hard Drive Media Device
        // Path, then we have found a match.
        //

        Match = FALSE;

        if (DevicePath != NULL) {

            PreviousNodeIsHardDriveDevicePath = FALSE;

            DevPath = DevicePath;

            //
            // Check for end of device path type
            //    

            for (; ;) {

                if ((DevicePathType(DevPath) == MEDIA_DEVICE_PATH) &&
                    (DevicePathSubType(DevPath) == MEDIA_HARDDRIVE_DP)) {

                    HardDriveDevicePath = (HARDDRIVE_DEVICE_PATH *)(DevPath);

                    if (PreviousNodeIsHardDriveDevicePath == FALSE) {

                        Next = NextDevicePathNode(DevPath);
                        if (IsDevicePathEndType(Next)) {
                            if ((HardDriveDevicePath->MBRType == MBRType) &&
                                (HardDriveDevicePath->SignatureType == SignatureType)) {
                                    switch(SignatureType) {
                                        case SIGNATURE_TYPE_MBR:
                                            if (*((UINT32 *)(Signature)) == *(UINT32 *)(&(HardDriveDevicePath->Signature[0]))) {
                                                Match = TRUE;
                                            }
                                            break;
                                        case SIGNATURE_TYPE_GUID:
                                            if (CompareGuid((EFI_GUID *)Signature,(EFI_GUID *)(&(HardDriveDevicePath->Signature[0]))) == 0) {
                                                Match = TRUE;
                                            }
                                            break;
                                    }
                            }
                        }
                    }
                    PreviousNodeIsHardDriveDevicePath = TRUE;
                } else {
                    PreviousNodeIsHardDriveDevicePath = FALSE;
                }

                if (IsDevicePathEnd(DevPath)) {
                    break;
                }

                DevPath = NextDevicePathNode(DevPath);
            }

        }

        if (Match == FALSE) {
            BlockIoBuffer[Index] = NULL;
        } else {
            *NoHandles = *NoHandles + 1;
        }
    }

    //
    // If there are no matches, then return
    //

    if (*NoHandles == 0) {
        FreePool(BlockIoBuffer);
        *NoHandles = 0;
        *Buffer = NULL;
        return EFI_SUCCESS;
    }

    //
    // Allocate space for the return buffer of device handles.
    //

    *Buffer = AllocatePool(*NoHandles * sizeof(EFI_HANDLE));

    if (*Buffer == NULL) {
        FreePool(BlockIoBuffer);
        *NoHandles = 0;
        *Buffer = NULL;
        return EFI_OUT_OF_RESOURCES;
    }

    //
    // Build list of matching device handles.
    //

    *NoHandles = 0;
    for(Index=0;Index<NoBlockIoHandles;Index++) {
        if (BlockIoBuffer[Index] != NULL) {
            (*Buffer)[*NoHandles] = BlockIoBuffer[Index];
            *NoHandles = *NoHandles + 1;
        }
    }

    FreePool(BlockIoBuffer);

    return EFI_SUCCESS;
}

EFI_FILE_HANDLE
LibOpenRoot (
    IN EFI_HANDLE               DeviceHandle
    )
{
    EFI_STATUS                  Status;
    EFI_FILE_IO_INTERFACE       *Volume;
    EFI_FILE_HANDLE             File;


    //
    // File the file system interface to the device
    //

    Status = uefi_call_wrapper(BS->HandleProtocol, 3, DeviceHandle, &FileSystemProtocol, (VOID*)&Volume);

    //
    // Open the root directory of the volume 
    //

    if (!EFI_ERROR(Status)) {
        Status = uefi_call_wrapper(Volume->OpenVolume, 2, Volume, &File);
    }

    //
    // Done
    //

    return EFI_ERROR(Status) ? NULL : File;
}

EFI_FILE_INFO *
LibFileInfo (
    IN EFI_FILE_HANDLE      FHand
    )
{
    EFI_STATUS              Status;
    EFI_FILE_INFO           *Buffer;
    UINTN                   BufferSize;

    //
    // Initialize for GrowBuffer loop
    //

    Status = EFI_SUCCESS;
    Buffer = NULL;
    BufferSize = SIZE_OF_EFI_FILE_INFO + 200;

    //
    // Call the real function
    //

    while (GrowBuffer (&Status, (VOID **) &Buffer, BufferSize)) {
        Status = uefi_call_wrapper(
		    FHand->GetInfo,
			4,
                    FHand,
                    &GenericFileInfo,
                    &BufferSize,
                    Buffer
                    );
    }

    return Buffer;
}

    
EFI_FILE_SYSTEM_INFO *
LibFileSystemInfo (
    IN EFI_FILE_HANDLE      FHand
    )
{
    EFI_STATUS              Status;
    EFI_FILE_SYSTEM_INFO    *Buffer;
    UINTN                   BufferSize;

    //
    // Initialize for GrowBuffer loop
    //

    Status = EFI_SUCCESS;
    Buffer = NULL;
    BufferSize = SIZE_OF_EFI_FILE_SYSTEM_INFO + 200;

    //
    // Call the real function
    //

    while (GrowBuffer (&Status, (VOID **) &Buffer, BufferSize)) {
        Status = uefi_call_wrapper(
		    FHand->GetInfo,
			4,
                    FHand,
                    &FileSystemInfo,
                    &BufferSize,
                    Buffer
                    );
    }

    return Buffer;
}

EFI_FILE_SYSTEM_VOLUME_LABEL_INFO *
LibFileSystemVolumeLabelInfo (
    IN EFI_FILE_HANDLE      FHand
    )
{
    EFI_STATUS                        Status;
    EFI_FILE_SYSTEM_VOLUME_LABEL_INFO *Buffer;
    UINTN                             BufferSize;

    //
    // Initialize for GrowBuffer loop
    //

    Status = EFI_SUCCESS;
    Buffer = NULL;
    BufferSize = SIZE_OF_EFI_FILE_SYSTEM_VOLUME_LABEL_INFO + 200;

    //
    // Call the real function
    //

    while (GrowBuffer (&Status, (VOID **) &Buffer, BufferSize)) {
        Status = uefi_call_wrapper(
		    FHand->GetInfo,
			4,
                    FHand,
                    &FileSystemVolumeLabelInfo,
                    &BufferSize,
                    Buffer
                    );
    }

    return Buffer;
}

    

EFI_STATUS
LibInstallProtocolInterfaces (
    IN OUT EFI_HANDLE           *Handle,
    ...
    )
{
    va_list         args;
    EFI_STATUS      Status;
    EFI_GUID        *Protocol;
    VOID            *Interface;
    EFI_TPL         OldTpl;
    UINTN           Index;
    EFI_HANDLE      OldHandle;

    //
    // Syncronize with notifcations
    // 

    OldTpl = uefi_call_wrapper(BS->RaiseTPL, 1, TPL_NOTIFY);
    OldHandle = *Handle;

    //
    // Install the protocol interfaces
    //

    Index = 0;
    Status = EFI_SUCCESS;
    va_start (args, Handle);

    while (!EFI_ERROR(Status)) {

        //
        // If protocol is NULL, then it's the end of the list
        //

        Protocol = va_arg(args, EFI_GUID *);
        if (!Protocol) {
            break;
        }

        Interface = va_arg(args, VOID *);

        //
        // Install it
        //

        DEBUG((D_INFO, "LibInstallProtocolInterface: %d %x\n", Protocol, Interface));
        Status = uefi_call_wrapper(BS->InstallProtocolInterface, 4, Handle, Protocol, EFI_NATIVE_INTERFACE, Interface);
        if (EFI_ERROR(Status)) {
            break;
        }

        Index += 1;
    }

    va_end (args);

    //
    // If there was an error, remove all the interfaces that were
    // installed without any errors
    //

    if (EFI_ERROR(Status)) {
        va_start (args, Handle);
        while (Index) {

            Protocol = va_arg(args, EFI_GUID *);
            Interface = va_arg(args, VOID *);
            uefi_call_wrapper(BS->UninstallProtocolInterface, 3, *Handle, Protocol, Interface);

            Index -= 1;
        }        
	va_end (args);

        *Handle = OldHandle;
    }

    //
    // Done
    //

    uefi_call_wrapper(BS->RestoreTPL, 1, OldTpl);
    return Status;
}


VOID
LibUninstallProtocolInterfaces (
    IN EFI_HANDLE           Handle,
    ...
    )
{
    va_list         args;
    EFI_STATUS      Status;
    EFI_GUID        *Protocol;
    VOID            *Interface;

    
    va_start (args, Handle);
    for (; ;) {

        //
        // If protocol is NULL, then it's the end of the list
        //

        Protocol = va_arg(args, EFI_GUID *);
        if (!Protocol) {
            break;
        }

        Interface = va_arg(args, VOID *);

        //
        // Uninstall it
        //

        Status = uefi_call_wrapper(BS->UninstallProtocolInterface, 3, Handle, Protocol, Interface);
        if (EFI_ERROR(Status)) {
            DEBUG((D_ERROR, "LibUninstallProtocolInterfaces: failed %g, %r\n", Protocol, Handle));
        }
    }
    va_end (args);
}    


EFI_STATUS
LibReinstallProtocolInterfaces (
    IN OUT EFI_HANDLE           *Handle,
    ...
    )
{
    va_list         args;
    EFI_STATUS      Status;
    EFI_GUID        *Protocol;
    VOID            *OldInterface, *NewInterface;
    EFI_TPL         OldTpl;
    UINTN           Index;

    //
    // Syncronize with notifcations
    // 

    OldTpl = uefi_call_wrapper(BS->RaiseTPL, 1, TPL_NOTIFY);

    //
    // Install the protocol interfaces
    //

    Index = 0;
    Status = EFI_SUCCESS;
    va_start (args, Handle);

    while (!EFI_ERROR(Status)) {

        //
        // If protocol is NULL, then it's the end of the list
        //

        Protocol = va_arg(args, EFI_GUID *);
        if (!Protocol) {
            break;
        }

        OldInterface = va_arg(args, VOID *);
        NewInterface = va_arg(args, VOID *);

        //
        // Reinstall it
        //

        Status = uefi_call_wrapper(BS->ReinstallProtocolInterface, 4, Handle, Protocol, OldInterface, NewInterface);
        if (EFI_ERROR(Status)) {
            break;
        }

        Index += 1;
    }

    va_end (args);

    //
    // If there was an error, undo all the interfaces that were
    // reinstalled without any errors
    //

    if (EFI_ERROR(Status)) {
        va_start (args, Handle);
        while (Index) {

            Protocol = va_arg(args, EFI_GUID *);
            OldInterface = va_arg(args, VOID *);
            NewInterface = va_arg(args, VOID *);

            uefi_call_wrapper(BS->ReinstallProtocolInterface, 4, Handle, Protocol, NewInterface, OldInterface);

            Index -= 1;
        }        
	va_end (args);
    }

    //
    // Done
    //

    uefi_call_wrapper(BS->RestoreTPL, 1, OldTpl);
    return Status;
}

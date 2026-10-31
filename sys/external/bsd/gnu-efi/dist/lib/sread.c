/*	$NetBSD: sread.c,v 1.1.1.1 2014/04/01 16:16:07 jakllsch Exp $	*/

/*++

Copyright (c) 1998  Intel Corporation

Module Name:
    
    sread.c

Abstract:

    Simple read file access



Revision History

--*/

#include "lib.h"

#define SIMPLE_READ_SIGNATURE       EFI_SIGNATURE_32('s','r','d','r')
typedef struct _SIMPLE_READ_FILE {
    UINTN               Signature;
    BOOLEAN             FreeBuffer;
    VOID                *Source;
    UINTN               SourceSize;
    EFI_FILE_HANDLE     FileHandle;
} SIMPLE_READ_HANDLE;

       

EFI_STATUS
OpenSimpleReadFile (
    IN BOOLEAN                  BootPolicy,
    IN VOID                     *SourceBuffer   OPTIONAL,
    IN UINTN                    SourceSize,
    IN OUT EFI_DEVICE_PATH      **FilePath,
    OUT EFI_HANDLE              *DeviceHandle,
    OUT SIMPLE_READ_FILE        *SimpleReadHandle
    )
/*++

Routine Description:

    Opens a file for (simple) reading.  The simple read abstraction
    will access the file either from a memory copy, from a file
    system interface, or from the load file interface. 

Arguments:

Returns:

    A handle to access the file

--*/
{
    SIMPLE_READ_HANDLE          *FHand;
    EFI_DEVICE_PATH             *UserFilePath;
    EFI_DEVICE_PATH             *TempFilePath;
    EFI_DEVICE_PATH             *TempFilePathPtr;
    FILEPATH_DEVICE_PATH        *FilePathNode;
    EFI_FILE_HANDLE             FileHandle, LastHandle;
    EFI_STATUS                  Status;
    EFI_LOAD_FILE_INTERFACE     *LoadFile;
  
    FHand = NULL;
    UserFilePath = *FilePath;

    //
    // Allocate a new simple read handle structure
    //

    FHand = AllocateZeroPool (sizeof(SIMPLE_READ_HANDLE));
    if (!FHand) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Done;
    }

    *SimpleReadHandle = (SIMPLE_READ_FILE) FHand;
    FHand->Signature = SIMPLE_READ_SIGNATURE;

    //
    // If the caller passed a copy of the file, then just use it
    //

    if (SourceBuffer) {
        FHand->Source = SourceBuffer;
        FHand->SourceSize = SourceSize;
        *DeviceHandle = NULL;
        Status = EFI_SUCCESS;
        goto Done;
    } 

    //
    // Attempt to access the file via a file system interface
    //

    FileHandle = NULL;
    Status = uefi_call_wrapper(BS->LocateDevicePath, 3, &FileSystemProtocol, FilePath, DeviceHandle);
    if (!EFI_ERROR(Status)) {
        FileHandle = LibOpenRoot (*DeviceHandle);
    }

    Status = FileHandle ? EFI_SUCCESS : EFI_UNSUPPORTED;

    //
    // To access as a filesystem, the filepath should only
    // contain filepath components.  Follow the filepath nodes
    // and find the target file
    //

    FilePathNode = (FILEPATH_DEVICE_PATH *) *FilePath;
    while (!IsDevicePathEnd(&FilePathNode->Header)) {

        //
        // For filesystem access each node should be a filepath component
        //

        if (DevicePathType(&FilePathNode->Header) != MEDIA_DEVICE_PATH ||
            DevicePathSubType(&FilePathNode->Header) != MEDIA_FILEPATH_DP) {
            Status = EFI_UNSUPPORTED;
        }

        //
        // If there's been an error, stop
        //

        if (EFI_ERROR(Status)) {
            break;
        }
        
        //
        // Open this file path node
        //

        LastHandle = FileHandle;
        FileHandle = NULL;

        Status = uefi_call_wrapper(
			LastHandle->Open,
			5, 
                        LastHandle,
                        &FileHandle,
                        FilePathNode->PathName,
                        EFI_FILE_MODE_READ,
                        0
                        );
        
        //
        // Close the last node
        //
        
        uefi_call_wrapper(LastHandle->Close, 1, LastHandle);

        //
        // Get the next node
        //

        FilePathNode = (FILEPATH_DEVICE_PATH *) NextDevicePathNode(&FilePathNode->Header);
    }

    //
    // If success, return the FHand
    //

    if (!EFI_ERROR(Status)) {
        ASSERT(FileHandle);
        FHand->FileHandle = FileHandle;
        goto Done;
    }

    //
    // Cleanup from filesystem access
    //

    if (FileHandle) {
        uefi_call_wrapper(FileHandle->Close, 1, FileHandle);
        FileHandle = NULL;
        *FilePath = UserFilePath;
    }

    //
    // If the error is something other then unsupported, return it
    //

    if (Status != EFI_UNSUPPORTED) {
        goto Done;
    }

    //
    // Attempt to access the file via the load file protocol
    //

    Status = LibDevicePathToInterface (&LoadFileProtocol, *FilePath, (VOID*)&LoadFile);
    if (!EFI_ERROR(Status)) {

        TempFilePath = DuplicateDevicePath (*FilePath);

        TempFilePathPtr = TempFilePath;

        Status = uefi_call_wrapper(BS->LocateDevicePath, 3, &LoadFileProtocol, &TempFilePath, DeviceHandle);

        FreePool (TempFilePathPtr);

        //
        // Determine the size of buffer needed to hold the file
        //

        SourceSize = 0;
        Status = uefi_call_wrapper(
		    LoadFile->LoadFile,
			5,
                    LoadFile,
                    *FilePath,
                    BootPolicy,
                    &SourceSize,
                    NULL
                    );

        //
        // We expect a buffer too small error to inform us 
        // of the buffer size needed
        //

        if (Status == EFI_BUFFER_TOO_SMALL) {
            SourceBuffer = AllocatePool (SourceSize);
            
            if (SourceBuffer) {
                FHand->FreeBuffer = TRUE;
                FHand->Source = SourceBuffer;
                FHand->SourceSize = SourceSize;

                Status = uefi_call_wrapper(
			    LoadFile->LoadFile,
				5,
                            LoadFile,
                            *FilePath,
                            BootPolicy,
                            &SourceSize,
                            SourceBuffer
                            );  
            }
        }

        //
        // If success, return FHand
        //

        if (!EFI_ERROR(Status) || Status == EFI_ALREADY_STARTED) {
            goto Done;
        }
    }

    //
    // Nothing else to try
    //

    DEBUG ((D_LOAD|D_WARN, "OpenSimpleReadFile: Device did not support a known load protocol\n"));
    Status = EFI_UNSUPPORTED;

Done:

    //
    // If the file was not accessed, clean up
    //
    if (EFI_ERROR(Status) && (Status != EFI_ALREADY_STARTED)) {
        if (FHand) {
            if (FHand->FreeBuffer) {
                FreePool (FHand->Source);
            }

            FreePool (FHand);
        }
    }

    return Status;
}

EFI_STATUS
ReadSimpleReadFile (
    IN SIMPLE_READ_FILE     UserHandle,
    IN UINTN                Offset,
    IN OUT UINTN            *ReadSize,
    OUT VOID                *Buffer
    )
{
    UINTN                   EndPos;
    SIMPLE_READ_HANDLE      *FHand;
    EFI_STATUS              Status;

    FHand = UserHandle;
    ASSERT (FHand->Signature == SIMPLE_READ_SIGNATURE);
    if (FHand->Source) {

        //
        // Move data from our local copy of the file
        //

        EndPos = Offset + *ReadSize;
        if (EndPos > FHand->SourceSize) {
            *ReadSize = FHand->SourceSize - Offset;
            if (Offset >= FHand->SourceSize) {
                *ReadSize = 0;
            }
        }

        CopyMem (Buffer, (CHAR8 *) FHand->Source + Offset, *ReadSize);
        Status = EFI_SUCCESS;

    } else {

        //
        // Read data from the file
        //

        Status = uefi_call_wrapper(FHand->FileHandle->SetPosition, 2, FHand->FileHandle, Offset);

        if (!EFI_ERROR(Status)) {
            Status = uefi_call_wrapper(FHand->FileHandle->Read, 3, FHand->FileHandle, ReadSize, Buffer);
        }
    }

    return Status;
}


VOID
CloseSimpleReadFile (
    IN SIMPLE_READ_FILE     UserHandle
    )
{
    SIMPLE_READ_HANDLE      *FHand;

    FHand = UserHandle;
    ASSERT (FHand->Signature == SIMPLE_READ_SIGNATURE);

    //
    // Free any file handle we opened
    //

    if (FHand->FileHandle) {
        uefi_call_wrapper(FHand->FileHandle->Close, 1, FHand->FileHandle);
    }

    //
    // If we allocated the Source buffer, free it
    //

    if (FHand->FreeBuffer) {
        FreePool (FHand->Source);
    }

    //
    // Done with this simple read file handle
    //

    FreePool (FHand);
}

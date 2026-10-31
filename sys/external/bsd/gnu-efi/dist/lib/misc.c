/*	$NetBSD: misc.c,v 1.1.1.2 2018/08/16 18:17:47 jmcneill Exp $	*/

/*++

Copyright (c) 1998  Intel Corporation

Module Name:

    misc.c

Abstract:




Revision History

--*/

#include "lib.h"


//
//
//

VOID *
AllocatePool (
    IN UINTN                Size
    )
{
    EFI_STATUS              Status;
    VOID                    *p;

    Status = uefi_call_wrapper(BS->AllocatePool, 3, PoolAllocationType, Size, &p);
    if (EFI_ERROR(Status)) {
        DEBUG((D_ERROR, "AllocatePool: out of pool  %x\n", Status));
        p = NULL;
    }
    return p;
}

VOID *
AllocateZeroPool (
    IN UINTN                Size
    )
{
    VOID                    *p;

    p = AllocatePool (Size);
    if (p) {
        ZeroMem (p, Size);
    }

    return p;
}

VOID *
ReallocatePool (
    IN VOID                 *OldPool,
    IN UINTN                OldSize,
    IN UINTN                NewSize
    )
{
    VOID                    *NewPool;

    NewPool = NULL;
    if (NewSize) {
        NewPool = AllocatePool (NewSize);
    }

    if (OldPool) {
        if (NewPool) {
            CopyMem (NewPool, OldPool, OldSize < NewSize ? OldSize : NewSize);
        }
    
        FreePool (OldPool);
    }
    
    return NewPool;
}


VOID
FreePool (
    IN VOID                 *Buffer
    )
{
    uefi_call_wrapper(BS->FreePool, 1, Buffer);
}



VOID
ZeroMem (
    IN VOID     *Buffer,
    IN UINTN    Size
    )
{
    RtZeroMem (Buffer, Size);
}

VOID
SetMem (
    IN VOID     *Buffer,
    IN UINTN    Size,
    IN UINT8    Value    
    )
{
    RtSetMem (Buffer, Size, Value);
}

VOID
CopyMem (
    IN VOID     *Dest,
    IN CONST VOID     *Src,
    IN UINTN    len
    )
{
    RtCopyMem (Dest, Src, len);
}

INTN
CompareMem (
    IN CONST VOID     *Dest,
    IN CONST VOID     *Src,
    IN UINTN    len
    )
{
    return RtCompareMem (Dest, Src, len);
}

BOOLEAN
GrowBuffer(
    IN OUT EFI_STATUS   *Status,
    IN OUT VOID         **Buffer,
    IN UINTN            BufferSize
    )
/*++

Routine Description:

    Helper function called as part of the code needed
    to allocate the proper sized buffer for various 
    EFI interfaces.

Arguments:

    Status      - Current status

    Buffer      - Current allocated buffer, or NULL

    BufferSize  - Current buffer size needed
    
Returns:
    
    TRUE - if the buffer was reallocated and the caller 
    should try the API again.

--*/
{
    BOOLEAN         TryAgain;

    //
    // If this is an initial request, buffer will be null with a new buffer size
    //

    if (!*Buffer && BufferSize) {
        *Status = EFI_BUFFER_TOO_SMALL;
    }

    //
    // If the status code is "buffer too small", resize the buffer
    //
        
    TryAgain = FALSE;
    if (*Status == EFI_BUFFER_TOO_SMALL) {

        if (*Buffer) {
            FreePool (*Buffer);
        }

        *Buffer = AllocatePool (BufferSize);

        if (*Buffer) {
            TryAgain = TRUE;
        } else {    
            *Status = EFI_OUT_OF_RESOURCES;
        } 
    }

    //
    // If there's an error, free the buffer
    //

    if (!TryAgain && EFI_ERROR(*Status) && *Buffer) {
        FreePool (*Buffer);
        *Buffer = NULL;
    }

    return TryAgain;
}


EFI_MEMORY_DESCRIPTOR *
LibMemoryMap (
    OUT UINTN               *NoEntries,
    OUT UINTN               *MapKey,
    OUT UINTN               *DescriptorSize,
    OUT UINT32              *DescriptorVersion
    )
{
    EFI_STATUS              Status;
    EFI_MEMORY_DESCRIPTOR   *Buffer;
    UINTN                   BufferSize;

    //
    // Initialize for GrowBuffer loop
    //

    Status = EFI_SUCCESS;
    Buffer = NULL;
    BufferSize = sizeof(EFI_MEMORY_DESCRIPTOR);

    //
    // Call the real function
    //

    while (GrowBuffer (&Status, (VOID **) &Buffer, BufferSize)) {
        Status = uefi_call_wrapper(BS->GetMemoryMap, 5, &BufferSize, Buffer, MapKey, DescriptorSize, DescriptorVersion);
    }

    //
    // Convert buffer size to NoEntries
    //

    if (!EFI_ERROR(Status)) {
        *NoEntries = BufferSize / *DescriptorSize;
    }

    return Buffer;
}

VOID *
LibGetVariableAndSize (
    IN CHAR16               *Name,
    IN EFI_GUID             *VendorGuid,
    OUT UINTN               *VarSize
    )
{
    EFI_STATUS              Status;
    VOID                    *Buffer;
    UINTN                   BufferSize;

    //
    // Initialize for GrowBuffer loop
    //

    Buffer = NULL;
    BufferSize = 100;

    //
    // Call the real function
    //

    while (GrowBuffer (&Status, &Buffer, BufferSize)) {
        Status = uefi_call_wrapper(
		    RT->GetVariable,
			5,
                    Name,
                    VendorGuid,
                    NULL,
                    &BufferSize,
                    Buffer
                    );
    }
    if (Buffer) {
        *VarSize = BufferSize;
    } else {
        *VarSize = 0;
    }
    return Buffer;
}
    
VOID *
LibGetVariable (
    IN CHAR16               *Name,
    IN EFI_GUID             *VendorGuid
    )
{
    UINTN   VarSize;

    return LibGetVariableAndSize (Name, VendorGuid, &VarSize);
}

EFI_STATUS
LibDeleteVariable (
    IN CHAR16   *VarName,
    IN EFI_GUID *VarGuid
    )
{
    VOID        *VarBuf;
    EFI_STATUS  Status;

    VarBuf = LibGetVariable(VarName,VarGuid);

    Status = EFI_NOT_FOUND;

    if (VarBuf) {
        //
        // Delete variable from Storage
        //
        Status = uefi_call_wrapper(
		    RT->SetVariable,
			5,
                    VarName, VarGuid,
                    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                    0, NULL
                 );
        ASSERT (!EFI_ERROR(Status));
        FreePool(VarBuf);
    }

    return (Status);
}

EFI_STATUS
LibSetNVVariable (
    IN CHAR16   *VarName,
    IN EFI_GUID *VarGuid,
    IN UINTN	 DataSize,
    IN VOID     *Data
    )
{
    EFI_STATUS  Status;

    Status = uefi_call_wrapper(
	    RT->SetVariable,
	    5,
	    VarName, VarGuid,
	    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
	    DataSize, Data
	    );
    ASSERT (!EFI_ERROR(Status));
    return (Status);
}

EFI_STATUS
LibSetVariable (
    IN CHAR16   *VarName,
    IN EFI_GUID *VarGuid,
    IN UINTN	 DataSize,
    IN VOID     *Data
    )
{
    EFI_STATUS  Status;

    Status = uefi_call_wrapper(
	    RT->SetVariable,
	    5,
	    VarName, VarGuid,
	    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
	    DataSize, Data
	    );
    ASSERT (!EFI_ERROR(Status));
    return (Status);
}

EFI_STATUS
LibInsertToTailOfBootOrder (
    IN  UINT16  BootOption,
    IN  BOOLEAN OnlyInsertIfEmpty
    )
{
    UINT16      *BootOptionArray;
    UINT16      *NewBootOptionArray;
    UINTN       VarSize;
    UINTN       Index;
    EFI_STATUS  Status;

    BootOptionArray = LibGetVariableAndSize (VarBootOrder, &EfiGlobalVariable, &VarSize);    
    if (VarSize != 0 && OnlyInsertIfEmpty) {
        if (BootOptionArray) {
            FreePool (BootOptionArray);
        }
        return EFI_UNSUPPORTED;
    }

    VarSize += sizeof(UINT16);
    NewBootOptionArray = AllocatePool (VarSize);
    
    for (Index = 0; Index < ((VarSize/sizeof(UINT16)) - 1); Index++) {
        NewBootOptionArray[Index] = BootOptionArray[Index];
    }
    //
    // Insert in the tail of the array
    //
    NewBootOptionArray[Index] = BootOption;

    Status = uefi_call_wrapper(
		RT->SetVariable,
		5,
                VarBootOrder, &EfiGlobalVariable,
                EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                VarSize, (VOID*) NewBootOptionArray
                );

    if (NewBootOptionArray) {
        FreePool (NewBootOptionArray);
    }
    if (BootOptionArray) {
        FreePool (BootOptionArray);
    }
    return Status;
}


BOOLEAN
ValidMBR(
    IN  MASTER_BOOT_RECORD  *Mbr,
    IN  EFI_BLOCK_IO        *BlkIo
    )
{
    UINT32      StartingLBA, EndingLBA;
    UINT32      NewEndingLBA;
    INTN        i, j;
    BOOLEAN     ValidMbr;

    if (Mbr->Signature != MBR_SIGNATURE) {
        //
        // The BPB also has this signature, so it can not be used alone.
        //
        return FALSE;
    } 

    ValidMbr = FALSE;
    for (i=0; i<MAX_MBR_PARTITIONS; i++) {
        if ( Mbr->Partition[i].OSIndicator == 0x00 || EXTRACT_UINT32(Mbr->Partition[i].SizeInLBA) == 0 ) {
            continue;
        }
        ValidMbr = TRUE;
        StartingLBA = EXTRACT_UINT32(Mbr->Partition[i].StartingLBA);
        EndingLBA = StartingLBA + EXTRACT_UINT32(Mbr->Partition[i].SizeInLBA) - 1;
        if (EndingLBA > BlkIo->Media->LastBlock) {
            //
            // Compatability Errata:
            //  Some systems try to hide drive space with thier INT 13h driver
            //  This does not hide space from the OS driver. This means the MBR
            //  that gets created from DOS is smaller than the MBR created from 
            //  a real OS (NT & Win98). This leads to BlkIo->LastBlock being 
            //  wrong on some systems FDISKed by the OS.
            //
            //
            if (BlkIo->Media->LastBlock < MIN_MBR_DEVICE_SIZE) {
                //
                // If this is a very small device then trust the BlkIo->LastBlock
                //
                return FALSE;
            }

            if (EndingLBA > (BlkIo->Media->LastBlock + MBR_ERRATA_PAD)) {
                return FALSE;
            }

        }
        for (j=i+1; j<MAX_MBR_PARTITIONS; j++) {
            if (Mbr->Partition[j].OSIndicator == 0x00 || EXTRACT_UINT32(Mbr->Partition[j].SizeInLBA) == 0) {
                continue;
            }
            if (   EXTRACT_UINT32(Mbr->Partition[j].StartingLBA) >= StartingLBA && 
                   EXTRACT_UINT32(Mbr->Partition[j].StartingLBA) <= EndingLBA       ) {
                //
                // The Start of this region overlaps with the i'th region
                //
                return FALSE;
            } 
            NewEndingLBA = EXTRACT_UINT32(Mbr->Partition[j].StartingLBA) + EXTRACT_UINT32(Mbr->Partition[j].SizeInLBA) - 1;
            if ( NewEndingLBA >= StartingLBA && NewEndingLBA <= EndingLBA ) {
                //
                // The End of this region overlaps with the i'th region
                //
                return FALSE;
            }
        }
    }
    //
    // Non of the regions overlapped so MBR is O.K.
    //
    return ValidMbr;
} 
   

UINT8
DecimaltoBCD(
    IN  UINT8 DecValue
    )
{
    return RtDecimaltoBCD (DecValue);
}


UINT8
BCDtoDecimal(
    IN  UINT8 BcdValue
    )
{
    return RtBCDtoDecimal (BcdValue);
}

EFI_STATUS
LibGetSystemConfigurationTable(
    IN EFI_GUID *TableGuid,
    IN OUT VOID **Table
    )

{
    UINTN Index;

    for(Index=0;Index<ST->NumberOfTableEntries;Index++) {
        if (CompareGuid(TableGuid,&(ST->ConfigurationTable[Index].VendorGuid))==0) {
            *Table = ST->ConfigurationTable[Index].VendorTable;
            return EFI_SUCCESS;
        }
    }
    return EFI_NOT_FOUND;
}


CHAR16 *
LibGetUiString (
    IN  EFI_HANDLE      Handle,
    IN  UI_STRING_TYPE  StringType,
    IN  ISO_639_2       *LangCode,
    IN  BOOLEAN         ReturnDevicePathStrOnMismatch
    )
{
    UI_INTERFACE    *Ui;
    UI_STRING_TYPE  Index;
    UI_STRING_ENTRY *Array;
    EFI_STATUS      Status;
    
    Status = uefi_call_wrapper(BS->HandleProtocol, 3, Handle, &UiProtocol, (VOID *)&Ui);
    if (EFI_ERROR(Status)) {
        return (ReturnDevicePathStrOnMismatch) ? DevicePathToStr(DevicePathFromHandle(Handle)) : NULL;
    }

    //
    // Skip the first strings
    //
    for (Index = UiDeviceString, Array = Ui->Entry; Index < StringType; Index++, Array++) {
        while (Array->LangCode) {
            Array++;
        }
    }

    //
    // Search for the match
    //
    while (Array->LangCode) {
        if (strcmpa (Array->LangCode, LangCode) == 0) {
            return Array->UiString; 
        }
    }
    return (ReturnDevicePathStrOnMismatch) ? DevicePathToStr(DevicePathFromHandle(Handle)) : NULL;
}

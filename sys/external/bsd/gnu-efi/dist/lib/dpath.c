/*	$NetBSD: dpath.c,v 1.3 2018/08/16 18:22:05 jmcneill Exp $	*/

/*++

Copyright (c) 1998  Intel Corporation

Module Name:

    dpath.c

Abstract:
    MBR & Device Path functions



Revision History

2014/04 B.Burette - updated device path text representation, conforming to
	UEFI specification 2.4 (dec. 2013). More specifically:
	- § 9.3.5: added some media types ie. Sata()
	- § 9.6.1.2: Acpi(PNP0A03,0) makes more sense when displayed as PciRoot(0)
	- § 9.6.1.5: use commas (instead of '|') between option specific parameters
	- § 9.6.1.6: hex values in device paths must be preceded by "0x" or "0X"

--*/

#include "lib.h"

#define ALIGN_SIZE(a)   ((a % MIN_ALIGNMENT_SIZE) ? MIN_ALIGNMENT_SIZE - (a % MIN_ALIGNMENT_SIZE) : 0)



EFI_DEVICE_PATH *
DevicePathFromHandle (
    IN EFI_HANDLE       Handle
    )
{
    EFI_STATUS          Status;
    EFI_DEVICE_PATH     *DevicePath;

    Status = uefi_call_wrapper(BS->HandleProtocol, 3, Handle, &DevicePathProtocol, (VOID*)&DevicePath);
    if (EFI_ERROR(Status)) {
        DevicePath = NULL;
    }

    return DevicePath;
}


EFI_DEVICE_PATH *
DevicePathInstance (
    IN OUT EFI_DEVICE_PATH  **DevicePath,
    OUT UINTN               *Size
    )
{
    EFI_DEVICE_PATH         *Start, *Next, *DevPath;
    UINTN                   Count;

    DevPath = *DevicePath;
    Start = DevPath;

    if (!DevPath) {
        return NULL;
    }

    //
    // Check for end of device path type
    //

    for (Count = 0; ; Count++) {
        Next = NextDevicePathNode(DevPath);

        if (IsDevicePathEndType(DevPath)) {
            break;
        }

        if (Count > 01000) {
            //
            // BugBug: Debug code to catch bogus device paths
            //
            DEBUG((D_ERROR, "DevicePathInstance: DevicePath %x Size %d", *DevicePath, ((UINT8 *) DevPath) - ((UINT8 *) Start) ));
            DumpHex (0, 0, ((UINT8 *) DevPath) - ((UINT8 *) Start), Start);
            break;
        }

        DevPath = Next;
    }

    ASSERT (DevicePathSubType(DevPath) == END_ENTIRE_DEVICE_PATH_SUBTYPE ||
            DevicePathSubType(DevPath) == END_INSTANCE_DEVICE_PATH_SUBTYPE);

    //
    // Set next position
    //

    if (DevicePathSubType(DevPath) == END_ENTIRE_DEVICE_PATH_SUBTYPE) {
        Next = NULL;
    }

    *DevicePath = Next;

    //
    // Return size and start of device path instance
    //

    *Size = ((UINT8 *) DevPath) - ((UINT8 *) Start);
    return Start;
}

UINTN
DevicePathInstanceCount (
    IN EFI_DEVICE_PATH      *DevicePath
    )
{
    UINTN       Count, Size;

    Count = 0;
    while (DevicePathInstance(&DevicePath, &Size)) {
        Count += 1;
    }

    return Count;
}


EFI_DEVICE_PATH *
AppendDevicePath (
    IN EFI_DEVICE_PATH  *Src1,
    IN EFI_DEVICE_PATH  *Src2
    )
// Src1 may have multiple "instances" and each instance is appended
// Src2 is appended to each instance is Src1.  (E.g., it's possible
// to append a new instance to the complete device path by passing
// it in Src2)
{
    UINTN               Src1Size, Src1Inst, Src2Size, Size;
    EFI_DEVICE_PATH     *Dst, *Inst;
    UINT8               *DstPos;

    //
    // If there's only 1 path, just duplicate it
    //

    if (!Src1) {
        ASSERT (!IsDevicePathUnpacked (Src2));
        return DuplicateDevicePath (Src2);
    }

    if (!Src2) {
        ASSERT (!IsDevicePathUnpacked (Src1));
        return DuplicateDevicePath (Src1);
    }

    //
    // Verify we're not working with unpacked paths
    //

//    ASSERT (!IsDevicePathUnpacked (Src1));
//    ASSERT (!IsDevicePathUnpacked (Src2));

    //
    // Append Src2 to every instance in Src1
    //

    Src1Size = DevicePathSize(Src1);
    Src1Inst = DevicePathInstanceCount(Src1);
    Src2Size = DevicePathSize(Src2);
    Size = Src1Size * Src1Inst + Src2Size;

    Dst = AllocatePool (Size);
    if (Dst) {
        DstPos = (UINT8 *) Dst;

        //
        // Copy all device path instances
        //

        while ((Inst = DevicePathInstance (&Src1, &Size))) {

            CopyMem(DstPos, Inst, Size);
            DstPos += Size;

            CopyMem(DstPos, Src2, Src2Size);
            DstPos += Src2Size;

            CopyMem(DstPos, EndInstanceDevicePath, sizeof(EFI_DEVICE_PATH));
            DstPos += sizeof(EFI_DEVICE_PATH);
        }

        // Change last end marker
        DstPos -= sizeof(EFI_DEVICE_PATH);
        CopyMem(DstPos, EndDevicePath, sizeof(EFI_DEVICE_PATH));
    }

    return Dst;
}


EFI_DEVICE_PATH *
AppendDevicePathNode (
    IN EFI_DEVICE_PATH  *Src1,
    IN EFI_DEVICE_PATH  *Src2
    )
// Src1 may have multiple "instances" and each instance is appended
// Src2 is a signal device path node (without a terminator) that is
// appended to each instance is Src1.
{
    EFI_DEVICE_PATH     *Temp, *Eop;
    UINTN               Length;

    //
    // Build a Src2 that has a terminator on it
    //

    Length = DevicePathNodeLength(Src2);
    Temp = AllocatePool (Length + sizeof(EFI_DEVICE_PATH));
    if (!Temp) {
        return NULL;
    }

    CopyMem (Temp, Src2, Length);
    Eop = NextDevicePathNode(Temp);
    SetDevicePathEndNode(Eop);

    //
    // Append device paths
    //

    Src1 = AppendDevicePath (Src1, Temp);
    FreePool (Temp);
    return Src1;
}


EFI_DEVICE_PATH *
FileDevicePath (
    IN EFI_HANDLE       Device  OPTIONAL,
    IN CHAR16           *FileName
    )
/*++

    N.B. Results are allocated from pool.  The caller must FreePool
    the resulting device path structure

--*/
{
    UINTN                   Size;
    FILEPATH_DEVICE_PATH    *FilePath;
    EFI_DEVICE_PATH         *Eop, *DevicePath;

    Size = StrSize(FileName);
    FilePath = AllocateZeroPool (Size + SIZE_OF_FILEPATH_DEVICE_PATH + sizeof(EFI_DEVICE_PATH));
    DevicePath = NULL;

    if (FilePath) {

        //
        // Build a file path
        //

        FilePath->Header.Type = MEDIA_DEVICE_PATH;
        FilePath->Header.SubType = MEDIA_FILEPATH_DP;
        SetDevicePathNodeLength (&FilePath->Header, Size + SIZE_OF_FILEPATH_DEVICE_PATH);
        CopyMem (FilePath->PathName, FileName, Size);
        Eop = NextDevicePathNode(&FilePath->Header);
        SetDevicePathEndNode(Eop);

        //
        // Append file path to device's device path
        //

        DevicePath = (EFI_DEVICE_PATH *) FilePath;
        if (Device) {
            DevicePath = AppendDevicePath (
                            DevicePathFromHandle(Device),
                            DevicePath
                            );

            FreePool(FilePath);
        }
    }

    return DevicePath;
}



UINTN
DevicePathSize (
    IN EFI_DEVICE_PATH  *DevPath
    )
{
    EFI_DEVICE_PATH     *Start;

    //
    // Search for the end of the device path structure
    //

    Start = DevPath;
    while (!IsDevicePathEnd(DevPath)) {
        DevPath = NextDevicePathNode(DevPath);
    }

    //
    // Compute the size
    //

    return ((UINTN) DevPath - (UINTN) Start) + sizeof(EFI_DEVICE_PATH);
}

EFI_DEVICE_PATH *
DuplicateDevicePath (
    IN EFI_DEVICE_PATH  *DevPath
    )
{
    EFI_DEVICE_PATH     *NewDevPath;
    UINTN               Size;


    //
    // Compute the size
    //

    Size = DevicePathSize (DevPath);

    //
    // Make a copy
    //

    NewDevPath = AllocatePool (Size);
    if (NewDevPath) {
        CopyMem (NewDevPath, DevPath, Size);
    }

    return NewDevPath;
}

EFI_DEVICE_PATH *
UnpackDevicePath (
    IN EFI_DEVICE_PATH  *DevPath
    )
{
    EFI_DEVICE_PATH     *Src, *Dest, *NewPath;
    UINTN               Size;

    //
    // Walk device path and round sizes to valid boundries
    //

    Src = DevPath;
    Size = 0;
    for (; ;) {
        Size += DevicePathNodeLength(Src);
        Size += ALIGN_SIZE(Size);

        if (IsDevicePathEnd(Src)) {
            break;
        }

        Src = NextDevicePathNode(Src);
    }


    //
    // Allocate space for the unpacked path
    //

    NewPath = AllocateZeroPool (Size);
    if (NewPath) {

        ASSERT (((UINTN)NewPath) % MIN_ALIGNMENT_SIZE == 0);

        //
        // Copy each node
        //

        Src = DevPath;
        Dest = NewPath;
        for (; ;) {
            Size = DevicePathNodeLength(Src);
            CopyMem (Dest, Src, Size);
            Size += ALIGN_SIZE(Size);
            SetDevicePathNodeLength (Dest, Size);
            Dest->Type |= EFI_DP_TYPE_UNPACKED;
            Dest = (EFI_DEVICE_PATH *) (((UINT8 *) Dest) + Size);

            if (IsDevicePathEnd(Src)) {
                break;
            }

            Src = NextDevicePathNode(Src);
        }
    }

    return NewPath;
}


EFI_DEVICE_PATH*
AppendDevicePathInstance (
    IN EFI_DEVICE_PATH  *Src,
    IN EFI_DEVICE_PATH  *Instance
    )
{
    UINT8           *Ptr;
    EFI_DEVICE_PATH *DevPath;
    UINTN           SrcSize;
    UINTN           InstanceSize;

    if (Src == NULL) {
        return DuplicateDevicePath (Instance);
    }
    SrcSize = DevicePathSize(Src);
    InstanceSize = DevicePathSize(Instance);
    Ptr = AllocatePool (SrcSize + InstanceSize);
    DevPath = (EFI_DEVICE_PATH *)Ptr;
    ASSERT(DevPath);

    CopyMem (Ptr, Src, SrcSize);
//    FreePool (Src);

    while (!IsDevicePathEnd(DevPath)) {
        DevPath = NextDevicePathNode(DevPath);
    }
    //
    // Convert the End to an End Instance, since we are
    //  appending another instacne after this one its a good
    //  idea.
    //
    DevPath->SubType = END_INSTANCE_DEVICE_PATH_SUBTYPE;

    DevPath = NextDevicePathNode(DevPath);
    CopyMem (DevPath, Instance, InstanceSize);
    return (EFI_DEVICE_PATH *)Ptr;
}

EFI_STATUS
LibDevicePathToInterface (
    IN EFI_GUID             *Protocol,
    IN EFI_DEVICE_PATH      *FilePath,
    OUT VOID                **Interface
    )
{
    EFI_STATUS              Status;
    EFI_HANDLE              Device;

    Status = uefi_call_wrapper(BS->LocateDevicePath, 3, Protocol, &FilePath, &Device);

    if (!EFI_ERROR(Status)) {

        // If we didn't get a direct match return not found
        Status = EFI_NOT_FOUND;

        if (IsDevicePathEnd(FilePath)) {

            //
            // It was a direct match, lookup the protocol interface
            //

            Status =uefi_call_wrapper(BS->HandleProtocol, 3, Device, Protocol, Interface);
        }
    }

    //
    // If there was an error, do not return an interface
    //

    if (EFI_ERROR(Status)) {
        *Interface = NULL;
    }

    return Status;
}

static VOID
_DevPathPci (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    PCI_DEVICE_PATH         *Pci;

    Pci = DevPath;
    CatPrint(Str, L"Pci(0x%x,0x%x)", Pci->Device, Pci->Function);
}

static VOID
_DevPathPccard (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    PCCARD_DEVICE_PATH      *Pccard;

    Pccard = DevPath;
    CatPrint(Str, L"Pccard(0x%x)", Pccard-> FunctionNumber );
}

static VOID
_DevPathMemMap (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    MEMMAP_DEVICE_PATH      *MemMap;

    MemMap = DevPath;
    CatPrint(Str, L"MemMap(%d,0x%x,0x%x)",
        MemMap->MemoryType,
        MemMap->StartingAddress,
        MemMap->EndingAddress
        );
}

static VOID
_DevPathController (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    CONTROLLER_DEVICE_PATH  *Controller;

    Controller = DevPath;
    CatPrint(Str, L"Ctrl(%d)",
        Controller->Controller
        );
}

static VOID
_DevPathVendor (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    VENDOR_DEVICE_PATH                  *Vendor;
    CHAR16                              *Type;
    UNKNOWN_DEVICE_VENDOR_DEVICE_PATH   *UnknownDevPath;

    Vendor = DevPath;
    switch (DevicePathType(&Vendor->Header)) {
    case HARDWARE_DEVICE_PATH:  Type = L"Hw";        break;
    case MESSAGING_DEVICE_PATH: Type = L"Msg";       break;
    case MEDIA_DEVICE_PATH:     Type = L"Media";     break;
    default:                    Type = L"?";         break;
    }

    CatPrint(Str, L"Ven%s(%g", Type, &Vendor->Guid);
    if (CompareGuid (&Vendor->Guid, &UnknownDevice) == 0) {
        //
        // GUID used by EFI to enumerate an EDD 1.1 device
        //
        UnknownDevPath = (UNKNOWN_DEVICE_VENDOR_DEVICE_PATH *)Vendor;
        CatPrint(Str, L":%02x)", UnknownDevPath->LegacyDriveLetter);
    } else {
        CatPrint(Str, L")");
    }
}


/*
  Type: 2 (ACPI Device Path) SubType: 1 (ACPI Device Path)
 */
static VOID
_DevPathAcpi (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    ACPI_HID_DEVICE_PATH        *Acpi;

    Acpi = DevPath;
    if ((Acpi->HID & PNP_EISA_ID_MASK) == PNP_EISA_ID_CONST) {
        switch ( EISA_ID_TO_NUM( Acpi-> HID ) ) {
            case 0x301 : {
                CatPrint( Str , L"Keyboard(%d)" , Acpi-> UID ) ;
                break ;
            }
            case 0x401 : {
                CatPrint( Str , L"ParallelPort(%d)" , Acpi-> UID ) ;
                break ;
            }
            case 0x501 : {
                CatPrint( Str , L"Serial(%d)" , Acpi-> UID ) ;
                break ;
            }
            case 0x604 : {
                CatPrint( Str , L"Floppy(%d)" , Acpi-> UID ) ;
                break ;
            }
            case 0xa03 : {
                CatPrint( Str , L"PciRoot(%d)" , Acpi-> UID ) ;
                break ;
            }
            case 0xa08 : {
                CatPrint( Str , L"PcieRoot(%d)" , Acpi-> UID ) ;
                break ;
            }
            default : {
                CatPrint( Str , L"Acpi(PNP%04x" , EISA_ID_TO_NUM( Acpi-> HID ) ) ;
                if ( Acpi-> UID ) CatPrint( Str , L",%d" , Acpi-> UID ) ;
                CatPrint( Str , L")" ) ;
                break ;
            }
	}
    } else {
        CatPrint( Str , L"Acpi(0x%X" , Acpi-> HID ) ;
        if ( Acpi-> UID ) CatPrint( Str , L",%d" , Acpi-> UID ) ;
        CatPrint( Str , L")" , Acpi-> HID , Acpi-> UID ) ;
    }
}


static VOID
_DevPathAtapi (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    ATAPI_DEVICE_PATH       *Atapi;

    Atapi = DevPath;
    CatPrint(Str, L"Ata(%s,%s)",
        Atapi->PrimarySecondary ? L"Secondary" : L"Primary",
        Atapi->SlaveMaster ? L"Slave" : L"Master"
        );
}

static VOID
_DevPathScsi (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    SCSI_DEVICE_PATH        *Scsi;

    Scsi = DevPath;
    CatPrint(Str, L"Scsi(%d,%d)", Scsi->Pun, Scsi->Lun);
}


static VOID
_DevPathFibre (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    FIBRECHANNEL_DEVICE_PATH    *Fibre;

    Fibre = DevPath;
    CatPrint( Str , L"Fibre%s(0x%016lx,0x%016lx)" ,
        DevicePathType( & Fibre-> Header ) == MSG_FIBRECHANNEL_DP ? L"" : L"Ex" ,
        Fibre-> WWN , Fibre-> Lun ) ;
}

static VOID
_DevPath1394 (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    F1394_DEVICE_PATH       *F1394;

    F1394 = DevPath;
    CatPrint(Str, L"1394(%g)", &F1394->Guid);
}



static VOID
_DevPathUsb (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    USB_DEVICE_PATH         *Usb;

    Usb = DevPath;
    CatPrint( Str , L"Usb(0x%x,0x%x)" , Usb-> Port , Usb-> Endpoint ) ;
}


static VOID
_DevPathI2O (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    I2O_DEVICE_PATH         *I2O;

    I2O = DevPath;
    CatPrint(Str, L"I2O(0x%X)", I2O->Tid);
}

static VOID
_DevPathMacAddr (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    MAC_ADDR_DEVICE_PATH    *MAC;
    UINTN                   HwAddressSize;
    UINTN                   Index;

    MAC = DevPath;

    /* HwAddressSize = sizeof(EFI_MAC_ADDRESS); */
    HwAddressSize = DevicePathNodeLength( & MAC-> Header ) ;
    HwAddressSize -= sizeof( MAC-> Header ) ;
    HwAddressSize -= sizeof( MAC-> IfType ) ;
    if (MAC->IfType == 0x01 || MAC->IfType == 0x00) {
        HwAddressSize = 6;
    }

    CatPrint(Str, L"Mac(");

    for(Index = 0; Index < HwAddressSize; Index++) {
        CatPrint(Str, L"%02x",MAC->MacAddress.Addr[Index]);
    }
    if ( MAC-> IfType != 0 ) {
        CatPrint(Str, L",%d" , MAC-> IfType ) ;
    }
    CatPrint(Str, L")");
}

static VOID
CatPrintIPv4(
    IN OUT POOL_PRINT * Str ,
    IN EFI_IPv4_ADDRESS * Address
    )
{
    CatPrint( Str , L"%d.%d.%d.%d" , Address-> Addr[ 0 ] , Address-> Addr[ 1 ] ,
        Address-> Addr[ 2 ] , Address-> Addr[ 3 ] ) ;
}

static BOOLEAN
IsNotNullIPv4(
    IN EFI_IPv4_ADDRESS * Address
    )
{
    UINT8 val ;
    val = Address-> Addr[ 0 ] | Address-> Addr[ 1 ] ;
    val |= Address-> Addr[ 2 ] | Address-> Addr[ 3 ] ;
    return val != 0 ;
}

static VOID
CatPrintNetworkProtocol(
    IN OUT POOL_PRINT * Str ,
    IN UINT16 Proto
    )
{
    if ( Proto == 6 ) {
        CatPrint( Str , L"TCP" ) ;
    } else if ( Proto == 17 ) {
        CatPrint( Str , L"UDP" ) ;
    } else {
        CatPrint( Str , L"%d" , Proto ) ;
    }
}

static VOID
_DevPathIPv4 (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    IPv4_DEVICE_PATH     *IP __unused;
    BOOLEAN show ;

    IP = DevPath;
    CatPrint( Str , L"IPv4(") ;
    CatPrintIPv4( Str , & IP-> RemoteIpAddress ) ;
    CatPrint( Str , L",") ;
    CatPrintNetworkProtocol( Str , IP-> Protocol ) ;
    CatPrint( Str , L",%s" , IP-> StaticIpAddress ? L"Static" : L"DHCP" ) ;
    show = IsNotNullIPv4( & IP-> LocalIpAddress ) ;
    if ( ! show && DevicePathNodeLength( & IP-> Header ) == sizeof( IPv4_DEVICE_PATH ) ) {
        /* only version 2 includes gateway and netmask */
        show |= IsNotNullIPv4( & IP-> GatewayIpAddress ) ;
        show |= IsNotNullIPv4( & IP-> SubnetMask  ) ;
    }
    if ( show ) {
        CatPrint( Str , L"," ) ;
        CatPrintIPv4( Str , & IP-> LocalIpAddress ) ;
        if ( DevicePathNodeLength( & IP-> Header ) == sizeof( IPv4_DEVICE_PATH ) ) {
            /* only version 2 includes gateway and netmask */
            show = IsNotNullIPv4( & IP-> GatewayIpAddress ) ;
            show |= IsNotNullIPv4( & IP-> SubnetMask ) ;
            if ( show ) {
                CatPrint( Str , L",") ;
                CatPrintIPv4( Str , & IP-> GatewayIpAddress ) ;
                if ( IsNotNullIPv4( & IP-> SubnetMask ) ) {
                    CatPrint( Str , L",") ;
                    CatPrintIPv4( Str , & IP-> SubnetMask ) ;
                }
            }
        }
    }
    CatPrint( Str , L")") ;
}

#define CatPrintIPv6_ADD( x , y ) ( ( (UINT16) ( x ) ) << 8 | ( y ) )
static VOID
CatPrintIPv6(
    IN OUT POOL_PRINT * Str ,
    IN EFI_IPv6_ADDRESS * Address
    )
{
    CatPrint( Str , L"%x:%x:%x:%x:%x:%x:%x:%x" ,
        CatPrintIPv6_ADD( Address-> Addr[ 0 ] , Address-> Addr[ 1 ] ) ,
        CatPrintIPv6_ADD( Address-> Addr[ 2 ] , Address-> Addr[ 3 ] ) ,
        CatPrintIPv6_ADD( Address-> Addr[ 4 ] , Address-> Addr[ 5 ] ) ,
        CatPrintIPv6_ADD( Address-> Addr[ 6 ] , Address-> Addr[ 7 ] ) ,
        CatPrintIPv6_ADD( Address-> Addr[ 8 ] , Address-> Addr[ 9 ] ) ,
        CatPrintIPv6_ADD( Address-> Addr[ 10 ] , Address-> Addr[ 11 ] ) ,
        CatPrintIPv6_ADD( Address-> Addr[ 12 ] , Address-> Addr[ 13 ] ) ,
        CatPrintIPv6_ADD( Address-> Addr[ 14 ] , Address-> Addr[ 15 ] ) ) ;
}

static VOID
_DevPathIPv6 (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    IPv6_DEVICE_PATH     *IP __unused;

    IP = DevPath;
    CatPrint( Str , L"IPv6(") ;
    CatPrintIPv6( Str , & IP-> RemoteIpAddress ) ;
    CatPrint( Str , L",") ;
    CatPrintNetworkProtocol( Str, IP-> Protocol ) ;
    CatPrint( Str , L",%s," , IP-> IPAddressOrigin ?
        ( IP-> IPAddressOrigin == 1 ? L"StatelessAutoConfigure" :
        L"StatefulAutoConfigure" ) : L"Static" ) ;
    CatPrintIPv6( Str , & IP-> LocalIpAddress ) ;
    if ( DevicePathNodeLength( & IP-> Header ) == sizeof( IPv6_DEVICE_PATH ) ) {
        CatPrint( Str , L",") ;
        CatPrintIPv6( Str , & IP-> GatewayIpAddress ) ;
        CatPrint( Str , L",") ;
        CatPrint( Str , L"%d" , & IP-> PrefixLength ) ;
    }
    CatPrint( Str , L")") ;
}

static VOID
_DevPathUri (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    URI_DEVICE_PATH  *Uri;

    Uri = DevPath;

    CatPrint( Str, L"Uri(%a)", Uri->Uri );
}

static VOID
_DevPathInfiniBand (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    INFINIBAND_DEVICE_PATH  *InfiniBand __unused;

    InfiniBand = DevPath;
    CatPrint( Str , L"Infiniband(0x%x,%g,0x%lx,0x%lx,0x%lx)" ,
        InfiniBand-> ResourceFlags , InfiniBand-> PortGid , InfiniBand-> ServiceId ,
        InfiniBand-> TargetPortId , InfiniBand-> DeviceId ) ;
}

static VOID
_DevPathUart (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    UART_DEVICE_PATH  *Uart;
    CHAR8             Parity;

    Uart = DevPath;
    switch (Uart->Parity) {
        case 0  : Parity = 'D'; break;
        case 1  : Parity = 'N'; break;
        case 2  : Parity = 'E'; break;
        case 3  : Parity = 'O'; break;
        case 4  : Parity = 'M'; break;
        case 5  : Parity = 'S'; break;
        default : Parity = 'x'; break;
    }

    if (Uart->BaudRate == 0) {
        CatPrint(Str, L"Uart(DEFAULT %c",Uart->BaudRate,Parity);
    } else {
        CatPrint(Str, L"Uart(%d %c",Uart->BaudRate,Parity);
    }

    if (Uart->DataBits == 0) {
        CatPrint(Str, L"D");
    } else {
        CatPrint(Str, L"%d",Uart->DataBits);
    }

    switch (Uart->StopBits) {
        case 0  : CatPrint(Str, L"D)");   break;
        case 1  : CatPrint(Str, L"1)");   break;
        case 2  : CatPrint(Str, L"1.5)"); break;
        case 3  : CatPrint(Str, L"2)");   break;
        default : CatPrint(Str, L"x)");   break;
    }
}

static VOID
_DevPathSata (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    SATA_DEVICE_PATH * Sata ;

    Sata = DevPath;
    CatPrint( Str , L"Sata(0x%x,0x%x,0x%x)" , Sata-> HBAPortNumber ,
        Sata-> PortMultiplierPortNumber , Sata-> Lun ) ;
}

static VOID
_DevPathHardDrive (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    HARDDRIVE_DEVICE_PATH   *Hd;

    Hd = DevPath;
    switch (Hd->SignatureType) {
        case SIGNATURE_TYPE_MBR:
            CatPrint(Str, L"HD(Part%d,Sig%08X)",
                Hd->PartitionNumber,
                *((UINT32 *)(&(Hd->Signature[0])))
                );
            break;
        case SIGNATURE_TYPE_GUID:
            CatPrint(Str, L"HD(Part%d,Sig%g)",
                Hd->PartitionNumber,
                (EFI_GUID *) &(Hd->Signature[0])
                );
            break;
        default:
            CatPrint(Str, L"HD(Part%d,MBRType=%02x,SigType=%02x)",
                Hd->PartitionNumber,
                Hd->MBRType,
                Hd->SignatureType
                );
            break;
    }
}

static VOID
_DevPathCDROM (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    CDROM_DEVICE_PATH       *Cd;

    Cd = DevPath;
    CatPrint( Str , L"CDROM(0x%x)" , Cd-> BootEntry ) ;
}

static VOID
_DevPathFilePath (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    FILEPATH_DEVICE_PATH    *Fp;

    Fp = DevPath;
    CatPrint(Str, L"%s", Fp->PathName);
}

static VOID
_DevPathMediaProtocol (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    MEDIA_PROTOCOL_DEVICE_PATH  *MediaProt;

    MediaProt = DevPath;
    CatPrint(Str, L"%g", &MediaProt->Protocol);
}

static VOID
_DevPathBssBss (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    BBS_BBS_DEVICE_PATH     *Bss;
    CHAR16                  *Type;

    Bss = DevPath;
    switch (Bss->DeviceType) {
    case BBS_TYPE_FLOPPY:               Type = L"Floppy";       break;
    case BBS_TYPE_HARDDRIVE:            Type = L"Harddrive";    break;
    case BBS_TYPE_CDROM:                Type = L"CDROM";        break;
    case BBS_TYPE_PCMCIA:               Type = L"PCMCIA";       break;
    case BBS_TYPE_USB:                  Type = L"Usb";          break;
    case BBS_TYPE_EMBEDDED_NETWORK:     Type = L"Net";          break;
    default:                            Type = L"?";            break;
    }

    CatPrint(Str, L"Bss-%s(%a)", Type, Bss->String);
}


static VOID
_DevPathEndInstance (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath EFI_UNUSED
    )
{
    CatPrint(Str, L",");
}

/**
 * Print unknown device node.
 * UEFI 2.4 § 9.6.1.6 table 89.
 */

static VOID
_DevPathNodeUnknown (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
    )
{
    EFI_DEVICE_PATH * Path ;
    UINT8 * value ;
    int length , index ;
    Path = DevPath ;
    value = DevPath ;
    value += 4 ;
    switch ( Path-> Type ) {
        case HARDWARE_DEVICE_PATH : { /* Unknown Hardware Device Path */
            CatPrint( Str , L"HardwarePath(%d" , Path-> SubType ) ;
            break ;
        }
        case ACPI_DEVICE_PATH : { /* Unknown ACPI Device Path */
            CatPrint( Str , L"AcpiPath(%d" , Path-> SubType ) ;
            break ;
        }
        case MESSAGING_DEVICE_PATH : { /* Unknown Messaging Device Path */
            CatPrint( Str , L"Msg(%d" , Path-> SubType ) ;
            break ;
        }
        case MEDIA_DEVICE_PATH : { /* Unknown Media Device Path */
            CatPrint( Str , L"MediaPath(%d" , Path-> SubType ) ;
            break ;
        }
        case BBS_DEVICE_PATH : { /* Unknown BIOS Boot Specification Device Path */
            CatPrint( Str , L"BbsPath(%d" , Path-> SubType ) ;
            break ;
        }
        default : { /* Unknown Device Path */
            CatPrint( Str , L"Path(%d,%d" , Path-> Type , Path-> SubType ) ;
            break ;
        }
    }
    length = DevicePathNodeLength( Path ) ;
    for ( index = 0 ; index < length ; index ++ ) {
        if ( index == 0 ) CatPrint( Str , L",0x" ) ;
        CatPrint( Str , L"%02x" , * value ) ;
	value ++ ;
    }
    CatPrint( Str , L")" ) ;
}


/*
 * Table to convert "Type" and "SubType" to a "convert to text" function/
 * Entries hold "Type" and "SubType" for know values.
 * Special "SubType" 0 is used as default for known type with unknown subtype.
 */
struct {
    UINT8   Type;
    UINT8   SubType;
    VOID    (*Function)(POOL_PRINT *, VOID *);
} DevPathTable[] = {
	{ HARDWARE_DEVICE_PATH,   HW_PCI_DP,                        _DevPathPci},
	{ HARDWARE_DEVICE_PATH,   HW_PCCARD_DP,                     _DevPathPccard},
	{ HARDWARE_DEVICE_PATH,   HW_MEMMAP_DP,                     _DevPathMemMap},
	{ HARDWARE_DEVICE_PATH,   HW_VENDOR_DP,                     _DevPathVendor},
	{ HARDWARE_DEVICE_PATH,   HW_CONTROLLER_DP,                 _DevPathController},
	{ ACPI_DEVICE_PATH,       ACPI_DP,                          _DevPathAcpi},
	{ MESSAGING_DEVICE_PATH,  MSG_ATAPI_DP,                     _DevPathAtapi},
	{ MESSAGING_DEVICE_PATH,  MSG_SCSI_DP,                      _DevPathScsi},
	{ MESSAGING_DEVICE_PATH,  MSG_FIBRECHANNEL_DP,              _DevPathFibre},
	{ MESSAGING_DEVICE_PATH,  MSG_1394_DP,                      _DevPath1394},
	{ MESSAGING_DEVICE_PATH,  MSG_USB_DP,                       _DevPathUsb},
	{ MESSAGING_DEVICE_PATH,  MSG_I2O_DP,                       _DevPathI2O},
	{ MESSAGING_DEVICE_PATH,  MSG_MAC_ADDR_DP,                  _DevPathMacAddr},
	{ MESSAGING_DEVICE_PATH,  MSG_IPv4_DP,                      _DevPathIPv4},
	{ MESSAGING_DEVICE_PATH,  MSG_IPv6_DP,                      _DevPathIPv6},
	{ MESSAGING_DEVICE_PATH,  MSG_URI_DP,                       _DevPathUri},
	{ MESSAGING_DEVICE_PATH,  MSG_INFINIBAND_DP,                _DevPathInfiniBand},
	{ MESSAGING_DEVICE_PATH,  MSG_UART_DP,                      _DevPathUart},
	{ MESSAGING_DEVICE_PATH , MSG_SATA_DP ,                     _DevPathSata } ,
	{ MESSAGING_DEVICE_PATH,  MSG_VENDOR_DP,                    _DevPathVendor},
	{ MEDIA_DEVICE_PATH,      MEDIA_HARDDRIVE_DP,               _DevPathHardDrive},
	{ MEDIA_DEVICE_PATH,      MEDIA_CDROM_DP,                   _DevPathCDROM},
	{ MEDIA_DEVICE_PATH,      MEDIA_VENDOR_DP,                  _DevPathVendor},
	{ MEDIA_DEVICE_PATH,      MEDIA_FILEPATH_DP,                _DevPathFilePath},
	{ MEDIA_DEVICE_PATH,      MEDIA_PROTOCOL_DP,                _DevPathMediaProtocol},
	{ BBS_DEVICE_PATH,        BBS_BBS_DP,                       _DevPathBssBss},
	{ END_DEVICE_PATH_TYPE,   END_INSTANCE_DEVICE_PATH_SUBTYPE, _DevPathEndInstance},
	{ 0,                      0,                          NULL}
};


CHAR16 *
DevicePathToStr (
    EFI_DEVICE_PATH     *DevPath
    )
/*++

    Turns the Device Path into a printable string.  Allcoates
    the string from pool.  The caller must FreePool the returned
    string.

--*/
{
    POOL_PRINT          Str;
    EFI_DEVICE_PATH     *DevPathNode;
    VOID                (*DumpNode)(POOL_PRINT *, VOID *);
    UINTN               Index, NewSize;

    ZeroMem(&Str, sizeof(Str));

    //
    // Unpacked the device path
    //

    DevPath = UnpackDevicePath(DevPath);
    ASSERT (DevPath);


    //
    // Process each device path node
    //

    DevPathNode = DevPath;
    while (!IsDevicePathEnd(DevPathNode)) {
        //
        // Find the handler to dump this device path node
        //

        DumpNode = NULL;
        for (Index = 0; DevPathTable[Index].Function; Index += 1) {

            if (DevicePathType(DevPathNode) == DevPathTable[Index].Type &&
                DevicePathSubType(DevPathNode) == DevPathTable[Index].SubType) {
                DumpNode = DevPathTable[Index].Function;
                break;
            }
        }

        //
        // If not found, use a generic function
        //

        if (!DumpNode) {
            DumpNode = _DevPathNodeUnknown;
        }

        //
        //  Put a path seperator in if needed
        //

        if (Str.len  &&  DumpNode != _DevPathEndInstance) {
            CatPrint (&Str, L"/");
        }

        //
        // Print this node of the device path
        //

        DumpNode (&Str, DevPathNode);

        //
        // Next device path node
        //

        DevPathNode = NextDevicePathNode(DevPathNode);
    }

    //
    // Shrink pool used for string allocation
    //

    FreePool (DevPath);
    NewSize = (Str.len + 1) * sizeof(CHAR16);
    Str.str = ReallocatePool (Str.str, NewSize, NewSize);
    Str.str[Str.len] = 0;
    return Str.str;
}

BOOLEAN
LibMatchDevicePaths (
    IN  EFI_DEVICE_PATH *Multi,
    IN  EFI_DEVICE_PATH *Single
    )
{
    EFI_DEVICE_PATH     *DevicePath, *DevicePathInst;
    UINTN               Size;

    if (!Multi || !Single) {
        return FALSE;
    }

    DevicePath = Multi;
    while ((DevicePathInst = DevicePathInstance (&DevicePath, &Size))) {
        if (CompareMem (Single, DevicePathInst, Size) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

EFI_DEVICE_PATH *
LibDuplicateDevicePathInstance (
    IN EFI_DEVICE_PATH  *DevPath
    )
{
    EFI_DEVICE_PATH     *NewDevPath,*DevicePathInst,*Temp;
    UINTN               Size = 0;

    //
    // get the size of an instance from the input
    //

    Temp = DevPath;
    DevicePathInst = DevicePathInstance (&Temp, &Size);

    //
    // Make a copy and set proper end type
    //
    NewDevPath = NULL;
    if (Size) {
        NewDevPath = AllocatePool (Size + sizeof(EFI_DEVICE_PATH));
    }

    if (NewDevPath) {
        CopyMem (NewDevPath, DevicePathInst, Size);
        Temp = NextDevicePathNode(NewDevPath);
        SetDevicePathEndNode(Temp);
    }

    return NewDevPath;
}


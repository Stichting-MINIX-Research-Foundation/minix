/*	$NetBSD: guid.c,v 1.1.1.2 2018/08/16 18:17:47 jmcneill Exp $	*/

/*++

Copyright (c) 1998  Intel Corporation

Module Name:

    misc.c

Abstract:

    Misc EFI support functions



Revision History

--*/

#include "lib.h"


//
// Additional Known guids
//

#define SHELL_INTERFACE_PROTOCOL \
    { 0x47c7b223, 0xc42a, 0x11d2, {0x8e, 0x57, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b} }

#define ENVIRONMENT_VARIABLE_ID  \
    { 0x47c7b224, 0xc42a, 0x11d2, {0x8e, 0x57, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b} }

#define DEVICE_PATH_MAPPING_ID  \
    { 0x47c7b225, 0xc42a, 0x11d2, {0x8e, 0x57, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b} }

#define PROTOCOL_ID_ID  \
    { 0x47c7b226, 0xc42a, 0x11d2, {0x8e, 0x57, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b} }

#define ALIAS_ID  \
    { 0x47c7b227, 0xc42a, 0x11d2, {0x8e, 0x57, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b} }

static EFI_GUID ShellInterfaceProtocol = SHELL_INTERFACE_PROTOCOL;
static EFI_GUID SEnvId                 = ENVIRONMENT_VARIABLE_ID;
static EFI_GUID SMapId                 = DEVICE_PATH_MAPPING_ID;
static EFI_GUID SProtId                = PROTOCOL_ID_ID;
static EFI_GUID SAliasId               = ALIAS_ID;

static struct {
    EFI_GUID        *Guid;
    WCHAR           *GuidName;
} KnownGuids[] = {
	{  &NullGuid,                                       L"G0" },
	{  &gEfiGlobalVariableGuid,                         L"EfiVar" },

	{  &VariableStoreProtocol,                          L"VarStore" },
	{  &gEfiDevicePathProtocolGuid,                     L"DevPath" },
	{  &gEfiLoadedImageProtocolGuid,                    L"LdImg" },
	{  &gEfiSimpleTextInProtocolGuid,                   L"TxtIn" },
	{  &gEfiSimpleTextOutProtocolGuid,                  L"TxtOut" },
	{  &gEfiBlockIoProtocolGuid,                        L"BlkIo" },
	{  &gEfiBlockIo2ProtocolGuid,                       L"BlkIo2" },
	{  &gEfiDiskIoProtocolGuid,                         L"DskIo" },
	{  &gEfiDiskIo2ProtocolGuid,                        L"DskIo2" },
	{  &gEfiSimpleFileSystemProtocolGuid,               L"Fs" },
	{  &gEfiLoadFileProtocolGuid,                       L"LdFile" },
	{  &gEfiDeviceIoProtocolGuid,                       L"DevIo" },
	{  &gEfiComponentNameProtocolGuid,                  L"CName" },
	{  &gEfiComponentName2ProtocolGuid,                 L"CName2" },

	{  &gEfiFileInfoGuid,                               L"FileInfo" },
	{  &gEfiFileSystemInfoGuid,                         L"FsInfo" },
	{  &gEfiFileSystemVolumeLabelInfoIdGuid,            L"FsVolInfo" },

	{  &gEfiUnicodeCollationProtocolGuid,               L"Unicode" },
	{  &LegacyBootProtocol,                             L"LegacyBoot" },
	{  &gEfiSerialIoProtocolGuid,                       L"SerIo" },
	{  &VgaClassProtocol,                               L"VgaClass"},
	{  &gEfiSimpleNetworkProtocolGuid,                  L"Net" },
	{  &gEfiNetworkInterfaceIdentifierProtocolGuid,     L"Nii" },
	{  &gEfiPxeBaseCodeProtocolGuid,                    L"Pxe" },
	{  &gEfiPxeBaseCodeCallbackProtocolGuid,            L"PxeCb" },

	{  &TextOutSpliterProtocol,                         L"TxtOutSplit" },
	{  &ErrorOutSpliterProtocol,                        L"ErrOutSplit" },
	{  &TextInSpliterProtocol,                          L"TxtInSplit" },
	{  &gEfiPcAnsiGuid,                                 L"PcAnsi" },
	{  &gEfiVT100Guid,                                  L"Vt100" },
	{  &gEfiVT100PlusGuid,                              L"Vt100Plus" },
	{  &gEfiVTUTF8Guid,                                 L"VtUtf8" },
	{  &UnknownDevice,                                  L"UnknownDev" },

	{  &EfiPartTypeSystemPartitionGuid,                 L"ESP" },
	{  &EfiPartTypeLegacyMbrGuid,                       L"GPT MBR" },

	{  &ShellInterfaceProtocol,                         L"ShellInt" },
	{  &SEnvId,                                         L"SEnv" },
	{  &SProtId,                                        L"ShellProtId" },
	{  &SMapId,                                         L"ShellDevPathMap" },
	{  &SAliasId,                                       L"ShellAlias" },

	{  NULL, L"" }
};

//
//
//

LIST_ENTRY          GuidList;


VOID
InitializeGuid (
    VOID
    )
{
}

INTN
CompareGuid(
    IN EFI_GUID     *Guid1,
    IN EFI_GUID     *Guid2
    )
/*++

Routine Description:

    Compares to GUIDs

Arguments:

    Guid1       - guid to compare
    Guid2       - guid to compare

Returns:
    = 0     if Guid1 == Guid2

--*/
{
    return RtCompareGuid (Guid1, Guid2);
}


VOID
GuidToString (
    OUT CHAR16      *Buffer,
    IN EFI_GUID     *Guid
    )
{

    UINTN           Index;

    //
    // Else, (for now) use additional internal function for mapping guids
    //

    for (Index=0; KnownGuids[Index].Guid; Index++) {
        if (CompareGuid(Guid, KnownGuids[Index].Guid) == 0) {
            SPrint (Buffer, 0, KnownGuids[Index].GuidName);
            return ;
        }
    }

    //
    // Else dump it
    //

    SPrint (Buffer, 0, L"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        Guid->Data1,
        Guid->Data2,
        Guid->Data3,
        Guid->Data4[0],
        Guid->Data4[1],
        Guid->Data4[2],
        Guid->Data4[3],
        Guid->Data4[4],
        Guid->Data4[5],
        Guid->Data4[6],
        Guid->Data4[7]
        );
}

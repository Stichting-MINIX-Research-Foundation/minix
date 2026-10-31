/*	$NetBSD: data.c,v 1.1.1.2 2018/08/16 18:17:47 jmcneill Exp $	*/

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
// LibInitialized - TRUE once InitializeLib() is called for the first time
//

BOOLEAN  LibInitialized = FALSE;

//
// ImageHandle - Current ImageHandle, as passed to InitializeLib
//
EFI_HANDLE LibImageHandle;

//
// ST - pointer to the EFI system table
//

EFI_SYSTEM_TABLE        *ST;

//
// BS - pointer to the boot services table
//

EFI_BOOT_SERVICES       *BS;


//
// Default pool allocation type
//

EFI_MEMORY_TYPE PoolAllocationType = EfiBootServicesData;

//
// Unicode collation functions that are in use
//

EFI_UNICODE_COLLATION_INTERFACE   LibStubUnicodeInterface = {
    LibStubStriCmp,
    LibStubMetaiMatch,
    LibStubStrLwrUpr,
    LibStubStrLwrUpr,
    NULL,   // FatToStr
    NULL,   // StrToFat
    NULL    // SupportedLanguages
};

EFI_UNICODE_COLLATION_INTERFACE   *UnicodeInterface = &LibStubUnicodeInterface;

//
// Root device path
//

EFI_DEVICE_PATH RootDevicePath[] = {
   {END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, {END_DEVICE_PATH_LENGTH,0}}
};

EFI_DEVICE_PATH EndDevicePath[] = {
   {END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, {END_DEVICE_PATH_LENGTH, 0}}
};

EFI_DEVICE_PATH EndInstanceDevicePath[] = {
   {END_DEVICE_PATH_TYPE, END_INSTANCE_DEVICE_PATH_SUBTYPE, {END_DEVICE_PATH_LENGTH, 0}}
};


//
// EFI IDs
//

EFI_GUID gEfiGlobalVariableGuid = EFI_GLOBAL_VARIABLE;
EFI_GUID NullGuid = { 0,0,0,{0,0,0,0,0,0,0,0} };

//
// Protocol IDs
//

EFI_GUID gEfiDevicePathProtocolGuid                 = EFI_DEVICE_PATH_PROTOCOL_GUID;
EFI_GUID gEfiDevicePathToTextProtocolGuid           = EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID;
EFI_GUID gEfiDevicePathFromTextProtocolGuid         = EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL_GUID;
EFI_GUID gEfiLoadedImageProtocolGuid                = EFI_LOADED_IMAGE_PROTOCOL_GUID;
EFI_GUID gEfiSimpleTextInProtocolGuid               = EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID;
EFI_GUID gEfiSimpleTextOutProtocolGuid              = EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID;
EFI_GUID gEfiBlockIoProtocolGuid                    = EFI_BLOCK_IO_PROTOCOL_GUID;
EFI_GUID gEfiBlockIo2ProtocolGuid                   = EFI_BLOCK_IO2_PROTOCOL_GUID;
EFI_GUID gEfiDiskIoProtocolGuid                     = EFI_DISK_IO_PROTOCOL_GUID;
EFI_GUID gEfiDiskIo2ProtocolGuid                    = EFI_DISK_IO2_PROTOCOL_GUID;
EFI_GUID gEfiSimpleFileSystemProtocolGuid           = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
EFI_GUID gEfiLoadFileProtocolGuid                   = EFI_LOAD_FILE_PROTOCOL_GUID;
EFI_GUID gEfiDeviceIoProtocolGuid                   = EFI_DEVICE_IO_PROTOCOL_GUID;
EFI_GUID gEfiUnicodeCollationProtocolGuid           = EFI_UNICODE_COLLATION_PROTOCOL_GUID;
EFI_GUID gEfiSerialIoProtocolGuid                   = EFI_SERIAL_IO_PROTOCOL_GUID;
EFI_GUID gEfiSimpleNetworkProtocolGuid              = EFI_SIMPLE_NETWORK_PROTOCOL_GUID;
EFI_GUID gEfiPxeBaseCodeProtocolGuid                = EFI_PXE_BASE_CODE_PROTOCOL_GUID;
EFI_GUID gEfiPxeBaseCodeCallbackProtocolGuid        = EFI_PXE_BASE_CODE_CALLBACK_PROTOCOL_GUID;
EFI_GUID gEfiNetworkInterfaceIdentifierProtocolGuid = EFI_NETWORK_INTERFACE_IDENTIFIER_PROTOCOL_GUID;
EFI_GUID gEFiUiInterfaceProtocolGuid                = EFI_UI_INTERFACE_PROTOCOL_GUID;
EFI_GUID gEfiPciIoProtocolGuid                      = EFI_PCI_IO_PROTOCOL_GUID;
EFI_GUID gEfiPciRootBridgeIoProtocolGuid            = EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_GUID;
EFI_GUID gEfiDriverBindingProtocolGuid              = EFI_DRIVER_BINDING_PROTOCOL_GUID;
EFI_GUID gEfiComponentNameProtocolGuid              = EFI_COMPONENT_NAME_PROTOCOL_GUID;
EFI_GUID gEfiComponentName2ProtocolGuid             = EFI_COMPONENT_NAME2_PROTOCOL_GUID;
EFI_GUID gEfiHashProtocolGuid                       = EFI_HASH_PROTOCOL_GUID;
EFI_GUID gEfiPlatformDriverOverrideProtocolGuid     = EFI_PLATFORM_DRIVER_OVERRIDE_PROTOCOL_GUID;
EFI_GUID gEfiBusSpecificDriverOverrideProtocolGuid  = EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_PROTOCOL_GUID;
EFI_GUID gEfiDriverFamilyOverrideProtocolGuid       = EFI_DRIVER_FAMILY_OVERRIDE_PROTOCOL_GUID;
EFI_GUID gEfiEbcProtocolGuid                        = EFI_EBC_PROTOCOL_GUID;

//
// File system information IDs
//

EFI_GUID gEfiFileInfoGuid                           = EFI_FILE_INFO_ID;
EFI_GUID gEfiFileSystemInfoGuid                     = EFI_FILE_SYSTEM_INFO_ID;
EFI_GUID gEfiFileSystemVolumeLabelInfoIdGuid        = EFI_FILE_SYSTEM_VOLUME_LABEL_INFO_ID;

//
// Reference implementation public protocol IDs
//

EFI_GUID InternalShellProtocol = INTERNAL_SHELL_GUID;
EFI_GUID VariableStoreProtocol = VARIABLE_STORE_PROTOCOL;
EFI_GUID LegacyBootProtocol = LEGACY_BOOT_PROTOCOL;
EFI_GUID VgaClassProtocol = VGA_CLASS_DRIVER_PROTOCOL;

EFI_GUID TextOutSpliterProtocol = TEXT_OUT_SPLITER_PROTOCOL;
EFI_GUID ErrorOutSpliterProtocol = ERROR_OUT_SPLITER_PROTOCOL;
EFI_GUID TextInSpliterProtocol = TEXT_IN_SPLITER_PROTOCOL;
/* Added for GOP support */
EFI_GUID gEfiGraphicsOutputProtocolGuid             = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
EFI_GUID gEfiEdidDiscoveredProtocolGuid             = EFI_EDID_DISCOVERED_PROTOCOL_GUID;
EFI_GUID gEfiEdidActiveProtocolGuid                 = EFI_EDID_ACTIVE_PROTOCOL_GUID;
EFI_GUID gEfiEdidOverrideProtocolGuid               = EFI_EDID_OVERRIDE_PROTOCOL_GUID;

EFI_GUID AdapterDebugProtocol = ADAPTER_DEBUG_PROTOCOL;

//
// Device path media protocol IDs
//
EFI_GUID gEfiPcAnsiGuid                             = EFI_PC_ANSI_GUID;
EFI_GUID gEfiVT100Guid                              = EFI_VT_100_GUID;
EFI_GUID gEfiVT100PlusGuid                          = EFI_VT_100_PLUS_GUID;
EFI_GUID gEfiVTUTF8Guid                             = EFI_VT_UTF8_GUID;

//
// EFI GPT Partition Type GUIDs
//
EFI_GUID EfiPartTypeSystemPartitionGuid = EFI_PART_TYPE_EFI_SYSTEM_PART_GUID;
EFI_GUID EfiPartTypeLegacyMbrGuid = EFI_PART_TYPE_LEGACY_MBR_GUID;


//
// Reference implementation Vendor Device Path Guids
//
EFI_GUID UnknownDevice      = UNKNOWN_DEVICE_GUID;

//
// Configuration Table GUIDs
//

EFI_GUID MpsTableGuid             = MPS_TABLE_GUID;
EFI_GUID AcpiTableGuid            = ACPI_TABLE_GUID;
EFI_GUID SMBIOSTableGuid          = SMBIOS_TABLE_GUID;
EFI_GUID SalSystemTableGuid       = SAL_SYSTEM_TABLE_GUID;

//
// Network protocol GUIDs
//
EFI_GUID Ip4ServiceBindingProtocol = EFI_IP4_SERVICE_BINDING_PROTOCOL;
EFI_GUID Ip4Protocol = EFI_IP4_PROTOCOL;
EFI_GUID Udp4ServiceBindingProtocol = EFI_UDP4_SERVICE_BINDING_PROTOCOL;
EFI_GUID Udp4Protocol = EFI_UDP4_PROTOCOL;
EFI_GUID Tcp4ServiceBindingProtocol = EFI_TCP4_SERVICE_BINDING_PROTOCOL;
EFI_GUID Tcp4Protocol = EFI_TCP4_PROTOCOL;

//
// Pointer protocol GUIDs
//
EFI_GUID SimplePointerProtocol    = EFI_SIMPLE_POINTER_PROTOCOL_GUID;
EFI_GUID AbsolutePointerProtocol  = EFI_ABSOLUTE_POINTER_PROTOCOL_GUID;

//
// Debugger protocol GUIDs
//
EFI_GUID gEfiDebugImageInfoTableGuid           = EFI_DEBUG_IMAGE_INFO_TABLE_GUID;
EFI_GUID gEfiDebugSupportProtocolGuid          = EFI_DEBUG_SUPPORT_PROTOCOL_GUID;

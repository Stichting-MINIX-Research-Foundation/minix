/******************************************************************************
 *
 * Module Name: dmtbinfo - Table info for non-AML tables
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2019, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#include "acpi.h"
#include "accommon.h"
#include "acdisasm.h"
#include "actbinfo.h"

/* This module used for application-level code only */

#define _COMPONENT          ACPI_CA_DISASSEMBLER
        ACPI_MODULE_NAME    ("dmtbinfo")

/*
 * How to add a new table:
 *
 * - Add the C table definition to the actbl1.h or actbl2.h header.
 * - Add ACPI_xxxx_OFFSET macro(s) for the table (and subtables) to list below.
 * - Define the table in this file (for the disassembler). If any
 *   new data types are required (ACPI_DMT_*), see below.
 * - Add an external declaration for the new table definition (AcpiDmTableInfo*)
 *     in acdisam.h
 * - Add new table definition to the dispatch table in dmtable.c (AcpiDmTableData)
 *     If a simple table (with no subtables), no disassembly code is needed.
 *     Otherwise, create the AcpiDmDump* function for to disassemble the table
 *     and add it to the dmtbdump.c file.
 * - Add an external declaration for the new AcpiDmDump* function in acdisasm.h
 * - Add the new AcpiDmDump* function to the dispatch table in dmtable.c
 * - Create a template for the new table
 * - Add data table compiler support
 *
 * How to add a new data type (ACPI_DMT_*):
 *
 * - Add new type at the end of the ACPI_DMT list in acdisasm.h
 * - Add length and implementation cases in dmtable.c  (disassembler)
 * - Add type and length cases in dtutils.c (DT compiler)
 */

/*
 * ACPI Table Information, used to dump formatted ACPI tables
 *
 * Each entry is of the form:  <Field Type, Field Offset, Field Name>
 */

/*******************************************************************************
 *
 * Common ACPI table header
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoHeader[] =
{
    {ACPI_DMT_SIG,      ACPI_HDR_OFFSET (Signature[0]),             "Signature", 0},
    {ACPI_DMT_UINT32,   ACPI_HDR_OFFSET (Length),                   "Table Length", DT_LENGTH},
    {ACPI_DMT_UINT8,    ACPI_HDR_OFFSET (Revision),                 "Revision", 0},
    {ACPI_DMT_CHKSUM,   ACPI_HDR_OFFSET (Checksum),                 "Checksum", 0},
    {ACPI_DMT_NAME6,    ACPI_HDR_OFFSET (OemId[0]),                 "Oem ID", 0},
    {ACPI_DMT_NAME8,    ACPI_HDR_OFFSET (OemTableId[0]),            "Oem Table ID", 0},
    {ACPI_DMT_UINT32,   ACPI_HDR_OFFSET (OemRevision),              "Oem Revision", 0},
    {ACPI_DMT_NAME4,    ACPI_HDR_OFFSET (AslCompilerId[0]),         "Asl Compiler ID", 0},
    {ACPI_DMT_UINT32,   ACPI_HDR_OFFSET (AslCompilerRevision),      "Asl Compiler Revision", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * GAS - Generic Address Structure
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoGas[] =
{
    {ACPI_DMT_SPACEID,  ACPI_GAS_OFFSET (SpaceId),                  "Space ID", 0},
    {ACPI_DMT_UINT8,    ACPI_GAS_OFFSET (BitWidth),                 "Bit Width", 0},
    {ACPI_DMT_UINT8,    ACPI_GAS_OFFSET (BitOffset),                "Bit Offset", 0},
    {ACPI_DMT_ACCWIDTH, ACPI_GAS_OFFSET (AccessWidth),              "Encoded Access Width", 0},
    {ACPI_DMT_UINT64,   ACPI_GAS_OFFSET (Address),                  "Address", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * RSDP - Root System Description Pointer (Signature is "RSD PTR ")
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoRsdp1[] =
{
    {ACPI_DMT_NAME8,    ACPI_RSDP_OFFSET (Signature[0]),            "Signature", 0},
    {ACPI_DMT_UINT8,    ACPI_RSDP_OFFSET (Checksum),                "Checksum", 0},
    {ACPI_DMT_NAME6,    ACPI_RSDP_OFFSET (OemId[0]),                "Oem ID", 0},
    {ACPI_DMT_UINT8,    ACPI_RSDP_OFFSET (Revision),                "Revision", 0},
    {ACPI_DMT_UINT32,   ACPI_RSDP_OFFSET (RsdtPhysicalAddress),     "RSDT Address", 0},
    ACPI_DMT_TERMINATOR
};

/* ACPI 2.0+ Extensions */

ACPI_DMTABLE_INFO           AcpiDmTableInfoRsdp2[] =
{
    {ACPI_DMT_UINT32,   ACPI_RSDP_OFFSET (Length),                  "Length", DT_LENGTH},
    {ACPI_DMT_UINT64,   ACPI_RSDP_OFFSET (XsdtPhysicalAddress),     "XSDT Address", 0},
    {ACPI_DMT_UINT8,    ACPI_RSDP_OFFSET (ExtendedChecksum),        "Extended Checksum", 0},
    {ACPI_DMT_UINT24,   ACPI_RSDP_OFFSET (Reserved[0]),             "Reserved", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * FACS - Firmware ACPI Control Structure
 *
 ******************************************************************************/

ACPI_DMTABLE_INFO           AcpiDmTableInfoFacs[] =
{
    {ACPI_DMT_NAME4,    ACPI_FACS_OFFSET (Signature[0]),            "Signature", 0},
    {ACPI_DMT_UINT32,   ACPI_FACS_OFFSET (Length),                  "Length", DT_LENGTH},
    {ACPI_DMT_UINT32,   ACPI_FACS_OFFSET (HardwareSignature),       "Hardware Signature", 0},
    {ACPI_DMT_UINT32,   ACPI_FACS_OFFSET (FirmwareWakingVector),    "32 Firmware Waking Vector", 0},
    {ACPI_DMT_UINT32,   ACPI_FACS_OFFSET (GlobalLock),              "Global Lock", 0},
    {ACPI_DMT_UINT32,   ACPI_FACS_OFFSET (Flags),                   "Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_FACS_FLAG_OFFSET (Flags,0),            "S4BIOS Support Present", 0},
    {ACPI_DMT_FLAG1,    ACPI_FACS_FLAG_OFFSET (Flags,0),            "64-bit Wake Supported (V2)", 0},
    {ACPI_DMT_UINT64,   ACPI_FACS_OFFSET (XFirmwareWakingVector),   "64 Firmware Waking Vector", 0},
    {ACPI_DMT_UINT8,    ACPI_FACS_OFFSET (Version),                 "Version", 0},
    {ACPI_DMT_UINT24,   ACPI_FACS_OFFSET (Reserved[0]),             "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_FACS_OFFSET (OspmFlags),               "OspmFlags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_FACS_FLAG_OFFSET (OspmFlags,0),        "64-bit Wake Env Required (V2)", 0},
    ACPI_DMT_TERMINATOR
};


/*******************************************************************************
 *
 * FADT - Fixed ACPI Description Table (Signature is FACP)
 *
 ******************************************************************************/

/* ACPI 1.0 FADT (Version 1) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoFadt1[] =
{
    {ACPI_DMT_UINT32,   ACPI_FADT_OFFSET (Facs),                    "FACS Address", 0},
    {ACPI_DMT_UINT32,   ACPI_FADT_OFFSET (Dsdt),                    "DSDT Address", DT_NON_ZERO},
    {ACPI_DMT_UINT8,    ACPI_FADT_OFFSET (Model),                   "Model", 0},
    {ACPI_DMT_FADTPM,   ACPI_FADT_OFFSET (PreferredProfile),        "PM Profile", 0},
    {ACPI_DMT_UINT16,   ACPI_FADT_OFFSET (SciInterrupt),            "SCI Interrupt", 0},
    {ACPI_DMT_UINT32,   ACPI_FADT_OFFSET (SmiCommand),              "SMI Command Port", 0},
    {ACPI_DMT_UINT8,    ACPI_FADT_OFFSET (AcpiEnable),              "ACPI Enable Value", 0},
    {ACPI_DMT_UINT8,    ACPI_FADT_OFFSET (AcpiDisable),             "ACPI Disable Value", 0},
    {ACPI_DMT_UINT8,    ACPI_FADT_OFFSET (S4BiosRequest),           "S4BIOS Command", 0},
    {ACPI_DMT_UINT8,    ACPI_FADT_OFFSET (PstateControl),           "P-State Control", 0},
    {ACPI_DMT_UINT32,   ACPI_FADT_OFFSET (Pm1aEventBlock),          "PM1A Event Block Address", 0},
    {ACPI_DMT_UINT32,   ACPI_FADT_OFFSET (Pm1bEventBlock),          "PM1B Event Block Address", 0},
    {ACPI_DMT_UINT32,   ACPI_FADT_OFFSET (Pm1aControlBlock),        "PM1A Control Block Address", 0},
    {ACPI_DMT_UINT32,   ACPI_FADT_OFFSET (Pm1bControlBlock),        "PM1B Control Block Address", 0},
    {ACPI_DMT_UINT32,   ACPI_FADT_OFFSET (Pm2ControlBlock),         "PM2 Control Block Address", 0},
    {ACPI_DMT_UINT32,   ACPI_FADT_OFFSET (PmTimerBlock),            "PM Timer Block Address", 0},
    {ACPI_DMT_UINT32,   ACPI_FADT_OFFSET (Gpe0Block),               "GPE0 Block Address", 0},
    {ACPI_DMT_UINT32,   ACPI_FADT_OFFSET (Gpe1Block),               "GPE1 Block Address", 0},
    {ACPI_DMT_UINT8,    ACPI_FADT_OFFSET (Pm1EventLength),          "PM1 Event Block Length", 0},
    {ACPI_DMT_UINT8,    ACPI_FADT_OFFSET (Pm1ControlLength),        "PM1 Control Block Length", 0},
    {ACPI_DMT_UINT8,    ACPI_FADT_OFFSET (Pm2ControlLength),        "PM2 Control Block Length", 0},
    {ACPI_DMT_UINT8,    ACPI_FADT_OFFSET (PmTimerLength),           "PM Timer Block Length", 0},
    {ACPI_DMT_UINT8,    ACPI_FADT_OFFSET (Gpe0BlockLength),         "GPE0 Block Length", 0},
    {ACPI_DMT_UINT8,    ACPI_FADT_OFFSET (Gpe1BlockLength),         "GPE1 Block Length", 0},
    {ACPI_DMT_UINT8,    ACPI_FADT_OFFSET (Gpe1Base),                "GPE1 Base Offset", 0},
    {ACPI_DMT_UINT8,    ACPI_FADT_OFFSET (CstControl),              "_CST Support", 0},
    {ACPI_DMT_UINT16,   ACPI_FADT_OFFSET (C2Latency),               "C2 Latency", 0},
    {ACPI_DMT_UINT16,   ACPI_FADT_OFFSET (C3Latency),               "C3 Latency", 0},
    {ACPI_DMT_UINT16,   ACPI_FADT_OFFSET (FlushSize),               "CPU Cache Size", 0},
    {ACPI_DMT_UINT16,   ACPI_FADT_OFFSET (FlushStride),             "Cache Flush Stride", 0},
    {ACPI_DMT_UINT8,    ACPI_FADT_OFFSET (DutyOffset),              "Duty Cycle Offset", 0},
    {ACPI_DMT_UINT8,    ACPI_FADT_OFFSET (DutyWidth),               "Duty Cycle Width", 0},
    {ACPI_DMT_UINT8,    ACPI_FADT_OFFSET (DayAlarm),                "RTC Day Alarm Index", 0},
    {ACPI_DMT_UINT8,    ACPI_FADT_OFFSET (MonthAlarm),              "RTC Month Alarm Index", 0},
    {ACPI_DMT_UINT8,    ACPI_FADT_OFFSET (Century),                 "RTC Century Index", 0},
    {ACPI_DMT_UINT16,   ACPI_FADT_OFFSET (BootFlags),               "Boot Flags (decoded below)", DT_FLAG},

    /* Boot Architecture Flags byte 0 */

    {ACPI_DMT_FLAG0,    ACPI_FADT_FLAG_OFFSET (BootFlags,0),        "Legacy Devices Supported (V2)", 0},
    {ACPI_DMT_FLAG1,    ACPI_FADT_FLAG_OFFSET (BootFlags,0),        "8042 Present on ports 60/64 (V2)", 0},
    {ACPI_DMT_FLAG2,    ACPI_FADT_FLAG_OFFSET (BootFlags,0),        "VGA Not Present (V4)", 0},
    {ACPI_DMT_FLAG3,    ACPI_FADT_FLAG_OFFSET (BootFlags,0),        "MSI Not Supported (V4)", 0},
    {ACPI_DMT_FLAG4,    ACPI_FADT_FLAG_OFFSET (BootFlags,0),        "PCIe ASPM Not Supported (V4)", 0},
    {ACPI_DMT_FLAG5,    ACPI_FADT_FLAG_OFFSET (BootFlags,0),        "CMOS RTC Not Present (V5)", 0},

    {ACPI_DMT_UINT8,    ACPI_FADT_OFFSET (Reserved),                "Reserved", 0},
    {ACPI_DMT_UINT32,   ACPI_FADT_OFFSET (Flags),                   "Flags (decoded below)", DT_FLAG},

    /* Flags byte 0 */

    {ACPI_DMT_FLAG0,    ACPI_FADT_FLAG_OFFSET (Flags,0),            "WBINVD instruction is operational (V1)", 0},
    {ACPI_DMT_FLAG1,    ACPI_FADT_FLAG_OFFSET (Flags,0),            "WBINVD flushes all caches (V1)", 0},
    {ACPI_DMT_FLAG2,    ACPI_FADT_FLAG_OFFSET (Flags,0),            "All CPUs support C1 (V1)", 0},
    {ACPI_DMT_FLAG3,    ACPI_FADT_FLAG_OFFSET (Flags,0),            "C2 works on MP system (V1)", 0},
    {ACPI_DMT_FLAG4,    ACPI_FADT_FLAG_OFFSET (Flags,0),            "Control Method Power Button (V1)", 0},
    {ACPI_DMT_FLAG5,    ACPI_FADT_FLAG_OFFSET (Flags,0),            "Control Method Sleep Button (V1)", 0},
    {ACPI_DMT_FLAG6,    ACPI_FADT_FLAG_OFFSET (Flags,0),            "RTC wake not in fixed reg space (V1)", 0},
    {ACPI_DMT_FLAG7,    ACPI_FADT_FLAG_OFFSET (Flags,0),            "RTC can wake system from S4 (V1)", 0},

    /* Flags byte 1 */

    {ACPI_DMT_FLAG0,    ACPI_FADT_FLAG_OFFSET (Flags,1),            "32-bit PM Timer (V1)", 0},
    {ACPI_DMT_FLAG1,    ACPI_FADT_FLAG_OFFSET (Flags,1),            "Docking Supported (V1)", 0},
    {ACPI_DMT_FLAG2,    ACPI_FADT_FLAG_OFFSET (Flags,1),            "Reset Register Supported (V2)", 0},
    {ACPI_DMT_FLAG3,    ACPI_FADT_FLAG_OFFSET (Flags,1),            "Sealed Case (V3)", 0},
    {ACPI_DMT_FLAG4,    ACPI_FADT_FLAG_OFFSET (Flags,1),            "Headless - No Video (V3)", 0},
    {ACPI_DMT_FLAG5,    ACPI_FADT_FLAG_OFFSET (Flags,1),            "Use native instr after SLP_TYPx (V3)", 0},
    {ACPI_DMT_FLAG6,    ACPI_FADT_FLAG_OFFSET (Flags,1),            "PCIEXP_WAK Bits Supported (V4)", 0},
    {ACPI_DMT_FLAG7,    ACPI_FADT_FLAG_OFFSET (Flags,1),            "Use Platform Timer (V4)", 0},

    /* Flags byte 2 */

    {ACPI_DMT_FLAG0,    ACPI_FADT_FLAG_OFFSET (Flags,2),            "RTC_STS valid on S4 wake (V4)", 0},
    {ACPI_DMT_FLAG1,    ACPI_FADT_FLAG_OFFSET (Flags,2),            "Remote Power-on capable (V4)", 0},
    {ACPI_DMT_FLAG2,    ACPI_FADT_FLAG_OFFSET (Flags,2),            "Use APIC Cluster Model (V4)", 0},
    {ACPI_DMT_FLAG3,    ACPI_FADT_FLAG_OFFSET (Flags,2),            "Use APIC Physical Destination Mode (V4)", 0},
    {ACPI_DMT_FLAG4,    ACPI_FADT_FLAG_OFFSET (Flags,2),            "Hardware Reduced (V5)", 0},
    {ACPI_DMT_FLAG5,    ACPI_FADT_FLAG_OFFSET (Flags,2),            "Low Power S0 Idle (V5)", 0},
    ACPI_DMT_TERMINATOR
};

/* ACPI 1.0 MS Extensions (FADT version 2) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoFadt2[] =
{
    {ACPI_DMT_GAS,      ACPI_FADT_OFFSET (ResetRegister),           "Reset Register", 0},
    {ACPI_DMT_UINT8,    ACPI_FADT_OFFSET (ResetValue),              "Value to cause reset", 0},
    {ACPI_DMT_UINT16,   ACPI_FADT_OFFSET (ArmBootFlags),            "Reserved", 0},
    {ACPI_DMT_UINT8,    ACPI_FADT_OFFSET (MinorRevision),           "Reserved", 0},
    ACPI_DMT_TERMINATOR
};

/* ACPI 2.0+ Extensions (FADT version 3, 4, and 5) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoFadt3[] =
{
    {ACPI_DMT_GAS,      ACPI_FADT_OFFSET (ResetRegister),           "Reset Register", 0},
    {ACPI_DMT_UINT8,    ACPI_FADT_OFFSET (ResetValue),              "Value to cause reset", 0},
    {ACPI_DMT_UINT16,   ACPI_FADT_OFFSET (ArmBootFlags),            "ARM Flags (decoded below)", DT_FLAG},
    {ACPI_DMT_FLAG0,    ACPI_FADT_FLAG_OFFSET(ArmBootFlags,0),      "PSCI Compliant", 0},
    {ACPI_DMT_FLAG1,    ACPI_FADT_FLAG_OFFSET(ArmBootFlags,0),      "Must use HVC for PSCI", 0},
    ACPI_DMT_NEW_LINE,
    {ACPI_DMT_UINT8,    ACPI_FADT_OFFSET (MinorRevision),           "FADT Minor Revision", 0},
    {ACPI_DMT_UINT64,   ACPI_FADT_OFFSET (XFacs),                   "FACS Address", 0},
    {ACPI_DMT_UINT64,   ACPI_FADT_OFFSET (XDsdt),                   "DSDT Address", 0},
    {ACPI_DMT_GAS,      ACPI_FADT_OFFSET (XPm1aEventBlock),         "PM1A Event Block", 0},
    {ACPI_DMT_GAS,      ACPI_FADT_OFFSET (XPm1bEventBlock),         "PM1B Event Block", 0},
    {ACPI_DMT_GAS,      ACPI_FADT_OFFSET (XPm1aControlBlock),       "PM1A Control Block", 0},
    {ACPI_DMT_GAS,      ACPI_FADT_OFFSET (XPm1bControlBlock),       "PM1B Control Block", 0},
    {ACPI_DMT_GAS,      ACPI_FADT_OFFSET (XPm2ControlBlock),        "PM2 Control Block", 0},
    {ACPI_DMT_GAS,      ACPI_FADT_OFFSET (XPmTimerBlock),           "PM Timer Block", 0},
    {ACPI_DMT_GAS,      ACPI_FADT_OFFSET (XGpe0Block),              "GPE0 Block", 0},
    {ACPI_DMT_GAS,      ACPI_FADT_OFFSET (XGpe1Block),              "GPE1 Block", 0},
    ACPI_DMT_TERMINATOR
};

/* ACPI 5.0 Extensions (FADT version 5) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoFadt5[] =
{
    {ACPI_DMT_GAS,      ACPI_FADT_OFFSET (SleepControl),            "Sleep Control Register", 0},
    {ACPI_DMT_GAS,      ACPI_FADT_OFFSET (SleepStatus),             "Sleep Status Register", 0},
    ACPI_DMT_TERMINATOR
};

/* ACPI 6.0 Extensions (FADT version 6) */

ACPI_DMTABLE_INFO           AcpiDmTableInfoFadt6[] =
{
    {ACPI_DMT_UINT64,   ACPI_FADT_OFFSET (HypervisorId),            "Hypervisor ID", 0},
    ACPI_DMT_TERMINATOR
};

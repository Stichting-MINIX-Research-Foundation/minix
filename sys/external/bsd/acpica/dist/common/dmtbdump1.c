/******************************************************************************
 *
 * Module Name: dmtbdump1 - Dump ACPI data tables that contain no AML code
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
#include "actables.h"

/* This module used for application-level code only */

#define _COMPONENT          ACPI_CA_DISASSEMBLER
        ACPI_MODULE_NAME    ("dmtbdump1")


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpAsf
 *
 * PARAMETERS:  Table               - A ASF table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a ASF table
 *
 ******************************************************************************/

void
AcpiDmDumpAsf (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    UINT32                  Offset = sizeof (ACPI_TABLE_HEADER);
    ACPI_ASF_INFO           *Subtable;
    ACPI_DMTABLE_INFO       *InfoTable;
    ACPI_DMTABLE_INFO       *DataInfoTable = NULL;
    UINT8                   *DataTable = NULL;
    UINT32                  DataCount = 0;
    UINT32                  DataLength = 0;
    UINT32                  DataOffset = 0;
    UINT32                  i;
    UINT8                   Type;


    /* No main table, only subtables */

    Subtable = ACPI_ADD_PTR (ACPI_ASF_INFO, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common subtable header */

        Status = AcpiDmDumpTable (Table->Length, Offset, Subtable,
            Subtable->Header.Length, AcpiDmTableInfoAsfHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* The actual type is the lower 7 bits of Type */

        Type = (UINT8) (Subtable->Header.Type & 0x7F);

        switch (Type)
        {
        case ACPI_ASF_TYPE_INFO:

            InfoTable = AcpiDmTableInfoAsf0;
            break;

        case ACPI_ASF_TYPE_ALERT:

            InfoTable = AcpiDmTableInfoAsf1;
            DataInfoTable = AcpiDmTableInfoAsf1a;
            DataTable = ACPI_ADD_PTR (UINT8, Subtable, sizeof (ACPI_ASF_ALERT));
            DataCount = ACPI_CAST_PTR (ACPI_ASF_ALERT, Subtable)->Alerts;
            DataLength = ACPI_CAST_PTR (ACPI_ASF_ALERT, Subtable)->DataLength;
            DataOffset = Offset + sizeof (ACPI_ASF_ALERT);
            break;

        case ACPI_ASF_TYPE_CONTROL:

            InfoTable = AcpiDmTableInfoAsf2;
            DataInfoTable = AcpiDmTableInfoAsf2a;
            DataTable = ACPI_ADD_PTR (UINT8, Subtable, sizeof (ACPI_ASF_REMOTE));
            DataCount = ACPI_CAST_PTR (ACPI_ASF_REMOTE, Subtable)->Controls;
            DataLength = ACPI_CAST_PTR (ACPI_ASF_REMOTE, Subtable)->DataLength;
            DataOffset = Offset + sizeof (ACPI_ASF_REMOTE);
            break;

        case ACPI_ASF_TYPE_BOOT:

            InfoTable = AcpiDmTableInfoAsf3;
            break;

        case ACPI_ASF_TYPE_ADDRESS:

            InfoTable = AcpiDmTableInfoAsf4;
            DataTable = ACPI_ADD_PTR (UINT8, Subtable, sizeof (ACPI_ASF_ADDRESS));
            DataLength = ACPI_CAST_PTR (ACPI_ASF_ADDRESS, Subtable)->Devices;
            DataOffset = Offset + sizeof (ACPI_ASF_ADDRESS);
            break;

        default:

            AcpiOsPrintf ("\n**** Unknown ASF subtable type 0x%X\n",
                Subtable->Header.Type);
            return;
        }

        Status = AcpiDmDumpTable (Table->Length, Offset, Subtable,
            Subtable->Header.Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Dump variable-length extra data */

        switch (Type)
        {
        case ACPI_ASF_TYPE_ALERT:
        case ACPI_ASF_TYPE_CONTROL:

            for (i = 0; i < DataCount; i++)
            {
                AcpiOsPrintf ("\n");
                Status = AcpiDmDumpTable (Table->Length, DataOffset,
                    DataTable, DataLength, DataInfoTable);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }

                DataTable = ACPI_ADD_PTR (UINT8, DataTable, DataLength);
                DataOffset += DataLength;
            }
            break;

        case ACPI_ASF_TYPE_ADDRESS:

            for (i = 0; i < DataLength; i++)
            {
                if (!(i % 16))
                {
                    AcpiDmLineHeader (DataOffset, 1, "Addresses");
                }

                AcpiOsPrintf ("%2.2X ", *DataTable);
                DataTable++;
                DataOffset++;

                if (DataOffset > Table->Length)
                {
                    AcpiOsPrintf (
                        "**** ACPI table terminates in the middle of a "
                        "data structure! (ASF! table)\n");
                    return;
                }
            }

            AcpiOsPrintf ("\n");
            break;

        default:

            break;
        }

        AcpiOsPrintf ("\n");

        /* Point to next subtable */

        if (!Subtable->Header.Length)
        {
            AcpiOsPrintf ("Invalid zero subtable header length\n");
            return;
        }

        Offset += Subtable->Header.Length;
        Subtable = ACPI_ADD_PTR (ACPI_ASF_INFO, Subtable,
            Subtable->Header.Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpCpep
 *
 * PARAMETERS:  Table               - A CPEP table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a CPEP. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpCpep (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_CPEP_POLLING       *Subtable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_CPEP);


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoCpep);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_CPEP_POLLING, Table, Offset);
    while (Offset < Table->Length)
    {
        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Header.Length, AcpiDmTableInfoCpep0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to next subtable */

        Offset += Subtable->Header.Length;
        Subtable = ACPI_ADD_PTR (ACPI_CPEP_POLLING, Subtable,
            Subtable->Header.Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpCsrt
 *
 * PARAMETERS:  Table               - A CSRT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a CSRT. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpCsrt (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_CSRT_GROUP         *Subtable;
    ACPI_CSRT_SHARED_INFO   *SharedInfoTable;
    ACPI_CSRT_DESCRIPTOR    *SubSubtable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_CSRT);
    UINT32                  SubOffset;
    UINT32                  SubSubOffset;
    UINT32                  InfoLength;


    /* The main table only contains the ACPI header, thus already handled */

    /* Subtables (Resource Groups) */

    Subtable = ACPI_ADD_PTR (ACPI_CSRT_GROUP, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Resource group subtable */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Length, AcpiDmTableInfoCsrt0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Shared info subtable (One per resource group) */

        SubOffset = sizeof (ACPI_CSRT_GROUP);
        SharedInfoTable = ACPI_ADD_PTR (ACPI_CSRT_SHARED_INFO, Table,
            Offset + SubOffset);

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset + SubOffset, SharedInfoTable,
            sizeof (ACPI_CSRT_SHARED_INFO), AcpiDmTableInfoCsrt1);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        SubOffset += Subtable->SharedInfoLength;

        /* Sub-Subtables (Resource Descriptors) */

        SubSubtable = ACPI_ADD_PTR (ACPI_CSRT_DESCRIPTOR, Table,
            Offset + SubOffset);

        while ((SubOffset < Subtable->Length) &&
              ((Offset + SubOffset) < Table->Length))
        {
            AcpiOsPrintf ("\n");
            Status = AcpiDmDumpTable (Length, Offset + SubOffset, SubSubtable,
                SubSubtable->Length, AcpiDmTableInfoCsrt2);
            if (ACPI_FAILURE (Status))
            {
                return;
            }

            SubSubOffset = sizeof (ACPI_CSRT_DESCRIPTOR);

            /* Resource-specific info buffer */

            InfoLength = SubSubtable->Length - SubSubOffset;
            if (InfoLength)
            {
                Status = AcpiDmDumpTable (Length,
                    Offset + SubOffset + SubSubOffset, Table,
                    InfoLength, AcpiDmTableInfoCsrt2a);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }
                SubSubOffset += InfoLength;
            }

            /* Point to next sub-subtable */

            SubOffset += SubSubtable->Length;
            SubSubtable = ACPI_ADD_PTR (ACPI_CSRT_DESCRIPTOR, SubSubtable,
                SubSubtable->Length);
        }

        /* Point to next subtable */

        Offset += Subtable->Length;
        Subtable = ACPI_ADD_PTR (ACPI_CSRT_GROUP, Subtable,
            Subtable->Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpDbg2
 *
 * PARAMETERS:  Table               - A DBG2 table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a DBG2. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpDbg2 (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_DBG2_DEVICE        *Subtable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_DBG2);
    UINT32                  i;
    UINT32                  ArrayOffset;
    UINT32                  AbsoluteOffset;
    UINT8                   *Array;


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoDbg2);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_DBG2_DEVICE, Table, Offset);
    while (Offset < Table->Length)
    {
        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Length, AcpiDmTableInfoDbg2Device);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Dump the BaseAddress array */

        for (i = 0; i < Subtable->RegisterCount; i++)
        {
            ArrayOffset = Subtable->BaseAddressOffset +
                (sizeof (ACPI_GENERIC_ADDRESS) * i);
            AbsoluteOffset = Offset + ArrayOffset;
            Array = (UINT8 *) Subtable + ArrayOffset;

            Status = AcpiDmDumpTable (Length, AbsoluteOffset, Array,
                Subtable->Length, AcpiDmTableInfoDbg2Addr);
            if (ACPI_FAILURE (Status))
            {
                return;
            }
        }

        /* Dump the AddressSize array */

        for (i = 0; i < Subtable->RegisterCount; i++)
        {
            ArrayOffset = Subtable->AddressSizeOffset +
                (sizeof (UINT32) * i);
            AbsoluteOffset = Offset + ArrayOffset;
            Array = (UINT8 *) Subtable + ArrayOffset;

            Status = AcpiDmDumpTable (Length, AbsoluteOffset, Array,
                Subtable->Length, AcpiDmTableInfoDbg2Size);
            if (ACPI_FAILURE (Status))
            {
                return;
            }
        }

        /* Dump the Namestring (required) */

        AcpiOsPrintf ("\n");
        ArrayOffset = Subtable->NamepathOffset;
        AbsoluteOffset = Offset + ArrayOffset;
        Array = (UINT8 *) Subtable + ArrayOffset;

        Status = AcpiDmDumpTable (Length, AbsoluteOffset, Array,
            Subtable->Length, AcpiDmTableInfoDbg2Name);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Dump the OemData (optional) */

        if (Subtable->OemDataOffset)
        {
            Status = AcpiDmDumpTable (Length, Offset + Subtable->OemDataOffset,
                Table, Subtable->OemDataLength,
                AcpiDmTableInfoDbg2OemData);
            if (ACPI_FAILURE (Status))
            {
                return;
            }
        }

        /* Point to next subtable */

        Offset += Subtable->Length;
        Subtable = ACPI_ADD_PTR (ACPI_DBG2_DEVICE, Subtable,
            Subtable->Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpDmar
 *
 * PARAMETERS:  Table               - A DMAR table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a DMAR. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpDmar (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_DMAR_HEADER        *Subtable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_DMAR);
    ACPI_DMTABLE_INFO       *InfoTable;
    ACPI_DMAR_DEVICE_SCOPE  *ScopeTable;
    UINT32                  ScopeOffset;
    UINT8                   *PciPath;
    UINT32                  PathOffset;


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoDmar);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_DMAR_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common subtable header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Length, AcpiDmTableInfoDmarHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        AcpiOsPrintf ("\n");

        switch (Subtable->Type)
        {
        case ACPI_DMAR_TYPE_HARDWARE_UNIT:

            InfoTable = AcpiDmTableInfoDmar0;
            ScopeOffset = sizeof (ACPI_DMAR_HARDWARE_UNIT);
            break;

        case ACPI_DMAR_TYPE_RESERVED_MEMORY:

            InfoTable = AcpiDmTableInfoDmar1;
            ScopeOffset = sizeof (ACPI_DMAR_RESERVED_MEMORY);
            break;

        case ACPI_DMAR_TYPE_ROOT_ATS:

            InfoTable = AcpiDmTableInfoDmar2;
            ScopeOffset = sizeof (ACPI_DMAR_ATSR);
            break;

        case ACPI_DMAR_TYPE_HARDWARE_AFFINITY:

            InfoTable = AcpiDmTableInfoDmar3;
            ScopeOffset = sizeof (ACPI_DMAR_RHSA);
            break;

        case ACPI_DMAR_TYPE_NAMESPACE:

            InfoTable = AcpiDmTableInfoDmar4;
            ScopeOffset = sizeof (ACPI_DMAR_ANDD);
            break;

        default:

            AcpiOsPrintf ("\n**** Unknown DMAR subtable type 0x%X\n\n",
                Subtable->Type);
            return;
        }

        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /*
         * Dump the optional device scope entries
         */
        if ((Subtable->Type == ACPI_DMAR_TYPE_HARDWARE_AFFINITY) ||
            (Subtable->Type == ACPI_DMAR_TYPE_NAMESPACE))
        {
            /* These types do not support device scopes */

            goto NextSubtable;
        }

        ScopeTable = ACPI_ADD_PTR (ACPI_DMAR_DEVICE_SCOPE, Subtable, ScopeOffset);
        while (ScopeOffset < Subtable->Length)
        {
            AcpiOsPrintf ("\n");
            Status = AcpiDmDumpTable (Length, Offset + ScopeOffset, ScopeTable,
                ScopeTable->Length, AcpiDmTableInfoDmarScope);
            if (ACPI_FAILURE (Status))
            {
                return;
            }
            AcpiOsPrintf ("\n");

            /* Dump the PCI Path entries for this device scope */

            PathOffset = sizeof (ACPI_DMAR_DEVICE_SCOPE); /* Path entries start at this offset */

            PciPath = ACPI_ADD_PTR (UINT8, ScopeTable,
                sizeof (ACPI_DMAR_DEVICE_SCOPE));

            while (PathOffset < ScopeTable->Length)
            {
                AcpiDmLineHeader ((PathOffset + ScopeOffset + Offset), 2,
                    "PCI Path");
                AcpiOsPrintf ("%2.2X,%2.2X\n", PciPath[0], PciPath[1]);

                /* Point to next PCI Path entry */

                PathOffset += 2;
                PciPath += 2;
                AcpiOsPrintf ("\n");
            }

            /* Point to next device scope entry */

            ScopeOffset += ScopeTable->Length;
            ScopeTable = ACPI_ADD_PTR (ACPI_DMAR_DEVICE_SCOPE,
                ScopeTable, ScopeTable->Length);
        }

NextSubtable:
        /* Point to next subtable */

        Offset += Subtable->Length;
        Subtable = ACPI_ADD_PTR (ACPI_DMAR_HEADER, Subtable,
            Subtable->Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpDrtm
 *
 * PARAMETERS:  Table               - A DRTM table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a DRTM.
 *
 ******************************************************************************/

void
AcpiDmDumpDrtm (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    UINT32                  Offset;
    ACPI_DRTM_VTABLE_LIST   *DrtmVtl;
    ACPI_DRTM_RESOURCE_LIST *DrtmRl;
    ACPI_DRTM_DPS_ID        *DrtmDps;
    UINT32                  Count;


    /* Main table */

    Status = AcpiDmDumpTable (Table->Length, 0, Table, 0,
        AcpiDmTableInfoDrtm);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    Offset = sizeof (ACPI_TABLE_DRTM);

    /* Sub-tables */

    /* Dump ValidatedTable length */

    DrtmVtl = ACPI_ADD_PTR (ACPI_DRTM_VTABLE_LIST, Table, Offset);
    AcpiOsPrintf ("\n");
    Status = AcpiDmDumpTable (Table->Length, Offset,
        DrtmVtl, ACPI_OFFSET (ACPI_DRTM_VTABLE_LIST, ValidatedTables),
        AcpiDmTableInfoDrtm0);
    if (ACPI_FAILURE (Status))
    {
            return;
    }

    Offset += ACPI_OFFSET (ACPI_DRTM_VTABLE_LIST, ValidatedTables);

    /* Dump Validated table addresses */

    Count = 0;
    while ((Offset < Table->Length) &&
            (DrtmVtl->ValidatedTableCount > Count))
    {
        Status = AcpiDmDumpTable (Table->Length, Offset,
            ACPI_ADD_PTR (void, Table, Offset), sizeof (UINT64),
            AcpiDmTableInfoDrtm0a);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        Offset += sizeof (UINT64);
        Count++;
    }

    /* Dump ResourceList length */

    DrtmRl = ACPI_ADD_PTR (ACPI_DRTM_RESOURCE_LIST, Table, Offset);
    AcpiOsPrintf ("\n");
    Status = AcpiDmDumpTable (Table->Length, Offset,
        DrtmRl, ACPI_OFFSET (ACPI_DRTM_RESOURCE_LIST, Resources),
        AcpiDmTableInfoDrtm1);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    Offset += ACPI_OFFSET (ACPI_DRTM_RESOURCE_LIST, Resources);

    /* Dump the Resource List */

    Count = 0;
    while ((Offset < Table->Length) &&
           (DrtmRl->ResourceCount > Count))
    {
        Status = AcpiDmDumpTable (Table->Length, Offset,
            ACPI_ADD_PTR (void, Table, Offset),
            sizeof (ACPI_DRTM_RESOURCE), AcpiDmTableInfoDrtm1a);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        Offset += sizeof (ACPI_DRTM_RESOURCE);
        Count++;
    }

    /* Dump DPS */

    DrtmDps = ACPI_ADD_PTR (ACPI_DRTM_DPS_ID, Table, Offset);
    AcpiOsPrintf ("\n");
    (void) AcpiDmDumpTable (Table->Length, Offset,
        DrtmDps, sizeof (ACPI_DRTM_DPS_ID), AcpiDmTableInfoDrtm2);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpEinj
 *
 * PARAMETERS:  Table               - A EINJ table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a EINJ. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpEinj (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_WHEA_HEADER        *Subtable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_EINJ);


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoEinj);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_WHEA_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            sizeof (ACPI_WHEA_HEADER), AcpiDmTableInfoEinj0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to next subtable (each subtable is of fixed length) */

        Offset += sizeof (ACPI_WHEA_HEADER);
        Subtable = ACPI_ADD_PTR (ACPI_WHEA_HEADER, Subtable,
            sizeof (ACPI_WHEA_HEADER));
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpErst
 *
 * PARAMETERS:  Table               - A ERST table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a ERST. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpErst (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_WHEA_HEADER        *Subtable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_ERST);


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoErst);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_WHEA_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            sizeof (ACPI_WHEA_HEADER), AcpiDmTableInfoErst0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to next subtable (each subtable is of fixed length) */

        Offset += sizeof (ACPI_WHEA_HEADER);
        Subtable = ACPI_ADD_PTR (ACPI_WHEA_HEADER, Subtable,
            sizeof (ACPI_WHEA_HEADER));
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpFpdt
 *
 * PARAMETERS:  Table               - A FPDT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a FPDT. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpFpdt (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_FPDT_HEADER        *Subtable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_FPDT);
    ACPI_DMTABLE_INFO       *InfoTable;


    /* There is no main table (other than the standard ACPI header) */

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_FPDT_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common subtable header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Length, AcpiDmTableInfoFpdtHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        switch (Subtable->Type)
        {
        case ACPI_FPDT_TYPE_BOOT:

            InfoTable = AcpiDmTableInfoFpdt0;
            break;

        case ACPI_FPDT_TYPE_S3PERF:

            InfoTable = AcpiDmTableInfoFpdt1;
            break;

        default:

            AcpiOsPrintf ("\n**** Unknown FPDT subtable type 0x%X\n\n",
                Subtable->Type);

            /* Attempt to continue */

            if (!Subtable->Length)
            {
                AcpiOsPrintf ("Invalid zero length subtable\n");
                return;
            }
            goto NextSubtable;
        }

        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

NextSubtable:
        /* Point to next subtable */

        Offset += Subtable->Length;
        Subtable = ACPI_ADD_PTR (ACPI_FPDT_HEADER, Subtable,
            Subtable->Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpGtdt
 *
 * PARAMETERS:  Table               - A GTDT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a GTDT. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpGtdt (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_GTDT_HEADER        *Subtable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_GTDT);
    ACPI_DMTABLE_INFO       *InfoTable;
    UINT32                  SubtableLength;
    UINT32                  GtCount;
    ACPI_GTDT_TIMER_ENTRY   *GtxTable;


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoGtdt);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Rev 3 fields */

    Subtable = ACPI_ADD_PTR (ACPI_GTDT_HEADER, Table, Offset);

    if (Table->Revision > 2)
    {
        SubtableLength = sizeof (ACPI_GTDT_EL2);
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            SubtableLength, AcpiDmTableInfoGtdtEl2);
        if (ACPI_FAILURE (Status))
        {
            return;
        }
        Offset += SubtableLength;
    }

    Subtable = ACPI_ADD_PTR (ACPI_GTDT_HEADER, Table, Offset);

    /* Subtables */

    while (Offset < Table->Length)
    {
        /* Common subtable header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Length, AcpiDmTableInfoGtdtHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        GtCount = 0;
        switch (Subtable->Type)
        {
        case ACPI_GTDT_TYPE_TIMER_BLOCK:

            SubtableLength = sizeof (ACPI_GTDT_TIMER_BLOCK);
            GtCount = (ACPI_CAST_PTR (ACPI_GTDT_TIMER_BLOCK,
                Subtable))->TimerCount;

            InfoTable = AcpiDmTableInfoGtdt0;
            break;

        case ACPI_GTDT_TYPE_WATCHDOG:

            SubtableLength = sizeof (ACPI_GTDT_WATCHDOG);

            InfoTable = AcpiDmTableInfoGtdt1;
            break;

        default:

            /* Cannot continue on unknown type - no length */

            AcpiOsPrintf ("\n**** Unknown GTDT subtable type 0x%X\n",
                Subtable->Type);
            return;
        }

        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to end of current subtable (each subtable above is of fixed length) */

        Offset += SubtableLength;

        /* If there are any Gt Timer Blocks from above, dump them now */

        if (GtCount)
        {
            GtxTable = ACPI_ADD_PTR (
                ACPI_GTDT_TIMER_ENTRY, Subtable, SubtableLength);
            SubtableLength += GtCount * sizeof (ACPI_GTDT_TIMER_ENTRY);

            while (GtCount)
            {
                AcpiOsPrintf ("\n");
                Status = AcpiDmDumpTable (Length, Offset, GtxTable,
                    sizeof (ACPI_GTDT_TIMER_ENTRY), AcpiDmTableInfoGtdt0a);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }
                Offset += sizeof (ACPI_GTDT_TIMER_ENTRY);
                GtxTable++;
                GtCount--;
            }
        }

        /* Point to next subtable */

        Subtable = ACPI_ADD_PTR (ACPI_GTDT_HEADER, Subtable, SubtableLength);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpHest
 *
 * PARAMETERS:  Table               - A HEST table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a HEST. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpHest (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_HEST_HEADER        *Subtable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_HEST);
    ACPI_DMTABLE_INFO       *InfoTable;
    UINT32                  SubtableLength;
    UINT32                  BankCount;
    ACPI_HEST_IA_ERROR_BANK *BankTable;


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoHest);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_HEST_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        BankCount = 0;
        switch (Subtable->Type)
        {
        case ACPI_HEST_TYPE_IA32_CHECK:

            InfoTable = AcpiDmTableInfoHest0;
            SubtableLength = sizeof (ACPI_HEST_IA_MACHINE_CHECK);
            BankCount = (ACPI_CAST_PTR (ACPI_HEST_IA_MACHINE_CHECK,
                Subtable))->NumHardwareBanks;
            break;

        case ACPI_HEST_TYPE_IA32_CORRECTED_CHECK:

            InfoTable = AcpiDmTableInfoHest1;
            SubtableLength = sizeof (ACPI_HEST_IA_CORRECTED);
            BankCount = (ACPI_CAST_PTR (ACPI_HEST_IA_CORRECTED,
                Subtable))->NumHardwareBanks;
            break;

        case ACPI_HEST_TYPE_IA32_NMI:

            InfoTable = AcpiDmTableInfoHest2;
            SubtableLength = sizeof (ACPI_HEST_IA_NMI);
            break;

        case ACPI_HEST_TYPE_AER_ROOT_PORT:

            InfoTable = AcpiDmTableInfoHest6;
            SubtableLength = sizeof (ACPI_HEST_AER_ROOT);
            break;

        case ACPI_HEST_TYPE_AER_ENDPOINT:

            InfoTable = AcpiDmTableInfoHest7;
            SubtableLength = sizeof (ACPI_HEST_AER);
            break;

        case ACPI_HEST_TYPE_AER_BRIDGE:

            InfoTable = AcpiDmTableInfoHest8;
            SubtableLength = sizeof (ACPI_HEST_AER_BRIDGE);
            break;

        case ACPI_HEST_TYPE_GENERIC_ERROR:

            InfoTable = AcpiDmTableInfoHest9;
            SubtableLength = sizeof (ACPI_HEST_GENERIC);
            break;

        case ACPI_HEST_TYPE_GENERIC_ERROR_V2:

            InfoTable = AcpiDmTableInfoHest10;
            SubtableLength = sizeof (ACPI_HEST_GENERIC_V2);
            break;

        case ACPI_HEST_TYPE_IA32_DEFERRED_CHECK:

            InfoTable = AcpiDmTableInfoHest11;
            SubtableLength = sizeof (ACPI_HEST_IA_DEFERRED_CHECK);
            BankCount = (ACPI_CAST_PTR (ACPI_HEST_IA_DEFERRED_CHECK,
                Subtable))->NumHardwareBanks;
            break;

        default:

            /* Cannot continue on unknown type - no length */

            AcpiOsPrintf ("\n**** Unknown HEST subtable type 0x%X\n",
                Subtable->Type);
            return;
        }

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            SubtableLength, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to end of current subtable (each subtable above is of fixed length) */

        Offset += SubtableLength;

        /* If there are any (fixed-length) Error Banks from above, dump them now */

        if (BankCount)
        {
            BankTable = ACPI_ADD_PTR (ACPI_HEST_IA_ERROR_BANK, Subtable,
                SubtableLength);
            SubtableLength += BankCount * sizeof (ACPI_HEST_IA_ERROR_BANK);

            while (BankCount)
            {
                AcpiOsPrintf ("\n");
                Status = AcpiDmDumpTable (Length, Offset, BankTable,
                    sizeof (ACPI_HEST_IA_ERROR_BANK), AcpiDmTableInfoHestBank);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }

                Offset += sizeof (ACPI_HEST_IA_ERROR_BANK);
                BankTable++;
                BankCount--;
            }
        }

        /* Point to next subtable */

        Subtable = ACPI_ADD_PTR (ACPI_HEST_HEADER, Subtable, SubtableLength);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpHmat
 *
 * PARAMETERS:  Table               - A HMAT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a HMAT.
 *
 ******************************************************************************/

void
AcpiDmDumpHmat (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_HMAT_STRUCTURE     *HmatStruct;
    ACPI_HMAT_LOCALITY      *HmatLocality;
    ACPI_HMAT_CACHE         *HmatCache;
    UINT32                  Offset;
    UINT32                  SubtableOffset;
    UINT32                  Length;
    ACPI_DMTABLE_INFO       *InfoTable;
    UINT32                  i, j;


    /* Main table */

    Status = AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoHmat);
    if (ACPI_FAILURE (Status))
    {
        return;
    }
    Offset = sizeof (ACPI_TABLE_HMAT);

    while (Offset < Table->Length)
    {
        AcpiOsPrintf ("\n");
        SubtableOffset = 0;

        /* Dump HMAT structure header */

        HmatStruct = ACPI_ADD_PTR (ACPI_HMAT_STRUCTURE, Table, Offset);
        if (HmatStruct->Length < sizeof (ACPI_HMAT_STRUCTURE))
        {
            AcpiOsPrintf ("Invalid HMAT structure length\n");
            return;
        }
        Status = AcpiDmDumpTable (Table->Length, Offset, HmatStruct,
            HmatStruct->Length, AcpiDmTableInfoHmatHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        switch (HmatStruct->Type)
        {
        case ACPI_HMAT_TYPE_ADDRESS_RANGE:

            InfoTable = AcpiDmTableInfoHmat0;
            Length = sizeof (ACPI_HMAT_PROXIMITY_DOMAIN);
            break;

        case ACPI_HMAT_TYPE_LOCALITY:

            InfoTable = AcpiDmTableInfoHmat1;
            Length = sizeof (ACPI_HMAT_LOCALITY);
            break;

        case ACPI_HMAT_TYPE_CACHE:

            InfoTable = AcpiDmTableInfoHmat2;
            Length = sizeof (ACPI_HMAT_CACHE);
            break;

        default:

            AcpiOsPrintf ("\n**** Unknown HMAT structure type 0x%X\n",
                HmatStruct->Type);

            /* Attempt to continue */

            goto NextSubtable;
        }

        /* Dump HMAT structure body */

        if (HmatStruct->Length < Length)
        {
            AcpiOsPrintf ("Invalid HMAT structure length\n");
            return;
        }
        Status = AcpiDmDumpTable (Table->Length, Offset, HmatStruct,
            HmatStruct->Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Dump HMAT structure additionals */

        switch (HmatStruct->Type)
        {
        case ACPI_HMAT_TYPE_LOCALITY:

            HmatLocality = ACPI_CAST_PTR (ACPI_HMAT_LOCALITY, HmatStruct);
            SubtableOffset = sizeof (ACPI_HMAT_LOCALITY);

            /* Dump initiator proximity domains */

            if ((UINT32)(HmatStruct->Length - SubtableOffset) <
                (UINT32)(HmatLocality->NumberOfInitiatorPDs * 4))
            {
                AcpiOsPrintf ("Invalid initiator proximity domain number\n");
                return;
            }
            for (i = 0; i < HmatLocality->NumberOfInitiatorPDs; i++)
            {
                Status = AcpiDmDumpTable (Table->Length, Offset + SubtableOffset,
                    ACPI_ADD_PTR (ACPI_HMAT_STRUCTURE, HmatStruct, SubtableOffset),
                    4, AcpiDmTableInfoHmat1a);
                SubtableOffset += 4;
            }

            /* Dump target proximity domains */

            if ((UINT32)(HmatStruct->Length - SubtableOffset) <
                (UINT32)(HmatLocality->NumberOfTargetPDs * 4))
            {
                AcpiOsPrintf ("Invalid target proximity domain number\n");
                return;
            }
            for (i = 0; i < HmatLocality->NumberOfTargetPDs; i++)
            {
                Status = AcpiDmDumpTable (Table->Length, Offset + SubtableOffset,
                    ACPI_ADD_PTR (ACPI_HMAT_STRUCTURE, HmatStruct, SubtableOffset),
                    4, AcpiDmTableInfoHmat1b);
                SubtableOffset += 4;
            }

            /* Dump latency/bandwidth entris */

            if ((UINT32)(HmatStruct->Length - SubtableOffset) <
                (UINT32)(HmatLocality->NumberOfInitiatorPDs *
                         HmatLocality->NumberOfTargetPDs * 2))
            {
                AcpiOsPrintf ("Invalid latency/bandwidth entry number\n");
                return;
            }
            for (i = 0; i < HmatLocality->NumberOfInitiatorPDs; i++)
            {
                for (j = 0; j < HmatLocality->NumberOfTargetPDs; j++)
                {
                    Status = AcpiDmDumpTable (Table->Length, Offset + SubtableOffset,
                        ACPI_ADD_PTR (ACPI_HMAT_STRUCTURE, HmatStruct, SubtableOffset),
                        2, AcpiDmTableInfoHmat1c);
                    SubtableOffset += 2;
                }
            }
            break;

        case ACPI_HMAT_TYPE_CACHE:

            HmatCache = ACPI_CAST_PTR (ACPI_HMAT_CACHE, HmatStruct);
            SubtableOffset = sizeof (ACPI_HMAT_CACHE);

            /* Dump SMBIOS handles */

            if ((UINT32)(HmatStruct->Length - SubtableOffset) <
                (UINT32)(HmatCache->NumberOfSMBIOSHandles * 2))
            {
                AcpiOsPrintf ("Invalid SMBIOS handle number\n");
                return;
            }
            for (i = 0; i < HmatCache->NumberOfSMBIOSHandles; i++)
            {
                Status = AcpiDmDumpTable (Table->Length, Offset + SubtableOffset,
                    ACPI_ADD_PTR (ACPI_HMAT_STRUCTURE, HmatStruct, SubtableOffset),
                    2, AcpiDmTableInfoHmat2a);
                SubtableOffset += 2;
            }
            break;

        default:

            break;
        }

NextSubtable:
        /* Point to next HMAT structure subtable */

        Offset += (HmatStruct->Length);
    }
}

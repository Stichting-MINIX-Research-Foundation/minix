/******************************************************************************
 *
 * Module Name: dmtbdump - Dump ACPI data tables that contain no AML code
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
        ACPI_MODULE_NAME    ("dmtbdump")


/* Local prototypes */

static void
AcpiDmValidateFadtLength (
    UINT32                  Revision,
    UINT32                  Length);


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpBuffer
 *
 * PARAMETERS:  Table               - ACPI Table or subtable
 *              BufferOffset        - Offset of buffer from Table above
 *              Length              - Length of the buffer
 *              AbsoluteOffset      - Offset of buffer in the main ACPI table
 *              Header              - Name of the buffer field (printed on the
 *                                    first line only.)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of an arbitrary length data buffer (in the
 *              disassembler output format.)
 *
 ******************************************************************************/

void
AcpiDmDumpBuffer (
    void                    *Table,
    UINT32                  BufferOffset,
    UINT32                  Length,
    UINT32                  AbsoluteOffset,
    char                    *Header)
{
    UINT8                   *Buffer;
    UINT32                  i;


    if (!Length)
    {
        return;
    }

    Buffer = ACPI_CAST_PTR (UINT8, Table) + BufferOffset;
    i = 0;

    while (i < Length)
    {
        if (!(i % 16))
        {
            /* Insert a backslash - line continuation character */

            if (Length > 16)
            {
                AcpiOsPrintf ("\\\n    ");
            }
        }

        AcpiOsPrintf ("%.02X ", *Buffer);
        i++;
        Buffer++;
        AbsoluteOffset++;
    }

    AcpiOsPrintf ("\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpUnicode
 *
 * PARAMETERS:  Table               - ACPI Table or subtable
 *              BufferOffset        - Offset of buffer from Table above
 *              ByteLength          - Length of the buffer
 *
 * RETURN:      None
 *
 * DESCRIPTION: Validate and dump the contents of a buffer that contains
 *              unicode data. The output is a standard ASCII string. If it
 *              appears that the data is not unicode, the buffer is dumped
 *              as hex characters.
 *
 ******************************************************************************/

void
AcpiDmDumpUnicode (
    void                    *Table,
    UINT32                  BufferOffset,
    UINT32                  ByteLength)
{
    UINT8                   *Buffer;
    UINT32                  Length;
    UINT32                  i;


    Buffer = ((UINT8 *) Table) + BufferOffset;
    Length = ByteLength - 2; /* Last two bytes are the null terminator */

    /* Ensure all low bytes are entirely printable ASCII */

    for (i = 0; i < Length; i += 2)
    {
        if (!isprint (Buffer[i]))
        {
            goto DumpRawBuffer;
        }
    }

    /* Ensure all high bytes are zero */

    for (i = 1; i < Length; i += 2)
    {
        if (Buffer[i])
        {
            goto DumpRawBuffer;
        }
    }

    /* Dump the buffer as a normal string */

    AcpiOsPrintf ("\"");
    for (i = 0; i < Length; i += 2)
    {
        AcpiOsPrintf ("%c", Buffer[i]);
    }

    AcpiOsPrintf ("\"\n");
    return;

DumpRawBuffer:
    AcpiDmDumpBuffer (Table, BufferOffset, ByteLength,
        BufferOffset, NULL);
    AcpiOsPrintf ("\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpRsdp
 *
 * PARAMETERS:  Table               - A RSDP
 *
 * RETURN:      Length of the table (there is not always a length field,
 *              use revision or length if available (ACPI 2.0+))
 *
 * DESCRIPTION: Format the contents of a RSDP
 *
 ******************************************************************************/

UINT32
AcpiDmDumpRsdp (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_TABLE_RSDP         *Rsdp = ACPI_CAST_PTR (ACPI_TABLE_RSDP, Table);
    UINT32                  Length = sizeof (ACPI_RSDP_COMMON);
    UINT8                   Checksum;
    ACPI_STATUS             Status;


    /* Dump the common ACPI 1.0 portion */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoRsdp1);
    if (ACPI_FAILURE (Status))
    {
        return (Length);
    }

    /* Validate the first checksum */

    Checksum = AcpiDmGenerateChecksum (Rsdp, sizeof (ACPI_RSDP_COMMON),
        Rsdp->Checksum);
    if (Checksum != Rsdp->Checksum)
    {
        AcpiOsPrintf ("/* Incorrect Checksum above, should be 0x%2.2X */\n",
            Checksum);
    }

    /* The RSDP for ACPI 2.0+ contains more data and has a Length field */

    if (Rsdp->Revision > 0)
    {
        Length = Rsdp->Length;
        Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoRsdp2);
        if (ACPI_FAILURE (Status))
        {
            return (Length);
        }

        /* Validate the extended checksum over entire RSDP */

        Checksum = AcpiDmGenerateChecksum (Rsdp, sizeof (ACPI_TABLE_RSDP),
            Rsdp->ExtendedChecksum);
        if (Checksum != Rsdp->ExtendedChecksum)
        {
            AcpiOsPrintf (
                "/* Incorrect Extended Checksum above, should be 0x%2.2X */\n",
                Checksum);
        }
    }

    return (Length);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpRsdt
 *
 * PARAMETERS:  Table               - A RSDT
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a RSDT
 *
 ******************************************************************************/

void
AcpiDmDumpRsdt (
    ACPI_TABLE_HEADER       *Table)
{
    UINT32                  *Array;
    UINT32                  Entries;
    UINT32                  Offset;
    UINT32                  i;


    /* Point to start of table pointer array */

    Array = ACPI_CAST_PTR (ACPI_TABLE_RSDT, Table)->TableOffsetEntry;
    Offset = sizeof (ACPI_TABLE_HEADER);

    /* RSDT uses 32-bit pointers */

    Entries = (Table->Length - sizeof (ACPI_TABLE_HEADER)) / sizeof (UINT32);

    for (i = 0; i < Entries; i++)
    {
        AcpiDmLineHeader2 (Offset, sizeof (UINT32), "ACPI Table Address", i);
        AcpiOsPrintf ("%8.8X\n", Array[i]);
        Offset += sizeof (UINT32);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpXsdt
 *
 * PARAMETERS:  Table               - A XSDT
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a XSDT
 *
 ******************************************************************************/

void
AcpiDmDumpXsdt (
    ACPI_TABLE_HEADER       *Table)
{
    UINT64                  *Array;
    UINT32                  Entries;
    UINT32                  Offset;
    UINT32                  i;


    /* Point to start of table pointer array */

    Array = ACPI_CAST_PTR (ACPI_TABLE_XSDT, Table)->TableOffsetEntry;
    Offset = sizeof (ACPI_TABLE_HEADER);

    /* XSDT uses 64-bit pointers */

    Entries = (Table->Length - sizeof (ACPI_TABLE_HEADER)) / sizeof (UINT64);

    for (i = 0; i < Entries; i++)
    {
        AcpiDmLineHeader2 (Offset, sizeof (UINT64), "ACPI Table Address", i);
        AcpiOsPrintf ("%8.8X%8.8X\n", ACPI_FORMAT_UINT64 (Array[i]));
        Offset += sizeof (UINT64);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpFadt
 *
 * PARAMETERS:  Table               - A FADT
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a FADT
 *
 * NOTE:        We cannot depend on the FADT version to indicate the actual
 *              contents of the FADT because of BIOS bugs. The table length
 *              is the only reliable indicator.
 *
 ******************************************************************************/

void
AcpiDmDumpFadt (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;


    /* Always dump the minimum FADT revision 1 fields (ACPI 1.0) */

    Status = AcpiDmDumpTable (Table->Length, 0, Table, 0,
        AcpiDmTableInfoFadt1);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Check for FADT revision 2 fields (ACPI 1.0B MS extensions) */

    if ((Table->Length > ACPI_FADT_V1_SIZE) &&
        (Table->Length <= ACPI_FADT_V2_SIZE))
    {
        Status = AcpiDmDumpTable (Table->Length, 0, Table, 0,
            AcpiDmTableInfoFadt2);
        if (ACPI_FAILURE (Status))
        {
            return;
        }
    }

    /* Check for FADT revision 3/4 fields and up (ACPI 2.0+ extended data) */

    else if (Table->Length > ACPI_FADT_V2_SIZE)
    {
        Status = AcpiDmDumpTable (Table->Length, 0, Table, 0,
            AcpiDmTableInfoFadt3);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Check for FADT revision 5 fields and up (ACPI 5.0+) */

        if (Table->Length > ACPI_FADT_V3_SIZE)
        {
            Status = AcpiDmDumpTable (Table->Length, 0, Table, 0,
                AcpiDmTableInfoFadt5);
            if (ACPI_FAILURE (Status))
            {
                return;
            }
        }

        /* Check for FADT revision 6 fields and up (ACPI 6.0+) */

        if (Table->Length > ACPI_FADT_V3_SIZE)
        {
            Status = AcpiDmDumpTable (Table->Length, 0, Table, 0,
                AcpiDmTableInfoFadt6);
            if (ACPI_FAILURE (Status))
            {
                return;
            }
        }
    }

    /* Validate various fields in the FADT, including length */

    AcpiTbCreateLocalFadt (Table, Table->Length);

    /* Validate FADT length against the revision */

    AcpiDmValidateFadtLength (Table->Revision, Table->Length);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmValidateFadtLength
 *
 * PARAMETERS:  Revision            - FADT revision (Header->Revision)
 *              Length              - FADT length (Header->Length
 *
 * RETURN:      None
 *
 * DESCRIPTION: Check the FADT revision against the expected table length for
 *              that revision. Issue a warning if the length is not what was
 *              expected. This seems to be such a common BIOS bug that the
 *              FADT revision has been rendered virtually meaningless.
 *
 ******************************************************************************/

static void
AcpiDmValidateFadtLength (
    UINT32                  Revision,
    UINT32                  Length)
{
    UINT32                  ExpectedLength;


    switch (Revision)
    {
    case 0:

        AcpiOsPrintf ("// ACPI Warning: Invalid FADT revision: 0\n");
        return;

    case 1:

        ExpectedLength = ACPI_FADT_V1_SIZE;
        break;

    case 2:

        ExpectedLength = ACPI_FADT_V2_SIZE;
        break;

    case 3:
    case 4:

        ExpectedLength = ACPI_FADT_V3_SIZE;
        break;

    case 5:

        ExpectedLength = ACPI_FADT_V5_SIZE;
        break;

    default:

        return;
    }

    if (Length == ExpectedLength)
    {
        return;
    }

    AcpiOsPrintf (
        "\n// ACPI Warning: FADT revision %X does not match length: "
        "found %X expected %X\n",
        Revision, Length, ExpectedLength);
}

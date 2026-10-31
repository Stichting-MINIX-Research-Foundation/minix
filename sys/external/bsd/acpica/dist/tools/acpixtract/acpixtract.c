/******************************************************************************
 *
 * Module Name: acpixtract - Top level functions to convert ascii/hex
 *                           ACPI tables to the original binary tables
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

#include "acpixtract.h"


/******************************************************************************
 *
 * FUNCTION:    AxExtractTables
 *
 * PARAMETERS:  InputPathname       - Filename for input acpidump file
 *              Signature           - Requested ACPI signature to extract.
 *                                    NULL means extract ALL tables.
 *              MinimumInstances    - Min instances that are acceptable
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert text ACPI tables to binary
 *
 ******************************************************************************/

int
AxExtractTables (
    char                    *InputPathname,
    char                    *Signature,
    unsigned int            MinimumInstances)
{
    FILE                    *InputFile;
    FILE                    *OutputFile = NULL;
    int                     BytesConverted;
    int                     ThisTableBytesWritten = 0;
    unsigned int            FoundTable = 0;
    unsigned int            Instances = 0;
    unsigned int            ThisInstance;
    char                    ThisSignature[5];
    char                    UpperSignature[5];
    int                     Status = 0;
    unsigned int            State = AX_STATE_FIND_HEADER;


    /* Open input in text mode, output is in binary mode */

    InputFile = fopen (InputPathname, "r");
    if (!InputFile)
    {
        printf ("Could not open input file %s\n", InputPathname);
        return (-1);
    }

    if (!AxIsFileAscii (InputFile))
    {
        fclose (InputFile);
        return (-1);
    }

    if (Signature)
    {
        strncpy (UpperSignature, Signature, ACPI_NAMESEG_SIZE);
        AcpiUtStrupr (UpperSignature);

        /* Are there enough instances of the table to continue? */

        AxNormalizeSignature (UpperSignature);
        Instances = AxCountTableInstances (InputPathname, UpperSignature);

        if (Instances < MinimumInstances)
        {
            printf ("Table [%s] was not found in %s\n",
                UpperSignature, InputPathname);
            fclose (InputFile);
            return (0);             /* Don't abort */
        }

        if (Instances == 0)
        {
            fclose (InputFile);
            return (-1);
        }
    }

    /* Convert all instances of the table to binary */

    while (fgets (Gbl_LineBuffer, AX_LINE_BUFFER_SIZE, InputFile))
    {
        /*
         * Check up front if we have a header line of the form:
         * DSDT @ 0xdfffd0c0 (10999 bytes)
         */
        if (AX_IS_TABLE_BLOCK_HEADER &&
            (State == AX_STATE_EXTRACT_DATA))
        {
            /* End of previous table, start of new table */

            if (ThisTableBytesWritten)
            {
                printf (AX_TABLE_INFO_FORMAT, ThisSignature, ThisTableBytesWritten,
                    ThisTableBytesWritten, Gbl_OutputFilename);
            }
            else
            {
                Gbl_TableCount--;
            }

            State = AX_STATE_FIND_HEADER;
        }

        switch (State)
        {
        case AX_STATE_FIND_HEADER:

            if (!AxIsDataBlockHeader ())
            {
                continue;
            }

            ACPI_COPY_NAMESEG (ThisSignature, Gbl_LineBuffer);
            if (Signature)
            {
                /* Ignore signatures that don't match */

                if (!ACPI_COMPARE_NAMESEG (ThisSignature, UpperSignature))
                {
                    continue;
                }
            }

            /*
             * Get the instance number for this signature. Only the
             * SSDT and PSDT tables can have multiple instances.
             */
            ThisInstance = AxGetNextInstance (InputPathname, ThisSignature);

            /* Build an output filename and create/open the output file */

            if (ThisInstance > 0)
            {
                /* Add instance number to the output filename */

                sprintf (Gbl_OutputFilename, "%4.4s%u.dat",
                    ThisSignature, ThisInstance);
            }
            else
            {
                sprintf (Gbl_OutputFilename, "%4.4s.dat",
                    ThisSignature);
            }

            AcpiUtStrlwr (Gbl_OutputFilename);
            OutputFile = fopen (Gbl_OutputFilename, "w+b");
            if (!OutputFile)
            {
                printf ("Could not open output file %s\n",
                    Gbl_OutputFilename);
                fclose (InputFile);
                return (-1);
            }

            /*
             * Toss this block header of the form "<sig> @ <addr>" line
             * and move on to the actual data block
             */
            Gbl_TableCount++;
            FoundTable = 1;
            ThisTableBytesWritten = 0;
            State = AX_STATE_EXTRACT_DATA;
            continue;

        case AX_STATE_EXTRACT_DATA:

            if (!AxIsHexDataLine ())
            {
                continue;   /* Toss any lines that are not raw hex data */
            }

            /* Empty line or non-data line terminates the data block */

            BytesConverted = AxConvertAndWrite (OutputFile, ThisSignature);
            switch (BytesConverted)
            {
            case 0:

                State = AX_STATE_FIND_HEADER; /* No more data block lines */
                continue;

            case -1:

                Status = -1;
                goto CleanupAndExit; /* There was a write error */

            default: /* Normal case, get next line */

                ThisTableBytesWritten += BytesConverted;
                continue;
            }

        default:

            Status = -1;
            goto CleanupAndExit;
        }
    }

    if (!FoundTable)
    {
        printf ("No ACPI tables were found in %s\n", InputPathname);
    }


CleanupAndExit:

    if (State == AX_STATE_EXTRACT_DATA)
    {
        /* Received an input file EOF while extracting data */

        printf (AX_TABLE_INFO_FORMAT, ThisSignature, ThisTableBytesWritten,
            ThisTableBytesWritten, Gbl_OutputFilename);
    }

    if (OutputFile)
    {
        fclose (OutputFile);
    }

    fclose (InputFile);
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AxExtractToMultiAmlFile
 *
 * PARAMETERS:  InputPathname       - Filename for input acpidump file
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert all DSDT/SSDT tables to binary and append them all
 *              into a single output file. Used to simplify the loading of
 *              multiple/many SSDTs into a utility like acpiexec -- instead
 *              of creating many separate output files.
 *
 ******************************************************************************/

int
AxExtractToMultiAmlFile (
    char                    *InputPathname)
{
    FILE                    *InputFile;
    FILE                    *OutputFile;
    int                     Status = 0;
    int                     TotalBytesWritten = 0;
    int                     ThisTableBytesWritten = 0;
    unsigned int            BytesConverted;
    char                    ThisSignature[4];
    unsigned int            State = AX_STATE_FIND_HEADER;


    strcpy (Gbl_OutputFilename, AX_MULTI_TABLE_FILENAME);

    /* Open the input file in text mode */

    InputFile = fopen (InputPathname, "r");
    if (!InputFile)
    {
        printf ("Could not open input file %s\n", InputPathname);
        return (-1);
    }

    if (!AxIsFileAscii (InputFile))
    {
        fclose (InputFile);
        return (-1);
    }

    /* Open the output file in binary mode */

    OutputFile = fopen (Gbl_OutputFilename, "w+b");
    if (!OutputFile)
    {
        printf ("Could not open output file %s\n", Gbl_OutputFilename);
        fclose (InputFile);
        return (-1);
    }

    /* Convert the DSDT and all SSDTs to binary */

    while (fgets (Gbl_LineBuffer, AX_LINE_BUFFER_SIZE, InputFile))
    {
        /*
         * Check up front if we have a header line of the form:
         * DSDT @ 0xdfffd0c0 (10999 bytes)
         */
        if (AX_IS_TABLE_BLOCK_HEADER &&
            (State == AX_STATE_EXTRACT_DATA))
        {
            /* End of previous table, start of new table */

            if (ThisTableBytesWritten)
            {
                printf (AX_TABLE_INFO_FORMAT, ThisSignature, ThisTableBytesWritten,
                    ThisTableBytesWritten, Gbl_OutputFilename);
            }
            else
            {
                Gbl_TableCount--;
            }

            State = AX_STATE_FIND_HEADER;
        }

        switch (State)
        {
        case AX_STATE_FIND_HEADER:

            if (!AxIsDataBlockHeader ())
            {
                continue;
            }

            ACPI_COPY_NAMESEG (ThisSignature, Gbl_LineBuffer);

            /* Only want DSDT and SSDTs */

            if (!ACPI_COMPARE_NAMESEG (ThisSignature, ACPI_SIG_DSDT) &&
                !ACPI_COMPARE_NAMESEG (ThisSignature, ACPI_SIG_SSDT))
            {
                continue;
            }

            /*
             * Toss this block header of the form "<sig> @ <addr>" line
             * and move on to the actual data block
             */
            Gbl_TableCount++;
            ThisTableBytesWritten = 0;
            State = AX_STATE_EXTRACT_DATA;
            continue;

        case AX_STATE_EXTRACT_DATA:

            if (!AxIsHexDataLine ())
            {
                continue;   /* Toss any lines that are not raw hex data */
            }

            /* Empty line or non-data line terminates the data block */

            BytesConverted = AxConvertAndWrite (OutputFile, ThisSignature);
            switch (BytesConverted)
            {
            case 0:

                State = AX_STATE_FIND_HEADER; /* No more data block lines */
                continue;

            case -1:

                Status = -1;
                goto CleanupAndExit; /* There was a write error */

            default: /* Normal case, get next line */

                ThisTableBytesWritten += BytesConverted;
                TotalBytesWritten += BytesConverted;
                continue;
            }

        default:

            Status = -1;
            goto CleanupAndExit;
        }
    }


CleanupAndExit:

    if (State == AX_STATE_EXTRACT_DATA)
    {
        /* Received an input file EOF or error while writing data */

        printf (AX_TABLE_INFO_FORMAT, ThisSignature, ThisTableBytesWritten,
            ThisTableBytesWritten, Gbl_OutputFilename);
    }

    printf ("\n%u binary ACPI tables extracted and written to %s (%u bytes)\n",
        Gbl_TableCount, Gbl_OutputFilename, TotalBytesWritten);

    fclose (InputFile);
    fclose (OutputFile);
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AxListAllTables
 *
 * PARAMETERS:  InputPathname       - Filename for acpidump file
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Display info for all ACPI tables found in input. Does not
 *              perform an actual extraction of the tables.
 *
 ******************************************************************************/

int
AxListAllTables (
    char                    *InputPathname)
{
    FILE                    *InputFile;
    unsigned char           Header[48];
    UINT32                  ByteCount = 0;
    UINT32                  ThisLineByteCount;
    unsigned int            State = AX_STATE_FIND_HEADER;


    /* Open input in text mode, output is in binary mode */

    InputFile = fopen (InputPathname, "r");
    if (!InputFile)
    {
        printf ("Could not open input file %s\n", InputPathname);
        return (-1);
    }

    if (!AxIsFileAscii (InputFile))
    {
        fclose (InputFile);
        return (-1);
    }

    /* Info header */

    printf ("\n Signature  Length    Version Oem       Oem         "
        "Oem         Compiler Compiler\n");
    printf (  "                              Id        TableId     "
        "RevisionId  Name     Revision\n");
    printf (  " _________  __________  ____  ________  __________  "
        "__________  _______  __________\n\n");

    /* Dump the headers for all tables found in the input file */

    while (fgets (Gbl_LineBuffer, AX_LINE_BUFFER_SIZE, InputFile))
    {
        /* Ignore empty lines */

        if (AxIsEmptyLine (Gbl_LineBuffer))
        {
            continue;
        }

        /*
         * Check up front if we have a header line of the form:
         * DSDT @ 0xdfffd0c0 (10999 bytes)
         */
        if (AX_IS_TABLE_BLOCK_HEADER &&
            (State == AX_STATE_EXTRACT_DATA))
        {
            State = AX_STATE_FIND_HEADER;
        }

        switch (State)
        {
        case AX_STATE_FIND_HEADER:

            ByteCount = 0;
            if (!AxIsDataBlockHeader ())
            {
                continue;
            }

            State = AX_STATE_EXTRACT_DATA;
            continue;

        case AX_STATE_EXTRACT_DATA:

            /* Ignore any lines that don't look like a data line */

            if (!AxIsHexDataLine ())
            {
                continue;   /* Toss any lines that are not raw hex data */
            }

            /* Convert header to hex and display it */

            ThisLineByteCount = AxConvertToBinary (Gbl_LineBuffer,
                &Header[ByteCount]);
            if (ThisLineByteCount == EOF)
            {
                fclose (InputFile);
                return (-1);
            }

            ByteCount += ThisLineByteCount;
            if (ByteCount >= sizeof (ACPI_TABLE_HEADER))
            {
                AxDumpTableHeader (Header);
                State = AX_STATE_FIND_HEADER;
            }
            continue;

        default:
            break;
        }
    }

    printf ("\nFound %u ACPI tables in %s\n", Gbl_TableCount, InputPathname);
    fclose (InputFile);
    return (0);
}

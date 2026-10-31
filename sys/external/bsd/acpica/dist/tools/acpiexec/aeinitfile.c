/******************************************************************************
 *
 * Module Name: aeinitfile - Support for optional initialization file
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

#include "aecommon.h"
#include "acdispat.h"

#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("aeinitfile")


/* Local prototypes */

static void
AeEnterInitFileEntry (
    INIT_FILE_ENTRY         InitEntry,
    ACPI_WALK_STATE         *WalkState);


#define AE_FILE_BUFFER_SIZE     512

static char                 LineBuffer[AE_FILE_BUFFER_SIZE];
static char                 NameBuffer[AE_FILE_BUFFER_SIZE];
static char                 ValueBuffer[AE_FILE_BUFFER_SIZE];
static FILE                 *InitFile;


/******************************************************************************
 *
 * FUNCTION:    AeOpenInitializationFile
 *
 * PARAMETERS:  Filename            - Path to the init file
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Open the initialization file for the -fi option
 *
 *****************************************************************************/

int
AeOpenInitializationFile (
    char                    *Filename)
{

    InitFile = fopen (Filename, "r");
    if (!InitFile)
    {
        fprintf (stderr,
            "Could not open initialization file: %s\n", Filename);
        return (-1);
    }

    AcpiOsPrintf ("Opened initialization file [%s]\n", Filename);
    return (0);
}


/******************************************************************************
 *
 * FUNCTION:    AeProcessInitFile
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Read the initialization file and perform all namespace
 *              initializations. AcpiGbl_InitEntries will be used for region
 *              field initialization.
 *
 * NOTE:        The format of the file is multiple lines, each of format:
 *                  <ACPI-pathname> <Integer Value>
 *
 *****************************************************************************/

void
AeProcessInitFile(
    void)
{
    ACPI_WALK_STATE         *WalkState;
    int                     i;
    UINT64                  idx;
    ACPI_STATUS             Status;


    if (!InitFile)
    {
        return;
    }

    /* Create needed objects to be reused for each init entry */

    WalkState = AcpiDsCreateWalkState (0, NULL, NULL, NULL);
    NameBuffer[0] = '\\';

    while (fgets (LineBuffer, AE_FILE_BUFFER_SIZE, InitFile) != NULL)
    {
        ++AcpiGbl_InitFileLineCount;
    }
    rewind (InitFile);

    AcpiGbl_InitEntries =
        AcpiOsAllocate (sizeof (INIT_FILE_ENTRY) * AcpiGbl_InitFileLineCount);
    for (idx = 0; fgets (LineBuffer, AE_FILE_BUFFER_SIZE, InitFile); ++idx)
    {
        if (sscanf (LineBuffer, "%s %s\n",
                &NameBuffer[1], ValueBuffer) != 2)
        {
            goto CleanupAndExit;
        }

        /* Add a root prefix if not present in the string */

        i = 0;
        if (NameBuffer[1] == '\\')
        {
            i = 1;
        }

        AcpiGbl_InitEntries[idx].Name =
            AcpiOsAllocateZeroed (strnlen (NameBuffer + i, AE_FILE_BUFFER_SIZE) + 1);

        strcpy (AcpiGbl_InitEntries[idx].Name, NameBuffer + i);

        Status = AcpiUtStrtoul64 (ValueBuffer, &AcpiGbl_InitEntries[idx].Value);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("%s %s\n", ValueBuffer,
                AcpiFormatException (Status));
            goto CleanupAndExit;
        }

        AeEnterInitFileEntry (AcpiGbl_InitEntries[idx], WalkState);
    }

    /* Cleanup */

CleanupAndExit:
    fclose (InitFile);
    AcpiDsDeleteWalkState (WalkState);
}


/******************************************************************************
 *
 * FUNCTION:    AeInitFileEntry
 *
 * PARAMETERS:  InitEntry           - Entry of the init file
 *              WalkState           - Used for the Store operation
 *
 * RETURN:      None
 *
 * DESCRIPTION: Perform initialization of a single namespace object
 *
 *              Note: namespace of objects are limited to integers and region
 *              fields units of 8 bytes at this time.
 *
 *****************************************************************************/

static void
AeEnterInitFileEntry (
    INIT_FILE_ENTRY         InitEntry,
    ACPI_WALK_STATE         *WalkState)
{
    char                    *Pathname = InitEntry.Name;
    UINT64                  Value = InitEntry.Value;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_NAMESPACE_NODE     *NewNode;
    ACPI_STATUS             Status;


    AcpiOsPrintf ("Initializing namespace element: %s\n", Pathname);
    Status = AcpiNsLookup (NULL, Pathname, ACPI_TYPE_INTEGER,
        ACPI_IMODE_LOAD_PASS2, ACPI_NS_ERROR_IF_FOUND | ACPI_NS_NO_UPSEARCH |
        ACPI_NS_EARLY_INIT, NULL, &NewNode);
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status,
            "While creating name from namespace initialization file: %s",
            Pathname));
        return;
    }

    ObjDesc = AcpiUtCreateIntegerObject (Value);

    AcpiOsPrintf ("New value: 0x%8.8X%8.8X\n",
        ACPI_FORMAT_UINT64 (Value));

    /* Store pointer to value descriptor in the Node */

    Status = AcpiNsAttachObject (NewNode, ObjDesc,
         ACPI_TYPE_INTEGER);

    /* Remove local reference to the object */

    AcpiUtRemoveReference (ObjDesc);
}


/******************************************************************************
 *
 * FUNCTION:    AeLookupInitFileEntry
 *
 * PARAMETERS:  Pathname            - AML namepath in external format
 *              ValueString         - value of the namepath if it exitst
 *
 * RETURN:      None
 *
 * DESCRIPTION: Search the init file for a particular name and its value.
 *
 *****************************************************************************/

ACPI_STATUS
AeLookupInitFileEntry (
    char                    *Pathname,
    UINT64                  *Value)
{
    UINT32                  i;

    if (!AcpiGbl_InitEntries)
    {
        return AE_NOT_FOUND;
    }

    for (i = 0; i < AcpiGbl_InitFileLineCount; ++i)
    {
        if (!strcmp(AcpiGbl_InitEntries[i].Name, Pathname))
        {
            *Value = AcpiGbl_InitEntries[i].Value;
            return AE_OK;
        }
    }
    return AE_NOT_FOUND;
}

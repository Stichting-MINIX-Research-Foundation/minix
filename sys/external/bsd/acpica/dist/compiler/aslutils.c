/******************************************************************************
 *
 * Module Name: aslutils -- compiler utilities
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

#include "aslcompiler.h"
#include "aslcompiler.y.h"
#include "acdisasm.h"
#include "acnamesp.h"
#include "amlcode.h"
#include "acapps.h"
#include <sys/stat.h>


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslutils")


/* Local prototypes */

static void
UtPadNameWithUnderscores (
    char                    *NameSeg,
    char                    *PaddedNameSeg);

static void
UtAttachNameseg (
    ACPI_PARSE_OBJECT       *Op,
    char                    *Name);

static void
UtDisplayErrorSummary (
    UINT32                  FileId);


/*******************************************************************************
 *
 * FUNCTION:    UtIsBigEndianMachine
 *
 * PARAMETERS:  None
 *
 * RETURN:      TRUE if machine is big endian
 *              FALSE if machine is little endian
 *
 * DESCRIPTION: Detect whether machine is little endian or big endian.
 *
 ******************************************************************************/

UINT8
UtIsBigEndianMachine (
    void)
{
    union {
        UINT32              Integer;
        UINT8               Bytes[4];
    } Overlay =                 {0xFF000000};


    return (Overlay.Bytes[0]); /* Returns 0xFF (TRUE) for big endian */
}


/******************************************************************************
 *
 * FUNCTION:    UtQueryForOverwrite
 *
 * PARAMETERS:  Pathname            - Output filename
 *
 * RETURN:      TRUE if file does not exist or overwrite is authorized
 *
 * DESCRIPTION: Query for file overwrite if it already exists.
 *
 ******************************************************************************/

BOOLEAN
UtQueryForOverwrite (
    char                    *Pathname)
{
    struct stat             StatInfo;


    if (!stat (Pathname, &StatInfo))
    {
        fprintf (stderr, "Target file \"%s\" already exists, overwrite? [y|n] ",
            Pathname);

        if (getchar () != 'y')
        {
            return (FALSE);
        }
    }

    return (TRUE);
}


/*******************************************************************************
 *
 * FUNCTION:    UtNodeIsDescendantOf
 *
 * PARAMETERS:  Node1                   - Child node
 *              Node2                   - Possible parent node
 *
 * RETURN:      Boolean
 *
 * DESCRIPTION: Returns TRUE if Node1 is a descendant of Node2. Otherwise,
 *              return FALSE. Note, we assume a NULL Node2 element to be the
 *              topmost (root) scope. All nodes are descendants of the root.
 *              Note: Nodes at the same level (siblings) are not considered
 *              descendants.
 *
 ******************************************************************************/

BOOLEAN
UtNodeIsDescendantOf (
    ACPI_NAMESPACE_NODE     *Node1,
    ACPI_NAMESPACE_NODE     *Node2)
{

    if (Node1 == Node2)
    {
        return (FALSE);
    }

    if (!Node2)
    {
        return (TRUE); /* All nodes descend from the root */
    }

    /* Walk upward until the root is reached or parent is found */

    while (Node1)
    {
        if (Node1 == Node2)
        {
            return (TRUE);
        }

        Node1 = Node1->Parent;
    }

    return (FALSE);
}


/*******************************************************************************
 *
 * FUNCTION:    UtGetParentMethod
 *
 * PARAMETERS:  Node                    - Namespace node for any object
 *
 * RETURN:      Namespace node for the parent method
 *              NULL - object is not within a method
 *
 * DESCRIPTION: Find the parent (owning) method node for a namespace object
 *
 ******************************************************************************/

void *
UtGetParentMethod (
    ACPI_NAMESPACE_NODE     *Node)
{
    ACPI_NAMESPACE_NODE     *ParentNode;


    if (!Node)
    {
        return (NULL);
    }

    /* Walk upward until a method is found, or the root is reached */

    ParentNode = Node->Parent;
    while (ParentNode)
    {
        if (ParentNode->Type == ACPI_TYPE_METHOD)
        {
            return (ParentNode);
        }

        ParentNode = ParentNode->Parent;
    }

    return (NULL); /* Object is not within a control method */
}


/*******************************************************************************
 *
 * FUNCTION:    UtDisplaySupportedTables
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print all supported ACPI table names.
 *
 ******************************************************************************/

void
UtDisplaySupportedTables (
    void)
{
    const AH_TABLE          *TableData;
    UINT32                  i;


    printf ("\nACPI tables supported by iASL version %8.8X:\n"
        "  (Compiler, Disassembler, Template Generator)\n\n",
        ACPI_CA_VERSION);

    /* All ACPI tables with the common table header */

    printf ("\n  Supported ACPI tables:\n");
    for (TableData = AcpiGbl_SupportedTables, i = 1;
         TableData->Signature; TableData++, i++)
    {
        printf ("%8u) %s    %s\n", i,
            TableData->Signature, TableData->Description);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    UtDisplayConstantOpcodes
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print AML opcodes that can be used in constant expressions.
 *
 ******************************************************************************/

void
UtDisplayConstantOpcodes (
    void)
{
    UINT32                  i;


    printf ("Constant expression opcode information\n\n");

    for (i = 0; i < sizeof (AcpiGbl_AmlOpInfo) / sizeof (ACPI_OPCODE_INFO); i++)
    {
        if (AcpiGbl_AmlOpInfo[i].Flags & AML_CONSTANT)
        {
            printf ("%s\n", AcpiGbl_AmlOpInfo[i].Name);
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    UtBeginEvent
 *
 * PARAMETERS:  Name                - Ascii name of this event
 *
 * RETURN:      Event number (integer index)
 *
 * DESCRIPTION: Saves the current time with this event
 *
 ******************************************************************************/

UINT8
UtBeginEvent (
    char                    *Name)
{

    if (AslGbl_NextEvent >= ASL_NUM_EVENTS)
    {
        AcpiOsPrintf ("Ran out of compiler event structs!\n");
        return (AslGbl_NextEvent);
    }

    /* Init event with current (start) time */

    AslGbl_Events[AslGbl_NextEvent].StartTime = AcpiOsGetTimer ();
    AslGbl_Events[AslGbl_NextEvent].EventName = Name;
    AslGbl_Events[AslGbl_NextEvent].Valid = TRUE;
    return (AslGbl_NextEvent++);
}


/*******************************************************************************
 *
 * FUNCTION:    UtEndEvent
 *
 * PARAMETERS:  Event               - Event number (integer index)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Saves the current time (end time) with this event
 *
 ******************************************************************************/

void
UtEndEvent (
    UINT8                   Event)
{

    if (Event >= ASL_NUM_EVENTS)
    {
        return;
    }

    /* Insert end time for event */

    AslGbl_Events[Event].EndTime = AcpiOsGetTimer ();
}


/*******************************************************************************
 *
 * FUNCTION:    DbgPrint
 *
 * PARAMETERS:  Type                - Type of output
 *              Fmt                 - Printf format string
 *              ...                 - variable printf list
 *
 * RETURN:      None
 *
 * DESCRIPTION: Conditional print statement. Prints to stderr only if the
 *              debug flag is set.
 *
 ******************************************************************************/

void
DbgPrint (
    UINT32                  Type,
    char                    *Fmt,
    ...)
{
    va_list                 Args;


    if (!AslGbl_DebugFlag)
    {
        return;
    }

    if ((Type == ASL_PARSE_OUTPUT) &&
        (!(AslCompilerdebug)))
    {
        return;
    }

    va_start (Args, Fmt);
    (void) vfprintf (stderr, Fmt, Args);
    va_end (Args);
    return;
}


/*******************************************************************************
 *
 * FUNCTION:    UtSetParseOpName
 *
 * PARAMETERS:  Op                  - Parse op to be named.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Insert the ascii name of the parse opcode
 *
 ******************************************************************************/

void
UtSetParseOpName (
    ACPI_PARSE_OBJECT       *Op)
{

    AcpiUtSafeStrncpy (Op->Asl.ParseOpName, UtGetOpName (Op->Asl.ParseOpcode),
        ACPI_MAX_PARSEOP_NAME);
}


/*******************************************************************************
 *
 * FUNCTION:    UtDisplayOneSummary
 *
 * PARAMETERS:  FileID              - ID of outpout file
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display compilation statistics for one input file
 *
 ******************************************************************************/

void
UtDisplayOneSummary (
    UINT32                  FileId,
    BOOLEAN                 DisplayErrorSummary)
{
    UINT32                  i;
    ASL_GLOBAL_FILE_NODE    *FileNode;


    if (FileId != ASL_FILE_STDOUT)
    {
        /* Compiler name and version number */

        FlPrintFile (FileId, "%s version %X [%s]\n\n",
            ASL_COMPILER_NAME, (UINT32) ACPI_CA_VERSION, "2017-01-19");
    }

    /* Summary of main input and output files */

    if (AslGbl_FileType == ASL_INPUT_TYPE_ASCII_DATA)
    {
        FlPrintFile (FileId,
            "%-14s %s - %u lines, %u bytes, %u fields\n",
            "Table Input:",
            AslGbl_Files[ASL_FILE_INPUT].Filename, AslGbl_CurrentLineNumber,
            AslGbl_InputByteCount, AslGbl_InputFieldCount);

        if ((AslGbl_ExceptionCount[ASL_ERROR] == 0) || (AslGbl_IgnoreErrors))
        {
            FlPrintFile (FileId,
                "%-14s %s - %u bytes\n",
                "Binary Output:",
                AslGbl_Files[ASL_FILE_AML_OUTPUT].Filename, AslGbl_TableLength);
        }
    }
    else
    {
        FileNode = FlGetCurrentFileNode ();
        if (!FileNode)
        {
            fprintf (stderr, "Summary could not be generated");
            return;
        }
        if (FileNode->ParserErrorDetected)
        {
            FlPrintFile (FileId,
                "%-14s %s - Compilation aborted due to parser-detected syntax error(s)\n",
                "ASL Input:", AslGbl_Files[ASL_FILE_INPUT].Filename);
        }
        else
        {
            FlPrintFile (FileId,
                "%-14s %s - %7u bytes %6u keywords %6u source lines\n",
                "ASL Input:",
                AslGbl_Files[ASL_FILE_INPUT].Filename,
                FileNode->OriginalInputFileSize,
                FileNode->TotalKeywords,
                FileNode->TotalLineCount);

            /* AML summary */

            if (!AslGbl_ParserErrorDetected &&
                ((AslGbl_ExceptionCount[ASL_ERROR] == 0) || AslGbl_IgnoreErrors) &&
                AslGbl_Files[ASL_FILE_AML_OUTPUT].Handle)
            {
                FlPrintFile (FileId,
                    "%-14s %s - %7u bytes %6u opcodes  %6u named objects\n",
                    "AML Output:",
                    AslGbl_Files[ASL_FILE_AML_OUTPUT].Filename,
                    FlGetFileSize (ASL_FILE_AML_OUTPUT),
                    FileNode->TotalExecutableOpcodes,
                    FileNode->TotalNamedObjects);
            }
        }
    }

    /* Display summary of any optional files */

    for (i = ASL_FILE_SOURCE_OUTPUT; i <= ASL_MAX_FILE_TYPE; i++)
    {
        if (!AslGbl_Files[i].Filename || !AslGbl_Files[i].Handle)
        {
            continue;
        }

        /* .SRC is a temp file unless specifically requested */

        if ((i == ASL_FILE_SOURCE_OUTPUT) && (!AslGbl_SourceOutputFlag))
        {
            continue;
        }

        /* .PRE is the preprocessor intermediate file */

        if ((i == ASL_FILE_PREPROCESSOR)  && (!AslGbl_KeepPreprocessorTempFile))
        {
            continue;
        }

        FlPrintFile (FileId, "%14s %s - %u bytes\n",
            AslGbl_FileDescs[i].ShortDescription,
            AslGbl_Files[i].Filename, FlGetFileSize (i));
    }


    /*
     * Optionally emit an error summary for a file. This is used to enhance the
     * appearance of listing files.
     */
    if (DisplayErrorSummary)
    {
        UtDisplayErrorSummary (FileId);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    UtDisplayErrorSummary
 *
 * PARAMETERS:  FileID              - ID of outpout file
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display compilation statistics for all input files
 *
 ******************************************************************************/

static void
UtDisplayErrorSummary (
    UINT32                  FileId)
{
    BOOLEAN                 ErrorDetected;


    ErrorDetected = AslGbl_ParserErrorDetected ||
        ((AslGbl_ExceptionCount[ASL_ERROR] > 0) && !AslGbl_IgnoreErrors);

    if (ErrorDetected)
    {
        FlPrintFile (FileId, "\nCompilation failed. ");
    }
    else
    {
        FlPrintFile (FileId, "\nCompilation successful. ");
    }

    FlPrintFile (FileId,
        "%u Errors, %u Warnings, %u Remarks",
        AslGbl_ExceptionCount[ASL_ERROR],
        AslGbl_ExceptionCount[ASL_WARNING] +
            AslGbl_ExceptionCount[ASL_WARNING2] +
            AslGbl_ExceptionCount[ASL_WARNING3],
        AslGbl_ExceptionCount[ASL_REMARK]);

    if (AslGbl_FileType != ASL_INPUT_TYPE_ASCII_DATA)
    {
        if (AslGbl_ParserErrorDetected)
        {
            FlPrintFile (FileId,
                "\nNo AML files were generated due to syntax error(s)\n");
            return;
        }
        else if (ErrorDetected)
        {
            FlPrintFile (FileId,
                "\nNo AML files were generated due to compiler error(s)\n");
            return;
        }

        FlPrintFile (FileId, ", %u Optimizations",
            AslGbl_ExceptionCount[ASL_OPTIMIZATION]);

        if (AslGbl_TotalFolds)
        {
            FlPrintFile (FileId, ", %u Constants Folded", AslGbl_TotalFolds);
        }
    }

    FlPrintFile (FileId, "\n");
}


/*******************************************************************************
 *
 * FUNCTION:    UtDisplaySummary
 *
 * PARAMETERS:  FileID              - ID of outpout file
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display compilation statistics for all input files
 *
 ******************************************************************************/

void
UtDisplaySummary (
    UINT32                  FileId)
{
    ASL_GLOBAL_FILE_NODE    *Current = AslGbl_FilesList;


    while (Current)
    {
        switch  (FlSwitchFileSet(Current->Files[ASL_FILE_INPUT].Filename))
        {
            case SWITCH_TO_SAME_FILE:
            case SWITCH_TO_DIFFERENT_FILE:

                UtDisplayOneSummary (FileId, FALSE);
                Current = Current->Next;
                break;

            case FILE_NOT_FOUND:
            default:

                Current = NULL;
                break;
        }
    }
    UtDisplayErrorSummary (FileId);
}

/*******************************************************************************
 *
 * FUNCTION:    UtCheckIntegerRange
 *
 * PARAMETERS:  Op                  - Integer parse node
 *              LowValue            - Smallest allowed value
 *              HighValue           - Largest allowed value
 *
 * RETURN:      Op if OK, otherwise NULL
 *
 * DESCRIPTION: Check integer for an allowable range
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
UtCheckIntegerRange (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  LowValue,
    UINT32                  HighValue)
{

    if (!Op)
    {
        return (NULL);
    }

    if ((Op->Asl.Value.Integer < LowValue) ||
        (Op->Asl.Value.Integer > HighValue))
    {
        snprintf (AslGbl_MsgBuffer, sizeof(AslGbl_MsgBuffer), "0x%X, allowable: 0x%X-0x%X",
            (UINT32) Op->Asl.Value.Integer, LowValue, HighValue);

        AslError (ASL_ERROR, ASL_MSG_RANGE, Op, AslGbl_MsgBuffer);
        return (NULL);
    }

    return (Op);
}


/*******************************************************************************
 *
 * FUNCTION:    UtInternalizeName
 *
 * PARAMETERS:  ExternalName        - Name to convert
 *              ConvertedName       - Where the converted name is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert an external (ASL) name to an internal (AML) name
 *
 ******************************************************************************/

ACPI_STATUS
UtInternalizeName (
    char                    *ExternalName,
    char                    **ConvertedName)
{
    ACPI_NAMESTRING_INFO    Info;
    ACPI_STATUS             Status;


    if (!ExternalName)
    {
        return (AE_OK);
    }

    /* Get the length of the new internal name */

    Info.ExternalName = ExternalName;
    AcpiNsGetInternalNameLength (&Info);

    /* We need a segment to store the internal name */

    Info.InternalName = UtLocalCacheCalloc (Info.Length);

    /* Build the name */

    Status = AcpiNsBuildInternalName (&Info);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    *ConvertedName = Info.InternalName;
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    UtPadNameWithUnderscores
 *
 * PARAMETERS:  NameSeg             - Input nameseg
 *              PaddedNameSeg       - Output padded nameseg
 *
 * RETURN:      Padded nameseg.
 *
 * DESCRIPTION: Pads a NameSeg with underscores if necessary to form a full
 *              ACPI_NAME.
 *
 ******************************************************************************/

static void
UtPadNameWithUnderscores (
    char                    *NameSeg,
    char                    *PaddedNameSeg)
{
    UINT32                  i;


    for (i = 0; (i < ACPI_NAMESEG_SIZE); i++)
    {
        if (*NameSeg)
        {
            *PaddedNameSeg = *NameSeg;
            NameSeg++;
        }
        else
        {
            *PaddedNameSeg = '_';
        }

        PaddedNameSeg++;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    UtAttachNameseg
 *
 * PARAMETERS:  Op                  - Parent parse node
 *              Name                - Full ExternalName
 *
 * RETURN:      None; Sets the NameSeg field in parent node
 *
 * DESCRIPTION: Extract the last nameseg of the ExternalName and store it
 *              in the NameSeg field of the Op.
 *
 ******************************************************************************/

static void
UtAttachNameseg (
    ACPI_PARSE_OBJECT       *Op,
    char                    *Name)
{
    char                    *NameSeg;
    char                    PaddedNameSeg[4];


    if (!Name)
    {
        return;
    }

    /* Look for the last dot in the namepath */

    NameSeg = strrchr (Name, '.');
    if (NameSeg)
    {
        /* Found last dot, we have also found the final nameseg */

        NameSeg++;
        UtPadNameWithUnderscores (NameSeg, PaddedNameSeg);
    }
    else
    {
        /* No dots in the namepath, there is only a single nameseg. */
        /* Handle prefixes */

        while (ACPI_IS_ROOT_PREFIX (*Name) ||
               ACPI_IS_PARENT_PREFIX (*Name))
        {
            Name++;
        }

        /* Remaining string should be one single nameseg */

        UtPadNameWithUnderscores (Name, PaddedNameSeg);
    }

    ACPI_COPY_NAMESEG (Op->Asl.NameSeg, PaddedNameSeg);
}


/*******************************************************************************
 *
 * FUNCTION:    UtAttachNamepathToOwner
 *
 * PARAMETERS:  Op                  - Parent parse node
 *              NameOp              - Node that contains the name
 *
 * RETURN:      Sets the ExternalName and Namepath in the parent node
 *
 * DESCRIPTION: Store the name in two forms in the parent node: The original
 *              (external) name, and the internalized name that is used within
 *              the ACPI namespace manager.
 *
 ******************************************************************************/

void
UtAttachNamepathToOwner (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_PARSE_OBJECT       *NameOp)
{
    ACPI_STATUS             Status;


    /* Full external path */

    Op->Asl.ExternalName = NameOp->Asl.Value.String;

    /* Save the NameOp for possible error reporting later */

    Op->Asl.ParentMethod = (void *) NameOp;

    /* Last nameseg of the path */

    UtAttachNameseg (Op, Op->Asl.ExternalName);

    /* Create internalized path */

    Status = UtInternalizeName (NameOp->Asl.Value.String, &Op->Asl.Namepath);
    if (ACPI_FAILURE (Status))
    {
        /* TBD: abort on no memory */
    }
}


/*******************************************************************************
 *
 * FUNCTION:    UtDoConstant
 *
 * PARAMETERS:  String              - Hex/Decimal/Octal
 *
 * RETURN:      Converted Integer
 *
 * DESCRIPTION: Convert a string to an integer, with overflow/error checking.
 *
 ******************************************************************************/

UINT64
UtDoConstant (
    char                    *String)
{
    ACPI_STATUS             Status;
    UINT64                  ConvertedInteger;
    char                    ErrBuf[64];


    Status = AcpiUtStrtoul64 (String, &ConvertedInteger);
    if (ACPI_FAILURE (Status))
    {
        snprintf (ErrBuf, sizeof(ErrBuf), "While creating 64-bit constant: %s\n",
            AcpiFormatException (Status));

        AslCommonError (ASL_ERROR, ASL_MSG_SYNTAX, AslGbl_CurrentLineNumber,
            AslGbl_LogicalLineNumber, AslGbl_CurrentLineOffset,
            AslGbl_CurrentColumn, AslGbl_Files[ASL_FILE_INPUT].Filename, ErrBuf);
    }

    return (ConvertedInteger);
}

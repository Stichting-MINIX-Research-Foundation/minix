/******************************************************************************
 *
 * Module Name: aslcompile - top level compile module
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
#include "acnamesp.h"

#include <stdio.h>
#include <time.h>
#include <acapps.h>

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslcompile")

/*
 * Main parser entry
 * External is here in case the parser emits the same external in the
 * generated header. (Newer versions of Bison)
 */
int
AslCompilerparse(
    void);

/* Local prototypes */

static void
CmFlushSourceCode (
    void);

static void
CmDumpAllEvents (
    void);

static void
CmFinishFiles(
    BOOLEAN                 DeleteAmlFile);


/*******************************************************************************
 *
 * FUNCTION:    CmDoCompile
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status (0 = OK)
 *
 * DESCRIPTION: This procedure performs the entire compile
 *
 ******************************************************************************/

ACPI_STATUS
CmDoCompile (
    void)
{
    UINT8                   FullCompile;
    UINT8                   Event;
    ASL_GLOBAL_FILE_NODE    *FileNode;


    FullCompile = UtBeginEvent ("*** Total Compile time ***");
    Event = UtBeginEvent ("Open input and output files");
    UtEndEvent (Event);

    Event = UtBeginEvent ("Preprocess input file");
    if (AslGbl_PreprocessFlag)
    {
        /* Enter compiler name as a #define */

        PrAddDefine (ASL_DEFINE, "", FALSE);

        /* Preprocessor */

        PrDoPreprocess ();
        AslGbl_CurrentLineNumber = 1;
        AslGbl_LogicalLineNumber = 1;

        if (AslGbl_PreprocessOnly)
        {
            UtEndEvent (Event);
            CmCleanupAndExit ();
            return (AE_OK);
        }
    }
    UtEndEvent (Event);


    /* Build the parse tree */

    Event = UtBeginEvent ("Parse source code and build parse tree");
    AslCompilerparse();
    UtEndEvent (Event);

    /* Check for parser-detected syntax errors */

    if (AslGbl_SyntaxError)
    {
        fprintf (stderr,
            "Compiler aborting due to parser-detected syntax error(s)\n");

        /* Flag this error in the FileNode for compilation summary */

        FileNode = FlGetCurrentFileNode ();
        FileNode->ParserErrorDetected = TRUE;
        AslGbl_ParserErrorDetected = TRUE;
        LsDumpParseTree ();
        goto ErrorExit;
    }

    /* Did the parse tree get successfully constructed? */

    if (!AslGbl_ParseTreeRoot)
    {
        /*
         * If there are no errors, then we have some sort of
         * internal problem.
         */
        AslError (ASL_ERROR, ASL_MSG_COMPILER_INTERNAL,
            NULL, "- Could not resolve parse tree root node");

        goto ErrorExit;
    }

    /* Flush out any remaining source after parse tree is complete */

    Event = UtBeginEvent ("Flush source input");
    CmFlushSourceCode ();

    /* Prune the parse tree if requested (debug purposes only) */

    if (AslGbl_PruneParseTree)
    {
        AslPruneParseTree (AslGbl_PruneDepth, AslGbl_PruneType);
    }

    /* Optional parse tree dump, compiler debug output only */

    LsDumpParseTree ();

    OpcGetIntegerWidth (AslGbl_ParseTreeRoot->Asl.Child);
    UtEndEvent (Event);

    /* Pre-process parse tree for any operator transforms */

    Event = UtBeginEvent ("Parse tree transforms");
    DbgPrint (ASL_DEBUG_OUTPUT, "\nParse tree transforms\n\n");
    TrWalkParseTree (AslGbl_ParseTreeRoot, ASL_WALK_VISIT_TWICE,
        TrAmlTransformWalkBegin, TrAmlTransformWalkEnd, NULL);
    UtEndEvent (Event);

    /* Generate AML opcodes corresponding to the parse tokens */

    Event = UtBeginEvent ("Generate AML opcodes");
    DbgPrint (ASL_DEBUG_OUTPUT, "Generating AML opcodes\n\n");
    TrWalkParseTree (AslGbl_ParseTreeRoot, ASL_WALK_VISIT_UPWARD, NULL,
        OpcAmlOpcodeWalk, NULL);
    UtEndEvent (Event);

    UtEndEvent (FullCompile);
    return (AE_OK);

ErrorExit:
    UtEndEvent (FullCompile);
    return (AE_ERROR);
}


/*******************************************************************************
 *
 * FUNCTION:    CmDoAslMiddleAndBackEnd
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status of middle-end and back-end
 *
 * DESCRIPTION: Perform compiler middle-end (type checking and semantic
 *              analysis) and back-end (code generation)
 *
 ******************************************************************************/

int
CmDoAslMiddleAndBackEnd (
    void)
{
    UINT8                   Event;
    ACPI_STATUS             Status;


    /* Interpret and generate all compile-time constants */

    Event = UtBeginEvent ("Constant folding via AML interpreter");
    DbgPrint (ASL_DEBUG_OUTPUT,
        "Interpreting compile-time constant expressions\n\n");

    if (AslGbl_FoldConstants)
    {
        TrWalkParseTree (AslGbl_ParseTreeRoot, ASL_WALK_VISIT_UPWARD,
            NULL, OpcAmlConstantWalk, NULL);
    }
    else
    {
        DbgPrint (ASL_PARSE_OUTPUT, "    Optional folding disabled\n");
    }
    UtEndEvent (Event);

    /* Update AML opcodes if necessary, after constant folding */

    Event = UtBeginEvent ("Updating AML opcodes after constant folding");
    DbgPrint (ASL_DEBUG_OUTPUT,
        "Updating AML opcodes after constant folding\n\n");
    TrWalkParseTree (AslGbl_ParseTreeRoot, ASL_WALK_VISIT_UPWARD,
        NULL, OpcAmlOpcodeUpdateWalk, NULL);
    UtEndEvent (Event);

    /* Calculate all AML package lengths */

    Event = UtBeginEvent ("Generate AML package lengths");
    DbgPrint (ASL_DEBUG_OUTPUT, "Generating Package lengths\n\n");
    TrWalkParseTree (AslGbl_ParseTreeRoot, ASL_WALK_VISIT_UPWARD, NULL,
        LnPackageLengthWalk, NULL);
    UtEndEvent (Event);

    if (AslGbl_ParseOnlyFlag)
    {
        AePrintErrorLog (ASL_FILE_STDERR);
        UtDisplaySummary (ASL_FILE_STDERR);
        if (AslGbl_DebugFlag)
        {
            /* Print error summary to the stdout also */

            AePrintErrorLog (ASL_FILE_STDOUT);
            UtDisplaySummary (ASL_FILE_STDOUT);
        }
        return (0);
    }

    /*
     * Create an internal namespace and use it as a symbol table
     */

    /* Namespace loading */

    Event = UtBeginEvent ("Create ACPI Namespace");
    DbgPrint (ASL_DEBUG_OUTPUT, "Creating ACPI Namespace\n\n");
    Status = LdLoadNamespace (AslGbl_ParseTreeRoot);
    UtEndEvent (Event);
    if (ACPI_FAILURE (Status))
    {
        return (-1);
    }

    /* Namespace cross-reference */

    AslGbl_NamespaceEvent = UtBeginEvent (
        "Cross reference parse tree and Namespace");
    DbgPrint (ASL_DEBUG_OUTPUT, "Cross referencing namespace\n\n");
    Status = XfCrossReferenceNamespace ();
    if (ACPI_FAILURE (Status))
    {
        return (-1);
    }

    /* Namespace - Check for non-referenced objects */

    LkFindUnreferencedObjects ();
    UtEndEvent (AslGbl_NamespaceEvent);

    /* Resolve External Declarations */

    Event = UtBeginEvent ("Resolve all Externals");
    DbgPrint (ASL_DEBUG_OUTPUT, "\nResolve Externals\n\n");

    if (AslGbl_DoExternalsInPlace)
    {
        TrWalkParseTree (AslGbl_ParseTreeRoot, ASL_WALK_VISIT_DOWNWARD,
            ExAmlExternalWalkBegin, NULL, NULL);
    }
    else
    {
        TrWalkParseTree (AslGbl_ParseTreeRoot, ASL_WALK_VISIT_TWICE,
            ExAmlExternalWalkBegin, ExAmlExternalWalkEnd, NULL);
    }
    UtEndEvent (Event);

    /*
     * Semantic analysis. This can happen only after the
     * namespace has been loaded and cross-referenced.
     *
     * part one - check control methods
     */
    Event = UtBeginEvent ("Analyze control method return types");
    AslGbl_AnalysisWalkInfo.MethodStack = NULL;

    DbgPrint (ASL_DEBUG_OUTPUT, "Semantic analysis - Method analysis\n\n");

    if (AslGbl_CrossReferenceOutput)
    {
        OtPrintHeaders ("Part 1: Object Reference Map "
            "(Object references from within each control method)");
    }

    TrWalkParseTree (AslGbl_ParseTreeRoot, ASL_WALK_VISIT_TWICE,
        MtMethodAnalysisWalkBegin,
        MtMethodAnalysisWalkEnd, &AslGbl_AnalysisWalkInfo);
    UtEndEvent (Event);

    /* Generate the object cross-reference file if requested */

    Event = UtBeginEvent ("Generate cross-reference file");
    OtCreateXrefFile ();
    UtEndEvent (Event);

    /* Semantic error checking part two - typing of method returns */

    Event = UtBeginEvent ("Determine object types returned by methods");
    DbgPrint (ASL_DEBUG_OUTPUT, "Semantic analysis - Method typing\n\n");
    TrWalkParseTree (AslGbl_ParseTreeRoot, ASL_WALK_VISIT_UPWARD,
        NULL, AnMethodTypingWalkEnd, NULL);
    UtEndEvent (Event);

    /* Semantic error checking part three - operand type checking */

    Event = UtBeginEvent ("Analyze AML operand types");
    DbgPrint (ASL_DEBUG_OUTPUT,
        "Semantic analysis - Operand type checking\n\n");
    if (AslGbl_DoTypechecking)
    {
        TrWalkParseTree (AslGbl_ParseTreeRoot, ASL_WALK_VISIT_UPWARD,
            NULL, AnOperandTypecheckWalkEnd, &AslGbl_AnalysisWalkInfo);
    }
    UtEndEvent (Event);

    /* Semantic error checking part four - other miscellaneous checks */

    Event = UtBeginEvent ("Miscellaneous analysis");
    DbgPrint (ASL_DEBUG_OUTPUT, "Semantic analysis - miscellaneous\n\n");
    TrWalkParseTree (AslGbl_ParseTreeRoot, ASL_WALK_VISIT_DOWNWARD,
        AnOtherSemanticAnalysisWalkBegin,
        NULL, &AslGbl_AnalysisWalkInfo);
    UtEndEvent (Event);

    /*
     * ASL-/ASL+ converter: Gbl_ParseTreeRoot->CommentList contains the
     * very last comment of a given ASL file because it's the last constructed
     * node during compilation. We take the very last comment and save it in a
     * global for it to be used by the disassembler.
     */
    if (AcpiGbl_CaptureComments)
    {
        AcpiGbl_LastListHead = AslGbl_ParseTreeRoot->Asl.CommentList;
        AslGbl_ParseTreeRoot->Asl.CommentList = NULL;
    }

    /* Calculate all AML package lengths */

    Event = UtBeginEvent ("Finish AML package length generation");
    DbgPrint (ASL_DEBUG_OUTPUT, "Generating Package lengths\n\n");
    TrWalkParseTree (AslGbl_ParseTreeRoot, ASL_WALK_VISIT_UPWARD, NULL,
        LnInitLengthsWalk, NULL);
    TrWalkParseTree (AslGbl_ParseTreeRoot, ASL_WALK_VISIT_UPWARD, NULL,
        LnPackageLengthWalk, NULL);
    UtEndEvent (Event);

    /* Code generation - emit the AML */

    Event = UtBeginEvent ("Generate AML code and write output files");
    DbgPrint (ASL_DEBUG_OUTPUT, "Writing AML byte code\n\n");

    AslGbl_CurrentDB = AslGbl_ParseTreeRoot->Asl.Child;

    while (AslGbl_CurrentDB)
    {
        switch  (FlSwitchFileSet(AslGbl_CurrentDB->Asl.Filename))
        {
            case SWITCH_TO_DIFFERENT_FILE:
                /*
                 * Reset these parameters when definition blocks belong in
                 * different files. If they belong in the same file, there is
                 * no need to reset these parameters
                 */
                FlSeekFile (ASL_FILE_SOURCE_OUTPUT, 0);
                AslGbl_SourceLine = 0;
                AslGbl_NextError = AslGbl_ErrorLog;

                /* fall-through */

            case SWITCH_TO_SAME_FILE:

                CgGenerateAmlOutput ();
                CmDoOutputFiles ();
                AslGbl_CurrentDB = AslGbl_CurrentDB->Asl.Next;

                break;

            default: /* FILE_NOT_FOUND */

                /* The requested file could not be found. Get out of here */

                AslGbl_CurrentDB = NULL;
                break;
        }
    }
    UtEndEvent (Event);

    Event = UtBeginEvent ("Write optional output files");
    UtEndEvent (Event);

    return (0);
}


/*******************************************************************************
 *
 * FUNCTION:    AslCompilerSignon
 *
 * PARAMETERS:  FileId      - ID of the output file
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display compiler signon
 *
 ******************************************************************************/

void
AslCompilerSignon (
    UINT32                  FileId)
{
    char                    *Prefix = "";
    char                    *UtilityName;


    /* Set line prefix depending on the destination file type */

    switch (FileId)
    {
    case ASL_FILE_ASM_SOURCE_OUTPUT:
    case ASL_FILE_ASM_INCLUDE_OUTPUT:

        Prefix = "; ";
        break;

    case ASL_FILE_HEX_OUTPUT:

        if (AslGbl_HexOutputFlag == HEX_OUTPUT_ASM)
        {
            Prefix = "; ";
        }
        else if ((AslGbl_HexOutputFlag == HEX_OUTPUT_C) ||
                 (AslGbl_HexOutputFlag == HEX_OUTPUT_ASL))
        {
            FlPrintFile (ASL_FILE_HEX_OUTPUT, "/*\n");
            Prefix = " * ";
        }
        break;

    case ASL_FILE_C_SOURCE_OUTPUT:
    case ASL_FILE_C_OFFSET_OUTPUT:
    case ASL_FILE_C_INCLUDE_OUTPUT:

        Prefix = " * ";
        break;

    default:

        /* No other output types supported */

        break;
    }

    /* Running compiler or disassembler? */

    if (AcpiGbl_DisasmFlag)
    {
        UtilityName = AML_DISASSEMBLER_NAME;
    }
    else
    {
        UtilityName = ASL_COMPILER_NAME;
    }

    /* Compiler signon with copyright */

    FlPrintFile (FileId, "%s\n", Prefix);
    FlPrintFile (FileId, ACPI_COMMON_HEADER (UtilityName, Prefix));
}


/*******************************************************************************
 *
 * FUNCTION:    AslCompilerFileHeader
 *
 * PARAMETERS:  FileId      - ID of the output file
 *
 * RETURN:      None
 *
 * DESCRIPTION: Header used at the beginning of output files
 *
 ******************************************************************************/

void
AslCompilerFileHeader (
    UINT32                  FileId)
{
    struct tm               *NewTime;
    time_t                  Aclock;
    char                    *Prefix = "";


    /* Set line prefix depending on the destination file type */

    switch (FileId)
    {
    case ASL_FILE_ASM_SOURCE_OUTPUT:
    case ASL_FILE_ASM_INCLUDE_OUTPUT:

        Prefix = "; ";
        break;

    case ASL_FILE_HEX_OUTPUT:

        if (AslGbl_HexOutputFlag == HEX_OUTPUT_ASM)
        {
            Prefix = "; ";
        }
        else if ((AslGbl_HexOutputFlag == HEX_OUTPUT_C) ||
                 (AslGbl_HexOutputFlag == HEX_OUTPUT_ASL))
        {
            Prefix = " * ";
        }
        break;

    case ASL_FILE_C_SOURCE_OUTPUT:
    case ASL_FILE_C_OFFSET_OUTPUT:
    case ASL_FILE_C_INCLUDE_OUTPUT:

        Prefix = " * ";
        break;

    default:

        /* No other output types supported */

        break;
    }

    /* Compilation header with timestamp */

    (void) time (&Aclock);
    NewTime = localtime (&Aclock);

    FlPrintFile (FileId,
        "%sCompilation of \"%s\" - %s%s\n",
        Prefix, AslGbl_Files[ASL_FILE_INPUT].Filename, asctime (NewTime),
        Prefix);

    switch (FileId)
    {
    case ASL_FILE_C_SOURCE_OUTPUT:
    case ASL_FILE_C_OFFSET_OUTPUT:
    case ASL_FILE_C_INCLUDE_OUTPUT:

        FlPrintFile (FileId, " */\n");
        break;

    default:

        /* Nothing to do for other output types */

        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CmFlushSourceCode
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Read in any remaining source code after the parse tree
 *              has been constructed.
 *
 ******************************************************************************/

static void
CmFlushSourceCode (
    void)
{
    char                    Buffer;


    while (FlReadFile (ASL_FILE_INPUT, &Buffer, 1) != AE_ERROR)
    {
        AslInsertLineBuffer ((int) Buffer);
    }

    AslResetCurrentLineBuffer ();
}


/*******************************************************************************
 *
 * FUNCTION:    CmDoOutputFiles
 *
 * PARAMETERS:  None
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Create all "listing" type files
 *
 ******************************************************************************/

void
CmDoOutputFiles (
    void)
{

    /* Create listings and hex files */

    LsDoListings ();
    HxDoHexOutput ();

    /* Dump the namespace to the .nsp file if requested */

    (void) NsDisplayNamespace ();

    /* Dump the device mapping file */

    MpEmitMappingInfo ();
}


/*******************************************************************************
 *
 * FUNCTION:    CmDumpAllEvents
 *
 * PARAMETERS:  None
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Dump all compiler events
 *
 ******************************************************************************/

static void
CmDumpAllEvents (
    void)
{
    ASL_EVENT_INFO          *Event;
    UINT32                  Delta;
    UINT32                  MicroSeconds;
    UINT32                  MilliSeconds;
    UINT32                  i;


    Event = AslGbl_Events;

    DbgPrint (ASL_DEBUG_OUTPUT, "\n\nElapsed time for major events\n\n");
    if (AslGbl_CompileTimesFlag)
    {
        printf ("\nElapsed time for major events\n\n");
    }

    for (i = 0; i < AslGbl_NextEvent; i++)
    {
        if (Event->Valid)
        {
            /* Delta will be in 100-nanosecond units */

            Delta = (UINT32) (Event->EndTime - Event->StartTime);

            MicroSeconds = Delta / ACPI_100NSEC_PER_USEC;
            MilliSeconds = Delta / ACPI_100NSEC_PER_MSEC;

            /* Round milliseconds up */

            if ((MicroSeconds - (MilliSeconds * ACPI_USEC_PER_MSEC)) >= 500)
            {
                MilliSeconds++;
            }

            DbgPrint (ASL_DEBUG_OUTPUT, "%8u usec %8u msec - %s\n",
                MicroSeconds, MilliSeconds, Event->EventName);

            if (AslGbl_CompileTimesFlag)
            {
                printf ("%8u usec %8u msec - %s\n",
                    MicroSeconds, MilliSeconds, Event->EventName);
            }
        }

        Event++;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CmCleanupAndExit
 *
 * PARAMETERS:  None
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Close all open files and exit the compiler
 *
 ******************************************************************************/

void
CmCleanupAndExit (
    void)
{
    BOOLEAN                 DeleteAmlFile = FALSE;
    ASL_GLOBAL_FILE_NODE    *CurrentFileNode = AslGbl_FilesList;


    /* Check if any errors occurred during compile */

    (void) AslCheckForErrorExit ();

    AePrintErrorLog (ASL_FILE_STDERR);
    if (AslGbl_DebugFlag)
    {
        /* Print error summary to stdout also */

        AePrintErrorLog (ASL_FILE_STDOUT);
    }

    /* Emit compile times if enabled */

    CmDumpAllEvents ();

    if (AslGbl_CompileTimesFlag)
    {
        printf ("\nMiscellaneous compile statistics\n\n");
        printf ("%11u : %s\n", AslGbl_TotalParseNodes, "Parse nodes");
        printf ("%11u : %s\n", AslGbl_NsLookupCount, "Namespace searches");
        printf ("%11u : %s\n", AslGbl_TotalNamedObjects, "Named objects");
        printf ("%11u : %s\n", AslGbl_TotalMethods, "Control methods");
        printf ("%11u : %s\n", AslGbl_TotalAllocations, "Memory Allocations");
        printf ("%11u : %s\n", AslGbl_TotalAllocated, "Total allocated memory");
        printf ("%11u : %s\n", AslGbl_TotalFolds, "Constant subtrees folded");
        printf ("\n");
    }

    if (AslGbl_NsLookupCount)
    {
        DbgPrint (ASL_DEBUG_OUTPUT,
            "\n\nMiscellaneous compile statistics\n\n");

        DbgPrint (ASL_DEBUG_OUTPUT,
            "%32s : %u\n", "Total Namespace searches",
            AslGbl_NsLookupCount);

        DbgPrint (ASL_DEBUG_OUTPUT,
            "%32s : %u usec\n", "Time per search", ((UINT32)
            (AslGbl_Events[AslGbl_NamespaceEvent].EndTime -
                AslGbl_Events[AslGbl_NamespaceEvent].StartTime) / 10) /
                AslGbl_NsLookupCount);
    }

    if (AslGbl_ExceptionCount[ASL_ERROR] > ASL_MAX_ERROR_COUNT)
    {
        printf ("\nMaximum error count (%d) exceeded\n",
            ASL_MAX_ERROR_COUNT);
    }

    UtDisplaySummary (ASL_FILE_STDOUT);

    /*
     * We will delete the AML file if there are errors and the
     * force AML output option has not been used.
     */
    if (AslGbl_ParserErrorDetected || ((AslGbl_ExceptionCount[ASL_ERROR] > 0) &&
        (!AslGbl_IgnoreErrors) &&
        AslGbl_Files[ASL_FILE_AML_OUTPUT].Handle))
    {
        DeleteAmlFile = TRUE;
    }

    /* Close all open files */

    while (CurrentFileNode)
    {
        switch  (FlSwitchFileSet (CurrentFileNode->Files[ASL_FILE_INPUT].Filename))
        {
            case SWITCH_TO_SAME_FILE:
            case SWITCH_TO_DIFFERENT_FILE:

                CmFinishFiles (DeleteAmlFile);
                CurrentFileNode = CurrentFileNode->Next;
                break;

            case FILE_NOT_FOUND:
            default:

                CurrentFileNode = NULL;
                break;
        }
    }

    /* Final cleanup after compiling one file */

    if (!AslGbl_DoAslConversion)
    {
        UtDeleteLocalCaches ();
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CmFinishFiles
 *
 * PARAMETERS:  DeleteAmlFile
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Close all open files, delete AML files depending on the
 *              function parameter is true.
 *
 ******************************************************************************/

static void
CmFinishFiles(
    BOOLEAN                 DeleteAmlFile)
{
    UINT32                  i;


    /*
     * Take care with the preprocessor file (.pre), it might be the same
     * as the "input" file, depending on where the compiler has terminated
     * or aborted. Prevent attempt to close the same file twice in
     * loop below.
     */
    if (AslGbl_Files[ASL_FILE_PREPROCESSOR].Handle ==
        AslGbl_Files[ASL_FILE_INPUT].Handle)
    {
        AslGbl_Files[ASL_FILE_PREPROCESSOR].Handle = NULL;
    }

    /* Close the standard I/O files */

    for (i = ASL_FILE_INPUT; i < ASL_MAX_FILE_TYPE; i++)
    {
        /*
         * Some files such as debug output files could be pointing to
         * stderr or stdout. Leave these alone.
         */
        if (AslGbl_Files[i].Handle != stderr &&
            AslGbl_Files[i].Handle != stdout)
        {
            FlCloseFile (i);
        }
    }

    /* Delete AML file if there are errors */

    if (DeleteAmlFile)
    {
        FlDeleteFile (ASL_FILE_AML_OUTPUT);
    }

    /* Delete the preprocessor temp file unless full debug was specified */

    if (AslGbl_PreprocessFlag && !AslGbl_KeepPreprocessorTempFile)
    {
        FlDeleteFile (ASL_FILE_PREPROCESSOR);
    }

    /*
     * Delete intermediate ("combined") source file (if -ls flag not set)
     * This file is created during normal ASL/AML compiles. It is not
     * created by the data table compiler.
     *
     * If the -ls flag is set, then the .SRC file should not be deleted.
     * In this case, Gbl_SourceOutputFlag is set to TRUE.
     *
     * Note: Handles are cleared by FlCloseFile above, so we look at the
     * filename instead, to determine if the .SRC file was actually
     * created.
     */
    if (!AslGbl_SourceOutputFlag)
    {
        FlDeleteFile (ASL_FILE_SOURCE_OUTPUT);
    }
}

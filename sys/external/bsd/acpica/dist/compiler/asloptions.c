/******************************************************************************
 *
 * Module Name: asloptions - compiler command line processing
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
#include "acapps.h"
#include "acdisasm.h"

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("asloption")


/* Local prototypes */

static int
AslDoOptions (
    int                     argc,
    char                    **argv,
    BOOLEAN                 IsResponseFile);

static void
AslMergeOptionTokens (
    char                    *InBuffer,
    char                    *OutBuffer);

static int
AslDoResponseFile (
    char                    *Filename);


#define ASL_TOKEN_SEPARATORS    " \t\n"
#define ASL_SUPPORTED_OPTIONS   "@:a:b|c|d^D:e:f^gh^i|I:l^m:no|p:P^q^r:s|t|T+G^v^w|x:z"


/*******************************************************************************
 *
 * FUNCTION:    AslCommandLine
 *
 * PARAMETERS:  argc/argv
 *
 * RETURN:      Last argv index
 *
 * DESCRIPTION: Command line processing
 *
 ******************************************************************************/

int
AslCommandLine (
    int                     argc,
    char                    **argv)
{
    int                     BadCommandLine = 0;
    ACPI_STATUS             Status;


    /* Minimum command line contains at least the command and an input file */

    if (argc < 2)
    {
        Usage ();
        exit (1);
    }

    /* Process all command line options */

    BadCommandLine = AslDoOptions (argc, argv, FALSE);

    if (AslGbl_DoTemplates)
    {
        Status = DtCreateTemplates (argv);
        if (ACPI_FAILURE (Status))
        {
            exit (-1);
        }
        exit (1);
    }

    /* Next parameter must be the input filename */

    if (!argv[AcpiGbl_Optind] &&
        !AcpiGbl_DisasmFlag)
    {
        printf ("Missing input filename\n");
        BadCommandLine = TRUE;
    }

    if (AslGbl_DoSignon)
    {
        printf (ACPI_COMMON_SIGNON (ASL_COMPILER_NAME));
        if (AslGbl_IgnoreErrors)
        {
            printf ("Ignoring all errors, forcing AML file generation\n\n");
        }
    }

    if (BadCommandLine)
    {
        printf ("Use -h option for help information\n");
        exit (1);
    }

    return (AcpiGbl_Optind);
}


/*******************************************************************************
 *
 * FUNCTION:    AslDoOptions
 *
 * PARAMETERS:  argc/argv           - Standard argc/argv
 *              IsResponseFile      - TRUE if executing a response file.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Command line option processing
 *
 ******************************************************************************/

static int
AslDoOptions (
    int                     argc,
    char                    **argv,
    BOOLEAN                 IsResponseFile)
{
    ACPI_STATUS             Status;
    UINT32                  j;


    /* Get the command line options */

    while ((j = AcpiGetopt (argc, argv, ASL_SUPPORTED_OPTIONS)) != ACPI_OPT_END) switch (j)
    {
    case '@':   /* Begin a response file */

        if (IsResponseFile)
        {
            printf ("Nested command files are not supported\n");
            return (-1);
        }

        if (AslDoResponseFile (AcpiGbl_Optarg))
        {
            return (-1);
        }
        break;

    case 'a':   /* Debug options */

        switch (AcpiGbl_Optarg[0])
        {
        case 'r':

            AslGbl_EnableReferenceTypechecking = TRUE;
            break;

        default:

            printf ("Unknown option: -a%s\n", AcpiGbl_Optarg);
            return (-1);
        }

        break;


    case 'b':   /* Debug options */

        switch (AcpiGbl_Optarg[0])
        {

        case 'c':

            printf ("Debug ASL to ASL+ conversion\n");

            AslGbl_DoAslConversion = TRUE;
            AslGbl_FoldConstants = FALSE;
            AslGbl_IntegerOptimizationFlag = FALSE;
            AslGbl_ReferenceOptimizationFlag = FALSE;
            AslGbl_OptimizeTrivialParseNodes = FALSE;
            AcpiGbl_CaptureComments = TRUE;
            AcpiGbl_DoDisassemblerOptimizations = FALSE;
            AcpiGbl_DebugAslConversion = TRUE;
            AcpiGbl_DmEmitExternalOpcodes = TRUE;
            AslGbl_DoExternalsInPlace = TRUE;

            return (0);

        case 'f':

            AslCompilerdebug = 1; /* same as yydebug */
            DtParserdebug = 1;
            PrParserdebug = 1;
            AslGbl_DebugFlag = TRUE;
            AslGbl_KeepPreprocessorTempFile = TRUE;
            break;

        case 'p':   /* Prune ASL parse tree */

            /* Get the required argument */

            if (AcpiGetoptArgument (argc, argv))
            {
                return (-1);
            }

            AslGbl_PruneParseTree = TRUE;
            AslGbl_PruneDepth = (UINT8) strtoul (AcpiGbl_Optarg, NULL, 0);
            break;

        case 's':

            AslGbl_DebugFlag = TRUE;
            break;

        case 't':

            /* Get the required argument */

            if (AcpiGetoptArgument (argc, argv))
            {
                return (-1);
            }

            AslGbl_PruneType = (UINT8) strtoul (AcpiGbl_Optarg, NULL, 0);
            break;

        default:

            printf ("Unknown option: -b%s\n", AcpiGbl_Optarg);
            return (-1);
        }

        break;

    case 'c':

        switch (AcpiGbl_Optarg[0])
        {

        case 'a':

            printf ("Convert ASL to ASL+ with comments\n");
            AslGbl_DoAslConversion = TRUE;
            AslGbl_FoldConstants = FALSE;
            AslGbl_IntegerOptimizationFlag = FALSE;
            AslGbl_ReferenceOptimizationFlag = FALSE;
            AslGbl_OptimizeTrivialParseNodes = FALSE;
            AcpiGbl_CaptureComments = TRUE;
            AcpiGbl_DoDisassemblerOptimizations = FALSE;
            AcpiGbl_DmEmitExternalOpcodes = TRUE;
            AslGbl_DoExternalsInPlace = TRUE;

            return (0);

        case 'r':

            AslGbl_NoResourceChecking = TRUE;
            break;

        default:

            printf ("Unknown option: -c%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;

    case 'd':   /* Disassembler */

        switch (AcpiGbl_Optarg[0])
        {
        case '^':

            AslGbl_DoCompile = FALSE;
            break;

        case 'a':

            AslGbl_DoCompile = FALSE;
            AslGbl_DisassembleAll = TRUE;
            break;

        case 'b':   /* Do not convert buffers to resource descriptors */

            AcpiGbl_NoResourceDisassembly = TRUE;
            break;

        case 'c':

            break;

        case 'f':

            AcpiGbl_ForceAmlDisassembly = TRUE;
            break;

        case 'l':   /* Use legacy ASL code (not ASL+) for disassembly */

            AslGbl_DoCompile = FALSE;
            AcpiGbl_CstyleDisassembly = FALSE;
            break;

        default:

            printf ("Unknown option: -d%s\n", AcpiGbl_Optarg);
            return (-1);
        }

        AcpiGbl_DisasmFlag = TRUE;
        break;

    case 'D':   /* Define a symbol */

        PrAddDefine (AcpiGbl_Optarg, NULL, TRUE);
        break;

    case 'e':   /* External files for disassembler */

        /* Get entire list of external files */

        AcpiGbl_Optind--;
        argv[AcpiGbl_Optind] = AcpiGbl_Optarg;

        while (argv[AcpiGbl_Optind] &&
              (argv[AcpiGbl_Optind][0] != '-'))
        {
            Status = AcpiDmAddToExternalFileList (argv[AcpiGbl_Optind]);
            if (ACPI_FAILURE (Status))
            {
                printf ("Could not add %s to external list\n",
                    argv[AcpiGbl_Optind]);
                return (-1);
            }

            AcpiGbl_Optind++;
        }
        break;

    case 'f':

        switch (AcpiGbl_Optarg[0])
        {
        case '^':   /* Ignore errors and force creation of aml file */

            AslGbl_IgnoreErrors = TRUE;
            break;

        case 'e':   /* Disassembler: Get external declaration file */

            if (AcpiGetoptArgument (argc, argv))
            {
                return (-1);
            }

            AslGbl_ExternalRefFilename = AcpiGbl_Optarg;
            break;

        default:

            printf ("Unknown option: -f%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;

    case 'G':

        AslGbl_CompileGeneric = TRUE;
        break;

    case 'g':   /* Get all ACPI tables */

        printf ("-g option is deprecated, use acpidump utility instead\n");
        exit (1);

    case 'h':

        switch (AcpiGbl_Optarg[0])
        {
        case '^':

            Usage ();
            exit (0);

        case 'c':

            UtDisplayConstantOpcodes ();
            exit (0);

        case 'd':

            AslDisassemblyHelp ();
            exit (0);

        case 'f':

            AslFilenameHelp ();
            exit (0);

        case 'r':

            /* reserved names */

            ApDisplayReservedNames ();
            exit (0);

        case 't':

            UtDisplaySupportedTables ();
            exit (0);

        default:

            printf ("Unknown option: -h%s\n", AcpiGbl_Optarg);
            return (-1);
        }

    case 'I':   /* Add an include file search directory */

        FlAddIncludeDirectory (AcpiGbl_Optarg);
        break;

    case 'i':   /* Output AML as an include file */

        switch (AcpiGbl_Optarg[0])
        {
        case 'a':

            /* Produce assembly code include file */

            AslGbl_AsmIncludeOutputFlag = TRUE;
            break;

        case 'c':

            /* Produce C include file */

            AslGbl_C_IncludeOutputFlag = TRUE;
            break;

        case 'n':

            /* Compiler/Disassembler: Ignore the NOOP operator */

            AcpiGbl_IgnoreNoopOperator = TRUE;
            break;

        default:

            printf ("Unknown option: -i%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;

    case 'l':   /* Listing files */

        switch (AcpiGbl_Optarg[0])
        {
        case '^':

            /* Produce listing file (Mixed source/aml) */

            AslGbl_ListingFlag = TRUE;
            AcpiGbl_DmOpt_Listing = TRUE;
            break;

        case 'i':

            /* Produce preprocessor output file */

            AslGbl_PreprocessorOutputFlag = TRUE;
            break;

        case 'm':

            /* Produce hardware map summary file */

            AslGbl_MapfileFlag = TRUE;
            break;

        case 'n':

            /* Produce namespace file */

            AslGbl_NsOutputFlag = TRUE;
            break;

        case 's':

            /* Produce combined source file */

            AslGbl_SourceOutputFlag = TRUE;
            break;

        case 'x':

            /* Produce cross-reference file */

            AslGbl_CrossReferenceOutput = TRUE;
            break;

        default:

            printf ("Unknown option: -l%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;

    case 'm':   /* Set line buffer size */

        AslGbl_LineBufferSize = (UINT32) strtoul (AcpiGbl_Optarg, NULL, 0) * 1024;
        if (AslGbl_LineBufferSize < ASL_DEFAULT_LINE_BUFFER_SIZE)
        {
            AslGbl_LineBufferSize = ASL_DEFAULT_LINE_BUFFER_SIZE;
        }
        printf ("Line Buffer Size: %u\n", AslGbl_LineBufferSize);
        break;

    case 'n':   /* Parse only */

        AslGbl_ParseOnlyFlag = TRUE;
        break;

    case 'o':   /* Control compiler AML optimizations */

        switch (AcpiGbl_Optarg[0])
        {
        case 'a':

            /* Disable all optimizations */

            AslGbl_FoldConstants = FALSE;
            AslGbl_IntegerOptimizationFlag = FALSE;
            AslGbl_ReferenceOptimizationFlag = FALSE;
            AslGbl_OptimizeTrivialParseNodes = FALSE;

            break;

        case 'c':

            /* Display compile time(s) */

            AslGbl_CompileTimesFlag = TRUE;
            break;

        case 'd':

            /* Disable disassembler code optimizations */

            AcpiGbl_DoDisassemblerOptimizations = FALSE;
            break;

        case 'e':

            /* Disassembler: Emit embedded external operators */

            AcpiGbl_DmEmitExternalOpcodes = TRUE;
            break;

        case 'E':

            /*
             * iASL: keep External opcodes in place.
             * No affect if Gbl_DoExternals is false.
             */

            AslGbl_DoExternalsInPlace = TRUE;
            break;

        case 'f':

            /* Disable folding on "normal" expressions */

            AslGbl_FoldConstants = FALSE;
            break;

        case 'i':

            /* Disable integer optimization to constants */

            AslGbl_IntegerOptimizationFlag = FALSE;
            break;

        case 'n':

            /* Disable named reference optimization */

            AslGbl_ReferenceOptimizationFlag = FALSE;
            break;

        case 't':

            /* Disable heavy typechecking */

            AslGbl_DoTypechecking = FALSE;
            break;

        default:

            printf ("Unknown option: -c%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;

    case 'P':   /* Preprocessor options */

        switch (AcpiGbl_Optarg[0])
        {
        case '^':   /* Proprocess only, emit (.i) file */

            AslGbl_PreprocessOnly = TRUE;
            AslGbl_PreprocessorOutputFlag = TRUE;
            break;

        case 'n':   /* Disable preprocessor */

            AslGbl_PreprocessFlag = FALSE;
            break;

        default:

            printf ("Unknown option: -P%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;

    case 'p':   /* Override default AML output filename */

        AslGbl_OutputFilenamePrefix = AcpiGbl_Optarg;
        UtConvertBackslashes (AslGbl_OutputFilenamePrefix);
        AslGbl_UseDefaultAmlFilename = FALSE;
        break;

    case 'q':   /* ASL/ASl+ converter: compile only and leave badaml. */

        printf ("Convert ASL to ASL+ with comments\n");
        AslGbl_FoldConstants = FALSE;
        AslGbl_IntegerOptimizationFlag = FALSE;
        AslGbl_ReferenceOptimizationFlag = FALSE;
        AslGbl_OptimizeTrivialParseNodes = FALSE;
        AslGbl_DoExternalsInPlace = TRUE;
        AcpiGbl_CaptureComments = TRUE;
        return (0);

    case 'r':   /* Override revision found in table header */

        AslGbl_RevisionOverride = (UINT8) strtoul (AcpiGbl_Optarg, NULL, 0);
        break;

    case 's':   /* Create AML in a source code file */

        switch (AcpiGbl_Optarg[0])
        {
        case 'a':

            /* Produce assembly code output file */

            AslGbl_AsmOutputFlag = TRUE;
            break;

        case 'c':

            /* Produce C hex output file */

            AslGbl_C_OutputFlag = TRUE;
            break;

        case 'o':

            /* Produce AML offset table in C */

            AslGbl_C_OffsetTableFlag = TRUE;
            break;

        default:

            printf ("Unknown option: -s%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;

    case 't':   /* Produce hex table output file */

        switch (AcpiGbl_Optarg[0])
        {
        case 'a':

            AslGbl_HexOutputFlag = HEX_OUTPUT_ASM;
            break;

        case 'c':

            AslGbl_HexOutputFlag = HEX_OUTPUT_C;
            break;

        case 's':

            AslGbl_HexOutputFlag = HEX_OUTPUT_ASL;
            break;

        default:

            printf ("Unknown option: -t%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;

    case 'T':   /* Create a ACPI table template file */

        AslGbl_DoTemplates = TRUE;
        break;

    case 'v':   /* Version and verbosity settings */

        switch (AcpiGbl_Optarg[0])
        {
        case '^':

            printf (ACPI_COMMON_SIGNON (ASL_COMPILER_NAME));
            exit (0);

        case 'a':

            /* Disable all error/warning/remark messages */

            AslGbl_NoErrors = TRUE;
            break;

        case 'd':

            printf (ACPI_COMMON_SIGNON (ASL_COMPILER_NAME));
            printf (ACPI_COMMON_BUILD_TIME);
            exit (0);

        case 'e':

            /* Disable all warning/remark messages (errors only) */

            AslGbl_DisplayRemarks = FALSE;
            AslGbl_DisplayWarnings = FALSE;
            break;

        case 'i':
            /*
             * Support for integrated development environment(s).
             *
             * 1) No compiler signon
             * 2) Send stderr messages to stdout
             * 3) Less verbose error messages (single line only for each)
             * 4) Error/warning messages are formatted appropriately to
             *    be recognized by MS Visual Studio
             */
            AslGbl_VerboseErrors = FALSE;
            AslGbl_DoSignon = FALSE;
            break;

        case 'o':

            AslGbl_DisplayOptimizations = TRUE;
            break;

        case 'r':

            AslGbl_DisplayRemarks = FALSE;
            break;

        case 's':

            AslGbl_DoSignon = FALSE;
            break;

        case 't':

            AslGbl_VerboseTemplates = TRUE;
            break;

        case 'w':

            /* Get the required argument */

            if (AcpiGetoptArgument (argc, argv))
            {
                return (-1);
            }

            Status = AslDisableException (AcpiGbl_Optarg);
            if (ACPI_FAILURE (Status))
            {
                return (-1);
            }
            break;

        case 'x':

            /* Get the required argument */

            if (AcpiGetoptArgument (argc, argv))
            {
                return (-1);
            }

            Status = AslExpectException (AcpiGbl_Optarg);
            if (ACPI_FAILURE (Status))
            {
                return (-1);
            }
            break;

        default:

            printf ("Unknown option: -v%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;

    case 'w': /* Set warning levels */

        switch (AcpiGbl_Optarg[0])
        {
        case '1':

            AslGbl_WarningLevel = ASL_WARNING;
            break;

        case '2':

            AslGbl_WarningLevel = ASL_WARNING2;
            break;

        case '3':

            AslGbl_WarningLevel = ASL_WARNING3;
            break;

        case 'e':

            AslGbl_WarningsAsErrors = TRUE;
            break;

        case 'w':

            /* Get the required argument */

            if (AcpiGetoptArgument (argc, argv))
            {
                return (-1);
            }

            Status = AslElevateException (AcpiGbl_Optarg);
            if (ACPI_FAILURE (Status))
            {
                return (-1);
            }
            break;


        default:

            printf ("Unknown option: -w%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;

    case 'x':   /* Set debug print output level */

        AcpiDbgLevel = strtoul (AcpiGbl_Optarg, NULL, 16);
        break;

    case 'z':

        AslGbl_UseOriginalCompilerId = TRUE;
        break;

    default:

        return (-1);
    }

    return (0);
}


/*******************************************************************************
 *
 * FUNCTION:    AslMergeOptionTokens
 *
 * PARAMETERS:  InBuffer            - Input containing an option string
 *              OutBuffer           - Merged output buffer
 *
 * RETURN:      None
 *
 * DESCRIPTION: Remove all whitespace from an option string.
 *
 ******************************************************************************/

static void
AslMergeOptionTokens (
    char                    *InBuffer,
    char                    *OutBuffer)
{
    char                    *Token;


    *OutBuffer = 0;

    Token = strtok (InBuffer, ASL_TOKEN_SEPARATORS);
    while (Token)
    {
        strcat (OutBuffer, Token);
        Token = strtok (NULL, ASL_TOKEN_SEPARATORS);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AslDoResponseFile
 *
 * PARAMETERS:  Filename        - Name of the response file
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Open a response file and process all options within.
 *
 ******************************************************************************/

static int
AslDoResponseFile (
    char                    *Filename)
{
    char                    *argv = AslGbl_StringBuffer2;
    FILE                    *ResponseFile;
    int                     OptStatus = 0;
    int                     Opterr;
    int                     Optind;


    ResponseFile = fopen (Filename, "r");
    if (!ResponseFile)
    {
        printf ("Could not open command file %s, %s\n",
            Filename, strerror (errno));
        return (-1);
    }

    /* Must save the current GetOpt globals */

    Opterr = AcpiGbl_Opterr;
    Optind = AcpiGbl_Optind;

    /*
     * Process all lines in the response file. There must be one complete
     * option per line
     */
    while (fgets (AslGbl_StringBuffer, ASL_STRING_BUFFER_SIZE, ResponseFile))
    {
        /* Compress all tokens, allowing us to use a single argv entry */

        AslMergeOptionTokens (AslGbl_StringBuffer, AslGbl_StringBuffer2);

        /* Process the option */

        AcpiGbl_Opterr = 0;
        AcpiGbl_Optind = 0;

        OptStatus = AslDoOptions (1, &argv, TRUE);
        if (OptStatus)
        {
            printf ("Invalid option in command file %s: %s\n",
                Filename, AslGbl_StringBuffer);
            break;
        }
    }

    /* Restore the GetOpt globals */

    AcpiGbl_Opterr = Opterr;
    AcpiGbl_Optind = Optind;

    fclose (ResponseFile);
    return (OptStatus);
}

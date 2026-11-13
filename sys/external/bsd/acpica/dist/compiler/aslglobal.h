/******************************************************************************
 *
 * Module Name: aslglobal.h - Global variable definitions
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

#ifndef __ASLGLOBAL_H
#define __ASLGLOBAL_H


/*
 * Global variables. Defined in aslmain.c only, externed in all other files
 */

#undef ASL_EXTERN

#ifdef _DECLARE_GLOBALS
#define ASL_EXTERN
#define ASL_INIT_GLOBAL(a,b)        (a)=(b)
#else
#define ASL_EXTERN                  extern
#define ASL_INIT_GLOBAL(a,b)        (a)
#endif


#ifdef _DECLARE_GLOBALS
UINT32                              AslGbl_ExceptionCount[ASL_NUM_REPORT_LEVELS] = {0,0,0,0,0,0};

/* Table below must match ASL_FILE_TYPES in asltypes.h */
ASL_FILE_DESC                       AslGbl_FileDescs [ASL_NUM_FILES] =
{
    {"stdout:       ", "Standard Output"},
    {"stderr:       ", "Standard Error"},
    {"Table Input:  ", "Source Input"},
    {"Binary Output:", "AML Output"},
    {"Source Output:", "Source Output"},
    {"Preprocessor: ", "Preprocessor Output"},
    {"Preprocessor: ", "Preprocessor Temp File"},
    {"Listing File: ", "Listing Output"},
    {"Hex Dump:     ", "Hex Table Output"},
    {"Namespace:    ", "Namespace Output"},
    {"Debug File:   ", "Debug Output"},
    {"ASM Source:   ", "Assembly Code Output"},
    {"C Source:     ", "C Code Output"},
    {"ASM Include:  ", "Assembly Header Output"},
    {"C Include:    ", "C Header Output"},
    {"Offset Table: ", "C Offset Table Output"},
    {"Device Map:   ", "Device Map Output"},
    {"Cross Ref:    ", "Cross-reference Output"},
    {"Converter dbg:", "Converter debug Output"}
};

/* Table below must match the defines with the same names in actypes.h */

const char                          *AslGbl_OpFlagNames[ACPI_NUM_OP_FLAGS] =
{
    "OP_VISITED",
    "OP_AML_PACKAGE",
    "OP_IS_TARGET",
    "OP_IS_RESOURCE_DESC",
    "OP_IS_RESOURCE_FIELD",
    "OP_HAS_NO_EXIT",
    "OP_IF_HAS_NO_EXIT",
    "OP_NAME_INTERNALIZED",
    "OP_METHOD_NO_RETVAL",
    "OP_METHOD_SOME_NO_RETVAL",
    "OP_RESULT_NOT_USED",
    "OP_METHOD_TYPED",
    "OP_COULD_NOT_REDUCE",
    "OP_COMPILE_TIME_CONST",
    "OP_IS_TERM_ARG",
    "OP_WAS_ONES_OP",
    "OP_IS_NAME_DECLARATION",
    "OP_COMPILER_EMITTED",
    "OP_IS_DUPLICATE",
    "OP_IS_RESOURCE_DATA",
    "OP_IS_NULL_RETURN",
    "OP_NOT_FOUND_DURING_LOAD"
};

#else
extern ASL_FILE_DESC                AslGbl_FileDescs [ASL_NUM_FILES];
extern UINT32                       AslGbl_ExceptionCount[ASL_NUM_REPORT_LEVELS];
extern const char                   *AslGbl_OpFlagNames[ACPI_NUM_OP_FLAGS];
#endif


/*
 * Parser and other externals
 */
extern int                          yydebug;
extern FILE                         *AslCompilerin;
extern int                          DtParserdebug;
extern int                          PrParserdebug;
extern const ASL_MAPPING_ENTRY      AslKeywordMapping[];
extern char                         *AslCompilertext;

/*
 * Older versions of Bison won't emit this external in the generated header.
 * Newer versions do emit the external, so we don't need to do it.
 */
#ifndef ASLCOMPILER_ASLCOMPILERPARSE_H
extern int                  AslCompilerdebug;
#endif


#define ASL_DEFAULT_LINE_BUFFER_SIZE    (1024 * 32) /* 32K */
#define ASL_MSG_BUFFER_SIZE             (1024 * 128) /* 128k */
#define ASL_STRING_BUFFER_SIZE          (1024 * 32) /* 32k */
#define ASL_MAX_DISABLED_MESSAGES       32
#define ASL_MAX_EXPECTED_MESSAGES       32
#define ASL_MAX_ELEVATED_MESSAGES       32
#define HEX_TABLE_LINE_SIZE             8
#define HEX_LISTING_LINE_SIZE           8


/* Source code buffers and pointers for error reporting */

ASL_EXTERN char                     ASL_INIT_GLOBAL (*AslGbl_CurrentLineBuffer, NULL);
ASL_EXTERN char                     ASL_INIT_GLOBAL (*AslGbl_LineBufPtr, NULL);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_LineBufferSize, ASL_DEFAULT_LINE_BUFFER_SIZE);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_CurrentColumn, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_CurrentLineNumber, 1);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_LogicalLineNumber, 1);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_CurrentLineOffset, 0);
ASL_EXTERN UINT8                    ASL_INIT_GLOBAL (AslGbl_SyntaxError, 0);

/* Exception reporting */

ASL_EXTERN ASL_ERROR_MSG            ASL_INIT_GLOBAL (*AslGbl_ErrorLog,NULL);
ASL_EXTERN ASL_ERROR_MSG            ASL_INIT_GLOBAL (*AslGbl_NextError,NULL);

/* Option flags */

ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_DoCompile, TRUE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_DoSignon, TRUE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_PreprocessOnly, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_PreprocessFlag, TRUE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_DisassembleAll, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_UseDefaultAmlFilename, TRUE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_MapfileFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_NsOutputFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_PreprocessorOutputFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_KeepPreprocessorTempFile, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_DebugFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_CrossReferenceOutput, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_AsmOutputFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_C_OutputFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_C_OffsetTableFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_AsmIncludeOutputFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_C_IncludeOutputFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_ListingFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_IgnoreErrors, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_SourceOutputFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_ParseOnlyFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_ParserErrorDetected, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_CompileTimesFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_FoldConstants, TRUE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_VerboseErrors, TRUE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_NoErrors, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_WarningsAsErrors, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_NoResourceChecking, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_IntegerOptimizationFlag, TRUE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_ReferenceOptimizationFlag, TRUE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_DisplayRemarks, TRUE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_DisplayWarnings, TRUE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_DisplayOptimizations, FALSE);
ASL_EXTERN UINT8                    ASL_INIT_GLOBAL (AslGbl_WarningLevel, ASL_WARNING);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_UseOriginalCompilerId, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_VerboseTemplates, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_DoTemplates, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_CompileGeneric, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_AllExceptionsDisabled, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_PruneParseTree, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_DoTypechecking, TRUE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_EnableReferenceTypechecking, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_DoExternalsInPlace, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_DoAslConversion, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_OptimizeTrivialParseNodes, TRUE);


#define HEX_OUTPUT_NONE             0
#define HEX_OUTPUT_C                1
#define HEX_OUTPUT_ASM              2
#define HEX_OUTPUT_ASL              3

ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_HexOutputFlag, HEX_OUTPUT_NONE);


/* Files */

ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (AslGbl_HasIncludeFiles, FALSE);
ASL_EXTERN char                     *AslGbl_DirectoryPath;
ASL_EXTERN char                     *AslGbl_CurrentInputFilename;
ASL_EXTERN char                     ASL_INIT_GLOBAL (*AslGbl_IncludeFilename, NULL);
ASL_EXTERN char                     ASL_INIT_GLOBAL (*AslGbl_OutputFilenamePrefix, NULL);
ASL_EXTERN ASL_INCLUDE_DIR          ASL_INIT_GLOBAL (*AslGbl_IncludeDirList, NULL);
ASL_EXTERN char                     ASL_INIT_GLOBAL (*AslGbl_ExternalRefFilename, NULL);
ASL_EXTERN char                     ASL_INIT_GLOBAL (*AslGbl_PreviousIncludeFilename, NULL);

/* Statistics */

ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_InputByteCount, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_InputFieldCount, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_NsLookupCount, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_TotalKeywords, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_TotalNamedObjects, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_TotalExecutableOpcodes, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_TotalParseNodes, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_TotalMethods, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_TotalAllocations, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_TotalAllocated, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_TotalFolds, 0);

/* Local caches */

ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_ParseOpCount, 0);
ASL_EXTERN ASL_CACHE_INFO           ASL_INIT_GLOBAL (*AslGbl_ParseOpCacheList, NULL);
ASL_EXTERN ACPI_PARSE_OBJECT        ASL_INIT_GLOBAL (*AslGbl_ParseOpCacheNext, NULL);
ASL_EXTERN ACPI_PARSE_OBJECT        ASL_INIT_GLOBAL (*AslGbl_ParseOpCacheLast, NULL);

ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_StringCount, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_StringSize, 0);
ASL_EXTERN ASL_CACHE_INFO           ASL_INIT_GLOBAL (*AslGbl_StringCacheList, NULL);
ASL_EXTERN char                     ASL_INIT_GLOBAL (*AslGbl_StringCacheNext, NULL);
ASL_EXTERN char                     ASL_INIT_GLOBAL (*AslGbl_StringCacheLast, NULL);

/* Map file */

ASL_EXTERN ACPI_GPIO_INFO           ASL_INIT_GLOBAL (*AslGbl_GpioList, NULL);
ASL_EXTERN ACPI_SERIAL_INFO         ASL_INIT_GLOBAL (*AslGbl_SerialList, NULL);

/* Misc */

ASL_EXTERN UINT8                    ASL_INIT_GLOBAL (AslGbl_RevisionOverride, 0);
ASL_EXTERN UINT8                    ASL_INIT_GLOBAL (AslGbl_TempCount, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_TableLength, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_SourceLine, 0);
ASL_EXTERN UINT8                    ASL_INIT_GLOBAL (AslGbl_FileType, 0);
ASL_EXTERN char                     ASL_INIT_GLOBAL (*AslGbl_Signature, NULL);
ASL_EXTERN ACPI_PARSE_OBJECT        ASL_INIT_GLOBAL (*AslGbl_ParseTreeRoot, NULL);
ASL_EXTERN ACPI_PARSE_OBJECT        ASL_INIT_GLOBAL (*AslGbl_CurrentDB, NULL);
ASL_EXTERN ACPI_PARSE_OBJECT        ASL_INIT_GLOBAL (*AslGbl_ExternalsListHead, NULL);
ASL_EXTERN ASL_LISTING_NODE         ASL_INIT_GLOBAL (*AslGbl_ListingNode, NULL);
ASL_EXTERN ACPI_PARSE_OBJECT        *AslGbl_FirstLevelInsertionNode;

ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_CurrentHexColumn, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_CurrentAmlOffset, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_CurrentLine, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_DisabledMessagesIndex, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_ExpectedMessagesIndex, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_ElevatedMessagesIndex, 0);
ASL_EXTERN UINT8                    ASL_INIT_GLOBAL (AslGbl_HexBytesWereWritten, FALSE);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_NumNamespaceObjects, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (AslGbl_ReservedMethods, 0);
ASL_EXTERN UINT8                    ASL_INIT_GLOBAL (AslGbl_PruneDepth, 0);
ASL_EXTERN UINT16                   ASL_INIT_GLOBAL (AslGbl_PruneType, 0);
ASL_EXTERN ASL_FILE_NODE            ASL_INIT_GLOBAL (*AslGbl_IncludeFileStack, NULL);
ASL_EXTERN char                     ASL_INIT_GLOBAL (*AslGbl_TableSignature, "NO_SIG");
ASL_EXTERN char                     ASL_INIT_GLOBAL (*AslGbl_TableId, "NO_ID");
ASL_EXTERN ASL_FILE_INFO            ASL_INIT_GLOBAL (*AslGbl_Files, NULL);
ASL_EXTERN ASL_GLOBAL_FILE_NODE     ASL_INIT_GLOBAL (*AslGbl_FilesList, NULL);

/* Specific to the -q option */

ASL_EXTERN ASL_COMMENT_STATE        AslGbl_CommentState;

/*
 * Determines if an inline comment should be saved in the InlineComment or NodeEndComment
 *  field of ACPI_PARSE_OBJECT.
 */
ASL_EXTERN ACPI_COMMENT_NODE        ASL_INIT_GLOBAL (*AslGbl_CommentListHead, NULL);
ASL_EXTERN ACPI_COMMENT_NODE        ASL_INIT_GLOBAL (*AslGbl_CommentListTail, NULL);
ASL_EXTERN char                     ASL_INIT_GLOBAL (*AslGbl_InlineCommentBuffer, NULL);

/* Static structures */

ASL_EXTERN ASL_ANALYSIS_WALK_INFO   AslGbl_AnalysisWalkInfo;
ASL_EXTERN ACPI_TABLE_HEADER        AslGbl_TableHeader;

/* Event timing */

#define ASL_NUM_EVENTS              24
ASL_EXTERN ASL_EVENT_INFO           AslGbl_Events[ASL_NUM_EVENTS];
ASL_EXTERN UINT8                    AslGbl_NextEvent;
ASL_EXTERN UINT8                    AslGbl_NamespaceEvent;

/* Scratch buffers */

ASL_EXTERN UINT8                    AslGbl_AmlBuffer[HEX_LISTING_LINE_SIZE];
ASL_EXTERN char                     AslGbl_MsgBuffer[ASL_MSG_BUFFER_SIZE];
ASL_EXTERN char                     AslGbl_StringBuffer[ASL_STRING_BUFFER_SIZE];
ASL_EXTERN char                     AslGbl_StringBuffer2[ASL_STRING_BUFFER_SIZE];
ASL_EXTERN UINT32                   AslGbl_DisabledMessages[ASL_MAX_DISABLED_MESSAGES];
ASL_EXTERN ASL_EXPECTED_MESSAGE     AslGbl_ExpectedMessages[ASL_MAX_EXPECTED_MESSAGES];
ASL_EXTERN UINT32                   AslGbl_ElevatedMessages[ASL_MAX_ELEVATED_MESSAGES];


#endif /* __ASLGLOBAL_H */

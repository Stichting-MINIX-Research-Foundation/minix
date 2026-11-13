NoEcho('
/******************************************************************************
 *
 * Module Name: aslhelpers.y - helper and option terms
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

')


/*******************************************************************************
 *
 * ASL Helper Terms
 *
 ******************************************************************************/

OptionalBusMasterKeyword
    : ','                                   {$$ = TrCreateLeafOp (
                                                PARSEOP_BUSMASTERTYPE_MASTER);}
    | ',' PARSEOP_BUSMASTERTYPE_MASTER      {$$ = TrCreateLeafOp (
                                                PARSEOP_BUSMASTERTYPE_MASTER);}
    | ',' PARSEOP_BUSMASTERTYPE_NOTMASTER   {$$ = TrCreateLeafOp (
                                                PARSEOP_BUSMASTERTYPE_NOTMASTER);}
    ;

OptionalAccessAttribTerm
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' ByteConstExpr             {$$ = $2;}
    | ',' AccessAttribKeyword       {$$ = $2;}
    ;

OptionalAccessSize
    :                               {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_BYTECONST, 0);}
    | ','                           {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_BYTECONST, 0);}
    | ',' ByteConstExpr             {$$ = $2;}
    ;

OptionalAccessTypeKeyword   /* Default: AnyAcc */
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_ACCESSTYPE_ANY);}
    | ','                           {$$ = TrCreateLeafOp (
                                        PARSEOP_ACCESSTYPE_ANY);}
    | ',' AccessTypeKeyword         {$$ = $2;}
    ;

OptionalAddressingMode
    : ','                           {$$ = NULL;}
    | ',' AddressingModeKeyword     {$$ = $2;}
    ;

OptionalAddressRange
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' AddressKeyword            {$$ = $2;}
    ;

OptionalBitsPerByte
    : ','                           {$$ = NULL;}
    | ',' BitsPerByteKeyword        {$$ = $2;}
    ;

OptionalBuffer_Last
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' RawDataBufferTerm         {$$ = $2;}
    ;

OptionalByteConstExpr
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' ByteConstExpr             {$$ = $2;}
    ;

OptionalDecodeType
    : ','                           {$$ = NULL;}
    | ',' DecodeKeyword             {$$ = $2;}
    ;

OptionalDevicePolarity
    : ','                           {$$ = NULL;}
    | ',' DevicePolarityKeyword     {$$ = $2;}
    ;

OptionalDWordConstExpr
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' DWordConstExpr            {$$ = $2;}
    ;

OptionalEndian
    : ','                           {$$ = NULL;}
    | ',' EndianKeyword             {$$ = $2;}
    ;

OptionalFlowControl
    : ','                           {$$ = NULL;}
    | ',' FlowControlKeyword        {$$ = $2;}
    ;

OptionalIoRestriction
    : ','                           {$$ = NULL;}
    | ',' IoRestrictionKeyword      {$$ = $2;}
    ;

OptionalListString
    :                               {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_STRING_LITERAL,
                                        ACPI_TO_INTEGER (""));}   /* Placeholder is a NULL string */
    | ','                           {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_STRING_LITERAL,
                                        ACPI_TO_INTEGER (""));}   /* Placeholder is a NULL string */
    | ',' TermArg                   {$$ = $2;}
    ;

OptionalLockRuleKeyword     /* Default: NoLock */
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_LOCKRULE_NOLOCK);}
    | ','                           {$$ = TrCreateLeafOp (
                                        PARSEOP_LOCKRULE_NOLOCK);}
    | ',' LockRuleKeyword           {$$ = $2;}
    ;

OptionalMaxType
    : ','                           {$$ = NULL;}
    | ',' MaxKeyword                {$$ = $2;}
    ;

OptionalMemType
    : ','                           {$$ = NULL;}
    | ',' MemTypeKeyword            {$$ = $2;}
    ;

OptionalMinType
    : ','                           {$$ = NULL;}
    | ',' MinKeyword                {$$ = $2;}
    ;

OptionalNameString
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' NameString                {$$ = $2;}
    ;

OptionalNameString_Last
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' NameString                {$$ = $2;}
    ;

OptionalNameString_First
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_ZERO);}
    | NameString                    {$$ = $1;}
    ;

OptionalObjectTypeKeyword
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_OBJECTTYPE_UNK);}
    | ',' ObjectTypeKeyword         {$$ = $2;}
    ;

OptionalParityType
    : ','                           {$$ = NULL;}
    | ',' ParityTypeKeyword         {$$ = $2;}
    ;

OptionalQWordConstExpr
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' QWordConstExpr            {$$ = $2;}
    ;

OptionalRangeType
    : ','                           {$$ = NULL;}
    | ',' RangeTypeKeyword          {$$ = $2;}
    ;

OptionalReadWriteKeyword
    :                                   {$$ = TrCreateLeafOp (
                                            PARSEOP_READWRITETYPE_BOTH);}
    | PARSEOP_READWRITETYPE_BOTH        {$$ = TrCreateLeafOp (
                                            PARSEOP_READWRITETYPE_BOTH);}
    | PARSEOP_READWRITETYPE_READONLY    {$$ = TrCreateLeafOp (
                                            PARSEOP_READWRITETYPE_READONLY);}
    ;

OptionalResourceType_First
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_RESOURCETYPE_CONSUMER);}
    | ResourceTypeKeyword           {$$ = $1;}
    ;

OptionalResourceType
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_RESOURCETYPE_CONSUMER);}
    | ','                           {$$ = TrCreateLeafOp (
                                        PARSEOP_RESOURCETYPE_CONSUMER);}
    | ',' ResourceTypeKeyword       {$$ = $2;}
    ;

/* Same as above except default is producer */
OptionalProducerResourceType
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_RESOURCETYPE_PRODUCER);}
    | ','                           {$$ = TrCreateLeafOp (
                                        PARSEOP_RESOURCETYPE_PRODUCER);}
    | ',' ResourceTypeKeyword       {$$ = $2;}
    ;

OptionalSlaveMode
    : ','                           {$$ = NULL;}
    | ',' SlaveModeKeyword          {$$ = $2;}
    ;

OptionalShareType
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' ShareTypeKeyword          {$$ = $2;}
    ;

OptionalShareType_First
    :                               {$$ = NULL;}
    | ShareTypeKeyword              {$$ = $1;}
    ;

OptionalStopBits
    : ','                           {$$ = NULL;}
    | ',' StopBitsKeyword           {$$ = $2;}
    ;

OptionalStringData
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' StringData                {$$ = $2;}
    ;

OptionalSyncLevel           /* Default: 0 */
    :                               {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_BYTECONST, 0);}
    | ','                           {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_BYTECONST, 0);}
    | ',' ByteConstExpr             {$$ = $2;}
    ;

OptionalTranslationType_Last
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' TranslationKeyword        {$$ = $2;}
    ;

OptionalType
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' TypeKeyword               {$$ = $2;}
    ;

OptionalType_Last
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' TypeKeyword               {$$ = $2;}
    ;

OptionalUpdateRuleKeyword   /* Default: Preserve */
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_UPDATERULE_PRESERVE);}
    | ','                           {$$ = TrCreateLeafOp (
                                        PARSEOP_UPDATERULE_PRESERVE);}
    | ',' UpdateRuleKeyword         {$$ = $2;}
    ;

OptionalWireMode
    : ','                           {$$ = NULL;}
    | ',' WireModeKeyword           {$$ = $2;}
    ;

OptionalWordConstExpr
    : ','                           {$$ = NULL;}
    | ',' WordConstExpr             {$$ = $2;}
    ;

OptionalXferSize
    :                               {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_XFERSIZE_32, 2);}
    | ','                           {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_XFERSIZE_32, 2);}
    | ',' XferSizeKeyword           {$$ = $2;}
    ;

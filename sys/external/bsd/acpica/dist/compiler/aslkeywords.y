NoEcho('
/******************************************************************************
 *
 * Module Name: aslkeywords.y - Rules for resource descriptor keywords
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
 * ASL Parameter Keyword Terms
 *
 ******************************************************************************/

AccessAttribKeyword
    : PARSEOP_ACCESSATTRIB_BLOCK            {$$ = TrCreateLeafOp (PARSEOP_ACCESSATTRIB_BLOCK);}
    | PARSEOP_ACCESSATTRIB_BLOCK_CALL       {$$ = TrCreateLeafOp (PARSEOP_ACCESSATTRIB_BLOCK_CALL);}
    | PARSEOP_ACCESSATTRIB_BYTE             {$$ = TrCreateLeafOp (PARSEOP_ACCESSATTRIB_BYTE);}
    | PARSEOP_ACCESSATTRIB_QUICK            {$$ = TrCreateLeafOp (PARSEOP_ACCESSATTRIB_QUICK );}
    | PARSEOP_ACCESSATTRIB_SND_RCV          {$$ = TrCreateLeafOp (PARSEOP_ACCESSATTRIB_SND_RCV);}
    | PARSEOP_ACCESSATTRIB_WORD             {$$ = TrCreateLeafOp (PARSEOP_ACCESSATTRIB_WORD);}
    | PARSEOP_ACCESSATTRIB_WORD_CALL        {$$ = TrCreateLeafOp (PARSEOP_ACCESSATTRIB_WORD_CALL);}
    | PARSEOP_ACCESSATTRIB_BYTES
        PARSEOP_OPEN_PAREN                  {$<n>$ = TrCreateLeafOp (PARSEOP_ACCESSATTRIB_BYTES);}
        ByteConst
        PARSEOP_CLOSE_PAREN                 {$$ = TrLinkOpChildren ($<n>3,1,$4);}
    | PARSEOP_ACCESSATTRIB_RAW_BYTES
        PARSEOP_OPEN_PAREN                  {$<n>$ = TrCreateLeafOp (PARSEOP_ACCESSATTRIB_RAW_BYTES);}
        ByteConst
        PARSEOP_CLOSE_PAREN                 {$$ = TrLinkOpChildren ($<n>3,1,$4);}
    | PARSEOP_ACCESSATTRIB_RAW_PROCESS
        PARSEOP_OPEN_PAREN                  {$<n>$ = TrCreateLeafOp (PARSEOP_ACCESSATTRIB_RAW_PROCESS);}
        ByteConst
        PARSEOP_CLOSE_PAREN                 {$$ = TrLinkOpChildren ($<n>3,1,$4);}
    ;

AccessTypeKeyword
    : PARSEOP_ACCESSTYPE_ANY                {$$ = TrCreateLeafOp (PARSEOP_ACCESSTYPE_ANY);}
    | PARSEOP_ACCESSTYPE_BYTE               {$$ = TrCreateLeafOp (PARSEOP_ACCESSTYPE_BYTE);}
    | PARSEOP_ACCESSTYPE_WORD               {$$ = TrCreateLeafOp (PARSEOP_ACCESSTYPE_WORD);}
    | PARSEOP_ACCESSTYPE_DWORD              {$$ = TrCreateLeafOp (PARSEOP_ACCESSTYPE_DWORD);}
    | PARSEOP_ACCESSTYPE_QWORD              {$$ = TrCreateLeafOp (PARSEOP_ACCESSTYPE_QWORD);}
    | PARSEOP_ACCESSTYPE_BUF                {$$ = TrCreateLeafOp (PARSEOP_ACCESSTYPE_BUF);}
    ;

AddressingModeKeyword
    : PARSEOP_ADDRESSINGMODE_7BIT           {$$ = TrCreateLeafOp (PARSEOP_ADDRESSINGMODE_7BIT);}
    | PARSEOP_ADDRESSINGMODE_10BIT          {$$ = TrCreateLeafOp (PARSEOP_ADDRESSINGMODE_10BIT);}
    ;

AddressKeyword
    : PARSEOP_ADDRESSTYPE_MEMORY            {$$ = TrCreateLeafOp (PARSEOP_ADDRESSTYPE_MEMORY);}
    | PARSEOP_ADDRESSTYPE_RESERVED          {$$ = TrCreateLeafOp (PARSEOP_ADDRESSTYPE_RESERVED);}
    | PARSEOP_ADDRESSTYPE_NVS               {$$ = TrCreateLeafOp (PARSEOP_ADDRESSTYPE_NVS);}
    | PARSEOP_ADDRESSTYPE_ACPI              {$$ = TrCreateLeafOp (PARSEOP_ADDRESSTYPE_ACPI);}
    ;

AddressSpaceKeyword
    : ByteConst                             {$$ = UtCheckIntegerRange ($1, 0x0A, 0xFF);}
    | RegionSpaceKeyword                    {}
    ;

BitsPerByteKeyword
    : PARSEOP_BITSPERBYTE_FIVE              {$$ = TrCreateLeafOp (PARSEOP_BITSPERBYTE_FIVE);}
    | PARSEOP_BITSPERBYTE_SIX               {$$ = TrCreateLeafOp (PARSEOP_BITSPERBYTE_SIX);}
    | PARSEOP_BITSPERBYTE_SEVEN             {$$ = TrCreateLeafOp (PARSEOP_BITSPERBYTE_SEVEN);}
    | PARSEOP_BITSPERBYTE_EIGHT             {$$ = TrCreateLeafOp (PARSEOP_BITSPERBYTE_EIGHT);}
    | PARSEOP_BITSPERBYTE_NINE              {$$ = TrCreateLeafOp (PARSEOP_BITSPERBYTE_NINE);}
    ;

ClockPhaseKeyword
    : PARSEOP_CLOCKPHASE_FIRST              {$$ = TrCreateLeafOp (PARSEOP_CLOCKPHASE_FIRST);}
    | PARSEOP_CLOCKPHASE_SECOND             {$$ = TrCreateLeafOp (PARSEOP_CLOCKPHASE_SECOND);}
    ;

ClockPolarityKeyword
    : PARSEOP_CLOCKPOLARITY_LOW             {$$ = TrCreateLeafOp (PARSEOP_CLOCKPOLARITY_LOW);}
    | PARSEOP_CLOCKPOLARITY_HIGH            {$$ = TrCreateLeafOp (PARSEOP_CLOCKPOLARITY_HIGH);}
    ;

DecodeKeyword
    : PARSEOP_DECODETYPE_POS                {$$ = TrCreateLeafOp (PARSEOP_DECODETYPE_POS);}
    | PARSEOP_DECODETYPE_SUB                {$$ = TrCreateLeafOp (PARSEOP_DECODETYPE_SUB);}
    ;

DevicePolarityKeyword
    : PARSEOP_DEVICEPOLARITY_LOW            {$$ = TrCreateLeafOp (PARSEOP_DEVICEPOLARITY_LOW);}
    | PARSEOP_DEVICEPOLARITY_HIGH           {$$ = TrCreateLeafOp (PARSEOP_DEVICEPOLARITY_HIGH);}
    ;

DMATypeKeyword
    : PARSEOP_DMATYPE_A                     {$$ = TrCreateLeafOp (PARSEOP_DMATYPE_A);}
    | PARSEOP_DMATYPE_COMPATIBILITY         {$$ = TrCreateLeafOp (PARSEOP_DMATYPE_COMPATIBILITY);}
    | PARSEOP_DMATYPE_B                     {$$ = TrCreateLeafOp (PARSEOP_DMATYPE_B);}
    | PARSEOP_DMATYPE_F                     {$$ = TrCreateLeafOp (PARSEOP_DMATYPE_F);}
    ;

EndianKeyword
    : PARSEOP_ENDIAN_LITTLE                 {$$ = TrCreateLeafOp (PARSEOP_ENDIAN_LITTLE);}
    | PARSEOP_ENDIAN_BIG                    {$$ = TrCreateLeafOp (PARSEOP_ENDIAN_BIG);}
    ;

FlowControlKeyword
    : PARSEOP_FLOWCONTROL_HW                {$$ = TrCreateLeafOp (PARSEOP_FLOWCONTROL_HW);}
    | PARSEOP_FLOWCONTROL_NONE              {$$ = TrCreateLeafOp (PARSEOP_FLOWCONTROL_NONE);}
    | PARSEOP_FLOWCONTROL_SW                {$$ = TrCreateLeafOp (PARSEOP_FLOWCONTROL_SW);}
    ;

InterruptLevel
    : PARSEOP_INTLEVEL_ACTIVEBOTH           {$$ = TrCreateLeafOp (PARSEOP_INTLEVEL_ACTIVEBOTH);}
    | PARSEOP_INTLEVEL_ACTIVEHIGH           {$$ = TrCreateLeafOp (PARSEOP_INTLEVEL_ACTIVEHIGH);}
    | PARSEOP_INTLEVEL_ACTIVELOW            {$$ = TrCreateLeafOp (PARSEOP_INTLEVEL_ACTIVELOW);}
    ;

InterruptTypeKeyword
    : PARSEOP_INTTYPE_EDGE                  {$$ = TrCreateLeafOp (PARSEOP_INTTYPE_EDGE);}
    | PARSEOP_INTTYPE_LEVEL                 {$$ = TrCreateLeafOp (PARSEOP_INTTYPE_LEVEL);}
    ;

IODecodeKeyword
    : PARSEOP_IODECODETYPE_16               {$$ = TrCreateLeafOp (PARSEOP_IODECODETYPE_16);}
    | PARSEOP_IODECODETYPE_10               {$$ = TrCreateLeafOp (PARSEOP_IODECODETYPE_10);}
    ;

IoRestrictionKeyword
    : PARSEOP_IORESTRICT_IN                 {$$ = TrCreateLeafOp (PARSEOP_IORESTRICT_IN);}
    | PARSEOP_IORESTRICT_OUT                {$$ = TrCreateLeafOp (PARSEOP_IORESTRICT_OUT);}
    | PARSEOP_IORESTRICT_NONE               {$$ = TrCreateLeafOp (PARSEOP_IORESTRICT_NONE);}
    | PARSEOP_IORESTRICT_PRESERVE           {$$ = TrCreateLeafOp (PARSEOP_IORESTRICT_PRESERVE);}
    ;

LockRuleKeyword
    : PARSEOP_LOCKRULE_LOCK                 {$$ = TrCreateLeafOp (PARSEOP_LOCKRULE_LOCK);}
    | PARSEOP_LOCKRULE_NOLOCK               {$$ = TrCreateLeafOp (PARSEOP_LOCKRULE_NOLOCK);}
    ;

MatchOpKeyword
    : PARSEOP_MATCHTYPE_MTR                 {$$ = TrCreateLeafOp (PARSEOP_MATCHTYPE_MTR);}
    | PARSEOP_MATCHTYPE_MEQ                 {$$ = TrCreateLeafOp (PARSEOP_MATCHTYPE_MEQ);}
    | PARSEOP_MATCHTYPE_MLE                 {$$ = TrCreateLeafOp (PARSEOP_MATCHTYPE_MLE);}
    | PARSEOP_MATCHTYPE_MLT                 {$$ = TrCreateLeafOp (PARSEOP_MATCHTYPE_MLT);}
    | PARSEOP_MATCHTYPE_MGE                 {$$ = TrCreateLeafOp (PARSEOP_MATCHTYPE_MGE);}
    | PARSEOP_MATCHTYPE_MGT                 {$$ = TrCreateLeafOp (PARSEOP_MATCHTYPE_MGT);}
    ;

MaxKeyword
    : PARSEOP_MAXTYPE_FIXED                 {$$ = TrCreateLeafOp (PARSEOP_MAXTYPE_FIXED);}
    | PARSEOP_MAXTYPE_NOTFIXED              {$$ = TrCreateLeafOp (PARSEOP_MAXTYPE_NOTFIXED);}
    ;

MemTypeKeyword
    : PARSEOP_MEMTYPE_CACHEABLE             {$$ = TrCreateLeafOp (PARSEOP_MEMTYPE_CACHEABLE);}
    | PARSEOP_MEMTYPE_WRITECOMBINING        {$$ = TrCreateLeafOp (PARSEOP_MEMTYPE_WRITECOMBINING);}
    | PARSEOP_MEMTYPE_PREFETCHABLE          {$$ = TrCreateLeafOp (PARSEOP_MEMTYPE_PREFETCHABLE);}
    | PARSEOP_MEMTYPE_NONCACHEABLE          {$$ = TrCreateLeafOp (PARSEOP_MEMTYPE_NONCACHEABLE);}
    ;

MinKeyword
    : PARSEOP_MINTYPE_FIXED                 {$$ = TrCreateLeafOp (PARSEOP_MINTYPE_FIXED);}
    | PARSEOP_MINTYPE_NOTFIXED              {$$ = TrCreateLeafOp (PARSEOP_MINTYPE_NOTFIXED);}
    ;

ObjectTypeKeyword
    : PARSEOP_OBJECTTYPE_UNK                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_UNK);}
    | PARSEOP_OBJECTTYPE_INT                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_INT);}
    | PARSEOP_OBJECTTYPE_STR                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_STR);}
    | PARSEOP_OBJECTTYPE_BUF                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_BUF);}
    | PARSEOP_OBJECTTYPE_PKG                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_PKG);}
    | PARSEOP_OBJECTTYPE_FLD                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_FLD);}
    | PARSEOP_OBJECTTYPE_DEV                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_DEV);}
    | PARSEOP_OBJECTTYPE_EVT                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_EVT);}
    | PARSEOP_OBJECTTYPE_MTH                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_MTH);}
    | PARSEOP_OBJECTTYPE_MTX                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_MTX);}
    | PARSEOP_OBJECTTYPE_OPR                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_OPR);}
    | PARSEOP_OBJECTTYPE_POW                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_POW);}
    | PARSEOP_OBJECTTYPE_PRO                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_PRO);}
    | PARSEOP_OBJECTTYPE_THZ                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_THZ);}
    | PARSEOP_OBJECTTYPE_BFF                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_BFF);}
    | PARSEOP_OBJECTTYPE_DDB                {$$ = TrCreateLeafOp (PARSEOP_OBJECTTYPE_DDB);}
    ;

ParityTypeKeyword
    : PARSEOP_PARITYTYPE_SPACE              {$$ = TrCreateLeafOp (PARSEOP_PARITYTYPE_SPACE);}
    | PARSEOP_PARITYTYPE_MARK               {$$ = TrCreateLeafOp (PARSEOP_PARITYTYPE_MARK);}
    | PARSEOP_PARITYTYPE_ODD                {$$ = TrCreateLeafOp (PARSEOP_PARITYTYPE_ODD);}
    | PARSEOP_PARITYTYPE_EVEN               {$$ = TrCreateLeafOp (PARSEOP_PARITYTYPE_EVEN);}
    | PARSEOP_PARITYTYPE_NONE               {$$ = TrCreateLeafOp (PARSEOP_PARITYTYPE_NONE);}
    ;

PinConfigByte
    : PinConfigKeyword                      {$$ = $1;}
    | ByteConstExpr                         {$$ = UtCheckIntegerRange ($1, 0x80, 0xFF);}
    ;

PinConfigKeyword
    : PARSEOP_PIN_NOPULL                    {$$ = TrCreateLeafOp (PARSEOP_PIN_NOPULL);}
    | PARSEOP_PIN_PULLDOWN                  {$$ = TrCreateLeafOp (PARSEOP_PIN_PULLDOWN);}
    | PARSEOP_PIN_PULLUP                    {$$ = TrCreateLeafOp (PARSEOP_PIN_PULLUP);}
    | PARSEOP_PIN_PULLDEFAULT               {$$ = TrCreateLeafOp (PARSEOP_PIN_PULLDEFAULT);}
    ;

PldKeyword
    : PARSEOP_PLD_REVISION                  {$$ = TrCreateLeafOp (PARSEOP_PLD_REVISION);}
    | PARSEOP_PLD_IGNORECOLOR               {$$ = TrCreateLeafOp (PARSEOP_PLD_IGNORECOLOR);}
    | PARSEOP_PLD_RED                       {$$ = TrCreateLeafOp (PARSEOP_PLD_RED);}
    | PARSEOP_PLD_GREEN                     {$$ = TrCreateLeafOp (PARSEOP_PLD_GREEN);}
    | PARSEOP_PLD_BLUE                      {$$ = TrCreateLeafOp (PARSEOP_PLD_BLUE);}
    | PARSEOP_PLD_WIDTH                     {$$ = TrCreateLeafOp (PARSEOP_PLD_WIDTH);}
    | PARSEOP_PLD_HEIGHT                    {$$ = TrCreateLeafOp (PARSEOP_PLD_HEIGHT);}
    | PARSEOP_PLD_USERVISIBLE               {$$ = TrCreateLeafOp (PARSEOP_PLD_USERVISIBLE);}
    | PARSEOP_PLD_DOCK                      {$$ = TrCreateLeafOp (PARSEOP_PLD_DOCK);}
    | PARSEOP_PLD_LID                       {$$ = TrCreateLeafOp (PARSEOP_PLD_LID);}
    | PARSEOP_PLD_PANEL                     {$$ = TrCreateLeafOp (PARSEOP_PLD_PANEL);}
    | PARSEOP_PLD_VERTICALPOSITION          {$$ = TrCreateLeafOp (PARSEOP_PLD_VERTICALPOSITION);}
    | PARSEOP_PLD_HORIZONTALPOSITION        {$$ = TrCreateLeafOp (PARSEOP_PLD_HORIZONTALPOSITION);}
    | PARSEOP_PLD_SHAPE                     {$$ = TrCreateLeafOp (PARSEOP_PLD_SHAPE);}
    | PARSEOP_PLD_GROUPORIENTATION          {$$ = TrCreateLeafOp (PARSEOP_PLD_GROUPORIENTATION);}
    | PARSEOP_PLD_GROUPTOKEN                {$$ = TrCreateLeafOp (PARSEOP_PLD_GROUPTOKEN);}
    | PARSEOP_PLD_GROUPPOSITION             {$$ = TrCreateLeafOp (PARSEOP_PLD_GROUPPOSITION);}
    | PARSEOP_PLD_BAY                       {$$ = TrCreateLeafOp (PARSEOP_PLD_BAY);}
    | PARSEOP_PLD_EJECTABLE                 {$$ = TrCreateLeafOp (PARSEOP_PLD_EJECTABLE);}
    | PARSEOP_PLD_EJECTREQUIRED             {$$ = TrCreateLeafOp (PARSEOP_PLD_EJECTREQUIRED);}
    | PARSEOP_PLD_CABINETNUMBER             {$$ = TrCreateLeafOp (PARSEOP_PLD_CABINETNUMBER);}
    | PARSEOP_PLD_CARDCAGENUMBER            {$$ = TrCreateLeafOp (PARSEOP_PLD_CARDCAGENUMBER);}
    | PARSEOP_PLD_REFERENCE                 {$$ = TrCreateLeafOp (PARSEOP_PLD_REFERENCE);}
    | PARSEOP_PLD_ROTATION                  {$$ = TrCreateLeafOp (PARSEOP_PLD_ROTATION);}
    | PARSEOP_PLD_ORDER                     {$$ = TrCreateLeafOp (PARSEOP_PLD_ORDER);}
    | PARSEOP_PLD_RESERVED                  {$$ = TrCreateLeafOp (PARSEOP_PLD_RESERVED);}
    | PARSEOP_PLD_VERTICALOFFSET            {$$ = TrCreateLeafOp (PARSEOP_PLD_VERTICALOFFSET);}
    | PARSEOP_PLD_HORIZONTALOFFSET          {$$ = TrCreateLeafOp (PARSEOP_PLD_HORIZONTALOFFSET);}
    ;

RangeTypeKeyword
    : PARSEOP_RANGETYPE_ISAONLY             {$$ = TrCreateLeafOp (PARSEOP_RANGETYPE_ISAONLY);}
    | PARSEOP_RANGETYPE_NONISAONLY          {$$ = TrCreateLeafOp (PARSEOP_RANGETYPE_NONISAONLY);}
    | PARSEOP_RANGETYPE_ENTIRE              {$$ = TrCreateLeafOp (PARSEOP_RANGETYPE_ENTIRE);}
    ;

RegionSpaceKeyword
    : PARSEOP_REGIONSPACE_IO                {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_IO);}
    | PARSEOP_REGIONSPACE_MEM               {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_MEM);}
    | PARSEOP_REGIONSPACE_PCI               {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_PCI);}
    | PARSEOP_REGIONSPACE_EC                {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_EC);}
    | PARSEOP_REGIONSPACE_SMBUS             {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_SMBUS);}
    | PARSEOP_REGIONSPACE_CMOS              {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_CMOS);}
    | PARSEOP_REGIONSPACE_PCIBAR            {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_PCIBAR);}
    | PARSEOP_REGIONSPACE_IPMI              {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_IPMI);}
    | PARSEOP_REGIONSPACE_GPIO              {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_GPIO);}
    | PARSEOP_REGIONSPACE_GSBUS             {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_GSBUS);}
    | PARSEOP_REGIONSPACE_PCC               {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_PCC);}
    | PARSEOP_REGIONSPACE_FFIXEDHW          {$$ = TrCreateLeafOp (PARSEOP_REGIONSPACE_FFIXEDHW);}
    ;

ResourceTypeKeyword
    : PARSEOP_RESOURCETYPE_CONSUMER         {$$ = TrCreateLeafOp (PARSEOP_RESOURCETYPE_CONSUMER);}
    | PARSEOP_RESOURCETYPE_PRODUCER         {$$ = TrCreateLeafOp (PARSEOP_RESOURCETYPE_PRODUCER);}
    ;

SerializeRuleKeyword
    : PARSEOP_SERIALIZERULE_SERIAL          {$$ = TrCreateLeafOp (PARSEOP_SERIALIZERULE_SERIAL);}
    | PARSEOP_SERIALIZERULE_NOTSERIAL       {$$ = TrCreateLeafOp (PARSEOP_SERIALIZERULE_NOTSERIAL);}
    ;

ShareTypeKeyword
    : PARSEOP_SHARETYPE_SHARED              {$$ = TrCreateLeafOp (PARSEOP_SHARETYPE_SHARED);}
    | PARSEOP_SHARETYPE_EXCLUSIVE           {$$ = TrCreateLeafOp (PARSEOP_SHARETYPE_EXCLUSIVE);}
    | PARSEOP_SHARETYPE_SHAREDWAKE          {$$ = TrCreateLeafOp (PARSEOP_SHARETYPE_SHAREDWAKE);}
    | PARSEOP_SHARETYPE_EXCLUSIVEWAKE       {$$ = TrCreateLeafOp (PARSEOP_SHARETYPE_EXCLUSIVEWAKE);}
   ;

SlaveModeKeyword
    : PARSEOP_SLAVEMODE_CONTROLLERINIT      {$$ = TrCreateLeafOp (PARSEOP_SLAVEMODE_CONTROLLERINIT);}
    | PARSEOP_SLAVEMODE_DEVICEINIT          {$$ = TrCreateLeafOp (PARSEOP_SLAVEMODE_DEVICEINIT);}
    ;

StopBitsKeyword
    : PARSEOP_STOPBITS_TWO                  {$$ = TrCreateLeafOp (PARSEOP_STOPBITS_TWO);}
    | PARSEOP_STOPBITS_ONEPLUSHALF          {$$ = TrCreateLeafOp (PARSEOP_STOPBITS_ONEPLUSHALF);}
    | PARSEOP_STOPBITS_ONE                  {$$ = TrCreateLeafOp (PARSEOP_STOPBITS_ONE);}
    | PARSEOP_STOPBITS_ZERO                 {$$ = TrCreateLeafOp (PARSEOP_STOPBITS_ZERO);}
    ;

TranslationKeyword
    : PARSEOP_TRANSLATIONTYPE_SPARSE        {$$ = TrCreateLeafOp (PARSEOP_TRANSLATIONTYPE_SPARSE);}
    | PARSEOP_TRANSLATIONTYPE_DENSE         {$$ = TrCreateLeafOp (PARSEOP_TRANSLATIONTYPE_DENSE);}
    ;

TypeKeyword
    : PARSEOP_TYPE_TRANSLATION              {$$ = TrCreateLeafOp (PARSEOP_TYPE_TRANSLATION);}
    | PARSEOP_TYPE_STATIC                   {$$ = TrCreateLeafOp (PARSEOP_TYPE_STATIC);}
    ;

UpdateRuleKeyword
    : PARSEOP_UPDATERULE_PRESERVE           {$$ = TrCreateLeafOp (PARSEOP_UPDATERULE_PRESERVE);}
    | PARSEOP_UPDATERULE_ONES               {$$ = TrCreateLeafOp (PARSEOP_UPDATERULE_ONES);}
    | PARSEOP_UPDATERULE_ZEROS              {$$ = TrCreateLeafOp (PARSEOP_UPDATERULE_ZEROS);}
    ;

WireModeKeyword
    : PARSEOP_WIREMODE_FOUR                 {$$ = TrCreateLeafOp (PARSEOP_WIREMODE_FOUR);}
    | PARSEOP_WIREMODE_THREE                {$$ = TrCreateLeafOp (PARSEOP_WIREMODE_THREE);}
    ;

XferSizeKeyword
    : PARSEOP_XFERSIZE_8                    {$$ = TrCreateValuedLeafOp (PARSEOP_XFERSIZE_8,   0);}
    | PARSEOP_XFERSIZE_16                   {$$ = TrCreateValuedLeafOp (PARSEOP_XFERSIZE_16,  1);}
    | PARSEOP_XFERSIZE_32                   {$$ = TrCreateValuedLeafOp (PARSEOP_XFERSIZE_32,  2);}
    | PARSEOP_XFERSIZE_64                   {$$ = TrCreateValuedLeafOp (PARSEOP_XFERSIZE_64,  3);}
    | PARSEOP_XFERSIZE_128                  {$$ = TrCreateValuedLeafOp (PARSEOP_XFERSIZE_128, 4);}
    | PARSEOP_XFERSIZE_256                  {$$ = TrCreateValuedLeafOp (PARSEOP_XFERSIZE_256, 5);}
    ;

XferTypeKeyword
    : PARSEOP_XFERTYPE_8                    {$$ = TrCreateLeafOp (PARSEOP_XFERTYPE_8);}
    | PARSEOP_XFERTYPE_8_16                 {$$ = TrCreateLeafOp (PARSEOP_XFERTYPE_8_16);}
    | PARSEOP_XFERTYPE_16                   {$$ = TrCreateLeafOp (PARSEOP_XFERTYPE_16);}
    ;

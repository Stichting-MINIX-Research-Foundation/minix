NoEcho('
/******************************************************************************
 *
 * Module Name: aslresources.y - Bison/Yacc production rules for resources
 *                             - Keep this file synched with the
 *                               CvParseOpBlockType function in cvcompiler.c
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
 * ASL Resource Template Terms
 *
 ******************************************************************************/

/*
 * Note: Create two default nodes to allow conversion to a Buffer AML opcode
 * Also, insert the EndTag at the end of the template.
 */
ResourceTemplateTerm
    : PARSEOP_RESOURCETEMPLATE      {COMMENT_CAPTURE_OFF;}
        OptionalParentheses
        '{'
        ResourceMacroList '}'       {$$ = TrCreateOp (PARSEOP_RESOURCETEMPLATE,4,
                                          TrCreateLeafOp (PARSEOP_DEFAULT_ARG),
                                          TrCreateLeafOp (PARSEOP_DEFAULT_ARG),
                                          $5,
                                          TrCreateLeafOp (PARSEOP_ENDTAG));
                                     COMMENT_CAPTURE_ON;}
    ;

OptionalParentheses
    :                               {$$ = NULL;}
    | PARSEOP_OPEN_PAREN
        PARSEOP_CLOSE_PAREN         {$$ = NULL;}
    ;

ResourceMacroList
    :                               {$$ = NULL;}
    | ResourceMacroList
        ResourceMacroTerm           {$$ = TrLinkPeerOp ($1,$2);}
    ;

ResourceMacroTerm
    : DMATerm                       {}
    | DWordIOTerm                   {}
    | DWordMemoryTerm               {}
    | DWordSpaceTerm                {}
    | EndDependentFnTerm            {}
    | ExtendedIOTerm                {}
    | ExtendedMemoryTerm            {}
    | ExtendedSpaceTerm             {}
    | FixedDmaTerm                  {}
    | FixedIOTerm                   {}
    | GpioIntTerm                   {}
    | GpioIoTerm                    {}
    | I2cSerialBusTerm              {}
    | I2cSerialBusTermV2            {}
    | InterruptTerm                 {}
    | IOTerm                        {}
    | IRQNoFlagsTerm                {}
    | IRQTerm                       {}
    | Memory24Term                  {}
    | Memory32FixedTerm             {}
    | Memory32Term                  {}
    | PinConfigTerm                 {}
    | PinFunctionTerm               {}
    | PinGroupTerm                  {}
    | PinGroupConfigTerm            {}
    | PinGroupFunctionTerm          {}
    | QWordIOTerm                   {}
    | QWordMemoryTerm               {}
    | QWordSpaceTerm                {}
    | RegisterTerm                  {}
    | SpiSerialBusTerm              {}
    | SpiSerialBusTermV2            {}
    | StartDependentFnNoPriTerm     {}
    | StartDependentFnTerm          {}
    | UartSerialBusTerm             {}
    | UartSerialBusTermV2           {}
    | VendorLongTerm                {}
    | VendorShortTerm               {}
    | WordBusNumberTerm             {}
    | WordIOTerm                    {}
    | WordSpaceTerm                 {}
    ;

DMATerm
    : PARSEOP_DMA
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_DMA);}
        DMATypeKeyword
        OptionalBusMasterKeyword
        ',' XferTypeKeyword
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN '{'
            ByteList '}'            {$$ = TrLinkOpChildren ($<n>3,5,$4,$5,$7,$8,$11);}
    | PARSEOP_DMA
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

DWordIOTerm
    : PARSEOP_DWORDIO
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_DWORDIO);}
        OptionalResourceType_First
        OptionalMinType
        OptionalMaxType
        OptionalDecodeType
        OptionalRangeType
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString
        OptionalType
        OptionalTranslationType_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,15,
                                        $4,$5,$6,$7,$8,$10,$12,$14,$16,$18,$19,$20,$21,$22,$23);}
    | PARSEOP_DWORDIO
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

DWordMemoryTerm
    : PARSEOP_DWORDMEMORY
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_DWORDMEMORY);}
        OptionalResourceType_First
        OptionalDecodeType
        OptionalMinType
        OptionalMaxType
        OptionalMemType
        ',' OptionalReadWriteKeyword
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString
        OptionalAddressRange
        OptionalType_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,16,
                                        $4,$5,$6,$7,$8,$10,$12,$14,$16,$18,$20,$21,$22,$23,$24,$25);}
    | PARSEOP_DWORDMEMORY
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

DWordSpaceTerm
    : PARSEOP_DWORDSPACE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_DWORDSPACE);}
        ByteConstExpr               {UtCheckIntegerRange ($4, 0xC0, 0xFF);}
        OptionalResourceType
        OptionalDecodeType
        OptionalMinType
        OptionalMaxType
        ',' ByteConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,14,
                                        $4,$6,$7,$8,$9,$11,$13,$15,$17,$19,$21,$22,$23,$24);}
    | PARSEOP_DWORDSPACE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

EndDependentFnTerm
    : PARSEOP_ENDDEPENDENTFN
        PARSEOP_OPEN_PAREN
        PARSEOP_CLOSE_PAREN         {$$ = TrCreateLeafOp (PARSEOP_ENDDEPENDENTFN);}
    | PARSEOP_ENDDEPENDENTFN
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ExtendedIOTerm
    : PARSEOP_EXTENDEDIO
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_EXTENDEDIO);}
        OptionalResourceType_First
        OptionalMinType
        OptionalMaxType
        OptionalDecodeType
        OptionalRangeType
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        OptionalQWordConstExpr
        OptionalNameString
        OptionalType
        OptionalTranslationType_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,14,
                                        $4,$5,$6,$7,$8,$10,$12,$14,$16,$18,$19,$20,$21,$22);}
    | PARSEOP_EXTENDEDIO
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ExtendedMemoryTerm
    : PARSEOP_EXTENDEDMEMORY
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_EXTENDEDMEMORY);}
        OptionalResourceType_First
        OptionalDecodeType
        OptionalMinType
        OptionalMaxType
        OptionalMemType
        ',' OptionalReadWriteKeyword
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        OptionalQWordConstExpr
        OptionalNameString
        OptionalAddressRange
        OptionalType_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,15,
                                        $4,$5,$6,$7,$8,$10,$12,$14,$16,$18,$20,$21,$22,$23,$24);}
    | PARSEOP_EXTENDEDMEMORY
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

ExtendedSpaceTerm
    : PARSEOP_EXTENDEDSPACE PARSEOP_OPEN_PAREN     {$<n>$ = TrCreateLeafOp (PARSEOP_EXTENDEDSPACE);}
        ByteConstExpr               {UtCheckIntegerRange ($4, 0xC0, 0xFF);}
        OptionalResourceType
        OptionalDecodeType
        OptionalMinType
        OptionalMaxType
        ',' ByteConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        OptionalQWordConstExpr
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN                         {$$ = TrLinkOpChildren ($<n>3,13,
                                        $4,$6,$7,$8,$9,$11,$13,$15,$17,$19,$21,$22,$23);}
    | PARSEOP_EXTENDEDSPACE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN                   {$$ = AslDoError(); yyclearin;}
    ;

FixedDmaTerm
    : PARSEOP_FIXEDDMA
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_FIXEDDMA);}
        WordConstExpr               /* 04: DMA RequestLines */
        ',' WordConstExpr           /* 06: DMA Channels */
        OptionalXferSize            /* 07: DMA TransferSize */
        OptionalNameString          /* 08: DescriptorName */
        PARSEOP_CLOSE_PAREN                         {$$ = TrLinkOpChildren ($<n>3,4,$4,$6,$7,$8);}
    | PARSEOP_FIXEDDMA
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN                   {$$ = AslDoError(); yyclearin;}
    ;

FixedIOTerm
    : PARSEOP_FIXEDIO
        PARSEOP_OPEN_PAREN           {$<n>$ = TrCreateLeafOp (PARSEOP_FIXEDIO);}
        WordConstExpr
        ',' ByteConstExpr
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN                         {$$ = TrLinkOpChildren ($<n>3,3,$4,$6,$7);}
    | PARSEOP_FIXEDIO
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN                   {$$ = AslDoError(); yyclearin;}
    ;

GpioIntTerm
    : PARSEOP_GPIO_INT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_GPIO_INT);}
        InterruptTypeKeyword        /* 04: InterruptType */
        ',' InterruptLevel          /* 06: InterruptLevel */
        OptionalShareType           /* 07: SharedType */
        ',' PinConfigByte           /* 09: PinConfig */
        OptionalWordConstExpr       /* 10: DebounceTimeout */
        ',' StringData              /* 12: ResourceSource */
        OptionalByteConstExpr       /* 13: ResourceSourceIndex */
        OptionalResourceType        /* 14: ResourceType */
        OptionalNameString          /* 15: DescriptorName */
        OptionalBuffer_Last         /* 16: VendorData */
        PARSEOP_CLOSE_PAREN '{'
            DWordConstExpr '}'      {$$ = TrLinkOpChildren ($<n>3,11,
                                        $4,$6,$7,$9,$10,$12,$13,$14,$15,$16,$19);}
    | PARSEOP_GPIO_INT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

GpioIoTerm
    : PARSEOP_GPIO_IO
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_GPIO_IO);}
        OptionalShareType_First     /* 04: SharedType */
        ',' PinConfigByte           /* 06: PinConfig */
        OptionalWordConstExpr       /* 07: DebounceTimeout */
        OptionalWordConstExpr       /* 08: DriveStrength */
        OptionalIoRestriction       /* 09: IoRestriction */
        ',' StringData              /* 11: ResourceSource */
        OptionalByteConstExpr       /* 12: ResourceSourceIndex */
        OptionalResourceType        /* 13: ResourceType */
        OptionalNameString          /* 14: DescriptorName */
        OptionalBuffer_Last         /* 15: VendorData */
        PARSEOP_CLOSE_PAREN '{'
            DWordList '}'           {$$ = TrLinkOpChildren ($<n>3,11,
                                        $4,$6,$7,$8,$9,$11,$12,$13,$14,$15,$18);}
    | PARSEOP_GPIO_IO
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN                   {$$ = AslDoError(); yyclearin;}
    ;

I2cSerialBusTerm
    : PARSEOP_I2C_SERIALBUS
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_I2C_SERIALBUS);}
        WordConstExpr               /* 04: SlaveAddress */
        OptionalSlaveMode           /* 05: SlaveMode */
        ',' DWordConstExpr          /* 07: ConnectionSpeed */
        OptionalAddressingMode      /* 08: AddressingMode */
        ',' StringData              /* 10: ResourceSource */
        OptionalByteConstExpr       /* 11: ResourceSourceIndex */
        OptionalResourceType        /* 12: ResourceType */
        OptionalNameString          /* 13: DescriptorName */
        OptionalBuffer_Last         /* 14: VendorData */
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,10,
                                        $4,$5,$7,$8,$10,$11,$12,$13,
                                        TrCreateLeafOp (PARSEOP_DEFAULT_ARG),$14);}
    | PARSEOP_I2C_SERIALBUS
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

I2cSerialBusTermV2
    : PARSEOP_I2C_SERIALBUS_V2
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_I2C_SERIALBUS_V2);}
        WordConstExpr               /* 04: SlaveAddress */
        OptionalSlaveMode           /* 05: SlaveMode */
        ',' DWordConstExpr          /* 07: ConnectionSpeed */
        OptionalAddressingMode      /* 08: AddressingMode */
        ',' StringData              /* 10: ResourceSource */
        OptionalByteConstExpr       /* 11: ResourceSourceIndex */
        OptionalResourceType        /* 12: ResourceType */
        OptionalNameString          /* 13: DescriptorName */
        OptionalShareType           /* 14: Share */
        OptionalBuffer_Last         /* 15: VendorData */
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,10,
                                        $4,$5,$7,$8,$10,$11,$12,$13,$14,$15);}
    | PARSEOP_I2C_SERIALBUS_V2
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

InterruptTerm
    : PARSEOP_INTERRUPT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_INTERRUPT);}
        OptionalResourceType_First
        ',' InterruptTypeKeyword
        ',' InterruptLevel
        OptionalShareType
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN '{'
            DWordList '}'           {$$ = TrLinkOpChildren ($<n>3,8,
                                        $4,$6,$8,$9,$10,$11,$12,$15);}
    | PARSEOP_INTERRUPT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

IOTerm
    : PARSEOP_IO
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_IO);}
        IODecodeKeyword
        ',' WordConstExpr
        ',' WordConstExpr
        ',' ByteConstExpr
        ',' ByteConstExpr
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,6,$4,$6,$8,$10,$12,$13);}
    | PARSEOP_IO
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

IRQNoFlagsTerm
    : PARSEOP_IRQNOFLAGS
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_IRQNOFLAGS);}
        OptionalNameString_First
        PARSEOP_CLOSE_PAREN '{'
            ByteList '}'            {$$ = TrLinkOpChildren ($<n>3,2,$4,$7);}
    | PARSEOP_IRQNOFLAGS
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

IRQTerm
    : PARSEOP_IRQ
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_IRQ);}
        InterruptTypeKeyword
        ',' InterruptLevel
        OptionalShareType
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN '{'
            ByteList '}'            {$$ = TrLinkOpChildren ($<n>3,5,$4,$6,$7,$8,$11);}
    | PARSEOP_IRQ
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

Memory24Term
    : PARSEOP_MEMORY24
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_MEMORY24);}
        OptionalReadWriteKeyword
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,6,$4,$6,$8,$10,$12,$13);}
    | PARSEOP_MEMORY24
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

Memory32FixedTerm
    : PARSEOP_MEMORY32FIXED
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_MEMORY32FIXED);}
        OptionalReadWriteKeyword
        ',' DWordConstExpr
        ',' DWordConstExpr
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,4,$4,$6,$8,$9);}
    | PARSEOP_MEMORY32FIXED
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

Memory32Term
    : PARSEOP_MEMORY32
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_MEMORY32);}
        OptionalReadWriteKeyword
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        ',' DWordConstExpr
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,6,$4,$6,$8,$10,$12,$13);}
    | PARSEOP_MEMORY32
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

PinConfigTerm
    : PARSEOP_PINCONFIG
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_PINCONFIG);}
        OptionalShareType_First     /* 04: SharedType */
        ',' ByteConstExpr           /* 06: PinConfigType */
        ',' DWordConstExpr          /* 08: PinConfigValue */
        ',' StringData              /* 10: ResourceSource */
        OptionalByteConstExpr       /* 11: ResourceSourceIndex */
        OptionalResourceType        /* 12: ResourceType */
        OptionalNameString          /* 13: DescriptorName */
        OptionalBuffer_Last         /* 14: VendorData */
        PARSEOP_CLOSE_PAREN '{'
            DWordList '}'           {$$ = TrLinkOpChildren ($<n>3,9,
                                        $4,$6,$8,$10,$11,$12,$13,$14,$17);}
    | PARSEOP_PINCONFIG
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

PinFunctionTerm
    : PARSEOP_PINFUNCTION
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_PINFUNCTION);}
        OptionalShareType_First     /* 04: SharedType */
        ',' PinConfigByte           /* 06: PinConfig */
        ',' WordConstExpr           /* 08: FunctionNumber */
        ',' StringData              /* 10: ResourceSource */
        OptionalByteConstExpr       /* 11: ResourceSourceIndex */
        OptionalResourceType        /* 12: ResourceType */
        OptionalNameString          /* 13: DescriptorName */
        OptionalBuffer_Last         /* 14: VendorData */
        PARSEOP_CLOSE_PAREN '{'
            DWordList '}'           {$$ = TrLinkOpChildren ($<n>3,9,
                                        $4,$6,$8,$10,$11,$12,$13,$14,$17);}
    | PARSEOP_PINFUNCTION
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

PinGroupTerm
    : PARSEOP_PINGROUP
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_PINGROUP);}
        StringData                  /* 04: ResourceLabel */
        OptionalProducerResourceType /* 05: ResourceType */
        OptionalNameString          /* 06: DescriptorName */
        OptionalBuffer_Last         /* 07: VendorData */
        PARSEOP_CLOSE_PAREN '{'
            DWordList '}'           {$$ = TrLinkOpChildren ($<n>3,5,$4,$5,$6,$7,$10);}
    | PARSEOP_PINGROUP
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

PinGroupConfigTerm
    : PARSEOP_PINGROUPCONFIG
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_PINGROUPCONFIG);}
        OptionalShareType_First     /* 04: SharedType */
        ',' ByteConstExpr           /* 06: PinConfigType */
        ',' DWordConstExpr          /* 08: PinConfigValue */
        ',' StringData              /* 10: ResourceSource */
        OptionalByteConstExpr       /* 11: ResourceSourceIndex */
        ',' StringData              /* 13: ResourceSourceLabel */
        OptionalResourceType        /* 14: ResourceType */
        OptionalNameString          /* 15: DescriptorName */
        OptionalBuffer_Last         /* 16: VendorData */
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,9,
                                        $4,$6,$8,$10,$11,$13,$14,$15,$16);}
    | PARSEOP_PINGROUPCONFIG
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

PinGroupFunctionTerm
    : PARSEOP_PINGROUPFUNCTION
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_PINGROUPFUNCTION);}
        OptionalShareType_First     /* 04: SharedType */
        ',' WordConstExpr           /* 06: FunctionNumber */
        ',' StringData              /* 08: ResourceSource */
        OptionalByteConstExpr       /* 09: ResourceSourceIndex */
        ',' StringData              /* 11: ResourceSourceLabel */
        OptionalResourceType        /* 12: ResourceType */
        OptionalNameString          /* 13: DescriptorName */
        OptionalBuffer_Last         /* 14: VendorData */
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,8,
                                        $4,$6,$8,$9,$11,$12,$13,$14);}
    | PARSEOP_PINGROUPFUNCTION
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

QWordIOTerm
    : PARSEOP_QWORDIO
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_QWORDIO);}
        OptionalResourceType_First
        OptionalMinType
        OptionalMaxType
        OptionalDecodeType
        OptionalRangeType
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString
        OptionalType
        OptionalTranslationType_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,15,
                                        $4,$5,$6,$7,$8,$10,$12,$14,$16,$18,$19,$20,$21,$22,$23);}
    | PARSEOP_QWORDIO
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

QWordMemoryTerm
    : PARSEOP_QWORDMEMORY
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_QWORDMEMORY);}
        OptionalResourceType_First
        OptionalDecodeType
        OptionalMinType
        OptionalMaxType
        OptionalMemType
        ',' OptionalReadWriteKeyword
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString
        OptionalAddressRange
        OptionalType_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,16,
                                        $4,$5,$6,$7,$8,$10,$12,$14,$16,$18,$20,$21,$22,$23,$24,$25);}
    | PARSEOP_QWORDMEMORY
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

QWordSpaceTerm
    : PARSEOP_QWORDSPACE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_QWORDSPACE);}
        ByteConstExpr               {UtCheckIntegerRange ($4, 0xC0, 0xFF);}
        OptionalResourceType
        OptionalDecodeType
        OptionalMinType
        OptionalMaxType
        ',' ByteConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        ',' QWordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,14,
                                        $4,$6,$7,$8,$9,$11,$13,$15,$17,$19,$21,$22,$23,$24);}
    | PARSEOP_QWORDSPACE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

RegisterTerm
    : PARSEOP_REGISTER
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_REGISTER);}
        AddressSpaceKeyword
        ',' ByteConstExpr
        ',' ByteConstExpr
        ',' QWordConstExpr
        OptionalAccessSize
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,6,$4,$6,$8,$10,$11,$12);}
    | PARSEOP_REGISTER
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

SpiSerialBusTerm
    : PARSEOP_SPI_SERIALBUS
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_SPI_SERIALBUS);}
        WordConstExpr               /* 04: DeviceSelection */
        OptionalDevicePolarity      /* 05: DevicePolarity */
        OptionalWireMode            /* 06: WireMode */
        ',' ByteConstExpr           /* 08: DataBitLength */
        OptionalSlaveMode           /* 09: SlaveMode */
        ',' DWordConstExpr          /* 11: ConnectionSpeed */
        ',' ClockPolarityKeyword    /* 13: ClockPolarity */
        ',' ClockPhaseKeyword       /* 15: ClockPhase */
        ',' StringData              /* 17: ResourceSource */
        OptionalByteConstExpr       /* 18: ResourceSourceIndex */
        OptionalResourceType        /* 19: ResourceType */
        OptionalNameString          /* 20: DescriptorName */
        OptionalBuffer_Last         /* 21: VendorData */
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,14,
                                        $4,$5,$6,$8,$9,$11,$13,$15,$17,$18,$19,$20,
                                        TrCreateLeafOp (PARSEOP_DEFAULT_ARG),$21);}
    | PARSEOP_SPI_SERIALBUS
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

SpiSerialBusTermV2
    : PARSEOP_SPI_SERIALBUS_V2
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_SPI_SERIALBUS_V2);}
        WordConstExpr               /* 04: DeviceSelection */
        OptionalDevicePolarity      /* 05: DevicePolarity */
        OptionalWireMode            /* 06: WireMode */
        ',' ByteConstExpr           /* 08: DataBitLength */
        OptionalSlaveMode           /* 09: SlaveMode */
        ',' DWordConstExpr          /* 11: ConnectionSpeed */
        ',' ClockPolarityKeyword    /* 13: ClockPolarity */
        ',' ClockPhaseKeyword       /* 15: ClockPhase */
        ',' StringData              /* 17: ResourceSource */
        OptionalByteConstExpr       /* 18: ResourceSourceIndex */
        OptionalResourceType        /* 19: ResourceType */
        OptionalNameString          /* 20: DescriptorName */
        OptionalShareType           /* 21: Share */
        OptionalBuffer_Last         /* 22: VendorData */
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,14,
                                        $4,$5,$6,$8,$9,$11,$13,$15,$17,$18,$19,$20,$21,$22);}
    | PARSEOP_SPI_SERIALBUS_V2
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

StartDependentFnNoPriTerm
    : PARSEOP_STARTDEPENDENTFN_NOPRI
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_STARTDEPENDENTFN_NOPRI);}
        PARSEOP_CLOSE_PAREN '{'
        ResourceMacroList '}'       {$$ = TrLinkOpChildren ($<n>3,1,$6);}
    | PARSEOP_STARTDEPENDENTFN_NOPRI
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

StartDependentFnTerm
    : PARSEOP_STARTDEPENDENTFN
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_STARTDEPENDENTFN);}
        ByteConstExpr
        ',' ByteConstExpr
        PARSEOP_CLOSE_PAREN '{'
        ResourceMacroList '}'       {$$ = TrLinkOpChildren ($<n>3,3,$4,$6,$9);}
    | PARSEOP_STARTDEPENDENTFN
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

UartSerialBusTerm
    : PARSEOP_UART_SERIALBUS
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_UART_SERIALBUS);}
        DWordConstExpr              /* 04: ConnectionSpeed */
        OptionalBitsPerByte         /* 05: BitsPerByte */
        OptionalStopBits            /* 06: StopBits */
        ',' ByteConstExpr           /* 08: LinesInUse */
        OptionalEndian              /* 09: Endianness */
        OptionalParityType          /* 10: Parity */
        OptionalFlowControl         /* 11: FlowControl */
        ',' WordConstExpr           /* 13: Rx BufferSize */
        ',' WordConstExpr           /* 15: Tx BufferSize */
        ',' StringData              /* 17: ResourceSource */
        OptionalByteConstExpr       /* 18: ResourceSourceIndex */
        OptionalResourceType        /* 19: ResourceType */
        OptionalNameString          /* 20: DescriptorName */
        OptionalBuffer_Last         /* 21: VendorData */
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,15,
                                        $4,$5,$6,$8,$9,$10,$11,$13,$15,$17,$18,$19,$20,
                                        TrCreateLeafOp (PARSEOP_DEFAULT_ARG),$21);}
    | PARSEOP_UART_SERIALBUS
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

UartSerialBusTermV2
    : PARSEOP_UART_SERIALBUS_V2
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_UART_SERIALBUS_V2);}
        DWordConstExpr              /* 04: ConnectionSpeed */
        OptionalBitsPerByte         /* 05: BitsPerByte */
        OptionalStopBits            /* 06: StopBits */
        ',' ByteConstExpr           /* 08: LinesInUse */
        OptionalEndian              /* 09: Endianness */
        OptionalParityType          /* 10: Parity */
        OptionalFlowControl         /* 11: FlowControl */
        ',' WordConstExpr           /* 13: Rx BufferSize */
        ',' WordConstExpr           /* 15: Tx BufferSize */
        ',' StringData              /* 17: ResourceSource */
        OptionalByteConstExpr       /* 18: ResourceSourceIndex */
        OptionalResourceType        /* 19: ResourceType */
        OptionalNameString          /* 20: DescriptorName */
        OptionalShareType           /* 21: Share */
        OptionalBuffer_Last         /* 22: VendorData */
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,15,
                                        $4,$5,$6,$8,$9,$10,$11,$13,$15,$17,$18,$19,$20,$21,$22);}
    | PARSEOP_UART_SERIALBUS_V2
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

VendorLongTerm
    : PARSEOP_VENDORLONG
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_VENDORLONG);}
        OptionalNameString_First
        PARSEOP_CLOSE_PAREN '{'
            ByteList '}'            {$$ = TrLinkOpChildren ($<n>3,2,$4,$7);}
    | PARSEOP_VENDORLONG
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

VendorShortTerm
    : PARSEOP_VENDORSHORT
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_VENDORSHORT);}
        OptionalNameString_First
        PARSEOP_CLOSE_PAREN '{'
            ByteList '}'            {$$ = TrLinkOpChildren ($<n>3,2,$4,$7);}
    | PARSEOP_VENDORSHORT
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

WordBusNumberTerm
    : PARSEOP_WORDBUSNUMBER
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_WORDBUSNUMBER);}
        OptionalResourceType_First
        OptionalMinType
        OptionalMaxType
        OptionalDecodeType
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,12,
                                        $4,$5,$6,$7,$9,$11,$13,$15,$17,$18,$19,$20);}
    | PARSEOP_WORDBUSNUMBER
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

WordIOTerm
    : PARSEOP_WORDIO
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_WORDIO);}
        OptionalResourceType_First
        OptionalMinType
        OptionalMaxType
        OptionalDecodeType
        OptionalRangeType
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString
        OptionalType
        OptionalTranslationType_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,15,
                                        $4,$5,$6,$7,$8,$10,$12,$14,$16,$18,$19,$20,$21,$22,$23);}
    | PARSEOP_WORDIO
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

WordSpaceTerm
    : PARSEOP_WORDSPACE
        PARSEOP_OPEN_PAREN          {$<n>$ = TrCreateLeafOp (PARSEOP_WORDSPACE);}
        ByteConstExpr               {UtCheckIntegerRange ($4, 0xC0, 0xFF);}
        OptionalResourceType
        OptionalDecodeType
        OptionalMinType
        OptionalMaxType
        ',' ByteConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        ',' WordConstExpr
        OptionalByteConstExpr
        OptionalStringData
        OptionalNameString_Last
        PARSEOP_CLOSE_PAREN         {$$ = TrLinkOpChildren ($<n>3,14,
                                        $4,$6,$7,$8,$9,$11,$13,$15,$17,$19,$21,$22,$23,$24);}
    | PARSEOP_WORDSPACE
        PARSEOP_OPEN_PAREN
        error PARSEOP_CLOSE_PAREN   {$$ = AslDoError(); yyclearin;}
    ;

/******************************************************************************
 *
 * Module Name: asltree - Parse tree management
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
#include "acapps.h"

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("asltree")


/*******************************************************************************
 *
 * FUNCTION:    TrSetOpIntegerValue
 *
 * PARAMETERS:  ParseOpcode         - New opcode to be assigned to the op
 *              Op                  - An existing parse op
 *
 * RETURN:      The updated op
 *
 * DESCRIPTION: Used to set the integer value of a op,
 *              usually to a specific size (8, 16, 32, or 64 bits)
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
TrSetOpIntegerValue (
    UINT32                  ParseOpcode,
    ACPI_PARSE_OBJECT       *Op)
{

    if (!Op)
    {
        return (NULL);
    }

    DbgPrint (ASL_PARSE_OUTPUT,
        "\nUpdateOp: Old - %s, New - %s\n",
        UtGetOpName (Op->Asl.ParseOpcode),
        UtGetOpName (ParseOpcode));

    /* Assign new opcode and name */

    if (Op->Asl.ParseOpcode == PARSEOP_ONES)
    {
        switch (ParseOpcode)
        {
        case PARSEOP_BYTECONST:

            Op->Asl.Value.Integer = ACPI_UINT8_MAX;
            break;

        case PARSEOP_WORDCONST:

            Op->Asl.Value.Integer = ACPI_UINT16_MAX;
            break;

        case PARSEOP_DWORDCONST:

            Op->Asl.Value.Integer = ACPI_UINT32_MAX;
            break;

        /* Don't need to do the QWORD case */

        default:

            /* Don't care about others */
            break;
        }
    }

    Op->Asl.ParseOpcode = (UINT16) ParseOpcode;
    UtSetParseOpName (Op);

    /*
     * For the BYTE, WORD, and DWORD constants, make sure that the integer
     * that was passed in will actually fit into the data type
     */
    switch (ParseOpcode)
    {
    case PARSEOP_BYTECONST:

        UtCheckIntegerRange (Op, 0x00, ACPI_UINT8_MAX);
        Op->Asl.Value.Integer &= ACPI_UINT8_MAX;
        break;

    case PARSEOP_WORDCONST:

        UtCheckIntegerRange (Op, 0x00, ACPI_UINT16_MAX);
        Op->Asl.Value.Integer &= ACPI_UINT16_MAX;
        break;

    case PARSEOP_DWORDCONST:

        UtCheckIntegerRange (Op, 0x00, ACPI_UINT32_MAX);
        Op->Asl.Value.Integer &= ACPI_UINT32_MAX;
        break;

    default:

        /* Don't care about others, don't need to check QWORD */

        break;
    }

    /* Converter: if this is a method invocation, turn off capture comments */

    if (AcpiGbl_CaptureComments &&
        (ParseOpcode == PARSEOP_METHODCALL))
    {
        AslGbl_CommentState.CaptureComments = FALSE;
    }

    return (Op);
}


/*******************************************************************************
 *
 * FUNCTION:    TrSetOpFlags
 *
 * PARAMETERS:  Op                  - An existing parse op
 *              Flags               - New flags word
 *
 * RETURN:      The updated parser op
 *
 * DESCRIPTION: Set bits in the op flags word. Will not clear bits, only set
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
TrSetOpFlags (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Flags)
{

    if (!Op)
    {
        return (NULL);
    }

    DbgPrint (ASL_PARSE_OUTPUT,
        "\nSetOpFlags: %s Op %p, %8.8X", Op->Asl.ParseOpName, Op, Flags);

    TrPrintOpFlags (Flags, ASL_PARSE_OUTPUT);
    DbgPrint (ASL_PARSE_OUTPUT, "\n\n");

    Op->Asl.CompileFlags |= Flags;
    return (Op);
}


/*******************************************************************************
 *
 * FUNCTION:    TrSetOpAmlLength
 *
 * PARAMETERS:  Op                  - An existing parse op
 *              Length              - AML Length
 *
 * RETURN:      The updated parser op
 *
 * DESCRIPTION: Set the AML Length in a op. Used by the parser to indicate
 *              the presence of a op that must be reduced to a fixed length
 *              constant.
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
TrSetOpAmlLength (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Length)
{

    DbgPrint (ASL_PARSE_OUTPUT,
        "\nSetOpAmlLength: Op %p, %8.8X\n", Op, Length);

    if (!Op)
    {
        return (NULL);
    }

    Op->Asl.AmlLength = Length;
    return (Op);
}


/*******************************************************************************
 *
 * FUNCTION:    TrSetOpParent
 *
 * PARAMETERS:  Op                  - To be set to new parent
 *              ParentOp            - The parent
 *
 * RETURN:      None, sets Op parent directly
 *
 * DESCRIPTION: Change the parent of a parse op.
 *
 ******************************************************************************/

void
TrSetOpParent (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_PARSE_OBJECT       *ParentOp)
{

    Op->Asl.Parent = ParentOp;
}


/*******************************************************************************
 *
 * FUNCTION:    TrSetOpCurrentFilename
 *
 * PARAMETERS:  Op                  - An existing parse op
 *
 * RETURN:      None
 *
 * DESCRIPTION: Save the include file filename. Used for debug output only.
 *
 ******************************************************************************/

void
TrSetOpCurrentFilename (
    ACPI_PARSE_OBJECT       *Op)
{

    Op->Asl.Filename = AslGbl_PreviousIncludeFilename;
}


/*******************************************************************************
 *
 * FUNCTION:    TrSetOpIntegerWidth
 *
 * PARAMETERS:  Op                  - An existing parse op
 *
 * RETURN:      None
 *
 * DESCRIPTION:
 *
 ******************************************************************************/

void
TrSetOpIntegerWidth (
    ACPI_PARSE_OBJECT       *TableSignatureOp,
    ACPI_PARSE_OBJECT       *RevisionOp)
{

    /* TBD: Check table sig? (DSDT vs. SSDT) */

    /* Handle command-line version override */

    if (AslGbl_RevisionOverride)
    {
        AcpiUtSetIntegerWidth (AslGbl_RevisionOverride);
    }
    else
    {
        AcpiUtSetIntegerWidth ((UINT8) RevisionOp->Asl.Value.Integer);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    TrSetOpEndLineNumber
 *
 * PARAMETERS:  Op                - An existing parse op
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Set the ending line numbers (file line and logical line) of a
 *              parse op to the current line numbers.
 *
 ******************************************************************************/

void
TrSetOpEndLineNumber (
    ACPI_PARSE_OBJECT       *Op)
{

    /* If the end line # is already set, just return */

    if (Op->Asl.EndLine)
    {
        return;
    }

    Op->Asl.EndLine = AslGbl_CurrentLineNumber;
    Op->Asl.EndLogicalLine = AslGbl_LogicalLineNumber;
}


/*******************************************************************************
 *
 * FUNCTION:    TrLinkOpChildren
 *
 * PARAMETERS:  Op                - An existing parse op
 *              NumChildren        - Number of children to follow
 *              ...                - A list of child ops to link to the new
 *                                   op. NumChildren long.
 *
 * RETURN:      The updated (linked) op
 *
 * DESCRIPTION: Link a group of ops to an existing parse op
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
TrLinkOpChildren (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  NumChildren,
    ...)
{
    ACPI_PARSE_OBJECT       *Child;
    ACPI_PARSE_OBJECT       *PrevChild;
    ACPI_PARSE_OBJECT       *LastSibling;
    va_list                 ap;
    UINT32                  i;
    BOOLEAN                 FirstChild;


    va_start (ap, NumChildren);

    TrSetOpEndLineNumber (Op);

    DbgPrint (ASL_PARSE_OUTPUT,
        "\nLinkChildren  Line [%u to %u] NewParent %p Child %u Op %s  ",
        Op->Asl.LineNumber, Op->Asl.EndLine,
        Op, NumChildren, UtGetOpName(Op->Asl.ParseOpcode));

    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_ASL_CODE:

        if (!AslGbl_ParseTreeRoot)
        {
            DbgPrint (ASL_PARSE_OUTPUT, "Creating first Definition Block\n");
            AslGbl_ParseTreeRoot = Op;
            Op->Asl.ParseOpcode = PARSEOP_DEFAULT_ARG;
        }
        else
        {
            DbgPrint (ASL_PARSE_OUTPUT, "Creating subsequent Definition Block\n");
            Op = AslGbl_ParseTreeRoot;
        }

        DbgPrint (ASL_PARSE_OUTPUT, "ASLCODE (Tree Completed)->");
        break;

    case PARSEOP_DEFINITION_BLOCK:

        DbgPrint (ASL_PARSE_OUTPUT, "DEFINITION_BLOCK (Tree Completed)->");
        break;

    case PARSEOP_OPERATIONREGION:

        DbgPrint (ASL_PARSE_OUTPUT, "OPREGION->");
        break;

    case PARSEOP_OR:

        DbgPrint (ASL_PARSE_OUTPUT, "OR->");
        break;

    default:

        /* Nothing to do for other opcodes */

        break;
    }

    /* The following is for capturing comments */

    if (AcpiGbl_CaptureComments)
    {
        /*
         * If there are "regular comments" detected at this point,
         * then is an endBlk comment. Categorize it as so and distribute
         * all regular comments to this parse op.
         */
        if (AslGbl_CommentListHead)
        {
            Op->Asl.EndBlkComment = AslGbl_CommentListHead;
            CvDbgPrint ("EndBlk Comment for %s: %s",
                Op->Asl.ParseOpName, AslGbl_CommentListHead->Comment);
            AslGbl_CommentListHead = NULL;
            AslGbl_CommentListTail = NULL;
        }
    }

    /* Link the new op to it's children */

    PrevChild = NULL;
    FirstChild = TRUE;
    for (i = 0; i < NumChildren; i++)
    {
        Child = va_arg (ap, ACPI_PARSE_OBJECT *);

        if ((Child == PrevChild) && (Child != NULL))
        {
            AslError (ASL_WARNING, ASL_MSG_COMPILER_INTERNAL, Child,
                "Child op list invalid");
            va_end(ap);
            return (Op);
        }

        DbgPrint (ASL_PARSE_OUTPUT, "%p, ", Child);

        /*
         * If child is NULL, this means that an optional argument
         * was omitted. We must create a placeholder with a special
         * opcode (DEFAULT_ARG) so that the code generator will know
         * that it must emit the correct default for this argument
         */
        if (!Child)
        {
            Child = TrAllocateOp (PARSEOP_DEFAULT_ARG);
        }

        /* Link first child to parent */

        if (FirstChild)
        {
            FirstChild = FALSE;

            /*
             * In the case that multiple definition blocks are being compiled,
             * append the definition block to the end of the child list as the
             * last sibling. This is done to facilitate namespace cross-
             * reference between multiple definition blocks.
             */
            if (Op->Asl.Child &&
                (Op->Asl.Child->Asl.ParseOpcode == PARSEOP_DEFINITION_BLOCK))
            {
                LastSibling = Op->Asl.Child;
                while (LastSibling->Asl.Next)
                {
                    LastSibling = LastSibling->Asl.Next;
                }
                LastSibling->Asl.Next = Child;
            }
            else
            {
                Op->Asl.Child = Child;
            }
        }

        /* Point all children to parent */

        Child->Asl.Parent = Op;

        /* Link children in a peer list */

        if (PrevChild)
        {
            PrevChild->Asl.Next = Child;
        }

        /*
         * This child might be a list, point all ops in the list
         * to the same parent
         */
        while (Child->Asl.Next)
        {
            Child = Child->Asl.Next;
            Child->Asl.Parent = Op;
        }

        PrevChild = Child;
    }

    va_end(ap);
    DbgPrint (ASL_PARSE_OUTPUT, "\n\n");

    if (AcpiGbl_CaptureComments)
    {
        AslGbl_CommentState.LatestParseOp = Op;
        CvDbgPrint ("TrLinkOpChildren=====Set latest parse op to this op.\n");
    }

    return (Op);
}


/*******************************************************************************
 *
 * FUNCTION:    TrLinkPeerOp
 *
 * PARAMETERS:  Op1           - First peer
 *              Op2           - Second peer
 *
 * RETURN:      Op1 or the non-null op.
 *
 * DESCRIPTION: Link two ops as peers. Handles cases where one peer is null.
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
TrLinkPeerOp (
    ACPI_PARSE_OBJECT       *Op1,
    ACPI_PARSE_OBJECT       *Op2)
{
    ACPI_PARSE_OBJECT       *Next;


    DbgPrint (ASL_PARSE_OUTPUT,
        "\nLinkPeerOp: 1=%p (%s), 2=%p (%s)\n",
        Op1, Op1 ? UtGetOpName(Op1->Asl.ParseOpcode) : NULL,
        Op2, Op2 ? UtGetOpName(Op2->Asl.ParseOpcode) : NULL);


    if ((!Op1) && (!Op2))
    {
        DbgPrint (ASL_PARSE_OUTPUT, "\nTwo Null ops!\n");
        return (Op1);
    }

    /* If one of the ops is null, just return the non-null op */

    if (!Op2)
    {
        return (Op1);
    }

    if (!Op1)
    {
        return (Op2);
    }

    if (Op1 == Op2)
    {
        DbgPrint (ASL_DEBUG_OUTPUT,
            "\n************* Internal error, linking op to itself %p\n",
            Op1);
        AslError (ASL_WARNING, ASL_MSG_COMPILER_INTERNAL, Op1,
            "Linking op to itself");
        return (Op1);
    }

    Op1->Asl.Parent = Op2->Asl.Parent;

    /*
     * Op 1 may already have a peer list (such as an IF/ELSE pair),
     * so we must walk to the end of the list and attach the new
     * peer at the end
     */
    Next = Op1;
    while (Next->Asl.Next)
    {
        Next = Next->Asl.Next;
    }

    Next->Asl.Next = Op2;
    return (Op1);
}


/*******************************************************************************
 *
 * FUNCTION:    TrLinkPeerOps
 *
 * PARAMETERS:  NumPeers            - The number of ops in the list to follow
 *              ...                 - A list of ops to link together as peers
 *
 * RETURN:      The first op in the list (head of the peer list)
 *
 * DESCRIPTION: Link together an arbitrary number of peer ops.
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
TrLinkPeerOps (
    UINT32                  NumPeers,
    ...)
{
    ACPI_PARSE_OBJECT       *This;
    ACPI_PARSE_OBJECT       *Next;
    va_list                 ap;
    UINT32                  i;
    ACPI_PARSE_OBJECT       *Start;


    DbgPrint (ASL_PARSE_OUTPUT,
        "\nLinkPeerOps: (%u) ", NumPeers);

    va_start (ap, NumPeers);
    This = va_arg (ap, ACPI_PARSE_OBJECT *);
    Start = This;

    /*
     * Link all peers
     */
    for (i = 0; i < (NumPeers -1); i++)
    {
        DbgPrint (ASL_PARSE_OUTPUT, "%u=%p ", (i+1), This);

        while (This->Asl.Next)
        {
            This = This->Asl.Next;
        }

        /* Get another peer op */

        Next = va_arg (ap, ACPI_PARSE_OBJECT *);
        if (!Next)
        {
            Next = TrAllocateOp (PARSEOP_DEFAULT_ARG);
        }

        /* link new op to the current op */

        This->Asl.Next = Next;
        This = Next;
    }

    va_end (ap);
    DbgPrint (ASL_PARSE_OUTPUT,"\n");
    return (Start);
}


/*******************************************************************************
 *
 * FUNCTION:    TrLinkChildOp
 *
 * PARAMETERS:  Op1           - Parent op
 *              Op2           - Op to become a child
 *
 * RETURN:      The parent op
 *
 * DESCRIPTION: Link two ops together as a parent and child
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
TrLinkChildOp (
    ACPI_PARSE_OBJECT       *Op1,
    ACPI_PARSE_OBJECT       *Op2)
{
    ACPI_PARSE_OBJECT       *Next;


    DbgPrint (ASL_PARSE_OUTPUT,
        "\nLinkChildOp: Parent=%p (%s), Child=%p (%s)\n",
        Op1, Op1 ? UtGetOpName(Op1->Asl.ParseOpcode): NULL,
        Op2, Op2 ? UtGetOpName(Op2->Asl.ParseOpcode): NULL);

    /*
     * Converter: if TrLinkChildOp is called to link a method call,
     * turn on capture comments as it signifies that we are done parsing
     * a method call.
     */
    if (AcpiGbl_CaptureComments && Op1)
    {
        if (Op1->Asl.ParseOpcode == PARSEOP_METHODCALL)
        {
            AslGbl_CommentState.CaptureComments = TRUE;
        }
        AslGbl_CommentState.LatestParseOp = Op1;
    }

    if (!Op1 || !Op2)
    {
        return (Op1);
    }

    Op1->Asl.Child = Op2;

    /* Set the child and all peers of the child to point to the parent */

    Next = Op2;
    while (Next)
    {
        Next->Asl.Parent = Op1;
        Next = Next->Asl.Next;
    }

    return (Op1);
}


/*******************************************************************************
 *
 * FUNCTION:    TrWalkParseTree
 *
 * PARAMETERS:  Op                      - Walk starting point
 *              Visitation              - Type of walk
 *              DescendingCallback      - Called during tree descent
 *              AscendingCallback       - Called during tree ascent
 *              Context                 - To be passed to the callbacks
 *
 * RETURN:      Status from callback(s)
 *
 * DESCRIPTION: Walk the entire parse tree.
 *
 ******************************************************************************/

ACPI_STATUS
TrWalkParseTree (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Visitation,
    ASL_WALK_CALLBACK       DescendingCallback,
    ASL_WALK_CALLBACK       AscendingCallback,
    void                    *Context)
{
    UINT32                  Level;
    BOOLEAN                 OpPreviouslyVisited;
    ACPI_PARSE_OBJECT       *StartOp = Op;
    ACPI_STATUS             Status;
    ACPI_PARSE_OBJECT       *Restore = NULL;
    BOOLEAN                 WalkOneDefinitionBlock = Visitation & ASL_WALK_VISIT_DB_SEPARATELY;


    if (!AslGbl_ParseTreeRoot)
    {
        return (AE_OK);
    }

    Level = 0;
    OpPreviouslyVisited = FALSE;

    if (Op->Asl.ParseOpcode == PARSEOP_DEFINITION_BLOCK &&
        WalkOneDefinitionBlock)
    {
        Restore = Op->Asl.Next;
        Op->Asl.Next = NULL;
    }
    switch (Visitation & ~ASL_WALK_VISIT_DB_SEPARATELY)
    {
    case ASL_WALK_VISIT_DOWNWARD:

        while (Op)
        {
            if (!OpPreviouslyVisited)
            {
                /* Let the callback process the op. */

                Status = DescendingCallback (Op, Level, Context);
                if (ACPI_SUCCESS (Status))
                {
                    /* Visit children first, once */

                    if (Op->Asl.Child)
                    {
                        Level++;
                        Op = Op->Asl.Child;
                        continue;
                    }
                }
                else if (Status != AE_CTRL_DEPTH)
                {
                    /* Exit immediately on any error */

                    goto ErrorExit;
                }
            }

            /* Terminate walk at start op */

            if (Op == StartOp)
            {
                break;
            }

            /* No more children, visit peers */

            if (Op->Asl.Next)
            {
                Op = Op->Asl.Next;
                OpPreviouslyVisited = FALSE;
            }
            else
            {
                /* No children or peers, re-visit parent */

                if (Level != 0 )
                {
                    Level--;
                }
                Op = Op->Asl.Parent;
                OpPreviouslyVisited = TRUE;
            }
        }
        break;

    case ASL_WALK_VISIT_UPWARD:

        while (Op)
        {
            /* Visit leaf op (no children) or parent op on return trip */

            if ((!Op->Asl.Child) ||
                (OpPreviouslyVisited))
            {
                /* Let the callback process the op. */

                Status = AscendingCallback (Op, Level, Context);
                if (ACPI_FAILURE (Status))
                {
                    goto ErrorExit;
                }
            }
            else
            {
                /* Visit children first, once */

                Level++;
                Op = Op->Asl.Child;
                continue;
            }

            /* Terminate walk at start op */

            if (Op == StartOp)
            {
                break;
            }

            /* No more children, visit peers */

            if (Op->Asl.Next)
            {
                Op = Op->Asl.Next;
                OpPreviouslyVisited = FALSE;
            }
            else
            {
                /* No children or peers, re-visit parent */

                if (Level != 0 )
                {
                    Level--;
                }
                Op = Op->Asl.Parent;
                OpPreviouslyVisited = TRUE;
            }
        }
        break;

     case ASL_WALK_VISIT_TWICE:

        while (Op)
        {
            if (OpPreviouslyVisited)
            {
                Status = AscendingCallback (Op, Level, Context);
                if (ACPI_FAILURE (Status))
                {
                    goto ErrorExit;
                }
            }
            else
            {
                /* Let the callback process the op. */

                Status = DescendingCallback (Op, Level, Context);
                if (ACPI_SUCCESS (Status))
                {
                    /* Visit children first, once */

                    if (Op->Asl.Child)
                    {
                        Level++;
                        Op = Op->Asl.Child;
                        continue;
                    }
                }
                else if (Status != AE_CTRL_DEPTH)
                {
                    /* Exit immediately on any error */

                    goto ErrorExit;
                }
            }

            /* Terminate walk at start op */

            if (Op == StartOp)
            {
                break;
            }

            /* No more children, visit peers */

            if (Op->Asl.Next)
            {
                Op = Op->Asl.Next;
                OpPreviouslyVisited = FALSE;
            }
            else
            {
                /* No children or peers, re-visit parent */

                if (Level != 0 )
                {
                    Level--;
                }
                Op = Op->Asl.Parent;
                OpPreviouslyVisited = TRUE;
            }
        }
        break;

    default:
        /* No other types supported */
        break;
    }

    /* If we get here, the walk completed with no errors */

    if (Op->Asl.ParseOpcode == PARSEOP_DEFINITION_BLOCK &&
        WalkOneDefinitionBlock)
    {
        Op->Asl.Next = Restore;
    }

    return (AE_OK);

ErrorExit:

    if (Op->Asl.ParseOpcode == PARSEOP_DEFINITION_BLOCK &&
        WalkOneDefinitionBlock)
    {
        Op->Asl.Next = Restore;
    }
    return (Status);
}

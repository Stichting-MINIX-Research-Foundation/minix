/******************************************************************************
 *
 * Module Name: adwalk - Disassembler routines for switch statements
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

#include "acpi.h"
#include "accommon.h"
#include "acparser.h"
#include "amlcode.h"
#include "acdisasm.h"
#include "acdispat.h"
#include "acnamesp.h"
#include "acapps.h"


#define _COMPONENT          ACPI_CA_DISASSEMBLER
        ACPI_MODULE_NAME    ("dmswitch")

static BOOLEAN
AcpiDmIsSwitchBlock (
    ACPI_PARSE_OBJECT       *Op,
    char                    **Temp);

static BOOLEAN
AcpiDmIsCaseBlock (
    ACPI_PARSE_OBJECT       *Op);


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmProcessSwitch
 *
 * PARAMETERS:  Op              - Object to be examined
 *
 * RETURN:      ACPI_STATUS
 *
 * DESCRIPTION: Walk function to create a list of all temporary (_T_) objects.
 *              If a While loop is found that can be converted to a Switch, do
 *              the conversion, remove the temporary name from the list, and
 *              mark the parse op with an IGNORE flag.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDmProcessSwitch (
    ACPI_PARSE_OBJECT       *Op)
{
    char                    *Temp = NULL;
    ACPI_PARSE_OBJECT_LIST  *NewTemp;
    ACPI_PARSE_OBJECT_LIST  *Current;
    ACPI_PARSE_OBJECT_LIST  *Previous;
    BOOLEAN                 FoundTemp = FALSE;


    switch (Op->Common.AmlOpcode)
    {
    case AML_NAME_OP:

        Temp = (char *) (&Op->Named.Name);

        if (!strncmp(Temp, "_T_", 3))
        {
            /* Allocate and init a new Temp List node */

            NewTemp = ACPI_ALLOCATE_ZEROED (sizeof (ACPI_PARSE_OBJECT_LIST));
            if (!NewTemp)
            {
                return (AE_NO_MEMORY);
            }

            if (AcpiGbl_TempListHead)
            {
                Current = AcpiGbl_TempListHead;
                AcpiGbl_TempListHead = NewTemp;
                AcpiGbl_TempListHead->Op = Op;
                AcpiGbl_TempListHead->Next = Current;
            }
            else
            {
                AcpiGbl_TempListHead = NewTemp;
                AcpiGbl_TempListHead->Op = Op;
                AcpiGbl_TempListHead->Next = NULL;
            }
        }
        break;

    case AML_WHILE_OP:

        if (!AcpiDmIsSwitchBlock (Op, &Temp))
        {
            break;
        }

        /* Found a Switch */

        Op->Common.DisasmOpcode = ACPI_DASM_SWITCH;

        Previous = Current = AcpiGbl_TempListHead;
        while (Current)
        {
            /* Note, if we get here Temp is not NULL */

            if (!strncmp(Temp, (char *) (&Current->Op->Named.Name), 4))
            {
                /* Match found. Ignore disassembly */

                Current->Op->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;

                /* Remove from list */

                if (Current == AcpiGbl_TempListHead)
                {
                    AcpiGbl_TempListHead = Current->Next;
                }
                else
                {
                    Previous->Next = Current->Next;
                }

                Current->Op = NULL;
                Current->Next = NULL;
                ACPI_FREE (Current);
                FoundTemp = TRUE;
                break;
            }

            Previous = Current;
            Current = Current->Next;
        }

        if (!FoundTemp)
        {
            fprintf (stderr,
                "Warning: Declaration for temp name %.4s not found\n", Temp);
        }
        break;

    default:
        break;
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmClearTempList
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Removes any remaining temporary objects from global list and
 *              frees
 *
 ******************************************************************************/

void
AcpiDmClearTempList (
    void)
{
    ACPI_PARSE_OBJECT_LIST      *Current;


    while (AcpiGbl_TempListHead)
    {
        Current = AcpiGbl_TempListHead;
        AcpiGbl_TempListHead = AcpiGbl_TempListHead->Next;
        Current->Op = NULL;
        Current->Next = NULL;
        ACPI_FREE (Current);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIsSwitchBlock
 *
 * PARAMETERS:  Op              - While Object
 *              Temp            - Where the compiler temp name is returned
 *                                  (_T_x)
 *
 * RETURN:      TRUE if While block can be converted to a Switch/Case block
 *
 * DESCRIPTION: Determines if While block is a Switch/Case statement. Modifies
 *              parse tree to allow for Switch/Case disassembly during walk.
 *
 * EXAMPLE: Example of parse tree to be converted
 *
 *    While
 *        One
 *        Store
 *            ByteConst
 *             -NamePath-
 *        If
 *            LEqual
 *                -NamePath-
 *                Zero
 *            Return
 *                One
 *        Else
 *            Return
 *                WordConst
 *        Break
 *
 ******************************************************************************/

BOOLEAN
AcpiDmIsSwitchBlock (
    ACPI_PARSE_OBJECT       *Op,
    char                    **Temp)
{
    ACPI_PARSE_OBJECT       *OneOp;
    ACPI_PARSE_OBJECT       *StoreOp;
    ACPI_PARSE_OBJECT       *NamePathOp;
    ACPI_PARSE_OBJECT       *PredicateOp;
    ACPI_PARSE_OBJECT       *CurrentOp;
    ACPI_PARSE_OBJECT       *TempOp;


    /* Check for One Op Predicate */

    OneOp = AcpiPsGetArg (Op, 0);
    if (!OneOp || (OneOp->Common.AmlOpcode != AML_ONE_OP))
    {
        return (FALSE);
    }

    /* Check for Store Op */

    StoreOp = OneOp->Common.Next;
    if (!StoreOp || (StoreOp->Common.AmlOpcode != AML_STORE_OP))
    {
        return (FALSE);
    }

    /* Check for Name Op with _T_ string */

    NamePathOp = AcpiPsGetArg (StoreOp, 1);
    if (!NamePathOp ||
        (NamePathOp->Common.AmlOpcode != AML_INT_NAMEPATH_OP))
    {
        return (FALSE);
    }

    if (strncmp ((char *) (NamePathOp->Common.Value.Name), "_T_", 3))
    {
        return (FALSE);
    }

    *Temp = (char *) (NamePathOp->Common.Value.Name);

    /* This is a Switch/Case control block */

    /* Ignore the One Op Predicate */

    OneOp->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;

    /* Ignore the Store Op, but not the children */

    StoreOp->Common.DisasmOpcode = ACPI_DASM_IGNORE_SINGLE;

    /*
     * First arg of Store Op is the Switch condition.
     * Mark it as a Switch predicate and as a parameter list for paren
     * closing and correct indentation.
     */
    PredicateOp = AcpiPsGetArg (StoreOp, 0);
    PredicateOp->Common.DisasmOpcode = ACPI_DASM_SWITCH_PREDICATE;
    PredicateOp->Common.DisasmFlags |= ACPI_PARSEOP_PARAMETER_LIST;

    /* Ignore the Name Op */

    NamePathOp->Common.DisasmFlags = ACPI_PARSEOP_IGNORE;

    /* Remaining opcodes are the Case statements (If/ElseIf's) */

    CurrentOp = StoreOp->Common.Next;
    while (AcpiDmIsCaseBlock (CurrentOp))
    {
        /* Block is a Case structure */

        if (CurrentOp->Common.AmlOpcode == AML_ELSE_OP)
        {
            /* ElseIf */

            CurrentOp->Common.DisasmOpcode = ACPI_DASM_CASE;
            CurrentOp = AcpiPsGetArg (CurrentOp, 0);
        }

        /* If */

        CurrentOp->Common.DisasmOpcode = ACPI_DASM_CASE;

        /*
         * Mark the parse tree for Case disassembly. There are two
         * types of Case statements. The first type of statement begins with
         * an LEqual. The second starts with an LNot and uses a Match statement
         * on a Package of constants.
         */
        TempOp = AcpiPsGetArg (CurrentOp, 0);
        switch (TempOp->Common.AmlOpcode)
        {
        case (AML_LOGICAL_EQUAL_OP):

            /* Ignore just the LEqual Op */

            TempOp->Common.DisasmOpcode = ACPI_DASM_IGNORE_SINGLE;

            /* Ignore the NamePath Op */

            TempOp = AcpiPsGetArg (TempOp, 0);
            TempOp->Common.DisasmFlags = ACPI_PARSEOP_IGNORE;

            /*
             * Second arg of LEqual will be the Case predicate.
             * Mark it as a predicate and also as a parameter list for paren
             * closing and correct indentation.
             */
            PredicateOp = TempOp->Common.Next;
            PredicateOp->Common.DisasmOpcode = ACPI_DASM_SWITCH_PREDICATE;
            PredicateOp->Common.DisasmFlags |= ACPI_PARSEOP_PARAMETER_LIST;
            break;

        case (AML_LOGICAL_NOT_OP):

            /*
             * The Package will be the predicate of the Case statement.
             * It's under:
             *            LNOT
             *                LEQUAL
             *                    MATCH
             *                        PACKAGE
             */

            /* Get the LEqual Op from LNot */

            TempOp = AcpiPsGetArg (TempOp, 0);

            /* Get the Match Op from LEqual */

            TempOp = AcpiPsGetArg (TempOp, 0);

            /* Get the Package Op from Match */

            PredicateOp = AcpiPsGetArg (TempOp, 0);

            /* Mark as parameter list for paren closing */

            PredicateOp->Common.DisasmFlags |= ACPI_PARSEOP_PARAMETER_LIST;

            /*
             * The Package list would be too deeply indented if we
             * chose to simply ignore the all the parent opcodes, so
             * we rearrange the parse tree instead.
             */

            /*
             * Save the second arg of the If/Else Op which is the
             * block code of code for this Case statement.
             */
            TempOp = AcpiPsGetArg (CurrentOp, 1);

            /*
             * Move the Package Op to the child (predicate) of the
             * Case statement.
             */
            CurrentOp->Common.Value.Arg = PredicateOp;
            PredicateOp->Common.Parent = CurrentOp;

            /* Add the block code */

            PredicateOp->Common.Next = TempOp;
            break;

        default:

            /* Should never get here */
            break;
        }

        /* Advance to next Case block */

        CurrentOp = CurrentOp->Common.Next;
    }

    /* If CurrentOp is now an Else, then this is a Default block */

    if (CurrentOp && CurrentOp->Common.AmlOpcode == AML_ELSE_OP)
    {
        CurrentOp->Common.DisasmOpcode = ACPI_DASM_DEFAULT;
    }

    /*
     * From the first If advance to the Break op. It's possible to
     * have an Else (Default) op here when there is only one Case
     * statement, so check for it.
     */
    CurrentOp = StoreOp->Common.Next->Common.Next;
    if (!CurrentOp)
    {
        return (FALSE);
    }
    if (CurrentOp->Common.AmlOpcode == AML_ELSE_OP)
    {
        CurrentOp = CurrentOp->Common.Next;
        if (!CurrentOp)
        {
            return (FALSE);
        }
    }

    /* Ignore the Break Op */

    CurrentOp->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;
    return (TRUE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIsCaseBlock
 *
 * PARAMETERS:  Op              - Object to test
 *
 * RETURN:      TRUE if Object is beginning of a Case block.
 *
 * DESCRIPTION: Determines if an Object is the beginning of a Case block for a
 *              Switch/Case statement. Parse tree must be one of the following
 *              forms:
 *
 *              Else (Optional)
 *                  If
 *                      LEqual
 *                          -NamePath- _T_x
 *
 *              Else (Optional)
 *                  If
 *                      LNot
 *                          LEqual
 *                              Match
 *                                  Package
 *                                      ByteConst
 *                                      -NamePath- _T_x
 *
 ******************************************************************************/

static BOOLEAN
AcpiDmIsCaseBlock (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *CurrentOp;


    if (!Op)
    {
        return (FALSE);
    }

    /* Look for an If or ElseIf */

    CurrentOp = Op;
    if (CurrentOp->Common.AmlOpcode == AML_ELSE_OP)
    {
        CurrentOp = AcpiPsGetArg (CurrentOp, 0);
        if (!CurrentOp)
        {
            return (FALSE);
        }
    }

    if (!CurrentOp || CurrentOp->Common.AmlOpcode != AML_IF_OP)
    {
        return (FALSE);
    }

    /* Child must be LEqual or LNot */

    CurrentOp = AcpiPsGetArg (CurrentOp, 0);
    if (!CurrentOp)
    {
        return (FALSE);
    }

    switch (CurrentOp->Common.AmlOpcode)
    {
    case (AML_LOGICAL_EQUAL_OP):

        /* Next child must be NamePath with string _T_ */

        CurrentOp = AcpiPsGetArg (CurrentOp, 0);
        if (!CurrentOp || !CurrentOp->Common.Value.Name ||
            strncmp(CurrentOp->Common.Value.Name, "_T_", 3))
        {
            return (FALSE);
        }
        break;

    case (AML_LOGICAL_NOT_OP):

        /* Child of LNot must be LEqual op */

        CurrentOp = AcpiPsGetArg (CurrentOp, 0);
        if (!CurrentOp || (CurrentOp->Common.AmlOpcode != AML_LOGICAL_EQUAL_OP))
        {
            return (FALSE);
        }

        /* Child of LNot must be Match op */

        CurrentOp = AcpiPsGetArg (CurrentOp, 0);
        if (!CurrentOp || (CurrentOp->Common.AmlOpcode != AML_MATCH_OP))
        {
            return (FALSE);
        }

        /* First child of Match must be Package op */

        CurrentOp = AcpiPsGetArg (CurrentOp, 0);
        if (!CurrentOp || (CurrentOp->Common.AmlOpcode != AML_PACKAGE_OP))
        {
            return (FALSE);
        }

        /* Third child of Match must be NamePath with string _T_ */

        CurrentOp = AcpiPsGetArg (CurrentOp->Common.Parent, 2);
        if (!CurrentOp || !CurrentOp->Common.Value.Name ||
            strncmp(CurrentOp->Common.Value.Name, "_T_", 3))
        {
            return (FALSE);
        }
        break;

    default:

        return (FALSE);
    }

    return (TRUE);
}

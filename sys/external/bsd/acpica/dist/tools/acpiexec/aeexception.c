/******************************************************************************
 *
 * Module Name: aeexception - Exception and signal handlers
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

#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("aeexception")


/* Local prototypes */

static void
AeDisplayMethodCallStack (
    void);


UINT32                      SigintCount = 0;
#define ACPI_MAX_CONTROL_C  5


/******************************************************************************
 *
 * FUNCTION:    AeExceptionHandler
 *
 * PARAMETERS:  Standard exception handler parameters
 *
 * RETURN:      Status
 *
 * DESCRIPTION: System exception handler for AcpiExec utility. Called from
 *              the core ACPICA code after any exception during method
 *              execution.
 *
 *****************************************************************************/

ACPI_STATUS
AeExceptionHandler (
    ACPI_STATUS             AmlStatus,
    ACPI_NAME               Name,
    UINT16                  Opcode,
    UINT32                  AmlOffset,
    void                    *Context)
{
    ACPI_STATUS             NewAmlStatus = AmlStatus;
    ACPI_STATUS             Status;
    ACPI_BUFFER             ReturnObj;
    ACPI_OBJECT_LIST        ArgList;
    ACPI_OBJECT             Arg[3];
    const char              *Exception;
    ACPI_HANDLE             ErrHandle;


    Exception = AcpiFormatException (AmlStatus);

    if (AcpiGbl_VerboseHandlers)
    {
        AcpiOsPrintf (AE_PREFIX
            "Exception %s during execution\n", Exception);

        if (Name)
        {
            if (ACPI_COMPARE_NAMESEG (&Name, ACPI_ROOT_PATHNAME))
            {
                AcpiOsPrintf (AE_PREFIX
                    "Evaluating executable code at [%s]\n", ACPI_NAMESPACE_ROOT);
            }
            else
            {
                AcpiOsPrintf (AE_PREFIX
                    "Evaluating Method or Node: [%4.4s]\n", (char *) &Name);
            }
        }

        /* Be terse about loop timeouts */

        if ((AmlStatus == AE_AML_LOOP_TIMEOUT) && AcpiGbl_AbortLoopOnTimeout)
        {
            AcpiOsPrintf (AE_PREFIX "Aborting loop after timeout\n");
            return (AE_OK);
        }

        AcpiOsPrintf ("\n" AE_PREFIX
            "AML Opcode [%s], Method Offset ~%5.5X\n",
            AcpiPsGetOpcodeName (Opcode), AmlOffset);
    }

    /* Invoke the _ERR method if present */

    Status = AcpiGetHandle (NULL, "\\_ERR", &ErrHandle);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    /* Setup parameter object */

    ArgList.Count = 3;
    ArgList.Pointer = Arg;

    Arg[0].Type = ACPI_TYPE_INTEGER;
    Arg[0].Integer.Value = AmlStatus;

    Arg[1].Type = ACPI_TYPE_STRING;
    Arg[1].String.Pointer = ACPI_CAST_PTR (char, Exception);
    Arg[1].String.Length = strlen (Exception);

    Arg[2].Type = ACPI_TYPE_INTEGER;
    Arg[2].Integer.Value = AcpiOsGetThreadId();

    /* Setup return buffer */

    ReturnObj.Pointer = NULL;
    ReturnObj.Length = ACPI_ALLOCATE_BUFFER;

    Status = AcpiEvaluateObject (ErrHandle, NULL, &ArgList, &ReturnObj);
    if (ACPI_SUCCESS (Status))
    {
        if (ReturnObj.Pointer)
        {
            /* Override original status */

            NewAmlStatus = (ACPI_STATUS)
                ((ACPI_OBJECT *) ReturnObj.Pointer)->Integer.Value;

            /* Free a buffer created via ACPI_ALLOCATE_BUFFER */

            AcpiOsFree (ReturnObj.Pointer);
        }
    }
    else if (Status != AE_NOT_FOUND)
    {
        AcpiOsPrintf (AE_PREFIX
            "Could not execute _ERR method, %s\n",
            AcpiFormatException (Status));
    }

Cleanup:

    if (AcpiGbl_IgnoreErrors)
    {
        /* Global option to ignore all method errors, just return OK */

        NewAmlStatus = AE_OK;
    }
    if (NewAmlStatus != AmlStatus)
    {
        /* Request to override actual status with a different status */

        AcpiOsPrintf (AE_PREFIX
            "Exception override, new status %s\n\n",
            AcpiFormatException (NewAmlStatus));
    }

    return (NewAmlStatus);
}


/******************************************************************************
 *
 * FUNCTION:    AeSignalHandler
 *
 * PARAMETERS:  Sig
 *
 * RETURN:      none
 *
 * DESCRIPTION: Master signal handler. Currently handles SIGINT (ctrl-c),
 *              and SIGSEGV (Segment violation).
 *
 *****************************************************************************/

void ACPI_SYSTEM_XFACE
AeSignalHandler (
    int                     Sig)
{

    fflush(stdout);
    AcpiOsPrintf ("\n" AE_PREFIX);

    switch (Sig)
    {
    case SIGINT:
        signal(Sig, SIG_IGN);
        AcpiOsPrintf ("<Control-C>\n");

        /* Force exit on multiple control-c */

        SigintCount++;
        if (SigintCount >= ACPI_MAX_CONTROL_C)
        {
            exit (0);
        }

        /* Abort the application if there are no methods executing */

        if (!AcpiGbl_MethodExecuting)
        {
            break;
        }

        /*
         * Abort the method(s). This will also dump the method call
         * stack so there is no need to do it here. The application
         * will then drop back into the debugger interface.
         */
        AcpiGbl_AbortMethod = TRUE;
        AcpiOsPrintf (AE_PREFIX "Control Method Call Stack:\n");
        signal (SIGINT, AeSignalHandler);
        return;

    case SIGSEGV:
        AcpiOsPrintf ("Segmentation Fault\n");
        AeDisplayMethodCallStack ();
        break;

    default:
        AcpiOsPrintf ("Unknown Signal, %X\n", Sig);
        break;
    }

    /* Terminate application -- cleanup then exit */

    AcpiOsPrintf (AE_PREFIX "Terminating\n");
    (void) AcpiOsTerminate ();
    exit (0);
}


/******************************************************************************
 *
 * FUNCTION:    AeDisplayMethodCallStack
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display current method call stack, if possible.
 *
 * NOTE:        Currently only called from a SIGSEGV, so AcpiExec is about
 *              to terminate.
 *
 *****************************************************************************/

static void
AeDisplayMethodCallStack (
    void)
{
    ACPI_WALK_STATE         *WalkState;
    ACPI_THREAD_STATE       *ThreadList = AcpiGbl_CurrentWalkList;
    char                    *FullPathname = NULL;


    if (!AcpiGbl_MethodExecuting)
    {
        AcpiOsPrintf (AE_PREFIX "No method is executing\n");
        return;
    }

    /*
     * Try to find the currently executing control method(s)
     *
     * Note: The following code may fault if the data structures are
     * in an indeterminate state when the interrupt occurs. However,
     * in practice, this works quite well and can provide very
     * valuable information.
     *
     * 1) Walk the global thread list
     */
    while (ThreadList &&
        (ThreadList->DescriptorType == ACPI_DESC_TYPE_STATE_THREAD))
    {
        /* 2) Walk the walk state list for this thread */

        WalkState = ThreadList->WalkStateList;
        while (WalkState &&
            (WalkState->DescriptorType == ACPI_DESC_TYPE_WALK))
        {
            /* An executing control method */

            if (WalkState->MethodNode)
            {
                FullPathname = AcpiNsGetExternalPathname (
                    WalkState->MethodNode);

                AcpiOsPrintf (AE_PREFIX
                    "Executing Method: %s\n", FullPathname);
            }

            /* Execution of a deferred opcode/node */

            if (WalkState->DeferredNode)
            {
                FullPathname = AcpiNsGetExternalPathname (
                    WalkState->DeferredNode);

                AcpiOsPrintf (AE_PREFIX
                    "Evaluating deferred node: %s\n", FullPathname);
            }

            /* Get the currently executing AML opcode */

            if ((WalkState->Opcode != AML_INT_METHODCALL_OP) &&
                FullPathname)
            {
                AcpiOsPrintf (AE_PREFIX
                    "Current AML Opcode in %s: [%s]-0x%4.4X at %p\n",
                    FullPathname, AcpiPsGetOpcodeName (WalkState->Opcode),
                    WalkState->Opcode, WalkState->Aml);
            }

            if (FullPathname)
            {
                ACPI_FREE (FullPathname);
                FullPathname = NULL;
            }

            WalkState = WalkState->Next;
        }

        ThreadList = ThreadList->Next;
    }
}

/*	$NetBSD: OsdMisc.c,v 1.16 2017/01/25 13:38:40 christos Exp $	*/

/*
 * Copyright 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (C) 2000 - 2017, Intel Corp.
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

/*
 * OS Services Layer
 *
 * 6.10: Miscellaneous
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: OsdMisc.c,v 1.16 2017/01/25 13:38:40 christos Exp $");

#include "opt_acpi.h"
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/db_machdep.h>

#include <ddb/db_extern.h>
#include <ddb/db_output.h>

#include <dev/acpi/acpica.h>
#include <dev/acpi/acpi_osd.h>

#ifdef ACPI_DEBUG
#include <external/bsd/acpica/dist/include/acpi.h>
#include <external/bsd/acpica/dist/include/accommon.h>
#include <external/bsd/acpica/dist/include/acdebug.h>
#endif

#ifdef ACPI_DSDT_OVERRIDE
#ifndef ACPI_DSDT_FILE
#define ACPI_DSDT_FILE "dsdt.hex"
#endif
#include ACPI_DSDT_FILE
#endif

int acpi_indebugger;

/*
 * AcpiOsSignal:
 *
 *	Break to the debugger or display a breakpoint message.
 */
ACPI_STATUS
AcpiOsSignal(UINT32 Function, void *Info)
{
	/*
	 * the upper layer might call with Info = NULL,
	 * which makes little sense.
	 */
	if (Info == NULL)
		return AE_NO_MEMORY;

	switch (Function) {
	case ACPI_SIGNAL_FATAL:
	    {
		ACPI_SIGNAL_FATAL_INFO *info = Info;

		panic("ACPI fatal signal: "
		    "Type 0x%08x, Code 0x%08x, Argument 0x%08x",
		    info->Type, info->Code, info->Argument);
		/* NOTREACHED */
		break;
	    }

	case ACPI_SIGNAL_BREAKPOINT:
	    {
#ifdef ACPI_BREAKPOINT
		char *info = Info;

		printf("%s\n", info);
#  if defined(DDB)
		Debugger();
#  else
		printf("ACPI: WARNING: DDB not configured into kernel.\n");
		return AE_NOT_EXIST;
#  endif
#endif
		break;
	    }

	default:
		return AE_BAD_PARAMETER;
	}

	return AE_OK;
}

ACPI_STATUS
AcpiOsGetLine(char *Buffer, UINT32 BufferLength, UINT32 *BytesRead)
{
#if defined(DDB)
	char *cp;

	db_readline(Buffer, 80);
	for (cp = Buffer; *cp != 0; cp++)
		if (*cp == '\n' || *cp == '\r')
			*cp = 0;
	db_output_line = 0;
	return AE_OK;
#else
	printf("ACPI: WARNING: DDB not configured into kernel.\n");
	return AE_NOT_EXIST;
#endif
}

ACPI_STATUS
AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable,
		    ACPI_TABLE_HEADER **NewTable)
{
#ifndef ACPI_DSDT_OVERRIDE
	*NewTable = NULL;
#else
	if (strncmp(ExistingTable->Signature, "DSDT", 4) == 0)
		*NewTable = (ACPI_TABLE_HEADER *)AmlCode;
	else
		*NewTable = NULL;
#endif
	return AE_OK;
}

ACPI_STATUS
AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *InitVal,
			 ACPI_STRING *NewVal)
{
	if (!InitVal || !NewVal)
		return AE_BAD_PARAMETER;

	*NewVal = NULL;
	return AE_OK;
}


/*
 * AcpiOsPhysicalTableOverride:
 *
 * ExistingTable       - Header of current table (probably firmware)
 * NewAddress          - Where new table address is returned
 *                       (Physical address)
 * NewTableLength      - Where new table length is returned
 *
 * RETURN:      Status, address/length of new table. Null pointer returned
 *              if no table is available to override.
 *
 * DESCRIPTION: Returns AE_SUPPORT, function not used in user space.
 */
ACPI_STATUS
AcpiOsPhysicalTableOverride (
    ACPI_TABLE_HEADER       *ExistingTable,
    ACPI_PHYSICAL_ADDRESS   *NewAddress,
    UINT32                  *NewTableLength)
{

	return AE_SUPPORT;
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsEnterSleep
 * 
 * PARAMETERS:  SleepState          - Which sleep state to enter
 *              RegaValue           - Register A value
 *              RegbValue           - Register B value
 *
 * RETURN:      Status
 *
 * DESCRIPTION: A hook before writing sleep registers to enter the sleep
 *              state. Return AE_CTRL_TERMINATE to skip further sleep register
 *              writes.
 *
 *****************************************************************************/
 
ACPI_STATUS
AcpiOsEnterSleep (
    UINT8                   SleepState,
    UINT32                  RegaValue,
    UINT32                  RegbValue)  
{

    return AE_OK;  
}   


/*
 * acpi_osd_debugger:
 *
 *	Enter the ACPICA debugger.
 */
void
acpi_osd_debugger(void)
{
#ifdef ACPI_DEBUG
	label_t	acpi_jmpbuf;
	label_t	*savejmp;

	printf("Entering ACPICA debugger...\n");
	savejmp = db_recover;
	setjmp(&acpi_jmpbuf);
	db_recover = &acpi_jmpbuf;

	acpi_indebugger = 1;
	AcpiDbUserCommands();
	acpi_indebugger = 0;

	db_recover = savejmp;
#else
	printf("ACPI: WARNING: ACPICA debugger not present.\n");
#endif
}

#ifdef ACPI_DEBUG

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("osnetbsdbg")


/******************************************************************************
 *
 * FUNCTION:    AcpiOsWaitCommandReady
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Negotiate with the debugger foreground thread (the user
 *              thread) to wait the readiness of a command.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsWaitCommandReady (
    void)
{
    ACPI_STATUS             Status;
    /* Force output to console until a command is entered */

    AcpiDbSetOutputDestination (ACPI_DB_CONSOLE_OUTPUT);

    /* Different prompt if method is executing */

    if (!AcpiGbl_MethodExecuting)
    {
	AcpiOsPrintf ("%1c ", ACPI_DEBUGGER_COMMAND_PROMPT);
    }
    else
    {
	AcpiOsPrintf ("%1c ", ACPI_DEBUGGER_EXECUTE_PROMPT);
    }

    /* Get the user input line */

    Status = AcpiOsGetLine (AcpiGbl_DbLineBuf,
	ACPI_DB_LINE_BUFFER_SIZE, NULL);

    if (ACPI_FAILURE (Status) && Status != AE_CTRL_TERMINATE)
    {
        ACPI_EXCEPTION ((AE_INFO, Status,
            "While parsing/handling command line"));
    }
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsNotifyCommandComplete
 *
 * PARAMETERS:  void
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Negotiate with the debugger foreground thread (the user
 *              thread) to notify the completion of a command.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsNotifyCommandComplete (
    void)
{

    return AE_OK;
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsInitializeDebugger
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initialize OSPM specific part of the debugger
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsInitializeDebugger (
    void)
{
    return AE_OK;
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsTerminateDebugger
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Terminate signals used by the multi-threading debugger
 *
 *****************************************************************************/

void
AcpiOsTerminateDebugger (
    void)
{
}


/******************************************************************************
 *
 * FUNCTION:    AcpiRunDebugger
 *
 * PARAMETERS:  BatchBuffer         - Buffer containing commands running in
 *                                    the batch mode
 *
 * RETURN:      None
 *
 * DESCRIPTION: Run a local/remote debugger
 *
 *****************************************************************************/

void
AcpiRunDebugger (
    char                    *BatchBuffer)
{
        AcpiDbUserCommands ();
}

ACPI_EXPORT_SYMBOL (AcpiRunDebugger)
#endif

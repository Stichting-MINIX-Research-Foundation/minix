/******************************************************************************
 *
 * Module Name: aetests - Miscellaneous tests for ACPICA public interfaces
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
        ACPI_MODULE_NAME    ("aetests")

/* Local prototypes */

static void
AeMutexInterfaces (
    void);

static void
AeHardwareInterfaces (
    void);

static void
AeTestSleepData (
    void);

static void
AeGlobalAddressRangeCheck(
    void);


/******************************************************************************
 *
 * FUNCTION:    AeMiscellaneousTests
 *
 * DESCRIPTION: Various ACPICA validation tests.
 *
 *****************************************************************************/

void
AeMiscellaneousTests (
    void)
{
    ACPI_BUFFER             ReturnBuf;
    char                    Buffer[32];
    ACPI_STATUS             Status;
    ACPI_STATISTICS         Stats;
    ACPI_HANDLE             Handle;

#if (!ACPI_REDUCED_HARDWARE)
    UINT32                  Temp;
    UINT32                  LockHandle1;
    UINT32                  LockHandle2;
    ACPI_VENDOR_UUID        Uuid = {0, {ACPI_INIT_UUID (0,0,0,0,0,0,0,0,0,0,0)}};
#endif /* !ACPI_REDUCED_HARDWARE */


    Status = AcpiGetHandle (NULL, "\\", &Handle);
    ACPI_CHECK_OK (AcpiGetHandle, Status);

    if (AcpiGbl_DoInterfaceTests)
    {
        /*
         * Tests for AcpiLoadTable and AcpiUnloadParentTable
         */

        /* Attempt unload of DSDT, should fail */

        Status = AcpiGetHandle (NULL, "\\_SB_", &Handle);
        ACPI_CHECK_OK (AcpiGetHandle, Status);

        Status = AcpiUnloadParentTable (Handle);
        ACPI_CHECK_STATUS (AcpiUnloadParentTable, Status, AE_TYPE);

        /* Load and unload SSDT4 */

        Status = AcpiLoadTable ((ACPI_TABLE_HEADER *) Ssdt4Code);
        ACPI_CHECK_OK (AcpiLoadTable, Status);

        Status = AcpiGetHandle (NULL, "\\_T96", &Handle);
        ACPI_CHECK_OK (AcpiGetHandle, Status);

        Status = AcpiUnloadParentTable (Handle);
        ACPI_CHECK_OK (AcpiUnloadParentTable, Status);

        /* Re-load SSDT4 */

        Status = AcpiLoadTable ((ACPI_TABLE_HEADER *) Ssdt4Code);
        ACPI_CHECK_OK (AcpiLoadTable, Status);

        /* Unload and re-load SSDT2 (SSDT2 is in the XSDT) */

        Status = AcpiGetHandle (NULL, "\\_T99", &Handle);
        ACPI_CHECK_OK (AcpiGetHandle, Status);

        Status = AcpiUnloadParentTable (Handle);
        ACPI_CHECK_OK (AcpiUnloadParentTable, Status);

        Status = AcpiLoadTable ((ACPI_TABLE_HEADER *) Ssdt2Code);
        ACPI_CHECK_OK (AcpiLoadTable, Status);

        /* Load OEM9 table (causes table override) */

        Status = AcpiLoadTable ((ACPI_TABLE_HEADER *) Ssdt3Code);
        ACPI_CHECK_OK (AcpiLoadTable, Status);
    }

    AeHardwareInterfaces ();
    AeGenericRegisters ();
    AeSetupConfiguration (Ssdt3Code);

    AeTestBufferArgument();
    AeTestPackageArgument ();
    AeMutexInterfaces ();
    AeTestSleepData ();

    /* Test _OSI install/remove */

    Status = AcpiInstallInterface ("");
    ACPI_CHECK_STATUS (AcpiInstallInterface, Status, AE_BAD_PARAMETER);

    Status = AcpiInstallInterface ("TestString");
    ACPI_CHECK_OK (AcpiInstallInterface, Status);

    Status = AcpiInstallInterface ("TestString");
    ACPI_CHECK_STATUS (AcpiInstallInterface, Status, AE_ALREADY_EXISTS);

    Status = AcpiRemoveInterface ("Windows 2006");
    ACPI_CHECK_OK (AcpiRemoveInterface, Status);

    Status = AcpiRemoveInterface ("TestString");
    ACPI_CHECK_OK (AcpiRemoveInterface, Status);

    Status = AcpiRemoveInterface ("XXXXXX");
    ACPI_CHECK_STATUS (AcpiRemoveInterface, Status, AE_NOT_EXIST);

    Status = AcpiInstallInterface ("AnotherTestString");
    ACPI_CHECK_OK (AcpiInstallInterface, Status);

    /* Test _OSI execution */

    Status = ExecuteOSI ("Extended Address Space Descriptor", ACPI_UINT64_MAX);
    ACPI_CHECK_OK (ExecuteOSI, Status);

    Status = ExecuteOSI ("Windows 2001", ACPI_UINT64_MAX);
    ACPI_CHECK_OK (ExecuteOSI, Status);

    Status = ExecuteOSI ("MichiganTerminalSystem", 0);
    ACPI_CHECK_OK (ExecuteOSI, Status);


    ReturnBuf.Length = 32;
    ReturnBuf.Pointer = Buffer;

    Status = AcpiGetName (ACPI_ROOT_OBJECT,
        ACPI_FULL_PATHNAME_NO_TRAILING, &ReturnBuf);
    ACPI_CHECK_OK (AcpiGetName, Status);

    /* Get Devices */

    Status = AcpiGetDevices (NULL, AeGetDevices, NULL, NULL);
    ACPI_CHECK_OK (AcpiGetDevices, Status);

    Status = AcpiGetStatistics (&Stats);
    ACPI_CHECK_OK (AcpiGetStatistics, Status);


#if (!ACPI_REDUCED_HARDWARE)

    Status = AcpiInstallGlobalEventHandler (AeGlobalEventHandler, NULL);
    ACPI_CHECK_OK (AcpiInstallGlobalEventHandler, Status);

    /* If Hardware Reduced flag is set, we are all done */

    if (AcpiGbl_ReducedHardware)
    {
        return;
    }

    Status = AcpiEnableEvent (ACPI_EVENT_GLOBAL, 0);
    ACPI_CHECK_OK (AcpiEnableEvent, Status);

    /*
     * GPEs: Handlers, enable/disable, etc.
     */
    Status = AcpiInstallGpeHandler (NULL, 0,
        ACPI_GPE_LEVEL_TRIGGERED, AeGpeHandler, NULL);
    ACPI_CHECK_OK (AcpiInstallGpeHandler, Status);

    Status = AcpiEnableGpe (NULL, 0);
    ACPI_CHECK_OK (AcpiEnableGpe, Status);

    Status = AcpiRemoveGpeHandler (NULL, 0, AeGpeHandler);
    ACPI_CHECK_OK (AcpiRemoveGpeHandler, Status);

    Status = AcpiInstallGpeHandler (NULL, 0,
        ACPI_GPE_LEVEL_TRIGGERED, AeGpeHandler, NULL);
    ACPI_CHECK_OK (AcpiInstallGpeHandler, Status);

    Status = AcpiEnableGpe (NULL, 0);
    ACPI_CHECK_OK (AcpiEnableGpe, Status);

    Status = AcpiSetGpe (NULL, 0, ACPI_GPE_DISABLE);
    ACPI_CHECK_OK (AcpiSetGpe, Status);

    Status = AcpiSetGpe (NULL, 0, ACPI_GPE_ENABLE);
    ACPI_CHECK_OK (AcpiSetGpe, Status);


    Status = AcpiInstallGpeHandler (NULL, 1,
        ACPI_GPE_EDGE_TRIGGERED, AeGpeHandler, NULL);
    ACPI_CHECK_OK (AcpiInstallGpeHandler, Status);

    Status = AcpiEnableGpe (NULL, 1);
    ACPI_CHECK_OK (AcpiEnableGpe, Status);


    Status = AcpiInstallGpeHandler (NULL, 2,
        ACPI_GPE_LEVEL_TRIGGERED, AeGpeHandler, NULL);
    ACPI_CHECK_OK (AcpiInstallGpeHandler, Status);

    Status = AcpiEnableGpe (NULL, 2);
    ACPI_CHECK_OK (AcpiEnableGpe, Status);


    Status = AcpiInstallGpeHandler (NULL, 3,
        ACPI_GPE_EDGE_TRIGGERED, AeGpeHandler, NULL);
    ACPI_CHECK_OK (AcpiInstallGpeHandler, Status);

    Status = AcpiInstallGpeHandler (NULL, 4,
        ACPI_GPE_LEVEL_TRIGGERED, AeGpeHandler, NULL);
    ACPI_CHECK_OK (AcpiInstallGpeHandler, Status);

    Status = AcpiInstallGpeHandler (NULL, 5,
        ACPI_GPE_EDGE_TRIGGERED, AeGpeHandler, NULL);
    ACPI_CHECK_OK (AcpiInstallGpeHandler, Status);

    Status = AcpiGetHandle (NULL, "\\_SB", &Handle);
    ACPI_CHECK_OK (AcpiGetHandle, Status);

    Status = AcpiSetupGpeForWake (Handle, NULL, 5);
    ACPI_CHECK_OK (AcpiSetupGpeForWake, Status);

    Status = AcpiSetGpeWakeMask (NULL, 5, ACPI_GPE_ENABLE);
    ACPI_CHECK_OK (AcpiSetGpeWakeMask, Status);

    Status = AcpiSetupGpeForWake (Handle, NULL, 6);
    ACPI_CHECK_OK (AcpiSetupGpeForWake, Status);

    Status = AcpiSetupGpeForWake (ACPI_ROOT_OBJECT, NULL, 6);
    ACPI_CHECK_OK (AcpiSetupGpeForWake, Status);

    Status = AcpiSetupGpeForWake (Handle, NULL, 9);
    ACPI_CHECK_OK (AcpiSetupGpeForWake, Status);

    Status = AcpiInstallGpeHandler (NULL, 0x19,
        ACPI_GPE_LEVEL_TRIGGERED, AeGpeHandler, NULL);
    ACPI_CHECK_OK (AcpiInstallGpeHandler, Status);

    Status = AcpiEnableGpe (NULL, 0x19);
    ACPI_CHECK_OK (AcpiEnableGpe, Status);


    /* GPE block 1 */

    Status = AcpiInstallGpeHandler (NULL, 101,
        ACPI_GPE_LEVEL_TRIGGERED, AeGpeHandler, NULL);
    ACPI_CHECK_OK (AcpiInstallGpeHandler, Status);

    Status = AcpiEnableGpe (NULL, 101);
    ACPI_CHECK_OK (AcpiEnableGpe, Status);

    Status = AcpiDisableGpe (NULL, 101);
    ACPI_CHECK_OK (AcpiDisableGpe, Status);

    AfInstallGpeBlock ();

    /* Here is where the GPEs are actually "enabled" */

    Status = AcpiUpdateAllGpes ();
    ACPI_CHECK_OK (AcpiUpdateAllGpes, Status);

    Status = AcpiGetHandle (NULL, "RSRC", &Handle);
    if (ACPI_SUCCESS (Status))
    {
        ReturnBuf.Length = ACPI_ALLOCATE_BUFFER;

        Status = AcpiGetVendorResource (Handle, "_CRS", &Uuid, &ReturnBuf);
        if (ACPI_SUCCESS (Status))
        {
            AcpiOsFree (ReturnBuf.Pointer);
        }
    }

    /* Test global lock */

    Status = AcpiAcquireGlobalLock (0xFFFF, &LockHandle1);
    ACPI_CHECK_OK (AcpiAcquireGlobalLock, Status);

    Status = AcpiAcquireGlobalLock (0x5, &LockHandle2);
    ACPI_CHECK_OK (AcpiAcquireGlobalLock, Status);

    Status = AcpiReleaseGlobalLock (LockHandle1);
    ACPI_CHECK_OK (AcpiReleaseGlobalLock, Status);

    Status = AcpiReleaseGlobalLock (LockHandle2);
    ACPI_CHECK_OK (AcpiReleaseGlobalLock, Status);

    /* Test timer interfaces */

    Status = AcpiGetTimerResolution (&Temp);
    ACPI_CHECK_OK (AcpiGetTimerResolution, Status);

    Status = AcpiGetTimer (&Temp);
    ACPI_CHECK_OK (AcpiGetTimer, Status);

    Status = AcpiGetTimerDuration (0x1000, 0x2000, &Temp);
    ACPI_CHECK_OK (AcpiGetTimerDuration, Status);


#endif /* !ACPI_REDUCED_HARDWARE */
}


/******************************************************************************
 *
 * FUNCTION:    AeMutexInterfaces
 *
 * DESCRIPTION: Exercise the AML mutex access interfaces
 *
 *****************************************************************************/

static void
AeMutexInterfaces (
    void)
{
    ACPI_STATUS             Status;
    ACPI_HANDLE             MutexHandle;


    /* Get a handle to an AML mutex */

    Status = AcpiGetHandle (NULL, "\\MTX1", &MutexHandle);
    if (Status == AE_NOT_FOUND)
    {
        return;
    }

    ACPI_CHECK_OK (AcpiGetHandle, Status);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Acquire the  mutex */

    Status = AcpiAcquireMutex (NULL, "\\MTX1", 0xFFFF);
    ACPI_CHECK_OK (AcpiAcquireMutex, Status);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Release mutex with different parameters */

    Status = AcpiReleaseMutex (MutexHandle, NULL);
    ACPI_CHECK_OK (AcpiReleaseMutex, Status);
}


/******************************************************************************
 *
 * FUNCTION:    AeHardwareInterfaces
 *
 * DESCRIPTION: Call various hardware support interfaces
 *
 *****************************************************************************/

static void
AeHardwareInterfaces (
    void)
{
#if (!ACPI_REDUCED_HARDWARE)

    ACPI_STATUS             Status;
    UINT32                  Value;


    /* If Hardware Reduced flag is set, we are all done */

    if (AcpiGbl_ReducedHardware)
    {
        return;
    }

    Status = AcpiWriteBitRegister (ACPI_BITREG_WAKE_STATUS, 1);
    ACPI_CHECK_OK (AcpiWriteBitRegister, Status);

    Status = AcpiWriteBitRegister (ACPI_BITREG_GLOBAL_LOCK_ENABLE, 1);
    ACPI_CHECK_OK (AcpiWriteBitRegister, Status);

    Status = AcpiWriteBitRegister (ACPI_BITREG_SLEEP_ENABLE, 1);
    ACPI_CHECK_OK (AcpiWriteBitRegister, Status);

    Status = AcpiWriteBitRegister (ACPI_BITREG_ARB_DISABLE, 1);
    ACPI_CHECK_OK (AcpiWriteBitRegister, Status);


    Status = AcpiReadBitRegister (ACPI_BITREG_WAKE_STATUS, &Value);
    ACPI_CHECK_OK (AcpiReadBitRegister, Status);

    Status = AcpiReadBitRegister (ACPI_BITREG_GLOBAL_LOCK_ENABLE, &Value);
    ACPI_CHECK_OK (AcpiReadBitRegister, Status);

    Status = AcpiReadBitRegister (ACPI_BITREG_SLEEP_ENABLE, &Value);
    ACPI_CHECK_OK (AcpiReadBitRegister, Status);

    Status = AcpiReadBitRegister (ACPI_BITREG_ARB_DISABLE, &Value);
    ACPI_CHECK_OK (AcpiReadBitRegister, Status);

#endif /* !ACPI_REDUCED_HARDWARE */
}


/******************************************************************************
 *
 * FUNCTION:    AeTestSleepData
 *
 * DESCRIPTION: Exercise the sleep/wake support (_S0, _S1, etc.)
 *
 *****************************************************************************/

static void
AeTestSleepData (
    void)
{
    int                     State;
    UINT8                   TypeA;
    UINT8                   TypeB;
    ACPI_STATUS             Status;


    /* Attempt to get sleep data for all known sleep states */

    for (State = ACPI_STATE_S0; State <= ACPI_S_STATES_MAX; State++)
    {
        Status = AcpiGetSleepTypeData ((UINT8) State, &TypeA, &TypeB);

        /* All sleep methods are optional */

        if (Status != AE_NOT_FOUND)
        {
            ACPI_CHECK_OK (AcpiGetSleepTypeData, Status);
        }
    }
}


/******************************************************************************
 *
 * FUNCTION:    AeLateTest
 *
 * DESCRIPTION: Exercise tests that should be performed before shutdown.
 *
 *****************************************************************************/

void
AeLateTest (
    void)
{
    AeGlobalAddressRangeCheck();
}


/******************************************************************************
 *
 * FUNCTION:    AeGlobalAddressRangeCheck
 *
 * DESCRIPTION: There have been some issues in the past with adding and
 *              removing items to the global address list from
 *              OperationRegions declared in control methods. This test loops
 *              over the list to ensure that dangling pointers do not exist in
 *              the global address list.
 *
 *****************************************************************************/

static void
AeGlobalAddressRangeCheck(
    void)
{
    ACPI_STATUS             Status;
    ACPI_ADDRESS_RANGE      *Current;
    ACPI_BUFFER             ReturnBuffer;
    UINT32                  i;


    ReturnBuffer.Length = ACPI_ALLOCATE_BUFFER;
    AcpiUtInitializeBuffer (&ReturnBuffer, ACPI_ALLOCATE_BUFFER);

    for (i = 0; i < ACPI_ADDRESS_RANGE_MAX; i++)
    {
        Current = AcpiGbl_AddressRangeList[i];

        while (Current)
        {
            Status = AcpiGetName (Current->RegionNode, ACPI_SINGLE_NAME, &ReturnBuffer);
            ACPI_CHECK_OK (AcpiGetname, Status);

            Current = Current->Next;
        }
    }
}

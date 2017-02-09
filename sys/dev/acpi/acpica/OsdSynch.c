/*	$NetBSD: OsdSynch.c,v 1.13 2009/08/18 16:41:02 jmcneill Exp $	*/

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

/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * OS Services Layer
 *
 * 6.4: Mutual Exclusion and Synchronization
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: OsdSynch.c,v 1.13 2009/08/18 16:41:02 jmcneill Exp $");

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/kernel.h>
#include <sys/proc.h>

extern int acpi_suspended;

#include <dev/acpi/acpica.h>

MALLOC_DECLARE(M_ACPI);

#define	_COMPONENT	ACPI_OS_SERVICES
ACPI_MODULE_NAME("SYNCH")

/*
 * Simple counting semaphore implemented using a mutex.  This is
 * subsequently used in the OSI code to implement a mutex.  Go figure.
 */
struct acpi_semaphore {
	kcondvar_t as_cv;
	kmutex_t as_slock;
	UINT32 as_units;
	UINT32 as_maxunits;
};

struct acpi_lock {
	kmutex_t al_slock;
};

/*
 * AcpiOsCreateSemaphore:
 *
 *	Create a semaphore.
 */
ACPI_STATUS
AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits,
    ACPI_HANDLE *OutHandle)
{
	struct acpi_semaphore *as;

	if (OutHandle == NULL)
		return AE_BAD_PARAMETER;
	if (InitialUnits > MaxUnits)
		return AE_BAD_PARAMETER;

	as = malloc(sizeof(*as), M_ACPI, M_NOWAIT);
	if (as == NULL)
		return AE_NO_MEMORY;

	mutex_init(&as->as_slock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&as->as_cv, "acpisem");
	as->as_units = InitialUnits;
	as->as_maxunits = MaxUnits;

	ACPI_DEBUG_PRINT((ACPI_DB_MUTEX,
	    "created semaphore %p max %u initial %u\n",
	    as, as->as_maxunits, as->as_units));

	*OutHandle = (ACPI_HANDLE) as;
	return AE_OK;
}

/*
 * AcpiOsDeleteSemaphore:
 *
 *	Delete a semaphore.
 */
ACPI_STATUS
AcpiOsDeleteSemaphore(ACPI_HANDLE Handle)
{
	struct acpi_semaphore *as = (void *) Handle;

	if (as == NULL)
		return AE_BAD_PARAMETER;

	cv_destroy(&as->as_cv);
	mutex_destroy(&as->as_slock);
	free(as, M_ACPI);

	ACPI_DEBUG_PRINT((ACPI_DB_MUTEX, "destroyed semaphore %p\n", as));

	return AE_OK;
}

/*
 * AcpiOsWaitSemaphore:
 *
 *	Wait for units from a semaphore.
 */
ACPI_STATUS
AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout)
{
	struct acpi_semaphore *as = (void *) Handle;
	ACPI_STATUS rv;
	int timo, error;

	/*
	 * This implementation has a bug: It has to stall for the entire
	 * timeout before it will return AE_TIME.  A better implementation
	 * would adjust the amount of time left after being awakened.
	 */

	if (as == NULL)
		return AE_BAD_PARAMETER;
	if (cold || doing_shutdown || acpi_suspended)
		return AE_OK;

	/* A timeout of 0xFFFF means "forever". */
	if (Timeout == 0xFFFF)
		timo = 0;
	else {
		/* Compute the timeout using uSec per tick. */
		timo = (Timeout * 1000) / (1000000 / hz);
		if (timo <= 0)
			timo = 1;
	}

	mutex_enter(&as->as_slock);

	ACPI_DEBUG_PRINT((ACPI_DB_MUTEX,
	    "get %d units from semaphore %p (has %d) timeout %d\n",
	    Units, as, as->as_units, Timeout));

	for (;;) {
		if (as->as_units >= Units) {
			as->as_units -= Units;
			rv = AE_OK;
			break;
		}

		ACPI_DEBUG_PRINT((ACPI_DB_MUTEX,
		    "semaphore blocked, sleeping %d ticks\n", timo));

		error = cv_timedwait(&as->as_cv, &as->as_slock, timo);
		if (error == EWOULDBLOCK) {
			rv = AE_TIME;
			break;
		}
	}

	mutex_exit(&as->as_slock);

	return rv;
}

/*
 * AcpiOsSignalSemaphore:
 *
 *	Send units to a semaphore.
 */
ACPI_STATUS
AcpiOsSignalSemaphore(ACPI_HANDLE Handle, UINT32 Units)
{
	struct acpi_semaphore *as = (void *) Handle;

	if (as == NULL)
		return AE_BAD_PARAMETER;

	mutex_enter(&as->as_slock);

	ACPI_DEBUG_PRINT((ACPI_DB_MUTEX,
	    "return %d units to semaphore %p (has %d)\n",
	    Units, as, as->as_units));

	as->as_units += Units;
	if (as->as_units > as->as_maxunits)
		as->as_units = as->as_maxunits;
	cv_broadcast(&as->as_cv);

	mutex_exit(&as->as_slock);

	return AE_OK;
}

/*
 * AcpiOsCreateLock:
 *
 *	Create a lock.
 */
ACPI_STATUS
AcpiOsCreateLock(ACPI_HANDLE *OutHandle)
{
	struct acpi_lock *al;

	if (OutHandle == NULL)
		return AE_BAD_PARAMETER;

	al = malloc(sizeof(*al), M_ACPI, M_NOWAIT);
	if (al == NULL)
		return AE_NO_MEMORY;

	mutex_init(&al->al_slock, MUTEX_DEFAULT, IPL_VM);

	ACPI_DEBUG_PRINT((ACPI_DB_MUTEX,
	    "created lock %p\n", al));

	*OutHandle = (ACPI_HANDLE) al;
	return AE_OK;
}

/*
 * AcpiOsDeleteLock:
 *
 *	Delete a lock.
 */
void
AcpiOsDeleteLock(ACPI_SPINLOCK Handle)
{
	struct acpi_lock *al = (void *) Handle;

	if (al == NULL)
		return;

	mutex_destroy(&al->al_slock);
	free(al, M_ACPI);

	ACPI_DEBUG_PRINT((ACPI_DB_MUTEX, "destroyed lock %p\n", al));

	return;
}

/*
 * AcpiOsAcquireLock:
 *
 *	Acquire a lock.
 */
ACPI_CPU_FLAGS
AcpiOsAcquireLock(ACPI_SPINLOCK Handle)
{
	struct acpi_lock *al = (void *) Handle;

	if (al == NULL)
		return 0;

	mutex_enter(&al->al_slock);

	return 0;
}

/*
 * AcpiOsReleaseLock:
 *
 *	Release a lock.
 */
void
AcpiOsReleaseLock(ACPI_HANDLE Handle, ACPI_CPU_FLAGS Flags)
{
	struct acpi_lock *al = (void *) Handle;

	if (al == NULL)
		return;

	mutex_exit(&al->al_slock);

	return;
}

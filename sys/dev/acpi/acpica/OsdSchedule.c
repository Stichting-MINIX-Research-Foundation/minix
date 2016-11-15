/*	$NetBSD: OsdSchedule.c,v 1.17 2013/12/27 18:53:25 christos Exp $	*/

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
 * OS Services Layer
 *
 * 6.3: Scheduling services
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: OsdSchedule.c,v 1.17 2013/12/27 18:53:25 christos Exp $");

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/condvar.h>
#include <sys/mutex.h>

#include <dev/acpi/acpica.h>

#include <dev/acpi/acpi_osd.h>

#include <dev/sysmon/sysmon_taskq.h>

extern int acpi_suspended;

#define	_COMPONENT	ACPI_OS_SERVICES
ACPI_MODULE_NAME("SCHEDULE")

static kcondvar_t	acpi_osd_sleep_cv;
static kmutex_t		acpi_osd_sleep_mtx;

/*
 * acpi_osd_sched_init:
 *
 *	Initialize the APCICA Osd scheduler.  Called from AcpiOsInitialize().
 */
void
acpi_osd_sched_init(void)
{
	sysmon_task_queue_init();
	mutex_init(&acpi_osd_sleep_mtx, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&acpi_osd_sleep_cv, "acpislp");
}

/*
 * AcpiOsGetThreadId:
 *
 *	Obtain the ID of the currently executing thread.
 */
ACPI_THREAD_ID
AcpiOsGetThreadId(void)
{
	return (ACPI_THREAD_ID)(uintptr_t)curlwp;
}

/*
 * AcpiOsQueueForExecution:
 *
 *	Schedule a procedure for deferred execution.
 */
ACPI_STATUS
AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function,
    void *Context)
{
	int pri;

	switch (Type) {
	case OSL_GPE_HANDLER:
		pri = 10;
		break;
	case OSL_GLOBAL_LOCK_HANDLER:
	case OSL_EC_POLL_HANDLER:
	case OSL_EC_BURST_HANDLER:
		pri = 5;
		break;
	case OSL_NOTIFY_HANDLER:
		pri = 3;
		break;
	case OSL_DEBUGGER_THREAD:
		pri = 0;
		break;
	default:
		return AE_BAD_PARAMETER;
	}

	switch (sysmon_task_queue_sched(pri, Function, Context)) {
	case 0:
		return AE_OK;

	case ENOMEM:
		return AE_NO_MEMORY;

	default:
		return AE_BAD_PARAMETER;
	}
}

/*
 * AcpiOsSleep:
 *
 *	Suspend the running task (coarse granularity).
 */
void
AcpiOsSleep(ACPI_INTEGER Milliseconds)
{

	if (cold || doing_shutdown || acpi_suspended)
		DELAY(Milliseconds * 1000);
	else {
		mutex_enter(&acpi_osd_sleep_mtx);
		cv_timedwait_sig(&acpi_osd_sleep_cv, &acpi_osd_sleep_mtx,
		    MAX(mstohz(Milliseconds), 1));
		mutex_exit(&acpi_osd_sleep_mtx);
	}
}

/*
 * AcpiOsStall:
 *
 *	Suspend the running task (fine granularity).
 */
void
AcpiOsStall(UINT32 Microseconds)
{

	delay(Microseconds);
}

/*
 * AcpiOsGetTimer:
 *
 *	Get the current system time in 100 nanosecond units
 */
UINT64
AcpiOsGetTimer(void)
{
	struct timeval tv;
	UINT64 t;

	/* XXX During early boot there is no (decent) timer available yet. */
	if (cold)
		panic("acpi: timer op not yet supported during boot");

	microtime(&tv);
	t = (UINT64)10 * tv.tv_usec;
	t += (UINT64)10000000 * tv.tv_sec;

	return t;
}

/*
 *
 * AcpiOsWaitEventsComplete:
 *
 * 	Wait for all asynchronous events to complete. This implementation
 *	does nothing.
 */
void
AcpiOsWaitEventsComplete(void)
{
	return;
}

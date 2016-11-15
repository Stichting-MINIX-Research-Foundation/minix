/* $NetBSD: acpi_timer.c,v 1.22 2013/12/27 18:51:44 christos Exp $ */

/*-
 * Copyright (c) 2006 Matthias Drochner <drochner@NetBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpi_timer.c,v 1.22 2013/12/27 18:51:44 christos Exp $");

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/timetc.h>

#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_timer.h>

#include <machine/acpi_machdep.h>

static int	acpitimer_test(void);

static struct timecounter acpi_timecounter = {
	acpitimer_read_safe,
	0,
	0x00ffffff,
	ACPI_PM_TIMER_FREQUENCY,
	"ACPI-Safe",
	900,
	NULL,
	NULL,
};

int
acpitimer_init(struct acpi_softc *sc)
{
	ACPI_STATUS rv;
	uint32_t bits;
	int i, j;

	rv = AcpiGetTimerResolution(&bits);

	if (ACPI_FAILURE(rv))
		return -1;

	if (bits == 32)
		acpi_timecounter.tc_counter_mask = 0xffffffff;

	for (i = j = 0; i < 10; i++)
		j += acpitimer_test();

	if (j >= 10) {
		acpi_timecounter.tc_name = "ACPI-Fast";
		acpi_timecounter.tc_get_timecount = acpitimer_read_fast;
		acpi_timecounter.tc_quality = 1000;
	}

	tc_init(&acpi_timecounter);

	aprint_debug_dev(sc->sc_dev, "%s %d-bit timer\n",
	    acpi_timecounter.tc_name, bits);

	return 0;
}

int
acpitimer_detach(void)
{

	return tc_detach(&acpi_timecounter);
}

u_int
acpitimer_read_fast(struct timecounter *tc)
{
	uint32_t t;

	(void)AcpiGetTimer(&t);

	return t;
}

/*
 * Some chipsets (PIIX4 variants) do not latch correctly;
 * there is a chance that a transition is hit.
 */
u_int
acpitimer_read_safe(struct timecounter *tc)
{
	uint32_t t1, t2, t3;

	(void)AcpiGetTimer(&t2);
	(void)AcpiGetTimer(&t3);

	do {
		t1 = t2;
		t2 = t3;

		(void)AcpiGetTimer(&t3);

	} while ((t1 > t2) || (t2 > t3));

	return t2;
}

uint32_t
acpitimer_delta(uint32_t end, uint32_t start)
{
	const u_int mask = acpi_timecounter.tc_counter_mask;
	uint32_t delta;

	if (end >= start)
		delta = end - start;
	else
		delta = ((mask - start) + end + 1) & mask;

	return delta;
}

#define N 2000

static int
acpitimer_test(void)
{
	uint32_t last, this, delta;
	int minl, maxl, n;

	minl = 10000000;
	maxl = 0;

	acpi_md_OsDisableInterrupt();

	(void)AcpiGetTimer(&last);

	for (n = 0; n < N; n++) {

		(void)AcpiGetTimer(&this);

		delta = acpitimer_delta(this, last);

		if (delta > maxl)
			maxl = delta;
		else if (delta < minl)
			minl = delta;

		last = this;
	}

	acpi_md_OsEnableInterrupt();

	if (maxl - minl > 2 )
		n = 0;
	else if (minl < 0 || maxl == 0)
		n = 0;
	else
		n = 1;

	return n;
}

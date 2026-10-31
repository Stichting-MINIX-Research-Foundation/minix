/* $NetBSD: acpi_slit.c,v 1.3 2010/03/05 14:00:17 jruoho Exp $ */

/*
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christoph Egger.
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
__KERNEL_RCSID(0, "$NetBSD: acpi_slit.c,v 1.3 2010/03/05 14:00:17 jruoho Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_slit.h>

static ACPI_TABLE_SLIT *slit;

bool
acpislit_exist(void)
{
	ACPI_TABLE_HEADER *table;
	ACPI_STATUS rv;

	rv = AcpiGetTable(ACPI_SIG_SLIT, 1, (ACPI_TABLE_HEADER **)&table);
	if (ACPI_FAILURE(rv))
		return false;

	/* Check if header is valid */
	if (table == NULL)
		return false;

	if (table->Length == 0xffffffff)
		return false;

	slit = (ACPI_TABLE_SLIT *)table;

	return true;
}

int
acpislit_init(void)
{
	if (!acpislit_exist())
		return EEXIST;
	return 0;
}

void
acpislit_dump(void)
{
	uint64_t idx, count;

	count = acpislit_cpus();

	aprint_debug("SLIT: matrix with %"PRIu64" nodes\n", count);
	aprint_debug("SLIT: ");
	for (idx = 0; idx < (count * count); idx++) {
		aprint_debug("%u ", slit->Entry[idx]);
		if ((idx % count) == (count - 1)) {
			aprint_debug("\n");
			if (idx < (count * count) - 1)
				aprint_debug("SLIT: ");
		}
	}
}

uint32_t
acpislit_cpus(void)
{
	return slit->LocalityCount;
}

uint32_t
acpislit_distance(uint32_t cpu1, uint32_t cpu2)
{
	uint64_t idx = cpu1 * slit->LocalityCount + cpu2;
	return slit->Entry[idx];
}

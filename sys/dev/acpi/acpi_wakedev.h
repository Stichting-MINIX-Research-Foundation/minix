/* $NetBSD: acpi_wakedev.h,v 1.6 2011/02/19 09:52:32 jruoho Exp $ */

/*-
 * Copyright (c) 2009, 2011 Jared D. McNeill <jmcneill@invisible.ca>
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

#ifndef _SYS_DEV_ACPI_ACPI_WAKEDEV_H
#define _SYS_DEV_ACPI_ACPI_WAKEDEV_H

struct acpi_wakedev {
	ACPI_HANDLE	aw_power[8];	/* Power resources */
	ACPI_HANDLE	aw_handle;	/* Wake GPE handle */
	ACPI_INTEGER	aw_number;	/* Wake GPE number */
	ACPI_INTEGER	aw_state;	/* Highest sleep state for wake */
	bool		aw_enable;	/* Wake enabled (sysctl)? */
};

void	 acpi_wakedev_init(struct acpi_devnode *);
void	 acpi_wakedev_add(struct acpi_devnode *);
void	 acpi_wakedev_commit(struct acpi_softc *, int);

#endif	/* !_SYS_DEV_ACPI_ACPI_WAKEDEV_H */

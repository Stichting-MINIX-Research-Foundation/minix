/*	$NetBSD: wmi_acpivar.h,v 1.5 2010/10/28 15:55:04 jruoho Exp $	*/

/*-
 * Copyright (c) 2009, 2010 Jukka Ruohonen <jruohonen@iki.fi>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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

#ifndef _SYS_DEV_ACPI_WMI_WMI_ACPIVAR_H
#define _SYS_DEV_ACPI_WMI_WMI_ACPIVAR_H

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: wmi_acpivar.h,v 1.5 2010/10/28 15:55:04 jruoho Exp $");

ACPI_STATUS	acpi_wmi_event_register(device_t, ACPI_NOTIFY_HANDLER);
ACPI_STATUS	acpi_wmi_event_deregister(device_t);
ACPI_STATUS	acpi_wmi_event_get(device_t, uint32_t, ACPI_BUFFER *);

int		acpi_wmi_guid_match(device_t, const char *);

ACPI_STATUS	acpi_wmi_data_query(device_t, const char *,
				uint8_t, ACPI_BUFFER *);
ACPI_STATUS	acpi_wmi_data_write(device_t, const char *,
				uint8_t, ACPI_BUFFER *);

ACPI_STATUS	acpi_wmi_method(device_t, const char *, uint8_t,
				uint32_t, ACPI_BUFFER *, ACPI_BUFFER *);

struct guid_t {

	/*
	 * The GUID itself. The used format is the usual 32-16-16-64-bit
	 * representation. All except the fourth field are in native byte
	 * order. A 32-16-16-16-48-bit hexadecimal notation with hyphens
	 * is used for human-readable GUIDs.
	 */
	struct {
		uint32_t data1;
		uint16_t data2;
		uint16_t data3;
		uint8_t  data4[8];
	} __packed;

	union {
		char oid[2];            /* ACPI object ID. */

		struct {
			uint8_t nid;    /* Notification value. */
			uint8_t res;    /* Reserved. */
		} __packed;
	} __packed;

	uint8_t count;	                /* Number of instances. */
	uint8_t flags;		        /* Additional flags. */

} __packed;

#define ACPI_WMI_FLAG_EXPENSIVE 0x01
#define ACPI_WMI_FLAG_METHOD    0x02
#define ACPI_WMI_FLAG_STRING    0x04
#define ACPI_WMI_FLAG_EVENT     0x08
#define ACPI_WMI_FLAG_DATA      (ACPI_WMI_FLAG_EXPENSIVE |		\
	                         ACPI_WMI_FLAG_STRING)

struct wmi_t {
	struct guid_t		guid;
	bool			eevent;

	SIMPLEQ_ENTRY(wmi_t)	wmi_link;
};

struct acpi_wmi_softc {
	device_t		 sc_dev;
	device_t		 sc_child;
	device_t		 sc_ecdev;
	struct acpi_devnode	*sc_node;
	ACPI_NOTIFY_HANDLER	 sc_handler;

	SIMPLEQ_HEAD(, wmi_t)	wmi_head;
};

#define UGET16(x)	(*(uint16_t *)(x))
#define UGET64(x)	(*(uint64_t *)(x))

#define HEXCHAR(x)	(((x) >= '0' && (x) <= '9') ||	\
			((x) >= 'a' && (x) <= 'f')  ||	\
			((x) >= 'A' && (x) <= 'F'))

#define GUIDCMP(a, b)					\
	((a)->data1 == (b)->data1 &&			\
	 (a)->data2 == (b)->data2 &&			\
	 (a)->data3 == (b)->data3 &&			\
	 UGET64((a)->data4) == UGET64((b)->data4))

#endif	/* !_SYS_DEV_ACPI_WMI_WMI_ACPIVAR_H */

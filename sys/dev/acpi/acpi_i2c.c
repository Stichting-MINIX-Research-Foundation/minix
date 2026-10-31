/* $NetBSD: acpi_i2c.c,v 1.4.8.1 2019/09/28 12:39:23 martin Exp $ */

/*-
 * Copyright (c) 2017 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Manuel Bouyer.
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
__KERNEL_RCSID(0, "$NetBSD: acpi_i2c.c,v 1.4.8.1 2019/09/28 12:39:23 martin Exp $");

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_i2c.h>

#define _COMPONENT	ACPI_BUS_COMPONENT
ACPI_MODULE_NAME	("acpi_i2c")

static void
acpi_enter_i2c_hid(struct acpi_devnode *devnode, prop_dictionary_t dev)
{
	ACPI_OBJECT_LIST arg;
	ACPI_OBJECT obj[4];
	ACPI_OBJECT *osc;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;
	/* 3cdff6f7-4267-4555-ad05-b30a3d8938de */
	static uint8_t i2c_hid_guid[] = {
		0xF7, 0xF6, 0xDF, 0x3C, 0x67, 0x42, 0x55, 0x45,
		0xAD, 0x05, 0xB3, 0x0A, 0x3D, 0x89, 0x38, 0xDE,
	};

	arg.Count = 4;
	arg.Pointer = obj;

	obj[0].Type = ACPI_TYPE_BUFFER;
	obj[0].Buffer.Length = sizeof(i2c_hid_guid);
	obj[0].Buffer.Pointer = i2c_hid_guid;

	/* rev */
	obj[1].Type = ACPI_TYPE_INTEGER;
	obj[1].Integer.Value = 1;

	/* func */
	obj[2].Type = ACPI_TYPE_INTEGER;
	obj[2].Integer.Value = 1;

	obj[3].Type = ACPI_TYPE_PACKAGE;
	obj[3].Buffer.Length = 0;

	buf.Pointer = NULL;
	buf.Length = ACPI_ALLOCATE_LOCAL_BUFFER;

	rv = AcpiEvaluateObject(devnode->ad_handle, "_DSM", &arg, &buf);

	if (ACPI_FAILURE(rv)) {
		aprint_error("failed to evaluate _DSM for %s: %s\n",
		    devnode->ad_name, AcpiFormatException(rv));
		return;
	}

	osc = buf.Pointer;
	if (osc->Type != ACPI_TYPE_INTEGER) {
		aprint_error("bad _DSM return type %d for %s\n",
		    osc->Type, devnode->ad_name);
		return;
	}
	prop_dictionary_set_uint32(dev, "hid-descr-addr", osc->Integer.Value);
}

struct acpi_i2c_id {
	const char *id;
	const char *compat;
	const int compatlen;
	void (*parse)(struct acpi_devnode *, prop_dictionary_t);
};

static const struct acpi_i2c_id acpi_i2c_ids[] = {
	{
		.id = "PNP0C50",
		.compat = "hid-over-i2c",
		.compatlen = 13,
		.parse = acpi_enter_i2c_hid
	},
	{
		.id = "ACPI0C50",
		.compat = "hid-over-i2c",
		.compatlen = 13,
		.parse = acpi_enter_i2c_hid
	},
	{
		.id = NULL,
		.compat = NULL,
		.compatlen = 0,
		.parse = NULL
	}
};

static const struct acpi_i2c_id *
acpi_i2c_search(const char *name)
{
	int i;
	for (i = 0; acpi_i2c_ids[i].id != NULL; i++) {
		if (strcmp(name, acpi_i2c_ids[i].id) == 0)
			return &acpi_i2c_ids[i];
	}
	return NULL;
}

struct acpi_i2c_context {
	uint16_t i2c_addr;
};

static ACPI_STATUS
acpi_i2c_resource_parse_callback(ACPI_RESOURCE *res, void *context)
{
	struct acpi_i2c_context *i2cc = context;

	switch (res->Type) {
	case ACPI_RESOURCE_TYPE_END_TAG:
		break;
	case ACPI_RESOURCE_TYPE_SERIAL_BUS:
		switch (res->Data.I2cSerialBus.Type) {
		case ACPI_RESOURCE_SERIAL_TYPE_I2C:
			i2cc->i2c_addr = res->Data.I2cSerialBus.SlaveAddress;
			break;
		}
		break;
	case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
		break;
	default:
		printf("ressource type 0x%x ignored\n", res->Type);
	}
	return_ACPI_STATUS(AE_OK);
}

static void
acpi_enter_i2c_device(struct acpi_devnode *ad, prop_array_t array)
{
	prop_dictionary_t dev;
	struct acpi_i2c_context i2cc;
	ACPI_STATUS rv;
	int cidi;
	ACPI_PNP_DEVICE_ID_LIST *idlist;
	const char *name;
	static const struct acpi_i2c_id *i2c_id;

	memset(&i2cc, 0, sizeof(i2cc));
	rv = AcpiWalkResources(ad->ad_handle, "_CRS",
	     acpi_i2c_resource_parse_callback, &i2cc);
	if (ACPI_FAILURE(rv)) {
		aprint_error("ACPI: unable to get resources "
		   "for %s: %s\n", ad->ad_name,
		   AcpiFormatException(rv));
		return;
	}
	if (i2cc.i2c_addr == 0)
		return;
	dev = prop_dictionary_create();
	if (dev == NULL) {
		aprint_error("ignoring device %s (no memory)\n",
		    ad->ad_name);
		return;
	}
	if ((ad->ad_devinfo->Valid &  ACPI_VALID_HID) == 0)
		name = ad->ad_name;
	else
		name = ad->ad_devinfo->HardwareId.String;
	prop_dictionary_set_cstring(dev, "name", name);
	prop_dictionary_set_uint32(dev, "addr", i2cc.i2c_addr);
	prop_dictionary_set_uint64(dev, "cookie", (uintptr_t)ad->ad_handle);
	/* first search by name, then by CID */
	i2c_id = acpi_i2c_search(name);
	idlist = &ad->ad_devinfo->CompatibleIdList;
	for (cidi = 0;
	    cidi < idlist->Count && i2c_id == NULL;
	    cidi++) {
		i2c_id = acpi_i2c_search(idlist->Ids[cidi].String);
	}
	if (i2c_id != NULL) {
		if (i2c_id->compat != NULL) {
			prop_data_t data;
			data = prop_data_create_data(i2c_id->compat,
			    i2c_id->compatlen);
			prop_dictionary_set(dev, "compatible", data);
			prop_object_release(data);
		}
		if (i2c_id->parse != NULL)
			i2c_id->parse(ad, dev);
	}
	prop_array_add(array, dev);
	prop_object_release(dev);
}


prop_array_t
acpi_enter_i2c_devs(struct acpi_devnode *devnode)
{
	struct acpi_devnode *ad;
	prop_array_t array = prop_array_create();

	if (array == NULL)
		return NULL;

	SIMPLEQ_FOREACH(ad, &devnode->ad_child_head, ad_child_list) {
		if (ad->ad_devinfo->Type != ACPI_TYPE_DEVICE)
			continue;
		if (!acpi_device_present(ad->ad_handle))
			continue;
		acpi_enter_i2c_device(ad, array);
	}
	return array;
}

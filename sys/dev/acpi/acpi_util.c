/*	$NetBSD: acpi_util.c,v 1.8 2011/06/21 03:37:21 jruoho Exp $ */

/*-
 * Copyright (c) 2003, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum of By Noon Software, Inc.
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

/*
 * Copyright 2001, 2003 Wasabi Systems, Inc.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpi_util.c,v 1.8 2011/06/21 03:37:21 jruoho Exp $");

#include <sys/param.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#define _COMPONENT	ACPI_BUS_COMPONENT
ACPI_MODULE_NAME	("acpi_util")

static void		acpi_clean_node(ACPI_HANDLE, void *);

static const char * const acpicpu_ids[] = {
	"ACPI0007",
	NULL
};

/*
 * Evaluate an integer object.
 */
ACPI_STATUS
acpi_eval_integer(ACPI_HANDLE handle, const char *path, ACPI_INTEGER *valp)
{
	ACPI_OBJECT obj;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;

	if (handle == NULL)
		handle = ACPI_ROOT_OBJECT;

	(void)memset(&obj, 0, sizeof(obj));
	buf.Pointer = &obj;
	buf.Length = sizeof(obj);

	rv = AcpiEvaluateObject(handle, path, NULL, &buf);

	if (ACPI_FAILURE(rv))
		return rv;

	/* Check that evaluation produced a return value. */
	if (buf.Length == 0)
		return AE_NULL_OBJECT;

	if (obj.Type != ACPI_TYPE_INTEGER)
		return AE_TYPE;

	if (valp != NULL)
		*valp = obj.Integer.Value;

	return AE_OK;
}

/*
 * Evaluate an integer object with a single integer input parameter.
 */
ACPI_STATUS
acpi_eval_set_integer(ACPI_HANDLE handle, const char *path, ACPI_INTEGER val)
{
	ACPI_OBJECT_LIST arg;
	ACPI_OBJECT obj;

	if (handle == NULL)
		handle = ACPI_ROOT_OBJECT;

	obj.Type = ACPI_TYPE_INTEGER;
	obj.Integer.Value = val;

	arg.Count = 1;
	arg.Pointer = &obj;

	return AcpiEvaluateObject(handle, path, &arg, NULL);
}

/*
 * Evaluate a (Unicode) string object.
 */
ACPI_STATUS
acpi_eval_string(ACPI_HANDLE handle, const char *path, char **stringp)
{
	ACPI_OBJECT *obj;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;

	rv = acpi_eval_struct(handle, path, &buf);

	if (ACPI_FAILURE(rv))
		return rv;

	obj = buf.Pointer;

	if (obj->Type != ACPI_TYPE_STRING) {
		rv = AE_TYPE;
		goto out;
	}

	if (obj->String.Length == 0) {
		rv = AE_BAD_DATA;
		goto out;
	}

	*stringp = ACPI_ALLOCATE(obj->String.Length + 1);

	if (*stringp == NULL) {
		rv = AE_NO_MEMORY;
		goto out;
	}

	(void)memcpy(*stringp, obj->String.Pointer, obj->String.Length);

	(*stringp)[obj->String.Length] = '\0';

out:
	ACPI_FREE(buf.Pointer);

	return rv;
}

/*
 * Evaluate a structure. Caller must free buf.Pointer by ACPI_FREE().
 */
ACPI_STATUS
acpi_eval_struct(ACPI_HANDLE handle, const char *path, ACPI_BUFFER *buf)
{

	if (handle == NULL)
		handle = ACPI_ROOT_OBJECT;

	buf->Pointer = NULL;
	buf->Length = ACPI_ALLOCATE_LOCAL_BUFFER;

	return AcpiEvaluateObject(handle, path, NULL, buf);
}

/*
 * Evaluate a reference handle from an element in a package.
 */
ACPI_STATUS
acpi_eval_reference_handle(ACPI_OBJECT *elm, ACPI_HANDLE *handle)
{

	if (elm == NULL || handle == NULL)
		return AE_BAD_PARAMETER;

	switch (elm->Type) {

	case ACPI_TYPE_ANY:
	case ACPI_TYPE_LOCAL_REFERENCE:

		if (elm->Reference.Handle == NULL)
			return AE_NULL_ENTRY;

		*handle = elm->Reference.Handle;

		return AE_OK;

	case ACPI_TYPE_STRING:
		return AcpiGetHandle(NULL, elm->String.Pointer, handle);

	default:
		return AE_TYPE;
	}
}

/*
 * Iterate over all objects in a package, and pass them all
 * to a function. If the called function returns non-AE_OK,
 * the iteration is stopped and that value is returned.
 */
ACPI_STATUS
acpi_foreach_package_object(ACPI_OBJECT *pkg,
    ACPI_STATUS (*func)(ACPI_OBJECT *, void *), void *arg)
{
	ACPI_STATUS rv = AE_OK;
	uint32_t i;

	if (pkg == NULL)
		return AE_BAD_PARAMETER;

	if (pkg->Type != ACPI_TYPE_PACKAGE)
		return AE_TYPE;

	for (i = 0; i < pkg->Package.Count; i++) {

		rv = (*func)(&pkg->Package.Elements[i], arg);

		if (ACPI_FAILURE(rv))
			break;
	}

	return rv;
}

/*
 * Fetch data info the specified (empty) ACPI buffer.
 * Caller must free buf.Pointer by ACPI_FREE().
 */
ACPI_STATUS
acpi_get(ACPI_HANDLE handle, ACPI_BUFFER *buf,
    ACPI_STATUS (*getit)(ACPI_HANDLE, ACPI_BUFFER *))
{

	buf->Pointer = NULL;
	buf->Length = ACPI_ALLOCATE_LOCAL_BUFFER;

	return (*getit)(handle, buf);
}

/*
 * Return a complete pathname from a handle.
 *
 * Note that the function uses static data storage;
 * if the data is needed for future use, it should be
 * copied before any subsequent calls overwrite it.
 */
const char *
acpi_name(ACPI_HANDLE handle)
{
	static char name[80];
	ACPI_BUFFER buf;
	ACPI_STATUS rv;

	if (handle == NULL)
		handle = ACPI_ROOT_OBJECT;

	buf.Pointer = name;
	buf.Length = sizeof(name);

	rv = AcpiGetName(handle, ACPI_FULL_PATHNAME, &buf);

	if (ACPI_FAILURE(rv))
		return "UNKNOWN";

	return name;
}

/*
 * Match given IDs against _HID and _CIDs.
 */
int
acpi_match_hid(ACPI_DEVICE_INFO *ad, const char * const *ids)
{
	uint32_t i, n;
	char *id;

	while (*ids) {

		if ((ad->Valid & ACPI_VALID_HID) != 0) {

			if (pmatch(ad->HardwareId.String, *ids, NULL) == 2)
				return 1;
		}

		if ((ad->Valid & ACPI_VALID_CID) != 0) {

			n = ad->CompatibleIdList.Count;

			for (i = 0; i < n; i++) {

				id = ad->CompatibleIdList.Ids[i].String;

				if (pmatch(id, *ids, NULL) == 2)
					return 1;
			}
		}

		ids++;
	}

	return 0;
}

/*
 * Match a device node from a handle.
 */
struct acpi_devnode *
acpi_match_node(ACPI_HANDLE handle)
{
	struct acpi_devnode *ad;
	ACPI_STATUS rv;

	if (handle == NULL)
		return NULL;

	rv = AcpiGetData(handle, acpi_clean_node, (void **)&ad);

	if (ACPI_FAILURE(rv))
		return NULL;

	return ad;
}

/*
 * Permanently associate a device node with a handle.
 */
void
acpi_match_node_init(struct acpi_devnode *ad)
{
	(void)AcpiAttachData(ad->ad_handle, acpi_clean_node, ad);
}

static void
acpi_clean_node(ACPI_HANDLE handle, void *aux)
{
	/* Nothing. */
}

/*
 * Match a handle from a cpu_info. Returns NULL on failure.
 *
 * Note that acpi_match_node() can be used if the device node
 * is also required.
 */
ACPI_HANDLE
acpi_match_cpu_info(struct cpu_info *ci)
{
	struct acpi_softc *sc = acpi_softc;
	struct acpi_devnode *ad;
	ACPI_INTEGER val;
	ACPI_OBJECT *obj;
	ACPI_BUFFER buf;
	ACPI_HANDLE hdl;
	ACPI_STATUS rv;

	if (sc == NULL || acpi_active == 0)
		return NULL;

	/*
	 * CPUs are declared in the ACPI namespace
	 * either as a Processor() or as a Device().
	 * In both cases the MADT entries are used
	 * for the match (see ACPI 4.0, section 8.4).
	 */
	SIMPLEQ_FOREACH(ad, &sc->ad_head, ad_list) {

		hdl = ad->ad_handle;

		switch (ad->ad_type) {

		case ACPI_TYPE_DEVICE:

			if (acpi_match_hid(ad->ad_devinfo, acpicpu_ids) == 0)
				break;

			rv = acpi_eval_integer(hdl, "_UID", &val);

			if (ACPI_SUCCESS(rv) && val == ci->ci_acpiid)
				return hdl;

			break;

		case ACPI_TYPE_PROCESSOR:

			rv = acpi_eval_struct(hdl, NULL, &buf);

			if (ACPI_FAILURE(rv))
				break;

			obj = buf.Pointer;

			if (obj->Processor.ProcId == ci->ci_acpiid) {
				ACPI_FREE(buf.Pointer);
				return hdl;
			}

			ACPI_FREE(buf.Pointer);
			break;
		}
	}

	return NULL;
}

/*
 * Match a CPU from a handle. Returns NULL on failure.
 */
struct cpu_info *
acpi_match_cpu_handle(ACPI_HANDLE hdl)
{
	struct cpu_info *ci;
	ACPI_DEVICE_INFO *di;
	CPU_INFO_ITERATOR cii;
	ACPI_INTEGER val;
	ACPI_OBJECT *obj;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;

	ci = NULL;
	di = NULL;
	buf.Pointer = NULL;

	rv = AcpiGetObjectInfo(hdl, &di);

	if (ACPI_FAILURE(rv))
		return NULL;

	switch (di->Type) {

	case ACPI_TYPE_DEVICE:

		if (acpi_match_hid(di, acpicpu_ids) == 0)
			goto out;

		rv = acpi_eval_integer(hdl, "_UID", &val);

		if (ACPI_FAILURE(rv))
			goto out;

		break;

	case ACPI_TYPE_PROCESSOR:

		rv = acpi_eval_struct(hdl, NULL, &buf);

		if (ACPI_FAILURE(rv))
			goto out;

		obj = buf.Pointer;
		val = obj->Processor.ProcId;
		break;

	default:
		goto out;
	}

	for (CPU_INFO_FOREACH(cii, ci)) {

		if (ci->ci_acpiid == val)
			goto out;
	}

	ci = NULL;

out:
	if (di != NULL)
		ACPI_FREE(di);

	if (buf.Pointer != NULL)
		ACPI_FREE(buf.Pointer);

	return ci;
}

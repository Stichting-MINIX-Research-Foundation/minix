/* $NetBSD: acpi_power.c,v 1.34 2014/02/25 18:30:09 pooka Exp $ */

/*-
 * Copyright (c) 2009, 2010, 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jukka Ruohonen.
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

/*-
 * Copyright (c) 2001 Michael Smith
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpi_power.c,v 1.34 2014/02/25 18:30:09 pooka Exp $");

#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_pci.h>
#include <dev/acpi/acpi_power.h>

#define _COMPONENT			ACPI_BUS_COMPONENT
ACPI_MODULE_NAME			("acpi_power")

#define	ACPI_STA_POW_OFF		0x00
#define	ACPI_STA_POW_ON			0x01

struct acpi_power_res {
	ACPI_HANDLE			res_handle;
	ACPI_INTEGER			res_level;
	ACPI_INTEGER			res_order;
	ACPI_HANDLE			res_ref[5];
	char				res_name[5];
	kmutex_t			res_mutex;

	TAILQ_ENTRY(acpi_power_res)	res_list;
};

static TAILQ_HEAD(, acpi_power_res) res_head =
	TAILQ_HEAD_INITIALIZER(res_head);

static int32_t acpi_power_acpinode = CTL_EOL;
static int32_t acpi_power_powernode = CTL_EOL;

static struct acpi_power_res	*acpi_power_res_init(ACPI_HANDLE);
static struct acpi_power_res	*acpi_power_res_get(ACPI_HANDLE);

static ACPI_STATUS	 acpi_power_get_direct(struct acpi_devnode *);
static ACPI_STATUS	 acpi_power_get_indirect(struct acpi_devnode *);
static ACPI_STATUS	 acpi_power_switch(struct acpi_devnode *,
						   int, bool);
static ACPI_STATUS	 acpi_power_res_ref(struct acpi_power_res *,
					    ACPI_HANDLE);
static ACPI_STATUS	 acpi_power_res_deref(struct acpi_power_res *,
					      ACPI_HANDLE);
static ACPI_STATUS	 acpi_power_res_sta(ACPI_OBJECT *, void *);

static ACPI_OBJECT	*acpi_power_pkg_get(ACPI_HANDLE, int);
static int		 acpi_power_sysctl(SYSCTLFN_PROTO);
static const char	*acpi_xname(ACPI_HANDLE);

static struct acpi_power_res *
acpi_power_res_init(ACPI_HANDLE hdl)
{
	struct acpi_power_res *tmp = NULL;
	struct acpi_power_res *res = NULL;
	ACPI_OBJECT *obj;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;
	size_t i;

	rv = acpi_eval_struct(hdl, NULL, &buf);

	if (ACPI_FAILURE(rv))
		goto out;

	obj = buf.Pointer;

	if (obj->Type != ACPI_TYPE_POWER) {
		rv = AE_TYPE;
		goto out;
	}

	res = kmem_zalloc(sizeof(*res), KM_SLEEP);

	if (res == NULL) {
		rv = AE_NO_MEMORY;
		goto out;
	}

	res->res_handle = hdl;
	res->res_level = obj->PowerResource.SystemLevel;
	res->res_order = obj->PowerResource.ResourceOrder;

	(void)strlcpy(res->res_name,
	    acpi_xname(hdl), sizeof(res->res_name));

	for (i = 0; i < __arraycount(res->res_ref); i++)
		res->res_ref[i] = NULL;

	mutex_init(&res->res_mutex, MUTEX_DEFAULT, IPL_NONE);

	/*
	 * Power resources should be ordered.
	 *
	 * These *should* be enabled from low values to high
	 * values and disabled from high values to low values.
	 */
	TAILQ_FOREACH(tmp, &res_head, res_list) {

		if (res->res_order < tmp->res_order) {
			TAILQ_INSERT_BEFORE(tmp, res, res_list);
			break;
		}
	}

	if (tmp == NULL)
		TAILQ_INSERT_TAIL(&res_head, res, res_list);

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "%s added to the "
		"power resource queue\n", res->res_name));

out:
	if (buf.Pointer != NULL)
		ACPI_FREE(buf.Pointer);

	return res;
}

static struct acpi_power_res *
acpi_power_res_get(ACPI_HANDLE hdl)
{
	struct acpi_power_res *res;

	TAILQ_FOREACH(res, &res_head, res_list) {

		if (res->res_handle == hdl)
			return res;
	}

	return acpi_power_res_init(hdl);
}

bool
acpi_power_register(ACPI_HANDLE hdl)
{
	return true;
}

void
acpi_power_deregister(ACPI_HANDLE hdl)
{
	struct acpi_devnode *ad = acpi_match_node(hdl);
	struct acpi_power_res *res;

	if (ad == NULL)
		return;

	/*
	 * Remove all references in each resource.
	 */
	TAILQ_FOREACH(res, &res_head, res_list)
	    (void)acpi_power_res_deref(res, ad->ad_handle);
}

/*
 * Get the D-state of an ACPI device node.
 */
bool
acpi_power_get(ACPI_HANDLE hdl, int *state)
{
	struct acpi_devnode *ad = acpi_match_node(hdl);
	ACPI_STATUS rv;

	if (ad == NULL)
		return false;

	/*
	 * As _PSC may be broken, first try to
	 * retrieve the power state indirectly
	 * via power resources.
	 */
	rv = acpi_power_get_indirect(ad);

	if (ACPI_FAILURE(rv))
		rv = acpi_power_get_direct(ad);

	if (ACPI_FAILURE(rv))
		goto fail;

	KASSERT(ad->ad_state != ACPI_STATE_ERROR);

	if (ad->ad_state < ACPI_STATE_D0 || ad->ad_state > ACPI_STATE_D3) {
		rv = AE_BAD_VALUE;
		goto fail;
	}

	if (state != NULL)
		*state = ad->ad_state;

	return true;

fail:
	ad->ad_state = ACPI_STATE_ERROR;

	if (state != NULL)
		*state = ad->ad_state;

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "failed to get power state "
		"for %s: %s\n", ad->ad_name, AcpiFormatException(rv)));

	return false;
}

static ACPI_STATUS
acpi_power_get_direct(struct acpi_devnode *ad)
{
	ACPI_INTEGER val = 0;
	ACPI_STATUS rv;

	rv = acpi_eval_integer(ad->ad_handle, "_PSC", &val);

	KDASSERT((uint64_t)val < INT_MAX);

	ad->ad_state = (int)val;

	return rv;
}

static ACPI_STATUS
acpi_power_get_indirect(struct acpi_devnode *ad)
{
	ACPI_OBJECT *pkg;
	ACPI_STATUS rv;
	int i;

	CTASSERT(ACPI_STATE_D0 == 0 && ACPI_STATE_D1 == 1);
	CTASSERT(ACPI_STATE_D2 == 2 && ACPI_STATE_D3 == 3);

	/*
	 * The device is in a given D-state if all resources are on.
	 * To derive this, evaluate all elements in each _PRx package
	 * (x = 0 ... 3) and break if the noted condition becomes true.
	 */
	for (ad->ad_state = ACPI_STATE_D3, i = 0; i < ACPI_STATE_D3; i++) {

		pkg = acpi_power_pkg_get(ad->ad_handle, i);

		if (pkg == NULL)
			continue;

		/*
		 * For each element in the _PRx package, evaluate _STA
		 * and return AE_OK only if all power resources are on.
		 */
		rv = acpi_foreach_package_object(pkg, acpi_power_res_sta, ad);

		if (ACPI_FAILURE(rv) && rv != AE_CTRL_FALSE)
			goto out;

		if (ACPI_SUCCESS(rv)) {
			ad->ad_state = i;
			goto out;
		}

		ACPI_FREE(pkg); pkg = NULL;
	}

	KASSERT(ad->ad_state == ACPI_STATE_D3);

	return AE_OK;

out:
	ACPI_FREE(pkg);

	return rv;
}

/*
 * Set the D-state of an ACPI device node.
 */
bool
acpi_power_set(ACPI_HANDLE hdl, int state)
{
	struct acpi_devnode *ad = acpi_match_node(hdl);
	ACPI_STATUS rv;
	char path[5];
	int old;

	if (ad == NULL)
		return false;

	if (state < ACPI_STATE_D0 || state > ACPI_STATE_D3) {
		rv = AE_BAD_PARAMETER;
		goto fail;
	}

	if (acpi_power_get(ad->ad_handle, &old) != true) {
		rv = AE_NOT_FOUND;
		goto fail;
	}

	KASSERT(ad->ad_state == old);
	KASSERT(ad->ad_state != ACPI_STATE_ERROR);

	if (ad->ad_state == state) {
		rv = AE_ALREADY_EXISTS;
		goto fail;
	}

	/*
	 * It is only possible to go to D0 ("on") from D3 ("off").
	 */
	if (ad->ad_state == ACPI_STATE_D3 && state != ACPI_STATE_D0) {
		rv = AE_BAD_PARAMETER;
		goto fail;
	}

	/*
	 * As noted in ACPI 4.0 (appendix A.2.1), the bus power state
	 * should never be lower than the highest state of one of its
	 * devices. Consequently, we cannot set the state to a lower
	 * (i.e. higher power) state than the parent device's state.
	 */
	if ((ad->ad_parent != NULL) &&
	    (ad->ad_parent->ad_flags & ACPI_DEVICE_POWER) != 0) {

		if (ad->ad_parent->ad_state > state) {
			rv = AE_ABORT_METHOD;
			goto fail;
		}
	}

	/*
	 * We first sweep through the resources required for the target
	 * state, turning things on and building references. After this
	 * we dereference the resources required for the current state,
	 * turning the resources off as we go.
	 */
	rv = acpi_power_switch(ad, state, true);

	if (ACPI_FAILURE(rv) && rv != AE_CTRL_CONTINUE)
		goto fail;

	rv = acpi_power_switch(ad, ad->ad_state, false);

	if (ACPI_FAILURE(rv) && rv != AE_CTRL_CONTINUE)
		goto fail;

	/*
	 * Last but not least, invoke the power state switch method,
	 * if available. Because some systems use only _PSx for the
	 * power state transitions, we do this even if there is no _PRx.
	 */
	(void)snprintf(path, sizeof(path), "_PS%d", state);
	(void)AcpiEvaluateObject(ad->ad_handle, path, NULL, NULL);

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "%s turned from "
		"D%d to D%d\n", ad->ad_name, old, state));

	ad->ad_state = state;

	return true;

fail:
	ad->ad_state = ACPI_STATE_ERROR;

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "failed to set power state to D%d "
		"for %s: %s\n", state, ad->ad_name, AcpiFormatException(rv)));

	return false;
}

static ACPI_STATUS
acpi_power_switch(struct acpi_devnode *ad, int state, bool on)
{
	ACPI_OBJECT *elm, *pkg;
	ACPI_STATUS rv = AE_OK;
	ACPI_HANDLE hdl;
	uint32_t i, n;

	/*
	 * For each element in the _PRx package, fetch
	 * the reference handle, search for this handle
	 * from the power resource queue, and turn the
	 * resource behind the handle on or off.
	 */
	pkg = acpi_power_pkg_get(ad->ad_handle, state);

	if (pkg == NULL)
		return AE_CTRL_CONTINUE;

	n = pkg->Package.Count;

	for (i = 0; i < n; i++) {

		elm = &pkg->Package.Elements[i];
		rv = acpi_eval_reference_handle(elm, &hdl);

		if (ACPI_FAILURE(rv))
			continue;

		(void)acpi_power_res(hdl, ad->ad_handle, on);
	}

	ACPI_FREE(pkg);

	return rv;
}

ACPI_STATUS
acpi_power_res(ACPI_HANDLE hdl, ACPI_HANDLE ref, bool on)
{
	struct acpi_power_res *res;
	const char *str;
	ACPI_STATUS rv;

	/*
	 * Search for the resource.
	 */
	res = acpi_power_res_get(hdl);

	if (res == NULL)
		return AE_NOT_FOUND;

	if (ref == NULL)
		return AE_BAD_PARAMETER;

	/*
	 * Adjust the reference counting. This is
	 * necessary since a single power resource
	 * can be shared by multiple devices.
	 */
	if (on) {
		rv = acpi_power_res_ref(res, ref);
		str = "_ON";
	} else {
		rv = acpi_power_res_deref(res, ref);
		str = "_OFF";
	}

	if (ACPI_FAILURE(rv))
		return rv;

	/*
	 * Turn the resource on or off.
	 */
	return AcpiEvaluateObject(res->res_handle, str, NULL, NULL);
}

static ACPI_STATUS
acpi_power_res_ref(struct acpi_power_res *res, ACPI_HANDLE ref)
{
	size_t i, j = SIZE_MAX;

	mutex_enter(&res->res_mutex);

	for (i = 0; i < __arraycount(res->res_ref); i++) {

		/*
		 * Do not error out if the handle
		 * has already been referenced.
		 */
		if (res->res_ref[i] == ref) {
			mutex_exit(&res->res_mutex);
			return AE_OK;
		}

		if (j == SIZE_MAX && res->res_ref[i] == NULL)
			j = i;
	}

	if (j == SIZE_MAX) {
		mutex_exit(&res->res_mutex);
		return AE_LIMIT;
	}

	res->res_ref[j] = ref;
	mutex_exit(&res->res_mutex);

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "%s referenced "
		"by %s\n", res->res_name, acpi_xname(ref)));

	return AE_OK;
}

static ACPI_STATUS
acpi_power_res_deref(struct acpi_power_res *res, ACPI_HANDLE ref)
{
	size_t i;

	mutex_enter(&res->res_mutex);

	for (i = 0; i < __arraycount(res->res_ref); i++) {

		if (res->res_ref[i] != ref)
			continue;

		res->res_ref[i] = NULL;
		mutex_exit(&res->res_mutex);

		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "%s dereferenced "
			"by %s\n", res->res_name, acpi_xname(ref)));

		return AE_OK;
	}

	/*
	 * If the array remains to be non-empty,
	 * something else is using the resource
	 * and hence it can not be turned off.
	 */
	mutex_exit(&res->res_mutex);

	return AE_ABORT_METHOD;
}

static ACPI_STATUS
acpi_power_res_sta(ACPI_OBJECT *elm, void *arg)
{
	ACPI_INTEGER val;
	ACPI_HANDLE hdl;
	ACPI_STATUS rv;

	rv = acpi_eval_reference_handle(elm, &hdl);

	if (ACPI_FAILURE(rv))
		goto fail;

	rv = acpi_eval_integer(hdl, "_STA", &val);

	if (ACPI_FAILURE(rv))
		goto fail;

	KDASSERT((uint64_t)val < INT_MAX);

	if ((int)val != ACPI_STA_POW_ON && (int)val != ACPI_STA_POW_OFF)
		return AE_BAD_VALUE;

	if ((int)val != ACPI_STA_POW_ON)
		return AE_CTRL_FALSE;		/* XXX: Not an error. */

	return AE_OK;

fail:
	if (rv == AE_CTRL_FALSE)
		rv = AE_ERROR;

	ACPI_DEBUG_PRINT((ACPI_DB_DEBUG_OBJECT, "failed to evaluate _STA "
		"for %s: %s\n", acpi_xname(hdl), AcpiFormatException(rv)));

	return rv;
}

static ACPI_OBJECT *
acpi_power_pkg_get(ACPI_HANDLE hdl, int state)
{
	char path[5] = "_PR?";
	ACPI_OBJECT *obj;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;

	path[3] = '0' + state;

	rv = acpi_eval_struct(hdl, path, &buf);

	if (ACPI_FAILURE(rv))
		goto fail;

	if (buf.Length == 0) {
		rv = AE_LIMIT;
		goto fail;
	}

	obj = buf.Pointer;

	if (obj->Type != ACPI_TYPE_PACKAGE) {
		rv = AE_TYPE;
		goto fail;
	}

	if (obj->Package.Count == 0) {
		rv = AE_LIMIT;
		goto fail;
	}

	return obj;

fail:
	if (buf.Pointer != NULL)
		ACPI_FREE(buf.Pointer);

	ACPI_DEBUG_PRINT((ACPI_DB_DEBUG_OBJECT, "failed to evaluate %s for "
		"%s: %s\n", path, acpi_xname(hdl), AcpiFormatException(rv)));

	return NULL;
}

SYSCTL_SETUP(sysctl_acpi_power_setup, "sysctl hw.acpi.power subtree setup")
{
	const struct sysctlnode *anode;
	int err;

	err = sysctl_createv(NULL, 0, NULL, &anode,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "acpi",
	    NULL, NULL, 0, NULL, 0,
	    CTL_HW, CTL_CREATE, CTL_EOL);

	if (err != 0)
		return;

	acpi_power_acpinode = anode->sysctl_num;

	err = sysctl_createv(NULL, 0, &anode, &anode,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE,
	    "power", SYSCTL_DESCR("ACPI device power states"),
	    NULL, 0, NULL, 0,
	    CTL_CREATE, CTL_EOL);

	if (err != 0)
		return;

	acpi_power_powernode = anode->sysctl_num;
}

void
acpi_power_add(struct acpi_devnode *ad)
{
	const char *str = NULL;
	device_t dev;
	int err;

	KASSERT(ad != NULL && ad->ad_root != NULL);
	KASSERT((ad->ad_flags & ACPI_DEVICE_POWER) != 0);

	if (acpi_power_acpinode == CTL_EOL ||
	    acpi_power_powernode == CTL_EOL)
		return;

	if (ad->ad_device != NULL)
		str = device_xname(ad->ad_device);
	else {
		dev = acpi_pcidev_find_dev(ad);

		if (dev != NULL)
			str = device_xname(dev);
	}

	if (str == NULL)
		return;

	err = sysctl_createv(NULL, 0, NULL, NULL,
	    CTLFLAG_READONLY, CTLTYPE_STRING, str,
	    NULL, acpi_power_sysctl, 0, (void *)ad, 0, CTL_HW,
	    acpi_power_acpinode, acpi_power_powernode,
	    CTL_CREATE, CTL_EOL);

	if (err != 0)
		aprint_error_dev(ad->ad_root, "sysctl_createv"
		    "(hw.acpi.power.%s) failed (err %d)\n", str, err);
}

static int
acpi_power_sysctl(SYSCTLFN_ARGS)
{
	struct acpi_devnode *ad;
	struct sysctlnode node;
	int err, state;
	char t[3];

	node = *rnode;
	ad = rnode->sysctl_data;

	if (acpi_power_get(ad->ad_handle, &state) != true)
		state = 0;

	(void)memset(t, '\0', sizeof(t));
	(void)snprintf(t, sizeof(t), "D%d", state);

	node.sysctl_data = &t;

	err = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (err || newp == NULL)
		return err;

	return 0;
}

/*
 * XXX: Move this to acpi_util.c by refactoring
 *	acpi_name() to optionally return a single name.
 */
static const char *
acpi_xname(ACPI_HANDLE hdl)
{
	static char str[5];
	ACPI_BUFFER buf;
	ACPI_STATUS rv;

	buf.Pointer = str;
	buf.Length = sizeof(str);

	rv = AcpiGetName(hdl, ACPI_SINGLE_NAME, &buf);

	if (ACPI_FAILURE(rv))
		return "????";

	return str;
}

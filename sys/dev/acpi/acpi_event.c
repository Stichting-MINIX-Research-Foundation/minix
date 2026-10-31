/* $NetBSD: acpi_event.c,v 1.1 2018/10/22 22:29:35 jmcneill Exp $ */

/*-
 * Copyright (c) 2018 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jared McNeill <jmcneill@invisible.ca>.
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
__KERNEL_RCSID(0, "$NetBSD: acpi_event.c,v 1.1 2018/10/22 22:29:35 jmcneill Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/kmem.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_event.h>

struct acpi_event {
	device_t	ev_dev;
	ACPI_HANDLE	ev_method;
	bool		ev_method_evt;
	UINT16		ev_data;
};

struct acpi_event_gpio_context {
	device_t	ctx_dev;
	ACPI_HANDLE	ctx_handle;
	void		(*ctx_func)(void *, struct acpi_event *, ACPI_RESOURCE_GPIO *);
	void		*ctx_arg;
};

static ACPI_STATUS
acpi_event_create(device_t dev, ACPI_HANDLE handle, UINT16 data, UINT8 trig, struct acpi_event **pev)
{
	struct acpi_event *ev;
	char namebuf[5];

	const char trigchar = trig == ACPI_LEVEL_SENSITIVE ? 'L' : 'E';

	ev = kmem_alloc(sizeof(*ev), KM_SLEEP);
	ev->ev_dev = dev;
	ev->ev_method = NULL;
	ev->ev_method_evt = false;
	ev->ev_data = data;

	if (data <= 255) {
		snprintf(namebuf, sizeof(namebuf), "_%c%02X", trigchar, data);
		AcpiGetHandle(handle, namebuf, &ev->ev_method);
	}
	if (ev->ev_method == NULL && ACPI_SUCCESS(AcpiGetHandle(handle, "_EVT", &ev->ev_method)))
		ev->ev_method_evt = true;

	if (ev->ev_method == NULL) {
		aprint_error_dev(dev, "%s is missing a control method for event %#x (trig %c)\n",
		    acpi_name(handle), data, trigchar);
		kmem_free(ev, sizeof(*ev));
		return AE_NOT_FOUND;
	}

	*pev = ev;

	return AE_OK;
}

static ACPI_STATUS
acpi_event_gpio_resource_cb(ACPI_RESOURCE *res, void *priv)
{
	struct acpi_event_gpio_context *ctx = priv;
	struct acpi_event *ev;
	ACPI_RESOURCE_GPIO *gpio;
	ACPI_STATUS rv;

	if (res->Type != ACPI_RESOURCE_TYPE_GPIO)
		return AE_OK;

	gpio = &res->Data.Gpio;
	if (gpio->ConnectionType != ACPI_RESOURCE_GPIO_TYPE_INT)
		return AE_OK;
	if (gpio->PinTableLength != 1)
		return AE_OK;

	rv = acpi_event_create(ctx->ctx_dev, ctx->ctx_handle, gpio->PinTable[0], gpio->Triggering, &ev);
	if (ACPI_SUCCESS(rv))
		ctx->ctx_func(ctx->ctx_arg, ev, gpio);

	return AE_OK;
}

ACPI_STATUS
acpi_event_create_gpio(device_t dev, ACPI_HANDLE handle,
    void (*func)(void *, struct acpi_event *, ACPI_RESOURCE_GPIO *), void *arg)
{
	struct acpi_event_gpio_context ctx;

	ctx.ctx_dev = dev;
	ctx.ctx_handle = handle;
	ctx.ctx_func = func;
	ctx.ctx_arg = arg;

	return AcpiWalkResources(handle, "_AEI", acpi_event_gpio_resource_cb, &ctx);
}

ACPI_STATUS
acpi_event_create_int(device_t dev, ACPI_HANDLE handle,
    void (*func)(void *, struct acpi_event *, struct acpi_irq *), void *arg)
{
	struct acpi_resources res;
	struct acpi_event *ev;
	struct acpi_irq *irq;
	ACPI_STATUS rv;
	int n;

	rv = acpi_resource_parse(dev, handle, "_CRS", &res,
            &acpi_resource_parse_ops_default);
	if (ACPI_FAILURE(rv))
		return rv;

	for (n = 0; (irq = acpi_res_irq(&res, n)) != NULL; n++) {
		rv = acpi_event_create(dev, handle, irq->ar_irq, irq->ar_type, &ev);
		if (ACPI_SUCCESS(rv))
			func(arg, ev, irq);
	}

	acpi_resource_cleanup(&res);

	return AE_OK;
}

static void
acpi_event_invoke(void *priv)
{
	struct acpi_event * const ev = priv;
	ACPI_OBJECT_LIST objs, *arg = NULL;
	ACPI_OBJECT obj[1];
	ACPI_STATUS rv;

	if (ev->ev_method_evt) {
		objs.Count = 1;
		objs.Pointer = obj;
		obj[0].Type = ACPI_TYPE_INTEGER;
		obj[0].Integer.Value = ev->ev_data;
		arg = &objs;
	}

	rv = AcpiEvaluateObject(ev->ev_method, NULL, arg, NULL);
	if (ACPI_FAILURE(rv))
		device_printf(ev->ev_dev, "failed to handle %s event: %s\n",
		    acpi_name(ev->ev_method), AcpiFormatException(rv));
}

ACPI_STATUS
acpi_event_notify(struct acpi_event *ev)
{
	return AcpiOsExecute(OSL_NOTIFY_HANDLER, acpi_event_invoke, ev);
}

/* $NetBSD: plgpio_acpi.c,v 1.5 2018/11/23 14:08:40 jmcneill Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: plgpio_acpi.c,v 1.5 2018/11/23 14:08:40 jmcneill Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/device.h>
#include <sys/gpio.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_intr.h>
#include <dev/acpi/acpi_event.h>

#include <dev/gpio/gpiovar.h>
#include <dev/ic/pl061var.h>
#include <dev/ic/pl061reg.h>

struct plgpio_acpi_softc;

struct plgpio_acpi_softc {
	struct plgpio_softc	sc_base;

	ACPI_HANDLE		sc_handle;

	struct acpi_event *	sc_event[8];
};

static int	plgpio_acpi_match(device_t, cfdata_t, void *);
static void	plgpio_acpi_attach(device_t, device_t, void *);

static void	plgpio_acpi_register_event(void *, struct acpi_event *, ACPI_RESOURCE_GPIO *);
static int	plgpio_acpi_intr(void *);

CFATTACH_DECL_NEW(plgpio_acpi, sizeof(struct plgpio_acpi_softc), plgpio_acpi_match, plgpio_acpi_attach, NULL, NULL);

static const char * const compatible[] = {
	"ARMH0061",
	NULL
};

static int
plgpio_acpi_match(device_t parent, cfdata_t cf, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, compatible);
}

static void
plgpio_acpi_attach(device_t parent, device_t self, void *aux)
{
	struct plgpio_acpi_softc * const asc = device_private(self);
	struct plgpio_softc * const sc = &asc->sc_base;
	struct acpi_attach_args *aa = aux;
	struct acpi_resources res;
	struct acpi_mem *mem;
	struct acpi_irq *irq;
	ACPI_STATUS rv;
	int error;
	void *ih;

	sc->sc_dev = self;
	asc->sc_handle = aa->aa_node->ad_handle;

	rv = acpi_resource_parse(sc->sc_dev, aa->aa_node->ad_handle, "_CRS",
	    &res, &acpi_resource_parse_ops_default);
	if (ACPI_FAILURE(rv))
		return;

	mem = acpi_res_mem(&res, 0);
	if (mem == NULL) {
		aprint_error_dev(self, "couldn't find mem resource\n");
		goto done;
	}

	irq = acpi_res_irq(&res, 0);
	if (irq == NULL) {
		aprint_error_dev(self, "couldn't find irq resource\n");
		goto done;
	}

	sc->sc_dev = self;
	sc->sc_bst = aa->aa_memt;
	error = bus_space_map(sc->sc_bst, mem->ar_base, mem->ar_length, 0, &sc->sc_bsh);
	if (error) {
		aprint_error_dev(self, "couldn't map registers\n");
		return;
	}

	plgpio_attach(sc);

	rv = acpi_event_create_gpio(self, asc->sc_handle, plgpio_acpi_register_event, asc);
	if (ACPI_FAILURE(rv)) {
		if (rv != AE_NOT_FOUND)
			aprint_error_dev(self, "failed to create events: %s\n", AcpiFormatException(rv));
		goto done;
	}

	ih = acpi_intr_establish(self, (uint64_t)asc->sc_handle,
	    IPL_VM, false, plgpio_acpi_intr, asc, device_xname(self));
	if (ih == NULL)
		aprint_error_dev(self, "couldn't establish interrupt\n");

done:
	acpi_resource_cleanup(&res);
}

static void
plgpio_acpi_register_event(void *priv, struct acpi_event *ev, ACPI_RESOURCE_GPIO *gpio)
{
	struct plgpio_acpi_softc * const asc = priv;
	struct plgpio_softc * const sc = &asc->sc_base;
	uint32_t ibe, iev, is, ie;

	const int pin = gpio->PinTable[0];

	if (pin >= __arraycount(asc->sc_event)) {
		aprint_error_dev(asc->sc_base.sc_dev,
		    "ignoring event for pin %u (out of range)\n", pin);
		return;
	}
	if (asc->sc_event[pin] != NULL) {
		aprint_error_dev(asc->sc_base.sc_dev,
		    "ignoring duplicate pin %u\n", pin);
		return;
	}

	asc->sc_event[pin] = ev;
	asc->sc_base.sc_reserved_mask |= __BIT(pin);

	/*
	 * Configure and enable interrupts for this pin.
	 */

	ibe = PLGPIO_READ(sc, PL061_GPIOIBE_REG);
	iev = PLGPIO_READ(sc, PL061_GPIOIEV_REG);
	switch (gpio->Polarity) {
	case ACPI_ACTIVE_HIGH:
		ibe &= ~__BIT(pin);
		iev |= __BIT(pin);
		break;
	case ACPI_ACTIVE_LOW:
		ibe &= ~__BIT(pin);
		iev &= ~__BIT(pin);
		break;
	case ACPI_ACTIVE_BOTH:
		ibe |= __BIT(pin);
		break;
	}
	PLGPIO_WRITE(sc, PL061_GPIOIBE_REG, ibe);
	PLGPIO_WRITE(sc, PL061_GPIOIEV_REG, iev);

	is = PLGPIO_READ(sc, PL061_GPIOIS_REG);
	switch (gpio->Triggering) {
	case ACPI_LEVEL_SENSITIVE:
		is |= __BIT(pin);
		break;
	case ACPI_EDGE_SENSITIVE:
		is &= ~__BIT(pin);
		break;
	}
	PLGPIO_WRITE(sc, PL061_GPIOIS_REG, is);

	delay(20);

	PLGPIO_WRITE(sc, PL061_GPIOIC_REG, __BIT(pin));

	ie = PLGPIO_READ(sc, PL061_GPIOIE_REG);
	ie |= __BIT(pin);
	PLGPIO_WRITE(sc, PL061_GPIOIE_REG, ie);
}

static int
plgpio_acpi_intr(void *priv)
{
	struct plgpio_acpi_softc * const asc = priv;
	struct plgpio_softc * const sc = &asc->sc_base;
	uint32_t mis;
	int bit;

	mis = PLGPIO_READ(sc, PL061_GPIOMIS_REG);
	PLGPIO_WRITE(sc, PL061_GPIOIC_REG, mis);

	while ((bit = __builtin_ffs(mis)) != 0) {
		const int pin = bit - 1;
		struct acpi_event * const ev = asc->sc_event[pin];
		KASSERT(ev != NULL);

		acpi_event_notify(ev);

		mis &= ~__BIT(pin);
	}

	return 1;
}

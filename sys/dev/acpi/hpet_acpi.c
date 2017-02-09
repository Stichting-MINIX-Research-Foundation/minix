/* $NetBSD: hpet_acpi.c,v 1.11 2011/06/15 09:02:38 jruoho Exp $ */

/*
 * Copyright (c) 2011 Jukka Ruohonen
 * Copyright (c) 2006 Nicolas Joly
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS
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
__KERNEL_RCSID(0, "$NetBSD: hpet_acpi.c,v 1.11 2011/06/15 09:02:38 jruoho Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/time.h>
#include <sys/timetc.h>

#include <dev/acpi/acpivar.h>

#include <dev/ic/hpetreg.h>
#include <dev/ic/hpetvar.h>

#define _COMPONENT	ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME	("acpi_hpet")

static int		hpet_acpi_tab_match(device_t, cfdata_t, void *);
static void		hpet_acpi_tab_attach(device_t, device_t, void *);
static bus_addr_t	hpet_acpi_tab_addr(void);
static int		hpet_acpi_dev_match(device_t, cfdata_t, void *);
static void		hpet_acpi_dev_attach(device_t, device_t, void *);
static bus_addr_t	hpet_acpi_dev_addr(device_t, void *, bus_size_t *);
static int		hpet_acpi_detach(device_t, int);

static const char * const hpet_acpi_ids[] = {
	"PNP0103",
        NULL
};

CFATTACH_DECL_NEW(hpet_acpi_tab, sizeof(struct hpet_softc),
    hpet_acpi_tab_match, hpet_acpi_tab_attach, hpet_acpi_detach, NULL);

CFATTACH_DECL_NEW(hpet_acpi_dev, sizeof(struct hpet_softc),
    hpet_acpi_dev_match, hpet_acpi_dev_attach, hpet_acpi_detach, NULL);

static int
hpet_acpi_tab_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;
	bus_space_handle_t bh;
	bus_space_tag_t bt;
	bus_addr_t addr;

	addr = hpet_acpi_tab_addr();

	if (addr == 0)
		return 0;

	bt = aa->aa_memt;

	if (bus_space_map(bt, addr, HPET_WINDOW_SIZE, 0, &bh) != 0)
		return 0;

	bus_space_unmap(bt, bh, HPET_WINDOW_SIZE);

	return 1;
}

static void
hpet_acpi_tab_attach(device_t parent, device_t self, void *aux)
{
	struct hpet_softc *sc = device_private(self);
	struct acpi_attach_args *aa = aux;
	bus_addr_t addr;

	sc->sc_mapped = false;
	addr = hpet_acpi_tab_addr();

	if (addr == 0) {
		aprint_error(": failed to get address\n");
		return;
	}

	sc->sc_memt = aa->aa_memt;
	sc->sc_mems = HPET_WINDOW_SIZE;

	if (bus_space_map(sc->sc_memt, addr, sc->sc_mems, 0, &sc->sc_memh)) {
		aprint_error(": failed to map mem space\n");
		return;
	}

	aprint_naive("\n");
	aprint_normal(": high precision event timer (mem 0x%08x-0x%08x)\n",
	    (uint32_t)addr, (uint32_t)addr + HPET_WINDOW_SIZE);

	sc->sc_mapped = true;
	hpet_attach_subr(self);
}

static bus_addr_t
hpet_acpi_tab_addr(void)
{
	ACPI_TABLE_HPET *hpet;
	ACPI_STATUS rv;

	rv = AcpiGetTable(ACPI_SIG_HPET, 1, (ACPI_TABLE_HEADER **)&hpet);

	if (ACPI_FAILURE(rv))
		return 0;

	if (hpet->Address.Address == 0)
		return 0;

	if (hpet->Address.SpaceId != ACPI_ADR_SPACE_SYSTEM_MEMORY)
		return 0;

	if (hpet->Address.Address == 0xfed0000000000000UL) /* A quirk. */
		hpet->Address.Address >>= 32;

	return hpet->Address.Address;
}

static int
hpet_acpi_dev_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;
	bus_space_handle_t bh;
	bus_space_tag_t bt;
	bus_size_t len = 0;
	bus_addr_t addr;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	if (acpi_match_hid(aa->aa_node->ad_devinfo, hpet_acpi_ids) == 0)
		return 0;

	addr = hpet_acpi_dev_addr(parent, aa, &len);

	if (addr == 0 || len == 0)
		return 0;

	bt = aa->aa_memt;

	if (bus_space_map(bt, addr, len, 0, &bh) == 0) {
		bus_space_unmap(bt, bh, len);
		return 1;
	}

	return 0;
}

static void
hpet_acpi_dev_attach(device_t parent, device_t self, void *aux)
{
	struct hpet_softc *sc = device_private(self);
	struct acpi_attach_args *aa = aux;
	bus_addr_t addr;

	sc->sc_mapped = false;
	addr = hpet_acpi_dev_addr(self, aa, &sc->sc_mems);

	if (addr == 0) {
		aprint_error(": failed to get address\n");
		return;
	}

	sc->sc_memt = aa->aa_memt;

	if (bus_space_map(sc->sc_memt, addr, sc->sc_mems, 0, &sc->sc_memh)) {
		aprint_error(": failed to map mem space\n");
		return;
	}

	aprint_naive("\n");
	aprint_normal(": high precision event timer (mem 0x%08x-0x%08x)\n",
	    (uint32_t)addr, (uint32_t)(addr + sc->sc_mems));

	sc->sc_mapped = true;
	hpet_attach_subr(self);
}

static bus_addr_t
hpet_acpi_dev_addr(device_t self, void *aux, bus_size_t *len)
{
	struct acpi_attach_args *aa = aux;
	struct acpi_resources res;
	struct acpi_mem *mem;
	bus_addr_t addr = 0;
	ACPI_STATUS rv;

	rv = acpi_resource_parse(self, aa->aa_node->ad_handle, "_CRS",
	    &res, &acpi_resource_parse_ops_quiet);

	if (ACPI_FAILURE(rv))
		return 0;

	mem = acpi_res_mem(&res, 0);

	if (mem == NULL)
		goto out;

	if (mem->ar_length < HPET_WINDOW_SIZE)
		goto out;

	addr = mem->ar_base;
	*len = mem->ar_length;

out:
	acpi_resource_cleanup(&res);

	return addr;
}

static int
hpet_acpi_detach(device_t self, int flags)
{
	struct hpet_softc *sc = device_private(self);
	int rv;

	if (sc->sc_mapped != true)
		return 0;

	rv = hpet_detach(self, flags);

	if (rv != 0)
		return rv;

	bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_mems);

	return 0;
}

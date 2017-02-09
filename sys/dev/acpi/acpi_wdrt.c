/* $NetBSD: acpi_wdrt.c,v 1.4 2015/04/23 23:23:00 pgoyette Exp $ */

/*
 * Copyright (c) 2011 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * ACPI "WDRT" watchdog support, based on:
 *
 * Watchdog Timer Hardware Requirements for Windows Server 2003, version 1.01
 * http://www.microsoft.com/whdc/system/sysinternals/watchdog.mspx
 */

/* #define ACPIWDRT_DEBUG */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpi_wdrt.c,v 1.4 2015/04/23 23:23:00 pgoyette Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/module.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#include <dev/sysmon/sysmonvar.h>

#define _COMPONENT	ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME	("acpi_wdrt")

#ifdef ACPIWDRT_DEBUG
#define DPRINTF(x)	printf x
#else
#define	DPRINTF(x)	do { } while (0)
#endif

/* Watchdog Control/Status Register (32 bits) */
#define	ACPI_WDRT_CSR_RUNSTOP		(1 << 0)	/* rw */
#define	 ACPI_WDRT_RUNSTOP_STOPPED	 (0 << 0)
#define	 ACPI_WDRT_RUNSTOP_RUNNING	 (1 << 0)
#define	ACPI_WDRT_CSR_FIRED		(1 << 1)	/* rw */
#define	ACPI_WDRT_CSR_ACTION		(1 << 2)	/* rw */
#define	 ACPI_WDRT_ACTION_RESET		 (0 << 2)
#define	 ACPI_WDRT_ACTION_POWEROFF	 (1 << 2)
#define	ACPI_WDRT_CSR_WDOGEN		(1 << 3)	/* ro */
#define	 ACPI_WDRT_WDOGEN_ENABLE	 (0 << 3)
#define	 ACPI_WDRT_WDOGEN_DISABLE	 (1 << 3)
#define	ACPI_WDRT_CSR_TRIGGER		(1 << 7)	/* wo */
/* Watchdog Count Register (32 bits) */
#define	ACPI_WDRT_COUNT_DATA_MASK	0xffff

/* Watchdog Resource Table - Units */
#define	ACPI_WDRT_UNITS_1S		0x0
#define	ACPI_WDRT_UNITS_100MS		0x1
#define	ACPI_WDRT_UNITS_10MS		0x2

struct acpi_wdrt_softc {
	device_t		sc_dev;

	bus_space_tag_t		sc_memt;
	bus_space_handle_t	sc_memh;

	struct sysmon_wdog	sc_smw;
	bool			sc_smw_valid;

	uint32_t		sc_max_period;
	unsigned int		sc_period_scale;

	ACPI_GENERIC_ADDRESS	sc_control_reg;
	ACPI_GENERIC_ADDRESS	sc_count_reg;
};

static int	acpi_wdrt_match(device_t, cfdata_t, void *);
static void	acpi_wdrt_attach(device_t, device_t, void *);
static int	acpi_wdrt_detach(device_t, int);
static bool	acpi_wdrt_suspend(device_t, const pmf_qual_t *);

static int	acpi_wdrt_setmode(struct sysmon_wdog *);
static int	acpi_wdrt_tickle(struct sysmon_wdog *);

static ACPI_STATUS acpi_wdrt_read_control(struct acpi_wdrt_softc *, uint64_t *);
static ACPI_STATUS acpi_wdrt_write_control(struct acpi_wdrt_softc *, uint64_t);
#if 0
static ACPI_STATUS acpi_wdrt_read_count(struct acpi_wdrt_softc *, uint32_t *);
#endif
static ACPI_STATUS acpi_wdrt_write_count(struct acpi_wdrt_softc *, uint32_t);

CFATTACH_DECL_NEW(
    acpiwdrt,
    sizeof(struct acpi_wdrt_softc),
    acpi_wdrt_match,
    acpi_wdrt_attach,
    acpi_wdrt_detach,
    NULL
);

static int
acpi_wdrt_match(device_t parent, cfdata_t match, void *opaque)
{
	ACPI_TABLE_WDRT *wdrt;
	ACPI_STATUS rv;
	uint64_t val;

	rv = AcpiGetTable(ACPI_SIG_WDRT, 1, (ACPI_TABLE_HEADER **)&wdrt);
	if (ACPI_FAILURE(rv))
		return 0;

	/* Only system memory address spaces are allowed */
	if (wdrt->ControlRegister.SpaceId != ACPI_ADR_SPACE_SYSTEM_MEMORY ||
	    wdrt->CountRegister.SpaceId != ACPI_ADR_SPACE_SYSTEM_MEMORY) {
		return 0;
	}
	/* Sanity check control & count register addresses */
	if (wdrt->ControlRegister.Address == 0 ||
	    wdrt->ControlRegister.Address == 0xffffffff ||
	    wdrt->ControlRegister.Address == 0xffffffffffffffff ||
	    wdrt->CountRegister.Address == 0 ||
	    wdrt->CountRegister.Address == 0xffffffff ||
	    wdrt->CountRegister.Address == 0xffffffffffffffff) {
		return 0;
	}

	/* Read control regster */
	rv = AcpiOsReadMemory(wdrt->ControlRegister.Address, &val,
	    wdrt->ControlRegister.BitWidth);
	if (ACPI_FAILURE(rv))
		return 0;

	/* Make sure the hardware watchdog is enabled */
	if ((val & ACPI_WDRT_CSR_WDOGEN) == ACPI_WDRT_WDOGEN_DISABLE)
		return 0;

	return 1;
}

static void
acpi_wdrt_attach(device_t parent, device_t self, void *opaque)
{
	struct acpi_wdrt_softc *sc = device_private(self);
	ACPI_TABLE_WDRT *wdrt;
	ACPI_STATUS rv;

	sc->sc_dev = self;

	pmf_device_register(self, acpi_wdrt_suspend, NULL);

	rv = AcpiGetTable(ACPI_SIG_WDRT, 1, (ACPI_TABLE_HEADER **)&wdrt);
	if (ACPI_FAILURE(rv)) {
		aprint_error(": couldn't get WDRT (%s)\n",
		    AcpiFormatException(rv));
		return;
	}

	/* Maximum counter value must be 511 - 65535 */
	if (wdrt->MaxCount < 511) {
		aprint_error(": maximum counter value out of range (%d)\n",
		    wdrt->MaxCount);
		return;
	}
	/* Counter units can be 1s, 100ms, or 10ms */
	switch (wdrt->Units) {
	case ACPI_WDRT_UNITS_1S:
	case ACPI_WDRT_UNITS_100MS:
	case ACPI_WDRT_UNITS_10MS:
		break;
	default:
		aprint_error(": units not supported (0x%x)\n", wdrt->Units);
		return;
	}

	sc->sc_control_reg = wdrt->ControlRegister;
	sc->sc_count_reg = wdrt->CountRegister;

	aprint_naive("\n");
	aprint_normal(": mem 0x%" PRIx64 ",0x%" PRIx64 "\n",
	    sc->sc_control_reg.Address, sc->sc_count_reg.Address);

	if (wdrt->PciVendorId != 0xffff && wdrt->PciDeviceId != 0xffff) {
		aprint_verbose_dev(sc->sc_dev, "PCI %u:%03u:%02u:%01u",
		    wdrt->PciSegment, wdrt->PciBus, wdrt->PciDevice,
		    wdrt->PciFunction);
		aprint_verbose(" vendor 0x%04x product 0x%04x\n",
		    wdrt->PciVendorId, wdrt->PciDeviceId);
	}

	sc->sc_max_period = wdrt->MaxCount;
	sc->sc_period_scale = 1;
	if (wdrt->Units == ACPI_WDRT_UNITS_100MS)
		sc->sc_period_scale = 10;
	if (wdrt->Units == ACPI_WDRT_UNITS_10MS)
		sc->sc_period_scale = 100;
	sc->sc_max_period /= sc->sc_period_scale;
	aprint_normal_dev(self, "watchdog interval 1-%d sec.\n",
	    sc->sc_max_period);

	sc->sc_smw.smw_name = device_xname(self);
	sc->sc_smw.smw_cookie = sc;
	sc->sc_smw.smw_setmode = acpi_wdrt_setmode;
	sc->sc_smw.smw_tickle = acpi_wdrt_tickle;
	sc->sc_smw.smw_period = sc->sc_max_period;

	if (sysmon_wdog_register(&sc->sc_smw))
		aprint_error_dev(self, "couldn't register with sysmon\n");
	else
		sc->sc_smw_valid = true;
}

static int
acpi_wdrt_detach(device_t self, int flags)
{
	struct acpi_wdrt_softc *sc = device_private(self);

	/* Don't allow detach if watchdog is armed */
	if (sc->sc_smw_valid &&
	    (sc->sc_smw.smw_mode & WDOG_MODE_MASK) != WDOG_MODE_DISARMED)
		return EBUSY;

	if (sc->sc_smw_valid) {
		sysmon_wdog_unregister(&sc->sc_smw);
		sc->sc_smw_valid = false;
	}

	pmf_device_deregister(self);

	return 0;
}

static bool
acpi_wdrt_suspend(device_t self, const pmf_qual_t *qual)
{
	struct acpi_wdrt_softc *sc = device_private(self);

	if (sc->sc_smw_valid == false)
		return true;

	/* Don't allow suspend if watchdog is armed */
	if ((sc->sc_smw.smw_mode & WDOG_MODE_MASK) != WDOG_MODE_DISARMED)
		return false;

	return true;
}

static int
acpi_wdrt_setmode(struct sysmon_wdog *smw)
{
	struct acpi_wdrt_softc *sc = smw->smw_cookie;
	uint64_t val;

	DPRINTF(("%s: %s mode 0x%x period %u\n", device_xname(sc->sc_dev),
	    __func__, smw->smw_mode, smw->smw_period));

	switch (smw->smw_mode & WDOG_MODE_MASK) {
	case WDOG_MODE_DISARMED:
		/* Disable watchdog timer */
		if (ACPI_FAILURE(acpi_wdrt_read_control(sc, &val)))
			goto failed;
		val &= ~ACPI_WDRT_CSR_RUNSTOP;
		val |= ACPI_WDRT_RUNSTOP_STOPPED;
		if (ACPI_FAILURE(acpi_wdrt_write_control(sc, val)))
			goto failed;
		break;
	default:
		if (smw->smw_period == 0)
			return EINVAL;
		if (smw->smw_period > sc->sc_max_period)
			smw->smw_period = sc->sc_max_period;

		/* Enable watchdog timer */
		if (ACPI_FAILURE(acpi_wdrt_read_control(sc, &val)))
			goto failed;
		val &= ~ACPI_WDRT_CSR_RUNSTOP;
		val &= ~ACPI_WDRT_CSR_ACTION;
		val |= ACPI_WDRT_ACTION_RESET;
		if (ACPI_FAILURE(acpi_wdrt_write_control(sc, val)))
			goto failed;
		/* Reset count register */
		if (acpi_wdrt_tickle(smw))
			goto failed;
		val |= ACPI_WDRT_RUNSTOP_RUNNING;
		if (ACPI_FAILURE(acpi_wdrt_write_control(sc, val)))
			goto failed;
		break;
	}

	return 0;

failed:
	return EIO;
}

static int
acpi_wdrt_tickle(struct sysmon_wdog *smw)
{
	struct acpi_wdrt_softc *sc = smw->smw_cookie;
	uint64_t val;
	
	DPRINTF(("%s: %s mode 0x%x period %u\n", device_xname(sc->sc_dev),
	    __func__, smw->smw_mode, smw->smw_period));

	/* Reset count register */
	val = smw->smw_period * sc->sc_period_scale;
	if (ACPI_FAILURE(acpi_wdrt_write_count(sc, val)))
		return EIO;

	/* Trigger new count interval */
	if (ACPI_FAILURE(acpi_wdrt_read_control(sc, &val)))
		return EIO;
	val |= ACPI_WDRT_CSR_TRIGGER;
	if (ACPI_FAILURE(acpi_wdrt_write_control(sc, val)))
		return EIO;

	return 0;
}

static ACPI_STATUS
acpi_wdrt_read_control(struct acpi_wdrt_softc *sc, uint64_t *val)
{
	ACPI_STATUS rv;

	KASSERT(sc->sc_smw_valid == true);

	rv = AcpiOsReadMemory(sc->sc_control_reg.Address,
	    val, sc->sc_control_reg.BitWidth);

	DPRINTF(("%s: %s 0x%" PRIx64 "/%u 0x%08x (%u)\n",
	    device_xname(sc->sc_dev),
	    __func__, sc->sc_control_reg.Address, sc->sc_control_reg.BitWidth,
	    *val, rv));

	return rv;
}

static ACPI_STATUS
acpi_wdrt_write_control(struct acpi_wdrt_softc *sc, uint64_t val)
{
	ACPI_STATUS rv;

	KASSERT(sc->sc_smw_valid == true);

	rv = AcpiOsWriteMemory(sc->sc_control_reg.Address,
	    val, sc->sc_control_reg.BitWidth);

	DPRINTF(("%s: %s 0x%" PRIx64 "/%u 0x%08x (%u)\n",
	    device_xname(sc->sc_dev),
	    __func__, sc->sc_control_reg.Address, sc->sc_control_reg.BitWidth,
	    val, rv));

	return rv;
}

#if 0
static ACPI_STATUS
acpi_wdrt_read_count(struct acpi_wdrt_softc *sc, uint32_t *val)
{
	ACPI_STATUS rv;

	KASSERT(sc->sc_smw_valid == true);

	rv = AcpiOsReadMemory(sc->sc_count_reg.Address,
	    val, sc->sc_count_reg.BitWidth);

	DPRINTF(("%s: %s 0x%" PRIx64 "/%u 0x%08x (%u)\n",
	    device_xname(sc->sc_dev),
	    __func__, sc->sc_count_reg.Address, sc->sc_count_reg.BitWidth,
	    *val, rv));

	return rv;
}
#endif

static ACPI_STATUS
acpi_wdrt_write_count(struct acpi_wdrt_softc *sc, uint32_t val)
{
	ACPI_STATUS rv;

	KASSERT(sc->sc_smw_valid == true);

	rv = AcpiOsWriteMemory(sc->sc_count_reg.Address,
	    val, sc->sc_count_reg.BitWidth);

	DPRINTF(("%s: %s 0x%" PRIx64 "/%u 0x%08x (%u)\n",
	    device_xname(sc->sc_dev),
	    __func__, sc->sc_count_reg.Address, sc->sc_count_reg.BitWidth,
	    val, rv));

	return rv;
}

MODULE(MODULE_CLASS_DRIVER, acpiwdrt, "sysmon_wdog");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
acpiwdrt_modcmd(modcmd_t cmd, void *opaque)
{
	int error = 0;

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_acpiwdrt,
		    cfattach_ioconf_acpiwdrt, cfdata_ioconf_acpiwdrt);
#endif
		return error;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		error = config_fini_component(cfdriver_ioconf_acpiwdrt,
		    cfattach_ioconf_acpiwdrt, cfdata_ioconf_acpiwdrt);
#endif
		return error;
	default:
		return ENOTTY;
	}
}

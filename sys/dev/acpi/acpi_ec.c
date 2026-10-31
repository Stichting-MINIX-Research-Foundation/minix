/*	$NetBSD: acpi_ec.c,v 1.75.20.4 2020/06/20 15:52:50 martin Exp $	*/

/*-
 * Copyright (c) 2007 Joerg Sonnenberger <joerg@NetBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * The ACPI Embedded Controller (EC) driver serves two different purposes:
 * - read and write access from ASL, e.g. to read battery state
 * - notification of ASL of System Control Interrupts.
 *
 * Access to the EC is serialised by sc_access_mtx and optionally the
 * ACPI global mutex.  Both locks are held until the request is fulfilled.
 * All access to the softc has to hold sc_mtx to serialise against the GPE
 * handler and the callout.  sc_mtx is also used for wakeup conditions.
 *
 * SCIs are processed in a kernel thread. Handling gets a bit complicated
 * by the lock order (sc_mtx must be acquired after sc_access_mtx and the
 * ACPI global mutex).
 *
 * Read and write requests spin around for a short time as many requests
 * can be handled instantly by the EC.  During normal processing interrupt
 * mode is used exclusively.  At boot and resume time interrupts are not
 * working and the handlers just busy loop.
 *
 * A callout is scheduled to compensate for missing interrupts on some
 * hardware.  If the EC doesn't process a request for 5s, it is most likely
 * in a wedged state.  No method to reset the EC is currently known.
 *
 * Special care has to be taken to not poll the EC in a busy loop without
 * delay.  This can prevent processing of Power Button events. At least some
 * Lenovo Thinkpads seem to be implement the Power Button Override in the EC
 * and the only option to recover on those models is to cut off all power.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpi_ec.c,v 1.75.20.4 2020/06/20 15:52:50 martin Exp $");

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/condvar.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/mutex.h>
#include <sys/systm.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_ecvar.h>

#define _COMPONENT          ACPI_EC_COMPONENT
ACPI_MODULE_NAME            ("acpi_ec")

/* Maximum time to wait for global ACPI lock in ms */
#define	EC_LOCK_TIMEOUT		5

/* Maximum time to poll for completion of a command  in ms */
#define	EC_POLL_TIMEOUT		5

/* Maximum time to give a single EC command in s */
#define EC_CMD_TIMEOUT		10

/* From ACPI 3.0b, chapter 12.3 */
#define EC_COMMAND_READ		0x80
#define	EC_COMMAND_WRITE	0x81
#define	EC_COMMAND_BURST_EN	0x82
#define	EC_COMMAND_BURST_DIS	0x83
#define	EC_COMMAND_QUERY	0x84

/* From ACPI 3.0b, chapter 12.2.1 */
#define	EC_STATUS_OBF		0x01
#define	EC_STATUS_IBF		0x02
#define	EC_STATUS_CMD		0x08
#define	EC_STATUS_BURST		0x10
#define	EC_STATUS_SCI		0x20
#define	EC_STATUS_SMI		0x40

static const char *ec_hid[] = {
	"PNP0C09",
	NULL,
};

enum ec_state_t {
	EC_STATE_QUERY,
	EC_STATE_QUERY_VAL,
	EC_STATE_READ,
	EC_STATE_READ_ADDR,
	EC_STATE_READ_VAL,
	EC_STATE_WRITE,
	EC_STATE_WRITE_ADDR,
	EC_STATE_WRITE_VAL,
	EC_STATE_FREE
};

struct acpiec_softc {
	ACPI_HANDLE sc_ech;

	ACPI_HANDLE sc_gpeh;
	uint8_t sc_gpebit;

	bus_space_tag_t sc_data_st;
	bus_space_handle_t sc_data_sh;

	bus_space_tag_t sc_csr_st;
	bus_space_handle_t sc_csr_sh;

	bool sc_need_global_lock;
	uint32_t sc_global_lock;

	kmutex_t sc_mtx, sc_access_mtx;
	kcondvar_t sc_cv, sc_cv_sci;
	enum ec_state_t sc_state;
	bool sc_got_sci;
	callout_t sc_pseudo_intr;

	uint8_t sc_cur_addr, sc_cur_val;
};

static int acpiecdt_match(device_t, cfdata_t, void *);
static void acpiecdt_attach(device_t, device_t, void *);

static int acpiec_match(device_t, cfdata_t, void *);
static void acpiec_attach(device_t, device_t, void *);

static void acpiec_common_attach(device_t, device_t, ACPI_HANDLE,
    bus_space_tag_t, bus_addr_t, bus_space_tag_t, bus_addr_t,
    ACPI_HANDLE, uint8_t);

static bool acpiec_suspend(device_t, const pmf_qual_t *);
static bool acpiec_resume(device_t, const pmf_qual_t *);
static bool acpiec_shutdown(device_t, int);

static bool acpiec_parse_gpe_package(device_t, ACPI_HANDLE,
    ACPI_HANDLE *, uint8_t *);

static void acpiec_callout(void *);
static void acpiec_gpe_query(void *);
static uint32_t acpiec_gpe_handler(ACPI_HANDLE, uint32_t, void *);
static ACPI_STATUS acpiec_space_setup(ACPI_HANDLE, uint32_t, void *, void **);
static ACPI_STATUS acpiec_space_handler(uint32_t, ACPI_PHYSICAL_ADDRESS,
    uint32_t, ACPI_INTEGER *, void *, void *);

static void acpiec_gpe_state_machine(device_t);

CFATTACH_DECL_NEW(acpiec, sizeof(struct acpiec_softc),
    acpiec_match, acpiec_attach, NULL, NULL);

CFATTACH_DECL_NEW(acpiecdt, sizeof(struct acpiec_softc),
    acpiecdt_match, acpiecdt_attach, NULL, NULL);

static device_t ec_singleton = NULL;
static bool acpiec_cold = false;

static bool
acpiecdt_find(device_t parent, ACPI_HANDLE *ec_handle,
    bus_addr_t *cmd_reg, bus_addr_t *data_reg, uint8_t *gpebit)
{
	ACPI_TABLE_ECDT *ecdt;
	ACPI_STATUS rv;

	rv = AcpiGetTable(ACPI_SIG_ECDT, 1, (ACPI_TABLE_HEADER **)&ecdt);
	if (ACPI_FAILURE(rv))
		return false;

	if (ecdt->Control.BitWidth != 8 || ecdt->Data.BitWidth != 8) {
		aprint_error_dev(parent,
		    "ECDT register width invalid (%u/%u)\n",
		    ecdt->Control.BitWidth, ecdt->Data.BitWidth);
		return false;
	}

	rv = AcpiGetHandle(ACPI_ROOT_OBJECT, ecdt->Id, ec_handle);
	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(parent,
		    "failed to look up EC object %s: %s\n",
		    ecdt->Id, AcpiFormatException(rv));
		return false;
	}

	*cmd_reg = ecdt->Control.Address;
	*data_reg = ecdt->Data.Address;
	*gpebit = ecdt->Gpe;

	return true;
}

static int
acpiecdt_match(device_t parent, cfdata_t match, void *aux)
{
	ACPI_HANDLE ec_handle;
	bus_addr_t cmd_reg, data_reg;
	uint8_t gpebit;

	if (acpiecdt_find(parent, &ec_handle, &cmd_reg, &data_reg, &gpebit))
		return 1;
	else
		return 0;
}

static void
acpiecdt_attach(device_t parent, device_t self, void *aux)
{
	struct acpibus_attach_args *aa = aux;
	ACPI_HANDLE ec_handle;
	bus_addr_t cmd_reg, data_reg;
	uint8_t gpebit;

	if (!acpiecdt_find(parent, &ec_handle, &cmd_reg, &data_reg, &gpebit))
		panic("ECDT disappeared");

	aprint_naive("\n");
	aprint_normal(": ACPI Embedded Controller via ECDT\n");

	acpiec_common_attach(parent, self, ec_handle, aa->aa_iot, cmd_reg,
	    aa->aa_iot, data_reg, NULL, gpebit);
}

static int
acpiec_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, ec_hid);
}

static void
acpiec_attach(device_t parent, device_t self, void *aux)
{
	struct acpi_attach_args *aa = aux;
	struct acpi_resources ec_res;
	struct acpi_io *io0, *io1;
	ACPI_HANDLE gpe_handle;
	uint8_t gpebit;
	ACPI_STATUS rv;

	if (ec_singleton != NULL) {
		aprint_naive(": using %s\n", device_xname(ec_singleton));
		aprint_normal(": using %s\n", device_xname(ec_singleton));
		goto fail0;
	}

	if (!acpi_device_present(aa->aa_node->ad_handle)) {
		aprint_normal(": not present\n");
		goto fail0;
	}

	if (!acpiec_parse_gpe_package(self, aa->aa_node->ad_handle,
				      &gpe_handle, &gpebit))
		goto fail0;

	rv = acpi_resource_parse(self, aa->aa_node->ad_handle, "_CRS",
	    &ec_res, &acpi_resource_parse_ops_default);
	if (rv != AE_OK) {
		aprint_error_dev(self, "resource parsing failed: %s\n",
		    AcpiFormatException(rv));
		goto fail0;
	}

	if ((io0 = acpi_res_io(&ec_res, 0)) == NULL) {
		aprint_error_dev(self, "no data register resource\n");
		goto fail1;
	}
	if ((io1 = acpi_res_io(&ec_res, 1)) == NULL) {
		aprint_error_dev(self, "no CSR register resource\n");
		goto fail1;
	}

	acpiec_common_attach(parent, self, aa->aa_node->ad_handle,
	    aa->aa_iot, io1->ar_base, aa->aa_iot, io0->ar_base,
	    gpe_handle, gpebit);

	acpi_resource_cleanup(&ec_res);
	return;

fail1:	acpi_resource_cleanup(&ec_res);
fail0:	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static void
acpiec_common_attach(device_t parent, device_t self,
    ACPI_HANDLE ec_handle, bus_space_tag_t cmdt, bus_addr_t cmd_reg,
    bus_space_tag_t datat, bus_addr_t data_reg,
    ACPI_HANDLE gpe_handle, uint8_t gpebit)
{
	struct acpiec_softc *sc = device_private(self);
	ACPI_STATUS rv;
	ACPI_INTEGER val;

	sc->sc_csr_st = cmdt;
	sc->sc_data_st = datat;

	sc->sc_ech = ec_handle;
	sc->sc_gpeh = gpe_handle;
	sc->sc_gpebit = gpebit;

	sc->sc_state = EC_STATE_FREE;
	mutex_init(&sc->sc_mtx, MUTEX_DRIVER, IPL_TTY);
	mutex_init(&sc->sc_access_mtx, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&sc->sc_cv, "eccv");
	cv_init(&sc->sc_cv_sci, "ecsci");

	if (bus_space_map(sc->sc_data_st, data_reg, 1, 0,
	    &sc->sc_data_sh) != 0) {
		aprint_error_dev(self, "unable to map data register\n");
		return;
	}

	if (bus_space_map(sc->sc_csr_st, cmd_reg, 1, 0, &sc->sc_csr_sh) != 0) {
		aprint_error_dev(self, "unable to map CSR register\n");
		goto post_data_map;
	}

	rv = acpi_eval_integer(sc->sc_ech, "_GLK", &val);
	if (rv == AE_OK) {
		sc->sc_need_global_lock = val != 0;
	} else if (rv != AE_NOT_FOUND) {
		aprint_error_dev(self, "unable to evaluate _GLK: %s\n",
		    AcpiFormatException(rv));
		goto post_csr_map;
	} else {
		sc->sc_need_global_lock = false;
	}
	if (sc->sc_need_global_lock)
		aprint_normal_dev(self, "using global ACPI lock\n");

	callout_init(&sc->sc_pseudo_intr, CALLOUT_MPSAFE);
	callout_setfunc(&sc->sc_pseudo_intr, acpiec_callout, self);

	rv = AcpiInstallAddressSpaceHandler(sc->sc_ech, ACPI_ADR_SPACE_EC,
	    acpiec_space_handler, acpiec_space_setup, self);
	if (rv != AE_OK) {
		aprint_error_dev(self,
		    "unable to install address space handler: %s\n",
		    AcpiFormatException(rv));
		goto post_csr_map;
	}

	rv = AcpiInstallGpeHandler(sc->sc_gpeh, sc->sc_gpebit,
	    ACPI_GPE_EDGE_TRIGGERED, acpiec_gpe_handler, self);
	if (rv != AE_OK) {
		aprint_error_dev(self, "unable to install GPE handler: %s\n",
		    AcpiFormatException(rv));
		goto post_csr_map;
	}

	rv = AcpiEnableGpe(sc->sc_gpeh, sc->sc_gpebit);
	if (rv != AE_OK) {
		aprint_error_dev(self, "unable to enable GPE: %s\n",
		    AcpiFormatException(rv));
		goto post_csr_map;
	}

	if (kthread_create(PRI_NONE, KTHREAD_MPSAFE, NULL, acpiec_gpe_query,
		           self, NULL, "acpiec sci thread")) {
		aprint_error_dev(self, "unable to create query kthread\n");
		goto post_csr_map;
	}

	ec_singleton = self;

	if (!pmf_device_register1(self, acpiec_suspend, acpiec_resume,
	    acpiec_shutdown))
		aprint_error_dev(self, "couldn't establish power handler\n");

	return;

post_csr_map:
	(void)AcpiRemoveGpeHandler(sc->sc_gpeh, sc->sc_gpebit,
	    acpiec_gpe_handler);
	(void)AcpiRemoveAddressSpaceHandler(sc->sc_ech,
	    ACPI_ADR_SPACE_EC, acpiec_space_handler);
	bus_space_unmap(sc->sc_csr_st, sc->sc_csr_sh, 1);
post_data_map:
	bus_space_unmap(sc->sc_data_st, sc->sc_data_sh, 1);
	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static bool
acpiec_suspend(device_t dv, const pmf_qual_t *qual)
{

	acpiec_cold = true;

	return true;
}

static bool
acpiec_resume(device_t dv, const pmf_qual_t *qual)
{

	acpiec_cold = false;

	return true;
}

static bool
acpiec_shutdown(device_t dv, int how)
{

	acpiec_cold = true;
	return true;
}

static bool
acpiec_parse_gpe_package(device_t self, ACPI_HANDLE ec_handle,
    ACPI_HANDLE *gpe_handle, uint8_t *gpebit)
{
	ACPI_BUFFER buf;
	ACPI_OBJECT *p, *c;
	ACPI_STATUS rv;

	rv = acpi_eval_struct(ec_handle, "_GPE", &buf);
	if (rv != AE_OK) {
		aprint_error_dev(self, "unable to evaluate _GPE: %s\n",
		    AcpiFormatException(rv));
		return false;
	}

	p = buf.Pointer;

	if (p->Type == ACPI_TYPE_INTEGER) {
		*gpe_handle = NULL;
		*gpebit = p->Integer.Value;
		ACPI_FREE(p);
		return true;
	}

	if (p->Type != ACPI_TYPE_PACKAGE) {
		aprint_error_dev(self, "_GPE is neither integer nor package\n");
		ACPI_FREE(p);
		return false;
	}

	if (p->Package.Count != 2) {
		aprint_error_dev(self,
		    "_GPE package does not contain 2 elements\n");
		ACPI_FREE(p);
		return false;
	}

	c = &p->Package.Elements[0];
	rv = acpi_eval_reference_handle(c, gpe_handle);

	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(self, "failed to evaluate _GPE handle\n");
		ACPI_FREE(p);
		return false;
	}

	c = &p->Package.Elements[1];

	if (c->Type != ACPI_TYPE_INTEGER) {
		aprint_error_dev(self,
		    "_GPE package needs integer as 2nd field\n");
		ACPI_FREE(p);
		return false;
	}
	*gpebit = c->Integer.Value;
	ACPI_FREE(p);
	return true;
}

static uint8_t
acpiec_read_data(struct acpiec_softc *sc)
{
	return bus_space_read_1(sc->sc_data_st, sc->sc_data_sh, 0);
}

static void
acpiec_write_data(struct acpiec_softc *sc, uint8_t val)
{
	bus_space_write_1(sc->sc_data_st, sc->sc_data_sh, 0, val);
}

static uint8_t
acpiec_read_status(struct acpiec_softc *sc)
{
	return bus_space_read_1(sc->sc_csr_st, sc->sc_csr_sh, 0);
}

static void
acpiec_write_command(struct acpiec_softc *sc, uint8_t cmd)
{
	bus_space_write_1(sc->sc_csr_st, sc->sc_csr_sh, 0, cmd);
}

static ACPI_STATUS
acpiec_space_setup(ACPI_HANDLE region, uint32_t func, void *arg,
    void **region_arg)
{

	if (func == ACPI_REGION_DEACTIVATE)
		*region_arg = NULL;
	else
		*region_arg = arg;

	return AE_OK;
}

static void
acpiec_lock(device_t dv)
{
	struct acpiec_softc *sc = device_private(dv);
	ACPI_STATUS rv;

	mutex_enter(&sc->sc_access_mtx);

	if (sc->sc_need_global_lock) {
		rv = AcpiAcquireGlobalLock(EC_LOCK_TIMEOUT,
		    &sc->sc_global_lock);
		if (rv != AE_OK) {
			aprint_error_dev(dv,
			    "failed to acquire global lock: %s\n",
			    AcpiFormatException(rv));
			return;
		}
	}
}

static void
acpiec_unlock(device_t dv)
{
	struct acpiec_softc *sc = device_private(dv);
	ACPI_STATUS rv;

	if (sc->sc_need_global_lock) {
		rv = AcpiReleaseGlobalLock(sc->sc_global_lock);
		if (rv != AE_OK) {
			aprint_error_dev(dv,
			    "failed to release global lock: %s\n",
			    AcpiFormatException(rv));
		}
	}
	mutex_exit(&sc->sc_access_mtx);
}

static ACPI_STATUS
acpiec_read(device_t dv, uint8_t addr, uint8_t *val)
{
	struct acpiec_softc *sc = device_private(dv);
	int i, timeo = 1000 * EC_CMD_TIMEOUT;

	acpiec_lock(dv);
	mutex_enter(&sc->sc_mtx);

	sc->sc_cur_addr = addr;
	sc->sc_state = EC_STATE_READ;

	for (i = 0; i < EC_POLL_TIMEOUT; ++i) {
		acpiec_gpe_state_machine(dv);
		if (sc->sc_state == EC_STATE_FREE)
			goto done;
		delay(1);
	}

	if (cold || acpiec_cold) {
		while (sc->sc_state != EC_STATE_FREE && timeo-- > 0) {
			delay(1000);
			acpiec_gpe_state_machine(dv);
		}
		if (sc->sc_state != EC_STATE_FREE) {
			mutex_exit(&sc->sc_mtx);
			acpiec_unlock(dv);
			aprint_error_dev(dv, "command timed out, state %d\n",
			    sc->sc_state);
			return AE_ERROR;
		}
	} else if (cv_timedwait(&sc->sc_cv, &sc->sc_mtx, EC_CMD_TIMEOUT * hz)) {
		mutex_exit(&sc->sc_mtx);
		acpiec_unlock(dv);
		aprint_error_dev(dv,
		    "command takes over %d sec...\n", EC_CMD_TIMEOUT);
		return AE_ERROR;
	}

done:
	*val = sc->sc_cur_val;

	mutex_exit(&sc->sc_mtx);
	acpiec_unlock(dv);
	return AE_OK;
}

static ACPI_STATUS
acpiec_write(device_t dv, uint8_t addr, uint8_t val)
{
	struct acpiec_softc *sc = device_private(dv);
	int i, timeo = 1000 * EC_CMD_TIMEOUT;

	acpiec_lock(dv);
	mutex_enter(&sc->sc_mtx);

	sc->sc_cur_addr = addr;
	sc->sc_cur_val = val;
	sc->sc_state = EC_STATE_WRITE;

	for (i = 0; i < EC_POLL_TIMEOUT; ++i) {
		acpiec_gpe_state_machine(dv);
		if (sc->sc_state == EC_STATE_FREE)
			goto done;
		delay(1);
	}

	if (cold || acpiec_cold) {
		while (sc->sc_state != EC_STATE_FREE && timeo-- > 0) {
			delay(1000);
			acpiec_gpe_state_machine(dv);
		}
		if (sc->sc_state != EC_STATE_FREE) {
			mutex_exit(&sc->sc_mtx);
			acpiec_unlock(dv);
			aprint_error_dev(dv, "command timed out, state %d\n",
			    sc->sc_state);
			return AE_ERROR;
		}
	} else if (cv_timedwait(&sc->sc_cv, &sc->sc_mtx, EC_CMD_TIMEOUT * hz)) {
		mutex_exit(&sc->sc_mtx);
		acpiec_unlock(dv);
		aprint_error_dev(dv,
		    "command takes over %d sec...\n", EC_CMD_TIMEOUT);
		return AE_ERROR;
	}

done:
	mutex_exit(&sc->sc_mtx);
	acpiec_unlock(dv);
	return AE_OK;
}

static ACPI_STATUS
acpiec_space_handler(uint32_t func, ACPI_PHYSICAL_ADDRESS paddr,
    uint32_t width, ACPI_INTEGER *value, void *arg, void *region_arg)
{
	device_t dv;
	ACPI_STATUS rv;
	uint8_t addr, *buf;
	unsigned int i;

	if (paddr > 0xff || width % 8 != 0 ||
	    value == NULL || arg == NULL || paddr + width / 8 > 0x100)
		return AE_BAD_PARAMETER;

	addr = paddr;
	dv = arg;
	buf = (uint8_t *)value;

	rv = AE_OK;

	switch (func) {
	case ACPI_READ:
		for (i = 0; i < width; i += 8, ++addr, ++buf) {
			rv = acpiec_read(dv, addr, buf);
			if (rv != AE_OK)
				break;
		}
		break;
	case ACPI_WRITE:
		for (i = 0; i < width; i += 8, ++addr, ++buf) {
			rv = acpiec_write(dv, addr, *buf);
			if (rv != AE_OK)
				break;
		}
		break;
	default:
		aprint_error("%s: invalid Address Space function called: %x\n",
		    device_xname(dv), (unsigned int)func);
		return AE_BAD_PARAMETER;
	}

	return rv;
}

static void
acpiec_gpe_query(void *arg)
{
	device_t dv = arg;
	struct acpiec_softc *sc = device_private(dv);
	uint8_t reg;
	char qxx[5];
	ACPI_STATUS rv;
	int i;

loop:
	mutex_enter(&sc->sc_mtx);

	if (sc->sc_got_sci == false)
		cv_wait(&sc->sc_cv_sci, &sc->sc_mtx);
	mutex_exit(&sc->sc_mtx);

	acpiec_lock(dv);
	mutex_enter(&sc->sc_mtx);

	/* The Query command can always be issued, so be defensive here. */
	sc->sc_got_sci = false;
	sc->sc_state = EC_STATE_QUERY;

	for (i = 0; i < EC_POLL_TIMEOUT; ++i) {
		acpiec_gpe_state_machine(dv);
		if (sc->sc_state == EC_STATE_FREE)
			goto done;
		delay(1);
	}

	cv_wait(&sc->sc_cv, &sc->sc_mtx);

done:
	reg = sc->sc_cur_val;

	mutex_exit(&sc->sc_mtx);
	acpiec_unlock(dv);

	if (reg == 0)
		goto loop; /* Spurious query result */

	/*
	 * Evaluate _Qxx to respond to the controller.
	 */
	snprintf(qxx, sizeof(qxx), "_Q%02X", (unsigned int)reg);
	rv = AcpiEvaluateObject(sc->sc_ech, qxx, NULL, NULL);
	if (rv != AE_OK && rv != AE_NOT_FOUND) {
		aprint_error_dev(dv, "GPE query method %s failed: %s",
		    qxx, AcpiFormatException(rv));
	}

	goto loop;
}

static void
acpiec_gpe_state_machine(device_t dv)
{
	struct acpiec_softc *sc = device_private(dv);
	uint8_t reg;

	reg = acpiec_read_status(sc);

	if (reg & EC_STATUS_SCI)
		sc->sc_got_sci = true;

	switch (sc->sc_state) {
	case EC_STATE_QUERY:
		if ((reg & EC_STATUS_IBF) != 0)
			break; /* Nothing of interest here. */
		acpiec_write_command(sc, EC_COMMAND_QUERY);
		sc->sc_state = EC_STATE_QUERY_VAL;
		break;

	case EC_STATE_QUERY_VAL:
		if ((reg & EC_STATUS_OBF) == 0)
			break; /* Nothing of interest here. */

		sc->sc_cur_val = acpiec_read_data(sc);
		sc->sc_state = EC_STATE_FREE;

		cv_signal(&sc->sc_cv);
		break;

	case EC_STATE_READ:
		if ((reg & EC_STATUS_IBF) != 0)
			break; /* Nothing of interest here. */

		acpiec_write_command(sc, EC_COMMAND_READ);
		sc->sc_state = EC_STATE_READ_ADDR;
		break;

	case EC_STATE_READ_ADDR:
		if ((reg & EC_STATUS_IBF) != 0)
			break; /* Nothing of interest here. */

		acpiec_write_data(sc, sc->sc_cur_addr);
		sc->sc_state = EC_STATE_READ_VAL;
		break;

	case EC_STATE_READ_VAL:
		if ((reg & EC_STATUS_OBF) == 0)
			break; /* Nothing of interest here. */
		sc->sc_cur_val = acpiec_read_data(sc);
		sc->sc_state = EC_STATE_FREE;

		cv_signal(&sc->sc_cv);
		break;

	case EC_STATE_WRITE:
		if ((reg & EC_STATUS_IBF) != 0)
			break; /* Nothing of interest here. */

		acpiec_write_command(sc, EC_COMMAND_WRITE);
		sc->sc_state = EC_STATE_WRITE_ADDR;
		break;

	case EC_STATE_WRITE_ADDR:
		if ((reg & EC_STATUS_IBF) != 0)
			break; /* Nothing of interest here. */
		acpiec_write_data(sc, sc->sc_cur_addr);
		sc->sc_state = EC_STATE_WRITE_VAL;
		break;

	case EC_STATE_WRITE_VAL:
		if ((reg & EC_STATUS_IBF) != 0)
			break; /* Nothing of interest here. */
		sc->sc_state = EC_STATE_FREE;
		cv_signal(&sc->sc_cv);

		acpiec_write_data(sc, sc->sc_cur_val);
		break;

	case EC_STATE_FREE:
		if (sc->sc_got_sci)
			cv_signal(&sc->sc_cv_sci);
		break;
	default:
		panic("invalid state");
	}

	if (sc->sc_state != EC_STATE_FREE)
		callout_schedule(&sc->sc_pseudo_intr, 1);
}

static void
acpiec_callout(void *arg)
{
	device_t dv = arg;
	struct acpiec_softc *sc = device_private(dv);

	mutex_enter(&sc->sc_mtx);
	acpiec_gpe_state_machine(dv);
	mutex_exit(&sc->sc_mtx);
}

static uint32_t
acpiec_gpe_handler(ACPI_HANDLE hdl, uint32_t gpebit, void *arg)
{
	device_t dv = arg;
	struct acpiec_softc *sc = device_private(dv);

	mutex_enter(&sc->sc_mtx);
	acpiec_gpe_state_machine(dv);
	mutex_exit(&sc->sc_mtx);

	return ACPI_INTERRUPT_HANDLED | ACPI_REENABLE_GPE;
}

ACPI_STATUS
acpiec_bus_read(device_t dv, u_int addr, ACPI_INTEGER *val, int width)
{
	return acpiec_space_handler(ACPI_READ, addr, width * 8, val, dv, NULL);
}

ACPI_STATUS
acpiec_bus_write(device_t dv, u_int addr, ACPI_INTEGER val, int width)
{
	return acpiec_space_handler(ACPI_WRITE, addr, width * 8, &val, dv,
	    NULL);
}

ACPI_HANDLE
acpiec_get_handle(device_t dv)
{
	struct acpiec_softc *sc = device_private(dv);

	return sc->sc_ech;
}

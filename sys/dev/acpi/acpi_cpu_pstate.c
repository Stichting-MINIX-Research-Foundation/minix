/* $NetBSD: acpi_cpu_pstate.c,v 1.53 2011/11/15 07:43:37 jruoho Exp $ */

/*-
 * Copyright (c) 2010, 2011 Jukka Ruohonen <jruohonen@iki.fi>
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
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpi_cpu_pstate.c,v 1.53 2011/11/15 07:43:37 jruoho Exp $");

#include <sys/param.h>
#include <sys/cpufreq.h>
#include <sys/kmem.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_cpu.h>

#define _COMPONENT	 ACPI_BUS_COMPONENT
ACPI_MODULE_NAME	 ("acpi_cpu_pstate")

static ACPI_STATUS	 acpicpu_pstate_pss(struct acpicpu_softc *);
static ACPI_STATUS	 acpicpu_pstate_pss_add(struct acpicpu_pstate *,
						ACPI_OBJECT *);
static ACPI_STATUS	 acpicpu_pstate_xpss(struct acpicpu_softc *);
static ACPI_STATUS	 acpicpu_pstate_xpss_add(struct acpicpu_pstate *,
						 ACPI_OBJECT *);
static ACPI_STATUS	 acpicpu_pstate_pct(struct acpicpu_softc *);
static ACPI_STATUS	 acpicpu_pstate_dep(struct acpicpu_softc *);
static int		 acpicpu_pstate_max(struct acpicpu_softc *);
static int		 acpicpu_pstate_min(struct acpicpu_softc *);
static void		 acpicpu_pstate_change(struct acpicpu_softc *);
static void		 acpicpu_pstate_reset(struct acpicpu_softc *);
static void		 acpicpu_pstate_bios(void);

extern struct acpicpu_softc **acpicpu_sc;

void
acpicpu_pstate_attach(device_t self)
{
	struct acpicpu_softc *sc = device_private(self);
	const char *str;
	ACPI_HANDLE tmp;
	ACPI_STATUS rv;

	rv = acpicpu_pstate_pss(sc);

	if (ACPI_FAILURE(rv)) {
		str = "_PSS";
		goto fail;
	}

	/*
	 * Append additional information from the extended _PSS,
	 * if available. Note that XPSS can not be used on Intel
	 * systems that use either _PDC or _OSC. From the XPSS
	 * method specification:
	 *
	 *   "The platform must not require the use of the
	 *    optional _PDC or _OSC methods to coordinate
	 *    between the operating system and firmware for
	 *    the purposes of enabling specific processor
	 *    power management features or implementations."
	 */
	if (sc->sc_cap == 0) {

		rv = acpicpu_pstate_xpss(sc);

		if (ACPI_SUCCESS(rv))
			sc->sc_flags |= ACPICPU_FLAG_P_XPSS;
	}

	rv = acpicpu_pstate_pct(sc);

	if (ACPI_FAILURE(rv)) {
		str = "_PCT";
		goto fail;
	}

	/*
	 * The ACPI 3.0 and 4.0 specifications mandate three
	 * objects for P-states: _PSS, _PCT, and _PPC. A less
	 * strict wording is however used in the earlier 2.0
	 * standard, and some systems conforming to ACPI 2.0
	 * do not have _PPC, the method for dynamic maximum.
	 */
	rv = AcpiGetHandle(sc->sc_node->ad_handle, "_PPC", &tmp);

	if (ACPI_FAILURE(rv))
		aprint_debug_dev(self, "_PPC missing\n");

	/*
	 * Carry out MD initialization.
	 */
	rv = acpicpu_md_pstate_init(sc);

	if (rv != 0) {
		rv = AE_SUPPORT;
		goto fail;
	}

	/*
	 * Query the optional _PSD.
	 */
	rv = acpicpu_pstate_dep(sc);

	if (ACPI_SUCCESS(rv))
		sc->sc_flags |= ACPICPU_FLAG_P_DEP;

	sc->sc_pstate_current = 0;
	sc->sc_flags |= ACPICPU_FLAG_P;

	acpicpu_pstate_bios();
	acpicpu_pstate_reset(sc);

	return;

fail:
	switch (rv) {

	case AE_NOT_FOUND:
		return;

	case AE_SUPPORT:
		aprint_verbose_dev(self, "P-states not supported\n");
		return;

	default:
		aprint_error_dev(self, "failed to evaluate "
		    "%s: %s\n", str, AcpiFormatException(rv));
	}
}

void
acpicpu_pstate_detach(device_t self)
{
	struct acpicpu_softc *sc = device_private(self);
	size_t size;

	if ((sc->sc_flags & ACPICPU_FLAG_P) == 0)
		return;

	(void)acpicpu_md_pstate_stop();

	size = sc->sc_pstate_count * sizeof(*sc->sc_pstate);

	if (sc->sc_pstate != NULL)
		kmem_free(sc->sc_pstate, size);

	sc->sc_flags &= ~ACPICPU_FLAG_P;
}

void
acpicpu_pstate_start(device_t self)
{
	struct acpicpu_softc *sc = device_private(self);

	if (acpicpu_md_pstate_start(sc) == 0)
		return;

	sc->sc_flags &= ~ACPICPU_FLAG_P;
	aprint_error_dev(self, "failed to start P-states\n");
}

void
acpicpu_pstate_suspend(void *aux)
{
	struct acpicpu_softc *sc;
	device_t self = aux;

	/*
	 * Reset any dynamic limits.
	 */
	sc = device_private(self);
	mutex_enter(&sc->sc_mtx);
	acpicpu_pstate_reset(sc);
	mutex_exit(&sc->sc_mtx);
}

void
acpicpu_pstate_resume(void *aux)
{
	/* Nothing. */
}

void
acpicpu_pstate_callback(void *aux)
{
	struct acpicpu_softc *sc;
	device_t self = aux;
	uint32_t freq;

	sc = device_private(self);
	mutex_enter(&sc->sc_mtx);
	acpicpu_pstate_change(sc);

	freq = sc->sc_pstate[sc->sc_pstate_max].ps_freq;

	if (sc->sc_pstate_saved == 0)
		sc->sc_pstate_saved = sc->sc_pstate_current;

	if (sc->sc_pstate_saved <= freq) {
		freq = sc->sc_pstate_saved;
		sc->sc_pstate_saved = 0;
	}

	mutex_exit(&sc->sc_mtx);
	cpufreq_set(sc->sc_ci, freq);
}

static ACPI_STATUS
acpicpu_pstate_pss(struct acpicpu_softc *sc)
{
	struct acpicpu_pstate *ps;
	ACPI_OBJECT *obj;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;
	uint32_t count;
	uint32_t i, j;

	rv = acpi_eval_struct(sc->sc_node->ad_handle, "_PSS", &buf);

	if (ACPI_FAILURE(rv))
		return rv;

	obj = buf.Pointer;

	if (obj->Type != ACPI_TYPE_PACKAGE) {
		rv = AE_TYPE;
		goto out;
	}

	sc->sc_pstate_count = obj->Package.Count;

	if (sc->sc_pstate_count == 0) {
		rv = AE_NOT_EXIST;
		goto out;
	}

	if (sc->sc_pstate_count > ACPICPU_P_STATE_MAX) {
		rv = AE_LIMIT;
		goto out;
	}

	sc->sc_pstate = kmem_zalloc(sc->sc_pstate_count *
	    sizeof(struct acpicpu_pstate), KM_SLEEP);

	if (sc->sc_pstate == NULL) {
		rv = AE_NO_MEMORY;
		goto out;
	}

	for (count = i = 0; i < sc->sc_pstate_count; i++) {

		ps = &sc->sc_pstate[i];
		rv = acpicpu_pstate_pss_add(ps, &obj->Package.Elements[i]);

		if (ACPI_FAILURE(rv)) {
			aprint_error_dev(sc->sc_dev, "failed to add "
			    "P-state: %s\n", AcpiFormatException(rv));
			ps->ps_freq = 0;
			continue;
		}

		for (j = 0; j < i; j++) {

			if (ps->ps_freq >= sc->sc_pstate[j].ps_freq) {
				ps->ps_freq = 0;
				break;
			}
		}

		if (ps->ps_freq != 0)
			count++;
	}

	rv = (count != 0) ? AE_OK : AE_NOT_EXIST;

out:
	if (buf.Pointer != NULL)
		ACPI_FREE(buf.Pointer);

	return rv;
}

static ACPI_STATUS
acpicpu_pstate_pss_add(struct acpicpu_pstate *ps, ACPI_OBJECT *obj)
{
	ACPI_OBJECT *elm;
	int i;

	if (obj->Type != ACPI_TYPE_PACKAGE)
		return AE_TYPE;

	if (obj->Package.Count != 6)
		return AE_BAD_DATA;

	elm = obj->Package.Elements;

	for (i = 0; i < 6; i++) {

		if (elm[i].Type != ACPI_TYPE_INTEGER)
			return AE_TYPE;

		if (elm[i].Integer.Value > UINT32_MAX)
			return AE_AML_NUMERIC_OVERFLOW;
	}

	ps->ps_freq       = elm[0].Integer.Value;
	ps->ps_power      = elm[1].Integer.Value;
	ps->ps_latency    = elm[2].Integer.Value;
	ps->ps_latency_bm = elm[3].Integer.Value;
	ps->ps_control    = elm[4].Integer.Value;
	ps->ps_status     = elm[5].Integer.Value;

	if (ps->ps_freq == 0 || ps->ps_freq > 9999)
		return AE_BAD_DECIMAL_CONSTANT;

	/*
	 * Sanity check also the latency levels. Some systems may
	 * report a value zero, but we keep one microsecond as the
	 * lower bound; see for instance AMD family 12h,
	 *
	 *	Advanced Micro Devices: BIOS and Kernel Developer's
	 *	Guide (BKDG) for AMD Family 12h Processors. Section
	 *	2.5.3.1.9.2, Revision 3.02, October, 2011.
	 */
	if (ps->ps_latency == 0 || ps->ps_latency > 1000)
		ps->ps_latency = 1;

	return AE_OK;
}

static ACPI_STATUS
acpicpu_pstate_xpss(struct acpicpu_softc *sc)
{
	struct acpicpu_pstate *ps;
	ACPI_OBJECT *obj;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;
	uint32_t i = 0;

	rv = acpi_eval_struct(sc->sc_node->ad_handle, "XPSS", &buf);

	if (ACPI_FAILURE(rv))
		goto out;

	obj = buf.Pointer;

	if (obj->Type != ACPI_TYPE_PACKAGE) {
		rv = AE_TYPE;
		goto out;
	}

	if (obj->Package.Count != sc->sc_pstate_count) {
		rv = AE_LIMIT;
		goto out;
	}

	while (i < sc->sc_pstate_count) {

		ps = &sc->sc_pstate[i];
		acpicpu_pstate_xpss_add(ps, &obj->Package.Elements[i]);

		i++;
	}

out:
	if (ACPI_FAILURE(rv) && rv != AE_NOT_FOUND)
		aprint_error_dev(sc->sc_dev, "failed to evaluate "
		    "XPSS: %s\n", AcpiFormatException(rv));

	if (buf.Pointer != NULL)
		ACPI_FREE(buf.Pointer);

	return rv;
}

static ACPI_STATUS
acpicpu_pstate_xpss_add(struct acpicpu_pstate *ps, ACPI_OBJECT *obj)
{
	ACPI_OBJECT *elm;
	int i;

	if (obj->Type != ACPI_TYPE_PACKAGE)
		return AE_TYPE;

	if (obj->Package.Count != 8)
		return AE_BAD_DATA;

	elm = obj->Package.Elements;

	for (i = 0; i < 4; i++) {

		if (elm[i].Type != ACPI_TYPE_INTEGER)
			return AE_TYPE;

		if (elm[i].Integer.Value > UINT32_MAX)
			return AE_AML_NUMERIC_OVERFLOW;
	}

	for (; i < 8; i++) {

		if (elm[i].Type != ACPI_TYPE_BUFFER)
			return AE_TYPE;

		if (elm[i].Buffer.Length != 8)
			return AE_LIMIT;
	}

	/*
	 * Only overwrite the elements that were
	 * not available from the conventional _PSS.
	 */
	if (ps->ps_freq == 0)
		ps->ps_freq = elm[0].Integer.Value;

	if (ps->ps_power == 0)
		ps->ps_power = elm[1].Integer.Value;

	if (ps->ps_latency == 0)
		ps->ps_latency = elm[2].Integer.Value;

	if (ps->ps_latency_bm == 0)
		ps->ps_latency_bm = elm[3].Integer.Value;

	if (ps->ps_control == 0)
		ps->ps_control = ACPI_GET64(elm[4].Buffer.Pointer);

	if (ps->ps_status == 0)
		ps->ps_status = ACPI_GET64(elm[5].Buffer.Pointer);

	if (ps->ps_control_mask == 0)
		ps->ps_control_mask = ACPI_GET64(elm[6].Buffer.Pointer);

	if (ps->ps_status_mask == 0)
		ps->ps_status_mask = ACPI_GET64(elm[7].Buffer.Pointer);

	ps->ps_flags |= ACPICPU_FLAG_P_XPSS;

	if (ps->ps_freq == 0 || ps->ps_freq > 9999)
		return AE_BAD_DECIMAL_CONSTANT;

	if (ps->ps_latency == 0 || ps->ps_latency > 1000)
		ps->ps_latency = 1;

	return AE_OK;
}

static ACPI_STATUS
acpicpu_pstate_pct(struct acpicpu_softc *sc)
{
	static const size_t size = sizeof(struct acpicpu_reg);
	struct acpicpu_reg *reg[2];
	struct acpicpu_pstate *ps;
	ACPI_OBJECT *elm, *obj;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;
	uint8_t width;
	uint32_t i;

	rv = acpi_eval_struct(sc->sc_node->ad_handle, "_PCT", &buf);

	if (ACPI_FAILURE(rv))
		return rv;

	obj = buf.Pointer;

	if (obj->Type != ACPI_TYPE_PACKAGE) {
		rv = AE_TYPE;
		goto out;
	}

	if (obj->Package.Count != 2) {
		rv = AE_LIMIT;
		goto out;
	}

	for (i = 0; i < 2; i++) {

		elm = &obj->Package.Elements[i];

		if (elm->Type != ACPI_TYPE_BUFFER) {
			rv = AE_TYPE;
			goto out;
		}

		if (size > elm->Buffer.Length) {
			rv = AE_AML_BAD_RESOURCE_LENGTH;
			goto out;
		}

		reg[i] = (struct acpicpu_reg *)elm->Buffer.Pointer;

		switch (reg[i]->reg_spaceid) {

		case ACPI_ADR_SPACE_SYSTEM_IO:

			if (reg[i]->reg_addr == 0) {
				rv = AE_AML_ILLEGAL_ADDRESS;
				goto out;
			}

			width = reg[i]->reg_bitwidth;

			if (width + reg[i]->reg_bitoffset > 32) {
				rv = AE_AML_BAD_RESOURCE_VALUE;
				goto out;
			}

			if (width != 8 && width != 16 && width != 32) {
				rv = AE_AML_BAD_RESOURCE_VALUE;
				goto out;
			}

			break;

		case ACPI_ADR_SPACE_FIXED_HARDWARE:

			if ((sc->sc_flags & ACPICPU_FLAG_P_XPSS) != 0) {

				if (reg[i]->reg_bitwidth != 64) {
					rv = AE_AML_BAD_RESOURCE_VALUE;
					goto out;
				}

				if (reg[i]->reg_bitoffset != 0) {
					rv = AE_AML_BAD_RESOURCE_VALUE;
					goto out;
				}

				break;
			}

			if ((sc->sc_flags & ACPICPU_FLAG_P_FFH) == 0) {
				rv = AE_SUPPORT;
				goto out;
			}

			break;

		default:
			rv = AE_AML_INVALID_SPACE_ID;
			goto out;
		}
	}

	if (reg[0]->reg_spaceid != reg[1]->reg_spaceid) {
		rv = AE_AML_INVALID_SPACE_ID;
		goto out;
	}

	(void)memcpy(&sc->sc_pstate_control, reg[0], size);
	(void)memcpy(&sc->sc_pstate_status,  reg[1], size);

	if ((sc->sc_flags & ACPICPU_FLAG_P_XPSS) != 0) {

		/*
		 * At the very least, mandate that
		 * XPSS supplies the control address.
		 */
		if (sc->sc_pstate_control.reg_addr == 0) {
			rv = AE_AML_BAD_RESOURCE_LENGTH;
			goto out;
		}

		/*
		 * If XPSS is present, copy the supplied
		 * MSR addresses to the P-state structures.
		 */
		for (i = 0; i < sc->sc_pstate_count; i++) {

			ps = &sc->sc_pstate[i];

			if (ps->ps_freq == 0)
				continue;

			ps->ps_status_addr  = sc->sc_pstate_status.reg_addr;
			ps->ps_control_addr = sc->sc_pstate_control.reg_addr;
		}
	}

out:
	if (buf.Pointer != NULL)
		ACPI_FREE(buf.Pointer);

	return rv;
}

static ACPI_STATUS
acpicpu_pstate_dep(struct acpicpu_softc *sc)
{
	ACPI_OBJECT *elm, *obj;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;
	uint32_t val;
	uint8_t i, n;

	rv = acpi_eval_struct(sc->sc_node->ad_handle, "_PSD", &buf);

	if (ACPI_FAILURE(rv))
		goto out;

	obj = buf.Pointer;

	if (obj->Type != ACPI_TYPE_PACKAGE) {
		rv = AE_TYPE;
		goto out;
	}

	if (obj->Package.Count != 1) {
		rv = AE_LIMIT;
		goto out;
	}

	elm = &obj->Package.Elements[0];

	if (obj->Type != ACPI_TYPE_PACKAGE) {
		rv = AE_TYPE;
		goto out;
	}

	n = elm->Package.Count;

	if (n != 5) {
		rv = AE_LIMIT;
		goto out;
	}

	elm = elm->Package.Elements;

	for (i = 0; i < n; i++) {

		if (elm[i].Type != ACPI_TYPE_INTEGER) {
			rv = AE_TYPE;
			goto out;
		}

		if (elm[i].Integer.Value > UINT32_MAX) {
			rv = AE_AML_NUMERIC_OVERFLOW;
			goto out;
		}
	}

	val = elm[1].Integer.Value;

	if (val != 0)
		aprint_debug_dev(sc->sc_dev, "invalid revision in _PSD\n");

	val = elm[3].Integer.Value;

	if (val < ACPICPU_DEP_SW_ALL || val > ACPICPU_DEP_HW_ALL) {
		rv = AE_AML_BAD_RESOURCE_VALUE;
		goto out;
	}

	val = elm[4].Integer.Value;

	if (val > sc->sc_ncpus) {
		rv = AE_BAD_VALUE;
		goto out;
	}

	sc->sc_pstate_dep.dep_domain = elm[2].Integer.Value;
	sc->sc_pstate_dep.dep_type   = elm[3].Integer.Value;
	sc->sc_pstate_dep.dep_ncpus  = elm[4].Integer.Value;

out:
	if (ACPI_FAILURE(rv) && rv != AE_NOT_FOUND)
		aprint_debug_dev(sc->sc_dev, "failed to evaluate "
		    "_PSD: %s\n", AcpiFormatException(rv));

	if (buf.Pointer != NULL)
		ACPI_FREE(buf.Pointer);

	return rv;
}

static int
acpicpu_pstate_max(struct acpicpu_softc *sc)
{
	ACPI_INTEGER val;
	ACPI_STATUS rv;

	/*
	 * Evaluate the currently highest P-state that can be used.
	 * If available, we can use either this state or any lower
	 * power (i.e. higher numbered) state from the _PSS object.
	 * Note that the return value must match the _OST parameter.
	 */
	rv = acpi_eval_integer(sc->sc_node->ad_handle, "_PPC", &val);

	if (ACPI_SUCCESS(rv) && val < sc->sc_pstate_count) {

		if (sc->sc_pstate[val].ps_freq != 0) {
			sc->sc_pstate_max = val;
			return 0;
		}
	}

	return 1;
}

static int
acpicpu_pstate_min(struct acpicpu_softc *sc)
{
	ACPI_INTEGER val;
	ACPI_STATUS rv;

	/*
	 * The _PDL object defines the minimum when passive cooling
	 * is being performed. If available, we can use the returned
	 * state or any higher power (i.e. lower numbered) state.
	 */
	rv = acpi_eval_integer(sc->sc_node->ad_handle, "_PDL", &val);

	if (ACPI_SUCCESS(rv) && val < sc->sc_pstate_count) {

		if (sc->sc_pstate[val].ps_freq == 0)
			return 1;

		if (val >= sc->sc_pstate_max) {
			sc->sc_pstate_min = val;
			return 0;
		}
	}

	return 1;
}

static void
acpicpu_pstate_change(struct acpicpu_softc *sc)
{
	static ACPI_STATUS rv = AE_OK;
	ACPI_OBJECT_LIST arg;
	ACPI_OBJECT obj[2];
	static int val = 0;

	acpicpu_pstate_reset(sc);

	/*
	 * Cache the checks as the optional
	 * _PDL and _OST are rarely present.
	 */
	if (val == 0)
		val = acpicpu_pstate_min(sc);

	arg.Count = 2;
	arg.Pointer = obj;

	obj[0].Type = ACPI_TYPE_INTEGER;
	obj[1].Type = ACPI_TYPE_INTEGER;

	obj[0].Integer.Value = ACPICPU_P_NOTIFY;
	obj[1].Integer.Value = acpicpu_pstate_max(sc);

	if (ACPI_FAILURE(rv))
		return;

	rv = AcpiEvaluateObject(sc->sc_node->ad_handle, "_OST", &arg, NULL);
}

static void
acpicpu_pstate_reset(struct acpicpu_softc *sc)
{

	sc->sc_pstate_max = 0;
	sc->sc_pstate_min = sc->sc_pstate_count - 1;

}

static void
acpicpu_pstate_bios(void)
{
	const uint8_t val = AcpiGbl_FADT.PstateControl;
	const uint32_t addr = AcpiGbl_FADT.SmiCommand;

	if (addr == 0 || val == 0)
		return;

	(void)AcpiOsWritePort(addr, val, 8);
}

void
acpicpu_pstate_get(void *aux, void *cpu_freq)
{
	struct acpicpu_pstate *ps = NULL;
	struct cpu_info *ci = curcpu();
	struct acpicpu_softc *sc;
	uint32_t freq, i, val = 0;
	uint64_t addr;
	uint8_t width;
	int rv;

	sc = acpicpu_sc[ci->ci_acpiid];

	if (__predict_false(sc == NULL)) {
		rv = ENXIO;
		goto fail;
	}

	if (__predict_false((sc->sc_flags & ACPICPU_FLAG_P) == 0)) {
		rv = ENODEV;
		goto fail;
	}

	mutex_enter(&sc->sc_mtx);

	/*
	 * Use the cached value, if available.
	 */
	if (sc->sc_pstate_current != 0) {
		*(uint32_t *)cpu_freq = sc->sc_pstate_current;
		mutex_exit(&sc->sc_mtx);
		return;
	}

	mutex_exit(&sc->sc_mtx);

	switch (sc->sc_pstate_status.reg_spaceid) {

	case ACPI_ADR_SPACE_FIXED_HARDWARE:

		rv = acpicpu_md_pstate_get(sc, &freq);

		if (__predict_false(rv != 0))
			goto fail;

		break;

	case ACPI_ADR_SPACE_SYSTEM_IO:

		addr  = sc->sc_pstate_status.reg_addr;
		width = sc->sc_pstate_status.reg_bitwidth;

		(void)AcpiOsReadPort(addr, &val, width);

		if (val == 0) {
			rv = EIO;
			goto fail;
		}

		for (i = 0; i < sc->sc_pstate_count; i++) {

			if (sc->sc_pstate[i].ps_freq == 0)
				continue;

			if (val == sc->sc_pstate[i].ps_status) {
				ps = &sc->sc_pstate[i];
				break;
			}
		}

		if (ps == NULL) {
			rv = EIO;
			goto fail;
		}

		freq = ps->ps_freq;
		break;

	default:
		rv = ENOTTY;
		goto fail;
	}

	mutex_enter(&sc->sc_mtx);
	sc->sc_pstate_current = freq;
	*(uint32_t *)cpu_freq = freq;
	mutex_exit(&sc->sc_mtx);

	return;

fail:
	aprint_error_dev(sc->sc_dev, "failed "
	    "to get frequency (err %d)\n", rv);

	mutex_enter(&sc->sc_mtx);
	sc->sc_pstate_current = 0;
	*(uint32_t *)cpu_freq = 0;
	mutex_exit(&sc->sc_mtx);
}

void
acpicpu_pstate_set(void *aux, void *cpu_freq)
{
	struct acpicpu_pstate *ps = NULL;
	struct cpu_info *ci = curcpu();
	struct acpicpu_softc *sc;
	uint32_t freq, i, val;
	uint64_t addr;
	uint8_t width;
	int rv;

	freq = *(uint32_t *)cpu_freq;
	sc = acpicpu_sc[ci->ci_acpiid];

	if (__predict_false(sc == NULL)) {
		rv = ENXIO;
		goto fail;
	}

	if (__predict_false((sc->sc_flags & ACPICPU_FLAG_P) == 0)) {
		rv = ENODEV;
		goto fail;
	}

	mutex_enter(&sc->sc_mtx);

	if (sc->sc_pstate_current == freq) {
		mutex_exit(&sc->sc_mtx);
		return;
	}

	/*
	 * Verify that the requested frequency is available.
	 *
	 * The access needs to be protected since the currently
	 * available maximum and minimum may change dynamically.
	 */
	for (i = sc->sc_pstate_max; i <= sc->sc_pstate_min; i++) {

		if (__predict_false(sc->sc_pstate[i].ps_freq == 0))
			continue;

		if (sc->sc_pstate[i].ps_freq == freq) {
			ps = &sc->sc_pstate[i];
			break;
		}
	}

	mutex_exit(&sc->sc_mtx);

	if (__predict_false(ps == NULL)) {
		rv = EINVAL;
		goto fail;
	}

	switch (sc->sc_pstate_control.reg_spaceid) {

	case ACPI_ADR_SPACE_FIXED_HARDWARE:

		rv = acpicpu_md_pstate_set(ps);

		if (__predict_false(rv != 0))
			goto fail;

		break;

	case ACPI_ADR_SPACE_SYSTEM_IO:

		addr  = sc->sc_pstate_control.reg_addr;
		width = sc->sc_pstate_control.reg_bitwidth;

		(void)AcpiOsWritePort(addr, ps->ps_control, width);

		addr  = sc->sc_pstate_status.reg_addr;
		width = sc->sc_pstate_status.reg_bitwidth;

		/*
		 * Some systems take longer to respond
		 * than the reported worst-case latency.
		 */
		for (i = val = 0; i < ACPICPU_P_STATE_RETRY; i++) {

			(void)AcpiOsReadPort(addr, &val, width);

			if (val == ps->ps_status)
				break;

			DELAY(ps->ps_latency);
		}

		if (i == ACPICPU_P_STATE_RETRY) {
			rv = EAGAIN;
			goto fail;
		}

		break;

	default:
		rv = ENOTTY;
		goto fail;
	}

	mutex_enter(&sc->sc_mtx);
	ps->ps_evcnt.ev_count++;
	sc->sc_pstate_current = freq;
	mutex_exit(&sc->sc_mtx);

	return;

fail:
	if (rv != EINVAL)
		aprint_error_dev(sc->sc_dev, "failed to set "
		    "frequency to %u (err %d)\n", freq, rv);

	mutex_enter(&sc->sc_mtx);
	sc->sc_pstate_current = 0;
	mutex_exit(&sc->sc_mtx);
}

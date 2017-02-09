/* $NetBSD: acpi_cpu.c,v 1.50 2014/02/25 18:30:09 pooka Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: acpi_cpu.c,v 1.50 2014/02/25 18:30:09 pooka Exp $");

#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/evcnt.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/cpufreq.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_cpu.h>

#include <machine/acpi_machdep.h>
#include <machine/cpuvar.h>

#define _COMPONENT	  ACPI_BUS_COMPONENT
ACPI_MODULE_NAME	  ("acpi_cpu")

static int		  acpicpu_match(device_t, cfdata_t, void *);
static void		  acpicpu_attach(device_t, device_t, void *);
static int		  acpicpu_detach(device_t, int);
static int		  acpicpu_once_attach(void);
static int		  acpicpu_once_detach(void);
static void		  acpicpu_start(device_t);
static void		  acpicpu_sysctl(device_t);

static ACPI_STATUS	  acpicpu_object(ACPI_HANDLE, struct acpicpu_object *);
static uint32_t		  acpicpu_cap(struct acpicpu_softc *);
static ACPI_STATUS	  acpicpu_cap_osc(struct acpicpu_softc *,
					  uint32_t, uint32_t *);
static void		  acpicpu_notify(ACPI_HANDLE, uint32_t, void *);
static bool		  acpicpu_suspend(device_t, const pmf_qual_t *);
static bool		  acpicpu_resume(device_t, const pmf_qual_t *);
static void		  acpicpu_evcnt_attach(device_t);
static void		  acpicpu_evcnt_detach(device_t);
static void		  acpicpu_debug_print(device_t);
static const char	 *acpicpu_debug_print_method_c(uint8_t);
static const char	 *acpicpu_debug_print_method_pt(uint8_t);
static const char	 *acpicpu_debug_print_dep(uint32_t);

static uint32_t		  acpicpu_count = 0;
struct acpicpu_softc	**acpicpu_sc = NULL;
static struct sysctllog	 *acpicpu_log = NULL;
static bool		  acpicpu_dynamic = true;
static bool		  acpicpu_passive = true;

static const struct {
	const char	 *manu;
	const char	 *prod;
	const char	 *vers;
} acpicpu_quirks[] = {
	{ "Supermicro", "PDSMi-LN4", "0123456789" },
	{ "ASUSTeK Computer INC.", "M2A-MX", "Rev 1.xx" },
};

CFATTACH_DECL_NEW(acpicpu, sizeof(struct acpicpu_softc),
    acpicpu_match, acpicpu_attach, acpicpu_detach, NULL);

static int
acpicpu_match(device_t parent, cfdata_t match, void *aux)
{
	const char *manu, *prod, *vers;
	struct cpu_info *ci;
	size_t i;

	if (acpi_softc == NULL)
		return 0;

	manu = pmf_get_platform("board-vendor");
	prod = pmf_get_platform("board-product");
	vers = pmf_get_platform("board-version");

	if (manu != NULL && prod != NULL && vers != NULL) {

		for (i = 0; i < __arraycount(acpicpu_quirks); i++) {

			if (strcasecmp(acpicpu_quirks[i].manu, manu) == 0 &&
			    strcasecmp(acpicpu_quirks[i].prod, prod) == 0 &&
			    strcasecmp(acpicpu_quirks[i].vers, vers) == 0)
				return 0;
		}
	}

	ci = acpicpu_md_match(parent, match, aux);

	if (ci == NULL)
		return 0;

	if (acpi_match_cpu_info(ci) == NULL)
		return 0;

	return 10;
}

static void
acpicpu_attach(device_t parent, device_t self, void *aux)
{
	struct acpicpu_softc *sc = device_private(self);
	struct cpu_info *ci;
	ACPI_HANDLE hdl;
	cpuid_t id;
	int rv;

	ci = acpicpu_md_attach(parent, self, aux);

	if (ci == NULL)
		return;

	sc->sc_ci = ci;
	sc->sc_dev = self;
	sc->sc_cold = true;

	hdl = acpi_match_cpu_info(ci);

	if (hdl == NULL) {
		aprint_normal(": failed to match processor\n");
		return;
	}

	sc->sc_node = acpi_match_node(hdl);

	if (acpicpu_once_attach() != 0) {
		aprint_normal(": failed to initialize\n");
		return;
	}

	KASSERT(acpi_softc != NULL);
	KASSERT(acpicpu_sc != NULL);
	KASSERT(sc->sc_node != NULL);

	id = sc->sc_ci->ci_acpiid;

	if (acpicpu_sc[id] != NULL) {
		aprint_normal(": already attached\n");
		return;
	}

	aprint_naive("\n");
	aprint_normal(": ACPI CPU\n");

	rv = acpicpu_object(sc->sc_node->ad_handle, &sc->sc_object);

	if (ACPI_FAILURE(rv))
		aprint_verbose_dev(self, "failed to obtain CPU object\n");

	acpicpu_count++;
	acpicpu_sc[id] = sc;

	sc->sc_cap = acpicpu_cap(sc);
	sc->sc_ncpus = acpi_md_ncpus();
	sc->sc_flags = acpicpu_md_flags();

	KASSERT(acpicpu_count <= sc->sc_ncpus);
	KASSERT(sc->sc_node->ad_device == NULL);

	sc->sc_node->ad_device = self;
	mutex_init(&sc->sc_mtx, MUTEX_DEFAULT, IPL_NONE);

	acpicpu_cstate_attach(self);
	acpicpu_pstate_attach(self);
	acpicpu_tstate_attach(self);

	acpicpu_debug_print(self);
	acpicpu_evcnt_attach(self);

	(void)config_interrupts(self, acpicpu_start);
	(void)acpi_register_notify(sc->sc_node, acpicpu_notify);
	(void)pmf_device_register(self, acpicpu_suspend, acpicpu_resume);
}

static int
acpicpu_detach(device_t self, int flags)
{
	struct acpicpu_softc *sc = device_private(self);

	sc->sc_cold = true;

	acpicpu_evcnt_detach(self);
	acpi_deregister_notify(sc->sc_node);

	acpicpu_cstate_detach(self);
	acpicpu_pstate_detach(self);
	acpicpu_tstate_detach(self);

	mutex_destroy(&sc->sc_mtx);
	sc->sc_node->ad_device = NULL;

	acpicpu_count--;
	acpicpu_once_detach();

	return 0;
}

static int
acpicpu_once_attach(void)
{
	struct acpicpu_softc *sc;
	unsigned int i;

	if (acpicpu_count != 0)
		return 0;

	KASSERT(acpicpu_sc == NULL);
	KASSERT(acpicpu_log == NULL);

	acpicpu_sc = kmem_zalloc(maxcpus * sizeof(*sc), KM_SLEEP);

	if (acpicpu_sc == NULL)
		return ENOMEM;

	for (i = 0; i < maxcpus; i++)
		acpicpu_sc[i] = NULL;

	return 0;
}

static int
acpicpu_once_detach(void)
{
	struct acpicpu_softc *sc;

	if (acpicpu_count != 0)
		return EDEADLK;

	cpufreq_deregister();

	if (acpicpu_log != NULL)
		sysctl_teardown(&acpicpu_log);

	if (acpicpu_sc != NULL)
		kmem_free(acpicpu_sc, maxcpus * sizeof(*sc));

	return 0;
}

static void
acpicpu_start(device_t self)
{
	struct acpicpu_softc *sc = device_private(self);
	static uint32_t count = 0;
	struct cpufreq cf;
	uint32_t i;

	/*
	 * Run the state-specific initialization routines. These
	 * must run only once, after interrupts have been enabled,
	 * all CPUs are running, and all ACPI CPUs have attached.
	 */
	if (++count != acpicpu_count || acpicpu_count != sc->sc_ncpus) {
		sc->sc_cold = false;
		return;
	}

	/*
	 * Set the last ACPI CPU as non-cold
	 * only after C-states are enabled.
	 */
	if ((sc->sc_flags & ACPICPU_FLAG_C) != 0)
		acpicpu_cstate_start(self);

	sc->sc_cold = false;

	if ((sc->sc_flags & ACPICPU_FLAG_P) != 0)
		acpicpu_pstate_start(self);

	if ((sc->sc_flags & ACPICPU_FLAG_T) != 0)
		acpicpu_tstate_start(self);

	acpicpu_sysctl(self);
	aprint_debug_dev(self, "ACPI CPUs started\n");

	/*
	 * Register with cpufreq(9).
	 */
	if ((sc->sc_flags & ACPICPU_FLAG_P) != 0) {

		(void)memset(&cf, 0, sizeof(struct cpufreq));

		cf.cf_mp = false;
		cf.cf_cookie = NULL;
		cf.cf_get_freq = acpicpu_pstate_get;
		cf.cf_set_freq = acpicpu_pstate_set;
		cf.cf_state_count = sc->sc_pstate_count;

		(void)strlcpy(cf.cf_name, "acpicpu", sizeof(cf.cf_name));

		for (i = 0; i < sc->sc_pstate_count; i++) {

			if (sc->sc_pstate[i].ps_freq == 0)
				continue;

			cf.cf_state[i].cfs_freq = sc->sc_pstate[i].ps_freq;
			cf.cf_state[i].cfs_power = sc->sc_pstate[i].ps_power;
		}

		if (cpufreq_register(&cf) != 0)
			aprint_error_dev(self, "failed to register cpufreq\n");
	}
}

static void
acpicpu_sysctl(device_t self)
{
	const struct sysctlnode *node;
	int err;

	KASSERT(acpicpu_log == NULL);

	err = sysctl_createv(&acpicpu_log, 0, NULL, &node,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "acpi", NULL,
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL);

	if (err != 0)
		goto fail;

	err = sysctl_createv(&acpicpu_log, 0, &node, &node,
	    0, CTLTYPE_NODE, "cpu", SYSCTL_DESCR("ACPI CPU"),
	    NULL, 0, NULL, 0, CTL_CREATE, CTL_EOL);

	if (err != 0)
		goto fail;

	err = sysctl_createv(&acpicpu_log, 0, &node, NULL,
	    CTLFLAG_READWRITE, CTLTYPE_BOOL, "dynamic",
	    SYSCTL_DESCR("Dynamic states"), NULL, 0,
	    &acpicpu_dynamic, 0, CTL_CREATE, CTL_EOL);

	if (err != 0)
		goto fail;

	err = sysctl_createv(&acpicpu_log, 0, &node, NULL,
	    CTLFLAG_READWRITE, CTLTYPE_BOOL, "passive",
	    SYSCTL_DESCR("Passive cooling"), NULL, 0,
	    &acpicpu_passive, 0, CTL_CREATE, CTL_EOL);

	if (err != 0)
		goto fail;

	return;

fail:
	aprint_error_dev(self, "failed to initialize sysctl (err %d)\n", err);
}

static ACPI_STATUS
acpicpu_object(ACPI_HANDLE hdl, struct acpicpu_object *ao)
{
	ACPI_OBJECT *obj;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;

	rv = acpi_eval_struct(hdl, NULL, &buf);

	if (ACPI_FAILURE(rv))
		goto out;

	obj = buf.Pointer;

	if (obj->Type != ACPI_TYPE_PROCESSOR) {
		rv = AE_TYPE;
		goto out;
	}

	if (obj->Processor.ProcId > (uint32_t)maxcpus) {
		rv = AE_LIMIT;
		goto out;
	}

	KDASSERT((uint64_t)obj->Processor.PblkAddress < UINT32_MAX);

	if (ao != NULL) {
		ao->ao_procid = obj->Processor.ProcId;
		ao->ao_pblklen = obj->Processor.PblkLength;
		ao->ao_pblkaddr = obj->Processor.PblkAddress;
	}

out:
	if (buf.Pointer != NULL)
		ACPI_FREE(buf.Pointer);

	return rv;
}

static uint32_t
acpicpu_cap(struct acpicpu_softc *sc)
{
	uint32_t flags, cap = 0;
	ACPI_STATUS rv;

	/*
	 * Query and set machine-dependent capabilities.
	 * Note that the Intel-specific _PDC method has
	 * already been evaluated. It was furthermore
	 * deprecated in the ACPI 3.0 in favor of _OSC.
	 */
	flags = acpi_md_pdc();
	rv = acpicpu_cap_osc(sc, flags, &cap);

	if (ACPI_FAILURE(rv) && rv != AE_NOT_FOUND) {

		aprint_error_dev(sc->sc_dev, "failed to evaluate "
		    "_OSC: %s\n", AcpiFormatException(rv));
	}

	return (cap != 0) ? cap : flags;
}

static ACPI_STATUS
acpicpu_cap_osc(struct acpicpu_softc *sc, uint32_t flags, uint32_t *val)
{
	ACPI_OBJECT_LIST arg;
	ACPI_OBJECT obj[4];
	ACPI_OBJECT *osc;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;
	uint32_t cap[2];
	uint32_t *ptr;
	int i = 5;

	static uint8_t intel_uuid[16] = {
		0x16, 0xA6, 0x77, 0x40, 0x0C, 0x29, 0xBE, 0x47,
		0x9E, 0xBD, 0xD8, 0x70, 0x58, 0x71, 0x39, 0x53
	};

	cap[0] = ACPI_OSC_QUERY;
	cap[1] = flags;

again:
	arg.Count = 4;
	arg.Pointer = obj;

	obj[0].Type = ACPI_TYPE_BUFFER;
	obj[0].Buffer.Length = sizeof(intel_uuid);
	obj[0].Buffer.Pointer = intel_uuid;

	obj[1].Type = ACPI_TYPE_INTEGER;
	obj[1].Integer.Value = ACPICPU_PDC_REVID;

	obj[2].Type = ACPI_TYPE_INTEGER;
	obj[2].Integer.Value = __arraycount(cap);

	obj[3].Type = ACPI_TYPE_BUFFER;
	obj[3].Buffer.Length = sizeof(cap);
	obj[3].Buffer.Pointer = (void *)cap;

	buf.Pointer = NULL;
	buf.Length = ACPI_ALLOCATE_LOCAL_BUFFER;

	rv = AcpiEvaluateObject(sc->sc_node->ad_handle, "_OSC", &arg, &buf);

	if (ACPI_FAILURE(rv))
		goto out;

	osc = buf.Pointer;

	if (osc->Type != ACPI_TYPE_BUFFER) {
		rv = AE_TYPE;
		goto out;
	}

	if (osc->Buffer.Length != sizeof(cap)) {
		rv = AE_BUFFER_OVERFLOW;
		goto out;
	}

	ptr = (uint32_t *)osc->Buffer.Pointer;

	if ((ptr[0] & ACPI_OSC_ERROR) != 0) {
		rv = AE_ERROR;
		goto out;
	}

	if ((ptr[0] & (ACPI_OSC_ERROR_REV | ACPI_OSC_ERROR_UUID)) != 0) {
		rv = AE_BAD_PARAMETER;
		goto out;
	}

	/*
	 * "It is strongly recommended that the OS evaluate
	 *  _OSC with the Query Support Flag set until _OSC
	 *  returns the Capabilities Masked bit clear, to
	 *  negotiate the set of features to be granted to
	 *  the OS for native support (ACPI 4.0, 6.2.10)."
	 */
	if ((ptr[0] & ACPI_OSC_ERROR_MASKED) != 0 && i >= 0) {

		ACPI_FREE(buf.Pointer);
		i--;

		goto again;
	}

	if ((cap[0] & ACPI_OSC_QUERY) != 0) {

		ACPI_FREE(buf.Pointer);
		cap[0] &= ~ACPI_OSC_QUERY;

		goto again;
	}

	/*
	 * It is permitted for _OSC to return all
	 * bits cleared, but this is specified to
	 * vary on per-device basis. Assume that
	 * everything rather than nothing will be
	 * supported in this case; we do not need
	 * the firmware to know the CPU features.
	 */
	*val = (ptr[1] != 0) ? ptr[1] : cap[1];

out:
	if (buf.Pointer != NULL)
		ACPI_FREE(buf.Pointer);

	return rv;
}

static void
acpicpu_notify(ACPI_HANDLE hdl, uint32_t evt, void *aux)
{
	ACPI_OSD_EXEC_CALLBACK func;
	struct acpicpu_softc *sc;
	device_t self = aux;

	sc = device_private(self);

	if (sc->sc_cold != false)
		return;

	if (acpicpu_dynamic != true)
		return;

	switch (evt) {

	case ACPICPU_C_NOTIFY:

		if ((sc->sc_flags & ACPICPU_FLAG_C) == 0)
			return;

		func = acpicpu_cstate_callback;
		break;

	case ACPICPU_P_NOTIFY:

		if ((sc->sc_flags & ACPICPU_FLAG_P) == 0)
			return;

		func = acpicpu_pstate_callback;
		break;

	case ACPICPU_T_NOTIFY:

		if ((sc->sc_flags & ACPICPU_FLAG_T) == 0)
			return;

		func = acpicpu_tstate_callback;
		break;

	default:
		aprint_error_dev(sc->sc_dev,  "unknown notify: 0x%02X\n", evt);
		return;
	}

	(void)AcpiOsExecute(OSL_NOTIFY_HANDLER, func, sc->sc_dev);
}

static bool
acpicpu_suspend(device_t self, const pmf_qual_t *qual)
{
	struct acpicpu_softc *sc = device_private(self);

	if ((sc->sc_flags & ACPICPU_FLAG_C) != 0)
		(void)acpicpu_cstate_suspend(self);

	if ((sc->sc_flags & ACPICPU_FLAG_P) != 0)
		(void)acpicpu_pstate_suspend(self);

	if ((sc->sc_flags & ACPICPU_FLAG_T) != 0)
		(void)acpicpu_tstate_suspend(self);

	sc->sc_cold = true;

	return true;
}

static bool
acpicpu_resume(device_t self, const pmf_qual_t *qual)
{
	struct acpicpu_softc *sc = device_private(self);
	static const int handler = OSL_NOTIFY_HANDLER;

	sc->sc_cold = false;

	if ((sc->sc_flags & ACPICPU_FLAG_C) != 0)
		(void)AcpiOsExecute(handler, acpicpu_cstate_resume, self);

	if ((sc->sc_flags & ACPICPU_FLAG_P) != 0)
		(void)AcpiOsExecute(handler, acpicpu_pstate_resume, self);

	if ((sc->sc_flags & ACPICPU_FLAG_T) != 0)
		(void)AcpiOsExecute(handler, acpicpu_tstate_resume, self);

	return true;
}

static void
acpicpu_evcnt_attach(device_t self)
{
	struct acpicpu_softc *sc = device_private(self);
	struct acpicpu_cstate *cs;
	struct acpicpu_pstate *ps;
	struct acpicpu_tstate *ts;
	const char *str;
	uint32_t i;

	for (i = 0; i < __arraycount(sc->sc_cstate); i++) {

		cs = &sc->sc_cstate[i];

		if (cs->cs_method == 0)
			continue;

		str = "HALT";

		if (cs->cs_method == ACPICPU_C_STATE_FFH)
			str = "MWAIT";

		if (cs->cs_method == ACPICPU_C_STATE_SYSIO)
			str = "I/O";

		(void)snprintf(cs->cs_name, sizeof(cs->cs_name),
		    "C%d (%s)", i, str);

		evcnt_attach_dynamic(&cs->cs_evcnt, EVCNT_TYPE_MISC,
		    NULL, device_xname(sc->sc_dev), cs->cs_name);
	}

	for (i = 0; i < sc->sc_pstate_count; i++) {

		ps = &sc->sc_pstate[i];

		if (ps->ps_freq == 0)
			continue;

		(void)snprintf(ps->ps_name, sizeof(ps->ps_name),
		    "P%u (%u MHz)", i, ps->ps_freq);

		evcnt_attach_dynamic(&ps->ps_evcnt, EVCNT_TYPE_MISC,
		    NULL, device_xname(sc->sc_dev), ps->ps_name);
	}

	for (i = 0; i < sc->sc_tstate_count; i++) {

		ts = &sc->sc_tstate[i];

		if (ts->ts_percent == 0)
			continue;

		(void)snprintf(ts->ts_name, sizeof(ts->ts_name),
		    "T%u (%u %%)", i, ts->ts_percent);

		evcnt_attach_dynamic(&ts->ts_evcnt, EVCNT_TYPE_MISC,
		    NULL, device_xname(sc->sc_dev), ts->ts_name);
	}
}

static void
acpicpu_evcnt_detach(device_t self)
{
	struct acpicpu_softc *sc = device_private(self);
	struct acpicpu_cstate *cs;
	struct acpicpu_pstate *ps;
	struct acpicpu_tstate *ts;
	uint32_t i;

	for (i = 0; i < __arraycount(sc->sc_cstate); i++) {

		cs = &sc->sc_cstate[i];

		if (cs->cs_method != 0)
			evcnt_detach(&cs->cs_evcnt);
	}

	for (i = 0; i < sc->sc_pstate_count; i++) {

		ps = &sc->sc_pstate[i];

		if (ps->ps_freq != 0)
			evcnt_detach(&ps->ps_evcnt);
	}

	for (i = 0; i < sc->sc_tstate_count; i++) {

		ts = &sc->sc_tstate[i];

		if (ts->ts_percent != 0)
			evcnt_detach(&ts->ts_evcnt);
	}
}

static void
acpicpu_debug_print(device_t self)
{
	struct acpicpu_softc *sc = device_private(self);
	struct cpu_info *ci = sc->sc_ci;
	struct acpicpu_cstate *cs;
	struct acpicpu_pstate *ps;
	struct acpicpu_tstate *ts;
	static bool once = false;
	struct acpicpu_dep *dep;
	uint32_t i, method;

	if (once != true) {

		for (i = 0; i < __arraycount(sc->sc_cstate); i++) {

			cs = &sc->sc_cstate[i];

			if (cs->cs_method == 0)
				continue;

			aprint_verbose_dev(sc->sc_dev, "C%d: %3s, "
			    "lat %3u us, pow %5u mW%s\n", i,
			    acpicpu_debug_print_method_c(cs->cs_method),
			    cs->cs_latency, cs->cs_power,
			    (cs->cs_flags != 0) ? ", bus master check" : "");
		}

		method = sc->sc_pstate_control.reg_spaceid;

		for (i = 0; i < sc->sc_pstate_count; i++) {

			ps = &sc->sc_pstate[i];

			if (ps->ps_freq == 0)
				continue;

			aprint_verbose_dev(sc->sc_dev, "P%d: %3s, "
			    "lat %3u us, pow %5u mW, %4u MHz%s\n", i,
			    acpicpu_debug_print_method_pt(method),
			    ps->ps_latency, ps->ps_power, ps->ps_freq,
			    (ps->ps_flags & ACPICPU_FLAG_P_TURBO) != 0 ?
			    ", turbo boost" : "");
		}

		method = sc->sc_tstate_control.reg_spaceid;

		for (i = 0; i < sc->sc_tstate_count; i++) {

			ts = &sc->sc_tstate[i];

			if (ts->ts_percent == 0)
				continue;

			aprint_verbose_dev(sc->sc_dev, "T%u: %3s, "
			    "lat %3u us, pow %5u mW, %3u %%\n", i,
			    acpicpu_debug_print_method_pt(method),
			    ts->ts_latency, ts->ts_power, ts->ts_percent);
		}

		once = true;
	}

	aprint_debug_dev(sc->sc_dev, "id %u, lapic id %u, "
	    "cap 0x%04x, flags 0x%08x\n", ci->ci_acpiid,
	    (uint32_t)ci->ci_cpuid, sc->sc_cap, sc->sc_flags);

	if ((sc->sc_flags & ACPICPU_FLAG_C_DEP) != 0) {

		dep = &sc->sc_cstate_dep;

		aprint_debug_dev(sc->sc_dev, "C-state coordination: "
		    "%u CPUs, domain %u, type %s\n", dep->dep_ncpus,
		    dep->dep_domain, acpicpu_debug_print_dep(dep->dep_type));
	}

	if ((sc->sc_flags & ACPICPU_FLAG_P_DEP) != 0) {

		dep = &sc->sc_pstate_dep;

		aprint_debug_dev(sc->sc_dev, "P-state coordination: "
		    "%u CPUs, domain %u, type %s\n", dep->dep_ncpus,
		    dep->dep_domain, acpicpu_debug_print_dep(dep->dep_type));
	}

	if ((sc->sc_flags & ACPICPU_FLAG_T_DEP) != 0) {

		dep = &sc->sc_tstate_dep;

		aprint_debug_dev(sc->sc_dev, "T-state coordination: "
		    "%u CPUs, domain %u, type %s\n", dep->dep_ncpus,
		    dep->dep_domain, acpicpu_debug_print_dep(dep->dep_type));
	}
}

static const char *
acpicpu_debug_print_method_c(uint8_t val)
{

	if (val == ACPICPU_C_STATE_FFH)
		return "FFH";

	if (val == ACPICPU_C_STATE_HALT)
		return "HLT";

	if (val == ACPICPU_C_STATE_SYSIO)
		return "I/O";

	return "???";
}

static const char *
acpicpu_debug_print_method_pt(uint8_t val)
{

	if (val == ACPI_ADR_SPACE_SYSTEM_IO)
		return "I/O";

	if (val == ACPI_ADR_SPACE_FIXED_HARDWARE)
		return "FFH";

	return "???";
}

static const char *
acpicpu_debug_print_dep(uint32_t val)
{

	switch (val) {

	case ACPICPU_DEP_SW_ALL:
		return "SW_ALL";

	case ACPICPU_DEP_SW_ANY:
		return "SW_ANY";

	case ACPICPU_DEP_HW_ALL:
		return "HW_ALL";

	default:
		return "unknown";
	}
}

MODULE(MODULE_CLASS_DRIVER, acpicpu, NULL);

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
acpicpu_modcmd(modcmd_t cmd, void *aux)
{
	int rv = 0;

	switch (cmd) {

	case MODULE_CMD_INIT:

#ifdef _MODULE
		rv = config_init_component(cfdriver_ioconf_acpicpu,
		    cfattach_ioconf_acpicpu, cfdata_ioconf_acpicpu);
#endif
		break;

	case MODULE_CMD_FINI:

#ifdef _MODULE
		rv = config_fini_component(cfdriver_ioconf_acpicpu,
		    cfattach_ioconf_acpicpu, cfdata_ioconf_acpicpu);
#endif
		break;

	default:
		rv = ENOTTY;
	}

	return rv;
}

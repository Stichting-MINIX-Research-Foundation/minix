/*	$NetBSD: acpi.c,v 1.261 2015/10/02 05:22:52 msaitoh Exp $	*/

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
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: acpi.c,v 1.261 2015/10/02 05:22:52 msaitoh Exp $");

#include "opt_acpi.h"
#include "opt_pcifixup.h"

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/timetc.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_mcfg.h>
#include <dev/acpi/acpi_osd.h>
#include <dev/acpi/acpi_pci.h>
#include <dev/acpi/acpi_power.h>
#include <dev/acpi/acpi_timer.h>
#include <dev/acpi/acpi_wakedev.h>

#include <machine/acpi_machdep.h>

#define _COMPONENT	ACPI_BUS_COMPONENT
ACPI_MODULE_NAME	("acpi")

/*
 * The acpi_active variable is set when the ACPI subsystem is active.
 * Machine-dependent code may wish to skip other steps (such as attaching
 * subsystems that ACPI supercedes) when ACPI is active.
 */
int		acpi_active = 0;
int		acpi_suspended = 0;
int		acpi_force_load = 0;
int		acpi_verbose_loaded = 0;

struct acpi_softc	*acpi_softc = NULL;
static uint64_t		 acpi_root_pointer;
extern kmutex_t		 acpi_interrupt_list_mtx;
extern struct		 cfdriver acpi_cd;
static ACPI_HANDLE	 acpi_scopes[4];
ACPI_TABLE_HEADER	*madt_header;

/*
 * This structure provides a context for the ACPI
 * namespace walk performed in acpi_build_tree().
 */
struct acpi_walkcontext {
	struct acpi_softc	*aw_sc;
	struct acpi_devnode	*aw_parent;
};

/*
 * Ignored HIDs.
 */
static const char * const acpi_ignored_ids[] = {
#if defined(i386) || defined(x86_64)
	"ACPI0007",	/* ACPI CPUs do not attach to acpi(4) */
	"PNP0000",	/* AT interrupt controller is handled internally */
	"PNP0200",	/* AT DMA controller is handled internally */
	"PNP0A??",	/* PCI Busses are handled internally */
	"PNP0B00",	/* AT RTC is handled internally */
	"PNP0C0F",	/* ACPI PCI link devices are handled internally */
#endif
#if defined(x86_64)
	"PNP0C04",	/* FPU is handled internally */
#endif
	NULL
};

/*
 * Devices that should be attached early.
 */
static const char * const acpi_early_ids[] = {
	"PNP0C09",	/* acpiec(4) */
	NULL
};

static int		acpi_match(device_t, cfdata_t, void *);
static int		acpi_submatch(device_t, cfdata_t, const int *, void *);
static void		acpi_attach(device_t, device_t, void *);
static int		acpi_detach(device_t, int);
static void		acpi_childdet(device_t, device_t);
static bool		acpi_suspend(device_t, const pmf_qual_t *);
static bool		acpi_resume(device_t, const pmf_qual_t *);

static void		acpi_build_tree(struct acpi_softc *);
static void		acpi_config_tree(struct acpi_softc *);
static ACPI_STATUS	acpi_make_devnode(ACPI_HANDLE, uint32_t,
					  void *, void **);
static ACPI_STATUS	acpi_make_devnode_post(ACPI_HANDLE, uint32_t,
					       void *, void **);
static void		acpi_make_name(struct acpi_devnode *, uint32_t);

static int		acpi_rescan(device_t, const char *, const int *);
static void		acpi_rescan_early(struct acpi_softc *);
static void		acpi_rescan_nodes(struct acpi_softc *);
static void		acpi_rescan_capabilities(device_t);
static int		acpi_print(void *aux, const char *);

static void		acpi_notify_handler(ACPI_HANDLE, uint32_t, void *);

static void		acpi_register_fixed_button(struct acpi_softc *, int);
static void		acpi_deregister_fixed_button(struct acpi_softc *, int);
static uint32_t		acpi_fixed_button_handler(void *);
static void		acpi_fixed_button_pressed(void *);

static void		acpi_sleep_init(struct acpi_softc *);

static int		sysctl_hw_acpi_fixedstats(SYSCTLFN_PROTO);
static int		sysctl_hw_acpi_sleepstate(SYSCTLFN_PROTO);
static int		sysctl_hw_acpi_sleepstates(SYSCTLFN_PROTO);

static bool		  acpi_is_scope(struct acpi_devnode *);
static ACPI_TABLE_HEADER *acpi_map_rsdt(void);
static void		  acpi_unmap_rsdt(ACPI_TABLE_HEADER *);

void			acpi_print_verbose_stub(struct acpi_softc *);
void			acpi_print_dev_stub(const char *);

static void		acpi_activate_device(ACPI_HANDLE, ACPI_DEVICE_INFO **);
ACPI_STATUS		acpi_allocate_resources(ACPI_HANDLE);

void (*acpi_print_verbose)(struct acpi_softc *) = acpi_print_verbose_stub;
void (*acpi_print_dev)(const char *) = acpi_print_dev_stub;

CFATTACH_DECL2_NEW(acpi, sizeof(struct acpi_softc),
    acpi_match, acpi_attach, acpi_detach, NULL, acpi_rescan, acpi_childdet);

/*
 * Probe for ACPI support.
 *
 * This is called by the machine-dependent ACPI front-end.
 * Note: this is not an autoconfiguration interface function.
 */
int
acpi_probe(void)
{
	ACPI_TABLE_HEADER *rsdt;
	ACPI_STATUS rv;
	int quirks;

	if (acpi_softc != NULL)
		panic("%s: already probed", __func__);

	mutex_init(&acpi_interrupt_list_mtx, MUTEX_DEFAULT, IPL_NONE);

	/*
	 * Start up ACPICA.
	 */
	AcpiGbl_EnableInterpreterSlack = true;

	rv = AcpiInitializeSubsystem();

	if (ACPI_FAILURE(rv)) {
		aprint_error("%s: failed to initialize subsystem\n", __func__);
		return 0;
	}

	/*
	 * Allocate space for RSDT/XSDT and DSDT,
	 * but allow resizing if more tables exist.
	 */
	rv = AcpiInitializeTables(NULL, 2, true);

	if (ACPI_FAILURE(rv)) {
		aprint_error("%s: failed to initialize tables\n", __func__);
		goto fail;
	}

	rv = AcpiLoadTables();

	if (ACPI_FAILURE(rv)) {
		aprint_error("%s: failed to load tables\n", __func__);
		goto fail;
	}

	rsdt = acpi_map_rsdt();

	if (rsdt == NULL) {
		aprint_error("%s: failed to map RSDT\n", __func__);
		goto fail;
	}

	quirks = acpi_find_quirks();

	if (acpi_force_load == 0 && (quirks & ACPI_QUIRK_BROKEN) != 0) {

		aprint_normal("ACPI: BIOS is listed as broken:\n");
		aprint_normal("ACPI: X/RSDT: OemId <%6.6s,%8.8s,%08x>, "
		       "AslId <%4.4s,%08x>\n", rsdt->OemId, rsdt->OemTableId,
		        rsdt->OemRevision, rsdt->AslCompilerId,
		        rsdt->AslCompilerRevision);
		aprint_normal("ACPI: Not used. Set acpi_force_load to use.\n");

		acpi_unmap_rsdt(rsdt);
		goto fail;
	}

	if (acpi_force_load == 0 && (quirks & ACPI_QUIRK_OLDBIOS) != 0) {

		aprint_normal("ACPI: BIOS is too old (%s). "
		    "Set acpi_force_load to use.\n",
		    pmf_get_platform("bios-date"));

		acpi_unmap_rsdt(rsdt);
		goto fail;
	}

	acpi_unmap_rsdt(rsdt);

	rv = AcpiEnableSubsystem(~(ACPI_NO_HARDWARE_INIT|ACPI_NO_ACPI_ENABLE));

	if (ACPI_FAILURE(rv)) {
		aprint_error("%s: failed to enable subsystem\n", __func__);
		goto fail;
	}

	return 1;

fail:
	(void)AcpiTerminate();

	return 0;
}

void
acpi_disable(void)
{

	if (acpi_softc == NULL)
		return;

	KASSERT(acpi_active != 0);

	if (AcpiGbl_FADT.SmiCommand != 0)
		AcpiDisable();
}

int
acpi_check(device_t parent, const char *ifattr)
{
	return (config_search_ia(acpi_submatch, parent, ifattr, NULL) != NULL);
}

int
acpi_reset(void)
{
	struct acpi_softc *sc = acpi_softc;
	ACPI_GENERIC_ADDRESS *ResetReg;
	ACPI_PCI_ID PciId;
	ACPI_STATUS status;

	if (sc == NULL)
		return ENXIO;

	ResetReg = &AcpiGbl_FADT.ResetRegister;

	/* Check if the reset register is supported */
	if (!(AcpiGbl_FADT.Flags & ACPI_FADT_RESET_REGISTER) ||
	    !ResetReg->Address) {
		return ENOENT;
	}

	switch (ResetReg->SpaceId) {
	case ACPI_ADR_SPACE_PCI_CONFIG:
		PciId.Segment = PciId.Bus = 0;
		PciId.Device = ACPI_GAS_PCI_DEV(ResetReg->Address);
		PciId.Function = ACPI_GAS_PCI_FUNC(ResetReg->Address);
		status = AcpiOsWritePciConfiguration(&PciId,
		    ACPI_GAS_PCI_REGOFF(ResetReg->Address),
		    AcpiGbl_FADT.ResetValue, ResetReg->BitWidth);
		break;
	case ACPI_ADR_SPACE_SYSTEM_IO:
	case ACPI_ADR_SPACE_SYSTEM_MEMORY:
		status = AcpiReset();
		break;
	default:
		status = AE_TYPE;
		break;
	}

	return ACPI_FAILURE(status) ? EIO : 0;
}

/*
 * Autoconfiguration.
 */
static int
acpi_match(device_t parent, cfdata_t match, void *aux)
{
	/*
	 * XXX: Nada; MD code has called acpi_probe().
	 */
	return 1;
}

static int
acpi_submatch(device_t parent, cfdata_t cf, const int *locs, void *aux)
{
	struct cfattach *ca;

	ca = config_cfattach_lookup(cf->cf_name, cf->cf_atname);

	return (ca == &acpi_ca);
}

static void
acpi_attach(device_t parent, device_t self, void *aux)
{
	struct acpi_softc *sc = device_private(self);
	struct acpibus_attach_args *aa = aux;
	ACPI_TABLE_HEADER *rsdt;
	ACPI_STATUS rv;

	aprint_naive("\n");
	aprint_normal(": Intel ACPICA %08x\n", ACPI_CA_VERSION);

	if (acpi_softc != NULL)
		panic("%s: already attached", __func__);

	rsdt = acpi_map_rsdt();

	if (rsdt == NULL)
		aprint_error_dev(self, "X/RSDT: Not found\n");
	else {
		aprint_verbose_dev(self,
		    "X/RSDT: OemId <%6.6s,%8.8s,%08x>, AslId <%4.4s,%08x>\n",
		    rsdt->OemId, rsdt->OemTableId,
		    rsdt->OemRevision,
		    rsdt->AslCompilerId, rsdt->AslCompilerRevision);
	}

	acpi_unmap_rsdt(rsdt);

	sc->sc_dev = self;
	sc->sc_root = NULL;

	sc->sc_sleepstate = ACPI_STATE_S0;
	sc->sc_quirks = acpi_find_quirks();

	sysmon_power_settype("acpi");

	sc->sc_iot = aa->aa_iot;
	sc->sc_memt = aa->aa_memt;
	sc->sc_pc = aa->aa_pc;
	sc->sc_pciflags = aa->aa_pciflags;
	sc->sc_ic = aa->aa_ic;

	SIMPLEQ_INIT(&sc->ad_head);

	acpi_softc = sc;

	if (pmf_device_register(self, acpi_suspend, acpi_resume) != true)
		aprint_error_dev(self, "couldn't establish power handler\n");

	/*
	 * Bring ACPICA on-line.
	 */

	rv = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);

	if (ACPI_FAILURE(rv))
		goto fail;

	/*
	 * Early initialization of acpiec(4) via ECDT.
	 */
	(void)config_found_ia(self, "acpiecdtbus", aa, NULL);

	rv = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);

	if (ACPI_FAILURE(rv))
		goto fail;

	/*
	 * Scan the namespace and build our device tree.
	 */
	acpi_build_tree(sc);

	/*
	 * Probe MCFG table
	 */
	acpimcfg_probe(sc);

	acpi_md_callback(sc);

	/*
	 * Early initialization of the _PDC control method
	 * that may load additional SSDT tables dynamically.
	 */
	(void)acpi_md_pdc();

	/*
	 * Install global notify handlers.
	 */
	rv = AcpiInstallNotifyHandler(ACPI_ROOT_OBJECT,
	    ACPI_SYSTEM_NOTIFY, acpi_notify_handler, NULL);

	if (ACPI_FAILURE(rv))
		goto fail;

	rv = AcpiInstallNotifyHandler(ACPI_ROOT_OBJECT,
	    ACPI_DEVICE_NOTIFY, acpi_notify_handler, NULL);

	if (ACPI_FAILURE(rv))
		goto fail;

	acpi_active = 1;

	/* Show SCI interrupt. */
	aprint_verbose_dev(self, "SCI interrupting at int %u\n",
	    AcpiGbl_FADT.SciInterrupt);

	/*
	 * Install fixed-event handlers.
	 */
	acpi_register_fixed_button(sc, ACPI_EVENT_POWER_BUTTON);
	acpi_register_fixed_button(sc, ACPI_EVENT_SLEEP_BUTTON);

	acpitimer_init(sc);
	acpi_config_tree(sc);
	acpi_sleep_init(sc);

#ifdef ACPI_DEBUG
	acpi_debug_init();
#endif

	/*
	 * Print debug information.
	 */
	acpi_print_verbose(sc);

	return;

fail:
	aprint_error("%s: failed to initialize ACPI: %s\n",
	    __func__, AcpiFormatException(rv));
}

/*
 * XXX: This is incomplete.
 */
static int
acpi_detach(device_t self, int flags)
{
	struct acpi_softc *sc = device_private(self);
	ACPI_STATUS rv;
	int rc;

	rv = AcpiRemoveNotifyHandler(ACPI_ROOT_OBJECT,
	    ACPI_SYSTEM_NOTIFY, acpi_notify_handler);

	if (ACPI_FAILURE(rv))
		return EBUSY;

	rv = AcpiRemoveNotifyHandler(ACPI_ROOT_OBJECT,
	    ACPI_DEVICE_NOTIFY, acpi_notify_handler);

	if (ACPI_FAILURE(rv))
		return EBUSY;

	if ((rc = config_detach_children(self, flags)) != 0)
		return rc;

	if ((rc = acpitimer_detach()) != 0)
		return rc;

	acpi_deregister_fixed_button(sc, ACPI_EVENT_POWER_BUTTON);
	acpi_deregister_fixed_button(sc, ACPI_EVENT_SLEEP_BUTTON);

	pmf_device_deregister(self);

	acpi_softc = NULL;

	return 0;
}

static void
acpi_childdet(device_t self, device_t child)
{
	struct acpi_softc *sc = device_private(self);
	struct acpi_devnode *ad;

	if (sc->sc_apmbus == child)
		sc->sc_apmbus = NULL;

	if (sc->sc_hpet == child)
		sc->sc_hpet = NULL;

	if (sc->sc_wdrt == child)
		sc->sc_wdrt = NULL;

	SIMPLEQ_FOREACH(ad, &sc->ad_head, ad_list) {

		if (ad->ad_device == child)
			ad->ad_device = NULL;
	}
}

static bool
acpi_suspend(device_t dv, const pmf_qual_t *qual)
{

	acpi_suspended = 1;

	return true;
}

static bool
acpi_resume(device_t dv, const pmf_qual_t *qual)
{

	acpi_suspended = 0;

	return true;
}

/*
 * Namespace scan.
 */
static void
acpi_build_tree(struct acpi_softc *sc)
{
	struct acpi_walkcontext awc;

	/*
	 * Get the root scope handles.
	 */
	KASSERT(__arraycount(acpi_scopes) == 4);

	(void)AcpiGetHandle(ACPI_ROOT_OBJECT, "\\_PR_", &acpi_scopes[0]);
	(void)AcpiGetHandle(ACPI_ROOT_OBJECT, "\\_SB_", &acpi_scopes[1]);
	(void)AcpiGetHandle(ACPI_ROOT_OBJECT, "\\_SI_", &acpi_scopes[2]);
	(void)AcpiGetHandle(ACPI_ROOT_OBJECT, "\\_TZ_", &acpi_scopes[3]);

	/*
	 * Make the root node.
	 */
	awc.aw_sc = sc;
	awc.aw_parent = NULL;

	(void)acpi_make_devnode(ACPI_ROOT_OBJECT, 0, &awc, NULL);

	KASSERT(sc->sc_root == NULL);
	KASSERT(awc.aw_parent != NULL);

	sc->sc_root = awc.aw_parent;

	/*
	 * Build the internal namespace.
	 */
	(void)AcpiWalkNamespace(ACPI_TYPE_ANY, ACPI_ROOT_OBJECT, UINT32_MAX,
	    acpi_make_devnode, acpi_make_devnode_post, &awc, NULL);

	/*
	 * Scan the internal namespace.
	 */
	(void)acpi_pcidev_scan(sc->sc_root);
}

static void
acpi_config_tree(struct acpi_softc *sc)
{

	/*
	 * Configure all everything found "at acpi?".
	 */
	(void)acpi_rescan(sc->sc_dev, NULL, NULL);

	/*
	 * Update GPE information.
	 *
	 * Note that this must be called after
	 * all GPE handlers have been installed.
	 */
	(void)AcpiUpdateAllGpes();

	/*
	 * Defer rest of the configuration.
	 */
	(void)config_defer(sc->sc_dev, acpi_rescan_capabilities);
}

static ACPI_STATUS
acpi_make_devnode(ACPI_HANDLE handle, uint32_t level,
    void *context, void **status)
{
	struct acpi_walkcontext *awc = context;
	struct acpi_softc *sc = awc->aw_sc;
	struct acpi_devnode *ad;
	ACPI_DEVICE_INFO *devinfo;
	ACPI_OBJECT_TYPE type;
	ACPI_STATUS rv;

	rv = AcpiGetObjectInfo(handle, &devinfo);

	if (ACPI_FAILURE(rv))
		return AE_OK;	/* Do not terminate the walk. */

	type = devinfo->Type;

	switch (type) {

	case ACPI_TYPE_DEVICE:
		acpi_activate_device(handle, &devinfo);
	case ACPI_TYPE_PROCESSOR:
	case ACPI_TYPE_THERMAL:
	case ACPI_TYPE_POWER:

		ad = kmem_zalloc(sizeof(*ad), KM_SLEEP);

		if (ad == NULL)
			return AE_NO_MEMORY;

		ad->ad_device = NULL;
		ad->ad_notify = NULL;
		ad->ad_pciinfo = NULL;
		ad->ad_wakedev = NULL;

		ad->ad_type = type;
		ad->ad_handle = handle;
		ad->ad_devinfo = devinfo;

		ad->ad_root = sc->sc_dev;
		ad->ad_parent = awc->aw_parent;

		acpi_match_node_init(ad);
		acpi_make_name(ad, devinfo->Name);

		/*
		 * Identify wake GPEs from the _PRW. Note that
		 * AcpiUpdateAllGpes() must be called afterwards.
		 */
		if (ad->ad_devinfo->Type == ACPI_TYPE_DEVICE)
			acpi_wakedev_init(ad);

		SIMPLEQ_INIT(&ad->ad_child_head);
		SIMPLEQ_INSERT_TAIL(&sc->ad_head, ad, ad_list);

		if (ad->ad_parent != NULL) {

			SIMPLEQ_INSERT_TAIL(&ad->ad_parent->ad_child_head,
			    ad, ad_child_list);
		}

		awc->aw_parent = ad;
	}

	return AE_OK;
}

static ACPI_STATUS
acpi_make_devnode_post(ACPI_HANDLE handle, uint32_t level,
    void *context, void **status)
{
	struct acpi_walkcontext *awc = context;

	KASSERT(awc != NULL);
	KASSERT(awc->aw_parent != NULL);

	if (handle == awc->aw_parent->ad_handle)
		awc->aw_parent = awc->aw_parent->ad_parent;

	return AE_OK;
}

static void
acpi_make_name(struct acpi_devnode *ad, uint32_t name)
{
	ACPI_NAME_UNION *anu;
	int clear, i;

	anu = (ACPI_NAME_UNION *)&name;
	ad->ad_name[4] = '\0';

	for (i = 3, clear = 0; i >= 0; i--) {

		if (clear == 0 && anu->Ascii[i] == '_')
			ad->ad_name[i] = '\0';
		else {
			ad->ad_name[i] = anu->Ascii[i];
			clear = 1;
		}
	}

	if (ad->ad_name[0] == '\0')
		ad->ad_name[0] = '_';
}

/*
 * Device attachment.
 */
static int
acpi_rescan(device_t self, const char *ifattr, const int *locators)
{
	struct acpi_softc *sc = device_private(self);
	struct acpi_attach_args aa;

	/*
	 * Try to attach hpet(4) first via a specific table.
	 */
	aa.aa_memt = sc->sc_memt;

	if (ifattr_match(ifattr, "acpihpetbus") && sc->sc_hpet == NULL)
		sc->sc_hpet = config_found_ia(sc->sc_dev,
		    "acpihpetbus", &aa, NULL);

	/*
	 * A two-pass scan for acpinodebus.
	 */
	if (ifattr_match(ifattr, "acpinodebus")) {
		acpi_rescan_early(sc);
		acpi_rescan_nodes(sc);
	}

	/*
	 * Attach APM emulation and acpiwdrt(4).
	 */
	if (ifattr_match(ifattr, "acpiapmbus") && sc->sc_apmbus == NULL)
		sc->sc_apmbus = config_found_ia(sc->sc_dev,
		    "acpiapmbus", NULL, NULL);

	if (ifattr_match(ifattr, "acpiwdrtbus") && sc->sc_wdrt == NULL)
		sc->sc_wdrt = config_found_ia(sc->sc_dev,
		    "acpiwdrtbus", NULL, NULL);

	return 0;
}

static void
acpi_rescan_early(struct acpi_softc *sc)
{
	struct acpi_attach_args aa;
	struct acpi_devnode *ad;

	/*
	 * First scan for devices such as acpiec(4) that
	 * should be always attached before anything else.
	 * We want these devices to attach regardless of
	 * the device status and other restrictions.
	 */
	SIMPLEQ_FOREACH(ad, &sc->ad_head, ad_list) {

		if (ad->ad_device != NULL)
			continue;

		if (ad->ad_devinfo->Type != ACPI_TYPE_DEVICE)
			continue;

		if (acpi_match_hid(ad->ad_devinfo, acpi_early_ids) == 0)
			continue;

		aa.aa_node = ad;
		aa.aa_iot = sc->sc_iot;
		aa.aa_memt = sc->sc_memt;
		aa.aa_pc = sc->sc_pc;
		aa.aa_pciflags = sc->sc_pciflags;
		aa.aa_ic = sc->sc_ic;

		ad->ad_device = config_found_ia(sc->sc_dev,
		    "acpinodebus", &aa, acpi_print);
	}
}

static void
acpi_rescan_nodes(struct acpi_softc *sc)
{
	const char * const hpet_ids[] = { "PNP0103", NULL };
	struct acpi_attach_args aa;
	struct acpi_devnode *ad;
	ACPI_DEVICE_INFO *di;

	SIMPLEQ_FOREACH(ad, &sc->ad_head, ad_list) {

		if (ad->ad_device != NULL)
			continue;

		/*
		 * There is a bug in ACPICA: it defines the type
		 * of the scopes incorrectly for its own reasons.
		 */
		if (acpi_is_scope(ad) != false)
			continue;

		di = ad->ad_devinfo;

		/*
		 * We only attach devices which are present, enabled, and
		 * functioning properly. However, if a device is enabled,
		 * it is decoding resources and we should claim these,
		 * if possible. This requires changes to bus_space(9).
		 * Note: there is a possible race condition, because _STA
		 * may have changed since di->CurrentStatus was set.
		 */
		if (di->Type == ACPI_TYPE_DEVICE) {

			if ((di->Valid & ACPI_VALID_STA) != 0 &&
			    (di->CurrentStatus & ACPI_STA_OK) != ACPI_STA_OK)
				continue;
		}

		if (di->Type == ACPI_TYPE_POWER)
			continue;

		if (di->Type == ACPI_TYPE_PROCESSOR)
			continue;

		if (acpi_match_hid(di, acpi_early_ids) != 0)
			continue;

		if (acpi_match_hid(di, acpi_ignored_ids) != 0)
			continue;

		if (acpi_match_hid(di, hpet_ids) != 0 && sc->sc_hpet != NULL)
			continue;

		aa.aa_node = ad;
		aa.aa_iot = sc->sc_iot;
		aa.aa_memt = sc->sc_memt;
		aa.aa_pc = sc->sc_pc;
		aa.aa_pciflags = sc->sc_pciflags;
		aa.aa_ic = sc->sc_ic;

		ad->ad_device = config_found_ia(sc->sc_dev,
		    "acpinodebus", &aa, acpi_print);
	}
}

static void
acpi_rescan_capabilities(device_t self)
{
	struct acpi_softc *sc = device_private(self);
	struct acpi_devnode *ad;
	ACPI_HANDLE tmp;
	ACPI_STATUS rv;

	SIMPLEQ_FOREACH(ad, &sc->ad_head, ad_list) {

		if (ad->ad_devinfo->Type != ACPI_TYPE_DEVICE)
			continue;

		/*
		 * Scan power resource capabilities.
		 *
		 * If any power states are supported,
		 * at least _PR0 and _PR3 must be present.
		 */
		rv = AcpiGetHandle(ad->ad_handle, "_PR0", &tmp);

		if (ACPI_SUCCESS(rv)) {
			ad->ad_flags |= ACPI_DEVICE_POWER;
			acpi_power_add(ad);
		}

		/*
		 * Scan wake-up capabilities.
		 */
		if (ad->ad_wakedev != NULL) {
			ad->ad_flags |= ACPI_DEVICE_WAKEUP;
			acpi_wakedev_add(ad);
		}

		/*
		 * Scan docking stations.
		 */
		rv = AcpiGetHandle(ad->ad_handle, "_DCK", &tmp);

		if (ACPI_SUCCESS(rv))
			ad->ad_flags |= ACPI_DEVICE_DOCK;

		/*
		 * Scan devices that are ejectable.
		 */
		rv = AcpiGetHandle(ad->ad_handle, "_EJ0", &tmp);

		if (ACPI_SUCCESS(rv))
			ad->ad_flags |= ACPI_DEVICE_EJECT;
	}
}

static int
acpi_print(void *aux, const char *pnp)
{
	struct acpi_attach_args *aa = aux;
	struct acpi_devnode *ad;
	const char *hid, *uid;
	ACPI_DEVICE_INFO *di;

	ad = aa->aa_node;
	di = ad->ad_devinfo;

	hid = di->HardwareId.String;
	uid = di->UniqueId.String;

	if (pnp != NULL) {

		if (di->Type != ACPI_TYPE_DEVICE) {

			aprint_normal("%s (ACPI Object Type '%s') at %s",
			    ad->ad_name, AcpiUtGetTypeName(ad->ad_type), pnp);

			return UNCONF;
		}

		if ((di->Valid & ACPI_VALID_HID) == 0 || hid == NULL)
			return 0;

		aprint_normal("%s (%s) ", ad->ad_name, hid);
		acpi_print_dev(hid);
		aprint_normal("at %s", pnp);

		return UNCONF;
	}

	aprint_normal(" (%s", ad->ad_name);

	if ((di->Valid & ACPI_VALID_HID) != 0 && hid != NULL) {

		aprint_normal(", %s", hid);

		if ((di->Valid & ACPI_VALID_UID) != 0 && uid != NULL) {

			if (uid[0] == '\0')
				uid = "<null>";

			aprint_normal("-%s", uid);
		}
	}

	aprint_normal(")");

	return UNCONF;
}

/*
 * Notify.
 */
static void
acpi_notify_handler(ACPI_HANDLE handle, uint32_t event, void *aux)
{
	struct acpi_softc *sc = acpi_softc;
	struct acpi_devnode *ad;

	KASSERT(sc != NULL);
	KASSERT(aux == NULL);
	KASSERT(acpi_active != 0);

	if (acpi_suspended != 0)
		return;

	/*
	 *  System: 0x00 - 0x7F.
	 *  Device: 0x80 - 0xFF.
	 */
	switch (event) {

	case ACPI_NOTIFY_BUS_CHECK:
	case ACPI_NOTIFY_DEVICE_CHECK:
	case ACPI_NOTIFY_DEVICE_WAKE:
	case ACPI_NOTIFY_EJECT_REQUEST:
	case ACPI_NOTIFY_DEVICE_CHECK_LIGHT:
	case ACPI_NOTIFY_FREQUENCY_MISMATCH:
	case ACPI_NOTIFY_BUS_MODE_MISMATCH:
	case ACPI_NOTIFY_POWER_FAULT:
	case ACPI_NOTIFY_CAPABILITIES_CHECK:
	case ACPI_NOTIFY_DEVICE_PLD_CHECK:
	case ACPI_NOTIFY_RESERVED:
	case ACPI_NOTIFY_LOCALITY_UPDATE:
		break;
	}

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "notification 0x%02X for "
		"%s (%p)\n", event, acpi_name(handle), handle));

	/*
	 * We deliver notifications only to drivers
	 * that have been successfully attached and
	 * that have registered a handler with us.
	 * The opaque pointer is always the device_t.
	 */
	SIMPLEQ_FOREACH(ad, &sc->ad_head, ad_list) {

		if (ad->ad_device == NULL)
			continue;

		if (ad->ad_notify == NULL)
			continue;

		if (ad->ad_handle != handle)
			continue;

		(*ad->ad_notify)(ad->ad_handle, event, ad->ad_device);

		return;
	}

	aprint_debug_dev(sc->sc_dev, "unhandled notify 0x%02X "
	    "for %s (%p)\n", event, acpi_name(handle), handle);
}

bool
acpi_register_notify(struct acpi_devnode *ad, ACPI_NOTIFY_HANDLER notify)
{
	struct acpi_softc *sc = acpi_softc;

	KASSERT(sc != NULL);
	KASSERT(acpi_active != 0);

	if (acpi_suspended != 0)
		goto fail;

	if (ad == NULL || notify == NULL)
		goto fail;

	ad->ad_notify = notify;

	return true;

fail:
	aprint_error_dev(sc->sc_dev, "failed to register notify "
	    "handler for %s (%p)\n", ad->ad_name, ad->ad_handle);

	return false;
}

void
acpi_deregister_notify(struct acpi_devnode *ad)
{

	ad->ad_notify = NULL;
}

/*
 * Fixed buttons.
 */
static void
acpi_register_fixed_button(struct acpi_softc *sc, int event)
{
	struct sysmon_pswitch *smpsw;
	ACPI_STATUS rv;
	int type;

	switch (event) {

	case ACPI_EVENT_POWER_BUTTON:

		if ((AcpiGbl_FADT.Flags & ACPI_FADT_POWER_BUTTON) != 0)
			return;

		type = PSWITCH_TYPE_POWER;
		smpsw = &sc->sc_smpsw_power;
		break;

	case ACPI_EVENT_SLEEP_BUTTON:

		if ((AcpiGbl_FADT.Flags & ACPI_FADT_SLEEP_BUTTON) != 0)
			return;

		type = PSWITCH_TYPE_SLEEP;
		smpsw = &sc->sc_smpsw_sleep;
		break;

	default:
		rv = AE_TYPE;
		goto fail;
	}

	smpsw->smpsw_type = type;
	smpsw->smpsw_name = device_xname(sc->sc_dev);

	if (sysmon_pswitch_register(smpsw) != 0) {
		rv = AE_ERROR;
		goto fail;
	}

	AcpiClearEvent(event);

	rv = AcpiInstallFixedEventHandler(event,
	    acpi_fixed_button_handler, smpsw);

	if (ACPI_FAILURE(rv)) {
		sysmon_pswitch_unregister(smpsw);
		goto fail;
	}

	aprint_debug_dev(sc->sc_dev, "fixed %s button present\n",
	    (type != ACPI_EVENT_SLEEP_BUTTON) ? "power" : "sleep");

	return;

fail:
	aprint_error_dev(sc->sc_dev, "failed to register "
	    "fixed event %d: %s\n", event, AcpiFormatException(rv));
}

static void
acpi_deregister_fixed_button(struct acpi_softc *sc, int event)
{
	struct sysmon_pswitch *smpsw;
	ACPI_STATUS rv;

	switch (event) {

	case ACPI_EVENT_POWER_BUTTON:
		smpsw = &sc->sc_smpsw_power;

		if ((AcpiGbl_FADT.Flags & ACPI_FADT_POWER_BUTTON) != 0) {
			KASSERT(smpsw->smpsw_type != PSWITCH_TYPE_POWER);
			return;
		}

		break;

	case ACPI_EVENT_SLEEP_BUTTON:
		smpsw = &sc->sc_smpsw_sleep;

		if ((AcpiGbl_FADT.Flags & ACPI_FADT_SLEEP_BUTTON) != 0) {
			KASSERT(smpsw->smpsw_type != PSWITCH_TYPE_SLEEP);
			return;
		}

		break;

	default:
		rv = AE_TYPE;
		goto fail;
	}

	rv = AcpiRemoveFixedEventHandler(event, acpi_fixed_button_handler);

	if (ACPI_SUCCESS(rv)) {
		sysmon_pswitch_unregister(smpsw);
		return;
	}

fail:
	aprint_error_dev(sc->sc_dev, "failed to deregister "
	    "fixed event: %s\n", AcpiFormatException(rv));
}

static uint32_t
acpi_fixed_button_handler(void *context)
{
	static const int handler = OSL_NOTIFY_HANDLER;
	struct sysmon_pswitch *smpsw = context;

	(void)AcpiOsExecute(handler, acpi_fixed_button_pressed, smpsw);

	return ACPI_INTERRUPT_HANDLED;
}

static void
acpi_fixed_button_pressed(void *context)
{
	struct sysmon_pswitch *smpsw = context;

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "%s fixed button pressed\n",
		(smpsw->smpsw_type != ACPI_EVENT_SLEEP_BUTTON) ?
		"power" : "sleep"));

	sysmon_pswitch_event(smpsw, PSWITCH_EVENT_PRESSED);
}

/*
 * Sleep.
 */
static void
acpi_sleep_init(struct acpi_softc *sc)
{
	uint8_t a, b, i;
	ACPI_STATUS rv;

	CTASSERT(ACPI_STATE_S0 == 0 && ACPI_STATE_S1 == 1);
	CTASSERT(ACPI_STATE_S2 == 2 && ACPI_STATE_S3 == 3);
	CTASSERT(ACPI_STATE_S4 == 4 && ACPI_STATE_S5 == 5);

	/*
	 * Evaluate supported sleep states.
	 */
	for (i = ACPI_STATE_S0; i <= ACPI_STATE_S5; i++) {

		rv = AcpiGetSleepTypeData(i, &a, &b);

		if (ACPI_SUCCESS(rv))
			sc->sc_sleepstates |= __BIT(i);
	}
}

/*
 * Must be called with interrupts enabled.
 */
void
acpi_enter_sleep_state(int state)
{
	struct acpi_softc *sc = acpi_softc;
	ACPI_STATUS rv;

	if (acpi_softc == NULL)
		return;

	if (state == sc->sc_sleepstate)
		return;

	if (state < ACPI_STATE_S0 || state > ACPI_STATE_S5)
		return;

	aprint_normal_dev(sc->sc_dev, "entering state S%d\n", state);

	switch (state) {

	case ACPI_STATE_S0:
		sc->sc_sleepstate = ACPI_STATE_S0;
		return;

	case ACPI_STATE_S1:
	case ACPI_STATE_S2:
	case ACPI_STATE_S3:
	case ACPI_STATE_S4:

		if ((sc->sc_sleepstates & __BIT(state)) == 0) {
			aprint_error_dev(sc->sc_dev, "sleep state "
			    "S%d is not available\n", state);
			return;
		}

		/*
		 * Evaluate the _TTS method. This should be done before
		 * pmf_system_suspend(9) and the evaluation of _PTS.
		 * We should also re-evaluate this once we return to
		 * S0 or if we abort the sleep state transition in the
		 * middle (see ACPI 3.0, section 7.3.6). In reality,
		 * however, the _TTS method is seldom seen in the field.
		 */
		rv = acpi_eval_set_integer(NULL, "\\_TTS", state);

		if (ACPI_SUCCESS(rv))
			aprint_debug_dev(sc->sc_dev, "evaluated _TTS\n");

		if (state != ACPI_STATE_S1 &&
		    pmf_system_suspend(PMF_Q_NONE) != true) {
			aprint_error_dev(sc->sc_dev, "aborting suspend\n");
			break;
		}

		/*
		 * This will evaluate the  _PTS and _SST methods,
		 * but unlike the documentation claims, not _GTS,
		 * which is evaluated in AcpiEnterSleepState().
		 * This must be called with interrupts enabled.
		 */
		rv = AcpiEnterSleepStatePrep(state);

		if (ACPI_FAILURE(rv)) {
			aprint_error_dev(sc->sc_dev, "failed to prepare "
			    "S%d: %s\n", state, AcpiFormatException(rv));
			break;
		}

		/*
		 * After the _PTS method has been evaluated, we can
		 * enable wake and evaluate _PSW (ACPI 4.0, p. 284).
		 */
		acpi_wakedev_commit(sc, state);

		sc->sc_sleepstate = state;

		if (state == ACPI_STATE_S1) {

			/*
			 * Before the transition to S1, CPU caches
			 * must be flushed (see ACPI 4.0, 7.3.4.2).
			 *
			 * Note that interrupts must be off before
			 * calling AcpiEnterSleepState(). Conversely,
			 * AcpiLeaveSleepState() should always be
			 * called with interrupts enabled.
			 */
			acpi_md_OsDisableInterrupt();

			ACPI_FLUSH_CPU_CACHE();
			rv = AcpiEnterSleepState(state);

			if (ACPI_FAILURE(rv))
				aprint_error_dev(sc->sc_dev, "failed to "
				    "enter S1: %s\n", AcpiFormatException(rv));

			/*
			 * Clear fixed events and disable all GPEs before
			 * interrupts are enabled.
			 */
			AcpiClearEvent(ACPI_EVENT_PMTIMER);
			AcpiClearEvent(ACPI_EVENT_GLOBAL);
			AcpiClearEvent(ACPI_EVENT_POWER_BUTTON);
			AcpiClearEvent(ACPI_EVENT_SLEEP_BUTTON);
			AcpiClearEvent(ACPI_EVENT_RTC);
			AcpiHwDisableAllGpes();

			acpi_md_OsEnableInterrupt();
			rv = AcpiLeaveSleepState(state);

		} else {

			(void)acpi_md_sleep(state);

			if (state == ACPI_STATE_S4)
				AcpiEnable();

			(void)pmf_system_bus_resume(PMF_Q_NONE);
			(void)AcpiLeaveSleepState(state);
			(void)AcpiSetFirmwareWakingVector(0, 0);
			(void)pmf_system_resume(PMF_Q_NONE);
		}

		/*
		 * No wake GPEs should be enabled at runtime.
		 */
		acpi_wakedev_commit(sc, ACPI_STATE_S0);
		break;

	case ACPI_STATE_S5:

		(void)acpi_eval_set_integer(NULL, "\\_TTS", ACPI_STATE_S5);

		rv = AcpiEnterSleepStatePrep(ACPI_STATE_S5);

		if (ACPI_FAILURE(rv)) {
			aprint_error_dev(sc->sc_dev, "failed to prepare "
			    "S%d: %s\n", state, AcpiFormatException(rv));
			break;
		}

		(void)AcpiDisableAllGpes();

		DELAY(1000000);

		sc->sc_sleepstate = state;
		acpi_md_OsDisableInterrupt();

		(void)AcpiEnterSleepState(ACPI_STATE_S5);

		aprint_error_dev(sc->sc_dev, "WARNING: powerdown failed!\n");

		break;
	}

	sc->sc_sleepstate = ACPI_STATE_S0;

	(void)acpi_eval_set_integer(NULL, "\\_TTS", ACPI_STATE_S0);
}

/*
 * Sysctl.
 */
SYSCTL_SETUP(sysctl_acpi_setup, "sysctl hw.acpi subtree setup")
{
	const struct sysctlnode *rnode, *snode;
	int err;

	err = sysctl_createv(clog, 0, NULL, &rnode,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE,
	    "acpi", SYSCTL_DESCR("ACPI subsystem parameters"),
	    NULL, 0, NULL, 0,
	    CTL_HW, CTL_CREATE, CTL_EOL);

	if (err != 0)
		return;

	(void)sysctl_createv(NULL, 0, &rnode, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READONLY, CTLTYPE_QUAD,
	    "root", SYSCTL_DESCR("ACPI root pointer"),
	    NULL, 0, &acpi_root_pointer, sizeof(acpi_root_pointer),
	    CTL_CREATE, CTL_EOL);

	err = sysctl_createv(clog, 0, &rnode, &snode,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE,
	    "sleep", SYSCTL_DESCR("ACPI sleep"),
	    NULL, 0, NULL, 0,
	    CTL_CREATE, CTL_EOL);

	if (err != 0)
		return;

	(void)sysctl_createv(NULL, 0, &snode, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE, CTLTYPE_INT,
	    "state", SYSCTL_DESCR("System sleep state"),
	    sysctl_hw_acpi_sleepstate, 0, NULL, 0,
	    CTL_CREATE, CTL_EOL);

	(void)sysctl_createv(NULL, 0, &snode, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READONLY, CTLTYPE_STRING,
	    "states", SYSCTL_DESCR("Supported sleep states"),
	    sysctl_hw_acpi_sleepstates, 0, NULL, 0,
	    CTL_CREATE, CTL_EOL);

	err = sysctl_createv(clog, 0, &rnode, &rnode,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE,
	    "stat", SYSCTL_DESCR("ACPI statistics"),
	    NULL, 0, NULL, 0,
	    CTL_CREATE, CTL_EOL);

	if (err != 0)
		return;

	(void)sysctl_createv(clog, 0, &rnode, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READONLY, CTLTYPE_QUAD,
	    "gpe", SYSCTL_DESCR("Number of dispatched GPEs"),
	    NULL, 0, &AcpiGpeCount, sizeof(AcpiGpeCount),
	    CTL_CREATE, CTL_EOL);

	(void)sysctl_createv(clog, 0, &rnode, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READONLY, CTLTYPE_QUAD,
	    "sci", SYSCTL_DESCR("Number of SCI interrupts"),
	    NULL, 0, &AcpiSciCount, sizeof(AcpiSciCount),
	    CTL_CREATE, CTL_EOL);

	(void)sysctl_createv(clog, 0, &rnode, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READONLY, CTLTYPE_QUAD,
	    "fixed", SYSCTL_DESCR("Number of fixed events"),
	    sysctl_hw_acpi_fixedstats, 0, NULL, 0,
	    CTL_CREATE, CTL_EOL);

	(void)sysctl_createv(clog, 0, &rnode, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READONLY, CTLTYPE_QUAD,
	    "method", SYSCTL_DESCR("Number of methods executed"),
	    NULL, 0, &AcpiMethodCount, sizeof(AcpiMethodCount),
	    CTL_CREATE, CTL_EOL);

	CTASSERT(sizeof(AcpiGpeCount) == sizeof(uint64_t));
	CTASSERT(sizeof(AcpiSciCount) == sizeof(uint64_t));
}

static int
sysctl_hw_acpi_fixedstats(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	uint64_t t;
	int err, i;

	for (i = t = 0; i < __arraycount(AcpiFixedEventCount); i++)
		t += AcpiFixedEventCount[i];

	node = *rnode;
	node.sysctl_data = &t;

	err = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (err || newp == NULL)
		return err;

	return 0;
}

static int
sysctl_hw_acpi_sleepstate(SYSCTLFN_ARGS)
{
	struct acpi_softc *sc = acpi_softc;
	struct sysctlnode node;
	int err, t;

	if (acpi_softc == NULL)
		return ENOSYS;

	node = *rnode;
	t = sc->sc_sleepstate;
	node.sysctl_data = &t;

	err = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (err || newp == NULL)
		return err;

	if (t < ACPI_STATE_S0 || t > ACPI_STATE_S5)
		return EINVAL;

	acpi_enter_sleep_state(t);

	return 0;
}

static int
sysctl_hw_acpi_sleepstates(SYSCTLFN_ARGS)
{
	struct acpi_softc *sc = acpi_softc;
	struct sysctlnode node;
	char t[3 * 6 + 1];
	int err;

	if (acpi_softc == NULL)
		return ENOSYS;

	(void)memset(t, '\0', sizeof(t));

	(void)snprintf(t, sizeof(t), "%s%s%s%s%s%s",
	    ((sc->sc_sleepstates & __BIT(0)) != 0) ? "S0 " : "",
	    ((sc->sc_sleepstates & __BIT(1)) != 0) ? "S1 " : "",
	    ((sc->sc_sleepstates & __BIT(2)) != 0) ? "S2 " : "",
	    ((sc->sc_sleepstates & __BIT(3)) != 0) ? "S3 " : "",
	    ((sc->sc_sleepstates & __BIT(4)) != 0) ? "S4 " : "",
	    ((sc->sc_sleepstates & __BIT(5)) != 0) ? "S5 " : "");

	node = *rnode;
	node.sysctl_data = &t;

	err = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (err || newp == NULL)
		return err;

	return 0;
}

/*
 * Tables.
 */
ACPI_PHYSICAL_ADDRESS
acpi_OsGetRootPointer(void)
{
	ACPI_PHYSICAL_ADDRESS PhysicalAddress;

	/*
	 * We let MD code handle this since there are multiple ways to do it:
	 *
	 *	IA-32: Use AcpiFindRootPointer() to locate the RSDP.
	 *
	 *	IA-64: Use the EFI.
	 */
	PhysicalAddress = acpi_md_OsGetRootPointer();

	if (acpi_root_pointer == 0)
		acpi_root_pointer = PhysicalAddress;

	return PhysicalAddress;
}

static ACPI_TABLE_HEADER *
acpi_map_rsdt(void)
{
	ACPI_PHYSICAL_ADDRESS paddr;
	ACPI_TABLE_RSDP *rsdp;

	paddr = AcpiOsGetRootPointer();

	if (paddr == 0)
		return NULL;

	rsdp = AcpiOsMapMemory(paddr, sizeof(ACPI_TABLE_RSDP));

	if (rsdp == NULL)
		return NULL;

	if (rsdp->Revision > 1 && rsdp->XsdtPhysicalAddress)
		paddr = rsdp->XsdtPhysicalAddress;
	else
		paddr = rsdp->RsdtPhysicalAddress;

	AcpiOsUnmapMemory(rsdp, sizeof(ACPI_TABLE_RSDP));

	return AcpiOsMapMemory(paddr, sizeof(ACPI_TABLE_HEADER));
}

/*
 * XXX: Refactor to be a generic function that unmaps tables.
 */
static void
acpi_unmap_rsdt(ACPI_TABLE_HEADER *rsdt)
{

	if (rsdt == NULL)
		return;

	AcpiOsUnmapMemory(rsdt, sizeof(ACPI_TABLE_HEADER));
}

/*
 * XXX: Refactor to be a generic function that maps tables.
 */
ACPI_STATUS
acpi_madt_map(void)
{
	ACPI_STATUS  rv;

	if (madt_header != NULL)
		return AE_ALREADY_EXISTS;

	rv = AcpiGetTable(ACPI_SIG_MADT, 1, &madt_header);

	if (ACPI_FAILURE(rv))
		return rv;

	return AE_OK;
}

void
acpi_madt_unmap(void)
{
	madt_header = NULL;
}

/*
 * XXX: Refactor to be a generic function that walks tables.
 */
void
acpi_madt_walk(ACPI_STATUS (*func)(ACPI_SUBTABLE_HEADER *, void *), void *aux)
{
	ACPI_SUBTABLE_HEADER *hdrp;
	char *madtend, *where;

	madtend = (char *)madt_header + madt_header->Length;
	where = (char *)madt_header + sizeof (ACPI_TABLE_MADT);

	while (where < madtend) {

		hdrp = (ACPI_SUBTABLE_HEADER *)where;

		if (ACPI_FAILURE(func(hdrp, aux)))
			break;

		where += hdrp->Length;
	}
}

/*
 * Miscellaneous.
 */
static bool
acpi_is_scope(struct acpi_devnode *ad)
{
	int i;

	/*
	 * Return true if the node is a root scope.
	 */
	if (ad->ad_parent == NULL)
		return false;

	if (ad->ad_parent->ad_handle != ACPI_ROOT_OBJECT)
		return false;

	for (i = 0; i < __arraycount(acpi_scopes); i++) {

		if (acpi_scopes[i] == NULL)
			continue;

		if (ad->ad_handle == acpi_scopes[i])
			return true;
	}

	return false;
}

/*
 * ACPIVERBOSE.
 */
void
acpi_load_verbose(void)
{

	if (acpi_verbose_loaded == 0)
		module_autoload("acpiverbose", MODULE_CLASS_MISC);
}

void
acpi_print_verbose_stub(struct acpi_softc *sc)
{

	acpi_load_verbose();

	if (acpi_verbose_loaded != 0)
		acpi_print_verbose(sc);
}

void
acpi_print_dev_stub(const char *pnpstr)
{

	acpi_load_verbose();

	if (acpi_verbose_loaded != 0)
		acpi_print_dev(pnpstr);
}

MALLOC_DECLARE(M_ACPI); /* XXX: ACPI_ACTIVATE_DEV should use kmem(9). */

/*
 * ACPI_ACTIVATE_DEV.
 */
static void
acpi_activate_device(ACPI_HANDLE handle, ACPI_DEVICE_INFO **di)
{

#ifndef ACPI_ACTIVATE_DEV
	return;
}
#else
	static const int valid = ACPI_VALID_STA | ACPI_VALID_HID;
	ACPI_DEVICE_INFO *newdi;
	ACPI_STATUS rv;
	uint32_t old;

	/*
	 * If the device is valid and present,
	 * but not enabled, try to activate it.
	 */
	if (((*di)->Valid & valid) != valid)
		return;

	old = (*di)->CurrentStatus;

	if ((old & (ACPI_STA_DEVICE_PRESENT | ACPI_STA_DEVICE_ENABLED)) !=
	    ACPI_STA_DEVICE_PRESENT)
		return;

	rv = acpi_allocate_resources(handle);

	if (ACPI_FAILURE(rv))
		goto fail;

	rv = AcpiGetObjectInfo(handle, &newdi);

	if (ACPI_FAILURE(rv))
		goto fail;

	ACPI_FREE(*di);
	*di = newdi;

	aprint_verbose_dev(acpi_softc->sc_dev,
	    "%s activated, STA 0x%08X -> STA 0x%08X\n",
	    (*di)->HardwareId.String, old, (*di)->CurrentStatus);

	return;

fail:
	aprint_error_dev(acpi_softc->sc_dev, "failed to "
	    "activate %s\n", (*di)->HardwareId.String);
}

/*
 * XXX: This very incomplete.
 */
ACPI_STATUS
acpi_allocate_resources(ACPI_HANDLE handle)
{
	ACPI_BUFFER bufp, bufc, bufn;
	ACPI_RESOURCE *resp, *resc, *resn;
	ACPI_RESOURCE_IRQ *irq;
#if 0
	ACPI_RESOURCE_EXTENDED_IRQ *xirq;
#endif
	ACPI_STATUS rv;
	uint delta;

	rv = acpi_get(handle, &bufp, AcpiGetPossibleResources);
	if (ACPI_FAILURE(rv))
		goto out;
	rv = acpi_get(handle, &bufc, AcpiGetCurrentResources);
	if (ACPI_FAILURE(rv)) {
		goto out1;
	}

	bufn.Length = 1000;
	bufn.Pointer = resn = malloc(bufn.Length, M_ACPI, M_WAITOK);
	resp = bufp.Pointer;
	resc = bufc.Pointer;
	while (resc->Type != ACPI_RESOURCE_TYPE_END_TAG &&
	       resp->Type != ACPI_RESOURCE_TYPE_END_TAG) {
		while (resc->Type != resp->Type && resp->Type != ACPI_RESOURCE_TYPE_END_TAG)
			resp = ACPI_NEXT_RESOURCE(resp);
		if (resp->Type == ACPI_RESOURCE_TYPE_END_TAG)
			break;
		/* Found identical Id */
		resn->Type = resc->Type;
		switch (resc->Type) {
		case ACPI_RESOURCE_TYPE_IRQ:
			memcpy(&resn->Data, &resp->Data,
			       sizeof(ACPI_RESOURCE_IRQ));
			irq = (ACPI_RESOURCE_IRQ *)&resn->Data;
			irq->Interrupts[0] =
			    ((ACPI_RESOURCE_IRQ *)&resp->Data)->
			        Interrupts[irq->InterruptCount-1];
			irq->InterruptCount = 1;
			resn->Length = ACPI_RS_SIZE(ACPI_RESOURCE_IRQ);
			break;
		case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
			memcpy(&resn->Data, &resp->Data,
			       sizeof(ACPI_RESOURCE_EXTENDED_IRQ));
#if 0
			xirq = (ACPI_RESOURCE_EXTENDED_IRQ *)&resn->Data;
			/*
			 * XXX:	Not duplicating the interrupt logic above
			 *	because its not clear what it accomplishes.
			 */
			xirq->Interrupts[0] =
			    ((ACPI_RESOURCE_EXT_IRQ *)&resp->Data)->
			    Interrupts[irq->NumberOfInterrupts-1];
			xirq->NumberOfInterrupts = 1;
#endif
			resn->Length = ACPI_RS_SIZE(ACPI_RESOURCE_EXTENDED_IRQ);
			break;
		case ACPI_RESOURCE_TYPE_IO:
			memcpy(&resn->Data, &resp->Data,
			       sizeof(ACPI_RESOURCE_IO));
			resn->Length = resp->Length;
			break;
		default:
			aprint_error_dev(acpi_softc->sc_dev,
			    "%s: invalid type %u\n", __func__, resc->Type);
			rv = AE_BAD_DATA;
			goto out2;
		}
		resc = ACPI_NEXT_RESOURCE(resc);
		resn = ACPI_NEXT_RESOURCE(resn);
		resp = ACPI_NEXT_RESOURCE(resp);
		delta = (uint8_t *)resn - (uint8_t *)bufn.Pointer;
		if (delta >=
		    bufn.Length-ACPI_RS_SIZE(ACPI_RESOURCE_DATA)) {
			bufn.Length *= 2;
			bufn.Pointer = realloc(bufn.Pointer, bufn.Length,
					       M_ACPI, M_WAITOK);
			resn = (ACPI_RESOURCE *)((uint8_t *)bufn.Pointer +
			    delta);
		}
	}

	if (resc->Type != ACPI_RESOURCE_TYPE_END_TAG) {
		aprint_error_dev(acpi_softc->sc_dev,
		    "%s: resc not exhausted\n", __func__);
		rv = AE_BAD_DATA;
		goto out3;
	}

	resn->Type = ACPI_RESOURCE_TYPE_END_TAG;
	rv = AcpiSetCurrentResources(handle, &bufn);

	if (ACPI_FAILURE(rv))
		aprint_error_dev(acpi_softc->sc_dev, "%s: failed to set "
		    "resources: %s\n", __func__, AcpiFormatException(rv));

out3:
	free(bufn.Pointer, M_ACPI);
out2:
	ACPI_FREE(bufc.Pointer);
out1:
	ACPI_FREE(bufp.Pointer);
out:
	return rv;
}

#endif	/* ACPI_ACTIVATE_DEV */

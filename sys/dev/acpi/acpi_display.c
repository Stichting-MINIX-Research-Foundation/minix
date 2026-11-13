/*	$NetBSD: acpi_display.c,v 1.16.16.3 2022/01/20 11:42:22 martin Exp $	*/

/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gregoire Sutre.
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
 * ACPI Display Adapter Driver.
 *
 * Appendix B of the ACPI specification presents ACPI extensions for display
 * adapters.  Systems containing a built-in display adapter are required to
 * implement these extensions (in their ACPI BIOS).  This driver uses these
 * extensions to provide generic support for brightness control and display
 * switching.
 *
 * If brightness control methods are absent or non-functional, ACPI brightness
 * notifications are relayed to the PMF framework.
 *
 * This driver sets the BIOS switch policy (_DOS method) as follows:
 * - The BIOS should automatically switch the active display output, with no
 *   interaction required on the OS part.
 * - The BIOS should not automatically control the brightness levels.
 *
 * Brightness and BIOS switch policy can be adjusted from userland, via the
 * sysctl variables acpivga<n>.policy and acpiout<n>.brightness under hw.acpi.
 */

/*
 * The driver uses mutex(9) protection since changes to the hardware/software
 * state may be initiated both by the BIOS (ACPI notifications) and by the user
 * (sysctl).  The ACPI display adapter's mutex is shared with all ACPI display
 * output devices attached to it.
 *
 * The mutex only prevents undesired interleavings of ACPI notify handlers,
 * sysctl callbacks, and pmf(9) suspend/resume routines.  Race conditions with
 * autoconf(9) detachment routines could, in theory, still occur.
 *
 * The array of connected output devices (sc_odinfo) is, after attachment, only
 * used in ACPI notify handler callbacks.  Since two such callbacks cannot be
 * running simultaneously, this information does not need protection.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpi_display.c,v 1.16.16.3 2022/01/20 11:42:22 martin Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kmem.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#define _COMPONENT		ACPI_DISPLAY_COMPONENT
ACPI_MODULE_NAME		("acpi_display")

/* Notifications specific to display adapter devices (ACPI 4.0a, Sec. B.5). */
#define ACPI_NOTIFY_CycleOutputDevice			0x80
#define ACPI_NOTIFY_OutputDeviceStatusChange		0x81
#define ACPI_NOTIFY_CycleDisplayOutputHotkeyPressed	0x82
#define ACPI_NOTIFY_NextDisplayOutputHotkeyPressed	0x83
#define ACPI_NOTIFY_PreviousDisplayOutputHotkeyPressed	0x84

/* Notifications specific to display output devices (ACPI 4.0a, Sec. B.7). */
#define ACPI_NOTIFY_CycleBrightness			0x85
#define ACPI_NOTIFY_IncreaseBrightness			0x86
#define ACPI_NOTIFY_DecreaseBrightness			0x87
#define ACPI_NOTIFY_ZeroBrightness			0x88
#define ACPI_NOTIFY_DisplayDeviceOff			0x89

/* Format of the BIOS switch policy set by _DOS (ACPI 4.0a, Sec. B.4.1). */
typedef union acpidisp_bios_policy_t {
	uint8_t			raw;
	struct {
		uint8_t		output:2;
		uint8_t		brightness:1;
		uint8_t		reserved:5;
	} __packed		fmt;
} acpidisp_bios_policy_t;

/* Default BIOS switch policy (ACPI 4.0a, Sec. B.4.1). */
static const acpidisp_bios_policy_t acpidisp_default_bios_policy = {
	.raw = 0x1
};

/* BIOS output switch policies (ACPI 4.0a, Sec. B.4.1). */
#define ACPI_DISP_POLICY_OUTPUT_NORMAL		0x0
#define ACPI_DISP_POLICY_OUTPUT_AUTO		0x1
#define ACPI_DISP_POLICY_OUTPUT_LOCKED		0x2
#define ACPI_DISP_POLICY_OUTPUT_HOTKEY		0x3

/* BIOS brightness switch policies (ACPI 4.0a, Sec. B.4.1). */
#define ACPI_DISP_POLICY_BRIGHTNESS_AUTO	0x0
#define ACPI_DISP_POLICY_BRIGHTNESS_NORMAL	0x1

/* Format of output device attributes (ACPI 4.0a, Table B-2). */
typedef union acpidisp_od_attrs_t {
	uint16_t		device_id;
	uint32_t		raw;
	struct {
		uint8_t		index:4;
		uint8_t		port:4;
		uint8_t		type:4;
		uint8_t		vendor_specific:4;
		uint8_t		bios_detect:1;
		uint8_t		non_vga:1;
		uint8_t		head_id:3;
		uint16_t	reserved:10;
		uint8_t		device_id_scheme:1;
	} __packed		fmt;
} acpidisp_od_attrs_t;

/* Common legacy output device IDs (ACPI 2.0c, Table B-3). */
#define ACPI_DISP_OUT_LEGACY_DEVID_MONITOR	0x0100
#define ACPI_DISP_OUT_LEGACY_DEVID_PANEL	0x0110
#define ACPI_DISP_OUT_LEGACY_DEVID_TV		0x0200

/* Output device display types (ACPI 4.0a, Table B-2). */
#define ACPI_DISP_OUT_ATTR_TYPE_OTHER		0x0
#define ACPI_DISP_OUT_ATTR_TYPE_VGA		0x1
#define ACPI_DISP_OUT_ATTR_TYPE_TV		0x2
#define ACPI_DISP_OUT_ATTR_TYPE_EXTDIG		0x3
#define ACPI_DISP_OUT_ATTR_TYPE_INTDFP		0x4

/* Format of output device status (ACPI 4.0a, Table B-4). */
typedef union acpidisp_od_status_t {
	uint32_t		raw;
	struct {
		uint8_t		exists:1;
		uint8_t		activated:1;
		uint8_t		ready:1;
		uint8_t		not_defective:1;
		uint8_t		attached:1;
		uint32_t	reserved:27;
	} __packed		fmt;
} acpidisp_od_status_t;

/* Format of output device state (ACPI 4.0a, Table B-6). */
typedef union acpidisp_od_state_t {
	uint32_t		raw;
	struct {
		uint8_t		active:1;
		uint32_t	reserved:29;
		uint8_t		no_switch:1;
		uint8_t		commit:1;
	} __packed		fmt;
} acpidisp_od_state_t;

/*
 * acpidisp_outdev:
 *
 *	Description of an ACPI display output device.  This structure groups
 *	together:
 *	- the output device attributes, given in the display adapter's _DOD
 *	  method (ACPI 4.0a, Sec. B.4.2).
 *	- the corresponding instance of the acpiout driver (if any).
 */
struct acpidisp_outdev {
	acpidisp_od_attrs_t	 od_attrs;	/* Attributes */
	device_t		 od_device;	/* Matching base device */
};

/*
 * acpidisp_odinfo:
 *
 *	Information on connected output devices (ACPI 4.0a, Sec. B.4.2).  This
 *	structure enumerates all devices (_DOD package) connected to a display
 *	adapter.  Currently, this information is only used for display output
 *	switching via hotkey.
 *
 * Invariants (after initialization):
 *
 *	(oi_dev != NULL) && (oi_dev_count > 0)
 */
struct acpidisp_odinfo {
	struct acpidisp_outdev	*oi_dev;	/* Array of output devices */
	uint32_t		 oi_dev_count;	/* Number of output devices */
};

/*
 * acpidisp_vga_softc:
 *
 *	Software state of an ACPI display adapter.
 *
 * Invariants (after attachment):
 *
 *	((sc_caps & ACPI_DISP_VGA_CAP__DOD) == 0) => (sc_odinfo == NULL)
 */
struct acpidisp_vga_softc {
	device_t		 sc_dev;	/* Base device info */
	struct acpi_devnode	*sc_node;	/* ACPI device node */
	struct sysctllog	*sc_log;	/* Sysctl log */
	kmutex_t		 sc_mtx;	/* Mutex (shared w/ outputs) */
	uint16_t		 sc_caps;	/* Capabilities (methods) */
	acpidisp_bios_policy_t	 sc_policy;	/* BIOS switch policy (_DOS) */
	struct acpidisp_odinfo	*sc_odinfo;	/* Connected output devices */
};

/*
 * ACPI display adapter capabilities (methods).
 */
#define ACPI_DISP_VGA_CAP__DOS	__BIT(0)
#define ACPI_DISP_VGA_CAP__DOD	__BIT(1)
#define ACPI_DISP_VGA_CAP__ROM	__BIT(2)
#define ACPI_DISP_VGA_CAP__GPD	__BIT(3)
#define ACPI_DISP_VGA_CAP__SPD	__BIT(4)
#define ACPI_DISP_VGA_CAP__VPO	__BIT(5)

/*
 * acpidisp_acpivga_attach_args:
 *
 *	Attachment structure for the acpivga interface.  Used to attach display
 *	output devices under a display adapter.
 */
struct acpidisp_acpivga_attach_args {
	struct acpi_devnode	*aa_node;	/* ACPI device node */
	kmutex_t		*aa_mtx;	/* Shared mutex */
};

/*
 * acpidisp_brctl:
 *
 *	Brightness control (ACPI 4.0a, Sec. B.6.2 to B.6.4).  This structure
 *	contains the supported brightness levels (_BCL package) and the current
 *	level.  Following Windows 7 brightness control, we ignore the fullpower
 *	and battery levels (as it simplifies the code).
 *
 *	The array bc_level is sorted in strictly ascending order.
 *
 * Invariants (after initialization):
 *
 *	(bc_level != NULL) && (bc_level_count > 0)
 */
struct acpidisp_brctl {
	uint8_t		*bc_level;		/* Array of levels */
	uint16_t	 bc_level_count;	/* Number of levels */
	uint8_t		 bc_current;		/* Current level */
};

/*
 * Minimum brightness increment/decrement in response to increase/decrease
 * brightness hotkey notifications.  Must be strictly positive.
 */
#define ACPI_DISP_BRCTL_STEP	5

/*
 * acpidisp_out_softc:
 *
 *	Software state of an ACPI display output device.
 *
 * Invariants (after attachment):
 *
 *	((sc_caps & ACPI_DISP_OUT_CAP__BCL) == 0) => (sc_brctl == NULL)
 *	((sc_caps & ACPI_DISP_OUT_CAP__BCM) == 0) => (sc_brctl == NULL)
 */
struct acpidisp_out_softc {
	device_t		 sc_dev;	/* Base device info */
	struct acpi_devnode	*sc_node;	/* ACPI device node */
	struct sysctllog	*sc_log;	/* Sysctl log */
	kmutex_t		*sc_mtx;	/* Mutex (shared w/ adapter) */
	uint16_t		 sc_caps;	/* Capabilities (methods) */
	struct acpidisp_brctl	*sc_brctl;	/* Brightness control */
};

/*
 * ACPI display output device capabilities (methods).
 */
#define ACPI_DISP_OUT_CAP__BCL	__BIT(0)
#define ACPI_DISP_OUT_CAP__BCM	__BIT(1)
#define ACPI_DISP_OUT_CAP__BQC	__BIT(2)
#define ACPI_DISP_OUT_CAP__DDC	__BIT(3)
#define ACPI_DISP_OUT_CAP__DCS	__BIT(4)
#define ACPI_DISP_OUT_CAP__DGS	__BIT(5)
#define ACPI_DISP_OUT_CAP__DSS	__BIT(6)

static int	acpidisp_vga_match(device_t, cfdata_t, void *);
static void	acpidisp_vga_attach(device_t, device_t, void *);
static int	acpidisp_vga_detach(device_t, int);
static void	acpidisp_vga_childdetached(device_t, device_t);

static void	acpidisp_vga_scan_outdevs(struct acpidisp_vga_softc *);
static int	acpidisp_acpivga_print(void *, const char *);

static int	acpidisp_out_match(device_t, cfdata_t, void *);
static void	acpidisp_out_attach(device_t, device_t, void *);
static int	acpidisp_out_detach(device_t, int);

CFATTACH_DECL2_NEW(acpivga, sizeof(struct acpidisp_vga_softc),
    acpidisp_vga_match, acpidisp_vga_attach, acpidisp_vga_detach, NULL,
    NULL, acpidisp_vga_childdetached);

CFATTACH_DECL_NEW(acpiout, sizeof(struct acpidisp_out_softc),
    acpidisp_out_match, acpidisp_out_attach, acpidisp_out_detach, NULL);

static bool	acpidisp_vga_resume(device_t, const pmf_qual_t *);
static bool	acpidisp_out_suspend(device_t, const pmf_qual_t *);
static bool	acpidisp_out_resume(device_t, const pmf_qual_t *);

static uint16_t	acpidisp_vga_capabilities(const struct acpi_devnode *);
static uint16_t	acpidisp_out_capabilities(const struct acpi_devnode *);
static void	acpidisp_vga_print_capabilities(device_t, uint16_t);
static void	acpidisp_out_print_capabilities(device_t, uint16_t);

static void	acpidisp_vga_notify_handler(ACPI_HANDLE, uint32_t, void *);
static void	acpidisp_out_notify_handler(ACPI_HANDLE, uint32_t, void *);

static void	acpidisp_vga_cycle_output_device_callback(void *);
static void	acpidisp_vga_output_device_change_callback(void *);
static void	acpidisp_out_increase_brightness_callback(void *);
static void	acpidisp_out_decrease_brightness_callback(void *);
static void	acpidisp_out_cycle_brightness_callback(void *);
static void	acpidisp_out_zero_brightness_callback(void *);

static void	acpidisp_vga_sysctl_setup(struct acpidisp_vga_softc *);
static void	acpidisp_out_sysctl_setup(struct acpidisp_out_softc *);
#ifdef ACPI_DEBUG
static int	acpidisp_vga_sysctl_policy(SYSCTLFN_PROTO);
#endif
static int	acpidisp_vga_sysctl_policy_output(SYSCTLFN_PROTO);
#ifdef ACPI_DISP_SWITCH_SYSCTLS
static int	acpidisp_out_sysctl_status(SYSCTLFN_PROTO);
static int	acpidisp_out_sysctl_state(SYSCTLFN_PROTO);
#endif
static int	acpidisp_out_sysctl_brightness(SYSCTLFN_PROTO);

static struct acpidisp_odinfo *
		acpidisp_init_odinfo(const struct acpidisp_vga_softc *);
static void	acpidisp_vga_bind_outdevs(struct acpidisp_vga_softc *);
static struct acpidisp_brctl *
		acpidisp_init_brctl(const struct acpidisp_out_softc *);

static int	acpidisp_set_policy(const struct acpidisp_vga_softc *,
		    uint8_t);
static int	acpidisp_get_status(const struct acpidisp_out_softc *,
		    uint32_t *);
static int	acpidisp_get_state(const struct acpidisp_out_softc *,
		    uint32_t *);
static int	acpidisp_set_state(const struct acpidisp_out_softc *,
		    uint32_t);
static int	acpidisp_get_brightness(const struct acpidisp_out_softc *,
		    uint8_t *);
static int	acpidisp_set_brightness(const struct acpidisp_out_softc *,
		    uint8_t);

static void	acpidisp_print_odinfo(device_t, const struct acpidisp_odinfo *);
static void	acpidisp_print_brctl(device_t, const struct acpidisp_brctl *);
static void	acpidisp_print_od_attrs(acpidisp_od_attrs_t);

static bool	acpidisp_has_method(ACPI_HANDLE, const char *,
		    ACPI_OBJECT_TYPE);
static ACPI_STATUS
		acpidisp_eval_package(ACPI_HANDLE, const char *, ACPI_OBJECT **,
		    unsigned int);
static void	acpidisp_array_search(const uint8_t *, uint16_t, int, uint8_t *,
		    uint8_t *);

/*
 * Autoconfiguration for the acpivga driver.
 */

static int
acpidisp_vga_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;
	struct acpi_devnode *ad = aa->aa_node;
	struct acpi_pci_info *ap;
	pcireg_t id, class;
	pcitag_t tag;

	if (ad->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	ap = ad->ad_pciinfo;

	if (ap == NULL)
		return 0;

	if ((ap->ap_flags & ACPI_PCI_INFO_DEVICE) == 0)
		return 0;

	if (ap->ap_function == 0xffff)
		return 0;

	KASSERT(ap->ap_bus < 256);
	KASSERT(ap->ap_device < 32);
	KASSERT(ap->ap_function < 8);

	/*
	 * Check that the PCI device is present, verify
	 * the class of the PCI device, and finally see
	 * if the ACPI device is capable of something.
	 */
	tag = pci_make_tag(aa->aa_pc, ap->ap_bus,
	    ap->ap_device, ap->ap_function);

	id = pci_conf_read(aa->aa_pc, tag, PCI_ID_REG);

	if (PCI_VENDOR(id) == PCI_VENDOR_INVALID || PCI_VENDOR(id) == 0)
		return 0;

	class = pci_conf_read(aa->aa_pc, tag, PCI_CLASS_REG);

	if (PCI_CLASS(class) != PCI_CLASS_DISPLAY)
		return 0;

	if (acpidisp_vga_capabilities(ad) == 0)
		return 0;

	return 1;
}

static void
acpidisp_vga_attach(device_t parent, device_t self, void *aux)
{
	struct acpidisp_vga_softc *asc = device_private(self);
	struct acpi_attach_args *aa = aux;
	struct acpi_devnode *ad = aa->aa_node;

	aprint_naive(": ACPI Display Adapter\n");
	aprint_normal(": ACPI Display Adapter\n");

	asc->sc_node = ad;
	asc->sc_dev = self;
	asc->sc_log = NULL;

	mutex_init(&asc->sc_mtx, MUTEX_DEFAULT, IPL_NONE);

	asc->sc_caps = acpidisp_vga_capabilities(ad);
	asc->sc_policy = acpidisp_default_bios_policy;
	asc->sc_odinfo = NULL;

	acpidisp_vga_print_capabilities(self, asc->sc_caps);

	/*
	 * Enumerate connected output devices, attach
	 * output display devices, and bind the attached
	 * output devices to the enumerated ones.
	 */
	asc->sc_odinfo = acpidisp_init_odinfo(asc);

	acpidisp_vga_scan_outdevs(asc);

	if (asc->sc_odinfo != NULL) {
		acpidisp_vga_bind_outdevs(asc);
		acpidisp_print_odinfo(self, asc->sc_odinfo);
	}

	/*
	 * Set BIOS automatic switch policy.
	 *
	 * Many laptops do not support output device switching with
	 * the methods specified in the ACPI extensions for display
	 * adapters. Therefore, we leave the BIOS output switch policy
	 * on "auto" instead of setting it to "normal".
	 */
	asc->sc_policy.fmt.output = ACPI_DISP_POLICY_OUTPUT_AUTO;
	asc->sc_policy.fmt.brightness = ACPI_DISP_POLICY_BRIGHTNESS_NORMAL;

	if (acpidisp_set_policy(asc, asc->sc_policy.raw))
		asc->sc_policy = acpidisp_default_bios_policy;

	acpidisp_vga_sysctl_setup(asc);

	(void)pmf_device_register(self, NULL, acpidisp_vga_resume);
	(void)acpi_register_notify(asc->sc_node, acpidisp_vga_notify_handler);
}

static int
acpidisp_vga_detach(device_t self, int flags)
{
	struct acpidisp_vga_softc *asc = device_private(self);
	struct acpidisp_odinfo *oi = asc->sc_odinfo;
	int rc;

	pmf_device_deregister(self);

	if (asc->sc_log != NULL)
		sysctl_teardown(&asc->sc_log);

	asc->sc_policy = acpidisp_default_bios_policy;
	acpidisp_set_policy(asc, asc->sc_policy.raw);

	acpi_deregister_notify(asc->sc_node);

	if ((rc = config_detach_children(self, flags)) != 0)
		return rc;

	if (oi != NULL) {
		kmem_free(oi->oi_dev,
		    oi->oi_dev_count * sizeof(*oi->oi_dev));
		kmem_free(oi, sizeof(*oi));
	}

	mutex_destroy(&asc->sc_mtx);

	return 0;
}

void
acpidisp_vga_childdetached(device_t self, device_t child)
{
	struct acpidisp_vga_softc *asc = device_private(self);
	struct acpidisp_odinfo *oi = asc->sc_odinfo;
	struct acpidisp_outdev *od;
	struct acpi_devnode *ad;
	uint32_t i;

	SIMPLEQ_FOREACH(ad, &asc->sc_node->ad_child_head, ad_child_list) {

		if (ad->ad_device == child)
			ad->ad_device = NULL;
	}

	if (oi == NULL)
		return;

	for (i = 0, od = oi->oi_dev; i < oi->oi_dev_count; i++, od++) {
		if (od->od_device == child)
			od->od_device = NULL;
	}
}

/*
 * Attachment of acpiout under acpivga.
 */

static void
acpidisp_vga_scan_outdevs(struct acpidisp_vga_softc *asc)
{
	struct acpidisp_acpivga_attach_args aa;
	struct acpi_devnode *ad;

	/*
	 * Display output devices are ACPI children of the display adapter.
	 */
	SIMPLEQ_FOREACH(ad, &asc->sc_node->ad_child_head, ad_child_list) {

		if (ad->ad_device != NULL)	/* This should not happen. */
			continue;

		aa.aa_node = ad;
		aa.aa_mtx = &asc->sc_mtx;

		ad->ad_device = config_found_ia(asc->sc_dev,
		    "acpivga", &aa, acpidisp_acpivga_print);
	}
}

static int
acpidisp_acpivga_print(void *aux, const char *pnp)
{
	struct acpidisp_acpivga_attach_args *aa = aux;
	struct acpi_devnode *ad = aa->aa_node;

	if (pnp) {
		aprint_normal("%s at %s", ad->ad_name, pnp);
	} else {
		aprint_normal(" (%s", ad->ad_name);
		if (ad->ad_devinfo->Valid & ACPI_VALID_ADR)
			aprint_normal(", 0x%04"PRIx64, ad->ad_devinfo->Address);
		aprint_normal(")");
	}

	return UNCONF;
}

/*
 * Autoconfiguration for the acpiout driver.
 */

static int
acpidisp_out_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpidisp_acpivga_attach_args *aa = aux;
	struct acpi_devnode *ad = aa->aa_node;

	if (ad->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	/*
	 * The method _ADR is required for display output
	 * devices (ACPI 4.0a, Sec. B.6.1).
	 */
	if (!(acpidisp_has_method(ad->ad_handle, "_ADR", ACPI_TYPE_INTEGER)))
		return 0;

	return 1;
}

static void
acpidisp_out_attach(device_t parent, device_t self, void *aux)
{
	struct acpidisp_out_softc *osc = device_private(self);
	struct acpidisp_acpivga_attach_args *aa = aux;
	struct acpi_devnode *ad = aa->aa_node;
	struct acpidisp_brctl *bc;

	aprint_naive("\n");
	aprint_normal(": ACPI Display Output Device\n");

	osc->sc_dev = self;
	osc->sc_node = ad;
	osc->sc_log = NULL;
	osc->sc_mtx = aa->aa_mtx;
	osc->sc_caps = acpidisp_out_capabilities(ad);
	osc->sc_brctl = NULL;

	acpidisp_out_print_capabilities(self, osc->sc_caps);

	osc->sc_brctl = acpidisp_init_brctl(osc);
	bc = osc->sc_brctl;
	if (bc != NULL) {
		bc->bc_current = bc->bc_level[bc->bc_level_count - 1];

		/*
		 * Synchronize ACPI and driver brightness levels, and
		 * check that brightness control is working.
		 */
		if (acpidisp_get_brightness(osc, &bc->bc_current) &&
		    acpidisp_set_brightness(osc, bc->bc_current)) {
			kmem_free(bc->bc_level,
			    bc->bc_level_count * sizeof(*bc->bc_level));
			kmem_free(bc, sizeof(*bc));
			osc->sc_brctl = NULL;
		} else {
			acpidisp_print_brctl(self, osc->sc_brctl);
		}
	}

	/* Install ACPI notify handler. */
	(void)acpi_register_notify(osc->sc_node, acpidisp_out_notify_handler);

	/* Setup sysctl. */
	acpidisp_out_sysctl_setup(osc);

	/* Power management. */
	if (!pmf_device_register(self, acpidisp_out_suspend,
	    acpidisp_out_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static int
acpidisp_out_detach(device_t self, int flags)
{
	struct acpidisp_out_softc *osc = device_private(self);
	struct acpidisp_brctl *bc = osc->sc_brctl;

	pmf_device_deregister(self);

	if (osc->sc_log != NULL)
		sysctl_teardown(&osc->sc_log);

	acpi_deregister_notify(osc->sc_node);

	if (bc != NULL) {
		kmem_free(bc->bc_level,
		    bc->bc_level_count * sizeof(*bc->bc_level));
		kmem_free(bc, sizeof(*bc));
	}

	return 0;
}

/*
 * Power management.
 */

static bool
acpidisp_vga_resume(device_t self, const pmf_qual_t *qual)
{
	struct acpidisp_vga_softc *asc = device_private(self);

	mutex_enter(&asc->sc_mtx);
	(void)acpidisp_set_policy(asc, asc->sc_policy.raw);
	mutex_exit(&asc->sc_mtx);

	return true;
}

static bool
acpidisp_out_suspend(device_t self, const pmf_qual_t *qual)
{
	struct acpidisp_out_softc *osc = device_private(self);

	mutex_enter(osc->sc_mtx);
	if (osc->sc_brctl != NULL)
		(void)acpidisp_get_brightness(osc, &osc->sc_brctl->bc_current);
	mutex_exit(osc->sc_mtx);

	return true;
}

static bool
acpidisp_out_resume(device_t self, const pmf_qual_t *qual)
{
	struct acpidisp_out_softc *osc = device_private(self);

	mutex_enter(osc->sc_mtx);
	if (osc->sc_brctl != NULL)
		(void)acpidisp_set_brightness(osc, osc->sc_brctl->bc_current);
	mutex_exit(osc->sc_mtx);

	return true;
}

/*
 * Capabilities (available methods).
 */

static uint16_t
acpidisp_vga_capabilities(const struct acpi_devnode *ad)
{
	uint16_t cap;

	cap = 0;

	if (acpidisp_has_method(ad->ad_handle, "_DOS", ACPI_TYPE_METHOD))
		cap |= ACPI_DISP_VGA_CAP__DOS;

	if (acpidisp_has_method(ad->ad_handle, "_DOD", ACPI_TYPE_PACKAGE))
		cap |= ACPI_DISP_VGA_CAP__DOD;

	if (acpidisp_has_method(ad->ad_handle, "_ROM", ACPI_TYPE_BUFFER))
		cap |= ACPI_DISP_VGA_CAP__ROM;

	if (acpidisp_has_method(ad->ad_handle, "_GPD", ACPI_TYPE_INTEGER))
		cap |= ACPI_DISP_VGA_CAP__GPD;

	if (acpidisp_has_method(ad->ad_handle, "_SPD", ACPI_TYPE_METHOD))
		cap |= ACPI_DISP_VGA_CAP__SPD;

	if (acpidisp_has_method(ad->ad_handle, "_VPO", ACPI_TYPE_INTEGER))
		cap |= ACPI_DISP_VGA_CAP__VPO;

	return cap;
}

static void
acpidisp_vga_print_capabilities(device_t self, uint16_t cap)
{
	aprint_debug_dev(self, "capabilities:%s%s%s%s%s%s\n",
	    (cap & ACPI_DISP_VGA_CAP__DOS) ? " _DOS" : "",
	    (cap & ACPI_DISP_VGA_CAP__DOD) ? " _DOD" : "",
	    (cap & ACPI_DISP_VGA_CAP__ROM) ? " _ROM" : "",
	    (cap & ACPI_DISP_VGA_CAP__GPD) ? " _GPD" : "",
	    (cap & ACPI_DISP_VGA_CAP__SPD) ? " _SPD" : "",
	    (cap & ACPI_DISP_VGA_CAP__VPO) ? " _VPO" : "");
}

static uint16_t
acpidisp_out_capabilities(const struct acpi_devnode *ad)
{
	uint16_t cap;

	cap = 0;

	/* List of Brigthness levels */
	if (acpidisp_has_method(ad->ad_handle, "_BCL", ACPI_TYPE_PACKAGE))
		cap |= ACPI_DISP_OUT_CAP__BCL;

	/* Set brightness level */
	if (acpidisp_has_method(ad->ad_handle, "_BCM", ACPI_TYPE_METHOD))
		cap |= ACPI_DISP_OUT_CAP__BCM;

	/* Get brightless level */
	if (acpidisp_has_method(ad->ad_handle, "_BQC", ACPI_TYPE_INTEGER))
		cap |= ACPI_DISP_OUT_CAP__BQC;

	/* Return EDID */
	if (acpidisp_has_method(ad->ad_handle, "_DDC", ACPI_TYPE_METHOD))
		cap |= ACPI_DISP_OUT_CAP__DDC;

	/* Get Status */
	if (acpidisp_has_method(ad->ad_handle, "_DCS", ACPI_TYPE_INTEGER))
		cap |= ACPI_DISP_OUT_CAP__DCS;

	/* Get Graphics State */
	if (acpidisp_has_method(ad->ad_handle, "_DGS", ACPI_TYPE_INTEGER))
		cap |= ACPI_DISP_OUT_CAP__DGS;

	/* Set Graphics State */
	if (acpidisp_has_method(ad->ad_handle, "_DSS", ACPI_TYPE_METHOD))
		cap |= ACPI_DISP_OUT_CAP__DSS;

	return cap;
}

static void
acpidisp_out_print_capabilities(device_t self, uint16_t cap)
{
	aprint_debug_dev(self, "capabilities:%s%s%s%s%s%s%s\n",
	    (cap & ACPI_DISP_OUT_CAP__BCL) ? " _BCL" : "",
	    (cap & ACPI_DISP_OUT_CAP__BCM) ? " _BCM" : "",
	    (cap & ACPI_DISP_OUT_CAP__BQC) ? " _BQC" : "",
	    (cap & ACPI_DISP_OUT_CAP__DDC) ? " _DDC" : "",
	    (cap & ACPI_DISP_OUT_CAP__DCS) ? " _DCS" : "",
	    (cap & ACPI_DISP_OUT_CAP__DGS) ? " _DGS" : "",
	    (cap & ACPI_DISP_OUT_CAP__DSS) ? " _DSS" : "");
}

/*
 * ACPI notify handlers.
 */

static void
acpidisp_vga_notify_handler(ACPI_HANDLE handle, uint32_t notify,
    void *context)
{
	struct acpidisp_vga_softc *asc = device_private(context);
	ACPI_OSD_EXEC_CALLBACK callback;

	callback = NULL;

	switch (notify) {
	case ACPI_NOTIFY_CycleOutputDevice:
		callback = acpidisp_vga_cycle_output_device_callback;
		break;
	case ACPI_NOTIFY_OutputDeviceStatusChange:
		callback = acpidisp_vga_output_device_change_callback;
		break;
	case ACPI_NOTIFY_CycleDisplayOutputHotkeyPressed:
	case ACPI_NOTIFY_NextDisplayOutputHotkeyPressed:
	case ACPI_NOTIFY_PreviousDisplayOutputHotkeyPressed:
		aprint_debug_dev(asc->sc_dev,
		    "unhandled notify: 0x%"PRIx32"\n", notify);
		return;
	default:
		aprint_error_dev(asc->sc_dev,
		    "unknown notify: 0x%"PRIx32"\n", notify);
		return;
	}

	KASSERT(callback != NULL);
	(void)AcpiOsExecute(OSL_NOTIFY_HANDLER, callback, asc);
}

static void
acpidisp_out_notify_handler(ACPI_HANDLE handle, uint32_t notify,
    void *context)
{
	struct acpidisp_out_softc *osc = device_private(context);
	ACPI_OSD_EXEC_CALLBACK callback;

	callback = NULL;

	switch (notify) {
	case ACPI_NOTIFY_IncreaseBrightness:
		callback = acpidisp_out_increase_brightness_callback;
		break;
	case ACPI_NOTIFY_DecreaseBrightness:
		callback = acpidisp_out_decrease_brightness_callback;
		break;
	case ACPI_NOTIFY_CycleBrightness:
		callback = acpidisp_out_cycle_brightness_callback;
		break;
	case ACPI_NOTIFY_ZeroBrightness:
		callback = acpidisp_out_zero_brightness_callback;
		break;
	case ACPI_NOTIFY_DisplayDeviceOff:
		aprint_debug_dev(osc->sc_dev,
		    "unhandled notify: 0x%"PRIx32"\n", notify);
		return;
	default:
		aprint_error_dev(osc->sc_dev,
		    "unknown notify: 0x%"PRIx32"\n", notify);
		return;
	}

	KASSERT(callback != NULL);
	(void)AcpiOsExecute(OSL_NOTIFY_HANDLER, callback, osc);
}

/*
 * ACPI notify callbacks.
 *
 * Exclusive access to the sc_odinfo field of struct acpidisp_vga_softc is
 * guaranteed since:
 *
 * (a) this field is only used in ACPI display notify callbacks,
 * (b) ACPI display notify callbacks are scheduled with AcpiOsExecute,
 * (c) callbacks scheduled with AcpiOsExecute are executed sequentially.
 */

static void
acpidisp_vga_cycle_output_device_callback(void *arg)
{
	struct acpidisp_vga_softc *asc = arg;
	struct acpidisp_odinfo *oi = asc->sc_odinfo;
	struct acpidisp_outdev *od;
	struct acpidisp_out_softc *osc, *last_osc;
	acpidisp_od_state_t state, last_state;
	acpidisp_od_status_t status;
	acpidisp_bios_policy_t lock_policy;
	uint32_t i;

	if (oi == NULL)
		return;

	/* Mutual exclusion with callbacks of connected output devices. */
	mutex_enter(&asc->sc_mtx);

	/* Lock the _DGS values. */
	lock_policy = asc->sc_policy;
	lock_policy.fmt.output = ACPI_DISP_POLICY_OUTPUT_LOCKED;
	(void)acpidisp_set_policy(asc, lock_policy.raw);

	last_osc = NULL;
	for (i = 0, od = oi->oi_dev; i < oi->oi_dev_count; i++, od++) {
		if (od->od_device == NULL)
			continue;
		osc = device_private(od->od_device);

		if (!(osc->sc_caps & ACPI_DISP_OUT_CAP__DSS))
			continue;
		if (acpidisp_get_state(osc, &state.raw))
			continue;

		if (acpidisp_get_status(osc, &status.raw)) {
			state.fmt.no_switch = 0;
		} else {
			state.fmt.active &= status.fmt.ready;

			if (state.fmt.active == status.fmt.activated)
				state.fmt.no_switch = 1;
			else
				state.fmt.no_switch = 0;
		}

		state.fmt.commit = 0;

		if (last_osc != NULL)
			(void)acpidisp_set_state(last_osc, last_state.raw);

		last_osc = osc;
		last_state = state;
	}

	if (last_osc != NULL) {
		last_state.fmt.commit = 1;
		(void)acpidisp_set_state(last_osc, last_state.raw);
	}

	/* Restore the original BIOS policy. */
	(void)acpidisp_set_policy(asc, asc->sc_policy.raw);

	mutex_exit(&asc->sc_mtx);
}

static void
acpidisp_vga_output_device_change_callback(void *arg)
{
	struct acpidisp_vga_softc *asc = arg;
	struct acpidisp_odinfo *oi = asc->sc_odinfo;
	bool switch_outputs;

	if (oi != NULL) {
		kmem_free(oi->oi_dev,
		    oi->oi_dev_count * sizeof(*oi->oi_dev));
		kmem_free(oi, sizeof(*oi));
	}

	asc->sc_odinfo = acpidisp_init_odinfo(asc);
	if (asc->sc_odinfo != NULL) {
		acpidisp_vga_bind_outdevs(asc);
		acpidisp_print_odinfo(asc->sc_dev, asc->sc_odinfo);
	}

	/* Perform display output switch if needed. */
	mutex_enter(&asc->sc_mtx);
	switch_outputs =
	    (asc->sc_policy.fmt.output == ACPI_DISP_POLICY_OUTPUT_NORMAL);
	mutex_exit(&asc->sc_mtx);
	if (switch_outputs)
		acpidisp_vga_cycle_output_device_callback(arg);
}

static void
acpidisp_out_increase_brightness_callback(void *arg)
{
	struct acpidisp_out_softc *osc = arg;
	struct acpidisp_brctl *bc = osc->sc_brctl;
	uint8_t max, lo, up;
	int cur;

	if (bc == NULL) {
		/* Fallback to pmf(9). */
		pmf_event_inject(NULL, PMFE_DISPLAY_BRIGHTNESS_UP);
		return;
	}

	mutex_enter(osc->sc_mtx);
	max = bc->bc_level[bc->bc_level_count - 1];
	if (acpidisp_get_brightness(osc, &bc->bc_current))
		goto out;
	for (cur = bc->bc_current; (cur += ACPI_DISP_BRCTL_STEP) <= max;) {
		acpidisp_array_search(bc->bc_level, bc->bc_level_count, cur,
		    &lo, &up);
		bc->bc_current = up;
		if (acpidisp_set_brightness(osc, bc->bc_current))
			goto out;
		if (acpidisp_get_brightness(osc, &bc->bc_current))
			goto out;
		if (bc->bc_current >= cur)
			break;
	}
out:	mutex_exit(osc->sc_mtx);
}

static void
acpidisp_out_decrease_brightness_callback(void *arg)
{
	struct acpidisp_out_softc *osc = arg;
	struct acpidisp_brctl *bc = osc->sc_brctl;
	uint8_t min, lo, up;
	int cur;

	if (bc == NULL) {
		/* Fallback to pmf(9). */
		pmf_event_inject(NULL, PMFE_DISPLAY_BRIGHTNESS_DOWN);
		return;
	}

	mutex_enter(osc->sc_mtx);
	min = bc->bc_level[0];
	if (acpidisp_get_brightness(osc, &bc->bc_current))
		goto out;
	for (cur = bc->bc_current; (cur -= ACPI_DISP_BRCTL_STEP) >= min;) {
		acpidisp_array_search(bc->bc_level, bc->bc_level_count, cur,
		    &lo, &up);
		bc->bc_current = lo;
		if (acpidisp_set_brightness(osc, bc->bc_current))
			goto out;
		if (acpidisp_get_brightness(osc, &bc->bc_current))
			goto out;
		if (bc->bc_current <= cur)
			break;
	}
out:	mutex_exit(osc->sc_mtx);
}

static void
acpidisp_out_cycle_brightness_callback(void *arg)
{
	struct acpidisp_out_softc *osc = arg;
	struct acpidisp_brctl *bc = osc->sc_brctl;
	uint8_t lo, up;

	if (bc == NULL) {
		/* No fallback. */
		return;
	}

	mutex_enter(osc->sc_mtx);

	(void)acpidisp_get_brightness(osc, &bc->bc_current);

	if (bc->bc_current >= bc->bc_level[bc->bc_level_count - 1]) {
		bc->bc_current = bc->bc_level[0];
	} else {
		acpidisp_array_search(bc->bc_level, bc->bc_level_count,
		    bc->bc_current + 1, &lo, &up);
		bc->bc_current = up;
	}

	(void)acpidisp_set_brightness(osc, bc->bc_current);

	mutex_exit(osc->sc_mtx);
}

static void
acpidisp_out_zero_brightness_callback(void *arg)
{
	struct acpidisp_out_softc *osc = arg;
	struct acpidisp_brctl *bc = osc->sc_brctl;

	if (bc == NULL) {
		/* Fallback to pmf(9). */
		/* XXX Is this the intended meaning of PMFE_DISPLAY_REDUCED? */
		pmf_event_inject(NULL, PMFE_DISPLAY_REDUCED);
		return;
	}

	mutex_enter(osc->sc_mtx);

	bc->bc_current = bc->bc_level[0];
	(void)acpidisp_set_brightness(osc, bc->bc_current);

	mutex_exit(osc->sc_mtx);
}

/*
 * Sysctl setup.
 */

static void
acpidisp_vga_sysctl_setup(struct acpidisp_vga_softc *asc)
{
	const struct sysctlnode *rnode;

	if (asc->sc_caps & ACPI_DISP_VGA_CAP__DOS) {
		if ((sysctl_createv(&asc->sc_log, 0, NULL, &rnode,
		    0, CTLTYPE_NODE, "acpi", NULL,
		    NULL, 0, NULL, 0,
		    CTL_HW, CTL_CREATE, CTL_EOL)) != 0)
			goto fail;

		if ((sysctl_createv(&asc->sc_log, 0, &rnode, &rnode,
		    0, CTLTYPE_NODE, device_xname(asc->sc_dev),
		    SYSCTL_DESCR("ACPI display adapter controls"),
		    NULL, 0, NULL, 0,
		    CTL_CREATE, CTL_EOL)) != 0)
			goto fail;

#ifdef ACPI_DEBUG
		(void)sysctl_createv(&asc->sc_log, 0, &rnode, NULL,
		    CTLFLAG_READWRITE | CTLFLAG_HEX, CTLTYPE_INT, "bios_policy",
		    SYSCTL_DESCR("Current BIOS switch policies (debug)"),
		    acpidisp_vga_sysctl_policy, 0, (void *)asc, 0,
		    CTL_CREATE, CTL_EOL);
#endif

		(void)sysctl_createv(&asc->sc_log, 0, &rnode, NULL,
		    CTLFLAG_READWRITE, CTLTYPE_BOOL, "bios_switch",
		    SYSCTL_DESCR("Current BIOS output switching policy"),
		    acpidisp_vga_sysctl_policy_output, 0, (void *)asc, 0,
		    CTL_CREATE, CTL_EOL);
	}

	return;

 fail:
	aprint_error_dev(asc->sc_dev, "couldn't add sysctl nodes\n");
}

static void
acpidisp_out_sysctl_setup(struct acpidisp_out_softc *osc)
{
	const struct sysctlnode *rnode;

#ifdef ACPI_DISP_SWITCH_SYSCTLS
	if ((osc->sc_brctl != NULL) ||
	    (osc->sc_caps & ACPI_DISP_OUT_CAP__DCS) ||
	    (osc->sc_caps & ACPI_DISP_OUT_CAP__DGS)) {
#else
	if (osc->sc_brctl != NULL) {
#endif
		if ((sysctl_createv(&osc->sc_log, 0, NULL, &rnode,
		    0, CTLTYPE_NODE, "acpi", NULL,
		    NULL, 0, NULL, 0,
		    CTL_HW, CTL_CREATE, CTL_EOL)) != 0)
			goto fail;

		if ((sysctl_createv(&osc->sc_log, 0, &rnode, &rnode,
		    0, CTLTYPE_NODE, device_xname(osc->sc_dev),
		    SYSCTL_DESCR("ACPI display output device controls"),
		    NULL, 0, NULL, 0,
		    CTL_CREATE, CTL_EOL)) != 0)
			goto fail;
	}

	if (osc->sc_brctl != NULL) {
		(void)sysctl_createv(&osc->sc_log, 0, &rnode, NULL,
		    CTLFLAG_READWRITE, CTLTYPE_INT, "brightness",
		    SYSCTL_DESCR("Current brightness level"),
		    acpidisp_out_sysctl_brightness, 0, (void *)osc, 0,
		    CTL_CREATE, CTL_EOL);
	}

#ifdef ACPI_DISP_SWITCH_SYSCTLS
	if (osc->sc_caps & ACPI_DISP_OUT_CAP__DCS) {
		(void)sysctl_createv(&osc->sc_log, 0, &rnode, NULL,
		    CTLFLAG_READONLY | CTLFLAG_HEX, CTLTYPE_INT, "status",
		    SYSCTL_DESCR("Current status"),
		    acpidisp_out_sysctl_status, 0, (void *)osc, 0,
		    CTL_CREATE, CTL_EOL);
	}

	if (osc->sc_caps & ACPI_DISP_OUT_CAP__DGS) {
		int access;

		if (osc->sc_caps & ACPI_DISP_OUT_CAP__DSS)
			access = CTLFLAG_READWRITE;
		else
			access = CTLFLAG_READONLY;

		(void)sysctl_createv(&osc->sc_log, 0, &rnode, NULL,
		    access | CTLFLAG_HEX, CTLTYPE_INT, "state",
		    SYSCTL_DESCR("Next state (active or inactive)"),
		    acpidisp_out_sysctl_state, 0, (void *)osc, 0,
		    CTL_CREATE, CTL_EOL);
	}
#endif

	return;

 fail:
	aprint_error_dev(osc->sc_dev, "couldn't add sysctl nodes\n");
}

/*
 * Sysctl callbacks.
 */

#ifdef ACPI_DEBUG
static int
acpidisp_vga_sysctl_policy(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct acpidisp_vga_softc *asc;
	uint32_t val;
	int error;

	node = *rnode;
	asc = node.sysctl_data;

	mutex_enter(&asc->sc_mtx);
	val = (uint32_t)asc->sc_policy.raw;
	mutex_exit(&asc->sc_mtx);

	node.sysctl_data = &val;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (val > 0x7)
		return EINVAL;

	mutex_enter(&asc->sc_mtx);
	asc->sc_policy.raw = (uint8_t)val;
	error = acpidisp_set_policy(asc, asc->sc_policy.raw);
	mutex_exit(&asc->sc_mtx);

	return error;
}
#endif

static int
acpidisp_vga_sysctl_policy_output(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct acpidisp_vga_softc *asc;
	bool val;
	int error;

	node = *rnode;
	asc = node.sysctl_data;

	mutex_enter(&asc->sc_mtx);
	val = (asc->sc_policy.fmt.output == ACPI_DISP_POLICY_OUTPUT_AUTO);
	mutex_exit(&asc->sc_mtx);

	node.sysctl_data = &val;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	mutex_enter(&asc->sc_mtx);
	if (val)
		asc->sc_policy.fmt.output = ACPI_DISP_POLICY_OUTPUT_AUTO;
	else
		asc->sc_policy.fmt.output = ACPI_DISP_POLICY_OUTPUT_NORMAL;
	error = acpidisp_set_policy(asc, asc->sc_policy.raw);
	mutex_exit(&asc->sc_mtx);

	return error;
}

#ifdef ACPI_DISP_SWITCH_SYSCTLS
static int
acpidisp_out_sysctl_status(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct acpidisp_out_softc *osc;
	uint32_t val;
	int error;

	node = *rnode;
	osc = node.sysctl_data;

	mutex_enter(osc->sc_mtx);
	error = acpidisp_get_status(osc, &val);
	mutex_exit(osc->sc_mtx);

	if (error)
		return error;

	node.sysctl_data = &val;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	return 0;
}

static int
acpidisp_out_sysctl_state(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct acpidisp_out_softc *osc;
	uint32_t val;
	int error;

	node = *rnode;
	osc = node.sysctl_data;

	mutex_enter(osc->sc_mtx);
	error = acpidisp_get_state(osc, &val);
	mutex_exit(osc->sc_mtx);

	if (error)
		return error;

	node.sysctl_data = &val;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	mutex_enter(osc->sc_mtx);
	error = acpidisp_set_state(osc, val);
	mutex_exit(osc->sc_mtx);

	return error;
}
#endif

static int
acpidisp_out_sysctl_brightness(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct acpidisp_out_softc *osc;
	struct acpidisp_brctl *bc;
	int val, error;
	uint8_t lo, up, level;

	node = *rnode;
	osc = node.sysctl_data;
	bc = osc->sc_brctl;

	KASSERT(bc != NULL);

	mutex_enter(osc->sc_mtx);
	(void)acpidisp_get_brightness(osc, &bc->bc_current);
	val = (int)bc->bc_current;
	mutex_exit(osc->sc_mtx);

	node.sysctl_data = &val;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	acpidisp_array_search(bc->bc_level, bc->bc_level_count, val, &lo, &up);
	if ((lo != up) && (val - lo) < (up - val))
		level = lo;
	else
		level = up;

	mutex_enter(osc->sc_mtx);
	bc->bc_current = level;
	error = acpidisp_set_brightness(osc, bc->bc_current);
	mutex_exit(osc->sc_mtx);

	return error;
}

/*
 * Initialization of acpidisp_odinfo (_DOD) and acpidisp_brctl (_BCL).
 */

/*
 * Regarding _DOD (ACPI 4.0a, Sec. B.4.2):
 *
 * "The _DOD method returns a list of devices attached to the graphics adapter,
 *  along with device-specific configuration information."
 *
 * "Every child device enumerated in the ACPI namespace under the graphics
 *  adapter must be specified in this list of devices.  Each display device
 *  must have its own ID, which is unique with respect to any other attachable
 *  devices enumerated."
 *
 * "Return value: a package containing a variable-length list of integers,
 *  each of which contains the 32-bit device attribute of a child device."
 */

static struct acpidisp_odinfo *
acpidisp_init_odinfo(const struct acpidisp_vga_softc *asc)
{
	ACPI_HANDLE hdl = asc->sc_node->ad_handle;
	ACPI_STATUS rv;
	ACPI_OBJECT *pkg;
	struct acpidisp_odinfo *oi;
	struct acpidisp_outdev *devp;
	uint32_t count, i;

	if (!(asc->sc_caps & ACPI_DISP_VGA_CAP__DOD))
		return NULL;

	oi = NULL;
	pkg = NULL;

	rv = acpidisp_eval_package(hdl, "_DOD", &pkg, 1);
	if (ACPI_FAILURE(rv))
		goto fail;

	/*
	 * Allocate and fill the struct acpidisp_odinfo to be returned.
	 */
	oi = kmem_zalloc(sizeof(*oi), KM_SLEEP);
	oi->oi_dev_count = pkg->Package.Count;
	oi->oi_dev = kmem_zalloc(oi->oi_dev_count * sizeof(*oi->oi_dev),
	    KM_SLEEP);

	/*
	 * Fill the array oi->oi_dev.
	 */
	for (count = 0, i = 0; i < pkg->Package.Count; i++) {
		/* List of 32-bit integers (ACPI 4.0a, Sec. B.4.2). */
		if (pkg->Package.Elements[i].Type != ACPI_TYPE_INTEGER ||
		    pkg->Package.Elements[i].Integer.Value > UINT32_MAX)
			continue;

		oi->oi_dev[count].od_attrs.raw =
		    (uint32_t)pkg->Package.Elements[i].Integer.Value;
		count++;
	}

	if (count == 0) {
		rv = AE_BAD_DATA;
		goto fail;
	}

	ACPI_FREE(pkg);
	pkg = NULL;

	/*
	 * Resize the array oi->oi_dev if needed.
	 */
	if (count < oi->oi_dev_count) {
		devp = kmem_alloc(count * sizeof(*devp), KM_SLEEP);
		(void)memcpy(devp, oi->oi_dev, count * sizeof(*devp));
		kmem_free(oi->oi_dev, oi->oi_dev_count * sizeof(*oi->oi_dev));
		oi->oi_dev = devp;
		oi->oi_dev_count = count;
	}

	return oi;

 fail:
	aprint_error_dev(asc->sc_dev, "failed to evaluate %s.%s: %s\n",
	    acpi_name(hdl), "_DOD", AcpiFormatException(rv));
	if (pkg != NULL)
		ACPI_FREE(pkg);
	if (oi != NULL) {
		if (oi->oi_dev != NULL)
			kmem_free(oi->oi_dev,
			    oi->oi_dev_count * sizeof(*oi->oi_dev));
		kmem_free(oi, sizeof(*oi));
	}
	return NULL;
}

/*
 * acpidisp_vga_bind_outdevs:
 *
 *	Bind each acpiout device attached under an acpivga device to the
 *	corresponding (_DOD enumerated) connected output device.
 */
static void
acpidisp_vga_bind_outdevs(struct acpidisp_vga_softc *asc)
{
	struct acpidisp_odinfo *oi = asc->sc_odinfo;
	struct acpidisp_out_softc *osc;
	struct acpidisp_outdev *od;
	struct acpi_devnode *ad;
	ACPI_HANDLE hdl;
	ACPI_INTEGER val;
	ACPI_STATUS rv;
	uint16_t devid;
	uint32_t i;

	KASSERT(oi != NULL);

	/* Reset all bindings. */
	for (i = 0, od = oi->oi_dev; i < oi->oi_dev_count; i++, od++)
		od->od_device = NULL;

	/*
	 * Iterate over all ACPI children that have been attached under this
	 * acpivga device (as acpiout devices).
	 */
	SIMPLEQ_FOREACH(ad, &asc->sc_node->ad_child_head, ad_child_list) {
		if ((ad->ad_device == NULL) ||
		    (device_parent(ad->ad_device) != asc->sc_dev))
			continue;

		KASSERT(device_is_a(ad->ad_device, "acpiout"));

		osc = device_private(ad->ad_device);

		/*
		 * For display output devices, the method _ADR returns
		 * the device's ID (ACPI 4.0a, Sec. B.6.1).  We do not
		 * cache the result of _ADR since it may vary.
		 */
		hdl = osc->sc_node->ad_handle;
		rv = acpi_eval_integer(hdl, "_ADR", &val);
		if (ACPI_FAILURE(rv)) {
			aprint_error_dev(asc->sc_dev,
			    "failed to evaluate %s.%s: %s\n",
			    acpi_name(hdl), "_ADR", AcpiFormatException(rv));
			continue;
		}

		/* The device ID is a 16-bit integer (ACPI 4.0a, Table B-2). */
		devid = (uint16_t)val;

		/*
		 * The device ID must be unique (among output devices), and must
		 * appear in the list returned by _DOD (ACPI 4.0a, Sec. B.6.1).
		 */
		for (i = 0, od = oi->oi_dev; i < oi->oi_dev_count; i++, od++) {
			if (devid == od->od_attrs.device_id) {
				if (od->od_device != NULL)
					aprint_error_dev(asc->sc_dev,
					    "%s has same device ID as %s\n",
					    device_xname(osc->sc_dev),
					    device_xname(od->od_device));
				else
					od->od_device = osc->sc_dev;
				break;
			}
		}
		if (i == oi->oi_dev_count)
			aprint_debug_dev(asc->sc_dev,
			    "output device %s not connected\n",
			    device_xname(osc->sc_dev));
	}
}

/*
 * Regarding _BCL (ACPI 4.0a, Sec. B.6.2):
 *
 * "This method allows the OS to query a list of brightness levels supported by
 *  built-in display output devices."
 *
 * "Return value: a variable-length package containing a list of integers
 *  representing the supported brightness levels.  Each integer has 8 bits of
 *  significant data."
 */

static struct acpidisp_brctl *
acpidisp_init_brctl(const struct acpidisp_out_softc *osc)
{
	ACPI_HANDLE hdl = osc->sc_node->ad_handle;
	ACPI_STATUS rv;
	ACPI_OBJECT *pkg;
	struct acpidisp_brctl *bc;
	uint8_t *levelp;
	uint32_t i;
	int32_t j;
	uint16_t count, k;
	uint8_t level;

	if (!(osc->sc_caps & ACPI_DISP_OUT_CAP__BCL))
		return NULL;

	bc = NULL;
	pkg = NULL;

	rv = acpidisp_eval_package(hdl, "_BCL", &pkg, 2);
	if (ACPI_FAILURE(rv))
		goto fail;

	/*
	 * Allocate and fill the struct acpidisp_brctl to be returned.
	 */
	bc = kmem_zalloc(sizeof(*bc), KM_SLEEP);

	/* At most 256 brightness levels (8-bit integers). */
	if (pkg->Package.Count > 256)
		bc->bc_level_count = 256;
	else
		bc->bc_level_count = (uint16_t)pkg->Package.Count;

	bc->bc_level = kmem_zalloc(bc->bc_level_count * sizeof(*bc->bc_level),
	    KM_SLEEP);

	/*
	 * Fill the array bc->bc_level with an insertion sort.
	 */
	for (count = 0, i = 0; i < pkg->Package.Count; i++) {
		/* List of 8-bit integers (ACPI 4.0a, Sec. B.6.2). */
		if (pkg->Package.Elements[i].Type != ACPI_TYPE_INTEGER ||
		    pkg->Package.Elements[i].Integer.Value > UINT8_MAX)
			continue;

		level = (uint8_t)pkg->Package.Elements[i].Integer.Value;

		/* Find the correct slot but do not modify the array yet. */
		for (j = count; --j >= 0 && bc->bc_level[j] > level; );
		if (j >= 0 && bc->bc_level[j] == level)
			continue;
		j++;

		/* Make room for the new level. */
		for (k = count; k > j; k--)
			bc->bc_level[k] = bc->bc_level[k-1];

		/* Insert the new level. */
		bc->bc_level[j] = level;
		count++;
	}

	if (count == 0) {
		rv = AE_BAD_DATA;
		goto fail;
	}

	ACPI_FREE(pkg);
	pkg = NULL;

	/*
	 * Resize the array bc->bc_level if needed.
	 */
	if (count < bc->bc_level_count) {
		levelp = kmem_alloc(count * sizeof(*levelp), KM_SLEEP);
		(void)memcpy(levelp, bc->bc_level, count * sizeof(*levelp));
		kmem_free(bc->bc_level,
		    bc->bc_level_count * sizeof(*bc->bc_level));
		bc->bc_level = levelp;
		bc->bc_level_count = count;
	}

	return bc;

 fail:
	aprint_error_dev(osc->sc_dev, "failed to evaluate %s.%s: %s\n",
	    acpi_name(hdl), "_BCL", AcpiFormatException(rv));
	if (pkg != NULL)
		ACPI_FREE(pkg);
	if (bc != NULL) {
		if (bc->bc_level != NULL)
			kmem_free(bc->bc_level,
			    bc->bc_level_count * sizeof(*bc->bc_level));
		kmem_free(bc, sizeof(*bc));
	}
	return NULL;
}

/*
 * Evaluation of simple ACPI display methods.
 */

static int
acpidisp_set_policy(const struct acpidisp_vga_softc *asc, uint8_t value)
{
	ACPI_HANDLE hdl = asc->sc_node->ad_handle;
	ACPI_INTEGER val;
	ACPI_STATUS rv;

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "%s: set %s: 0x%"PRIx8"\n",
	    device_xname(asc->sc_dev), "policy", value));

	if (!(asc->sc_caps & ACPI_DISP_VGA_CAP__DOS))
		return ENODEV;

	val = (ACPI_INTEGER)value;
	rv = acpi_eval_set_integer(hdl, "_DOS", val);
	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(asc->sc_dev, "failed to evaluate %s.%s: %s\n",
		    acpi_name(hdl), "_DOS", AcpiFormatException(rv));
		return EIO;
	}

	return 0;
}

static int
acpidisp_get_status(const struct acpidisp_out_softc *osc, uint32_t *valuep)
{
	ACPI_HANDLE hdl = osc->sc_node->ad_handle;
	ACPI_INTEGER val;
	ACPI_STATUS rv;

	if (!(osc->sc_caps & ACPI_DISP_OUT_CAP__DCS))
		return ENODEV;

	rv = acpi_eval_integer(hdl, "_DCS", &val);
	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(osc->sc_dev, "failed to evaluate %s.%s: %s\n",
		    acpi_name(hdl), "_DCS", AcpiFormatException(rv));
		return EIO;
	}

	if (val > UINT32_MAX)
		return ERANGE;

	*valuep = (uint32_t)val;

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "%s: get %s: 0x%"PRIx32"\n",
	    device_xname(osc->sc_dev), "status", *valuep));

	return 0;
}

static int
acpidisp_get_state(const struct acpidisp_out_softc *osc, uint32_t *valuep)
{
	ACPI_HANDLE hdl = osc->sc_node->ad_handle;
	ACPI_INTEGER val;
	ACPI_STATUS rv;

	if (!(osc->sc_caps & ACPI_DISP_OUT_CAP__DGS))
		return ENODEV;

	rv = acpi_eval_integer(hdl, "_DGS", &val);
	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(osc->sc_dev, "failed to evaluate %s.%s: %s\n",
		    acpi_name(hdl), "_DGS", AcpiFormatException(rv));
		return EIO;
	}

	if (val > UINT32_MAX)
		return ERANGE;

	*valuep = (uint32_t)val;

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "%s: get %s: 0x%"PRIx32"\n",
	    device_xname(osc->sc_dev), "state", *valuep));

	return 0;
}

static int
acpidisp_set_state(const struct acpidisp_out_softc *osc, uint32_t value)
{
	ACPI_HANDLE hdl = osc->sc_node->ad_handle;
	ACPI_INTEGER val;
	ACPI_STATUS rv;

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "%s: set %s: 0x%"PRIx32"\n",
	    device_xname(osc->sc_dev), "state", value));

	if (!(osc->sc_caps & ACPI_DISP_OUT_CAP__DSS))
		return ENODEV;

	val = (ACPI_INTEGER)value;
	rv = acpi_eval_set_integer(hdl, "_DSS", val);
	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(osc->sc_dev, "failed to evaluate %s.%s: %s\n",
		    acpi_name(hdl), "_DSS", AcpiFormatException(rv));
		return EIO;
	}

	return 0;
}

static int
acpidisp_get_brightness(const struct acpidisp_out_softc *osc, uint8_t *valuep)
{
	ACPI_HANDLE hdl = osc->sc_node->ad_handle;
	ACPI_INTEGER val;
	ACPI_STATUS rv;

	if (!(osc->sc_caps & ACPI_DISP_OUT_CAP__BQC))
		return ENODEV;

	rv = acpi_eval_integer(hdl, "_BQC", &val);
	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(osc->sc_dev, "failed to evaluate %s.%s: %s\n",
		    acpi_name(hdl), "_BQC", AcpiFormatException(rv));
		return EIO;
	}

	if (val > UINT8_MAX)
		return ERANGE;

	*valuep = (uint8_t)val;

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "%s: get %s: %"PRIu8"\n",
	    device_xname(osc->sc_dev), "brightness", *valuep));

	return 0;
}

static int
acpidisp_set_brightness(const struct acpidisp_out_softc *osc, uint8_t value)
{
	ACPI_HANDLE hdl = osc->sc_node->ad_handle;
	ACPI_INTEGER val;
	ACPI_STATUS rv;

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "%s: set %s: %"PRIu8"\n",
	    device_xname(osc->sc_dev), "brightness", value));

	if (!(osc->sc_caps & ACPI_DISP_OUT_CAP__BCM))
		return ENODEV;

	val = (ACPI_INTEGER)value;
	rv = acpi_eval_set_integer(hdl, "_BCM", val);
	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(osc->sc_dev, "failed to evaluate %s.%s: %s\n",
		    acpi_name(hdl), "_BCM", AcpiFormatException(rv));
		return EIO;
	}

	return 0;
}

/*
 * Pretty printing.
 */

static void
acpidisp_print_odinfo(device_t self, const struct acpidisp_odinfo *oi)
{
	struct acpidisp_outdev *od;
	uint32_t i;

	KASSERT(oi != NULL);

	aprint_verbose_dev(self, "connected output devices:\n");
	for (i = 0, od = oi->oi_dev; i < oi->oi_dev_count; i++, od++) {
		aprint_verbose_dev(self, "  0x%04"PRIx16, od->od_attrs.device_id);
		if (od->od_device != NULL)
			aprint_verbose(" (%s)", device_xname(od->od_device));
		aprint_verbose(": ");
		acpidisp_print_od_attrs(od->od_attrs);
		aprint_verbose("\n");
	}
}

/*
 * general purpose range printing function
 * 1 -> 1
 * 1 2 4 6 7-> [1-2,4,6-7]
 */
static void
ranger(uint8_t *a, size_t l, void (*pr)(const char *, ...) __printflike(1, 2))
{
	uint8_t b, e; 

	if (l > 1)
		(*pr)("[");

	for (size_t i = 0; i < l; i++) {
		for (b = e = a[i]; i + 1 < l && a[i + 1] == e + 1; i++, e++)
			continue;
		(*pr)("%"PRIu8, b);
		if (b != e)
			(*pr)("-%"PRIu8, e);
		if (i < l - 1)
			(*pr)(",");
	}

	if (l > 1)
		(*pr)("]");
}

static void
acpidisp_print_brctl(device_t self, const struct acpidisp_brctl *bc)
{
	KASSERT(bc != NULL);

	aprint_verbose_dev(self, "brightness levels: ");
	ranger(bc->bc_level, bc->bc_level_count, aprint_verbose);
	aprint_verbose("\n");
}

static void
acpidisp_print_od_attrs(acpidisp_od_attrs_t oda)
{
	const char *type;

	if (oda.fmt.device_id_scheme == 1) {
		/* Uses the device ID scheme introduced in ACPI 3.0. */
		switch (oda.fmt.type) {
		case ACPI_DISP_OUT_ATTR_TYPE_OTHER:
			type = "Other";
			break;
		case ACPI_DISP_OUT_ATTR_TYPE_VGA:
			type = "VGA Analog Monitor";
			break;
		case ACPI_DISP_OUT_ATTR_TYPE_TV:
			type = "TV/HDTV Monitor";
			break;
		case ACPI_DISP_OUT_ATTR_TYPE_EXTDIG:
			type = "Ext. Digital Monitor";
			break;
		case ACPI_DISP_OUT_ATTR_TYPE_INTDFP:
			type = "Int. Digital Flat Panel";
			break;
		default:
			type = "Invalid";
			break;
		}

		aprint_verbose("%s, index %d, port %d",
		    type, oda.fmt.index, oda.fmt.port);
	} else {
		/* Uses vendor-specific device IDs. */
		switch (oda.device_id) {
		case ACPI_DISP_OUT_LEGACY_DEVID_MONITOR:
			type = "Ext. Monitor";
			break;
		case ACPI_DISP_OUT_LEGACY_DEVID_PANEL:
			type = "LCD Panel";
			break;
		case ACPI_DISP_OUT_LEGACY_DEVID_TV:
			type = "TV";
			break;
		default:
			type = "Unknown Output Device";
			break;
		}

		aprint_verbose("%s", type);
	}

	aprint_verbose(", head %d", oda.fmt.head_id);
	if (oda.fmt.bios_detect)
		aprint_verbose(", bios detect");
	if (oda.fmt.non_vga)
		aprint_verbose(", non vga");
}

/*
 * General-purpose utility functions.
 */

/*
 * acpidisp_has_method:
 *
 *	Returns true if and only if (a) the object handle.path exists and
 *	(b) this object is a method or has the given type.
 */
static bool
acpidisp_has_method(ACPI_HANDLE handle, const char *path, ACPI_OBJECT_TYPE type)
{
	ACPI_HANDLE hdl;
	ACPI_OBJECT_TYPE typ;

	KASSERT(handle != NULL);

	if (ACPI_FAILURE(AcpiGetHandle(handle, path, &hdl)))
		return false;

	if (ACPI_FAILURE(AcpiGetType(hdl, &typ)))
		return false;

	if (typ != ACPI_TYPE_METHOD && typ != type)
		return false;

	return true;
}

/*
 * acpidisp_eval_package:
 *
 *	Evaluate a package (with an expected minimum number of elements).
 *	Caller must free *pkg by ACPI_FREE().
 */
static ACPI_STATUS
acpidisp_eval_package(ACPI_HANDLE handle, const char *path, ACPI_OBJECT **pkg,
    unsigned int mincount)
{
	ACPI_BUFFER buf;
	ACPI_OBJECT *obj;
	ACPI_STATUS rv;

	rv = acpi_eval_struct(handle, path, &buf);
	if (ACPI_FAILURE(rv))
		return rv;

	if (buf.Length == 0 || buf.Pointer == NULL)
		return AE_NULL_OBJECT;

	obj = buf.Pointer;

	if (obj->Type != ACPI_TYPE_PACKAGE) {
		ACPI_FREE(obj);
		return AE_TYPE;
	}

	if (obj->Package.Count < mincount) {
		ACPI_FREE(obj);
		return AE_BAD_DATA;
	}

	*pkg = obj;
	return rv;
}

/*
 * acpidisp_array_search:
 *
 *	Look for a value v in a sorted array a of n integers (n > 0).  Fill *l
 *	and *u as follows:
 *
 *	   *l = Max {a[i] | a[i] <= v or i = 0}
 *	   *u = Min {a[i] | a[i] >= v or i = n-1}
 */
static void
acpidisp_array_search(const uint8_t *a, uint16_t n, int v, uint8_t *l, uint8_t *u)
{
	uint16_t i, j, m;

	if (v <= a[0]) {
		*l = a[0];
		*u = a[0];
		return;
	}
	if (v >= a[n-1]) {
		*l = a[n-1];
		*u = a[n-1];
		return;
	}

	for (i = 0, j = n - 1; j - i > 1; ) {
		m = (i + j) / 2;

		if (a[m] == v) {
			*l = v;
			*u = v;
			return;
		}

		if (a[m] < v)
			i = m;
		else
			j = m;
	}

	/* Here a[i] < v < a[j] and j = i + 1. */
	*l = a[i];
	*u = a[j];
	return;
}

MODULE(MODULE_CLASS_DRIVER, acpivga, NULL);

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
acpivga_modcmd(modcmd_t cmd, void *aux)
{
	int rv = 0;

	switch (cmd) {

	case MODULE_CMD_INIT:

#ifdef _MODULE
		rv = config_init_component(cfdriver_ioconf_acpivga,
		    cfattach_ioconf_acpivga, cfdata_ioconf_acpivga);
#endif
		break;

	case MODULE_CMD_FINI:

#ifdef _MODULE
		rv = config_fini_component(cfdriver_ioconf_acpivga,
		    cfattach_ioconf_acpivga, cfdata_ioconf_acpivga);
#endif
		break;

	default:
		rv = ENOTTY;
	}

	return rv;
}

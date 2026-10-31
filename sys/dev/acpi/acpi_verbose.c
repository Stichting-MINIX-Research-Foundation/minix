/*	$NetBSD: acpi_verbose.c,v 1.19 2018/04/07 15:49:52 christos Exp $ */

/*-
 * Copyright (c) 2003, 2007, 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum of By Noon Software, Inc, and Jukka Ruohonen.
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
__KERNEL_RCSID(0, "$NetBSD: acpi_verbose.c,v 1.19 2018/04/07 15:49:52 christos Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/module.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidevs_data.h>
#include <dev/acpi/acpi_pci.h>

#include <prop/proplib.h>

#define _COMPONENT ACPI_UTILITIES
ACPI_MODULE_NAME   ("acpi_verbose")

static bool	   acpiverbose_modcmd_prop(prop_dictionary_t);

void		   acpi_print_verbose_real(struct acpi_softc *);
void		   acpi_print_dev_real(const char *);
static void	   acpi_print_madt(struct acpi_softc *);
static ACPI_STATUS acpi_print_madt_callback(ACPI_SUBTABLE_HEADER *, void *);
static void	   acpi_print_fadt(struct acpi_softc *);
static void	   acpi_print_devnodes(struct acpi_softc *);
static void	   acpi_print_tree(struct acpi_devnode *, uint32_t);

extern ACPI_TABLE_HEADER *madt_header;

MODULE(MODULE_CLASS_MISC, acpiverbose, NULL);

static int
acpiverbose_modcmd(modcmd_t cmd, void *arg)
{
	static void (*saved_print_verbose)(struct acpi_softc *);
	static void (*saved_print_dev)(const char *);
	bool dump;

	dump = false;

	switch (cmd) {

	case MODULE_CMD_INIT:
		saved_print_verbose = acpi_print_verbose;
		saved_print_dev = acpi_print_dev;
		acpi_print_verbose = acpi_print_verbose_real;
		acpi_print_dev = acpi_print_dev_real;
		acpi_verbose_loaded = 1;

		if (arg != NULL)
			dump = acpiverbose_modcmd_prop(arg);

		if (dump != false)
			acpi_print_verbose_real(acpi_softc);

		return 0;

	case MODULE_CMD_FINI:
		acpi_print_verbose = saved_print_verbose;
		acpi_print_dev = saved_print_dev;
		acpi_verbose_loaded = 0;
		return 0;

	default:
		return ENOTTY;
	}
}

static bool
acpiverbose_modcmd_prop(prop_dictionary_t dict)
{
	prop_object_t obj;

	obj = prop_dictionary_get(dict, "dump");

	if (obj == NULL || prop_object_type(obj) != PROP_TYPE_BOOL)
		return false;

	return prop_bool_true(obj);
}

void
acpi_print_verbose_real(struct acpi_softc *sc)
{

	acpi_print_madt(sc);
	acpi_print_fadt(sc);
	acpi_print_devnodes(sc);
	acpi_print_tree(sc->sc_root, 0);
}

void
acpi_print_dev_real(const char *pnpstr)
{
	int i;

	for (i = 0; i < __arraycount(acpi_knowndevs); i++) {

		if (strcmp(acpi_knowndevs[i].pnp, pnpstr) == 0)
			aprint_normal("[%s] ", acpi_knowndevs[i].str);
	}
}

static void
acpi_print_madt(struct acpi_softc *sc)
{
	ACPI_STATUS rv;

	rv = acpi_madt_map();

	if (ACPI_FAILURE(rv) && rv != AE_ALREADY_EXISTS)
		return;

	if (madt_header == NULL)
		return;

	acpi_madt_walk(acpi_print_madt_callback, sc);
}

static ACPI_STATUS
acpi_print_madt_callback(ACPI_SUBTABLE_HEADER *hdr, void *aux)
{
	struct acpi_softc *sc = aux;
	device_t self = sc->sc_dev;

	/*
	 * See ACPI 4.0, section 5.2.12.
	 */
	switch (hdr->Type) {

	case ACPI_MADT_TYPE_LOCAL_APIC:

		aprint_normal_dev(self, "[MADT] %-15s: "
		    "CPU ID %u, LAPIC ID %u, FLAGS 0x%02X", "LAPIC",
		    ((ACPI_MADT_LOCAL_APIC *)hdr)->ProcessorId,
		    ((ACPI_MADT_LOCAL_APIC *)hdr)->Id,
		    ((ACPI_MADT_LOCAL_APIC *)hdr)->LapicFlags);

		break;

	case ACPI_MADT_TYPE_IO_APIC:

		aprint_normal_dev(self, "[MADT] %-15s: "
		    "ID %u, GSI %u, ADDR 0x%04X", "I/O APIC",
		    ((ACPI_MADT_IO_APIC *)hdr)->Id,
		    ((ACPI_MADT_IO_APIC *)hdr)->GlobalIrqBase,
		    ((ACPI_MADT_IO_APIC *)hdr)->Address);

		break;

	case ACPI_MADT_TYPE_INTERRUPT_OVERRIDE:

		aprint_normal_dev(self, "[MADT] %-15s: "
		    "BUS %u, IRQ %u, GSI %u, FLAGS 0x%02X", "INTR OVERRIDE",
		    ((ACPI_MADT_INTERRUPT_OVERRIDE *)hdr)->Bus,
		    ((ACPI_MADT_INTERRUPT_OVERRIDE *)hdr)->SourceIrq,
		    ((ACPI_MADT_INTERRUPT_OVERRIDE *)hdr)->GlobalIrq,
		    ((ACPI_MADT_INTERRUPT_OVERRIDE *)hdr)->IntiFlags);

		break;

	case ACPI_MADT_TYPE_NMI_SOURCE:

		aprint_normal_dev(self, "[MADT] %-15s: "
		    "GSI %u, FLAGS 0x%02X", "NMI SOURCE",
		    ((ACPI_MADT_NMI_SOURCE *)hdr)->GlobalIrq,
		    ((ACPI_MADT_NMI_SOURCE *)hdr)->IntiFlags);

		break;

	case ACPI_MADT_TYPE_LOCAL_APIC_NMI:

		aprint_normal_dev(self, "[MADT] %-15s: "
		    "CPU ID %u, LINT %u, FLAGS 0x%02X", "LAPIC NMI",
		    ((ACPI_MADT_LOCAL_APIC_NMI *)hdr)->ProcessorId,
		    ((ACPI_MADT_LOCAL_APIC_NMI *)hdr)->Lint,
		    ((ACPI_MADT_LOCAL_APIC_NMI *)hdr)->IntiFlags);

		break;

	case ACPI_MADT_TYPE_LOCAL_APIC_OVERRIDE:

		aprint_normal_dev(self, "[MADT] %-15s: "
		    "ADDR 0x%016" PRIX64"", "APIC OVERRIDE",
		    ((ACPI_MADT_LOCAL_APIC_OVERRIDE *)hdr)->Address);

		break;

	case ACPI_MADT_TYPE_IO_SAPIC:

		aprint_normal_dev(self, "[MADT] %-15s: "
		    "ID %u, GSI %u, ADDR 0x%016" PRIX64"", "I/O SAPIC",
		    ((ACPI_MADT_IO_SAPIC *)hdr)->Id,
		    ((ACPI_MADT_IO_SAPIC *)hdr)->GlobalIrqBase,
		    ((ACPI_MADT_IO_SAPIC *)hdr)->Address);

		break;

	case ACPI_MADT_TYPE_LOCAL_SAPIC:

		aprint_normal_dev(self, "[MADT] %-15s: "
		    "CPU ID %u, ID %u, EID %u, UID %u, FLAGS 0x%02X", "LSAPIC",
		    ((ACPI_MADT_LOCAL_SAPIC*)hdr)->ProcessorId,
		    ((ACPI_MADT_LOCAL_SAPIC*)hdr)->Id,
		    ((ACPI_MADT_LOCAL_SAPIC*)hdr)->Eid,
		    ((ACPI_MADT_LOCAL_SAPIC*)hdr)->Uid,
		    ((ACPI_MADT_LOCAL_SAPIC*)hdr)->LapicFlags);

		break;

	case ACPI_MADT_TYPE_INTERRUPT_SOURCE:

		aprint_normal_dev(self, "[MADT] %-15s: ID %u, EID %u, "
		    "TYPE %u, PMI %u, GSI %u, FLAGS 0x%02X", "INTR SOURCE",
		    ((ACPI_MADT_INTERRUPT_SOURCE *)hdr)->Id,
		    ((ACPI_MADT_INTERRUPT_SOURCE *)hdr)->Eid,
		    ((ACPI_MADT_INTERRUPT_SOURCE *)hdr)->Type,
		    ((ACPI_MADT_INTERRUPT_SOURCE *)hdr)->IoSapicVector,
		    ((ACPI_MADT_INTERRUPT_SOURCE *)hdr)->GlobalIrq,
		    ((ACPI_MADT_INTERRUPT_SOURCE *)hdr)->Flags);

		break;

	case ACPI_MADT_TYPE_LOCAL_X2APIC:

		aprint_normal_dev(self, "[MADT] %-15s: "
		    "ID %u, UID %u, FLAGS 0x%02X", "X2APIC",
		    ((ACPI_MADT_LOCAL_X2APIC *)hdr)->LocalApicId,
		    ((ACPI_MADT_LOCAL_X2APIC *)hdr)->Uid,
		    ((ACPI_MADT_LOCAL_X2APIC *)hdr)->LapicFlags);

		break;

	case ACPI_MADT_TYPE_LOCAL_X2APIC_NMI:

		aprint_normal_dev(self, "[MADT] %-15s: "
		    "UID %u, LINT %u, FLAGS 0x%02X", "X2APIC NMI",
		    ((ACPI_MADT_LOCAL_X2APIC_NMI *)hdr)->Uid,
		    ((ACPI_MADT_LOCAL_X2APIC_NMI *)hdr)->Lint,
		    ((ACPI_MADT_LOCAL_X2APIC_NMI *)hdr)->IntiFlags);

		break;

	default:
		aprint_normal_dev(self, "[MADT] %-15s", "UNKNOWN");
		break;
	}

	aprint_normal("\n");

	return AE_OK;
}

static void
acpi_print_fadt(struct acpi_softc *sc)
{
	uint32_t i;

	/*
	 * See ACPI 4.0, section 5.2.9.
	 */
	struct acpi_fadt {
		uint32_t	 fadt_offset;
		const char	*fadt_name;
		uint64_t	 fadt_value;
	};

	struct acpi_fadt_genaddr {
		uint32_t		 fadt_offset;
		const char		*fadt_name;
		ACPI_GENERIC_ADDRESS	 fadt_value;
	};

	const struct acpi_fadt acpi_fadt_table[] = {

		{ 36,	"FACS",		 AcpiGbl_FADT.Facs		},
		{ 40,	"DSDT",		 AcpiGbl_FADT.Dsdt		},
		{ 44,	"INT_MODEL",	 AcpiGbl_FADT.Model		},
		{ 45,	"PM_PROFILE",	 AcpiGbl_FADT.PreferredProfile	},
		{ 46,	"SCI_INT",	 AcpiGbl_FADT.SciInterrupt	},
		{ 48,	"SMI_CMD",	 AcpiGbl_FADT.SmiCommand	},
		{ 52,	"ACPI_ENABLE",	 AcpiGbl_FADT.AcpiEnable	},
		{ 53,	"ACPI_DISABLE",	 AcpiGbl_FADT.AcpiDisable	},
		{ 54,	"S4BIOS_REQ",	 AcpiGbl_FADT.S4BiosRequest	},
		{ 55,	"PSTATE_CNT",	 AcpiGbl_FADT.PstateControl	},
		{ 56,	"PM1a_EVT_BLK",	 AcpiGbl_FADT.Pm1aEventBlock	},
		{ 60,	"PM1b_EVT_BLK",	 AcpiGbl_FADT.Pm1bEventBlock	},
		{ 64,	"PM1a_CNT_BLK",	 AcpiGbl_FADT.Pm1aControlBlock	},
		{ 68,	"PM1b_CNT_BLK",	 AcpiGbl_FADT.Pm1bControlBlock	},
		{ 72,	"PM2_CNT_BLK",	 AcpiGbl_FADT.Pm2ControlBlock	},
		{ 76,	"PM_TMR_BLK",	 AcpiGbl_FADT.PmTimerBlock	},
		{ 80,	"GPE0_BLK",	 AcpiGbl_FADT.Gpe0Block		},
		{ 84,	"GPE1_BLK",	 AcpiGbl_FADT.Gpe1Block		},
		{ 88,	"PM1_EVT_LEN",	 AcpiGbl_FADT.Pm1EventLength	},
		{ 89,	"PM1_CNT_LEN",	 AcpiGbl_FADT.Pm1ControlLength	},
		{ 90,	"PM2_CNT_LEN",	 AcpiGbl_FADT.Pm2ControlLength	},
		{ 91,	"PM_TMR_LEN",	 AcpiGbl_FADT.PmTimerLength	},
		{ 92,	"GPE0_BLK_LEN",	 AcpiGbl_FADT.Gpe0BlockLength	},
		{ 93,	"GPE1_BLK_LEN",	 AcpiGbl_FADT.Gpe1BlockLength	},
		{ 94,	"GPE1_BASE",	 AcpiGbl_FADT.Gpe1Base		},
		{ 95,	"CST_CNT",	 AcpiGbl_FADT.CstControl	},
		{ 96,	"P_LVL2_LAT",	 AcpiGbl_FADT.C2Latency		},
		{ 98,	"P_LVL3_LAT",	 AcpiGbl_FADT.C3Latency		},
		{ 100,	"FLUSH_SIZE",	 AcpiGbl_FADT.FlushSize		},
		{ 102,	"FLUSH_STRIDE",	 AcpiGbl_FADT.FlushStride	},
		{ 104,	"DUTY_OFFSET",	 AcpiGbl_FADT.DutyOffset	},
		{ 105,	"DUTY_WIDTH",	 AcpiGbl_FADT.DutyWidth		},
		{ 106,	"DAY_ALRM",	 AcpiGbl_FADT.DayAlarm		},
		{ 107,	"MON_ALRM",	 AcpiGbl_FADT.MonthAlarm	},
		{ 108,	"CENTURY",	 AcpiGbl_FADT.Century		},
		{ 109,	"IAPC_BOOT_ARCH",AcpiGbl_FADT.BootFlags		},
		{ 128,	"RESET_VALUE",	 AcpiGbl_FADT.ResetValue	},
		{ 129,	"ARM_BOOT_ARCH", AcpiGbl_FADT.ArmBootFlags	},
		{ 132,	"X_FACS",	 AcpiGbl_FADT.XFacs  		},
		{ 140,	"X_DSDT",	 AcpiGbl_FADT.XDsdt  		},
	};

	const struct acpi_fadt_genaddr acpi_fadt_genaddr_table[] = {

		{ 116,	"RESET_REG",	 AcpiGbl_FADT.ResetRegister  	},
		{ 148,	"X_PM1a_EVT_BLK",AcpiGbl_FADT.XPm1aEventBlock	},
		{ 160,	"X_PM1b_EVT_BLK",AcpiGbl_FADT.XPm1bEventBlock	},
		{ 172,	"X_PM1a_CNT_BLK",AcpiGbl_FADT.XPm1aControlBlock	},
		{ 184,	"X_PM1b_CNT_BLK",AcpiGbl_FADT.XPm1bControlBlock	},
		{ 196,	"X_PM2_CNT_BLK", AcpiGbl_FADT.XPm2ControlBlock	},
		{ 208,	"X_PM_TMR_BLK",	 AcpiGbl_FADT.XPmTimerBlock	},
		{ 220,	"X_GPE0_BLK",	 AcpiGbl_FADT.XGpe0Block	},
		{ 232,	"X_GPE1_BLK",	 AcpiGbl_FADT.XGpe1Block	},
		{ 244,	"SLEEP_CTRL_REG",AcpiGbl_FADT.SleepControl	},
		{ 256,	"SLEEP_STAT_REG",AcpiGbl_FADT.SleepStatus	},
	};

	const struct acpi_fadt acpi_fadt_flags[] = {

		{ 0,	"WBINVD",	ACPI_FADT_WBINVD		},
		{ 1,	"WBINVD_FLUSH",	ACPI_FADT_WBINVD_FLUSH		},
		{ 2,	"PROC_C1",	ACPI_FADT_C1_SUPPORTED		},
		{ 3,	"P_LVL2_UP",	ACPI_FADT_C2_MP_SUPPORTED	},
		{ 4,	"PWR_BUTTON",	ACPI_FADT_POWER_BUTTON		},
		{ 5,	"SLP_BUTTON",	ACPI_FADT_SLEEP_BUTTON		},
		{ 6,	"FIX_RTC",	ACPI_FADT_FIXED_RTC		},
		{ 7,	"RTC_S4",	ACPI_FADT_S4_RTC_WAKE		},
		{ 8,	"TMR_VAL_EXT",	ACPI_FADT_32BIT_TIMER		},
		{ 9,	"DCK_CAP",	ACPI_FADT_DOCKING_SUPPORTED	},
		{ 10,	"RESET_REG_SUP",ACPI_FADT_RESET_REGISTER	},
		{ 11,	"SEALED_CASE",	ACPI_FADT_SEALED_CASE		},
		{ 12,	"HEADLESS",	ACPI_FADT_HEADLESS		},
		{ 13,	"CPU_SW_SLP",	ACPI_FADT_SLEEP_TYPE		},
		{ 14,	"PCI_EXP_WAK",	ACPI_FADT_PCI_EXPRESS_WAKE	},
		{ 15,	"PLATFORM_CLK", ACPI_FADT_PLATFORM_CLOCK	},
		{ 16,	"S4_RTC_STS",	ACPI_FADT_S4_RTC_VALID		},
		{ 17,	"REMOTE_POWER", ACPI_FADT_REMOTE_POWER_ON	},
		{ 18,	"APIC_CLUSTER",	ACPI_FADT_APIC_CLUSTER		},
		{ 19,	"APIC_PHYSICAL",ACPI_FADT_APIC_PHYSICAL		},
		{ 20,	"HW_REDUCED",	ACPI_FADT_HW_REDUCED		},
		{ 21,	"LOW_POWER_S0",	ACPI_FADT_LOW_POWER_S0		},
	};

	for (i = 0; i < __arraycount(acpi_fadt_table); i++) {

		aprint_normal_dev(sc->sc_dev,
		    "[FADT] %-15s: 0x%016" PRIX64"\n",
		    acpi_fadt_table[i].fadt_name,
		    acpi_fadt_table[i].fadt_value);
	}

	for (i = 0; i < __arraycount(acpi_fadt_genaddr_table); i++) {

		aprint_normal_dev(sc->sc_dev,
		    "[FADT] %-15s: 0x%016" PRIX64", "
		    "SPACE ID %u, BIT WIDTH %u, BIT OFFSET %u, "
		    "ACCESS WIDTH %u\n",
		    acpi_fadt_genaddr_table[i].fadt_name,
		    acpi_fadt_genaddr_table[i].fadt_value.Address,
		    acpi_fadt_genaddr_table[i].fadt_value.SpaceId,
		    acpi_fadt_genaddr_table[i].fadt_value.BitWidth,
		    acpi_fadt_genaddr_table[i].fadt_value.BitOffset,
		    acpi_fadt_genaddr_table[i].fadt_value.AccessWidth);
	}

	for (i = 0; i < __arraycount(acpi_fadt_flags); i++) {

		aprint_normal_dev(sc->sc_dev,
		    "[FADT] %-15s: 0x%016" PRIX64"\n",
		    acpi_fadt_flags[i].fadt_name, AcpiGbl_FADT.Flags &
		    acpi_fadt_flags[i].fadt_value);

		KASSERT(i ==  acpi_fadt_flags[i].fadt_offset);
		KASSERT(__BIT(acpi_fadt_flags[i].fadt_offset) ==
		              acpi_fadt_flags[i].fadt_value);
	}
}

static void
acpi_print_devnodes(struct acpi_softc *sc)
{
	struct acpi_devnode *ad;
	ACPI_DEVICE_INFO *di;

	SIMPLEQ_FOREACH(ad, &sc->ad_head, ad_list) {

		di = ad->ad_devinfo;
		aprint_normal_dev(sc->sc_dev, "[%-4s] ", ad->ad_name);

		aprint_normal("HID %-10s ",
		    ((di->Valid & ACPI_VALID_HID) != 0) ?
		    di->HardwareId.String: "-");

		aprint_normal("UID %-4s ",
		    ((di->Valid & ACPI_VALID_UID) != 0) ?
		    di->UniqueId.String : "-");

		if ((di->Valid & ACPI_VALID_ADR) != 0)
			aprint_normal("ADR 0x%016" PRIX64"", di->Address);
		else
			aprint_normal("ADR -");

		aprint_normal("\n");
	}
	aprint_normal("\n");
}

static void
acpi_print_tree(struct acpi_devnode *ad, uint32_t level)
{
	struct acpi_devnode *child;
	device_t dev;
	char buf[5];
	uint32_t i;

	for (i = 0; i < level; i++)
		aprint_normal("    ");

	buf[0] = '\0';

	if ((ad->ad_flags & ACPI_DEVICE_POWER) != 0)
		(void)strlcat(buf, "P", sizeof(buf));

	if ((ad->ad_flags & ACPI_DEVICE_WAKEUP) != 0)
		(void)strlcat(buf, "W", sizeof(buf));

	if ((ad->ad_flags & ACPI_DEVICE_EJECT) != 0)
		(void)strlcat(buf, "E", sizeof(buf));

	aprint_normal("%-5s [%02u] [%s]", ad->ad_name, ad->ad_type, buf);

	if (ad->ad_device != NULL)
		aprint_normal(" <%s>", device_xname(ad->ad_device));

	if (ad->ad_pciinfo != NULL) {

		aprint_normal(" (PCI)");

		if ((ad->ad_pciinfo->ap_flags & ACPI_PCI_INFO_DEVICE) != 0)
			aprint_normal(" @ 0x%02X:0x%02X:0x%02X:0x%02X",
			    ad->ad_pciinfo->ap_segment,
			    ad->ad_pciinfo->ap_bus,
			    ad->ad_pciinfo->ap_device,
			    ad->ad_pciinfo->ap_function);

		if ((ad->ad_devinfo->Flags & ACPI_PCI_ROOT_BRIDGE) != 0)
			aprint_normal(" [R]");

		if ((ad->ad_pciinfo->ap_flags & ACPI_PCI_INFO_BRIDGE) != 0)
			aprint_normal(" [B] -> 0x%02X:0x%02X",
			    ad->ad_pciinfo->ap_segment,
			    ad->ad_pciinfo->ap_downbus);

		dev = acpi_pcidev_find_dev(ad);

		if (dev != NULL)
			aprint_normal(" <%s>", device_xname(dev));
	}

	aprint_normal("\n");

	SIMPLEQ_FOREACH(child, &ad->ad_child_head, ad_child_list)
	    acpi_print_tree(child, level + 1);
}

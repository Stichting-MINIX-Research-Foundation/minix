/*	$NetBSD: pci_subr.c,v 1.106 2013/08/05 07:53:31 msaitoh Exp $	*/

/*
 * Copyright (c) 1997 Zubin D. Dittia.  All rights reserved.
 * Copyright (c) 1995, 1996, 1998, 2000
 *	Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1994 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * PCI autoconfiguration support functions.
 *
 * Note: This file is also built into a userland library (libpci).
 * Pay attention to this when you make modifications.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pci_subr.c,v 1.106 2013/08/05 07:53:31 msaitoh Exp $");

#ifdef _KERNEL_OPT
#include "opt_pci.h"
#endif

#include <sys/param.h>

#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/intr.h>
#include <sys/module.h>
#else
#include <pci.h>
#include <stdbool.h>
#include <stdio.h>
#endif

#include <dev/pci/pcireg.h>
#ifdef _KERNEL
#include <dev/pci/pcivar.h>
#endif

/*
 * Descriptions of known PCI classes and subclasses.
 *
 * Subclasses are described in the same way as classes, but have a
 * NULL subclass pointer.
 */
struct pci_class {
	const char	*name;
	u_int		val;		/* as wide as pci_{,sub}class_t */
	const struct pci_class *subclasses;
};

static const struct pci_class pci_subclass_prehistoric[] = {
	{ "miscellaneous",	PCI_SUBCLASS_PREHISTORIC_MISC,	NULL,	},
	{ "VGA",		PCI_SUBCLASS_PREHISTORIC_VGA,	NULL,	},
	{ NULL,			0,				NULL,	},
};

static const struct pci_class pci_subclass_mass_storage[] = {
	{ "SCSI",		PCI_SUBCLASS_MASS_STORAGE_SCSI,	NULL,	},
	{ "IDE",		PCI_SUBCLASS_MASS_STORAGE_IDE,	NULL,	},
	{ "floppy",		PCI_SUBCLASS_MASS_STORAGE_FLOPPY, NULL, },
	{ "IPI",		PCI_SUBCLASS_MASS_STORAGE_IPI,	NULL,	},
	{ "RAID",		PCI_SUBCLASS_MASS_STORAGE_RAID,	NULL,	},
	{ "ATA",		PCI_SUBCLASS_MASS_STORAGE_ATA,	NULL,	},
	{ "SATA",		PCI_SUBCLASS_MASS_STORAGE_SATA,	NULL,	},
	{ "SAS",		PCI_SUBCLASS_MASS_STORAGE_SAS,	NULL,	},
	{ "NVM",		PCI_SUBCLASS_MASS_STORAGE_NVM,	NULL,	},
	{ "miscellaneous",	PCI_SUBCLASS_MASS_STORAGE_MISC,	NULL,	},
	{ NULL,			0,				NULL,	},
};

static const struct pci_class pci_subclass_network[] = {
	{ "ethernet",		PCI_SUBCLASS_NETWORK_ETHERNET,	NULL,	},
	{ "token ring",		PCI_SUBCLASS_NETWORK_TOKENRING,	NULL,	},
	{ "FDDI",		PCI_SUBCLASS_NETWORK_FDDI,	NULL,	},
	{ "ATM",		PCI_SUBCLASS_NETWORK_ATM,	NULL,	},
	{ "ISDN",		PCI_SUBCLASS_NETWORK_ISDN,	NULL,	},
	{ "WorldFip",		PCI_SUBCLASS_NETWORK_WORLDFIP,	NULL,	},
	{ "PCMIG Multi Computing", PCI_SUBCLASS_NETWORK_PCIMGMULTICOMP, NULL, },
	{ "miscellaneous",	PCI_SUBCLASS_NETWORK_MISC,	NULL,	},
	{ NULL,			0,				NULL,	},
};

static const struct pci_class pci_subclass_display[] = {
	{ "VGA",		PCI_SUBCLASS_DISPLAY_VGA,	NULL,	},
	{ "XGA",		PCI_SUBCLASS_DISPLAY_XGA,	NULL,	},
	{ "3D",			PCI_SUBCLASS_DISPLAY_3D,	NULL,	},
	{ "miscellaneous",	PCI_SUBCLASS_DISPLAY_MISC,	NULL,	},
	{ NULL,			0,				NULL,	},
};

static const struct pci_class pci_subclass_multimedia[] = {
	{ "video",		PCI_SUBCLASS_MULTIMEDIA_VIDEO,	NULL,	},
	{ "audio",		PCI_SUBCLASS_MULTIMEDIA_AUDIO,	NULL,	},
	{ "telephony",		PCI_SUBCLASS_MULTIMEDIA_TELEPHONY, NULL,},
	{ "HD audio",		PCI_SUBCLASS_MULTIMEDIA_HDAUDIO, NULL,	},
	{ "miscellaneous",	PCI_SUBCLASS_MULTIMEDIA_MISC,	NULL,	},
	{ NULL,			0,				NULL,	},
};

static const struct pci_class pci_subclass_memory[] = {
	{ "RAM",		PCI_SUBCLASS_MEMORY_RAM,	NULL,	},
	{ "flash",		PCI_SUBCLASS_MEMORY_FLASH,	NULL,	},
	{ "miscellaneous",	PCI_SUBCLASS_MEMORY_MISC,	NULL,	},
	{ NULL,			0,				NULL,	},
};

static const struct pci_class pci_subclass_bridge[] = {
	{ "host",		PCI_SUBCLASS_BRIDGE_HOST,	NULL,	},
	{ "ISA",		PCI_SUBCLASS_BRIDGE_ISA,	NULL,	},
	{ "EISA",		PCI_SUBCLASS_BRIDGE_EISA,	NULL,	},
	{ "MicroChannel",	PCI_SUBCLASS_BRIDGE_MC,		NULL,	},
	{ "PCI",		PCI_SUBCLASS_BRIDGE_PCI,	NULL,	},
	{ "PCMCIA",		PCI_SUBCLASS_BRIDGE_PCMCIA,	NULL,	},
	{ "NuBus",		PCI_SUBCLASS_BRIDGE_NUBUS,	NULL,	},
	{ "CardBus",		PCI_SUBCLASS_BRIDGE_CARDBUS,	NULL,	},
	{ "RACEway",		PCI_SUBCLASS_BRIDGE_RACEWAY,	NULL,	},
	{ "Semi-transparent PCI", PCI_SUBCLASS_BRIDGE_STPCI,	NULL,	},
	{ "InfiniBand",		PCI_SUBCLASS_BRIDGE_INFINIBAND,	NULL,	},
	{ "miscellaneous",	PCI_SUBCLASS_BRIDGE_MISC,	NULL,	},
	{ NULL,			0,				NULL,	},
};

static const struct pci_class pci_subclass_communications[] = {
	{ "serial",		PCI_SUBCLASS_COMMUNICATIONS_SERIAL,	NULL, },
	{ "parallel",		PCI_SUBCLASS_COMMUNICATIONS_PARALLEL,	NULL, },
	{ "multi-port serial",	PCI_SUBCLASS_COMMUNICATIONS_MPSERIAL,	NULL, },
	{ "modem",		PCI_SUBCLASS_COMMUNICATIONS_MODEM,	NULL, },
	{ "GPIB",		PCI_SUBCLASS_COMMUNICATIONS_GPIB,	NULL, },
	{ "smartcard",		PCI_SUBCLASS_COMMUNICATIONS_SMARTCARD,	NULL, },
	{ "miscellaneous",	PCI_SUBCLASS_COMMUNICATIONS_MISC,	NULL, },
	{ NULL,			0,					NULL, },
};

static const struct pci_class pci_subclass_system[] = {
	{ "interrupt",		PCI_SUBCLASS_SYSTEM_PIC,	NULL,	},
	{ "8237 DMA",		PCI_SUBCLASS_SYSTEM_DMA,	NULL,	},
	{ "8254 timer",		PCI_SUBCLASS_SYSTEM_TIMER,	NULL,	},
	{ "RTC",		PCI_SUBCLASS_SYSTEM_RTC,	NULL,	},
	{ "PCI Hot-Plug",	PCI_SUBCLASS_SYSTEM_PCIHOTPLUG, NULL,	},
	{ "SD Host Controller",	PCI_SUBCLASS_SYSTEM_SDHC,	NULL,	},
	{ "miscellaneous",	PCI_SUBCLASS_SYSTEM_MISC,	NULL,	},
	{ NULL,			0,				NULL,	},
};

static const struct pci_class pci_subclass_input[] = {
	{ "keyboard",		PCI_SUBCLASS_INPUT_KEYBOARD,	NULL,	},
	{ "digitizer",		PCI_SUBCLASS_INPUT_DIGITIZER,	NULL,	},
	{ "mouse",		PCI_SUBCLASS_INPUT_MOUSE,	NULL,	},
	{ "scanner",		PCI_SUBCLASS_INPUT_SCANNER,	NULL,	},
	{ "game port",		PCI_SUBCLASS_INPUT_GAMEPORT,	NULL,	},
	{ "miscellaneous",	PCI_SUBCLASS_INPUT_MISC,	NULL,	},
	{ NULL,			0,				NULL,	},
};

static const struct pci_class pci_subclass_dock[] = {
	{ "generic",		PCI_SUBCLASS_DOCK_GENERIC,	NULL,	},
	{ "miscellaneous",	PCI_SUBCLASS_DOCK_MISC,		NULL,	},
	{ NULL,			0,				NULL,	},
};

static const struct pci_class pci_subclass_processor[] = {
	{ "386",		PCI_SUBCLASS_PROCESSOR_386,	NULL,	},
	{ "486",		PCI_SUBCLASS_PROCESSOR_486,	NULL,	},
	{ "Pentium",		PCI_SUBCLASS_PROCESSOR_PENTIUM, NULL,	},
	{ "Alpha",		PCI_SUBCLASS_PROCESSOR_ALPHA,	NULL,	},
	{ "PowerPC",		PCI_SUBCLASS_PROCESSOR_POWERPC, NULL,	},
	{ "MIPS",		PCI_SUBCLASS_PROCESSOR_MIPS,	NULL,	},
	{ "Co-processor",	PCI_SUBCLASS_PROCESSOR_COPROC,	NULL,	},
	{ NULL,			0,				NULL,	},
};

static const struct pci_class pci_subclass_serialbus[] = {
	{ "Firewire",		PCI_SUBCLASS_SERIALBUS_FIREWIRE, NULL,	},
	{ "ACCESS.bus",		PCI_SUBCLASS_SERIALBUS_ACCESS,	NULL,	},
	{ "SSA",		PCI_SUBCLASS_SERIALBUS_SSA,	NULL,	},
	{ "USB",		PCI_SUBCLASS_SERIALBUS_USB,	NULL,	},
	/* XXX Fiber Channel/_FIBRECHANNEL */
	{ "Fiber Channel",	PCI_SUBCLASS_SERIALBUS_FIBER,	NULL,	},
	{ "SMBus",		PCI_SUBCLASS_SERIALBUS_SMBUS,	NULL,	},
	{ "InfiniBand",		PCI_SUBCLASS_SERIALBUS_INFINIBAND, NULL,},
	{ "IPMI",		PCI_SUBCLASS_SERIALBUS_IPMI,	NULL,	},
	{ "SERCOS",		PCI_SUBCLASS_SERIALBUS_SERCOS,	NULL,	},
	{ "CANbus",		PCI_SUBCLASS_SERIALBUS_CANBUS,	NULL,	},
	{ NULL,			0,				NULL,	},
};

static const struct pci_class pci_subclass_wireless[] = {
	{ "IrDA",		PCI_SUBCLASS_WIRELESS_IRDA,	NULL,	},
	{ "Consumer IR",	PCI_SUBCLASS_WIRELESS_CONSUMERIR, NULL,	},
	{ "RF",			PCI_SUBCLASS_WIRELESS_RF,	NULL,	},
	{ "bluetooth",		PCI_SUBCLASS_WIRELESS_BLUETOOTH, NULL,	},
	{ "broadband",		PCI_SUBCLASS_WIRELESS_BROADBAND, NULL,	},
	{ "802.11a (5 GHz)",	PCI_SUBCLASS_WIRELESS_802_11A,	NULL,	},
	{ "802.11b (2.4 GHz)",	PCI_SUBCLASS_WIRELESS_802_11B,	NULL,	},
	{ "miscellaneous",	PCI_SUBCLASS_WIRELESS_MISC,	NULL,	},
	{ NULL,			0,				NULL,	},
};

static const struct pci_class pci_subclass_i2o[] = {
	{ "standard",		PCI_SUBCLASS_I2O_STANDARD,	NULL,	},
	{ NULL,			0,				NULL,	},
};

static const struct pci_class pci_subclass_satcom[] = {
	{ "TV",			PCI_SUBCLASS_SATCOM_TV,	 	NULL,	},
	{ "audio",		PCI_SUBCLASS_SATCOM_AUDIO, 	NULL,	},
	{ "voice",		PCI_SUBCLASS_SATCOM_VOICE, 	NULL,	},
	{ "data",		PCI_SUBCLASS_SATCOM_DATA,	NULL,	},
	{ NULL,			0,				NULL,	},
};

static const struct pci_class pci_subclass_crypto[] = {
	{ "network/computing",	PCI_SUBCLASS_CRYPTO_NETCOMP, 	NULL,	},
	{ "entertainment",	PCI_SUBCLASS_CRYPTO_ENTERTAINMENT, NULL,},
	{ "miscellaneous",	PCI_SUBCLASS_CRYPTO_MISC, 	NULL,	},
	{ NULL,			0,				NULL,	},
};

static const struct pci_class pci_subclass_dasp[] = {
	{ "DPIO",		PCI_SUBCLASS_DASP_DPIO,		NULL,	},
	{ "Time and Frequency",	PCI_SUBCLASS_DASP_TIMEFREQ,	NULL,	},
	{ "synchronization",	PCI_SUBCLASS_DASP_SYNC,		NULL,	},
	{ "management",		PCI_SUBCLASS_DASP_MGMT,		NULL,	},
	{ "miscellaneous",	PCI_SUBCLASS_DASP_MISC,		NULL,	},
	{ NULL,			0,				NULL,	},
};

static const struct pci_class pci_class[] = {
	{ "prehistoric",	PCI_CLASS_PREHISTORIC,
	    pci_subclass_prehistoric,				},
	{ "mass storage",	PCI_CLASS_MASS_STORAGE,
	    pci_subclass_mass_storage,				},
	{ "network",		PCI_CLASS_NETWORK,
	    pci_subclass_network,				},
	{ "display",		PCI_CLASS_DISPLAY,
	    pci_subclass_display,				},
	{ "multimedia",		PCI_CLASS_MULTIMEDIA,
	    pci_subclass_multimedia,				},
	{ "memory",		PCI_CLASS_MEMORY,
	    pci_subclass_memory,				},
	{ "bridge",		PCI_CLASS_BRIDGE,
	    pci_subclass_bridge,				},
	{ "communications",	PCI_CLASS_COMMUNICATIONS,
	    pci_subclass_communications,			},
	{ "system",		PCI_CLASS_SYSTEM,
	    pci_subclass_system,				},
	{ "input",		PCI_CLASS_INPUT,
	    pci_subclass_input,					},
	{ "dock",		PCI_CLASS_DOCK,
	    pci_subclass_dock,					},
	{ "processor",		PCI_CLASS_PROCESSOR,
	    pci_subclass_processor,				},
	{ "serial bus",		PCI_CLASS_SERIALBUS,
	    pci_subclass_serialbus,				},
	{ "wireless",		PCI_CLASS_WIRELESS,
	    pci_subclass_wireless,				},
	{ "I2O",		PCI_CLASS_I2O,
	    pci_subclass_i2o,					},
	{ "satellite comm",	PCI_CLASS_SATCOM,
	    pci_subclass_satcom,				},
	{ "crypto",		PCI_CLASS_CRYPTO,
	    pci_subclass_crypto,				},
	{ "DASP",		PCI_CLASS_DASP,
	    pci_subclass_dasp,					},
	{ "undefined",		PCI_CLASS_UNDEFINED,
	    NULL,						},
	{ NULL,			0,
	    NULL,						},
};

void pci_load_verbose(void);

#if defined(_KERNEL)
/*
 * In kernel, these routines are provided and linked via the
 * pciverbose module.
 */
const char *pci_findvendor_stub(pcireg_t);
const char *pci_findproduct_stub(pcireg_t);

const char *(*pci_findvendor)(pcireg_t) = pci_findvendor_stub;
const char *(*pci_findproduct)(pcireg_t) = pci_findproduct_stub;
const char *pci_unmatched = "";
#else
/*
 * For userland we just set the vectors here.
 */
const char *(*pci_findvendor)(pcireg_t id_reg) = pci_findvendor_real;
const char *(*pci_findproduct)(pcireg_t id_reg) = pci_findproduct_real;
const char *pci_unmatched = "unmatched ";
#endif

int pciverbose_loaded = 0;

#if defined(_KERNEL)
/*
 * Routine to load the pciverbose kernel module as needed
 */
void pci_load_verbose(void)
{
	if (pciverbose_loaded == 0)
		module_autoload("pciverbose", MODULE_CLASS_MISC);
}

const char *pci_findvendor_stub(pcireg_t id_reg)
{
	pci_load_verbose();
	if (pciverbose_loaded)
		return pci_findvendor(id_reg);
	else
		return NULL;
}

const char *pci_findproduct_stub(pcireg_t id_reg)
{
	pci_load_verbose();
	if (pciverbose_loaded)
		return pci_findproduct(id_reg);
	else
		return NULL;
}
#endif

void
pci_devinfo(pcireg_t id_reg, pcireg_t class_reg, int showclass, char *cp,
    size_t l)
{
	pci_vendor_id_t vendor;
	pci_product_id_t product;
	pci_class_t class;
	pci_subclass_t subclass;
	pci_interface_t interface;
	pci_revision_t revision;
	const char *unmatched = pci_unmatched;
	const char *vendor_namep, *product_namep;
	const struct pci_class *classp, *subclassp;
	char *ep;

	ep = cp + l;

	vendor = PCI_VENDOR(id_reg);
	product = PCI_PRODUCT(id_reg);

	class = PCI_CLASS(class_reg);
	subclass = PCI_SUBCLASS(class_reg);
	interface = PCI_INTERFACE(class_reg);
	revision = PCI_REVISION(class_reg);

	vendor_namep = pci_findvendor(id_reg);
	product_namep = pci_findproduct(id_reg);

	classp = pci_class;
	while (classp->name != NULL) {
		if (class == classp->val)
			break;
		classp++;
	}

	subclassp = (classp->name != NULL) ? classp->subclasses : NULL;
	while (subclassp && subclassp->name != NULL) {
		if (subclass == subclassp->val)
			break;
		subclassp++;
	}

	if (vendor_namep == NULL)
		cp += snprintf(cp, ep - cp, "%svendor 0x%04x product 0x%04x",
		    unmatched, vendor, product);
	else if (product_namep != NULL)
		cp += snprintf(cp, ep - cp, "%s %s", vendor_namep,
		    product_namep);
	else
		cp += snprintf(cp, ep - cp, "%s product 0x%04x",
		    vendor_namep, product);
	if (showclass) {
		cp += snprintf(cp, ep - cp, " (");
		if (classp->name == NULL)
			cp += snprintf(cp, ep - cp,
			    "class 0x%02x, subclass 0x%02x", class, subclass);
		else {
			if (subclassp == NULL || subclassp->name == NULL)
				cp += snprintf(cp, ep - cp,
				    "%s, subclass 0x%02x",
				    classp->name, subclass);
			else
				cp += snprintf(cp, ep - cp, "%s %s",
				    subclassp->name, classp->name);
		}
		if (interface != 0)
			cp += snprintf(cp, ep - cp, ", interface 0x%02x",
			    interface);
		if (revision != 0)
			cp += snprintf(cp, ep - cp, ", revision 0x%02x",
			    revision);
		cp += snprintf(cp, ep - cp, ")");
	}
}

#ifdef _KERNEL
void
pci_aprint_devinfo_fancy(const struct pci_attach_args *pa, const char *naive,
			 const char *known, int addrev)
{
	char devinfo[256];

	if (known) {
		aprint_normal(": %s", known);
		if (addrev)
			aprint_normal(" (rev. 0x%02x)",
				      PCI_REVISION(pa->pa_class));
		aprint_normal("\n");
	} else {
		pci_devinfo(pa->pa_id, pa->pa_class, 0,
			    devinfo, sizeof(devinfo));
		aprint_normal(": %s (rev. 0x%02x)\n", devinfo,
			      PCI_REVISION(pa->pa_class));
	}
	if (naive)
		aprint_naive(": %s\n", naive);
	else
		aprint_naive("\n");
}
#endif

/*
 * Print out most of the PCI configuration registers.  Typically used
 * in a device attach routine like this:
 *
 *	#ifdef MYDEV_DEBUG
 *		printf("%s: ", device_xname(sc->sc_dev));
 *		pci_conf_print(pa->pa_pc, pa->pa_tag, NULL);
 *	#endif
 */

#define	i2o(i)	((i) * 4)
#define	o2i(o)	((o) / 4)
#define	onoff2(str, bit, onstr, offstr)					\
	printf("      %s: %s\n", (str), (rval & (bit)) ? onstr : offstr);
#define	onoff(str, bit)	onoff2(str, bit, "on", "off")

static void
pci_conf_print_common(
#ifdef _KERNEL
    pci_chipset_tag_t pc, pcitag_t tag,
#endif
    const pcireg_t *regs)
{
	const char *name;
	const struct pci_class *classp, *subclassp;
	pcireg_t rval;

	rval = regs[o2i(PCI_ID_REG)];
	name = pci_findvendor(rval);
	if (name)
		printf("    Vendor Name: %s (0x%04x)\n", name,
		    PCI_VENDOR(rval));
	else
		printf("    Vendor ID: 0x%04x\n", PCI_VENDOR(rval));
	name = pci_findproduct(rval);
	if (name)
		printf("    Device Name: %s (0x%04x)\n", name,
		    PCI_PRODUCT(rval));
	else
		printf("    Device ID: 0x%04x\n", PCI_PRODUCT(rval));

	rval = regs[o2i(PCI_COMMAND_STATUS_REG)];

	printf("    Command register: 0x%04x\n", rval & 0xffff);
	onoff("I/O space accesses", PCI_COMMAND_IO_ENABLE);
	onoff("Memory space accesses", PCI_COMMAND_MEM_ENABLE);
	onoff("Bus mastering", PCI_COMMAND_MASTER_ENABLE);
	onoff("Special cycles", PCI_COMMAND_SPECIAL_ENABLE);
	onoff("MWI transactions", PCI_COMMAND_INVALIDATE_ENABLE);
	onoff("Palette snooping", PCI_COMMAND_PALETTE_ENABLE);
	onoff("Parity error checking", PCI_COMMAND_PARITY_ENABLE);
	onoff("Address/data stepping", PCI_COMMAND_STEPPING_ENABLE);
	onoff("System error (SERR)", PCI_COMMAND_SERR_ENABLE);
	onoff("Fast back-to-back transactions", PCI_COMMAND_BACKTOBACK_ENABLE);
	onoff("Interrupt disable", PCI_COMMAND_INTERRUPT_DISABLE);

	printf("    Status register: 0x%04x\n", (rval >> 16) & 0xffff);
	onoff2("Interrupt status", PCI_STATUS_INT_STATUS, "active", "inactive");
	onoff("Capability List support", PCI_STATUS_CAPLIST_SUPPORT);
	onoff("66 MHz capable", PCI_STATUS_66MHZ_SUPPORT);
	onoff("User Definable Features (UDF) support", PCI_STATUS_UDF_SUPPORT);
	onoff("Fast back-to-back capable", PCI_STATUS_BACKTOBACK_SUPPORT);
	onoff("Data parity error detected", PCI_STATUS_PARITY_ERROR);

	printf("      DEVSEL timing: ");
	switch (rval & PCI_STATUS_DEVSEL_MASK) {
	case PCI_STATUS_DEVSEL_FAST:
		printf("fast");
		break;
	case PCI_STATUS_DEVSEL_MEDIUM:
		printf("medium");
		break;
	case PCI_STATUS_DEVSEL_SLOW:
		printf("slow");
		break;
	default:
		printf("unknown/reserved");	/* XXX */
		break;
	}
	printf(" (0x%x)\n", (rval & PCI_STATUS_DEVSEL_MASK) >> 25);

	onoff("Slave signaled Target Abort", PCI_STATUS_TARGET_TARGET_ABORT);
	onoff("Master received Target Abort", PCI_STATUS_MASTER_TARGET_ABORT);
	onoff("Master received Master Abort", PCI_STATUS_MASTER_ABORT);
	onoff("Asserted System Error (SERR)", PCI_STATUS_SPECIAL_ERROR);
	onoff("Parity error detected", PCI_STATUS_PARITY_DETECT);

	rval = regs[o2i(PCI_CLASS_REG)];
	for (classp = pci_class; classp->name != NULL; classp++) {
		if (PCI_CLASS(rval) == classp->val)
			break;
	}
	subclassp = (classp->name != NULL) ? classp->subclasses : NULL;
	while (subclassp && subclassp->name != NULL) {
		if (PCI_SUBCLASS(rval) == subclassp->val)
			break;
		subclassp++;
	}
	if (classp->name != NULL) {
		printf("    Class Name: %s (0x%02x)\n", classp->name,
		    PCI_CLASS(rval));
		if (subclassp != NULL && subclassp->name != NULL)
			printf("    Subclass Name: %s (0x%02x)\n",
			    subclassp->name, PCI_SUBCLASS(rval));
		else
			printf("    Subclass ID: 0x%02x\n", PCI_SUBCLASS(rval));
	} else {
		printf("    Class ID: 0x%02x\n", PCI_CLASS(rval));
		printf("    Subclass ID: 0x%02x\n", PCI_SUBCLASS(rval));
	}
	printf("    Interface: 0x%02x\n", PCI_INTERFACE(rval));
	printf("    Revision ID: 0x%02x\n", PCI_REVISION(rval));

	rval = regs[o2i(PCI_BHLC_REG)];
	printf("    BIST: 0x%02x\n", PCI_BIST(rval));
	printf("    Header Type: 0x%02x%s (0x%02x)\n", PCI_HDRTYPE_TYPE(rval),
	    PCI_HDRTYPE_MULTIFN(rval) ? "+multifunction" : "",
	    PCI_HDRTYPE(rval));
	printf("    Latency Timer: 0x%02x\n", PCI_LATTIMER(rval));
	printf("    Cache Line Size: 0x%02x\n", PCI_CACHELINE(rval));
}

static int
pci_conf_print_bar(
#ifdef _KERNEL
    pci_chipset_tag_t pc, pcitag_t tag,
#endif
    const pcireg_t *regs, int reg, const char *name
#ifdef _KERNEL
    , int sizebar
#endif
    )
{
	int width;
	pcireg_t rval, rval64h;
#ifdef _KERNEL
	int s;
	pcireg_t mask, mask64h;
#endif

	width = 4;

	/*
	 * Section 6.2.5.1, `Address Maps', tells us that:
	 *
	 * 1) The builtin software should have already mapped the
	 * device in a reasonable way.
	 *
	 * 2) A device which wants 2^n bytes of memory will hardwire
	 * the bottom n bits of the address to 0.  As recommended,
	 * we write all 1s and see what we get back.
	 */

	rval = regs[o2i(reg)];
	if (PCI_MAPREG_TYPE(rval) == PCI_MAPREG_TYPE_MEM &&
	    PCI_MAPREG_MEM_TYPE(rval) == PCI_MAPREG_MEM_TYPE_64BIT) {
		rval64h = regs[o2i(reg + 4)];
		width = 8;
	} else
		rval64h = 0;

#ifdef _KERNEL
	/* XXX don't size unknown memory type? */
	if (rval != 0 && sizebar) {
		/*
		 * The following sequence seems to make some devices
		 * (e.g. host bus bridges, which don't normally
		 * have their space mapped) very unhappy, to
		 * the point of crashing the system.
		 *
		 * Therefore, if the mapping register is zero to
		 * start out with, don't bother trying.
		 */
		s = splhigh();
		pci_conf_write(pc, tag, reg, 0xffffffff);
		mask = pci_conf_read(pc, tag, reg);
		pci_conf_write(pc, tag, reg, rval);
		if (PCI_MAPREG_TYPE(rval) == PCI_MAPREG_TYPE_MEM &&
		    PCI_MAPREG_MEM_TYPE(rval) == PCI_MAPREG_MEM_TYPE_64BIT) {
			pci_conf_write(pc, tag, reg + 4, 0xffffffff);
			mask64h = pci_conf_read(pc, tag, reg + 4);
			pci_conf_write(pc, tag, reg + 4, rval64h);
		} else
			mask64h = 0;
		splx(s);
	} else
		mask = mask64h = 0;
#endif /* _KERNEL */

	printf("    Base address register at 0x%02x", reg);
	if (name)
		printf(" (%s)", name);
	printf("\n      ");
	if (rval == 0) {
		printf("not implemented(?)\n");
		return width;
	}
	printf("type: ");
	if (PCI_MAPREG_TYPE(rval) == PCI_MAPREG_TYPE_MEM) {
		const char *type, *prefetch;

		switch (PCI_MAPREG_MEM_TYPE(rval)) {
		case PCI_MAPREG_MEM_TYPE_32BIT:
			type = "32-bit";
			break;
		case PCI_MAPREG_MEM_TYPE_32BIT_1M:
			type = "32-bit-1M";
			break;
		case PCI_MAPREG_MEM_TYPE_64BIT:
			type = "64-bit";
			break;
		default:
			type = "unknown (XXX)";
			break;
		}
		if (PCI_MAPREG_MEM_PREFETCHABLE(rval))
			prefetch = "";
		else
			prefetch = "non";
		printf("%s %sprefetchable memory\n", type, prefetch);
		switch (PCI_MAPREG_MEM_TYPE(rval)) {
		case PCI_MAPREG_MEM_TYPE_64BIT:
			printf("      base: 0x%016llx, ",
			    PCI_MAPREG_MEM64_ADDR(
				((((long long) rval64h) << 32) | rval)));
#ifdef _KERNEL
			if (sizebar)
				printf("size: 0x%016llx",
				    PCI_MAPREG_MEM64_SIZE(
				      ((((long long) mask64h) << 32) | mask)));
			else
#endif /* _KERNEL */
				printf("not sized");
			printf("\n");
			break;
		case PCI_MAPREG_MEM_TYPE_32BIT:
		case PCI_MAPREG_MEM_TYPE_32BIT_1M:
		default:
			printf("      base: 0x%08x, ",
			    PCI_MAPREG_MEM_ADDR(rval));
#ifdef _KERNEL
			if (sizebar)
				printf("size: 0x%08x",
				    PCI_MAPREG_MEM_SIZE(mask));
			else
#endif /* _KERNEL */
				printf("not sized");
			printf("\n");
			break;
		}
	} else {
#ifdef _KERNEL
		if (sizebar)
			printf("%d-bit ", mask & ~0x0000ffff ? 32 : 16);
#endif /* _KERNEL */
		printf("i/o\n");
		printf("      base: 0x%08x, ", PCI_MAPREG_IO_ADDR(rval));
#ifdef _KERNEL
		if (sizebar)
			printf("size: 0x%08x", PCI_MAPREG_IO_SIZE(mask));
		else
#endif /* _KERNEL */
			printf("not sized");
		printf("\n");
	}

	return width;
}

static void
pci_conf_print_regs(const pcireg_t *regs, int first, int pastlast)
{
	int off, needaddr, neednl;

	needaddr = 1;
	neednl = 0;
	for (off = first; off < pastlast; off += 4) {
		if ((off % 16) == 0 || needaddr) {
			printf("    0x%02x:", off);
			needaddr = 0;
		}
		printf(" 0x%08x", regs[o2i(off)]);
		neednl = 1;
		if ((off % 16) == 12) {
			printf("\n");
			neednl = 0;
		}
	}
	if (neednl)
		printf("\n");
}

static void
pci_conf_print_type0(
#ifdef _KERNEL
    pci_chipset_tag_t pc, pcitag_t tag,
#endif
    const pcireg_t *regs
#ifdef _KERNEL
    , int sizebars
#endif
    )
{
	int off, width;
	pcireg_t rval;

	for (off = PCI_MAPREG_START; off < PCI_MAPREG_END; off += width) {
#ifdef _KERNEL
		width = pci_conf_print_bar(pc, tag, regs, off, NULL, sizebars);
#else
		width = pci_conf_print_bar(regs, off, NULL);
#endif
	}

	printf("    Cardbus CIS Pointer: 0x%08x\n", regs[o2i(0x28)]);

	rval = regs[o2i(PCI_SUBSYS_ID_REG)];
	printf("    Subsystem vendor ID: 0x%04x\n", PCI_VENDOR(rval));
	printf("    Subsystem ID: 0x%04x\n", PCI_PRODUCT(rval));

	/* XXX */
	printf("    Expansion ROM Base Address: 0x%08x\n", regs[o2i(0x30)]);

	if (regs[o2i(PCI_COMMAND_STATUS_REG)] & PCI_STATUS_CAPLIST_SUPPORT)
		printf("    Capability list pointer: 0x%02x\n",
		    PCI_CAPLIST_PTR(regs[o2i(PCI_CAPLISTPTR_REG)]));
	else
		printf("    Reserved @ 0x34: 0x%08x\n", regs[o2i(0x34)]);

	printf("    Reserved @ 0x38: 0x%08x\n", regs[o2i(0x38)]);

	rval = regs[o2i(PCI_INTERRUPT_REG)];
	printf("    Maximum Latency: 0x%02x\n", (rval >> 24) & 0xff);
	printf("    Minimum Grant: 0x%02x\n", (rval >> 16) & 0xff);
	printf("    Interrupt pin: 0x%02x ", PCI_INTERRUPT_PIN(rval));
	switch (PCI_INTERRUPT_PIN(rval)) {
	case PCI_INTERRUPT_PIN_NONE:
		printf("(none)");
		break;
	case PCI_INTERRUPT_PIN_A:
		printf("(pin A)");
		break;
	case PCI_INTERRUPT_PIN_B:
		printf("(pin B)");
		break;
	case PCI_INTERRUPT_PIN_C:
		printf("(pin C)");
		break;
	case PCI_INTERRUPT_PIN_D:
		printf("(pin D)");
		break;
	default:
		printf("(? ? ?)");
		break;
	}
	printf("\n");
	printf("    Interrupt line: 0x%02x\n", PCI_INTERRUPT_LINE(rval));
}

static void
pci_print_pcie_L0s_latency(uint32_t val)
{

	switch (val) {
	case 0x0:
		printf("Less than 64ns\n");
		break;
	case 0x1:
	case 0x2:
	case 0x3:
		printf("%dns to less than %dns\n", 32 << val, 32 << (val + 1));
		break;
	case 0x4:
		printf("512ns to less than 1us\n");
		break;
	case 0x5:
		printf("1us to less than 2us\n");
		break;
	case 0x6:
		printf("2us - 4us\n");
		break;
	case 0x7:
		printf("More than 4us\n");
		break;
	}
}

static void
pci_print_pcie_L1_latency(uint32_t val)
{

	switch (val) {
	case 0x0:
		printf("Less than 1us\n");
		break;
	case 0x6:
		printf("32us - 64us\n");
		break;
	case 0x7:
		printf("More than 64us\n");
		break;
	default:
		printf("%dus to less than %dus\n", 1 << (val - 1), 1 << val);
		break;
	}
}

static void
pci_print_pcie_compl_timeout(uint32_t val)
{

	switch (val) {
	case 0x0:
		printf("50us to 50ms\n");
		break;
	case 0x5:
		printf("16ms to 55ms\n");
		break;
	case 0x6:
		printf("65ms to 210ms\n");
		break;
	case 0x9:
		printf("260ms to 900ms\n");
		break;
	case 0xa:
		printf("1s to 3.5s\n");
		break;
	default:
		printf("unknown %u value\n", val);
		break;
	}
}

static void
pci_conf_print_pcie_cap(const pcireg_t *regs, int capoff)
{
	pcireg_t reg; /* for each register */
	pcireg_t val; /* for each bitfield */
	bool check_link = false;
	bool check_slot = false;
	bool check_rootport = false;
	unsigned int pciever;
	static const char * const linkspeeds[] = {"2.5", "5.0", "8.0"};
	int i;

	printf("\n  PCI Express Capabilities Register\n");
	/* Capability Register */
	reg = regs[o2i(capoff)];
	printf("    Capability register: %04x\n", reg >> 16);
	pciever = (unsigned int)((reg & 0x000f0000) >> 16);
	printf("      Capability version: %u\n", pciever);
	printf("      Device type: ");
	switch ((reg & 0x00f00000) >> 20) {
	case 0x0:
		printf("PCI Express Endpoint device\n");
		check_link = true;
		break;
	case 0x1:
		printf("Legacy PCI Express Endpoint device\n");
		check_link = true;
		break;
	case 0x4:
		printf("Root Port of PCI Express Root Complex\n");
		check_link = true;
		check_slot = true;
		check_rootport = true;
		break;
	case 0x5:
		printf("Upstream Port of PCI Express Switch\n");
		break;
	case 0x6:
		printf("Downstream Port of PCI Express Switch\n");
		check_slot = true;
		check_rootport = true;
		break;
	case 0x7:
		printf("PCI Express to PCI/PCI-X Bridge\n");
		break;
	case 0x8:
		printf("PCI/PCI-X to PCI Express Bridge\n");
		break;
	case 0x9:
		printf("Root Complex Integrated Endpoint\n");
		break;
	case 0xa:
		check_rootport = true;
		printf("Root Complex Event Collector\n");
		break;
	default:
		printf("unknown\n");
		break;
	}
	if (check_slot && (reg & PCIE_XCAP_SI) != 0)
		printf("      Slot implemented\n");
	printf("      Interrupt Message Number: %x\n",
	    (unsigned int)((reg & PCIE_XCAP_IRQ) >> 27));

	/* Device Capability Register */
	reg = regs[o2i(capoff + PCIE_DCAP)];
	printf("    Device Capabilities Register: 0x%08x\n", reg);
	printf("      Max Payload Size Supported: %u bytes max\n",
	    (unsigned int)(reg & PCIE_DCAP_MAX_PAYLOAD) * 256);
	printf("      Phantom Functions Supported: ");
	switch ((reg & PCIE_DCAP_PHANTOM_FUNCS) >> 3) {
	case 0x0:
		printf("not available\n");
		break;
	case 0x1:
		printf("MSB\n");
		break;
	case 0x2:
		printf("two MSB\n");
		break;
	case 0x3:
		printf("All three bits\n");
		break;
	}
	printf("      Extended Tag Field Supported: %dbit\n",
	    (reg & PCIE_DCAP_EXT_TAG_FIELD) == 0 ? 5 : 8);
	printf("      Endpoint L0 Acceptable Latency: ");
	pci_print_pcie_L0s_latency((reg & PCIE_DCAP_L0S_LATENCY) >> 6);
	printf("      Endpoint L1 Acceptable Latency: ");
	pci_print_pcie_L1_latency((reg & PCIE_DCAP_L1_LATENCY) >> 9);
	printf("      Attention Button Present: %s\n",
	    (reg & PCIE_DCAP_ATTN_BUTTON) != 0 ? "yes" : "no");
	printf("      Attention Indicator Present: %s\n",
	    (reg & PCIE_DCAP_ATTN_IND) != 0 ? "yes" : "no");
	printf("      Power Indicator Present: %s\n",
	    (reg & PCIE_DCAP_PWR_IND) != 0 ? "yes" : "no");
	printf("      Role-Based Error Report: %s\n",
	    (reg & PCIE_DCAP_ROLE_ERR_RPT) != 0 ? "yes" : "no");
	printf("      Captured Slot Power Limit Value: %d\n",
	    (unsigned int)(reg & PCIE_DCAP_SLOT_PWR_LIM_VAL) >> 18);
	printf("      Captured Slot Power Limit Scale: %d\n",
	    (unsigned int)(reg & PCIE_DCAP_SLOT_PWR_LIM_SCALE) >> 26);
	printf("      Function-Level Reset Capability: %s\n",
	    (reg & PCIE_DCAP_FLR) != 0 ? "yes" : "no");

	/* Device Control Register */
	reg = regs[o2i(capoff + PCIE_DCSR)];
	printf("    Device Control Register: 0x%04x\n", reg & 0xffff);
	printf("      Correctable Error Reporting Enable: %s\n",
	    (reg & PCIE_DCSR_ENA_COR_ERR) != 0 ? "on" : "off");
	printf("      Non Fatal Error Reporting Enable: %s\n",
	    (reg & PCIE_DCSR_ENA_NFER) != 0 ? "on" : "off");
	printf("      Fatal Error Reporting Enable: %s\n",
	    (reg & PCIE_DCSR_ENA_FER) != 0 ? "on" : "off");
	printf("      Unsupported Request Reporting Enable: %s\n",
	    (reg & PCIE_DCSR_ENA_URR) != 0 ? "on" : "off");
	printf("      Enable Relaxed Ordering: %s\n",
	    (reg & PCIE_DCSR_ENA_RELAX_ORD) != 0 ? "on" : "off");
	printf("      Max Payload Size: %d byte\n",
	    128 << (((unsigned int)(reg & PCIE_DCSR_MAX_PAYLOAD) >> 5)));
	printf("      Extended Tag Field Enable: %s\n",
	    (reg & PCIE_DCSR_EXT_TAG_FIELD) != 0 ? "on" : "off");
	printf("      Phantom Functions Enable: %s\n",
	    (reg & PCIE_DCSR_PHANTOM_FUNCS) != 0 ? "on" : "off");
	printf("      Aux Power PM Enable: %s\n",
	    (reg & PCIE_DCSR_AUX_POWER_PM) != 0 ? "on" : "off");
	printf("      Enable No Snoop: %s\n",
	    (reg & PCIE_DCSR_ENA_NO_SNOOP) != 0 ? "on" : "off");
	printf("      Max Read Request Size: %d byte\n",
	    128 << ((unsigned int)(reg & PCIE_DCSR_MAX_READ_REQ) >> 12));

	/* Device Status Register */
	reg = regs[o2i(capoff + PCIE_DCSR)];
	printf("    Device Status Register: 0x%04x\n", reg >> 16);
	printf("      Correctable Error Detected: %s\n",
	    (reg & PCIE_DCSR_CED) != 0 ? "on" : "off");
	printf("      Non Fatal Error Detected: %s\n",
	    (reg & PCIE_DCSR_NFED) != 0 ? "on" : "off");
	printf("      Fatal Error Detected: %s\n",
	    (reg & PCIE_DCSR_FED) != 0 ? "on" : "off");
	printf("      Unsupported Request Detected: %s\n",
	    (reg & PCIE_DCSR_URD) != 0 ? "on" : "off");
	printf("      Aux Power Detected: %s\n",
	    (reg & PCIE_DCSR_AUX_PWR) != 0 ? "on" : "off");
	printf("      Transaction Pending: %s\n",
	    (reg & PCIE_DCSR_TRANSACTION_PND) != 0 ? "on" : "off");

	if (check_link) {
		/* Link Capability Register */
		reg = regs[o2i(capoff + PCIE_LCAP)];
		printf("    Link Capabilities Register: 0x%08x\n", reg);
		printf("      Maximum Link Speed: ");
		val = reg & PCIE_LCAP_MAX_SPEED;
		if (val < 1 || val > 3) {
			printf("unknown %u value\n", val);
		} else {
			printf("%sGT/s\n", linkspeeds[val - 1]);
		}
		printf("      Maximum Link Width: x%u lanes\n",
		    (unsigned int)(reg & PCIE_LCAP_MAX_WIDTH) >> 4);
		printf("      Active State PM Support: ");
		val = (reg & PCIE_LCAP_ASPM) >> 10;
		switch (val) {
		case 0x1:
			printf("L0s Entry supported\n");
			break;
		case 0x3:
			printf("L0s and L1 supported\n");
			break;
		default:
			printf("Reserved value\n");
			break;
		}
		printf("      L0 Exit Latency: ");
		pci_print_pcie_L0s_latency((reg & PCIE_LCAP_L0S_EXIT) >> 12);
		printf("      L1 Exit Latency: ");
		pci_print_pcie_L1_latency((reg & PCIE_LCAP_L1_EXIT) >> 15);
		printf("      Port Number: %u\n", reg >> 24);

		/* Link Control Register */
		reg = regs[o2i(capoff + PCIE_LCSR)];
		printf("    Link Control Register: 0x%04x\n", reg & 0xffff);
		printf("      Active State PM Control: ");
		val = reg & (PCIE_LCSR_ASPM_L1 | PCIE_LCSR_ASPM_L0S);
		switch (val) {
		case 0:
			printf("disabled\n");
			break;
		case 1:
			printf("L0s Entry Enabled\n");
			break;
		case 2:
			printf("L1 Entry Enabled\n");
			break;
		case 3:
			printf("L0s and L1 Entry Enabled\n");
			break;
		}
		printf("      Read Completion Boundary Control: %dbyte\n",
		    (reg & PCIE_LCSR_RCB) != 0 ? 128 : 64);
		printf("      Link Disable: %s\n",
		    (reg & PCIE_LCSR_LINK_DIS) != 0 ? "on" : "off");
		printf("      Retrain Link: %s\n",
		    (reg & PCIE_LCSR_RETRAIN) != 0 ? "on" : "off");
		printf("      Common Clock Configuration: %s\n",
		    (reg & PCIE_LCSR_COMCLKCFG) != 0 ? "on" : "off");
		printf("      Extended Synch: %s\n",
		    (reg & PCIE_LCSR_EXTNDSYNC) != 0 ? "on" : "off");
		printf("      Enable Clock Power Management: %s\n",
		    (reg & PCIE_LCSR_ENCLKPM) != 0 ? "on" : "off");
		printf("      Hardware Autonomous Width Disable: %s\n",
		    (reg & PCIE_LCSR_HAWD) != 0 ? "on" : "off");
		printf("      Link Bandwidth Management Interrupt Enable: %s\n",
		    (reg & PCIE_LCSR_LBMIE) != 0 ? "on" : "off");
		printf("      Link Autonomous Bandwidth Interrupt Enable: %s\n",
		    (reg & PCIE_LCSR_LABIE) != 0 ? "on" : "off");

		/* Link Status Register */
		reg = regs[o2i(capoff + PCIE_LCSR)];
		printf("    Link Status Register: 0x%04x\n", reg >> 16);
		printf("      Negotiated Link Speed: ");
		if (((reg >> 16) & 0x000f) < 1 ||
		    ((reg >> 16) & 0x000f) > 3) {
			printf("unknown %u value\n",
			    (unsigned int)(reg & PCIE_LCSR_LINKSPEED) >> 16);
		} else {
			printf("%sGT/s\n",
			    linkspeeds[((reg & PCIE_LCSR_LINKSPEED) >> 16) - 1]);
		}
		printf("      Negotiated Link Width: x%u lanes\n",
		    (reg >> 20) & 0x003f);
		printf("      Training Error: %s\n",
		    (reg & PCIE_LCSR_LINKTRAIN_ERR) != 0 ? "on" : "off");
		printf("      Link Training: %s\n",
		    (reg & PCIE_LCSR_LINKTRAIN) != 0 ? "on" : "off");
		printf("      Slot Clock Configuration: %s\n",
		    (reg & PCIE_LCSR_SLOTCLKCFG) != 0 ? "on" : "off");
		printf("      Data Link Layer Link Active: %s\n",
		    (reg & PCIE_LCSR_DLACTIVE) != 0 ? "on" : "off");
		printf("      Link Bandwidth Management Status: %s\n",
		    (reg & PCIE_LCSR_LINK_BW_MGMT) != 0 ? "on" : "off");
		printf("      Link Autonomous Bandwidth Status: %s\n",
		    (reg & PCIE_LCSR_LINK_AUTO_BW) != 0 ? "on" : "off");
	}

	if (check_slot == true) {
		/* Slot Capability Register */
		reg = regs[o2i(capoff + PCIE_SLCAP)];
		printf("    Slot Capability Register: %08x\n", reg);
		if ((reg & PCIE_SLCAP_ABP) != 0)
			printf("      Attention Button Present\n");
		if ((reg & PCIE_SLCAP_PCP) != 0)
			printf("      Power Controller Present\n");
		if ((reg & PCIE_SLCAP_MSP) != 0)
			printf("      MRL Sensor Present\n");
		if ((reg & PCIE_SLCAP_AIP) != 0)
			printf("      Attention Indicator Present\n");
		if ((reg & PCIE_SLCAP_PIP) != 0)
			printf("      Power Indicator Present\n");
		if ((reg & PCIE_SLCAP_HPS) != 0)
			printf("      Hot-Plug Surprise\n");
		if ((reg & PCIE_SLCAP_HPC) != 0)
			printf("      Hot-Plug Capable\n");
		printf("      Slot Power Limit Value: %d\n",
		    (unsigned int)(reg & PCIE_SLCAP_SPLV) >> 7);
		printf("      Slot Power Limit Scale: %d\n",
		    (unsigned int)(reg & PCIE_SLCAP_SPLS) >> 15);
		if ((reg & PCIE_SLCAP_EIP) != 0)
			printf("      Electromechanical Interlock Present\n");
		if ((reg & PCIE_SLCAP_NCCS) != 0)
			printf("      No Command Completed Support\n");
		printf("      Physical Slot Number: %d\n",
		    (unsigned int)(reg & PCIE_SLCAP_PSN) >> 19);

		/* Slot Control Register */
		reg = regs[o2i(capoff + PCIE_SLCSR)];
		printf("    Slot Control Register: %04x\n", reg & 0xffff);
		if ((reg & PCIE_SLCSR_ABE) != 0)
			printf("      Attention Button Pressed Enabled\n");
		if ((reg & PCIE_SLCSR_PFE) != 0)
			printf("      Power Fault Detected Enabled\n");
		if ((reg & PCIE_SLCSR_MSE) != 0)
			printf("      MRL Sensor Changed Enabled\n");
		if ((reg & PCIE_SLCSR_PDE) != 0)
			printf("      Presense Detect Changed Enabled\n");
		if ((reg & PCIE_SLCSR_CCE) != 0)
			printf("      Command Completed Interrupt Enabled\n");
		if ((reg & PCIE_SLCSR_HPE) != 0)
			printf("      Hot-Plug Interrupt Enabled\n");
		printf("      Attention Indicator Control: ");
		switch ((reg & PCIE_SLCSR_AIC) >> 6) {
		case 0x0:
			printf("reserved\n");
			break;
		case 0x1:
			printf("on\n");
			break;
		case 0x2:
			printf("blink\n");
			break;
		case 0x3:
			printf("off\n");
			break;
		}
		printf("      Power Indicator Control: ");
		switch ((reg & PCIE_SLCSR_PIC) >> 8) {
		case 0x0:
			printf("reserved\n");
			break;
		case 0x1:
			printf("on\n");
			break;
		case 0x2:
			printf("blink\n");
			break;
		case 0x3:
			printf("off\n");
			break;
		}
		printf("      Power Controller Control: ");
		if ((reg & PCIE_SLCSR_PCC) != 0)
			printf("off\n");
		else
			printf("on\n");
		if ((reg & PCIE_SLCSR_EIC) != 0)
			printf("      Electromechanical Interlock Control\n");
		if ((reg & PCIE_SLCSR_LACS) != 0)
			printf("      Data Link Layer State Changed Enable\n");

		/* Slot Status Register */
		printf("    Slot Status Register: %04x\n", reg >> 16);
		if ((reg & PCIE_SLCSR_ABP) != 0)
			printf("      Attention Button Pressed\n");
		if ((reg & PCIE_SLCSR_PFD) != 0)
			printf("      Power Fault Detected\n");
		if ((reg & PCIE_SLCSR_MSC) != 0)
			printf("      MRL Sensor Changed\n");
		if ((reg & PCIE_SLCSR_PDC) != 0)
			printf("      Presense Detect Changed\n");
		if ((reg & PCIE_SLCSR_CC) != 0)
			printf("      Command Completed\n");
		if ((reg & PCIE_SLCSR_MS) != 0)
			printf("      MRL Open\n");
		if ((reg & PCIE_SLCSR_PDS) != 0)
			printf("      Card Present in slot\n");
		if ((reg & PCIE_SLCSR_EIS) != 0)
			printf("      Electromechanical Interlock engaged\n");
		if ((reg & PCIE_SLCSR_LACS) != 0)
			printf("      Data Link Layer State Changed\n");
	}

	if (check_rootport == true) {
		/* Root Control Register */
		reg = regs[o2i(capoff + PCIE_RCR)];
		printf("    Root Control Register: %04x\n", reg & 0xffff);
		if ((reg & PCIE_RCR_SERR_CER) != 0)
			printf("      SERR on Correctable Error Enable\n");
		if ((reg & PCIE_RCR_SERR_NFER) != 0)
			printf("      SERR on Non-Fatal Error Enable\n");
		if ((reg & PCIE_RCR_SERR_FER) != 0)
			printf("      SERR on Fatal Error Enable\n");
		if ((reg & PCIE_RCR_PME_IE) != 0)
			printf("      PME Interrupt Enable\n");

		/* Root Capability Register */
		printf("    Root Capability Register: %04x\n",
		    reg >> 16);

		/* Root Status Register */
		reg = regs[o2i(capoff + PCIE_RSR)];
		printf("    Root Status Register: %08x\n", reg);
		printf("      PME Requester ID: %04x\n",
		    (unsigned int)(reg & PCIE_RSR_PME_REQESTER));
		if ((reg & PCIE_RSR_PME_STAT) != 0)
			printf("      PME was asserted\n");
		if ((reg & PCIE_RSR_PME_PEND) != 0)
			printf("      another PME is pending\n");
	}

	/* PCIe DW9 to DW14 is for PCIe 2.0 and newer */
	if (pciever < 2)
		return;

	/* Device Capabilities 2 */
	reg = regs[o2i(capoff + PCIE_DCAP2)];
	printf("    Device Capabilities 2: 0x%08x\n", reg);
	printf("      Completion Timeout Ranges Supported: %u \n",
	    (unsigned int)(reg & PCIE_DCAP2_COMPT_RANGE));
	printf("      Completion Timeout Disable Supported: %s\n",
	    (reg & PCIE_DCAP2_COMPT_DIS) != 0 ? "yes" : "no");
	printf("      ARI Forwarding Supported: %s\n",
	    (reg & PCIE_DCAP2_ARI_FWD) != 0 ? "yes" : "no");
	printf("      AtomicOp Routing Supported: %s\n",
	    (reg & PCIE_DCAP2_ATOM_ROUT) != 0 ? "yes" : "no");
	printf("      32bit AtomicOp Completer Supported: %s\n",
	    (reg & PCIE_DCAP2_32ATOM) != 0 ? "yes" : "no");
	printf("      64bit AtomicOp Completer Supported: %s\n",
	    (reg & PCIE_DCAP2_64ATOM) != 0 ? "yes" : "no");
	printf("      128-bit CAS Completer Supported: %s\n",
	    (reg & PCIE_DCAP2_128CAS) != 0 ? "yes" : "no");
	printf("      No RO-enabled PR-PR passing: %s\n",
	    (reg & PCIE_DCAP2_NO_ROPR_PASS) != 0 ? "yes" : "no");
	printf("      LTR Mechanism Supported: %s\n",
	    (reg & PCIE_DCAP2_LTR_MEC) != 0 ? "yes" : "no");
	printf("      TPH Completer Supported: %u\n",
	    (unsigned int)(reg & PCIE_DCAP2_TPH_COMP) >> 12);
	printf("      OBFF Supported: ");
	switch ((reg & PCIE_DCAP2_OBFF) >> 18) {
	case 0x0:
		printf("Not supported\n");
		break;
	case 0x1:
		printf("Message only\n");
		break;
	case 0x2:
		printf("WAKE# only\n");
		break;
	case 0x3:
		printf("Both\n");
		break;
	}
	printf("      Extended Fmt Field Supported: %s\n",
	    (reg & PCIE_DCAP2_EXTFMT_FLD) != 0 ? "yes" : "no");
	printf("      End-End TLP Prefix Supported: %s\n",
	    (reg & PCIE_DCAP2_EETLP_PREF) != 0 ? "yes" : "no");
	printf("      Max End-End TLP Prefixes: %u\n",
	    (unsigned int)(reg & PCIE_DCAP2_MAX_EETLP) >> 22);

	/* Device Control 2 */
	reg = regs[o2i(capoff + PCIE_DCSR2)];
	printf("    Device Control 2: 0x%04x\n", reg & 0xffff);
	printf("      Completion Timeout Value: ");
	pci_print_pcie_compl_timeout(reg & PCIE_DCSR2_COMPT_VAL);
	if ((reg & PCIE_DCSR2_COMPT_DIS) != 0)
		printf("      Completion Timeout Disabled\n");
	if ((reg & PCIE_DCSR2_ARI_FWD) != 0)
		printf("      ARI Forwarding Enabled\n");
	if ((reg & PCIE_DCSR2_ATOM_REQ) != 0)
		printf("      AtomicOp Rquester Enabled\n");
	if ((reg & PCIE_DCSR2_ATOM_EBLK) != 0)
		printf("      AtomicOp Egress Blocking on\n");
	if ((reg & PCIE_DCSR2_IDO_REQ) != 0)
		printf("      IDO Request Enabled\n");
	if ((reg & PCIE_DCSR2_IDO_COMP) != 0)
		printf("      IDO Completion Enabled\n");
	if ((reg & PCIE_DCSR2_LTR_MEC) != 0)
		printf("      LTR Mechanism Enabled\n");
	printf("      OBFF: ");
	switch ((reg & PCIE_DCSR2_OBFF_EN) >> 13) {
	case 0x0:
		printf("Disabled\n");
		break;
	case 0x1:
		printf("Enabled with Message Signaling Variation A\n");
		break;
	case 0x2:
		printf("Enabled with Message Signaling Variation B\n");
		break;
	case 0x3:
		printf("Enabled using WAKE# signaling\n");
		break;
	}
	if ((reg & PCIE_DCSR2_EETLP) != 0)
		printf("      End-End TLP Prefix Blocking on\n");

	if (check_link) {
		/* Link Capability 2 */
		reg = regs[o2i(capoff + PCIE_LCAP2)];
		printf("    Link Capabilities 2: 0x%08x\n", reg);
		val = (reg & PCIE_LCAP2_SUP_LNKSV) >> 1;
		printf("      Supported Link Speed Vector:");
		for (i = 0; i <= 2; i++) {
			if (((val >> i) & 0x01) != 0)
				printf(" %sGT/s", linkspeeds[i]);
		}
		printf("\n");

		/* Link Control 2 */
		reg = regs[o2i(capoff + PCIE_LCSR2)];
		printf("    Link Control 2: 0x%04x\n", reg & 0xffff);
		printf("      Target Link Speed: ");
		val = reg & PCIE_LCSR2_TGT_LSPEED;
		if (val < 1 || val > 3) {
			printf("unknown %u value\n", val);
		} else {
			printf("%sGT/s\n", linkspeeds[val - 1]);
		}
		if ((reg & PCIE_LCSR2_ENT_COMPL) != 0)
			printf("      Enter Compliance Enabled\n");
		if ((reg & PCIE_LCSR2_HW_AS_DIS) != 0)
			printf("      HW Autonomous Speed Disabled\n");
		if ((reg & PCIE_LCSR2_SEL_DEEMP) != 0)
			printf("      Selectable De-emphasis\n");
		printf("      Transmit Margin: %u\n",
		    (unsigned int)(reg & PCIE_LCSR2_TX_MARGIN) >> 7);
		if ((reg & PCIE_LCSR2_EN_MCOMP) != 0)
			printf("      Enter Modified Compliance\n");
		if ((reg & PCIE_LCSR2_COMP_SOS) != 0)
			printf("      Compliance SOS\n");
		printf("      Compliance Present/De-emphasis: %u\n",
		    (unsigned int)(reg & PCIE_LCSR2_COMP_DEEMP) >> 12);

		/* Link Status 2 */
		if ((reg & PCIE_LCSR2_DEEMP_LVL) != 0)
			printf("      Current De-emphasis Level\n");
		if ((reg & PCIE_LCSR2_EQ_COMPL) != 0)
			printf("      Equalization Complete\n");
		if ((reg & PCIE_LCSR2_EQP1_SUC) != 0)
			printf("      Equalization Phase 1 Successful\n");
		if ((reg & PCIE_LCSR2_EQP2_SUC) != 0)
			printf("      Equalization Phase 2 Successful\n");
		if ((reg & PCIE_LCSR2_EQP3_SUC) != 0)
			printf("      Equalization Phase 3 Successful\n");
		if ((reg & PCIE_LCSR2_LNKEQ_REQ) != 0)
			printf("      Link Equalization Request\n");
	}

	/* Slot Capability 2 */
	/* Slot Control 2 */
	/* Slot Status 2 */
}

static const char *
pci_conf_print_pcipm_cap_aux(uint16_t caps)
{
	switch ((caps >> 6) & 7) {
	case 0:	return "self-powered";
	case 1: return "55 mA";
	case 2: return "100 mA";
	case 3: return "160 mA";
	case 4: return "220 mA";
	case 5: return "270 mA";
	case 6: return "320 mA";
	case 7:
	default: return "375 mA";
	}
}

static const char *
pci_conf_print_pcipm_cap_pmrev(uint8_t val)
{
	static const char unk[] = "unknown";
	static const char *pmrev[8] = {
		unk, "1.0", "1.1", "1.2", unk, unk, unk, unk
	};
	if (val > 7)
		return unk;
	return pmrev[val];
}

static void
pci_conf_print_pcipm_cap(const pcireg_t *regs, int capoff)
{
	uint16_t caps, pmcsr;

	caps = regs[o2i(capoff)] >> 16;
	pmcsr = regs[o2i(capoff + 0x04)] & 0xffff;

	printf("\n  PCI Power Management Capabilities Register\n");

	printf("    Capabilities register: 0x%04x\n", caps);
	printf("      Version: %s\n",
	    pci_conf_print_pcipm_cap_pmrev(caps & 0x3));
	printf("      PME# clock: %s\n", caps & 0x4 ? "on" : "off");
	printf("      Device specific initialization: %s\n",
	    caps & 0x20 ? "on" : "off");
	printf("      3.3V auxiliary current: %s\n",
	    pci_conf_print_pcipm_cap_aux(caps));
	printf("      D1 power management state support: %s\n",
	    (caps >> 9) & 1 ? "on" : "off");
	printf("      D2 power management state support: %s\n",
	    (caps >> 10) & 1 ? "on" : "off");
	printf("      PME# support: 0x%02x\n", caps >> 11);

	printf("    Control/status register: 0x%04x\n", pmcsr);
	printf("      Power state: D%d\n", pmcsr & 3);
	printf("      PCI Express reserved: %s\n",
	    (pmcsr >> 2) & 1 ? "on" : "off");
	printf("      No soft reset: %s\n", (pmcsr >> 3) & 1 ? "on" : "off");
	printf("      PME# assertion %sabled\n",
	    (pmcsr >> 8) & 1 ? "en" : "dis");
	printf("      PME# status: %s\n", (pmcsr >> 15) ? "on" : "off");
}

static void
pci_conf_print_msi_cap(const pcireg_t *regs, int capoff)
{
	uint32_t ctl, mmc, mme;

	regs += o2i(capoff);
	ctl = *regs++;
	mmc = __SHIFTOUT(ctl, PCI_MSI_CTL_MMC_MASK);
	mme = __SHIFTOUT(ctl, PCI_MSI_CTL_MME_MASK);

	printf("\n  PCI Message Signaled Interrupt\n");

	printf("    Message Control register: 0x%04x\n", ctl >> 16);
	printf("      MSI Enabled: %s\n",
	    ctl & PCI_MSI_CTL_MSI_ENABLE ? "yes" : "no");
	printf("      Multiple Message Capable: %s (%d vector%s)\n",
	    mmc > 0 ? "yes" : "no", 1 << mmc, mmc > 0 ? "s" : "");
	printf("      Multiple Message Enabled: %s (%d vector%s)\n",
	    mme > 0 ? "on" : "off", 1 << mme, mme > 0 ? "s" : "");
	printf("      64 Bit Address Capable: %s\n",
	    ctl & PCI_MSI_CTL_64BIT_ADDR ? "yes" : "no");
	printf("      Per-Vector Masking Capable: %s\n",
	    ctl & PCI_MSI_CTL_PERVEC_MASK ? "yes" : "no");
	printf("    Message Address %sregister: 0x%08x\n",
	    ctl & PCI_MSI_CTL_64BIT_ADDR ? "(lower) " : "", *regs++);
	if (ctl & PCI_MSI_CTL_64BIT_ADDR) {
		printf("    Message Address %sregister: 0x%08x\n",
		    "(upper) ", *regs++);
	}
	printf("    Message Data register: 0x%08x\n", *regs++);
	if (ctl & PCI_MSI_CTL_PERVEC_MASK) {
		printf("    Vector Mask register: 0x%08x\n", *regs++);
		printf("    Vector Pending register: 0x%08x\n", *regs++);
	}
}
static void
pci_conf_print_caplist(
#ifdef _KERNEL
    pci_chipset_tag_t pc, pcitag_t tag,
#endif
    const pcireg_t *regs, int capoff)
{
	int off;
	pcireg_t rval;
	int pcie_off = -1, pcipm_off = -1, msi_off = -1;

	for (off = PCI_CAPLIST_PTR(regs[o2i(capoff)]);
	     off != 0;
	     off = PCI_CAPLIST_NEXT(regs[o2i(off)])) {
		rval = regs[o2i(off)];
		printf("  Capability register at 0x%02x\n", off);

		printf("    type: 0x%02x (", PCI_CAPLIST_CAP(rval));
		switch (PCI_CAPLIST_CAP(rval)) {
		case PCI_CAP_RESERVED0:
			printf("reserved");
			break;
		case PCI_CAP_PWRMGMT:
			printf("Power Management, rev. %s",
			    pci_conf_print_pcipm_cap_pmrev((rval >> 0) & 0x07));
			pcipm_off = off;
			break;
		case PCI_CAP_AGP:
			printf("AGP, rev. %d.%d",
				PCI_CAP_AGP_MAJOR(rval),
				PCI_CAP_AGP_MINOR(rval));
			break;
		case PCI_CAP_VPD:
			printf("VPD");
			break;
		case PCI_CAP_SLOTID:
			printf("SlotID");
			break;
		case PCI_CAP_MSI:
			printf("MSI");
			msi_off = off;
			break;
		case PCI_CAP_CPCI_HOTSWAP:
			printf("CompactPCI Hot-swapping");
			break;
		case PCI_CAP_PCIX:
			printf("PCI-X");
			break;
		case PCI_CAP_LDT:
			printf("LDT");
			break;
		case PCI_CAP_VENDSPEC:
			printf("Vendor-specific");
			break;
		case PCI_CAP_DEBUGPORT:
			printf("Debug Port");
			break;
		case PCI_CAP_CPCI_RSRCCTL:
			printf("CompactPCI Resource Control");
			break;
		case PCI_CAP_HOTPLUG:
			printf("Hot-Plug");
			break;
		case PCI_CAP_SUBVENDOR:
			printf("Sub Vendor ID");
			break;
		case PCI_CAP_AGP8:
			printf("AGP 8x");
			break;
		case PCI_CAP_SECURE:
			printf("Secure Device");
			break;
		case PCI_CAP_PCIEXPRESS:
			printf("PCI Express");
			pcie_off = off;
			break;
		case PCI_CAP_MSIX:
			printf("MSI-X");
			break;
		case PCI_CAP_SATA:
			printf("SATA");
			break;
		case PCI_CAP_PCIAF:
			printf("Advanced Features");
			break;
		default:
			printf("unknown");
		}
		printf(")\n");
	}
	if (msi_off != -1)
		pci_conf_print_msi_cap(regs, msi_off);
	if (pcipm_off != -1)
		pci_conf_print_pcipm_cap(regs, pcipm_off);
	if (pcie_off != -1)
		pci_conf_print_pcie_cap(regs, pcie_off);
}

/* Print the Secondary Status Register. */
static void
pci_conf_print_ssr(pcireg_t rval)
{
	pcireg_t devsel;

	printf("    Secondary status register: 0x%04x\n", rval); /* XXX bits */
	onoff("66 MHz capable", __BIT(5));
	onoff("User Definable Features (UDF) support", __BIT(6));
	onoff("Fast back-to-back capable", __BIT(7));
	onoff("Data parity error detected", __BIT(8));

	printf("      DEVSEL timing: ");
	devsel = __SHIFTOUT(rval, __BITS(10, 9));
	switch (devsel) {
	case 0:
		printf("fast");
		break;
	case 1:
		printf("medium");
		break;
	case 2:
		printf("slow");
		break;
	default:
		printf("unknown/reserved");	/* XXX */
		break;
	}
	printf(" (0x%x)\n", devsel);

	onoff("Signalled target abort", __BIT(11));
	onoff("Received target abort", __BIT(12));
	onoff("Received master abort", __BIT(13));
	onoff("Received system error", __BIT(14));
	onoff("Detected parity error", __BIT(15));
}

static void
pci_conf_print_type1(
#ifdef _KERNEL
    pci_chipset_tag_t pc, pcitag_t tag,
#endif
    const pcireg_t *regs
#ifdef _KERNEL
    , int sizebars
#endif
    )
{
	int off, width;
	pcireg_t rval;

	/*
	 * XXX these need to be printed in more detail, need to be
	 * XXX checked against specs/docs, etc.
	 *
	 * This layout was cribbed from the TI PCI2030 PCI-to-PCI
	 * Bridge chip documentation, and may not be correct with
	 * respect to various standards. (XXX)
	 */

	for (off = 0x10; off < 0x18; off += width) {
#ifdef _KERNEL
		width = pci_conf_print_bar(pc, tag, regs, off, NULL, sizebars);
#else
		width = pci_conf_print_bar(regs, off, NULL);
#endif
	}

	printf("    Primary bus number: 0x%02x\n",
	    (regs[o2i(0x18)] >> 0) & 0xff);
	printf("    Secondary bus number: 0x%02x\n",
	    (regs[o2i(0x18)] >> 8) & 0xff);
	printf("    Subordinate bus number: 0x%02x\n",
	    (regs[o2i(0x18)] >> 16) & 0xff);
	printf("    Secondary bus latency timer: 0x%02x\n",
	    (regs[o2i(0x18)] >> 24) & 0xff);

	pci_conf_print_ssr(__SHIFTOUT(regs[o2i(0x1c)], __BITS(31, 16)));

	/* XXX Print more prettily */
	printf("    I/O region:\n");
	printf("      base register:  0x%02x\n", (regs[o2i(0x1c)] >> 0) & 0xff);
	printf("      limit register: 0x%02x\n", (regs[o2i(0x1c)] >> 8) & 0xff);
	printf("      base upper 16 bits register:  0x%04x\n",
	    (regs[o2i(0x30)] >> 0) & 0xffff);
	printf("      limit upper 16 bits register: 0x%04x\n",
	    (regs[o2i(0x30)] >> 16) & 0xffff);

	/* XXX Print more prettily */
	printf("    Memory region:\n");
	printf("      base register:  0x%04x\n",
	    (regs[o2i(0x20)] >> 0) & 0xffff);
	printf("      limit register: 0x%04x\n",
	    (regs[o2i(0x20)] >> 16) & 0xffff);

	/* XXX Print more prettily */
	printf("    Prefetchable memory region:\n");
	printf("      base register:  0x%04x\n",
	    (regs[o2i(0x24)] >> 0) & 0xffff);
	printf("      limit register: 0x%04x\n",
	    (regs[o2i(0x24)] >> 16) & 0xffff);
	printf("      base upper 32 bits register:  0x%08x\n", regs[o2i(0x28)]);
	printf("      limit upper 32 bits register: 0x%08x\n", regs[o2i(0x2c)]);

	if (regs[o2i(PCI_COMMAND_STATUS_REG)] & PCI_STATUS_CAPLIST_SUPPORT)
		printf("    Capability list pointer: 0x%02x\n",
		    PCI_CAPLIST_PTR(regs[o2i(PCI_CAPLISTPTR_REG)]));
	else
		printf("    Reserved @ 0x34: 0x%08x\n", regs[o2i(0x34)]);

	/* XXX */
	printf("    Expansion ROM Base Address: 0x%08x\n", regs[o2i(0x38)]);

	printf("    Interrupt line: 0x%02x\n",
	    (regs[o2i(0x3c)] >> 0) & 0xff);
	printf("    Interrupt pin: 0x%02x ",
	    (regs[o2i(0x3c)] >> 8) & 0xff);
	switch ((regs[o2i(0x3c)] >> 8) & 0xff) {
	case PCI_INTERRUPT_PIN_NONE:
		printf("(none)");
		break;
	case PCI_INTERRUPT_PIN_A:
		printf("(pin A)");
		break;
	case PCI_INTERRUPT_PIN_B:
		printf("(pin B)");
		break;
	case PCI_INTERRUPT_PIN_C:
		printf("(pin C)");
		break;
	case PCI_INTERRUPT_PIN_D:
		printf("(pin D)");
		break;
	default:
		printf("(? ? ?)");
		break;
	}
	printf("\n");
	rval = (regs[o2i(0x3c)] >> 16) & 0xffff;
	printf("    Bridge control register: 0x%04x\n", rval); /* XXX bits */
	onoff("Parity error response", 0x0001);
	onoff("Secondary SERR forwarding", 0x0002);
	onoff("ISA enable", 0x0004);
	onoff("VGA enable", 0x0008);
	onoff("Master abort reporting", 0x0020);
	onoff("Secondary bus reset", 0x0040);
	onoff("Fast back-to-back capable", 0x0080);
}

static void
pci_conf_print_type2(
#ifdef _KERNEL
    pci_chipset_tag_t pc, pcitag_t tag,
#endif
    const pcireg_t *regs
#ifdef _KERNEL
    , int sizebars
#endif
    )
{
	pcireg_t rval;

	/*
	 * XXX these need to be printed in more detail, need to be
	 * XXX checked against specs/docs, etc.
	 *
	 * This layout was cribbed from the TI PCI1420 PCI-to-CardBus
	 * controller chip documentation, and may not be correct with
	 * respect to various standards. (XXX)
	 */

#ifdef _KERNEL
	pci_conf_print_bar(pc, tag, regs, 0x10,
	    "CardBus socket/ExCA registers", sizebars);
#else
	pci_conf_print_bar(regs, 0x10, "CardBus socket/ExCA registers");
#endif

	if (regs[o2i(PCI_COMMAND_STATUS_REG)] & PCI_STATUS_CAPLIST_SUPPORT)
		printf("    Capability list pointer: 0x%02x\n",
		    PCI_CAPLIST_PTR(regs[o2i(PCI_CARDBUS_CAPLISTPTR_REG)]));
	else
		printf("    Reserved @ 0x14: 0x%04" PRIxMAX "\n",
		       __SHIFTOUT(regs[o2i(0x14)], __BITS(15, 0)));
	pci_conf_print_ssr(__SHIFTOUT(regs[o2i(0x14)], __BITS(31, 16)));

	printf("    PCI bus number: 0x%02x\n",
	    (regs[o2i(0x18)] >> 0) & 0xff);
	printf("    CardBus bus number: 0x%02x\n",
	    (regs[o2i(0x18)] >> 8) & 0xff);
	printf("    Subordinate bus number: 0x%02x\n",
	    (regs[o2i(0x18)] >> 16) & 0xff);
	printf("    CardBus latency timer: 0x%02x\n",
	    (regs[o2i(0x18)] >> 24) & 0xff);

	/* XXX Print more prettily */
	printf("    CardBus memory region 0:\n");
	printf("      base register:  0x%08x\n", regs[o2i(0x1c)]);
	printf("      limit register: 0x%08x\n", regs[o2i(0x20)]);
	printf("    CardBus memory region 1:\n");
	printf("      base register:  0x%08x\n", regs[o2i(0x24)]);
	printf("      limit register: 0x%08x\n", regs[o2i(0x28)]);
	printf("    CardBus I/O region 0:\n");
	printf("      base register:  0x%08x\n", regs[o2i(0x2c)]);
	printf("      limit register: 0x%08x\n", regs[o2i(0x30)]);
	printf("    CardBus I/O region 1:\n");
	printf("      base register:  0x%08x\n", regs[o2i(0x34)]);
	printf("      limit register: 0x%08x\n", regs[o2i(0x38)]);

	printf("    Interrupt line: 0x%02x\n",
	    (regs[o2i(0x3c)] >> 0) & 0xff);
	printf("    Interrupt pin: 0x%02x ",
	    (regs[o2i(0x3c)] >> 8) & 0xff);
	switch ((regs[o2i(0x3c)] >> 8) & 0xff) {
	case PCI_INTERRUPT_PIN_NONE:
		printf("(none)");
		break;
	case PCI_INTERRUPT_PIN_A:
		printf("(pin A)");
		break;
	case PCI_INTERRUPT_PIN_B:
		printf("(pin B)");
		break;
	case PCI_INTERRUPT_PIN_C:
		printf("(pin C)");
		break;
	case PCI_INTERRUPT_PIN_D:
		printf("(pin D)");
		break;
	default:
		printf("(? ? ?)");
		break;
	}
	printf("\n");
	rval = (regs[o2i(0x3c)] >> 16) & 0xffff;
	printf("    Bridge control register: 0x%04x\n", rval);
	onoff("Parity error response", __BIT(0));
	onoff("SERR# enable", __BIT(1));
	onoff("ISA enable", __BIT(2));
	onoff("VGA enable", __BIT(3));
	onoff("Master abort mode", __BIT(5));
	onoff("Secondary (CardBus) bus reset", __BIT(6));
	onoff("Functional interrupts routed by ExCA registers", __BIT(7));
	onoff("Memory window 0 prefetchable", __BIT(8));
	onoff("Memory window 1 prefetchable", __BIT(9));
	onoff("Write posting enable", __BIT(10));

	rval = regs[o2i(0x40)];
	printf("    Subsystem vendor ID: 0x%04x\n", PCI_VENDOR(rval));
	printf("    Subsystem ID: 0x%04x\n", PCI_PRODUCT(rval));

#ifdef _KERNEL
	pci_conf_print_bar(pc, tag, regs, 0x44, "legacy-mode registers",
	    sizebars);
#else
	pci_conf_print_bar(regs, 0x44, "legacy-mode registers");
#endif
}

void
pci_conf_print(
#ifdef _KERNEL
    pci_chipset_tag_t pc, pcitag_t tag,
    void (*printfn)(pci_chipset_tag_t, pcitag_t, const pcireg_t *)
#else
    int pcifd, u_int bus, u_int dev, u_int func
#endif
    )
{
	pcireg_t regs[o2i(256)];
	int off, capoff, endoff, hdrtype;
	const char *typename;
#ifdef _KERNEL
	void (*typeprintfn)(pci_chipset_tag_t, pcitag_t, const pcireg_t *, int);
	int sizebars;
#else
	void (*typeprintfn)(const pcireg_t *);
#endif

	printf("PCI configuration registers:\n");

	for (off = 0; off < 256; off += 4) {
#ifdef _KERNEL
		regs[o2i(off)] = pci_conf_read(pc, tag, off);
#else
		if (pcibus_conf_read(pcifd, bus, dev, func, off,
		    &regs[o2i(off)]) == -1)
			regs[o2i(off)] = 0;
#endif
	}

#ifdef _KERNEL
	sizebars = 1;
	if (PCI_CLASS(regs[o2i(PCI_CLASS_REG)]) == PCI_CLASS_BRIDGE &&
	    PCI_SUBCLASS(regs[o2i(PCI_CLASS_REG)]) == PCI_SUBCLASS_BRIDGE_HOST)
		sizebars = 0;
#endif

	/* common header */
	printf("  Common header:\n");
	pci_conf_print_regs(regs, 0, 16);

	printf("\n");
#ifdef _KERNEL
	pci_conf_print_common(pc, tag, regs);
#else
	pci_conf_print_common(regs);
#endif
	printf("\n");

	/* type-dependent header */
	hdrtype = PCI_HDRTYPE_TYPE(regs[o2i(PCI_BHLC_REG)]);
	switch (hdrtype) {		/* XXX make a table, eventually */
	case 0:
		/* Standard device header */
		typename = "\"normal\" device";
		typeprintfn = &pci_conf_print_type0;
		capoff = PCI_CAPLISTPTR_REG;
		endoff = 64;
		break;
	case 1:
		/* PCI-PCI bridge header */
		typename = "PCI-PCI bridge";
		typeprintfn = &pci_conf_print_type1;
		capoff = PCI_CAPLISTPTR_REG;
		endoff = 64;
		break;
	case 2:
		/* PCI-CardBus bridge header */
		typename = "PCI-CardBus bridge";
		typeprintfn = &pci_conf_print_type2;
		capoff = PCI_CARDBUS_CAPLISTPTR_REG;
		endoff = 72;
		break;
	default:
		typename = NULL;
		typeprintfn = 0;
		capoff = -1;
		endoff = 64;
		break;
	}
	printf("  Type %d ", hdrtype);
	if (typename != NULL)
		printf("(%s) ", typename);
	printf("header:\n");
	pci_conf_print_regs(regs, 16, endoff);
	printf("\n");
	if (typeprintfn) {
#ifdef _KERNEL
		(*typeprintfn)(pc, tag, regs, sizebars);
#else
		(*typeprintfn)(regs);
#endif
	} else
		printf("    Don't know how to pretty-print type %d header.\n",
		    hdrtype);
	printf("\n");

	/* capability list, if present */
	if ((regs[o2i(PCI_COMMAND_STATUS_REG)] & PCI_STATUS_CAPLIST_SUPPORT)
		&& (capoff > 0)) {
#ifdef _KERNEL
		pci_conf_print_caplist(pc, tag, regs, capoff);
#else
		pci_conf_print_caplist(regs, capoff);
#endif
		printf("\n");
	}

	/* device-dependent header */
	printf("  Device-dependent header:\n");
	pci_conf_print_regs(regs, endoff, 256);
	printf("\n");
#ifdef _KERNEL
	if (printfn)
		(*printfn)(pc, tag, regs);
	else
		printf("    Don't know how to pretty-print device-dependent header.\n");
	printf("\n");
#endif /* _KERNEL */
}

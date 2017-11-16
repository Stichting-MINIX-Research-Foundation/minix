/*	$NetBSD: pci_subr.c,v 1.137 2015/10/03 15:22:14 joerg Exp $	*/

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

#if defined(__minix) && defined(_PCI_SERVER)
/* This is a quick hack, simple copy of the file, until we can use it as is. */
#include <sys/types.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <pci.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pci_verbose.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pcidevs_data.h>

const char *pci_baseclass_name(pcireg_t reg);
const char *pci_subclass_name(pcireg_t reg);
#else
/*
 * PCI autoconfiguration support functions.
 *
 * Note: This file is also built into a userland library (libpci).
 * Pay attention to this when you make modifications.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pci_subr.c,v 1.137 2015/10/03 15:22:14 joerg Exp $");

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
#include <stdlib.h>
#include <string.h>
#endif

#include <dev/pci/pcireg.h>
#ifdef _KERNEL
#include <dev/pci/pcivar.h>
#else
#include <dev/pci/pci_verbose.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pcidevs_data.h>
#endif
#endif /* defined(__minix) && defined(_PCI_SERVER) */

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

/*
 * Class 0x00.
 * Before rev. 2.0.
 */
static const struct pci_class pci_subclass_prehistoric[] = {
	{ "miscellaneous",	PCI_SUBCLASS_PREHISTORIC_MISC,	NULL,	},
	{ "VGA",		PCI_SUBCLASS_PREHISTORIC_VGA,	NULL,	},
	{ NULL,			0,				NULL,	},
};

/*
 * Class 0x01.
 * Mass storage controller
 */

/* ATA programming interface */
static const struct pci_class pci_interface_ata[] = {
	{ "with single DMA",	PCI_INTERFACE_ATA_SINGLEDMA,	NULL,	},
	{ "with chained DMA",	PCI_INTERFACE_ATA_CHAINEDDMA,	NULL,	},
	{ NULL,			0,				NULL,	},
};

/* SATA programming interface */
static const struct pci_class pci_interface_sata[] = {
	{ "vendor specific",	PCI_INTERFACE_SATA_VND,		NULL,	},
	{ "AHCI 1.0",		PCI_INTERFACE_SATA_AHCI10,	NULL,	},
	{ "Serial Storage Bus Interface", PCI_INTERFACE_SATA_SSBI, NULL, },
	{ NULL,			0,				NULL,	},
};

/* Flash programming interface */
static const struct pci_class pci_interface_nvm[] = {
	{ "vendor specific",	PCI_INTERFACE_NVM_VND,		NULL,	},
	{ "NVMHCI 1.0",		PCI_INTERFACE_NVM_NVMHCI10,	NULL,	},
	{ "NVMe",		PCI_INTERFACE_NVM_NVME,		NULL,	},
	{ NULL,			0,				NULL,	},
};

/* Subclasses */
static const struct pci_class pci_subclass_mass_storage[] = {
	{ "SCSI",		PCI_SUBCLASS_MASS_STORAGE_SCSI,	NULL,	},
	{ "IDE",		PCI_SUBCLASS_MASS_STORAGE_IDE,	NULL,	},
	{ "floppy",		PCI_SUBCLASS_MASS_STORAGE_FLOPPY, NULL, },
	{ "IPI",		PCI_SUBCLASS_MASS_STORAGE_IPI,	NULL,	},
	{ "RAID",		PCI_SUBCLASS_MASS_STORAGE_RAID,	NULL,	},
	{ "ATA",		PCI_SUBCLASS_MASS_STORAGE_ATA,
	  pci_interface_ata, },
	{ "SATA",		PCI_SUBCLASS_MASS_STORAGE_SATA,
	  pci_interface_sata, },
	{ "SAS",		PCI_SUBCLASS_MASS_STORAGE_SAS,	NULL,	},
	{ "Flash",		PCI_SUBCLASS_MASS_STORAGE_NVM,
	  pci_interface_nvm,	},
	{ "miscellaneous",	PCI_SUBCLASS_MASS_STORAGE_MISC,	NULL,	},
	{ NULL,			0,				NULL,	},
};

/*
 * Class 0x02.
 * Network controller.
 */
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

/*
 * Class 0x03.
 * Display controller.
 */

/* VGA programming interface */
static const struct pci_class pci_interface_vga[] = {
	{ "",			PCI_INTERFACE_VGA_VGA,		NULL,	},
	{ "8514-compat",	PCI_INTERFACE_VGA_8514,		NULL,	},
	{ NULL,			0,				NULL,	},
};
/* Subclasses */
static const struct pci_class pci_subclass_display[] = {
	{ "VGA",		PCI_SUBCLASS_DISPLAY_VGA,  pci_interface_vga,},
	{ "XGA",		PCI_SUBCLASS_DISPLAY_XGA,	NULL,	},
	{ "3D",			PCI_SUBCLASS_DISPLAY_3D,	NULL,	},
	{ "miscellaneous",	PCI_SUBCLASS_DISPLAY_MISC,	NULL,	},
	{ NULL,			0,				NULL,	},
};

/*
 * Class 0x04.
 * Multimedia device.
 */
static const struct pci_class pci_subclass_multimedia[] = {
	{ "video",		PCI_SUBCLASS_MULTIMEDIA_VIDEO,	NULL,	},
	{ "audio",		PCI_SUBCLASS_MULTIMEDIA_AUDIO,	NULL,	},
	{ "telephony",		PCI_SUBCLASS_MULTIMEDIA_TELEPHONY, NULL,},
	{ "mixed mode",		PCI_SUBCLASS_MULTIMEDIA_HDAUDIO, NULL, },
	{ "miscellaneous",	PCI_SUBCLASS_MULTIMEDIA_MISC,	NULL,	},
	{ NULL,			0,				NULL,	},
};

/*
 * Class 0x05.
 * Memory controller.
 */
static const struct pci_class pci_subclass_memory[] = {
	{ "RAM",		PCI_SUBCLASS_MEMORY_RAM,	NULL,	},
	{ "flash",		PCI_SUBCLASS_MEMORY_FLASH,	NULL,	},
	{ "miscellaneous",	PCI_SUBCLASS_MEMORY_MISC,	NULL,	},
	{ NULL,			0,				NULL,	},
};

/*
 * Class 0x06.
 * Bridge device.
 */

/* PCI bridge programming interface */
static const struct pci_class pci_interface_pcibridge[] = {
	{ "",			PCI_INTERFACE_BRIDGE_PCI_PCI, NULL,	},
	{ "subtractive decode",	PCI_INTERFACE_BRIDGE_PCI_SUBDEC, NULL,	},
	{ NULL,			0,				NULL,	},
};

/* Semi-transparent PCI-to-PCI bridge programming interface */
static const struct pci_class pci_interface_stpci[] = {
	{ "primary side facing host",	PCI_INTERFACE_STPCI_PRIMARY, NULL, },
	{ "secondary side facing host",	PCI_INTERFACE_STPCI_SECONDARY, NULL, },
	{ NULL,			0,				NULL,	},
};

/* Advanced Switching programming interface */
static const struct pci_class pci_interface_advsw[] = {
	{ "custom interface",	PCI_INTERFACE_ADVSW_CUSTOM, NULL, },
	{ "ASI-SIG",		PCI_INTERFACE_ADVSW_ASISIG, NULL, },
	{ NULL,			0,				NULL,	},
};

/* Subclasses */
static const struct pci_class pci_subclass_bridge[] = {
	{ "host",		PCI_SUBCLASS_BRIDGE_HOST,	NULL,	},
	{ "ISA",		PCI_SUBCLASS_BRIDGE_ISA,	NULL,	},
	{ "EISA",		PCI_SUBCLASS_BRIDGE_EISA,	NULL,	},
	{ "MicroChannel",	PCI_SUBCLASS_BRIDGE_MC,		NULL,	},
	{ "PCI",		PCI_SUBCLASS_BRIDGE_PCI,
	  pci_interface_pcibridge,	},
	{ "PCMCIA",		PCI_SUBCLASS_BRIDGE_PCMCIA,	NULL,	},
	{ "NuBus",		PCI_SUBCLASS_BRIDGE_NUBUS,	NULL,	},
	{ "CardBus",		PCI_SUBCLASS_BRIDGE_CARDBUS,	NULL,	},
	{ "RACEway",		PCI_SUBCLASS_BRIDGE_RACEWAY,	NULL,	},
	{ "Semi-transparent PCI", PCI_SUBCLASS_BRIDGE_STPCI,
	  pci_interface_stpci,	},
	{ "InfiniBand",		PCI_SUBCLASS_BRIDGE_INFINIBAND,	NULL,	},
	{ "advanced switching",	PCI_SUBCLASS_BRIDGE_ADVSW,
	  pci_interface_advsw,	},
	{ "miscellaneous",	PCI_SUBCLASS_BRIDGE_MISC,	NULL,	},
	{ NULL,			0,				NULL,	},
};

/*
 * Class 0x07.
 * Simple communications controller.
 */

/* Serial controller programming interface */
static const struct pci_class pci_interface_serial[] = {
	{ "generic XT-compat",	PCI_INTERFACE_SERIAL_XT,	NULL,	},
	{ "16450-compat",	PCI_INTERFACE_SERIAL_16450,	NULL,	},
	{ "16550-compat",	PCI_INTERFACE_SERIAL_16550,	NULL,	},
	{ "16650-compat",	PCI_INTERFACE_SERIAL_16650,	NULL,	},
	{ "16750-compat",	PCI_INTERFACE_SERIAL_16750,	NULL,	},
	{ "16850-compat",	PCI_INTERFACE_SERIAL_16850,	NULL,	},
	{ "16950-compat",	PCI_INTERFACE_SERIAL_16950,	NULL,	},
	{ NULL,			0,				NULL,	},
};

/* Parallel controller programming interface */
static const struct pci_class pci_interface_parallel[] = {
	{ "",			PCI_INTERFACE_PARALLEL,			NULL,},
	{ "bi-directional",	PCI_INTERFACE_PARALLEL_BIDIRECTIONAL,	NULL,},
	{ "ECP 1.X-compat",	PCI_INTERFACE_PARALLEL_ECP1X,		NULL,},
	{ "IEEE1284 controller", PCI_INTERFACE_PARALLEL_IEEE1284_CNTRL,	NULL,},
	{ "IEEE1284 target",	PCI_INTERFACE_PARALLEL_IEEE1284_TGT,	NULL,},
	{ NULL,			0,					NULL,},
};

/* Modem programming interface */
static const struct pci_class pci_interface_modem[] = {
	{ "",			PCI_INTERFACE_MODEM,			NULL,},
	{ "Hayes&16450-compat",	PCI_INTERFACE_MODEM_HAYES16450,		NULL,},
	{ "Hayes&16550-compat",	PCI_INTERFACE_MODEM_HAYES16550,		NULL,},
	{ "Hayes&16650-compat",	PCI_INTERFACE_MODEM_HAYES16650,		NULL,},
	{ "Hayes&16750-compat",	PCI_INTERFACE_MODEM_HAYES16750,		NULL,},
	{ NULL,			0,					NULL,},
};

/* Subclasses */
static const struct pci_class pci_subclass_communications[] = {
	{ "serial",		PCI_SUBCLASS_COMMUNICATIONS_SERIAL,
	  pci_interface_serial, },
	{ "parallel",		PCI_SUBCLASS_COMMUNICATIONS_PARALLEL,
	  pci_interface_parallel, },
	{ "multi-port serial",	PCI_SUBCLASS_COMMUNICATIONS_MPSERIAL,	NULL,},
	{ "modem",		PCI_SUBCLASS_COMMUNICATIONS_MODEM,
	  pci_interface_modem, },
	{ "GPIB",		PCI_SUBCLASS_COMMUNICATIONS_GPIB,	NULL,},
	{ "smartcard",		PCI_SUBCLASS_COMMUNICATIONS_SMARTCARD,	NULL,},
	{ "miscellaneous",	PCI_SUBCLASS_COMMUNICATIONS_MISC,	NULL,},
	{ NULL,			0,					NULL,},
};

/*
 * Class 0x08.
 * Base system peripheral.
 */ 

/* PIC programming interface */
static const struct pci_class pci_interface_pic[] = {
	{ "generic 8259",	PCI_INTERFACE_PIC_8259,		NULL,	},
	{ "ISA PIC",		PCI_INTERFACE_PIC_ISA,		NULL,	},
	{ "EISA PIC",		PCI_INTERFACE_PIC_EISA,		NULL,	},
	{ "IO APIC",		PCI_INTERFACE_PIC_IOAPIC,	NULL,	},
	{ "IO(x) APIC",		PCI_INTERFACE_PIC_IOXAPIC,	NULL,	},
	{ NULL,			0,				NULL,	},
};

/* DMA programming interface */
static const struct pci_class pci_interface_dma[] = {
	{ "generic 8237",	PCI_INTERFACE_DMA_8237,		NULL,	},
	{ "ISA",		PCI_INTERFACE_DMA_ISA,		NULL,	},
	{ "EISA",		PCI_INTERFACE_DMA_EISA,		NULL,	},
	{ NULL,			0,				NULL,	},
};

/* Timer programming interface */
static const struct pci_class pci_interface_tmr[] = {
	{ "generic 8254",	PCI_INTERFACE_TIMER_8254,	NULL,	},
	{ "ISA",		PCI_INTERFACE_TIMER_ISA,	NULL,	},
	{ "EISA",		PCI_INTERFACE_TIMER_EISA,	NULL,	},
	{ "HPET",		PCI_INTERFACE_TIMER_HPET,	NULL,	},
	{ NULL,			0,				NULL,	},
};

/* RTC programming interface */
static const struct pci_class pci_interface_rtc[] = {
	{ "generic",		PCI_INTERFACE_RTC_GENERIC,	NULL,	},
	{ "ISA",		PCI_INTERFACE_RTC_ISA,		NULL,	},
	{ NULL,			0,				NULL,	},
};

/* Subclasses */
static const struct pci_class pci_subclass_system[] = {
	{ "interrupt",		PCI_SUBCLASS_SYSTEM_PIC,   pci_interface_pic,},
	{ "DMA",		PCI_SUBCLASS_SYSTEM_DMA,   pci_interface_dma,},
	{ "timer",		PCI_SUBCLASS_SYSTEM_TIMER, pci_interface_tmr,},
	{ "RTC",		PCI_SUBCLASS_SYSTEM_RTC,   pci_interface_rtc,},
	{ "PCI Hot-Plug",	PCI_SUBCLASS_SYSTEM_PCIHOTPLUG, NULL,	},
	{ "SD Host Controller",	PCI_SUBCLASS_SYSTEM_SDHC,	NULL,	},
	{ "IOMMU",		PCI_SUBCLASS_SYSTEM_IOMMU,	NULL,	},
	{ "Root Complex Event Collector", PCI_SUBCLASS_SYSTEM_RCEC, NULL, },
	{ "miscellaneous",	PCI_SUBCLASS_SYSTEM_MISC,	NULL,	},
	{ NULL,			0,				NULL,	},
};

/*
 * Class 0x09.
 * Input device.
 */

/* Gameport programming interface */
static const struct pci_class pci_interface_game[] = {
	{ "generic",		PCI_INTERFACE_GAMEPORT_GENERIC,	NULL,	},
	{ "legacy",		PCI_INTERFACE_GAMEPORT_LEGACY,	NULL,	},
	{ NULL,			0,				NULL,	},
};

/* Subclasses */
static const struct pci_class pci_subclass_input[] = {
	{ "keyboard",		PCI_SUBCLASS_INPUT_KEYBOARD,	NULL,	},
	{ "digitizer",		PCI_SUBCLASS_INPUT_DIGITIZER,	NULL,	},
	{ "mouse",		PCI_SUBCLASS_INPUT_MOUSE,	NULL,	},
	{ "scanner",		PCI_SUBCLASS_INPUT_SCANNER,	NULL,	},
	{ "game port",		PCI_SUBCLASS_INPUT_GAMEPORT,
	  pci_interface_game, },
	{ "miscellaneous",	PCI_SUBCLASS_INPUT_MISC,	NULL,	},
	{ NULL,			0,				NULL,	},
};

/*
 * Class 0x0a.
 * Docking station.
 */
static const struct pci_class pci_subclass_dock[] = {
	{ "generic",		PCI_SUBCLASS_DOCK_GENERIC,	NULL,	},
	{ "miscellaneous",	PCI_SUBCLASS_DOCK_MISC,		NULL,	},
	{ NULL,			0,				NULL,	},
};

/*
 * Class 0x0b.
 * Processor.
 */
static const struct pci_class pci_subclass_processor[] = {
	{ "386",		PCI_SUBCLASS_PROCESSOR_386,	NULL,	},
	{ "486",		PCI_SUBCLASS_PROCESSOR_486,	NULL,	},
	{ "Pentium",		PCI_SUBCLASS_PROCESSOR_PENTIUM, NULL,	},
	{ "Alpha",		PCI_SUBCLASS_PROCESSOR_ALPHA,	NULL,	},
	{ "PowerPC",		PCI_SUBCLASS_PROCESSOR_POWERPC, NULL,	},
	{ "MIPS",		PCI_SUBCLASS_PROCESSOR_MIPS,	NULL,	},
	{ "Co-processor",	PCI_SUBCLASS_PROCESSOR_COPROC,	NULL,	},
	{ "miscellaneous",	PCI_SUBCLASS_PROCESSOR_MISC,	NULL,	},
	{ NULL,			0,				NULL,	},
};

/*
 * Class 0x0c.
 * Serial bus controller.
 */

/* IEEE1394 programming interface */
static const struct pci_class pci_interface_ieee1394[] = {
	{ "Firewire",		PCI_INTERFACE_IEEE1394_FIREWIRE,	NULL,},
	{ "OpenHCI",		PCI_INTERFACE_IEEE1394_OPENHCI,		NULL,},
	{ NULL,			0,					NULL,},
};

/* USB programming interface */
static const struct pci_class pci_interface_usb[] = {
	{ "UHCI",		PCI_INTERFACE_USB_UHCI,		NULL,	},
	{ "OHCI",		PCI_INTERFACE_USB_OHCI,		NULL,	},
	{ "EHCI",		PCI_INTERFACE_USB_EHCI,		NULL,	},
	{ "xHCI",		PCI_INTERFACE_USB_XHCI,		NULL,	},
	{ "other HC",		PCI_INTERFACE_USB_OTHERHC,	NULL,	},
	{ "device",		PCI_INTERFACE_USB_DEVICE,	NULL,	},
	{ NULL,			0,				NULL,	},
};

/* IPMI programming interface */
static const struct pci_class pci_interface_ipmi[] = {
	{ "SMIC",		PCI_INTERFACE_IPMI_SMIC,		NULL,},
	{ "keyboard",		PCI_INTERFACE_IPMI_KBD,			NULL,},
	{ "block transfer",	PCI_INTERFACE_IPMI_BLOCKXFER,		NULL,},
	{ NULL,			0,					NULL,},
};

/* Subclasses */
static const struct pci_class pci_subclass_serialbus[] = {
	{ "IEEE1394",		PCI_SUBCLASS_SERIALBUS_FIREWIRE,
	  pci_interface_ieee1394, },
	{ "ACCESS.bus",		PCI_SUBCLASS_SERIALBUS_ACCESS,	NULL,	},
	{ "SSA",		PCI_SUBCLASS_SERIALBUS_SSA,	NULL,	},
	{ "USB",		PCI_SUBCLASS_SERIALBUS_USB,
	  pci_interface_usb, },
	/* XXX Fiber Channel/_FIBRECHANNEL */
	{ "Fiber Channel",	PCI_SUBCLASS_SERIALBUS_FIBER,	NULL,	},
	{ "SMBus",		PCI_SUBCLASS_SERIALBUS_SMBUS,	NULL,	},
	{ "InfiniBand",		PCI_SUBCLASS_SERIALBUS_INFINIBAND, NULL,},
	{ "IPMI",		PCI_SUBCLASS_SERIALBUS_IPMI,
	  pci_interface_ipmi, },
	{ "SERCOS",		PCI_SUBCLASS_SERIALBUS_SERCOS,	NULL,	},
	{ "CANbus",		PCI_SUBCLASS_SERIALBUS_CANBUS,	NULL,	},
	{ "miscellaneous",	PCI_SUBCLASS_SERIALBUS_MISC,	NULL,	},
	{ NULL,			0,				NULL,	},
};

/*
 * Class 0x0d.
 * Wireless Controller.
 */
static const struct pci_class pci_subclass_wireless[] = {
	{ "IrDA",		PCI_SUBCLASS_WIRELESS_IRDA,	NULL,	},
	{ "Consumer IR",/*XXX*/	PCI_SUBCLASS_WIRELESS_CONSUMERIR, NULL,	},
	{ "RF",			PCI_SUBCLASS_WIRELESS_RF,	NULL,	},
	{ "bluetooth",		PCI_SUBCLASS_WIRELESS_BLUETOOTH, NULL,	},
	{ "broadband",		PCI_SUBCLASS_WIRELESS_BROADBAND, NULL,	},
	{ "802.11a (5 GHz)",	PCI_SUBCLASS_WIRELESS_802_11A,	NULL,	},
	{ "802.11b (2.4 GHz)",	PCI_SUBCLASS_WIRELESS_802_11B,	NULL,	},
	{ "miscellaneous",	PCI_SUBCLASS_WIRELESS_MISC,	NULL,	},
	{ NULL,			0,				NULL,	},
};

/*
 * Class 0x0e.
 * Intelligent IO controller.
 */

/* Intelligent IO programming interface */
static const struct pci_class pci_interface_i2o[] = {
	{ "FIFO at offset 0x40", PCI_INTERFACE_I2O_FIFOAT40,		NULL,},
	{ NULL,			0,					NULL,},
};

/* Subclasses */
static const struct pci_class pci_subclass_i2o[] = {
	{ "standard",		PCI_SUBCLASS_I2O_STANDARD, pci_interface_i2o,},
	{ "miscellaneous",	PCI_SUBCLASS_I2O_MISC,		NULL,	},
	{ NULL,			0,				NULL,	},
};

/*
 * Class 0x0f.
 * Satellite communication controller.
 */
static const struct pci_class pci_subclass_satcom[] = {
	{ "TV",			PCI_SUBCLASS_SATCOM_TV,	 	NULL,	},
	{ "audio",		PCI_SUBCLASS_SATCOM_AUDIO, 	NULL,	},
	{ "voice",		PCI_SUBCLASS_SATCOM_VOICE, 	NULL,	},
	{ "data",		PCI_SUBCLASS_SATCOM_DATA,	NULL,	},
	{ "miscellaneous",	PCI_SUBCLASS_SATCOM_MISC,	NULL,	},
	{ NULL,			0,				NULL,	},
};

/*
 * Class 0x10.
 * Encryption/Decryption controller.
 */
static const struct pci_class pci_subclass_crypto[] = {
	{ "network/computing",	PCI_SUBCLASS_CRYPTO_NETCOMP, 	NULL,	},
	{ "entertainment",	PCI_SUBCLASS_CRYPTO_ENTERTAINMENT, NULL,},
	{ "miscellaneous",	PCI_SUBCLASS_CRYPTO_MISC, 	NULL,	},
	{ NULL,			0,				NULL,	},
};

/*
 * Class 0x11.
 * Data aquuisition and signal processing controller.
 */
static const struct pci_class pci_subclass_dasp[] = {
	{ "DPIO",		PCI_SUBCLASS_DASP_DPIO,		NULL,	},
	{ "performance counters", PCI_SUBCLASS_DASP_TIMEFREQ,	NULL,	},
	{ "synchronization",	PCI_SUBCLASS_DASP_SYNC,		NULL,	},
	{ "management",		PCI_SUBCLASS_DASP_MGMT,		NULL,	},
	{ "miscellaneous",	PCI_SUBCLASS_DASP_MISC,		NULL,	},
	{ NULL,			0,				NULL,	},
};

/* List of classes */
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

#if defined(__minix) && defined(_PCI_SERVER)
const char *
pci_baseclass_name(pcireg_t reg)
{
	const struct pci_class *classp = pci_class;

	while (classp->name != NULL) {
		if (PCI_CLASS(reg) == classp->val)
			break;
		classp++;
	}

	return classp->name;
}

const char *
pci_subclass_name(pcireg_t reg)
{
	const struct pci_class *classp = pci_class;
	const struct pci_class *subclassp;

	while (classp->name != NULL) {
		if (PCI_CLASS(reg) == classp->val)
			break;
		classp++;
	}

	subclassp = (classp->name != NULL) ? classp->subclasses : NULL;
	while (subclassp && subclassp->name != NULL) {
		if (PCI_SUBCLASS(reg) == subclassp->val)
			break;
		subclassp++;
	}

	if (subclassp) {
		return subclassp->name;
	} else {
		return NULL;
	}
}
#endif /* defined(__minix) && defined(_PCI_SERVER) */

DEV_VERBOSE_DEFINE(pci);

#if defined(__minix) && !defined(_PCI_SERVER)

void
pci_devinfo(pcireg_t id_reg, pcireg_t class_reg, int showclass, char *cp,
    size_t l)
{
	pci_class_t pciclass;
	pci_subclass_t subclass;
	pci_interface_t interface;
	pci_revision_t revision;
	char vendor[PCI_VENDORSTR_LEN], product[PCI_PRODUCTSTR_LEN];
	const struct pci_class *classp, *subclassp, *interfacep;
	char *ep;

	ep = cp + l;

	pciclass = PCI_CLASS(class_reg);
	subclass = PCI_SUBCLASS(class_reg);
	interface = PCI_INTERFACE(class_reg);
	revision = PCI_REVISION(class_reg);

	pci_findvendor(vendor, sizeof(vendor), PCI_VENDOR(id_reg));
	pci_findproduct(product, sizeof(product), PCI_VENDOR(id_reg),
	    PCI_PRODUCT(id_reg));

	classp = pci_class;
	while (classp->name != NULL) {
		if (pciclass == classp->val)
			break;
		classp++;
	}

	subclassp = (classp->name != NULL) ? classp->subclasses : NULL;
	while (subclassp && subclassp->name != NULL) {
		if (subclass == subclassp->val)
			break;
		subclassp++;
	}

	interfacep = (subclassp && subclassp->name != NULL) ?
	    subclassp->subclasses : NULL;
	while (interfacep && interfacep->name != NULL) {
		if (interface == interfacep->val)
			break;
		interfacep++;
	}

	cp += snprintf(cp, ep - cp, "%s %s", vendor, product);
	if (showclass) {
		cp += snprintf(cp, ep - cp, " (");
		if (classp->name == NULL)
			cp += snprintf(cp, ep - cp,
			    "class 0x%02x, subclass 0x%02x", pciclass, subclass);
		else {
			if (subclassp == NULL || subclassp->name == NULL)
				cp += snprintf(cp, ep - cp,
				    "%s, subclass 0x%02x",
				    classp->name, subclass);
			else
				cp += snprintf(cp, ep - cp, "%s %s",
				    subclassp->name, classp->name);
		}
		if ((interfacep == NULL) || (interfacep->name == NULL)) {
			if (interface != 0)
				cp += snprintf(cp, ep - cp,
				    ", interface 0x%02x", interface);
		} else if (strncmp(interfacep->name, "", 1) != 0)
			cp += snprintf(cp, ep - cp, ", %s",
			    interfacep->name);
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
#define	onoff2(str, rval, bit, onstr, offstr)				      \
	printf("      %s: %s\n", (str), ((rval) & (bit)) ? onstr : offstr);
#define	onoff(str, rval, bit)	onoff2(str, rval, bit, "on", "off")

static void
pci_conf_print_common(
#ifdef _KERNEL
    pci_chipset_tag_t pc, pcitag_t tag,
#endif
    const pcireg_t *regs)
{
	const char *name;
	const struct pci_class *classp, *subclassp;
	char vendor[PCI_VENDORSTR_LEN];
	char product[PCI_PRODUCTSTR_LEN];
	pcireg_t rval;
	unsigned int num;

	rval = regs[o2i(PCI_ID_REG)];
	name = pci_findvendor(vendor, sizeof(vendor), PCI_VENDOR(rval));
	if (name)
		printf("    Vendor Name: %s (0x%04x)\n", name,
		    PCI_VENDOR(rval));
	else
		printf("    Vendor ID: 0x%04x\n", PCI_VENDOR(rval));
	name = pci_findproduct(product, sizeof(product), PCI_VENDOR(rval),
	    PCI_PRODUCT(rval));
	if (name)
		printf("    Device Name: %s (0x%04x)\n", name,
		    PCI_PRODUCT(rval));
	else
		printf("    Device ID: 0x%04x\n", PCI_PRODUCT(rval));

	rval = regs[o2i(PCI_COMMAND_STATUS_REG)];

	printf("    Command register: 0x%04x\n", rval & 0xffff);
	onoff("I/O space accesses", rval, PCI_COMMAND_IO_ENABLE);
	onoff("Memory space accesses", rval, PCI_COMMAND_MEM_ENABLE);
	onoff("Bus mastering", rval, PCI_COMMAND_MASTER_ENABLE);
	onoff("Special cycles", rval, PCI_COMMAND_SPECIAL_ENABLE);
	onoff("MWI transactions", rval, PCI_COMMAND_INVALIDATE_ENABLE);
	onoff("Palette snooping", rval, PCI_COMMAND_PALETTE_ENABLE);
	onoff("Parity error checking", rval, PCI_COMMAND_PARITY_ENABLE);
	onoff("Address/data stepping", rval, PCI_COMMAND_STEPPING_ENABLE);
	onoff("System error (SERR)", rval, PCI_COMMAND_SERR_ENABLE);
	onoff("Fast back-to-back transactions", rval,
	    PCI_COMMAND_BACKTOBACK_ENABLE);
	onoff("Interrupt disable", rval, PCI_COMMAND_INTERRUPT_DISABLE);

	printf("    Status register: 0x%04x\n", (rval >> 16) & 0xffff);
	onoff2("Interrupt status", rval, PCI_STATUS_INT_STATUS, "active",
	    "inactive");
	onoff("Capability List support", rval, PCI_STATUS_CAPLIST_SUPPORT);
	onoff("66 MHz capable", rval, PCI_STATUS_66MHZ_SUPPORT);
	onoff("User Definable Features (UDF) support", rval,
	    PCI_STATUS_UDF_SUPPORT);
	onoff("Fast back-to-back capable", rval,
	    PCI_STATUS_BACKTOBACK_SUPPORT);
	onoff("Data parity error detected", rval, PCI_STATUS_PARITY_ERROR);

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

	onoff("Slave signaled Target Abort", rval,
	    PCI_STATUS_TARGET_TARGET_ABORT);
	onoff("Master received Target Abort", rval,
	    PCI_STATUS_MASTER_TARGET_ABORT);
	onoff("Master received Master Abort", rval, PCI_STATUS_MASTER_ABORT);
	onoff("Asserted System Error (SERR)", rval, PCI_STATUS_SPECIAL_ERROR);
	onoff("Parity error detected", rval, PCI_STATUS_PARITY_DETECT);

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
			printf("    Subclass ID: 0x%02x\n",
			    PCI_SUBCLASS(rval));
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
	num = PCI_CACHELINE(rval);
	printf("    Cache Line Size: %ubytes (0x%02x)\n", num * 4, num);
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
pci_conf_print_agp_cap(const pcireg_t *regs, int capoff)
{
	pcireg_t rval;

	printf("\n  AGP Capabilities Register\n");

	rval = regs[o2i(capoff)];
	printf("    Revision: %d.%d\n",
	    PCI_CAP_AGP_MAJOR(rval), PCI_CAP_AGP_MINOR(rval));

	/* XXX need more */
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
	pcireg_t reg;

	caps = regs[o2i(capoff)] >> PCI_PMCR_SHIFT;
	reg = regs[o2i(capoff + PCI_PMCSR)];
	pmcsr = reg & 0xffff;

	printf("\n  PCI Power Management Capabilities Register\n");

	printf("    Capabilities register: 0x%04x\n", caps);
	printf("      Version: %s\n",
	    pci_conf_print_pcipm_cap_pmrev(caps & PCI_PMCR_VERSION_MASK));
	onoff("PME# clock", caps, PCI_PMCR_PME_CLOCK);
	onoff("Device specific initialization", caps, PCI_PMCR_DSI);
	printf("      3.3V auxiliary current: %s\n",
	    pci_conf_print_pcipm_cap_aux(caps));
	onoff("D1 power management state support", caps, PCI_PMCR_D1SUPP);
	onoff("D2 power management state support", caps, PCI_PMCR_D2SUPP);
	onoff("PME# support D0", caps, PCI_PMCR_PME_D0);
	onoff("PME# support D1", caps, PCI_PMCR_PME_D1);
	onoff("PME# support D2", caps, PCI_PMCR_PME_D2);
	onoff("PME# support D3 hot", caps, PCI_PMCR_PME_D3HOT);
	onoff("PME# support D3 cold", caps, PCI_PMCR_PME_D3COLD);

	printf("    Control/status register: 0x%04x\n", pmcsr);
	printf("      Power state: D%d\n", pmcsr & PCI_PMCSR_STATE_MASK);
	onoff("PCI Express reserved", (pmcsr >> 2), 1);
	onoff("No soft reset", pmcsr, PCI_PMCSR_NO_SOFTRST);
	printf("      PME# assertion: %sabled\n",
	    (pmcsr & PCI_PMCSR_PME_EN) ? "en" : "dis");
	onoff("PME# status", pmcsr, PCI_PMCSR_PME_STS);
	printf("    Bridge Support Extensions register: 0x%02x\n",
	    (reg >> 16) & 0xff);
	onoff("B2/B3 support", reg, PCI_PMCSR_B2B3_SUPPORT);
	onoff("Bus Power/Clock Control Enable", reg, PCI_PMCSR_BPCC_EN);
	printf("    Data register: 0x%02x\n", (reg >> 24) & 0xff);
	
}

/* XXX pci_conf_print_vpd_cap */
/* XXX pci_conf_print_slotid_cap */

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
	onoff("MSI Enabled", ctl, PCI_MSI_CTL_MSI_ENABLE);
	printf("      Multiple Message Capable: %s (%d vector%s)\n",
	    mmc > 0 ? "yes" : "no", 1 << mmc, mmc > 0 ? "s" : "");
	printf("      Multiple Message Enabled: %s (%d vector%s)\n",
	    mme > 0 ? "on" : "off", 1 << mme, mme > 0 ? "s" : "");
	onoff("64 Bit Address Capable", ctl, PCI_MSI_CTL_64BIT_ADDR);
	onoff("Per-Vector Masking Capable", ctl, PCI_MSI_CTL_PERVEC_MASK);
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

/* XXX pci_conf_print_cpci_hostwap_cap */

/*
 * For both command register and status register.
 * The argument "idx" is index number (0 to 7).
 */
static int
pcix_split_trans(unsigned int idx)
{
	static int table[8] = {
		1, 2, 3, 4, 8, 12, 16, 32
	};

	if (idx >= __arraycount(table))
		return -1;
	return table[idx];
}

static void
pci_conf_print_pcix_cap(const pcireg_t *regs, int capoff)
{
	pcireg_t reg;
	int isbridge;
	int i;

	isbridge = (PCI_HDRTYPE_TYPE(regs[o2i(PCI_BHLC_REG)])
	    & PCI_HDRTYPE_PPB) != 0 ? 1 : 0;
	printf("\n  PCI-X %s Capabilities Register\n",
	    isbridge ? "Bridge" : "Non-bridge");

	reg = regs[o2i(capoff)];
	if (isbridge != 0) {
		printf("    Secondary status register: 0x%04x\n",
		    (reg & 0xffff0000) >> 16);
		onoff("64bit device", reg, PCIX_STATUS_64BIT);
		onoff("133MHz capable", reg, PCIX_STATUS_133);
		onoff("Split completion discarded", reg, PCIX_STATUS_SPLDISC);
		onoff("Unexpected split completion", reg, PCIX_STATUS_SPLUNEX);
		onoff("Split completion overrun", reg, PCIX_BRIDGE_ST_SPLOVRN);
		onoff("Split request delayed", reg, PCIX_BRIDGE_ST_SPLRQDL);
		printf("      Secondary clock frequency: 0x%x\n",
		    (reg & PCIX_BRIDGE_2NDST_CLKF)
		    >> PCIX_BRIDGE_2NDST_CLKF_SHIFT);
		printf("      Version: 0x%x\n",
		    (reg & PCIX_BRIDGE_2NDST_VER_MASK)
		    >> PCIX_BRIDGE_2NDST_VER_SHIFT);
		onoff("266MHz capable", reg, PCIX_BRIDGE_ST_266);
		onoff("533MHz capable", reg, PCIX_BRIDGE_ST_533);
	} else {
		printf("    Command register: 0x%04x\n",
		    (reg & 0xffff0000) >> 16);
		onoff("Data Parity Error Recovery", reg,
		    PCIX_CMD_PERR_RECOVER);
		onoff("Enable Relaxed Ordering", reg, PCIX_CMD_RELAXED_ORDER);
		printf("      Maximum Burst Read Count: %u\n",
		    PCIX_CMD_BYTECNT(reg));
		printf("      Maximum Split Transactions: %d\n",
		    pcix_split_trans((reg & PCIX_CMD_SPLTRANS_MASK)
			>> PCIX_CMD_SPLTRANS_SHIFT));
	}
	reg = regs[o2i(capoff+PCIX_STATUS)]; /* Or PCIX_BRIDGE_PRI_STATUS */
	printf("    %sStatus register: 0x%08x\n",
	    isbridge ? "Bridge " : "", reg);
	printf("      Function: %d\n", PCIX_STATUS_FN(reg));
	printf("      Device: %d\n", PCIX_STATUS_DEV(reg));
	printf("      Bus: %d\n", PCIX_STATUS_BUS(reg));
	onoff("64bit device", reg, PCIX_STATUS_64BIT);
	onoff("133MHz capable", reg, PCIX_STATUS_133);
	onoff("Split completion discarded", reg, PCIX_STATUS_SPLDISC);
	onoff("Unexpected split completion", reg, PCIX_STATUS_SPLUNEX);
	if (isbridge != 0) {
		onoff("Split completion overrun", reg, PCIX_BRIDGE_ST_SPLOVRN);
		onoff("Split request delayed", reg, PCIX_BRIDGE_ST_SPLRQDL);
	} else {
		onoff2("Device Complexity", reg, PCIX_STATUS_DEVCPLX,
		    "bridge device", "simple device");
		printf("      Designed max memory read byte count: %d\n",
		    512 << ((reg & PCIX_STATUS_MAXB_MASK)
			>> PCIX_STATUS_MAXB_SHIFT));
		printf("      Designed max outstanding split transaction: %d\n",
		    pcix_split_trans((reg & PCIX_STATUS_MAXST_MASK)
			>> PCIX_STATUS_MAXST_SHIFT));
		printf("      MAX cumulative Read Size: %u\n",
		    8 << ((reg & 0x1c000000) >> PCIX_STATUS_MAXRS_SHIFT));
		onoff("Received split completion error", reg,
		    PCIX_STATUS_SCERR);
	}
	onoff("266MHz capable", reg, PCIX_STATUS_266);
	onoff("533MHz capable", reg, PCIX_STATUS_533);

	if (isbridge == 0)
		return;

	/* Only for bridge */
	for (i = 0; i < 2; i++) {
		reg = regs[o2i(capoff+PCIX_BRIDGE_UP_STCR + (4 * i))];
		printf("    %s split transaction control register: 0x%08x\n",
		    (i == 0) ? "Upstream" : "Downstream", reg);
		printf("      Capacity: %d\n", reg & PCIX_BRIDGE_STCAP);
		printf("      Commitment Limit: %d\n",
		    (reg & PCIX_BRIDGE_STCLIM) >> PCIX_BRIDGE_STCLIM_SHIFT);
	}
}

/* XXX pci_conf_print_ldt_cap */

static void
pci_conf_print_vendspec_cap(const pcireg_t *regs, int capoff)
{
	uint16_t caps;

	caps = regs[o2i(capoff)] >> PCI_VENDORSPECIFIC_SHIFT;

	printf("\n  PCI Vendor Specific Capabilities Register\n");
	printf("    Capabilities length: 0x%02x\n", caps & 0xff);
}

static void
pci_conf_print_debugport_cap(const pcireg_t *regs, int capoff)
{
	pcireg_t val;

	val = regs[o2i(capoff + PCI_DEBUG_BASER)];

	printf("\n  Debugport Capability Register\n");
	printf("    Debug base Register: 0x%04x\n",
	    val >> PCI_DEBUG_BASER_SHIFT);
	printf("      port offset: 0x%04x\n",
	    (val & PCI_DEBUG_PORTOFF_MASK) >> PCI_DEBUG_PORTOFF_SHIFT);
	printf("      BAR number: %u\n",
	    (val & PCI_DEBUG_BARNUM_MASK) >> PCI_DEBUG_BARNUM_SHIFT);
}

/* XXX pci_conf_print_cpci_rsrcctl_cap */
/* XXX pci_conf_print_hotplug_cap */

static void
pci_conf_print_subsystem_cap(const pcireg_t *regs, int capoff)
{
	pcireg_t reg;

	reg = regs[o2i(capoff + PCI_CAP_SUBSYS_ID)];

	printf("\n  Subsystem ID Capability Register\n");
	printf("    Subsystem ID : 0x%08x\n", reg);
}

/* XXX pci_conf_print_agp8_cap */
/* XXX pci_conf_print_secure_cap */

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
	onoff("Slot implemented", reg, PCIE_XCAP_SI);
	printf("      Interrupt Message Number: %x\n",
	    (unsigned int)((reg & PCIE_XCAP_IRQ) >> 27));

	/* Device Capability Register */
	reg = regs[o2i(capoff + PCIE_DCAP)];
	printf("    Device Capabilities Register: 0x%08x\n", reg);
	printf("      Max Payload Size Supported: %u bytes max\n",
	    128 << (unsigned int)(reg & PCIE_DCAP_MAX_PAYLOAD));
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
	onoff("Attention Button Present", reg, PCIE_DCAP_ATTN_BUTTON);
	onoff("Attention Indicator Present", reg, PCIE_DCAP_ATTN_IND);
	onoff("Power Indicator Present", reg, PCIE_DCAP_PWR_IND);
	onoff("Role-Based Error Report", reg, PCIE_DCAP_ROLE_ERR_RPT);
	printf("      Captured Slot Power Limit Value: %d\n",
	    (unsigned int)(reg & PCIE_DCAP_SLOT_PWR_LIM_VAL) >> 18);
	printf("      Captured Slot Power Limit Scale: %d\n",
	    (unsigned int)(reg & PCIE_DCAP_SLOT_PWR_LIM_SCALE) >> 26);
	onoff("Function-Level Reset Capability", reg, PCIE_DCAP_FLR);

	/* Device Control Register */
	reg = regs[o2i(capoff + PCIE_DCSR)];
	printf("    Device Control Register: 0x%04x\n", reg & 0xffff);
	onoff("Correctable Error Reporting Enable", reg,
	    PCIE_DCSR_ENA_COR_ERR);
	onoff("Non Fatal Error Reporting Enable", reg, PCIE_DCSR_ENA_NFER);
	onoff("Fatal Error Reporting Enable", reg, PCIE_DCSR_ENA_FER);
	onoff("Unsupported Request Reporting Enable", reg, PCIE_DCSR_ENA_URR);
	onoff("Enable Relaxed Ordering", reg, PCIE_DCSR_ENA_RELAX_ORD);
	printf("      Max Payload Size: %d byte\n",
	    128 << (((unsigned int)(reg & PCIE_DCSR_MAX_PAYLOAD) >> 5)));
	onoff("Extended Tag Field Enable", reg, PCIE_DCSR_EXT_TAG_FIELD);
	onoff("Phantom Functions Enable", reg, PCIE_DCSR_PHANTOM_FUNCS);
	onoff("Aux Power PM Enable", reg, PCIE_DCSR_AUX_POWER_PM);
	onoff("Enable No Snoop", reg, PCIE_DCSR_ENA_NO_SNOOP);
	printf("      Max Read Request Size: %d byte\n",
	    128 << ((unsigned int)(reg & PCIE_DCSR_MAX_READ_REQ) >> 12));

	/* Device Status Register */
	reg = regs[o2i(capoff + PCIE_DCSR)];
	printf("    Device Status Register: 0x%04x\n", reg >> 16);
	onoff("Correctable Error Detected", reg, PCIE_DCSR_CED);
	onoff("Non Fatal Error Detected", reg, PCIE_DCSR_NFED);
	onoff("Fatal Error Detected", reg, PCIE_DCSR_FED);
	onoff("Unsupported Request Detected", reg, PCIE_DCSR_URD);
	onoff("Aux Power Detected", reg, PCIE_DCSR_AUX_PWR);
	onoff("Transaction Pending", reg, PCIE_DCSR_TRANSACTION_PND);

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
		onoff("Clock Power Management", reg, PCIE_LCAP_CLOCK_PM);
		onoff("Surprise Down Error Report", reg,
		    PCIE_LCAP_SURPRISE_DOWN);
		onoff("Data Link Layer Link Active", reg, PCIE_LCAP_DL_ACTIVE);
		onoff("Link BW Notification Capable", reg,
			PCIE_LCAP_LINK_BW_NOTIFY);
		onoff("ASPM Optionally Compliance", reg,
		    PCIE_LCAP_ASPM_COMPLIANCE);

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
		onoff2("Read Completion Boundary Control", reg, PCIE_LCSR_RCB,
		    "128bytes", "64bytes");
		onoff("Link Disable", reg, PCIE_LCSR_LINK_DIS);
		onoff("Retrain Link", reg, PCIE_LCSR_RETRAIN);
		onoff("Common Clock Configuration", reg, PCIE_LCSR_COMCLKCFG);
		onoff("Extended Synch", reg, PCIE_LCSR_EXTNDSYNC);
		onoff("Enable Clock Power Management", reg, PCIE_LCSR_ENCLKPM);
		onoff("Hardware Autonomous Width Disable", reg,
		    PCIE_LCSR_HAWD);
		onoff("Link Bandwidth Management Interrupt Enable", reg,
		    PCIE_LCSR_LBMIE);
		onoff("Link Autonomous Bandwidth Interrupt Enable", reg,
		    PCIE_LCSR_LABIE);

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
			    linkspeeds[((reg & PCIE_LCSR_LINKSPEED) >> 16)-1]);
		}
		printf("      Negotiated Link Width: x%u lanes\n",
		    (reg >> 20) & 0x003f);
		onoff("Training Error", reg, PCIE_LCSR_LINKTRAIN_ERR);
		onoff("Link Training", reg, PCIE_LCSR_LINKTRAIN);
		onoff("Slot Clock Configuration", reg, PCIE_LCSR_SLOTCLKCFG);
		onoff("Data Link Layer Link Active", reg, PCIE_LCSR_DLACTIVE);
		onoff("Link Bandwidth Management Status", reg,
		    PCIE_LCSR_LINK_BW_MGMT);
		onoff("Link Autonomous Bandwidth Status", reg,
		    PCIE_LCSR_LINK_AUTO_BW);
	}

	if (check_slot == true) {
		/* Slot Capability Register */
		reg = regs[o2i(capoff + PCIE_SLCAP)];
		printf("    Slot Capability Register: %08x\n", reg);
		onoff("Attention Button Present", reg, PCIE_SLCAP_ABP);
		onoff("Power Controller Present", reg, PCIE_SLCAP_PCP);
		onoff("MRL Sensor Present", reg, PCIE_SLCAP_MSP);
		onoff("Attention Indicator Present", reg, PCIE_SLCAP_AIP);
		onoff("Power Indicator Present", reg, PCIE_SLCAP_PIP);
		onoff("Hot-Plug Surprise", reg, PCIE_SLCAP_HPS);
		onoff("Hot-Plug Capable", reg, PCIE_SLCAP_HPC);
		printf("      Slot Power Limit Value: %d\n",
		    (unsigned int)(reg & PCIE_SLCAP_SPLV) >> 7);
		printf("      Slot Power Limit Scale: %d\n",
		    (unsigned int)(reg & PCIE_SLCAP_SPLS) >> 15);
		onoff("Electromechanical Interlock Present", reg,
		    PCIE_SLCAP_EIP);
		onoff("No Command Completed Support", reg, PCIE_SLCAP_NCCS);
		printf("      Physical Slot Number: %d\n",
		    (unsigned int)(reg & PCIE_SLCAP_PSN) >> 19);

		/* Slot Control Register */
		reg = regs[o2i(capoff + PCIE_SLCSR)];
		printf("    Slot Control Register: %04x\n", reg & 0xffff);
		onoff("Attention Button Pressed Enabled", reg, PCIE_SLCSR_ABE);
		onoff("Power Fault Detected Enabled", reg, PCIE_SLCSR_PFE);
		onoff("MRL Sensor Changed Enabled", reg, PCIE_SLCSR_MSE);
		onoff("Presense Detect Changed Enabled", reg, PCIE_SLCSR_PDE);
		onoff("Command Completed Interrupt Enabled", reg,
		    PCIE_SLCSR_CCE);
		onoff("Hot-Plug Interrupt Enabled", reg, PCIE_SLCSR_HPE);
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
		onoff("Power Controller Control", reg, PCIE_SLCSR_PCC);
		onoff("Electromechanical Interlock Control",
		    reg, PCIE_SLCSR_EIC);
		onoff("Data Link Layer State Changed Enable", reg,
		    PCIE_SLCSR_DLLSCE);

		/* Slot Status Register */
		printf("    Slot Status Register: %04x\n", reg >> 16);
		onoff("Attention Button Pressed", reg, PCIE_SLCSR_ABP);
		onoff("Power Fault Detected", reg, PCIE_SLCSR_PFD);
		onoff("MRL Sensor Changed", reg, PCIE_SLCSR_MSC);
		onoff("Presense Detect Changed", reg, PCIE_SLCSR_PDC);
		onoff("Command Completed", reg, PCIE_SLCSR_CC);
		onoff("MRL Open", reg, PCIE_SLCSR_MS);
		onoff("Card Present in slot", reg, PCIE_SLCSR_PDS);
		onoff("Electromechanical Interlock engaged", reg,
		    PCIE_SLCSR_EIS);
		onoff("Data Link Layer State Changed", reg, PCIE_SLCSR_LACS);
	}

	if (check_rootport == true) {
		/* Root Control Register */
		reg = regs[o2i(capoff + PCIE_RCR)];
		printf("    Root Control Register: %04x\n", reg & 0xffff);
		onoff("SERR on Correctable Error Enable", reg,
		    PCIE_RCR_SERR_CER);
		onoff("SERR on Non-Fatal Error Enable", reg,
		    PCIE_RCR_SERR_NFER);
		onoff("SERR on Fatal Error Enable", reg, PCIE_RCR_SERR_FER);
		onoff("PME Interrupt Enable", reg, PCIE_RCR_PME_IE);
		onoff("CRS Software Visibility Enable", reg, PCIE_RCR_CRS_SVE);

		/* Root Capability Register */
		printf("    Root Capability Register: %04x\n",
		    reg >> 16);
		onoff("CRS Software Visibility", reg, PCIE_RCR_CRS_SV);

		/* Root Status Register */
		reg = regs[o2i(capoff + PCIE_RSR)];
		printf("    Root Status Register: %08x\n", reg);
		printf("      PME Requester ID: %04x\n",
		    (unsigned int)(reg & PCIE_RSR_PME_REQESTER));
		onoff("PME was asserted", reg, PCIE_RSR_PME_STAT);
		onoff("another PME is pending", reg, PCIE_RSR_PME_PEND);
	}

	/* PCIe DW9 to DW14 is for PCIe 2.0 and newer */
	if (pciever < 2)
		return;

	/* Device Capabilities 2 */
	reg = regs[o2i(capoff + PCIE_DCAP2)];
	printf("    Device Capabilities 2: 0x%08x\n", reg);
	printf("      Completion Timeout Ranges Supported: %u \n",
	    (unsigned int)(reg & PCIE_DCAP2_COMPT_RANGE));
	onoff("Completion Timeout Disable Supported", reg,
	    PCIE_DCAP2_COMPT_DIS);
	onoff("ARI Forwarding Supported", reg, PCIE_DCAP2_ARI_FWD);
	onoff("AtomicOp Routing Supported", reg, PCIE_DCAP2_ATOM_ROUT);
	onoff("32bit AtomicOp Completer Supported", reg, PCIE_DCAP2_32ATOM);
	onoff("64bit AtomicOp Completer Supported", reg, PCIE_DCAP2_64ATOM);
	onoff("128-bit CAS Completer Supported", reg, PCIE_DCAP2_128CAS);
	onoff("No RO-enabled PR-PR passing", reg, PCIE_DCAP2_NO_ROPR_PASS);
	onoff("LTR Mechanism Supported", reg, PCIE_DCAP2_LTR_MEC);
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
	onoff("Extended Fmt Field Supported", reg, PCIE_DCAP2_EXTFMT_FLD);
	onoff("End-End TLP Prefix Supported", reg, PCIE_DCAP2_EETLP_PREF);
	printf("      Max End-End TLP Prefixes: %u\n",
	    (unsigned int)(reg & PCIE_DCAP2_MAX_EETLP) >> 22);

	/* Device Control 2 */
	reg = regs[o2i(capoff + PCIE_DCSR2)];
	printf("    Device Control 2: 0x%04x\n", reg & 0xffff);
	printf("      Completion Timeout Value: ");
	pci_print_pcie_compl_timeout(reg & PCIE_DCSR2_COMPT_VAL);
	onoff("Completion Timeout Disabled", reg, PCIE_DCSR2_COMPT_DIS);
	onoff("ARI Forwarding Enabled", reg, PCIE_DCSR2_ARI_FWD);
	onoff("AtomicOp Rquester Enabled", reg, PCIE_DCSR2_ATOM_REQ);
	onoff("AtomicOp Egress Blocking", reg, PCIE_DCSR2_ATOM_EBLK);
	onoff("IDO Request Enabled", reg, PCIE_DCSR2_IDO_REQ);
	onoff("IDO Completion Enabled", reg, PCIE_DCSR2_IDO_COMP);
	onoff("LTR Mechanism Enabled", reg, PCIE_DCSR2_LTR_MEC);
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
	onoff("End-End TLP Prefix Blocking on", reg, PCIE_DCSR2_EETLP);

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
		onoff("Crosslink Supported", reg, PCIE_LCAP2_CROSSLNK);

		/* Link Control 2 */
		reg = regs[o2i(capoff + PCIE_LCSR2)];
		printf("    Link Control 2: 0x%04x\n", reg & 0xffff);
		printf("      Target Link Speed: ");
		val = reg & PCIE_LCSR2_TGT_LSPEED;
		if (val < 1 || val > 3)
			printf("unknown %u value\n", val);
		else
			printf("%sGT/s\n", linkspeeds[val - 1]);
		onoff("Enter Compliance Enabled", reg, PCIE_LCSR2_ENT_COMPL);
		onoff("HW Autonomous Speed Disabled", reg,
		    PCIE_LCSR2_HW_AS_DIS);
		onoff("Selectable De-emphasis", reg, PCIE_LCSR2_SEL_DEEMP);
		printf("      Transmit Margin: %u\n",
		    (unsigned int)(reg & PCIE_LCSR2_TX_MARGIN) >> 7);
		onoff("Enter Modified Compliance", reg, PCIE_LCSR2_EN_MCOMP);
		onoff("Compliance SOS", reg, PCIE_LCSR2_COMP_SOS);
		printf("      Compliance Present/De-emphasis: %u\n",
		    (unsigned int)(reg & PCIE_LCSR2_COMP_DEEMP) >> 12);

		/* Link Status 2 */
		printf("    Link Status 2: 0x%04x\n", (reg >> 16) & 0xffff);
		onoff("Current De-emphasis Level", reg, PCIE_LCSR2_DEEMP_LVL);
		onoff("Equalization Complete", reg, PCIE_LCSR2_EQ_COMPL);
		onoff("Equalization Phase 1 Successful", reg,
		    PCIE_LCSR2_EQP1_SUC);
		onoff("Equalization Phase 2 Successful", reg,
		    PCIE_LCSR2_EQP2_SUC);
		onoff("Equalization Phase 3 Successful", reg,
		    PCIE_LCSR2_EQP3_SUC);
		onoff("Link Equalization Request", reg, PCIE_LCSR2_LNKEQ_REQ);
	}

	/* Slot Capability 2 */
	/* Slot Control 2 */
	/* Slot Status 2 */
}

static void
pci_conf_print_msix_cap(const pcireg_t *regs, int capoff)
{
	pcireg_t reg;

	printf("\n  MSI-X Capability Register\n");

	reg = regs[o2i(capoff + PCI_MSIX_CTL)];
	printf("    Message Control register: 0x%04x\n",
	    (reg >> 16) & 0xff);
	printf("      Table Size: %d\n",PCI_MSIX_CTL_TBLSIZE(reg));
	onoff("Function Mask", reg, PCI_MSIX_CTL_FUNCMASK);
	onoff("MSI-X Enable", reg, PCI_MSIX_CTL_ENABLE);
	reg = regs[o2i(capoff + PCI_MSIX_TBLOFFSET)];
	printf("    Table offset register: 0x%08x\n", reg);
	printf("      Table offset: %08x\n", reg & PCI_MSIX_TBLOFFSET_MASK);
	printf("      BIR: 0x%x\n", reg & PCI_MSIX_TBLBIR_MASK);
	reg = regs[o2i(capoff + PCI_MSIX_PBAOFFSET)];
	printf("    Pending bit array register: 0x%08x\n", reg);
	printf("      Pending bit array offset: %08x\n",
	    reg & PCI_MSIX_PBAOFFSET_MASK);
	printf("      BIR: 0x%x\n", reg & PCI_MSIX_PBABIR_MASK);
}

/* XXX pci_conf_print_sata_cap */
static void
pci_conf_print_pciaf_cap(const pcireg_t *regs, int capoff)
{
	pcireg_t reg;

	printf("\n  Advanced Features Capability Register\n");

	reg = regs[o2i(capoff + PCI_AFCAPR)];
	printf("    AF Capabilities register: 0x%02x\n", (reg >> 24) & 0xff);
	onoff("Transaction Pending", reg, PCI_AF_TP_CAP);
	onoff("Function Level Reset", reg, PCI_AF_FLR_CAP);
	reg = regs[o2i(capoff + PCI_AFCSR)];
	printf("    AF Control register: 0x%02x\n", reg & 0xff);
	/*
	 * Only PCI_AFCR_INITIATE_FLR is a member of the AF control register
	 * and it's always 0 on read
	 */
	printf("    AF Status register: 0x%02x\n", (reg >> 8) & 0xff);
	onoff("Transaction Pending", reg, PCI_AFSR_TP);
}

static struct {
	pcireg_t cap;
	const char *name;
	void (*printfunc)(const pcireg_t *, int);
} pci_captab[] = {
	{ PCI_CAP_RESERVED0,	"reserved",	NULL },
	{ PCI_CAP_PWRMGMT,	"Power Management", pci_conf_print_pcipm_cap },
	{ PCI_CAP_AGP,		"AGP",		pci_conf_print_agp_cap },
	{ PCI_CAP_VPD,		"VPD",		NULL },
	{ PCI_CAP_SLOTID,	"SlotID",	NULL },
	{ PCI_CAP_MSI,		"MSI",		pci_conf_print_msi_cap }, 
	{ PCI_CAP_CPCI_HOTSWAP,	"CompactPCI Hot-swapping", NULL },
	{ PCI_CAP_PCIX,		"PCI-X",	pci_conf_print_pcix_cap },
	{ PCI_CAP_LDT,		"HyperTransport", NULL },
	{ PCI_CAP_VENDSPEC,	"Vendor-specific",
	  pci_conf_print_vendspec_cap },
	{ PCI_CAP_DEBUGPORT,	"Debug Port",	pci_conf_print_debugport_cap },
	{ PCI_CAP_CPCI_RSRCCTL, "CompactPCI Resource Control", NULL },
	{ PCI_CAP_HOTPLUG,	"Hot-Plug",	NULL },
	{ PCI_CAP_SUBVENDOR,	"Subsystem vendor ID",
	  pci_conf_print_subsystem_cap },
	{ PCI_CAP_AGP8,		"AGP 8x",	NULL },
	{ PCI_CAP_SECURE,	"Secure Device", NULL },
	{ PCI_CAP_PCIEXPRESS,	"PCI Express",	pci_conf_print_pcie_cap },
	{ PCI_CAP_MSIX,		"MSI-X",	pci_conf_print_msix_cap },
	{ PCI_CAP_SATA,		"SATA",		NULL },
	{ PCI_CAP_PCIAF,	"Advanced Features", pci_conf_print_pciaf_cap }
};

static int
pci_conf_find_cap(const pcireg_t *regs, int capoff, unsigned int capid,
    int *offsetp)
{
	pcireg_t rval;
	int off;

	for (off = PCI_CAPLIST_PTR(regs[o2i(capoff)]);
	     off != 0;
	     off = PCI_CAPLIST_NEXT(rval)) {
		rval = regs[o2i(off)];
		if (capid == PCI_CAPLIST_CAP(rval)) {
			if (offsetp != NULL)
				*offsetp = off;
			return 1;
		}
	}
	return 0;
}

static void
pci_conf_print_caplist(
#ifdef _KERNEL
    pci_chipset_tag_t pc, pcitag_t tag,
#endif
    const pcireg_t *regs, int capoff)
{
	int off;
	pcireg_t foundcap;
	pcireg_t rval;
	bool foundtable[__arraycount(pci_captab)];
	unsigned int i;

	/* Clear table */
	for (i = 0; i < __arraycount(pci_captab); i++)
		foundtable[i] = false;

	/* Print capability register's offset and the type first */
	for (off = PCI_CAPLIST_PTR(regs[o2i(capoff)]);
	     off != 0;
	     off = PCI_CAPLIST_NEXT(regs[o2i(off)])) {
		rval = regs[o2i(off)];
		printf("  Capability register at 0x%02x\n", off);

		printf("    type: 0x%02x (", PCI_CAPLIST_CAP(rval));
		foundcap = PCI_CAPLIST_CAP(rval);
		if (foundcap < __arraycount(pci_captab)) {
			printf("%s)\n", pci_captab[foundcap].name);
			/* Mark as found */
			foundtable[foundcap] = true;
		} else
			printf("unknown)\n");
	}

	/*
	 * And then, print the detail of each capability registers
	 * in capability value's order.
	 */
	for (i = 0; i < __arraycount(pci_captab); i++) {
		if (foundtable[i] == false)
			continue;

		/*
		 * The type was found. Search capability list again and
		 * print all capabilities that the capabiliy type is
		 * the same. This is required because some capabilities
		 * appear multiple times (e.g. HyperTransport capability).
		 */
		if (pci_conf_find_cap(regs, capoff, i, &off)) {
			rval = regs[o2i(off)];
			if (pci_captab[i].printfunc != NULL)
				pci_captab[i].printfunc(regs, off);
		}
	}
}

/* Extended Capability */

static void
pci_conf_print_aer_cap_uc(pcireg_t reg)
{

	onoff("Undefined", reg, PCI_AER_UC_UNDEFINED);
	onoff("Data Link Protocol Error", reg, PCI_AER_UC_DL_PROTOCOL_ERROR);
	onoff("Surprise Down Error", reg, PCI_AER_UC_SURPRISE_DOWN_ERROR);
	onoff("Poisoned TLP", reg, PCI_AER_UC_POISONED_TLP);
	onoff("Flow Control Protocol Error", reg, PCI_AER_UC_FC_PROTOCOL_ERROR);
	onoff("Completion Timeout", reg, PCI_AER_UC_COMPLETION_TIMEOUT);
	onoff("Completer Abort", reg, PCI_AER_UC_COMPLETER_ABORT);
	onoff("Unexpected Completion", reg, PCI_AER_UC_UNEXPECTED_COMPLETION);
	onoff("Receiver Overflow", reg, PCI_AER_UC_RECEIVER_OVERFLOW);
	onoff("Malformed TLP", reg, PCI_AER_UC_MALFORMED_TLP);
	onoff("ECRC Error", reg, PCI_AER_UC_ECRC_ERROR);
	onoff("Unsupported Request Error", reg,
	    PCI_AER_UC_UNSUPPORTED_REQUEST_ERROR);
	onoff("ACS Violation", reg, PCI_AER_UC_ACS_VIOLATION);
	onoff("Uncorrectable Internal Error", reg, PCI_AER_UC_INTERNAL_ERROR);
	onoff("MC Blocked TLP", reg, PCI_AER_UC_MC_BLOCKED_TLP);
	onoff("AtomicOp Egress BLK", reg, PCI_AER_UC_ATOMIC_OP_EGRESS_BLOCKED);
	onoff("TLP Prefix Blocked Error", reg,
	   PCI_AER_UC_TLP_PREFIX_BLOCKED_ERROR);
}

static void
pci_conf_print_aer_cap_cor(pcireg_t reg)
{

	onoff("Receiver Error", reg, PCI_AER_COR_RECEIVER_ERROR);
	onoff("Bad TLP", reg, PCI_AER_COR_BAD_TLP);
	onoff("Bad DLLP", reg, PCI_AER_COR_BAD_DLLP);
	onoff("REPLAY_NUM Rollover", reg, PCI_AER_COR_REPLAY_NUM_ROLLOVER);
	onoff("Replay Timer Timeout", reg, PCI_AER_COR_REPLAY_TIMER_TIMEOUT);
	onoff("Advisory Non-Fatal Error", reg, PCI_AER_COR_ADVISORY_NF_ERROR);
	onoff("Corrected Internal Error", reg, PCI_AER_COR_INTERNAL_ERROR);
	onoff("Header Log Overflow", reg, PCI_AER_COR_HEADER_LOG_OVERFLOW);
}

static void
pci_conf_print_aer_cap_control(pcireg_t reg, bool *tlp_prefix_log)
{

	printf("      First Error Pointer: 0x%04x\n",
	    (pcireg_t)__SHIFTOUT(reg, PCI_AER_FIRST_ERROR_PTR));
	onoff("ECRC Generation Capable", reg, PCI_AER_ECRC_GEN_CAPABLE);
	onoff("ECRC Generation Enable", reg, PCI_AER_ECRC_GEN_ENABLE);
	onoff("ECRC Check Capable", reg, PCI_AER_ECRC_CHECK_CAPABLE);
	onoff("ECRC Check Enab", reg, PCI_AER_ECRC_CHECK_ENABLE);
	onoff("Multiple Header Recording Capable", reg,
	    PCI_AER_MULT_HDR_CAPABLE);
	onoff("Multiple Header Recording Enable", reg, PCI_AER_MULT_HDR_ENABLE);

	/* This bit is RsvdP if the End-End TLP Prefix Supported bit is Clear */
	if (!tlp_prefix_log)
		return;
	onoff("TLP Prefix Log Present", reg, PCI_AER_TLP_PREFIX_LOG_PRESENT);
	*tlp_prefix_log = (reg & PCI_AER_TLP_PREFIX_LOG_PRESENT) ? true : false;
}

static void
pci_conf_print_aer_cap_rooterr_cmd(pcireg_t reg)
{

	onoff("Correctable Error Reporting Enable", reg,
	    PCI_AER_ROOTERR_COR_ENABLE);
	onoff("Non-Fatal Error Reporting Enable", reg,
	    PCI_AER_ROOTERR_NF_ENABLE);
	onoff("Fatal Error Reporting Enable", reg, PCI_AER_ROOTERR_F_ENABLE);
}

static void
pci_conf_print_aer_cap_rooterr_status(pcireg_t reg)
{

	onoff("ERR_COR Received", reg, PCI_AER_ROOTERR_COR_ERR);
	onoff("Multiple ERR_COR Received", reg, PCI_AER_ROOTERR_MULTI_COR_ERR);
	onoff("ERR_FATAL/NONFATAL_ERR Received", reg, PCI_AER_ROOTERR_UC_ERR);
	onoff("Multiple ERR_FATAL/NONFATAL_ERR Received", reg,
	    PCI_AER_ROOTERR_MULTI_UC_ERR);
	onoff("First Uncorrectable Fatal", reg, PCI_AER_ROOTERR_FIRST_UC_FATAL);
	onoff("Non-Fatal Error Messages Received", reg, PCI_AER_ROOTERR_NF_ERR);
	onoff("Fatal Error Messages Received", reg, PCI_AER_ROOTERR_F_ERR);
	printf("      Advanced Error Interrupt Message Number: 0x%u\n",
	    (pcireg_t)__SHIFTOUT(reg, PCI_AER_ROOTERR_INT_MESSAGE));
}

static void
pci_conf_print_aer_cap_errsrc_id(pcireg_t reg)
{

	printf("      Correctable Source ID: 0x%04x\n",
	    (pcireg_t)__SHIFTOUT(reg, PCI_AER_ERRSRC_ID_ERR_COR));
	printf("      ERR_FATAL/NONFATAL Source ID: 0x%04x\n",
	    (pcireg_t)__SHIFTOUT(reg, PCI_AER_ERRSRC_ID_ERR_UC));
}

static void
pci_conf_print_aer_cap(const pcireg_t *regs, int capoff, int extcapoff)
{
	pcireg_t reg;
	int pcie_capoff;
	int pcie_devtype = -1;
	bool tlp_prefix_log = false;

	if (pci_conf_find_cap(regs, capoff, PCI_CAP_PCIEXPRESS, &pcie_capoff)) {
		reg = regs[o2i(pcie_capoff)];
		pcie_devtype = reg & PCIE_XCAP_TYPE_MASK;
		/* PCIe DW9 to DW14 is for PCIe 2.0 and newer */
		if (__SHIFTOUT(reg, PCIE_XCAP_VER_MASK) >= 2) {
			reg = regs[o2i(pcie_capoff + PCIE_DCAP2)];
			/* End-End TLP Prefix Supported */
			if (reg & PCIE_DCAP2_EETLP_PREF) {
				tlp_prefix_log = true;
			}
		}
	}

	printf("\n  Advanced Error Reporting Register\n");

	reg = regs[o2i(extcapoff + PCI_AER_UC_STATUS)];
	printf("    Uncorrectable Error Status register: 0x%08x\n", reg);
	pci_conf_print_aer_cap_uc(reg);
	reg = regs[o2i(extcapoff + PCI_AER_UC_MASK)];
	printf("    Uncorrectable Error Mask register: 0x%08x\n", reg);
	pci_conf_print_aer_cap_uc(reg);
	reg = regs[o2i(extcapoff + PCI_AER_UC_SEVERITY)];
	printf("    Uncorrectable Error Severity register: 0x%08x\n", reg);
	pci_conf_print_aer_cap_uc(reg);

	reg = regs[o2i(extcapoff + PCI_AER_COR_STATUS)];
	printf("    Correctable Error Status register: 0x%08x\n", reg);
	pci_conf_print_aer_cap_cor(reg);
	reg = regs[o2i(extcapoff + PCI_AER_COR_MASK)];
	printf("    Correctable Error Mask register: 0x%08x\n", reg);
	pci_conf_print_aer_cap_cor(reg);

	reg = regs[o2i(extcapoff + PCI_AER_CAP_CONTROL)];
	printf("    Advanced Error Capabilities and Control register: 0x%08x\n",
	    reg);
	pci_conf_print_aer_cap_control(reg, &tlp_prefix_log);
	reg = regs[o2i(extcapoff + PCI_AER_HEADER_LOG)];
	printf("    Header Log register:\n");
	pci_conf_print_regs(regs, extcapoff + PCI_AER_HEADER_LOG,
	    extcapoff + PCI_AER_ROOTERR_CMD);

	switch (pcie_devtype) {
	case PCIE_XCAP_TYPE_ROOT: /* Root Port of PCI Express Root Complex */
	case PCIE_XCAP_TYPE_ROOT_EVNTC:	/* Root Complex Event Collector */
		reg = regs[o2i(extcapoff + PCI_AER_ROOTERR_CMD)];
		printf("    Root Error Command register: 0x%08x\n", reg);
		pci_conf_print_aer_cap_rooterr_cmd(reg);
		reg = regs[o2i(extcapoff + PCI_AER_ROOTERR_STATUS)];
		printf("    Root Error Status register: 0x%08x\n", reg);
		pci_conf_print_aer_cap_rooterr_status(reg);

		reg = regs[o2i(extcapoff + PCI_AER_ERRSRC_ID)];
		printf("    Error Source Identification: 0x%04x\n", reg);
		pci_conf_print_aer_cap_errsrc_id(reg);
		break;
	}

	if (tlp_prefix_log) {
		reg = regs[o2i(extcapoff + PCI_AER_TLP_PREFIX_LOG)];
		printf("    TLP Prefix Log register: 0x%08x\n", reg);
	}
}

static void
pci_conf_print_vc_cap_arbtab(const pcireg_t *regs, int off, const char *name,
    pcireg_t parbsel, int parbsize)
{
	pcireg_t reg;
	int num = 16 << parbsel;
	int num_per_reg = sizeof(pcireg_t) / parbsize;
	int i, j;

	/* First, dump the table */
	for (i = 0; i < num; i += num_per_reg) {
		reg = regs[o2i(off + i / num_per_reg)];
		printf("    %s Arbitration Table: 0x%08x\n", name, reg);
	}
	/* And then, decode each entry */
	for (i = 0; i < num; i += num_per_reg) {
		reg = regs[o2i(off + i / num_per_reg)];
		for (j = 0; j < num_per_reg; j++)
			printf("      Phase[%d]: %d\n", j, reg);
	}
}

static void
pci_conf_print_vc_cap(const pcireg_t *regs, int capoff, int extcapoff)
{
	pcireg_t reg, n;
	int parbtab, parbsize;
	pcireg_t parbsel;
	int varbtab, varbsize;
	pcireg_t varbsel;
	int i, count;

	printf("\n  Virtual Channel Register\n");
	reg = regs[o2i(extcapoff + PCI_VC_CAP1)];
	printf("    Port VC Capability register 1: 0x%08x\n", reg);
	count = __SHIFTOUT(reg, PCI_VC_CAP1_EXT_COUNT);
	printf("      Extended VC Count: %d\n", count);
	n = __SHIFTOUT(reg, PCI_VC_CAP1_LOWPRI_EXT_COUNT);
	printf("      Low Priority Extended VC Count: %u\n", n);
	n = __SHIFTOUT(reg, PCI_VC_CAP1_REFCLK);
	printf("      Reference Clock: %s\n",
	    (n == PCI_VC_CAP1_REFCLK_100NS) ? "100" : "unknown");
	parbsize = 1 << __SHIFTOUT(reg, PCI_VC_CAP1_PORT_ARB_TABLE_SIZE);
	printf("      Port Arbitration Table Entry Size: %dbit\n", parbsize);

	reg = regs[o2i(extcapoff + PCI_VC_CAP2)];
	printf("    Port VC Capability register 2: 0x%08x\n", reg);
	onoff("Hardware fixed arbitration scheme",
	    reg, PCI_VC_CAP2_ARB_CAP_HW_FIXED_SCHEME);
	onoff("WRR arbitration with 32 phases",
	    reg, PCI_VC_CAP2_ARB_CAP_WRR_32);
	onoff("WRR arbitration with 64 phases",
	    reg, PCI_VC_CAP2_ARB_CAP_WRR_64);
	onoff("WRR arbitration with 128 phases",
	    reg, PCI_VC_CAP2_ARB_CAP_WRR_128);
	varbtab = __SHIFTOUT(reg, PCI_VC_CAP2_ARB_TABLE_OFFSET);
	printf("      VC Arbitration Table Offset: 0x%x\n", varbtab);

	reg = regs[o2i(extcapoff + PCI_VC_CONTROL)] & 0xffff;
	printf("    Port VC Control register: 0x%04x\n", reg);
	varbsel = __SHIFTOUT(reg, PCI_VC_CONTROL_VC_ARB_SELECT);
	printf("      VC Arbitration Select: 0x%x\n", varbsel);

	reg = regs[o2i(extcapoff + PCI_VC_STATUS)] >> 16;
	printf("    Port VC Status register: 0x%04x\n", reg);
	onoff("VC Arbitration Table Status",
	    reg, PCI_VC_STATUS_LOAD_VC_ARB_TABLE);

	for (i = 0; i < count + 1; i++) {
		reg = regs[o2i(extcapoff + PCI_VC_RESOURCE_CAP(i))];
		printf("    VC number %d\n", i);
		printf("      VC Resource Capability Register: 0x%08x\n", reg);
		onoff("  Non-configurable Hardware fixed arbitration scheme",
		    reg, PCI_VC_RESOURCE_CAP_PORT_ARB_CAP_HW_FIXED_SCHEME);
		onoff("  WRR arbitration with 32 phases",
		    reg, PCI_VC_RESOURCE_CAP_PORT_ARB_CAP_WRR_32);
		onoff("  WRR arbitration with 64 phases",
		    reg, PCI_VC_RESOURCE_CAP_PORT_ARB_CAP_WRR_64);
		onoff("  WRR arbitration with 128 phases",
		    reg, PCI_VC_RESOURCE_CAP_PORT_ARB_CAP_WRR_128);
		onoff("  Time-based WRR arbitration with 128 phases",
		    reg, PCI_VC_RESOURCE_CAP_PORT_ARB_CAP_TWRR_128);
		onoff("  WRR arbitration with 256 phases",
		    reg, PCI_VC_RESOURCE_CAP_PORT_ARB_CAP_WRR_256);
		onoff("  Advanced Packet Switching",
		    reg, PCI_VC_RESOURCE_CAP_ADV_PKT_SWITCH);
		onoff("  Reject Snoop Transaction",
		    reg, PCI_VC_RESOURCE_CAP_REJCT_SNOOP_TRANS);
		n = __SHIFTOUT(reg, PCI_VC_RESOURCE_CAP_MAX_TIME_SLOTS) + 1;
		printf("        Maximum Time Slots: %d\n", n);
		parbtab = reg >> PCI_VC_RESOURCE_CAP_PORT_ARB_TABLE_OFFSET_S;
		printf("        Port Arbitration Table offset: 0x%02x\n",
		    parbtab);

		reg = regs[o2i(extcapoff + PCI_VC_RESOURCE_CTL(i))];
		printf("      VC Resource Control Register: 0x%08x\n", reg);
		printf("        TC/VC Map: %02x\n",
		    (pcireg_t)__SHIFTOUT(reg, PCI_VC_RESOURCE_CTL_TCVC_MAP));
		/*
		 * The load Port Arbitration Table bit is used to update
		 * the Port Arbitration logic and it's always 0 on read, so
		 * we don't print it.
		 */
		parbsel = __SHIFTOUT(reg, PCI_VC_RESOURCE_CTL_PORT_ARB_SELECT);
		printf("        Port Arbitration Select: %x\n", parbsel);
		n = __SHIFTOUT(reg, PCI_VC_RESOURCE_CTL_VC_ID);
		printf("        VC ID %d\n", n);
		onoff("  VC Enable", reg, PCI_VC_RESOURCE_CTL_VC_ENABLE);

		reg = regs[o2i(extcapoff + PCI_VC_RESOURCE_STA(i))] >> 16;
		printf("      VC Resource Status Register: 0x%08x\n", reg);
		onoff("  Port Arbitration Table Status",
		    reg, PCI_VC_RESOURCE_STA_PORT_ARB_TABLE);
		onoff("  VC Negotiation Pending",
		    reg, PCI_VC_RESOURCE_STA_VC_NEG_PENDING);

		if ((parbtab != 0) && (parbsel != 0))
			pci_conf_print_vc_cap_arbtab(regs, extcapoff + parbtab,
			    "Port", parbsel, parbsize);
	}

	varbsize = 8;
	if ((varbtab != 0) && (varbsel != 0))
		pci_conf_print_vc_cap_arbtab(regs, extcapoff + varbtab,
		    "  VC", varbsel, varbsize);
}

static const char *
pci_conf_print_pwrbdgt_base_power(uint8_t reg)
{

	switch (reg) {
	case 0xf0:
		return "250W";
	case 0xf1:
		return "275W";
	case 0xf2:
		return "300W";
	default:
		return "Unknown";
	}
}

static const char *
pci_conf_print_pwrbdgt_data_scale(uint8_t reg)
{

	switch (reg) {
	case 0x00:
		return "1.0x";
	case 0x01:
		return "0.1x";
	case 0x02:
		return "0.01x";
	case 0x03:
		return "0.001x";
	default:
		return "wrong value!";
	}
}

static const char *
pci_conf_print_pwrbdgt_type(uint8_t reg)
{

	switch (reg) {
	case 0x00:
		return "PME Aux";
	case 0x01:
		return "Auxilary";
	case 0x02:
		return "Idle";
	case 0x03:
		return "Sustained";
	case 0x07:
		return "Maximun";
	default:
		return "Unknown";
	}
}

static const char *
pci_conf_print_pwrbdgt_pwrrail(uint8_t reg)
{

	switch (reg) {
	case 0x00:
		return "Power(12V)";
	case 0x01:
		return "Power(3.3V)";
	case 0x02:
		return "Power(1.5V or 1.8V)";
	case 0x07:
		return "Thermal";
	default:
		return "Unknown";
	}
}

static void
pci_conf_print_pwrbdgt_cap(const pcireg_t *regs, int capoff, int extcapoff)
{
	pcireg_t reg;

	printf("\n  Power Budget Register\n");

	reg = regs[o2i(extcapoff + PCI_PWRBDGT_DSEL)];
	printf("    Data Select register: 0x%08x\n", reg);

	reg = regs[o2i(extcapoff + PCI_PWRBDGT_DATA)];
	printf("    Data register: 0x%08x\n", reg);
	printf("      Base Power: %s\n",
	    pci_conf_print_pwrbdgt_base_power((uint8_t)reg));
	printf("      Data Scale: %s\n",
	    pci_conf_print_pwrbdgt_data_scale(
		    (uint8_t)(__SHIFTOUT(reg, PCI_PWRBDGT_DATA_SCALE))));
	printf("      PM Sub State: 0x%hhx\n",
	    (uint8_t)__SHIFTOUT(reg, PCI_PWRBDGT_PM_SUBSTAT));
	printf("      PM State: D%u\n",
	    (unsigned int)__SHIFTOUT(reg, PCI_PWRBDGT_PM_STAT));
	printf("      Type: %s\n",
	    pci_conf_print_pwrbdgt_type(
		    (uint8_t)(__SHIFTOUT(reg, PCI_PWRBDGT_TYPE))));
	printf("      Power Rail: %s\n",
	    pci_conf_print_pwrbdgt_pwrrail(
		    (uint8_t)(__SHIFTOUT(reg, PCI_PWRBDGT_PWRRAIL))));

	reg = regs[o2i(extcapoff + PCI_PWRBDGT_CAP)];
	printf("    Power Budget Capability register: 0x%08x\n", reg);
	onoff("System Allocated",
	    reg, PCI_PWRBDGT_CAP_SYSALLOC);
}

static const char *
pci_conf_print_rclink_dcl_cap_elmtype(unsigned char type)
{

	switch (type) {
	case 0x00:
		return "Configuration Space Element";
	case 0x01:
		return "System Egress Port or internal sink (memory)";
	case 0x02:
		return "Internal Root Complex Link";
	default:
		return "Unknown";
	}
}

static void
pci_conf_print_rclink_dcl_cap(const pcireg_t *regs, int capoff, int extcapoff)
{
	pcireg_t reg;
	unsigned char nent, linktype;
	int i;

	printf("\n  Root Complex Link Declaration\n");

	reg = regs[o2i(extcapoff + PCI_RCLINK_DCL_ESDESC)];
	printf("    Element Self Description Register: 0x%08x\n", reg);
	printf("      Element Type: %s\n",
	    pci_conf_print_rclink_dcl_cap_elmtype((unsigned char)reg));
	nent = __SHIFTOUT(reg, PCI_RCLINK_DCL_ESDESC_NUMLINKENT);
	printf("      Number of Link Entries: %hhu\n", nent);
	printf("      Component ID: %hhu\n",
	    (uint8_t)__SHIFTOUT(reg, PCI_RCLINK_DCL_ESDESC_COMPID));
	printf("      Port Number: %hhu\n",
	    (uint8_t)__SHIFTOUT(reg, PCI_RCLINK_DCL_ESDESC_PORTNUM));
	for (i = 0; i < nent; i++) {
		reg = regs[o2i(extcapoff + PCI_RCLINK_DCL_LINKDESC(i))];
		printf("    Link Description Register: 0x%08x\n", reg);
		onoff("Link Valid", reg,PCI_RCLINK_DCL_LINKDESC_LVALID);
		linktype = reg & PCI_RCLINK_DCL_LINKDESC_LTYPE;
		onoff2("Link Type", reg, PCI_RCLINK_DCL_LINKDESC_LTYPE,
		    "Configuration Space", "Memory-Mapped Space");
		onoff("Associated RCRB Header", reg,
		    PCI_RCLINK_DCL_LINKDESC_ARCRBH);
		printf("      Target Component ID: %hhu\n",
		    (unsigned char)__SHIFTOUT(reg,
			PCI_RCLINK_DCL_LINKDESC_TCOMPID));
		printf("      Target Port Number: %hhu\n",
		    (unsigned char)__SHIFTOUT(reg,
			PCI_RCLINK_DCL_LINKDESC_TPNUM));

		if (linktype == 0) {
			/* Memory-Mapped Space */
			reg = regs[o2i(extcapoff
				    + PCI_RCLINK_DCL_LINKADDR_LT0_LO(i))];
			printf("    Link Address Low Register: 0x%08x\n", reg);
			reg = regs[o2i(extcapoff
				    + PCI_RCLINK_DCL_LINKADDR_LT0_HI(i))];
			printf("    Link Address High Register: 0x%08x\n",reg);
		} else {
			unsigned int nb;
			pcireg_t lo, hi;

			/* Configuration Space */
			lo = regs[o2i(extcapoff
				    + PCI_RCLINK_DCL_LINKADDR_LT1_LO(i))];
			printf("    Configuration Space Low Register: 0x%08x"
			    "\n", lo);
			hi = regs[o2i(extcapoff
				    + PCI_RCLINK_DCL_LINKADDR_LT1_HI(i))];
			printf("    Configuration Space High Register: 0x%08x"
			    "\n", hi);
			nb = __SHIFTOUT(lo, PCI_RCLINK_DCL_LINKADDR_LT1_N);
			printf("      N: %u\n", nb);
			printf("      Func: %hhu\n",
			    (unsigned char)__SHIFTOUT(lo,
				PCI_RCLINK_DCL_LINKADDR_LT1_FUNC));
			printf("      Dev: %hhu\n",
			    (unsigned char)__SHIFTOUT(lo,
				PCI_RCLINK_DCL_LINKADDR_LT1_DEV));
			printf("      Bus: %hhu\n",
			    (unsigned char)__SHIFTOUT(lo,
				PCI_RCLINK_DCL_LINKADDR_LT1_BUS(nb)));
			lo &= PCI_RCLINK_DCL_LINKADDR_LT1_BAL(i);
			printf("      Configuration Space Base Address: 0x%016"
			    PRIx64 "\n", ((uint64_t)hi << 32) + lo);
		}
	}
}

/* XXX pci_conf_print_rclink_ctl_cap */

static void
pci_conf_print_rcec_assoc_cap(const pcireg_t *regs, int capoff, int extcapoff)
{
	pcireg_t reg;

	printf("\n  Root Complex Event Collector Association\n");

	reg = regs[o2i(extcapoff + PCI_RCEC_ASSOC_ASSOCBITMAP)];
	printf("    Association Bitmap for Root Complex Integrated Devices:"
	    " 0x%08x\n", reg);
}

/* XXX pci_conf_print_mfvc_cap */
/* XXX pci_conf_print_vc2_cap */
/* XXX pci_conf_print_rcrb_cap */
/* XXX pci_conf_print_vendor_cap */
/* XXX pci_conf_print_cac_cap */

static void
pci_conf_print_acs_cap(const pcireg_t *regs, int capoff, int extcapoff)
{
	pcireg_t reg, cap, ctl;
	unsigned int size, i;

	printf("\n  Access Control Services\n");

	reg = regs[o2i(extcapoff + PCI_ACS_CAP)];
	cap = reg & 0xffff;
	ctl = reg >> 16;
	printf("    ACS Capability register: 0x%08x\n", cap);
	onoff("ACS Source Validation", cap, PCI_ACS_CAP_V);
	onoff("ACS Transaction Blocking", cap, PCI_ACS_CAP_B);
	onoff("ACS P2P Request Redirect", cap, PCI_ACS_CAP_R);
	onoff("ACS P2P Completion Redirect", cap, PCI_ACS_CAP_C);
	onoff("ACS Upstream Forwarding", cap, PCI_ACS_CAP_U);
	onoff("ACS Egress Control", cap, PCI_ACS_CAP_E);
	onoff("ACS Direct Translated P2P", cap, PCI_ACS_CAP_T);
	size = __SHIFTOUT(cap, PCI_ACS_CAP_ECVSIZE);
	if (size == 0)
		size = 256;
	printf("      Egress Control Vector Size: %u\n", size);
	printf("    ACS Control register: 0x%08x\n", ctl);
	onoff("ACS Source Validation Enable", ctl, PCI_ACS_CTL_V);
	onoff("ACS Transaction Blocking Enable", ctl, PCI_ACS_CTL_B);
	onoff("ACS P2P Request Redirect Enable", ctl, PCI_ACS_CTL_R);
	onoff("ACS P2P Completion Redirect Enable", ctl, PCI_ACS_CTL_C);
	onoff("ACS Upstream Forwarding Enable", ctl, PCI_ACS_CTL_U);
	onoff("ACS Egress Control Enable", ctl, PCI_ACS_CTL_E);
	onoff("ACS Direct Translated P2P Enable", ctl, PCI_ACS_CTL_T);

	/*
	 * If the P2P Egress Control Capability bit is 0, ignore the Egress
	 * Control vector.
	 */
	if ((cap & PCI_ACS_CAP_E) == 0)
		return;
	for (i = 0; i < size; i += 32)
		printf("    Egress Control Vector [%u..%u]: %x\n", i + 31,
		    i, regs[o2i(extcapoff + PCI_ACS_ECV + (i / 32) * 4 )]);
}

static void
pci_conf_print_ari_cap(const pcireg_t *regs, int capoff, int extcapoff)
{
	pcireg_t reg, cap, ctl;

	printf("\n  Alternative Routing-ID Interpretation Register\n");

	reg = regs[o2i(extcapoff + PCI_ARI_CAP)];
	cap = reg & 0xffff;
	ctl = reg >> 16;
	printf("    Capability register: 0x%08x\n", cap);
	onoff("MVFC Function Groups Capability", reg, PCI_ARI_CAP_M);
	onoff("ACS Function Groups Capability", reg, PCI_ARI_CAP_A);
	printf("      Next Function Number: %u\n",
	    (unsigned int)__SHIFTOUT(reg, PCI_ARI_CAP_NXTFN));
	printf("    Control register: 0x%08x\n", ctl);
	onoff("MVFC Function Groups Enable", reg, PCI_ARI_CTL_M);
	onoff("ACS Function Groups Enable", reg, PCI_ARI_CTL_A);
	printf("      Function Group: %u\n",
	    (unsigned int)__SHIFTOUT(reg, PCI_ARI_CTL_FUNCGRP));
}

static void
pci_conf_print_ats_cap(const pcireg_t *regs, int capoff, int extcapoff)
{
	pcireg_t reg, cap, ctl;
	unsigned int num;

	printf("\n  Address Translation Services\n");

	reg = regs[o2i(extcapoff + PCI_ARI_CAP)];
	cap = reg & 0xffff;
	ctl = reg >> 16;
	printf("    Capability register: 0x%04x\n", cap);
	num = __SHIFTOUT(reg, PCI_ATS_CAP_INVQDEPTH);
	if (num == 0)
		num = 32;
	printf("      Invalidate Queue Depth: %u\n", num);
	onoff("Page Aligned Request", reg, PCI_ATS_CAP_PALIGNREQ);

	printf("    Control register: 0x%04x\n", ctl);
	printf("      Smallest Translation Unit: %u\n",
	    (unsigned int)__SHIFTOUT(reg, PCI_ATS_CTL_STU));
	onoff("Enable", reg, PCI_ATS_CTL_EN);
}

static void
pci_conf_print_sernum_cap(const pcireg_t *regs, int capoff, int extcapoff)
{
	pcireg_t lo, hi;

	printf("\n  Device Serial Number Register\n");

	lo = regs[o2i(extcapoff + PCI_SERIAL_LOW)];
	hi = regs[o2i(extcapoff + PCI_SERIAL_HIGH)];
	printf("    Serial Number: %02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\n",
	    hi >> 24, (hi >> 16) & 0xff, (hi >> 8) & 0xff, hi & 0xff,
	    lo >> 24, (lo >> 16) & 0xff, (lo >> 8) & 0xff, lo & 0xff);
}

static void
pci_conf_print_sriov_cap(const pcireg_t *regs, int capoff, int extcapoff)
{
	char buf[sizeof("99999 MB")];
	pcireg_t reg;
	pcireg_t total_vfs;
	int i;
	bool first;

	printf("\n  Single Root IO Virtualization Register\n");

	reg = regs[o2i(extcapoff + PCI_SRIOV_CAP)];
	printf("    Capabilities register: 0x%08x\n", reg);
	onoff("VF Migration Capable", reg, PCI_SRIOV_CAP_VF_MIGRATION);
	onoff("ARI Capable Hierarchy Preserved", reg,
	    PCI_SRIOV_CAP_ARI_CAP_HIER_PRESERVED);
	if (reg & PCI_SRIOV_CAP_VF_MIGRATION) {
		printf("      VF Migration Interrupt Message Number: 0x%u\n",
		    (pcireg_t)__SHIFTOUT(reg,
		      PCI_SRIOV_CAP_VF_MIGRATION_INTMSG_N));
	}

	reg = regs[o2i(extcapoff + PCI_SRIOV_CTL)] & 0xffff;
	printf("    Control register: 0x%04x\n", reg);
	onoff("VF Enable", reg, PCI_SRIOV_CTL_VF_ENABLE);
	onoff("VF Migration Enable", reg, PCI_SRIOV_CTL_VF_MIGRATION_SUPPORT);
	onoff("VF Migration Interrupt Enable", reg,
	    PCI_SRIOV_CTL_VF_MIGRATION_INT_ENABLE);
	onoff("VF Memory Space Enable", reg, PCI_SRIOV_CTL_VF_MSE);
	onoff("ARI Capable Hierarchy", reg, PCI_SRIOV_CTL_ARI_CAP_HIER);

	reg = regs[o2i(extcapoff + PCI_SRIOV_STA)] >> 16;
	printf("    Status register: 0x%04x\n", reg);
	onoff("VF Migration Status", reg, PCI_SRIOV_STA_VF_MIGRATION);

	reg = regs[o2i(extcapoff + PCI_SRIOV_INITIAL_VFS)] & 0xffff;
	printf("    InitialVFs register: 0x%04x\n", reg);
	total_vfs = reg = regs[o2i(extcapoff + PCI_SRIOV_TOTAL_VFS)] >> 16;
	printf("    TotalVFs register: 0x%04x\n", reg);
	reg = regs[o2i(extcapoff + PCI_SRIOV_NUM_VFS)] & 0xffff;
	printf("    NumVFs register: 0x%04x\n", reg);

	reg = regs[o2i(extcapoff + PCI_SRIOV_FUNC_DEP_LINK)] >> 16;
	printf("    Function Dependency Link register: 0x%04x\n", reg);

	reg = regs[o2i(extcapoff + PCI_SRIOV_VF_OFF)] & 0xffff;
	printf("    First VF Offset register: 0x%04x\n", reg);
	reg = regs[o2i(extcapoff + PCI_SRIOV_VF_STRIDE)] >> 16;
	printf("    VF Stride register: 0x%04x\n", reg);

	reg = regs[o2i(extcapoff + PCI_SRIOV_PAGE_CAP)];
	printf("    Supported Page Sizes register: 0x%08x\n", reg);
	printf("      Supported Page Size:");
	for (i = 0, first = true; i < 32; i++) {
		if (reg & __BIT(i)) {
#ifdef _KERNEL
			format_bytes(buf, sizeof(buf), 1LL << (i + 12));
#else
			humanize_number(buf, sizeof(buf), 1LL << (i + 12), "B",
			    HN_AUTOSCALE, 0);
#endif
			printf("%s %s", first ? "" : ",", buf);
			first = false;
		}
	}
	printf("\n");

	reg = regs[o2i(extcapoff + PCI_SRIOV_PAGE_SIZE)];
	printf("    System Page Sizes register: 0x%08x\n", reg);
	printf("      Page Size: ");
	if (reg != 0) {
#ifdef _KERNEL
		format_bytes(buf, sizeof(buf), 1LL << (ffs(reg) + 12));
#else
		humanize_number(buf, sizeof(buf), 1LL << (ffs(reg) + 12), "B",
		    HN_AUTOSCALE, 0);
#endif
		printf("%s", buf);
	} else {
		printf("unknown");
	}
	printf("\n");

	for (i = 0; i < 6; i++) {
		reg = regs[o2i(extcapoff + PCI_SRIOV_BAR(i))];
		printf("    VF BAR%d register: 0x%08x\n", i, reg);
	}

	if (total_vfs > 0) {
		reg = regs[o2i(extcapoff + PCI_SRIOV_VF_MIG_STA_AR)];
		printf("    VF Migration State Array Offset register: 0x%08x\n",
		    reg);
		printf("      VF Migration State Offset: 0x%08x\n",
		    (pcireg_t)__SHIFTOUT(reg, PCI_SRIOV_VF_MIG_STA_OFFSET));
		i = __SHIFTOUT(reg, PCI_SRIOV_VF_MIG_STA_BIR);
		printf("      VF Migration State BIR: ");
		if (i >= 0 && i <= 5) {
			printf("BAR%d", i);
		} else {
			printf("unknown BAR (%d)", i);
		}
		printf("\n");
	}
}

/* XXX pci_conf_print_mriov_cap */
/* XXX pci_conf_print_multicast_cap */

static void
pci_conf_print_page_req_cap(const pcireg_t *regs, int capoff, int extcapoff)
{
	pcireg_t reg, ctl, sta;

	printf("\n  Page Request\n");

	reg = regs[o2i(extcapoff + PCI_PAGE_REQ_CTL)];
	ctl = reg & 0xffff;
	sta = reg >> 16;
	printf("    Control Register: 0x%04x\n", ctl);
	onoff("Enalbe", reg, PCI_PAGE_REQ_CTL_E);
	onoff("Reset", reg, PCI_PAGE_REQ_CTL_R);

	printf("    Status Register: 0x%04x\n", sta);
	onoff("Response Failure", reg, PCI_PAGE_REQ_STA_RF);
	onoff("Unexpected Page Request Group Index", reg,
	    PCI_PAGE_REQ_STA_UPRGI);
	onoff("Stopped", reg, PCI_PAGE_REQ_STA_S);

	reg = regs[o2i(extcapoff + PCI_PAGE_REQ_OUTSTCAPA)];
	printf("    Outstanding Page Request Capacity: %u\n", reg);
	reg = regs[o2i(extcapoff + PCI_PAGE_REQ_OUTSTALLOC)];
	printf("    Outstanding Page Request Allocation: %u\n", reg);
}

/* XXX pci_conf_print_amd_cap */
/* XXX pci_conf_print_resize_bar_cap */
/* XXX pci_conf_print_dpa_cap */

static const char *
pci_conf_print_tph_req_cap_sttabloc(unsigned char val)
{

	switch (val) {
	case 0x0:
		return "Not Present";
	case 0x1:
		return "in the TPH Requester Capability Structure";
	case 0x2:
		return "in the MSI-X Table";
	default:
		return "Unknown";
	}
}

static void
pci_conf_print_tph_req_cap(const pcireg_t *regs, int capoff, int extcapoff)
{
	pcireg_t reg;
	int size, i, j;

	printf("\n  TPH Requester Extended Capability\n");

	reg = regs[o2i(extcapoff + PCI_TPH_REQ_CAP)];
	printf("    TPH Requester Capabililty register: 0x%08x\n", reg);
	onoff("No ST Mode Supported", reg, PCI_TPH_REQ_CAP_NOST);
	onoff("Interrupt Vector Mode Supported", reg, PCI_TPH_REQ_CAP_INTVEC);
	onoff("Device Specific Mode Supported", reg, PCI_TPH_REQ_CAP_DEVSPEC);
	onoff("Extend TPH Reqester Supported", reg, PCI_TPH_REQ_CAP_XTPHREQ);
	printf("      ST Table Location: %s\n",
	    pci_conf_print_tph_req_cap_sttabloc(
		    (unsigned char)__SHIFTOUT(reg, PCI_TPH_REQ_CAP_STTBLLOC)));
	size = __SHIFTOUT(reg, PCI_TPH_REQ_CAP_STTBLSIZ) + 1;
	printf("      ST Table Size: %d\n", size);
	for (i = 0; i < size ; i += 2) {
		reg = regs[o2i(extcapoff + PCI_TPH_REQ_STTBL + i / 2)];
		for (j = 0; j < 2 ; j++) {
			uint32_t entry = reg;

			if (j != 0)
				entry >>= 16;
			entry &= 0xffff;
			printf("    TPH ST Table Entry (%d): 0x%04"PRIx32"\n",
			    i + j, entry);
		}
	}
}

static void
pci_conf_print_ltr_cap(const pcireg_t *regs, int capoff, int extcapoff)
{
	pcireg_t reg;

	printf("\n  Latency Tolerance Reporting\n");
	reg = regs[o2i(extcapoff + PCI_LTR_MAXSNOOPLAT)] & 0xffff;
	printf("    Max Snoop Latency Register: 0x%04x\n", reg);
	printf("      Max Snoop LatencyValue: %u\n",
	    (pcireg_t)__SHIFTOUT(reg, PCI_LTR_MAXSNOOPLAT_VAL));
	printf("      Max Snoop LatencyScale: %uns\n",
	    PCI_LTR_SCALETONS(__SHIFTOUT(reg, PCI_LTR_MAXSNOOPLAT_SCALE)));
	reg = regs[o2i(extcapoff + PCI_LTR_MAXNOSNOOPLAT)] >> 16;
	printf("    Max No-Snoop Latency Register: 0x%04x\n", reg);
	printf("      Max No-Snoop LatencyValue: %u\n",
	    (pcireg_t)__SHIFTOUT(reg, PCI_LTR_MAXNOSNOOPLAT_VAL));
	printf("      Max No-Snoop LatencyScale: %uns\n",
	    PCI_LTR_SCALETONS(__SHIFTOUT(reg, PCI_LTR_MAXNOSNOOPLAT_SCALE)));
}

static void
pci_conf_print_sec_pcie_cap(const pcireg_t *regs, int capoff, int extcapoff)
{
	int pcie_capoff;
	pcireg_t reg;
	int i, maxlinkwidth;

	printf("\n  Secondary PCI Express Register\n");

	reg = regs[o2i(extcapoff + PCI_SECPCIE_LCTL3)];
	printf("    Link Control 3 register: 0x%08x\n", reg);
	onoff("Perform Equalization", reg, PCI_SECPCIE_LCTL3_PERFEQ);
	onoff("Link Equalization Request Interrupt Enable",
	    reg, PCI_SECPCIE_LCTL3_LINKEQREQ_IE);

	reg = regs[o2i(extcapoff + PCI_SECPCIE_LANEERR_STA)];
	printf("    Lane Error Status register: 0x%08x\n", reg);

	/* Get Max Link Width */
	if (pci_conf_find_cap(regs, capoff, PCI_CAP_PCIEXPRESS, &pcie_capoff)){
		reg = regs[o2i(pcie_capoff + PCIE_LCAP)];
		maxlinkwidth = __SHIFTOUT(reg, PCIE_LCAP_MAX_WIDTH);
	} else {
		printf("error: falied to get PCIe capablity\n");
		return;
	}
	for (i = 0; i < maxlinkwidth; i++) {
		reg = regs[o2i(extcapoff + PCI_SECPCIE_EQCTL(i))];
		if (i % 2 != 0)
			reg >>= 16;
		else
			reg &= 0xffff;
		printf("    Equalization Control Register (Link %d): %04x\n",
		    i, reg);
		printf("      Downstream Port Transmit Preset: 0x%x\n",
		    (pcireg_t)__SHIFTOUT(reg,
			PCI_SECPCIE_EQCTL_DP_XMIT_PRESET));
		printf("      Downstream Port Receive Hint: 0x%x\n",
		    (pcireg_t)__SHIFTOUT(reg, PCI_SECPCIE_EQCTL_DP_RCV_HINT));
		printf("      Upstream Port Transmit Preset: 0x%x\n",
		    (pcireg_t)__SHIFTOUT(reg,
			PCI_SECPCIE_EQCTL_UP_XMIT_PRESET));
		printf("      Upstream Port Receive Hint: 0x%x\n",
		    (pcireg_t)__SHIFTOUT(reg, PCI_SECPCIE_EQCTL_UP_RCV_HINT));
	}
}

/* XXX pci_conf_print_pmux_cap */

static void
pci_conf_print_pasid_cap(const pcireg_t *regs, int capoff, int extcapoff)
{
	pcireg_t reg, cap, ctl;
	unsigned int num;

	printf("\n  Process Address Space ID\n");

	reg = regs[o2i(extcapoff + PCI_PASID_CAP)];
	cap = reg & 0xffff;
	ctl = reg >> 16;
	printf("    PASID Capability Register: 0x%04x\n", cap);
	onoff("Execute Permission Supported", reg, PCI_PASID_CAP_XPERM);
	onoff("Privileged Mode Supported", reg, PCI_PASID_CAP_PRIVMODE);
	num = (1 << __SHIFTOUT(reg, PCI_PASID_CAP_MAXPASIDW)) - 1;
	printf("      Max PASID Width: %u\n", num);

	printf("    PASID Control Register: 0x%04x\n", ctl);
	onoff("PASID Enable", reg, PCI_PASID_CTL_PASID_EN);
	onoff("Execute Permission Enable", reg, PCI_PASID_CTL_XPERM_EN);
	onoff("Privileged Mode Enable", reg, PCI_PASID_CTL_PRIVMODE_EN);
}

static void
pci_conf_print_lnr_cap(const pcireg_t *regs, int capoff, int extcapoff)
{
	pcireg_t reg, cap, ctl;
	unsigned int num;

	printf("\n  LN Requester\n");

	reg = regs[o2i(extcapoff + PCI_LNR_CAP)];
	cap = reg & 0xffff;
	ctl = reg >> 16;
	printf("    LNR Capability register: 0x%04x\n", cap);
	onoff("LNR-64 Supported", reg, PCI_LNR_CAP_64);
	onoff("LNR-128 Supported", reg, PCI_LNR_CAP_128);
	num = 1 << __SHIFTOUT(reg, PCI_LNR_CAP_REGISTMAX);
	printf("      LNR Registration MAX: %u\n", num);

	printf("    LNR Control register: 0x%04x\n", ctl);
	onoff("LNR Enable", reg, PCI_LNR_CTL_EN);
	onoff("LNR CLS", reg, PCI_LNR_CTL_CLS);
	num = 1 << __SHIFTOUT(reg, PCI_LNR_CTL_REGISTLIM);
	printf("      LNR Registration Limit: %u\n", num);
}

/* XXX pci_conf_print_dpc_cap */

static int
pci_conf_l1pm_cap_tposcale(unsigned char scale)
{

	/* Return scale in us */
	switch (scale) {
	case 0x0:
		return 2;
	case 0x1:
		return 10;
	case 0x2:
		return 100;
	default:
		return -1;
	}
}

static void
pci_conf_print_l1pm_cap(const pcireg_t *regs, int capoff, int extcapoff)
{
	pcireg_t reg;
	int scale, val;

	printf("\n  L1 PM Substates\n");

	reg = regs[o2i(extcapoff + PCI_L1PM_CAP)];
	printf("    L1 PM Substates Capability register: 0x%08x\n", reg);
	onoff("PCI-PM L1.2 Supported", reg, PCI_L1PM_CAP_PCIPM12);
	onoff("PCI-PM L1.1 Supported", reg, PCI_L1PM_CAP_PCIPM11);
	onoff("ASPM L1.2 Supported", reg, PCI_L1PM_CAP_ASPM12);
	onoff("ASPM L1.1 Supported", reg, PCI_L1PM_CAP_ASPM11);
	onoff("L1 PM Substates Supported", reg, PCI_L1PM_CAP_L1PM);
	printf("      Port Common Mode Restore Time: %uus\n",
	    (unsigned int)__SHIFTOUT(reg, PCI_L1PM_CAP_PCMRT));
	scale = pci_conf_l1pm_cap_tposcale(
		__SHIFTOUT(reg, PCI_L1PM_CAP_PTPOSCALE));
	val = __SHIFTOUT(reg, PCI_L1PM_CAP_PTPOVAL);
	printf("      Port T_POWER_ON: ");
	if (scale == -1)
		printf("unknown\n");
	else
		printf("%dus\n", val * scale);

	reg = regs[o2i(extcapoff + PCI_L1PM_CTL1)];
	printf("    L1 PM Substates Control register 1: 0x%08x\n", reg);
	onoff("PCI-PM L1.2 Enable", reg, PCI_L1PM_CTL1_PCIPM12_EN);
	onoff("PCI-PM L1.1 Enable", reg, PCI_L1PM_CTL1_PCIPM11_EN);
	onoff("ASPM L1.2 Enable", reg, PCI_L1PM_CTL1_ASPM12_EN);
	onoff("ASPM L1.1 Enable", reg, PCI_L1PM_CTL1_ASPM11_EN);
	printf("      Common Mode Restore Time: %uus\n",
	    (unsigned int)__SHIFTOUT(reg, PCI_L1PM_CTL1_CMRT));
	scale = PCI_LTR_SCALETONS(__SHIFTOUT(reg, PCI_L1PM_CTL1_LTRTHSCALE));
	val = __SHIFTOUT(reg, PCI_L1PM_CTL1_LTRTHVAL);
	printf("      LTR L1.2 THRESHOLD: %dus\n", val * scale);

	reg = regs[o2i(extcapoff + PCI_L1PM_CTL2)];
	printf("    L1 PM Substates Control register 2: 0x%08x\n", reg);
	scale = pci_conf_l1pm_cap_tposcale(
		__SHIFTOUT(reg, PCI_L1PM_CTL2_TPOSCALE));
	val = __SHIFTOUT(reg, PCI_L1PM_CTL2_TPOVAL);
	printf("      T_POWER_ON: ");
	if (scale == -1)
		printf("unknown\n");
	else
		printf("%dus\n", val * scale);
}

/* XXX pci_conf_print_ptm_cap */
/* XXX pci_conf_print_mpcie_cap */
/* XXX pci_conf_print_frsq_cap */
/* XXX pci_conf_print_rtr_cap */
/* XXX pci_conf_print_desigvndsp_cap */

#undef	MS
#undef	SM
#undef	RW

static struct {
	pcireg_t cap;
	const char *name;
	void (*printfunc)(const pcireg_t *, int, int);
} pci_extcaptab[] = {
	{ 0,			"reserved",
	  NULL },
	{ PCI_EXTCAP_AER,	"Advanced Error Reporting",
	  pci_conf_print_aer_cap },
	{ PCI_EXTCAP_VC,	"Virtual Channel",
	  pci_conf_print_vc_cap },
	{ PCI_EXTCAP_SERNUM,	"Device Serial Number",
	  pci_conf_print_sernum_cap },
	{ PCI_EXTCAP_PWRBDGT,	"Power Budgeting",
	  pci_conf_print_pwrbdgt_cap },
	{ PCI_EXTCAP_RCLINK_DCL,"Root Complex Link Declaration",
	  pci_conf_print_rclink_dcl_cap },
	{ PCI_EXTCAP_RCLINK_CTL,"Root Complex Internal Link Control",
	  NULL },
	{ PCI_EXTCAP_RCEC_ASSOC,"Root Complex Event Collector Association",
	  pci_conf_print_rcec_assoc_cap },
	{ PCI_EXTCAP_MFVC,	"Multi-Function Virtual Channel",
	  NULL },
	{ PCI_EXTCAP_VC2,	"Virtual Channel",
	  NULL },
	{ PCI_EXTCAP_RCRB,	"RCRB Header",
	  NULL },
	{ PCI_EXTCAP_VENDOR,	"Vendor Unique",
	  NULL },
	{ PCI_EXTCAP_CAC,	"Configuration Access Correction",
	  NULL },
	{ PCI_EXTCAP_ACS,	"Access Control Services",
	  pci_conf_print_acs_cap },
	{ PCI_EXTCAP_ARI,	"Alternative Routing-ID Interpretation",
	  pci_conf_print_ari_cap },
	{ PCI_EXTCAP_ATS,	"Address Translation Services",
	  pci_conf_print_ats_cap },
	{ PCI_EXTCAP_SRIOV,	"Single Root IO Virtualization",
	  pci_conf_print_sriov_cap },
	{ PCI_EXTCAP_MRIOV,	"Multiple Root IO Virtualization",
	  NULL },
	{ PCI_EXTCAP_MULTICAST,	"Multicast",
	  NULL },
	{ PCI_EXTCAP_PAGE_REQ,	"Page Request",
	  pci_conf_print_page_req_cap },
	{ PCI_EXTCAP_AMD,	"Reserved for AMD",
	  NULL },
	{ PCI_EXTCAP_RESIZE_BAR,"Resizable BAR",
	  NULL },
	{ PCI_EXTCAP_DPA,	"Dynamic Power Allocation",
	  NULL },
	{ PCI_EXTCAP_TPH_REQ,	"TPH Requester",
	  pci_conf_print_tph_req_cap },
	{ PCI_EXTCAP_LTR,	"Latency Tolerance Reporting",
	  pci_conf_print_ltr_cap },
	{ PCI_EXTCAP_SEC_PCIE,	"Secondary PCI Express",
	  pci_conf_print_sec_pcie_cap },
	{ PCI_EXTCAP_PMUX,	"Protocol Multiplexing",
	  NULL },
	{ PCI_EXTCAP_PASID,	"Process Address Space ID",
	  pci_conf_print_pasid_cap },
	{ PCI_EXTCAP_LN_REQ,	"LN Requester",
	  pci_conf_print_lnr_cap },
	{ PCI_EXTCAP_DPC,	"Downstream Port Containment",
	  NULL },
	{ PCI_EXTCAP_L1PM,	"L1 PM Substates",
	  pci_conf_print_l1pm_cap },
	{ PCI_EXTCAP_PTM,	"Precision Time Management",
	  NULL },
	{ PCI_EXTCAP_MPCIE,	"M-PCIe",
	  NULL },
	{ PCI_EXTCAP_FRSQ,	"Function Reading Status Queueing",
	  NULL },
	{ PCI_EXTCAP_RTR,	"Readiness Time Reporting",
	  NULL },
	{ PCI_EXTCAP_DESIGVNDSP, "Designated Vendor-Specific",
	  NULL },
};

static int
pci_conf_find_extcap(const pcireg_t *regs, int capoff, unsigned int capid,
    int *offsetp)
{
	int off;
	pcireg_t rval;

	for (off = PCI_EXTCAPLIST_BASE;
	     off != 0;
	     off = PCI_EXTCAPLIST_NEXT(rval)) {
		rval = regs[o2i(off)];
		if (capid == PCI_EXTCAPLIST_CAP(rval)) {
			if (offsetp != NULL)
				*offsetp = off;
			return 1;
		}
	}
	return 0;
}

static void
pci_conf_print_extcaplist(
#ifdef _KERNEL
    pci_chipset_tag_t pc, pcitag_t tag,
#endif
    const pcireg_t *regs, int capoff)
{
	int off;
	pcireg_t foundcap;
	pcireg_t rval;
	bool foundtable[__arraycount(pci_extcaptab)];
	unsigned int i;

	/* Check Extended capability structure */
	off = PCI_EXTCAPLIST_BASE;
	rval = regs[o2i(off)];
	if (rval == 0xffffffff || rval == 0)
		return;

	/* Clear table */
	for (i = 0; i < __arraycount(pci_extcaptab); i++)
		foundtable[i] = false;

	/* Print extended capability register's offset and the type first */
	for (;;) {
		printf("  Extended Capability Register at 0x%02x\n", off);

		foundcap = PCI_EXTCAPLIST_CAP(rval);
		printf("    type: 0x%04x (", foundcap);
		if (foundcap < __arraycount(pci_extcaptab)) {
			printf("%s)\n", pci_extcaptab[foundcap].name);
			/* Mark as found */
			foundtable[foundcap] = true;
		} else
			printf("unknown)\n");
		printf("    version: %d\n", PCI_EXTCAPLIST_VERSION(rval));

		off = PCI_EXTCAPLIST_NEXT(rval);
		if (off == 0)
			break;
		rval = regs[o2i(off)];
	}

	/*
	 * And then, print the detail of each capability registers
	 * in capability value's order.
	 */
	for (i = 0; i < __arraycount(pci_extcaptab); i++) {
		if (foundtable[i] == false)
			continue;

		/*
		 * The type was found. Search capability list again and
		 * print all capabilities that the capabiliy type is
		 * the same.
		 */
		if (pci_conf_find_extcap(regs, capoff, i, &off) == 0)
			continue;
		rval = regs[o2i(off)];
		if ((PCI_EXTCAPLIST_VERSION(rval) <= 0)
		    || (pci_extcaptab[i].printfunc == NULL))
			continue;

		pci_extcaptab[i].printfunc(regs, capoff, off);

	}
}

/* Print the Secondary Status Register. */
static void
pci_conf_print_ssr(pcireg_t rval)
{
	pcireg_t devsel;

	printf("    Secondary status register: 0x%04x\n", rval); /* XXX bits */
	onoff("66 MHz capable", rval, __BIT(5));
	onoff("User Definable Features (UDF) support", rval, __BIT(6));
	onoff("Fast back-to-back capable", rval, __BIT(7));
	onoff("Data parity error detected", rval, __BIT(8));

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

	onoff("Signalled target abort", rval, __BIT(11));
	onoff("Received target abort", rval, __BIT(12));
	onoff("Received master abort", rval, __BIT(13));
	onoff("Received system error", rval, __BIT(14));
	onoff("Detected parity error", rval, __BIT(15));
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
	uint32_t base, limit;
	uint32_t base_h, limit_h;
	uint64_t pbase, plimit;
	int use_upper;

	/*
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

	rval = regs[o2i(PCI_BRIDGE_BUS_REG)];
	printf("    Primary bus number: 0x%02x\n",
	    PCI_BRIDGE_BUS_PRIMARY(rval));
	printf("    Secondary bus number: 0x%02x\n",
	    PCI_BRIDGE_BUS_SECONDARY(rval));
	printf("    Subordinate bus number: 0x%02x\n",
	    PCI_BRIDGE_BUS_SUBORDINATE(rval));
	printf("    Secondary bus latency timer: 0x%02x\n",
	    PCI_BRIDGE_BUS_SEC_LATTIMER(rval));

	rval = regs[o2i(PCI_BRIDGE_STATIO_REG)];
	pci_conf_print_ssr(__SHIFTOUT(rval, __BITS(31, 16)));

	/* I/O region */
	printf("    I/O region:\n");
	printf("      base register:  0x%02x\n", (rval >> 0) & 0xff);
	printf("      limit register: 0x%02x\n", (rval >> 8) & 0xff);
	if (PCI_BRIDGE_IO_32BITS(rval))
		use_upper = 1;
	else
		use_upper = 0;
	onoff("32bit I/O", rval, use_upper);
	base = (rval & PCI_BRIDGE_STATIO_IOBASE_MASK) << 8;
	limit = ((rval >> PCI_BRIDGE_STATIO_IOLIMIT_SHIFT)
	    & PCI_BRIDGE_STATIO_IOLIMIT_MASK) << 8;
	limit |= 0x00000fff;

	rval = regs[o2i(PCI_BRIDGE_IOHIGH_REG)];
	base_h = (rval >> 0) & 0xffff;
	limit_h = (rval >> 16) & 0xffff;
	printf("      base upper 16 bits register:  0x%04x\n", base_h);
	printf("      limit upper 16 bits register: 0x%04x\n", limit_h);

	if (use_upper == 1) {
		base |= base_h << 16;
		limit |= limit_h << 16;
	}
	if (base < limit) {
		if (use_upper == 1)
			printf("      range:  0x%08x-0x%08x\n", base, limit);
		else
			printf("      range:  0x%04x-0x%04x\n", base, limit);
	} else
		printf("      range:  not set\n");

	/* Non-prefetchable memory region */
	rval = regs[o2i(PCI_BRIDGE_MEMORY_REG)];
	printf("    Memory region:\n");
	printf("      base register:  0x%04x\n",
	    (rval >> 0) & 0xffff);
	printf("      limit register: 0x%04x\n",
	    (rval >> 16) & 0xffff);
	base = ((rval >> PCI_BRIDGE_MEMORY_BASE_SHIFT)
	    & PCI_BRIDGE_MEMORY_BASE_MASK) << 20;
	limit = (((rval >> PCI_BRIDGE_MEMORY_LIMIT_SHIFT)
		& PCI_BRIDGE_MEMORY_LIMIT_MASK) << 20) | 0x000fffff;
	if (base < limit)
		printf("      range:  0x%08x-0x%08x\n", base, limit);
	else
		printf("      range:  not set\n");

	/* Prefetchable memory region */
	rval = regs[o2i(PCI_BRIDGE_PREFETCHMEM_REG)];
	printf("    Prefetchable memory region:\n");
	printf("      base register:  0x%04x\n",
	    (rval >> 0) & 0xffff);
	printf("      limit register: 0x%04x\n",
	    (rval >> 16) & 0xffff);
	base_h = regs[o2i(PCI_BRIDGE_PREFETCHBASE32_REG)];
	limit_h = regs[o2i(PCI_BRIDGE_PREFETCHLIMIT32_REG)];
	printf("      base upper 32 bits register:  0x%08x\n",
	    base_h);
	printf("      limit upper 32 bits register: 0x%08x\n",
	    limit_h);
	if (PCI_BRIDGE_PREFETCHMEM_64BITS(rval))
		use_upper = 1;
	else
		use_upper = 0;
	onoff("64bit memory address", rval, use_upper);
	pbase = ((rval >> PCI_BRIDGE_PREFETCHMEM_BASE_SHIFT)
	    & PCI_BRIDGE_PREFETCHMEM_BASE_MASK) << 20;
	plimit = (((rval >> PCI_BRIDGE_PREFETCHMEM_LIMIT_SHIFT)
		& PCI_BRIDGE_PREFETCHMEM_LIMIT_MASK) << 20) | 0x000fffff;
	if (use_upper == 1) {
		pbase |= (uint64_t)base_h << 32;
		plimit |= (uint64_t)limit_h << 32;
	}
	if (pbase < plimit) {
		if (use_upper == 1)
			printf("      range:  0x%016" PRIx64 "-0x%016" PRIx64
			    "\n", pbase, plimit);
		else
			printf("      range:  0x%08x-0x%08x\n",
			    (uint32_t)pbase, (uint32_t)plimit);
	} else
		printf("      range:  not set\n");

	if (regs[o2i(PCI_COMMAND_STATUS_REG)] & PCI_STATUS_CAPLIST_SUPPORT)
		printf("    Capability list pointer: 0x%02x\n",
		    PCI_CAPLIST_PTR(regs[o2i(PCI_CAPLISTPTR_REG)]));
	else
		printf("    Reserved @ 0x34: 0x%08x\n", regs[o2i(0x34)]);

	/* XXX */
	printf("    Expansion ROM Base Address: 0x%08x\n", regs[o2i(0x38)]);

	rval = regs[o2i(PCI_INTERRUPT_REG)];
	printf("    Interrupt line: 0x%02x\n",
	    (rval >> 0) & 0xff);
	printf("    Interrupt pin: 0x%02x ",
	    (rval >> 8) & 0xff);
	switch ((rval >> 8) & 0xff) {
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
	rval = (regs[o2i(PCI_BRIDGE_CONTROL_REG)] >> PCI_BRIDGE_CONTROL_SHIFT)
	    & PCI_BRIDGE_CONTROL_MASK;
	printf("    Bridge control register: 0x%04x\n", rval); /* XXX bits */
	onoff("Parity error response", rval, 0x0001);
	onoff("Secondary SERR forwarding", rval, 0x0002);
	onoff("ISA enable", rval, 0x0004);
	onoff("VGA enable", rval, 0x0008);
	onoff("Master abort reporting", rval, 0x0020);
	onoff("Secondary bus reset", rval, 0x0040);
	onoff("Fast back-to-back capable", rval, 0x0080);
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

	/* Capability list pointer and secondary status register */
	rval = regs[o2i(PCI_CARDBUS_CAPLISTPTR_REG)];
	if (regs[o2i(PCI_COMMAND_STATUS_REG)] & PCI_STATUS_CAPLIST_SUPPORT)
		printf("    Capability list pointer: 0x%02x\n",
		    PCI_CAPLIST_PTR(rval));
	else
		printf("    Reserved @ 0x14: 0x%04x\n",
		       (pcireg_t)__SHIFTOUT(rval, __BITS(15, 0)));
	pci_conf_print_ssr(__SHIFTOUT(rval, __BITS(31, 16)));

	rval = regs[o2i(PCI_BRIDGE_BUS_REG)];
	printf("    PCI bus number: 0x%02x\n",
	    (rval >> 0) & 0xff);
	printf("    CardBus bus number: 0x%02x\n",
	    (rval >> 8) & 0xff);
	printf("    Subordinate bus number: 0x%02x\n",
	    (rval >> 16) & 0xff);
	printf("    CardBus latency timer: 0x%02x\n",
	    (rval >> 24) & 0xff);

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

	rval = regs[o2i(PCI_INTERRUPT_REG)];
	printf("    Interrupt line: 0x%02x\n",
	    (rval >> 0) & 0xff);
	printf("    Interrupt pin: 0x%02x ",
	    (rval >> 8) & 0xff);
	switch ((rval >> 8) & 0xff) {
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
	onoff("Parity error response", rval, __BIT(0));
	onoff("SERR# enable", rval, __BIT(1));
	onoff("ISA enable", rval, __BIT(2));
	onoff("VGA enable", rval, __BIT(3));
	onoff("Master abort mode", rval, __BIT(5));
	onoff("Secondary (CardBus) bus reset", rval, __BIT(6));
	onoff("Functional interrupts routed by ExCA registers", rval,
	    __BIT(7));
	onoff("Memory window 0 prefetchable", rval, __BIT(8));
	onoff("Memory window 1 prefetchable", rval, __BIT(9));
	onoff("Write posting enable", rval, __BIT(10));

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
	pcireg_t regs[o2i(PCI_EXTCONF_SIZE)];
	int off, capoff, endoff, hdrtype;
	const char *type_name;
#ifdef _KERNEL
	void (*type_printfn)(pci_chipset_tag_t, pcitag_t, const pcireg_t *,
	    int);
	int sizebars;
#else
	void (*type_printfn)(const pcireg_t *);
#endif

	printf("PCI configuration registers:\n");

	for (off = 0; off < PCI_EXTCONF_SIZE; off += 4) {
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
		type_name = "\"normal\" device";
		type_printfn = &pci_conf_print_type0;
		capoff = PCI_CAPLISTPTR_REG;
		endoff = 64;
		break;
	case 1:
		/* PCI-PCI bridge header */
		type_name = "PCI-PCI bridge";
		type_printfn = &pci_conf_print_type1;
		capoff = PCI_CAPLISTPTR_REG;
		endoff = 64;
		break;
	case 2:
		/* PCI-CardBus bridge header */
		type_name = "PCI-CardBus bridge";
		type_printfn = &pci_conf_print_type2;
		capoff = PCI_CARDBUS_CAPLISTPTR_REG;
		endoff = 72;
		break;
	default:
		type_name = NULL;
		type_printfn = 0;
		capoff = -1;
		endoff = 64;
		break;
	}
	printf("  Type %d ", hdrtype);
	if (type_name != NULL)
		printf("(%s) ", type_name);
	printf("header:\n");
	pci_conf_print_regs(regs, 16, endoff);
	printf("\n");
	if (type_printfn) {
#ifdef _KERNEL
		(*type_printfn)(pc, tag, regs, sizebars);
#else
		(*type_printfn)(regs);
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
	pci_conf_print_regs(regs, endoff, PCI_CONF_SIZE);
	printf("\n");
#ifdef _KERNEL
	if (printfn)
		(*printfn)(pc, tag, regs);
	else
		printf("    Don't know how to pretty-print device-dependent header.\n");
	printf("\n");
#endif /* _KERNEL */

	if (regs[o2i(PCI_EXTCAPLIST_BASE)] == 0xffffffff ||
	    regs[o2i(PCI_EXTCAPLIST_BASE)] == 0)
		return;

#ifdef _KERNEL
	pci_conf_print_extcaplist(pc, tag, regs, capoff);
#else
	pci_conf_print_extcaplist(regs, capoff);
#endif
	printf("\n");

	/* Extended Configuration Space, if present */
	printf("  Extended Configuration Space:\n");
	pci_conf_print_regs(regs, PCI_EXTCAPLIST_BASE, PCI_EXTCONF_SIZE);
}
#endif /* defined(__minix) && !defined(_PCI_SERVER) */

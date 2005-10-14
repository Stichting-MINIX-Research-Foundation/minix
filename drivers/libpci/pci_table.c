/*
pci_table.c

Tables with PCI vendor and device identifiers

Created:	Jan 2000 by Philip Homburg <philip@cs.vu.nl>

See the Linux PCI ID Repository <http://pciids.sourceforge.net/>.
*/

/* Changes from original Minix 2.0.4 version (2003-09-05):
 * 2003-11-30 (kjb) Minix 2.0.4 FIX.TAZ add D-Link RTL8139 (0x1186, 0x1300)
 * 2004-08-08 (asw) add Intel 82371AB (0x8086, 0x7100) 
 */

#include "../drivers.h"
#include "pci.h"
#if __minix_vmd
#include "config.h"
#endif

struct pci_vendor pci_vendor_table[]=
{
	{ 0x1000, "NCR" },
	{ 0x1002, "ATI Technologies" },
	{ 0x100B, "National Semiconductor Corporation" },
	{ 0x1013, "Cirrus Logic" },
	{ 0x1022, "Advanced Micro Devices" },
	{ 0x102B, "Matrox Graphics, Inc." },
	{ 0x1039, "Silicon Integrated Systems (SiS)" },
	{ 0x104C, "Texas Instruments" },
	{ 0x105A, "Promise Technology" },
	{ 0x10B7, "3Com Corporation" },
	{ 0x10B9, "AcerLabs (ALI)" },
	{ 0x10DE, "nVidia Corporation" },
	{ 0x10EC, "Realtek" },
	{ 0x1106, "VIA" },
	{ 0x110A, "Siemens Nixdorf AG" },
	{ 0x125D, "ESS Technology" },
	{ 0x1274, "Ensoniq" },
	{ 0x5333, "S3" },
	{ 0x8086, "Intel" },
	{ 0x9004, "Adaptec" },
	{ 0x9005, "Adaptec" },
	{ 0x0000, NULL }
};

struct pci_device pci_device_table[]=
{
	{ 0x1000, 0x0001, "NCR 53C810" },
	{ 0x1000, 0x000F, "NCR 53C875" },
	{ 0x1002, 0x4752, "ATI Rage XL PCI" },
	{ 0x100B, 0xD001, "Nat. Semi. 87410" },
	{ 0x1013, 0x00B8, "Cirrus Logic GD 5446" },
	{ 0x1013, 0x6003, "Cirrus Logic CS4614/22/24 CrystalClear" },
	{ 0x1022, 0x1100, "K8 HyperTransport Tech. Conf." },
	{ 0x1022, 0x1101, "K8 [Athlon64/Opteron] Address Map" },
	{ 0x1022, 0x1102, "K8 [Athlon64/Opteron] DRAM Controller" },
	{ 0x1022, 0x1103, "K8 [Athlon64/Opteron] Misc. Control" },
	{ 0x1022, 0x2000, "AMD Lance/PCI" },
	{ 0x1022, 0x700C, "AMD-762 CPU to PCI Bridge (SMP chipset)" },
	{ 0x1022, 0x700D, "AMD-762 CPU to PCI Bridge (AGP 4x)" },
	{ 0x1022, 0x7410, "AMD-766 PCI to ISA/LPC Bridge" },
	{ 0x1022, 0x7411, "AMD-766 EIDE Controller" },
	{ 0x102B, 0x051B, "Matrox MGA 2164W [Millennium II]" },
	{ 0x102B, 0x0525, "Matrox MGA G400 AGP" },
	{ 0x1039, 0x0008, "SiS 85C503/5513" },
	{ 0x1039, 0x0200, "SiS 5597/5598 VGA" },
	{ 0x1039, 0x0406, "SiS 85C501/2" },
	{ 0x1039, 0x5597, "SiS 5582" },
	{ 0x104C, 0xAC1C, "TI PCI1225" },
	{ 0x105A, 0x0D30, "Promise Technology 20265" },
	{ 0x10B7, 0x9058, "3Com 3c905B-Combo" },
	{ 0x10B7, 0x9805, "3Com 3c980-TX Python-T" },
	{ 0x10B9, 0x1533, "ALI M1533 ISA-bridge [Aladdin IV]" },
	{ 0x10B9, 0x1541, "ALI M1541" },
	{ 0x10B9, 0x5229, "ALI M5229 (IDE)" },
	{ 0x10B9, 0x5243, "ALI M5243" },
	{ 0x10B9, 0x7101, "ALI M7101 PMU" },
	{ 0x10DE, 0x0020, "nVidia Riva TnT [NV04]" },
	{ 0x10DE, 0x0110, "nVidia GeForce2 MX [NV11]" },
	{ 0x10EC, 0x8029, "Realtek RTL8029" },
	{ 0x10EC, 0x8139, "Realtek RTL8139" },
	{ 0x1106, 0x0305, "VIA VT8363/8365 [KT133/KM133]" },
	{ 0x1106, 0x0571, "VIA IDE controller" },
	{ 0x1106, 0x0686, "VIA VT82C686 (Apollo South Bridge)" },
	{ 0x1106, 0x1204, "K8M800 Host Bridge" },
	{ 0x1106, 0x2204, "K8M800 Host Bridge" },
	{ 0x1106, 0x3038, "VT83C572 PCI USB Controller" },
	{ 0x1106, 0x3057, "VT82C686A ACPI Power Management Controller" },
	{ 0x1106, 0x3058, "VIA AC97 Audio Controller" },
	{ 0x1106, 0x3059, "VIA AC97 Audio Controller" },
	{ 0x1106, 0x3065, "VT6102 [Rhine-II]" },
	{ 0x1106, 0x3074, "VIA VT8233" },
	{ 0x1106, 0x3099, "VIA VT8367 [KT266]" },
	{ 0x1106, 0x3104, "VIA USB 2.0" },
	{ 0x1106, 0x3108, "VIA S3 Unichrome Pro VGA Adapter" },
	{ 0x1106, 0x3149, "VIA VT6420 SATA RAID Controller" },
	{ 0x1106, 0x3204, "K8M800 Host Bridge" },
	{ 0x1106, 0x3227, "VT8237 ISA bridge" },
	{ 0x1106, 0x4204, "K8M800 Host Bridge" },
	{ 0x1106, 0x8305, "VIA VT8365 [KM133 AGP]" },
	{ 0x1106, 0xB099, "VIA VT8367 [KT266 AGP]" },
	{ 0x1106, 0xB188, "VT8237 PCI bridge" },
	{ 0x110A, 0x0005, "Siemens Nixdorf Tulip Cntlr., Power Management" },
	{ 0x1186, 0x1300, "D-Link RTL8139" },
	{ 0x125D, 0x1969, "ESS ES1969 Solo-1 Audiodrive" },
	{ 0x1274, 0x1371, "Ensoniq ES1371 [AudioPCI-97]" },
	{ 0x1274, 0x5000, "Ensoniq ES1370" },
	{ 0x1274, 0x5880, "Ensoniq CT5880 [AudioPCI]" },
	{ 0x5333, 0x8811, "S3 86c764/765 [Trio32/64/64V+]" },
	{ 0x5333, 0x883d, "S3 Virge/VX" },
	{ 0x5333, 0x88d0, "S3 Vision 964 vers 0" },
	{ 0x5333, 0x8a01, "S3 Virge/DX or /GX" },
	{ 0x8086, 0x1004, "Intel 82543GC Gigabit Ethernet Controller" },
 	{ 0x8086, 0x1029, "Intel EtherExpressPro100 ID1029" },
 	{ 0x8086, 0x1030, "Intel Corporation 82559 InBusiness 10/100" },
 	{ 0x8086, 0x1209, "Intel EtherExpressPro100 82559ER" },
 	{ 0x8086, 0x1229, "Intel EtherExpressPro100 82557/8/9" },
	{ 0x8086, 0x122D, "Intel 82437FX" },
	{ 0x8086, 0x122E, "Intel 82371FB (PIIX)" },
	{ 0x8086, 0x1230, "Intel 82371FB (IDE)" },
	{ 0x8086, 0x1237, "Intel 82441FX (440FX)" },
	{ 0x8086, 0x1250, "Intel 82439HX" },
 	{ 0x8086, 0x2449, "Intel EtherExpressPro100 82562EM" },
 	{ 0x8086, 0x244e, "Intel 82801 PCI Bridge" },
 	{ 0x8086, 0x2560, "Intel 82845G/GL[Brookdale-G]/GE/PE" },
 	{ 0x8086, 0x2561, "Intel 82845G/GL/GE/PE Host-to-AGP Bridge" },
	{ 0x8086, 0x7000, "Intel 82371SB" },
	{ 0x8086, 0x7010, "Intel 82371SB (IDE)" },
	{ 0x8086, 0x7020, "Intel 82371SB (USB)" },
 	{ 0x8086, 0x7030, "Intel 82437VX" },	/* asw 2005-03-02 */
 	{ 0x8086, 0x7100, "Intel 82371AB" },  	/* asw 2004-07-31 */
	{ 0x8086, 0x7100, "Intel 82371AB" },
	{ 0x8086, 0x7110, "Intel 82371AB (PIIX4)" },
	{ 0x8086, 0x7111, "Intel 82371AB (IDE)" },
	{ 0x8086, 0x7112, "Intel 82371AB (USB)" },
	{ 0x8086, 0x7113, "Intel 82371AB (Power)" },
 	{ 0x8086, 0x7124, "Intel 82801AA" },	/* asw 2004-11-09 */
	{ 0x8086, 0x7190, "Intel 82443BX" },
	{ 0x8086, 0x7191, "Intel 82443BX (AGP bridge)" },
	{ 0x8086, 0x7192, "Intel 82443BX (Host-to-PCI bridge)" },
	{ 0x9004, 0x8178, "Adaptec AHA-2940U/2940UW Ultra/Ultra-Wide SCSI Ctrlr" },
	{ 0x9005, 0x0080, "Adaptec AIC-7892A Ultra160/m PCI SCSI Controller" },
	{ 0x0000, 0x0000, NULL }
};

struct pci_baseclass pci_baseclass_table[]=
{
	{ 0x00, "No device class" },
	{ 0x01, "Mass storage controller" },
	{ 0x02, "Network controller" },
	{ 0x03, "Display controller" },
	{ 0x04, "Multimedia device" },
	{ 0x05, "Memory controller" },
	{ 0x06, "Bridge device" },
	{ 0x07, "Simple comm. controller" },
	{ 0x08, "Base system peripheral" },
	{ 0x09, "Input device" },
	{ 0x0A, "Docking station" },
	{ 0x0B, "Processor" },
	{ 0x0C, "Serial bus controller" },
	{ 0x0d, "Wireless controller" },
	{ 0x0e, "Intelligent I/O controller" },
	{ 0x0f, "Satellite comm. controller" },
	{ 0x10, "Encryption/decryption controller" },
	{ 0x11, "Data acquisition controller" },
	{ 0xff, "Misc. device" },

	{ 0x00, NULL }
};

/* -1 in the infclass field is a wildcard for infclass */
struct pci_subclass pci_subclass_table[]=
{
	{ 0x00, 0x01, 0x00, "VGA-compatible device" },

	{ 0x01, 0x00, 0x00, "SCSI bus controller" },
	{ 0x01, 0x01, -1,   "IDE controller" },
	{ 0x01, 0x02, 0x00, "Floppy disk controller" },
	{ 0x01, 0x03, 0x00, "IPI controller" },
	{ 0x01, 0x04, 0x00, "RAID controller" },
	{ 0x01, 0x80, 0x00, "Other mass storage controller" },

	{ 0x02, 0x00, 0x00, "Ethernet controller" },
	{ 0x02, 0x01, 0x00, "Token Ring controller" },
	{ 0x02, 0x02, 0x00, "FDDI controller" },
	{ 0x02, 0x03, 0x00, "ATM controller" },
	{ 0x02, 0x04, 0x00, "ISDN controller" },
	{ 0x02, 0x80, 0x00, "Other network controller" },

	{ 0x03, 0x00, 0x00, "VGA-compatible controller" },
	{ 0x03, 0x00, 0x01, "8514-compatible controller" },
	{ 0x03, 0x01, 0x00, "XGA controller" },
	{ 0x03, 0x02, 0x00, "3D controller" },
	{ 0x03, 0x80, 0x00, "Other display controller" },

	{ 0x04, 0x00, 0x00, "Video device" },
	{ 0x04, 0x01, 0x00, "Audio device" },
	{ 0x04, 0x02, 0x00, "Computer telephony device" },
	{ 0x04, 0x80, 0x00, "Other multimedia device" },

	{ 0x06, 0x00, 0x00, "Host bridge" },
	{ 0x06, 0x01, 0x00, "ISA bridge" },
	{ 0x06, 0x02, 0x00, "EISA bridge" },
	{ 0x06, 0x03, 0x00, "MCA bridge" },
	{ 0x06, 0x04, 0x00, "PCI-to-PCI bridge" },
	{ 0x06, 0x04, 0x01, "Subtractive decode PCI-to-PCI bridge" },
	{ 0x06, 0x05, 0x00, "PCMCIA bridge" },
	{ 0x06, 0x06, 0x00, "NuBus bridge" },
	{ 0x06, 0x07, 0x00, "CardBus bridge" },
	{ 0x06, 0x08, -1,   "RACEway bridge" },
	{ 0x06, 0x09, -1,   "Semi-transparent PCI-to-PCI bridge" },
	{ 0x06, 0x80, 0x00, "Other bridge device" },

	{ 0x0C, 0x00, 0x00, "IEEE 1394 (FireWire)" },
	{ 0x0C, 0x00, 0x10, "IEEE 1394 (OpenHCI)" },
	{ 0x0C, 0x01, 0x00, "ACCESS bus" },
	{ 0x0C, 0x02, 0x00, "SSA" },
	{ 0x0C, 0x03, 0x00, "USB (with UHC)" },
	{ 0x0C, 0x03, 0x10, "USB (with OHC)" },
	{ 0x0C, 0x03, 0x80, "USB (other host inf.)" },
	{ 0x0C, 0x03, 0xFE, "USB device" },
	{ 0x0C, 0x04, 0x00, "Fibre Channel" },
	{ 0x0C, 0x05, 0x00, "SMBus" },

	{ 0x00, 0x00, 0x00, NULL }
};

struct pci_intel_ctrl pci_intel_ctrl[]=
{
	{ 0x1022, 0x700C, },	/* AMD-762 */
	{ 0x1039, 0x0406, },	/* SiS 85C501/2 */
	{ 0x1039, 0x5597, },	/* SiS 5582 */
	{ 0x10B9, 0x1541, },	/* ALI M1541 */
	{ 0x1106, 0x0305, },	/* VIA VT8363/8365 */
	{ 0x1106, 0x3099, },	/* VIA VT8367 [KT266] */
	{ 0x1106, 0x3188, },	/* VIA */
	{ 0x1106, 0x0282, },	/* VIA */
	{ 0x1106, 0x0204, },	/* VIA VT8367 [KT266] */
	{ 0x8086, 0x122D, },	/* Intel 82437FX */
	{ 0x8086, 0x1237, }, 	/* Intel 82441FX */
	{ 0x8086, 0x1250, },	/* Intel 82439HX */
	{ 0x8086, 0x2560, },	/* Intel 82845G/GL[Brookdale-G]/GE/PE */
 	{ 0x8086, 0x7030, },	/* Intel 82437VX (asw 2005-03-02) */ 
 	{ 0x8086, 0x7100, },	/* Intel 82371AB (asw 2004-07-31) */
 	{ 0x8086, 0x7124, },	/* Intel 82801AA (asw 2004-11-09) */
	{ 0x8086, 0x7190, },	/* Intel 82443BX - AGP enabled */
	{ 0x8086, 0x7192, },	/* Intel 82443BX - AGP disabled */
	{ 0x0000, 0x0000, },
};

struct pci_isabridge pci_isabridge[]=
{
	{ 0x1022, 0x7410, 1, PCI_IB_AMD,	},	/* AMD-766 */
	{ 0x1039, 0x0008, 1, PCI_IB_SIS,	},	/* SiS 85C503/5513 */
	{ 0x10B9, 0x1533, 1, PCI_IB_PIIX,	},	/* ALI M1533 */
	{ 0x1106, 0x0686, 1, PCI_IB_VIA,	},	/* VIA VT82C686 */
	{ 0x1106, 0x3074, 1, PCI_IB_VIA,	},	/* VIA VT8233 */
	{ 0x1106, 0x3227, 1, PCI_IB_VIA,	},	/* VIA */
	{ 0x8086, 0x122E, 1, PCI_IB_PIIX,	},	/* Intel 82371FB */
	{ 0x8086, 0x7000, 1, PCI_IB_PIIX,	},	/* Intel 82371SB */
 	{ 0x8086, 0x7030, 1, PCI_IB_PIIX,	},	/* Intel 82437VX (asw 2005-03-02) */
 	{ 0x8086, 0x7100, 1, PCI_IB_PIIX,	},	/* Intel 82371AB (asw 2004-07-31) */
 	{ 0x8086, 0x7110, 1, PCI_IB_PIIX,	},	/* Intel PIIX4 */
 	{ 0x8086, 0x7124, 1, PCI_IB_PIIX,	},	/* Intel 82801AA (asw 2004-11-09) */
	{ 0x8086, 0x2641, 1, PCI_IB_PIIX,	},
	{ 0x0000, 0x0000, 0, 0, 		},
};

struct pci_pcibridge pci_pcibridge[]=
{
	{ 0x8086, 0x244e, PCI_PCIB_INTEL, },	/* Intel 82801 PCI Bridge */
	{ 0x8086, 0x2561, PCI_AGPB_INTEL, },	/* Intel 82845 AGP Bridge */
	{ 0x8086, 0x7191, PCI_AGPB_INTEL, },	/* Intel 82443BX (AGP bridge) */
	{ 0x1022, 0x700D, PCI_AGPB_INTEL, },	/* AMD-762 (AGP 4x) */
	{ 0x10B9, 0x5243, PCI_AGPB_INTEL, },	/* ALI M5243 */
	{ 0x1106, 0x8305, PCI_AGPB_VIA, },	/* VIA VT8365 [KM133 AGP] */
	{ 0x1106, 0xB188, PCI_AGPB_VIA, },	/* VT8237 PCI bridge */
	{ 0x0000, 0x0000, 0, },
};

/*
 * $PchId: pci_table.c,v 1.7 2003/09/05 10:53:22 philip Exp $
 */

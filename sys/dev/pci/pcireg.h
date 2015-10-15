/*	$NetBSD: pcireg.h,v 1.104 2015/10/02 05:22:53 msaitoh Exp $	*/

/*
 * Copyright (c) 1995, 1996, 1999, 2000
 *     Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1994, 1996 Charles M. Hannum.  All rights reserved.
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

#ifndef _DEV_PCI_PCIREG_H_
#define	_DEV_PCI_PCIREG_H_

/*
 * Standardized PCI configuration information
 *
 * XXX This is not complete.
 */

/*
 * Size of each function's configuration space.
 */

#define	PCI_CONF_SIZE			0x100
#define	PCI_EXTCONF_SIZE		0x1000

/*
 * Device identification register; contains a vendor ID and a device ID.
 */
#define	PCI_ID_REG			0x00

typedef u_int16_t pci_vendor_id_t;
typedef u_int16_t pci_product_id_t;

#define	PCI_VENDOR_SHIFT			0
#define	PCI_VENDOR_MASK				0xffff
#define	PCI_VENDOR(id) \
	    (((id) >> PCI_VENDOR_SHIFT) & PCI_VENDOR_MASK)

#define	PCI_PRODUCT_SHIFT			16
#define	PCI_PRODUCT_MASK			0xffff
#define	PCI_PRODUCT(id) \
	    (((id) >> PCI_PRODUCT_SHIFT) & PCI_PRODUCT_MASK)

#define PCI_ID_CODE(vid,pid)					\
	((((vid) & PCI_VENDOR_MASK) << PCI_VENDOR_SHIFT) |	\
	 (((pid) & PCI_PRODUCT_MASK) << PCI_PRODUCT_SHIFT))	\

/*
 * Command and status register.
 */
#define	PCI_COMMAND_STATUS_REG			0x04
#define	PCI_COMMAND_SHIFT			0
#define	PCI_COMMAND_MASK			0xffff
#define	PCI_STATUS_SHIFT			16
#define	PCI_STATUS_MASK				0xffff

#define PCI_COMMAND_STATUS_CODE(cmd,stat)			\
	((((cmd) & PCI_COMMAND_MASK) << PCI_COMMAND_SHIFT) |	\
	 (((stat) & PCI_STATUS_MASK) << PCI_STATUS_SHIFT))	\

#define	PCI_COMMAND_IO_ENABLE			0x00000001
#define	PCI_COMMAND_MEM_ENABLE			0x00000002
#define	PCI_COMMAND_MASTER_ENABLE		0x00000004
#define	PCI_COMMAND_SPECIAL_ENABLE		0x00000008
#define	PCI_COMMAND_INVALIDATE_ENABLE		0x00000010
#define	PCI_COMMAND_PALETTE_ENABLE		0x00000020
#define	PCI_COMMAND_PARITY_ENABLE		0x00000040
#define	PCI_COMMAND_STEPPING_ENABLE		0x00000080
#define	PCI_COMMAND_SERR_ENABLE			0x00000100
#define	PCI_COMMAND_BACKTOBACK_ENABLE		0x00000200
#define	PCI_COMMAND_INTERRUPT_DISABLE		0x00000400

#define	PCI_STATUS_INT_STATUS			0x00080000
#define	PCI_STATUS_CAPLIST_SUPPORT		0x00100000
#define	PCI_STATUS_66MHZ_SUPPORT		0x00200000
#define	PCI_STATUS_UDF_SUPPORT			0x00400000
#define	PCI_STATUS_BACKTOBACK_SUPPORT		0x00800000
#define	PCI_STATUS_PARITY_ERROR			0x01000000
#define	PCI_STATUS_DEVSEL_FAST			0x00000000
#define	PCI_STATUS_DEVSEL_MEDIUM		0x02000000
#define	PCI_STATUS_DEVSEL_SLOW			0x04000000
#define	PCI_STATUS_DEVSEL_MASK			0x06000000
#define	PCI_STATUS_TARGET_TARGET_ABORT		0x08000000
#define	PCI_STATUS_MASTER_TARGET_ABORT		0x10000000
#define	PCI_STATUS_MASTER_ABORT			0x20000000
#define	PCI_STATUS_SPECIAL_ERROR		0x40000000
#define	PCI_STATUS_PARITY_DETECT		0x80000000

/*
 * PCI Class and Revision Register; defines type and revision of device.
 */
#define	PCI_CLASS_REG			0x08

typedef u_int8_t pci_class_t;
typedef u_int8_t pci_subclass_t;
typedef u_int8_t pci_interface_t;
typedef u_int8_t pci_revision_t;

#define	PCI_CLASS_SHIFT				24
#define	PCI_CLASS_MASK				0xff
#define	PCI_CLASS(cr) \
	    (((cr) >> PCI_CLASS_SHIFT) & PCI_CLASS_MASK)

#define	PCI_SUBCLASS_SHIFT			16
#define	PCI_SUBCLASS_MASK			0xff
#define	PCI_SUBCLASS(cr) \
	    (((cr) >> PCI_SUBCLASS_SHIFT) & PCI_SUBCLASS_MASK)

#define	PCI_INTERFACE_SHIFT			8
#define	PCI_INTERFACE_MASK			0xff
#define	PCI_INTERFACE(cr) \
	    (((cr) >> PCI_INTERFACE_SHIFT) & PCI_INTERFACE_MASK)

#define	PCI_REVISION_SHIFT			0
#define	PCI_REVISION_MASK			0xff
#define	PCI_REVISION(cr) \
	    (((cr) >> PCI_REVISION_SHIFT) & PCI_REVISION_MASK)

#define	PCI_CLASS_CODE(mainclass, subclass, interface) \
	    ((((mainclass) & PCI_CLASS_MASK) << PCI_CLASS_SHIFT) | \
	     (((subclass) & PCI_SUBCLASS_MASK) << PCI_SUBCLASS_SHIFT) | \
	     (((interface) & PCI_INTERFACE_MASK) << PCI_INTERFACE_SHIFT))

/* base classes */
#define	PCI_CLASS_PREHISTORIC			0x00
#define	PCI_CLASS_MASS_STORAGE			0x01
#define	PCI_CLASS_NETWORK			0x02
#define	PCI_CLASS_DISPLAY			0x03
#define	PCI_CLASS_MULTIMEDIA			0x04
#define	PCI_CLASS_MEMORY			0x05
#define	PCI_CLASS_BRIDGE			0x06
#define	PCI_CLASS_COMMUNICATIONS		0x07
#define	PCI_CLASS_SYSTEM			0x08
#define	PCI_CLASS_INPUT				0x09
#define	PCI_CLASS_DOCK				0x0a
#define	PCI_CLASS_PROCESSOR			0x0b
#define	PCI_CLASS_SERIALBUS			0x0c
#define	PCI_CLASS_WIRELESS			0x0d
#define	PCI_CLASS_I2O				0x0e
#define	PCI_CLASS_SATCOM			0x0f
#define	PCI_CLASS_CRYPTO			0x10
#define	PCI_CLASS_DASP				0x11
#define	PCI_CLASS_UNDEFINED			0xff

/* 0x00 prehistoric subclasses */
#define	PCI_SUBCLASS_PREHISTORIC_MISC		0x00
#define	PCI_SUBCLASS_PREHISTORIC_VGA		0x01

/* 0x01 mass storage subclasses */
#define	PCI_SUBCLASS_MASS_STORAGE_SCSI		0x00
#define	PCI_SUBCLASS_MASS_STORAGE_IDE		0x01
#define	PCI_SUBCLASS_MASS_STORAGE_FLOPPY	0x02
#define	PCI_SUBCLASS_MASS_STORAGE_IPI		0x03
#define	PCI_SUBCLASS_MASS_STORAGE_RAID		0x04
#define	PCI_SUBCLASS_MASS_STORAGE_ATA		0x05
#define		PCI_INTERFACE_ATA_SINGLEDMA		0x20
#define		PCI_INTERFACE_ATA_CHAINEDDMA		0x30
#define	PCI_SUBCLASS_MASS_STORAGE_SATA		0x06
#define		PCI_INTERFACE_SATA_VND			0x00
#define		PCI_INTERFACE_SATA_AHCI10		0x01
#define		PCI_INTERFACE_SATA_SSBI			0x02
#define	PCI_SUBCLASS_MASS_STORAGE_SAS		0x07
#define	PCI_SUBCLASS_MASS_STORAGE_NVM		0x08
#define		PCI_INTERFACE_NVM_VND			0x00
#define		PCI_INTERFACE_NVM_NVMHCI10		0x01
#define		PCI_INTERFACE_NVM_NVME			0x02
#define	PCI_SUBCLASS_MASS_STORAGE_MISC		0x80

/* 0x02 network subclasses */
#define	PCI_SUBCLASS_NETWORK_ETHERNET		0x00
#define	PCI_SUBCLASS_NETWORK_TOKENRING		0x01
#define	PCI_SUBCLASS_NETWORK_FDDI		0x02
#define	PCI_SUBCLASS_NETWORK_ATM		0x03
#define	PCI_SUBCLASS_NETWORK_ISDN		0x04
#define	PCI_SUBCLASS_NETWORK_WORLDFIP		0x05
#define	PCI_SUBCLASS_NETWORK_PCIMGMULTICOMP	0x06
#define	PCI_SUBCLASS_NETWORK_INFINIBAND		0x07
#define	PCI_SUBCLASS_NETWORK_MISC		0x80

/* 0x03 display subclasses */
#define	PCI_SUBCLASS_DISPLAY_VGA		0x00
#define		PCI_INTERFACE_VGA_VGA			0x00
#define		PCI_INTERFACE_VGA_8514			0x01
#define	PCI_SUBCLASS_DISPLAY_XGA		0x01
#define	PCI_SUBCLASS_DISPLAY_3D			0x02
#define	PCI_SUBCLASS_DISPLAY_MISC		0x80

/* 0x04 multimedia subclasses */
#define	PCI_SUBCLASS_MULTIMEDIA_VIDEO		0x00
#define	PCI_SUBCLASS_MULTIMEDIA_AUDIO		0x01
#define	PCI_SUBCLASS_MULTIMEDIA_TELEPHONY	0x02
#define	PCI_SUBCLASS_MULTIMEDIA_HDAUDIO		0x03
#define	PCI_SUBCLASS_MULTIMEDIA_MISC		0x80

/* 0x05 memory subclasses */
#define	PCI_SUBCLASS_MEMORY_RAM			0x00
#define	PCI_SUBCLASS_MEMORY_FLASH		0x01
#define	PCI_SUBCLASS_MEMORY_MISC		0x80

/* 0x06 bridge subclasses */
#define	PCI_SUBCLASS_BRIDGE_HOST		0x00
#define	PCI_SUBCLASS_BRIDGE_ISA			0x01
#define	PCI_SUBCLASS_BRIDGE_EISA		0x02
#define	PCI_SUBCLASS_BRIDGE_MC			0x03	/* XXX _MCA */
#define	PCI_SUBCLASS_BRIDGE_PCI			0x04
#define		PCI_INTERFACE_BRIDGE_PCI_PCI		0x00
#define		PCI_INTERFACE_BRIDGE_PCI_SUBDEC		0x01
#define	PCI_SUBCLASS_BRIDGE_PCMCIA		0x05
#define	PCI_SUBCLASS_BRIDGE_NUBUS		0x06
#define	PCI_SUBCLASS_BRIDGE_CARDBUS		0x07
#define	PCI_SUBCLASS_BRIDGE_RACEWAY		0x08
		/* bit0 == 0 ? "transparent mode" : "endpoint mode" */
#define	PCI_SUBCLASS_BRIDGE_STPCI		0x09
#define		PCI_INTERFACE_STPCI_PRIMARY		0x40
#define		PCI_INTERFACE_STPCI_SECONDARY		0x80
#define	PCI_SUBCLASS_BRIDGE_INFINIBAND		0x0a
#define	PCI_SUBCLASS_BRIDGE_ADVSW		0x0b
#define		PCI_INTERFACE_ADVSW_CUSTOM		0x00
#define		PCI_INTERFACE_ADVSW_ASISIG		0x01
#define	PCI_SUBCLASS_BRIDGE_MISC		0x80

/* 0x07 communications subclasses */
#define	PCI_SUBCLASS_COMMUNICATIONS_SERIAL	0x00
#define		PCI_INTERFACE_SERIAL_XT			0x00
#define		PCI_INTERFACE_SERIAL_16450		0x01
#define		PCI_INTERFACE_SERIAL_16550		0x02
#define		PCI_INTERFACE_SERIAL_16650		0x03
#define		PCI_INTERFACE_SERIAL_16750		0x04
#define		PCI_INTERFACE_SERIAL_16850		0x05
#define		PCI_INTERFACE_SERIAL_16950		0x06
#define	PCI_SUBCLASS_COMMUNICATIONS_PARALLEL	0x01
#define		PCI_INTERFACE_PARALLEL			0x00
#define		PCI_INTERFACE_PARALLEL_BIDIRECTIONAL	0x01
#define		PCI_INTERFACE_PARALLEL_ECP1X		0x02
#define		PCI_INTERFACE_PARALLEL_IEEE1284_CNTRL	0x03
#define		PCI_INTERFACE_PARALLEL_IEEE1284_TGT	0xfe
#define	PCI_SUBCLASS_COMMUNICATIONS_MPSERIAL	0x02
#define	PCI_SUBCLASS_COMMUNICATIONS_MODEM	0x03
#define		PCI_INTERFACE_MODEM			0x00
#define		PCI_INTERFACE_MODEM_HAYES16450		0x01
#define		PCI_INTERFACE_MODEM_HAYES16550		0x02
#define		PCI_INTERFACE_MODEM_HAYES16650		0x03
#define		PCI_INTERFACE_MODEM_HAYES16750		0x04
#define	PCI_SUBCLASS_COMMUNICATIONS_GPIB	0x04
#define	PCI_SUBCLASS_COMMUNICATIONS_SMARTCARD	0x05
#define	PCI_SUBCLASS_COMMUNICATIONS_MISC	0x80

/* 0x08 system subclasses */
#define	PCI_SUBCLASS_SYSTEM_PIC			0x00
#define		PCI_INTERFACE_PIC_8259			0x00
#define		PCI_INTERFACE_PIC_ISA			0x01
#define		PCI_INTERFACE_PIC_EISA			0x02
#define		PCI_INTERFACE_PIC_IOAPIC		0x10
#define		PCI_INTERFACE_PIC_IOXAPIC		0x20
#define	PCI_SUBCLASS_SYSTEM_DMA			0x01
#define		PCI_INTERFACE_DMA_8237			0x00
#define		PCI_INTERFACE_DMA_ISA			0x01
#define		PCI_INTERFACE_DMA_EISA			0x02
#define	PCI_SUBCLASS_SYSTEM_TIMER		0x02
#define		PCI_INTERFACE_TIMER_8254		0x00
#define		PCI_INTERFACE_TIMER_ISA			0x01
#define		PCI_INTERFACE_TIMER_EISA		0x02
#define		PCI_INTERFACE_TIMER_HPET		0x03
#define	PCI_SUBCLASS_SYSTEM_RTC			0x03
#define		PCI_INTERFACE_RTC_GENERIC		0x00
#define		PCI_INTERFACE_RTC_ISA			0x01
#define	PCI_SUBCLASS_SYSTEM_PCIHOTPLUG		0x04
#define	PCI_SUBCLASS_SYSTEM_SDHC		0x05
#define	PCI_SUBCLASS_SYSTEM_IOMMU		0x06 /* or RCEC in old spec */
#define	PCI_SUBCLASS_SYSTEM_RCEC		0x07
#define	PCI_SUBCLASS_SYSTEM_MISC		0x80

/* 0x09 input subclasses */
#define	PCI_SUBCLASS_INPUT_KEYBOARD		0x00
#define	PCI_SUBCLASS_INPUT_DIGITIZER		0x01
#define	PCI_SUBCLASS_INPUT_MOUSE		0x02
#define	PCI_SUBCLASS_INPUT_SCANNER		0x03
#define	PCI_SUBCLASS_INPUT_GAMEPORT		0x04
#define		PCI_INTERFACE_GAMEPORT_GENERIC		0x00
#define		PCI_INTERFACE_GAMEPORT_LEGACY		0x10
#define	PCI_SUBCLASS_INPUT_MISC			0x80

/* 0x0a dock subclasses */
#define	PCI_SUBCLASS_DOCK_GENERIC		0x00
#define	PCI_SUBCLASS_DOCK_MISC			0x80

/* 0x0b processor subclasses */
#define	PCI_SUBCLASS_PROCESSOR_386		0x00
#define	PCI_SUBCLASS_PROCESSOR_486		0x01
#define	PCI_SUBCLASS_PROCESSOR_PENTIUM		0x02
#define	PCI_SUBCLASS_PROCESSOR_ALPHA		0x10
#define	PCI_SUBCLASS_PROCESSOR_POWERPC		0x20
#define	PCI_SUBCLASS_PROCESSOR_MIPS		0x30
#define	PCI_SUBCLASS_PROCESSOR_COPROC		0x40
#define	PCI_SUBCLASS_PROCESSOR_MISC		0x80

/* 0x0c serial bus subclasses */
#define	PCI_SUBCLASS_SERIALBUS_FIREWIRE		0x00
#define		PCI_INTERFACE_IEEE1394_FIREWIRE		0x00
#define		PCI_INTERFACE_IEEE1394_OPENHCI		0x10
#define	PCI_SUBCLASS_SERIALBUS_ACCESS		0x01
#define	PCI_SUBCLASS_SERIALBUS_SSA		0x02
#define	PCI_SUBCLASS_SERIALBUS_USB		0x03
#define		PCI_INTERFACE_USB_UHCI			0x00
#define		PCI_INTERFACE_USB_OHCI			0x10
#define		PCI_INTERFACE_USB_EHCI			0x20
#define		PCI_INTERFACE_USB_XHCI			0x30
#define		PCI_INTERFACE_USB_OTHERHC		0x80
#define		PCI_INTERFACE_USB_DEVICE		0xfe
#define	PCI_SUBCLASS_SERIALBUS_FIBER		0x04	/* XXX _FIBRECHANNEL */
#define	PCI_SUBCLASS_SERIALBUS_SMBUS		0x05
#define	PCI_SUBCLASS_SERIALBUS_INFINIBAND	0x06	/* Deprecated */
#define	PCI_SUBCLASS_SERIALBUS_IPMI		0x07
#define		PCI_INTERFACE_IPMI_SMIC			0x00
#define		PCI_INTERFACE_IPMI_KBD			0x01
#define		PCI_INTERFACE_IPMI_BLOCKXFER		0x02
#define	PCI_SUBCLASS_SERIALBUS_SERCOS		0x08
#define	PCI_SUBCLASS_SERIALBUS_CANBUS		0x09
#define	PCI_SUBCLASS_SERIALBUS_MISC		0x80

/* 0x0d wireless subclasses */
#define	PCI_SUBCLASS_WIRELESS_IRDA		0x00
#define	PCI_SUBCLASS_WIRELESS_CONSUMERIR	0x01
#define		PCI_INTERFACE_CONSUMERIR		0x00
#define		PCI_INTERFACE_UWB			0x10
#define	PCI_SUBCLASS_WIRELESS_RF		0x10
#define	PCI_SUBCLASS_WIRELESS_BLUETOOTH		0x11
#define	PCI_SUBCLASS_WIRELESS_BROADBAND		0x12
#define	PCI_SUBCLASS_WIRELESS_802_11A		0x20
#define	PCI_SUBCLASS_WIRELESS_802_11B		0x21
#define	PCI_SUBCLASS_WIRELESS_MISC		0x80

/* 0x0e I2O (Intelligent I/O) subclasses */
#define	PCI_SUBCLASS_I2O_STANDARD		0x00
#define		PCI_INTERFACE_I2O_FIFOAT40		0x00
		/* others for I2O spec */
#define	PCI_SUBCLASS_I2O_MISC			0x80

/* 0x0f satellite communication subclasses */
/*	PCI_SUBCLASS_SATCOM_???			0x00	/ * XXX ??? */
#define	PCI_SUBCLASS_SATCOM_TV			0x01
#define	PCI_SUBCLASS_SATCOM_AUDIO		0x02
#define	PCI_SUBCLASS_SATCOM_VOICE		0x03
#define	PCI_SUBCLASS_SATCOM_DATA		0x04
#define	PCI_SUBCLASS_SATCOM_MISC		0x80

/* 0x10 encryption/decryption subclasses */
#define	PCI_SUBCLASS_CRYPTO_NETCOMP		0x00
#define	PCI_SUBCLASS_CRYPTO_ENTERTAINMENT	0x10
#define	PCI_SUBCLASS_CRYPTO_MISC		0x80

/* 0x11 data acquisition and signal processing subclasses */
#define	PCI_SUBCLASS_DASP_DPIO			0x00
#define	PCI_SUBCLASS_DASP_TIMEFREQ		0x01 /* performance counters */
#define	PCI_SUBCLASS_DASP_SYNC			0x10
#define	PCI_SUBCLASS_DASP_MGMT			0x20
#define	PCI_SUBCLASS_DASP_MISC			0x80

/*
 * PCI BIST/Header Type/Latency Timer/Cache Line Size Register.
 */
#define	PCI_BHLC_REG			0x0c

#define	PCI_BIST_SHIFT				24
#define	PCI_BIST_MASK				0xff
#define	PCI_BIST(bhlcr) \
	    (((bhlcr) >> PCI_BIST_SHIFT) & PCI_BIST_MASK)

#define	PCI_HDRTYPE_SHIFT			16
#define	PCI_HDRTYPE_MASK			0xff
#define	PCI_HDRTYPE(bhlcr) \
	    (((bhlcr) >> PCI_HDRTYPE_SHIFT) & PCI_HDRTYPE_MASK)

#define	PCI_HDRTYPE_TYPE(bhlcr) \
	    (PCI_HDRTYPE(bhlcr) & 0x7f)
#define	PCI_HDRTYPE_MULTIFN(bhlcr) \
	    ((PCI_HDRTYPE(bhlcr) & 0x80) != 0)

#define	PCI_LATTIMER_SHIFT			8
#define	PCI_LATTIMER_MASK			0xff
#define	PCI_LATTIMER(bhlcr) \
	    (((bhlcr) >> PCI_LATTIMER_SHIFT) & PCI_LATTIMER_MASK)

#define	PCI_CACHELINE_SHIFT			0
#define	PCI_CACHELINE_MASK			0xff
#define	PCI_CACHELINE(bhlcr) \
	    (((bhlcr) >> PCI_CACHELINE_SHIFT) & PCI_CACHELINE_MASK)

#define PCI_BHLC_CODE(bist,type,multi,latency,cacheline)		\
	    ((((bist) & PCI_BIST_MASK) << PCI_BIST_SHIFT) |		\
	     (((type) & PCI_HDRTYPE_MASK) << PCI_HDRTYPE_SHIFT) |	\
	     (((multi)?0x80:0) << PCI_HDRTYPE_SHIFT) |			\
	     (((latency) & PCI_LATTIMER_MASK) << PCI_LATTIMER_SHIFT) |	\
	     (((cacheline) & PCI_CACHELINE_MASK) << PCI_CACHELINE_SHIFT))

/*
 * PCI header type
 */
#define PCI_HDRTYPE_DEVICE	0	/* PCI/PCIX/Cardbus */
#define PCI_HDRTYPE_PPB		1	/* PCI/PCIX/Cardbus */
#define PCI_HDRTYPE_PCB		2	/* PCI/PCIX/Cardbus */
#define PCI_HDRTYPE_EP		0	/* PCI Express */
#define PCI_HDRTYPE_RC		1	/* PCI Express */


/*
 * Mapping registers
 */
#define	PCI_MAPREG_START		0x10
#define	PCI_MAPREG_END			0x28
#define	PCI_MAPREG_ROM			0x30
#define	PCI_MAPREG_PPB_END		0x18
#define	PCI_MAPREG_PCB_END		0x14

#define PCI_BAR0		0x10
#define PCI_BAR1		0x14
#define PCI_BAR2		0x18
#define PCI_BAR3		0x1C
#define PCI_BAR4		0x20
#define PCI_BAR5		0x24

#define	PCI_BAR(__n)		(PCI_MAPREG_START + 4 * (__n))

#define	PCI_MAPREG_TYPE(mr)						\
	    ((mr) & PCI_MAPREG_TYPE_MASK)
#define	PCI_MAPREG_TYPE_MASK			0x00000001

#define	PCI_MAPREG_TYPE_MEM			0x00000000
#define	PCI_MAPREG_TYPE_ROM			0x00000000
#define	PCI_MAPREG_TYPE_IO			0x00000001
#define	PCI_MAPREG_ROM_ENABLE			0x00000001

#define	PCI_MAPREG_MEM_TYPE(mr)						\
	    ((mr) & PCI_MAPREG_MEM_TYPE_MASK)
#define	PCI_MAPREG_MEM_TYPE_MASK		0x00000006

#define	PCI_MAPREG_MEM_TYPE_32BIT		0x00000000
#define	PCI_MAPREG_MEM_TYPE_32BIT_1M		0x00000002
#define	PCI_MAPREG_MEM_TYPE_64BIT		0x00000004

#define	PCI_MAPREG_MEM_PREFETCHABLE(mr)				\
	    (((mr) & PCI_MAPREG_MEM_PREFETCHABLE_MASK) != 0)
#define	PCI_MAPREG_MEM_PREFETCHABLE_MASK	0x00000008

#define	PCI_MAPREG_MEM_ADDR(mr)						\
	    ((mr) & PCI_MAPREG_MEM_ADDR_MASK)
#define	PCI_MAPREG_MEM_SIZE(mr)						\
	    (PCI_MAPREG_MEM_ADDR(mr) & -PCI_MAPREG_MEM_ADDR(mr))
#define	PCI_MAPREG_MEM_ADDR_MASK		0xfffffff0

#define	PCI_MAPREG_MEM64_ADDR(mr)					\
	    ((mr) & PCI_MAPREG_MEM64_ADDR_MASK)
#define	PCI_MAPREG_MEM64_SIZE(mr)					\
	    (PCI_MAPREG_MEM64_ADDR(mr) & -PCI_MAPREG_MEM64_ADDR(mr))
#define	PCI_MAPREG_MEM64_ADDR_MASK		0xfffffffffffffff0ULL

#define	PCI_MAPREG_IO_ADDR(mr)						\
	    ((mr) & PCI_MAPREG_IO_ADDR_MASK)
#define	PCI_MAPREG_IO_SIZE(mr)						\
	    (PCI_MAPREG_IO_ADDR(mr) & -PCI_MAPREG_IO_ADDR(mr))
#define	PCI_MAPREG_IO_ADDR_MASK			0xfffffffc

#define PCI_MAPREG_SIZE_TO_MASK(size)					\
	    (-(size))

#define PCI_MAPREG_NUM(offset)						\
	    (((unsigned)(offset)-PCI_MAPREG_START)/4)


/*
 * Cardbus CIS pointer (PCI rev. 2.1)
 */
#define PCI_CARDBUS_CIS_REG 0x28

/*
 * Subsystem identification register; contains a vendor ID and a device ID.
 * Types/macros for PCI_ID_REG apply.
 * (PCI rev. 2.1)
 */
#define PCI_SUBSYS_ID_REG 0x2c

#define	PCI_SUBSYS_VENDOR_MASK	__BITS(15, 0)
#define	PCI_SUBSYS_ID_MASK		__BITS(31, 16)

#define	PCI_SUBSYS_VENDOR(__subsys_id)	\
    __SHIFTOUT(__subsys_id, PCI_SUBSYS_VENDOR_MASK)

#define	PCI_SUBSYS_ID(__subsys_id)	\
    __SHIFTOUT(__subsys_id, PCI_SUBSYS_ID_MASK)

/*
 * Capabilities link list (PCI rev. 2.2)
 */
#define	PCI_CAPLISTPTR_REG		0x34	/* header type 0 */
#define	PCI_CARDBUS_CAPLISTPTR_REG	0x14	/* header type 2 */
#define	PCI_CAPLIST_PTR(cpr)	((cpr) & 0xff)
#define	PCI_CAPLIST_NEXT(cr)	(((cr) >> 8) & 0xff)
#define	PCI_CAPLIST_CAP(cr)	((cr) & 0xff)

#define	PCI_CAP_RESERVED0	0x00
#define	PCI_CAP_PWRMGMT		0x01
#define	PCI_CAP_AGP		0x02
#define PCI_CAP_AGP_MAJOR(cr)	(((cr) >> 20) & 0xf)
#define PCI_CAP_AGP_MINOR(cr)	(((cr) >> 16) & 0xf)
#define	PCI_CAP_VPD		0x03
#define	PCI_CAP_SLOTID		0x04
#define	PCI_CAP_MSI		0x05
#define	PCI_CAP_CPCI_HOTSWAP	0x06
#define	PCI_CAP_PCIX		0x07
#define	PCI_CAP_LDT		0x08	/* HyperTransport */
#define	PCI_CAP_VENDSPEC	0x09
#define	PCI_CAP_DEBUGPORT	0x0a
#define	PCI_CAP_CPCI_RSRCCTL	0x0b
#define	PCI_CAP_HOTPLUG		0x0c
#define	PCI_CAP_SUBVENDOR	0x0d
#define	PCI_CAP_AGP8		0x0e
#define	PCI_CAP_SECURE		0x0f
#define	PCI_CAP_PCIEXPRESS     	0x10
#define	PCI_CAP_MSIX		0x11
#define	PCI_CAP_SATA		0x12
#define	PCI_CAP_PCIAF		0x13

/*
 * Capability ID: 0x01
 * Power Management Capability; access via capability pointer.
 */

/* Power Management Capability Register */
#define PCI_PMCR_SHIFT		16
#define PCI_PMCR		0x02
#define PCI_PMCR_VERSION_MASK	0x0007
#define PCI_PMCR_VERSION_10	0x0001
#define PCI_PMCR_VERSION_11	0x0002
#define PCI_PMCR_VERSION_12	0x0003
#define PCI_PMCR_PME_CLOCK	0x0008
#define PCI_PMCR_DSI		0x0020
#define PCI_PMCR_AUXCUR_MASK	0x01c0
#define PCI_PMCR_AUXCUR_0	0x0000
#define PCI_PMCR_AUXCUR_55	0x0040
#define PCI_PMCR_AUXCUR_100	0x0080
#define PCI_PMCR_AUXCUR_160	0x00c0
#define PCI_PMCR_AUXCUR_220	0x0100
#define PCI_PMCR_AUXCUR_270	0x0140
#define PCI_PMCR_AUXCUR_320	0x0180
#define PCI_PMCR_AUXCUR_375	0x01c0
#define PCI_PMCR_D1SUPP		0x0200
#define PCI_PMCR_D2SUPP		0x0400
#define PCI_PMCR_PME_D0		0x0800
#define PCI_PMCR_PME_D1		0x1000
#define PCI_PMCR_PME_D2		0x2000
#define PCI_PMCR_PME_D3HOT	0x4000
#define PCI_PMCR_PME_D3COLD	0x8000
/*
 * Power Management Control Status Register, Bridge Support Extensions Register
 * and Data Register.
 */
#define PCI_PMCSR		0x04
#define PCI_PMCSR_STATE_MASK	0x00000003
#define PCI_PMCSR_STATE_D0	0x00000000
#define PCI_PMCSR_STATE_D1	0x00000001
#define PCI_PMCSR_STATE_D2	0x00000002
#define PCI_PMCSR_STATE_D3	0x00000003
#define PCI_PMCSR_NO_SOFTRST	0x00000008
#define	PCI_PMCSR_PME_EN	0x00000100
#define PCI_PMCSR_DATASEL_MASK	0x00001e00
#define PCI_PMCSR_DATASCL_MASK	0x00006000
#define PCI_PMCSR_PME_STS	0x00008000
#define PCI_PMCSR_B2B3_SUPPORT	0x00400000
#define PCI_PMCSR_BPCC_EN	0x00800000


/*
 * Capability ID: 0x02
 * AGP
 */

/*
 * Capability ID: 0x03
 * Vital Product Data; access via capability pointer (PCI rev 2.2).
 */
#define	PCI_VPD_ADDRESS_MASK	0x7fff
#define	PCI_VPD_ADDRESS_SHIFT	16
#define	PCI_VPD_ADDRESS(ofs)	\
	(((ofs) & PCI_VPD_ADDRESS_MASK) << PCI_VPD_ADDRESS_SHIFT)
#define	PCI_VPD_DATAREG(ofs)	((ofs) + 4)
#define	PCI_VPD_OPFLAG		0x80000000

/*
 * Capability ID: 0x04
 * Slot ID
 */

/*
 * Capability ID: 0x05
 * MSI
 */

#define	PCI_MSI_CTL		0x0	/* Message Control Register offset */
#define	PCI_MSI_MADDR		0x4	/* Message Address Register (least
					 * significant bits) offset
					 */
#define	PCI_MSI_MADDR64_LO	0x4	/* 64-bit Message Address Register
					 * (least significant bits) offset
					 */
#define	PCI_MSI_MADDR64_HI	0x8	/* 64-bit Message Address Register
					 * (most significant bits) offset
					 */
#define	PCI_MSI_MDATA		0x8	/* Message Data Register offset */
#define	PCI_MSI_MDATA64		0xC	/* 64-bit Message Data Register
					 * offset
					 */
#define	PCI_MSI_MASK		0x10	/* Vector Mask register */
#define	PCI_MSI_PENDING		0x14	/* Vector Pending register */

#define	PCI_MSI_CTL_MASK	__BITS(31, 16)
#define	PCI_MSI_CTL_PERVEC_MASK	__SHIFTIN(__BIT(8), PCI_MSI_CTL_MASK)
#define	PCI_MSI_CTL_64BIT_ADDR	__SHIFTIN(__BIT(7), PCI_MSI_CTL_MASK)
#define	PCI_MSI_CTL_MME_MASK	__SHIFTIN(__BITS(6, 4), PCI_MSI_CTL_MASK)
#define	PCI_MSI_CTL_MME(reg)	__SHIFTOUT(reg, PCI_MSI_CTL_MME_MASK)
#define	PCI_MSI_CTL_MMC_MASK	__SHIFTIN(__BITS(3, 1), PCI_MSI_CTL_MASK)
#define	PCI_MSI_CTL_MMC(reg)	__SHIFTOUT(reg, PCI_MSI_CTL_MMC_MASK)
#define	PCI_MSI_CTL_MSI_ENABLE	__SHIFTIN(__BIT(0), PCI_MSI_CTL_MASK)

/*
 * MSI Message Address is at offset 4.
 * MSI Message Upper Address (if 64bit) is at offset 8.
 * MSI Message data is at offset 8 or 12 and is 16 bits.
 * [16 bit reserved field]
 * MSI Mask Bits (32 bit field)
 * MSI Pending Bits (32 bit field)
 */

 /* Max number of MSI vectors. See PCI-SIG specification. */
#define	PCI_MSI_MAX_VECTORS	32

/*
 * Capability ID: 0x07
 * PCI-X capability.
 *
 * PCI-X capability register has two different layouts. One is for bridge
 * function. Another is for non-bridge functions.
 */


/* For non-bridge functions */

/*
 * Command. 16 bits at offset 2 (e.g. upper 16 bits of the first 32-bit
 * word at the capability; the lower 16 bits are the capability ID and
 * next capability pointer).
 *
 * Since we always read PCI config space in 32-bit words, we define these
 * as 32-bit values, offset and shifted appropriately.  Make sure you perform
 * the appropriate R/M/W cycles!
 */
#define PCIX_CMD		0x00
#define PCIX_CMD_PERR_RECOVER	0x00010000
#define PCIX_CMD_RELAXED_ORDER	0x00020000
#define PCIX_CMD_BYTECNT_MASK	0x000c0000
#define	PCIX_CMD_BYTECNT_SHIFT	18
#define	PCIX_CMD_BYTECNT(reg)	\
	(512 << (((reg) & PCIX_CMD_BYTECNT_MASK) >> PCIX_CMD_BYTECNT_SHIFT))
#define		PCIX_CMD_BCNT_512	0x00000000
#define		PCIX_CMD_BCNT_1024	0x00040000
#define		PCIX_CMD_BCNT_2048	0x00080000
#define		PCIX_CMD_BCNT_4096	0x000c0000
#define PCIX_CMD_SPLTRANS_MASK	0x00700000
#define	PCIX_CMD_SPLTRANS_SHIFT	20
#define		PCIX_CMD_SPLTRANS_1	0x00000000
#define		PCIX_CMD_SPLTRANS_2	0x00100000
#define		PCIX_CMD_SPLTRANS_3	0x00200000
#define		PCIX_CMD_SPLTRANS_4	0x00300000
#define		PCIX_CMD_SPLTRANS_8	0x00400000
#define		PCIX_CMD_SPLTRANS_12	0x00500000
#define		PCIX_CMD_SPLTRANS_16	0x00600000
#define		PCIX_CMD_SPLTRANS_32	0x00700000

/*
 * Status. 32 bits at offset 4.
 */
#define PCIX_STATUS		0x04
#define PCIX_STATUS_FN_MASK	0x00000007
#define PCIX_STATUS_DEV_MASK	0x000000f8
#define PCIX_STATUS_DEV_SHIFT	3
#define PCIX_STATUS_BUS_MASK	0x0000ff00
#define PCIX_STATUS_BUS_SHIFT	8
#define PCIX_STATUS_FN(val)	((val) & PCIX_STATUS_FN_MASK)
#define PCIX_STATUS_DEV(val)	\
	(((val) & PCIX_STATUS_DEV_MASK) >> PCIX_STATUS_DEV_SHIFT)
#define PCIX_STATUS_BUS(val)	\
	(((val) & PCIX_STATUS_BUS_MASK) >> PCIX_STATUS_BUS_SHIFT)
#define PCIX_STATUS_64BIT	0x00010000	/* 64bit device */
#define PCIX_STATUS_133		0x00020000	/* 133MHz capable */
#define PCIX_STATUS_SPLDISC	0x00040000	/* Split completion discarded*/
#define PCIX_STATUS_SPLUNEX	0x00080000	/* Unexpected split complet. */
#define PCIX_STATUS_DEVCPLX	0x00100000	/* Device Complexity */
#define PCIX_STATUS_MAXB_MASK	0x00600000	/* MAX memory read Byte count*/
#define	PCIX_STATUS_MAXB_SHIFT	21
#define		PCIX_STATUS_MAXB_512	0x00000000
#define		PCIX_STATUS_MAXB_1024	0x00200000
#define		PCIX_STATUS_MAXB_2048	0x00400000
#define		PCIX_STATUS_MAXB_4096	0x00600000
#define PCIX_STATUS_MAXST_MASK	0x03800000	/* MAX outstand. Split Trans.*/
#define	PCIX_STATUS_MAXST_SHIFT	23
#define		PCIX_STATUS_MAXST_1	0x00000000
#define		PCIX_STATUS_MAXST_2	0x00800000
#define		PCIX_STATUS_MAXST_3	0x01000000
#define		PCIX_STATUS_MAXST_4	0x01800000
#define		PCIX_STATUS_MAXST_8	0x02000000
#define		PCIX_STATUS_MAXST_12	0x02800000
#define		PCIX_STATUS_MAXST_16	0x03000000
#define		PCIX_STATUS_MAXST_32	0x03800000
#define PCIX_STATUS_MAXRS_MASK	0x1c000000	/* MAX cumulative Read Size */
#define PCIX_STATUS_MAXRS_SHIFT	26
#define		PCIX_STATUS_MAXRS_1K	0x00000000
#define		PCIX_STATUS_MAXRS_2K	0x04000000
#define		PCIX_STATUS_MAXRS_4K	0x08000000
#define		PCIX_STATUS_MAXRS_8K	0x0c000000
#define		PCIX_STATUS_MAXRS_16K	0x10000000
#define		PCIX_STATUS_MAXRS_32K	0x14000000
#define		PCIX_STATUS_MAXRS_64K	0x18000000
#define		PCIX_STATUS_MAXRS_128K	0x1c000000
#define PCIX_STATUS_SCERR	0x20000000	/* rcv. Split Completion ERR.*/
#define PCIX_STATUS_266		0x40000000	/* 266MHz capable */
#define PCIX_STATUS_533		0x80000000	/* 533MHz capable */

/* For bridge function */

#define PCIX_BRIDGE_2ND_STATUS	0x00
#define PCIX_BRIDGE_ST_64BIT	0x00010000	/* Same as PCIX_STATUS (nonb)*/
#define PCIX_BRIDGE_ST_133	0x00020000	/* Same as PCIX_STATUS (nonb)*/
#define PCIX_BRIDGE_ST_SPLDISC	0x00040000	/* Same as PCIX_STATUS (nonb)*/
#define PCIX_BRIDGE_ST_SPLUNEX	0x00080000	/* Same as PCIX_STATUS (nonb)*/
#define PCIX_BRIDGE_ST_SPLOVRN	0x00100000	/* Split completion overrun */
#define PCIX_BRIDGE_ST_SPLRQDL	0x00200000	/* Split request delayed */
#define PCIX_BRIDGE_2NDST_CLKF	0x03c00000	/* Secondary clock frequency */
#define PCIX_BRIDGE_2NDST_CLKF_SHIFT 22
#define PCIX_BRIDGE_2NDST_VER_MASK 0x30000000	/* Version */
#define PCIX_BRIDGE_2NDST_VER_SHIFT 28
#define PCIX_BRIDGE_ST_266	0x40000000	/* Same as PCIX_STATUS (nonb)*/
#define PCIX_BRIDGE_ST_533	0x80000000	/* Same as PCIX_STATUS (nonb)*/

#define PCIX_BRIDGE_PRI_STATUS	0x04
/* Bit 0 to 15 are the same as PCIX_STATUS */
/* Bit 16 to 21 are the same as PCIX_BRIDGE_2ND_STATUS */
/* Bit 30 and 31 are the same as PCIX_BRIDGE_2ND_STATUS */

#define PCIX_BRIDGE_UP_STCR	0x08 /* Upstream Split Transaction Control */
#define PCIX_BRIDGE_DOWN_STCR	0x0c /* Downstream Split Transaction Control */
/* The layouts of above two registers are the same */
#define PCIX_BRIDGE_STCAP	0x0000ffff	/* Sp. Tr. Capacity */
#define PCIX_BRIDGE_STCLIM	0xffff0000	/* Sp. Tr. Commitment Limit */
#define PCIX_BRIDGE_STCLIM_SHIFT 16

/*
 * Capability ID: 0x08
 * HyperTransport
 */

#define PCI_HT_CMD	0x00	/* Capability List & Command Register */
#define	PCI_HT_CMD_MASK		__BITS(31, 16)
#define PCI_HT_MSI_ENABLED	__BIT(16)
#define PCI_HT_MSI_FIXED	__BIT(17)
#define PCI_HT_CAP(cr) ((((cr) >> 27) < 0x08) ?				      \
    (((cr) >> 27) & 0x1c) : (((cr) >> 27) & 0x1f))
#define PCI_HT_CAPMASK		__BITS(31, 27)
#define PCI_HT_CAP_SLAVE	__SHIFTIN(0b00000, PCI_HT_CAPMASK) /* 000xx */
#define PCI_HT_CAP_HOST		__SHIFTIN(0b00100, PCI_HT_CAPMASK) /* 001xx */
#define PCI_HT_CAP_SWITCH	__SHIFTIN(0b01000, PCI_HT_CAPMASK)
#define PCI_HT_CAP_INTERRUPT	__SHIFTIN(0b10000, PCI_HT_CAPMASK)
#define PCI_HT_CAP_REVID	__SHIFTIN(0b10001, PCI_HT_CAPMASK)
#define PCI_HT_CAP_UNITID_CLUMP	__SHIFTIN(0b10010, PCI_HT_CAPMASK)
#define PCI_HT_CAP_EXTCNFSPACE	__SHIFTIN(0b10011, PCI_HT_CAPMASK)
#define PCI_HT_CAP_ADDRMAP	__SHIFTIN(0b10100, PCI_HT_CAPMASK)
#define PCI_HT_CAP_MSIMAP	__SHIFTIN(0b10101, PCI_HT_CAPMASK)
#define PCI_HT_CAP_DIRECTROUTE	__SHIFTIN(0b10110, PCI_HT_CAPMASK)
#define PCI_HT_CAP_VCSET	__SHIFTIN(0b10111, PCI_HT_CAPMASK)
#define PCI_HT_CAP_RETRYMODE	__SHIFTIN(0b11000, PCI_HT_CAPMASK)
#define PCI_HT_CAP_X86ENCODE	__SHIFTIN(0b11001, PCI_HT_CAPMASK)
#define PCI_HT_CAP_GEN3		__SHIFTIN(0b11010, PCI_HT_CAPMASK)
#define PCI_HT_CAP_FLE		__SHIFTIN(0b11011, PCI_HT_CAPMASK)
#define PCI_HT_CAP_PM		__SHIFTIN(0b11100, PCI_HT_CAPMASK)
#define PCI_HT_CAP_HIGHNODECNT	__SHIFTIN(0b11101, PCI_HT_CAPMASK)

#define PCI_HT_MSI_ADDR_LO	0x04
#define PCI_HT_MSI_ADDR_HI	0x08
#define PCI_HT_MSI_FIXED_ADDR	0xfee00000UL

/*
 * Capability ID: 0x09
 * Vendor Specific
 */
#define PCI_VENDORSPECIFIC_SHIFT	16
#define PCI_VENDORSPECIFIC		0x02

/*
 * Capability ID: 0x0a
 * Debug Port
 */
#define PCI_DEBUG_BASER		0x00	/* Debug Base Register */
#define PCI_DEBUG_BASER_SHIFT	16
#define PCI_DEBUG_PORTOFF_SHIFT	16
#define	PCI_DEBUG_PORTOFF_MASK	0x1fff0000	/* Debug port offset */
#define PCI_DEBUG_BARNUM_SHIFT	29
#define	PCI_DEBUG_BARNUM_MASK	0xe0000000	/* BAR number */

/*
 * Capability ID: 0x0b
 * Compact PCI
 */

/*
 * Capability ID: 0x0c
 * Hotplug
 */

/*
 * Capability ID: 0x0d
 * Subsystem
 */
#define PCI_CAP_SUBSYS_ID 0x04
/* bit field layout is the same as PCI_SUBSYS_ID_REG's one */

/*
 * Capability ID: 0x0e
 * AGP8
 */

/*
 * Capability ID: 0x0f
 * Secure
 */

/*
 * Capability ID: 0x10
 * PCI Express; access via capability pointer.
 */
#define PCIE_XCAP	0x00	/* Capability List & Capabilities Register */
#define	PCIE_XCAP_MASK		__BITS(31, 16)
/* Capability Version */
#define PCIE_XCAP_VER_MASK	__SHIFTIN(__BITS(3, 0), PCIE_XCAP_MASK)
#define	 PCIE_XCAP_VER_1	__SHIFTIN(1, PCIE_XCAP_VER_MASK)
#define	 PCIE_XCAP_VER_2	__SHIFTIN(2, PCIE_XCAP_VER_MASK)
#define	PCIE_XCAP_TYPE_MASK	__SHIFTIN(__BITS(7, 4), PCIE_XCAP_MASK)
#define	 PCIE_XCAP_TYPE_PCIE_DEV __SHIFTIN(0x0, PCIE_XCAP_TYPE_MASK)
#define	 PCIE_XCAP_TYPE_PCI_DEV	__SHIFTIN(0x1, PCIE_XCAP_TYPE_MASK)
#define	 PCIE_XCAP_TYPE_ROOT	__SHIFTIN(0x4, PCIE_XCAP_TYPE_MASK)
#define	 PCIE_XCAP_TYPE_UP	__SHIFTIN(0x5, PCIE_XCAP_TYPE_MASK)
#define	 PCIE_XCAP_TYPE_DOWN	__SHIFTIN(0x6, PCIE_XCAP_TYPE_MASK)
#define	 PCIE_XCAP_TYPE_PCIE2PCI __SHIFTIN(0x7, PCIE_XCAP_TYPE_MASK)
#define	 PCIE_XCAP_TYPE_PCI2PCIE __SHIFTIN(0x8, PCIE_XCAP_TYPE_MASK)
#define	 PCIE_XCAP_TYPE_ROOT_INTEP __SHIFTIN(0x9, PCIE_XCAP_TYPE_MASK)
#define	 PCIE_XCAP_TYPE_ROOT_EVNTC __SHIFTIN(0xa, PCIE_XCAP_TYPE_MASK)
#define PCIE_XCAP_SI		__SHIFTIN(__BIT(8), PCIE_XCAP_MASK) /* Slot Implemented */
#define PCIE_XCAP_IRQ		__SHIFTIN(__BITS(13, 9), PCIE_XCAP_MASK)
#define PCIE_DCAP	0x04	/* Device Capabilities Register */
#define PCIE_DCAP_MAX_PAYLOAD	__BITS(2, 0)   /* Max Payload Size Supported */
#define PCIE_DCAP_PHANTOM_FUNCS	__BITS(4, 3)   /* Phantom Functions Supported*/
#define PCIE_DCAP_EXT_TAG_FIELD	__BIT(5)       /* Extended Tag Field Support */
#define PCIE_DCAP_L0S_LATENCY	__BITS(8, 6)   /* Endpoint L0 Accptbl Latency*/
#define PCIE_DCAP_L1_LATENCY	__BITS(11, 9)  /* Endpoint L1 Accptbl Latency*/
#define PCIE_DCAP_ATTN_BUTTON	__BIT(12)      /* Attention Indicator Button */
#define PCIE_DCAP_ATTN_IND	__BIT(13)      /* Attention Indicator Present*/
#define PCIE_DCAP_PWR_IND	__BIT(14)      /* Power Indicator Present */
#define PCIE_DCAP_ROLE_ERR_RPT	__BIT(15)      /* Role-Based Error Reporting */
#define PCIE_DCAP_SLOT_PWR_LIM_VAL __BITS(25, 18) /* Cap. Slot PWR Limit Val */
#define PCIE_DCAP_SLOT_PWR_LIM_SCALE __BITS(27, 26) /* Cap. SlotPWRLimit Scl */
#define PCIE_DCAP_FLR		__BIT(28)      /* Function-Level Reset Cap. */
#define PCIE_DCSR	0x08	/* Device Control & Status Register */
#define PCIE_DCSR_ENA_COR_ERR	__BIT(0)       /* Correctable Error Report En*/
#define PCIE_DCSR_ENA_NFER	__BIT(1)       /* Non-Fatal Error Report En. */
#define PCIE_DCSR_ENA_FER	__BIT(2)       /* Fatal Error Reporting Enabl*/
#define PCIE_DCSR_ENA_URR	__BIT(3)       /* Unsupported Request Rpt En */
#define PCIE_DCSR_ENA_RELAX_ORD	__BIT(4)       /* Enable Relaxed Ordering */
#define PCIE_DCSR_MAX_PAYLOAD	__BITS(7, 5)   /* Max Payload Size */
#define PCIE_DCSR_EXT_TAG_FIELD	__BIT(8)       /* Extended Tag Field Enable */
#define PCIE_DCSR_PHANTOM_FUNCS	__BIT(9)       /* Phantom Functions Enable */
#define PCIE_DCSR_AUX_POWER_PM	__BIT(10)      /* Aux Power PM Enable */
#define PCIE_DCSR_ENA_NO_SNOOP	__BIT(11)      /* Enable No Snoop */
#define PCIE_DCSR_MAX_READ_REQ	__BITS(14, 12) /* Max Read Request Size */
#define PCIE_DCSR_BRDG_CFG_RETRY __BIT(15)     /* Bridge Config Retry Enable */
#define PCIE_DCSR_INITIATE_FLR	__BIT(15)      /* Initiate Function-Level Rst*/
#define PCIE_DCSR_CED		__BIT(0 + 16)  /* Correctable Error Detected */
#define PCIE_DCSR_NFED		__BIT(1 + 16)  /* Non-Fatal Error Detected */
#define PCIE_DCSR_FED		__BIT(2 + 16)  /* Fatal Error Detected */
#define PCIE_DCSR_URD		__BIT(3 + 16)  /* Unsupported Req. Detected */
#define PCIE_DCSR_AUX_PWR	__BIT(4 + 16)  /* Aux Power Detected */
#define PCIE_DCSR_TRANSACTION_PND __BIT(5 + 16) /* Transaction Pending */
#define PCIE_LCAP	0x0c	/* Link Capabilities Register */
#define PCIE_LCAP_MAX_SPEED	__BITS(3, 0)   /* Max Link Speed */
#define PCIE_LCAP_MAX_WIDTH	__BITS(9, 4)   /* Maximum Link Width */
#define PCIE_LCAP_ASPM		__BITS(11, 10) /* Active State Link PM Supp. */
#define PCIE_LCAP_L0S_EXIT	__BITS(14, 12) /* L0s Exit Latency */
#define PCIE_LCAP_L1_EXIT	__BITS(17, 15) /* L1 Exit Latency */
#define PCIE_LCAP_CLOCK_PM	__BIT(18)      /* Clock Power Management */
#define PCIE_LCAP_SURPRISE_DOWN	__BIT(19)      /* Surprise Down Err Rpt Cap. */
#define PCIE_LCAP_DL_ACTIVE	__BIT(20)      /* Data Link Layer Link Active*/
#define PCIE_LCAP_LINK_BW_NOTIFY __BIT(21)     /* Link BW Notification Capabl*/
#define PCIE_LCAP_ASPM_COMPLIANCE __BIT(22)    /* ASPM Optionally Compliance */
#define PCIE_LCAP_PORT		__BITS(31, 24) /* Port Number */
#define PCIE_LCSR	0x10	/* Link Control & Status Register */
#define PCIE_LCSR_ASPM_L0S	__BIT(0)       /* Active State PM Control L0s*/
#define PCIE_LCSR_ASPM_L1	__BIT(1)       /* Active State PM Control L1 */
#define PCIE_LCSR_RCB		__BIT(3)       /* Read Completion Boundry Ctl*/
#define PCIE_LCSR_LINK_DIS	__BIT(4)       /* Link Disable */
#define PCIE_LCSR_RETRAIN	__BIT(5)       /* Retrain Link */
#define PCIE_LCSR_COMCLKCFG	__BIT(6)       /* Common Clock Configuration */
#define PCIE_LCSR_EXTNDSYNC	__BIT(7)       /* Extended Synch */
#define PCIE_LCSR_ENCLKPM	__BIT(8)       /* Enable Clock Power Managmt */
#define PCIE_LCSR_HAWD		__BIT(9)       /* HW Autonomous Width Disable*/
#define PCIE_LCSR_LBMIE		__BIT(10)      /* Link BW Management Intr En */
#define PCIE_LCSR_LABIE		__BIT(11)      /* Link Autonomous BW Intr En */
#define	PCIE_LCSR_LINKSPEED	__BITS(19, 16) /* Link Speed */
#define	PCIE_LCSR_NLW		__BITS(25, 20) /* Negotiated Link Width */
#define	PCIE_LCSR_LINKTRAIN_ERR	__BIT(10 + 16) /* Link Training Error */
#define	PCIE_LCSR_LINKTRAIN	__BIT(11 + 16) /* Link Training */
#define	PCIE_LCSR_SLOTCLKCFG 	__BIT(12 + 16) /* Slot Clock Configuration */
#define	PCIE_LCSR_DLACTIVE	__BIT(13 + 16) /* Data Link Layer Link Active*/
#define	PCIE_LCSR_LINK_BW_MGMT	__BIT(14 + 16) /* Link BW Management Status */
#define	PCIE_LCSR_LINK_AUTO_BW	__BIT(15 + 16) /* Link Autonomous BW Status */
#define PCIE_SLCAP	0x14	/* Slot Capabilities Register */
#define PCIE_SLCAP_ABP		__BIT(0)       /* Attention Button Present */
#define PCIE_SLCAP_PCP		__BIT(1)       /* Power Controller Present */
#define PCIE_SLCAP_MSP		__BIT(2)       /* MRL Sensor Present */
#define PCIE_SLCAP_AIP		__BIT(3)       /* Attention Indicator Present*/
#define PCIE_SLCAP_PIP		__BIT(4)       /* Power Indicator Present */
#define PCIE_SLCAP_HPS		__BIT(5)       /* Hot-Plug Surprise */
#define PCIE_SLCAP_HPC		__BIT(6)       /* Hot-Plug Capable */
#define	PCIE_SLCAP_SPLV		__BITS(14, 7)  /* Slot Power Limit Value */
#define	PCIE_SLCAP_SPLS		__BITS(16, 15) /* Slot Power Limit Scale */
#define	PCIE_SLCAP_EIP		__BIT(17)      /* Electromechanical Interlock*/
#define	PCIE_SLCAP_NCCS		__BIT(18)      /* No Command Completed Supp. */
#define	PCIE_SLCAP_PSN		__BITS(31, 19) /* Physical Slot Number */
#define PCIE_SLCSR	0x18	/* Slot Control & Status Register */
#define PCIE_SLCSR_ABE		__BIT(0)       /* Attention Button Pressed En*/
#define PCIE_SLCSR_PFE		__BIT(1)       /* Power Button Pressed Enable*/
#define PCIE_SLCSR_MSE		__BIT(2)       /* MRL Sensor Changed Enable */
#define PCIE_SLCSR_PDE		__BIT(3)       /* Presence Detect Changed Ena*/
#define PCIE_SLCSR_CCE		__BIT(4)       /* Command Completed Intr. En */
#define PCIE_SLCSR_HPE		__BIT(5)       /* Hot Plug Interrupt Enable */
#define PCIE_SLCSR_AIC		__BITS(7, 6)   /* Attention Indicator Control*/
#define PCIE_SLCSR_PIC		__BITS(9, 8)   /* Power Indicator Control */
#define PCIE_SLCSR_PCC		__BIT(10)      /* Power Controller Control */
#define PCIE_SLCSR_EIC		__BIT(11)      /* Electromechanical Interlock*/
#define PCIE_SLCSR_DLLSCE	__BIT(12)      /* DataLinkLayer State Changed*/
#define PCIE_SLCSR_ABP		__BIT(0 + 16)  /* Attention Button Pressed */
#define PCIE_SLCSR_PFD		__BIT(1 + 16)  /* Power Fault Detected */
#define PCIE_SLCSR_MSC		__BIT(2 + 16)  /* MRL Sensor Changed */
#define PCIE_SLCSR_PDC		__BIT(3 + 16)  /* Presence Detect Changed */
#define PCIE_SLCSR_CC		__BIT(4 + 16)  /* Command Completed */
#define PCIE_SLCSR_MS		__BIT(5 + 16)  /* MRL Sensor State */
#define PCIE_SLCSR_PDS		__BIT(6 + 16)  /* Presence Detect State */
#define PCIE_SLCSR_EIS		__BIT(7 + 16)  /* Electromechanical Interlock*/
#define PCIE_SLCSR_LACS		__BIT(8 + 16)  /* Data Link Layer State Chg. */
#define PCIE_RCR	0x1c	/* Root Control & Capabilities Reg. */
#define PCIE_RCR_SERR_CER	__BIT(0)       /* SERR on Correctable Err. En*/
#define PCIE_RCR_SERR_NFER	__BIT(1)       /* SERR on Non-Fatal Error En */
#define PCIE_RCR_SERR_FER	__BIT(2)       /* SERR on Fatal Error Enable */
#define PCIE_RCR_PME_IE		__BIT(3)       /* PME Interrupt Enable */
#define PCIE_RCR_CRS_SVE	__BIT(4)       /* CRS Software Visibility En */
#define PCIE_RCR_CRS_SV		__BIT(16)      /* CRS Software Visibility */
#define PCIE_RSR	0x20	/* Root Status Register */
#define PCIE_RSR_PME_REQESTER	__BITS(15, 0)  /* PME Requester ID */
#define PCIE_RSR_PME_STAT	__BIT(16)      /* PME Status */
#define PCIE_RSR_PME_PEND	__BIT(17)      /* PME Pending */
#define PCIE_DCAP2	0x24	/* Device Capabilities 2 Register */
#define PCIE_DCAP2_COMPT_RANGE	__BITS(3,0)    /* Compl. Timeout Ranges Supp */
#define PCIE_DCAP2_COMPT_DIS	__BIT(4)       /* Compl. Timeout Disable Supp*/
#define PCIE_DCAP2_ARI_FWD	__BIT(5)       /* ARI Forward Supported */
#define PCIE_DCAP2_ATOM_ROUT	__BIT(6)       /* AtomicOp Routing Supported */
#define PCIE_DCAP2_32ATOM	__BIT(7)       /* 32bit AtomicOp Compl. Supp */
#define PCIE_DCAP2_64ATOM	__BIT(8)       /* 64bit AtomicOp Compl. Supp */
#define PCIE_DCAP2_128CAS	__BIT(9)       /* 128bit Cas Completer Supp. */
#define PCIE_DCAP2_NO_ROPR_PASS	__BIT(10)      /* No RO-enabled PR-PR Passng */
#define PCIE_DCAP2_LTR_MEC	__BIT(11)      /* LTR Mechanism Supported */
#define PCIE_DCAP2_TPH_COMP	__BITS(13, 12) /* TPH Completer Supported */
#define PCIE_DCAP2_OBFF		__BITS(19, 18) /* OBPF */
#define PCIE_DCAP2_EXTFMT_FLD	__BIT(20)      /* Extended Fmt Field Support */
#define PCIE_DCAP2_EETLP_PREF	__BIT(21)      /* End-End TLP Prefix Support */
#define PCIE_DCAP2_MAX_EETLP	__BITS(23, 22) /* Max End-End TLP Prefix Sup */
#define PCIE_DCSR2	0x28	/* Device Control & Status 2 Register */
#define PCIE_DCSR2_COMPT_VAL	__BITS(3, 0)   /* Completion Timeout Value */
#define PCIE_DCSR2_COMPT_DIS	__BIT(4)       /* Completion Timeout Disable */
#define PCIE_DCSR2_ARI_FWD	__BIT(5)       /* ARI Forwarding Enable */
#define PCIE_DCSR2_ATOM_REQ	__BIT(6)       /* AtomicOp Requester Enable */
#define PCIE_DCSR2_ATOM_EBLK	__BIT(7)       /* AtomicOp Egress Blocking */
#define PCIE_DCSR2_IDO_REQ	__BIT(8)       /* IDO Request Enable */
#define PCIE_DCSR2_IDO_COMP	__BIT(9)       /* IDO Completion Enable */
#define PCIE_DCSR2_LTR_MEC	__BIT(10)      /* LTR Mechanism Enable */
#define PCIE_DCSR2_OBFF_EN	__BITS(14, 13) /* OBPF Enable */
#define PCIE_DCSR2_EETLP	__BIT(15)      /* End-End TLP Prefix Blcking */
#define PCIE_LCAP2	0x2c	/* Link Capabilities 2 Register */
#define PCIE_LCAP2_SUP_LNKSV	__BITS(7, 1)   /* Supported Link Speeds Vect */
#define PCIE_LCAP2_CROSSLNK	__BIT(8)       /* Crosslink Supported */
#define PCIE_LCSR2	0x30	/* Link Control & Status 2 Register */
#define PCIE_LCSR2_TGT_LSPEED	__BITS(3, 0)   /* Target Link Speed */
#define PCIE_LCSR2_ENT_COMPL	__BIT(4)       /* Enter Compliance */
#define PCIE_LCSR2_HW_AS_DIS	__BIT(5)       /* HW Autonomous Speed Disabl */
#define PCIE_LCSR2_SEL_DEEMP	__BIT(6)       /* Selectable De-emphasis */
#define PCIE_LCSR2_TX_MARGIN	__BITS(9, 7)   /* Transmit Margin */
#define PCIE_LCSR2_EN_MCOMP	__BIT(10)      /* Enter Modified Compliance */
#define PCIE_LCSR2_COMP_SOS	__BIT(11)      /* Compliance SOS */
#define PCIE_LCSR2_COMP_DEEMP	__BITS(15, 12) /* Compliance Present/De-emph */
#define PCIE_LCSR2_DEEMP_LVL	__BIT(0 + 16)  /* Current De-emphasis Level */
#define PCIE_LCSR2_EQ_COMPL	__BIT(1 + 16)  /* Equalization Complete */
#define PCIE_LCSR2_EQP1_SUC	__BIT(2 + 16)  /* Equaliz Phase 1 Successful */
#define PCIE_LCSR2_EQP2_SUC	__BIT(3 + 16)  /* Equaliz Phase 2 Successful */
#define PCIE_LCSR2_EQP3_SUC	__BIT(4 + 16)  /* Equaliz Phase 3 Successful */
#define PCIE_LCSR2_LNKEQ_REQ	__BIT(5 + 16)  /* Link Equalization Request */

#define PCIE_SLCAP2	0x34	/* Slot Capabilities 2 Register */
#define PCIE_SLCSR2	0x38	/* Slot Control & Status 2 Register */

/*
 * Capability ID: 0x11
 * MSIX
 */

#define PCI_MSIX_CTL	0x00
#define	PCI_MSIX_CTL_ENABLE	0x80000000
#define	PCI_MSIX_CTL_FUNCMASK	0x40000000
#define	PCI_MSIX_CTL_TBLSIZE_MASK 0x07ff0000
#define	PCI_MSIX_CTL_TBLSIZE_SHIFT 16
#define	PCI_MSIX_CTL_TBLSIZE(ofs)	((((ofs) & PCI_MSIX_CTL_TBLSIZE_MASK) \
		>> PCI_MSIX_CTL_TBLSIZE_SHIFT) + 1)
/*
 * 2nd DWORD is the Table Offset
 */
#define	PCI_MSIX_TBLOFFSET	0x04
#define	PCI_MSIX_TBLOFFSET_MASK	0xfffffff8
#define	PCI_MSIX_TBLBIR_MASK	0x00000007
/*
 * 3rd DWORD is the Pending Bitmap Array Offset
 */
#define	PCI_MSIX_PBAOFFSET	0x08
#define	PCI_MSIX_PBAOFFSET_MASK	0xfffffff8
#define	PCI_MSIX_PBABIR_MASK	0x00000007

#define PCI_MSIX_TABLE_ENTRY_SIZE	16
#define PCI_MSIX_TABLE_ENTRY_ADDR_LO	0x0
#define PCI_MSIX_TABLE_ENTRY_ADDR_HI	0x4
#define PCI_MSIX_TABLE_ENTRY_DATA	0x8
#define PCI_MSIX_TABLE_ENTRY_VECTCTL	0xc
struct pci_msix_table_entry {
	uint32_t pci_msix_addr_lo;
	uint32_t pci_msix_addr_hi;
	uint32_t pci_msix_value;
	uint32_t pci_msix_vector_control;
};
#define	PCI_MSIX_VECTCTL_HWMASK_MASK	0x00000001

 /* Max number of MSI-X vectors. See PCI-SIG specification. */
#define	PCI_MSIX_MAX_VECTORS		2048

/*
 * Capability ID: 0x12
 * SATA
 */

/*
 * Capability ID: 0x13
 * Advanced Feature
 */
#define PCI_AFCAPR		0x00	/* Capabilities */
#define	PCI_AFCAPR_MASK		__BITS(31, 24)
#define	PCI_AF_TP_CAP		__BIT(24)	/* Transaction Pending */
#define	PCI_AF_FLR_CAP		__BIT(25)	/* Function Level Reset */
#define PCI_AFCSR		0x04	/* Control & Status register */
#define PCI_AFCR_INITIATE_FLR	__BIT(0)	/* Initiate Function LVL RST */
#define PCI_AFSR_TP		__BIT(8)	/* Transaction Pending */


/*
 * Interrupt Configuration Register; contains interrupt pin and line.
 */
#define	PCI_INTERRUPT_REG		0x3c

typedef u_int8_t pci_intr_latency_t;
typedef u_int8_t pci_intr_grant_t;
typedef u_int8_t pci_intr_pin_t;
typedef u_int8_t pci_intr_line_t;

#define PCI_MAX_LAT_SHIFT			24
#define	PCI_MAX_LAT_MASK			0xff
#define	PCI_MAX_LAT(icr) \
	    (((icr) >> PCI_MAX_LAT_SHIFT) & PCI_MAX_LAT_MASK)

#define PCI_MIN_GNT_SHIFT			16
#define	PCI_MIN_GNT_MASK			0xff
#define	PCI_MIN_GNT(icr) \
	    (((icr) >> PCI_MIN_GNT_SHIFT) & PCI_MIN_GNT_MASK)

#define	PCI_INTERRUPT_GRANT_SHIFT		24
#define	PCI_INTERRUPT_GRANT_MASK		0xff
#define	PCI_INTERRUPT_GRANT(icr) \
	    (((icr) >> PCI_INTERRUPT_GRANT_SHIFT) & PCI_INTERRUPT_GRANT_MASK)

#define	PCI_INTERRUPT_LATENCY_SHIFT		16
#define	PCI_INTERRUPT_LATENCY_MASK		0xff
#define	PCI_INTERRUPT_LATENCY(icr) \
	    (((icr) >> PCI_INTERRUPT_LATENCY_SHIFT) & PCI_INTERRUPT_LATENCY_MASK)

#define	PCI_INTERRUPT_PIN_SHIFT			8
#define	PCI_INTERRUPT_PIN_MASK			0xff
#define	PCI_INTERRUPT_PIN(icr) \
	    (((icr) >> PCI_INTERRUPT_PIN_SHIFT) & PCI_INTERRUPT_PIN_MASK)

#define	PCI_INTERRUPT_LINE_SHIFT		0
#define	PCI_INTERRUPT_LINE_MASK			0xff
#define	PCI_INTERRUPT_LINE(icr) \
	    (((icr) >> PCI_INTERRUPT_LINE_SHIFT) & PCI_INTERRUPT_LINE_MASK)

#define PCI_INTERRUPT_CODE(lat,gnt,pin,line)		\
	  ((((lat)&PCI_INTERRUPT_LATENCY_MASK)<<PCI_INTERRUPT_LATENCY_SHIFT)| \
	   (((gnt)&PCI_INTERRUPT_GRANT_MASK)  <<PCI_INTERRUPT_GRANT_SHIFT)  | \
	   (((pin)&PCI_INTERRUPT_PIN_MASK)    <<PCI_INTERRUPT_PIN_SHIFT)    | \
	   (((line)&PCI_INTERRUPT_LINE_MASK)  <<PCI_INTERRUPT_LINE_SHIFT))

#define	PCI_INTERRUPT_PIN_NONE			0x00
#define	PCI_INTERRUPT_PIN_A			0x01
#define	PCI_INTERRUPT_PIN_B			0x02
#define	PCI_INTERRUPT_PIN_C			0x03
#define	PCI_INTERRUPT_PIN_D			0x04
#define	PCI_INTERRUPT_PIN_MAX			0x04

/* Header Type 1 (Bridge) configuration registers */
#define PCI_BRIDGE_BUS_REG		0x18
#define   PCI_BRIDGE_BUS_EACH_MASK		0xff
#define   PCI_BRIDGE_BUS_PRIMARY_SHIFT		0
#define   PCI_BRIDGE_BUS_SECONDARY_SHIFT	8
#define   PCI_BRIDGE_BUS_SUBORDINATE_SHIFT	16
#define   PCI_BRIDGE_BUS_SEC_LATTIMER_SHIFT	24
#define   PCI_BRIDGE_BUS_PRIMARY(reg) \
	(((reg) >> PCI_BRIDGE_BUS_PRIMARY_SHIFT) & PCI_BRIDGE_BUS_EACH_MASK)
#define   PCI_BRIDGE_BUS_SECONDARY(reg) \
	(((reg) >> PCI_BRIDGE_BUS_SECONDARY_SHIFT) & PCI_BRIDGE_BUS_EACH_MASK)
#define   PCI_BRIDGE_BUS_SUBORDINATE(reg) \
	(((reg) >> PCI_BRIDGE_BUS_SUBORDINATE_SHIFT) &PCI_BRIDGE_BUS_EACH_MASK)
#define   PCI_BRIDGE_BUS_SEC_LATTIMER(reg) \
	(((reg) >> PCI_BRIDGE_BUS_SEC_LATTIMER_SHIFT)&PCI_BRIDGE_BUS_EACH_MASK)


#define PCI_BRIDGE_STATIO_REG		0x1C
#define	  PCI_BRIDGE_STATIO_IOBASE_SHIFT	0
#define	  PCI_BRIDGE_STATIO_IOLIMIT_SHIFT	8
#define	  PCI_BRIDGE_STATIO_STATUS_SHIFT	16
#define	  PCI_BRIDGE_STATIO_IOBASE_MASK		0xf0
#define	  PCI_BRIDGE_STATIO_IOLIMIT_MASK	0xf0
#define	  PCI_BRIDGE_STATIO_STATUS_MASK		0xffff
#define	  PCI_BRIDGE_IO_32BITS(reg)		(((reg) & 0xf) == 1)

#define PCI_BRIDGE_MEMORY_REG		0x20
#define	  PCI_BRIDGE_MEMORY_BASE_SHIFT		4
#define	  PCI_BRIDGE_MEMORY_LIMIT_SHIFT		20
#define	  PCI_BRIDGE_MEMORY_BASE_MASK		0x0fff
#define	  PCI_BRIDGE_MEMORY_LIMIT_MASK		0x0fff

#define PCI_BRIDGE_PREFETCHMEM_REG	0x24
#define	  PCI_BRIDGE_PREFETCHMEM_BASE_SHIFT	4
#define	  PCI_BRIDGE_PREFETCHMEM_LIMIT_SHIFT	20
#define	  PCI_BRIDGE_PREFETCHMEM_BASE_MASK	0x0fff
#define	  PCI_BRIDGE_PREFETCHMEM_LIMIT_MASK	0x0fff
#define	  PCI_BRIDGE_PREFETCHMEM_64BITS(reg)	((reg) & 0xf)

#define PCI_BRIDGE_PREFETCHBASE32_REG	0x28
#define PCI_BRIDGE_PREFETCHLIMIT32_REG	0x2C

#define PCI_BRIDGE_IOHIGH_REG		0x30
#define	  PCI_BRIDGE_IOHIGH_BASE_SHIFT		0
#define	  PCI_BRIDGE_IOHIGH_LIMIT_SHIFT		16
#define	  PCI_BRIDGE_IOHIGH_BASE_MASK		0xffff
#define	  PCI_BRIDGE_IOHIGH_LIMIT_MASK		0xffff

#define PCI_BRIDGE_CONTROL_REG		0x3C
#define	  PCI_BRIDGE_CONTROL_SHIFT		16
#define	  PCI_BRIDGE_CONTROL_MASK		0xffff
#define   PCI_BRIDGE_CONTROL_PERE		(1 <<  0)
#define   PCI_BRIDGE_CONTROL_SERR		(1 <<  1)
#define   PCI_BRIDGE_CONTROL_ISA		(1 <<  2)
#define   PCI_BRIDGE_CONTROL_VGA		(1 <<  3)
/* Reserved					(1 <<  4) */
#define   PCI_BRIDGE_CONTROL_MABRT		(1 <<  5)
#define   PCI_BRIDGE_CONTROL_SECBR		(1 <<  6)
#define   PCI_BRIDGE_CONTROL_SECFASTB2B		(1 <<  7)
#define   PCI_BRIDGE_CONTROL_PRI_DISC_TIMER	(1 <<  8)
#define   PCI_BRIDGE_CONTROL_SEC_DISC_TIMER	(1 <<  9)
#define   PCI_BRIDGE_CONTROL_DISC_TIMER_STAT	(1 << 10)
#define   PCI_BRIDGE_CONTROL_DISC_TIMER_SERR	(1 << 11)
/* Reserved					(1 << 12) - (1 << 15) */

/*
 * Vital Product Data resource tags.
 */
struct pci_vpd_smallres {
	uint8_t		vpdres_byte0;		/* length of data + tag */
	/* Actual data. */
} __packed;

struct pci_vpd_largeres {
	uint8_t		vpdres_byte0;
	uint8_t		vpdres_len_lsb;		/* length of data only */
	uint8_t		vpdres_len_msb;
	/* Actual data. */
} __packed;

#define	PCI_VPDRES_ISLARGE(x)			((x) & 0x80)

#define	PCI_VPDRES_SMALL_LENGTH(x)		((x) & 0x7)
#define	PCI_VPDRES_SMALL_NAME(x)		(((x) >> 3) & 0xf)

#define	PCI_VPDRES_LARGE_NAME(x)		((x) & 0x7f)

#define	PCI_VPDRES_TYPE_COMPATIBLE_DEVICE_ID	0x3	/* small */
#define	PCI_VPDRES_TYPE_VENDOR_DEFINED		0xe	/* small */
#define	PCI_VPDRES_TYPE_END_TAG			0xf	/* small */

#define	PCI_VPDRES_TYPE_IDENTIFIER_STRING	0x02	/* large */
#define	PCI_VPDRES_TYPE_VPD			0x10	/* large */

struct pci_vpd {
	uint8_t		vpd_key0;
	uint8_t		vpd_key1;
	uint8_t		vpd_len;		/* length of data only */
	/* Actual data. */
} __packed;

/*
 * Recommended VPD fields:
 *
 *	PN		Part number of assembly
 *	FN		FRU part number
 *	EC		EC level of assembly
 *	MN		Manufacture ID
 *	SN		Serial Number
 *
 * Conditionally recommended VPD fields:
 *
 *	LI		Load ID
 *	RL		ROM Level
 *	RM		Alterable ROM Level
 *	NA		Network Address
 *	DD		Device Driver Level
 *	DG		Diagnostic Level
 *	LL		Loadable Microcode Level
 *	VI		Vendor ID/Device ID
 *	FU		Function Number
 *	SI		Subsystem Vendor ID/Subsystem ID
 *
 * Additional VPD fields:
 *
 *	Z0-ZZ		User/Product Specific
 */

/*
 * PCI Expansion Rom
 */

struct pci_rom_header {
	uint16_t		romh_magic;	/* 0xAA55 little endian */
	uint8_t			romh_reserved[22];
	uint16_t		romh_data_ptr;	/* pointer to pci_rom struct */
} __packed;

#define	PCI_ROM_HEADER_MAGIC	0xAA55		/* little endian */

struct pci_rom {
	uint32_t		rom_signature;
	pci_vendor_id_t		rom_vendor;
	pci_product_id_t	rom_product;
	uint16_t		rom_vpd_ptr;	/* reserved in PCI 2.2 */
	uint16_t		rom_data_len;
	uint8_t			rom_data_rev;
	pci_interface_t		rom_interface;	/* the class reg is 24-bits */
	pci_subclass_t		rom_subclass;	/* in little endian */
	pci_class_t		rom_class;
	uint16_t		rom_len;	/* code length / 512 byte */
	uint16_t		rom_rev;	/* code revision level */
	uint8_t			rom_code_type;	/* type of code */
	uint8_t			rom_indicator;
	uint16_t		rom_reserved;
	/* Actual data. */
} __packed;

#define	PCI_ROM_SIGNATURE	0x52494350	/* "PCIR", endian reversed */
#define	PCI_ROM_CODE_TYPE_X86	0		/* Intel x86 BIOS */
#define	PCI_ROM_CODE_TYPE_OFW	1		/* Open Firmware */
#define	PCI_ROM_CODE_TYPE_HPPA	2		/* HP PA/RISC */
#define	PCI_ROM_CODE_TYPE_EFI	3		/* EFI Image */

#define	PCI_ROM_INDICATOR_LAST	0x80

/*
 * Threshold below which 32bit PCI DMA needs bouncing.
 */
#define PCI32_DMA_BOUNCE_THRESHOLD	0x100000000ULL

/*
 * PCI-X 2.0/ PCI-express Extended Capability List
 */

#define	PCI_EXTCAPLIST_BASE		0x100

#define	PCI_EXTCAPLIST_CAP(ecr)		((ecr) & 0xffff)
#define	PCI_EXTCAPLIST_VERSION(ecr)	(((ecr) >> 16) & 0xf)
#define	PCI_EXTCAPLIST_NEXT(ecr)	(((ecr) >> 20) & 0xfff)

/* Extended Capability Identification Numbers */

#define	PCI_EXTCAP_AER		0x0001	/* Advanced Error Reporting */
#define	PCI_EXTCAP_VC		0x0002	/* Virtual Channel if MFVC Ext Cap not set */
#define	PCI_EXTCAP_SERNUM	0x0003	/* Device Serial Number */
#define	PCI_EXTCAP_PWRBDGT	0x0004	/* Power Budgeting */
#define	PCI_EXTCAP_RCLINK_DCL	0x0005	/* Root Complex Link Declaration */
#define	PCI_EXTCAP_RCLINK_CTL	0x0006	/* Root Complex Internal Link Control */
#define	PCI_EXTCAP_RCEC_ASSOC	0x0007	/* Root Complex Event Collector Association */
#define	PCI_EXTCAP_MFVC		0x0008	/* Multi-Function Virtual Channel */
#define	PCI_EXTCAP_VC2		0x0009	/* Virtual Channel if MFVC Ext Cap set */
#define	PCI_EXTCAP_RCRB		0x000a	/* RCRB Header */
#define	PCI_EXTCAP_VENDOR	0x000b	/* Vendor Unique */
#define	PCI_EXTCAP_CAC		0x000c	/* Configuration Access Correction -- obsolete */
#define	PCI_EXTCAP_ACS		0x000d	/* Access Control Services */
#define	PCI_EXTCAP_ARI		0x000e	/* Alternative Routing-ID Interpretation */
#define	PCI_EXTCAP_ATS		0x000f	/* Address Translation Services */
#define	PCI_EXTCAP_SRIOV	0x0010	/* Single Root IO Virtualization */
#define	PCI_EXTCAP_MRIOV	0x0011	/* Multiple Root IO Virtualization */
#define	PCI_EXTCAP_MULTICAST	0x0012	/* Multicast */
#define	PCI_EXTCAP_PAGE_REQ	0x0013	/* Page Request */
#define	PCI_EXTCAP_AMD		0x0014	/* Reserved for AMD */
#define	PCI_EXTCAP_RESIZE_BAR	0x0015	/* Resizable BAR */
#define	PCI_EXTCAP_DPA		0x0016	/* Dynamic Power Allocation */
#define	PCI_EXTCAP_TPH_REQ	0x0017	/* TPH Requester */
#define	PCI_EXTCAP_LTR		0x0018	/* Latency Tolerance Reporting */
#define	PCI_EXTCAP_SEC_PCIE	0x0019	/* Secondary PCI Express */
#define	PCI_EXTCAP_PMUX		0x001a	/* Protocol Multiplexing */
#define	PCI_EXTCAP_PASID	0x001b	/* Process Address Space ID */
#define	PCI_EXTCAP_LN_REQ	0x001c	/* LN Requester */
#define	PCI_EXTCAP_DPC		0x001d	/* Downstream Port Containment */
#define	PCI_EXTCAP_L1PM		0x001e	/* L1 PM Substates */
#define	PCI_EXTCAP_PTM		0x001f	/* Precision Time Management */
#define	PCI_EXTCAP_MPCIE	0x0020	/* M-PCIe */
#define	PCI_EXTCAP_FRSQ		0x0021	/* Function Reading Status Queueing */
#define	PCI_EXTCAP_RTR		0x0022	/* Readiness Time Reporting */
#define	PCI_EXTCAP_DESIGVNDSP	0x0023	/* Designated Vendor-Specific */

/*
 * Extended capability ID: 0x0001
 * Advanced Error Reporting
 */
#define	PCI_AER_UC_STATUS	0x04	/* Uncorrectable Error Status Register */
#define	  PCI_AER_UC_UNDEFINED			__BIT(0)
#define	  PCI_AER_UC_DL_PROTOCOL_ERROR		__BIT(4)
#define	  PCI_AER_UC_SURPRISE_DOWN_ERROR	__BIT(5)
#define	  PCI_AER_UC_POISONED_TLP		__BIT(12)
#define	  PCI_AER_UC_FC_PROTOCOL_ERROR		__BIT(13)
#define	  PCI_AER_UC_COMPLETION_TIMEOUT		__BIT(14)
#define	  PCI_AER_UC_COMPLETER_ABORT		__BIT(15)
#define	  PCI_AER_UC_UNEXPECTED_COMPLETION	__BIT(16)
#define	  PCI_AER_UC_RECEIVER_OVERFLOW		__BIT(17)
#define	  PCI_AER_UC_MALFORMED_TLP		__BIT(18)
#define	  PCI_AER_UC_ECRC_ERROR			__BIT(19)
#define	  PCI_AER_UC_UNSUPPORTED_REQUEST_ERROR	__BIT(20)
#define	  PCI_AER_UC_ACS_VIOLATION		__BIT(21)
#define	  PCI_AER_UC_INTERNAL_ERROR		__BIT(22)
#define	  PCI_AER_UC_MC_BLOCKED_TLP		__BIT(23)
#define	  PCI_AER_UC_ATOMIC_OP_EGRESS_BLOCKED	__BIT(24)
#define	  PCI_AER_UC_TLP_PREFIX_BLOCKED_ERROR	__BIT(25)
#define	PCI_AER_UC_MASK		0x08	/* Uncorrectable Error Mask Register */
	  /* Shares bits with UC_STATUS */
#define	PCI_AER_UC_SEVERITY	0x0c	/* Uncorrectable Error Severity Register */
	  /* Shares bits with UC_STATUS */
#define	PCI_AER_COR_STATUS	0x10	/* Correctable Error Status Register */
#define	  PCI_AER_COR_RECEIVER_ERROR		__BIT(0)
#define	  PCI_AER_COR_BAD_TLP			__BIT(6)
#define	  PCI_AER_COR_BAD_DLLP			__BIT(7)
#define	  PCI_AER_COR_REPLAY_NUM_ROLLOVER	__BIT(8)
#define	  PCI_AER_COR_REPLAY_TIMER_TIMEOUT	__BIT(12)
#define	  PCI_AER_COR_ADVISORY_NF_ERROR		__BIT(13)
#define	  PCI_AER_COR_INTERNAL_ERROR		__BIT(14)
#define	  PCI_AER_COR_HEADER_LOG_OVERFLOW	__BIT(15)
#define	PCI_AER_COR_MASK	0x14	/* Correctable Error Mask Register */
	  /* Shares bits with COR_STATUS */
#define	PCI_AER_CAP_CONTROL	0x18	/* Advanced Error Capabilities and Control Register */
#define	  PCI_AER_FIRST_ERROR_PTR		__BITS(4, 0)
#define	  PCI_AER_FIRST_ERROR_PTR_S		0
#define	  PCI_AER_FIRST_ERROR_PTR_M		0x1f
#define	  PCI_AER_ECRC_GEN_CAPABLE		__BIT(5)
#define	  PCI_AER_ECRC_GEN_ENABLE		__BIT(6)
#define	  PCI_AER_ECRC_CHECK_CAPABLE		__BIT(7)
#define	  PCI_AER_ECRC_CHECK_ENABLE		__BIT(8)
#define	  PCI_AER_MULT_HDR_CAPABLE		__BIT(9)
#define	  PCI_AER_MULT_HDR_ENABLE		__BIT(10)
#define	  PCI_AER_TLP_PREFIX_LOG_PRESENT	__BIT(11)
#define	PCI_AER_HEADER_LOG	0x1c	/* Header Log Register */
#define	PCI_AER_ROOTERR_CMD	0x2c	/* Root Error Command Register */
					/* Only for root complex ports */
#define	  PCI_AER_ROOTERR_COR_ENABLE		__BIT(0)
#define	  PCI_AER_ROOTERR_NF_ENABLE		__BIT(1)
#define	  PCI_AER_ROOTERR_F_ENABLE		__BIT(2)
#define	PCI_AER_ROOTERR_STATUS	0x30	/* Root Error Status Register */
					/* Only for root complex ports */
#define	  PCI_AER_ROOTERR_COR_ERR		__BIT(0)
#define	  PCI_AER_ROOTERR_MULTI_COR_ERR		__BIT(1)
#define	  PCI_AER_ROOTERR_UC_ERR		__BIT(2)
#define	  PCI_AER_ROOTERR_MULTI_UC_ERR		__BIT(3)
#define	  PCI_AER_ROOTERR_FIRST_UC_FATAL	__BIT(4)
#define	  PCI_AER_ROOTERR_NF_ERR		__BIT(5)
#define	  PCI_AER_ROOTERR_F_ERR			__BIT(6)
#define	  PCI_AER_ROOTERR_INT_MESSAGE		__BITS(31, 27)
#define	  PCI_AER_ROOTERR_INT_MESSAGE_S		27
#define	  PCI_AER_ROOTERR_INT_MESSAGE_M		0x1f
#define	PCI_AER_ERRSRC_ID	0x34	/* Error Source Identification Register */
#define	  PCI_AER_ERRSRC_ID_ERR_COR		__BITS(15, 0)
#define	  PCI_AER_ERRSRC_ID_ERR_COR_S		0
#define	  PCI_AER_ERRSRC_ID_ERR_COR_M		0xffff
#define	  PCI_AER_ERRSRC_ID_ERR_UC		__BITS(31, 16)
#define	  PCI_AER_ERRSRC_ID_ERR_UC_S		16
#define	  PCI_AER_ERRSRC_ID_ERR_UC_M		0xffff
					/* Only for root complex ports */
#define	PCI_AER_TLP_PREFIX_LOG	0x38	/*TLP Prefix Log Register */
					/* Only for TLP prefix functions */

/*
 * Extended capability ID: 0x0002, 0x0009
 * Virtual Channel
 */
#define	PCI_VC_CAP1		0x04	/* Port VC Capability Register 1 */
#define	  PCI_VC_CAP1_EXT_COUNT			__BITS(2, 0)
#define	  PCI_VC_CAP1_EXT_COUNT_S		0
#define	  PCI_VC_CAP1_EXT_COUNT_M		0x7
#define	  PCI_VC_CAP1_LOWPRI_EXT_COUNT		__BITS(6, 4)
#define	  PCI_VC_CAP1_LOWPRI_EXT_COUNT_S	4
#define	  PCI_VC_CAP1_LOWPRI_EXT_COUNT_M	0x7
#define	  PCI_VC_CAP1_REFCLK			__BITS(9, 8)
#define	  PCI_VC_CAP1_REFCLK_S			8
#define	  PCI_VC_CAP1_REFCLK_M			0x3
#define	  PCI_VC_CAP1_REFCLK_100NS		0x0
#define	  PCI_VC_CAP1_PORT_ARB_TABLE_SIZE	__BITS(11, 10)
#define	  PCI_VC_CAP1_PORT_ARB_TABLE_SIZE_S	10
#define	  PCI_VC_CAP1_PORT_ARB_TABLE_SIZE_M	0x3
#define	PCI_VC_CAP2		0x08	/* Port VC Capability Register 2 */
#define	  PCI_VC_CAP2_ARB_CAP_HW_FIXED_SCHEME	__BIT(0)
#define	  PCI_VC_CAP2_ARB_CAP_WRR_32		__BIT(1)
#define	  PCI_VC_CAP2_ARB_CAP_WRR_64		__BIT(2)
#define	  PCI_VC_CAP2_ARB_CAP_WRR_128		__BIT(3)
#define	  PCI_VC_CAP2_ARB_TABLE_OFFSET		__BITS(31, 24)
#define	  PCI_VC_CAP2_ARB_TABLE_OFFSET_S	24
#define	  PCI_VC_CAP2_ARB_TABLE_OFFSET_M	0xff
#define	PCI_VC_CONTROL		0x0c	/* Port VC Control Register (16bit) */
#define	  PCI_VC_CONTROL_LOAD_VC_ARB_TABLE	__BIT(0)
#define	  PCI_VC_CONTROL_VC_ARB_SELECT		__BITS(3, 1)
#define	  PCI_VC_CONTROL_VC_ARB_SELECT_S	1
#define	  PCI_VC_CONTROL_VC_ARB_SELECT_M	0x7
#define	PCI_VC_STATUS		0x0e	/* Port VC Status Register (16bit) */
#define	  PCI_VC_STATUS_LOAD_VC_ARB_TABLE	__BIT(0)
#define	PCI_VC_RESOURCE_CAP(n)	(0x10 + ((n) * 0x0c))	/* VC Resource Capability Register */
#define	  PCI_VC_RESOURCE_CAP_PORT_ARB_CAP_HW_FIXED_SCHEME __BIT(0)
#define	  PCI_VC_RESOURCE_CAP_PORT_ARB_CAP_WRR_32          __BIT(1)
#define	  PCI_VC_RESOURCE_CAP_PORT_ARB_CAP_WRR_64          __BIT(2)
#define	  PCI_VC_RESOURCE_CAP_PORT_ARB_CAP_WRR_128         __BIT(3)
#define	  PCI_VC_RESOURCE_CAP_PORT_ARB_CAP_TWRR_128        __BIT(4)
#define	  PCI_VC_RESOURCE_CAP_PORT_ARB_CAP_WRR_256         __BIT(5)
#define	  PCI_VC_RESOURCE_CAP_ADV_PKT_SWITCH	__BIT(14)
#define	  PCI_VC_RESOURCE_CAP_REJCT_SNOOP_TRANS	__BIT(15)
#define	  PCI_VC_RESOURCE_CAP_MAX_TIME_SLOTS	__BITS(22, 16)
#define	  PCI_VC_RESOURCE_CAP_MAX_TIME_SLOTS_S	16
#define	  PCI_VC_RESOURCE_CAP_MAX_TIME_SLOTS_M	0x7f
#define	  PCI_VC_RESOURCE_CAP_PORT_ARB_TABLE_OFFSET   __BITS(31, 24)
#define	  PCI_VC_RESOURCE_CAP_PORT_ARB_TABLE_OFFSET_S 24
#define	  PCI_VC_RESOURCE_CAP_PORT_ARB_TABLE_OFFSET_M 0xff
#define	PCI_VC_RESOURCE_CTL(n)	(0x14 + ((n) * 0x0c))	/* VC Resource Control Register */
#define	  PCI_VC_RESOURCE_CTL_TCVC_MAP		__BITS(7, 0)
#define	  PCI_VC_RESOURCE_CTL_TCVC_MAP_S	0
#define	  PCI_VC_RESOURCE_CTL_TCVC_MAP_M	0xff
#define	  PCI_VC_RESOURCE_CTL_LOAD_PORT_ARB_TABLE __BIT(16)
#define	  PCI_VC_RESOURCE_CTL_PORT_ARB_SELECT	__BITS(19, 17)
#define	  PCI_VC_RESOURCE_CTL_PORT_ARB_SELECT_S	17
#define	  PCI_VC_RESOURCE_CTL_PORT_ARB_SELECT_M	0x7
#define	  PCI_VC_RESOURCE_CTL_VC_ID		__BITS(26, 24)
#define	  PCI_VC_RESOURCE_CTL_VC_ID_S		24
#define	  PCI_VC_RESOURCE_CTL_VC_ID_M		0x7
#define	  PCI_VC_RESOURCE_CTL_VC_ENABLE		__BIT(31)
#define	PCI_VC_RESOURCE_STA(n)	(0x18 + ((n) * 0x0c))	/* VC Resource Status Register */
#define	  PCI_VC_RESOURCE_STA_PORT_ARB_TABLE	__BIT(0)
#define	  PCI_VC_RESOURCE_STA_VC_NEG_PENDING	__BIT(1)

/*
 * Extended capability ID: 0x0003
 * Serial Number
 */
#define	PCI_SERIAL_LOW		0x04
#define	PCI_SERIAL_HIGH		0x08

/*
 * Extended capability ID: 0x0004
 * Power Budgeting
 */
#define	PCI_PWRBDGT_DSEL	0x04	/* Data Select */
#define	PCI_PWRBDGT_DATA	0x08	/* Data */
#define	PCI_PWRBDGT_DATA_BASEPWR	__BITS(7, 0)	/* Base Power */
#define	PCI_PWRBDGT_DATA_SCALE		__BITS(9, 8)	/* Data Scale */
#define	PCI_PWRBDGT_PM_SUBSTAT		__BITS(12, 10)	/* PM Sub State */
#define	PCI_PWRBDGT_PM_STAT		__BITS(14, 13)	/* PM State */
#define	PCI_PWRBDGT_TYPE		__BITS(17, 15)	/* Type */
#define	PCI_PWRBDGT_PWRRAIL		__BITS(20, 18)	/* Power Rail */
#define	PCI_PWRBDGT_CAP		0x0c	/* Capability */
#define	PCI_PWRBDGT_CAP_SYSALLOC	__BIT(0)	/* System Allocated */

/*
 * Extended capability ID: 0x0005
 * Root Complex Link Declaration
 */
#define	PCI_RCLINK_DCL_ESDESC	0x04	/* Element Self Description */
#define	PCI_RCLINK_DCL_ESDESC_ELMTYPE __BITS(3, 0)	/* Element Type */
#define	PCI_RCLINK_DCL_ESDESC_NUMLINKENT __BITS(15, 8) /* Num of Link Entries*/
#define	PCI_RCLINK_DCL_ESDESC_COMPID  __BITS(23, 16)	/* Component ID */
#define	PCI_RCLINK_DCL_ESDESC_PORTNUM __BITS(31, 24)	/* Port Number */
#define	PCI_RCLINK_DCL_LINKENTS	0x10	/* Link Entries */
#define	PCI_RCLINK_DCL_LINKDESC(x)	/* Link Description */	\
	(PCI_RCLINK_DCL_LINKENTS + ((x) * 16))
#define	PCI_RCLINK_DCL_LINKDESC_LVALID	__BIT(0)	/* Link Valid */
#define	PCI_RCLINK_DCL_LINKDESC_LTYPE	__BIT(1)	/* Link Type */
#define	PCI_RCLINK_DCL_LINKDESC_ARCRBH	__BIT(2)    /* Associate RCRB Header */
#define	PCI_RCLINK_DCL_LINKDESC_TCOMPID	__BITS(23, 16) /* Target Component ID*/
#define	PCI_RCLINK_DCL_LINKDESC_TPNUM	__BITS(31, 24) /* Target Port Number */
#define	PCI_RCLINK_DCL_LINKADDR_LT0_LO(x) /* LT0: Link Address Low */	\
	(PCI_RCLINK_DCL_LINKENTS + ((x) * 16) + 0x08)
#define	PCI_RCLINK_DCL_LINKADDR_LT0_HI(x) /* LT0: Link Address High */	\
	(PCI_RCLINK_DCL_LINKENTS + ((x) * 16) + 0x0c)
#define	PCI_RCLINK_DCL_LINKADDR_LT1_LO(x) /* LT1: Config Space (low) */	\
	(PCI_RCLINK_DCL_LINKENTS + ((x) * 16) + 0x08)
#define	PCI_RCLINK_DCL_LINKADDR_LT1_N	__BITS(2, 0)	/* N */
#define	PCI_RCLINK_DCL_LINKADDR_LT1_FUNC __BITS(14, 12)	/* Function Number */
#define	PCI_RCLINK_DCL_LINKADDR_LT1_DEV	__BITS(19, 15)	/* Device Number */
#define	PCI_RCLINK_DCL_LINKADDR_LT1_BUS(N) __BITS(19 + (N), 20) /* Bus Number*/
#define	PCI_RCLINK_DCL_LINKADDR_LT1_BAL(N) __BITS(31, 20 + (N)) /* BAddr(L) */
#define	PCI_RCLINK_DCL_LINKADDR_LT1_HI(x) /* LT1: Config Space Base Addr(H) */\
	(PCI_RCLINK_DCL_LINKENTS + ((x) * 16) + 0x0c)

/*
 * Extended capability ID: 0x0006
 * Root Complex Internal Link Control
 */

/*
 * Extended capability ID: 0x0007
 * Root Complex Event Collector Association
 */
#define	PCI_RCEC_ASSOC_ASSOCBITMAP 0x04

/*
 * Extended capability ID: 0x0008
 * Multi-Function Virtual Channel
 */

/*
 * Extended capability ID: 0x0009
 * Virtual Channel if MFVC Ext Cap set
 */

/*
 * Extended capability ID: 0x000a
 * RCRB Header
 */

/*
 * Extended capability ID: 0x000b
 * Vendor Unique
 */

/*
 * Extended capability ID: 0x000c
 * Configuration Access Correction
 */

/*
 * Extended capability ID: 0x000d
 * Access Control Services
 */
#define	PCI_ACS_CAP	0x04	/* Capability Register */
#define PCI_ACS_CAP_V	__BIT(0)	/* Source Validation */
#define PCI_ACS_CAP_B	__BIT(1)	/* Transaction Blocking */
#define PCI_ACS_CAP_R	__BIT(2)	/* P2P Request Redirect */
#define PCI_ACS_CAP_C	__BIT(3)	/* P2P Completion Redirect */
#define PCI_ACS_CAP_U	__BIT(4)	/* Upstream Forwarding */
#define PCI_ACS_CAP_E	__BIT(5)	/* Egress Control */
#define PCI_ACS_CAP_T	__BIT(6)	/* Direct Translated P2P */
#define PCI_ACS_CAP_ECVSIZE __BITS(15, 8) /* Egress Control Vector Size */
#define	PCI_ACS_CTL	0x04	/* Control Register */
#define PCI_ACS_CTL_V	__BIT(0 + 16)	/* Source Validation Enable */
#define PCI_ACS_CTL_B	__BIT(1 + 16)	/* Transaction Blocking Enable */
#define PCI_ACS_CTL_R	__BIT(2 + 16)	/* P2P Request Redirect Enable */
#define PCI_ACS_CTL_C	__BIT(3 + 16)	/* P2P Completion Redirect Enable */
#define PCI_ACS_CTL_U	__BIT(4 + 16)	/* Upstream Forwarding Enable */
#define PCI_ACS_CTL_E	__BIT(5 + 16)	/* Egress Control Enable */
#define PCI_ACS_CTL_T	__BIT(6 + 16)	/* Direct Translated P2P Enable */
#define	PCI_ACS_ECV	0x08	/* Egress Control Vector */

/*
 * Extended capability ID: 0x000e
 * ARI
 */
#define PCI_ARI_CAP	0x04	/* Capability Register */
#define PCI_ARI_CAP_M		__BIT(0)	/* MFVC Function Groups Cap. */
#define PCI_ARI_CAP_A		__BIT(1)	/* ACS Function Groups Cap. */
#define PCI_ARI_CAP_NXTFN	__BITS(15, 8)	/* Next Function Number */
#define PCI_ARI_CTL	0x04	/* Control Register */
#define PCI_ARI_CTL_M		__BIT(16)	/* MFVC Function Groups Ena. */
#define PCI_ARI_CTL_A		__BIT(17)	/* ACS Function Groups Ena. */
#define PCI_ARI_CTL_FUNCGRP	__BITS(31, 24)	/* Function Group */

/*
 * Extended capability ID: 0x000f
 * Address Translation Services
 */
#define	PCI_ATS_CAP	0x04	/* Capability Register */
#define	PCI_ATS_CAP_INVQDEPTH	__BITS(4, 0)	/* Invalidate Queue Depth */
#define	PCI_ATS_CAP_PALIGNREQ	__BIT(5)	/* Page Aligned Request */
#define	PCI_ATS_CTL	0x04	/* Control Register */
#define	PCI_ATS_CTL_STU		__BITS(20, 16)	/* Smallest Translation Unit */
#define	PCI_ATS_CTL_EN		__BIT(31)	/* Enable */

/*
 * Extended capability ID: 0x0010
 * SR-IOV
 */
#define	PCI_SRIOV_CAP		0x04	/* SR-IOV Capabilities */
#define	  PCI_SRIOV_CAP_VF_MIGRATION		__BIT(0)
#define	  PCI_SRIOV_CAP_ARI_CAP_HIER_PRESERVED	__BIT(1)
#define	  PCI_SRIOV_CAP_VF_MIGRATION_INTMSG_N	__BITS(31, 21)
#define	  PCI_SRIOV_CAP_VF_MIGRATION_INTMSG_N_S	21
#define	  PCI_SRIOV_CAP_VF_MIGRATION_INTMSG_N_M	0x7ff
#define	PCI_SRIOV_CTL		0x08	/* SR-IOV Control (16bit) */
#define	  PCI_SRIOV_CTL_VF_ENABLE		__BIT(0)
#define	  PCI_SRIOV_CTL_VF_MIGRATION_SUPPORT	__BIT(1)
#define	  PCI_SRIOV_CTL_VF_MIGRATION_INT_ENABLE	__BIT(2)
#define	  PCI_SRIOV_CTL_VF_MSE			__BIT(3)
#define	  PCI_SRIOV_CTL_ARI_CAP_HIER		__BIT(4)
#define	PCI_SRIOV_STA		0x0a	/* SR-IOV Status (16bit) */
#define	  PCI_SRIOV_STA_VF_MIGRATION		__BIT(0)
#define	PCI_SRIOV_INITIAL_VFS	0x0c	/* InitialVFs (16bit) */
#define	PCI_SRIOV_TOTAL_VFS	0x0e	/* TotalVFs (16bit) */
#define	PCI_SRIOV_NUM_VFS	0x10	/* NumVFs (16bit) */
#define	PCI_SRIOV_FUNC_DEP_LINK	0x12	/* Function Dependency Link (16bit) */
#define	PCI_SRIOV_VF_OFF	0x14	/* First VF Offset (16bit) */
#define	PCI_SRIOV_VF_STRIDE	0x16	/* VF Stride (16bit) */
#define	PCI_SRIOV_VF_DID	0x1a	/* VF Device ID (16bit) */
#define	PCI_SRIOV_PAGE_CAP	0x1c	/* Supported Page Sizes */
#define	PCI_SRIOV_PAGE_SIZE	0x20	/* System Page Size */
#define	  PCI_SRIOV_BASE_PAGE_SHIFT	12
#define	PCI_SRIOV_BARS		0x24	/* VF BAR0-5 */
#define	PCI_SRIOV_BAR(x)	(PCI_SRIOV_BARS + ((x) * 4))
#define	PCI_SRIOV_VF_MIG_STA_AR	0x3c	/* VF Migration State Array Offset */
#define	  PCI_SRIOV_VF_MIG_STA_OFFSET	__BITS(31, 3)
#define	  PCI_SRIOV_VF_MIG_STA_OFFSET_S	3
#define	  PCI_SRIOV_VF_MIG_STA_OFFSET_M	0x1fffffff
#define	  PCI_SRIOV_VF_MIG_STA_BIR		__BITS(2, 0)
#define	  PCI_SRIOV_VF_MIG_STA_BIR_S		0
#define	  PCI_SRIOV_VF_MIG_STA_BIR_M		0x7

/*
 * Extended capability ID: 0x0011
 * Multiple Root IO Virtualization
 */

/*
 * Extended capability ID: 0x0012
 * Multicast
 */

/*
 * Extended capability ID: 0x0013
 * Page Request
 */
#define	PCI_PAGE_REQ_CTL	0x04	/* Control Register */
#define	PCI_PAGE_REQ_CTL_E	__BIT(0)	/* Enalbe */
#define	PCI_PAGE_REQ_CTL_R	__BIT(1)	/* Reset */
#define	PCI_PAGE_REQ_STA	0x04	/* Status Register */
#define	PCI_PAGE_REQ_STA_RF	__BIT(0+16)	/* Response Failure */
#define	PCI_PAGE_REQ_STA_UPRGI	__BIT(1+16)   /* Unexpected Page Req Grp Idx */
#define	PCI_PAGE_REQ_STA_S	__BIT(8+16)	/* Stopped */
#define	PCI_PAGE_REQ_OUTSTCAPA	0x08	/* Outstanding Page Request Capacity */
#define	PCI_PAGE_REQ_OUTSTALLOC	0x0c  /* Outstanding Page Request Allocation */

/*
 * Extended capability ID: 0x0014
 * (Reserved for AMD)
 */

/*
 * Extended capability ID: 0x0015
 * Resizable BAR
 */

/*
 * Extended capability ID: 0x0016
 * Dynamic Power Allocation
 */

/*
 * Extended capability ID: 0x0017
 * TPH Requester
 */
#define	PCI_TPH_REQ_CAP	0x04	/* TPH Requester Capability */
#define	PCI_TPH_REQ_CAP_NOST	__BIT(0)	/* No ST Mode Supported */
#define	PCI_TPH_REQ_CAP_INTVEC	__BIT(1)	/* Intr Vec Mode Supported */
#define	PCI_TPH_REQ_CAP_DEVSPEC	__BIT(2)   /* Device Specific Mode Supported */
#define	PCI_TPH_REQ_CAP_XTPHREQ	__BIT(8)    /* Extend TPH Reqester Supported */
#define	PCI_TPH_REQ_CAP_STTBLLOC __BITS(10, 9)	/* ST Table Location */
#define	PCI_TPH_REQ_CAP_STTBLSIZ __BITS(26, 16)	/* ST Table Size */
#define	PCI_TPH_REQ_CTL	0x08	/* TPH Requester Control */
#define	PCI_TPH_REQ_CTL_STSEL	_BITS(2, 0)	/* ST Mode Select */
#define	PCI_TPH_REQ_CTL_TPHREQEN _BITS(9, 8)	/* TPH Requester Enable */
#define	PCI_TPH_REQ_STTBL 0x0c	/* TPH ST Table */

/*
 * Extended capability ID: 0x0018
 * Latency Tolerance Reporting
 */
#define	PCI_LTR_MAXSNOOPLAT	0x04	/* Max Snoop Latency */
#define	PCI_LTR_MAXSNOOPLAT_VAL	__BITS(9, 0)	/* Max Snoop LatencyValue */
#define	PCI_LTR_MAXSNOOPLAT_SCALE __BITS(12, 10) /* Max Snoop LatencyScale */
#define	PCI_LTR_MAXNOSNOOPLAT	0x04	/* Max No-Snoop Latency */
#define	PCI_LTR_MAXNOSNOOPLAT_VAL __BITS(25, 16) /* Max No-Snoop LatencyValue*/
#define	PCI_LTR_MAXNOSNOOPLAT_SCALE __BITS(28, 26) /*Max NoSnoop LatencyScale*/
#define	PCI_LTR_SCALETONS(x) ((32 << (x)) / 32)

/*
 * Extended capability ID: 0x0019
 * Seconday PCI Express Extended Capability
 */
#define PCI_SECPCIE_LCTL3	0x04	/* Link Control 3 */
#define PCI_SECPCIE_LCTL3_PERFEQ	__BIT(0) /* Perform Equalization */
#define PCI_SECPCIE_LCTL3_LINKEQREQ_IE	__BIT(1) /* Link Eq. Req. Int. Ena. */
#define PCI_SECPCIE_LANEERR_STA 0x08	/* Lane Error Status */
#define PCI_SECPCIE_EQCTLS	0x0c	/* Equalization Control [0-maxlane] */
#define	PCI_SECPCIE_EQCTL(x)	(PCI_SECPCIE_EQCTLS + ((x) * 2))
#define	PCI_SECPCIE_EQCTL_DP_XMIT_PRESET __BITS(3, 0) /* DwnStPort Xmit Pres */
#define	PCI_SECPCIE_EQCTL_DP_RCV_HINT	__BITS(6, 4) /* DwnStPort Rcv PreHnt */
#define	PCI_SECPCIE_EQCTL_UP_XMIT_PRESET __BITS(11, 8) /* UpStPort Xmit Pres */
#define	PCI_SECPCIE_EQCTL_UP_RCV_HINT	__BITS(14, 12) /* UpStPort Rcv PreHnt*/

/*
 * Extended capability ID: 0x001a
 * Protocol Multiplexing
 */

/*
 * Extended capability ID: 0x001b
 * Process Address Space ID
 */
#define	PCI_PASID_CAP	0x04	/* Capability Register */
#define	PCI_PASID_CAP_XPERM	__BIT(1)     /* Execute Permission Supported */
#define	PCI_PASID_CAP_PRIVMODE	__BIT(2)	/* Privileged Mode Supported */
#define	PCI_PASID_CAP_MAXPASIDW	__BITS(12, 8)	/* Max PASID Width */
#define	PCI_PASID_CTL	0x04	/* Control Register */
#define	PCI_PASID_CTL_PASID_EN	__BIT(0)	/* PASID Enable */
#define	PCI_PASID_CTL_XPERM_EN	__BIT(1)	/* Execute Permission Enable */
#define	PCI_PASID_CTL_PRIVMODE_EN __BIT(2)	/* Privileged Mode Enable */

/*
 * Extended capability ID: 0x001c
 * LN Requester
 */
#define	PCI_LNR_CAP	0x04	/* Capability Register */
#define	PCI_LNR_CAP_64		__BIT(0)	/* LNR-64 Supported */
#define	PCI_LNR_CAP_128		__BIT(1)	/* LNR-128 Supported */
#define	PCI_LNR_CAP_REGISTMAX	__BITS(12, 8)	/* LNR Registration MAX */
#define	PCI_LNR_CTL	0x04	/* Control Register */
#define	PCI_LNR_CTL_EN		__BIT(0+16)	/* LNR Enable */
#define	PCI_LNR_CTL_CLS		__BIT(1+16)	/* LNR CLS */
#define	PCI_LNR_CTL_REGISTLIM	__BITS(28, 24)	/* LNR Registration Limit */

/*
 * Extended capability ID: 0x001d
 * Downstream Port Containment
 */

/*
 * Extended capability ID: 0x001e
 * L1 PM Substates
 */
#define	PCI_L1PM_CAP	0x04	/* Capabilities Register */
#define	PCI_L1PM_CAP_PCIPM12	__BIT(0)	/* PCI-PM L1.2 Supported */
#define	PCI_L1PM_CAP_PCIPM11	__BIT(1)	/* PCI-PM L1.1 Supported */
#define	PCI_L1PM_CAP_ASPM12	__BIT(2)	/* ASPM L1.2 Supported */
#define	PCI_L1PM_CAP_ASPM11	__BIT(3)	/* ASPM L1.1 Supported */
#define	PCI_L1PM_CAP_L1PM	__BIT(4)	/* L1 PM Substates Supported */
#define	PCI_L1PM_CAP_PCMRT	__BITS(15, 8) /*Port Common Mode Restore Time*/
#define	PCI_L1PM_CAP_PTPOSCALE	__BITS(17, 16)	/* Port T_POWER_ON Scale */
#define	PCI_L1PM_CAP_PTPOVAL	__BITS(23, 19)	/* Port T_POWER_ON Value */
#define	PCI_L1PM_CTL1	0x08	/* Control Register 1 */
#define	PCI_L1PM_CTL1_PCIPM12_EN __BIT(0)	/* PCI-PM L1.2 Enable */
#define	PCI_L1PM_CTL1_PCIPM11_EN __BIT(1)	/* PCI-PM L1.1 Enable */
#define	PCI_L1PM_CTL1_ASPM12_EN	__BIT(2)	/* ASPM L1.2 Enable */
#define	PCI_L1PM_CTL1_ASPM11_EN	__BIT(3)	/* ASPM L1.1 Enable */
#define	PCI_L1PM_CTL1_CMRT	__BITS(15, 8)	/* Common Mode Restore Time */
#define	PCI_L1PM_CTL1_LTRTHVAL	__BITS(25, 16)	/* LTR L1.2 THRESHOLD Value */
#define	PCI_L1PM_CTL1_LTRTHSCALE __BITS(31, 29)	/* LTR L1.2 THRESHOLD Scale */
#define	PCI_L1PM_CTL2	0x0c	/* Control Register 2 */
#define	PCI_L1PM_CTL2_TPOSCALE	__BITS(1, 0)	/* T_POWER_ON Scale */
#define	PCI_L1PM_CTL2_TPOVAL	__BITS(7, 3)	/* T_POWER_ON Value */

/*
 * Local constants
 */
#define PCI_INTRSTR_LEN			64

#endif /* _DEV_PCI_PCIREG_H_ */

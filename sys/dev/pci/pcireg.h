/*	$NetBSD: pcireg.h,v 1.84 2013/04/21 23:46:06 msaitoh Exp $	*/

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
#define	PCI_SUBCLASS_MASS_STORAGE_SATA		0x06
#define	PCI_SUBCLASS_MASS_STORAGE_SAS		0x07
#define	PCI_SUBCLASS_MASS_STORAGE_NVM		0x08
#define	PCI_SUBCLASS_MASS_STORAGE_MISC		0x80

/* 0x02 network subclasses */
#define	PCI_SUBCLASS_NETWORK_ETHERNET		0x00
#define	PCI_SUBCLASS_NETWORK_TOKENRING		0x01
#define	PCI_SUBCLASS_NETWORK_FDDI		0x02
#define	PCI_SUBCLASS_NETWORK_ATM		0x03
#define	PCI_SUBCLASS_NETWORK_ISDN		0x04
#define	PCI_SUBCLASS_NETWORK_WORLDFIP		0x05
#define	PCI_SUBCLASS_NETWORK_PCIMGMULTICOMP	0x06
#define	PCI_SUBCLASS_NETWORK_MISC		0x80

/* 0x03 display subclasses */
#define	PCI_SUBCLASS_DISPLAY_VGA		0x00
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
#define	PCI_SUBCLASS_BRIDGE_MC			0x03	/* XXX _MCA? */
#define	PCI_SUBCLASS_BRIDGE_PCI			0x04
#define	PCI_SUBCLASS_BRIDGE_PCMCIA		0x05
#define	PCI_SUBCLASS_BRIDGE_NUBUS		0x06
#define	PCI_SUBCLASS_BRIDGE_CARDBUS		0x07
#define	PCI_SUBCLASS_BRIDGE_RACEWAY		0x08
#define	PCI_SUBCLASS_BRIDGE_STPCI		0x09
#define	PCI_SUBCLASS_BRIDGE_INFINIBAND		0x0a
#define	PCI_SUBCLASS_BRIDGE_MISC		0x80

/* 0x07 communications subclasses */
#define	PCI_SUBCLASS_COMMUNICATIONS_SERIAL	0x00
#define	PCI_SUBCLASS_COMMUNICATIONS_PARALLEL	0x01
#define	PCI_SUBCLASS_COMMUNICATIONS_MPSERIAL	0x02
#define	PCI_SUBCLASS_COMMUNICATIONS_MODEM	0x03
#define	PCI_SUBCLASS_COMMUNICATIONS_GPIB	0x04
#define	PCI_SUBCLASS_COMMUNICATIONS_SMARTCARD	0x05
#define	PCI_SUBCLASS_COMMUNICATIONS_MISC	0x80

/* 0x08 system subclasses */
#define	PCI_SUBCLASS_SYSTEM_PIC			0x00
#define	PCI_SUBCLASS_SYSTEM_DMA			0x01
#define	PCI_SUBCLASS_SYSTEM_TIMER		0x02
#define	PCI_SUBCLASS_SYSTEM_RTC			0x03
#define	PCI_SUBCLASS_SYSTEM_PCIHOTPLUG		0x04
#define	PCI_SUBCLASS_SYSTEM_SDHC		0x05
#define	PCI_SUBCLASS_SYSTEM_MISC		0x80

/* 0x09 input subclasses */
#define	PCI_SUBCLASS_INPUT_KEYBOARD		0x00
#define	PCI_SUBCLASS_INPUT_DIGITIZER		0x01
#define	PCI_SUBCLASS_INPUT_MOUSE		0x02
#define	PCI_SUBCLASS_INPUT_SCANNER		0x03
#define	PCI_SUBCLASS_INPUT_GAMEPORT		0x04
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

/* 0x0c serial bus subclasses */
#define	PCI_SUBCLASS_SERIALBUS_FIREWIRE		0x00
#define	PCI_SUBCLASS_SERIALBUS_ACCESS		0x01
#define	PCI_SUBCLASS_SERIALBUS_SSA		0x02
#define	PCI_SUBCLASS_SERIALBUS_USB		0x03
#define	PCI_SUBCLASS_SERIALBUS_FIBER		0x04	/* XXX _FIBRECHANNEL */
#define	PCI_SUBCLASS_SERIALBUS_SMBUS		0x05
#define	PCI_SUBCLASS_SERIALBUS_INFINIBAND	0x06
#define	PCI_SUBCLASS_SERIALBUS_IPMI		0x07
#define	PCI_SUBCLASS_SERIALBUS_SERCOS		0x08
#define	PCI_SUBCLASS_SERIALBUS_CANBUS		0x09

/* 0x0d wireless subclasses */
#define	PCI_SUBCLASS_WIRELESS_IRDA		0x00
#define	PCI_SUBCLASS_WIRELESS_CONSUMERIR	0x01
#define	PCI_SUBCLASS_WIRELESS_RF		0x10
#define	PCI_SUBCLASS_WIRELESS_BLUETOOTH		0x11
#define	PCI_SUBCLASS_WIRELESS_BROADBAND		0x12
#define	PCI_SUBCLASS_WIRELESS_802_11A		0x20
#define	PCI_SUBCLASS_WIRELESS_802_11B		0x21
#define	PCI_SUBCLASS_WIRELESS_MISC		0x80

/* 0x0e I2O (Intelligent I/O) subclasses */
#define	PCI_SUBCLASS_I2O_STANDARD		0x00

/* 0x0f satellite communication subclasses */
/*	PCI_SUBCLASS_SATCOM_???			0x00	/ * XXX ??? */
#define	PCI_SUBCLASS_SATCOM_TV			0x01
#define	PCI_SUBCLASS_SATCOM_AUDIO		0x02
#define	PCI_SUBCLASS_SATCOM_VOICE		0x03
#define	PCI_SUBCLASS_SATCOM_DATA		0x04

/* 0x10 encryption/decryption subclasses */
#define	PCI_SUBCLASS_CRYPTO_NETCOMP		0x00
#define	PCI_SUBCLASS_CRYPTO_ENTERTAINMENT	0x10
#define	PCI_SUBCLASS_CRYPTO_MISC		0x80

/* 0x11 data acquisition and signal processing subclasses */
#define	PCI_SUBCLASS_DASP_DPIO			0x00
#define	PCI_SUBCLASS_DASP_TIMEFREQ		0x01
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
#define	PCI_CAP_LDT		0x08
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
 * Vital Product Data; access via capability pointer (PCI rev 2.2).
 */
#define	PCI_VPD_ADDRESS_MASK	0x7fff
#define	PCI_VPD_ADDRESS_SHIFT	16
#define	PCI_VPD_ADDRESS(ofs)	\
	(((ofs) & PCI_VPD_ADDRESS_MASK) << PCI_VPD_ADDRESS_SHIFT)
#define	PCI_VPD_DATAREG(ofs)	((ofs) + 4)
#define	PCI_VPD_OPFLAG		0x80000000

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

#define	PCI_MSI_CTL_MASK	__BITS(31, 16)
#define	PCI_MSI_CTL_PERVEC_MASK	__SHIFTIN(__BIT(8), PCI_MSI_CTL_MASK)
#define	PCI_MSI_CTL_64BIT_ADDR	__SHIFTIN(__BIT(7), PCI_MSI_CTL_MASK)
#define	PCI_MSI_CTL_MME_MASK	__SHIFTIN(__BITS(6, 4), PCI_MSI_CTL_MASK)
#define	PCI_MSI_CTL_MMC_MASK	__SHIFTIN(__BITS(3, 1), PCI_MSI_CTL_MASK)
#define	PCI_MSI_CTL_MSI_ENABLE	__SHIFTIN(__BIT(0), PCI_MSI_CTL_MASK)

/*
 * MSI Message Address is at offset 4.
 * MSI Message Upper Address (if 64bit) is at offset 8.
 * MSI Message data is at offset 8 or 12 and is 16 bits.
 * [16 bit reserved field]
 * MSI Mask Bits (32 bit field)
 * MSI Pending Bits (32 bit field)
 */

#define	PCI_MSIX_CTL_ENABLE	0x80000000
#define	PCI_MSIX_CTL_FUNCMASK	0x40000000
#define	PCI_MSIX_CTL_TBLSIZE_MASK 0x07ff0000
#define	PCI_MSIX_CTL_TBLSIZE_SHIFT 16
#define	PCI_MSIX_CTL_TBLSIZE(ofs)	(((ofs) >> PCI_MSIX_CTL_TBLSIZE_SHIFT) & PCI_MSIX_CTL_TBLSIZE_MASK)
/*
 * 2nd DWORD is the Table Offset
 */
#define	PCI_MSIX_TBLOFFSET_MASK	0xfffffff8
#define	PCI_MSIX_TBLBIR_MASK	0x00000007
/*
 * 3rd DWORD is the Pending Bitmap Array Offset
 */
#define	PCI_MSIX_PBAOFFSET_MASK	0xfffffff8
#define	PCI_MSIX_PBABIR_MASK	0x00000007

struct pci_msix_table_entry {
	uint32_t pci_msix_addr_lo;
	uint32_t pci_msix_addr_hi;
	uint32_t pci_msix_value;
	uint32_t pci_msix_vendor_control;
};
#define	PCI_MSIX_VENDCTL_MASK	0x00000001


/*
 * Power Management Capability; access via capability pointer.
 */

/* Power Management Capability Register */
#define PCI_PMCR_SHIFT		16
#define PCI_PMCR		0x02
#define PCI_PMCR_D1SUPP		0x0200
#define PCI_PMCR_D2SUPP		0x0400
/* Power Management Control Status Register */
#define PCI_PMCSR		0x04
#define	PCI_PMCSR_PME_EN	0x100
#define PCI_PMCSR_STATE_MASK	0x03
#define PCI_PMCSR_STATE_D0      0x00
#define PCI_PMCSR_STATE_D1      0x01
#define PCI_PMCSR_STATE_D2      0x02
#define PCI_PMCSR_STATE_D3      0x03
#define PCI_PMCSR_PME_STS       0x8000

/*
 * PCI-X capability.
 */

/*
 * Command. 16 bits at offset 2 (e.g. upper 16 bits of the first 32-bit
 * word at the capability; the lower 16 bits are the capability ID and
 * next capability pointer).
 *
 * Since we always read PCI config space in 32-bit words, we define these
 * as 32-bit values, offset and shifted appropriately.  Make sure you perform
 * the appropriate R/M/W cycles!
 */
#define PCIX_CMD			0x00
#define PCIX_CMD_PERR_RECOVER	0x00010000
#define PCIX_CMD_RELAXED_ORDER	0x00020000
#define PCIX_CMD_BYTECNT_MASK	0x000c0000
#define	PCIX_CMD_BYTECNT_SHIFT	18
#define		PCIX_CMD_BCNT_512		0x00000000
#define		PCIX_CMD_BCNT_1024		0x00040000
#define		PCIX_CMD_BCNT_2048		0x00080000
#define		PCIX_CMD_BCNT_4096		0x000c0000
#define PCIX_CMD_SPLTRANS_MASK	0x00700000
#define		PCIX_CMD_SPLTRANS_1		0x00000000
#define		PCIX_CMD_SPLTRANS_2		0x00100000
#define		PCIX_CMD_SPLTRANS_3		0x00200000
#define		PCIX_CMD_SPLTRANS_4		0x00300000
#define		PCIX_CMD_SPLTRANS_8		0x00400000
#define		PCIX_CMD_SPLTRANS_12	0x00500000
#define		PCIX_CMD_SPLTRANS_16	0x00600000
#define		PCIX_CMD_SPLTRANS_32	0x00700000

/*
 * Status. 32 bits at offset 4.
 */
#define PCIX_STATUS			0x04
#define PCIX_STATUS_FN_MASK		0x00000007
#define PCIX_STATUS_DEV_MASK	0x000000f8
#define PCIX_STATUS_BUS_MASK	0x0000ff00
#define PCIX_STATUS_64BIT		0x00010000
#define PCIX_STATUS_133		0x00020000
#define PCIX_STATUS_SPLDISC		0x00040000
#define PCIX_STATUS_SPLUNEX		0x00080000
#define PCIX_STATUS_DEVCPLX		0x00100000
#define PCIX_STATUS_MAXB_MASK	0x00600000
#define	PCIX_STATUS_MAXB_SHIFT	21
#define		PCIX_STATUS_MAXB_512	0x00000000
#define		PCIX_STATUS_MAXB_1024	0x00200000
#define		PCIX_STATUS_MAXB_2048	0x00400000
#define		PCIX_STATUS_MAXB_4096	0x00600000
#define PCIX_STATUS_MAXST_MASK	0x03800000
#define		PCIX_STATUS_MAXST_1		0x00000000
#define		PCIX_STATUS_MAXST_2		0x00800000
#define		PCIX_STATUS_MAXST_3		0x01000000
#define		PCIX_STATUS_MAXST_4		0x01800000
#define		PCIX_STATUS_MAXST_8		0x02000000
#define		PCIX_STATUS_MAXST_12	0x02800000
#define		PCIX_STATUS_MAXST_16	0x03000000
#define		PCIX_STATUS_MAXST_32	0x03800000
#define PCIX_STATUS_MAXRS_MASK	0x1c000000
#define		PCIX_STATUS_MAXRS_1K	0x00000000
#define		PCIX_STATUS_MAXRS_2K	0x04000000
#define		PCIX_STATUS_MAXRS_4K	0x08000000
#define		PCIX_STATUS_MAXRS_8K	0x0c000000
#define		PCIX_STATUS_MAXRS_16K	0x10000000
#define		PCIX_STATUS_MAXRS_32K	0x14000000
#define		PCIX_STATUS_MAXRS_64K	0x18000000
#define		PCIX_STATUS_MAXRS_128K	0x1c000000
#define PCIX_STATUS_SCERR			0x20000000

/*
 * PCI Express; access via capability pointer.
 */
#define PCIE_XCAP	0x00	/* Capability List & Capabilities Register */
#define	PCIE_XCAP_MASK		__BITS(31, 16)
/* Capability Version */
#define PCIE_XCAP_VER_MASK	__SHIFTIN(__BITS(3, 0), PCIE_XCAP_MASK)
#define	 PCIE_XCAP_VER_1_0	__SHIFTIN(1, PCIE_XCAP_VER_MASK)
#define	 PCIE_XCAP_VER_2_0	__SHIFTIN(2, PCIE_XCAP_VER_MASK)
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
#define   PCI_BRIDGE_BUS_PRIMARY_SHIFT		0
#define   PCI_BRIDGE_BUS_SECONDARY_SHIFT	8
#define   PCI_BRIDGE_BUS_SUBORDINATE_SHIFT	16

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
 * PCI-X 2.0 Extended Capability List
 */

#define	PCI_EXTCAPLIST_BASE		0x100

#define	PCI_EXTCAPLIST_CAP(ecr)		((ecr) & 0xffff)
#define	PCI_EXTCAPLIST_VERSION(ecr)	(((ecr) >> 16) & 0xf)
#define	PCI_EXTCAPLIST_NEXT(ecr)	(((ecr) >> 20) & 0xfff)

#endif /* _DEV_PCI_PCIREG_H_ */

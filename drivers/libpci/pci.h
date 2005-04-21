/*
pci.h

Created:	Jan 2000 by Philip Homburg <philip@cs.vu.nl>
*/

#define PCI_VID		0x00	/* Vendor ID, 16-bit */
#define PCI_DID		0x02	/* Device ID, 16-bit */
#define PCI_CR		0x04	/* Command Register, 16-bit */
#define PCI_PCISTS	0x06	/* PCI status, 16-bit */
#define		 PSR_SSE	0x4000	/* Signaled System Error */
#define		 PSR_RMAS	0x2000	/* Received Master Abort Status */
#define		 PSR_RTAS	0x1000	/* Received Target Abort Status */
#define PCI_PIFR	0x09	/* Prog. Interface Register */
#define PCI_SCR		0x0A	/* Sub-Class Register */
#define PCI_BCR		0x0B	/* Base-Class Register */
#define PCI_HEADT	0x0E	/* Header type, 8-bit */
#define		PHT_MULTIFUNC	0x80	/* Multiple functions */
#define PCI_BAR		0x10	/* Base Address Register */
#define PCI_ILR		0x3C	/* Interrupt Line Register */
#define PCI_IPR		0x3D	/* Interrupt Pin Register */

/* PCI bridge devices (AGP) */
#define PPB_SBUSN	0x19	/* Secondary Bus Number */

/* Intel compatible PCI bridge devices (AGP) */
#define PPB_SSTS	0x1E	/* Secondary PCI-to-PCI Status Register */

#define NO_VID		0xffff	/* No PCI card present */

struct pci_vendor
{
	u16_t vid;
	char *name;
};

struct pci_device
{
	u16_t vid;
	u16_t did;
	char *name;
};

struct pci_baseclass
{
	u8_t baseclass;
	char *name;
};

struct pci_subclass
{
	u8_t baseclass;
	u8_t subclass;
	u16_t infclass;
	char *name;
};

struct pci_intel_ctrl
{
	u16_t vid;
	u16_t did;
};

struct pci_isabridge
{
	u16_t vid;
	u16_t did;
	int checkclass;
	int type;
};

struct pci_pcibridge
{
	u16_t vid;
	u16_t did;
	int type;
};

#define PCI_IB_PIIX	1	/* Intel PIIX compatible ISA bridge */
#define PCI_IB_VIA	2	/* VIA compatible ISA bridge */
#define PCI_IB_AMD	3	/* AMD compatible ISA bridge */
#define PCI_IB_SIS	4	/* SIS compatible ISA bridge */

#define PCI_AGPB_INTEL	1	/* Intel compatible AGP bridge */
#define PCI_AGPB_VIA	2	/* VIA compatible AGP bridge */

extern struct pci_vendor pci_vendor_table[];
extern struct pci_device pci_device_table[];
extern struct pci_baseclass pci_baseclass_table[];
extern struct pci_subclass pci_subclass_table[];
extern struct pci_intel_ctrl pci_intel_ctrl[];
extern struct pci_isabridge pci_isabridge[];
extern struct pci_pcibridge pci_pcibridge[];

/*
 * $PchId: pci.h,v 1.4 2001/12/06 20:21:22 philip Exp $
 */

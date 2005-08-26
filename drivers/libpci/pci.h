/*
pci.h

Created:	Jan 2000 by Philip Homburg <philip@cs.vu.nl>
*/

/* tempory functions: to be replaced later (see pci_intel.h) */
_PROTOTYPE( unsigned pci_inb, (U16_t port) );
_PROTOTYPE( unsigned pci_inw, (U16_t port) );
_PROTOTYPE( unsigned pci_inl, (U16_t port) );

_PROTOTYPE( void pci_outb, (U16_t port, U8_t value) );
_PROTOTYPE( void pci_outw, (U16_t port, U16_t value) );
_PROTOTYPE( void pci_outl, (U16_t port, U32_t value) );

/* pci.c */
_PROTOTYPE( void pci_init, (void)					);
_PROTOTYPE( int pci_find_dev, (U8_t bus, U8_t dev, U8_t func,
							int *devindp)	);
_PROTOTYPE( int pci_first_dev, (int *devindp, u16_t *vidp, u16_t *didp)	);
_PROTOTYPE( int pci_next_dev, (int *devindp, u16_t *vidp, u16_t *didp)	);
_PROTOTYPE( void pci_reserve, (int devind)				);
_PROTOTYPE( void pci_ids, (int devind, u16_t *vidp, u16_t *didp)	);
_PROTOTYPE( char *pci_slot_name, (int devind)				);
_PROTOTYPE( char *pci_dev_name, (U16_t vid, U16_t did)			);
_PROTOTYPE( u8_t pci_attr_r8, (int devind, int port)			);
_PROTOTYPE( u16_t pci_attr_r16, (int devind, int port)			);
_PROTOTYPE( u32_t pci_attr_r32, (int devind, int port)			);
_PROTOTYPE( void pci_attr_w16, (int devind, int port, U16_t value)	);
_PROTOTYPE( void pci_attr_w32, (int devind, int port, u32_t value)	);

#define PCI_VID		0x00	/* Vendor ID, 16-bit */
#define PCI_DID		0x02	/* Device ID, 16-bit */
#define PCI_CR		0x04	/* Command Register, 16-bit */
#define PCI_PCISTS	0x06	/* PCI status, 16-bit */
#define		 PSR_SSE	0x4000	/* Signaled System Error */
#define		 PSR_RMAS	0x2000	/* Received Master Abort Status */
#define		 PSR_RTAS	0x1000	/* Received Target Abort Status */
#define PCI_REV		0x08	/* Revision ID */
#define PCI_PIFR	0x09	/* Prog. Interface Register */
#define PCI_SCR		0x0A	/* Sub-Class Register */
#define PCI_BCR		0x0B	/* Base-Class Register */
#define PCI_HEADT	0x0E	/* Header type, 8-bit */
#define		PHT_MULTIFUNC	0x80	/* Multiple functions */
#define PCI_BAR		0x10	/* Base Address Register */
#define PCI_BAR_2	0x14	/* Base Address Register */
#define PCI_BAR_3	0x18	/* Base Address Register */
#define PCI_BAR_4	0x1C	/* Base Address Register */
#define PCI_ILR		0x3C	/* Interrupt Line Register */
#define PCI_IPR		0x3D	/* Interrupt Pin Register */

/* Device type values as ([PCI_BCR] << 16) | ([PCI_SCR] << 8) | [PCI_PIFR] */
#define	PCI_T3_PCI2PCI		0x060400	/* PCI-to-PCI Bridge device */
#define	PCI_T3_PCI2PCI_SUBTR	0x060401	/* Subtr. PCI-to-PCI Bridge */

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

#define PCI_PCIB_INTEL	1	/* Intel compatible PCI bridge */
#define PCI_AGPB_INTEL	2	/* Intel compatible AGP bridge */
#define PCI_AGPB_VIA	3	/* VIA compatible AGP bridge */

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

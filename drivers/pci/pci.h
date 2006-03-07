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

#define PCI_PPB_STD	1	/* Standard PCI-to-PCI bridge */
#define PCI_PPB_CB	2	/* Cardbus bridge */
/* Still needed? */
#define PCI_AGPB_VIA	3	/* VIA compatible AGP bridge */

extern struct pci_vendor pci_vendor_table[];
extern struct pci_device pci_device_table[];
extern struct pci_baseclass pci_baseclass_table[];
extern struct pci_subclass pci_subclass_table[];
#if 0
extern struct pci_intel_ctrl pci_intel_ctrl[];
#endif
extern struct pci_isabridge pci_isabridge[];
extern struct pci_pcibridge pci_pcibridge[];

/* Utility functions */
_PROTOTYPE( void pci_reserve3, (int devind, int proc, char name[M3_STRING]));
_PROTOTYPE( void pci_release, (char name[M3_STRING])			);

/*
 * $PchId: pci.h,v 1.4 2001/12/06 20:21:22 philip Exp $
 */

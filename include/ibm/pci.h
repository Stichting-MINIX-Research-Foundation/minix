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

/*
 * $PchId: pci.h,v 1.4 2001/12/06 20:21:22 philip Exp $
 */

/*
pci_intel.h

Created:	Jan 2000 by Philip Homburg <philip@cs.vu.nl>
*/

#define PCII_CONFADD	0xCF8
#define		PCIIC_CODE		0x80000000
#define		PCIIC_BUSNUM_MASK	  0xff0000
#define		PCIIC_BUSNUM_SHIFT	        16
#define		PCIIC_DEVNUM_MASK	    0xf800
#define		PCIIC_DEVNUM_SHIFT	        11
#define		PCIIC_FUNCNUM_MASK	     0x700
#define		PCIIC_FUNCNUM_SHIFT	         8
#define		PCIIC_REGNUM_MASK	      0xfc
#define		PCIIC_REGNUM_SHIFT	         2
#define PCII_CONFDATA	0xCFC

#define PCII_SELREG_(bus, dev, func, reg) \
	(PCIIC_CODE | \
	    (((bus) << PCIIC_BUSNUM_SHIFT) & PCIIC_BUSNUM_MASK) | \
	    (((dev) << PCIIC_DEVNUM_SHIFT) & PCIIC_DEVNUM_MASK) | \
	    (((func) << PCIIC_FUNCNUM_SHIFT) & PCIIC_FUNCNUM_MASK) | \
	    ((((reg)/4) << PCIIC_REGNUM_SHIFT) & PCIIC_REGNUM_MASK))
#define PCII_UNSEL 	(0)

#define PCII_RREG8_(bus, dev, func, reg) \
	(pci_outl(PCII_CONFADD, PCII_SELREG_(bus, dev, func, reg)), \
	pci_inb(PCII_CONFDATA+((reg)&3)))
#define PCII_RREG16_(bus, dev, func, reg) \
	(PCII_RREG8_(bus, dev, func, reg) | \
	(PCII_RREG8_(bus, dev, func, reg+1) << 8))
#define PCII_RREG32_(bus, dev, func, reg) \
	(PCII_RREG16_(bus, dev, func, reg) | \
	(PCII_RREG16_(bus, dev, func, reg+2) << 16))

#define PCII_WREG8_(bus, dev, func, reg, val) \
	(pci_outl(PCII_CONFADD, PCII_SELREG_(bus, dev, func, reg)), \
	pci_outb(PCII_CONFDATA+((reg)&3), (val)))
#define PCII_WREG16_(bus, dev, func, reg, val) \
	(PCII_WREG8_(bus, dev, func, reg, (val)), \
	(PCII_WREG8_(bus, dev, func, reg+1, (val) >> 8)))
#define PCII_WREG32_(bus, dev, func, reg, val) \
	(PCII_WREG16_(bus, dev, func, reg, (val)), \
	(PCII_WREG16_(bus, dev, func, reg+2, (val) >> 16)))

/* PIIX configuration registers */
#define PIIX_PIRQRCA	0x60
#define 	PIIX_IRQ_DI	0x80
#define 	PIIX_IRQ_MASK	0x0F

/* PIIX extensions to the PIC */
#define PIIX_ELCR1	0x4D0
#define PIIX_ELCR2	0x4D1

/*
 * $PchId: pci_intel.h,v 1.1 2000/08/12 11:20:17 philip Exp $
 */

/*
pci.h

Created:	Jan 2000 by Philip Homburg <philip@cs.vu.nl>
*/

/* Header type 00, normal PCI devices */
#define PCI_VID		0x00	/* Vendor ID, 16-bit */
#define PCI_DID		0x02	/* Device ID, 16-bit */
#define PCI_CR		0x04	/* Command Register, 16-bit */
#define		PCI_CR_MAST_EN	0x0004	/* Enable Busmaster Access */
#define		PCI_CR_MEM_EN	0x0002	/* Enable Mem Cycles */
#define		PCI_CR_IO_EN	0x0001	/* Enable I/O Cycles */
#define PCI_SR		0x06	/* PCI status, 16-bit */
#define		 PSR_SSE	0x4000	/* Signaled System Error */
#define		 PSR_RMAS	0x2000	/* Received Master Abort Status */
#define		 PSR_RTAS	0x1000	/* Received Target Abort Status */
#define		 PSR_CAPPTR	0x0010	/* Capabilities list */
#define PCI_REV		0x08	/* Revision ID */
#define PCI_PIFR	0x09	/* Prog. Interface Register */
#define PCI_SCR		0x0A	/* Sub-Class Register */
#define PCI_BCR		0x0B	/* Base-Class Register */
#define PCI_CLS		0x0C	/* Cache Line Size */
#define PCI_LT		0x0D	/* Latency Timer */
#define PCI_HEADT	0x0E	/* Header type, 8-bit */
#define	    PHT_MASK		0x7F	/* Header type mask */
#define	    	PHT_NORMAL		0x00
#define	    	PHT_BRIDGE		0x01
#define	    	PHT_CARDBUS		0x02
#define	    PHT_MULTIFUNC	0x80	/* Multiple functions */
#define PCI_BIST	0x0F	/* Built-in Self Test */
#define PCI_BAR		0x10	/* Base Address Register */
#define	    PCI_BAR_IO		0x00000001	/* Reg. refers to I/O space */
#define	    PCI_BAR_TYPE	0x00000006	/* Memory BAR type */
#define	        PCI_TYPE_32	0x00000000	/* 32-bit BAR */
#define	        PCI_TYPE_32_1M	0x00000002	/* 32-bit below 1MB (legacy) */
#define	        PCI_TYPE_64	0x00000004	/* 64-bit BAR */
#define	    PCI_BAR_PREFETCH	0x00000008	/* Memory is prefetchable */
#define	    PCI_BAR_IO_MASK	0xFFFFFFFC	/* I/O address mask */
#define	    PCI_BAR_MEM_MASK	0xFFFFFFF0	/* Memory address mask */
#define PCI_BAR_2	0x14	/* Base Address Register */
#define PCI_BAR_3	0x18	/* Base Address Register */
#define PCI_BAR_4	0x1C	/* Base Address Register */
#define PCI_BAR_5	0x20	/* Base Address Register */
#define PCI_BAR_6	0x24	/* Base Address Register */
#define PCI_CBCISPTR	0x28	/* Cardbus CIS Pointer */
#define PCI_SUBVID	0x2C	/* Subsystem Vendor ID */
#define PCI_SUBDID	0x2E	/* Subsystem Device ID */
#define PCI_EXPROM	0x30	/* Expansion ROM Base Address */
#define PCI_CAPPTR	0x34	/* Capabilities Pointer */
#define		PCI_CP_MASK	0xfc	/* Lower 2 bits should be ignored */
#define PCI_ILR		0x3C	/* Interrupt Line Register */
#define		PCI_ILR_UNKNOWN	0xFF	/* IRQ is unassigned or unknown */
#define PCI_IPR		0x3D	/* Interrupt Pin Register */
#define PCI_MINGNT	0x3E	/* Min Grant */
#define PCI_MAXLAT	0x3F	/* Max Latency */

/* Header type 01, PCI-to-PCI bridge devices */
/* The following registers are in common with type 00:
 * PCI_VID, PCI_DID, PCI_CR, PCI_SR, PCI_REV, PCI_PIFR, PCI_SCR, PCI_BCR,
 * PCI_CLS, PCI_LT, PCI_HEADT, PCI_BIST, PCI_BAR, PCI_BAR2, PCI_CAPPTR,
 * PCI_ILR, PCI_IPR.
 */
#define PPB_PRIMBN	0x18	/* Primary Bus Number */
#define PPB_SECBN	0x19	/* Secondary Bus Number */
#define PPB_SUBORDBN	0x1A	/* Subordinate Bus Number */
#define PPB_SECBLT	0x1B	/* Secondary Bus Latency Timer */
#define PPB_IOBASE	0x1C	/* I/O Base */
#define		PPB_IOB_MASK	0xf0
#define PPB_IOLIMIT	0x1D	/* I/O Limit */
#define		PPB_IOL_MASK	0xf0
#define PPB_SSTS	0x1E	/* Secondary Status Register */
#define PPB_MEMBASE	0x20	/* Memory Base */
#define		PPB_MEMB_MASK	0xfff0
#define PPB_MEMLIMIT	0x22	/* Memory Limit */
#define		PPB_MEML_MASK	0xfff0
#define PPB_PFMEMBASE	0x24	/* Prefetchable Memory Base */
#define		PPB_PFMEMB_MASK	0xfff0
#define PPB_PFMEMLIMIT	0x26	/* Prefetchable Memory Limit */
#define		PPB_PFMEML_MASK	0xfff0
#define PPB_PFMBU32	0x28	/* Prefetchable Memory Base Upper 32 */
#define PPB_PFMLU32	0x2C	/* Prefetchable Memory Limit Upper 32 */
#define PPB_IOBASEU16	0x30	/* I/O Base Upper 16 */
#define PPB_IOLIMITU16	0x32	/* I/O Limit Upper 16 */
#define PPB_EXPROM	0x38	/* Expansion ROM Base Address */
#define PPB_BRIDGECTRL	0x3E	/* Bridge Control */
#define		PPB_BC_CRST	0x40	/* Assert reset line */

/* Header type 02, Cardbus bridge devices */
/* The following registers are in common with type 00:
 * PCI_VID, PCI_DID, PCI_CR, PCI_SR, PCI_REV, PCI_PIFR, PCI_SCR, PCI_BCR,
 * PCI_CLS, PCI_LT, PCI_HEADT, PCI_BIST, PCI_BAR, PCI_ILR, PCI_IPR.
 */
/* The following registers are in common with type 01:
 * PPB_PRIMBN, PPB_SECBN, PPB_SUBORDBN, PPB_SECBLT.
 */
#define CBB_CAPPTR	0x14	/* Capability Pointer */
#define CBB_SSTS	0x16	/* Secondary Status Register */
#define CBB_MEMBASE_0	0x1C	/* Memory Base 0 */
#define CBB_MEMLIMIT_0	0x20	/* Memory Limit 0 */
#define 	CBB_MEML_MASK	0xfffff000	
#define CBB_MEMBASE_1	0x24	/* Memory Base 1 */
#define CBB_MEMLIMIT_1	0x28	/* Memory Limit 1 */
#define CBB_IOBASE_0	0x2C	/* I/O Base 0 */
#define CBB_IOLIMIT_0	0x30	/* I/O Limit 0 */
#define 	CBB_IOL_MASK	0xfffffffc	
#define CBB_IOBASE_1	0x34	/* I/O Base 1 */
#define CBB_IOLIMIT_1	0x38	/* I/O Limit 1 */
#define CBB_BRIDGECTRL	0x3E	/* Bridge Control */
#define		CBB_BC_INTEXCA	0x80	/* Interrupt are routed to ExCAs */
#define		CBB_BC_CRST	0x40	/* Assert reset line */

#define CAP_TYPE	0x00	/* Type field in capability */
#define CAP_NEXT	0x01	/* Next field in capability */

#define PCI_BCR_MASS_STORAGE	0x01	/* Mass Storage class */
#define 	PCI_MS_IDE		0x01	/* IDE storage class */
#define			PCI_IDE_PRI_NATIVE	0x01	/* Primary channel is
							 * in native mode.
							 */
#define			PCI_IDE_SEC_NATIVE	0x04	/* Secondary channel is
							 * in native mode.
							 */

/* Device type values as ([PCI_BCR] << 16) | ([PCI_SCR] << 8) | [PCI_PIFR] */
#define PCI_T3_VGA_OLD		0x000100	/* OLD VGA class code */
#define	PCI_T3_RAID		0x010400	/* RAID controller */
#define PCI_T3_AHCI		0x010601	/* AHCI controller */
#define PCI_T3_VGA		0x030000	/* VGA-compatible video card */
#define PCI_T3_ISA		0x060100	/* ISA bridge */
#define	PCI_T3_PCI2PCI		0x060400	/* PCI-to-PCI Bridge device */
#define	PCI_T3_PCI2PCI_SUBTR	0x060401	/* Subtr. PCI-to-PCI Bridge */
#define	PCI_T3_CARDBUS		0x060700	/* Bardbus Bridge */

#define NO_VID		0xffff	/* No PCI card present */

/* Capabilities */
#define CAP_T_SECURE_DEV	0x0f		/* (AMD) Secure device
						 * capability
						 */
#define CAP_SD_INFO		2		/* Offset from CAP ptr */
#define 	CAP_SD_SUBTYPE_MASK	0x0f	/* Mask for subtype */
#define			CAP_T_SD_DEV		0 /* AMD DEV */

/*
 * $PchId: pci.h,v 1.4 2001/12/06 20:21:22 philip Exp $
 */

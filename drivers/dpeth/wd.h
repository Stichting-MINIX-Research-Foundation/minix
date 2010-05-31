/*
**  File: wd.h
**   
**  Created:	before Dec 28, 1992 by Philip Homburg
**  $PchId: wdeth.h,v 1.4 1995/12/22 08:36:57 philip Exp $
**
**  $Log$
**  Revision 1.2  2005/08/22 15:17:40  beng
**  Remove double-blank lines (Al)
**
**  Revision 1.1  2005/06/29 10:16:46  beng
**  Import of dpeth 3c501/3c509b/.. ethernet driver by
**  Giovanni Falzoni <fgalzoni@inwind.it>.
**
**  Revision 2.0  2005/06/26 16:16:46  lsodgf0
**  Initial revision for Minix 3.0.6
*/

#ifndef WDETH_H
#define WDETH_H

/* Western Digital Ethercard Plus, or WD8003E card. */

#define EPL_REG0	 0x0	/* Control(write) and status(read) */
#define EPL_REG1	 0x1
#define EPL_REG2	 0x2
#define EPL_REG3	 0x3
#define EPL_REG4	 0x4
#define EPL_REG5	 0x5
#define EPL_REG6	 0x6
#define EPL_REG7	 0x7
#define EPL_EA0		 0x8	/* Most significant eaddr byte */
#define EPL_EA1		 0x9
#define EPL_EA2		 0xA
#define EPL_EA3		 0xB
#define EPL_EA4		 0xC
#define EPL_EA5		 0xD	/* Least significant eaddr byte */
#define EPL_TLB		 0xE
#define EPL_CHKSUM	 0xF	/* sum from epl_ea0 upto here is 0xFF */
#define EPL_DP8390	0x10	/* NatSemi chip */

#define EPL_MSR		EPL_REG0/* memory select register */
#define EPL_ICR 	EPL_REG1/* interface configuration register */
#define EPL_IRR		EPL_REG4/* interrupt request register (IRR) */
#define EPL_790_HWR	EPL_REG4/* '790 hardware support register */
#define EPL_LAAR	EPL_REG5/* LA address register (write only) */
#define EPL_790_ICR	EPL_REG6/* '790 interrupt control register */
#define EPL_GP2		EPL_REG7/* general purpose register 2 */
#define EPL_790_B	EPL_EA3	/* '790 memory register */
#define EPL_790_GCR	EPL_EA5	/* '790 General Control Register */

/* Bits in EPL_MSR */
#define E_MSR_MEMADDR	0x3F	/* Bits SA18-SA13, SA19 implicit 1 */
#define E_MSR_MENABLE	0x40	/* Memory Enable */
#define E_MSR_RESET	0x80	/* Software Reset */

/* Bits in EPL_ICR */
#define E_ICR_16BIT	0x01	/* 16 bit bus */
#define E_ICR_IR2	0x04	/* bit 2 of encoded IRQ */
#define E_ICR_MEMBIT	0x08	/* 583 mem size mask */

/* Bits in EPL_IRR */
#define E_IRR_IR0	0x20	/* bit 0 of encoded IRQ */
#define E_IRR_IR1	0x40	/* bit 1 of encoded IRQ */
#define E_IRR_IEN	0x80	/* enable interrupts */

/* Bits in EPL_LAAR */
#define E_LAAR_A19	0x01	/* address lines for above 1M ram */
#define E_LAAR_A20	0x02	/* address lines for above 1M ram */
#define E_LAAR_A21	0x04	/* address lines for above 1M ram */
#define E_LAAR_A22	0x08	/* address lines for above 1M ram */
#define E_LAAR_A23	0x10	/* address lines for above 1M ram */
#define E_LAAR_SOFTINT	0x20	/* enable software interrupt */
#define E_LAAR_LAN16E	0x40	/* enables 16 bit RAM for LAN */
#define E_LAAR_MEM16E	0x80	/* enables 16 bit RAM for host */

/* Bits and values in EPL_TLB */
#define E_TLB_EB	0x05	/* WD8013EB */
#define E_TLB_E		0x27	/* WD8013 Elite */
#define E_TLB_SMCE	0x29	/* SMC Elite 16 */
#define E_TLB_SMC8216C	0x2B	/* SMC 8216 C */

#define E_TLB_REV	0x1F	/* revision mask */
#define E_TLB_SOFT	0x20	/* soft config */
#define E_TLB_RAM	0x40	/* extra ram bit */

/* Bits in EPL_790_HWR */
#define E_790_HWR_SWH	0x80	/* switch register set */

/* Bits in EPL_790_ICR */
#define E_790_ICR_EIL	0x01	/* enable interrupts */

/* Bits in EPL_790_GCR when E_790_HWR_SWH is set in EPL_790_HWR */
#define E_790_GCR_IR0	0x04	/* bit 0 of encoded IRQ */
#define E_790_GCR_IR1	0x08	/* bit 1 of encoded IRQ */
#define E_790_GCR_IR2	0x40	/* bit 2 of encoded IRQ */

#define inb_we(dep, reg) (inb(dep->de_base_port+reg))
#define outb_we(dep, reg, data) (outb(dep->de_base_port+reg, data))

#endif				/* WDETH_H */

/** wd.h **/

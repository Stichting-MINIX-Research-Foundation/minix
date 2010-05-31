/*
**  File:	8390.h		May  02, 2000
**
**  Author:	Giovanni Falzoni <gfalzoni@inwind.it>
**
**  National Semiconductor NS 8390 Network Interface Controller
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

#define DP_PAGESIZE	256	/* NS 8390 page size */
#define SENDQ_PAGES	6	/* SENDQ_PAGES * DP_PAGESIZE >= 1514 bytes */

/* Page 0, read/write ------------- */
#define	DP_CR		0x00	/* Command Register		RW */
#define	DP_CLDA0	0x01	/* Current Local Dma Address 0	RO */
#define	DP_PSTART	0x01	/* Page Start Register		WO */
#define	DP_CLDA1	0x02	/* Current Local Dma Address 1	RO */
#define	DP_PSTOP	0x02	/* Page Stop Register		WO */
#define	DP_BNRY		0x03	/* Boundary Pointer		RW */
#define	DP_TSR		0x04	/* Transmit Status Register	RO */
#define	DP_TPSR		0x04	/* Transmit Page Start Register	WO */
#define	DP_NCR		0x05	/* No. of Collisions Register	RO */
#define	DP_TBCR0	0x05	/* Transmit Byte Count Reg. 0	WO */
#define	DP_FIFO		0x06	/* Fifo				RO */
#define	DP_TBCR1	0x06	/* Transmit Byte Count Reg. 1	WO */
#define	DP_ISR		0x07	/* Interrupt Status Register	RW */
#define	DP_CRDA0	0x08	/* Current Remote Dma Addr.Low	RO */
#define	DP_RSAR0	0x08	/* Remote Start Address Low	WO */
#define	DP_CRDA1	0x09	/* Current Remote Dma Addr.High	RO */
#define	DP_RSAR1	0x09	/* Remote Start Address High	WO */
#define	DP_RBCR0	0x0A	/* Remote Byte Count Low	WO */
#define	DP_RBCR1	0x0B	/* Remote Byte Count Hihg	WO */
#define	DP_RSR		0x0C	/* Receive Status Register	RO */
#define	DP_RCR		0x0C	/* Receive Config. Register	WO */
#define	DP_CNTR0	0x0D	/* Tally Counter 0		RO */
#define	DP_TCR		0x0D	/* Transmit Config. Register	WO */
#define	DP_CNTR1	0x0E	/* Tally Counter 1		RO */
#define	DP_DCR		0x0E	/* Data Configuration Register	WO */
#define	DP_CNTR2	0x0F	/* Tally Counter 2		RO */
#define	DP_IMR		0x0F	/* Interrupt Mask Register	WO */

 /* Page 1, read/write -------------- */
/*	DP_CR		0x00	   Command Register */
#define	DP_PAR0		0x01	/* Physical Address Register 0 */
#define	DP_PAR1		0x02	/* Physical Address Register 1 */
#define	DP_PAR2		0x03	/* Physical Address Register 2 */
#define	DP_PAR3		0x04	/* Physical Address Register 3 */
#define	DP_PAR4		0x05	/* Physical Address Register 4 */
#define	DP_PAR5		0x06	/* Physical Address Register 5 */
#define	DP_CURR		0x07	/* Current Page Register */
#define	DP_MAR0		0x08	/* Multicast Address Register 0 */
#define	DP_MAR1		0x09	/* Multicast Address Register 1 */
#define	DP_MAR2		0x0A	/* Multicast Address Register 2 */
#define	DP_MAR3		0x0B	/* Multicast Address Register 3 */
#define	DP_MAR4		0x0C	/* Multicast Address Register 4 */
#define	DP_MAR5		0x0D	/* Multicast Address Register 5 */
#define	DP_MAR6		0x0E	/* Multicast Address Register 6 */
#define	DP_MAR7		0x0F	/* Multicast Address Register 7 */

/* Bits in dp_cr */
#define CR_STP		0x01	/* Stop: software reset */
#define CR_STA		0x02	/* Start: activate NIC */
#define CR_TXP		0x04	/* Transmit Packet */
#define CR_DMA		0x38	/* Mask for DMA control */
#define CR_DM_RR	0x08	/* DMA: Remote Read */
#define CR_DM_RW	0x10	/* DMA: Remote Write */
#define CR_DM_SP	0x18	/* DMA: Send Packet */
#define CR_NO_DMA	0x20	/* DMA: Stop Remote DMA Operation */
#define CR_PS		0xC0	/* Mask for Page Select */
#define CR_PS_P0	0x00	/* Register Page 0 */
#define CR_PS_P1	0x40	/* Register Page 1 */
#define CR_PS_P2	0x80	/* Register Page 2 */

/* Bits in dp_isr */
#define ISR_MASK	0x3F
#define ISR_PRX		0x01	/* Packet Received with no errors */
#define ISR_PTX		0x02	/* Packet Transmitted with no errors */
#define ISR_RXE		0x04	/* Receive Error */
#define ISR_TXE		0x08	/* Transmit Error */
#define ISR_OVW		0x10	/* Overwrite Warning */
#define ISR_CNT		0x20	/* Counter Overflow */
#define ISR_RDC		0x40	/* Remote DMA Complete */
#define ISR_RST		0x80	/* Reset Status */

/* Bits in dp_imr */
#define IMR_PRXE	0x01	/* Packet Received Enable */
#define IMR_PTXE	0x02	/* Packet Transmitted Enable */
#define IMR_RXEE	0x04	/* Receive Error Enable */
#define IMR_TXEE	0x08	/* Transmit Error Enable */
#define IMR_OVWE	0x10	/* Overwrite Warning Enable */
#define IMR_CNTE	0x20	/* Counter Overflow Enable */
#define IMR_RDCE	0x40	/* DMA Complete Enable */

/* Bits in dp_dcr */
#define DCR_WTS		0x01	/* Word Transfer Select */
#define DCR_BYTEWIDE	0x00	/* WTS: byte wide transfers */
#define DCR_WORDWIDE	0x01	/* WTS: word wide transfers */
#define DCR_BOS		0x02	/* Byte Order Select */
#define DCR_LTLENDIAN	0x00	/* BOS: Little Endian */
#define DCR_BIGENDIAN	0x02	/* BOS: Big Endian */
#define DCR_LAS		0x04	/* Long Address Select */
#define DCR_BMS		0x08	/* Burst Mode Select */
#define DCR_AR		0x10	/* Autoinitialize Remote */
#define DCR_FTS		0x60	/* Fifo Threshold Select */
#define DCR_2BYTES	0x00	/* Fifo Threshold: 2 bytes */
#define DCR_4BYTES	0x20	/* Fifo Threshold: 4 bytes */
#define DCR_8BYTES	0x40	/* Fifo Threshold: 8 bytes */
#define DCR_12BYTES	0x60	/* Fifo Threshold: 12 bytes */

/* Bits in dp_tcr */
#define TCR_CRC		0x01	/* Inhibit CRC */
#define TCR_ELC		0x06	/* Encoded Loopback Control */
#define TCR_NORMAL	0x00	/* ELC: Normal Operation */
#define TCR_INTERNAL	0x02	/* ELC: Internal Loopback */
#define TCR_0EXTERNAL	0x04	/* ELC: External Loopback LPBK=0 */
#define TCR_1EXTERNAL	0x06	/* ELC: External Loopback LPBK=1 */
#define TCR_ATD		0x08	/* Auto Transmit */
#define TCR_OFST	0x10	/* Collision Offset Enable */

/* Bits in dp_tsr */
#define TSR_PTX		0x01	/* Packet Transmitted (without error) */
#define TSR_DFR		0x02	/* Transmit Deferred */
#define TSR_COL		0x04	/* Transmit Collided */
#define TSR_ABT		0x08	/* Transmit Aborted */
#define TSR_CRS		0x10	/* Carrier Sense Lost */
#define TSR_FU		0x20	/* FIFO Underrun */
#define TSR_CDH		0x40	/* CD Heartbeat */
#define TSR_OWC		0x80	/* Out of Window Collision */

/* Bits in dp_rcr */
#define RCR_SEP		0x01	/* Save Errored Packets */
#define RCR_AR		0x02	/* Accept Runt Packets */
#define RCR_AB		0x04	/* Accept Broadcast */
#define RCR_AM		0x08	/* Accept Multicast */
#define RCR_PRO		0x10	/* Physical Promiscuous */
#define RCR_MON		0x20	/* Monitor Mode */

/* Bits in dp_rsr */
#define RSR_PRX		0x01	/* Packet Received Intact */
#define RSR_CRC		0x02	/* CRC Error */
#define RSR_FAE		0x04	/* Frame Alignment Error */
#define RSR_FO		0x08	/* FIFO Overrun */
#define RSR_MPA		0x10	/* Missed Packet */
#define RSR_PHY		0x20	/* Multicast Address Match !! */
#define RSR_DIS		0x40	/* Receiver Disabled */

/* Some macros to simplify accessing the dp8390 */
#define inb_reg0(dep,reg) (inb(dep->de_dp8390_port+reg))
#define outb_reg0(dep,reg,data) (outb(dep->de_dp8390_port+reg,data))
#define inb_reg1(dep,reg) (inb(dep->de_dp8390_port+reg))
#define outb_reg1(dep,reg,data) (outb(dep->de_dp8390_port+reg,data))

typedef struct dp_rcvhdr {
  u8_t dr_status;		/* Copy of rsr */
  u8_t dr_next;			/* Pointer to next packet */
  u8_t dr_rbcl;			/* Receive Byte Count Low */
  u8_t dr_rbch;			/* Receive Byte Count High */
} dp_rcvhdr_t;

void ns_init(dpeth_t *);

/** 8390.h **/

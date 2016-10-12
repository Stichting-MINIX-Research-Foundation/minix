/*
dp8390.h

Created:	before Dec 28, 1992 by Philip Homburg
*/

/* National Semiconductor DP8390 Network Interface Controller. */

				/* Page 0, for reading ------------- */
#define	DP_CR		0x0	/* Read side of Command Register     */
#define	DP_CLDA0	0x1	/* Current Local Dma Address 0       */
#define	DP_CLDA1	0x2	/* Current Local Dma Address 1       */
#define	DP_BNRY		0x3	/* Boundary Pointer                  */
#define	DP_TSR		0x4	/* Transmit Status Register          */
#define	DP_NCR		0x5	/* Number of Collisions Register     */
#define	DP_FIFO		0x6	/* Fifo ??                           */
#define	DP_ISR		0x7	/* Interrupt Status Register         */
#define	DP_CRDA0	0x8	/* Current Remote Dma Address 0      */
#define	DP_CRDA1	0x9	/* Current Remote Dma Address 1      */
#define	DP_DUM1		0xA	/* unused                            */
#define	DP_DUM2		0xB	/* unused                            */
#define	DP_RSR		0xC	/* Receive Status Register           */
#define	DP_CNTR0	0xD	/* Tally Counter 0                   */
#define	DP_CNTR1	0xE	/* Tally Counter 1                   */
#define	DP_CNTR2	0xF	/* Tally Counter 2                   */

				/* Page 0, for writing ------------- */
#define	DP_CR		0x0	/* Write side of Command Register    */
#define	DP_PSTART	0x1	/* Page Start Register               */
#define	DP_PSTOP	0x2	/* Page Stop Register                */
#define	DP_BNRY		0x3	/* Boundary Pointer                  */
#define	DP_TPSR		0x4	/* Transmit Page Start Register      */
#define	DP_TBCR0	0x5	/* Transmit Byte Count Register 0    */
#define	DP_TBCR1	0x6	/* Transmit Byte Count Register 1    */
#define	DP_ISR		0x7	/* Interrupt Status Register         */
#define	DP_RSAR0	0x8	/* Remote Start Address Register 0   */
#define	DP_RSAR1	0x9	/* Remote Start Address Register 1   */
#define	DP_RBCR0	0xA	/* Remote Byte Count Register 0      */
#define	DP_RBCR1	0xB	/* Remote Byte Count Register 1      */
#define	DP_RCR		0xC	/* Receive Configuration Register    */
#define	DP_TCR		0xD	/* Transmit Configuration Register   */
#define	DP_DCR		0xE	/* Data Configuration Register       */
#define	DP_IMR		0xF	/* Interrupt Mask Register           */

				/* Page 1, read/write -------------- */
#define	DP_CR		0x0	/* Command Register                  */
#define	DP_PAR0		0x1	/* Physical Address Register 0       */
#define	DP_PAR1		0x2	/* Physical Address Register 1       */
#define	DP_PAR2		0x3	/* Physical Address Register 2       */
#define	DP_PAR3		0x4	/* Physical Address Register 3       */
#define	DP_PAR4		0x5	/* Physical Address Register 4       */
#define	DP_PAR5		0x6	/* Physical Address Register 5       */
#define	DP_CURR		0x7	/* Current Page Register             */
#define	DP_MAR0		0x8	/* Multicast Address Register 0      */
#define	DP_MAR1		0x9	/* Multicast Address Register 1      */
#define	DP_MAR2		0xA	/* Multicast Address Register 2      */
#define	DP_MAR3		0xB	/* Multicast Address Register 3      */
#define	DP_MAR4		0xC	/* Multicast Address Register 4      */
#define	DP_MAR5		0xD	/* Multicast Address Register 5      */
#define	DP_MAR6		0xE	/* Multicast Address Register 6      */
#define	DP_MAR7		0xF	/* Multicast Address Register 7      */

/* Bits in dp_cr */
#define CR_STP		0x01	/* Stop: software reset              */
#define CR_STA		0x02	/* Start: activate NIC               */
#define CR_TXP		0x04	/* Transmit Packet                   */
#define CR_DMA		0x38	/* Mask for DMA control              */
#define CR_DM_NOP	0x00	/* DMA: No Operation                 */
#define CR_DM_RR	0x08	/* DMA: Remote Read                  */
#define CR_DM_RW	0x10	/* DMA: Remote Write                 */
#define CR_DM_SP	0x18	/* DMA: Send Packet                  */
#define CR_DM_ABORT	0x20	/* DMA: Abort Remote DMA Operation   */
#define CR_PS		0xC0	/* Mask for Page Select              */
#define CR_PS_P0	0x00	/* Register Page 0                   */
#define CR_PS_P1	0x40	/* Register Page 1                   */
#define CR_PS_P2	0x80	/* Register Page 2                   */
#define CR_PS_T1	0xC0	/* Test Mode Register Map            */

/* Bits in dp_isr */
#define ISR_PRX		0x01	/* Packet Received with no errors    */
#define ISR_PTX		0x02	/* Packet Transmitted with no errors */
#define ISR_RXE		0x04	/* Receive Error                     */
#define ISR_TXE		0x08	/* Transmit Error                    */
#define ISR_OVW		0x10	/* Overwrite Warning                 */
#define ISR_CNT		0x20	/* Counter Overflow                  */
#define ISR_RDC		0x40	/* Remote DMA Complete               */
#define ISR_RST		0x80	/* Reset Status                      */

/* Bits in dp_imr */
#define IMR_PRXE	0x01	/* Packet Received iEnable           */
#define IMR_PTXE	0x02	/* Packet Transmitted iEnable        */
#define IMR_RXEE	0x04	/* Receive Error iEnable             */
#define IMR_TXEE	0x08	/* Transmit Error iEnable            */
#define IMR_OVWE	0x10	/* Overwrite Warning iEnable         */
#define IMR_CNTE	0x20	/* Counter Overflow iEnable          */
#define IMR_RDCE	0x40	/* DMA Complete iEnable              */

/* Bits in dp_dcr */
#define DCR_WTS		0x01	/* Word Transfer Select              */
#define DCR_BYTEWIDE	0x00	/* WTS: byte wide transfers          */
#define DCR_WORDWIDE	0x01	/* WTS: word wide transfers          */
#define DCR_BOS		0x02	/* Byte Order Select                 */
#define DCR_LTLENDIAN	0x00	/* BOS: Little Endian                */
#define DCR_BIGENDIAN	0x02	/* BOS: Big Endian                   */
#define DCR_LAS		0x04	/* Long Address Select               */
#define DCR_BMS		0x08	/* Burst Mode Select
				 * Called Loopback Select (LS) in 
				 * later manuals. Should be set.     */
#define DCR_AR		0x10	/* Autoinitialize Remote             */
#define DCR_FTS		0x60	/* Fifo Threshold Select             */
#define DCR_2BYTES	0x00	/* 2 bytes                           */
#define DCR_4BYTES	0x40	/* 4 bytes                           */
#define DCR_8BYTES	0x20	/* 8 bytes                           */
#define DCR_12BYTES	0x60	/* 12 bytes                          */

/* Bits in dp_tcr */
#define TCR_CRC		0x01	/* Inhibit CRC                       */
#define TCR_ELC		0x06	/* Encoded Loopback Control          */
#define TCR_NORMAL	0x00	/* ELC: Normal Operation             */
#define TCR_INTERNAL	0x02	/* ELC: Internal Loopback            */
#define TCR_0EXTERNAL	0x04	/* ELC: External Loopback LPBK=0     */
#define TCR_1EXTERNAL	0x06	/* ELC: External Loopback LPBK=1     */
#define TCR_ATD		0x08	/* Auto Transmit Disable             */
#define TCR_OFST	0x10	/* Collision Offset Enable (be nice) */

/* Bits in dp_tsr */
#define TSR_PTX		0x01	/* Packet Transmitted (without error)*/
#define TSR_DFR		0x02	/* Transmit Deferred, reserved in
				 * later manuals.		     */
#define TSR_COL		0x04	/* Transmit Collided                 */
#define TSR_ABT		0x08	/* Transmit Aborted                  */
#define TSR_CRS		0x10	/* Carrier Sense Lost                */
#define TSR_FU		0x20	/* FIFO Underrun                     */
#define TSR_CDH		0x40	/* CD Heartbeat                      */
#define TSR_OWC		0x80	/* Out of Window Collision           */

/* Bits in tp_rcr */
#define RCR_SEP		0x01	/* Save Errored Packets              */
#define RCR_AR		0x02	/* Accept Runt Packets               */
#define RCR_AB		0x04	/* Accept Broadcast                  */
#define RCR_AM		0x08	/* Accept Multicast                  */
#define RCR_PRO		0x10	/* Physical Promiscuous              */
#define RCR_MON		0x20	/* Monitor Mode                      */

/* Bits in dp_rsr */
#define RSR_PRX		0x01	/* Packet Received Intact            */
#define RSR_CRC		0x02	/* CRC Error                         */
#define RSR_FAE		0x04	/* Frame Alignment Error             */
#define RSR_FO		0x08	/* FIFO Overrun                      */
#define RSR_MPA		0x10	/* Missed Packet                     */
#define RSR_PHY		0x20	/* Multicast Address Match           */
#define RSR_DIS		0x40	/* Receiver Disabled                 */
#define RSR_DFR		0x80	/* In later manuals: Deferring       */

typedef struct dp_rcvhdr
{
	u8_t dr_status;			/* Copy of rsr                       */
	u8_t dr_next;			/* Pointer to next packet            */
	u8_t dr_rbcl;			/* Receive Byte Count Low            */
	u8_t dr_rbch;			/* Receive Byte Count High           */
} dp_rcvhdr_t;

#define DP_PAGESIZE	256

/* Some macros to simplify accessing the dp8390 */
#define inb_reg0(dep, reg)		(inb(dep->de_dp8390_port+reg))
#define outb_reg0(dep, reg, data)	(outb(dep->de_dp8390_port+reg, data))
#define inb_reg1(dep, reg)		(inb(dep->de_dp8390_port+reg))
#define outb_reg1(dep, reg, data)	(outb(dep->de_dp8390_port+reg, data))

/* Software interface to the dp8390 driver */

struct dpeth;
struct iovec_dat;
struct iovec_dat_s;
typedef void (*dp_initf_t)(struct dpeth *dep);
typedef void (*dp_stopf_t)(struct dpeth *dep);
typedef void (*dp_user2nicf_s_t)(struct dpeth *dep,
	struct netdriver_data *data, int nic_addr, size_t offset,
	size_t count);
typedef void (*dp_nic2userf_s_t)(struct dpeth *dep,
	struct netdriver_data *data, int nic_addr, size_t offset,
	size_t count);
typedef void (*dp_getblock_t)(struct dpeth *dep, int page, size_t
	offset, size_t size, void *dst);

typedef int irq_hook_t;

#define SENDQ_NR	2	/* Maximum size of the send queue */
#define SENDQ_PAGES	6	/* 6 * DP_PAGESIZE >= 1514 bytes */

typedef struct dpeth
{
	/* The de_base_port field is the starting point of the probe.
	 * The conf routine also fills de_linmem and de_irq. If the probe
	 * routine knows the irq and/or memory address because they are
	 * hardwired in the board, the probe should modify these fields.
	 * Futhermore, the probe routine should also fill in de_initf and
	 * de_stopf fields with the appropriate function pointers and set
	 * de_prog_IO iff programmed I/O is to be used.
	 */
	port_t de_base_port;
	phys_bytes de_linmem;
	char *de_locmem;
	int de_irq;
	int de_int_pending;
	irq_hook_t de_hook;
	dp_initf_t de_initf; 
	dp_stopf_t de_stopf; 
	int de_prog_IO;

	/* The initf function fills the following fields. Only cards that do
	 * programmed I/O fill in the de_pata_port field.
	 * In addition, the init routine has to fill in the sendq data
	 * structures.
	 */
	netdriver_addr_t de_address;
	port_t de_dp8390_port;
	port_t de_data_port;
	int de_16bit;
	unsigned int de_ramsize;
	unsigned int de_offset_page;
	unsigned int de_startpage;
	unsigned int de_stoppage;

	/* PCI config */
	char de_pci;			/* TRUE iff PCI device */

	/* Do it yourself send queue */
	struct sendq
	{
		int sq_filled;		/* this buffer contains a packet */
		int sq_size;		/* with this size */
		int sq_sendpage;	/* starting page of the buffer */
	} de_sendq[SENDQ_NR];
	int de_sendq_nr;
	int de_sendq_head;		/* Enqueue at the head */
	int de_sendq_tail;		/* Dequeue at the tail */

	/* Fields for internal use by the dp8390 driver. */
	int de_flags;
	dp_user2nicf_s_t de_user2nicf_s; 
	dp_nic2userf_s_t de_nic2userf_s; 
	dp_getblock_t de_getblockf; 
} dpeth_t;

#define DEI_DEFAULT	0x8000

#define DEF_EMPTY	0x00
#define DEF_STOPPED	0x01

#define debug		0

/*
 * $PchId: dp8390.h,v 1.10 2005/02/10 17:26:06 philip Exp $
 */

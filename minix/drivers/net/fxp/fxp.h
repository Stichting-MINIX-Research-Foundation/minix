/*
ibm/fxp.h

Registers and datastructures of the Intel 82557, 82558, 82559, 82550,
and 82562 fast ethernet controllers.

Created:	Nov 2004 by Philip Homburg <philip@f-mnx.phicoh.com>
*/

#ifndef _FXP_FXP_H
#define _FXP_FXP_H

#define VERBOSE 	0	/* display output during intialization */

/* Revisions in PCI_REV */
#define FXP_REV_82557A		0x01
#define FXP_REV_82557B		0x02
#define FXP_REV_82557C		0x03
#define FXP_REV_82558A		0x04
#define FXP_REV_82558B		0x05
#define FXP_REV_82559A		0x06
#define FXP_REV_82559B		0x07
#define FXP_REV_82559C		0x08
#define FXP_REV_82559ERA	0x09
#define FXP_REV_82550_1 	0x0C
#define FXP_REV_82550_2		0x0D
#define FXP_REV_82550_3		0x0E
#define FXP_REV_82551_1		0x0F
#define FXP_REV_82551_2		0x10
#define FXP_REV_82801CAM	0x42
#define FXP_REV_82801DB		0x81
#define FXP_REV_82550_4		0x83

/* Control/Status Registers (CSR). The first 8 bytes are called
 * System Control Block (SCB)
 */
#define SCB_STATUS	0x00	/* Lower half of the SCB status word. CU and
				 * RU status.
				 */
#define	    SS_CUS_MASK		0xC0	/* CU Status */
#define		SS_CU_IDLE		0x00	/* Idle */
#define		SS_CU_SUSP		0x40	/* Suspended */
#define		SS_CU_LPQA		0x80	/* LPQ Active */
#define		SS_CU_HQPA		0xC0	/* HQP Active */
#define	    SS_RUS_MASK		0x3C	/* RU Status */
#define		SS_RU_IDLE		0x00	/* Idle */
#define		SS_RU_SUSP		0x04	/* Suspended */
#define		SS_RU_NORES		0x08	/* No Resources */
#define		SS_RU_READY		0x10	/* Ready */
						/* Other values are reserved */
#define	    SS_RESERVED		0x03	/* Reserved */
#define SCB_INT_STAT	0x01	/* Upper half of the SCB status word.
				 * Interrupt status. Also used to acknoledge
				 * interrupts.
				 */
#define		SIS_CX	0x80	/* CU command with interrupt bit set. On
				 * 82557 also TNO Interrupt.
				 */
#define		SIS_FR	0x40	/* Frame Received */
#define		SIS_CNA	0x20	/* CU Not Active */
#define		SIS_RNR	0x10	/* RU Not Ready */
#define		SIS_MDI	0x08	/* MDI read/write cycle completed */
#define		SIS_SWI	0x04	/* Software Interrupt */
#define		SIS_RES	0x02	/* Reserved */
#define		SIS_FCP	0x01	/* Flow Control Pause Interrupt (82558 and
				 * later, reserved on 82557)
				 */
#define SCB_CMD		0x02	/* Lower half of the SCB command word. CU and
				 * RU commands.
				 */
#define	    SC_CUC_MASK		0xF0
#define		SC_CU_NOP		0x00	/* NOP */
#define		SC_CU_START		0x10	/* Start CU */
#define		SC_CU_RESUME		0x20	/* Resume CU */
#define		SC_CU_LOAD_DCA		0x40	/* Load Dump Counters Address */
#define		SC_CU_DUMP_SC		0x50	/* Dump Statistical Counters */
#define		SC_CU_LOAD_BASE		0x60	/* Load CU Base */
#define		SC_CU_DUMP_RSET_SC	0x70	/* Dump and Reset Counters */
#define		SC_CU_STATIC_RESUME	0xA0	/* Static Resume, 82558 and
						 * above
						 */
#define	    SC_RESERVED		0x08	/* Reserved */
#define	    SC_RUC_MASK		0x07	/* RU Command Mask */
#define		SC_RU_NOP		0x00	/* NOP */
#define		SC_RU_START		0x01	/* Start RU */
#define		SC_RU_RESUME		0x02	/* Resume RU */
#define		SC_RU_DMA_REDIR		0x03	/* DMA Redirect */
#define		SC_RU_ABORT		0x04	/* Abort RU */
#define		SC_RU_LOAD_HDR		0x05	/* Load Header Data Size */
#define		SC_RU_LOAD_BASE		0x06	/* Load RU Base */
#define SCB_INT_MASK	0x03	/* Upper half of the SCB command word. 
				 * Interrupt mask. Can also be used to
				 * generate a 'software' interrupt.
				 */
				/* The following 6 mask bits are not valid on
				 * the 82557.
				 */
#define	    SIM_CX	0x80	/* Mask CX */
#define	    SIM_FR	0x40	/* Mask FR */
#define	    SIM_CNA	0x20	/* Mask CNA */
#define	    SIM_RNR	0x10	/* Mask RNR */
#define	    SIM_ER	0x08	/* Mask ER */
#define	    SIM_FCP	0x04	/* Mask FCP */
#define	    SIM_SI	0x02	/* Generate Software Interrupt */
#define	    SIM_M	0x01	/* Mask all interrupts */
#define SCB_POINTER	0x04	/* A 32-bit (pointer) argument for CU and RU
				 * commands.
				 */
#define CSR_PORT	0x08	/* Control functions that bypass the SCB */
#define	    CP_PTR_MASK		0xFFFFFFF0	/* Argument pointer */
#define	    CP_CMD_MASK		0x0000000F	/* Commands bits */
#define		CP_CMD_SOFT_RESET	0x00000000	/* Software reset */
#define	    	    CSR_PORT_RESET_DELAY	10	/* Wait for reset to
							 * complete. In micro
							 * seconds.
							 */
#define		CP_CMD_SELF_TEST	0x00000001	/* Self test */
#define		CP_CMD_SEL_RESET	0x00000002	/* Selective reset */
#define		CP_CMD_DUMP		0x00000003	/* Dump */
#define		CP_CMD_DUMP_WAKEUP	0x00000007	/* Dump and wake-up,
							 * 82559 and later.
							 */
#define	CSR_RESERVED	0x0C	/* reserved, 16-bits */
#define CSR_EEPROM	0x0E	/* EEPROM Control Register */
#define	    CE_RESERVED	0xF0	/* Reserved */
#define	    CE_EEDO	0x08	/* Serial Data Out  (of the EEPROM) */
#define	    CE_EEDI	0x04	/* Serial Data In (to the EEPROM) */
#define	    CE_EECS	0x02	/* Chip Select */
#define	    CE_EESK	0x01	/* Serial Clock */
#define CSR_RESERVED1	0x0F	/* Reserved */
#define CSR_MDI_CTL	0x10	/* MDI Control Register, 32-bits */
#define	    CM_RESERVED		0xC0000000	/* Reserved */
#define	    CM_IE		0x20000000	/* Enable Interrupt */
#define	    CM_READY		0x10000000	/* Command completed */
#define	    CM_OPCODE_MASK	0x0C000000	/* Opcode */
#define		CM_WRITE		0x04000000	/* Write */
#define		CM_READ			0x08000000	/* Read */
#define	    CM_PHYADDR_MASK	0x03E00000	/* Which PHY */
#define		CM_PHYADDR_SHIFT 21
#define	    CM_REG_MASK		0x001F0000	/* Which register in the PHY */
#define	    	CM_REG_SHIFT	16
#define	    CM_DATA_MASK	0x0000FFFF	/* Data to be read or written */

/* Control Block List (CBL) commands */
#define CBL_NOP		0	/* No-operation */
#define CBL_AIS		1	/* Individual Address Setup */
#define CBL_CONF	2	/* Configure NIC */
#define CBL_MAS		3	/* Multicast Address Setup */
#define CBL_XMIT	4	/* Transmit */
#define CBL_LM		5	/* Load Microcode */
#define CBL_DUMP	6	/* Dump Internal Registers */
#define CBL_DIAG	7	/* Diagnose Command */

/* Common command fields */
#define CBL_C_CMD_MASK	0x0007	/* Command bits */
#define CBL_C_EL	0x8000	/* End of CBL */
#define CBL_C_S		0x4000	/* Suspend after the completion of the CB */
#define CBL_C_I		0x2000	/* Request CX Interrupt */
#define CBL_C_RES	0x1FF8	/* Reserved */

/* Command flags */
#define CBL_F_C		0x8000	/* Command has completed */
#define CBL_F_RES1	0x4000	/* Reserved */
#define CBL_F_OK	0x2000	/* Command was executed without errors */
#define CBL_F_RES0	0x1FFF	/* Reserved */

/* Individual Address Setup (1) */
struct ias
{
	u16_t ias_status;
	u16_t ias_command;
	u32_t ias_linkaddr;
	u8_t ias_ethaddr[6];
	u8_t ias_reserved[2];
};

/* Configure (2) */
#define CC_BYTES_NR	22	/* Number of configuration bytes */
struct cbl_conf
{
	u16_t cc_status;
	u16_t cc_command;
	u32_t cc_linkaddr;
	u8_t cc_bytes[CC_BYTES_NR];
};

/* Byte 0 */
#define CCB0_RES	0xC0	/* Reserved (0) */
#define CCB0_BYTECOUNT	0x3F	/* Byte Count (typically either 8 or 22) */

/* Byte 1 */
#define CCB1_RES	0x80	/* Reserved (0) */
#define CCB1_TXFIFO_LIM	0x70	/* Transmit FIFO Limit, in DWORDS */
#define		CTL_DEFAULT	0x00	/* 0 bytes */
#define CCB1_RXFIFO_LIM	0x0F	/* Receive FIFO Limit */
#define		CRL_DEFAULT	0x08	/* 32 bytes on 82557, 64 bytes on
					 * 82558/82559.
					 */

/* Byte 2 */
#define CCB2_AIFS	0xFF	/* Adaptive IFS */
#define		CAI_DEFAULT	0

/* Byte 3 */
				/* Reserved (must be 0) on 82557 */
#define CCB3_RES	0xF0	/* Reserved (0) */
#define	CCB3_TWCL	0x08	/* Terminate Write on Cache Line */
#define	CCB3_RAE	0x04	/* Read Alignment Enable */
#define	CCB3_TE		0x02	/* Type Enable??? */
#define	CCB3_MWIE	0x01	/* Memory Write and Invalidate (MWI) Enable
				 * Additionally the MWI bit in the PCI
				 * command register has to be set.
				 * Recommended by Intel.
				 */

/* Byte 4 */
#define CCB4_RES	0x80	/* Reserved (0) */
#define CCB4_RXDMA_MAX	0x7F	/* Receive DMA Maximum Byte Count */

/* Byte 5 */
#define CCB5_DMBCE	0x80	/* DMA Maximum Byte Count Enable */
#define CCB5_TXDMA_MAX	0x7F	/* Transmit DMA Maximum Byte Count */

/* Byte 6 */
#define CCB6_SBF	0x80	/* Save Bad Frames */
#define CCB6_DORF	0x40	/* (Do not) Discard Overrun Receive Frame,
				 * Set this bit to keep them.
				 */
#define CCB6_ESC	0x20	/* Extended Statistical Counter. Reserved
				 * on 82557, must be set to 1.
				 * Clear this bit to get more counters.
				 */
#define CCB6_ETCB	0x10	/* Extended Transmit CB. Reserved on 82557,
				 * must be set to 1.
				 * Clear this bit to use Extended TxCBs.
				 */
#define CCB6_CI_INT	0x08	/* CPU Idle (CI) Interrupt. Generate a
				 * CI Int (bit set) or a CNA Int (bit clear)
				 * when the CU goes to the idle state (or
				 * to suspended for CNA).
				 */
#define CCB6_TNO_INT	0x04	/* Enable TNO Interrupt (82557 only) */
#define CCB6_TCOSC	0x04	/* TCO Statistical Counter (82559 only) */
#define CCB6_RES	0x02	/* Reserved, must be set to 1. Called "disable
				 * direct rcv dma mode" by the FreeBSD
				 * driver.
				 */
#define CCB6_LSCB	0x01	/* Late SCB Update. Only on 82557. */

/* Byte 7 */
#define CCB7_DTBD	0x80	/* Dynamic TBD. Reserved on 82557, should be
				 * be set to 0.
				 */
#define CCB7_2FFIFO	0x40	/* (At Most) Two Frames in FIFO. Reserved on
				 * 82557, should be set to 0.
				 */
#define CCB7_RES	0x38	/* Reserved (0) */
#define CCB7_UR		0x06	/* Underrun Retry */
#define	    CUR_0		0x00	/* No re-transmission */
#define	    CUR_1		0x02	/* One re-transmission */
#define	    CUR_2		0x04	/* Two re-transmissions, 1st retry with
					 * 512 bytes.
					 */
#define	    CUR_3		0x06	/* Tree re-transmissions, 1st retry
					 * with 512 bytes, 2nd retry with 1024.
					 */
#define CCB7_DSRF	0x01	/* Discard Short Receive Frames. */

/* Byte 8 */
#define CCB8_CSMAD	0x80	/* CSMA Disable. Reserved on 82557, should be
				 * set to zero.
				 */
#define CCB8_RES	0x7E	/* Reserved (0) */
#define CCB8_503_MII	0x01	/* 503 mode or MII mode. Reserved on 82558
				 * and 82559, should be set to 1.
				 */

/* Byte 9 */
#define CCB9_MMWE	0x80	/* Multicast Match Wake Enable. 82558 B-step
				 * only, should be set to zero on other
				 * devices.
				 */
#define CCB9_AWE	0x40	/* ARP Wake-up Enable. 82558 B-step only,
				 * should be set to zero on other devices.
				 */
#define CCB9_LSCWE	0x20	/* Link Status Change Wake Enable. Available
				 * on 82558 B-step and 82559. Should be
				 * set to zero on 82557 and 82558 A-step
				 */
#define CCB9_VARP	0x10	/* VLAN ARP (82558 B-step) or VLAN TCO (82559).
				 * Should be zero on 82557 and 82558 A-step
				 */
#define CCB9_RES	0x0E	/* Reserved (0) */
#define CCB9_TUC	0x01	/* TCP/UDP Checksum. 82559 only, should be
				 * zero on other devices.
				 */

/* Byte 10 */
#define CCB10_LOOPBACK	0xC0	/* Loopback mode */
#define	    CLB_NORMAL		0x00	/* Normal operation */
#define	    CLB_INTERNAL	0x40	/* Internal loopback */
#define	    CLB_RESERVED	0x80	/* Reserved */
#define	    CLB_EXTERNAL	0xC0	/* External loopback */
#define CCB10_PAL	0x30	/* Pre-amble length */
#define	    CPAL_1		0x00	/* 1 byte */
#define	    CPAL_3		0x10	/* 3 bytes */
#define	    CPAL_7		0x20	/* 7 bytes */
#define	    CPAL_15		0x30	/* 15 bytes */
#define	    CPAL_DEFAULT	CPAL_7
#define CCB10_NSAI		0x08	/* No Source Address Insertion */
#define CCB10_RES1		0x06	/* Reserved, should be set to 1 */
#define CCB10_RES0		0x01	/* Reserved (0) */

/* Byte 11 */
#define CCB11_RES		0xF8	/* Reserved (0) */
#define CCB11_LINPRIO		0x07	/* Linear Priority. 82557 only,
					 * should be zero on other devices.
					 */

/* Byte 12 */
#define CCB12_IS		0xF0	/* Interframe spacing in multiples of
					 * 16 bit times.
					 */
#define	    CIS_DEFAULT			0x60	/* 96 (6 in register) */
#define	CCB12_RES		0x0E	/* Reserved (0) */
#define	CCB12_LPM		0x01	/* Linear Priority Mode. 82557 only,
					 * should be zero on other devices.
					 */

/* Byte 13, 4th byte of IP address for ARP frame filtering. Only valid on
 * 82558 B-step. Should be 0 on other devices.
 */
#define CCB13_DEFAULT		0x00
/* Byte 14, 3rd byte of IP address for ARP fram efiltering. Only valid on
 * 82558 B-step. Should be 0xF2 on other devices.
 */
#define CCB14_DEFAULT		0xF2

/* Byte 15 */
#define CCB15_CRSCDT		0x80	/* CRS or CDT. */
#define CCB15_RES1		0x40	/* Reserved, should be set to one. */
#define CCB15_CRC16		0x20	/* 16-bit CRC. Only on 82559,
					 * should be zero on other devices
					 */
#define CCB15_IUL		0x10	/* Ignore U/L. Reserved on 82557 and
					 * should be set to zero.
					 */
#define CCB15_RES2		0x08	/* Reserved, should be set to one. */
#define CCB15_WAW		0x04	/* Wait After Win. Reserved on 82557,
					 * should be set to zero.
					 */
#define CCB15_BD		0x02	/* Broadcast disable */
#define CCB15_PM		0x01	/* Promiscuous mode */

/* Byte 16. FC Delay Least Significant Byte. Reserved on the 82557 and
 * should be set to zero.
 */
#define CCB16_DEFAULT		0x00

/* Byte 17. FC Delay Most Significant Byte. This byte is reserved on the
 * 82557 and should be set to 0x40.
 */
#define CCB17_DEFAULT		0x40

/* Byte 18 */
#define CCB18_RES1		0x80	/* Reserved, should be set to 1 */
#define CCB18_PFCT		0x70	/* Priority Flow Control Threshold.
					 * Reserved on the 82557 and should
					 * be set to 1. All bits 1 (disabled)
					 * is the recommended default.
					 */
#define CCB18_LROK		0x08	/* Long Receive OK. Reserved on the
					 * 82557 and should be set to zero.
					 * Required for VLANs.
					 */
#define CCB18_RCRCT		0x04	/* Receive CRC Transfer */
#define CCB18_PE		0x02	/* Padding Enable */
#define CCB18_SE		0x01	/* Stripping Enable */

/* Byte 19 */
#define CCB19_FDPE		0x80	/* Full Duplex Pin Enable */
#define CCB19_FFD		0x40	/* Force Full Duplex */
#define CCB19_RFC		0x20	/* Reject FC. Reserved on the 82557
					 * and should be set to zero.
					 */
#define CCB19_FDRSTAFC		0x10	/* Full Duplex Restart Flow Control.
					 * Reserved on the 82557 and should be
					 * set to zero.
					 */
#define CCB19_FDRSTOFC		0x08	/* Full Duplex Restop Flow Control.
					 * Reserved on the 82557 and should be
					 * set to zero.
					 */
#define CCB19_FDTFCD		0x04	/* Full Duplex Transmit Flow Control
					 * Disable. Reserved on the 82557 and
					 * should be set to zero.
					 */
#define CCB19_MPWD		0x02	/* Magic Packet Wake-up Disable.
					 * Reserved on the 82557 and 82559ER
					 * and should be set to zero.
					 */
#define CCB19_AW		0x01	/* Address Wake-up (82558 A-step) and
					 * IA Match Wake Enable (82558 B-step)
					 * Reserved on the 82557 and 82559 and
					 * should be set to zero.
					 */

/* Byte 20 */
#define CCB20_RES		0x80	/* Reserved (0) */
#define CCB20_MIA		0x40	/* Multiple IA */
#define CCB20_PFCL		0x20	/* Priority FC Location. Reserved on
					 * the 82557 and should be set to 1.
					 */
#define CCB20_RES1		0x1F	/* Reserved, should be set to 1 */

/* Byte 21 */
#define CCB21_RES		0xF0	/* Reserved (0) */
#define CCB21_MA		0x08	/* Multicast All */
#define CCB21_RES1_MASK		0x07	/* Reserved, should be set to 5 */
#define     CCB21_RES21			0x05

/* Transmit (4) */
struct tx
{
	u16_t tx_status;
	u16_t tx_command;
	u32_t tx_linkaddr;
	u32_t tx_tbda;
	u16_t tx_size;
	u8_t tx_tthresh;
	u8_t tx_ntbd;
	u8_t tx_buf[NDEV_ETH_PACKET_MAX_TAGGED];
};

#define TXS_C		0x8000	/* Transmit DMA has completed */
#define TXS_RES		0x4000	/* Reserved */
#define TXS_OK		0x2000	/* Command was executed without error */
#define TXS_U		0x1000	/* This or previous frame encoutered underrun */
#define TXS_RES1	0x0FFF	/* Reserved (0) */

#define TXC_EL		0x8000	/* End of List */
#define TXC_S		0x4000	/* Suspend after this CB */
#define TXC_I		0x2000	/* Interrupt after this CB */
#define TXC_CID_MASK	0x1F00	/* CNA Interrupt Delay */
#define TXC_RES		0x00E0	/* Reserved (0) */
#define TXC_NC		0x0010	/* No CRC and Source Address Insertion */
#define TXC_SF		0x0008	/* Not in Simplified Mode */
#define TXC_CMD		0x0007	/* Command */

#define TXSZ_EOF	0x8000	/* End of Frame */
#define TXSZ_RES	0x4000	/* Reserved (0) */
#define TXSZ_COUNT	0x3FFF	/* Transmit Byte Count */

#define TX_TBDA_NIL	0xFFFFFFFF	/* Null Pointer for TBD Array */

#define TXTT_MIN	0x01	/* Minimum for Transmit Threshold */
#define TXTT_MAX	0xE0	/* Maximum for Transmit Threshold */

/* Statistical Counters */
struct sc
{
	u32_t sc_tx_good;	/* Transmit Good Frames */
	u32_t sc_tx_maxcol;	/* Transmit Maximum Collisions errors */
	u32_t sc_tx_latecol;	/* Transmit Late Collisions errors */
	u32_t sc_tx_underrun;	/* Transmit Underrun errors */
	u32_t sc_tx_crs;	/* Transmit Lost Carrier Sense */
	u32_t sc_tx_defered;	/* Transmit Defered */
	u32_t sc_tx_scol;	/* Transmit Single Collision */
	u32_t sc_tx_mcol;	/* Transmit Multiple Collisions */
	u32_t sc_tx_totcol;	/* Transmit Total Collisions */
	u32_t sc_rx_good;	/* Receive Good Frames */
	u32_t sc_rx_crc;	/* Receive CRC errors */
	u32_t sc_rx_align;	/* Receive Alignment errors */
	u32_t sc_rx_resource;	/* Receive Resource errors */
	u32_t sc_rx_overrun;	/* Receive Overrun errors */
	u32_t sc_rx_cd;		/* Receive Collision Detect errors */
	u32_t sc_rx_short;	/* Receive Short Frame errors */

				/* Short form ends here. The magic number will
				 * be stored in the next field.
				 */

	u32_t sc_tx_fcp;	/* Transmit Flow Control Pause */
	u32_t sc_rx_fcp;	/* Receive Flow Control Pause */
	u32_t sc_rx_fcu;	/* Receive Flow Control Unsupported */

				/* Longer form (82558 and later) ends here.
				 * The magic number will be stored in the
				 * next field.
				 */

	u32_t sc_tx_tco;	/* Transmit TCO frames */
	u32_t sc_rx_tco;	/* Receive TCO frames */
	u32_t sc_magic;		/* Dump of counters completed */
};

#define SCM_DSC		0x0000A005	/* Magic for SC_CU_DUMP_SC command */
#define SCM_DRSC	0x0000A007	/* Magic for SC_CU_DUMP_RSET_SC cmd */

/* Receive Frame Descriptor (RFD) */
struct rfd
{
	u16_t rfd_status;
	u16_t rfd_command;
	u32_t rfd_linkaddr;
	u32_t rfd_reserved;
	u16_t rfd_res;
	u16_t rfd_size;
	u8_t rfd_buf[NDEV_ETH_PACKET_MAX_TAGGED];
};

#define RFDS_C		0x8000	/* Frame Reception Completed */
#define RFDS_RES	0x4000	/* Reserved (0) */
#define RFDS_OK		0x2000	/* Frame received without any errors */
#define RFDS_RES1	0x1000	/* Reserved */
#define RFDS_CRCERR	0x0800	/* CRC error */
#define RFDS_ALIGNERR	0x0400	/* Alignment error */
#define RFDS_OUTOFBUF	0x0200	/* Ran out of buffer space (frame is frager
				 * than supplied buffer).
				 */
#define RFDS_DMAOVR	0x0100	/* DMA overrun failure */
#define RFDS_TOOSHORT	0x0080	/* Frame Too Short */
#define RFDS_RES2	0x0040	/* Reserved */
#define RFDS_TYPED	0x0020	/* Frame Is Typed (Type/Length field is 0 or
				 * >1500)
				 */
#define RFDS_RXERR	0x0010	/* Receive Error */
#define RFDS_RES3	0x0008	/* Reserved */
#define RFDS_NOAM	0x0004	/* No Address Match */
#define RFDS_NOAIAM	0x0002	/* No IA Address Match */
#define RFDS_RXCOL	0x0001	/* Collition Detected During Reception (82557
				 * and 82558 only)
				 */
#define RFDS_TCO	0x0001	/* TCO Packet (82559 and later) */

#define RFDC_EL		0x8000	/* End of List */
#define RFDC_S		0x4000	/* Suspend */
#define RFDC_RES	0x3FE0	/* Reserved (0) */
#define RFDC_H		0x0010	/* Header RFD */
#define RFDC_SF		0x0008	/* (Not) Simplified Mode */
#define RFDC_RES1	0x0007	/* Reserved (0) */

#define RFDR_EOF	0x8000	/* End of Frame (all data is in the buffer) */
#define RFDR_F		0x4000	/* Finished updating the count field */
#define RFDR_COUNT	0x3FFF	/* Actual Count */

#define RFDSZ_RES	0xC000	/* Reserved (0) */
#define RFDSZ_SIZE	0x3FFF	/* Buffer Size */

/* EEPROM commands */
#define EEPROM_READ_PREFIX	0x6	/* Read command */
#define EEPROM_PREFIX_LEN	3	/* Start bit and two command bits */

/* EEPROM timing parameters */
#define EECS_DELAY	1	/* Keep EECS low for at least EECS_DELAY
				 * microseconds
				 */
#define EESK_PERIOD	4	/* A cycle of driving EESK high followed by
				 * driving EESK low should take at least
				 * EESK_PERIOD microseconds
				 */

/* Special registers in the 82555 (and compatible) PHYs. Should be moved
 * to a separate file if other drivers need this too.
 */
#define MII_SCR		0x10	/* Status and Control Register */
#define	    MII_SCR_FC		0x8000	/* Flow Control */
#define	    MII_SCR_T4E		0x4000	/* Enable T4 unless auto-negotiation */
#define	    MII_SCR_CRSDC	0x2000	/* RX100 CRS Disconnect */
#define	    MII_SCR_RES		0x1000	/* Reserved */
#define	    MII_SCR_RCVSYNC	0x0800	/* RCV De-Serializer in sync */
#define	    MII_SCR_100DOWN	0x0400	/* 100Base-T Power Down */
#define	    MII_SCR_10DOWN	0x0200	/* 10Base-T Power Down */
#define	    MII_SCR_POLARITY	0x0100	/* 10Base-T Polarity */
#define	    MII_SCR_RES_1	0x00F8	/* Reserved */
#define	    MII_SCR_T4		0x0004	/* 100Base-T4 negotiated */
#define	    MII_SCR_100		0x0002	/* 100 Mbps negotiated */
#define	    MII_SCR_FD		0x0001	/* Full Duplex negotiated */

/*
 * $PchId: fxp.h,v 1.1 2004/11/23 14:34:03 philip Exp $
 */

#endif

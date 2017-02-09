/*	$NetBSD: mc6854reg.h,v 1.1 2001/09/10 23:41:49 bjh21 Exp $	*/

/*
 * Ben Harris, 2001
 *
 * This file is in the public domain.
 */

/* mc6854reg.h - Motorola 6854 Advanced Data Link Controller registers */

/*
 * The 6854 has two address lines, and uses one of the bits of CR1 as
 * an additional register select.
 */
#define MC6854_CR1	0 /* Control Register #1 (W) */
#define MC6854_CR2	1 /* Control Register #2 (W) (AC = 0) */
#define MC6854_CR3	1 /* Control Register #3 (W) (AC = 1) */
#define MC6854_TXFIFOFC	2 /* Transmit FIFO (Frame Continue) (W) */
#define MC6854_TXFIFOFT	3 /* Transmit FIFO (Frame Terminate) (W) (AC = 0) */
#define MC6854_CR4	3 /* Control Register #4 (W) (AC = 1) */

#define MC6854_SR1	0 /* Status Register #1 (R) */
#define MC6854_SR2	1 /* Status Register #2 (R) */
#define MC6854_RXFIFO	2 /* Receiver FIFO (R) */

/* Control Regsiter #1 bits */
#define MC6854_CR1_AC		0x01 /* Address Control */
#define MC6854_CR1_RIE		0x02 /* Receiver Interrupt Enable */
#define MC6854_CR1_TIE		0x04 /* Transmitter Interrupt Enable */
#define MC6854_CR1_RDSR_MODE	0x08 /* Receiver Data Service Request Mode */
#define MC6854_CR1_TDSR_MODE	0x10 /* Transmitter Data Service Request Mode*/
#define MC6854_CR1_DISCONTINUE	0x20 /* Rx Frame Discontinue */
#define MC6854_CR1_RX_RS	0x40 /* Receiver Reset */
#define MC6854_CR1_TX_RS	0x80 /* Transmitter Reset */
#define MC6854_CR1_BITS \
	"\20\1AC\2RIE\3TIE\4RDSR_MODE\5TDSR_MODE\6DISCONTINUE\7RX_RS\10TX_RS"

/* Control Register #2 bits */
#define MC6854_CR2_PSE		0x01 /* Prioritized Status Enable */
#define MC6854_CR2_2_1_BYTE	0x02 /* 2-Byte/1-Byte Transfer */
#define MC6854_CR2_F_M_IDLE	0x04 /* Flag/Mark Idle Select */
#define MC6854_CR2_FC_TDRA_SEL	0x08 /* Frame Complete/TDRA Select */
#define MC6854_CR2_TX_LAST	0x10 /* Transmit Last Data */
#define MC6854_CR2_CLR_RX_ST	0x20 /* Clear Receiver Status */
#define MC6854_CR2_CLR_TX_ST	0x40 /* Clear Transmitter Status */
#define MC6854_CR2_RTS		0x80 /* Request-to-Send Control */
#define MC6854_CR2_BITS \
	"\20\1PSE\22_1_BYTE\3F_M_IDLE\4RC_TDRA_SEL"	\
	"\5TX_LAST\6CLR_RX_ST\7CLR_TX_ST\10RTS"

/* Control Register #3 bits */
#define MC6854_CR3_LCF		0x01 /* Logical Control Field Select */
#define MC6854_CR3_CEX		0x02 /* Extended Control Field Select */
#define MC6854_CR3_AEX		0x04 /* Auto/Address Extend Mode */
#define MC6854_CR3_00_01_IDLE	0x08 /* 00/01 Idle */
#define MC6854_CR3_FDSE		0x10 /* Flag Detect Status Enable */
#define MC6854_CR3_LOOP		0x20 /* LOOP/NON-LOOP Mode */
#define MC6854_CR3_GAP_TST	0x40 /* Go Active On Poll/Test */
#define MC6854_CR3_LOC_DTR	0x80 /* Loop On-Line Control/DTR Control */
#define MC6854_CR3_BITS \
	"\20\1LCF\2CEX\3AEX\400_01_IDLE\5FDSE\6LOOP\7GAP_TST\10LOC_DTR"

/* Control Register #4 bits */
#define MC6854_CR4_FF_F		0x01 /* Double/Single Flag Interframe Control*/
#define MC6854_CR4_TX_WL_MASK	0x06 /* Transmitter Word Length Select: */
#define MC6854_CR4_TX_WL_5BITS	0x00 /*   5 bits */
#define MC6854_CR4_TX_WL_6BITS	0x02 /*   6 bits */
#define MC6854_CR4_TX_WL_7BITS	0x04 /*   7 bits */
#define MC6854_CR4_TX_WL_8BITS	0x06 /*   8 bits */
#define MC6854_CR4_RX_WL_MASK	0x18 /* Receiver Word Length Select: */
#define MC6854_CR4_RX_WL_5BITS	0x00 /*   5 bits */
#define MC6854_CR4_RX_WL_6BITS	0x08 /*   6 bits */
#define MC6854_CR4_RX_WL_7BITS	0x10 /*   7 bits */
#define MC6854_CR4_RX_WL_8BITS	0x18 /*   8 bits */
#define MC6854_CR4_ABT		0x20 /* Transmit Abort */
#define MC6854_CR4_ABTEX	0x40 /* Abort Extend */
#define MC6854_CR4_NRZI_NRZ	0x80 /* NRZI (Zero Complement)/NRZ Select */

/* Status Register #1 bits */
#define MC6854_SR1_RDA		0x01 /* Receiver Data Available */
#define MC6854_SR1_S2RQ		0x02 /* Status Register #2 Read Request */
#define MC6854_SR1_LOOP		0x04 /* Loop Status */
#define MC6854_SR1_FD		0x08 /* Flag Detected */
#define MC6854_SR1_NCTS		0x10 /* not Clear-to-Send */
#define MC6854_SR1_TXU		0x20 /* Transmitter Underrun */
#define MC6854_SR1_TDRA		0x40 /* Transmitter Data Register Available */
#define MC6854_SR1_FC		0x40 /* Frame Complete */
#define MC6854_SR1_IRQ		0x80 /* Interrupt Request */

#define MC6854_SR1_BITS "\20\1RDA\2S2RQ\3LOOP\4FD\5NCTS\6TXU\7TDRA_FC\10IRQ"

/* Status Register #2 bits */
#define MC6854_SR2_AP		0x01 /* Address Present */
#define MC6854_SR2_FV		0x02 /* Frame Valid */
#define MC6854_SR2_RX_IDLE	0x04 /* Inactive Idle Received */
#define MC6854_SR2_RXABT	0x08 /* Abort Received */
#define MC6854_SR2_ERR		0x10 /* FCS/Invalid Frame Error */
#define MC6854_SR2_NDCD		0x20 /* not Data Carrier Detect */
#define MC6854_SR2_OVRN		0x40 /* Receiver Overrun */
#define MC6854_SR2_RDA		0x80 /* Receiver Data Available */

#define MC6854_SR2_BITS "\20\1AP\2FV\3RX_IDLE\4RXABT\5ERR\6NDCD\7OVRN\10RDA"


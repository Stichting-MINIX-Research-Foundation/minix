/*  $NetBSD: clmpccreg.h,v 1.4 2008/04/28 20:23:49 martin Exp $ */

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Steve C. Woodford.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Register definitions for the Cirrus Logic CD2400/CD2401
 * Four Channel Multi-Protocol Communications Controller.
 *
 * The values defined here are based on the August 1993 data book.
 * At the present time, this driver supports non-DMA async. mode only.
 */

#ifndef __clmpccreg_h
#define __clmpccreg_h

/*
 * Register offsets depend on the level on the chip's BYTESWAP pin.
 * When BYTESWAP is low, Motorola byte alignment is in effect.
 * Otherwise, Intel byte alignment is in effect.
 * The values given here assume BYTESWAP is low. See 'sc_byteswap'
 * <dev/ic/clmpccvar.h>.
 */

/* Number of bytes of FIFO (Rx & Tx) */
#define CLMPCC_FIFO_DEPTH   16

/* Global Registers */
#define CLMPCC_REG_GFRCR    0x81    /* Global Firmware Revision Code Register */
#define CLMPCC_REG_CAR      0xee    /* Channel Access Register */

/* Option Registers */
#define CLMPCC_REG_CMR      0x1b    /* Channel Mode Register */
#define CLMPCC_REG_COR1     0x10    /* Channel Option Register #1 */
#define CLMPCC_REG_COR2     0x17    /* Channel Option Register #2 */
#define CLMPCC_REG_COR3     0x16    /* Channel Option Register #3 */
#define CLMPCC_REG_COR4     0x15    /* Channel Option Register #4 */
#define CLMPCC_REG_COR5     0x14    /* Channel Option Register #5 */
#define CLMPCC_REG_COR6     0x18    /* Channel Option Register #6 */
#define CLMPCC_REG_COR7     0x07    /* Channel Option Register #7 */
#define CLMPCC_REG_SCHR1    0x1f    /* Special Character Register #1 */
#define CLMPCC_REG_SCHR2    0x1e    /* Special Character Register #2 */
#define CLMPCC_REG_SCHR3    0x1d    /* Special Character Register #3 */
#define CLMPCC_REG_SCHR4    0x1c    /* Special Character Register #4 */
#define CLMPCC_REG_SCRl     0x23    /* Special Character Range (low) */
#define CLMPCC_REG_SCRh     0x22    /* Special Character Range (high) */
#define CLMPCC_REG_LNXT     0x2e    /* LNext Character */
#define CLMPCC_REG_RFAR1    0x1f    /* Receive Frame Address Register #1 */
#define CLMPCC_REG_RFAR2    0x1e    /* Receive Frame Address Register #2 */
#define CLMPCC_REG_RFAR3    0x1d    /* Receive Frame Address Register #3 */
#define CLMPCC_REG_RFAR4    0x1c    /* Receive Frame Address Register #4 */
#define CLMPCC_REG_CPSR     0xd6    /* CRC Polynomial Select Register */

/* Bit Rate and Clock Option Registers */
#define CLMPCC_REG_RBPR     0xcb    /* Receive Baud Rate Period Register */
#define CLMPCC_REG_RCOR     0xc8    /* Receive Clock Options Register */
#define CLMPCC_REG_TBPR     0xc3    /* Transmit Baud Rate Period Register */
#define CLMPCC_REG_TCOR     0xc0    /* Transmit Clock Options Register */

/* Channel Command and Status Registers */
#define CLMPCC_REG_CCR      0x13    /* Channel Command Register */
#define CLMPCC_REG_STCR     0x12    /* Special Transmit Command Register */
#define CLMPCC_REG_CSR      0x1a    /* Channel Status Register */
#define CLMPCC_REG_MSVR     0xde    /* Modem Signal Value Register */
#define CLMPCC_REG_MSVR_RTS 0xde    /* Modem Signal Value Register (RTS) */
#define CLMPCC_REG_MSVR_DTR 0xdf    /* Modem Signal Value Register (DTR) */

/* Interrupt Registers */
#define CLMPCC_REG_LIVR     0x09    /* Local Interrupt Vector Register */
#define CLMPCC_REG_IER      0x11    /* Interrupt Enable Register */
#define CLMPCC_REG_LICR     0x26    /* Local Interrupting Channel Register */
#define CLMPCC_REG_STK      0xe2    /* Stack Register */

/* Receive Interrupt Registers */
#define CLMPCC_REG_RPILR    0xe1    /* Receive Priority Interrupt Level Reg */
#define CLMPCC_REG_RIR      0xed    /* Receive Interrupt Register */
#define CLMPCC_REG_RISR     0x88    /* Receive Interrupt Status Reg (16-bits) */
#define CLMPCC_REG_RISRl    0x89    /* Receive Interrupt Status Reg (low) */
#define CLMPCC_REG_RISRh    0x88    /* Receive Interrupt Status Reg (high) */
#define CLMPCC_REG_RFOC     0x30    /* Receive FIFO Output Count */
#define CLMPCC_REG_RDR      0xf8    /* Receive Data Register */
#define CLMPCC_REG_REOIR    0x84    /* Receive End of Interrupt Register */

/* Transmit Interrupt Registers */
#define CLMPCC_REG_TPILR    0xe0    /* Transmit Priority Interrupt Level Reg */
#define CLMPCC_REG_TIR      0xec    /* Transmit Interrupt Register */
#define CLMPCC_REG_TISR     0x8a    /* Transmit Interrupt Status Register */
#define CLMPCC_REG_TFTC     0x80    /* Transmit FIFO Transfer Count */
#define CLMPCC_REG_TDR      0xf8    /* Transmit Data Register */
#define CLMPCC_REG_TEOIR    0x85    /* Transmit End of Interrupt Register */

/* Modem Interrupt Registers */
#define CLMPCC_REG_MPILR    0xe3    /* Modem Priority Interrupt Level Reg */
#define CLMPCC_REG_MIR      0xef    /* Modem Interrupt Register */
#define CLMPCC_REG_MISR     0x8b    /* Modem (/Timer) Interrupt Status Reg */
#define CLMPCC_REG_MEOIR    0x86    /* Modem End of Interrupt Register */

/* DMA Registers */
#define CLMPCC_REG_DMR      0xf6    /* DMA Mode Register (write only) */
#define CLMPCC_REG_BERCNT   0x8e    /* Bus Error Retry Count */
#define CLMPCC_REG_DMABSTS  0x19    /* DMA Buffer Status */

/* DMA Receive Registers */
#define CLMPCC_REG_ARBADRL  0x42    /* A Receive Buffer Address Lower (word) */
#define CLMPCC_REG_ARBADRU  0x40    /* A Receive Buffer Address Upper (word) */
#define CLMPCC_REG_BRBADRL  0x46    /* B Receive Buffer Address Lower (word) */
#define CLMPCC_REG_BRBADRU  0x44    /* B Receive Buffer Address Upper (16bit) */
#define CLMPCC_REG_ARBCNT   0x4a    /* A Receive Buffer Byte Count (word) */
#define CLMPCC_REG_BRBCNT   0x48    /* B Receive Buffer Byte Count (word) */
#define CLMPCC_REG_ARBSTS   0x4f    /* A Receive Buffer Status */
#define CLMPCC_REG_BRBSTS   0x4e    /* B Receive Buffer Status */
#define CLMPCC_REG_RCBADRL  0x3e    /* Receive Current Buff Addr Lower (word) */
#define CLMPCC_REG_RCBADRU  0x3c    /* Receive Current Buff Addr Upper (word) */

/* DMA Transmit Registers */
#define CLMPCC_REG_ATBADRL  0x52    /* A Transmit Buffer Address Lower (word) */
#define CLMPCC_REG_ATBADRU  0x50    /* A Transmit Buffer Address Upper (word) */
#define CLMPCC_REG_BTBADRL  0x56    /* B Transmit Buffer Address Lower (word) */
#define CLMPCC_REG_BTBADRU  0x54    /* B Transmit Buffer Address Upper (word) */
#define CLMPCC_REG_ATBCNT   0x5a    /* A Transmit Buffer Byte Count (word) */
#define CLMPCC_REG_BTBCNT   0x58    /* B Transmit Buffer Byte Count (word) */
#define CLMPCC_REG_ATBSTS   0x5f    /* A Transmit Buffer Status */
#define CLMPCC_REG_BTBSTS   0x5e    /* B Transmit Buffer Status */
#define CLMPCC_REG_TCBADRL  0x3a    /* Transmit Current Buf Addr Lower (word) */
#define CLMPCC_REG_TCBADRU  0x38    /* Transmit Current Buf Addr Upper (word) */

/* Timer Registers */
#define CLMPCC_REG_TPR      0xda    /* Timer Period Register */
#define CLMPCC_REG_RTPR     0x24    /* Receive Timeout Period Register (word) */
#define CLMPCC_REG_RTPRl    0x25    /* Receive Timeout Period Register (low) */
#define CLMPCC_REG_RTPRh    0x24    /* Receive Timeout Period Register (high) */
#define CLMPCC_REG_GT1      0x2a    /* General Timer 1 (word) */
#define CLMPCC_REG_GT1l     0x2b    /* General Timer 1 (low) */
#define CLMPCC_REG_GT1h     0x2a    /* General Timer 1 (high) */
#define CLMPCC_REG_GT2      0x29    /* General Timer 2 */
#define CLMPCC_REG_TTR      0x29    /* Transmit Timer Register */


/* Channel Access Register */
#define CLMPCC_CAR_MASK         0x03        /* Channel bit mask */

/* Channel Mode Register */
#define CLMPCC_CMR_RX_INT       (0 << 7)    /* Rx using interrupts */
#define CLMPCC_CMR_RX_DMA       (1 << 7)    /* Rx using DMA */
#define CLMPCC_CMR_TX_INT       (0 << 6)    /* Tx using interrupts */
#define CLMPCC_CMR_TX_DMA       (1 << 6)    /* Tx using DMA */
#define CLMPCC_CMR_HDLC         0x00        /* Select HDLC mode */
#define CLMPCC_CMR_BISYNC       0x01        /* Select Bisync mode */
#define CLMPCC_CMR_ASYNC        0x02        /* Select async mode */
#define CLMPCC_CMR_X21          0x03        /* Select X.21 mode */

/* Channel Option Register #1 (Async options) */
#define CLMPCC_COR1_EVEN_PARITY (0 << 7)    /* Even parity */
#define CLMPCC_COR1_ODD_PARITY  (1 << 7)    /* Odd parity */
#define CLMPCC_COR1_NO_PARITY   (0 << 5)    /* No parity */
#define CLMPCC_COR1_FORCE_PAR   (1 << 5)    /* Force parity */
#define CLMPCC_COR1_NORM_PARITY (2 << 5)    /* Normal parity */
#define CLMPCC_COR1_CHECK_PAR   (0 << 4)    /* Check parity */
#define CLMPCC_COR1_IGNORE_PAR  (1 << 4)    /* Ignore parity */
#define CLMPCC_COR1_CHAR_5BITS  0x04        /* 5 bits per character */
#define CLMPCC_COR1_CHAR_6BITS  0x05        /* 6 bits per character */
#define CLMPCC_COR1_CHAR_7BITS  0x06        /* 7 bits per character */
#define CLMPCC_COR1_CHAR_8BITS  0x07        /* 8 bits per character */

/* Channel Option Register #2 (Async options) */
#define CLMPCC_COR2_IXM         (1 << 7)    /* Implied XON mode */
#define CLMPCC_COR2_TxIBE       (1 << 6)    /* Transmit In-Band Flow Control */
#define CLMPCC_COR2_ETC         (1 << 5)    /* Embedded Tx Command Enable */
#define CLMPCC_COR2_RLM         (1 << 3)    /* Remote Loopback Mode */
#define CLMPCC_COR2_RtsAO       (1 << 2)    /* RTS Automatic Output Enable */
#define CLMPCC_COR2_CtsAE       (1 << 1)    /* CTS Automatic Enable */
#define CLMPCC_COR2_DsrAE       (1 << 1)    /* DSR Automatic Enable */

/* Embedded transmit commands */
#define	CLMPCC_ETC_MAGIC		0x00		/* Introduces a command */
#define	CLMPCC_ETC_SEND_BREAK	0x81		/* Send a BREAK character */
#define	CLMPCC_ETC_DELAY		0x82		/* Insert a delay */
#define	CLMPCC_ETC_STOP_BREAK	0x83		/* Stop sending BREAK */

/* Channel Option Register #3 (Async options) */
#define CLMPCC_COR3_ESCDE       (1 << 7)    /* Ext Special Char Detect Enab */
#define CLMPCC_COR3_RngDE       (1 << 6)    /* Range Detect Enable */
#define CLMPCC_COR3_FCT         (1 << 5)    /* Flow Ctrl Transparency Mode */
#define CLMPCC_COR3_SCDE        (1 << 4)    /* Special Character Detection */
#define CLMPCC_COR3_SpIstp      (1 << 3)    /* Special Character I Strip */
#define CLMPCC_COR3_STOP_1      0x02        /* 1 Stop Bit */
#define CLMPCC_COR3_STOP_1_5    0x03        /* 1.5 Stop Bits */
#define CLMPCC_COR3_STOP_2      0x04        /* 2 Stop Bits */

/* Channel Option Register #4 */
#define CLMPCC_COR4_DSRzd       (1 << 7)    /* Detect 1->0 transition on DSR */
#define CLMPCC_COR4_CDzd        (1 << 6)    /* Detect 1->0 transition on CD */
#define CLMPCC_COR4_CTSzd       (1 << 5)    /* Detect 1->0 transition on CTS */
#define CLMPCC_COR4_FIFO_MASK   0x0f        /* FIFO Threshold bits */
#define CLMPCC_COR4_FIFO_LOW	1
#define CLMPCC_COR4_FIFO_MED	4
#define CLMPCC_COR4_FIFO_HIGH	8

/* Channel Option Register #5 */
#define CLMPCC_COR5_DSRod       (1 << 7)    /* Detect 0->1 transition on DSR */
#define CLMPCC_COR5_CDod        (1 << 6)    /* Detect 0->1 transition on CD */
#define CLMPCC_COR5_CTSod       (1 << 5)    /* Detect 0->1 transition on CTS */
#define CLMPCC_COR5_FLOW_MASK   0x0f        /* Rx Flow Control FIFO Threshold */
#define CLMPCC_COR5_FLOW_NORM	8

/* Channel Option Register #6 (Async options) */
#define CLMPCC_COR6_RX_CRNL     0x00        /* No special action on CR or NL */
#define CLMPCC_COR6_BRK_EXCEPT  (0 << 3)    /* Exception interrupt on BREAK */
#define CLMPCC_COR6_BRK_2_NULL  (1 << 3)    /* Translate BREAK to NULL char */
#define CLMPCC_COR6_BRK_DISCARD (3 << 3)    /* Discard BREAK characters */
#define CLMPCC_COR6_PF_EXCEPT   0x00        /* Exception irq on parity/frame */
#define CLMPCC_COR6_PF_2_NULL   0x01        /* Translate parity/frame to NULL */
#define CLMPCC_COR6_PF_IGNORE   0x02        /* Ignore error */
#define CLMPCC_COR6_PF_DISCARD  0x03        /* Discard character */
#define CLMPCC_COR6_PF_TRANS    0x05        /* Translate to FF NULL + char */

/* Channel Option Register #7 (Async options) */
#define CLMPCC_COR7_ISTRIP      (1 << 7)    /* Strip MSB */
#define CLMPCC_COR7_LNE         (1 << 6)    /* Enable LNext Option */
#define CLMPCC_COR7_FCERR       (1 << 5)    /* Flow Control on Error Char */
#define CLMPCC_COR7_TX_CRNL     0x00        /* No special action on NL or CR */

/* Receive Clock Options Register */
#define CLMPCC_RCOR_CLK(x)	(x)
#define CLMPCC_RCOR_TLVAL       (1 << 7)    /* Transmit Line Value */
#define CLMPCC_RCOR_DPLL_ENABLE (1 << 5)    /* Phase Locked Loop Enable */
#define CLMPCC_RCOR_DPLL_NRZ    (0 << 3)    /* PLL runs in NRZ mode */
#define CLMPCC_RCOR_DPLL_NRZI   (1 << 3)    /* PLL runs in NRZI mode */
#define CLMPCC_RCOR_DPLL_MAN    (2 << 3)    /* PLL runs in Manchester mode */
#define CLMPCC_RCOR_CLK_0       0x0         /* Rx Clock Source 'Clk0' */
#define CLMPCC_RCOR_CLK_1       0x1         /* Rx Clock Source 'Clk1' */
#define CLMPCC_RCOR_CLK_2       0x2         /* Rx Clock Source 'Clk2' */
#define CLMPCC_RCOR_CLK_3       0x3         /* Rx Clock Source 'Clk3' */
#define CLMPCC_RCOR_CLK_4       0x4         /* Rx Clock Source 'Clk4' */
#define CLMPCC_RCOR_CLK_EXT     0x6         /* Rx Clock Source 'External' */

/* Transmit Clock Options Register */
#define CLMPCC_TCOR_CLK(x)	((x) << 5)
#define CLMPCC_TCOR_CLK_0       (0 << 5)    /* Tx Clock Source 'Clk0' */
#define CLMPCC_TCOR_CLK_1       (1 << 5)    /* Tx Clock Source 'Clk1' */
#define CLMPCC_TCOR_CLK_2       (2 << 5)    /* Tx Clock Source 'Clk2' */
#define CLMPCC_TCOR_CLK_3       (3 << 5)    /* Tx Clock Source 'Clk3' */
#define CLMPCC_TCOR_CLK_4       (4 << 5)    /* Tx Clock Source 'Clk4' */
#define CLMPCC_TCOR_CLK_EXT     (6 << 5)    /* Tx Clock Source 'External' */
#define CLMPCC_TCOR_CLK_RX      (7 << 5)    /* Tx Clock Source 'Same as Rx' */
#define CLMPCC_TCOR_EXT_1X      (1 << 3)    /* Times 1 External Clock */
#define CLMPCC_TCOR_LOCAL_LOOP  (1 << 1)    /* Enable Local Loopback */

/* Special Transmit Command Register */
#define	CLMPCC_STCR_SSPC(n)	((n) & 0x7) /* Send special character 'n' */
#define CLMPCC_STCR_SND_SPC	(1 << 3)    /* Initiate send special char */
#define	CLMPCC_STCR_APPEND_COMP	(1 << 5)    /* Append complete (Async DMA) */
#define CLMPCC_STCR_ABORT_TX	(1 << 6)    /* Abort Tx (HDLC Mode only) */

/* Channel Command Register */
#define CLMPCC_CCR_T0_CLEAR     0x40        /* Type 0: Clear Channel */
#define CLMPCC_CCR_T0_INIT      0x20        /* Type 0: Initialise Channel */
#define CLMPCC_CCR_T0_RESET_ALL 0x10        /* Type 0: Reset All */
#define CLMPCC_CCR_T0_TX_EN     0x08        /* Type 0: Transmitter Enable */
#define CLMPCC_CCR_T0_TX_DIS    0x04        /* Type 0: Transmitter Disable */
#define CLMPCC_CCR_T0_RX_EN     0x02        /* Type 0: Receiver Enable */
#define CLMPCC_CCR_T0_RX_DIS    0x01        /* Type 0: Receiver Disable */
#define CLMPCC_CCR_T1_CLR_TMR1  0xc0        /* Type 1: Clear Timer 1 */
#define CLMPCC_CCR_T1_CLR_TMR2  0xa0        /* Type 1: Clear Timer 5 */
#define CLMPCC_CCR_T1_CLR_RECV  0x90        /* Type 1: Clear Receiver */

/* Channel Status Register (Async Mode) */
#define CLMPCC_CSR_RX_ENABLED   (1 << 7)    /* Receiver Enabled */
#define CLMPCC_CSR_RX_FLOW_OFF  (1 << 6)    /* Receive Flow Off */
#define CLMPCC_CSR_RX_FLOW_ON   (1 << 5)    /* Receive Flow On */
#define CLMPCC_CSR_TX_ENABLED   (1 << 3)    /* Transmitter Enabled */
#define CLMPCC_CSR_TX_FLOW_OFF  (1 << 2)    /* Transmit Flow Off */
#define CLMPCC_CSR_TX_FLOW_ON   (1 << 1)    /* Transmit Flow On */

/* Modem Signal Value Register */
#define CLMPCC_MSVR_DSR         (1 << 7)    /* Current State of DSR Input */
#define CLMPCC_MSVR_CD          (1 << 6)    /* Current State of CD Input */
#define CLMPCC_MSVR_CTS         (1 << 5)    /* Current State of CTS Input */
#define CLMPCC_MSVR_DTR_OPT     (1 << 4)    /* DTR Option Select */
#define CLMPCC_MSVR_PORT_ID     (1 << 2)    /* Device Type (2400 / 2401) */
#define CLMPCC_MSVR_DTR         (1 << 1)    /* Current State of DTR Output */
#define CLMPCC_MSVR_RTS         (1 << 0)    /* Current State of RTS Output */

/* Local Interrupt Vector Register */
#define CLMPCC_LIVR_TYPE_MASK	0x03	    /* Type of Interrupt */
#define CLMPCC_LIVR_EXCEPTION	0x0	    /* Exception (DMA Completion) */
#define CLMPCC_LIVR_MODEM	0x1	    /* Modem Signal Change */
#define CLMPCC_LIVR_TX		0x2	    /* Transmit Data Interrupt */
#define CLMPCC_LIVR_RX		0x3	    /* Receive Data Interrupt */

/* Interrupt Enable Register */
#define CLMPCC_IER_MODEM        (1 << 7)    /* Modem Pin Change Detect */
#define CLMPCC_IER_RET          (1 << 5)    /* Receive Exception Timeout */
#define CLMPCC_IER_RX_FIFO      (1 << 3)    /* Rx FIFO Threshold Reached */
#define CLMPCC_IER_TIMER        (1 << 2)    /* General Timer(s) Timeout */
#define CLMPCC_IER_TX_EMPTY     (1 << 1)    /* Tx Empty */
#define CLMPCC_IER_TX_FIFO      (1 << 0)    /* Tx FIFO Threshold Reached */

/* Local Interrupting Channel Register */
#define CLMPCC_LICR_MASK	0x0c	    /* Mask for channel number */
#define CLMPCC_LICR_CHAN(v)	(((v) & CLMPCC_LICR_MASK) >> 2)

/* Receive Interrupt Register */
#define CLMPCC_RIR_REN		(1 << 7)    /* Receive Enable */
#define CLMPCC_RIR_RACT		(1 << 6)    /* Receive Active */
#define CLMPCC_RIR_REOI		(1 << 5)    /* Receive End of Interrupt */
#define CLMPCC_RIR_RCVT_MASK	0x0c
#define CLMPCC_RIR_RCN_MASK	0x03

/* Receive Interrupt Status Register, Low (Async option) */
#define CLMPCC_RISR_TIMEOUT	(1 << 7)    /* Rx FIFO Empty and Timeout */
#define CLMPCC_RISR_OVERRUN	(1 << 3)    /* Rx Overrun Error */
#define CLMPCC_RISR_PARITY	(1 << 2)    /* Rx Parity Error */
#define CLMPCC_RISR_FRAMING	(1 << 1)    /* Rx Framing Error */
#define CLMPCC_RISR_BREAK	(1 << 0)    /* BREAK Detected */

/* Receive FIFO Counter Register */
#define CLMPCC_RFOC_MASK	0x1f	    /* Mask for valid bits */

/* Receive End of Interrupt Register */
#define CLMPCC_REOIR_TERMBUFF	(1 << 7)    /* Terminate Current DMA Buffer */
#define CLMPCC_REOIR_DIS_EX_CHR	(1 << 6)    /* Discard Exception Char (DMA) */
#define CLMPCC_REOIR_TMR2_SYNC	(1 << 5)    /* Set Timer 2 in Sync Mode */
#define CLMPCC_REOIR_TMR1_SYNC	(1 << 4)    /* Set Timer 1 in Sync Mode */
#define CLMPCC_REOIR_NO_TRANS	(1 << 3)    /* No Transfer of Data */

/* Transmit Interrupt Register */
#define CLMPCC_TIR_TEN		(1 << 7)    /* Transmit Enable */
#define CLMPCC_TIR_TACT		(1 << 6)    /* Transmit Active */
#define CLMPCC_TIR_TEOI		(1 << 5)    /* Transmit End of Interrupt */
#define CLMPCC_TIR_TCVT_MASK	0x0c
#define CLMPCC_TIR_TCN_MASK	0x03

/* Transmit Interrupt Status Register (Async option) */
#define CLMPCC_TISR_BERR	(1 << 7)    /* Bus Error (DMA) */
#define CLMPCC_TISR_EOF		(1 << 6)    /* Transmit End of Frame (DMA) */
#define CLMPCC_TISR_EOB		(1 << 5)    /* Transmit End of Buffer (DMA) */
#define CLMPCC_TISR_UNDERRUN	(1 << 4)    /* Transmit Underrun (sync only) */
#define CLMPCC_TISR_BUFF_ID	(1 << 3)    /* Buffer that has exception */
#define CLMPCC_TISR_TX_EMPTY	(1 << 1)    /* Transmitter Empty */
#define CLMPCC_TISR_TX_FIFO	(1 << 0)    /* Transmit FIFO Below Threshold */

/* Transmit FIFO Transfer Count Register */
#define CLMPCC_TFTC_MASK	0x1f	    /* Mask for valid bits */

/* Transmit End of Interrupt Register */
#define CLMPCC_TEOIR_TERMBUFF	(1 << 7)    /* Terminate Current DMA Buffer */
#define CLMPCC_TEOIR_END_OF_FRM	(1 << 6)    /* End of Frame (sync mode) */
#define CLMPCC_TEOIR_TMR2_SYNC	(1 << 5)    /* Set Timer 2 in Sync Mode */
#define CLMPCC_TEOIR_TMR1_SYNC	(1 << 4)    /* Set Timer 1 in Sync Mode */
#define CLMPCC_TEOIR_NO_TRANS	(1 << 3)    /* No Transfer of Data */

/* Modem Interrupt Register */
#define CLMPCC_MIR_MEN		(1 << 7)    /* Modem Enable */
#define CLMPCC_MIR_MACT		(1 << 6)    /* Modem Active */
#define CLMPCC_MIR_MEOI		(1 << 5)    /* Modem End of Interrupt */
#define CLMPCC_MIR_MCVT_MASK	0x0c
#define CLMPCC_MIR_MCN_MASK	0x03

/* Modem/Timer Interrupt Status Register */
#define CLMPCC_MISR_DSR		(1 << 7)    /* DSR Changed State */
#define CLMPCC_MISR_CD		(1 << 6)    /* CD Changed State */
#define CLMPCC_MISR_CTS		(1 << 5)    /* CTS Changed State */
#define CLMPCC_MISR_TMR2	(1 << 1)    /* Timer 2 Timed Out */
#define CLMPCC_MISR_TMR1	(1 << 0)    /* Timer 1 Timed Out */

/* Modem End of Interrupt Register */
#define CLMPCC_MEOIR_TMR2_SYNC	(1 << 5)    /* Set Timer 2 in Sync Mode */
#define CLMPCC_MEOIR_TMR1_SYNC	(1 << 4)    /* Set Timer 1 in Sync Mode */

/* Default value for CLMPCC_REG_RTPRl */
#define CLMPCC_RTPR_DEFAULT	2	    /* 2mS timeout period */

/*
 * Return a value for the Receive Timer Prescaler register
 * for a given clock rate and number of milliseconds.
 * The minimum recommended value for this register is 0x0a.
 */
#define CLMPCC_MSEC_TO_TPR(c,m)	(((((c)/2048)/(1000/(m))) > 0x0a) ?	\
				  (((c)/2048)/(1000/(m))) : 0x0a)

#endif  /* __clmpccreg_h */

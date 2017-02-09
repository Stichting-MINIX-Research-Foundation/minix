/*	$NetBSD: pcctworeg.h,v 1.3 2008/04/28 20:23:54 martin Exp $	*/

/*-
 * Copyright (c) 1999, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Steve C. Woodford
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
 * Register definitions for the PCCchip2 device, and offset to the
 * various subordinate devices which hang off it.
 */
#ifndef	_MVME_PCCTWOREG_H
#define	_MVME_PCCTWOREG_H

/*
 * Offsets to the various devices which hang off the PCCChip2.
 * Note that these are offsets from the base of the PCCChip2's
 * own registers.
 */
#define PCCTWO_REG_OFF	    0x00000	/* Offset of PCCChip2's own registers */
#define PCCTWO_LPT_OFF	    0x00000	/* Offset of parallel port registers */
#define PCCTWO_SCC_OFF      0x03000	/* Offset of CD2401 Serial Comms chip */
#define PCCTWO_IE_OFF	    0x04000	/* Offset of 82596 LAN controller */
#define PCCTWO_NCRSC_OFF    0x05000	/* Offset of NCR53C710 SCSI chip */

/*
 * The two devices on mvme1[67]2's MCchip
 */
#define MCCHIP_ZS0_OFF      0x03000
#define MCCHIP_ZS1_OFF      0x03800

/*
 * This is needed to figure out the boot device.
 * (The physical address of the boot device's registers are passed in
 * from the Boot ROM)
 */
#define PCCTWO_PADDR(off)	((void *)(0xfff42000u + (off)))


/*
 * The layout of the PCCchip2's Registers.
 * Each one is 8-bits wide, unless otherwise indicated.
 */
#define	PCC2REG_CHIP_ID		0x00	/* Chip ID */
#define	PCC2REG_CHIP_REVISION	0x01	/* Chip Revision */
#define	PCC2REG_GENERAL_CONTROL	0x02	/* General Control */
#define	PCC2REG_VECTOR_BASE	0x03	/* Vector Base */
#define	PCC2REG_TIMER1_COMPARE	0x04	/* Tick Timer 1 Compare (32-bit) */
#define	PCC2REG_TIMER1_COUNTER	0x08	/* Tick Timer 1 Counter (32-bit) */
#define	PCC2REG_TIMER2_COMPARE	0x0c	/* Tick Timer 2 Compare (32-bit) */
#define	PCC2REG_TIMER2_COUNTER	0x10	/* Tick Timer 2 Counter (32-bit) */
#define	PCC2REG_PRESCALE_COUNT	0x14	/* Prescaler Count */
#define	PCC2REG_PRESCALE_ADJUST	0x15	/* Prescaler Clock Adjust */
#define	PCC2REG_TIMER2_CONTROL	0x16	/* Tick Timer 2 Control */
#define	PCC2REG_TIMER1_CONTROL	0x17	/* Tick Timer 1 Control */
#define	PCC2REG_GPIO_ICSR	0x18	/* GP Input Interrupt Control */
#define	PCC2REG_GPIO_CONTROL	0x19	/* GP Input/Output Control */
#define	PCC2REG_TIMER2_ICSR	0x1a	/* Tick Timer 2 Interrupt Control */
#define	PCC2REG_TIMER1_ICSR	0x1b	/* Tick Timer 1 Interrupt Control */
#define	PCC2REG_SCC_ERR_STATUS	0x1c	/* SCC Error Status */
#define	PCC2REG_SCC_MODEM_ICSR	0x1d	/* SCC Modem Interrupt Control */
#define	PCC2REG_SCC_TX_ICSR	0x1e	/* SCC Transmit Interrupt Control */
#define	PCC2REG_SCC_RX_ICSR	0x1f	/* SCC Receive Interrupt Control */
#define	PCC2REG_SCC_MODEM_PIACK	0x23	/* SCC Modem PIACK */
#define	PCC2REG_SCC_TX_PIACK	0x25	/* SCC Transmit PIACK */
#define	PCC2REG_SCC_RX_PIACK	0x27	/* SCC Receive PIACK */
#define	PCC2REG_ETH_ERR_STATUS	0x28	/* LANC Error Status */
#define	PCC2REG_ETH_ICSR	0x2a	/* LANC Interrupt Control */
#define	PCC2REG_ETH_BERR_STATUS	0x2b	/* LANC Bus Error Interrupt Ctrl */
#define	PCC2REG_SCSI_ERR_STATUS	0x2c	/* SCSI Error Status */
#define	PCC2REG_SCSI_ICSR	0x2f	/* SCSI Interrupt Control */
#define	PCC2REG_PRT_ACK_ICSR	0x30	/* Printer ACK Interrupt Control */
#define	PCC2REG_PRT_FAULT_ICSR	0x31	/* Printer FAULT Interrupt Ctrl */
#define	PCC2REG_PRT_SEL_ICSR	0x32	/* Printer SEL Interrupt Control */
#define	PCC2REG_PRT_PE_ICSR	0x33	/* Printer PE Interrupt Control */
#define	PCC2REG_PRT_BUSY_ICSR	0x34	/* Printer BUSY Interrupt Control */
#define	PCC2REG_PRT_INPUT_STATUS 0x36	/* Printer Input Status */
#define	PCC2REG_PRT_CONTROL	0x37	/* Printer Port Control */
#define	PCC2REG_CHIP_SPEED	0x38	/* Chip Speed (16-bit) */
#define	PCC2REG_PRT_DATA	0x3a	/* Printer Data (16-bit) */
#define	PCC2REG_IRQ_LEVEL	0x3e	/* Interrupt Priority Level */
#define	PCC2REG_IRQ_MASK	0x3f	/* Interrupt Mask */

/*
 * Additions to the registers for the MCChip. Some of these overlap with
 * the PCCchip2's registers, but only where hardware is not present, eg.
 * the printer registers.
 */
#define	MCCHIPREG_TIMER4_ICSR	0x18	/* Tick timer 4 interrupt control */
#define	MCCHIPREG_TIMER3_ICSR	0x19	/* Tick timer 4 interrupt control */
#define	MCCHIPREG_PARERR_ICSR	0x1c	/* Parity error interrupt control */
#define	MCCHIPREG_SCC_ICSR	0x1d	/* ZS-85230 interrupt control */
#define	MCCHIPREG_TIMER4_CTRL	0x1e	/* Tick timer 4 control */
#define	MCCHIPREG_TIMER3_CTRL	0x1f	/* Tick timer 3 control */
#define	MCCHIPREG_DRAM_BAR	0x20	/* DRAM Base Address (16-bits) */
#define	MCCHIPREG_SRAM_BAR	0x22	/* SRAM Base Address (16-bits) */
#define	MCCHIPREG_DRAM_SIZE	0x24	/* DRAM Size */
#define	MCCHIPREG_RAM_OPTIONS	0x25	/* DRAM/SRAM Options */
#define	MCCHIPREG_SRAM_SIZE	0x26	/* SRAM Size */
#define	MCCHIPREG_GP_INPUTS	0x2d	/* General Purpose Inputs */
#define	MCCHIPREG_162_VERSION	0x2e	/* MVME162-LX Series Version */
#define	MCCHIPREG_TIMER3_COMP	0x30	/* Tick Timer 3 Compare (32-bit) */
#define	MCCHIPREG_TIMER3_CNTR	0x34	/* Tick Timer 3 Counter (32-bit) */
#define	MCCHIPREG_TIMER4_COMP	0x38	/* Tick Timer 4 Compare (32-bit) */
#define	MCCHIPREG_TIMER4_CNTR	0x3c	/* Tick Timer 4 Counter (32-bit) */
#define	MCCHIPREG_BUS_CLOCK	0x40	/* Bus Clock */
#define	MCCHIPREG_EPROM_TIMING	0x41	/* EPROM Access Time Control */
#define	MCCHIPREG_FLASH_TIMING	0x42	/* FLASH Access Time Control */
#define	MCCHIPREG_ABORT_ICSR	0x43	/* ABORT Switch Interrupt Control */
#define	MCCHIPREG_RESET_CONTROL	0x44	/* Reset Switch Control */
#define	MCCHIPREG_WDOG_CONTROL	0x45	/* Watchdog Timer Control */
#define	MCCHIPREG_TIMEBASE_SEL	0x46	/* Access & Watchdog Timebase Select */
#define	MCCHIPREG_DRAM_CONTROL	0x48	/* Parity DRAM Control */
#define	MCCHIPREG_MPU_STATUS	0x4a	/* MPU Status */
#define	MCCHIPREG_PRESCALER	0x4c	/* Prescaler Count Register (32-bits) */

/*
 * PCCchip2's register size is 0x40. MCchip's is 0x50. Plump for the latter.
 */
#define PCC2REG_SIZE		0x50

/*
 * Convenience macroes for accessing the PCCChip2's registers
 * through bus_space.
 */
#define	pcc2_reg_read(sc,r)	\
		bus_space_read_1((sc)->sc_bust, (sc)->sc_bush, (r))
#define	pcc2_reg_read16(sc,r)	\
		bus_space_read_2((sc)->sc_bust, (sc)->sc_bush, (r))
#define	pcc2_reg_read32(sc,r)	\
		bus_space_read_4((sc)->sc_bust, (sc)->sc_bush, (r))
#define	pcc2_reg_write(sc,r,v)	\
		bus_space_write_1((sc)->sc_bust, (sc)->sc_bush, (r), (v))
#define	pcc2_reg_write16(sc,r,v)	\
		bus_space_write_2((sc)->sc_bust, (sc)->sc_bush, (r), (v))
#define	pcc2_reg_write32(sc,r,v)	\
		bus_space_write_4((sc)->sc_bust, (sc)->sc_bush, (r), (v))

/*
 * We use the interrupt vector bases suggested in the Motorola Docs...
 * The first is written to the PCCChip2 for interrupt sources under
 * its control. The second is written to the CD2401's Local Interrupt
 * Vector Register. Thus, we don't use the Auto-Vector facilities
 * for the CD2401, as recommended in the PCCChip2 Programmer's Guide.
 * The third is used as a base for the ZS85230 serial chips on mvme162.
 */
#define PCCTWO_VECBASE		0x50
#define PCCTWO_SCC_VECBASE	0x5c
#define MCCHIP_ZS_VECBASE	0x5c

/*
 * PCCchip2 Vector Encoding (Offsets from PCCTWO_VECBASE)
 * The order 0x0 -> 0xf also indicates priority, with 0x0 lowest.
 */
#define PCCTWOV_PRT_BUSY	0x0	/* Printer Port 'BSY' */
#define PCCTWOV_PRT_PE		0x1	/* Printer Port 'PE' (Paper Empty) */
#define PCCTWOV_PRT_SELECT	0x2	/* Printer Port 'SELECT' */
#define PCCTWOV_PRT_FAULT	0x3	/* Printer Port 'FAULT' */
#define PCCTWOV_PRT_ACK		0x4	/* Printer Port 'ACK' */
#define PCCTWOV_SCSI		0x5	/* SCSI Interrupt */
#define PCCTWOV_LANC_ERR	0x6	/* LAN Controller Error */
#define PCCTWOV_LANC_IRQ	0x7	/* LAN Controller Interrupt */
#define PCCTWOV_TIMER2		0x8	/* Tick Timer 2 Interrupt */
#define PCCTWOV_TIMER1		0x9	/* Tick Timer 1 Interrupt */
#define PCCTWOV_GPIO		0xa	/* General Purpose Input Interrupt */
#define PCCTWOV_SCC_RX_EXCEP	0xc	/* SCC Receive Exception */
#define PCCTWOV_SCC_MODEM	0xd	/* SCC Modem (Non-Auto-vector mode) */
#define PCCTWOV_SCC_TX		0xe	/* SCC Tx (Non-Auto-vector mode) */
#define PCCTWOV_SCC_RX		0xf	/* SCC Rx (Non-Auto-vector mode) */
#define PCCTWOV_MAX		16

/*
 * MCchip-specific Vector Encoding (Offsets from PCCTWO_VECBASE)
 */
#define MCCHIPV_TIMER4		0x3	/* Tick Timer 4 Interrupt */
#define MCCHIPV_TIMER3		0x4	/* Tick Timer 3 Interrupt */
#define MCCHIPV_PARITY_ERR	0xb	/* Parity DRAM Error Exception */
#define MCCHIPV_ZS0		0xc	/* First ZS85230 Interrupt Vector */
#define MCCHIPV_ZS1		0xc	/* Second ZS85230 Interrupt Vector */
#define MCCHIPV_ABORT		0xe	/* Abort Switch */

/*
 * How to identify the PCCchip2 from an MCchip
 */
#define PCCTWO_CHIP_ID_PCC2	0x20
#define PCCTWO_CHIP_ID_MCCHIP	0x84


/*
 * Bit Values for the General Control Register (PCC2REG_GENERAL_CONTROL)
 */
#define	PCCTWO_GEN_CTRL_FAST	(1u<<0)	/* BBRAM Speed Control */
#define	PCCTWO_GEN_CTRL_MIEN	(1u<<1)	/* Master Interrupt Enable */
#define PCCTWO_GEN_CTRL_C040	(1u<<2)	/* Set when CPU is mc68k family */
#define PCCTWO_GEN_CTRL_DR0	(1u<<3)	/* Download ROM at 0 (mvme166 only) */


/*
 * Calculate the Prescaler Adjust value for a given
 * value of BCLK in MHz. (PCC2REG_PRESCALE_ADJUST)
 */
#define PCCTWO_PRES_ADJ(mhz)	(256 - (mhz))


/*
 * Calculate the Tick Timer Compare register value for
 * a given number of micro-seconds. With the PCCChip2,
 * this is simple since the Tick Counters already have
 * a 1uS period. (PCC2REG_TIMER[12]_COMPARE)
 */
#define PCCTWO_TIMERFREQ	1000000
#define PCCTWO_US2LIM(us)	(us)
#define PCCTWO_LIM2US(lim)	(lim)

/*
 * The Tick Timer Control Registers (PCC2REG_TIMER[12]_CONTROL)
 */
#define PCCTWO_TT_CTRL_CEN	(1u<<0)	/* Counter Enable */
#define PCCTWO_TT_CTRL_COC	(1u<<1)	/* Clear On Compare */
#define PCCTWO_TT_CTRL_COVF	(1u<<2)	/* Clear Overflow Counter */
#define PCCTWO_TT_CTRL_OVF(r)	((r)>>4)/* Value of the Overflow Counter */


/*
 * All the Interrupt Control Registers (PCC2REG_*_ICSR) on the PCCChip2
 * mostly share the same basic layout. These are defined as follows:
 */
#define PCCTWO_ICR_LEVEL_MASK	0x7	/* Mask for the interrupt level */
#define PCCTWO_ICR_ICLR		(1u<<3)	/* Clear Int. (edge-sensitive mode) */
#define PCCTWO_ICR_AVEC		(1u<<3)	/* Enable Auto-Vector Mode */
#define PCCTWO_ICR_IEN		(1u<<4)	/* Interrupt Enable */
#define PCCTWO_ICR_INT		(1u<<5)	/* Interrupt Active */
#define PCCTWO_ICR_LEVEL	(0u<<6)	/* Level Triggered */
#define PCCTWO_ICR_EDGE		(1u<<6)	/* Edge Triggered */
#define PCCTWO_ICR_RISE_HIGH	(0u<<7)	/* Polarity: Rising Edge or Hi Level */
#define PCCTWO_ICR_FALL_LOW	(1u<<7)	/* Polarity: Falling Edge or Lo Level */
#define PCCTWO_ICR_SC_RD(r)	((r)>>6)/* Get Snoop Control Bits */
#define PCCTWO_ICR_SC_WR(r)	((r)<<6)/* Write Snoop Control Bits */



/*
 * Most of the Error Status Registers (PCC2REG_*_ERR_STATUS) mostly
 * follow the same layout. These error registers are used when a
 * device (eg. SCC, LANC) is mastering the PCCChip2's local bus (for
 * example, performing a DMA) and some error occurs. The bits are
 * defined as follows:
 */
#define PCCTWO_ERR_SR_SCLR	(1u<<0)	/* Clear Error Status */
#define PCCTWO_ERR_SR_LTO	(1u<<1)	/* Local Bus Timeout */
#define PCCTWO_ERR_SR_EXT	(1u<<2)	/* External (VMEbus) Error */
#define PCCTWO_ERR_SR_PRTY	(1u<<3)	/* DRAM Parity Error */
#define PCCTWO_ERR_SR_RTRY	(1u<<4)	/* Retry Required */
#define PCCTWO_ERR_SR_MASK	0x0Eu


/*
 * General Purpose Input/Output Pin Control Register
 * (PCC2REG_GPIO_CONTROL)
 */
#define PCCTWO_GPIO_CTRL_GPO	(1u<<0)	/* Controls the GP Output Pin */
#define PCCTWO_GPIO_CTRL_GPOE	(1u<<1)	/* General Purpose Output Enable */
#define PCCTWO_GPIO_CTRL_GPI	(1u<<3)	/* The current state of the GP Input */


/*
 * Printer Input Status Register (PCC2REG_PRT_INPUT_STATUS)
 */
#define PCCTWO_PRT_IN_SR_BSY	(1u<<0)	/* State of printer's BSY Input */
#define PCCTWO_PRT_IN_SR_PE	(1u<<1)	/* State of printer's PE Input */
#define PCCTWO_PRT_IN_SR_SEL	(1u<<2)	/* State of printer's SELECT Input */
#define PCCTWO_PRT_IN_SR_FLT	(1u<<3)	/* State of printer's FAULT Input */
#define PCCTWO_PRT_IN_SR_ACK	(1u<<4)	/* State of printer's ACK Input */
#define PCCTWO_PRT_IN_SR_PINT	(1u<<7)	/* Printer Interrupt Status */


/*
 * Printer Port Control Register (PCC2REG_PRT_CONTROL)
 */
#define PCCTWO_PRT_CTRL_MAN	(1u<<0)	/* Manual Strobe Control */
#define PCCTWO_PRT_CTRL_FAST	(1u<<1)	/* Fast Auto Strobe */
#define PCCTWO_PRT_CTRL_STB	(1u<<2)	/* Strobe Pin, in manual control mode */
#define PCCTWO_PRT_CTRL_INP	(1u<<3)	/* Printer Input Prime */
#define	PCCTWO_PRT_CTRL_DOEN	(1u<<4)	/* Printer Data Output Enable */

#endif	/* _MVME_PCCTWOREG_H */

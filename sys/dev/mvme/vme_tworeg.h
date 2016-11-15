/*	$NetBSD: vme_tworeg.h,v 1.2 2008/04/28 20:23:54 martin Exp $	*/

/*-
 * Copyright (c) 1999, 2002 The NetBSD Foundation, Inc.
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

#ifndef _MVME_VME_TWOREG_H
#define _MVME_VME_TWOREG_H

/*
 * Where the VMEchip2's registers live relative to the start
 * of the VMEChip2's register space.
 */
#define	VME2REG_LCSR_OFFSET	0x0000
#define	VME2REG_GCSR_OFFSET	0x0100


/*
 * Register map of the Type 2 VMEchip found on many MVME boards.
 * Note: Only responds to D32 accesses.
 */

	/*
	 * Slave window configuration registers
	 */
#define VME2_SLAVE_WINDOWS		2
#define	VME2LCSR_SLAVE_ADDRESS(x)	(0x00 + ((x) * 4))
#define  VME2_SLAVE_ADDRESS_START_SHIFT	16
#define  VME2_SLAVE_ADDRESS_START_MASK	(0x0000ffffu)
#define  VME2_SLAVE_ADDRESS_END_SHIFT	0
#define  VME2_SLAVE_ADDRESS_END_MASK	(0xffff0000u)

#define	VME2LCSR_SLAVE_TRANS(x)		(0x08 + ((x) * 4))
#define  VME2_SLAVE_TRANS_SELECT_SHIFT	16
#define  VME2_SLAVE_TRANS_SELECT_MASK	(0x0000ffffu)
#define  VME2_SLAVE_TRANS_ADDRESS_SHIFT	0
#define  VME2_SLAVE_TRANS_ADDRESS_MASK	(0xffff0000u)

#define	VME2LCSR_SLAVE_CTRL		0x10
#define	 VME2_SLAVE_AMSEL_DAT(x)	(1u << (0 + ((x) * 16)))
#define	 VME2_SLAVE_AMSEL_PGM(x)	(1u << (1 + ((x) * 16)))
#define	 VME2_SLAVE_AMSEL_BLK(x)	(1u << (2 + ((x) * 16)))
#define	 VME2_SLAVE_AMSEL_BLKD64(x)	(1u << (3 + ((x) * 16)))
#define	 VME2_SLAVE_AMSEL_A24(x)	(1u << (4 + ((x) * 16)))
#define	 VME2_SLAVE_AMSEL_A32(x)	(1u << (5 + ((x) * 16)))
#define	 VME2_SLAVE_AMSEL_USR(x)	(1u << (6 + ((x) * 16)))
#define	 VME2_SLAVE_AMSEL_SUP(x)	(1u << (7 + ((x) * 16)))
#define	 VME2_SLAVE_CTRL_WP(x)		(1u << (8 + ((x) * 16)))
#define	 VME2_SLAVE_CTRL_SNOOP_INHIBIT(x) (0u << (9 + ((x) * 16)))
#define	 VME2_SLAVE_CTRL_SNOOP_WRSINK(x)  (1u << (9 + ((x) * 16)))
#define	 VME2_SLAVE_CTRL_SNOOP_WRINVAL(x) (2u << (9 + ((x) * 16)))
#define	 VME2_SLAVE_CTRL_ADDER(x)	(1u << (11 + ((x) * 16)))

	/*
	 * Master window address control registers
	 */
#define VME2_MASTER_WINDOWS		4
#define	VME2LCSR_MASTER_ADDRESS(x)	(0x14 + ((x) * 4))
#define  VME2_MAST_ADDRESS_START_SHIFT	16
#define  VME2_MAST_ADDRESS_START_MASK	(0x0000ffffu)
#define  VME2_MAST_ADDRESS_END_SHIFT	0
#define  VME2_MAST_ADDRESS_END_MASK	(0xffff0000u)

#define	VME2LCSR_MAST4_TRANS		0x24
#define  VME2_MAST4_TRANS_SELECT_SHIFT	16
#define  VME2_MAST4_TRANS_SELECT_MASK	(0x0000ffffu)
#define  VME2_MAST4_TRANS_ADDRESS_SHIFT	0
#define  VME2_MAST4_TRANS_ADDRESS_MASK	(0xffff0000u)

	/*
	 * VMEbus master attribute control register
	 */
#define	VME2LCSR_MASTER_ATTR		0x28
#define  VME2_MASTER_ATTR_AM_SHIFT(x)	((x) * 8)
#define  VME2_MASTER_ATTR_AM_MASK	(0x0000003fu)
#define  VME2_MASTER_ATTR_WP		(1u << 6)
#define  VME2_MASTER_ATTR_D16		(1u << 7)

	/*
	 * GCSR Group/Board addresses, and
	 * VMEbus Master Enable Control register, and
	 * Local to VMEbus I/O Control register, and
	 * ROM Control register (unused).
	 */
#define	VME2LCSR_GCSR_ADDRESS		0x2c
#define	 VME2_GCSR_ADDRESS_SHIFT	16
#define	 VME2_GCSR_ADDRESS_MASK		(0xfff00000u)

#define VME2LCSR_MASTER_ENABLE		0x2c
#define  VME2_MASTER_ENABLE_MASK	(0x000f0000u)
#define  VME2_MASTER_ENABLE(x)		(1u << ((x) + 16))

#define VME2LCSR_IO_CONTROL		0x2c
#define  VME2_IO_CONTROL_SHIFT		8
#define  VME2_IO_CONTROL_MASK		(0x0000ff00u)
#define  VME2_IO_CONTROL_I1SU		(1u << 8)
#define  VME2_IO_CONTROL_I1WP		(1u << 9)
#define  VME2_IO_CONTROL_I1D16		(1u << 10)
#define  VME2_IO_CONTROL_I1EN		(1u << 11)
#define  VME2_IO_CONTROL_I2PD		(1u << 12)
#define  VME2_IO_CONTROL_I2SU		(1u << 13)
#define  VME2_IO_CONTROL_I2WP		(1u << 14)
#define  VME2_IO_CONTROL_I2EN		(1u << 15)

	/*
	 * VMEChip2 PROM Decoder, SRAM and DMA Control register
	 */
#define VME2LCSR_PROM_SRAM_DMA_CTRL	0x30
#define	 VME2_PSD_SRAMS_MASK		(0x00ff0000u)
#define	 VME2_PSD_SRAMS_CLKS6		(0u << 16)
#define	 VME2_PSD_SRAMS_CLKS5		(1u << 16)
#define	 VME2_PSD_SRAMS_CLKS4		(2u << 16)
#define	 VME2_PSD_SRAMS_CLKS3		(3u << 16)
#define  VME2_PSD_TBLSC_INHIB		(0u << 18)
#define  VME2_PSD_TBLSC_WRSINK		(1u << 18)
#define  VME2_PSD_TBLSC_WRINV		(2u << 18)
#define  VME2_PSD_ROM0			(1u << 20)
#define  VME2_PSD_WAITRMW		(1u << 21)

	/*
	 * VMEbus requester control register
	 */
#define VME2LCSR_VME_REQUESTER_CONTROL	0x30
#define	 VME2_VMEREQ_CTRL_MASK		(0x0000ff00u)
#define	 VME2_VMEREQ_CTRL_LVREQL_MASK	(0x00000300u)
#define	 VME2_VMEREQ_CTRL_LVREQL(x)	((u_int)(x) << 8)
#define  VME2_VMEREQ_CTRL_LVRWD		(1u << 10)
#define  VME2_VMEREQ_CTRL_LVFAIR	(1u << 11)
#define  VME2_VMEREQ_CTRL_DWB		(1u << 13)
#define  VME2_VMEREQ_CTRL_DHB		(1u << 14)
#define  VME2_VMEREQ_CTRL_ROBN		(1u << 15)

	/*
	 * DMAC control register
	 */
#define VME2LCSR_DMAC_CONTROL1		0x30
#define	 VME2_DMAC_CTRL1_MASK		(0x000000ffu)
#define	 VME2_DMAC_CTRL1_DREQL_MASK	(0x00000003u)
#define	 VME2_DMAC_CTRL1_DREQL(x)	((u_int)(x) << 0)
#define	 VME2_DMAC_CTRL1_DRELM_MASK	(0x0000000cu)
#define	 VME2_DMAC_CTRL1_DRELM(x)	((u_int)(x) << 2)
#define  VME2_DMAC_CTRL1_DFAIR		(1u << 4)
#define  VME2_DMAC_CTRL1_DTBL		(1u << 5)
#define  VME2_DMAC_CTRL1_DEN		(1u << 6)
#define  VME2_DMAC_CTRL1_DHALT		(1u << 7)

	/*
	 * DMA Control register #2
	 */
#define VME2LCSR_DMAC_CONTROL2		0x34
#define  VME2_DMAC_CTRL2_MASK		(0x0000ffffu)
#define  VME2_DMAC_CTRL2_SHIFT		0
#define  VME2_DMAC_CTRL2_AM_MASK	(0x0000003fu)
#define  VME2_DMAC_CTRL2_BLK_D32	(1u << 6)
#define  VME2_DMAC_CTRL2_BLK_D64	(3u << 6)
#define	 VME2_DMAC_CTRL2_D16		(1u << 8)
#define	 VME2_DMAC_CTRL2_TVME		(1u << 9)
#define	 VME2_DMAC_CTRL2_LINC		(1u << 10)
#define	 VME2_DMAC_CTRL2_VINC		(1u << 11)
#define	 VME2_DMAC_CTRL2_SNOOP_INHIB	(0u << 13)
#define	 VME2_DMAC_CTRL2_SNOOP_WRSNK	(1u << 13)
#define	 VME2_DMAC_CTRL2_SNOOP_WRINV	(2u << 13)
#define	 VME2_DMAC_CTRL2_INTE		(1u << 15)

	/*
	 * DMA Controller Local Bus and VMEbus Addresses, Byte
	 * Counter and Table Address Counter registers
	 */
#define VME2LCSR_DMAC_LOCAL_ADDRESS	0x38
#define VME2LCSR_DMAC_VME_ADDRESS	0x3c
#define VME2LCSR_DMAC_BYTE_COUNTER	0x40
#define VME2LCSR_DMAC_TABLE_ADDRESS	0x44

	/*
	 * VMEbus Interrupter Control register
	 */
#define VME2LCSR_INTERRUPT_CONTROL	0x48
#define	 VME2_INT_CTRL_MASK		(0xff000000u)
#define	 VME2_INT_CTRL_SHIFT		24
#define	 VME2_INT_CTRL_IRQL_MASK	(0x07000000u)
#define	 VME2_INT_CTRL_IRQS		(1u << 27)
#define	 VME2_INT_CTRL_IRQC		(1u << 28)
#define	 VME2_INT_CTRL_IRQ1S_INT	(0u << 29)
#define	 VME2_INT_CTRL_IRQ1S_TICK1	(1u << 29)
#define	 VME2_INT_CTRL_IRQ1S_TICK2	(3u << 29)

	/*
	 * VMEbus Interrupt Vector register
	 */
#define VME2LCSR_INTERRUPT_VECTOR	0x48
#define  VME2_INTERRUPT_VECTOR_MASK	(0x00ff0000u)
#define  VME2_INTERRUPT_VECTOR_SHIFT	16

	/*
	 * MPU Status register
	 */
#define VME2LCSR_MPU_STATUS		0x48
#define	 VME2_MPU_STATUS_MLOB		(1u << 0)
#define	 VME2_MPU_STATUS_MLPE		(1u << 1)
#define	 VME2_MPU_STATUS_MLBE		(1u << 2)
#define	 VME2_MPU_STATUS_MCLR		(1u << 3)

	/*
	 * DMA Interrupt Count register
	 */
#define VME2LCSR_DMAC_INTERRUPT_CONTROL	0x48
#define	 VME2_DMAC_INT_COUNT_MASK	(0x0000f000u)
#define	 VME2_DMAC_INT_COUNT_SHIFT	12

	/*
	 * DMA Controller Status register
	 */
#define VME2LCSR_DMAC_STATUS		0x48
#define  VME2_DMAC_STATUS_DONE		(1u << 0)
#define  VME2_DMAC_STATUS_VME		(1u << 1)
#define  VME2_DMAC_STATUS_TBL		(1u << 2)
#define  VME2_DMAC_STATUS_DLTO		(1u << 3)
#define  VME2_DMAC_STATUS_DLOB		(1u << 4)
#define  VME2_DMAC_STATUS_DLPE		(1u << 5)
#define  VME2_DMAC_STATUS_DLBE		(1u << 6)
#define  VME2_DMAC_STATUS_MLTO		(1u << 7)


	/*
	 * VMEbus Arbiter Time-out register
	 */
#define VME2LCSR_VME_ARB_TIMEOUT	0x4c
#define	 VME2_VME_ARB_TIMEOUT_ENAB	(1u << 24)

	/*
	 * DMA Controller Timers and VMEbus Global Time-out Control registers
	 */
#define VME2LCSR_DMAC_TIME_ONOFF	0x4c
#define  VME2_DMAC_TIME_ON_MASK		(0x001c0000u)
#define  VME2_DMAC_TIME_ON_16US		(0u << 18)
#define  VME2_DMAC_TIME_ON_32US		(1u << 18)
#define  VME2_DMAC_TIME_ON_64US		(2u << 18)
#define  VME2_DMAC_TIME_ON_128US	(3u << 18)
#define  VME2_DMAC_TIME_ON_256US	(4u << 18)
#define  VME2_DMAC_TIME_ON_512US	(5u << 18)
#define  VME2_DMAC_TIME_ON_1024US	(6u << 18)
#define  VME2_DMAC_TIME_ON_DONE		(7u << 18)
#define  VME2_DMAC_TIME_OFF_MASK	(0x00e00000u)
#define  VME2_DMAC_TIME_OFF_0US		(0u << 21)
#define  VME2_DMAC_TIME_OFF_16US	(1u << 21)
#define  VME2_DMAC_TIME_OFF_32US	(2u << 21)
#define  VME2_DMAC_TIME_OFF_64US	(3u << 21)
#define  VME2_DMAC_TIME_OFF_128US	(4u << 21)
#define  VME2_DMAC_TIME_OFF_256US	(5u << 21)
#define  VME2_DMAC_TIME_OFF_512US	(6u << 21)
#define  VME2_DMAC_TIME_OFF_1024US	(7u << 21)
#define  VME2_VME_GLOBAL_TO_MASK	(0x00030000u)
#define	 VME2_VME_GLOBAL_TO_8US		(0u << 16)
#define	 VME2_VME_GLOBAL_TO_16US	(1u << 16)
#define	 VME2_VME_GLOBAL_TO_256US	(2u << 16)
#define	 VME2_VME_GLOBAL_TO_DISABLE	(3u << 16)

	/*
	 * VME Access, Local Bus and Watchdog Time-out Control register
	 */
#define VME2LCSR_VME_ACCESS_TIMEOUT	0x4c
#define  VME2_VME_ACCESS_TIMEOUT_MASK	(0x0000c000u)
#define  VME2_VME_ACCESS_TIMEOUT_64US	(0u << 14)
#define  VME2_VME_ACCESS_TIMEOUT_1MS	(1u << 14)
#define  VME2_VME_ACCESS_TIMEOUT_32MS	(2u << 14)
#define  VME2_VME_ACCESS_TIMEOUT_DISABLE (3u << 14)

#define VME2LCSR_LOCAL_BUS_TIMEOUT	0x4c
#define  VME2_LOCAL_BUS_TIMEOUT_MASK	(0x00003000u)
#define  VME2_LOCAL_BUS_TIMEOUT_64US	(0u << 12)
#define  VME2_LOCAL_BUS_TIMEOUT_1MS	(1u << 12)
#define  VME2_LOCAL_BUS_TIMEOUT_32MS	(2u << 12)
#define  VME2_LOCAL_BUS_TIMEOUT_DISABLE	(3u << 12)

#define VME2LCSR_WATCHDOG_TIMEOUT	0x4c
#define  VME2_WATCHDOG_TIMEOUT_MASK	(0x00000f00u)
#define  VME2_WATCHDOG_TIMEOUT_512US	(0u << 8)
#define  VME2_WATCHDOG_TIMEOUT_1MS	(1u << 8)
#define  VME2_WATCHDOG_TIMEOUT_2MS	(2u << 8)
#define  VME2_WATCHDOG_TIMEOUT_4MS	(3u << 8)
#define  VME2_WATCHDOG_TIMEOUT_8MS	(4u << 8)
#define  VME2_WATCHDOG_TIMEOUT_16MS	(5u << 8)
#define  VME2_WATCHDOG_TIMEOUT_32MS	(6u << 8)
#define  VME2_WATCHDOG_TIMEOUT_64MS	(7u << 8)
#define  VME2_WATCHDOG_TIMEOUT_128MS	(8u << 8)
#define  VME2_WATCHDOG_TIMEOUT_256MS	(9u << 8)
#define  VME2_WATCHDOG_TIMEOUT_512MS	(10u << 8)
#define  VME2_WATCHDOG_TIMEOUT_1S	(11u << 8)
#define  VME2_WATCHDOG_TIMEOUT_4S	(12u << 8)
#define  VME2_WATCHDOG_TIMEOUT_16S	(13u << 8)
#define  VME2_WATCHDOG_TIMEOUT_32S	(14u << 8)
#define  VME2_WATCHDOG_TIMEOUT_64S	(15u << 8)

	/*
	 * Prescaler Control register
	 */
#define VME2LCSR_PRESCALER_CONTROL	0x4c
#define	 VME2_PRESCALER_MASK		(0x000000ffu)
#define	 VME2_PRESCALER_SHIFT		0
#define	 VME2_PRESCALER_CTRL(c)		(256 - (c))

	/*
	 * Tick Timer registers
	 */
#define	VME2LCSR_TIMER_COMPARE(x)	(0x50 + ((x) * 8))
#define	VME2LCSR_TIMER_COUNTER(x)	(0x54 + ((x) * 8))


	/*
	 * Board Control register
	 */
#define VME2LCSR_BOARD_CONTROL		0x60
#define  VME2_BOARD_CONTROL_RSWE	(1u << 24)
#define  VME2_BOARD_CONTROL_BDFLO	(1u << 25)
#define  VME2_BOARD_CONTROL_CPURS	(1u << 26)
#define  VME2_BOARD_CONTROL_PURS	(1u << 27)
#define  VME2_BOARD_CONTROL_BRFLI	(1u << 28)
#define  VME2_BOARD_CONTROL_SFFL	(1u << 29)
#define  VME2_BOARD_CONTROL_SCON	(1u << 30)

	/*
	 * Watchdog Timer Control register
	 */
#define VME2LCSR_WATCHDOG_TIMER_CONTROL	0x60
#define  VME2_WATCHDOG_TCONTROL_WDEN	(1u << 16)
#define  VME2_WATCHDOG_TCONTTRL_WDRSE	(1u << 17)
#define  VME2_WATCHDOG_TCONTTRL_WDSL	(1u << 18)
#define  VME2_WATCHDOG_TCONTTRL_WDBFE	(1u << 19)
#define  VME2_WATCHDOG_TCONTTRL_WDTO	(1u << 20)
#define  VME2_WATCHDOG_TCONTTRL_WDCC	(1u << 21)
#define  VME2_WATCHDOG_TCONTTRL_WDCS	(1u << 22)
#define  VME2_WATCHDOG_TCONTTRL_SRST	(1u << 23)

	/*
	 * Tick Timer Control registers
	 */
#define	VME2LCSR_TIMER_CONTROL		0x60
#define  VME2_TIMER_CONTROL_EN(x)	(1u << (0 + ((x) * 8)))
#define  VME2_TIMER_CONTROL_COC(x)	(1u << (1 + ((x) * 8)))
#define  VME2_TIMER_CONTROL_COF(x)	(1u << (2 + ((x) * 8)))
#define  VME2_TIMER_CONTROL_OVF_SHIFT(x) (4 + ((x) * 8))
#define  VME2_TIMER_CONTROL_OVF_MASK(x)	(0x000000f0u << (4 + ((x) * 8)))

	/*
	 * Prescaler Counter register
	 */
#define VME2LCSR_PRESCALER_COUNTER	0x64

	/*
	 * Local Bus Interrupter Status/Enable/Clear registers
	 */
#define VME2LCSR_LOCAL_INTERRUPT_STATUS	0x68
#define VME2LCSR_LOCAL_INTERRUPT_ENABLE	0x6c
#define VME2LCSR_LOCAL_INTERRUPT_CLEAR	0x74
#define  VME2_LOCAL_INTERRUPT(x)	(1u << (x))
#define  VME2_LOCAL_INTERRUPT_VME(x)	(1u << ((x) - 1))
#define  VME2_LOCAL_INTERRUPT_SWINT(x)	(1u << ((x) + 8))
#define  VME2_LOCAL_INTERRUPT_LM(x)	(1u << ((x) + 16))
#define  VME2_LOCAL_INTERRUPT_SIG(x)	(1u << ((x) + 18))
#define  VME2_LOCAL_INTERRUPT_DMAC	(1u << 22)
#define  VME2_LOCAL_INTERRUPT_VIA	(1u << 23)
#define  VME2_LOCAL_INTERRUPT_TIC(x)	(1u << ((x) + 24))
#define  VME2_LOCAL_INTERRUPT_VI1E	(1u << 26)
#define  VME2_LOCAL_INTERRUPT_PE	(1u << 27)
#define  VME2_LOCAL_INTERRUPT_MWP	(1u << 28)
#define  VME2_LOCAL_INTERRUPT_SYSF	(1u << 29)
#define  VME2_LOCAL_INTERRUPT_ABORT	(1u << 30)
#define  VME2_LOCAL_INTERRUPT_ACFAIL	(1u << 31)
#define  VME2_LOCAL_INTERRUPT_CLEAR_ALL	(0xffffff00u)

	/*
	 * Software Interrupt Set register
	 */
#define VME2LCSR_SOFTINT_SET		0x70
#define  VME2_SOFTINT_SET(x)		(1u << ((x) + 8))

	/*
	 * Interrupt Level registers
	 */
#define VME2LCSR_INTERRUPT_LEVEL_BASE	0x78
#define  VME2_NUM_IL_REGS		4
#define	 VME2_ILOFFSET_FROM_VECTOR(v)	(((((VME2_NUM_IL_REGS*8)-1)-(v))/8)<<2)
#define	 VME2_ILSHIFT_FROM_VECTOR(v)	(((v) & 7) * 4)
#define	 VME2_INTERRUPT_LEVEL_MASK	(0x0fu)

	/*
	 * Vector Base register
	 */
#define VME2LCSR_VECTOR_BASE		0x88
#define  VME2_VECTOR_BASE_MASK		(0xff000000u)
#define	 VME2_VECTOR_BASE_REG_VALUE	(0x76000000u)
#define	 VME2_VECTOR_BASE		(0x60u)
#define	 VME2_VECTOR_LOCAL_OFFSET	(0x08u)
#define	 VME2_VECTOR_LOCAL_MIN		(VME2_VECTOR_BASE + 0x08u)
#define  VME2_VECTOR_LOCAL_MAX		(VME2_VECTOR_BASE + 0x1fu)
#define  VME2_VEC_SOFT0			(VME2_VECTOR_BASE + 0x08u)
#define  VME2_VEC_SOFT1			(VME2_VECTOR_BASE + 0x09u)
#define  VME2_VEC_SOFT2			(VME2_VECTOR_BASE + 0x0au)
#define  VME2_VEC_SOFT3			(VME2_VECTOR_BASE + 0x0bu)
#define  VME2_VEC_SOFT4			(VME2_VECTOR_BASE + 0x0cu)
#define  VME2_VEC_SOFT5			(VME2_VECTOR_BASE + 0x0du)
#define  VME2_VEC_SOFT6			(VME2_VECTOR_BASE + 0x0eu)
#define  VME2_VEC_SOFT7			(VME2_VECTOR_BASE + 0x0fu)
#define  VME2_VEC_GCSRLM0		(VME2_VECTOR_BASE + 0x10u)
#define  VME2_VEC_GCSRLM1		(VME2_VECTOR_BASE + 0x11u)
#define  VME2_VEC_GCSRSIG0		(VME2_VECTOR_BASE + 0x12u)
#define  VME2_VEC_GCSRSIG1		(VME2_VECTOR_BASE + 0x13u)
#define  VME2_VEC_GCSRSIG2		(VME2_VECTOR_BASE + 0x14u)
#define  VME2_VEC_GCSRSIG3		(VME2_VECTOR_BASE + 0x15u)
#define  VME2_VEC_DMAC			(VME2_VECTOR_BASE + 0x16u)
#define  VME2_VEC_VIA			(VME2_VECTOR_BASE + 0x17u)
#define  VME2_VEC_TT1			(VME2_VECTOR_BASE + 0x18u)
#define  VME2_VEC_TT2			(VME2_VECTOR_BASE + 0x19u)
#define  VME2_VEC_IRQ1			(VME2_VECTOR_BASE + 0x1au)
#define  VME2_VEC_PARITY_ERROR		(VME2_VECTOR_BASE + 0x1bu)
#define  VME2_VEC_MWP_ERROR		(VME2_VECTOR_BASE + 0x1cu)
#define  VME2_VEC_SYSFAIL		(VME2_VECTOR_BASE + 0x1du)
#define  VME2_VEC_ABORT			(VME2_VECTOR_BASE + 0x1eu)
#define  VME2_VEC_ACFAIL		(VME2_VECTOR_BASE + 0x1fu)

	/*
	 * I/O Control register #1
	 */
#define VME2LCSR_GPIO_DIRECTION		0x88
#define  VME2_GPIO_DIRECTION_OUT(x)	(1u << ((x) + 16))

	/*
	 * Misc. Status register
	 */
#define VME2LCSR_MISC_STATUS		0x88
#define  VME2_MISC_STATUS_ABRTL		(1u << 20)
#define  VME2_MISC_STATUS_ACFL		(1u << 21)
#define  VME2_MISC_STATUS_SYSFL		(1u << 22)
#define  VME2_MISC_STATUS_MIEN		(1u << 23)

	/*
	 * GPIO Status register
	 */
#define VME2LCSR_GPIO_STATUS		0x88
#define  VME2_GPIO_STATUS(x)		(1u << ((x) + 8))

	/*
	 * GPIO Control register #2
	 */
#define VME2LCSR_GPIO_CONTROL		0x88
#define  VME2_GPIO_CONTROL_SET(x)	(1u << ((x) + 12))

	/*
	 * General purpose input registers
	 */
#define VME2LCSR_GP_INPUTS		0x88
#define  VME2_GP_INPUT(x)		(1u << (x))

	/*
	 * Miscellaneous Control register
	 */
#define VME2LCSR_MISC_CONTROL		0x8c
#define  VME2_MISC_CONTROL_DISBGN	(1u << 0)
#define  VME2_MISC_CONTROL_ENINT	(1u << 1)
#define  VME2_MISC_CONTROL_DISBSYT	(1u << 2)
#define  VME2_MISC_CONTROL_NOELBBSY	(1u << 3)
#define  VME2_MISC_CONTROL_DISMST	(1u << 4)
#define  VME2_MISC_CONTROL_DISSRAM	(1u << 5)
#define  VME2_MISC_CONTROL_REVEROM	(1u << 6)
#define  VME2_MISC_CONTROL_MPIRQEN	(1u << 7)

#define VME2LCSR_SIZE		0x90


#define	vme2_lcsr_read(s,r) \
	bus_space_read_4((s)->sc_mvmebus.sc_bust, (s)->sc_lcrh, (r))
#define	vme2_lcsr_write(s,r,v) \
	bus_space_write_4((s)->sc_mvmebus.sc_bust, (s)->sc_lcrh, (r), (v))


/*
 * Locations of the three fixed VMEbus I/O ranges
 */
#define	VME2_IO0_LOCAL_START		(0xffff0000u)
#define	VME2_IO0_MASK			(0x0000ffffu)
#define	VME2_IO0_VME_START		(0x00000000u)
#define	VME2_IO0_VME_END		(0x0000ffffu)

#define	VME2_IO1_LOCAL_START		(0xf0000000u)
#define	VME2_IO1_MASK			(0x00ffffffu)
#define	VME2_IO1_VME_START		(0x00000000u)
#define	VME2_IO1_VME_END		(0x00ffffffu)

#define	VME2_IO2_LOCAL_START		(0x00000000u)
#define	VME2_IO2_MASK			(0xffffffffu)
#define	VME2_IO2_VME_START		(0xf1000000u)	/* Maybe starts@ 0x0? */
#define	VME2_IO2_VME_END		(0xff7fffffu)

#endif /* _MVME_VME_TWOREG_H */

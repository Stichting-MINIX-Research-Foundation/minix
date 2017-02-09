/*	$NetBSD: gtreg.h,v 1.5 2010/06/09 02:19:51 kiyohara Exp $	*/

/*
 * Copyright (c) 2002 Allegro Networks, Inc., Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Allegro Networks, Inc., and Wasabi Systems, Inc.
 * 4. The name of Allegro Networks, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 * 5. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ALLEGRO NETWORKS, INC. AND
 * WASABI SYSTEMS, INC. ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL EITHER ALLEGRO NETWORKS, INC. OR WASABI SYSTEMS, INC.
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DISCOVERY_DEV_GTREG_H_
#define _DISCOVERY_DEV_GTREG_H_

#define GT__BIT(bit)			(1U << (bit))
#define GT__MASK(bit)			(GT__BIT(bit) - 1)
#define	GT__EXT(data, bit, len)		(((data) >> (bit)) & GT__MASK(len))
#define	GT__CLR(data, bit, len)		((data) &= ~(GT__MASK(len) << (bit)))
#define	GT__INS(new, bit)		((new) << (bit))

#define GT_SIZE			0x10000

/*
 * Table 30: CPU Address Decode Register Map
 */
#define GT_SCS0_Low_Decode		0x0008
#define GT_SCS0_High_Decode		0x0010
#define GT_SCS1_Low_Decode		0x0208
#define GT_SCS1_High_Decode		0x0210
#define GT_SCS2_Low_Decode		0x0018
#define GT_SCS2_High_Decode		0x0020
#define GT_SCS3_Low_Decode		0x0218
#define GT_SCS3_High_Decode		0x0220
#define GT_CS0_Low_Decode		0x0028
#define GT_CS0_High_Decode		0x0030
#define GT_CS1_Low_Decode		0x0228
#define GT_CS1_High_Decode		0x0230
#define GT_CS2_Low_Decode		0x0248
#define GT_CS2_High_Decode		0x0250
#define GT_CS3_Low_Decode		0x0038
#define GT_CS3_High_Decode		0x0040
#define GT_BootCS_Low_Decode		0x0238
#define GT_BootCS_High_Decode		0x0240
#define GT_PCI0_IO_Low_Decode		0x0048
#define GT_PCI0_IO_High_Decode		0x0050
#define GT_PCI0_Mem0_Low_Decode		0x0058
#define GT_PCI0_Mem0_High_Decode	0x0060
#define GT_PCI0_Mem1_Low_Decode		0x0080
#define GT_PCI0_Mem1_High_Decode	0x0088
#define GT_PCI0_Mem2_Low_Decode		0x0258
#define GT_PCI0_Mem2_High_Decode	0x0260
#define GT_PCI0_Mem3_Low_Decode		0x0280
#define GT_PCI0_Mem3_High_Decode	0x0288
#define GT_PCI1_IO_Low_Decode		0x0090
#define GT_PCI1_IO_High_Decode		0x0098
#define GT_PCI1_Mem0_Low_Decode		0x00a0
#define GT_PCI1_Mem0_High_Decode	0x00a8
#define GT_PCI1_Mem1_Low_Decode		0x00b0
#define GT_PCI1_Mem1_High_Decode	0x00b8
#define GT_PCI1_Mem2_Low_Decode		0x02a0
#define GT_PCI1_Mem2_High_Decode	0x02a8
#define GT_PCI1_Mem3_Low_Decode		0x02b0
#define GT_PCI1_Mem3_High_Decode	0x02b8
#define GT_Internal_Decode		0x0068
#define GT_CPU0_Low_Decode		0x0290
#define GT_CPU0_High_Decode		0x0298
#define GT_CPU1_Low_Decode		0x02c0
#define GT_CPU1_High_Decode		0x02c8
#define GT_PCI0_IO_Remap		0x00f0
#define GT_PCI0_Mem0_Remap_Low		0x00f8
#define GT_PCI0_Mem0_Remap_High		0x0320
#define GT_PCI0_Mem1_Remap_Low		0x0100
#define GT_PCI0_Mem1_Remap_High		0x0328
#define GT_PCI0_Mem2_Remap_Low		0x02f8
#define GT_PCI0_Mem2_Remap_High		0x0330
#define GT_PCI0_Mem3_Remap_Low		0x0300
#define GT_PCI0_Mem3_Remap_High		0x0338
#define GT_PCI1_IO_Remap		0x0108
#define GT_PCI1_Mem0_Remap_Low		0x0110
#define GT_PCI1_Mem0_Remap_High		0x0340
#define GT_PCI1_Mem1_Remap_Low		0x0118
#define GT_PCI1_Mem1_Remap_High		0x0348
#define GT_PCI1_Mem2_Remap_Low		0x0310
#define GT_PCI1_Mem2_Remap_High		0x0350
#define GT_PCI1_Mem3_Remap_Low		0x0318
#define GT_PCI1_Mem3_Remap_High		0x0358


/*
 * Table 31: CPU Control Register Map
 */
#define GT_CPU_Cfg			0x0000
#define GT_CPU_Mode			0x0120
#define GT_CPU_Master_Ctl		0x0160
#define GT_CPU_If_Xbar_Ctl_Low		0x0150
#define GT_CPU_If_Xbar_Ctl_High		0x0158
#define GT_CPU_If_Xbar_Timeout		0x0168
#define GT_CPU_Rd_Rsp_Xbar_Ctl_Low	0x0170
#define GT_CPU_Rd_Rsp_Xbar_Ctl_High	0x0178

/*
 * Table 32: CPU Sync Barrier Register Map
 */
#define	GT_PCI_Sync_Barrier(bus)	(0x00c0 | ((bus) << 3))
#define GT_PCI0_Sync_Barrier		0x00c0
#define GT_PCI1_Sync_Barrier		0x00c8

/*
 * Table 33: CPU Access Protection Register Map
 */
#define GT_Protect_Low_0		0x0180
#define GT_Protect_High_0		0x0188
#define GT_Protect_Low_1		0x0190
#define GT_Protect_High_1		0x0198
#define GT_Protect_Low_2		0x01a0
#define GT_Protect_High_2		0x01a8
#define GT_Protect_Low_3		0x01b0
#define GT_Protect_High_3		0x01b8
#define GT_Protect_Low_4		0x01c0
#define GT_Protect_High_4		0x01c8
#define GT_Protect_Low_5		0x01d0
#define GT_Protect_High_5		0x01d8
#define GT_Protect_Low_6		0x01e0
#define GT_Protect_High_6		0x01e8
#define GT_Protect_Low_7		0x01f0
#define GT_Protect_High_7		0x01f8

/*
 * Table 34: Snoop Control Register Map
 */
#define GT_Snoop_Base_0			0x0380
#define GT_Snoop_Top_0			0x0388
#define GT_Snoop_Base_1			0x0390
#define GT_Snoop_Top_1			0x0398
#define GT_Snoop_Base_2			0x03a0
#define GT_Snoop_Top_2			0x03a8
#define GT_Snoop_Base_3			0x03b0
#define GT_Snoop_Top_3			0x03b8

/*
 * Table 35: CPU Error Report Register Map
 */
#define GT_CPU_Error_Address_Low	0x0070
#define GT_CPU_Error_Address_High	0x0078
#define GT_CPU_Error_Data_Low		0x0128
#define GT_CPU_Error_Data_High		0x0130
#define GT_CPU_Error_Parity		0x0138
#define GT_CPU_Error_Cause		0x0140
#define GT_CPU_Error_Mask		0x0148

#define	GT_LowAddr_GET(v)		(GT__EXT((v), 0, 12) << 20)
#define	GT_HighAddr_GET(v)	\
    ((v) != 0 ? ((GT__EXT((v), 0, 12) << 20) | 0xfffff) : 0)
#define	GT_LowAddr2_GET(v)		(GT__EXT((v), 0, 16) << 16)
#define	GT_HighAddr2_GET(v)	\
    ((v) != 0 ? ((GT__EXT((v), 0, 16) << 16) | 0xffff) : 0)
#define	GT_LADDR_GET(v, mdl)	\
    (((mdl) == MARVELL_DISCOVERY) ? GT_LowAddr_GET(v) : GT_LowAddr2_GET(v))
#define	GT_HADDR_GET(v, mdl)	\
    (((mdl) == MARVELL_DISCOVERY) ? GT_HighAddr_GET(v) : GT_HighAddr2_GET(v))

#define GT_MPP_Control0			0xf000
#define GT_MPP_Control1			0xf004
#define GT_MPP_Control2			0xf008
#define GT_MPP_Control3			0xf00c

#define	GT_GPP_IO_Control		0xf100
#define GT_GPP_Value			0xf104
#define	GT_GPP_Interrupt_Cause		0xf108
#define GT_GPP_Interrupt_Mask		0xf10c
#define	GT_GPP_Level_Control		0xf110
#define	GT_GPP_Interrupt_Mask1		0xf114
#define	GT_GPP_Value_Set		0xf118
#define	GT_GPP_Value_Clear		0xf11c
/*
 * Table 36: SCS[0]* Low Decode Address, Offset: 0x008
 * Table 38: SCS[1]* Low Decode Address, Offset: 0x208
 * Table 40: SCS[2]* Low Decode Address, Offset: 0x018
 * Table 42: SCS[3]* Low Decode Address, Offset: 0x218
 * Table 44: CS[0]*  Low Decode Address, Offset: 0x028
 * Table 46: CS[1]*  Low Decode Address, Offset: 0x228
 * Table 48: CS[2]*  Low Decode Address, Offset: 0x248
 * Table 50: CS[3]*  Low Decode Address, Offset: 0x038
 * Table 52: BootCS* Low Decode Address, Offset: 0x238
 * Table 75: CPU 0   Low Decode Address, Offset: 0x290
 * Table 77: CPU 1   Low Decode Address, Offset: 0x2c0
 *
 * 11:00 LowAddr		SCS[0] Base Address
 * 31:12 Reserved		Must be 0.
 */

/*
 * Table 37: SCS[0]* High Decode Address, Offset: 0x010
 * Table 39: SCS[1]* High Decode Address, Offset: 0x210
 * Table 41: SCS[2]* High Decode Address, Offset: 0x020
 * Table 43: SCS[3]* High Decode Address, Offset: 0x220
 * Table 45: CS[0]*  High Decode Address, Offset: 0x030
 * Table 47: CS[1]*  High Decode Address, Offset: 0x230
 * Table 49: CS[2]*  High Decode Address, Offset: 0x250
 * Table 51: CS[3]*  High Decode Address, Offset: 0x040
 * Table 53: BootCS* High Decode Address, Offset: 0x240
 * Table 76: CPU 0   High Decode Address, Offset: 0x298
 * Table 78: CPU 1   High Decode Address, Offset: 0x2c8
 *
 * 11:00 HighAddr		SCS[0] Top Address
 * 31:12 Reserved
 */

/*
 * Table 54: PCI_0 I/O Low Decode Address,      Offset: 0x048
 * Table 56: PCI_0 Memory 0 Low Decode Address, Offset: 0x058
 * Table 58: PCI_0 Memory 1 Low Decode Address, Offset: 0x080
 * Table 60: PCI_0 Memory 2 Low Decode Address, Offset: 0x258
 * Table 62: PCI_0 Memory 3 Low Decode Address, Offset: 0x280
 * Table 64: PCI_1 I/O Low Decode Address,      Offset: 0x090
 * Table 66: PCI_1 Memory 0 Low Decode Address, Offset: 0x0a0
 * Table 68: PCI_1 Memory 1 Low Decode Address, Offset: 0x0b0
 * Table 70: PCI_1 Memory 2 Low Decode Address, Offset: 0x2a0
 * Table 72: PCI_1 Memory 3 Low Decode Address, Offset: 0x2b0
 *
 * 11:00 LowAddr		PCI IO/Memory Space Base Address
 * 23:12 Reserved
 * 26:24 PCISwap		PCI Master Data Swap Control (0: Byte Swap;
 *				1: No swapping; 2: Both byte and word swap;
 *				3: Word swap; 4..7: Reserved)
 * 27:27 PCIReq64		PCI master REQ64* policy (Relevant only when
 *				configured to 64-bit PCI bus and not I/O)
 *				0: Assert s REQ64* only when transaction
 *				   is longer than 64-bits.
 *				1: Always assert REQ64*.
 * 31:28 Reserved
 */
#define	GT_PCISwap_GET(v)		GT__EXT((v), 24, 3)
#define	GT_PCISwap_ByteSwap		0
#define	GT_PCISwap_NoSwap		1
#define	GT_PCISwap_ByteWordSwap		2
#define	GT_PCISwap_WordSwap		3
#define	GT_PCI_LowDecode_PCIReq64	GT__BIT(27)

/*
 * Table 55: PCI_0 I/O High Decode Address,      Offset: 0x050
 * Table 57: PCI_0 Memory 0 High Decode Address, Offset: 0x060
 * Table 59: PCI_0 Memory 1 High Decode Address, Offset: 0x088
 * Table 61: PCI_0 Memory 2 High Decode Address, Offset: 0x260
 * Table 63: PCI_0 Memory 3 High Decode Address, Offset: 0x288
 * Table 65: PCI_1 I/O High Decode Address,      Offset: 0x098
 * Table 67: PCI_1 Memory 0 High Decode Address, Offset: 0x0a8
 * Table 69: PCI_1 Memory 1 High Decode Address, Offset: 0x0b8
 * Table 71: PCI_1 Memory 2 High Decode Address, Offset: 0x2a8
 * Table 73: PCI_1 Memory 3 High Decode Address, Offset: 0x2b8
 *
 * 11:00 HighAddr		PCI_0 I/O Space Top Address
 * 31:12 Reserved
 */

/*
 * Table 74: Internal Space Decode, Offset: 0x068
 * 15:00 IntDecode		GT64260 Internal Space Base Address
 * 23:16 Reserved
 * 26:24 PCISwap		Same as PCI_0 Memory 0 Low Decode Address.
 *				NOTE: Reserved for Galileo Technology usage.
 *				Relevant only for PCI master configuration
 *				transactions on the PCI bus.
 * 31:27 Reserved
 */

/*
 * Table 79: PCI_0 I/O Address Remap,          Offset: 0x0f0
 * Table 80: PCI_0 Memory 0 Address Remap Low, Offset: 0x0f8
 * Table 82: PCI_0 Memory 1 Address Remap Low, Offset: 0x100
 * Table 84: PCI_0 Memory 2 Address Remap Low, Offset: 0x2f8
 * Table 86: PCI_0 Memory 3 Address Remap Low, Offset: 0x300
 * Table 88: PCI_1 I/O Address Remap,          Offset: 0x108
 * Table 89: PCI_1 Memory 0 Address Remap Low, Offset: 0x110
 * Table 91: PCI_1 Memory 1 Address Remap Low, Offset: 0x118
 * Table 93: PCI_1 Memory 2 Address Remap Low, Offset: 0x310
 * Table 95: PCI_1 Memory 3 Address Remap Low, Offset: 0x318
 *
 * 11:00 Remap			PCI IO/Memory Space Address Remap (31:20)
 * 31:12 Reserved
 */

/*
 * Table 81: PCI_0 Memory 0 Address Remap High, Offset: 0x320
 * Table 83: PCI_0 Memory 1 Address Remap High, Offset: 0x328
 * Table 85: PCI_0 Memory 2 Address Remap High, Offset: 0x330
 * Table 87: PCI_0 Memory 3 Address Remap High, Offset: 0x338
 * Table 90: PCI_1 Memory 0 Address Remap High, Offset: 0x340
 * Table 92: PCI_1 Memory 1 Address Remap High, Offset: 0x348
 * Table 94: PCI_1 Memory 2 Address Remap High, Offset: 0x350
 * Table 96: PCI_1 Memory 3 Address Remap High, Offset: 0x358
 *
 * 31:00 Remap			PCI Memory Address Remap (high 32 bits)
 */

/*
 * Table 97: CPU Configuration, Offset: 0x000
 * 07:00 NoMatchCnt		CPU Address Miss Counter
 * 08:08 NoMatchCntEn		CPU Address Miss Counter Enable
 *				NOTE: Relevant only if multi-GT is enabled.
 *				(0: Disabled; 1: Enabled)
 * 09:09 NoMatchCntExt		CPU address miss counter MSB
 * 10:10 Reserved
 * 11:11 AACKDelay		Address Acknowledge Delay
 *				0: AACK* is asserted one cycle after TS*.
 *				1: AACK* is asserted two cycles after TS*.
 * 12:12 Endianess		Must be 0
 *				NOTE: The GT64260 does not support the PowerPC
 *				      Little Endian convention
 * 13:13 Pipeline		Pipeline Enable
 *				0: Disabled. The GT64260 will not respond with
 *				   AACK* to a new CPU transaction, before the
 *				   previous transaction data phase completes.
 *				1: Enabled.
 * 14:14 Reserved
 * 15:15 TADelay		Transfer Acknowledge Delay
 *				0: TA* is asserted one cycle after AACK*
 *				1: TA* is asserted two cycles after AACK*
 * 16:16 RdOOO			Read Out of Order Completion
 *				0: Not Supported, Data is always returned in
 *				   order (DTI[0-2] is always driven
 *				1: Supported
 * 17:17 StopRetry		Relevant only if PCI Retry is enabled
 *				0: Keep Retry all PCI transactions targeted
 *				   to the GT64260.
 *				1: Stop Retry of PCI transactions.
 * 18:18 MultiGTDec		Multi-GT Address Decode
 *				0: Normal address decoding
 *				1: Multi-GT address decoding
 * 19:19 DPValid		CPU DP[0-7] Connection.  CPU write parity ...
 *				0: is not checked. (Not connected)
 *				1: is checked (Connected)
 * 21:20 Reserved
 * 22:22 PErrProp		Parity Error Propagation
 *				0: GT64260 always drives good parity on
 *				   DP[0-7] during CPU reads.
 *				1: GT64260 drives bad parity on DP[0-7] in case
 *				   the read response from the target interface
 *				   comes with erroneous data indication
 *				   (e.g. ECC error from SDRAM interface).
 * 25:23 Reserved
 * 26:26 APValid		CPU AP[0-3] Connection.  CPU address parity ...
 *				0: is not checked. (Not connected)
 *				1: is checked (Connected)
 * 27:27 RemapWrDis		Address Remap Registers Write Control
 *				0: Write to Low Address decode register.
 *				   Results in writing of the corresponding
 *				   Remap register.
 *				1: Write to Low Address decode register.  No
 *				   affect on the corresponding Remap register.
 * 28:28 ConfSBDis		Configuration Read Sync Barrier Disable
 *				0: enabled; 1: disabled
 * 29:29 IOSBDis		I/O Read Sync Barrier Disable
 *				0: enabled; 1: disabled
 * 30:30 ClkSync		Clocks Synchronization
 *				0: The CPU interface is running with SysClk,
 *				   which is asynchronous to TClk.
 *				1: The CPU interface is running with TClk.
 * 31:31 Reserved
 */
#define	GT_CPUCfg_NoMatchCnt_GET(v)	GT__EXT((v), 0, 8)
#define	GT_CPUCfg_NoMatchCntEn		GT__BIT( 9)
#define	GT_CPUCfg_NoMatchCntExt		GT__BIT(10)
#define	GT_CPUCfg_AACKDelay		GT__BIT(11)
#define	GT_CPUCfg_Endianess		GT__BIT(12)
#define	GT_CPUCfg_Pipeline		GT__BIT(13)
#define	GT_CPUCfg_TADelay		GT__BIT(15)
#define	GT_CPUCfg_RdOOO			GT__BIT(16)
#define	GT_CPUCfg_StopRetry		GT__BIT(17)
#define	GT_CPUCfg_MultiGTDec		GT__BIT(18)
#define	GT_CPUCfg_DPValid		GT__BIT(19)
#define	GT_CPUCfg_PErrProp		GT__BIT(22)
#define	GT_CPUCfg_APValid		GT__BIT(26)
#define	GT_CPUCfg_RemapWrDis		GT__BIT(27)
#define	GT_CPUCfg_ConfSBDis		GT__BIT(28)
#define	GT_CPUCfg_IOSBDis		GT__BIT(29)
#define	GT_CPUCfg_ClkSync		GT__BIT(30)

/*
 * Table 98: CPU Mode, Offset: 0x120, Read only
 * 01:00 MultiGTID		Multi-GT ID
 *				Represents the ID to which the GT64260 responds
 *				to during a multi-GT address decoding period.
 * 02:02 MultiGT		(0: Single; 1: Multiple) GT configuration
 * 03:03 RetryEn		(0: Don't; 1: Do) Retry PCI transactions
 * 07:04 CPUType
 *				0x0-0x3: Reserved
 *				0x4:     64-bit PowerPC CPU, 60x bus
 *				0x5:     64-bit PowerPC CPU, MPX bus
 *				0x6-0xf: Reserved
 * 31:08 Reserved
 */
#define	GT_CPUMode_MultiGTID_GET(v)	GT__EXT(v, 0, 2)
#define GT_CPUMode_MultiGT		GT__BIT(2)
#define GT_CPUMode_RetryEn		GT__BIT(3)
#define	GT_CPUMode_CPUType_GET(v)	GT__EXT(v, 4, 4)

/*
 * Table 99: CPU Master Control, Offset: 0x160
 * 07:00 Reserved
 * 08:08 IntArb			CPU Bus Internal Arbiter Enable
 *				NOTE: Only relevant to 60x bus mode. When
 *				      running MPX bus, the GT64260 internal
 *				      arbiter must be used.
 *				0: Disabled.  External arbiter is required.
 *				1: Enabled.  Use the GT64260 CPU bus arbiter.
 * 09:09 IntBusCtl		CPU Interface Unit Internal Bus Control
 *				NOTE: This bit must be set to 1. It is reserved
 *				      for Galileo Technology usage.
 *				0: Enable internal bus sharing between master
 *				   and slave interfaces.
 *				1: Disable internal bus sharing between master
 *				   and slave interfaces.
 * 10:10 MWrTrig		Master Write Transaction Trigger
 *				0: With first valid write data
 *				1: With last valid write data
 * 11:11 MRdTrig		Master Read Response Trigger
 *				0: With first valid read data
 *				1: With last valid read data
 * 12:12 CleanBlock		Clean Block Snoop Transaction Support
 *				0: CPU does not support clean block (603e,750)
 *				1: CPU supports clean block (604e,G4)
 * 13:13 FlushBlock		Flush Block Snoop Transaction Support
 *				0: CPU does not support flush block (603e,750)
 *				1: CPU supports flush block (604e,G4)
 * 31:14 Reserved
 */
#define GT_CPUMstrCtl_IntArb			GT__BIT(8)
#define GT_CPUMstrCtl_IntBusCtl			GT__BIT(9)
#define GT_CPUMstrCtl_MWrTrig			GT__BIT(10)
#define GT_CPUMstrCtl_MRdTrig			GT__BIT(11)
#define GT_CPUMstrCtl_CleanBlock		GT__BIT(12)
#define GT_CPUMstrCtl_FlushBlock		GT__BIT(13)

#define	GT_ArbSlice_SDRAM	0x0	/* SDRAM interface snoop request */
#define GT_ArbSlice_DEVICE	0x1	/* Device request */
#define GT_ArbSlice_NULL	0x2	/* NULL request */
#define GT_ArbSlice_PCI0	0x3	/* PCI_0 access */
#define GT_ArbSlice_PCI1	0x4	/* PCI_1 access */
#define GT_ArbSlice_COMM	0x5	/* Comm unit access */
#define GT_ArbSlice_IDMA0123	0x6	/* IDMA channels 0/1/2/3 access */
#define GT_ArbSlice_IDMA4567	0x7	/* IDMA channels 4/5/6/7 access */
					/* 0x8-0xf: Reserved */

/* Pass in the slice number (from 0..16) as 'n'
 */
#define	GT_XbarCtl_GET_ArbSlice(v, n)		GT__EXT((v), (((n) & 7)*4, 4)

/*
 * Table 100: CPU Interface Crossbar Control Low, Offset: 0x150
 * 03:00 Arb0			Slice  0 of CPU Master pizza Arbiter
 * 07:04 Arb1			Slice  1 of CPU Master pizza Arbiter
 * 11:08 Arb2			Slice  2 of CPU Master pizza Arbiter
 * 15:12 Arb3			Slice  3 of CPU Master pizza Arbiter
 * 19:16 Arb4			Slice  4 of CPU Master pizza Arbiter
 * 23:20 Arb5			Slice  5 of CPU Master pizza Arbiter
 * 27:24 Arb6			Slice  6 of CPU Master pizza Arbiter
 * 31:28 Arb7			Slice  7 of CPU Master pizza Arbiter
 */

/*
 * Table 101: CPU Interface Crossbar Control High, Offset: 0x158
 * 03:00 Arb8			Slice  8 of CPU Master pizza Arbiter
 * 07:04 Arb9			Slice  9 of CPU Master pizza Arbiter
 * 11:08 Arb10			Slice 10 of CPU Master pizza Arbiter
 * 15:12 Arb11			Slice 11 of CPU Master pizza Arbiter
 * 19:16 Arb12			Slice 12 of CPU Master pizza Arbiter
 * 23:20 Arb13			Slice 13 of CPU Master pizza Arbiter
 * 27:24 Arb14			Slice 14 of CPU Master pizza Arbiter
 * 31:28 Arb15			Slice 15 of CPU Master pizza Arbiter
 */

/*
 * Table 102: CPU Interface Crossbar Timeout, Offset: 0x168
 * NOTE: Reserved for Galileo Technology usage.
 * 07:00 Timeout		Crossbar Arbiter Timeout Preset Value
 * 15:08 Reserved
 * 16:16 TimeoutEn		Crossbar Arbiter Timer Enable
 *				(0: Enable; 1: Disable)
 * 31:17 Reserved
 */

/*
 * Table 103: CPU Read Response Crossbar Control Low, Offset: 0x170
 * 03:00 Arb0			Slice  0 of CPU Slave pizza Arbiter
 * 07:04 Arb1			Slice  1 of CPU Slave pizza Arbiter
 * 11:08 Arb2			Slice  2 of CPU Slave pizza Arbiter
 * 15:12 Arb3			Slice  3 of CPU Slave pizza Arbiter
 * 19:16 Arb4			Slice  4 of CPU Slave pizza Arbiter
 * 23:20 Arb5			Slice  5 of CPU Slave pizza Arbiter
 * 27:24 Arb6			Slice  6 of CPU Slave pizza Arbiter
 * 31:28 Arb7			Slice  7 of CPU Slave pizza Arbiter
 */
/*
 * Table 104: CPU Read Response Crossbar Control High, Offset: 0x178
 * 03:00 Arb8			Slice  8 of CPU Slave pizza Arbiter
 * 07:04 Arb9			Slice  9 of CPU Slave pizza Arbiter
 * 11:08 Arb10			Slice 10 of CPU Slave pizza Arbiter
 * 15:12 Arb11			Slice 11 of CPU Slave pizza Arbiter
 * 19:16 Arb12			Slice 12 of CPU Slave pizza Arbiter
 * 23:20 Arb13			Slice 13 of CPU Slave pizza Arbiter
 * 27:24 Arb14			Slice 14 of CPU Slave pizza Arbiter
 * 31:28 Arb15			Slice 15 of CPU Slave pizza Arbiter
 */

/*
 * Table 105: PCI_0 Sync Barrier Virtual Register, Offset: 0x0c0
 * Table 106: PCI_1 Sync Barrier Virtual Register, Offset: 0x0c8
 *   NOTE: The read data is random and should be ignored.
 * 31:00 SyncBarrier		A CPU read from this register creates a
 *				synchronization barrier cycle.
 */

/*
 * Table 107: CPU Protect Address 0 Low, Offset: 0x180
 * Table 109: CPU Protect Address 1 Low, Offset: 0x190
 * Table 111: CPU Protect Address 2 Low, Offset: 0x1a0
 * Table 113: CPU Protect Address 3 Low, Offset: 0x1b0
 * Table 115: CPU Protect Address 4 Low, Offset: 0x1c0
 * Table 117: CPU Protect Address 5 Low, Offset: 0x1d0
 * Table 119: CPU Protect Address 6 Low, Offset: 0x1e0
 * Table 121: CPU Protect Address 7 Low, Offset: 0x1f0
 *
 * 11:00 LowAddr		CPU Protect Region Base Address
 *				Corresponds to address bits[31:20].
 * 15:12 Reserved.		Must be 0
 * 16:16 AccProtect		CPU Access Protect
 *				Access is (0: allowed; 1: forbidden)
 * 17:17 WrProtect		CPU Write Protect
 *				Writes are (0: allowed; 1: forbidden)
 * 18:18 CacheProtect		CPU caching protect. 	Caching (block read)
 *				is (0: allowed; 1: forbidden)
 * 31:19 Reserved
 */
#define GT_CPU_AccProtect			GT__BIT(16)
#define GT_CPU_WrProtect			GT__BIT(17)
#define GT_CPU_CacheProtect			GT__BIT(18)

/*
 * Table 108: CPU Protect Address 0 High, Offset: 0x188
 * Table 110: CPU Protect Address 1 High, Offset: 0x198
 * Table 112: CPU Protect Address 2 High, Offset: 0x1a8
 * Table 114: CPU Protect Address 3 High, Offset: 0x1b8
 * Table 116: CPU Protect Address 4 High, Offset: 0x1c8
 * Table 118: CPU Protect Address 5 High, Offset: 0x1d8
 * Table 120: CPU Protect Address 6 High, Offset: 0x1e8
 * Table 122: CPU Protect Address 7 High, Offset: 0x1f8
 *
 * 11:00 HighAddr		CPU Protect Region Top Address
 *				Corresponds to address bits[31:20]
 * 31:12 Reserved
 */

/*
 * Table 123: Snoop Base Address 0, Offset: 0x380
 * Table 125: Snoop Base Address 1, Offset: 0x390
 * Table 127: Snoop Base Address 2, Offset: 0x3a0
 * Table 129: Snoop Base Address 3, Offset: 0x3b0
 *
 * 11:00 LowAddr		Snoop Region Base Address [31:20]
 * 15:12 Reserved		Must be 0.
 * 17:16 Snoop			Snoop Type
 *				0x0: No Snoop
 *				0x1: Snoop to WT region
 *				0x2: Snoop to WB region
 *				0x3: Reserved
 * 31:18 Reserved
 */
#define GT_Snoop_GET(v)				GT__EXT((v), 16, 2)
#define GT_Snoop_INS(v)				GT__INS((v), 16)
#define	GT_Snoop_None				0
#define	GT_Snoop_WT				1
#define	GT_Snoop_WB				2


/*
 * Table 124: Snoop Top Address 0, Offset: 0x388
 * Table 126: Snoop Top Address 1, Offset: 0x398
 * Table 128: Snoop Top Address 2, Offset: 0x3a8
 * Table 130: Snoop Top Address 3, Offset: 0x3b8
 * 11:00 HighAddr		Snoop Region Top Address [31:20]
 * 31:12 Reserved
 */


/*
 * Table 131: CPU Error Address Low, Offset: 0x070, Read Only.
 *   In case of multiple errors, only the first one is latched.  New error
 *   report latching is enabled only after the CPU Error Address Low register
 *   is being read.
 * 31:00 ErrAddr		Latched address bits [31:0] of a CPU
 *				transaction in case of:
 *				o illegal address (failed address decoding)
 *				o access protection violation
 *				o bad data parity
 *				o bad address parity
 *				Upon address latch, no new address are
 *				registered (due to additional error condition),
 *				until the register is being read.
 */

/*
 * Table 132: CPU Error Address High, Offset: 0x078, Read Only.
 *   Once data is latched, no new data can be registered (due to additional
 *   error condition), until CPU Error Low Address is being read (which
 *   implies, it should be the last being read by the interrupt handler).
 * 03:00 Reserved
 * 07:04 ErrPar			Latched address parity bits in case
 *				of bad CPU address parity detection.
 * 31:08 Reserved
 */
#define	GT_CPUErrorAddrHigh_ErrPar_GET(v)	GT__EXT((v), 4, 4)

/*
 * Table 133: CPU Error Data Low, Offset: 0x128, Read only.
 * 31:00 PErrData		Latched data bits [31:0] in case of bad data
 *				parity sampled on write transactions or on
 *				master read transactions.
 */

/*
 * Table 134: CPU Error Data High, Offset: 0x130, Read only.
 * 31:00 PErrData		Latched data bits [63:32] in case of bad data
 *				parity sampled on write transactions or on
 *				master read transactions.
 */

/*
 * Table 135: CPU Error Parity, Offset: 0x138, Read only.
 * 07:00 PErrPar		Latched data parity bus in case of bad data
 *				parity sampled on write transactions or on
 *				master read transactions.
 * 31:10 Reserved
 */
#define	GT_CPUErrorParity_PErrPar_GET(v)	GT__EXT((v), 0, 8)

/*
 * Table 136: CPU Error Cause, Offset: 0x140
 *   Bits[7:0] are clear only. A cause bit is set upon an error condition
 *   occurrence. Write a 0 value to clear the bit.  Writing a 1 value has
 *   no affect.
 * 00:00 AddrOut		CPU Address Out of Range
 * 01:01 AddrPErr		Bad Address Parity Detected
 * 02:02 TTErr			Transfer Type Violation.
 *				The CPU attempts to burst (read or write) to an
 *				internal register.
 * 03:03 AccErr			Access to a Protected Region
 * 04:04 WrErr			Write to a Write Protected Region
 * 05:05 CacheErr		Read from a Caching protected region
 * 06:06 WrDataPErr		Bad Write Data Parity Detected
 * 07:07 RdDataPErr		Bad Read Data Parity Detected
 * 26:08 Reserved
 * 31:27 Sel			Specifies the error event currently being
 *				reported in Error Address, Error Data, and
 *				Error Parity registers.
 *				0x0: AddrOut
 *				0x1: AddrPErr
 *				0x2: TTErr
 *				0x3: AccErr
 *				0x4: WrErr
 *				0x5: CacheErr
 *				0x6: WrDataPErr
 *				0x7: RdDataPErr
 *				0x8-0x1f: Reserved
 */
#define GT_CPUError_AddrOut		GT__BIT(GT_CPUError_Sel_AddrOut)
#define GT_CPUError_AddrPErr		GT__BIT(GT_CPUError_Sel_AddrPErr)
#define GT_CPUError_TTErr		GT__BIT(GT_CPUError_Sel_TTErr)
#define GT_CPUError_AccErr		GT__BIT(GT_CPUError_Sel_AccErr)
#define GT_CPUError_WrErr		GT__BIT(GT_CPUError_Sel_WrPErr)
#define GT_CPUError_CacheErr		GT__BIT(GT_CPUError_Sel_CachePErr)
#define GT_CPUError_WrDataPErr		GT__BIT(GT_CPUError_Sel_WrDataPErr)
#define GT_CPUError_RdDataPErr		GT__BIT(GT_CPUError_Sel_RdDataPErr)

#define GT_CPUError_Sel_AddrOut		0
#define GT_CPUError_Sel_AddrPErr	1
#define GT_CPUError_Sel_TTErr		2
#define GT_CPUError_Sel_AccErr		3
#define GT_CPUError_Sel_WrErr		4
#define GT_CPUError_Sel_CacheErr	5
#define GT_CPUError_Sel_WrDataPErr	6
#define GT_CPUError_Sel_RdDataPErr	7

#define	GT_CPUError_Sel_GET(v)		GT__EXT((v), 27, 5)

/*
 * Table 137: CPU Error Mask, Offset: 0x148
 * 00:00 AddrOut		If set to 1, enables AddrOut interrupt.
 * 01:01 AddrPErr		If set to 1, enables AddrPErr interrupt.
 * 02:02 TTErr			If set to 1, enables TTErr interrupt.
 * 03:03 AccErr			If set to 1, enables AccErr interrupt.
 * 04:04 WrErr			If set to 1, enables WrErr interrupt.
 * 05:05 CacheErr		If set to 1, enables CacheErr interrupt.
 * 06:06 WrDataPErr		If set to 1, enables WrDataPErr interrupt.
 * 07:07 RdDataPErr		If set to 1, enables RdDataPErr interrupt.
 * 31:08 Reserved
 */

/*
 * Comm Unit Interrupt registers
 */
#define GT_CommUnitIntr_Cause	0xf310
#define GT_CommUnitIntr_Mask	0xf314
#define GT_CommUnitIntr_ErrAddr	0xf318

#define GT_CommUnitIntr_E0	0x00000007
#define GT_CommUnitIntr_E1	0x00000070
#define GT_CommUnitIntr_E2	0x00000700
#define GT_CommUnitIntr_S0	0x00070000
#define GT_CommUnitIntr_S1	0x00700000
#define GT_CommUnitIntr_Sel	0x70000000

/*
 * SDRAM Error Report (ECC) Registers
 */
#define GT_ECC_Data_Lo		0x484	/* latched Error Data (low) */
#define GT_ECC_Data_Hi		0x480	/* latched Error Data (high) */
#define GT_ECC_Addr		0x490	/* latched Error Address */
#define GT_ECC_Rec		0x488	/* latched ECC code from SDRAM */
#define GT_ECC_Calc		0x48c	/* latched ECC code from SDRAM */
#define GT_ECC_Ctl		0x494	/* ECC Control */
#define GT_ECC_Count		0x498	/* ECC 1-bit error count */

/*
 * Watchdog Registers
 */
#define GT_WDOG_Config		0xb410
#define GT_WDOG_Value		0xb414
#define GT_WDOG_Value_NMI	GT__MASK(24)
#define GT_WDOG_Config_Preset	GT__MASK(24)
#define GT_WDOG_Config_Ctl1a	GT__BIT(24)
#define GT_WDOG_Config_Ctl1b	GT__BIT(25)
#define GT_WDOG_Config_Ctl2a	GT__BIT(26)
#define GT_WDOG_Config_Ctl2b	GT__BIT(27)
#define GT_WDOG_Config_Enb	GT__BIT(31)

#define GT_WDOG_NMI_DFLT	(GT__MASK(24) & GT_WDOG_Value_NMI)
#define GT_WDOG_Preset_DFLT	(GT__MASK(22) & GT_WDOG_Config_Preset)

/*
 * Device Bus Interrupts
 */
#define GT_DEVBUS_ICAUSE	0x4d0	/* Device Interrupt Cause */
#define GT_DEVBUS_IMASK		0x4d4	/* Device Interrupt Mask */
#define GT_DEVBUS_ERR_ADDR	0x4d8	/* Device Error Address */

/*
 * bit defines for GT_DEVBUS_ICAUSE, GT_DEVBUS_IMASK
 */
#define GT_DEVBUS_DBurstErr	GT__BIT(0)
#define GT_DEVBUS_DRdyErr	GT__BIT(1)
#define GT_DEVBUS_Sel		GT__BIT(27)
#define GT_DEVBUS_RES	~(GT_DEVBUS_DBurstErr|GT_DEVBUS_DRdyErr|GT_DEVBUS_Sel)


#define ETH0_BASE		0x2400
#define ETH1_BASE		0x2800
#define ETH2_BASE		0x2c00
#define MPSC0_BASE		0x8000
#define MPSC1_BASE		0x9000

#endif /* !_DISCOVERY_DEV_GTREG_H */

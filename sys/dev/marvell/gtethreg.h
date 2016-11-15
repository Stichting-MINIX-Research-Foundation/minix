/*	$NetBSD: gtethreg.h,v 1.5 2010/04/28 13:51:56 kiyohara Exp $	*/

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

#ifndef _DEV_GTETHREG_H_
#define	_DEV_GTETHREG_H_

#define ETH__BIT(bit)			(1U << (bit))
#define ETH__LLBIT(bit)			(1LLU << (bit))
#define ETH__MASK(bit)			(ETH__BIT(bit) - 1)
#define ETH__LLMASK(bit)		(ETH__LLBIT(bit) - 1)
#define	ETH__EXT(data, bit, len)	(((data) >> (bit)) & ETH__MASK(len))
#define	ETH__LLEXT(data, bit, len)	(((data) >> (bit)) & ETH__LLMASK(len))
#define	ETH__CLR(data, bit, len)	((data) &= ~(ETH__MASK(len) << (bit)))
#define	ETH__INS(new, bit)		((new) << (bit))
#define	ETH__LLINS(new, bit)		((uint64_t)(new) << (bit))

/*
 * Descriptors used for both receive & transmit data.  Note that the descriptor
 * must start on a 4LW boundary.  Since the GT accesses the descriptor as
 * two 64-bit quantities, we must present them 32bit quantities in the right
 * order based on endianess.
 */

struct gt_eth_desc {
#if defined(BYTE_ORDER) && BYTE_ORDER == BIG_ENDIAN
	u_int32_t ed_lencnt;	/* length is hi 16 bits; count (rx) is lo 16 */
	u_int32_t ed_cmdsts;	/* command (hi16)/status (lo16) bits */
	u_int32_t ed_nxtptr;	/* next descriptor (must be 4LW aligned) */
	u_int32_t ed_bufptr;	/* pointer to packet buffer */
#endif
#if defined(BYTE_ORDER) && BYTE_ORDER == LITTLE_ENDIAN
	u_int32_t ed_cmdsts;	/* command (hi16)/status (lo16) bits */
	u_int32_t ed_lencnt;	/* length is hi 16 bits; count (rx) is lo 16 */
	u_int32_t ed_bufptr;	/* pointer to packet buffer */
	u_int32_t ed_nxtptr;	/* next descriptor (must be 4LW aligned) */
#endif
};

/* Table 578: Ethernet TX Descriptor - Command/Status word
 * All bits except F, EI, AM, O are only valid if TX_CMD_L is also set,
 * otherwise should be 0 (tx).
 */
#define	TX_STS_LC	ETH__BIT(5)	/* Late Collision */
#define	TX_STS_UR	ETH__BIT(6)	/* Underrun error */
#define	TX_STS_RL	ETH__BIT(8)	/* Retransmit Limit (excession coll) */
#define	TX_STS_COL	ETH__BIT(9)	/* Collision Occurred */
#define	TX_STS_RC(v)	ETH__GETBITS(v, 10, 4)	/* Retransmit Count */
#define	TX_STS_ES	ETH__BIT(15)	/* Error Summary (LC|UR|RL) */
#define	TX_CMD_L	ETH__BIT(16)	/* Last - End Of Packet */
#define	TX_CMD_F	ETH__BIT(17)	/* First - Start Of Packet */
#define	TX_CMD_P	ETH__BIT(18)	/* Pad Packet */
#define	TX_CMD_GC	ETH__BIT(22)	/* Generate CRC */
#define	TX_CMD_EI	ETH__BIT(23)	/* Enable Interrupt */
#define	TX_CMD_AM	ETH__BIT(30)	/* Auto Mode */
#define	TX_CMD_O	ETH__BIT(31)	/* Ownership (1=GT 0=CPU) */

#define	TX_CMD_FIRST	(TX_CMD_F|TX_CMD_O)
#define	TX_CMD_LAST	(TX_CMD_L|TX_CMD_GC|TX_CMD_P|TX_CMD_O)

/* Table 582: Ethernet RX Descriptor - Command/Status Word
 * All bits except F, EI, AM, O are only valid if RX_CMD_L is also set,
 * otherwise should be ignored (rx).
 */
#define	RX_STS_CE	ETH__BIT(0)	/* CRC Error */
#define	RX_STS_COL	ETH__BIT(1)	/* Collision sensed during reception */
#define	RX_STS_LC	ETH__BIT(5)	/* Late Collision (Reserved) */
#define	RX_STS_OR	ETH__BIT(6)	/* Overrun Error */
#define	RX_STS_MFL	ETH__BIT(7)	/* Max Frame Len Error */
#define	RX_STS_SF	ETH__BIT(8)	/* Short Frame Error (< 64 bytes) */
#define	RX_STS_FT	ETH__BIT(11)	/* Frame Type (1 = 802.3) */
#define	RX_STS_M	ETH__BIT(12)	/* Missed Frame */
#define	RX_STS_HE	ETH__BIT(13)	/* Hash Expired (manual match) */
#define	RX_STS_IGMP	ETH__BIT(14)	/* IGMP Packet */
#define	RX_STS_ES	ETH__BIT(15)	/* Error Summary (CE|COL|LC|OR|MFL|SF) */
#define	RX_CMD_L	ETH__BIT(16)	/* Last - End Of Packet */
#define	RX_CMD_F	ETH__BIT(17)	/* First - Start Of Packet */
#define	RX_CMD_EI	ETH__BIT(23)	/* Enable Interrupt */
#define	RX_CMD_AM	ETH__BIT(30)	/* Auto Mode */
#define	RX_CMD_O	ETH__BIT(31)	/* Ownership (1=GT 0=CPU) */

/* Table 586: Hash Table Entry Fields
 */
#define HSH_V		ETH__LLBIT(0)	/* Entry is valid */
#define HSH_S		ETH__LLBIT(1)	/* Skip this entry */
#define HSH_RD		ETH__LLBIT(2)	/* Receive(1) / Discard (0) */
#define HSH_R		ETH__LLBIT(2)	/* Receive(1) */
#define	HSH_PRIO_GET(v)	ETH__LLEXT(v, 51, 2)
#define	HSH_PRIO_INS(v)	ETH__LLINS(v, 51)
#define	HSH_ADDR_MASK	0x7fffff8LLU
#define	HSH_LIMIT	12


#define	ETHC_SIZE	0x4000		/* Register Space */

#define	ETH_EPAR	0x2000		/* PHY Address Register */
#define	ETH_ESMIR	0x2010		/* SMI Register */

#define	ETH_BASE(u)	(ETH0_BASE + ((u) << 10)) /* Ethernet Register Base */
#define	ETH_NUM		3
#define	ETH_SIZE	0x0400			  /* Register Space */

#define	ETH_EBASE	0x0000		/* Base of Registers */
#define	ETH_EPCR	0x0000		/* Port Config. Register */
#define	ETH_EPCXR	0x0008		/* Port Config. Extend Reg */
#define	ETH_EPCMR	0x0010		/* Port Command Register */
#define	ETH_EPSR	0x0018		/* Port Status Register */
#define	ETH_ESPR	0x0020		/* Port Serial Parameters Reg */
#define	ETH_EHTPR	0x0028		/* Port Hash Table Pointer Reg*/
#define	ETH_EFCSAL	0x0030		/* Flow Control Src Addr Low */
#define	ETH_EFCSAH	0x0038		/* Flow Control Src Addr High */
#define	ETH_ESDCR	0x0040		/* SDMA Configuration Reg */
#define	ETH_ESDCMR	0x0048		/* SDMA Command Register */
#define	ETH_EICR	0x0050		/* Interrupt Cause Register */
#define	ETH_EIMR	0x0058		/* Interrupt Mask Register */
#define	ETH_EFRDP0	0x0080		/* First Rx Desc Pointer 0 */
#define	ETH_EFRDP1	0x0084		/* First Rx Desc Pointer 1 */
#define	ETH_EFRDP2	0x0088		/* First Rx Desc Pointer 2 */
#define	ETH_EFRDP3	0x008c		/* First Rx Desc Pointer 3 */
#define	ETH_ECRDP0	0x00a0		/* Current Rx Desc Pointer 0 */
#define	ETH_ECRDP1	0x00a4		/* Current Rx Desc Pointer 1 */
#define	ETH_ECRDP2	0x00a8		/* Current Rx Desc Pointer 2 */
#define	ETH_ECRDP3	0x00ac		/* Current Rx Desc Pointer 3 */
#define	ETH_ECTDP0	0x00e0		/* Current Tx Desc Pointer 0 */
#define	ETH_ECTDP1	0x00e4		/* Current Tx Desc Pointer 1 */
#define	ETH_EDSCP2P0L	0x0060		/* IP Differentiated Services
					   CodePoint to Priority0 low */
#define	ETH_EDSCP2P0H	0x0064		/* IP Differentiated Services
					   CodePoint to Priority0 high*/
#define	ETH_EDSCP2P1L	0x0068		/* IP Differentiated Services
					   CodePoint to Priority1 low */
#define	ETH_EDSCP2P1H	0x006c		/* IP Differentiated Services
					   CodePoint to Priority1 high*/
#define	ETH_EVPT2P	0x0068		/* VLAN Prio. Tag to Priority */
#define	ETH_EMIBCTRS	0x0100		/* MIB Counters */


#define	ETH_EPAR_PhyAD_GET(v, n)	(((v) >> ((n) * 5)) & 0x1f)

#define ETH_ESMIR_READ(phy, reg)	(ETH__INS(phy, 16)|\
					 ETH__INS(reg, 21)|\
					 ETH_ESMIR_ReadOpcode)
#define ETH_ESMIR_WRITE(phy, reg, val)	(ETH__INS(phy, 16)|\
					 ETH__INS(reg, 21)|\
					 ETH__INS(val,  0)|\
					 ETH_ESMIR_WriteOpcode)
#define ETH_ESMIR_Value_GET(v)		ETH__EXT(v, 0, 16)
#define	ETH_ESMIR_WriteOpcode		0
#define	ETH_ESMIR_ReadOpcode		ETH__BIT(26)
#define	ETH_ESMIR_ReadValid		ETH__BIT(27)
#define	ETH_ESMIR_Busy			ETH__BIT(28)

/*
 * Table 597: Port Configuration Register (PCR)
 * 00:00 PM			Promiscuous mode
 *				0: Normal mode (Frames are only received if the
 *				   destination address is found in the hash
 *				   table)
 *				1: Promiscuous mode (Frames are received
 *				   regardless of their destination address.
 *				   Errored frames are discarded unless the Port
 *				   Configuration register's PBF bit is set)
 * 01:01 RBM			Reject Broadcast Mode
 *				0: Receive broadcast address
 *				1: Reject frames with broadcast address
 *				Overridden by the promiscuous mode.
 * 02:02 PBF			Pass Bad Frames
 *				(0: Normal mode, 1: Pass bad Frames)
 *				The Ethernet receiver passes to the CPU errored
 *				frames (like fragments and collided packets)
 *				that are normally rejected.
 *				NOTE: Frames are only passed if they
 *				      successfully pass address filtering.
 * 06:03 Reserved
 * 07:07 EN			Enable (0: Disabled, 1: Enable)
 *				When enabled, the ethernet port is ready to
 *				transmit/receive.
 * 09:08 LPBK			Loop Back Mode
 *				00: Normal mode
 *				01: Internal loop back mode (TX data is looped
 *				    back to the RX lines. No transition is seen
 *				    on the interface pins)
 *				10: External loop back mode (TX data is looped
 *				    back to the RX lines and also transmitted
 *				    out to the MII interface pins)
 *				11: Reserved
 * 10:10 FC			Force Collision
 *				0: Normal mode.
 *				1: Force Collision on any TX frame.
 *				   For RXM test (in Loopback mode).
 * 11:11 Reserved.
 * 12:12 HS			Hash Size
 *				0: 8K address filtering
 *				   (256KB of memory space required).
 *				1: 512 address filtering
 *				   ( 16KB of memory space required).
 * 13:13 HM			Hash Mode (0: Hash Func. 0; 1: Hash Func. 1)
 * 14:14 HDM			Hash Default Mode
 *				0: Discard addresses not found in address table
 *				1: Pass addresses not found in address table
 * 15:15 HD			Duplex Mode (0: Half Duplex, 1: Full Duplex)
 *				NOTE: Valid only when auto-negotiation for
 *				      duplex mode is disabled.
 * 30:16 Reserved
 * 31:31 ACCS			Accelerate Slot Time
 *				(0: Normal mode, 1: Reserved)
 */
#define	ETH_EPCR_PM		ETH__BIT(0)
#define	ETH_EPCR_RBM		ETH__BIT(1)
#define	ETH_EPCR_PBF		ETH__BIT(2)
#define	ETH_EPCR_EN		ETH__BIT(7)
#define	ETH_EPCR_LPBK_GET(v)	ETH__BIT(v, 8, 2)
#define	ETH_EPCR_LPBK_Normal	0
#define	ETH_EPCR_LPBK_Internal	1
#define	ETH_EPCR_LPBK_External	2
#define	ETH_EPCR_FC		ETH__BIT(10)

#define	ETH_EPCR_HS		ETH__BIT(12)
#define	ETH_EPCR_HS_8K		0
#define	ETH_EPCR_HS_512		ETH_EPCR_HS

#define	ETH_EPCR_HM		ETH__BIT(13)
#define	ETH_EPCR_HM_0		0
#define	ETH_EPCR_HM_1		ETH_EPCR_HM

#define	ETH_EPCR_HDM		ETH__BIT(14)
#define	ETH_EPCR_HDM_Discard	0
#define	ETH_EPCR_HDM_Pass	ETH_EPCR_HDM

#define	ETH_EPCR_HD_Half	0
#define	ETH_EPCR_HD_Full	ETH_EPCR_HD_Full

#define	ETH_EPCR_ACCS		ETH__BIT(31)



/*
 * Table 598: Port Configuration Extend Register (PCXR)
 * 00:00 IGMP			IGMP Packets Capture Enable
 *				0: IGMP packets are treated as normal Multicast
 *				   packets.
 *				1: IGMP packets on IPv4/Ipv6 over Ethernet/802.3
 *				   are trapped and sent to high priority RX
 *				   queue.
 * 01:01 SPAN			Spanning Tree Packets Capture Enable
 *				0: BPDU (Bridge Protocol Data Unit) packets are
 *				   treated as normal Multicast packets.
 *				1: BPDU packets are trapped and sent to high
 *				   priority RX queue.
 * 02:02 PAR			Partition Enable (0: Normal, 1: Partition)
 *				When more than 61 collisions occur while
 *				transmitting, the port enters Partition mode.
 *				It waits for the first good packet from the
 *				wire and then goes back to Normal mode.  Under
 *				Partition mode it continues transmitting, but
 *				it does not receive.
 * 05:03 PRIOtx			Priority weight in the round-robin between high
 *				and low priority TX queues.
 *				000: 1 pkt from HIGH, 1 pkt from LOW.
 *				001: 2 pkt from HIGH, 1 pkt from LOW.
 *				010: 4 pkt from HIGH, 1 pkt from LOW.
 *				011: 6 pkt from HIGH, 1 pkt from LOW.
 *				100: 8 pkt from HIGH, 1 pkt from LOW.
 *				101: 10 pkt from HIGH, 1 pkt from LOW.
 *				110: 12 pkt from HIGH, 1 pkt from LOW.
 *				111: All pkt from HIGH, 0 pkt from LOW. LOW is
 *				     served only if HIGH is empty.
 *				NOTE: If the HIGH queue is emptied before
 *				      finishing the count, the count is reset
 *				      until the next first HIGH comes in.
 * 07:06 PRIOrx			Default Priority for Packets Received on this
 *				Port (00: Lowest priority, 11: Highest priority)
 * 08:08 PRIOrx_Override	Override Priority for Packets Received on this
 *				Port (0: Do not override, 1: Override with
 *				<PRIOrx> field)
 * 09:09 DPLXen			Enable Auto-negotiation for Duplex Mode
 *				(0: Enable, 1: Disable)
 * 11:10 FCTLen			Enable Auto-negotiation for 802.3x Flow-control
 *				0: Enable; When enabled, 1 is written (through
 *				   SMI access) to the PHY's register 4 bit 10
 *				   to advertise flow-control capability.
 *				1: Disable; Only enables flow control after the
 *				   PHY address is set by the CPU. When changing
 *				   the PHY address the flow control
 *				   auto-negotiation must be disabled.
 * 11:11 FLP			Force Link Pass
 *				(0: Force Link Pass, 1: Do NOT Force Link pass)
 * 12:12 FCTL			802.3x Flow-Control Mode (0: Enable, 1: Disable)
 *				NOTE: Only valid when auto negotiation for flow
 *				      control is disabled.
 * 13:13 Reserved
 * 15:14 MFL			Max Frame Length
 *				Maximum packet allowed for reception (including
 *				CRC):   00: 1518 bytes,   01: 1536 bytes,
 *					10: 2048 bytes,   11:  64K bytes
 * 16:16 MIBclrMode		MIB Counters Clear Mode (0: Clear, 1: No effect)
 * 17:17 MIBctrMode		Reserved. (MBZ)
 * 18:18 Speed			Port Speed (0: 10Mbit/Sec, 1: 100Mbit/Sec)
 *				NOTE: Only valid if SpeedEn bit is set.
 * 19:19 SpeedEn		Enable Auto-negotiation for Speed
 *				(0: Enable, 1: Disable)
 * 20:20 RMIIen			RMII enable
 *				0: Port functions as MII port
 *				1: Port functions as RMII port
 * 21:21 DSCPen			DSCP enable
 *				0: IP DSCP field decoding is disabled.
 *				1: IP DSCP field decoding is enabled.
 * 31:22 Reserved
 */
#define	ETH_EPCXR_IGMP			ETH__BIT(0)
#define	ETH_EPCXR_SPAN			ETH__BIT(1)
#define	ETH_EPCXR_PAR			ETH__BIT(2)
#define	ETH_EPCXR_PRIOtx_GET(v)		ETH__EXT(v, 3, 3)
#define	ETH_EPCXR_PRIOrx_GET(v)		ETH__EXT(v, 3, 3)
#define	ETH_EPCXR_PRIOrx_Override	ETH__BIT(8)
#define	ETH_EPCXR_DLPXen		ETH__BIT(9)
#define	ETH_EPCXR_FCTLen		ETH__BIT(10)
#define	ETH_EPCXR_FLP			ETH__BIT(11)
#define	ETH_EPCXR_FCTL			ETH__BIT(12)
#define	ETH_EPCXR_MFL_GET(v)		ETH__EXT(v, 14, 2)
#define	ETH_EPCXR_MFL_SET(v)		((v) << 14)
#define	ETH_EPCXR_MFL_MASK		0x3
#define	ETH_EPCXR_MFL_1518		0
#define	ETH_EPCXR_MFL_1536		1
#define	ETH_EPCXR_MFL_2084		2
#define	ETH_EPCXR_MFL_64K		3
#define	ETH_EPCXR_MIBclrMode		ETH__BIT(16)
#define	ETH_EPCXR_MIBctrMode		ETH__BIT(17)
#define	ETH_EPCXR_Speed			ETH__BIT(18)
#define	ETH_EPCXR_SpeedEn		ETH__BIT(19)
#define	ETH_EPCXR_RMIIEn		ETH__BIT(20)
#define	ETH_EPCXR_DSCPEn		ETH__BIT(21)



/*
 * Table 599: Port Command Register (PCMR)
 * 14:00 Reserved
 * 15:15 FJ			Force Jam / Flow Control
 *				When in half-duplex mode, the CPU uses this bit
 *				to force collisions on the Ethernet segment.
 *				When the CPU recognizes that it is going to run
 *				out of receive buffers, it can force the
 *				transmitter to send jam frames, forcing
 *				collisions on the wire.  To allow transmission
 *				on the Ethernet segment, the CPU must clear the
 *				FJ bit when more resources are available.  When
 *				in full-duplex and flow-control is enabled, this
 *				bit causes the port's transmitter to send
 *				flow-control PAUSE packets. The CPU must reset
 *				this bit when more resources are available.
 * 31:16 Reserved
 */

#define	ETH_EPCMR_FJ		ETH__BIT(15)


/*
 * Table 600: Port Status Register (PSR) -- Read Only
 * 00:00 Speed			Indicates Port Speed (0: 10Mbs, 1: 100Mbs)
 * 01:01 Duplex			Indicates Port Duplex Mode (0: Half, 1: Full)
 * 02:02 Fctl			Indicates Flow-control Mode
 *				(0: enabled, 1: disabled)
 * 03:03 Link			Indicates Link Status (0: down, 1: up)
 * 04:04 Pause			Indicates that the port is in flow-control
 *				disabled state.  This bit is set when an IEEE
 *				802.3x flow-control PAUSE (XOFF) packet is
 *				received (assuming that flow-control is
 *				enabled and the port is in full-duplex mode).
 *				Reset when XON is received, or when the XOFF
 *				timer has expired.
 * 05:05 TxLow			Tx Low Priority Status
 *				Indicates the status of the low priority
 *				transmit queue: (0: Stopped, 1: Running)
 * 06:06 TxHigh			Tx High Priority Status
 *				Indicates the status of the high priority
 *				transmit queue: (0: Stopped, 1: Running)
 * 07:07 TXinProg		TX in Progress
 *				Indicates that the port's transmitter is in an
 *				active transmission state.
 * 31:08 Reserved
 */
#define	ETH_EPSR_Speed		ETH__BIT(0)
#define	ETH_EPSR_Duplex		ETH__BIT(1)
#define	ETH_EPSR_Fctl		ETH__BIT(2)
#define	ETH_EPSR_Link		ETH__BIT(3)
#define	ETH_EPSR_Pause		ETH__BIT(4)
#define	ETH_EPSR_TxLow		ETH__BIT(5)
#define	ETH_EPSR_TxHigh		ETH__BIT(6)
#define	ETH_EPSR_TXinProg	ETH__BIT(7)


/*
 * Table 601: Serial Parameters Register (SPR)
 * 01:00 JAM_LENGTH		Two bits to determine the JAM Length
 *				(in Backpressure) as follows:
 *					00 = 12K bit-times
 *					01 = 24K bit-times
 *					10 = 32K bit-times
 *					11 = 48K bit-times
 * 06:02 JAM_IPG		Five bits to determine the JAM IPG.
 *				The step is four bit-times. The value may vary
 *				between 4 bit time to 124.
 * 11:07 IPG_JAM_TO_DATA	Five bits to determine the IPG JAM to DATA.
 *				The step is four bit-times. The value may vary
 *				between 4 bit time to 124.
 * 16:12 IPG_DATA		Inter-Packet Gap (IPG)
 *				The step is four bit-times. The value may vary
 *				between 12 bit time to 124.
 *				NOTE: These bits may be changed only when the
 *				      Ethernet ports is disabled.
 * 21:17 Data_Blind		Data Blinder
 *				The number of nibbles from the beginning of the
 *				IPG, in which the IPG counter is restarted when
 *				detecting a carrier activity.  Following this
 *				value, the port enters the Data Blinder zone and
 *				does not reset the IPG counter. This ensures
 *				fair access to the medium.
 *				The default is 10 hex (64 bit times - 2/3 of the
 *				default IPG).  The step is 4 bit-times. Valid
 *				range is 3 to 1F hex nibbles.
 *				NOTE: These bits may be only changed when the
 *				      Ethernet port is disabled.
 * 22:22 Limit4			The number of consecutive packet collisions that
 *				occur before the collision counter is reset.
 *				  0: The port resets its collision counter after
 *				     16 consecutive retransmit trials and
 *				     restarts the Backoff algorithm.
 *				  1: The port resets its collision counter and
 *				     restarts the Backoff algorithm after 4
 *				     consecutive transmit trials.
 * 31:23 Reserved
 */
#define	ETH_ESPR_JAM_LENGTH_GET(v)	ETH__EXT(v, 0, 2)
#define	ETH_ESPR_JAM_IPG_GET(v)		ETH__EXT(v, 2, 5)
#define	ETH_ESPR_IPG_JAM_TO_DATA_GET(v)	ETH__EXT(v, 7, 5)
#define	ETH_ESPR_IPG_DATA_GET(v)	ETH__EXT(v, 12, 5)
#define	ETH_ESPR_Data_Bilnd_GET(v)	ETH__EXT(v, 17, 5)
#define	ETH_ESPR_Limit4(v)		ETH__BIT(22)

/*
 * Table 602: Hash Table Pointer Register (HTPR)
 * 31:00 HTP			32-bit pointer to the address table.
 *				Bits [2:0] must be set to zero.
 */

/*
 * Table 603: Flow Control Source Address Low (FCSAL)
 * 15:0 SA[15:0]		Source Address
 *				The least significant bits of the source
 *				address for the port.  This address is used for
 *				Flow Control.
 * 31:16 Reserved
 */

/*
 * Table 604: Flow Control Source Address High (FCSAH)
 * 31:0 SA[47:16]		Source Address
 *				The most significant bits of the source address
 *				for the port.  This address is used for Flow
 *				Control.
 */


/*
 * Table 605: SDMA Configuration Register (SDCR)
 * 01:00 Reserved
 * 05:02 RC			Retransmit Count
 *				Sets the maximum number of retransmits per
 *				packet.  After executing retransmit for RC
 *				times, the TX SDMA closes the descriptor with a
 *				Retransmit Limit error indication and processes
 *				the next packet.  When RC is set to 0, the
 *				number of retransmits is unlimited. In this
 *				case, the retransmit process is only terminated
 *				if CPU issues an Abort command.
 * 06:06 BLMR			Big/Little Endian Receive Mode
 *				The DMA supports Big or Little Endian
 *				configurations on a per channel basis. The BLMR
 *				bit only affects data transfer to memory.
 *					0: Big Endian
 *					1: Little Endian
 * 07:07 BLMT			Big/Little Endian Transmit Mode
 *				The DMA supports Big or Little Endian
 *				configurations on a per channel basis. The BLMT
 *				bit only affects data transfer from memory.
 *					0: Big Endian
 *					1: Little Endian
 * 08:08 POVR			PCI Override
 *				When set, causes the SDMA to direct all its
 *				accesses in PCI_0 direction and overrides
 *				normal address decoding process.
 * 09:09 RIFB			Receive Interrupt on Frame Boundaries
 *				When set, the SDMA Rx generates interrupts only
 *				on frame boundaries (i.e. after writing the
 *				frame status to the descriptor).
 * 11:10 Reserved
 * 13:12 BSZ			Burst Size
 *				Sets the maximum burst size for SDMA
 *				transactions:
 *					00: Burst is limited to 1 64bit words.
 *					01: Burst is limited to 2 64bit words.
 *					10: Burst is limited to 4 64bit words.
 *					11: Burst is limited to 8 64bit words.
 * 31:14 Reserved
 */
#define	ETH_ESDCR_RC_GET(v)		ETH__EXT(v, 2, 4)
#define	ETH_ESDCR_BLMR			ETH__BIT(6)
#define	ETH_ESDCR_BLMT			ETH__BIT(7)
#define	ETH_ESDCR_POVR			ETH__BIT(8)
#define	ETH_ESDCR_RIFB			ETH__BIT(9)
#define	ETH_ESDCR_BSZ_GET(v)		ETH__EXT(v, 12, 2)
#define	ETH_ESDCR_BSZ_SET(v, n)		(ETH__CLR(v, 12, 2),\
					 (v) |= ETH__INS(n, 12))
#define	ETH_ESDCR_BSZ_1			0
#define	ETH_ESDCR_BSZ_2			1
#define	ETH_ESDCR_BSZ_4			2
#define	ETH_ESDCR_BSZ_8			3

#define	ETH_ESDCR_BSZ_Strings		{ "1 64-bit word", "2 64-bit words", \
					  "4 64-bit words", "8 64-bit words" }

/*
 * Table 606: SDMA Command Register (SDCMR)
 * 06:00 Reserved
 * 07:07 ERD			Enable RX DMA.
 *				Set to 1 by the CPU to cause the SDMA to start
 *				a receive process.  Cleared when the CPU issues
 *				an Abort Receive command.
 * 14:08 Reserved
 * 15:15 AR			Abort Receive
 *				Set to 1 by the CPU to abort a receive SDMA
 *				operation.  When the AR bit is set, the SDMA
 *				aborts its current operation and moves to IDLE.
 *				No descriptor is closed.  The AR bit is cleared
 *				upon entering IDLE.  After setting the AR bit,
 *				the CPU must poll the bit to verify that the
 *				abort sequence is completed.
 * 16:16 STDH			Stop TX High
 *				Set to 1 by the CPU to stop the transmission
 *				process from the high priority queue at the end
 *				of the current frame. An interrupt is generated
 *				when the stop command has been executed.
 *				  Writing 1 to STDH resets TXDH bit.
 *				  Writing 0 to this bit has no effect.
 * 17:17 STDL			Stop TX Low
 *				Set to 1 by the CPU to stop the transmission
 *				process from the low priority queue at the end
 *				of the current frame. An interrupt is generated
 *				when the stop command has been executed.
 *				  Writing 1 to STDL resets TXDL bit.
 *				  Writing 0 to this bit has no effect.
 * 22:18 Reserved
 * 23:23 TXDH			Start Tx High
 *				Set to 1 by the CPU to cause the SDMA to fetch
 *				the first descriptor and start a transmit
 *				process from the high priority Tx queue.
 *				  Writing 1 to TXDH resets STDH bit.
 *				  Writing 0 to this bit has no effect.
 * 24:24 TXDL			Start Tx Low
 *				Set to 1 by the CPU to cause the SDMA to fetch
 *				the first descriptor and start a transmit
 *				process from the low priority Tx queue.
 *				  Writing 1 to TXDL resets STDL bit.
 *				  Writing 0 to this bit has no effect.
 * 30:25 Reserved
 * 31:31 AT			Abort Transmit
 *				Set to 1 by the CPU to abort a transmit DMA
 *				operation.  When the AT bit is set, the SDMA
 *				aborts its current operation and moves to IDLE.
 *				No descriptor is closed.  Cleared upon entering
 *				IDLE.  After setting AT bit, the CPU must poll
 *				it in order to verify that the abort sequence
 *				is completed.
 */
#define	ETH_ESDCMR_ERD			ETH__BIT(7)
#define	ETH_ESDCMR_AR			ETH__BIT(15)
#define	ETH_ESDCMR_STDH			ETH__BIT(16)
#define	ETH_ESDCMR_STDL			ETH__BIT(17)
#define	ETH_ESDCMR_TXDH			ETH__BIT(23)
#define	ETH_ESDCMR_TXDL			ETH__BIT(24)
#define	ETH_ESDCMR_AT			ETH__BIT(31)

/*
 * Table 607: Interrupt Cause Register (ICR)
 * 00:00 RxBuffer		Rx Buffer Return
 *				Indicates an Rx buffer returned to CPU ownership
 *				or that the port finished reception of a Rx
 *				frame in either priority queues.
 *				NOTE: In order to get a Rx Buffer return per
 *				      priority queue, use bit 19:16. This bit is
 *				      set upon closing any Rx descriptor which
 *				      has its EI bit set. To limit the
 *				      interrupts to frame (rather than buffer)
 *				      boundaries, the user must set SDMA
 *				      Configuration register's RIFB bit. When
 *				      the RIFB bit is set, an interrupt
 *				      generates only upon closing the first
 *				      descriptor of a received packet, if this
 *				      descriptor has it EI bit set.
 * 01:01 Reserved
 * 02:02 TxBufferHigh		Tx Buffer for High priority Queue
 *				Indicates a Tx buffer returned to CPU ownership
 *				or that the port finished transmission of a Tx
 *				frame.
 *				NOTE: This bit is set upon closing any Tx
 *				      descriptor which has its EI bit set. To
 *				      limit the interrupts to frame (rather than
 *				      buffer) boundaries, the user must set EI
 *				      only in the last descriptor.
 * 03:03 TxBufferLow		Tx Buffer for Low Priority Queue
 *				Indicates a Tx buffer returned to CPU ownership
 *				or that the port finished transmission of a Tx
 *				frame.
 *				NOTE: This bit is set upon closing any Tx
 *				      descriptor which has its EI bit set. To
 *				      limit the interrupts to frame (rather than
 *				      buffer) boundaries, the user must set EI
 *				      only in the last descriptor.
 * 05:04 Reserved
 * 06:06 TxEndHigh		Tx End for High Priority Queue
 *				Indicates that the Tx DMA stopped processing the
 *				high priority queue after stop command, or that
 *				it reached the end of the high priority
 *				descriptor chain.
 * 07:07 TxEndLow		Tx End for Low Priority Queue
 *				Indicates that the Tx DMA stopped processing the
 *				low priority queue after stop command, or that
 *				it reached the end of the low priority
 *				descriptor chain.
 * 08:08 RxError		Rx Resource Error
 *				Indicates a Rx resource error event in one of
 *				the priority queues.
 *				NOTE: To get a Rx Resource Error Indication per
 *				      priority queue, use bit 23:20.
 * 09:09 Reserved
 * 10:10 TxErrorHigh		Tx Resource Error for High Priority Queue
 *				Indicates a Tx resource error event during
 *				packet transmission from the high priority queue
 * 11:11 TxErrorLow		Tx Resource Error for Low Priority Queue
 *				Indicates a Tx resource error event during
 *				packet transmission from the low priority queue
 * 12:12 RxOVR			Rx Overrun
 *				Indicates an overrun event that occurred during
 *				reception of a packet.
 * 13:13 TxUdr			Tx Underrun
 *				Indicates an underrun event that occurred during
 *				transmission of packet from either queue.
 * 15:14 Reserved
 * 16:16 RxBuffer-Queue[0]	Rx Buffer Return in Priority Queue[0]
 *				Indicates a Rx buffer returned to CPU ownership
 *				or that the port completed reception of a Rx
 *				frame in a receive priority queue[0]
 * 17:17 RxBuffer-Queue[1]	Rx Buffer Return in Priority Queue[1]
 *				Indicates a Rx buffer returned to CPU ownership
 *				or that the port completed reception of a Rx
 *				frame in a receive priority queue[1].
 * 18:18 RxBuffer-Queue[2]	Rx Buffer Return in Priority Queue[2]
 *				Indicates a Rx buffer returned to CPU ownership
 *				or that the port completed reception of a Rx
 *				frame in a receive priority queue[2].
 * 19:19 RxBuffer-Queue[3]	Rx Buffer Return in Priority Queue[3]
 *				Indicates a Rx buffer returned to CPU ownership
 *				or that the port completed reception of a Rx
 *				frame in a receive priority queue[3].
 * 20:20 RxError-Queue[0]	Rx Resource Error in Priority Queue[0]
 *				Indicates a Rx resource error event in receive
 *				priority queue[0].
 * 21:21 RxError-Queue[1]	Rx Resource Error in Priority Queue[1]
 *				Indicates a Rx resource error event in receive
 *				priority queue[1].
 * 22:22 RxError-Queue[2]	Rx Resource Error in Priority Queue[2]
 *				Indicates a Rx resource error event in receive
 *				priority queue[2].
 * 23:23 RxError-Queue[3]	Rx Resource Error in Priority Queue[3]
 *				Indicates a Rx resource error event in receive
 *				priority queue[3].
 * 27:24 Reserved
 * 28:29 MIIPhySTC		MII PHY Status Change
 *				Indicates a status change reported by the PHY
 *				connected to this port.  Set when the MII
 *				management interface block identifies a change
 *				in PHY's register 1.
 * 29:29 SMIdone		SMI Command Done
 *				Indicates that the SMI completed a MII
 *				management command (either read or write) that
 *				was initiated by the CPU writing to the SMI
 *				register.
 * 30:30 Reserved
 * 31:31 EtherIntSum		Ethernet Interrupt Summary
 *				This bit is a logical OR of the (unmasked) bits
 *				[30:04] in the Interrupt Cause register.
 */

#define	ETH_IR_RxBuffer		ETH__BIT(0)
#define	ETH_IR_TxBufferHigh	ETH__BIT(2)
#define	ETH_IR_TxBufferLow	ETH__BIT(3)
#define	ETH_IR_TxEndHigh	ETH__BIT(6)
#define	ETH_IR_TxEndLow		ETH__BIT(7)
#define	ETH_IR_RxError		ETH__BIT(8)
#define	ETH_IR_TxErrorHigh	ETH__BIT(10)
#define	ETH_IR_TxErrorLow	ETH__BIT(11)
#define	ETH_IR_RxOVR		ETH__BIT(12)
#define	ETH_IR_TxUdr		ETH__BIT(13)
#define	ETH_IR_RxBuffer_0	ETH__BIT(16)
#define	ETH_IR_RxBuffer_1	ETH__BIT(17)
#define	ETH_IR_RxBuffer_2	ETH__BIT(18)
#define	ETH_IR_RxBuffer_3	ETH__BIT(19)
#define	ETH_IR_RxBuffer_GET(v)	ETH__EXT(v, 16, 4)
#define	ETH_IR_RxError_0	ETH__BIT(20)
#define	ETH_IR_RxError_1	ETH__BIT(21)
#define	ETH_IR_RxError_2	ETH__BIT(22)
#define	ETH_IR_RxError_3	ETH__BIT(23)
#define	ETH_IR_RxError_GET(v)	ETH__EXT(v, 20, 4)
#define	ETH_IR_RxBits		(ETH_IR_RxBuffer_0|\
				 ETH_IR_RxBuffer_1|\
				 ETH_IR_RxBuffer_2|\
				 ETH_IR_RxBuffer_3|\
				 ETH_IR_RxError_0|\
				 ETH_IR_RxError_1|\
				 ETH_IR_RxError_2|\
				 ETH_IR_RxError_3)
#define	ETH_IR_MIIPhySTC	ETH__BIT(28)
#define	ETH_IR_SMIdone		ETH__BIT(29)
#define	ETH_IR_EtherIntSum	ETH__BIT(31)
#define	ETH_IR_Summary		ETH__BIT(31)

/*
 * Table 608: Interrupt Mask Register (IMR)
 * 31:00 Various		Mask bits for the Interrupt Cause register.
 */

/*
 * Table 609: IP Differentiated Services CodePoint to Priority0 low (DSCP2P0L),
 * 31:00 Priority0_low		The LSB priority bits for DSCP[31:0] entries.
 */

/*
 * Table 610: IP Differentiated Services CodePoint to Priority0 high (DSCP2P0H)
 * 31:00 Priority0_high		The LSB priority bits for DSCP[63:32] entries.
 */

/*
 * Table 611: IP Differentiated Services CodePoint to Priority1 low (DSCP2P1L)
 * 31:00 Priority1_low		The MSB priority bits for DSCP[31:0] entries.
 */

/*
 * Table 612: IP Differentiated Services CodePoint to Priority1 high (DSCP2P1H)
 * 31:00 Priority1_high		The MSB priority bit for DSCP[63:32] entries.
 */

/*
 * Table 613: VLAN Priority Tag to Priority (VPT2P)
 * 07:00 Priority0		The LSB priority bits for VLAN Priority[7:0]
 *				entries.
 * 15:08 Priority1		The MSB priority bits for VLAN Priority[7:0]
 *				entries.
 * 31:16 Reserved
 */
#endif /* _DEV_GTETHREG_H_ */

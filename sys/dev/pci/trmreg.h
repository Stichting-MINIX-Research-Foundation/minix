/*	$NetBSD: trmreg.h,v 1.2 2012/05/10 03:16:50 macallan Exp $	*/
/*
 * Device Driver for Tekram DC395U/UW/F, DC315/U
 * PCI SCSI Bus Master Host Adapter
 * (SCSI chip set used Tekram ASIC TRM-S1040)
 *
 * Copyright (c) 2001 Rui-Xiang Guo
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Ported from
 *   dc395x_trm.h
 *
 * Written for NetBSD 1.4.x by
 *   Erich Chen     (erich@tekram.com.tw)
 *
 * Provided by
 *   (C)Copyright 1995-1999 Tekram Technology Co., Ltd. All rights reserved.
 */

#define TRM_BAR_PIO	0x10
/* DC-315 has an MMIO BAR */
#define TRM_BAR_MMIO	0x14

/*
 **********************************************************************
 *
 * The SCSI register offset for TRM_S1040
 *
 **********************************************************************
 */
#define TRM_SCSI_STATUS   	0x80	/* SCSI Status (R) */
#define     COMMANDPHASEDONE	 0x2000	/* SCSI command phase done     */
#define     SCSIXFERDONE         0x0800	/* SCSI transfer done          */
#define     SCSIXFERCNT_2_ZERO   0x0100	/* SCSI transfer count to zero */
#define     SCSIINTERRUPT        0x0080	/* SCSI interrupt pending      */
#define     COMMANDABORT         0x0040	/* SCSI command abort	       */
#define     SEQUENCERACTIVE      0x0020	/* SCSI sequencer active       */
#define     PHASEMISMATCH        0x0010	/* SCSI phase mismatch	       */
#define     PARITYERROR	         0x0008	/* SCSI parity error	       */
#define     PHASEMASK	         0x0007	/* Phase MSG/CD/IO	       */
#define	      PH_DATA_OUT	 0x00	/* Data out phase	     */
#define	      PH_DATA_IN	 0x01	/* Data in phase	     */
#define	      PH_COMMAND	 0x02	/* Command phase	     */
#define	      PH_STATUS		 0x03	/* Status phase		     */
#define	      PH_BUS_FREE	 0x05	/* Invalid phase as bus free */
#define	      PH_MSG_OUT	 0x06	/* Message out phase	     */
#define	      PH_MSG_IN		 0x07	/* Message in phase	     */
#define TRM_SCSI_CONTROL  	0x80	/* SCSI Control (W) */
#define     DO_CLRATN	         0x0400	/* Clear ATN	               */
#define     DO_SETATN	         0x0200	/* Set ATN		       */
#define     DO_CMDABORT	         0x0100	/* Abort SCSI command          */
#define     DO_RSTMODULE         0x0010	/* Reset SCSI chip             */
#define     DO_RSTSCSI	         0x0008	/* Reset SCSI bus	       */
#define     DO_CLRFIFO	         0x0004	/* Clear SCSI transfer FIFO    */
#define     DO_DATALATCH    	 0x0002	/* Enable SCSI bus data latch  */
#define     DO_HWRESELECT        0x0001	/* Enable hardware reselection */
#define TRM_SCSI_FIFOCNT  	0x82	/* SCSI FIFO Counter (R) */
#define     SCSI_FIFOCNT_MASK	 0x1F	/* 5 bits SCSI FIFO counter */
#define     SCSI_FIFO_EMPTY	 0x40	/* SCSI FIFO Empty          */
#define TRM_SCSI_SIGNAL   	0x83	/* SCSI low level signal (R/W) */
#define TRM_SCSI_INTSTATUS	0x84	/* SCSI Interrupt Status (R) */
#define     INT_SCAM	         0x80	/* SCAM selection interrupt      */
#define     INT_SELECT	         0x40	/* Selection interrupt	         */
#define     INT_SELTIMEOUT       0x20	/* Selection timeout interrupt   */
#define     INT_DISCONNECT       0x10	/* Bus disconnected interrupt    */
#define     INT_RESELECTED       0x08	/* Reselected interrupt	         */
#define     INT_SCSIRESET        0x04	/* SCSI reset detected interrupt */
#define     INT_BUSSERVICE       0x02	/* Bus service interrupt         */
#define     INT_CMDDONE	         0x01	/* SCSI command done interrupt   */
#define TRM_SCSI_OFFSET   	0x84	/* SCSI Offset Count (W) */
/*
 *   Bit		Name	        Definition
 *   07-05	0	RSVD	        Reversed. Always 0.
 *   04 	0	OFFSET4	        Reversed for LVDS. Always 0.
 *   03-00	0	OFFSET[03:00]	Offset number from 0 to 15
 */
#define TRM_SCSI_SYNC	       	0x85	/* SCSI Sync Control (R/W) */
#define     LVDS_SYNC	         0x20	/* Enable LVDS sync          	 */
#define     WIDE_SYNC	         0x10	/* Enable WIDE sync          	 */
#define     ALT_SYNC	         0x08	/* Enable Fast-20 alternate sync */
/*
 * SYNCM	7    6	  5    4	3   	2   	1   	0
 * Name 	RSVD RSVD LVDS WIDE	ALTPERD	PERIOD2	PERIOD1	PERIOD0
 * Default	0    0	  0    0	0       0       0	0
 *
 * Bit		    Name                Definition
 * 07-06	0   RSVD                Reversed. Always read 0
 * 05   	0   LVDS                Reversed. Always read 0
 * 04   	0   WIDE/WSCSI          Enable wide (16-bits) SCSI transfer.
 * 03   	0   ALTPERD/ALTPD	Alternate (Sync./Period) mode.
 *
 *                                      @@ When this bit is set,
 *                                         the synchronous period bits 2:0
 *                                         in the Synchronous Mode register
 *                                         are used to transfer data
 *                                         at the Fast-20 rate.
 *                                      @@ When this bit is reset,
 *                                         the synchronous period bits 2:0
 *                                         in the Synchronous Mode Register
 *                                         are used to transfer data
 *                                         at the Fast-40 rate.
 *
 * 02-00	0   PERIOD[2:0]/SXPD[02:00]	Synchronous SCSI Transfer Rate.
 *                                      These 3 bits specify
 *                                      the Synchronous SCSI Transfer Rate
 *                                      for Fast-20 and Fast-10.
 *                                      These bits are also reset
 *                                      by a SCSI Bus reset.
 *
 * For Fast-10 bit ALTPD = 0 and LVDS = 0
 *     and bit2,bit1,bit0 is defined as follows :
 *
 *  	       000	100ns, 10.0 Mbytes/s
 *   	       001	150ns,  6.6 Mbytes/s
 *  	       010	200ns,  5.0 Mbytes/s
 *  	       011	250ns,  4.0 Mbytes/s
 *   	       100	300ns,  3.3 Mbytes/s
 *  	       101	350ns,  2.8 Mbytes/s
 *	       110	400ns,  2.5 Mbytes/s
 *	       111	450ns,  2.2 Mbytes/s
 *
 * For Fast-20 bit ALTPD = 1 and LVDS = 0
 *     and bit2,bit1,bit0 is defined as follows :
 *
 *	       000	 50ns, 20.0 Mbytes/s
 *	       001	 75ns, 13.3 Mbytes/s
 *	       010	100ns, 10.0 Mbytes/s
 *	       011	125ns,  8.0 Mbytes/s
 *	       100	150ns,  6.6 Mbytes/s
 *	       101	175ns,  5.7 Mbytes/s
 *	       110	200ns,  5.0 Mbytes/s
 *	       111	250ns,  4.0 Mbytes/s
 *
 * For Fast-40 bit ALTPD = 0 and LVDS = 1
 *     and bit2,bit1,bit0 is defined as follows :
 *
 *	       000	 25ns, 40.0 Mbytes/s
 *	       001	 50ns, 20.0 Mbytes/s
 *	       010	 75ns, 13.3 Mbytes/s
 *	       011	100ns, 10.0 Mbytes/s
 *	       100	125ns,  8.0 Mbytes/s
 *	       101	150ns,  6.6 Mbytes/s
 *	       110	175ns,  5.7 Mbytes/s
 *	       111	200ns,  5.0 Mbytes/s
 */
#define TRM_SCSI_TARGETID 	0x86	/* SCSI Target ID (R/W) */
#define TRM_SCSI_IDMSG    	0x87	/* SCSI Identify Message (R) */
#define TRM_SCSI_HOSTID   	0x87	/* SCSI Host ID (W) */
#define TRM_SCSI_XCNT		0x88	/* SCSI Transfer Counter (R/W) */
#define     SCSI_XCNT_MASK 0x00FFFFFF	/* 24 bits SCSI transfer counter */
#define TRM_SCSI_INTEN    	0x8C	/* SCSI Interrupt Enable (R/W) */
#define     EN_SCAM	         0x80	/* Enable SCAM selection intr      */
#define     EN_SELECT	         0x40	/* Enable selection intr       	   */
#define     EN_SELTIMEOUT        0x20	/* Enable selection timeout intr   */
#define     EN_DISCONNECT        0x10	/* Enable bus disconnected intr	   */
#define     EN_RESELECTED        0x08	/* Enable reselected intr      	   */
#define     EN_SCSIRESET         0x04	/* Enable SCSI reset detected intr */
#define     EN_BUSSERVICE        0x02	/* Enable bus service intr     	   */
#define     EN_CMDDONE	         0x01	/* Enable SCSI command done intr   */
#define TRM_SCSI_CONFIG0  	0x8D	/* SCSI Configuration 0 (R/W) */
#define     PHASELATCH	         0x40	/* Enable phase latch	 */
#define     INITIATOR	         0x20	/* Enable initiator mode */
#define     PARITYCHECK	         0x10	/* Enable parity check	 */
#define     BLOCKRST	         0x01	/* Disable SCSI reset    */
#define TRM_SCSI_CONFIG1  	0x8E	/* SCSI Configuration 1 (R/W) */
#define     ACTIVE_NEGPLUS       0x10	/* Enhance active negation  */
#define     FILTER_DISABLE       0x08	/* Disable SCSI data filter */
#define     ACTIVE_NEG	         0x02	/* Enable active negation   */
#define TRM_SCSI_COMMAND   	0x90	/* SCSI Command (R/W) */
#define     SCMD_COMP	         0x12	/* Command complete	       */
#define     SCMD_SEL_ATN         0x60	/* Selection with ATN          */
#define     SCMD_SEL_ATN3        0x64	/* Selection with ATN3         */
#define     SCMD_SEL_ATNSTOP     0xB8	/* Selection with ATN and Stop */
#define     SCMD_FIFO_OUT        0xC0	/* SCSI FIFO transfer out      */
#define     SCMD_DMA_OUT         0xC1	/* SCSI DMA transfer out       */
#define     SCMD_FIFO_IN         0xC2	/* SCSI FIFO transfer in       */
#define     SCMD_DMA_IN	         0xC3	/* SCSI DMA transfer in	       */
#define     SCMD_MSGACCEPT       0xD8	/* Message accept	       */
/*
 *  Code Command Description
 *
 *  02	 Enable reselection with FIFO
 *  40   Select without ATN with FIFO
 *  60   Select with ATN with FIFO
 *  64   Select with ATN3 with FIFO
 *  A0   Select with ATN and stop with FIFO
 *  C0   Transfer information out with FIFO
 *  C1   Transfer information out with DMA
 *  C2   Transfer information in with FIFO
 *  C3   Transfer information in with DMA
 *  12   Initiator command complete with FIFO
 *  50   Initiator transfer information out sequence without ATN with FIFO
 *  70   Initiator transfer information out sequence with ATN with FIFO
 *  74   Initiator transfer information out sequence with ATN3 with FIFO
 *  52   Initiator transfer information in sequence without ATN with FIFO
 *  72   Initiator transfer information in sequence with ATN with FIFO
 *  76	 Initiator transfer information in sequence with ATN3 with FIFO
 *  90   Initiator transfer information out command complete with FIFO
 *  92   Initiator transfer information in command complete with FIFO
 *  D2   Enable selection
 *  08   Reselection
 *  48   Disconnect command with FIFO
 *  88   Terminate command with FIFO
 *  C8   Target command complete with FIFO
 *  18   SCAM Arbitration/ Selection
 *  5A   Enable reselection
 *  98   Select without ATN with FIFO
 *  B8   Select with ATN with FIFO
 *  D8   Message Accepted
 *  58   NOP
 */
#define TRM_SCSI_TIMEOUT  	0x91	/* SCSI Time Out Value (R/W) */
#define     SEL_TIMEOUT		 153	/* 250ms selection timeout (@ 40 MHz) */
#define TRM_SCSI_FIFO     	0x98	/* SCSI FIFO (R/W) */
#define TRM_SCSI_TCR0     	0x9C	/* SCSI Target Control 0 (R/W) */
#define     TCR0_WIDE_NEGO_DONE	 0x8000	/* Wide nego done      	 */
#define     TCR0_SYNC_NEGO_DONE	 0x4000	/* Sync nego done      	 */
#define     TCR0_ENABLE_LVDS     0x2000	/* Enable LVDS sync    	 */
#define     TCR0_ENABLE_WIDE     0x1000	/* Enable WIDE sync    	 */
#define     TCR0_ENABLE_ALT	 0x0800	/* Enable alternate sync */
#define     TCR0_PERIOD_MASK     0x0700	/* Transfer rate       	 */
#define     TCR0_DO_WIDE_NEGO    0x0080	/* Do wide NEGO	       	 */
#define     TCR0_DO_SYNC_NEGO    0x0040	/* Do sync NEGO	       	 */
#define     TCR0_DISCONNECT_EN	 0x0020	/* Disconnection enable	 */
#define     TCR0_OFFSET_MASK	 0x001F	/* Offset number       	 */
#define TRM_SCSI_TCR1     	0x9E	/* SCSI Target Control 1 (R/W) */
#define     MAXTAG_MASK	         0x7F00	/* Maximum tags (127)	  */
#define     NON_TAG_BUSY         0x0080	/* Non tag command active */
#define     ACTTAG_MASK	         0x007F	/* Active tags		  */
/*
 **********************************************************************
 *
 * The DMA register offset for TRM_S1040
 *
 **********************************************************************
 */
#define TRM_DMA_COMMAND   	0xA0	/* DMA Command (R/W) */
#define     SGXFER		 0x02	/* Scatter/Gather transfer */
#define     XFERDATAIN	         0x01	/* Transfer data in        */
#define     XFERDATAOUT	         0x00	/* Transfer data out       */
#define TRM_DMA_CONTROL   	0xA1	/* DMA Control (W) */
#define     STOPDMAXFER	         0x08	/* Stop  DMA transfer      */
#define     ABORTXFER	         0x04	/* Abort DMA transfer      */
#define     CLRXFIFO	         0x02	/* Clear DMA transfer FIFO */
#define     STARTDMAXFER         0x01	/* Start DMA transfer      */
#define TRM_DMA_FIFOCNT		0xA1	/* DMA FIFO Counter (R) */
#define     DMA_FIFOCNT_MASK	 0xFF	/* Data FIFO Count */
#define TRM_DMA_FIFOSTATUS	0xA2	/* DMA FIFO Status (R) */
#define     DMA_FIFO_EMPTY	 0x80	/* DMA FIFO Empty */
#define     DMA_FIFO_FULL	 0x01	/* DMA FIFO Full  */
#define TRM_DMA_STATUS    	0xA3	/* DMA Interrupt Status (R/W) */
#define     XFERPENDING	         0x80	/* Transfer pending	           */
#define     DMAXFERCOMP	         0x02	/* Bus Master XFER Complete status */
#define     SCSICOMP	         0x01	/* SCSI complete interrupt         */
#define TRM_DMA_INTEN  	       	0xA4	/* DMA Interrupt Enable (R/W) */
#define     EN_SCSIINTR	         0x01	/* Enable SCSI complete interrupt */
#define TRM_DMA_CONFIG    	0xA6	/* DMA Configuration (R/W) */
#define     DMA_ENHANCE	         0x8000	/* Enable DMA enhance feature */
#define TRM_DMA_XCNT   	       	0xA8	/* DMA Transfer Counter (R/W) */
#define TRM_DMA_CXCNT           0xAC	/* DMA Current Transfer Counter (R) */
#define TRM_DMA_XLOWADDR  	0xB0	/* DMA Xfer Physical Low Addr (R/W) */
#define TRM_DMA_XHIGHADDR 	0xB4	/* DMA Xfer Physical High Addr (R/W) */
/*
 **********************************************************************
 *
 * The general register offset for TRM_S1040
 *
 **********************************************************************
 */
#define TRM_GEN_CONTROL   	0xD4	/* Global Control (R/W) */
#define     EN_LED		 0x80	/* Enable Control onboard LED         */
#define     EN_EEPROM	         0x10	/* Enable EEPROM programming          */
#define     AUTOTERM	         0x04	/* Enable Auto SCSI terminator        */
#define     LOW8TERM	         0x02	/* Enable Lower 8 bit SCSI terminator */
#define     UP8TERM	         0x01	/* Enable Upper 8 bit SCSI terminator */
#define TRM_GEN_STATUS    	0xD5	/* Global Status (R) */
#define     GTIMEOUT	         0x80	/* Global timer reach 0         */
#define     CON5068	         0x10	/* External 50/68 pin connected */
#define     CON68	         0x08	/* Internal 68 pin connected    */
#define     CON50	         0x04	/* Internal 50 pin connected    */
#define     WIDESCSI	         0x02	/* Wide SCSI card	        */
#define TRM_GEN_NVRAM     	0xD6	/* Serial NON-VOLATILE RAM port (R/W) */
#define     NVR_BITOUT	         0x08	/* Serial data out */
#define     NVR_BITIN	         0x04	/* Serial data in  */
#define     NVR_CLOCK	         0x02	/* Serial clock	   */
#define     NVR_SELECT	         0x01	/* Serial select   */
#define TRM_GEN_EDATA     	0xD7	/* Parallel EEPROM data port (R/W) */
#define TRM_GEN_EADDRESS  	0xD8	/* Parallel EEPROM address (R/W) */
#define TRM_GEN_TIMER       	0xDB	/* Global timer (R/W) */

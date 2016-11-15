/*	$NetBSD: ihareg.h,v 1.9 2008/05/03 05:21:25 tsutsui Exp $ */

/*-
 * Device driver for the INI-9XXXU/UW or INIC-940/950 PCI SCSI Controller.
 *
 *  Written for 386bsd and FreeBSD by
 *	Winston Hung		<winstonh@initio.com>
 *
 * Copyright (c) 1997-1999 Initio Corp.
 * Copyright (c) 2000 Ken Westerback
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Ported to NetBSD by Izumi Tsutsui <tsutsui@NetBSD.org> from OpenBSD:
 * $OpenBSD: iha.h,v 1.2 2001/02/08 17:35:05 krw Exp $
 */

/*
 *  Tulip (aka inic-940/950) PCI Configuration Space Initio Specific Registers
 *
 *  Offsets 0x00 through 0x3f are the standard PCI Configuration Header
 *  registers.
 *
 *  Offsets 0x40 through 0x4f, 0x51, 0x53, 0x57, 0x5b, 0x5e and 0x5f are
 *  reserved registers.
 *
 *  Registers 0x50 and 0x52 always read as 0.
 *
 *  The register offset names and associated bit field names are taken
 *  from the Inic-950 Data Sheet, Version 2.1, March 1997
 */
#define TUL_GCTRL0	0x54	       /* R/W Global Control 0		     */
#define     EEPRG	    0x04       /*     Enable EEPROM Programming	     */
#define TUL_GCTRL1	0x55	       /* R/W Global Control 1		     */
#define     ATDEN	    0x01       /*     Auto Termination Detect Enable */
#define TUL_GSTAT	0x56	       /* R/W Global Status - connector type */
#define TUL_EPAD0	0x58	       /* R/W External EEPROM Addr (lo byte) */
#define TUL_EPAD1	0x59	       /* R/W External EEPROM Addr (hi byte) */
#define TUL_PNVPG	0x5A	       /* R/W Data port to external BIOS     */
#define TUL_EPDATA	0x5C	       /* R/W EEPROM Data port		     */
#define TUL_NVRAM	0x5D	       /* R/W Non-volatile RAM port	     */
#define     READ	    0x80       /*     Read from given NVRAM addr     */
#define     WRITE           0x40       /*     Write to given NVRAM addr	     */
#define     ENABLE_ERASE    0x30       /*     Enable NVRAM Erase/Write       */
#define     NVRCS	    0x08       /*     Select external NVRAM	     */
#define     NVRCK	    0x04       /*     NVRAM Clock		     */
#define     NVRDO	    0x02       /*     NVRAM Write Data		     */
#define     NVRDI	    0x01       /*     NVRAM Read  Data		     */

/*
 *   Tulip (aka inic-940/950) SCSI Registers
 */
#define TUL_STCNT0	0x80	       /* R/W 24 bit SCSI Xfer Count	     */
#define     TCNT	    0x00ffffff /*     SCSI Xfer Transfer Count	     */
#define TUL_SFIFOCNT	0x83	       /* R/W  5 bit FIFO counter	     */
#define     FIFOC	    0x1f       /*     SCSI Offset Fifo Count	     */
#define TUL_SISTAT	0x84	       /* R   Interrupt Register	     */
#define     RSELED	    0x80       /*     Reselected		     */
#define     STIMEO	    0x40       /*     Selected/Reselected Timeout    */
#define     SBSRV	    0x20       /*     SCSI Bus Service		     */
#define     SRSTD	    0x10       /*     SCSI Reset Detected	     */
#define     DISCD	    0x08       /*     Disconnected Status	     */
#define     SELED	    0x04       /*     Select Interrupt		     */
#define     SCAMSCT	    0x02       /*     SCAM selected		     */
#define     SCMDN	    0x01       /*     Command Complete		     */
#define TUL_SIEN	0x84	       /* W   Interrupt enable		     */
#define     ALL_INTERRUPTS  0xff
#define TUL_STAT0	0x85	       /* R   Status 0			     */
#define     INTPD	    0x80       /*     Interrupt pending		     */
#define     SQACT	    0x40       /*     Sequencer active		     */
#define     XFCZ	    0x20       /*     Xfer counter zero		     */
#define     SFEMP	    0x10       /*     FIFO empty		     */
#define     SPERR	    0x08       /*     SCSI parity error		     */
#define     PH_MASK	    0x07       /*     SCSI phase mask		     */
#define TUL_SCTRL0	0x85	       /* W   Control 0			     */
#define     RSSQC	    0x20       /*     Reset sequence counter	     */
#define     RSFIFO	    0x10       /*     Flush FIFO		     */
#define     CMDAB	    0x04       /*     Abort command (sequence)	     */
#define     RSMOD	    0x02       /*     Reset SCSI Chip		     */
#define     RSCSI	    0x01       /*     Reset SCSI Bus		     */
#define TUL_STAT1	0x86	       /* R   Status 1			     */
#define     STRCV	    0x80       /*     Status received		     */
#define     MSGST	    0x40       /*     Message sent		     */
#define     CPDNE	    0x20       /*     Data phase done		     */
#define     DPHDN	    0x10       /*     Data phase done		     */
#define     STSNT	    0x08       /*     Status sent		     */
#define     SXCMP	    0x04       /*     Xfer completed		     */
#define     SLCMP	    0x02       /*     Selection completed	     */
#define     ARBCMP	    0x01       /*     Arbitration completed	     */
#define TUL_SCTRL1	0x86	       /* W   Control 1			     */
#define     ENSCAM	    0x80       /*     Enable SCAM		     */
#define     NIDARB	    0x40       /*     No ID for Arbitration	     */
#define     ENLRS	    0x20       /*     Low Level Reselect	     */
#define     PWDN	    0x10       /*     Power down mode		     */
#define     WCPU	    0x08       /*     Wide CPU			     */
#define     EHRSL	    0x04       /*     Enable HW reselect	     */
#define     ESBUSOUT	    0x02       /*     Enable SCSI data bus out latch */
#define     ESBUSIN	    0x01       /*     Enable SCSI data bus in latch  */
#define TUL_SSTATUS2	0x87	       /* R   Status 2			     */
#define     SABRT	    0x80       /*     Command aborted		     */
#define     OSCZ	    0x40       /*     Offset counter zero	     */
#define     SFFUL	    0x20       /*     FIFO full			     */
#define     TMCZ	    0x10       /*     Timeout counter zero	     */
#define     BSYGN	    0x08       /*     Busy release		     */
#define     PHMIS	    0x04       /*     Phase mismatch		     */
#define     SBEN	    0x02       /*     SCSI data bus enable	     */
#define     SRST	    0x01       /*     SCSI bus reset in progress     */
#define TUL_SCONFIG0	0x87	       /* W   Configuration		     */
#define     PHLAT	    0x80       /*     Enable phase latch	     */
#define     ITMOD	    0x40       /*     Initiator mode		     */
#define     SPCHK	    0x20       /*     Enable SCSI parity	     */
#define     ADMA8	    0x10       /*     Alternate dma 8-bits mode	     */
#define     ADMAW	    0x08       /*     Alternate dma 16-bits mode     */
#define     EDACK	    0x04       /*     Enable DACK in wide SCSI xfer  */
#define     ALTPD	    0x02       /*     Alternate sync period mode     */
#define     DSRST	    0x01       /*     Disable SCSI Reset signal	     */
#define     SCONFIG0DEFAULT (PHLAT | ITMOD | ALTPD | DSRST)
#define TUL_SOFSC	0x88	       /* R   Offset			     */
#define     PERIOD_WIDE_SCSI	0x80   /*     Enable Wide SCSI               */
#define     PERIOD_SYXPD	0x70   /*     Synch. SCSI Xfer rate          */
#define     PERIOD_SYOFS	0x0f   /*     Synch. SCSI Offset             */
#define TUL_SYNCM	0x88	       /* W   Sync. Xfer Period & Offset     */
#define TUL_SBID	0x89	       /* R   SCSI BUS ID		     */
#define TUL_SID		0x89	       /* W   SCSI ID			     */
#define TUL_SALVC	0x8A	       /* R   FIFO Avail Cnt/Identify Msg    */
#define     IHA_MSG_IDENTIFY_LUNMASK 0x07
#define TUL_STIMO	0x8A	       /* W   Sel/Resel Time Out Register    */
#define     STIMO_250MS	153	       /*     in units of 1.6385us           */
#define TUL_SDATI	0x8B	       /* R   SCSI Bus contents		     */
#define TUL_SDAT0	0x8B	       /* W   SCSI Data Out		     */
#define TUL_SFIFO	0x8C	       /* R/W FIFO			     */
#define TUL_SSIGI	0x90	       /* R   SCSI signal in		     */
#define     REQ		    0x80       /*     REQ signal		     */
#define     ACK		    0x40       /*     ACK signal		     */
#define     BSY		    0x20       /*     BSY signal		     */
#define     SEL		    0x10       /*     SEL signal		     */
#define     ATN		    0x08       /*     ATN signal		     */
#define     MSG		    0x04       /*     MSG signal		     */
#define     CD		    0x02       /*     C/D signal		     */
#define     IO		    0x01       /*     I/O signal		     */
#define TUL_SSIGO	0x90	       /* W   SCSI signal out		     */
#define TUL_SCMD	0x91	       /* R/W SCSI Command		     */
#define     NO_OP	    0x00       /*     Place Holder for tulip_wait()  */
#define     SEL_NOATN	    0x01       /*     Select w/o ATN Sequence	     */
#define     XF_FIFO_OUT	    0x03       /*     FIFO Xfer Information out	     */
#define     MSG_ACCEPT	    0x0F       /*     Message Accept		     */
#define     SEL_ATN	    0x11       /*     Select w ATN Sequence	     */
#define     SEL_ATNSTOP	    0x12       /*     Select w ATN & Stop Sequence   */
#define     SELATNSTOP	    0x1E       /*     Select w ATN & Stop Sequence   */
#define     SEL_ATN3	    0x31       /*     Select w ATN3 Sequence	     */
#define     XF_DMA_OUT	    0x43       /*     DMA Xfer Information out	     */
#define     EN_RESEL	    0x80       /*     Enable Reselection	     */
#define     XF_FIFO_IN	    0x83       /*     FIFO Xfer Information in	     */
#define     CMD_COMP	    0x84       /*     Command Complete Sequence	     */
#define     XF_DMA_IN	    0xC3       /*     DMA Xfer Information in	     */
#define TUL_STEST0	0x92	       /* R/W Test0			     */
#define TUL_STEST1	0x93	       /* R/W Test1			     */

/*
 *   Tulip (aka inic-940/950) DMA Registers
 */
#define TUL_DXPA	0xC0	       /* R/W DMA      Xfer Physcl Addr	 0-31*/
#define TUL_DXPAE	0xC4	       /* R/W DMA      Xfer Physcl Addr 32-63*/
#define TUL_DCXA	0xC8	       /* R   DMA Curr Xfer Physcl Addr	 0-31*/
#define TUL_DCXAE	0xCC	       /* R   DMA Curr Xfer Physcl Addr 32-63*/
#define TUL_DXC		0xD0	       /* R/W DMA Xfer Counter		     */
#define TUL_DCXC	0xD4	       /* R   DMA Current Xfer Counter	     */
#define TUL_DCMD	0xD8	       /* R/W DMA Command Register	     */
#define     SGXFR	    0x80       /*     Scatter/Gather Xfer	     */
#define     RSVD	    0x40       /*     Reserved - always reads as 0   */
#define     XDIR	    0x20       /*     Xfer Direction 0/1 = out/in    */
#define     BMTST	    0x10       /*     Bus Master Test		     */
#define     CLFIFO	    0x08       /*     Clear FIFO		     */
#define     ABTXFR	    0x04       /*     Abort Xfer		     */
#define     FRXFR	    0x02       /*     Force Xfer		     */
#define     STRXFR	    0x01       /*     Start Xfer		     */
#define TUL_ISTUS0	0xDC	       /* R/W Interrupt Status Register	     */
#define     DGINT	    0x80       /*     DMA Global Interrupt	     */
#define     RSVRD0	    0x40       /*     Reserved			     */
#define     RSVRD1	    0x20       /*     Reserved			     */
#define     SCMP	    0x10       /*     SCSI Complete		     */
#define     PXERR	    0x08       /*     PCI Xfer Error		     */
#define     DABT	    0x04       /*     DMA Xfer Aborted		     */
#define     FXCMP	    0x02       /*     Forced Xfer Complete	     */
#define     XCMP	    0x01       /*     Bus Master Xfer Complete	     */
#define TUL_ISTUS1	0xDD	       /* R   DMA status Register	     */
#define     SCBSY	    0x08       /*     SCSI Busy			     */
#define     FFULL	    0x04       /*     FIFO Full			     */
#define     FEMPT	    0x02       /*     FIFO Empty		     */
#define     XPEND	    0x01       /*     Xfer pending		     */
#define TUL_IMSK	0xE0	       /* R/W Interrupt Mask Register	     */
#define     MSCMP	    0x10       /*     Mask SCSI Complete	     */
#define     MPXFER	    0x08       /*     Mask PCI Xfer Error	     */
#define     MDABT	    0x04       /*     Mask Bus Master Abort	     */
#define     MFCMP	    0x02       /*     Mask Force Xfer Complete	     */
#define     MXCMP	    0x01       /*     Mask Bus Master Xfer Complete  */
#define     MASK_ALL	    (MXCMP | MFCMP | MDABT | MPXFER | MSCMP)
#define TUL_DCTRL0	0xE4	       /* R/W DMA Control Register	     */
#define     SXSTP	    0x80       /*     SCSI Xfer Stop		     */
#define     RPMOD	    0x40       /*     Reset PCI Module		     */
#define     RSVRD2	    0x20       /*     SCSI Xfer Stop		     */
#define     PWDWN	    0x10       /*     Power Down		     */
#define     ENTM	    0x08       /*     Enable SCSI Terminator Low     */
#define     ENTMW	    0x04       /*     Enable SCSI Terminator High    */
#define     DISAFC	    0x02       /*     Disable Auto Clear	     */
#define     LEDCTL	    0x01       /*     LED Control		     */
#define TUL_DCTRL1	0xE5	       /* R/W DMA Control Register 1	     */
#define     SDWS	    0x01       /*     SCSI DMA Wait State	     */
#define TUL_DFIFO	0xE8	       /* R/W DMA FIFO			     */

#define TUL_WCTRL	0xF7	       /* ?/? Bus master wait state control  */
#define TUL_DCTRL	0xFB	       /* ?/? DMA delay control		     */

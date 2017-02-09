/*	$NetBSD: dm9000reg.h,v 1.2 2012/01/28 08:29:55 nisimura Exp $	*/

/*
 * Copyright (c) 2009 Paul Fleischer
 * All rights reserved.
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Registers accesible on the DM9000, extracted from pp. 11-12 from the data sheet */

/* There are two interesting addresses for the DM9000
 * (at least in the context of the FriendlyARM MINI2440)
 * The I/O or register select address, which is the base address.
 * The DATA address, which is located at offset 4 from the base address.
 *
 * Chances are that this will not work generally, as it really depends on how the address lines are
 * mapped from the CPU to the DM9000. But for now it is a good starting point.
 */
#define DM9000_IOSIZE	4	/* XXX: Depends on the wiring of the address lines. */

#define DM9000_NCR		0x00
#define  DM9000_NCR_RST		(1 << 0)
#define  DM9000_NCR_LBK_MASK	(0x06)
#define  DM9000_NCR_LBK_SHIFT	(1)
#define  DM9000_NCR_LBK_MAC_INTERNAL ( 1 << DM9000_NCR_LBK_SHIFT )
#define  DM9000_NCR_LBK_NORMAL	( 0 << DM9000_NCR_LBK_SHIFT)
#define  DM9000_NCR_LBK_INT_PHY ( 2 << DM9000_NCR_LBK_SHIFT)
#define  DM9000_NCR_FDX		(1 << 3)
#define  DM9000_NCR_FCOL	(1 << 4)
#define  DM9000_NCR_WAKEEN	(1 << 6)
#define  DM9000_NCR_EXY_PHY	(1 << 7)
#define DM9000_NSR		0x01
#define  DM9000_NSR_RXOV	(1 << 1)
#define  DM9000_NSR_TX1END	(1 << 2)
#define  DM9000_NSR_TX2END	(1 << 3)
#define  DM9000_NSR_WAKEST	(1 << 5)
#define  DM9000_NSR_LINKST	(1 << 6)
#define  DM9000_NSR_SPEED	(1 << 7)
#define DM9000_TCR		0x02
#define  DM9000_TCR_TXREQ	(1 << 0)
#define  DM9000_TCR_CRC_DIS1	(1 << 1)
#define  DM9000_TCR_PAD_DIS1	(1 << 2)
#define  DM9000_TCR_CRC_DIS2	(1 << 3)
#define  DM9000_TCR_PAD_DIS2	(1 << 4)
#define  DM9000_TCR_EXCECM	(1 << 5)
#define  DM9000_TCR_TJDIS	(1 << 6)
#define DM9000_TSR1		0x03
#define DM9000_TSR2		0x04
#define DM9000_RCR		0x05
#define  DM9000_RCR_RXEN	(1 << 0)
#define  DM9000_RCR_PRMSC	(1 << 1)
#define  DM9000_RCR_RUNT	(1 << 2)
#define  DM9000_RCR_ALL		(1 << 3)
#define  DM9000_RCR_DIS_CRC	(1 << 4)
#define  DM9000_RCR_DIS_LONG	(1 << 5)
#define  DM9000_RCR_WTDIS	(1 << 6)
#define DM9000_RSR		0x06
#define  DM9000_RSR_FOE		(1<<0)
#define  DM9000_RSR_CE		(1<<1)
#define  DM9000_RSR_AE		(1<<2)
#define  DM9000_RSR_PLE		(1<<3)
#define  DM9000_RSR_RWTO	(1<<4)
#define  DM9000_RSR_LCS		(1<<5)
#define  DM9000_RSR_MF		(1<<6)
#define  DM9000_RSR_RF		(1<<7)
#define DM9000_ROCR		0x07
#define DM9000_BPTR		0x08
#define DM9000_FCTR		0x09
#define DM9000_FCR		0x0A
#define  DM9000_FCR_FLCE	(1 << 0)
#define  DM9000_FCR_RXPCS	(1 << 1)
#define  DM9000_FCR_RXPS	(1 << 2)
#define  DM9000_FCR_BKPM	(1 << 3)
#define  DM9000_FCR_BKPA	(1 << 4)
#define  DM9000_FCR_TXPEN	(1 << 5)
#define  DM9000_FCR_TXPF	(1 << 6)
#define  DM9000_FCR_TXP0	(1 << 7)
#define DM9000_EPCR		0x0B
#define  DM9000_EPCR_ERRE	(1 << 0)
#define  DM9000_EPCR_ERPRW	(1 << 1)
#define  DM9000_EPCR_ERPRR	(1 << 2)
#define  DM9000_EPCR_EPOS_EEPROM (0 << 3)
#define  DM9000_EPCR_EPOS_PHY    (1 << 3)
#define  DM9000_EPCR_WEP	(1 << 4)
#define  DM9000_EPCR_REEP	(1 << 5)
#define DM9000_EPAR		0x0C
#define  DM9000_EPAR_EROA_MASK	0x3F /* bits 0-5 */
#define  DM9000_EPAR_INT_PHY	0x40 /* EPAR[7:6] = 01 for internal PHY */
#define DM9000_EPDRL		0x0D
#define DM9000_EPDRH		0x0E
#define DM9000_WCR		0x0F

#define DM9000_PAB0		0x10
#define DM9000_PAB1		0x11
#define DM9000_PAB2		0x12
#define DM9000_PAB3		0x13
#define DM9000_PAB4		0x14
#define DM9000_PAB5		0x15

#define DM9000_MAB0		0x16
#define DM9000_MAB1		0x17
#define DM9000_MAB2		0x18
#define DM9000_MAB3		0x19
#define DM9000_MAB4		0x1A
#define DM9000_MAB5		0x1B
#define DM9000_MAB6		0x1C
#define DM9000_MAB7		0x1D

#define DM9000_GPCR		0x1E
#define  DM9000_GPCR_GPIO0_OUT	(1 << 0)
#define DM9000_GPR		0x1F
#define  DM9000_GPR_PHY_PWROFF	(1 << 0)
#define DM9000_TRPAL		0x22
#define DM9000_TRPAH		0x23
#define DM9000_RWPAL	0x24
#define DM9000_RWPAH	0x25

#define DM9000_VID0	0x28
#define DM9000_VID1	0x29

#define DM9000_PID0	0x2A
#define DM9000_PID1	0x2B

#define DM9000_CHIPR	0x2C
#define DM9000_SMCR	0x2F
#define DM9000_MRCMDX	0xF0
#define DM9000_MRCMD	0xF2
#define DM9000_MRRL	0xF4
#define DM9000_MRRH	0xF5
#define DM9000_MWCMDX	0xF6
#define DM9000_MWCMD	0xF8
#define DM9000_MWRL	0xFA
#define DM9000_MWRH	0xFB
#define DM9000_TXPLL	0xFC
#define DM9000_TXPLH	0xFD
#define DM9000_ISR	0xFE
#define  DM9000_IOMODE_MASK	0xC0
#define  DM9000_IOMODE_SHIFT	6
#define  DM9000_ISR_PRS		(1<<0)
#define  DM9000_ISR_PTS		(1<<1)
#define  DM9000_ISR_ROS		(1<<2)
#define  DM9000_ISR_ROOS	(1<<3)
#define  DM9000_ISR_UNDERRUN	(1<<4)
#define  DM9000_ISR_LNKCHNG	(1<<5)
#define DM9000_IMR	0xFF
#define  DM9000_IMR_PRM 	(1<<0)
#define  DM9000_IMR_PTM 	(1<<1)
#define  DM9000_IMR_ROM 	(1<<2)
#define  DM9000_IMR_ROOM	(1<<3)
#define  DM9000_IMR_PAR 	(1<<7)

#define DM9000_PHY_BMCR 	0x00
#define  DM9000_PHY_BMCR_COLL_TEST	(1<<7)
#define  DM9000_PHY_BMCR_DUPLEX_MODE	(1<<8)
#define  DM9000_PHY_BMCR_RESTART_AN	(1<<9)
#define  DM9000_PHY_BMCR_ISOLATE	(1<<10)
#define  DM9000_PHY_BMCR_POWER_DOWN	(1<<11)
#define  DM9000_PHY_BMCR_AUTO_NEG_EN	(1<<12)
#define  DM9000_PHY_BMCR_SPEED_SELECT	(1<<13)
#define  DM9000_PHY_BMCR_LOOPBACK	(1<<14)
#define  DM9000_PHY_BMCR_RESET  	(1<<15)
#define DM9000_PHY_BMSR 	0x01
#define  DM9000_PHY_BMSR_EXT_CAP	(1<<0)
#define  DM9000_PHY_BMSR_JAB_DET	(1<<1)
#define  DM9000_PHY_BMSR_LINK_ST	(1<<2)
#define  DM9000_PHY_BMSR_AUTO_NEG_AB	(1<<3)
#define  DM9000_PHY_BMSR_REMOTE_FAULT	(1<<4)
#define  DM9000_PHY_BMSR_AUTO_NEG_COM	(1<<5)
#define  DM9000_PHY_BMSR_MF_PRE_SUP	(1<<6)
#define  DM9000_PHY_BMSR_10BASE_HALF_DUPLEX (1<<11)
#define  DM9000_PHY_BMSR_10BASE_FULL_DUPLEX (1<<12)
#define  DM9000_PHY_BMSR_100BASE_HALF_DUPLEX (1<<13)
#define  DM9000_PHY_BMSR_100BASE_FULL_DUPLEX (1<<14)
#define  DM9000_PHY_BMSR_100BASE_T4	(1<<15)
#define DM9000_PHY_PHYID1	0x02
#define DM9000_PHY_PHYID2 	0x03
#define DM9000_PHY_ANAR 	0x04
#define  DM9000_PHY_ANAR_10_HDX	 (1<<5)
#define  DM9000_PHY_ANAR_10_FDX  (1<<6)
#define  DM9000_PHY_ANAR_TX_HDX  (1<<7)
#define  DM9000_PHY_ANAR_TX_FDX  (1<<8)
#define DM9000_PHY_ANLPAR	0x05
#define DM9000_PHY_ANER 	0x06
#define DM9000_PHY_DSCR 	0x16
#define DM9000_PHY_DSCSR 	0x17
#define DM9000_PHY_10BTCSR	0x18

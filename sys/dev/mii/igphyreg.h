/*	$NetBSD: igphyreg.h,v 1.6 2010/03/07 09:05:19 msaitoh Exp $	*/

/*******************************************************************************

  Copyright (c) 2001-2003, Intel Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/*
 * Copied from the Intel code, and then modified to match NetBSD
 * style for MII registers more.
 */

/*
 * IGP01E1000 Specific Registers
 */

/* IGP01E1000 Specific Port Control Register - R/W */
#define MII_IGPPHY_PORT_CONFIG		0x10 /* PHY specific config register */
#define PSCR_AUTO_MDIX_PAR_DETECT	0x0010
#define PSCR_PRE_EN			0x0020
#define PSCR_SMART_SPEED		0x0080
#define PSCR_DISABLE_TPLOOPBACK		0x0100
#define PSCR_DISABLE_JABBER		0x0400
#define PSCR_DISABLE_TRANSMIT		0x2000

/* IGP01E1000 Specific Port Status Register - R/O */
#define MII_IGPHY_PORT_STATUS		0x11
#define PSSR_AUTONEG_FAILED		0x0001 /* RO LH SC */
#define PSSR_POLARITY_REVERSED		0x0002
#define PSSR_CABLE_LENGTH		0x007C
#define PSSR_FULL_DUPLEX		0x0200
#define PSSR_LINK_UP			0x0400
#define PSSR_MDIX			0x0800
#define PSSR_SPEED_MASK			0xC000 /* speed bits mask */
#define PSSR_SPEED_10MBPS		0x4000
#define PSSR_SPEED_100MBPS		0x8000
#define PSSR_SPEED_1000MBPS		0xC000
#define PSSR_CABLE_LENGTH_SHIFT 	0x0002 /* shift right 2 */
#define PSSR_MDIX_SHIFT			0x000B /* shift right 11 */

/* IGP01E1000 Specific Port Control Register - R/W */
#define MII_IGPHY_PORT_CTRL		0x12
#define PSCR_TP_LOOPBACK		0x0001
#define PSCR_CORRECT_NC_SCMBLR		0x0200
#define PSCR_TEN_CRS_SELECT		0x0400
#define PSCR_FLIP_CHIP			0x0800
#define PSCR_AUTO_MDIX			0x1000
#define PSCR_FORCE_MDI_MDIX 		0x2000 /* 0-MDI, 1-MDIX */

/* IGP01E1000 Specific Port Link Health Register */
#define MII_IGPHY_LINK_HEALTH		0x13
#define PLHR_SS_DOWNGRADE		0x8000
#define PLHR_GIG_SCRAMBLER_ERROR	0x4000
#define PLHR_GIG_REM_RCVR_NOK		0x0800 /* LH */
#define PLHR_IDLE_ERROR_CNT_OFLOW	0x0400 /* LH */
#define PLHR_DATA_ERR_1			0x0200 /* LH */
#define PLHR_DATA_ERR_0			0x0100
#define PLHR_AUTONEG_FAULT		0x0010
#define PLHR_AUTONEG_ACTIVE		0x0008
#define PLHR_VALID_CHANNEL_D		0x0004
#define PLHR_VALID_CHANNEL_C		0x0002
#define PLHR_VALID_CHANNEL_B		0x0001
#define PLHR_VALID_CHANNEL_A		0x0000

/* IGP01E1000 GMII FIFO Register */
#define MII_IGGMII_FIFO			0x14
#define GMII_FLEX_SPD			0x10 /* Enable flexible speed */
#define GMII_SPD			0x20 /* Enable SPD */

/* IGP01E1000 Channel Quality Register */
#define MII_IGPHY_CHANNEL_QUALITY	0x15
#define MSE_CHANNEL_D			0x000F
#define MSE_CHANNEL_C			0x00F0
#define MSE_CHANNEL_B			0x0F00
#define MSE_CHANNEL_A			0xF000

#define MII_IGPHY_PAGE_SELECT		0x1F
#define IGPHY_MAXREGADDR		0x1F
#define IGPHY_PAGEMASK			(~IGPHY_MAXREGADDR)

/* IGP01E1000 AGC Registers - stores the cable length values*/
#define MII_IGPHY_AGC_A			0x1172
#define MII_IGPHY_AGC_PARAM_A		0x1171
#define MII_IGPHY_AGC_B			0x1272
#define MII_IGPHY_AGC_PARAM_B		0x1271
#define MII_IGPHY_AGC_C			0x1472
#define MII_IGPHY_AGC_PARAM_C		0x1471
#define MII_IGPHY_AGC_D			0x1872
#define MII_IGPHY_AGC_PARAM_D		0x1871
#define AGC_LENGTH_SHIFT		7  /* Coarse - 13:11, Fine - 10:7 */
#define AGC_LENGTH_TABLE_SIZE		128
#define AGC_RANGE			10

/* IGP01E1000 DSP Reset Register */
#define MII_IGPHY_DSP_RESET		0x1F33
#define MII_IGPHY_DSP_SET		0x1F71
#define MII_IGPHY_DSP_FFE		0x1F35
#define MII_IGPHY_CHANNEL_NUM		4
#define MII_IGPHY_EDAC_MU_INDEX		0xC000
#define MII_IGPHY_EDAC_SIGN_EXT_9_BITS	0x8000
#define MII_IGPHY_ANALOG_TX_STATE	0x2890
#define MII_IGPHY_ANALOG_CLASS_A	0x2000
#define MII_IGPHY_FORCE_ANALOG_ENABLE	0x0004
#define MII_IGPHY_DSP_FFE_CM_CP		0x0069
#define MII_IGPHY_DSP_FFE_DEFAULT	0x002A

/* IGP01E1000 PCS Initialization register - stores the polarity status */
#define MII_IGPHY_PCS_INIT_REG		0x00B4
#define MII_IGPHY_PCS_CTRL_REG		0x00B5

#define MII_IGPHY_ANALOG_REGS_PAGE	0x20C0
#define PHY_POLARITY_MASK		0x0078

/* IGP01E1000 Analog Register */
#define MII_IGPHY_ANALOG_SPARE_FUSE_STATUS	0x20D1
#define MII_IGPHY_ANALOG_FUSE_STATUS		0x20D0
#define MII_IGPHY_ANALOG_FUSE_CONTROL		0x20DC
#define MII_IGPHY_ANALOG_FUSE_BYPASS		0x20DE
#define ANALOG_FUSE_POLY_MASK		0xF000
#define ANALOG_FUSE_FINE_MASK		0x0F80
#define ANALOG_FUSE_COARSE_MASK		0x0070
#define ANALOG_SPARE_FUSE_ENABLED	0x0100
#define ANALOG_FUSE_ENABLE_SW_CONTROL	0x0002
#define ANALOG_FUSE_COARSE_THRESH	0x0040
#define ANALOG_FUSE_COARSE_10		0x0010
#define ANALOG_FUSE_FINE_1		0x0080
#define ANALOG_FUSE_FINE_10		0x0500

/*
 * IGP3 regs
 */
#define IGP3_PAGE_SHIFT		5
#define IGP3_MAX_REG_ADDRESS	0x1f  /* 5 bit address bus (0-0x1f) */
#define IGP3_REG(page, reg) \
	(((page) << IGP3_PAGE_SHIFT) | ((reg) & IGP3_MAX_REG_ADDRESS))

#define IGP3_VR_CTRL	IGP3_REG(776, 18)
#define IGP3_VR_CTRL_DEV_POWERDOWN_MODE_MASK	0x0300
#define IGP3_VR_CTRL_MODE_SHUTDOWN		0x0200

#define IGP3_PM_CTRL	IGP3_REG(769, 20)
#define IGP3_PM_CTRL_FORCE_PWR_DOWN		0x0020


#define IGPHY_READ(sc, reg) \
    (PHY_WRITE(sc, MII_IGPHY_PAGE_SELECT, (reg) & ~0x1f), \
     PHY_READ(sc, (reg) & 0x1f))

#define IGPHY_WRITE(sc, reg, val) \
    do { \
	PHY_WRITE(sc, MII_IGPHY_PAGE_SELECT, (reg) & ~0x1f); \
	PHY_WRITE(sc, (reg) & 0x1f, val); \
    } while (/*CONSTCOND*/0)

#define	IGPHY_TICK_DOWNSHIFT	3
#define	IGPHY_TICK_MAX		15

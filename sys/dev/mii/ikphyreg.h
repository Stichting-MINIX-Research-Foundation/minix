/*	$NetBSD: ikphyreg.h,v 1.2 2010/11/29 23:04:42 jym Exp $	*/
/*******************************************************************************
Copyright (c) 2001-2005, Intel Corporation 
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

/* Bits...
 * 15-5: page
 * 4-0: register offset
 */
#define GG82563_PAGE_SHIFT        5
#define GG82563_REG(page, reg)    \
        (((page) << GG82563_PAGE_SHIFT) | ((reg) & GG82563_MAX_REG_ADDRESS))
#define GG82563_MIN_ALT_REG       30

#define GG82563_MAX_REG_ADDRESS        0x1F  /* 5 bit address bus (0-0x1F) */
#define GG82563_MAX_MULTI_PAGE_REG     0xF   /* Registers equal on all pages */


/* GG82563 Specific Registers */
#define GG82563_PHY_SPEC_CTRL	GG82563_REG(0, 16) /* PHY Specific Control */
#define GG82563_PSCR_DISABLE_JABBER             0x0001 /* 1=Disable Jabber */
#define GG82563_PSCR_POLARITY_REVERSAL_DISABLE  0x0002 /* 1=Polarity Reversal Disabled */
#define GG82563_PSCR_POWER_DOWN                 0x0004 /* 1=Power Down */
#define GG82563_PSCR_COPPER_TRANSMITER_DISABLE  0x0008 /* 1=Transmitter Disabled */
#define GG82563_PSCR_CROSSOVER_MODE_MASK        0x0060
#define GG82563_PSCR_CROSSOVER_MODE_MDI         0x0000 /* 00=Manual MDI configuration */
#define GG82563_PSCR_CROSSOVER_MODE_MDIX        0x0020 /* 01=Manual MDIX configuration */
#define GG82563_PSCR_CROSSOVER_MODE_AUTO        0x0060 /* 11=Automatic crossover */
#define GG82563_PSCR_ENALBE_EXTENDED_DISTANCE   0x0080 /* 1=Enable Extended Distance */
#define GG82563_PSCR_ENERGY_DETECT_MASK         0x0300
#define GG82563_PSCR_ENERGY_DETECT_OFF          0x0000 /* 00,01=Off */
#define GG82563_PSCR_ENERGY_DETECT_RX           0x0200 /* 10=Sense on Rx only (Energy Detect) */
#define GG82563_PSCR_ENERGY_DETECT_RX_TM        0x0300 /* 11=Sense and Tx NLP */
#define GG82563_PSCR_FORCE_LINK_GOOD            0x0400 /* 1=Force Link Good */
#define GG82563_PSCR_DOWNSHIFT_ENABLE           0x0800 /* 1=Enable Downshift */
#define GG82563_PSCR_DOWNSHIFT_COUNTER_MASK     0x7000
#define GG82563_PSCR_DOWNSHIFT_COUNTER_SHIFT    12

#define GG82563_PHY_SPEC_STATUS	GG82563_REG(0, 17) /* PHY Specific Status */
#define GG82563_PSSR_JABBER                0x0001 /* 1=Jabber */
#define GG82563_PSSR_POLARITY              0x0002 /* 1=Polarity Reversed */
#define GG82563_PSSR_LINK                  0x0008 /* 1=Link is Up */
#define GG82563_PSSR_ENERGY_DETECT         0x0010 /* 1=Sleep, 0=Active */
#define GG82563_PSSR_DOWNSHIFT             0x0020 /* 1=Downshift */
#define GG82563_PSSR_CROSSOVER_STATUS      0x0040 /* 1=MDIX, 0=MDI */
#define GG82563_PSSR_RX_PAUSE_ENABLED      0x0100 /* 1=Receive Pause Enabled */
#define GG82563_PSSR_TX_PAUSE_ENABLED      0x0200 /* 1=Transmit Pause Enabled */
#define GG82563_PSSR_LINK_UP               0x0400 /* 1=Link Up */
#define GG82563_PSSR_SPEED_DUPLEX_RESOLVED 0x0800 /* 1=Resolved */
#define GG82563_PSSR_PAGE_RECEIVED         0x1000 /* 1=Page Received */
#define GG82563_PSSR_DUPLEX                0x2000 /* 1-Full-Duplex */
#define GG82563_PSSR_SPEED_MASK            0xC000
#define GG82563_PSSR_SPEED_10MBPS          0x0000 /* 00=10Mbps */
#define GG82563_PSSR_SPEED_100MBPS         0x4000 /* 01=100Mbps */
#define GG82563_PSSR_SPEED_1000MBPS        0x8000 /* 10=1000Mbps */

#define GG82563_PHY_INT_ENABLE          \
        GG82563_REG(0, 18) /* Interrupt Enable */

#define GG82563_PHY_SPEC_STATUS_2 GG82563_REG(0, 19) /* PHY Specific Status 2 */
#define GG82563_PSSR2_JABBER                0x0001 /* 1=Jabber */
#define GG82563_PSSR2_POLARITY_CHANGED      0x0002 /* 1=Polarity Changed */
#define GG82563_PSSR2_ENERGY_DETECT_CHANGED 0x0010 /* 1=Energy Detect Changed */
#define GG82563_PSSR2_DOWNSHIFT_INTERRUPT   0x0020 /* 1=Downshift Detected */
#define GG82563_PSSR2_MDI_CROSSOVER_CHANGE  0x0040 /* 1=Crossover Changed */
#define GG82563_PSSR2_FALSE_CARRIER         0x0100 /* 1=False Carrier */
#define GG82563_PSSR2_SYMBOL_ERROR          0x0200 /* 1=Symbol Error */
#define GG82563_PSSR2_LINK_STATUS_CHANGED   0x0400 /* 1=Link Status Changed */
#define GG82563_PSSR2_AUTO_NEG_COMPLETED    0x0800 /* 1=Auto-Neg Completed */
#define GG82563_PSSR2_PAGE_RECEIVED         0x1000 /* 1=Page Received */
#define GG82563_PSSR2_DUPLEX_CHANGED        0x2000 /* 1=Duplex Changed */
#define GG82563_PSSR2_SPEED_CHANGED         0x4000 /* 1=Speed Changed */
#define GG82563_PSSR2_AUTO_NEG_ERROR        0x8000 /* 1=Auto-Neg Error */

#define GG82563_PHY_RX_ERR_CNTR	GG82563_REG(0, 21) /* Receive Error Counter */

#define GG82563_PHY_PAGE_SELECT	GG82563_REG(0, 22) /* Page Select */

#define GG82563_PHY_SPEC_CTRL_2	GG82563_REG(0, 26) /* PHY Specific Control 2 */
#define GG82563_PSCR2_10BT_POLARITY_FORCE           0x0002 /* 1=Force Negative Polarity */
#define GG82563_PSCR2_1000MB_TEST_SELECT_MASK       0x000C
#define GG82563_PSCR2_1000MB_TEST_SELECT_NORMAL     0x0000 /* 00,01=Normal Operation */
#define GG82563_PSCR2_1000MB_TEST_SELECT_112NS      0x0008 /* 10=Select 112ns Sequence */
#define GG82563_PSCR2_1000MB_TEST_SELECT_16NS       0x000C /* 11=Select 16ns Sequence */
#define GG82563_PSCR2_REVERSE_AUTO_NEG              0x2000 /* 1=Reverse Auto-Negotiation */
#define GG82563_PSCR2_1000BT_DISABLE                0x4000 /* 1=Disable 1000BASE-T */
#define GG82563_PSCR2_TRANSMITER_TYPE_MASK          0x8000
#define GG82563_PSCR2_TRANSMITTER_TYPE_CLASS_B      0x0000 /* 0=Class B */
#define GG82563_PSCR2_TRANSMITTER_TYPE_CLASS_A      0x8000 /* 1=Class A */

#define GG82563_PHY_PAGE_SELECT_ALT GG82563_REG(0, 29) /* Alternate Page Select */
#define GG82563_PHY_TEST_CLK_CTRL GG82563_REG(0, 30) /* Test Clock Control (use reg. 29 to select) */

#define GG82563_PHY_MAC_SPEC_CTRL GG82563_REG(2, 21) /* MAC Specific Control Register */
/* Tx clock speed for Link Down and 1000BASE-T for the following speeds */
#define GG82563_MSCR_TX_CLK_MASK                    0x0007
#define GG82563_MSCR_TX_CLK_10MBPS_2_5MHZ           0x0004
#define GG82563_MSCR_TX_CLK_100MBPS_25MHZ           0x0005
#define GG82563_MSCR_TX_CLK_1000MBPS_2_5MHZ         0x0006
#define GG82563_MSCR_TX_CLK_1000MBPS_25MHZ          0x0007
#define GG82563_MSCR_ASSERT_CRS_ON_TX               0x0010 /* 1=Assert */

#define GG82563_PHY_MAC_SPEC_CTRL_2 GG82563_REG(2, 26) /* MAC Specific Control 2 */

#define GG82563_PHY_DSP_DISTANCE GG82563_REG(5, 26) /* DSP Distance */
#define GG82563_DSPD_CABLE_LENGTH               0x0007 /* 0 = <50M;
                                                          1 = 50-80M;
                                                          2 = 80-110M;
                                                          3 = 110-140M;
                                                          4 = >140M */

/* Page 193 - Port Control Registers */
#define GG82563_PHY_KMRN_MODE_CTRL GG82563_REG(193, 16) /* Kumeran Mode Control */
#define GG82563_KMCR_PHY_LEDS_EN                    0x0020 /* 1=PHY LEDs, 0=Kumeran Inband LEDs */
#define GG82563_KMCR_FORCE_LINK_UP                  0x0040 /* 1=Force Link Up */
#define GG82563_KMCR_SUPPRESS_SGMII_EPD_EXT         0x0080
#define GG82563_KMCR_MDIO_BUS_SPEED_SELECT_MASK     0x0400
#define GG82563_KMCR_MDIO_BUS_SPEED_SELECT          0x0400 /* 1=6.25MHz, 0=0.8MHz */
#define GG82563_KMCR_PASS_FALSE_CARRIER             0x0800

#define GG82563_PHY_PORT_RESET	GG82563_REG(193, 17) /* Port Reset */

#define GG82563_PHY_REVISION_ID	GG82563_REG(193, 18) /* Revision ID */

#define GG82563_PHY_DEVICE_ID	GG82563_REG(193, 19) /* Device ID */

#define GG82563_PHY_PWR_MGMT_CTRL GG82563_REG(193, 20) /* Power Management Control */
#define GG82563_PMCR_ENABLE_ELECTRICAL_IDLE         0x0001 /* 1=Enable SERDES Electrical Idle */
#define GG82563_PMCR_DISABLE_PORT                   0x0002 /* 1=Disable Port */
#define GG82563_PMCR_DISABLE_SERDES                 0x0004 /* 1=Disable SERDES */
#define GG82563_PMCR_REVERSE_AUTO_NEG               0x0008 /* 1=Enable Reverse Auto-Negotiation */
#define GG82563_PMCR_DISABLE_1000_NON_D0            0x0010 /* 1=Disable 1000Mbps Auto-Neg in non D0 */
#define GG82563_PMCR_DISABLE_1000                   0x0020 /* 1=Disable 1000Mbps Auto-Neg Always */
#define GG82563_PMCR_REVERSE_AUTO_NEG_D0A           0x0040 /* 1=Enable D0a Reverse Auto-Negotiation */
#define GG82563_PMCR_FORCE_POWER_STATE              0x0080 /* 1=Force Power State */
#define GG82563_PMCR_PROGRAMMED_POWER_STATE_MASK    0x0300
#define GG82563_PMCR_PROGRAMMED_POWER_STATE_DR      0x0000 /* 00=Dr */
#define GG82563_PMCR_PROGRAMMED_POWER_STATE_D0U     0x0100 /* 01=D0u */
#define GG82563_PMCR_PROGRAMMED_POWER_STATE_D0A     0x0200 /* 10=D0a */
#define GG82563_PMCR_PROGRAMMED_POWER_STATE_D3      0x0300 /* 11=D3 */


#define GG82563_PHY_RATE_ADAPT_CTRL GG82563_REG(193, 25) /* Rate Adaptation Control */

/* Page 194 - KMRN Registers */
#define GG82563_PHY_KMRN_FIFO_CTRL_STAT GG82563_REG(194, 16) /* FIFO's Control/Status */

#define GG82563_PHY_KMRN_CTRL	GG82563_REG(194, 17) /* Control */

#define GG82563_PHY_INBAND_CTRL	GG82563_REG(194, 18) /* Inband Control */
#define GG82563_ICR_DIS_PADDING                     0x0010 /* Disable Padding Use */

#define GG82563_PHY_KMRN_DIAGNOSTIC GG82563_REG(194, 19) /* Diagnostic */

#define GG82563_PHY_ACK_TIMEOUTS GG82563_REG(194, 20) /* Acknowledge Timeouts */

#define GG82563_PHY_ADV_ABILITY	GG82563_REG(194, 21) /* Advertised Ability */

#define GG82563_PHY_LINK_PARTNER_ADV_ABILITY GG82563_REG(194, 23) /* Link Partner Advertised Ability */

#define GG82563_PHY_ADV_NEXT_PAGE GG82563_REG(194, 24) /* Advertised Next Page */

#define GG82563_PHY_LINK_PARTNER_ADV_NEXT_PAGE GG82563_REG(194, 25) /* Link Partner Advertised Next page */

#define GG82563_PHY_KMRN_MISC	GG82563_REG(194, 26) /* Misc. */

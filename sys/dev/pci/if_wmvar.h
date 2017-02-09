/*	$NetBSD: if_wmvar.h,v 1.29 2015/06/06 04:39:12 msaitoh Exp $	*/

/*
 * Copyright (c) 2001, 2002, 2003, 2004 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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

#ifndef _DEV_PCI_IF_WMVAR_H_
#define _DEV_PCI_IF_WMVAR_H_

/* sc_flags */
#define	WM_F_HAS_MII		0x00000001 /* has MII */
#define	WM_F_LOCK_EECD		0x00000002 /* Lock using with EECD register */
#define	WM_F_LOCK_SWSM		0x00000004 /* Lock using with SWSM register */
#define WM_F_LOCK_SWFW		0x00000008 /* Lock using with SWFW register */
#define WM_F_LOCK_EXTCNF	0x00000010 /* Lock using with EXTCNF reg. */
#define	WM_F_EEPROM_EERDEEWR	0x00000020 /* EEPROM access via EERD/EEWR */
#define	WM_F_EEPROM_SPI		0x00000040 /* EEPROM is SPI */
#define	WM_F_EEPROM_FLASH	0x00000080 /* EEPROM is FLASH */
#define	WM_F_EEPROM_FLASH_HW	0x00000100 /* EEPROM is FLASH */
#define	WM_F_EEPROM_INVALID	0x00000200 /* EEPROM not present (bad cksum) */
#define	WM_F_IOH_VALID		0x00000400 /* I/O handle is valid */
#define	WM_F_BUS64		0x00000800 /* bus is 64-bit */
#define	WM_F_PCIX		0x00001000 /* bus is PCI-X */
#define	WM_F_CSA		0x00002000 /* bus is CSA */
#define	WM_F_PCIE		0x00004000 /* bus is PCI-Express */
#define WM_F_SGMII		0x00008000 /* use SGMII */
#define WM_F_NEWQUEUE		0x00010000 /* use new queue system */
#define WM_F_ASF_FIRMWARE_PRES	0x00020000
#define WM_F_ARC_SUBSYS_VALID	0x00040000
#define WM_F_HAS_AMT		0x00080000
#define WM_F_HAS_MANAGE		0x00100000
#define WM_F_WOL		0x00200000
#define WM_F_EEE		0x00400000 /* Energy Efficiency Ethernet */
#define WM_F_ATTACHED		0x00800000 /* attach() finished successfully */
#define	WM_F_EEPROM_INVM	0x01000000 /* NVM is iNVM */
#define	WM_F_PCS_DIS_AUTONEGO	0x02000000 /* PCS Disable Autonego */
#define	WM_F_PLL_WA_I210	0x04000000 /* I21[01] PLL workaround */

/*
 * Variations of Intel gigabit Ethernet controller:
 *
 *  +-- 82542
 *  |  +-- 82543 - 82544
 *  |  |  +-- 82540 - 82545 - 82546
 *  |  |  |  +-- 82541 - 82547
 *  |  |  |  |  +---------- 82571 - 82572 - 82573 - 82574 - 82583
 *  |  |  |  |  |  +--------- 82575 - 82576 - 82580 - I350 - I354 - I210 - I211
 *  |  |  |  |  |  |  +-- 80003
 *  |  |  |  |  |  |  |  +-- ICH8 - ICH9 - ICH10 - PCH - PCH2 - PCH_LPT
 *  |  |  |  |  |  |  |  |
 * -+--+--+--+--+--+--+--+----------------------------------------------->
 */

typedef enum {
	WM_T_unknown		= 0,
	WM_T_82542_2_0,			/* i82542 2.0 (really old) */
	WM_T_82542_2_1,			/* i82542 2.1+ (old) */
	WM_T_82543,			/* i82543 */
	WM_T_82544,			/* i82544 */
	WM_T_82540,			/* i82540 */
	WM_T_82545,			/* i82545 */
	WM_T_82545_3,			/* i82545 3.0+ */
	WM_T_82546,			/* i82546 */
	WM_T_82546_3,			/* i82546 3.0+ */
	WM_T_82541,			/* i82541 */
	WM_T_82541_2,			/* i82541 2.0+ */
	WM_T_82547,			/* i82547 */
	WM_T_82547_2,			/* i82547 2.0+ */
	WM_T_82571,			/* i82571 */
	WM_T_82572,			/* i82572 */
	WM_T_82573,			/* i82573 */
	WM_T_82574,			/* i82574 */
	WM_T_82583,			/* i82583 */
	WM_T_82575,			/* i82575 */
	WM_T_82576,			/* i82576 */
	WM_T_82580,			/* i82580 */
	WM_T_I350,			/* I350 */
	WM_T_I354,			/* I354 */
	WM_T_I210,			/* I210 */
	WM_T_I211,			/* I211 */
	WM_T_80003,			/* i80003 */
	WM_T_ICH8,			/* ICH8 LAN */
	WM_T_ICH9,			/* ICH9 LAN */
	WM_T_ICH10,			/* ICH10 LAN */
	WM_T_PCH,			/* PCH LAN */
	WM_T_PCH2,			/* PCH2 LAN */
	WM_T_PCH_LPT,			/* PCH LPT LAN (I21[78]) */
} wm_chip_type;

typedef enum {
	WMPHY_UNKNOWN = 0,
	WMPHY_NONE,
	WMPHY_M88,
	WMPHY_IGP,
	WMPHY_IGP_2,
	WMPHY_GG82563,
	WMPHY_IGP_3,
	WMPHY_IFE,
	WMPHY_BM,
	WMPHY_82577,
	WMPHY_82578,
	WMPHY_82579,
	WMPHY_82580
} wm_phy_type;


#define WM_GEN_POLL_TIMEOUT	640
#define WM_PHY_CFG_TIMEOUT	100
#define	WM_ICH8_LAN_INIT_TIMEOUT 1500
#define	WM_MDIO_OWNERSHIP_TIMEOUT 10
#define	WM_MAX_PLL_TRIES	5

#endif /* _DEV_PCI_IF_WMVAR_H_ */

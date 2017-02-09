/* $NetBSD: ibm82660reg.h,v 1.3 2008/06/14 12:01:28 mjf Exp $ */

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Tim Rightnour
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

#ifndef _DEV_IC_IBM82660REG_H_
#define _DEV_IC_IBM82660REG_H_

/* Register definitions for the IBM 82660 PCI Bridge Controller.
 * Also known as a Lanai/Kauai.
 */

/* Memmory Bank Starting Addresses */
#define	IBM_82660_MEM_BANK0_START	0x80
#define	IBM_82660_MEM_BANK1_START	0x81
#define	IBM_82660_MEM_BANK2_START	0x82
#define	IBM_82660_MEM_BANK3_START	0x83
#define	IBM_82660_MEM_BANK4_START	0x84
#define	IBM_82660_MEM_BANK5_START	0x85
#define	IBM_82660_MEM_BANK6_START	0x86
#define	IBM_82660_MEM_BANK7_START	0x87

/* Memory Bank Extended Starting Addresses */
#define	IBM_82660_MEM_BANK0_EXTSTART	0x88
#define	IBM_82660_MEM_BANK1_EXTSTART	0x89
#define	IBM_82660_MEM_BANK2_EXTSTART	0x8A
#define	IBM_82660_MEM_BANK3_EXTSTART	0x8B
#define	IBM_82660_MEM_BANK4_EXTSTART	0x8C
#define	IBM_82660_MEM_BANK5_EXTSTART	0x8D
#define	IBM_82660_MEM_BANK6_EXTSTART	0x8E
#define	IBM_82660_MEM_BANK7_EXTSTART	0x8F

/* Memory Bank Ending Addresses */
#define	IBM_82660_MEM_BANK0_END		0x90
#define	IBM_82660_MEM_BANK1_END		0x91
#define	IBM_82660_MEM_BANK2_END		0x92
#define	IBM_82660_MEM_BANK3_END		0x93
#define	IBM_82660_MEM_BANK4_END		0x94
#define	IBM_82660_MEM_BANK5_END		0x95
#define	IBM_82660_MEM_BANK6_END		0x96
#define	IBM_82660_MEM_BANK7_END		0x97

/* 
 * Helper functions for working with the Memory Bank 
 * Start/End Address registers.
 */
#define	IBM_82660_BANK0_ADDR(x)		((x) & 0xFF)
#define	IBM_82660_BANK1_ADDR(x)		(((x) & 0xFF00) >> 8)
#define	IBM_82660_BANK2_ADDR(x)		(((x) & 0xFF0000) >> 16)
#define	IBM_82660_BANK3_ADDR(x)		(((x) & 0xFF000000) >> 24)

/* Memory Bank Extended Ending Addresses */
#define	IBM_82660_MEM_BANK0_EXTEND	0x98
#define	IBM_82660_MEM_BANK1_EXTEND	0x99
#define	IBM_82660_MEM_BANK2_EXTEND	0x9A
#define	IBM_82660_MEM_BANK3_EXTEND	0x9B
#define	IBM_82660_MEM_BANK4_EXTEND	0x9C
#define	IBM_82660_MEM_BANK5_EXTEND	0x9D
#define	IBM_82660_MEM_BANK6_EXTEND	0x9E
#define	IBM_82660_MEM_BANK7_EXTEND	0x9F

#define	IBM_82660_MEM_BANK_ENABLE	0xA0
#define	IBM_82660_MEM_BANK0_ENABLED	0x01
#define	IBM_82660_MEM_BANK1_ENABLED	0x02
#define	IBM_82660_MEM_BANK2_ENABLED	0x04
#define	IBM_82660_MEM_BANK3_ENABLED	0x08

#define	IBM_82660_MEM_TIMING_1		0xA1
#define	IBM_82660_MEM_TIMING_2		0xA2

/* Memory Bank Addressing Modes */
#define	IBM_82660_MEM_BANK01_ADDR_MODE	0xA4		/* Bank 0 and 1 */
#define	IBM_82660_MEM_BANK23_ADDR_MODE	0xA5		/* Bank 2 and 3 */
#define	IBM_82660_MEM_BANK45_ADDR_MODE	0xA6		/* Bank 4 and 5 */
#define	IBM_82660_MEM_BANK67_ADDR_MODE	0xA7		/* Bank 6 and 7 */

#define IBM_82660_CACHE_STATUS          0xB1
#define IBM_82660_CACHE_STATUS_L1_EN    0x01
#define IBM_82660_CACHE_STATUS_L2_EN    0x02

#define	IBM_82660_RAS_WATCHDOG_TIMER	0xB6

#define	IBM_82660_SINGLEBIT_ERR_CNTR	0xB8
#define	IBM_82660_SINGLEBIT_ERR_LEVEL	0xB9

/* Bridge Options */
#define IBM_82660_OPTIONS_1             0xBA
#define IBM_82660_OPTIONS_1_MCP         0x01
#define IBM_82660_OPTIONS_1_TEA         0x02
#define IBM_82660_OPTIONS_1_ISA         0x04

#define	IBM_82660_OPTIONS_2		0xBB

#define	IBM_82660_ERR_ENABLE_1		0xC0
#define	IBM_82660_ERR_STATUS_1		0xC1

#define	IBM_82660_CPU_ERR_STATUS	0xC3

#define	IBM_82660_ERR_ENABLE_2		0xC4
#define	IBM_82660_ERR_STATUS_2		0xC5

#define	IBM_82660_PCI_ERR_STATUS	0xC7

#define IBM_82660_OPTIONS_3             0xD4
#define IBM_82660_OPTIONS_3_ECC         0x01
#define IBM_82660_OPTIONS_3_DRAM        0x04
#define IBM_82660_OPTIONS_3_SRAM        0x08
#define IBM_82660_OPTIONS_3_SNOOP       0x80

#define IBM_82660_SYSTEM_CTRL           0x81C
#define IBM_82660_SYSTEM_CTRL_L2_EN     0x40
#define IBM_82660_SYSTEM_CTRL_L2_MI     0x80

#endif /* _DEV_IC_IBM82660REG_H_ */

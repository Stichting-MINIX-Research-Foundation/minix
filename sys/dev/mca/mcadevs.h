/*	$NetBSD: mcadevs.h,v 1.25 2008/04/28 20:23:53 martin Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	NetBSD: mcadevs,v 1.22 2007/04/12 18:00:05 garbled Exp
 */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * Copyright (c) 1996-1999 Scott D. Telford.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Scott Telford <s.telford@ed.ac.uk>.
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

/*
 * Supported MCA devices
 */

#define	MCA_PRODUCT_BT640A	0x0708	/* BusTek BT-640A SCSI Host Adapter */
#define	MCA_PRODUCT_AHA1640	0x0F1F	/* Adaptec AHA-1640 SCSI Adapter */
#define	MCA_PRODUCT_TIARA	0x6000	/* Tiara LANCard/E Ethernet Adapter */
#define	MCA_PRODUCT_TIARA_TP	0x6001	/* Tiara LANcard/E Ethernet Adapter */
#define	MCA_PRODUCT_SMC3016	0x6016	/* Standard MicroSystems 3016/MC Ethernet */
#define	MCA_PRODUCT_3C523	0x6042	/* 3Com EtherLink/MC Ethernet Adapter (3C523) */
#define	MCA_PRODUCT_WD_8013EP	0x61C8	/* EtherCard PLUS Elite/A (8013EP/A) */
#define	MCA_PRODUCT_WD_8013WP	0x61C9	/* EtherCard PLUS Elite 10T/A (8013WP/A) */
#define	MCA_PRODUCT_3C529	0x627C	/* 3Com 3C529 Ethernet Adapter */
#define	MCA_PRODUCT_3C529_TP	0x627D	/* 3Com 3C529-TP Ethernet Adapter */
#define	MCA_PRODUCT_3C529_TM	0x62DB	/* 3Com 3C529 Ethernet Adapter (test mode) */
#define	MCA_PRODUCT_3C529_2T	0x62F6	/* 3Com 3C529 Ethernet Adapter (10base2/T) */
#define	MCA_PRODUCT_3C529_T	0x62F7	/* 3Com 3C529 Ethernet Adapter (10baseT) */
#define	MCA_PRODUCT_ARCOAE	0x6354	/* Arco Electronics AE/2 Ethernet Adapter */
#define	MCA_PRODUCT_AT1720T	0x6410	/* ATI AT1720T */
#define	MCA_PRODUCT_AT1720BT	0x6413	/* ATI AT1720BT */
#define	MCA_PRODUCT_AT1720AT	0x6416	/* ATI AT1720AT */
#define	MCA_PRODUCT_AT1720FT	0x6419	/* ATI AT1720FT */
#define	MCA_PRODUCT_NEOCOM1	0x6791	/* NeoTecH Single RS-232 Async. Adapter, SM110 */
#define	MCA_PRODUCT_SKNETG	0x6AFD	/* SKNET MC2+ Ethernet Adapter/G */
#define	MCA_PRODUCT_SKNETPER	0x6BE9	/* SKNET Personal Ethernet Adapter */
#define	MCA_PRODUCT_WD_8003E	0x6FC0	/* WD EtherCard PLUS/A (WD8003E/A or WD8003ET/A) */
#define	MCA_PRODUCT_WD_8003ST	0x6FC1	/* WD StarCard PLUS/A (WD8003ST/A) */
#define	MCA_PRODUCT_WD_8003W	0x6FC2	/* WD EtherCard PLUS 10T/A (WD8003W/A) */
#define	MCA_PRODUCT_HRAM	0x7007	/* HyperRAM MC 32/16 Memory Expansion */
#define	MCA_PRODUCT_IQRAM	0x7024	/* InterQuadram QuadMEG PS8 Extended Memory/Adapter */
#define	MCA_PRODUCT_MICRAM	0x7049	/* Micron Beyond 50/60 Memory Expansion */
#define	MCA_PRODUCT_ASTRAM	0x7050	/* AST RampagePlus/MC Memory Expansion */
#define	MCA_PRODUCT_KINGRAM	0x708E	/* Kingston KTM-8000/286 Memory Expansion */
#define	MCA_PRODUCT_KINGRAM8	0x708F	/* Kingston KTM-8000/386 Memory Expansion */
#define	MCA_PRODUCT_KINGRAM16	0x70D0	/* Kingston KTM-16000/386 Memory Expansion */
#define	MCA_PRODUCT_KINGRAM609	0x70D4	/* Kingston KTM-609/16 Memory Expansion */
#define	MCA_PRODUCT_CENET16	0x70DE	/* Compex Inc. PS/2 ENET16-MC/P Microchannel Ad. */
#define	MCA_PRODUCT_NE2	0x7154	/* Novell NE/2 Ethernet Adapter */
#define	MCA_PRODUCT_HYPRAM	0x72F3	/* HyperRAM MC 32/16 SIMM-MF Memory Expansion */
#define	MCA_PRODUCT_QRAM1	0x76DA	/* Quadmeg PS/Q Memory Adapter */
#define	MCA_PRODUCT_QRAM2	0x76DE	/* Quadmeg PS/Q Memory Adapter */
#define	MCA_PRODUCT_EVERAM	0x77FB	/* Everex EV-136 4Mb Memory Expansion */
#define	MCA_PRODUCT_BOCARAM	0x7A7A	/* Boca BOCARAM/2 PLUS Memory Expansion */
#define	MCA_PRODUCT_NCR53C90	0x7F4F	/* NCR SCSI Host Adapter */
#define	MCA_PRODUCT_IBM_ETH_RAINBOW	0x8F98	/* IBM 10 Mbps Integrated Ethernet (Rainbow) */
#define	MCA_PRODUCT_IBM_53C720	0x8FBA	/* IBM NCR 53C720 Integrated SCSI Controller */
#define	MCA_PRODUCT_IBM_SVGA	0x90EE	/* IBM SVGA Adapter/A */
#define	MCA_PRODUCT_IBM_ESDIC	0xDDFF	/* IBM ESDI Fixed Disk Controller */
#define	MCA_PRODUCT_IBM_MPCOM	0xDEFF	/* IBM Multi-Protocol Communications Adapter */
#define	MCA_PRODUCT_IBM_ESDIC_IG	0xDF9F	/* IBM Integ. ESDI Fixed Disk & Controller */
#define	MCA_PRODUCT_ITR	0xE001	/* IBM Token Ring 16/4 Adapter/A */
#define	MCA_PRODUCT_IBM_MOD	0xEDFF	/* IBM Internal Modem */
#define	MCA_PRODUCT_IBM_WD_2	0xEFD4	/* IBM PS/2 Adapter/A for Ethernet Networks (UTP) */
#define	MCA_PRODUCT_IBM_WD_T	0xEFD5	/* IBM PS/2 Adapter/A for Ethernet Networks (BNC) */
#define	MCA_PRODUCT_IBM_WD_O	0xEFE5	/* IBM PS/2 Adapter/A for Ethernet Networks */
#define	MCA_PRODUCT_IBM_SIO_RAINBOW	0xF6FE	/* IBM Standard I/O Planar (Rainbow) */
#define	MCA_PRODUCT_IBMRAM1	0xF7F7	/* IBM 2-8Mb 80286 Memory Expansion */
#define	MCA_PRODUCT_IBMRAM2	0xF7FE	/* IBM Expanded Memory */
#define	MCA_PRODUCT_IBMRAM3	0xFAFF	/* IBM 32-bit Memory Expansion */
#define	MCA_PRODUCT_IBMRAM4	0xFCFF	/* IBM Memory Expansion */
#define	MCA_PRODUCT_IBMRAM5	0xFDDE	/* IBM Enhanced 80386 Memory Expansion w/ROM */
#define	MCA_PRODUCT_IBMRAM6	0xFDDF	/* IBM Enhanced 80386 Memory Expansion */
#define	MCA_PRODUCT_IBMRAM7	0xFEFE	/* IBM 2Mb 16-bit Memory Adapter */

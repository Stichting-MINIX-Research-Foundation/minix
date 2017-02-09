/*	$NetBSD: eisadevs.h,v 1.30 2005/12/11 12:21:20 christos Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	NetBSD: eisadevs,v 1.27 2005/02/27 00:26:59 perry Exp
 */

/*
 * Copyright (c) 1995, 1996 Christopher G. Demetriou
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * List of known products, grouped and sorted by vendor code.
 */

/* Adaptec products */
#define	EISA_PRODUCT_ADP0000	"Adaptec AHA-1740 SCSI"
#define	EISA_PRODUCT_ADP0001	"Adaptec AHA-1740A SCSI"
#define	EISA_PRODUCT_ADP0002	"Adaptec AHA-1742A SCSI"
#define	EISA_PRODUCT_ADP0400	"Adaptec AHA-1744 SCSI"
#define	EISA_PRODUCT_ADP7770	"Adaptec AIC-7770 SCSI (on motherboard)"
#define	EISA_PRODUCT_ADP7771	"Adaptec AHA-274x SCSI"
#define	EISA_PRODUCT_ADP7756	"Adaptec AHA-284x SCSI (BIOS enabled)"
#define	EISA_PRODUCT_ADP7757	"Adaptec AHA-284x SCSI (BIOS disabled)"

/* AMI products */
#define	EISA_PRODUCT_AMI4801	"AMI Series 48 SCSI"

/* AT&T products */
#define	EISA_PRODUCT_ATT2408	"AT&T EATA SCSI controller"

/* BusLogic products */
#define	EISA_PRODUCT_BUS4201	"BusLogic Bt74xB SCSI"
#define	EISA_PRODUCT_BUS4202	"BusLogic Bt74xC SCSI"
#define	EISA_PRODUCT_BUS6001	"BusLogic Bt760 Ethernet"
#define	EISA_PRODUCT_BUS6301	"BusLogic Bt763E Ethernet"

/* Compaq products */
#define	EISA_PRODUCT_CPQ4001	"Compaq IDA array controller"
#define	EISA_PRODUCT_CPQ4002	"Compaq IDA-2 array controller"
#define	EISA_PRODUCT_CPQ4010	"Compaq IEAS array controller"
#define	EISA_PRODUCT_CPQ4020	"Compaq SMART array controller"
#define	EISA_PRODUCT_CPQ4030	"Compaq SMART-2/E array controller"
#define	EISA_PRODUCT_CPQ6101	"Compaq NetFlex-2 ENET-TR"

/* Digital Equipment products */
#define	EISA_PRODUCT_DEC4220	"Digital Equipment DE422 Ethernet"
#define	EISA_PRODUCT_DEC4250	"Digital Equipment DE425 Ethernet"
#define	EISA_PRODUCT_DEC3001	"Digital Equipment DEFEA FDDI Controller"
#define	EISA_PRODUCT_DEC3002	"Digital Equipment DEFEA FDDI Controller"
#define	EISA_PRODUCT_DEC3003	"Digital Equipment DEFEA FDDI Controller"
#define	EISA_PRODUCT_DEC3004	"Digital Equipment DEFEA FDDI Controller"

/* DPT products */
#define	EISA_PRODUCT_DPT2402	"Distributed Processing Technology PM2012A/9X EATA SCSI controller"
#define	EISA_PRODUCT_DPTA401	"Distributed Processing Technology PM2012B/9X EATA SCSI controller"
#define	EISA_PRODUCT_DPTA402	"Distributed Processing Technology PM2012B2/9X EATA SCSI controller"
#define	EISA_PRODUCT_DPTA410	"Distributed Processing Technology PM2x22A/9X EATA SCSI controller"
#define	EISA_PRODUCT_DPTA411	"Distributed Processing Technology Spectre EATA SCSI controller"
#define	EISA_PRODUCT_DPTA412	"Distributed Processing Technology PM2021A/9X EATA SCSI controller"
#define	EISA_PRODUCT_DPTA420	"Distributed Processing Technology PM2042 EATA SCSI controller"
#define	EISA_PRODUCT_DPTA501	"Distributed Processing Technology PM2012B1/9X EATA SCSI controller"
#define	EISA_PRODUCT_DPTA502	"Distributed Processing Technology PM2012Bx/9X EATA SCSI controller"
#define	EISA_PRODUCT_DPTA701	"Distributed Processing Technology PM2011B1/9X EATA SCSI controller"
#define	EISA_PRODUCT_DPTBC01	"Distributed Processing Technology PM3011/7X ESDI controller"

/* DeskStation Technology, Inc. products */
#define	EISA_PRODUCT_DTI0000	"DeskStation Technology, Inc. Evolution RISC PC"

/* ETI products */
#define	EISA_PRODUCT_ETI1001	"Microdyne NE3300 Ethernet Rev. C & D"

/* FORE Systems products */
#define	EISA_PRODUCT_FSI2001	"FORE Systems ESA-200 ATM"
#define	EISA_PRODUCT_FSI2002	"FORE Systems ESA-200A ATM"
#define	EISA_PRODUCT_FSI2003	"FORE Systems ESA-200E ATM"

/* HP products */
#define	EISA_PRODUCT_HWP0C70	"Hewlett-Packard HP-IB Host Adapter"
#define	EISA_PRODUCT_HWP0C80	"Hewlett-Packard SCSI Host Adapter (Cirrus-II) -- 25525A"
#define	EISA_PRODUCT_HWP1850	"Hewlett-Packard LAN AdapterCard -- 25567A"
#define	EISA_PRODUCT_HWP1980	"Hewlett-Packard ATM Adapter -- J2802A"
#define	EISA_PRODUCT_HWP2051	"Hewlett-Packard EISA Test Adapter Card"
#define	EISA_PRODUCT_HWPC000	"Hewlett-Packard Series 700 EISA System Board"
#define	EISA_PRODUCT_HWPC010	"Hewlett-Packard Series 700 EISA System Board"
#define	EISA_PRODUCT_HWPC051	"Hewlett-Packard Series 700 EISA System Board"
#define	EISA_PRODUCT_HWPC091	"Hewlett-Packard EISA System Board"
#define	EISA_PRODUCT_HWPC0D1	"Hewlett-Packard EISA System Board"
#define	EISA_PRODUCT_HWPC0E1	"Hewlett-Packard EISA System Board"

/* Interphase products */
#define	EISA_PRODUCT_INP25D0	"Interphase Seahawk 4811 FDDI Controller"

/* Intel products */
#define	EISA_PRODUCT_INT1010	"Intel EtherExpress 32 Flash Ethernet"

/* Madge products */
#define	EISA_PRODUCT_MDG0002	"Madge Networks Smart 16/4 EISA Ringnode"

/* Mylex products */
#define	EISA_PRODUCT_MLX0070	"Mylex DAC960 RAID controller"
#define	EISA_PRODUCT_MLX0071	"Mylex DAC960 RAID controller"
#define	EISA_PRODUCT_MLX0072	"Mylex DAC960 RAID controller"
#define	EISA_PRODUCT_MLX0073	"Mylex DAC960 RAID controller"
#define	EISA_PRODUCT_MLX0074	"Mylex DAC960 RAID controller"
#define	EISA_PRODUCT_MLX0075	"Mylex DAC960-A RAID controller"
#define	EISA_PRODUCT_MLX0076	"Mylex DAC960-A RAID controller"
#define	EISA_PRODUCT_MLX0077	"Mylex DAC960-A RAID controller"

/* NEC products */
#define	EISA_PRODUCT_NEC8200	"NEC EATA SCSI controller"

/* Standard Microsystems (SMC) */
#define	EISA_PRODUCT_SMC8010	"Standard Microsystems Corp. Ethercard Elite32C Ultra"
#define	EISA_PRODUCT_SMC0110	"Standard Microsystems Corp. Elite32 Ethernet"
#define	EISA_PRODUCT_SMCA010	"Standard Microsystems Corp. Ether 10/100 (9232DST)"

/* UltraStor products */
#define	EISA_PRODUCT_USC0240	"UltraStor 24f SCSI"

/* 3Com products */
#define	EISA_PRODUCT_TCM5090	"3Com 3C509-TP Ethernet"
#define	EISA_PRODUCT_TCM5091	"3Com 3C509 Ethernet"
#define	EISA_PRODUCT_TCM5092	"3Com 3C579-TP Ethernet"
#define	EISA_PRODUCT_TCM5093	"3Com 3C579 Ethernet"
#define	EISA_PRODUCT_TCM5094	"3Com 3C509 Ethernet Combo"
#define	EISA_PRODUCT_TCM5095	"3Com 3C509-TP Ethernet"
#define	EISA_PRODUCT_TCM5098	"3Com 3C509-TP Ethernet"
#define	EISA_PRODUCT_TCM5920	"3Com 3C592 Etherlink III"
#define	EISA_PRODUCT_TCM5970	"3Com 3C597 Fast Etherlink TX"
#define	EISA_PRODUCT_TCM5971	"3Com 3C597 Fast Etherlink T4"
#define	EISA_PRODUCT_TCM5972	"3Com 3C597 Fast Etherlink MII"
#define	EISA_PRODUCT_TCM6790	"3Com 3C679 Tokenlink III 16/4"

/*	$NetBSD: pucdata.c,v 1.97 2015/08/23 18:00:30 jakllsch Exp $	*/

/*
 * Copyright (c) 1998, 1999 Christopher G. Demetriou.  All rights reserved.
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
 * PCI "universal" communications card driver configuration data (used to
 * match/attach the cards).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pucdata.c,v 1.97 2015/08/23 18:00:30 jakllsch Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pucvar.h>
#include <dev/ic/comreg.h>

const struct puc_device_description puc_devices[] = {
	/*
	 * Advantech multi serial cards
	 */
	/* Advantech PCI-1604UP 2 UARTs based on OX16PCI952 */
	{   "Advantech PCI-1604UP UARTs",
	    {	PCI_VENDOR_ADVANTECH,	PCI_PRODUCT_ADVANTECH_PCI1604, 0, 0 },
	    {	0xffff,	0xffff,	0x0,	0x0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ * 8 },
	    },
	},

	{   "Advantech PCI-1610 UARTs",
	    {	PCI_VENDOR_ADVANTECH,	PCI_PRODUCT_ADVANTECH_PCI1600,
		PCI_PRODUCT_ADVANTECH_PCI1610,	0x0 },
	    {	0xffff,	0xffff,	0xffff,	0x0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 8 },
	    },
	},

	{   "Advantech PCI-1612 UARTs",
	    {	PCI_VENDOR_ADVANTECH,	PCI_PRODUCT_ADVANTECH_PCI1600,
		PCI_PRODUCT_ADVANTECH_PCI1612,	0x0 },
	    {	0xffff,	0xffff,	0xffff,	0x0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 8 },
	    },
	},

	/* The use of subvendor ID is bit strange... */
	{   "Advantech PCI-1620 (1-4) UARTs",
	    {	PCI_VENDOR_ADVANTECH,	PCI_PRODUCT_ADVANTECH_PCI1600,
		PCI_PRODUCT_ADVANTECH_PCI1620,	0x0 },
	    {	0xffff,	0xffff,	0xffff,	0x0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 8 },
	    },
	},

	/* The use of subvendor ID is bit strange... */
	{   "Advantech PCI-1620 (5-8) UARTs",
	    {	PCI_VENDOR_ADVANTECH,	PCI_PRODUCT_ADVANTECH_PCI1620_1,
		PCI_PRODUCT_ADVANTECH_PCI1620,	0x0 },
	    {	0xffff,	0xffff,	0xffff,	0x0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 2 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 2 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 2 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 2 },
	    },
	},

	/*
	 * Addi-Data APCI-7800 8-port serial card.
	 * Uses an AMCC chip as PCI bridge.
	 */
	{   "Addi-Data APCI-7800",
	    {   PCI_VENDOR_AMCIRCUITS, PCI_PRODUCT_AMCIRCUITS_ADDI7800, 0, 0  },
	    {   0xffff, 0xffff, 0, 0  },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x08, COM_FREQ },
	    },
	},

	/* Avlab Technology, Inc. PCI 2 Serial: 2S */
	{   "Avlab PCI 2 Serial",
	    {	PCI_VENDOR_AVLAB, PCI_PRODUCT_AVLAB_PCI2S,	0, 0  },
	    {	0xffff,	0xffff,					0, 0  },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
	    },
	},

	/* Avlab Technology, Inc. Low Profile PCI 4 Serial: 4S */
	{   "Avlab Low Profile PCI 4 Serial",
	    {	PCI_VENDOR_AVLAB, PCI_PRODUCT_AVLAB_LPPCI4S,	0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ },
	    },
	},

	/* Avlab Technology, Inc. Low Profile PCI 4 Serial: 4S */
	{   "Avlab Low Profile PCI 4 Serial",
	    {	PCI_VENDOR_AVLAB, PCI_PRODUCT_AVLAB_LPPCI4S_2,	0, 0  },
	    {	0xffff,	0xffff,					0, 0  },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ },
	    },
	},

	/*
	 * B&B Electronics MIPort Serial cards.
	 */
	{   "BBELEC ISOLATED_2_PORT",
	    {	PCI_VENDOR_BBELEC, PCI_PRODUCT_BBELEC_ISOLATED_2_PORT, 0, 0 },
	    {	0xffff, 0xffff, 0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0000, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0200, COM_FREQ * 8 },
	    },
	},
	{   "BBELEC ISOLATED_4_PORT",
	    {	PCI_VENDOR_BBELEC, PCI_PRODUCT_BBELEC_ISOLATED_4_PORT, 0, 0 },
	    {	0xffff, 0xffff, 0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0000, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0200, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0400, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0600, COM_FREQ * 8 },
	    },
	},
	{   "BBELEC ISOLATED_8_PORT",
	    {	PCI_VENDOR_BBELEC, PCI_PRODUCT_BBELEC_ISOLATED_8_PORT, 0, 0 },
	    {	0xffff, 0xffff, 0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0000, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0200, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0400, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0600, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0800, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0a00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0c00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0e00, COM_FREQ * 8 },
	    },
	},

	/*
	 * Comtrol
	 */
	{   "Comtrol RocketPort 550/8 RJ11 part A",
	    {	PCI_VENDOR_COMTROL, PCI_PRODUCT_COMTROL_ROCKETPORT550811A,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 4},
	    },
	},
	{   "Comtrol RocketPort 550/8 RJ11 part B",
	    {	PCI_VENDOR_COMTROL, PCI_PRODUCT_COMTROL_ROCKETPORT550811B,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 4},
	    },
	},
	{   "Comtrol RocketPort 550/8 Octa part A",
	    {	PCI_VENDOR_COMTROL, PCI_PRODUCT_COMTROL_ROCKETPORT5508OA,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 4},
	    },
	},
	{   "Comtrol RocketPort 550/8 Octa part B",
	    {	PCI_VENDOR_COMTROL, PCI_PRODUCT_COMTROL_ROCKETPORT5508OB,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 4},
	    },
	},
	{   "Comtrol RocketPort 550/4 RJ45",
	    {	PCI_VENDOR_COMTROL, PCI_PRODUCT_COMTROL_ROCKETPORT5504, 0, 0 },
	    {	0xffff,	0xffff,	0,	0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 4},
	    },
	},
	{   "Comtrol RocketPort 550/Quad",
	    {	PCI_VENDOR_COMTROL, PCI_PRODUCT_COMTROL_ROCKETPORT550Q, 0, 0 },
	    {	0xffff,	0xffff,	0,	0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 4},
	    },
	},
	{   "Comtrol RocketPort 550/16 part A",
	    {	PCI_VENDOR_COMTROL, PCI_PRODUCT_COMTROL_ROCKETPORT55016A,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 4},
	    },
	},
	{   "Comtrol RocketPort 550/16 part B",
	    {	PCI_VENDOR_COMTROL, PCI_PRODUCT_COMTROL_ROCKETPORT55016B,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x20, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x28, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x30, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x38, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x40, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x48, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x50, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x58, COM_FREQ * 4},
	    },
	},
	{   "Comtrol RocketPort 550/8 part A",
	    {	PCI_VENDOR_COMTROL, PCI_PRODUCT_COMTROL_ROCKETPORT5508A,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 4},
	    },
	},
	{   "Comtrol RocketPort 550/8 part B",
	    {	PCI_VENDOR_COMTROL, PCI_PRODUCT_COMTROL_ROCKETPORT5508B,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 4},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 4},
	    },
	},

	/*
	 * Decision PCCOM PCI series. PLX 9052 with 1 or 2 16554 UARTS
	 */
	/* Decision Computer Inc PCCOM 2 Port RS232/422/485: 2S */
	{   "Decision Computer Inc PCCOM 2 Port RS232/422/485",
	    {	PCI_VENDOR_DCI,	PCI_PRODUCT_DCI_APCI2,	0x0,	0x0	},
	    {	0xffff,	0xffff,	0x0,	0x0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x08, COM_FREQ},
	    },
	},

	/* Decision Computer Inc PCCOM 4 Port RS232/422/485: 4S */
	{   "Decision Computer Inc PCCOM 4 Port RS232/422/485",
	    {	PCI_VENDOR_DCI,	PCI_PRODUCT_DCI_APCI4,	0x0,	0x0	},
	    {	0xffff,	0xffff,	0x0,	0x0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x08, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x10, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x18, COM_FREQ},
	    },
	},

	/* Decision Computer Inc PCCOM 8 Port RS232/422/485: 8S */
	{   "Decision Computer Inc PCCOM 8 Port RS232/422/485",
	    {	PCI_VENDOR_DCI,	PCI_PRODUCT_DCI_APCI8,	0x0,	0x0	},
	    {	0xffff,	0xffff,	0x0,	0x0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x08, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x10, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x18, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x20, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x28, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x30, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x38, COM_FREQ},
	    },
	},

	/* Digi International Digi Neo 4 Serial */
	{   "Digi International Digi Neo 4 Serial",
	    {	PCI_VENDOR_DIGI, PCI_PRODUCT_DIGI_NEO4,		0, 0  },
	    {	0xffff, 0xffff,					0, 0  },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0000, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0200, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0400, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0600, COM_FREQ * 8 },
	    },
	},

	/* Digi International Digi Neo 8 Serial */
	{   "Digi International Digi Neo 8 Serial",
	    {	PCI_VENDOR_DIGI, PCI_PRODUCT_DIGI_NEO8,		0, 0  },
	    {	0xffff, 0xffff,					0, 0  },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0000, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0200, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0400, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0600, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0800, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0a00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0c00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0e00, COM_FREQ * 8 },
	    },
	},

	/* Digi International Digi Neo 8 Serial (PCIe) */
	{   "Digi International Digi Neo 8 Serial (PCIe)",
	    {	PCI_VENDOR_DIGI, PCI_PRODUCT_DIGI_NEO8_PCIE,	0, 0  },
	    {	0xffff, 0xffff,					0, 0  },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0000, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0200, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0400, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0600, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0800, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0a00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0c00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0e00, COM_FREQ * 8 },
	    },
	},

	{   "EXAR XR17D152",
	    {   PCI_VENDOR_EXAR, PCI_PRODUCT_EXAR_XR17D152, 0,      0       },
	    {   0xffff, 0xffff, 0,      0       },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0000, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0200, COM_FREQ * 8 },
	    },
	},
	{   "EXAR XR17D154",
	    {   PCI_VENDOR_EXAR, PCI_PRODUCT_EXAR_XR17D154, 0,      0       },
	    {   0xffff, 0xffff, 0,      0       },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0000, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0200, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0400, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0600, COM_FREQ * 8 },
	    },
	},

	/*
	 * Multi-Tech ISI5634PCI/4 4-port modem board.
	 * Has a 4-channel Exar XR17C154 UART, but with bogus product ID in its
	 * config EEPROM.
	 */
	{   "Multi-Tech ISI5634PCI/4",
	    {   PCI_VENDOR_EXAR, PCI_PRODUCT_EXAR_XR17D158, 0x2205,      0x2003       },
	    {   0xffff, 0xffff, 0xffff,      0xffff       },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0000, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0200, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0400, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0600, COM_FREQ * 8 },
	    },
	},

	{   "EXAR XR17D158",
	    {   PCI_VENDOR_EXAR, PCI_PRODUCT_EXAR_XR17D158, 0,      0       },
	    {   0xffff, 0xffff, 0,      0       },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0000, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0200, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0400, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0600, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0800, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0a00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0c00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x0e00, COM_FREQ * 8 },
	    },
	},

	/* IBM SurePOS 300 Series (481033H) serial ports */
	{   "IBM SurePOS 300 Series (481033H)",
	    {   PCI_VENDOR_IBM, PCI_PRODUCT_IBM_4810_SCC, 0, 0 },
	    {   0xffff, 0xffff,                           0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ }, /* Port C */
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ }, /* Port D */
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ }, /* Port E */
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ }, /* Port F */
	    },
	},

	/* I-O DATA RSA-PCI: 2S */
	{   "I-O DATA RSA-PCI 2-port serial",
	    {	PCI_VENDOR_IODATA, PCI_PRODUCT_IODATA_RSAPCI, 0, 0 },
	    {	0xffff, 0xffff, 0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ },
	    },
	},

	/* Lava Computers 2SP-PCI */
	{   "Lava Computers 2SP-PCI parallel port",
	    {	PCI_VENDOR_LAVA,	PCI_PRODUCT_LAVA_TWOSP_1P, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_LPT, PCI_BAR0, 0x00, 0x00 },
	    },
	},

	/* Lava Computers 2SP-PCI and Quattro-PCI serial ports */
	{   "Lava Computers dual serial port",
	    {	PCI_VENDOR_LAVA,	PCI_PRODUCT_LAVA_TWOSP_2S, 0, 0 },
	    {	0xffff,	0xfffc,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
	    },
	},

	/* Lava Computers 2SP-PCI and Quattro-PCI serial ports */
	{   "Lava Computers Quattro A",
	    {	PCI_VENDOR_LAVA,	PCI_PRODUCT_LAVA_QUATTRO_AB, 0, 0 },
	    {	0xffff,	0xfffc,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
	    },
	},

	/* Lava Computers 2SP-PCI and Quattro-PCI serial ports */
	{   "Lava Computers Quattro B",
	    {	PCI_VENDOR_LAVA,	PCI_PRODUCT_LAVA_QUATTRO_CD, 0, 0 },
	    {	0xffff,	0xfffc,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
	    },
	},

	/* Lava Computers DSerial PCI serial ports */
	{   "Lava Computers serial port",
	    {	PCI_VENDOR_LAVA,	PCI_PRODUCT_LAVA_IOFLEX_2S_0, 0, 0 },
	    {	0xffff,	0xfffc,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Lava Computers Quattro-PCI serial ports */
	{   "Lava Quattro-PCI A 4-port serial",
	    {   PCI_VENDOR_LAVA, PCI_PRODUCT_LAVA_QUATTRO_AB2, 0, 0 },
	    {   0xffff, 0xfffc, 0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
	    },
	},

	/* Lava Computers Quattro-PCI serial ports */
	{   "Lava Quattro-PCI B 4-port serial",
	    {   PCI_VENDOR_LAVA, PCI_PRODUCT_LAVA_QUATTRO_CD2, 0, 0 },
	    {   0xffff, 0xfffc, 0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
	    },
	},

	/* Lava Computers Octopus-550 serial ports */
	{   "Lava Computers Octopus-550 8-port serial",
	    {	PCI_VENDOR_LAVA,	PCI_PRODUCT_LAVA_OCTOPUS550_0, 0, 0 },
	    {	0xffff,	0xfffc,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ },
	    },
	},

	/* Lava Computers Octopus-550 serial ports */
	{   "Lava Computers Octopus-550 B 8-port serial",
	    {	PCI_VENDOR_LAVA,	PCI_PRODUCT_LAVA_OCTOPUS550_1, 0, 0 },
	    {	0xffff,	0xfffc,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ },
	    },
	},

	/* Lava Computers single port serial PCI card */
	{   "Lava Computers SSERIAL-PCI",
	    {	PCI_VENDOR_LAVA,	PCI_PRODUCT_LAVA_SSERIAL, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Actiontec  56K PCI Master */
	{   "Actiontec 56K PCI Master",
	    {	PCI_VENDOR_LUCENT,	PCI_PRODUCT_LUCENT_VENUSMODEM,
		0x0, 0x0 },
	    {	0xffff,	0xffff,	0x0,	0x0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR1,	0x00, COM_FREQ },
	    },
	},

	/*
	 * This is the Middle Digital, Inc. PCI-Weasel, which
	 * uses a PCI interface implemented in FPGA.
	 */
	{   "Middle Digital, Inc. Weasel serial port",
	    {	PCI_VENDOR_MIDDLE_DIGITAL,
		PCI_PRODUCT_MIDDLE_DIGITAL_WEASEL_SERIAL, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Moxa Technologies Co., Ltd. PCI I/O Card 2S RS232 */
	{   "Moxa Technologies, SmartIO CP-102/PCI",
	    {	PCI_VENDOR_MOXA,	PCI_PRODUCT_MOXA_CP102U, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x08, COM_FREQ * 8 },
	    },
	},

	/* Moxa Technologies Co., Ltd. PCI I/O Card 4S RS232/422/485 */
	{   "Moxa Technologies, SmartIO C104H/PCI",
	    {	PCI_VENDOR_MOXA,	PCI_PRODUCT_MOXA_C104H, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x18, COM_FREQ * 8 },
	    },
	},

	/* Moxa Technologies Co., Ltd. PCI I/O Card 4S RS232 */
	{   "Moxa Technologies, SmartIO CP-104/PCI",
	    {	PCI_VENDOR_MOXA,	PCI_PRODUCT_MOXA_CP104, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x18, COM_FREQ * 8 },
	    },
	},

	/* Moxa Technologies Co., Ltd. PCI I/O Card 4S RS232 */
	{   "Moxa Technologies, SmartIO CP-104-V2/PCI",
	    {	PCI_VENDOR_MOXA,	PCI_PRODUCT_MOXA_CP104V2, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x18, COM_FREQ * 8 },
	    },
	},

	/* Moxa Technologies Co., Ltd. PCI I/O Card 4S RS232 */
	{   "Moxa Technologies, SmartIO CP-104-EL/PCIe",
	    {	PCI_VENDOR_MOXA,	PCI_PRODUCT_MOXA_CP104EL, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x18, COM_FREQ * 8 },
	    },
	},

	/* Moxa Technologies Co., Ltd. PCI I/O Card 4S RS232/422/485 */
	{   "Moxa Technologies, SmartIO CP-114/PCI",
	    {	PCI_VENDOR_MOXA,	PCI_PRODUCT_MOXA_CP114, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x18, COM_FREQ * 8 },
	    },
	},

	/* Moxa Technologies Co., Ltd. PCI I/O Card 8S RS232 */
	{   "Moxa Technologies, SmartIO C168H/PCI",
	    {	PCI_VENDOR_MOXA,	PCI_PRODUCT_MOXA_C168H, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x18, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x20, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x28, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x30, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x38, COM_FREQ * 8 },
	    },
	},

	/* Moxa Technologies Co., Ltd. PCI I/O Card 8S RS232 */
	{   "Moxa Technologies, SmartIO C168U/PCI",
	    {	PCI_VENDOR_MOXA,	PCI_PRODUCT_MOXA_C168U, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x18, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x20, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x28, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x30, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x38, COM_FREQ * 8 },
	    },
	},

	/* Moxa Technologies Co., Ltd. PCI-Express I/O Card 8S RS232 */
	{   "Moxa Technologies, SmartIO C168EL/PCIe",
	    {	PCI_VENDOR_MOXA,	PCI_PRODUCT_MOXA_C168EL, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x18, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x20, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x28, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x30, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x38, COM_FREQ * 8 },
	    },
	},
	/* Moxa Technologies Co., Ltd. PCI-Express I/O Card 8S RS232 */
	{   "Moxa Technologies, SmartIO CP-168EL-A/PCIe",
	    {	PCI_VENDOR_MOXA,	PCI_PRODUCT_MOXA_C168ELA, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x000, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x200, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x400, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x600, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x800, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0xa00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0xc00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0xe00, COM_FREQ * 8 },
	    },
	},

	/* NEC PK-UG-X001 K56flex PCI Modem card.
	   NEC MARTH bridge chip and Rockwell RCVDL56ACF/SP using. */
	{   "NEC PK-UG-X001 K56flex PCI Modem",
	    {	PCI_VENDOR_NEC,	PCI_PRODUCT_NEC_PKUGX001, PCI_VENDOR_NEC,
		0x8014 },
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* NEC PK-UG-X008 */
	{   "NEC PK-UG-X008",
	    {	PCI_VENDOR_NEC,	PCI_PRODUCT_NEC_PKUGX008, PCI_VENDOR_NEC,
		0x8012 },
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ},
	    },
	},

	/* NetMos 1P PCI : 1P */
	{   "NetMos NM9805 1284 Printer port",
	    {	PCI_VENDOR_NETMOS,	PCI_PRODUCT_NETMOS_NM9805, 0, 0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_LPT, PCI_BAR0, 0x00, 0x00 },
	    },
	},

	/* NetMos 2P PCI : 2P */
	{   "NetMos NM9815 Dual 1284 Printer port",
	    {	PCI_VENDOR_NETMOS,	PCI_PRODUCT_NETMOS_NM9815, 0, 0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_LPT, PCI_BAR0, 0x00, 0x00 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR2, 0x00, 0x00 },
	    },
	},

	/* NetMos 1S PCI NM9835 : 1S */
	{   "NetMos NM9835 UART",
	    {	PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9835, 0x1000, 0x0001 },
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* NetMos 2S PCI NM9835 : 2S */
	{   "NetMos NM9835 Dual UART",
	    {	PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9835, 0x1000, 0x0002 },
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
	    },
	},

	/* NetMos 2S1P PCI 16C650 : 2S, 1P */
	{   "NetMos NM9835 Dual UART and 1284 Printer port",
	    {	PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9835, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, PCI_BAR2, 0x00, 0x00 },
	    },
	},

	/* NetMos 4S0P PCI NM9845 : 4S, 0P */
	{   "NetMos NM9845 Quad UART",
	   {   PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9845, 0x1000, 0x0004 },
	   {   0xffff, 0xffff, 0xffff, 0xffff  },
	   {
	       { PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	       { PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
	       { PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ },
	       { PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ },
	   },
	},

	/* NetMos 4S1P PCI NM9845 : 4S, 1P */
	{   "NetMos NM9845 Quad UART and 1284 Printer port",
	    {   PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9845, 0x1000, 0x0014 },
	    {   0xffff, 0xffff, 0xffff, 0xffff  },
	    {
	       { PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	       { PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
	       { PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ },
	       { PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ },
	       { PUC_PORT_TYPE_LPT, PCI_BAR4, 0x00, 0x00 },
	    },
	},

	/* NetMos 6S PCI 16C650 : 6S, 0P */
	{   "NetMos NM9845 6 UART",
	    {   PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9845, 0x1000, 0x0006 },
	    {   0xffff, 0xffff, 0xffff, 0xffff  },
	    {
	       { PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	       { PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
	       { PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ },
	       { PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ },
	       { PUC_PORT_TYPE_COM, PCI_BAR4, 0x00, COM_FREQ },
	       { PUC_PORT_TYPE_COM, PCI_BAR5, 0x00, COM_FREQ },
	    },
	},

	/* NetMos 4S1P PCI NM9845 : 4S, 1P */
	{   "NetMos NM9845 Quad UART and 1284 Printer port (unknown type)",
	    {	PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9845, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, PCI_BAR4, 0x00, 0x00 },
	    },
	},

	/* NetMos 4S1P PCI NM9855 : 4S, 1P */
	{   "NetMos NM9855 Quad UART and 1284 Printer port (unknown type)",
	    {	PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9855, 0x1000, 0x0014 },
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_LPT, PCI_BAR0, 0x00, 0x00 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR5, 0x00, COM_FREQ },
	    },
	},

	/* NetMos 6S PCI NM9865 : 1S */
	{   "NetMos NM9865 1 UART",
	    {	PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9865, 0xa000, 0x1000 },
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* NetMos 2S PCI NM9865 : 2S */
	{   "NetMos NM9865 2 UART",
	    {	PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9865, 0xa000, 0x3002 },
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
	    },
	},

	/* NetMos 3S PCI NM9865 : 3S */
	{   "NetMos NM9865 3 UART",
	    {	PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9865, 0xa000, 0x3003 },
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ },
	    },
	},

	/* NetMos 4S PCI NM9865 : 4S */
	{   "NetMos NM9865 4 UART",
	    {	PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9865, 0xa000, 0x3004 },
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ },
	    },
	},

	/* NetMos PCI NM9865 : 1S 1P */
	{   "NetMos NM9865 Single UART and Single LPT",
	    {	PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9865, 0xa000, 0x3011 },
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, PCI_BAR1, 0x00, 0x00 },
	    },
	},

	/* NetMos PCI NM9865 : 2S 1P */
	{   "NetMos NM9865 Dual UART and Single LPT",
	    {	PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9865, 0xa000, 0x3012 },
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, PCI_BAR2, 0x00, 0x00 },
	    },
	},

	/* NetMos PCI NM9865 : 2P */
	{   "NetMos NM9865 Dual LPT",
	    {	PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9865, 0xa000, 0x3020 },
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_LPT, PCI_BAR0, 0x00, 0x00 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR1, 0x00, 0x00 },
	    },
	},

	/*
	 * Two 1-port and one 2-port found on a 4-port
	 * card sold as Sunsway/ST Lab I-430.
	 */
	{   "NetMos NM9865 1S",
	    {	PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9865, 0xa000, 0x1000 },
	    {	0xffff, 0xffff, 0xffff, 0xffff },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},
	{   "NetMos NM9865 2S",
	    {	PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9865, 0xa000, 0x3002 },
	    {	0xffff, 0xffff, 0xffff, 0xffff },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
	    },
	},

	/* NetMos PCIe Peripheral Controller :UART part */
	{   "NetMos NM9901 UART",
	    {   PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9901, 0xa000, 0x1000 },
	    {	0xffff,	0xffff,				      0xffff, 0xffff },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* NetMos PCIe NM9901 : 1P */
	{   "NetMos NM9901 LPT",
	    {	PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9901, 0xa000, 0x2000 },
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_LPT, PCI_BAR0, 0x00, 0x00 },
	    },
	},

	/* NetMos PCIe NM9904 (PCI multi function): 4S */
	{   "NetMos NM9904 UART",
	    {   PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9904, 0, 0 },
	    {	0xffff,	0xffff,				      0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* NetMos PCIe NM9922 (PCI multi function): 2S */
	{   "NetMos NM9922 UART",
	    {   PCI_VENDOR_NETMOS, PCI_PRODUCT_NETMOS_NM9922, 0, 0 },
	    {	0xffff,	0xffff,				      0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/*
	 * Boards with an Oxford Semiconductor chip.
	 *
	 * Oxford Semiconductor provides documentation for their chip at:
	 * <URL:http://www.plxtech.com/products/uart>
	 *
	 * As sold by Kouwell <URL:http://www.kouwell.com/>.
	 * I/O Flex PCI I/O Card Model-223 with 4 serial and 1 parallel ports.
	 */

	/* Oxford Semiconductor OXPCIe952 PCIe 1P */
	{   "Oxford Semiconductor OXPCIe952 LPT",
	    {	PCI_VENDOR_OXFORDSEMI, PCI_PRODUCT_OXFORDSEMI_OXPCIE952P,
		0, 0},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_LPT, PCI_BAR0, 0x00, 0x00 },
	    },
	},

	/* Oxford Semiconductor OXPCIe952 PCIe UARTs */
	{   "Oxford Semiconductor OXPCIe952 UART",
	    {	PCI_VENDOR_OXFORDSEMI, PCI_PRODUCT_OXFORDSEMI_OXPCIE952_0,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Oxford Semiconductor OXPCIe952 PCIe UARTs */
	{   "Oxford Semiconductor OXPCIe952 UART",
	    {	PCI_VENDOR_OXFORDSEMI, PCI_PRODUCT_OXFORDSEMI_OXPCIE952_1,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Oxford Semiconductor OXPCIe952 PCIe UARTs */
	{   "Oxford Semiconductor OXPCIe952 UARTs",
	    {	PCI_VENDOR_OXFORDSEMI, PCI_PRODUCT_OXFORDSEMI_OXPCIE952_2S,
		PCI_VENDOR_OXFORDSEMI, PCI_PRODUCT_OXFORDSEMI_OXPCIE952_2S },
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Oxford Semiconductor OXPCIe952 PCIe UARTs */
	{   "Oxford Semiconductor OXPCIe952 UART",
	    {	PCI_VENDOR_OXFORDSEMI, PCI_PRODUCT_OXFORDSEMI_OXPCIE952_2,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Oxford Semiconductor OXPCIe952 PCIe UARTs */
	{   "Oxford Semiconductor OXPCIe952 UART",
	    {	PCI_VENDOR_OXFORDSEMI, PCI_PRODUCT_OXFORDSEMI_OXPCIE952_3,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Oxford Semiconductor OXPCIe952 PCIe UARTs */
	{   "Oxford Semiconductor OXPCIe952 UART",
	    {	PCI_VENDOR_OXFORDSEMI, PCI_PRODUCT_OXFORDSEMI_OXPCIE952_4,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Oxford Semiconductor OXPCIe952 PCIe UARTs */
	{   "Oxford Semiconductor OXPCIe952 UART",
	    {	PCI_VENDOR_OXFORDSEMI, PCI_PRODUCT_OXFORDSEMI_OXPCIE952_5,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Oxford Semiconductor OXPCIe952 PCIe UARTs */
	{   "Oxford Semiconductor OXPCIe952 UART",
	    {	PCI_VENDOR_OXFORDSEMI, PCI_PRODUCT_OXFORDSEMI_OXPCIE952_6,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Oxford Semiconductor OXmPCI952 PCI UARTs */
	{   "Oxford Semiconductor OXmPCI952 UARTs",
	    {	PCI_VENDOR_OXFORDSEMI,	PCI_PRODUCT_OXFORDSEMI_EXSYS_EX41092,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 10 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 10 },
	    },
	},

	/* Oxford Semiconductor OXuPCI952 950 PCI UARTs */
	{   "Oxford Semiconductor OXuPCI952 UARTs",
	    {	PCI_VENDOR_OXFORDSEMI,	PCI_PRODUCT_OXFORDSEMI_OXUPCI952,
		0, 0 },
	    {	0xffff, 0xffff, 0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ * 8 },
	    },
	},

	/* Oxford Semiconductor OX16PCI952 PCI `950 UARTs - 128 byte FIFOs */
	{   "Oxford Semiconductor OX16PCI952 UARTs",
	    {   PCI_VENDOR_OXFORDSEMI, PCI_PRODUCT_OXFORDSEMI_OX16PCI952,
		0, 0 },
	    {   0xffff, 0xffff, 0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
	    },
	},

	/* Oxford Semiconductor OX16PCI952 PCI Parallel port */
	{   "Oxford Semiconductor OX16PCI952 Parallel port",
	    {   PCI_VENDOR_OXFORDSEMI, PCI_PRODUCT_OXFORDSEMI_OX16PCI952P,
		0, 0 },
	    {   0xffff, 0xffff, 0,	0	},
	    {
		{ PUC_PORT_TYPE_LPT, PCI_BAR0, 0x00, 0x00 },
	    },
	},

	/* InnoSys Keyspan SX Pro OX16PCI954 based 4 UARTs */
	{   "InnoSys Keyspan SX Pro Serial Card",
	    {	PCI_VENDOR_OXFORDSEMI,	PCI_PRODUCT_OXFORDSEMI_OX16PCI954,
		PCI_VENDOR_INNOSYS, 0x5850 },
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 8 },
	    },
	},

	/* I-O DATA RSA-PCI2 two UARTs based on OX16PCI954 */
	{   "I-O DATA RSA-PCI2 UARTs",
	    {	PCI_VENDOR_OXFORDSEMI,	PCI_PRODUCT_OXFORDSEMI_OX16PCI954,
		PCI_VENDOR_IODATA, 0xc070 },
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8 },
	    },
	},

	/* I-O DATA RSA-PCI2 four/eight(1-4) UARTs based on OX16PCI954 */
	{   "I-O DATA RSA-PCI2/P4 or P8 (1-4) UARTs",
	    {	PCI_VENDOR_OXFORDSEMI,	PCI_PRODUCT_OXFORDSEMI_OX16PCI954,
		PCI_VENDOR_IODATA, 0xd007 },
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 8 },
	    },
	},

	/* OEM of Oxford Semiconductor PCI UARTs? */
	{   "SIIG Cyber 4 PCI 16550",
	    {	PCI_VENDOR_OXFORDSEMI,	PCI_PRODUCT_OXFORDSEMI_OX16PCI954,
		PCI_VENDOR_SIIG, 0x2050	},
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 10 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 10 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 10 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 10 },
	    },
	},

	/* OEM of Oxford Semiconductor PCI UARTs? */
	{   "SIIG Cyber 4S PCI 16C650 (20x family)",
	    {	PCI_VENDOR_OXFORDSEMI,	PCI_PRODUCT_OXFORDSEMI_OX16PCI954,
		PCI_VENDOR_SIIG, 0x2051	},
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 10 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 10 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 10 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 10 },
	    },
	},

	/* OEM of Oxford Semiconductor PCI UARTs? */
	{   "Avlab LP PCI 4S Quartet",
	    {	PCI_VENDOR_OXFORDSEMI,	PCI_PRODUCT_OXFORDSEMI_OX16PCI954,
		PCI_VENDOR_AVLAB, 0x2150 },
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 10 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 10 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 10 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 10 },
	    },
	},

	/* Oxford Semiconductor OX16PCI954 PCI UARTs */
	{   "Oxford Semiconductor OX16PCI954 UARTs",
	    {	PCI_VENDOR_OXFORDSEMI,	PCI_PRODUCT_OXFORDSEMI_OX16PCI954,
		PCI_VENDOR_OXFORDSEMI,	0 },
	    {	0xffff,	0xffff,	0xffff,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 8},
	    },
	},

	/* Oxford Semiconductor OX16PCI954 PCI UARTs (default for 0x9501) */
	{   "Oxford Semiconductor OX16PCI954 UARTs",
	    {	PCI_VENDOR_OXFORDSEMI,	PCI_PRODUCT_OXFORDSEMI_OX16PCI954,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ},
	    },
	},

	/* I-O DATA RSA-PCI2 eight(5-8) UARTs base on OX16PCI954 */
	{   "I-O DATA RSA-PCI2/P8 (5-8) UARTs",
	    {	PCI_VENDOR_OXFORDSEMI,	PCI_PRODUCT_OXFORDSEMI_EXSYS_EX41098,
		PCI_VENDOR_IODATA, 0xd007 },
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 8 },
	    },
	},

	/* Exsys EX-41098, second part of SIIG Cyber 8S PCI Card */
	{   "Exsys EX-41098",
	    {	PCI_VENDOR_OXFORDSEMI,	PCI_PRODUCT_OXFORDSEMI_EXSYS_EX41098,
		PCI_VENDOR_SIIG, 0x2082	},
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 10},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 10},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 10},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 10},
	    },
	},

	/* Oxford Semiconductor OX16PCI954 PCI Parallel port */
	{   "Oxford Semiconductor OX16PCI954 Parallel port",
	    {	PCI_VENDOR_OXFORDSEMI,	PCI_PRODUCT_OXFORDSEMI_OX16PCI954P,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_LPT, PCI_BAR0, 0x00, 0x00 },
	    },
	},

	/* EXSYS EX-41098-2 UARTs */
	{   "EXSYS EX-41098-2 UARTs",
	    {	PCI_VENDOR_OXFORDSEMI,	PCI_PRODUCT_OXFORDSEMI_OX16PCI958,
		PCI_VENDOR_OXFORDSEMI, 0x0671 },
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x08, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x10, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x18, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x20, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x28, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x30, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x38, COM_FREQ},
	    },
	},

	/* Oxford Semiconductor OX16PCI958 UARTs (wildcard)*/
	{   "Oxford Semiconductor OX16PCI958 UARTs",
	    {	PCI_VENDOR_OXFORDSEMI,	PCI_PRODUCT_OXFORDSEMI_OX16PCI958,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 10},
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x08, COM_FREQ * 10},
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x10, COM_FREQ * 10},
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x18, COM_FREQ * 10},
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x20, COM_FREQ * 10},
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x28, COM_FREQ * 10},
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x30, COM_FREQ * 10},
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x38, COM_FREQ * 10},
	    },
	},

	{   "SUNIX 5008 1P",
	    {	PCI_VENDOR_SUNIX2,	PCI_PRODUCT_SUNIX2_SER5XXXX,
		0x1fd4,	0x0100 },
	    {	0xffff,	0xffff,	0xffff,	0xeff0 },
	    {
		{ PUC_PORT_TYPE_LPT, PCI_BAR1, 0x00, 0x00 },
	    },
	},

	{   "SUNIX 5016 8S",
	    {	PCI_VENDOR_SUNIX2,	PCI_PRODUCT_SUNIX2_SER5XXXX,
		0x1fd4,	0x0010 },
	    {	0xffff,	0xffff,	0xffff,	0xffff },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x08, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x10, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x18, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x20, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x28, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x30, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x38, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x40, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x48, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x50, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x58, COM_FREQ * 8},
	    },
	},

	{   "SUNIX 5027 1S",
	    {	PCI_VENDOR_SUNIX2,	PCI_PRODUCT_SUNIX2_SER5XXXX,
		0x1fd4,	0x0001 },
	    {	0xffff,	0xffff,	0xffff,	0xffff },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8},
	    },
	},

	{   "SUNIX 5037 2S",
	    {	PCI_VENDOR_SUNIX2,	PCI_PRODUCT_SUNIX2_SER5XXXX,
		0x1fd4,	0x0002 },
	    {	0xffff,	0xffff,	0xffff,	0xffff },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8},
	    },
	},

	{   "SUNIX 5056 4S",
	    {	PCI_VENDOR_SUNIX2,	PCI_PRODUCT_SUNIX2_SER5XXXX,
		0x1fd4,	0x0004 },
	    {	0xffff,	0xffff,	0xffff,	0xffff },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 8},
	    },
	},

	{   "SUNIX 5066 8S",
	    {	PCI_VENDOR_SUNIX2,	PCI_PRODUCT_SUNIX2_SER5XXXX,
		0x1fd4,	0x0008 },
	    {	0xffff,	0xffff,	0xffff,	0xffff },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x08, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x10, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x18, COM_FREQ * 8},
	    },
	},

	{   "SUNIX 5069 1S / 1P",
	    {	PCI_VENDOR_SUNIX2,	PCI_PRODUCT_SUNIX2_SER5XXXX,
		0x1fd4,	0x0101 },
	    {	0xffff,	0xffff,	0xffff,	0xeff0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8},
		{ PUC_PORT_TYPE_LPT, PCI_BAR1, 0x00, 0x00 },
	    },
	},

	{   "SUNIX 5079 2S / 1P",
	    {	PCI_VENDOR_SUNIX2,	PCI_PRODUCT_SUNIX2_SER5XXXX,
		0x1fd4,	0x0102 },
	    {	0xffff,	0xffff,	0xffff,	0xffff },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8},
		{ PUC_PORT_TYPE_LPT, PCI_BAR1, 0x00, 0x00 },
	    },
	},

	{   "SUNIX 5099 4S / 1P",
	    {	PCI_VENDOR_SUNIX2,	PCI_PRODUCT_SUNIX2_SER5XXXX,
		0x1fd4,	0x0104 },
	    {	0xffff,	0xffff,	0xffff,	0xffff },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 8},
		{ PUC_PORT_TYPE_LPT, PCI_BAR1, 0x00, 0x00 },
	    },
	},

	/*
	 * XXX no entry because I have no data:
	 * XXX Dolphin Peripherals 4006 (single parallel)
	 */

	/*
	 * Dolphin Peripherals 4014 (dual parallel port) card.  PLX 9050, with
	 * a seemingly-lame EEPROM setup that puts the Dolphin IDs
	 * into the subsystem fields, and claims that it's a
	 * network/misc (0x02/0x80) device.
	 */
	{   "Dolphin Peripherals 4014",
	    {	PCI_VENDOR_PLX,	PCI_PRODUCT_PLX_9050,	0xd84d,	0x6810	},
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_LPT, PCI_BAR4, 0x00, 0x00 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR5, 0x00, 0x00 },
	    },
	},

	/*
	 * XXX Dolphin Peripherals 4025 (single serial)
	 * (clashes with Dolphin Peripherals  4036 (2s variant)
	 */

	/*
	 * Dolphin Peripherals 4035 (dual serial port) card.  PLX 9050, with
	 * a seemingly-lame EEPROM setup that puts the Dolphin IDs
	 * into the subsystem fields, and claims that it's a
	 * network/misc (0x02/0x80) device.
	 */
	{   "Dolphin Peripherals 4035",
	    {	PCI_VENDOR_PLX,	PCI_PRODUCT_PLX_9050,	0xd84d,	0x6808	},
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ },
	    },
	},

	/*
	 * Nanjing QinHeng Electronics
	 * Products based on CH353 chip which can be
	 * configured to provide various combinations
	 * including 2 serial ports and a parallel port
	 * or 4 serial ports (using a CH432 parallel to
	 * 2 serial port converter. Product codes from
	 * documentation (and physical 2 port serial card)
	 */
	{   "Nanjing QinHeng Electronics CH352",
	    {	PCI_VENDOR_QINHENG, PCI_PRODUCT_QINHENG_CH352_2S,
		PCI_VENDOR_QINHENG, PCI_PRODUCT_QINHENG_CH352_2S },
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
	    },
	},

	{   "Nanjing QinHeng Electronics CH352",
	    {	PCI_VENDOR_QINHENG, PCI_PRODUCT_QINHENG_CH352_1S1P,
		PCI_VENDOR_QINHENG, PCI_PRODUCT_QINHENG_CH352_1S1P },
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, PCI_BAR1, 0x00, 0x00 },
	    },
	},

	{   "Nanjing QinHeng Electronics CH353",
	    {	PCI_VENDOR_QINHENG, PCI_PRODUCT_QINHENG_CH353_4S,
		PCI_VENDOR_QINHENG, PCI_PRODUCT_QINHENG_CH353_4S },
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ },
	    },
	},

	{   "Nanjing QinHeng Electronics CH353",
	    {	PCI_VENDOR_QINHENG, PCI_PRODUCT_QINHENG_CH353_2S1P,
		PCI_VENDOR_QINHENG, 0x3253 },
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, PCI_BAR2, 0x00, 0x00 },
	    },
	},

	{   "Nanjing QinHeng Electronics CH353 (fixed address)",
	    {	PCI_VENDOR_QINHENG, PCI_PRODUCT_QINHENG_CH353_2S1PAR,
		PCI_VENDOR_QINHENG, PCI_PRODUCT_QINHENG_CH353_2S1PAR },
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, PCI_BAR2, 0x00, 0x00 },
	    },
	},

	{   "Nanjing QinHeng Electronics CH355",
	    {	PCI_VENDOR_QINHENG, PCI_PRODUCT_QINHENG_CH355_4S,
		PCI_VENDOR_QINHENG, 0x3473 },
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ },
	    },
	},

	{   "Nanjing QinHeng Electronics CH356",
	    {	PCI_VENDOR_QINHENG, PCI_PRODUCT_QINHENG_CH356_4S1P,
		PCI_VENDOR_QINHENG, 0x3473 },
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, PCI_BAR4, 0x00, 0x00 },
	    },
	},

	{   "Nanjing QinHeng Electronics CH356",
	    {	PCI_VENDOR_QINHENG, PCI_PRODUCT_QINHENG_CH356_6S,
		PCI_VENDOR_QINHENG, PCI_PRODUCT_QINHENG_CH356_6S },
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x08, COM_FREQ },
	    },
	},

	{   "Nanjing QinHeng Electronics CH356",
	    {	PCI_VENDOR_QINHENG, PCI_PRODUCT_QINHENG_CH356_8S,
		PCI_VENDOR_QINHENG, PCI_PRODUCT_QINHENG_CH356_8S },
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x10, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x18, COM_FREQ },
	    },
	},

	{   "Nanjing QinHeng Electronics CH357",
	    {	PCI_VENDOR_QINHENG, PCI_PRODUCT_QINHENG_CH357_4S,
		PCI_VENDOR_QINHENG, 0x5053 },
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ },
	    },
	},

	{   "Nanjing QinHeng Electronics CH358",
	    {	PCI_VENDOR_QINHENG, PCI_PRODUCT_QINHENG_CH358_4S1P,
		PCI_VENDOR_QINHENG, PCI_PRODUCT_QINHENG_CH358_4S1P },
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, PCI_BAR4, 0x00, 0x00 },
	    },
	},

	{   "Nanjing QinHeng Electronics CH358",
	    {	PCI_VENDOR_QINHENG, PCI_PRODUCT_QINHENG_CH358_8S,
		PCI_VENDOR_QINHENG, PCI_PRODUCT_QINHENG_CH358_8S },
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x08, COM_FREQ },
	    },
	},

	{   "Nanjing QinHeng Electronics CH359",
	    {	PCI_VENDOR_QINHENG, PCI_PRODUCT_QINHENG_CH359_16S,
		PCI_VENDOR_QINHENG, PCI_PRODUCT_QINHENG_CH359_16S },
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x10, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x20, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x30, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x18, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x28, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x38, COM_FREQ },
	    },
	},

	{   "Nanjing QinHeng Electronics CH382",
	    {	PCI_VENDOR_QINHENG2, PCI_PRODUCT_QINHENG2_CH382_2S,
		PCI_VENDOR_QINHENG2, PCI_PRODUCT_QINHENG2_CH382_2S },
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0xc0, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0xc8, COM_FREQ },
	    },
	},

	{   "Nanjing QinHeng Electronics CH382",
	    {	PCI_VENDOR_QINHENG2, PCI_PRODUCT_QINHENG2_CH382_2S1P,
		PCI_VENDOR_QINHENG2, PCI_PRODUCT_QINHENG2_CH382_2S1P },
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0xc0, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0xc8, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, PCI_BAR1, 0x00, 0x00 },
	    },
	},

	{   "Nanjing QinHeng Electronics CH384",
	    {	PCI_VENDOR_QINHENG2, PCI_PRODUCT_QINHENG2_CH384_4S,
		PCI_VENDOR_QINHENG2, PCI_PRODUCT_QINHENG2_CH384_4S },
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0xc0, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0xc8, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0xd0, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0xd8, COM_FREQ },
	    },
	},

	{   "Nanjing QinHeng Electronics CH384",
	    {	PCI_VENDOR_QINHENG2, PCI_PRODUCT_QINHENG2_CH384_4S1P,
		PCI_VENDOR_QINHENG2, PCI_PRODUCT_QINHENG2_CH384_4S1P },
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0xc0, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0xc8, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0xd0, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0xd8, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, PCI_BAR1, 0x00, 0x00 },
	    },
	},

	{   "Nanjing QinHeng Electronics CH384",
	    {	PCI_VENDOR_QINHENG2, PCI_PRODUCT_QINHENG2_CH384_8S,
		PCI_VENDOR_QINHENG2, PCI_PRODUCT_QINHENG2_CH384_8S },
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x20, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x30, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x28, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x38, COM_FREQ },
	    },
	},

	{   "Nanjing QinHeng Electronics CH384",
	    {	PCI_VENDOR_QINHENG2, PCI_PRODUCT_QINHENG2_CH384_28S,
		PCI_VENDOR_QINHENG2, PCI_PRODUCT_QINHENG2_CH384_28S },
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0xc0, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0xc8, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0xd0, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0xd8, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x20, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x30, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x28, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x38, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x40, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x50, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x60, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x70, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x48, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x58, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x68, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x78, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x80, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x90, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0xa0, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0xb0, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x88, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x98, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0xa8, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0xb8, COM_FREQ },
	    },
	},

	/* Intel 82946GZ/GL KT */
	{   "Intel 82946GZ/GL KT",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82946GZ_KT, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Intel P965/G965 KT */
	{   "Intel P965/G965 KT",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82P965_KT, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Intel PM965/GM965 KT */
	{   "Intel PM965/GM965 KT",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82965PM_KT, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Intel GME965/GLE965 KT */
	{   "Intel GME965/GLE965 KT",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82965GME_KT, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Intel Q963/Q965 KT */
	{   "Intel Q963/Q965 KT",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q965_KT, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Intel G33/G31/P35/P31 KT */
	{   "Intel G33/G31/P35/P31 KT",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G33_KT, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Intel Q35 KT */
	{   "Intel Q35 KT",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q35_KT, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Intel Q33 KT */
	{   "Intel Q33 KT",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q33_KT, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Intel X38 KT */
	{   "Intel X38 KT",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82X38_KT, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Intel 3200 KT */
	{   "Intel 3200 KT",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_3200_KT, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Intel GM45 KT */
	{   "Intel GM45 KT",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GM45_KT, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Intel Q45 KT */
	{   "Intel Q45 KT",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q45_KT, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Intel Q45 KT (again) */
	{   "Intel Q45 KT",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q45_KT_1, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},
	/* Intel 5 Series and Intel 3400 Series KT */
	{   "Intel 5 Series KT",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_3400_KT, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Intel 6 Series KT */
	{   "Intel 6 Series KT",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_6SERIES_KT, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Intel 7 Series KT */
	{   "Intel 7 Series KT",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_7SERIES_KT, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Intel 8 Series KT */
	{   "Intel 8 Series KT",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_8SERIES_KT, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Intel 9 Series KT */
	{   "Intel 9 Series KT",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_9SERIES_KT, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Intel C600/X79 Series KT */
	{   "Intel C600/X79 Series KT",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_C600_KT, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Intel Core 4G (mobile) KT */
	{   "Intel Core 4G (mobile) KT",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE4G_M_KT, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Intel EG20T UART */
	{   "Intel EG20T UART #0",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_EG20T_UART_0, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Intel EG20T UART */
	{   "Intel EG20T UART #1",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_EG20T_UART_1, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Intel EG20T UART */
	{   "Intel EG20T UART #2",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_EG20T_UART_2, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Intel EG20T UART */
	{   "Intel EG20T UART #3",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_EG20T_UART_3, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* Intel Quark X1000 UART */
	{   "Intel Quark X1000 UART",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_X1000_HS_UART, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, 44236800 },
	    },
	},

	/* Intel S1200 UART */
	{   "Intel S1200 UART",
	    {	PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_S1200_UART, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	/* VScom PCI-200: 2S */
	{   "VScom PCI-200",
	    {	PCI_VENDOR_PLX,	PCI_PRODUCT_PLX_PCI_200,
		PCI_VENDOR_PLX,	0x1103 },
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ * 8 },
	    },
	},

	/* VScom PCI-400: 4S */
	{   "VScom PCI-400",
	    {	PCI_VENDOR_PLX,	PCI_PRODUCT_PLX_PCI_400,
		PCI_VENDOR_PLX,	0x1077	},
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x18, COM_FREQ * 8 },
	    },
	},

	/* VScom PCI-800: 8S */
	{   "VScom PCI-800",
	    {	PCI_VENDOR_PLX,	PCI_PRODUCT_PLX_PCI_800,
		PCI_VENDOR_PLX,	0x1076	},
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x18, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x20, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x28, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x30, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x38, COM_FREQ * 8 },
	    },
	},

	/*
	 * Perle PCI-RAS 4 Modem ports
	 */
	{   "Perle Systems PCI-RAS 4 modem ports",
	    {	PCI_VENDOR_PLX, PCI_PRODUCT_PLX_9030, 0x155f, 0xf001	},
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x08, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x10, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x18, COM_FREQ * 4 },
	    },
	},

	/*
	 * Perle PCI-RASV92 4 Modem ports
	 */
	{   "Perle Systems PCI-RASV92 4 modem ports",
	    {	PCI_VENDOR_PLX, PCI_PRODUCT_PLX_9050, 0x155f, 0xf001	},
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x08, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x10, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x18, COM_FREQ * 4 },
	    },
	},

	/*
	 * Perle PCI-RAS 8 Modem ports
	 */
	{   "Perle Systems PCI-RAS 8 modem ports",
	    {	PCI_VENDOR_PLX, PCI_PRODUCT_PLX_9030, 0x155f, 0xf010	},
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x08, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x10, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x18, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x20, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x28, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x30, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x38, COM_FREQ * 4 },
	    },
	},

	/*
	 * Perle PCI-RASV92 8 Modem ports
	 */
	{   "Perle Systems PCI-RASV92 8 modem ports",
	    {	PCI_VENDOR_PLX, PCI_PRODUCT_PLX_9050, 0x155f, 0xf010	},
	    {	0xffff, 0xffff, 0xffff, 0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x08, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x10, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x18, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x20, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x28, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x30, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x38, COM_FREQ * 4 },
	    },
	},

	/*
	 * Boca Research Turbo Serial 654 (4 serial port) card.
	 * Appears to be the same as Chase Research PLC PCI-FAST4
	 * and Perle PCI-FAST4 Multi-Port serial cards.
	 */
	{   "Boca Research Turbo Serial 654",
	    {   PCI_VENDOR_PLX, PCI_PRODUCT_PLX_9050, 0x12e0, 0x0031  },
	    {   0xffff, 0xffff, 0xffff, 0xffff  },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x08, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x10, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x18, COM_FREQ * 4 },
	    },
	},

	/*
	 * Boca Research Turbo Serial 658 (8 serial port) card.
	 * Appears to be the same as Chase Research PLC PCI-FAST8
	 * and Perle PCI-FAST8 Multi-Port serial cards.
	 */
	{   "Boca Research Turbo Serial 658",
	    {   PCI_VENDOR_PLX, PCI_PRODUCT_PLX_9050, 0x12e0, 0x0021  },
	    {   0xffff, 0xffff, 0xffff, 0xffff  },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x08, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x10, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x18, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x20, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x28, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x30, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x38, COM_FREQ * 4 },
	    },
	},

	/*
	 * SIIG Boards.
	 *
	 * SIIG provides documentation for their boards at:
	 * <URL:http://www.siig.com/driver.htm>
	 *
	 * Please excuse the weird ordering, it's the order they
	 * use in their documentation.
	 */

	/*
	 * SIIG "10x" family boards.
	 */

	/* SIIG Cyber Serial PCI 16C550 (10x family): 1S */
	{   "SIIG Cyber Serial PCI 16C550 (10x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER10_S550, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG10x|PUC_PORT_USR1 },
	    },
	},

	/* SIIG Cyber Serial PCI 16C650 (10x family): 1S */
	{   "SIIG Cyber Serial PCI 16C650 (10x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER10_S650, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG10x|PUC_PORT_USR1 },
	    },
	},

	/* SIIG Cyber Serial PCI 16C850 (10x family): 1S */
	{   "SIIG Cyber Serial PCI 16C850 (10x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER10_S850, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG10x|PUC_PORT_USR1 },
	    },
	},

	/* SIIG Cyber I/O PCI 16C550 (10x family): 1S, 1P */
	{   "SIIG Cyber I/O PCI 16C550 (10x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER10_IO550, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG10x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR3, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber I/O PCI 16C650 (10x family): 1S, 1P */
	{   "SIIG Cyber I/O PCI 16C650 (10x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER10_IO650, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG10x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR3, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber I/O PCI 16C850 (10x family): 1S, 1P */
	{   "SIIG Cyber I/O PCI 16C850 (10x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER10_IO850, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG10x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR3, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber Parallel PCI (10x family): 1P */
	{   "SIIG Cyber Parallel PCI (10x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER10_P, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_LPT, PCI_BAR2, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber Parallel Dual PCI (10x family): 2P */
	{   "SIIG Cyber Parallel Dual PCI (10x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER10_2P, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_LPT, PCI_BAR2, 0x00, 0x00 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR4, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C550 (10x family): 2S */
	{   "SIIG Cyber Serial Dual PCI 16C550 (10x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER10_2S550, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG10x|PUC_PORT_USR2 },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG10x|PUC_PORT_USR3 },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C650 (10x family): 2S */
	{   "SIIG Cyber Serial Dual PCI 16C650 (10x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER10_2S650, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG10x|PUC_PORT_USR2 },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG10x|PUC_PORT_USR3 },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C850 (10x family): 2S */
	{   "SIIG Cyber Serial Dual PCI 16C850 (10x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER10_2S850, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG10x|PUC_PORT_USR2 },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG10x|PUC_PORT_USR3 },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C550 (10x family): 2S, 1P */
	{   "SIIG Cyber 2S1P PCI 16C550 (10x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER10_2S1P550,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG10x|PUC_PORT_USR2 },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG10x|PUC_PORT_USR3 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR4, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C650 (10x family): 2S, 1P */
	{   "SIIG Cyber 2S1P PCI 16C650 (10x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER10_2S1P650,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG10x|PUC_PORT_USR2 },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG10x|PUC_PORT_USR3 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR4, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C850 (10x family): 2S, 1P */
	{   "SIIG Cyber 2S1P PCI 16C850 (10x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER10_2S1P850,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG10x|PUC_PORT_USR2 },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG10x|PUC_PORT_USR3 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR4, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber 4S PCI 16C550 (10x family): 4S */
	{   "SIIG Cyber 4S PCI 16C550 (10x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER10_4S550, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG10x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR5, 0x00, COM_FREQ * 8 },
	    },
	},

	/* SIIG Cyber 4S PCI 16C650 (10x family): 4S */
	{   "SIIG Cyber 4S PCI 16C650 (10x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER10_4S650, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG10x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR5, 0x00, COM_FREQ * 8 },
	    },
	},

	/* SIIG Cyber 4S PCI 16C850 (10x family): 4S */
	{   "SIIG Cyber 4S PCI 16C850 (10x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER10_4S850, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG10x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR5, 0x00, COM_FREQ * 8 },
	    },
	},

	/*
	 * SIIG "20x" family boards.
	 */

	/* SIIG Cyber Serial PCI 16C550 (20x family): 1S */
	{   "SIIG Cyber Serial PCI 16C550 (20x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER20_S550, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR0 },
	    },
	},

	/* SIIG Cyber Serial PCI 16C650 (20x family): 1S */
	{   "SIIG Cyber Serial PCI 16C650 (20x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER20_S650, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR0 },
	    },
	},

	/* SIIG Cyber Serial PCI 16C850 (20x family): 1S */
	{   "SIIG Cyber Serial PCI 16C850 (20x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER20_S850, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR0 },
	    },
	},

	/* SIIG Cyber I/O PCI 16C550 (20x family): 1S, 1P */
	{   "SIIG Cyber I/O PCI 16C550 (20x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER20_IO550, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR1, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber I/O PCI 16C650 (20x family): 1S, 1P */
	{   "SIIG Cyber I/O PCI 16C650 (20x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER20_IO650, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR1, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber I/O PCI 16C850 (20x family): 1S, 1P */
	{   "SIIG Cyber I/O PCI 16C850 (20x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER20_IO850, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR1, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber Parallel PCI (20x family): 1P */
	{   "SIIG Cyber Parallel PCI (20x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER20_P, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_LPT, PCI_BAR0, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber Parallel Dual PCI (20x family): 2P */
	{   "SIIG Cyber Parallel Dual PCI (20x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER20_2P, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_LPT, PCI_BAR0, 0x00, 0x00 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR2, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C550 (20x family): 2S */
	{   "SIIG Cyber Serial Dual PCI 16C550 (20x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER20_2S550, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR1 },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C650 (20x family): 2S */
	{   "SIIG Cyber Serial Dual PCI 16C650 (20x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER20_2S650, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR1 },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C850 (20x family): 2S */
	{   "SIIG Cyber Serial Dual PCI 16C850 (20x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER20_2S850, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR1 },
	    },
	},

	/* SIIG Cyber 2P1S PCI 16C550 (20x family): 1S, 2P */
	{   "SIIG Cyber 2P1S PCI 16C550 (20x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER20_2P1S550,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR1, 0x00, 0x00 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR3, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber 2P1S PCI 16C650 (20x family): 1S, 2P */
	{   "SIIG Cyber 2P1S PCI 16C650 (20x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER20_2P1S650,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR1, 0x00, 0x00 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR3, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber 2P1S PCI 16C850 (20x family): 1S, 2P */
	{   "SIIG Cyber 2P1S PCI 16C850 (20x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER20_2P1S850,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR1, 0x00, 0x00 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR3, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber 4S PCI 16C550 (20x family): 4S */
	{   "SIIG Cyber 4S PCI 16C550 (20x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER20_4S550, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ * 8 },
	    },
	},

	/* SIIG Cyber 4S PCI 16C650 (20x family): 4S */
	{   "SIIG Cyber 4S PCI 16C650 (20x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER20_4S650, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ * 8 },
	    },
	},

	/* SIIG Cyber 4S PCI 16C850 (20x family): 4S */
	{   "SIIG Cyber 4S PCI 16C850 (20x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER20_4S850, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ * 8 },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C550 (20x family): 2S, 1P */
	{   "SIIG Cyber 2S1P PCI 16C550 (20x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER20_2S1P550,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR1 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR2, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C650 (20x family): 2S, 1P */
	{   "SIIG Cyber 2S1P PCI 16C650 (20x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER20_2S1P650,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR1 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR2, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C850 (20x family): 2S, 1P */
	{   "SIIG Cyber 2S1P PCI 16C850 (20x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_CYBER20_2S1P850,
		0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR1 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR2, 0x00, 0x00 },
	    },
	},

	/* SIIG PS8000 PCI 8S 16C550 (20x family): 8S - 16 Byte FIFOs */
	{   "SIIG PS8000 PCI 8S 16C550 (20x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_PS8000P550, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x18, COM_FREQ * 8 },
	    },
	},

	/* SIIG PS8000 PCI 8S 16C650 (20x family): 8S - 32 Byte FIFOs */
	{   "SIIG PS8000 PCI 8S 16C650 (20x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_PS8000P650, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x18, COM_FREQ * 8 },
	    },
	},

	/* SIIG PS8000 PCI 8S 16C850 (20x family): 8S - 128 Byte FIFOs */
	{   "SIIG PS8000 PCI 8S 16C850 (20x family)",
	    {	PCI_VENDOR_SIIG,	PCI_PRODUCT_SIIG_PS8000P850, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00,
		    (COM_FREQ * 8)|PUC_COM_SIIG20x|PUC_PORT_USR0 },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x18, COM_FREQ * 8 },
	    },
	},

	/*
	 * SUNIX 40XX series of serial/parallel combo cards.
	 * Tested with 4055A and 4065A.
	 */
	{   "SUNIX 400X 1P",
	    {	PCI_VENDOR_SUNIX, PCI_PRODUCT_SUNIX_PCI2S550,
		PCI_VENDOR_SUNIX, 0x4000 },
	    {	0xffff,	0xffff,	0xffff,	0xeff0 },
	    {
		{ PUC_PORT_TYPE_LPT, PCI_BAR0, 0x00, 0x00 },
	    },
	},

	{   "SUNIX 401X 2P",
	    {	PCI_VENDOR_SUNIX, PCI_PRODUCT_SUNIX_PCI2S550,
		PCI_VENDOR_SUNIX, 0x4010 },
	    {	0xffff,	0xffff,	0xffff,	0xeff0 },
	    {
		{ PUC_PORT_TYPE_LPT, PCI_BAR0, 0x00, 0x00 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR2, 0x00, 0x00 },
	    },
	},

	{   "SUNIX 402X 1S",
	    {	PCI_VENDOR_SUNIX, PCI_PRODUCT_SUNIX_PCI2S550,
		PCI_VENDOR_SUNIX, 0x4020 },
	    {	0xffff,	0xffff,	0xffff,	0xeff0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8},
	    },
	},

	{   "SUNIX 403X 2S",
	    {	PCI_VENDOR_SUNIX, PCI_PRODUCT_SUNIX_PCI2S550,
		PCI_VENDOR_SUNIX, 0x4030 },
	    {	0xffff,	0xffff,	0xffff,	0xeff0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8},
	    },
	},

	{   "SUNIX 4036 2S",
	    {	PCI_VENDOR_SUNIX, PCI_PRODUCT_SUNIX_PCI2S550,
		PCI_VENDOR_SUNIX, 0x0002 },
	    {	0xffff,	0xffff,	0xffff,	0xeff0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8},
	    },
	},

	{   "SUNIX 405X 4S",
	    {	PCI_VENDOR_SUNIX, PCI_PRODUCT_SUNIX_PCI2S550,
		PCI_VENDOR_SUNIX, 0x4050 },
	    {	0xffff,	0xffff,	0xffff,	0xeff0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x08, COM_FREQ},
	    },
	},

	{   "SUNIX 406X 8S",
	    {	PCI_VENDOR_SUNIX, PCI_PRODUCT_SUNIX_PCI2S550,
		PCI_VENDOR_SUNIX, 0x4060 },
	    {	0xffff,	0xffff,	0xffff,	0xeff0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x08, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR3, 0x00, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x00, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR5, 0x00, COM_FREQ},
	    },
	},

	{   "SUNIX 407X 2S/1P",
	    {	PCI_VENDOR_SUNIX, PCI_PRODUCT_SUNIX_PCI2S550,
		PCI_VENDOR_SUNIX, 0x4070 },
	    {	0xffff,	0xffff,	0xffff,	0xeff0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8},
		{ PUC_PORT_TYPE_LPT, PCI_BAR2, 0x00, 0x00 },
	    },
	},

	{   "SUNIX 408X 2S/2P",
	    {	PCI_VENDOR_SUNIX, PCI_PRODUCT_SUNIX_PCI2S550,
		PCI_VENDOR_SUNIX, 0x4080 },
	    {	0xffff,	0xffff,	0xffff,	0xeff0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8},
		{ PUC_PORT_TYPE_LPT, PCI_BAR2, 0x00, 0x00 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR4, 0x00, 0x00 },
	    },
	},

	{   "SUNIX 409X 4S/2P",
	    {	PCI_VENDOR_SUNIX, PCI_PRODUCT_SUNIX_PCI2S550,
		PCI_VENDOR_SUNIX, 0x4090 },
	    {	0xffff,	0xffff,	0xffff,	0xeff0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ},
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x08, COM_FREQ},
		{ PUC_PORT_TYPE_LPT, PCI_BAR2, 0x00, 0x00 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR4, 0x00, 0x00 },
	    },
	},

	/*
	 * Dolphin Peripherals 4036 (dual serial port) card.
	 * (Dolpin 4025 has the same ID but only one port)
	 */
	{   "Dolphin Peripherals 4036",
	    {	PCI_VENDOR_SUNIX, PCI_PRODUCT_SUNIX_PCI2S550,
		0x0,	0x0	},
	    {	0xffff,	0xffff,	0x0,	0x0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8},
	    },
	},

	/*
	 * XXX no entry because I have no data:
	 * XXX Dolphin Peripherals 4078 (dual serial and single parallel)
	 */

	/* SD-LAB PCI I/O Card 4S */
	{   "Syba Tech Ltd. PCI-4S",
	    {   PCI_VENDOR_SYBA, PCI_PRODUCT_SYBA_4S,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x3e8, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x2e8, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x3f8, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x2f8, COM_FREQ },
	    },
	},

	/* SD-LAB PCI I/O Card 4S2P */
	{   "Syba Tech Ltd. PCI-4S2P-550-ECP",
	    {   PCI_VENDOR_SYBA, PCI_PRODUCT_SYBA_4S2P,		0, 0	},
	    {	0xffff,	0xffff,					0, 0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x2e8, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x2f8, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, PCI_BAR0, 0x000, 0x00 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x3e8, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x3f8, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, PCI_BAR0, 0x000, 0x00 },
	    },
	},

	/* SystemBase SB16C1050 UARTs */
	{   "SystemBase SB16C1050",
	    {	PCI_VENDOR_SYSTEMBASE, PCI_PRODUCT_SYSTEMBASE_SB16C1050, 0, 0 },
	    {	0xffff, 0xffff,						 0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8},
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8},
	    },
	},

	/* SystemBase SB16C1054 UARTs */
	{   "SystemBase SB16C1054",
	    {	PCI_VENDOR_SYSTEMBASE, PCI_PRODUCT_SYSTEMBASE_SB16C1054, 0, 0 },
	    {	0xffff,	0xffff,						 0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ },
	    },
	},

	/* SystemBase SB16C1058 UARTs */
	{   "SystemBase SB16C1058",
	    {   PCI_VENDOR_SYSTEMBASE, PCI_PRODUCT_SYSTEMBASE_SB16C1058, 0, 0 },
	    {	0xffff,	0xffff,						 0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x20, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x28, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x30, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x38, COM_FREQ },
	    },
	},

	/*
	 * VScom PCI 010L
	 * one lpt
	 * untested
	 */
	{   "VScom PCI-010L",
	    {	PCI_VENDOR_TITAN, PCI_PRODUCT_TITAN_VSCOM_PCI010L,    0, 0 },
	    {	0xffff, 0xffff,					0, 0 },
	    {
		{ PUC_PORT_TYPE_LPT, PCI_BAR3, 0x00, 0x00 },
	    },
	},

	/*
	 * VScom PCI 100L
	 * one com
	 * The one I have defaults to a fequency of 14.7456 MHz which is
	 * jumper J1 set to 2-3.
	 */
	{   "VScom PCI-100L",
	    {	PCI_VENDOR_TITAN, PCI_PRODUCT_TITAN_VSCOM_PCI100L,	0, 0 },
	    {	0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ * 8 },
	    },
	},

	/*
	 * VScom PCI 110L
	 * one com, one lpt
	 * untested
	 */
	{   "VScom PCI-110L",
	    {	PCI_VENDOR_TITAN, PCI_PRODUCT_TITAN_VSCOM_PCI110L,	0, 0 },
	    {	0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR3, 0x00, 0x00 },
	    },
	},

	/*
	 * VScom PCI-200L has 2 x 16550 UARTS.
	 * The board has a jumper which allows you to select a clock speed
	 * of either 14.7456MHz or 1.8432MHz. By default it runs at
	 * the fast speed.
	 */
	{   "VScom PCI-200L with 2 x 16550 UARTS",
	    {	PCI_VENDOR_TITAN, PCI_PRODUCT_TITAN_VSCOM_PCI200L,	0, 0 },
	    {	0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 8 },
	    },
	},

	/*
	 * VScom PCI-210L
	 * Has a jumper for frequency selection, defaults to 8x as used here
	 * two com, one lpt
	 */
	{   "VScom PCI-210L",
	    {	PCI_VENDOR_TITAN, PCI_PRODUCT_TITAN_VSCOM_PCI210L,	0, 0 },
	    {	0xffff, 0xffff,					0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_LPT, PCI_BAR3, 0x00, 0x00 },
	    },
	},

	/* VScom PCI-200Li */
	{   "VScom PCI-200Li",
	    {	PCI_VENDOR_TITAN, PCI_PRODUCT_TITAN_VSCOM_PCI200LI,	0, 0 },
	    {	0xffff, 0xffff,						0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x08, COM_FREQ },
	    },
	},

	/* PCI-400L: VendorID is reported to be 0x10d2 instead of 0x14d2. */
	{   "VScom PCI-400L",
	    {	PCI_VENDOR_MOLEX, PCI_PRODUCT_MOLEX_VSCOM_PCI400L,	0, 0 },
	    {	0xffff, 0xffff,					0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x08, COM_FREQ * 8 },
	    },
	},

	{   "VScom PCI-800L",
	    {	PCI_VENDOR_TITAN, PCI_PRODUCT_TITAN_VSCOM_PCI800L,	0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR2, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x18, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x20, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR4, 0x28, COM_FREQ * 8 },
	    },
	},

	{   "VScom PCI-011H",
	    {	PCI_VENDOR_TITAN, PCI_PRODUCT_TITAN_VSCOM_PCI011H,	0, 0 },
	    {	0xffff, 0xffff,					0, 0 },
	    {
		{ PUC_PORT_TYPE_LPT, PCI_BAR0, 0x00, 0x00 },
	    },
	},

	/*
	 * VScom PCI x10H, 1 lpt.
	 * is the lpt part of VScom 110H, 210H, 410H
	 */
	{   "VScom PCI-x10H",
	    {	PCI_VENDOR_TITAN, PCI_PRODUCT_TITAN_VSCOM_PCIx10H,	0, 0 },
	    {	0xffff, 0xffff,					0, 0 },
	    {
		{ PUC_PORT_TYPE_LPT, PCI_BAR0, 0x00, 0x00 },
	    },
	},

	/*
	 * VScom PCI 100H, little sister of 800H, 1 com.
	 * also com part of VScom 110H
	 * The one I have defaults to a fequency of 14.7456 MHz which is
	 * jumper J1 set to 2-3.
	 */
	{   "VScom PCI-100H",
	    {	PCI_VENDOR_TITAN, PCI_PRODUCT_TITAN_VSCOM_PCI100H,	0, 0 },
	    {	0xffff, 0xffff,					0, 0 },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8 },
	    },
	},

	/*
	 * VScom PCI-800H. Uses 8 16950 UART, behind a PCI chips that offers
	 * 4 com port on PCI device 0 and 4 on PCI device 1. PCI device 0 has
	 * device ID 3 and PCI device 1 device ID 4.
	 */
	{   "VScom PCI-800H",
	    {	PCI_VENDOR_TITAN, PCI_PRODUCT_TITAN_VSCOM_PCI800H,	0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 8 },
	    },
	},
	{   "VScom PCI-800H",
	    {	PCI_VENDOR_TITAN, PCI_PRODUCT_TITAN_VSCOM_PCI800H_1,	0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x18, COM_FREQ * 8 },
	    },
	},
        {   "VScom PCI-200H",
	    {   PCI_VENDOR_TITAN, PCI_PRODUCT_TITAN_VSCOM_PCI200H, 0, 0 },
            {   0xffff, 0xffff, 0,      0       },
            {
                { PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8 },
                { PUC_PORT_TYPE_COM, PCI_BAR0, 0x08, COM_FREQ * 8 },
            },
        },

	{   "VScom PCI-010HV2",
	    {   PCI_VENDOR_TITAN, PCI_PRODUCT_TITAN_VSCOM_PCI010HV2,	0, 0 },
	    {   0xffff, 0xffff, 0,      0       },
	    {
		{ PUC_PORT_TYPE_LPT, PCI_BAR0, 0x00, 0x00 },
	    },
	},
	{   "VScom PCI-200HV2",
	    {   PCI_VENDOR_TITAN, PCI_PRODUCT_TITAN_VSCOM_PCI200HV2,	0, 0 },
	    {   0xffff, 0xffff, 0,      0       },
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, PCI_BAR1, 0x00, COM_FREQ * 8 },
	    },
	},

	/* US Robotics (3Com) PCI Modems */
	{   "US Robotics (3Com) 3CP5609 PCI 16550 Modem",
	    {	PCI_VENDOR_USR,	PCI_PRODUCT_USR_3CP5609, 0, 0 },
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, PCI_BAR0, 0x00, COM_FREQ },
	    },
	},

	{ .name = NULL },
};

/*	$NetBSD: gt.c,v 1.27 2015/07/11 10:32:46 kamil Exp $	*/

/*
 * Copyright (c) 2002 Allegro Networks, Inc., Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Allegro Networks, Inc., and Wasabi Systems, Inc.
 * 4. The name of Allegro Networks, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 * 5. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ALLEGRO NETWORKS, INC. AND
 * WASABI SYSTEMS, INC. ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL EITHER ALLEGRO NETWORKS, INC. OR WASABI SYSTEMS, INC.
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * gt.c -- GT system controller driver
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: gt.c,v 1.27 2015/07/11 10:32:46 kamil Exp $");

#include "opt_marvell.h"
#include "gtmpsc.h"
#include "opt_multiprocessor.h"
#include "locators.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/types.h>

#include <dev/marvell/gtintrreg.h>
#include <dev/marvell/gtsdmareg.h>
#if NGTMPSC > 0
#include <dev/marvell/gtmpscreg.h>
#include <dev/marvell/gtmpscvar.h>
#endif
#include <dev/marvell/gtpcireg.h>
#include <dev/marvell/gtreg.h>
#include <dev/marvell/gtvar.h>
#include <dev/marvell/marvellreg.h>
#include <dev/marvell/marvellvar.h>

#include <dev/pci/pcireg.h>

#if ((GT_MPP_WATCHDOG & 0xf0f0f0f0) != 0)
# error		/* unqualified: configuration botch! */
#endif

#define gt_read(sc,r)	 bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (r))
#define gt_write(sc,r,v) bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (r), (v))


static int	gt_cfprint(void *, const char *);
static int	gt_cfsearch(device_t, cfdata_t, const int *, void *);
static void	gt_attach_peripherals(struct gt_softc *);

#ifdef GT_DEVBUS
static int	gt_devbus_intr(void *);
static void	gt_devbus_intr_enb(struct gt_softc *);
#endif
#ifdef GT_ECC
static int	gt_ecc_intr(void *);
static void	gt_ecc_intr_enb(struct gt_softc *);
#endif
#if NGTMPSC > 0
static void	gt_sdma_intr_enb(struct gt_softc *);
#endif
#ifdef GT_COMM
static int	gt_comm_intr(void *);
static void	gt_comm_intr_enb(struct gt_softc *);
#endif


#ifdef GT_WATCHDOG
static void gt_watchdog_init(struct gt_softc *);
static void gt_watchdog_enable(struct gt_softc *);
#ifndef GT_MPP_WATCHDOG
static void gt_watchdog_disable(struct gt_softc *);
#endif

static struct gt_softc *gt_watchdog_sc = NULL;
static int gt_watchdog_state = 0;
#endif


#define OFFSET_DEFAULT	MVA_OFFSET_DEFAULT
#define IRQ_DEFAULT	MVA_IRQ_DEFAULT
static const struct gt_dev {
	int model;
	const char *name;
	int unit;
	bus_size_t offset;
	int irq;
} gt_devs[] = {
	{ MARVELL_DISCOVERY,	"gfec",    0,	0x0000,		IRQ_DEFAULT },
	{ MARVELL_DISCOVERY,	"gtidmac", 0,	0x0000,		4 /*...7 */ },
	{ MARVELL_DISCOVERY,	"gtmpsc",  0,	0x8000,		40 },
	{ MARVELL_DISCOVERY,	"gtmpsc",  1,	0x9000,		42 },
	{ MARVELL_DISCOVERY,	"gtpci",   0,	OFFSET_DEFAULT,	IRQ_DEFAULT },
	{ MARVELL_DISCOVERY,	"gtpci",   1,	OFFSET_DEFAULT,	IRQ_DEFAULT },
	{ MARVELL_DISCOVERY,	"gttwsi",  0,	0xc000,		37 },
	{ MARVELL_DISCOVERY,	"obio",    0,	OFFSET_DEFAULT,	IRQ_DEFAULT },
	{ MARVELL_DISCOVERY,	"obio",    1,	OFFSET_DEFAULT,	IRQ_DEFAULT },
	{ MARVELL_DISCOVERY,	"obio",    2,	OFFSET_DEFAULT,	IRQ_DEFAULT },
	{ MARVELL_DISCOVERY,	"obio",    3,	OFFSET_DEFAULT,	IRQ_DEFAULT },
	{ MARVELL_DISCOVERY,	"obio",    4,	OFFSET_DEFAULT,	IRQ_DEFAULT },

	{ MARVELL_DISCOVERY_II,	"gtidmac", 0,	0x0000,		4 /*...7 */ },
	{ MARVELL_DISCOVERY_II,	"gtmpsc",  0,	0x8000,		40 },
	{ MARVELL_DISCOVERY_II,	"gtmpsc",  1,	0x9000,		42 },
	{ MARVELL_DISCOVERY_II,	"gtpci",   0,	OFFSET_DEFAULT,	IRQ_DEFAULT },
	{ MARVELL_DISCOVERY_II,	"gtpci",   1,	OFFSET_DEFAULT,	IRQ_DEFAULT },
	{ MARVELL_DISCOVERY_II,	"gttwsi",  0,	0xc000,		37 },
	{ MARVELL_DISCOVERY_II,	"mvgbec",  0,	0x0000,		IRQ_DEFAULT },

	{ MARVELL_DISCOVERY_III,"gtidmac", 0,	0x0000,		4 /*...7 */ },
	{ MARVELL_DISCOVERY_III,"gtmpsc",  0,	0x8000,		40 },
	{ MARVELL_DISCOVERY_III,"gtmpsc",  1,	0x9000,		42 },
	{ MARVELL_DISCOVERY_III,"gtpci",   0,	OFFSET_DEFAULT,	IRQ_DEFAULT },
	{ MARVELL_DISCOVERY_III,"gtpci",   1,	OFFSET_DEFAULT,	IRQ_DEFAULT },
	{ MARVELL_DISCOVERY_III,"gttwsi",  0,	0xc000,		37 },
	{ MARVELL_DISCOVERY_III,"mvgbec",  0,	0x0000,		IRQ_DEFAULT },

#if 0	/* XXXXXX: from www.marvell.com */
	/* Discovery LT (Discovery Light) MV644[23]0 */
	{ MARVELL_DISCOVERY_LT,	"gtidmac", 0,	0x?000,		? /*...? */ },
	{ MARVELL_DISCOVERY_LT,	"gtmpsc",  0,	0x?000,		? },
	{ MARVELL_DISCOVERY_LT,	"gtmpsc",  1,	0x?000,		? },
	{ MARVELL_DISCOVERY_LT,	"gtpci",   0,	OFFSET_DEFAULT,	IRQ_DEFAULT },
	{ MARVELL_DISCOVERY_LT,	"gtpci",   1,	OFFSET_DEFAULT,	IRQ_DEFAULT },
	{ MARVELL_DISCOVERY_LT,	"gttwsi",  0,	0x?000,		? },
	{ MARVELL_DISCOVERY_LT,	"mvgbec",  0,	0x?000,		IRQ_DEFAULT },

	/* Discovery V MV64560 */
	{ MARVELL_DISCOVERY_V,	"com",     ?,	0x?0000,	? },
	{ MARVELL_DISCOVERY_V,	"ehci",    0,	0x?0000,	? },
	{ MARVELL_DISCOVERY_V,	"ehci",    1,	0x?0000,	? },
	{ MARVELL_DISCOVERY_V,	"gtidmac", 0,	0x?0000,	? /*...? */ },
	{ MARVELL_DISCOVERY_V,	"gtpci",   0,	0x?0000,	IRQ_DEFAULT },
	{ MARVELL_DISCOVERY_V,	"gttwsi",  0,	0x?0000,	? },
	{ MARVELL_DISCOVERY_V,	"mvgbec",  0,	0x?0000,	IRQ_DEFAULT },
	{ MARVELL_DISCOVERY_V,	"mvpex or gtpci?", 0, 0x?0000,	IRQ_DEFAULT },
	{ MARVELL_DISCOVERY_V,	"obio",    0,	OFFSET_DEFAULT,	IRQ_DEFAULT },

	/* Discovery VI MV64660 */
	/* MV64560 + SATA? */
	{ MARVELL_DISCOVERY_VI, "mvsata",  0,	0x?0000,	? },
#endif
};


static int
gt_cfprint(void *aux, const char *pnp)
{
	struct marvell_attach_args *mva = aux;

	if (pnp)
		aprint_normal("%s at %s unit %d",
		    mva->mva_name, pnp, mva->mva_unit);
	else {
		if (mva->mva_unit != MVA_UNIT_DEFAULT)
			aprint_normal(" unit %d", mva->mva_unit);
		if (mva->mva_offset != MVA_OFFSET_DEFAULT) {
			aprint_normal(" offset 0x%04x", mva->mva_offset);
			if (mva->mva_size > 0)
				aprint_normal("-0x%04x",
				    mva->mva_offset + mva->mva_size - 1);
		}
		if (mva->mva_irq != MVA_IRQ_DEFAULT)
			aprint_normal(" irq %d", mva->mva_irq);
	}

	return UNCONF;
}


/* ARGSUSED */
static int
gt_cfsearch(device_t parent, cfdata_t cf, const int *ldesc, void *aux)
{
	struct marvell_attach_args *mva = aux;

	if (cf->cf_loc[GTCF_IRQ] != MVA_IRQ_DEFAULT)
		mva->mva_irq = cf->cf_loc[GTCF_IRQ];

	return config_match(parent, cf, aux);
}

static void
gt_attach_peripherals(struct gt_softc *sc)
{
	struct marvell_attach_args mva;
	int i;

	for (i = 0; i < __arraycount(gt_devs); i++) {
		if (gt_devs[i].model != sc->sc_model)
			continue;

		mva.mva_name = gt_devs[i].name;
		mva.mva_model = sc->sc_model;
		mva.mva_revision = sc->sc_rev;
		mva.mva_iot = sc->sc_iot;
		mva.mva_ioh = sc->sc_ioh;
		mva.mva_unit = gt_devs[i].unit;
		mva.mva_addr = sc->sc_addr;
		mva.mva_offset = gt_devs[i].offset;
		mva.mva_size = 0;
		mva.mva_dmat = sc->sc_dmat;
		mva.mva_irq = gt_devs[i].irq;

		config_found_sm_loc(sc->sc_dev, "gt", NULL, &mva,
		    gt_cfprint, gt_cfsearch);
	}
}

void
gt_attach_common(struct gt_softc *gt)
{
	uint32_t cpucfg, cpumode, cpumstr;
#ifdef GT_DEBUG
	uint32_t loaddr, hiaddr;
#endif

	gt_write(gt, GTPCI_CA(0), PCI_ID_REG);
	gt->sc_model = PCI_PRODUCT(gt_read(gt, GTPCI_CD(0)));
	gt_write(gt, GTPCI_CA(0), PCI_CLASS_REG);
	gt->sc_rev = PCI_REVISION(gt_read(gt, GTPCI_CD(0)));

	aprint_naive("\n");
	switch (gt->sc_model) {
	case MARVELL_DISCOVERY:
		aprint_normal(": GT-6426x%c Discovery\n",
		    (gt->sc_rev == MARVELL_DISCOVERY_REVA) ? 'A' : 'B');
		break;
	case MARVELL_DISCOVERY_II:
		aprint_normal(": MV6436x Discovery II\n");
		break;

	case MARVELL_DISCOVERY_III:
		aprint_normal(": MV6446x Discovery III\n");
		break;
#if 0
	case MARVELL_DISCOVERY_LT:
	case MARVELL_DISCOVERY_V:
	case MARVELL_DISCOVERY_VI:
#endif

	default:
		aprint_normal(": type unknown\n"); break;
	}

	cpumode = gt_read(gt, GT_CPU_Mode);
	aprint_normal_dev(gt->sc_dev,
	    "id %d", GT_CPUMode_MultiGTID_GET(cpumode));
	if (cpumode & GT_CPUMode_MultiGT)
		aprint_normal (" (multi)");
	switch (GT_CPUMode_CPUType_GET(cpumode)) {
	case 4: aprint_normal(", 60x bus"); break;
	case 5: aprint_normal(", MPX bus"); break;

	default:
		aprint_normal(", %#x(?) bus", GT_CPUMode_CPUType_GET(cpumode));
		break;
	}

	cpumstr = gt_read(gt, GT_CPU_Master_Ctl);
	switch (cpumstr & (GT_CPUMstrCtl_CleanBlock|GT_CPUMstrCtl_FlushBlock)) {
	case 0: break;
	case GT_CPUMstrCtl_CleanBlock: aprint_normal(", snoop=clean"); break;
	case GT_CPUMstrCtl_FlushBlock: aprint_normal(", snoop=flush"); break;
	case GT_CPUMstrCtl_CleanBlock|GT_CPUMstrCtl_FlushBlock:
		aprint_normal(", snoop=clean&flush"); break;
	}
	aprint_normal(" wdog=%#x,%#x\n",
	    gt_read(gt, GT_WDOG_Config), gt_read(gt, GT_WDOG_Value));

#ifdef GT_DEBUG
	loaddr = GT_LADDR_GET(gt_read(gt, GT_SCS0_Low_Decode), gt->sc_model);
	hiaddr = GT_HADDR_GET(gt_read(gt, GT_SCS0_High_Decode), gt->sc_model);
	aprint_normal_dev(gt->sc_dev, "     scs[0]=%#10x-%#10x\n",
	    loaddr, hiaddr);

	loaddr = GT_LADDR_GET(gt_read(gt, GT_SCS1_Low_Decode), gt->sc_model);
	hiaddr = GT_HADDR_GET(gt_read(gt, GT_SCS1_High_Decode), gt->sc_model);
	aprint_normal_dev(gt->sc_dev, "     scs[1]=%#10x-%#10x\n",
	    loaddr, hiaddr);

	loaddr = GT_LADDR_GET(gt_read(gt, GT_SCS2_Low_Decode), gt->sc_model);
	hiaddr = GT_HADDR_GET(gt_read(gt, GT_SCS2_High_Decode), gt->sc_model);
	aprint_normal_dev(gt->sc_dev, "     scs[2]=%#10x-%#10x\n",
	    loaddr, hiaddr);

	loaddr = GT_LADDR_GET(gt_read(gt, GT_SCS3_Low_Decode), gt->sc_model);
	hiaddr = GT_HADDR_GET(gt_read(gt, GT_SCS3_High_Decode), gt->sc_model);
	aprint_normal_dev(gt->sc_dev, "     scs[3]=%#10x-%#10x\n",
	    loaddr, hiaddr);

	loaddr = GT_LADDR_GET(gt_read(gt, GT_CS0_Low_Decode), gt->sc_model);
	hiaddr = GT_HADDR_GET(gt_read(gt, GT_CS0_High_Decode), gt->sc_model);
	aprint_normal_dev(gt->sc_dev, "      cs[0]=%#10x-%#10x\n",
	    loaddr, hiaddr);

	loaddr = GT_LADDR_GET(gt_read(gt, GT_CS1_Low_Decode), gt->sc_model);
	hiaddr = GT_HADDR_GET(gt_read(gt, GT_CS1_High_Decode), gt->sc_model);
	aprint_normal_dev(gt->sc_dev, "      cs[1]=%#10x-%#10x\n",
	    loaddr, hiaddr);

	loaddr = GT_LADDR_GET(gt_read(gt, GT_CS2_Low_Decode), gt->sc_model);
	hiaddr = GT_HADDR_GET(gt_read(gt, GT_CS2_High_Decode), gt->sc_model);
	aprint_normal_dev(gt->sc_dev, "      cs[2]=%#10x-%#10x\n",
	    loaddr, hiaddr);

	loaddr = GT_LADDR_GET(gt_read(gt, GT_CS3_Low_Decode), gt->sc_model);
	hiaddr = GT_HADDR_GET(gt_read(gt, GT_CS3_High_Decode), gt->sc_model);
	aprint_normal_dev(gt->sc_dev, "      cs[3]=%#10x-%#10x\n",
	    loaddr, hiaddr);

	loaddr = GT_LADDR_GET(gt_read(gt, GT_BootCS_Low_Decode), gt->sc_model);
	hiaddr = GT_HADDR_GET(gt_read(gt, GT_BootCS_High_Decode), gt->sc_model);
	aprint_normal_dev(gt->sc_dev, "     bootcs=%#10x-%#10x\n",
	    loaddr, hiaddr);

	loaddr = GT_LADDR_GET(gt_read(gt, GT_PCI0_IO_Low_Decode), gt->sc_model);
	hiaddr =
	    GT_HADDR_GET(gt_read(gt, GT_PCI0_IO_High_Decode), gt->sc_model);
	aprint_normal_dev(gt->sc_dev, "     pci0io=%#10x-%#10x  ",
	    loaddr, hiaddr);

	loaddr = gt_read(gt, GT_PCI0_IO_Remap);
	aprint_normal("remap=%#010x\n", loaddr);

	loaddr =
	    GT_LADDR_GET(gt_read(gt, GT_PCI0_Mem0_Low_Decode), gt->sc_model);
	hiaddr =
	    GT_HADDR_GET(gt_read(gt, GT_PCI0_Mem0_High_Decode), gt->sc_model);
	aprint_normal_dev(gt->sc_dev, " pci0mem[0]=%#10x-%#10x  ",
	    loaddr, hiaddr);

	loaddr = gt_read(gt, GT_PCI0_Mem0_Remap_Low);
	hiaddr = gt_read(gt, GT_PCI0_Mem0_Remap_High);
	aprint_normal("remap=%#010x.%#010x\n", hiaddr, loaddr);

	loaddr =
	    GT_LADDR_GET(gt_read(gt, GT_PCI0_Mem1_Low_Decode), gt->sc_model);
	hiaddr =
	    GT_HADDR_GET(gt_read(gt, GT_PCI0_Mem1_High_Decode), gt->sc_model);
	aprint_normal_dev(gt->sc_dev, " pci0mem[1]=%#10x-%#10x  ",
	    loaddr, hiaddr);

	loaddr = gt_read(gt, GT_PCI0_Mem1_Remap_Low);
	hiaddr = gt_read(gt, GT_PCI0_Mem1_Remap_High);
	aprint_normal("remap=%#010x.%#010x\n", hiaddr, loaddr);

	loaddr =
	    GT_LADDR_GET(gt_read(gt, GT_PCI0_Mem2_Low_Decode), gt->sc_model);
	hiaddr =
	    GT_HADDR_GET(gt_read(gt, GT_PCI0_Mem2_High_Decode), gt->sc_model);
	aprint_normal_dev(gt->sc_dev, " pci0mem[2]=%#10x-%#10x  ",
	    loaddr, hiaddr);

	loaddr = gt_read(gt, GT_PCI0_Mem2_Remap_Low);
	hiaddr = gt_read(gt, GT_PCI0_Mem2_Remap_High);
	aprint_normal("remap=%#010x.%#010x\n", hiaddr, loaddr);

	loaddr =
	    GT_LADDR_GET(gt_read(gt, GT_PCI0_Mem3_Low_Decode), gt->sc_model);
	hiaddr =
	    GT_HADDR_GET(gt_read(gt, GT_PCI0_Mem3_High_Decode), gt->sc_model);
	aprint_normal_dev(gt->sc_dev, " pci0mem[3]=%#10x-%#10x  ",
	    loaddr, hiaddr);

	loaddr = gt_read(gt, GT_PCI0_Mem3_Remap_Low);
	hiaddr = gt_read(gt, GT_PCI0_Mem3_Remap_High);
	aprint_normal("remap=%#010x.%#010x\n", hiaddr, loaddr);

	loaddr = GT_LADDR_GET(gt_read(gt, GT_PCI1_IO_Low_Decode), gt->sc_model);
	hiaddr =
	    GT_HADDR_GET(gt_read(gt, GT_PCI1_IO_High_Decode), gt->sc_model);
	aprint_normal_dev(gt->sc_dev, "     pci1io=%#10x-%#10x  ",
	    loaddr, hiaddr);

	loaddr = gt_read(gt, GT_PCI1_IO_Remap);
	aprint_normal("remap=%#010x\n", loaddr);

	loaddr =
	    GT_LADDR_GET(gt_read(gt, GT_PCI1_Mem0_Low_Decode), gt->sc_model);
	hiaddr =
	    GT_HADDR_GET(gt_read(gt, GT_PCI1_Mem0_High_Decode), gt->sc_model);
	aprint_normal_dev(gt->sc_dev, " pci1mem[0]=%#10x-%#10x  ",
	    loaddr, hiaddr);

	loaddr = gt_read(gt, GT_PCI1_Mem0_Remap_Low);
	hiaddr = gt_read(gt, GT_PCI1_Mem0_Remap_High);
	aprint_normal("remap=%#010x.%#010x\n", hiaddr, loaddr);

	loaddr =
	    GT_LADDR_GET(gt_read(gt, GT_PCI1_Mem1_Low_Decode), gt->sc_model);
	hiaddr =
	    GT_HADDR_GET(gt_read(gt, GT_PCI1_Mem1_High_Decode), gt->sc_model);
	aprint_normal_dev(gt->sc_dev, " pci1mem[1]=%#10x-%#10x  ",
	    loaddr, hiaddr);

	loaddr = gt_read(gt, GT_PCI1_Mem1_Remap_Low);
	hiaddr = gt_read(gt, GT_PCI1_Mem1_Remap_High);
	aprint_normal("remap=%#010x.%#010x\n", hiaddr, loaddr);

	loaddr =
	    GT_LADDR_GET(gt_read(gt, GT_PCI1_Mem2_Low_Decode), gt->sc_model);
	hiaddr =
	    GT_HADDR_GET(gt_read(gt, GT_PCI1_Mem2_High_Decode), gt->sc_model);
	aprint_normal_dev(gt->sc_dev, " pci1mem[2]=%#10x-%#10x  ",
	    loaddr, hiaddr);

	loaddr = gt_read(gt, GT_PCI1_Mem2_Remap_Low);
	hiaddr = gt_read(gt, GT_PCI1_Mem2_Remap_High);
	aprint_normal("remap=%#010x.%#010x\n", hiaddr, loaddr);

	loaddr =
	    GT_LADDR_GET(gt_read(gt, GT_PCI1_Mem3_Low_Decode), gt->sc_model);
	hiaddr =
	    GT_HADDR_GET(gt_read(gt, GT_PCI1_Mem3_High_Decode), gt->sc_model);
	aprint_normal_dev(gt->sc_dev, " pci1mem[3]=%#10x-%#10x  ",
	    loaddr, hiaddr);

	loaddr = gt_read(gt, GT_PCI1_Mem3_Remap_Low);
	hiaddr = gt_read(gt, GT_PCI1_Mem3_Remap_High);
	aprint_normal("remap=%#010x.%#010x\n", hiaddr, loaddr);

	loaddr = GT_LADDR_GET(gt_read(gt, GT_Internal_Decode), gt->sc_model);
	aprint_normal_dev(gt->sc_dev, "   internal=%#10x-%#10x\n",
	    loaddr, loaddr + 256 * 1024);

	loaddr = GT_LADDR_GET(gt_read(gt, GT_CPU0_Low_Decode), gt->sc_model);
	hiaddr = GT_HADDR_GET(gt_read(gt, GT_CPU0_High_Decode), gt->sc_model);
	aprint_normal_dev(gt->sc_dev, "       cpu0=%#10x-%#10x\n",
	    loaddr, hiaddr);

#ifdef MULTIPROCESSOR
	loaddr = GT_LADDR_GET(gt_read(gt, GT_CPU1_Low_Decode), gt->sc_model);
	hiaddr = GT_HADDR_GET(gt_read(gt, GT_CPU1_High_Decode), gt->sc_model);
	aprint_normal_dev(gt->sc_dev, "       cpu1=%#10x-%#10x",
	    loaddr, hiaddr);
#endif
#endif

	aprint_normal("%s:", device_xname(gt->sc_dev));

	cpucfg = gt_read(gt, GT_CPU_Cfg);
	cpucfg |= GT_CPUCfg_ConfSBDis;		/* per errata #46 */
	cpucfg |= GT_CPUCfg_AACKDelay;		/* per restriction #18 */
	gt_write(gt, GT_CPU_Cfg, cpucfg);
	if (cpucfg & GT_CPUCfg_Pipeline)
		aprint_normal(" pipeline");
	if (cpucfg & GT_CPUCfg_AACKDelay)
		aprint_normal(" aack-delay");
	if (cpucfg & GT_CPUCfg_RdOOO)
		aprint_normal(" read-ooo");
	if (cpucfg & GT_CPUCfg_IOSBDis)
		aprint_normal(" io-sb-dis");
	if (cpucfg & GT_CPUCfg_ConfSBDis)
		aprint_normal(" conf-sb-dis");
	if (cpucfg & GT_CPUCfg_ClkSync)
		aprint_normal(" clk-sync");
	aprint_normal("\n");

#ifdef GT_WATCHDOG
	gt_watchdog_init(gt);
#endif

#ifdef GT_DEVBUS
	gt_devbus_intr_enb(gt);
#endif
#ifdef GT_ECC
	gt_ecc_intr_enb(gt);
#endif
#if NGTMPSC > 0
	gt_sdma_intr_enb(gt);
#endif
#ifdef GT_COMM
	gt_comm_intr_enb(gt);
#endif

	gt_attach_peripherals(gt);

#ifdef GT_WATCHDOG
	gt_watchdog_service();
	gt_watchdog_enable(gt);
#endif
}


#ifdef GT_DEVBUS
static int
gt_devbus_intr(void *arg)
{
	struct gt_softc *gt = (struct gt_softc *)arg;
	u_int32_t cause;
	u_int32_t addr;

	cause = gt_read(gt, GT_DEVBUS_ICAUSE);
	addr = gt_read(gt, GT_DEVBUS_ERR_ADDR);
	gt_write(gt, GT_DEVBUS_ICAUSE, 0);	/* clear intr */

	if (cause & GT_DEVBUS_DBurstErr) {
		aprint_error_dev(gt->sc_dev,
		    "Device Bus error: burst violation");
		if ((cause & GT_DEVBUS_Sel) == 0)
			aprint_error(", addr %#x", addr);
		aprint_error("\n");
	}
	if (cause & GT_DEVBUS_DRdyErr) {
		aprint_error_dev(gt->sc_dev,
		    "Device Bus error: ready timer expired");
		if ((cause & GT_DEVBUS_Sel) != 0)
			aprint_error(", addr %#x\n", addr);
		aprint_error("\n");
	}

	return cause != 0;
}

/*
 * gt_devbus_intr_enb - enable GT-64260 Device Bus interrupts
 */
static void
gt_devbus_intr_enb(struct gt_softc *gt)
{
	gt_write(gt, GT_DEVBUS_IMASK,
		GT_DEVBUS_DBurstErr|GT_DEVBUS_DRdyErr);
	(void)gt_read(gt, GT_DEVBUS_ERR_ADDR);	/* clear addr */
	gt_write(gt, GT_DEVBUS_ICAUSE, 0);	/* clear intr */

	(void)marvell_intr_establish(IRQ_DEV, IPL_VM, gt_devbus_intr, gt);
}
#endif	/* GT_DEVBUS */

#ifdef GT_ECC
const static char *gt_ecc_intr_str[4] = {
	"(none)",
	"single bit",
	"double bit",
	"(reserved)"
};

static int
gt_ecc_intr(void *arg)
{
	struct gt_softc *gt = (struct gt_softc *)arg;
	uint32_t addr, dlo, dhi, rec, calc, count;
	int err;

	count = gt_read(gt, GT_ECC_Count);
	dlo   = gt_read(gt, GT_ECC_Data_Lo);
	dhi   = gt_read(gt, GT_ECC_Data_Hi);
	rec   = gt_read(gt, GT_ECC_Rec);
	calc  = gt_read(gt, GT_ECC_Calc);
	addr  = gt_read(gt, GT_ECC_Addr);	/* read last! */
	gt_write(gt, GT_ECC_Addr, 0);		/* clear intr */

	err = addr & 0x3;

	aprint_error_dev(gt->sc_dev,
	    "ECC error: %s: addr %#x data %#x.%#x rec %#x calc %#x cnt %#x\n",
	    gt_ecc_intr_str[err], addr, dhi, dlo, rec, calc, count);

	if (err == 2)
		panic("ecc");

	return err == 1;
}

/*
 * gt_ecc_intr_enb - enable GT-64260 ECC interrupts
 */
static void
gt_ecc_intr_enb(struct gt_softc *gt)
{
	uint32_t ctl;

	ctl = gt_read(gt, GT_ECC_Ctl);
	ctl |= 1 << 16;		/* XXX 1-bit threshold == 1 */
	gt_write(gt, GT_ECC_Ctl, ctl);
	(void)gt_read(gt, GT_ECC_Data_Lo);
	(void)gt_read(gt, GT_ECC_Data_Hi);
	(void)gt_read(gt, GT_ECC_Rec);
	(void)gt_read(gt, GT_ECC_Calc);
	(void)gt_read(gt, GT_ECC_Addr);		/* read last! */
	gt_write(gt, GT_ECC_Addr, 0);		/* clear intr */

	(void)marvell_intr_establish(IRQ_ECC, IPL_VM, gt_ecc_intr, gt);
}
#endif	/* GT_ECC */

#if NGTMPSC > 0
/*
 * gt_sdma_intr_enb - enable GT-64260 SDMA interrupts
 */
static void
gt_sdma_intr_enb(struct gt_softc *gt)
{

	(void)marvell_intr_establish(IRQ_SDMA, IPL_SERIAL, gtmpsc_intr, gt);
}
#endif

#ifdef GT_COMM
/*
 * unknown board, enable everything
 */
# define GT_CommUnitIntr_DFLT	\
	    GT_CommUnitIntr_S0 |\
	    GT_CommUnitIntr_S1 |\
	    GT_CommUnitIntr_E0 |\
	    GT_CommUnitIntr_E1 |\
	    GT_CommUnitIntr_E2

static const char * const gt_comm_subunit_name[8] = {
	"ethernet 0",
	"ethernet 1",
	"ethernet 2",
	"(reserved)",
	"MPSC 0",
	"MPSC 1",
	"(reserved)",
	"(sel)",
};

static int
gt_comm_intr(void *arg)
{
	struct gt_softc *gt = (struct gt_softc *)arg;
	uint32_t cause, addr;
	unsigned int mask;
	int i;

	cause = gt_read(gt, GT_CommUnitIntr_Cause);
	gt_write(gt, GT_CommUnitIntr_Cause, ~cause);
	addr = gt_read(gt, GT_CommUnitIntr_ErrAddr);

	aprint_error_dev(gt->sc_dev,
	    "Communications Unit Controller interrupt, cause %#x addr %#x\n",
	    cause, addr);

	cause &= GT_CommUnitIntr_DFLT;
	if (cause == 0)
		return 0;

	mask = 0x7;
	for (i=0; i<7; i++) {
		if (cause & mask) {
			printf("%s: Comm Unit %s:", device_xname(gt->sc_dev),
				gt_comm_subunit_name[i]);
			if (cause & 1)
				printf(" AddrMiss");
			if (cause & 2)
				printf(" AccProt");
			if (cause & 4)
				printf(" WrProt");
			printf("\n");
		}
		cause >>= 4;
	}
	return 1;
}

/*
 * gt_comm_intr_init - enable GT-64260 Comm Unit interrupts
 */
static void
gt_comm_intr_enb(struct gt_softc *gt)
{
	uint32_t cause;

	cause = gt_read(gt, GT_CommUnitIntr_Cause);
	if (cause)
		gt_write(gt, GT_CommUnitIntr_Cause, ~cause);
	gt_write(gt, GT_CommUnitIntr_Mask, GT_CommUnitIntr_DFLT);
	(void)gt_read(gt, GT_CommUnitIntr_ErrAddr);

	(void)marvell_intr_establish(IRQ_COMM, IPL_VM, gt_comm_intr, gt);
}
#endif	/* GT_COMM */


#ifdef GT_WATCHDOG
#ifndef GT_MPP_WATCHDOG
static void
gt_watchdog_init(struct gt_softc *gt)
{
	u_int32_t r;

	aprint_normal_dev(gt->sc_dev, "watchdog");

	/*
	 * handle case where firmware started watchdog
	 */
	r = gt_read(gt, GT_WDOG_Config);
	aprint_normal(" status %#x,%#x:", r, gt_read(gt, GT_WDOG_Value));
	if ((r & 0x80000000) != 0) {
		gt_watchdog_sc = gt;		/* enabled */
		gt_watchdog_state = 1;
		aprint_normal(" firmware-enabled\n");
		gt_watchdog_disable(gt);
	} else
		aprint_normal(" firmware-disabled\n");
}

#elif	GT_MPP_WATCHDOG == 0

static void
gt_watchdog_init(struct gt_softc *gt)
{

	aprint_normal_dev(gt->sc_dev, "watchdog not configured\n");
	return;
}

#else	/* GT_MPP_WATCHDOG > 0 */

static void
gt_watchdog_init(struct gt_softc *gt)
{
	u_int32_t mpp_watchdog = GT_MPP_WATCHDOG;	/* from config */
	u_int32_t cfgbits, mppbits, mppmask, regoff, r;

	mppmask = 0;

	aprint_normal_dev(gt->sc_dev, "watchdog");

	/*
	 * if firmware started watchdog, we disable and start
	 * from scratch to get it in a known state.
	 *
	 * on GT-64260A we always see 0xffffffff
	 * in both the GT_WDOG_Config_Enb and GT_WDOG_Value registers.
	 */
	r = gt_read(gt, GT_WDOG_Config);
	if (r != ~0) {
		if ((r & GT_WDOG_Config_Enb) != 0) {
			gt_write(gt, GT_WDOG_Config,
			    GT_WDOG_Config_Ctl1a | GT_WDOG_Preset_DFLT);
			gt_write(gt, GT_WDOG_Config,
			    GT_WDOG_Config_Ctl1b | GT_WDOG_Preset_DFLT);
		}
	}

	/*
	 * "the watchdog timer can be activated only after
	 * configuring two MPP pins to act as WDE and WDNMI"
	 */
	mppbits = 0;
	cfgbits = 0x3;
	for (regoff = GT_MPP_Control0; regoff <= GT_MPP_Control3; regoff += 4) {
		if ((mpp_watchdog & cfgbits) == cfgbits) {
			mppbits = 0x99;
			mppmask = 0xff;
			break;
		}
		cfgbits <<= 2;
		if ((mpp_watchdog & cfgbits) == cfgbits) {
			mppbits = 0x9900;
			mppmask = 0xff00;
			break;
		}
		cfgbits <<= 6;	/* skip unqualified bits */
	}
	if (mppbits == 0) {
		aprint_error(" config error\n");
		return;
	}

	r = gt_read(gt, regoff);
	r &= ~mppmask;
	r |= mppbits;
	gt_write(gt, regoff, r);
	aprint_normal(" mpp %#x %#x", regoff, mppbits);

	gt_write(gt, GT_WDOG_Value, GT_WDOG_NMI_DFLT);

	gt_write(gt, GT_WDOG_Config, GT_WDOG_Config_Ctl1a|GT_WDOG_Preset_DFLT);
	gt_write(gt, GT_WDOG_Config, GT_WDOG_Config_Ctl1b|GT_WDOG_Preset_DFLT);

	r = gt_read(gt, GT_WDOG_Config),
	aprint_normal(" status %#x,%#x: %s\n",
	    r, gt_read(gt, GT_WDOG_Value),
	    ((r & GT_WDOG_Config_Enb) != 0) ? "enabled" : "botch");
}
#endif	/* GT_MPP_WATCHDOG */

static void
gt_watchdog_enable(struct gt_softc *gt)
{

	if (gt_watchdog_state == 0) {
		gt_watchdog_state = 1;

		gt_write(gt, GT_WDOG_Config,
		    GT_WDOG_Config_Ctl1a | GT_WDOG_Preset_DFLT);
		gt_write(gt, GT_WDOG_Config,
		    GT_WDOG_Config_Ctl1b | GT_WDOG_Preset_DFLT);
	}
}

#ifndef GT_MPP_WATCHDOG
static void
gt_watchdog_disable(struct gt_softc *gt)
{

	if (gt_watchdog_state != 0) {
		gt_watchdog_state = 0;

		gt_write(gt, GT_WDOG_Config,
		    GT_WDOG_Config_Ctl1a | GT_WDOG_Preset_DFLT);
		gt_write(gt, GT_WDOG_Config,
		    GT_WDOG_Config_Ctl1b | GT_WDOG_Preset_DFLT);
	}
}
#endif

/*
 * XXXX: gt_watchdog_service/reset functions need mutex lock...
 */

#ifdef GT_DEBUG
int inhibit_watchdog_service = 0;
#endif
void
gt_watchdog_service(void)
{
	struct gt_softc *gt = gt_watchdog_sc;

	if ((gt == NULL) || (gt_watchdog_state == 0))
		return;		/* not enabled */
#ifdef GT_DEBUG
	if (inhibit_watchdog_service)
		return;
#endif

	gt_write(gt, GT_WDOG_Config, GT_WDOG_Config_Ctl2a|GT_WDOG_Preset_DFLT);
	gt_write(gt, GT_WDOG_Config, GT_WDOG_Config_Ctl2b|GT_WDOG_Preset_DFLT);
}

/*
 * gt_watchdog_reset - force a watchdog reset using Preset_VAL=0
 */
void
gt_watchdog_reset(void)
{
	struct gt_softc *gt = gt_watchdog_sc;
	u_int32_t r;

	r = gt_read(gt, GT_WDOG_Config);
	gt_write(gt, GT_WDOG_Config, GT_WDOG_Config_Ctl1a);
	gt_write(gt, GT_WDOG_Config, GT_WDOG_Config_Ctl1b);
	if ((r & GT_WDOG_Config_Enb) != 0) {
		/*
		 * was enabled, we just toggled it off, toggle on again
		 */
		gt_write(gt, GT_WDOG_Config, GT_WDOG_Config_Ctl1a);
		gt_write(gt, GT_WDOG_Config, GT_WDOG_Config_Ctl1b);
	}
	for(;;);
}
#endif


int
marvell_winparams_by_tag(device_t dev, int tag, int *target, int *attr,
			 uint64_t *base, uint32_t *size)
{
	static const struct {
		int tag;
		uint32_t attribute;
		uint32_t basereg;
		uint32_t sizereg;
	} tagtbl[] = {
		{ MARVELL_TAG_SDRAM_CS0,	MARVELL_ATTR_SDRAM_CS0,
		  GT_SCS0_Low_Decode,		GT_SCS0_High_Decode },
		{ MARVELL_TAG_SDRAM_CS1,	MARVELL_ATTR_SDRAM_CS1,
		  GT_SCS1_Low_Decode,		GT_SCS1_High_Decode },
		{ MARVELL_TAG_SDRAM_CS2,	MARVELL_ATTR_SDRAM_CS2,
		  GT_SCS2_Low_Decode,		GT_SCS2_High_Decode },
		{ MARVELL_TAG_SDRAM_CS3,	MARVELL_ATTR_SDRAM_CS3,
		  GT_SCS3_Low_Decode,		GT_SCS3_High_Decode },

		{ MARVELL_TAG_UNDEFINED, 0, 0 }
	};
	struct gt_softc *sc = device_private(dev);
	int i;

	for (i = 0; tagtbl[i].tag != MARVELL_TAG_UNDEFINED; i++)
		if (tag == tagtbl[i].tag)
			break;
	if (tagtbl[i].tag == MARVELL_TAG_UNDEFINED)
		return -1;

	if (target != NULL)
		*target = 0;
	if (attr != NULL)
		*attr = tagtbl[i].attribute;
	if (base != NULL)
		*base = gt_read(sc, tagtbl[i].basereg) <<
		    (sc->sc_model == MARVELL_DISCOVERY ? 20 : 16);
	if (size != NULL) {
		const uint32_t s = gt_read(sc, tagtbl[i].sizereg);

		if (s != 0)
			*size = (s + 1) <<
			    (sc->sc_model == MARVELL_DISCOVERY ? 20 : 16);
		else
			*size = 0;
	}

	return 0;
}

/* $NetBSD: isp_pci.c,v 1.117 2014/03/29 19:28:25 christos Exp $ */
/*
 * Copyright (C) 1997, 1998, 1999 National Aeronautics & Space Administration
 * All rights reserved.
 *
 * Additional Copyright (C) 2000-2007 by Matthew Jacob
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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
 * PCI specific probe and attach routines for Qlogic ISP SCSI adapters.
 */

/*
 * 24XX 4Gb material support provided by MetrumRG Associates.
 * Many thanks are due to them.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: isp_pci.c,v 1.117 2014/03/29 19:28:25 christos Exp $");

#include <dev/ic/isp_netbsd.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <sys/reboot.h>

static uint32_t isp_pci_rd_reg(struct ispsoftc *, int);
static void isp_pci_wr_reg(struct ispsoftc *, int, uint32_t);
#if !(defined(ISP_DISABLE_1080_SUPPORT) && defined(ISP_DISABLE_12160_SUPPORT))
static uint32_t isp_pci_rd_reg_1080(struct ispsoftc *, int);
static void isp_pci_wr_reg_1080(struct ispsoftc *, int, uint32_t);
#endif
#if !defined(ISP_DISABLE_2100_SUPPORT) && \
	 !defined(ISP_DISABLE_2200_SUPPORT) && \
	 !defined(ISP_DISABLE_1020_SUPPORT) && \
	 !defined(ISP_DISABLE_1080_SUPPORT) && \
	 !defined(ISP_DISABLE_12160_SUPPORT)
static int
isp_pci_rd_isr(struct ispsoftc *, uint32_t *, uint16_t *, uint16_t *);
#endif
#if !(defined(ISP_DISABLE_2300_SUPPORT) && defined(ISP_DISABLE_2322_SUPPORT))
static int
isp_pci_rd_isr_2300(struct ispsoftc *, uint32_t *, uint16_t *, uint16_t *);
#endif
#if !defined(ISP_DISABLE_2400_SUPPORT)
static uint32_t isp_pci_rd_reg_2400(struct ispsoftc *, int);
static void isp_pci_wr_reg_2400(struct ispsoftc *, int, uint32_t);
static int
isp_pci_rd_isr_2400(struct ispsoftc *, uint32_t *, uint16_t *, uint16_t *);
#endif
static int isp_pci_mbxdma(struct ispsoftc *);
static int isp_pci_dmasetup(struct ispsoftc *, XS_T *, void *);
static void isp_pci_dmateardown(struct ispsoftc *, XS_T *, uint32_t);
static void isp_pci_reset0(struct ispsoftc *);
static void isp_pci_reset1(struct ispsoftc *);
static void isp_pci_dumpregs(struct ispsoftc *, const char *);
static int isp_pci_intr(void *);

#if	defined(ISP_DISABLE_1020_SUPPORT) || defined(ISP_DISABLE_FW)
#define	ISP_1040_RISC_CODE	NULL
#else
#define	ISP_1040_RISC_CODE	(const uint16_t *) isp_1040_risc_code
#include <dev/microcode/isp/asm_1040.h>
#endif

#if	defined(ISP_DISABLE_1080_SUPPORT) || defined(ISP_DISABLE_FW)
#define	ISP_1080_RISC_CODE	NULL
#else
#define	ISP_1080_RISC_CODE	(const uint16_t *) isp_1080_risc_code
#include <dev/microcode/isp/asm_1080.h>
#endif

#if	defined(ISP_DISABLE_12160_SUPPORT) || defined(ISP_DISABLE_FW)
#define	ISP_12160_RISC_CODE	NULL
#else
#define	ISP_12160_RISC_CODE	(const uint16_t *) isp_12160_risc_code
#include <dev/microcode/isp/asm_12160.h>
#endif

#if	defined(ISP_DISABLE_2100_SUPPORT) || defined(ISP_DISABLE_FW)
#define	ISP_2100_RISC_CODE	NULL
#else
#define	ISP_2100_RISC_CODE	(const uint16_t *) isp_2100_risc_code
#include <dev/microcode/isp/asm_2100.h>
#endif

#if	defined(ISP_DISABLE_2200_SUPPORT) || defined(ISP_DISABLE_FW)
#define	ISP_2200_RISC_CODE	NULL
#else
#define	ISP_2200_RISC_CODE	(const uint16_t *) isp_2200_risc_code
#include <dev/microcode/isp/asm_2200.h>
#endif

#if	defined(ISP_DISABLE_2300_SUPPORT) || defined(ISP_DISABLE_FW)
#define	ISP_2300_RISC_CODE	NULL
#else
#define	ISP_2300_RISC_CODE	(const uint16_t *) isp_2300_risc_code
#include <dev/microcode/isp/asm_2300.h>
#endif
#if	defined(ISP_DISABLE_2322_SUPPORT) || defined(ISP_DISABLE_FW)
#define	ISP_2322_RISC_CODE	NULL
#else
#define	ISP_2322_RISC_CODE	(const uint16_t *) isp_2322_risc_code
#include <dev/microcode/isp/asm_2322.h>
#endif

#if	defined(ISP_DISABLE_2400_SUPPORT) || defined(ISP_DISABLE_FW)
#define	ISP_2400_RISC_CODE	NULL
#define	ISP_2500_RISC_CODE	NULL
#else
#define	ISP_2500
#define	ISP_2400
#define	ISP_2400_RISC_CODE	(const uint32_t *) isp_2400_risc_code
#define	ISP_2500_RISC_CODE	(const uint32_t *) isp_2500_risc_code
#include <dev/microcode/isp/asm_2400.h>
#include <dev/microcode/isp/asm_2500.h>
#endif

#ifndef	ISP_DISABLE_1020_SUPPORT
static struct ispmdvec mdvec = {
	isp_pci_rd_isr,
	isp_pci_rd_reg,
	isp_pci_wr_reg,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP_1040_RISC_CODE,
	BIU_BURST_ENABLE|BIU_PCI_CONF1_FIFO_64,
	0
};
#endif

#ifndef	ISP_DISABLE_1080_SUPPORT
static struct ispmdvec mdvec_1080 = {
	isp_pci_rd_isr,
	isp_pci_rd_reg_1080,
	isp_pci_wr_reg_1080,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP_1080_RISC_CODE,
	BIU_BURST_ENABLE|BIU_PCI_CONF1_FIFO_64,
	0
};
#endif

#ifndef	ISP_DISABLE_12160_SUPPORT
static struct ispmdvec mdvec_12160 = {
	isp_pci_rd_isr,
	isp_pci_rd_reg_1080,
	isp_pci_wr_reg_1080,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP_12160_RISC_CODE,
	BIU_BURST_ENABLE|BIU_PCI_CONF1_FIFO_64,
	0
};
#endif

#ifndef	ISP_DISABLE_2100_SUPPORT
static struct ispmdvec mdvec_2100 = {
	isp_pci_rd_isr,
	isp_pci_rd_reg,
	isp_pci_wr_reg,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP_2100_RISC_CODE,
	0,
	0
};
#endif

#ifndef	ISP_DISABLE_2200_SUPPORT
static struct ispmdvec mdvec_2200 = {
	isp_pci_rd_isr,
	isp_pci_rd_reg,
	isp_pci_wr_reg,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP_2200_RISC_CODE,
	0,
	0
};
#endif

#ifndef ISP_DISABLE_2300_SUPPORT
static struct ispmdvec mdvec_2300 = {
	isp_pci_rd_isr_2300,
	isp_pci_rd_reg,
	isp_pci_wr_reg,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP_2300_RISC_CODE,
	0,
	0
};
#endif

#ifndef ISP_DISABLE_2322_SUPPORT
static struct ispmdvec mdvec_2322 = {
	isp_pci_rd_isr_2300,
	isp_pci_rd_reg,
	isp_pci_wr_reg,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP_2322_RISC_CODE,
	0,
	0
};
#endif

#ifndef	ISP_DISABLE_2400_SUPPORT
static struct ispmdvec mdvec_2400 = {
	isp_pci_rd_isr_2400,
	isp_pci_rd_reg_2400,
	isp_pci_wr_reg_2400,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
	NULL,
	ISP_2400_RISC_CODE,
	0,
	0
};
static struct ispmdvec mdvec_2500 = {
	isp_pci_rd_isr_2400,
	isp_pci_rd_reg_2400,
	isp_pci_wr_reg_2400,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
	NULL,
	ISP_2500_RISC_CODE,
	0,
	0
};
#endif

#ifndef	PCI_VENDOR_QLOGIC
#define	PCI_VENDOR_QLOGIC	0x1077
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP1020
#define	PCI_PRODUCT_QLOGIC_ISP1020	0x1020
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP1080
#define	PCI_PRODUCT_QLOGIC_ISP1080	0x1080
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP1240
#define	PCI_PRODUCT_QLOGIC_ISP1240	0x1240
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP1280
#define	PCI_PRODUCT_QLOGIC_ISP1280	0x1280
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP10160
#define	PCI_PRODUCT_QLOGIC_ISP10160	0x1016
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP12160
#define	PCI_PRODUCT_QLOGIC_ISP12160	0x1216
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2100
#define	PCI_PRODUCT_QLOGIC_ISP2100	0x2100
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2200
#define	PCI_PRODUCT_QLOGIC_ISP2200	0x2200
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2300
#define	PCI_PRODUCT_QLOGIC_ISP2300	0x2300
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2312
#define	PCI_PRODUCT_QLOGIC_ISP2312	0x2312
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2322
#define	PCI_PRODUCT_QLOGIC_ISP2322	0x2322
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2422
#define	PCI_PRODUCT_QLOGIC_ISP2422	0x2422
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2432
#define	PCI_PRODUCT_QLOGIC_ISP2432	0x2432
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2532
#define	PCI_PRODUCT_QLOGIC_ISP2532	0x2532
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP6312
#define	PCI_PRODUCT_QLOGIC_ISP6312	0x6312
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP6322
#define	PCI_PRODUCT_QLOGIC_ISP6322	0x6322
#endif


#define	PCI_QLOGIC_ISP	((PCI_PRODUCT_QLOGIC_ISP1020 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP1080	\
	((PCI_PRODUCT_QLOGIC_ISP1080 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP10160	\
	((PCI_PRODUCT_QLOGIC_ISP10160 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP12160	\
	((PCI_PRODUCT_QLOGIC_ISP12160 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP1240	\
	((PCI_PRODUCT_QLOGIC_ISP1240 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP1280	\
	((PCI_PRODUCT_QLOGIC_ISP1280 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2100	\
	((PCI_PRODUCT_QLOGIC_ISP2100 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2200	\
	((PCI_PRODUCT_QLOGIC_ISP2200 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2300	\
	((PCI_PRODUCT_QLOGIC_ISP2300 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2312	\
	((PCI_PRODUCT_QLOGIC_ISP2312 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2322	\
	((PCI_PRODUCT_QLOGIC_ISP2322 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2422	\
	((PCI_PRODUCT_QLOGIC_ISP2422 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2432	\
	((PCI_PRODUCT_QLOGIC_ISP2432 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2532	\
	((PCI_PRODUCT_QLOGIC_ISP2532 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP6312	\
	((PCI_PRODUCT_QLOGIC_ISP6312 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP6322	\
	((PCI_PRODUCT_QLOGIC_ISP6322 << 16) | PCI_VENDOR_QLOGIC)

#define	IO_MAP_REG	0x10
#define	MEM_MAP_REG	0x14
#define	PCIR_ROMADDR	0x30

#define	PCI_DFLT_LTNCY	0x40
#define	PCI_DFLT_LNSZ	0x10

static int isp_pci_probe(device_t, cfdata_t, void *);
static void isp_pci_attach(device_t, device_t, void *);

struct isp_pcisoftc {
	struct ispsoftc		pci_isp;
	pci_chipset_tag_t	pci_pc;
	pcitag_t		pci_tag;
	bus_space_tag_t		pci_st;
	bus_space_handle_t	pci_sh;
	bus_dmamap_t		*pci_xfer_dmap;
	void *			pci_ih;
	int16_t			pci_poff[_NREG_BLKS];
};

CFATTACH_DECL_NEW(isp_pci, sizeof (struct isp_pcisoftc),
    isp_pci_probe, isp_pci_attach, NULL, NULL);

static int
isp_pci_probe(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;
	switch (pa->pa_id) {
#ifndef	ISP_DISABLE_1020_SUPPORT
	case PCI_QLOGIC_ISP:
		return (1);
#endif
#ifndef	ISP_DISABLE_1080_SUPPORT
	case PCI_QLOGIC_ISP1080:
	case PCI_QLOGIC_ISP1240:
	case PCI_QLOGIC_ISP1280:
		return (1);
#endif
#ifndef	ISP_DISABLE_12160_SUPPORT
	case PCI_QLOGIC_ISP10160:
	case PCI_QLOGIC_ISP12160:
		return (1);
#endif
#ifndef	ISP_DISABLE_2100_SUPPORT
	case PCI_QLOGIC_ISP2100:
		return (1);
#endif
#ifndef	ISP_DISABLE_2200_SUPPORT
	case PCI_QLOGIC_ISP2200:
		return (1);
#endif
#ifndef	ISP_DISABLE_2300_SUPPORT
	case PCI_QLOGIC_ISP2300:
	case PCI_QLOGIC_ISP2312:
	case PCI_QLOGIC_ISP6312:
#endif
#ifndef	ISP_DISABLE_2322_SUPPORT
	case PCI_QLOGIC_ISP2322:
	case PCI_QLOGIC_ISP6322:
		return (1);
#endif
#ifndef	ISP_DISABLE_2400_SUPPORT
	case PCI_QLOGIC_ISP2422:
	case PCI_QLOGIC_ISP2432:
	case PCI_QLOGIC_ISP2532:
		return (1);
#endif
	default:
		return (0);
	}
}

static void
isp_pci_attach(device_t parent, device_t self, void *aux)
{
	static const char nomem[] = "\n%s: no mem for sdparam table\n";
	uint32_t data, rev, linesz = PCI_DFLT_LNSZ;
	struct pci_attach_args *pa = aux;
	struct isp_pcisoftc *pcs = device_private(self);
	struct ispsoftc *isp = &pcs->pci_isp;
	bus_space_tag_t st, iot, memt;
	bus_space_handle_t sh, ioh, memh;
	pci_intr_handle_t ih;
	pcireg_t mem_type;
	const char *dstring;
	const char *intrstr;
	int ioh_valid, memh_valid;
	size_t mamt;
	char intrbuf[PCI_INTRSTR_LEN];

	isp->isp_osinfo.dev = self;

	ioh_valid = (pci_mapreg_map(pa, IO_MAP_REG,
	    PCI_MAPREG_TYPE_IO, 0,
	    &iot, &ioh, NULL, NULL) == 0);

	mem_type = pci_mapreg_type(pa->pa_pc, pa->pa_tag, MEM_MAP_REG);
	if (PCI_MAPREG_TYPE(mem_type) != PCI_MAPREG_TYPE_MEM) {
		memh_valid = 0;
	} else if (PCI_MAPREG_MEM_TYPE(mem_type) != PCI_MAPREG_MEM_TYPE_32BIT &&
	    PCI_MAPREG_MEM_TYPE(mem_type) != PCI_MAPREG_MEM_TYPE_64BIT) {
		memh_valid = 0;
	} else {
		memh_valid = (pci_mapreg_map(pa, MEM_MAP_REG, mem_type, 0,
		    &memt, &memh, NULL, NULL) == 0);
	}
	if (memh_valid) {
		st = memt;
		sh = memh;
	} else if (ioh_valid) {
		st = iot;
		sh = ioh;
	} else {
		printf(": unable to map device registers\n");
		return;
	}
	dstring = "\n";

	isp->isp_nchan = 1;
	mamt = 0;

	pcs->pci_st = st;
	pcs->pci_sh = sh;
	pcs->pci_pc = pa->pa_pc;
	pcs->pci_tag = pa->pa_tag;
	pcs->pci_poff[BIU_BLOCK >> _BLK_REG_SHFT] = BIU_REGS_OFF;
	pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] = PCI_MBOX_REGS_OFF;
	pcs->pci_poff[SXP_BLOCK >> _BLK_REG_SHFT] = PCI_SXP_REGS_OFF;
	pcs->pci_poff[RISC_BLOCK >> _BLK_REG_SHFT] = PCI_RISC_REGS_OFF;
	pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] = DMA_REGS_OFF;
	rev = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CLASS_REG) & 0xff;


#ifndef	ISP_DISABLE_1020_SUPPORT
	if (pa->pa_id == PCI_QLOGIC_ISP) {
		dstring = ": QLogic 1020 Fast Wide SCSI HBA\n";
		isp->isp_mdvec = &mdvec;
		isp->isp_type = ISP_HA_SCSI_UNKNOWN;
		mamt = sizeof (sdparam);
	}
#endif
#ifndef	ISP_DISABLE_1080_SUPPORT
	if (pa->pa_id == PCI_QLOGIC_ISP1080) {
		dstring = ": QLogic 1080 Ultra-2 Wide SCSI HBA\n";
		isp->isp_mdvec = &mdvec_1080;
		isp->isp_type = ISP_HA_SCSI_1080;
		mamt = sizeof (sdparam);
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] =
		    ISP1080_DMA_REGS_OFF;
	}
	if (pa->pa_id == PCI_QLOGIC_ISP1240) {
		dstring = ": QLogic Dual Channel Ultra Wide SCSI HBA\n";
		isp->isp_mdvec = &mdvec_1080;
		isp->isp_type = ISP_HA_SCSI_1240;
		isp->isp_nchan++;
		mamt = sizeof (sdparam) * 2;
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] =
		    ISP1080_DMA_REGS_OFF;
	}
	if (pa->pa_id == PCI_QLOGIC_ISP1280) {
		dstring = ": QLogic Dual Channel Ultra-2 Wide SCSI HBA\n";
		isp->isp_mdvec = &mdvec_1080;
		isp->isp_type = ISP_HA_SCSI_1280;
		isp->isp_nchan++;
		mamt = sizeof (sdparam) * 2;
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] =
		    ISP1080_DMA_REGS_OFF;
	}
#endif
#ifndef	ISP_DISABLE_12160_SUPPORT
	if (pa->pa_id == PCI_QLOGIC_ISP10160) {
		dstring = ": QLogic Ultra-3 Wide SCSI HBA\n";
		isp->isp_mdvec = &mdvec_12160;
		isp->isp_type = ISP_HA_SCSI_10160;
		mamt = sizeof (sdparam);
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] =
		    ISP1080_DMA_REGS_OFF;
	}
	if (pa->pa_id == PCI_QLOGIC_ISP12160) {
		dstring = ": QLogic Dual Channel Ultra-3 Wide SCSI HBA\n";
		isp->isp_mdvec = &mdvec_12160;
		isp->isp_type = ISP_HA_SCSI_12160;
		isp->isp_nchan++;
		mamt = sizeof (sdparam) * 2;
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] =
		    ISP1080_DMA_REGS_OFF;
	}
#endif
#ifndef	ISP_DISABLE_2100_SUPPORT
	if (pa->pa_id == PCI_QLOGIC_ISP2100) {
		dstring = ": QLogic FC-AL HBA\n";
		isp->isp_mdvec = &mdvec_2100;
		isp->isp_type = ISP_HA_FC_2100;
		mamt = sizeof (fcparam);
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] =
		    PCI_MBOX_REGS2100_OFF;
		if (rev < 3) {
			/*
			 * XXX: Need to get the actual revision
			 * XXX: number of the 2100 FB. At any rate,
			 * XXX: lower cache line size for early revision
			 * XXX; boards.
			 */
			linesz = 1;
		}
	}
#endif
#ifndef	ISP_DISABLE_2200_SUPPORT
	if (pa->pa_id == PCI_QLOGIC_ISP2200) {
		dstring = ": QLogic FC-AL and Fabric HBA\n";
		isp->isp_mdvec = &mdvec_2200;
		isp->isp_type = ISP_HA_FC_2200;
		mamt = sizeof (fcparam);
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] =
		    PCI_MBOX_REGS2100_OFF;
		data = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CLASS_REG);
	}
#endif
#ifndef	ISP_DISABLE_2300_SUPPORT
	if (pa->pa_id == PCI_QLOGIC_ISP2300 ||
	    pa->pa_id == PCI_QLOGIC_ISP2312 ||
	    pa->pa_id == PCI_QLOGIC_ISP6312) {
		isp->isp_mdvec = &mdvec_2300;
		if (pa->pa_id == PCI_QLOGIC_ISP2300 ||
		    pa->pa_id == PCI_QLOGIC_ISP6312) {
			dstring = ": QLogic FC-AL and 2Gbps Fabric HBA\n";
			isp->isp_type = ISP_HA_FC_2300;
		} else {
			dstring =
			    ": QLogic Dual Port FC-AL and 2Gbps Fabric HBA\n";
			isp->isp_port = pa->pa_function;
		}
		isp->isp_type = ISP_HA_FC_2312;
		mamt = sizeof (fcparam);
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] =
		    PCI_MBOX_REGS2300_OFF;
		data = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CLASS_REG);
	}
#endif
#ifndef	ISP_DISABLE_2322_SUPPORT
	if (pa->pa_id == PCI_QLOGIC_ISP2322 ||
	    pa->pa_id == PCI_QLOGIC_ISP6322) {
		isp->isp_mdvec = &mdvec_2322;
		dstring = ": QLogic FC-AL and 2Gbps Fabric PCI-E HBA\n";
		isp->isp_type = ISP_HA_FC_2322;
		isp->isp_port = pa->pa_function;
		mamt = sizeof (fcparam);
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] =
		    PCI_MBOX_REGS2300_OFF;
		data = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CLASS_REG);
	}
#endif
#ifndef	ISP_DISABLE_2400_SUPPORT
	if (pa->pa_id == PCI_QLOGIC_ISP2422 ||
	    pa->pa_id == PCI_QLOGIC_ISP2432) {
		isp->isp_mdvec = &mdvec_2400;
		if (pa->pa_id == PCI_QLOGIC_ISP2422) {
			dstring = ": QLogic FC-AL and 4Gbps Fabric PCI-X HBA\n";
		} else {
			dstring = ": QLogic FC-AL and 4Gbps Fabric PCI-E HBA\n";
		}
		isp->isp_type = ISP_HA_FC_2400;
		isp->isp_port = pa->pa_function;
		mamt = sizeof (fcparam);
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] =
		    PCI_MBOX_REGS2400_OFF;
		data = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CLASS_REG);
	}
	if (pa->pa_id == PCI_QLOGIC_ISP2532) {
		isp->isp_mdvec = &mdvec_2500;
		dstring = ": QLogic FC-AL and 8Gbps Fabric PCI-E HBA\n";
		isp->isp_type = ISP_HA_FC_2500;
		isp->isp_port = pa->pa_function;
		mamt = sizeof (fcparam);
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] =
		    PCI_MBOX_REGS2400_OFF;
		data = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CLASS_REG);
	}
#endif
	if (mamt == 0) {
		return;
	}

	isp->isp_param = malloc(mamt, M_DEVBUF, M_NOWAIT);
	if (isp->isp_param == NULL) {
		printf(nomem, device_xname(self));
		return;
	}
	memset(isp->isp_param, 0, mamt);
	mamt = sizeof (struct scsipi_channel) * isp->isp_nchan;
	isp->isp_osinfo.chan = malloc(mamt, M_DEVBUF, M_NOWAIT);
	if (isp->isp_osinfo.chan == NULL) {
		free(isp->isp_param, M_DEVBUF);
		printf(nomem, device_xname(self));
		return;
	}
	memset(isp->isp_osinfo.chan, 0, mamt);
	isp->isp_osinfo.adapter.adapt_nchannels = isp->isp_nchan;

	/*
	 * Set up logging levels.
	 */
#ifdef	ISP_LOGDEFAULT
	isp->isp_dblev = ISP_LOGDEFAULT;
#else
	isp->isp_dblev = ISP_LOGWARN|ISP_LOGERR;
	if (bootverbose)
		isp->isp_dblev |= ISP_LOGCONFIG|ISP_LOGINFO;
#ifdef	SCSIDEBUG
	isp->isp_dblev |= ISP_LOGDEBUG0|ISP_LOGDEBUG1|ISP_LOGDEBUG2;
#endif
#endif
	if (isp->isp_dblev & ISP_LOGCONFIG) {
		printf("\n");
	} else {
		printf("%s", dstring);
	}

	isp->isp_dmatag = pa->pa_dmat;
	isp->isp_revision = rev;

	/*
	 * Make sure that command register set sanely.
	 */
	data = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	data |= PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_INVALIDATE_ENABLE;

	/*
	 * Not so sure about these- but I think it's important that they get
	 * enabled......
	 */
	data |= PCI_COMMAND_PARITY_ENABLE | PCI_COMMAND_SERR_ENABLE;
	if (IS_2300(isp)) {	/* per QLogic errata */
		data &= ~PCI_COMMAND_INVALIDATE_ENABLE;
	}
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, data);

	/*
	 * Make sure that the latency timer, cache line size,
	 * and ROM is disabled.
	 */
	data = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG);
	data &= ~(PCI_LATTIMER_MASK << PCI_LATTIMER_SHIFT);
	data &= ~(PCI_CACHELINE_MASK << PCI_CACHELINE_SHIFT);
	data |= (PCI_DFLT_LTNCY	<< PCI_LATTIMER_SHIFT);
	data |= (linesz << PCI_CACHELINE_SHIFT);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG, data);

	data = pci_conf_read(pa->pa_pc, pa->pa_tag, PCIR_ROMADDR);
	data &= ~1;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCIR_ROMADDR, data);

	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		free(isp->isp_param, M_DEVBUF);
		free(isp->isp_osinfo.chan, M_DEVBUF);
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih, intrbuf, sizeof(intrbuf));
	if (intrstr == NULL)
		intrstr = "<I dunno>";
	pcs->pci_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO,
	    isp_pci_intr, isp);
	if (pcs->pci_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt at %s\n",
			intrstr);
		free(isp->isp_param, M_DEVBUF);
		free(isp->isp_osinfo.chan, M_DEVBUF);
		return;
	}

	printf("%s: interrupting at %s\n", device_xname(self), intrstr);

	isp->isp_confopts = device_cfdata(self)->cf_flags;
	ISP_LOCK(isp);
	isp_reset(isp, 1);
	if (isp->isp_state != ISP_RESETSTATE) {
		ISP_UNLOCK(isp);
		free(isp->isp_param, M_DEVBUF);
		free(isp->isp_osinfo.chan, M_DEVBUF);
		return;
	}
	isp_init(isp);
	if (isp->isp_state != ISP_INITSTATE) {
		isp_uninit(isp);
		ISP_UNLOCK(isp);
		free(isp->isp_param, M_DEVBUF);
		free(isp->isp_osinfo.chan, M_DEVBUF);
		return;
	}
	/*
	 * Do platform attach.
	 */
	ISP_UNLOCK(isp);
	isp_attach(isp);
}

#define	IspVirt2Off(a, x)	\
	(((struct isp_pcisoftc *)a)->pci_poff[((x) & _BLK_REG_MASK) >> \
	_BLK_REG_SHFT] + ((x) & 0xff))

#define	BXR2(pcs, off)		\
	bus_space_read_2(pcs->pci_st, pcs->pci_sh, off)
#define	BXW2(pcs, off, v)	\
	bus_space_write_2(pcs->pci_st, pcs->pci_sh, off, v)
#define	BXR4(pcs, off)		\
	bus_space_read_4(pcs->pci_st, pcs->pci_sh, off)
#define	BXW4(pcs, off, v)	\
	bus_space_write_4(pcs->pci_st, pcs->pci_sh, off, v)


static int
isp_pci_rd_debounced(struct ispsoftc *isp, int off, uint16_t *rp)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	uint16_t val0, val1;
	int i = 0;

	do {
		val0 = BXR2(pcs, IspVirt2Off(isp, off));
		val1 = BXR2(pcs, IspVirt2Off(isp, off));
	} while (val0 != val1 && ++i < 1000);
	if (val0 != val1) {
		return (1);
	}
	*rp = val0;
	return (0);
}

#if !defined(ISP_DISABLE_2100_SUPPORT) && \
	 !defined(ISP_DISABLE_2200_SUPPORT) && \
	 !defined(ISP_DISABLE_1020_SUPPORT) && \
	 !defined(ISP_DISABLE_1080_SUPPORT) && \
	 !defined(ISP_DISABLE_12160_SUPPORT)
static int
isp_pci_rd_isr(struct ispsoftc *isp, uint32_t *isrp,
    uint16_t *semap, uint16_t *mbp)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	uint16_t isr, sema;

	if (IS_2100(isp)) {
		if (isp_pci_rd_debounced(isp, BIU_ISR, &isr)) {
		    return (0);
		}
		if (isp_pci_rd_debounced(isp, BIU_SEMA, &sema)) {
		    return (0);
		}
	} else {
		isr = BXR2(pcs, IspVirt2Off(isp, BIU_ISR));
		sema = BXR2(pcs, IspVirt2Off(isp, BIU_SEMA));
	}
	isp_prt(isp, ISP_LOGDEBUG3, "ISR 0x%x SEMA 0x%x", isr, sema);
	isr &= INT_PENDING_MASK(isp);
	sema &= BIU_SEMA_LOCK;
	if (isr == 0 && sema == 0) {
		return (0);
	}
	*isrp = isr;
	if ((*semap = sema) != 0) {
		if (IS_2100(isp)) {
			if (isp_pci_rd_debounced(isp, OUTMAILBOX0, mbp)) {
				return (0);
			}
		} else {
			*mbp = BXR2(pcs, IspVirt2Off(isp, OUTMAILBOX0));
		}
	}
	return (1);
}
#endif

#if !(defined(ISP_DISABLE_2300_SUPPORT) || defined(ISP_DISABLE_2322_SUPPORT))
static int
isp_pci_rd_isr_2300(struct ispsoftc *isp, uint32_t *isrp,
    uint16_t *semap, uint16_t *mbox0p)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	uint32_t r2hisr;

	if (!(BXR2(pcs, IspVirt2Off(isp, BIU_ISR)) & BIU2100_ISR_RISC_INT)) {
		*isrp = 0;
		return (0);
	}
	r2hisr = bus_space_read_4(pcs->pci_st, pcs->pci_sh,
	    IspVirt2Off(pcs, BIU_R2HSTSLO));
	isp_prt(isp, ISP_LOGDEBUG3, "RISC2HOST ISR 0x%x", r2hisr);
	if ((r2hisr & BIU_R2HST_INTR) == 0) {
		*isrp = 0;
		return (0);
	}
	switch (r2hisr & BIU_R2HST_ISTAT_MASK) {
	case ISPR2HST_ROM_MBX_OK:
	case ISPR2HST_ROM_MBX_FAIL:
	case ISPR2HST_MBX_OK:
	case ISPR2HST_MBX_FAIL:
	case ISPR2HST_ASYNC_EVENT:
		*isrp = r2hisr & 0xffff;
		*mbox0p = (r2hisr >> 16);
		*semap = 1;
		return (1);
	case ISPR2HST_RIO_16:
		*isrp = r2hisr & 0xffff;
		*mbox0p = ASYNC_RIO16_1;
		*semap = 1;
		return (1);
	case ISPR2HST_FPOST:
		*isrp = r2hisr & 0xffff;
		*mbox0p = ASYNC_CMD_CMPLT;
		*semap = 1;
		return (1);
	case ISPR2HST_FPOST_CTIO:
		*isrp = r2hisr & 0xffff;
		*mbox0p = ASYNC_CTIO_DONE;
		*semap = 1;
		return (1);
	case ISPR2HST_RSPQ_UPDATE:
		*isrp = r2hisr & 0xffff;
		*mbox0p = 0;
		*semap = 0;
		return (1);
	default:
		return (0);
	}
}
#endif

#ifndef	ISP_DISABLE_2400_SUPPORT
static int
isp_pci_rd_isr_2400(ispsoftc_t *isp, uint32_t *isrp,
    uint16_t *semap, uint16_t *mbox0p)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	uint32_t r2hisr;

	r2hisr = BXR4(pcs, IspVirt2Off(pcs, BIU2400_R2HSTSLO));
	isp_prt(isp, ISP_LOGDEBUG3, "RISC2HOST ISR 0x%x", r2hisr);
	if ((r2hisr & BIU2400_R2HST_INTR) == 0) {
		*isrp = 0;
		return (0);
	}
	switch (r2hisr & BIU2400_R2HST_ISTAT_MASK) {
	case ISP2400R2HST_ROM_MBX_OK:
	case ISP2400R2HST_ROM_MBX_FAIL:
	case ISP2400R2HST_MBX_OK:
	case ISP2400R2HST_MBX_FAIL:
	case ISP2400R2HST_ASYNC_EVENT:
		*isrp = r2hisr & 0xffff;
		*mbox0p = (r2hisr >> 16);
		*semap = 1;
		return (1);
	case ISP2400R2HST_RSPQ_UPDATE:
	case ISP2400R2HST_ATIO_RSPQ_UPDATE:
	case ISP2400R2HST_ATIO_RQST_UPDATE:
		*isrp = r2hisr & 0xffff;
		*mbox0p = 0;
		*semap = 0;
		return (1);
	default:
		ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_CLEAR_RISC_INT);
		isp_prt(isp, ISP_LOGERR, "unknown interrupt 0x%x\n", r2hisr);
		return (0);
	}
}

static uint32_t
isp_pci_rd_reg_2400(ispsoftc_t *isp, int regoff)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	uint32_t rv;
	int block = regoff & _BLK_REG_MASK;

	switch (block) {
	case BIU_BLOCK:
		break;
	case MBOX_BLOCK:
		return (BXR2(pcs, IspVirt2Off(pcs, regoff)));
	case SXP_BLOCK:
		isp_prt(isp, ISP_LOGWARN, "SXP_BLOCK read at 0x%x", regoff);
		return (0xffffffff);
	case RISC_BLOCK:
		isp_prt(isp, ISP_LOGWARN, "RISC_BLOCK read at 0x%x", regoff);
		return (0xffffffff);
	case DMA_BLOCK:
		isp_prt(isp, ISP_LOGWARN, "DMA_BLOCK read at 0x%x", regoff);
		return (0xffffffff);
	default:
		isp_prt(isp, ISP_LOGWARN, "unknown block read at 0x%x", regoff);
		return (0xffffffff);
	}


	switch (regoff) {
	case BIU2400_FLASH_ADDR:
	case BIU2400_FLASH_DATA:
	case BIU2400_ICR:
	case BIU2400_ISR:
	case BIU2400_CSR:
	case BIU2400_REQINP:
	case BIU2400_REQOUTP:
	case BIU2400_RSPINP:
	case BIU2400_RSPOUTP:
	case BIU2400_PRI_REQINP:
	case BIU2400_PRI_REQOUTP:
	case BIU2400_ATIO_RSPINP:
	case BIU2400_ATIO_RSPOUTP:
	case BIU2400_HCCR:
	case BIU2400_GPIOD:
	case BIU2400_GPIOE:
	case BIU2400_HSEMA:
		rv = BXR4(pcs, IspVirt2Off(pcs, regoff));
		break;
	case BIU2400_R2HSTSLO:
		rv = BXR4(pcs, IspVirt2Off(pcs, regoff));
		break;
	case BIU2400_R2HSTSHI:
		rv = BXR4(pcs, IspVirt2Off(pcs, regoff)) >> 16;
		break;
	default:
		isp_prt(isp, ISP_LOGERR,
		    "isp_pci_rd_reg_2400: unknown offset %x", regoff);
		rv = 0xffffffff;
		break;
	}
	return (rv);
}

static void
isp_pci_wr_reg_2400(ispsoftc_t *isp, int regoff, uint32_t val)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	int block = regoff & _BLK_REG_MASK;

	switch (block) {
	case BIU_BLOCK:
		break;
	case MBOX_BLOCK:
		BXW2(pcs, IspVirt2Off(pcs, regoff), val);
		(void)BXR2(pcs, IspVirt2Off(pcs, regoff));
		return;
	case SXP_BLOCK:
		isp_prt(isp, ISP_LOGWARN, "SXP_BLOCK write at 0x%x", regoff);
		return;
	case RISC_BLOCK:
		isp_prt(isp, ISP_LOGWARN, "RISC_BLOCK write at 0x%x", regoff);
		return;
	case DMA_BLOCK:
		isp_prt(isp, ISP_LOGWARN, "DMA_BLOCK write at 0x%x", regoff);
		return;
	default:
		isp_prt(isp, ISP_LOGWARN, "unknown block write at 0x%x",
		    regoff);
		break;
	}

	switch (regoff) {
	case BIU2400_FLASH_ADDR:
	case BIU2400_FLASH_DATA:
	case BIU2400_ICR:
	case BIU2400_ISR:
	case BIU2400_CSR:
	case BIU2400_REQINP:
	case BIU2400_REQOUTP:
	case BIU2400_RSPINP:
	case BIU2400_RSPOUTP:
	case BIU2400_PRI_REQINP:
	case BIU2400_PRI_REQOUTP:
	case BIU2400_ATIO_RSPINP:
	case BIU2400_ATIO_RSPOUTP:
	case BIU2400_HCCR:
	case BIU2400_GPIOD:
	case BIU2400_GPIOE:
	case BIU2400_HSEMA:
		BXW4(pcs, IspVirt2Off(pcs, regoff), val);
		(void)BXR4(pcs, IspVirt2Off(pcs, regoff));
		break;
	default:
		isp_prt(isp, ISP_LOGERR,
		    "isp_pci_wr_reg_2400: bad offset 0x%x", regoff);
		break;
	}
}
#endif

static uint32_t
isp_pci_rd_reg(struct ispsoftc *isp, int regoff)
{
	uint32_t rv;
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	int oldconf = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oldconf = BXR2(pcs, IspVirt2Off(isp, BIU_CONF1));
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1),
		    oldconf | BIU_PCI_CONF1_SXP);
	}
	rv = BXR2(pcs, IspVirt2Off(isp, regoff));
	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1), oldconf);
	}
	return (rv);
}

static void
isp_pci_wr_reg(struct ispsoftc *isp, int regoff, uint32_t val)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	int oldconf = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oldconf = BXR2(pcs, IspVirt2Off(isp, BIU_CONF1));
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1),
		    oldconf | BIU_PCI_CONF1_SXP);
	}
	BXW2(pcs, IspVirt2Off(isp, regoff), val);
	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1), oldconf);
	}
}

#if !(defined(ISP_DISABLE_1080_SUPPORT) && defined(ISP_DISABLE_12160_SUPPORT))
static uint32_t
isp_pci_rd_reg_1080(struct ispsoftc *isp, int regoff)
{
	uint16_t rv, oc = 0;
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK ||
	    (regoff & _BLK_REG_MASK) == (SXP_BLOCK|SXP_BANK1_SELECT)) {
		uint16_t tc;
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oc = BXR2(pcs, IspVirt2Off(isp, BIU_CONF1));
		tc = oc & ~BIU_PCI1080_CONF1_DMA;
		if (regoff & SXP_BANK1_SELECT)
			tc |= BIU_PCI1080_CONF1_SXP1;
		else
			tc |= BIU_PCI1080_CONF1_SXP0;
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1), tc);
	} else if ((regoff & _BLK_REG_MASK) == DMA_BLOCK) {
		oc = BXR2(pcs, IspVirt2Off(isp, BIU_CONF1));
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1),
		    oc | BIU_PCI1080_CONF1_DMA);
	}
	rv = BXR2(pcs, IspVirt2Off(isp, regoff));
	if (oc) {
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1), oc);
	}
	return (rv);
}

static void
isp_pci_wr_reg_1080(struct ispsoftc *isp, int regoff, uint32_t val)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	int oc = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK ||
	    (regoff & _BLK_REG_MASK) == (SXP_BLOCK|SXP_BANK1_SELECT)) {
		uint16_t tc;
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oc = BXR2(pcs, IspVirt2Off(isp, BIU_CONF1));
		tc = oc & ~BIU_PCI1080_CONF1_DMA;
		if (regoff & SXP_BANK1_SELECT)
			tc |= BIU_PCI1080_CONF1_SXP1;
		else
			tc |= BIU_PCI1080_CONF1_SXP0;
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1), tc);
	} else if ((regoff & _BLK_REG_MASK) == DMA_BLOCK) {
		oc = BXR2(pcs, IspVirt2Off(isp, BIU_CONF1));
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1),
		    oc | BIU_PCI1080_CONF1_DMA);
	}
	BXW2(pcs, IspVirt2Off(isp, regoff), val);
	if (oc) {
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1), oc);
	}
}
#endif

static int
isp_pci_mbxdma(struct ispsoftc *isp)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *)isp;
	bus_dma_tag_t dmat = isp->isp_dmatag;
	bus_dma_segment_t sg;
	bus_size_t len, dbound;
	fcparam *fcp;
	int rs, i;

	if (isp->isp_rquest_dma)	/* been here before? */
		return (0);

	if (isp->isp_type <= ISP_HA_SCSI_1040B) {
		dbound = 1 << 24;
	} else {
		/*
		 * For 32-bit PCI DMA, the range is 32 bits or zero :-)
		 */
		dbound = 0;
	}
	len = isp->isp_maxcmds * sizeof (isp_hdl_t);
	isp->isp_xflist = (isp_hdl_t *) malloc(len, M_DEVBUF, M_WAITOK);
	if (isp->isp_xflist == NULL) {
		isp_prt(isp, ISP_LOGERR, "cannot malloc xflist array");
		return (1);
	}
	memset(isp->isp_xflist, 0, len);
	for (len = 0; len < isp->isp_maxcmds - 1; len++) {
		isp->isp_xflist[len].cmd = &isp->isp_xflist[len+1];
	}
	isp->isp_xffree = isp->isp_xflist;
	len = isp->isp_maxcmds * sizeof (bus_dmamap_t);
	pcs->pci_xfer_dmap = (bus_dmamap_t *) malloc(len, M_DEVBUF, M_WAITOK);
	if (pcs->pci_xfer_dmap == NULL) {
		free(isp->isp_xflist, M_DEVBUF);
		isp->isp_xflist = NULL;
		isp_prt(isp, ISP_LOGERR, "cannot malloc DMA map array");
		return (1);
	}
	for (i = 0; i < isp->isp_maxcmds; i++) {
		if (bus_dmamap_create(dmat, MAXPHYS, (MAXPHYS / PAGE_SIZE) + 1,
		    MAXPHYS, dbound, BUS_DMA_NOWAIT, &pcs->pci_xfer_dmap[i])) {
			isp_prt(isp, ISP_LOGERR, "cannot create DMA maps");
			break;
		}
	}
	if (i < isp->isp_maxcmds) {
		while (--i >= 0) {
			bus_dmamap_destroy(dmat, pcs->pci_xfer_dmap[i]);
		}
		free(isp->isp_xflist, M_DEVBUF);
		free(pcs->pci_xfer_dmap, M_DEVBUF);
		isp->isp_xflist = NULL;
		pcs->pci_xfer_dmap = NULL;
		return (1);
	}

	/*
	 * Allocate and map the request queue.
	 */
	len = ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp));
	if (bus_dmamem_alloc(dmat, len, PAGE_SIZE, 0, &sg, 1, &rs, 0)) {
		goto dmafail;
	}
 	if (bus_dmamem_map(isp->isp_dmatag, &sg, rs, len,
	    (void *)&isp->isp_rquest, BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) {
		goto dmafail;
	}
	if (bus_dmamap_create(dmat, len, 1, len, dbound, BUS_DMA_NOWAIT,
	    &isp->isp_rqdmap)) {
		goto dmafail;
	}
	if (bus_dmamap_load(dmat, isp->isp_rqdmap, isp->isp_rquest, len, NULL,
	    BUS_DMA_NOWAIT)) {
		goto dmafail;
	}
	isp->isp_rquest_dma = isp->isp_rqdmap->dm_segs[0].ds_addr;

	/*
	 * Allocate and map the result queue.
	 */
	len = ISP_QUEUE_SIZE(RESULT_QUEUE_LEN(isp));
	if (bus_dmamem_alloc(dmat, len, PAGE_SIZE, 0, &sg, 1, &rs,
	    BUS_DMA_NOWAIT)) {
		goto dmafail;
	}
	if (bus_dmamem_map(dmat, &sg, rs, len,
	    (void *)&isp->isp_result, BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) {
		goto dmafail;
	}
	if (bus_dmamap_create(dmat, len, 1, len, dbound, BUS_DMA_NOWAIT,
	    &isp->isp_rsdmap)) {
		goto dmafail;
	}
	if (bus_dmamap_load(dmat, isp->isp_rsdmap, isp->isp_result, len, NULL,
	    BUS_DMA_NOWAIT)) {
		goto dmafail;
	}
	isp->isp_result_dma = isp->isp_rsdmap->dm_segs[0].ds_addr;

	if (IS_SCSI(isp)) {
		return (0);
	}

	/*
	 * Allocate and map an FC scratch area
	 */
	fcp = isp->isp_param;
	len = ISP_FC_SCRLEN;
	if (bus_dmamem_alloc(dmat, len, sizeof (uint64_t), 0, &sg, 1, &rs,
	    BUS_DMA_NOWAIT)) {
		goto dmafail;
	}
	if (bus_dmamem_map(dmat, &sg, rs, len,
	    (void *)&fcp->isp_scratch, BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) {
		goto dmafail;
	}
	if (bus_dmamap_create(dmat, len, 1, len, dbound, BUS_DMA_NOWAIT,
	    &isp->isp_scdmap)) {
		goto dmafail;
	}
	if (bus_dmamap_load(dmat, isp->isp_scdmap, fcp->isp_scratch, len, NULL,
	    BUS_DMA_NOWAIT)) {
		goto dmafail;
	}
	fcp->isp_scdma = isp->isp_scdmap->dm_segs[0].ds_addr;
	return (0);
dmafail:
	isp_prt(isp, ISP_LOGERR, "mailbox DMA setup failure");
	for (i = 0; i < isp->isp_maxcmds; i++) {
		bus_dmamap_destroy(dmat, pcs->pci_xfer_dmap[i]);
	}
	free(isp->isp_xflist, M_DEVBUF);
	free(pcs->pci_xfer_dmap, M_DEVBUF);
	isp->isp_xflist = NULL;
	pcs->pci_xfer_dmap = NULL;
	return (1);
}

static int
isp_pci_dmasetup(struct ispsoftc *isp, struct scsipi_xfer *xs, void *arg)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *)isp;
	ispreq_t *rq = arg;
	bus_dmamap_t dmap;
	bus_dma_segment_t *dm_segs;
	uint32_t nsegs, hidx;
	isp_ddir_t ddir;

	hidx = isp_handle_index(isp, rq->req_handle);
	if (hidx == ISP_BAD_HANDLE_INDEX) {
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_COMPLETE);
	}
	dmap = pcs->pci_xfer_dmap[hidx];
	if (xs->datalen == 0) {
		ddir = ISP_NOXFR;
		nsegs = 0;
		dm_segs = NULL;
	 } else {
		int error;
		uint32_t flag, flg2;

		if (sizeof (bus_addr_t) > 4) {
			if (rq->req_header.rqs_entry_type == RQSTYPE_T2RQS) {
				rq->req_header.rqs_entry_type = RQSTYPE_T3RQS;
			} else if (rq->req_header.rqs_entry_type == RQSTYPE_REQUEST) {
				rq->req_header.rqs_entry_type = RQSTYPE_A64;
			}
		}

		if (xs->xs_control & XS_CTL_DATA_IN) {
			flg2 = BUS_DMASYNC_PREREAD;
			flag = BUS_DMA_READ;
			ddir = ISP_FROM_DEVICE;
		} else {
			flg2 = BUS_DMASYNC_PREWRITE;
			flag = BUS_DMA_WRITE;
			ddir = ISP_TO_DEVICE;
		}
		error = bus_dmamap_load(isp->isp_dmatag, dmap, xs->data, xs->datalen,
		    NULL, ((xs->xs_control & XS_CTL_NOSLEEP) ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK) | BUS_DMA_STREAMING | flag);
		if (error) {
			isp_prt(isp, ISP_LOGWARN, "unable to load DMA (%d)", error);
			XS_SETERR(xs, HBA_BOTCH);
			if (error == EAGAIN || error == ENOMEM) {
				return (CMD_EAGAIN);
			} else {
				return (CMD_COMPLETE);
			}
		}
		dm_segs = dmap->dm_segs;
		nsegs = dmap->dm_nsegs;
		bus_dmamap_sync(isp->isp_dmatag, dmap, 0, dmap->dm_mapsize, flg2);
	}

	if (isp_send_cmd(isp, rq, dm_segs, nsegs, xs->datalen, ddir) != CMD_QUEUED) {
		return (CMD_EAGAIN);
	} else {
		return (CMD_QUEUED);
	}
}

static int
isp_pci_intr(void *arg)
{
	uint32_t isr;
	uint16_t sema, mbox;
	struct ispsoftc *isp = arg;

	isp->isp_intcnt++;
	if (ISP_READ_ISR(isp, &isr, &sema, &mbox) == 0) {
		isp->isp_intbogus++;
		return (0);
	} else {
		isp->isp_osinfo.onintstack = 1;
		isp_intr(isp, isr, sema, mbox);
		isp->isp_osinfo.onintstack = 0;
		return (1);
	}
}

static void
isp_pci_dmateardown(struct ispsoftc *isp, XS_T *xs, uint32_t handle)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *)isp;
	uint32_t hidx;
	bus_dmamap_t dmap;

	hidx = isp_handle_index(isp, handle);
	if (hidx == ISP_BAD_HANDLE_INDEX) {
		isp_xs_prt(isp, xs, ISP_LOGERR, "bad handle on teardown");
		return;
	}
	dmap = pcs->pci_xfer_dmap[hidx];
	bus_dmamap_sync(isp->isp_dmatag, dmap, 0, dmap->dm_mapsize,
	    xs->xs_control & XS_CTL_DATA_IN ?
	    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(isp->isp_dmatag, dmap);
}

static void
isp_pci_reset0(ispsoftc_t *isp)
{
	ISP_DISABLE_INTS(isp);
}

static void
isp_pci_reset1(ispsoftc_t *isp)
{
	if (!IS_24XX(isp)) {
		/* Make sure the BIOS is disabled */
		isp_pci_wr_reg(isp, HCCR, PCI_HCCR_CMD_BIOS);
	}
	/* and enable interrupts */
	ISP_ENABLE_INTS(isp);
}

static void
isp_pci_dumpregs(struct ispsoftc *isp, const char *msg)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *)isp;
	if (msg)
		printf("%s: %s\n", device_xname(isp->isp_osinfo.dev), msg);
	if (IS_SCSI(isp))
		printf("    biu_conf1=%x", ISP_READ(isp, BIU_CONF1));
	else
		printf("    biu_csr=%x", ISP_READ(isp, BIU2100_CSR));
	printf(" biu_icr=%x biu_isr=%x biu_sema=%x ", ISP_READ(isp, BIU_ICR),
	    ISP_READ(isp, BIU_ISR), ISP_READ(isp, BIU_SEMA));
	printf("risc_hccr=%x\n", ISP_READ(isp, HCCR));


	if (IS_SCSI(isp)) {
		ISP_WRITE(isp, HCCR, HCCR_CMD_PAUSE);
		printf("    cdma_conf=%x cdma_sts=%x cdma_fifostat=%x\n",
			ISP_READ(isp, CDMA_CONF), ISP_READ(isp, CDMA_STATUS),
			ISP_READ(isp, CDMA_FIFO_STS));
		printf("    ddma_conf=%x ddma_sts=%x ddma_fifostat=%x\n",
			ISP_READ(isp, DDMA_CONF), ISP_READ(isp, DDMA_STATUS),
			ISP_READ(isp, DDMA_FIFO_STS));
		printf("    sxp_int=%x sxp_gross=%x sxp(scsi_ctrl)=%x\n",
			ISP_READ(isp, SXP_INTERRUPT),
			ISP_READ(isp, SXP_GROSS_ERR),
			ISP_READ(isp, SXP_PINS_CTRL));
		ISP_WRITE(isp, HCCR, HCCR_CMD_RELEASE);
	}
	printf("    mbox regs: %x %x %x %x %x\n",
	    ISP_READ(isp, OUTMAILBOX0), ISP_READ(isp, OUTMAILBOX1),
	    ISP_READ(isp, OUTMAILBOX2), ISP_READ(isp, OUTMAILBOX3),
	    ISP_READ(isp, OUTMAILBOX4));
	printf("    PCI Status Command/Status=%x\n",
	    pci_conf_read(pcs->pci_pc, pcs->pci_tag, PCI_COMMAND_STATUS_REG));
}

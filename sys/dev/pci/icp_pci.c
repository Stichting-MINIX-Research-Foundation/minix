/*	$NetBSD: icp_pci.c,v 1.22 2014/03/29 19:28:24 christos Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 * Copyright (c) 1999, 2000 Niklas Hallqvist.  All rights reserved.
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
 *	This product includes software developed by Niklas Hallqvist.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 * from OpenBSD: icp_pci.c,v 1.11 2001/06/12 15:40:30 niklas Exp
 */

/*
 * This driver would not have written if it was not for the hardware donations
 * from both ICP-Vortex and Öko.neT.  I want to thank them for their support.
 *
 * Re-worked for NetBSD by Andrew Doran.  Test hardware kindly supplied by
 * Intel.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: icp_pci.c,v 1.22 2014/03/29 19:28:24 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/buf.h>
#include <sys/endian.h>
#include <sys/conf.h>

#include <sys/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/icpreg.h>
#include <dev/ic/icpvar.h>

/* Product numbers for Fibre-Channel are greater than or equal to 0x200 */
#define	ICP_PCI_PRODUCT_FC	0x200

/* Mapping registers for various areas */
#define	ICP_PCI_DPMEM		0x10
#define	ICP_PCINEW_IOMEM	0x10
#define	ICP_PCINEW_IO		0x14
#define	ICP_PCINEW_DPMEM	0x18

/* PCI SRAM structure */
#define	ICP_MAGIC	0x00	/* u_int32_t, controller ID from BIOS */
#define	ICP_NEED_DEINIT	0x04	/* u_int16_t, switch between BIOS/driver */
#define	ICP_SWITCH_SUPPORT 0x06	/* u_int8_t, see ICP_NEED_DEINIT */
#define	ICP_OS_USED	0x10	/* u_int8_t [16], OS code per service */
#define	ICP_FW_MAGIC	0x3c	/* u_int8_t, controller ID from firmware */
#define	ICP_SRAM_SZ	0x40

/* DPRAM PCI controllers */
#define	ICP_DPR_IF	0x00	/* interface area */
#define	ICP_6SR		(0xff0 - ICP_SRAM_SZ)
#define	ICP_SEMA1	0xff1	/* volatile u_int8_t, command semaphore */
#define	ICP_IRQEN	0xff5	/* u_int8_t, board interrupts enable */
#define	ICP_EVENT	0xff8	/* u_int8_t, release event */
#define	ICP_IRQDEL	0xffc	/* u_int8_t, acknowledge board interrupt */
#define	ICP_DPRAM_SZ	0x1000

/* PLX register structure (new PCI controllers) */
#define	ICP_CFG_REG	0x00	/* u_int8_t, DPRAM cfg. (2: < 1MB, 0: any) */
#define	ICP_SEMA0_REG	0x40	/* volatile u_int8_t, command semaphore */
#define	ICP_SEMA1_REG	0x41	/* volatile u_int8_t, status semaphore */
#define	ICP_PLX_STATUS	0x44	/* volatile u_int16_t, command status */
#define	ICP_PLX_SERVICE	0x46	/* u_int16_t, service */
#define	ICP_PLX_INFO	0x48	/* u_int32_t [2], additional info */
#define	ICP_LDOOR_REG	0x60	/* u_int8_t, PCI to local doorbell */
#define	ICP_EDOOR_REG	0x64	/* volatile u_int8_t, local to PCI doorbell */
#define	ICP_CONTROL0	0x68	/* u_int8_t, control0 register (unused) */
#define	ICP_CONTROL1	0x69	/* u_int8_t, board interrupts enable */
#define	ICP_PLX_SZ	0x80

/* DPRAM new PCI controllers */
#define	ICP_IC		0x00	/* interface */
#define	ICP_PCINEW_6SR	(0x4000 - ICP_SRAM_SZ)
				/* SRAM structure */
#define	ICP_PCINEW_SZ	0x4000

/* i960 register structure (PCI MPR controllers) */
#define	ICP_MPR_SEMA0	0x10	/* volatile u_int8_t, command semaphore */
#define	ICP_MPR_SEMA1	0x12	/* volatile u_int8_t, status semaphore */
#define	ICP_MPR_STATUS	0x14	/* volatile u_int16_t, command status */
#define	ICP_MPR_SERVICE	0x16	/* u_int16_t, service */
#define	ICP_MPR_INFO	0x18	/* u_int32_t [2], additional info */
#define	ICP_MPR_LDOOR	0x20	/* u_int8_t, PCI to local doorbell */
#define	ICP_MPR_EDOOR	0x2c	/* volatile u_int8_t, locl to PCI doorbell */
#define	ICP_EDOOR_EN	0x34	/* u_int8_t, board interrupts enable */
#define	ICP_SEVERITY	0xefc	/* u_int8_t, event severity */
#define	ICP_EVT_BUF	0xf00	/* u_int8_t [256], event buffer */
#define	ICP_I960_SZ	0x1000

/* DPRAM PCI MPR controllers */
#define	ICP_I960R	0x00	/* 4KB i960 registers */
#define	ICP_MPR_IC	ICP_I960_SZ
				/* interface area */
#define	ICP_MPR_6SR	(ICP_I960_SZ + 0x3000 - ICP_SRAM_SZ)
				/* SRAM structure */
#define	ICP_MPR_SZ	0x4000

int	icp_pci_match(device_t, cfdata_t, void *);
void	icp_pci_attach(device_t, device_t, void *);
void	icp_pci_enable_intr(struct icp_softc *);
int	icp_pci_find_class(struct pci_attach_args *);

void	icp_pci_copy_cmd(struct icp_softc *, struct icp_ccb *);
u_int8_t icp_pci_get_status(struct icp_softc *);
void	icp_pci_intr(struct icp_softc *, struct icp_intr_ctx *);
void	icp_pci_release_event(struct icp_softc *, struct icp_ccb *);
void	icp_pci_set_sema0(struct icp_softc *);
int	icp_pci_test_busy(struct icp_softc *);

void	icp_pcinew_copy_cmd(struct icp_softc *, struct icp_ccb *);
u_int8_t icp_pcinew_get_status(struct icp_softc *);
void	icp_pcinew_intr(struct icp_softc *, struct icp_intr_ctx *);
void	icp_pcinew_release_event(struct icp_softc *, struct icp_ccb *);
void	icp_pcinew_set_sema0(struct icp_softc *);
int	icp_pcinew_test_busy(struct icp_softc *);

void	icp_mpr_copy_cmd(struct icp_softc *, struct icp_ccb *);
u_int8_t icp_mpr_get_status(struct icp_softc *);
void	icp_mpr_intr(struct icp_softc *, struct icp_intr_ctx *);
void	icp_mpr_release_event(struct icp_softc *, struct icp_ccb *);
void	icp_mpr_set_sema0(struct icp_softc *);
int	icp_mpr_test_busy(struct icp_softc *);

CFATTACH_DECL_NEW(icp_pci, sizeof(struct icp_softc),
    icp_pci_match, icp_pci_attach, NULL, NULL);

struct icp_pci_ident {
	u_short	gpi_vendor;
	u_short	gpi_product;
	u_short	gpi_class;
} const icp_pci_ident[] = {
	{ PCI_VENDOR_VORTEX,	PCI_PRODUCT_VORTEX_GDT_60x0,	ICP_PCI },
	{ PCI_VENDOR_VORTEX,	PCI_PRODUCT_VORTEX_GDT_6000B,	ICP_PCI },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_GDT_RAID1,	ICP_MPR },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_GDT_RAID2,	ICP_MPR },
};

int
icp_pci_find_class(struct pci_attach_args *pa)
{
	const struct icp_pci_ident *gpi, *maxgpi;

	gpi = icp_pci_ident;
	maxgpi = gpi + sizeof(icp_pci_ident) / sizeof(icp_pci_ident[0]);

	for (; gpi < maxgpi; gpi++)
		if (PCI_VENDOR(pa->pa_id) == gpi->gpi_vendor &&
		    PCI_PRODUCT(pa->pa_id) == gpi->gpi_product)
			return (gpi->gpi_class);

	/*
	 * ICP-Vortex only make RAID controllers, so we employ a heuristic
	 * to match unlisted boards.
	 */
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_VORTEX)
		return (PCI_PRODUCT(pa->pa_id) < 0x100 ? ICP_PCINEW : ICP_MPR);

	return (-1);
}

int
icp_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa;

	pa = aux;

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_I2O)
		return (0);

	return (icp_pci_find_class(pa) != -1);
}

void
icp_pci_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa;
	struct icp_softc *icp;
	bus_space_tag_t dpmemt, iomemt, iot;
	bus_space_handle_t dpmemh, iomemh, ioh;
	bus_addr_t dpmembase, iomembase, iobase;
	bus_size_t dpmemsize, iomemsize, iosize;
	u_int32_t status;
#define	DPMEM_MAPPED		1
#define	IOMEM_MAPPED		2
#define	IO_MAPPED		4
#define	INTR_ESTABLISHED	8
	int retries;
	u_int8_t protocol;
	pci_intr_handle_t ih;
	const char *intrstr;
	char intrbuf[PCI_INTRSTR_LEN];

	pa = aux;
	status = 0;
	icp = device_private(self);
	icp->icp_dv = self;
	icp->icp_class = icp_pci_find_class(pa);

	aprint_naive(": RAID controller\n");
	aprint_normal(": ");

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_VORTEX &&
	    PCI_PRODUCT(pa->pa_id) >= ICP_PCI_PRODUCT_FC)
		icp->icp_class |= ICP_FC;

	if (pci_mapreg_map(pa,
	    ICP_CLASS(icp) == ICP_PCINEW ? ICP_PCINEW_DPMEM : ICP_PCI_DPMEM,
	    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0, &dpmemt,
	    &dpmemh, &dpmembase, &dpmemsize)) {
		if (pci_mapreg_map(pa,
		    ICP_CLASS(icp) == ICP_PCINEW ? ICP_PCINEW_DPMEM :
		    ICP_PCI_DPMEM,
		    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT_1M, 0,
		    &dpmemt, &dpmemh, &dpmembase, &dpmemsize)) {
			aprint_error("cannot map DPMEM\n");
			goto bail_out;
		}
	}
	status |= DPMEM_MAPPED;
	icp->icp_dpmemt = dpmemt;
	icp->icp_dpmemh = dpmemh;
	icp->icp_dpmembase = dpmembase;
	icp->icp_dmat = pa->pa_dmat;

	/*
	 * The ICP_PCINEW series also has two other regions to map.
	 */
	if (ICP_CLASS(icp) == ICP_PCINEW) {
		if (pci_mapreg_map(pa, ICP_PCINEW_IOMEM, PCI_MAPREG_TYPE_MEM,
		    0, &iomemt, &iomemh, &iomembase, &iomemsize)) {
			aprint_error("cannot map memory mapped I/O ports\n");
			goto bail_out;
		}
		status |= IOMEM_MAPPED;

		if (pci_mapreg_map(pa, ICP_PCINEW_IO, PCI_MAPREG_TYPE_IO, 0,
		    &iot, &ioh, &iobase, &iosize)) {
			aprint_error("cannot map I/O ports\n");
			goto bail_out;
		}
		status |= IO_MAPPED;
		icp->icp_iot = iot;
		icp->icp_ioh = ioh;
		icp->icp_iobase = iobase;
	}

	switch (ICP_CLASS(icp)) {
	case ICP_PCI:
		bus_space_set_region_4(dpmemt, dpmemh, 0, 0,
		    ICP_DPR_IF_SZ >> 2);
		if (bus_space_read_1(dpmemt, dpmemh, 0) != 0) {
			aprint_error("cannot write to DPMEM\n");
			goto bail_out;
		}

#if 0
		/* disable board interrupts, deinit services */
		icph_writeb(0xff, &dp6_ptr->io.irqdel);
		icph_writeb(0x00, &dp6_ptr->io.irqen);
		icph_writeb(0x00, &dp6_ptr->u.ic.S_Status);
		icph_writeb(0x00, &dp6_ptr->u.ic.Cmd_Index);

		icph_writel(pcistr->dpmem, &dp6_ptr->u.ic.S_Info[0]);
		icph_writeb(0xff, &dp6_ptr->u.ic.S_Cmd_Indx);
		icph_writeb(0, &dp6_ptr->io.event);
		retries = INIT_RETRIES;
		icph_delay(20);
		while (icph_readb(&dp6_ptr->u.ic.S_Status) != 0xff) {
		  if (--retries == 0) {
		    printk("initialization error (DEINIT failed)\n");
		    icph_munmap(ha->brd);
		    return 0;
		  }
		  icph_delay(1);
		}
		prot_ver = (unchar)icph_readl(&dp6_ptr->u.ic.S_Info[0]);
		icph_writeb(0, &dp6_ptr->u.ic.S_Status);
		icph_writeb(0xff, &dp6_ptr->io.irqdel);
		if (prot_ver != PROTOCOL_VERSION) {
		  printk("illegal protocol version\n");
		  icph_munmap(ha->brd);
		  return 0;
		}

		ha->type = ICP_PCI;
		ha->ic_all_size = sizeof(dp6_ptr->u);

		/* special command to controller BIOS */
		icph_writel(0x00, &dp6_ptr->u.ic.S_Info[0]);
		icph_writel(0x00, &dp6_ptr->u.ic.S_Info[1]);
		icph_writel(0x01, &dp6_ptr->u.ic.S_Info[2]);
		icph_writel(0x00, &dp6_ptr->u.ic.S_Info[3]);
		icph_writeb(0xfe, &dp6_ptr->u.ic.S_Cmd_Indx);
		icph_writeb(0, &dp6_ptr->io.event);
		retries = INIT_RETRIES;
		icph_delay(20);
		while (icph_readb(&dp6_ptr->u.ic.S_Status) != 0xfe) {
		  if (--retries == 0) {
		    printk("initialization error\n");
		    icph_munmap(ha->brd);
		    return 0;
		  }
		  icph_delay(1);
		}
		icph_writeb(0, &dp6_ptr->u.ic.S_Status);
		icph_writeb(0xff, &dp6_ptr->io.irqdel);
#endif

		icp->icp_ic_all_size = ICP_DPRAM_SZ;

		icp->icp_copy_cmd = icp_pci_copy_cmd;
		icp->icp_get_status = icp_pci_get_status;
		icp->icp_intr = icp_pci_intr;
		icp->icp_release_event = icp_pci_release_event;
		icp->icp_set_sema0 = icp_pci_set_sema0;
		icp->icp_test_busy = icp_pci_test_busy;

		break;

	case ICP_PCINEW:
		bus_space_set_region_4(dpmemt, dpmemh, 0, 0,
		    ICP_DPR_IF_SZ >> 2);
		if (bus_space_read_1(dpmemt, dpmemh, 0) != 0) {
			aprint_error("cannot write to DPMEM\n");
			goto bail_out;
		}

#if 0
		/* disable board interrupts, deinit services */
		outb(0x00,PTR2USHORT(&ha->plx->control1));
		outb(0xff,PTR2USHORT(&ha->plx->edoor_reg));

		icph_writeb(0x00, &dp6c_ptr->u.ic.S_Status);
		icph_writeb(0x00, &dp6c_ptr->u.ic.Cmd_Index);

		icph_writel(pcistr->dpmem, &dp6c_ptr->u.ic.S_Info[0]);
		icph_writeb(0xff, &dp6c_ptr->u.ic.S_Cmd_Indx);

		outb(1,PTR2USHORT(&ha->plx->ldoor_reg));

		retries = INIT_RETRIES;
		icph_delay(20);
		while (icph_readb(&dp6c_ptr->u.ic.S_Status) != 0xff) {
		  if (--retries == 0) {
		    printk("initialization error (DEINIT failed)\n");
		    icph_munmap(ha->brd);
		    return 0;
		  }
		  icph_delay(1);
		}
		prot_ver = (unchar)icph_readl(&dp6c_ptr->u.ic.S_Info[0]);
		icph_writeb(0, &dp6c_ptr->u.ic.Status);
		if (prot_ver != PROTOCOL_VERSION) {
		  printk("illegal protocol version\n");
		  icph_munmap(ha->brd);
		  return 0;
		}

		ha->type = ICP_PCINEW;
		ha->ic_all_size = sizeof(dp6c_ptr->u);

		/* special command to controller BIOS */
		icph_writel(0x00, &dp6c_ptr->u.ic.S_Info[0]);
		icph_writel(0x00, &dp6c_ptr->u.ic.S_Info[1]);
		icph_writel(0x01, &dp6c_ptr->u.ic.S_Info[2]);
		icph_writel(0x00, &dp6c_ptr->u.ic.S_Info[3]);
		icph_writeb(0xfe, &dp6c_ptr->u.ic.S_Cmd_Indx);

		outb(1,PTR2USHORT(&ha->plx->ldoor_reg));

		retries = INIT_RETRIES;
		icph_delay(20);
		while (icph_readb(&dp6c_ptr->u.ic.S_Status) != 0xfe) {
		  if (--retries == 0) {
		    printk("initialization error\n");
		    icph_munmap(ha->brd);
		    return 0;
		  }
		  icph_delay(1);
		}
		icph_writeb(0, &dp6c_ptr->u.ic.S_Status);
#endif

		icp->icp_ic_all_size = ICP_PCINEW_SZ;

		icp->icp_copy_cmd = icp_pcinew_copy_cmd;
		icp->icp_get_status = icp_pcinew_get_status;
		icp->icp_intr = icp_pcinew_intr;
		icp->icp_release_event = icp_pcinew_release_event;
		icp->icp_set_sema0 = icp_pcinew_set_sema0;
		icp->icp_test_busy = icp_pcinew_test_busy;

		break;

	case ICP_MPR:
		bus_space_write_4(dpmemt, dpmemh, ICP_MPR_IC, ICP_MPR_MAGIC);
		if (bus_space_read_4(dpmemt, dpmemh, ICP_MPR_IC) !=
		    ICP_MPR_MAGIC) {
			aprint_error(
			    "cannot access DPMEM at 0x%lx (shadowed?)\n",
			    (u_long)dpmembase);
			goto bail_out;
		}

		/*
		 * XXX Here the Linux driver has a weird remapping logic I
		 * don't understand.  My controller does not need it, and I
		 * cannot see what purpose it serves, therefore I did not
		 * do anything similar.
		 */

		bus_space_set_region_4(dpmemt, dpmemh, ICP_I960_SZ, 0,
		    ICP_DPR_IF_SZ >> 2);

		/* Disable everything. */
		bus_space_write_1(dpmemt, dpmemh, ICP_EDOOR_EN,
		    bus_space_read_1(dpmemt, dpmemh, ICP_EDOOR_EN) | 4);
		bus_space_write_1(dpmemt, dpmemh, ICP_MPR_EDOOR, 0xff);
		bus_space_write_1(dpmemt, dpmemh, ICP_MPR_IC + ICP_S_STATUS,
		    0);
		bus_space_write_1(dpmemt, dpmemh, ICP_MPR_IC + ICP_CMD_INDEX,
		    0);

		bus_space_write_4(dpmemt, dpmemh, ICP_MPR_IC + ICP_S_INFO,
		    htole32(dpmembase));
		bus_space_write_1(dpmemt, dpmemh, ICP_MPR_IC + ICP_S_CMD_INDX,
		    0xff);
		bus_space_write_1(dpmemt, dpmemh, ICP_MPR_LDOOR, 1);

		DELAY(20);
		retries = 1000000;
		while (bus_space_read_1(dpmemt, dpmemh,
		    ICP_MPR_IC + ICP_S_STATUS) != 0xff) {
			if (--retries == 0) {
				aprint_error("DEINIT failed\n");
				goto bail_out;
			}
			DELAY(1);
		}

		protocol = (u_int8_t)bus_space_read_4(dpmemt, dpmemh,
		    ICP_MPR_IC + ICP_S_INFO);
		bus_space_write_1(dpmemt, dpmemh, ICP_MPR_IC + ICP_S_STATUS,
		    0);
		if (protocol != ICP_PROTOCOL_VERSION) {
		 	aprint_error("unsupported protocol %d\n", protocol);
			goto bail_out;
		}

		/* special commnd to controller BIOS */
		bus_space_write_4(dpmemt, dpmemh, ICP_MPR_IC + ICP_S_INFO, 0);
		bus_space_write_4(dpmemt, dpmemh,
		    ICP_MPR_IC + ICP_S_INFO + sizeof(u_int32_t), 0);
		bus_space_write_4(dpmemt, dpmemh,
		    ICP_MPR_IC + ICP_S_INFO + 2 * sizeof(u_int32_t), 1);
		bus_space_write_4(dpmemt, dpmemh,
		    ICP_MPR_IC + ICP_S_INFO + 3 * sizeof(u_int32_t), 0);
		bus_space_write_1(dpmemt, dpmemh, ICP_MPR_IC + ICP_S_CMD_INDX,
		    0xfe);
		bus_space_write_1(dpmemt, dpmemh, ICP_MPR_LDOOR, 1);

		DELAY(20);
		retries = 1000000;
		while (bus_space_read_1(dpmemt, dpmemh,
		    ICP_MPR_IC + ICP_S_STATUS) != 0xfe) {
			if (--retries == 0) {
				aprint_error("initialization error\n");
				goto bail_out;
			}
			DELAY(1);
		}

		bus_space_write_1(dpmemt, dpmemh, ICP_MPR_IC + ICP_S_STATUS,
		    0);

		icp->icp_copy_cmd = icp_mpr_copy_cmd;
		icp->icp_get_status = icp_mpr_get_status;
		icp->icp_intr = icp_mpr_intr;
		icp->icp_release_event = icp_mpr_release_event;
		icp->icp_set_sema0 = icp_mpr_set_sema0;
		icp->icp_test_busy = icp_mpr_test_busy;
		break;
	}

	if (pci_intr_map(pa, &ih)) {
		aprint_error("couldn't map interrupt\n");
		goto bail_out;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih, intrbuf, sizeof(intrbuf));
	icp->icp_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO, icp_intr, icp);
	if (icp->icp_ih == NULL) {
		aprint_error("couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		goto bail_out;
	}
	status |= INTR_ESTABLISHED;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INTEL)
		aprint_normal("Intel Storage RAID controller\n");
	else
		aprint_normal("ICP-Vortex RAID controller\n");

	icp->icp_pci_bus = pa->pa_bus;
	icp->icp_pci_device = pa->pa_device;
	icp->icp_pci_device_id = PCI_PRODUCT(pa->pa_id);
	icp->icp_pci_subdevice_id = pci_conf_read(pa->pa_pc, pa->pa_tag,
	    PCI_SUBSYS_ID_REG);

	if (icp_init(icp, intrstr))
		goto bail_out;

	icp_pci_enable_intr(icp);
	return;

 bail_out:
	if ((status & DPMEM_MAPPED) != 0)
		bus_space_unmap(dpmemt, dpmemh, dpmemsize);
	if ((status & IOMEM_MAPPED) != 0)
		bus_space_unmap(iomemt, iomemh, iomembase);
	if ((status & IO_MAPPED) != 0)
		bus_space_unmap(iot, ioh, iosize);
	if ((status & INTR_ESTABLISHED) != 0)
		pci_intr_disestablish(pa->pa_pc, icp->icp_ih);
}

/*
 * Enable interrupts.
 */
void
icp_pci_enable_intr(struct icp_softc *icp)
{

	switch (ICP_CLASS(icp)) {
	case ICP_PCI:
		bus_space_write_1(icp->icp_dpmemt, icp->icp_dpmemh, ICP_IRQDEL,
		    1);
		bus_space_write_1(icp->icp_dpmemt, icp->icp_dpmemh,
		    ICP_CMD_INDEX, 0);
		bus_space_write_1(icp->icp_dpmemt, icp->icp_dpmemh, ICP_IRQEN,
		    1);
		break;

	case ICP_PCINEW:
		bus_space_write_1(icp->icp_iot, icp->icp_ioh, ICP_EDOOR_REG,
		    0xff);
		bus_space_write_1(icp->icp_iot, icp->icp_ioh, ICP_CONTROL1, 3);
		break;

	case ICP_MPR:
		bus_space_write_1(icp->icp_dpmemt, icp->icp_dpmemh,
		    ICP_MPR_EDOOR, 0xff);
		bus_space_write_1(icp->icp_dpmemt, icp->icp_dpmemh, ICP_EDOOR_EN,
		    bus_space_read_1(icp->icp_dpmemt, icp->icp_dpmemh,
		    ICP_EDOOR_EN) & ~4);
		break;
	}
}

/*
 * "Old" PCI controller-specific functions.
 */

void
icp_pci_copy_cmd(struct icp_softc *icp, struct icp_ccb *ccb)
{

	/* XXX Not yet implemented */
}

u_int8_t
icp_pci_get_status(struct icp_softc *icp)
{

	/* XXX Not yet implemented */
	return (0);
}

void
icp_pci_intr(struct icp_softc *icp, struct icp_intr_ctx *ctx)
{

	/* XXX Not yet implemented */
}

void
icp_pci_release_event(struct icp_softc *icp,
    struct icp_ccb *ccb)
{

	/* XXX Not yet implemented */
}

void
icp_pci_set_sema0(struct icp_softc *icp)
{

	bus_space_write_1(icp->icp_dpmemt, icp->icp_dpmemh, ICP_SEMA0, 1);
}

int
icp_pci_test_busy(struct icp_softc *icp)
{

	/* XXX Not yet implemented */
	return (0);
}

/*
 * "New" PCI controller-specific functions.
 */

void
icp_pcinew_copy_cmd(struct icp_softc *icp,
    struct icp_ccb *ccb)
{

	/* XXX Not yet implemented */
}

u_int8_t
icp_pcinew_get_status(struct icp_softc *icp)
{

	/* XXX Not yet implemented */
	return (0);
}

void
icp_pcinew_intr(struct icp_softc *icp,
    struct icp_intr_ctx *ctx)
{

	/* XXX Not yet implemented */
}

void
icp_pcinew_release_event(struct icp_softc *icp,
    struct icp_ccb *ccb)
{

	/* XXX Not yet implemented */
}

void
icp_pcinew_set_sema0(struct icp_softc *icp)
{

	bus_space_write_1(icp->icp_iot, icp->icp_ioh, ICP_SEMA0_REG, 1);
}

int
icp_pcinew_test_busy(struct icp_softc *icp)
{

	/* XXX Not yet implemented */
	return (0);
}

/*
 * MPR PCI controller-specific functions
 */

void
icp_mpr_copy_cmd(struct icp_softc *icp, struct icp_ccb *ic)
{

	bus_space_write_2(icp->icp_dpmemt, icp->icp_dpmemh,
	    ICP_MPR_IC + ICP_COMM_QUEUE + 0 * ICP_COMM_Q_SZ + ICP_OFFSET,
	    ICP_DPR_CMD);
	bus_space_write_2(icp->icp_dpmemt, icp->icp_dpmemh,
	    ICP_MPR_IC + ICP_COMM_QUEUE + 0 * ICP_COMM_Q_SZ + ICP_SERV_ID,
	    ic->ic_service);
	bus_space_write_region_4(icp->icp_dpmemt, icp->icp_dpmemh,
	    ICP_MPR_IC + ICP_DPR_CMD, (u_int32_t *)&ic->ic_cmd,
	    ic->ic_cmdlen >> 2);
}

u_int8_t
icp_mpr_get_status(struct icp_softc *icp)
{

	return (bus_space_read_1(icp->icp_dpmemt, icp->icp_dpmemh,
	    ICP_MPR_EDOOR));
}

void
icp_mpr_intr(struct icp_softc *icp, struct icp_intr_ctx *ctx)
{

	if ((ctx->istatus & 0x80) != 0) {	/* error flag */
		ctx->istatus &= ~0x80;
		ctx->cmd_status = bus_space_read_2(icp->icp_dpmemt,
		    icp->icp_dpmemh, ICP_MPR_STATUS);
	} else
		ctx->cmd_status = ICP_S_OK;

	ctx->service = bus_space_read_2(icp->icp_dpmemt, icp->icp_dpmemh,
	    ICP_MPR_SERVICE);
	ctx->info = bus_space_read_4(icp->icp_dpmemt, icp->icp_dpmemh,
	    ICP_MPR_INFO);
	ctx->info2 = bus_space_read_4(icp->icp_dpmemt, icp->icp_dpmemh,
	    ICP_MPR_INFO + sizeof(u_int32_t));

	if (ctx->istatus == ICP_ASYNCINDEX) {
		if (ctx->service != ICP_SCREENSERVICE &&
		    (icp->icp_fw_vers & 0xff) >= 0x1a) {
			int i;

			icp->icp_evt.severity =
			    bus_space_read_1(icp->icp_dpmemt,
			        icp->icp_dpmemh, ICP_SEVERITY);
			for (i = 0;
			     i < sizeof(icp->icp_evt.event_string); i++) {
				icp->icp_evt.event_string[i] =
				    bus_space_read_1(icp->icp_dpmemt,
				    icp->icp_dpmemh, ICP_EVT_BUF + i);
				if (icp->icp_evt.event_string[i] == '\0')
					break;
			}
		}
	}

	bus_space_write_1(icp->icp_dpmemt, icp->icp_dpmemh, ICP_MPR_EDOOR,
	    0xff);
	bus_space_write_1(icp->icp_dpmemt, icp->icp_dpmemh, ICP_MPR_SEMA1, 0);
}

void
icp_mpr_release_event(struct icp_softc *icp, struct icp_ccb *ic)
{

	bus_space_write_1(icp->icp_dpmemt, icp->icp_dpmemh, ICP_MPR_LDOOR, 1);
}

void
icp_mpr_set_sema0(struct icp_softc *icp)
{

	bus_space_write_1(icp->icp_dpmemt, icp->icp_dpmemh, ICP_MPR_SEMA0, 1);
}

int
icp_mpr_test_busy(struct icp_softc *icp)
{

	return (bus_space_read_1(icp->icp_dpmemt, icp->icp_dpmemh,
	    ICP_MPR_SEMA0) & 1);
}

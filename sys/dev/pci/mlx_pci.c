/*	$NetBSD: mlx_pci.c,v 1.25 2014/03/29 19:28:25 christos Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
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

/*-
 * Copyright (c) 1999 Michael Smith
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from FreeBSD: mlx_pci.c,v 1.4.2.4 2000/10/28 10:48:09 msmith Exp
 */

/*
 * PCI front-end for the mlx(4) driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mlx_pci.c,v 1.25 2014/03/29 19:28:25 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/callout.h>

#include <machine/endian.h>
#include <sys/bus.h>

#include <dev/ic/mlxreg.h>
#include <dev/ic/mlxio.h>
#include <dev/ic/mlxvar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

static void	mlx_pci_attach(device_t, device_t, void *);
static int	mlx_pci_match(device_t, cfdata_t, void *);
static const struct mlx_pci_ident *mlx_pci_findmpi(struct pci_attach_args *);

static int	mlx_v3_submit(struct mlx_softc *, struct mlx_ccb *);
static int	mlx_v3_findcomplete(struct mlx_softc *, u_int *, u_int *);
static void	mlx_v3_intaction(struct mlx_softc *, int);
static int	mlx_v3_fw_handshake(struct mlx_softc *, int *, int *, int *);
#ifdef	MLX_RESET
static int	mlx_v3_reset(struct mlx_softc *);
#endif

static int	mlx_v4_submit(struct mlx_softc *, struct mlx_ccb *);
static int	mlx_v4_findcomplete(struct mlx_softc *, u_int *, u_int *);
static void	mlx_v4_intaction(struct mlx_softc *, int);
static int	mlx_v4_fw_handshake(struct mlx_softc *, int *, int *, int *);

static int	mlx_v5_submit(struct mlx_softc *, struct mlx_ccb *);
static int	mlx_v5_findcomplete(struct mlx_softc *, u_int *, u_int *);
static void	mlx_v5_intaction(struct mlx_softc *, int);
static int	mlx_v5_fw_handshake(struct mlx_softc *, int *, int *, int *);

static struct mlx_pci_ident {
	u_short	mpi_vendor;
	u_short	mpi_product;
	u_short	mpi_subvendor;
	u_short	mpi_subproduct;
	int	mpi_iftype;
} const mlx_pci_ident[] = {
	{
		PCI_VENDOR_MYLEX,
		PCI_PRODUCT_MYLEX_RAID_V2,
		0x0000,
		0x0000,
		2,
	},
	{
		PCI_VENDOR_MYLEX,
		PCI_PRODUCT_MYLEX_RAID_V3,
		0x0000,
		0x0000,
		3,
	},
	{
		PCI_VENDOR_MYLEX,
		PCI_PRODUCT_MYLEX_RAID_V4,
		0x0000,
		0x0000,
		4,
	},
	{
		PCI_VENDOR_DEC,
		PCI_PRODUCT_DEC_SWXCR,
		PCI_VENDOR_MYLEX,
		PCI_PRODUCT_MYLEX_RAID_V5,
		5,
	},
};

CFATTACH_DECL_NEW(mlx_pci, sizeof(struct mlx_softc),
    mlx_pci_match, mlx_pci_attach, NULL, NULL);

/*
 * Try to find a `mlx_pci_ident' entry corresponding to this board.
 */
static const struct mlx_pci_ident *
mlx_pci_findmpi(struct pci_attach_args *pa)
{
	const struct mlx_pci_ident *mpi, *maxmpi;
	pcireg_t reg;

	mpi = mlx_pci_ident;
	maxmpi = mpi + sizeof(mlx_pci_ident) / sizeof(mlx_pci_ident[0]);

	for (; mpi < maxmpi; mpi++) {
		if (PCI_VENDOR(pa->pa_id) != mpi->mpi_vendor ||
		    PCI_PRODUCT(pa->pa_id) != mpi->mpi_product)
			continue;

		if (mpi->mpi_subvendor == 0x0000)
			return (mpi);

		reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);

		if (PCI_VENDOR(reg) == mpi->mpi_subvendor &&
		    PCI_PRODUCT(reg) == mpi->mpi_subproduct)
			return (mpi);
	}

	return (NULL);
}

/*
 * Match a supported board.
 */
static int
mlx_pci_match(device_t parent, cfdata_t cfdata, void *aux)
{

	return (mlx_pci_findmpi(aux) != NULL);
}

/*
 * Attach a supported board.
 */
static void
mlx_pci_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa;
	struct mlx_softc *mlx;
	pci_chipset_tag_t pc;
	pci_intr_handle_t ih;
	bus_space_handle_t memh, ioh;
	bus_space_tag_t memt, iot;
	pcireg_t reg;
	const char *intrstr;
	int ior, memr, i;
	const struct mlx_pci_ident *mpi;
	char intrbuf[PCI_INTRSTR_LEN];

	mlx = device_private(self);
	pa = aux;
	pc = pa->pa_pc;
	mpi = mlx_pci_findmpi(aux);

	mlx->mlx_dv = self;
	mlx->mlx_dmat = pa->pa_dmat;
	mlx->mlx_ci.ci_iftype = mpi->mpi_iftype;

	printf(": Mylex RAID (v%d interface)\n", mpi->mpi_iftype);

	/*
	 * Map the PCI register window.
	 */
	memr = -1;
	ior = -1;

	for (i = 0x10; i <= 0x14; i += 4) {
		reg = pci_conf_read(pa->pa_pc, pa->pa_tag, i);

		if (PCI_MAPREG_TYPE(reg) == PCI_MAPREG_TYPE_IO) {
			if (ior == -1 && PCI_MAPREG_IO_SIZE(reg) != 0)
				ior = i;
		} else {
			if (memr == -1 && PCI_MAPREG_MEM_SIZE(reg) != 0)
				memr = i;
		}
	}

	if (memr != -1)
		if (pci_mapreg_map(pa, memr, PCI_MAPREG_TYPE_MEM, 0,
		    &memt, &memh, NULL, NULL))
			memr = -1;
	if (ior != -1)
		if (pci_mapreg_map(pa, ior, PCI_MAPREG_TYPE_IO, 0,
		    &iot, &ioh, NULL, NULL))
		    	ior = -1;

	if (memr != -1) {
		mlx->mlx_iot = memt;
		mlx->mlx_ioh = memh;
	} else if (ior != -1) {
		mlx->mlx_iot = iot;
		mlx->mlx_ioh = ioh;
	} else {
		aprint_error_dev(self, "can't map i/o or memory space\n");
		return;
	}

	/* Enable the device. */
	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    reg | PCI_COMMAND_MASTER_ENABLE);

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "can't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	mlx->mlx_ih = pci_intr_establish(pc, ih, IPL_BIO, mlx_intr, mlx);
	if (mlx->mlx_ih == NULL) {
		aprint_error_dev(self, "can't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}

	/* Select linkage based on controller interface type. */
	switch (mlx->mlx_ci.ci_iftype) {
	case 2:
	case 3:
		mlx->mlx_submit = mlx_v3_submit;
		mlx->mlx_findcomplete = mlx_v3_findcomplete;
		mlx->mlx_intaction = mlx_v3_intaction;
		mlx->mlx_fw_handshake = mlx_v3_fw_handshake;
#ifdef MLX_RESET
		mlx->mlx_reset = mlx_v3_reset;
#endif
		break;

	case 4:
		mlx->mlx_submit = mlx_v4_submit;
		mlx->mlx_findcomplete = mlx_v4_findcomplete;
		mlx->mlx_intaction = mlx_v4_intaction;
		mlx->mlx_fw_handshake = mlx_v4_fw_handshake;
		break;

	case 5:
		mlx->mlx_submit = mlx_v5_submit;
		mlx->mlx_findcomplete = mlx_v5_findcomplete;
		mlx->mlx_intaction = mlx_v5_intaction;
		mlx->mlx_fw_handshake = mlx_v5_fw_handshake;
		break;
	}

	mlx_init(mlx, intrstr);
}

/*
 * ================= V3 interface linkage =================
 */

/*
 * Try to give (mc) to the controller.  Returns 1 if successful, 0 on
 * failure (the controller is not ready to take a command).
 *
 * Must be called at splbio or in a fashion that prevents reentry.
 */
static int
mlx_v3_submit(struct mlx_softc *mlx, struct mlx_ccb *mc)
{

	/* Ready for our command? */
	if ((mlx_inb(mlx, MLX_V3REG_IDB) & MLX_V3_IDB_FULL) == 0) {
		/* Copy mailbox data to window. */
		bus_space_write_region_1(mlx->mlx_iot, mlx->mlx_ioh,
		    MLX_V3REG_MAILBOX, mc->mc_mbox, 13);
		bus_space_barrier(mlx->mlx_iot, mlx->mlx_ioh,
		    MLX_V3REG_MAILBOX, 13,
		    BUS_SPACE_BARRIER_WRITE);

		/* Post command. */
		mlx_outb(mlx, MLX_V3REG_IDB, MLX_V3_IDB_FULL);
		return (1);
	}

	return (0);
}

/*
 * See if a command has been completed, if so acknowledge its completion and
 * recover the slot number and status code.
 *
 * Must be called at splbio or in a fashion that prevents reentry.
 */
static int
mlx_v3_findcomplete(struct mlx_softc *mlx, u_int *slot, u_int *status)
{

	/* Status available? */
	if ((mlx_inb(mlx, MLX_V3REG_ODB) & MLX_V3_ODB_SAVAIL) != 0) {
		*slot = mlx_inb(mlx, MLX_V3REG_STATUS_IDENT);
		*status = mlx_inw(mlx, MLX_V3REG_STATUS);

		/* Acknowledge completion. */
		mlx_outb(mlx, MLX_V3REG_ODB, MLX_V3_ODB_SAVAIL);
		mlx_outb(mlx, MLX_V3REG_IDB, MLX_V3_IDB_SACK);
		return (1);
	}

	return (0);
}

/*
 * Enable/disable interrupts as requested. (No acknowledge required)
 *
 * Must be called at splbio or in a fashion that prevents reentry.
 */
static void
mlx_v3_intaction(struct mlx_softc *mlx, int action)
{

	mlx_outb(mlx, MLX_V3REG_IE, action != 0);
}

/*
 * Poll for firmware error codes during controller initialisation.
 *
 * Returns 0 if initialisation is complete, 1 if still in progress but no
 * error has been fetched, 2 if an error has been retrieved.
 */
static int
mlx_v3_fw_handshake(struct mlx_softc *mlx, int *error, int *param1, int *param2)
{
	u_int8_t fwerror;

	/* First time around, clear any hardware completion status. */
	if ((mlx->mlx_flags & MLXF_FW_INITTED) == 0) {
		mlx_outb(mlx, MLX_V3REG_IDB, MLX_V3_IDB_SACK);
		DELAY(1000);
		mlx->mlx_flags |= MLXF_FW_INITTED;
	}

	/* Init in progress? */
	if ((mlx_inb(mlx, MLX_V3REG_IDB) & MLX_V3_IDB_INIT_BUSY) == 0)
		return (0);

	/* Test error value. */
	fwerror = mlx_inb(mlx, MLX_V3REG_FWERROR);

	if ((fwerror & MLX_V3_FWERROR_PEND) == 0)
		return (1);

	/* Mask status pending bit, fetch status. */
	*error = fwerror & ~MLX_V3_FWERROR_PEND;
	*param1 = mlx_inb(mlx, MLX_V3REG_FWERROR_PARAM1);
	*param2 = mlx_inb(mlx, MLX_V3REG_FWERROR_PARAM2);

	/* Acknowledge. */
	mlx_outb(mlx, MLX_V3REG_FWERROR, 0);

	return (2);
}

#ifdef MLX_RESET
/*
 * Reset the controller.  Return non-zero on failure.
 */
static int
mlx_v3_reset(struct mlx_softc *mlx)
{
	int i;

	mlx_outb(mlx, MLX_V3REG_IDB, MLX_V3_IDB_SACK);
	delay(1000000);

	/* Wait up to 2 minutes for the bit to clear. */
	for (i = 120; i != 0; i--) {
		delay(1000000);
		if ((mlx_inb(mlx, MLX_V3REG_IDB) & MLX_V3_IDB_SACK) == 0)
			break;
	}
	if (i == 0) {
		/* ZZZ */
		printf("mlx0: SACK didn't clear\n");
		return (-1);
	}

	mlx_outb(mlx, MLX_V3REG_IDB, MLX_V3_IDB_RESET);

	/* Wait up to 5 seconds for the bit to clear. */
	for (i = 5; i != 0; i--) {
		delay(1000000);
		if ((mlx_inb(mlx, MLX_V3REG_IDB) & MLX_V3_IDB_RESET) == 0)
			break;
	}
	if (i == 0) {
		/* ZZZ */
		printf("mlx0: RESET didn't clear\n");
		return (-1);
	}

	return (0);
}
#endif	/* MLX_RESET */

/*
 * ================= V4 interface linkage =================
 */

/*
 * Try to give (mc) to the controller.  Returns 1 if successful, 0 on
 * failure (the controller is not ready to take a command).
 *
 * Must be called at splbio or in a fashion that prevents reentry.
 */
static int
mlx_v4_submit(struct mlx_softc *mlx, struct mlx_ccb *mc)
{

	/* Ready for our command? */
	if ((mlx_inl(mlx, MLX_V4REG_IDB) & MLX_V4_IDB_FULL) == 0) {
		/* Copy mailbox data to window. */
		bus_space_write_region_1(mlx->mlx_iot, mlx->mlx_ioh,
		    MLX_V4REG_MAILBOX, mc->mc_mbox, 13);
		bus_space_barrier(mlx->mlx_iot, mlx->mlx_ioh,
		    MLX_V4REG_MAILBOX, 13,
		    BUS_SPACE_BARRIER_WRITE);

		/* Post command. */
		mlx_outl(mlx, MLX_V4REG_IDB, MLX_V4_IDB_HWMBOX_CMD);
		return (1);
	}

	return (0);
}

/*
 * See if a command has been completed, if so acknowledge its completion and
 * recover the slot number and status code.
 *
 * Must be called at splbio or in a fashion that prevents reentry.
 */
static int
mlx_v4_findcomplete(struct mlx_softc *mlx, u_int *slot, u_int *status)
{

	/* Status available? */
	if ((mlx_inl(mlx, MLX_V4REG_ODB) & MLX_V4_ODB_HWSAVAIL) != 0) {
		*slot = mlx_inb(mlx, MLX_V4REG_STATUS_IDENT);
		*status = mlx_inw(mlx, MLX_V4REG_STATUS);

		/* Acknowledge completion. */
		mlx_outl(mlx, MLX_V4REG_ODB, MLX_V4_ODB_HWMBOX_ACK);
		mlx_outl(mlx, MLX_V4REG_IDB, MLX_V4_IDB_SACK);
		return (1);
	}

	return (0);
}

/*
 * Enable/disable interrupts as requested.
 *
 * Must be called at splbio or in a fashion that prevents reentry.
 */
static void
mlx_v4_intaction(struct mlx_softc *mlx, int action)
{
	u_int32_t ier;

	if (!action)
		ier = MLX_V4_IE_MASK | MLX_V4_IE_DISINT;
	else
		ier = MLX_V4_IE_MASK & ~MLX_V4_IE_DISINT;

	mlx_outl(mlx, MLX_V4REG_IE, ier);
}

/*
 * Poll for firmware error codes during controller initialisation.
 *
 * Returns 0 if initialisation is complete, 1 if still in progress but no
 * error has been fetched, 2 if an error has been retrieved.
 */
static int
mlx_v4_fw_handshake(struct mlx_softc *mlx, int *error, int *param1, int *param2)
{
	u_int8_t fwerror;

	/* First time around, clear any hardware completion status. */
	if ((mlx->mlx_flags & MLXF_FW_INITTED) == 0) {
		mlx_outl(mlx, MLX_V4REG_IDB, MLX_V4_IDB_SACK);
		DELAY(1000);
		mlx->mlx_flags |= MLXF_FW_INITTED;
	}

	/* Init in progress? */
	if ((mlx_inl(mlx, MLX_V4REG_IDB) & MLX_V4_IDB_INIT_BUSY) == 0)
		return (0);

	/* Test error value */
	fwerror = mlx_inb(mlx, MLX_V4REG_FWERROR);
	if ((fwerror & MLX_V4_FWERROR_PEND) == 0)
		return (1);

	/* Mask status pending bit, fetch status. */
	*error = fwerror & ~MLX_V4_FWERROR_PEND;
	*param1 = mlx_inb(mlx, MLX_V4REG_FWERROR_PARAM1);
	*param2 = mlx_inb(mlx, MLX_V4REG_FWERROR_PARAM2);

	/* Acknowledge. */
	mlx_outb(mlx, MLX_V4REG_FWERROR, 0);

	return (2);
}

/*
 * ================= V5 interface linkage =================
 */

/*
 * Try to give (mc) to the controller.  Returns 1 if successful, 0 on failure
 * (the controller is not ready to take a command).
 *
 * Must be called at splbio or in a fashion that prevents reentry.
 */
static int
mlx_v5_submit(struct mlx_softc *mlx, struct mlx_ccb *mc)
{

	/* Ready for our command? */
	if ((mlx_inb(mlx, MLX_V5REG_IDB) & MLX_V5_IDB_EMPTY) != 0) {
		/* Copy mailbox data to window. */
		bus_space_write_region_1(mlx->mlx_iot, mlx->mlx_ioh,
		    MLX_V5REG_MAILBOX, mc->mc_mbox, 13);
		bus_space_barrier(mlx->mlx_iot, mlx->mlx_ioh,
		    MLX_V5REG_MAILBOX, 13,
		    BUS_SPACE_BARRIER_WRITE);

		/* Post command */
		mlx_outb(mlx, MLX_V5REG_IDB, MLX_V5_IDB_HWMBOX_CMD);
		return (1);
	}

	return (0);
}

/*
 * See if a command has been completed, if so acknowledge its completion and
 * recover the slot number and status code.
 *
 * Must be called at splbio or in a fashion that prevents reentry.
 */
static int
mlx_v5_findcomplete(struct mlx_softc *mlx, u_int *slot, u_int *status)
{

	/* Status available? */
	if ((mlx_inb(mlx, MLX_V5REG_ODB) & MLX_V5_ODB_HWSAVAIL) != 0) {
		*slot = mlx_inb(mlx, MLX_V5REG_STATUS_IDENT);
		*status = mlx_inw(mlx, MLX_V5REG_STATUS);

		/* Acknowledge completion. */
		mlx_outb(mlx, MLX_V5REG_ODB, MLX_V5_ODB_HWMBOX_ACK);
		mlx_outb(mlx, MLX_V5REG_IDB, MLX_V5_IDB_SACK);
		return (1);
	}

	return (0);
}

/*
 * Enable/disable interrupts as requested.
 *
 * Must be called at splbio or in a fashion that prevents reentry.
 */
static void
mlx_v5_intaction(struct mlx_softc *mlx, int action)
{
	u_int8_t ier;

	if (!action)
		ier = 0xff & MLX_V5_IE_DISINT;
	else
		ier = 0xff & ~MLX_V5_IE_DISINT;

	mlx_outb(mlx, MLX_V5REG_IE, ier);
}

/*
 * Poll for firmware error codes during controller initialisation.
 *
 * Returns 0 if initialisation is complete, 1 if still in progress but no
 * error has been fetched, 2 if an error has been retrieved.
 */
static int
mlx_v5_fw_handshake(struct mlx_softc *mlx, int *error, int *param1, int *param2)
{
	u_int8_t fwerror;

	/* First time around, clear any hardware completion status. */
	if ((mlx->mlx_flags & MLXF_FW_INITTED) == 0) {
		mlx_outb(mlx, MLX_V5REG_IDB, MLX_V5_IDB_SACK);
		DELAY(1000);
		mlx->mlx_flags |= MLXF_FW_INITTED;
	}

	/* Init in progress? */
	if ((mlx_inb(mlx, MLX_V5REG_IDB) & MLX_V5_IDB_INIT_DONE) != 0)
		return (0);

	/* Test for error value. */
	fwerror = mlx_inb(mlx, MLX_V5REG_FWERROR);
	if ((fwerror & MLX_V5_FWERROR_PEND) == 0)
		return (1);

	/* Mask status pending bit, fetch status. */
	*error = fwerror & ~MLX_V5_FWERROR_PEND;
	*param1 = mlx_inb(mlx, MLX_V5REG_FWERROR_PARAM1);
	*param2 = mlx_inb(mlx, MLX_V5REG_FWERROR_PARAM2);

	/* Acknowledge. */
	mlx_outb(mlx, MLX_V5REG_FWERROR, 0xff);

	return (2);
}

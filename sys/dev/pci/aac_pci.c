/*	$NetBSD: aac_pci.c,v 1.36 2014/03/29 19:28:24 christos Exp $	*/

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

/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
 * Copyright (c) 2000 Niklas Hallqvist
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
 * from FreeBSD: aac_pci.c,v 1.1 2000/09/13 03:20:34 msmith Exp
 * via OpenBSD: aac_pci.c,v 1.7 2002/03/14 01:26:58 millert Exp
 */

/*
 * PCI front-end for the `aac' driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: aac_pci.c,v 1.36 2014/03/29 19:28:24 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#include <sys/bus.h>
#include <machine/endian.h>
#include <sys/intr.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/ic/aacreg.h>
#include <dev/ic/aacvar.h>

struct aac_pci_softc {
	struct aac_softc	sc_aac;
	pci_chipset_tag_t	sc_pc;
	pci_intr_handle_t	sc_ih;
};

/* i960Rx interface */
static int	aac_rx_get_fwstatus(struct aac_softc *);
static void	aac_rx_qnotify(struct aac_softc *, int);
static int	aac_rx_get_istatus(struct aac_softc *);
static void	aac_rx_clear_istatus(struct aac_softc *, int);
static void	aac_rx_set_mailbox(struct aac_softc *, u_int32_t, u_int32_t,
			   u_int32_t, u_int32_t, u_int32_t);
static uint32_t aac_rx_get_mailbox(struct aac_softc *, int);
static void	aac_rx_set_interrupts(struct aac_softc *, int);
static int	aac_rx_send_command(struct aac_softc *, struct aac_ccb *);
static int	aac_rx_get_outb_queue(struct aac_softc *);
static void	aac_rx_set_outb_queue(struct aac_softc *, int);

/* StrongARM interface */
static int	aac_sa_get_fwstatus(struct aac_softc *);
static void	aac_sa_qnotify(struct aac_softc *, int);
static int	aac_sa_get_istatus(struct aac_softc *);
static void	aac_sa_clear_istatus(struct aac_softc *, int);
static void	aac_sa_set_mailbox(struct aac_softc *, u_int32_t, u_int32_t,
			   u_int32_t, u_int32_t, u_int32_t);
static uint32_t aac_sa_get_mailbox(struct aac_softc *, int);
static void	aac_sa_set_interrupts(struct aac_softc *, int);

/* Rocket/MIPS interface */
static int	aac_rkt_get_fwstatus(struct aac_softc *);
static void	aac_rkt_qnotify(struct aac_softc *, int);
static int	aac_rkt_get_istatus(struct aac_softc *);
static void	aac_rkt_clear_istatus(struct aac_softc *, int);
static void	aac_rkt_set_mailbox(struct aac_softc *, u_int32_t, u_int32_t,
			   u_int32_t, u_int32_t, u_int32_t);
static uint32_t aac_rkt_get_mailbox(struct aac_softc *, int);
static void	aac_rkt_set_interrupts(struct aac_softc *, int);
static int	aac_rkt_send_command(struct aac_softc *, struct aac_ccb *);
static int	aac_rkt_get_outb_queue(struct aac_softc *);
static void	aac_rkt_set_outb_queue(struct aac_softc *, int);

static const struct aac_interface aac_rx_interface = {
	aac_rx_get_fwstatus,
	aac_rx_qnotify,
	aac_rx_get_istatus,
	aac_rx_clear_istatus,
	aac_rx_set_mailbox,
	aac_rx_get_mailbox,
	aac_rx_set_interrupts,
	aac_rx_send_command,
	aac_rx_get_outb_queue,
	aac_rx_set_outb_queue
};

static const struct aac_interface aac_sa_interface = {
	aac_sa_get_fwstatus,
	aac_sa_qnotify,
	aac_sa_get_istatus,
	aac_sa_clear_istatus,
	aac_sa_set_mailbox,
	aac_sa_get_mailbox,
	aac_sa_set_interrupts,
	NULL, NULL, NULL
};

static const struct aac_interface aac_rkt_interface = {
	aac_rkt_get_fwstatus,
	aac_rkt_qnotify,
	aac_rkt_get_istatus,
	aac_rkt_clear_istatus,
	aac_rkt_set_mailbox,
	aac_rkt_get_mailbox,
	aac_rkt_set_interrupts,
	aac_rkt_send_command,
	aac_rkt_get_outb_queue,
	aac_rkt_set_outb_queue
};

static struct aac_ident {
	u_short	vendor;
	u_short	device;
	u_short	subvendor;
	u_short	subdevice;
	u_short	hwif;
	u_short	quirks;
	const char	*prodstr;
} const aac_ident[] = {
	{
		PCI_VENDOR_DELL,
		PCI_PRODUCT_DELL_PERC_2SI,
		PCI_VENDOR_DELL,
		PCI_PRODUCT_DELL_PERC_2SI,
		AAC_HWIF_I960RX,
		0,
		"Dell PERC 2/Si"
	},
	{
		PCI_VENDOR_DELL,
		PCI_PRODUCT_DELL_PERC_3DI,
		PCI_VENDOR_DELL,
		PCI_PRODUCT_DELL_PERC_3DI,
		AAC_HWIF_I960RX,
		0,
		"Dell PERC 3/Di"
	},
	{
		PCI_VENDOR_DELL,
		PCI_PRODUCT_DELL_PERC_3DI,
		PCI_VENDOR_DELL,
		PCI_PRODUCT_DELL_PERC_3DI_SUB2,
		AAC_HWIF_I960RX,
		0,
		"Dell PERC 3/Di"
	},
	{
		PCI_VENDOR_DELL,
		PCI_PRODUCT_DELL_PERC_3DI,
		PCI_VENDOR_DELL,
		PCI_PRODUCT_DELL_PERC_3DI_SUB3,
		AAC_HWIF_I960RX,
		0,
		"Dell PERC 3/Di"
	},
	{
		PCI_VENDOR_DELL,
		PCI_PRODUCT_DELL_PERC_3DI_2,
		PCI_VENDOR_DELL,
		PCI_PRODUCT_DELL_PERC_3DI_2_SUB,
		AAC_HWIF_I960RX,
		0,
		"Dell PERC 3/Di"
	},
        {
		PCI_VENDOR_DELL,
		PCI_PRODUCT_DELL_PERC_3DI_3,
		PCI_VENDOR_DELL,
		PCI_PRODUCT_DELL_PERC_3DI_3_SUB,
		AAC_HWIF_I960RX,
		0,
		"Dell PERC 3/Di"
	},
	{
		PCI_VENDOR_DELL,
		PCI_PRODUCT_DELL_PERC_3DI_3,
		PCI_VENDOR_DELL,
		PCI_PRODUCT_DELL_PERC_3DI_3_SUB2,
		AAC_HWIF_I960RX,
		0,
		"Dell PERC 3/Di"
	},
	{
		PCI_VENDOR_DELL,
		PCI_PRODUCT_DELL_PERC_3DI_3,
		PCI_VENDOR_DELL,
		PCI_PRODUCT_DELL_PERC_3DI_3_SUB3,
		AAC_HWIF_I960RX,
		0,
		"Dell PERC 3/Di"
	},
	{
		PCI_VENDOR_DELL,
		PCI_PRODUCT_DELL_PERC_3SI,
		PCI_VENDOR_DELL,
		PCI_PRODUCT_DELL_PERC_3SI,
		AAC_HWIF_I960RX,
		0,
		"Dell PERC 3/Si"
	},
	{
		PCI_VENDOR_DELL,
		PCI_PRODUCT_DELL_PERC_3SI_2,
		PCI_VENDOR_DELL,
		PCI_PRODUCT_DELL_PERC_3SI_2_SUB,
		AAC_HWIF_I960RX,
		0,
		"Dell PERC 3/Si"
	},
	{
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_ASR2200S,
		PCI_VENDOR_DELL,
		PCI_PRODUCT_DELL_CERC_1_5,
		AAC_HWIF_I960RX,
		AAC_QUIRK_NO4GB,
		"Dell CERC SATA RAID 1.5/6ch"
	},
	{
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_AAC2622,
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_AAC2622,
		AAC_HWIF_I960RX,
		0,
		"Adaptec ADP-2622"
	},
	{
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_ASR2200S,
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_ASR2200S_SUB2M,
		AAC_HWIF_I960RX,
		AAC_QUIRK_NO4GB | AAC_QUIRK_256FIBS,
		"Adaptec ASR-2200S"
	},
	{
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_ASR2200S,
		PCI_VENDOR_DELL,
		PCI_PRODUCT_ADP2_ASR2200S_SUB2M,
		AAC_HWIF_I960RX,
		AAC_QUIRK_NO4GB | AAC_QUIRK_256FIBS,
		"Dell PERC 320/DC"
	},
	{
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_ASR2200S,
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_ASR2200S,
		AAC_HWIF_I960RX,
		AAC_QUIRK_NO4GB | AAC_QUIRK_256FIBS,
		"Adaptec ASR-2200S"
	},
	{
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_ASR2200S,
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_AAR2810SA,
		AAC_HWIF_I960RX,
		AAC_QUIRK_NO4GB,
		"Adaptec AAR-2810SA"
	},
	{
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_ASR2200S,
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_ASR2120S,
		AAC_HWIF_I960RX,
		AAC_QUIRK_NO4GB | AAC_QUIRK_256FIBS,
		"Adaptec ASR-2120S"
	},
	{
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_ASR2200S,
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_ASR2410SA,
		AAC_HWIF_I960RX,
		AAC_QUIRK_NO4GB,
		"Adaptec ASR-2410SA"
	},
	{
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_ASR2200S,
		PCI_VENDOR_HP,
		PCI_PRODUCT_ADP2_HP_M110_G2,
		AAC_HWIF_I960RX,
		AAC_QUIRK_NO4GB,
		"HP ML110 G2 (Adaptec ASR-2610SA)"
	},
	{
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_ASR2120S,
		PCI_VENDOR_IBM,
		PCI_PRODUCT_IBM_SERVERAID8K,
		AAC_HWIF_RKT,
		0,
		"IBM ServeRAID 8k"
	},
	{	PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_ASR2200S,
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_2405,
		AAC_HWIF_I960RX,
		0,
		"Adaptec RAID 2405"
	},
	{	PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_ASR2200S,
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_3405,
		AAC_HWIF_I960RX,
		0,
		"Adaptec RAID 3405"
	},
	{	PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_ASR2200S,
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_3805,
		AAC_HWIF_I960RX,
		0,
		"Adaptec RAID 3805"
	},
	{
		PCI_VENDOR_DEC,
		PCI_PRODUCT_DEC_21554,
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_AAC364,
		AAC_HWIF_STRONGARM,
		0,
		"Adaptec AAC-364"
	},
	{
		PCI_VENDOR_DEC,
		PCI_PRODUCT_DEC_21554,
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_ASR5400S,
		AAC_HWIF_STRONGARM,
		AAC_QUIRK_BROKEN_MMAP,
		"Adaptec ASR-5400S"
	},
	{
		PCI_VENDOR_DEC,
		PCI_PRODUCT_DEC_21554,
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_PERC_2QC,
		AAC_HWIF_STRONGARM,
		AAC_QUIRK_PERC2QC,
		"Dell PERC 2/QC"
	},
	{
		PCI_VENDOR_DEC,
		PCI_PRODUCT_DEC_21554,
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_PERC_3QC,
		AAC_HWIF_STRONGARM,
		0,
		"Dell PERC 3/QC"
	},
	{
		PCI_VENDOR_DEC,
		PCI_PRODUCT_DEC_21554,
		PCI_VENDOR_HP,
		PCI_PRODUCT_HP_NETRAID_4M,
		AAC_HWIF_STRONGARM,
		0,
		"HP NetRAID-4M"
	},
	{
		PCI_VENDOR_ADP2,
		PCI_PRODUCT_ADP2_ASR2200S,
		PCI_VENDOR_SUN,
		PCI_PRODUCT_ADP2_ASR2120S,
		AAC_HWIF_I960RX,
		0,
		"SG-XPCIESAS-R-IN"
	},
};

static const struct aac_ident *
aac_find_ident(struct pci_attach_args *pa)
{
	const struct aac_ident *m, *mm;
	u_int32_t subsysid;

	m = aac_ident;
	mm = aac_ident + (sizeof(aac_ident) / sizeof(aac_ident[0]));

	while (m < mm) {
		if (m->vendor == PCI_VENDOR(pa->pa_id) &&
		    m->device == PCI_PRODUCT(pa->pa_id)) {
			subsysid = pci_conf_read(pa->pa_pc, pa->pa_tag,
			    PCI_SUBSYS_ID_REG);
			if (m->subvendor == PCI_VENDOR(subsysid) &&
			    m->subdevice == PCI_PRODUCT(subsysid))
				return (m);
		}
		m++;
	}

	return (NULL);
}

static int
aac_pci_intr_set(struct aac_softc *sc, int (*hand)(void*), void *arg)
{
	struct aac_pci_softc	*pcisc;

	pcisc = (struct aac_pci_softc *) sc;

	pci_intr_disestablish(pcisc->sc_pc, sc->sc_ih);
	sc->sc_ih = pci_intr_establish(pcisc->sc_pc, pcisc->sc_ih,
				       IPL_BIO, hand, arg);
	if (sc->sc_ih == NULL) {
		return ENXIO;
	}
	return 0;
}

static int
aac_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa;

	pa = aux;

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_I2O)
		return (0);

	return (aac_find_ident(pa) != NULL);
}

static void
aac_pci_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa;
	pci_chipset_tag_t pc;
	struct aac_pci_softc *pcisc;
	struct aac_softc *sc;
	u_int16_t command;
	bus_addr_t membase;
	bus_size_t memsize;
	const char *intrstr;
	int state;
	const struct aac_ident *m;
	char intrbuf[PCI_INTRSTR_LEN];

	pa = aux;
	pc = pa->pa_pc;
	pcisc = device_private(self);
	pcisc->sc_pc = pc;
	sc = &pcisc->sc_aac;
	sc->sc_dv = self;
	state = 0;

	aprint_naive(": RAID controller\n");
	aprint_normal(": ");

	/*
	 * Verify that the adapter is correctly set up in PCI space.
	 */
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	command |= PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, command);
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	AAC_DPRINTF(AAC_D_MISC, ("pci command status reg 0x08x "));

	if ((command & PCI_COMMAND_MASTER_ENABLE) == 0) {
		aprint_error("can't enable bus-master feature\n");
		goto bail_out;
	}

	if ((command & PCI_COMMAND_MEM_ENABLE) == 0) {
		aprint_error("memory window not available\n");
		goto bail_out;
	}

	/*
	 * Map control/status registers.
	 */
	if (pci_mapreg_map(pa, PCI_MAPREG_START,
	    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0, &sc->sc_memt,
	    &sc->sc_memh, &membase, &memsize)) {
		aprint_error("can't find mem space\n");
		goto bail_out;
	}
	state++;

	if (pci_intr_map(pa, &pcisc->sc_ih)) {
		aprint_error("couldn't map interrupt\n");
		goto bail_out;
	}
	intrstr = pci_intr_string(pc, pcisc->sc_ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pc, pcisc->sc_ih, IPL_BIO, aac_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error("couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		goto bail_out;
	}
	state++;

	sc->sc_dmat = pa->pa_dmat;

	m = aac_find_ident(pa);
	aprint_normal("%s\n", m->prodstr);
	if (intrstr != NULL)
		aprint_normal_dev(self, "interrupting at %s\n",
		    intrstr);

	sc->sc_hwif = m->hwif;
	sc->sc_quirks = m->quirks;
	switch (sc->sc_hwif) {
		case AAC_HWIF_I960RX:
			AAC_DPRINTF(AAC_D_MISC,
			    ("set hardware up for i960Rx"));
			sc->sc_if = aac_rx_interface;
			break;

		case AAC_HWIF_STRONGARM:
			AAC_DPRINTF(AAC_D_MISC,
			    ("set hardware up for StrongARM"));
			sc->sc_if = aac_sa_interface;
			break;

		case AAC_HWIF_RKT:
			AAC_DPRINTF(AAC_D_MISC,
			    ("set hardware up for MIPS/Rocket"));
			sc->sc_if = aac_rkt_interface;
			break;
	}
	sc->sc_regsize = memsize;
	sc->sc_intr_set = aac_pci_intr_set;

	if (!aac_attach(sc))
		return;

 bail_out:
	if (state > 1)
		pci_intr_disestablish(pc, sc->sc_ih);
	if (state > 0)
		bus_space_unmap(sc->sc_memt, sc->sc_memh, memsize);
}

CFATTACH_DECL_NEW(aac_pci, sizeof(struct aac_pci_softc),
    aac_pci_match, aac_pci_attach, NULL, NULL);

/*
 * Read the current firmware status word.
 */
static int
aac_sa_get_fwstatus(struct aac_softc *sc)
{

	return (AAC_GETREG4(sc, AAC_SA_FWSTATUS));
}

static int
aac_rx_get_fwstatus(struct aac_softc *sc)
{

	return (AAC_GETREG4(sc, AAC_RX_FWSTATUS));
}

static int
aac_rkt_get_fwstatus(struct aac_softc *sc)
{

	return (AAC_GETREG4(sc, AAC_RKT_FWSTATUS));
}

/*
 * Notify the controller of a change in a given queue
 */

static void
aac_sa_qnotify(struct aac_softc *sc, int qbit)
{

	AAC_SETREG2(sc, AAC_SA_DOORBELL1_SET, qbit);
}

static void
aac_rx_qnotify(struct aac_softc *sc, int qbit)
{

	AAC_SETREG4(sc, AAC_RX_IDBR, qbit);
}

static void
aac_rkt_qnotify(struct aac_softc *sc, int qbit)
{

	AAC_SETREG4(sc, AAC_RKT_IDBR, qbit);
}

/*
 * Get the interrupt reason bits
 */
static int
aac_sa_get_istatus(struct aac_softc *sc)
{

	return (AAC_GETREG2(sc, AAC_SA_DOORBELL0));
}

static int
aac_rx_get_istatus(struct aac_softc *sc)
{

	return (AAC_GETREG4(sc, AAC_RX_ODBR));
}

static int
aac_rkt_get_istatus(struct aac_softc *sc)
{

	return (AAC_GETREG4(sc, AAC_RKT_ODBR));
}

/*
 * Clear some interrupt reason bits
 */
static void
aac_sa_clear_istatus(struct aac_softc *sc, int mask)
{

	AAC_SETREG2(sc, AAC_SA_DOORBELL0_CLEAR, mask);
}

static void
aac_rx_clear_istatus(struct aac_softc *sc, int mask)
{

	AAC_SETREG4(sc, AAC_RX_ODBR, mask);
}

static void
aac_rkt_clear_istatus(struct aac_softc *sc, int mask)
{

	AAC_SETREG4(sc, AAC_RKT_ODBR, mask);
}

/*
 * Populate the mailbox and set the command word
 */
static void
aac_sa_set_mailbox(struct aac_softc *sc, u_int32_t command,
		   u_int32_t arg0, u_int32_t arg1, u_int32_t arg2,
		   u_int32_t arg3)
{

	AAC_SETREG4(sc, AAC_SA_MAILBOX, command);
	AAC_SETREG4(sc, AAC_SA_MAILBOX + 4, arg0);
	AAC_SETREG4(sc, AAC_SA_MAILBOX + 8, arg1);
	AAC_SETREG4(sc, AAC_SA_MAILBOX + 12, arg2);
	AAC_SETREG4(sc, AAC_SA_MAILBOX + 16, arg3);
}

static void
aac_rx_set_mailbox(struct aac_softc *sc, u_int32_t command,
		   u_int32_t arg0, u_int32_t arg1, u_int32_t arg2,
		   u_int32_t arg3)
{

	AAC_SETREG4(sc, AAC_RX_MAILBOX, command);
	AAC_SETREG4(sc, AAC_RX_MAILBOX + 4, arg0);
	AAC_SETREG4(sc, AAC_RX_MAILBOX + 8, arg1);
	AAC_SETREG4(sc, AAC_RX_MAILBOX + 12, arg2);
	AAC_SETREG4(sc, AAC_RX_MAILBOX + 16, arg3);
}

static void
aac_rkt_set_mailbox(struct aac_softc *sc, u_int32_t command,
		    u_int32_t arg0, u_int32_t arg1, u_int32_t arg2,
		    u_int32_t arg3)
{

	AAC_SETREG4(sc, AAC_RKT_MAILBOX, command);
	AAC_SETREG4(sc, AAC_RKT_MAILBOX + 4, arg0);
	AAC_SETREG4(sc, AAC_RKT_MAILBOX + 8, arg1);
	AAC_SETREG4(sc, AAC_RKT_MAILBOX + 12, arg2);
	AAC_SETREG4(sc, AAC_RKT_MAILBOX + 16, arg3);
}

/*
 * Fetch the specified mailbox
 */
static uint32_t
aac_sa_get_mailbox(struct aac_softc *sc, int mb)
{

	return (AAC_GETREG4(sc, AAC_SA_MAILBOX + (mb * 4)));
}

static uint32_t
aac_rx_get_mailbox(struct aac_softc *sc, int mb)
{

	return (AAC_GETREG4(sc, AAC_RX_MAILBOX + (mb * 4)));
}

static uint32_t
aac_rkt_get_mailbox(struct aac_softc *sc, int mb)
{

	return (AAC_GETREG4(sc, AAC_RKT_MAILBOX + (mb * 4)));
}

/*
 * Set/clear interrupt masks
 */
static void
aac_sa_set_interrupts(struct aac_softc *sc, int enable)
{

	if (enable)
		AAC_SETREG2((sc), AAC_SA_MASK0_CLEAR, AAC_DB_INTERRUPTS);
	else
		AAC_SETREG2((sc), AAC_SA_MASK0_SET, ~0);
}

static void
aac_rx_set_interrupts(struct aac_softc *sc, int enable)
{

	if (enable) {
		if (sc->sc_quirks & AAC_QUIRK_NEW_COMM)
			AAC_SETREG4(sc, AAC_RX_OIMR, ~AAC_DB_INT_NEW_COMM);
		else
			AAC_SETREG4(sc, AAC_RX_OIMR, ~AAC_DB_INTERRUPTS);
	} else {
		AAC_SETREG4(sc, AAC_RX_OIMR, ~0);
	}
}

static void
aac_rkt_set_interrupts(struct aac_softc *sc, int enable)
{

	if (enable) {
		if (sc->sc_quirks & AAC_QUIRK_NEW_COMM)
			AAC_SETREG4(sc, AAC_RKT_OIMR, ~AAC_DB_INT_NEW_COMM);
		else
			AAC_SETREG4(sc, AAC_RKT_OIMR, ~AAC_DB_INTERRUPTS);
	} else {
		AAC_SETREG4(sc, AAC_RKT_OIMR, ~0);
	}
}

/*
 * New comm. interface: Send command functions
 */
static int
aac_rx_send_command(struct aac_softc *sc, struct aac_ccb *ac)
{
	u_int32_t	index, device;

	index = AAC_GETREG4(sc, AAC_RX_IQUE);
	if (index == 0xffffffffL)
		index = AAC_GETREG4(sc, AAC_RX_IQUE);
	if (index == 0xffffffffL)
		return index;
#ifdef notyet
	aac_enqueue_busy(ac);
#endif
	device = index;
	AAC_SETREG4(sc, device,
	    htole32((u_int32_t)(ac->ac_fibphys & 0xffffffffUL)));
	device += 4;
	if (sizeof(bus_addr_t) > 4) {
		AAC_SETREG4(sc, device,
		    htole32((u_int32_t)((u_int64_t)ac->ac_fibphys >> 32)));
	} else {
		AAC_SETREG4(sc, device, 0);
	}
	device += 4;
	AAC_SETREG4(sc, device, ac->ac_fib->Header.Size);
	AAC_SETREG4(sc, AAC_RX_IQUE, index);
	return 0;
}

static int
aac_rkt_send_command(struct aac_softc *sc, struct aac_ccb *ac)
{
	u_int32_t	index, device;

	index = AAC_GETREG4(sc, AAC_RKT_IQUE);
	if (index == 0xffffffffL)
		index = AAC_GETREG4(sc, AAC_RKT_IQUE);
	if (index == 0xffffffffL)
		return index;
#ifdef notyet
	aac_enqueue_busy(ac);
#endif
	device = index;
	AAC_SETREG4(sc, device,
	    htole32((u_int32_t)(ac->ac_fibphys & 0xffffffffUL)));
	device += 4;
	if (sizeof(bus_addr_t) > 4) {
		AAC_SETREG4(sc, device,
		    htole32((u_int32_t)((u_int64_t)ac->ac_fibphys >> 32)));
	} else {
		AAC_SETREG4(sc, device, 0);
	}
	device += 4;
	AAC_SETREG4(sc, device, ac->ac_fib->Header.Size);
	AAC_SETREG4(sc, AAC_RKT_IQUE, index);
	return 0;
}

/*
 * New comm. interface: get, set outbound queue index
 */
static int
aac_rx_get_outb_queue(struct aac_softc *sc)
{

	return AAC_GETREG4(sc, AAC_RX_OQUE);
}

static int
aac_rkt_get_outb_queue(struct aac_softc *sc)
{

	return AAC_GETREG4(sc, AAC_RKT_OQUE);
}

static void
aac_rx_set_outb_queue(struct aac_softc *sc, int index)
{

	AAC_SETREG4(sc, AAC_RX_OQUE, index);
}

static void
aac_rkt_set_outb_queue(struct aac_softc *sc, int index)
{

	AAC_SETREG4(sc, AAC_RKT_OQUE, index);
}

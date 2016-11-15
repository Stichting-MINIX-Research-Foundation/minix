/*	$NetBSD: mly.c,v 1.49 2014/07/25 08:10:38 dholland Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran, Thor Lancelot Simon, and Eric Haszlakiewicz.
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
 * Copyright (c) 2000, 2001 Michael Smith
 * Copyright (c) 2000 BSDi
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
 * from FreeBSD: mly.c,v 1.8 2001/07/14 00:12:22 msmith Exp
 */

/*
 * Driver for the Mylex AcceleRAID and eXtremeRAID family with v6 firmware.
 *
 * TODO:
 *
 * o Make mly->mly_btl a hash, then MLY_BTL_RESCAN becomes a SIMPLEQ.
 * o Handle FC and multiple LUNs.
 * o Fix mmbox usage.
 * o Fix transfer speed fudge.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mly.c,v 1.49 2014/07/25 08:10:38 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/buf.h>
#include <sys/endian.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/scsiio.h>
#include <sys/kthread.h>
#include <sys/kauth.h>

#include <sys/bus.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/mlyreg.h>
#include <dev/pci/mlyio.h>
#include <dev/pci/mlyvar.h>
#include <dev/pci/mly_tables.h>

static void	mly_attach(device_t, device_t, void *);
static int	mly_match(device_t, cfdata_t, void *);
static const	struct mly_ident *mly_find_ident(struct pci_attach_args *);
static int	mly_fwhandshake(struct mly_softc *);
static int	mly_flush(struct mly_softc *);
static int	mly_intr(void *);
static void	mly_shutdown(void *);

static int	mly_alloc_ccbs(struct mly_softc *);
static void	mly_check_event(struct mly_softc *);
static void	mly_complete_event(struct mly_softc *, struct mly_ccb *);
static void	mly_complete_rescan(struct mly_softc *, struct mly_ccb *);
static int	mly_dmamem_alloc(struct mly_softc *, int, bus_dmamap_t *,
				 void **, bus_addr_t *, bus_dma_segment_t *);
static void	mly_dmamem_free(struct mly_softc *, int, bus_dmamap_t,
				void *, bus_dma_segment_t *);
static int	mly_enable_mmbox(struct mly_softc *);
static void	mly_fetch_event(struct mly_softc *);
static int	mly_get_controllerinfo(struct mly_softc *);
static int	mly_get_eventstatus(struct mly_softc *);
static int	mly_ioctl(struct mly_softc *, struct mly_cmd_ioctl *,
			  void **, size_t, void *, size_t *);
static void	mly_padstr(char *, const char *, int);
static void	mly_process_event(struct mly_softc *, struct mly_event *);
static void	mly_release_ccbs(struct mly_softc *);
static int	mly_scan_btl(struct mly_softc *, int, int);
static void	mly_scan_channel(struct mly_softc *, int);
static void	mly_thread(void *);

static int	mly_ccb_alloc(struct mly_softc *, struct mly_ccb **);
static void	mly_ccb_complete(struct mly_softc *, struct mly_ccb *);
static void	mly_ccb_enqueue(struct mly_softc *, struct mly_ccb *);
static void	mly_ccb_free(struct mly_softc *, struct mly_ccb *);
static int	mly_ccb_map(struct mly_softc *, struct mly_ccb *);
static int	mly_ccb_poll(struct mly_softc *, struct mly_ccb *, int);
static int	mly_ccb_submit(struct mly_softc *, struct mly_ccb *);
static void	mly_ccb_unmap(struct mly_softc *, struct mly_ccb *);
static int	mly_ccb_wait(struct mly_softc *, struct mly_ccb *, int);

static void	mly_get_xfer_mode(struct mly_softc *, int,
				  struct scsipi_xfer_mode *);
static void	mly_scsipi_complete(struct mly_softc *, struct mly_ccb *);
static int	mly_scsipi_ioctl(struct scsipi_channel *, u_long, void *,
				 int, struct proc *);
static void	mly_scsipi_minphys(struct buf *);
static void	mly_scsipi_request(struct scsipi_channel *,
				   scsipi_adapter_req_t, void *);

static int	mly_user_command(struct mly_softc *, struct mly_user_command *);
static int	mly_user_health(struct mly_softc *, struct mly_user_health *);

extern struct	cfdriver mly_cd;

CFATTACH_DECL_NEW(mly, sizeof(struct mly_softc),
    mly_match, mly_attach, NULL, NULL);

dev_type_open(mlyopen);
dev_type_close(mlyclose);
dev_type_ioctl(mlyioctl);

const struct cdevsw mly_cdevsw = {
	.d_open = mlyopen,
	.d_close = mlyclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = mlyioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

static struct mly_ident {
	u_short	vendor;
	u_short	product;
	u_short	subvendor;
	u_short	subproduct;
	int	hwif;
	const char	*desc;
} const mly_ident[] = {
	{
		PCI_VENDOR_MYLEX,
		PCI_PRODUCT_MYLEX_EXTREMERAID,
		PCI_VENDOR_MYLEX,
		0x0040,
		MLY_HWIF_STRONGARM,
		"eXtremeRAID 2000"
	},
	{
		PCI_VENDOR_MYLEX,
		PCI_PRODUCT_MYLEX_EXTREMERAID,
		PCI_VENDOR_MYLEX,
		0x0030,
		MLY_HWIF_STRONGARM,
		"eXtremeRAID 3000"
	},
	{
		PCI_VENDOR_MYLEX,
		PCI_PRODUCT_MYLEX_ACCELERAID,
		PCI_VENDOR_MYLEX,
		0x0050,
		MLY_HWIF_I960RX,
		"AcceleRAID 352"
	},
	{
		PCI_VENDOR_MYLEX,
		PCI_PRODUCT_MYLEX_ACCELERAID,
		PCI_VENDOR_MYLEX,
		0x0052,
		MLY_HWIF_I960RX,
		"AcceleRAID 170"
	},
	{
		PCI_VENDOR_MYLEX,
		PCI_PRODUCT_MYLEX_ACCELERAID,
		PCI_VENDOR_MYLEX,
		0x0054,
		MLY_HWIF_I960RX,
		"AcceleRAID 160"
	},
};

static void	*mly_sdh;

/*
 * Try to find a `mly_ident' entry corresponding to this board.
 */
static const struct mly_ident *
mly_find_ident(struct pci_attach_args *pa)
{
	const struct mly_ident *mpi, *maxmpi;
	pcireg_t reg;

	mpi = mly_ident;
	maxmpi = mpi + sizeof(mly_ident) / sizeof(mly_ident[0]);

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_I2O)
		return (NULL);

	for (; mpi < maxmpi; mpi++) {
		if (PCI_VENDOR(pa->pa_id) != mpi->vendor ||
		    PCI_PRODUCT(pa->pa_id) != mpi->product)
			continue;

		if (mpi->subvendor == 0x0000)
			return (mpi);

		reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);

		if (PCI_VENDOR(reg) == mpi->subvendor &&
		    PCI_PRODUCT(reg) == mpi->subproduct)
			return (mpi);
	}

	return (NULL);
}

/*
 * Match a supported board.
 */
static int
mly_match(device_t parent, cfdata_t cfdata, void *aux)
{

	return (mly_find_ident(aux) != NULL);
}

/*
 * Attach a supported board.
 */
static void
mly_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa;
	struct mly_softc *mly;
	struct mly_ioctl_getcontrollerinfo *mi;
	const struct mly_ident *ident;
	pci_chipset_tag_t pc;
	pci_intr_handle_t ih;
	bus_space_handle_t memh, ioh;
	bus_space_tag_t memt, iot;
	pcireg_t reg;
	const char *intrstr;
	int ior, memr, i, rv, state;
	struct scsipi_adapter *adapt;
	struct scsipi_channel *chan;
	char intrbuf[PCI_INTRSTR_LEN];

	mly = device_private(self);
	mly->mly_dv = self;
	pa = aux;
	pc = pa->pa_pc;
	ident = mly_find_ident(pa);
	state = 0;

	mly->mly_dmat = pa->pa_dmat;
	mly->mly_hwif = ident->hwif;

	printf(": Mylex %s\n", ident->desc);

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
		mly->mly_iot = memt;
		mly->mly_ioh = memh;
	} else if (ior != -1) {
		mly->mly_iot = iot;
		mly->mly_ioh = ioh;
	} else {
		aprint_error_dev(self, "can't map i/o or memory space\n");
		return;
	}

	/*
	 * Enable the device.
	 */
	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    reg | PCI_COMMAND_MASTER_ENABLE);

	/*
	 * Map and establish the interrupt.
	 */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "can't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	mly->mly_ih = pci_intr_establish(pc, ih, IPL_BIO, mly_intr, mly);
	if (mly->mly_ih == NULL) {
		aprint_error_dev(self, "can't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}

	if (intrstr != NULL)
		aprint_normal_dev(self, "interrupting at %s\n",
		    intrstr);

	/*
	 * Take care of interface-specific tasks.
	 */
	switch (mly->mly_hwif) {
	case MLY_HWIF_I960RX:
		mly->mly_doorbell_true = 0x00;
		mly->mly_cmd_mailbox = MLY_I960RX_COMMAND_MAILBOX;
		mly->mly_status_mailbox = MLY_I960RX_STATUS_MAILBOX;
		mly->mly_idbr = MLY_I960RX_IDBR;
		mly->mly_odbr = MLY_I960RX_ODBR;
		mly->mly_error_status = MLY_I960RX_ERROR_STATUS;
		mly->mly_interrupt_status = MLY_I960RX_INTERRUPT_STATUS;
		mly->mly_interrupt_mask = MLY_I960RX_INTERRUPT_MASK;
		break;

	case MLY_HWIF_STRONGARM:
		mly->mly_doorbell_true = 0xff;
		mly->mly_cmd_mailbox = MLY_STRONGARM_COMMAND_MAILBOX;
		mly->mly_status_mailbox = MLY_STRONGARM_STATUS_MAILBOX;
		mly->mly_idbr = MLY_STRONGARM_IDBR;
		mly->mly_odbr = MLY_STRONGARM_ODBR;
		mly->mly_error_status = MLY_STRONGARM_ERROR_STATUS;
		mly->mly_interrupt_status = MLY_STRONGARM_INTERRUPT_STATUS;
		mly->mly_interrupt_mask = MLY_STRONGARM_INTERRUPT_MASK;
		break;
	}

	/*
	 * Allocate and map the scatter/gather lists.
	 */
	rv = mly_dmamem_alloc(mly, MLY_SGL_SIZE * MLY_MAX_CCBS,
	    &mly->mly_sg_dmamap, (void **)&mly->mly_sg,
	    &mly->mly_sg_busaddr, &mly->mly_sg_seg);
	if (rv) {
		printf("%s: unable to allocate S/G maps\n",
		    device_xname(self));
		goto bad;
	}
	state++;

	/*
	 * Allocate and map the memory mailbox.
	 */
	rv = mly_dmamem_alloc(mly, sizeof(struct mly_mmbox),
	    &mly->mly_mmbox_dmamap, (void **)&mly->mly_mmbox,
	    &mly->mly_mmbox_busaddr, &mly->mly_mmbox_seg);
	if (rv) {
		aprint_error_dev(self, "unable to allocate mailboxes\n");
		goto bad;
	}
	state++;

	/*
	 * Initialise per-controller queues.
	 */
	SLIST_INIT(&mly->mly_ccb_free);
	SIMPLEQ_INIT(&mly->mly_ccb_queue);

	/*
	 * Disable interrupts before we start talking to the controller.
	 */
	mly_outb(mly, mly->mly_interrupt_mask, MLY_INTERRUPT_MASK_DISABLE);

	/*
	 * Wait for the controller to come ready, handshaking with the
	 * firmware if required.  This is typically only necessary on
	 * platforms where the controller BIOS does not run.
	 */
	if (mly_fwhandshake(mly)) {
		aprint_error_dev(self, "unable to bring controller online\n");
		goto bad;
	}

	/*
	 * Allocate initial command buffers, obtain controller feature
	 * information, and then reallocate command buffers, since we'll
	 * know how many we want.
	 */
	if (mly_alloc_ccbs(mly)) {
		aprint_error_dev(self, "unable to allocate CCBs\n");
		goto bad;
	}
	state++;
	if (mly_get_controllerinfo(mly)) {
		aprint_error_dev(self, "unable to retrieve controller info\n");
		goto bad;
	}
	mly_release_ccbs(mly);
	if (mly_alloc_ccbs(mly)) {
		aprint_error_dev(self, "unable to allocate CCBs\n");
		state--;
		goto bad;
	}

	/*
	 * Get the current event counter for health purposes, populate the
	 * initial health status buffer.
	 */
	if (mly_get_eventstatus(mly)) {
		aprint_error_dev(self, "unable to retrieve event status\n");
		goto bad;
	}

	/*
	 * Enable memory-mailbox mode.
	 */
	if (mly_enable_mmbox(mly)) {
		aprint_error_dev(self, "unable to enable memory mailbox\n");
		goto bad;
	}

	/*
	 * Print a little information about the controller.
	 */
	mi = mly->mly_controllerinfo;

	printf("%s: %d physical channel%s, firmware %d.%02d-%d-%02d "
	    "(%02d%02d%02d%02d), %dMB RAM\n", device_xname(self),
	    mi->physical_channels_present,
	    (mi->physical_channels_present) > 1 ? "s" : "",
	    mi->fw_major, mi->fw_minor, mi->fw_turn, mi->fw_build,
	    mi->fw_century, mi->fw_year, mi->fw_month, mi->fw_day,
	    le16toh(mi->memory_size));

	/*
	 * Register our `shutdownhook'.
	 */
	if (mly_sdh == NULL)
		shutdownhook_establish(mly_shutdown, NULL);

	/*
	 * Clear any previous BTL information.  For each bus that scsipi
	 * wants to scan, we'll receive the SCBUSIOLLSCAN ioctl and retrieve
	 * all BTL info at that point.
	 */
	memset(&mly->mly_btl, 0, sizeof(mly->mly_btl));

	mly->mly_nchans = mly->mly_controllerinfo->physical_channels_present +
	    mly->mly_controllerinfo->virtual_channels_present;

	/*
	 * Attach to scsipi.
	 */
	adapt = &mly->mly_adapt;
	memset(adapt, 0, sizeof(*adapt));
	adapt->adapt_dev = self;
	adapt->adapt_nchannels = mly->mly_nchans;
	adapt->adapt_openings = mly->mly_ncmds - MLY_CCBS_RESV;
	adapt->adapt_max_periph = mly->mly_ncmds - MLY_CCBS_RESV;
	adapt->adapt_request = mly_scsipi_request;
	adapt->adapt_minphys = mly_scsipi_minphys;
	adapt->adapt_ioctl = mly_scsipi_ioctl;

	for (i = 0; i < mly->mly_nchans; i++) {
		chan = &mly->mly_chans[i];
		memset(chan, 0, sizeof(*chan));
		chan->chan_adapter = adapt;
		chan->chan_bustype = &scsi_bustype;
		chan->chan_channel = i;
		chan->chan_ntargets = MLY_MAX_TARGETS;
		chan->chan_nluns = MLY_MAX_LUNS;
		chan->chan_id = mly->mly_controllerparam->initiator_id;
		chan->chan_flags = SCSIPI_CHAN_NOSETTLE;
		config_found(self, chan, scsiprint);
	}

	/*
	 * Now enable interrupts...
	 */
	mly_outb(mly, mly->mly_interrupt_mask, MLY_INTERRUPT_MASK_ENABLE);

	/*
	 * Finally, create our monitoring thread.
	 */
	mly->mly_state |= MLY_STATE_INITOK;
	rv = kthread_create(PRI_NONE, 0, NULL, mly_thread, mly,
	    &mly->mly_thread, "%s", device_xname(self));
 	if (rv != 0)
		aprint_error_dev(self, "unable to create thread (%d)\n",
		    rv);
	return;

 bad:
	if (state > 2)
		mly_release_ccbs(mly);
	if (state > 1)
		mly_dmamem_free(mly, sizeof(struct mly_mmbox),
		    mly->mly_mmbox_dmamap, (void *)mly->mly_mmbox,
		    &mly->mly_mmbox_seg);
	if (state > 0)
		mly_dmamem_free(mly, MLY_SGL_SIZE * MLY_MAX_CCBS,
		    mly->mly_sg_dmamap, (void *)mly->mly_sg,
		    &mly->mly_sg_seg);
}

/*
 * Scan all possible devices on the specified channel.
 */
static void
mly_scan_channel(struct mly_softc *mly, int bus)
{
	int s, target;

	for (target = 0; target < MLY_MAX_TARGETS; target++) {
		s = splbio();
		if (!mly_scan_btl(mly, bus, target)) {
			tsleep(&mly->mly_btl[bus][target], PRIBIO, "mlyscan",
			    0);
		}
		splx(s);
	}
}

/*
 * Shut down all configured `mly' devices.
 */
static void
mly_shutdown(void *cookie)
{
	struct mly_softc *mly;
	int i;

	for (i = 0; i < mly_cd.cd_ndevs; i++) {
		if ((mly = device_lookup_private(&mly_cd, i)) == NULL)
			continue;

		if (mly_flush(mly))
			aprint_error_dev(mly->mly_dv, "unable to flush cache\n");
	}
}

/*
 * Fill in the mly_controllerinfo and mly_controllerparam fields in the
 * softc.
 */
static int
mly_get_controllerinfo(struct mly_softc *mly)
{
	struct mly_cmd_ioctl mci;
	int rv;

	/*
	 * Build the getcontrollerinfo ioctl and send it.
	 */
	memset(&mci, 0, sizeof(mci));
	mci.sub_ioctl = MDACIOCTL_GETCONTROLLERINFO;
	rv = mly_ioctl(mly, &mci, (void **)&mly->mly_controllerinfo,
	    sizeof(*mly->mly_controllerinfo), NULL, NULL);
	if (rv != 0)
		return (rv);

	/*
	 * Build the getcontrollerparameter ioctl and send it.
	 */
	memset(&mci, 0, sizeof(mci));
	mci.sub_ioctl = MDACIOCTL_GETCONTROLLERPARAMETER;
	rv = mly_ioctl(mly, &mci, (void **)&mly->mly_controllerparam,
	    sizeof(*mly->mly_controllerparam), NULL, NULL);

	return (rv);
}

/*
 * Rescan a device, possibly as a consequence of getting an event which
 * suggests that it may have changed.  Must be called with interrupts
 * blocked.
 */
static int
mly_scan_btl(struct mly_softc *mly, int bus, int target)
{
	struct mly_ccb *mc;
	struct mly_cmd_ioctl *mci;
	int rv;

	if (target == mly->mly_controllerparam->initiator_id) {
		mly->mly_btl[bus][target].mb_flags = MLY_BTL_PROTECTED;
		return (EIO);
	}

	/* Don't re-scan if a scan is already in progress. */
	if ((mly->mly_btl[bus][target].mb_flags & MLY_BTL_SCANNING) != 0)
		return (EBUSY);

	/* Get a command. */
	if ((rv = mly_ccb_alloc(mly, &mc)) != 0)
		return (rv);

	/* Set up the data buffer. */
	mc->mc_data = malloc(sizeof(union mly_devinfo),
	    M_DEVBUF, M_NOWAIT|M_ZERO);

	mc->mc_flags |= MLY_CCB_DATAIN;
	mc->mc_complete = mly_complete_rescan;

	/*
	 * Build the ioctl.
	 */
	mci = (struct mly_cmd_ioctl *)&mc->mc_packet->ioctl;
	mci->opcode = MDACMD_IOCTL;
	mci->timeout = 30 | MLY_TIMEOUT_SECONDS;
	memset(&mci->param, 0, sizeof(mci->param));

	if (MLY_BUS_IS_VIRTUAL(mly, bus)) {
		mc->mc_length = sizeof(struct mly_ioctl_getlogdevinfovalid);
		mci->data_size = htole32(mc->mc_length);
		mci->sub_ioctl = MDACIOCTL_GETLOGDEVINFOVALID;
		_lto3l(MLY_LOGADDR(0, MLY_LOGDEV_ID(mly, bus, target)),
		    mci->addr);
	} else {
		mc->mc_length = sizeof(struct mly_ioctl_getphysdevinfovalid);
		mci->data_size = htole32(mc->mc_length);
		mci->sub_ioctl = MDACIOCTL_GETPHYSDEVINFOVALID;
		_lto3l(MLY_PHYADDR(0, bus, target, 0), mci->addr);
	}

	/*
	 * Dispatch the command.
	 */
	if ((rv = mly_ccb_map(mly, mc)) != 0) {
		free(mc->mc_data, M_DEVBUF);
		mly_ccb_free(mly, mc);
		return(rv);
	}

	mly->mly_btl[bus][target].mb_flags |= MLY_BTL_SCANNING;
	mly_ccb_enqueue(mly, mc);
	return (0);
}

/*
 * Handle the completion of a rescan operation.
 */
static void
mly_complete_rescan(struct mly_softc *mly, struct mly_ccb *mc)
{
	struct mly_ioctl_getlogdevinfovalid *ldi;
	struct mly_ioctl_getphysdevinfovalid *pdi;
	struct mly_cmd_ioctl *mci;
	struct mly_btl btl, *btlp;
	struct scsipi_xfer_mode xm;
	int bus, target, rescan;
	u_int tmp;

	mly_ccb_unmap(mly, mc);

	/*
	 * Recover the bus and target from the command.  We need these even
	 * in the case where we don't have a useful response.
	 */
	mci = (struct mly_cmd_ioctl *)&mc->mc_packet->ioctl;
	tmp = _3ltol(mci->addr);
	rescan = 0;

	if (mci->sub_ioctl == MDACIOCTL_GETLOGDEVINFOVALID) {
		bus = MLY_LOGDEV_BUS(mly, MLY_LOGADDR_DEV(tmp));
		target = MLY_LOGDEV_TARGET(mly, MLY_LOGADDR_DEV(tmp));
	} else {
		bus = MLY_PHYADDR_CHANNEL(tmp);
		target = MLY_PHYADDR_TARGET(tmp);
	}

	btlp = &mly->mly_btl[bus][target];

	/* The default result is 'no device'. */
	memset(&btl, 0, sizeof(btl));
	btl.mb_flags = MLY_BTL_PROTECTED;

	/* If the rescan completed OK, we have possibly-new BTL data. */
	if (mc->mc_status != 0)
		goto out;

	if (mc->mc_length == sizeof(*ldi)) {
		ldi = (struct mly_ioctl_getlogdevinfovalid *)mc->mc_data;
		tmp = le32toh(ldi->logical_device_number);

		if (MLY_LOGDEV_BUS(mly, tmp) != bus ||
		    MLY_LOGDEV_TARGET(mly, tmp) != target) {
#ifdef MLYDEBUG
			printf("%s: WARNING: BTL rescan (logical) for %d:%d "
			    "returned data for %d:%d instead\n",
			   device_xname(mly->mly_dv), bus, target,
			   MLY_LOGDEV_BUS(mly, tmp),
			   MLY_LOGDEV_TARGET(mly, tmp));
#endif
			goto out;
		}

		btl.mb_flags = MLY_BTL_LOGICAL | MLY_BTL_TQING;
		btl.mb_type = ldi->raid_level;
		btl.mb_state = ldi->state;
	} else if (mc->mc_length == sizeof(*pdi)) {
		pdi = (struct mly_ioctl_getphysdevinfovalid *)mc->mc_data;

		if (pdi->channel != bus || pdi->target != target) {
#ifdef MLYDEBUG
			printf("%s: WARNING: BTL rescan (physical) for %d:%d "
			    " returned data for %d:%d instead\n",
			   device_xname(mly->mly_dv),
			   bus, target, pdi->channel, pdi->target);
#endif
			goto out;
		}

		btl.mb_flags = MLY_BTL_PHYSICAL;
		btl.mb_type = MLY_DEVICE_TYPE_PHYSICAL;
		btl.mb_state = pdi->state;
		btl.mb_speed = pdi->speed;
		btl.mb_width = pdi->width;

		if (pdi->state != MLY_DEVICE_STATE_UNCONFIGURED)
			btl.mb_flags |= MLY_BTL_PROTECTED;
		if (pdi->command_tags != 0)
			btl.mb_flags |= MLY_BTL_TQING;
	} else {
		printf("%s: BTL rescan result invalid\n", device_xname(mly->mly_dv));
		goto out;
	}

	/* Decide whether we need to rescan the device. */
	if (btl.mb_flags != btlp->mb_flags ||
	    btl.mb_speed != btlp->mb_speed ||
	    btl.mb_width != btlp->mb_width)
		rescan = 1;

 out:
	*btlp = btl;

	if (rescan && (btl.mb_flags & MLY_BTL_PROTECTED) == 0) {
		xm.xm_target = target;
		mly_get_xfer_mode(mly, bus, &xm);
		/* XXX SCSI mid-layer rescan goes here. */
	}

	/* Wake anybody waiting on the device to be rescanned. */
	wakeup(btlp);

	free(mc->mc_data, M_DEVBUF);
	mly_ccb_free(mly, mc);
}

/*
 * Get the current health status and set the 'next event' counter to suit.
 */
static int
mly_get_eventstatus(struct mly_softc *mly)
{
	struct mly_cmd_ioctl mci;
	struct mly_health_status *mh;
	int rv;

	/* Build the gethealthstatus ioctl and send it. */
	memset(&mci, 0, sizeof(mci));
	mh = NULL;
	mci.sub_ioctl = MDACIOCTL_GETHEALTHSTATUS;

	rv = mly_ioctl(mly, &mci, (void *)&mh, sizeof(*mh), NULL, NULL);
	if (rv)
		return (rv);

	/* Get the event counter. */
	mly->mly_event_change = le32toh(mh->change_counter);
	mly->mly_event_waiting = le32toh(mh->next_event);
	mly->mly_event_counter = le32toh(mh->next_event);

	/* Save the health status into the memory mailbox */
	memcpy(&mly->mly_mmbox->mmm_health.status, mh, sizeof(*mh));

	bus_dmamap_sync(mly->mly_dmat, mly->mly_mmbox_dmamap,
	    offsetof(struct mly_mmbox, mmm_health),
	    sizeof(mly->mly_mmbox->mmm_health),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	free(mh, M_DEVBUF);
	return (0);
}

/*
 * Enable memory mailbox mode.
 */
static int
mly_enable_mmbox(struct mly_softc *mly)
{
	struct mly_cmd_ioctl mci;
	u_int8_t *sp;
	u_int64_t tmp;
	int rv;

	/* Build the ioctl and send it. */
	memset(&mci, 0, sizeof(mci));
	mci.sub_ioctl = MDACIOCTL_SETMEMORYMAILBOX;

	/* Set buffer addresses. */
	tmp = mly->mly_mmbox_busaddr + offsetof(struct mly_mmbox, mmm_command);
	mci.param.setmemorymailbox.command_mailbox_physaddr = htole64(tmp);

	tmp = mly->mly_mmbox_busaddr + offsetof(struct mly_mmbox, mmm_status);
	mci.param.setmemorymailbox.status_mailbox_physaddr = htole64(tmp);

	tmp = mly->mly_mmbox_busaddr + offsetof(struct mly_mmbox, mmm_health);
	mci.param.setmemorymailbox.health_buffer_physaddr = htole64(tmp);

	/* Set buffer sizes - abuse of data_size field is revolting. */
	sp = (u_int8_t *)&mci.data_size;
	sp[0] = (sizeof(union mly_cmd_packet) * MLY_MMBOX_COMMANDS) >> 10;
	sp[1] = (sizeof(union mly_status_packet) * MLY_MMBOX_STATUS) >> 10;
	mci.param.setmemorymailbox.health_buffer_size =
	    sizeof(union mly_health_region) >> 10;

	rv = mly_ioctl(mly, &mci, NULL, 0, NULL, NULL);
	if (rv)
		return (rv);

	mly->mly_state |= MLY_STATE_MMBOX_ACTIVE;
	return (0);
}

/*
 * Flush all pending I/O from the controller.
 */
static int
mly_flush(struct mly_softc *mly)
{
	struct mly_cmd_ioctl mci;

	/* Build the ioctl */
	memset(&mci, 0, sizeof(mci));
	mci.sub_ioctl = MDACIOCTL_FLUSHDEVICEDATA;
	mci.param.deviceoperation.operation_device =
	    MLY_OPDEVICE_PHYSICAL_CONTROLLER;

	/* Pass it off to the controller */
	return (mly_ioctl(mly, &mci, NULL, 0, NULL, NULL));
}

/*
 * Perform an ioctl command.
 *
 * If (data) is not NULL, the command requires data transfer to the
 * controller.  If (*data) is NULL the command requires data transfer from
 * the controller, and we will allocate a buffer for it.
 */
static int
mly_ioctl(struct mly_softc *mly, struct mly_cmd_ioctl *ioctl, void **data,
	  size_t datasize, void *sense_buffer,
	  size_t *sense_length)
{
	struct mly_ccb *mc;
	struct mly_cmd_ioctl *mci;
	u_int8_t status;
	int rv;

	mc = NULL;
	if ((rv = mly_ccb_alloc(mly, &mc)) != 0)
		goto bad;

	/*
	 * Copy the ioctl structure, but save some important fields and then
	 * fixup.
	 */
	mci = &mc->mc_packet->ioctl;
	ioctl->sense_buffer_address = htole64(mci->sense_buffer_address);
	ioctl->maximum_sense_size = mci->maximum_sense_size;
	*mci = *ioctl;
	mci->opcode = MDACMD_IOCTL;
	mci->timeout = 30 | MLY_TIMEOUT_SECONDS;

	/* Handle the data buffer. */
	if (data != NULL) {
		if (*data == NULL) {
			/* Allocate data buffer */
			mc->mc_data = malloc(datasize, M_DEVBUF, M_NOWAIT);
			mc->mc_flags |= MLY_CCB_DATAIN;
		} else {
			mc->mc_data = *data;
			mc->mc_flags |= MLY_CCB_DATAOUT;
		}
		mc->mc_length = datasize;
		mc->mc_packet->generic.data_size = htole32(datasize);
	}

	/* Run the command. */
	if (datasize > 0)
		if ((rv = mly_ccb_map(mly, mc)) != 0)
			goto bad;
	rv = mly_ccb_poll(mly, mc, 30000);
	if (datasize > 0)
		mly_ccb_unmap(mly, mc);
	if (rv != 0)
		goto bad;

	/* Clean up and return any data. */
	status = mc->mc_status;

	if (status != 0)
		printf("mly_ioctl: command status %d\n", status);

	if (mc->mc_sense > 0 && sense_buffer != NULL) {
		memcpy(sense_buffer, mc->mc_packet, mc->mc_sense);
		*sense_length = mc->mc_sense;
		goto bad;
	}

	/* Should we return a data pointer? */
	if (data != NULL && *data == NULL)
		*data = mc->mc_data;

	/* Command completed OK. */
	rv = (status != 0 ? EIO : 0);

 bad:
	if (mc != NULL) {
		/* Do we need to free a data buffer we allocated? */
		if (rv != 0 && mc->mc_data != NULL &&
		    (data == NULL || *data == NULL))
			free(mc->mc_data, M_DEVBUF);
		mly_ccb_free(mly, mc);
	}

	return (rv);
}

/*
 * Check for event(s) outstanding in the controller.
 */
static void
mly_check_event(struct mly_softc *mly)
{

	bus_dmamap_sync(mly->mly_dmat, mly->mly_mmbox_dmamap,
	    offsetof(struct mly_mmbox, mmm_health),
	    sizeof(mly->mly_mmbox->mmm_health),
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);

	/*
	 * The controller may have updated the health status information, so
	 * check for it here.  Note that the counters are all in host
	 * memory, so this check is very cheap.  Also note that we depend on
	 * checking on completion
	 */
	if (le32toh(mly->mly_mmbox->mmm_health.status.change_counter) !=
	    mly->mly_event_change) {
		mly->mly_event_change =
		    le32toh(mly->mly_mmbox->mmm_health.status.change_counter);
		mly->mly_event_waiting =
		    le32toh(mly->mly_mmbox->mmm_health.status.next_event);

		/* Wake up anyone that might be interested in this. */
		wakeup(&mly->mly_event_change);
	}

	bus_dmamap_sync(mly->mly_dmat, mly->mly_mmbox_dmamap,
	    offsetof(struct mly_mmbox, mmm_health),
	    sizeof(mly->mly_mmbox->mmm_health),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	if (mly->mly_event_counter != mly->mly_event_waiting)
		mly_fetch_event(mly);
}

/*
 * Fetch one event from the controller.  If we fail due to resource
 * starvation, we'll be retried the next time a command completes.
 */
static void
mly_fetch_event(struct mly_softc *mly)
{
	struct mly_ccb *mc;
	struct mly_cmd_ioctl *mci;
	int s;
	u_int32_t event;

	/* Get a command. */
	if (mly_ccb_alloc(mly, &mc))
		return;

	/* Set up the data buffer. */
	mc->mc_data = malloc(sizeof(struct mly_event), M_DEVBUF,
	    M_NOWAIT|M_ZERO);

	mc->mc_length = sizeof(struct mly_event);
	mc->mc_flags |= MLY_CCB_DATAIN;
	mc->mc_complete = mly_complete_event;

	/*
	 * Get an event number to fetch.  It's possible that we've raced
	 * with another context for the last event, in which case there will
	 * be no more events.
	 */
	s = splbio();
	if (mly->mly_event_counter == mly->mly_event_waiting) {
		splx(s);
		free(mc->mc_data, M_DEVBUF);
		mly_ccb_free(mly, mc);
		return;
	}
	event = mly->mly_event_counter++;
	splx(s);

	/*
	 * Build the ioctl.
	 *
	 * At this point we are committed to sending this request, as it
	 * will be the only one constructed for this particular event
	 * number.
	 */
	mci = (struct mly_cmd_ioctl *)&mc->mc_packet->ioctl;
	mci->opcode = MDACMD_IOCTL;
	mci->data_size = htole32(sizeof(struct mly_event));
	_lto3l(MLY_PHYADDR(0, 0, (event >> 16) & 0xff, (event >> 24) & 0xff),
	    mci->addr);
	mci->timeout = 30 | MLY_TIMEOUT_SECONDS;
	mci->sub_ioctl = MDACIOCTL_GETEVENT;
	mci->param.getevent.sequence_number_low = htole16(event & 0xffff);

	/*
	 * Submit the command.
	 */
	if (mly_ccb_map(mly, mc) != 0)
		goto bad;
	mly_ccb_enqueue(mly, mc);
	return;

 bad:
	printf("%s: couldn't fetch event %u\n", device_xname(mly->mly_dv), event);
	free(mc->mc_data, M_DEVBUF);
	mly_ccb_free(mly, mc);
}

/*
 * Handle the completion of an event poll.
 */
static void
mly_complete_event(struct mly_softc *mly, struct mly_ccb *mc)
{
	struct mly_event *me;

	me = (struct mly_event *)mc->mc_data;
	mly_ccb_unmap(mly, mc);
	mly_ccb_free(mly, mc);

	/* If the event was successfully fetched, process it. */
	if (mc->mc_status == SCSI_OK)
		mly_process_event(mly, me);
	else
		aprint_error_dev(mly->mly_dv, "unable to fetch event; status = 0x%x\n",
		    mc->mc_status);

	free(me, M_DEVBUF);

	/* Check for another event. */
	mly_check_event(mly);
}

/*
 * Process a controller event.  Called with interrupts blocked (i.e., at
 * interrupt time).
 */
static void
mly_process_event(struct mly_softc *mly, struct mly_event *me)
{
	struct scsi_sense_data *ssd;
	int bus, target, event, class, action;
	const char *fp, *tp;

	ssd = (struct scsi_sense_data *)&me->sense[0];

	/*
	 * Errors can be reported using vendor-unique sense data.  In this
	 * case, the event code will be 0x1c (Request sense data present),
	 * the sense key will be 0x09 (vendor specific), the MSB of the ASC
	 * will be set, and the actual event code will be a 16-bit value
	 * comprised of the ASCQ (low byte) and low seven bits of the ASC
	 * (low seven bits of the high byte).
	 */
	if (le32toh(me->code) == 0x1c &&
	    SSD_SENSE_KEY(ssd->flags) == SKEY_VENDOR_SPECIFIC &&
	    (ssd->asc & 0x80) != 0) {
		event = ((int)(ssd->asc & ~0x80) << 8) +
		    ssd->ascq;
	} else
		event = le32toh(me->code);

	/* Look up event, get codes. */
	fp = mly_describe_code(mly_table_event, event);

	/* Quiet event? */
	class = fp[0];
#ifdef notyet
	if (isupper(class) && bootverbose)
		class = tolower(class);
#endif

	/* Get action code, text string. */
	action = fp[1];
	tp = fp + 3;

	/*
	 * Print some information about the event.
	 *
	 * This code uses a table derived from the corresponding portion of
	 * the Linux driver, and thus the parser is very similar.
	 */
	switch (class) {
	case 'p':
		/*
		 * Error on physical drive.
		 */
		printf("%s: physical device %d:%d %s\n", device_xname(mly->mly_dv),
		    me->channel, me->target, tp);
		if (action == 'r')
			mly->mly_btl[me->channel][me->target].mb_flags |=
			    MLY_BTL_RESCAN;
		break;

	case 'l':
	case 'm':
		/*
		 * Error on logical unit, or message about logical unit.
	 	 */
		bus = MLY_LOGDEV_BUS(mly, me->lun);
		target = MLY_LOGDEV_TARGET(mly, me->lun);
		printf("%s: logical device %d:%d %s\n", device_xname(mly->mly_dv),
		    bus, target, tp);
		if (action == 'r')
			mly->mly_btl[bus][target].mb_flags |= MLY_BTL_RESCAN;
		break;

	case 's':
		/*
		 * Report of sense data.
		 */
		if ((SSD_SENSE_KEY(ssd->flags) == SKEY_NO_SENSE ||
		     SSD_SENSE_KEY(ssd->flags) == SKEY_NOT_READY) &&
		    ssd->asc == 0x04 &&
		    (ssd->ascq == 0x01 ||
		     ssd->ascq == 0x02)) {
			/* Ignore NO_SENSE or NOT_READY in one case */
			break;
		}

		/*
		 * XXX Should translate this if SCSIVERBOSE.
		 */
		printf("%s: physical device %d:%d %s\n", device_xname(mly->mly_dv),
		    me->channel, me->target, tp);
		printf("%s:  sense key %d  asc %02x  ascq %02x\n",
		    device_xname(mly->mly_dv), SSD_SENSE_KEY(ssd->flags),
		    ssd->asc, ssd->ascq);
		printf("%s:  info %x%x%x%x  csi %x%x%x%x\n",
		    device_xname(mly->mly_dv), ssd->info[0], ssd->info[1],
		    ssd->info[2], ssd->info[3], ssd->csi[0],
		    ssd->csi[1], ssd->csi[2],
		    ssd->csi[3]);
		if (action == 'r')
			mly->mly_btl[me->channel][me->target].mb_flags |=
			    MLY_BTL_RESCAN;
		break;

	case 'e':
		printf("%s: ", device_xname(mly->mly_dv));
		printf(tp, me->target, me->lun);
		break;

	case 'c':
		printf("%s: controller %s\n", device_xname(mly->mly_dv), tp);
		break;

	case '?':
		printf("%s: %s - %d\n", device_xname(mly->mly_dv), tp, event);
		break;

	default:
		/* Probably a 'noisy' event being ignored. */
		break;
	}
}

/*
 * Perform periodic activities.
 */
static void
mly_thread(void *cookie)
{
	struct mly_softc *mly;
	struct mly_btl *btl;
	int s, bus, target, done;

	mly = (struct mly_softc *)cookie;

	for (;;) {
		/* Check for new events. */
		mly_check_event(mly);

		/* Re-scan up to 1 device. */
		s = splbio();
		done = 0;
		for (bus = 0; bus < mly->mly_nchans && !done; bus++) {
			for (target = 0; target < MLY_MAX_TARGETS; target++) {
				/* Perform device rescan? */
				btl = &mly->mly_btl[bus][target];
				if ((btl->mb_flags & MLY_BTL_RESCAN) != 0) {
					btl->mb_flags ^= MLY_BTL_RESCAN;
					mly_scan_btl(mly, bus, target);
					done = 1;
					break;
				}
			}
		}
		splx(s);

		/* Sleep for N seconds. */
		tsleep(mly_thread, PWAIT, "mlyzzz",
		    hz * MLY_PERIODIC_INTERVAL);
	}
}

/*
 * Submit a command to the controller and poll on completion.  Return
 * non-zero on timeout.
 */
static int
mly_ccb_poll(struct mly_softc *mly, struct mly_ccb *mc, int timo)
{
	int rv;

	if ((rv = mly_ccb_submit(mly, mc)) != 0)
		return (rv);

	for (timo *= 10; timo != 0; timo--) {
		if ((mc->mc_flags & MLY_CCB_COMPLETE) != 0)
			break;
		mly_intr(mly);
		DELAY(100);
	}

	return (timo == 0);
}

/*
 * Submit a command to the controller and sleep on completion.  Return
 * non-zero on timeout.
 */
static int
mly_ccb_wait(struct mly_softc *mly, struct mly_ccb *mc, int timo)
{
	int rv, s;

	mly_ccb_enqueue(mly, mc);

	s = splbio();
	if ((mc->mc_flags & MLY_CCB_COMPLETE) != 0) {
		splx(s);
		return (0);
	}
	rv = tsleep(mc, PRIBIO, "mlywccb", timo * hz / 1000);
	splx(s);

	return (rv);
}

/*
 * If a CCB is specified, enqueue it.  Pull CCBs off the software queue in
 * the order that they were enqueued and try to submit their command blocks
 * to the controller for execution.
 */
void
mly_ccb_enqueue(struct mly_softc *mly, struct mly_ccb *mc)
{
	int s;

	s = splbio();

	if (mc != NULL)
		SIMPLEQ_INSERT_TAIL(&mly->mly_ccb_queue, mc, mc_link.simpleq);

	while ((mc = SIMPLEQ_FIRST(&mly->mly_ccb_queue)) != NULL) {
		if (mly_ccb_submit(mly, mc))
			break;
		SIMPLEQ_REMOVE_HEAD(&mly->mly_ccb_queue, mc_link.simpleq);
	}

	splx(s);
}

/*
 * Deliver a command to the controller.
 */
static int
mly_ccb_submit(struct mly_softc *mly, struct mly_ccb *mc)
{
	union mly_cmd_packet *pkt;
	int s, off;

	mc->mc_packet->generic.command_id = htole16(mc->mc_slot);

	bus_dmamap_sync(mly->mly_dmat, mly->mly_pkt_dmamap,
	    mc->mc_packetphys - mly->mly_pkt_busaddr,
	    sizeof(union mly_cmd_packet),
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	s = splbio();

	/*
	 * Do we have to use the hardware mailbox?
	 */
	if ((mly->mly_state & MLY_STATE_MMBOX_ACTIVE) == 0) {
		/*
		 * Check to see if the controller is ready for us.
		 */
		if (mly_idbr_true(mly, MLY_HM_CMDSENT)) {
			splx(s);
			return (EBUSY);
		}

		/*
		 * It's ready, send the command.
		 */
		mly_outl(mly, mly->mly_cmd_mailbox,
		    (u_int64_t)mc->mc_packetphys & 0xffffffff);
		mly_outl(mly, mly->mly_cmd_mailbox + 4,
		    (u_int64_t)mc->mc_packetphys >> 32);
		mly_outb(mly, mly->mly_idbr, MLY_HM_CMDSENT);
	} else {
		pkt = &mly->mly_mmbox->mmm_command[mly->mly_mmbox_cmd_idx];
		off = (char *)pkt - (char *)mly->mly_mmbox;

		bus_dmamap_sync(mly->mly_dmat, mly->mly_mmbox_dmamap,
		    off, sizeof(mly->mly_mmbox->mmm_command[0]),
		    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);

		/* Check to see if the next index is free yet. */
		if (pkt->mmbox.flag != 0) {
			splx(s);
			return (EBUSY);
		}

		/* Copy in new command */
		memcpy(pkt->mmbox.data, mc->mc_packet->mmbox.data,
		    sizeof(pkt->mmbox.data));

		/* Copy flag last. */
		pkt->mmbox.flag = mc->mc_packet->mmbox.flag;

		bus_dmamap_sync(mly->mly_dmat, mly->mly_mmbox_dmamap,
		    off, sizeof(mly->mly_mmbox->mmm_command[0]),
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

		/* Signal controller and update index. */
		mly_outb(mly, mly->mly_idbr, MLY_AM_CMDSENT);
		mly->mly_mmbox_cmd_idx =
		    (mly->mly_mmbox_cmd_idx + 1) % MLY_MMBOX_COMMANDS;
	}

	splx(s);
	return (0);
}

/*
 * Pick up completed commands from the controller and handle accordingly.
 */
int
mly_intr(void *cookie)
{
	struct mly_ccb *mc;
	union mly_status_packet	*sp;
	u_int16_t slot;
	int forus, off;
	struct mly_softc *mly;

	mly = cookie;
	forus = 0;

	/*
	 * Pick up hardware-mailbox commands.
	 */
	if (mly_odbr_true(mly, MLY_HM_STSREADY)) {
		slot = mly_inw(mly, mly->mly_status_mailbox);

		if (slot < MLY_SLOT_MAX) {
			mc = mly->mly_ccbs + (slot - MLY_SLOT_START);
			mc->mc_status =
			    mly_inb(mly, mly->mly_status_mailbox + 2);
			mc->mc_sense =
			    mly_inb(mly, mly->mly_status_mailbox + 3);
			mc->mc_resid =
			    mly_inl(mly, mly->mly_status_mailbox + 4);

			mly_ccb_complete(mly, mc);
		} else {
			/* Slot 0xffff may mean "extremely bogus command". */
			printf("%s: got HM completion for illegal slot %u\n",
			    device_xname(mly->mly_dv), slot);
		}

		/* Unconditionally acknowledge status. */
		mly_outb(mly, mly->mly_odbr, MLY_HM_STSREADY);
		mly_outb(mly, mly->mly_idbr, MLY_HM_STSACK);
		forus = 1;
	}

	/*
	 * Pick up memory-mailbox commands.
	 */
	if (mly_odbr_true(mly, MLY_AM_STSREADY)) {
		for (;;) {
			sp = &mly->mly_mmbox->mmm_status[mly->mly_mmbox_sts_idx];
			off = (char *)sp - (char *)mly->mly_mmbox;

			bus_dmamap_sync(mly->mly_dmat, mly->mly_mmbox_dmamap,
			    off, sizeof(mly->mly_mmbox->mmm_command[0]),
			    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);

			/* Check for more status. */
			if (sp->mmbox.flag == 0)
				break;

			/* Get slot number. */
			slot = le16toh(sp->status.command_id);
			if (slot < MLY_SLOT_MAX) {
				mc = mly->mly_ccbs + (slot - MLY_SLOT_START);
				mc->mc_status = sp->status.status;
				mc->mc_sense = sp->status.sense_length;
				mc->mc_resid = le32toh(sp->status.residue);
				mly_ccb_complete(mly, mc);
			} else {
				/*
				 * Slot 0xffff may mean "extremely bogus
				 * command".
				 */
				printf("%s: got AM completion for illegal "
				    "slot %u at %d\n", device_xname(mly->mly_dv),
				    slot, mly->mly_mmbox_sts_idx);
			}

			/* Clear and move to next index. */
			sp->mmbox.flag = 0;
			mly->mly_mmbox_sts_idx =
			    (mly->mly_mmbox_sts_idx + 1) % MLY_MMBOX_STATUS;
		}

		/* Acknowledge that we have collected status value(s). */
		mly_outb(mly, mly->mly_odbr, MLY_AM_STSREADY);
		forus = 1;
	}

	/*
	 * Run the queue.
	 */
	if (forus && ! SIMPLEQ_EMPTY(&mly->mly_ccb_queue))
		mly_ccb_enqueue(mly, NULL);

	return (forus);
}

/*
 * Process completed commands
 */
static void
mly_ccb_complete(struct mly_softc *mly, struct mly_ccb *mc)
{
	void (*complete)(struct mly_softc *, struct mly_ccb *);

	bus_dmamap_sync(mly->mly_dmat, mly->mly_pkt_dmamap,
	    mc->mc_packetphys - mly->mly_pkt_busaddr,
	    sizeof(union mly_cmd_packet),
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	complete = mc->mc_complete;
	mc->mc_flags |= MLY_CCB_COMPLETE;

	/*
	 * Call completion handler or wake up sleeping consumer.
	 */
	if (complete != NULL)
		(*complete)(mly, mc);
	else
		wakeup(mc);
}

/*
 * Allocate a command.
 */
int
mly_ccb_alloc(struct mly_softc *mly, struct mly_ccb **mcp)
{
	struct mly_ccb *mc;
	int s;

	s = splbio();
	mc = SLIST_FIRST(&mly->mly_ccb_free);
	if (mc != NULL)
		SLIST_REMOVE_HEAD(&mly->mly_ccb_free, mc_link.slist);
	splx(s);

	*mcp = mc;
	return (mc == NULL ? EAGAIN : 0);
}

/*
 * Release a command back to the freelist.
 */
void
mly_ccb_free(struct mly_softc *mly, struct mly_ccb *mc)
{
	int s;

	/*
	 * Fill in parts of the command that may cause confusion if a
	 * consumer doesn't when we are later allocated.
	 */
	mc->mc_data = NULL;
	mc->mc_flags = 0;
	mc->mc_complete = NULL;
	mc->mc_private = NULL;
	mc->mc_packet->generic.command_control = 0;

	/*
	 * By default, we set up to overwrite the command packet with sense
	 * information.
	 */
	mc->mc_packet->generic.sense_buffer_address =
	    htole64(mc->mc_packetphys);
	mc->mc_packet->generic.maximum_sense_size =
	    sizeof(union mly_cmd_packet);

	s = splbio();
	SLIST_INSERT_HEAD(&mly->mly_ccb_free, mc, mc_link.slist);
	splx(s);
}

/*
 * Allocate and initialize command and packet structures.
 *
 * If the controller supports fewer than MLY_MAX_CCBS commands, limit our
 * allocation to that number.  If we don't yet know how many commands the
 * controller supports, allocate a very small set (suitable for initialization
 * purposes only).
 */
static int
mly_alloc_ccbs(struct mly_softc *mly)
{
	struct mly_ccb *mc;
	int i, rv;

	if (mly->mly_controllerinfo == NULL)
		mly->mly_ncmds = MLY_CCBS_RESV;
	else {
		i = le16toh(mly->mly_controllerinfo->maximum_parallel_commands);
		mly->mly_ncmds = min(MLY_MAX_CCBS, i);
	}

	/*
	 * Allocate enough space for all the command packets in one chunk
	 * and map them permanently into controller-visible space.
	 */
	rv = mly_dmamem_alloc(mly,
	    mly->mly_ncmds * sizeof(union mly_cmd_packet),
	    &mly->mly_pkt_dmamap, (void **)&mly->mly_pkt,
	    &mly->mly_pkt_busaddr, &mly->mly_pkt_seg);
	if (rv)
		return (rv);

	mly->mly_ccbs = malloc(sizeof(struct mly_ccb) * mly->mly_ncmds,
	    M_DEVBUF, M_NOWAIT|M_ZERO);

	for (i = 0; i < mly->mly_ncmds; i++) {
		mc = mly->mly_ccbs + i;
		mc->mc_slot = MLY_SLOT_START + i;
		mc->mc_packet = mly->mly_pkt + i;
		mc->mc_packetphys = mly->mly_pkt_busaddr +
		    (i * sizeof(union mly_cmd_packet));

		rv = bus_dmamap_create(mly->mly_dmat, MLY_MAX_XFER,
		    MLY_MAX_SEGS, MLY_MAX_XFER, 0,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &mc->mc_datamap);
		if (rv) {
			mly_release_ccbs(mly);
			return (rv);
		}

		mly_ccb_free(mly, mc);
	}

	return (0);
}

/*
 * Free all the storage held by commands.
 *
 * Must be called with all commands on the free list.
 */
static void
mly_release_ccbs(struct mly_softc *mly)
{
	struct mly_ccb *mc;

	/* Throw away command buffer DMA maps. */
	while (mly_ccb_alloc(mly, &mc) == 0)
		bus_dmamap_destroy(mly->mly_dmat, mc->mc_datamap);

	/* Release CCB storage. */
	free(mly->mly_ccbs, M_DEVBUF);

	/* Release the packet storage. */
	mly_dmamem_free(mly, mly->mly_ncmds * sizeof(union mly_cmd_packet),
	    mly->mly_pkt_dmamap, (void *)mly->mly_pkt, &mly->mly_pkt_seg);
}

/*
 * Map a command into controller-visible space.
 */
static int
mly_ccb_map(struct mly_softc *mly, struct mly_ccb *mc)
{
	struct mly_cmd_generic *gen;
	struct mly_sg_entry *sg;
	bus_dma_segment_t *ds;
	int flg, nseg, rv;

#ifdef DIAGNOSTIC
	/* Don't map more than once. */
	if ((mc->mc_flags & MLY_CCB_MAPPED) != 0)
		panic("mly_ccb_map: already mapped");
	mc->mc_flags |= MLY_CCB_MAPPED;

	/* Does the command have a data buffer? */
	if (mc->mc_data == NULL)
		panic("mly_ccb_map: no data buffer");
#endif

	rv = bus_dmamap_load(mly->mly_dmat, mc->mc_datamap, mc->mc_data,
	    mc->mc_length, NULL, BUS_DMA_NOWAIT | BUS_DMA_STREAMING |
	    ((mc->mc_flags & MLY_CCB_DATAIN) != 0 ?
	    BUS_DMA_READ : BUS_DMA_WRITE));
	if (rv != 0)
		return (rv);

	gen = &mc->mc_packet->generic;

	/*
	 * Can we use the transfer structure directly?
	 */
	if ((nseg = mc->mc_datamap->dm_nsegs) <= 2) {
		mc->mc_sgoff = -1;
		sg = &gen->transfer.direct.sg[0];
	} else {
		mc->mc_sgoff = (mc->mc_slot - MLY_SLOT_START) *
		    MLY_MAX_SEGS;
		sg = mly->mly_sg + mc->mc_sgoff;
		gen->command_control |= MLY_CMDCTL_EXTENDED_SG_TABLE;
		gen->transfer.indirect.entries[0] = htole16(nseg);
		gen->transfer.indirect.table_physaddr[0] =
		    htole64(mly->mly_sg_busaddr +
		    (mc->mc_sgoff * sizeof(struct mly_sg_entry)));
	}

	/*
	 * Fill the S/G table.
	 */
	for (ds = mc->mc_datamap->dm_segs; nseg != 0; nseg--, sg++, ds++) {
		sg->physaddr = htole64(ds->ds_addr);
		sg->length = htole64(ds->ds_len);
	}

	/*
	 * Sync up the data map.
	 */
	if ((mc->mc_flags & MLY_CCB_DATAIN) != 0)
		flg = BUS_DMASYNC_PREREAD;
	else /* if ((mc->mc_flags & MLY_CCB_DATAOUT) != 0) */ {
		gen->command_control |= MLY_CMDCTL_DATA_DIRECTION;
		flg = BUS_DMASYNC_PREWRITE;
	}

	bus_dmamap_sync(mly->mly_dmat, mc->mc_datamap, 0, mc->mc_length, flg);

	/*
	 * Sync up the chained S/G table, if we're using one.
	 */
	if (mc->mc_sgoff == -1)
		return (0);

	bus_dmamap_sync(mly->mly_dmat, mly->mly_sg_dmamap, mc->mc_sgoff,
	    MLY_SGL_SIZE, BUS_DMASYNC_PREWRITE);

	return (0);
}

/*
 * Unmap a command from controller-visible space.
 */
static void
mly_ccb_unmap(struct mly_softc *mly, struct mly_ccb *mc)
{
	int flg;

#ifdef DIAGNOSTIC
	if ((mc->mc_flags & MLY_CCB_MAPPED) == 0)
		panic("mly_ccb_unmap: not mapped");
	mc->mc_flags &= ~MLY_CCB_MAPPED;
#endif

	if ((mc->mc_flags & MLY_CCB_DATAIN) != 0)
		flg = BUS_DMASYNC_POSTREAD;
	else /* if ((mc->mc_flags & MLY_CCB_DATAOUT) != 0) */
		flg = BUS_DMASYNC_POSTWRITE;

	bus_dmamap_sync(mly->mly_dmat, mc->mc_datamap, 0, mc->mc_length, flg);
	bus_dmamap_unload(mly->mly_dmat, mc->mc_datamap);

	if (mc->mc_sgoff == -1)
		return;

	bus_dmamap_sync(mly->mly_dmat, mly->mly_sg_dmamap, mc->mc_sgoff,
	    MLY_SGL_SIZE, BUS_DMASYNC_POSTWRITE);
}

/*
 * Adjust the size of each I/O before it passes to the SCSI layer.
 */
static void
mly_scsipi_minphys(struct buf *bp)
{

	if (bp->b_bcount > MLY_MAX_XFER)
		bp->b_bcount = MLY_MAX_XFER;
	minphys(bp);
}

/*
 * Start a SCSI command.
 */
static void
mly_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req,
		   void *arg)
{
	struct mly_ccb *mc;
	struct mly_cmd_scsi_small *ss;
	struct scsipi_xfer *xs;
	struct scsipi_periph *periph;
	struct mly_softc *mly;
	struct mly_btl *btl;
	int s, tmp;

	mly = device_private(chan->chan_adapter->adapt_dev);

	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		xs = arg;
		periph = xs->xs_periph;
		btl = &mly->mly_btl[chan->chan_channel][periph->periph_target];
		s = splbio();
		tmp = btl->mb_flags;
		splx(s);

		/*
		 * Check for I/O attempt to a protected or non-existant
		 * device.
		 */
		if ((tmp & MLY_BTL_PROTECTED) != 0) {
			xs->error = XS_SELTIMEOUT;
			scsipi_done(xs);
			break;
		}

#ifdef DIAGNOSTIC
		/* XXX Increase if/when we support large SCSI commands. */
		if (xs->cmdlen > MLY_CMD_SCSI_SMALL_CDB) {
			printf("%s: cmd too large\n", device_xname(mly->mly_dv));
			xs->error = XS_DRIVER_STUFFUP;
			scsipi_done(xs);
			break;
		}
#endif

		if (mly_ccb_alloc(mly, &mc)) {
			xs->error = XS_RESOURCE_SHORTAGE;
			scsipi_done(xs);
			break;
		}

		/* Build the command. */
		mc->mc_data = xs->data;
		mc->mc_length = xs->datalen;
		mc->mc_complete = mly_scsipi_complete;
		mc->mc_private = xs;

		/* Build the packet for the controller. */
		ss = &mc->mc_packet->scsi_small;
		ss->opcode = MDACMD_SCSI;
#ifdef notdef
		/*
		 * XXX FreeBSD does this, but it doesn't fix anything,
		 * XXX and appears potentially harmful.
		 */
		ss->command_control |= MLY_CMDCTL_DISABLE_DISCONNECT;
#endif

		ss->data_size = htole32(xs->datalen);
		_lto3l(MLY_PHYADDR(0, chan->chan_channel,
		    periph->periph_target, periph->periph_lun), ss->addr);

		if (xs->timeout < 60 * 1000)
			ss->timeout = xs->timeout / 1000 |
			    MLY_TIMEOUT_SECONDS;
		else if (xs->timeout < 60 * 60 * 1000)
			ss->timeout = xs->timeout / (60 * 1000) |
			    MLY_TIMEOUT_MINUTES;
		else
			ss->timeout = xs->timeout / (60 * 60 * 1000) |
			    MLY_TIMEOUT_HOURS;

		ss->maximum_sense_size = sizeof(xs->sense);
		ss->cdb_length = xs->cmdlen;
		memcpy(ss->cdb, xs->cmd, xs->cmdlen);

		if (mc->mc_length != 0) {
			if ((xs->xs_control & XS_CTL_DATA_OUT) != 0)
				mc->mc_flags |= MLY_CCB_DATAOUT;
			else /* if ((xs->xs_control & XS_CTL_DATA_IN) != 0) */
				mc->mc_flags |= MLY_CCB_DATAIN;

			if (mly_ccb_map(mly, mc) != 0) {
				xs->error = XS_DRIVER_STUFFUP;
				mly_ccb_free(mly, mc);
				scsipi_done(xs);
				break;
			}
		}

		/*
		 * Give the command to the controller.
		 */
		if ((xs->xs_control & XS_CTL_POLL) != 0) {
			if (mly_ccb_poll(mly, mc, xs->timeout + 5000)) {
				xs->error = XS_REQUEUE;
				if (mc->mc_length != 0)
					mly_ccb_unmap(mly, mc);
				mly_ccb_free(mly, mc);
				scsipi_done(xs);
			}
		} else
			mly_ccb_enqueue(mly, mc);

		break;

	case ADAPTER_REQ_GROW_RESOURCES:
		/*
		 * Not supported.
		 */
		break;

	case ADAPTER_REQ_SET_XFER_MODE:
		/*
		 * We can't change the transfer mode, but at least let
		 * scsipi know what the adapter has negotiated.
		 */
		mly_get_xfer_mode(mly, chan->chan_channel, arg);
		break;
	}
}

/*
 * Handle completion of a SCSI command.
 */
static void
mly_scsipi_complete(struct mly_softc *mly, struct mly_ccb *mc)
{
	struct scsipi_xfer *xs;
	struct scsipi_channel *chan;
	struct scsipi_inquiry_data *inq;
	struct mly_btl *btl;
	int target, sl, s;
	const char *p;

	xs = mc->mc_private;
	xs->status = mc->mc_status;

	/*
	 * XXX The `resid' value as returned by the controller appears to be
	 * bogus, so we always set it to zero.  Is it perhaps the transfer
	 * count?
	 */
	xs->resid = 0; /* mc->mc_resid; */

	if (mc->mc_length != 0)
		mly_ccb_unmap(mly, mc);

	switch (mc->mc_status) {
	case SCSI_OK:
		/*
		 * In order to report logical device type and status, we
		 * overwrite the result of the INQUIRY command to logical
		 * devices.
		 */
		if (xs->cmd->opcode == INQUIRY) {
			chan = xs->xs_periph->periph_channel;
			target = xs->xs_periph->periph_target;
			btl = &mly->mly_btl[chan->chan_channel][target];

			s = splbio();
			if ((btl->mb_flags & MLY_BTL_LOGICAL) != 0) {
				inq = (struct scsipi_inquiry_data *)xs->data;
				mly_padstr(inq->vendor, "MYLEX", 8);
				p = mly_describe_code(mly_table_device_type,
				    btl->mb_type);
				mly_padstr(inq->product, p, 16);
				p = mly_describe_code(mly_table_device_state,
				    btl->mb_state);
				mly_padstr(inq->revision, p, 4);
			}
			splx(s);
		}

		xs->error = XS_NOERROR;
		break;

	case SCSI_CHECK:
		sl = mc->mc_sense;
		if (sl > sizeof(xs->sense.scsi_sense))
			sl = sizeof(xs->sense.scsi_sense);
		memcpy(&xs->sense.scsi_sense, mc->mc_packet, sl);
		xs->error = XS_SENSE;
		break;

	case SCSI_BUSY:
	case SCSI_QUEUE_FULL:
		xs->error = XS_BUSY;
		break;

	default:
		printf("%s: unknown SCSI status 0x%x\n",
		    device_xname(mly->mly_dv), xs->status);
		xs->error = XS_DRIVER_STUFFUP;
		break;
	}

	mly_ccb_free(mly, mc);
	scsipi_done(xs);
}

/*
 * Notify scsipi about a target's transfer mode.
 */
static void
mly_get_xfer_mode(struct mly_softc *mly, int bus, struct scsipi_xfer_mode *xm)
{
	struct mly_btl *btl;
	int s;

	btl = &mly->mly_btl[bus][xm->xm_target];
	xm->xm_mode = 0;

	s = splbio();

	if ((btl->mb_flags & MLY_BTL_PHYSICAL) != 0) {
		if (btl->mb_speed == 0) {
			xm->xm_period = 0;
			xm->xm_offset = 0;
		} else {
			xm->xm_period = 12;			/* XXX */
			xm->xm_offset = 8;			/* XXX */
			xm->xm_mode |= PERIPH_CAP_SYNC;		/* XXX */
		}

		switch (btl->mb_width) {
		case 32:
			xm->xm_mode = PERIPH_CAP_WIDE32;
			break;
		case 16:
			xm->xm_mode = PERIPH_CAP_WIDE16;
			break;
		default:
			xm->xm_mode = 0;
			break;
		}
	} else /* ((btl->mb_flags & MLY_BTL_LOGICAL) != 0) */ {
		xm->xm_mode = PERIPH_CAP_WIDE16 | PERIPH_CAP_SYNC;
		xm->xm_period = 12;
		xm->xm_offset = 8;
	}

	if ((btl->mb_flags & MLY_BTL_TQING) != 0)
		xm->xm_mode |= PERIPH_CAP_TQING;

	splx(s);

	scsipi_async_event(&mly->mly_chans[bus], ASYNC_EVENT_XFER_MODE, xm);
}

/*
 * ioctl hook; used here only to initiate low-level rescans.
 */
static int
mly_scsipi_ioctl(struct scsipi_channel *chan, u_long cmd, void *data,
    int flag, struct proc *p)
{
	struct mly_softc *mly;
	int rv;

	mly = device_private(chan->chan_adapter->adapt_dev);

	switch (cmd) {
	case SCBUSIOLLSCAN:
		mly_scan_channel(mly, chan->chan_channel);
		rv = 0;
		break;
	default:
		rv = ENOTTY;
		break;
	}

	return (rv);
}

/*
 * Handshake with the firmware while the card is being initialized.
 */
static int
mly_fwhandshake(struct mly_softc *mly)
{
	u_int8_t error;
	int spinup;

	spinup = 0;

	/* Set HM_STSACK and let the firmware initialize. */
	mly_outb(mly, mly->mly_idbr, MLY_HM_STSACK);
	DELAY(1000);	/* too short? */

	/* If HM_STSACK is still true, the controller is initializing. */
	if (!mly_idbr_true(mly, MLY_HM_STSACK))
		return (0);

	printf("%s: controller initialization started\n",
	    device_xname(mly->mly_dv));

	/*
	 * Spin waiting for initialization to finish, or for a message to be
	 * delivered.
	 */
	while (mly_idbr_true(mly, MLY_HM_STSACK)) {
		/* Check for a message */
		if (!mly_error_valid(mly))
			continue;

		error = mly_inb(mly, mly->mly_error_status) & ~MLY_MSG_EMPTY;
		(void)mly_inb(mly, mly->mly_cmd_mailbox);
		(void)mly_inb(mly, mly->mly_cmd_mailbox + 1);

		switch (error) {
		case MLY_MSG_SPINUP:
			if (!spinup) {
				printf("%s: drive spinup in progress\n",
				    device_xname(mly->mly_dv));
				spinup = 1;
			}
			break;

		case MLY_MSG_RACE_RECOVERY_FAIL:
			printf("%s: mirror race recovery failed - \n",
			    device_xname(mly->mly_dv));
			printf("%s: one or more drives offline\n",
			    device_xname(mly->mly_dv));
			break;

		case MLY_MSG_RACE_IN_PROGRESS:
			printf("%s: mirror race recovery in progress\n",
			    device_xname(mly->mly_dv));
			break;

		case MLY_MSG_RACE_ON_CRITICAL:
			printf("%s: mirror race recovery on critical drive\n",
			    device_xname(mly->mly_dv));
			break;

		case MLY_MSG_PARITY_ERROR:
			printf("%s: FATAL MEMORY PARITY ERROR\n",
			    device_xname(mly->mly_dv));
			return (ENXIO);

		default:
			printf("%s: unknown initialization code 0x%x\n",
			    device_xname(mly->mly_dv), error);
			break;
		}
	}

	return (0);
}

/*
 * Space-fill a character string
 */
static void
mly_padstr(char *dst, const char *src, int len)
{

	while (len-- > 0) {
		if (*src != '\0')
			*dst++ = *src++;
		else
			*dst++ = ' ';
	}
}

/*
 * Allocate DMA safe memory.
 */
static int
mly_dmamem_alloc(struct mly_softc *mly, int size, bus_dmamap_t *dmamap,
		 void **kva, bus_addr_t *paddr, bus_dma_segment_t *seg)
{
	int rseg, rv, state;

	state = 0;

	if ((rv = bus_dmamem_alloc(mly->mly_dmat, size, PAGE_SIZE, 0,
	    seg, 1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(mly->mly_dv, "dmamem_alloc = %d\n", rv);
		goto bad;
	}

	state++;

	if ((rv = bus_dmamem_map(mly->mly_dmat, seg, 1, size, kva,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(mly->mly_dv, "dmamem_map = %d\n", rv);
		goto bad;
	}

	state++;

	if ((rv = bus_dmamap_create(mly->mly_dmat, size, size, 1, 0,
	    BUS_DMA_NOWAIT, dmamap)) != 0) {
		aprint_error_dev(mly->mly_dv, "dmamap_create = %d\n", rv);
		goto bad;
	}

	state++;

	if ((rv = bus_dmamap_load(mly->mly_dmat, *dmamap, *kva, size,
	    NULL, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(mly->mly_dv, "dmamap_load = %d\n", rv);
		goto bad;
	}

	*paddr = (*dmamap)->dm_segs[0].ds_addr;
	memset(*kva, 0, size);
	return (0);

 bad:
	if (state > 2)
		bus_dmamap_destroy(mly->mly_dmat, *dmamap);
	if (state > 1)
		bus_dmamem_unmap(mly->mly_dmat, *kva, size);
	if (state > 0)
		bus_dmamem_free(mly->mly_dmat, seg, 1);

	return (rv);
}

/*
 * Free DMA safe memory.
 */
static void
mly_dmamem_free(struct mly_softc *mly, int size, bus_dmamap_t dmamap,
		void *kva, bus_dma_segment_t *seg)
{

	bus_dmamap_unload(mly->mly_dmat, dmamap);
	bus_dmamap_destroy(mly->mly_dmat, dmamap);
	bus_dmamem_unmap(mly->mly_dmat, kva, size);
	bus_dmamem_free(mly->mly_dmat, seg, 1);
}


/*
 * Accept an open operation on the control device.
 */
int
mlyopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct mly_softc *mly;

	if ((mly = device_lookup_private(&mly_cd, minor(dev))) == NULL)
		return (ENXIO);
	if ((mly->mly_state & MLY_STATE_INITOK) == 0)
		return (ENXIO);
	if ((mly->mly_state & MLY_STATE_OPEN) != 0)
		return (EBUSY);

	mly->mly_state |= MLY_STATE_OPEN;
	return (0);
}

/*
 * Accept the last close on the control device.
 */
int
mlyclose(dev_t dev, int flag, int mode,
    struct lwp *l)
{
	struct mly_softc *mly;

	mly = device_lookup_private(&mly_cd, minor(dev));
	mly->mly_state &= ~MLY_STATE_OPEN;
	return (0);
}

/*
 * Handle control operations.
 */
int
mlyioctl(dev_t dev, u_long cmd, void *data, int flag,
    struct lwp *l)
{
	struct mly_softc *mly;
	int rv;

	mly = device_lookup_private(&mly_cd, minor(dev));

	switch (cmd) {
	case MLYIO_COMMAND:
		rv = kauth_authorize_device_passthru(l->l_cred, dev,
		    KAUTH_REQ_DEVICE_RAWIO_PASSTHRU_ALL, data);
		if (rv)
			break;

		rv = mly_user_command(mly, (void *)data);
		break;
	case MLYIO_HEALTH:
		rv = mly_user_health(mly, (void *)data);
		break;
	default:
		rv = ENOTTY;
		break;
	}

	return (rv);
}

/*
 * Execute a command passed in from userspace.
 *
 * The control structure contains the actual command for the controller, as
 * well as the user-space data pointer and data size, and an optional sense
 * buffer size/pointer.  On completion, the data size is adjusted to the
 * command residual, and the sense buffer size to the size of the returned
 * sense data.
 */
static int
mly_user_command(struct mly_softc *mly, struct mly_user_command *uc)
{
	struct mly_ccb	*mc;
	int rv, mapped;

	if ((rv = mly_ccb_alloc(mly, &mc)) != 0)
		return (rv);

	mapped = 0;
	mc->mc_data = NULL;

	/*
	 * Handle data size/direction.
	 */
	if ((mc->mc_length = abs(uc->DataTransferLength)) != 0) {
		if (mc->mc_length > MAXPHYS) {
			rv = EINVAL;
			goto out;
		}

		mc->mc_data = malloc(mc->mc_length, M_DEVBUF, M_WAITOK);
		if (mc->mc_data == NULL) {
			rv = ENOMEM;
			goto out;
		}

		if (uc->DataTransferLength > 0) {
			mc->mc_flags |= MLY_CCB_DATAIN;
			memset(mc->mc_data, 0, mc->mc_length);
		}

		if (uc->DataTransferLength < 0) {
			mc->mc_flags |= MLY_CCB_DATAOUT;
			rv = copyin(uc->DataTransferBuffer, mc->mc_data,
			    mc->mc_length);
			if (rv != 0)
				goto out;
		}

		if ((rv = mly_ccb_map(mly, mc)) != 0)
			goto out;
		mapped = 1;
	}

	/* Copy in the command and execute it. */
	memcpy(mc->mc_packet, &uc->CommandMailbox, sizeof(uc->CommandMailbox));

	if ((rv = mly_ccb_wait(mly, mc, 60000)) != 0)
		goto out;

	/* Return the data to userspace. */
	if (uc->DataTransferLength > 0) {
		rv = copyout(mc->mc_data, uc->DataTransferBuffer,
		    mc->mc_length);
		if (rv != 0)
			goto out;
	}

	/* Return the sense buffer to userspace. */
	if (uc->RequestSenseLength > 0 && mc->mc_sense > 0) {
		rv = copyout(mc->mc_packet, uc->RequestSenseBuffer,
		    min(uc->RequestSenseLength, mc->mc_sense));
		if (rv != 0)
			goto out;
	}

	/* Return command results to userspace (caller will copy out). */
	uc->DataTransferLength = mc->mc_resid;
	uc->RequestSenseLength = min(uc->RequestSenseLength, mc->mc_sense);
	uc->CommandStatus = mc->mc_status;
	rv = 0;

 out:
 	if (mapped)
 		mly_ccb_unmap(mly, mc);
	if (mc->mc_data != NULL)
		free(mc->mc_data, M_DEVBUF);
	mly_ccb_free(mly, mc);

	return (rv);
}

/*
 * Return health status to userspace.  If the health change index in the
 * user structure does not match that currently exported by the controller,
 * we return the current status immediately.  Otherwise, we block until
 * either interrupted or new status is delivered.
 */
static int
mly_user_health(struct mly_softc *mly, struct mly_user_health *uh)
{
	struct mly_health_status mh;
	int rv, s;

	/* Fetch the current health status from userspace. */
	rv = copyin(uh->HealthStatusBuffer, &mh, sizeof(mh));
	if (rv != 0)
		return (rv);

	/* spin waiting for a status update */
	s = splbio();
	if (mly->mly_event_change == mh.change_counter)
		rv = tsleep(&mly->mly_event_change, PRIBIO | PCATCH,
		    "mlyhealth", 0);
	splx(s);

	if (rv == 0) {
		/*
		 * Copy the controller's health status buffer out (there is
		 * a race here if it changes again).
		 */
		rv = copyout(&mly->mly_mmbox->mmm_health.status,
		    uh->HealthStatusBuffer, sizeof(uh->HealthStatusBuffer));
	}

	return (rv);
}

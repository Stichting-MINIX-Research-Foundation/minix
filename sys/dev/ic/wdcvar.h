/*	$NetBSD: wdcvar.h,v 1.97 2013/02/03 20:13:28 jakllsch Exp $	*/

/*-
 * Copyright (c) 1998, 2003, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum, by Onno van der Linden and by Manuel Bouyer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
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

#ifndef _DEV_IC_WDCVAR_H_
#define	_DEV_IC_WDCVAR_H_

#include <sys/callout.h>

#include <dev/ata/ataconf.h>
#include <dev/ic/wdcreg.h>

#define	WAITTIME    (10 * hz)    /* time to wait for a completion */
	/* this is a lot for hard drives, but not for cdroms */

#define WDC_NREG	8 /* number of command registers */
#define	WDC_NSHADOWREG	2 /* number of command "shadow" registers */

#define WDC_MAXDRIVES	2 /* absolute max number of drives per channel */

struct wdc_regs {
	/* Our registers */
	bus_space_tag_t       cmd_iot;
	bus_space_handle_t    cmd_baseioh;
	bus_size_t            cmd_ios;
	bus_space_handle_t    cmd_iohs[WDC_NREG+WDC_NSHADOWREG];
	bus_space_tag_t       ctl_iot;
	bus_space_handle_t    ctl_ioh;
	bus_size_t            ctl_ios;

	/* data32{iot,ioh} are only used for 32-bit data xfers */
	bus_space_tag_t       data32iot;
	bus_space_handle_t    data32ioh;

	/* SATA native registers */
	bus_space_tag_t       sata_iot;
	bus_space_handle_t    sata_baseioh;
	bus_space_handle_t    sata_control;
	bus_space_handle_t    sata_status;
	bus_space_handle_t    sata_error;

};

/*
 * Per-controller data
 */
struct wdc_softc {
	struct atac_softc sc_atac;	/* generic ATA controller info */

	struct wdc_regs *regs;		/* register array (per-channel) */

	int		wdc_maxdrives;	/* max number of drives per channel */

	int		cap;		/* controller capabilities */
#define WDC_CAPABILITY_NO_EXTRA_RESETS 0x0100 /* only reset once */
#define WDC_CAPABILITY_PREATA	0x0200	/* ctrl can be a pre-ata one */
#define WDC_CAPABILITY_WIDEREGS 0x0400  /* ctrl has wide (16bit) registers  */
#define WDC_CAPABILITY_NO_AUXCTL 0x0800 /* ctrl has no aux control registers */

#if NATA_DMA || NATA_PIOBM
	/* if WDC_CAPABILITY_DMA set in 'cap' */
	void            *dma_arg;
	int            (*dma_init)(void *, int, int, void *, size_t, int);
	void           (*dma_start)(void *, int, int);
	int            (*dma_finish)(void *, int, int, int);
#if NATA_PIOBM
	void           (*piobm_start)(void *, int, int, int, int, int);
	void           (*piobm_done)(void *, int, int);
#endif
/* flags passed to dma_init */
#define WDC_DMA_READ		0x01
#define WDC_DMA_IRQW		0x02
#define WDC_DMA_LBA48		0x04
#define WDC_DMA_PIOBM_ATA	0x08
#define WDC_DMA_PIOBM_ATAPI	0x10
#if NATA_PIOBM
/* flags passed to piobm_start */
#define WDC_PIOBM_XFER_IRQ	0x01
#endif

/* values passed to dma_finish */
#define WDC_DMAEND_END	0	/* check for proper end of a DMA xfer */
#define WDC_DMAEND_ABRT 1	/* abort a DMA xfer, verbose */
#define WDC_DMAEND_ABRT_QUIET 2	/* abort a DMA xfer, quiet */

	int		dma_status; /* status returned from dma_finish() */
#define WDC_DMAST_NOIRQ	0x01	/* missing IRQ */
#define WDC_DMAST_ERR	0x02	/* DMA error */
#define WDC_DMAST_UNDER	0x04	/* DMA underrun */
#endif	/* NATA_DMA || NATA_PIOBM */

	/* Optional callback to select drive. */
	void		(*select)(struct ata_channel *,int);

	/* Optional callback to ack IRQ. */
	void		(*irqack)(struct ata_channel *);

	/* Optional callback to perform a bus reset */
	void		(*reset)(struct ata_channel *, int);

	/* overridden if the backend has a different data transfer method */
	void	(*datain_pio)(struct ata_channel *, int, void *, size_t);
	void	(*dataout_pio)(struct ata_channel *, int, void *, size_t);
};

/* Given an ata_channel, get the wdc_softc. */
#define	CHAN_TO_WDC(chp)	((struct wdc_softc *)(chp)->ch_atac)

/* Given an ata_channel, get the wdc_regs. */
#define	CHAN_TO_WDC_REGS(chp)	(&CHAN_TO_WDC(chp)->regs[(chp)->ch_channel])

/*
 * Public functions which can be called by ATA or ATAPI specific parts,
 * or bus-specific backends.
 */

void	wdc_allocate_regs(struct wdc_softc *);
void	wdc_init_shadow_regs(struct ata_channel *);

int	wdcprobe(struct ata_channel *);
void	wdcattach(struct ata_channel *);
int	wdcdetach(device_t, int);
void	wdc_childdetached(device_t, device_t);
int	wdcintr(void *);

void	wdc_sataprobe(struct ata_channel *);
void	wdc_drvprobe(struct ata_channel *);

void	wdcrestart(void*);

int	wdcwait(struct ata_channel *, int, int, int, int);
#define WDCWAIT_OK	0  /* we have what we asked */
#define WDCWAIT_TOUT	-1 /* timed out */
#define WDCWAIT_THR	1  /* return, the kernel thread has been awakened */

void	wdcbit_bucket(struct ata_channel *, int);

int	wdc_dmawait(struct ata_channel *, struct ata_xfer *, int);
void	wdccommand(struct ata_channel *, u_int8_t, u_int8_t, u_int16_t,
		   u_int8_t, u_int8_t, u_int8_t, u_int8_t);
void	wdccommandext(struct ata_channel *, u_int8_t, u_int8_t, u_int64_t,
		      u_int16_t, u_int16_t, u_int8_t);
void	wdccommandshort(struct ata_channel *, int, int);
void	wdctimeout(void *arg);
void	wdc_reset_drive(struct ata_drive_datas *, int, uint32_t *);
void	wdc_reset_channel(struct ata_channel *, int);
void	wdc_do_reset(struct ata_channel *, int);

int	wdc_exec_command(struct ata_drive_datas *, struct ata_command*);

/*
 * ST506 spec says that if READY or SEEKCMPLT go off, then the read or write
 * command is aborted.
 */
#define wdc_wait_for_drq(chp, timeout, flags) \
		wdcwait((chp), WDCS_DRQ, WDCS_DRQ, (timeout), (flags))
#define wdc_wait_for_unbusy(chp, timeout, flags) \
		wdcwait((chp), 0, 0, (timeout), (flags))
#define wdc_wait_for_ready(chp, timeout, flags) \
		wdcwait((chp), WDCS_DRDY, WDCS_DRDY, (timeout), (flags))

/* ATA/ATAPI specs says a device can take 31s to reset */
#define WDC_RESET_WAIT 31000

void	wdc_atapibus_attach(struct atabus_softc *);

#endif /* _DEV_IC_WDCVAR_H_ */

/*	$NetBSD: icpvar.h,v 1.13 2012/10/27 17:18:20 chs Exp $	*/

/*-
 * Copyright (c) 2002, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran, and by Jason R. Thorpe of Wasabi Systems, Inc.
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

#ifndef _IC_ICPVAR_H_
#define _IC_ICPVAR_H_

#include <sys/mutex.h>

#include <dev/ic/icp_ioctl.h>

/*
 * Miscellaneous constants.
 */
#define ICP_RETRIES		6
#define	ICP_WATCHDOG_FREQ	5
#define	ICP_BUSY_WAIT_MS	2500
#define	ICP_MAX_XFER		65536
#define	ICP_UCMD_SCRATCH_SIZE	4096
#define ICP_SCRATCH_SIZE	(8192 + ICP_UCMD_SCRATCH_SIZE)
#define	ICP_SCRATCH_SENSE \
    (ICP_SCRATCH_SIZE - sizeof(struct scsi_sense_data) * (ICP_NCCBS + ICP_NCCB_RESERVE))
#define	ICP_SCRATCH_UCMD	(ICP_SCRATCH_SENSE - ICP_UCMD_SCRATCH_SIZE)

#define	ICP_NCCBS		ICP_MAX_CMDS
#define	ICP_NCCB_RESERVE	4

/*
 * Context structure for interrupt service.
 */
struct icp_intr_ctx {
	u_int32_t	info;
	u_int32_t	info2;
	u_int16_t	cmd_status;
	u_int16_t	service;
	u_int8_t	istatus;
};

/*
 * Command control block.
 */
struct icp_ccb {
	SIMPLEQ_ENTRY(icp_ccb) ic_chain;
	u_int		ic_service;
	u_int		ic_flags;
	u_int		ic_status;
	u_int		ic_ident;
	u_int		ic_nsgent;
	u_int		ic_cmdlen;
	u_int		ic_xfer_size;
	bus_dmamap_t	ic_xfer_map;
	struct icp_sg	*ic_sg;
	device_t	ic_dv;
	void		*ic_context;
	void		(*ic_intr)(struct icp_ccb *);
	struct icp_cmd	ic_cmd;
};
#define	IC_XFER_IN	0x01	/* Map describes inbound xfer */
#define	IC_XFER_OUT	0x02	/* Map describes outbound xfer */
#define	IC_WAITING	0x04	/* We have waiters */
#define	IC_COMPLETE	0x08	/* Command completed */
#define	IC_ALLOCED	0x10	/* CCB allocated */
#define	IC_UCMD		0x20	/* user ioctl */

/*
 * Logical drive information.
 */
struct icp_cachedrv {
	u_int		cd_size;
	u_int		cd_type;
};

/*
 * Call-backs into the service back-ends (ld for cache service,
 * icpsp for raw service).
 */
struct icp_servicecb {
	void	(*iscb_openings)(device_t, int);
};

/*
 * Per-controller context.
 */
struct icp_softc {
	device_t		icp_dv;
	void			*icp_ih;
	bus_dma_tag_t		icp_dmat;
	bus_space_tag_t		icp_dpmemt;
	bus_space_handle_t	icp_dpmemh;
	bus_addr_t		icp_dpmembase;
	bus_space_tag_t		icp_iot;
	bus_space_handle_t	icp_ioh;
	bus_addr_t		icp_iobase;

	int			icp_class;
	u_int16_t		icp_fw_vers;
	u_int16_t		icp_ic_all_size;
	u_int8_t		icp_bus_cnt;
	u_int8_t		icp_bus_id[ICP_MAXBUS];
	struct icp_cachedrv	icp_cdr[ICP_MAX_HDRIVES];
	const struct icp_servicecb *icp_servicecb[ICP_MAX_HDRIVES + ICP_MAXBUS];
	device_t		icp_children[ICP_MAX_HDRIVES + ICP_MAXBUS];
	int			icp_ndevs;
	int			icp_openings;
	int			icp_features;
	int			icp_nchan;

	u_int32_t		icp_info;
	u_int32_t		icp_info2;
	u_int16_t		icp_status;
	u_int16_t		icp_service;

	bus_dmamap_t		icp_scr_dmamap;
	bus_dma_segment_t	icp_scr_seg[1];
	void *			icp_scr;

	struct icp_ccb		*icp_ccbs;
	u_int			icp_nccbs;
	u_int			icp_flags;
	u_int			icp_qfreeze;
	u_int			icp_running;
	SIMPLEQ_HEAD(,icp_ccb)	icp_ccb_freelist;
	SIMPLEQ_HEAD(,icp_ccb)	icp_ccb_queue;
	SIMPLEQ_HEAD(,icp_ccb)	icp_ucmd_queue;
	struct callout		icp_wdog_callout;

	struct icp_ccb		*icp_ucmd_ccb;

	/* Temporary buffer for event data. */
	gdt_evt_data		icp_evt;

	void		(*icp_copy_cmd)(struct icp_softc *, struct icp_ccb *);
	u_int8_t	(*icp_get_status)(struct icp_softc *);
	void		(*icp_intr)(struct icp_softc *, struct icp_intr_ctx *);
	void		(*icp_release_event)(struct icp_softc *,
					     struct icp_ccb *);
	void		(*icp_set_sema0)(struct icp_softc *);
	int		(*icp_test_busy)(struct icp_softc *);

	/*
	 * This info is needed by the user ioctl interface needed to
	 * support the ICP configuration tools.
	 */
	int			icp_pci_bus;
	int			icp_pci_device;
	int			icp_pci_device_id;
	int			icp_pci_subdevice_id;
};

/* icp_features */
#define	ICP_FEAT_CACHESERVICE	0x01	/* cache service usable */
#define	ICP_FEAT_RAWSERVICE	0x02	/* raw service usable */

/* icp_flags */
#define	ICP_F_WAIT_CCB		0x01	/* someone waiting for CCBs */
#define	ICP_F_WAIT_FREEZE	0x02	/* someone waiting for qfreeze */

#define	ICP_HAS_WORK(icp)						\
	(! SIMPLEQ_EMPTY(&(icp)->icp_ccb_queue) ||			\
	 ! SIMPLEQ_EMPTY(&(icp)->icp_ucmd_queue))

#define	ICP_STAT_INCR(icp, x)						\
do {									\
	/* XXX Globals, for now. XXX */					\
	icp_stats. ## x ## _act++;					\
	if (icp_stats. ## x ## _act > icp_stats. ## x ## _max)		\
		icp_stats. ## x ## _max = icp_stats. ## x ## _act;	\
} while (/*CONSTCOND*/0)

#define	ICP_STAT_SET(icp, x, v)						\
do {									\
	/* XXX Globals, for now. XXX */					\
	icp_stats. ## x ## _act = (v);					\
	if (icp_stats. ## x ## _act > icp_stats. ## x ## _max)		\
		icp_stats. ## x ## _max = icp_stats. ## x ## _act;	\
} while (/*CONSTCOND*/0)

#define	ICP_STAT_DECR(icp, x)						\
do {									\
	/* XXX Globals, for now. XXX */					\
	icp_stats. ## x ## _act--;					\
} while (/*CONSTCOND*/0)

#define ICP_ISA		0x01
#define ICP_EISA	0x02
#define ICP_PCI		0x03
#define ICP_PCINEW	0x04
#define ICP_MPR		0x05
#define ICP_CLASS_MASK	0x07
#define ICP_FC		0x10
#define ICP_CLASS(icp)	((icp)->icp_class & ICP_CLASS_MASK)

int	icp_init(struct icp_softc *, const char *);
int	icp_intr(void *);

extern int icp_count;
extern gdt_statist_t icp_stats;

/*
 * Consumer interface.
 */
struct icp_attach_args {
	int		icpa_unit;
};

#define	ICPA_UNIT_SCSI	100

struct icp_ccb	*icp_ccb_alloc(struct icp_softc *);
struct icp_ccb	*icp_ccb_alloc_wait(struct icp_softc *);
void	icp_ccb_enqueue(struct icp_softc *, struct icp_ccb *);
void	icp_ccb_free(struct icp_softc *, struct icp_ccb *);
int	icp_ccb_map(struct icp_softc *, struct icp_ccb *, void *, int, int);
int	icp_ccb_poll(struct icp_softc *, struct icp_ccb *, int);
void	icp_ccb_unmap(struct icp_softc *, struct icp_ccb *);
int	icp_ccb_wait(struct icp_softc *, struct icp_ccb *, int);
int	icp_ccb_wait_user(struct icp_softc *, struct icp_ccb *, int);
int	icp_cmd(struct icp_softc *, u_int8_t, u_int16_t, u_int32_t, u_int32_t,
		u_int32_t);
int	icp_ucmd(struct icp_softc *, gdt_ucmd_t *);
int	icp_freeze(struct icp_softc *);
void	icp_unfreeze(struct icp_softc *);

void	icp_rescan(struct icp_softc *, int);
void	icp_rescan_all(struct icp_softc *);

void	icp_register_servicecb(struct icp_softc *, int,
	    const struct icp_servicecb *);

gdt_evt_str *icp_store_event(struct icp_softc *, u_int16_t, u_int16_t,
	    gdt_evt_data *);
int	icp_read_event(struct icp_softc *, int, gdt_evt_str *);
void	icp_readapp_event(struct icp_softc *, u_int8_t, gdt_evt_str *);
void	icp_clear_events(struct icp_softc *);

extern kmutex_t icp_ioctl_mutex;

#endif	/* !_IC_ICPVAR_H_ */

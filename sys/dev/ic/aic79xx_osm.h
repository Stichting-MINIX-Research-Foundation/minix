/*	$NetBSD: aic79xx_osm.h,v 1.23 2013/04/03 14:40:41 christos Exp $	*/

/*
 * NetBSD platform specific driver option settings, data structures,
 * function declarations and includes.
 *
 * Copyright (c) 1994-2001 Justin T. Gibbs.
 * Copyright (c) 2001-2002 Adaptec Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU Public License ("GPL").
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $NetBSD: aic79xx_osm.h,v 1.23 2013/04/03 14:40:41 christos Exp $
 *
 * //depot/aic7xxx/freebsd/dev/aic7xxx/aic79xx_osm.h#19 $$NetBSD: aic79xx_osm.h,v 1.23 2013/04/03 14:40:41 christos Exp $
 *
 * $FreeBSD: src/sys/dev/aic7xxx/aic79xx_osm.h,v 1.9 2003/05/26 21:43:29 gibbs Exp $
 */
/*
 * Ported from FreeBSD by Pascal Renauld, Network Storage Solutions, Inc.
 * - April 2003
 */

#ifndef _AIC79XX_NETBSD_H_
#define _AIC79XX_NETBSD_H_

#include "opt_ahd.h"	/* for config options */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/scsiio.h>
#include <sys/reboot.h>
#include <sys/kthread.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsi_message.h>
#include <dev/scsipi/scsipi_debug.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsi_iu.h>

#include <dev/ic/aic7xxx_cam.h>

/****************************** Platform Macros *******************************/
#define	SIM_IS_SCSIBUS_B(ahd, sim)	\
	(0)
#define	SIM_CHANNEL(ahd, sim)	\
	('A')
#define	SIM_SCSI_ID(ahd, sim)	\
	(ahd->our_id)
#define	SIM_PATH(ahd, sim)	\
	(ahd->platform_data->path)
#define BUILD_SCSIID(ahd, sim, target_id, our_id) \
	((((target_id) << TID_SHIFT) & TID) | (our_id))


#define SCB_GET_SIM(ahd, scb) \
	((ahd)->platform_data->sim)

#ifndef offsetof
#define offsetof(type, member)  ((size_t)(&((type *)0)->member))
#endif

/************************* Forward Declarations *******************************/
typedef pcireg_t ahd_dev_softc_t;
/***************************** Bus Space/DMA **********************************/
#define ahd_dma_tag_create(ahd, parent_tag, alignment, boundary,	\
			   lowaddr, highaddr, filter, filterarg,	\
			   maxsize, nsegments, maxsegsz, flags,		\
			   dma_tagp)					\
	bus_dma_tag_create(parent_tag, alignment, boundary,		\
			   lowaddr, highaddr, filter, filterarg,	\
			   maxsize, nsegments, maxsegsz, flags,		\
			   dma_tagp)

#define ahd_dma_tag_destroy(ahd, tag)					\
	bus_dma_tag_destroy(tag)

#define ahd_dmamem_alloc(ahd, dmat, vaddr, flags, mapp)			\
	bus_dmamem_alloc(dmat, vaddr, flags, mapp)

#define ahd_dmamem_free(ahd, dmat, vaddr, map)				\
	bus_dmamem_free(dmat, vaddr, map)

#define ahd_dmamap_create(ahd, tag, flags, mapp)			\
	bus_dmamap_create(tag, flags, mapp)

#define ahd_dmamap_destroy(ahd, tag, map)				\
	bus_dmamap_destroy(tag, map)

#define ahd_dmamap_load(ahd, dmat, map, addr, buflen, callback,		\
			callback_arg, flags)				\
	bus_dmamap_load(dmat, map, addr, buflen, callback, callback_arg, flags)

#define ahd_dmamap_unload(ahd, tag, map)				\
	bus_dmamap_unload(tag, map)

/* XXX Need to update Bus DMA for partial map syncs */
#define ahd_dmamap_sync(ahd, dma_tag, dmamap, offset, len, op)		\
	bus_dmamap_sync(dma_tag, dmamap, offset, len, op)

/************************ Tunable Driver Parameters  **************************/
/*
 * The number of DMA segments supported.  The sequencer can handle any number
 * of physically contiguous S/G entrys.  To reduce the driver's memory
 * consumption, we limit the number supported to be sufficient to handle
 * the largest mapping supported by the kernel, MAXPHYS.  Assuming the
 * transfer is as fragmented as possible and unaligned, this turns out to
 * be the number of paged sized transfers in MAXPHYS plus an extra element
 * to handle any unaligned residual.  The sequencer fetches SG elements
 * in cacheline sized chucks, so make the number per-transaction an even
 * multiple of 16 which should align us on even the largest of cacheline
 * boundaries.
 */
#define AHD_NSEG (roundup(btoc(MAXPHYS) + 1, 16))

/* This driver supports target mode */
#if NOT_YET
#define AHD_TARGET_MODE 1
#endif

/************************** Softc/SCB Platform Data ***************************/
struct ahd_platform_data {
};

struct scb_platform_data {
};

/********************************* Byte Order *********************************/
#define ahd_htobe16(x) htobe16(x)
#define ahd_htobe32(x) htobe32(x)
#define ahd_htobe64(x) htobe64(x)
#define ahd_htole16(x) htole16(x)
#define ahd_htole32(x) htole32(x)
#define ahd_htole64(x) htole64(x)

#define ahd_be16toh(x) be16toh(x)
#define ahd_be32toh(x) be32toh(x)
#define ahd_be64toh(x) be64toh(x)
#define ahd_le16toh(x) le16toh(x)
#define ahd_le32toh(x) le32toh(x)
#define ahd_le64toh(x) le64toh(x)

/************************** Timer DataStructures ******************************/
typedef struct callout ahd_timer_t;

/***************************** Core Includes **********************************/
#if AHD_REG_PRETTY_PRINT
#define AIC_DEBUG_REGISTERS 1
#else
#define AIC_DEBUG_REGISTERS 0
#endif
#include <dev/ic/aic79xxvar.h>

/***************************** Timer Facilities *******************************/
void ahd_timeout(void*);
#define ahd_timer_init(timer) callout_init(timer, 0)
#define ahd_timer_stop callout_stop

static __inline void
ahd_timer_reset(ahd_timer_t *timer, u_int usec, ahd_callback_t *func, void *arg)
{
	callout_reset(timer, (usec * hz)/1000000, func, arg);
}

static __inline void
ahd_scb_timer_reset(struct scb *scb, u_int usec)
{
	if (!(scb->xs->xs_control & XS_CTL_POLL)) {
		callout_reset(&scb->xs->xs_callout,
			      (usec * hz)/1000000, ahd_timeout, scb);
	}
}

/*************************** Device Access ************************************/
#define ahd_inb(ahd, port)					\
	bus_space_read_1((ahd)->tags[(port) >> 8],		\
			 (ahd)->bshs[(port) >> 8], (port) & 0xFF)

#define ahd_outb(ahd, port, value)				\
	bus_space_write_1((ahd)->tags[(port) >> 8],		\
			  (ahd)->bshs[(port) >> 8], (port) & 0xFF, value)

#define ahd_inw_atomic(ahd, port)				\
	ahd_le16toh(bus_space_read_2((ahd)->tags[(port) >> 8],	\
				     (ahd)->bshs[(port) >> 8], (port) & 0xFF))

#define ahd_outw_atomic(ahd, port, value)			\
	bus_space_write_2((ahd)->tags[(port) >> 8],		\
			  (ahd)->bshs[(port) >> 8],		\
			  (port & 0xFF), ahd_htole16(value))

#define ahd_outsb(ahd, port, valp, count)			\
	bus_space_write_multi_1((ahd)->tags[(port) >> 8],	\
				(ahd)->bshs[(port) >> 8],	\
				(port & 0xFF), valp, count)

#define ahd_insb(ahd, port, valp, count)			\
	bus_space_read_multi_1((ahd)->tags[(port) >> 8],	\
			       (ahd)->bshs[(port) >> 8],	\
			       (port & 0xFF), valp, count)

static __inline void ahd_flush_device_writes(struct ahd_softc *);

static __inline void
ahd_flush_device_writes(struct ahd_softc *ahd)
{
	/* XXX Is this sufficient for all architectures??? */
	ahd_inb(ahd, INTSTAT);
}

/**************************** Locking Primitives ******************************/
/* Lock protecting internal data structures */
static __inline void ahd_lockinit(struct ahd_softc *);
static __inline void ahd_lock(struct ahd_softc *, int *);
static __inline void ahd_unlock(struct ahd_softc *, int *);

/* Lock held during command completion to the upper layer */
static __inline void ahd_done_lockinit(struct ahd_softc *);
static __inline void ahd_done_lock(struct ahd_softc *, unsigned long *);
static __inline void ahd_done_unlock(struct ahd_softc *, unsigned long *);

/* Lock held during ahd_list manipulation and ahd softc frees */
static __inline void ahd_list_lockinit(void);
static __inline void ahd_list_lock(unsigned long *);
static __inline void ahd_list_unlock(unsigned long *);

static __inline void
ahd_lockinit(struct ahd_softc *ahd)
{
}

static __inline void
ahd_lock(struct ahd_softc *ahd, int *flags)
{
	*flags = splbio();
}

static __inline void
ahd_unlock(struct ahd_softc *ahd, int *flags)
{
	splx(*flags);
}

/* Lock held during command completion to the upper layer */
static __inline void
ahd_done_lockinit(struct ahd_softc *ahd)
{
}

static __inline void
ahd_done_lock(struct ahd_softc *ahd, unsigned long *flags)
{
}

static __inline void
ahd_done_unlock(struct ahd_softc *ahd, unsigned long *flags)
{
}

/* Lock held during ahd_list manipulation and ahd softc frees */
static __inline void
ahd_list_lockinit(void)
{
}

static __inline void
ahd_list_lock(unsigned long *flags)
{
}

static __inline void
ahd_list_unlock(unsigned long *flags)
{
}
/****************************** OS Primitives *********************************/
#define ahd_delay DELAY

/************************** Transaction Operations ****************************/
static __inline void ahd_set_transaction_status(struct scb *, uint32_t);
static __inline void ahd_set_scsi_status(struct scb *, uint32_t);
static __inline void ahd_set_xfer_status(struct scb *, uint32_t);
static __inline uint32_t ahd_get_transaction_status(struct scb *);
static __inline uint32_t ahd_get_scsi_status(struct scb *);
static __inline void ahd_set_transaction_tag(struct scb *, int, u_int);
static __inline u_long ahd_get_transfer_length(struct scb *);
static __inline int ahd_get_transfer_dir(struct scb *);
static __inline void ahd_set_residual(struct scb *, u_long);
static __inline void ahd_set_sense_residual(struct scb *, u_long);
static __inline u_long ahd_get_residual(struct scb *);
static __inline int ahd_perform_autosense(struct scb *);
static __inline uint32_t ahd_get_sense_bufsize(struct ahd_softc*, struct scb*);
static __inline void ahd_freeze_simq(struct ahd_softc *);
static __inline void ahd_release_simq(struct ahd_softc *);
static __inline void ahd_freeze_scb(struct scb *);
static __inline void ahd_platform_freeze_devq(struct ahd_softc *, struct scb *);
static __inline int  ahd_platform_abort_scbs(struct ahd_softc *, int,
    char, int, u_int, role_t, uint32_t);

static __inline
void ahd_set_transaction_status(struct scb *scb, uint32_t status)
{
	scb->xs->error = status;
}

static __inline
void ahd_set_scsi_status(struct scb *scb, uint32_t status)
{
	scb->xs->xs_status = status;
}

static __inline
void ahd_set_xfer_status(struct scb *scb, uint32_t status)
{
	scb->xs->status = status;
}

static __inline
uint32_t ahd_get_transaction_status(struct scb *scb)
{
	return (scb->xs->error);
}

static __inline
uint32_t ahd_get_scsi_status(struct scb *scb)
{
	return (scb->xs->xs_status);
}

static __inline
void ahd_set_transaction_tag(struct scb *scb, int enabled, u_int type)
{
	scb->xs->xs_tag_type = type;
}

static __inline
u_long ahd_get_transfer_length(struct scb *scb)
{
	return (scb->xs->datalen);
}

static __inline
int ahd_get_transfer_dir(struct scb *scb)
{
	return (scb->xs->xs_control & (XS_CTL_DATA_IN|XS_CTL_DATA_OUT));
}

static __inline
void ahd_set_residual(struct scb *scb, u_long resid)
{
	scb->xs->resid = resid;
}


static __inline
void ahd_set_sense_residual(struct scb *scb, u_long resid)
{
	//scb->xs->sense.scsi_sense.extra_len = resid; /* ??? */
}


static __inline
u_long ahd_get_residual(struct scb *scb)
{
	return (scb->xs->resid);
}

static __inline
int ahd_perform_autosense(struct scb *scb)
{
	return (!(scb->flags & SCB_SENSE));
}

static __inline uint32_t
ahd_get_sense_bufsize(struct ahd_softc *ahd, struct scb *scb)
{
	return (sizeof(struct scsi_sense_data));
}

static __inline void
ahd_freeze_simq(struct ahd_softc *ahd)
{
	/* do nothing for now */
}

static __inline void
ahd_release_simq(struct ahd_softc *ahd)
{
	/* do nothing for now */
}

static __inline void
ahd_freeze_scb(struct scb *scb)
{
	struct scsipi_xfer *xs = scb->xs;

	if (!(scb->flags & SCB_FREEZE_QUEUE)) {
	 	scsipi_periph_freeze(xs->xs_periph, 1);
		scb->flags |= SCB_FREEZE_QUEUE;
	}
}

static __inline void
ahd_platform_freeze_devq(struct ahd_softc *ahd, struct scb *scb)
{
	/* Nothing to do here for NetBSD */
}

static __inline int
ahd_platform_abort_scbs(struct ahd_softc *ahd, int target,
    char channel, int lun, u_int tag,
    role_t role, uint32_t status)
{
	/* Nothing to do here for NetBSD */
	return (0);
}

static __inline void
ahd_platform_scb_free(struct ahd_softc *ahd, struct scb *scb)
{
#ifdef __FreeBSD__
	/* What do we do to generically handle driver resource shortages??? */
	if ((ahd->flags & AHD_RESOURCE_SHORTAGE) != 0
	 && scb->io_ctx != NULL
	 && (scb->io_ctx->ccb_h.status & CAM_RELEASE_SIMQ) == 0) {
		scb->io_ctx->ccb_h.status |= CAM_RELEASE_SIMQ;
		ahd->flags &= ~AHD_RESOURCE_SHORTAGE;
	}
	scb->io_ctx = NULL;
#endif /* end of modification */
}

/********************************** PCI ***************************************/
#if AHD_PCI_CONFIG > 0
static __inline uint32_t ahd_pci_read_config(ahd_dev_softc_t, int, int);
static __inline void	 ahd_pci_write_config(ahd_dev_softc_t, int,
    uint32_t, int);
static __inline int	 ahd_get_pci_function(ahd_dev_softc_t);
static __inline int	 ahd_get_pci_slot(ahd_dev_softc_t);
static __inline int	 ahd_get_pci_bus(ahd_dev_softc_t);

int			 ahd_pci_map_registers(struct ahd_softc *);
int			 ahd_pci_map_int(struct ahd_softc *);

static __inline uint32_t
ahd_pci_read_config(ahd_dev_softc_t pci, int reg, int width)
{
	return (pci_read_config(pci, reg, width));
}

static __inline void
ahd_pci_write_config(ahd_dev_softc_t pci, int reg, uint32_t value, int width)
{
	pci_write_config(pci, reg, value, width);
}

static __inline int
ahd_get_pci_function(ahd_dev_softc_t pci)
{
	return (pci_get_function(pci));
}

static __inline int
ahd_get_pci_slot(ahd_dev_softc_t pci)
{
	return (pci_get_slot(pci));
}


static __inline int
ahd_get_pci_bus(ahd_dev_softc_t pci)
{
	return (pci_get_bus(pci));
}
#endif

typedef enum
{
	AHD_POWER_STATE_D0,
	AHD_POWER_STATE_D1,
	AHD_POWER_STATE_D2,
	AHD_POWER_STATE_D3
} ahd_power_state;

void ahd_power_state_change(struct ahd_softc *, ahd_power_state);

/******************************** VL/EISA *************************************/
int aic7770_map_registers(struct ahd_softc *);
int aic7770_map_int(struct ahd_softc *, int);

/********************************* Debug **************************************/
static __inline void	ahd_print_path(struct ahd_softc *, struct scb *);
static __inline void	ahd_platform_dump_card_state(struct ahd_softc *);
static __inline void
ahd_print_path(struct ahd_softc *ahd, struct scb *scb)
{
	printf("%s:", device_xname(ahd->sc_dev));
}

static __inline void
ahd_platform_dump_card_state(struct ahd_softc *ahd)
{
	/* Nothing to do here for NetBSD */
}
/**************************** Transfer Settings *******************************/
void	  ahd_notify_xfer_settings_change(struct ahd_softc *,
					  struct ahd_devinfo *);
void	  ahd_platform_set_tags(struct ahd_softc *, struct ahd_devinfo *,
				ahd_queue_alg);

/************************* Initialization/Teardown ****************************/
int	  ahd_platform_alloc(struct ahd_softc *, void *);
void	  ahd_platform_free(struct ahd_softc *);
int	  ahd_map_int(struct ahd_softc *);
int	  ahd_attach(struct ahd_softc *);
int	  ahd_softc_comp(struct ahd_softc *, struct ahd_softc *);
int 	  ahd_detach(struct ahd_softc *, int);
#define	ahd_platform_init(arg)


/****************************** Interrupts ************************************/
void			ahd_platform_intr(void *);
static __inline void	ahd_platform_flushwork(struct ahd_softc *);
static __inline void
ahd_platform_flushwork(struct ahd_softc *ahd)
{
	/* Nothing to do here for NetBSD */
}

/************************ Misc Function Declarations **************************/
void	ahd_done(struct ahd_softc *, struct scb *);
void	ahd_send_async(struct ahd_softc *, char, u_int, u_int, ac_code, void *);

#endif  /* _AIC79XX_NETBSD_H_ */

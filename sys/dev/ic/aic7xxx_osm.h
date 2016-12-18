/*	$NetBSD: aic7xxx_osm.h,v 1.25 2010/11/13 13:52:00 uebayasi Exp $	*/

/*
 * NetBSD platform specific driver option settings, data structures,
 * function declarations and includes.
 *
 * Copyright (c) 1994-2001 Justin T. Gibbs.
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
 * //depot/aic7xxx/freebsd/dev/aic7xxx/aic7xxx_osm.h#14 $
 *
 * $FreeBSD: /repoman/r/ncvs/src/sys/dev/aic7xxx/aic7xxx_osm.h,v 1.20 2002/12/04 22:51:29 scottl Exp $
 */
/*
 * Ported from FreeBSD by Pascal Renauld, Network Storage Solutions, Inc. - April 2003
 */

#ifndef _AIC7XXX_NETBSD_H_
#define _AIC7XXX_NETBSD_H_

#include "opt_ahc.h"	/* for config options */

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

#ifdef CAM_NEW_TRAN_CODE
#define AHC_NEW_TRAN_SETTINGS
#endif /* CAM_NEW_TRAN_CODE */


/****************************** Platform Macros *******************************/
#define	SIM_IS_SCSIBUS_B(ahc, sim)	\
	((sim) == ahc->platform_data->sim_b)
#define	SIM_CHANNEL(ahc, sim)	\
	(((sim) == ahc->platform_data->sim_b) ? 'B' : 'A')
#define	SIM_SCSI_ID(ahc, sim)	\
	(((sim) == ahc->platform_data->sim_b) ? ahc->our_id_b : ahc->our_id)
#define	SIM_PATH(ahc, sim)	\
	(((sim) == ahc->platform_data->sim_b) ? ahc->platform_data->path_b \
					      : ahc->platform_data->path)
#define BUILD_SCSIID(ahc, sim, target_id, our_id) \
	((((target_id) << TID_SHIFT) & TID) | (our_id))

#define SCB_GET_SIM(ahc, scb) \
	(SCB_GET_CHANNEL(ahc, scb) == 'A' ? (ahc)->platform_data->sim \
					  : (ahc)->platform_data->sim_b)

#ifndef offsetof
#define offsetof(type, member)  ((size_t)(&((type *)0)->member))
#endif
/************************* Forward Declarations *******************************/
typedef pcireg_t ahc_dev_softc_t;

/***************************** Bus Space/DMA **********************************/
#define ahc_dma_tag_create(ahc, parent_tag, alignment, boundary,	\
			   lowaddr, highaddr, filter, filterarg,	\
			   maxsize, nsegments, maxsegsz, flags,		\
			   dma_tagp)					\
	bus_dma_tag_create(parent_tag, alignment, boundary,		\
			   lowaddr, highaddr, filter, filterarg,	\
			   maxsize, nsegments, maxsegsz, flags,		\
			   dma_tagp)

#define ahc_dma_tag_destroy(ahc, tag)					\
	bus_dma_tag_destroy(tag)

#define ahc_dmamem_alloc(ahc, dmat, vaddr, flags, mapp)			\
	bus_dmamem_alloc(dmat, vaddr, flags, mapp)

#define ahc_dmamem_free(ahc, dmat, vaddr, map)				\
	bus_dmamem_free(dmat, vaddr, map)

#define ahc_dmamap_create(ahc, tag, flags, mapp)			\
	bus_dmamap_create(tag, flags, mapp)

#define ahc_dmamap_destroy(ahc, tag, map)				\
	bus_dmamap_destroy(tag, map)

#define ahc_dmamap_load(ahc, dmat, map, addr, buflen, callback,		\
			callback_arg, flags)				\
	bus_dmamap_load(dmat, map, addr, buflen, callback, callback_arg, flags)

#define ahc_dmamap_unload(ahc, tag, map)				\
	bus_dmamap_unload(tag, map)

/* XXX Need to update Bus DMA for partial map syncs */
#define ahc_dmamap_sync(ahc, dma_tag, dmamap, offset, len, op)		\
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
#define AHC_NSEG (roundup(btoc(MAXPHYS) + 1, 16))

/* This driver supports target mode */
//#define AHC_TARGET_MODE 1

/************************** Softc/SCB Platform Data ***************************/
struct ahc_platform_data {
	/*
	 * Hooks into the XPT.
	 */
#if 0
	struct	cam_sim		*sim;
	struct	cam_sim		*sim_b;
	struct	cam_path	*path;
	struct	cam_path	*path_b;

	int			 regs_res_type;
	int			 regs_res_id;
	int			 irq_res_type;
	struct resource		*regs;
	struct resource		*irq;
	void			*ih;
	eventhandler_tag	 eh;
#endif
};

struct scb_platform_data {
};

/********************************* Byte Order *********************************/
#define ahc_htobe16(x) htobe16(x)
#define ahc_htobe32(x) htobe32(x)
#define ahc_htobe64(x) htobe64(x)
#define ahc_htole16(x) htole16(x)
#define ahc_htole32(x) htole32(x)
#define ahc_htole64(x) htole64(x)

#define ahc_be16toh(x) be16toh(x)
#define ahc_be32toh(x) be32toh(x)
#define ahc_be64toh(x) be64toh(x)
#define ahc_le16toh(x) le16toh(x)
#define ahc_le32toh(x) le32toh(x)
#define ahc_le64toh(x) le64toh(x)

/************************** Timer DataStructures ******************************/
typedef struct callout ahc_timer_t;

/***************************** Core Includes **********************************/
#if AHC_REG_PRETTY_PRINT
#define AIC_DEBUG_REGISTERS 1
#else
#define AIC_DEBUG_REGISTERS 0
#endif

#include <dev/ic/aic7xxxvar.h>

/***************************** Timer Facilities *******************************/
void ahc_timeout(void*);

#define ahc_timer_init(x) callout_init(x, 0)
#define ahc_timer_stop callout_stop

static __inline void
ahc_timer_reset(ahc_timer_t *timer, u_int usec, ahc_callback_t *func, void *arg)
{
	callout_reset(timer, (usec * hz)/1000000, func, arg);
}

static __inline void
ahc_scb_timer_reset(struct scb *scb, u_int usec)
{
	if (!(scb->xs->xs_control & XS_CTL_POLL)) {
		callout_reset(&scb->xs->xs_callout,
			      (usec * hz)/1000000, ahc_timeout, scb);
	}
}

/*************************** Device Access ************************************/
#define ahc_inb(ahc, port)				\
	bus_space_read_1((ahc)->tag, (ahc)->bsh, port)

#define ahc_outb(ahc, port, value)			\
	bus_space_write_1((ahc)->tag, (ahc)->bsh, port, value)

#define ahc_outsb(ahc, port, valp, count)		\
	bus_space_write_multi_1((ahc)->tag, (ahc)->bsh, port, valp, count)

#define ahc_insb(ahc, port, valp, count)		\
	bus_space_read_multi_1((ahc)->tag, (ahc)->bsh, port, valp, count)

static __inline void ahc_flush_device_writes(struct ahc_softc *);

static __inline void
ahc_flush_device_writes(struct ahc_softc *ahc)
{
	/* XXX Is this sufficient for all architectures??? */
	(void)ahc_inb(ahc, INTSTAT);
}

/**************************** Locking Primitives ******************************/
/* Lock protecting internal data structures */
static __inline void ahc_lockinit(struct ahc_softc *);
static __inline void ahc_lock(struct ahc_softc *, unsigned long *);
static __inline void ahc_unlock(struct ahc_softc *, unsigned long *);

/* Lock held during command completion to the upper layer */
static __inline void ahc_done_lockinit(struct ahc_softc *);
static __inline void ahc_done_lock(struct ahc_softc *, unsigned long *);
static __inline void ahc_done_unlock(struct ahc_softc *, unsigned long *);

/* Lock held during ahc_list manipulation and ahc softc frees */
static __inline void ahc_list_lockinit(void);
static __inline void ahc_list_lock(unsigned long *);
static __inline void ahc_list_unlock(unsigned long *);

static __inline void
ahc_lockinit(struct ahc_softc *ahc)
{
}

static __inline void
ahc_lock(struct ahc_softc *ahc, unsigned long *flags)
{
	*flags = splbio();
}

static __inline void
ahc_unlock(struct ahc_softc *ahc, unsigned long *flags)
{
	splx(*flags);
}

/* Lock held during command completion to the upper layer */
static __inline void
ahc_done_lockinit(struct ahc_softc *ahc)
{
}

static __inline void
ahc_done_lock(struct ahc_softc *ahc, unsigned long *flags)
{
}

static __inline void
ahc_done_unlock(struct ahc_softc *ahc, unsigned long *flags)
{
}

/* Lock held during ahc_list manipulation and ahc softc frees */
static __inline void
ahc_list_lockinit(void)
{
}

static __inline void
ahc_list_lock(unsigned long *flags)
{
}

static __inline void
ahc_list_unlock(unsigned long *flags)
{
}
/****************************** OS Primitives *********************************/
#define ahc_delay DELAY

/************************** Transaction Operations ****************************/
static __inline void ahc_set_transaction_status(struct scb *, uint32_t);
static __inline void ahc_set_scsi_status(struct scb *, uint32_t);
static __inline uint32_t ahc_get_transaction_status(struct scb *);
static __inline uint32_t ahc_get_scsi_status(struct scb *);
static __inline void ahc_set_transaction_tag(struct scb *, int, u_int);
static __inline u_long ahc_get_transfer_length(struct scb *);
static __inline int ahc_get_transfer_dir(struct scb *);
static __inline void ahc_set_residual(struct scb *, u_long);
static __inline void ahc_set_sense_residual(struct scb *, u_long);
static __inline u_long ahc_get_residual(struct scb *);
static __inline int ahc_perform_autosense(struct scb *);
static __inline uint32_t ahc_get_sense_bufsize(struct ahc_softc *,
    struct scb *);
static __inline void ahc_freeze_scb(struct scb *);
static __inline void ahc_platform_freeze_devq(struct ahc_softc *, struct scb *);
static __inline int  ahc_platform_abort_scbs(struct ahc_softc *, int, char,
    int, u_int, role_t, uint32_t);

static __inline
void ahc_set_transaction_status(struct scb *scb, uint32_t status)
{
	scb->xs->error = status;
}

static __inline
void ahc_set_scsi_status(struct scb *scb, uint32_t status)
{
	scb->xs->xs_status = status;
}

static __inline
uint32_t ahc_get_transaction_status(struct scb *scb)
{
	return (scb->xs->error);
}

static __inline
uint32_t ahc_get_scsi_status(struct scb *scb)
{
	return (scb->xs->xs_status);
}

static __inline
void ahc_set_transaction_tag(struct scb *scb, int enabled, u_int type)
{
	scb->xs->xs_tag_type = type;
}

static __inline
u_long ahc_get_transfer_length(struct scb *scb)
{
	return (scb->xs->datalen);
}

static __inline
int ahc_get_transfer_dir(struct scb *scb)
{
	return (scb->xs->xs_control & (XS_CTL_DATA_IN|XS_CTL_DATA_OUT));
}

static __inline
void ahc_set_residual(struct scb *scb, u_long resid)
{
	scb->xs->resid = resid;
}

static __inline
void ahc_set_sense_residual(struct scb *scb, u_long resid)
{
#ifdef notdef
    scb->io_ctx->csio.sense_resid = resid;
#endif
}

static __inline
u_long ahc_get_residual(struct scb *scb)
{
	return (scb->xs->resid);
}

static __inline
int ahc_perform_autosense(struct scb *scb)
{
	return (!(scb->flags & SCB_SENSE));
}

static __inline uint32_t
ahc_get_sense_bufsize(struct ahc_softc *ahc, struct scb *scb)
{
	return (sizeof(struct scsi_sense_data));
}

static __inline void
ahc_freeze_scb(struct scb *scb)
{
	struct scsipi_xfer *xs = scb->xs;

	if (!(scb->flags & SCB_FREEZE_QUEUE)) {
	 	scsipi_periph_freeze(xs->xs_periph, 1);
		scb->flags |= SCB_FREEZE_QUEUE;
	}
}

static __inline void
ahc_platform_freeze_devq(struct ahc_softc *ahc, struct scb *scb)
{
}

static __inline int
ahc_platform_abort_scbs(struct ahc_softc *ahc, int target,
    char channel, int lun, u_int tag,
    role_t role, uint32_t status)
{
	return (0);
}

static __inline void
ahc_platform_scb_free(struct ahc_softc *ahc, struct scb *scb)
{
#ifdef __FreeBSD__
	/* What do we do to generically handle driver resource shortages??? */
	if ((ahc->flags & AHC_RESOURCE_SHORTAGE) != 0
	 && scb->io_ctx != NULL
	 && (scb->io_ctx->ccb_h.status & CAM_RELEASE_SIMQ) == 0) {
		scb->io_ctx->ccb_h.status |= CAM_RELEASE_SIMQ;
		ahc->flags &= ~AHC_RESOURCE_SHORTAGE;
	}
	scb->io_ctx = NULL;
#endif
}

/********************************** PCI ***************************************/
#ifdef AHC_PCI_CONFIG
static __inline uint32_t ahc_pci_read_config(ahc_dev_softc_t, int, int);
static __inline void	 ahc_pci_write_config(ahc_dev_softc_t, int, uint32_t,
    int);
static __inline int	 ahc_get_pci_function(ahc_dev_softc_t);
static __inline int	 ahc_get_pci_slot(ahc_dev_softc_t);
static __inline int	 ahc_get_pci_bus(ahc_dev_softc_t);

int			 ahc_pci_map_registers(struct ahc_softc *);
int			 ahc_pci_map_int(struct ahc_softc *);

static __inline uint32_t
ahc_pci_read_config(ahc_dev_softc_t pci, int reg, int width)
{
	return (pci_read_config(pci, reg, width));
}

static __inline void
ahc_pci_write_config(ahc_dev_softc_t pci, int reg, uint32_t value, int width)
{
	pci_write_config(pci, reg, value, width);
}

static __inline int
ahc_get_pci_function(ahc_dev_softc_t pci)
{
	return (pci_get_function(pci));
}

static __inline int
ahc_get_pci_slot(ahc_dev_softc_t pci)
{
	return (pci_get_slot(pci));
}

static __inline int
ahc_get_pci_bus(ahc_dev_softc_t pci)
{
	return (pci_get_bus(pci));
}

typedef enum
{
	AHC_POWER_STATE_D0,
	AHC_POWER_STATE_D1,
	AHC_POWER_STATE_D2,
	AHC_POWER_STATE_D3
} ahc_power_state;

void ahc_power_state_change(struct ahc_softc *, ahc_power_state);
#endif
/******************************** VL/EISA *************************************/
int aic7770_map_registers(struct ahc_softc *, u_int);
int aic7770_map_int(struct ahc_softc *, int);

/********************************* Debug **************************************/
static __inline void	ahc_print_path(struct ahc_softc *, struct scb *);
static __inline void	ahc_platform_dump_card_state(struct ahc_softc *);

static __inline void
ahc_print_path(struct ahc_softc *ahc, struct scb *scb)
{
	printf("%s:", device_xname(ahc->sc_dev));
}

static __inline void
ahc_platform_dump_card_state(struct ahc_softc *ahc)
{
}
/**************************** Transfer Settings *******************************/
void	  ahc_notify_xfer_settings_change(struct ahc_softc *,
					  struct ahc_devinfo *);
void	  ahc_platform_set_tags(struct ahc_softc *, struct ahc_devinfo *, int);

/************************* Initialization/Teardown ****************************/
int	  ahc_platform_alloc(struct ahc_softc *, void *);
void	  ahc_platform_free(struct ahc_softc *);
int	  ahc_map_int(struct ahc_softc *);
int	  ahc_attach(struct ahc_softc *);
int	  ahc_softc_comp(struct ahc_softc *, struct ahc_softc *);
int	  ahc_detach(struct ahc_softc *, int);

/****************************** Interrupts ************************************/
void			ahc_platform_intr(void *);
static __inline void	ahc_platform_flushwork(struct ahc_softc *);
static __inline void
ahc_platform_flushwork(struct ahc_softc *ahc)
{
}

/************************ Misc Function Declarations **************************/
void	  ahc_done(struct ahc_softc *, struct scb *);
void	  ahc_send_async(struct ahc_softc *, char, u_int, u_int, ac_code,
    void *);
#endif  /* _AIC7XXX_NETBSD_H_ */

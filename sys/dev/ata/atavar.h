/*	$NetBSD: atavar.h,v 1.92 2014/09/10 07:04:48 matt Exp $	*/

/*
 * Copyright (c) 1998, 2001 Manuel Bouyer.
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

#ifndef _DEV_ATA_ATAVAR_H_
#define	_DEV_ATA_ATAVAR_H_

#include <sys/lock.h>
#include <sys/queue.h>

#include <dev/ata/ataconf.h>

/* XXX For scsipi_adapter and scsipi_channel. */
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/atapiconf.h>

/*
 * Description of a command to be handled by an ATA controller.  These
 * commands are queued in a list.
 */
struct ata_xfer {
	volatile u_int c_flags;	/* command state flags */

	/* Channel and drive that are to process the request. */
	struct ata_channel *c_chp;
	int	c_drive;

	void	*c_cmd;			/* private request structure pointer */
	void	*c_databuf;		/* pointer to data buffer */
	int	c_bcount;		/* byte count left */
	int	c_skip;			/* bytes already transferred */
	int	c_dscpoll;		/* counter for dsc polling (ATAPI) */
	int	c_lenoff;		/* offset to c_bcount (ATAPI) */

	/* Link on the command queue. */
	TAILQ_ENTRY(ata_xfer) c_xferchain;

	/* Low-level protocol handlers. */
	void	(*c_start)(struct ata_channel *, struct ata_xfer *);
	int	(*c_intr)(struct ata_channel *, struct ata_xfer *, int);
	void	(*c_kill_xfer)(struct ata_channel *, struct ata_xfer *, int);
};

/* flags in c_flags */
#define	C_ATAPI		0x0001		/* xfer is ATAPI request */
#define	C_TIMEOU	0x0002		/* xfer processing timed out */
#define	C_POLL		0x0004		/* command is polled */
#define	C_DMA		0x0008		/* command uses DMA */
#define C_WAIT		0x0010		/* can use tsleep */
#define C_WAITACT	0x0020		/* wakeup when active */
#define C_FREE		0x0040		/* call ata_free_xfer() asap */
#define C_PIOBM		0x0080		/* command uses busmastering PIO */

/* reasons for c_kill_xfer() */
#define KILL_GONE 1 /* device is gone */
#define KILL_RESET 2 /* xfer was reset */

/* Per-channel queue of ata_xfers.  May be shared by multiple channels. */
struct ata_queue {
	TAILQ_HEAD(, ata_xfer) queue_xfer; /* queue of pending commands */
	int queue_freeze; /* freeze count for the queue */
	struct ata_xfer *active_xfer; /* active command */
	int queue_flags;	/* flags for this queue */
#define QF_IDLE_WAIT   0x01    /* someone is wants the controller idle */
};

/* ATA bus instance state information. */
struct atabus_softc {
	device_t sc_dev;
	struct ata_channel *sc_chan;
	int sc_flags;
#define ATABUSCF_OPEN	0x01
};

/*
 * A queue of atabus instances, used to ensure the same bus probe order
 * for a given hardware configuration at each boot.
 */
struct atabus_initq {
	TAILQ_ENTRY(atabus_initq) atabus_initq;
	struct atabus_softc *atabus_sc;
};

/* High-level functions and structures used by both ATA and ATAPI devices */
struct ataparams;

/* Datas common to drives and controller drivers */
struct ata_drive_datas {
	uint8_t drive;		/* drive number */
	int8_t ata_vers;	/* ATA version supported */
	uint16_t drive_flags;	/* bitmask for drives present/absent and cap */
#define	ATA_DRIVE_CAP32		0x0001
#define	ATA_DRIVE_DMA		0x0002
#define	ATA_DRIVE_UDMA		0x0004
#define	ATA_DRIVE_MODE		0x0008	/* the drive reported its mode */
#define	ATA_DRIVE_RESET		0x0010	/* reset the drive state at next xfer */
#define	ATA_DRIVE_WAITDRAIN	0x0020	/* device is waiting for the queue to drain */
#define	ATA_DRIVE_NOSTREAM	0x0040	/* no stream methods on this drive */
#define ATA_DRIVE_ATAPIDSCW	0x0080	/* needs to wait for DSC in phase_complete */

	uint8_t drive_type;
#define	ATA_DRIVET_NONE		0
#define	ATA_DRIVET_ATA		1
#define	ATA_DRIVET_ATAPI	2
#define	ATA_DRIVET_OLD		3
#define	ATA_DRIVET_PM		4

	/*
	 * Current setting of drive's PIO, DMA and UDMA modes.
	 * Is initialised by the disks drivers at attach time, and may be
	 * changed later by the controller's code if needed
	 */
	uint8_t PIO_mode;	/* Current setting of drive's PIO mode */
#if NATA_DMA
	uint8_t DMA_mode;	/* Current setting of drive's DMA mode */
#if NATA_UDMA
	uint8_t UDMA_mode;	/* Current setting of drive's UDMA mode */
#endif
#endif

	/* Supported modes for this drive */
	uint8_t PIO_cap;	/* supported drive's PIO mode */
#if NATA_DMA
	uint8_t DMA_cap;	/* supported drive's DMA mode */
#if NATA_UDMA
	uint8_t UDMA_cap;	/* supported drive's UDMA mode */
#endif
#endif

	/*
	 * Drive state.
	 * This is reset to 0 after a channel reset.
	 */
	uint8_t state;

#define RESET          0
#define READY          1

#if NATA_DMA
	/* numbers of xfers and DMA errs. Used by ata_dmaerr() */
	uint8_t n_dmaerrs;
	uint32_t n_xfers;

	/* Downgrade after NERRS_MAX errors in at most NXFER xfers */
#define NERRS_MAX 4
#define NXFER 4000
#endif

	/* Callbacks into the drive's driver. */
	void	(*drv_done)(void *);	/* transfer is done */

	device_t drv_softc;	/* ATA drives softc, if any */
	void *chnl_softc;		/* channel softc */
};

/* User config flags that force (or disable) the use of a mode */
#define ATA_CONFIG_PIO_MODES	0x0007
#define ATA_CONFIG_PIO_SET	0x0008
#define ATA_CONFIG_PIO_OFF	0
#define ATA_CONFIG_DMA_MODES	0x0070
#define ATA_CONFIG_DMA_SET	0x0080
#define ATA_CONFIG_DMA_DISABLE	0x0070
#define ATA_CONFIG_DMA_OFF	4
#define ATA_CONFIG_UDMA_MODES	0x0700
#define ATA_CONFIG_UDMA_SET	0x0800
#define ATA_CONFIG_UDMA_DISABLE	0x0700
#define ATA_CONFIG_UDMA_OFF	8

/*
 * Parameters/state needed by the controller to perform an ATA bio.
 */
struct ata_bio {
	volatile uint16_t flags;/* cmd flags */
/* 			0x0001	free, was ATA_NOSLEEP */
#define	ATA_POLL	0x0002	/* poll for completion */
#define	ATA_ITSDONE	0x0004	/* the transfer is as done as it gets */
#define	ATA_SINGLE	0x0008	/* transfer must be done in singlesector mode */
#define	ATA_LBA		0x0010	/* transfer uses LBA addressing */
#define	ATA_READ	0x0020	/* transfer is a read (otherwise a write) */
#define	ATA_CORR	0x0040	/* transfer had a corrected error */
#define	ATA_LBA48	0x0080	/* transfer uses 48-bit LBA addressing */
	int		multi;	/* # of blocks to transfer in multi-mode */
	struct disklabel *lp;	/* pointer to drive's label info */
	daddr_t		blkno;	/* block addr */
	daddr_t		blkdone;/* number of blks transferred */
	daddr_t		nblks;	/* number of block currently transferring */
	int		nbytes;	/* number of bytes currently transferring */
	long		bcount;	/* total number of bytes */
	char		*databuf;/* data buffer address */
	volatile int	error;
#define	NOERROR 	0	/* There was no error (r_error invalid) */
#define	ERROR		1	/* check r_error */
#define	ERR_DF		2	/* Drive fault */
#define	ERR_DMA		3	/* DMA error */
#define	TIMEOUT		4	/* device timed out */
#define	ERR_NODEV	5	/* device has been gone */
#define ERR_RESET	6	/* command was terminated by channel reset */
	uint8_t		r_error;/* copy of error register */
	daddr_t		badsect[127];/* 126 plus trailing -1 marker */
};

/*
 * ATA/ATAPI commands description
 *
 * This structure defines the interface between the ATA/ATAPI device driver
 * and the controller for short commands. It contains the command's parameter,
 * the length of data to read/write (if any), and a function to call upon
 * completion.
 * If no sleep is allowed, the driver can poll for command completion.
 * Once the command completed, if the error registered is valid, the flag
 * AT_ERROR is set and the error register value is copied to r_error .
 * A separate interface is needed for read/write or ATAPI packet commands
 * (which need multiple interrupts per commands).
 */
struct ata_command {
	/* ATA parameters */
	uint64_t r_lba;		/* before & after */
	uint16_t r_count;	/* before & after */
	union {
		uint16_t r_features; /* before */
		uint8_t r_error; /* after */
	};
	union {
		uint8_t r_command; /* before */
		uint8_t r_status; /* after */
	};
	uint8_t r_device;	/* before & after */

	uint8_t r_st_bmask;	/* status register mask to wait for before
				   command */
	uint8_t r_st_pmask;	/* status register mask to wait for after
				   command */
	volatile uint16_t flags;

#define AT_READ     0x0001 /* There is data to read */
#define AT_WRITE    0x0002 /* There is data to write (excl. with AT_READ) */
#define AT_WAIT     0x0008 /* wait in controller code for command completion */
#define AT_POLL     0x0010 /* poll for command completion (no interrupts) */
#define AT_DONE     0x0020 /* command is done */
#define AT_XFDONE   0x0040 /* data xfer is done */
#define AT_ERROR    0x0080 /* command is done with error */
#define AT_TIMEOU   0x0100 /* command timed out */
#define AT_DF       0x0200 /* Drive fault */
#define AT_RESET    0x0400 /* command terminated by channel reset */
#define AT_GONE     0x0800 /* command terminated because device is gone */
#define AT_READREG  0x1000 /* Read registers on completion */
#define AT_LBA      0x2000 /* LBA28 */
#define AT_LBA48    0x4000 /* LBA48 */

	int timeout;		/* timeout (in ms) */
	void *data;		/* Data buffer address */
	int bcount;		/* number of bytes to transfer */
	void (*callback)(void *); /* command to call once command completed */
	void *callback_arg;	/* argument passed to *callback() */
};

/*
 * ata_bustype.  The first field must be compatible with scsipi_bustype,
 * as it's used for autoconfig by both ata and atapi drivers.
 */
struct ata_bustype {
	int	bustype_type;	/* symbolic name of type */
	int	(*ata_bio)(struct ata_drive_datas *, struct ata_bio *);
	void	(*ata_reset_drive)(struct ata_drive_datas *, int, uint32_t *);
	void	(*ata_reset_channel)(struct ata_channel *, int);
/* extra flags for ata_reset_*(), in addition to AT_* */
#define AT_RST_EMERG 0x10000 /* emergency - e.g. for a dump */
#define	AT_RST_NOCMD 0x20000 /* XXX has to go - temporary until we have tagged queuing */

	int	(*ata_exec_command)(struct ata_drive_datas *,
				    struct ata_command *);

#define	ATACMD_COMPLETE		0x01
#define	ATACMD_QUEUED		0x02
#define	ATACMD_TRY_AGAIN	0x03

	int	(*ata_get_params)(struct ata_drive_datas *, uint8_t,
				  struct ataparams *);
	int	(*ata_addref)(struct ata_drive_datas *);
	void	(*ata_delref)(struct ata_drive_datas *);
	void	(*ata_killpending)(struct ata_drive_datas *);
};

/* bustype_type */	/* XXX XXX XXX */
/* #define SCSIPI_BUSTYPE_SCSI	0 */
/* #define SCSIPI_BUSTYPE_ATAPI	1 */
#define	SCSIPI_BUSTYPE_ATA	2

/*
 * Describe an ATA device.  Has to be compatible with scsipi_channel, so
 * start with a pointer to ata_bustype.
 */
struct ata_device {
	const struct ata_bustype *adev_bustype;
	int adev_channel;
	int adev_openings;
	struct ata_drive_datas *adev_drv_data;
};

/*
 * Per-channel data
 */
struct ata_channel {
	struct callout ch_callout;	/* callout handle */
	int ch_channel;			/* location */
	struct atac_softc *ch_atac;	/* ATA controller softc */

	/* Our state */
	volatile int ch_flags;
#define ATACH_SHUTDOWN 0x02	/* channel is shutting down */
#define ATACH_IRQ_WAIT 0x10	/* controller is waiting for irq */
#define ATACH_DMA_WAIT 0x20	/* controller is waiting for DMA */
#define ATACH_PIOBM_WAIT 0x40	/* controller is waiting for busmastering PIO */
#define	ATACH_DISABLED 0x80	/* channel is disabled */
#define ATACH_TH_RUN   0x100	/* the kernel thread is working */
#define ATACH_TH_RESET 0x200	/* someone ask the thread to reset */
#define ATACH_TH_RESCAN 0x400	/* rescan requested */
	uint8_t ch_status;	/* copy of status register */
	uint8_t ch_error;	/* copy of error register */

	/* for the reset callback */
	int ch_reset_flags;

	/* per-drive info */
	int ch_ndrives; /* number of entries in ch_drive[] */
	struct ata_drive_datas *ch_drive; /* array of ata_drive_datas */

	device_t atabus;	/* self */

	/* ATAPI children */
	device_t atapibus;
	struct scsipi_channel ch_atapi_channel;

	/*
	 * Channel queues.  May be the same for all channels, if hw
	 * channels are not independent.
	 */
	struct ata_queue *ch_queue;

	/* The channel kernel thread */
	struct lwp *ch_thread;

	/* Number of sata PMP ports, if any */
	int ch_satapmp_nports;
};

/*
 * ATA controller softc.
 *
 * This contains a bunch of generic info that all ATA controllers need
 * to have.
 *
 * XXX There is still some lingering wdc-centricity here.
 */
struct atac_softc {
	device_t atac_dev;		/* generic device info */

	int	atac_cap;		/* controller capabilities */

#define	ATAC_CAP_DATA16	0x0001		/* can do 16-bit data access */
#define	ATAC_CAP_DATA32	0x0002		/* can do 32-bit data access */
#define	ATAC_CAP_DMA	0x0008		/* can do ATA DMA modes */
#define	ATAC_CAP_UDMA	0x0010		/* can do ATA Ultra DMA modes */
#define	ATAC_CAP_PIOBM	0x0020		/* can do busmastering PIO transfer */
#define	ATAC_CAP_ATA_NOSTREAM 0x0040	/* don't use stream funcs on ATA */
#define	ATAC_CAP_ATAPI_NOSTREAM 0x0080	/* don't use stream funcs on ATAPI */
#define	ATAC_CAP_NOIRQ	0x1000		/* controller never interrupts */
#define	ATAC_CAP_RAID	0x4000		/* controller "supports" RAID */

	uint8_t	atac_pio_cap;		/* highest PIO mode supported */
#if NATA_DMA
	uint8_t	atac_dma_cap;		/* highest DMA mode supported */
#if NATA_UDMA
	uint8_t	atac_udma_cap;		/* highest UDMA mode supported */
#endif
#endif

	/* Array of pointers to channel-specific data. */
	struct ata_channel **atac_channels;
	int		     atac_nchannels;

	const struct ata_bustype *atac_bustype_ata;

	/*
	 * Glue between ATA and SCSIPI for the benefit of ATAPI.
	 *
	 * Note: The reference count here is used for both ATA and ATAPI
	 * devices.
	 */
	struct atapi_adapter atac_atapi_adapter;
	void (*atac_atapibus_attach)(struct atabus_softc *);

	/* Driver callback to probe for drives. */
	void (*atac_probe)(struct ata_channel *);

	/* Optional callbacks to lock/unlock hardware. */
	int  (*atac_claim_hw)(struct ata_channel *, int);
	void (*atac_free_hw)(struct ata_channel *);

	/*
	 * Optional callbacks to set drive mode.  Required for anything
	 * but basic PIO operation.
	 */
	void (*atac_set_modes)(struct ata_channel *);
};

#ifdef _KERNEL
void	ata_channel_attach(struct ata_channel *);
int	atabusprint(void *aux, const char *);
int	ataprint(void *aux, const char *);

int	atabus_alloc_drives(struct ata_channel *, int);
void	atabus_free_drives(struct ata_channel *);

struct ataparams;
int	ata_get_params(struct ata_drive_datas *, uint8_t, struct ataparams *);
int	ata_set_mode(struct ata_drive_datas *, uint8_t, uint8_t);
/* return code for these cmds */
#define CMD_OK    0
#define CMD_ERR   1
#define CMD_AGAIN 2

struct ata_xfer *ata_get_xfer(int);
void	ata_free_xfer(struct ata_channel *, struct ata_xfer *);
#define	ATAXF_CANSLEEP	0x00
#define	ATAXF_NOSLEEP	0x01

void	ata_exec_xfer(struct ata_channel *, struct ata_xfer *);
void	ata_kill_pending(struct ata_drive_datas *);
void	ata_reset_channel(struct ata_channel *, int);

int	ata_addref(struct ata_channel *);
void	ata_delref(struct ata_channel *);
void	atastart(struct ata_channel *);
void	ata_print_modes(struct ata_channel *);
#if NATA_DMA
int	ata_downgrade_mode(struct ata_drive_datas *, int);
#endif
void	ata_probe_caps(struct ata_drive_datas *);

#if NATA_DMA
void	ata_dmaerr(struct ata_drive_datas *, int);
#endif
void	ata_queue_idle(struct ata_queue *);
void	ata_delay(int, const char *, int);
#endif /* _KERNEL */

#endif /* _DEV_ATA_ATAVAR_H_ */

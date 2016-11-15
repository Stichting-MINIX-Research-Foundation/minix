/* $NetBSD: isp_netbsd.h,v 1.75 2015/08/28 13:03:36 joerg Exp $ */
/*
 * NetBSD Specific definitions for the Qlogic ISP Host Adapter
 */
/*
 * Copyright (C) 1997, 1998, 1999 National Aeronautics & Space Administration
 * All rights reserved.
 *
 * Additional Copyright (C) 2000-2007 by Matthew Jacob
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
#ifndef	_ISP_NETBSD_H
#define	_ISP_NETBSD_H

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/kthread.h>

#include <sys/bus.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/scsipi/scsi_message.h>
#include <dev/scsipi/scsipi_debug.h>

#include "opt_isp.h"

/*
 * Efficiency- get rid of SBus code && tests unless we need them.
 */
#if	defined(__sparcv9__ ) || defined(__sparc__)
#define	ISP_SBUS_SUPPORTED	1
#else
#define	ISP_SBUS_SUPPORTED	0
#endif

#define	ISP_PLATFORM_VERSION_MAJOR	5
#define	ISP_PLATFORM_VERSION_MINOR	0

struct isposinfo {
	device_t		dev;
	struct scsipi_adapter   adapter;
	struct scsipi_channel * chan;
	bus_dma_tag_t		dmatag;
	bus_dmamap_t		rqdmap;
	bus_dmamap_t		rsdmap;
	bus_dmamap_t		scdmap;		/* FC only */
	uint64_t 		wwns[256];	/* FC only */
	int			splsaved;
	int			mboxwaiting;
	uint32_t		islocked;
	uint32_t		onintstack;
	uint32_t		loop_down_time;
	uint32_t		loop_down_limit;
	uint32_t		gone_device_time;
	unsigned int		: 16,
				: 8,
		gdt_running	: 1,
		loop_checked	: 1,
		mbox_sleeping	: 1,
		mbox_sleep_ok	: 1,
		mboxcmd_done	: 1,
		mboxbsy		: 1,
		paused		: 1,
		blocked		: 1;
	struct callout		ldt;	/* loop down timer */
	struct callout		gdt;	/* gone device timer */
	uint64_t		wwn;
	uint16_t		framesize;
	uint16_t		exec_throttle;
	struct lwp *		thread;
};
#define	isp_dmatag		isp_osinfo.dmatag
#define	isp_rqdmap		isp_osinfo.rqdmap
#define	isp_rsdmap		isp_osinfo.rsdmap
#define	isp_scdmap		isp_osinfo.scdmap

#define	ISP_MUSTPOLL(isp)	\
 	(isp->isp_osinfo.onintstack || isp->isp_osinfo.mbox_sleep_ok == 0)

/*
 * Required Macros/Defines
 */

#define	ISP_FC_SCRLEN		0x1000

#define	ISP_MEMZERO(dst, a)	memset((dst), 0, (a))
#define	ISP_MEMCPY(dst, src, a)	memcpy((dst), (src), (a))
#define	ISP_SNPRINTF		snprintf
#define	ISP_DELAY		DELAY
#define	ISP_SLEEP(isp, x)		\
	if (!ISP_MUSTPOLL(isp))		\
		ISP_UNLOCK(isp);	\
	DELAY(x);			\
	if (!ISP_MUSTPOLL(isp))		\
		ISP_LOCK(isp)

#define	ISP_MIN	imin
#define	ISP_INLINE

#define	NANOTIME_T		struct timeval
#define	GET_NANOTIME		microtime
#define	GET_NANOSEC(x)		(((x)->tv_sec * 1000000 + (x)->tv_usec) * 1000)
#define	NANOTIME_SUB		isp_microtime_sub

#define	MAXISPREQUEST(isp)	256


#define	MEMORYBARRIER(isp, type, offset, size, c)		\
switch (type) {							\
case SYNC_REQUEST:						\
{								\
	off_t off = (off_t) offset * QENTRY_LEN;		\
	bus_dmamap_sync(isp->isp_dmatag, isp->isp_rqdmap,	\
	    off, size, BUS_DMASYNC_PREWRITE);			\
	break;							\
}								\
case SYNC_RESULT:						\
{								\
	off_t off = (off_t) offset * QENTRY_LEN;		\
	bus_dmamap_sync(isp->isp_dmatag, isp->isp_rsdmap,	\
	    off, size, BUS_DMASYNC_POSTREAD);			\
	break;							\
}								\
case SYNC_SFORDEV:						\
{								\
	off_t off = (off_t) offset;				\
	bus_dmamap_sync(isp->isp_dmatag, isp->isp_scdmap,	\
	    off, size, BUS_DMASYNC_PREWRITE);			\
	break;							\
}								\
case SYNC_SFORCPU:						\
{								\
	off_t off = (off_t) offset;				\
	bus_dmamap_sync(isp->isp_dmatag, isp->isp_scdmap,	\
	    off, size, BUS_DMASYNC_POSTREAD);			\
	break;							\
}								\
default:							\
	break;							\
}

#define	MBOX_ACQUIRE		isp_mbox_acquire
#define	MBOX_WAIT_COMPLETE	isp_mbox_wait_complete
#define	MBOX_NOTIFY_COMPLETE	isp_mbox_notify_done
#define	MBOX_RELEASE		isp_mbox_release

#define	FC_SCRATCH_ACQUIRE(isp, chan)	0
#define	FC_SCRATCH_RELEASE(isp, chan)	do { } while (0)

#ifndef	SCSI_GOOD
#define	SCSI_GOOD	0x0
#endif
#ifndef	SCSI_CHECK
#define	SCSI_CHECK	0x2
#endif
#ifndef	SCSI_BUSY
#define	SCSI_BUSY	0x8
#endif
#ifndef	SCSI_QFULL
#define	SCSI_QFULL	0x28
#endif

#define	XS_T			struct scsipi_xfer
#define	XS_DMA_ADDR_T		bus_addr_t
#define	XS_DMA_ADDR_T		bus_addr_t
#define XS_GET_DMA64_SEG(a, b, c)		\
{						\
	ispds64_t *d = a;			\
	bus_dma_segment_t *e = b;		\
	uint32_t f = c;				\
	e += f;					\
        d->ds_base = DMA_LO32(e->ds_addr);	\
        d->ds_basehi = DMA_HI32(e->ds_addr);	\
        d->ds_count = e->ds_len;		\
}
#define XS_GET_DMA_SEG(a, b, c)			\
{						\
	ispds_t *d = a;				\
	bus_dma_segment_t *e = b;		\
	uint32_t f = c;				\
	e += f;					\
        d->ds_base = DMA_LO32(e->ds_addr);	\
        d->ds_count = e->ds_len;		\
}
#define	XS_CHANNEL(xs)		\
	((int) (xs)->xs_periph->periph_channel->chan_channel)
#define	XS_ISP(xs)		\
	device_private((xs)->xs_periph->periph_channel->chan_adapter->adapt_dev)
#define	XS_LUN(xs)		((int) (xs)->xs_periph->periph_lun)
#define	XS_TGT(xs)		((int) (xs)->xs_periph->periph_target)
#define	XS_CDBP(xs)		((uint8_t *) (xs)->cmd)
#define	XS_CDBLEN(xs)		(xs)->cmdlen
#define	XS_XFRLEN(xs)		(xs)->datalen
#define	XS_TIME(xs)		(xs)->timeout
#define	XS_GET_RESID(xs)	(xs)->resid
#define	XS_SET_RESID(xs, r)	(xs)->resid = r
#define	XS_STSP(xs)		(&(xs)->status)
#define	XS_SNSP(xs)		(&(xs)->sense.scsi_sense)
#define	XS_SNSLEN(xs)		(sizeof (xs)->sense)
#define	XS_SNSKEY(xs)		SSD_SENSE_KEY((xs)->sense.scsi_sense.flags)
#define	XS_SNSASC(xs)		((xs)->sense.scsi_sense.asc)
#define	XS_SNSASCQ(xs)		((xs)->sense.scsi_sense.ascq)
/* PORTING NOTES: check to see if there's a better way of checking for tagged */
#define	XS_TAG_P(ccb)		(((xs)->xs_control & XS_CTL_POLL) != 0)
/* PORTING NOTES: We elimited OTAG option for performance */
#define	XS_TAG_TYPE(xs)	\
	(((xs)->xs_control & XS_CTL_URGENT) ? REQFLAG_HTAG : REQFLAG_STAG)

#define	XS_SETERR(xs, v)	(xs)->error = v

#	define	HBA_NOERROR		XS_NOERROR
#	define	HBA_BOTCH		XS_DRIVER_STUFFUP
#	define	HBA_CMDTIMEOUT		XS_TIMEOUT
#	define	HBA_SELTIMEOUT		XS_SELTIMEOUT
#	define	HBA_TGTBSY		XS_BUSY
#	define	HBA_BUSRESET		XS_RESET
#	define	HBA_ABORTED		XS_DRIVER_STUFFUP
#	define	HBA_DATAOVR		XS_DRIVER_STUFFUP
#	define	HBA_ARQFAIL		XS_DRIVER_STUFFUP

#define	XS_ERR(xs)		(xs)->error
#define	XS_NOERR(xs)		((xs)->error == XS_NOERROR)
#define	XS_INITERR(xs)		(xs)->error = 0, XS_CMD_S_CLEAR(xs)

#define	XS_SAVE_SENSE(xs, ptr, len)				\
	if (xs->error == XS_NOERROR) {				\
		xs->error = XS_SENSE;				\
	}							\
	memcpy(&(xs)->sense, ptr, imin(XS_SNSLEN(xs), len))

#define	XS_SENSE_VALID(xs)		(xs)->error == XS_SENSE

#define	DEFAULT_FRAMESIZE(isp)		(isp)->isp_osinfo.framesize
#define	DEFAULT_EXEC_THROTTLE(isp)	(isp)->isp_osinfo.exec_throttle
#define	GET_DEFAULT_ROLE(isp, chan)	ISP_ROLE_INITIATOR
#define	SET_DEFAULT_ROLE(isp, chan)	do { } while (0)
#define	DEFAULT_IID(x, chan)		7
#define	DEFAULT_LOOPID(x, chan)		108
#define	DEFAULT_NODEWWN(isp, chan)	(isp)->isp_osinfo.wwn
#define	DEFAULT_PORTWWN(isp, chan)	(isp)->isp_osinfo.wwn
#define	ACTIVE_NODEWWN(isp, chan)				\
	((isp)->isp_osinfo.wwn? (isp)->isp_osinfo.wwn :	\
	(FCPARAM(isp, chan)->isp_wwnn_nvram?		\
	 FCPARAM(isp, chan)->isp_wwnn_nvram : 0x400000007F000008ull))
#define	ACTIVE_PORTWWN(isp, chan)				\
	((isp)->isp_osinfo.wwn? (isp)->isp_osinfo.wwn :	\
	(FCPARAM(isp, chan)->isp_wwpn_nvram?		\
	 FCPARAM(isp, chan)->isp_wwpn_nvram : 0x400000007F000008ull))

#if	_BYTE_ORDER == _BIG_ENDIAN
#ifdef	ISP_SBUS_SUPPORTED
#define	ISP_IOXPUT_8(isp, s, d)		*(d) = s
#define	ISP_IOXPUT_16(isp, s, d)				\
	*(d) = (isp->isp_bustype == ISP_BT_SBUS)? s : bswap16(s)
#define	ISP_IOXPUT_32(isp, s, d)				\
	*(d) = (isp->isp_bustype == ISP_BT_SBUS)? s : bswap32(s)

#define	ISP_IOXGET_8(isp, s, d)		d = (*((uint8_t *)s))
#define	ISP_IOXGET_16(isp, s, d)				\
	d = (isp->isp_bustype == ISP_BT_SBUS)?			\
	*((uint16_t *)s) : bswap16(*((uint16_t *)s))
#define	ISP_IOXGET_32(isp, s, d)				\
	d = (isp->isp_bustype == ISP_BT_SBUS)?			\
	*((uint32_t *)s) : bswap32(*((uint32_t *)s))

#else	/* ISP_SBUS_SUPPORTED */
#define	ISP_IOXPUT_8(isp, s, d)		*(d) = s
#define	ISP_IOXPUT_16(isp, s, d)	*(d) = bswap16(s)
#define	ISP_IOXPUT_32(isp, s, d)	*(d) = bswap32(s)
#define	ISP_IOXGET_8(isp, s, d)		d = (*((uint8_t *)s))
#define	ISP_IOXGET_16(isp, s, d)	d = bswap16(*((uint16_t *)s))
#define	ISP_IOXGET_32(isp, s, d)	d = bswap32(*((uint32_t *)s))
#endif	/* ISP_SBUS_SUPPORTED */
#define	ISP_SWIZZLE_NVRAM_WORD(isp, rp)	*rp = bswap16(*rp)
#define	ISP_SWIZZLE_NVRAM_LONG(isp, rp)	*rp = bswap32(*rp)

#define	ISP_IOZGET_8(isp, s, d)		d = (*((uint8_t *)s))
#define	ISP_IOZGET_16(isp, s, d)	d = (*((uint16_t *)s))
#define	ISP_IOZGET_32(isp, s, d)	d = (*((uint32_t *)s))
#define	ISP_IOZPUT_8(isp, s, d)		*(d) = s
#define	ISP_IOZPUT_16(isp, s, d)	*(d) = s
#define	ISP_IOZPUT_32(isp, s, d)	*(d) = s

#else
#define	ISP_IOXPUT_8(isp, s, d)		*(d) = s
#define	ISP_IOXPUT_16(isp, s, d)	*(d) = s
#define	ISP_IOXPUT_32(isp, s, d)	*(d) = s
#define	ISP_IOXGET_8(isp, s, d)		d = *(s)
#define	ISP_IOXGET_16(isp, s, d)	d = *(s)
#define	ISP_IOXGET_32(isp, s, d)	d = *(s)
#define	ISP_SWIZZLE_NVRAM_WORD(isp, rp)
#define	ISP_SWIZZLE_NVRAM_LONG(isp, rp)

#define	ISP_IOZPUT_8(isp, s, d)		*(d) = s
#define	ISP_IOZPUT_16(isp, s, d)	*(d) = bswap16(s)
#define	ISP_IOZPUT_32(isp, s, d)	*(d) = bswap32(s)

#define	ISP_IOZGET_8(isp, s, d)		d = (*((uint8_t *)(s)))
#define	ISP_IOZGET_16(isp, s, d)	d = bswap16(*((uint16_t *)(s)))
#define	ISP_IOZGET_32(isp, s, d)	d = bswap32(*((uint32_t *)(s)))

#endif

#define	ISP_SWAP16(isp, x)		bswap16(x)
#define	ISP_SWAP32(isp, x)		bswap32(x)

/*
 * Includes of common header files
 */

#include <dev/ic/ispreg.h>
#include <dev/ic/ispvar.h>
#include <dev/ic/ispmbox.h>

/*
 * isp_osinfo definitions, extensions and shorthand.
 */
#define	isp_unit	device_unit(isp_osinfo.dev)


/*
 * Driver prototypes..
 */
void isp_attach(ispsoftc_t *);
void isp_uninit(ispsoftc_t *);

/*
 * Driver wide data...
 */

/*
 * Locking macros...
 */
#define	ISP_LOCK		isp_lock
#define	ISP_UNLOCK		isp_unlock
#define	ISP_ILOCK(x)		isp_lock(x); isp->isp_osinfo.onintstack++
#define	ISP_IUNLOCK(x)		isp->isp_osinfo.onintstack--; isp_unlock(x)

/*
 * Platform private flags
 */

#define	XS_PSTS_INWDOG		0x10000000
#define	XS_PSTS_GRACE		0x20000000
#define	XS_PSTS_ALL		0x30000000

#define	XS_CMD_S_WDOG(xs)	(xs)->xs_status |= XS_PSTS_INWDOG
#define	XS_CMD_C_WDOG(xs)	(xs)->xs_status &= ~XS_PSTS_INWDOG
#define	XS_CMD_WDOG_P(xs)	(((xs)->xs_status & XS_PSTS_INWDOG) != 0)

#define	XS_CMD_S_GRACE(xs)	(xs)->xs_status |= XS_PSTS_GRACE
#define	XS_CMD_C_GRACE(xs)	(xs)->xs_status &= ~XS_PSTS_GRACE
#define	XS_CMD_GRACE_P(xs)	(((xs)->xs_status & XS_PSTS_GRACE) != 0)

#define	XS_CMD_S_DONE(xs)	(xs)->xs_status |= XS_STS_DONE
#define	XS_CMD_C_DONE(xs)	(xs)->xs_status &= ~XS_STS_DONE
#define	XS_CMD_DONE_P(xs)	(((xs)->xs_status & XS_STS_DONE) != 0)

#define	XS_CMD_S_CLEAR(xs)	(xs)->xs_status &= ~XS_PSTS_ALL

/*
 * Platform Library Functionw
 */
void isp_prt(ispsoftc_t *, int level, const char *, ...);
void isp_xs_prt(ispsoftc_t *, XS_T *, int level, const char *, ...);
void isp_lock(ispsoftc_t *);
void isp_unlock(ispsoftc_t *);
uint64_t isp_microtime_sub(struct timeval *, struct timeval *);
int isp_mbox_acquire(ispsoftc_t *);
void isp_mbox_wait_complete(ispsoftc_t *, mbreg_t *);
void isp_mbox_notify_done(ispsoftc_t *);
void isp_mbox_release(ispsoftc_t *);

/*
 * Common Library functions
 */
#include <dev/ic/isp_library.h>

#if	!defined(ISP_DISABLE_FW) && !defined(ISP_COMPILE_FW)
#define	ISP_COMPILE_FW	1
#endif
#endif	/* _ISP_NETBSD_H */

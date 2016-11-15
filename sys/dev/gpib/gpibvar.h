/*	$NetBSD: gpibvar.h,v 1.5 2012/10/27 17:18:16 chs Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gregory McGarry.
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

#include <sys/queue.h>

#define GPIB_NDEVS		30	/* max address */
#define GPIB_ADDRMASK		0x1f	/* address mask */
#define GPIB_BROADCAST_ADDR	31	/* GPIB broadcast address */

/*
 * GPIB commands
 */

/* Universal command group (UCG) [0x10] */
#define GPIBCMD_LLO		0x11	/* local lockout */
#define	GPIBCMD_DCL		0x14	/* universal device clear */
#define GPIBCMD_PPU		0x15	/* parallel poll unconfigure */
#define GPIBCMD_SPE		0x18
#define GPIBCMD_SPD		0x19

/* Addressed command group (ACG) [0x00] */
#define GPIBCMD_GTL		0x01
#define	GPIBCMD_SDC		0x04	/* selected device clear */
#define GPIBCMD_PPC		0x05 	/* parallel poll clear */
#define GPIBCMD_GET		0x08
#define GPIBCMD_TCT		0x09

#define	GPIBCMD_LAG		0x20	/* listener address group commands */
#define	GPIBCMD_UNL		0x3f	/* universal unlisten */
#define	GPIBCMD_TAG		0x40	/* talker address group commands */
#define	GPIBCMD_UNA		0x5e	/* unaddress (master talk address?) */
#define	GPIBCMD_UNT		0x5f	/* universal untalk */
#define	GPIBCMD_SCG		0x60	/* secondary group commands */
#define GPIBCMD_PPD		0x70
#define GPIBCMD_DEL		0x7f

struct gpib_softc;

struct gpib_chipset_tag {
	void	(*reset)(void *);
	int	(*send)(void *, int, int, void *, int);
	int	(*recv)(void *, int, int, void *, int);
	int	(*pptest)(void *, int);
	void	(*ppwatch)(void *, int);
	void	(*ppclear)(void *);
	void	(*xfer)(void *, int, int, void *, int, int, int);
	int	(*tc)(void *, int);
	int	(*gts)(void *);
	void	(*ifc)(void *);
	int	(*sendcmds)(void *, void *, int);
	int	(*senddata)(void *, void *, int);
	int	(*recvdata)(void *, void *, int);
	void	*cookie;
	struct gpib_softc *bus;
};
typedef struct gpib_chipset_tag *gpib_chipset_tag_t;

/*
 * Wrapper functions that go directly to the hardware driver.
 */
#define gpibreset(ic)							\
	(*((ic)->reset))((ic)->cookie)
#define gpibpptest(ic, slave)						\
	(*((ic)->pptest))((ic)->cookie, (slave))
#define gpibppclear(ic)							\
	(*((ic)->ppclear))((ic)->cookie)
#define gpibxfer(ic, slave, sec, buf, cnt, rw, timo)			\
	(*((ic)->xfer))((ic)->cookie, (slave), (sec), (buf), (cnt),	\
	    (rw), (timo))

/*
 * An GPIB job queue entry.  Slave drivers have one of these used
 * to queue requests with the controller.
 */
typedef void (*gpib_callback_t)(void *, int);
struct gpibqueue {
	TAILQ_ENTRY(gpibqueue) hq_list;	/* entry on queue */
	void	*hq_softc;		/* slave's softc */
	int	hq_slave;		/* slave on bus */
	gpib_callback_t hq_callback;	/* slave's callback function */
};
typedef struct gpibqueue *gpib_handle_t;

int	_gpibregister(struct gpib_softc *, int, void (*cb)(void *, int),
	    void *, gpib_handle_t *);
int	_gpibrequest(struct gpib_softc *, gpib_handle_t);
void	_gpibrelease(struct gpib_softc *, gpib_handle_t);
int	_gpibswait(struct gpib_softc *, int);
void	_gpibawait(struct gpib_softc *);
int	_gpibsend(struct gpib_softc *, int, int, void *, int);
int	_gpibrecv(struct gpib_softc *, int, int, void *, int);

#define gpibsend(ic, slave, sec, addr, cnt)				\
	_gpibsend((ic)->bus, (slave), (sec), (addr), (cnt))
#define gpibrecv(ic, slave, sec, addr, cnt)				\
	_gpibrecv((ic)->bus, (slave), (sec), (addr), (cnt))
#define gpibregister(ic, slave, callback, arg, hdlp)		\
	_gpibregister((ic)->bus, (slave), (callback), (arg), (hdlp))
#define gpibrequest(ic, hdl)					\
	_gpibrequest((ic)->bus, hdl)
#define gpibrelease(ic, hdl)					\
	_gpibrelease((ic)->bus, hdl)
#define gpibawait(ic)						\
	_gpibawait((ic)->bus)
#define gpibswait(ic, slave)					\
	_gpibswait((ic)->bus, (slave))

int	gpib_alloc(struct gpib_softc *, u_int8_t);
int	gpib_isalloc(struct gpib_softc *, u_int8_t);
void	gpib_dealloc(struct gpib_softc *, u_int8_t);

/* called from controller drivers only */
int	gpibintr(void *);
int	gpibdevprint(void *, const char *);

/* callback flags */
#define GPIBCBF_START		1
#define GPIBCBF_INTR		2

/* gpibxfer dir(ection) parameter */
#define GPIB_READ		1
#define GPIB_WRITE		2

/*
 * Attach devices
 */
struct gpib_attach_args {
	gpib_chipset_tag_t ga_ic;		/* GPIB chipset tag */
	int ga_address;				/* device GPIB address */
};

/*
 * Attach a GPIB to controller.
 */
struct gpibdev_attach_args {
	gpib_chipset_tag_t ga_ic;		/* GPIB chipset tag */
	int ga_address;				/* host GPIB address */
};

/*
 * Software state per GPIB bus.
 */
struct gpib_softc {
	device_t sc_dev;			/* generic device glue */
	gpib_chipset_tag_t sc_ic;		/* GPIB chipset tag */
	u_int8_t sc_myaddr;			/* my (host) GPIB address */
	int sc_flags;
#define GPIBF_ACTIVE	0x01
	u_int32_t sc_rmap;			/* resource map */
	TAILQ_HEAD(, gpibqueue) sc_queue;	/* GPIB job queue */
};

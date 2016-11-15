/* $NetBSD: dkvar.h,v 1.24 2015/08/28 17:41:49 mlelstv Exp $ */

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Roland C. Dowdeswell.
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

#include <sys/rndsource.h>

struct pathbuf; /* from namei.h */

/* literally this is not a softc, but is intended to be included in
 * the pseudo-disk's softc and passed to calls in dksubr.c.  It
 * should include the common elements of the pseudo-disk's softc.
 * All elements that are included here should describe the external
 * representation of the disk to the higher layers, and flags that
 * are common to each of the pseudo-disk drivers.
 */
struct dk_softc {
	device_t		 sc_dev;
	u_int32_t		 sc_flags;	/* flags */
#define DK_XNAME_SIZE 8
	char			 sc_xname[DK_XNAME_SIZE]; /* external name */
	struct disk		 sc_dkdev;	/* generic disk info */
	kmutex_t		 sc_iolock;	/* protects buffer queue */
	struct bufq_state	*sc_bufq;	/* buffer queue */
	int			 sc_dtype;	/* disk type */
	struct buf		*sc_deferred;	/* retry after start failed */
	bool			 sc_busy;	/* processing buffers */
	krndsource_t		 sc_rnd_source;	/* entropy source */
};

/* sc_flags:
 *   We separate the flags into two varieties, those that dksubr.c
 *   understands and manipulates and those that it does not.
 */

#define DKF_INITED	0x00010000 /* unit has been initialised */
#define DKF_WLABEL	0x00020000 /* label area is writable */
#define DKF_LABELLING	0x00040000 /* unit is currently being labeled */
#define DKF_WARNLABEL	0x00080000 /* warn if disklabel not present */
#define DKF_LABELSANITY	0x00100000 /* warn if disklabel not sane */
#define DKF_TAKEDUMP	0x00200000 /* allow dumping */
#define DKF_KLABEL      0x00400000 /* keep label on close */
#define DKF_VLABEL      0x00800000 /* label is valid */
#define DKF_SLEEP       0x80000000 /* dk_start/dk_done may sleep */

/* Mask of flags that dksubr.c understands, other flags are fair game */
#define DK_FLAGMASK	0xffff0000

#define DK_ATTACHED(_dksc) ((_dksc)->sc_flags & DKF_INITED)

#define DK_BUSY(_dksc, _pmask)				\
	(((_dksc)->sc_dkdev.dk_openmask & ~(_pmask)) ||	\
	((_dksc)->sc_dkdev.dk_bopenmask & (_pmask)  &&	\
	((_dksc)->sc_dkdev.dk_copenmask & (_pmask))))

/*
 * Functions that are exported to the pseudo disk implementations:
 */

void	dk_init(struct dk_softc *, device_t, int);
void	dk_attach(struct dk_softc *);
void	dk_detach(struct dk_softc *);

int	dk_open(struct dk_softc *, dev_t,
		int, int, struct lwp *);
int	dk_close(struct dk_softc *, dev_t,
		 int, int, struct lwp *);
void	dk_strategy(struct dk_softc *, struct buf *);
int	dk_discard(struct dk_softc *, dev_t, off_t, off_t);
void	dk_start(struct dk_softc *, struct buf *);
void	dk_done(struct dk_softc *, struct buf *);
void	dk_drain(struct dk_softc *);
int	dk_size(struct dk_softc *, dev_t);
int	dk_ioctl(struct dk_softc *, dev_t,
		 u_long, void *, int, struct lwp *);
int	dk_dump(struct dk_softc *, dev_t,
		daddr_t, void *, size_t);
void	dk_getdisklabel(struct dk_softc *, dev_t);
void	dk_getdefaultlabel(struct dk_softc *, struct disklabel *);

int	dk_lookup(struct pathbuf *, struct lwp *, struct vnode **);

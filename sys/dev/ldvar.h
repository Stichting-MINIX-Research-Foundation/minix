/*	$NetBSD: ldvar.h,v 1.27 2015/08/28 17:41:49 mlelstv Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
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

#ifndef	_DEV_LDVAR_H_
#define	_DEV_LDVAR_H_

#include <sys/mutex.h>
#include <sys/rndsource.h>

#include <dev/dkvar.h> /* for dk_softc */

struct ld_softc {
	struct dk_softc	sc_dksc;
	kmutex_t	sc_mutex;
	kcondvar_t	sc_drain;

	int		sc_queuecnt;	/* current h/w queue depth */
	int		sc_ncylinders;	/* # cylinders */
	int		sc_nheads;	/* # heads */
	int		sc_nsectors;	/* # sectors per track */
	uint64_t	sc_disksize512;

	/*
	 * The following are filled by hardware specific attachment code.
	 */
	device_t	sc_dv;
	int		sc_flags;	/* control flags */
	uint64_t	sc_secperunit;	/* # sectors in total */
	int		sc_secsize;	/* sector size in bytes */
	int		sc_maxxfer;	/* max xfer size in bytes */
	int		sc_maxqueuecnt;	/* maximum h/w queue depth */

	int		(*sc_dump)(struct ld_softc *, void *, int, int);
	int		(*sc_flush)(struct ld_softc *, int);
	int		(*sc_start)(struct ld_softc *, struct buf *);
	int		(*sc_discard)(struct ld_softc *, off_t, off_t);
};

/* sc_flags */
#define	LDF_ENABLED	0x001		/* device enabled */
#define	LDF_DRAIN	0x020		/* maxqueuecnt has changed; drain */

/* sc_flush() flags */
#define	LDFL_POLL	0x001		/* poll for completion */

int	ldadjqparam(struct ld_softc *, int);
void	ldattach(struct ld_softc *);
int	ldbegindetach(struct ld_softc *, int);
void	ldenddetach(struct ld_softc *);
void	lddone(struct ld_softc *, struct buf *);

#endif	/* !_DEV_LDVAR_H_ */

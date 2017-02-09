/* $NetBSD: lptvar.h,v 1.10 2008/04/28 20:23:56 martin Exp $ */

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gary Thorpe.
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

#ifndef __DEV_PPBUS_LPTVAR_H
#define __DEV_PPBUS_LPTVAR_H

#include <machine/vmparam.h>
#include <dev/ppbus/ppbus_device.h>

/* #define LPINITRDY       4       wait up to 4 seconds for a ready
#define BUFSTATSIZE     32
#define LPTOUTINITIAL   10       initial timeout to wait for ready 1/10 s
#define LPTOUTMAX       1        maximal timeout 1 s */
#define LPPRI           (PZERO+8)
#define BUFSIZE		PAGE_SIZE

#define	LPTUNIT(s)	(minor(s) & 0xff)
#define	LPTCTL(s)	(minor(s) & 0x100)

/* Wait up to 16 seconds for a ready */
#define	LPT_TIMEOUT		((hz)*16)
#define LPT_STEP		((hz)/4)

struct lpt_softc {
	struct ppbus_device_softc ppbus_dev;

	int sc_state;
/* bits for state */
#define OPEN            (unsigned)(1<<0)  /* device is open */
#define ASLP            (unsigned)(1<<1)  /* awaiting draining of printer */
#define EERROR          (unsigned)(1<<2)  /* error was received from printer */
#define OBUSY           (unsigned)(1<<3)  /* printer is busy doing output */
#define LPTOUT          (unsigned)(1<<4)  /* timeout while not selected */
#define TOUT            (unsigned)(1<<5)  /* timeout while not selected */
#define LPTINIT         (unsigned)(1<<6)  /* waiting to initialize for open */
#define INTERRUPTED     (unsigned)(1<<7)  /* write call was interrupted */
#define HAVEBUS         (unsigned)(1<<8)  /* the driver owns the bus */

	int sc_flags;		/* flags from lptio.h */

	void *sc_inbuf;
	void *sc_outbuf;
	bus_addr_t sc_in_baddr;
	bus_addr_t sc_out_baddr;
};

#define MAX_SLEEP       (hz*5)  /* Timeout while waiting for device ready */
#define MAX_SPIN        20      /* Max delay for device ready in usecs */

#ifdef LPT_DEBUG
static volatile int lptdebug = 1;
#ifndef LPT_DPRINTF
#define LPT_DPRINTF(arg) if(lptdebug) printf arg
#else
#define LPT_DPRINTF(arg) if(lptdebug) printf("WARNING: LPT_DPRINTF " \
	"REDEFINED!!!")
#endif /* LPT_DPRINTF */
#else
#define LPT_DPRINTF(arg)
#endif /* LPT_DEBUG */

#ifdef LPT_VERBOSE
static volatile int lptverbose = 1;
#ifndef LPT_VPRINTF
#define LPT_VPRINTF(arg) if(lptverbose) printf arg
#else
#define LPT_VPRINTF(arg) if(lptverbose) printf("WARNING: LPT_VPRINTF " \
	"REDEFINED!!!")
#endif /* LPT_VPRINTF */
#else
#define LPT_VPRINTF(arg)
#endif /* LPT_VERBOSE */

#endif /* __DEV_PPBUS_LPTVAR_H */

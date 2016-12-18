/*	$NetBSD: gtvar.h,v 1.15 2010/04/28 13:51:56 kiyohara Exp $	*/
/*
 * Copyright (c) 2009 KIYOHARA Takashi
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_DISCOVERY_GTVAR_H_
#define	_DEV_DISCOVERY_GTVAR_H_

struct gt_softc {
	device_t sc_dev;
	int sc_model;
	int sc_rev;
	bus_addr_t sc_addr;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_dma_tag_t sc_dmat;
};

struct obio_attach_args {
	const char *oa_name;		/* call name of device */
	bus_space_tag_t oa_memt;	/* bus space tag */
	bus_addr_t oa_offset;		/* offset (absolute) to device */
	bus_size_t oa_size;		/* size (strided) of device */
	int oa_irq;			/* irq */
};


void	gt_attach_common(struct gt_softc *);

#ifdef GT_WATCHDOG
void	gt_watchdog_service(void);
void	gt_watchdog_reset(void);
#else
#define gt_watchdog_service()	((void)0)
#define gt_watchdog_reset()	((void)0)
#endif

int	gt_mii_read(device_t, device_t, int, int);
void	gt_mii_write(device_t, device_t, int, int, int);


uint32_t gt_read_mpp(void);

#endif /* _DEV_DISCOVERY_GTVAR_H_ */

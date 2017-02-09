/*	$NetBSD: gtsdmavar.h,v 1.1 2010/04/28 13:51:56 kiyohara Exp $	*/
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
#ifndef _GTSDMAVAR_H_
#define _GTSDMAVAR_H_

static __inline uint32_t
gt_sdma_icause(device_t gt, uint32_t _imask)
{
	struct gt_softc *sc = device_private(gt);
	uint32_t icause;

	icause = bus_space_read_4(sc->sc_iot, sc->sc_ioh, SDMA_ICAUSE) & _imask;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, SDMA_ICAUSE, icause);
	return icause;
}

static __inline void
gt_sdma_imask(device_t gt, uint32_t _imask)
{
	struct gt_softc *sc = device_private(gt);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, SDMA_IMASK, _imask);
}

#endif	/* _GTSDMAVAR_H_ */

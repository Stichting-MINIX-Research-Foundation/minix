/*	$NetBSD: gtdevbusvar.h,v 1.1 2010/04/28 13:51:56 kiyohara Exp $	*/
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
#ifndef _GTDEVBUSVAR_H_
#define _GTDEVBUSVAR_H_

static __inline int
gt_devbus_addr(device_t gt, int unit, uint32_t *ldp, uint32_t *hdp)
{
	static const struct {
		bus_addr_t low_decode;
		bus_addr_t high_decode;
	} obio_info[5] = {
		{ GT_CS0_Low_Decode, GT_CS0_High_Decode, },
		{ GT_CS1_Low_Decode, GT_CS1_High_Decode, },
		{ GT_CS2_Low_Decode, GT_CS2_High_Decode, },
		{ GT_CS3_Low_Decode, GT_CS3_High_Decode, },
		{ GT_BootCS_Low_Decode, GT_BootCS_High_Decode, },
	};
	struct gt_softc *sc = device_private(gt);

	if (unit >= __arraycount(obio_info))
		return -1;

	*ldp = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
	    obio_info[unit].low_decode);
	*hdp = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
	    obio_info[unit].high_decode);
	return 0;
}

#endif	/* _GTDEVBUSVAR_H_ */

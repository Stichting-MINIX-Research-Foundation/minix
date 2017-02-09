/*	$NetBSD: if_le_tc.c,v 1.21 2008/04/04 12:25:07 tsutsui Exp $	*/

/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * LANCE on TurboChannel.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_le_tc.c,v 1.21 2008/04/04 12:25:07 tsutsui Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif

#include <dev/ic/lancereg.h>
#include <dev/ic/lancevar.h>
#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

#include <dev/tc/if_levar.h>
#include <dev/tc/tcvar.h>

static int	le_tc_match(device_t, cfdata_t, void *);
static void	le_tc_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(le_tc, sizeof(struct le_softc),
    le_tc_match, le_tc_attach, NULL, NULL);

#define	LE_OFFSET_RAM		0x0
#define	LE_OFFSET_LANCE		0x100000
#define	LE_OFFSET_ROM		0x1c0000

static int
le_tc_match(device_t parent, cfdata_t cf, void *aux)
{
	struct tc_attach_args *d = aux;

	if (strncmp("PMAD-AA ", d->ta_modname, TC_ROM_LLEN) != 0)
		return (0);

	return (1);
}

static void
le_tc_attach(device_t parent, device_t self, void *aux)
{
	struct le_softc *lesc = device_private(self);
	struct lance_softc *sc = &lesc->sc_am7990.lsc;
	struct tc_attach_args *d = aux;

	sc->sc_dev = self;

	/*
	 * It's on the turbochannel proper, or a kn02
	 * baseboard implementation of a TC option card.
	 */
	lesc->sc_r1 = (struct lereg1 *)
	    TC_DENSE_TO_SPARSE(TC_PHYS_TO_UNCACHED(d->ta_addr + LE_OFFSET_LANCE));
	sc->sc_mem = (void *)(d->ta_addr + LE_OFFSET_RAM);

	sc->sc_copytodesc = lance_copytobuf_contig;
	sc->sc_copyfromdesc = lance_copyfrombuf_contig;
	sc->sc_copytobuf = lance_copytobuf_contig;
	sc->sc_copyfrombuf = lance_copyfrombuf_contig;
	sc->sc_zerobuf = lance_zerobuf_contig;

	/*
	 * TC lance boards have onboard SRAM buffers.  DMA
	 * between the onbard RAM and main memory is not possible,
	 * so  DMA setup is not required.
	 */

	dec_le_common_attach(&lesc->sc_am7990,
			     (uint8_t *)(d->ta_addr + LE_OFFSET_ROM + 2));

	tc_intr_establish(parent, d->ta_cookie, TC_IPL_NET, am7990_intr, sc);
}

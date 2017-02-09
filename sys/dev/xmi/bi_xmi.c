/*	$NetBSD: bi_xmi.c,v 1.9 2009/05/12 14:48:08 cegger Exp $	*/

/*
 * Copyright (c) 2000 Ludd, University of Lule}, Sweden. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed at Ludd, University of
 *      Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
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


/*
 * DWMBA XMI-BI adapter.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: bi_xmi.c,v 1.9 2009/05/12 14:48:08 cegger Exp $");

#include <sys/param.h>
#include <sys/device.h>

#include <sys/bus.h>

#include <dev/xmi/xmireg.h>
#include <dev/xmi/xmivar.h>

#include <dev/bi/bireg.h>
#include <dev/bi/bivar.h>

#include "locators.h"

static int
bi_xmi_match(device_t parent, cfdata_t cf, void *aux)
{
	struct xmi_attach_args *xa = aux;

	if (bus_space_read_2(xa->xa_iot, xa->xa_ioh, XMI_TYPE) != XMIDT_DWMBA)
		return 0;

	if (cf->cf_loc[XMICF_NODE] != XMICF_NODE_DEFAULT &&
	    cf->cf_loc[XMICF_NODE] != xa->xa_nodenr)
		return 0;

	return 1;
}

static void
bi_xmi_attach(device_t parent, device_t self, void *aux)
{
	struct bi_softc *sc = device_private(self);
	struct xmi_attach_args *xa = aux;

	/*
	 * Fill in bus specific data.
	 */
	sc->sc_dev = self;
	sc->sc_addr = (bus_addr_t)BI_BASE(xa->xa_nodenr, 0);
	sc->sc_iot = xa->xa_iot; /* No special I/O handling */
	sc->sc_dmat = xa->xa_dmat; /* No special DMA handling either */
	sc->sc_intcpu = xa->xa_intcpu;

	bi_attach(sc);
}

CFATTACH_DECL_NEW(bi_xmi, sizeof(struct bi_softc),
    bi_xmi_match, bi_xmi_attach, NULL, NULL);

/*	$NetBSD: lpt_puc.c,v 1.17 2014/03/29 19:28:25 christos Exp $	*/

/*
 * Copyright (c) 1998 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
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
 * Machine-independent parallel port ('lpt') driver attachment to "PCI
 * Universal Communications" controller driver.
 *
 * Author: Christopher G. Demetriou, May 17, 1998.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lpt_puc.c,v 1.17 2014/03/29 19:28:25 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <sys/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pucvar.h>
#include <dev/ic/lptvar.h>

static int
lpt_puc_probe(device_t parent, cfdata_t match, void *aux)
{
	struct puc_attach_args *aa = aux;

	/*
	 * Locators already matched, just check the type.
	 */
	if (aa->type != PUC_PORT_TYPE_LPT)
		return (0);

	return (1);
}

static void
lpt_puc_attach(device_t parent, device_t self, void *aux)
{
	struct lpt_softc *sc = device_private(self);
	struct puc_attach_args *aa = aux;
	const char *intrstr;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;
	sc->sc_iot = aa->t;
	sc->sc_ioh = aa->h;

	aprint_naive(": Parallel port");
	aprint_normal(": ");

	intrstr = pci_intr_string(aa->pc, aa->intrhandle, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(aa->pc, aa->intrhandle, IPL_TTY,
	    lptintr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error("couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}

#if defined(amd64) || defined(i386)
	/*
	 * Parallel ports are sometimes used for improvised GPIO by
	 * userspace programs which need to know the port's I/O address.
	 * Print the address here so the user doesn't have to dig through
	 * PCI configuration space to find it.
	*/
	if (aa->h < 0x10000)
		aprint_normal("ioaddr 0x%04lx, ", aa->h);
#endif
	aprint_normal("interrupting at %s\n", intrstr);

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	lpt_attach_subr(sc);
}

CFATTACH_DECL_NEW(lpt_puc, sizeof(struct lpt_softc),
    lpt_puc_probe, lpt_puc_attach, NULL, NULL);

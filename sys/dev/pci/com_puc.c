/*	$NetBSD: com_puc.c,v 1.23 2014/05/23 14:16:39 msaitoh Exp $	*/

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
 * Machine-independent ns16x50 serial port ('com') driver attachment to
 * "PCI Universal Communications" controller driver.
 *
 * Author: Christopher G. Demetriou, May 17, 1998.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: com_puc.c,v 1.23 2014/05/23 14:16:39 msaitoh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/tty.h>

#include <sys/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pucvar.h>
#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>
#include <dev/pci/cybervar.h>

struct com_puc_softc {
	struct com_softc sc_com;	/* real "com" softc */

	/* puc-specific goo. */
	void	*sc_ih;			/* interrupt handler */
};

/* Interface field in PCI Class register */
static const char *serialtype[] = {
	"Generic XT",
	"16450",
	"16550",
	"16650",
	"16750",
	"16850",
	"16950",
};

static int
com_puc_probe(device_t parent, cfdata_t match, void *aux)
{
	struct puc_attach_args *aa = aux;

	/*
	 * Locators already matched, just check the type.
	 */
	if (aa->type != PUC_PORT_TYPE_COM)
		return (0);

	return (1);
}

static void
com_puc_attach(device_t parent, device_t self, void *aux)
{
	struct com_puc_softc *psc = device_private(self);
	struct com_softc *sc = &psc->sc_com;
	struct puc_attach_args *aa = aux;
	const char *intrstr;
	char intrbuf[PCI_INTRSTR_LEN];
	unsigned int iface;

	sc->sc_dev = self;

	iface = PCI_INTERFACE(pci_conf_read(aa->pc, aa->tag, PCI_CLASS_REG));
	aprint_naive(": Serial port");
	if (iface < __arraycount(serialtype))
		aprint_normal(" (%s-compatible)", serialtype[iface]);
	aprint_normal(": ");

	COM_INIT_REGS(sc->sc_regs, aa->t, aa->h, aa->a);
	sc->sc_frequency = aa->flags & PUC_COM_CLOCKMASK;

	intrstr = pci_intr_string(aa->pc, aa->intrhandle, intrbuf, sizeof(intrbuf));
	psc->sc_ih = pci_intr_establish(aa->pc, aa->intrhandle, IPL_SERIAL,
	    comintr, sc);
	if (psc->sc_ih == NULL) {
		aprint_error("couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}

#if defined(amd64) || defined(i386)
	/*
	 * Since puc(4) serial ports are typically not identified in the
	 * BIOS COM[1234] table, the I/O address must be manually set using
	 * installboot(8) in order to enable a serial console.
	 * Print the address here so the user doesn't have to dig through
	 * PCI configuration space to find it.
	 */
	if (aa->h < 0x10000)
		aprint_normal("ioaddr 0x%04lx, ", aa->h);
#endif
	aprint_normal("interrupting at %s\n", intrstr);

	/* Enable Cyberserial 8X clock. */
	if (aa->flags & (PUC_COM_SIIG10x|PUC_COM_SIIG20x)) {
		int usrregno;

		if	(aa->flags & PUC_PORT_USR3) usrregno = 3;
		else if (aa->flags & PUC_PORT_USR2) usrregno = 2;
		else if (aa->flags & PUC_PORT_USR1) usrregno = 1;
		else /* (aa->flags & PUC_PORT_USR0) */ usrregno = 0;

		if (aa->flags & PUC_COM_SIIG10x)
			write_siig10x_usrreg(aa->pc, aa->tag, usrregno, 1);
		else
			write_siig20x_usrreg(aa->pc, aa->tag, usrregno, 1);
	} else {
		if (!pmf_device_register(self, NULL, com_resume))
			aprint_error_dev(self,
			    "couldn't establish power handler\n");
	}

	aprint_normal("%s", device_xname(self));
	com_attach_subr(sc);
}

CFATTACH_DECL_NEW(com_puc, sizeof(struct com_puc_softc),
    com_puc_probe, com_puc_attach, NULL, NULL);

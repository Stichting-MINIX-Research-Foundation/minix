/*	$NetBSD: xmi.c,v 1.11 2010/11/13 13:52:13 uebayasi Exp $	*/

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
 * XMI specific routines.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: xmi.c,v 1.11 2010/11/13 13:52:13 uebayasi Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <sys/cpu.h>

#include <dev/xmi/xmireg.h>
#include <dev/xmi/xmivar.h>

static int xmi_print(void *, const char *);

const struct xmi_list xmi_list[] = {
	{XMIDT_KA62, 0, "ka6200"},
	{XMIDT_KA64, 1, "ka6400"},
	{XMIDT_KA65, 0, "ka6500"},
	{XMIDT_KA66, 0, "ka6600"},
	{XMIDT_MS62, 1, "ms62"},
	{XMIDT_DWMBA, 1, "dwmba"},
	{0,0,0}
};

int
xmi_print(void *aux, const char *name)
{
	struct xmi_attach_args * const xa = aux;
	const struct xmi_list *xl;

	for (xl = &xmi_list[0]; xl->xl_nr; xl++)
		if (xl->xl_nr == bus_space_read_2(xa->xa_iot, xa->xa_ioh, 0))
			break;

	if (name) {
		if (xl->xl_nr == 0)
			aprint_normal("unknown device 0x%x",
			    bus_space_read_2(xa->xa_iot, xa->xa_ioh, 0));
		else
			aprint_normal(xl->xl_name);
		aprint_normal(" at %s", name);
	}
	aprint_normal(" node %d", xa->xa_nodenr);
	return xl->xl_havedriver ? UNCONF : UNSUPP;
}

static	int lastiv = 0;

void
xmi_attach(struct xmi_softc *sc)
{
	struct xmi_attach_args xa;
	int nodenr;

	aprint_normal("\n");

	xa.xa_iot = sc->sc_iot;
	xa.xa_busnr = sc->sc_busnr;
	xa.xa_dmat = sc->sc_dmat;
	xa.xa_intcpu = sc->sc_intcpu;
	/*
	 * Interrupt numbers. All vectors from 256-512 are free, use
	 * them for XMI devices and just count them up.
	 * Above 512 are only interrupt vectors for unibus devices.
	 *
	 * The XMI nodespace is giant in size. Only map in the first
	 * page here and map more (if needed) in the device itself.
	 */
	for (nodenr = 0; nodenr < NNODEXMI; nodenr++) {
		if (bus_space_map(sc->sc_iot, sc->sc_addr + XMI_NODE(nodenr),
		    PAGE_SIZE, 0, &xa.xa_ioh)) {
			aprint_error_dev(sc->sc_dev,
			    "bus_space_map failed, node %d\n", nodenr);
			return;
		}
		if (badaddr((void *)xa.xa_ioh, 4)) {
			bus_space_unmap(sc->sc_iot, xa.xa_ioh, PAGE_SIZE);
			continue;
		}
		xa.xa_nodenr = nodenr;
		xa.xa_ivec = 256 + lastiv;
		lastiv += 4;
		config_found(sc->sc_dev, &xa, xmi_print);
	}
}

/*      $NetBSD: rumpdev_pci.c,v 1.5 2015/05/17 13:51:31 pooka Exp $	*/

/*
 * Copyright (c) 2013 Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rumpdev_pci.c,v 1.5 2015/05/17 13:51:31 pooka Exp $");

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/atomic.h>

#include <dev/pci/pcivar.h>

#include "pci_user.h"

void
pci_attach_hook(device_t parent, device_t self, struct pcibus_attach_args *pba)
{

	/* nada */
}

int
pci_bus_maxdevs(pci_chipset_tag_t pc, int busno)
{

	return 32;
}

pcitag_t
pci_make_tag(pci_chipset_tag_t pc, int bus, int device, int function)
{
	pcitag_t pt;
	int *tag;
	unsigned csr;
	int rv;

	CTASSERT(sizeof(pt) >= sizeof(int));

	/* a "bit" ugly, but keeps us MI */
	tag = (int *)&pt;
	*tag = (bus << 16) | (device << 8) | (function << 0);

	/*
	 * On Xen, we need to enable the device io/mem space.
	 * Doesn't really belong here, but we need to do it somewhere.
	 */
	rv = rumpcomp_pci_confread(bus, device, function,
	    PCI_COMMAND_STATUS_REG, &csr);
	if (rv == 0 && (csr & PCI_COMMAND_MEM_ENABLE) == 0) {
		rumpcomp_pci_confwrite(bus, device, function,
		    PCI_COMMAND_STATUS_REG, csr | PCI_COMMAND_MEM_ENABLE);
	}

	return pt;
}

pcireg_t
pci_conf_read(pci_chipset_tag_t pc, pcitag_t tag, int reg)
{
	unsigned int rv;
	int bus, device, fun;

	pci_decompose_tag(pc, tag, &bus, &device, &fun);
	rumpcomp_pci_confread(bus, device, fun, reg, &rv);
	return rv;
}

void
pci_conf_write(pci_chipset_tag_t pc, pcitag_t tag, int reg, pcireg_t data)
{
	int bus, device, fun;

	pci_decompose_tag(pc, tag, &bus, &device, &fun);
	rumpcomp_pci_confwrite(bus, device, fun, reg, data);
}

void
pci_decompose_tag(pci_chipset_tag_t pc, pcitag_t tag,
	int *bp, int *dp, int *fp)
{
	int *t = (int *)&tag;

	*bp = (*t >> 16) & 0xff;
	*dp = (*t >> 8)  & 0xff;
	*fp = (*t >> 0)  & 0xff;
}

/*
 * Well, yay, deal with the wonders of weird_t.  We'll just
 * assume it's an integral type (which, btw, isn't universally true).
 * The hypercall will map "cookie" to its internal structure.
 * Dial _t for a good time.
 */
int
pci_intr_map(const struct pci_attach_args *pa, pci_intr_handle_t *ih)
{
	static unsigned int intrhandle;
	unsigned cookie;
	int rv;

	cookie = atomic_inc_uint_nv(&intrhandle);
	rv = rumpcomp_pci_irq_map(pa->pa_bus,
	    pa->pa_device, pa->pa_function, pa->pa_intrline, cookie);
	if (rv == 0)
		*ih = cookie;
	return 0;
}

const char *
pci_intr_string(pci_chipset_tag_t pc, pci_intr_handle_t ih,
	char *buf, size_t buflen)
{

	snprintf(buf, buflen, "pausebreak");
	return buf;
}

void *
pci_intr_establish(pci_chipset_tag_t pc, pci_intr_handle_t ih,
	int level, int (*func)(void *), void *arg)
{

	return rumpcomp_pci_irq_establish(ih, func, arg);
}

int
pci_intr_setattr(pci_chipset_tag_t pc, pci_intr_handle_t *ih,
	int attr, uint64_t data)
{

	switch (attr) {
	case PCI_INTR_MPSAFE:
		return 0;
	default:
		return ENODEV;
	}
}

void
pci_intr_disestablish(pci_chipset_tag_t pc, void *not_your_above_ih)
{

	panic("%s: unimplemented", __func__);
}

#ifdef __HAVE_PCIIDE_MACHDEP_COMPAT_INTR_ESTABLISH
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>

void *
pciide_machdep_compat_intr_establish(device_t dev,
	const struct pci_attach_args *pa, int chan,
	int (*func)(void *), void *arg)
{
	pci_intr_handle_t ih;
	struct pci_attach_args mypa = *pa;

	mypa.pa_intrline = PCIIDE_COMPAT_IRQ(chan);
	if (pci_intr_map(&mypa, &ih) != 0)
		return NULL;
	return rumpcomp_pci_irq_establish(ih, func, arg);
}

__strong_alias(pciide_machdep_compat_intr_disestablish,pci_intr_disestablish);
#endif /* __HAVE_PCIIDE_MACHDEP_COMPAT_INTR_ESTABLISH */

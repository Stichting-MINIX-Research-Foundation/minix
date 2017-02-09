/* $NetBSD: universe_pci.c,v 1.12 2014/03/29 19:28:25 christos Exp $ */

/*
 * Copyright (c) 1999
 * 	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Common functions for PCI-VME-interfaces using the
 * Newbridge/Tundra Universe II chip (CA91C142).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: universe_pci.c,v 1.12 2014/03/29 19:28:25 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
/*#include <dev/pci/pcidevs.h>*/

#include <sys/bus.h>

#include <dev/vme/vmereg.h>
#include <dev/vme/vmevar.h>

#include <dev/ic/universereg.h>
#include <dev/pci/universe_pci_var.h>

int univ_pci_intr(void *);

#define read_csr_4(d, reg) \
  bus_space_read_4(d->csrt, d->csrh, offsetof(struct universereg, reg))
#define write_csr_4(d, reg, val) \
  bus_space_write_4(d->csrt, d->csrh, offsetof(struct universereg, reg), val)

#define _pso(i) offsetof(struct universereg, __CONCAT(pcislv, i))
static int pcislvoffsets[8] = {
	_pso(0), _pso(1), _pso(2), _pso(3),
	_pso(4), _pso(5), _pso(6), _pso(7)
};
#undef _pso

#define read_pcislv(d, idx, reg) \
  bus_space_read_4(d->csrt, d->csrh, \
   pcislvoffsets[idx] + offsetof(struct universe_pcislvimg, reg))
#define write_pcislv(d, idx, reg, val) \
  bus_space_write_4(d->csrt, d->csrh, \
   pcislvoffsets[idx] + offsetof(struct universe_pcislvimg, reg), val)


#define _vso(i) offsetof(struct universereg, __CONCAT(vmeslv, i))
static int vmeslvoffsets[8] = {
	_vso(0), _vso(1), _vso(2), _vso(3),
	_vso(4), _vso(5), _vso(6), _vso(7)
};
#undef _vso

#define read_vmeslv(d, idx, reg) \
  bus_space_read_4(d->csrt, d->csrh, \
   vmeslvoffsets[idx] + offsetof(struct universe_vmeslvimg, reg))
#define write_vmeslv(d, idx, reg, val) \
  bus_space_write_4(d->csrt, d->csrh, \
   vmeslvoffsets[idx] + offsetof(struct universe_vmeslvimg, reg), val)

int
univ_pci_attach(struct univ_pci_data *d, struct pci_attach_args *pa, const char *name, void (*inthdl)(void *, int, int), void *intcookie)
{
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	u_int32_t reg;
	int i;
	char intrbuf[PCI_INTRSTR_LEN];

	d->pc = pc;
	strncpy(d->devname, name, sizeof(d->devname));
	d->devname[sizeof(d->devname) - 1] = '\0';

	if (pci_mapreg_map(pa, 0x10,
			   PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT,
			   0, &d->csrt, &d->csrh, NULL, NULL) &&
	    pci_mapreg_map(pa, 0x14,
			   PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT,
			   0, &d->csrt, &d->csrh, NULL, NULL) &&
	    pci_mapreg_map(pa, 0x10,
			   PCI_MAPREG_TYPE_IO,
			   0, &d->csrt, &d->csrh, NULL, NULL) &&
	    pci_mapreg_map(pa, 0x14,
			   PCI_MAPREG_TYPE_IO,
			   0, &d->csrt, &d->csrh, NULL, NULL))
		return (-1);

	/* name sure the chip is in a sane state */
	write_csr_4(d, lint_en, 0); /* mask all PCI interrupts */
	write_csr_4(d, vint_en, 0); /* mask all VME interrupts */
	write_csr_4(d, dgcs, 0x40000000); /* stop DMA activity */
	for (i = 0; i < 8; i++) {
		univ_pci_unmapvme(d, i);
		univ_pci_unmappci(d, i);
	}
	write_csr_4(d, slsi, 0); /* disable "special PCI slave image" */

	/* enable DMA */
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG) |
	    PCI_COMMAND_MASTER_ENABLE);

	reg = read_csr_4(d, misc_ctl);
	aprint_normal("%s: ", name);
	if (reg & 0x00020000) /* SYSCON */
		aprint_normal("VME bus controller, ");
	reg = read_csr_4(d, mast_ctl);
	aprint_normal("requesting at VME bus level %d\n", (reg >> 22) & 3);

	/* Map and establish the PCI interrupt. */
	if (pci_intr_map(pa, &ih)) {
		aprint_error("%s: couldn't map interrupt\n", name);
		return (-1);
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	/*
	 * Use a low interrupt level (the lowest?).
	 * We will raise before calling a subdevice's handler.
	 */
	d->ih = pci_intr_establish(pc, ih, IPL_BIO, univ_pci_intr, d);
	if (d->ih == NULL) {
		aprint_error("%s: couldn't establish interrupt", name);
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return (-1);
	}
	aprint_normal("%s: interrupting at %s\n", name, intrstr);

	/* handle all VME interrupts (XXX should be configurable) */
	d->vmeinthandler = inthdl;
	d->vmeintcookie = intcookie;
	write_csr_4(d, lint_stat, 0x00ff37ff); /* ack all pending IRQs */
	write_csr_4(d, lint_en, 0x000000fe); /* enable VME IRQ 1..7 */

	return (0);
}

int
univ_pci_mapvme(struct univ_pci_data *d, int wnd, vme_addr_t vmebase, u_int32_t len, vme_am_t am, vme_datasize_t datawidth, u_int32_t pcibase)
{
	u_int32_t ctl = 0x80000000;

	switch (am & VME_AM_ADRSIZEMASK) {
	case VME_AM_A32:
		ctl |= 0x00020000;
		break;
	case VME_AM_A24:
		ctl |= 0x00010000;
		break;
	case VME_AM_A16:
		break;
	default:
		return (EINVAL);
	}
	if (am & VME_AM_SUPER)
		ctl |= 0x00001000;
	if ((am & VME_AM_MODEMASK) == VME_AM_PRG)
		ctl |= 0x00004000;
	if (datawidth & VME_D32)
		ctl |= 0x00800000;
	else if (datawidth & VME_D16)
		ctl |= 0x00400000;
	else if (!(datawidth & VME_D8))
		return (EINVAL);

#ifdef UNIV_DEBUG
	printf("%s: wnd %d, map VME %x-%x to %x, ctl=%x\n",
	       d->devname, wnd, vmebase, vmebase + len, pcibase, ctl);
#endif

	write_pcislv(d, wnd, lsi_bs, pcibase);
	write_pcislv(d, wnd, lsi_bd, pcibase + len);
	write_pcislv(d, wnd, lsi_to, vmebase - pcibase);
	write_pcislv(d, wnd, lsi_ctl, ctl);
	return (0);
}

void
univ_pci_unmapvme(struct univ_pci_data *d, int wnd)
{
#ifdef UNIV_DEBUG
	printf("%s: unmap VME wnd %d\n", d->devname, wnd);
#endif
	write_pcislv(d, wnd, lsi_ctl, 0);
}


int
univ_pci_mappci(struct univ_pci_data *d, int wnd, u_int32_t pcibase, u_int32_t len, vme_addr_t vmebase, vme_am_t am)
{
	u_int32_t ctl = 0x80000000;

	switch (am & VME_AM_ADRSIZEMASK) {
	case VME_AM_A32:
		ctl |= 0x00020000;
		break;
	case VME_AM_A24:
		ctl |= 0x00010000;
		break;
	case VME_AM_A16:
		break;
	default:
		return (EINVAL);
	}
	if (am & VME_AM_SUPER)
		ctl |= 0x00200000;
	else
		ctl |= 0x00300000; /* both */
	if ((am & VME_AM_MODEMASK) == VME_AM_PRG)
		ctl |= 0x00800000;
	else
		ctl |= 0x00c00000; /* both */

#ifdef UNIV_DEBUG
	printf("%s: wnd %d, map PCI %x-%x to %x, ctl=%x\n",
	       d->devname, wnd, pcibase, pcibase + len, vmebase, ctl);
#endif

	write_vmeslv(d, wnd, vsi_bs, vmebase);
	write_vmeslv(d, wnd, vsi_bd, vmebase + len);
	write_vmeslv(d, wnd, vsi_to, pcibase - vmebase);
	write_vmeslv(d, wnd, vsi_ctl, ctl);
	return (0);
}

void
univ_pci_unmappci(struct univ_pci_data *d, int wnd)
{
#ifdef UNIV_DEBUG
	printf("%s: unmap PCI wnd %d\n", d->devname, wnd);
#endif
	write_vmeslv(d, wnd, vsi_ctl, 0);
}

int
univ_pci_vmebuserr(struct univ_pci_data *d, int clear)
{
	u_int32_t pcicsr;

	pcicsr = read_csr_4(d, pci_csr);
	if ((pcicsr & 0xf8000000) && clear)
		write_csr_4(d, pci_csr, pcicsr | 0xf8000000);
	return (pcicsr & 0x08000000); /* target abort */
}

int
univ_pci_intr(void *v)
{
	struct univ_pci_data *d = v;
	u_int32_t intcsr;
	int i, vec;

	intcsr = read_csr_4(d, lint_stat) & 0xffffff;
	if (!intcsr)
		return (0);

	/* ack everything */
	write_csr_4(d, lint_stat, intcsr);
#ifdef UNIV_DEBUG
	printf("%s: intr, lint_stat=%x\n", d->devname, intcsr);
#endif
	if (intcsr & 0x000000fe) { /* VME interrupt */
		for (i = 7; i >= 1; i--) {
			if (!(intcsr & (1 << i)))
				continue;
			vec = read_csr_4(d, v_statid[i - 1]);
			if (vec & 0x100) {
				printf("%s: err irq %d\n", d->devname, i);
				continue;
			}
			if (d->vmeinthandler)
				(*d->vmeinthandler)(d->vmeintcookie, i, vec);
		}
	}

	return (1);
}

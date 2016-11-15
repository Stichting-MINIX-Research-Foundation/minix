/*	$NetBSD: puccn.c,v 1.14 2014/03/05 05:56:04 msaitoh Exp $ */

/*
 * Derived from  pci.c
 * Copyright (c) 2000 Geocast Networks Systems.  All rights reserved.
 *
 * Copyright (c) 1995, 1996, 1997, 1998
 *     Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1994 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * Machine independent support for PCI serial console support.
 *
 * Scan the PCI bus for something which resembles a 16550
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: puccn.c,v 1.14 2014/03/05 05:56:04 msaitoh Exp $");

#include "opt_kgdb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <sys/termios.h>
#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

#include <dev/cons.h>

#include <dev/pci/pucvar.h>
#include <dev/pci/puccn.h>

#ifndef CONSPEED
#define CONSPEED	TTYDEF_SPEED
#endif
#ifndef CONMODE
#define	CONMODE		((TTYDEF_CFLAG & ~(CSIZE|CSTOPB|PARENB))|CS8) /* 8N1 */
#endif

cons_decl(com);

static bus_addr_t puccnbase;
static bus_space_tag_t puctag;
static int puccnflags;

#ifdef KGDB
static bus_addr_t pucgdbbase;
#endif

/*
 * Static dev/func variables allow pucprobe to be called multiple times,
 * resuming the search where it left off, never retrying the same adaptor.
 */

static bus_addr_t
pucprobe_doit(struct consdev *cn)
{
	struct pci_attach_args pa;
	int bus;
	static int dev = 0, func = 0;
	int maxdev, nfunctions = 0, i; /* XXX */
	pcireg_t reg, bhlcr, subsys = 0; /* XXX */
	int foundport = 0;
	const struct puc_device_description *desc;
	pcireg_t base;

	/* Fetch our tags */
#if defined(amd64) || defined(i386)
	if (cpu_puc_cnprobe(cn, &pa) != 0)
#endif
		return 0;

	pci_decompose_tag(pa.pa_pc, pa.pa_tag, &bus, &maxdev, NULL);

	/* Scan through devices and find a communication class device. */
	for (; dev <= maxdev ; dev++) {
		pa.pa_tag = pci_make_tag(pa.pa_pc, bus, dev, 0);
		reg = pci_conf_read(pa.pa_pc, pa.pa_tag, PCI_ID_REG);
		if (PCI_VENDOR(reg) == PCI_VENDOR_INVALID
		    || PCI_VENDOR(reg) == 0)
			continue;
		bhlcr = pci_conf_read(pa.pa_pc, pa.pa_tag, PCI_BHLC_REG);
		if (PCI_HDRTYPE_MULTIFN(bhlcr)) {
			nfunctions = 8;
		} else {
			nfunctions = 1;
		}
resume_scan:
		for (; func < nfunctions; func++)  {
			pa.pa_tag = pci_make_tag(pa.pa_pc, bus, dev, func);
			reg = pci_conf_read(pa.pa_pc, pa.pa_tag,
			    PCI_CLASS_REG);
			if (PCI_CLASS(reg)  == PCI_CLASS_COMMUNICATIONS
			    && PCI_SUBCLASS(reg)
			       == PCI_SUBCLASS_COMMUNICATIONS_SERIAL) {
				pa.pa_id = pci_conf_read(pa.pa_pc, pa.pa_tag,
				    PCI_ID_REG);
				subsys = pci_conf_read(pa.pa_pc, pa.pa_tag,
				    PCI_SUBSYS_ID_REG);
				foundport = 1;
				break;
			}
		}
		if (foundport)
			break;

		func = 0;
	}

	/*
	 * If all devices was scanned and couldn't find any communication
	 * device, return with 0.
	 */
	if (!foundport)
		return 0;

	/* Clear foundport flag */
	foundport = 0;

	/* Check whether the device is in the puc device table or not */
	desc = puc_find_description(PCI_VENDOR(pa.pa_id),
	    PCI_PRODUCT(pa.pa_id), PCI_VENDOR(subsys), PCI_PRODUCT(subsys));

	/* If not, check the next communication device */
	if (desc == NULL) {
		/* Resume from the next function */
		func++;
		goto resume_scan;
	}

	/*
	 * We found a device and it's on the puc table. Set the tag and
	 * the base address.
	 */
	for (i = 0; PUC_PORT_VALID(desc, i); i++) {
		if (desc->ports[i].type != PUC_PORT_TYPE_COM)
			continue;
		puccnflags = desc->ports[i].flags;
		base = pci_conf_read(pa.pa_pc, pa.pa_tag, desc->ports[i].bar);
		base += desc->ports[i].offset;

		if (PCI_MAPREG_TYPE(base) == PCI_MAPREG_TYPE_IO) {
			puctag = pa.pa_iot;
			base = PCI_MAPREG_IO_ADDR(base);
		}
#if 0 /* For MMIO device */
		else {
			puctag = pa.pa_memt;
			base = PCI_MAPREG_MEM_ADDR(base);
		}
#endif

		if (com_is_console(puctag, base, NULL))
			continue;
		foundport = 1;
		break;
	}

	if (foundport == 0) {
		func++;
		goto resume_scan;
	}

#if 0
	cn->cn_pri = CN_REMOTE;
#else
	if (cn)
		cn->cn_pri = CN_REMOTE;
#endif
	return base;
}

#ifdef KGDB
void
puc_gdbprobe(struct consdev *cn)
{

	pucgdbbase = pucprobe_doit(cn);
}

void
puc_gdbinit(struct consdev *cn)
{

	if (pucgdbbase == 0)
		return;

	com_kgdb_attach(puctag, pucgdbbase, CONSPEED, COM_FREQ,
	    COM_TYPE_NORMAL, CONMODE);
}
#endif

void
puc_cnprobe(struct consdev *cn)
{

	puccnbase = pucprobe_doit(cn);
}

int
puc_cninit(struct consdev *cn)
{

	if (puccnbase == 0)
		return -1;

	return comcnattach(puctag, puccnbase, CONSPEED,
	    puccnflags & PUC_COM_CLOCKMASK, COM_TYPE_NORMAL,
	    CONMODE);
}

/* comcngetc, comcnputc, comcnpollc provided by dev/ic/com.c */

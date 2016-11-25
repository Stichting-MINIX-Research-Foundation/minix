/*	$NetBSD: pci_at_mainbus.c,v 1.6 2015/06/15 15:38:52 pooka Exp $	*/

/*
 * Copyright (c) 2010 Antti Kantee.  All Rights Reserved.
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
__KERNEL_RCSID(0, "$NetBSD: pci_at_mainbus.c,v 1.6 2015/06/15 15:38:52 pooka Exp $");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/mbuf.h>
#include <sys/stat.h>
#include <sys/bus.h>

#include <dev/pci/pcivar.h>
#include <machine/bus_private.h>

#include "ioconf.c"

#include "rump_private.h"
#include "rump_vfs_private.h"

#include "pci_user.h"

RUMP_COMPONENT(RUMP_COMPONENT_DEV)
{
	extern const struct cdevsw pci_cdevsw;
	devmajor_t cmaj, bmaj;
	int error;

	config_init_component(cfdriver_ioconf_pci,
	    cfattach_ioconf_pci, cfdata_ioconf_pci);

	bmaj = cmaj = -1;
	if ((error = devsw_attach("pci", NULL, &bmaj,
	    &pci_cdevsw, &cmaj)) != 0) {
		printf("pci: devsw_attach failed: %d\n", error);
		return;
	}

	if ((error = rump_vfs_makedevnodes(S_IFCHR, "/dev/pci", '0',
	    cmaj, 0, 4)) != 0)
		printf("pci: failed to create /dev/pci nodes: %d\n", error);
}

RUMP_COMPONENT(RUMP_COMPONENT_DEV_AFTERMAINBUS)
{
	struct pcibus_attach_args pba;
	device_t mainbus;

	/* XXX: attach args should come from elsewhere */
	memset(&pba, 0, sizeof(pba));
	pba.pba_bus = 0;
	pba.pba_iot = (bus_space_tag_t)0;
	pba.pba_memt = (bus_space_tag_t)1;
	pba.pba_dmat = (void *)0x20;
#ifdef _LP64
	pba.pba_dmat64 = (void *)0x40;
#endif
	pba.pba_flags = PCI_FLAGS_MEM_OKAY |
	    PCI_FLAGS_MRL_OKAY | PCI_FLAGS_MRM_OKAY | PCI_FLAGS_MWI_OKAY;;

#ifdef RUMPCOMP_USERFEATURE_PCI_IOSPACE
	int error;

	error = rumpcomp_pci_iospace_init();
	if (!error) {
		pba.pba_flags |= PCI_FLAGS_IO_OKAY;
	} else {
		aprint_error("pci: I/O space init error %d, I/O space not "
		    "available\n", error);
	}
#endif

	mainbus = device_find_by_driver_unit("mainbus", 0);
	if (!mainbus)
		panic("no mainbus.  use maintaxi instead?");
	config_found_ia(mainbus, "pcibus", &pba, pcibusprint);
}

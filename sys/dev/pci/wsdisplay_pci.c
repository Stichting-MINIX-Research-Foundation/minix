/*	$NetBSD: wsdisplay_pci.c,v 1.1 2011/01/22 15:14:28 cegger Exp $ */
/*
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christoph Egger.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: wsdisplay_pci.c,v 1.1 2011/01/22 15:14:28 cegger Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <dev/pci/pcivar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/pci/wsdisplay_pci.h>

int
wsdisplayio_busid_pci(device_t self, pci_chipset_tag_t pc,
    pcitag_t tag, void *data)
{
	struct wsdisplayio_bus_id *busid = data;

	KASSERT(device_is_a(device_parent(self), "pci"));
	busid->bus_type = WSDISPLAYIO_BUS_PCI;
	busid->ubus.pci.domain = device_unit(device_parent(self));
	pci_decompose_tag(pc, tag,
	    &busid->ubus.pci.bus, &busid->ubus.pci.device,
	    &busid->ubus.pci.function);
	return 0;
}

/*	$NetBSD: pci_device.c,v 1.2 2003/03/08 09:53:45 lukem Exp $	*/

/*
 * Copyright 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Interface to PCI config space registers for a specific PCI device.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: pci_device.c,v 1.2 2003/03/08 09:53:45 lukem Exp $");

#include <sys/types.h>
#include <sys/ioctl.h>

#include <pci.h>

#include <dev/pci/pciio.h>

/*
 * pcidev_conf_read:
 *
 *	Read a config word for a given bus/device/function.
 */
int
pcidev_conf_read(int fd, u_int reg, uint32_t *valp)
{
	struct pciio_cfgreg r;
	int rv;

	r.reg = reg;

	rv = ioctl(fd, PCI_IOC_CFGREAD, &r);
	if (rv == -1) {
		/* errno already set. */
		return (-1);
	}

	*valp = r.val;
	return (0);
}

/*
 * pcidev_conf_write:
 *
 *	Write a config word for a given bus/device/function.
 */
int
pcidev_conf_write(int fd, u_int reg, uint32_t val)
{
	struct pciio_cfgreg r;

	r.reg = reg;
	r.val = val;

	return (ioctl(fd, PCI_IOC_CFGWRITE, &r));
}

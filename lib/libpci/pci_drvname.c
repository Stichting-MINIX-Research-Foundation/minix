/*	$NetBSD: pci_drvname.c,v 1.1 2014/07/25 01:38:26 mrg Exp $	*/

/*
 * Copyright (c) 2014 Matthew R. Green
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: pci_drvname.c,v 1.1 2014/07/25 01:38:26 mrg Exp $");

#include <sys/types.h>
#include <sys/ioctl.h>
#include <string.h>

#include <pci.h>

#include <dev/pci/pciio.h>

/*
 * pci_drvname:
 *
 *	What's the driver name for a PCI device?
 */
int
pci_drvname(int fd, u_int device, u_int func, char *name, size_t len)
{
	struct pciio_drvname drvname;
	int rv;

	drvname.device = device;
	drvname.function = func;

	rv = ioctl(fd, PCI_IOC_DRVNAME, &drvname);
	if (rv == -1)
		return -1;

	strlcpy(name, drvname.name, len);
	return 0;
}

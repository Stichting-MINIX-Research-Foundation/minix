/*	$NetBSD: cyber.c,v 1.5 2008/04/28 20:23:54 martin Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Frederick S. Bruckman.
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

/* Store one "Usr" register on an SIIG Cyberserial multiport PCI card. */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cyber.c,v 1.5 2008/04/28 20:23:54 martin Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/termios.h> /* XXX for tcflag_t in comvar.h */

#include <sys/bus.h>

#include <dev/pci/cyberreg.h>
#include <dev/pci/cybervar.h>

void
write_siig10x_usrreg(pci_chipset_tag_t pc, pcitag_t tag, int usrregno,
    int high_speed)
{
	pcireg_t curregs, newregs;

	newregs = curregs = pci_conf_read(pc, tag, SIIG10x_USR_BASE);

	if (high_speed)					/* Clear bit. */
		switch (usrregno) {
		case 0:
			newregs &= ~SIIG10x_USR0_MASK;
			break;
		case 1:
			newregs &= ~SIIG10x_USR1_MASK;
			break;
		case 2:
			newregs &= ~SIIG10x_USR2_MASK;
			break;
		case 3:
			newregs &= ~SIIG10x_USR3_MASK;
		}
	else /* if (!high_speed) */			/* Set bit. */
		switch (usrregno) {
		case 0:
			newregs |= SIIG10x_USR0_MASK;
			break;
		case 1:
			newregs |= SIIG10x_USR1_MASK;
			break;
		case 2:
			newregs |= SIIG10x_USR2_MASK;
			break;
		case 3:
			newregs |= SIIG10x_USR3_MASK;
		}

	if (newregs != curregs)
		pci_conf_write(pc, tag, SIIG10x_USR_BASE, newregs);
}

void
write_siig20x_usrreg(pci_chipset_tag_t pc, pcitag_t tag, int usrregno,
    int high_speed)
{
	pcireg_t curreg, newreg;
	int offset;

	switch (usrregno) {
		case 0:
			offset = SIIG20x_USR0;
			break;
		case 1:
			offset = SIIG20x_USR1;
			break;
		default:
			return;
	}

	newreg = curreg = pci_conf_read(pc, tag, offset);

	if (high_speed)					/* Clear bit. */
		newreg &= ~SIIG20x_USR_MASK;
	else /* if (!high_speed) */			/* Set bit. */
		newreg |= SIIG20x_USR_MASK;

	if (newreg != curreg)
		pci_conf_write(pc, tag, offset, newreg);
}

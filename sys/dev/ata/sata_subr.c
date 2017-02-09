/*	$NetBSD: sata_subr.c,v 1.21 2013/04/03 17:15:07 bouyer Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of Wasabi Systems, Inc.
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

/*
 * Common functions for Serial ATA.
 */
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sata_subr.c,v 1.21 2013/04/03 17:15:07 bouyer Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/proc.h>

#include <dev/ata/satareg.h>
#include <dev/ata/satavar.h>
#include <dev/ata/satapmpreg.h>

/*
 * sata_speed:
 *
 *	Return a string describing the port speed reported by
 *	the port's SStatus register.
 */
const char *
sata_speed(uint32_t sstatus)
{
	static const char * const sata_speedtab[] = {
		"no negotiated speed",
		"1.5Gb/s",
		"3.0Gb/s",
		"6.0Gb/s",
		"<unknown 4>",
		"<unknown 5>",
		"<unknown 6>",
		"<unknown 7>",
		"<unknown 8>",
		"<unknown 9>",
		"<unknown 10>",
		"<unknown 11>",
		"<unknown 12>",
		"<unknown 13>",
		"<unknown 14>",
		"<unknown 15>",
	};

	return (sata_speedtab[(sstatus & SStatus_SPD_mask) >>
			      SStatus_SPD_shift]);
}

/*
 * reset the PHY and bring it online
 */
uint32_t
sata_reset_interface(struct ata_channel *chp, bus_space_tag_t sata_t,
    bus_space_handle_t scontrol_r, bus_space_handle_t sstatus_r, int flags)
{
	uint32_t scontrol, sstatus;
	int i;

	/* bring the PHYs online.
	 * The work-around for errata #1 of the Intel GD31244 says that we must
	 * write 0 to the port first to be sure of correctly initializing
	 * the device. It doesn't hurt for other devices.
	 */
	bus_space_write_4(sata_t, scontrol_r, 0, 0);
	scontrol = SControl_IPM_NONE | SControl_SPD_ANY | SControl_DET_INIT;
	bus_space_write_4 (sata_t, scontrol_r, 0, scontrol);

	ata_delay(50, "sataup", flags);
	scontrol &= ~SControl_DET_INIT;
	bus_space_write_4(sata_t, scontrol_r, 0, scontrol);

	ata_delay(50, "sataup", flags);
	/* wait up to 1s for device to come up */
	for (i = 0; i < 100; i++) {
		sstatus = bus_space_read_4(sata_t, sstatus_r, 0);
		if ((sstatus & SStatus_DET_mask) == SStatus_DET_DEV)
			break;
		ata_delay(10, "sataup", flags);
	}
	/*
	 * if we have a link up without device, wait a few more seconds
	 * for connection to establish
	 */
	if ((sstatus & SStatus_DET_mask) == SStatus_DET_DEV_NE) {
		for (i = 0; i < 500; i++) {
			ata_delay(10, "sataup", flags);
			sstatus = bus_space_read_4(sata_t, sstatus_r, 0);
			if ((sstatus & SStatus_DET_mask) == SStatus_DET_DEV)
				break;
		}
	}

	switch (sstatus & SStatus_DET_mask) {
	case SStatus_DET_NODEV:
		/* No Device; be silent.  */
		break;

	case SStatus_DET_DEV_NE:
		aprint_error("%s port %d: device connected, but "
		    "communication not established\n",
		    device_xname(chp->ch_atac->atac_dev), chp->ch_channel);
		break;

	case SStatus_DET_OFFLINE:
		aprint_error("%s port %d: PHY offline\n",
		    device_xname(chp->ch_atac->atac_dev), chp->ch_channel);
		break;

	case SStatus_DET_DEV:
		aprint_normal("%s port %d: device present, speed: %s\n",
		    device_xname(chp->ch_atac->atac_dev), chp->ch_channel,
		    sata_speed(sstatus));
		break;
	default:
		aprint_error("%s port %d: unknown SStatus: 0x%08x\n",
		    device_xname(chp->ch_atac->atac_dev), chp->ch_channel,
		    sstatus);
	}
	return(sstatus & SStatus_DET_mask);
}

void
sata_interpret_sig(struct ata_channel *chp, int port, uint32_t sig)
{
	int err;
	int s;

	/* some ATAPI devices have bogus lower two bytes, sigh */
	if ((sig & 0xffff0000) == 0xeb140000) {
		sig &= 0xffff0000;
		sig |= 0x00000101;
	}
	if (chp->ch_drive == NULL) {
		if (sig == 0x96690101)
			err = atabus_alloc_drives(chp, PMP_MAX_DRIVES);
		else
			err = atabus_alloc_drives(chp, 1);
		if (err)
			return;
	}
	KASSERT(port < chp->ch_ndrives);

	s = splbio();
	switch(sig) {
	case 0x96690101:
		KASSERT(port == 0 || port == PMP_PORT_CTL);
		chp->ch_drive[PMP_PORT_CTL].drive_type = ATA_DRIVET_PM;
		break;
	case 0xc33c0101:
		aprint_verbose_dev(chp->atabus, "port %d is SEMB, ignored\n",
		    port);
		break;
	case 0xeb140101:
		chp->ch_drive[port].drive_type = ATA_DRIVET_ATAPI;
		break;
	case 0x00000101:
		chp->ch_drive[port].drive_type = ATA_DRIVET_ATA;
		break;
	case 0xffffffff:
		/* COMRESET time out */
		break;
	default:
		chp->ch_drive[port].drive_type = ATA_DRIVET_ATA;
		aprint_verbose_dev(chp->atabus,
		    "Unrecognized signature 0x%08x on port %d. "
		    "Assuming it's a disk.\n", sig, port);
		break;
	}
	splx(s);
}

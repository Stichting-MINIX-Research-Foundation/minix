/*	$NetBSD: eisa.c,v 1.45 2008/04/06 08:54:43 cegger Exp $	*/

/*
 * Copyright (c) 1995, 1996 Christopher G. Demetriou
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *      for the NetBSD Project.
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
 * EISA Bus device
 *
 * Makes sure an EISA bus is present, and finds and attaches devices
 * living on it.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: eisa.c,v 1.45 2008/04/06 08:54:43 cegger Exp $");

#include "opt_eisaverbose.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <sys/bus.h>

#include <dev/eisa/eisareg.h>
#include <dev/eisa/eisavar.h>
#include <dev/eisa/eisadevs.h>

#include "locators.h"

static int	eisamatch(device_t, cfdata_t, void *);
static void	eisaattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(eisa, 0,
    eisamatch, eisaattach, NULL, NULL);

static int	eisaprint(void *, const char *);
static void	eisa_devinfo(const char *, char *, size_t);

static int
eisamatch(device_t parent, cfdata_t cf,
    void *aux)
{
	/* XXX check other indicators */

	return (1);
}

static int
eisaprint(void *aux, const char *pnp)
{
	struct eisa_attach_args *ea = aux;
	char devinfo[256];

	if (pnp) {
		eisa_devinfo(ea->ea_idstring, devinfo, sizeof(devinfo));
		aprint_normal("%s at %s", devinfo, pnp);
	}
	aprint_normal(" slot %d", ea->ea_slot);
	return (UNCONF);
}

static void
eisaattach(device_t parent, device_t self, void *aux)
{
	struct eisabus_attach_args *eba = aux;
	bus_space_tag_t iot, memt;
	bus_dma_tag_t dmat;
	eisa_chipset_tag_t ec;
	int slot, maxnslots;

	eisa_attach_hook(parent, self, eba);
	printf("\n");

	iot = eba->eba_iot;
	memt = eba->eba_memt;
	ec = eba->eba_ec;
	dmat = eba->eba_dmat;

	/*
	 * Search for and attach subdevices.
	 *
	 * Slot 0 is the "motherboard" slot, and the code attaching
	 * the EISA bus should have already attached an ISA bus there.
	 */
	maxnslots = eisa_maxslots(ec);
	for (slot = 1; slot < maxnslots; slot++) {
		struct eisa_attach_args ea;
		u_int slotaddr;
		bus_space_handle_t slotioh;
		int i;
		int locs[EISACF_NLOCS];

		ea.ea_iot = iot;
		ea.ea_memt = memt;
		ea.ea_ec = ec;
		ea.ea_dmat = dmat;
		ea.ea_slot = slot;
		slotaddr = EISA_SLOT_ADDR(slot);

		/*
		 * Get a mapping for the whole slot-specific address
		 * space.  If we can't, assume nothing's there but warn
		 * about it.
		 */
		if (bus_space_map(iot, slotaddr, EISA_SLOT_SIZE, 0, &slotioh)) {
			aprint_error_dev(self, "can't map I/O space for slot %d\n",
			    slot);
			continue;
		}

		/* Get the vendor ID bytes */
		for (i = 0; i < EISA_NVIDREGS; i++)
			ea.ea_vid[i] = bus_space_read_1(iot, slotioh,
			    EISA_SLOTOFF_VID + i);

		/* Check for device existence */
		if (EISA_VENDID_NODEV(ea.ea_vid)) {
#if 0
			printf("no device at %s slot %d\n", device_xname(self),
			    slot);
			printf("\t(0x%x, 0x%x)\n", ea.ea_vid[0],
			    ea.ea_vid[1]);
#endif
			bus_space_unmap(iot, slotioh, EISA_SLOT_SIZE);
			continue;
		}

		/* And check that the firmware didn't biff something badly */
		if (EISA_VENDID_IDDELAY(ea.ea_vid)) {
			printf("%s slot %d not configured by BIOS?\n",
			    device_xname(self), slot);
			bus_space_unmap(iot, slotioh, EISA_SLOT_SIZE);
			continue;
		}

		/* Get the product ID bytes */
		for (i = 0; i < EISA_NPIDREGS; i++)
			ea.ea_pid[i] = bus_space_read_1(iot, slotioh,
			    EISA_SLOTOFF_PID + i);

		/* Create the ID string from the vendor and product IDs */
		ea.ea_idstring[0] = EISA_VENDID_0(ea.ea_vid);
		ea.ea_idstring[1] = EISA_VENDID_1(ea.ea_vid);
		ea.ea_idstring[2] = EISA_VENDID_2(ea.ea_vid);
		ea.ea_idstring[3] = EISA_PRODID_0(ea.ea_pid);
		ea.ea_idstring[4] = EISA_PRODID_1(ea.ea_pid);
		ea.ea_idstring[5] = EISA_PRODID_2(ea.ea_pid);
		ea.ea_idstring[6] = EISA_PRODID_3(ea.ea_pid);
		ea.ea_idstring[7] = '\0';		/* sanity */

		/* We no longer need the I/O handle; free it. */
		bus_space_unmap(iot, slotioh, EISA_SLOT_SIZE);

		locs[EISACF_SLOT] = slot;

		/* Attach matching device. */
		config_found_sm_loc(self, "eisa", locs, &ea,
				    eisaprint, config_stdsubmatch);
	}
}

#ifdef EISAVERBOSE
/*
 * Descriptions of of known vendors and devices ("products").
 */
struct eisa_knowndev {
	int	flags;
	const char *id, *name;
};
#define EISA_KNOWNDEV_NOPROD	0x01		/* match on vendor only */

#include <dev/eisa/eisadevs_data.h>
#endif	/* EISAVEBSOSE */

void
eisa_devinfo(const char *id, char *cp, size_t l)
{
#ifdef EISAVERBOSE
	const char *name;
	const struct eisa_knowndev *edp;
	int match, onlyvendor;

	onlyvendor = 0;
	name = NULL;

	/* find the device in the table, if possible. */
	for (edp = eisa_knowndevs; edp->id != NULL; edp++) {
		if ((edp->flags & EISA_KNOWNDEV_NOPROD) != 0)
			match = (strncmp(edp->id, id, 3) == 0);
		else
			match = (strcmp(edp->id, id) == 0);
		if (match) {
			name = edp->name;
			onlyvendor = (edp->flags & EISA_KNOWNDEV_NOPROD) != 0;
			break;
		}
	}

	if (name == NULL)
		snprintf(cp, l, "unknown device %s", id);
	else if (onlyvendor)
		snprintf(cp, l, "unknown %s device %s", name, id);
	else
		snprintf(cp, l, "%s", name);
#else	/* EISAVERBOSE */

	snprintf(cp, l, "device %s", id);
#endif	/* EISAVERBOSE */
}

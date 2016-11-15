/*	$NetBSD: ofbus.c,v 1.25 2011/06/03 07:39:30 matt Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ofbus.c,v 1.25 2011/06/03 07:39:30 matt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/ofw/openfirm.h>

int ofbus_match(device_t, cfdata_t, void *);
void ofbus_attach(device_t, device_t, void *);
static int ofbus_print(void *, const char *);

CFATTACH_DECL_NEW(ofbus, 0,
    ofbus_match, ofbus_attach, NULL, NULL);

static int
ofbus_print(void *aux, const char *pnp)
{
	struct ofbus_attach_args *oba = aux;

	if (pnp)
		aprint_normal("%s at %s", oba->oba_ofname, pnp);
	else
		aprint_normal(" (%s)", oba->oba_ofname);
	return UNCONF;
}

int
ofbus_match(device_t parent, cfdata_t cf, void *aux)
{
	struct ofbus_attach_args *oba = aux;

	if (strcmp(oba->oba_busname, "ofw"))
		return (0);
	if (!OF_child(oba->oba_phandle))
		return (0);
	return (1);
}

void
ofbus_attach(device_t parent, device_t dev, void *aux)
{
	struct ofbus_attach_args *oba = aux;
	struct ofbus_attach_args oba2;
	char name[64];
	int child, units;

	printf("\n");

	/*
	 * This is a hack to make the probe work on the scsi (and ide) bus.
	 * YES, I THINK IT IS A BUG IN THE OPENFIRMWARE TO NOT PROBE ALL
	 * DEVICES ON THESE BUSSES.
	 */
	units = 1;
	name[0] = 0;
	if (OF_getprop(oba->oba_phandle, "name", name, sizeof name) > 0) {
		if (!strcmp(name, "scsi"))
			units = 7; /* What about wide or hostid != 7?	XXX */
		else if (!strcmp(name, "ide"))
			units = 2;
	}

	for (child = OF_child(oba->oba_phandle); child != 0;
	     child = OF_peer(child)) {
		oba2.oba_busname = "ofw";
		of_packagename(child, name, sizeof name);
		oba2.oba_phandle = child;
		for (oba2.oba_unit = 0; oba2.oba_unit < units;
		     oba2.oba_unit++) {
			if (units > 1) {
				snprintf(oba2.oba_ofname,
				    sizeof(oba2.oba_ofname), "%s@%d", name,
				    oba2.oba_unit);
			} else {
				strlcpy(oba2.oba_ofname, name,
				    sizeof(oba2.oba_ofname));
			}
			config_found(dev, &oba2, ofbus_print);
		}
	}
}

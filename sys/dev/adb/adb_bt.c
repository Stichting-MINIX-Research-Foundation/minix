/*	$NetBSD: adb_bt.c,v 1.6 2010/09/08 04:48:03 macallan Exp $ */

/*-
 * Copyright (c) 2006 Michael Lorenz
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
__KERNEL_RCSID(0, "$NetBSD: adb_bt.c,v 1.6 2010/09/08 04:48:03 macallan Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <machine/autoconf.h>
#include <machine/adbsys.h>
#include <machine/keyboard.h>

#include <dev/adb/adbvar.h>

#include "opt_wsdisplay_compat.h"
#include "adbdebug.h"

#ifdef ADBBT_DEBUG
#define DPRINTF printf
#else
#define DPRINTF while (0) printf
#endif

#define BT_VOL_UP	0x06
#define BT_VOL_DOWN	0x07
#define BT_VOL_MUTE	0x08
#define BT_BRT_UP	0x09
#define BT_BRT_DOWN	0x0a
#define BT_EJECT	0x0b
#define BT_F7		0x0c
#define BT_NUMLOCK	0x7f

static int adbbt_match(device_t, cfdata_t, void *);
static void adbbt_attach(device_t, device_t, void *);

struct adbbt_softc {
	device_t sc_dev;
	struct adb_device *sc_adbdev;
	struct adb_bus_accessops *sc_ops;
	int sc_msg_len;
	int sc_event;
	uint8_t sc_buffer[16];
	uint8_t sc_us;
};	

/* Driver definition. */
CFATTACH_DECL_NEW(adbbt, sizeof(struct adbbt_softc),
    adbbt_match, adbbt_attach, NULL, NULL);

extern struct cfdriver adbbt_cd;

static void adbbt_handler(void *, int, uint8_t *);

static int
adbbt_match(device_t parent, cfdata_t cf, void *aux)
{
	struct adb_attach_args *aaa = aux;

	if ((aaa->dev->original_addr == ADBADDR_MISC) &&
	    (aaa->dev->handler_id == 0x1f))
		return 100;
	else
		return 0;
}

static void
adbbt_attach(device_t parent, device_t self, void *aux)
{
	struct adbbt_softc *sc = device_private(self);
	struct adb_attach_args *aaa = aux;

	sc->sc_dev = self;
	sc->sc_ops = aaa->ops;
	sc->sc_adbdev = aaa->dev;
	sc->sc_adbdev->cookie = sc;
	sc->sc_adbdev->handler = adbbt_handler;
	sc->sc_us = ADBTALK(sc->sc_adbdev->current_addr, 0);

	sc->sc_msg_len = 0;

	printf(" addr %d: button device\n", sc->sc_adbdev->current_addr);
}

static void
adbbt_handler(void *cookie, int len, uint8_t *data)
{
	/* struct adbbt_softc *sc = cookie; */
	uint8_t k, scancode;

#ifdef ADBBT_DEBUG
	struct adbbt_softc *sc = cookie;
	int i;
	printf("%s: %02x - ", device_xname(sc->sc_dev), sc->sc_us);
	for (i = 0; i < len; i++) {
		printf(" %02x", data[i]);
	}
	printf("\n");
#endif
	k = data[2];
	scancode = ADBK_KEYVAL(k);
	if ((scancode < 6) || (scancode > 0x0c))
		return;

	if (ADBK_PRESS(k)) {

		switch (scancode) {
			case BT_VOL_UP:
				pmf_event_inject(NULL, PMFE_AUDIO_VOLUME_UP);
				break;
			case BT_VOL_DOWN:
				pmf_event_inject(NULL, PMFE_AUDIO_VOLUME_DOWN);
				break;
			case BT_VOL_MUTE:
				pmf_event_inject(NULL, 
				    PMFE_AUDIO_VOLUME_TOGGLE);
				break;
			case BT_BRT_UP:
				pmf_event_inject(NULL, 
				    PMFE_DISPLAY_BRIGHTNESS_UP);
				break;
			case BT_BRT_DOWN:
				pmf_event_inject(NULL, 
				    PMFE_DISPLAY_BRIGHTNESS_DOWN);
				break;
		}
	}
}

/*	$NetBSD: adb_bus.c,v 1.10 2014/11/08 17:21:51 macallan Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: adb_bus.c,v 1.10 2014/11/08 17:21:51 macallan Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <sys/bus.h>
#include <machine/autoconf.h>
#include <dev/adb/adbvar.h>

#include "adbdebug.h"

#ifdef ADB_DEBUG
#define DPRINTF printf
#else
#define DPRINTF while (0) printf
#endif

static int nadb_match(device_t, cfdata_t, void *);
static void nadb_attach(device_t, device_t, void *);

struct nadb_softc {
	device_t sc_dev;
	struct adb_bus_accessops *sc_ops;
	uint32_t sc_msg;
	uint32_t sc_event;
	struct adb_device sc_devtable[16];
	int sc_free;	/* highest free address */
	int sc_len;	/* length of received message */
	uint8_t sc_data[16];
};

CFATTACH_DECL_NEW(nadb, sizeof(struct nadb_softc),
    nadb_match, nadb_attach, NULL, NULL);

static void nadb_init(device_t);
static void nadb_handler(void *, int, uint8_t *);
static void nadb_send_sync(void *, int, int, uint8_t *);
static int nadb_register(struct nadb_softc *, int, int, int);
static void nadb_remove(struct nadb_softc *, int);
static int nadb_devprint(void *, const char *);

static int
nadb_match(device_t parent, cfdata_t cf, void *aux)
{

	return 1;
}

static void
nadb_attach(device_t parent, device_t self, void *aux)
{
	struct nadb_softc *sc = device_private(self);
	struct adb_bus_accessops *ops = aux;

	sc->sc_dev = self;
	sc->sc_ops = ops;
	sc->sc_ops->set_handler(sc->sc_ops->cookie, nadb_handler, sc);

	config_interrupts(self, nadb_init);
}

static void
nadb_init(device_t dev)
{
	struct nadb_softc *sc = device_private(dev);
	struct adb_attach_args aaa;
	int i, last_moved_up, devmask = 0;
	uint8_t cmd[2];

	sc->sc_free = 15;
	for (i = 0; i < 16; i++) {
		sc->sc_devtable[i].original_addr = 0;
		sc->sc_devtable[i].current_addr = 0;
		sc->sc_devtable[i].handler_id = 0;
		sc->sc_devtable[i].cookie = NULL;
		sc->sc_devtable[i].handler = NULL;
	}

	/* bus reset (?) */
	nadb_send_sync(sc, 0, 0, NULL);
	delay(200000);

	/* 
	 * scan only addresses 1 - 7 
	 * if something responds move it to >7 and see if something else is 
	 * there. If not move the previous one back.
	 * XXX we don't check for collisions if we use up all addresses >7
	 */
	for (i = 1; i < 8; i++) {
		DPRINTF("\n%d: ", i);
		last_moved_up = 0;
		nadb_send_sync(sc, ADBTALK(i, 3), 0, NULL);
		/* found something? */
		while (sc->sc_len > 2) {
			/* something answered, so move it up */

			DPRINTF("Found a device on address %d\n", i);
			cmd[0] = sc->sc_free | 0x60;
			cmd[1] = 0xfe;
			nadb_send_sync(sc, ADBLISTEN(i, 3), 2, cmd);

			/* see if it really moved */
			nadb_send_sync(sc, ADBTALK(sc->sc_free, 3), 0, NULL);
			if (sc->sc_len > 2) {
				/* ok */
				DPRINTF("moved it to %d\n", sc->sc_free);
				nadb_register(sc, sc->sc_free, i, sc->sc_data[3]);
				last_moved_up = sc->sc_free;
				sc->sc_free--;
			}
			/* see if something else is there */
			nadb_send_sync(sc, ADBTALK(i, 3), 0, NULL);
		}
		if (last_moved_up != 0) {
			/* move last one back to original address */
			cmd[0] = i | 0x60;
			cmd[1] = 0xfe;
			nadb_send_sync(sc, ADBLISTEN(last_moved_up, 3), 2, cmd);

			nadb_send_sync(sc, ADBTALK(i, 3), 0, NULL);
			if (sc->sc_len > 2) {
				DPRINTF("moved %d back to %d\n", last_moved_up, i);
				nadb_remove(sc, last_moved_up);
				nadb_register(sc, i, i, sc->sc_data[3]);
				sc->sc_free = last_moved_up;
			}
		}
	}

	/* now attach the buggers we've found */
	aaa.ops = sc->sc_ops;
	for (i = 0; i < 16; i++) {
		if (sc->sc_devtable[i].current_addr != 0) {
			DPRINTF("dev: %d %d %02x\n",
			    sc->sc_devtable[i].current_addr,
			    sc->sc_devtable[i].original_addr,
			    sc->sc_devtable[i].handler_id);
			aaa.dev = &sc->sc_devtable[i];
			if (config_found(sc->sc_dev, &aaa, nadb_devprint)) {
				devmask |= (1 << i);
			} else {
				aprint_normal(" not configured\n");
			}
		}
	}
	/* now enable autopolling */
	DPRINTF("devmask: %04x\n", devmask);
	sc->sc_ops->autopoll(sc->sc_ops->cookie, devmask);
}

int
nadb_print(void *aux, const char *what)
{
	aprint_normal(": Apple Desktop Bus\n");
	return 0;
}

static int
nadb_devprint(void *aux, const char *what)
{
	struct adb_attach_args *aaa = aux;

	if (what == NULL)
		return 0;

	switch(aaa->dev->original_addr) {
		case 2:
			aprint_normal("%s: ADB Keyboard", what);
			break;
		case 3:
			aprint_normal("%s: ADB relative pointing device", what);
			break;
		default:
			aprint_normal("%s: something from address %d:%02x",
			    what,
			    aaa->dev->original_addr,
			    aaa->dev->handler_id);
			break;
	}
	return 0;
}

static void
nadb_handler(void *cookie, int len, uint8_t *data)
{
	struct nadb_softc *sc = cookie;
	struct adb_device *dev;
	int addr;

#ifdef ADB_DEBUG
	int i;
	printf("adb:");
	for (i = 0; i < len; i++) {
		printf(" %02x", data[i]);
	}
	printf("\n");
#endif

	addr = data[1] >> 4;
	dev  = &sc->sc_devtable[addr];
	if ((dev->current_addr != 0) && (dev->handler != NULL)) {

		dev->handler(dev->cookie, len, data);
	} else {
		sc->sc_msg = 1;
		sc->sc_len = len;
		memcpy(sc->sc_data, data, len);
		wakeup(&sc->sc_event);
	}
}

static void
nadb_send_sync(void *cookie, int command, int len, uint8_t *data)
{
	struct nadb_softc *sc = cookie;

	sc->sc_msg = 0;
	sc->sc_ops->send(sc->sc_ops->cookie, 0, command, len, data);
	while (sc->sc_msg == 0) {
		tsleep(&sc->sc_event, 0, "adb_send", 100);
	}
}

static int
nadb_register(struct nadb_softc *sc, int current, int orig, int handler)
{
	struct adb_device *dev;

	if ((current > 0) && (current < 16)) {
		dev = &sc->sc_devtable[current];		
		if (dev->current_addr != 0)
			/* in use! */
			return -1;
		dev->current_addr = current;
		dev->original_addr = orig;
		dev->handler_id = handler;
		return 0;
	}
	return -1;
}

static void
nadb_remove(struct nadb_softc *sc, int addr)
{

	if ((addr > 0) && (addr < 16)) {
		sc->sc_devtable[addr].current_addr = 0;
		sc->sc_devtable[addr].original_addr = 0;
		sc->sc_devtable[addr].handler_id = 0;
	}
}

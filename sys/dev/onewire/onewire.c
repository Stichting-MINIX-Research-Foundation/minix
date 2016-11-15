/* $NetBSD: onewire.c,v 1.16 2014/07/25 08:10:38 dholland Exp $ */
/*	$OpenBSD: onewire.c,v 1.1 2006/03/04 16:27:03 grange Exp $	*/

/*
 * Copyright (c) 2006 Alexander Yurchenko <grange@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: onewire.c,v 1.16 2014/07/25 08:10:38 dholland Exp $");

/*
 * 1-Wire bus driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/rwlock.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/module.h>

#include <dev/onewire/onewirereg.h>
#include <dev/onewire/onewirevar.h>

#ifdef ONEWIRE_DEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

//#define ONEWIRE_MAXDEVS		256
#define ONEWIRE_MAXDEVS		8
#define ONEWIRE_SCANTIME	3

struct onewire_softc {
	device_t			sc_dev;

	struct onewire_bus *		sc_bus;
	krwlock_t			sc_rwlock;
	struct lwp *			sc_thread;
	TAILQ_HEAD(, onewire_device)	sc_devs;

	int				sc_dying;
};

struct onewire_device {
	TAILQ_ENTRY(onewire_device)	d_list;
	device_t			d_dev;
	u_int64_t			d_rom;
	int				d_present;
};

static int	onewire_match(device_t, cfdata_t, void *);
static void	onewire_attach(device_t, device_t, void *);
static int	onewire_detach(device_t, int);
static int	onewire_activate(device_t, enum devact);
int		onewire_print(void *, const char *);

static void	onewire_thread(void *);
static void	onewire_scan(struct onewire_softc *);

CFATTACH_DECL_NEW(onewire, sizeof(struct onewire_softc),
	onewire_match, onewire_attach, onewire_detach, onewire_activate);

const struct cdevsw onewire_cdevsw = {
	.d_open = noopen,
	.d_close = noclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = noioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

extern struct cfdriver onewire_cd;

static int
onewire_match(device_t parent, cfdata_t cf, void *aux)
{
	return 1;
}

static void
onewire_attach(device_t parent, device_t self, void *aux)
{
	struct onewire_softc *sc = device_private(self);
	struct onewirebus_attach_args *oba = aux;

	sc->sc_dev = self;
	sc->sc_bus = oba->oba_bus;
	rw_init(&sc->sc_rwlock);
	TAILQ_INIT(&sc->sc_devs);

	aprint_normal("\n");

	if (kthread_create(PRI_NONE, 0, NULL, onewire_thread, sc,
	    &sc->sc_thread, "%s", device_xname(self)) != 0)
		aprint_error_dev(self, "can't create kernel thread\n");
}

static int
onewire_detach(device_t self, int flags)
{
	struct onewire_softc *sc = device_private(self);
	int rv;

	sc->sc_dying = 1;
	if (sc->sc_thread != NULL) {
		wakeup(sc->sc_thread);
		tsleep(&sc->sc_dying, PWAIT, "owdt", 0);
	}

	onewire_lock(sc);
	//rv = config_detach_children(self, flags);
	rv = 0;  /* XXX riz */
	onewire_unlock(sc);
	rw_destroy(&sc->sc_rwlock);

	return rv;
}

static int
onewire_activate(device_t self, enum devact act)
{
	struct onewire_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

int
onewire_print(void *aux, const char *pnp)
{
	struct onewire_attach_args *oa = aux;
	const char *famname;

	if (pnp == NULL)
		aprint_normal(" ");

	famname = onewire_famname(ONEWIRE_ROM_FAMILY_TYPE(oa->oa_rom));
	if (famname == NULL)
		aprint_normal("family 0x%02x",
		    (uint)ONEWIRE_ROM_FAMILY_TYPE(oa->oa_rom));
	else
		aprint_normal("\"%s\"", famname);
	aprint_normal(" sn %012llx", ONEWIRE_ROM_SN(oa->oa_rom));

	if (pnp != NULL)
		aprint_normal(" at %s", pnp);

	return UNCONF;
}

int
onewirebus_print(void *aux, const char *pnp)
{
	if (pnp != NULL)
		aprint_normal("onewire at %s", pnp);

	return UNCONF;
}

void
onewire_lock(void *arg)
{
	struct onewire_softc *sc = arg;

	rw_enter(&sc->sc_rwlock, RW_WRITER);
}

void
onewire_unlock(void *arg)
{
	struct onewire_softc *sc = arg;

	rw_exit(&sc->sc_rwlock);
}

int
onewire_reset(void *arg)
{
	struct onewire_softc *sc = arg;
	struct onewire_bus *bus = sc->sc_bus;

	return bus->bus_reset(bus->bus_cookie);
}

int
onewire_bit(void *arg, int value)
{
	struct onewire_softc *sc = arg;
	struct onewire_bus *bus = sc->sc_bus;

	return bus->bus_bit(bus->bus_cookie, value);
}

int
onewire_read_byte(void *arg)
{
	struct onewire_softc *sc = arg;
	struct onewire_bus *bus = sc->sc_bus;
	uint8_t value = 0;
	int i;

	if (bus->bus_read_byte != NULL)
		return bus->bus_read_byte(bus->bus_cookie);

	for (i = 0; i < 8; i++)
		value |= (bus->bus_bit(bus->bus_cookie, 1) << i);

	return value;
}

void
onewire_write_byte(void *arg, int value)
{
	struct onewire_softc *sc = arg;
	struct onewire_bus *bus = sc->sc_bus;
	int i;

	if (bus->bus_write_byte != NULL)
		return bus->bus_write_byte(bus->bus_cookie, value);

	for (i = 0; i < 8; i++)
		bus->bus_bit(bus->bus_cookie, (value >> i) & 0x1);
}

int
onewire_triplet(void *arg, int dir)
{
	struct onewire_softc *sc = arg;
	struct onewire_bus *bus = sc->sc_bus;
	int rv;

	if (bus->bus_triplet != NULL)
		return bus->bus_triplet(bus->bus_cookie, dir);

	rv = bus->bus_bit(bus->bus_cookie, 1);
	rv <<= 1;
	rv |= bus->bus_bit(bus->bus_cookie, 1);

	switch (rv) {
	case 0x0:
		bus->bus_bit(bus->bus_cookie, dir);
		break;
	case 0x1:
		bus->bus_bit(bus->bus_cookie, 0);
		break;
	default:
		bus->bus_bit(bus->bus_cookie, 1);
	}

	return rv;
}

void
onewire_read_block(void *arg, void *buf, int len)
{
	uint8_t *p = buf;

	while (len--)
		*p++ = onewire_read_byte(arg);
}

void
onewire_write_block(void *arg, const void *buf, int len)
{
	const uint8_t *p = buf;

	while (len--)
		onewire_write_byte(arg, *p++);
}

void
onewire_matchrom(void *arg, u_int64_t rom)
{
	int i;

	onewire_write_byte(arg, ONEWIRE_CMD_MATCH_ROM);
	for (i = 0; i < 8; i++)
		onewire_write_byte(arg, (rom >> (i * 8)) & 0xff);
}

static void
onewire_thread(void *arg)
{
	struct onewire_softc *sc = arg;

	while (!sc->sc_dying) {
		onewire_scan(sc);
		tsleep(sc->sc_thread, PWAIT, "owidle", ONEWIRE_SCANTIME * hz);
	}

	sc->sc_thread = NULL;
	wakeup(&sc->sc_dying);
	kthread_exit(0);
}

static void
onewire_scan(struct onewire_softc *sc)
{
	struct onewire_device *d, *next, *nd;
	struct onewire_attach_args oa;
	device_t dev;
	int search = 1, count = 0, present;
	int dir, rv;
	uint64_t mask, rom = 0, lastrom;
	uint8_t data[8];
	int i, i0 = -1, lastd = -1;

	TAILQ_FOREACH(d, &sc->sc_devs, d_list)
		d->d_present = 0;

	while (search && count++ < ONEWIRE_MAXDEVS) {
		/* XXX: yield processor */
		tsleep(sc, PWAIT, "owscan", hz / 10);

		/*
		 * Reset the bus. If there's no presence pulse
		 * don't search for any devices.
		 */
		onewire_lock(sc);
		if (onewire_reset(sc) != 0) {
			DPRINTF(("%s: scan: no presence pulse\n",
			    device_xname(sc->sc_dev)));
			onewire_unlock(sc);
			break;
		}

		/*
		 * Start new search. Go through the previous path to
		 * the point we made a decision last time and make an
		 * opposite decision. If we didn't make any decision
		 * stop searching.
		 */
		search = 0;
		lastrom = rom;
		rom = 0;
		onewire_write_byte(sc, ONEWIRE_CMD_SEARCH_ROM);
		for (i = 0,i0 = -1; i < 64; i++) {
			dir = (lastrom >> i) & 0x1;
			if (i == lastd)
				dir = 1;
			else if (i > lastd)
				dir = 0;
			rv = onewire_triplet(sc, dir);
			switch (rv) {
			case 0x0:
				if (i != lastd) {
					if (dir == 0)
						i0 = i;
					search = 1;
				}
				mask = dir;
				break;
			case 0x1:
				mask = 0;
				break;
			case 0x2:
				mask = 1;
				break;
			default:
				DPRINTF(("%s: scan: triplet error 0x%x, "
				    "step %d\n",
				    device_xname(sc->sc_dev), rv, i));
				onewire_unlock(sc);
				return;
			}
			rom |= (mask << i);
		}
		lastd = i0;
		onewire_unlock(sc);

		if (rom == 0)
			continue;

		/*
		 * The last byte of the ROM code contains a CRC calculated
		 * from the first 7 bytes. Re-calculate it to make sure
		 * we found a valid device.
		 */
		for (i = 0; i < 8; i++)
			data[i] = (rom >> (i * 8)) & 0xff;
		if (onewire_crc(data, 7) != data[7])
			continue;

		/*
		 * Go through the list of attached devices to see if we
		 * found a new one.
		 */
		present = 0;
	 	TAILQ_FOREACH(d, &sc->sc_devs, d_list) {
			if (d->d_rom == rom) {
				d->d_present = 1;
				present = 1;
				break;
			}
		}
		if (!present) {
			memset(&oa, 0, sizeof(oa));
			oa.oa_onewire = sc;
			oa.oa_rom = rom;
			if ((dev = config_found(sc->sc_dev, &oa,
			    onewire_print)) == NULL)
				continue;

			nd = malloc(sizeof(struct onewire_device),
				M_DEVBUF, M_NOWAIT);
			if (nd == NULL)
				continue;
			nd->d_dev = dev;
			nd->d_rom = rom;
			nd->d_present = 1;
			TAILQ_INSERT_TAIL(&sc->sc_devs, nd, d_list);
		}
	}

	/* Detach disappeared devices */
	onewire_lock(sc);
	for (d = TAILQ_FIRST(&sc->sc_devs);
	    d != NULL; d = next) {
		next = TAILQ_NEXT(d, d_list);
		if (!d->d_present) {
			config_detach(d->d_dev, DETACH_FORCE);
			TAILQ_REMOVE(&sc->sc_devs, d, d_list);
			free(d, M_DEVBUF);
		}
	}
	onewire_unlock(sc);
}

MODULE(MODULE_CLASS_DRIVER, onewire, NULL);

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
onewire_modcmd(modcmd_t cmd, void *opaque)
{
	int error;

	error = 0;
	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_onewire,
		    cfattach_ioconf_onewire, cfdata_ioconf_onewire);
		if (error)
			aprint_error("%s: unable to init component\n",
			    onewire_cd.cd_name);
#endif
		break;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		config_fini_component(cfdriver_ioconf_onewire,
		    cfattach_ioconf_onewire, cfdata_ioconf_onewire);
#endif
		break;
	default:
		error = ENOTTY;
	}
	return error;
}

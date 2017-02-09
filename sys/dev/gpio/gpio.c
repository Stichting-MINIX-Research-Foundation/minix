/* $NetBSD: gpio.c,v 1.57 2014/07/25 08:10:36 dholland Exp $ */
/*	$OpenBSD: gpio.c,v 1.6 2006/01/14 12:33:49 grange Exp $	*/

/*
 * Copyright (c) 2008, 2009, 2010, 2011 Marc Balmer <marc@msys.ch>
 * Copyright (c) 2004, 2006 Alexander Yurchenko <grange@openbsd.org>
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
__KERNEL_RCSID(0, "$NetBSD: gpio.c,v 1.57 2014/07/25 08:10:36 dholland Exp $");

/*
 * General Purpose Input/Output framework.
 */

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/queue.h>
#include <sys/kauth.h>
#include <sys/module.h>
#include <dev/gpio/gpiovar.h>

#include "locators.h"

#ifdef GPIO_DEBUG
#define DPRINTFN(n, x)	do { if (gpiodebug > (n)) printf x; } while (0)
int gpiodebug = 0;
#else
#define DPRINTFN(n, x)
#endif
#define DPRINTF(x)	DPRINTFN(0, x)

struct gpio_softc {
	device_t		 sc_dev;

	gpio_chipset_tag_t	 sc_gc;		/* GPIO controller */
	gpio_pin_t		*sc_pins;	/* pins array */
	int			 sc_npins;	/* number of pins */

	kmutex_t		 sc_mtx;
	kcondvar_t		 sc_ioctl;	/* ioctl in progress */
	int			 sc_ioctl_busy;	/* ioctl is busy */
	kcondvar_t		 sc_attach;	/* attach/detach in progress */
	int			 sc_attach_busy;/* busy in attach/detach */
#ifdef COMPAT_50
	LIST_HEAD(, gpio_dev)	 sc_devs;	/* devices */
#endif
	LIST_HEAD(, gpio_name)	 sc_names;	/* named pins */
};

static int	gpio_match(device_t, cfdata_t, void *);
int		gpio_submatch(device_t, cfdata_t, const int *, void *);
static void	gpio_attach(device_t, device_t, void *);
static int	gpio_rescan(device_t, const char *, const int *);
static void	gpio_childdetached(device_t, device_t);
static bool	gpio_resume(device_t, const pmf_qual_t *);
static int	gpio_detach(device_t, int);
static int	gpio_search(device_t, cfdata_t, const int *, void *);
static int	gpio_print(void *, const char *);
static int	gpio_pinbyname(struct gpio_softc *, char *);
static int	gpio_ioctl(struct gpio_softc *, u_long, void *, int,
    struct lwp *);

#ifdef COMPAT_50
/* Old API */
static int	gpio_ioctl_oapi(struct gpio_softc *, u_long, void *, int,
    kauth_cred_t);
#endif

CFATTACH_DECL3_NEW(gpio, sizeof(struct gpio_softc),
    gpio_match, gpio_attach, gpio_detach, NULL, gpio_rescan,
    gpio_childdetached, DVF_DETACH_SHUTDOWN);

dev_type_open(gpioopen);
dev_type_close(gpioclose);
dev_type_ioctl(gpioioctl);
dev_type_ioctl(gpioioctl_locked);

const struct cdevsw gpio_cdevsw = {
	.d_open = gpioopen,
	.d_close = gpioclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = gpioioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER | D_MPSAFE
};

extern struct cfdriver gpio_cd;

static int
gpio_match(device_t parent, cfdata_t cf, void *aux)
{
	return 1;
}

int
gpio_submatch(device_t parent, cfdata_t cf, const int *ip, void *aux)
{
	struct gpio_attach_args *ga = aux;

	if (ga->ga_offset == -1)
		return 0;

	return strcmp(ga->ga_dvname, cf->cf_name) == 0;
}

static bool
gpio_resume(device_t self, const pmf_qual_t *qual)
{
	struct gpio_softc *sc = device_private(self);
	int pin;

	for (pin = 0; pin < sc->sc_npins; pin++) {
		gpiobus_pin_ctl(sc->sc_gc, pin, sc->sc_pins[pin].pin_flags);
		gpiobus_pin_write(sc->sc_gc, pin, sc->sc_pins[pin].pin_state);
	}
	return true;
}

static void
gpio_childdetached(device_t self, device_t child)
{
#ifdef COMPAT_50
	struct gpio_dev *gdev;
	struct gpio_softc *sc;
	int error;

	/*
	 * gpio_childetached is serialized because it can be entered in
	 * different ways concurrently, e.g. via the GPIODETACH ioctl and
	 * drvctl(8) or modunload(8).
	 */
	sc = device_private(self);
	error = 0;
	mutex_enter(&sc->sc_mtx);
	while (sc->sc_attach_busy) {
		error = cv_wait_sig(&sc->sc_attach, &sc->sc_mtx);
		if (error)
			break;
	}
	if (!error)
		sc->sc_attach_busy = 1;
	mutex_exit(&sc->sc_mtx);
	if (error)
		return;

	LIST_FOREACH(gdev, &sc->sc_devs, sc_next)
		if (gdev->sc_dev == child) {
			LIST_REMOVE(gdev, sc_next);
			kmem_free(gdev, sizeof(struct gpio_dev));
			break;
		}

	mutex_enter(&sc->sc_mtx);
	sc->sc_attach_busy = 0;
	cv_signal(&sc->sc_attach);
	mutex_exit(&sc->sc_mtx);
#endif
}

static int
gpio_rescan(device_t self, const char *ifattr, const int *locators)
{
	struct gpio_softc *sc = device_private(self);

	config_search_loc(gpio_search, self, ifattr, locators, sc);

	return 0;
}

static void
gpio_attach(device_t parent, device_t self, void *aux)
{
	struct gpio_softc *sc = device_private(self);
	struct gpiobus_attach_args *gba = aux;

	sc->sc_dev = self;
	sc->sc_gc = gba->gba_gc;
	sc->sc_pins = gba->gba_pins;
	sc->sc_npins = gba->gba_npins;

	aprint_normal(": %d pins\n", sc->sc_npins);
	aprint_naive("\n");

	if (!pmf_device_register(self, NULL, gpio_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");
	mutex_init(&sc->sc_mtx, MUTEX_DEFAULT, IPL_VM);
	cv_init(&sc->sc_ioctl, "gpioctl");
	cv_init(&sc->sc_attach, "gpioatch");
	/*
	 * Attach all devices that can be connected to the GPIO pins
	 * described in the kernel configuration file.
	 */
	gpio_rescan(self, "gpio", NULL);
}

static int
gpio_detach(device_t self, int flags)
{
	struct gpio_softc *sc;
	int rc;

	sc = device_private(self);

	if ((rc = config_detach_children(self, flags)) != 0)
		return rc;
	mutex_destroy(&sc->sc_mtx);
	cv_destroy(&sc->sc_ioctl);
#if 0
	int maj, mn;

	/* Locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == gpioopen)
			break;

	/* Nuke the vnodes for any open instances (calls close) */
	mn = device_unit(self);
	vdevgone(maj, mn, mn, VCHR);
#endif
	return 0;
}

static int
gpio_search(device_t parent, cfdata_t cf, const int *ldesc, void *aux)
{
	struct gpio_attach_args ga;
	size_t namlen;

	ga.ga_gpio = aux;
	ga.ga_offset = cf->cf_loc[GPIOCF_OFFSET];
	ga.ga_mask = cf->cf_loc[GPIOCF_MASK];
	ga.ga_flags = cf->cf_loc[GPIOCF_FLAG];
	namlen = strlen(cf->cf_name) + 1;
	ga.ga_dvname = kmem_alloc(namlen, KM_NOSLEEP);
	if (ga.ga_dvname == NULL)
		return 0;
	strcpy(ga.ga_dvname, cf->cf_name);

	if (config_match(parent, cf, &ga) > 0)
		config_attach(parent, cf, &ga, gpio_print);
	kmem_free(ga.ga_dvname, namlen);
	return 0;
}

int
gpio_print(void *aux, const char *pnp)
{
	struct gpio_attach_args *ga = aux;
	int i;

	aprint_normal(" pins");
	for (i = 0; i < 32; i++)
		if (ga->ga_mask & (1 << i))
			aprint_normal(" %d", ga->ga_offset + i);

	return UNCONF;
}

int
gpiobus_print(void *aux, const char *pnp)
{
#if 0
	struct gpiobus_attach_args *gba = aux;
#endif
	if (pnp != NULL)
		aprint_normal("gpiobus at %s", pnp);

	return UNCONF;
}

/* return 1 if all pins can be mapped, 0 if not */
int
gpio_pin_can_map(void *gpio, int offset, uint32_t mask)
{
	struct gpio_softc *sc = gpio;
	int npins, pin, i;

	npins = gpio_npins(mask);
	if (npins > sc->sc_npins)
		return 0;

	for (npins = 0, i = 0; i < 32; i++)
		if (mask & (1 << i)) {
			pin = offset + i;
			if (pin < 0 || pin >= sc->sc_npins)
				return 0;
			if (sc->sc_pins[pin].pin_mapped)
				return 0;
		}

	return 1;
}

int
gpio_pin_map(void *gpio, int offset, uint32_t mask, struct gpio_pinmap *map)
{
	struct gpio_softc *sc = gpio;
	int npins, pin, i;

	npins = gpio_npins(mask);
	if (npins > sc->sc_npins)
		return 1;

	for (npins = 0, i = 0; i < 32; i++)
		if (mask & (1 << i)) {
			pin = offset + i;
			if (pin < 0 || pin >= sc->sc_npins)
				return 1;
			if (sc->sc_pins[pin].pin_mapped)
				return 1;
			sc->sc_pins[pin].pin_mapped = 1;
			map->pm_map[npins++] = pin;
		}
	map->pm_size = npins;

	return 0;
}

void
gpio_pin_unmap(void *gpio, struct gpio_pinmap *map)
{
	struct gpio_softc *sc = gpio;
	int pin, i;

	for (i = 0; i < map->pm_size; i++) {
		pin = map->pm_map[i];
		sc->sc_pins[pin].pin_mapped = 0;
	}
}

int
gpio_pin_read(void *gpio, struct gpio_pinmap *map, int pin)
{
	struct gpio_softc *sc = gpio;

	return gpiobus_pin_read(sc->sc_gc, map->pm_map[pin]);
}

void
gpio_pin_write(void *gpio, struct gpio_pinmap *map, int pin, int value)
{
	struct gpio_softc *sc = gpio;

	gpiobus_pin_write(sc->sc_gc, map->pm_map[pin], value);
	sc->sc_pins[map->pm_map[pin]].pin_state = value;
}

void
gpio_pin_ctl(void *gpio, struct gpio_pinmap *map, int pin, int flags)
{
	struct gpio_softc *sc = gpio;

	return gpiobus_pin_ctl(sc->sc_gc, map->pm_map[pin], flags);
}

int
gpio_pin_caps(void *gpio, struct gpio_pinmap *map, int pin)
{
	struct gpio_softc *sc = gpio;

	return sc->sc_pins[map->pm_map[pin]].pin_caps;
}

int
gpio_npins(uint32_t mask)
{
	int npins, i;

	for (npins = 0, i = 0; i < 32; i++)
		if (mask & (1 << i))
			npins++;

	return npins;
}

int
gpio_lock(void *data)
{
	struct gpio_softc *sc;
	int error;

	error = 0;
	sc = data;
	mutex_enter(&sc->sc_mtx);
	while (sc->sc_ioctl_busy) {
		error = cv_wait_sig(&sc->sc_ioctl, &sc->sc_mtx);
		if (error)
			break;
	}
	if (!error)
		sc->sc_ioctl_busy = 1;
	mutex_exit(&sc->sc_mtx);
	return error;
}

void
gpio_unlock(void *data)
{
	struct gpio_softc *sc;

	sc = data;
	mutex_enter(&sc->sc_mtx);
	sc->sc_ioctl_busy = 0;
	cv_signal(&sc->sc_ioctl);
	mutex_exit(&sc->sc_mtx);
}

int
gpioopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct gpio_softc *sc;

	sc = device_lookup_private(&gpio_cd, minor(dev));
	if (sc == NULL)
		return ENXIO;

	return gpiobus_open(sc->sc_gc, sc->sc_dev);
}

int
gpioclose(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct gpio_softc *sc;

	sc = device_lookup_private(&gpio_cd, minor(dev));
	return gpiobus_close(sc->sc_gc, sc->sc_dev);
}

static int
gpio_pinbyname(struct gpio_softc *sc, char *gp_name)
{
        struct gpio_name *nm;

        LIST_FOREACH(nm, &sc->sc_names, gp_next)
                if (!strcmp(nm->gp_name, gp_name))
                        return nm->gp_pin;
        return -1;
}

int
gpioioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	int error;
	struct gpio_softc *sc;

	sc = device_lookup_private(&gpio_cd, minor(dev));

	error = gpio_lock(sc);
	if (error)
		return error;

	error = gpio_ioctl(sc, cmd, data, flag, l);
	gpio_unlock(sc);
	return error;
}

static int
gpio_ioctl(struct gpio_softc *sc, u_long cmd, void *data, int flag,
    struct lwp *l)
{
	gpio_chipset_tag_t gc;
	struct gpio_info *info;
	struct gpio_attach *attach;
	struct gpio_attach_args ga;
	struct gpio_req *req;
	struct gpio_name *nm;
	struct gpio_set *set;
#ifdef COMPAT_50
	struct gpio_dev *gdev;
#endif
	device_t dv;
	cfdata_t cf;
	kauth_cred_t cred;
	int locs[GPIOCF_NLOCS];
	int error, pin, value, flags, npins;

	gc = sc->sc_gc;
	ga.ga_flags = 0;

	if (cmd != GPIOINFO && !device_is_active(sc->sc_dev)) {
		DPRINTF(("%s: device is not active\n",
		    device_xname(sc->sc_dev)));
		return EBUSY;
	}

	cred = kauth_cred_get();

	switch (cmd) {
	case GPIOINFO:
		info = data;
		if (!kauth_authorize_device(cred, KAUTH_DEVICE_GPIO_PINSET,
		    NULL, NULL, NULL, NULL))
			info->gpio_npins = sc->sc_npins;
		else {
			for (pin = npins = 0; pin < sc->sc_npins; pin++)
				if (sc->sc_pins[pin].pin_flags & GPIO_PIN_SET)
					++npins;
			info->gpio_npins = npins;
		}
		break;
	case GPIOREAD:
		req = data;

		if (req->gp_name[0] != '\0')
			pin = gpio_pinbyname(sc, req->gp_name);
		else
			pin = req->gp_pin;

		if (pin < 0 || pin >= sc->sc_npins)
			return EINVAL;

		if (!(sc->sc_pins[pin].pin_flags & GPIO_PIN_SET) &&
		    kauth_authorize_device(cred, KAUTH_DEVICE_GPIO_PINSET,
		    NULL, NULL, NULL, NULL))
			return EPERM;

		/* return read value */
		req->gp_value = gpiobus_pin_read(gc, pin);
		break;
	case GPIOWRITE:
		if ((flag & FWRITE) == 0)
			return EBADF;

		req = data;

		if (req->gp_name[0] != '\0')
			pin = gpio_pinbyname(sc, req->gp_name);
		else
			pin = req->gp_pin;

		if (pin < 0 || pin >= sc->sc_npins)
			return EINVAL;

		if (sc->sc_pins[pin].pin_mapped)
			return EBUSY;

		if (!(sc->sc_pins[pin].pin_flags & GPIO_PIN_SET) &&
		    kauth_authorize_device(cred, KAUTH_DEVICE_GPIO_PINSET,
		    NULL, NULL, NULL, NULL))
			return EPERM;

		value = req->gp_value;
		if (value != GPIO_PIN_LOW && value != GPIO_PIN_HIGH)
			return EINVAL;

		/* return old value */
		req->gp_value = gpiobus_pin_read(gc, pin);
		gpiobus_pin_write(gc, pin, value);
		/* update current value */
		sc->sc_pins[pin].pin_state = value;
		break;
	case GPIOTOGGLE:
		if ((flag & FWRITE) == 0)
			return EBADF;

		req = data;

		if (req->gp_name[0] != '\0')
			pin = gpio_pinbyname(sc, req->gp_name);
		else
			pin = req->gp_pin;

		if (pin < 0 || pin >= sc->sc_npins)
			return EINVAL;

		if (sc->sc_pins[pin].pin_mapped)
			return EBUSY;

		if (!(sc->sc_pins[pin].pin_flags & GPIO_PIN_SET) &&
		    kauth_authorize_device(cred, KAUTH_DEVICE_GPIO_PINSET,
		    NULL, NULL, NULL, NULL))
			return EPERM;

		value = (sc->sc_pins[pin].pin_state == GPIO_PIN_LOW ?
		    GPIO_PIN_HIGH : GPIO_PIN_LOW);
		gpiobus_pin_write(gc, pin, value);
		/* return old value */
		req->gp_value = sc->sc_pins[pin].pin_state;
		/* update current value */
		sc->sc_pins[pin].pin_state = value;
		break;
	case GPIOATTACH:
		attach = data;
		ga.ga_flags = attach->ga_flags;
#ifdef COMPAT_50
		/* FALLTHROUGH */
	case GPIOATTACH50:
		/*
		 * The double assignment to 'attach' in case of GPIOATTACH
		 * and COMPAT_50 is on purpose. It ensures backward
		 * compatability in case we are called through the old
		 * GPIOATTACH50 ioctl(2), which had not the ga_flags field
		 * in struct gpio_attach.
		 */
		attach = data;
#endif
		if (kauth_authorize_device(cred, KAUTH_DEVICE_GPIO_PINSET,
		    NULL, NULL, NULL, NULL))
			return EPERM;

		/* do not try to attach if the pins are already mapped */
		if (!gpio_pin_can_map(sc, attach->ga_offset, attach->ga_mask))
			return EBUSY;

		error = 0;
		mutex_enter(&sc->sc_mtx);
		while (sc->sc_attach_busy) {
			error = cv_wait_sig(&sc->sc_attach, &sc->sc_mtx);
			if (error)
				break;
		}
		if (!error)
			sc->sc_attach_busy = 1;
		mutex_exit(&sc->sc_mtx);
		if (error)
			return EBUSY;

		ga.ga_gpio = sc;
		/* Don't access attach->ga_flags here. */
		ga.ga_dvname = attach->ga_dvname;
		ga.ga_offset = attach->ga_offset;
		ga.ga_mask = attach->ga_mask;
		DPRINTF(("%s: attach %s with offset %d, mask "
		    "0x%02x, and flags 0x%02x\n", device_xname(sc->sc_dev),
		    ga.ga_dvname, ga.ga_offset, ga.ga_mask, ga.ga_flags));

		locs[GPIOCF_OFFSET] = ga.ga_offset;
		locs[GPIOCF_MASK] = ga.ga_mask;
		locs[GPIOCF_FLAG] = ga.ga_flags;

		cf = config_search_loc(NULL, sc->sc_dev, "gpio", locs, &ga);
		if (cf != NULL) {
			dv = config_attach_loc(sc->sc_dev, cf, locs, &ga,
			    gpiobus_print);
#ifdef COMPAT_50
			if (dv != NULL) {
				gdev = kmem_alloc(sizeof(struct gpio_dev),
				    KM_SLEEP);
				gdev->sc_dev = dv;
				LIST_INSERT_HEAD(&sc->sc_devs, gdev, sc_next);
			} else
				error = EINVAL;
#else
			if (dv == NULL)
				error = EINVAL;
#endif
		} else
			error = EINVAL;
		mutex_enter(&sc->sc_mtx);
		sc->sc_attach_busy = 0;
		cv_signal(&sc->sc_attach);
		mutex_exit(&sc->sc_mtx);
		return error;
	case GPIOSET:
		if (kauth_authorize_device(cred, KAUTH_DEVICE_GPIO_PINSET,
		    NULL, NULL, NULL, NULL))
			return EPERM;

		set = data;

		if (set->gp_name[0] != '\0')
			pin = gpio_pinbyname(sc, set->gp_name);
		else
			pin = set->gp_pin;

		if (pin < 0 || pin >= sc->sc_npins)
			return EINVAL;
		flags = set->gp_flags;

		/* check that the controller supports all requested flags */
		if ((flags & sc->sc_pins[pin].pin_caps) != flags)
			return ENODEV;
		flags = set->gp_flags;

		set->gp_caps = sc->sc_pins[pin].pin_caps;
		/* return old value */
		set->gp_flags = sc->sc_pins[pin].pin_flags;

		if (flags > 0) {
			flags |= GPIO_PIN_SET;
			gpiobus_pin_ctl(gc, pin, flags);
			/* update current value */
			sc->sc_pins[pin].pin_flags = flags;
		}

		/* rename pin or new pin? */
		if (set->gp_name2[0] != '\0') {
			struct gpio_name *gnm;

			gnm = NULL;
			LIST_FOREACH(nm, &sc->sc_names, gp_next) {
				if (!strcmp(nm->gp_name, set->gp_name2) &&
				    nm->gp_pin != pin)
					return EINVAL;	/* duplicate name */
				if (nm->gp_pin == pin)
					gnm = nm;
			}
			if (gnm != NULL)
				strlcpy(gnm->gp_name, set->gp_name2,
				    sizeof(gnm->gp_name));
			else  {
				nm = kmem_alloc(sizeof(struct gpio_name),
				    KM_SLEEP);
				strlcpy(nm->gp_name, set->gp_name2,
				    sizeof(nm->gp_name));
				nm->gp_pin = set->gp_pin;
				LIST_INSERT_HEAD(&sc->sc_names, nm, gp_next);
			}
		}
		break;
	case GPIOUNSET:
		if (kauth_authorize_device(cred, KAUTH_DEVICE_GPIO_PINSET,
		    NULL, NULL, NULL, NULL))
			return EPERM;

		set = data;
		if (set->gp_name[0] != '\0')
			pin = gpio_pinbyname(sc, set->gp_name);
		else
			pin = set->gp_pin;

		if (pin < 0 || pin >= sc->sc_npins)
			return EINVAL;
		if (sc->sc_pins[pin].pin_mapped)
			return EBUSY;
		if (!(sc->sc_pins[pin].pin_flags & GPIO_PIN_SET))
			return EINVAL;

		LIST_FOREACH(nm, &sc->sc_names, gp_next) {
			if (nm->gp_pin == pin) {
				LIST_REMOVE(nm, gp_next);
				kmem_free(nm, sizeof(struct gpio_name));
				break;
			}
		}
		sc->sc_pins[pin].pin_flags &= ~GPIO_PIN_SET;
		break;
	default:
#ifdef COMPAT_50
		/* Try the old API */
		DPRINTF(("%s: trying the old API\n", device_xname(sc->sc_dev)));
		return gpio_ioctl_oapi(sc, cmd, data, flag, cred);
#else
		return ENOTTY;
#endif
	}
	return 0;
}

#ifdef COMPAT_50
static int
gpio_ioctl_oapi(struct gpio_softc *sc, u_long cmd, void *data, int flag,
    kauth_cred_t cred)
{
	gpio_chipset_tag_t gc;
	struct gpio_pin_op *op;
	struct gpio_pin_ctl *ctl;
	struct gpio_attach *attach;
	struct gpio_dev *gdev;

	int error, pin, value, flags;

	gc = sc->sc_gc;

	switch (cmd) {
	case GPIOPINREAD:
		op = data;

		pin = op->gp_pin;

		if (pin < 0 || pin >= sc->sc_npins)
			return EINVAL;

		if (!(sc->sc_pins[pin].pin_flags & GPIO_PIN_SET) &&
		    kauth_authorize_device(cred, KAUTH_DEVICE_GPIO_PINSET,
		    NULL, NULL, NULL, NULL))
			return EPERM;

		/* return read value */
		op->gp_value = gpiobus_pin_read(gc, pin);
		break;
	case GPIOPINWRITE:
		if ((flag & FWRITE) == 0)
			return EBADF;

		op = data;

		pin = op->gp_pin;

		if (pin < 0 || pin >= sc->sc_npins)
			return EINVAL;

		if (sc->sc_pins[pin].pin_mapped)
			return EBUSY;

		if (!(sc->sc_pins[pin].pin_flags & GPIO_PIN_SET) &&
		    kauth_authorize_device(cred, KAUTH_DEVICE_GPIO_PINSET,
		    NULL, NULL, NULL, NULL))
			return EPERM;

		value = op->gp_value;
		if (value != GPIO_PIN_LOW && value != GPIO_PIN_HIGH)
			return EINVAL;

		gpiobus_pin_write(gc, pin, value);
		/* return old value */
		op->gp_value = sc->sc_pins[pin].pin_state;
		/* update current value */
		sc->sc_pins[pin].pin_state = value;
		break;
	case GPIOPINTOGGLE:
		if ((flag & FWRITE) == 0)
			return EBADF;

		op = data;

		pin = op->gp_pin;

		if (pin < 0 || pin >= sc->sc_npins)
			return EINVAL;

		if (sc->sc_pins[pin].pin_mapped)
			return EBUSY;

		if (!(sc->sc_pins[pin].pin_flags & GPIO_PIN_SET) &&
		    kauth_authorize_device(cred, KAUTH_DEVICE_GPIO_PINSET,
		    NULL, NULL, NULL, NULL))
			return EPERM;

		value = (sc->sc_pins[pin].pin_state == GPIO_PIN_LOW ?
		    GPIO_PIN_HIGH : GPIO_PIN_LOW);
		gpiobus_pin_write(gc, pin, value);
		/* return old value */
		op->gp_value = sc->sc_pins[pin].pin_state;
		/* update current value */
		sc->sc_pins[pin].pin_state = value;
		break;
	case GPIOPINCTL:
		ctl = data;

		if (kauth_authorize_device(cred, KAUTH_DEVICE_GPIO_PINSET,
		    NULL, NULL, NULL, NULL))
			return EPERM;

		pin = ctl->gp_pin;

		if (pin < 0 || pin >= sc->sc_npins)
			return EINVAL;
		if (sc->sc_pins[pin].pin_mapped)
			return EBUSY;
		flags = ctl->gp_flags;

		/* check that the controller supports all requested flags */
		if ((flags & sc->sc_pins[pin].pin_caps) != flags)
			return ENODEV;

		ctl->gp_caps = sc->sc_pins[pin].pin_caps;
		/* return old value */
		ctl->gp_flags = sc->sc_pins[pin].pin_flags;
		if (flags > 0) {
			gpiobus_pin_ctl(gc, pin, flags);
			/* update current value */
			sc->sc_pins[pin].pin_flags = flags;
		}
		break;
	case GPIODETACH50:
		/* FALLTHOUGH */
	case GPIODETACH:
		if (kauth_authorize_device(cred, KAUTH_DEVICE_GPIO_PINSET,
		    NULL, NULL, NULL, NULL))
			return EPERM;

		error = 0;
		mutex_enter(&sc->sc_mtx);
		while (sc->sc_attach_busy) {
			error = cv_wait_sig(&sc->sc_attach, &sc->sc_mtx);
			if (error)
				break;
		}
		if (!error)
			sc->sc_attach_busy = 1;
		mutex_exit(&sc->sc_mtx);
		if (error)
			return EBUSY;

		attach = data;
		LIST_FOREACH(gdev, &sc->sc_devs, sc_next) {
			if (strcmp(device_xname(gdev->sc_dev),
			    attach->ga_dvname) == 0) {
				mutex_enter(&sc->sc_mtx);
				sc->sc_attach_busy = 0;
				cv_signal(&sc->sc_attach);
				mutex_exit(&sc->sc_mtx);

				if (config_detach(gdev->sc_dev, 0) == 0)
					return 0;
				break;
			}
		}
		if (gdev == NULL) {
			mutex_enter(&sc->sc_mtx);
			sc->sc_attach_busy = 0;
			cv_signal(&sc->sc_attach);
			mutex_exit(&sc->sc_mtx);
		}
		return EINVAL;

	default:
		return ENOTTY;
	}
	return 0;
}
#endif	/* COMPAT_50 */

MODULE(MODULE_CLASS_DRIVER, gpio, NULL);

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
gpio_modcmd(modcmd_t cmd, void *opaque)
{
#ifdef _MODULE
	devmajor_t cmajor = NODEVMAJOR, bmajor = NODEVMAJOR;
	int error;
#endif
	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_gpio,
		    cfattach_ioconf_gpio, cfdata_ioconf_gpio);
		if (error) {
			aprint_error("%s: unable to init component\n",
			    gpio_cd.cd_name);
			return error;
		}
		error = devsw_attach(gpio_cd.cd_name, NULL, &bmajor,
		    &gpio_cdevsw, &cmajor);
		if (error) {
			aprint_error("%s: unable to register devsw\n",
			    gpio_cd.cd_name);
			return config_fini_component(cfdriver_ioconf_gpio,
			    cfattach_ioconf_gpio, cfdata_ioconf_gpio);
		}
#endif
		return 0;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		config_fini_component(cfdriver_ioconf_gpio,
		    cfattach_ioconf_gpio, cfdata_ioconf_gpio);
		devsw_detach(NULL, &gpio_cdevsw);
#endif
		return 0;
	default:
		return ENOTTY;
	}
}

/* $NetBSD: gpio.h,v 1.14 2015/09/06 06:01:02 dholland Exp $ */
/*	$OpenBSD: gpio.h,v 1.7 2008/11/26 14:51:20 mbalmer Exp $	*/
/*
 * Copyright (c) 2009, 2011 Marc Balmer <marc@msys.ch>
 * Copyright (c) 2004 Alexander Yurchenko <grange@openbsd.org>
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

#ifndef _SYS_GPIO_H_
#define _SYS_GPIO_H_

#include <sys/ioccom.h>
#include <sys/time.h>

/* GPIO pin states */
#define GPIO_PIN_LOW		0x00	/* low level (logical 0) */
#define GPIO_PIN_HIGH		0x01	/* high level (logical 1) */

/* Max name length of a pin */
#define GPIOMAXNAME		64

/* GPIO pin configuration flags */
#define GPIO_PIN_INPUT		0x0001	/* input direction */
#define GPIO_PIN_OUTPUT		0x0002	/* output direction */
#define GPIO_PIN_INOUT		0x0004	/* bi-directional */
#define GPIO_PIN_OPENDRAIN	0x0008	/* open-drain output */
#define GPIO_PIN_PUSHPULL	0x0010	/* push-pull output */
#define GPIO_PIN_TRISTATE	0x0020	/* output disabled */
#define GPIO_PIN_PULLUP		0x0040	/* internal pull-up enabled */
#define GPIO_PIN_PULLDOWN	0x0080	/* internal pull-down enabled */
#define GPIO_PIN_INVIN		0x0100	/* invert input */
#define GPIO_PIN_INVOUT		0x0200	/* invert output */
#define GPIO_PIN_USER		0x0400	/* user != 0 can access */
#define GPIO_PIN_PULSATE	0x0800	/* pulsate in hardware */
#define GPIO_PIN_SET		0x8000	/* set for securelevel access */

/* GPIO controller description */
struct gpio_info {
	int gpio_npins;		/* total number of pins available */
};

/* GPIO pin request (read/write/toggle) */
struct gpio_req {
	char		gp_name[GPIOMAXNAME];	/* pin name */
	int		gp_pin;			/* pin number */
	int		gp_value;		/* value */
};

/* GPIO pin configuration */
struct gpio_set {
	char	gp_name[GPIOMAXNAME];
	int	gp_pin;
	int	gp_caps;
	int	gp_flags;
	char	gp_name2[GPIOMAXNAME];	/* new name */
};

/* Attach device drivers that use GPIO pins */
struct gpio_attach {
	char		ga_dvname[16];	/* device name */
	int		ga_offset;	/* pin number */
	uint32_t	ga_mask;	/* binary mask */
	uint32_t	ga_flags;	/* driver dependent flags */
};

/* gpio(4) API */
#define GPIOINFO		_IOR('G', 0, struct gpio_info)
#define GPIOSET			_IOWR('G', 5, struct gpio_set)
#define GPIOUNSET		_IOWR('G', 6, struct gpio_set)
#define GPIOREAD		_IOWR('G', 7, struct gpio_req)
#define GPIOWRITE		_IOWR('G', 8, struct gpio_req)
#define GPIOTOGGLE		_IOWR('G', 9, struct gpio_req)
#define GPIOATTACH		_IOWR('G', 10, struct gpio_attach)

#ifdef COMPAT_50
/* Old structure to attach/detach devices */
struct gpio_attach50 {
	char		ga_dvname[16];	/* device name */
	int		ga_offset;	/* pin number */
	uint32_t	ga_mask;	/* binary mask */
};

/* GPIO pin control (old API) */
struct gpio_pin_ctl {
	int gp_pin;		/* pin number */
	int gp_caps;		/* pin capabilities (read-only) */
	int gp_flags;		/* pin configuration flags */
};

/* GPIO pin operation (read/write/toggle) (old API) */
struct gpio_pin_op {
	int gp_pin;		/* pin number */
	int gp_value;		/* value */
};

/* the old API */
#define GPIOPINREAD		_IOWR('G', 1, struct gpio_pin_op)
#define GPIOPINWRITE		_IOWR('G', 2, struct gpio_pin_op)
#define GPIOPINTOGGLE		_IOWR('G', 3, struct gpio_pin_op)
#define GPIOPINCTL		_IOWR('G', 4, struct gpio_pin_ctl)
#define GPIOATTACH50		_IOWR('G', 10, struct gpio_attach50)
#define GPIODETACH50		_IOWR('G', 11, struct gpio_attach50)
#define GPIODETACH		_IOWR('G', 11, struct gpio_attach)
#endif	/* COMPAT_50 */

#endif	/* !_SYS_GPIO_H_ */

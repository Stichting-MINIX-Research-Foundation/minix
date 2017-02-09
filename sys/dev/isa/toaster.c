/* $NetBSD: toaster.c,v 1.13 2014/02/25 18:30:09 pooka Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jesse Off.
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
__KERNEL_RCSID(0, "$NetBSD: toaster.c,v 1.13 2014/02/25 18:30:09 pooka Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/poll.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/callout.h>
#include <sys/select.h>

#include <sys/bus.h>
#include <machine/autoconf.h>

#include <dev/isa/tsdiovar.h>
#include <dev/isa/tsdioreg.h>

struct toaster_softc {
	device_t sc_dev;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_gpioh;
	u_int32_t latch;
	u_int32_t burner;
	u_int32_t led_width[4];
	u_int32_t led_duty[4];
	u_int32_t led_width_sysctl[4];
	u_int32_t led_duty_sysctl[4];
	callout_t led_callout[4];
};

static int	toaster_match(device_t, cfdata_t, void *);
static void	toaster_attach(device_t, device_t, void *);

extern struct cfdriver toaster_cd;

CFATTACH_DECL_NEW(toaster, sizeof(struct toaster_softc),
    toaster_match, toaster_attach, NULL, NULL);

static struct toaster_softc *toaster_sc = NULL;

static int
toaster_match(device_t parent, cfdata_t match, void *aux)
{
	/* No more than one toaster per system */
	if (toaster_sc == NULL)
		return 1;
	else
		return 0;
}

#define TSDIO_GET(x)	bus_space_read_1(sc->sc_iot, sc->sc_gpioh, \
	(TSDIO_ ## x))

#define TSDIO_SET(x, y)	bus_space_write_1(sc->sc_iot, sc->sc_gpioh, \
	(TSDIO_ ## x), (y))

#define TSDIO_SETBITS(x, y)	bus_space_write_1(sc->sc_iot, sc->sc_gpioh, \
	(TSDIO_ ## x), TSDIO_GET(x) | (y))

#define TSDIO_CLEARBITS(x, y)	bus_space_write_1(sc->sc_iot, sc->sc_gpioh, \
	(TSDIO_ ## x), TSDIO_GET(x) & (~(y)))

#define LEDCALLOUT_DECL(x)	static void led ## x ## _on(void *);	\
static void led ## x ## _off(void *);					\
static void  								\
led ## x ## _on(void *arg)						\
{									\
	struct toaster_softc *sc = arg;					\
									\
	if (sc->led_duty[(x)]) {					\
		TSDIO_CLEARBITS(PBDR, (1 << (4 + (x))));		\
		callout_reset(&sc->led_callout[(x)], 			\
			sc->led_duty[(x)], led ## x ## _off, arg);	\
	} else {							\
		TSDIO_SETBITS(PBDR, (1 << (4 + (x))));			\
	}								\
}									\
									\
static void								\
led ## x ## _off(void *arg)						\
{									\
	struct toaster_softc *sc = arg;					\
	int offtime = sc->led_width[(x)] - sc->led_duty[(x)];		\
									\
	if (offtime > 0) {						\
		TSDIO_SETBITS(PBDR, (1 << (4 + (x))));			\
		callout_reset(&sc->led_callout[(x)], offtime, 		\
			led ## x ## _on, arg);				\
	}								\
}

LEDCALLOUT_DECL(0)
LEDCALLOUT_DECL(1)
LEDCALLOUT_DECL(2)
LEDCALLOUT_DECL(3)

static int
led_sysctl(SYSCTLFN_ARGS)
{
	int error, t;
	struct sysctlnode node;
	struct toaster_softc *sc = toaster_sc;

	node = *rnode;
	t = *(int*)rnode->sysctl_data;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	if (t < 0) return EINVAL;

	*(int*)rnode->sysctl_data = t;

	if (node.sysctl_num == sc->led_width_sysctl[0] ||
		node.sysctl_num == sc->led_duty_sysctl[0])
		led0_on(sc);
	if (node.sysctl_num == sc->led_width_sysctl[1] ||
		node.sysctl_num == sc->led_duty_sysctl[1])
		led1_on(sc);
	if (node.sysctl_num == sc->led_width_sysctl[2] ||
		node.sysctl_num == sc->led_duty_sysctl[2])
		led2_on(sc);
	if (node.sysctl_num == sc->led_width_sysctl[3] ||
		node.sysctl_num == sc->led_duty_sysctl[3])
		led3_on(sc);

	return (0);
}

static int
latch_sysctl(SYSCTLFN_ARGS)
{
	int error, t;
	struct sysctlnode node;
	struct toaster_softc *sc = toaster_sc;

	node = *rnode;
	t = *(int*)rnode->sysctl_data;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	if (t != 0 && t != 1) return EINVAL;

	*(int*)rnode->sysctl_data = t;

	if (t)
		TSDIO_SETBITS(PADR, 0x1);
	else
		TSDIO_CLEARBITS(PADR, 0x1);

	return (0);
}

static int
burner_sysctl(SYSCTLFN_ARGS)
{
	int error, t;
	struct sysctlnode node;
	struct toaster_softc *sc = toaster_sc;

	node = *rnode;
	t = *(int*)rnode->sysctl_data;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	if (t != 0 && t != 1) return EINVAL;

	*(int*)rnode->sysctl_data = t;

	if (t)
		TSDIO_SETBITS(PADR, 0x2);
	else
		TSDIO_CLEARBITS(PADR, 0x2);

	return (0);
}


static void
toaster_attach(device_t parent, device_t self, void *aux)
{
	struct toaster_softc *sc = device_private(self);
	struct tsdio_attach_args *taa = aux;
        const struct sysctlnode *node, *datnode;
	int i;

	sc->sc_dev = self;
	toaster_sc = sc;
	sc->sc_iot = taa->ta_iot;
	sc->sc_gpioh = taa->ta_ioh;

	TSDIO_SETBITS(DDR, 0x2);	/* Port B as outputs */
	TSDIO_SETBITS(PBDR, 0xf0);	/* Turn off LED's */

	aprint_normal(": internal toaster control outputs\n");
	aprint_normal_dev(sc->sc_dev, "using port B, bits 4-7 for front panel LEDs\n");
	aprint_normal_dev(sc->sc_dev, "using port A, bit 0 for magnetic latch\n");
	aprint_normal_dev(sc->sc_dev, "using port A, bit 1 for burner element\n");
	
	callout_init(&sc->led_callout[0], 0);
	callout_init(&sc->led_callout[1], 0);
	callout_init(&sc->led_callout[2], 0);
	callout_init(&sc->led_callout[3], 0);
	sc->led_duty[0] = sc->led_width[0] = 0;
	sc->led_duty[1] = sc->led_width[1] = 0;
	sc->led_duty[2] = sc->led_width[2] = 0;
	sc->led_duty[3] = sc->led_width[3] = 0;

	sc->burner = 0;
	sc->latch = 0;

	if (sysctl_createv(NULL, 0, NULL, &node,
        			0, CTLTYPE_NODE, device_xname(sc->sc_dev),
        			NULL,
        			NULL, 0, NULL, 0,
				CTL_HW, CTL_CREATE, CTL_EOL) != 0) {
                aprint_error_dev(sc->sc_dev, "could not create sysctl\n");
		return;
	}

#define LEDSYSCTL_SETUP(x) if ((i = sysctl_createv(NULL, 		\
				0, NULL, &datnode,			\
        			CTLFLAG_READWRITE|CTLFLAG_ANYWRITE,	\
				 CTLTYPE_INT, 				\
				"led" #x "_duty",			\
        			SYSCTL_DESCR(				\
				"LED duty cycle in HZ tick units"),	\
        			led_sysctl, 0, &sc->led_duty[(x)], 0,	\
				CTL_HW, node->sysctl_num,		\
				CTL_CREATE, CTL_EOL))			\
				!= 0) {					\
                aprint_error_dev(sc->sc_dev, "could not create sysctl\n"); 		\
		return;							\
	}								\
	sc->led_duty_sysctl[(x)] = datnode->sysctl_num;			\
									\
	if ((i = sysctl_createv(NULL, 0, NULL, &datnode,		\
        			CTLFLAG_READWRITE|CTLFLAG_ANYWRITE, 	\
				CTLTYPE_INT, 				\
				"led" #x "_width",			\
        			SYSCTL_DESCR(				\
				"LED cycle width in HZ tick units"),	\
        			led_sysctl, 0, (void *)&sc->led_width[(x)], 0,	\
				CTL_HW, node->sysctl_num,		\
				CTL_CREATE, CTL_EOL))			\
				!= 0) {					\
                aprint_error_dev(sc->sc_dev, "could not create sysctl\n"); 		\
		return;							\
	}								\
	sc->led_width_sysctl[(x)] = datnode->sysctl_num;

	LEDSYSCTL_SETUP(0);
	LEDSYSCTL_SETUP(1);
	LEDSYSCTL_SETUP(2);
	LEDSYSCTL_SETUP(3);

	if ((i = sysctl_createv(NULL, 0, NULL, &datnode,
        			CTLFLAG_READWRITE|CTLFLAG_ANYWRITE, 
				CTLTYPE_INT,
				"magnetic_latch",
        			SYSCTL_DESCR(
				"magnetic latch that holds the toast down"),
        			latch_sysctl, 0, (void *)&sc->latch, 0,
				CTL_HW, node->sysctl_num,
				CTL_CREATE, CTL_EOL))
				!= 0) {
                aprint_error_dev(sc->sc_dev, "could not create sysctl\n");
		return;
	}

	if ((i = sysctl_createv(NULL, 0, NULL, &datnode,
        			CTLFLAG_READWRITE, CTLTYPE_INT,
				"burner_element",
        			SYSCTL_DESCR(
				"800-watt burner element control for toasting"),
        			burner_sysctl, 0, (void *)&sc->burner, 0,
				CTL_HW, node->sysctl_num,
				CTL_CREATE, CTL_EOL))
				!= 0) {
                aprint_error_dev(sc->sc_dev, "could not create sysctl\n");
		return;
	}
}

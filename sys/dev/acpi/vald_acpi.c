/*	$NetBSD: vald_acpi.c,v 1.4 2010/04/15 07:02:24 jruoho Exp $ */

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Masanori Kanaoka.
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
 * Copyright 2001 Bill Sommerfeld.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * ACPI VALD Driver for Toshiba Libretto L3.
 *	This driver is based on acpibat driver.
 */

/*
 * Obtain information of Toshiba "GHCI" Method from next URL.
 *           http://www.buzzard.org.uk/toshiba/docs.html
 *           http://memebeam.org/toys/ToshibaAcpiDriver
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vald_acpi.c,v 1.4 2010/04/15 07:02:24 jruoho Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/acpi/acpica.h>
#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#define _COMPONENT          ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME            ("vald_acpi")

#define GHCI_WORDS 6
#define GHCI_FIFO_EMPTY  0x8c00
#define GHCI_NOT_SUPPORT  0x8000

#define GHCI_BACKLIGHT		0x0002
#define GHCI_ACADAPTOR		0x0003
#define GHCI_FAN		0x0004
#define GHCI_SYSTEM_EVENT_FIFO	0x0016
#define GHCI_DISPLAY_DEVICE	0x001C
#define GHCI_HOTKEY_EVENT	0x001E

#define GHCI_ON			0x0001
#define GHCI_OFF		0x0000
#define GHCI_ENABLE		0x0001
#define GHCI_DISABLE		0x0000

#define GHCI_CRT		0x0002
#define GHCI_LCD		0x0001


struct vald_acpi_softc {
	device_t sc_dev;		/* base device glue */
	struct acpi_devnode *sc_node;	/* our ACPI devnode */
	int sc_flags;			/* see below */

	ACPI_HANDLE lcd_handle;		/* lcd handle */
	int *lcd_level;			/* lcd brightness table */
	int lcd_num;			/* size of lcd brightness table */
	int lcd_index;			/* index of lcd brightness table */

	ACPI_INTEGER sc_ac_status;	/* AC adaptor status when attach */
};

static const char * const vald_acpi_hids[] = {
	"TOS6200",
	NULL
};

#define LIBRIGHT_HOLD	0x00
#define LIBRIGHT_UP	0x01
#define LIBRIGHT_DOWN	0x02

static int	vald_acpi_match(device_t, cfdata_t, void *);
static void	vald_acpi_attach(device_t, device_t, void *);

static void	vald_acpi_event(void *);
static void	vald_acpi_notify_handler(ACPI_HANDLE, uint32_t, void *);

#define ACPI_NOTIFY_ValdStatusChanged	0x80


static ACPI_STATUS	vald_acpi_ghci_get(struct vald_acpi_softc *, uint32_t,
					uint32_t *, uint32_t *);
static ACPI_STATUS	vald_acpi_ghci_set(struct vald_acpi_softc *, uint32_t,
					uint32_t, uint32_t *);

static ACPI_STATUS	vald_acpi_libright_get_bus(ACPI_HANDLE, uint32_t,
					void *, void **);
static void		vald_acpi_libright_get(struct vald_acpi_softc *);
static void		vald_acpi_libright_set(struct vald_acpi_softc *, int);

static void		vald_acpi_video_switch(struct vald_acpi_softc *);
static void		vald_acpi_fan_switch(struct vald_acpi_softc *);

static ACPI_STATUS	vald_acpi_bcm_set(ACPI_HANDLE, uint32_t);
static ACPI_STATUS	vald_acpi_dssx_set(uint32_t);

CFATTACH_DECL_NEW(vald_acpi, sizeof(struct vald_acpi_softc),
    vald_acpi_match, vald_acpi_attach, NULL, NULL);

/*
 * vald_acpi_match:
 *
 *	Autoconfiguration `match' routine.
 */
static int
vald_acpi_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return (0);

	return (acpi_match_hid(aa->aa_node->ad_devinfo, vald_acpi_hids));
}

/*
 * vald_acpi_attach:
 *
 *	Autoconfiguration `attach' routine.
 */
static void
vald_acpi_attach(device_t parent, device_t self, void *aux)
{
	struct vald_acpi_softc *sc = device_private(self);
	struct acpi_attach_args *aa = aux;
	ACPI_STATUS rv;
	uint32_t value, result;

	aprint_naive(": Toshiba VALD\n");
	aprint_normal(": Toshiba VALD\n");

	sc->sc_node = aa->aa_node;
	sc->sc_dev = self;

	/* Get AC adaptor status via _PSR. */
	rv = acpi_eval_integer(ACPI_ROOT_OBJECT, "\\_SB_.ADP1._PSR",
	    &sc->sc_ac_status);
	if (ACPI_FAILURE(rv))
		aprint_error_dev(self, "Unable to evaluate _PSR: %s\n",
		    AcpiFormatException(rv));
	else
		aprint_verbose_dev(self, "AC adaptor %sconnected\n",
		    (sc->sc_ac_status == 0 ? "not ": ""));

	/* Get LCD backlight status. */
	rv = vald_acpi_ghci_get(sc, GHCI_BACKLIGHT, &value, &result);
	if (ACPI_SUCCESS(rv)) {
		if (result != 0)
			aprint_error_dev(self, "can't get backlight status error=%d\n",
			    result);
		else
			aprint_verbose_dev(self, "LCD backlight %s\n",
			    ((value == GHCI_ON) ? "on" : "off"));
	}

	/* Enable SystemEventFIFO,HotkeyEvent */
	rv = vald_acpi_ghci_set(sc, GHCI_SYSTEM_EVENT_FIFO, GHCI_ENABLE,
	    &result);
	if (ACPI_SUCCESS(rv) && result != 0)
		aprint_error_dev(self, "can't enable SystemEventFIFO error=%d\n",
		    result);

	rv = vald_acpi_ghci_set(sc, GHCI_HOTKEY_EVENT, GHCI_ENABLE, &result);
	if (ACPI_SUCCESS(rv) && result != 0)
		aprint_error_dev(self, "can't enable HotkeyEvent error=%d\n",
		    result);

	/* Check SystemFIFO events. */
	vald_acpi_event(sc);

	/* Get LCD brightness level via _BCL. */
	vald_acpi_libright_get(sc);

	/* Set LCD brightness level via _BCM. */
	vald_acpi_libright_set(sc, LIBRIGHT_HOLD);

	/* enable vald notify */
	rv = AcpiEvaluateObject(sc->sc_node->ad_handle, "ENAB", NULL, NULL);

	if (ACPI_SUCCESS(rv))
		(void)acpi_register_notify(sc->sc_node,
		    vald_acpi_notify_handler);
}

/*
 * vald_acpi_notify_handler:
 *
 *	Notify handler.
 */
static void
vald_acpi_notify_handler(ACPI_HANDLE handle, uint32_t notify, void *context)
{
	struct vald_acpi_softc *sc;
	device_t self = context;

	sc = device_private(self);

	switch (notify) {

	case ACPI_NOTIFY_ValdStatusChanged:
		(void)AcpiOsExecute(OSL_NOTIFY_HANDLER, vald_acpi_event, sc);
		break;

	default:
		aprint_error_dev(sc->sc_dev,
		    "unknown notify 0x%02X\n", notify);
		break;
	}
}

/*
 * vald_acpi_event:
 *
 *	Check hotkey event and do it, if event occur.
 */
static void
vald_acpi_event(void *arg)
{
	struct vald_acpi_softc *sc = arg;
	ACPI_STATUS rv;
	uint32_t value, result;

	while(1) {
		rv = vald_acpi_ghci_get(sc, GHCI_SYSTEM_EVENT_FIFO, &value,
		    &result);
		if (ACPI_SUCCESS(rv) && result == 0) {

			switch (value) {
			case 0x1c3: /* Fn + F9 */
				break;
			case 0x1c2: /* Fn + F8 */
				vald_acpi_fan_switch(sc);
				break;
			case 0x1c1: /* Fn + F7 */
				vald_acpi_libright_set(sc, LIBRIGHT_UP);
				break;
			case 0x1c0: /* Fn + F6 */
				vald_acpi_libright_set(sc, LIBRIGHT_DOWN);
				break;
			case 0x1bf: /* Fn + F5 */
				vald_acpi_video_switch(sc);
				break;
			default:
				break;
			}
		}
		if (ACPI_FAILURE(rv) || result == GHCI_FIFO_EMPTY)
			break;
	}
}

/*
 * vald_acpi_ghci_get:
 *
 *	Get value via "GHCI" Method.
 */
static ACPI_STATUS
vald_acpi_ghci_get(struct vald_acpi_softc *sc,
    uint32_t reg, uint32_t *value, uint32_t *result)
{
	ACPI_STATUS rv;
	ACPI_OBJECT Arg[GHCI_WORDS];
	ACPI_OBJECT_LIST ArgList;
	ACPI_OBJECT *param, *PrtElement;
	ACPI_BUFFER buf;
	int		i;

	for (i = 0; i < GHCI_WORDS; i++) {
		Arg[i].Type = ACPI_TYPE_INTEGER;
		Arg[i].Integer.Value = 0;
	}

	Arg[0].Integer.Value = 0xfe00;
	Arg[1].Integer.Value = reg;
	Arg[2].Integer.Value = 0;

	ArgList.Count = GHCI_WORDS;
	ArgList.Pointer = Arg;

	buf.Pointer = NULL;
	buf.Length = ACPI_ALLOCATE_LOCAL_BUFFER;

	rv = AcpiEvaluateObject(sc->sc_node->ad_handle,
	    "GHCI", &ArgList, &buf);
	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(sc->sc_dev, "failed to evaluate GHCI: %s\n",
		    AcpiFormatException(rv));
		return (rv);
	}

	*result = GHCI_NOT_SUPPORT;
	*value = 0;
	param = buf.Pointer;
	if (param->Type == ACPI_TYPE_PACKAGE) {
		PrtElement = param->Package.Elements;
		if (PrtElement->Type == ACPI_TYPE_INTEGER)
			*result = PrtElement->Integer.Value;
		PrtElement++;
		PrtElement++;
		if (PrtElement->Type == ACPI_TYPE_INTEGER)
			*value = PrtElement->Integer.Value;
	}

	if (buf.Pointer)
		ACPI_FREE(buf.Pointer);
	return (rv);
}

/*
 * vald_acpi_ghci_set:
 *
 *	Set value via "GHCI" Method.
 */
static ACPI_STATUS
vald_acpi_ghci_set(struct vald_acpi_softc *sc,
    uint32_t reg, uint32_t value, uint32_t *result)
{
	ACPI_STATUS rv;
	ACPI_OBJECT Arg[GHCI_WORDS];
	ACPI_OBJECT_LIST ArgList;
	ACPI_OBJECT *param, *PrtElement;
	ACPI_BUFFER buf;
	int	i;


	for (i = 0; i < GHCI_WORDS; i++) {
		Arg[i].Type = ACPI_TYPE_INTEGER;
		Arg[i].Integer.Value = 0;
	}

	Arg[0].Integer.Value = 0xff00;
	Arg[1].Integer.Value = reg;
	Arg[2].Integer.Value = value;

	ArgList.Count = GHCI_WORDS;
	ArgList.Pointer = Arg;

	buf.Pointer = NULL;
	buf.Length = ACPI_ALLOCATE_LOCAL_BUFFER;

	rv = AcpiEvaluateObject(sc->sc_node->ad_handle,
	    "GHCI", &ArgList, &buf);
	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(sc->sc_dev, "failed to evaluate GHCI: %s\n",
		    AcpiFormatException(rv));
		return (rv);
	}

	*result = GHCI_NOT_SUPPORT;
	param = buf.Pointer;
	if (param->Type == ACPI_TYPE_PACKAGE) {
		PrtElement = param->Package.Elements;
	    	if (PrtElement->Type == ACPI_TYPE_INTEGER)
			*result = PrtElement->Integer.Value;
	}

	if (buf.Pointer)
		ACPI_FREE(buf.Pointer);
	return (rv);
}

/*
 * vald_acpi_libright_get_bus:
 *
 *	Get LCD brightness level via "_BCL" Method,
 *	and save this handle.
 */
static ACPI_STATUS
vald_acpi_libright_get_bus(ACPI_HANDLE handle, uint32_t level,
    void *context, void **status)
{
	struct vald_acpi_softc *sc = context;
	ACPI_STATUS rv;
	ACPI_BUFFER buf;
	ACPI_OBJECT *param, *PrtElement;
	int i, *pi;

	rv = acpi_eval_struct(handle, "_BCL", &buf);
	if (ACPI_FAILURE(rv))
		return (AE_OK);

	sc->lcd_handle = handle;
	param = buf.Pointer;
	if (param->Type == ACPI_TYPE_PACKAGE) {
		printf("_BCL retrun: %d packages\n", param->Package.Count);

		sc->lcd_num = param->Package.Count;
		sc->lcd_level = ACPI_ALLOCATE(sizeof(int) * sc->lcd_num);
		if (sc->lcd_level == NULL) {
			if (buf.Pointer)
				ACPI_FREE(buf.Pointer);
			return (AE_NO_MEMORY);
		}

		PrtElement = param->Package.Elements;
		pi = sc->lcd_level;
		for (i = 0; i < param->Package.Count; i++) {
			if (PrtElement->Type == ACPI_TYPE_INTEGER) {
				*pi = (unsigned)PrtElement->Integer.Value;
				PrtElement++;
				pi++;
			}
		}
		if (sc->sc_ac_status == 1) /* AC adaptor on when attach */
			sc->lcd_index = sc->lcd_num -1; /* MAX Brightness */
		else
			sc->lcd_index = 3;

#ifdef ACPI_DEBUG
		pi = sc->lcd_level;
		printf("\t Full Power Level: %d\n", *pi);
		printf("\t on Battery Level: %d\n", *(pi+1));
		printf("\t Possible Level: ");
		for (i = 2;i < sc->lcd_num; i++)
			printf(" %d", *(pi+i));
		printf("\n");
#endif
	}

	if (buf.Pointer)
		ACPI_FREE(buf.Pointer);
	return (AE_OK);
}

/*
 * vald_acpi_libright_get:
 *
 *	Search node that have "_BCL" Method.
 */
static void
vald_acpi_libright_get(struct vald_acpi_softc *sc)
{
	ACPI_HANDLE parent;
	ACPI_STATUS rv;

	aprint_verbose_dev(sc->sc_dev, "get LCD brightness via _BCL\n");

#ifdef ACPI_DEBUG
	printf("acpi_libright_get: start\n");
#endif
	rv = AcpiGetHandle(ACPI_ROOT_OBJECT, "\\_SB_", &parent);
	if (ACPI_FAILURE(rv))
		return;

	AcpiWalkNamespace(ACPI_TYPE_DEVICE, parent, 100,
	    vald_acpi_libright_get_bus, NULL, sc, NULL);
}

/*
 * vald_acpi_libright_set:
 *
 *	Figure up next status and set it.
 */
static void
vald_acpi_libright_set(struct vald_acpi_softc *sc, int UpDown)
{
	uint32_t backlight, backlight_new, result, bright;
	ACPI_STATUS rv;
	int *pi;

	/* Skip,if it does not support _BCL. */
	if (sc->lcd_handle == NULL)
		return;

	/* Get LCD backlight status. */
	rv = vald_acpi_ghci_get(sc, GHCI_BACKLIGHT, &backlight, &result);
	if (ACPI_FAILURE(rv) || result != 0)
		return;

	/* Figure up next status. */
	backlight_new = backlight;
	if (UpDown == LIBRIGHT_UP) {
		if (backlight == 1)
			sc->lcd_index++;
		else {
			/* backlight on */
			backlight_new = 1;
			sc->lcd_index = 2;
		}
	} else if (UpDown == LIBRIGHT_DOWN) {
		if ((backlight == 1) && (sc->lcd_index > 2))
			sc->lcd_index--;
		else {
			/* backlight off */
			backlight_new = 0;
			sc->lcd_index = 2;
		}
	}

	/* Check index value. */
	if (sc->lcd_index < 2)
		sc->lcd_index = 2; /* index Minium Value */
	if (sc->lcd_index >= sc->lcd_num)
		sc->lcd_index = sc->lcd_num - 1;

	/* Set LCD backlight,if status is changed. */
	if (backlight_new != backlight) {
		rv = vald_acpi_ghci_set(sc, GHCI_BACKLIGHT, backlight_new,
		    &result);
		if (ACPI_SUCCESS(rv) && result != 0)
			aprint_error_dev(sc->sc_dev, "can't set LCD backlight %s error=%x\n",
			    ((backlight_new == 1) ? "on" : "off"), result);
	}

	if (backlight_new == 1) {

		pi = sc->lcd_level;
		bright = *(pi + sc->lcd_index);

		rv = vald_acpi_bcm_set(sc->lcd_handle, bright);
		if (ACPI_FAILURE(rv))
			aprint_error_dev(sc->sc_dev, "unable to evaluate _BCM: %s\n",
			    AcpiFormatException(rv));
	} else {
		bright = 0;
	}
#ifdef ACPI_DEBUG
	printf("LCD bright");
	printf(" %s", ((UpDown == LIBRIGHT_UP) ? "up":""));
	printf("%s\n", ((UpDown == LIBRIGHT_DOWN) ? "down":""));
	printf("\t acpi_libright_set: Set brightness to %d%%\n", bright);
#endif
}

/*
 * vald_acpi_video_switch:
 *
 *	Get video status(LCD/CRT) and set new video status.
 */
static void
vald_acpi_video_switch(struct vald_acpi_softc *sc)
{
	ACPI_STATUS	rv;
	uint32_t	value, result;

	/* Get video status. */
	rv = vald_acpi_ghci_get(sc, GHCI_DISPLAY_DEVICE, &value, &result);
	if (ACPI_FAILURE(rv))
		return;
	if (result != 0) {
		aprint_error_dev(sc->sc_dev, "can't get video status  error=%x\n",
		    result);
		return;
	}

#ifdef ACPI_DEBUG
	printf("Toggle LCD/CRT\n");
	printf("\t Before switch, video status:   %s",
	    (((value & GHCI_LCD) == GHCI_LCD) ? "LCD" : ""));
	printf("%s\n", (((value & GHCI_CRT) == GHCI_CRT) ? "CRT": ""));
#endif

	/* Toggle LCD/CRT */
	if (value & GHCI_LCD) {
		value &= ~GHCI_LCD;
		value |= GHCI_CRT;
	} else if (value & GHCI_CRT){
		value &= ~GHCI_CRT;
		value |= GHCI_LCD;
	}

	rv = vald_acpi_dssx_set(value);
	if (ACPI_FAILURE(rv))
		aprint_error_dev(sc->sc_dev, "unable to evaluate DSSX: %s\n",
		    AcpiFormatException(rv));

}

/*
 * vald_acpi_bcm_set:
 *
 *	Set LCD brightness via "_BCM" Method.
 */
static ACPI_STATUS
vald_acpi_bcm_set(ACPI_HANDLE handle, uint32_t bright)
{
	ACPI_STATUS rv;
	ACPI_OBJECT Arg;
	ACPI_OBJECT_LIST ArgList;

	ArgList.Count = 1;
	ArgList.Pointer = &Arg;

	Arg.Type = ACPI_TYPE_INTEGER;
	Arg.Integer.Value = bright;

	rv = AcpiEvaluateObject(handle, "_BCM", &ArgList, NULL);
	return (rv);
}

/*
 * vald_acpi_dssx_set:
 *
 *	Set value via "\\_SB_.VALX.DSSX" Method.
 */
static ACPI_STATUS
vald_acpi_dssx_set(uint32_t value)
{
	return acpi_eval_set_integer(NULL, "\\_SB_.VALX.DSSX", value);
}

/*
 * vald_acpi_fan_switch:
 *
 *	Get FAN status and set new FAN status.
 */
static void
vald_acpi_fan_switch(struct vald_acpi_softc *sc)
{
	ACPI_STATUS rv;
	uint32_t value, result;

	/* Get FAN status */
	rv = vald_acpi_ghci_get(sc, GHCI_FAN, &value, &result);
	if (ACPI_FAILURE(rv))
		return;
	if (result != 0) {
		aprint_error_dev(sc->sc_dev, "can't get FAN status error=%d\n",
		    result);
		return;
	}

#ifdef ACPI_DEBUG
	printf("Toggle FAN on/off\n");
	printf("\t Before toggle, FAN status %s\n",
	    (value == GHCI_OFF ? "off" : "on"));
#endif

	/* Toggle FAN on/off */
	if (value == GHCI_OFF)
		value = GHCI_ON;
	else
		value = GHCI_OFF;

	/* Set FAN new status. */
	rv = vald_acpi_ghci_set(sc, GHCI_FAN, value, &result);
	if (ACPI_FAILURE(rv))
		return;
	if (result != 0) {
		aprint_error_dev(sc->sc_dev, "can't set FAN status error=%d\n",
		    result);
		return;
	}

#ifdef ACPI_DEBUG
	printf("\t After toggle, FAN status %s\n",
	    (value == GHCI_OFF ? "off" : "on"));
#endif
}

/*	$NetBSD: btmagic.c,v 1.14 2015/07/03 14:18:18 bouyer Exp $	*/

/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Iain Hibbert.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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
/*-
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Written by Iain Hibbert for Itronix Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


/*****************************************************************************
 *
 *		Apple Bluetooth Magic Mouse driver
 *
 * The Apple Magic Mouse is a HID device but it doesn't provide a proper HID
 * descriptor, and requires extra initializations to enable the proprietary
 * touch reports. We match against the vendor-id and product-id and provide
 * our own Bluetooth connection handling as the bthidev driver does not cater
 * for such complications.
 *
 * This driver interprets the touch reports only as far as emulating a
 * middle mouse button and providing horizontal and vertical scroll action.
 * Full gesture support would be more complicated and is left as an exercise
 * for the reader.
 *
 * Credit for decoding the proprietary touch reports goes to Michael Poole
 * who wrote the Linux hid-magicmouse input driver.
 *
 *****************************************************************************/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: btmagic.c,v 1.14 2015/07/03 14:18:18 bouyer Exp $");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/socketvar.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

#include <prop/proplib.h>

#include <netbt/bluetooth.h>
#include <netbt/l2cap.h>

#include <dev/bluetooth/btdev.h>
#include <dev/bluetooth/bthid.h>
#include <dev/bluetooth/bthidev.h>

#include <dev/usb/hid.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbdevs.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

#undef	DPRINTF
#ifdef	BTMAGIC_DEBUG
#define	DPRINTF(sc, ...) do {				\
	printf("%s: ", device_xname((sc)->sc_dev));	\
	printf(__VA_ARGS__);				\
	printf("\n");					\
} while (/*CONSTCOND*/0)
#else
#define	DPRINTF(...)	(void)0
#endif

struct btmagic_softc {
	bdaddr_t		sc_laddr;	/* local address */
	bdaddr_t		sc_raddr;	/* remote address */
	struct sockopt		sc_mode;	/* link mode */

	device_t		sc_dev;
	uint16_t		sc_state;
	uint16_t		sc_flags;

	callout_t		sc_timeout;

	/* control */
	struct l2cap_channel	*sc_ctl;
	struct l2cap_channel	*sc_ctl_l;

	/* interrupt */
	struct l2cap_channel	*sc_int;
	struct l2cap_channel	*sc_int_l;

	/* wsmouse child */
	device_t		sc_wsmouse;
	int			sc_enabled;

	/* config */
	int			sc_resolution;	/* for soft scaling */
	int			sc_firm;	/* firm touch threshold */
	int			sc_dist;	/* scroll distance threshold */
	int			sc_scale;	/* scroll descaling */
	struct sysctllog	*sc_log;	/* sysctl teardown log */

	/* remainders */
	int			sc_rx;
	int			sc_ry;
	int			sc_rz;
	int			sc_rw;

	/* previous touches */
	uint32_t		sc_smask;	/* active(s) IDs */
	int			sc_nfingers;	/* number of active IDs */
	int			sc_ax[16];
	int			sc_ay[16];

	/* previous mouse buttons */
	int			sc_mb_id; /* which ID selects the button */
	uint32_t		sc_mb;
	/* button emulation with tap */
	int			sc_tapmb_id; /* which ID selects the button */
	struct timeval		sc_taptime;
	int			sc_taptimeout;
	callout_t		sc_tapcallout;
};

/* sc_flags */
#define BTMAGIC_CONNECTING	__BIT(0) /* we are connecting */
#define BTMAGIC_ENABLED		__BIT(1) /* touch reports enabled */

/* sc_state */
#define BTMAGIC_CLOSED		0
#define BTMAGIC_WAIT_CTL	1
#define BTMAGIC_WAIT_INT	2
#define BTMAGIC_OPEN		3

/* autoconf(9) glue */
static int  btmagic_match(device_t, cfdata_t, void *);
static void btmagic_attach(device_t, device_t, void *);
static int  btmagic_detach(device_t, int);
static int  btmagic_listen(struct btmagic_softc *);
static int  btmagic_connect(struct btmagic_softc *);
static int  btmagic_sysctl_resolution(SYSCTLFN_PROTO);
static int  btmagic_sysctl_scale(SYSCTLFN_PROTO);
static int btmagic_tap(struct btmagic_softc *, int);
static int  btmagic_sysctl_taptimeout(SYSCTLFN_PROTO);

CFATTACH_DECL_NEW(btmagic, sizeof(struct btmagic_softc),
    btmagic_match, btmagic_attach, btmagic_detach, NULL);

/* wsmouse(4) accessops */
static int btmagic_wsmouse_enable(void *);
static int btmagic_wsmouse_ioctl(void *, unsigned long, void *, int, struct lwp *);
static void btmagic_wsmouse_disable(void *);

static const struct wsmouse_accessops btmagic_wsmouse_accessops = {
	btmagic_wsmouse_enable,
	btmagic_wsmouse_ioctl,
	btmagic_wsmouse_disable,
};

/* bluetooth(9) protocol methods for L2CAP */
static void  btmagic_connecting(void *);
static void  btmagic_ctl_connected(void *);
static void  btmagic_int_connected(void *);
static void  btmagic_ctl_disconnected(void *, int);
static void  btmagic_int_disconnected(void *, int);
static void *btmagic_ctl_newconn(void *, struct sockaddr_bt *, struct sockaddr_bt *);
static void *btmagic_int_newconn(void *, struct sockaddr_bt *, struct sockaddr_bt *);
static void  btmagic_complete(void *, int);
static void  btmagic_linkmode(void *, int);
static void  btmagic_input(void *, struct mbuf *);
static void  btmagic_input_basic(struct btmagic_softc *, uint8_t *, size_t);
static void  btmagic_input_magicm(struct btmagic_softc *, uint8_t *, size_t);
static void  btmagic_input_magict(struct btmagic_softc *, uint8_t *, size_t);
static void  btmagic_tapcallout(void *);

/* report types (data[1]) */
#define BASIC_REPORT_ID		0x10
#define TRACKPAD_REPORT_ID	0x28
#define MOUSE_REPORT_ID		0x29
#define BATT_STAT_REPORT_ID	0x30
#define BATT_STRENGHT_REPORT_ID	0x47
#define SURFACE_REPORT_ID	0x61

static const struct btproto btmagic_ctl_proto = {
	btmagic_connecting,
	btmagic_ctl_connected,
	btmagic_ctl_disconnected,
	btmagic_ctl_newconn,
	btmagic_complete,
	btmagic_linkmode,
	btmagic_input,
};

static const struct btproto btmagic_int_proto = {
	btmagic_connecting,
	btmagic_int_connected,
	btmagic_int_disconnected,
	btmagic_int_newconn,
	btmagic_complete,
	btmagic_linkmode,
	btmagic_input,
};

/* btmagic internals */
static void btmagic_timeout(void *);
static int  btmagic_ctl_send(struct btmagic_softc *, const uint8_t *, size_t);
static void btmagic_enable(struct btmagic_softc *);
static void btmagic_check_battery(struct btmagic_softc *);
static int  btmagic_scale(int, int *, int);


/*****************************************************************************
 *
 *	Magic Mouse autoconf(9) routines
 */

static int
btmagic_match(device_t self, cfdata_t cfdata, void *aux)
{
	uint16_t v, p;

	if (prop_dictionary_get_uint16(aux, BTDEVvendor, &v)
	    && prop_dictionary_get_uint16(aux, BTDEVproduct, &p)
	    && v == USB_VENDOR_APPLE
	    && (p == USB_PRODUCT_APPLE_MAGICMOUSE ||
		p == USB_PRODUCT_APPLE_MAGICTRACKPAD))
		return 2;	/* trump bthidev(4) */

	return 0;
}

static void
btmagic_attach(device_t parent, device_t self, void *aux)
{
	struct btmagic_softc *sc = device_private(self);
	struct wsmousedev_attach_args wsma;
	const struct sysctlnode *node;
	prop_object_t obj;
	int err;

	/*
	 * Init softc
	 */
	sc->sc_dev = self;
	sc->sc_state = BTMAGIC_CLOSED;
	sc->sc_mb_id = -1;
	sc->sc_tapmb_id = -1;
	callout_init(&sc->sc_timeout, 0);
	callout_setfunc(&sc->sc_timeout, btmagic_timeout, sc);
	callout_init(&sc->sc_tapcallout, 0);
	callout_setfunc(&sc->sc_tapcallout, btmagic_tapcallout, sc);
	sockopt_init(&sc->sc_mode, BTPROTO_L2CAP, SO_L2CAP_LM, 0);

	/*
	 * extract config from proplist
	 */
	obj = prop_dictionary_get(aux, BTDEVladdr);
	bdaddr_copy(&sc->sc_laddr, prop_data_data_nocopy(obj));

	obj = prop_dictionary_get(aux, BTDEVraddr);
	bdaddr_copy(&sc->sc_raddr, prop_data_data_nocopy(obj));

	obj = prop_dictionary_get(aux, BTDEVmode);
	if (prop_object_type(obj) == PROP_TYPE_STRING) {
		if (prop_string_equals_cstring(obj, BTDEVauth))
			sockopt_setint(&sc->sc_mode, L2CAP_LM_AUTH);
		else if (prop_string_equals_cstring(obj, BTDEVencrypt))
			sockopt_setint(&sc->sc_mode, L2CAP_LM_ENCRYPT);
		else if (prop_string_equals_cstring(obj, BTDEVsecure))
			sockopt_setint(&sc->sc_mode, L2CAP_LM_SECURE);
		else  {
			aprint_error(" unknown %s\n", BTDEVmode);
			return;
		}

		aprint_verbose(" %s %s", BTDEVmode,
		    prop_string_cstring_nocopy(obj));
	} else
		sockopt_setint(&sc->sc_mode, 0);

	aprint_normal(": 3 buttons, W and Z dirs\n");
	aprint_naive("\n");

	/*
	 * set defaults
	 */
	sc->sc_resolution = 650;
	sc->sc_firm = 6;
	sc->sc_dist = 130;
	sc->sc_scale = 20;
	sc->sc_taptimeout = 100;

	sysctl_createv(&sc->sc_log, 0, NULL, &node,
		0,
		CTLTYPE_NODE, device_xname(self),
		NULL,
		NULL, 0,
		NULL, 0,
		CTL_HW,
		CTL_CREATE, CTL_EOL);

	if (node != NULL) {
		sysctl_createv(&sc->sc_log, 0, NULL, NULL,
			CTLFLAG_READWRITE,
			CTLTYPE_INT, "soft_resolution",
			NULL,
			btmagic_sysctl_resolution, 0,
			(void *)sc, 0,
			CTL_HW, node->sysctl_num,
			CTL_CREATE, CTL_EOL);

		sysctl_createv(&sc->sc_log, 0, NULL, NULL,
			CTLFLAG_READWRITE,
			CTLTYPE_INT, "firm_touch_threshold",
			NULL,
			NULL, 0,
			&sc->sc_firm, sizeof(sc->sc_firm),
			CTL_HW, node->sysctl_num,
			CTL_CREATE, CTL_EOL);

		sysctl_createv(&sc->sc_log, 0, NULL, NULL,
			CTLFLAG_READWRITE,
			CTLTYPE_INT, "scroll_distance_threshold",
			NULL,
			NULL, 0,
			&sc->sc_dist, sizeof(sc->sc_dist),
			CTL_HW, node->sysctl_num,
			CTL_CREATE, CTL_EOL);

		sysctl_createv(&sc->sc_log, 0, NULL, NULL,
			CTLFLAG_READWRITE,
			CTLTYPE_INT, "scroll_downscale_factor",
			NULL,
			btmagic_sysctl_scale, 0,
			(void *)sc, 0,
			CTL_HW, node->sysctl_num,
			CTL_CREATE, CTL_EOL);
		sysctl_createv(&sc->sc_log, 0, NULL, NULL,
			CTLFLAG_READWRITE,
			CTLTYPE_INT, "taptimeout",
			"timeout for tap detection in milliseconds",
			btmagic_sysctl_taptimeout, 0,
			(void *)sc, 0,
			CTL_HW, node->sysctl_num,
			CTL_CREATE, CTL_EOL);
	}

	/*
	 * attach the wsmouse
	 */
	wsma.accessops = &btmagic_wsmouse_accessops;
	wsma.accesscookie = self;
	sc->sc_wsmouse = config_found(self, &wsma, wsmousedevprint);
	if (sc->sc_wsmouse == NULL) {
		aprint_error_dev(self, "failed to attach wsmouse\n");
		return;
	}

	pmf_device_register(self, NULL, NULL);

	/*
	 * start bluetooth connections
	 */
	mutex_enter(bt_lock);
	if ((err = btmagic_listen(sc)) != 0)
		aprint_error_dev(self, "failed to listen (%d)\n", err);
	btmagic_connect(sc);
	mutex_exit(bt_lock);
}

static int
btmagic_detach(device_t self, int flags)
{
	struct btmagic_softc *sc = device_private(self);
	int err = 0;

	mutex_enter(bt_lock);

	/* release interrupt listen */
	if (sc->sc_int_l != NULL) {
		l2cap_detach_pcb(&sc->sc_int_l);
		sc->sc_int_l = NULL;
	}

	/* release control listen */
	if (sc->sc_ctl_l != NULL) {
		l2cap_detach_pcb(&sc->sc_ctl_l);
		sc->sc_ctl_l = NULL;
	}

	/* close interrupt channel */
	if (sc->sc_int != NULL) {
		l2cap_disconnect_pcb(sc->sc_int, 0);
		l2cap_detach_pcb(&sc->sc_int);
		sc->sc_int = NULL;
	}

	/* close control channel */
	if (sc->sc_ctl != NULL) {
		l2cap_disconnect_pcb(sc->sc_ctl, 0);
		l2cap_detach_pcb(&sc->sc_ctl);
		sc->sc_ctl = NULL;
	}

	callout_halt(&sc->sc_tapcallout, bt_lock);
	callout_destroy(&sc->sc_tapcallout);
	callout_halt(&sc->sc_timeout, bt_lock);
	callout_destroy(&sc->sc_timeout);

	mutex_exit(bt_lock);

	pmf_device_deregister(self);

	sockopt_destroy(&sc->sc_mode);

	sysctl_teardown(&sc->sc_log);

	if (sc->sc_wsmouse != NULL) {
		err = config_detach(sc->sc_wsmouse, flags);
		sc->sc_wsmouse = NULL;
	}

	return err;
}

/*
 * listen for our device
 *
 * bt_lock is held
 */
static int
btmagic_listen(struct btmagic_softc *sc)
{
	struct sockaddr_bt sa;
	int err;

	memset(&sa, 0, sizeof(sa));
	sa.bt_len = sizeof(sa);
	sa.bt_family = AF_BLUETOOTH;
	bdaddr_copy(&sa.bt_bdaddr, &sc->sc_laddr);

	/*
	 * Listen on control PSM
	 */
	err = l2cap_attach_pcb(&sc->sc_ctl_l, &btmagic_ctl_proto, sc);
	if (err)
		return err;

	err = l2cap_setopt(sc->sc_ctl_l, &sc->sc_mode);
	if (err)
		return err;

	sa.bt_psm = L2CAP_PSM_HID_CNTL;
	err = l2cap_bind_pcb(sc->sc_ctl_l, &sa);
	if (err)
		return err;

	err = l2cap_listen_pcb(sc->sc_ctl_l);
	if (err)
		return err;

	/*
	 * Listen on interrupt PSM
	 */
	err = l2cap_attach_pcb(&sc->sc_int_l, &btmagic_int_proto, sc);
	if (err)
		return err;

	err = l2cap_setopt(sc->sc_int_l, &sc->sc_mode);
	if (err)
		return err;

	sa.bt_psm = L2CAP_PSM_HID_INTR;
	err = l2cap_bind_pcb(sc->sc_int_l, &sa);
	if (err)
		return err;

	err = l2cap_listen_pcb(sc->sc_int_l);
	if (err)
		return err;

	sc->sc_state = BTMAGIC_WAIT_CTL;
	return 0;
}

/*
 * start connecting to our device
 *
 * bt_lock is held
 */
static int
btmagic_connect(struct btmagic_softc *sc)
{
	struct sockaddr_bt sa;
	int err;

	memset(&sa, 0, sizeof(sa));
	sa.bt_len = sizeof(sa);
	sa.bt_family = AF_BLUETOOTH;

	err = l2cap_attach_pcb(&sc->sc_ctl, &btmagic_ctl_proto, sc);
	if (err) {
		printf("%s: l2cap_attach failed (%d)\n",
		    device_xname(sc->sc_dev), err);
		return err;
	}

	err = l2cap_setopt(sc->sc_ctl, &sc->sc_mode);
	if (err) {
		printf("%s: l2cap_setopt failed (%d)\n",
		    device_xname(sc->sc_dev), err);
		return err;
	}

	bdaddr_copy(&sa.bt_bdaddr, &sc->sc_laddr);
	err = l2cap_bind_pcb(sc->sc_ctl, &sa);
	if (err) {
		printf("%s: l2cap_bind_pcb failed (%d)\n",
		    device_xname(sc->sc_dev), err);
		return err;
	}

	sa.bt_psm = L2CAP_PSM_HID_CNTL;
	bdaddr_copy(&sa.bt_bdaddr, &sc->sc_raddr);
	err = l2cap_connect_pcb(sc->sc_ctl, &sa);
	if (err) {
		printf("%s: l2cap_connect_pcb failed (%d)\n",
		    device_xname(sc->sc_dev), err);
		return err;
	}

	SET(sc->sc_flags, BTMAGIC_CONNECTING);
	sc->sc_state = BTMAGIC_WAIT_CTL;
	return 0;
}

/* validate soft_resolution */
static int
btmagic_sysctl_resolution(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct btmagic_softc *sc;
	int t, error;

	node = *rnode;
	sc = node.sysctl_data;

	t = sc->sc_resolution;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (t < 100 || t > 4000 || (t / sc->sc_scale) == 0)
		return EINVAL;

	sc->sc_resolution = t;
	DPRINTF(sc, "sc_resolution = %u", t);
	return 0;
}

/* validate scroll_downscale_factor */
static int
btmagic_sysctl_scale(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct btmagic_softc *sc;
	int t, error;

	node = *rnode;
	sc = node.sysctl_data;

	t = sc->sc_scale;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (t < 1 || t > 40 || (sc->sc_resolution / t) == 0)
		return EINVAL;

	sc->sc_scale = t;
	DPRINTF(sc, "sc_scale = %u", t);
	return 0;
}

/* validate tap timeout */
static int
btmagic_sysctl_taptimeout(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct btmagic_softc *sc;
	int t, error;

	node = *rnode;
	sc = node.sysctl_data;

	t = sc->sc_taptimeout;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (t < max(1000 / hz, 1) || t > 999)
		return EINVAL;

	sc->sc_taptimeout = t;
	DPRINTF(sc, "taptimeout = %u", t);
	return 0;
}

/*****************************************************************************
 *
 *	wsmouse(4) accessops
 */

static int
btmagic_wsmouse_enable(void *self)
{
	struct btmagic_softc *sc = device_private(self);

	if (sc->sc_enabled)
		return EBUSY;

	sc->sc_enabled = 1;
	DPRINTF(sc, "enable");
	return 0;
}

static int
btmagic_wsmouse_ioctl(void *self, unsigned long cmd, void *data,
    int flag, struct lwp *l)
{
	/* struct btmagic_softc *sc = device_private(self); */
	int err;

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(uint *)data = WSMOUSE_TYPE_BLUETOOTH;
		err = 0;
		break;

	default:
		err = EPASSTHROUGH;
		break;
	}

	return err;
}

static void
btmagic_wsmouse_disable(void *self)
{
	struct btmagic_softc *sc = device_private(self);

	DPRINTF(sc, "disable");
	sc->sc_enabled = 0;
}


/*****************************************************************************
 *
 *	setup routines
 */

static void
btmagic_timeout(void *arg)
{
	struct btmagic_softc *sc = arg;

	mutex_enter(bt_lock);
	callout_ack(&sc->sc_timeout);

	switch (sc->sc_state) {
	case BTMAGIC_CLOSED:
		if (sc->sc_int != NULL) {
			l2cap_disconnect_pcb(sc->sc_int, 0);
			break;
		}

		if (sc->sc_ctl != NULL) {
			l2cap_disconnect_pcb(sc->sc_ctl, 0);
			break;
		}
		break;

	case BTMAGIC_OPEN:
		if (!ISSET(sc->sc_flags, BTMAGIC_ENABLED)) {
			btmagic_enable(sc);
			break;
		}

		btmagic_check_battery(sc);
		break;

	case BTMAGIC_WAIT_CTL:
	case BTMAGIC_WAIT_INT:
	default:
		break;
	}
	mutex_exit(bt_lock);
}

/*
 * Send report on control channel
 *
 * bt_lock is held
 */
static int
btmagic_ctl_send(struct btmagic_softc *sc, const uint8_t *data, size_t len)
{
	struct mbuf *m;

	if (len > MLEN)
		return EINVAL;

	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return ENOMEM;

#ifdef BTMAGIC_DEBUG
	printf("%s: send", device_xname(sc->sc_dev));
	for (size_t i = 0; i < len; i++)
		printf(" 0x%02x", data[i]);
	printf("\n");
#endif

	memcpy(mtod(m, uint8_t *), data, len);
	m->m_pkthdr.len = m->m_len = len;
	return l2cap_send_pcb(sc->sc_ctl, m);
}

/*
 * Enable touch reports by sending the following report
 *
 *	 SET_REPORT(FEATURE, 0xd7) = 0x01
 *
 * bt_lock is held
 */
static void
btmagic_enable(struct btmagic_softc *sc)
{
	static const uint8_t rep[] = { 0x53, 0xd7, 0x01 };

	if (btmagic_ctl_send(sc, rep, sizeof(rep)) != 0) {
		printf("%s: cannot enable touch reports\n",
		    device_xname(sc->sc_dev));

		return;
	}

	SET(sc->sc_flags, BTMAGIC_ENABLED);
}

/*
 * Request the battery level by sending the following report
 *
 *	GET_REPORT(FEATURE, 0x47)
 *
 * bt_lock is held
 */
static void
btmagic_check_battery(struct btmagic_softc *sc)
{
	static const uint8_t rep[] = { 0x43, 0x47 };

	if (btmagic_ctl_send(sc, rep, sizeof(rep)) != 0)
		printf("%s: cannot request battery level\n",
		    device_xname(sc->sc_dev));
}

/*
 * the Magic Mouse has a base resolution of 1300dpi which is rather flighty. We
 * scale the output to the requested resolution, taking care to account for the
 * remainders to prevent loss of small deltas.
 */
static int
btmagic_scale(int delta, int *remainder, int resolution)
{
	int new;

	delta += *remainder;
	new = delta * resolution / 1300;
	*remainder = delta - new * 1300 / resolution;
	return new;
}


/*****************************************************************************
 *
 *	bluetooth(9) callback methods for L2CAP
 *
 *	All these are called from Bluetooth Protocol code, holding bt_lock.
 */

static void
btmagic_connecting(void *arg)
{

	/* dont care */
}

static void
btmagic_ctl_connected(void *arg)
{
	struct sockaddr_bt sa;
	struct btmagic_softc *sc = arg;
	int err;

	if (sc->sc_state != BTMAGIC_WAIT_CTL)
		return;

	KASSERT(sc->sc_ctl != NULL);
	KASSERT(sc->sc_int == NULL);

	if (ISSET(sc->sc_flags, BTMAGIC_CONNECTING)) {
		/* initiate connect on interrupt PSM */
		err = l2cap_attach_pcb(&sc->sc_int, &btmagic_int_proto, sc);
		if (err)
			goto fail;

		err = l2cap_setopt(sc->sc_int, &sc->sc_mode);
		if (err)
			goto fail;

		memset(&sa, 0, sizeof(sa));
		sa.bt_len = sizeof(sa);
		sa.bt_family = AF_BLUETOOTH;
		bdaddr_copy(&sa.bt_bdaddr, &sc->sc_laddr);

		err = l2cap_bind_pcb(sc->sc_int, &sa);
		if (err)
			goto fail;

		sa.bt_psm = L2CAP_PSM_HID_INTR;
		bdaddr_copy(&sa.bt_bdaddr, &sc->sc_raddr);
		err = l2cap_connect_pcb(sc->sc_int, &sa);
		if (err)
			goto fail;
	}

	sc->sc_state = BTMAGIC_WAIT_INT;
	return;

fail:
	l2cap_detach_pcb(&sc->sc_ctl);
	sc->sc_ctl = NULL;

	printf("%s: connect failed (%d)\n", device_xname(sc->sc_dev), err);
}

static void
btmagic_int_connected(void *arg)
{
	struct btmagic_softc *sc = arg;

	if (sc->sc_state != BTMAGIC_WAIT_INT)
		return;

	KASSERT(sc->sc_ctl != NULL);
	KASSERT(sc->sc_int != NULL);

	printf("%s: connected\n", device_xname(sc->sc_dev));
	CLR(sc->sc_flags, BTMAGIC_CONNECTING);
	sc->sc_state = BTMAGIC_OPEN;

	/* trigger the setup */
	CLR(sc->sc_flags, BTMAGIC_ENABLED);
	callout_schedule(&sc->sc_timeout, hz);
}

/*
 * Disconnected
 *
 * Depending on our state, this could mean several things, but essentially
 * we are lost. If both channels are closed, schedule another connection.
 */
static void
btmagic_ctl_disconnected(void *arg, int err)
{
	struct btmagic_softc *sc = arg;

	if (sc->sc_ctl != NULL) {
		l2cap_detach_pcb(&sc->sc_ctl);
		sc->sc_ctl = NULL;
	}

	if (sc->sc_int == NULL) {
		printf("%s: disconnected (%d)\n", device_xname(sc->sc_dev), err);
		CLR(sc->sc_flags, BTMAGIC_CONNECTING);
		sc->sc_state = BTMAGIC_WAIT_CTL;
	} else {
		/*
		 * The interrupt channel should have been closed first,
		 * but its potentially unsafe to detach that from here.
		 * Give them a second to do the right thing or let the
		 * callout handle it.
		 */
		sc->sc_state = BTMAGIC_CLOSED;
		callout_schedule(&sc->sc_timeout, hz);
	}
}

static void
btmagic_int_disconnected(void *arg, int err)
{
	struct btmagic_softc *sc = arg;

	if (sc->sc_int != NULL) {
		l2cap_detach_pcb(&sc->sc_int);
		sc->sc_int = NULL;
	}

	if (sc->sc_ctl == NULL) {
		printf("%s: disconnected (%d)\n", device_xname(sc->sc_dev), err);
		CLR(sc->sc_flags, BTMAGIC_CONNECTING);
		sc->sc_state = BTMAGIC_WAIT_CTL;
	} else {
		/*
		 * The control channel should be closing also, allow
		 * them a chance to do that before we force it.
		 */
		sc->sc_state = BTMAGIC_CLOSED;
		callout_schedule(&sc->sc_timeout, hz);
	}
}

/*
 * New Connections
 *
 * We give a new L2CAP handle back if this matches the BDADDR we are
 * listening for and we are in the right state. btmagic_connected will
 * be called when the connection is open, so nothing else to do here
 */
static void *
btmagic_ctl_newconn(void *arg, struct sockaddr_bt *laddr,
    struct sockaddr_bt *raddr)
{
	struct btmagic_softc *sc = arg;

	if (bdaddr_same(&raddr->bt_bdaddr, &sc->sc_raddr) == 0)
		return NULL;

	if (sc->sc_state != BTMAGIC_WAIT_CTL
	    || ISSET(sc->sc_flags, BTMAGIC_CONNECTING)
	    || sc->sc_ctl != NULL
	    || sc->sc_int != NULL) {
		DPRINTF(sc, "reject ctl newconn %s%s%s%s",
		    (sc->sc_state == BTMAGIC_WAIT_CTL) ? " (WAITING)": "",
		    ISSET(sc->sc_flags, BTMAGIC_CONNECTING) ? " (CONNECTING)" : "",
		    (sc->sc_ctl != NULL) ? " (GOT CONTROL)" : "",
		    (sc->sc_int != NULL) ? " (GOT INTERRUPT)" : "");

		return NULL;
	}

	l2cap_attach_pcb(&sc->sc_ctl, &btmagic_ctl_proto, sc);
	return sc->sc_ctl;
}

static void *
btmagic_int_newconn(void *arg, struct sockaddr_bt *laddr,
    struct sockaddr_bt *raddr)
{
	struct btmagic_softc *sc = arg;

	if (bdaddr_same(&raddr->bt_bdaddr, &sc->sc_raddr) == 0)
		return NULL;

	if (sc->sc_state != BTMAGIC_WAIT_INT
	    || ISSET(sc->sc_flags, BTMAGIC_CONNECTING)
	    || sc->sc_ctl == NULL
	    || sc->sc_int != NULL) {
		DPRINTF(sc, "reject int newconn %s%s%s%s",
		    (sc->sc_state == BTMAGIC_WAIT_INT) ? " (WAITING)": "",
		    ISSET(sc->sc_flags, BTMAGIC_CONNECTING) ? " (CONNECTING)" : "",
		    (sc->sc_ctl == NULL) ? " (NO CONTROL)" : "",
		    (sc->sc_int != NULL) ? " (GOT INTERRUPT)" : "");

		return NULL;
	}

	l2cap_attach_pcb(&sc->sc_int, &btmagic_int_proto, sc);
	return sc->sc_int;
}

static void
btmagic_complete(void *arg, int count)
{

	/* dont care */
}

static void
btmagic_linkmode(void *arg, int new)
{
	struct btmagic_softc *sc = arg;
	int mode;

	(void)sockopt_getint(&sc->sc_mode, &mode);

	if (ISSET(mode, L2CAP_LM_AUTH) && !ISSET(new, L2CAP_LM_AUTH))
		printf("%s: auth failed\n", device_xname(sc->sc_dev));
	else if (ISSET(mode, L2CAP_LM_ENCRYPT) && !ISSET(new, L2CAP_LM_ENCRYPT))
		printf("%s: encrypt off\n", device_xname(sc->sc_dev));
	else if (ISSET(mode, L2CAP_LM_SECURE) && !ISSET(new, L2CAP_LM_SECURE))
		printf("%s: insecure\n", device_xname(sc->sc_dev));
	else
		return;

	if (sc->sc_int != NULL)
		l2cap_disconnect_pcb(sc->sc_int, 0);

	if (sc->sc_ctl != NULL)
		l2cap_disconnect_pcb(sc->sc_ctl, 0);
}

/*
 * Receive transaction from the mouse. We don't differentiate between
 * interrupt and control channel here, there is no need.
 */
static void
btmagic_input(void *arg, struct mbuf *m)
{
	struct btmagic_softc *sc = arg;
	uint8_t *data;
	size_t len;

	if (sc->sc_state != BTMAGIC_OPEN
	    || sc->sc_wsmouse == NULL
	    || sc->sc_enabled == 0)
		goto release;

	if (m->m_pkthdr.len > m->m_len)
		printf("%s: truncating input\n", device_xname(sc->sc_dev));

	data = mtod(m, uint8_t *);
	len = m->m_len;

	if (len < 1)
		goto release;

	switch (BTHID_TYPE(data[0])) {
	case BTHID_HANDSHAKE:
		DPRINTF(sc, "Handshake: 0x%x", BTHID_HANDSHAKE_PARAM(data[0]));
		callout_schedule(&sc->sc_timeout, hz);
		break;

	case BTHID_DATA:
		if (len < 2)
			break;

		switch (data[1]) {
		case BASIC_REPORT_ID: /* Basic mouse (input) */
			btmagic_input_basic(sc, data + 2, len - 2);
			break;

		case TRACKPAD_REPORT_ID: /* Magic trackpad (input) */
			btmagic_input_magict(sc, data + 2, len - 2);
			break;
		case MOUSE_REPORT_ID: /* Magic touch (input) */
			btmagic_input_magicm(sc, data + 2, len - 2);
			break;

		case BATT_STAT_REPORT_ID: /* Battery status (input) */
			if (len != 3)
				break;

			printf("%s: Battery ", device_xname(sc->sc_dev));
			switch (data[2]) {
			case 0:	printf("Ok\n");			break;
			case 1:	printf("Warning\n");		break;
			case 2:	printf("Critical\n");		break;
			default: printf("0x%02x\n", data[2]);	break;
			}
			break;

		case BATT_STRENGHT_REPORT_ID: /* Battery strength (feature) */
			if (len != 3)
				break;

			printf("%s: Battery %d%%\n", device_xname(sc->sc_dev),
			    data[2]);
			break;

		case SURFACE_REPORT_ID: /* Surface detection (input) */
			if (len != 3)
				break;

			DPRINTF(sc, "Mouse %s",
			    (data[2] == 0 ? "lowered" : "raised"));
			break;

		case 0x60: /* unknown (input) */
		case 0xf0: /* unknown (feature) */
		case 0xf1: /* unknown (feature) */
		default:
#if BTMAGIC_DEBUG
			printf("%s: recv", device_xname(sc->sc_dev));
			for (size_t i = 0; i < len; i++)
				printf(" 0x%02x", data[i]);
			printf("\n");
#endif
			break;
		}
		break;

	default:
		DPRINTF(sc, "transaction (type 0x%x)", BTHID_TYPE(data[0]));
		break;
	}

release:
	m_freem(m);
}

/*
 * parse the Basic report (0x10), which according to the provided
 * HID descriptor is in the following format
 *
 *	button 1	1-bit
 *	button 2	1-bit
 *	padding		6-bits
 *	dX		16-bits (signed)
 *	dY		16-bits (signed)
 *
 * Even when the magic touch reports are enabled, the basic report is
 * sent for mouse move events where no touches are detected.
 */
static const struct {
	struct hid_location button1;
	struct hid_location button2;
	struct hid_location dX;
	struct hid_location dY;
} basic = {
	.button1 = { .pos =  0, .size = 1 },
	.button2 = { .pos =  1, .size = 1 },
	.dX = { .pos =  8, .size = 16 },
	.dY = { .pos = 24, .size = 16 },
};

static void
btmagic_input_basic(struct btmagic_softc *sc, uint8_t *data, size_t len)
{
	int dx, dy;
	uint32_t mb;
	int s;

	if (len != 5)
		return;

	dx = hid_get_data(data, &basic.dX);
	dx = btmagic_scale(dx, &sc->sc_rx, sc->sc_resolution);

	dy = hid_get_data(data, &basic.dY);
	dy = btmagic_scale(dy, &sc->sc_ry, sc->sc_resolution);

	mb = 0;
	if (hid_get_udata(data, &basic.button1))
		mb |= __BIT(0);
	if (hid_get_udata(data, &basic.button2))
		mb |= __BIT(2);

	if (dx != 0 || dy != 0 || mb != sc->sc_mb) {
		sc->sc_mb = mb;

		s = spltty();
		wsmouse_input(sc->sc_wsmouse, mb,
		    dx, -dy, 0, 0, WSMOUSE_INPUT_DELTA);
		splx(s);
	}
}

/*
 * the Magic touch report (0x29), according to the Linux driver
 * written by Michael Poole, is variable length starting with the
 * fixed 40-bit header
 *
 *	dX lsb		8-bits (signed)
 *	dY lsb		8-bits (signed)
 *	button 1	1-bit
 *	button 2	1-bit
 *	dX msb		2-bits (signed)
 *	dY msb		2-bits (signed)
 *	timestamp	18-bits
 *
 * followed by (up to 5?) touch reports of 64-bits each
 *
 *	abs W		12-bits (signed)
 *	abs Z		12-bits (signed)
 *	axis major	8-bits
 *	axis minor	8-bits
 *	pressure	6-bits
 *	id		4-bits
 *	angle		6-bits	(from E(0)->N(32)->W(64))
 *	unknown		4-bits
 *	phase		4-bits
 */

static const struct {
	struct hid_location dXl;
	struct hid_location dYl;
	struct hid_location button1;
	struct hid_location button2;
	struct hid_location dXm;
	struct hid_location dYm;
	struct hid_location timestamp;
} magic = {
	.dXl = { .pos = 0, .size = 8 },
	.dYl = { .pos = 8, .size = 8 },
	.button1 = { .pos = 16, .size = 1 },
	.button2 = { .pos = 17, .size = 1 },
	.dXm = { .pos = 18, .size = 2 },
	.dYm = { .pos = 20, .size = 2 },
	.timestamp = { .pos = 22, .size = 18 },
};

static const struct {
	struct hid_location aW;
	struct hid_location aZ;
	struct hid_location major;
	struct hid_location minor;
	struct hid_location pressure;
	struct hid_location id;
	struct hid_location angle;
	struct hid_location unknown;
	struct hid_location phase;
} touch = {
	.aW = { .pos = 0, .size = 12 },
	.aZ = { .pos = 12, .size = 12 },
	.major = { .pos = 24, .size = 8 },
	.minor = { .pos = 32, .size = 8 },
	.pressure = { .pos = 40, .size = 6 },
	.id = { .pos = 46, .size = 4 },
	.angle = { .pos = 50, .size = 6 },
	.unknown = { .pos = 56, .size = 4 },
	.phase = { .pos = 60, .size = 4 },
};

/*
 * the phase of the touch starts at 0x01 as the finger is first detected
 * approaching the mouse, increasing to 0x04 while the finger is touching,
 * then increases towards 0x07 as the finger is lifted, and we get 0x00
 * when the touch is cancelled. The values below seem to be produced for
 * every touch, the others less consistently depending on how fast the
 * approach or departure is.
 *
 * In fact we ignore touches unless they are in the steady 0x04 phase.
 */
#define BTMAGIC_PHASE_START	0x3
#define BTMAGIC_PHASE_CONT	0x4
#define BTMAGIC_PHASE_END	0x7
#define BTMAGIC_PHASE_CANCEL	0x0

static void
btmagic_input_magicm(struct btmagic_softc *sc, uint8_t *data, size_t len)
{
	uint32_t mb;
	int dx, dy, dz, dw;
	int id, nf, az, aw, tz, tw;
	int s;

	if (((len - 5) % 8) != 0)
		return;

	dx = (hid_get_data(data, &magic.dXm) << 8)
	    | (hid_get_data(data, &magic.dXl) & 0xff);
	dx = btmagic_scale(dx, &sc->sc_rx, sc->sc_resolution);

	dy = (hid_get_data(data, &magic.dYm) << 8)
	    | (hid_get_data(data, &magic.dYl) & 0xff);
	dy = btmagic_scale(dy, &sc->sc_ry, sc->sc_resolution);

	mb = 0;
	if (hid_get_udata(data, &magic.button1))
		mb |= __BIT(0);
	if (hid_get_udata(data, &magic.button2))
		mb |= __BIT(2);

	nf = 0;
	dz = 0;
	dw = 0;
	len = (len - 5) / 8;
	for (data += 5; len-- > 0; data += 8) {
		id = hid_get_udata(data, &touch.id);
		az = hid_get_data(data, &touch.aZ);
		aw = hid_get_data(data, &touch.aW);

		/*
		 * scrolling is triggered by an established touch moving
		 * beyond a minimum distance from its start point and is
		 * cancelled as the touch starts to fade.
		 *
		 * Multiple touches may be scrolling simultaneously, the
		 * effect is cumulative.
		 */

		switch (hid_get_udata(data, &touch.phase)) {
		case BTMAGIC_PHASE_CONT:
#define sc_az sc_ay
#define sc_aw sc_ax
			tz = az - sc->sc_az[id];
			tw = aw - sc->sc_aw[id];

			if (ISSET(sc->sc_smask, __BIT(id))) {
				/* scrolling finger */
				dz += btmagic_scale(tz, &sc->sc_rz,
				    sc->sc_resolution / sc->sc_scale);
				dw += btmagic_scale(tw, &sc->sc_rw,
				    sc->sc_resolution / sc->sc_scale);
			} else if (abs(tz) > sc->sc_dist
			    || abs(tw) > sc->sc_dist) {
				/* new scrolling finger */
				if (sc->sc_smask == 0) {
					sc->sc_rz = 0;
					sc->sc_rw = 0;
				}

				SET(sc->sc_smask, __BIT(id));
			} else {
				/* not scrolling finger */
				az = sc->sc_az[id];
				aw = sc->sc_aw[id];
			}

			/* count firm touches for middle-click */
			if (hid_get_udata(data, &touch.pressure) > sc->sc_firm)
				nf++;

			break;

		default:
			CLR(sc->sc_smask, __BIT(id));
			break;
		}

		sc->sc_az[id] = az;
		sc->sc_aw[id] = aw;
#undef sc_az
#undef sc_aw
	}

	/*
	 * The mouse only has one click detector, and says left or right but
	 * never both. We convert multiple firm touches while clicking into
	 * a middle button press, and cancel any scroll effects while click
	 * is active.
	 */
	if (mb != 0) {
		if (sc->sc_mb != 0)
			mb = sc->sc_mb;
		else if (nf > 1)
			mb = __BIT(1);

		sc->sc_smask = 0;
		dz = 0;
		dw = 0;
	}

	if (dx != 0 || dy != 0 || dz != 0 || dw != 0 || mb != sc->sc_mb) {
		sc->sc_mb = mb;

		s = spltty();
		wsmouse_input(sc->sc_wsmouse, mb,
		    dx, -dy, -dz, dw, WSMOUSE_INPUT_DELTA);
		splx(s);
	}
}

/*
 * the Magic touch trackpad report (0x28), according to the Linux driver
 * written by Michael Poole and Chase Douglas, is variable length starting
 * with the fixed 24-bit header
 *
 *	button 1	1-bit
 *      unknown		5-bits
 *	timestamp	18-bits
 *
 * followed by (up to 5?) touch reports of 72-bits each
 *
 *	abs X		13-bits (signed)
 *	abs Y		13-bits (signed)
 * 	unknown		6-bits
 *	axis major	8-bits
 *	axis minor	8-bits
 *	pressure	6-bits
 *	id		4-bits
 *	angle		6-bits	(from E(0)->N(32)->W(64))
 *	unknown		4-bits
 *	phase		4-bits
 */

static const struct {
	struct hid_location button;
	struct hid_location timestamp;
} magict = {
	.button = { .pos =  0, .size = 1 },
	.timestamp = { .pos = 6, .size = 18 },
};

static const struct {
	struct hid_location aX;
	struct hid_location aY;
	struct hid_location major;
	struct hid_location minor;
	struct hid_location pressure;
	struct hid_location id;
	struct hid_location angle;
	struct hid_location unknown;
	struct hid_location phase;
} toucht = {
	.aX = { .pos = 0, .size = 13 },
	.aY = { .pos = 13, .size = 13 },
	.major = { .pos = 32, .size = 8 },
	.minor = { .pos = 40, .size = 8 },
	.pressure = { .pos = 48, .size = 6 },
	.id = { .pos = 54, .size = 4 },
	.angle = { .pos = 58, .size = 6 },
	.unknown = { .pos = 64, .size = 4 },
	.phase = { .pos = 68, .size = 4 },
};

/*
 * as for btmagic_input_magicm, 
 * the phase of the touch starts at 0x01 as the finger is first detected
 * approaching the mouse, increasing to 0x04 while the finger is touching,
 * then increases towards 0x07 as the finger is lifted, and we get 0x00
 * when the touch is cancelled. The values below seem to be produced for
 * every touch, the others less consistently depending on how fast the
 * approach or departure is.
 *
 * In fact we ignore touches unless they are in the steady 0x04 phase.
 */

/* min and max values reported */
#define MAGICT_X_MIN	(-2910)
#define MAGICT_X_MAX	(3170)
#define MAGICT_Y_MIN	(-2565)
#define MAGICT_Y_MAX	(2455)

/*
 * area for detecting the buttons: divide in 3 areas on X, 
 * below -1900 on y
 */
#define MAGICT_B_YMAX	(-1900)
#define MAGICT_B_XSIZE	((MAGICT_X_MAX - MAGICT_X_MIN) / 3)
#define MAGICT_B_X1MAX	(MAGICT_X_MIN + MAGICT_B_XSIZE)
#define MAGICT_B_X2MAX	(MAGICT_X_MIN + MAGICT_B_XSIZE * 2)

static void
btmagic_input_magict(struct btmagic_softc *sc, uint8_t *data, size_t len)
{
	bool bpress;
	uint32_t mb;
	int id, ax, ay, tx, ty;
	int dx, dy, dz, dw;
	int s;

	if (((len - 3) % 9) != 0)
		return;

	bpress = 0;
	if (hid_get_udata(data, &magict.button))
		bpress = 1;

	dx = dy = dz = dw = 0;
	mb = 0;

	len = (len - 3) / 9;
	for (data += 3; len-- > 0; data += 9) {
		id = hid_get_udata(data, &toucht.id);
		ax = hid_get_data(data, &toucht.aX);
		ay = hid_get_data(data, &toucht.aY);

		DPRINTF(sc,
		    "btmagic_input_magicm: id %d ax %d ay %d phase %ld %s\n",
		    id, ax, ay, hid_get_udata(data, &toucht.phase),
		    bpress ? "button pressed" : "");

		/*
		 * a single touch is interpreted as a mouse move.
		 * If a button is pressed, the touch in the button area
		 * defined above defines the button; a second touch is
		 * interpreted as a mouse move.
		 */

		switch (hid_get_udata(data, &toucht.phase)) {
		case BTMAGIC_PHASE_CONT:
			if (bpress) {
				if (sc->sc_mb == 0 && ay < MAGICT_B_YMAX) {
					/*
					 * we have a new button press,
					 * and this id tells which one
					 */
					if (ax < MAGICT_B_X1MAX)
						mb = __BIT(0);
					else if (ax > MAGICT_B_X2MAX)
						mb = __BIT(2);
					else
						mb = __BIT(1);
					sc->sc_mb_id = id;
				} else {
					/* keep previous state */
					mb = sc->sc_mb;
				}
			} else {
				/* no button pressed */
				mb = 0;
				sc->sc_mb_id = -1;
			}
			if (id == sc->sc_mb_id) {
				/*
				 * this id selects the button
				 * ignore for move/scroll
				 */
				 continue;
			}
			if (id >= __arraycount(sc->sc_ax))
				continue;
					
			tx = ax - sc->sc_ax[id];
			ty = ay - sc->sc_ay[id];

			if (ISSET(sc->sc_smask, __BIT(id))) {
				struct timeval now_tv;
				getmicrotime(&now_tv);
				if (sc->sc_nfingers == 1 && mb == 0 &&
				    timercmp(&sc->sc_taptime, &now_tv, >)) {
					/* still detecting a tap */
					continue;
				}

				if (sc->sc_nfingers == 1 || mb != 0) {
					/* single finger moving */
					dx += btmagic_scale(tx, &sc->sc_rx,
					    sc->sc_resolution);
					dy += btmagic_scale(ty, &sc->sc_ry,
					    sc->sc_resolution);
				} else {
					/* scrolling fingers */
					dz += btmagic_scale(ty, &sc->sc_rz,
					    sc->sc_resolution / sc->sc_scale);
					dw += btmagic_scale(tx, &sc->sc_rw,
					    sc->sc_resolution / sc->sc_scale);
				}
			} else if (ay > MAGICT_B_YMAX) { /* new finger */
				sc->sc_rx = 0;
				sc->sc_ry = 0;
				sc->sc_rz = 0;
				sc->sc_rw = 0;
				KASSERT(!ISSET(sc->sc_smask, __BIT(id)));
				SET(sc->sc_smask, __BIT(id));
				sc->sc_nfingers++;
				if (sc->sc_tapmb_id == -1 &&
				    mb == 0 && sc->sc_mb == 0) {
					sc->sc_tapmb_id = id;
					getmicrotime(&sc->sc_taptime);
					sc->sc_taptime.tv_usec +=
					    sc->sc_taptimeout * 1000;
					if (sc->sc_taptime.tv_usec > 1000000) {
						sc->sc_taptime.tv_usec -=
						    1000000;
						sc->sc_taptime.tv_sec++;
					}
				}
					
			}

			break;
		default:
			if (ISSET(sc->sc_smask, __BIT(id))) {
				CLR(sc->sc_smask, __BIT(id));
				sc->sc_nfingers--;
				KASSERT(sc->sc_nfingers >= 0);
				if (id == sc->sc_tapmb_id) {
					mb = btmagic_tap(sc, id);
				}
			}
			break;
		}

		if (id >= __arraycount(sc->sc_ax))
			continue;

		sc->sc_ax[id] = ax;
		sc->sc_ay[id] = ay;
	}

	if (dx != 0 || dy != 0 || dz != 0 || dw != 0 || mb != sc->sc_mb) {
		sc->sc_mb = mb;

		s = spltty();
		wsmouse_input(sc->sc_wsmouse, mb,
		    dx, dy, -dz, dw, WSMOUSE_INPUT_DELTA);
		splx(s);
	}
}

static int
btmagic_tap(struct btmagic_softc *sc, int id)
{
	struct timeval now_tv;

	sc->sc_tapmb_id = -1;
	getmicrotime(&now_tv);
	if (timercmp(&sc->sc_taptime, &now_tv, >)) {
		/* got a tap */
		callout_schedule(
		    &sc->sc_tapcallout,
		    mstohz(sc->sc_taptimeout));
		return __BIT(0);
	}
	return 0;
}

static void
btmagic_tapcallout(void *arg)
{
	struct btmagic_softc *sc = arg;
	int s;

	mutex_enter(bt_lock);
	callout_ack(&sc->sc_tapcallout);
	if ((sc->sc_mb & __BIT(0)) != 0) {
		sc->sc_mb &= ~__BIT(0);
		s = spltty();
		wsmouse_input(sc->sc_wsmouse, sc->sc_mb,
		    0, 0, 0, 0, WSMOUSE_INPUT_DELTA);
		splx(s);
	}
	mutex_exit(bt_lock);
}

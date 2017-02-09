/*	$NetBSD: bthidev.c,v 1.29 2014/08/05 07:55:31 rtr Exp $	*/

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: bthidev.c,v 1.29 2014/08/05 07:55:31 rtr Exp $");

#include <sys/param.h>
#include <sys/condvar.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/socketvar.h>
#include <sys/systm.h>

#include <prop/proplib.h>

#include <netbt/bluetooth.h>
#include <netbt/l2cap.h>

#include <dev/usb/hid.h>
#include <dev/bluetooth/btdev.h>
#include <dev/bluetooth/bthid.h>
#include <dev/bluetooth/bthidev.h>

#include "locators.h"

/*****************************************************************************
 *
 *	Bluetooth HID device
 */

#define MAX_DESCRIPTOR_LEN	1024		/* sanity check */

/* bthidev softc */
struct bthidev_softc {
	uint16_t		sc_state;
	uint16_t		sc_flags;
	device_t		sc_dev;

	bdaddr_t		sc_laddr;	/* local address */
	bdaddr_t		sc_raddr;	/* remote address */
	struct sockopt		sc_mode;	/* link mode sockopt */

	uint16_t		sc_ctlpsm;	/* control PSM */
	struct l2cap_channel	*sc_ctl;	/* control channel */
	struct l2cap_channel	*sc_ctl_l;	/* control listen */

	uint16_t		sc_intpsm;	/* interrupt PSM */
	struct l2cap_channel	*sc_int;	/* interrupt channel */
	struct l2cap_channel	*sc_int_l;	/* interrupt listen */

	MBUFQ_HEAD()		sc_inq;		/* input queue */
	kmutex_t		sc_lock;	/* input queue lock */
	kcondvar_t		sc_cv;		/* input queue trigger */
	lwp_t			*sc_lwp;	/* input queue processor */
	int			sc_detach;

	LIST_HEAD(,bthidev)	sc_list;	/* child list */

	callout_t		sc_reconnect;
	int			sc_attempts;	/* connection attempts */
};

/* sc_flags */
#define BTHID_RECONNECT		(1 << 0)	/* reconnect on link loss */
#define BTHID_CONNECTING	(1 << 1)	/* we are connecting */

/* device state */
#define BTHID_CLOSED		0
#define BTHID_WAIT_CTL		1
#define BTHID_WAIT_INT		2
#define BTHID_OPEN		3

#define	BTHID_RETRY_INTERVAL	5	/* seconds between connection attempts */

/* bthidev internals */
static void bthidev_timeout(void *);
static int  bthidev_listen(struct bthidev_softc *);
static int  bthidev_connect(struct bthidev_softc *);
static int  bthidev_output(struct bthidev *, uint8_t *, int);
static void bthidev_null(struct bthidev *, uint8_t *, int);
static void bthidev_process(void *);
static void bthidev_process_one(struct bthidev_softc *, struct mbuf *);

/* autoconf(9) glue */
static int  bthidev_match(device_t, cfdata_t, void *);
static void bthidev_attach(device_t, device_t, void *);
static int  bthidev_detach(device_t, int);
static int  bthidev_print(void *, const char *);

CFATTACH_DECL_NEW(bthidev, sizeof(struct bthidev_softc),
    bthidev_match, bthidev_attach, bthidev_detach, NULL);

/* bluetooth(9) protocol methods for L2CAP */
static void  bthidev_connecting(void *);
static void  bthidev_ctl_connected(void *);
static void  bthidev_int_connected(void *);
static void  bthidev_ctl_disconnected(void *, int);
static void  bthidev_int_disconnected(void *, int);
static void *bthidev_ctl_newconn(void *, struct sockaddr_bt *, struct sockaddr_bt *);
static void *bthidev_int_newconn(void *, struct sockaddr_bt *, struct sockaddr_bt *);
static void  bthidev_complete(void *, int);
static void  bthidev_linkmode(void *, int);
static void  bthidev_input(void *, struct mbuf *);

static const struct btproto bthidev_ctl_proto = {
	bthidev_connecting,
	bthidev_ctl_connected,
	bthidev_ctl_disconnected,
	bthidev_ctl_newconn,
	bthidev_complete,
	bthidev_linkmode,
	bthidev_input,
};

static const struct btproto bthidev_int_proto = {
	bthidev_connecting,
	bthidev_int_connected,
	bthidev_int_disconnected,
	bthidev_int_newconn,
	bthidev_complete,
	bthidev_linkmode,
	bthidev_input,
};

/*****************************************************************************
 *
 *	bthidev autoconf(9) routines
 */

static int
bthidev_match(device_t self, cfdata_t cfdata, void *aux)
{
	prop_dictionary_t dict = aux;
	prop_object_t obj;

	obj = prop_dictionary_get(dict, BTDEVservice);
	if (prop_string_equals_cstring(obj, "HID"))
		return 1;

	return 0;
}

static void
bthidev_attach(device_t parent, device_t self, void *aux)
{
	struct bthidev_softc *sc = device_private(self);
	prop_dictionary_t dict = aux;
	prop_object_t obj;
	device_t dev;
	struct bthidev_attach_args bha;
	struct bthidev *hidev;
	struct hid_data *d;
	struct hid_item h;
	const void *desc;
	int locs[BTHIDBUSCF_NLOCS];
	int maxid, rep, dlen;
	int vendor, product;
	int err;

	/*
	 * Init softc
	 */
	sc->sc_dev = self;
	LIST_INIT(&sc->sc_list);
	MBUFQ_INIT(&sc->sc_inq);
	callout_init(&sc->sc_reconnect, 0);
	callout_setfunc(&sc->sc_reconnect, bthidev_timeout, sc);
	sc->sc_state = BTHID_CLOSED;
	sc->sc_flags = BTHID_CONNECTING;
	sc->sc_ctlpsm = L2CAP_PSM_HID_CNTL;
	sc->sc_intpsm = L2CAP_PSM_HID_INTR;

	sockopt_init(&sc->sc_mode, BTPROTO_L2CAP, SO_L2CAP_LM, 0);
	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&sc->sc_cv, device_xname(self));

	/*
	 * extract config from proplist
	 */
	obj = prop_dictionary_get(dict, BTDEVladdr);
	bdaddr_copy(&sc->sc_laddr, prop_data_data_nocopy(obj));

	obj = prop_dictionary_get(dict, BTDEVraddr);
	bdaddr_copy(&sc->sc_raddr, prop_data_data_nocopy(obj));

	obj = prop_dictionary_get(dict, BTDEVvendor);
	vendor = (int)prop_number_integer_value(obj);

	obj = prop_dictionary_get(dict, BTDEVproduct);
	product = (int)prop_number_integer_value(obj);

	obj = prop_dictionary_get(dict, BTDEVmode);
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

	obj = prop_dictionary_get(dict, BTHIDEVcontrolpsm);
	if (prop_object_type(obj) == PROP_TYPE_NUMBER) {
		sc->sc_ctlpsm = prop_number_integer_value(obj);
		if (L2CAP_PSM_INVALID(sc->sc_ctlpsm)) {
			aprint_error(" invalid %s\n", BTHIDEVcontrolpsm);
			return;
		}
	}

	obj = prop_dictionary_get(dict, BTHIDEVinterruptpsm);
	if (prop_object_type(obj) == PROP_TYPE_NUMBER) {
		sc->sc_intpsm = prop_number_integer_value(obj);
		if (L2CAP_PSM_INVALID(sc->sc_intpsm)) {
			aprint_error(" invalid %s\n", BTHIDEVinterruptpsm);
			return;
		}
	}

	obj = prop_dictionary_get(dict, BTHIDEVdescriptor);
	if (prop_object_type(obj) == PROP_TYPE_DATA) {
		dlen = prop_data_size(obj);
		desc = prop_data_data_nocopy(obj);
	} else {
		aprint_error(" no %s\n", BTHIDEVdescriptor);
		return;
	}

	obj = prop_dictionary_get(dict, BTHIDEVreconnect);
	if (prop_object_type(obj) == PROP_TYPE_BOOL
	    && !prop_bool_true(obj))
		sc->sc_flags |= BTHID_RECONNECT;

	/*
	 * Parse the descriptor and attach child devices, one per report.
	 */
	maxid = -1;
	h.report_ID = 0;
	d = hid_start_parse(desc, dlen, hid_none);
	while (hid_get_item(d, &h)) {
		if (h.report_ID > maxid)
			maxid = h.report_ID;
	}
	hid_end_parse(d);

	if (maxid < 0) {
		aprint_error(" no reports found\n");
		return;
	}

	aprint_normal("\n");

	if (kthread_create(PRI_NONE, KTHREAD_MUSTJOIN, NULL, bthidev_process,
	    sc, &sc->sc_lwp, "%s", device_xname(self)) != 0) {
		aprint_error_dev(self, "failed to create input thread\n");
		return;
	}

	for (rep = 0 ; rep <= maxid ; rep++) {
		if (hid_report_size(desc, dlen, hid_feature, rep) == 0
		    && hid_report_size(desc, dlen, hid_input, rep) == 0
		    && hid_report_size(desc, dlen, hid_output, rep) == 0)
			continue;

		bha.ba_vendor = vendor;
		bha.ba_product = product;
		bha.ba_desc = desc;
		bha.ba_dlen = dlen;
		bha.ba_input = bthidev_null;
		bha.ba_feature = bthidev_null;
		bha.ba_output = bthidev_output;
		bha.ba_id = rep;

		locs[BTHIDBUSCF_REPORTID] = rep;

		dev = config_found_sm_loc(self, "bthidbus",
					locs, &bha, bthidev_print, config_stdsubmatch);
		if (dev != NULL) {
			hidev = device_private(dev);
			hidev->sc_dev = dev;
			hidev->sc_parent = self;
			hidev->sc_id = rep;
			hidev->sc_input = bha.ba_input;
			hidev->sc_feature = bha.ba_feature;
			LIST_INSERT_HEAD(&sc->sc_list, hidev, sc_next);
		}
	}

	pmf_device_register(self, NULL, NULL);

	/*
	 * start bluetooth connections
	 */
	mutex_enter(bt_lock);
	if ((sc->sc_flags & BTHID_RECONNECT) == 0
	    && (err = bthidev_listen(sc)) != 0)
		aprint_error_dev(self, "failed to listen (%d)\n", err);

	if (sc->sc_flags & BTHID_CONNECTING)
		bthidev_connect(sc);
	mutex_exit(bt_lock);
}

static int
bthidev_detach(device_t self, int flags)
{
	struct bthidev_softc *sc = device_private(self);
	struct bthidev *hidev;

	mutex_enter(bt_lock);
	sc->sc_flags = 0;	/* disable reconnecting */

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

	callout_halt(&sc->sc_reconnect, bt_lock);
	callout_destroy(&sc->sc_reconnect);

	mutex_exit(bt_lock);

	pmf_device_deregister(self);

	/* kill off the input processor */
	if (sc->sc_lwp != NULL) {
		mutex_enter(&sc->sc_lock);
		sc->sc_detach = 1;
		cv_signal(&sc->sc_cv);
		mutex_exit(&sc->sc_lock);
		kthread_join(sc->sc_lwp);
		sc->sc_lwp = NULL;
	}

	/* detach children */
	while ((hidev = LIST_FIRST(&sc->sc_list)) != NULL) {
		LIST_REMOVE(hidev, sc_next);
		config_detach(hidev->sc_dev, flags);
	}

	MBUFQ_DRAIN(&sc->sc_inq);
	cv_destroy(&sc->sc_cv);
	mutex_destroy(&sc->sc_lock);
	sockopt_destroy(&sc->sc_mode);

	return 0;
}

/*
 * bthidev config print
 */
static int
bthidev_print(void *aux, const char *pnp)
{
	struct bthidev_attach_args *ba = aux;

	if (pnp != NULL)
		aprint_normal("%s:", pnp);

	if (ba->ba_id > 0)
		aprint_normal(" reportid %d", ba->ba_id);

	return UNCONF;
}

/*****************************************************************************
 *
 *	bluetooth(4) HID attach/detach routines
 */

/*
 * callouts are scheduled after connections have been lost, in order
 * to clean up and reconnect.
 */
static void
bthidev_timeout(void *arg)
{
	struct bthidev_softc *sc = arg;

	mutex_enter(bt_lock);
	callout_ack(&sc->sc_reconnect);

	switch (sc->sc_state) {
	case BTHID_CLOSED:
		if (sc->sc_int != NULL) {
			l2cap_disconnect_pcb(sc->sc_int, 0);
			break;
		}

		if (sc->sc_ctl != NULL) {
			l2cap_disconnect_pcb(sc->sc_ctl, 0);
			break;
		}

		if (sc->sc_flags & BTHID_RECONNECT) {
			sc->sc_flags |= BTHID_CONNECTING;
			bthidev_connect(sc);
			break;
		}

		break;

	case BTHID_WAIT_CTL:
		break;

	case BTHID_WAIT_INT:
		break;

	case BTHID_OPEN:
		break;

	default:
		break;
	}
	mutex_exit(bt_lock);
}

/*
 * listen for our device
 */
static int
bthidev_listen(struct bthidev_softc *sc)
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
	err = l2cap_attach_pcb(&sc->sc_ctl_l, &bthidev_ctl_proto, sc);
	if (err)
		return err;

	err = l2cap_setopt(sc->sc_ctl_l, &sc->sc_mode);
	if (err)
		return err;

	sa.bt_psm = sc->sc_ctlpsm;
	err = l2cap_bind_pcb(sc->sc_ctl_l, &sa);
	if (err)
		return err;

	err = l2cap_listen_pcb(sc->sc_ctl_l);
	if (err)
		return err;

	/*
	 * Listen on interrupt PSM
	 */
	err = l2cap_attach_pcb(&sc->sc_int_l, &bthidev_int_proto, sc);
	if (err)
		return err;

	err = l2cap_setopt(sc->sc_int_l, &sc->sc_mode);
	if (err)
		return err;

	sa.bt_psm = sc->sc_intpsm;
	err = l2cap_bind_pcb(sc->sc_int_l, &sa);
	if (err)
		return err;

	err = l2cap_listen_pcb(sc->sc_int_l);
	if (err)
		return err;

	sc->sc_state = BTHID_WAIT_CTL;
	return 0;
}

/*
 * start connecting to our device
 */
static int
bthidev_connect(struct bthidev_softc *sc)
{
	struct sockaddr_bt sa;
	int err;

	if (sc->sc_attempts++ > 0)
		aprint_verbose_dev(sc->sc_dev, "connect (#%d)\n", sc->sc_attempts);

	memset(&sa, 0, sizeof(sa));
	sa.bt_len = sizeof(sa);
	sa.bt_family = AF_BLUETOOTH;

	err = l2cap_attach_pcb(&sc->sc_ctl, &bthidev_ctl_proto, sc);
	if (err) {
		aprint_error_dev(sc->sc_dev, "l2cap_attach failed (%d)\n", err);
		return err;
	}

	err = l2cap_setopt(sc->sc_ctl, &sc->sc_mode);
	if (err) {
		aprint_error_dev(sc->sc_dev, "l2cap_setopt failed (%d)\n", err);
		return err;
	}

	bdaddr_copy(&sa.bt_bdaddr, &sc->sc_laddr);
	err = l2cap_bind_pcb(sc->sc_ctl, &sa);
	if (err) {
		aprint_error_dev(sc->sc_dev, "l2cap_bind_pcb failed (%d)\n", err);
		return err;
	}

	sa.bt_psm = sc->sc_ctlpsm;
	bdaddr_copy(&sa.bt_bdaddr, &sc->sc_raddr);
	err = l2cap_connect_pcb(sc->sc_ctl, &sa);
	if (err) {
		aprint_error_dev(sc->sc_dev, "l2cap_connect_pcb failed (%d)\n", err);
		return err;
	}

	sc->sc_state = BTHID_WAIT_CTL;
	return 0;
}

/*
 * The LWP which processes input reports, forwarding to child devices.
 * We are always either processing input reports, holding the lock, or
 * waiting for a signal on condvar.
 */
static void
bthidev_process(void *arg)
{
	struct bthidev_softc *sc = arg;
	struct mbuf *m;

	mutex_enter(&sc->sc_lock);
	while (sc->sc_detach == 0) {
		MBUFQ_DEQUEUE(&sc->sc_inq, m);
		if (m == NULL) {
			cv_wait(&sc->sc_cv, &sc->sc_lock);
			continue;
		}

		mutex_exit(&sc->sc_lock);
		bthidev_process_one(sc, m);
		m_freem(m);
		mutex_enter(&sc->sc_lock);
	}
	mutex_exit(&sc->sc_lock);
	kthread_exit(0);
}

static void
bthidev_process_one(struct bthidev_softc *sc, struct mbuf *m)
{
	struct bthidev *hidev;
	uint8_t *data;
	int len;

	if (sc->sc_state != BTHID_OPEN)
		return;

	if (m->m_pkthdr.len > m->m_len)
		aprint_error_dev(sc->sc_dev, "truncating HID report\n");

	len = m->m_len;
	data = mtod(m, uint8_t *);

	switch (BTHID_TYPE(data[0])) {
	case BTHID_DATA:
		/*
		 * data[0] == type / parameter
		 * data[1] == id
		 * data[2..len] == report
		 */
		if (len < 3)
			break;

		LIST_FOREACH(hidev, &sc->sc_list, sc_next)
			if (data[1] == hidev->sc_id)
				break;

		if (hidev == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "report id %d, len = %d ignored\n", data[1], len - 2);

			break;
		}

		switch (BTHID_DATA_PARAM(data[0])) {
		case BTHID_DATA_INPUT:
			(*hidev->sc_input)(hidev, data + 2, len - 2);
			break;

		case BTHID_DATA_FEATURE:
			(*hidev->sc_feature)(hidev, data + 2, len - 2);
			break;

		default:
			break;
		}

		break;

	case BTHID_CONTROL:
		if (len < 1)
			break;

		switch (BTHID_DATA_PARAM(data[0])) {
		case BTHID_CONTROL_UNPLUG:
			aprint_normal_dev(sc->sc_dev, "unplugged\n");

			mutex_enter(bt_lock);
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
			mutex_exit(bt_lock);

			break;

		default:
			break;
		}

		break;

	default:
		break;
	}
}

/*****************************************************************************
 *
 *	bluetooth(9) callback methods for L2CAP
 *
 *	All these are called from Bluetooth Protocol code, in a soft
 *	interrupt context at IPL_SOFTNET.
 */

static void
bthidev_connecting(void *arg)
{

	/* dont care */
}

static void
bthidev_ctl_connected(void *arg)
{
	struct sockaddr_bt sa;
	struct bthidev_softc *sc = arg;
	int err;

	if (sc->sc_state != BTHID_WAIT_CTL)
		return;

	KASSERT(sc->sc_ctl != NULL);
	KASSERT(sc->sc_int == NULL);

	if (sc->sc_flags & BTHID_CONNECTING) {
		/* initiate connect on interrupt PSM */
		err = l2cap_attach_pcb(&sc->sc_int, &bthidev_int_proto, sc);
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

		sa.bt_psm = sc->sc_intpsm;
		bdaddr_copy(&sa.bt_bdaddr, &sc->sc_raddr);
		err = l2cap_connect_pcb(sc->sc_int, &sa);
		if (err)
			goto fail;
	}

	sc->sc_state = BTHID_WAIT_INT;
	return;

fail:
	l2cap_detach_pcb(&sc->sc_ctl);
	sc->sc_ctl = NULL;

	aprint_error_dev(sc->sc_dev, "connect failed (%d)\n", err);
}

static void
bthidev_int_connected(void *arg)
{
	struct bthidev_softc *sc = arg;

	if (sc->sc_state != BTHID_WAIT_INT)
		return;

	KASSERT(sc->sc_ctl != NULL);
	KASSERT(sc->sc_int != NULL);

	sc->sc_attempts = 0;
	sc->sc_flags &= ~BTHID_CONNECTING;
	sc->sc_state = BTHID_OPEN;

	aprint_normal_dev(sc->sc_dev, "connected\n");
}

/*
 * Disconnected
 *
 * Depending on our state, this could mean several things, but essentially
 * we are lost. If both channels are closed, and we are marked to reconnect,
 * schedule another try otherwise just give up. They will contact us.
 */
static void
bthidev_ctl_disconnected(void *arg, int err)
{
	struct bthidev_softc *sc = arg;

	if (sc->sc_ctl != NULL) {
		l2cap_detach_pcb(&sc->sc_ctl);
		sc->sc_ctl = NULL;
	}

	sc->sc_state = BTHID_CLOSED;

	if (sc->sc_int == NULL) {
		aprint_normal_dev(sc->sc_dev, "disconnected (%d)\n", err);
		sc->sc_flags &= ~BTHID_CONNECTING;

		if (sc->sc_flags & BTHID_RECONNECT)
			callout_schedule(&sc->sc_reconnect,
					BTHID_RETRY_INTERVAL * hz);
		else
			sc->sc_state = BTHID_WAIT_CTL;
	} else {
		/*
		 * The interrupt channel should have been closed first,
		 * but its potentially unsafe to detach that from here.
		 * Give them a second to do the right thing or let the
		 * callout handle it.
		 */
		callout_schedule(&sc->sc_reconnect, hz);
	}
}

static void
bthidev_int_disconnected(void *arg, int err)
{
	struct bthidev_softc *sc = arg;

	if (sc->sc_int != NULL) {
		l2cap_detach_pcb(&sc->sc_int);
		sc->sc_int = NULL;
	}

	sc->sc_state = BTHID_CLOSED;

	if (sc->sc_ctl == NULL) {
		aprint_normal_dev(sc->sc_dev, "disconnected (%d)\n", err);
		sc->sc_flags &= ~BTHID_CONNECTING;

		if (sc->sc_flags & BTHID_RECONNECT)
			callout_schedule(&sc->sc_reconnect,
					BTHID_RETRY_INTERVAL * hz);
		else
			sc->sc_state = BTHID_WAIT_CTL;
	} else {
		/*
		 * The control channel should be closing also, allow
		 * them a chance to do that before we force it.
		 */
		callout_schedule(&sc->sc_reconnect, hz);
	}
}

/*
 * New Connections
 *
 * We give a new L2CAP handle back if this matches the BDADDR we are
 * listening for and we are in the right state. bthidev_connected will
 * be called when the connection is open, so nothing else to do here
 */
static void *
bthidev_ctl_newconn(void *arg, struct sockaddr_bt *laddr,
    struct sockaddr_bt *raddr)
{
	struct bthidev_softc *sc = arg;

	if (bdaddr_same(&raddr->bt_bdaddr, &sc->sc_raddr) == 0)
		return NULL;

	if ((sc->sc_flags & BTHID_CONNECTING)
	    || sc->sc_state != BTHID_WAIT_CTL
	    || sc->sc_ctl != NULL
	    || sc->sc_int != NULL) {
		aprint_verbose_dev(sc->sc_dev, "reject ctl newconn %s%s%s%s\n",
		    (sc->sc_flags & BTHID_CONNECTING) ? " (CONNECTING)" : "",
		    (sc->sc_state == BTHID_WAIT_CTL) ? " (WAITING)": "",
		    (sc->sc_ctl != NULL) ? " (GOT CONTROL)" : "",
		    (sc->sc_int != NULL) ? " (GOT INTERRUPT)" : "");

		return NULL;
	}

	l2cap_attach_pcb(&sc->sc_ctl, &bthidev_ctl_proto, sc);
	return sc->sc_ctl;
}

static void *
bthidev_int_newconn(void *arg, struct sockaddr_bt *laddr,
    struct sockaddr_bt *raddr)
{
	struct bthidev_softc *sc = arg;

	if (bdaddr_same(&raddr->bt_bdaddr, &sc->sc_raddr) == 0)
		return NULL;

	if ((sc->sc_flags & BTHID_CONNECTING)
	    || sc->sc_state != BTHID_WAIT_INT
	    || sc->sc_ctl == NULL
	    || sc->sc_int != NULL) {
		aprint_verbose_dev(sc->sc_dev, "reject int newconn %s%s%s%s\n",
		    (sc->sc_flags & BTHID_CONNECTING) ? " (CONNECTING)" : "",
		    (sc->sc_state == BTHID_WAIT_INT) ? " (WAITING)": "",
		    (sc->sc_ctl == NULL) ? " (NO CONTROL)" : "",
		    (sc->sc_int != NULL) ? " (GOT INTERRUPT)" : "");

		return NULL;
	}

	l2cap_attach_pcb(&sc->sc_int, &bthidev_int_proto, sc);
	return sc->sc_int;
}

static void
bthidev_complete(void *arg, int count)
{

	/* dont care */
}

static void
bthidev_linkmode(void *arg, int new)
{
	struct bthidev_softc *sc = arg;
	int mode;

	(void)sockopt_getint(&sc->sc_mode, &mode);

	if ((mode & L2CAP_LM_AUTH) && !(new & L2CAP_LM_AUTH))
		aprint_error_dev(sc->sc_dev, "auth failed\n");
	else if ((mode & L2CAP_LM_ENCRYPT) && !(new & L2CAP_LM_ENCRYPT))
		aprint_error_dev(sc->sc_dev, "encrypt off\n");
	else if ((mode & L2CAP_LM_SECURE) && !(new & L2CAP_LM_SECURE))
		aprint_error_dev(sc->sc_dev, "insecure\n");
	else
		return;

	if (sc->sc_int != NULL)
		l2cap_disconnect_pcb(sc->sc_int, 0);

	if (sc->sc_ctl != NULL)
		l2cap_disconnect_pcb(sc->sc_ctl, 0);
}

/*
 * Receive reports from the protocol stack. Because this will be called
 * with bt_lock held, we queue the mbuf and process it with a kernel thread
 */
static void
bthidev_input(void *arg, struct mbuf *m)
{
	struct bthidev_softc *sc = arg;

	if (sc->sc_state != BTHID_OPEN) {
		m_freem(m);
		return;
	}

	mutex_enter(&sc->sc_lock);
	MBUFQ_ENQUEUE(&sc->sc_inq, m);
	cv_signal(&sc->sc_cv);
	mutex_exit(&sc->sc_lock);
}

/*****************************************************************************
 *
 *	IO routines
 */

static void
bthidev_null(struct bthidev *hidev, uint8_t *report, int len)
{

	/*
	 * empty routine just in case the device
	 * provided no method to handle this report
	 */
}

static int
bthidev_output(struct bthidev *hidev, uint8_t *report, int rlen)
{
	struct bthidev_softc *sc = device_private(hidev->sc_parent);
	struct mbuf *m;
	int err;

	if (sc == NULL || sc->sc_state != BTHID_OPEN)
		return ENOTCONN;

	KASSERT(sc->sc_ctl != NULL);
	KASSERT(sc->sc_int != NULL);

	if (rlen == 0 || report == NULL)
		return 0;

	if (rlen > MHLEN - 2) {
		aprint_error_dev(sc->sc_dev,
		    "output report too long (%d)!\n", rlen);
		return EMSGSIZE;
	}

	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return ENOMEM;

	/*
	 * data[0] = type / parameter
	 * data[1] = id
	 * data[2..N] = report
	 */
	mtod(m, uint8_t *)[0] = (uint8_t)((BTHID_DATA << 4) | BTHID_DATA_OUTPUT);
	mtod(m, uint8_t *)[1] = hidev->sc_id;
	memcpy(mtod(m, uint8_t *) + 2, report, rlen);
	m->m_pkthdr.len = m->m_len = rlen + 2;

	mutex_enter(bt_lock);
	err = l2cap_send_pcb(sc->sc_int, m);
	mutex_exit(bt_lock);

	return err;
}

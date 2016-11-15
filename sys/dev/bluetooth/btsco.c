/*	$NetBSD: btsco.c,v 1.34 2015/07/10 22:03:12 nat Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: btsco.c,v 1.34 2015/07/10 22:03:12 nat Exp $");

#include <sys/param.h>
#include <sys/audioio.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/socketvar.h>
#include <sys/systm.h>
#include <sys/intr.h>

#include <prop/proplib.h>

#include <netbt/bluetooth.h>
#include <netbt/rfcomm.h>
#include <netbt/sco.h>

#include <dev/audio_if.h>
#include <dev/auconv.h>
#include <dev/mulaw.h>

#include <dev/bluetooth/btdev.h>
#include <dev/bluetooth/btsco.h>

#undef DPRINTF
#undef DPRINTFN

#ifdef BTSCO_DEBUG
int btsco_debug = BTSCO_DEBUG;
#define DPRINTF(...)		do {		\
	if (btsco_debug) {			\
		printf("%s: ", __func__);	\
		printf(__VA_ARGS__);		\
	}					\
} while (/* CONSTCOND */0)

#define DPRINTFN(n, ...)	do {		\
	if (btsco_debug > (n)) {		\
		printf("%s: ", __func__);	\
		printf(__VA_ARGS__);		\
	}					\
} while (/* CONSTCOND */0)
#else
#define DPRINTF(...)
#define DPRINTFN(...)
#endif

/*****************************************************************************
 *
 *	Bluetooth SCO Audio device
 */

/* btsco softc */
struct btsco_softc {
	uint16_t		 sc_flags;
	const char		*sc_name;	/* our device_xname */

	device_t		 sc_audio;	/* MI audio device */
	void			*sc_intr;	/* interrupt cookie */
	kcondvar_t		 sc_connect;	/* connect wait */
	kmutex_t		 sc_intr_lock;	/* for audio */

	/* Bluetooth */
	bdaddr_t		 sc_laddr;	/* local address */
	bdaddr_t		 sc_raddr;	/* remote address */
	uint16_t		 sc_state;	/* link state */
	struct sco_pcb		*sc_sco;	/* SCO handle */
	struct sco_pcb		*sc_sco_l;	/* SCO listen handle */
	uint16_t		 sc_mtu;	/* SCO mtu */
	uint8_t			 sc_channel;	/* RFCOMM channel */
	int			 sc_err;	/* stored error */

	/* Receive */
	int			 sc_rx_want;	/* bytes wanted */
	uint8_t			*sc_rx_block;	/* receive block */
	void		       (*sc_rx_intr)(void *);	/* callback */
	void			*sc_rx_intrarg;	/* callback arg */
	struct mbuf		*sc_rx_mbuf;	/* leftover mbuf */

	/* Transmit */
	int			 sc_tx_size;	/* bytes to send */
	int			 sc_tx_pending;	/* packets pending */
	uint8_t			*sc_tx_block;	/* transmit block */
	void		       (*sc_tx_intr)(void *);	/* callback */
	void			*sc_tx_intrarg;	/* callback arg */
	void			*sc_tx_buf;	/* transmit buffer */
	int			 sc_tx_refcnt;	/* buffer refcnt */

	/* mixer data */
	int			 sc_vgs;	/* speaker volume */
	int			 sc_vgm;	/* mic volume */
};

/* sc_state */
#define BTSCO_CLOSED		0
#define BTSCO_WAIT_CONNECT	1
#define BTSCO_OPEN		2

/* sc_flags */
#define BTSCO_LISTEN		(1 << 1)

/* autoconf(9) glue */
static int  btsco_match(device_t, cfdata_t, void *);
static void btsco_attach(device_t, device_t, void *);
static int  btsco_detach(device_t, int);

CFATTACH_DECL_NEW(btsco, sizeof(struct btsco_softc),
    btsco_match, btsco_attach, btsco_detach, NULL);

/* audio(9) glue */
static int btsco_open(void *, int);
static void btsco_close(void *);
static int btsco_query_encoding(void *, struct audio_encoding *);
static int btsco_set_params(void *, int, int, audio_params_t *, audio_params_t *,
				stream_filter_list_t *, stream_filter_list_t *);
static int btsco_round_blocksize(void *, int, int, const audio_params_t *);
static int btsco_start_output(void *, void *, int, void (*)(void *), void *);
static int btsco_start_input(void *, void *, int, void (*)(void *), void *);
static int btsco_halt_output(void *);
static int btsco_halt_input(void *);
static int btsco_getdev(void *, struct audio_device *);
static int btsco_setfd(void *, int);
static int btsco_set_port(void *, mixer_ctrl_t *);
static int btsco_get_port(void *, mixer_ctrl_t *);
static int btsco_query_devinfo(void *, mixer_devinfo_t *);
static void *btsco_allocm(void *, int, size_t);
static void btsco_freem(void *, void *, size_t);
static int btsco_get_props(void *);
static int btsco_dev_ioctl(void *, u_long, void *, int, struct lwp *);
static void btsco_get_locks(void *, kmutex_t **, kmutex_t **);

static const struct audio_hw_if btsco_if = {
	btsco_open,		/* open */
	btsco_close,		/* close */
	NULL,			/* drain */
	btsco_query_encoding,	/* query_encoding */
	btsco_set_params,	/* set_params */
	btsco_round_blocksize,	/* round_blocksize */
	NULL,			/* commit_settings */
	NULL,			/* init_output */
	NULL,			/* init_input */
	btsco_start_output,	/* start_output */
	btsco_start_input,	/* start_input */
	btsco_halt_output,	/* halt_output */
	btsco_halt_input,	/* halt_input */
	NULL,			/* speaker_ctl */
	btsco_getdev,		/* getdev */
	btsco_setfd,		/* setfd */
	btsco_set_port,		/* set_port */
	btsco_get_port,		/* get_port */
	btsco_query_devinfo,	/* query_devinfo */
	btsco_allocm,		/* allocm */
	btsco_freem,		/* freem */
	NULL,			/* round_buffersize */
	NULL,			/* mappage */
	btsco_get_props,	/* get_props */
	NULL,			/* trigger_output */
	NULL,			/* trigger_input */
	btsco_dev_ioctl,	/* dev_ioctl */
	btsco_get_locks,	/* get_locks */
};

static const struct audio_device btsco_device = {
	"Bluetooth Audio",
	"",
	"btsco"
};

/* Voice_Setting == 0x0060: 8000Hz, mono, 16-bit, slinear_le */
static const struct audio_format btsco_format = {
	NULL,				/* driver_data */
	(AUMODE_PLAY | AUMODE_RECORD),	/* mode */
	AUDIO_ENCODING_SLINEAR_LE,	/* encoding */
	16,				/* validbits */
	16,				/* precision */
	1,				/* channels */
	AUFMT_MONAURAL,			/* channel_mask */
	1,				/* frequency_type */
	{ 8000 }			/* frequency */
};

/* bluetooth(9) glue for SCO */
static void  btsco_sco_connecting(void *);
static void  btsco_sco_connected(void *);
static void  btsco_sco_disconnected(void *, int);
static void *btsco_sco_newconn(void *, struct sockaddr_bt *, struct sockaddr_bt *);
static void  btsco_sco_complete(void *, int);
static void  btsco_sco_linkmode(void *, int);
static void  btsco_sco_input(void *, struct mbuf *);

static const struct btproto btsco_sco_proto = {
	btsco_sco_connecting,
	btsco_sco_connected,
	btsco_sco_disconnected,
	btsco_sco_newconn,
	btsco_sco_complete,
	btsco_sco_linkmode,
	btsco_sco_input,
};


/*****************************************************************************
 *
 *	btsco definitions
 */

/*
 * btsco mixer class
 */
#define BTSCO_VGS		0
#define BTSCO_VGM		1
#define BTSCO_INPUT_CLASS	2
#define BTSCO_OUTPUT_CLASS	3

/* connect timeout */
#define BTSCO_TIMEOUT		(30 * hz)

/* misc btsco functions */
static void btsco_extfree(struct mbuf *, void *, size_t, void *);
static void btsco_intr(void *);


/*****************************************************************************
 *
 *	btsco autoconf(9) routines
 */

static int
btsco_match(device_t self, cfdata_t cfdata, void *aux)
{
	prop_dictionary_t dict = aux;
	prop_object_t obj;

	obj = prop_dictionary_get(dict, BTDEVservice);
	if (prop_string_equals_cstring(obj, "HSET"))
		return 1;

	if (prop_string_equals_cstring(obj, "HF"))
		return 1;

	return 0;
}

static void
btsco_attach(device_t parent, device_t self, void *aux)
{
	struct btsco_softc *sc = device_private(self);
	prop_dictionary_t dict = aux;
	prop_object_t obj;

	/*
	 * Init softc
	 */
	sc->sc_vgs = 200;
	sc->sc_vgm = 200;
	sc->sc_state = BTSCO_CLOSED;
	sc->sc_name = device_xname(self);
	cv_init(&sc->sc_connect, "connect");
	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_NONE);

	/*
	 * copy in our configuration info
	 */
	obj = prop_dictionary_get(dict, BTDEVladdr);
	bdaddr_copy(&sc->sc_laddr, prop_data_data_nocopy(obj));

	obj = prop_dictionary_get(dict, BTDEVraddr);
	bdaddr_copy(&sc->sc_raddr, prop_data_data_nocopy(obj));

	obj = prop_dictionary_get(dict, BTDEVservice);
	if (prop_string_equals_cstring(obj, "HF")) {
		sc->sc_flags |= BTSCO_LISTEN;
		aprint_verbose(" listen mode");
	}

	obj = prop_dictionary_get(dict, BTSCOchannel);
	if (prop_object_type(obj) != PROP_TYPE_NUMBER
	    || prop_number_integer_value(obj) < RFCOMM_CHANNEL_MIN
	    || prop_number_integer_value(obj) > RFCOMM_CHANNEL_MAX) {
		aprint_error(" invalid %s", BTSCOchannel);
		return;
	}
	sc->sc_channel = prop_number_integer_value(obj);

	aprint_verbose(" channel %d", sc->sc_channel);
	aprint_normal("\n");

	DPRINTF("sc=%p\n", sc);

	/*
	 * set up transmit interrupt
	 */
	sc->sc_intr = softint_establish(SOFTINT_NET, btsco_intr, sc);
	if (sc->sc_intr == NULL) {
		aprint_error_dev(self, "softint_establish failed\n");
		return;
	}

	/*
	 * attach audio device
	 */
	sc->sc_audio = audio_attach_mi(&btsco_if, sc, self);
	if (sc->sc_audio == NULL) {
		aprint_error_dev(self, "audio_attach_mi failed\n");
		return;
	}

	pmf_device_register(self, NULL, NULL);
}

static int
btsco_detach(device_t self, int flags)
{
	struct btsco_softc *sc = device_private(self);

	DPRINTF("sc=%p\n", sc);

	pmf_device_deregister(self);

	mutex_enter(bt_lock);
	if (sc->sc_sco != NULL) {
		DPRINTF("sc_sco=%p\n", sc->sc_sco);
		sco_disconnect_pcb(sc->sc_sco, 0);
		sco_detach_pcb(&sc->sc_sco);
		sc->sc_sco = NULL;
	}

	if (sc->sc_sco_l != NULL) {
		DPRINTF("sc_sco_l=%p\n", sc->sc_sco_l);
		sco_detach_pcb(&sc->sc_sco_l);
		sc->sc_sco_l = NULL;
	}
	mutex_exit(bt_lock);

	if (sc->sc_audio != NULL) {
		DPRINTF("sc_audio=%p\n", sc->sc_audio);
		config_detach(sc->sc_audio, flags);
		sc->sc_audio = NULL;
	}

	if (sc->sc_intr != NULL) {
		softint_disestablish(sc->sc_intr);
		sc->sc_intr = NULL;
	}

	if (sc->sc_rx_mbuf != NULL) {
		m_freem(sc->sc_rx_mbuf);
		sc->sc_rx_mbuf = NULL;
	}

	if (sc->sc_tx_refcnt > 0) {
		aprint_error_dev(self, "tx_refcnt=%d!\n", sc->sc_tx_refcnt);

		if ((flags & DETACH_FORCE) == 0)
			return EAGAIN;
	}

	cv_destroy(&sc->sc_connect);
	mutex_destroy(&sc->sc_intr_lock);

	return 0;
}

/*****************************************************************************
 *
 *	bluetooth(9) methods for SCO
 *
 *	All these are called from Bluetooth Protocol code, in a soft
 *	interrupt context at IPL_SOFTNET.
 */

static void
btsco_sco_connecting(void *arg)
{
/*	struct btsco_softc *sc = arg;	*/

	/* dont care */
}

static void
btsco_sco_connected(void *arg)
{
	struct btsco_softc *sc = arg;

	DPRINTF("%s\n", sc->sc_name);

	KASSERT(sc->sc_sco != NULL);
	KASSERT(sc->sc_state == BTSCO_WAIT_CONNECT);

	/*
	 * If we are listening, no more need
	 */
	if (sc->sc_sco_l != NULL)
		sco_detach_pcb(&sc->sc_sco_l);

	sc->sc_state = BTSCO_OPEN;
	cv_broadcast(&sc->sc_connect);
}

static void
btsco_sco_disconnected(void *arg, int err)
{
	struct btsco_softc *sc = arg;

	DPRINTF("%s sc_state %d\n", sc->sc_name, sc->sc_state);

	KASSERT(sc->sc_sco != NULL);

	sc->sc_err = err;
	sco_detach_pcb(&sc->sc_sco);

	switch (sc->sc_state) {
	case BTSCO_CLOSED:		/* dont think this can happen */
		break;

	case BTSCO_WAIT_CONNECT:	/* connect failed */
		cv_broadcast(&sc->sc_connect);
		break;

	case BTSCO_OPEN:		/* link lost */
		/*
		 * If IO is in progress, tell the audio driver that it
		 * has completed so that when it tries to send more, we
		 * can indicate an error.
		 */
		mutex_enter(&sc->sc_intr_lock);
		if (sc->sc_tx_pending > 0) {
			sc->sc_tx_pending = 0;
			(*sc->sc_tx_intr)(sc->sc_tx_intrarg);
		}
		if (sc->sc_rx_want > 0) {
			sc->sc_rx_want = 0;
			(*sc->sc_rx_intr)(sc->sc_rx_intrarg);
		}
		mutex_exit(&sc->sc_intr_lock);
		break;

	default:
		UNKNOWN(sc->sc_state);
	}

	sc->sc_state = BTSCO_CLOSED;
}

static void *
btsco_sco_newconn(void *arg, struct sockaddr_bt *laddr,
    struct sockaddr_bt *raddr)
{
	struct btsco_softc *sc = arg;

	DPRINTF("%s\n", sc->sc_name);

	if (bdaddr_same(&raddr->bt_bdaddr, &sc->sc_raddr) == 0
	    || sc->sc_state != BTSCO_WAIT_CONNECT
	    || sc->sc_sco != NULL)
	    return NULL;

	sco_attach_pcb(&sc->sc_sco, &btsco_sco_proto, sc);
	return sc->sc_sco;
}

static void
btsco_sco_complete(void *arg, int count)
{
	struct btsco_softc *sc = arg;

	DPRINTFN(10, "%s count %d\n", sc->sc_name, count);

	mutex_enter(&sc->sc_intr_lock);
	if (sc->sc_tx_pending > 0) {
		sc->sc_tx_pending -= count;
		if (sc->sc_tx_pending == 0)
			(*sc->sc_tx_intr)(sc->sc_tx_intrarg);
	}
	mutex_exit(&sc->sc_intr_lock);
}

static void
btsco_sco_linkmode(void *arg, int new)
{
/*	struct btsco_softc *sc = arg;	*/

	/* dont care */
}

static void
btsco_sco_input(void *arg, struct mbuf *m)
{
	struct btsco_softc *sc = arg;
	int len;

	DPRINTFN(10, "%s len=%d\n", sc->sc_name, m->m_pkthdr.len);

	mutex_enter(&sc->sc_intr_lock);
	if (sc->sc_rx_want == 0) {
		m_freem(m);
	} else {
		KASSERT(sc->sc_rx_intr != NULL);
		KASSERT(sc->sc_rx_block != NULL);

		len = MIN(sc->sc_rx_want, m->m_pkthdr.len);
		m_copydata(m, 0, len, sc->sc_rx_block);

		sc->sc_rx_want -= len;
		sc->sc_rx_block += len;

		if (len > m->m_pkthdr.len) {
			if (sc->sc_rx_mbuf != NULL)
				m_freem(sc->sc_rx_mbuf);

			m_adj(m, len);
			sc->sc_rx_mbuf = m;
		} else {
			m_freem(m);
		}

		if (sc->sc_rx_want == 0)
			(*sc->sc_rx_intr)(sc->sc_rx_intrarg);
	}
	mutex_exit(&sc->sc_intr_lock);
}


/*****************************************************************************
 *
 *	audio(9) methods
 *
 */

static int
btsco_open(void *hdl, int flags)
{
	struct sockaddr_bt sa;
	struct btsco_softc *sc = hdl;
	struct sockopt sopt;
	int err, timo;

	DPRINTF("%s flags 0x%x\n", sc->sc_name, flags);
	/* flags FREAD & FWRITE? */

	if (sc->sc_sco != NULL || sc->sc_sco_l != NULL)
		return EIO;

	KASSERT(mutex_owned(bt_lock));

	memset(&sa, 0, sizeof(sa));
	sa.bt_len = sizeof(sa);
	sa.bt_family = AF_BLUETOOTH;
	bdaddr_copy(&sa.bt_bdaddr, &sc->sc_laddr);

	if (sc->sc_flags & BTSCO_LISTEN) {
		err = sco_attach_pcb(&sc->sc_sco_l, &btsco_sco_proto, sc);
		if (err)
			goto done;

		err = sco_bind_pcb(sc->sc_sco_l, &sa);
		if (err) {
			sco_detach_pcb(&sc->sc_sco_l);
			goto done;
		}

		err = sco_listen_pcb(sc->sc_sco_l);
		if (err) {
			sco_detach_pcb(&sc->sc_sco_l);
			goto done;
		}

		timo = 0;	/* no timeout */
	} else {
		err = sco_attach_pcb(&sc->sc_sco, &btsco_sco_proto, sc);
		if (err)
			goto done;

		err = sco_bind_pcb(sc->sc_sco, &sa);
		if (err) {
			sco_detach_pcb(&sc->sc_sco);
			goto done;
		}

		bdaddr_copy(&sa.bt_bdaddr, &sc->sc_raddr);
		err = sco_connect_pcb(sc->sc_sco, &sa);
		if (err) {
			sco_detach_pcb(&sc->sc_sco);
			goto done;
		}

		timo = BTSCO_TIMEOUT;
	}

	sc->sc_state = BTSCO_WAIT_CONNECT;
	while (err == 0 && sc->sc_state == BTSCO_WAIT_CONNECT)
		err = cv_timedwait_sig(&sc->sc_connect, bt_lock, timo);

	switch (sc->sc_state) {
	case BTSCO_CLOSED:		/* disconnected */
		err = sc->sc_err;

		/* fall through to */
	case BTSCO_WAIT_CONNECT:	/* error */
		if (sc->sc_sco != NULL)
			sco_detach_pcb(&sc->sc_sco);

		if (sc->sc_sco_l != NULL)
			sco_detach_pcb(&sc->sc_sco_l);

		break;

	case BTSCO_OPEN:		/* hurrah */
		sockopt_init(&sopt, BTPROTO_SCO, SO_SCO_MTU, 0);
		(void)sco_getopt(sc->sc_sco, &sopt);
		(void)sockopt_get(&sopt, &sc->sc_mtu, sizeof(sc->sc_mtu));
		sockopt_destroy(&sopt);
		break;

	default:
		UNKNOWN(sc->sc_state);
		break;
	}

done:
	DPRINTF("done err=%d, sc_state=%d, sc_mtu=%d\n",
			err, sc->sc_state, sc->sc_mtu);
	return err;
}

static void
btsco_close(void *hdl)
{
	struct btsco_softc *sc = hdl;

	DPRINTF("%s\n", sc->sc_name);

	KASSERT(mutex_owned(bt_lock));

	if (sc->sc_sco != NULL) {
		sco_disconnect_pcb(sc->sc_sco, 0);
		sco_detach_pcb(&sc->sc_sco);
	}

	if (sc->sc_sco_l != NULL) {
		sco_detach_pcb(&sc->sc_sco_l);
	}

	if (sc->sc_rx_mbuf != NULL) {
		m_freem(sc->sc_rx_mbuf);
		sc->sc_rx_mbuf = NULL;
	}

	sc->sc_rx_want = 0;
	sc->sc_rx_block = NULL;
	sc->sc_rx_intr = NULL;
	sc->sc_rx_intrarg = NULL;

	sc->sc_tx_size = 0;
	sc->sc_tx_block = NULL;
	sc->sc_tx_pending = 0;
	sc->sc_tx_intr = NULL;
	sc->sc_tx_intrarg = NULL;
}

static int
btsco_query_encoding(void *hdl, struct audio_encoding *ae)
{
/*	struct btsco_softc *sc = hdl;	*/
	int err = 0;

	switch (ae->index) {
	case 0:
		strcpy(ae->name, AudioEslinear_le);
		ae->encoding = AUDIO_ENCODING_SLINEAR_LE;
		ae->precision = 16;
		ae->flags = 0;
		break;

	default:
		err = EINVAL;
	}

	return err;
}

static int
btsco_set_params(void *hdl, int setmode, int usemode,
		audio_params_t *play, audio_params_t *rec,
		stream_filter_list_t *pfil, stream_filter_list_t *rfil)
{
/*	struct btsco_softc *sc = hdl;	*/
	const struct audio_format *f;
	int rv;

	DPRINTF("setmode 0x%x usemode 0x%x\n", setmode, usemode);
	DPRINTF("rate %d, precision %d, channels %d encoding %d\n",
		play->sample_rate, play->precision, play->channels, play->encoding);

	/*
	 * If we had a list of formats, we could check the HCI_Voice_Setting
	 * and select the appropriate one to use. Currently only one is
	 * supported: 0x0060 == 8000Hz, mono, 16-bit, slinear_le
	 */
	f = &btsco_format;

	if (setmode & AUMODE_PLAY) {
		rv = auconv_set_converter(f, 1, AUMODE_PLAY, play, TRUE, pfil);
		if (rv < 0)
			return EINVAL;
	}

	if (setmode & AUMODE_RECORD) {
		rv = auconv_set_converter(f, 1, AUMODE_RECORD, rec, TRUE, rfil);
		if (rv < 0)
			return EINVAL;
	}

	return 0;
}

/*
 * If we have an MTU value to use, round the blocksize to that.
 */
static int
btsco_round_blocksize(void *hdl, int bs, int mode,
    const audio_params_t *param)
{
	struct btsco_softc *sc = hdl;

	if (sc->sc_mtu > 0) {
		bs = (bs / sc->sc_mtu) * sc->sc_mtu;
		if (bs == 0)
			bs = sc->sc_mtu;
	}

	DPRINTF("%s mode=0x%x, bs=%d, sc_mtu=%d\n",
			sc->sc_name, mode, bs, sc->sc_mtu);

	return bs;
}

/*
 * Start Output
 *
 * We dont want to be calling the network stack with sc_intr_lock held
 * so make a note of what is to be sent, and schedule an interrupt to
 * bundle it up and queue it.
 */
static int
btsco_start_output(void *hdl, void *block, int blksize,
		void (*intr)(void *), void *intrarg)
{
	struct btsco_softc *sc = hdl;

	DPRINTFN(5, "%s blksize %d\n", sc->sc_name, blksize);

	if (sc->sc_sco == NULL)
		return ENOTCONN;	/* connection lost */

	sc->sc_tx_block = block;
	sc->sc_tx_pending = 0;
	sc->sc_tx_size = blksize;
	sc->sc_tx_intr = intr;
	sc->sc_tx_intrarg = intrarg;

	kpreempt_disable();
	softint_schedule(sc->sc_intr);
	kpreempt_enable();
	return 0;
}

/*
 * Start Input
 *
 * When the SCO link is up, we are getting data in any case, so all we do
 * is note what we want and where to put it and let the sco_input routine
 * fill in the data.
 *
 * If there was any leftover data that didnt fit in the last block, retry
 * it now.
 */
static int
btsco_start_input(void *hdl, void *block, int blksize,
		void (*intr)(void *), void *intrarg)
{
	struct btsco_softc *sc = hdl;
	struct mbuf *m;

	DPRINTFN(5, "%s blksize %d\n", sc->sc_name, blksize);

	if (sc->sc_sco == NULL)
		return ENOTCONN;

	sc->sc_rx_want = blksize;
	sc->sc_rx_block = block;
	sc->sc_rx_intr = intr;
	sc->sc_rx_intrarg = intrarg;

	if (sc->sc_rx_mbuf != NULL) {
		m = sc->sc_rx_mbuf;
		sc->sc_rx_mbuf = NULL;
		btsco_sco_input(sc, m);
	}

	return 0;
}

/*
 * Halt Output
 *
 * This doesnt really halt the output, but it will look
 * that way to the audio driver. The current block will
 * still be transmitted.
 */
static int
btsco_halt_output(void *hdl)
{
	struct btsco_softc *sc = hdl;

	DPRINTFN(5, "%s\n", sc->sc_name);

	sc->sc_tx_size = 0;
	sc->sc_tx_block = NULL;
	sc->sc_tx_pending = 0;
	sc->sc_tx_intr = NULL;
	sc->sc_tx_intrarg = NULL;

	return 0;
}

/*
 * Halt Input
 *
 * This doesnt really halt the input, but it will look
 * that way to the audio driver. Incoming data will be
 * discarded.
 */
static int
btsco_halt_input(void *hdl)
{
	struct btsco_softc *sc = hdl;

	DPRINTFN(5, "%s\n", sc->sc_name);

	sc->sc_rx_want = 0;
	sc->sc_rx_block = NULL;
	sc->sc_rx_intr = NULL;
	sc->sc_rx_intrarg = NULL;

	if (sc->sc_rx_mbuf != NULL) {
		m_freem(sc->sc_rx_mbuf);
		sc->sc_rx_mbuf = NULL;
	}

	return 0;
}

static int
btsco_getdev(void *hdl, struct audio_device *ret)
{

	*ret = btsco_device;
	return 0;
}

static int
btsco_setfd(void *hdl, int fd)
{
	DPRINTF("set %s duplex\n", fd ? "full" : "half");

	return 0;
}

static int
btsco_set_port(void *hdl, mixer_ctrl_t *mc)
{
	struct btsco_softc *sc = hdl;
	int err = 0;

	DPRINTF("%s dev %d type %d\n", sc->sc_name, mc->dev, mc->type);

	switch (mc->dev) {
	case BTSCO_VGS:
		if (mc->type != AUDIO_MIXER_VALUE ||
		    mc->un.value.num_channels != 1) {
			err = EINVAL;
			break;
		}

		sc->sc_vgs = mc->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		break;

	case BTSCO_VGM:
		if (mc->type != AUDIO_MIXER_VALUE ||
		    mc->un.value.num_channels != 1) {
			err = EINVAL;
			break;
		}

		sc->sc_vgm = mc->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		break;

	default:
		err = EINVAL;
		break;
	}

	return err;
}

static int
btsco_get_port(void *hdl, mixer_ctrl_t *mc)
{
	struct btsco_softc *sc = hdl;
	int err = 0;

	DPRINTF("%s dev %d\n", sc->sc_name, mc->dev);

	switch (mc->dev) {
	case BTSCO_VGS:
		mc->type = AUDIO_MIXER_VALUE;
		mc->un.value.num_channels = 1;
		mc->un.value.level[AUDIO_MIXER_LEVEL_MONO] = sc->sc_vgs;
		break;

	case BTSCO_VGM:
		mc->type = AUDIO_MIXER_VALUE;
		mc->un.value.num_channels = 1;
		mc->un.value.level[AUDIO_MIXER_LEVEL_MONO] = sc->sc_vgm;
		break;

	default:
		err = EINVAL;
		break;
	}

	return err;
}

static int
btsco_query_devinfo(void *hdl, mixer_devinfo_t *di)
{
/*	struct btsco_softc *sc = hdl;	*/
	int err = 0;

	switch(di->index) {
	case BTSCO_VGS:
		di->mixer_class = BTSCO_INPUT_CLASS;
		di->next = di->prev = AUDIO_MIXER_LAST;
		strcpy(di->label.name, AudioNspeaker);
		di->type = AUDIO_MIXER_VALUE;
		strcpy(di->un.v.units.name, AudioNvolume);
		di->un.v.num_channels = 1;
		di->un.v.delta = BTSCO_DELTA;
		break;

	case BTSCO_VGM:
		di->mixer_class = BTSCO_INPUT_CLASS;
		di->next = di->prev = AUDIO_MIXER_LAST;
		strcpy(di->label.name, AudioNmicrophone);
		di->type = AUDIO_MIXER_VALUE;
		strcpy(di->un.v.units.name, AudioNvolume);
		di->un.v.num_channels = 1;
		di->un.v.delta = BTSCO_DELTA;
		break;

	case BTSCO_INPUT_CLASS:
		di->mixer_class = BTSCO_INPUT_CLASS;
		di->next = di->prev = AUDIO_MIXER_LAST;
		strcpy(di->label.name, AudioCinputs);
		di->type = AUDIO_MIXER_CLASS;
		break;

	default:
		err = ENXIO;
		break;
	}

	return err;
}

/*
 * Allocate Ring Buffers.
 */
static void *
btsco_allocm(void *hdl, int direction, size_t size)
{
	struct btsco_softc *sc = hdl;
	void *addr;

	DPRINTF("%s: size %d direction %d\n", sc->sc_name, size, direction);

	addr = kmem_alloc(size, KM_SLEEP);

	if (addr != NULL && direction == AUMODE_PLAY) {
		sc->sc_tx_buf = addr;
		sc->sc_tx_refcnt = 0;
	}

	return addr;
}

/*
 * Free Ring Buffers.
 *
 * Because we used external memory for the tx mbufs, we dont
 * want to free the memory until all the mbufs are done with
 *
 * Just to be sure, dont free if something is still pending.
 * This would be a memory leak but at least there is a warning..
 */
static void
btsco_freem(void *hdl, void *addr, size_t size)
{
	struct btsco_softc *sc = hdl;
	int count = hz / 2;

	if (addr == sc->sc_tx_buf) {
		DPRINTF("%s: tx_refcnt=%d\n", sc->sc_name, sc->sc_tx_refcnt);

		sc->sc_tx_buf = NULL;

		while (sc->sc_tx_refcnt> 0 && count-- > 0)
			kpause("drain", false, 1, NULL);

		if (sc->sc_tx_refcnt > 0) {
			aprint_error("%s: ring buffer unreleased!\n", sc->sc_name);
			return;
		}
	}

	kmem_free(addr, size);
}

static int
btsco_get_props(void *hdl)
{

	return AUDIO_PROP_FULLDUPLEX;
}

static void
btsco_get_locks(void *hdl, kmutex_t **intr, kmutex_t **thread)
{
	struct btsco_softc *sc = hdl;

	*intr = &sc->sc_intr_lock;
	*thread = bt_lock;
}

/*
 * Handle private ioctl. We pass information out about how to talk
 * to the device and mixer.
 */
static int
btsco_dev_ioctl(void *hdl, u_long cmd, void *addr, int flag,
    struct lwp *l)
{
	struct btsco_softc *sc = hdl;
	struct btsco_info *bi = (struct btsco_info *)addr;
	int err = 0;

	DPRINTF("%s cmd 0x%lx flag %d\n", sc->sc_name, cmd, flag);

	switch (cmd) {
	case BTSCO_GETINFO:
		memset(bi, 0, sizeof(*bi));
		bdaddr_copy(&bi->laddr, &sc->sc_laddr);
		bdaddr_copy(&bi->raddr, &sc->sc_raddr);
		bi->channel = sc->sc_channel;
		bi->vgs = BTSCO_VGS;
		bi->vgm = BTSCO_VGM;
		break;

	default:
		err = EPASSTHROUGH;
		break;
	}

	return err;
}


/*****************************************************************************
 *
 *	misc btsco functions
 *
 */

/*
 * Our transmit interrupt. This is triggered when a new block is to be
 * sent.  We send mtu sized chunks of the block as mbufs with external
 * storage to sco_send_pcb()
 */
static void
btsco_intr(void *arg)
{
	struct btsco_softc *sc = arg;
	struct mbuf *m;
	uint8_t *block;
	int mlen, size;

	DPRINTFN(10, "%s block %p size %d\n",
	    sc->sc_name, sc->sc_tx_block, sc->sc_tx_size);

	if (sc->sc_sco == NULL)
		return;		/* connection is lost */

	block = sc->sc_tx_block;
	size = sc->sc_tx_size;
	sc->sc_tx_block = NULL;
	sc->sc_tx_size = 0;

	mutex_enter(bt_lock);
	while (size > 0) {
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL)
			break;

		mlen = MIN(sc->sc_mtu, size);

		/* I think M_DEVBUF is true but not relevant */
		MEXTADD(m, block, mlen, M_DEVBUF, btsco_extfree, sc);
		if ((m->m_flags & M_EXT) == 0) {
			m_free(m);
			break;
		}
		sc->sc_tx_refcnt++;

		m->m_pkthdr.len = m->m_len = mlen;
		sc->sc_tx_pending++;

		if (sco_send_pcb(sc->sc_sco, m) > 0) {
			sc->sc_tx_pending--;
			break;
		}

		block += mlen;
		size -= mlen;
	}
	mutex_exit(bt_lock);
}

/*
 * Release the mbuf, we keep a reference count on the tx buffer so
 * that we dont release it before its free.
 */
static void
btsco_extfree(struct mbuf *m, void *addr, size_t size,
    void *arg)
{
	struct btsco_softc *sc = arg;

	if (m != NULL)
		pool_cache_put(mb_cache, m);

	sc->sc_tx_refcnt--;
}

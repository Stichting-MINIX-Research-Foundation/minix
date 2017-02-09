/* $NetBSD: hdafg.c,v 1.3 2015/07/26 19:06:26 jmcneill Exp $ */

/*
 * Copyright (c) 2009 Precedence Technologies Ltd <support@precedence.co.uk>
 * Copyright (c) 2009-2011 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Precedence Technologies Ltd
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Widget parsing from FreeBSD hdac.c:
 *
 * Copyright (c) 2006 Stephane E. Potvin <sepotvin@videotron.ca>
 * Copyright (c) 2006 Ariff Abdullah <ariff@FreeBSD.org>
 * Copyright (c) 2008 Alexander Motin <mav@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hdafg.c,v 1.3 2015/07/26 19:06:26 jmcneill Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kmem.h>
#include <sys/module.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/auconv.h>

#ifdef _KERNEL_OPT
#include "opt_hdaudio.h"
#endif

#include "hdaudiovar.h"
#include "hdaudioreg.h"
#include "hdaudio_mixer.h"
#include "hdaudioio.h"
#include "hdaudio_verbose.h"
#include "hdaudiodevs.h"
#include "hdafg_dd.h"
#include "hdmireg.h"

#ifndef AUFMT_SURROUND_7_1
#define	AUFMT_SURROUND_7_1 (AUFMT_DOLBY_5_1|AUFMT_SIDE_LEFT|AUFMT_SIDE_RIGHT)
#endif

#if defined(HDAFG_DEBUG)
static int hdafg_debug = HDAFG_DEBUG;
#else
static int hdafg_debug = 0;
#endif

#define	hda_debug(sc, ...)		\
	if (hdafg_debug) hda_print(sc, __VA_ARGS__)
#define	hda_debug1(sc, ...)		\
	if (hdafg_debug) hda_print1(sc, __VA_ARGS__)

#define HDAUDIO_MIXER_CLASS_OUTPUTS	0
#define	HDAUDIO_MIXER_CLASS_INPUTS	1
#define	HDAUDIO_MIXER_CLASS_RECORD	2
#define	HDAUDIO_MIXER_CLASS_LAST	HDAUDIO_MIXER_CLASS_RECORD

#define	HDAUDIO_GPIO_MASK	0
#define	HDAUDIO_GPIO_DIR	1
#define	HDAUDIO_GPIO_DATA	2

#define	HDAUDIO_UNSOLTAG_EVENT_HP	0x01
#define	HDAUDIO_UNSOLTAG_EVENT_DD	0x02

#define	HDAUDIO_HP_SENSE_PERIOD		hz

const u_int hdafg_possible_rates[] = {
	8000, 11025, 16000, 22050, 32000, 44100,
	48000, 88200, 96000, 176500, 192000, /* 384000, */
};

static const char *hdafg_mixer_names[] = HDAUDIO_DEVICE_NAMES;

static const char *hdafg_port_connectivity[] = {
	"Jack",
	"Unconnected",
	"Built-In",
	"Jack & Built-In"
};
static const char *hdafg_default_device[] = {
	"Line Out",
	"Speaker",
	"HP Out",
	"CD",
	"SPDIF Out",
	"Digital Out",
	"Modem Line Side",
	"Modem Handset Side",
	"Line In",
	"AUX",
	"Mic In",
	"Telephony",
	"SPDIF In",
	"Digital In",
	"Reserved",
	"Other"
};
static const char *hdafg_color[] = {
	"Unknown",
	"Black",
	"Grey",
	"Blue",
	"Green",
	"Red",
	"Orange",
	"Yellow",
	"Purple",
	"Pink",
	"ReservedA",
	"ReservedB",
	"ReservedC",
	"ReservedD",
	"White",
	"Other"
};

#define	HDAUDIO_MAXFORMATS	24
#define	HDAUDIO_MAXCONNECTIONS	32
#define	HDAUDIO_MAXPINS		16
#define	HDAUDIO_PARSE_MAXDEPTH	10

#define	HDAUDIO_AMP_VOL_DEFAULT	(-1)
#define	HDAUDIO_AMP_MUTE_DEFAULT (0xffffffff)
#define	HDAUDIO_AMP_MUTE_NONE	0
#define	HDAUDIO_AMP_MUTE_LEFT	(1 << 0)
#define	HDAUDIO_AMP_MUTE_RIGHT	(1 << 1)
#define	HDAUDIO_AMP_MUTE_ALL	(HDAUDIO_AMP_MUTE_LEFT | HDAUDIO_AMP_MUTE_RIGHT)
#define	HDAUDIO_AMP_LEFT_MUTED(x)	((x) & HDAUDIO_AMP_MUTE_LEFT)
#define	HDAUDIO_AMP_RIGHT_MUTED(x)	(((x) & HDAUDIO_AMP_MUTE_RIGHT) >> 1)

#define	HDAUDIO_ADC_MONITOR	1

enum hdaudio_pindir {
	HDAUDIO_PINDIR_NONE = 0,
	HDAUDIO_PINDIR_OUT = 1,
	HDAUDIO_PINDIR_IN = 2,
	HDAUDIO_PINDIR_INOUT = 3,
};

#define	hda_get_param(sc, cop)					\
	hdaudio_command((sc)->sc_codec, (sc)->sc_nid,		\
	  CORB_GET_PARAMETER, COP_##cop)
#define	hda_get_wparam(w, cop)					\
	hdaudio_command((w)->w_afg->sc_codec, (w)->w_nid,	\
	  CORB_GET_PARAMETER, COP_##cop)

struct hdaudio_assoc {
	bool			as_enable;
	bool			as_activated;
	u_char			as_index;
	enum hdaudio_pindir	as_dir;
	u_char			as_pincnt;
	u_char			as_fakeredir;
	int			as_digital;
#define	HDAFG_AS_ANALOG		0
#define	HDAFG_AS_SPDIF		1
#define	HDAFG_AS_HDMI		2
#define	HDAFG_AS_DISPLAYPORT	3
	bool			as_displaydev;
	int			as_hpredir;
	int			as_pins[HDAUDIO_MAXPINS];
	int			as_dacs[HDAUDIO_MAXPINS];
};

struct hdaudio_widget {
	struct hdafg_softc	*w_afg;
	char				w_name[32];
	int				w_nid;
	bool				w_enable;
	bool				w_waspin;
	int				w_selconn;
	int				w_bindas;
	int				w_bindseqmask;
	int				w_pflags;
	int				w_audiodev;
	uint32_t			w_audiomask;

	int				w_nconns;
	int				w_conns[HDAUDIO_MAXCONNECTIONS];
	bool				w_connsenable[HDAUDIO_MAXCONNECTIONS];

	int				w_type;
	struct {
		uint32_t		aw_cap;
		uint32_t		pcm_size_rate;
		uint32_t		stream_format;
		uint32_t		outamp_cap;
		uint32_t		inamp_cap;
		uint32_t		eapdbtl;
	} w_p;
	struct {
		uint32_t		config;
		uint32_t		biosconfig;
		uint32_t		cap;
		uint32_t		ctrl;
	} w_pin;
};

struct hdaudio_control {
	struct hdaudio_widget	*ctl_widget, *ctl_childwidget;
	bool			ctl_enable;
	int			ctl_index;
	enum hdaudio_pindir	ctl_dir, ctl_ndir;
	int			ctl_mute, ctl_step, ctl_size, ctl_offset;
	int			ctl_left, ctl_right, ctl_forcemute;
	uint32_t		ctl_muted;
	uint32_t		ctl_audiomask, ctl_paudiomask;
};

#define	HDAUDIO_CONTROL_GIVE(ctl)	((ctl)->ctl_step ? 1 : 0)

struct hdaudio_mixer {
	struct hdaudio_control		*mx_ctl;
	mixer_devinfo_t			mx_di;
};

struct hdaudio_audiodev {
	struct hdafg_softc	*ad_sc;
	device_t			ad_audiodev;
	struct audio_encoding_set	*ad_encodings;
	int				ad_nformats;
	struct audio_format		ad_formats[HDAUDIO_MAXFORMATS];

	struct hdaudio_stream		*ad_playback;
	void				(*ad_playbackintr)(void *);
	void				*ad_playbackintrarg;
	int				ad_playbacknid[HDAUDIO_MAXPINS];
	struct hdaudio_assoc		*ad_playbackassoc;
	struct hdaudio_stream		*ad_capture;
	void				(*ad_captureintr)(void *);
	void				*ad_captureintrarg;
	int				ad_capturenid[HDAUDIO_MAXPINS];
	struct hdaudio_assoc		*ad_captureassoc;
};

struct hdafg_softc {
	device_t			sc_dev;
	kmutex_t			sc_lock;
	kmutex_t			sc_intr_lock;
	struct hdaudio_softc		*sc_host;
	struct hdaudio_codec		*sc_codec;
	struct hdaudio_function_group	*sc_fg;
	int				sc_nid;
	uint16_t			sc_vendor, sc_product;

	prop_array_t			sc_config;

	int				sc_startnode, sc_endnode;
	int				sc_nwidgets;
	struct hdaudio_widget		*sc_widgets;
	int				sc_nassocs;
	struct hdaudio_assoc		*sc_assocs;
	int				sc_nctls;
	struct hdaudio_control		*sc_ctls;
	int				sc_nmixers;
	struct hdaudio_mixer		*sc_mixers;
	bool				sc_has_beepgen;

	int				sc_pchan, sc_rchan;
	audio_params_t			sc_pparam, sc_rparam;

	struct callout			sc_jack_callout;
	bool				sc_jack_polling;

	struct {
		uint32_t		afg_cap;
		uint32_t		pcm_size_rate;
		uint32_t		stream_format;
		uint32_t		outamp_cap;
		uint32_t		inamp_cap;
		uint32_t		power_states;
		uint32_t		gpio_cnt;
	} sc_p;

	struct hdaudio_audiodev		sc_audiodev;

	uint16_t			sc_fixed_rate;
};

static int	hdafg_match(device_t, cfdata_t, void *);
static void	hdafg_attach(device_t, device_t, void *);
static int	hdafg_detach(device_t, int);
static void	hdafg_childdet(device_t, device_t);
static bool	hdafg_suspend(device_t, const pmf_qual_t *);
static bool	hdafg_resume(device_t, const pmf_qual_t *);

static int	hdafg_unsol(device_t, uint8_t);
static int	hdafg_widget_info(void *, prop_dictionary_t,
					prop_dictionary_t);
static int	hdafg_codec_info(void *, prop_dictionary_t,
				       prop_dictionary_t);
static void	hdafg_enable_analog_beep(struct hdafg_softc *);

CFATTACH_DECL2_NEW(
    hdafg,
    sizeof(struct hdafg_softc),
    hdafg_match,
    hdafg_attach,
    hdafg_detach,
    NULL,
    NULL,
    hdafg_childdet
);

static int	hdafg_query_encoding(void *, struct audio_encoding *);
static int	hdafg_set_params(void *, int, int,
				   audio_params_t *,
				   audio_params_t *,
				   stream_filter_list_t *,
				   stream_filter_list_t *);
static int	hdafg_round_blocksize(void *, int, int,
					const audio_params_t *);
static int	hdafg_commit_settings(void *);
static int	hdafg_halt_output(void *);
static int	hdafg_halt_input(void *);
static int	hdafg_set_port(void *, mixer_ctrl_t *);
static int	hdafg_get_port(void *, mixer_ctrl_t *);
static int	hdafg_query_devinfo(void *, mixer_devinfo_t *);
static void *	hdafg_allocm(void *, int, size_t);
static void	hdafg_freem(void *, void *, size_t);
static int	hdafg_getdev(void *, struct audio_device *);
static size_t	hdafg_round_buffersize(void *, int, size_t);
static paddr_t	hdafg_mappage(void *, void *, off_t, int);
static int	hdafg_get_props(void *);
static int	hdafg_trigger_output(void *, void *, void *, int,
				       void (*)(void *), void *,
				       const audio_params_t *);
static int	hdafg_trigger_input(void *, void *, void *, int,
				      void (*)(void *), void *,
				      const audio_params_t *);
static void	hdafg_get_locks(void *, kmutex_t **, kmutex_t **);

static const struct audio_hw_if hdafg_hw_if = {
	.query_encoding		= hdafg_query_encoding,
	.set_params		= hdafg_set_params,
	.round_blocksize	= hdafg_round_blocksize,
	.commit_settings	= hdafg_commit_settings,
	.halt_output		= hdafg_halt_output,
	.halt_input		= hdafg_halt_input,
	.getdev			= hdafg_getdev,
	.set_port		= hdafg_set_port,
	.get_port		= hdafg_get_port,
	.query_devinfo		= hdafg_query_devinfo,
	.allocm			= hdafg_allocm,
	.freem			= hdafg_freem,
	.round_buffersize	= hdafg_round_buffersize,
	.mappage		= hdafg_mappage,
	.get_props		= hdafg_get_props,
	.trigger_output		= hdafg_trigger_output,
	.trigger_input		= hdafg_trigger_input,
	.get_locks		= hdafg_get_locks,
};

static int
hdafg_append_formats(struct hdaudio_audiodev *ad,
    const struct audio_format *format)
{
	if (ad->ad_nformats + 1 >= HDAUDIO_MAXFORMATS) {
		hda_print1(ad->ad_sc, "[ENOMEM] ");
		return ENOMEM;
	}
	ad->ad_formats[ad->ad_nformats++] = *format;

	return 0;
}

static struct hdaudio_widget *
hdafg_widget_lookup(struct hdafg_softc *sc, int nid)
{
	if (sc->sc_widgets == NULL || sc->sc_nwidgets == 0) {
		hda_error(sc, "lookup failed; widgets %p nwidgets %d\n",
		    sc->sc_widgets, sc->sc_nwidgets);
		return NULL;
	}
	if (nid < sc->sc_startnode || nid >= sc->sc_endnode) {
		hda_debug(sc, "nid %02X out of range (%02X-%02X)\n",
		    nid, sc->sc_startnode, sc->sc_endnode);
		return NULL;
	}
	return &sc->sc_widgets[nid - sc->sc_startnode];
}

static struct hdaudio_control *
hdafg_control_lookup(struct hdafg_softc *sc, int nid,
    enum hdaudio_pindir dir, int index, int cnt)
{
	struct hdaudio_control *ctl;
	int i, found = 0;

	if (sc->sc_ctls == NULL)
		return NULL;
	for (i = 0; i < sc->sc_nctls; i++) {
		ctl = &sc->sc_ctls[i];
		if (ctl->ctl_enable == false)
			continue;
		if (ctl->ctl_widget->w_nid != nid)
			continue;
		if (dir && ctl->ctl_ndir != dir)
			continue;
		if (index >= 0 && ctl->ctl_ndir == HDAUDIO_PINDIR_IN &&
		    ctl->ctl_dir == ctl->ctl_ndir && ctl->ctl_index != index)
			continue;
		found++;
		if (found == cnt || cnt <= 0)
			return ctl;
	}

	return NULL;
}

static void
hdafg_widget_connection_parse(struct hdaudio_widget *w)
{
	struct hdafg_softc *sc = w->w_afg;
	uint32_t res;
	int i, j, maxconns, ents, entnum;
	int cnid, addcnid, prevcnid;

	w->w_nconns = 0;

	res = hda_get_wparam(w, CONNECTION_LIST_LENGTH);
	ents = COP_CONNECTION_LIST_LENGTH_LEN(res);
	if (ents < 1)
		return;
	if (res & COP_CONNECTION_LIST_LENGTH_LONG_FORM)
		entnum = 2;
	else
		entnum = 4;
	maxconns = (sizeof(w->w_conns) / sizeof(w->w_conns[0])) - 1;
	prevcnid = 0;

#define	CONN_RMASK(e)		(1 << ((32 / (e)) - 1))
#define	CONN_NMASK(e)		(CONN_RMASK(e) - 1)
#define	CONN_RESVAL(r, e, n)	((r) >> ((32 / (e)) * (n)))
#define	CONN_RANGE(r, e, n)	(CONN_RESVAL(r, e, n) & CONN_RMASK(e))
#define	CONN_CNID(r, e, n)	(CONN_RESVAL(r, e, n) & CONN_NMASK(e))

	for (i = 0; i < ents; i += entnum) {
		res = hdaudio_command(sc->sc_codec, w->w_nid,
		    CORB_GET_CONNECTION_LIST_ENTRY, i);
		for (j = 0; j < entnum; j++) {
			cnid = CONN_CNID(res, entnum, j);
			if (cnid == 0) {
				if (w->w_nconns < ents) {
					hda_error(sc, "WARNING: zero cnid\n");
				} else {
					goto getconns_out;
				}
			}
			if (cnid < sc->sc_startnode || cnid >= sc->sc_endnode)
				hda_debug(sc, "ghost nid=%02X\n", cnid);
			if (CONN_RANGE(res, entnum, j) == 0)
				addcnid = cnid;
			else if (prevcnid == 0 || prevcnid >= cnid) {
				hda_error(sc, "invalid child range\n");
				addcnid = cnid;
			} else
				addcnid = prevcnid + 1;
			while (addcnid <= cnid) {
				if (w->w_nconns > maxconns) {
					hda_error(sc,
					    "max connections reached\n");
					goto getconns_out;
				} 
				w->w_connsenable[w->w_nconns] = true;
				w->w_conns[w->w_nconns++] = addcnid++;
				hda_trace(sc, "add connection %02X->%02X\n",
				    w->w_nid, addcnid - 1);
			}
			prevcnid = cnid;
		}
	}
#undef CONN_RMASK
#undef CONN_NMASK
#undef CONN_RESVAL
#undef CONN_RANGE
#undef CONN_CNID

getconns_out:
	return;
}

static void
hdafg_widget_pin_dump(struct hdafg_softc *sc)
{
	struct hdaudio_widget *w;
	int i, conn;

	for (i = sc->sc_startnode; i < sc->sc_endnode; i++) {
		w = hdafg_widget_lookup(sc, i);
		if (w == NULL || w->w_enable == false)
			continue;
		if (w->w_type != COP_AWCAP_TYPE_PIN_COMPLEX)
			continue;
		conn = COP_CFG_PORT_CONNECTIVITY(w->w_pin.config);
		if (conn != 1) {
#ifdef HDAUDIO_DEBUG
			int color = COP_CFG_COLOR(w->w_pin.config);
			int defdev = COP_CFG_DEFAULT_DEVICE(w->w_pin.config);
			hda_trace(sc, "io %02X: %s (%s, %s)\n",
			    w->w_nid,
			    hdafg_default_device[defdev],
			    hdafg_color[color],
			    hdafg_port_connectivity[conn]);
#endif
		}
	}
}

static void
hdafg_widget_setconfig(struct hdaudio_widget *w, uint32_t cfg)
{
	struct hdafg_softc *sc = w->w_afg;

	hdaudio_command(sc->sc_codec, w->w_nid,
	    CORB_SET_CONFIGURATION_DEFAULT_1, (cfg >>  0) & 0xff);
	hdaudio_command(sc->sc_codec, w->w_nid,
	    CORB_SET_CONFIGURATION_DEFAULT_2, (cfg >>  8) & 0xff);
	hdaudio_command(sc->sc_codec, w->w_nid,
	    CORB_SET_CONFIGURATION_DEFAULT_3, (cfg >> 16) & 0xff);
	hdaudio_command(sc->sc_codec, w->w_nid,
	    CORB_SET_CONFIGURATION_DEFAULT_4, (cfg >> 24) & 0xff);
}

static uint32_t
hdafg_widget_getconfig(struct hdaudio_widget *w)
{
	struct hdafg_softc *sc = w->w_afg;
	uint32_t config = 0;
	prop_object_iterator_t iter;
	prop_dictionary_t dict;
	prop_object_t obj;
	int16_t nid;

	if (sc->sc_config == NULL)
		goto biosconfig;

	iter = prop_array_iterator(sc->sc_config);
	if (iter == NULL)
		goto biosconfig;
	prop_object_iterator_reset(iter);
	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		if (prop_object_type(obj) != PROP_TYPE_DICTIONARY)
			continue;
		dict = (prop_dictionary_t)obj;
		if (!prop_dictionary_get_int16(dict, "nid", &nid) ||
		    !prop_dictionary_get_uint32(dict, "config", &config))
			continue;
		if (nid == w->w_nid)
			return config;
	}

biosconfig:
	return hdaudio_command(sc->sc_codec, w->w_nid,
	    CORB_GET_CONFIGURATION_DEFAULT, 0);
}

static void
hdafg_widget_pin_parse(struct hdaudio_widget *w)
{
	struct hdafg_softc *sc = w->w_afg;
	int conn, color, defdev;

	w->w_pin.cap = hda_get_wparam(w, PIN_CAPABILITIES);
	w->w_pin.config = hdafg_widget_getconfig(w);
	w->w_pin.biosconfig = hdaudio_command(sc->sc_codec, w->w_nid,
	    CORB_GET_CONFIGURATION_DEFAULT, 0);
	w->w_pin.ctrl = hdaudio_command(sc->sc_codec, w->w_nid,
	    CORB_GET_PIN_WIDGET_CONTROL, 0);

	/* treat line-out as speaker, unless connection type is RCA */
	if (COP_CFG_DEFAULT_DEVICE(w->w_pin.config) == COP_DEVICE_LINE_OUT &&
	    COP_CFG_CONNECTION_TYPE(w->w_pin.config) != COP_CONN_TYPE_RCA) {
		w->w_pin.config &= ~COP_DEVICE_MASK;
		w->w_pin.config |= (COP_DEVICE_SPEAKER << COP_DEVICE_SHIFT);
	}

	if (w->w_pin.cap & COP_PINCAP_EAPD_CAPABLE) {
		w->w_p.eapdbtl = hdaudio_command(sc->sc_codec, w->w_nid,
		    CORB_GET_EAPD_BTL_ENABLE, 0);
		w->w_p.eapdbtl &= 0x7;
		w->w_p.eapdbtl |= COP_EAPD_ENABLE_EAPD;
	} else
		w->w_p.eapdbtl = 0xffffffff;

#if 0
	/* XXX VT1708 */
	if (COP_CFG_DEFAULT_DEVICE(w->w_pin.config) == COP_DEVICE_SPEAKER &&
	    COP_CFG_DEFAULT_ASSOCIATION(w->w_pin.config) == 15) {
		hda_trace(sc, "forcing speaker nid %02X to assoc=14\n",
		    w->w_nid);
		/* set assoc=14 */
		w->w_pin.config &= ~0xf0;
		w->w_pin.config |= 0xe0;
	}
	if (COP_CFG_DEFAULT_DEVICE(w->w_pin.config) == COP_DEVICE_HP_OUT &&
	    COP_CFG_PORT_CONNECTIVITY(w->w_pin.config) == COP_PORT_NONE) {
		hda_trace(sc, "forcing hp out nid %02X to assoc=14\n",
		    w->w_nid);
		/* set connectivity to 'jack' */
		w->w_pin.config &= ~(COP_PORT_BOTH << 30);
		w->w_pin.config |= (COP_PORT_JACK << 30);
		/* set seq=15 */
		w->w_pin.config &= ~0xf;
		w->w_pin.config |= 15;
		/* set assoc=14 */
		w->w_pin.config &= ~0xf0;
		w->w_pin.config |= 0xe0;
	}
#endif

	conn = COP_CFG_PORT_CONNECTIVITY(w->w_pin.config);
	color = COP_CFG_COLOR(w->w_pin.config);
	defdev = COP_CFG_DEFAULT_DEVICE(w->w_pin.config);

	strlcat(w->w_name, ": ", sizeof(w->w_name));
	strlcat(w->w_name, hdafg_default_device[defdev], sizeof(w->w_name));
	strlcat(w->w_name, " (", sizeof(w->w_name));
	if (conn == 0 && color != 0 && color != 15) {
		strlcat(w->w_name, hdafg_color[color], sizeof(w->w_name));
		strlcat(w->w_name, " ", sizeof(w->w_name));
	}
	strlcat(w->w_name, hdafg_port_connectivity[conn], sizeof(w->w_name));
	strlcat(w->w_name, ")", sizeof(w->w_name));
}

static uint32_t
hdafg_widget_getcaps(struct hdaudio_widget *w)
{
	struct hdafg_softc *sc = w->w_afg;
	uint32_t wcap, config;
	bool pcbeep = false;

	wcap = hda_get_wparam(w, AUDIO_WIDGET_CAPABILITIES);
	config = hdafg_widget_getconfig(w);

	w->w_waspin = false;

	switch (sc->sc_vendor) {
	case HDAUDIO_VENDOR_ANALOG:
		/*
		 * help the parser by marking the analog
		 * beeper as a beep generator
		 */
		if (w->w_nid == 0x1a &&
		    COP_CFG_SEQUENCE(config) == 0x0 &&
		    COP_CFG_DEFAULT_ASSOCIATION(config) == 0xf &&
		    COP_CFG_PORT_CONNECTIVITY(config) ==
		      COP_PORT_FIXED_FUNCTION &&
		    COP_CFG_DEFAULT_DEVICE(config) ==
		      COP_DEVICE_OTHER) {
			pcbeep = true;
		}
		break;
	}

	if (pcbeep ||
	    (sc->sc_has_beepgen == false &&
	    COP_CFG_DEFAULT_DEVICE(config) == COP_DEVICE_SPEAKER &&
	    (wcap & (COP_AWCAP_INAMP_PRESENT|COP_AWCAP_OUTAMP_PRESENT)) == 0)) {
		wcap &= ~COP_AWCAP_TYPE_MASK;
		wcap |= (COP_AWCAP_TYPE_BEEP_GENERATOR << COP_AWCAP_TYPE_SHIFT);
		w->w_waspin = true;
	}

	return wcap;
}

static void
hdafg_widget_parse(struct hdaudio_widget *w)
{
	struct hdafg_softc *sc = w->w_afg;
	const char *tstr;

	w->w_p.aw_cap = hdafg_widget_getcaps(w);
	w->w_type = COP_AWCAP_TYPE(w->w_p.aw_cap);

	switch (w->w_type) {
	case COP_AWCAP_TYPE_AUDIO_OUTPUT:	tstr = "audio output"; break;
	case COP_AWCAP_TYPE_AUDIO_INPUT:	tstr = "audio input"; break;
	case COP_AWCAP_TYPE_AUDIO_MIXER:	tstr = "audio mixer"; break;
	case COP_AWCAP_TYPE_AUDIO_SELECTOR:	tstr = "audio selector"; break;
	case COP_AWCAP_TYPE_PIN_COMPLEX:	tstr = "pin"; break;
	case COP_AWCAP_TYPE_POWER_WIDGET:	tstr = "power widget"; break;
	case COP_AWCAP_TYPE_VOLUME_KNOB:	tstr = "volume knob"; break;
	case COP_AWCAP_TYPE_BEEP_GENERATOR:	tstr = "beep generator"; break;
	case COP_AWCAP_TYPE_VENDOR_DEFINED:	tstr = "vendor defined"; break;
	default:				tstr = "unknown"; break;
	}

	strlcpy(w->w_name, tstr, sizeof(w->w_name));

	hdafg_widget_connection_parse(w);

	if (w->w_p.aw_cap & COP_AWCAP_INAMP_PRESENT) {
		if (w->w_p.aw_cap & COP_AWCAP_AMP_PARAM_OVERRIDE)
			w->w_p.inamp_cap = hda_get_wparam(w,
			    AMPLIFIER_CAPABILITIES_INAMP);
		else
			w->w_p.inamp_cap = sc->sc_p.inamp_cap;
	}
	if (w->w_p.aw_cap & COP_AWCAP_OUTAMP_PRESENT) {
		if (w->w_p.aw_cap & COP_AWCAP_AMP_PARAM_OVERRIDE)
			w->w_p.outamp_cap = hda_get_wparam(w,
			    AMPLIFIER_CAPABILITIES_OUTAMP);
		else
			w->w_p.outamp_cap = sc->sc_p.outamp_cap;
	}

	w->w_p.stream_format = 0;
	w->w_p.pcm_size_rate = 0;
	switch (w->w_type) {
	case COP_AWCAP_TYPE_AUDIO_OUTPUT:
	case COP_AWCAP_TYPE_AUDIO_INPUT:
		if (w->w_p.aw_cap & COP_AWCAP_FORMAT_OVERRIDE) {
			w->w_p.stream_format = hda_get_wparam(w,
			    SUPPORTED_STREAM_FORMATS);
			w->w_p.pcm_size_rate = hda_get_wparam(w,
			    SUPPORTED_PCM_SIZE_RATES);
		} else {
			w->w_p.stream_format = sc->sc_p.stream_format;
			w->w_p.pcm_size_rate = sc->sc_p.pcm_size_rate;
		}
		break;
	case COP_AWCAP_TYPE_PIN_COMPLEX:
		hdafg_widget_pin_parse(w);
		hdafg_widget_setconfig(w, w->w_pin.config);
		break;
	}
}

static int
hdafg_assoc_count_channels(struct hdafg_softc *sc,
    struct hdaudio_assoc *as, enum hdaudio_pindir dir)
{
	struct hdaudio_widget *w;
	int *dacmap;
	int i, dacmapsz = sizeof(*dacmap) * sc->sc_endnode;
	int nchans = 0;

	if (as->as_enable == false || as->as_dir != dir)
		return 0;

	dacmap = kmem_zalloc(dacmapsz, KM_SLEEP);
	if (dacmap == NULL)
		return 0;

	for (i = 0; i < HDAUDIO_MAXPINS; i++)
		if (as->as_dacs[i])
			dacmap[as->as_dacs[i]] = 1;

	for (i = 1; i < sc->sc_endnode; i++) {
		if (!dacmap[i])
			continue;
		w = hdafg_widget_lookup(sc, i);
		if (w == NULL || w->w_enable == false)
			continue;
		nchans += COP_AWCAP_CHANNEL_COUNT(w->w_p.aw_cap);
	}

	kmem_free(dacmap, dacmapsz);

	return nchans;
}

static const char *
hdafg_assoc_type_string(struct hdaudio_assoc *as)
{
	switch (as->as_digital) {
	case HDAFG_AS_ANALOG:
		return as->as_dir == HDAUDIO_PINDIR_IN ?
		    "ADC" : "DAC";
	case HDAFG_AS_SPDIF:
		return as->as_dir == HDAUDIO_PINDIR_IN ?
		    "DIG-In" : "DIG";
	case HDAFG_AS_HDMI:
		return as->as_dir == HDAUDIO_PINDIR_IN ?
		    "HDMI-In" : "HDMI";
	case HDAFG_AS_DISPLAYPORT:
		return as->as_dir == HDAUDIO_PINDIR_IN ?
		    "DP-In" : "DP";
	default:
		return as->as_dir == HDAUDIO_PINDIR_IN ?
		    "Unknown-In" : "Unknown-Out";
	}
}

static void
hdafg_assoc_dump_dd(struct hdafg_softc *sc, struct hdaudio_assoc *as, int pin,
	int lock)
{
	struct hdafg_dd_info hdi;
	struct hdaudio_widget *w;
	uint8_t elddata[256];
	unsigned int elddatalen = 0, i;
	uint32_t res;
	uint32_t (*cmd)(struct hdaudio_codec *, int, uint32_t, uint32_t) =
	    lock ? hdaudio_command : hdaudio_command_unlocked;

	w = hdafg_widget_lookup(sc, as->as_pins[pin]);

	if (w->w_pin.cap & COP_PINCAP_TRIGGER_REQD) {
		(*cmd)(sc->sc_codec, as->as_pins[pin],
		    CORB_SET_PIN_SENSE, 0);
	}
	res = (*cmd)(sc->sc_codec, as->as_pins[pin],
	    CORB_GET_PIN_SENSE, 0);

#ifdef HDAFG_HDMI_DEBUG
	hda_print(sc, "Display Device, pin=%02X\n", as->as_pins[pin]);
	hda_print(sc, "  COP_GET_PIN_SENSE_PRESENSE_DETECT=%d\n",
	    !!(res & COP_GET_PIN_SENSE_PRESENSE_DETECT));
	hda_print(sc, "  COP_GET_PIN_SENSE_ELD_VALID=%d\n",
	    !!(res & COP_GET_PIN_SENSE_ELD_VALID));
#endif

	if ((res &
	    (COP_GET_PIN_SENSE_PRESENSE_DETECT|COP_GET_PIN_SENSE_ELD_VALID)) ==
	    (COP_GET_PIN_SENSE_PRESENSE_DETECT|COP_GET_PIN_SENSE_ELD_VALID)) {
		res = (*cmd)(sc->sc_codec, as->as_pins[pin],
		    CORB_GET_HDMI_DIP_SIZE, COP_DIP_ELD_SIZE);
		elddatalen = COP_DIP_BUFFER_SIZE(res);
		if (elddatalen == 0)
			elddatalen = sizeof(elddata); /* paranoid */
		for (i = 0; i < elddatalen; i++) {
			res = (*cmd)(sc->sc_codec, as->as_pins[pin],
			    CORB_GET_HDMI_ELD_DATA, i);
			if (!(res & COP_ELD_VALID)) {
				hda_error(sc, "bad ELD size (%u/%u)\n",
				    i, elddatalen);
				break;
			}
			elddata[i] = COP_ELD_DATA(res);
		}

		if (hdafg_dd_parse_info(elddata, elddatalen, &hdi) != 0) {
			hda_error(sc, "failed to parse ELD data\n");
			return;
		}

		hda_print(sc, "  ELD version=0x%x", ELD_VER(&hdi.eld));
		hda_print1(sc, ",len=%u", hdi.eld.header.baseline_eld_len * 4);
		hda_print1(sc, ",edid=0x%x", ELD_CEA_EDID_VER(&hdi.eld));
		hda_print1(sc, ",port=0x%" PRIx64, hdi.eld.port_id);
		hda_print1(sc, ",vendor=0x%04x", hdi.eld.vendor);
		hda_print1(sc, ",product=0x%04x", hdi.eld.product);
		hda_print1(sc, "\n");
		hda_print(sc, "  Monitor = '%s'\n", hdi.monitor);
		for (i = 0; i < hdi.nsad; i++) {
			hda_print(sc, "  SAD id=%u", i);
			hda_print1(sc, ",format=%u",
			    CEA_AUDIO_FORMAT(&hdi.sad[i]));
			hda_print1(sc, ",channels=%u",
			    CEA_MAX_CHANNELS(&hdi.sad[i]));
			hda_print1(sc, ",rate=0x%02x",
			    CEA_SAMPLE_RATE(&hdi.sad[i]));
			if (CEA_AUDIO_FORMAT(&hdi.sad[i]) ==
			    CEA_AUDIO_FORMAT_LPCM)
				hda_print1(sc, ",precision=0x%x",
				    CEA_PRECISION(&hdi.sad[i]));
			else
				hda_print1(sc, ",maxbitrate=%u",
				    CEA_MAX_BITRATE(&hdi.sad[i]));
			hda_print1(sc, "\n");
		}
	}
}

static char *
hdafg_mixer_mask2allname(uint32_t mask, char *buf, size_t len)
{
	static const char *audioname[] = HDAUDIO_DEVICE_NAMES;
	int i, first = 1;

	memset(buf, 0, len);
	for (i = 0; i < HDAUDIO_MIXER_NRDEVICES; i++) {
		if (mask & (1 << i)) {
			if (first == 0)
				strlcat(buf, ", ", len);
			strlcat(buf, audioname[i], len);
			first = 0;
		}
	}

	return buf;
}

static void
hdafg_dump_dst_nid(struct hdafg_softc *sc, int nid, int depth)
{
	struct hdaudio_widget *w, *cw;
	char buf[64];
	int i;

	if (depth > HDAUDIO_PARSE_MAXDEPTH)
		return;

	w = hdafg_widget_lookup(sc, nid);
	if (w == NULL || w->w_enable == false)
		return;

	aprint_debug("%*s", 4 + depth * 7, "");
	aprint_debug("nid=%02X [%s]", w->w_nid, w->w_name);

	if (depth > 0) {
		if (w->w_audiomask == 0) {
			aprint_debug("\n");
			return;
		}
		aprint_debug(" [source: %s]",
		    hdafg_mixer_mask2allname(w->w_audiomask, buf, sizeof(buf)));
		if (w->w_audiodev >= 0) {
			aprint_debug("\n");
			return;
		}
	}

	aprint_debug("\n");

	for (i = 0; i < w->w_nconns; i++) {
		if (w->w_connsenable[i] == 0)
			continue;
		cw = hdafg_widget_lookup(sc, w->w_conns[i]);
		if (cw == NULL || cw->w_enable == false || cw->w_bindas == -1)
			continue;
		hdafg_dump_dst_nid(sc, w->w_conns[i], depth + 1);
	}
}

static void
hdafg_assoc_dump(struct hdafg_softc *sc)
{
	struct hdaudio_assoc *as = sc->sc_assocs;
	struct hdaudio_widget *w;
	uint32_t conn, defdev, curdev, curport;
	int maxassocs = sc->sc_nassocs;
	int i, j;

	for (i = 0; i < maxassocs; i++) {
		uint32_t devmask = 0, portmask = 0;
		bool firstdev = true;
		int nchan;

		if (as[i].as_enable == false)
			continue;

		hda_print(sc, "%s%02X",
		    hdafg_assoc_type_string(&as[i]), i);

		nchan = hdafg_assoc_count_channels(sc, &as[i],
		    as[i].as_dir);
		hda_print1(sc, " %dch:", nchan);

		for (j = 0; j < HDAUDIO_MAXPINS; j++) {
			if (as[i].as_dacs[j] == 0)
				continue;
			w = hdafg_widget_lookup(sc, as[i].as_pins[j]);
			if (w == NULL)
				continue;
			conn = COP_CFG_PORT_CONNECTIVITY(w->w_pin.config);
			defdev = COP_CFG_DEFAULT_DEVICE(w->w_pin.config);
			if (conn != COP_PORT_NONE) {
				devmask |= (1 << defdev);
				portmask |= (1 << conn);
			}
		}
		for (curdev = 0; curdev < 16; curdev++) {
			bool firstport = true;
			if ((devmask & (1 << curdev)) == 0)
				continue;

			if (firstdev == false)
				hda_print1(sc, ",");
			firstdev = false;
			hda_print1(sc, " %s",
			    hdafg_default_device[curdev]);

			for (curport = 0; curport < 4; curport++) {
				bool devonport = false;
				if ((portmask & (1 << curport)) == 0)
					continue;

				for (j = 0; j < HDAUDIO_MAXPINS; j++) {
					if (as[i].as_dacs[j] == 0)
						continue;

					w = hdafg_widget_lookup(sc,
					    as[i].as_pins[j]);
					if (w == NULL)
						continue;
					conn = COP_CFG_PORT_CONNECTIVITY(w->w_pin.config);
					defdev = COP_CFG_DEFAULT_DEVICE(w->w_pin.config);
					if (conn != curport || defdev != curdev)
						continue;

					devonport = true;
				}

				if (devonport == false)
					continue;

				hda_print1(sc, " [%s",
				    hdafg_port_connectivity[curport]);
				for (j = 0; j < HDAUDIO_MAXPINS; j++) {
					if (as[i].as_dacs[j] == 0)
						continue;

					w = hdafg_widget_lookup(sc,
					    as[i].as_pins[j]);
					if (w == NULL)
						continue;
					conn = COP_CFG_PORT_CONNECTIVITY(w->w_pin.config);
					defdev = COP_CFG_DEFAULT_DEVICE(w->w_pin.config);
					if (conn != curport || defdev != curdev)
						continue;

					if (firstport == false)
						hda_trace1(sc, ",");
					else
						hda_trace1(sc, " ");
					firstport = false;
#ifdef HDAUDIO_DEBUG
					int color =
					    COP_CFG_COLOR(w->w_pin.config);
					hda_trace1(sc, "%s",
					    hdafg_color[color]);
#endif
					hda_trace1(sc, "(%02X)", w->w_nid);
				}
				hda_print1(sc, "]");
			}
		}
		hda_print1(sc, "\n");

		for (j = 0; j < HDAUDIO_MAXPINS; j++) {
			if (as[i].as_pins[j] == 0)
				continue;
			hdafg_dump_dst_nid(sc, as[i].as_pins[j], 0);
		}

		if (as[i].as_displaydev == true) {
			for (j = 0; j < HDAUDIO_MAXPINS; j++) {
				if (as[i].as_pins[j] == 0)
					continue;
				hdafg_assoc_dump_dd(sc, &as[i], j, 1);
			}
		}
	}
}

static void
hdafg_assoc_parse(struct hdafg_softc *sc)
{
	struct hdaudio_assoc *as;
	struct hdaudio_widget *w;
	int i, j, cnt, maxassocs, type, assoc, seq, first, hpredir;
	enum hdaudio_pindir dir;

	hda_debug(sc, "  count present associations\n");
	/* Count present associations */
	maxassocs = 0;
	for (j = 1; j < HDAUDIO_MAXPINS; j++) {
		for (i = sc->sc_startnode; i < sc->sc_endnode; i++) {
			w = hdafg_widget_lookup(sc, i);
			if (w == NULL || w->w_enable == false)
				continue;
			if (w->w_type != COP_AWCAP_TYPE_PIN_COMPLEX)
				continue;
			if (COP_CFG_DEFAULT_ASSOCIATION(w->w_pin.config) != j)
				continue;
			maxassocs++;
			if (j != 15) /* There could be many 1-pin assocs #15 */
				break;
		}
	}

	hda_debug(sc, "  maxassocs %d\n", maxassocs);
	sc->sc_nassocs = maxassocs;

	if (maxassocs < 1)
		return;

	hda_debug(sc, "  allocating memory\n");
	as = kmem_zalloc(maxassocs * sizeof(*as), KM_SLEEP);
	for (i = 0; i < maxassocs; i++) {
		as[i].as_hpredir = -1;
		/* as[i].as_chan = NULL; */
		as[i].as_digital = HDAFG_AS_SPDIF;
	}

	hda_debug(sc, "  scan associations, skipping as=0\n");
	/* Scan associations skipping as=0 */
	cnt = 0;
	for (j = 1; j < HDAUDIO_MAXPINS && cnt < maxassocs; j++) {
		first = 16;
		hpredir = 0;
		for (i = sc->sc_startnode; i < sc->sc_endnode; i++) {
			w = hdafg_widget_lookup(sc, i);
			if (w == NULL || w->w_enable == false)
				continue;
			if (w->w_type != COP_AWCAP_TYPE_PIN_COMPLEX)
				continue;
			assoc = COP_CFG_DEFAULT_ASSOCIATION(w->w_pin.config);
			seq = COP_CFG_SEQUENCE(w->w_pin.config);
			if (assoc != j)
				continue;
			KASSERT(cnt < maxassocs);
			type = COP_CFG_DEFAULT_DEVICE(w->w_pin.config);
			/* Get pin direction */
			switch (type) {
			case COP_DEVICE_LINE_OUT:
			case COP_DEVICE_SPEAKER:
			case COP_DEVICE_HP_OUT:
			case COP_DEVICE_SPDIF_OUT:
			case COP_DEVICE_DIGITAL_OTHER_OUT:
				dir = HDAUDIO_PINDIR_OUT;
				break;
			default:
				dir = HDAUDIO_PINDIR_IN;
				break;
			}
			/* If this is a first pin, create new association */
			if (as[cnt].as_pincnt == 0) {
				as[cnt].as_enable = true;
				as[cnt].as_activated = true;
				as[cnt].as_index = j;
				as[cnt].as_dir = dir;
			}
			if (seq < first)
				first = seq;
			/* Check association correctness */
			if (as[cnt].as_pins[seq] != 0) {
				hda_error(sc, "duplicate pin in association\n");
				as[cnt].as_enable = false;
			}
			if (dir != as[cnt].as_dir) {
				hda_error(sc,
				    "pin %02X has wrong direction for %02X\n",
				    w->w_nid, j);
				as[cnt].as_enable = false;
			}
			if ((w->w_p.aw_cap & COP_AWCAP_DIGITAL) == 0)
				as[cnt].as_digital = HDAFG_AS_ANALOG;
			if (w->w_pin.cap & (COP_PINCAP_HDMI|COP_PINCAP_DP))
				as[cnt].as_displaydev = true;
			if (w->w_pin.cap & COP_PINCAP_HDMI)
				as[cnt].as_digital = HDAFG_AS_HDMI;
			if (w->w_pin.cap & COP_PINCAP_DP)
				as[cnt].as_digital = HDAFG_AS_DISPLAYPORT;
			/* Headphones with seq=15 may mean redirection */
			if (type == COP_DEVICE_HP_OUT && seq == 15)
				hpredir = 1;
			as[cnt].as_pins[seq] = w->w_nid;
			as[cnt].as_pincnt++;
			if (j == 15)
				cnt++;
		}
		if (j != 15 && cnt < maxassocs && as[cnt].as_pincnt > 0) {
			if (hpredir && as[cnt].as_pincnt > 1)
				as[cnt].as_hpredir = first;
			cnt++;
		}
	}

	hda_debug(sc, "  all done\n");
	sc->sc_assocs = as;
}

static void
hdafg_control_parse(struct hdafg_softc *sc)
{
	struct hdaudio_control *ctl;
	struct hdaudio_widget *w, *cw;
	int i, j, cnt, maxctls, ocap, icap;
	int mute, offset, step, size;

	maxctls = 0;
	for (i = sc->sc_startnode; i < sc->sc_endnode; i++) {
		w = hdafg_widget_lookup(sc, i);
		if (w == NULL || w->w_enable == false)
			continue;
		if (w->w_p.outamp_cap)
			maxctls++;
		if (w->w_p.inamp_cap) {
			switch (w->w_type) {
			case COP_AWCAP_TYPE_AUDIO_SELECTOR:
			case COP_AWCAP_TYPE_AUDIO_MIXER:
				for (j = 0; j < w->w_nconns; j++) {
					cw = hdafg_widget_lookup(sc,
					    w->w_conns[j]);
					if (cw == NULL || cw->w_enable == false)
						continue;
					maxctls++;
				}
				break;
			default:
				maxctls++;
				break;
			}
		}
	}

	sc->sc_nctls = maxctls;
	if (maxctls < 1)
		return;

	ctl = kmem_zalloc(sc->sc_nctls * sizeof(*ctl), KM_SLEEP);

	cnt = 0;
	for (i = sc->sc_startnode; cnt < maxctls && i < sc->sc_endnode; i++) {
		if (cnt >= maxctls) {
			hda_error(sc, "ctl overflow\n");
			break;
		}
		w = hdafg_widget_lookup(sc, i);
		if (w == NULL || w->w_enable == false)
			continue;
		ocap = w->w_p.outamp_cap;
		icap = w->w_p.inamp_cap;
		if (ocap) {
			hda_trace(sc, "add ctrl outamp %d:%02X:FF\n",
			    cnt, w->w_nid);
			mute = COP_AMPCAP_MUTE_CAPABLE(ocap);
			step = COP_AMPCAP_NUM_STEPS(ocap);
			size = COP_AMPCAP_STEP_SIZE(ocap);
			offset = COP_AMPCAP_OFFSET(ocap);
			ctl[cnt].ctl_enable = true;
			ctl[cnt].ctl_widget = w;
			ctl[cnt].ctl_mute = mute;
			ctl[cnt].ctl_step = step;
			ctl[cnt].ctl_size = size;
			ctl[cnt].ctl_offset = offset;
			ctl[cnt].ctl_left = offset;
			ctl[cnt].ctl_right = offset;
			if (w->w_type == COP_AWCAP_TYPE_PIN_COMPLEX ||
			    w->w_waspin == true)
				ctl[cnt].ctl_ndir = HDAUDIO_PINDIR_IN;
			else
				ctl[cnt].ctl_ndir = HDAUDIO_PINDIR_OUT;
			ctl[cnt++].ctl_dir = HDAUDIO_PINDIR_OUT;
		}
		if (icap) {
			mute = COP_AMPCAP_MUTE_CAPABLE(icap);
			step = COP_AMPCAP_NUM_STEPS(icap);
			size = COP_AMPCAP_STEP_SIZE(icap);
			offset = COP_AMPCAP_OFFSET(icap);
			switch (w->w_type) {
			case COP_AWCAP_TYPE_AUDIO_SELECTOR:
			case COP_AWCAP_TYPE_AUDIO_MIXER:
				for (j = 0; j < w->w_nconns; j++) {
					if (cnt >= maxctls)
						break;
					cw = hdafg_widget_lookup(sc,
					    w->w_conns[j]);
					if (cw == NULL || cw->w_enable == false)
						continue;
					hda_trace(sc, "add ctrl inamp selmix "
					    "%d:%02X:%02X\n", cnt, w->w_nid,
					    cw->w_nid);
					ctl[cnt].ctl_enable = true;
					ctl[cnt].ctl_widget = w;
					ctl[cnt].ctl_childwidget = cw;
					ctl[cnt].ctl_index = j;
					ctl[cnt].ctl_mute = mute;
					ctl[cnt].ctl_step = step;
					ctl[cnt].ctl_size = size;
					ctl[cnt].ctl_offset = offset;
					ctl[cnt].ctl_left = offset;
					ctl[cnt].ctl_right = offset;
					ctl[cnt].ctl_ndir = HDAUDIO_PINDIR_IN;
					ctl[cnt++].ctl_dir = HDAUDIO_PINDIR_IN;
				}
				break;
			default:
				if (cnt >= maxctls)
					break;
				hda_trace(sc, "add ctrl inamp "
				    "%d:%02X:FF\n", cnt, w->w_nid);
				ctl[cnt].ctl_enable = true;
				ctl[cnt].ctl_widget = w;
				ctl[cnt].ctl_mute = mute;
				ctl[cnt].ctl_step = step;
				ctl[cnt].ctl_size = size;
				ctl[cnt].ctl_offset = offset;
				ctl[cnt].ctl_left = offset;
				ctl[cnt].ctl_right = offset;
				if (w->w_type == COP_AWCAP_TYPE_PIN_COMPLEX)
					ctl[cnt].ctl_ndir = HDAUDIO_PINDIR_OUT;
				else
					ctl[cnt].ctl_ndir = HDAUDIO_PINDIR_IN;
				ctl[cnt++].ctl_dir = HDAUDIO_PINDIR_IN;
				break;
			}
		}
	}

	sc->sc_ctls = ctl;
}

static void
hdafg_parse(struct hdafg_softc *sc)
{
	struct hdaudio_widget *w;
	uint32_t nodecnt, wcap;
	int nid;

	nodecnt = hda_get_param(sc, SUBORDINATE_NODE_COUNT);
	sc->sc_startnode = COP_NODECNT_STARTNODE(nodecnt);
	sc->sc_nwidgets = COP_NODECNT_NUMNODES(nodecnt);
	sc->sc_endnode = sc->sc_startnode + sc->sc_nwidgets;
	hda_debug(sc, "afg start %02X end %02X nwidgets %d\n",
	    sc->sc_startnode, sc->sc_endnode, sc->sc_nwidgets);

	hda_debug(sc, "powering up widgets\n");
	hdaudio_command(sc->sc_codec, sc->sc_nid,
	    CORB_SET_POWER_STATE, COP_POWER_STATE_D0);
	hda_delay(100);
	for (nid = sc->sc_startnode; nid < sc->sc_endnode; nid++)
		hdaudio_command(sc->sc_codec, nid,
		    CORB_SET_POWER_STATE, COP_POWER_STATE_D0);
	hda_delay(1000);

	sc->sc_p.afg_cap = hda_get_param(sc, AUDIO_FUNCTION_GROUP_CAPABILITIES);
	sc->sc_p.stream_format = hda_get_param(sc, SUPPORTED_STREAM_FORMATS);
	sc->sc_p.pcm_size_rate = hda_get_param(sc, SUPPORTED_PCM_SIZE_RATES);
	sc->sc_p.outamp_cap = hda_get_param(sc, AMPLIFIER_CAPABILITIES_OUTAMP);
	sc->sc_p.inamp_cap = hda_get_param(sc, AMPLIFIER_CAPABILITIES_INAMP);
	sc->sc_p.power_states = hda_get_param(sc, SUPPORTED_POWER_STATES);
	sc->sc_p.gpio_cnt = hda_get_param(sc, GPIO_COUNT);

	sc->sc_widgets = kmem_zalloc(sc->sc_nwidgets * sizeof(*w), KM_SLEEP);
	hda_debug(sc, "afg widgets %p-%p\n",
	    sc->sc_widgets, sc->sc_widgets + sc->sc_nwidgets);

	for (nid = sc->sc_startnode; nid < sc->sc_endnode; nid++) {
		w = hdafg_widget_lookup(sc, nid);
		if (w == NULL)
			continue;
		wcap = hdaudio_command(sc->sc_codec, nid, CORB_GET_PARAMETER,
		    COP_AUDIO_WIDGET_CAPABILITIES);
		switch (COP_AWCAP_TYPE(wcap)) {
		case COP_AWCAP_TYPE_BEEP_GENERATOR:
			sc->sc_has_beepgen = true;
			break;
		}
	}

	for (nid = sc->sc_startnode; nid < sc->sc_endnode; nid++) {
		w = hdafg_widget_lookup(sc, nid);
		if (w == NULL)
			continue;
		w->w_afg = sc;
		w->w_nid = nid;
		w->w_enable = true;
		w->w_pflags = 0;
		w->w_audiodev = -1;
		w->w_selconn = -1;
		w->w_bindas = -1;
		w->w_p.eapdbtl = 0xffffffff;
		hdafg_widget_parse(w);
	}
}

static void
hdafg_disable_nonaudio(struct hdafg_softc *sc)
{
	struct hdaudio_widget *w;
	int i;

	/* Disable power and volume widgets */
	for (i = sc->sc_startnode; i < sc->sc_endnode; i++) {
		w = hdafg_widget_lookup(sc, i);
		if (w == NULL || w->w_enable == false)
			continue;
		if (w->w_type == COP_AWCAP_TYPE_POWER_WIDGET ||
		    w->w_type == COP_AWCAP_TYPE_VOLUME_KNOB) {
			hda_trace(w->w_afg, "disable %02X [nonaudio]\n",
			    w->w_nid);
		    	w->w_enable = false;
		}
	}
}

static void
hdafg_disable_useless(struct hdafg_softc *sc)
{
	struct hdaudio_widget *w, *cw;
	struct hdaudio_control *ctl;
	int done, found, i, j, k;
	int conn, assoc;

	/* Disable useless pins */
	for (i = sc->sc_startnode; i < sc->sc_endnode; i++) {
		w = hdafg_widget_lookup(sc, i);
		if (w == NULL || w->w_enable == false)
			continue;
		if (w->w_type != COP_AWCAP_TYPE_PIN_COMPLEX)
			continue;
		conn = COP_CFG_PORT_CONNECTIVITY(w->w_pin.config);
		assoc = COP_CFG_DEFAULT_ASSOCIATION(w->w_pin.config);
		if (conn == COP_PORT_NONE) {
			hda_trace(w->w_afg, "disable %02X [no connectivity]\n",
			    w->w_nid);
			w->w_enable = false;
		}
		if (assoc == 0) {
			hda_trace(w->w_afg, "disable %02X [no association]\n",
			    w->w_nid);
			w->w_enable = false;
		}
	}

	do {
		done = 1;
		/* Disable and mute controls for disabled widgets */
		i = 0;
		for (i = 0; i < sc->sc_nctls; i++) {
			ctl = &sc->sc_ctls[i];
			if (ctl->ctl_enable == false)
				continue;
			if (ctl->ctl_widget->w_enable == false ||
			    (ctl->ctl_childwidget != NULL &&
			     ctl->ctl_childwidget->w_enable == false)) {
				ctl->ctl_forcemute = 1;
				ctl->ctl_muted = HDAUDIO_AMP_MUTE_ALL;
				ctl->ctl_left = ctl->ctl_right = 0;
				ctl->ctl_enable = false;
				if (ctl->ctl_ndir == HDAUDIO_PINDIR_IN)
					ctl->ctl_widget->w_connsenable[
					    ctl->ctl_index] = false;
				done = 0;
				hda_trace(ctl->ctl_widget->w_afg,
				    "disable ctl %d:%02X:%02X [widget disabled]\n",
				    i, ctl->ctl_widget->w_nid,
				    ctl->ctl_childwidget ?
				    ctl->ctl_childwidget->w_nid : 0xff);
			}
		}
		/* Disable useless widgets */
		for (i = sc->sc_startnode; i < sc->sc_endnode; i++) {
			w = hdafg_widget_lookup(sc, i);
			if (w == NULL || w->w_enable == false)
				continue;
			/* Disable inputs with disabled child widgets */
			for (j = 0; j < w->w_nconns; j++) {
				if (!w->w_connsenable[j])
					continue;
				cw = hdafg_widget_lookup(sc,
				    w->w_conns[j]);
				if (cw == NULL || cw->w_enable == false) {
					w->w_connsenable[j] = false;
					hda_trace(w->w_afg,
					    "disable conn %02X->%02X "
					    "[disabled child]\n",
					    w->w_nid, w->w_conns[j]);
				}
			}
			if (w->w_type != COP_AWCAP_TYPE_AUDIO_SELECTOR &&
			    w->w_type != COP_AWCAP_TYPE_AUDIO_MIXER)
				continue;
			/* Disable mixers and selectors without inputs */
			found = 0;
			for (j = 0; j < w->w_nconns; j++)
				if (w->w_connsenable[j]) {
					found = 1;
					break;
				}
			if (found == 0) {
				w->w_enable = false;
				done = 0;
				hda_trace(w->w_afg,
				    "disable %02X [inputs disabled]\n",
				    w->w_nid);
			}
			/* Disable nodes without consumers */
			if (w->w_type != COP_AWCAP_TYPE_AUDIO_SELECTOR &&
			    w->w_type != COP_AWCAP_TYPE_AUDIO_MIXER)
				continue;
			found = 0;
			for (k = sc->sc_startnode; k < sc->sc_endnode; k++) {
				cw = hdafg_widget_lookup(sc, k);
				if (cw == NULL || cw->w_enable == false)
					continue;
				for (j = 0; j < cw->w_nconns; j++) {
					if (cw->w_connsenable[j] &&
					    cw->w_conns[j] == i) {
						found = 1;
						break;
					}
				}
			}
			if (found == 0) {
				w->w_enable = false;
				done = 0;
				hda_trace(w->w_afg,
				    "disable %02X [consumers disabled]\n",
				    w->w_nid);
			}
		}
	} while (done == 0);
}

static void
hdafg_assoc_trace_undo(struct hdafg_softc *sc, int as, int seq)
{
	struct hdaudio_widget *w;
	int i;

	for (i = sc->sc_startnode; i < sc->sc_endnode; i++) {
		w = hdafg_widget_lookup(sc, i);
		if (w == NULL || w->w_enable == false)
			continue;
		if (w->w_bindas != as)
			continue;
		if (seq >= 0) {
			w->w_bindseqmask &= ~(1 << seq);
			if (w->w_bindseqmask == 0) {
				w->w_bindas = -1;
				w->w_selconn = -1;
			}
		} else {
			w->w_bindas = -1;
			w->w_bindseqmask = 0;
			w->w_selconn = -1;
		}
	}
}

static int
hdafg_assoc_trace_dac(struct hdafg_softc *sc, int as, int seq,
    int nid, int dupseq, int minassoc, int only, int depth)
{
	struct hdaudio_widget *w;
	int i, im = -1;
	int m = 0, ret;

	if (depth >= HDAUDIO_PARSE_MAXDEPTH)
		return 0;
	w = hdafg_widget_lookup(sc, nid);
	if (w == NULL || w->w_enable == false)
		return 0;
	/* We use only unused widgets */
	if (w->w_bindas >= 0 && w->w_bindas != as) {
		if (!only)
			hda_trace(sc, "depth %d nid %02X busy by assoc %d\n",
			    depth + 1, nid, w->w_bindas);
		return 0;
	}
	if (dupseq < 0) {
		if (w->w_bindseqmask != 0) {
			if (!only)
				hda_trace(sc,
				    "depth %d nid %02X busy by seqmask %x\n",
				    depth + 1, nid, w->w_bindas);
			return 0;
		}
	} else {
		/* If this is headphones, allow duplicate first pin */
		if (w->w_bindseqmask != 0 &&
		    (w->w_bindseqmask & (1 << dupseq)) == 0)
			return 0;
	}

	switch (w->w_type) {
	case COP_AWCAP_TYPE_AUDIO_INPUT:
		break;
	case COP_AWCAP_TYPE_AUDIO_OUTPUT:
		/* If we are tracing HP take only dac of first pin */
		if ((only == 0 || only == w->w_nid) &&
		    (w->w_nid >= minassoc) && (dupseq < 0 || w->w_nid ==
		    sc->sc_assocs[as].as_dacs[dupseq]))
			m = w->w_nid;
		break;
	case COP_AWCAP_TYPE_PIN_COMPLEX:
		if (depth > 0)
			break;
		/* FALLTHROUGH */
	default:
		for (i = 0; i < w->w_nconns; i++) {
			if (w->w_connsenable[i] == false)
				continue;
			if (w->w_selconn != -1 && w->w_selconn != i)
				continue;
			ret = hdafg_assoc_trace_dac(sc, as, seq,
			    w->w_conns[i], dupseq, minassoc, only, depth + 1);
			if (ret) {
				if (m == 0 || ret < m) {
					m = ret;
					im = i;
				}
				if (only || dupseq >= 0)
					break;
			}
		}
		if (m && only && ((w->w_nconns > 1 &&
		    w->w_type != COP_AWCAP_TYPE_AUDIO_MIXER) ||
		    w->w_type == COP_AWCAP_TYPE_AUDIO_SELECTOR))
			w->w_selconn = im;
		break;
	}
	if (m && only) {
		w->w_bindas = as;
		w->w_bindseqmask |= (1 << seq);
	}
	if (!only)
		hda_trace(sc, "depth %d nid %02X dupseq %d returned %02X\n",
		    depth + 1, nid, dupseq, m);

	return m;
}

static int 
hdafg_assoc_trace_out(struct hdafg_softc *sc, int as, int seq)
{
	struct hdaudio_assoc *assocs = sc->sc_assocs;
	int i, hpredir;
	int minassoc, res;

	/* Find next pin */
	for (i = seq; i < HDAUDIO_MAXPINS && assocs[as].as_pins[i] == 0; i++)
		;
	/* Check if there is any left, if not then we have succeeded */
	if (i == HDAUDIO_MAXPINS)
		return 1;

	hpredir = (i == 15 && assocs[as].as_fakeredir == 0) ?
	    assocs[as].as_hpredir : -1;
	minassoc = res = 0;
	do {
		/* Trace this pin taking min nid into account */
		res = hdafg_assoc_trace_dac(sc, as, i,
		    assocs[as].as_pins[i], hpredir, minassoc, 0, 0);
		if (res == 0) {
			/* If we failed, return to previous and redo it */
			hda_trace(sc, "  trace failed as=%d seq=%d pin=%02X "
			    "hpredir=%d minassoc=%d\n",
			    as, seq, assocs[as].as_pins[i], hpredir, minassoc);
			return 0;
		}
		/* Trace again to mark the path */
		hdafg_assoc_trace_dac(sc, as, i,
		    assocs[as].as_pins[i], hpredir, minassoc, res, 0);
		assocs[as].as_dacs[i] = res;
		/* We succeeded, so call next */
		if (hdafg_assoc_trace_out(sc, as, i + 1))
			return 1;
		/* If next failed, we should retry with next min */
		hdafg_assoc_trace_undo(sc, as, i);
		assocs[as].as_dacs[i] = 0;
		minassoc = res + 1;
	} while (1);
}

static int
hdafg_assoc_trace_adc(struct hdafg_softc *sc, int assoc, int seq,
    int nid, int only, int depth)
{
	struct hdaudio_widget *w, *wc;
	int i, j;
	int res = 0;

	if (depth > HDAUDIO_PARSE_MAXDEPTH)
		return 0;
	w = hdafg_widget_lookup(sc, nid);
	if (w == NULL || w->w_enable == false)
		return 0;
	/* Use only unused widgets */
	if (w->w_bindas >= 0 && w->w_bindas != assoc)
		return 0;

	switch (w->w_type) {
	case COP_AWCAP_TYPE_AUDIO_INPUT:
		if (only == w->w_nid)
			res = 1;
		break;
	case COP_AWCAP_TYPE_PIN_COMPLEX:
		if (depth > 0)
			break;
		/* FALLTHROUGH */
	default:
		/* Try to find reachable ADCs with specified nid */
		for (j = sc->sc_startnode; j < sc->sc_endnode; j++) {
			wc = hdafg_widget_lookup(sc, j);
			if (w == NULL || w->w_enable == false)
				continue;
			for (i = 0; i < wc->w_nconns; i++) {
				if (wc->w_connsenable[i] == false)
					continue;
				if (wc->w_conns[i] != nid)
					continue;
				if (hdafg_assoc_trace_adc(sc, assoc, seq,
				    j, only, depth + 1) != 0) {
					res = 1;
					if (((wc->w_nconns > 1 &&
					    wc->w_type != COP_AWCAP_TYPE_AUDIO_MIXER) ||
					    wc->w_type != COP_AWCAP_TYPE_AUDIO_SELECTOR)
					    && wc->w_selconn == -1)
						wc->w_selconn = i;
				}
			}
		}
		break;
	}
	if (res) {
		w->w_bindas = assoc;
		w->w_bindseqmask |= (1 << seq);
	}
	return res;
}

static int
hdafg_assoc_trace_in(struct hdafg_softc *sc, int assoc)
{
	struct hdaudio_assoc *as = sc->sc_assocs;
	struct hdaudio_widget *w;
	int i, j, k;

	for (j = sc->sc_startnode; j < sc->sc_endnode; j++) {
		w = hdafg_widget_lookup(sc, j);
		if (w == NULL || w->w_enable == false)
			continue;
		if (w->w_type != COP_AWCAP_TYPE_AUDIO_INPUT)
			continue;
		if (w->w_bindas >= 0 && w->w_bindas != assoc)
			continue;

		/* Find next pin */
		for (i = 0; i < HDAUDIO_MAXPINS; i++) {
			if (as[assoc].as_pins[i] == 0)
				continue;
			/* Trace this pin taking goal into account */
			if (hdafg_assoc_trace_adc(sc, assoc, i,
			    as[assoc].as_pins[i], j, 0) == 0) {
				hdafg_assoc_trace_undo(sc, assoc, -1);
				for (k = 0; k < HDAUDIO_MAXPINS; k++)
					as[assoc].as_dacs[k] = 0;
				break;
			}
			as[assoc].as_dacs[i] = j;
		}
		if (i == HDAUDIO_MAXPINS)
			return 1;
	}
	return 0;
}

static int
hdafg_assoc_trace_to_out(struct hdafg_softc *sc, int nid, int depth)
{
	struct hdaudio_assoc *as = sc->sc_assocs;
	struct hdaudio_widget *w, *wc;
	int i, j;
	int res = 0;

	if (depth > HDAUDIO_PARSE_MAXDEPTH)
		return 0;
	w = hdafg_widget_lookup(sc, nid);
	if (w == NULL || w->w_enable == false)
		return 0;

	/* Use only unused widgets */
	if (depth > 0 && w->w_bindas != -1) {
		if (w->w_bindas < 0 ||
		    as[w->w_bindas].as_dir == HDAUDIO_PINDIR_OUT) {
			return 1;
		} else {
			return 0;
		}
	}

	switch (w->w_type) {
	case COP_AWCAP_TYPE_AUDIO_INPUT:
		/* Do not traverse input (not yet supported) */
		break;
	case COP_AWCAP_TYPE_PIN_COMPLEX:
		if (depth > 0)
			break;
		/* FALLTHROUGH */
	default:
		/* Try to find reachable ADCs with specified nid */
		for (j = sc->sc_startnode; j < sc->sc_endnode; j++) {
			wc = hdafg_widget_lookup(sc, j);
			if (wc == NULL || wc->w_enable == false)
				continue;
			for (i = 0; i < wc->w_nconns; i++) {
				if (wc->w_connsenable[i] == false)
					continue;
				if (wc->w_conns[i] != nid)
					continue;
				if (hdafg_assoc_trace_to_out(sc,
				    j, depth + 1) != 0) {
					res = 1;
					if (wc->w_type ==
					    COP_AWCAP_TYPE_AUDIO_SELECTOR &&
					    wc->w_selconn == -1)
						wc->w_selconn = i;
				}
			}
		}
		break;
	}
	if (res)
		w->w_bindas = -2;
	return res;
}

static void
hdafg_assoc_trace_misc(struct hdafg_softc *sc)
{
	struct hdaudio_assoc *as = sc->sc_assocs;
	struct hdaudio_widget *w;
	int j;

	/* Input monitor */
	/*
	 * Find mixer associated with input, but supplying signal
	 * for output associations. Hope it will be input monitor.
	 */
	for (j = sc->sc_startnode; j < sc->sc_endnode; j++) {
		w = hdafg_widget_lookup(sc, j);
		if (w == NULL || w->w_enable == false)
			continue;
		if (w->w_type != COP_AWCAP_TYPE_AUDIO_MIXER)
			continue;
		if (w->w_bindas < 0 ||
		    as[w->w_bindas].as_dir != HDAUDIO_PINDIR_IN)
			continue;
		if (hdafg_assoc_trace_to_out(sc, w->w_nid, 0)) {
			w->w_pflags |= HDAUDIO_ADC_MONITOR;
			w->w_audiodev = HDAUDIO_MIXER_IMIX;
		}
	}

	/* Beeper */
	for (j = sc->sc_startnode; j < sc->sc_endnode; j++) {
		w = hdafg_widget_lookup(sc, j);
		if (w == NULL || w->w_enable == false)
			continue;
		if (w->w_type != COP_AWCAP_TYPE_BEEP_GENERATOR)
			continue;
		if (hdafg_assoc_trace_to_out(sc, w->w_nid, 0)) {
			hda_debug(sc, "beeper %02X traced to out\n", w->w_nid);
		}
		w->w_bindas = -2;
	}
}

static void
hdafg_build_tree(struct hdafg_softc *sc)
{
	struct hdaudio_assoc *as = sc->sc_assocs;
	int i, j, res;

	/* Trace all associations in order of their numbers */

	/* Trace DACs first */
	for (j = 0; j < sc->sc_nassocs; j++) {
		if (as[j].as_enable == false)
			continue;
		if (as[j].as_dir != HDAUDIO_PINDIR_OUT)
			continue;
retry:
		res = hdafg_assoc_trace_out(sc, j, 0);
		if (res == 0 && as[j].as_hpredir >= 0 &&
		    as[j].as_fakeredir == 0) {
		    	/*
		    	 * If codec can't do analog HP redirection
			 * try to make it using one more DAC
			 */
			as[j].as_fakeredir = 1;
			goto retry;
		}
		if (!res) {
			hda_debug(sc, "disable assoc %d (%d) [trace failed]\n",
			    j, as[j].as_index);
			for (i = 0; i < HDAUDIO_MAXPINS; i++) {
				if (as[j].as_pins[i] == 0)
					continue;
				hda_debug(sc, "  assoc %d pin%d: %02X\n", j, i,
				    as[j].as_pins[i]);
			}
			for (i = 0; i < HDAUDIO_MAXPINS; i++) {
				if (as[j].as_dacs[i] == 0)
					continue;
				hda_debug(sc, "  assoc %d dac%d: %02X\n", j, i,
				    as[j].as_dacs[i]);
			}

			as[j].as_enable = false;
		}
	}

	/* Trace ADCs */
	for (j = 0; j < sc->sc_nassocs; j++) {
		if (as[j].as_enable == false)
			continue;
		if (as[j].as_dir != HDAUDIO_PINDIR_IN)
			continue;
		res = hdafg_assoc_trace_in(sc, j);
		if (!res) {
			hda_debug(sc, "disable assoc %d (%d) [trace failed]\n",
			    j, as[j].as_index);
			for (i = 0; i < HDAUDIO_MAXPINS; i++) {
				if (as[j].as_pins[i] == 0)
					continue;
				hda_debug(sc, "  assoc %d pin%d: %02X\n", j, i,
				    as[j].as_pins[i]);
			}
			for (i = 0; i < HDAUDIO_MAXPINS; i++) {
				if (as[j].as_dacs[i] == 0)
					continue;
				hda_debug(sc, "  assoc %d adc%d: %02X\n", j, i,
				    as[j].as_dacs[i]);
			}

			as[j].as_enable = false;
		}
	}

	/* Trace mixer and beeper pseudo associations */
	hdafg_assoc_trace_misc(sc);
}

static void
hdafg_prepare_pin_controls(struct hdafg_softc *sc)
{
	struct hdaudio_assoc *as = sc->sc_assocs;
	struct hdaudio_widget *w;
	uint32_t pincap;
	int i;

	hda_debug(sc, "*** prepare pin controls, nwidgets = %d\n",
	    sc->sc_nwidgets);

	for (i = 0; i < sc->sc_nwidgets; i++) {
		w = &sc->sc_widgets[i];
		if (w->w_type != COP_AWCAP_TYPE_PIN_COMPLEX) {
			hda_debug(sc, "  skipping pin %02X type 0x%x\n",
			    w->w_nid, w->w_type);
			continue;
		}
		pincap = w->w_pin.cap;

		/* Disable everything */
		w->w_pin.ctrl &= ~(
		    COP_PWC_VREF_ENABLE_MASK |
		    COP_PWC_IN_ENABLE |
		    COP_PWC_OUT_ENABLE |
		    COP_PWC_HPHN_ENABLE);

		if (w->w_enable == false ||
		    w->w_bindas < 0 || as[w->w_bindas].as_enable == false) {
			/* Pin is unused so leave it disabled */
			if ((pincap & (COP_PINCAP_OUTPUT_CAPABLE |
			    COP_PINCAP_INPUT_CAPABLE)) ==
			    (COP_PINCAP_OUTPUT_CAPABLE |
			    COP_PINCAP_INPUT_CAPABLE)) {
				hda_debug(sc, "pin %02X off, "
				    "in/out capable (bindas=%d "
				    "enable=%d as_enable=%d)\n",
				    w->w_nid, w->w_bindas, w->w_enable,
				    w->w_bindas >= 0 ?
				    as[w->w_bindas].as_enable : -1);
				w->w_pin.ctrl |= COP_PWC_OUT_ENABLE;
			} else
				hda_debug(sc, "pin %02X off\n", w->w_nid);
			continue;
		} else if (as[w->w_bindas].as_dir == HDAUDIO_PINDIR_IN) {
			/* Input pin, configure for input */
			if (pincap & COP_PINCAP_INPUT_CAPABLE)
				w->w_pin.ctrl |= COP_PWC_IN_ENABLE;

			hda_debug(sc, "pin %02X in ctrl 0x%x\n", w->w_nid,
			    w->w_pin.ctrl);

			if (COP_CFG_DEFAULT_DEVICE(w->w_pin.config) !=
			    COP_DEVICE_MIC_IN)
				continue;
			if (COP_PINCAP_VREF_CONTROL(pincap) & COP_VREF_80)
				w->w_pin.ctrl |= COP_PWC_VREF_80;
			else if (COP_PINCAP_VREF_CONTROL(pincap) & COP_VREF_50)
				w->w_pin.ctrl |= COP_PWC_VREF_50;
		} else {
			/* Output pin, configure for output */
			if (pincap & COP_PINCAP_OUTPUT_CAPABLE)
				w->w_pin.ctrl |= COP_PWC_OUT_ENABLE;
			if ((pincap & COP_PINCAP_HEADPHONE_DRIVE_CAPABLE) &&
			    (COP_CFG_DEFAULT_DEVICE(w->w_pin.config) ==
			    COP_DEVICE_HP_OUT))
				w->w_pin.ctrl |= COP_PWC_HPHN_ENABLE;
			/* XXX VREF */
			hda_debug(sc, "pin %02X out ctrl 0x%x\n", w->w_nid,
			    w->w_pin.ctrl);
		}
	}
}

static void
hdafg_dump(struct hdafg_softc *sc)
{
#if defined(HDAFG_DEBUG) && HDAFG_DEBUG > 1
	struct hdaudio_control *ctl;
	int i, type;

	for (i = 0; i < sc->sc_nctls; i++) {
		ctl = &sc->sc_ctls[i];
		type = (ctl->ctl_widget ? ctl->ctl_widget->w_type : -1);
		hda_print(sc, "%03X: nid %02X type %d %s (%s) index %d",
		    i, (ctl->ctl_widget ? ctl->ctl_widget->w_nid : -1), type,
		    (ctl->ctl_ndir == HDAUDIO_PINDIR_IN) ? "in " : "out",
		    (ctl->ctl_dir == HDAUDIO_PINDIR_IN) ? "in " : "out",
		    ctl->ctl_index);
		if (ctl->ctl_childwidget)
			hda_print1(sc, " cnid %02X",
			    ctl->ctl_childwidget->w_nid);
		else
			hda_print1(sc, "          ");
		hda_print1(sc, "\n");
		hda_print(sc, "     mute: %d step: %3d size: %3d off: %3d%s\n",
		    ctl->ctl_mute, ctl->ctl_step, ctl->ctl_size,
		    ctl->ctl_offset,
		    (ctl->ctl_enable == false) ? " [DISABLED]" : "");
	}
#endif
}

static int
hdafg_match(device_t parent, cfdata_t match, void *opaque)
{
	prop_dictionary_t args = opaque;
	uint8_t fgtype;
	bool rv;

	rv = prop_dictionary_get_uint8(args, "function-group-type", &fgtype);
	if (rv == false || fgtype != HDAUDIO_GROUP_TYPE_AFG)
		return 0;

	return 1;
}

static void
hdafg_disable_unassoc(struct hdafg_softc *sc)
{
	struct hdaudio_assoc *as = sc->sc_assocs;
	struct hdaudio_widget *w, *cw;
	struct hdaudio_control *ctl;
	int i, j, k;

	/* Disable unassociated widgets */
	for (i = sc->sc_startnode; i < sc->sc_endnode; i++) {
		w = hdafg_widget_lookup(sc, i);
		if (w == NULL || w->w_enable == false)
			continue;
		if (w->w_bindas == -1) {
			w->w_enable = 0;
			hda_trace(sc, "disable %02X [unassociated]\n",
			    w->w_nid);
		}
	}

	/* Disable input connections on input pin and output on output */
	for (i = sc->sc_startnode; i < sc->sc_endnode; i++) {
		w = hdafg_widget_lookup(sc, i);
		if (w == NULL || w->w_enable == false)
			continue;
		if (w->w_type != COP_AWCAP_TYPE_PIN_COMPLEX)
			continue;
		if (w->w_bindas < 0)
			continue;
		if (as[w->w_bindas].as_dir == HDAUDIO_PINDIR_IN) {
			hda_trace(sc, "disable %02X input connections\n",
			    w->w_nid);
			for (j = 0; j < w->w_nconns; j++)
				w->w_connsenable[j] = false;
			ctl = hdafg_control_lookup(sc, w->w_nid,
			    HDAUDIO_PINDIR_IN, -1, 1);
			if (ctl && ctl->ctl_enable == true) {
				ctl->ctl_forcemute = 1;
				ctl->ctl_muted = HDAUDIO_AMP_MUTE_ALL;
				ctl->ctl_left = ctl->ctl_right = 0;
				ctl->ctl_enable = false;
			}
		} else {
			ctl = hdafg_control_lookup(sc, w->w_nid,
			    HDAUDIO_PINDIR_OUT, -1, 1);
			if (ctl && ctl->ctl_enable == true) {
				ctl->ctl_forcemute = 1;
				ctl->ctl_muted = HDAUDIO_AMP_MUTE_ALL;
				ctl->ctl_left = ctl->ctl_right = 0;
				ctl->ctl_enable = false;
			}
			for (k = sc->sc_startnode; k < sc->sc_endnode; k++) {
				cw = hdafg_widget_lookup(sc, k);
				if (cw == NULL || cw->w_enable == false)
					continue;
				for (j = 0; j < cw->w_nconns; j++) {
					if (!cw->w_connsenable[j])
						continue;
					if (cw->w_conns[j] != i)
						continue;
					hda_trace(sc, "disable %02X -> %02X "
					    "output connection\n",
					    cw->w_nid, cw->w_conns[j]);
					cw->w_connsenable[j] = false;
					if (cw->w_type ==
					    COP_AWCAP_TYPE_PIN_COMPLEX &&
					    cw->w_nconns > 1)
						continue;
					ctl = hdafg_control_lookup(sc,
					    k, HDAUDIO_PINDIR_IN, j, 1);
					if (ctl && ctl->ctl_enable == true) {
						ctl->ctl_forcemute = 1;
						ctl->ctl_muted =
						    HDAUDIO_AMP_MUTE_ALL;
						ctl->ctl_left =
						    ctl->ctl_right = 0;
						ctl->ctl_enable = false;
					}
				}
			}
		}
	}
}

static void
hdafg_disable_unsel(struct hdafg_softc *sc)
{
	struct hdaudio_assoc *as = sc->sc_assocs;
	struct hdaudio_widget *w;
	int i, j;

	/* On playback path we can safely disable all unselected inputs */
	for (i = sc->sc_startnode; i < sc->sc_endnode; i++) {
		w = hdafg_widget_lookup(sc, i);
		if (w == NULL || w->w_enable == false)
			continue;
		if (w->w_nconns <= 1)
			continue;
		if (w->w_type == COP_AWCAP_TYPE_AUDIO_MIXER)
			continue;
		if (w->w_bindas < 0 ||
		    as[w->w_bindas].as_dir == HDAUDIO_PINDIR_IN)
			continue;
		for (j = 0; j < w->w_nconns; j++) {
			if (w->w_connsenable[j] == false)
				continue;
			if (w->w_selconn < 0 || w->w_selconn == j)
				continue;
			hda_trace(sc, "disable %02X->%02X [unselected]\n",
			    w->w_nid, w->w_conns[j]);
			w->w_connsenable[j] = false;
		}
	}
}

static void
hdafg_disable_crossassoc(struct hdafg_softc *sc)
{
	struct hdaudio_widget *w, *cw;
	struct hdaudio_control *ctl;
	int i, j;

	/* Disable cross associated and unwanted cross channel connections */

	/* ... using selectors */
	for (i = sc->sc_startnode; i < sc->sc_endnode; i++) {
		w = hdafg_widget_lookup(sc, i);
		if (w == NULL || w->w_enable == false)
			continue;
		if (w->w_nconns <= 1)
			continue;
		if (w->w_type == COP_AWCAP_TYPE_AUDIO_MIXER)
			continue;
		if (w->w_bindas == -2)
			continue;
		for (j = 0; j < w->w_nconns; j++) {
			if (w->w_connsenable[j] == false)
				continue;
			cw = hdafg_widget_lookup(sc, w->w_conns[j]);
			if (cw == NULL || cw->w_enable == false)
				continue;
			if (cw->w_bindas == -2)
				continue;
			if (w->w_bindas == cw->w_bindas &&
			    (w->w_bindseqmask & cw->w_bindseqmask) != 0)
				continue;
			hda_trace(sc, "disable %02X->%02X [crossassoc]\n",
			    w->w_nid, w->w_conns[j]);
			w->w_connsenable[j] = false;
		}
	}
	/* ... using controls */
	for (i = 0; i < sc->sc_nctls; i++) {
		ctl = &sc->sc_ctls[i];
		if (ctl->ctl_enable == false || ctl->ctl_childwidget == NULL)
			continue;
		if (ctl->ctl_widget->w_bindas == -2 ||
		    ctl->ctl_childwidget->w_bindas == -2)
			continue;
		if (ctl->ctl_widget->w_bindas !=
		    ctl->ctl_childwidget->w_bindas ||
		    (ctl->ctl_widget->w_bindseqmask &
		    ctl->ctl_childwidget->w_bindseqmask) == 0) {
			ctl->ctl_forcemute = 1;
			ctl->ctl_muted = HDAUDIO_AMP_MUTE_ALL;
			ctl->ctl_left = ctl->ctl_right = 0;
			ctl->ctl_enable = false;
			if (ctl->ctl_ndir == HDAUDIO_PINDIR_IN) {
				hda_trace(sc, "disable ctl %d:%02X:%02X "
				    "[crossassoc]\n",
			    	    i, ctl->ctl_widget->w_nid,
				    ctl->ctl_widget->w_conns[ctl->ctl_index]);
				ctl->ctl_widget->w_connsenable[
				    ctl->ctl_index] = false;
			}
		}
	}
}

static struct hdaudio_control *
hdafg_control_amp_get(struct hdafg_softc *sc, int nid,
    enum hdaudio_pindir dir, int index, int cnt)
{
	struct hdaudio_control *ctl;
	int i, found = 0;

	for (i = 0; i < sc->sc_nctls; i++) {
		ctl = &sc->sc_ctls[i];
		if (ctl->ctl_enable == false)
			continue;
		if (ctl->ctl_widget->w_nid != nid)
			continue;
		if (dir && ctl->ctl_ndir != dir)
			continue;
		if (index >= 0 && ctl->ctl_ndir == HDAUDIO_PINDIR_IN &&
		    ctl->ctl_dir == ctl->ctl_ndir &&
		    ctl->ctl_index != index)
			continue;
		++found;
		if (found == cnt || cnt <= 0)
			return ctl;
	}

	return NULL;
}

static void
hdafg_control_amp_set1(struct hdaudio_control *ctl, int lmute, int rmute,
    int left, int right, int dir)
{
	struct hdafg_softc *sc = ctl->ctl_widget->w_afg;
	int index = ctl->ctl_index;
	uint16_t v = 0;

	if (left != right || lmute != rmute) {
		v = (1 << (15 - dir)) | (1 << 13) | (index << 8) |
		    (lmute << 7) | left;
		hdaudio_command(sc->sc_codec, ctl->ctl_widget->w_nid,
		    CORB_SET_AMPLIFIER_GAIN_MUTE, v);
		v = (1 << (15 - dir)) | (1 << 12) | (index << 8) |
		    (rmute << 7) | right;
	} else
		v = (1 << (15 - dir)) | (3 << 12) | (index << 8) |
		    (lmute << 7) | left;
	hdaudio_command(sc->sc_codec, ctl->ctl_widget->w_nid,
	    CORB_SET_AMPLIFIER_GAIN_MUTE, v);
}

static void
hdafg_control_amp_set(struct hdaudio_control *ctl, uint32_t mute,
    int left, int right)
{
	int lmute, rmute;

	/* Save new values if valid */
	if (mute != HDAUDIO_AMP_MUTE_DEFAULT)
		ctl->ctl_muted = mute;
	if (left != HDAUDIO_AMP_VOL_DEFAULT)
		ctl->ctl_left = left;
	if (right != HDAUDIO_AMP_VOL_DEFAULT)
		ctl->ctl_right = right;

	/* Prepare effective values */
	if (ctl->ctl_forcemute) {
		lmute = rmute = 1;
		left = right = 0;
	} else {
		lmute = HDAUDIO_AMP_LEFT_MUTED(ctl->ctl_muted);
		rmute = HDAUDIO_AMP_RIGHT_MUTED(ctl->ctl_muted);
		left = ctl->ctl_left;
		right = ctl->ctl_right;
	}

	/* Apply effective values */
	if (ctl->ctl_dir & HDAUDIO_PINDIR_OUT)
		hdafg_control_amp_set1(ctl, lmute, rmute, left, right, 0);
	if (ctl->ctl_dir & HDAUDIO_PINDIR_IN)
		hdafg_control_amp_set1(ctl, lmute, rmute, left, right, 1);
}

static void
hdafg_control_commit(struct hdafg_softc *sc)
{
	struct hdaudio_control *ctl;
	int i, z;

	for (i = 0; i < sc->sc_nctls; i++) {
		ctl = &sc->sc_ctls[i];
		//if (ctl->ctl_enable == false || ctl->ctl_audiomask != 0)
		if (ctl->ctl_enable == false)
			continue;
		/* Init fixed controls to 0dB amplification */
		z = ctl->ctl_offset;
		if (z > ctl->ctl_step)
			z = ctl->ctl_step;
		hdafg_control_amp_set(ctl, HDAUDIO_AMP_MUTE_NONE, z, z);
	}
}

static void
hdafg_widget_connection_select(struct hdaudio_widget *w, uint8_t index)
{
	struct hdafg_softc *sc = w->w_afg;

	if (w->w_nconns < 1 || index > (w->w_nconns - 1))
		return;

	hdaudio_command(sc->sc_codec, w->w_nid,
	    CORB_SET_CONNECTION_SELECT_CONTROL, index);
	w->w_selconn = index;
}

static void
hdafg_assign_names(struct hdafg_softc *sc)
{
	struct hdaudio_assoc *as = sc->sc_assocs;
	struct hdaudio_widget *w;
	int i, j;
	int type = -1, use, used =0;
	static const int types[7][13] = {
	    { HDAUDIO_MIXER_LINE, HDAUDIO_MIXER_LINE1, HDAUDIO_MIXER_LINE2,
	      HDAUDIO_MIXER_LINE3, -1 },
	    { HDAUDIO_MIXER_MONITOR, HDAUDIO_MIXER_MIC, -1 }, /* int mic */
	    { HDAUDIO_MIXER_MIC, HDAUDIO_MIXER_MONITOR, -1 }, /* ext mic */
	    { HDAUDIO_MIXER_CD, -1 },
	    { HDAUDIO_MIXER_SPEAKER, -1 },
	    { HDAUDIO_MIXER_DIGITAL1, HDAUDIO_MIXER_DIGITAL2,
	      HDAUDIO_MIXER_DIGITAL3, -1 },
	    { HDAUDIO_MIXER_LINE, HDAUDIO_MIXER_LINE1, HDAUDIO_MIXER_LINE2,
	      HDAUDIO_MIXER_LINE3, HDAUDIO_MIXER_PHONEIN,
	      HDAUDIO_MIXER_PHONEOUT, HDAUDIO_MIXER_VIDEO, HDAUDIO_MIXER_RADIO,
	      HDAUDIO_MIXER_DIGITAL1, HDAUDIO_MIXER_DIGITAL2,
	      HDAUDIO_MIXER_DIGITAL3, HDAUDIO_MIXER_MONITOR, -1 } /* others */
	};

	/* Surely known names */
	for (i = sc->sc_startnode; i < sc->sc_endnode; i++) {
		w = hdafg_widget_lookup(sc, i);
		if (w == NULL || w->w_enable == false)
			continue;
		if (w->w_bindas == -1)
			continue;
		use = -1;
		switch (w->w_type) {
		case COP_AWCAP_TYPE_PIN_COMPLEX:
			if (as[w->w_bindas].as_dir == HDAUDIO_PINDIR_OUT)
				break;
			type = -1;
			switch (COP_CFG_DEFAULT_DEVICE(w->w_pin.config)) {
			case COP_DEVICE_LINE_IN:
				type = 0;
				break;
			case COP_DEVICE_MIC_IN:
				if (COP_CFG_PORT_CONNECTIVITY(w->w_pin.config)
				    == COP_PORT_JACK)
					break;
				type = 1;
				break;
			case COP_DEVICE_CD:
				type = 3;
				break;
			case COP_DEVICE_SPEAKER:
				type = 4;
				break;
			case COP_DEVICE_SPDIF_IN:
			case COP_DEVICE_DIGITAL_OTHER_IN:
				type = 5;
				break;
			}
			if (type == -1)
				break;
			j = 0;
			while (types[type][j] >= 0 &&
			    (used & (1 << types[type][j])) != 0) {
				j++;
			}
			if (types[type][j] >= 0)
				use = types[type][j];
			break;
		case COP_AWCAP_TYPE_AUDIO_OUTPUT:
			use = HDAUDIO_MIXER_PCM;
			break;
		case COP_AWCAP_TYPE_BEEP_GENERATOR:
			use = HDAUDIO_MIXER_SPEAKER;
			break;
		default:
			break;
		}
		if (use >= 0) {
			w->w_audiodev = use;
			used |= (1 << use);
		}
	}
	/* Semi-known names */
	for (i = sc->sc_startnode; i < sc->sc_endnode; i++) {
		w = hdafg_widget_lookup(sc, i);
		if (w == NULL || w->w_enable == false)
			continue;
		if (w->w_audiodev >= 0)
			continue;
		if (w->w_bindas == -1)
			continue;
		if (w->w_type != COP_AWCAP_TYPE_PIN_COMPLEX)
			continue;
		if (as[w->w_bindas].as_dir == HDAUDIO_PINDIR_OUT)
			continue;
		type = -1;
		switch (COP_CFG_DEFAULT_DEVICE(w->w_pin.config)) {
		case COP_DEVICE_LINE_OUT:
		case COP_DEVICE_SPEAKER:
		case COP_DEVICE_HP_OUT:
		case COP_DEVICE_AUX:
			type = 0;
			break;
		case COP_DEVICE_MIC_IN:
			type = 2;
			break;
		case COP_DEVICE_SPDIF_OUT:
		case COP_DEVICE_DIGITAL_OTHER_OUT:
			type = 5;
			break;
		}
		if (type == -1)
			break;
		j = 0;
		while (types[type][j] >= 0 &&
		    (used & (1 << types[type][j])) != 0) {
			j++;
		}
		if (types[type][j] >= 0) {
			w->w_audiodev = types[type][j];
			used |= (1 << types[type][j]);
		}
	}
	/* Others */
	for (i = sc->sc_startnode; i < sc->sc_endnode; i++) {
		w = hdafg_widget_lookup(sc, i);
		if (w == NULL || w->w_enable == false)
			continue;
		if (w->w_audiodev >= 0)
			continue;
		if (w->w_bindas == -1)
			continue;
		if (w->w_type != COP_AWCAP_TYPE_PIN_COMPLEX)
			continue;
		if (as[w->w_bindas].as_dir == HDAUDIO_PINDIR_OUT)
			continue;
		j = 0;
		while (types[6][j] >= 0 &&
		    (used & (1 << types[6][j])) != 0) {
			j++;
		}
		if (types[6][j] >= 0) {
			w->w_audiodev = types[6][j];
			used |= (1 << types[6][j]);
		}
	}
}

static int
hdafg_control_source_amp(struct hdafg_softc *sc, int nid, int index,
    int audiodev, int ctlable, int depth, int need)
{
	struct hdaudio_widget *w, *wc;
	struct hdaudio_control *ctl;
	int i, j, conns = 0, rneed;

	if (depth >= HDAUDIO_PARSE_MAXDEPTH)
		return need;

	w = hdafg_widget_lookup(sc, nid);
	if (w == NULL || w->w_enable == false)
		return need;

	/* Count number of active inputs */
	if (depth > 0) {
		for (j = 0; j < w->w_nconns; j++) {
			if (w->w_connsenable[j])
				++conns;
		}
	}

	/*
	 * If this is not a first step, use input mixer. Pins have common
	 * input ctl so care must be taken
	 */
	if (depth > 0 && ctlable && (conns == 1 ||
	    w->w_type != COP_AWCAP_TYPE_PIN_COMPLEX)) {
		ctl = hdafg_control_amp_get(sc, w->w_nid,
		    HDAUDIO_PINDIR_IN, index, 1);
		if (ctl) {
			if (HDAUDIO_CONTROL_GIVE(ctl) & need)
				ctl->ctl_audiomask |= (1 << audiodev);
			else
				ctl->ctl_paudiomask |= (1 << audiodev);
			need &= ~HDAUDIO_CONTROL_GIVE(ctl);
		}
	}

	/* If widget has own audiodev, don't traverse it. */
	if (w->w_audiodev >= 0 && depth > 0)
		return need;

	/* We must not traverse pins */
	if ((w->w_type == COP_AWCAP_TYPE_AUDIO_INPUT ||
	    w->w_type == COP_AWCAP_TYPE_PIN_COMPLEX) && depth > 0)
		return need;

	/* Record that this widget exports such signal */
	w->w_audiomask |= (1 << audiodev);

	/*
	 * If signals mixed, we can't assign controls further. Ignore this
	 * on depth zero. Caller must know why. Ignore this for static
	 * selectors if this input is selected.
	 */
	if (conns > 1)
		ctlable = 0;

	if (ctlable) {
		ctl = hdafg_control_amp_get(sc, w->w_nid,
		    HDAUDIO_PINDIR_OUT, -1, 1);
		if (ctl) {
			if (HDAUDIO_CONTROL_GIVE(ctl) & need)
				ctl->ctl_audiomask |= (1 << audiodev);
			else
				ctl->ctl_paudiomask |= (1 << audiodev);
			need &= ~HDAUDIO_CONTROL_GIVE(ctl);
		}
	}

	rneed = 0;
	for (i = sc->sc_startnode; i < sc->sc_endnode; i++) {
		wc = hdafg_widget_lookup(sc, i);
		if (wc == NULL || wc->w_enable == false)
			continue;
		for (j = 0; j < wc->w_nconns; j++) {
			if (wc->w_connsenable[j] && wc->w_conns[j] == nid) {
				rneed |= hdafg_control_source_amp(sc,
				    wc->w_nid, j, audiodev, ctlable, depth + 1,
				    need);
			}
		}
	}
	rneed &= need;

	return rneed;
}

static void
hdafg_control_dest_amp(struct hdafg_softc *sc, int nid,
    int audiodev, int depth, int need)
{
	struct hdaudio_assoc *as = sc->sc_assocs;
	struct hdaudio_widget *w, *wc;
	struct hdaudio_control *ctl;
	int i, j, consumers;

	if (depth > HDAUDIO_PARSE_MAXDEPTH)
		return;

	w = hdafg_widget_lookup(sc, nid);
	if (w == NULL || w->w_enable == false)
		return;

	if (depth > 0) {
		/*
		 * If this node produces output for several consumers,
		 * we can't touch it
		 */
		consumers = 0;
		for (i = sc->sc_startnode; i < sc->sc_endnode; i++) {
			wc = hdafg_widget_lookup(sc, i);
			if (wc == NULL || wc->w_enable == false)
				continue;
			for (j = 0; j < wc->w_nconns; j++) {
				if (wc->w_connsenable[j] &&
				    wc->w_conns[j] == nid)
					++consumers;
			}
		}
		/*
		 * The only exception is if real HP redirection is configured
		 * and this is a duplication point.
		 * XXX: Not completely correct.
		 */
		if ((consumers == 2 && (w->w_bindas < 0 ||
		    as[w->w_bindas].as_hpredir < 0 ||
		    as[w->w_bindas].as_fakeredir ||
		    (w->w_bindseqmask & (1 << 15)) == 0)) ||
		    consumers > 2)
			return;

		/* Else use its output mixer */
		ctl = hdafg_control_amp_get(sc, w->w_nid,
		    HDAUDIO_PINDIR_OUT, -1, 1);
		if (ctl) {
			if (HDAUDIO_CONTROL_GIVE(ctl) & need)
				ctl->ctl_audiomask |= (1 << audiodev);
			else
				ctl->ctl_paudiomask |= (1 << audiodev);
			need &= ~HDAUDIO_CONTROL_GIVE(ctl);
		}
	}

	/* We must not traverse pin */
	if (w->w_type == COP_AWCAP_TYPE_PIN_COMPLEX && depth > 0)
		return;

	for (i = 0; i < w->w_nconns; i++) {
		int tneed = need;
		if (w->w_connsenable[i] == false)
			continue;
		ctl = hdafg_control_amp_get(sc, w->w_nid,
		    HDAUDIO_PINDIR_IN, i, 1);
		if (ctl) {
			if (HDAUDIO_CONTROL_GIVE(ctl) & tneed)
				ctl->ctl_audiomask |= (1 << audiodev);
			else
				ctl->ctl_paudiomask |= (1 << audiodev);
			tneed &= ~HDAUDIO_CONTROL_GIVE(ctl);
		}
		hdafg_control_dest_amp(sc, w->w_conns[i], audiodev,
		    depth + 1, tneed);
	}
}

static void
hdafg_assign_mixers(struct hdafg_softc *sc)
{
	struct hdaudio_assoc *as = sc->sc_assocs;
	struct hdaudio_control *ctl;
	struct hdaudio_widget *w;
	int i;

	/* Assign mixers to the tree */
	for (i = sc->sc_startnode; i < sc->sc_endnode; i++) {
		w = hdafg_widget_lookup(sc, i);
		if (w == NULL || w->w_enable == FALSE)
			continue;
		if (w->w_type == COP_AWCAP_TYPE_AUDIO_OUTPUT ||
		    w->w_type == COP_AWCAP_TYPE_BEEP_GENERATOR ||
		    (w->w_type == COP_AWCAP_TYPE_PIN_COMPLEX &&
		    as[w->w_bindas].as_dir == HDAUDIO_PINDIR_IN)) {
			if (w->w_audiodev < 0)
				continue;
			hdafg_control_source_amp(sc, w->w_nid, -1,
			    w->w_audiodev, 1, 0, 1);
		} else if (w->w_pflags & HDAUDIO_ADC_MONITOR) {
			if (w->w_audiodev < 0)
				continue;
			if (hdafg_control_source_amp(sc, w->w_nid, -1,
			    w->w_audiodev, 1, 0, 1)) {
				/* If we are unable to control input monitor
				   as source, try to control it as dest */
				hdafg_control_dest_amp(sc, w->w_nid,
				    w->w_audiodev, 0, 1);
			}
		} else if (w->w_type == COP_AWCAP_TYPE_AUDIO_INPUT) {
			hdafg_control_dest_amp(sc, w->w_nid,
			    HDAUDIO_MIXER_RECLEV, 0, 1);
		} else if (w->w_type == COP_AWCAP_TYPE_PIN_COMPLEX &&
		    as[w->w_bindas].as_dir == HDAUDIO_PINDIR_OUT) {
			hdafg_control_dest_amp(sc, w->w_nid,
			    HDAUDIO_MIXER_VOLUME, 0, 1);
		}
	}
	/* Treat unrequired as possible */
	i = 0;
	for (i = 0; i < sc->sc_nctls; i++) {
		ctl = &sc->sc_ctls[i];
		if (ctl->ctl_audiomask == 0)
			ctl->ctl_audiomask = ctl->ctl_paudiomask;
	}
}

static void
hdafg_build_mixers(struct hdafg_softc *sc)
{
	struct hdaudio_mixer *mx;
	struct hdaudio_control *ctl, *masterctl = NULL;
	uint32_t audiomask = 0;
	int nmixers = 0;
	int i, j, index = 0;
	int ndac, nadc;
	int ctrlcnt[HDAUDIO_MIXER_NRDEVICES];

	memset(ctrlcnt, 0, sizeof(ctrlcnt));

	/* Count the number of required mixers */
	for (i = 0; i < sc->sc_nctls; i++) {
		ctl = &sc->sc_ctls[i];
		if (ctl->ctl_enable == false ||
		    ctl->ctl_audiomask == 0)
			continue;
		audiomask |= ctl->ctl_audiomask;
		++nmixers;
		if (ctl->ctl_mute)
			++nmixers;
	}

	/* XXXJDM TODO: softvol */
	/* Declare master volume if needed */
	if ((audiomask & (HDAUDIO_MASK(VOLUME) | HDAUDIO_MASK(PCM))) ==
	    HDAUDIO_MASK(PCM)) {
		audiomask |= HDAUDIO_MASK(VOLUME);
		for (i = 0; i < sc->sc_nctls; i++) {
			if (sc->sc_ctls[i].ctl_audiomask == HDAUDIO_MASK(PCM)) {
				masterctl = &sc->sc_ctls[i];
				++nmixers;
				if (masterctl->ctl_mute)
					++nmixers;
				break;
			}
		}
	}

	/* Make room for mixer classes */
	nmixers += (HDAUDIO_MIXER_CLASS_LAST + 1);

	/* count DACs and ADCs for selectors */
	ndac = nadc = 0;
	for (i = 0; i < sc->sc_nassocs; i++) {
		if (sc->sc_assocs[i].as_enable == false)
			continue;
		if (sc->sc_assocs[i].as_dir == HDAUDIO_PINDIR_OUT)
			++ndac;
		else if (sc->sc_assocs[i].as_dir == HDAUDIO_PINDIR_IN)
			++nadc;
	}

	/* Make room for selectors */
	if (ndac > 0)
		++nmixers;
	if (nadc > 0)
		++nmixers;

	hda_trace(sc, "  need %d mixers (3 classes%s)\n",
	    nmixers, masterctl ? " + fake master" : "");

	/* Allocate memory for the mixers */
	mx = kmem_zalloc(nmixers * sizeof(*mx), KM_SLEEP);
	sc->sc_nmixers = nmixers;

	/* Build class mixers */
	for (i = 0; i <= HDAUDIO_MIXER_CLASS_LAST; i++) {
		mx[index].mx_ctl = NULL;
		mx[index].mx_di.index = index;
		mx[index].mx_di.type = AUDIO_MIXER_CLASS;
		mx[index].mx_di.mixer_class = i;
		mx[index].mx_di.next = mx[index].mx_di.prev = AUDIO_MIXER_LAST;
		switch (i) {
		case HDAUDIO_MIXER_CLASS_OUTPUTS:
			strcpy(mx[index].mx_di.label.name, AudioCoutputs);
			break;
		case HDAUDIO_MIXER_CLASS_INPUTS:
			strcpy(mx[index].mx_di.label.name, AudioCinputs);
			break;
		case HDAUDIO_MIXER_CLASS_RECORD:
			strcpy(mx[index].mx_di.label.name, AudioCrecord);
			break;
		}
		++index;
	}

	/* Shadow master control */
	if (masterctl != NULL) {
		mx[index].mx_ctl = masterctl;
		mx[index].mx_di.index = index;
		mx[index].mx_di.type = AUDIO_MIXER_VALUE;
		mx[index].mx_di.prev = mx[index].mx_di.next = AUDIO_MIXER_LAST;
		mx[index].mx_di.un.v.num_channels = 2;	/* XXX */
		mx[index].mx_di.mixer_class = HDAUDIO_MIXER_CLASS_OUTPUTS;
		mx[index].mx_di.un.v.delta = 256 / (masterctl->ctl_step + 1);
		strcpy(mx[index].mx_di.label.name, AudioNmaster);
		strcpy(mx[index].mx_di.un.v.units.name, AudioNvolume);
		hda_trace(sc, "  adding outputs.%s\n",
		    mx[index].mx_di.label.name);
		++index;
		if (masterctl->ctl_mute) {
			mx[index] = mx[index - 1];
			mx[index].mx_di.index = index;
			mx[index].mx_di.type = AUDIO_MIXER_ENUM;
			mx[index].mx_di.prev = mx[index].mx_di.next = AUDIO_MIXER_LAST;
			strcpy(mx[index].mx_di.label.name, AudioNmaster "." AudioNmute);
			mx[index].mx_di.un.e.num_mem = 2;
			strcpy(mx[index].mx_di.un.e.member[0].label.name, AudioNoff);
			mx[index].mx_di.un.e.member[0].ord = 0;
			strcpy(mx[index].mx_di.un.e.member[1].label.name, AudioNon);
			mx[index].mx_di.un.e.member[1].ord = 1;
			++index;
		}
	}

	/* Build volume mixers */
	for (i = 0; i < sc->sc_nctls; i++) {
		uint32_t audiodev;

		ctl = &sc->sc_ctls[i];
		if (ctl->ctl_enable == false ||
		    ctl->ctl_audiomask == 0)
			continue;
		audiodev = ffs(ctl->ctl_audiomask) - 1;
		mx[index].mx_ctl = ctl;
		mx[index].mx_di.index = index;
		mx[index].mx_di.type = AUDIO_MIXER_VALUE;
		mx[index].mx_di.prev = mx[index].mx_di.next = AUDIO_MIXER_LAST;
		mx[index].mx_di.un.v.num_channels = 2;	/* XXX */
		mx[index].mx_di.un.v.delta = 256 / (ctl->ctl_step + 1);
		if (ctrlcnt[audiodev] > 0)
			snprintf(mx[index].mx_di.label.name,
			    sizeof(mx[index].mx_di.label.name),
			    "%s%d",
			    hdafg_mixer_names[audiodev],
			    ctrlcnt[audiodev] + 1);
		else
			strcpy(mx[index].mx_di.label.name,
			    hdafg_mixer_names[audiodev]);
		ctrlcnt[audiodev]++;

		switch (audiodev) {
		case HDAUDIO_MIXER_VOLUME:
		case HDAUDIO_MIXER_BASS:
		case HDAUDIO_MIXER_TREBLE:
		case HDAUDIO_MIXER_OGAIN:
			mx[index].mx_di.mixer_class =
			    HDAUDIO_MIXER_CLASS_OUTPUTS;
			hda_trace(sc, "  adding outputs.%s\n",
			    mx[index].mx_di.label.name);
			break;
		case HDAUDIO_MIXER_MIC:
		case HDAUDIO_MIXER_MONITOR:
			mx[index].mx_di.mixer_class =
			    HDAUDIO_MIXER_CLASS_RECORD;
			hda_trace(sc, "  adding record.%s\n",
			    mx[index].mx_di.label.name);
			break;
		default:
			mx[index].mx_di.mixer_class =
			    HDAUDIO_MIXER_CLASS_INPUTS;
			hda_trace(sc, "  adding inputs.%s\n",
			    mx[index].mx_di.label.name);
			break;
		}
		strcpy(mx[index].mx_di.un.v.units.name, AudioNvolume);
		
		++index;

		if (ctl->ctl_mute) {
			mx[index] = mx[index - 1];
			mx[index].mx_di.index = index;
			mx[index].mx_di.type = AUDIO_MIXER_ENUM;
			mx[index].mx_di.prev = mx[index].mx_di.next = AUDIO_MIXER_LAST;
			snprintf(mx[index].mx_di.label.name,
			    sizeof(mx[index].mx_di.label.name),
			    "%s." AudioNmute,
			    mx[index - 1].mx_di.label.name);
			mx[index].mx_di.un.e.num_mem = 2;
			strcpy(mx[index].mx_di.un.e.member[0].label.name, AudioNoff);
			mx[index].mx_di.un.e.member[0].ord = 0;
			strcpy(mx[index].mx_di.un.e.member[1].label.name, AudioNon);
			mx[index].mx_di.un.e.member[1].ord = 1;
			++index;
		}
	}

	/* DAC selector */
	if (ndac > 0) {
		mx[index].mx_ctl = NULL;
		mx[index].mx_di.index = index;
		mx[index].mx_di.type = AUDIO_MIXER_SET;
		mx[index].mx_di.mixer_class = HDAUDIO_MIXER_CLASS_OUTPUTS;
		mx[index].mx_di.prev = mx[index].mx_di.next = AUDIO_MIXER_LAST;
		strcpy(mx[index].mx_di.label.name, "dacsel"); /* AudioNselect */
		mx[index].mx_di.un.s.num_mem = ndac;
		for (i = 0, j = 0; i < sc->sc_nassocs; i++) {
			if (sc->sc_assocs[i].as_enable == false)
				continue;
			if (sc->sc_assocs[i].as_dir != HDAUDIO_PINDIR_OUT)
				continue;
			mx[index].mx_di.un.s.member[j].mask = 1 << i;
			snprintf(mx[index].mx_di.un.s.member[j].label.name,
			    sizeof(mx[index].mx_di.un.s.member[j].label.name),
			    "%s%02X",
			    hdafg_assoc_type_string(&sc->sc_assocs[i]), i);
			++j;
		}
		++index;
	}

	/* ADC selector */
	if (nadc > 0) {
		mx[index].mx_ctl = NULL;
		mx[index].mx_di.index = index;
		mx[index].mx_di.type = AUDIO_MIXER_SET;
		mx[index].mx_di.mixer_class = HDAUDIO_MIXER_CLASS_RECORD;
		mx[index].mx_di.prev = mx[index].mx_di.next = AUDIO_MIXER_LAST;
		strcpy(mx[index].mx_di.label.name, AudioNsource);
		mx[index].mx_di.un.s.num_mem = nadc;
		for (i = 0, j = 0; i < sc->sc_nassocs; i++) {
			if (sc->sc_assocs[i].as_enable == false)
				continue;
			if (sc->sc_assocs[i].as_dir != HDAUDIO_PINDIR_IN)
				continue;
			mx[index].mx_di.un.s.member[j].mask = 1 << i;
			snprintf(mx[index].mx_di.un.s.member[j].label.name,
			    sizeof(mx[index].mx_di.un.s.member[j].label.name),
			    "%s%02X",
			    hdafg_assoc_type_string(&sc->sc_assocs[i]), i);
			++j;
		}
		++index;
	}

	sc->sc_mixers = mx;
}

static void
hdafg_commit(struct hdafg_softc *sc)
{
	struct hdaudio_widget *w;
	uint32_t gdata, gmask, gdir;
	int commitgpio;
	int i;

	/* Commit controls */
	hdafg_control_commit(sc);

	/* Commit selectors, pins, and EAPD */
	for (i = 0; i < sc->sc_nwidgets; i++) {
		w = &sc->sc_widgets[i];
		if (w->w_selconn == -1)
			w->w_selconn = 0;
		if (w->w_nconns > 0)
			hdafg_widget_connection_select(w, w->w_selconn);
		if (w->w_type == COP_AWCAP_TYPE_PIN_COMPLEX)
			hdaudio_command(sc->sc_codec, w->w_nid,
			    CORB_SET_PIN_WIDGET_CONTROL, w->w_pin.ctrl);
		if (w->w_p.eapdbtl != 0xffffffff)
			hdaudio_command(sc->sc_codec, w->w_nid,
			    CORB_SET_EAPD_BTL_ENABLE, w->w_p.eapdbtl);
	}

	gdata = gmask = gdir = commitgpio = 0;
#ifdef notyet
	int numgpio = COP_GPIO_COUNT_NUM_GPIO(sc->sc_p.gpio_cnt);

	hda_trace(sc, "found %d GPIOs\n", numgpio);
	for (i = 0; i < numgpio && i < 8; i++) {
		if (commitgpio == 0)
			commitgpio = 1;
		gdata |= 1 << i;
		gmask |= 1 << i;
		gdir |= 1 << i;
	}
#endif

	if (commitgpio) {
		hda_trace(sc, "GPIO commit: data=%08X mask=%08X dir=%08X\n",
		    gdata, gmask, gdir);
		hdaudio_command(sc->sc_codec, sc->sc_nid,
		    CORB_SET_GPIO_ENABLE_MASK, gmask);
		hdaudio_command(sc->sc_codec, sc->sc_nid,
		    CORB_SET_GPIO_DIRECTION, gdir);
		hdaudio_command(sc->sc_codec, sc->sc_nid,
		    CORB_SET_GPIO_DATA, gdata);
	}
}

static void
hdafg_stream_connect_hdmi(struct hdafg_softc *sc, struct hdaudio_assoc *as,
    struct hdaudio_widget *w, const audio_params_t *params)
{
	struct hdmi_audio_infoframe hdmi;
	/* TODO struct displayport_audio_infoframe dp; */
	uint8_t *dip = NULL;
	size_t diplen = 0;
	int i;

#ifdef HDAFG_HDMI_DEBUG
	uint32_t res;
	res = hdaudio_command(sc->sc_codec, w->w_nid,
	    CORB_GET_HDMI_DIP_XMIT_CTRL, 0);
	hda_print(sc, "connect HDMI nid %02X, xmitctrl = 0x%08X\n",
	    w->w_nid, res);
#endif

	/* disable infoframe transmission */
	hdaudio_command(sc->sc_codec, w->w_nid,
	    CORB_SET_HDMI_DIP_XMIT_CTRL, COP_DIP_XMIT_CTRL_DISABLE);

	/* build new infoframe */
	if (as->as_digital == HDAFG_AS_HDMI) {
		dip = (uint8_t *)&hdmi;
		diplen = sizeof(hdmi);
		memset(&hdmi, 0, sizeof(hdmi));
		hdmi.header.packet_type = HDMI_AI_PACKET_TYPE;
		hdmi.header.version = HDMI_AI_VERSION;
		hdmi.header.length = HDMI_AI_LENGTH;
		hdmi.ct_cc = params->channels - 1;
		if (params->channels > 2) {
			hdmi.ca = 0x1f;
		} else {
			hdmi.ca = 0x00;
		}
		hdafg_dd_hdmi_ai_cksum(&hdmi);
	}
	/* update data island with new audio infoframe */
	if (dip) {
		hdaudio_command(sc->sc_codec, w->w_nid,
		    CORB_SET_HDMI_DIP_INDEX, 0);
		for (i = 0; i < diplen; i++) {
			hdaudio_command(sc->sc_codec, w->w_nid,
			    CORB_SET_HDMI_DIP_DATA, dip[i]);
		}
	}

	/* enable infoframe transmission */
	hdaudio_command(sc->sc_codec, w->w_nid,
	    CORB_SET_HDMI_DIP_XMIT_CTRL, COP_DIP_XMIT_CTRL_BEST_EFFORT);
}

static void
hdafg_stream_connect(struct hdafg_softc *sc, int mode)
{
	struct hdaudio_assoc *as = sc->sc_assocs;
	struct hdaudio_widget *w;
	const audio_params_t *params;
	uint16_t fmt, dfmt;
	int tag, chn, maxchan, c;
	int i, j, k;

	KASSERT(mode == AUMODE_PLAY || mode == AUMODE_RECORD);

	if (mode == AUMODE_PLAY) {
		fmt = hdaudio_stream_param(sc->sc_audiodev.ad_playback,
		    &sc->sc_pparam);
		params = &sc->sc_pparam;
	} else {
		fmt = hdaudio_stream_param(sc->sc_audiodev.ad_capture,
		    &sc->sc_rparam);
		params = &sc->sc_rparam;
	}

	for (i = 0; i < sc->sc_nassocs; i++) {
		if (as[i].as_enable == false)
			continue;

		if (mode == AUMODE_PLAY && as[i].as_dir != HDAUDIO_PINDIR_OUT)
			continue;
		if (mode == AUMODE_RECORD && as[i].as_dir != HDAUDIO_PINDIR_IN)
			continue;

		fmt &= ~HDAUDIO_FMT_CHAN_MASK;
		if (as[i].as_dir == HDAUDIO_PINDIR_OUT &&
		    sc->sc_audiodev.ad_playback != NULL) {
			tag = hdaudio_stream_tag(sc->sc_audiodev.ad_playback);
			fmt |= HDAUDIO_FMT_CHAN(sc->sc_pparam.channels);
			maxchan = sc->sc_pparam.channels;
		} else if (as[i].as_dir == HDAUDIO_PINDIR_IN &&
		    sc->sc_audiodev.ad_capture != NULL) {
			tag = hdaudio_stream_tag(sc->sc_audiodev.ad_capture);
			fmt |= HDAUDIO_FMT_CHAN(sc->sc_rparam.channels);
			maxchan = sc->sc_rparam.channels;
		} else {
			tag = 0;
			if (as[i].as_dir == HDAUDIO_PINDIR_OUT) {
				fmt |= HDAUDIO_FMT_CHAN(sc->sc_pchan);
				maxchan = sc->sc_pchan;
			} else {
				fmt |= HDAUDIO_FMT_CHAN(sc->sc_rchan);
				maxchan = sc->sc_rchan;
			}
		}

		chn = 0;
		for (j = 0; j < HDAUDIO_MAXPINS; j++) {
			if (as[i].as_dacs[j] == 0)
				continue;
			w = hdafg_widget_lookup(sc, as[i].as_dacs[j]);
			if (w == NULL || w->w_enable == FALSE)
				continue;
			if (as[i].as_hpredir >= 0 && i == as[i].as_pincnt)
				chn = 0;
			if (chn >= maxchan)
				chn = 0;	/* XXX */
			c = (tag << 4) | chn;

			if (as[i].as_activated == false)
				c = 0;

			/*
			 * If a non-PCM stream is being connected, and the
			 * analog converter doesn't support non-PCM streams,
			 * then don't decode it
			 */
			if (!(w->w_p.aw_cap & COP_AWCAP_DIGITAL) &&
			    !(w->w_p.stream_format & COP_STREAM_FORMAT_AC3) &&
			    (fmt & HDAUDIO_FMT_TYPE_NONPCM)) {
				hdaudio_command(sc->sc_codec, w->w_nid,
				    CORB_SET_CONVERTER_STREAM_CHANNEL, 0);
				continue;
			}

			hdaudio_command(sc->sc_codec, w->w_nid,
			    CORB_SET_CONVERTER_FORMAT, fmt);
			if (w->w_p.aw_cap & COP_AWCAP_DIGITAL) {
				dfmt = hdaudio_command(sc->sc_codec, w->w_nid,
				    CORB_GET_DIGITAL_CONVERTER_CONTROL, 0) &
				    0xff;
				dfmt |= COP_DIGITAL_CONVCTRL1_DIGEN;
				if (fmt & HDAUDIO_FMT_TYPE_NONPCM)
					dfmt |= COP_DIGITAL_CONVCTRL1_NAUDIO;
				else
					dfmt &= ~COP_DIGITAL_CONVCTRL1_NAUDIO;
				if (sc->sc_vendor == HDAUDIO_VENDOR_NVIDIA)
					dfmt |= COP_DIGITAL_CONVCTRL1_COPY;
				hdaudio_command(sc->sc_codec, w->w_nid,
				    CORB_SET_DIGITAL_CONVERTER_CONTROL_1, dfmt);
			}
			if (w->w_pin.cap & (COP_PINCAP_HDMI|COP_PINCAP_DP)) {
				hdaudio_command(sc->sc_codec, w->w_nid,
				    CORB_SET_CONVERTER_CHANNEL_COUNT,
				    maxchan - 1);
				for (k = 0; k < maxchan; k++) {
					hdaudio_command(sc->sc_codec, w->w_nid,
					    CORB_ASP_SET_CHANNEL_MAPPING,
					    (k << 4) | k);
				}
			}
			hdaudio_command(sc->sc_codec, w->w_nid,
			    CORB_SET_CONVERTER_STREAM_CHANNEL, c);
			chn += COP_AWCAP_CHANNEL_COUNT(w->w_p.aw_cap);
		}

		for (j = 0; j < HDAUDIO_MAXPINS; j++) {
			if (as[i].as_pins[j] == 0)
				continue;
			w = hdafg_widget_lookup(sc, as[i].as_pins[j]);
			if (w == NULL || w->w_enable == FALSE)
				continue;
			if (w->w_pin.cap & (COP_PINCAP_HDMI|COP_PINCAP_DP))
				hdafg_stream_connect_hdmi(sc, &as[i],
				    w, params);
		}
	}
}

static int
hdafg_stream_intr(struct hdaudio_stream *st)
{
	struct hdaudio_audiodev *ad = st->st_cookie;
	int handled = 0;

	(void)hda_read1(ad->ad_sc->sc_host, HDAUDIO_SD_STS(st->st_shift));
	hda_write1(ad->ad_sc->sc_host, HDAUDIO_SD_STS(st->st_shift),
	    HDAUDIO_STS_DESE | HDAUDIO_STS_FIFOE | HDAUDIO_STS_BCIS);

	mutex_spin_enter(&ad->ad_sc->sc_intr_lock);
	/* XXX test (sts & HDAUDIO_STS_BCIS)? */
	if (st == ad->ad_playback && ad->ad_playbackintr) {
		ad->ad_playbackintr(ad->ad_playbackintrarg);
		handled = 1;
	} else if (st == ad->ad_capture && ad->ad_captureintr) {
		ad->ad_captureintr(ad->ad_captureintrarg);
		handled = 1;
	}
	mutex_spin_exit(&ad->ad_sc->sc_intr_lock);

	return handled;
}

static bool
hdafg_rate_supported(struct hdafg_softc *sc, u_int frequency)
{
	uint32_t caps = sc->sc_p.pcm_size_rate;

	if (sc->sc_fixed_rate)
		return frequency == sc->sc_fixed_rate;

#define ISFREQOK(shift)	((caps & (1 << (shift))) ? true : false)
	switch (frequency) {
	case 8000:
		return ISFREQOK(0);
	case 11025:
		return ISFREQOK(1);
	case 16000:
		return ISFREQOK(2);
	case 22050:
		return ISFREQOK(3);
	case 32000:
		return ISFREQOK(4);
	case 44100:
		return ISFREQOK(5);
		return true;
	case 48000:
		return true;	/* Must be supported by all codecs */
	case 88200:
		return ISFREQOK(7);
	case 96000:
		return ISFREQOK(8);
	case 176400:
		return ISFREQOK(9);
	case 192000:
		return ISFREQOK(10);
	case 384000:
		return ISFREQOK(11);
	default:
		return false;
	}
#undef ISFREQOK
}

static bool
hdafg_bits_supported(struct hdafg_softc *sc, u_int bits)
{
	uint32_t caps = sc->sc_p.pcm_size_rate;
#define ISBITSOK(shift)	((caps & (1 << (shift))) ? true : false)
	switch (bits) {
	case 8:
		return ISBITSOK(16);
	case 16:
		return ISBITSOK(17);
	case 20:
		return ISBITSOK(18);
	case 24:
		return ISBITSOK(19);
	case 32:
		return ISBITSOK(20);
	default:
		return false;
	}
#undef ISBITSOK
}

static bool
hdafg_probe_encoding(struct hdafg_softc *sc,
    u_int validbits, u_int precision, int encoding, bool force)
{
	struct audio_format f;
	int i;

	if (!force && hdafg_bits_supported(sc, validbits) == false)
		return false;

	memset(&f, 0, sizeof(f));
	f.driver_data = NULL;
	f.mode = 0;
	f.encoding = encoding;
	f.validbits = validbits;
	f.precision = precision;
	f.channels = 0;
	f.channel_mask = 0;
	f.frequency_type = 0;
	for (i = 0; i < __arraycount(hdafg_possible_rates); i++) {
		u_int rate = hdafg_possible_rates[i];
		if (hdafg_rate_supported(sc, rate))
			f.frequency[f.frequency_type++] = rate;
	}

#define HDAUDIO_INITFMT(ch, chmask)			\
	do {						\
		f.channels = (ch);			\
		f.channel_mask = (chmask);		\
		f.mode = 0;				\
		if (sc->sc_pchan >= (ch))		\
			f.mode |= AUMODE_PLAY;		\
		if (sc->sc_rchan >= (ch))		\
			f.mode |= AUMODE_RECORD;	\
		if (f.mode != 0)			\
			hdafg_append_formats(&sc->sc_audiodev, &f); \
	} while (0)

	/* Commented out, otherwise monaural samples play through left
	 * channel only
	 */
	/* HDAUDIO_INITFMT(1, AUFMT_MONAURAL); */
	HDAUDIO_INITFMT(2, AUFMT_STEREO);
	HDAUDIO_INITFMT(4, AUFMT_SURROUND4);
	HDAUDIO_INITFMT(6, AUFMT_DOLBY_5_1);
	HDAUDIO_INITFMT(8, AUFMT_SURROUND_7_1);

#undef HDAUDIO_INITFMT

	return true;
}


static void
hdafg_configure_encodings(struct hdafg_softc *sc)
{
	struct hdaudio_assoc *as = sc->sc_assocs;
	struct hdaudio_widget *w;
	struct audio_format f;
	uint32_t stream_format, caps;
	int nchan, i, nid;

	sc->sc_pchan = sc->sc_rchan = 0;

	for (nchan = 0, i = 0; i < sc->sc_nassocs; i++) {
		nchan = hdafg_assoc_count_channels(sc, &as[i],
		    HDAUDIO_PINDIR_OUT);
		if (nchan > sc->sc_pchan)
			sc->sc_pchan = nchan;
	}
	for (nchan = 0, i = 0; i < sc->sc_nassocs; i++) {
		nchan = hdafg_assoc_count_channels(sc, &as[i],
		    HDAUDIO_PINDIR_IN);
		if (nchan > sc->sc_rchan)
			sc->sc_rchan = nchan;
	}
	hda_print(sc, "%dch/%dch", sc->sc_pchan, sc->sc_rchan);

	for (i = 0; i < __arraycount(hdafg_possible_rates); i++)
		if (hdafg_rate_supported(sc,
		    hdafg_possible_rates[i]))
			hda_print1(sc, " %uHz", hdafg_possible_rates[i]);

	stream_format = sc->sc_p.stream_format;
	caps = 0;
	for (nid = sc->sc_startnode; nid < sc->sc_endnode; nid++) {
		w = hdafg_widget_lookup(sc, nid);
		if (w == NULL)
			continue;
		stream_format |= w->w_p.stream_format;
		caps |= w->w_p.aw_cap;
	}
	if (stream_format == 0) {
		hda_print(sc,
		    "WARNING: unsupported stream format mask 0x%X, assuming PCM\n",
		    stream_format);
		stream_format |= COP_STREAM_FORMAT_PCM;
	}

	if (stream_format & COP_STREAM_FORMAT_PCM) {
		int e = AUDIO_ENCODING_SLINEAR_LE;
		if (hdafg_probe_encoding(sc, 8, 16, e, false))
			hda_print1(sc, " PCM8");
		if (hdafg_probe_encoding(sc, 16, 16, e, false))
			hda_print1(sc, " PCM16");
		if (hdafg_probe_encoding(sc, 20, 32, e, false))
			hda_print1(sc, " PCM20");
		if (hdafg_probe_encoding(sc, 24, 32, e, false))
			hda_print1(sc, " PCM24");
		if (hdafg_probe_encoding(sc, 32, 32, e, false))
			hda_print1(sc, " PCM32");
	}

	if ((stream_format & COP_STREAM_FORMAT_AC3) ||
	    (caps & COP_AWCAP_DIGITAL)) {
		int e = AUDIO_ENCODING_AC3;
		if (hdafg_probe_encoding(sc, 16, 16, e, false))
			hda_print1(sc, " AC3");
	}

	if (sc->sc_audiodev.ad_nformats == 0) {
		hdafg_probe_encoding(sc, 16, 16, AUDIO_ENCODING_SLINEAR_LE, true);
		hda_print1(sc, " PCM16*");
	}

	/*
	 * XXX JDM 20090614
	 * MI audio assumes that at least one playback and one capture format
	 * is reported by the hw driver; until this bug is resolved just
	 * report 2ch capabilities if the function group does not support
	 * the direction.
	 */
	if (sc->sc_rchan == 0 || sc->sc_pchan == 0) {
		memset(&f, 0, sizeof(f));
		f.driver_data = NULL;
		f.mode = 0;
		f.encoding = AUDIO_ENCODING_SLINEAR_LE;
		f.validbits = 16;
		f.precision = 16;
		f.channels = 2;
		f.channel_mask = AUFMT_STEREO;
		f.frequency_type = 0;
		f.frequency[0] = f.frequency[1] = sc->sc_fixed_rate ?
		    sc->sc_fixed_rate : 48000;
		f.mode = AUMODE_PLAY|AUMODE_RECORD;
		hdafg_append_formats(&sc->sc_audiodev, &f);
	}

	hda_print1(sc, "\n");
}

static void
hdafg_hp_switch_handler(void *opaque)
{
	struct hdafg_softc *sc = opaque;
	struct hdaudio_assoc *as = sc->sc_assocs;
	struct hdaudio_widget *w;
	uint32_t res = 0;
	int i, j;

	if (!device_is_active(sc->sc_dev))
		goto resched;

	for (i = 0; i < sc->sc_nassocs; i++) {
		if (as[i].as_digital != HDAFG_AS_ANALOG &&
		    as[i].as_digital != HDAFG_AS_SPDIF)
			continue;
		for (j = 0; j < HDAUDIO_MAXPINS; j++) {
			if (as[i].as_pins[j] == 0)
				continue;
			w = hdafg_widget_lookup(sc, as[i].as_pins[j]);
			if (w == NULL || w->w_enable == false)
				continue;
			if (w->w_type != COP_AWCAP_TYPE_PIN_COMPLEX)
				continue;
			if (COP_CFG_DEFAULT_DEVICE(w->w_pin.config) !=
			    COP_DEVICE_HP_OUT)
				continue;
			res |= hdaudio_command(sc->sc_codec, as[i].as_pins[j],
			    CORB_GET_PIN_SENSE, 0) &
			    COP_GET_PIN_SENSE_PRESENSE_DETECT;
		}
	}

	for (i = 0; i < sc->sc_nassocs; i++) {
		if (as[i].as_digital != HDAFG_AS_ANALOG &&
		    as[i].as_digital != HDAFG_AS_SPDIF)
			continue;
		for (j = 0; j < HDAUDIO_MAXPINS; j++) {
			if (as[i].as_pins[j] == 0)
				continue;
			w = hdafg_widget_lookup(sc, as[i].as_pins[j]);
			if (w == NULL || w->w_enable == false)
				continue;
			if (w->w_type != COP_AWCAP_TYPE_PIN_COMPLEX)
				continue;
			switch (COP_CFG_DEFAULT_DEVICE(w->w_pin.config)) {
			case COP_DEVICE_HP_OUT:
				if (res & COP_GET_PIN_SENSE_PRESENSE_DETECT)
					w->w_pin.ctrl |= COP_PWC_OUT_ENABLE;
				else
					w->w_pin.ctrl &= ~COP_PWC_OUT_ENABLE;
				hdaudio_command(sc->sc_codec, w->w_nid,
				    CORB_SET_PIN_WIDGET_CONTROL, w->w_pin.ctrl);
				break;
			case COP_DEVICE_LINE_OUT:
			case COP_DEVICE_SPEAKER:
			case COP_DEVICE_AUX:
				if (res & COP_GET_PIN_SENSE_PRESENSE_DETECT)
					w->w_pin.ctrl &= ~COP_PWC_OUT_ENABLE;
				else
					w->w_pin.ctrl |= COP_PWC_OUT_ENABLE;
				hdaudio_command(sc->sc_codec, w->w_nid,
				    CORB_SET_PIN_WIDGET_CONTROL, w->w_pin.ctrl);
				break;
			default:
				break;
			}
		}
	}

resched:
	callout_schedule(&sc->sc_jack_callout, HDAUDIO_HP_SENSE_PERIOD);
}

static void
hdafg_hp_switch_init(struct hdafg_softc *sc)
{
	struct hdaudio_assoc *as = sc->sc_assocs;
	struct hdaudio_widget *w;
	bool enable = false;
	int i, j;

	for (i = 0; i < sc->sc_nassocs; i++) {
		if (as[i].as_hpredir < 0 && as[i].as_displaydev == false)
			continue;
		if (as[i].as_displaydev == false)
			w = hdafg_widget_lookup(sc, as[i].as_pins[15]);
		else {
			w = NULL;
			for (j = 0; j < HDAUDIO_MAXPINS; j++) {
				if (as[i].as_pins[j] == 0)
					continue;
				w = hdafg_widget_lookup(sc, as[i].as_pins[j]);
				if (w && w->w_enable &&
				    w->w_type == COP_AWCAP_TYPE_PIN_COMPLEX)
					break;
				w = NULL;
			}
		}
		if (w == NULL || w->w_enable == false)
			continue;
		if (w->w_type != COP_AWCAP_TYPE_PIN_COMPLEX)
			continue;
		if (!(w->w_pin.cap & COP_PINCAP_PRESENSE_DETECT_CAPABLE)) {
			continue;
		}
		if (COP_CFG_MISC(w->w_pin.config) & 1) {
			hda_trace(sc, "no presence detect on pin %02X\n",
			    w->w_nid);
			continue;
		}
		if ((w->w_pin.cap & (COP_PINCAP_HDMI|COP_PINCAP_DP)) == 0)
			enable = true;

		if (w->w_p.aw_cap & COP_AWCAP_UNSOL_CAPABLE) {
			uint8_t val = COP_SET_UNSOLICITED_RESPONSE_ENABLE;
			if (w->w_pin.cap & (COP_PINCAP_HDMI|COP_PINCAP_DP))
				val |= HDAUDIO_UNSOLTAG_EVENT_DD;
			else
				val |= HDAUDIO_UNSOLTAG_EVENT_HP;

			hdaudio_command(sc->sc_codec, w->w_nid,
			    CORB_SET_UNSOLICITED_RESPONSE, val);

			hdaudio_command(sc->sc_codec, w->w_nid,
			    CORB_SET_AMPLIFIER_GAIN_MUTE, 0xb000);
		}

		hda_trace(sc, "presence detect [pin=%02X,%s",
		    w->w_nid,
		    (w->w_p.aw_cap & COP_AWCAP_UNSOL_CAPABLE) ?
		     "unsol" : "poll"
		    );
		if (w->w_pin.cap & COP_PINCAP_HDMI)
			hda_trace1(sc, ",hdmi");
		if (w->w_pin.cap & COP_PINCAP_DP)
			hda_trace1(sc, ",displayport");
		hda_trace1(sc, "]\n");
	}
	if (enable) {
		sc->sc_jack_polling = true;
		hdafg_hp_switch_handler(sc);
	} else
		hda_trace(sc, "jack detect not enabled\n");
}

static void
hdafg_attach(device_t parent, device_t self, void *opaque)
{
	struct hdafg_softc *sc = device_private(self);
	audio_params_t defparams;
	prop_dictionary_t args = opaque;
	char vendor[16], product[16];
	uint64_t fgptr = 0;
	uint32_t astype = 0;
	uint8_t nid = 0;
	int err, i;
	bool rv;

	aprint_naive("\n");
	sc->sc_dev = self;

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_SCHED);

	callout_init(&sc->sc_jack_callout, 0);
	callout_setfunc(&sc->sc_jack_callout,
	    hdafg_hp_switch_handler, sc);

	if (!pmf_device_register(self, hdafg_suspend, hdafg_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");

	sc->sc_config = prop_dictionary_get(args, "pin-config");
	if (sc->sc_config && prop_object_type(sc->sc_config) != PROP_TYPE_ARRAY)
		sc->sc_config = NULL;

	prop_dictionary_get_uint16(args, "vendor-id", &sc->sc_vendor);
	prop_dictionary_get_uint16(args, "product-id", &sc->sc_product);
	hdaudio_findvendor(vendor, sizeof(vendor), sc->sc_vendor);
	hdaudio_findproduct(product, sizeof(product), sc->sc_vendor,
	    sc->sc_product);
	hda_print1(sc, ": %s %s%s\n", vendor, product,
	    sc->sc_config ? " (custom configuration)" : "");

	switch (sc->sc_vendor) {
	case HDAUDIO_VENDOR_NVIDIA:
		switch (sc->sc_product) {
		case HDAUDIO_PRODUCT_NVIDIA_TEGRA124_HDMI:
			sc->sc_fixed_rate = 44100;
			break;
		}
		break;
	}

	rv = prop_dictionary_get_uint64(args, "function-group", &fgptr);
	if (rv == false || fgptr == 0) {
		hda_error(sc, "missing function-group property\n");
		return;
	}
	rv = prop_dictionary_get_uint8(args, "node-id", &nid);
	if (rv == false || nid == 0) {
		hda_error(sc, "missing node-id property\n");
		return;
	}

	prop_dictionary_set_uint64(device_properties(self),
	    "codecinfo-callback",
	    (uint64_t)(uintptr_t)hdafg_codec_info);
	prop_dictionary_set_uint64(device_properties(self),
	    "widgetinfo-callback",
	    (uint64_t)(uintptr_t)hdafg_widget_info);

	sc->sc_nid = nid;
	sc->sc_fg = (struct hdaudio_function_group *)(vaddr_t)fgptr;
	sc->sc_fg->fg_unsol = hdafg_unsol;
	sc->sc_codec = sc->sc_fg->fg_codec;
	KASSERT(sc->sc_codec != NULL);
	sc->sc_host = sc->sc_codec->co_host;
	KASSERT(sc->sc_host != NULL);

	hda_debug(sc, "parsing widgets\n");
	hdafg_parse(sc);
	hda_debug(sc, "parsing controls\n");
	hdafg_control_parse(sc);
	hda_debug(sc, "disabling non-audio devices\n");
	hdafg_disable_nonaudio(sc);
	hda_debug(sc, "disabling useless devices\n");
	hdafg_disable_useless(sc);
	hda_debug(sc, "parsing associations\n");
	hdafg_assoc_parse(sc);
	hda_debug(sc, "building tree\n");
	hdafg_build_tree(sc);
	hda_debug(sc, "disabling unassociated pins\n");
	hdafg_disable_unassoc(sc);
	hda_debug(sc, "disabling unselected pins\n");
	hdafg_disable_unsel(sc);
	hda_debug(sc, "disabling useless devices\n");
	hdafg_disable_useless(sc);
	hda_debug(sc, "disabling cross-associated pins\n");
	hdafg_disable_crossassoc(sc);
	hda_debug(sc, "disabling useless devices\n");
	hdafg_disable_useless(sc);

	hda_debug(sc, "assigning mixer names to sound sources\n");
	hdafg_assign_names(sc);
	hda_debug(sc, "assigning mixers to device tree\n");
	hdafg_assign_mixers(sc);

	hda_debug(sc, "preparing pin controls\n");
	hdafg_prepare_pin_controls(sc);
	hda_debug(sc, "commiting settings\n");
	hdafg_commit(sc);

	hda_debug(sc, "setup jack sensing\n");
	hdafg_hp_switch_init(sc);

	hda_debug(sc, "building mixer controls\n");
	hdafg_build_mixers(sc);

	hdafg_dump(sc);
	if (1) hdafg_widget_pin_dump(sc);
	hdafg_assoc_dump(sc);

	hda_debug(sc, "enabling analog beep\n");
	hdafg_enable_analog_beep(sc);

	hda_debug(sc, "configuring encodings\n");
	sc->sc_audiodev.ad_sc = sc;
	hdafg_configure_encodings(sc);
	err = auconv_create_encodings(sc->sc_audiodev.ad_formats,
	    sc->sc_audiodev.ad_nformats, &sc->sc_audiodev.ad_encodings);
	if (err) {
		hda_error(sc, "couldn't create encodings\n");
		return;
	}

	hda_debug(sc, "reserving streams\n");
	sc->sc_audiodev.ad_capture = hdaudio_stream_establish(sc->sc_host,
	    HDAUDIO_STREAM_ISS, hdafg_stream_intr, &sc->sc_audiodev);
	sc->sc_audiodev.ad_playback = hdaudio_stream_establish(sc->sc_host,
	    HDAUDIO_STREAM_OSS, hdafg_stream_intr, &sc->sc_audiodev);

	hda_debug(sc, "connecting streams\n");
	defparams.channels = 2;
	defparams.sample_rate = sc->sc_fixed_rate ? sc->sc_fixed_rate : 48000;
	defparams.precision = defparams.validbits = 16;
	defparams.encoding = AUDIO_ENCODING_SLINEAR_LE;
	sc->sc_pparam = sc->sc_rparam = defparams;
	hdafg_stream_connect(sc, AUMODE_PLAY);
	hdafg_stream_connect(sc, AUMODE_RECORD);

	for (i = 0; i < sc->sc_nassocs; i++) {
		astype |= (1 << sc->sc_assocs[i].as_digital);
	}
	hda_debug(sc, "assoc type mask: %x\n", astype);

#ifndef HDAUDIO_ENABLE_HDMI
	astype &= ~(1 << HDAFG_AS_HDMI);
#endif
#ifndef HDAUDIO_ENABLE_DISPLAYPORT
	astype &= ~(1 << HDAFG_AS_DISPLAYPORT);
#endif

	if (astype == 0)
		return;

	hda_debug(sc, "attaching audio device\n");
	sc->sc_audiodev.ad_audiodev = audio_attach_mi(&hdafg_hw_if,
	    &sc->sc_audiodev, self);
}

static int
hdafg_detach(device_t self, int flags)
{
	struct hdafg_softc *sc = device_private(self);
	struct hdaudio_widget *wl, *w = sc->sc_widgets;
	struct hdaudio_assoc *as = sc->sc_assocs;
	struct hdaudio_control *ctl = sc->sc_ctls;
	struct hdaudio_mixer *mx = sc->sc_mixers;
	int nid;

	callout_halt(&sc->sc_jack_callout, NULL);
	callout_destroy(&sc->sc_jack_callout);

	if (sc->sc_config)
		prop_object_release(sc->sc_config);
	if (sc->sc_audiodev.ad_audiodev)
		config_detach(sc->sc_audiodev.ad_audiodev, flags);
	if (sc->sc_audiodev.ad_encodings)
		auconv_delete_encodings(sc->sc_audiodev.ad_encodings);
	if (sc->sc_audiodev.ad_playback)
		hdaudio_stream_disestablish(sc->sc_audiodev.ad_playback);
	if (sc->sc_audiodev.ad_capture)
		hdaudio_stream_disestablish(sc->sc_audiodev.ad_capture);

	/* restore bios pin widget configuration */
	for (nid = sc->sc_startnode; nid < sc->sc_endnode; nid++) {
		wl = hdafg_widget_lookup(sc, nid);		
		if (wl == NULL || wl->w_type != COP_AWCAP_TYPE_PIN_COMPLEX)
			continue;
		hdafg_widget_setconfig(wl, wl->w_pin.biosconfig);
	}

	if (w)
		kmem_free(w, sc->sc_nwidgets * sizeof(*w));
	if (as)
		kmem_free(as, sc->sc_nassocs * sizeof(*as));
	if (ctl)
		kmem_free(ctl, sc->sc_nctls * sizeof(*ctl));
	if (mx)
		kmem_free(mx, sc->sc_nmixers * sizeof(*mx));

	mutex_destroy(&sc->sc_lock);
	mutex_destroy(&sc->sc_intr_lock);

	pmf_device_deregister(self);

	return 0;
}

static void
hdafg_childdet(device_t self, device_t child)
{
	struct hdafg_softc *sc = device_private(self);

	if (child == sc->sc_audiodev.ad_audiodev)
		sc->sc_audiodev.ad_audiodev = NULL;
}

static bool
hdafg_suspend(device_t self, const pmf_qual_t *qual)
{
	struct hdafg_softc *sc = device_private(self);

	callout_halt(&sc->sc_jack_callout, NULL);

	return true;
}

static bool
hdafg_resume(device_t self, const pmf_qual_t *qual)
{
	struct hdafg_softc *sc = device_private(self);
	struct hdaudio_widget *w;
	int nid;

	hdaudio_command(sc->sc_codec, sc->sc_nid,
	    CORB_SET_POWER_STATE, COP_POWER_STATE_D0);
	hda_delay(100);
	for (nid = sc->sc_startnode; nid < sc->sc_endnode; nid++) {
		hdaudio_command(sc->sc_codec, nid,
		    CORB_SET_POWER_STATE, COP_POWER_STATE_D0);
		w = hdafg_widget_lookup(sc, nid);		

		/* restore pin widget configuration */
		if (w == NULL || w->w_type != COP_AWCAP_TYPE_PIN_COMPLEX)
			continue;
		hdafg_widget_setconfig(w, w->w_pin.config);
	}
	hda_delay(1000);

	hdafg_commit(sc);
	hdafg_stream_connect(sc, AUMODE_PLAY);
	hdafg_stream_connect(sc, AUMODE_RECORD);

	if (sc->sc_jack_polling)
		hdafg_hp_switch_handler(sc);

	return true;
}

static int
hdafg_query_encoding(void *opaque, struct audio_encoding *ae)
{
	struct hdaudio_audiodev *ad = opaque;
	return auconv_query_encoding(ad->ad_encodings, ae);
}

static int
hdafg_set_params(void *opaque, int setmode, int usemode,
    audio_params_t *play, audio_params_t *rec,
    stream_filter_list_t *pfil, stream_filter_list_t *rfil)
{
	struct hdaudio_audiodev *ad = opaque;
	int index;

	if (play && (setmode & AUMODE_PLAY)) {
		index = auconv_set_converter(ad->ad_formats, ad->ad_nformats,
		    AUMODE_PLAY, play, TRUE, pfil);
		if (index < 0)
			return EINVAL;
		ad->ad_sc->sc_pparam = pfil->req_size > 0 ?
		    pfil->filters[0].param : *play;
		hdafg_stream_connect(ad->ad_sc, AUMODE_PLAY);
	}
	if (rec && (setmode & AUMODE_RECORD)) {
		index = auconv_set_converter(ad->ad_formats, ad->ad_nformats,
		    AUMODE_RECORD, rec, TRUE, rfil);
		if (index < 0)
			return EINVAL;
		ad->ad_sc->sc_rparam = rfil->req_size > 0 ?
		    rfil->filters[0].param : *rec;
		hdafg_stream_connect(ad->ad_sc, AUMODE_RECORD);
	}
	return 0;
}

static int
hdafg_round_blocksize(void *opaque, int blksize, int mode,
    const audio_params_t *param)
{
	struct hdaudio_audiodev *ad = opaque;
	struct hdaudio_stream *st;
	int bufsize, nblksize;

	st = (mode == AUMODE_PLAY) ? ad->ad_playback : ad->ad_capture;
	if (st == NULL) {
		hda_trace(ad->ad_sc,
		    "round_blocksize called for invalid stream\n");
		return 128;
	}

	if (blksize > 8192)
		blksize = 8192;
	else if (blksize < 0)
		blksize = 128;

	/* HD audio wants a multiple of 128, and OSS wants a power of 2 */
	for (nblksize = 128; nblksize < blksize; nblksize <<= 1)
		;

	/* Make sure there are enough BDL descriptors */
	bufsize = st->st_data.dma_size;
	if (bufsize > HDAUDIO_BDL_MAX * nblksize) {
		blksize = bufsize / HDAUDIO_BDL_MAX;
		for (nblksize = 128; nblksize < blksize; nblksize <<= 1)
			;
	}

	return nblksize;
}

static int
hdafg_commit_settings(void *opaque)
{
	return 0;
}

static int
hdafg_halt_output(void *opaque)
{
	struct hdaudio_audiodev *ad = opaque;
	struct hdafg_softc *sc = ad->ad_sc;
	struct hdaudio_assoc *as = ad->ad_sc->sc_assocs;
	struct hdaudio_widget *w;
	uint16_t dfmt;
	int i, j;

	/* Disable digital outputs */
	for (i = 0; i < sc->sc_nassocs; i++) {
		if (as[i].as_enable == false)
			continue;
		if (as[i].as_dir != HDAUDIO_PINDIR_OUT)
			continue;
		for (j = 0; j < HDAUDIO_MAXPINS; j++) {
			if (as[i].as_dacs[j] == 0)
				continue;
			w = hdafg_widget_lookup(sc, as[i].as_dacs[j]);
			if (w == NULL || w->w_enable == false)
				continue;
			if (w->w_p.aw_cap & COP_AWCAP_DIGITAL) {
				dfmt = hdaudio_command(sc->sc_codec, w->w_nid,
				    CORB_GET_DIGITAL_CONVERTER_CONTROL, 0) &
				    0xff;
				dfmt &= ~COP_DIGITAL_CONVCTRL1_DIGEN;
				hdaudio_command(sc->sc_codec, w->w_nid,
				    CORB_SET_DIGITAL_CONVERTER_CONTROL_1, dfmt);
			}
		}
	}

	hdaudio_stream_stop(ad->ad_playback);

	return 0;
}

static int
hdafg_halt_input(void *opaque)
{
	struct hdaudio_audiodev *ad = opaque;

	hdaudio_stream_stop(ad->ad_capture);

	return 0;
}

static int
hdafg_getdev(void *opaque, struct audio_device *audiodev)
{
	struct hdaudio_audiodev *ad = opaque;
	struct hdafg_softc *sc = ad->ad_sc;

	hdaudio_findvendor(audiodev->name, sizeof(audiodev->name),
	    sc->sc_vendor);
	hdaudio_findproduct(audiodev->version, sizeof(audiodev->version),
	    sc->sc_vendor, sc->sc_product);
	snprintf(audiodev->config, sizeof(audiodev->config) - 1,
	    "%02Xh", sc->sc_nid);

	return 0;
}

static int
hdafg_set_port(void *opaque, mixer_ctrl_t *mc)
{
	struct hdaudio_audiodev *ad = opaque;
	struct hdafg_softc *sc = ad->ad_sc;
	struct hdaudio_mixer *mx;
	struct hdaudio_control *ctl;
	int i, divisor;

	if (mc->dev < 0 || mc->dev >= sc->sc_nmixers)
		return EINVAL;
	mx = &sc->sc_mixers[mc->dev];
	ctl = mx->mx_ctl;
	if (ctl == NULL) {
		if (mx->mx_di.type != AUDIO_MIXER_SET)
			return ENXIO;
		if (mx->mx_di.mixer_class != HDAUDIO_MIXER_CLASS_OUTPUTS &&
		    mx->mx_di.mixer_class != HDAUDIO_MIXER_CLASS_RECORD)
			return ENXIO;
		for (i = 0; i < sc->sc_nassocs; i++) {
			if (sc->sc_assocs[i].as_dir != HDAUDIO_PINDIR_OUT &&
			    mx->mx_di.mixer_class ==
			    HDAUDIO_MIXER_CLASS_OUTPUTS)
				continue;
			if (sc->sc_assocs[i].as_dir != HDAUDIO_PINDIR_IN &&
			    mx->mx_di.mixer_class ==
			    HDAUDIO_MIXER_CLASS_RECORD)
				continue;
			sc->sc_assocs[i].as_activated =
			    (mc->un.mask & (1 << i)) ? true : false;
		}
		hdafg_stream_connect(ad->ad_sc,
		    mx->mx_di.mixer_class == HDAUDIO_MIXER_CLASS_OUTPUTS ?
		    AUMODE_PLAY : AUMODE_RECORD);
		return 0;
	}

	switch (mx->mx_di.type) {
	case AUDIO_MIXER_VALUE:
		if (ctl->ctl_step == 0)
			divisor = 128; /* ??? - just avoid div by 0 */
		else
			divisor = 255 / ctl->ctl_step;

		hdafg_control_amp_set(ctl, HDAUDIO_AMP_MUTE_NONE,
		  mc->un.value.level[AUDIO_MIXER_LEVEL_LEFT] / divisor,
		  mc->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] / divisor);
		break;
	case AUDIO_MIXER_ENUM:
		hdafg_control_amp_set(ctl,
		    mc->un.ord ? HDAUDIO_AMP_MUTE_ALL : HDAUDIO_AMP_MUTE_NONE,
		    ctl->ctl_left, ctl->ctl_right);
		break;
	default:
		return ENXIO;
	}
	    
	return 0;
}

static int
hdafg_get_port(void *opaque, mixer_ctrl_t *mc)
{
	struct hdaudio_audiodev *ad = opaque;
	struct hdafg_softc *sc = ad->ad_sc;
	struct hdaudio_mixer *mx;
	struct hdaudio_control *ctl;
	u_int mask = 0;
	int i, factor;

	if (mc->dev < 0 || mc->dev >= sc->sc_nmixers)
		return EINVAL;
	mx = &sc->sc_mixers[mc->dev];
	ctl = mx->mx_ctl;
	if (ctl == NULL) {
		if (mx->mx_di.type != AUDIO_MIXER_SET)
			return ENXIO;
		if (mx->mx_di.mixer_class != HDAUDIO_MIXER_CLASS_OUTPUTS &&
		    mx->mx_di.mixer_class != HDAUDIO_MIXER_CLASS_RECORD)
			return ENXIO;
		for (i = 0; i < sc->sc_nassocs; i++) {
			if (sc->sc_assocs[i].as_enable == false)
				continue;
			if (sc->sc_assocs[i].as_activated == false)
				continue;
			if (sc->sc_assocs[i].as_dir == HDAUDIO_PINDIR_OUT &&
			    mx->mx_di.mixer_class ==
			    HDAUDIO_MIXER_CLASS_OUTPUTS)
				mask |= (1 << i);
			if (sc->sc_assocs[i].as_dir == HDAUDIO_PINDIR_IN &&
			    mx->mx_di.mixer_class ==
			    HDAUDIO_MIXER_CLASS_RECORD)
				mask |= (1 << i);
		}
		mc->un.mask = mask;
		return 0;
	}

	switch (mx->mx_di.type) {
	case AUDIO_MIXER_VALUE:
		if (ctl->ctl_step == 0)
			factor = 128; /* ??? - just avoid div by 0 */
		else
			factor = 255 / ctl->ctl_step;

		mc->un.value.level[AUDIO_MIXER_LEVEL_LEFT] = ctl->ctl_left * factor;
		mc->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = ctl->ctl_right * factor;
		break;
	case AUDIO_MIXER_ENUM:
		mc->un.ord = (ctl->ctl_muted || ctl->ctl_forcemute) ? 1 : 0;
		break;
	default:
		return ENXIO;
	}
	return 0;
}

static int
hdafg_query_devinfo(void *opaque, mixer_devinfo_t *di)
{
	struct hdaudio_audiodev *ad = opaque;
	struct hdafg_softc *sc = ad->ad_sc;

	if (di->index < 0 || di->index >= sc->sc_nmixers)
		return ENXIO;

	*di = sc->sc_mixers[di->index].mx_di;

	return 0;
}

static void *
hdafg_allocm(void *opaque, int direction, size_t size)
{
	struct hdaudio_audiodev *ad = opaque;
	struct hdaudio_stream *st;
	int err;

	st = (direction == AUMODE_PLAY) ? ad->ad_playback : ad->ad_capture;
	if (st == NULL)
		return NULL;

	if (st->st_data.dma_valid == true)
		hda_error(ad->ad_sc, "WARNING: allocm leak\n");
	
	st->st_data.dma_size = size;
	err = hdaudio_dma_alloc(st->st_host, &st->st_data,
	    BUS_DMA_COHERENT | BUS_DMA_NOCACHE);
	if (err || st->st_data.dma_valid == false)
		return NULL;

	return DMA_KERNADDR(&st->st_data);
}

static void
hdafg_freem(void *opaque, void *addr, size_t size)
{
	struct hdaudio_audiodev *ad = opaque;
	struct hdaudio_stream *st;

	if (addr == DMA_KERNADDR(&ad->ad_playback->st_data))
		st = ad->ad_playback;
	else if (addr == DMA_KERNADDR(&ad->ad_capture->st_data))
		st = ad->ad_capture;
	else
		return;

	hdaudio_dma_free(st->st_host, &st->st_data);
}

static size_t
hdafg_round_buffersize(void *opaque, int direction, size_t bufsize)
{
	/* Multiple of 128 */
	bufsize &= ~127;
	if (bufsize <= 0)
		bufsize = 128;
	return bufsize;
}

static paddr_t
hdafg_mappage(void *opaque, void *addr, off_t off, int prot)
{
	struct hdaudio_audiodev *ad = opaque;
	struct hdaudio_stream *st;

	if (addr == DMA_KERNADDR(&ad->ad_playback->st_data))
		st = ad->ad_playback;
	else if (addr == DMA_KERNADDR(&ad->ad_capture->st_data))
		st = ad->ad_capture;
	else
		return -1;

	if (st->st_data.dma_valid == false)
		return -1;

	return bus_dmamem_mmap(st->st_host->sc_dmat, st->st_data.dma_segs,
	    st->st_data.dma_nsegs, off, prot, BUS_DMA_WAITOK);
}

static int
hdafg_get_props(void *opaque)
{
	struct hdaudio_audiodev *ad = opaque;
	int props = AUDIO_PROP_MMAP;

	if (ad->ad_playback)
		props |= AUDIO_PROP_PLAYBACK;
	if (ad->ad_capture)
		props |= AUDIO_PROP_CAPTURE;
	if (ad->ad_playback && ad->ad_capture) {
		props |= AUDIO_PROP_FULLDUPLEX;
		props |= AUDIO_PROP_INDEPENDENT;
	}

	return props;
}

static int
hdafg_trigger_output(void *opaque, void *start, void *end, int blksize,
    void (*intr)(void *), void *intrarg, const audio_params_t *param)
{
	struct hdaudio_audiodev *ad = opaque;
	bus_size_t dmasize;

	if (ad->ad_playback == NULL)
		return ENXIO;
	if (ad->ad_playback->st_data.dma_valid == false)
		return ENOMEM;

	ad->ad_playbackintr = intr;
	ad->ad_playbackintrarg = intrarg;

	dmasize = (char *)end - (char *)start;
	hdafg_stream_connect(ad->ad_sc, AUMODE_PLAY);
	hdaudio_stream_start(ad->ad_playback, blksize, dmasize,
	    &ad->ad_sc->sc_pparam);

	return 0;
}

static int
hdafg_trigger_input(void *opaque, void *start, void *end, int blksize,
    void (*intr)(void *), void *intrarg, const audio_params_t *param)
{
	struct hdaudio_audiodev *ad = opaque;
	bus_size_t dmasize;

	if (ad->ad_capture == NULL)
		return ENXIO;
	if (ad->ad_capture->st_data.dma_valid == false)
		return ENOMEM;

	ad->ad_captureintr = intr;
	ad->ad_captureintrarg = intrarg;

	dmasize = (char *)end - (char *)start;
	hdafg_stream_connect(ad->ad_sc, AUMODE_RECORD);
	hdaudio_stream_start(ad->ad_capture, blksize, dmasize,
	    &ad->ad_sc->sc_rparam);

	return 0;
}

static void
hdafg_get_locks(void *opaque, kmutex_t **intr, kmutex_t **thread)
{
	struct hdaudio_audiodev *ad = opaque;

	*intr = &ad->ad_sc->sc_intr_lock;
	*thread = &ad->ad_sc->sc_lock;
}

static int
hdafg_unsol(device_t self, uint8_t tag)
{
	struct hdafg_softc *sc = device_private(self);
	struct hdaudio_assoc *as = sc->sc_assocs;
	int i, j;

	switch (tag) {
	case HDAUDIO_UNSOLTAG_EVENT_DD:
		hda_print(sc, "unsol: display device hotplug\n");
		for (i = 0; i < sc->sc_nassocs; i++) {
			if (as[i].as_displaydev == false)
				continue;
			for (j = 0; j < HDAUDIO_MAXPINS; j++) {
				if (as[i].as_pins[j] == 0)
					continue;
				hdafg_assoc_dump_dd(sc, &as[i], j, 0);
			}
		}
		break;
	default:
		hda_print(sc, "unsol: tag=%u\n", tag);
		break;
	}

	return 0;
}

static int
hdafg_widget_info(void *opaque, prop_dictionary_t request,
    prop_dictionary_t response)
{
	struct hdafg_softc *sc = opaque;
	struct hdaudio_widget *w;
	prop_array_t connlist;
	uint32_t config, wcap;
	uint16_t index;
	int nid;
	int i;

	if (prop_dictionary_get_uint16(request, "index", &index) == false)
		return EINVAL;

	nid = sc->sc_startnode + index;
	if (nid >= sc->sc_endnode)
		return EINVAL;

	w = hdafg_widget_lookup(sc, nid);
	if (w == NULL)
		return ENXIO;
	wcap = hda_get_wparam(w, PIN_CAPABILITIES);
	config = hdaudio_command(sc->sc_codec, w->w_nid,
	    CORB_GET_CONFIGURATION_DEFAULT, 0);
	prop_dictionary_set_cstring_nocopy(response, "name", w->w_name);
	prop_dictionary_set_bool(response, "enable", w->w_enable);
	prop_dictionary_set_uint8(response, "nid", w->w_nid);
	prop_dictionary_set_uint8(response, "type", w->w_type);
	prop_dictionary_set_uint32(response, "config", config);
	prop_dictionary_set_uint32(response, "cap", wcap);
	if (w->w_nconns == 0)
		return 0;
	connlist = prop_array_create();
	for (i = 0; i < w->w_nconns; i++) {
		if (w->w_conns[i] == 0)
			continue;
		prop_array_add(connlist,
		    prop_number_create_unsigned_integer(w->w_conns[i]));
	}
	prop_dictionary_set(response, "connlist", connlist);
	prop_object_release(connlist);
	return 0;
}

static int
hdafg_codec_info(void *opaque, prop_dictionary_t request,
    prop_dictionary_t response)
{
	struct hdafg_softc *sc = opaque;
	prop_dictionary_set_uint16(response, "vendor-id",
	    sc->sc_vendor);
	prop_dictionary_set_uint16(response, "product-id",
	    sc->sc_product);
	return 0;
}

MODULE(MODULE_CLASS_DRIVER, hdafg, "hdaudio");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
hdafg_modcmd(modcmd_t cmd, void *opaque)
{
	int error = 0;

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_hdafg,
		    cfattach_ioconf_hdafg, cfdata_ioconf_hdafg);
#endif
		return error;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		error = config_fini_component(cfdriver_ioconf_hdafg,
		    cfattach_ioconf_hdafg, cfdata_ioconf_hdafg);
#endif
		return error;
	default:
		return ENOTTY;
	}
}

#define HDAFG_GET_ANACTRL 		0xfe0
#define HDAFG_SET_ANACTRL 		0x7e0
#define HDAFG_ANALOG_BEEP_EN		__BIT(5)
#define HDAFG_ALC231_MONO_OUT_MIXER 	0xf
#define HDAFG_STAC9200_AFG		0x1
#define HDAFG_STAC9200_GET_ANACTRL_PAYLOAD	0x0
#define HDAFG_ALC231_INPUT_BOTH_CHANNELS_UNMUTE	0x7100

static void
hdafg_enable_analog_beep(struct hdafg_softc *sc)
{
	int nid;
	uint32_t response;
	
	switch (sc->sc_vendor) {
	case HDAUDIO_VENDOR_SIGMATEL:
		switch (sc->sc_product) {
		case HDAUDIO_PRODUCT_SIGMATEL_STAC9200:
		case HDAUDIO_PRODUCT_SIGMATEL_STAC9200D:
		case HDAUDIO_PRODUCT_SIGMATEL_STAC9202:
		case HDAUDIO_PRODUCT_SIGMATEL_STAC9202D:
		case HDAUDIO_PRODUCT_SIGMATEL_STAC9204:
		case HDAUDIO_PRODUCT_SIGMATEL_STAC9204D:
		case HDAUDIO_PRODUCT_SIGMATEL_STAC9205:
		case HDAUDIO_PRODUCT_SIGMATEL_STAC9205_1:
		case HDAUDIO_PRODUCT_SIGMATEL_STAC9205D:
			nid = HDAFG_STAC9200_AFG;

			response = hdaudio_command(sc->sc_codec, nid,
			    HDAFG_GET_ANACTRL,
			    HDAFG_STAC9200_GET_ANACTRL_PAYLOAD);
			hda_delay(100);

			response |= HDAFG_ANALOG_BEEP_EN;

			hdaudio_command(sc->sc_codec, nid, HDAFG_SET_ANACTRL,
			    response);
			hda_delay(100);
			break;
		default:
			break;
		}
		break;
	case HDAUDIO_VENDOR_REALTEK:
		switch (sc->sc_product) {
		case HDAUDIO_PRODUCT_REALTEK_ALC269:
			/* The Panasonic Toughbook CF19 - Mk 5 uses a Realtek
			 * ALC231 that identifies as an ALC269.
			 * This unmutes the PCBEEP on the speaker.
			 */
 			nid = HDAFG_ALC231_MONO_OUT_MIXER;
			response = hdaudio_command(sc->sc_codec, nid,
			    CORB_SET_AMPLIFIER_GAIN_MUTE,
			    HDAFG_ALC231_INPUT_BOTH_CHANNELS_UNMUTE);
			hda_delay(100);
			break;
		default:
			break;
		}
	default:
		break;
	}
}

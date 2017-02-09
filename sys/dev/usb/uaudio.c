/*	$NetBSD: uaudio.c,v 1.144 2015/01/26 20:56:44 gson Exp $	*/

/*
 * Copyright (c) 1999, 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology, and Matthew R. Green (mrg@eterna.com.au).
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
 * USB audio specs: http://www.usb.org/developers/docs/devclass_docs/audio10.pdf
 *                  http://www.usb.org/developers/docs/devclass_docs/frmts10.pdf
 *                  http://www.usb.org/developers/docs/devclass_docs/termt10.pdf
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uaudio.c,v 1.144 2015/01/26 20:56:44 gson Exp $");

#ifdef _KERNEL_OPT
#include "opt_usb.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/reboot.h>		/* for bootverbose */
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/poll.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/atomic.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/audiovar.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/usbdevs.h>

#include <dev/usb/uaudioreg.h>

/* #define UAUDIO_DEBUG */
/* #define UAUDIO_MULTIPLE_ENDPOINTS */
#ifdef UAUDIO_DEBUG
#define DPRINTF(x,y...)		do { \
		if (uaudiodebug) { \
			struct lwp *l = curlwp; \
			printf("%s[%d:%d]: "x, __func__, l->l_proc->p_pid, l->l_lid, y); \
		} \
	} while (0)
#define DPRINTFN_CLEAN(n,x...)	do { \
		if (uaudiodebug > (n)) \
			printf(x); \
	} while (0)
#define DPRINTFN(n,x,y...)	do { \
		if (uaudiodebug > (n)) { \
			struct lwp *l = curlwp; \
			printf("%s[%d:%d]: "x, __func__, l->l_proc->p_pid, l->l_lid, y); \
		} \
	} while (0)
int	uaudiodebug = 0;
#else
#define DPRINTF(x,y...)
#define DPRINTFN_CLEAN(n,x...)
#define DPRINTFN(n,x,y...)
#endif

#define UAUDIO_NCHANBUFS 6	/* number of outstanding request */
#define UAUDIO_NFRAMES   10	/* ms of sound in each request */


#define MIX_MAX_CHAN 8
struct mixerctl {
	uint16_t	wValue[MIX_MAX_CHAN]; /* using nchan */
	uint16_t	wIndex;
	uint8_t		nchan;
	uint8_t		type;
#define MIX_ON_OFF	1
#define MIX_SIGNED_16	2
#define MIX_UNSIGNED_16	3
#define MIX_SIGNED_8	4
#define MIX_SELECTOR	5
#define MIX_SIZE(n) ((n) == MIX_SIGNED_16 || (n) == MIX_UNSIGNED_16 ? 2 : 1)
#define MIX_UNSIGNED(n) ((n) == MIX_UNSIGNED_16)
	int		minval, maxval;
	u_int		delta;
	u_int		mul;
	uint8_t		class;
	char		ctlname[MAX_AUDIO_DEV_LEN];
	const char	*ctlunit;
};
#define MAKE(h,l) (((h) << 8) | (l))

struct as_info {
	uint8_t		alt;
	uint8_t		encoding;
	uint8_t		attributes; /* Copy of bmAttributes of
				     * usb_audio_streaming_endpoint_descriptor
				     */
	usbd_interface_handle	ifaceh;
	const usb_interface_descriptor_t *idesc;
	const usb_endpoint_descriptor_audio_t *edesc;
	const usb_endpoint_descriptor_audio_t *edesc1;
	const struct usb_audio_streaming_type1_descriptor *asf1desc;
	struct audio_format *aformat;
	int		sc_busy;	/* currently used */
};

struct chan {
	void	(*intr)(void *);	/* DMA completion intr handler */
	void	*arg;		/* arg for intr() */
	usbd_pipe_handle pipe;
	usbd_pipe_handle sync_pipe;

	u_int	sample_size;
	u_int	sample_rate;
	u_int	bytes_per_frame;
	u_int	fraction;	/* fraction/1000 is the extra samples/frame */
	u_int	residue;	/* accumulates the fractional samples */

	u_char	*start;		/* upper layer buffer start */
	u_char	*end;		/* upper layer buffer end */
	u_char	*cur;		/* current position in upper layer buffer */
	int	blksize;	/* chunk size to report up */
	int	transferred;	/* transferred bytes not reported up */

	int	altidx;		/* currently used altidx */

	int	curchanbuf;
	struct chanbuf {
		struct chan	*chan;
		usbd_xfer_handle xfer;
		u_char		*buffer;
		uint16_t	sizes[UAUDIO_NFRAMES];
		uint16_t	offsets[UAUDIO_NFRAMES];
		uint16_t	size;
	} chanbufs[UAUDIO_NCHANBUFS];

	struct uaudio_softc *sc; /* our softc */
};

/*
 * XXX Locking notes:
 *
 *    The MI USB audio subsystem is not MP-SAFE.  Our strategy here
 *    is to ensure we have the kernel lock held when calling into
 *    usbd, and, generally, to have dropped the sc_intr_lock during
 *    these sections as well since the usb code will sleep.
 */
struct uaudio_softc {
	device_t	sc_dev;		/* base device */
	kmutex_t	sc_lock;
	kmutex_t	sc_intr_lock;
	usbd_device_handle sc_udev;	/* USB device */
	int		sc_ac_iface;	/* Audio Control interface */
	usbd_interface_handle	sc_ac_ifaceh;
	struct chan	sc_playchan;	/* play channel */
	struct chan	sc_recchan;	/* record channel */
	int		sc_nullalt;
	int		sc_audio_rev;
	struct as_info	*sc_alts;	/* alternate settings */
	int		sc_nalts;	/* # of alternate settings */
	int		sc_altflags;
#define HAS_8		0x01
#define HAS_16		0x02
#define HAS_8U		0x04
#define HAS_ALAW	0x08
#define HAS_MULAW	0x10
#define UA_NOFRAC	0x20		/* don't do sample rate adjustment */
#define HAS_24		0x40
	int		sc_mode;	/* play/record capability */
	struct mixerctl *sc_ctls;	/* mixer controls */
	int		sc_nctls;	/* # of mixer controls */
	device_t	sc_audiodev;
	struct audio_format *sc_formats;
	int		sc_nformats;
	struct audio_encoding_set *sc_encodings;
	u_int		sc_channel_config;
	char		sc_dying;
	struct audio_device sc_adev;
};

struct terminal_list {
	int size;
	uint16_t terminals[1];
};
#define TERMINAL_LIST_SIZE(N)	(offsetof(struct terminal_list, terminals) \
				+ sizeof(uint16_t) * (N))

struct io_terminal {
	union {
		const uaudio_cs_descriptor_t *desc;
		const struct usb_audio_input_terminal *it;
		const struct usb_audio_output_terminal *ot;
		const struct usb_audio_mixer_unit *mu;
		const struct usb_audio_selector_unit *su;
		const struct usb_audio_feature_unit *fu;
		const struct usb_audio_processing_unit *pu;
		const struct usb_audio_extension_unit *eu;
	} d;
	int inputs_size;
	struct terminal_list **inputs; /* list of source input terminals */
	struct terminal_list *output; /* list of destination output terminals */
	int direct;		/* directly connected to an output terminal */
};

#define UAC_OUTPUT	0
#define UAC_INPUT	1
#define UAC_EQUAL	2
#define UAC_RECORD	3
#define UAC_NCLASSES	4
#ifdef UAUDIO_DEBUG
Static const char *uac_names[] = {
	AudioCoutputs, AudioCinputs, AudioCequalization, AudioCrecord,
};
#endif

#ifdef UAUDIO_DEBUG
Static void uaudio_dump_tml
	(struct terminal_list *tml);
#endif
Static usbd_status uaudio_identify_ac
	(struct uaudio_softc *, const usb_config_descriptor_t *);
Static usbd_status uaudio_identify_as
	(struct uaudio_softc *, const usb_config_descriptor_t *);
Static usbd_status uaudio_process_as
	(struct uaudio_softc *, const char *, int *, int,
	 const usb_interface_descriptor_t *);

Static void	uaudio_add_alt(struct uaudio_softc *, const struct as_info *);

Static const usb_interface_descriptor_t *uaudio_find_iface
	(const char *, int, int *, int);

Static void	uaudio_mixer_add_ctl(struct uaudio_softc *, struct mixerctl *);
Static char	*uaudio_id_name
	(struct uaudio_softc *, const struct io_terminal *, int);
#ifdef UAUDIO_DEBUG
Static void	uaudio_dump_cluster(const struct usb_audio_cluster *);
#endif
Static struct usb_audio_cluster uaudio_get_cluster
	(int, const struct io_terminal *);
Static void	uaudio_add_input
	(struct uaudio_softc *, const struct io_terminal *, int);
Static void	uaudio_add_output
	(struct uaudio_softc *, const struct io_terminal *, int);
Static void	uaudio_add_mixer
	(struct uaudio_softc *, const struct io_terminal *, int);
Static void	uaudio_add_selector
	(struct uaudio_softc *, const struct io_terminal *, int);
#ifdef UAUDIO_DEBUG
Static const char *uaudio_get_terminal_name(int);
#endif
Static int	uaudio_determine_class
	(const struct io_terminal *, struct mixerctl *);
Static const char *uaudio_feature_name
	(const struct io_terminal *, struct mixerctl *);
Static void	uaudio_add_feature
	(struct uaudio_softc *, const struct io_terminal *, int);
Static void	uaudio_add_processing_updown
	(struct uaudio_softc *, const struct io_terminal *, int);
Static void	uaudio_add_processing
	(struct uaudio_softc *, const struct io_terminal *, int);
Static void	uaudio_add_extension
	(struct uaudio_softc *, const struct io_terminal *, int);
Static struct terminal_list *uaudio_merge_terminal_list
	(const struct io_terminal *);
Static struct terminal_list *uaudio_io_terminaltype
	(int, struct io_terminal *, int);
Static usbd_status uaudio_identify
	(struct uaudio_softc *, const usb_config_descriptor_t *);

Static int	uaudio_signext(int, int);
Static int	uaudio_value2bsd(struct mixerctl *, int);
Static int	uaudio_bsd2value(struct mixerctl *, int);
Static int	uaudio_get(struct uaudio_softc *, int, int, int, int, int);
Static int	uaudio_ctl_get
	(struct uaudio_softc *, int, struct mixerctl *, int);
Static void	uaudio_set
	(struct uaudio_softc *, int, int, int, int, int, int);
Static void	uaudio_ctl_set
	(struct uaudio_softc *, int, struct mixerctl *, int, int);

Static usbd_status uaudio_set_speed(struct uaudio_softc *, int, u_int);

Static usbd_status uaudio_chan_open(struct uaudio_softc *, struct chan *);
Static void	uaudio_chan_close(struct uaudio_softc *, struct chan *);
Static usbd_status uaudio_chan_alloc_buffers
	(struct uaudio_softc *, struct chan *);
Static void	uaudio_chan_free_buffers(struct uaudio_softc *, struct chan *);
Static void	uaudio_chan_init
	(struct chan *, int, const struct audio_params *, int);
Static void	uaudio_chan_set_param(struct chan *, u_char *, u_char *, int);
Static void	uaudio_chan_ptransfer(struct chan *);
Static void	uaudio_chan_pintr
	(usbd_xfer_handle, usbd_private_handle, usbd_status);

Static void	uaudio_chan_rtransfer(struct chan *);
Static void	uaudio_chan_rintr
	(usbd_xfer_handle, usbd_private_handle, usbd_status);

Static int	uaudio_open(void *, int);
Static void	uaudio_close(void *);
Static int	uaudio_drain(void *);
Static int	uaudio_query_encoding(void *, struct audio_encoding *);
Static int	uaudio_set_params
	(void *, int, int, struct audio_params *, struct audio_params *,
	 stream_filter_list_t *, stream_filter_list_t *);
Static int	uaudio_round_blocksize(void *, int, int, const audio_params_t *);
Static int	uaudio_trigger_output
	(void *, void *, void *, int, void (*)(void *), void *,
	 const audio_params_t *);
Static int	uaudio_trigger_input
	(void *, void *, void *, int, void (*)(void *), void *,
	 const audio_params_t *);
Static int	uaudio_halt_in_dma(void *);
Static int	uaudio_halt_out_dma(void *);
Static int	uaudio_getdev(void *, struct audio_device *);
Static int	uaudio_mixer_set_port(void *, mixer_ctrl_t *);
Static int	uaudio_mixer_get_port(void *, mixer_ctrl_t *);
Static int	uaudio_query_devinfo(void *, mixer_devinfo_t *);
Static int	uaudio_get_props(void *);
Static void	uaudio_get_locks(void *, kmutex_t **, kmutex_t **);

Static const struct audio_hw_if uaudio_hw_if = {
	uaudio_open,
	uaudio_close,
	uaudio_drain,
	uaudio_query_encoding,
	uaudio_set_params,
	uaudio_round_blocksize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	uaudio_halt_out_dma,
	uaudio_halt_in_dma,
	NULL,
	uaudio_getdev,
	NULL,
	uaudio_mixer_set_port,
	uaudio_mixer_get_port,
	uaudio_query_devinfo,
	NULL,
	NULL,
	NULL,
	NULL,
	uaudio_get_props,
	uaudio_trigger_output,
	uaudio_trigger_input,
	NULL,
	uaudio_get_locks,
};

int uaudio_match(device_t, cfdata_t, void *);
void uaudio_attach(device_t, device_t, void *);
int uaudio_detach(device_t, int);
void uaudio_childdet(device_t, device_t);
int uaudio_activate(device_t, enum devact);

extern struct cfdriver uaudio_cd;

CFATTACH_DECL2_NEW(uaudio, sizeof(struct uaudio_softc),
    uaudio_match, uaudio_attach, uaudio_detach, uaudio_activate, NULL,
    uaudio_childdet);

int
uaudio_match(device_t parent, cfdata_t match, void *aux)
{
	struct usbif_attach_arg *uaa = aux;

	/* Trigger on the control interface. */
	if (uaa->class != UICLASS_AUDIO ||
	    uaa->subclass != UISUBCLASS_AUDIOCONTROL ||
	    (usbd_get_quirks(uaa->device)->uq_flags & UQ_BAD_AUDIO))
		return UMATCH_NONE;

	return UMATCH_IFACECLASS_IFACESUBCLASS;
}

void
uaudio_attach(device_t parent, device_t self, void *aux)
{
	struct uaudio_softc *sc = device_private(self);
	struct usbif_attach_arg *uaa = aux;
	usb_interface_descriptor_t *id;
	usb_config_descriptor_t *cdesc;
	char *devinfop;
	usbd_status err;
	int i, j, found;

	sc->sc_dev = self;
	sc->sc_udev = uaa->device;
	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_SCHED);

	strlcpy(sc->sc_adev.name, "USB audio", sizeof(sc->sc_adev.name));
	strlcpy(sc->sc_adev.version, "", sizeof(sc->sc_adev.version));
	snprintf(sc->sc_adev.config, sizeof(sc->sc_adev.config), "usb:%08x",
	    sc->sc_udev->cookie.cookie);

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(uaa->device, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	cdesc = usbd_get_config_descriptor(sc->sc_udev);
	if (cdesc == NULL) {
		aprint_error_dev(self,
		    "failed to get configuration descriptor\n");
		return;
	}

	err = uaudio_identify(sc, cdesc);
	if (err) {
		aprint_error_dev(self,
		    "audio descriptors make no sense, error=%d\n", err);
		return;
	}

	sc->sc_ac_ifaceh = uaa->iface;
	/* Pick up the AS interface. */
	for (i = 0; i < uaa->nifaces; i++) {
		if (uaa->ifaces[i] == NULL)
			continue;
		id = usbd_get_interface_descriptor(uaa->ifaces[i]);
		if (id == NULL)
			continue;
		found = 0;
		for (j = 0; j < sc->sc_nalts; j++) {
			if (id->bInterfaceNumber ==
			    sc->sc_alts[j].idesc->bInterfaceNumber) {
				sc->sc_alts[j].ifaceh = uaa->ifaces[i];
				found = 1;
			}
		}
		if (found)
			uaa->ifaces[i] = NULL;
	}

	for (j = 0; j < sc->sc_nalts; j++) {
		if (sc->sc_alts[j].ifaceh == NULL) {
			aprint_error_dev(self,
			    "alt %d missing AS interface(s)\n", j);
			return;
		}
	}

	aprint_normal_dev(self, "audio rev %d.%02x\n",
	       sc->sc_audio_rev >> 8, sc->sc_audio_rev & 0xff);

	sc->sc_playchan.sc = sc->sc_recchan.sc = sc;
	sc->sc_playchan.altidx = -1;
	sc->sc_recchan.altidx = -1;

	if (usbd_get_quirks(sc->sc_udev)->uq_flags & UQ_AU_NO_FRAC)
		sc->sc_altflags |= UA_NOFRAC;

#ifndef UAUDIO_DEBUG
	if (bootverbose)
#endif
		aprint_normal_dev(self, "%d mixer controls\n",
		    sc->sc_nctls);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   sc->sc_dev);

	DPRINTF("%s", "doing audio_attach_mi\n");
	sc->sc_audiodev = audio_attach_mi(&uaudio_hw_if, sc, sc->sc_dev);

	return;
}

int
uaudio_activate(device_t self, enum devact act)
{
	struct uaudio_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

void
uaudio_childdet(device_t self, device_t child)
{
	struct uaudio_softc *sc = device_private(self);

	KASSERT(sc->sc_audiodev == child);
	sc->sc_audiodev = NULL;
}

int
uaudio_detach(device_t self, int flags)
{
	struct uaudio_softc *sc = device_private(self);
	int rv;

	rv = 0;
	/* Wait for outstanding requests to complete. */
	usbd_delay_ms(sc->sc_udev, UAUDIO_NCHANBUFS * UAUDIO_NFRAMES);

	if (sc->sc_audiodev != NULL)
		rv = config_detach(sc->sc_audiodev, flags);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   sc->sc_dev);

	if (sc->sc_formats != NULL)
		free(sc->sc_formats, M_USBDEV);
	auconv_delete_encodings(sc->sc_encodings);

	mutex_destroy(&sc->sc_lock);
	mutex_destroy(&sc->sc_intr_lock);

	return rv;
}

Static int
uaudio_query_encoding(void *addr, struct audio_encoding *fp)
{
	struct uaudio_softc *sc;
	int flags;

	sc = addr;
	flags = sc->sc_altflags;
	if (sc->sc_dying)
		return EIO;

	if (sc->sc_nalts == 0 || flags == 0)
		return ENXIO;

	return auconv_query_encoding(sc->sc_encodings, fp);
}

Static const usb_interface_descriptor_t *
uaudio_find_iface(const char *tbuf, int size, int *offsp, int subtype)
{
	const usb_interface_descriptor_t *d;

	while (*offsp < size) {
		d = (const void *)(tbuf + *offsp);
		*offsp += d->bLength;
		if (d->bDescriptorType == UDESC_INTERFACE &&
		    d->bInterfaceClass == UICLASS_AUDIO &&
		    d->bInterfaceSubClass == subtype)
			return d;
	}
	return NULL;
}

Static void
uaudio_mixer_add_ctl(struct uaudio_softc *sc, struct mixerctl *mc)
{
	int res;
	size_t len;
	struct mixerctl *nmc;

	if (mc->class < UAC_NCLASSES) {
		DPRINTF("adding %s.%s\n", uac_names[mc->class], mc->ctlname);
	} else {
		DPRINTF("adding %s\n", mc->ctlname);
	}
	len = sizeof(*mc) * (sc->sc_nctls + 1);
	nmc = malloc(len, M_USBDEV, M_NOWAIT);
	if (nmc == NULL) {
		aprint_error("uaudio_mixer_add_ctl: no memory\n");
		return;
	}
	/* Copy old data, if there was any */
	if (sc->sc_nctls != 0) {
		memcpy(nmc, sc->sc_ctls, sizeof(*mc) * (sc->sc_nctls));
		free(sc->sc_ctls, M_USBDEV);
	}
	sc->sc_ctls = nmc;

	mc->delta = 0;
	if (mc->type == MIX_ON_OFF) {
		mc->minval = 0;
		mc->maxval = 1;
	} else if (mc->type == MIX_SELECTOR) {
		;
	} else {
		/* Determine min and max values. */
		mc->minval = uaudio_signext(mc->type,
			uaudio_get(sc, GET_MIN, UT_READ_CLASS_INTERFACE,
				   mc->wValue[0], mc->wIndex,
				   MIX_SIZE(mc->type)));
		mc->maxval = 1 + uaudio_signext(mc->type,
			uaudio_get(sc, GET_MAX, UT_READ_CLASS_INTERFACE,
				   mc->wValue[0], mc->wIndex,
				   MIX_SIZE(mc->type)));
		mc->mul = mc->maxval - mc->minval;
		if (mc->mul == 0)
			mc->mul = 1;
		res = uaudio_get(sc, GET_RES, UT_READ_CLASS_INTERFACE,
				 mc->wValue[0], mc->wIndex,
				 MIX_SIZE(mc->type));
		if (res > 0)
			mc->delta = (res * 255 + mc->mul/2) / mc->mul;
	}

	sc->sc_ctls[sc->sc_nctls++] = *mc;

#ifdef UAUDIO_DEBUG
	if (uaudiodebug > 2) {
		int i;

		DPRINTFN_CLEAN(2, "wValue=%04x", mc->wValue[0]);
		for (i = 1; i < mc->nchan; i++)
			DPRINTFN_CLEAN(2, ",%04x", mc->wValue[i]);
		DPRINTFN_CLEAN(2, " wIndex=%04x type=%d name='%s' unit='%s' "
			 "min=%d max=%d\n",
			 mc->wIndex, mc->type, mc->ctlname, mc->ctlunit,
			 mc->minval, mc->maxval);
	}
#endif
}

Static char *
uaudio_id_name(struct uaudio_softc *sc,
    const struct io_terminal *iot, int id)
{
	static char tbuf[32];

	snprintf(tbuf, sizeof(tbuf), "i%d", id);
	return tbuf;
}

#ifdef UAUDIO_DEBUG
Static void
uaudio_dump_cluster(const struct usb_audio_cluster *cl)
{
	static const char *channel_names[16] = {
		"LEFT", "RIGHT", "CENTER", "LFE",
		"LEFT_SURROUND", "RIGHT_SURROUND", "LEFT_CENTER", "RIGHT_CENTER",
		"SURROUND", "LEFT_SIDE", "RIGHT_SIDE", "TOP",
		"RESERVED12", "RESERVED13", "RESERVED14", "RESERVED15",
	};
	int cc, i, first;

	cc = UGETW(cl->wChannelConfig);
	printf("cluster: bNrChannels=%u wChannelConfig=0x%.4x",
		  cl->bNrChannels, cc);
	first = TRUE;
	for (i = 0; cc != 0; i++) {
		if (cc & 1) {
			printf("%c%s", first ? '<' : ',', channel_names[i]);
			first = FALSE;
		}
		cc = cc >> 1;
	}
	printf("> iChannelNames=%u", cl->iChannelNames);
}
#endif

Static struct usb_audio_cluster
uaudio_get_cluster(int id, const struct io_terminal *iot)
{
	struct usb_audio_cluster r;
	const uaudio_cs_descriptor_t *dp;
	int i;

	for (i = 0; i < 25; i++) { /* avoid infinite loops */
		dp = iot[id].d.desc;
		if (dp == 0)
			goto bad;
		switch (dp->bDescriptorSubtype) {
		case UDESCSUB_AC_INPUT:
			r.bNrChannels = iot[id].d.it->bNrChannels;
			USETW(r.wChannelConfig, UGETW(iot[id].d.it->wChannelConfig));
			r.iChannelNames = iot[id].d.it->iChannelNames;
			return r;
		case UDESCSUB_AC_OUTPUT:
			id = iot[id].d.ot->bSourceId;
			break;
		case UDESCSUB_AC_MIXER:
			r = *(const struct usb_audio_cluster *)
				&iot[id].d.mu->baSourceId[iot[id].d.mu->bNrInPins];
			return r;
		case UDESCSUB_AC_SELECTOR:
			/* XXX This is not really right */
			id = iot[id].d.su->baSourceId[0];
			break;
		case UDESCSUB_AC_FEATURE:
			id = iot[id].d.fu->bSourceId;
			break;
		case UDESCSUB_AC_PROCESSING:
			r = *(const struct usb_audio_cluster *)
				&iot[id].d.pu->baSourceId[iot[id].d.pu->bNrInPins];
			return r;
		case UDESCSUB_AC_EXTENSION:
			r = *(const struct usb_audio_cluster *)
				&iot[id].d.eu->baSourceId[iot[id].d.eu->bNrInPins];
			return r;
		default:
			goto bad;
		}
	}
 bad:
	aprint_error("uaudio_get_cluster: bad data\n");
	memset(&r, 0, sizeof r);
	return r;

}

Static void
uaudio_add_input(struct uaudio_softc *sc, const struct io_terminal *iot, int id)
{
	const struct usb_audio_input_terminal *d;

	d = iot[id].d.it;
#ifdef UAUDIO_DEBUG
	DPRINTFN(2,"bTerminalId=%d wTerminalType=0x%04x "
		    "bAssocTerminal=%d bNrChannels=%d wChannelConfig=%d "
		    "iChannelNames=%d iTerminal=%d\n",
		    d->bTerminalId, UGETW(d->wTerminalType), d->bAssocTerminal,
		    d->bNrChannels, UGETW(d->wChannelConfig),
		    d->iChannelNames, d->iTerminal);
#endif
	/* If USB input terminal, record wChannelConfig */
	if ((UGETW(d->wTerminalType) & 0xff00) != 0x0100)
		return;
	sc->sc_channel_config = UGETW(d->wChannelConfig);
}

Static void
uaudio_add_output(struct uaudio_softc *sc,
    const struct io_terminal *iot, int id)
{
#ifdef UAUDIO_DEBUG
	const struct usb_audio_output_terminal *d;

	d = iot[id].d.ot;
	DPRINTFN(2,"bTerminalId=%d wTerminalType=0x%04x "
		    "bAssocTerminal=%d bSourceId=%d iTerminal=%d\n",
		    d->bTerminalId, UGETW(d->wTerminalType), d->bAssocTerminal,
		    d->bSourceId, d->iTerminal);
#endif
}

Static void
uaudio_add_mixer(struct uaudio_softc *sc, const struct io_terminal *iot, int id)
{
	const struct usb_audio_mixer_unit *d;
	const struct usb_audio_mixer_unit_1 *d1;
	int c, chs, ichs, ochs, i, o, bno, p, mo, mc, k;
	const uByte *bm;
	struct mixerctl mix;

	d = iot[id].d.mu;
	DPRINTFN(2,"bUnitId=%d bNrInPins=%d\n",
		    d->bUnitId, d->bNrInPins);

	/* Compute the number of input channels */
	ichs = 0;
	for (i = 0; i < d->bNrInPins; i++)
		ichs += uaudio_get_cluster(d->baSourceId[i], iot).bNrChannels;

	/* and the number of output channels */
	d1 = (const struct usb_audio_mixer_unit_1 *)&d->baSourceId[d->bNrInPins];
	ochs = d1->bNrChannels;
	DPRINTFN(2,"ichs=%d ochs=%d\n", ichs, ochs);

	bm = d1->bmControls;
	mix.wIndex = MAKE(d->bUnitId, sc->sc_ac_iface);
	uaudio_determine_class(&iot[id], &mix);
	mix.type = MIX_SIGNED_16;
	mix.ctlunit = AudioNvolume;
#define _BIT(bno) ((bm[bno / 8] >> (7 - bno % 8)) & 1)
	for (p = i = 0; i < d->bNrInPins; i++) {
		chs = uaudio_get_cluster(d->baSourceId[i], iot).bNrChannels;
		mc = 0;
		for (c = 0; c < chs; c++) {
			mo = 0;
			for (o = 0; o < ochs; o++) {
				bno = (p + c) * ochs + o;
				if (_BIT(bno))
					mo++;
			}
			if (mo == 1)
				mc++;
		}
		if (mc == chs && chs <= MIX_MAX_CHAN) {
			k = 0;
			for (c = 0; c < chs; c++)
				for (o = 0; o < ochs; o++) {
					bno = (p + c) * ochs + o;
					if (_BIT(bno))
						mix.wValue[k++] =
							MAKE(p+c+1, o+1);
				}
			snprintf(mix.ctlname, sizeof(mix.ctlname), "mix%d-%s",
			    d->bUnitId, uaudio_id_name(sc, iot,
			    d->baSourceId[i]));
			mix.nchan = chs;
			uaudio_mixer_add_ctl(sc, &mix);
		} else {
			/* XXX */
		}
#undef _BIT
		p += chs;
	}

}

Static void
uaudio_add_selector(struct uaudio_softc *sc, const struct io_terminal *iot, int id)
{
	const struct usb_audio_selector_unit *d;
	struct mixerctl mix;
	int i, wp;

	d = iot[id].d.su;
	DPRINTFN(2,"bUnitId=%d bNrInPins=%d\n",
		    d->bUnitId, d->bNrInPins);
	mix.wIndex = MAKE(d->bUnitId, sc->sc_ac_iface);
	mix.wValue[0] = MAKE(0, 0);
	uaudio_determine_class(&iot[id], &mix);
	mix.nchan = 1;
	mix.type = MIX_SELECTOR;
	mix.ctlunit = "";
	mix.minval = 1;
	mix.maxval = d->bNrInPins;
	mix.mul = mix.maxval - mix.minval;
	wp = snprintf(mix.ctlname, MAX_AUDIO_DEV_LEN, "sel%d-", d->bUnitId);
	for (i = 1; i <= d->bNrInPins; i++) {
		wp += snprintf(mix.ctlname + wp, MAX_AUDIO_DEV_LEN - wp,
			       "i%d", d->baSourceId[i - 1]);
		if (wp > MAX_AUDIO_DEV_LEN - 1)
			break;
	}
	uaudio_mixer_add_ctl(sc, &mix);
}

#ifdef UAUDIO_DEBUG
Static const char *
uaudio_get_terminal_name(int terminal_type)
{
	static char tbuf[100];

	switch (terminal_type) {
	/* USB terminal types */
	case UAT_UNDEFINED:	return "UAT_UNDEFINED";
	case UAT_STREAM:	return "UAT_STREAM";
	case UAT_VENDOR:	return "UAT_VENDOR";
	/* input terminal types */
	case UATI_UNDEFINED:	return "UATI_UNDEFINED";
	case UATI_MICROPHONE:	return "UATI_MICROPHONE";
	case UATI_DESKMICROPHONE:	return "UATI_DESKMICROPHONE";
	case UATI_PERSONALMICROPHONE:	return "UATI_PERSONALMICROPHONE";
	case UATI_OMNIMICROPHONE:	return "UATI_OMNIMICROPHONE";
	case UATI_MICROPHONEARRAY:	return "UATI_MICROPHONEARRAY";
	case UATI_PROCMICROPHONEARR:	return "UATI_PROCMICROPHONEARR";
	/* output terminal types */
	case UATO_UNDEFINED:	return "UATO_UNDEFINED";
	case UATO_SPEAKER:	return "UATO_SPEAKER";
	case UATO_HEADPHONES:	return "UATO_HEADPHONES";
	case UATO_DISPLAYAUDIO:	return "UATO_DISPLAYAUDIO";
	case UATO_DESKTOPSPEAKER:	return "UATO_DESKTOPSPEAKER";
	case UATO_ROOMSPEAKER:	return "UATO_ROOMSPEAKER";
	case UATO_COMMSPEAKER:	return "UATO_COMMSPEAKER";
	case UATO_SUBWOOFER:	return "UATO_SUBWOOFER";
	/* bidir terminal types */
	case UATB_UNDEFINED:	return "UATB_UNDEFINED";
	case UATB_HANDSET:	return "UATB_HANDSET";
	case UATB_HEADSET:	return "UATB_HEADSET";
	case UATB_SPEAKERPHONE:	return "UATB_SPEAKERPHONE";
	case UATB_SPEAKERPHONEESUP:	return "UATB_SPEAKERPHONEESUP";
	case UATB_SPEAKERPHONEECANC:	return "UATB_SPEAKERPHONEECANC";
	/* telephony terminal types */
	case UATT_UNDEFINED:	return "UATT_UNDEFINED";
	case UATT_PHONELINE:	return "UATT_PHONELINE";
	case UATT_TELEPHONE:	return "UATT_TELEPHONE";
	case UATT_DOWNLINEPHONE:	return "UATT_DOWNLINEPHONE";
	/* external terminal types */
	case UATE_UNDEFINED:	return "UATE_UNDEFINED";
	case UATE_ANALOGCONN:	return "UATE_ANALOGCONN";
	case UATE_LINECONN:	return "UATE_LINECONN";
	case UATE_LEGACYCONN:	return "UATE_LEGACYCONN";
	case UATE_DIGITALAUIFC:	return "UATE_DIGITALAUIFC";
	case UATE_SPDIF:	return "UATE_SPDIF";
	case UATE_1394DA:	return "UATE_1394DA";
	case UATE_1394DV:	return "UATE_1394DV";
	/* embedded function terminal types */
	case UATF_UNDEFINED:	return "UATF_UNDEFINED";
	case UATF_CALIBNOISE:	return "UATF_CALIBNOISE";
	case UATF_EQUNOISE:	return "UATF_EQUNOISE";
	case UATF_CDPLAYER:	return "UATF_CDPLAYER";
	case UATF_DAT:	return "UATF_DAT";
	case UATF_DCC:	return "UATF_DCC";
	case UATF_MINIDISK:	return "UATF_MINIDISK";
	case UATF_ANALOGTAPE:	return "UATF_ANALOGTAPE";
	case UATF_PHONOGRAPH:	return "UATF_PHONOGRAPH";
	case UATF_VCRAUDIO:	return "UATF_VCRAUDIO";
	case UATF_VIDEODISCAUDIO:	return "UATF_VIDEODISCAUDIO";
	case UATF_DVDAUDIO:	return "UATF_DVDAUDIO";
	case UATF_TVTUNERAUDIO:	return "UATF_TVTUNERAUDIO";
	case UATF_SATELLITE:	return "UATF_SATELLITE";
	case UATF_CABLETUNER:	return "UATF_CABLETUNER";
	case UATF_DSS:	return "UATF_DSS";
	case UATF_RADIORECV:	return "UATF_RADIORECV";
	case UATF_RADIOXMIT:	return "UATF_RADIOXMIT";
	case UATF_MULTITRACK:	return "UATF_MULTITRACK";
	case UATF_SYNTHESIZER:	return "UATF_SYNTHESIZER";
	default:
		snprintf(tbuf, sizeof(tbuf), "unknown type (0x%.4x)", terminal_type);
		return tbuf;
	}
}
#endif

Static int
uaudio_determine_class(const struct io_terminal *iot, struct mixerctl *mix)
{
	int terminal_type;

	if (iot == NULL || iot->output == NULL) {
		mix->class = UAC_OUTPUT;
		return 0;
	}
	terminal_type = 0;
	if (iot->output->size == 1)
		terminal_type = iot->output->terminals[0];
	/*
	 * If the only output terminal is USB,
	 * the class is UAC_RECORD.
	 */
	if ((terminal_type & 0xff00) == (UAT_UNDEFINED & 0xff00)) {
		mix->class = UAC_RECORD;
		if (iot->inputs_size == 1
		    && iot->inputs[0] != NULL
		    && iot->inputs[0]->size == 1)
			return iot->inputs[0]->terminals[0];
		else
			return 0;
	}
	/*
	 * If the ultimate destination of the unit is just one output
	 * terminal and the unit is connected to the output terminal
	 * directly, the class is UAC_OUTPUT.
	 */
	if (terminal_type != 0 && iot->direct) {
		mix->class = UAC_OUTPUT;
		return terminal_type;
	}
	/*
	 * If the unit is connected to just one input terminal,
	 * the class is UAC_INPUT.
	 */
	if (iot->inputs_size == 1 && iot->inputs[0] != NULL
	    && iot->inputs[0]->size == 1) {
		mix->class = UAC_INPUT;
		return iot->inputs[0]->terminals[0];
	}
	/*
	 * Otherwise, the class is UAC_OUTPUT.
	 */
	mix->class = UAC_OUTPUT;
	return terminal_type;
}

Static const char *
uaudio_feature_name(const struct io_terminal *iot, struct mixerctl *mix)
{
	int terminal_type;

	terminal_type = uaudio_determine_class(iot, mix);
	if (mix->class == UAC_RECORD && terminal_type == 0)
		return AudioNmixerout;
	DPRINTF("terminal_type=%s\n", uaudio_get_terminal_name(terminal_type));
	switch (terminal_type) {
	case UAT_STREAM:
		return AudioNdac;

	case UATI_MICROPHONE:
	case UATI_DESKMICROPHONE:
	case UATI_PERSONALMICROPHONE:
	case UATI_OMNIMICROPHONE:
	case UATI_MICROPHONEARRAY:
	case UATI_PROCMICROPHONEARR:
		return AudioNmicrophone;

	case UATO_SPEAKER:
	case UATO_DESKTOPSPEAKER:
	case UATO_ROOMSPEAKER:
	case UATO_COMMSPEAKER:
		return AudioNspeaker;

	case UATO_HEADPHONES:
		return AudioNheadphone;

	case UATO_SUBWOOFER:
		return AudioNlfe;

	/* telephony terminal types */
	case UATT_UNDEFINED:
	case UATT_PHONELINE:
	case UATT_TELEPHONE:
	case UATT_DOWNLINEPHONE:
		return "phone";

	case UATE_ANALOGCONN:
	case UATE_LINECONN:
	case UATE_LEGACYCONN:
		return AudioNline;

	case UATE_DIGITALAUIFC:
	case UATE_SPDIF:
	case UATE_1394DA:
	case UATE_1394DV:
		return AudioNaux;

	case UATF_CDPLAYER:
		return AudioNcd;

	case UATF_SYNTHESIZER:
		return AudioNfmsynth;

	case UATF_VIDEODISCAUDIO:
	case UATF_DVDAUDIO:
	case UATF_TVTUNERAUDIO:
		return AudioNvideo;

	case UAT_UNDEFINED:
	case UAT_VENDOR:
	case UATI_UNDEFINED:
/* output terminal types */
	case UATO_UNDEFINED:
	case UATO_DISPLAYAUDIO:
/* bidir terminal types */
	case UATB_UNDEFINED:
	case UATB_HANDSET:
	case UATB_HEADSET:
	case UATB_SPEAKERPHONE:
	case UATB_SPEAKERPHONEESUP:
	case UATB_SPEAKERPHONEECANC:
/* external terminal types */
	case UATE_UNDEFINED:
/* embedded function terminal types */
	case UATF_UNDEFINED:
	case UATF_CALIBNOISE:
	case UATF_EQUNOISE:
	case UATF_DAT:
	case UATF_DCC:
	case UATF_MINIDISK:
	case UATF_ANALOGTAPE:
	case UATF_PHONOGRAPH:
	case UATF_VCRAUDIO:
	case UATF_SATELLITE:
	case UATF_CABLETUNER:
	case UATF_DSS:
	case UATF_RADIORECV:
	case UATF_RADIOXMIT:
	case UATF_MULTITRACK:
	case 0xffff:
	default:
		DPRINTF("'master' for 0x%.4x\n", terminal_type);
		return AudioNmaster;
	}
	return AudioNmaster;
}

Static void
uaudio_add_feature(struct uaudio_softc *sc, const struct io_terminal *iot, int id)
{
	const struct usb_audio_feature_unit *d;
	const uByte *ctls;
	int ctlsize;
	int nchan;
	u_int fumask, mmask, cmask;
	struct mixerctl mix;
	int chan, ctl, i, unit;
	const char *mixername;

#define GET(i) (ctls[(i)*ctlsize] | \
		(ctlsize > 1 ? ctls[(i)*ctlsize+1] << 8 : 0))
	d = iot[id].d.fu;
	ctls = d->bmaControls;
	ctlsize = d->bControlSize;
	if (ctlsize == 0) {
		DPRINTF("ignoring feature %d with controlSize of zero\n", id);
		return;
	}
	nchan = (d->bLength - 7) / ctlsize;
	mmask = GET(0);
	/* Figure out what we can control */
	for (cmask = 0, chan = 1; chan < nchan; chan++) {
		DPRINTFN(9,"chan=%d mask=%x\n",
			    chan, GET(chan));
		cmask |= GET(chan);
	}

	DPRINTFN(1,"bUnitId=%d, "
		    "%d channels, mmask=0x%04x, cmask=0x%04x\n",
		    d->bUnitId, nchan, mmask, cmask);

	if (nchan > MIX_MAX_CHAN)
		nchan = MIX_MAX_CHAN;
	unit = d->bUnitId;
	mix.wIndex = MAKE(unit, sc->sc_ac_iface);
	for (ctl = MUTE_CONTROL; ctl < LOUDNESS_CONTROL; ctl++) {
		fumask = FU_MASK(ctl);
		DPRINTFN(4,"ctl=%d fumask=0x%04x\n",
			    ctl, fumask);
		if (mmask & fumask) {
			mix.nchan = 1;
			mix.wValue[0] = MAKE(ctl, 0);
		} else if (cmask & fumask) {
			mix.nchan = nchan - 1;
			for (i = 1; i < nchan; i++) {
				if (GET(i) & fumask)
					mix.wValue[i-1] = MAKE(ctl, i);
				else
					mix.wValue[i-1] = -1;
			}
		} else {
			continue;
		}
#undef GET
		mixername = uaudio_feature_name(&iot[id], &mix);
		switch (ctl) {
		case MUTE_CONTROL:
			mix.type = MIX_ON_OFF;
			mix.ctlunit = "";
			snprintf(mix.ctlname, sizeof(mix.ctlname),
				 "%s.%s", mixername, AudioNmute);
			break;
		case VOLUME_CONTROL:
			mix.type = MIX_SIGNED_16;
			mix.ctlunit = AudioNvolume;
			strlcpy(mix.ctlname, mixername, sizeof(mix.ctlname));
			break;
		case BASS_CONTROL:
			mix.type = MIX_SIGNED_8;
			mix.ctlunit = AudioNbass;
			snprintf(mix.ctlname, sizeof(mix.ctlname),
				 "%s.%s", mixername, AudioNbass);
			break;
		case MID_CONTROL:
			mix.type = MIX_SIGNED_8;
			mix.ctlunit = AudioNmid;
			snprintf(mix.ctlname, sizeof(mix.ctlname),
				 "%s.%s", mixername, AudioNmid);
			break;
		case TREBLE_CONTROL:
			mix.type = MIX_SIGNED_8;
			mix.ctlunit = AudioNtreble;
			snprintf(mix.ctlname, sizeof(mix.ctlname),
				 "%s.%s", mixername, AudioNtreble);
			break;
		case GRAPHIC_EQUALIZER_CONTROL:
			continue; /* XXX don't add anything */
			break;
		case AGC_CONTROL:
			mix.type = MIX_ON_OFF;
			mix.ctlunit = "";
			snprintf(mix.ctlname, sizeof(mix.ctlname), "%s.%s",
				 mixername, AudioNagc);
			break;
		case DELAY_CONTROL:
			mix.type = MIX_UNSIGNED_16;
			mix.ctlunit = "4 ms";
			snprintf(mix.ctlname, sizeof(mix.ctlname),
				 "%s.%s", mixername, AudioNdelay);
			break;
		case BASS_BOOST_CONTROL:
			mix.type = MIX_ON_OFF;
			mix.ctlunit = "";
			snprintf(mix.ctlname, sizeof(mix.ctlname),
				 "%s.%s", mixername, AudioNbassboost);
			break;
		case LOUDNESS_CONTROL:
			mix.type = MIX_ON_OFF;
			mix.ctlunit = "";
			snprintf(mix.ctlname, sizeof(mix.ctlname),
				 "%s.%s", mixername, AudioNloudness);
			break;
		}
		uaudio_mixer_add_ctl(sc, &mix);
	}
}

Static void
uaudio_add_processing_updown(struct uaudio_softc *sc,
			     const struct io_terminal *iot, int id)
{
	const struct usb_audio_processing_unit *d;
	const struct usb_audio_processing_unit_1 *d1;
	const struct usb_audio_processing_unit_updown *ud;
	struct mixerctl mix;
	int i;

	d = iot[id].d.pu;
	d1 = (const struct usb_audio_processing_unit_1 *)
	    &d->baSourceId[d->bNrInPins];
	ud = (const struct usb_audio_processing_unit_updown *)
	    &d1->bmControls[d1->bControlSize];
	DPRINTFN(2,"bUnitId=%d bNrModes=%d\n",
		    d->bUnitId, ud->bNrModes);

	if (!(d1->bmControls[0] & UA_PROC_MASK(UD_MODE_SELECT_CONTROL))) {
		DPRINTF("%s", "no mode select\n");
		return;
	}

	mix.wIndex = MAKE(d->bUnitId, sc->sc_ac_iface);
	mix.nchan = 1;
	mix.wValue[0] = MAKE(UD_MODE_SELECT_CONTROL, 0);
	uaudio_determine_class(&iot[id], &mix);
	mix.type = MIX_ON_OFF;	/* XXX */
	mix.ctlunit = "";
	snprintf(mix.ctlname, sizeof(mix.ctlname), "pro%d-mode", d->bUnitId);

	for (i = 0; i < ud->bNrModes; i++) {
		DPRINTFN(2,"i=%d bm=0x%x\n",
			    i, UGETW(ud->waModes[i]));
		/* XXX */
	}
	uaudio_mixer_add_ctl(sc, &mix);
}

Static void
uaudio_add_processing(struct uaudio_softc *sc, const struct io_terminal *iot, int id)
{
	const struct usb_audio_processing_unit *d;
	const struct usb_audio_processing_unit_1 *d1;
	int ptype;
	struct mixerctl mix;

	d = iot[id].d.pu;
	d1 = (const struct usb_audio_processing_unit_1 *)
	    &d->baSourceId[d->bNrInPins];
	ptype = UGETW(d->wProcessType);
	DPRINTFN(2,"wProcessType=%d bUnitId=%d "
		    "bNrInPins=%d\n", ptype, d->bUnitId, d->bNrInPins);

	if (d1->bmControls[0] & UA_PROC_ENABLE_MASK) {
		mix.wIndex = MAKE(d->bUnitId, sc->sc_ac_iface);
		mix.nchan = 1;
		mix.wValue[0] = MAKE(XX_ENABLE_CONTROL, 0);
		uaudio_determine_class(&iot[id], &mix);
		mix.type = MIX_ON_OFF;
		mix.ctlunit = "";
		snprintf(mix.ctlname, sizeof(mix.ctlname), "pro%d.%d-enable",
		    d->bUnitId, ptype);
		uaudio_mixer_add_ctl(sc, &mix);
	}

	switch(ptype) {
	case UPDOWNMIX_PROCESS:
		uaudio_add_processing_updown(sc, iot, id);
		break;
	case DOLBY_PROLOGIC_PROCESS:
	case P3D_STEREO_EXTENDER_PROCESS:
	case REVERBATION_PROCESS:
	case CHORUS_PROCESS:
	case DYN_RANGE_COMP_PROCESS:
	default:
#ifdef UAUDIO_DEBUG
		aprint_debug(
		    "uaudio_add_processing: unit %d, type=%d not impl.\n",
		    d->bUnitId, ptype);
#endif
		break;
	}
}

Static void
uaudio_add_extension(struct uaudio_softc *sc, const struct io_terminal *iot, int id)
{
	const struct usb_audio_extension_unit *d;
	const struct usb_audio_extension_unit_1 *d1;
	struct mixerctl mix;

	d = iot[id].d.eu;
	d1 = (const struct usb_audio_extension_unit_1 *)
	    &d->baSourceId[d->bNrInPins];
	DPRINTFN(2,"bUnitId=%d bNrInPins=%d\n",
		    d->bUnitId, d->bNrInPins);

	if (usbd_get_quirks(sc->sc_udev)->uq_flags & UQ_AU_NO_XU)
		return;

	if (d1->bmControls[0] & UA_EXT_ENABLE_MASK) {
		mix.wIndex = MAKE(d->bUnitId, sc->sc_ac_iface);
		mix.nchan = 1;
		mix.wValue[0] = MAKE(UA_EXT_ENABLE, 0);
		uaudio_determine_class(&iot[id], &mix);
		mix.type = MIX_ON_OFF;
		mix.ctlunit = "";
		snprintf(mix.ctlname, sizeof(mix.ctlname), "ext%d-enable",
		    d->bUnitId);
		uaudio_mixer_add_ctl(sc, &mix);
	}
}

Static struct terminal_list*
uaudio_merge_terminal_list(const struct io_terminal *iot)
{
	struct terminal_list *tml;
	uint16_t *ptm;
	int i, len;

	len = 0;
	if (iot->inputs == NULL)
		return NULL;
	for (i = 0; i < iot->inputs_size; i++) {
		if (iot->inputs[i] != NULL)
			len += iot->inputs[i]->size;
	}
	tml = malloc(TERMINAL_LIST_SIZE(len), M_TEMP, M_NOWAIT);
	if (tml == NULL) {
		aprint_error("uaudio_merge_terminal_list: no memory\n");
		return NULL;
	}
	tml->size = 0;
	ptm = tml->terminals;
	for (i = 0; i < iot->inputs_size; i++) {
		if (iot->inputs[i] == NULL)
			continue;
		if (iot->inputs[i]->size > len)
			break;
		memcpy(ptm, iot->inputs[i]->terminals,
		       iot->inputs[i]->size * sizeof(uint16_t));
		tml->size += iot->inputs[i]->size;
		ptm += iot->inputs[i]->size;
		len -= iot->inputs[i]->size;
	}
	return tml;
}

Static struct terminal_list *
uaudio_io_terminaltype(int outtype, struct io_terminal *iot, int id)
{
	struct terminal_list *tml;
	struct io_terminal *it;
	int src_id, i;

	it = &iot[id];
	if (it->output != NULL) {
		/* already has outtype? */
		for (i = 0; i < it->output->size; i++)
			if (it->output->terminals[i] == outtype)
				return uaudio_merge_terminal_list(it);
		tml = malloc(TERMINAL_LIST_SIZE(it->output->size + 1),
			     M_TEMP, M_NOWAIT);
		if (tml == NULL) {
			aprint_error("uaudio_io_terminaltype: no memory\n");
			return uaudio_merge_terminal_list(it);
		}
		memcpy(tml, it->output, TERMINAL_LIST_SIZE(it->output->size));
		tml->terminals[it->output->size] = outtype;
		tml->size++;
		free(it->output, M_TEMP);
		it->output = tml;
		if (it->inputs != NULL) {
			for (i = 0; i < it->inputs_size; i++)
				if (it->inputs[i] != NULL)
					free(it->inputs[i], M_TEMP);
			free(it->inputs, M_TEMP);
		}
		it->inputs_size = 0;
		it->inputs = NULL;
	} else {		/* end `iot[id] != NULL' */
		it->inputs_size = 0;
		it->inputs = NULL;
		it->output = malloc(TERMINAL_LIST_SIZE(1), M_TEMP, M_NOWAIT);
		if (it->output == NULL) {
			aprint_error("uaudio_io_terminaltype: no memory\n");
			return NULL;
		}
		it->output->terminals[0] = outtype;
		it->output->size = 1;
		it->direct = FALSE;
	}

	switch (it->d.desc->bDescriptorSubtype) {
	case UDESCSUB_AC_INPUT:
		it->inputs = malloc(sizeof(struct terminal_list *), M_TEMP, M_NOWAIT);
		if (it->inputs == NULL) {
			aprint_error("uaudio_io_terminaltype: no memory\n");
			return NULL;
		}
		tml = malloc(TERMINAL_LIST_SIZE(1), M_TEMP, M_NOWAIT);
		if (tml == NULL) {
			aprint_error("uaudio_io_terminaltype: no memory\n");
			free(it->inputs, M_TEMP);
			it->inputs = NULL;
			return NULL;
		}
		it->inputs[0] = tml;
		tml->terminals[0] = UGETW(it->d.it->wTerminalType);
		tml->size = 1;
		it->inputs_size = 1;
		return uaudio_merge_terminal_list(it);
	case UDESCSUB_AC_FEATURE:
		src_id = it->d.fu->bSourceId;
		it->inputs = malloc(sizeof(struct terminal_list *), M_TEMP, M_NOWAIT);
		if (it->inputs == NULL) {
			aprint_error("uaudio_io_terminaltype: no memory\n");
			return uaudio_io_terminaltype(outtype, iot, src_id);
		}
		it->inputs[0] = uaudio_io_terminaltype(outtype, iot, src_id);
		it->inputs_size = 1;
		return uaudio_merge_terminal_list(it);
	case UDESCSUB_AC_OUTPUT:
		it->inputs = malloc(sizeof(struct terminal_list *), M_TEMP, M_NOWAIT);
		if (it->inputs == NULL) {
			aprint_error("uaudio_io_terminaltype: no memory\n");
			return NULL;
		}
		src_id = it->d.ot->bSourceId;
		it->inputs[0] = uaudio_io_terminaltype(outtype, iot, src_id);
		it->inputs_size = 1;
		iot[src_id].direct = TRUE;
		return NULL;
	case UDESCSUB_AC_MIXER:
		it->inputs_size = 0;
		it->inputs = malloc(sizeof(struct terminal_list *)
				    * it->d.mu->bNrInPins, M_TEMP, M_NOWAIT);
		if (it->inputs == NULL) {
			aprint_error("uaudio_io_terminaltype: no memory\n");
			return NULL;
		}
		for (i = 0; i < it->d.mu->bNrInPins; i++) {
			src_id = it->d.mu->baSourceId[i];
			it->inputs[i] = uaudio_io_terminaltype(outtype, iot,
							       src_id);
			it->inputs_size++;
		}
		return uaudio_merge_terminal_list(it);
	case UDESCSUB_AC_SELECTOR:
		it->inputs_size = 0;
		it->inputs = malloc(sizeof(struct terminal_list *)
				    * it->d.su->bNrInPins, M_TEMP, M_NOWAIT);
		if (it->inputs == NULL) {
			aprint_error("uaudio_io_terminaltype: no memory\n");
			return NULL;
		}
		for (i = 0; i < it->d.su->bNrInPins; i++) {
			src_id = it->d.su->baSourceId[i];
			it->inputs[i] = uaudio_io_terminaltype(outtype, iot,
							       src_id);
			it->inputs_size++;
		}
		return uaudio_merge_terminal_list(it);
	case UDESCSUB_AC_PROCESSING:
		it->inputs_size = 0;
		it->inputs = malloc(sizeof(struct terminal_list *)
				    * it->d.pu->bNrInPins, M_TEMP, M_NOWAIT);
		if (it->inputs == NULL) {
			aprint_error("uaudio_io_terminaltype: no memory\n");
			return NULL;
		}
		for (i = 0; i < it->d.pu->bNrInPins; i++) {
			src_id = it->d.pu->baSourceId[i];
			it->inputs[i] = uaudio_io_terminaltype(outtype, iot,
							       src_id);
			it->inputs_size++;
		}
		return uaudio_merge_terminal_list(it);
	case UDESCSUB_AC_EXTENSION:
		it->inputs_size = 0;
		it->inputs = malloc(sizeof(struct terminal_list *)
				    * it->d.eu->bNrInPins, M_TEMP, M_NOWAIT);
		if (it->inputs == NULL) {
			aprint_error("uaudio_io_terminaltype: no memory\n");
			return NULL;
		}
		for (i = 0; i < it->d.eu->bNrInPins; i++) {
			src_id = it->d.eu->baSourceId[i];
			it->inputs[i] = uaudio_io_terminaltype(outtype, iot,
							       src_id);
			it->inputs_size++;
		}
		return uaudio_merge_terminal_list(it);
	case UDESCSUB_AC_HEADER:
	default:
		return NULL;
	}
}

Static usbd_status
uaudio_identify(struct uaudio_softc *sc, const usb_config_descriptor_t *cdesc)
{
	usbd_status err;

	err = uaudio_identify_ac(sc, cdesc);
	if (err)
		return err;
	return uaudio_identify_as(sc, cdesc);
}

Static void
uaudio_add_alt(struct uaudio_softc *sc, const struct as_info *ai)
{
	size_t len;
	struct as_info *nai;

	len = sizeof(*ai) * (sc->sc_nalts + 1);
	nai = malloc(len, M_USBDEV, M_NOWAIT);
	if (nai == NULL) {
		aprint_error("uaudio_add_alt: no memory\n");
		return;
	}
	/* Copy old data, if there was any */
	if (sc->sc_nalts != 0) {
		memcpy(nai, sc->sc_alts, sizeof(*ai) * (sc->sc_nalts));
		free(sc->sc_alts, M_USBDEV);
	}
	sc->sc_alts = nai;
	DPRINTFN(2,"adding alt=%d, enc=%d\n",
		    ai->alt, ai->encoding);
	sc->sc_alts[sc->sc_nalts++] = *ai;
}

Static usbd_status
uaudio_process_as(struct uaudio_softc *sc, const char *tbuf, int *offsp,
		  int size, const usb_interface_descriptor_t *id)
#define offs (*offsp)
{
	const struct usb_audio_streaming_interface_descriptor *asid;
	const struct usb_audio_streaming_type1_descriptor *asf1d;
	const usb_endpoint_descriptor_audio_t *ed;
	const usb_endpoint_descriptor_audio_t *epdesc1;
	const struct usb_audio_streaming_endpoint_descriptor *sed;
	int format, chan __unused, prec, enc;
	int dir, type, sync;
	struct as_info ai;
	const char *format_str __unused;

	asid = (const void *)(tbuf + offs);
	if (asid->bDescriptorType != UDESC_CS_INTERFACE ||
	    asid->bDescriptorSubtype != AS_GENERAL)
		return USBD_INVAL;
	DPRINTF("asid: bTerminakLink=%d wFormatTag=%d\n",
		 asid->bTerminalLink, UGETW(asid->wFormatTag));
	offs += asid->bLength;
	if (offs > size)
		return USBD_INVAL;

	asf1d = (const void *)(tbuf + offs);
	if (asf1d->bDescriptorType != UDESC_CS_INTERFACE ||
	    asf1d->bDescriptorSubtype != FORMAT_TYPE)
		return USBD_INVAL;
	offs += asf1d->bLength;
	if (offs > size)
		return USBD_INVAL;

	if (asf1d->bFormatType != FORMAT_TYPE_I) {
		aprint_error_dev(sc->sc_dev,
		    "ignored setting with type %d format\n", UGETW(asid->wFormatTag));
		return USBD_NORMAL_COMPLETION;
	}

	ed = (const void *)(tbuf + offs);
	if (ed->bDescriptorType != UDESC_ENDPOINT)
		return USBD_INVAL;
	DPRINTF("endpoint[0] bLength=%d bDescriptorType=%d "
		 "bEndpointAddress=%d bmAttributes=0x%x wMaxPacketSize=%d "
		 "bInterval=%d bRefresh=%d bSynchAddress=%d\n",
		 ed->bLength, ed->bDescriptorType, ed->bEndpointAddress,
		 ed->bmAttributes, UGETW(ed->wMaxPacketSize),
		 ed->bInterval, ed->bRefresh, ed->bSynchAddress);
	offs += ed->bLength;
	if (offs > size)
		return USBD_INVAL;
	if (UE_GET_XFERTYPE(ed->bmAttributes) != UE_ISOCHRONOUS)
		return USBD_INVAL;

	dir = UE_GET_DIR(ed->bEndpointAddress);
	type = UE_GET_ISO_TYPE(ed->bmAttributes);
	if ((usbd_get_quirks(sc->sc_udev)->uq_flags & UQ_AU_INP_ASYNC) &&
	    dir == UE_DIR_IN && type == UE_ISO_ADAPT)
		type = UE_ISO_ASYNC;

	/* We can't handle endpoints that need a sync pipe yet. */
	sync = FALSE;
	if (dir == UE_DIR_IN && type == UE_ISO_ADAPT) {
		sync = TRUE;
#ifndef UAUDIO_MULTIPLE_ENDPOINTS
		aprint_error_dev(sc->sc_dev,
		    "ignored input endpoint of type adaptive\n");
		return USBD_NORMAL_COMPLETION;
#endif
	}
	if (dir != UE_DIR_IN && type == UE_ISO_ASYNC) {
		sync = TRUE;
#ifndef UAUDIO_MULTIPLE_ENDPOINTS
		aprint_error_dev(sc->sc_dev,
		    "ignored output endpoint of type async\n");
		return USBD_NORMAL_COMPLETION;
#endif
	}

	sed = (const void *)(tbuf + offs);
	if (sed->bDescriptorType != UDESC_CS_ENDPOINT ||
	    sed->bDescriptorSubtype != AS_GENERAL)
		return USBD_INVAL;
	DPRINTF(" streadming_endpoint: offset=%d bLength=%d\n", offs, sed->bLength);
	offs += sed->bLength;
	if (offs > size)
		return USBD_INVAL;

#ifdef UAUDIO_MULTIPLE_ENDPOINTS
	if (sync && id->bNumEndpoints <= 1) {
		aprint_error_dev(sc->sc_dev,
		    "a sync-pipe endpoint but no other endpoint\n");
		return USBD_INVAL;
	}
#endif
	if (!sync && id->bNumEndpoints > 1) {
		aprint_error_dev(sc->sc_dev,
		    "non sync-pipe endpoint but multiple endpoints\n");
		return USBD_INVAL;
	}
	epdesc1 = NULL;
	if (id->bNumEndpoints > 1) {
		epdesc1 = (const void*)(tbuf + offs);
		if (epdesc1->bDescriptorType != UDESC_ENDPOINT)
			return USBD_INVAL;
		DPRINTF("endpoint[1] bLength=%d "
			 "bDescriptorType=%d bEndpointAddress=%d "
			 "bmAttributes=0x%x wMaxPacketSize=%d bInterval=%d "
			 "bRefresh=%d bSynchAddress=%d\n",
			 epdesc1->bLength, epdesc1->bDescriptorType,
			 epdesc1->bEndpointAddress, epdesc1->bmAttributes,
			 UGETW(epdesc1->wMaxPacketSize), epdesc1->bInterval,
			 epdesc1->bRefresh, epdesc1->bSynchAddress);
		offs += epdesc1->bLength;
		if (offs > size)
			return USBD_INVAL;
		if (epdesc1->bSynchAddress != 0) {
			aprint_error_dev(sc->sc_dev,
			    "invalid endpoint: bSynchAddress=0\n");
			return USBD_INVAL;
		}
		if (UE_GET_XFERTYPE(epdesc1->bmAttributes) != UE_ISOCHRONOUS) {
			aprint_error_dev(sc->sc_dev,
			    "invalid endpoint: bmAttributes=0x%x\n",
			     epdesc1->bmAttributes);
			return USBD_INVAL;
		}
		if (epdesc1->bEndpointAddress != ed->bSynchAddress) {
			aprint_error_dev(sc->sc_dev,
			    "invalid endpoint addresses: "
			    "ep[0]->bSynchAddress=0x%x "
			    "ep[1]->bEndpointAddress=0x%x\n",
			    ed->bSynchAddress, epdesc1->bEndpointAddress);
			return USBD_INVAL;
		}
		/* UE_GET_ADDR(epdesc1->bEndpointAddress), and epdesc1->bRefresh */
	}

	format = UGETW(asid->wFormatTag);
	chan = asf1d->bNrChannels;
	prec = asf1d->bBitResolution;
	if (prec != 8 && prec != 16 && prec != 24) {
		aprint_error_dev(sc->sc_dev,
		    "ignored setting with precision %d\n", prec);
		return USBD_NORMAL_COMPLETION;
	}
	switch (format) {
	case UA_FMT_PCM:
		if (prec == 8) {
			sc->sc_altflags |= HAS_8;
		} else if (prec == 16) {
			sc->sc_altflags |= HAS_16;
		} else if (prec == 24) {
			sc->sc_altflags |= HAS_24;
		}
		enc = AUDIO_ENCODING_SLINEAR_LE;
		format_str = "pcm";
		break;
	case UA_FMT_PCM8:
		enc = AUDIO_ENCODING_ULINEAR_LE;
		sc->sc_altflags |= HAS_8U;
		format_str = "pcm8";
		break;
	case UA_FMT_ALAW:
		enc = AUDIO_ENCODING_ALAW;
		sc->sc_altflags |= HAS_ALAW;
		format_str = "alaw";
		break;
	case UA_FMT_MULAW:
		enc = AUDIO_ENCODING_ULAW;
		sc->sc_altflags |= HAS_MULAW;
		format_str = "mulaw";
		break;
	case UA_FMT_IEEE_FLOAT:
	default:
		aprint_error_dev(sc->sc_dev,
		    "ignored setting with format %d\n", format);
		return USBD_NORMAL_COMPLETION;
	}
#ifdef UAUDIO_DEBUG
	aprint_debug_dev(sc->sc_dev, "%s: %dch, %d/%dbit, %s,",
	       dir == UE_DIR_IN ? "recording" : "playback",
	       chan, prec, asf1d->bSubFrameSize * 8, format_str);
	if (asf1d->bSamFreqType == UA_SAMP_CONTNUOUS) {
		aprint_debug(" %d-%dHz\n", UA_SAMP_LO(asf1d),
		    UA_SAMP_HI(asf1d));
	} else {
		int r;
		aprint_debug(" %d", UA_GETSAMP(asf1d, 0));
		for (r = 1; r < asf1d->bSamFreqType; r++)
			aprint_debug(",%d", UA_GETSAMP(asf1d, r));
		aprint_debug("Hz\n");
	}
#endif
	ai.alt = id->bAlternateSetting;
	ai.encoding = enc;
	ai.attributes = sed->bmAttributes;
	ai.idesc = id;
	ai.edesc = ed;
	ai.edesc1 = epdesc1;
	ai.asf1desc = asf1d;
	ai.sc_busy = 0;
	ai.aformat = NULL;
	ai.ifaceh = NULL;
	uaudio_add_alt(sc, &ai);
#ifdef UAUDIO_DEBUG
	if (ai.attributes & UA_SED_FREQ_CONTROL)
		DPRINTFN(1, "%s", "FREQ_CONTROL\n");
	if (ai.attributes & UA_SED_PITCH_CONTROL)
		DPRINTFN(1, "%s", "PITCH_CONTROL\n");
#endif
	sc->sc_mode |= (dir == UE_DIR_OUT) ? AUMODE_PLAY : AUMODE_RECORD;

	return USBD_NORMAL_COMPLETION;
}
#undef offs

Static usbd_status
uaudio_identify_as(struct uaudio_softc *sc,
		   const usb_config_descriptor_t *cdesc)
{
	const usb_interface_descriptor_t *id;
	const char *tbuf;
	struct audio_format *auf;
	const struct usb_audio_streaming_type1_descriptor *t1desc;
	int size, offs;
	int i, j;

	size = UGETW(cdesc->wTotalLength);
	tbuf = (const char *)cdesc;

	/* Locate the AudioStreaming interface descriptor. */
	offs = 0;
	id = uaudio_find_iface(tbuf, size, &offs, UISUBCLASS_AUDIOSTREAM);
	if (id == NULL)
		return USBD_INVAL;

	/* Loop through all the alternate settings. */
	while (offs <= size) {
		DPRINTFN(2, "interface=%d offset=%d\n",
		    id->bInterfaceNumber, offs);
		switch (id->bNumEndpoints) {
		case 0:
			DPRINTFN(2, "AS null alt=%d\n",
				     id->bAlternateSetting);
			sc->sc_nullalt = id->bAlternateSetting;
			break;
		case 1:
#ifdef UAUDIO_MULTIPLE_ENDPOINTS
		case 2:
#endif
			uaudio_process_as(sc, tbuf, &offs, size, id);
			break;
		default:
			aprint_error_dev(sc->sc_dev,
			    "ignored audio interface with %d endpoints\n",
			     id->bNumEndpoints);
			break;
		}
		id = uaudio_find_iface(tbuf, size, &offs,UISUBCLASS_AUDIOSTREAM);
		if (id == NULL)
			break;
	}
	if (offs > size)
		return USBD_INVAL;
	DPRINTF("%d alts available\n", sc->sc_nalts);

	if (sc->sc_mode == 0) {
		aprint_error_dev(sc->sc_dev, "no usable endpoint found\n");
		return USBD_INVAL;
	}

	/* build audio_format array */
	sc->sc_formats = malloc(sizeof(struct audio_format) * sc->sc_nalts,
				M_USBDEV, M_NOWAIT);
	if (sc->sc_formats == NULL)
		return USBD_NOMEM;
	sc->sc_nformats = sc->sc_nalts;
	for (i = 0; i < sc->sc_nalts; i++) {
		auf = &sc->sc_formats[i];
		t1desc = sc->sc_alts[i].asf1desc;
		auf->driver_data = NULL;
		if (UE_GET_DIR(sc->sc_alts[i].edesc->bEndpointAddress) == UE_DIR_OUT)
			auf->mode = AUMODE_PLAY;
		else
			auf->mode = AUMODE_RECORD;
		auf->encoding = sc->sc_alts[i].encoding;
		auf->validbits = t1desc->bBitResolution;
		auf->precision = t1desc->bSubFrameSize * 8;
		auf->channels = t1desc->bNrChannels;
		auf->channel_mask = sc->sc_channel_config;
		auf->frequency_type = t1desc->bSamFreqType;
		if (t1desc->bSamFreqType == UA_SAMP_CONTNUOUS) {
			auf->frequency[0] = UA_SAMP_LO(t1desc);
			auf->frequency[1] = UA_SAMP_HI(t1desc);
		} else {
			for (j = 0; j  < t1desc->bSamFreqType; j++) {
				if (j >= AUFMT_MAX_FREQUENCIES) {
					aprint_error("%s: please increase "
					       "AUFMT_MAX_FREQUENCIES to %d\n",
					       __func__, t1desc->bSamFreqType);
					auf->frequency_type =
					    AUFMT_MAX_FREQUENCIES;
					break;
				}
				auf->frequency[j] = UA_GETSAMP(t1desc, j);
			}
		}
		sc->sc_alts[i].aformat = auf;
	}

	if (0 != auconv_create_encodings(sc->sc_formats, sc->sc_nformats,
					 &sc->sc_encodings)) {
		free(sc->sc_formats, M_DEVBUF);
		sc->sc_formats = NULL;
		return ENOMEM;
	}

	return USBD_NORMAL_COMPLETION;
}

#ifdef UAUDIO_DEBUG
Static void
uaudio_dump_tml(struct terminal_list *tml) {
	if (tml == NULL) {
		printf("NULL");
	} else {
                int i;
		for (i = 0; i < tml->size; i++)
			printf("%s ", uaudio_get_terminal_name
			       (tml->terminals[i]));
	}
	printf("\n");
}
#endif

Static usbd_status
uaudio_identify_ac(struct uaudio_softc *sc, const usb_config_descriptor_t *cdesc)
{
	struct io_terminal* iot;
	const usb_interface_descriptor_t *id;
	const struct usb_audio_control_descriptor *acdp;
	const uaudio_cs_descriptor_t *dp;
	const struct usb_audio_output_terminal *pot;
	struct terminal_list *tml;
	const char *tbuf, *ibuf, *ibufend;
	int size, offs, ndps, i, j;

	size = UGETW(cdesc->wTotalLength);
	tbuf = (const char *)cdesc;

	/* Locate the AudioControl interface descriptor. */
	offs = 0;
	id = uaudio_find_iface(tbuf, size, &offs, UISUBCLASS_AUDIOCONTROL);
	if (id == NULL)
		return USBD_INVAL;
	if (offs + sizeof *acdp > size)
		return USBD_INVAL;
	sc->sc_ac_iface = id->bInterfaceNumber;
	DPRINTFN(2,"AC interface is %d\n", sc->sc_ac_iface);

	/* A class-specific AC interface header should follow. */
	ibuf = tbuf + offs;
	ibufend = tbuf + size;
	acdp = (const struct usb_audio_control_descriptor *)ibuf;
	if (acdp->bDescriptorType != UDESC_CS_INTERFACE ||
	    acdp->bDescriptorSubtype != UDESCSUB_AC_HEADER)
		return USBD_INVAL;

	if (!(usbd_get_quirks(sc->sc_udev)->uq_flags & UQ_BAD_ADC) &&
	     UGETW(acdp->bcdADC) != UAUDIO_VERSION)
		return USBD_INVAL;

	sc->sc_audio_rev = UGETW(acdp->bcdADC);
	DPRINTFN(2, "found AC header, vers=%03x\n", sc->sc_audio_rev);

	sc->sc_nullalt = -1;

	/* Scan through all the AC specific descriptors */
	dp = (const uaudio_cs_descriptor_t *)ibuf;
	ndps = 0;
	iot = malloc(sizeof(struct io_terminal) * 256, M_TEMP, M_NOWAIT | M_ZERO);
	if (iot == NULL) {
		aprint_error("%s: no memory\n", __func__);
		return USBD_NOMEM;
	}
	for (;;) {
		ibuf += dp->bLength;
		if (ibuf >= ibufend)
			break;
		dp = (const uaudio_cs_descriptor_t *)ibuf;
		if (ibuf + dp->bLength > ibufend) {
			free(iot, M_TEMP);
			return USBD_INVAL;
		}
		if (dp->bDescriptorType != UDESC_CS_INTERFACE)
			break;
		i = ((const struct usb_audio_input_terminal *)dp)->bTerminalId;
		iot[i].d.desc = dp;
		if (i > ndps)
			ndps = i;
	}
	ndps++;

	/* construct io_terminal */
	for (i = 0; i < ndps; i++) {
		dp = iot[i].d.desc;
		if (dp == NULL)
			continue;
		if (dp->bDescriptorSubtype != UDESCSUB_AC_OUTPUT)
			continue;
		pot = iot[i].d.ot;
		tml = uaudio_io_terminaltype(UGETW(pot->wTerminalType), iot, i);
		if (tml != NULL)
			free(tml, M_TEMP);
	}

#ifdef UAUDIO_DEBUG
	for (i = 0; i < 256; i++) {
		struct usb_audio_cluster cluster;

		if (iot[i].d.desc == NULL)
			continue;
		printf("id %d:\t", i);
		switch (iot[i].d.desc->bDescriptorSubtype) {
		case UDESCSUB_AC_INPUT:
			printf("AC_INPUT type=%s\n", uaudio_get_terminal_name
				  (UGETW(iot[i].d.it->wTerminalType)));
			printf("\t");
			cluster = uaudio_get_cluster(i, iot);
			uaudio_dump_cluster(&cluster);
			printf("\n");
			break;
		case UDESCSUB_AC_OUTPUT:
			printf("AC_OUTPUT type=%s ", uaudio_get_terminal_name
				  (UGETW(iot[i].d.ot->wTerminalType)));
			printf("src=%d\n", iot[i].d.ot->bSourceId);
			break;
		case UDESCSUB_AC_MIXER:
			printf("AC_MIXER src=");
			for (j = 0; j < iot[i].d.mu->bNrInPins; j++)
				printf("%d ", iot[i].d.mu->baSourceId[j]);
			printf("\n\t");
			cluster = uaudio_get_cluster(i, iot);
			uaudio_dump_cluster(&cluster);
			printf("\n");
			break;
		case UDESCSUB_AC_SELECTOR:
			printf("AC_SELECTOR src=");
			for (j = 0; j < iot[i].d.su->bNrInPins; j++)
				printf("%d ", iot[i].d.su->baSourceId[j]);
			printf("\n");
			break;
		case UDESCSUB_AC_FEATURE:
			printf("AC_FEATURE src=%d\n", iot[i].d.fu->bSourceId);
			break;
		case UDESCSUB_AC_PROCESSING:
			printf("AC_PROCESSING src=");
			for (j = 0; j < iot[i].d.pu->bNrInPins; j++)
				printf("%d ", iot[i].d.pu->baSourceId[j]);
			printf("\n\t");
			cluster = uaudio_get_cluster(i, iot);
			uaudio_dump_cluster(&cluster);
			printf("\n");
			break;
		case UDESCSUB_AC_EXTENSION:
			printf("AC_EXTENSION src=");
			for (j = 0; j < iot[i].d.eu->bNrInPins; j++)
				printf("%d ", iot[i].d.eu->baSourceId[j]);
			printf("\n\t");
			cluster = uaudio_get_cluster(i, iot);
			uaudio_dump_cluster(&cluster);
			printf("\n");
			break;
		default:
			printf("unknown audio control (subtype=%d)\n",
				  iot[i].d.desc->bDescriptorSubtype);
		}
		for (j = 0; j < iot[i].inputs_size; j++) {
			printf("\tinput%d: ", j);
			uaudio_dump_tml(iot[i].inputs[j]);
		}
		printf("\toutput: ");
		uaudio_dump_tml(iot[i].output);
	}
#endif

	for (i = 0; i < ndps; i++) {
		dp = iot[i].d.desc;
		if (dp == NULL)
			continue;
		DPRINTF("id=%d subtype=%d\n", i, dp->bDescriptorSubtype);
		switch (dp->bDescriptorSubtype) {
		case UDESCSUB_AC_HEADER:
			aprint_error("uaudio_identify_ac: unexpected AC header\n");
			break;
		case UDESCSUB_AC_INPUT:
			uaudio_add_input(sc, iot, i);
			break;
		case UDESCSUB_AC_OUTPUT:
			uaudio_add_output(sc, iot, i);
			break;
		case UDESCSUB_AC_MIXER:
			uaudio_add_mixer(sc, iot, i);
			break;
		case UDESCSUB_AC_SELECTOR:
			uaudio_add_selector(sc, iot, i);
			break;
		case UDESCSUB_AC_FEATURE:
			uaudio_add_feature(sc, iot, i);
			break;
		case UDESCSUB_AC_PROCESSING:
			uaudio_add_processing(sc, iot, i);
			break;
		case UDESCSUB_AC_EXTENSION:
			uaudio_add_extension(sc, iot, i);
			break;
		default:
			aprint_error(
			    "uaudio_identify_ac: bad AC desc subtype=0x%02x\n",
			    dp->bDescriptorSubtype);
			break;
		}
	}

	/* delete io_terminal */
	for (i = 0; i < 256; i++) {
		if (iot[i].d.desc == NULL)
			continue;
		if (iot[i].inputs != NULL) {
			for (j = 0; j < iot[i].inputs_size; j++) {
				if (iot[i].inputs[j] != NULL)
					free(iot[i].inputs[j], M_TEMP);
			}
			free(iot[i].inputs, M_TEMP);
		}
		if (iot[i].output != NULL)
			free(iot[i].output, M_TEMP);
		iot[i].d.desc = NULL;
	}
	free(iot, M_TEMP);

	return USBD_NORMAL_COMPLETION;
}

Static int
uaudio_query_devinfo(void *addr, mixer_devinfo_t *mi)
{
	struct uaudio_softc *sc;
	struct mixerctl *mc;
	int n, nctls, i;

	DPRINTFN(7, "index=%d\n", mi->index);
	sc = addr;
	if (sc->sc_dying)
		return EIO;

	n = mi->index;
	nctls = sc->sc_nctls;

	switch (n) {
	case UAC_OUTPUT:
		mi->type = AUDIO_MIXER_CLASS;
		mi->mixer_class = UAC_OUTPUT;
		mi->next = mi->prev = AUDIO_MIXER_LAST;
		strlcpy(mi->label.name, AudioCoutputs, sizeof(mi->label.name));
		return 0;
	case UAC_INPUT:
		mi->type = AUDIO_MIXER_CLASS;
		mi->mixer_class = UAC_INPUT;
		mi->next = mi->prev = AUDIO_MIXER_LAST;
		strlcpy(mi->label.name, AudioCinputs, sizeof(mi->label.name));
		return 0;
	case UAC_EQUAL:
		mi->type = AUDIO_MIXER_CLASS;
		mi->mixer_class = UAC_EQUAL;
		mi->next = mi->prev = AUDIO_MIXER_LAST;
		strlcpy(mi->label.name, AudioCequalization,
		    sizeof(mi->label.name));
		return 0;
	case UAC_RECORD:
		mi->type = AUDIO_MIXER_CLASS;
		mi->mixer_class = UAC_RECORD;
		mi->next = mi->prev = AUDIO_MIXER_LAST;
		strlcpy(mi->label.name, AudioCrecord, sizeof(mi->label.name));
		return 0;
	default:
		break;
	}

	n -= UAC_NCLASSES;
	if (n < 0 || n >= nctls)
		return ENXIO;

	mc = &sc->sc_ctls[n];
	strlcpy(mi->label.name, mc->ctlname, sizeof(mi->label.name));
	mi->mixer_class = mc->class;
	mi->next = mi->prev = AUDIO_MIXER_LAST;	/* XXX */
	switch (mc->type) {
	case MIX_ON_OFF:
		mi->type = AUDIO_MIXER_ENUM;
		mi->un.e.num_mem = 2;
		strlcpy(mi->un.e.member[0].label.name, AudioNoff,
		    sizeof(mi->un.e.member[0].label.name));
		mi->un.e.member[0].ord = 0;
		strlcpy(mi->un.e.member[1].label.name, AudioNon,
		    sizeof(mi->un.e.member[1].label.name));
		mi->un.e.member[1].ord = 1;
		break;
	case MIX_SELECTOR:
		mi->type = AUDIO_MIXER_ENUM;
		mi->un.e.num_mem = mc->maxval - mc->minval + 1;
		for (i = 0; i <= mc->maxval - mc->minval; i++) {
			snprintf(mi->un.e.member[i].label.name,
				 sizeof(mi->un.e.member[i].label.name),
				 "%d", i + mc->minval);
			mi->un.e.member[i].ord = i + mc->minval;
		}
		break;
	default:
		mi->type = AUDIO_MIXER_VALUE;
		strncpy(mi->un.v.units.name, mc->ctlunit, MAX_AUDIO_DEV_LEN);
		mi->un.v.num_channels = mc->nchan;
		mi->un.v.delta = mc->delta;
		break;
	}
	return 0;
}

Static int
uaudio_open(void *addr, int flags)
{
	struct uaudio_softc *sc;

	sc = addr;
	DPRINTF("sc=%p\n", sc);
	if (sc->sc_dying)
		return EIO;

	if ((flags & FWRITE) && !(sc->sc_mode & AUMODE_PLAY))
		return EACCES;
	if ((flags & FREAD) && !(sc->sc_mode & AUMODE_RECORD))
		return EACCES;

	return 0;
}

/*
 * Close function is called at splaudio().
 */
Static void
uaudio_close(void *addr)
{
}

Static int
uaudio_drain(void *addr)
{
	struct uaudio_softc *sc = addr;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

	kpause("uaudiodr", false,
	    mstohz(UAUDIO_NCHANBUFS * UAUDIO_NFRAMES), &sc->sc_intr_lock);

	return 0;
}

Static int
uaudio_halt_out_dma(void *addr)
{
	struct uaudio_softc *sc = addr;

	DPRINTF("%s", "enter\n");

	mutex_spin_exit(&sc->sc_intr_lock);
	if (sc->sc_playchan.pipe != NULL) {
		uaudio_chan_close(sc, &sc->sc_playchan);
		uaudio_chan_free_buffers(sc, &sc->sc_playchan);
		sc->sc_playchan.intr = NULL;
	}
	mutex_spin_enter(&sc->sc_intr_lock);

	return 0;
}

Static int
uaudio_halt_in_dma(void *addr)
{
	struct uaudio_softc *sc = addr;

	DPRINTF("%s", "enter\n");

	mutex_spin_exit(&sc->sc_intr_lock);
	if (sc->sc_recchan.pipe != NULL) {
		uaudio_chan_close(sc, &sc->sc_recchan);
		uaudio_chan_free_buffers(sc, &sc->sc_recchan);
		sc->sc_recchan.intr = NULL;
	}
	mutex_spin_enter(&sc->sc_intr_lock);

	return 0;
}

Static int
uaudio_getdev(void *addr, struct audio_device *retp)
{
	struct uaudio_softc *sc;

	DPRINTF("%s", "\n");
	sc = addr;
	if (sc->sc_dying)
		return EIO;

	*retp = sc->sc_adev;
	return 0;
}

/*
 * Make sure the block size is large enough to hold all outstanding transfers.
 */
Static int
uaudio_round_blocksize(void *addr, int blk,
		       int mode, const audio_params_t *param)
{
	struct uaudio_softc *sc;
	int b;

	sc = addr;
	DPRINTF("blk=%d mode=%s\n", blk,
	    mode == AUMODE_PLAY ? "AUMODE_PLAY" : "AUMODE_RECORD");

	/* chan.bytes_per_frame can be 0. */
	if (mode == AUMODE_PLAY || sc->sc_recchan.bytes_per_frame <= 0) {
		b = param->sample_rate * UAUDIO_NFRAMES * UAUDIO_NCHANBUFS;

		/*
		 * This does not make accurate value in the case
		 * of b % USB_FRAMES_PER_SECOND != 0
		 */
		b /= USB_FRAMES_PER_SECOND;

		b *= param->precision / 8 * param->channels;
	} else {
		/*
		 * use wMaxPacketSize in bytes_per_frame.
		 * See uaudio_set_params() and uaudio_chan_init()
		 */
		b = sc->sc_recchan.bytes_per_frame
		    * UAUDIO_NFRAMES * UAUDIO_NCHANBUFS;
	}

	if (b <= 0)
		b = 1;
	blk = blk <= b ? b : blk / b * b;

#ifdef DIAGNOSTIC
	if (blk <= 0) {
		aprint_debug("uaudio_round_blocksize: blk=%d\n", blk);
		blk = 512;
	}
#endif

	DPRINTF("resultant blk=%d\n", blk);
	return blk;
}

Static int
uaudio_get_props(void *addr)
{
	return AUDIO_PROP_FULLDUPLEX | AUDIO_PROP_INDEPENDENT;

}

Static void
uaudio_get_locks(void *addr, kmutex_t **intr, kmutex_t **thread)
{
	struct uaudio_softc *sc;

	sc = addr;
	*intr = &sc->sc_intr_lock;
	*thread = &sc->sc_lock;
}

Static int
uaudio_get(struct uaudio_softc *sc, int which, int type, int wValue,
	   int wIndex, int len)
{
	usb_device_request_t req;
	u_int8_t data[4];
	usbd_status err;
	int val;

	if (wValue == -1)
		return 0;

	req.bmRequestType = type;
	req.bRequest = which;
	USETW(req.wValue, wValue);
	USETW(req.wIndex, wIndex);
	USETW(req.wLength, len);
	DPRINTFN(2,"type=0x%02x req=0x%02x wValue=0x%04x "
		    "wIndex=0x%04x len=%d\n",
		    type, which, wValue, wIndex, len);
	err = usbd_do_request(sc->sc_udev, &req, data);
	if (err) {
		DPRINTF("err=%s\n", usbd_errstr(err));
		return -1;
	}
	switch (len) {
	case 1:
		val = data[0];
		break;
	case 2:
		val = data[0] | (data[1] << 8);
		break;
	default:
		DPRINTF("bad length=%d\n", len);
		return -1;
	}
	DPRINTFN(2,"val=%d\n", val);
	return val;
}

Static void
uaudio_set(struct uaudio_softc *sc, int which, int type, int wValue,
	   int wIndex, int len, int val)
{
	usb_device_request_t req;
	u_int8_t data[4];
	int err __unused;

	if (wValue == -1)
		return;

	req.bmRequestType = type;
	req.bRequest = which;
	USETW(req.wValue, wValue);
	USETW(req.wIndex, wIndex);
	USETW(req.wLength, len);
	switch (len) {
	case 1:
		data[0] = val;
		break;
	case 2:
		data[0] = val;
		data[1] = val >> 8;
		break;
	default:
		return;
	}
	DPRINTFN(2,"type=0x%02x req=0x%02x wValue=0x%04x "
		    "wIndex=0x%04x len=%d, val=%d\n",
		    type, which, wValue, wIndex, len, val & 0xffff);
	err = usbd_do_request(sc->sc_udev, &req, data);
#ifdef UAUDIO_DEBUG
	if (err)
		DPRINTF("err=%d\n", err);
#endif
}

Static int
uaudio_signext(int type, int val)
{
	if (!MIX_UNSIGNED(type)) {
		if (MIX_SIZE(type) == 2)
			val = (int16_t)val;
		else
			val = (int8_t)val;
	}
	return val;
}

Static int
uaudio_value2bsd(struct mixerctl *mc, int val)
{
	DPRINTFN(5, "type=%03x val=%d min=%d max=%d ",
		     mc->type, val, mc->minval, mc->maxval);
	if (mc->type == MIX_ON_OFF) {
		val = (val != 0);
	} else if (mc->type == MIX_SELECTOR) {
		if (val < mc->minval || val > mc->maxval)
			val = mc->minval;
	} else
		val = ((uaudio_signext(mc->type, val) - mc->minval) * 255
			+ mc->mul/2) / mc->mul;
	DPRINTFN_CLEAN(5, "val'=%d\n", val);
	return val;
}

int
uaudio_bsd2value(struct mixerctl *mc, int val)
{
	DPRINTFN(5,"type=%03x val=%d min=%d max=%d ",
		    mc->type, val, mc->minval, mc->maxval);
	if (mc->type == MIX_ON_OFF) {
		val = (val != 0);
	} else if (mc->type == MIX_SELECTOR) {
		if (val < mc->minval || val > mc->maxval)
			val = mc->minval;
	} else
		val = (val + mc->delta/2) * mc->mul / 255 + mc->minval;
	DPRINTFN_CLEAN(5, "val'=%d\n", val);
	return val;
}

Static int
uaudio_ctl_get(struct uaudio_softc *sc, int which, struct mixerctl *mc,
	       int chan)
{
	int val;

	DPRINTFN(5,"which=%d chan=%d\n", which, chan);
	mutex_exit(&sc->sc_lock);
	val = uaudio_get(sc, which, UT_READ_CLASS_INTERFACE, mc->wValue[chan],
			 mc->wIndex, MIX_SIZE(mc->type));
	mutex_enter(&sc->sc_lock);
	return uaudio_value2bsd(mc, val);
}

Static void
uaudio_ctl_set(struct uaudio_softc *sc, int which, struct mixerctl *mc,
	       int chan, int val)
{

	val = uaudio_bsd2value(mc, val);
	mutex_exit(&sc->sc_lock);
	uaudio_set(sc, which, UT_WRITE_CLASS_INTERFACE, mc->wValue[chan],
		   mc->wIndex, MIX_SIZE(mc->type), val);
	mutex_enter(&sc->sc_lock);
}

Static int
uaudio_mixer_get_port(void *addr, mixer_ctrl_t *cp)
{
	struct uaudio_softc *sc;
	struct mixerctl *mc;
	int i, n, vals[MIX_MAX_CHAN], val;

	DPRINTFN(2, "index=%d\n", cp->dev);
	sc = addr;
	if (sc->sc_dying)
		return EIO;

	n = cp->dev - UAC_NCLASSES;
	if (n < 0 || n >= sc->sc_nctls)
		return ENXIO;
	mc = &sc->sc_ctls[n];

	if (mc->type == MIX_ON_OFF) {
		if (cp->type != AUDIO_MIXER_ENUM)
			return EINVAL;
		cp->un.ord = uaudio_ctl_get(sc, GET_CUR, mc, 0);
	} else if (mc->type == MIX_SELECTOR) {
		if (cp->type != AUDIO_MIXER_ENUM)
			return EINVAL;
		cp->un.ord = uaudio_ctl_get(sc, GET_CUR, mc, 0);
	} else {
		if (cp->type != AUDIO_MIXER_VALUE)
			return EINVAL;
		if (cp->un.value.num_channels != 1 &&
		    cp->un.value.num_channels != mc->nchan)
			return EINVAL;
		for (i = 0; i < mc->nchan; i++)
			vals[i] = uaudio_ctl_get(sc, GET_CUR, mc, i);
		if (cp->un.value.num_channels == 1 && mc->nchan != 1) {
			for (val = 0, i = 0; i < mc->nchan; i++)
				val += vals[i];
			vals[0] = val / mc->nchan;
		}
		for (i = 0; i < cp->un.value.num_channels; i++)
			cp->un.value.level[i] = vals[i];
	}

	return 0;
}

Static int
uaudio_mixer_set_port(void *addr, mixer_ctrl_t *cp)
{
	struct uaudio_softc *sc;
	struct mixerctl *mc;
	int i, n, vals[MIX_MAX_CHAN];

	DPRINTFN(2, "index = %d\n", cp->dev);
	sc = addr;
	if (sc->sc_dying)
		return EIO;

	n = cp->dev - UAC_NCLASSES;
	if (n < 0 || n >= sc->sc_nctls)
		return ENXIO;
	mc = &sc->sc_ctls[n];

	if (mc->type == MIX_ON_OFF) {
		if (cp->type != AUDIO_MIXER_ENUM)
			return EINVAL;
		uaudio_ctl_set(sc, SET_CUR, mc, 0, cp->un.ord);
	} else if (mc->type == MIX_SELECTOR) {
		if (cp->type != AUDIO_MIXER_ENUM)
			return EINVAL;
		uaudio_ctl_set(sc, SET_CUR, mc, 0, cp->un.ord);
	} else {
		if (cp->type != AUDIO_MIXER_VALUE)
			return EINVAL;
		if (cp->un.value.num_channels == 1)
			for (i = 0; i < mc->nchan; i++)
				vals[i] = cp->un.value.level[0];
		else if (cp->un.value.num_channels == mc->nchan)
			for (i = 0; i < mc->nchan; i++)
				vals[i] = cp->un.value.level[i];
		else
			return EINVAL;
		for (i = 0; i < mc->nchan; i++)
			uaudio_ctl_set(sc, SET_CUR, mc, i, vals[i]);
	}
	return 0;
}

Static int
uaudio_trigger_input(void *addr, void *start, void *end, int blksize,
		     void (*intr)(void *), void *arg,
		     const audio_params_t *param)
{
	struct uaudio_softc *sc;
	struct chan *ch;
	usbd_status err;
	int i;

	sc = addr;
	if (sc->sc_dying)
		return EIO;

	DPRINTFN(3, "sc=%p start=%p end=%p "
		    "blksize=%d\n", sc, start, end, blksize);
	ch = &sc->sc_recchan;
	uaudio_chan_set_param(ch, start, end, blksize);
	DPRINTFN(3, "sample_size=%d bytes/frame=%d "
		    "fraction=0.%03d\n", ch->sample_size, ch->bytes_per_frame,
		    ch->fraction);

	mutex_spin_exit(&sc->sc_intr_lock);
	err = uaudio_chan_alloc_buffers(sc, ch);
	if (err) {
		mutex_spin_enter(&sc->sc_intr_lock);
		return EIO;
	}

	err = uaudio_chan_open(sc, ch);
	if (err) {
		uaudio_chan_free_buffers(sc, ch);
		mutex_spin_enter(&sc->sc_intr_lock);
		return EIO;
	}

	ch->intr = intr;
	ch->arg = arg;

	for (i = 0; i < UAUDIO_NCHANBUFS-1; i++) /* XXX -1 shouldn't be needed */
		uaudio_chan_rtransfer(ch);
	mutex_spin_enter(&sc->sc_intr_lock);

	return 0;
}

Static int
uaudio_trigger_output(void *addr, void *start, void *end, int blksize,
		      void (*intr)(void *), void *arg,
		      const audio_params_t *param)
{
	struct uaudio_softc *sc;
	struct chan *ch;
	usbd_status err;
	int i;

	sc = addr;
	if (sc->sc_dying)
		return EIO;

	DPRINTFN(3, "sc=%p start=%p end=%p "
		    "blksize=%d\n", sc, start, end, blksize);
	ch = &sc->sc_playchan;
	uaudio_chan_set_param(ch, start, end, blksize);
	DPRINTFN(3, "sample_size=%d bytes/frame=%d "
		    "fraction=0.%03d\n", ch->sample_size, ch->bytes_per_frame,
		    ch->fraction);

	mutex_spin_exit(&sc->sc_intr_lock);
	err = uaudio_chan_alloc_buffers(sc, ch);
	if (err) {
		mutex_spin_enter(&sc->sc_intr_lock);
		return EIO;
	}

	err = uaudio_chan_open(sc, ch);
	if (err) {
		uaudio_chan_free_buffers(sc, ch);
		mutex_spin_enter(&sc->sc_intr_lock);
		return EIO;
	}

	ch->intr = intr;
	ch->arg = arg;

	for (i = 0; i < UAUDIO_NCHANBUFS-1; i++) /* XXX */
		uaudio_chan_ptransfer(ch);
	mutex_spin_enter(&sc->sc_intr_lock);

	return 0;
}

/* Set up a pipe for a channel. */
Static usbd_status
uaudio_chan_open(struct uaudio_softc *sc, struct chan *ch)
{
	struct as_info *as;
	usb_device_descriptor_t *ddesc;
	int endpt;
	usbd_status err;

	as = &sc->sc_alts[ch->altidx];
	endpt = as->edesc->bEndpointAddress;
	DPRINTF("endpt=0x%02x, speed=%d, alt=%d\n",
		 endpt, ch->sample_rate, as->alt);

	/* Set alternate interface corresponding to the mode. */
	err = usbd_set_interface(as->ifaceh, as->alt);
	if (err)
		return err;

	/*
	 * Roland SD-90 freezes by a SAMPLING_FREQ_CONTROL request.
	 */
	ddesc = usbd_get_device_descriptor(sc->sc_udev);
	if ((UGETW(ddesc->idVendor) != USB_VENDOR_ROLAND) &&
	    (UGETW(ddesc->idProduct) != USB_PRODUCT_ROLAND_SD90)) {
		err = uaudio_set_speed(sc, endpt, ch->sample_rate);
		if (err) {
			DPRINTF("set_speed failed err=%s\n", usbd_errstr(err));
		}
	}

	DPRINTF("create pipe to 0x%02x\n", endpt);
	err = usbd_open_pipe(as->ifaceh, endpt, USBD_MPSAFE, &ch->pipe);
	if (err)
		return err;
	if (as->edesc1 != NULL) {
		endpt = as->edesc1->bEndpointAddress;
		DPRINTF("create sync-pipe to 0x%02x\n", endpt);
		err = usbd_open_pipe(as->ifaceh, endpt, USBD_MPSAFE,
		    &ch->sync_pipe);
	}
	return err;
}

Static void
uaudio_chan_close(struct uaudio_softc *sc, struct chan *ch)
{
	usbd_pipe_handle pipe;
	struct as_info *as;

	as = &sc->sc_alts[ch->altidx];
	as->sc_busy = 0;
	AUFMT_VALIDATE(as->aformat);
	if (sc->sc_nullalt >= 0) {
		DPRINTF("set null alt=%d\n", sc->sc_nullalt);
		usbd_set_interface(as->ifaceh, sc->sc_nullalt);
	}
	pipe = atomic_swap_ptr(&ch->pipe, NULL);
	if (pipe) {
		usbd_abort_pipe(pipe);
		usbd_close_pipe(pipe);
	}
	pipe = atomic_swap_ptr(&ch->sync_pipe, NULL);
	if (pipe) {
		usbd_abort_pipe(pipe);
		usbd_close_pipe(pipe);
	}
}

Static usbd_status
uaudio_chan_alloc_buffers(struct uaudio_softc *sc, struct chan *ch)
{
	usbd_xfer_handle xfer;
	void *tbuf;
	int i, size;

	size = (ch->bytes_per_frame + ch->sample_size) * UAUDIO_NFRAMES;
	for (i = 0; i < UAUDIO_NCHANBUFS; i++) {
		xfer = usbd_alloc_xfer(sc->sc_udev);
		if (xfer == 0)
			goto bad;
		ch->chanbufs[i].xfer = xfer;
		tbuf = usbd_alloc_buffer(xfer, size);
		if (tbuf == 0) {
			i++;
			goto bad;
		}
		ch->chanbufs[i].buffer = tbuf;
		ch->chanbufs[i].chan = ch;
	}

	return USBD_NORMAL_COMPLETION;

bad:
	while (--i >= 0)
		/* implicit buffer free */
		usbd_free_xfer(ch->chanbufs[i].xfer);
	return USBD_NOMEM;
}

Static void
uaudio_chan_free_buffers(struct uaudio_softc *sc, struct chan *ch)
{
	int i;

	for (i = 0; i < UAUDIO_NCHANBUFS; i++)
		usbd_free_xfer(ch->chanbufs[i].xfer);
}

/* Called with USB lock held. */
Static void
uaudio_chan_ptransfer(struct chan *ch)
{
	struct chanbuf *cb;
	int i, n, size, residue, total;

	if (ch->sc->sc_dying)
		return;

	/* Pick the next channel buffer. */
	cb = &ch->chanbufs[ch->curchanbuf];
	if (++ch->curchanbuf >= UAUDIO_NCHANBUFS)
		ch->curchanbuf = 0;

	/* Compute the size of each frame in the next transfer. */
	residue = ch->residue;
	total = 0;
	for (i = 0; i < UAUDIO_NFRAMES; i++) {
		size = ch->bytes_per_frame;
		residue += ch->fraction;
		if (residue >= USB_FRAMES_PER_SECOND) {
			if ((ch->sc->sc_altflags & UA_NOFRAC) == 0)
				size += ch->sample_size;
			residue -= USB_FRAMES_PER_SECOND;
		}
		cb->sizes[i] = size;
		total += size;
	}
	ch->residue = residue;
	cb->size = total;

	/*
	 * Transfer data from upper layer buffer to channel buffer, taking
	 * care of wrapping the upper layer buffer.
	 */
	n = min(total, ch->end - ch->cur);
	memcpy(cb->buffer, ch->cur, n);
	ch->cur += n;
	if (ch->cur >= ch->end)
		ch->cur = ch->start;
	if (total > n) {
		total -= n;
		memcpy(cb->buffer + n, ch->cur, total);
		ch->cur += total;
	}

#ifdef UAUDIO_DEBUG
	if (uaudiodebug > 8) {
		DPRINTF("buffer=%p, residue=0.%03d\n", cb->buffer, ch->residue);
		for (i = 0; i < UAUDIO_NFRAMES; i++) {
			DPRINTF("   [%d] length %d\n", i, cb->sizes[i]);
		}
	}
#endif

	//DPRINTFN(5, "ptransfer xfer=%p\n", cb->xfer);
	/* Fill the request */
	usbd_setup_isoc_xfer(cb->xfer, ch->pipe, cb, cb->sizes,
			     UAUDIO_NFRAMES, USBD_NO_COPY,
			     uaudio_chan_pintr);

	(void)usbd_transfer(cb->xfer);
}

Static void
uaudio_chan_pintr(usbd_xfer_handle xfer, usbd_private_handle priv,
		  usbd_status status)
{
	struct chanbuf *cb;
	struct chan *ch;
	uint32_t count;

	cb = priv;
	ch = cb->chan;
	/* Return if we are aborting. */
	if (status == USBD_CANCELLED)
		return;

	usbd_get_xfer_status(xfer, NULL, NULL, &count, NULL);
	DPRINTFN(5, "count=%d, transferred=%d\n",
		    count, ch->transferred);
#ifdef DIAGNOSTIC
	if (count != cb->size) {
		aprint_error("uaudio_chan_pintr: count(%d) != size(%d)\n",
		       count, cb->size);
	}
#endif

	ch->transferred += cb->size;
	mutex_spin_enter(&ch->sc->sc_intr_lock);
	/* Call back to upper layer */
	while (ch->transferred >= ch->blksize) {
		ch->transferred -= ch->blksize;
		DPRINTFN(5, "call %p(%p)\n", ch->intr, ch->arg);
		ch->intr(ch->arg);
	}
	mutex_spin_exit(&ch->sc->sc_intr_lock);

	/* start next transfer */
	uaudio_chan_ptransfer(ch);
}

/* Called with USB lock held. */
Static void
uaudio_chan_rtransfer(struct chan *ch)
{
	struct chanbuf *cb;
	int i, size, residue, total;

	if (ch->sc->sc_dying)
		return;

	/* Pick the next channel buffer. */
	cb = &ch->chanbufs[ch->curchanbuf];
	if (++ch->curchanbuf >= UAUDIO_NCHANBUFS)
		ch->curchanbuf = 0;

	/* Compute the size of each frame in the next transfer. */
	residue = ch->residue;
	total = 0;
	for (i = 0; i < UAUDIO_NFRAMES; i++) {
		size = ch->bytes_per_frame;
		cb->sizes[i] = size;
		cb->offsets[i] = total;
		total += size;
	}
	ch->residue = residue;
	cb->size = total;

#ifdef UAUDIO_DEBUG
	if (uaudiodebug > 8) {
		DPRINTF("buffer=%p, residue=0.%03d\n", cb->buffer, ch->residue);
		for (i = 0; i < UAUDIO_NFRAMES; i++) {
			DPRINTF("   [%d] length %d\n", i, cb->sizes[i]);
		}
	}
#endif

	DPRINTFN(5, "transfer xfer=%p\n", cb->xfer);
	/* Fill the request */
	usbd_setup_isoc_xfer(cb->xfer, ch->pipe, cb, cb->sizes,
			     UAUDIO_NFRAMES, USBD_NO_COPY,
			     uaudio_chan_rintr);

	(void)usbd_transfer(cb->xfer);
}

Static void
uaudio_chan_rintr(usbd_xfer_handle xfer, usbd_private_handle priv,
		  usbd_status status)
{
	struct chanbuf *cb;
	struct chan *ch;
	uint32_t count;
	int i, n, frsize;

	cb = priv;
	ch = cb->chan;
	/* Return if we are aborting. */
	if (status == USBD_CANCELLED)
		return;

	usbd_get_xfer_status(xfer, NULL, NULL, &count, NULL);
	DPRINTFN(5, "count=%d, transferred=%d\n", count, ch->transferred);

	/* count < cb->size is normal for asynchronous source */
#ifdef DIAGNOSTIC
	if (count > cb->size) {
		aprint_error("uaudio_chan_rintr: count(%d) > size(%d)\n",
		       count, cb->size);
	}
#endif

	/*
	 * Transfer data from channel buffer to upper layer buffer, taking
	 * care of wrapping the upper layer buffer.
	 */
	for(i = 0; i < UAUDIO_NFRAMES; i++) {
		frsize = cb->sizes[i];
		n = min(frsize, ch->end - ch->cur);
		memcpy(ch->cur, cb->buffer + cb->offsets[i], n);
		ch->cur += n;
		if (ch->cur >= ch->end)
			ch->cur = ch->start;
		if (frsize > n) {
			memcpy(ch->cur, cb->buffer + cb->offsets[i] + n,
			    frsize - n);
			ch->cur += frsize - n;
		}
	}

	/* Call back to upper layer */
	ch->transferred += count;
	mutex_spin_enter(&ch->sc->sc_intr_lock);
	while (ch->transferred >= ch->blksize) {
		ch->transferred -= ch->blksize;
		DPRINTFN(5, "call %p(%p)\n", ch->intr, ch->arg);
		ch->intr(ch->arg);
	}
	mutex_spin_exit(&ch->sc->sc_intr_lock);

	/* start next transfer */
	uaudio_chan_rtransfer(ch);
}

Static void
uaudio_chan_init(struct chan *ch, int altidx, const struct audio_params *param,
    int maxpktsize)
{
	int samples_per_frame, sample_size;

	ch->altidx = altidx;
	sample_size = param->precision * param->channels / 8;
	samples_per_frame = param->sample_rate / USB_FRAMES_PER_SECOND;
	ch->sample_size = sample_size;
	ch->sample_rate = param->sample_rate;
	if (maxpktsize == 0) {
		ch->fraction = param->sample_rate % USB_FRAMES_PER_SECOND;
		ch->bytes_per_frame = samples_per_frame * sample_size;
	} else {
		ch->fraction = 0;
		ch->bytes_per_frame = maxpktsize;
	}
	ch->residue = 0;
}

Static void
uaudio_chan_set_param(struct chan *ch, u_char *start, u_char *end, int blksize)
{

	ch->start = start;
	ch->end = end;
	ch->cur = start;
	ch->blksize = blksize;
	ch->transferred = 0;
	ch->curchanbuf = 0;
}

Static int
uaudio_set_params(void *addr, int setmode, int usemode,
		  struct audio_params *play, struct audio_params *rec,
		  stream_filter_list_t *pfil, stream_filter_list_t *rfil)
{
	struct uaudio_softc *sc;
	int paltidx, raltidx;
	struct audio_params *p;
	stream_filter_list_t *fil;
	int mode, i;

	sc = addr;
	paltidx = -1;
	raltidx = -1;
	if (sc->sc_dying)
		return EIO;

	if (((usemode & AUMODE_PLAY) && sc->sc_playchan.pipe != NULL) ||
	    ((usemode & AUMODE_RECORD) && sc->sc_recchan.pipe != NULL))
		return EBUSY;

	if ((usemode & AUMODE_PLAY) && sc->sc_playchan.altidx != -1) {
		sc->sc_alts[sc->sc_playchan.altidx].sc_busy = 0;
		AUFMT_VALIDATE(sc->sc_alts[sc->sc_playchan.altidx].aformat);
	}
	if ((usemode & AUMODE_RECORD) && sc->sc_recchan.altidx != -1) {
		sc->sc_alts[sc->sc_recchan.altidx].sc_busy = 0;
		AUFMT_VALIDATE(sc->sc_alts[sc->sc_recchan.altidx].aformat);
	}

	/* Some uaudio devices are unidirectional.  Don't try to find a
	   matching mode for the unsupported direction. */
	setmode &= sc->sc_mode;

	for (mode = AUMODE_RECORD; mode != -1;
	     mode = mode == AUMODE_RECORD ? AUMODE_PLAY : -1) {
		if ((setmode & mode) == 0)
			continue;

		if (mode == AUMODE_PLAY) {
			p = play;
			fil = pfil;
		} else {
			p = rec;
			fil = rfil;
		}
		i = auconv_set_converter(sc->sc_formats, sc->sc_nformats,
					 mode, p, TRUE, fil);
		if (i < 0)
			return EINVAL;

		if (mode == AUMODE_PLAY)
			paltidx = i;
		else
			raltidx = i;
	}

	if ((setmode & AUMODE_PLAY)) {
		p = pfil->req_size > 0 ? &pfil->filters[0].param : play;
		/* XXX abort transfer if currently happening? */
		uaudio_chan_init(&sc->sc_playchan, paltidx, p, 0);
	}
	if ((setmode & AUMODE_RECORD)) {
		p = rfil->req_size > 0 ? &rfil->filters[0].param : rec;
		/* XXX abort transfer if currently happening? */
		uaudio_chan_init(&sc->sc_recchan, raltidx, p,
		    UGETW(sc->sc_alts[raltidx].edesc->wMaxPacketSize));
	}

	if ((usemode & AUMODE_PLAY) && sc->sc_playchan.altidx != -1) {
		sc->sc_alts[sc->sc_playchan.altidx].sc_busy = 1;
		AUFMT_INVALIDATE(sc->sc_alts[sc->sc_playchan.altidx].aformat);
	}
	if ((usemode & AUMODE_RECORD) && sc->sc_recchan.altidx != -1) {
		sc->sc_alts[sc->sc_recchan.altidx].sc_busy = 1;
		AUFMT_INVALIDATE(sc->sc_alts[sc->sc_recchan.altidx].aformat);
	}

	DPRINTF("use altidx=p%d/r%d, altno=p%d/r%d\n",
		 sc->sc_playchan.altidx, sc->sc_recchan.altidx,
		 (sc->sc_playchan.altidx >= 0)
		   ?sc->sc_alts[sc->sc_playchan.altidx].idesc->bAlternateSetting
		   : -1,
		 (sc->sc_recchan.altidx >= 0)
		   ? sc->sc_alts[sc->sc_recchan.altidx].idesc->bAlternateSetting
		   : -1);

	return 0;
}

Static usbd_status
uaudio_set_speed(struct uaudio_softc *sc, int endpt, u_int speed)
{
	usb_device_request_t req;
	usbd_status err;
	uint8_t data[3];

	DPRINTFN(5, "endpt=%d speed=%u\n", endpt, speed);
	req.bmRequestType = UT_WRITE_CLASS_ENDPOINT;
	req.bRequest = SET_CUR;
	USETW2(req.wValue, SAMPLING_FREQ_CONTROL, 0);
	USETW(req.wIndex, endpt);
	USETW(req.wLength, 3);
	data[0] = speed;
	data[1] = speed >> 8;
	data[2] = speed >> 16;

	err = usbd_do_request(sc->sc_udev, &req, data);

	return err;
}

#ifdef _MODULE

MODULE(MODULE_CLASS_DRIVER, uaudio, NULL);

static const struct cfiattrdata audiobuscf_iattrdata = {
	"audiobus", 0, { { NULL, NULL, 0 }, }
};
static const struct cfiattrdata * const uaudio_attrs[] = {
	&audiobuscf_iattrdata, NULL
};
CFDRIVER_DECL(uaudio, DV_DULL, uaudio_attrs);
extern struct cfattach uaudio_ca;
static int uaudioloc[6/*USBIFIFCF_NLOCS*/] = {
	-1/*USBIFIFCF_PORT_DEFAULT*/,
	-1/*USBIFIFCF_CONFIGURATION_DEFAULT*/,
	-1/*USBIFIFCF_INTERFACE_DEFAULT*/,
	-1/*USBIFIFCF_VENDOR_DEFAULT*/,
	-1/*USBIFIFCF_PRODUCT_DEFAULT*/,
	-1/*USBIFIFCF_RELEASE_DEFAULT*/};
static struct cfparent uhubparent = {
	"usbifif", NULL, DVUNIT_ANY
};
static struct cfdata uaudio_cfdata[] = {
	{
		.cf_name = "uaudio",
		.cf_atname = "uaudio",
		.cf_unit = 0,
		.cf_fstate = FSTATE_STAR,
		.cf_loc = uaudioloc,
		.cf_flags = 0,
		.cf_pspec = &uhubparent,
	},
	{ NULL }
};

static int
uaudio_modcmd(modcmd_t cmd, void *arg)
{
	int err;

	switch (cmd) {
	case MODULE_CMD_INIT:
		err = config_cfdriver_attach(&uaudio_cd);
		if (err) {
			return err;
		}
		err = config_cfattach_attach("uaudio", &uaudio_ca);
		if (err) {
			config_cfdriver_detach(&uaudio_cd);
			return err;
		}
		err = config_cfdata_attach(uaudio_cfdata, 1);
		if (err) {
			config_cfattach_detach("uaudio", &uaudio_ca);
			config_cfdriver_detach(&uaudio_cd);
			return err;
		}
		return 0;
	case MODULE_CMD_FINI:
		err = config_cfdata_detach(uaudio_cfdata);
		if (err)
			return err;
		config_cfattach_detach("uaudio", &uaudio_ca);
		config_cfdriver_detach(&uaudio_cd);
		return 0;
	default:
		return ENOTTY;
	}
}

#endif

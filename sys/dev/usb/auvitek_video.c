/* $NetBSD: auvitek_video.c,v 1.6 2011/10/02 19:15:40 jmcneill Exp $ */

/*-
 * Copyright (c) 2010 Jared D. McNeill <jmcneill@invisible.ca>
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
 * Auvitek AU0828 USB controller
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: auvitek_video.c,v 1.6 2011/10/02 19:15:40 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/video_if.h>

#include <dev/usb/auvitekreg.h>
#include <dev/usb/auvitekvar.h>

#define	AUVITEK_FORMAT_DEFAULT		0
#define	AUVITEK_STANDARD_NTSC_M		0
#define	AUVITEK_TUNER_DEFAULT		0
#define	AUVITEK_AUDIO_TELEVISION	0
#define	AUVITEK_AUDIO_LINEIN		1
#define	AUVITEK_INPUT_COMPOSITE		0
#define	AUVITEK_INPUT_SVIDEO		1
#define	AUVITEK_INPUT_TELEVISION	2
#define	AUVITEK_INPUT_CABLE		3

static int		auvitek_open(void *, int);
static void		auvitek_close(void *);
static const char *	auvitek_get_devname(void *);
static const char *	auvitek_get_businfo(void *);
static int		auvitek_enum_format(void *, uint32_t,
					  struct video_format *);
static int		auvitek_get_format(void *, struct video_format *);
static int		auvitek_set_format(void *, struct video_format *);
static int		auvitek_try_format(void *, struct video_format *);
static int		auvitek_enum_standard(void *, uint32_t,
					      enum video_standard *);
static int		auvitek_get_standard(void *, enum video_standard *);
static int		auvitek_set_standard(void *, enum video_standard);
static int		auvitek_start_transfer(void *);
static int		auvitek_stop_transfer(void *);
static int		auvitek_get_tuner(void *, struct video_tuner *);
static int		auvitek_set_tuner(void *, struct video_tuner *);
static int		auvitek_enum_audio(void *, uint32_t,
					   struct video_audio *);
static int		auvitek_get_audio(void *, struct video_audio *);
static int		auvitek_set_audio(void *, struct video_audio *);
static int		auvitek_enum_input(void *, uint32_t,
					   struct video_input *);
static int		auvitek_get_input(void *, struct video_input *);
static int		auvitek_set_input(void *, struct video_input *);
static int		auvitek_get_frequency(void *, struct video_frequency *);
static int		auvitek_set_frequency(void *, struct video_frequency *);

static int		auvitek_start_xfer(struct auvitek_softc *);
static int		auvitek_stop_xfer(struct auvitek_softc *);
static int		auvitek_isoc_start(struct auvitek_softc *);
static int		auvitek_isoc_start1(struct auvitek_isoc *);
static void		auvitek_isoc_intr(usbd_xfer_handle,
					  usbd_private_handle,
					  usbd_status);
static int		auvitek_isoc_process(struct auvitek_softc *,
					     uint8_t *, uint32_t);
static void		auvitek_videobuf_weave(struct auvitek_softc *,
					       uint8_t *, uint32_t);

static const struct video_hw_if auvitek_video_if = {
	.open = auvitek_open,
	.close = auvitek_close,
	.get_devname = auvitek_get_devname,
	.get_businfo = auvitek_get_businfo,
	.enum_format = auvitek_enum_format,
	.get_format = auvitek_get_format,
	.set_format = auvitek_set_format,
	.try_format = auvitek_try_format,
	.enum_standard = auvitek_enum_standard,
	.get_standard = auvitek_get_standard,
	.set_standard = auvitek_set_standard,
	.start_transfer = auvitek_start_transfer,
	.stop_transfer = auvitek_stop_transfer,
	.get_tuner = auvitek_get_tuner,
	.set_tuner = auvitek_set_tuner,
	.enum_audio = auvitek_enum_audio,
	.get_audio = auvitek_get_audio,
	.set_audio = auvitek_set_audio,
	.enum_input = auvitek_enum_input,
	.get_input = auvitek_get_input,
	.set_input = auvitek_set_input,
	.get_frequency = auvitek_get_frequency,
	.set_frequency = auvitek_set_frequency,
};

int
auvitek_video_attach(struct auvitek_softc *sc)
{
	snprintf(sc->sc_businfo, sizeof(sc->sc_businfo), "usb:%08x",
	    sc->sc_udev->cookie.cookie);

	auvitek_video_rescan(sc, NULL, NULL);

	return (sc->sc_videodev != NULL);
}

int
auvitek_video_detach(struct auvitek_softc *sc, int flags)
{
	if (sc->sc_videodev != NULL) {
		config_detach(sc->sc_videodev, flags);
		sc->sc_videodev = NULL;
	}

	return 0;
}

void
auvitek_video_rescan(struct auvitek_softc *sc, const char *ifattr,
    const int *locs)
{
	if (ifattr_match(ifattr, "videobus") && sc->sc_videodev == NULL)
		sc->sc_videodev = video_attach_mi(&auvitek_video_if,
		    sc->sc_dev);
}

void
auvitek_video_childdet(struct auvitek_softc *sc, device_t child)
{
	if (sc->sc_videodev == child)
		sc->sc_videodev = NULL;
}

static int
auvitek_open(void *opaque, int flags)
{
	struct auvitek_softc *sc = opaque;

	if (sc->sc_dying)
		return EIO;

	auvitek_attach_tuner(sc->sc_dev);

	if (sc->sc_xc5k == NULL)
		return ENXIO;

	return 0;
}

static void
auvitek_close(void *opaque)
{
}

static const char *
auvitek_get_devname(void *opaque)
{
	struct auvitek_softc *sc = opaque;

	return sc->sc_descr;
}

static const char *
auvitek_get_businfo(void *opaque)
{
	struct auvitek_softc *sc = opaque;

	return sc->sc_businfo;
}

static int
auvitek_enum_format(void *opaque, uint32_t index, struct video_format *format)
{
	if (index != AUVITEK_FORMAT_DEFAULT)
		return EINVAL;

	format->pixel_format = VIDEO_FORMAT_UYVY;

	return 0;
}

static int
auvitek_get_format(void *opaque, struct video_format *format)
{

	format->pixel_format = VIDEO_FORMAT_UYVY;
	format->width = 720;
	format->height = 480;
	format->stride = format->width * 2;
	format->sample_size = format->stride * format->height;
	format->aspect_x = 4;
	format->aspect_y = 3;
	format->color.primaries = VIDEO_COLOR_PRIMARIES_SMPTE_170M;
	format->color.gamma_function = VIDEO_GAMMA_FUNCTION_UNSPECIFIED;
	format->color.matrix_coeff = VIDEO_MATRIX_COEFF_UNSPECIFIED;
	format->interlace_flags = VIDEO_INTERLACE_ON;
	format->priv = 0;

	return 0;
}

static int
auvitek_set_format(void *opaque, struct video_format *format)
{
	if (format->pixel_format != VIDEO_FORMAT_UYVY)
		return EINVAL;

	return auvitek_get_format(opaque, format);
}

static int
auvitek_try_format(void *opaque, struct video_format *format)
{
	return auvitek_get_format(opaque, format);
}

static int
auvitek_enum_standard(void *opaque, uint32_t index, enum video_standard *vstd)
{
	switch (index) {
	case AUVITEK_STANDARD_NTSC_M:
		*vstd = VIDEO_STANDARD_NTSC_M;
		return 0;
	default:
		return EINVAL;
	}
}

static int
auvitek_get_standard(void *opaque, enum video_standard *vstd)
{
	*vstd = VIDEO_STANDARD_NTSC_M;
	return 0;
}

static int
auvitek_set_standard(void *opaque, enum video_standard vstd)
{
	switch (vstd) {
	case VIDEO_STANDARD_NTSC_M:
		return 0;
	default:
		return EINVAL;
	}
}

static int
auvitek_start_transfer(void *opaque)
{
	struct auvitek_softc *sc = opaque;
	int error, s;
	uint16_t vpos = 0, hpos = 0;
	uint16_t hres = 720 * 2;
	uint16_t vres = 484 / 2;

	auvitek_write_1(sc, AU0828_REG_SENSORVBI_CTL, 0x00);

	/* program video position and size */
	auvitek_write_1(sc, AU0828_REG_HPOS_LO, hpos & 0xff);
	auvitek_write_1(sc, AU0828_REG_HPOS_HI, hpos >> 8);
	auvitek_write_1(sc, AU0828_REG_VPOS_LO, vpos & 0xff);
	auvitek_write_1(sc, AU0828_REG_VPOS_HI, vpos >> 8);
	auvitek_write_1(sc, AU0828_REG_HRES_LO, hres & 0xff);
	auvitek_write_1(sc, AU0828_REG_HRES_HI, hres >> 8);
	auvitek_write_1(sc, AU0828_REG_VRES_LO, vres & 0xff);
	auvitek_write_1(sc, AU0828_REG_VRES_HI, vres >> 8);

	auvitek_write_1(sc, AU0828_REG_SENSOR_CTL, 0xb3);

	auvitek_write_1(sc, AU0828_REG_AUDIOCTL, 0x01);

	s = splusb();
	error = auvitek_start_xfer(sc);
	splx(s);

	if (error)
		auvitek_stop_transfer(sc);

	return error;
}

static int
auvitek_stop_transfer(void *opaque)
{
	struct auvitek_softc *sc = opaque;
	int error, s;

	auvitek_write_1(sc, AU0828_REG_SENSOR_CTL, 0x00);

	s = splusb();
	error = auvitek_stop_xfer(sc);
	splx(s);

	return error;
}

static int
auvitek_get_tuner(void *opaque, struct video_tuner *vt)
{
	struct auvitek_softc *sc = opaque;

	switch (vt->index) {
	case AUVITEK_TUNER_DEFAULT:
		strlcpy(vt->name, "XC5000", sizeof(vt->name));
		vt->freq_lo =  44000000 / 62500;
		vt->freq_hi = 958000000 / 62500;
		vt->caps = VIDEO_TUNER_F_STEREO;
		vt->mode = VIDEO_TUNER_F_STEREO;
		if (sc->sc_au8522)
			vt->signal = au8522_get_signal(sc->sc_au8522);
		else
			vt->signal = 0;
		break;
	default:
		return EINVAL;
	}

	return 0;
}

static int
auvitek_set_tuner(void *opaque, struct video_tuner *vt)
{
	if (vt->index != AUVITEK_TUNER_DEFAULT)
		return EINVAL;
	return 0;
}

static int
auvitek_enum_audio(void *opaque, uint32_t index, struct video_audio *va)
{
	switch (index) {
	case AUVITEK_AUDIO_TELEVISION:
		strlcpy(va->name, "Television", sizeof(va->name));
		va->caps = VIDEO_AUDIO_F_STEREO;
		break;
	case AUVITEK_AUDIO_LINEIN:
		strlcpy(va->name, "Line In", sizeof(va->name));
		va->caps = VIDEO_AUDIO_F_STEREO;
		break;
	default:
		return EINVAL;
	}

	return 0;
}

static int
auvitek_get_audio(void *opaque, struct video_audio *va)
{
	struct auvitek_softc *sc = opaque;

	return auvitek_enum_audio(opaque, sc->sc_ainput, va);
}

static int
auvitek_set_audio(void *opaque, struct video_audio *va)
{
	struct auvitek_softc *sc = opaque;

	if (va->index == sc->sc_ainput)
		return 0;

	return EINVAL;
}

static int
auvitek_enum_input(void *opaque, uint32_t index, struct video_input *vi)
{
	switch (index) {
	case AUVITEK_INPUT_COMPOSITE:
		strlcpy(vi->name, "Composite", sizeof(vi->name));
		vi->type = VIDEO_INPUT_TYPE_BASEBAND;
		vi->standards = VIDEO_STANDARD_NTSC_M;
		break;
	case AUVITEK_INPUT_SVIDEO:
		strlcpy(vi->name, "S-Video", sizeof(vi->name));
		vi->type = VIDEO_INPUT_TYPE_BASEBAND;
		vi->standards = VIDEO_STANDARD_NTSC_M;
		break;
	case AUVITEK_INPUT_TELEVISION:
		strlcpy(vi->name, "Television", sizeof(vi->name));
		vi->type = VIDEO_INPUT_TYPE_TUNER;
		vi->standards = VIDEO_STANDARD_NTSC_M;
		break;
	case AUVITEK_INPUT_CABLE:
		strlcpy(vi->name, "Cable TV", sizeof(vi->name));
		vi->type = VIDEO_INPUT_TYPE_TUNER;
		vi->standards = VIDEO_STANDARD_NTSC_M;
		break;
	default:
		return EINVAL;
	}

	vi->index = index;
	vi->tuner_index = AUVITEK_TUNER_DEFAULT;

	return 0;
}

static int
auvitek_get_input(void *opaque, struct video_input *vi)
{
	struct auvitek_softc *sc = opaque;

	return auvitek_enum_input(opaque, sc->sc_vinput, vi);
}

static int
auvitek_set_input(void *opaque, struct video_input *vi)
{
	struct auvitek_softc *sc = opaque;
	struct video_frequency vf;
	au8522_vinput_t vinput = AU8522_VINPUT_UNCONF;
	au8522_ainput_t ainput = AU8522_AINPUT_UNCONF;
	uint8_t r;

	switch (vi->index) {
	case AUVITEK_INPUT_COMPOSITE:
		vinput = AU8522_VINPUT_CVBS;
		ainput = AU8522_AINPUT_NONE;
		sc->sc_ainput = AUVITEK_AUDIO_LINEIN;
		break;
	case AUVITEK_INPUT_SVIDEO:
		vinput = AU8522_VINPUT_SVIDEO;
		ainput = AU8522_AINPUT_NONE;
		sc->sc_ainput = AUVITEK_AUDIO_LINEIN;
		break;
	case AUVITEK_INPUT_TELEVISION:
	case AUVITEK_INPUT_CABLE:
		vinput = AU8522_VINPUT_CVBS_TUNER;
		ainput = AU8522_AINPUT_SIF;
		sc->sc_ainput = AUVITEK_AUDIO_TELEVISION;
		break;
	default:
		return EINVAL;
	}

	sc->sc_vinput = vi->index;

	au8522_set_input(sc->sc_au8522, vinput, ainput);

	/* XXX HVR-850/950Q specific */
	r = auvitek_read_1(sc, AU0828_REG_GPIO1_OUTEN);
	if (ainput == AU8522_AINPUT_NONE)
		r |= 0x10;
	else
		r &= ~0x10;
	auvitek_write_1(sc, AU0828_REG_GPIO1_OUTEN, r);

	if (vinput == AU8522_VINPUT_CVBS_TUNER && sc->sc_curfreq > 0) {
		vf.tuner_index = AUVITEK_TUNER_DEFAULT;
		vf.frequency = sc->sc_curfreq;
		auvitek_set_frequency(sc, &vf);
	}

	return 0;
}

static int
auvitek_get_frequency(void *opaque, struct video_frequency *vf)
{
	struct auvitek_softc *sc = opaque;

	if (sc->sc_vinput != AUVITEK_INPUT_TELEVISION &&
	    sc->sc_vinput != AUVITEK_INPUT_CABLE)
		return EINVAL;

	vf->tuner_index = AUVITEK_TUNER_DEFAULT;
	vf->frequency = sc->sc_curfreq;

	return 0;
}

static int
auvitek_set_frequency(void *opaque, struct video_frequency *vf)
{
	struct auvitek_softc *sc = opaque;
	struct xc5k_params params;
	int error;

	if (sc->sc_vinput != AUVITEK_INPUT_TELEVISION &&
	    sc->sc_vinput != AUVITEK_INPUT_CABLE)
		return EINVAL;
	if (vf->tuner_index != AUVITEK_TUNER_DEFAULT)
		return EINVAL;
	if (sc->sc_xc5k == NULL)
		return ENODEV;

	params.standard = VIDEO_STANDARD_NTSC_M;
	if (sc->sc_vinput == AUVITEK_INPUT_TELEVISION)
		params.signal_source = XC5K_SIGNAL_SOURCE_AIR;
	else
		params.signal_source = XC5K_SIGNAL_SOURCE_CABLE;
	params.frequency = vf->frequency;
	if (sc->sc_au8522)
		au8522_set_audio(sc->sc_au8522, false);
	error = xc5k_tune_video(sc->sc_xc5k, &params);
	if (sc->sc_au8522)
		au8522_set_audio(sc->sc_au8522, true);
	if (error)
		return error;

	sc->sc_curfreq = vf->frequency;

	auvitek_write_1(sc, AU0828_REG_SENSOR_CTL, 0x00);
	delay(30000);
	auvitek_write_1(sc, AU0828_REG_SENSOR_CTL, 0xb3);

	return 0;
}

static int
auvitek_start_xfer(struct auvitek_softc *sc)
{
	struct auvitek_xfer *ax = &sc->sc_ax;
	uint32_t vframe_len, uframe_len, nframes;
	usbd_status err;
	int i;

	err = usbd_set_interface(sc->sc_isoc_iface, AUVITEK_XFER_ALTNO);
	if (err != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(sc->sc_dev, "couldn't set altno %d: %s\n",
		    AUVITEK_XFER_ALTNO, usbd_errstr(err));
		return EIO;
	}

	vframe_len = 720 * 480 * 2;
	uframe_len = ax->ax_maxpktlen;
	nframes = (vframe_len + uframe_len - 1) / uframe_len;
	nframes = (nframes + 7) & ~7;

	ax->ax_nframes = nframes;
	ax->ax_uframe_len = uframe_len;
	for (i = 0; i < AUVITEK_NISOC_XFERS; i++) {
		struct auvitek_isoc *isoc = &ax->ax_i[i];
		isoc->i_ax = ax;
		isoc->i_frlengths =
		    kmem_alloc(sizeof(isoc->i_frlengths[0]) * nframes,
			KM_SLEEP);
	}

	err = usbd_open_pipe(sc->sc_isoc_iface, ax->ax_endpt,
	    USBD_EXCLUSIVE_USE, &ax->ax_pipe);
	if (err != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(sc->sc_dev, "couldn't open pipe: %s\n",
		    usbd_errstr(err));
		return EIO;
	}

	for (i = 0; i < AUVITEK_NISOC_XFERS; i++) {
		struct auvitek_isoc *isoc = &ax->ax_i[i];

		isoc->i_xfer = usbd_alloc_xfer(sc->sc_udev);
		if (isoc->i_xfer == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "couldn't allocate usb xfer\n");
			return ENOMEM;
		}

		isoc->i_buf = usbd_alloc_buffer(isoc->i_xfer,
						nframes * uframe_len);
		if (isoc->i_buf == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "couldn't allocate usb xfer buffer\n");
			return ENOMEM;
		}
	}

	return auvitek_isoc_start(sc);
}

static int
auvitek_stop_xfer(struct auvitek_softc *sc)
{
	struct auvitek_xfer *ax = &sc->sc_ax;
	usbd_status err;
	int i;

	if (ax->ax_pipe != NULL) {
		usbd_abort_pipe(ax->ax_pipe);
		usbd_close_pipe(ax->ax_pipe);
		ax->ax_pipe = NULL;
	}

	for (i = 0; i < AUVITEK_NISOC_XFERS; i++) {
		struct auvitek_isoc *isoc = &ax->ax_i[i];
		if (isoc->i_xfer != NULL) {
			usbd_free_buffer(isoc->i_xfer);
			usbd_free_xfer(isoc->i_xfer);
			isoc->i_xfer = NULL;
		}
		if (isoc->i_frlengths != NULL) {
			kmem_free(isoc->i_frlengths,
			    sizeof(isoc->i_frlengths[0]) * ax->ax_nframes);
			isoc->i_frlengths = NULL;
		}
	}

	usbd_delay_ms(sc->sc_udev, 1000);
	err = usbd_set_interface(sc->sc_isoc_iface, 0);
	if (err != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(sc->sc_dev,
		    "couldn't set zero bw interface: %s\n",
		    usbd_errstr(err));
		return EIO;
	}

	return 0;
}

static int
auvitek_isoc_start(struct auvitek_softc *sc)
{
	struct auvitek_xfer *ax = &sc->sc_ax;
	int i, error;

	ax->ax_av.av_el = ax->ax_av.av_ol = 0;
	ax->ax_av.av_eb = ax->ax_av.av_ob = 0;
	ax->ax_av.av_stride = 720 * 2;

	for (i = 0; i < AUVITEK_NISOC_XFERS; i++) {
		error = auvitek_isoc_start1(&ax->ax_i[i]);
		if (error)
			return error;
	}

	return 0;
}

static int
auvitek_isoc_start1(struct auvitek_isoc *isoc)
{
	struct auvitek_xfer *ax = isoc->i_ax;
	struct auvitek_softc *sc = ax->ax_sc;
	usbd_status err;
	unsigned int i;

	ax = isoc->i_ax;

	for (i = 0; i < ax->ax_nframes; i++)
		isoc->i_frlengths[i] = ax->ax_uframe_len;

	usbd_setup_isoc_xfer(isoc->i_xfer,
			     ax->ax_pipe,
			     isoc,
			     isoc->i_frlengths,
			     ax->ax_nframes,
			     USBD_NO_COPY | USBD_SHORT_XFER_OK,
			     auvitek_isoc_intr);

	err = usbd_transfer(isoc->i_xfer);
	if (err != USBD_IN_PROGRESS) {
		aprint_error_dev(sc->sc_dev, "couldn't start isoc xfer: %s\n",
				 usbd_errstr(err));
		return ENODEV;
	}

	return 0;
}

static void
auvitek_isoc_intr(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct auvitek_isoc *isoc = priv;
	struct auvitek_xfer *ax = isoc->i_ax;
	struct auvitek_softc *sc = ax->ax_sc;
	uint32_t count;
	uint8_t *buf;
	unsigned int i;

	if (sc->sc_dying)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_STALLED) {
			usbd_clear_endpoint_stall_async(ax->ax_pipe);
			goto next;
		}
		return;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &count, NULL);

	if (count == 0)
		goto next;

	for (i = 0, buf = isoc->i_buf;
	     i < ax->ax_nframes;
	     ++i, buf += ax->ax_uframe_len) {
		status = auvitek_isoc_process(sc, buf, isoc->i_frlengths[i]);
		if (status == USBD_IOERROR)
			break;
	}

next:
	auvitek_isoc_start1(isoc);
}

static int
auvitek_isoc_process(struct auvitek_softc *sc, uint8_t *buf, uint32_t len)
{
	struct video_payload payload;
	bool submit = false;

	if (buf[0] & 0x80) {
		sc->sc_ax.ax_frinfo = buf[0];
		if (sc->sc_ax.ax_frinfo & 0x40) {
			sc->sc_ax.ax_frno = !sc->sc_ax.ax_frno;
			submit = true;
		}
		buf += 4;
		len -= 4;
	}
	buf += 4;
	len -= 4;

	auvitek_videobuf_weave(sc, buf, len);

	if (submit) {
		payload.end_of_frame = 1;
		payload.data = sc->sc_ax.ax_av.av_buf;
		payload.size = sizeof(sc->sc_ax.ax_av.av_buf);
		payload.frameno = -1;

		video_submit_payload(sc->sc_videodev, &payload);

		sc->sc_ax.ax_av.av_el = sc->sc_ax.ax_av.av_ol = 0;
		sc->sc_ax.ax_av.av_eb = sc->sc_ax.ax_av.av_ob = 0;
	}

	return USBD_NORMAL_COMPLETION;
}

static void
auvitek_videobuf_weave(struct auvitek_softc *sc, uint8_t *buf, uint32_t len)
{
	struct auvitek_videobuf *av = &sc->sc_ax.ax_av;
	uint32_t resid, wlen;
	uint32_t *l, *b;
	uint8_t *vp;

	if (sc->sc_ax.ax_frinfo & 0x40) {
		l = &av->av_ol;
		b = &av->av_ob;
		vp = av->av_buf;
	} else {
		l = &av->av_el;
		b = &av->av_eb;
		vp = av->av_buf + av->av_stride;
	}

	resid = len;
	while (resid > 0) {
		if (*b == av->av_stride) {
			*l = *l + 1;
			*b = 0;
		}
		if (*l >= 240) {
			break;
		}
		wlen = min(resid, av->av_stride - *b);
		memcpy(vp + (av->av_stride * 2 * *l) + *b, buf, wlen);
		*b += wlen;
		buf += wlen;
		resid -= wlen;
	}
}

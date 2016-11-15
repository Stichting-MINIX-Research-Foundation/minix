/* $NetBSD: video.c,v 1.32 2014/07/25 08:10:35 dholland Exp $ */

/*
 * Copyright (c) 2008 Patrick Mahoney <pat@polycrystal.org>
 * All rights reserved.
 *
 * This code was written by Patrick Mahoney (pat@polycrystal.org) as
 * part of Google Summer of Code 2008.
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
 * This ia a Video4Linux 2 compatible /dev/video driver for NetBSD
 *
 * See http://v4l2spec.bytesex.org/ for Video4Linux 2 specifications
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: video.c,v 1.32 2014/07/25 08:10:35 dholland Exp $");

#include "video.h"
#if NVIDEO > 0

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/kmem.h>
#include <sys/pool.h>
#include <sys/conf.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/condvar.h>
#include <sys/queue.h>
#include <sys/videoio.h>

#include <dev/video_if.h>

/* #define VIDEO_DEBUG 1 */

#ifdef VIDEO_DEBUG
#define	DPRINTF(x)	do { if (videodebug) printf x; } while (0)
#define	DPRINTFN(n,x)	do { if (videodebug>(n)) printf x; } while (0)
int	videodebug = VIDEO_DEBUG;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define PAGE_ALIGN(a)		(((a) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

#define VIDEO_DRIVER_VERSION				\
	(((__NetBSD_Version__ / 100000000) << 16) |	\
	 ((__NetBSD_Version__ / 1000000 % 100) << 8) |	\
	 (__NetBSD_Version__ / 100 % 100))

/* TODO: move to sys/intr.h */
#define IPL_VIDEO	IPL_VM
#define splvideo()	splvm()

#define VIDEO_MIN_BUFS 2
#define VIDEO_MAX_BUFS 32
#define VIDEO_NUM_BUFS 4

/* Scatter Buffer - an array of fixed size (PAGE_SIZE) chunks
 * allocated non-contiguously and functions to get data into and out
 * of the scatter buffer. */
struct scatter_buf {
	pool_cache_t	sb_pool;
	size_t		sb_size;    /* size in bytes */
	size_t		sb_npages;  /* number of pages */
	uint8_t		**sb_page_ary; /* array of page pointers */
};

struct scatter_io {
	struct scatter_buf *sio_buf;
	off_t		sio_offset;
	size_t		sio_resid;
};

static void	scatter_buf_init(struct scatter_buf *);
static void	scatter_buf_destroy(struct scatter_buf *);
static int	scatter_buf_set_size(struct scatter_buf *, size_t);
static paddr_t	scatter_buf_map(struct scatter_buf *, off_t);

static bool	scatter_io_init(struct scatter_buf *, off_t, size_t, struct scatter_io *);
static bool	scatter_io_next(struct scatter_io *, void **, size_t *);
static void	scatter_io_undo(struct scatter_io *, size_t);
static void	scatter_io_copyin(struct scatter_io *, const void *);
/* static void	scatter_io_copyout(struct scatter_io *, void *); */
static int	scatter_io_uiomove(struct scatter_io *, struct uio *);


enum video_stream_method {
	VIDEO_STREAM_METHOD_NONE,
	VIDEO_STREAM_METHOD_READ,
	VIDEO_STREAM_METHOD_MMAP,
	VIDEO_STREAM_METHOD_USERPTR
};

struct video_buffer {
	struct v4l2_buffer		*vb_buf;
	SIMPLEQ_ENTRY(video_buffer)	entries;
};

SIMPLEQ_HEAD(sample_queue, video_buffer);

struct video_stream {
	int			vs_flags; /* flags given to open() */

	struct video_format	vs_format;

	int			vs_frameno; /* toggles between 0 and 1,
					     * or -1 if new */
	uint32_t		vs_sequence; /* absoulte frame/sample number in
					      * sequence, wraps around */
	bool			vs_drop; /* drop payloads from current
					  * frameno? */
	
	enum v4l2_buf_type	vs_type;
	uint8_t			vs_nbufs;
	struct video_buffer	**vs_buf;

	struct scatter_buf	vs_data; /* stores video data for MMAP
					  * and READ */

	/* Video samples may exist in different locations.  Initially,
	 * samples are queued into the ingress queue.  The driver
	 * grabs these in turn and fills them with video data.  Once
	 * filled, they are moved to the egress queue.  Samples are
	 * dequeued either by user with MMAP method or, with READ
	 * method, videoread() works from the fist sample in the
	 * ingress queue without dequeing.  In the first case, the
	 * user re-queues the buffer when finished, and videoread()
	 * does the same when all data has been read.  The sample now
	 * returns to the ingress queue. */
	struct sample_queue	vs_ingress; /* samples under driver control */
	struct sample_queue	vs_egress; /* samples headed for userspace */

	bool			vs_streaming;
	enum video_stream_method vs_method; /* method by which
					     * userspace will read
					     * samples */

	kmutex_t		vs_lock; /* Lock to manipulate queues.
					  * Should also be held when
					  * changing number of
					  * buffers. */
	kcondvar_t		vs_sample_cv; /* signaled on new
					       * ingress sample */
	struct selinfo		vs_sel;

	uint32_t		vs_bytesread; /* bytes read() from current
					       * sample thus far */
};

struct video_softc {
	device_t	sc_dev;
	device_t	hw_dev;	  	 /* Hardware (parent) device */
	void *		hw_softc;	 /* Hardware device private softc */
	const struct video_hw_if *hw_if; /* Hardware interface */

	u_int		sc_open;
	int		sc_refcnt;
	int		sc_opencnt;
	bool		sc_dying;

	struct video_stream sc_stream_in;
};
static int	video_print(void *, const char *);

static int	video_match(device_t, cfdata_t, void *);
static void	video_attach(device_t, device_t, void *);
static int	video_detach(device_t, int);
static int	video_activate(device_t, enum devact);

dev_type_open(videoopen);
dev_type_close(videoclose);
dev_type_read(videoread);
dev_type_write(videowrite);
dev_type_ioctl(videoioctl);
dev_type_poll(videopoll);
dev_type_mmap(videommap);

const struct cdevsw video_cdevsw = {
	.d_open = videoopen,
	.d_close = videoclose,
	.d_read = videoread,
	.d_write = videowrite,
	.d_ioctl = videoioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = videopoll,
	.d_mmap = videommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

#define VIDEOUNIT(n)	(minor(n))

CFATTACH_DECL_NEW(video, sizeof(struct video_softc),
		  video_match, video_attach, video_detach, video_activate);

extern struct cfdriver video_cd;

static const char *	video_pixel_format_str(enum video_pixel_format);

/* convert various values from V4L2 to native values of this driver */
static uint16_t	v4l2id_to_control_id(uint32_t);
static uint32_t control_flags_to_v4l2flags(uint32_t);
static enum v4l2_ctrl_type control_type_to_v4l2type(enum video_control_type);

static void	v4l2_format_to_video_format(const struct v4l2_format *,
					    struct video_format *);
static void	video_format_to_v4l2_format(const struct video_format *,
					    struct v4l2_format *);
static void	v4l2_standard_to_video_standard(v4l2_std_id,
						enum video_standard *);
static void	video_standard_to_v4l2_standard(enum video_standard,
						struct v4l2_standard *);
static void	v4l2_input_to_video_input(const struct v4l2_input *,
					  struct video_input *);
static void	video_input_to_v4l2_input(const struct video_input *,
					  struct v4l2_input *);
static void	v4l2_audio_to_video_audio(const struct v4l2_audio *,
					  struct video_audio *);
static void	video_audio_to_v4l2_audio(const struct video_audio *,
					  struct v4l2_audio *);
static void	v4l2_tuner_to_video_tuner(const struct v4l2_tuner *,
					  struct video_tuner *);
static void	video_tuner_to_v4l2_tuner(const struct video_tuner *,
					  struct v4l2_tuner *);

/* V4L2 api functions, typically called from videoioctl() */
static int	video_enum_format(struct video_softc *, struct v4l2_fmtdesc *);
static int	video_get_format(struct video_softc *,
				 struct v4l2_format *);
static int	video_set_format(struct video_softc *,
				 struct v4l2_format *);
static int	video_try_format(struct video_softc *,
				 struct v4l2_format *);
static int	video_enum_standard(struct video_softc *,
				    struct v4l2_standard *);
static int	video_get_standard(struct video_softc *, v4l2_std_id *);
static int	video_set_standard(struct video_softc *, v4l2_std_id);
static int	video_enum_input(struct video_softc *, struct v4l2_input *);
static int	video_get_input(struct video_softc *, int *);
static int	video_set_input(struct video_softc *, int);
static int	video_enum_audio(struct video_softc *, struct v4l2_audio *);
static int	video_get_audio(struct video_softc *, struct v4l2_audio *);
static int	video_set_audio(struct video_softc *, struct v4l2_audio *);
static int	video_get_tuner(struct video_softc *, struct v4l2_tuner *);
static int	video_set_tuner(struct video_softc *, struct v4l2_tuner *);
static int	video_get_frequency(struct video_softc *,
				    struct v4l2_frequency *);
static int	video_set_frequency(struct video_softc *,
				    struct v4l2_frequency *);
static int	video_query_control(struct video_softc *,
				    struct v4l2_queryctrl *);
static int	video_get_control(struct video_softc *,
				  struct v4l2_control *);
static int	video_set_control(struct video_softc *,
				  const struct v4l2_control *);
static int	video_request_bufs(struct video_softc *,
				   struct v4l2_requestbuffers *);
static int	video_query_buf(struct video_softc *, struct v4l2_buffer *);
static int	video_queue_buf(struct video_softc *, struct v4l2_buffer *);
static int	video_dequeue_buf(struct video_softc *, struct v4l2_buffer *);
static int	video_stream_on(struct video_softc *, enum v4l2_buf_type);
static int	video_stream_off(struct video_softc *, enum v4l2_buf_type);

static struct video_buffer *	video_buffer_alloc(void);
static void			video_buffer_free(struct video_buffer *);


/* functions for video_stream */
static void	video_stream_init(struct video_stream *);
static void	video_stream_fini(struct video_stream *);

static int	video_stream_setup_bufs(struct video_stream *,
					enum video_stream_method,
					uint8_t);
static void	video_stream_teardown_bufs(struct video_stream *);

static int	video_stream_realloc_bufs(struct video_stream *, uint8_t);
#define		video_stream_free_bufs(vs) \
	video_stream_realloc_bufs((vs), 0)

static void	video_stream_enqueue(struct video_stream *,
				     struct video_buffer *);
static struct video_buffer * video_stream_dequeue(struct video_stream *);
static void	video_stream_write(struct video_stream *,
				   const struct video_payload *);
static void	video_stream_sample_done(struct video_stream *);

#ifdef VIDEO_DEBUG
/* debugging */
static const char *	video_ioctl_str(u_long);
#endif

	
static int
video_match(device_t parent, cfdata_t match, void *aux)
{
#ifdef VIDEO_DEBUG
	struct video_attach_args *args;

	args = aux;
	DPRINTF(("video_match: hw=%p\n", args->hw_if));
#endif
	return 1;
}


static void
video_attach(device_t parent, device_t self, void *aux)
{
	struct video_softc *sc;
	struct video_attach_args *args;

	sc = device_private(self);
	args = aux;
	
	sc->sc_dev = self;
	sc->hw_dev = parent;
	sc->hw_if = args->hw_if;
	sc->hw_softc = device_private(parent);

	sc->sc_open = 0;
	sc->sc_refcnt = 0;
	sc->sc_opencnt = 0;
	sc->sc_dying = false;

	video_stream_init(&sc->sc_stream_in);

	aprint_naive("\n");
	aprint_normal(": %s\n", sc->hw_if->get_devname(sc->hw_softc));

	DPRINTF(("video_attach: sc=%p hwif=%p\n", sc, sc->hw_if));

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
}


static int
video_activate(device_t self, enum devact act)
{
	struct video_softc *sc = device_private(self);

	DPRINTF(("video_activate: sc=%p\n", sc));
	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = true;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}


static int
video_detach(device_t self, int flags)
{
	struct video_softc *sc;
	int maj, mn;

	sc = device_private(self);
	DPRINTF(("video_detach: sc=%p flags=%d\n", sc, flags));

	sc->sc_dying = true;

	pmf_device_deregister(self);
	
	maj = cdevsw_lookup_major(&video_cdevsw);
	mn = device_unit(self);
	/* close open instances */
	vdevgone(maj, mn, mn, VCHR);

	video_stream_fini(&sc->sc_stream_in);

	return 0;
}


static int
video_print(void *aux, const char *pnp)
{
	if (pnp != NULL) {
		DPRINTF(("video_print: have pnp\n"));
		aprint_normal("%s at %s\n", "video", pnp);
	} else {
		DPRINTF(("video_print: pnp is NULL\n"));
	}
	return UNCONF;
}


/*
 * Called from hardware driver.  This is where the MI audio driver
 * gets probed/attached to the hardware driver.
 */
device_t
video_attach_mi(const struct video_hw_if *hw_if, device_t parent)
{
	struct video_attach_args args;

	args.hw_if = hw_if;
	return config_found_ia(parent, "videobus", &args, video_print);
}

/* video_submit_payload - called by hardware driver to submit payload data */
void
video_submit_payload(device_t self, const struct video_payload *payload)
{
	struct video_softc *sc;

	sc = device_private(self);

	if (sc == NULL)
		return;

	video_stream_write(&sc->sc_stream_in, payload);
}

static const char *
video_pixel_format_str(enum video_pixel_format px)
{
	switch (px) {
	case VIDEO_FORMAT_UYVY:		return "UYVY";
	case VIDEO_FORMAT_YUV420:	return "YUV420";
	case VIDEO_FORMAT_YUY2: 	return "YUYV";
	case VIDEO_FORMAT_NV12:		return "NV12";
	case VIDEO_FORMAT_RGB24:	return "RGB24";
	case VIDEO_FORMAT_RGB555:	return "RGB555";
	case VIDEO_FORMAT_RGB565:	return "RGB565";
	case VIDEO_FORMAT_SBGGR8:	return "SBGGR8";
	case VIDEO_FORMAT_MJPEG:	return "MJPEG";
	case VIDEO_FORMAT_DV:		return "DV";
	case VIDEO_FORMAT_MPEG:		return "MPEG";
	default:			return "Unknown";
	}
}

/* Takes a V4L2 id and returns a "native" video driver control id.
 * TODO: is there a better way to do this?  some kind of array? */
static uint16_t
v4l2id_to_control_id(uint32_t v4l2id)
{
	/* mask includes class bits and control id bits */
	switch (v4l2id & 0xffffff) {
	case V4L2_CID_BRIGHTNESS:	return VIDEO_CONTROL_BRIGHTNESS;
	case V4L2_CID_CONTRAST:		return VIDEO_CONTROL_CONTRAST;
	case V4L2_CID_SATURATION:	return VIDEO_CONTROL_SATURATION;
	case V4L2_CID_HUE:		return VIDEO_CONTROL_HUE;
	case V4L2_CID_HUE_AUTO:		return VIDEO_CONTROL_HUE_AUTO;
	case V4L2_CID_SHARPNESS:	return VIDEO_CONTROL_SHARPNESS;
	case V4L2_CID_GAMMA:		return VIDEO_CONTROL_GAMMA;

	/* "black level" means the same as "brightness", but V4L2
	 * defines two separate controls that are not identical.
	 * V4L2_CID_BLACK_LEVEL is deprecated however in V4L2. */
	case V4L2_CID_BLACK_LEVEL:	return VIDEO_CONTROL_BRIGHTNESS;

	case V4L2_CID_AUDIO_VOLUME:	return VIDEO_CONTROL_UNDEFINED;
	case V4L2_CID_AUDIO_BALANCE:	return VIDEO_CONTROL_UNDEFINED;
	case V4L2_CID_AUDIO_BASS:	return VIDEO_CONTROL_UNDEFINED;
	case V4L2_CID_AUDIO_TREBLE:	return VIDEO_CONTROL_UNDEFINED;
	case V4L2_CID_AUDIO_MUTE:	return VIDEO_CONTROL_UNDEFINED;
	case V4L2_CID_AUDIO_LOUDNESS:	return VIDEO_CONTROL_UNDEFINED;
		
	case V4L2_CID_AUTO_WHITE_BALANCE:
		return VIDEO_CONTROL_WHITE_BALANCE_AUTO;
	case V4L2_CID_DO_WHITE_BALANCE:
		return VIDEO_CONTROL_WHITE_BALANCE_ACTION;
	case V4L2_CID_RED_BALANCE:
	case V4L2_CID_BLUE_BALANCE:
		/* This might not fit in with the control_id/value_id scheme */
		return VIDEO_CONTROL_WHITE_BALANCE_COMPONENT;
	case V4L2_CID_WHITE_BALANCE_TEMPERATURE:
		return VIDEO_CONTROL_WHITE_BALANCE_TEMPERATURE;
	case V4L2_CID_EXPOSURE:
		return VIDEO_CONTROL_EXPOSURE_TIME_ABSOLUTE;
	case V4L2_CID_GAIN:		return VIDEO_CONTROL_GAIN;
	case V4L2_CID_AUTOGAIN:		return VIDEO_CONTROL_GAIN_AUTO;
	case V4L2_CID_HFLIP:		return VIDEO_CONTROL_HFLIP;
	case V4L2_CID_VFLIP:		return VIDEO_CONTROL_VFLIP;
	case V4L2_CID_HCENTER_DEPRECATED:
	case V4L2_CID_VCENTER_DEPRECATED:
		return VIDEO_CONTROL_UNDEFINED;
	case V4L2_CID_POWER_LINE_FREQUENCY:
		return VIDEO_CONTROL_POWER_LINE_FREQUENCY;
	case V4L2_CID_BACKLIGHT_COMPENSATION:
		return VIDEO_CONTROL_BACKLIGHT_COMPENSATION;
	default:			return V4L2_CTRL_ID2CID(v4l2id);
	}
}


static uint32_t
control_flags_to_v4l2flags(uint32_t flags)
{
	uint32_t v4l2flags = 0;

	if (flags & VIDEO_CONTROL_FLAG_DISABLED)
		v4l2flags |= V4L2_CTRL_FLAG_INACTIVE;

	if (!(flags & VIDEO_CONTROL_FLAG_WRITE))
		v4l2flags |= V4L2_CTRL_FLAG_READ_ONLY;

	if (flags & VIDEO_CONTROL_FLAG_AUTOUPDATE)
		v4l2flags |= V4L2_CTRL_FLAG_GRABBED;

	return v4l2flags;
}


static enum v4l2_ctrl_type
control_type_to_v4l2type(enum video_control_type type) {
	switch (type) {
	case VIDEO_CONTROL_TYPE_INT:	return V4L2_CTRL_TYPE_INTEGER;
	case VIDEO_CONTROL_TYPE_BOOL:	return V4L2_CTRL_TYPE_BOOLEAN;
	case VIDEO_CONTROL_TYPE_LIST:	return V4L2_CTRL_TYPE_MENU;
	case VIDEO_CONTROL_TYPE_ACTION:	return V4L2_CTRL_TYPE_BUTTON;
	default:			return V4L2_CTRL_TYPE_INTEGER; /* err? */
	}
}


static int
video_query_control(struct video_softc *sc,
		    struct v4l2_queryctrl *query)
{
	const struct video_hw_if *hw;
	struct video_control_desc_group desc_group;
	struct video_control_desc desc;
	int err;

	hw = sc->hw_if;
	if (hw->get_control_desc_group) {
		desc.group_id = desc.control_id =
		    v4l2id_to_control_id(query->id);

		desc_group.group_id = desc.group_id;
		desc_group.length = 1;
		desc_group.desc = &desc;
		
		err = hw->get_control_desc_group(sc->hw_softc, &desc_group);
		if (err != 0)
			return err;

		query->type = control_type_to_v4l2type(desc.type);
		memcpy(query->name, desc.name, 32);
		query->minimum = desc.min;
		query->maximum = desc.max;
		query->step = desc.step;
		query->default_value = desc.def;
		query->flags = control_flags_to_v4l2flags(desc.flags);

		return 0;
	} else {
		return EINVAL;
	}
}


/* Takes a single Video4Linux2 control and queries the driver for the
 * current value. */
static int
video_get_control(struct video_softc *sc,
		  struct v4l2_control *vcontrol)
{
	const struct video_hw_if *hw;
	struct video_control_group group;
	struct video_control control;
	int err;

	hw = sc->hw_if;
	if (hw->get_control_group) {
		control.group_id = control.control_id =
		    v4l2id_to_control_id(vcontrol->id);
		/* ?? if "control_id" is arbitrarily defined by the
		 * driver, then we need some way to store it...  Maybe
		 * it doesn't matter for single value controls. */
		control.value = 0;

		group.group_id = control.group_id;
		group.length = 1;
		group.control = &control;

		err = hw->get_control_group(sc->hw_softc, &group);
		if (err != 0)
			return err;
		
		vcontrol->value = control.value;
		return 0;
	} else {
		return EINVAL;
	}
}

static void
video_format_to_v4l2_format(const struct video_format *src,
			    struct v4l2_format *dest)
{
	/* TODO: what about win and vbi formats? */
	dest->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	dest->fmt.pix.width = src->width;
	dest->fmt.pix.height = src->height;
	if (VIDEO_INTERLACED(src->interlace_flags))
		dest->fmt.pix.field = V4L2_FIELD_INTERLACED;
	else
		dest->fmt.pix.field = V4L2_FIELD_NONE;
	dest->fmt.pix.bytesperline = src->stride;
	dest->fmt.pix.sizeimage = src->sample_size;
	dest->fmt.pix.priv = src->priv;
	
	switch (src->color.primaries) {
	case VIDEO_COLOR_PRIMARIES_SMPTE_170M:
		dest->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
		break;
	/* XXX */
	case VIDEO_COLOR_PRIMARIES_UNSPECIFIED:
	default:
		dest->fmt.pix.colorspace = 0;
		break;
	}

	switch (src->pixel_format) {
	case VIDEO_FORMAT_UYVY:
		dest->fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
		break;
	case VIDEO_FORMAT_YUV420:
		dest->fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
		break;
	case VIDEO_FORMAT_YUY2:
		dest->fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
		break;
	case VIDEO_FORMAT_NV12:
		dest->fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
		break;
	case VIDEO_FORMAT_RGB24:
		dest->fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
		break;
	case VIDEO_FORMAT_RGB555:
		dest->fmt.pix.pixelformat = V4L2_PIX_FMT_RGB555;
		break;
	case VIDEO_FORMAT_RGB565:
		dest->fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
		break;
	case VIDEO_FORMAT_SBGGR8:
		dest->fmt.pix.pixelformat = V4L2_PIX_FMT_SBGGR8;
		break;
	case VIDEO_FORMAT_MJPEG:
		dest->fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
		break;
	case VIDEO_FORMAT_DV:
		dest->fmt.pix.pixelformat = V4L2_PIX_FMT_DV;
		break;
	case VIDEO_FORMAT_MPEG:
		dest->fmt.pix.pixelformat = V4L2_PIX_FMT_MPEG;
		break;
	case VIDEO_FORMAT_UNDEFINED:
	default:
		DPRINTF(("video_get_format: unknown pixel format %d\n",
			 src->pixel_format));
		dest->fmt.pix.pixelformat = 0; /* V4L2 doesn't define
					       * and "undefined"
					       * format? */
		break;
	}

}

static void
v4l2_format_to_video_format(const struct v4l2_format *src,
			    struct video_format *dest)
{
	switch (src->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		dest->width = src->fmt.pix.width;
		dest->height = src->fmt.pix.height;

		dest->stride = src->fmt.pix.bytesperline;
		dest->sample_size = src->fmt.pix.sizeimage;

		if (src->fmt.pix.field == V4L2_FIELD_INTERLACED)
			dest->interlace_flags = VIDEO_INTERLACE_ON;
		else
			dest->interlace_flags = VIDEO_INTERLACE_OFF;

		switch (src->fmt.pix.colorspace) {
		case V4L2_COLORSPACE_SMPTE170M:
			dest->color.primaries =
			    VIDEO_COLOR_PRIMARIES_SMPTE_170M;
			break;
		/* XXX */
		default:
			dest->color.primaries =
			    VIDEO_COLOR_PRIMARIES_UNSPECIFIED;
			break;
		}

		switch (src->fmt.pix.pixelformat) {
		case V4L2_PIX_FMT_UYVY:
			dest->pixel_format = VIDEO_FORMAT_UYVY;
			break;
		case V4L2_PIX_FMT_YUV420:
			dest->pixel_format = VIDEO_FORMAT_YUV420;
			break;
		case V4L2_PIX_FMT_YUYV:
			dest->pixel_format = VIDEO_FORMAT_YUY2;
			break;
		case V4L2_PIX_FMT_NV12:
			dest->pixel_format = VIDEO_FORMAT_NV12;
			break;
		case V4L2_PIX_FMT_RGB24:
			dest->pixel_format = VIDEO_FORMAT_RGB24;
			break;
		case V4L2_PIX_FMT_RGB555:
			dest->pixel_format = VIDEO_FORMAT_RGB555;
			break;
		case V4L2_PIX_FMT_RGB565:
			dest->pixel_format = VIDEO_FORMAT_RGB565;
			break;
		case V4L2_PIX_FMT_SBGGR8:
			dest->pixel_format = VIDEO_FORMAT_SBGGR8;
			break;
		case V4L2_PIX_FMT_MJPEG:
			dest->pixel_format = VIDEO_FORMAT_MJPEG;
			break;
		case V4L2_PIX_FMT_DV:
			dest->pixel_format = VIDEO_FORMAT_DV;
			break;
		case V4L2_PIX_FMT_MPEG:
			dest->pixel_format = VIDEO_FORMAT_MPEG;
			break;
		default:
			DPRINTF(("video: unknown v4l2 pixel format %d\n",
				 src->fmt.pix.pixelformat));
			dest->pixel_format = VIDEO_FORMAT_UNDEFINED;
			break;
		}
		break;
	default:
		/* TODO: other v4l2 format types */
		DPRINTF(("video: unsupported v4l2 format type %d\n",
			 src->type));
		break;
	}
}

static int
video_enum_format(struct video_softc *sc, struct v4l2_fmtdesc *fmtdesc)
{
	const struct video_hw_if *hw;
	struct video_format vfmt;
	struct v4l2_format fmt;
	int err;

	hw = sc->hw_if;
	if (hw->enum_format == NULL)
		return ENOTTY;

	err = hw->enum_format(sc->hw_softc, fmtdesc->index, &vfmt);
	if (err != 0)
		return err;

	video_format_to_v4l2_format(&vfmt, &fmt);

	fmtdesc->type = V4L2_BUF_TYPE_VIDEO_CAPTURE; /* TODO: only one type for now */
	fmtdesc->flags = 0;
	if (vfmt.pixel_format >= VIDEO_FORMAT_MJPEG)
		fmtdesc->flags = V4L2_FMT_FLAG_COMPRESSED;
	strlcpy(fmtdesc->description,
		video_pixel_format_str(vfmt.pixel_format),
		sizeof(fmtdesc->description));
	fmtdesc->pixelformat = fmt.fmt.pix.pixelformat;

	return 0;
}

static int
video_get_format(struct video_softc *sc,
		      struct v4l2_format *format)
{
	const struct video_hw_if *hw;
	struct video_format vfmt;
	int err;

	hw = sc->hw_if;
	if (hw->get_format == NULL)
		return ENOTTY;

	err = hw->get_format(sc->hw_softc, &vfmt);
	if (err != 0)
		return err;

	video_format_to_v4l2_format(&vfmt, format);
	
	return 0;
}

static int
video_set_format(struct video_softc *sc, struct v4l2_format *fmt)
{
	const struct video_hw_if *hw;
	struct video_format vfmt;
	int err;

	hw = sc->hw_if;
	if (hw->set_format == NULL)
		return ENOTTY;

	v4l2_format_to_video_format(fmt, &vfmt);

	err = hw->set_format(sc->hw_softc, &vfmt);
	if (err != 0)
		return err;

	video_format_to_v4l2_format(&vfmt, fmt);
	sc->sc_stream_in.vs_format = vfmt;
	
	return 0;
}


static int
video_try_format(struct video_softc *sc,
		      struct v4l2_format *format)
{
	const struct video_hw_if *hw;
	struct video_format vfmt;
	int err;

	hw = sc->hw_if;
	if (hw->try_format == NULL)
		return ENOTTY;

	v4l2_format_to_video_format(format, &vfmt);

	err = hw->try_format(sc->hw_softc, &vfmt);
	if (err != 0)
		return err;

	video_format_to_v4l2_format(&vfmt, format);

	return 0;
}

static void
v4l2_standard_to_video_standard(v4l2_std_id stdid,
    enum video_standard *vstd)
{
#define VSTD(id, vid)	case (id):	*vstd = (vid); break;
	switch (stdid) {
	VSTD(V4L2_STD_NTSC_M, VIDEO_STANDARD_NTSC_M)
	default:
		*vstd = VIDEO_STANDARD_UNKNOWN;
		break;
	}
#undef VSTD
}

static void
video_standard_to_v4l2_standard(enum video_standard vstd,
    struct v4l2_standard *std)
{
	switch (vstd) {
	case VIDEO_STANDARD_NTSC_M:
		std->id = V4L2_STD_NTSC_M;
		strlcpy(std->name, "NTSC-M", sizeof(std->name));
		std->frameperiod.numerator = 1001;
		std->frameperiod.denominator = 30000;
		std->framelines = 525;
		break;
	default:
		std->id = V4L2_STD_UNKNOWN;
		strlcpy(std->name, "Unknown", sizeof(std->name));
		break;
	}
}

static int
video_enum_standard(struct video_softc *sc, struct v4l2_standard *std)
{
	const struct video_hw_if *hw = sc->hw_if;
	enum video_standard vstd;
	int err;

	/* simple webcam drivers don't need to implement this callback */
	if (hw->enum_standard == NULL) {
		if (std->index != 0)
			return EINVAL;
		std->id = V4L2_STD_UNKNOWN;
		strlcpy(std->name, "webcam", sizeof(std->name));
		return 0;
	}

	v4l2_standard_to_video_standard(std->id, &vstd);

	err = hw->enum_standard(sc->hw_softc, std->index, &vstd);
	if (err != 0)
		return err;

	video_standard_to_v4l2_standard(vstd, std);

	return 0;
}

static int
video_get_standard(struct video_softc *sc, v4l2_std_id *stdid)
{
	const struct video_hw_if *hw = sc->hw_if;
	struct v4l2_standard std;
	enum video_standard vstd;
	int err;

	/* simple webcam drivers don't need to implement this callback */
	if (hw->get_standard == NULL) {
		*stdid = V4L2_STD_UNKNOWN;
		return 0;
	}

	err = hw->get_standard(sc->hw_softc, &vstd);
	if (err != 0)
		return err;

	video_standard_to_v4l2_standard(vstd, &std);
	*stdid = std.id;
	
	return 0;
}

static int
video_set_standard(struct video_softc *sc, v4l2_std_id stdid)
{
	const struct video_hw_if *hw = sc->hw_if;
	enum video_standard vstd;

	/* simple webcam drivers don't need to implement this callback */
	if (hw->set_standard == NULL) {
		if (stdid != V4L2_STD_UNKNOWN)
			return EINVAL;
		return 0;
	}

	v4l2_standard_to_video_standard(stdid, &vstd);

	return hw->set_standard(sc->hw_softc, vstd);
}

static void
v4l2_input_to_video_input(const struct v4l2_input *input,
    struct video_input *vi)
{
	vi->index = input->index;
	strlcpy(vi->name, input->name, sizeof(vi->name));
	switch (input->type) {
	case V4L2_INPUT_TYPE_TUNER:
		vi->type = VIDEO_INPUT_TYPE_TUNER;
		break;
	case V4L2_INPUT_TYPE_CAMERA:
		vi->type = VIDEO_INPUT_TYPE_CAMERA;
		break;
	}
	vi->audiomask = input->audioset;
	vi->tuner_index = input->tuner;
	vi->standards = input->std;	/* ... values are the same */
	vi->status = 0;
	if (input->status & V4L2_IN_ST_NO_POWER)
		vi->status |= VIDEO_STATUS_NO_POWER;
	if (input->status & V4L2_IN_ST_NO_SIGNAL)
		vi->status |= VIDEO_STATUS_NO_SIGNAL;
	if (input->status & V4L2_IN_ST_NO_COLOR)
		vi->status |= VIDEO_STATUS_NO_COLOR;
	if (input->status & V4L2_IN_ST_NO_H_LOCK)
		vi->status |= VIDEO_STATUS_NO_HLOCK;
	if (input->status & V4L2_IN_ST_MACROVISION)
		vi->status |= VIDEO_STATUS_MACROVISION;
}

static void
video_input_to_v4l2_input(const struct video_input *vi,
    struct v4l2_input *input)
{
	input->index = vi->index;
	strlcpy(input->name, vi->name, sizeof(input->name));
	switch (vi->type) {
	case VIDEO_INPUT_TYPE_TUNER:
		input->type = V4L2_INPUT_TYPE_TUNER;
		break;
	case VIDEO_INPUT_TYPE_CAMERA:
		input->type = V4L2_INPUT_TYPE_CAMERA;
		break;
	}
	input->audioset = vi->audiomask;
	input->tuner = vi->tuner_index;
	input->std = vi->standards;	/* ... values are the same */
	input->status = 0;
	if (vi->status & VIDEO_STATUS_NO_POWER)
		input->status |= V4L2_IN_ST_NO_POWER;
	if (vi->status & VIDEO_STATUS_NO_SIGNAL)
		input->status |= V4L2_IN_ST_NO_SIGNAL;
	if (vi->status & VIDEO_STATUS_NO_COLOR)
		input->status |= V4L2_IN_ST_NO_COLOR;
	if (vi->status & VIDEO_STATUS_NO_HLOCK)
		input->status |= V4L2_IN_ST_NO_H_LOCK;
	if (vi->status & VIDEO_STATUS_MACROVISION)
		input->status |= V4L2_IN_ST_MACROVISION;
}

static int
video_enum_input(struct video_softc *sc, struct v4l2_input *input)
{
	const struct video_hw_if *hw = sc->hw_if;
	struct video_input vi;
	int err;

	/* simple webcam drivers don't need to implement this callback */
	if (hw->enum_input == NULL) {
		if (input->index != 0)
			return EINVAL;
		memset(input, 0, sizeof(*input));
		input->index = 0;
		strlcpy(input->name, "Camera", sizeof(input->name));
		input->type = V4L2_INPUT_TYPE_CAMERA;
		return 0;
	}

	v4l2_input_to_video_input(input, &vi);

	err = hw->enum_input(sc->hw_softc, input->index, &vi);
	if (err != 0)
		return err;

	video_input_to_v4l2_input(&vi, input);

	return 0;
}

static int
video_get_input(struct video_softc *sc, int *index)
{
	const struct video_hw_if *hw = sc->hw_if;
	struct video_input vi;
	struct v4l2_input input;
	int err;

	/* simple webcam drivers don't need to implement this callback */
	if (hw->get_input == NULL) {
		*index = 0;
		return 0;
	}

	input.index = *index;
	v4l2_input_to_video_input(&input, &vi);

	err = hw->get_input(sc->hw_softc, &vi);
	if (err != 0)
		return err;

	video_input_to_v4l2_input(&vi, &input);
	*index = input.index;

	return 0;
}

static int
video_set_input(struct video_softc *sc, int index)
{
	const struct video_hw_if *hw = sc->hw_if;
	struct video_input vi;
	struct v4l2_input input;

	/* simple webcam drivers don't need to implement this callback */
	if (hw->set_input == NULL) {
		if (index != 0)
			return EINVAL;
		return 0;
	}

	input.index = index;
	v4l2_input_to_video_input(&input, &vi);

	return hw->set_input(sc->hw_softc, &vi);
}

static void
v4l2_audio_to_video_audio(const struct v4l2_audio *audio,
    struct video_audio *va)
{
	va->index = audio->index;
	strlcpy(va->name, audio->name, sizeof(va->name));
	va->caps = va->mode = 0;
	if (audio->capability & V4L2_AUDCAP_STEREO)
		va->caps |= VIDEO_AUDIO_F_STEREO;
	if (audio->capability & V4L2_AUDCAP_AVL)
		va->caps |= VIDEO_AUDIO_F_AVL;
	if (audio->mode & V4L2_AUDMODE_AVL)
		va->mode |= VIDEO_AUDIO_F_AVL;
}

static void
video_audio_to_v4l2_audio(const struct video_audio *va,
    struct v4l2_audio *audio)
{
	audio->index = va->index;
	strlcpy(audio->name, va->name, sizeof(audio->name));
	audio->capability = audio->mode = 0;
	if (va->caps & VIDEO_AUDIO_F_STEREO)
		audio->capability |= V4L2_AUDCAP_STEREO;
	if (va->caps & VIDEO_AUDIO_F_AVL)
		audio->capability |= V4L2_AUDCAP_AVL;
	if (va->mode & VIDEO_AUDIO_F_AVL)
		audio->mode |= V4L2_AUDMODE_AVL;
}

static int
video_enum_audio(struct video_softc *sc, struct v4l2_audio *audio)
{
	const struct video_hw_if *hw = sc->hw_if;
	struct video_audio va;
	int err;

	if (hw->enum_audio == NULL)
		return ENOTTY;

	v4l2_audio_to_video_audio(audio, &va);

	err = hw->enum_audio(sc->hw_softc, audio->index, &va);
	if (err != 0)
		return err;

	video_audio_to_v4l2_audio(&va, audio);

	return 0;
}

static int
video_get_audio(struct video_softc *sc, struct v4l2_audio *audio)
{
	const struct video_hw_if *hw = sc->hw_if;
	struct video_audio va;
	int err;

	if (hw->get_audio == NULL)
		return ENOTTY;

	v4l2_audio_to_video_audio(audio, &va);

	err = hw->get_audio(sc->hw_softc, &va);
	if (err != 0)
		return err;

	video_audio_to_v4l2_audio(&va, audio);

	return 0;
}

static int
video_set_audio(struct video_softc *sc, struct v4l2_audio *audio)
{
	const struct video_hw_if *hw = sc->hw_if;
	struct video_audio va;

	if (hw->set_audio == NULL)
		return ENOTTY;

	v4l2_audio_to_video_audio(audio, &va);

	return hw->set_audio(sc->hw_softc, &va);
}

static void
v4l2_tuner_to_video_tuner(const struct v4l2_tuner *tuner,
    struct video_tuner *vt)
{
	vt->index = tuner->index;
	strlcpy(vt->name, tuner->name, sizeof(vt->name));
	vt->freq_lo = tuner->rangelow;
	vt->freq_hi = tuner->rangehigh;
	vt->signal = tuner->signal;
	vt->afc = tuner->afc;
	vt->caps = 0;
	if (tuner->capability & V4L2_TUNER_CAP_STEREO)
		vt->caps |= VIDEO_TUNER_F_STEREO;
	if (tuner->capability & V4L2_TUNER_CAP_LANG1)
		vt->caps |= VIDEO_TUNER_F_LANG1;
	if (tuner->capability & V4L2_TUNER_CAP_LANG2)
		vt->caps |= VIDEO_TUNER_F_LANG2;
	switch (tuner->audmode) {
	case V4L2_TUNER_MODE_MONO:
		vt->mode = VIDEO_TUNER_F_MONO;
		break;
	case V4L2_TUNER_MODE_STEREO:
		vt->mode = VIDEO_TUNER_F_STEREO;
		break;
	case V4L2_TUNER_MODE_LANG1:
		vt->mode = VIDEO_TUNER_F_LANG1;
		break;
	case V4L2_TUNER_MODE_LANG2:
		vt->mode = VIDEO_TUNER_F_LANG2;
		break;
	case V4L2_TUNER_MODE_LANG1_LANG2:
		vt->mode = VIDEO_TUNER_F_LANG1 | VIDEO_TUNER_F_LANG2;
		break;
	}
}

static void
video_tuner_to_v4l2_tuner(const struct video_tuner *vt,
    struct v4l2_tuner *tuner)
{
	tuner->index = vt->index;
	strlcpy(tuner->name, vt->name, sizeof(tuner->name));
	tuner->rangelow = vt->freq_lo;
	tuner->rangehigh = vt->freq_hi;
	tuner->signal = vt->signal;
	tuner->afc = vt->afc;
	tuner->capability = 0;
	if (vt->caps & VIDEO_TUNER_F_STEREO)
		tuner->capability |= V4L2_TUNER_CAP_STEREO;
	if (vt->caps & VIDEO_TUNER_F_LANG1)
		tuner->capability |= V4L2_TUNER_CAP_LANG1;
	if (vt->caps & VIDEO_TUNER_F_LANG2)
		tuner->capability |= V4L2_TUNER_CAP_LANG2;
	switch (vt->mode) {
	case VIDEO_TUNER_F_MONO:
		tuner->audmode = V4L2_TUNER_MODE_MONO;
		break;
	case VIDEO_TUNER_F_STEREO:
		tuner->audmode = V4L2_TUNER_MODE_STEREO;
		break;
	case VIDEO_TUNER_F_LANG1:
		tuner->audmode = V4L2_TUNER_MODE_LANG1;
		break;
	case VIDEO_TUNER_F_LANG2:
		tuner->audmode = V4L2_TUNER_MODE_LANG2;
		break;
	case VIDEO_TUNER_F_LANG1|VIDEO_TUNER_F_LANG2:
		tuner->audmode = V4L2_TUNER_MODE_LANG1_LANG2;
		break;
	}
}

static int
video_get_tuner(struct video_softc *sc, struct v4l2_tuner *tuner)
{
	const struct video_hw_if *hw = sc->hw_if;
	struct video_tuner vt;
	int err;

	if (hw->get_tuner == NULL)
		return ENOTTY;

	v4l2_tuner_to_video_tuner(tuner, &vt);

	err = hw->get_tuner(sc->hw_softc, &vt);
	if (err != 0)
		return err;

	video_tuner_to_v4l2_tuner(&vt, tuner);

	return 0;
}

static int
video_set_tuner(struct video_softc *sc, struct v4l2_tuner *tuner)
{
	const struct video_hw_if *hw = sc->hw_if;
	struct video_tuner vt;

	if (hw->set_tuner == NULL)
		return ENOTTY;

	v4l2_tuner_to_video_tuner(tuner, &vt);

	return hw->set_tuner(sc->hw_softc, &vt);
}

static int
video_get_frequency(struct video_softc *sc, struct v4l2_frequency *freq)
{
	const struct video_hw_if *hw = sc->hw_if;
	struct video_frequency vfreq;
	int err;

	if (hw->get_frequency == NULL)
		return ENOTTY;

	err = hw->get_frequency(sc->hw_softc, &vfreq);
	if (err)
		return err;

	freq->tuner = vfreq.tuner_index;
	freq->type = V4L2_TUNER_ANALOG_TV;
	freq->frequency = vfreq.frequency;

	return 0;
}

static int
video_set_frequency(struct video_softc *sc, struct v4l2_frequency *freq)
{
	const struct video_hw_if *hw = sc->hw_if;
	struct video_frequency vfreq;
	struct video_tuner vt;
	int error;

	if (hw->set_frequency == NULL || hw->get_tuner == NULL)
		return ENOTTY;
	if (freq->type != V4L2_TUNER_ANALOG_TV)
		return EINVAL;

	vt.index = freq->tuner;
	error = hw->get_tuner(sc->hw_softc, &vt);
	if (error)
		return error;

	if (freq->frequency < vt.freq_lo)
		freq->frequency = vt.freq_lo;
	else if (freq->frequency > vt.freq_hi)
		freq->frequency = vt.freq_hi;

	vfreq.tuner_index = freq->tuner;
	vfreq.frequency = freq->frequency;

	return hw->set_frequency(sc->hw_softc, &vfreq);
}

/* Takes a single Video4Linux2 control, converts it to a struct
 * video_control, and calls the hardware driver. */
static int
video_set_control(struct video_softc *sc,
		       const struct v4l2_control *vcontrol)
{
	const struct video_hw_if *hw;
	struct video_control_group group;
	struct video_control control;

	hw = sc->hw_if;
	if (hw->set_control_group) {
		control.group_id = control.control_id =
		    v4l2id_to_control_id(vcontrol->id);
		/* ?? if "control_id" is arbitrarily defined by the
		 * driver, then we need some way to store it...  Maybe
		 * it doesn't matter for single value controls. */
		control.value = vcontrol->value;

		group.group_id = control.group_id;
		group.length = 1;
		group.control = &control;
		
		return (hw->set_control_group(sc->hw_softc, &group));
	} else {
		return EINVAL;
	}
}

static int
video_request_bufs(struct video_softc *sc,
		   struct v4l2_requestbuffers *req)
{
	struct video_stream *vs = &sc->sc_stream_in;
	struct v4l2_buffer *buf;
	int i, err;

	if (req->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return EINVAL;

	vs->vs_type = req->type;

	switch (req->memory) {
	case V4L2_MEMORY_MMAP:
		if (req->count < VIDEO_MIN_BUFS)
			req->count = VIDEO_MIN_BUFS;
		else if (req->count > VIDEO_MAX_BUFS)
			req->count = VIDEO_MAX_BUFS;

		err = video_stream_setup_bufs(vs,
					      VIDEO_STREAM_METHOD_MMAP,
					      req->count);
		if (err != 0)
			return err;

		for (i = 0; i < req->count; ++i) {
			buf = vs->vs_buf[i]->vb_buf;
			buf->memory = V4L2_MEMORY_MMAP;
			buf->flags |= V4L2_BUF_FLAG_MAPPED;
		}
		break;
	case V4L2_MEMORY_USERPTR:
	default:
		return EINVAL;
	}

	return 0;
}

static int
video_query_buf(struct video_softc *sc,
		struct v4l2_buffer *buf)
{
	struct video_stream *vs = &sc->sc_stream_in;

	if (buf->type != vs->vs_type)
		return EINVAL;
	if (buf->index >= vs->vs_nbufs)
		return EINVAL;
	
	memcpy(buf, vs->vs_buf[buf->index]->vb_buf, sizeof(*buf));

	return 0;
}

/* Accept a buffer descriptor from userspace and return the indicated
 * buffer to the driver's queue. */
static int
video_queue_buf(struct video_softc *sc, struct v4l2_buffer *userbuf)
{
	struct video_stream *vs = &sc->sc_stream_in;
	struct video_buffer *vb;
	struct v4l2_buffer *driverbuf;
	
	if (userbuf->type != vs->vs_type) {
		DPRINTF(("video_queue_buf: expected type=%d got type=%d\n",
			 userbuf->type, vs->vs_type));
		return EINVAL;
	}
	if (userbuf->index >= vs->vs_nbufs) {
		DPRINTF(("video_queue_buf: invalid index %d >= %d\n",
			 userbuf->index, vs->vs_nbufs));
		return EINVAL;
	}

	switch (vs->vs_method) {
	case VIDEO_STREAM_METHOD_MMAP:
		if (userbuf->memory != V4L2_MEMORY_MMAP) {
			DPRINTF(("video_queue_buf: invalid memory=%d\n",
				 userbuf->memory));
			return EINVAL;
		}
		
		mutex_enter(&vs->vs_lock);
		
		vb = vs->vs_buf[userbuf->index];
		driverbuf = vb->vb_buf;
		if (driverbuf->flags & V4L2_BUF_FLAG_QUEUED) {
			DPRINTF(("video_queue_buf: buf already queued; "
				 "flags=0x%x\n", driverbuf->flags));
			mutex_exit(&vs->vs_lock);
			return EINVAL;
		}
		video_stream_enqueue(vs, vb);
		memcpy(userbuf, driverbuf, sizeof(*driverbuf));
		
		mutex_exit(&vs->vs_lock);
		break;
	default:
		return EINVAL;
	}

	return 0;
}

/* Dequeue the described buffer from the driver queue, making it
 * available for reading via mmap. */
static int
video_dequeue_buf(struct video_softc *sc, struct v4l2_buffer *buf)
{
	struct video_stream *vs = &sc->sc_stream_in;
	struct video_buffer *vb;
	int err;
	
	if (buf->type != vs->vs_type) {
		aprint_debug_dev(sc->sc_dev,
		    "requested type %d (expected %d)\n",
		    buf->type, vs->vs_type);
		return EINVAL;
	}
	
	switch (vs->vs_method) {
	case VIDEO_STREAM_METHOD_MMAP:
		if (buf->memory != V4L2_MEMORY_MMAP) {
			aprint_debug_dev(sc->sc_dev,
			    "requested memory %d (expected %d)\n",
			    buf->memory, V4L2_MEMORY_MMAP);
			return EINVAL;
		}
		
		mutex_enter(&vs->vs_lock);

		if (vs->vs_flags & O_NONBLOCK) {
			vb = video_stream_dequeue(vs);
			if (vb == NULL) {
				mutex_exit(&vs->vs_lock);
				return EAGAIN;
			}
		} else {
			/* Block until we have sample */
			while ((vb = video_stream_dequeue(vs)) == NULL) {
				if (!vs->vs_streaming) {
					mutex_exit(&vs->vs_lock);
					return EINVAL;
				}
				err = cv_wait_sig(&vs->vs_sample_cv,
						  &vs->vs_lock);
				if (err != 0) {
					mutex_exit(&vs->vs_lock);
					return EINTR;
				}
			}
		}

		memcpy(buf, vb->vb_buf, sizeof(*buf));
		
		mutex_exit(&vs->vs_lock);
		break;
	default:
		aprint_debug_dev(sc->sc_dev, "unknown vs_method %d\n",
		    vs->vs_method);
		return EINVAL;
	}

	return 0;
}

static int
video_stream_on(struct video_softc *sc, enum v4l2_buf_type type)
{
	int err;
	struct video_stream *vs = &sc->sc_stream_in;
	const struct video_hw_if *hw;
	
	if (vs->vs_streaming)
		return 0;
	if (type != vs->vs_type)
		return EINVAL;

	hw = sc->hw_if;
	if (hw == NULL)
		return ENXIO;


	err = hw->start_transfer(sc->hw_softc);
	if (err != 0)
		return err;

	vs->vs_streaming = true;
	return 0;
}

static int
video_stream_off(struct video_softc *sc, enum v4l2_buf_type type)
{
	int err;
	struct video_stream *vs = &sc->sc_stream_in;
	const struct video_hw_if *hw;
	
	if (!vs->vs_streaming)
		return 0;
	if (type != vs->vs_type)
		return EINVAL;

	hw = sc->hw_if;
	if (hw == NULL)
		return ENXIO;

	err = hw->stop_transfer(sc->hw_softc);
	if (err != 0)
		return err;

	vs->vs_frameno = -1;
	vs->vs_sequence = 0;
	vs->vs_streaming = false;
	
	return 0;
}

int
videoopen(dev_t dev, int flags, int ifmt, struct lwp *l)
{
	struct video_softc *sc;
	const struct video_hw_if *hw;
	struct video_stream *vs;
	int err;

	DPRINTF(("videoopen\n"));

	sc = device_private(device_lookup(&video_cd, VIDEOUNIT(dev)));
	if (sc == NULL) {
		DPRINTF(("videoopen: failed to get softc for unit %d\n",
			VIDEOUNIT(dev)));
		return ENXIO;
	}
	
	if (sc->sc_dying) {
		DPRINTF(("videoopen: dying\n"));
		return EIO;
	}

	sc->sc_stream_in.vs_flags = flags;

	DPRINTF(("videoopen: flags=0x%x sc=%p parent=%p\n",
		 flags, sc, sc->hw_dev));

	hw = sc->hw_if;
	if (hw == NULL)
		return ENXIO;

	device_active(sc->sc_dev, DVA_SYSTEM);

	sc->sc_opencnt++;

	if (hw->open != NULL) {
		err = hw->open(sc->hw_softc, flags);
		if (err)
			return err;
	}

	/* set up input stream.  TODO: check flags to determine if
	 * "read" is desired? */
	vs = &sc->sc_stream_in;

	if (hw->get_format != NULL) {
		err = hw->get_format(sc->hw_softc, &vs->vs_format);
		if (err != 0)
			return err;
	}
	return 0;
}


int
videoclose(dev_t dev, int flags, int ifmt, struct lwp *l)
{
	struct video_softc *sc;
	const struct video_hw_if *hw;

	sc = device_private(device_lookup(&video_cd, VIDEOUNIT(dev)));
	if (sc == NULL)
		return ENXIO;

	DPRINTF(("videoclose: sc=%p\n", sc));

	hw = sc->hw_if;
	if (hw == NULL)
		return ENXIO;

	device_active(sc->sc_dev, DVA_SYSTEM);

	video_stream_off(sc, sc->sc_stream_in.vs_type);

	/* ignore error */
	if (hw->close != NULL)
		hw->close(sc->hw_softc);

	video_stream_teardown_bufs(&sc->sc_stream_in);
	
	sc->sc_open = 0;
	sc->sc_opencnt--;
	
	return 0;
}


int
videoread(dev_t dev, struct uio *uio, int ioflag)
{
	struct video_softc *sc;
	struct video_stream *vs;
	struct video_buffer *vb;
	struct scatter_io sio;
	int err;
	size_t len;
	off_t offset;

	sc = device_private(device_lookup(&video_cd, VIDEOUNIT(dev)));
	if (sc == NULL)
		return ENXIO;

	if (sc->sc_dying)
		return EIO;

	vs = &sc->sc_stream_in;

	/* userspace has chosen read() method */
	if (vs->vs_method == VIDEO_STREAM_METHOD_NONE) {
		err = video_stream_setup_bufs(vs,
					      VIDEO_STREAM_METHOD_READ,
					      VIDEO_NUM_BUFS);
		if (err != 0)
			return err;

		err = video_stream_on(sc, vs->vs_type);
		if (err != 0)
			return err;
	} else if (vs->vs_method != VIDEO_STREAM_METHOD_READ) {
		return EBUSY;
	}

	mutex_enter(&vs->vs_lock);
	
retry:
	if (SIMPLEQ_EMPTY(&vs->vs_egress)) {
		if (vs->vs_flags & O_NONBLOCK) {
			mutex_exit(&vs->vs_lock);
			return EAGAIN;
		}
		
		/* Block until we have a sample */
		while (SIMPLEQ_EMPTY(&vs->vs_egress)) {
			err = cv_wait_sig(&vs->vs_sample_cv,
					  &vs->vs_lock);
			if (err != 0) {
				mutex_exit(&vs->vs_lock);
				return EINTR;
			}
		}

		vb = SIMPLEQ_FIRST(&vs->vs_egress);
	} else {
	        vb = SIMPLEQ_FIRST(&vs->vs_egress);
	}

	/* Oops, empty sample buffer. */
	if (vb->vb_buf->bytesused == 0) {
		vb = video_stream_dequeue(vs);
		video_stream_enqueue(vs, vb);
		vs->vs_bytesread = 0;
		goto retry;
	}

	mutex_exit(&vs->vs_lock);
	
	len = min(uio->uio_resid, vb->vb_buf->bytesused - vs->vs_bytesread);
	offset = vb->vb_buf->m.offset + vs->vs_bytesread;

	if (scatter_io_init(&vs->vs_data, offset, len, &sio)) {
		err = scatter_io_uiomove(&sio, uio);
		if (err == EFAULT)
			return EFAULT;
		vs->vs_bytesread += (len - sio.sio_resid);
	} else {
		DPRINTF(("video: invalid read\n"));
	}
	
	/* Move the sample to the ingress queue if everything has
	 * been read */
	if (vs->vs_bytesread >= vb->vb_buf->bytesused) {
		mutex_enter(&vs->vs_lock);
		vb = video_stream_dequeue(vs);
		video_stream_enqueue(vs, vb);
		mutex_exit(&vs->vs_lock);
		
		vs->vs_bytesread = 0;
	}

	return 0;
}


int
videowrite(dev_t dev, struct uio *uio, int ioflag)
{
	return ENXIO;
}


/*
 * Before 64-bit time_t, timeval's tv_sec was 'long'.  Thus on LP64 ports
 * v4l2_buffer is the same size and layout as before.  However it did change
 * on LP32 ports, and we thus handle this difference here for "COMPAT_50".
 */

#ifndef _LP64
static void
buf50tobuf(const void *data, struct v4l2_buffer *buf)
{
	const struct v4l2_buffer50 *b50 = data;

	buf->index = b50->index;
	buf->type = b50->type;
	buf->bytesused = b50->bytesused;
	buf->flags = b50->flags;
	buf->field = b50->field;
	timeval50_to_timeval(&b50->timestamp, &buf->timestamp);
	buf->timecode = b50->timecode;
	buf->sequence = b50->sequence;
	buf->memory = b50->memory;
	buf->m.offset = b50->m.offset;
	/* XXX: Handle userptr */
	buf->length = b50->length;
	buf->input = b50->input;
	buf->reserved = b50->reserved;
}

static void
buftobuf50(void *data, const struct v4l2_buffer *buf)
{
	struct v4l2_buffer50 *b50 = data;

	b50->index = buf->index;
	b50->type = buf->type;
	b50->bytesused = buf->bytesused;
	b50->flags = buf->flags;
	b50->field = buf->field;
	timeval_to_timeval50(&buf->timestamp, &b50->timestamp);
	b50->timecode = buf->timecode;
	b50->sequence = buf->sequence;
	b50->memory = buf->memory;
	b50->m.offset = buf->m.offset;
	/* XXX: Handle userptr */
	b50->length = buf->length;
	b50->input = buf->input;
	b50->reserved = buf->reserved;
}
#endif

int
videoioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct video_softc *sc;
	const struct video_hw_if *hw;
	struct v4l2_capability *cap;
	struct v4l2_fmtdesc *fmtdesc;
	struct v4l2_format *fmt;
	struct v4l2_standard *std;
	struct v4l2_input *input;
	struct v4l2_audio *audio;
	struct v4l2_tuner *tuner;
	struct v4l2_frequency *freq;
	struct v4l2_control *control;
	struct v4l2_queryctrl *query;
	struct v4l2_requestbuffers *reqbufs;
	struct v4l2_buffer *buf;
	v4l2_std_id *stdid;
	enum v4l2_buf_type *typep;
	int *ip;
#ifndef _LP64
	struct v4l2_buffer bufspace;
	int error;
#endif

	sc = device_private(device_lookup(&video_cd, VIDEOUNIT(dev)));

	if (sc->sc_dying)
		return EIO;

	hw = sc->hw_if;
	if (hw == NULL)
		return ENXIO;

	switch (cmd) {
	case VIDIOC_QUERYCAP:
		cap = data;
		memset(cap, 0, sizeof(*cap));
		strlcpy(cap->driver,
			device_cfdriver(sc->hw_dev)->cd_name,
			sizeof(cap->driver));
		strlcpy(cap->card, hw->get_devname(sc->hw_softc),
			sizeof(cap->card));
		strlcpy(cap->bus_info, hw->get_businfo(sc->hw_softc),
			sizeof(cap->bus_info));
		cap->version = VIDEO_DRIVER_VERSION;
		cap->capabilities = 0;
		if (hw->start_transfer != NULL && hw->stop_transfer != NULL)
			cap->capabilities |= V4L2_CAP_VIDEO_CAPTURE |
			    V4L2_CAP_READWRITE | V4L2_CAP_STREAMING;
		if (hw->set_tuner != NULL && hw->get_tuner != NULL)
			cap->capabilities |= V4L2_CAP_TUNER;
		if (hw->set_audio != NULL && hw->get_audio != NULL &&
		    hw->enum_audio != NULL)
			cap->capabilities |= V4L2_CAP_AUDIO;
		return 0;
	case VIDIOC_ENUM_FMT:
		/* TODO: for now, just enumerate one default format */
		fmtdesc = data;
		if (fmtdesc->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
			return EINVAL;
		return video_enum_format(sc, fmtdesc);
	case VIDIOC_G_FMT:
		fmt = data;
		return video_get_format(sc, fmt);
	case VIDIOC_S_FMT:
		fmt = data;
		if ((flag & FWRITE) == 0)
			return EPERM;
		return video_set_format(sc, fmt);
	case VIDIOC_TRY_FMT:
		fmt = data;
		return video_try_format(sc, fmt);
	case VIDIOC_ENUMSTD:
		std = data;
		return video_enum_standard(sc, std);
	case VIDIOC_G_STD:
		stdid = data;
		return video_get_standard(sc, stdid);
	case VIDIOC_S_STD:
		stdid = data;
		if ((flag & FWRITE) == 0)
			return EPERM;
		return video_set_standard(sc, *stdid);
	case VIDIOC_ENUMINPUT:
		input = data;
		return video_enum_input(sc, input);
	case VIDIOC_G_INPUT:
		ip = data;
		return video_get_input(sc, ip);
	case VIDIOC_S_INPUT:
		ip = data;
		if ((flag & FWRITE) == 0)
			return EPERM;
		return video_set_input(sc, *ip);
	case VIDIOC_ENUMAUDIO:
		audio = data;
		return video_enum_audio(sc, audio);
	case VIDIOC_G_AUDIO:
		audio = data;
		return video_get_audio(sc, audio);
	case VIDIOC_S_AUDIO:
		audio = data;
		if ((flag & FWRITE) == 0)
			return EPERM;
		return video_set_audio(sc, audio);
	case VIDIOC_G_TUNER:
		tuner = data;
		return video_get_tuner(sc, tuner);
	case VIDIOC_S_TUNER:
		tuner = data;
		if ((flag & FWRITE) == 0)
			return EPERM;
		return video_set_tuner(sc, tuner);
	case VIDIOC_G_FREQUENCY:
		freq = data;
		return video_get_frequency(sc, freq);
	case VIDIOC_S_FREQUENCY:
		freq = data;
		if ((flag & FWRITE) == 0)
			return EPERM;
		return video_set_frequency(sc, freq);
	case VIDIOC_QUERYCTRL:
		query = data;
		return (video_query_control(sc, query));
	case VIDIOC_G_CTRL:
		control = data;
		return (video_get_control(sc, control));
	case VIDIOC_S_CTRL:
		control = data;
		if ((flag & FWRITE) == 0)
			return EPERM;
		return (video_set_control(sc, control));
	case VIDIOC_REQBUFS:
		reqbufs = data;
		return (video_request_bufs(sc, reqbufs));
	case VIDIOC_QUERYBUF:
		buf = data;
		return video_query_buf(sc, buf);
#ifndef _LP64
	case VIDIOC_QUERYBUF50:
		buf50tobuf(data, buf = &bufspace);
		if ((error = video_query_buf(sc, buf)) != 0)
			return error;
		buftobuf50(data, buf);
		return 0;
#endif
	case VIDIOC_QBUF:
		buf = data;
		return video_queue_buf(sc, buf);
#ifndef _LP64
	case VIDIOC_QBUF50:
		buf50tobuf(data, buf = &bufspace);
		return video_queue_buf(sc, buf);
#endif
	case VIDIOC_DQBUF:
		buf = data;
		return video_dequeue_buf(sc, buf);
#ifndef _LP64
	case VIDIOC_DQBUF50:
		buf50tobuf(data, buf = &bufspace);
		if ((error = video_dequeue_buf(sc, buf)) != 0)
			return error;
		buftobuf50(data, buf);
		return 0;
#endif
	case VIDIOC_STREAMON:
		typep = data;
		return video_stream_on(sc, *typep);
	case VIDIOC_STREAMOFF:
		typep = data;
		return video_stream_off(sc, *typep);
	default:
		DPRINTF(("videoioctl: invalid cmd %s (%lx)\n",
			 video_ioctl_str(cmd), cmd));
		return EINVAL;
	}
}

#ifdef VIDEO_DEBUG
static const char *
video_ioctl_str(u_long cmd)
{
	const char *str;
	
	switch (cmd) {
	case VIDIOC_QUERYCAP:
		str = "VIDIOC_QUERYCAP";
		break;
	case VIDIOC_RESERVED:
		str = "VIDIOC_RESERVED";
		break;
	case VIDIOC_ENUM_FMT:
		str = "VIDIOC_ENUM_FMT";
		break;
	case VIDIOC_G_FMT:
		str = "VIDIOC_G_FMT";
		break;
	case VIDIOC_S_FMT:
		str = "VIDIOC_S_FMT";
		break;
/* 6 and 7 are VIDIOC_[SG]_COMP, which are unsupported */
	case VIDIOC_REQBUFS:
		str = "VIDIOC_REQBUFS";
		break;
	case VIDIOC_QUERYBUF:
		str = "VIDIOC_QUERYBUF";
		break;
#ifndef _LP64
	case VIDIOC_QUERYBUF50:
		str = "VIDIOC_QUERYBUF50";
		break;
#endif
	case VIDIOC_G_FBUF:
		str = "VIDIOC_G_FBUF";
		break;
	case VIDIOC_S_FBUF:
		str = "VIDIOC_S_FBUF";
		break;
	case VIDIOC_OVERLAY:
		str = "VIDIOC_OVERLAY";
		break;
	case VIDIOC_QBUF:
		str = "VIDIOC_QBUF";
		break;
#ifndef _LP64
	case VIDIOC_QBUF50:
		str = "VIDIOC_QBUF50";
		break;
#endif
	case VIDIOC_DQBUF:
		str = "VIDIOC_DQBUF";
		break;
#ifndef _LP64
	case VIDIOC_DQBUF50:
		str = "VIDIOC_DQBUF50";
		break;
#endif
	case VIDIOC_STREAMON:
		str = "VIDIOC_STREAMON";
		break;
	case VIDIOC_STREAMOFF:
		str = "VIDIOC_STREAMOFF";
		break;
	case VIDIOC_G_PARM:
		str = "VIDIOC_G_PARAM";
		break;
	case VIDIOC_S_PARM:
		str = "VIDIOC_S_PARAM";
		break;
	case VIDIOC_G_STD:
		str = "VIDIOC_G_STD";
		break;
	case VIDIOC_S_STD:
		str = "VIDIOC_S_STD";
		break;
	case VIDIOC_ENUMSTD:
		str = "VIDIOC_ENUMSTD";
		break;
	case VIDIOC_ENUMINPUT:
		str = "VIDIOC_ENUMINPUT";
		break;
	case VIDIOC_G_CTRL:
		str = "VIDIOC_G_CTRL";
		break;
	case VIDIOC_S_CTRL:
		str = "VIDIOC_S_CTRL";
		break;
	case VIDIOC_G_TUNER:
		str = "VIDIOC_G_TUNER";
		break;
	case VIDIOC_S_TUNER:
		str = "VIDIOC_S_TUNER";
		break;
	case VIDIOC_G_AUDIO:
		str = "VIDIOC_G_AUDIO";
		break;
	case VIDIOC_S_AUDIO:
		str = "VIDIOC_S_AUDIO";
		break;
	case VIDIOC_QUERYCTRL:
		str = "VIDIOC_QUERYCTRL";
		break;
	case VIDIOC_QUERYMENU:
		str = "VIDIOC_QUERYMENU";
		break;
	case VIDIOC_G_INPUT:
		str = "VIDIOC_G_INPUT";
		break;
	case VIDIOC_S_INPUT:
		str = "VIDIOC_S_INPUT";
		break;
	case VIDIOC_G_OUTPUT:
		str = "VIDIOC_G_OUTPUT";
		break;
	case VIDIOC_S_OUTPUT:
		str = "VIDIOC_S_OUTPUT";
		break;
	case VIDIOC_ENUMOUTPUT:
		str = "VIDIOC_ENUMOUTPUT";
		break;
	case VIDIOC_G_AUDOUT:
		str = "VIDIOC_G_AUDOUT";
		break;
	case VIDIOC_S_AUDOUT:
		str = "VIDIOC_S_AUDOUT";
		break;
	case VIDIOC_G_MODULATOR:
		str = "VIDIOC_G_MODULATOR";
		break;
	case VIDIOC_S_MODULATOR:
		str = "VIDIOC_S_MODULATOR";
		break;
	case VIDIOC_G_FREQUENCY:
		str = "VIDIOC_G_FREQUENCY";
		break;
	case VIDIOC_S_FREQUENCY:
		str = "VIDIOC_S_FREQUENCY";
		break;
	case VIDIOC_CROPCAP:
		str = "VIDIOC_CROPCAP";
		break;
	case VIDIOC_G_CROP:
		str = "VIDIOC_G_CROP";
		break;
	case VIDIOC_S_CROP:
		str = "VIDIOC_S_CROP";
		break;
	case VIDIOC_G_JPEGCOMP:
		str = "VIDIOC_G_JPEGCOMP";
		break;
	case VIDIOC_S_JPEGCOMP:
		str = "VIDIOC_S_JPEGCOMP";
		break;
	case VIDIOC_QUERYSTD:
		str = "VIDIOC_QUERYSTD";
		break;
	case VIDIOC_TRY_FMT:
		str = "VIDIOC_TRY_FMT";
		break;
	case VIDIOC_ENUMAUDIO:
		str = "VIDIOC_ENUMAUDIO";
		break;
	case VIDIOC_ENUMAUDOUT:
		str = "VIDIOC_ENUMAUDOUT";
		break;
	case VIDIOC_G_PRIORITY:
		str = "VIDIOC_G_PRIORITY";
		break;
	case VIDIOC_S_PRIORITY:
		str = "VIDIOC_S_PRIORITY";
		break;
	default:
		str = "unknown";
		break;
	}
	return str;
}
#endif


int
videopoll(dev_t dev, int events, struct lwp *l)
{
	struct video_softc *sc;
	struct video_stream *vs;
	int err, revents = 0;

	sc = device_private(device_lookup(&video_cd, VIDEOUNIT(dev)));
	vs = &sc->sc_stream_in;

	if (sc->sc_dying)
		return (POLLHUP);

	/* userspace has chosen read() method */
	if (vs->vs_method == VIDEO_STREAM_METHOD_NONE) {
		err = video_stream_setup_bufs(vs,
					      VIDEO_STREAM_METHOD_READ,
					      VIDEO_NUM_BUFS);
		if (err != 0)
			return POLLERR;

		err = video_stream_on(sc, vs->vs_type);
		if (err != 0)
			return POLLERR;
	}

	mutex_enter(&vs->vs_lock);
	if (!SIMPLEQ_EMPTY(&sc->sc_stream_in.vs_egress))
		revents |= events & (POLLIN | POLLRDNORM);
	else
		selrecord(l, &vs->vs_sel);
	mutex_exit(&vs->vs_lock);

	return (revents);
}


paddr_t
videommap(dev_t dev, off_t off, int prot)
{
	struct video_softc *sc;
	struct video_stream *vs;
	/* paddr_t pa; */

	sc = device_lookup_private(&video_cd, VIDEOUNIT(dev));
	if (sc->sc_dying)
		return -1;

	vs = &sc->sc_stream_in;
	
	return scatter_buf_map(&vs->vs_data, off);
}


/* Allocates buffers and initizlizes some fields.  The format field
 * must already have been initialized. */
void
video_stream_init(struct video_stream *vs)
{
	vs->vs_method = VIDEO_STREAM_METHOD_NONE;
	vs->vs_flags = 0;
	vs->vs_frameno = -1;
	vs->vs_sequence = 0;
	vs->vs_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vs->vs_nbufs = 0;
	vs->vs_buf = NULL;
	vs->vs_streaming = false;
	
	memset(&vs->vs_format, 0, sizeof(vs->vs_format));
	
	SIMPLEQ_INIT(&vs->vs_ingress);
	SIMPLEQ_INIT(&vs->vs_egress);

	mutex_init(&vs->vs_lock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&vs->vs_sample_cv, "video");
	selinit(&vs->vs_sel);

	scatter_buf_init(&vs->vs_data);
}

void
video_stream_fini(struct video_stream *vs)
{
	/* Sample data in queues has already been freed */
	/* while (SIMPLEQ_FIRST(&vs->vs_ingress) != NULL)
		SIMPLEQ_REMOVE_HEAD(&vs->vs_ingress, entries);
	while (SIMPLEQ_FIRST(&vs->vs_egress) != NULL)
	SIMPLEQ_REMOVE_HEAD(&vs->vs_egress, entries); */
	
	mutex_destroy(&vs->vs_lock);
	cv_destroy(&vs->vs_sample_cv);
	seldestroy(&vs->vs_sel);

	scatter_buf_destroy(&vs->vs_data);
}

static int
video_stream_setup_bufs(struct video_stream *vs,
			enum video_stream_method method,
			uint8_t nbufs)
{
	int i, err;
	
	mutex_enter(&vs->vs_lock);

	/* Ensure that all allocated buffers are queued and not under
	 * userspace control. */
	for (i = 0; i < vs->vs_nbufs; ++i) {
		if (!(vs->vs_buf[i]->vb_buf->flags & V4L2_BUF_FLAG_QUEUED)) {
			mutex_exit(&vs->vs_lock);
			return EBUSY;
		}
	}

	/* Allocate the buffers */
	err = video_stream_realloc_bufs(vs, nbufs);
	if (err != 0) {
		mutex_exit(&vs->vs_lock);
		return err;
	}

	/* Queue up buffers for read method.  Other methods are queued
	 * by VIDIOC_QBUF ioctl. */
	if (method == VIDEO_STREAM_METHOD_READ) {
		for (i = 0; i < nbufs; ++i)
			if (!(vs->vs_buf[i]->vb_buf->flags & V4L2_BUF_FLAG_QUEUED))
				video_stream_enqueue(vs, vs->vs_buf[i]);
	}

	vs->vs_method = method;
	mutex_exit(&vs->vs_lock);
	
	return 0;
}

/* Free all buffer memory in preparation for close().  This should
 * free buffers regardless of errors.  Use video_stream_setup_bufs if
 * you need to check for errors. Streaming should be off before
 * calling this function. */
static void
video_stream_teardown_bufs(struct video_stream *vs)
{
	int err;

	mutex_enter(&vs->vs_lock);

	if (vs->vs_streaming) {
		DPRINTF(("video_stream_teardown_bufs: "
			 "tearing down bufs while streaming\n"));
	}

	/* dequeue all buffers */
	while (SIMPLEQ_FIRST(&vs->vs_ingress) != NULL)
		SIMPLEQ_REMOVE_HEAD(&vs->vs_ingress, entries);
	while (SIMPLEQ_FIRST(&vs->vs_egress) != NULL)
		SIMPLEQ_REMOVE_HEAD(&vs->vs_egress, entries);
	
	err = video_stream_free_bufs(vs);
	if (err != 0) {
		DPRINTF(("video_stream_teardown_bufs: "
			 "error releasing buffers: %d\n",
			 err));
	}
	vs->vs_method = VIDEO_STREAM_METHOD_NONE;

	mutex_exit(&vs->vs_lock);
}

static struct video_buffer *
video_buffer_alloc(void)
{
	struct video_buffer *vb;

	vb = kmem_alloc(sizeof(*vb), KM_SLEEP);
	if (vb == NULL)
		return NULL;

	vb->vb_buf = kmem_alloc(sizeof(*vb->vb_buf), KM_SLEEP);
	if (vb->vb_buf == NULL) {
		kmem_free(vb, sizeof(*vb));
		return NULL;
	}

	return vb;
}

static void
video_buffer_free(struct video_buffer *vb)
{
	kmem_free(vb->vb_buf, sizeof(*vb->vb_buf));
	vb->vb_buf = NULL;
	kmem_free(vb, sizeof(*vb));
}

/* TODO: for userptr method
struct video_buffer *
video_buf_alloc_with_ubuf(struct v4l2_buffer *buf)
{
}

void
video_buffer_free_with_ubuf(struct video_buffer *vb)
{
}
*/

static int
video_stream_realloc_bufs(struct video_stream *vs, uint8_t nbufs)
{
	int i, err;
	uint8_t minnbufs, oldnbufs;
	size_t size;
	off_t offset;
	struct video_buffer **oldbuf;
	struct v4l2_buffer *buf;

	size = PAGE_ALIGN(vs->vs_format.sample_size) * nbufs;
	err = scatter_buf_set_size(&vs->vs_data, size);
	if (err != 0)
		return err;

	oldnbufs = vs->vs_nbufs;
	oldbuf = vs->vs_buf;

	vs->vs_nbufs = nbufs;
	if (nbufs > 0) {
		vs->vs_buf =
		    kmem_alloc(sizeof(struct video_buffer *) * nbufs, KM_SLEEP);
		if (vs->vs_buf == NULL) {
			vs->vs_nbufs = oldnbufs;
			vs->vs_buf = oldbuf;

			return ENOMEM;
		}
	} else {
		vs->vs_buf = NULL;
	}

	minnbufs = min(vs->vs_nbufs, oldnbufs);
	/* copy any bufs that will be reused */
	for (i = 0; i < minnbufs; ++i)
		vs->vs_buf[i] = oldbuf[i];
	/* allocate any necessary new bufs */
	for (; i < vs->vs_nbufs; ++i)
		vs->vs_buf[i] = video_buffer_alloc();
	/* free any bufs no longer used */
	for (; i < oldnbufs; ++i) {
		video_buffer_free(oldbuf[i]);
		oldbuf[i] = NULL;
	}

	/* Free old buffer metadata */
	if (oldbuf != NULL)
		kmem_free(oldbuf, sizeof(struct video_buffer *) * oldnbufs);

	/* initialize bufs */
	offset = 0;
	for (i = 0; i < vs->vs_nbufs; ++i) {
		buf = vs->vs_buf[i]->vb_buf;
		buf->index = i;
		buf->type = vs->vs_type;
		buf->bytesused = 0;
		buf->flags = 0;
		buf->field = 0;
		buf->sequence = 0;
		buf->memory = V4L2_MEMORY_MMAP;
		buf->m.offset = offset;
		buf->length = PAGE_ALIGN(vs->vs_format.sample_size);
		buf->input = 0;
		buf->reserved = 0;

		offset += buf->length;
	}

	return 0;
}

/* Accepts a video_sample into the ingress queue.  Caller must hold
 * the stream lock. */
void
video_stream_enqueue(struct video_stream *vs, struct video_buffer *vb)
{
	if (vb->vb_buf->flags & V4L2_BUF_FLAG_QUEUED) {
		DPRINTF(("video_stream_enqueue: sample already queued\n"));
		return;
	}

	vb->vb_buf->flags |= V4L2_BUF_FLAG_QUEUED;
	vb->vb_buf->flags &= ~V4L2_BUF_FLAG_DONE;

	vb->vb_buf->bytesused = 0;

	SIMPLEQ_INSERT_TAIL(&vs->vs_ingress, vb, entries);
}


/* Removes the head of the egress queue for use by userspace.  Caller
 * must hold the stream lock. */
struct video_buffer *
video_stream_dequeue(struct video_stream *vs)
{
	struct video_buffer *vb;

	if (!SIMPLEQ_EMPTY(&vs->vs_egress)) {
		vb = SIMPLEQ_FIRST(&vs->vs_egress);
		SIMPLEQ_REMOVE_HEAD(&vs->vs_egress, entries);
		vb->vb_buf->flags &= ~V4L2_BUF_FLAG_QUEUED;
		vb->vb_buf->flags |= V4L2_BUF_FLAG_DONE;
		return vb;
	} else {
		return NULL;
	}
}

static void
v4l2buf_set_timestamp(struct v4l2_buffer *buf)
{

	getmicrotime(&buf->timestamp);
}

/*
 * write payload data to the appropriate video sample, possibly moving
 * the sample from ingress to egress queues
 */
void
video_stream_write(struct video_stream *vs,
		   const struct video_payload *payload)
{
	struct video_buffer *vb;
	struct v4l2_buffer *buf;
	struct scatter_io sio;

	mutex_enter(&vs->vs_lock);

	/* change of frameno implies end of current frame */
	if (vs->vs_frameno >= 0 && vs->vs_frameno != payload->frameno)
		video_stream_sample_done(vs);

	vs->vs_frameno = payload->frameno;
	
	if (vs->vs_drop || SIMPLEQ_EMPTY(&vs->vs_ingress)) {
		/* DPRINTF(("video_stream_write: dropping sample %d\n",
		   vs->vs_sequence)); */
		vs->vs_drop = true;
	} else if (payload->size > 0) {
		vb = SIMPLEQ_FIRST(&vs->vs_ingress);
		buf = vb->vb_buf;
		if (!buf->bytesused)
			v4l2buf_set_timestamp(buf);
		if (payload->size > buf->length - buf->bytesused) {
			DPRINTF(("video_stream_write: "
				 "payload would overflow\n"));
		} else if (scatter_io_init(&vs->vs_data,
					   buf->m.offset + buf->bytesused,
					   payload->size,
					   &sio))
		{
			scatter_io_copyin(&sio, payload->data);
			buf->bytesused += (payload->size - sio.sio_resid);
		} else {
			DPRINTF(("video_stream_write: failed to init scatter io "
				 "vb=%p buf=%p "
				 "buf->m.offset=%d buf->bytesused=%u "
				 "payload->size=%zu\n",
				 vb, buf,
				 buf->m.offset, buf->bytesused, payload->size));
		}
	}

	/* if the payload marks it, we can do sample_done() early */
	if (payload->end_of_frame)
		video_stream_sample_done(vs);

	mutex_exit(&vs->vs_lock);
}


/* Moves the head of the ingress queue to the tail of the egress
 * queue, or resets drop status if we were dropping this sample.
 * Caller should hold the stream queue lock. */
void
video_stream_sample_done(struct video_stream *vs)
{
	struct video_buffer *vb;

	if (vs->vs_drop) {
		vs->vs_drop = false;
	} else if (!SIMPLEQ_EMPTY(&vs->vs_ingress)) {
		vb = SIMPLEQ_FIRST(&vs->vs_ingress);
		vb->vb_buf->sequence = vs->vs_sequence;
		SIMPLEQ_REMOVE_HEAD(&vs->vs_ingress, entries);

		SIMPLEQ_INSERT_TAIL(&vs->vs_egress, vb, entries);
		cv_signal(&vs->vs_sample_cv);
		selnotify(&vs->vs_sel, 0, 0);
	} else {
		DPRINTF(("video_stream_sample_done: no sample\n"));
	}

	vs->vs_frameno ^= 1;
	vs->vs_sequence++;
}

/* Check if all buffers are queued, i.e. none are under control of
 * userspace. */
/*
static bool
video_stream_all_queued(struct video_stream *vs)
{
}
*/


static void
scatter_buf_init(struct scatter_buf *sb)
{
	sb->sb_pool = pool_cache_init(PAGE_SIZE, 0, 0, 0,
				      "video", NULL, IPL_VIDEO,
				      NULL, NULL, NULL);
	sb->sb_size = 0;
	sb->sb_npages = 0;
	sb->sb_page_ary = NULL;
}

static void
scatter_buf_destroy(struct scatter_buf *sb)
{
	/* Do we need to return everything to the pool first? */
	scatter_buf_set_size(sb, 0);
	pool_cache_destroy(sb->sb_pool);
	sb->sb_pool = 0;
	sb->sb_npages = 0;
	sb->sb_page_ary = NULL;
}

/* Increase or decrease the size of the buffer */
static int
scatter_buf_set_size(struct scatter_buf *sb, size_t sz)
{
	int i;
	size_t npages, minpages, oldnpages;
	uint8_t **old_ary;

	npages = (sz >> PAGE_SHIFT) + ((sz & PAGE_MASK) > 0);
	
	if (sb->sb_npages == npages) {
		return 0;
	}

	oldnpages = sb->sb_npages;
	old_ary = sb->sb_page_ary;

	sb->sb_npages = npages;
	if (npages > 0) {
		sb->sb_page_ary =
		    kmem_alloc(sizeof(uint8_t *) * npages, KM_SLEEP);
		if (sb->sb_page_ary == NULL) {
			sb->sb_npages = oldnpages;
			sb->sb_page_ary = old_ary;
			return ENOMEM;
		}
	} else {
		sb->sb_page_ary = NULL;
	}

	minpages = min(npages, oldnpages);
	/* copy any pages that will be reused */
	for (i = 0; i < minpages; ++i)
		sb->sb_page_ary[i] = old_ary[i];
	/* allocate any new pages */
	for (; i < npages; ++i) {
		sb->sb_page_ary[i] = pool_cache_get(sb->sb_pool, 0);
		/* TODO: does pool_cache_get return NULL on
		 * ENOMEM?  If so, we need to release or note
		 * the pages with did allocate
		 * successfully. */
		if (sb->sb_page_ary[i] == NULL) {
			DPRINTF(("video: pool_cache_get ENOMEM\n"));
			return ENOMEM;
		}
	}
	/* return any pages no longer needed */
	for (; i < oldnpages; ++i)
		pool_cache_put(sb->sb_pool, old_ary[i]);

	if (old_ary != NULL)
		kmem_free(old_ary, sizeof(uint8_t *) * oldnpages);

	sb->sb_size = sb->sb_npages << PAGE_SHIFT;
	
	return 0;
}


static paddr_t
scatter_buf_map(struct scatter_buf *sb, off_t off)
{
	size_t pg;
	paddr_t pa;
	
	pg = off >> PAGE_SHIFT;

	if (pg >= sb->sb_npages)
		return -1;
	else if (!pmap_extract(pmap_kernel(), (vaddr_t)sb->sb_page_ary[pg], &pa))
		return -1;

	return atop(pa);
}

/* Initialize data for an io operation on a scatter buffer. Returns
 * true if the transfer is valid, or false if out of range. */
static bool
scatter_io_init(struct scatter_buf *sb,
		    off_t off, size_t len,
		    struct scatter_io *sio)
{
	if ((off + len) > sb->sb_size) {
		DPRINTF(("video: scatter_io_init failed: off=%" PRId64
			 " len=%zu sb->sb_size=%zu\n",
			 off, len, sb->sb_size));
		return false;
	}

	sio->sio_buf = sb;
	sio->sio_offset = off;
	sio->sio_resid = len;

	return true;
}

/* Store the pointer and size of the next contiguous segment.  Returns
 * true if the segment is valid, or false if all has been transfered.
 * Does not check for overflow. */
static bool
scatter_io_next(struct scatter_io *sio, void **p, size_t *sz)
{
	size_t pg, pgo;

	if (sio->sio_resid == 0)
		return false;
	
	pg = sio->sio_offset >> PAGE_SHIFT;
	pgo = sio->sio_offset & PAGE_MASK;

	*sz = min(PAGE_SIZE - pgo, sio->sio_resid);
	*p = sio->sio_buf->sb_page_ary[pg] + pgo;

	sio->sio_offset += *sz;
	sio->sio_resid -= *sz;

	return true;
}

/* Semi-undo of a failed segment copy.  Updates the scatter_io
 * struct to the previous values prior to a failed segment copy. */
static void
scatter_io_undo(struct scatter_io *sio, size_t sz)
{
	sio->sio_offset -= sz;
	sio->sio_resid += sz;
}

/* Copy data from src into the scatter_buf as described by io. */
static void
scatter_io_copyin(struct scatter_io *sio, const void *p)
{
	void *dst;
	const uint8_t *src = p;
	size_t sz;

	while(scatter_io_next(sio, &dst, &sz)) {
		memcpy(dst, src, sz);
		src += sz;
	}
}

/* --not used; commented to avoid compiler warnings--
static void
scatter_io_copyout(struct scatter_io *sio, void *p)
{
	void *src;
	uint8_t *dst = p;
	size_t sz;

	while(scatter_io_next(sio, &src, &sz)) {
		memcpy(dst, src, sz);
		dst += sz;
	}
}
*/

/* Performat a series of uiomove calls on a scatter buf.  Returns
 * EFAULT if uiomove EFAULTs on the first segment.  Otherwise, returns
 * an incomplete transfer but with no error. */
static int
scatter_io_uiomove(struct scatter_io *sio, struct uio *uio)
{
	void *p;
	size_t sz;
	bool first = true;
	int err;
	
	while(scatter_io_next(sio, &p, &sz)) {
		err = uiomove(p, sz, uio);
		if (err == EFAULT) {
			scatter_io_undo(sio, sz);
			if (first)
				return EFAULT;
			else
				return 0;
		}
		first = false;
	}

	return 0;
}

#endif /* NVIDEO > 0 */

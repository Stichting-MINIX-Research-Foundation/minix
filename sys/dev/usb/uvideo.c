/*	$NetBSD: uvideo.c,v 1.41 2014/09/12 16:40:38 skrll Exp $	*/

/*
 * Copyright (c) 2008 Patrick Mahoney
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 * USB video specs:
 *      http://www.usb.org/developers/devclass_docs/USB_Video_Class_1_1.zip
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvideo.c,v 1.41 2014/09/12 16:40:38 skrll Exp $");

#ifdef _KERNEL_OPT
#include "opt_usb.h"
#endif

#ifdef _MODULE
#include <sys/module.h>
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
/* #include <sys/malloc.h> */
#include <sys/kmem.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/vnode.h>
#include <sys/poll.h>
#include <sys/queue.h>	/* SLIST */
#include <sys/kthread.h>
#include <sys/bus.h>

#include <sys/videoio.h>
#include <dev/video_if.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/uvideoreg.h>

#define UVIDEO_NXFERS	3
#define PRI_UVIDEO	PRI_BIO

/* #define UVIDEO_DISABLE_MJPEG */

#ifdef UVIDEO_DEBUG
#define DPRINTF(x)	do { if (uvideodebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (uvideodebug>(n)) printf x; } while (0)
int	uvideodebug = 20;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

typedef enum {
	UVIDEO_STATE_CLOSED,
	UVIDEO_STATE_OPENING,
	UVIDEO_STATE_IDLE
} uvideo_state;

struct uvideo_camera_terminal {
	uint16_t	ct_objective_focal_min;
	uint16_t	ct_objective_focal_max;
	uint16_t	ct_ocular_focal_length;
};

struct uvideo_processing_unit {
	uint16_t	pu_max_multiplier; /* digital zoom */
	uint8_t		pu_video_standards;
};

struct uvideo_extension_unit {
	guid_t		xu_guid;
};

/* For simplicity, we consider a Terminal a special case of Unit
 * rather than a separate entity. */
struct uvideo_unit {
	uint8_t		vu_id;
	uint8_t		vu_type;
	uint8_t		vu_dst_id;
	uint8_t		vu_nsrcs;
	union {
		uint8_t	vu_src_id;	/* vu_nsrcs = 1 */
		uint8_t	*vu_src_id_ary; /* vu_nsrcs > 1 */
	} s;

	/* fields for individual unit/terminal types */
	union {
		struct uvideo_camera_terminal	vu_camera;
		struct uvideo_processing_unit	vu_processing;
		struct uvideo_extension_unit	vu_extension;
	} u;

	/* Used by camera terminal, processing and extention units. */
	uint8_t		vu_control_size; /* number of bytes in vu_controls */
	uint8_t		*vu_controls;	 /* array of bytes. bits are
					  * numbered from 0 at least
					  * significant bit to
					  * (8*vu_control_size - 1)*/
};

struct uvideo_alternate {
	uint8_t		altno;
	uint8_t		interval;
	uint16_t	max_packet_size;
	SLIST_ENTRY(uvideo_alternate)	entries;
};
SLIST_HEAD(altlist, uvideo_alternate);

#define UVIDEO_FORMAT_GET_FORMAT_INDEX(fmt)	\
	((fmt)->format.priv & 0xff)
#define UVIDEO_FORMAT_GET_FRAME_INDEX(fmt)	\
	(((fmt)->format.priv >> 8) & 0xff)
/* TODO: find a better way to set bytes within this 32 bit value? */
#define UVIDEO_FORMAT_SET_FORMAT_INDEX(fmt, index) do {	\
		(fmt)->format.priv &= ~0xff;		\
		(fmt)->format.priv |= ((index) & 0xff);	\
	} while (0)
#define UVIDEO_FORMAT_SET_FRAME_INDEX(fmt, index) do {			\
		(fmt)->format.priv &= ~(0xff << 8);			\
		((fmt)->format.priv |= (((index) & 0xff) << 8));	\
	} while (0)

struct uvideo_pixel_format {
	enum video_pixel_format	pixel_format;
	SIMPLEQ_ENTRY(uvideo_pixel_format) entries;
};
SIMPLEQ_HEAD(uvideo_pixel_format_list, uvideo_pixel_format);

struct uvideo_format {
	struct video_format	format;
	SIMPLEQ_ENTRY(uvideo_format) entries;
};
SIMPLEQ_HEAD(uvideo_format_list, uvideo_format);

struct uvideo_isoc_xfer;
struct uvideo_stream;

struct uvideo_isoc {
	struct uvideo_isoc_xfer	*i_ix;
	struct uvideo_stream	*i_vs;
	usbd_xfer_handle	i_xfer;
	uint8_t			*i_buf;
	uint16_t		*i_frlengths;
};

struct uvideo_isoc_xfer {
	uint8_t			ix_endpt;
	usbd_pipe_handle	ix_pipe;
	struct uvideo_isoc	ix_i[UVIDEO_NXFERS];
	uint32_t		ix_nframes;
	uint32_t		ix_uframe_len;

	struct altlist		ix_altlist;
};

struct uvideo_bulk_xfer {
	uint8_t			bx_endpt;
	usbd_pipe_handle	bx_pipe;
	usbd_xfer_handle	bx_xfer;
	uint8_t			*bx_buffer;
	int			bx_buflen;
	bool			bx_running;
	kcondvar_t		bx_cv;
	kmutex_t		bx_lock;
};

struct uvideo_stream {
	struct uvideo_softc	*vs_parent;
	usbd_interface_handle	vs_iface;
	uint8_t			vs_ifaceno;
	uint8_t			vs_subtype;  /* input or output */
	uint16_t		vs_probelen; /* length of probe and
					      * commit data; varies
					      * depending on version
					      * of spec. */
	struct uvideo_format_list vs_formats;
	struct uvideo_pixel_format_list vs_pixel_formats;
	struct video_format	*vs_default_format;
	struct video_format	vs_current_format;

	/* usb transfer details */
	uint8_t			vs_xfer_type;
	union {
		struct uvideo_bulk_xfer	bulk;
		struct uvideo_isoc_xfer isoc;
	} vs_xfer;

	int			vs_frameno;	/* toggles between 0 and 1 */

	/* current video format */
	uint32_t		vs_max_payload_size;
	uint32_t		vs_frame_interval;
	SLIST_ENTRY(uvideo_stream) entries;
};
SLIST_HEAD(uvideo_stream_list, uvideo_stream);

struct uvideo_softc {
        device_t   	sc_dev;		/* base device */
        usbd_device_handle      sc_udev;	/* device */
	usbd_interface_handle   sc_iface;	/* interface handle */
        int     		sc_ifaceno;	/* interface number */
	char			*sc_devname;

	device_t		sc_videodev;

	int			sc_dying;
	uvideo_state		sc_state;

	uint8_t			sc_nunits;
	struct uvideo_unit	**sc_unit;

	struct uvideo_stream	*sc_stream_in;

	struct uvideo_stream_list sc_stream_list;

	char			sc_businfo[32];
};

int	uvideo_match(device_t, cfdata_t, void *);
void	uvideo_attach(device_t, device_t, void *);
int	uvideo_detach(device_t, int);
void	uvideo_childdet(device_t, device_t);
int	uvideo_activate(device_t, enum devact);

static int	uvideo_open(void *, int);
static void	uvideo_close(void *);
static const char * uvideo_get_devname(void *);
static const char * uvideo_get_businfo(void *);

static int	uvideo_enum_format(void *, uint32_t, struct video_format *);
static int	uvideo_get_format(void *, struct video_format *);
static int	uvideo_set_format(void *, struct video_format *);
static int	uvideo_try_format(void *, struct video_format *);
static int	uvideo_start_transfer(void *);
static int	uvideo_stop_transfer(void *);

static int	uvideo_get_control_group(void *,
					 struct video_control_group *);
static int	uvideo_set_control_group(void *,
					 const struct video_control_group *);

static usbd_status	uvideo_init_control(
	struct uvideo_softc *,
	const usb_interface_descriptor_t *,
	usbd_desc_iter_t *);
static usbd_status	uvideo_init_collection(
	struct uvideo_softc *,
	const usb_interface_descriptor_t *,
	usbd_desc_iter_t *);

/* Functions for unit & terminal descriptors */
static struct uvideo_unit *	uvideo_unit_alloc(const uvideo_descriptor_t *);
static usbd_status		uvideo_unit_init(struct uvideo_unit *,
						 const uvideo_descriptor_t *);
static void			uvideo_unit_free(struct uvideo_unit *);
static usbd_status		uvideo_unit_alloc_controls(struct uvideo_unit *,
							   uint8_t,
							   const uint8_t *);
static void			uvideo_unit_free_controls(struct uvideo_unit *);
static usbd_status		uvideo_unit_alloc_sources(struct uvideo_unit *,
							  uint8_t,
							  const uint8_t *);
static void			uvideo_unit_free_sources(struct uvideo_unit *);




/* Functions for uvideo_stream, primary unit associated with a video
 * driver or device file. */
static struct uvideo_stream *	uvideo_find_stream(struct uvideo_softc *,
						   uint8_t);
#if 0
static struct uvideo_format *	uvideo_stream_find_format(
	struct uvideo_stream *,
	uint8_t, uint8_t);
#endif
static struct uvideo_format *	uvideo_stream_guess_format(
	struct uvideo_stream *,
	enum video_pixel_format, uint32_t, uint32_t);
static struct uvideo_stream *	uvideo_stream_alloc(void);
static usbd_status		uvideo_stream_init(
	struct uvideo_stream *stream,
	struct uvideo_softc *sc,
	const usb_interface_descriptor_t *ifdesc,
	uint8_t idx);
static usbd_status		uvideo_stream_init_desc(
	struct uvideo_stream *,
	const usb_interface_descriptor_t *ifdesc,
	usbd_desc_iter_t *iter);
static usbd_status		uvideo_stream_init_frame_based_format(
	struct uvideo_stream *,
	const uvideo_descriptor_t *,
	usbd_desc_iter_t *);
static void			uvideo_stream_free(struct uvideo_stream *);

static int		uvideo_stream_start_xfer(struct uvideo_stream *);
static int		uvideo_stream_stop_xfer(struct uvideo_stream *);
static usbd_status	uvideo_stream_recv_process(struct uvideo_stream *,
						   uint8_t *, uint32_t);
static usbd_status	uvideo_stream_recv_isoc_start(struct uvideo_stream *);
static usbd_status	uvideo_stream_recv_isoc_start1(struct uvideo_isoc *);
static void		uvideo_stream_recv_isoc_complete(usbd_xfer_handle,
							 usbd_private_handle,
							 usbd_status);
static void		uvideo_stream_recv_bulk_transfer(void *);

/* format probe and commit */
#define uvideo_stream_probe(vs, act, data)				\
	(uvideo_stream_probe_and_commit((vs), (act),			\
					UVIDEO_VS_PROBE_CONTROL, (data)))
#define uvideo_stream_commit(vs, act, data)				\
	(uvideo_stream_probe_and_commit((vs), (act),			\
					UVIDEO_VS_COMMIT_CONTROL, (data)))
static usbd_status	uvideo_stream_probe_and_commit(struct uvideo_stream *,
						       uint8_t, uint8_t,
						       void *);
static void		uvideo_init_probe_data(uvideo_probe_and_commit_data_t *);


static const usb_descriptor_t * usb_desc_iter_peek_next(usbd_desc_iter_t *);
static const usb_interface_descriptor_t * usb_desc_iter_next_interface(
	usbd_desc_iter_t *);
static const usb_descriptor_t * usb_desc_iter_next_non_interface(
	usbd_desc_iter_t *);

static int	usb_guid_cmp(const usb_guid_t *, const guid_t *);


CFATTACH_DECL2_NEW(uvideo, sizeof(struct uvideo_softc),
    uvideo_match, uvideo_attach, uvideo_detach, uvideo_activate, NULL,
    uvideo_childdet);

extern struct cfdriver uvideo_cd;


static const struct video_hw_if uvideo_hw_if = {
	.open = uvideo_open,
	.close = uvideo_close,
	.get_devname = uvideo_get_devname,
	.get_businfo = uvideo_get_businfo,
	.enum_format = uvideo_enum_format,
	.get_format = uvideo_get_format,
	.set_format = uvideo_set_format,
	.try_format = uvideo_try_format,
	.start_transfer = uvideo_start_transfer,
	.stop_transfer = uvideo_stop_transfer,
	.control_iter_init = NULL,
	.control_iter_next = NULL,
	.get_control_desc_group = NULL,
	.get_control_group = uvideo_get_control_group,
	.set_control_group = uvideo_set_control_group,
};

#ifdef UVIDEO_DEBUG
/* Some functions to print out descriptors.  Mostly useless other than
 * debugging/exploration purposes. */
static void usb_guid_print(const usb_guid_t *);
static void print_descriptor(const usb_descriptor_t *);
static void print_interface_descriptor(const usb_interface_descriptor_t *);
static void print_endpoint_descriptor(const usb_endpoint_descriptor_t *);

static void print_vc_descriptor(const usb_descriptor_t *);
static void print_vs_descriptor(const usb_descriptor_t *);

static void print_vc_header_descriptor(
	const uvideo_vc_header_descriptor_t *);
static void print_input_terminal_descriptor(
	const uvideo_input_terminal_descriptor_t *);
static void print_output_terminal_descriptor(
	const uvideo_output_terminal_descriptor_t *);
static void print_camera_terminal_descriptor(
	const uvideo_camera_terminal_descriptor_t *);
static void print_selector_unit_descriptor(
	const uvideo_selector_unit_descriptor_t *);
static void print_processing_unit_descriptor(
	const uvideo_processing_unit_descriptor_t *);
static void print_extension_unit_descriptor(
	const uvideo_extension_unit_descriptor_t *);
static void print_interrupt_endpoint_descriptor(
	const uvideo_vc_interrupt_endpoint_descriptor_t *);

static void print_vs_input_header_descriptor(
	const uvideo_vs_input_header_descriptor_t *);
static void print_vs_output_header_descriptor(
	const uvideo_vs_output_header_descriptor_t *);

static void print_vs_format_uncompressed_descriptor(
	const uvideo_vs_format_uncompressed_descriptor_t *);
static void print_vs_frame_uncompressed_descriptor(
	const uvideo_vs_frame_uncompressed_descriptor_t *);
static void print_vs_format_mjpeg_descriptor(
	const uvideo_vs_format_mjpeg_descriptor_t *);
static void print_vs_frame_mjpeg_descriptor(
	const uvideo_vs_frame_mjpeg_descriptor_t *);
static void print_vs_format_dv_descriptor(
	const uvideo_vs_format_dv_descriptor_t *);
#endif /* !UVIDEO_DEBUG */

#define GET(type, descp, field) (((const type *)(descp))->field)
#define GETP(type, descp, field) (&(((const type *)(descp))->field))

/* Given a format descriptor and frame descriptor, copy values common
 * to all formats into a struct uvideo_format. */
#define UVIDEO_FORMAT_INIT_FRAME_BASED(format_type, format_desc,	\
				       frame_type, frame_desc,		\
				       format)				\
	do {								\
		UVIDEO_FORMAT_SET_FORMAT_INDEX(				\
			format,						\
			GET(format_type, format_desc, bFormatIndex));	\
		UVIDEO_FORMAT_SET_FRAME_INDEX(				\
			format,						\
			GET(frame_type, frame_desc, bFrameIndex));	\
		format->format.width =					\
		    UGETW(GET(frame_type, frame_desc, wWidth));		\
		format->format.height =					\
		    UGETW(GET(frame_type, frame_desc, wHeight));	\
		format->format.aspect_x =				\
		    GET(format_type, format_desc, bAspectRatioX);	\
		format->format.aspect_y =				\
		    GET(format_type, format_desc, bAspectRatioY);	\
	} while (0)


int
uvideo_match(device_t parent, cfdata_t match, void *aux)
{
	struct usbif_attach_arg *uaa = aux;

        /* TODO: May need to change in the future to work with
         * Interface Association Descriptor. */

	/* Trigger on the Video Control Interface which must be present */
	if (uaa->class == UICLASS_VIDEO &&
	    uaa->subclass == UISUBCLASS_VIDEOCONTROL)
		return UMATCH_IFACECLASS_IFACESUBCLASS;

	return UMATCH_NONE;
}

void
uvideo_attach(device_t parent, device_t self, void *aux)
{
	struct uvideo_softc *sc = device_private(self);
	struct usbif_attach_arg *uaa = aux;
	usbd_desc_iter_t iter;
	const usb_interface_descriptor_t *ifdesc;
	struct uvideo_stream *vs;
	usbd_status err;
	uint8_t ifaceidx;

	sc->sc_dev = self;

	sc->sc_devname = usbd_devinfo_alloc(uaa->device, 0);

	aprint_naive("\n");
	aprint_normal(": %s\n", sc->sc_devname);

	sc->sc_udev = uaa->device;
	sc->sc_iface = uaa->iface;
	sc->sc_ifaceno = uaa->ifaceno;
	sc->sc_dying = 0;
	sc->sc_state = UVIDEO_STATE_CLOSED;
	SLIST_INIT(&sc->sc_stream_list);
	snprintf(sc->sc_businfo, sizeof(sc->sc_businfo), "usb:%08x",
	    sc->sc_udev->cookie.cookie);

#ifdef UVIDEO_DEBUG
	/* Debugging dump of descriptors. TODO: move this to userspace
	 * via a custom IOCTL or something. */
	const usb_descriptor_t *desc;
	usb_desc_iter_init(sc->sc_udev, &iter);
	while ((desc = usb_desc_iter_next(&iter)) != NULL) {
		/* print out all descriptors */
		printf("uvideo_attach: ");
		print_descriptor(desc);
	}
#endif /* !UVIDEO_DEBUG */

	/* iterate through interface descriptors and initialize softc */
	usb_desc_iter_init(sc->sc_udev, &iter);
	for (ifaceidx = 0;
	     (ifdesc = usb_desc_iter_next_interface(&iter)) != NULL;
	     ++ifaceidx)
	{
		if (ifdesc->bInterfaceClass != UICLASS_VIDEO) {
			DPRINTFN(50, ("uvideo_attach: "
				      "ignoring non-uvc interface: "
				      "len=%d type=0x%02x "
				      "class=0x%02x subclass=0x%02x\n",
				      ifdesc->bLength,
				      ifdesc->bDescriptorType,
				      ifdesc->bInterfaceClass,
				      ifdesc->bInterfaceSubClass));
			continue;
		}

		switch (ifdesc->bInterfaceSubClass) {
		case UISUBCLASS_VIDEOCONTROL:
			err = uvideo_init_control(sc, ifdesc, &iter);
			if (err != USBD_NORMAL_COMPLETION) {
				DPRINTF(("uvideo_attach: error with interface "
					 "%d, VideoControl, "
					 "descriptor len=%d type=0x%02x: "
					 "%s (%d)\n",
					 ifdesc->bInterfaceNumber,
					 ifdesc->bLength,
					 ifdesc->bDescriptorType,
					 usbd_errstr(err), err));
			}
			break;
		case UISUBCLASS_VIDEOSTREAMING:
			vs = uvideo_find_stream(sc, ifdesc->bInterfaceNumber);
			if (vs == NULL) {
				vs = uvideo_stream_alloc();
				if (vs == NULL) {
					DPRINTF(("uvideo_attach: "
						 "failed to alloc stream\n"));
					err = USBD_NOMEM;
					goto bad;
				}
				err = uvideo_stream_init(vs, sc, ifdesc,
							 ifaceidx);
				if (err != USBD_NORMAL_COMPLETION) {
					DPRINTF(("uvideo_attach: "
						 "error initializing stream: "
						 "%s (%d)\n",
						 usbd_errstr(err), err));
					goto bad;
				}
			}
			err = uvideo_stream_init_desc(vs, ifdesc, &iter);
			if (err != USBD_NORMAL_COMPLETION) {
				DPRINTF(("uvideo_attach: "
					 "error initializing stream descriptor: "
					 "%s (%d)\n",
					 usbd_errstr(err), err));
				goto bad;
			}
			/* TODO: for now, set (each) stream to stream_in. */
			sc->sc_stream_in = vs;
			break;
		case UISUBCLASS_VIDEOCOLLECTION:
			err = uvideo_init_collection(sc, ifdesc, &iter);
			if (err != USBD_NORMAL_COMPLETION) {
				DPRINTF(("uvideo_attach: error with interface "
				       "%d, VideoCollection, "
				       "descriptor len=%d type=0x%02x: "
				       "%s (%d)\n",
				       ifdesc->bInterfaceNumber,
				       ifdesc->bLength,
				       ifdesc->bDescriptorType,
				       usbd_errstr(err), err));
				goto bad;
			}
			break;
		default:
			DPRINTF(("uvideo_attach: unknown UICLASS_VIDEO "
				 "subclass=0x%02x\n",
				 ifdesc->bInterfaceSubClass));
			break;
		}

	}


	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   sc->sc_dev);

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	sc->sc_videodev = video_attach_mi(&uvideo_hw_if, sc->sc_dev);
	DPRINTF(("uvideo_attach: attached video driver at %p\n",
		 sc->sc_videodev));

	return;

bad:
	if (err != USBD_NORMAL_COMPLETION) {
		DPRINTF(("uvideo_attach: error: %s (%d)\n",
			 usbd_errstr(err), err));
	}
	return;
}


int
uvideo_activate(device_t self, enum devact act)
{
	struct uvideo_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		DPRINTF(("uvideo_activate: deactivating\n"));
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}


/* Detach child (video interface) */
void
uvideo_childdet(device_t self, device_t child)
{
	struct uvideo_softc *sc = device_private(self);

	KASSERT(sc->sc_videodev == child);
	sc->sc_videodev = NULL;
}


int
uvideo_detach(device_t self, int flags)
{
	struct uvideo_softc *sc;
	struct uvideo_stream *vs;
	int rv;

	sc = device_private(self);
	rv = 0;

	sc->sc_dying = 1;

	pmf_device_deregister(self);

	/* TODO: close the device if it is currently opened?  Or will
	 * close be called automatically? */

	while (!SLIST_EMPTY(&sc->sc_stream_list)) {
		vs = SLIST_FIRST(&sc->sc_stream_list);
		SLIST_REMOVE_HEAD(&sc->sc_stream_list, entries);
		uvideo_stream_stop_xfer(vs);
		uvideo_stream_free(vs);
	}

#if 0
	/* Wait for outstanding request to complete.  TODO: what is
	 * appropriate here? */
	usbd_delay_ms(sc->sc_udev, 1000);
#endif

	DPRINTFN(15, ("uvideo: detaching from %s\n",
		device_xname(sc->sc_dev)));

	if (sc->sc_videodev != NULL)
		rv = config_detach(sc->sc_videodev, flags);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
	    sc->sc_dev);

	usbd_devinfo_free(sc->sc_devname);

	return rv;
}

/* Search the stream list for a stream matching the interface number.
 * This is an O(n) search, but most devices should have only one or at
 * most two streams. */
static struct uvideo_stream *
uvideo_find_stream(struct uvideo_softc *sc, uint8_t ifaceno)
{
	struct uvideo_stream *vs;

	SLIST_FOREACH(vs, &sc->sc_stream_list, entries) {
		if (vs->vs_ifaceno == ifaceno)
			return vs;
	}

	return NULL;
}

/* Search the format list for the given format and frame index.  This
 * might be improved through indexing, but the format and frame count
 * is unknown ahead of time (only after iterating through the
 * usb device descriptors). */
#if 0
static struct uvideo_format *
uvideo_stream_find_format(struct uvideo_stream *vs,
			  uint8_t format_index, uint8_t frame_index)
{
	struct uvideo_format *format;

	SIMPLEQ_FOREACH(format, &vs->vs_formats, entries) {
		if (UVIDEO_FORMAT_GET_FORMAT_INDEX(format) == format_index &&
		    UVIDEO_FORMAT_GET_FRAME_INDEX(format) == frame_index)
			return format;
	}
	return NULL;
}
#endif

static struct uvideo_format *
uvideo_stream_guess_format(struct uvideo_stream *vs,
			   enum video_pixel_format pixel_format,
			   uint32_t width, uint32_t height)
{
	struct uvideo_format *format, *gformat = NULL;

	SIMPLEQ_FOREACH(format, &vs->vs_formats, entries) {
		if (format->format.pixel_format != pixel_format)
			continue;
		if (format->format.width <= width &&
		    format->format.height <= height) {
			if (gformat == NULL ||
			    (gformat->format.width < format->format.width &&
			     gformat->format.height < format->format.height))
				gformat = format;
		}
	}

	return gformat;
}

static struct uvideo_stream *
uvideo_stream_alloc(void)
{
	return (kmem_alloc(sizeof(struct uvideo_stream), KM_NOSLEEP));
}


static usbd_status
uvideo_init_control(struct uvideo_softc *sc,
		    const usb_interface_descriptor_t *ifdesc,
		    usbd_desc_iter_t *iter)
{
	const usb_descriptor_t *desc;
	const uvideo_descriptor_t *uvdesc;
	usbd_desc_iter_t orig;
	uint8_t i, j, nunits;

	/* save original iterator state */
	memcpy(&orig, iter, sizeof(orig));

	/* count number of units and terminals */
	nunits = 0;
	while ((desc = usb_desc_iter_next_non_interface(iter)) != NULL) {
		uvdesc = (const uvideo_descriptor_t *)desc;

		if (uvdesc->bDescriptorType != UDESC_CS_INTERFACE)
			continue;
		if (uvdesc->bDescriptorSubtype < UDESC_INPUT_TERMINAL ||
		    uvdesc->bDescriptorSubtype > UDESC_EXTENSION_UNIT)
			continue;
		++nunits;
	}

	if (nunits == 0) {
		DPRINTF(("uvideo_init_control: no units\n"));
		return USBD_NORMAL_COMPLETION;
	}

	i = 0;

	/* allocate space for units */
	sc->sc_nunits = nunits;
	sc->sc_unit = kmem_alloc(sizeof(*sc->sc_unit) * nunits, KM_SLEEP);
	if (sc->sc_unit == NULL)
		goto enomem;

	/* restore original iterator state */
	memcpy(iter, &orig, sizeof(orig));

	/* iterate again, initializing the units */
	while ((desc = usb_desc_iter_next_non_interface(iter)) != NULL) {
		uvdesc = (const uvideo_descriptor_t *)desc;

		if (uvdesc->bDescriptorType != UDESC_CS_INTERFACE)
			continue;
		if (uvdesc->bDescriptorSubtype < UDESC_INPUT_TERMINAL ||
		    uvdesc->bDescriptorSubtype > UDESC_EXTENSION_UNIT)
			continue;

		sc->sc_unit[i] = uvideo_unit_alloc(uvdesc);
		/* TODO: free other units before returning? */
		if (sc->sc_unit[i] == NULL)
			goto enomem;
		++i;
	}

	return USBD_NORMAL_COMPLETION;

enomem:
	if (sc->sc_unit != NULL) {
		for (j = 0; j < i; ++j) {
			uvideo_unit_free(sc->sc_unit[j]);
			sc->sc_unit[j] = NULL;
		}
		kmem_free(sc->sc_unit, sizeof(*sc->sc_unit) * nunits);
		sc->sc_unit = NULL;
	}
	sc->sc_nunits = 0;

	return USBD_NOMEM;
}

static usbd_status
uvideo_init_collection(struct uvideo_softc *sc,
		       const usb_interface_descriptor_t *ifdesc,
		       usbd_desc_iter_t *iter)
{
	DPRINTF(("uvideo: ignoring Video Collection\n"));
	return USBD_NORMAL_COMPLETION;
}

/* Allocates space for and initializes a uvideo unit based on the
 * given descriptor.  Returns NULL with bad descriptor or ENOMEM. */
static struct uvideo_unit *
uvideo_unit_alloc(const uvideo_descriptor_t *desc)
{
	struct uvideo_unit *vu;
	usbd_status err;

	if (desc->bDescriptorType != UDESC_CS_INTERFACE)
		return NULL;

	vu = kmem_alloc(sizeof(*vu), KM_SLEEP);
	if (vu == NULL)
		return NULL;

	err = uvideo_unit_init(vu, desc);
	if (err != USBD_NORMAL_COMPLETION) {
		DPRINTF(("uvideo_unit_alloc: error initializing unit: "
			 "%s (%d)\n", usbd_errstr(err), err));
		kmem_free(vu, sizeof(*vu));
		return NULL;
	}

	return vu;
}

static usbd_status
uvideo_unit_init(struct uvideo_unit *vu, const uvideo_descriptor_t *desc)
{
	struct uvideo_camera_terminal *ct;
	struct uvideo_processing_unit *pu;

	const uvideo_input_terminal_descriptor_t *input;
	const uvideo_camera_terminal_descriptor_t *camera;
	const uvideo_selector_unit_descriptor_t *selector;
	const uvideo_processing_unit_descriptor_t *processing;
	const uvideo_extension_unit_descriptor_t *extension;

	memset(vu, 0, sizeof(*vu));

	switch (desc->bDescriptorSubtype) {
	case UDESC_INPUT_TERMINAL:
		input = (const uvideo_input_terminal_descriptor_t *)desc;
		switch (UGETW(input->wTerminalType)) {
		case UVIDEO_ITT_CAMERA:
			camera =
			    (const uvideo_camera_terminal_descriptor_t *)desc;
			ct = &vu->u.vu_camera;

			ct->ct_objective_focal_min =
			    UGETW(camera->wObjectiveFocalLengthMin);
			ct->ct_objective_focal_max =
			    UGETW(camera->wObjectiveFocalLengthMax);
			ct->ct_ocular_focal_length =
			    UGETW(camera->wOcularFocalLength);

			uvideo_unit_alloc_controls(vu, camera->bControlSize,
						   camera->bmControls);
			break;
		default:
			DPRINTF(("uvideo_unit_init: "
				 "unknown input terminal type 0x%04x\n",
				 UGETW(input->wTerminalType)));
			return USBD_INVAL;
		}
		break;
	case UDESC_OUTPUT_TERMINAL:
		break;
	case UDESC_SELECTOR_UNIT:
		selector = (const uvideo_selector_unit_descriptor_t *)desc;

		uvideo_unit_alloc_sources(vu, selector->bNrInPins,
					  selector->baSourceID);
		break;
	case UDESC_PROCESSING_UNIT:
		processing = (const uvideo_processing_unit_descriptor_t *)desc;
		pu = &vu->u.vu_processing;

		pu->pu_video_standards = PU_GET_VIDEO_STANDARDS(processing);
		pu->pu_max_multiplier = UGETW(processing->wMaxMultiplier);

		uvideo_unit_alloc_sources(vu, 1, &processing->bSourceID);
		uvideo_unit_alloc_controls(vu, processing->bControlSize,
					   processing->bmControls);
		break;
	case UDESC_EXTENSION_UNIT:
		extension = (const uvideo_extension_unit_descriptor_t *)desc;
		/* TODO: copy guid */

		uvideo_unit_alloc_sources(vu, extension->bNrInPins,
					  extension->baSourceID);
		uvideo_unit_alloc_controls(vu, XU_GET_CONTROL_SIZE(extension),
					   XU_GET_CONTROLS(extension));
		break;
	default:
		DPRINTF(("uvideo_unit_alloc: unknown descriptor "
			 "type=0x%02x subtype=0x%02x\n",
			 desc->bDescriptorType, desc->bDescriptorSubtype));
		return USBD_INVAL;
	}

	return USBD_NORMAL_COMPLETION;
}

static void
uvideo_unit_free(struct uvideo_unit *vu)
{
	uvideo_unit_free_sources(vu);
	uvideo_unit_free_controls(vu);
	kmem_free(vu, sizeof(*vu));
}

static usbd_status
uvideo_unit_alloc_sources(struct uvideo_unit *vu,
			  uint8_t nsrcs, const uint8_t *src_ids)
{
	vu->vu_nsrcs = nsrcs;

	if (nsrcs == 0) {
		/* do nothing */
	} else if (nsrcs == 1) {
		vu->s.vu_src_id = src_ids[0];
	} else {
		vu->s.vu_src_id_ary =
		    kmem_alloc(sizeof(*vu->s.vu_src_id_ary) * nsrcs, KM_SLEEP);
		if (vu->s.vu_src_id_ary == NULL) {
			vu->vu_nsrcs = 0;
			return USBD_NOMEM;
		}

		memcpy(vu->s.vu_src_id_ary, src_ids, nsrcs);
	}

	return USBD_NORMAL_COMPLETION;
}

static void
uvideo_unit_free_sources(struct uvideo_unit *vu)
{
	if (vu->vu_nsrcs == 1)
		return;

	kmem_free(vu->s.vu_src_id_ary,
		  sizeof(*vu->s.vu_src_id_ary) * vu->vu_nsrcs);
	vu->vu_nsrcs = 0;
	vu->s.vu_src_id_ary = NULL;
}

static usbd_status
uvideo_unit_alloc_controls(struct uvideo_unit *vu, uint8_t size,
			   const uint8_t *controls)
{
	vu->vu_controls = kmem_alloc(sizeof(*vu->vu_controls) * size, KM_SLEEP);
	if (vu->vu_controls == NULL)
		return USBD_NOMEM;

	vu->vu_control_size = size;
	memcpy(vu->vu_controls, controls, size);

	return USBD_NORMAL_COMPLETION;
}

static void
uvideo_unit_free_controls(struct uvideo_unit *vu)
{
	kmem_free(vu->vu_controls,
		  sizeof(*vu->vu_controls) * vu->vu_control_size);
	vu->vu_controls = NULL;
	vu->vu_control_size = 0;
}


/* Initialize a stream from a Video Streaming interface
 * descriptor. Adds the stream to the stream_list in uvideo_softc.
 * This should be called once for new streams, and
 * uvideo_stream_init_desc() should then be called for this and each
 * additional interface with the same interface number. */
static usbd_status
uvideo_stream_init(struct uvideo_stream *vs,
		   struct uvideo_softc *sc,
		   const usb_interface_descriptor_t *ifdesc,
		   uint8_t idx)
{
	uWord len;
	usbd_status err;

	SLIST_INSERT_HEAD(&sc->sc_stream_list, vs, entries);
	memset(vs, 0, sizeof(*vs));
	vs->vs_parent = sc;
	vs->vs_ifaceno = ifdesc->bInterfaceNumber;
	vs->vs_subtype = 0;
	SIMPLEQ_INIT(&vs->vs_formats);
	SIMPLEQ_INIT(&vs->vs_pixel_formats);
	vs->vs_default_format = NULL;
	vs->vs_current_format.priv = -1;
	vs->vs_xfer_type = 0;

	err = usbd_device2interface_handle(sc->sc_udev, idx, &vs->vs_iface);
	if (err != USBD_NORMAL_COMPLETION) {
		DPRINTF(("uvideo_stream_init: "
			 "error getting vs interface: "
			 "%s (%d)\n",
			 usbd_errstr(err), err));
		return err;
	}

	/* For Xbox Live Vision camera, linux-uvc folk say we need to
	 * set an alternate interface and wait ~3 seconds prior to
	 * doing the format probe/commit.  We set to alternate
	 * interface 0, which is the default, zero bandwidth
	 * interface.  This should not have adverse affects on other
	 * cameras.  Errors are ignored. */
	err = usbd_set_interface(vs->vs_iface, 0);
	if (err != USBD_NORMAL_COMPLETION) {
		DPRINTF(("uvideo_stream_init: error setting alt interface: "
			 "%s (%d)\n",
			 usbd_errstr(err), err));
	}

	/* Initialize probe and commit data size.  This value is
	 * dependent on the version of the spec the hardware
	 * implements. */
	err = uvideo_stream_probe(vs, UR_GET_LEN, &len);
	if (err != USBD_NORMAL_COMPLETION) {
		DPRINTF(("uvideo_stream_init: "
			 "error getting probe data len: "
			 "%s (%d)\n",
			 usbd_errstr(err), err));
		vs->vs_probelen = 26; /* conservative v1.0 length */
	} else if (UGETW(len) <= sizeof(uvideo_probe_and_commit_data_t)) {
		DPRINTFN(15,("uvideo_stream_init: probelen=%d\n", UGETW(len)));
		vs->vs_probelen = UGETW(len);
	} else {
		DPRINTFN(15,("uvideo_stream_init: device returned invalid probe"
				" len %d, using default\n", UGETW(len)));
		vs->vs_probelen = 26;
	}

	return USBD_NORMAL_COMPLETION;
}

/* Further stream initialization based on a Video Streaming interface
 * descriptor and following descriptors belonging to that interface.
 * Iterates through all descriptors belonging to this particular
 * interface descriptor, modifying the iterator.  This may be called
 * multiple times because there may be several alternate interfaces
 * associated with the same interface number. */
static usbd_status
uvideo_stream_init_desc(struct uvideo_stream *vs,
			const usb_interface_descriptor_t *ifdesc,
			usbd_desc_iter_t *iter)
{
	const usb_descriptor_t *desc;
	const uvideo_descriptor_t *uvdesc;
	struct uvideo_bulk_xfer *bx;
	struct uvideo_isoc_xfer *ix;
	struct uvideo_alternate *alt;
	uint8_t xfer_type, xfer_dir;
	uint8_t bmAttributes, bEndpointAddress;
	int i;

	/* Iterate until the next interface descriptor.  All
	 * descriptors until then belong to this streaming
	 * interface. */
	while ((desc = usb_desc_iter_next_non_interface(iter)) != NULL) {
		uvdesc = (const uvideo_descriptor_t *)desc;

		switch (uvdesc->bDescriptorType) {
		case UDESC_ENDPOINT:
			bmAttributes = GET(usb_endpoint_descriptor_t,
					   desc, bmAttributes);
			bEndpointAddress = GET(usb_endpoint_descriptor_t,
					       desc, bEndpointAddress);
			xfer_type = UE_GET_XFERTYPE(bmAttributes);
			xfer_dir = UE_GET_DIR(bEndpointAddress);
			if (xfer_type == UE_BULK && xfer_dir == UE_DIR_IN) {
				bx = &vs->vs_xfer.bulk;
				if (vs->vs_xfer_type == 0) {
					DPRINTFN(15, ("uvideo_attach: "
						      "BULK stream *\n"));
					vs->vs_xfer_type = UE_BULK;
					bx->bx_endpt = bEndpointAddress;
					DPRINTF(("uvideo_attach: BULK "
						 "endpoint %x\n",
						 bx->bx_endpt));
					bx->bx_running = false;
					cv_init(&bx->bx_cv,
					    device_xname(vs->vs_parent->sc_dev)
					    );
					mutex_init(&bx->bx_lock,
					  MUTEX_DEFAULT, IPL_NONE);
				}
			} else if (xfer_type == UE_ISOCHRONOUS) {
				ix = &vs->vs_xfer.isoc;
				for (i = 0; i < UVIDEO_NXFERS; i++) {
					ix->ix_i[i].i_ix = ix;
					ix->ix_i[i].i_vs = vs;
				}
				if (vs->vs_xfer_type == 0) {
					DPRINTFN(15, ("uvideo_attach: "
						      "ISOC stream *\n"));
					SLIST_INIT(&ix->ix_altlist);
					vs->vs_xfer_type = UE_ISOCHRONOUS;
					ix->ix_endpt =
					    GET(usb_endpoint_descriptor_t,
						desc, bEndpointAddress);
				}

				alt = kmem_alloc(sizeof(*alt), KM_NOSLEEP);
				if (alt == NULL)
					return USBD_NOMEM;

				alt->altno = ifdesc->bAlternateSetting;
				alt->interval =
				    GET(usb_endpoint_descriptor_t,
					desc, bInterval);

				alt->max_packet_size =
				UE_GET_SIZE(UGETW(GET(usb_endpoint_descriptor_t,
					desc, wMaxPacketSize)));
				alt->max_packet_size *=
					(UE_GET_TRANS(UGETW(GET(
						usb_endpoint_descriptor_t, desc,
						wMaxPacketSize)))) + 1;

				SLIST_INSERT_HEAD(&ix->ix_altlist,
						  alt, entries);
			}
			break;
		case UDESC_CS_INTERFACE:
			if (ifdesc->bAlternateSetting != 0) {
				DPRINTF(("uvideo_stream_init_alternate: "
					 "unexpected class-specific descriptor "
					 "len=%d type=0x%02x subtype=0x%02x\n",
					 uvdesc->bLength,
					 uvdesc->bDescriptorType,
					 uvdesc->bDescriptorSubtype));
				break;
			}

			switch (uvdesc->bDescriptorSubtype) {
			case UDESC_VS_INPUT_HEADER:
				vs->vs_subtype = UDESC_VS_INPUT_HEADER;
				break;
			case UDESC_VS_OUTPUT_HEADER:
				/* TODO: handle output stream */
				DPRINTF(("uvideo: VS output not implemented\n"));
				vs->vs_subtype = UDESC_VS_OUTPUT_HEADER;
				return USBD_INVAL;
			case UDESC_VS_FORMAT_UNCOMPRESSED:
			case UDESC_VS_FORMAT_FRAME_BASED:
			case UDESC_VS_FORMAT_MJPEG:
				uvideo_stream_init_frame_based_format(vs,
								      uvdesc,
								      iter);
				break;
			case UDESC_VS_FORMAT_MPEG2TS:
			case UDESC_VS_FORMAT_DV:
			case UDESC_VS_FORMAT_STREAM_BASED:
			default:
				DPRINTF(("uvideo: unimplemented VS CS "
					 "descriptor len=%d type=0x%02x "
					 "subtype=0x%02x\n",
					 uvdesc->bLength,
					 uvdesc->bDescriptorType,
					 uvdesc->bDescriptorSubtype));
				break;
			}
			break;
		default:
			DPRINTF(("uvideo_stream_init_desc: "
				 "unknown descriptor "
				 "len=%d type=0x%02x\n",
				 uvdesc->bLength,
				 uvdesc->bDescriptorType));
			break;
		}
	}

	return USBD_NORMAL_COMPLETION;
}

/* Finialize and free memory associated with this stream. */
static void
uvideo_stream_free(struct uvideo_stream *vs)
{
	struct uvideo_alternate *alt;
	struct uvideo_pixel_format *pixel_format;
	struct uvideo_format *format;

	/* free linked list of alternate interfaces */
	if (vs->vs_xfer_type == UE_ISOCHRONOUS) {
		while (!SLIST_EMPTY(&vs->vs_xfer.isoc.ix_altlist)) {
			alt = SLIST_FIRST(&vs->vs_xfer.isoc.ix_altlist);
			SLIST_REMOVE_HEAD(&vs->vs_xfer.isoc.ix_altlist,
					  entries);
			kmem_free(alt, sizeof(*alt));
		}
	}

	/* free linked-list of formats and pixel formats */
	while ((format = SIMPLEQ_FIRST(&vs->vs_formats)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&vs->vs_formats, entries);
		kmem_free(format, sizeof(struct uvideo_format));
	}
	while ((pixel_format = SIMPLEQ_FIRST(&vs->vs_pixel_formats)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&vs->vs_pixel_formats, entries);
		kmem_free(pixel_format, sizeof(struct uvideo_pixel_format));
	}

	kmem_free(vs, sizeof(*vs));
}


static usbd_status
uvideo_stream_init_frame_based_format(struct uvideo_stream *vs,
				      const uvideo_descriptor_t *format_desc,
				      usbd_desc_iter_t *iter)
{
	struct uvideo_pixel_format *pformat, *pfiter;
	enum video_pixel_format pixel_format;
	struct uvideo_format *format;
	const uvideo_descriptor_t *uvdesc;
	uint8_t subtype, default_index, index;
	uint32_t frame_interval;
	const usb_guid_t *guid;

	pixel_format = VIDEO_FORMAT_UNDEFINED;

	switch (format_desc->bDescriptorSubtype) {
	case UDESC_VS_FORMAT_UNCOMPRESSED:
		subtype = UDESC_VS_FRAME_UNCOMPRESSED;
		default_index = GET(uvideo_vs_format_uncompressed_descriptor_t,
				    format_desc,
				    bDefaultFrameIndex);
		guid = GETP(uvideo_vs_format_uncompressed_descriptor_t,
			    format_desc,
			    guidFormat);
		if (usb_guid_cmp(guid, &uvideo_guid_format_yuy2) == 0)
			pixel_format = VIDEO_FORMAT_YUY2;
		else if (usb_guid_cmp(guid, &uvideo_guid_format_nv12) == 0)
			pixel_format = VIDEO_FORMAT_NV12;
		else if (usb_guid_cmp(guid, &uvideo_guid_format_uyvy) == 0)
			pixel_format = VIDEO_FORMAT_UYVY;
		break;
	case UDESC_VS_FORMAT_FRAME_BASED:
		subtype = UDESC_VS_FRAME_FRAME_BASED;
		default_index = GET(uvideo_format_frame_based_descriptor_t,
				    format_desc,
				    bDefaultFrameIndex);
		break;
	case UDESC_VS_FORMAT_MJPEG:
		subtype = UDESC_VS_FRAME_MJPEG;
		default_index = GET(uvideo_vs_format_mjpeg_descriptor_t,
				    format_desc,
				    bDefaultFrameIndex);
		pixel_format = VIDEO_FORMAT_MJPEG;
		break;
	default:
		DPRINTF(("uvideo: unknown frame based format %d\n",
			 format_desc->bDescriptorSubtype));
		return USBD_INVAL;
	}

	pformat = NULL;
	SIMPLEQ_FOREACH(pfiter, &vs->vs_pixel_formats, entries) {
		if (pfiter->pixel_format == pixel_format) {
			pformat = pfiter;
			break;
		}
	}
	if (pixel_format != VIDEO_FORMAT_UNDEFINED && pformat == NULL) {
		pformat = kmem_zalloc(sizeof(*pformat), KM_SLEEP);
		pformat->pixel_format = pixel_format;
		DPRINTF(("uvideo: Adding pixel format %d\n",
		    pixel_format));
		SIMPLEQ_INSERT_TAIL(&vs->vs_pixel_formats,
		    pformat, entries);
	}

	/* Iterate through frame descriptors directly following the
	 * format descriptor, and add a format to the format list for
	 * each frame descriptor. */
	while ((uvdesc = (const uvideo_descriptor_t *) usb_desc_iter_peek_next(iter)) &&
	       (uvdesc != NULL) && (uvdesc->bDescriptorSubtype == subtype))
	{
		uvdesc = (const uvideo_descriptor_t *) usb_desc_iter_next(iter);

		format = kmem_zalloc(sizeof(struct uvideo_format), KM_SLEEP);
		if (format == NULL) {
			DPRINTF(("uvideo: failed to alloc video format\n"));
			return USBD_NOMEM;
		}

		format->format.pixel_format = pixel_format;

		switch (format_desc->bDescriptorSubtype) {
		case UDESC_VS_FORMAT_UNCOMPRESSED:
#ifdef UVIDEO_DEBUG
			if (pixel_format == VIDEO_FORMAT_UNDEFINED &&
			    uvideodebug) {
				guid = GETP(
				    uvideo_vs_format_uncompressed_descriptor_t,
				    format_desc,
				    guidFormat);

				DPRINTF(("uvideo: format undefined "));
				usb_guid_print(guid);
				DPRINTF(("\n"));
			}
#endif

			UVIDEO_FORMAT_INIT_FRAME_BASED(
				uvideo_vs_format_uncompressed_descriptor_t,
				format_desc,
				uvideo_vs_frame_uncompressed_descriptor_t,
				uvdesc,
				format);
			format->format.sample_size =
			    UGETDW(
			      GET(uvideo_vs_frame_uncompressed_descriptor_t,
			      uvdesc, dwMaxVideoFrameBufferSize));
			format->format.stride =
			    format->format.sample_size / format->format.height;
			index = GET(uvideo_vs_frame_uncompressed_descriptor_t,
				    uvdesc,
				    bFrameIndex);
			frame_interval =
			    UGETDW(
				GET(uvideo_vs_frame_uncompressed_descriptor_t,
				uvdesc,
				dwDefaultFrameInterval));
			break;
		case UDESC_VS_FORMAT_MJPEG:
			UVIDEO_FORMAT_INIT_FRAME_BASED(
				uvideo_vs_format_mjpeg_descriptor_t,
				format_desc,
				uvideo_vs_frame_mjpeg_descriptor_t,
				uvdesc,
				format);
			format->format.sample_size =
			    UGETDW(
				GET(uvideo_vs_frame_mjpeg_descriptor_t,
			        uvdesc, dwMaxVideoFrameBufferSize));
			format->format.stride =
			    format->format.sample_size / format->format.height;
			index = GET(uvideo_vs_frame_mjpeg_descriptor_t,
				    uvdesc,
				    bFrameIndex);
			frame_interval =
			    UGETDW(
				GET(uvideo_vs_frame_mjpeg_descriptor_t,
				uvdesc,
				dwDefaultFrameInterval));
			break;
		case UDESC_VS_FORMAT_FRAME_BASED:
			format->format.pixel_format = VIDEO_FORMAT_UNDEFINED;
			UVIDEO_FORMAT_INIT_FRAME_BASED(
				uvideo_format_frame_based_descriptor_t,
				format_desc,
				uvideo_frame_frame_based_descriptor_t,
				uvdesc,
				format);
			index = GET(uvideo_frame_frame_based_descriptor_t,
				    uvdesc,
				    bFrameIndex);
			format->format.stride =
			    UGETDW(
				GET(uvideo_frame_frame_based_descriptor_t,
			        uvdesc, dwBytesPerLine));
			format->format.sample_size =
			    format->format.stride * format->format.height;
			frame_interval =
			    UGETDW(
				GET(uvideo_frame_frame_based_descriptor_t,
				uvdesc, dwDefaultFrameInterval));
			break;
		default:
			/* shouldn't ever get here */
			DPRINTF(("uvideo: unknown frame based format %d\n",
				 format_desc->bDescriptorSubtype));
			kmem_free(format, sizeof(struct uvideo_format));
			return USBD_INVAL;
		}

		DPRINTF(("uvideo: found format (index %d) type %d "
		    "size %ux%u size %u stride %u interval %u\n",
		    index, format->format.pixel_format, format->format.width,
		    format->format.height, format->format.sample_size,
		    format->format.stride, frame_interval));

		SIMPLEQ_INSERT_TAIL(&vs->vs_formats, format, entries);

		if (vs->vs_default_format == NULL && index == default_index
#ifdef UVIDEO_DISABLE_MJPEG
		    && subtype != UDESC_VS_FRAME_MJPEG
#endif
		    ) {
			DPRINTF((" ^ picking this one\n"));
			vs->vs_default_format = &format->format;
			vs->vs_frame_interval = frame_interval;
		}

	}

	return USBD_NORMAL_COMPLETION;
}

static int
uvideo_stream_start_xfer(struct uvideo_stream *vs)
{
	struct uvideo_softc *sc = vs->vs_parent;
	struct uvideo_bulk_xfer *bx;
	struct uvideo_isoc_xfer *ix;
	uint32_t vframe_len;	/* rough bytes per video frame */
	uint32_t uframe_len;	/* bytes per usb frame (TODO: or microframe?) */
	uint32_t nframes;	/* number of usb frames (TODO: or microframs?) */
	int i, ret;

	struct uvideo_alternate *alt, *alt_maybe;
	usbd_status err;

	switch (vs->vs_xfer_type) {
	case UE_BULK:
		ret = 0;
		bx = &vs->vs_xfer.bulk;

		bx->bx_xfer = usbd_alloc_xfer(sc->sc_udev);
		if (bx->bx_xfer == NULL) {
			DPRINTF(("uvideo: couldn't allocate xfer\n"));
			return ENOMEM;
		}
		DPRINTF(("uvideo: xfer %p\n", bx->bx_xfer));

		bx->bx_buflen = vs->vs_max_payload_size;

		DPRINTF(("uvideo: allocating %u byte buffer\n", bx->bx_buflen));
		bx->bx_buffer = usbd_alloc_buffer(bx->bx_xfer, bx->bx_buflen);

		if (bx->bx_buffer == NULL) {
			DPRINTF(("uvideo: couldn't allocate buffer\n"));
			return ENOMEM;
		}

		err = usbd_open_pipe(vs->vs_iface, bx->bx_endpt, 0,
		    &bx->bx_pipe);
		if (err != USBD_NORMAL_COMPLETION) {
			DPRINTF(("uvideo: error opening pipe: %s (%d)\n",
				 usbd_errstr(err), err));
			return EIO;
		}
		DPRINTF(("uvideo: pipe %p\n", bx->bx_pipe));

		mutex_enter(&bx->bx_lock);
		if (bx->bx_running == false) {
			bx->bx_running = true;
			ret = kthread_create(PRI_UVIDEO, 0, NULL,
			    uvideo_stream_recv_bulk_transfer, vs,
			    NULL, "%s", device_xname(sc->sc_dev));
			if (ret) {
				DPRINTF(("uvideo: couldn't create kthread:"
					 " %d\n", err));
				bx->bx_running = false;
				mutex_exit(&bx->bx_lock);
				return err;
			}
		} else
			aprint_error_dev(sc->sc_dev,
			    "transfer already in progress\n");
		mutex_exit(&bx->bx_lock);

		DPRINTF(("uvideo: thread created\n"));

		return 0;
	case UE_ISOCHRONOUS:
		ix = &vs->vs_xfer.isoc;

		/* Choose an alternate interface most suitable for
		 * this format.  Choose the smallest size that can
		 * contain max_payload_size.
		 *
		 * It is assumed that the list is sorted in descending
		 * order from largest to smallest packet size.
		 *
		 * TODO: what should the strategy be for choosing an
		 * alt interface?
		 */
		alt = NULL;
		SLIST_FOREACH(alt_maybe, &ix->ix_altlist, entries) {
			/* TODO: define "packet" and "payload".  I think
			 * several packets can make up one payload which would
			 * call into question this method of selecting an
			 * alternate interface... */

			if (alt_maybe->max_packet_size > vs->vs_max_payload_size)
				continue;

			if (alt == NULL ||
			    alt_maybe->max_packet_size >= alt->max_packet_size)
				alt = alt_maybe;
		}

		if (alt == NULL) {
			DPRINTF(("uvideo_stream_start_xfer: "
				 "no suitable alternate interface found\n"));
			return EINVAL;
		}

		DPRINTFN(15,("uvideo_stream_start_xfer: "
			     "choosing alternate interface "
			     "%d wMaxPacketSize=%d bInterval=%d\n",
			     alt->altno, alt->max_packet_size, alt->interval));

		err = usbd_set_interface(vs->vs_iface, alt->altno);
		if (err != USBD_NORMAL_COMPLETION) {
			DPRINTF(("uvideo_stream_start_xfer: "
				 "error setting alt interface: %s (%d)\n",
				 usbd_errstr(err), err));
			return EIO;
		}

		/* TODO: "packet" not same as frame */
		vframe_len = vs->vs_current_format.sample_size;
		uframe_len = alt->max_packet_size;
		nframes = (vframe_len + uframe_len - 1) / uframe_len;
		nframes = (nframes + 7) & ~7; /*round up for ehci inefficiency*/
		DPRINTF(("uvideo_stream_start_xfer: nframes=%d\n", nframes));

		ix->ix_nframes = nframes;
		ix->ix_uframe_len = uframe_len;
		for (i = 0; i < UVIDEO_NXFERS; i++) {
			struct uvideo_isoc *isoc = &ix->ix_i[i];
			isoc->i_frlengths =
			    kmem_alloc(sizeof(isoc->i_frlengths[0]) * nframes,
				KM_SLEEP);
			if (isoc->i_frlengths == NULL) {
				DPRINTF(("uvideo: failed to alloc frlengths:"
				 "%s (%d)\n",
				 usbd_errstr(err), err));
				return ENOMEM;
			}
		}

		err = usbd_open_pipe(vs->vs_iface, ix->ix_endpt,
				     USBD_EXCLUSIVE_USE, &ix->ix_pipe);
		if (err != USBD_NORMAL_COMPLETION) {
			DPRINTF(("uvideo: error opening pipe: %s (%d)\n",
				 usbd_errstr(err), err));
			return EIO;
		}

		for (i = 0; i < UVIDEO_NXFERS; i++) {
			struct uvideo_isoc *isoc = &ix->ix_i[i];
			isoc->i_xfer = usbd_alloc_xfer(sc->sc_udev);
			if (isoc->i_xfer == NULL) {
				DPRINTF(("uvideo: failed to alloc xfer: %s"
				 " (%d)\n",
				 usbd_errstr(err), err));
				return ENOMEM;
			}

			isoc->i_buf = usbd_alloc_buffer(isoc->i_xfer,
					       nframes * uframe_len);

			if (isoc->i_buf == NULL) {
				DPRINTF(("uvideo: failed to alloc buf: %s"
				 " (%d)\n",
				 usbd_errstr(err), err));
				return ENOMEM;
			}
		}

		uvideo_stream_recv_isoc_start(vs);

		return 0;
	default:
		/* should never get here */
		DPRINTF(("uvideo_stream_start_xfer: unknown xfer type 0x%x\n",
			 vs->vs_xfer_type));
		return EINVAL;
	}
}

static int
uvideo_stream_stop_xfer(struct uvideo_stream *vs)
{
	struct uvideo_bulk_xfer *bx;
	struct uvideo_isoc_xfer *ix;
	usbd_status err;
	int i;

	switch (vs->vs_xfer_type) {
	case UE_BULK:
		bx = &vs->vs_xfer.bulk;

		DPRINTF(("uvideo_stream_stop_xfer: UE_BULK: "
			 "waiting for thread to complete\n"));
		mutex_enter(&bx->bx_lock);
		if (bx->bx_running == true) {
			bx->bx_running = false;
			cv_wait_sig(&bx->bx_cv, &bx->bx_lock);
		}
		mutex_exit(&bx->bx_lock);

		DPRINTF(("uvideo_stream_stop_xfer: UE_BULK: cleaning up\n"));

		if (bx->bx_pipe) {
			usbd_abort_pipe(bx->bx_pipe);
			usbd_close_pipe(bx->bx_pipe);
			bx->bx_pipe = NULL;
		}

		if (bx->bx_xfer) {
			usbd_free_xfer(bx->bx_xfer);
			bx->bx_xfer = NULL;
		}

		DPRINTF(("uvideo_stream_stop_xfer: UE_BULK: done\n"));

		return 0;
	case UE_ISOCHRONOUS:
		ix = &vs->vs_xfer.isoc;
		if (ix->ix_pipe != NULL) {
			usbd_abort_pipe(ix->ix_pipe);
			usbd_close_pipe(ix->ix_pipe);
			ix->ix_pipe = NULL;
		}

		for (i = 0; i < UVIDEO_NXFERS; i++) {
			struct uvideo_isoc *isoc = &ix->ix_i[i];
			if (isoc->i_xfer != NULL) {
				usbd_free_buffer(isoc->i_xfer);
				usbd_free_xfer(isoc->i_xfer);
				isoc->i_xfer = NULL;
			}

			if (isoc->i_frlengths != NULL) {
				kmem_free(isoc->i_frlengths,
				  sizeof(isoc->i_frlengths[0]) *
				  ix->ix_nframes);
				isoc->i_frlengths = NULL;
			}
		}

		/* Give it some time to settle */
		usbd_delay_ms(vs->vs_parent->sc_udev, 1000);

		/* Set to zero bandwidth alternate interface zero */
		err = usbd_set_interface(vs->vs_iface, 0);
		if (err != USBD_NORMAL_COMPLETION) {
			DPRINTF(("uvideo_stream_stop_transfer: "
				 "error setting zero bandwidth interface: "
				 "%s (%d)\n",
				 usbd_errstr(err), err));
			return EIO;
		}

		return 0;
	default:
		/* should never get here */
		DPRINTF(("uvideo_stream_stop_xfer: unknown xfer type 0x%x\n",
			 vs->vs_xfer_type));
		return EINVAL;
	}
}

static usbd_status
uvideo_stream_recv_isoc_start(struct uvideo_stream *vs)
{
	int i;

	for (i = 0; i < UVIDEO_NXFERS; i++)
		uvideo_stream_recv_isoc_start1(&vs->vs_xfer.isoc.ix_i[i]);

	return USBD_NORMAL_COMPLETION;
}

/* Initiate a usb transfer. */
static usbd_status
uvideo_stream_recv_isoc_start1(struct uvideo_isoc *isoc)
{
	struct uvideo_isoc_xfer *ix;
	usbd_status err;
	int i;

	ix = isoc->i_ix;

	for (i = 0; i < ix->ix_nframes; ++i)
		isoc->i_frlengths[i] = ix->ix_uframe_len;

	usbd_setup_isoc_xfer(isoc->i_xfer,
			     ix->ix_pipe,
			     isoc,
			     isoc->i_frlengths,
			     ix->ix_nframes,
			     USBD_NO_COPY | USBD_SHORT_XFER_OK,
			     uvideo_stream_recv_isoc_complete);

	err = usbd_transfer(isoc->i_xfer);
	if (err != USBD_IN_PROGRESS) {
		DPRINTF(("uvideo_stream_recv_start: "
			 "usbd_transfer status=%s (%d)\n",
			 usbd_errstr(err), err));
	}
	return err;
}

static usbd_status
uvideo_stream_recv_process(struct uvideo_stream *vs, uint8_t *buf, uint32_t len)
{
	uvideo_payload_header_t *hdr;
	struct video_payload payload;

	if (len < sizeof(uvideo_payload_header_t)) {
		DPRINTF(("uvideo_stream_recv_process: len %d < payload hdr\n",
			 len));
		return USBD_SHORT_XFER;
	}

	hdr = (uvideo_payload_header_t *)buf;

	if (hdr->bHeaderLength > UVIDEO_PAYLOAD_HEADER_SIZE ||
	    hdr->bHeaderLength < sizeof(uvideo_payload_header_t))
		return USBD_INVAL;
	if (hdr->bHeaderLength == len && !(hdr->bmHeaderInfo & UV_END_OF_FRAME))
		return USBD_INVAL;
	if (hdr->bmHeaderInfo & UV_ERROR)
		return USBD_IOERROR;

	payload.data = buf + hdr->bHeaderLength;
	payload.size = len - hdr->bHeaderLength;
	payload.frameno = hdr->bmHeaderInfo & UV_FRAME_ID;
	payload.end_of_frame = hdr->bmHeaderInfo & UV_END_OF_FRAME;

	video_submit_payload(vs->vs_parent->sc_videodev, &payload);

	return USBD_NORMAL_COMPLETION;
}

/* Callback on completion of usb isoc transfer */
static void
uvideo_stream_recv_isoc_complete(usbd_xfer_handle xfer,
				 usbd_private_handle priv,
				 usbd_status status)
{
	struct uvideo_stream *vs;
	struct uvideo_isoc_xfer *ix;
	struct uvideo_isoc *isoc;
	int i;
	uint32_t count;
	uint8_t *buf;

	isoc = priv;
	vs = isoc->i_vs;
	ix = isoc->i_ix;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(("uvideo_stream_recv_isoc_complete: status=%s (%d)\n",
			usbd_errstr(status), status));

		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(ix->ix_pipe);
		else
			return;
	} else {
		usbd_get_xfer_status(xfer, NULL, NULL, &count, NULL);

		if (count == 0) {
			/* DPRINTF(("uvideo: zero length transfer\n")); */
			goto next;
		}


		for (i = 0, buf = isoc->i_buf;
		     i < ix->ix_nframes;
		     ++i, buf += ix->ix_uframe_len)
		{
			status = uvideo_stream_recv_process(vs, buf,
			    isoc->i_frlengths[i]);
			if (status == USBD_IOERROR)
				break;
		}
	}

next:
	uvideo_stream_recv_isoc_start1(isoc);
}

static void
uvideo_stream_recv_bulk_transfer(void *addr)
{
	struct uvideo_stream *vs = addr;
	struct uvideo_bulk_xfer *bx = &vs->vs_xfer.bulk;
	usbd_status err;
	uint32_t len;

	DPRINTF(("uvideo_stream_recv_bulk_transfer: "
		 "vs %p sc %p bx %p buffer %p\n", vs, vs->vs_parent, bx,
		 bx->bx_buffer));

	while (bx->bx_running) {
		len = bx->bx_buflen;
		err = usbd_bulk_transfer(bx->bx_xfer, bx->bx_pipe,
		    USBD_SHORT_XFER_OK | USBD_NO_COPY,
		    USBD_NO_TIMEOUT,
		    bx->bx_buffer, &len, "uvideorb");

		if (err == USBD_NORMAL_COMPLETION) {
			uvideo_stream_recv_process(vs, bx->bx_buffer, len);
		} else {
			DPRINTF(("uvideo_stream_recv_bulk_transfer: %s\n",
				 usbd_errstr(err)));
		}
	}

	DPRINTF(("uvideo_stream_recv_bulk_transfer: notify complete\n"));

	mutex_enter(&bx->bx_lock);
	cv_broadcast(&bx->bx_cv);
	mutex_exit(&bx->bx_lock);

	DPRINTF(("uvideo_stream_recv_bulk_transfer: return\n"));

	kthread_exit(0);
}

/*
 * uvideo_open - probe and commit video format and start receiving
 * video data
 */
static int
uvideo_open(void *addr, int flags)
{
	struct uvideo_softc *sc;
	struct uvideo_stream *vs;
	struct video_format fmt;

	sc = addr;
	vs = sc->sc_stream_in;

	DPRINTF(("uvideo_open: sc=%p\n", sc));
	if (sc->sc_dying)
		return EIO;

	/* XXX select default format */
	fmt = *vs->vs_default_format;
	return uvideo_set_format(addr, &fmt);
}


static void
uvideo_close(void *addr)
{
	struct uvideo_softc *sc;

	sc = addr;

	if (sc->sc_state != UVIDEO_STATE_CLOSED) {
		sc->sc_state = UVIDEO_STATE_CLOSED;
	}
}

static const char *
uvideo_get_devname(void *addr)
{
	struct uvideo_softc *sc = addr;
	return sc->sc_devname;
}

static const char *
uvideo_get_businfo(void *addr)
{
	struct uvideo_softc *sc = addr;
	return sc->sc_businfo;
}

static int
uvideo_enum_format(void *addr, uint32_t index, struct video_format *format)
{
	struct uvideo_softc *sc = addr;
	struct uvideo_stream *vs = sc->sc_stream_in;
	struct uvideo_pixel_format *pixel_format;
	int off;

	if (sc->sc_dying)
		return EIO;

	off = 0;
	SIMPLEQ_FOREACH(pixel_format, &vs->vs_pixel_formats, entries) {
		if (off++ != index)
			continue;
		format->pixel_format = pixel_format->pixel_format;
		return 0;
	}

	return EINVAL;
}

/*
 * uvideo_get_format
 */
static int
uvideo_get_format(void *addr, struct video_format *format)
{
	struct uvideo_softc *sc = addr;
	struct uvideo_stream *vs = sc->sc_stream_in;

	if (sc->sc_dying)
		return EIO;

	*format = vs->vs_current_format;

	return 0;
}

/*
 * uvideo_set_format - TODO: this is boken and does nothing
 */
static int
uvideo_set_format(void *addr, struct video_format *format)
{
	struct uvideo_softc *sc;
	struct uvideo_stream *vs;
	struct uvideo_format *uvfmt;
	uvideo_probe_and_commit_data_t probe, maxprobe;
	usbd_status err;

	sc = addr;

	DPRINTF(("uvideo_set_format: sc=%p\n", sc));
	if (sc->sc_dying)
		return EIO;

	vs = sc->sc_stream_in;

	uvfmt =	uvideo_stream_guess_format(vs, format->pixel_format,
					   format->width, format->height);
	if (uvfmt == NULL) {
		DPRINTF(("uvideo: uvideo_stream_guess_format couldn't find "
			 "%dx%d format %d\n", format->width, format->height,
			 format->pixel_format));
		return EINVAL;
	}

	uvideo_init_probe_data(&probe);
	probe.bFormatIndex = UVIDEO_FORMAT_GET_FORMAT_INDEX(uvfmt);
	probe.bFrameIndex = UVIDEO_FORMAT_GET_FRAME_INDEX(uvfmt);
	USETDW(probe.dwFrameInterval, vs->vs_frame_interval);	/* XXX */

	maxprobe = probe;
	err = uvideo_stream_probe(vs, UR_GET_MAX, &maxprobe);
	if (err) {
		DPRINTF(("uvideo: error probe/GET_MAX: %s (%d)\n",
			 usbd_errstr(err), err));
	} else {
		USETW(probe.wCompQuality, UGETW(maxprobe.wCompQuality));
	}

	err = uvideo_stream_probe(vs, UR_SET_CUR, &probe);
	if (err) {
		DPRINTF(("uvideo: error commit/SET_CUR: %s (%d)\n",
			 usbd_errstr(err), err));
		return EIO;
	}

	uvideo_init_probe_data(&probe);
	err = uvideo_stream_probe(vs, UR_GET_CUR, &probe);
	if (err) {
		DPRINTF(("uvideo: error commit/SET_CUR: %s (%d)\n",
			 usbd_errstr(err), err));
		return EIO;
	}

	if (probe.bFormatIndex != UVIDEO_FORMAT_GET_FORMAT_INDEX(uvfmt)) {
		DPRINTF(("uvideo: probe/GET_CUR returned format index %d "
			 "(expected %d)\n", probe.bFormatIndex,
			 UVIDEO_FORMAT_GET_FORMAT_INDEX(uvfmt)));
		probe.bFormatIndex = UVIDEO_FORMAT_GET_FORMAT_INDEX(uvfmt);
	}
	if (probe.bFrameIndex != UVIDEO_FORMAT_GET_FRAME_INDEX(uvfmt)) {
		DPRINTF(("uvideo: probe/GET_CUR returned frame index %d "
			 "(expected %d)\n", probe.bFrameIndex,
			 UVIDEO_FORMAT_GET_FRAME_INDEX(uvfmt)));
		probe.bFrameIndex = UVIDEO_FORMAT_GET_FRAME_INDEX(uvfmt);
	}
	USETDW(probe.dwFrameInterval, vs->vs_frame_interval);	/* XXX */

	/* commit/SET_CUR. Fourth step is to set the alternate
	 * interface.  Currently the fourth step is in
	 * uvideo_start_transfer.  Maybe move it here? */
	err = uvideo_stream_commit(vs, UR_SET_CUR, &probe);
	if (err) {
		DPRINTF(("uvideo: error commit/SET_CUR: %s (%d)\n",
			 usbd_errstr(err), err));
		return EIO;
	}

	DPRINTFN(15, ("uvideo_set_format: committing to format: "
		      "bmHint=0x%04x bFormatIndex=%d bFrameIndex=%d "
		      "dwFrameInterval=%u wKeyFrameRate=%d wPFrameRate=%d "
		      "wCompQuality=%d wCompWindowSize=%d wDelay=%d "
		      "dwMaxVideoFrameSize=%u dwMaxPayloadTransferSize=%u",
		      UGETW(probe.bmHint),
		      probe.bFormatIndex,
		      probe.bFrameIndex,
		      UGETDW(probe.dwFrameInterval),
		      UGETW(probe.wKeyFrameRate),
		      UGETW(probe.wPFrameRate),
		      UGETW(probe.wCompQuality),
		      UGETW(probe.wCompWindowSize),
		      UGETW(probe.wDelay),
		      UGETDW(probe.dwMaxVideoFrameSize),
		      UGETDW(probe.dwMaxPayloadTransferSize)));
	if (vs->vs_probelen == 34) {
		DPRINTFN(15, (" dwClockFrequency=%u bmFramingInfo=0x%02x "
			      "bPreferedVersion=%d bMinVersion=%d "
			      "bMaxVersion=%d",
			      UGETDW(probe.dwClockFrequency),
			      probe.bmFramingInfo,
			      probe.bPreferedVersion,
			      probe.bMinVersion,
			      probe.bMaxVersion));
	}
	DPRINTFN(15, ("\n"));

	vs->vs_frame_interval = UGETDW(probe.dwFrameInterval);
	vs->vs_max_payload_size = UGETDW(probe.dwMaxPayloadTransferSize);

	*format = uvfmt->format;
	vs->vs_current_format = *format;
	DPRINTF(("uvideo_set_format: pixeltype is %d\n", format->pixel_format));

	return 0;
}

static int
uvideo_try_format(void *addr, struct video_format *format)
{
	struct uvideo_softc *sc = addr;
	struct uvideo_stream *vs = sc->sc_stream_in;
	struct uvideo_format *uvfmt;

	uvfmt =	uvideo_stream_guess_format(vs, format->pixel_format,
					   format->width, format->height);
	if (uvfmt == NULL)
		return EINVAL;

	*format = uvfmt->format;
	return 0;
}

static int
uvideo_start_transfer(void *addr)
{
	struct uvideo_softc *sc = addr;
	struct uvideo_stream *vs;
	int s, err;

	/* FIXME: this functions should be stream specific */
	vs = SLIST_FIRST(&sc->sc_stream_list);
	s = splusb();
	err = uvideo_stream_start_xfer(vs);
	splx(s);

	return err;
}

static int
uvideo_stop_transfer(void *addr)
{
	struct uvideo_softc *sc;
	int err, s;

	sc = addr;

	s = splusb();
	err = uvideo_stream_stop_xfer(sc->sc_stream_in);
	splx(s);

	return err;
}


static int
uvideo_get_control_group(void *addr, struct video_control_group *group)
{
	struct uvideo_softc *sc;
	usb_device_request_t req;
	usbd_status err;
	uint8_t control_id, ent_id, data[16];
	uint16_t len;
	int s;

	sc = addr;

	/* request setup */
	switch (group->group_id) {
	case VIDEO_CONTROL_PANTILT_RELATIVE:
		if (group->length != 4)
			return EINVAL;

		return EINVAL;
	case VIDEO_CONTROL_SHARPNESS:
		if (group->length != 1)
			return EINVAL;

		control_id = UVIDEO_PU_SHARPNESS_CONTROL;
		ent_id = 2; /* TODO: hardcoded logitech processing unit */
		len = 2;
		break;
	default:
		return EINVAL;
	}

	/* do request */
	req.bmRequestType = UVIDEO_REQUEST_TYPE_INTERFACE |
	    UVIDEO_REQUEST_TYPE_CLASS_SPECIFIC |
	    UVIDEO_REQUEST_TYPE_GET;
	req.bRequest = UR_GET_CUR;
	USETW(req.wValue, control_id << 8);
	USETW(req.wIndex, (ent_id << 8) | sc->sc_ifaceno);
	USETW(req.wLength, len);

	s = splusb();
	err = usbd_do_request(sc->sc_udev, &req, data);
	splx(s);
	if (err != USBD_NORMAL_COMPLETION) {
		DPRINTF(("uvideo_set_control: error %s (%d)\n",
			 usbd_errstr(err), err));
		return EIO;	/* TODO: more detail here? */
	}

	/* extract request data */
	switch (group->group_id) {
	case VIDEO_CONTROL_SHARPNESS:
		group->control[0].value = UGETW(data);
		break;
	default:
		return EINVAL;
	}

	return 0;
}


static int
uvideo_set_control_group(void *addr, const struct video_control_group *group)
{
	struct uvideo_softc *sc;
	usb_device_request_t req;
	usbd_status err;
	uint8_t control_id, ent_id, data[16]; /* long enough for all controls */
	uint16_t len;
	int s;

	sc = addr;

	switch (group->group_id) {
	case VIDEO_CONTROL_PANTILT_RELATIVE:
		if (group->length != 4)
			return EINVAL;

		if (group->control[0].value != 0 ||
		    group->control[0].value != 1 ||
		    group->control[0].value != 0xff)
			return ERANGE;

		if (group->control[2].value != 0 ||
		    group->control[2].value != 1 ||
		    group->control[2].value != 0xff)
			return ERANGE;

		control_id = UVIDEO_CT_PANTILT_RELATIVE_CONTROL;
		ent_id = 1;	/* TODO: hardcoded logitech camera terminal  */
		len = 4;
		data[0] = group->control[0].value;
		data[1] = group->control[1].value;
		data[2] = group->control[2].value;
		data[3] = group->control[3].value;
		break;
	case VIDEO_CONTROL_BRIGHTNESS:
		if (group->length != 1)
			return EINVAL;
		control_id = UVIDEO_PU_BRIGHTNESS_CONTROL;
		ent_id = 2;
		len = 2;
		USETW(data, group->control[0].value);
		break;
	case VIDEO_CONTROL_GAIN:
		if (group->length != 1)
			return EINVAL;
		control_id = UVIDEO_PU_GAIN_CONTROL;
		ent_id = 2;
		len = 2;
		USETW(data, group->control[0].value);
		break;
	case VIDEO_CONTROL_SHARPNESS:
		if (group->length != 1)
			return EINVAL;
		control_id = UVIDEO_PU_SHARPNESS_CONTROL;
		ent_id = 2; /* TODO: hardcoded logitech processing unit */
		len = 2;
		USETW(data, group->control[0].value);
		break;
	default:
		return EINVAL;
	}

	req.bmRequestType = UVIDEO_REQUEST_TYPE_INTERFACE |
	    UVIDEO_REQUEST_TYPE_CLASS_SPECIFIC |
	    UVIDEO_REQUEST_TYPE_SET;
	req.bRequest = UR_SET_CUR;
	USETW(req.wValue, control_id << 8);
	USETW(req.wIndex, (ent_id << 8) | sc->sc_ifaceno);
	USETW(req.wLength, len);

	s = splusb();
	err = usbd_do_request(sc->sc_udev, &req, data);
	splx(s);
	if (err != USBD_NORMAL_COMPLETION) {
		DPRINTF(("uvideo_set_control: error %s (%d)\n",
			 usbd_errstr(err), err));
		return EIO;	/* TODO: more detail here? */
	}

	return 0;
}

static usbd_status
uvideo_stream_probe_and_commit(struct uvideo_stream *vs,
			       uint8_t action, uint8_t control,
			       void *data)
{
	usb_device_request_t req;

	switch (action) {
	case UR_SET_CUR:
		req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
		USETW(req.wLength, vs->vs_probelen);
		break;
	case UR_GET_CUR:
	case UR_GET_MIN:
	case UR_GET_MAX:
	case UR_GET_DEF:
		req.bmRequestType = UT_READ_CLASS_INTERFACE;
		USETW(req.wLength, vs->vs_probelen);
		break;
	case UR_GET_INFO:
		req.bmRequestType = UT_READ_CLASS_INTERFACE;
		USETW(req.wLength, sizeof(uByte));
		break;
	case UR_GET_LEN:
		req.bmRequestType = UT_READ_CLASS_INTERFACE;
		USETW(req.wLength, sizeof(uWord)); /* is this right? */
		break;
	default:
		DPRINTF(("uvideo_probe_and_commit: "
			 "unknown request action %d\n", action));
		return USBD_NOT_STARTED;
	}

	req.bRequest = action;
	USETW2(req.wValue, control, 0);
	USETW2(req.wIndex, 0, vs->vs_ifaceno);

	return (usbd_do_request_flags(vs->vs_parent->sc_udev, &req, data,
				      0, 0,
				      USBD_DEFAULT_TIMEOUT));
}

static void
uvideo_init_probe_data(uvideo_probe_and_commit_data_t *probe)
{
	/* all zeroes tells camera to choose what it wants */
	memset(probe, 0, sizeof(*probe));
}


#ifdef _MODULE

MODULE(MODULE_CLASS_DRIVER, uvideo, NULL);
static const struct cfiattrdata videobuscf_iattrdata = {
        "videobus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata * const uvideo_attrs[] = {
	&videobuscf_iattrdata, NULL
};
CFDRIVER_DECL(uvideo, DV_DULL, uvideo_attrs);
extern struct cfattach uvideo_ca;
extern struct cfattach uvideo_ca;
static int uvideoloc[6] = { -1, -1, -1, -1, -1, -1 };
static struct cfparent uhubparent = {
        "usbifif", NULL, DVUNIT_ANY
};
static struct cfdata uvideo_cfdata[] = {
	{
		.cf_name = "uvideo",
		.cf_atname = "uvideo",
		.cf_unit = 0,
		.cf_fstate = FSTATE_STAR,
		.cf_loc = uvideoloc,
		.cf_flags = 0,
		.cf_pspec = &uhubparent,
	},
	{ NULL, NULL, 0, 0, NULL, 0, NULL },
};

static int
uvideo_modcmd(modcmd_t cmd, void *arg)
{
	int err;


	switch (cmd) {
	case MODULE_CMD_INIT:
		DPRINTF(("uvideo: attempting to load\n"));

		err = config_cfdriver_attach(&uvideo_cd);
		if (err)
			return err;
		err = config_cfattach_attach("uvideo", &uvideo_ca);
		if (err) {
			config_cfdriver_detach(&uvideo_cd);
			return err;
		}
		err = config_cfdata_attach(uvideo_cfdata, 1);
		if (err) {
			config_cfattach_detach("uvideo", &uvideo_ca);
			config_cfdriver_detach(&uvideo_cd);
			return err;
		}
		DPRINTF(("uvideo: loaded module\n"));
		return 0;
	case MODULE_CMD_FINI:
		DPRINTF(("uvideo: attempting to unload module\n"));
		err = config_cfdata_detach(uvideo_cfdata);
		if (err)
			return err;
		config_cfattach_detach("uvideo", &uvideo_ca);
		config_cfdriver_detach(&uvideo_cd);
		DPRINTF(("uvideo: module unload\n"));
		return 0;
	default:
		return ENOTTY;
	}
}

#endif	/* _MODULE */


#ifdef UVIDEO_DEBUG
/* Some functions to print out descriptors.  Mostly useless other than
 * debugging/exploration purposes. */


static void
print_bitmap(const uByte *start, uByte nbytes)
{
	int byte, bit;

	/* most significant first */
	for (byte = nbytes-1; byte >= 0; --byte) {
		if (byte < nbytes-1) printf("-");
		for (bit = 7; bit >= 0; --bit)
			printf("%01d", (start[byte] >> bit) &1);
	}
}

static void
print_descriptor(const usb_descriptor_t *desc)
{
	static int current_class = -1;
	static int current_subclass = -1;

	if (desc->bDescriptorType == UDESC_INTERFACE) {
		const usb_interface_descriptor_t *id;
		id = (const usb_interface_descriptor_t *)desc;
		current_class = id->bInterfaceClass;
		current_subclass = id->bInterfaceSubClass;
		print_interface_descriptor(id);
		printf("\n");
		return;
	}

	printf("  ");		/* indent */

	if (current_class == UICLASS_VIDEO) {
		switch (current_subclass) {
		case UISUBCLASS_VIDEOCONTROL:
			print_vc_descriptor(desc);
			break;
		case UISUBCLASS_VIDEOSTREAMING:
			print_vs_descriptor(desc);
			break;
		case UISUBCLASS_VIDEOCOLLECTION:
			printf("uvc collection: len=%d type=0x%02x",
			    desc->bLength, desc->bDescriptorType);
			break;
		}
	} else {
		printf("non uvc descriptor len=%d type=0x%02x",
		    desc->bLength, desc->bDescriptorType);
	}

	printf("\n");
}

static void
print_vc_descriptor(const usb_descriptor_t *desc)
{
	const uvideo_descriptor_t *vcdesc;

	printf("VC ");

	switch (desc->bDescriptorType) {
	case UDESC_ENDPOINT:
		print_endpoint_descriptor(
			(const usb_endpoint_descriptor_t *)desc);
		break;
	case UDESC_CS_INTERFACE:
		vcdesc = (const uvideo_descriptor_t *)desc;
		switch (vcdesc->bDescriptorSubtype) {
		case UDESC_VC_HEADER:
			print_vc_header_descriptor(
			  (const uvideo_vc_header_descriptor_t *)
				vcdesc);
			break;
		case UDESC_INPUT_TERMINAL:
			switch (UGETW(
			   ((const uvideo_input_terminal_descriptor_t *)
				    vcdesc)->wTerminalType)) {
			case UVIDEO_ITT_CAMERA:
				print_camera_terminal_descriptor(
			  (const uvideo_camera_terminal_descriptor_t *)vcdesc);
				break;
			default:
				print_input_terminal_descriptor(
			  (const uvideo_input_terminal_descriptor_t *)vcdesc);
				break;
			}
			break;
		case UDESC_OUTPUT_TERMINAL:
			print_output_terminal_descriptor(
				(const uvideo_output_terminal_descriptor_t *)
				vcdesc);
			break;
		case UDESC_SELECTOR_UNIT:
			print_selector_unit_descriptor(
				(const uvideo_selector_unit_descriptor_t *)
				vcdesc);
			break;
		case UDESC_PROCESSING_UNIT:
			print_processing_unit_descriptor(
				(const uvideo_processing_unit_descriptor_t *)
				vcdesc);
			break;
		case UDESC_EXTENSION_UNIT:
			print_extension_unit_descriptor(
				(const uvideo_extension_unit_descriptor_t *)
				vcdesc);
			break;
		default:
			printf("class specific interface "
			    "len=%d type=0x%02x subtype=0x%02x",
			    vcdesc->bLength,
			    vcdesc->bDescriptorType,
			    vcdesc->bDescriptorSubtype);
			break;
		}
		break;
	case UDESC_CS_ENDPOINT:
		vcdesc = (const uvideo_descriptor_t *)desc;
		switch (vcdesc->bDescriptorSubtype) {
		case UDESC_VC_INTERRUPT_ENDPOINT:
			print_interrupt_endpoint_descriptor(
			    (const uvideo_vc_interrupt_endpoint_descriptor_t *)
				vcdesc);
			break;
		default:
			printf("class specific endpoint "
			    "len=%d type=0x%02x subtype=0x%02x",
			    vcdesc->bLength,
			    vcdesc->bDescriptorType,
			    vcdesc->bDescriptorSubtype);
			break;
		}
		break;
	default:
		printf("unknown: len=%d type=0x%02x",
		    desc->bLength, desc->bDescriptorType);
		break;
	}
}

static void
print_vs_descriptor(const usb_descriptor_t *desc)
{
	const uvideo_descriptor_t * vsdesc;
	printf("VS ");

	switch (desc->bDescriptorType) {
	case UDESC_ENDPOINT:
		print_endpoint_descriptor(
			(const usb_endpoint_descriptor_t *)desc);
		break;
	case UDESC_CS_INTERFACE:
		vsdesc = (const uvideo_descriptor_t *)desc;
		switch (vsdesc->bDescriptorSubtype) {
		case UDESC_VS_INPUT_HEADER:
			print_vs_input_header_descriptor(
			 (const uvideo_vs_input_header_descriptor_t *)
				vsdesc);
			break;
		case UDESC_VS_OUTPUT_HEADER:
			print_vs_output_header_descriptor(
			(const uvideo_vs_output_header_descriptor_t *)
				vsdesc);
			break;
		case UDESC_VS_FORMAT_UNCOMPRESSED:
			print_vs_format_uncompressed_descriptor(
			   (const uvideo_vs_format_uncompressed_descriptor_t *)
				vsdesc);
			break;
		case UDESC_VS_FRAME_UNCOMPRESSED:
			print_vs_frame_uncompressed_descriptor(
			    (const uvideo_vs_frame_uncompressed_descriptor_t *)
				vsdesc);
			break;
		case UDESC_VS_FORMAT_MJPEG:
			print_vs_format_mjpeg_descriptor(
				(const uvideo_vs_format_mjpeg_descriptor_t *)
				vsdesc);
			break;
		case UDESC_VS_FRAME_MJPEG:
			print_vs_frame_mjpeg_descriptor(
				(const uvideo_vs_frame_mjpeg_descriptor_t *)
				vsdesc);
			break;
		case UDESC_VS_FORMAT_DV:
			print_vs_format_dv_descriptor(
				(const uvideo_vs_format_dv_descriptor_t *)
				vsdesc);
			break;
		default:
			printf("unknown cs interface: len=%d type=0x%02x "
			    "subtype=0x%02x",
			    vsdesc->bLength, vsdesc->bDescriptorType,
			    vsdesc->bDescriptorSubtype);
		}
		break;
	default:
		printf("unknown: len=%d type=0x%02x",
		    desc->bLength, desc->bDescriptorType);
		break;
	}
}

static void
print_interface_descriptor(const usb_interface_descriptor_t *id)
{
	printf("Interface: Len=%d Type=0x%02x "
	    "bInterfaceNumber=0x%02x "
	    "bAlternateSetting=0x%02x bNumEndpoints=0x%02x "
	    "bInterfaceClass=0x%02x bInterfaceSubClass=0x%02x "
	    "bInterfaceProtocol=0x%02x iInterface=0x%02x",
	    id->bLength,
	    id->bDescriptorType,
	    id->bInterfaceNumber,
	    id->bAlternateSetting,
	    id->bNumEndpoints,
	    id->bInterfaceClass,
	    id->bInterfaceSubClass,
	    id->bInterfaceProtocol,
	    id->iInterface);
}

static void
print_endpoint_descriptor(const usb_endpoint_descriptor_t *desc)
{
	printf("Endpoint: Len=%d Type=0x%02x "
	    "bEndpointAddress=0x%02x ",
	    desc->bLength,
	    desc->bDescriptorType,
	    desc->bEndpointAddress);
	printf("bmAttributes=");
	print_bitmap(&desc->bmAttributes, 1);
	printf(" wMaxPacketSize=%d bInterval=%d",
	    UGETW(desc->wMaxPacketSize),
	    desc->bInterval);
}

static void
print_vc_header_descriptor(
	const uvideo_vc_header_descriptor_t *desc)
{
	printf("Interface Header: "
	    "Len=%d Type=0x%02x Subtype=0x%02x "
	    "bcdUVC=%d wTotalLength=%d "
	    "dwClockFrequency=%u bInCollection=%d",
	    desc->bLength,
	    desc->bDescriptorType,
	    desc->bDescriptorSubtype,
	    UGETW(desc->bcdUVC),
	    UGETW(desc->wTotalLength),
	    UGETDW(desc->dwClockFrequency),
	    desc->bInCollection);
}

static void
print_input_terminal_descriptor(
	const uvideo_input_terminal_descriptor_t *desc)
{
	printf("Input Terminal: "
	    "Len=%d Type=0x%02x Subtype=0x%02x "
	    "bTerminalID=%d wTerminalType=%x bAssocTerminal=%d "
	    "iTerminal=%d",
	    desc->bLength,
	    desc->bDescriptorType,
	    desc->bDescriptorSubtype,
	    desc->bTerminalID,
	    UGETW(desc->wTerminalType),
	    desc->bAssocTerminal,
	    desc->iTerminal);
}

static void
print_output_terminal_descriptor(
	const uvideo_output_terminal_descriptor_t *desc)
{
	printf("Output Terminal: "
	    "Len=%d Type=0x%02x Subtype=0x%02x "
	    "bTerminalID=%d wTerminalType=%x bAssocTerminal=%d "
	    "bSourceID=%d iTerminal=%d",
	    desc->bLength,
	    desc->bDescriptorType,
	    desc->bDescriptorSubtype,
	    desc->bTerminalID,
	    UGETW(desc->wTerminalType),
	    desc->bAssocTerminal,
	    desc->bSourceID,
	    desc->iTerminal);
}

static void
print_camera_terminal_descriptor(
	const uvideo_camera_terminal_descriptor_t *desc)
{
	printf("Camera Terminal: "
	    "Len=%d Type=0x%02x Subtype=0x%02x "
	    "bTerminalID=%d wTerminalType=%x bAssocTerminal=%d "
	    "iTerminal=%d "
	    "wObjectiveFocalLengthMin/Max=%d/%d "
	    "wOcularFocalLength=%d "
	    "bControlSize=%d ",
	    desc->bLength,
	    desc->bDescriptorType,
	    desc->bDescriptorSubtype,
	    desc->bTerminalID,
	    UGETW(desc->wTerminalType),
	    desc->bAssocTerminal,
	    desc->iTerminal,
	    UGETW(desc->wObjectiveFocalLengthMin),
	    UGETW(desc->wObjectiveFocalLengthMax),
	    UGETW(desc->wOcularFocalLength),
	    desc->bControlSize);
	printf("bmControls=");
	print_bitmap(desc->bmControls, desc->bControlSize);
}

static void
print_selector_unit_descriptor(
	const uvideo_selector_unit_descriptor_t *desc)
{
	int i;
	const uByte *b;
	printf("Selector Unit: "
	    "Len=%d Type=0x%02x Subtype=0x%02x "
	    "bUnitID=%d bNrInPins=%d ",
	    desc->bLength,
	    desc->bDescriptorType,
	    desc->bDescriptorSubtype,
	    desc->bUnitID,
	    desc->bNrInPins);
	printf("baSourceIDs=");
	b = &desc->baSourceID[0];
	for (i = 0; i < desc->bNrInPins; ++i)
		printf("%d ", *b++);
	printf("iSelector=%d", *b);
}

static void
print_processing_unit_descriptor(
	const uvideo_processing_unit_descriptor_t *desc)
{
	const uByte *b;

	printf("Processing Unit: "
	    "Len=%d Type=0x%02x Subtype=0x%02x "
	    "bUnitID=%d bSourceID=%d wMaxMultiplier=%d bControlSize=%d ",
	    desc->bLength,
	    desc->bDescriptorType,
	    desc->bDescriptorSubtype,
	    desc->bUnitID,
	    desc->bSourceID,
	    UGETW(desc->wMaxMultiplier),
	    desc->bControlSize);
	printf("bmControls=");
	print_bitmap(desc->bmControls, desc->bControlSize);
	b = &desc->bControlSize + desc->bControlSize + 1;
	printf(" iProcessing=%d bmVideoStandards=", *b);
	b += 1;
	print_bitmap(b, 1);
}

static void
print_extension_unit_descriptor(
	const uvideo_extension_unit_descriptor_t *desc)
{
	const uByte * byte;
	uByte controlbytes;
	int i;

	printf("Extension Unit: "
	    "Len=%d Type=0x%02x Subtype=0x%02x "
	    "bUnitID=%d ",
	    desc->bLength,
	    desc->bDescriptorType,
	    desc->bDescriptorSubtype,
	    desc->bUnitID);

	printf("guidExtensionCode=");
	usb_guid_print(&desc->guidExtensionCode);
	printf(" ");

	printf("bNumControls=%d bNrInPins=%d ",
	    desc->bNumControls,
	    desc->bNrInPins);

	printf("baSourceIDs=");
	byte = &desc->baSourceID[0];
	for (i = 0; i < desc->bNrInPins; ++i)
		printf("%d ", *byte++);

	controlbytes = *byte++;
	printf("bControlSize=%d ", controlbytes);
	printf("bmControls=");
	print_bitmap(byte, controlbytes);

	byte += controlbytes;
	printf(" iExtension=%d", *byte);
}

static void
print_interrupt_endpoint_descriptor(
	const uvideo_vc_interrupt_endpoint_descriptor_t *desc)
{
	printf("Interrupt Endpoint: "
	    "Len=%d Type=0x%02x Subtype=0x%02x "
	    "wMaxTransferSize=%d ",
	    desc->bLength,
	    desc->bDescriptorType,
	    desc->bDescriptorSubtype,
	    UGETW(desc->wMaxTransferSize));
}


static void
print_vs_output_header_descriptor(
	const uvideo_vs_output_header_descriptor_t *desc)
{
	printf("Interface Output Header: "
	    "Len=%d Type=0x%02x Subtype=0x%02x "
	    "bNumFormats=%d wTotalLength=%d bEndpointAddress=%d "
	    "bTerminalLink=%d bControlSize=%d",
	    desc->bLength,
	    desc->bDescriptorType,
	    desc->bDescriptorSubtype,
	    desc->bNumFormats,
	    UGETW(desc->wTotalLength),
	    desc->bEndpointAddress,
	    desc->bTerminalLink,
	    desc->bControlSize);
}

static void
print_vs_input_header_descriptor(
	const uvideo_vs_input_header_descriptor_t *desc)
{
	printf("Interface Input Header: "
	    "Len=%d Type=0x%02x Subtype=0x%02x "
	    "bNumFormats=%d wTotalLength=%d bEndpointAddress=%d "
	    "bmInfo=%x bTerminalLink=%d bStillCaptureMethod=%d "
	    "bTriggerSupport=%d bTriggerUsage=%d bControlSize=%d ",
	    desc->bLength,
	    desc->bDescriptorType,
	    desc->bDescriptorSubtype,
	    desc->bNumFormats,
	    UGETW(desc->wTotalLength),
	    desc->bEndpointAddress,
	    desc->bmInfo,
	    desc->bTerminalLink,
	    desc->bStillCaptureMethod,
	    desc->bTriggerSupport,
	    desc->bTriggerUsage,
	    desc->bControlSize);
	print_bitmap(desc->bmaControls, desc->bControlSize);
}

static void
print_vs_format_uncompressed_descriptor(
	const uvideo_vs_format_uncompressed_descriptor_t *desc)
{
	printf("Format Uncompressed: "
	    "Len=%d Type=0x%02x Subtype=0x%02x "
	    "bFormatIndex=%d bNumFrameDescriptors=%d ",
	    desc->bLength,
	    desc->bDescriptorType,
	    desc->bDescriptorSubtype,
	    desc->bFormatIndex,
	    desc->bNumFrameDescriptors);
	usb_guid_print(&desc->guidFormat);
	printf(" bBitsPerPixel=%d bDefaultFrameIndex=%d "
	    "bAspectRatioX=%d bAspectRatioY=%d "
	    "bmInterlaceFlags=0x%02x bCopyProtect=%d",
	    desc->bBitsPerPixel,
	    desc->bDefaultFrameIndex,
	    desc->bAspectRatioX,
	    desc->bAspectRatioY,
	    desc->bmInterlaceFlags,
	    desc->bCopyProtect);
}

static void
print_vs_frame_uncompressed_descriptor(
	const uvideo_vs_frame_uncompressed_descriptor_t *desc)
{
	printf("Frame Uncompressed: "
	    "Len=%d Type=0x%02x Subtype=0x%02x "
	    "bFrameIndex=%d bmCapabilities=0x%02x "
	    "wWidth=%d wHeight=%d dwMinBitRate=%u dwMaxBitRate=%u "
	    "dwMaxVideoFrameBufferSize=%u dwDefaultFrameInterval=%u "
	    "bFrameIntervalType=%d",
	    desc->bLength,
	    desc->bDescriptorType,
	    desc->bDescriptorSubtype,
	    desc->bFrameIndex,
	    desc->bmCapabilities,
	    UGETW(desc->wWidth),
	    UGETW(desc->wHeight),
	    UGETDW(desc->dwMinBitRate),
	    UGETDW(desc->dwMaxBitRate),
	    UGETDW(desc->dwMaxVideoFrameBufferSize),
	    UGETDW(desc->dwDefaultFrameInterval),
	    desc->bFrameIntervalType);
}

static void
print_vs_format_mjpeg_descriptor(
	const uvideo_vs_format_mjpeg_descriptor_t *desc)
{
	printf("MJPEG format: "
	    "Len=%d Type=0x%02x Subtype=0x%02x "
	    "bFormatIndex=%d bNumFrameDescriptors=%d bmFlags=0x%02x "
	    "bDefaultFrameIndex=%d bAspectRatioX=%d bAspectRatioY=%d "
	    "bmInterlaceFlags=0x%02x bCopyProtect=%d",
	    desc->bLength,
	    desc->bDescriptorType,
	    desc->bDescriptorSubtype,
	    desc->bFormatIndex,
	    desc->bNumFrameDescriptors,
	    desc->bmFlags,
	    desc->bDefaultFrameIndex,
	    desc->bAspectRatioX,
	    desc->bAspectRatioY,
	    desc->bmInterlaceFlags,
	    desc->bCopyProtect);
}

static void
print_vs_frame_mjpeg_descriptor(
	const uvideo_vs_frame_mjpeg_descriptor_t *desc)
{
	printf("MJPEG frame: "
	    "Len=%d Type=0x%02x Subtype=0x%02x "
	    "bFrameIndex=%d bmCapabilities=0x%02x "
	    "wWidth=%d wHeight=%d dwMinBitRate=%u dwMaxBitRate=%u "
	    "dwMaxVideoFrameBufferSize=%u dwDefaultFrameInterval=%u "
	    "bFrameIntervalType=%d",
	    desc->bLength,
	    desc->bDescriptorType,
	    desc->bDescriptorSubtype,
	    desc->bFrameIndex,
	    desc->bmCapabilities,
	    UGETW(desc->wWidth),
	    UGETW(desc->wHeight),
	    UGETDW(desc->dwMinBitRate),
	    UGETDW(desc->dwMaxBitRate),
	    UGETDW(desc->dwMaxVideoFrameBufferSize),
	    UGETDW(desc->dwDefaultFrameInterval),
	    desc->bFrameIntervalType);
}

static void
print_vs_format_dv_descriptor(
	const uvideo_vs_format_dv_descriptor_t *desc)
{
	printf("MJPEG format: "
	    "Len=%d Type=0x%02x Subtype=0x%02x "
	    "bFormatIndex=%d dwMaxVideoFrameBufferSize=%u "
	    "bFormatType/Rate=%d bFormatType/Format=%d",
	    desc->bLength,
	    desc->bDescriptorType,
	    desc->bDescriptorSubtype,
	    desc->bFormatIndex,
	    UGETDW(desc->dwMaxVideoFrameBufferSize),
	    UVIDEO_GET_DV_FREQ(desc->bFormatType),
	    UVIDEO_GET_DV_FORMAT(desc->bFormatType));
}

#endif /* !UVIDEO_DEBUG */

static const usb_descriptor_t *
usb_desc_iter_peek_next(usbd_desc_iter_t *iter)
{
        const usb_descriptor_t *desc;

        if (iter->cur + sizeof(usb_descriptor_t) >= iter->end) {
                if (iter->cur != iter->end)
                        printf("usb_desc_iter_peek_next: bad descriptor\n");
                return NULL;
        }
        desc = (const usb_descriptor_t *)iter->cur;
        if (desc->bLength == 0) {
                printf("usb_desc_iter_peek_next: descriptor length = 0\n");
                return NULL;
        }
        if (iter->cur + desc->bLength > iter->end) {
                printf("usb_desc_iter_peek_next: descriptor length too large\n");
                return NULL;
        }
        return desc;
}

/* Return the next interface descriptor, skipping over any other
 * descriptors.  Returns NULL at the end or on error. */
static const usb_interface_descriptor_t *
usb_desc_iter_next_interface(usbd_desc_iter_t *iter)
{
	const usb_descriptor_t *desc;

	while ((desc = usb_desc_iter_peek_next(iter)) != NULL &&
	       desc->bDescriptorType != UDESC_INTERFACE)
	{
		usb_desc_iter_next(iter);
	}

	return (const usb_interface_descriptor_t *)usb_desc_iter_next(iter);
}

/* Returns the next non-interface descriptor, returning NULL when the
 * next descriptor would be an interface descriptor. */
static const usb_descriptor_t *
usb_desc_iter_next_non_interface(usbd_desc_iter_t *iter)
{
	const usb_descriptor_t *desc;

	if ((desc = usb_desc_iter_peek_next(iter)) != NULL &&
	    desc->bDescriptorType != UDESC_INTERFACE)
	{
		return (usb_desc_iter_next(iter));
	} else {
		return NULL;
	}
}

#ifdef UVIDEO_DEBUG
static void
usb_guid_print(const usb_guid_t *guid)
{
	printf("%04X-%02X-%02X-",
	       UGETDW(guid->data1),
	       UGETW(guid->data2),
	       UGETW(guid->data3));
	printf("%02X%02X-",
	       guid->data4[0],
	       guid->data4[1]);
	printf("%02X%02X%02X%02X%02X%02X",
	       guid->data4[2],
	       guid->data4[3],
	       guid->data4[4],
	       guid->data4[5],
	       guid->data4[6],
	       guid->data4[7]);
}
#endif /* !UVIDEO_DEBUG */

/* Returns less than zero, zero, or greater than zero if uguid is less
 * than, equal to, or greater than guid. */
static int
usb_guid_cmp(const usb_guid_t *uguid, const guid_t *guid)
{
	if (guid->data1 > UGETDW(uguid->data1))
		return 1;
	else if (guid->data1 < UGETDW(uguid->data1))
		return -1;

	if (guid->data2 > UGETW(uguid->data2))
		return 1;
	else if (guid->data2 < UGETW(uguid->data2))
		return -1;

	if (guid->data3 > UGETW(uguid->data3))
		return 1;
	else if (guid->data3 < UGETW(uguid->data3))
		return -1;

	return (memcmp(guid->data4, uguid->data4, 8));
}

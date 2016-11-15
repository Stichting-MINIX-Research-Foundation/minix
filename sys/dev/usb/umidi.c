/*	$NetBSD: umidi.c,v 1.68 2015/01/02 20:42:44 mrg Exp $	*/

/*
 * Copyright (c) 2001, 2012, 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Takuya SHIOZAKI (tshiozak@NetBSD.org), (full-size transfers, extended
 * hw_if) Chapman Flack (chap@NetBSD.org), and Matthew R. Green
 * (mrg@eterna.com.au).
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
__KERNEL_RCSID(0, "$NetBSD: umidi.c,v 1.68 2015/01/02 20:42:44 mrg Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/poll.h>
#include <sys/intr.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#include <dev/auconv.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/umidi_quirks.h>
#include <dev/midi_if.h>

/* Jack Descriptor */
#define UMIDI_MS_HEADER	0x01
#define UMIDI_IN_JACK	0x02
#define UMIDI_OUT_JACK	0x03

/* Jack Type */
#define UMIDI_EMBEDDED	0x01
#define UMIDI_EXTERNAL	0x02

/* generic, for iteration */
typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
} UPACKED umidi_cs_descriptor_t;

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uWord		bcdMSC;
	uWord		wTotalLength;
} UPACKED umidi_cs_interface_descriptor_t;
#define UMIDI_CS_INTERFACE_DESCRIPTOR_SIZE 7

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bNumEmbMIDIJack;
} UPACKED umidi_cs_endpoint_descriptor_t;
#define UMIDI_CS_ENDPOINT_DESCRIPTOR_SIZE 4

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bJackType;
	uByte		bJackID;
} UPACKED umidi_jack_descriptor_t;
#define	UMIDI_JACK_DESCRIPTOR_SIZE	5


#define TO_D(p) ((usb_descriptor_t *)(p))
#define NEXT_D(desc) TO_D((char *)(desc)+(desc)->bLength)
#define TO_IFD(desc) ((usb_interface_descriptor_t *)(desc))
#define TO_CSIFD(desc) ((umidi_cs_interface_descriptor_t *)(desc))
#define TO_EPD(desc) ((usb_endpoint_descriptor_t *)(desc))
#define TO_CSEPD(desc) ((umidi_cs_endpoint_descriptor_t *)(desc))


#define UMIDI_PACKET_SIZE 4

/*
 * hierarchie
 *
 * <-- parent	       child -->
 *
 * umidi(sc) -> endpoint -> jack   <- (dynamically assignable) - mididev
 *	   ^	 |    ^	    |
 *	   +-----+    +-----+
 */

/* midi device */
struct umidi_mididev {
	struct umidi_softc	*sc;
	device_t		mdev;
	/* */
	struct umidi_jack	*in_jack;
	struct umidi_jack	*out_jack;
	char			*label;
	size_t			label_len;
	/* */
	int			opened;
	int			closing;
	int			flags;
};

/* Jack Information */
struct umidi_jack {
	struct umidi_endpoint	*endpoint;
	/* */
	int			cable_number;
	void			*arg;
	int			bound;
	int			opened;
	unsigned char		*midiman_ppkt;
	union {
		struct {
			void			(*intr)(void *);
		} out;
		struct {
			void			(*intr)(void *, int);
		} in;
	} u;
};

#define UMIDI_MAX_EPJACKS	16
typedef unsigned char (*umidi_packet_bufp)[UMIDI_PACKET_SIZE];
/* endpoint data */
struct umidi_endpoint {
	struct umidi_softc	*sc;
	/* */
	int			addr;
	usbd_pipe_handle	pipe;
	usbd_xfer_handle	xfer;
	umidi_packet_bufp	buffer;
	umidi_packet_bufp	next_slot;
	u_int32_t               buffer_size;
	int			num_scheduled;
	int			num_open;
	int			num_jacks;
	int			soliciting;
	void			*solicit_cookie;
	int			armed;
	struct umidi_jack	*jacks[UMIDI_MAX_EPJACKS];
	u_int16_t		this_schedule; /* see UMIDI_MAX_EPJACKS */
	u_int16_t		next_schedule;
};

/* software context */
struct umidi_softc {
	device_t		sc_dev;
	usbd_device_handle	sc_udev;
	usbd_interface_handle	sc_iface;
	const struct umidi_quirk	*sc_quirk;

	int			sc_dying;

	int			sc_out_num_jacks;
	struct umidi_jack	*sc_out_jacks;
	int			sc_in_num_jacks;
	struct umidi_jack	*sc_in_jacks;
	struct umidi_jack	*sc_jacks;

	int			sc_num_mididevs;
	struct umidi_mididev	*sc_mididevs;

	int			sc_out_num_endpoints;
	struct umidi_endpoint	*sc_out_ep;
	int			sc_in_num_endpoints;
	struct umidi_endpoint	*sc_in_ep;
	struct umidi_endpoint	*sc_endpoints;
	size_t			sc_endpoints_len;
	int			cblnums_global;

	kmutex_t		sc_lock;
	kcondvar_t		sc_cv;
	kcondvar_t		sc_detach_cv;

	int			sc_refcnt;
};

#ifdef UMIDI_DEBUG
#define DPRINTF(x)	if (umididebug) printf x
#define DPRINTFN(n,x)	if (umididebug >= (n)) printf x
#include <sys/time.h>
static struct timeval umidi_tv;
int	umididebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define UMIDI_ENDPOINT_SIZE(sc)	(sizeof(*(sc)->sc_out_ep) * \
				 (sc->sc_out_num_endpoints + \
				  sc->sc_in_num_endpoints))


static int umidi_open(void *, int,
		      void (*)(void *, int), void (*)(void *), void *);
static void umidi_close(void *);
static int umidi_channelmsg(void *, int, int, u_char *, int);
static int umidi_commonmsg(void *, int, u_char *, int);
static int umidi_sysex(void *, u_char *, int);
static int umidi_rtmsg(void *, int);
static void umidi_getinfo(void *, struct midi_info *);
static void umidi_get_locks(void *, kmutex_t **, kmutex_t **);

static usbd_status alloc_pipe(struct umidi_endpoint *);
static void free_pipe(struct umidi_endpoint *);

static usbd_status alloc_all_endpoints(struct umidi_softc *);
static void free_all_endpoints(struct umidi_softc *);

static usbd_status alloc_all_jacks(struct umidi_softc *);
static void free_all_jacks(struct umidi_softc *);
static usbd_status bind_jacks_to_mididev(struct umidi_softc *,
					 struct umidi_jack *,
					 struct umidi_jack *,
					 struct umidi_mididev *);
static void unbind_jacks_from_mididev(struct umidi_mididev *);
static void unbind_all_jacks(struct umidi_softc *);
static usbd_status assign_all_jacks_automatically(struct umidi_softc *);
static usbd_status open_out_jack(struct umidi_jack *, void *,
				 void (*)(void *));
static usbd_status open_in_jack(struct umidi_jack *, void *,
				void (*)(void *, int));
static void close_out_jack(struct umidi_jack *);
static void close_in_jack(struct umidi_jack *);

static usbd_status attach_mididev(struct umidi_softc *, struct umidi_mididev *);
static usbd_status detach_mididev(struct umidi_mididev *, int);
static void deactivate_mididev(struct umidi_mididev *);
static usbd_status alloc_all_mididevs(struct umidi_softc *, int);
static void free_all_mididevs(struct umidi_softc *);
static usbd_status attach_all_mididevs(struct umidi_softc *);
static usbd_status detach_all_mididevs(struct umidi_softc *, int);
static void deactivate_all_mididevs(struct umidi_softc *);
static void describe_mididev(struct umidi_mididev *);

#ifdef UMIDI_DEBUG
static void dump_sc(struct umidi_softc *);
static void dump_ep(struct umidi_endpoint *);
static void dump_jack(struct umidi_jack *);
#endif

static usbd_status start_input_transfer(struct umidi_endpoint *);
static usbd_status start_output_transfer(struct umidi_endpoint *);
static int out_jack_output(struct umidi_jack *, u_char *, int, int);
static void in_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);
static void out_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);
static void out_solicit(void *); /* struct umidi_endpoint* for softintr */
static void out_solicit_locked(void *); /* pre-locked version */


const struct midi_hw_if umidi_hw_if = {
	.open = umidi_open,
	.close = umidi_close,
	.output = umidi_rtmsg,
	.getinfo = umidi_getinfo,
	.get_locks = umidi_get_locks,
};

struct midi_hw_if_ext umidi_hw_if_ext = {
	.channel = umidi_channelmsg,
	.common  = umidi_commonmsg,
	.sysex   = umidi_sysex,
};

struct midi_hw_if_ext umidi_hw_if_mm = {
	.channel = umidi_channelmsg,
	.common  = umidi_commonmsg,
	.sysex   = umidi_sysex,
	.compress = 1,
};

int umidi_match(device_t, cfdata_t, void *);
void umidi_attach(device_t, device_t, void *);
void umidi_childdet(device_t, device_t);
int umidi_detach(device_t, int);
int umidi_activate(device_t, enum devact);
extern struct cfdriver umidi_cd;
CFATTACH_DECL2_NEW(umidi, sizeof(struct umidi_softc), umidi_match,
    umidi_attach, umidi_detach, umidi_activate, NULL, umidi_childdet);

int 
umidi_match(device_t parent, cfdata_t match, void *aux)
{
	struct usbif_attach_arg *uaa = aux;

	DPRINTFN(1,("umidi_match\n"));

	if (umidi_search_quirk(uaa->vendor, uaa->product, uaa->ifaceno))
		return UMATCH_IFACECLASS_IFACESUBCLASS;

	if (uaa->class == UICLASS_AUDIO &&
	    uaa->subclass == UISUBCLASS_MIDISTREAM)
		return UMATCH_IFACECLASS_IFACESUBCLASS;

	return UMATCH_NONE;
}

void 
umidi_attach(device_t parent, device_t self, void *aux)
{
	usbd_status     err;
	struct umidi_softc *sc = device_private(self);
	struct usbif_attach_arg *uaa = aux;
	char *devinfop;

	DPRINTFN(1,("umidi_attach\n"));

	sc->sc_dev = self;

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(uaa->device, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	sc->sc_iface = uaa->iface;
	sc->sc_udev = uaa->device;

	sc->sc_quirk =
	    umidi_search_quirk(uaa->vendor, uaa->product, uaa->ifaceno);
	aprint_normal_dev(self, "");
	umidi_print_quirk(sc->sc_quirk);

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_USB);
	cv_init(&sc->sc_cv, "umidopcl");
	cv_init(&sc->sc_detach_cv, "umidetcv");
	sc->sc_refcnt = 0;

	err = alloc_all_endpoints(sc);
	if (err != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(self,
		    "alloc_all_endpoints failed. (err=%d)\n", err);
		goto out;
	}
	err = alloc_all_jacks(sc);
	if (err != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(self, "alloc_all_jacks failed. (err=%d)\n",
		    err);
		goto out_free_endpoints;
	}
	aprint_normal_dev(self, "out=%d, in=%d\n",
	       sc->sc_out_num_jacks, sc->sc_in_num_jacks);

	err = assign_all_jacks_automatically(sc);
	if (err != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(self,
		    "assign_all_jacks_automatically failed. (err=%d)\n", err);
		goto out_free_jacks;
	}
	err = attach_all_mididevs(sc);
	if (err != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(self,
		    "attach_all_mididevs failed. (err=%d)\n", err);
		goto out_free_jacks;
	}

#ifdef UMIDI_DEBUG
	dump_sc(sc);
#endif

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH,
			   sc->sc_udev, sc->sc_dev);

	return;

out_free_jacks:
	unbind_all_jacks(sc);
	free_all_jacks(sc);
	
out_free_endpoints:
	free_all_endpoints(sc);

out:
	aprint_error_dev(self, "disabled.\n");
	sc->sc_dying = 1;
	KERNEL_UNLOCK_ONE(curlwp);
	return;
}

void
umidi_childdet(device_t self, device_t child)
{
	int i;
	struct umidi_softc *sc = device_private(self);

	KASSERT(sc->sc_mididevs != NULL);

	for (i = 0; i < sc->sc_num_mididevs; i++) {
		if (sc->sc_mididevs[i].mdev == child)
			break;
	}
	KASSERT(i < sc->sc_num_mididevs);
	sc->sc_mididevs[i].mdev = NULL;
}

int
umidi_activate(device_t self, enum devact act)
{
	struct umidi_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		DPRINTFN(1,("umidi_activate (deactivate)\n"));
		sc->sc_dying = 1;
		deactivate_all_mididevs(sc);
		return 0;
	default:
		DPRINTFN(1,("umidi_activate (%d)\n", act));
		return EOPNOTSUPP;
	}
}

int 
umidi_detach(device_t self, int flags)
{
	struct umidi_softc *sc = device_private(self);

	DPRINTFN(1,("umidi_detach\n"));

	mutex_enter(&sc->sc_lock);
	sc->sc_dying = 1;
	if (--sc->sc_refcnt >= 0)
		usb_detach_wait(sc->sc_dev, &sc->sc_detach_cv, &sc->sc_lock);
	mutex_exit(&sc->sc_lock);

	detach_all_mididevs(sc, flags);
	free_all_mididevs(sc);
	free_all_jacks(sc);
	free_all_endpoints(sc);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   sc->sc_dev);

	mutex_destroy(&sc->sc_lock);
	cv_destroy(&sc->sc_detach_cv);
	cv_destroy(&sc->sc_cv);

	return 0;
}


/*
 * midi_if stuffs
 */
int
umidi_open(void *addr,
	   int flags,
	   void (*iintr)(void *, int),
	   void (*ointr)(void *),
	   void *arg)
{
	struct umidi_mididev *mididev = addr;
	struct umidi_softc *sc = mididev->sc;
	usbd_status err;

	KASSERT(mutex_owned(&sc->sc_lock));
	DPRINTF(("umidi_open: sc=%p\n", sc));

	if (mididev->opened)
		return EBUSY;
	if (sc->sc_dying)
		return EIO;

	mididev->opened = 1;
	mididev->flags = flags;
	if ((mididev->flags & FWRITE) && mididev->out_jack) {
		err = open_out_jack(mididev->out_jack, arg, ointr);
		if (err != USBD_NORMAL_COMPLETION)
			goto bad;
	}
	if ((mididev->flags & FREAD) && mididev->in_jack) {
		err = open_in_jack(mididev->in_jack, arg, iintr);
		KASSERT(mididev->opened);
		if (err != USBD_NORMAL_COMPLETION &&
		    err != USBD_IN_PROGRESS) {
			if (mididev->out_jack)
				close_out_jack(mididev->out_jack);
			goto bad;
		}
	}

	return 0;
bad:
	mididev->opened = 0;
	DPRINTF(("umidi_open: usbd_status %d\n", err));
	KASSERT(mutex_owned(&sc->sc_lock));
	return USBD_IN_USE == err ? EBUSY : EIO;
}

void
umidi_close(void *addr)
{
	struct umidi_mididev *mididev = addr;
	struct umidi_softc *sc = mididev->sc;

	KASSERT(mutex_owned(&sc->sc_lock));

	if (mididev->closing)
		return;

	mididev->closing = 1;

	sc->sc_refcnt++;

	if ((mididev->flags & FWRITE) && mididev->out_jack)
		close_out_jack(mididev->out_jack);
	if ((mididev->flags & FREAD) && mididev->in_jack)
		close_in_jack(mididev->in_jack);

	if (--sc->sc_refcnt < 0)
		usb_detach_broadcast(sc->sc_dev, &sc->sc_detach_cv);

	mididev->opened = 0;
	mididev->closing = 0;
}

int
umidi_channelmsg(void *addr, int status, int channel, u_char *msg,
    int len)
{
	struct umidi_mididev *mididev = addr;

	KASSERT(mutex_owned(&mididev->sc->sc_lock));

	if (!mididev->out_jack || !mididev->opened || mididev->closing)
		return EIO;
	
	return out_jack_output(mididev->out_jack, msg, len, (status>>4)&0xf);
}

int
umidi_commonmsg(void *addr, int status, u_char *msg, int len)
{
	struct umidi_mididev *mididev = addr;
	int cin;

	KASSERT(mutex_owned(&mididev->sc->sc_lock));

	if (!mididev->out_jack || !mididev->opened || mididev->closing)
		return EIO;

	switch ( len ) {
	case 1: cin = 5; break;
	case 2: cin = 2; break;
	case 3: cin = 3; break;
	default: return EIO; /* or gcc warns of cin uninitialized */
	}
	
	return out_jack_output(mididev->out_jack, msg, len, cin);
}

int
umidi_sysex(void *addr, u_char *msg, int len)
{
	struct umidi_mididev *mididev = addr;
	int cin;

	KASSERT(mutex_owned(&mididev->sc->sc_lock));

	if (!mididev->out_jack || !mididev->opened || mididev->closing)
		return EIO;

	switch ( len ) {
	case 1: cin = 5; break;
	case 2: cin = 6; break;
	case 3: cin = (msg[2] == 0xf7) ? 7 : 4; break;
	default: return EIO; /* or gcc warns of cin uninitialized */
	}
	
	return out_jack_output(mididev->out_jack, msg, len, cin);
}

int
umidi_rtmsg(void *addr, int d)
{
	struct umidi_mididev *mididev = addr;
	u_char msg = d;

	KASSERT(mutex_owned(&mididev->sc->sc_lock));

	if (!mididev->out_jack || !mididev->opened || mididev->closing)
		return EIO;

	return out_jack_output(mididev->out_jack, &msg, 1, 0xf);
}

void
umidi_getinfo(void *addr, struct midi_info *mi)
{
	struct umidi_mididev *mididev = addr;
	struct umidi_softc *sc = mididev->sc;
	int mm = UMQ_ISTYPE(sc, UMQ_TYPE_MIDIMAN_GARBLE);

	KASSERT(mutex_owned(&sc->sc_lock));

	mi->name = mididev->label;
	mi->props = MIDI_PROP_OUT_INTR;
	if (mididev->in_jack)
		mi->props |= MIDI_PROP_CAN_INPUT;
	midi_register_hw_if_ext(mm? &umidi_hw_if_mm : &umidi_hw_if_ext);
}

static void
umidi_get_locks(void *addr, kmutex_t **thread, kmutex_t **intr)
{
	struct umidi_mididev *mididev = addr;
	struct umidi_softc *sc = mididev->sc;

	*intr = NULL;
	*thread = &sc->sc_lock;
}

/*
 * each endpoint stuffs
 */

/* alloc/free pipe */
static usbd_status
alloc_pipe(struct umidi_endpoint *ep)
{
	struct umidi_softc *sc = ep->sc;
	usbd_status err;
	usb_endpoint_descriptor_t *epd;
	
	epd = usbd_get_endpoint_descriptor(sc->sc_iface, ep->addr);
	/*
	 * For output, an improvement would be to have a buffer bigger than
	 * wMaxPacketSize by num_jacks-1 additional packet slots; that would
	 * allow out_solicit to fill the buffer to the full packet size in
	 * all cases. But to use usbd_alloc_buffer to get a slightly larger
	 * buffer would not be a good way to do that, because if the addition
	 * would make the buffer exceed USB_MEM_SMALL then a substantially
	 * larger block may be wastefully allocated. Some flavor of double
	 * buffering could serve the same purpose, but would increase the
	 * code complexity, so for now I will live with the current slight
	 * penalty of reducing max transfer size by (num_open-num_scheduled)
	 * packet slots.
	 */
	ep->buffer_size = UGETW(epd->wMaxPacketSize);
	ep->buffer_size -= ep->buffer_size % UMIDI_PACKET_SIZE;

	DPRINTF(("%s: alloc_pipe %p, buffer size %u\n",
	        device_xname(sc->sc_dev), ep, ep->buffer_size));
	ep->num_scheduled = 0;
	ep->this_schedule = 0;
	ep->next_schedule = 0;
	ep->soliciting = 0;
	ep->armed = 0;
	ep->xfer = usbd_alloc_xfer(sc->sc_udev);
	if (ep->xfer == NULL) {
	    err = USBD_NOMEM;
	    goto quit;
	}
	ep->buffer = usbd_alloc_buffer(ep->xfer, ep->buffer_size);
	if (ep->buffer == NULL) {
	    usbd_free_xfer(ep->xfer);
	    err = USBD_NOMEM;
	    goto quit;
	}
	ep->next_slot = ep->buffer;
	err = usbd_open_pipe(sc->sc_iface, ep->addr, USBD_MPSAFE, &ep->pipe);
	if (err)
	    usbd_free_xfer(ep->xfer);
	ep->solicit_cookie = softint_establish(SOFTINT_CLOCK | SOFTINT_MPSAFE, out_solicit, ep);
quit:
	return err;
}

static void
free_pipe(struct umidi_endpoint *ep)
{
	DPRINTF(("%s: free_pipe %p\n", device_xname(ep->sc->sc_dev), ep));
	usbd_abort_pipe(ep->pipe);
	usbd_close_pipe(ep->pipe);
	usbd_free_xfer(ep->xfer);
	softint_disestablish(ep->solicit_cookie);
}


/* alloc/free the array of endpoint structures */

static usbd_status alloc_all_endpoints_fixed_ep(struct umidi_softc *);
static usbd_status alloc_all_endpoints_yamaha(struct umidi_softc *);
static usbd_status alloc_all_endpoints_genuine(struct umidi_softc *);

static usbd_status
alloc_all_endpoints(struct umidi_softc *sc)
{
	usbd_status err;
	struct umidi_endpoint *ep;
	int i;

	if (UMQ_ISTYPE(sc, UMQ_TYPE_FIXED_EP)) {
		err = alloc_all_endpoints_fixed_ep(sc);
	} else if (UMQ_ISTYPE(sc, UMQ_TYPE_YAMAHA)) {
		err = alloc_all_endpoints_yamaha(sc);
	} else {
		err = alloc_all_endpoints_genuine(sc);
	}
	if (err != USBD_NORMAL_COMPLETION)
		return err;

	ep = sc->sc_endpoints;
	for (i = sc->sc_out_num_endpoints+sc->sc_in_num_endpoints; i > 0; i--) {
		err = alloc_pipe(ep++);
		if (err != USBD_NORMAL_COMPLETION) {
			for (; ep != sc->sc_endpoints; ep--)
				free_pipe(ep-1);
			kmem_free(sc->sc_endpoints, sc->sc_endpoints_len);
			sc->sc_endpoints = sc->sc_out_ep = sc->sc_in_ep = NULL;
			break;
		}
	}
	return err;
}

static void
free_all_endpoints(struct umidi_softc *sc)
{
	int i;

	for (i=0; i<sc->sc_in_num_endpoints+sc->sc_out_num_endpoints; i++)
		free_pipe(&sc->sc_endpoints[i]);
	if (sc->sc_endpoints != NULL)
		kmem_free(sc->sc_endpoints, sc->sc_endpoints_len);
	sc->sc_endpoints = sc->sc_out_ep = sc->sc_in_ep = NULL;
}

static usbd_status
alloc_all_endpoints_fixed_ep(struct umidi_softc *sc)
{
	usbd_status err;
	const struct umq_fixed_ep_desc *fp;
	struct umidi_endpoint *ep;
	usb_endpoint_descriptor_t *epd;
	int i;

	fp = umidi_get_quirk_data_from_type(sc->sc_quirk,
					    UMQ_TYPE_FIXED_EP);
	sc->sc_out_num_jacks = 0;
	sc->sc_in_num_jacks = 0;
	sc->sc_out_num_endpoints = fp->num_out_ep;
	sc->sc_in_num_endpoints = fp->num_in_ep;
	sc->sc_endpoints_len = UMIDI_ENDPOINT_SIZE(sc);
	sc->sc_endpoints = kmem_zalloc(sc->sc_endpoints_len, KM_SLEEP);
	if (!sc->sc_endpoints)
		return USBD_NOMEM;

	sc->sc_out_ep = sc->sc_out_num_endpoints ? sc->sc_endpoints : NULL;
	sc->sc_in_ep =
	    sc->sc_in_num_endpoints ?
		sc->sc_endpoints+sc->sc_out_num_endpoints : NULL;

	ep = &sc->sc_out_ep[0];
	for (i = 0; i < sc->sc_out_num_endpoints; i++) {
		epd = usbd_interface2endpoint_descriptor(
			sc->sc_iface,
			fp->out_ep[i].ep);
		if (!epd) {
			aprint_error_dev(sc->sc_dev,
			    "cannot get endpoint descriptor(out:%d)\n",
			     fp->out_ep[i].ep);
			err = USBD_INVAL;
			goto error;
		}
		if (UE_GET_XFERTYPE(epd->bmAttributes)!=UE_BULK ||
		    UE_GET_DIR(epd->bEndpointAddress)!=UE_DIR_OUT) {
			aprint_error_dev(sc->sc_dev, "illegal endpoint(out:%d)\n",
			    fp->out_ep[i].ep);
			err = USBD_INVAL;
			goto error;
		}
		ep->sc = sc;
		ep->addr = epd->bEndpointAddress;
		ep->num_jacks = fp->out_ep[i].num_jacks;
		sc->sc_out_num_jacks += fp->out_ep[i].num_jacks;
		ep->num_open = 0;
		ep++;
	}
	ep = &sc->sc_in_ep[0];
	for (i = 0; i < sc->sc_in_num_endpoints; i++) {
		epd = usbd_interface2endpoint_descriptor(
			sc->sc_iface,
			fp->in_ep[i].ep);
		if (!epd) {
			aprint_error_dev(sc->sc_dev,
			    "cannot get endpoint descriptor(in:%d)\n",
			     fp->in_ep[i].ep);
			err = USBD_INVAL;
			goto error;
		}
		/*
		 * MIDISPORT_2X4 inputs on an interrupt rather than a bulk
		 * endpoint.  The existing input logic in this driver seems
		 * to work successfully if we just stop treating an interrupt
		 * endpoint as illegal (or the in_progress status we get on
		 * the initial transfer).  It does not seem necessary to
		 * actually use the interrupt flavor of alloc_pipe or make
		 * other serious rearrangements of logic.  I like that.
		 */
		switch ( UE_GET_XFERTYPE(epd->bmAttributes) ) {
		case UE_BULK:
		case UE_INTERRUPT:
			if (UE_DIR_IN == UE_GET_DIR(epd->bEndpointAddress))
				break;
			/*FALLTHROUGH*/
		default:
			aprint_error_dev(sc->sc_dev,
			    "illegal endpoint(in:%d)\n", fp->in_ep[i].ep);
			err = USBD_INVAL;
			goto error;
		}

		ep->sc = sc;
		ep->addr = epd->bEndpointAddress;
		ep->num_jacks = fp->in_ep[i].num_jacks;
		sc->sc_in_num_jacks += fp->in_ep[i].num_jacks;
		ep->num_open = 0;
		ep++;
	}

	return USBD_NORMAL_COMPLETION;
error:
	kmem_free(sc->sc_endpoints, UMIDI_ENDPOINT_SIZE(sc));
	sc->sc_endpoints = NULL;
	return err;
}

static usbd_status
alloc_all_endpoints_yamaha(struct umidi_softc *sc)
{
	/* This driver currently supports max 1in/1out bulk endpoints */
	usb_descriptor_t *desc;
	umidi_cs_descriptor_t *udesc;
	usb_endpoint_descriptor_t *epd;
	int out_addr, in_addr, i;
	int dir;
	size_t remain, descsize;

	sc->sc_out_num_jacks = sc->sc_in_num_jacks = 0;
	out_addr = in_addr = 0;

	/* detect endpoints */
	desc = TO_D(usbd_get_interface_descriptor(sc->sc_iface));
	for (i=(int)TO_IFD(desc)->bNumEndpoints-1; i>=0; i--) {
		epd = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		KASSERT(epd != NULL);
		if (UE_GET_XFERTYPE(epd->bmAttributes) == UE_BULK) {
			dir = UE_GET_DIR(epd->bEndpointAddress);
			if (dir==UE_DIR_OUT && !out_addr)
				out_addr = epd->bEndpointAddress;
			else if (dir==UE_DIR_IN && !in_addr)
				in_addr = epd->bEndpointAddress;
		}
	}
	udesc = (umidi_cs_descriptor_t *)NEXT_D(desc);

	/* count jacks */
	if (!(udesc->bDescriptorType==UDESC_CS_INTERFACE &&
	      udesc->bDescriptorSubtype==UMIDI_MS_HEADER))
		return USBD_INVAL;
	remain = (size_t)UGETW(TO_CSIFD(udesc)->wTotalLength) -
		(size_t)udesc->bLength;
	udesc = (umidi_cs_descriptor_t *)NEXT_D(udesc);

	while (remain >= sizeof(usb_descriptor_t)) {
		descsize = udesc->bLength;
		if (descsize>remain || descsize==0)
			break;
		if (udesc->bDescriptorType == UDESC_CS_INTERFACE &&
		    remain >= UMIDI_JACK_DESCRIPTOR_SIZE) {
			if (udesc->bDescriptorSubtype == UMIDI_OUT_JACK)
				sc->sc_out_num_jacks++;
			else if (udesc->bDescriptorSubtype == UMIDI_IN_JACK)
				sc->sc_in_num_jacks++;
		}
		udesc = (umidi_cs_descriptor_t *)NEXT_D(udesc);
		remain -= descsize;
	}

	/* validate some parameters */
	if (sc->sc_out_num_jacks>UMIDI_MAX_EPJACKS)
		sc->sc_out_num_jacks = UMIDI_MAX_EPJACKS;
	if (sc->sc_in_num_jacks>UMIDI_MAX_EPJACKS)
		sc->sc_in_num_jacks = UMIDI_MAX_EPJACKS;
	if (sc->sc_out_num_jacks && out_addr) {
		sc->sc_out_num_endpoints = 1;
	} else {
		sc->sc_out_num_endpoints = 0;
		sc->sc_out_num_jacks = 0;
	}
	if (sc->sc_in_num_jacks && in_addr) {
		sc->sc_in_num_endpoints = 1;
	} else {
		sc->sc_in_num_endpoints = 0;
		sc->sc_in_num_jacks = 0;
	}
	sc->sc_endpoints_len = UMIDI_ENDPOINT_SIZE(sc);
	sc->sc_endpoints = kmem_zalloc(sc->sc_endpoints_len, KM_SLEEP);
	if (!sc->sc_endpoints)
		return USBD_NOMEM;
	if (sc->sc_out_num_endpoints) {
		sc->sc_out_ep = sc->sc_endpoints;
		sc->sc_out_ep->sc = sc;
		sc->sc_out_ep->addr = out_addr;
		sc->sc_out_ep->num_jacks = sc->sc_out_num_jacks;
		sc->sc_out_ep->num_open = 0;
	} else
		sc->sc_out_ep = NULL;

	if (sc->sc_in_num_endpoints) {
		sc->sc_in_ep = sc->sc_endpoints+sc->sc_out_num_endpoints;
		sc->sc_in_ep->sc = sc;
		sc->sc_in_ep->addr = in_addr;
		sc->sc_in_ep->num_jacks = sc->sc_in_num_jacks;
		sc->sc_in_ep->num_open = 0;
	} else
		sc->sc_in_ep = NULL;

	return USBD_NORMAL_COMPLETION;
}

static usbd_status
alloc_all_endpoints_genuine(struct umidi_softc *sc)
{
	usb_interface_descriptor_t *interface_desc;
	usb_config_descriptor_t *config_desc;
	usb_descriptor_t *desc;
	int num_ep;
	size_t remain, descsize;
	struct umidi_endpoint *p, *q, *lowest, *endep, tmpep;
	int epaddr;

	interface_desc = usbd_get_interface_descriptor(sc->sc_iface);
	num_ep = interface_desc->bNumEndpoints;
	sc->sc_endpoints_len = sizeof(struct umidi_endpoint) * num_ep;
	sc->sc_endpoints = p = kmem_zalloc(sc->sc_endpoints_len, KM_SLEEP);
	if (!p)
		return USBD_NOMEM;

	sc->sc_out_num_jacks = sc->sc_in_num_jacks = 0;
	sc->sc_out_num_endpoints = sc->sc_in_num_endpoints = 0;
	epaddr = -1;

	/* get the list of endpoints for midi stream */
	config_desc = usbd_get_config_descriptor(sc->sc_udev);
	desc = (usb_descriptor_t *) config_desc;
	remain = (size_t)UGETW(config_desc->wTotalLength);
	while (remain>=sizeof(usb_descriptor_t)) {
		descsize = desc->bLength;
		if (descsize>remain || descsize==0)
			break;
		if (desc->bDescriptorType==UDESC_ENDPOINT &&
		    remain>=USB_ENDPOINT_DESCRIPTOR_SIZE &&
		    UE_GET_XFERTYPE(TO_EPD(desc)->bmAttributes) == UE_BULK) {
			epaddr = TO_EPD(desc)->bEndpointAddress;
		} else if (desc->bDescriptorType==UDESC_CS_ENDPOINT &&
			   remain>=UMIDI_CS_ENDPOINT_DESCRIPTOR_SIZE &&
			   epaddr!=-1) {
			if (num_ep>0) {
				num_ep--;
				p->sc = sc;
				p->addr = epaddr;
				p->num_jacks = TO_CSEPD(desc)->bNumEmbMIDIJack;
				if (UE_GET_DIR(epaddr)==UE_DIR_OUT) {
					sc->sc_out_num_endpoints++;
					sc->sc_out_num_jacks += p->num_jacks;
				} else {
					sc->sc_in_num_endpoints++;
					sc->sc_in_num_jacks += p->num_jacks;
				}
				p++;
			}
		} else
			epaddr = -1;
		desc = NEXT_D(desc);
		remain-=descsize;
	}

	/* sort endpoints */
	num_ep = sc->sc_out_num_endpoints + sc->sc_in_num_endpoints;
	p = sc->sc_endpoints;
	endep = p + num_ep;
	while (p<endep) {
		lowest = p;
		for (q=p+1; q<endep; q++) {
			if ((UE_GET_DIR(lowest->addr)==UE_DIR_IN &&
			     UE_GET_DIR(q->addr)==UE_DIR_OUT) ||
			    ((UE_GET_DIR(lowest->addr)==
			      UE_GET_DIR(q->addr)) &&
			     (UE_GET_ADDR(lowest->addr)>
			      UE_GET_ADDR(q->addr))))
				lowest = q;
		}
		if (lowest != p) {
			memcpy((void *)&tmpep, (void *)p, sizeof(tmpep));
			memcpy((void *)p, (void *)lowest, sizeof(tmpep));
			memcpy((void *)lowest, (void *)&tmpep, sizeof(tmpep));
		}
		p->num_open = 0;
		p++;
	}

	sc->sc_out_ep = sc->sc_out_num_endpoints ? sc->sc_endpoints : NULL;
	sc->sc_in_ep =
	    sc->sc_in_num_endpoints ?
		sc->sc_endpoints+sc->sc_out_num_endpoints : NULL;

	return USBD_NORMAL_COMPLETION;
}


/*
 * jack stuffs
 */

static usbd_status
alloc_all_jacks(struct umidi_softc *sc)
{
	int i, j;
	struct umidi_endpoint *ep;
	struct umidi_jack *jack;
	const unsigned char *cn_spec;
	
	if (UMQ_ISTYPE(sc, UMQ_TYPE_CN_SEQ_PER_EP))
		sc->cblnums_global = 0;
	else if (UMQ_ISTYPE(sc, UMQ_TYPE_CN_SEQ_GLOBAL))
		sc->cblnums_global = 1;
	else {
		/*
		 * I don't think this default is correct, but it preserves
		 * the prior behavior of the code. That's why I defined two
		 * complementary quirks. Any device for which the default
		 * behavior is wrong can be made to work by giving it an
		 * explicit quirk, and if a pattern ever develops (as I suspect
		 * it will) that a lot of otherwise standard USB MIDI devices
		 * need the CN_SEQ_PER_EP "quirk," then this default can be
		 * changed to 0, and the only devices that will break are those
		 * listing neither quirk, and they'll easily be fixed by giving
		 * them the CN_SEQ_GLOBAL quirk.
		 */
		sc->cblnums_global = 1;
	}
	
	if (UMQ_ISTYPE(sc, UMQ_TYPE_CN_FIXED))
		cn_spec = umidi_get_quirk_data_from_type(sc->sc_quirk,
					    		 UMQ_TYPE_CN_FIXED);
	else
		cn_spec = NULL;

	/* allocate/initialize structures */
	sc->sc_jacks = kmem_zalloc(sizeof(*sc->sc_out_jacks)*(sc->sc_in_num_jacks+
						      sc->sc_out_num_jacks), KM_SLEEP);
	if (!sc->sc_jacks)
		return USBD_NOMEM;
	sc->sc_out_jacks =
	    sc->sc_out_num_jacks ? sc->sc_jacks : NULL;
	sc->sc_in_jacks =
	    sc->sc_in_num_jacks ? sc->sc_jacks+sc->sc_out_num_jacks : NULL;

	jack = &sc->sc_out_jacks[0];
	for (i = 0; i < sc->sc_out_num_jacks; i++) {
		jack->opened = 0;
		jack->bound = 0;
		jack->arg = NULL;
		jack->u.out.intr = NULL;
		jack->midiman_ppkt = NULL;
		if (sc->cblnums_global)
			jack->cable_number = i;
		jack++;
	}
	jack = &sc->sc_in_jacks[0];
	for (i = 0; i < sc->sc_in_num_jacks; i++) {
		jack->opened = 0;
		jack->bound = 0;
		jack->arg = NULL;
		jack->u.in.intr = NULL;
		if (sc->cblnums_global)
			jack->cable_number = i;
		jack++;
	}

	/* assign each jacks to each endpoints */
	jack = &sc->sc_out_jacks[0];
	ep = &sc->sc_out_ep[0];
	for (i = 0; i < sc->sc_out_num_endpoints; i++) {
		for (j = 0; j < ep->num_jacks; j++) {
			jack->endpoint = ep;
			if (cn_spec != NULL)
				jack->cable_number = *cn_spec++;
			else if (!sc->cblnums_global)
				jack->cable_number = j;
			ep->jacks[jack->cable_number] = jack;
			jack++;
		}
		ep++;
	}
	jack = &sc->sc_in_jacks[0];
	ep = &sc->sc_in_ep[0];
	for (i = 0; i < sc->sc_in_num_endpoints; i++) {
		for (j = 0; j < ep->num_jacks; j++) {
			jack->endpoint = ep;
			if (cn_spec != NULL)
				jack->cable_number = *cn_spec++;
			else if (!sc->cblnums_global)
				jack->cable_number = j;
			ep->jacks[jack->cable_number] = jack;
			jack++;
		}
		ep++;
	}

	return USBD_NORMAL_COMPLETION;
}

static void
free_all_jacks(struct umidi_softc *sc)
{
	struct umidi_jack *jacks;
	size_t len;

	mutex_enter(&sc->sc_lock);
	jacks = sc->sc_jacks;
	len = sizeof(*sc->sc_out_jacks)*(sc->sc_in_num_jacks+sc->sc_out_num_jacks);
	sc->sc_jacks = sc->sc_in_jacks = sc->sc_out_jacks = NULL;
	mutex_exit(&sc->sc_lock);

	if (jacks)
		kmem_free(jacks, len);
}

static usbd_status
bind_jacks_to_mididev(struct umidi_softc *sc,
		      struct umidi_jack *out_jack,
		      struct umidi_jack *in_jack,
		      struct umidi_mididev *mididev)
{
	if ((out_jack && out_jack->bound) || (in_jack && in_jack->bound))
		return USBD_IN_USE;
	if (mididev->out_jack || mididev->in_jack)
		return USBD_IN_USE;

	if (out_jack)
		out_jack->bound = 1;
	if (in_jack)
		in_jack->bound = 1;
	mididev->in_jack = in_jack;
	mididev->out_jack = out_jack;

	mididev->closing = 0;

	return USBD_NORMAL_COMPLETION;
}

static void
unbind_jacks_from_mididev(struct umidi_mididev *mididev)
{
	KASSERT(mutex_owned(&mididev->sc->sc_lock));

	mididev->closing = 1;

	if ((mididev->flags & FWRITE) && mididev->out_jack)
		close_out_jack(mididev->out_jack);
	if ((mididev->flags & FREAD) && mididev->in_jack)
		close_in_jack(mididev->in_jack);

	if (mididev->out_jack) {
		mididev->out_jack->bound = 0;
		mididev->out_jack = NULL;
	}
	if (mididev->in_jack) {
		mididev->in_jack->bound = 0;
		mididev->in_jack = NULL;
	}
}

static void
unbind_all_jacks(struct umidi_softc *sc)
{
	int i;

	mutex_spin_enter(&sc->sc_lock);
	if (sc->sc_mididevs)
		for (i = 0; i < sc->sc_num_mididevs; i++)
			unbind_jacks_from_mididev(&sc->sc_mididevs[i]);
	mutex_spin_exit(&sc->sc_lock);
}

static usbd_status
assign_all_jacks_automatically(struct umidi_softc *sc)
{
	usbd_status err;
	int i;
	struct umidi_jack *out, *in;
	const signed char *asg_spec;

	err =
	    alloc_all_mididevs(sc,
			       max(sc->sc_out_num_jacks, sc->sc_in_num_jacks));
	if (err!=USBD_NORMAL_COMPLETION)
		return err;

	if (UMQ_ISTYPE(sc, UMQ_TYPE_MD_FIXED))
		asg_spec = umidi_get_quirk_data_from_type(sc->sc_quirk,
					    		  UMQ_TYPE_MD_FIXED);
	else
		asg_spec = NULL;

	for (i = 0; i < sc->sc_num_mididevs; i++) {
		if (asg_spec != NULL) {
			if (*asg_spec == -1)
				out = NULL;
			else
				out = &sc->sc_out_jacks[*asg_spec];
			++ asg_spec;
			if (*asg_spec == -1)
				in = NULL;
			else
				in = &sc->sc_in_jacks[*asg_spec];
			++ asg_spec;
		} else {
			out = (i<sc->sc_out_num_jacks) ? &sc->sc_out_jacks[i]
			                               : NULL;
			in = (i<sc->sc_in_num_jacks) ? &sc->sc_in_jacks[i]
						     : NULL;
		}
		err = bind_jacks_to_mididev(sc, out, in, &sc->sc_mididevs[i]);
		if (err != USBD_NORMAL_COMPLETION) {
			free_all_mididevs(sc);
			return err;
		}
	}

	return USBD_NORMAL_COMPLETION;
}

static usbd_status
open_out_jack(struct umidi_jack *jack, void *arg, void (*intr)(void *))
{
	struct umidi_endpoint *ep = jack->endpoint;
	struct umidi_softc *sc = ep->sc;
	umidi_packet_bufp end;
	int err;

	KASSERT(mutex_owned(&sc->sc_lock));

	if (jack->opened)
		return USBD_IN_USE;

	jack->arg = arg;
	jack->u.out.intr = intr;
	jack->midiman_ppkt = NULL;
	end = ep->buffer + ep->buffer_size / sizeof *ep->buffer;
	jack->opened = 1;
	ep->num_open++;
	/*
	 * out_solicit maintains an invariant that there will always be
	 * (num_open - num_scheduled) slots free in the buffer. as we have
	 * just incremented num_open, the buffer may be too full to satisfy
	 * the invariant until a transfer completes, for which we must wait.
	 */
	while (end - ep->next_slot < ep->num_open - ep->num_scheduled) {
		err = cv_timedwait_sig(&sc->sc_cv, &sc->sc_lock,
		     mstohz(10));
		if (err) {
			ep->num_open--;
			jack->opened = 0;
			return USBD_IOERROR;
		}
	}

	return USBD_NORMAL_COMPLETION;
}

static usbd_status
open_in_jack(struct umidi_jack *jack, void *arg, void (*intr)(void *, int))
{
	usbd_status err = USBD_NORMAL_COMPLETION;
	struct umidi_endpoint *ep = jack->endpoint;

	KASSERT(mutex_owned(&ep->sc->sc_lock));

	if (jack->opened)
		return USBD_IN_USE;

	jack->arg = arg;
	jack->u.in.intr = intr;
	jack->opened = 1;
	if (ep->num_open++ == 0 && UE_GET_DIR(ep->addr)==UE_DIR_IN) {
		/*
		 * Can't hold the interrupt lock while calling into USB,
		 * but we can safely drop it here.
		 */
		mutex_exit(&ep->sc->sc_lock);
		err = start_input_transfer(ep);
		if (err != USBD_NORMAL_COMPLETION &&
		    err != USBD_IN_PROGRESS) {
			ep->num_open--;
		}
		mutex_enter(&ep->sc->sc_lock);
	}

	return err;
}

static void
close_out_jack(struct umidi_jack *jack)
{
	struct umidi_endpoint *ep;
	struct umidi_softc *sc;
	u_int16_t mask;
	int err;

	if (jack->opened) {
		ep = jack->endpoint;
		sc = ep->sc;

		KASSERT(mutex_owned(&sc->sc_lock));
		mask = 1 << (jack->cable_number);
		while (mask & (ep->this_schedule | ep->next_schedule)) {
			err = cv_timedwait_sig(&sc->sc_cv, &sc->sc_lock,
			     mstohz(10));
			if (err)
				break;
		}
		/*
		 * We can re-enter this function from both close() and
		 * detach().  Make sure only one of them does this part.
		 */
		if (jack->opened) {
			jack->opened = 0;
			jack->endpoint->num_open--;
			ep->this_schedule &= ~mask;
			ep->next_schedule &= ~mask;
		}
	}
}

static void
close_in_jack(struct umidi_jack *jack)
{
	if (jack->opened) {
		struct umidi_softc *sc = jack->endpoint->sc;

		KASSERT(mutex_owned(&sc->sc_lock));

		jack->opened = 0;
		if (--jack->endpoint->num_open == 0) {
			/*
			 * We have to drop the (interrupt) lock so that
			 * the USB thread lock can be safely taken by
			 * the abort operation.  This is safe as this
			 * either closing or dying will be set proerly.
			 */
			mutex_spin_exit(&sc->sc_lock);
			usbd_abort_pipe(jack->endpoint->pipe);
			mutex_spin_enter(&sc->sc_lock);
		}
	}
}

static usbd_status
attach_mididev(struct umidi_softc *sc, struct umidi_mididev *mididev)
{
	if (mididev->sc)
		return USBD_IN_USE;

	mididev->sc = sc;
	
	describe_mididev(mididev);

	mididev->mdev = midi_attach_mi(&umidi_hw_if, mididev, sc->sc_dev);

	return USBD_NORMAL_COMPLETION;
}

static usbd_status
detach_mididev(struct umidi_mididev *mididev, int flags)
{
	struct umidi_softc *sc = mididev->sc;

	if (!sc)
		return USBD_NO_ADDR;

	mutex_spin_enter(&sc->sc_lock);
	if (mididev->opened) {
		umidi_close(mididev);
	}
	unbind_jacks_from_mididev(mididev);
	mutex_spin_exit(&sc->sc_lock);

	if (mididev->mdev != NULL)
		config_detach(mididev->mdev, flags);
	
	if (NULL != mididev->label) {
		kmem_free(mididev->label, mididev->label_len);
		mididev->label = NULL;
	}

	mididev->sc = NULL;

	return USBD_NORMAL_COMPLETION;
}

static void
deactivate_mididev(struct umidi_mididev *mididev)
{
	if (mididev->out_jack)
		mididev->out_jack->bound = 0;
	if (mididev->in_jack)
		mididev->in_jack->bound = 0;
}

static usbd_status
alloc_all_mididevs(struct umidi_softc *sc, int nmidi)
{
	sc->sc_num_mididevs = nmidi;
	sc->sc_mididevs = kmem_zalloc(sizeof(*sc->sc_mididevs)*nmidi, KM_SLEEP);
	if (!sc->sc_mididevs)
		return USBD_NOMEM;

	return USBD_NORMAL_COMPLETION;
}

static void
free_all_mididevs(struct umidi_softc *sc)
{
	struct umidi_mididev *mididevs;
	size_t len;

	mutex_enter(&sc->sc_lock);
	mididevs = sc->sc_mididevs;
	if (mididevs)
		  len = sizeof(*sc->sc_mididevs )* sc->sc_num_mididevs;
	sc->sc_mididevs = NULL;
	sc->sc_num_mididevs = 0;
	mutex_exit(&sc->sc_lock);

	if (mididevs)
		kmem_free(mididevs, len);
}

static usbd_status
attach_all_mididevs(struct umidi_softc *sc)
{
	usbd_status err;
	int i;

	if (sc->sc_mididevs)
		for (i = 0; i < sc->sc_num_mididevs; i++) {
			err = attach_mididev(sc, &sc->sc_mididevs[i]);
			if (err != USBD_NORMAL_COMPLETION)
				return err;
		}

	return USBD_NORMAL_COMPLETION;
}

static usbd_status
detach_all_mididevs(struct umidi_softc *sc, int flags)
{
	usbd_status err;
	int i;

	if (sc->sc_mididevs)
		for (i = 0; i < sc->sc_num_mididevs; i++) {
			err = detach_mididev(&sc->sc_mididevs[i], flags);
			if (err != USBD_NORMAL_COMPLETION)
				return err;
		}

	return USBD_NORMAL_COMPLETION;
}

static void
deactivate_all_mididevs(struct umidi_softc *sc)
{
	int i;

	if (sc->sc_mididevs) {
		for (i = 0; i < sc->sc_num_mididevs; i++)
			deactivate_mididev(&sc->sc_mididevs[i]);
	}
}

/*
 * TODO: the 0-based cable numbers will often not match the labeling of the
 * equipment. Ideally:
 *  For class-compliant devices: get the iJack string from the jack descriptor.
 *  Otherwise:
 *  - support a DISPLAY_BASE_CN quirk (add the value to each internal cable
 *    number for display)
 *  - support an array quirk explictly giving a char * for each jack.
 * For now, you get 0-based cable numbers. If there are multiple endpoints and
 * the CNs are not globally unique, each is shown with its associated endpoint
 * address in hex also. That should not be necessary when using iJack values
 * or a quirk array.
 */
void
describe_mididev(struct umidi_mididev *md)
{
	char in_label[16];
	char out_label[16];
	const char *unit_label;
	char *final_label;
	struct umidi_softc *sc;
	int show_ep_in;
	int show_ep_out;
	size_t len;
	
	sc = md->sc;
	show_ep_in  = sc-> sc_in_num_endpoints > 1 && !sc->cblnums_global;
	show_ep_out = sc->sc_out_num_endpoints > 1 && !sc->cblnums_global;
	
	if (NULL == md->in_jack)
		in_label[0] = '\0';
	else if (show_ep_in)
		snprintf(in_label, sizeof in_label, "<%d(%x) ",
		    md->in_jack->cable_number, md->in_jack->endpoint->addr);
	else
		snprintf(in_label, sizeof in_label, "<%d ",
		    md->in_jack->cable_number);
	
	if (NULL == md->out_jack)
		out_label[0] = '\0';
	else if (show_ep_out)
		snprintf(out_label, sizeof out_label, ">%d(%x) ",
		    md->out_jack->cable_number, md->out_jack->endpoint->addr);
	else
		snprintf(out_label, sizeof out_label, ">%d ",
		    md->out_jack->cable_number);

	unit_label = device_xname(sc->sc_dev);
	
	len = strlen(in_label) + strlen(out_label) + strlen(unit_label) + 4;
	
	final_label = kmem_alloc(len, KM_SLEEP);
	
	snprintf(final_label, len, "%s%son %s",
	    in_label, out_label, unit_label);

	md->label = final_label;
	md->label_len = len;
}

#ifdef UMIDI_DEBUG
static void
dump_sc(struct umidi_softc *sc)
{
	int i;

	DPRINTFN(10, ("%s: dump_sc\n", device_xname(sc->sc_dev)));
	for (i=0; i<sc->sc_out_num_endpoints; i++) {
		DPRINTFN(10, ("\tout_ep(%p):\n", &sc->sc_out_ep[i]));
		dump_ep(&sc->sc_out_ep[i]);
	}
	for (i=0; i<sc->sc_in_num_endpoints; i++) {
		DPRINTFN(10, ("\tin_ep(%p):\n", &sc->sc_in_ep[i]));
		dump_ep(&sc->sc_in_ep[i]);
	}
}

static void
dump_ep(struct umidi_endpoint *ep)
{
	int i;
	for (i=0; i<UMIDI_MAX_EPJACKS; i++) {
		if (NULL==ep->jacks[i])
			continue;
		DPRINTFN(10, ("\t\tjack[%d]:%p:\n", i, ep->jacks[i]));
		dump_jack(ep->jacks[i]);
	}
}
static void
dump_jack(struct umidi_jack *jack)
{
	DPRINTFN(10, ("\t\t\tep=%p\n",
		      jack->endpoint));
}

#endif /* UMIDI_DEBUG */



/*
 * MUX MIDI PACKET
 */

static const int packet_length[16] = {
	/*0*/	-1,
	/*1*/	-1,
	/*2*/	2,
	/*3*/	3,
	/*4*/	3,
	/*5*/	1,
	/*6*/	2,
	/*7*/	3,
	/*8*/	3,
	/*9*/	3,
	/*A*/	3,
	/*B*/	3,
	/*C*/	2,
	/*D*/	2,
	/*E*/	3,
	/*F*/	1,
};

#define	GET_CN(p)		(((unsigned char)(p)>>4)&0x0F)
#define GET_CIN(p)		((unsigned char)(p)&0x0F)
#define MIX_CN_CIN(cn, cin) \
	((unsigned char)((((unsigned char)(cn)&0x0F)<<4)| \
			  ((unsigned char)(cin)&0x0F)))

static usbd_status
start_input_transfer(struct umidi_endpoint *ep)
{
	usbd_setup_xfer(ep->xfer, ep->pipe,
			(usbd_private_handle)ep,
			ep->buffer, ep->buffer_size,
			USBD_SHORT_XFER_OK | USBD_NO_COPY,
                        USBD_NO_TIMEOUT, in_intr);
	return usbd_transfer(ep->xfer);
}

static usbd_status
start_output_transfer(struct umidi_endpoint *ep)
{
	usbd_status rv;
	u_int32_t length;
	int i;
	
	length = (ep->next_slot - ep->buffer) * sizeof *ep->buffer;
	DPRINTFN(200,("umidi out transfer: start %p end %p length %u\n",
	    ep->buffer, ep->next_slot, length));
	usbd_setup_xfer(ep->xfer, ep->pipe,
			(usbd_private_handle)ep,
			ep->buffer, length,
			USBD_NO_COPY, USBD_NO_TIMEOUT, out_intr);
	rv = usbd_transfer(ep->xfer);
	
	/*
	 * Once the transfer is scheduled, no more adding to partial
	 * packets within it.
	 */
	if (UMQ_ISTYPE(ep->sc, UMQ_TYPE_MIDIMAN_GARBLE)) {
		for (i=0; i<UMIDI_MAX_EPJACKS; ++i)
			if (NULL != ep->jacks[i])
				ep->jacks[i]->midiman_ppkt = NULL;
	}
	
	return rv;
}

#ifdef UMIDI_DEBUG
#define DPR_PACKET(dir, sc, p)						\
if ((unsigned char)(p)[1]!=0xFE)				\
	DPRINTFN(500,							\
		 ("%s: umidi packet(" #dir "): %02X %02X %02X %02X\n",	\
		  device_xname(sc->sc_dev),				\
		  (unsigned char)(p)[0],			\
		  (unsigned char)(p)[1],			\
		  (unsigned char)(p)[2],			\
		  (unsigned char)(p)[3]));
#else
#define DPR_PACKET(dir, sc, p)
#endif

/*
 * A 4-byte Midiman packet superficially resembles a 4-byte USB MIDI packet
 * with the cable number and length in the last byte instead of the first,
 * but there the resemblance ends. Where a USB MIDI packet is a semantic
 * unit, a Midiman packet is just a wrapper for 1 to 3 bytes of raw MIDI
 * with a cable nybble and a length nybble (which, unlike the CIN of a
 * real USB MIDI packet, has no semantics at all besides the length).
 * A packet received from a Midiman may contain part of a MIDI message,
 * more than one MIDI message, or parts of more than one MIDI message. A
 * three-byte MIDI message may arrive in three packets of data length 1, and
 * running status may be used. Happily, the midi(4) driver above us will put
 * it all back together, so the only cost is in USB bandwidth. The device
 * has an easier time with what it receives from us: we'll pack messages in
 * and across packets, but filling the packets whenever possible and,
 * as midi(4) hands us a complete message at a time, we'll never send one
 * in a dribble of short packets.
 */

static int
out_jack_output(struct umidi_jack *out_jack, u_char *src, int len, int cin)
{
	struct umidi_endpoint *ep = out_jack->endpoint;
	struct umidi_softc *sc = ep->sc;
	unsigned char *packet;
	int plen;
	int poff;

	KASSERT(mutex_owned(&sc->sc_lock));

	if (sc->sc_dying)
		return EIO;

	if (!out_jack->opened)
		return ENODEV; /* XXX as it was, is this the right errno? */

	sc->sc_refcnt++;

#ifdef UMIDI_DEBUG
	if (umididebug >= 100)
		microtime(&umidi_tv);
#endif
	DPRINTFN(100, ("umidi out: %"PRIu64".%06"PRIu64"s ep=%p cn=%d len=%d cin=%#x\n",
	    umidi_tv.tv_sec%100, (uint64_t)umidi_tv.tv_usec,
	    ep, out_jack->cable_number, len, cin));
	
	packet = *ep->next_slot++;
	KASSERT(ep->buffer_size >=
	    (ep->next_slot - ep->buffer) * sizeof *ep->buffer);
	memset(packet, 0, UMIDI_PACKET_SIZE);
	if (UMQ_ISTYPE(sc, UMQ_TYPE_MIDIMAN_GARBLE)) {
		if (NULL != out_jack->midiman_ppkt) { /* fill out a prev pkt */
			poff = 0x0f & (out_jack->midiman_ppkt[3]);
			plen = 3 - poff;
			if (plen > len)
				plen = len;
			memcpy(out_jack->midiman_ppkt+poff, src, plen);
			src += plen;
			len -= plen;
			plen += poff;
			out_jack->midiman_ppkt[3] =
			    MIX_CN_CIN(out_jack->cable_number, plen);
			DPR_PACKET(out+, sc, out_jack->midiman_ppkt);
			if (3 == plen)
				out_jack->midiman_ppkt = NULL; /* no more */
		}
		if (0 == len)
			ep->next_slot--; /* won't be needed, nevermind */
		else {
			memcpy(packet, src, len);
			packet[3] = MIX_CN_CIN(out_jack->cable_number, len);
			DPR_PACKET(out, sc, packet);
			if (len < 3)
				out_jack->midiman_ppkt = packet;
		}
	} else { /* the nice simple USB class-compliant case */
		packet[0] = MIX_CN_CIN(out_jack->cable_number, cin);
		memcpy(packet+1, src, len);
		DPR_PACKET(out, sc, packet);
	}
	ep->next_schedule |= 1<<(out_jack->cable_number);
	++ ep->num_scheduled;
	if (!ep->armed && !ep->soliciting) {
		/*
		 * It would be bad to call out_solicit directly here (the
		 * caller need not be reentrant) but a soft interrupt allows
		 * solicit to run immediately the caller exits its critical
		 * section, and if the caller has more to write we can get it
		 * before starting the USB transfer, and send a longer one.
		 */
		ep->soliciting = 1;
		softint_schedule(ep->solicit_cookie);
	}

	if (--sc->sc_refcnt < 0)
		usb_detach_broadcast(sc->sc_dev, &sc->sc_detach_cv);
	
	return 0;
}

static void
in_intr(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	int cn, len, i;
	struct umidi_endpoint *ep = (struct umidi_endpoint *)priv;
	struct umidi_softc *sc = ep->sc;
	struct umidi_jack *jack;
	unsigned char *packet;
	umidi_packet_bufp slot;
	umidi_packet_bufp end;
	unsigned char *data;
	u_int32_t count;

	if (ep->sc->sc_dying || !ep->num_open)
		return;

	mutex_enter(&sc->sc_lock);
	usbd_get_xfer_status(xfer, NULL, NULL, &count, NULL);
        if (0 == count % UMIDI_PACKET_SIZE) {
		DPRINTFN(200,("%s: input endpoint %p transfer length %u\n",
			     device_xname(ep->sc->sc_dev), ep, count));
        } else {
                DPRINTF(("%s: input endpoint %p odd transfer length %u\n",
                        device_xname(ep->sc->sc_dev), ep, count));
        }
	
	slot = ep->buffer;
	end = slot + count / sizeof *slot;

	for (packet = *slot; slot < end; packet = *++slot) {
	
		if (UMQ_ISTYPE(ep->sc, UMQ_TYPE_MIDIMAN_GARBLE)) {
			cn = (0xf0&(packet[3]))>>4;
			len = 0x0f&(packet[3]);
			data = packet;
		} else {
			cn = GET_CN(packet[0]);
			len = packet_length[GET_CIN(packet[0])];
			data = packet + 1;
		}
		/* 0 <= cn <= 15 by inspection of above code */
		if (!(jack = ep->jacks[cn]) || cn != jack->cable_number) {
			DPRINTF(("%s: stray input endpoint %p cable %d len %d: "
			         "%02X %02X %02X (try CN_SEQ quirk?)\n",
				 device_xname(ep->sc->sc_dev), ep, cn, len,
				 (unsigned)data[0],
				 (unsigned)data[1],
				 (unsigned)data[2]));
			mutex_exit(&sc->sc_lock);
			return;
		}

		if (!jack->bound || !jack->opened)
			continue;

		DPRINTFN(500,("%s: input endpoint %p cable %d len %d: "
		             "%02X %02X %02X\n",
			     device_xname(ep->sc->sc_dev), ep, cn, len,
			     (unsigned)data[0],
			     (unsigned)data[1],
			     (unsigned)data[2]));

		if (jack->u.in.intr) {
			for (i = 0; i < len; i++) {
				(*jack->u.in.intr)(jack->arg, data[i]);
			}
		}

	}

	(void)start_input_transfer(ep);
	mutex_exit(&sc->sc_lock);
}

static void
out_intr(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct umidi_endpoint *ep = (struct umidi_endpoint *)priv;
	struct umidi_softc *sc = ep->sc;
	u_int32_t count;

	if (sc->sc_dying)
		return;

	mutex_enter(&sc->sc_lock);
#ifdef UMIDI_DEBUG
	if (umididebug >= 200)
		microtime(&umidi_tv);
#endif
	usbd_get_xfer_status(xfer, NULL, NULL, &count, NULL);
        if (0 == count % UMIDI_PACKET_SIZE) {
		DPRINTFN(200,("%s: %"PRIu64".%06"PRIu64"s out ep %p xfer length %u\n",
			     device_xname(ep->sc->sc_dev),
			     umidi_tv.tv_sec%100, (uint64_t)umidi_tv.tv_usec, ep, count));
        } else {
                DPRINTF(("%s: output endpoint %p odd transfer length %u\n",
                        device_xname(ep->sc->sc_dev), ep, count));
        }
	count /= UMIDI_PACKET_SIZE;
	
	/*
	 * If while the transfer was pending we buffered any new messages,
	 * move them to the start of the buffer.
	 */
	ep->next_slot -= count;
	if (ep->buffer < ep->next_slot) {
		memcpy(ep->buffer, ep->buffer + count,
		       (char *)ep->next_slot - (char *)ep->buffer);
	}
	cv_broadcast(&sc->sc_cv);
	/*
	 * Do not want anyone else to see armed <- 0 before soliciting <- 1.
	 * Running at IPL_USB so the following should happen to be safe.
	 */
	ep->armed = 0;
	if (!ep->soliciting) {
		ep->soliciting = 1;
		out_solicit_locked(ep);
	}
	mutex_exit(&sc->sc_lock);
}

/*
 * A jack on which we have received a packet must be called back on its
 * out.intr handler before it will send us another; it is considered
 * 'scheduled'. It is nice and predictable - as long as it is scheduled,
 * we need no extra buffer space for it.
 *
 * In contrast, a jack that is open but not scheduled may supply us a packet
 * at any time, driven by the top half, and we must be able to accept it, no
 * excuses. So we must ensure that at any point in time there are at least
 * (num_open - num_scheduled) slots free.
 *
 * As long as there are more slots free than that minimum, we can loop calling
 * scheduled jacks back on their "interrupt" handlers, soliciting more
 * packets, starting the USB transfer only when the buffer space is down to
 * the minimum or no jack has any more to send.
 */

static void
out_solicit_locked(void *arg)
{
	struct umidi_endpoint *ep = arg;
	umidi_packet_bufp end;
	u_int16_t which;
	struct umidi_jack *jack;

	KASSERT(mutex_owned(&ep->sc->sc_lock));
	
	end = ep->buffer + ep->buffer_size / sizeof *ep->buffer;
	
	for ( ;; ) {
		if (end - ep->next_slot <= ep->num_open - ep->num_scheduled)
			break; /* at IPL_USB */
		if (ep->this_schedule == 0) {
			if (ep->next_schedule == 0)
				break; /* at IPL_USB */
			ep->this_schedule = ep->next_schedule;
			ep->next_schedule = 0;
		}
		/*
		 * At least one jack is scheduled. Find and mask off the least
		 * set bit in this_schedule and decrement num_scheduled.
		 * Convert mask to bit index to find the corresponding jack,
		 * and call its intr handler. If it has a message, it will call
		 * back one of the output methods, which will set its bit in
		 * next_schedule (not copied into this_schedule until the
		 * latter is empty). In this way we round-robin the jacks that
		 * have messages to send, until the buffer is as full as we
		 * dare, and then start a transfer.
		 */
		which = ep->this_schedule;
		which &= (~which)+1; /* now mask of least set bit */
		ep->this_schedule &= ~which;
		--ep->num_scheduled;

		--which; /* now 1s below mask - count 1s to get index */
		which -= ((which >> 1) & 0x5555);/* SWAR credit aggregate.org */
		which = (((which >> 2) & 0x3333) + (which & 0x3333));
		which = (((which >> 4) + which) & 0x0f0f);
		which +=  (which >> 8);
		which &= 0x1f; /* the bit index a/k/a jack number */
		
		jack = ep->jacks[which];
		if (jack->u.out.intr)
			(*jack->u.out.intr)(jack->arg);
	}
	/* intr lock held at loop exit */
	if (!ep->armed && ep->next_slot > ep->buffer) {
		/*
		 * Can't hold the interrupt lock while calling into USB,
		 * but we can safely drop it here.
		 */
		mutex_exit(&ep->sc->sc_lock);
		ep->armed = (USBD_IN_PROGRESS == start_output_transfer(ep));
		mutex_enter(&ep->sc->sc_lock);
	}
	ep->soliciting = 0;
}

/* Entry point for the softintr.  */
static void
out_solicit(void *arg)
{
	struct umidi_endpoint *ep = arg;
	struct umidi_softc *sc = ep->sc;

	mutex_enter(&sc->sc_lock);
	out_solicit_locked(arg);
	mutex_exit(&sc->sc_lock);
}

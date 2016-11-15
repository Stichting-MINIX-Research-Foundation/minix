/*	$NetBSD: usscanner.c,v 1.38 2013/09/15 19:27:22 martin Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) and LLoyd Parkes.
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
 * This driver is partly based on information taken from the Linux driver
 * by John Fremlin, Oliver Neukum, and Jeremy Hall.
 */
/*
 * Protocol:
 * Send raw SCSI command on the bulk-out pipe.
 * If output command then
 *     send further data on the bulk-out pipe
 * else if input command then
 *     read data on the bulk-in pipe
 * else
 *     don't do anything.
 * Read status byte on the interrupt pipe (which doesn't seem to be
 * an interrupt pipe at all).  This operation sometimes times out.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: usscanner.c,v 1.38 2013/09/15 19:27:22 martin Exp $");

#include "scsibus.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lwp.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/buf.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#include <dev/usb/usbdevs.h>

#include <sys/scsiio.h>
#include <dev/scsipi/scsi_spc.h>
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/atapiconf.h>

#ifdef USSCANNER_DEBUG
#define DPRINTF(x)	if (usscannerdebug) printf x
#define DPRINTFN(n,x)	if (usscannerdebug>(n)) printf x
int	usscannerdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif


#define USSCANNER_CONFIG_NO		1
#define USSCANNER_IFACE_IDX		0

#define USSCANNER_SCSIID_HOST	0x00
#define USSCANNER_SCSIID_DEVICE	0x01

#define USSCANNER_MAX_TRANSFER_SIZE	MAXPHYS

#define USSCANNER_TIMEOUT 2000

struct usscanner_softc {
 	device_t		sc_dev;
	usbd_device_handle	sc_udev;
	usbd_interface_handle	sc_iface;

	int			sc_in_addr;
	usbd_pipe_handle	sc_in_pipe;

	int			sc_intr_addr;
	usbd_pipe_handle	sc_intr_pipe;
	usbd_xfer_handle	sc_intr_xfer;
	u_char			sc_status;

	int			sc_out_addr;
	usbd_pipe_handle	sc_out_pipe;

	usbd_xfer_handle	sc_cmd_xfer;
	void			*sc_cmd_buffer;
	usbd_xfer_handle	sc_data_xfer;
	void			*sc_data_buffer;

	int			sc_state;
#define UAS_IDLE	0
#define UAS_CMD		1
#define UAS_DATA	2
#define UAS_SENSECMD	3
#define UAS_SENSEDATA	4
#define UAS_STATUS	5

	struct scsipi_xfer	*sc_xs;

	device_t		sc_child;	/* child device, for detach */

	struct scsipi_adapter	sc_adapter;
	struct scsipi_channel	sc_channel;

	int			sc_refcnt;
	char			sc_dying;
};


Static void usscanner_cleanup(struct usscanner_softc *sc);
Static void usscanner_scsipi_request(struct scsipi_channel *chan,
				scsipi_adapter_req_t req, void *arg);
Static void usscanner_scsipi_minphys(struct buf *bp);
Static void usscanner_done(struct usscanner_softc *sc);
Static void usscanner_sense(struct usscanner_softc *sc);
typedef void callback(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static callback usscanner_intr_cb;
Static callback usscanner_cmd_cb;
Static callback usscanner_data_cb;
Static callback usscanner_sensecmd_cb;
Static callback usscanner_sensedata_cb;

int usscanner_match(device_t, cfdata_t, void *);
void usscanner_attach(device_t, device_t, void *);
void usscanner_childdet(device_t, device_t);
int usscanner_detach(device_t, int);
int usscanner_activate(device_t, enum devact);
extern struct cfdriver usscanner_cd;
CFATTACH_DECL2_NEW(usscanner, sizeof(struct usscanner_softc),
    usscanner_match, usscanner_attach, usscanner_detach, usscanner_activate,
    NULL, usscanner_childdet);

int 
usscanner_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	DPRINTFN(50,("usscanner_match\n"));

	if (uaa->vendor == USB_VENDOR_HP &&
	    uaa->product == USB_PRODUCT_HP_5300C)
		return (UMATCH_VENDOR_PRODUCT);
	else
		return (UMATCH_NONE);
}

void 
usscanner_attach(device_t parent, device_t self, void *aux)
{
	struct usscanner_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	usbd_device_handle	dev = uaa->device;
	usbd_interface_handle	iface;
	char			*devinfop;
	usbd_status		err;
	usb_endpoint_descriptor_t *ed;
	u_int8_t		epcount;
	int			i;

	DPRINTFN(10,("usscanner_attach: sc=%p\n", sc));

	sc->sc_dev = self;

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(dev, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	err = usbd_set_config_no(dev, USSCANNER_CONFIG_NO, 1);
	if (err) {
		aprint_error_dev(self, "failed to set configuration"
		    ", err=%s\n", usbd_errstr(err));
		return;
	}

	err = usbd_device2interface_handle(dev, USSCANNER_IFACE_IDX, &iface);
	if (err) {
		aprint_error_dev(self, "getting interface handle failed\n");
		return;
	}

	sc->sc_udev = dev;
	sc->sc_iface = iface;

	epcount = 0;
	(void)usbd_endpoint_count(iface, &epcount);

	sc->sc_in_addr = -1;
	sc->sc_intr_addr = -1;
	sc->sc_out_addr = -1;
	for (i = 0; i < epcount; i++) {
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL) {
			aprint_error_dev(self, "couldn't get ep %d\n", i);
			return;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->sc_in_addr = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->sc_intr_addr = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->sc_out_addr = ed->bEndpointAddress;
		}
	}
	if (sc->sc_in_addr == -1 || sc->sc_intr_addr == -1 ||
	    sc->sc_out_addr == -1) {
		aprint_error_dev(self, "missing endpoint\n");
		return;
	}

	err = usbd_open_pipe(sc->sc_iface, sc->sc_in_addr,
			     USBD_EXCLUSIVE_USE, &sc->sc_in_pipe);
	if (err) {
		aprint_error_dev(self, "open in pipe failed, err=%d\n", err);
		return;
	}

	/* The interrupt endpoint must be opened as a normal pipe. */
	err = usbd_open_pipe(sc->sc_iface, sc->sc_intr_addr,
			     USBD_EXCLUSIVE_USE, &sc->sc_intr_pipe);

	if (err) {
		aprint_error_dev(self, "open intr pipe failed, err=%d\n", err);
		usscanner_cleanup(sc);
		return;
	}
	err = usbd_open_pipe(sc->sc_iface, sc->sc_out_addr,
			     USBD_EXCLUSIVE_USE, &sc->sc_out_pipe);
	if (err) {
		aprint_error_dev(self, "open out pipe failed, err=%d\n",  err);
		usscanner_cleanup(sc);
		return;
	}

	sc->sc_cmd_xfer = usbd_alloc_xfer(uaa->device);
	if (sc->sc_cmd_xfer == NULL) {
		aprint_error_dev(self, "alloc cmd xfer failed, err=%d\n", err);
		usscanner_cleanup(sc);
		return;
	}

	/* XXX too big */
	sc->sc_cmd_buffer = usbd_alloc_buffer(sc->sc_cmd_xfer,
					     USSCANNER_MAX_TRANSFER_SIZE);
	if (sc->sc_cmd_buffer == NULL) {
		aprint_error_dev(self, "alloc cmd buffer failed, err=%d\n",
		    err);
		usscanner_cleanup(sc);
		return;
	}

	sc->sc_intr_xfer = usbd_alloc_xfer (uaa->device);
	if (sc->sc_intr_xfer == NULL) {
	  aprint_error_dev(self, "alloc intr xfer failed, err=%d\n", err);
	  usscanner_cleanup(sc);
	  return;
        }

	sc->sc_data_xfer = usbd_alloc_xfer(uaa->device);
	if (sc->sc_data_xfer == NULL) {
		aprint_error_dev(self, "alloc data xfer failed, err=%d\n", err);
		usscanner_cleanup(sc);
		return;
	}
	sc->sc_data_buffer = usbd_alloc_buffer(sc->sc_data_xfer,
					      USSCANNER_MAX_TRANSFER_SIZE);
	if (sc->sc_data_buffer == NULL) {
		aprint_error_dev(self, "alloc data buffer failed, err=%d\n",
		    err);
		usscanner_cleanup(sc);
		return;
	}

	/*
	 * Fill in the adapter.
	 */
	sc->sc_adapter.adapt_request = usscanner_scsipi_request;
	sc->sc_adapter.adapt_dev = sc->sc_dev;
	sc->sc_adapter.adapt_nchannels = 1;
	sc->sc_adapter.adapt_openings = 1;
	sc->sc_adapter.adapt_max_periph = 1;
	sc->sc_adapter.adapt_minphys = usscanner_scsipi_minphys;

#if NSCSIBUS > 0
	/*
	 * fill in the scsipi_channel.
	 */
	sc->sc_channel.chan_adapter = &sc->sc_adapter;
	sc->sc_channel.chan_bustype = &scsi_bustype;
	sc->sc_channel.chan_channel = 0;
	sc->sc_channel.chan_ntargets = USSCANNER_SCSIID_DEVICE + 1;
	sc->sc_channel.chan_nluns = 1;
	sc->sc_channel.chan_id = USSCANNER_SCSIID_HOST;

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   sc->sc_dev);

	sc->sc_child = config_found(sc->sc_dev, &sc->sc_channel, scsiprint);

	DPRINTFN(10, ("usscanner_attach: %p\n", sc->sc_udev));

	return;

#else
	/* No SCSI bus, just ignore it */
	usscanner_cleanup(sc);

	aprint_error_dev(self,
	    "no scsibus configured, see usscanner(4) for details\n");

	return;

#endif
}

void
usscanner_childdet(device_t self, device_t child)
{
	struct usscanner_softc *sc = device_private(self);

	KASSERT(sc->sc_child == NULL);
	sc->sc_child = NULL;
}

int 
usscanner_detach(device_t self, int flags)
{
	struct usscanner_softc *sc = device_private(self);
	int rv, s;

	DPRINTF(("usscanner_detach: sc=%p flags=%d\n", sc, flags));

	sc->sc_dying = 1;
	/* Abort all pipes.  Causes processes waiting for transfer to wake. */
	if (sc->sc_in_pipe != NULL)
		usbd_abort_pipe(sc->sc_in_pipe);
	if (sc->sc_intr_pipe != NULL)
		usbd_abort_pipe(sc->sc_intr_pipe);
	if (sc->sc_out_pipe != NULL)
		usbd_abort_pipe(sc->sc_out_pipe);

	s = splusb();
	if (--sc->sc_refcnt >= 0) {
		/* Wait for processes to go away. */
		usb_detach_waitold(sc->sc_dev);
	}
	splx(s);

	if (sc->sc_child != NULL)
		rv = config_detach(sc->sc_child, flags);
	else
		rv = 0;

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   sc->sc_dev);

	return (rv);
}

Static void
usscanner_cleanup(struct usscanner_softc *sc)
{
	if (sc->sc_in_pipe != NULL) {
		usbd_close_pipe(sc->sc_in_pipe);
		sc->sc_in_pipe = NULL;
	}
	if (sc->sc_intr_pipe != NULL) {
		usbd_close_pipe(sc->sc_intr_pipe);
		sc->sc_intr_pipe = NULL;
	}
	if (sc->sc_out_pipe != NULL) {
		usbd_close_pipe(sc->sc_out_pipe);
		sc->sc_out_pipe = NULL;
	}
	if (sc->sc_cmd_xfer != NULL) {
		usbd_free_xfer(sc->sc_cmd_xfer);
		sc->sc_cmd_xfer = NULL;
	}
	if (sc->sc_data_xfer != NULL) {
		usbd_free_xfer(sc->sc_data_xfer);
		sc->sc_data_xfer = NULL;
	}
}

int
usscanner_activate(device_t self, enum devact act)
{
	struct usscanner_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

Static void
usscanner_scsipi_minphys(struct buf *bp)
{
	if (bp->b_bcount > USSCANNER_MAX_TRANSFER_SIZE)
		bp->b_bcount = USSCANNER_MAX_TRANSFER_SIZE;
	minphys(bp);
}

Static void
usscanner_sense(struct usscanner_softc *sc)
{
	struct scsipi_xfer *xs = sc->sc_xs;
	struct scsipi_periph *periph = xs->xs_periph;
	struct scsi_request_sense sense_cmd;
	usbd_status err;

	/* fetch sense data */
	memset(&sense_cmd, 0, sizeof(sense_cmd));
	sense_cmd.opcode = SCSI_REQUEST_SENSE;
	sense_cmd.byte2 = periph->periph_lun << SCSI_CMD_LUN_SHIFT;
	sense_cmd.length = sizeof xs->sense;

	sc->sc_state = UAS_SENSECMD;
	memcpy(sc->sc_cmd_buffer, &sense_cmd, sizeof sense_cmd);
	usbd_setup_xfer(sc->sc_cmd_xfer, sc->sc_out_pipe, sc, sc->sc_cmd_buffer,
	    sizeof sense_cmd, USBD_NO_COPY, USSCANNER_TIMEOUT,
	    usscanner_sensecmd_cb);
	err = usbd_transfer(sc->sc_cmd_xfer);
	if (err == USBD_IN_PROGRESS)
		return;

	xs->error = XS_DRIVER_STUFFUP;
	usscanner_done(sc);
}

Static void
usscanner_intr_cb(usbd_xfer_handle xfer, usbd_private_handle priv,
		 usbd_status status)
{
	struct usscanner_softc *sc = priv;
	int s;

	DPRINTFN(10, ("usscanner_data_cb status=%d\n", status));

#ifdef USSCANNER_DEBUG
	if (sc->sc_state != UAS_STATUS) {
		printf("%s: !UAS_STATUS\n", device_xname(sc->sc_dev));
	}
	if (sc->sc_status != 0) {
		printf("%s: status byte=0x%02x\n", device_xname(sc->sc_dev), sc->sc_status);
	}
#endif
	/* XXX what should we do on non-0 status */

	sc->sc_state = UAS_IDLE;

	s = splbio();
	KERNEL_LOCK(1, curlwp);
	scsipi_done(sc->sc_xs);
	KERNEL_UNLOCK_ONE(curlwp);
	splx(s);
}

Static void
usscanner_data_cb(usbd_xfer_handle xfer, usbd_private_handle priv,
		 usbd_status status)
{
	struct usscanner_softc *sc = priv;
	struct scsipi_xfer *xs = sc->sc_xs;
	u_int32_t len;

	DPRINTFN(10, ("usscanner_data_cb status=%d\n", status));

#ifdef USSCANNER_DEBUG
	if (sc->sc_state != UAS_DATA) {
		printf("%s: !UAS_DATA\n", device_xname(sc->sc_dev));
	}
#endif

	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	xs->resid = xs->datalen - len;

	switch (status) {
	case USBD_NORMAL_COMPLETION:
		if (xs->xs_control & XS_CTL_DATA_IN)
			memcpy(xs->data, sc->sc_data_buffer, len);
		xs->error = XS_NOERROR;
		break;
	case USBD_TIMEOUT:
		xs->error = XS_TIMEOUT;
		break;
	case USBD_CANCELLED:
		if (xs->error == XS_SENSE) {
			usscanner_sense(sc);
			return;
		}
		break;
	default:
		xs->error = XS_DRIVER_STUFFUP; /* XXX ? */
		break;
	}
	usscanner_done(sc);
}

Static void
usscanner_sensedata_cb(usbd_xfer_handle xfer, usbd_private_handle priv,
		       usbd_status status)
{
	struct usscanner_softc *sc = priv;
	struct scsipi_xfer *xs = sc->sc_xs;
	u_int32_t len;

	DPRINTFN(10, ("usscanner_sensedata_cb status=%d\n", status));

#ifdef USSCANNER_DEBUG
	if (sc->sc_state != UAS_SENSEDATA) {
		printf("%s: !UAS_SENSEDATA\n", device_xname(sc->sc_dev));
	}
#endif

	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	switch (status) {
	case USBD_NORMAL_COMPLETION:
		memcpy(&xs->sense, sc->sc_data_buffer, len);
		if (len < sizeof xs->sense)
			xs->error = XS_SHORTSENSE;
		break;
	case USBD_TIMEOUT:
		xs->error = XS_TIMEOUT;
		break;
	case USBD_CANCELLED:
		xs->error = XS_RESET;
		break;
	default:
		xs->error = XS_DRIVER_STUFFUP; /* XXX ? */
		break;
	}
	usscanner_done(sc);
}

Static void
usscanner_done(struct usscanner_softc *sc)
{
	struct scsipi_xfer *xs = sc->sc_xs;
	usbd_status err;

	DPRINTFN(10,("usscanner_done: error=%d\n", sc->sc_xs->error));

	sc->sc_state = UAS_STATUS;
	usbd_setup_xfer(sc->sc_intr_xfer, sc->sc_intr_pipe, sc, &sc->sc_status,
	    1, USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    USSCANNER_TIMEOUT, usscanner_intr_cb);
	err = usbd_transfer(sc->sc_intr_xfer);
	if (err == USBD_IN_PROGRESS)
		return;
	xs->error = XS_DRIVER_STUFFUP;
}

Static void
usscanner_sensecmd_cb(usbd_xfer_handle xfer, usbd_private_handle priv,
		      usbd_status status)
{
	struct usscanner_softc *sc = priv;
	struct scsipi_xfer *xs = sc->sc_xs;
	usbd_status err;

	DPRINTFN(10, ("usscanner_sensecmd_cb status=%d\n", status));

#ifdef USSCANNER_DEBUG
	if (usscannerdebug > 15)
		xs->xs_periph->periph_flags |= 1; /* XXX 1 */

	if (sc->sc_state != UAS_SENSECMD) {
		aprint_error_dev(sc->sc_dev, "!UAS_SENSECMD\n");
		xs->error = XS_DRIVER_STUFFUP;
		goto done;
	}
#endif

	switch (status) {
	case USBD_NORMAL_COMPLETION:
		break;
	case USBD_TIMEOUT:
		xs->error = XS_TIMEOUT;
		goto done;
	default:
		xs->error = XS_DRIVER_STUFFUP; /* XXX ? */
		goto done;
	}

	sc->sc_state = UAS_SENSEDATA;
	usbd_setup_xfer(sc->sc_data_xfer, sc->sc_in_pipe, sc,
	    sc->sc_data_buffer,
	    sizeof xs->sense, USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    USSCANNER_TIMEOUT, usscanner_sensedata_cb);
	err = usbd_transfer(sc->sc_data_xfer);
	if (err == USBD_IN_PROGRESS)
		return;
	xs->error = XS_DRIVER_STUFFUP;
 done:
	usscanner_done(sc);
}

Static void
usscanner_cmd_cb(usbd_xfer_handle xfer, usbd_private_handle priv,
		 usbd_status status)
{
	struct usscanner_softc *sc = priv;
	struct scsipi_xfer *xs = sc->sc_xs;
	usbd_pipe_handle pipe;
	usbd_status err;

	DPRINTFN(10, ("usscanner_cmd_cb status=%d\n", status));

#ifdef USSCANNER_DEBUG
	if (usscannerdebug > 15)
		xs->xs_periph->periph_flags |= 1;	/* XXX 1 */

	if (sc->sc_state != UAS_CMD) {
		aprint_error_dev(sc->sc_dev, "!UAS_CMD\n");
		xs->error = XS_DRIVER_STUFFUP;
		goto done;
	}
#endif

	switch (status) {
	case USBD_NORMAL_COMPLETION:
		break;
	case USBD_TIMEOUT:
		xs->error = XS_TIMEOUT;
		goto done;
	case USBD_CANCELLED:
		goto done;
	default:
		xs->error = XS_DRIVER_STUFFUP; /* XXX ? */
		goto done;
	}

	if (xs->datalen == 0) {
		DPRINTFN(4, ("usscanner_cmd_cb: no data phase\n"));
		xs->error = XS_NOERROR;
		goto done;
	}

	if (xs->xs_control & XS_CTL_DATA_IN) {
		DPRINTFN(4, ("usscanner_cmd_cb: data in len=%d\n",
			     xs->datalen));
		pipe = sc->sc_in_pipe;
	} else {
		DPRINTFN(4, ("usscanner_cmd_cb: data out len=%d\n",
			     xs->datalen));
		memcpy(sc->sc_data_buffer, xs->data, xs->datalen);
		pipe = sc->sc_out_pipe;
	}
	sc->sc_state = UAS_DATA;
	usbd_setup_xfer(sc->sc_data_xfer, pipe, sc, sc->sc_data_buffer,
	    xs->datalen, USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    xs->timeout, usscanner_data_cb);
	err = usbd_transfer(sc->sc_data_xfer);
	if (err == USBD_IN_PROGRESS)
		return;
	xs->error = XS_DRIVER_STUFFUP;

 done:
	usscanner_done(sc);
}

Static void
usscanner_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req, void *arg)
{
	struct scsipi_xfer *xs;
	struct usscanner_softc *sc =
	    device_private(chan->chan_adapter->adapt_dev);
	usbd_status err;

	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		xs = arg;

		DPRINTFN(8, ("%s: usscanner_scsipi_request: %d:%d "
		    "xs=%p cmd=0x%02x datalen=%d (quirks=0x%x, poll=%d)\n",
		    device_xname(sc->sc_dev),
		    xs->xs_periph->periph_target, xs->xs_periph->periph_lun,
		    xs, xs->cmd->opcode, xs->datalen,
		    xs->xs_periph->periph_quirks,
		    xs->xs_control & XS_CTL_POLL));

		if (sc->sc_dying) {
			xs->error = XS_DRIVER_STUFFUP;
			goto done;
		}

#ifdef USSCANNER_DEBUG
		if (xs->xs_periph->periph_target != USSCANNER_SCSIID_DEVICE) {
			DPRINTF(("%s: wrong SCSI ID %d\n",
			    device_xname(sc->sc_dev),
			    xs->xs_periph->periph_target));
			xs->error = XS_DRIVER_STUFFUP;
			goto done;
		}
		if (sc->sc_state != UAS_IDLE) {
			printf("%s: !UAS_IDLE\n", device_xname(sc->sc_dev));
			xs->error = XS_DRIVER_STUFFUP;
			goto done;
		}
#endif

		if (xs->datalen > USSCANNER_MAX_TRANSFER_SIZE) {
			aprint_normal_dev(sc->sc_dev,
			    "usscanner_scsipi_request: large datalen, %d\n",
			    xs->datalen);
			xs->error = XS_DRIVER_STUFFUP;
			goto done;
		}

		DPRINTFN(4, ("%s: usscanner_scsipi_request: async cmdlen=%d"
		    " datalen=%d\n", device_xname(sc->sc_dev), xs->cmdlen,
		    xs->datalen));
		sc->sc_state = UAS_CMD;
		sc->sc_xs = xs;
		memcpy(sc->sc_cmd_buffer, xs->cmd, xs->cmdlen);
		usbd_setup_xfer(sc->sc_cmd_xfer, sc->sc_out_pipe, sc,
		    sc->sc_cmd_buffer, xs->cmdlen, USBD_NO_COPY,
		    USSCANNER_TIMEOUT, usscanner_cmd_cb);
		err = usbd_transfer(sc->sc_cmd_xfer);
		if (err != USBD_IN_PROGRESS) {
			xs->error = XS_DRIVER_STUFFUP;
			goto done;
		}

		return;


 done:
		sc->sc_state = UAS_IDLE;
		KERNEL_LOCK(1, curlwp);
		scsipi_done(xs);
		KERNEL_UNLOCK_ONE(curlwp);
		return;

	case ADAPTER_REQ_GROW_RESOURCES:
		/* XXX Not supported. */
		return;
	case ADAPTER_REQ_SET_XFER_MODE:
		/* XXX Not supported. */
		return;
	}

}

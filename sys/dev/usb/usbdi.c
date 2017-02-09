/*	$NetBSD: usbdi.c,v 1.165 2015/09/26 13:59:28 skrll Exp $	*/

/*
 * Copyright (c) 1998, 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology and Matthew R. Green (mrg@eterna.com.au).
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
__KERNEL_RCSID(0, "$NetBSD: usbdi.c,v 1.165 2015/09/26 13:59:28 skrll Exp $");

#ifdef _KERNEL_OPT
#include "opt_usb.h"
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/bus.h>
#include <sys/cpu.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>
#include <dev/usb/usb_quirks.h>
#include <dev/usb/usbhist.h>

/* UTF-8 encoding stuff */
#include <fs/unicode.h>

extern int usbdebug;

Static usbd_status usbd_ar_pipe(usbd_pipe_handle);
Static void usbd_start_next(usbd_pipe_handle);
Static usbd_status usbd_open_pipe_ival
	(usbd_interface_handle, u_int8_t, u_int8_t, usbd_pipe_handle *, int);

#if defined(USB_DEBUG)
void
usbd_dump_iface(struct usbd_interface *iface)
{
	USBHIST_FUNC(); USBHIST_CALLED(usbdebug);

	USBHIST_LOG(usbdebug, "iface %p\n", iface, 0, 0, 0);
	if (iface == NULL)
		return;
	USBHIST_LOG(usbdebug, "     device = %p idesc = %p index = %d",
	    iface->device, iface->idesc, iface->index, 0);
	USBHIST_LOG(usbdebug, "     altindex=%d priv=%p",
	    iface->altindex, iface->priv, 0, 0);
}

void
usbd_dump_device(struct usbd_device *dev)
{
	USBHIST_FUNC(); USBHIST_CALLED(usbdebug);

	USBHIST_LOG(usbdebug, "dev = %p", dev, 0, 0, 0);
	if (dev == NULL)
		return;
	USBHIST_LOG(usbdebug, "     bus = %p default_pipe = %p",
	    dev->bus, dev->default_pipe, 0, 0);
	USBHIST_LOG(usbdebug, "     address = %d config = %d depth = %d ",
	    dev->address, dev->config, dev->depth, 0);
	USBHIST_LOG(usbdebug, "     speed = %d self_powered = %d "
	    "power = %d langid = %d",
	    dev->speed, dev->self_powered, dev->power, dev->langid);
}

void
usbd_dump_endpoint(struct usbd_endpoint *endp)
{
	USBHIST_FUNC(); USBHIST_CALLED(usbdebug);

	USBHIST_LOG(usbdebug, "endp = %p", endp, 0, 0, 0);
	if (endp == NULL)
		return;
	USBHIST_LOG(usbdebug, "    edesc = %p refcnt = %d",
	    endp->edesc, endp->refcnt, 0, 0);
	if (endp->edesc)
		USBHIST_LOG(usbdebug, "     bEndpointAddress=0x%02x",
		    endp->edesc->bEndpointAddress, 0, 0, 0);
}

void
usbd_dump_queue(usbd_pipe_handle pipe)
{
	usbd_xfer_handle xfer;

	USBHIST_FUNC(); USBHIST_CALLED(usbdebug);

	USBHIST_LOG(usbdebug, "pipe = %p", pipe, 0, 0, 0);
	SIMPLEQ_FOREACH(xfer, &pipe->queue, next) {
		USBHIST_LOG(usbdebug, "     xfer = %p", xfer, 0, 0, 0);
	}
}

void
usbd_dump_pipe(usbd_pipe_handle pipe)
{
	USBHIST_FUNC(); USBHIST_CALLED(usbdebug);

	USBHIST_LOG(usbdebug, "pipe = %p", pipe, 0, 0, 0);
	if (pipe == NULL)
		return;
	usbd_dump_iface(pipe->iface);
	usbd_dump_device(pipe->device);
	usbd_dump_endpoint(pipe->endpoint);
	USBHIST_LOG(usbdebug, "(usbd_dump_pipe)", 0, 0, 0, 0);
	USBHIST_LOG(usbdebug, "     refcnt = %d running = %d aborting = %d",
	    pipe->refcnt, pipe->running, pipe->aborting, 0);
	USBHIST_LOG(usbdebug, "     intrxfer = %p, repeat = %d, interval = %d",
	    pipe->intrxfer, pipe->repeat, pipe->interval, 0);
}
#endif

usbd_status
usbd_open_pipe(usbd_interface_handle iface, u_int8_t address,
	       u_int8_t flags, usbd_pipe_handle *pipe)
{
	return (usbd_open_pipe_ival(iface, address, flags, pipe,
				    USBD_DEFAULT_INTERVAL));
}

usbd_status
usbd_open_pipe_ival(usbd_interface_handle iface, u_int8_t address,
		    u_int8_t flags, usbd_pipe_handle *pipe, int ival)
{
	usbd_pipe_handle p;
	struct usbd_endpoint *ep;
	usbd_status err;
	int i;

	USBHIST_FUNC(); USBHIST_CALLED(usbdebug);

	USBHIST_LOG(usbdebug, "iface = %p address = 0x%x flags = 0x%x",
	    iface, address, flags, 0);

	for (i = 0; i < iface->idesc->bNumEndpoints; i++) {
		ep = &iface->endpoints[i];
		if (ep->edesc == NULL)
			return (USBD_IOERROR);
		if (ep->edesc->bEndpointAddress == address)
			goto found;
	}
	return (USBD_BAD_ADDRESS);
 found:
	if ((flags & USBD_EXCLUSIVE_USE) && ep->refcnt != 0)
		return (USBD_IN_USE);
	err = usbd_setup_pipe_flags(iface->device, iface, ep, ival, &p, flags);
	if (err)
		return (err);
	LIST_INSERT_HEAD(&iface->pipes, p, next);
	*pipe = p;
	return (USBD_NORMAL_COMPLETION);
}

usbd_status
usbd_open_pipe_intr(usbd_interface_handle iface, u_int8_t address,
		    u_int8_t flags, usbd_pipe_handle *pipe,
		    usbd_private_handle priv, void *buffer, u_int32_t len,
		    usbd_callback cb, int ival)
{
	usbd_status err;
	usbd_xfer_handle xfer;
	usbd_pipe_handle ipipe;

	USBHIST_FUNC(); USBHIST_CALLED(usbdebug);

	USBHIST_LOG(usbdebug, "address = 0x%x flags = 0x%x len = %d",
	    address, flags, len, 0);

	err = usbd_open_pipe_ival(iface, address,
				  USBD_EXCLUSIVE_USE | (flags & USBD_MPSAFE),
				  &ipipe, ival);
	if (err)
		return (err);
	xfer = usbd_alloc_xfer(iface->device);
	if (xfer == NULL) {
		err = USBD_NOMEM;
		goto bad1;
	}
	usbd_setup_xfer(xfer, ipipe, priv, buffer, len, flags,
	    USBD_NO_TIMEOUT, cb);
	ipipe->intrxfer = xfer;
	ipipe->repeat = 1;
	err = usbd_transfer(xfer);
	*pipe = ipipe;
	if (err != USBD_IN_PROGRESS)
		goto bad2;
	return (USBD_NORMAL_COMPLETION);

 bad2:
	ipipe->intrxfer = NULL;
	ipipe->repeat = 0;
	usbd_free_xfer(xfer);
 bad1:
	usbd_close_pipe(ipipe);
	return (err);
}

usbd_status
usbd_close_pipe(usbd_pipe_handle pipe)
{

	USBHIST_FUNC(); USBHIST_CALLED(usbdebug);

#ifdef DIAGNOSTIC
	if (pipe == NULL) {
		USBHIST_LOG(usbdebug, "pipe == NULL", 0, 0, 0, 0);
		return (USBD_NORMAL_COMPLETION);
	}
#endif

	usbd_lock_pipe(pipe);
	if (--pipe->refcnt != 0) {
		usbd_unlock_pipe(pipe);
		return (USBD_NORMAL_COMPLETION);
	}
	if (! SIMPLEQ_EMPTY(&pipe->queue)) {
		usbd_unlock_pipe(pipe);
		return (USBD_PENDING_REQUESTS);
	}
	LIST_REMOVE(pipe, next);
	pipe->endpoint->refcnt--;
	pipe->methods->close(pipe);
	usbd_unlock_pipe(pipe);
	if (pipe->intrxfer != NULL)
		usbd_free_xfer(pipe->intrxfer);
	free(pipe, M_USB);
	return (USBD_NORMAL_COMPLETION);
}

usbd_status
usbd_transfer(usbd_xfer_handle xfer)
{
	usbd_pipe_handle pipe = xfer->pipe;
	usb_dma_t *dmap = &xfer->dmabuf;
	usbd_status err;
	unsigned int size, flags;

	USBHIST_FUNC(); USBHIST_CALLED(usbdebug);

	USBHIST_LOG(usbdebug,
	    "xfer = %p, flags = %#x, pipe = %p, running = %d",
	    xfer, xfer->flags, pipe, pipe->running);

#ifdef USB_DEBUG
	if (usbdebug > 5)
		usbd_dump_queue(pipe);
#endif
	xfer->done = 0;

	if (pipe->aborting) {
		USBHIST_LOG(usbdebug, "<- done xfer %p, aborting", xfer, 0, 0,
		    0);
		return (USBD_CANCELLED);
	}

	size = xfer->length;
	/* If there is no buffer, allocate one. */
	if (!(xfer->rqflags & URQ_DEV_DMABUF) && size != 0) {
		struct usbd_bus *bus = pipe->device->bus;

#ifdef DIAGNOSTIC
		if (xfer->rqflags & URQ_AUTO_DMABUF)
			printf("usbd_transfer: has old buffer!\n");
#endif
		err = bus->methods->allocm(bus, dmap, size);
		if (err) {
			USBHIST_LOG(usbdebug,
			    "<- done xfer %p, no mem", xfer, 0, 0, 0);
			return (err);
		}
		xfer->rqflags |= URQ_AUTO_DMABUF;
	}

	flags = xfer->flags;

	/* Copy data if going out. */
	if (!(flags & USBD_NO_COPY) && size != 0 && !usbd_xfer_isread(xfer))
		memcpy(KERNADDR(dmap, 0), xfer->buffer, size);

	/* xfer is not valid after the transfer method unless synchronous */
	err = pipe->methods->transfer(xfer);
	USBHIST_LOG(usbdebug, "<- done transfer %p, err = %d", xfer, err, 0, 0);

	if (err != USBD_IN_PROGRESS && err) {
		/* The transfer has not been queued, so free buffer. */
		if (xfer->rqflags & URQ_AUTO_DMABUF) {
			struct usbd_bus *bus = pipe->device->bus;

			bus->methods->freem(bus, &xfer->dmabuf);
			xfer->rqflags &= ~URQ_AUTO_DMABUF;
		}
	}

	if (!(flags & USBD_SYNCHRONOUS)) {
		USBHIST_LOG(usbdebug, "<- done xfer %p, not sync", xfer, 0, 0,
		    0);
		return (err);
	}

	/* Sync transfer, wait for completion. */
	if (err != USBD_IN_PROGRESS) {
		USBHIST_LOG(usbdebug, "<- done xfer %p, not in progress", xfer,
		    0, 0, 0);
		return (err);
	}
	usbd_lock_pipe(pipe);
	while (!xfer->done) {
		if (pipe->device->bus->use_polling)
			panic("usbd_transfer: not done");
		USBHIST_LOG(usbdebug, "<- sleeping on xfer %p", xfer, 0, 0, 0);

		err = 0;
		if ((flags & USBD_SYNCHRONOUS_SIG) != 0) {
			err = cv_wait_sig(&xfer->cv, pipe->device->bus->lock);
		} else {
			cv_wait(&xfer->cv, pipe->device->bus->lock);
		}
		if (err) {
			if (!xfer->done)
				pipe->methods->abort(xfer);
			break;
		}
	}
	usbd_unlock_pipe(pipe);
	return (xfer->status);
}

/* Like usbd_transfer(), but waits for completion. */
usbd_status
usbd_sync_transfer(usbd_xfer_handle xfer)
{
	xfer->flags |= USBD_SYNCHRONOUS;
	return (usbd_transfer(xfer));
}

/* Like usbd_transfer(), but waits for completion and listens for signals. */
usbd_status
usbd_sync_transfer_sig(usbd_xfer_handle xfer)
{
	xfer->flags |= USBD_SYNCHRONOUS | USBD_SYNCHRONOUS_SIG;
	return (usbd_transfer(xfer));
}

void *
usbd_alloc_buffer(usbd_xfer_handle xfer, u_int32_t size)
{
	struct usbd_bus *bus = xfer->device->bus;
	usbd_status err;

#ifdef DIAGNOSTIC
	if (xfer->rqflags & (URQ_DEV_DMABUF | URQ_AUTO_DMABUF))
		printf("usbd_alloc_buffer: xfer already has a buffer\n");
#endif
	err = bus->methods->allocm(bus, &xfer->dmabuf, size);
	if (err)
		return (NULL);
	xfer->rqflags |= URQ_DEV_DMABUF;
	return (KERNADDR(&xfer->dmabuf, 0));
}

void
usbd_free_buffer(usbd_xfer_handle xfer)
{
#ifdef DIAGNOSTIC
	if (!(xfer->rqflags & (URQ_DEV_DMABUF | URQ_AUTO_DMABUF))) {
		printf("usbd_free_buffer: no buffer\n");
		return;
	}
#endif
	xfer->rqflags &= ~(URQ_DEV_DMABUF | URQ_AUTO_DMABUF);
	xfer->device->bus->methods->freem(xfer->device->bus, &xfer->dmabuf);
}

void *
usbd_get_buffer(usbd_xfer_handle xfer)
{
	if (!(xfer->rqflags & URQ_DEV_DMABUF))
		return (NULL);
	return (KERNADDR(&xfer->dmabuf, 0));
}

usbd_xfer_handle
usbd_alloc_xfer(usbd_device_handle dev)
{
	usbd_xfer_handle xfer;

	USBHIST_FUNC(); USBHIST_CALLED(usbdebug);

	xfer = dev->bus->methods->allocx(dev->bus);
	if (xfer == NULL)
		return (NULL);
	xfer->device = dev;
	callout_init(&xfer->timeout_handle, CALLOUT_MPSAFE);
	cv_init(&xfer->cv, "usbxfer");
	cv_init(&xfer->hccv, "usbhcxfer");

	USBHIST_LOG(usbdebug, "returns %p", xfer, 0, 0, 0);

	return (xfer);
}

usbd_status
usbd_free_xfer(usbd_xfer_handle xfer)
{
	USBHIST_FUNC(); USBHIST_CALLED(usbdebug);

	USBHIST_LOG(usbdebug, "%p", xfer, 0, 0, 0);
	if (xfer->rqflags & (URQ_DEV_DMABUF | URQ_AUTO_DMABUF))
		usbd_free_buffer(xfer);
#if defined(DIAGNOSTIC)
	if (callout_pending(&xfer->timeout_handle)) {
		callout_stop(&xfer->timeout_handle);
		printf("usbd_free_xfer: timeout_handle pending\n");
	}
#endif
	cv_destroy(&xfer->cv);
	cv_destroy(&xfer->hccv);
	xfer->device->bus->methods->freex(xfer->device->bus, xfer);
	return (USBD_NORMAL_COMPLETION);
}

void
usbd_setup_xfer(usbd_xfer_handle xfer, usbd_pipe_handle pipe,
		usbd_private_handle priv, void *buffer, u_int32_t length,
		u_int16_t flags, u_int32_t timeout,
		usbd_callback callback)
{
	xfer->pipe = pipe;
	xfer->priv = priv;
	xfer->buffer = buffer;
	xfer->length = length;
	xfer->actlen = 0;
	xfer->flags = flags;
	xfer->timeout = timeout;
	xfer->status = USBD_NOT_STARTED;
	xfer->callback = callback;
	xfer->rqflags &= ~URQ_REQUEST;
	xfer->nframes = 0;
}

void
usbd_setup_default_xfer(usbd_xfer_handle xfer, usbd_device_handle dev,
			usbd_private_handle priv, u_int32_t timeout,
			usb_device_request_t *req, void *buffer,
			u_int32_t length, u_int16_t flags,
			usbd_callback callback)
{
	xfer->pipe = dev->default_pipe;
	xfer->priv = priv;
	xfer->buffer = buffer;
	xfer->length = length;
	xfer->actlen = 0;
	xfer->flags = flags;
	xfer->timeout = timeout;
	xfer->status = USBD_NOT_STARTED;
	xfer->callback = callback;
	xfer->request = *req;
	xfer->rqflags |= URQ_REQUEST;
	xfer->nframes = 0;
}

void
usbd_setup_isoc_xfer(usbd_xfer_handle xfer, usbd_pipe_handle pipe,
		     usbd_private_handle priv, u_int16_t *frlengths,
		     u_int32_t nframes, u_int16_t flags, usbd_callback callback)
{
	xfer->pipe = pipe;
	xfer->priv = priv;
	xfer->buffer = 0;
	xfer->length = 0;
	xfer->actlen = 0;
	xfer->flags = flags;
	xfer->timeout = USBD_NO_TIMEOUT;
	xfer->status = USBD_NOT_STARTED;
	xfer->callback = callback;
	xfer->rqflags &= ~URQ_REQUEST;
	xfer->frlengths = frlengths;
	xfer->nframes = nframes;
}

void
usbd_get_xfer_status(usbd_xfer_handle xfer, usbd_private_handle *priv,
		     void **buffer, u_int32_t *count, usbd_status *status)
{
	if (priv != NULL)
		*priv = xfer->priv;
	if (buffer != NULL)
		*buffer = xfer->buffer;
	if (count != NULL)
		*count = xfer->actlen;
	if (status != NULL)
		*status = xfer->status;
}

usb_config_descriptor_t *
usbd_get_config_descriptor(usbd_device_handle dev)
{
#ifdef DIAGNOSTIC
	if (dev == NULL) {
		printf("usbd_get_config_descriptor: dev == NULL\n");
		return (NULL);
	}
#endif
	return (dev->cdesc);
}

usb_interface_descriptor_t *
usbd_get_interface_descriptor(usbd_interface_handle iface)
{
#ifdef DIAGNOSTIC
	if (iface == NULL) {
		printf("usbd_get_interface_descriptor: dev == NULL\n");
		return (NULL);
	}
#endif
	return (iface->idesc);
}

usb_device_descriptor_t *
usbd_get_device_descriptor(usbd_device_handle dev)
{
	return (&dev->ddesc);
}

usb_endpoint_descriptor_t *
usbd_interface2endpoint_descriptor(usbd_interface_handle iface, u_int8_t index)
{
	if (index >= iface->idesc->bNumEndpoints)
		return (NULL);
	return (iface->endpoints[index].edesc);
}

/* Some drivers may wish to abort requests on the default pipe, *
 * but there is no mechanism for getting a handle on it.        */
usbd_status
usbd_abort_default_pipe(struct usbd_device *device)
{

	return usbd_abort_pipe(device->default_pipe);
}

usbd_status
usbd_abort_pipe(usbd_pipe_handle pipe)
{
	usbd_status err;

#ifdef DIAGNOSTIC
	if (pipe == NULL) {
		printf("usbd_abort_pipe: pipe==NULL\n");
		return (USBD_NORMAL_COMPLETION);
	}
#endif
	usbd_lock_pipe(pipe);
	err = usbd_ar_pipe(pipe);
	usbd_unlock_pipe(pipe);
	return (err);
}

usbd_status
usbd_clear_endpoint_stall(usbd_pipe_handle pipe)
{
	usbd_device_handle dev = pipe->device;
	usb_device_request_t req;
	usbd_status err;

	USBHIST_FUNC(); USBHIST_CALLED(usbdebug);

	/*
	 * Clearing en endpoint stall resets the endpoint toggle, so
	 * do the same to the HC toggle.
	 */
	pipe->methods->cleartoggle(pipe);

	req.bmRequestType = UT_WRITE_ENDPOINT;
	req.bRequest = UR_CLEAR_FEATURE;
	USETW(req.wValue, UF_ENDPOINT_HALT);
	USETW(req.wIndex, pipe->endpoint->edesc->bEndpointAddress);
	USETW(req.wLength, 0);
	err = usbd_do_request(dev, &req, 0);
#if 0
XXX should we do this?
	if (!err) {
		pipe->state = USBD_PIPE_ACTIVE;
		/* XXX activate pipe */
	}
#endif
	return (err);
}

void
usbd_clear_endpoint_stall_task(void *arg)
{
	usbd_pipe_handle pipe = arg;
	usbd_device_handle dev = pipe->device;
	usb_device_request_t req;

	pipe->methods->cleartoggle(pipe);

	req.bmRequestType = UT_WRITE_ENDPOINT;
	req.bRequest = UR_CLEAR_FEATURE;
	USETW(req.wValue, UF_ENDPOINT_HALT);
	USETW(req.wIndex, pipe->endpoint->edesc->bEndpointAddress);
	USETW(req.wLength, 0);
	(void)usbd_do_request(dev, &req, 0);
}

void
usbd_clear_endpoint_stall_async(usbd_pipe_handle pipe)
{

	usb_add_task(pipe->device, &pipe->async_task, USB_TASKQ_DRIVER);
}

void
usbd_clear_endpoint_toggle(usbd_pipe_handle pipe)
{
	pipe->methods->cleartoggle(pipe);
}

usbd_status
usbd_endpoint_count(usbd_interface_handle iface, u_int8_t *count)
{
#ifdef DIAGNOSTIC
	if (iface == NULL || iface->idesc == NULL) {
		printf("usbd_endpoint_count: NULL pointer\n");
		return (USBD_INVAL);
	}
#endif
	*count = iface->idesc->bNumEndpoints;
	return (USBD_NORMAL_COMPLETION);
}

usbd_status
usbd_interface_count(usbd_device_handle dev, u_int8_t *count)
{
	if (dev->cdesc == NULL)
		return (USBD_NOT_CONFIGURED);
	*count = dev->cdesc->bNumInterface;
	return (USBD_NORMAL_COMPLETION);
}

void
usbd_interface2device_handle(usbd_interface_handle iface,
			     usbd_device_handle *dev)
{
	*dev = iface->device;
}

usbd_status
usbd_device2interface_handle(usbd_device_handle dev,
			     u_int8_t ifaceno, usbd_interface_handle *iface)
{
	if (dev->cdesc == NULL)
		return (USBD_NOT_CONFIGURED);
	if (ifaceno >= dev->cdesc->bNumInterface)
		return (USBD_INVAL);
	*iface = &dev->ifaces[ifaceno];
	return (USBD_NORMAL_COMPLETION);
}

usbd_device_handle
usbd_pipe2device_handle(usbd_pipe_handle pipe)
{
	return (pipe->device);
}

/* XXXX use altno */
usbd_status
usbd_set_interface(usbd_interface_handle iface, int altidx)
{
	usb_device_request_t req;
	usbd_status err;
	void *endpoints;

	if (LIST_FIRST(&iface->pipes) != 0)
		return (USBD_IN_USE);

	endpoints = iface->endpoints;
	err = usbd_fill_iface_data(iface->device, iface->index, altidx);
	if (err)
		return (err);

	/* new setting works, we can free old endpoints */
	if (endpoints != NULL)
		free(endpoints, M_USB);

#ifdef DIAGNOSTIC
	if (iface->idesc == NULL) {
		printf("usbd_set_interface: NULL pointer\n");
		return (USBD_INVAL);
	}
#endif

	req.bmRequestType = UT_WRITE_INTERFACE;
	req.bRequest = UR_SET_INTERFACE;
	USETW(req.wValue, iface->idesc->bAlternateSetting);
	USETW(req.wIndex, iface->idesc->bInterfaceNumber);
	USETW(req.wLength, 0);
	return (usbd_do_request(iface->device, &req, 0));
}

int
usbd_get_no_alts(usb_config_descriptor_t *cdesc, int ifaceno)
{
	char *p = (char *)cdesc;
	char *end = p + UGETW(cdesc->wTotalLength);
	usb_interface_descriptor_t *d;
	int n;

	for (n = 0; p < end; p += d->bLength) {
		d = (usb_interface_descriptor_t *)p;
		if (p + d->bLength <= end &&
		    d->bDescriptorType == UDESC_INTERFACE &&
		    d->bInterfaceNumber == ifaceno)
			n++;
	}
	return (n);
}

int
usbd_get_interface_altindex(usbd_interface_handle iface)
{
	return (iface->altindex);
}

usbd_status
usbd_get_interface(usbd_interface_handle iface, u_int8_t *aiface)
{
	usb_device_request_t req;

	req.bmRequestType = UT_READ_INTERFACE;
	req.bRequest = UR_GET_INTERFACE;
	USETW(req.wValue, 0);
	USETW(req.wIndex, iface->idesc->bInterfaceNumber);
	USETW(req.wLength, 1);
	return (usbd_do_request(iface->device, &req, aiface));
}

/*** Internal routines ***/

/* Dequeue all pipe operations, called at splusb(). */
Static usbd_status
usbd_ar_pipe(usbd_pipe_handle pipe)
{
	usbd_xfer_handle xfer;

	USBHIST_FUNC(); USBHIST_CALLED(usbdebug);

	KASSERT(mutex_owned(pipe->device->bus->lock));

	USBHIST_LOG(usbdebug, "pipe = %p", pipe, 0, 0, 0);
#ifdef USB_DEBUG
	if (usbdebug > 5)
		usbd_dump_queue(pipe);
#endif
	pipe->repeat = 0;
	pipe->aborting = 1;
	while ((xfer = SIMPLEQ_FIRST(&pipe->queue)) != NULL) {
		USBHIST_LOG(usbdebug, "pipe = %p xfer = %p (methods = %p)",
		    pipe, xfer, pipe->methods, 0);
		/* Make the HC abort it (and invoke the callback). */
		pipe->methods->abort(xfer);
		/* XXX only for non-0 usbd_clear_endpoint_stall(pipe); */
	}
	pipe->aborting = 0;
	return (USBD_NORMAL_COMPLETION);
}

/* Called with USB lock held. */
void
usb_transfer_complete(usbd_xfer_handle xfer)
{
	usbd_pipe_handle pipe = xfer->pipe;
	usb_dma_t *dmap = &xfer->dmabuf;
	int sync = xfer->flags & USBD_SYNCHRONOUS;
	int erred = xfer->status == USBD_CANCELLED ||
	    xfer->status == USBD_TIMEOUT;
	int polling = pipe->device->bus->use_polling;
	int repeat;

	USBHIST_FUNC(); USBHIST_CALLED(usbdebug);

	USBHIST_LOG(usbdebug, "pipe = %p xfer = %p status = %d actlen = %d",
		pipe, xfer, xfer->status, xfer->actlen);

	KASSERT(polling || mutex_owned(pipe->device->bus->lock));

#ifdef DIAGNOSTIC
	if (xfer->busy_free != XFER_ONQU) {
		printf("usb_transfer_complete: xfer=%p not queued 0x%08x\n",
		       xfer, xfer->busy_free);
	}
#endif

#ifdef DIAGNOSTIC
	if (pipe == NULL) {
		printf("usb_transfer_complete: pipe==0, xfer=%p\n", xfer);
		return;
	}
#endif
	repeat = pipe->repeat;
	/* XXXX */
	if (polling)
		pipe->running = 0;

	if (!(xfer->flags & USBD_NO_COPY) && xfer->actlen != 0 &&
	    usbd_xfer_isread(xfer)) {
		if (xfer->actlen > xfer->length) {
#ifdef DIAGNOSTIC
			printf("%s: actlen (%d) > len (%d)\n", __func__,
			       xfer->actlen, xfer->length);
#endif
			xfer->actlen = xfer->length;
		}
		memcpy(xfer->buffer, KERNADDR(dmap, 0), xfer->actlen);
	}

	/* if we allocated the buffer in usbd_transfer() we free it here. */
	if (xfer->rqflags & URQ_AUTO_DMABUF) {
		if (!repeat) {
			struct usbd_bus *bus = pipe->device->bus;
			bus->methods->freem(bus, dmap);
			xfer->rqflags &= ~URQ_AUTO_DMABUF;
		}
	}

	if (!repeat) {
		/* Remove request from queue. */

		KASSERTMSG(!SIMPLEQ_EMPTY(&pipe->queue),
		    "pipe %p is empty, but xfer %p wants to complete", pipe,
		     xfer);
#ifdef DIAGNOSTIC
		if (xfer != SIMPLEQ_FIRST(&pipe->queue))
			printf("%s: bad dequeue %p != %p\n", __func__,
			       xfer, SIMPLEQ_FIRST(&pipe->queue));
		xfer->busy_free = XFER_BUSY;
#endif
		SIMPLEQ_REMOVE_HEAD(&pipe->queue, next);
	}
	USBHIST_LOG(usbdebug, "xfer %p: repeat %d new head = %p",
	    xfer, repeat, SIMPLEQ_FIRST(&pipe->queue), 0);

	/* Count completed transfers. */
	++pipe->device->bus->stats.uds_requests
		[pipe->endpoint->edesc->bmAttributes & UE_XFERTYPE];

	xfer->done = 1;
	if (!xfer->status && xfer->actlen < xfer->length &&
	    !(xfer->flags & USBD_SHORT_XFER_OK)) {
		USBHIST_LOG(usbdebug, "short transfer %d < %d",
		    xfer->actlen, xfer->length, 0, 0);
		xfer->status = USBD_SHORT_XFER;
	}

	if (repeat) {
		USBHIST_LOG(usbdebug, "xfer %p doing callback %p status %x",
		    xfer, xfer->callback, xfer->status, 0);
		if (xfer->callback) {
			if (!polling)
				mutex_exit(pipe->device->bus->lock);

			if (!(pipe->flags & USBD_MPSAFE))
				KERNEL_LOCK(1, curlwp);
			xfer->callback(xfer, xfer->priv, xfer->status);
			USBHIST_LOG(usbdebug, "xfer %p doing done %p", xfer,
			    pipe->methods->done, 0, 0);
			if (!(pipe->flags & USBD_MPSAFE))
				KERNEL_UNLOCK_ONE(curlwp);

			if (!polling)
				mutex_enter(pipe->device->bus->lock);
		}
		pipe->methods->done(xfer);
	} else {
		USBHIST_LOG(usbdebug, "xfer %p doing done %p", xfer,
		    pipe->methods->done, 0, 0);
		pipe->methods->done(xfer);
		USBHIST_LOG(usbdebug, "xfer %p doing callback %p status %x",
		    xfer, xfer->callback, xfer->status, 0);
		if (xfer->callback) {
			if (!polling)
				mutex_exit(pipe->device->bus->lock);

			if (!(pipe->flags & USBD_MPSAFE))
				KERNEL_LOCK(1, curlwp);
			xfer->callback(xfer, xfer->priv, xfer->status);
			if (!(pipe->flags & USBD_MPSAFE))
				KERNEL_UNLOCK_ONE(curlwp);

			if (!polling)
				mutex_enter(pipe->device->bus->lock);
		}
	}

	if (sync && !polling) {
		USBHIST_LOG(usbdebug, "<- done xfer %p, wakeup", xfer, 0, 0, 0);
		cv_broadcast(&xfer->cv);
	}

	if (!repeat) {
		/* XXX should we stop the queue on all errors? */
		if (erred && pipe->iface != NULL)	/* not control pipe */
			pipe->running = 0;
		else
			usbd_start_next(pipe);
	}
}

/* Called with USB lock held. */
usbd_status
usb_insert_transfer(usbd_xfer_handle xfer)
{
	usbd_pipe_handle pipe = xfer->pipe;
	usbd_status err;

	USBHIST_FUNC(); USBHIST_CALLED(usbdebug);

	USBHIST_LOG(usbdebug, "pipe = %p running = %d timeout = %d",
	    pipe, pipe->running, xfer->timeout, 0);

	KASSERT(mutex_owned(pipe->device->bus->lock));

#ifdef DIAGNOSTIC
	if (xfer->busy_free != XFER_BUSY) {
		USBHIST_LOG(usbdebug, "<- done, xfer %p not busy", xfer, 0, 0,
		    0);
		printf("usb_insert_transfer: xfer=%p not busy 0x%08x\n",
		       xfer, xfer->busy_free);
		return (USBD_INVAL);
	}
	xfer->busy_free = XFER_ONQU;
#endif
	SIMPLEQ_INSERT_TAIL(&pipe->queue, xfer, next);
	if (pipe->running)
		err = USBD_IN_PROGRESS;
	else {
		pipe->running = 1;
		err = USBD_NORMAL_COMPLETION;
	}
	USBHIST_LOG(usbdebug, "<- done xfer %p, err %d", xfer, err, 0, 0);
	return (err);
}

/* Called with USB lock held. */
void
usbd_start_next(usbd_pipe_handle pipe)
{
	usbd_xfer_handle xfer;
	usbd_status err;

	USBHIST_FUNC(); USBHIST_CALLED(usbdebug);

#ifdef DIAGNOSTIC
	if (pipe == NULL) {
		printf("usbd_start_next: pipe == NULL\n");
		return;
	}
	if (pipe->methods == NULL || pipe->methods->start == NULL) {
		printf("usbd_start_next: pipe=%p no start method\n", pipe);
		return;
	}
#endif

	int polling = pipe->device->bus->use_polling;
	KASSERT(polling || mutex_owned(pipe->device->bus->lock));

	/* Get next request in queue. */
	xfer = SIMPLEQ_FIRST(&pipe->queue);
	USBHIST_LOG(usbdebug, "pipe = %p, xfer = %p", pipe, xfer, 0, 0);
	if (xfer == NULL) {
		pipe->running = 0;
	} else {
		if (!polling)
			mutex_exit(pipe->device->bus->lock);
		err = pipe->methods->start(xfer);
		if (!polling)
			mutex_enter(pipe->device->bus->lock);

		if (err != USBD_IN_PROGRESS) {
			USBHIST_LOG(usbdebug, "error = %d", err, 0, 0, 0);
			pipe->running = 0;
			/* XXX do what? */
		}
	}

	KASSERT(polling || mutex_owned(pipe->device->bus->lock));
}

usbd_status
usbd_do_request(usbd_device_handle dev, usb_device_request_t *req, void *data)
{
	return (usbd_do_request_flags(dev, req, data, 0, 0,
				      USBD_DEFAULT_TIMEOUT));
}

usbd_status
usbd_do_request_flags(usbd_device_handle dev, usb_device_request_t *req,
		      void *data, u_int16_t flags, int *actlen, u_int32_t timo)
{
	return (usbd_do_request_flags_pipe(dev, dev->default_pipe, req,
					   data, flags, actlen, timo));
}

usbd_status
usbd_do_request_flags_pipe(usbd_device_handle dev, usbd_pipe_handle pipe,
	usb_device_request_t *req, void *data, u_int16_t flags, int *actlen,
	u_int32_t timeout)
{
	usbd_xfer_handle xfer;
	usbd_status err;

	USBHIST_FUNC(); USBHIST_CALLED(usbdebug);

#ifdef DIAGNOSTIC
	if (cpu_intr_p() || cpu_softintr_p()) {
		USBHIST_LOG(usbdebug, "not in process context", 0, 0, 0, 0);
		return (USBD_INVAL);
	}
#endif

	xfer = usbd_alloc_xfer(dev);
	if (xfer == NULL)
		return (USBD_NOMEM);
	usbd_setup_default_xfer(xfer, dev, 0, timeout, req,
				data, UGETW(req->wLength), flags, 0);
	xfer->pipe = pipe;
	err = usbd_sync_transfer(xfer);
#if defined(USB_DEBUG) || defined(DIAGNOSTIC)
	if (xfer->actlen > xfer->length) {
		USBHIST_LOG(usbdebug, "overrun addr = %d type = 0x%02x",
		    dev->address, xfer->request.bmRequestType, 0, 0);
		USBHIST_LOG(usbdebug, "     req = 0x%02x val = %d index = %d",
		    xfer->request.bRequest, UGETW(xfer->request.wValue),
		    UGETW(xfer->request.wIndex), 0);
		USBHIST_LOG(usbdebug, "     rlen = %d length = %d actlen = %d",
		    UGETW(xfer->request.wLength),
		    xfer->length, xfer->actlen, 0);
	}
#endif
	if (actlen != NULL)
		*actlen = xfer->actlen;
	if (err == USBD_STALLED) {
		/*
		 * The control endpoint has stalled.  Control endpoints
		 * should not halt, but some may do so anyway so clear
		 * any halt condition.
		 */
		usb_device_request_t treq;
		usb_status_t status;
		u_int16_t s;
		usbd_status nerr;

		treq.bmRequestType = UT_READ_ENDPOINT;
		treq.bRequest = UR_GET_STATUS;
		USETW(treq.wValue, 0);
		USETW(treq.wIndex, 0);
		USETW(treq.wLength, sizeof(usb_status_t));
		usbd_setup_default_xfer(xfer, dev, 0, USBD_DEFAULT_TIMEOUT,
					   &treq, &status,sizeof(usb_status_t),
					   0, 0);
		nerr = usbd_sync_transfer(xfer);
		if (nerr)
			goto bad;
		s = UGETW(status.wStatus);
		USBHIST_LOG(usbdebug, "status = 0x%04x", s, 0, 0, 0);
		if (!(s & UES_HALT))
			goto bad;
		treq.bmRequestType = UT_WRITE_ENDPOINT;
		treq.bRequest = UR_CLEAR_FEATURE;
		USETW(treq.wValue, UF_ENDPOINT_HALT);
		USETW(treq.wIndex, 0);
		USETW(treq.wLength, 0);
		usbd_setup_default_xfer(xfer, dev, 0, USBD_DEFAULT_TIMEOUT,
					   &treq, &status, 0, 0, 0);
		nerr = usbd_sync_transfer(xfer);
		if (nerr)
			goto bad;
	}

 bad:
	if (err) {
		USBHIST_LOG(usbdebug, "returning err = %s",
		    usbd_errstr(err), 0, 0, 0);
	}
	usbd_free_xfer(xfer);
	return (err);
}

const struct usbd_quirks *
usbd_get_quirks(usbd_device_handle dev)
{
#ifdef DIAGNOSTIC
	if (dev == NULL) {
		printf("usbd_get_quirks: dev == NULL\n");
		return 0;
	}
#endif
	return (dev->quirks);
}

/* XXX do periodic free() of free list */

/*
 * Called from keyboard driver when in polling mode.
 */
void
usbd_dopoll(usbd_interface_handle iface)
{
	iface->device->bus->methods->do_poll(iface->device->bus);
}

/*
 * XXX use this more???  use_polling it touched manually all over
 */
void
usbd_set_polling(usbd_device_handle dev, int on)
{
	if (on)
		dev->bus->use_polling++;
	else
		dev->bus->use_polling--;

	/* Kick the host controller when switching modes */
	mutex_enter(dev->bus->lock);
	(*dev->bus->methods->soft_intr)(dev->bus);
	mutex_exit(dev->bus->lock);
}


usb_endpoint_descriptor_t *
usbd_get_endpoint_descriptor(usbd_interface_handle iface, u_int8_t address)
{
	struct usbd_endpoint *ep;
	int i;

	for (i = 0; i < iface->idesc->bNumEndpoints; i++) {
		ep = &iface->endpoints[i];
		if (ep->edesc->bEndpointAddress == address)
			return (iface->endpoints[i].edesc);
	}
	return (NULL);
}

/*
 * usbd_ratecheck() can limit the number of error messages that occurs.
 * When a device is unplugged it may take up to 0.25s for the hub driver
 * to notice it.  If the driver continuosly tries to do I/O operations
 * this can generate a large number of messages.
 */
int
usbd_ratecheck(struct timeval *last)
{
	static struct timeval errinterval = { 0, 250000 }; /* 0.25 s*/

	return (ratecheck(last, &errinterval));
}

/*
 * Search for a vendor/product pair in an array.  The item size is
 * given as an argument.
 */
const struct usb_devno *
usb_match_device(const struct usb_devno *tbl, u_int nentries, u_int sz,
		 u_int16_t vendor, u_int16_t product)
{
	while (nentries-- > 0) {
		u_int16_t tproduct = tbl->ud_product;
		if (tbl->ud_vendor == vendor &&
		    (tproduct == product || tproduct == USB_PRODUCT_ANY))
			return (tbl);
		tbl = (const struct usb_devno *)((const char *)tbl + sz);
	}
	return (NULL);
}


void
usb_desc_iter_init(usbd_device_handle dev, usbd_desc_iter_t *iter)
{
	const usb_config_descriptor_t *cd = usbd_get_config_descriptor(dev);

        iter->cur = (const uByte *)cd;
        iter->end = (const uByte *)cd + UGETW(cd->wTotalLength);
}

const usb_descriptor_t *
usb_desc_iter_next(usbd_desc_iter_t *iter)
{
	const usb_descriptor_t *desc;

	if (iter->cur + sizeof(usb_descriptor_t) >= iter->end) {
		if (iter->cur != iter->end)
			printf("usb_desc_iter_next: bad descriptor\n");
		return NULL;
	}
	desc = (const usb_descriptor_t *)iter->cur;
	if (desc->bLength == 0) {
		printf("usb_desc_iter_next: descriptor length = 0\n");
		return NULL;
	}
	iter->cur += desc->bLength;
	if (iter->cur > iter->end) {
		printf("usb_desc_iter_next: descriptor length too large\n");
		return NULL;
	}
	return desc;
}

usbd_status
usbd_get_string(usbd_device_handle dev, int si, char *buf)
{
	return usbd_get_string0(dev, si, buf, 1);
}

usbd_status
usbd_get_string0(usbd_device_handle dev, int si, char *buf, int unicode)
{
	int swap = dev->quirks->uq_flags & UQ_SWAP_UNICODE;
	usb_string_descriptor_t us;
	char *s;
	int i, n;
	u_int16_t c;
	usbd_status err;
	int size;

	USBHIST_FUNC(); USBHIST_CALLED(usbdebug);

	buf[0] = '\0';
	if (si == 0)
		return (USBD_INVAL);
	if (dev->quirks->uq_flags & UQ_NO_STRINGS)
		return (USBD_STALLED);
	if (dev->langid == USBD_NOLANG) {
		/* Set up default language */
		err = usbd_get_string_desc(dev, USB_LANGUAGE_TABLE, 0, &us,
		    &size);
		if (err || size < 4) {
			USBHIST_LOG(usbdebug, "getting lang failed, using 0",
			    0, 0, 0, 0);
			dev->langid = 0; /* Well, just pick something then */
		} else {
			/* Pick the first language as the default. */
			dev->langid = UGETW(us.bString[0]);
		}
	}
	err = usbd_get_string_desc(dev, si, dev->langid, &us, &size);
	if (err)
		return (err);
	s = buf;
	n = size / 2 - 1;
	if (unicode) {
		for (i = 0; i < n; i++) {
			c = UGETW(us.bString[i]);
			if (swap)
				c = (c >> 8) | (c << 8);
			s += wput_utf8(s, 3, c);
		}
		*s++ = 0;
	}
#ifdef COMPAT_30
	else {
		for (i = 0; i < n; i++) {
			c = UGETW(us.bString[i]);
			if (swap)
				c = (c >> 8) | (c << 8);
			*s++ = (c < 0x80) ? c : '?';
		}
		*s++ = 0;
	}
#endif
	return (USBD_NORMAL_COMPLETION);
}

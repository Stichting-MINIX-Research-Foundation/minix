/*	$NetBSD: udl.c,v 1.12 2014/12/12 05:19:33 msaitoh Exp $	*/

/*-
 * Copyright (c) 2009 FUKAUMI Naoki.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 2009 Marcus Glocker <mglocker@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Driver for the ``DisplayLink DL-1x0 / DL-1x5'' graphic chips based
 * on the reversed engineered specifications of Florian Echtler
 * <floe at butterbrot dot org>:
 *
 * 	http://floe.butterbrot.org/displaylink/doku.php
 *
 * This driver was written by Marcus Glocker for OpenBSD and ported to
 * NetBSD by FUKAUMI Naoki with many modification.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: udl.c,v 1.12 2014/12/12 05:19:33 msaitoh Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <uvm/uvm.h>

#include <sys/bus.h>
#include <sys/endian.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usb_mem.h>
#include <dev/usb/usbdevs.h>

#include <dev/firmload.h>

#include <dev/videomode/videomode.h>
#include <dev/videomode/edidvar.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#include <dev/usb/udl.h>
#ifdef notyet
#include <dev/usb/udlio.h>
#endif

/*
 * Defines.
 */
#ifdef UDL_DEBUG
#define DPRINTF(x)	do { if (udl_debug) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (udl_debug >= (n)) printf x; } while (0)
int udl_debug = 1;
#else
#define DPRINTF(x)	do {} while (0)
#define DPRINTFN(n, x)	do {} while (0)
#endif

/*
 * Prototypes.
 */
static int		udl_match(device_t, cfdata_t, void *);
static void		udl_attach(device_t, device_t, void *);
static int		udl_detach(device_t, int);

static int		udl_ioctl(void *, void *, u_long, void *, int,
			    struct lwp *);
static paddr_t		udl_mmap(void *, void *, off_t, int);
static int		udl_alloc_screen(void *, const struct wsscreen_descr *,
			    void **, int *, int *, long *);
static void		udl_free_screen(void *, void *);
static int		udl_show_screen(void *, void *, int,
			    void (*)(void *, int, int), void *);

static void		udl_comp_load(struct udl_softc *);
static void		udl_comp_unload(struct udl_softc *);
static int		udl_fbmem_alloc(struct udl_softc *);
static void		udl_fbmem_free(struct udl_softc *);
static int		udl_cmdq_alloc(struct udl_softc *);
static void		udl_cmdq_free(struct udl_softc *);
static struct udl_cmdq *udl_cmdq_get(struct udl_softc *sc);
static void		udl_cmdq_put(struct udl_softc *sc,
			    struct udl_cmdq *cmdq);
static void		udl_cmdq_flush(struct udl_softc *);

static void		udl_cursor(void *, int, int, int);
static void		udl_putchar(void *, int, int, u_int, long);
static void		udl_copycols(void *, int, int, int, int);
static void		udl_erasecols(void *, int, int, int, long);
static void		udl_copyrows(void *, int, int, int);
static void		udl_eraserows(void *, int, int, long);

static void		udl_restore_char(struct rasops_info *);
static void		udl_draw_char(struct rasops_info *, uint16_t *, u_int,
			    int, int);
static void		udl_copy_rect(struct udl_softc *, int, int, int, int,
			    int, int);
static void		udl_fill_rect(struct udl_softc *, uint16_t, int, int,
			    int, int);
#ifdef notyet
static void		udl_draw_rect(struct udl_softc *,
			    struct udl_ioctl_damage *);
static void		udl_draw_rect_comp(struct udl_softc *,
			    struct udl_ioctl_damage *);
#endif

static inline void	udl_copy_line(struct udl_softc *, int, int, int);
static inline void	udl_fill_line(struct udl_softc *, uint16_t, int, int);
static inline void	udl_draw_line(struct udl_softc *, uint16_t *, int,
			    int);
#ifdef notyet
static inline void	udl_draw_line_comp(struct udl_softc *, uint16_t *, int,
			    int);
#endif

static int		udl_cmd_send(struct udl_softc *);
static void		udl_cmd_send_async(struct udl_softc *);
static void		udl_cmd_send_async_cb(usbd_xfer_handle,
			    usbd_private_handle, usbd_status);

static int		udl_ctrl_msg(struct udl_softc *, uint8_t, uint8_t,
			    uint16_t, uint16_t, uint8_t *, uint16_t);
static int		udl_init(struct udl_softc *);
static void		udl_read_edid(struct udl_softc *);
static void		udl_set_address(struct udl_softc *, int, int, int,
			    int);
static void		udl_blank(struct udl_softc *, int);
static uint16_t		udl_lfsr(uint16_t);
static int		udl_set_resolution(struct udl_softc *,
			    const struct videomode *);
static const struct videomode *udl_videomode_lookup(const char *);

static inline void
udl_cmd_add_1(struct udl_softc *sc, uint8_t val)
{

	*sc->sc_cmd_buf++ = val;
}

static inline void
udl_cmd_add_2(struct udl_softc *sc, uint16_t val)
{

	be16enc(sc->sc_cmd_buf, val);
	sc->sc_cmd_buf += 2;
}

static inline void
udl_cmd_add_3(struct udl_softc *sc, uint32_t val)
{

	udl_cmd_add_2(sc, val >> 8);
	udl_cmd_add_1(sc, val);
}

static inline void
udl_cmd_add_4(struct udl_softc *sc, uint32_t val)
{

	be32enc(sc->sc_cmd_buf, val);
	sc->sc_cmd_buf += 4;
}

static inline void
udl_cmd_add_buf(struct udl_softc *sc, uint16_t *buf, int width)
{
#if BYTE_ORDER == BIG_ENDIAN
	memcpy(sc->sc_cmd_buf, buf, width * 2);
	sc->sc_cmd_buf += width * 2;
#else
	uint16_t *endp;

	endp = buf + width;

	if (((uintptr_t)sc->sc_cmd_buf & 1) == 0) {
		while (buf < endp) {
			*(uint16_t *)sc->sc_cmd_buf = htobe16(*buf++);
			sc->sc_cmd_buf += 2;
		}
	} else {
		while (buf < endp) {
			be16enc(sc->sc_cmd_buf, *buf++);
			sc->sc_cmd_buf += 2;
		}
	}
#endif
}

static inline void
udl_reg_write_1(struct udl_softc *sc, uint8_t reg, uint8_t val)
{

	udl_cmd_add_4(sc, (UDL_BULK_SOC << 24) |
	    (UDL_BULK_CMD_REG_WRITE_1 << 16) | (reg << 8) | val);
}

static inline void
udl_reg_write_2(struct udl_softc *sc, uint8_t reg, uint16_t val)
{

	udl_reg_write_1(sc, reg++, val >> 8);
	udl_reg_write_1(sc, reg, val);
}

static inline void
udl_reg_write_3(struct udl_softc *sc, uint8_t reg, uint32_t val)
{

	udl_reg_write_1(sc, reg++, val >> 16);
	udl_reg_write_1(sc, reg++, val >> 8);
	udl_reg_write_1(sc, reg, val);
}

/* XXX */
static int
firmware_load(const char *dname, const char *iname, uint8_t **ucodep,
    size_t *sizep)
{
	firmware_handle_t fh;
	int error;

	if ((error = firmware_open(dname, iname, &fh)) != 0)
		return error;
	*sizep = firmware_get_size(fh);
	if ((*ucodep = firmware_malloc(*sizep)) == NULL) {
		firmware_close(fh);
		return ENOMEM;
	}
	if ((error = firmware_read(fh, 0, *ucodep, *sizep)) != 0)
		firmware_free(*ucodep, *sizep);
	firmware_close(fh);

	return error;
}

/*
 * Driver glue.
 */
CFATTACH_DECL_NEW(udl, sizeof(struct udl_softc),
	udl_match, udl_attach, udl_detach, NULL);

/*
 * wsdisplay glue.
 */
static struct wsdisplay_accessops udl_accessops = {
	udl_ioctl,
	udl_mmap,
	udl_alloc_screen,
	udl_free_screen,
	udl_show_screen,
	NULL,
	NULL,
	NULL,
};

/*
 * Matching devices.
 */
static const struct usb_devno udl_devs[] = {
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_GUC2020 },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_LD220 },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_LD190 },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_U70 },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_VCUD60 },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_CONV },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_DLDVI },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_USBRGB },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_LCDUSB7X },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_LCDUSB10X },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_VGA10 },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_WSDVI },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_EC008 },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_GXDVIU2 },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_GXDVIU2B },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_LCD4300U },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_LCD8000U },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_HPDOCK },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_NL571 },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_M01061 },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_NBDOCK },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_SWDVI },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_LUM70 },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_LCD8000UD_DVI },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_LDEWX015U },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_LT1421WIDE },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_SD_U2VDH },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_UM7X0 }
};

static int
udl_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	if (usb_lookup(udl_devs, uaa->vendor, uaa->product) != NULL)
		return UMATCH_VENDOR_PRODUCT;

	return UMATCH_NONE;
}

static void
udl_attach(device_t parent, device_t self, void *aux)
{
	struct udl_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	struct wsemuldisplaydev_attach_args aa;
	const struct videomode *vmp;
	usbd_status error;
	char *devinfop;

	aprint_naive("\n");
	aprint_normal("\n");

	sc->sc_dev = self;
	sc->sc_udev = uaa->device;

	devinfop = usbd_devinfo_alloc(sc->sc_udev, 0);
	aprint_normal_dev(sc->sc_dev, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	/*
	 * Set device configuration descriptor number.
	 */
	error = usbd_set_config_no(sc->sc_udev, 1, 0);
	if (error != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(self, "failed to set configuration"
		    ", err=%s\n", usbd_errstr(error));
		return;
	}

	/*
	 * Create device handle to interface descriptor.
	 */
	error = usbd_device2interface_handle(sc->sc_udev, 0, &sc->sc_iface);
	if (error != USBD_NORMAL_COMPLETION)
		return;

	/*
	 * Open bulk TX pipe.
	 */
	error = usbd_open_pipe(sc->sc_iface, 1, USBD_EXCLUSIVE_USE,
	    &sc->sc_tx_pipeh);
	if (error != USBD_NORMAL_COMPLETION)
		return;

	/*
	 * Allocate bulk command queue.
	 */
#ifdef UDL_EVENT_COUNTERS
	evcnt_attach_dynamic(&sc->sc_ev_cmdq_get, EVCNT_TYPE_MISC, NULL,
	    device_xname(sc->sc_dev), "udl_cmdq_get");
	evcnt_attach_dynamic(&sc->sc_ev_cmdq_put, EVCNT_TYPE_MISC, NULL,
	    device_xname(sc->sc_dev), "udl_cmdq_put");
	evcnt_attach_dynamic(&sc->sc_ev_cmdq_wait, EVCNT_TYPE_MISC, NULL,
	    device_xname(sc->sc_dev), "udl_cmdq_wait");
	evcnt_attach_dynamic(&sc->sc_ev_cmdq_timeout, EVCNT_TYPE_MISC, NULL,
	    device_xname(sc->sc_dev), "udl_cmdq_timeout");
#endif

	if (udl_cmdq_alloc(sc) != 0)
		return;

	cv_init(&sc->sc_cv, device_xname(sc->sc_dev));
	mutex_init(&sc->sc_mtx, MUTEX_DEFAULT, IPL_TTY); /* XXX for tty_lock */

	if ((sc->sc_cmd_cur = udl_cmdq_get(sc)) == NULL)
		return;
	UDL_CMD_BUFINIT(sc);

	/*
	 * Initialize chip.
	 */
	if (udl_init(sc) != 0)
		return;

	udl_read_edid(sc);

	/*
	 * Initialize resolution.
	 */
#ifndef UDL_VIDEOMODE
	if (sc->sc_ei.edid_nmodes != 0 &&
	    sc->sc_ei.edid_preferred_mode != NULL)
		vmp = sc->sc_ei.edid_preferred_mode;
	else
#define UDL_VIDEOMODE	"640x480x60"
#endif
		vmp = udl_videomode_lookup(UDL_VIDEOMODE);

	if (vmp == NULL)
		return;

	sc->sc_width = vmp->hdisplay;
	sc->sc_height = vmp->vdisplay;
	sc->sc_offscreen = sc->sc_height * 3 / 2;
	sc->sc_depth = 16;

	if (udl_set_resolution(sc, vmp) != 0)
		return;

	sc->sc_defaultscreen.name = "default";
	sc->sc_screens[0] = &sc->sc_defaultscreen;
	sc->sc_screenlist.nscreens = 1;
	sc->sc_screenlist.screens = sc->sc_screens;

	/*
	 * Set initial wsdisplay emulation mode.
	 */
	sc->sc_mode = WSDISPLAYIO_MODE_EMUL;

	/*
	 * Attach wsdisplay.
	 */
	aa.console = 0;
	aa.scrdata = &sc->sc_screenlist;
	aa.accessops = &udl_accessops;
	aa.accesscookie = sc;

	sc->sc_wsdisplay =
	    config_found(sc->sc_dev, &aa, wsemuldisplaydevprint);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev, sc->sc_dev);
}

static int
udl_detach(device_t self, int flags)
{
	struct udl_softc *sc = device_private(self);

	/*
	 * Close bulk TX pipe.
	 */
	if (sc->sc_tx_pipeh != NULL) {
		usbd_abort_pipe(sc->sc_tx_pipeh);
		usbd_close_pipe(sc->sc_tx_pipeh);
	}

	/*
	 * Free command xfer buffers.
	 */
	udl_cmdq_flush(sc);
	udl_cmdq_free(sc);

	cv_destroy(&sc->sc_cv);
	mutex_destroy(&sc->sc_mtx);

	/*
	 * Free Huffman table.
	 */
	udl_comp_unload(sc);

	/*
	 * Free framebuffer memory.
	 */
	udl_fbmem_free(sc);

	/*
	 * Detach wsdisplay.
	 */
	if (sc->sc_wsdisplay != NULL)
		config_detach(sc->sc_wsdisplay, DETACH_FORCE);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev, sc->sc_dev);

#ifdef UDL_EVENT_COUNTERS
	evcnt_detach(&sc->sc_ev_cmdq_get);
	evcnt_detach(&sc->sc_ev_cmdq_put);
	evcnt_detach(&sc->sc_ev_cmdq_wait);
	evcnt_detach(&sc->sc_ev_cmdq_timeout);
#endif

	return 0;
}

static int
udl_ioctl(void *v, void *vs, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct udl_softc *sc = v;
#ifdef notyet
	struct udl_ioctl_damage *d;
#endif
	struct wsdisplay_fbinfo *wdf;
	u_int mode;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_DL;
		return 0;

	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->height = sc->sc_height;
		wdf->width = sc->sc_width;
		wdf->depth = sc->sc_depth;
		wdf->cmsize = 0;
		return 0;

	case WSDISPLAYIO_GVIDEO:
		*(u_int *)data = sc->sc_blank;
		return 0;

	case WSDISPLAYIO_SVIDEO:
		mode = *(u_int *)data;
		if (mode == sc->sc_blank)
			return 0;
		switch (mode) {
		case WSDISPLAYIO_VIDEO_OFF:
			udl_blank(sc, 1);
			break;
		case WSDISPLAYIO_VIDEO_ON:
			udl_blank(sc, 0);
			break;
		default:
			return EINVAL;
		}
		udl_cmd_send_async(sc);
		udl_cmdq_flush(sc);
		sc->sc_blank = mode;
		return 0;

	case WSDISPLAYIO_SMODE:
		mode = *(u_int *)data;
		if (mode == sc->sc_mode)
			return 0;
		switch (mode) {
		case WSDISPLAYIO_MODE_EMUL:
			/* clear screen */
			udl_fill_rect(sc, 0, 0, 0, sc->sc_width,
			    sc->sc_height);
			udl_cmd_send_async(sc);
			udl_cmdq_flush(sc);
			udl_comp_unload(sc);
			break;
		case WSDISPLAYIO_MODE_DUMBFB:
			if (UDL_CMD_BUFSIZE(sc) > 0)
				udl_cmd_send_async(sc);
			udl_cmdq_flush(sc);
			udl_comp_load(sc);
			break;
		default:
			return EINVAL;
		}
		sc->sc_mode = mode;
		return 0;

	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = sc->sc_width * (sc->sc_depth / 8);
		return 0;

#ifdef notyet
	/*
	 * XXX
	 * OpenBSD allows device specific ioctl()s and use this
	 * UDLIO_DAMAGE for the damage extension ops of X servers.
	 * Before blindly pulling such interfaces, probably we should
	 * discuss how such devices should be handled which have
	 * in-direct framebuffer memories that should be transfered
	 * per updated rectangle regions via MI wscons APIs.
	 */
	case UDLIO_DAMAGE:
		d = (struct udl_ioctl_damage *)data;
		d->status = UDLIO_STATUS_OK;
		if (sc->sc_flags & UDL_COMPRDY)
			udl_draw_rect_comp(sc, d);
		else
			udl_draw_rect(sc, d);
		return 0;
#endif
	}

	return EPASSTHROUGH;
}

static paddr_t
udl_mmap(void *v, void *vs, off_t off, int prot)
{
	struct udl_softc *sc = v;
	vaddr_t vaddr;
	paddr_t paddr;
	bool rv __diagused;

	if (off < 0 || off > roundup2(UDL_FBMEM_SIZE(sc), PAGE_SIZE))
		return -1;

	/* allocate framebuffer memory */
	if (udl_fbmem_alloc(sc) != 0)
		return -1;

	vaddr = (vaddr_t)sc->sc_fbmem + off;
	rv = pmap_extract(pmap_kernel(), vaddr, &paddr);
	KASSERT(rv);
	paddr += vaddr & PGOFSET;

	/* XXX we need MI paddr_t -> mmap cookie API */
#if defined(__alpha__)
#define PTOMMAP(paddr)	alpha_btop((char *)paddr)
#elif defined(__arm__)
#define PTOMMAP(paddr)	arm_btop((u_long)paddr)
#elif defined(__hppa__)
#define PTOMMAP(paddr)	btop((u_long)paddr)
#elif defined(__i386__) || defined(__x86_64__)
#define PTOMMAP(paddr)	x86_btop(paddr)
#elif defined(__m68k__)
#define PTOMMAP(paddr)	m68k_btop((char *)paddr)
#elif defined(__mips__)
#define PTOMMAP(paddr)	mips_btop(paddr)
#elif defined(__powerpc__)
#define PTOMMAP(paddr)	(paddr)
#elif defined(__sh__)
#define PTOMMAP(paddr)	sh3_btop(paddr)
#elif defined(__sparc__)
#define PTOMMAP(paddr)	(paddr)
#elif defined(__sparc64__)
#define PTOMMAP(paddr)	atop(paddr)
#elif defined(__vax__)
#define PTOMMAP(paddr)	btop((u_int)paddr)
#endif

	return PTOMMAP(paddr);
}

static int
udl_alloc_screen(void *v, const struct wsscreen_descr *type,
    void **cookiep, int *curxp, int *curyp, long *attrp)
{
	struct udl_softc *sc = v;

	if (sc->sc_nscreens > 0)
		return ENOMEM;

	/*
	 * Initialize rasops.
	 */
	sc->sc_ri.ri_depth = sc->sc_depth;
	sc->sc_ri.ri_bits = NULL;
	sc->sc_ri.ri_width = sc->sc_width;
	sc->sc_ri.ri_height = sc->sc_height;
	sc->sc_ri.ri_stride = sc->sc_width * (sc->sc_depth / 8);
	sc->sc_ri.ri_hw = sc;
	sc->sc_ri.ri_flg = 0;

	if (sc->sc_depth == 16) {
		sc->sc_ri.ri_rnum = 5;
		sc->sc_ri.ri_gnum = 6;
		sc->sc_ri.ri_bnum = 5;
		sc->sc_ri.ri_rpos = 11;
		sc->sc_ri.ri_gpos = 5;
		sc->sc_ri.ri_bpos = 0;
	}

	rasops_init(&sc->sc_ri, sc->sc_height / 8, sc->sc_width / 8);

	sc->sc_ri.ri_ops.cursor = udl_cursor;
	sc->sc_ri.ri_ops.putchar = udl_putchar;
	sc->sc_ri.ri_ops.copycols = udl_copycols;
	sc->sc_ri.ri_ops.erasecols = udl_erasecols;
	sc->sc_ri.ri_ops.copyrows = udl_copyrows;
	sc->sc_ri.ri_ops.eraserows = udl_eraserows;

	sc->sc_ri.ri_ops.allocattr(&sc->sc_ri, 0, 0, 0, attrp);

	sc->sc_defaultscreen.ncols = sc->sc_ri.ri_cols;
	sc->sc_defaultscreen.nrows = sc->sc_ri.ri_rows;
	sc->sc_defaultscreen.textops = &sc->sc_ri.ri_ops;
	sc->sc_defaultscreen.fontwidth = sc->sc_ri.ri_font->fontwidth;
	sc->sc_defaultscreen.fontheight = sc->sc_ri.ri_font->fontheight;
	sc->sc_defaultscreen.capabilities = sc->sc_ri.ri_caps;

	*cookiep = &sc->sc_ri;
	*curxp = 0;
	*curyp = 0;

	sc->sc_nscreens++;

	return 0;
}

static void
udl_free_screen(void *v, void *cookie)
{
	struct udl_softc *sc = v;

	sc->sc_nscreens--;
}

static int
udl_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{

	return 0;
}

static inline void
udl_cmd_add_decomptable(struct udl_softc *sc, uint8_t *buf, int len)
{

	udl_cmd_add_2(sc, (UDL_BULK_SOC << 8) | UDL_BULK_CMD_DECOMP);
	udl_cmd_add_4(sc, 0x263871cd);	/* magic number */
	udl_cmd_add_4(sc, 0x00000200);	/* 512 byte chunks */
	memcpy(sc->sc_cmd_buf, buf, len);
	sc->sc_cmd_buf += len;
}

static void
udl_comp_load(struct udl_softc *sc)
{
	struct udl_huffman *h;
	uint8_t *decomp;
	size_t decomp_size;
	int error, i;

	if (!(sc->sc_flags & UDL_DECOMPRDY)) {
		error = firmware_load("udl", "udl-decomp", &decomp,
		    &decomp_size);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "error %d, could not read decomp table %s!\n",
			    error, "udl-decomp");
			return;
		}
		udl_cmd_add_decomptable(sc, decomp, decomp_size);
		firmware_free(decomp, decomp_size);
		if (udl_cmd_send(sc) != 0)
			return;
		sc->sc_flags |= UDL_DECOMPRDY;
	}

	if (!(sc->sc_flags & UDL_COMPRDY)) {
		error = firmware_load("udl", "udl-comp", &sc->sc_huffman,
		    &sc->sc_huffman_size);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "error %d, could not read huffman table %s!\n",
			    error, "udl-comp");
			return;
		}
		h = (struct udl_huffman *)sc->sc_huffman;
		for (i = 0; i < UDL_HUFFMAN_RECORDS; i++)
			h[i].bit_pattern = be32toh(h[i].bit_pattern);
		sc->sc_huffman_base = sc->sc_huffman + UDL_HUFFMAN_BASE;
		sc->sc_flags |= UDL_COMPRDY;
	}
}

static void
udl_comp_unload(struct udl_softc *sc)
{

	if (sc->sc_flags & UDL_COMPRDY) {
		firmware_free(sc->sc_huffman, sc->sc_huffman_size);
		sc->sc_huffman = NULL;
		sc->sc_huffman_size = 0;
		sc->sc_flags &= ~UDL_COMPRDY;
	}
}

static int
udl_fbmem_alloc(struct udl_softc *sc)
{

	if (sc->sc_fbmem == NULL) {
		sc->sc_fbmem = kmem_alloc(UDL_FBMEM_SIZE(sc), KM_SLEEP);
		if (sc->sc_fbmem == NULL)
			return -1;
	}

	return 0;
}

static void
udl_fbmem_free(struct udl_softc *sc)
{

	if (sc->sc_fbmem != NULL) {
		kmem_free(sc->sc_fbmem, UDL_FBMEM_SIZE(sc));
		sc->sc_fbmem = NULL;
	}
}

static int
udl_cmdq_alloc(struct udl_softc *sc)
{
	struct udl_cmdq *cmdq;
	int i;

	TAILQ_INIT(&sc->sc_freecmd);
	TAILQ_INIT(&sc->sc_xfercmd);

	for (i = 0; i < UDL_NCMDQ; i++) {
		cmdq = &sc->sc_cmdq[i];

		cmdq->cq_sc = sc;

		cmdq->cq_xfer = usbd_alloc_xfer(sc->sc_udev);
		if (cmdq->cq_xfer == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "%s: can't allocate xfer handle!\n", __func__);
			goto error;
		}

		cmdq->cq_buf =
		    usbd_alloc_buffer(cmdq->cq_xfer, UDL_CMD_BUFFER_SIZE);
		if (cmdq->cq_buf == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "%s: can't allocate xfer buffer!\n", __func__);
			goto error;
		}

		TAILQ_INSERT_TAIL(&sc->sc_freecmd, cmdq, cq_chain);
	}

	return 0;

 error:
	udl_cmdq_free(sc);
	return -1;
}

static void
udl_cmdq_free(struct udl_softc *sc)
{
	struct udl_cmdq *cmdq;
	int i;

	for (i = 0; i < UDL_NCMDQ; i++) {
		cmdq = &sc->sc_cmdq[i];

		if (cmdq->cq_xfer != NULL) {
			usbd_free_xfer(cmdq->cq_xfer);
			cmdq->cq_xfer = NULL;
			cmdq->cq_buf = NULL;
		}
	}
}

static struct udl_cmdq *
udl_cmdq_get(struct udl_softc *sc)
{
	struct udl_cmdq *cmdq;

	cmdq = TAILQ_FIRST(&sc->sc_freecmd);
	if (cmdq != NULL) {
		TAILQ_REMOVE(&sc->sc_freecmd, cmdq, cq_chain);
		UDL_EVCNT_INCR(&sc->sc_ev_cmdq_get);
	}

	return cmdq;
}

static void
udl_cmdq_put(struct udl_softc *sc, struct udl_cmdq *cmdq)
{

	TAILQ_INSERT_TAIL(&sc->sc_freecmd, cmdq, cq_chain);
	UDL_EVCNT_INCR(&sc->sc_ev_cmdq_put);
}

static void
udl_cmdq_flush(struct udl_softc *sc)
{

	mutex_enter(&sc->sc_mtx);
	while (TAILQ_FIRST(&sc->sc_xfercmd) != NULL)
		cv_wait(&sc->sc_cv, &sc->sc_mtx);
	mutex_exit(&sc->sc_mtx);
}

static void
udl_cursor(void *cookie, int on, int row, int col)
{
	struct rasops_info *ri = cookie;
	struct udl_softc *sc = ri->ri_hw;
	int x, y, width, height;

	if (ri->ri_flg & RI_CURSOR)
		udl_restore_char(ri);

	ri->ri_crow = row;
	ri->ri_ccol = col;

	if (on != 0) {
		ri->ri_flg |= RI_CURSOR;

		x = col * ri->ri_font->fontwidth;
		y = row * ri->ri_font->fontheight;
		width = ri->ri_font->fontwidth;
		height = ri->ri_font->fontheight;

		/* save the last character block to off-screen */
		udl_copy_rect(sc, x, y, 0, sc->sc_offscreen, width, height);

		/* draw cursor */
		udl_fill_rect(sc, 0xffff, x, y, width, 1);
		udl_fill_rect(sc, 0xffff, x, y + 1, 1, height - 2);
		udl_fill_rect(sc, 0xffff, x + width - 1, y + 1, 1, height - 2);
		udl_fill_rect(sc, 0xffff, x, y + height - 1, width, 1);

		udl_cmd_send_async(sc);
	} else
		ri->ri_flg &= ~RI_CURSOR;
}

static void
udl_putchar(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = cookie;
	struct udl_softc *sc = ri->ri_hw;
	uint16_t rgb16[2];
	int fg, bg, underline, x, y, width, height;

	rasops_unpack_attr(attr, &fg, &bg, &underline);
	rgb16[1] = (uint16_t)ri->ri_devcmap[fg];
	rgb16[0] = (uint16_t)ri->ri_devcmap[bg];

	x = col * ri->ri_font->fontwidth;
	y = row * ri->ri_font->fontheight;
	width = ri->ri_font->fontwidth;
	height = ri->ri_font->fontheight;

	if (uc == ' ') {
		/*
		 * Writting a block for the space character instead rendering
		 * it from font bits is more slim.
		 */
		udl_fill_rect(sc, rgb16[0], x, y, width, height);
	} else {
		/* render a character from font bits */
		udl_draw_char(ri, rgb16, uc, x, y);
	}

	if (underline != 0)
		udl_fill_rect(sc, rgb16[1], x, y + height - 1, width, 1);

#if 0
	udl_cmd_send_async(sc);
#endif
}

static void
udl_copycols(void *cookie, int row, int src, int dst, int num)
{
	struct rasops_info *ri = cookie;
	struct udl_softc *sc = ri->ri_hw;
	int sx, dx, y, width, height;

	sx = src * ri->ri_font->fontwidth;
	dx = dst * ri->ri_font->fontwidth;
	y = row * ri->ri_font->fontheight;
	width = num * ri->ri_font->fontwidth;
	height = ri->ri_font->fontheight;

	/* copy row block to off-screen first to fix overlay-copy problem */
	udl_copy_rect(sc, sx, y, 0, sc->sc_offscreen, width, height);

	/* copy row block back from off-screen now */
	udl_copy_rect(sc, 0, sc->sc_offscreen, dx, y, width, height);
#if 0
	udl_cmd_send_async(sc);
#endif
}

static void
udl_erasecols(void *cookie, int row, int col, int num, long attr)
{
	struct rasops_info *ri = cookie;
	struct udl_softc *sc = ri->ri_hw;
	uint16_t rgb16;
	int fg, bg, x, y, width, height;

	rasops_unpack_attr(attr, &fg, &bg, NULL);
	rgb16 = (uint16_t)ri->ri_devcmap[bg];

	x = col * ri->ri_font->fontwidth;
	y = row * ri->ri_font->fontheight;
	width = num * ri->ri_font->fontwidth;
	height = ri->ri_font->fontheight;

	udl_fill_rect(sc, rgb16, x, y, width, height);
#if 0
	udl_cmd_send_async(sc);
#endif
}

static void
udl_copyrows(void *cookie, int src, int dst, int num)
{
	struct rasops_info *ri = cookie;
	struct udl_softc *sc = ri->ri_hw;
	int sy, ey, dy, width, height;

	width = ri->ri_emuwidth;
	height = ri->ri_font->fontheight;

	if (dst < src) {
		sy = src * height;
		ey = (src + num) * height;
		dy = dst * height;

		while (sy < ey) {
			udl_copy_rect(sc, 0, sy, 0, dy, width, height);
			sy += height;
			dy += height;
		}
	} else {
		sy = (src + num) * height;
		ey = src * height;
		dy = (dst + num) * height;

		while (sy > ey) {
			sy -= height;
			dy -= height;
			udl_copy_rect(sc, 0, sy, 0, dy, width, height);
		}
	}
#if 0
	udl_cmd_send_async(sc);
#endif
}

static void
udl_eraserows(void *cookie, int row, int num, long attr)
{
	struct rasops_info *ri = cookie;
	struct udl_softc *sc = ri->ri_hw;
	uint16_t rgb16;
	int fg, bg, y, width, height;

	rasops_unpack_attr(attr, &fg, &bg, NULL);
	rgb16 = (uint16_t)ri->ri_devcmap[bg];

	y = row * ri->ri_font->fontheight;
	width = ri->ri_emuwidth;
	height = num * ri->ri_font->fontheight;

	udl_fill_rect(sc, rgb16, 0, y, width, height);
#if 0
	udl_cmd_send_async(sc);
#endif
}

static void
udl_restore_char(struct rasops_info *ri)
{
	struct udl_softc *sc = ri->ri_hw;
	int x, y, width, height;

	x = ri->ri_ccol * ri->ri_font->fontwidth;
	y = ri->ri_crow * ri->ri_font->fontheight;
	width = ri->ri_font->fontwidth;
	height = ri->ri_font->fontheight;

	/* restore the last saved character from off-screen */
	udl_copy_rect(sc, 0, sc->sc_offscreen, x, y, width, height);
}

static void
udl_draw_char(struct rasops_info *ri, uint16_t *rgb16, u_int uc, int x, int y)
{
	struct udl_softc *sc = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	uint32_t fontbits;
	uint16_t pixels[32];
	uint8_t *fontbase;
	int i, soff, eoff;

	soff = y * sc->sc_width + x;
	eoff = (y + font->fontheight) * sc->sc_width + x;
	fontbase = (uint8_t *)font->data + (uc - font->firstchar) *
	    ri->ri_fontscale;

	while (soff < eoff) {
		fontbits = 0;
		switch (font->stride) {
		case 4:
			fontbits |= fontbase[3];
			/* FALLTHROUGH */
		case 3:
			fontbits |= fontbase[2] << 8;
			/* FALLTHROUGH */
		case 2:
			fontbits |= fontbase[1] << 16;
			/* FALLTHROUGH */
		case 1:
			fontbits |= fontbase[0] << 24;
		}
		fontbase += font->stride;

		for (i = 0; i < font->fontwidth; i++) {
			pixels[i] = rgb16[(fontbits >> 31) & 1];
			fontbits <<= 1;
		}

		udl_draw_line(sc, pixels, soff, font->fontwidth);
		soff += sc->sc_width;
	}
}

static void
udl_copy_rect(struct udl_softc *sc, int sx, int sy, int dx, int dy, int width,
    int height)
{
	int sbase, soff, ebase, eoff, dbase, doff, width_cur;

	sbase = sy * sc->sc_width;
	ebase = (sy + height) * sc->sc_width;
	dbase = dy * sc->sc_width;

	while (width > 0) {
		soff = sbase + sx;
		eoff = ebase + sx;
		doff = dbase + dx;

		if (width >= UDL_CMD_WIDTH_MAX)
			width_cur = UDL_CMD_WIDTH_MAX;
		else
			width_cur = width;

		while (soff < eoff) {
			udl_copy_line(sc, soff, doff, width_cur);
			soff += sc->sc_width;
			doff += sc->sc_width;
		}

		sx += width_cur;
		dx += width_cur;
		width -= width_cur;
	}
}

static void
udl_fill_rect(struct udl_softc *sc, uint16_t rgb16, int x, int y, int width,
    int height)
{
	int sbase, soff, ebase, eoff, width_cur;

	sbase = y * sc->sc_width;
	ebase = (y + height) * sc->sc_width;

	while (width > 0) {
		soff = sbase + x;
		eoff = ebase + x;

		if (width >= UDL_CMD_WIDTH_MAX)
			width_cur = UDL_CMD_WIDTH_MAX;
		else
			width_cur = width;

		while (soff < eoff) {
			udl_fill_line(sc, rgb16, soff, width_cur);
			soff += sc->sc_width;
		}

		x += width_cur;
		width -= width_cur;
	}
}

#ifdef notyet
static void
udl_draw_rect(struct udl_softc *sc, struct udl_ioctl_damage *d)
{
	int sbase, soff, ebase, eoff, x, y, width, width_cur, height;

	x = d->x1;
	y = d->y1;
	width = d->x2 - d->x1;
	height = d->y2 - d->y1;
	sbase = y * sc->sc_width;
	ebase = (y + height) * sc->sc_width;

	while (width > 0) {
		soff = sbase + x;
		eoff = ebase + x;

		if (width >= UDL_CMD_WIDTH_MAX)
			width_cur = UDL_CMD_WIDTH_MAX;
		else
			width_cur = width;

		while (soff < eoff) {
			udl_draw_line(sc, (uint16_t *)sc->sc_fbmem + soff,
			    soff, width_cur);
			soff += sc->sc_width;
		}

		x += width_cur;
		width -= width_cur;
	}

	udl_cmd_send_async(sc);
}

static void
udl_draw_rect_comp(struct udl_softc *sc, struct udl_ioctl_damage *d)
{
	int soff, eoff, x, y, width, height;

	x = d->x1;
	y = d->y1;
	width = d->x2 - d->x1;
	height = d->y2 - d->y1;
	soff = y * sc->sc_width + x;
	eoff = (y + height) * sc->sc_width + x;

	udl_reg_write_1(sc, UDL_REG_SYNC, 0xff);
	sc->sc_cmd_cblen = 4;

	while (soff < eoff) {
		udl_draw_line_comp(sc, (uint16_t *)sc->sc_fbmem + soff, soff,
		    width);
		soff += sc->sc_width;
	}

	udl_cmd_send_async(sc);
}
#endif

static inline void
udl_copy_line(struct udl_softc *sc, int soff, int doff, int width)
{

	if (__predict_false((UDL_CMD_BUFSIZE(sc) + UDL_CMD_COPY_SIZE + 2) >
	    UDL_CMD_BUFFER_SIZE))
		udl_cmd_send_async(sc);

	udl_cmd_add_2(sc, (UDL_BULK_SOC << 8) | UDL_BULK_CMD_FB_COPY16);
	udl_cmd_add_4(sc, ((doff * 2) << 8) | (width & 0xff));

	udl_cmd_add_3(sc, soff * 2);
}

static inline void
udl_fill_line(struct udl_softc *sc, uint16_t rgb16, int off, int width)
{

	if (__predict_false((UDL_CMD_BUFSIZE(sc) + UDL_CMD_FILL_SIZE + 2) >
	    UDL_CMD_BUFFER_SIZE))
		udl_cmd_send_async(sc);

	udl_cmd_add_2(sc, (UDL_BULK_SOC << 8) | UDL_BULK_CMD_FB_RLE16);
	udl_cmd_add_4(sc, ((off * 2) << 8) | (width & 0xff));

	udl_cmd_add_1(sc, width);
	udl_cmd_add_2(sc, rgb16);
}

static inline void
udl_draw_line(struct udl_softc *sc, uint16_t *buf, int off, int width)
{

	if (__predict_false(
	    (UDL_CMD_BUFSIZE(sc) + UDL_CMD_DRAW_SIZE(width) + 2) >
	    UDL_CMD_BUFFER_SIZE))
		udl_cmd_send_async(sc);

	udl_cmd_add_2(sc, (UDL_BULK_SOC << 8) | UDL_BULK_CMD_FB_WRITE16);
	udl_cmd_add_4(sc, ((off * 2) << 8) | (width & 0xff));

	udl_cmd_add_buf(sc, buf, width);
}

#ifdef notyet
static inline int
udl_cmd_add_buf_comp(struct udl_softc *sc, uint16_t *buf, int width)
{
	struct udl_huffman *h;
	uint16_t *startp, *endp;
	uint32_t bit_pattern;
	uint16_t prev;
	int16_t diff;
	uint8_t bit_count, bit_pos, bit_rem, curlen;

	startp = buf;
	if (width >= UDL_CMD_WIDTH_MAX)
		endp = buf + UDL_CMD_WIDTH_MAX;
	else
		endp = buf + width;

	prev = bit_pos = *sc->sc_cmd_buf = 0;
	bit_rem = 8;

	/*
	 * Generate a sub-block with maximal 256 pixels compressed data.
	 */
	while (buf < endp) {
		/* get difference between current and previous pixel */
		diff = *buf - prev;

		/* get the huffman difference bit sequence */
		h = (struct udl_huffman *)sc->sc_huffman_base + diff;
		bit_count = h->bit_count;
		bit_pattern = h->bit_pattern;

		curlen = (bit_pos + bit_count + 7) / 8;
		if (__predict_false((sc->sc_cmd_cblen + curlen + 1) >
		    UDL_CMD_COMP_BLOCK_SIZE))
			break;

		/* generate one pixel compressed data */
		while (bit_count >= bit_rem) {
			*sc->sc_cmd_buf++ |=
			    (bit_pattern & ((1 << bit_rem) - 1)) << bit_pos;
			*sc->sc_cmd_buf = 0;
			sc->sc_cmd_cblen++;
			bit_count -= bit_rem;
			bit_pattern >>= bit_rem;
			bit_pos = 0;
			bit_rem = 8;
		}

		if (bit_count > 0) {
			*sc->sc_cmd_buf |=
			    (bit_pattern & ((1 << bit_count) - 1)) << bit_pos;
			bit_pos += bit_count;
			bit_rem -= bit_count;
		}

		prev = *buf++;
	}

	/*
 	 * If we have bits left in our last byte, round up to the next
 	 * byte, so we don't overwrite them.
 	 */
	if (bit_pos > 0) {
		sc->sc_cmd_buf++;
		sc->sc_cmd_cblen++;
	}

	/* return how many pixels we have compressed */
	return buf - startp;
}

static inline void
udl_draw_line_comp(struct udl_softc *sc, uint16_t *buf, int off, int width)
{
	uint8_t *widthp;
	int width_cur;

	while (width > 0) {
		if (__predict_false(
		    (sc->sc_cmd_cblen + UDL_CMD_COMP_MIN_SIZE + 1) >
		    UDL_CMD_COMP_BLOCK_SIZE)) {
			if (UDL_CMD_BUFSIZE(sc) < UDL_CMD_COMP_THRESHOLD) {
				while (sc->sc_cmd_cblen <
				    UDL_CMD_COMP_BLOCK_SIZE) {
					*sc->sc_cmd_buf++ = 0;
					sc->sc_cmd_cblen++;
				}
			} else
				udl_cmd_send_async(sc);
			udl_reg_write_1(sc, UDL_REG_SYNC, 0xff);
			sc->sc_cmd_cblen = 4;
		}

		udl_cmd_add_2(sc, (UDL_BULK_SOC << 8) |
		    (UDL_BULK_CMD_FB_WRITE16 | UDL_BULK_CMD_FB_COMP));
		udl_cmd_add_4(sc, (off * 2) << 8);

		widthp = sc->sc_cmd_buf - 1;

		sc->sc_cmd_cblen += UDL_CMD_HEADER_SIZE;

		width_cur = udl_cmd_add_buf_comp(sc, buf, width);

		*widthp = width_cur;
		buf += width_cur;
		off += width_cur;
		width -= width_cur;
	}
}
#endif

static int
udl_cmd_send(struct udl_softc *sc)
{
	struct udl_cmdq *cmdq;
	usbd_status error;
	uint32_t len;

	cmdq = sc->sc_cmd_cur;

	/* mark end of command stack */
	udl_cmd_add_2(sc, (UDL_BULK_SOC << 8) | UDL_BULK_CMD_EOC);

	len = UDL_CMD_BUFSIZE(sc);

	/* do xfer */
	error = usbd_bulk_transfer(cmdq->cq_xfer, sc->sc_tx_pipeh,
	    USBD_NO_COPY, USBD_NO_TIMEOUT, cmdq->cq_buf, &len, "udlcmds");

	UDL_CMD_BUFINIT(sc);

	if (error != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(sc->sc_dev, "%s: %s!\n", __func__,
		    usbd_errstr(error));
		return -1;
	}

	return 0;
}

static void
udl_cmd_send_async(struct udl_softc *sc)
{
	struct udl_cmdq *cmdq;
	usbd_status error;
	uint32_t len;

#if 1
	/*
	 * XXX
	 * All tty ops for wsemul are called with tty_lock spin mutex held,
	 * so we can't call cv_wait(9) here to acquire a free buffer.
	 * For now, all commands and data for wsemul ops are discarded
	 * if there is no free command buffer, and then screen text might
	 * be corrupted on large scroll ops etc.
	 *
	 * Probably we have to reorganize the giant tty_lock mutex, or
	 * change wsdisplay APIs (especially wsdisplaystart()) to return
	 * a number of actually handled characters as OpenBSD does, but
	 * the latter one requires whole API changes around rasops(9) etc.
	 */
	if (sc->sc_mode == WSDISPLAYIO_MODE_EMUL) {
		if (TAILQ_FIRST(&sc->sc_freecmd) == NULL) {
			UDL_CMD_BUFINIT(sc);
			return;
		}
	}
#endif

	cmdq = sc->sc_cmd_cur;

	/* mark end of command stack */
	udl_cmd_add_2(sc, (UDL_BULK_SOC << 8) | UDL_BULK_CMD_EOC);

	len = UDL_CMD_BUFSIZE(sc);

	/* do xfer */
	mutex_enter(&sc->sc_mtx);
	usbd_setup_xfer(cmdq->cq_xfer, sc->sc_tx_pipeh, cmdq, cmdq->cq_buf,
	    len, USBD_NO_COPY, USBD_NO_TIMEOUT, udl_cmd_send_async_cb);
	error = usbd_transfer(cmdq->cq_xfer);
	if (error != USBD_NORMAL_COMPLETION && error != USBD_IN_PROGRESS) {
		aprint_error_dev(sc->sc_dev, "%s: %s!\n", __func__,
		    usbd_errstr(error));
		mutex_exit(&sc->sc_mtx);
		goto end;
	}

	TAILQ_INSERT_TAIL(&sc->sc_xfercmd, cmdq, cq_chain);
	cmdq = udl_cmdq_get(sc);
	mutex_exit(&sc->sc_mtx);
	while (cmdq == NULL) {
		int err;
		UDL_EVCNT_INCR(&sc->sc_ev_cmdq_wait);
		mutex_enter(&sc->sc_mtx);
		err = cv_timedwait(&sc->sc_cv, &sc->sc_mtx,
		    mstohz(100) /* XXX is this needed? */);
		if (err != 0) {
			DPRINTF(("%s: %s: cv timeout (error = %d)\n",
			    device_xname(sc->sc_dev), __func__, err));
			UDL_EVCNT_INCR(&sc->sc_ev_cmdq_timeout);
		}
		cmdq = udl_cmdq_get(sc);
		mutex_exit(&sc->sc_mtx);
	}
	sc->sc_cmd_cur = cmdq;
 end:
	UDL_CMD_BUFINIT(sc);
}

static void
udl_cmd_send_async_cb(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct udl_cmdq *cmdq = priv;
	struct udl_softc *sc = cmdq->cq_sc;

	if (status != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(sc->sc_dev, "%s: %s!\n", __func__,
		    usbd_errstr(status));

		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_tx_pipeh);
	}

	mutex_enter(&sc->sc_mtx);
	TAILQ_REMOVE(&sc->sc_xfercmd, cmdq, cq_chain);
	udl_cmdq_put(sc, cmdq);

	/* signal xfer op that sleeps for a free xfer buffer */
	cv_signal(&sc->sc_cv);
	mutex_exit(&sc->sc_mtx);
}

static int
udl_ctrl_msg(struct udl_softc *sc, uint8_t rt, uint8_t r, uint16_t index,
    uint16_t value, uint8_t *buf, uint16_t len)
{
	usb_device_request_t req;
	usbd_status error;

	req.bmRequestType = rt;
	req.bRequest = r;
	USETW(req.wIndex, index);
	USETW(req.wValue, value);
	USETW(req.wLength, len);

	error = usbd_do_request(sc->sc_udev, &req, buf);
	if (error != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(sc->sc_dev, "%s: %s!\n", __func__,
		    usbd_errstr(error));
		return -1;
	}

	return 0;
}

static int
udl_init(struct udl_softc *sc)
{
	static uint8_t key[16] = {
	    0x57, 0xcd, 0xdc, 0xa7, 0x1c, 0x88, 0x5e, 0x15,
	    0x60, 0xfe, 0xc6, 0x97, 0x16, 0x3d, 0x47, 0xf2
	};
	uint8_t status[4], val;

	if (udl_ctrl_msg(sc, UT_READ_VENDOR_DEVICE, UDL_CTRL_CMD_READ_STATUS,
	    0x0000, 0x0000, status, sizeof(status)) != 0)
		return -1;

	if (udl_ctrl_msg(sc, UT_READ_VENDOR_DEVICE, UDL_CTRL_CMD_READ_1,
	    0xc484, 0x0000, &val, 1) != 0)
		return -1;

	val = 1;
	if (udl_ctrl_msg(sc, UT_WRITE_VENDOR_DEVICE, UDL_CTRL_CMD_WRITE_1,
	    0xc41f, 0x0000, &val, 1) != 0)
		return -1;

	if (udl_ctrl_msg(sc, UT_WRITE_VENDOR_DEVICE, UDL_CTRL_CMD_SET_KEY,
	    0x0000, 0x0000, key, sizeof(key)) != 0)
		return -1;

	val = 0;
	if (udl_ctrl_msg(sc, UT_WRITE_VENDOR_DEVICE, UDL_CTRL_CMD_WRITE_1,
	    0xc40b, 0x0000, &val, 1) != 0)
		return -1;

	return 0;
}

static void
udl_read_edid(struct udl_softc *sc)
{
	uint8_t buf[64], edid[128];
	int offset;

	memset(&sc->sc_ei, 0, sizeof(struct edid_info));

	offset = 0;
	if (udl_ctrl_msg(sc, UT_READ_VENDOR_DEVICE, UDL_CTRL_CMD_READ_EDID,
	    0x00a1, (offset << 8), buf, 64) != 0)
		return;
	if (buf[0] != 0)
		return;
	memcpy(&edid[offset], &buf[1], 63);
	offset += 63;

	if (udl_ctrl_msg(sc, UT_READ_VENDOR_DEVICE, UDL_CTRL_CMD_READ_EDID,
	    0x00a1, (offset << 8), buf, 64) != 0)
		return;
	if (buf[0] != 0)
		return;
	memcpy(&edid[offset], &buf[1], 63);
	offset += 63;

	if (udl_ctrl_msg(sc, UT_READ_VENDOR_DEVICE, UDL_CTRL_CMD_READ_EDID,
	    0x00a1, (offset << 8), buf, 3) != 0)
		return;
	if (buf[0] != 0)
		return;
	memcpy(&edid[offset], &buf[1], 2);

	if (edid_parse(edid, &sc->sc_ei) == 0) {
#ifdef UDL_DEBUG
		edid_print(&sc->sc_ei);
#endif
	}
}

static void
udl_set_address(struct udl_softc *sc, int start16, int stride16, int start8,
    int stride8)
{
	udl_reg_write_1(sc, UDL_REG_SYNC, 0x00);
	udl_reg_write_3(sc, UDL_REG_ADDR_START16, start16);
	udl_reg_write_3(sc, UDL_REG_ADDR_STRIDE16, stride16);
	udl_reg_write_3(sc, UDL_REG_ADDR_START8, start8);
	udl_reg_write_3(sc, UDL_REG_ADDR_STRIDE8, stride8);
	udl_reg_write_1(sc, UDL_REG_SYNC, 0xff);
}

static void
udl_blank(struct udl_softc *sc, int blank)
{

	if (blank != 0)
		udl_reg_write_1(sc, UDL_REG_BLANK, UDL_REG_BLANK_ON);
	else
		udl_reg_write_1(sc, UDL_REG_BLANK, UDL_REG_BLANK_OFF);
	udl_reg_write_1(sc, UDL_REG_SYNC, 0xff);
}

static uint16_t
udl_lfsr(uint16_t count)
{
	uint16_t val = 0xffff;

	while (count > 0) {
		val = (uint16_t)(val << 1) | ((uint16_t)(
		    (uint16_t)(val << 0) ^
		    (uint16_t)(val << 11) ^
		    (uint16_t)(val << 13) ^
		    (uint16_t)(val << 14)
		    ) >> 15);
		count--;
	}

	return val;
}

static int
udl_set_resolution(struct udl_softc *sc, const struct videomode *vmp)
{
	uint16_t val;
	int start16, stride16, start8, stride8;

	/* set video memory offsets */
	start16 = 0;
	stride16 = sc->sc_width * 2;
	start8 = stride16 * sc->sc_height;
	stride8 = sc->sc_width;
	udl_set_address(sc, start16, stride16, start8, stride8);

	/* write resolution values */
	udl_reg_write_1(sc, UDL_REG_SYNC, 0x00);
	udl_reg_write_1(sc, UDL_REG_COLORDEPTH, UDL_REG_COLORDEPTH_16);
	val = vmp->htotal - vmp->hsync_start;
	udl_reg_write_2(sc, UDL_REG_XDISPLAYSTART, udl_lfsr(val));
	val += vmp->hdisplay;
	udl_reg_write_2(sc, UDL_REG_XDISPLAYEND, udl_lfsr(val));
	val = vmp->vtotal - vmp->vsync_start;
	udl_reg_write_2(sc, UDL_REG_YDISPLAYSTART, udl_lfsr(val));
	val += vmp->vdisplay;
	udl_reg_write_2(sc, UDL_REG_YDISPLAYEND, udl_lfsr(val));
	val = vmp->htotal - 1;
	udl_reg_write_2(sc, UDL_REG_XENDCOUNT, udl_lfsr(val));
	val = vmp->hsync_end - vmp->hsync_start + 1;
	if (vmp->flags & VID_PHSYNC) {
		udl_reg_write_2(sc, UDL_REG_HSYNCSTART, udl_lfsr(1));
		udl_reg_write_2(sc, UDL_REG_HSYNCEND, udl_lfsr(val));
	} else {
		udl_reg_write_2(sc, UDL_REG_HSYNCSTART, udl_lfsr(val));
		udl_reg_write_2(sc, UDL_REG_HSYNCEND, udl_lfsr(1));
	}
	val = vmp->hdisplay;
	udl_reg_write_2(sc, UDL_REG_HPIXELS, val);
	val = vmp->vtotal;
	udl_reg_write_2(sc, UDL_REG_YENDCOUNT, udl_lfsr(val));
	val = vmp->vsync_end - vmp->vsync_start;
	if (vmp->flags & VID_PVSYNC) {
		udl_reg_write_2(sc, UDL_REG_VSYNCSTART, udl_lfsr(0));
		udl_reg_write_2(sc, UDL_REG_VSYNCEND, udl_lfsr(val));
	} else {
		udl_reg_write_2(sc, UDL_REG_VSYNCSTART, udl_lfsr(val));
		udl_reg_write_2(sc, UDL_REG_VSYNCEND, udl_lfsr(0));
	}
	val = vmp->vdisplay;
	udl_reg_write_2(sc, UDL_REG_VPIXELS, val);
	val = vmp->dot_clock / 5;
	udl_reg_write_2(sc, UDL_REG_PIXELCLOCK5KHZ, bswap16(val));
	udl_reg_write_1(sc, UDL_REG_SYNC, 0xff);

	if (udl_cmd_send(sc) != 0)
		return -1;

	/* clear screen */
	udl_fill_rect(sc, 0, 0, 0, sc->sc_width, sc->sc_height);

	if (udl_cmd_send(sc) != 0)
		return -1;

	/* show framebuffer content */
	udl_blank(sc, 0);

	if (udl_cmd_send(sc) != 0)
		return -1;

	sc->sc_blank = WSDISPLAYIO_VIDEO_ON;

	return 0;
}

static const struct videomode *
udl_videomode_lookup(const char *name)
{
	int i;

	for (i = 0; i < videomode_count; i++)
		if (strcmp(name, videomode_list[i].name) == 0)
			return &videomode_list[i];

	return NULL;
}

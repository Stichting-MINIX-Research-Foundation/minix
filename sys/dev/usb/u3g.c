/*	$NetBSD: u3g.c,v 1.31 2014/09/24 00:17:13 christos Exp $	*/

/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation.
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
 * Copyright (c) 2008 AnyWi Technologies
 *   Author: Andrea Guzzo <aguzzo@anywi.com>
 *   * based on uark.c 1.1 2006/08/14 08:30:22 jsg *
 *   * parts from ubsa.c 183348 2008-09-25 12:00:56Z phk *
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
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: u3g.c,v 1.31 2014/09/24 00:17:13 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/tty.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdi_util.h>

#include <dev/usb/ucomvar.h>

#include "usbdevs.h"

/*
 * We read/write data from/to the device in 4KB chunks to maximise
 * performance.
 */
#define U3G_BUFF_SIZE	4096

/*
 * Some 3G devices (the Huawei E160/E220 springs to mind here) buffer up
 * data internally even when the USB pipes are closed. So on first open,
 * we can receive a large chunk of stale data.
 *
 * This causes a real problem because the default TTYDEF_LFLAG (applied
 * on first open) has the ECHO flag set, resulting in all the stale data
 * being echoed straight back to the device by the tty(4) layer. Some
 * devices (again, the Huawei E160/E220 for example) react to this spew
 * by going catatonic.
 *
 * All this happens before the application gets a chance to disable ECHO.
 *
 * We work around this by ignoring all data received from the device for
 * a period of two seconds, or until the application starts sending data -
 * whichever comes first.
 */
#define	U3G_PURGE_SECS	2

/*
 * Define bits for the virtual modem control pins.
 * The input pin states are reported via the interrupt pipe on some devices.
 */
#define	U3G_OUTPIN_DTR	(1u << 0)
#define	U3G_OUTPIN_RTS	(1u << 1)
#define	U3G_INPIN_DCD	(1u << 0)
#define	U3G_INPIN_DSR	(1u << 1)
#define	U3G_INPIN_RI	(1u << 3)

/*
 * USB request to set the output pin status
 */
#define	U3G_SET_PIN	0x22

struct u3g_softc {
	device_t		sc_dev;
	usbd_device_handle	sc_udev;
	bool			sc_dying;	/* We're going away */
	int			sc_ifaceno;	/* Device interface number */

	struct u3g_com {
		device_t	c_dev;		/* Child ucom(4) handle */

		bool		c_open;		/* Device is in use */
		bool		c_purging;	/* Purging stale data */
		struct timeval	c_purge_start;	/* Control duration of purge */

		u_char		c_msr;		/* Emulated 'msr' */
		uint16_t	c_outpins;	/* Output pin state */
	} sc_com[10];
	size_t			sc_ncom;

	usbd_pipe_handle	sc_intr_pipe;	/* Interrupt pipe */
	u_char			*sc_intr_buff;	/* Interrupt buffer */
};

/*
 * The device driver has two personalities. The first uses the 'usbdevif'
 * interface attribute so that a match will claim the entire USB device
 * for itself. This is used for when a device needs to be mode-switched
 * and ensures any other interfaces present cannot be claimed by other
 * drivers while the mode-switch is in progress.
 *
 * The second personality uses the 'usbifif' interface attribute so that
 * it can claim the 3G modem interfaces for itself, leaving others (such
 * as the mass storage interfaces on some devices) for other drivers.
 */
static int u3ginit_match(device_t, cfdata_t, void *);
static void u3ginit_attach(device_t, device_t, void *);
static int u3ginit_detach(device_t, int);

CFATTACH_DECL2_NEW(u3ginit, 0, u3ginit_match,
    u3ginit_attach, u3ginit_detach, NULL, NULL, NULL);


static int u3g_match(device_t, cfdata_t, void *);
static void u3g_attach(device_t, device_t, void *);
static int u3g_detach(device_t, int);
static int u3g_activate(device_t, enum devact);
static void u3g_childdet(device_t, device_t);

CFATTACH_DECL2_NEW(u3g, sizeof(struct u3g_softc), u3g_match,
    u3g_attach, u3g_detach, u3g_activate, NULL, u3g_childdet);


static void u3g_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);
static void u3g_get_status(void *, int, u_char *, u_char *);
static void u3g_set(void *, int, int, int);
static int  u3g_open(void *, int);
static void u3g_close(void *, int);
static void u3g_read(void *, int, u_char **, uint32_t *);
static void u3g_write(void *, int, u_char *, u_char *, u_int32_t *);

struct ucom_methods u3g_methods = {
	u3g_get_status,
	u3g_set,
	NULL,
	NULL,
	u3g_open,
	u3g_close,
	u3g_read,
	u3g_write,
};

/*
 * Allegedly supported devices
 */
static const struct usb_devno u3g_devs[] = {
        { USB_VENDOR_DELL, USB_PRODUCT_DELL_W5500 },
	/* OEM: Huawei */
	{ USB_VENDOR_HUAWEI, USB_PRODUCT_HUAWEI_E1750 },
	{ USB_VENDOR_HUAWEI, USB_PRODUCT_HUAWEI_E1820 },
	{ USB_VENDOR_HUAWEI, USB_PRODUCT_HUAWEI_E220 },
	{ USB_VENDOR_HUAWEI, USB_PRODUCT_HUAWEI_EM770W },
	{ USB_VENDOR_HUAWEI, USB_PRODUCT_HUAWEI_K3765 },
	{ USB_VENDOR_HUAWEI, USB_PRODUCT_HUAWEI_MOBILE },
	{ USB_VENDOR_HUAWEI, USB_PRODUCT_HUAWEI_E171 },
	{ USB_VENDOR_HUAWEI, USB_PRODUCT_HUAWEI_E353 },
	/* OEM: Merlin */
	{ USB_VENDOR_MERLIN, USB_PRODUCT_MERLIN_V620 },
	/* OEM: Novatel */
	{ USB_VENDOR_NOVATEL2, USB_PRODUCT_NOVATEL2_ES620 },
	{ USB_VENDOR_NOVATEL2, USB_PRODUCT_NOVATEL2_EU8X0D },
	{ USB_VENDOR_NOVATEL2, USB_PRODUCT_NOVATEL2_MC950D },
#if 0
	/* These are matched in u3ginit_match() */
	{ USB_VENDOR_NOVATEL2, USB_PRODUCT_NOVATEL2_MC950D_DRIVER },
	{ USB_VENDOR_NOVATEL2, USB_PRODUCT_NOVATEL2_U760_DRIVER },
#endif
	{ USB_VENDOR_NOVATEL2, USB_PRODUCT_NOVATEL2_MERLINU740 },
	{ USB_VENDOR_NOVATEL2, USB_PRODUCT_NOVATEL2_MERLINV620 },
	{ USB_VENDOR_NOVATEL2, USB_PRODUCT_NOVATEL2_S720 },
	{ USB_VENDOR_NOVATEL2, USB_PRODUCT_NOVATEL2_U720 },
	{ USB_VENDOR_NOVATEL2, USB_PRODUCT_NOVATEL2_U727 },
	{ USB_VENDOR_NOVATEL2, USB_PRODUCT_NOVATEL2_U740_2 },
	{ USB_VENDOR_NOVATEL2, USB_PRODUCT_NOVATEL2_U760 },
	{ USB_VENDOR_NOVATEL2, USB_PRODUCT_NOVATEL2_U870 },
	{ USB_VENDOR_NOVATEL2, USB_PRODUCT_NOVATEL2_V740 },
	{ USB_VENDOR_NOVATEL2, USB_PRODUCT_NOVATEL2_X950D },
	{ USB_VENDOR_NOVATEL2, USB_PRODUCT_NOVATEL2_XU870 },
	/* OEM: Option N.V. */
	{ USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_QUADPLUSUMTS },
	{ USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_HSDPA },
	{ USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_GTMAXHSUPA },
	/* OEM: Qualcomm, Inc. */
	{ USB_VENDOR_QUALCOMM, USB_PRODUCT_QUALCOMM_NTT_DOCOMO_L02C_MODEM },

	/* OEM: Sierra Wireless: */
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC595U },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC597E },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC875U },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC880 },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC880E },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC880U },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC881 },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC881E },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC881U },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AIRCARD580 },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AIRCARD595 },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AIRCARD875 },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_C597 },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_EM5625 },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC5720 },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC5720_2 },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC5725 },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8755 },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8755_2 },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8755_3 },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8765 },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8775_2 },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8780 },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8781 },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MINI5725 },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_USB305 },
	{ USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_250U },
	/* Toshiba */
	{ USB_VENDOR_TOSHIBA, USB_PRODUCT_TOSHIBA_HSDPA_MODEM_EU870DT1 },

	/* ZTE */
	{ USB_VENDOR_ZTE, USB_PRODUCT_ZTE_MF622 },
	{ USB_VENDOR_ZTE, USB_PRODUCT_ZTE_MF626 },
	{ USB_VENDOR_ZTE, USB_PRODUCT_ZTE_MF628 },
	{ USB_VENDOR_ZTE, USB_PRODUCT_ZTE_MF820D },

	/* 4G Systems */
	{ USB_VENDOR_4GSYSTEMS, USB_PRODUCT_4GSYSTEMS_XSSTICK_P14 },
	{ USB_VENDOR_4GSYSTEMS, USB_PRODUCT_4GSYSTEMS_XSSTICK_W14 },
};

static int
send_bulkmsg(usbd_device_handle dev, void *cmd, size_t cmdlen)
{
	usbd_interface_handle iface;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	usbd_pipe_handle pipe;
	usbd_xfer_handle xfer;
	int err, i;

	/* Move the device into the configured state. */
	err = usbd_set_config_index(dev, 0, 0);
	if (err) {
		aprint_error("u3ginit: failed to set config index\n");
		return UMATCH_NONE;
	}

	err = usbd_device2interface_handle(dev, 0, &iface);
	if (err != 0) {
		aprint_error("u3ginit: failed to get interface\n");
		return UMATCH_NONE;
	}

	id = usbd_get_interface_descriptor(iface);
	ed = NULL;
	for (i = 0 ; i < id->bNumEndpoints ; i++) {
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL)
			continue;
		if (UE_GET_DIR(ed->bEndpointAddress) != UE_DIR_OUT)
			continue;
		if ((ed->bmAttributes & UE_XFERTYPE) == UE_BULK)
			break;
	}

	if (i == id->bNumEndpoints)
		return UMATCH_NONE;

	err = usbd_open_pipe(iface, ed->bEndpointAddress,
	    USBD_EXCLUSIVE_USE, &pipe);
	if (err != 0) {
		aprint_error("u3ginit: failed to open bulk transfer pipe %d\n",
		    ed->bEndpointAddress);
		return UMATCH_NONE;
	}

	xfer = usbd_alloc_xfer(dev);
	if (xfer != NULL) {
		usbd_setup_xfer(xfer, pipe, NULL, cmd, cmdlen,
		    USBD_SYNCHRONOUS, USBD_DEFAULT_TIMEOUT, NULL);

		err = usbd_transfer(xfer);

#if 0 /* XXXpooka: at least my huawei "fails" this always, but still detaches */
		if (err)
			aprint_error("u3ginit: transfer failed\n");
#else
		err = 0;
#endif
		usbd_free_xfer(xfer);
	} else {
		aprint_error("u3ginit: failed to allocate xfer\n");
		err = USBD_NOMEM;
	}

	usbd_abort_pipe(pipe);
	usbd_close_pipe(pipe);

	return (err == USBD_NORMAL_COMPLETION ? UMATCH_HIGHEST : UMATCH_NONE);
}

/* Byte 0..3: Command Block Wrapper (CBW) signature */
static void
set_cbw(unsigned char *cmd)
{
	cmd[0] = 0x55; 
	cmd[1] = 0x53;
	cmd[2] = 0x42;
	cmd[3] = 0x43;
}

static int
u3g_bulk_scsi_eject(usbd_device_handle dev)
{
	unsigned char cmd[31];

	memset(cmd, 0, sizeof(cmd));
	/* Byte 0..3: Command Block Wrapper (CBW) signature */
	set_cbw(cmd);
	/* 4..7: CBW Tag, has to unique, but only a single transfer used. */
	cmd[4] = 0x01;
	/* 8..11: CBW Transfer Length, no data here */
	/* 12: CBW Flag: output, so 0 */
	/* 13: CBW Lun: 0 */
	/* 14: CBW Length */
	cmd[14] = 0x06;

	/* Rest is the SCSI payload */

	/* 0: SCSI START/STOP opcode */
	cmd[15] = 0x1b;
	/* 1..3 unused */
	/* 4 Load/Eject command */
	cmd[19] = 0x02;
	/* 5: unused */

	return send_bulkmsg(dev, cmd, sizeof(cmd));
}

static int
u3g_bulk_ata_eject(usbd_device_handle dev)
{
	unsigned char cmd[31];

	memset(cmd, 0, sizeof(cmd));
	/* Byte 0..3: Command Block Wrapper (CBW) signature */
	set_cbw(cmd);
	/* 4..7: CBW Tag, has to unique, but only a single transfer used. */
	cmd[4] = 0x01;
	/* 8..11: CBW Transfer Length, no data here */
	/* 12: CBW Flag: output, so 0 */
	/* 13: CBW Lun: 0 */
	/* 14: CBW Length */
	cmd[14] = 0x06;

	/* Rest is the SCSI payload */

	/* 0: ATA pass-through */
	cmd[15] = 0x85;
	/* 1..3 unused */
	/* 4 XXX What is this command? */
	cmd[19] = 0x24;
	/* 5: unused */

	return send_bulkmsg(dev, cmd, sizeof(cmd));
}

static int
u3g_huawei_reinit(usbd_device_handle dev)
{
	/*
	 * The Huawei device presents itself as a umass device with Windows
	 * drivers on it. After installation of the driver, it reinits into a
	 * 3G serial device.
	 */
	usb_device_request_t req;
	usb_config_descriptor_t *cdesc;

	/* Get the config descriptor */
	cdesc = usbd_get_config_descriptor(dev);
	if (cdesc == NULL) {
		usb_device_descriptor_t dd;

		if (usbd_get_device_desc(dev, &dd) != 0)
			return (UMATCH_NONE);

		if (dd.bNumConfigurations != 1)
			return (UMATCH_NONE);

		if (usbd_set_config_index(dev, 0, 1) != 0)
			return (UMATCH_NONE);

		cdesc = usbd_get_config_descriptor(dev);

		if (cdesc == NULL)
			return (UMATCH_NONE);
	}

	/*
	 * One iface means umass mode, more than 1 (4 usually) means 3G mode.
	 *
	 * XXX: We should check the first interface's device class just to be
	 * sure. If it's a mass storage device, then we can be fairly certain
	 * it needs a mode-switch.
	 */
	if (cdesc->bNumInterface > 1)
		return (UMATCH_NONE);

	req.bmRequestType = UT_WRITE_DEVICE;
	req.bRequest = UR_SET_FEATURE;
	USETW(req.wValue, UF_DEVICE_REMOTE_WAKEUP);
	USETW(req.wIndex, UHF_PORT_SUSPEND);
	USETW(req.wLength, 0);

	(void) usbd_do_request(dev, &req, 0);

	return (UMATCH_HIGHEST); /* Prevent umass from attaching */
}

static int
u3g_huawei_k3765_reinit(usbd_device_handle dev)
{
	unsigned char cmd[31];

	/* magic string adapted from some webpage */
	memset(cmd, 0, sizeof(cmd));
	/* Byte 0..3: Command Block Wrapper (CBW) signature */
	set_cbw(cmd);

	cmd[15]= 0x11;
	cmd[16]= 0x06;

	return send_bulkmsg(dev, cmd, sizeof(cmd));
}
static int
u3g_huawei_e171_reinit(usbd_device_handle dev)
{
	unsigned char cmd[31];

	/* magic string adapted from some webpage */
	memset(cmd, 0, sizeof(cmd));
	/* Byte 0..3: Command Block Wrapper (CBW) signature */
	set_cbw(cmd);

	cmd[15]= 0x11;
	cmd[16]= 0x06;
	cmd[17]= 0x20;
	cmd[20]= 0x01;

	return send_bulkmsg(dev, cmd, sizeof(cmd));
}

static int
u3g_huawei_e353_reinit(usbd_device_handle dev)
{
	unsigned char cmd[31];

	/* magic string adapted from some webpage */
	memset(cmd, 0, sizeof(cmd));
	/* Byte 0..3: Command Block Wrapper (CBW) signature */
	set_cbw(cmd);

	cmd[4] = 0x7f;
	cmd[9] = 0x02;
	cmd[12] = 0x80;
	cmd[14] = 0x0a;
	cmd[15] = 0x11;
	cmd[16] = 0x06;
	cmd[17] = 0x20;
	cmd[23] = 0x01;

	return send_bulkmsg(dev, cmd, sizeof(cmd));
}

static int
u3g_sierra_reinit(usbd_device_handle dev)
{
	/* Some Sierra devices presents themselves as a umass device with
	 * Windows drivers on it. After installation of the driver, it
	 * reinits into a * 3G serial device.
	 */
	usb_device_request_t req;

	req.bmRequestType = UT_VENDOR;
	req.bRequest = UR_SET_INTERFACE;
	USETW(req.wValue, UF_DEVICE_REMOTE_WAKEUP);
	USETW(req.wIndex, UHF_PORT_CONNECTION);
	USETW(req.wLength, 0);

	(void) usbd_do_request(dev, &req, 0);

	return (UMATCH_HIGHEST); /* Match to prevent umass from attaching */
}

static int
u3g_4gsystems_reinit(usbd_device_handle dev)
{
	/* magic string adapted from usb_modeswitch database */
	unsigned char cmd[31];

	memset(cmd, 0, sizeof(cmd));
	/* Byte 0..3: Command Block Wrapper (CBW) signature */
	set_cbw(cmd);

	cmd[4] = 0x12;
	cmd[5] = 0x34;
	cmd[6] = 0x56;
	cmd[7] = 0x78;
	cmd[8] = 0x80;
	cmd[12] = 0x80;
	cmd[14] = 0x06;
	cmd[15] = 0x06;
	cmd[16] = 0xf5;
	cmd[17] = 0x04;
	cmd[18] = 0x02;
	cmd[19] = 0x52;
	cmd[20] = 0x70;

	return send_bulkmsg(dev, cmd, sizeof(cmd));
}

/*
 * First personality:
 *
 * Claim the entire device if a mode-switch is required.
 */

static int
u3ginit_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	/*
	 * Huawei changes product when it is configured as a modem.
	 */
	switch (uaa->vendor) {
	case USB_VENDOR_HUAWEI:
		if (uaa->product == USB_PRODUCT_HUAWEI_K3765)
			return UMATCH_NONE;

		switch (uaa->product) {
		case USB_PRODUCT_HUAWEI_E1750INIT:
		case USB_PRODUCT_HUAWEI_K3765INIT:
			return u3g_huawei_k3765_reinit(uaa->device);
			break;
		case USB_PRODUCT_HUAWEI_E171INIT:
			return u3g_huawei_e171_reinit(uaa->device);
			break;
		case USB_PRODUCT_HUAWEI_E353INIT:
			return u3g_huawei_e353_reinit(uaa->device);
			break;
		default:
			return u3g_huawei_reinit(uaa->device);
			break;
		}
		break;

	case USB_VENDOR_NOVATEL2:
		switch (uaa->product){
		case USB_PRODUCT_NOVATEL2_MC950D_DRIVER:
		case USB_PRODUCT_NOVATEL2_U760_DRIVER:
			return u3g_bulk_scsi_eject(uaa->device);
			break;
		default:
			break;
		}
		break;

	case USB_VENDOR_SIERRA:
		if (uaa->product == USB_PRODUCT_SIERRA_INSTALLER)
			return u3g_sierra_reinit(uaa->device);
		break;

	case USB_VENDOR_QUALCOMM:
		if (uaa->product == USB_PRODUCT_QUALCOMM_NTT_DOCOMO_L02C_STORAGE)
			return u3g_bulk_scsi_eject(uaa->device);
		break;

	case USB_VENDOR_ZTE:
		switch (uaa->product){
		case USB_PRODUCT_ZTE_INSTALLER:
		case USB_PRODUCT_ZTE_MF820D_INSTALLER:
			(void)u3g_bulk_ata_eject(uaa->device);
			(void)u3g_bulk_scsi_eject(uaa->device);
			return UMATCH_HIGHEST;
		default:
			break;
		}
		break;

	case USB_VENDOR_4GSYSTEMS:
		if (uaa->product == USB_PRODUCT_4GSYSTEMS_XSSTICK_P14_INSTALLER)
			return u3g_4gsystems_reinit(uaa->device);
		break;

	default:
		break;
	}

	return UMATCH_NONE;
}

static void
u3ginit_attach(device_t parent, device_t self, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	aprint_naive("\n");
	aprint_normal(": Switching to 3G mode\n");

	if (uaa->vendor == USB_VENDOR_NOVATEL2) {
		switch (uaa->product) {
	    	case USB_PRODUCT_NOVATEL2_MC950D_DRIVER:
	    	case USB_PRODUCT_NOVATEL2_U760_DRIVER:
			/* About to disappear... */
			return;
			break;
		default:
			break;
		}
	}

	/* Move the device into the configured state. */
	(void) usbd_set_config_index(uaa->device, 0, 1);
}

static int
u3ginit_detach(device_t self, int flags)
{

	return (0);
}


/*
 * Second personality:
 *
 * Claim only those interfaces required for 3G modem operation.
 */

static int
u3g_match(device_t parent, cfdata_t match, void *aux)
{
	struct usbif_attach_arg *uaa = aux;
	usbd_interface_handle iface;
	usb_interface_descriptor_t *id;
	usbd_status error;

	if (!usb_lookup(u3g_devs, uaa->vendor, uaa->product))
		return (UMATCH_NONE);

	error = usbd_device2interface_handle(uaa->device, uaa->ifaceno, &iface);
	if (error) {
		printf("u3g_match: failed to get interface, err=%s\n",
		    usbd_errstr(error));
		return (UMATCH_NONE);
	}

	id = usbd_get_interface_descriptor(iface);
	if (id == NULL) {
		printf("u3g_match: failed to get interface descriptor\n");
		return (UMATCH_NONE);
	}

	/*
	 * Huawei modems use the vendor-specific class for all interfaces,
	 * both tty and CDC NCM, which we should avoid attaching to.
	 */
	if (uaa->vendor == USB_VENDOR_HUAWEI && id->bInterfaceSubClass == 2 &&
	    (id->bInterfaceProtocol & 0xf) == 6)	/* 0x16, 0x46, 0x76 */
		return (UMATCH_NONE);

	/*
	 * 3G modems generally report vendor-specific class
	 *
	 * XXX: this may be too generalised.
	 */
	return ((id->bInterfaceClass == UICLASS_VENDOR) ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

static void
u3g_attach(device_t parent, device_t self, void *aux)
{
	struct u3g_softc *sc = device_private(self);
	struct usbif_attach_arg *uaa = aux;
	usbd_device_handle dev = uaa->device;
	usbd_interface_handle iface;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	struct ucom_attach_args uca;
	usbd_status error;
	int n, intr_address, intr_size; 

	aprint_naive("\n");
	aprint_normal("\n");

	sc->sc_dev = self;
	sc->sc_dying = false;
	sc->sc_udev = dev;

	error = usbd_device2interface_handle(dev, uaa->ifaceno, &iface);
	if (error) {
		aprint_error_dev(self, "failed to get interface, err=%s\n",
		    usbd_errstr(error));
		return;
	}

	id = usbd_get_interface_descriptor(iface);

	uca.info = "3G Modem";
	uca.ibufsize = U3G_BUFF_SIZE;
	uca.obufsize = U3G_BUFF_SIZE;
	uca.ibufsizepad = U3G_BUFF_SIZE;
	uca.opkthdrlen = 0;
	uca.device = dev;
	uca.iface = iface;
	uca.methods = &u3g_methods;
	uca.arg = sc;
	uca.portno = -1;
	uca.bulkin = uca.bulkout = -1;


	sc->sc_ifaceno = uaa->ifaceno;
	intr_address = -1;
	intr_size = 0;

	for (n = 0; n < id->bNumEndpoints; n++) {
		ed = usbd_interface2endpoint_descriptor(iface, n);
		if (ed == NULL) {
			aprint_error_dev(self, "no endpoint descriptor "
			    "for %d (interface: %d)\n", n, sc->sc_ifaceno);
			sc->sc_dying = true;
			return;
		}

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			intr_address = ed->bEndpointAddress;
			intr_size = UGETW(ed->wMaxPacketSize);
		} else
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			uca.bulkin = ed->bEndpointAddress;
		} else
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			uca.bulkout = ed->bEndpointAddress;
		}
		if (uca.bulkin != -1 && uca.bulkout != -1) {
			struct u3g_com *com;
			if (sc->sc_ncom == __arraycount(sc->sc_com)) {
				aprint_error_dev(self, "Need to configure "
				    "more than %zu ttys", sc->sc_ncom);
				continue;
			}
			uca.portno = sc->sc_ncom++;
			com = &sc->sc_com[uca.portno];
			com->c_outpins = 0;
			com->c_msr = UMSR_DSR | UMSR_CTS | UMSR_DCD;
			com->c_open = false;
			com->c_purging = false;
			com->c_dev = config_found_sm_loc(self, "ucombus",
				NULL, &uca, ucomprint, ucomsubmatch);
			uca.bulkin = -1;
			uca.bulkout = -1;
		}
	}

	if (sc->sc_ncom == 0) {
		aprint_error_dev(self, "Missing bulk in/out for interface %d\n",
		    sc->sc_ifaceno);
		sc->sc_dying = true;
		return;
	}

	/*
	 * If the interface has an interrupt pipe, open it immediately so
	 * that we can track input pin state changes regardless of whether
	 * the tty(4) device is open or not.
	 */
	if (intr_address != -1) {
		sc->sc_intr_buff = malloc(intr_size, M_USBDEV, M_WAITOK);
		error = usbd_open_pipe_intr(iface, intr_address,
		    USBD_SHORT_XFER_OK, &sc->sc_intr_pipe, sc, sc->sc_intr_buff,
		    intr_size, u3g_intr, 100);
		if (error) {
			aprint_error_dev(self, "cannot open interrupt pipe "
			    "(addr %d)\n", intr_address);
			return;
		}
	} else {
		sc->sc_intr_pipe = NULL;
		sc->sc_intr_buff = NULL;
	}

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static int
u3g_detach(device_t self, int flags)
{
	struct u3g_softc *sc = device_private(self);
	int rv;

	if (sc->sc_dying)
		return 0;

	pmf_device_deregister(self);

	for (size_t i = 0; i < sc->sc_ncom; i++)
		if (sc->sc_com[i].c_dev != NULL) {
			rv = config_detach(sc->sc_com[i].c_dev, flags);
			if (rv != 0) {
				aprint_verbose_dev(self, "Can't deallocate "
				    "port (%d)", rv);
			}
		}

	if (sc->sc_intr_pipe != NULL) {
		(void) usbd_abort_pipe(sc->sc_intr_pipe);
		(void) usbd_close_pipe(sc->sc_intr_pipe);
		sc->sc_intr_pipe = NULL;
	}
	if (sc->sc_intr_buff != NULL) {
		free(sc->sc_intr_buff, M_USBDEV);
		sc->sc_intr_buff = NULL;
	}

	return (0);
}

static void
u3g_childdet(device_t self, device_t child)
{
	struct u3g_softc *sc = device_private(self);

	for (size_t i = 0; i < sc->sc_ncom; i++)
		    if (sc->sc_com[i].c_dev == child)
			    sc->sc_com[i].c_dev = NULL;
}

static int
u3g_activate(device_t self, enum devact act)
{
	struct u3g_softc *sc = device_private(self);
	int rv = 0;

	switch (act) {
	case DVACT_DEACTIVATE:
		for (size_t i = 0; i < sc->sc_ncom; i++)
			if (sc->sc_com[i].c_dev != NULL &&
			    config_deactivate(sc->sc_com[i].c_dev) && rv == 0)
			rv = -1;
		else
			rv = 0;
		break;

	default:
		break;
	}

	return rv;
}

static void
u3g_intr(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct u3g_softc *sc = (struct u3g_softc *)priv;
	u_char *buf;
	int portno = 0;	/* XXX */
	struct u3g_com *com = &sc->sc_com[portno];

	if (sc->sc_dying)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;
		usbd_clear_endpoint_stall_async(sc->sc_intr_pipe);
		return;
	}

	buf = sc->sc_intr_buff;
	if (buf[0] == 0xa1 && buf[1] == 0x20) {
		u_char msr;

		msr = com->c_msr & ~(UMSR_DCD | UMSR_DSR | UMSR_RI);

		if (buf[8] & U3G_INPIN_DCD)
			msr |= UMSR_DCD;

		if (buf[8] & U3G_INPIN_DSR)
			msr |= UMSR_DSR;

		if (buf[8] & U3G_INPIN_RI)
			msr |= UMSR_RI;

		if (msr != com->c_msr) {
			com->c_msr = msr;
			if (com->c_open)
				ucom_status_change(device_private(com->c_dev));
		}
	}
}

/*ARGSUSED*/
static void
u3g_get_status(void *arg, int portno, u_char *lsr, u_char *msr)
{
	struct u3g_softc *sc = arg;

	if (lsr != NULL)
		*lsr = 0;	/* LSR isn't supported */
	if (msr != NULL)
		*msr = sc->sc_com[portno].c_msr;
}

/*ARGSUSED*/
static void
u3g_set(void *arg, int portno, int reg, int onoff)
{
	struct u3g_softc *sc = arg;
	usb_device_request_t req;
	uint16_t mask, new_state;
	usbd_status err;
	struct u3g_com *com = &sc->sc_com[portno];

	if (sc->sc_dying)
		return;

	switch (reg) {
	case UCOM_SET_DTR:
		mask = U3G_OUTPIN_DTR;
		break;
	case UCOM_SET_RTS:
		mask = U3G_OUTPIN_RTS;
		break;
	default:
		return;
	}

	new_state = com->c_outpins & ~mask;
	if (onoff)
		new_state |= mask;

	if (new_state == com->c_outpins)
		return;

	com->c_outpins = new_state;

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = U3G_SET_PIN;
	USETW(req.wValue, new_state);
	USETW(req.wIndex, sc->sc_ifaceno);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->sc_udev, &req, 0);
	if (err == USBD_STALLED)
		usbd_clear_endpoint_stall(sc->sc_udev->default_pipe);
}

/*ARGSUSED*/
static int 
u3g_open(void *arg, int portno)
{
	struct u3g_softc *sc = arg;
	usb_device_request_t req;
	usb_endpoint_descriptor_t *ed;
	usb_interface_descriptor_t *id;
	usbd_interface_handle ih;
	usbd_status err;
	struct u3g_com *com = &sc->sc_com[portno];
	int i, nin;

	if (sc->sc_dying)
		return (0);

	err = usbd_device2interface_handle(sc->sc_udev, sc->sc_ifaceno, &ih);
	if (err)
		return (EIO);

	id = usbd_get_interface_descriptor(ih);

	for (nin = i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(ih, i);
		if (ed == NULL)	
			return (EIO);

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK &&
		    nin++ == portno) {
			/* Issue ENDPOINT_HALT request */
			req.bmRequestType = UT_WRITE_ENDPOINT;
			req.bRequest = UR_CLEAR_FEATURE;
			USETW(req.wValue, UF_ENDPOINT_HALT);
			USETW(req.wIndex, ed->bEndpointAddress);
			USETW(req.wLength, 0);
			err = usbd_do_request(sc->sc_udev, &req, 0);
			if (err)
				return (EIO);
		}
	}

	com->c_open = true;
	com->c_purging = true;
	getmicrotime(&com->c_purge_start);

	return (0);
}

/*ARGSUSED*/
static void 
u3g_close(void *arg, int portno)
{
	struct u3g_softc *sc = arg;
	struct u3g_com *com = &sc->sc_com[portno];

	com->c_open = false;
}

/*ARGSUSED*/
static void
u3g_read(void *arg, int portno, u_char **cpp, uint32_t *ccp)
{
	struct u3g_softc *sc = arg;
	struct timeval curr_tv, diff_tv;
	struct u3g_com *com = &sc->sc_com[portno];

	/*
	 * If we're not purging input data following first open, do nothing.
	 */
	if (com->c_purging == false)
		return;

	/*
	 * Otherwise check if the purge timeout has expired
	 */
	getmicrotime(&curr_tv);
	timersub(&curr_tv, &com->c_purge_start, &diff_tv);

	if (diff_tv.tv_sec >= U3G_PURGE_SECS) {
		/* Timeout expired. */
		com->c_purging = false;
	} else {
		/* Still purging. Adjust the caller's byte count. */
		*ccp = 0;
	}
}

/*ARGSUSED*/
static void
u3g_write(void *arg, int portno, u_char *to, u_char *from, u_int32_t *count)
{
	struct u3g_softc *sc = arg;
	struct u3g_com *com = &sc->sc_com[portno];

	/*
	 * Stop purging as soon as the first data is written to the device.
	 */
	com->c_purging = false;
	memcpy(to, from, *count);
}

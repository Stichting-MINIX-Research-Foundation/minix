/*	$NetBSD: utoppy.c,v 1.24 2014/07/25 08:10:39 dholland Exp $	*/

/*-
 * Copyright (c) 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Steve C. Woodford.
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
__KERNEL_RCSID(0, "$NetBSD: utoppy.c,v 1.24 2014/07/25 08:10:39 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/vnode.h>
#include <sys/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>
#include <dev/usb/utoppy.h>

#undef UTOPPY_DEBUG
#ifdef UTOPPY_DEBUG
#define	UTOPPY_DBG_OPEN		0x0001
#define	UTOPPY_DBG_CLOSE	0x0002
#define	UTOPPY_DBG_READ		0x0004
#define	UTOPPY_DBG_WRITE	0x0008
#define	UTOPPY_DBG_IOCTL	0x0010
#define	UTOPPY_DBG_SEND_PACKET	0x0020
#define	UTOPPY_DBG_RECV_PACKET	0x0040
#define	UTOPPY_DBG_ADDPATH	0x0080
#define	UTOPPY_DBG_READDIR	0x0100
#define	UTOPPY_DBG_DUMP		0x0200
#define	DPRINTF(l, m)				\
		do {				\
			if (utoppy_debug & l)	\
				printf m;	\
		} while (/*CONSTCOND*/0)
static int utoppy_debug = 0;
static void utoppy_dump_packet(const void *, size_t);
#define	DDUMP_PACKET(p, l)					\
		do {						\
			if (utoppy_debug & UTOPPY_DBG_DUMP)	\
				utoppy_dump_packet((p), (l));	\
		} while (/*CONSTCOND*/0)
#else
#define	DPRINTF(l, m)		/* nothing */
#define	DDUMP_PACKET(p, l)	/* nothing */
#endif


#define	UTOPPY_CONFIG_NO	1
#define	UTOPPY_NUMENDPOINTS	2

#define	UTOPPY_BSIZE		0xffff
#define	UTOPPY_FRAG_SIZE	0x1000
#define	UTOPPY_HEADER_SIZE	8
#define	UTOPPY_SHORT_TIMEOUT	(500)		/* 0.5 seconds */
#define	UTOPPY_LONG_TIMEOUT	(10 * 1000)	/* 10 seconds */

/* Protocol Commands and Responses */
#define	UTOPPY_RESP_ERROR		0x0001
#define	UTOPPY_CMD_ACK			0x0002
#define	 UTOPPY_RESP_SUCCESS		UTOPPY_CMD_ACK
#define	UTOPPY_CMD_CANCEL		0x0003
#define	UTOPPY_CMD_READY		0x0100
#define	UTOPPY_CMD_RESET		0x0101
#define	UTOPPY_CMD_TURBO		0x0102
#define	UTOPPY_CMD_STATS		0x1000
#define  UTOPPY_RESP_STATS_DATA		0x1001
#define	UTOPPY_CMD_READDIR		0x1002
#define	 UTOPPY_RESP_READDIR_DATA	0x1003
#define	 UTOPPY_RESP_READDIR_END	0x1004
#define	UTOPPY_CMD_DELETE		0x1005
#define	UTOPPY_CMD_RENAME		0x1006
#define	UTOPPY_CMD_MKDIR		0x1007
#define	UTOPPY_CMD_FILE			0x1008
#define  UTOPPY_FILE_WRITE		0
#define  UTOPPY_FILE_READ		1
#define	 UTOPPY_RESP_FILE_HEADER	0x1009
#define	 UTOPPY_RESP_FILE_DATA		0x100a
#define	 UTOPPY_RESP_FILE_END		0x100b

enum utoppy_state {
	UTOPPY_STATE_CLOSED,
	UTOPPY_STATE_OPENING,
	UTOPPY_STATE_IDLE,
	UTOPPY_STATE_READDIR,
	UTOPPY_STATE_READFILE,
	UTOPPY_STATE_WRITEFILE
};

struct utoppy_softc {
	device_t sc_dev;
	usbd_device_handle sc_udev;	/* device */
	usbd_interface_handle sc_iface;	/* interface */
	int sc_dying;
	int sc_refcnt;

	enum utoppy_state sc_state;
	u_int sc_turbo_mode;

	int sc_out;
	usbd_pipe_handle sc_out_pipe;	/* bulk out pipe */
	usbd_xfer_handle sc_out_xfer;
	void *sc_out_buf;
	void *sc_out_data;
	uint64_t sc_wr_offset;
	uint64_t sc_wr_size;

	int sc_in;
	usbd_pipe_handle sc_in_pipe;	/* bulk in pipe */
	usbd_xfer_handle sc_in_xfer;
	void *sc_in_buf;
	void *sc_in_data;
	size_t sc_in_len;
	u_int sc_in_offset;
};

struct utoppy_header {
	uint16_t h_len;
	uint16_t h_crc;
	uint16_t h_cmd2;
	uint16_t h_cmd;
	uint8_t h_data[0];
};
#define	UTOPPY_OUT_INIT(sc)					\
	do {							\
		struct utoppy_header *_h = sc->sc_out_data;	\
		_h->h_len = 0;					\
	} while (/*CONSTCOND*/0)

#define	UTOPPY_MJD_1970 40587u	/* MJD value for Jan 1 00:00:00 1970 */

#define	UTOPPY_FTYPE_DIR	1
#define	UTOPPY_FTYPE_FILE	2

#define	UTOPPY_IN_DATA(sc)	\
 ((void*)&(((uint8_t*)(sc)->sc_in_data)[(sc)->sc_in_offset+UTOPPY_HEADER_SIZE]))

dev_type_open(utoppyopen);
dev_type_close(utoppyclose);
dev_type_read(utoppyread);
dev_type_write(utoppywrite);
dev_type_ioctl(utoppyioctl);

const struct cdevsw utoppy_cdevsw = {
	.d_open = utoppyopen,
	.d_close = utoppyclose,
	.d_read = utoppyread,
	.d_write = utoppywrite,
	.d_ioctl = utoppyioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

#define	UTOPPYUNIT(n)	(minor(n))

int             utoppy_match(device_t, cfdata_t, void *);
void            utoppy_attach(device_t, device_t, void *);
int             utoppy_detach(device_t, int);
int             utoppy_activate(device_t, enum devact);
extern struct cfdriver utoppy_cd;
CFATTACH_DECL_NEW(utoppy, sizeof(struct utoppy_softc), utoppy_match, utoppy_attach, utoppy_detach, utoppy_activate);

int 
utoppy_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	if (uaa->vendor == USB_VENDOR_TOPFIELD &&
	    uaa->product == USB_PRODUCT_TOPFIELD_TF5000PVR)
		return (UMATCH_VENDOR_PRODUCT);

	return (UMATCH_NONE);
}

void 
utoppy_attach(device_t parent, device_t self, void *aux)
{
	struct utoppy_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	usbd_device_handle dev = uaa->device;
	usbd_interface_handle iface;
	usb_endpoint_descriptor_t *ed;
	char *devinfop;
	u_int8_t epcount;
	int i;

	sc->sc_dev = self;

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(dev, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	sc->sc_dying = 0;
	sc->sc_refcnt = 0;
	sc->sc_udev = dev;

	if (usbd_set_config_index(dev, 0, 1)
	    || usbd_device2interface_handle(dev, 0, &iface)) {
		aprint_error_dev(self, "Configuration failed\n");
		return;
	}

	epcount = 0;
	(void) usbd_endpoint_count(iface, &epcount);
	if (epcount != UTOPPY_NUMENDPOINTS) {
		aprint_error_dev(self, "Expected %d endpoints, got %d\n",
		    UTOPPY_NUMENDPOINTS, epcount);
		return;
	}

	sc->sc_in = -1;
	sc->sc_out = -1;

	for (i = 0; i < epcount; i++) {
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL) {
			aprint_error_dev(self, "couldn't get ep %d\n", i);
			return;
		}

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->sc_in = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->sc_out = ed->bEndpointAddress;
		}
	}

	if (sc->sc_out == -1 || sc->sc_in == -1) {
		aprint_error_dev(self,
		    "could not find bulk in/out endpoints\n");
		sc->sc_dying = 1;
		return;
	}

	sc->sc_iface = iface;
	sc->sc_udev = dev;

	sc->sc_out_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_out_xfer == NULL) {
		aprint_error_dev(self, "could not allocate bulk out xfer\n");
		goto fail0;
	}

	sc->sc_out_buf = usbd_alloc_buffer(sc->sc_out_xfer, UTOPPY_FRAG_SIZE);
	if (sc->sc_out_buf == NULL) {
		aprint_error_dev(self, "could not allocate bulk out buffer\n");
		goto fail1;
	}

	sc->sc_in_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_in_xfer == NULL) {
		aprint_error_dev(self, "could not allocate bulk in xfer\n");
		goto fail1;
	}

	sc->sc_in_buf = usbd_alloc_buffer(sc->sc_in_xfer, UTOPPY_FRAG_SIZE);
	if (sc->sc_in_buf == NULL) {
		aprint_error_dev(self, "could not allocate bulk in buffer\n");
		goto fail2;
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   sc->sc_dev);

	return;

 fail2:	usbd_free_xfer(sc->sc_in_xfer);
	sc->sc_in_xfer = NULL;

 fail1:	usbd_free_xfer(sc->sc_out_xfer);
	sc->sc_out_xfer = NULL;

 fail0:	sc->sc_dying = 1;
	return;
}

int
utoppy_activate(device_t self, enum devact act)
{
	struct utoppy_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

int 
utoppy_detach(device_t self, int flags)
{
	struct utoppy_softc *sc = device_private(self);
	int maj, mn;
	int s;

	sc->sc_dying = 1;
	if (sc->sc_out_pipe != NULL)
		usbd_abort_pipe(sc->sc_out_pipe);
	if (sc->sc_in_pipe != NULL)
		usbd_abort_pipe(sc->sc_in_pipe);

	if (sc->sc_in_xfer != NULL)
		usbd_free_xfer(sc->sc_in_xfer);
	if (sc->sc_out_xfer != NULL)
		usbd_free_xfer(sc->sc_out_xfer);

	s = splusb();
	if (--sc->sc_refcnt >= 0)
		usb_detach_waitold(sc->sc_dev);
	splx(s);

	/* locate the major number */
	maj = cdevsw_lookup_major(&utoppy_cdevsw);

	/* Nuke the vnodes for any open instances (calls close). */
	mn = device_unit(self);
	vdevgone(maj, mn, mn, VCHR);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   sc->sc_dev);

	return (0);
}

static const uint16_t utoppy_crc16_lookup[] = {
	0x0000, 0xc0c1, 0xc181, 0x0140, 0xc301, 0x03c0, 0x0280, 0xc241,
	0xc601, 0x06c0, 0x0780, 0xc741, 0x0500, 0xc5c1, 0xc481, 0x0440,
	0xcc01, 0x0cc0, 0x0d80, 0xcd41, 0x0f00, 0xcfc1, 0xce81, 0x0e40,
	0x0a00, 0xcac1, 0xcb81, 0x0b40, 0xc901, 0x09c0, 0x0880, 0xc841,
	0xd801, 0x18c0, 0x1980, 0xd941, 0x1b00, 0xdbc1, 0xda81, 0x1a40,
	0x1e00, 0xdec1, 0xdf81, 0x1f40, 0xdd01, 0x1dc0, 0x1c80, 0xdc41,
	0x1400, 0xd4c1, 0xd581, 0x1540, 0xd701, 0x17c0, 0x1680, 0xd641,
	0xd201, 0x12c0, 0x1380, 0xd341, 0x1100, 0xd1c1, 0xd081, 0x1040,
	0xf001, 0x30c0, 0x3180, 0xf141, 0x3300, 0xf3c1, 0xf281, 0x3240,
	0x3600, 0xf6c1, 0xf781, 0x3740, 0xf501, 0x35c0, 0x3480, 0xf441,
	0x3c00, 0xfcc1, 0xfd81, 0x3d40, 0xff01, 0x3fc0, 0x3e80, 0xfe41,
	0xfa01, 0x3ac0, 0x3b80, 0xfb41, 0x3900, 0xf9c1, 0xf881, 0x3840,
	0x2800, 0xe8c1, 0xe981, 0x2940, 0xeb01, 0x2bc0, 0x2a80, 0xea41,
	0xee01, 0x2ec0, 0x2f80, 0xef41, 0x2d00, 0xedc1, 0xec81, 0x2c40,
	0xe401, 0x24c0, 0x2580, 0xe541, 0x2700, 0xe7c1, 0xe681, 0x2640,
	0x2200, 0xe2c1, 0xe381, 0x2340, 0xe101, 0x21c0, 0x2080, 0xe041,
	0xa001, 0x60c0, 0x6180, 0xa141, 0x6300, 0xa3c1, 0xa281, 0x6240,
	0x6600, 0xa6c1, 0xa781, 0x6740, 0xa501, 0x65c0, 0x6480, 0xa441,
	0x6c00, 0xacc1, 0xad81, 0x6d40, 0xaf01, 0x6fc0, 0x6e80, 0xae41,
	0xaa01, 0x6ac0, 0x6b80, 0xab41, 0x6900, 0xa9c1, 0xa881, 0x6840,
	0x7800, 0xb8c1, 0xb981, 0x7940, 0xbb01, 0x7bc0, 0x7a80, 0xba41,
	0xbe01, 0x7ec0, 0x7f80, 0xbf41, 0x7d00, 0xbdc1, 0xbc81, 0x7c40,
	0xb401, 0x74c0, 0x7580, 0xb541, 0x7700, 0xb7c1, 0xb681, 0x7640,
	0x7200, 0xb2c1, 0xb381, 0x7340, 0xb101, 0x71c0, 0x7080, 0xb041,
	0x5000, 0x90c1, 0x9181, 0x5140, 0x9301, 0x53c0, 0x5280, 0x9241,
	0x9601, 0x56c0, 0x5780, 0x9741, 0x5500, 0x95c1, 0x9481, 0x5440,
	0x9c01, 0x5cc0, 0x5d80, 0x9d41, 0x5f00, 0x9fc1, 0x9e81, 0x5e40,
	0x5a00, 0x9ac1, 0x9b81, 0x5b40, 0x9901, 0x59c0, 0x5880, 0x9841,
	0x8801, 0x48c0, 0x4980, 0x8941, 0x4b00, 0x8bc1, 0x8a81, 0x4a40,
	0x4e00, 0x8ec1, 0x8f81, 0x4f40, 0x8d01, 0x4dc0, 0x4c80, 0x8c41,
	0x4400, 0x84c1, 0x8581, 0x4540, 0x8701, 0x47c0, 0x4680, 0x8641,
	0x8201, 0x42c0, 0x4380, 0x8341, 0x4100, 0x81c1, 0x8081, 0x4040
};

#define	UTOPPY_CRC16(ccrc,b)	\
	(utoppy_crc16_lookup[((ccrc) ^ (b)) & 0xffu] ^ ((ccrc) >> 8))

static const int utoppy_usbdstatus_lookup[] = {
	0,		/* USBD_NORMAL_COMPLETION */
	EINPROGRESS,	/* USBD_IN_PROGRESS */
	EALREADY,	/* USBD_PENDING_REQUESTS */
	EAGAIN,		/* USBD_NOT_STARTED */
	EINVAL,		/* USBD_INVAL */
	ENOMEM,		/* USBD_NOMEM */
	ECONNRESET,	/* USBD_CANCELLED */
	EFAULT,		/* USBD_BAD_ADDRESS */
	EBUSY,		/* USBD_IN_USE */
	EADDRNOTAVAIL,	/* USBD_NO_ADDR */
	ENETDOWN,	/* USBD_SET_ADDR_FAILED */
	EIO,		/* USBD_NO_POWER */
	EMLINK,		/* USBD_TOO_DEEP */
	EIO,		/* USBD_IOERROR */
	ENXIO,		/* USBD_NOT_CONFIGURED */
	ETIMEDOUT,	/* USBD_TIMEOUT */
	EBADMSG,	/* USBD_SHORT_XFER */
	EHOSTDOWN,	/* USBD_STALLED */
	EINTR		/* USBD_INTERRUPTED */
};

static __inline int
utoppy_usbd_status2errno(usbd_status err)
{

	if (err >= USBD_ERROR_MAX)
		return (EFAULT);
	return (utoppy_usbdstatus_lookup[err]);
}

#ifdef UTOPPY_DEBUG
static const char *
utoppy_state_string(enum utoppy_state state)
{
	const char *str;

	switch (state) {
	case UTOPPY_STATE_CLOSED:
		str = "CLOSED";
		break;
	case UTOPPY_STATE_OPENING:
		str = "OPENING";
		break;
	case UTOPPY_STATE_IDLE:
		str = "IDLE";
		break;
	case UTOPPY_STATE_READDIR:
		str = "READ DIRECTORY";
		break;
	case UTOPPY_STATE_READFILE:
		str = "READ FILE";
		break;
	case UTOPPY_STATE_WRITEFILE:
		str = "WRITE FILE";
		break;
	default:
		str = "INVALID!";
		break;
	}

	return (str);
}

static void
utoppy_dump_packet(const void *b, size_t len)
{
	const uint8_t *buf = b, *l;
	uint8_t c;
	size_t i, j;

	if (len == 0)
		return;

	len = min(len, 256);

	printf("00: ");

	for (i = 0, l = buf; i < len; i++) {
		printf("%02x ", *buf++);

		if ((i % 16) == 15) {
			for (j = 0; j < 16; j++) {
				c = *l++;
				if (c < ' ' || c > 0x7e)
					c = '.';
				printf("%c", c);
			}

			printf("\n");
			l = buf;

			if ((i + 1) < len)
				printf("%02x: ", (u_int)i + 1);
		}
	}

	while ((i++ % 16) != 0)
		printf("   ");

	if (l < buf) {
		while (l < buf) {
			c = *l++;
			if (c < ' ' || c > 0x7e)
				c = '.';
			printf("%c", c);
		}

		printf("\n");
	}
}
#endif

static usbd_status
utoppy_bulk_transfer(usbd_xfer_handle xfer, usbd_pipe_handle pipe,
    u_int16_t flags, u_int32_t timeout, void *buf, u_int32_t *size,
    const char *lbl)
{
	usbd_status err;

	usbd_setup_xfer(xfer, pipe, 0, buf, *size, flags, timeout, NULL);

	err = usbd_sync_transfer_sig(xfer);

	usbd_get_xfer_status(xfer, NULL, NULL, size, NULL);
	return (err);
}

static int
utoppy_send_packet(struct utoppy_softc *sc, uint16_t cmd, uint32_t timeout)
{
	struct utoppy_header *h;
	usbd_status err;
	uint32_t len;
	uint16_t dlen, crc;
	uint8_t *data, *e, t1, t2;

	h = sc->sc_out_data;

	DPRINTF(UTOPPY_DBG_SEND_PACKET, ("%s: utoppy_send_packet: cmd 0x%04x, "
	    "len %d\n", device_xname(sc->sc_dev), (u_int)cmd, h->h_len));

	dlen = h->h_len;
	len = dlen + UTOPPY_HEADER_SIZE;

	if (len & 1)
		len++;
	if ((len % 64) == 0)
		len += 2;

	if (len >= UTOPPY_BSIZE) {
		DPRINTF(UTOPPY_DBG_SEND_PACKET, ("%s: utoppy_send_packet: "
		    "packet too big (%d)\n", device_xname(sc->sc_dev), (int)len));
		return (EINVAL);
	}

	h->h_len = htole16(dlen + UTOPPY_HEADER_SIZE);
	h->h_cmd2 = 0;
	h->h_cmd = htole16(cmd);

	/* The command word is part of the CRC */
	crc = UTOPPY_CRC16(0,   0);
	crc = UTOPPY_CRC16(crc, 0);
	crc = UTOPPY_CRC16(crc, cmd >> 8);
	crc = UTOPPY_CRC16(crc, cmd);

	/*
	 * If there is data following the header, calculate the CRC and
	 * byte-swap as we go.
	 */
	if (dlen) {
		data = h->h_data;
		e = data + (dlen & ~1);

		do {
			t1 = data[0];
			t2 = data[1];
			crc = UTOPPY_CRC16(crc, t1);
			crc = UTOPPY_CRC16(crc, t2);
			*data++ = t2;
			*data++ = t1;
		} while (data < e);

		if (dlen & 1) {
			t1 = data[0];
			crc = UTOPPY_CRC16(crc, t1);
			data[1] = t1;
		}
	}

	h->h_crc = htole16(crc);
	data = sc->sc_out_data;

	DPRINTF(UTOPPY_DBG_SEND_PACKET, ("%s: utoppy_send_packet: total len "
	    "%d...\n", device_xname(sc->sc_dev), (int)len));
	DDUMP_PACKET(data, len);

	do {
		uint32_t thislen;

		thislen = min(len, UTOPPY_FRAG_SIZE);

		memcpy(sc->sc_out_buf, data, thislen);

		err = utoppy_bulk_transfer(sc->sc_out_xfer, sc->sc_out_pipe,
		    USBD_NO_COPY, timeout, sc->sc_out_buf, &thislen,
		    "utoppytx");

		if (thislen != min(len, UTOPPY_FRAG_SIZE)) {
			DPRINTF(UTOPPY_DBG_SEND_PACKET, ("%s: "
			    "utoppy_send_packet: sent %ld, err %d\n",
			    device_xname(sc->sc_dev), (u_long)thislen, err));
		}

		if (err == 0) {
			len -= thislen;
			data += thislen;
		}
	} while (err == 0 && len);

	DPRINTF(UTOPPY_DBG_SEND_PACKET, ("%s: utoppy_send_packet: "
	    "usbd_bulk_transfer() returned %d.\n", device_xname(sc->sc_dev),err));

	return (err ? utoppy_usbd_status2errno(err) : 0);
}

static int
utoppy_recv_packet(struct utoppy_softc *sc, uint16_t *respp, uint32_t timeout)
{
	struct utoppy_header *h;
	usbd_status err;
	uint32_t len, thislen, requested, bytesleft;
	uint16_t crc;
	uint8_t *data, *e, t1, t2;

	data = sc->sc_in_data;
	len = 0;
	bytesleft = UTOPPY_BSIZE;

	DPRINTF(UTOPPY_DBG_RECV_PACKET, ("%s: utoppy_recv_packet: ...\n",
	    device_xname(sc->sc_dev)));

	do {
		requested = thislen = min(bytesleft, UTOPPY_FRAG_SIZE);

		err = utoppy_bulk_transfer(sc->sc_in_xfer, sc->sc_in_pipe,
		    USBD_NO_COPY | USBD_SHORT_XFER_OK, timeout, sc->sc_in_buf,
		    &thislen, "utoppyrx");

		DPRINTF(UTOPPY_DBG_RECV_PACKET, ("%s: utoppy_recv_packet: "
		    "usbd_bulk_transfer() returned %d, thislen %d, data %p\n",
		    device_xname(sc->sc_dev), err, (u_int)thislen, data));

		if (err == 0) {
			memcpy(data, sc->sc_in_buf, thislen);
			DDUMP_PACKET(data, thislen);
			len += thislen;
			bytesleft -= thislen;
			data += thislen;
		}
	} while (err == 0 && bytesleft && thislen == requested);

	if (err)
		return (utoppy_usbd_status2errno(err));

	h = sc->sc_in_data;

	DPRINTF(UTOPPY_DBG_RECV_PACKET, ("%s: utoppy_recv_packet: received %d "
	    "bytes in total to %p\n", device_xname(sc->sc_dev), (u_int)len, h));
	DDUMP_PACKET(h, len);

	if (len < UTOPPY_HEADER_SIZE || len < (uint32_t)le16toh(h->h_len)) {
		DPRINTF(UTOPPY_DBG_RECV_PACKET, ("%s: utoppy_recv_packet: bad "
		    " length (len %d, h_len %d)\n", device_xname(sc->sc_dev),
		    (int)len, le16toh(h->h_len)));
		return (EIO);
	}

	len = h->h_len = le16toh(h->h_len);
	h->h_crc = le16toh(h->h_crc);
	*respp = h->h_cmd = le16toh(h->h_cmd);
	h->h_cmd2 = le16toh(h->h_cmd2);

	/*
	 * To maximise data throughput when transferring files, acknowledge
	 * data blocks as soon as we receive them. If we detect an error
	 * later on, we can always cancel.
	 */
	if (*respp == UTOPPY_RESP_FILE_DATA) {
		DPRINTF(UTOPPY_DBG_RECV_PACKET, ("%s: utoppy_recv_packet: "
		    "ACKing file data\n", device_xname(sc->sc_dev)));

		UTOPPY_OUT_INIT(sc);
		err = utoppy_send_packet(sc, UTOPPY_CMD_ACK,
		    UTOPPY_SHORT_TIMEOUT);
		if (err) {
			DPRINTF(UTOPPY_DBG_RECV_PACKET, ("%s: "
			    "utoppy_recv_packet: failed to ACK file data: %d\n",
			    device_xname(sc->sc_dev), err));
			return (err);
		}
	}

	/* The command word is part of the CRC */
	crc = UTOPPY_CRC16(0,   h->h_cmd2 >> 8);
	crc = UTOPPY_CRC16(crc, h->h_cmd2);
	crc = UTOPPY_CRC16(crc, h->h_cmd >> 8);
	crc = UTOPPY_CRC16(crc, h->h_cmd);

	/*
	 * Extract any payload, byte-swapping and calculating the CRC16
	 * as we go.
	 */
	if (len > UTOPPY_HEADER_SIZE) {
		data = h->h_data;
		e = data + ((len & ~1) - UTOPPY_HEADER_SIZE);

		while (data < e) {
			t1 = data[0];
			t2 = data[1];
			crc = UTOPPY_CRC16(crc, t2);
			crc = UTOPPY_CRC16(crc, t1);
			*data++ = t2;
			*data++ = t1;
		}

		if (len & 1) {
			t1 = data[1];
			crc = UTOPPY_CRC16(crc, t1);
			*data = t1;
		}
	}

	sc->sc_in_len = (size_t) len - UTOPPY_HEADER_SIZE;
	sc->sc_in_offset = 0;

	DPRINTF(UTOPPY_DBG_RECV_PACKET, ("%s: utoppy_recv_packet: len %d, "
	    "crc 0x%04x, hdrcrc 0x%04x\n", device_xname(sc->sc_dev),
	    (int)len, crc, h->h_crc));
	DDUMP_PACKET(h, len);

	return ((crc == h->h_crc) ? 0 : EBADMSG);
}

static __inline void *
utoppy_current_ptr(void *b)
{
	struct utoppy_header *h = b;

	return (&h->h_data[h->h_len]);
}

static __inline void
utoppy_advance_ptr(void *b, size_t len)
{
	struct utoppy_header *h = b;

	h->h_len += len;
}

static __inline void
utoppy_add_8(struct utoppy_softc *sc, uint8_t v)
{
	struct utoppy_header *h = sc->sc_out_data;
	uint8_t *p;

	p = utoppy_current_ptr(h);
	*p = v;
	utoppy_advance_ptr(h, sizeof(v));
}

static __inline void
utoppy_add_16(struct utoppy_softc *sc, uint16_t v)
{
	struct utoppy_header *h = sc->sc_out_data;
	uint8_t *p;

	p = utoppy_current_ptr(h);
	*p++ = (uint8_t)(v >> 8);
	*p = (uint8_t)v;
	utoppy_advance_ptr(h, sizeof(v));
}

static __inline void
utoppy_add_32(struct utoppy_softc *sc, uint32_t v)
{
	struct utoppy_header *h = sc->sc_out_data;
	uint8_t *p;

	p = utoppy_current_ptr(h);
	*p++ = (uint8_t)(v >> 24);
	*p++ = (uint8_t)(v >> 16);
	*p++ = (uint8_t)(v >> 8);
	*p = (uint8_t)v;
	utoppy_advance_ptr(h, sizeof(v));
}

static __inline void
utoppy_add_64(struct utoppy_softc *sc, uint64_t v)
{
	struct utoppy_header *h = sc->sc_out_data;
	uint8_t *p;

	p = utoppy_current_ptr(h);
	*p++ = (uint8_t)(v >> 56);
	*p++ = (uint8_t)(v >> 48);
	*p++ = (uint8_t)(v >> 40);
	*p++ = (uint8_t)(v >> 32);
	*p++ = (uint8_t)(v >> 24);
	*p++ = (uint8_t)(v >> 16);
	*p++ = (uint8_t)(v >> 8);
	*p = (uint8_t)v;
	utoppy_advance_ptr(h, sizeof(v));
}

static __inline void
utoppy_add_string(struct utoppy_softc *sc, const char *str, size_t len)
{
	struct utoppy_header *h = sc->sc_out_data;
	char *p;

	p = utoppy_current_ptr(h);
	memset(p, 0, len);
	strncpy(p, str, len);
	utoppy_advance_ptr(h, len);
}

static int
utoppy_add_path(struct utoppy_softc *sc, const char *path, int putlen)
{
	struct utoppy_header *h = sc->sc_out_data;
	uint8_t *p, *str, *s;
	size_t len;
	int err;

	p = utoppy_current_ptr(h);

	str = putlen ? (p + sizeof(uint16_t)) : p;

	err = copyinstr(path, str, UTOPPY_MAX_FILENAME_LEN, &len);

	DPRINTF(UTOPPY_DBG_ADDPATH, ("utoppy_add_path: err %d, len %d\n",
	    err, (int)len));

	if (err)
		return (err);

	if (len < 2)
		return (EINVAL);

	/*
	 * copyinstr(9) has already copied the terminating NUL character,
	 * but we append another one in case we have to pad the length
	 * later on.
	 */
	str[len] = '\0';

	/*
	 * The Toppy uses backslash as the directory separator, so convert
	 * all forward slashes.
	 */
	for (s = &str[len - 2]; s >= str; s--)
		if (*s == '/')
			*s = '\\';

	if ((len + h->h_len) & 1)
		len++;

	if (putlen)
		utoppy_add_16(sc, len);

	utoppy_advance_ptr(h, len);

	DPRINTF(UTOPPY_DBG_ADDPATH, ("utoppy_add_path: final len %d\n",
	    (u_int)len));

	return (0);
}

static __inline int
utoppy_get_8(struct utoppy_softc *sc, uint8_t *vp)
{
	uint8_t *p;

	if (sc->sc_in_len < sizeof(*vp))
		return (1);

	p = UTOPPY_IN_DATA(sc);
	*vp = *p;
	sc->sc_in_offset += sizeof(*vp);
	sc->sc_in_len -= sizeof(*vp);
	return (0);
}

static __inline int
utoppy_get_16(struct utoppy_softc *sc, uint16_t *vp)
{
	uint16_t v;
	uint8_t *p;

	if (sc->sc_in_len < sizeof(v))
		return (1);

	p = UTOPPY_IN_DATA(sc);
	v = *p++;
	v = (v << 8) | *p;
	*vp = v;
	sc->sc_in_offset += sizeof(v);
	sc->sc_in_len -= sizeof(v);
	return (0);
}

static __inline int
utoppy_get_32(struct utoppy_softc *sc, uint32_t *vp)
{
	uint32_t v;
	uint8_t *p;

	if (sc->sc_in_len < sizeof(v))
		return (1);

	p = UTOPPY_IN_DATA(sc);
	v = *p++;
	v = (v << 8) | *p++;
	v = (v << 8) | *p++;
	v = (v << 8) | *p;
	*vp = v;
	sc->sc_in_offset += sizeof(v);
	sc->sc_in_len -= sizeof(v);
	return (0);
}

static __inline int
utoppy_get_64(struct utoppy_softc *sc, uint64_t *vp)
{
	uint64_t v;
	uint8_t *p;

	if (sc->sc_in_len < sizeof(v))
		return (1);

	p = UTOPPY_IN_DATA(sc);
	v = *p++;
	v = (v << 8) | *p++;
	v = (v << 8) | *p++;
	v = (v << 8) | *p++;
	v = (v << 8) | *p++;
	v = (v << 8) | *p++;
	v = (v << 8) | *p++;
	v = (v << 8) | *p;
	*vp = v;
	sc->sc_in_offset += sizeof(v);
	sc->sc_in_len -= sizeof(v);
	return (0);
}

static __inline int
utoppy_get_string(struct utoppy_softc *sc, char *str, size_t len)
{
	char *p;

	if (sc->sc_in_len < len)
		return (1);

	memset(str, 0, len);
	p = UTOPPY_IN_DATA(sc);
	strncpy(str, p, len);
	sc->sc_in_offset += len;
	sc->sc_in_len -= len;
	return (0);
}

static int
utoppy_command(struct utoppy_softc *sc, uint16_t cmd, int timeout,
    uint16_t *presp)
{
	int err;

	err = utoppy_send_packet(sc, cmd, timeout);
	if (err)
		return (err);

	err = utoppy_recv_packet(sc, presp, timeout);
	if (err == EBADMSG) {
		UTOPPY_OUT_INIT(sc);
		utoppy_send_packet(sc, UTOPPY_RESP_ERROR, timeout);
	}

	return (err);
}

static int
utoppy_timestamp_decode(struct utoppy_softc *sc, time_t *tp)
{
	uint16_t mjd;
	uint8_t hour, minute, sec;
	uint32_t rv;

	if (utoppy_get_16(sc, &mjd) || utoppy_get_8(sc, &hour) ||
	    utoppy_get_8(sc, &minute) || utoppy_get_8(sc, &sec))
		return (1);

	if (mjd == 0xffffu && hour == 0xffu && minute == 0xffu && sec == 0xffu){
		*tp = 0;
		return (0);
	}

	rv = (mjd < UTOPPY_MJD_1970) ? UTOPPY_MJD_1970 : (uint32_t) mjd;

	/* Calculate seconds since 1970 */
	rv = (rv - UTOPPY_MJD_1970) * 60 * 60 * 24;

	/* Add in the hours, minutes, and seconds */
	rv += (uint32_t)hour * 60 * 60;
	rv += (uint32_t)minute * 60;
	rv += sec;
	*tp = (time_t)rv;

	return (0);
}

static void
utoppy_timestamp_encode(struct utoppy_softc *sc, time_t t)
{
	u_int mjd, hour, minute;

	mjd = t / (60 * 60 * 24);
	t -= mjd * 60 * 60 * 24;

	hour = t / (60 * 60);
	t -= hour * 60 * 60;

	minute = t / 60;
	t -= minute * 60;

	utoppy_add_16(sc, mjd + UTOPPY_MJD_1970);
	utoppy_add_8(sc, hour);
	utoppy_add_8(sc, minute);
	utoppy_add_8(sc, t);
}

static int
utoppy_turbo_mode(struct utoppy_softc *sc, int state)
{
	uint16_t r;
	int err;

	UTOPPY_OUT_INIT(sc);
	utoppy_add_32(sc, state);

	err = utoppy_command(sc, UTOPPY_CMD_TURBO, UTOPPY_SHORT_TIMEOUT, &r);
	if (err)
		return (err);

	return ((r == UTOPPY_RESP_SUCCESS) ? 0 : EIO);
}

static int
utoppy_check_ready(struct utoppy_softc *sc)
{
	uint16_t r;
	int err;

	UTOPPY_OUT_INIT(sc);

	err = utoppy_command(sc, UTOPPY_CMD_READY, UTOPPY_LONG_TIMEOUT, &r);
	if (err)
		return (err);

	return ((r == UTOPPY_RESP_SUCCESS) ? 0 : EIO);
}

static int
utoppy_cancel(struct utoppy_softc *sc)
{
	uint16_t r;
	int err, i;

	/*
	 * Issue the cancel command serveral times. the Toppy doesn't
	 * always respond to the first.
	 */
	for (i = 0; i < 3; i++) {
		UTOPPY_OUT_INIT(sc);
		err = utoppy_command(sc, UTOPPY_CMD_CANCEL,
		    UTOPPY_SHORT_TIMEOUT, &r);
		if (err == 0 && r == UTOPPY_RESP_SUCCESS)
			break;
		err = ETIMEDOUT;
	}

	if (err)
		return (err);

	/*
	 * Make sure turbo mode is off, otherwise the Toppy will not
	 * respond to remote control input.
	 */
	(void) utoppy_turbo_mode(sc, 0);

	sc->sc_state = UTOPPY_STATE_IDLE;
	return (0);
}

static int
utoppy_stats(struct utoppy_softc *sc, struct utoppy_stats *us)
{
	uint32_t hsize, hfree;
	uint16_t r;
	int err;

	UTOPPY_OUT_INIT(sc);
	err = utoppy_command(sc, UTOPPY_CMD_STATS, UTOPPY_LONG_TIMEOUT, &r);
	if (err)
		return (err);

	if (r != UTOPPY_RESP_STATS_DATA)
		return (EIO);

	if (utoppy_get_32(sc, &hsize) || utoppy_get_32(sc, &hfree))
		return (EIO);

	us->us_hdd_size = hsize;
	us->us_hdd_size *= 1024;
	us->us_hdd_free = hfree;
	us->us_hdd_free *= 1024;

	return (0);
}

static int
utoppy_readdir_next(struct utoppy_softc *sc)
{
	uint16_t resp;
	int err;

	DPRINTF(UTOPPY_DBG_READDIR, ("%s: utoppy_readdir_next: running...\n",
	    device_xname(sc->sc_dev)));

	/*
	 * Fetch the next READDIR response
	 */
	err = utoppy_recv_packet(sc, &resp, UTOPPY_LONG_TIMEOUT);
	if (err) {
		DPRINTF(UTOPPY_DBG_READDIR, ("%s: utoppy_readdir_next: "
		    "utoppy_recv_packet() returned %d\n",
		    device_xname(sc->sc_dev), err));
		if (err == EBADMSG) {
			UTOPPY_OUT_INIT(sc);
			utoppy_send_packet(sc, UTOPPY_RESP_ERROR,
			    UTOPPY_LONG_TIMEOUT);
		}
		utoppy_cancel(sc);
		return (err);
	}

	DPRINTF(UTOPPY_DBG_READDIR, ("%s: utoppy_readdir_next: "
	    "utoppy_recv_packet() returned %d, len %ld\n",
	    device_xname(sc->sc_dev), err, (u_long)sc->sc_in_len));

	switch (resp) {
	case UTOPPY_RESP_READDIR_DATA:
		DPRINTF(UTOPPY_DBG_READDIR, ("%s: utoppy_readdir_next: "
		    "UTOPPY_RESP_READDIR_DATA\n", device_xname(sc->sc_dev)));

		UTOPPY_OUT_INIT(sc);
		err = utoppy_send_packet(sc, UTOPPY_CMD_ACK,
		    UTOPPY_LONG_TIMEOUT);
		if (err) {
			DPRINTF(UTOPPY_DBG_READDIR, ("%s: utoppy_readdir_next: "
			    "utoppy_send_packet(ACK) returned %d\n",
			    device_xname(sc->sc_dev), err));
			utoppy_cancel(sc);
			return (err);
		}
		sc->sc_state = UTOPPY_STATE_READDIR;
		sc->sc_in_offset = 0;
		break;

	case UTOPPY_RESP_READDIR_END:
		DPRINTF(UTOPPY_DBG_READDIR, ("%s: utoppy_readdir_next: "
		    "UTOPPY_RESP_READDIR_END\n", device_xname(sc->sc_dev)));

		UTOPPY_OUT_INIT(sc);
		utoppy_send_packet(sc, UTOPPY_CMD_ACK, UTOPPY_SHORT_TIMEOUT);
		sc->sc_state = UTOPPY_STATE_IDLE;
		sc->sc_in_len = 0;
		break;

	default:
		DPRINTF(UTOPPY_DBG_READDIR, ("%s: utoppy_readdir_next: "
		    "bad response: 0x%x\n", device_xname(sc->sc_dev), resp));
		sc->sc_state = UTOPPY_STATE_IDLE;
		sc->sc_in_len = 0;
		return (EIO);
	}

	return (0);
}

static size_t
utoppy_readdir_decode(struct utoppy_softc *sc, struct utoppy_dirent *ud)
{
	uint8_t ftype;

	DPRINTF(UTOPPY_DBG_READDIR, ("%s: utoppy_readdir_decode: bytes left"
	    " %d\n", device_xname(sc->sc_dev), (int)sc->sc_in_len));

	if (utoppy_timestamp_decode(sc, &ud->ud_mtime) ||
	    utoppy_get_8(sc, &ftype) || utoppy_get_64(sc, &ud->ud_size) ||
	    utoppy_get_string(sc, ud->ud_path, UTOPPY_MAX_FILENAME_LEN + 1) ||
	    utoppy_get_32(sc, &ud->ud_attributes)) {
		DPRINTF(UTOPPY_DBG_READDIR, ("%s: utoppy_readdir_decode: no "
		    "more to decode\n", device_xname(sc->sc_dev)));
		return (0);
	}

	switch (ftype) {
	case UTOPPY_FTYPE_DIR:
		ud->ud_type = UTOPPY_DIRENT_DIRECTORY;
		break;
	case UTOPPY_FTYPE_FILE:
		ud->ud_type = UTOPPY_DIRENT_FILE;
		break;
	default:
		ud->ud_type = UTOPPY_DIRENT_UNKNOWN;
		break;
	}

	DPRINTF(UTOPPY_DBG_READDIR, ("%s: utoppy_readdir_decode: %s '%s', "
	    "size %lld, time 0x%08lx, attr 0x%08x\n", device_xname(sc->sc_dev),
	    (ftype == UTOPPY_FTYPE_DIR) ? "DIR" :
	    ((ftype == UTOPPY_FTYPE_FILE) ? "FILE" : "UNKNOWN"), ud->ud_path,
	    ud->ud_size, (u_long)ud->ud_mtime, ud->ud_attributes));

	return (1);
}

static int
utoppy_readfile_next(struct utoppy_softc *sc)
{
	uint64_t off;
	uint16_t resp;
	int err;

	err = utoppy_recv_packet(sc, &resp, UTOPPY_LONG_TIMEOUT);
	if (err) {
		DPRINTF(UTOPPY_DBG_READ, ("%s: utoppy_readfile_next: "
		    "utoppy_recv_packet() returned %d\n",
		    device_xname(sc->sc_dev), err));
		utoppy_cancel(sc);
		return (err);
	}

	switch (resp) {
	case UTOPPY_RESP_FILE_HEADER:
		/* ACK it */
		UTOPPY_OUT_INIT(sc);
		err = utoppy_send_packet(sc, UTOPPY_CMD_ACK,
		    UTOPPY_LONG_TIMEOUT);
		if (err) {
			DPRINTF(UTOPPY_DBG_READ, ("%s: utoppy_readfile_next: "
			    "utoppy_send_packet(UTOPPY_CMD_ACK) returned %d\n",
			    device_xname(sc->sc_dev), err));
			utoppy_cancel(sc);
			return (err);
		}

		sc->sc_in_len = 0;
		DPRINTF(UTOPPY_DBG_READ, ("%s: utoppy_readfile_next: "
		    "FILE_HEADER done\n", device_xname(sc->sc_dev)));
		break;

	case UTOPPY_RESP_FILE_DATA:
		/* Already ACK'd */
		if (utoppy_get_64(sc, &off)) {
			DPRINTF(UTOPPY_DBG_READ, ("%s: utoppy_readfile_next: "
			    "UTOPPY_RESP_FILE_DATA did not provide offset\n",
			    device_xname(sc->sc_dev)));
			utoppy_cancel(sc);
			return (EBADMSG);
		}

		DPRINTF(UTOPPY_DBG_READ, ("%s: utoppy_readfile_next: "
		    "UTOPPY_RESP_FILE_DATA: offset %lld, bytes left %ld\n",
		    device_xname(sc->sc_dev), off, (u_long)sc->sc_in_len));
		break;

	case UTOPPY_RESP_FILE_END:
		DPRINTF(UTOPPY_DBG_READ, ("%s: utoppy_readfile_next: "
		    "UTOPPY_RESP_FILE_END: sending ACK\n",
		    device_xname(sc->sc_dev)));
		UTOPPY_OUT_INIT(sc);
		utoppy_send_packet(sc, UTOPPY_CMD_ACK, UTOPPY_SHORT_TIMEOUT);
		/*FALLTHROUGH*/

	case UTOPPY_RESP_SUCCESS:
		sc->sc_state = UTOPPY_STATE_IDLE;
		(void) utoppy_turbo_mode(sc, 0);
		DPRINTF(UTOPPY_DBG_READ, ("%s: utoppy_readfile_next: all "
		    "done\n", device_xname(sc->sc_dev)));
		break;

	case UTOPPY_RESP_ERROR:
	default:
		DPRINTF(UTOPPY_DBG_READ, ("%s: utoppy_readfile_next: bad "
		    "response code 0x%0x\n", device_xname(sc->sc_dev), resp));
		utoppy_cancel(sc);
		return (EIO);
	}

	return (0);
}

int
utoppyopen(dev_t dev, int flag, int mode,
    struct lwp *l)
{
	struct utoppy_softc *sc;
	int error = 0;

	sc = device_lookup_private(&utoppy_cd, UTOPPYUNIT(dev));
	if (sc == NULL)
		return ENXIO;

	if (sc == NULL || sc->sc_iface == NULL || sc->sc_dying)
		return (ENXIO);

	if (sc->sc_state != UTOPPY_STATE_CLOSED) {
		DPRINTF(UTOPPY_DBG_OPEN, ("%s: utoppyopen: already open\n",
		    device_xname(sc->sc_dev)));
		return (EBUSY);
	}

	DPRINTF(UTOPPY_DBG_OPEN, ("%s: utoppyopen: opening...\n",
	    device_xname(sc->sc_dev)));

	sc->sc_refcnt++;
	sc->sc_state = UTOPPY_STATE_OPENING;
	sc->sc_turbo_mode = 0;
	sc->sc_out_pipe = NULL;
	sc->sc_in_pipe = NULL;

	if (usbd_open_pipe(sc->sc_iface, sc->sc_out, 0, &sc->sc_out_pipe)) {
		DPRINTF(UTOPPY_DBG_OPEN, ("%s: utoppyopen: usbd_open_pipe(OUT) "
		    "failed\n", device_xname(sc->sc_dev)));
		error = EIO;
		goto done;
	}

	if (usbd_open_pipe(sc->sc_iface, sc->sc_in, 0, &sc->sc_in_pipe)) {
		DPRINTF(UTOPPY_DBG_OPEN, ("%s: utoppyopen: usbd_open_pipe(IN) "
		    "failed\n", device_xname(sc->sc_dev)));
		error = EIO;
		usbd_close_pipe(sc->sc_out_pipe);
		sc->sc_out_pipe = NULL;
		goto done;
	}

	sc->sc_out_data = malloc(UTOPPY_BSIZE + 1, M_DEVBUF, M_WAITOK);
	if (sc->sc_out_data == NULL) {
		error = ENOMEM;
		goto error;
	}

	sc->sc_in_data = malloc(UTOPPY_BSIZE + 1, M_DEVBUF, M_WAITOK);
	if (sc->sc_in_data == NULL) {
		free(sc->sc_out_data, M_DEVBUF);
		sc->sc_out_data = NULL;
		error = ENOMEM;
		goto error;
	}

	if ((error = utoppy_cancel(sc)) != 0)
		goto error;

	if ((error = utoppy_check_ready(sc)) != 0) {
		DPRINTF(UTOPPY_DBG_OPEN, ("%s: utoppyopen: utoppy_check_ready()"
		    " returned %d\n", device_xname(sc->sc_dev), error));
 error:
		usbd_abort_pipe(sc->sc_out_pipe);
		usbd_close_pipe(sc->sc_out_pipe);
		sc->sc_out_pipe = NULL;
		usbd_abort_pipe(sc->sc_in_pipe);
		usbd_close_pipe(sc->sc_in_pipe);
		sc->sc_in_pipe = NULL;
	}

 done:
	sc->sc_state = error ? UTOPPY_STATE_CLOSED : UTOPPY_STATE_IDLE;

	DPRINTF(UTOPPY_DBG_OPEN, ("%s: utoppyopen: done. error %d, new state "
	    "'%s'\n", device_xname(sc->sc_dev), error,
	    utoppy_state_string(sc->sc_state)));

	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);

	return (error);
}

int
utoppyclose(dev_t dev, int flag, int mode,
    struct lwp *l)
{
	struct utoppy_softc *sc;
	usbd_status err;

	sc = device_lookup_private(&utoppy_cd, UTOPPYUNIT(dev));

	DPRINTF(UTOPPY_DBG_CLOSE, ("%s: utoppyclose: closing...\n",
	    device_xname(sc->sc_dev)));

	if (sc->sc_state < UTOPPY_STATE_IDLE) {
		/* We are being forced to close before the open completed. */
		DPRINTF(UTOPPY_DBG_CLOSE, ("%s: utoppyclose: not properly open:"
		    " %s\n", device_xname(sc->sc_dev),
		    utoppy_state_string(sc->sc_state)));
		return (0);
	}

	if (sc->sc_out_data)
		(void) utoppy_cancel(sc);

	if (sc->sc_out_pipe != NULL) {
		if ((err = usbd_abort_pipe(sc->sc_out_pipe)) != 0)
			printf("usbd_abort_pipe(OUT) returned %d\n", err);
		if ((err = usbd_close_pipe(sc->sc_out_pipe)) != 0)
			printf("usbd_close_pipe(OUT) returned %d\n", err);
		sc->sc_out_pipe = NULL;
	}

	if (sc->sc_in_pipe != NULL) {
		if ((err = usbd_abort_pipe(sc->sc_in_pipe)) != 0)
			printf("usbd_abort_pipe(IN) returned %d\n", err);
		if ((err = usbd_close_pipe(sc->sc_in_pipe)) != 0)
			printf("usbd_close_pipe(IN) returned %d\n", err);
		sc->sc_in_pipe = NULL;
	}

	if (sc->sc_out_data) {
		free(sc->sc_out_data, M_DEVBUF);
		sc->sc_out_data = NULL;
	}

	if (sc->sc_in_data) {
		free(sc->sc_in_data, M_DEVBUF);
		sc->sc_in_data = NULL;
	}

	sc->sc_state = UTOPPY_STATE_CLOSED;

	DPRINTF(UTOPPY_DBG_CLOSE, ("%s: utoppyclose: done.\n",
	    device_xname(sc->sc_dev)));

	return (0);
}

int
utoppyread(dev_t dev, struct uio *uio, int flags)
{
	struct utoppy_softc *sc;
	struct utoppy_dirent ud;
	size_t len;
	int err;

	sc = device_lookup_private(&utoppy_cd, UTOPPYUNIT(dev));

	if (sc->sc_dying)
		return (EIO);

	sc->sc_refcnt++;

	DPRINTF(UTOPPY_DBG_READ, ("%s: utoppyread: reading: state '%s'\n",
	    device_xname(sc->sc_dev), utoppy_state_string(sc->sc_state)));

	switch (sc->sc_state) {
	case UTOPPY_STATE_READDIR:
		err = 0;
		while (err == 0 && uio->uio_resid >= sizeof(ud) &&
		    sc->sc_state != UTOPPY_STATE_IDLE) {
			if (utoppy_readdir_decode(sc, &ud) == 0)
				err = utoppy_readdir_next(sc);
			else
			if ((err = uiomove(&ud, sizeof(ud), uio)) != 0)
				utoppy_cancel(sc); 
		}
		break;

	case UTOPPY_STATE_READFILE:
		err = 0;
		while (err == 0 && uio->uio_resid > 0 &&
		    sc->sc_state != UTOPPY_STATE_IDLE) {
			DPRINTF(UTOPPY_DBG_READ, ("%s: utoppyread: READFILE: "
			    "resid %ld, bytes_left %ld\n",
			    device_xname(sc->sc_dev), (u_long)uio->uio_resid,
			    (u_long)sc->sc_in_len));

			if (sc->sc_in_len == 0 &&
			    (err = utoppy_readfile_next(sc)) != 0) {
				DPRINTF(UTOPPY_DBG_READ, ("%s: utoppyread: "
				    "READFILE: utoppy_readfile_next returned "
				    "%d\n", device_xname(sc->sc_dev), err));
				break;
			}

			len = min(uio->uio_resid, sc->sc_in_len);
			if (len) {
				err = uiomove(UTOPPY_IN_DATA(sc), len, uio);
				if (err == 0) {
					sc->sc_in_offset += len;
					sc->sc_in_len -= len;
				}
			}
		}
		break;

	case UTOPPY_STATE_IDLE:
		err = 0;
		break;

	case UTOPPY_STATE_WRITEFILE:
		err = EBUSY;
		break;

	default:
		err = EIO;
		break;
	}

	DPRINTF(UTOPPY_DBG_READ, ("%s: utoppyread: done. err %d, state '%s'\n",
	    device_xname(sc->sc_dev), err, utoppy_state_string(sc->sc_state)));

	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);

	return (err);
}

int
utoppywrite(dev_t dev, struct uio *uio, int flags)
{
	struct utoppy_softc *sc;
	uint16_t resp;
	size_t len;
	int err;

	sc = device_lookup_private(&utoppy_cd, UTOPPYUNIT(dev));

	if (sc->sc_dying)
		return (EIO);

	switch(sc->sc_state) {
	case UTOPPY_STATE_WRITEFILE:
		break;

	case UTOPPY_STATE_IDLE:
		return (0);

	default:
		return (EIO);
	}

	sc->sc_refcnt++;
	err = 0;

	DPRINTF(UTOPPY_DBG_WRITE, ("%s: utoppywrite: PRE-WRITEFILE: resid %ld, "
	    "wr_size %lld, wr_offset %lld\n", device_xname(sc->sc_dev),
	    (u_long)uio->uio_resid, sc->sc_wr_size, sc->sc_wr_offset));

	while (sc->sc_state == UTOPPY_STATE_WRITEFILE &&
	    (len = min(uio->uio_resid, sc->sc_wr_size)) != 0) {

		len = min(len, UTOPPY_BSIZE - (UTOPPY_HEADER_SIZE +
		    sizeof(uint64_t) + 3));

		DPRINTF(UTOPPY_DBG_WRITE, ("%s: utoppywrite: uiomove(%ld)\n",
		    device_xname(sc->sc_dev), (u_long)len));

		UTOPPY_OUT_INIT(sc);
		utoppy_add_64(sc, sc->sc_wr_offset);

		err = uiomove(utoppy_current_ptr(sc->sc_out_data), len, uio);
		if (err) {
			DPRINTF(UTOPPY_DBG_WRITE, ("%s: utoppywrite: uiomove() "
			    "returned %d\n", device_xname(sc->sc_dev), err));
			break;
		}

		utoppy_advance_ptr(sc->sc_out_data, len);

		err = utoppy_command(sc, UTOPPY_RESP_FILE_DATA,
		    UTOPPY_LONG_TIMEOUT, &resp);
		if (err) {
			DPRINTF(UTOPPY_DBG_WRITE, ("%s: utoppywrite: "
			    "utoppy_command(UTOPPY_RESP_FILE_DATA) "
			    "returned %d\n", device_xname(sc->sc_dev), err));
			break;
		}
		if (resp != UTOPPY_RESP_SUCCESS) {
			DPRINTF(UTOPPY_DBG_WRITE, ("%s: utoppywrite: "
			    "utoppy_command(UTOPPY_RESP_FILE_DATA) returned "
			    "bad response 0x%x\n", device_xname(sc->sc_dev),
			    resp));
			utoppy_cancel(sc);
			err = EIO;
			break;
		}

		sc->sc_wr_offset += len;
		sc->sc_wr_size -= len;
	}

	DPRINTF(UTOPPY_DBG_WRITE, ("%s: utoppywrite: POST-WRITEFILE: resid %ld,"
	    " wr_size %lld, wr_offset %lld, err %d\n", device_xname(sc->sc_dev),
	    (u_long)uio->uio_resid, sc->sc_wr_size, sc->sc_wr_offset, err));

	if (err == 0 && sc->sc_wr_size == 0) {
		DPRINTF(UTOPPY_DBG_WRITE, ("%s: utoppywrite: sending "
		    "FILE_END...\n", device_xname(sc->sc_dev)));
		UTOPPY_OUT_INIT(sc);
		err = utoppy_command(sc, UTOPPY_RESP_FILE_END,
		    UTOPPY_LONG_TIMEOUT, &resp);
		if (err) {
			DPRINTF(UTOPPY_DBG_WRITE, ("%s: utoppywrite: "
			    "utoppy_command(UTOPPY_RESP_FILE_END) returned "
			    "%d\n", device_xname(sc->sc_dev), err));

			utoppy_cancel(sc);
		}

		sc->sc_state = UTOPPY_STATE_IDLE;
		DPRINTF(UTOPPY_DBG_WRITE, ("%s: utoppywrite: state %s\n",
		    device_xname(sc->sc_dev), utoppy_state_string(sc->sc_state)));
	}

	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);

	return (err);
}

int
utoppyioctl(dev_t dev, u_long cmd, void *data, int flag,
    struct lwp *l)
{
	struct utoppy_softc *sc;
	struct utoppy_rename *ur;
	struct utoppy_readfile *urf;
	struct utoppy_writefile *uw;
	char uwf[UTOPPY_MAX_FILENAME_LEN + 1], *uwfp;
	uint16_t resp;
	int err;

	sc = device_lookup_private(&utoppy_cd, UTOPPYUNIT(dev));

	if (sc->sc_dying)
		return (EIO);

	DPRINTF(UTOPPY_DBG_IOCTL, ("%s: utoppyioctl: cmd 0x%08lx, state '%s'\n",
	    device_xname(sc->sc_dev), cmd, utoppy_state_string(sc->sc_state)));

	if (sc->sc_state != UTOPPY_STATE_IDLE && cmd != UTOPPYIOCANCEL) {
		DPRINTF(UTOPPY_DBG_IOCTL, ("%s: utoppyioctl: still busy.\n",
		    device_xname(sc->sc_dev)));
		return (EBUSY);
	}

	sc->sc_refcnt++;

	switch (cmd) {
	case UTOPPYIOTURBO:
		err = 0;
		sc->sc_turbo_mode = *((int *)data) ? 1 : 0;
		DPRINTF(UTOPPY_DBG_IOCTL, ("%s: utoppyioctl: UTOPPYIOTURBO: "
		    "%s\n", device_xname(sc->sc_dev), sc->sc_turbo_mode ? "On" :
		    "Off"));
		break;

	case UTOPPYIOCANCEL:
		DPRINTF(UTOPPY_DBG_IOCTL, ("%s: utoppyioctl: UTOPPYIOCANCEL\n",
		    device_xname(sc->sc_dev)));
		err = utoppy_cancel(sc);
		break;

	case UTOPPYIOREBOOT:
		DPRINTF(UTOPPY_DBG_IOCTL, ("%s: utoppyioctl: UTOPPYIOREBOOT\n",
		    device_xname(sc->sc_dev)));
		UTOPPY_OUT_INIT(sc);
		err = utoppy_command(sc, UTOPPY_CMD_RESET, UTOPPY_LONG_TIMEOUT,
		    &resp);
		if (err)
			break;

		if (resp != UTOPPY_RESP_SUCCESS)
			err = EIO;
		break;

	case UTOPPYIOSTATS:
		DPRINTF(UTOPPY_DBG_IOCTL, ("%s: utoppyioctl: UTOPPYIOSTATS\n",
		    device_xname(sc->sc_dev)));
		err = utoppy_stats(sc, (struct utoppy_stats *)data);
		break;

	case UTOPPYIORENAME:
		DPRINTF(UTOPPY_DBG_IOCTL, ("%s: utoppyioctl: UTOPPYIORENAME\n",
		    device_xname(sc->sc_dev)));
		ur = (struct utoppy_rename *)data;
		UTOPPY_OUT_INIT(sc);

		if ((err = utoppy_add_path(sc, ur->ur_old_path, 1)) != 0)
			break;
		if ((err = utoppy_add_path(sc, ur->ur_new_path, 1)) != 0)
			break;

		err = utoppy_command(sc, UTOPPY_CMD_RENAME, UTOPPY_LONG_TIMEOUT,
		    &resp);
		if (err)
			break;

		if (resp != UTOPPY_RESP_SUCCESS)
			err = EIO;
		break;

	case UTOPPYIOMKDIR:
		DPRINTF(UTOPPY_DBG_IOCTL, ("%s: utoppyioctl: UTOPPYIOMKDIR\n",
		    device_xname(sc->sc_dev)));
		UTOPPY_OUT_INIT(sc);
		err = utoppy_add_path(sc, *((const char **)data), 1);
		if (err)
			break;

		err = utoppy_command(sc, UTOPPY_CMD_MKDIR, UTOPPY_LONG_TIMEOUT,
		    &resp);
		if (err)
			break;

		if (resp != UTOPPY_RESP_SUCCESS)
			err = EIO;
		break;

	case UTOPPYIODELETE:
		DPRINTF(UTOPPY_DBG_IOCTL, ("%s: utoppyioctl: UTOPPYIODELETE\n",
		    device_xname(sc->sc_dev)));
		UTOPPY_OUT_INIT(sc);
		err = utoppy_add_path(sc, *((const char **)data), 0);
		if (err)
			break;

		err = utoppy_command(sc, UTOPPY_CMD_DELETE, UTOPPY_LONG_TIMEOUT,
		    &resp);
		if (err)
			break;

		if (resp != UTOPPY_RESP_SUCCESS)
			err = EIO;
		break;

	case UTOPPYIOREADDIR:
		DPRINTF(UTOPPY_DBG_IOCTL, ("%s: utoppyioctl: UTOPPYIOREADDIR\n",
		    device_xname(sc->sc_dev)));
		UTOPPY_OUT_INIT(sc);
		err = utoppy_add_path(sc, *((const char **)data), 0);
		if (err) {
			DPRINTF(UTOPPY_DBG_READDIR, ("%s: utoppyioctl: "
			    "utoppy_add_path() returned %d\n",
			    device_xname(sc->sc_dev), err));
			break;
		}

		err = utoppy_send_packet(sc, UTOPPY_CMD_READDIR,
		    UTOPPY_LONG_TIMEOUT);
		if (err != 0) {
			DPRINTF(UTOPPY_DBG_READDIR, ("%s: utoppyioctl: "
			    "UTOPPY_CMD_READDIR returned %d\n",
			    device_xname(sc->sc_dev), err));
			break;
		}

		err = utoppy_readdir_next(sc);
		if (err) {
			DPRINTF(UTOPPY_DBG_READDIR, ("%s: utoppyioctl: "
			    "utoppy_readdir_next() returned %d\n",
			    device_xname(sc->sc_dev), err));
		}
		break;

	case UTOPPYIOREADFILE:
		urf = (struct utoppy_readfile *)data;

		DPRINTF(UTOPPY_DBG_IOCTL,("%s: utoppyioctl: UTOPPYIOREADFILE "
		    "%s, offset %lld\n", device_xname(sc->sc_dev), urf->ur_path,
		    urf->ur_offset));

		if ((err = utoppy_turbo_mode(sc, sc->sc_turbo_mode)) != 0)
			break;

		UTOPPY_OUT_INIT(sc);
		utoppy_add_8(sc, UTOPPY_FILE_READ);

		if ((err = utoppy_add_path(sc, urf->ur_path, 1)) != 0)
			break;

		utoppy_add_64(sc, urf->ur_offset);

		sc->sc_state = UTOPPY_STATE_READFILE;
		sc->sc_in_offset = 0;

		err = utoppy_send_packet(sc, UTOPPY_CMD_FILE,
		    UTOPPY_LONG_TIMEOUT);
		if (err == 0)
			err = utoppy_readfile_next(sc);
		break;

	case UTOPPYIOWRITEFILE:
		uw = (struct utoppy_writefile *)data;

		DPRINTF(UTOPPY_DBG_IOCTL,("%s: utoppyioctl: UTOPPYIOWRITEFILE "
		    "%s, size %lld, offset %lld\n", device_xname(sc->sc_dev),
		    uw->uw_path, uw->uw_size, uw->uw_offset));

		if ((err = utoppy_turbo_mode(sc, sc->sc_turbo_mode)) != 0)
			break;

		UTOPPY_OUT_INIT(sc);
		utoppy_add_8(sc, UTOPPY_FILE_WRITE);
		uwfp = utoppy_current_ptr(sc->sc_out_data);

		if ((err = utoppy_add_path(sc, uw->uw_path, 1)) != 0) {
			DPRINTF(UTOPPY_DBG_WRITE,("%s: utoppyioctl: add_path() "
			    "returned %d\n", device_xname(sc->sc_dev), err));
			break;
		}

		strncpy(uwf, &uwfp[2], sizeof(uwf));
		utoppy_add_64(sc, uw->uw_offset);

		err = utoppy_command(sc, UTOPPY_CMD_FILE, UTOPPY_LONG_TIMEOUT,
		    &resp);
		if (err) {
			DPRINTF(UTOPPY_DBG_WRITE,("%s: utoppyioctl: "
			    "utoppy_command(UTOPPY_CMD_FILE) returned "
			    "%d\n", device_xname(sc->sc_dev), err));
			break;
		}
		if (resp != UTOPPY_RESP_SUCCESS) {
			DPRINTF(UTOPPY_DBG_WRITE,("%s: utoppyioctl: "
			    "utoppy_command(UTOPPY_CMD_FILE) returned "
			    "bad response 0x%x\n", device_xname(sc->sc_dev),
			    resp));
			err = EIO;
			break;
		}

		UTOPPY_OUT_INIT(sc);
		utoppy_timestamp_encode(sc, uw->uw_mtime);
		utoppy_add_8(sc, UTOPPY_FTYPE_FILE);
		utoppy_add_64(sc, uw->uw_size);
		utoppy_add_string(sc, uwf, sizeof(uwf));
		utoppy_add_32(sc, 0);

		err = utoppy_command(sc, UTOPPY_RESP_FILE_HEADER,
		    UTOPPY_LONG_TIMEOUT, &resp);
		if (err) {
			DPRINTF(UTOPPY_DBG_WRITE,("%s: utoppyioctl: "
			    "utoppy_command(UTOPPY_RESP_FILE_HEADER) "
			    "returned %d\n", device_xname(sc->sc_dev), err));
			break;
		}
		if (resp != UTOPPY_RESP_SUCCESS) {
			DPRINTF(UTOPPY_DBG_WRITE,("%s: utoppyioctl: "
			    "utoppy_command(UTOPPY_RESP_FILE_HEADER) "
			    "returned bad response 0x%x\n",
			    device_xname(sc->sc_dev), resp));
			err = EIO;
			break;
		}

		sc->sc_wr_offset = uw->uw_offset;
		sc->sc_wr_size = uw->uw_size;
		sc->sc_state = UTOPPY_STATE_WRITEFILE;

		DPRINTF(UTOPPY_DBG_WRITE,("%s: utoppyioctl: Changing state to "
		    "%s. wr_offset %lld, wr_size %lld\n",
		    device_xname(sc->sc_dev), utoppy_state_string(sc->sc_state),
		    sc->sc_wr_offset, sc->sc_wr_size));
		break;

	default:
		DPRINTF(UTOPPY_DBG_IOCTL,("%s: utoppyioctl: Invalid cmd\n",
		    device_xname(sc->sc_dev)));
		err = ENODEV;
		break;
	}

	DPRINTF(UTOPPY_DBG_IOCTL,("%s: utoppyioctl: done. err %d, state '%s'\n",
	    device_xname(sc->sc_dev), err, utoppy_state_string(sc->sc_state)));

	if (err)
		utoppy_cancel(sc);

	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);

	return (err);
}

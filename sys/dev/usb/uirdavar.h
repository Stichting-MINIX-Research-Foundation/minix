/*	$NetBSD: uirdavar.h,v 1.5 2010/11/03 22:34:24 dyoung Exp $	*/

/*
 * Copyright (c) 2001,2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net).
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
 * Protocol related definitions
 */

#define UIRDA_INPUT_HEADER_SIZE 1
/* Inbound header byte */
#define UIRDA_MEDIA_BUSY	0x80
#define UIRDA_SPEED_MASK	0x0f
#define UIRDA_NO_SPEED		0x00
#define UIRDA_2400		0x01
#define UIRDA_9600		0x02
#define UIRDA_19200		0x03
#define UIRDA_38400		0x04
#define UIRDA_57600		0x05
#define UIRDA_115200		0x06
#define UIRDA_576000		0x07
#define UIRDA_1152000		0x08
#define UIRDA_4000000		0x09

#define UIRDA_OUTPUT_HEADER_SIZE 1
/* Outbound header byte */
#define UIRDA_EB_NO_CHANGE	0x00
#define UIRDA_EB_48		0x10
#define UIRDA_EB_24		0x20
#define UIRDA_EB_12		0x30
#define UIRDA_EB_6		0x40
#define UIRDA_EB_3		0x50
#define UIRDA_EB_2		0x60
#define UIRDA_EB_1		0x70
#define UIRDA_EB_0		0x80
/* Speeds as above */

#define UIRDA_WR_TIMEOUT 200

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
#define UDESC_IRDA	0x21
	uWord		bcdSpecRevision;
	uByte		bmDataSize;
#define UI_DS_2048	0x20
#define UI_DS_1024	0x10
#define UI_DS_512	0x08
#define UI_DS_256	0x04
#define UI_DS_128	0x02
#define UI_DS_64	0x01
	uByte		bmWindowSize;
#define UI_WS_7		0x40
#define UI_WS_6		0x20
#define UI_WS_5		0x10
#define UI_WS_4		0x08
#define UI_WS_3		0x04
#define UI_WS_2		0x02
#define UI_WS_1		0x01
	uByte		bmMinTurnaroundTime;
#define UI_TA_0		0x80
#define UI_TA_10	0x40
#define UI_TA_50	0x20
#define UI_TA_100	0x10
#define UI_TA_500	0x08
#define UI_TA_1000	0x04
#define UI_TA_5000	0x02
#define UI_TA_10000	0x01
	uWord		wBaudRate;
#define UI_BR_4000000	0x0100
#define UI_BR_1152000	0x0080
#define UI_BR_576000	0x0040
#define UI_BR_115200	0x0020
#define UI_BR_57600	0x0010
#define UI_BR_38400	0x0008
#define UI_BR_19200	0x0004
#define UI_BR_9600	0x0002
#define UI_BR_2400	0x0001
	uByte		bmAdditionalBOFs;
#define UI_EB_0		0x80
#define UI_EB_1		0x40
#define UI_EB_2		0x20
#define UI_EB_3		0x10
#define UI_EB_6		0x08
#define UI_EB_12	0x04
#define UI_EB_24	0x02
#define UI_EB_48	0x01
	uByte		bIrdaSniff;
	uByte		bMaxUnicastList;
} UPACKED usb_irda_descriptor_t;
#define USB_IRDA_DESCRIPTOR_SIZE 12


struct uirda_softc {
 	device_t		sc_dev;
	usbd_device_handle	sc_udev;
	usbd_interface_handle	sc_iface;

	kmutex_t		sc_rd_buf_lk;
	u_int8_t		*sc_rd_buf;
	int			sc_rd_addr;
	usbd_pipe_handle	sc_rd_pipe;
	usbd_xfer_handle	sc_rd_xfer;
	struct selinfo		sc_rd_sel;
	u_int			sc_rd_count;
	u_char			sc_rd_err;

	kmutex_t		sc_wr_buf_lk;
	u_int8_t		*sc_wr_buf;
	int			sc_wr_addr;
	usbd_xfer_handle	sc_wr_xfer;
	usbd_pipe_handle	sc_wr_pipe;
	int			sc_wr_hdr;
	struct selinfo		sc_wr_sel;

	device_t		sc_child;
	struct irda_params	sc_params;
	usb_irda_descriptor_t	sc_irdadesc;

	int			sc_refcnt;
	char			sc_dying;
	u_int8_t		sc_hdszi; /* set to value if != 1 needed */

	int			(*sc_loadfw)(struct uirda_softc *);
	struct irframe_methods	*sc_irm;
};

usbd_status usbd_get_class_desc(usbd_device_handle, int type, int index,
	int len, void *desc);

int uirda_open(void *h, int flag, int mode, struct lwp *l);
int uirda_close(void *h, int flag, int mode, struct lwp *l);
int uirda_read(void *h, struct uio *uio, int flag);
int uirda_write(void *h, struct uio *uio, int flag);
int uirda_set_params(void *h, struct irda_params *params);
int uirda_get_speeds(void *h, int *speeds);
int uirda_get_turnarounds(void *h, int *times);
int uirda_poll(void *h, int events, struct lwp *l);
int uirda_kqfilter(void *h, struct knote *kn);

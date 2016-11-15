/* $NetBSD: emdtvvar.h,v 1.3 2011/08/09 01:42:24 jmcneill Exp $ */

/*-
 * Copyright (c) 2008 Jared D. McNeill <jmcneill@invisible.ca>
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

#ifndef _DEV_USB_EMDTVVAR_H
#define _DEV_USB_EMDTVVAR_H

#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/workqueue.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/usb/emdtv_board.h>

#include <dev/i2c/i2cvar.h>
#include <dev/dtv/dtvif.h>

#include <dev/i2c/lg3303var.h>
#include <dev/i2c/xc3028var.h>

#define EMDTV_EEPROM_LEN	256

#define EMDTV_NXFERS		6
#define EMDTV_NFRAMES		64

#define EMDTV_CIR_BUFLEN	32

struct emdtv_softc;

struct emdtv_isoc_xfer {
	struct emdtv_softc	*ix_sc;
	usbd_xfer_handle	ix_xfer;
	uint8_t			*ix_buf;
	uint16_t		ix_frlengths[EMDTV_NFRAMES];
	struct emdtv_isoc_xfer	*ix_altix;
};

struct emdtv_softc {
	device_t		sc_dev;
	usbd_device_handle	sc_udev;

	device_t		sc_cirdev;
	device_t		sc_dtvdev;

	uint16_t		sc_vendor, sc_product;

	const struct emdtv_board *sc_board;

	struct lg3303		*sc_lg3303;
	struct xc3028		*sc_xc3028;

	struct i2c_controller	sc_i2c;
	kmutex_t		sc_i2c_lock;

	uint8_t			sc_eeprom[EMDTV_EEPROM_LEN];

	usbd_interface_handle	sc_iface;

	usbd_pipe_handle	sc_isoc_pipe;
	int			sc_isoc_buflen;
	int			sc_isoc_maxpacketsize;
	struct emdtv_isoc_xfer	sc_ix[EMDTV_NXFERS];

	usbd_pipe_handle	sc_intr_pipe;
	uint8_t			sc_intr_buf;
	struct workqueue	*sc_ir_wq;
	struct work		sc_ir_work;
	uint8_t			sc_ir_keyid;

	kmutex_t		sc_ir_mutex;
	uint8_t			sc_ir_queue[EMDTV_CIR_BUFLEN][3];
	int			sc_ir_cnt;
	int			sc_ir_ptr;
	bool			sc_ir_open;

	uint32_t		sc_frequency;

	bool			sc_streaming;
	void			(*sc_dtvsubmitcb)(void *,
				    const struct dtv_payload *);
	void			*sc_dtvsubmitarg;

	bool			sc_dying;
};

void	emdtv_dtv_attach(struct emdtv_softc *);
void	emdtv_dtv_detach(struct emdtv_softc *, int);
void	emdtv_dtv_rescan(struct emdtv_softc *, const char *, const int *);
void	emdtv_ir_attach(struct emdtv_softc *);
void	emdtv_ir_detach(struct emdtv_softc *, int);
int	emdtv_i2c_attach(struct emdtv_softc *);
int	emdtv_i2c_detach(struct emdtv_softc *, int);

uint8_t	emdtv_read_1(struct emdtv_softc *, uint8_t, uint16_t);
void	emdtv_read_multi_1(struct emdtv_softc *, uint8_t, uint16_t,
			   uint8_t *, uint16_t);
void	emdtv_write_1(struct emdtv_softc *, uint8_t, uint16_t, uint8_t);
void	emdtv_write_multi_1(struct emdtv_softc *, uint8_t, uint16_t,
			    const uint8_t *, uint16_t);

bool	emdtv_gpio_ctl(struct emdtv_softc *, emdtv_gpio_reg_t, bool);

#endif /* !_DEV_USB_EMDTVVAR_H */
